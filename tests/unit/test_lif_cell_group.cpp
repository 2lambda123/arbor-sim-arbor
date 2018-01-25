#include "../gtest.h"
#include <cell_group_factory.hpp>
#include <fstream>
#include <lif_cell_description.hpp>
#include <lif_cell_group.hpp>
#include <load_balance.hpp>
#include <model.hpp>
#include <recipe.hpp>
#include <rss_cell.hpp>
#include <rss_cell_group.hpp>

using namespace arb;
// Simple ring network of lif neurons.
// with one regularly spiking cell connecting to the first cell in the ring.
class ring_recipe: public arb::recipe {
public:
    ring_recipe(cell_size_type n, float weight, float delay):
        ncells_(n), weight_(weight), delay_(delay)
    {}

    cell_size_type num_cells() const override {
        return ncells_ + 1;
    }

    // LIF neurons have gid in range [1..ncells_] whereas fake cell is numbered with 0.
    cell_kind get_cell_kind(cell_gid_type gid) const override {
        if (gid == 0) {
            return cell_kind::regular_spike_source;
        }
        return cell_kind::lif_neuron;
    }

    std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
        if (gid == 0) {
            return {};
        }
        // In a ring, each cell has just one incoming connection.
        std::vector<cell_connection> connections;
        // gid-1 >= 0 since gid != 0
        cell_member_type source{(gid - 1) % ncells_, 0};
        cell_member_type target{gid, 0};
        cell_connection conn(source, target, weight_, delay_);
        connections.push_back(conn);

        return connections;
    }

    util::unique_any get_cell_description(cell_gid_type gid) const override {
        // regularly spiking cell.
        if (gid == 0) {
            // Produces just a single spike at time 0ms.
            auto rs = arb::rss_cell();
            rs.start_time = 0;
            rs.period = 1;
            rs.stop_time = 0.5;
            return rs;
        }
        // LIF cell.
        return lif_cell_description();
    }

    cell_size_type num_sources(cell_gid_type) const override {
        return 1;
    }
    cell_size_type num_targets(cell_gid_type) const override {
        return 1;
    }
    cell_size_type num_probes(cell_gid_type) const override {
        return 0;
    }
    probe_info get_probe(cell_member_type probe_id) const override {
        return {};
    }
    std::vector<event_generator_ptr> event_generators(cell_gid_type) const override {
        return {};
    }

private:
    cell_size_type ncells_;
    float weight_, delay_;
};

class path_recipe: public arb::recipe {
public:
    path_recipe(cell_size_type n, float weight, float delay):
        ncells_(n), weight_(weight), delay_(delay)
    {}

    cell_size_type num_cells() const override {
        return ncells_ + 1;
    }

    cell_kind get_cell_kind(cell_gid_type gid) const override {
        return cell_kind::lif_neuron;
    }

    std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
        if (gid == 0) {
            return {};
        }
        std::vector<cell_connection> connections;
        cell_member_type source{gid - 1, 0};
        cell_member_type target{gid, 0};
        cell_connection conn(source, target, weight_, delay_);
        connections.push_back(conn);

        // If first LIF cell, then add
        // the connection from the last LIF cell as well
        if (gid == 1) {
            cell_member_type source{ncells_, 0};
            cell_member_type target{gid, 0};
            cell_connection conn(source, target, weight_, delay_);
            connections.push_back(conn);
        }

        return connections;
    }

    util::unique_any get_cell_description(cell_gid_type gid) const override {
        return lif_cell_description();
    }

    cell_size_type num_sources(cell_gid_type) const override {
        return 1;
    }
    cell_size_type num_targets(cell_gid_type) const override {
        return 1;
    }
    cell_size_type num_probes(cell_gid_type) const override {
        return 0;
    }
    probe_info get_probe(cell_member_type probe_id) const override {
        return {};
    }
    std::vector<event_generator_ptr> event_generators(cell_gid_type) const override {
        return {};
    }

private:
    int ncells_;
    float weight_, delay_;
};

TEST(lif_cell_group, recipe)
{
    ring_recipe rr(100, 1, 0.1);
    EXPECT_EQ(101u, rr.num_cells());
    EXPECT_EQ(2u, rr.connections_on(1u).size());
    EXPECT_EQ(1u, rr.connections_on(55u).size());
    EXPECT_EQ(0u, rr.connections_on(1u)[0].source.gid);
    EXPECT_EQ(99u, rr.connections_on(1u)[1].source.gid);
}

TEST(lif_cell_group, spikes) {
    // make two lif cells
    path_recipe recipe(2, 1000, 0.1);

    hw::node_info nd;
    nd.num_cpu_cores = threading::num_threads();

    auto decomp = partition_load_balance(recipe, nd);
    model m(recipe, decomp);

    std::vector<postsynaptic_spike_event> events;

    // First event to trigger the spike (first neuron).
    events.push_back({{0, 0}, 1, 1000});

    // This event happens inside the refractory period of the previous
    // event, thus, should be ignored (first neuron)
    events.push_back({{0, 0}, 1.1, 1000});

    // This event happens long after the refractory period of the previous
    // event, should thus trigger new spike (first neuron).
    events.push_back({{0, 0}, 50, 1000});

    m.inject_events(events);

    time_type tfinal = 100;
    time_type dt = 0.01;
    m.run(tfinal, dt);

    // we expect 4 spikes: 2 by both neurons
    EXPECT_EQ(unsigned(4), m.num_spikes());
}

TEST(lif_cell_group, ring)
{
    // Total number of LIF cells.
    int num_cells = 99;
    double weight = 1000;
    double delay = 1;

    hw::node_info nd;
    nd.num_cpu_cores = threading::num_threads();

    // Total simulation time.
    time_type simulation_time = 100;

    auto recipe = ring_recipe(num_cells, weight, delay);
    auto decomp = partition_load_balance(recipe, nd);

    // Creates a model with a ring recipe of lif neurons
    model mod(recipe, decomp);

    std::vector<spike> spike_buffer;

    mod.set_global_spike_callback(
        [&spike_buffer](const std::vector<spike>& spikes) {
            spike_buffer.insert(spike_buffer.end(), spikes.begin(), spikes.end());
        }
    );

    // Runs the simulation for simulation_time with given timestep
    mod.run(simulation_time, 0.01);
    // The total number of cells in all the cell groups.
    EXPECT_EQ(unsigned(num_cells + 1), recipe.num_cells());

    for (auto& spike : spike_buffer) {
        // Assumes that delay = 1
        // We expect that Regular Spiking Cell spiked at time 0s.
        if (spike.source.gid == 0) {
            EXPECT_EQ(0, spike.time);
        // Other LIF cell should spike consecutively.
        } else {
            EXPECT_EQ(spike.source.gid + 1, spike.time);
        }
    }
}

