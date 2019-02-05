#include "../gtest.h"

#include <arbor/common_types.hpp>

#include "epoch.hpp"
#include "fvm_lowered_cell.hpp"
#include "mc_cell_group.hpp"
#include "util/rangeutil.hpp"

#include "common.hpp"
#include "../common_cells.hpp"
#include "../simple_recipes.hpp"

using namespace arb;

namespace {
    execution_context context;

    fvm_lowered_cell_ptr lowered_cell() {
        return make_fvm_lowered_cell(backend_kind::multicore, context);
    }

    mc_cell make_cell() {
        auto c = make_cell_ball_and_stick();

        c.add_detector({0, 0}, 0);
        c.segment(1)->set_compartments(101);

        return c;
    }

    class gap_recipe_0: public recipe {
    public:
        gap_recipe_0() {}

        cell_size_type num_cells() const override {
            return size_;
        }

        arb::util::unique_any get_cell_description(cell_gid_type gid) const override {
            mc_cell c;
            c.add_soma(20);
            c.add_gap_junction({0, 1});
            return {std::move(c)};
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return cell_kind::cable1d_neuron;
        }
        std::vector<gap_junction_connection> gap_junctions_on(cell_gid_type gid) const override {
            switch (gid) {
                case 0 :
                    return {gap_junction_connection({5, 0}, {0, 0}, 0.1)};
                case 2 :
                    return {
                            gap_junction_connection({3, 0}, {2, 0}, 0.1),
                    };
                case 3 :
                    return {
                            gap_junction_connection({7, 0}, {3, 0}, 0.1),
                            gap_junction_connection({3, 0}, {2, 0}, 0.1)
                    };
                case 5 :
                    return {gap_junction_connection({5, 0}, {0, 0}, 0.1)};
                case 7 :
                    return {
                            gap_junction_connection({3, 0}, {7, 0}, 0.1),
                    };
                default :
                    return {};
            }
        }

    private:
        cell_size_type size_ = 12;
    };

    class gap_recipe_1: public recipe {
    public:
        gap_recipe_1() {}

        cell_size_type num_cells() const override {
            return size_;
        }

        arb::util::unique_any get_cell_description(cell_gid_type) const override {
            mc_cell c;
            c.add_soma(20);
            return {std::move(c)};
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return cell_kind::cable1d_neuron;
        }

    private:
        cell_size_type size_ = 12;
    };

    class gap_recipe_2: public recipe {
    public:
        gap_recipe_2() {}

        cell_size_type num_cells() const override {
            return size_;
        }

        arb::util::unique_any get_cell_description(cell_gid_type) const override {
            mc_cell c;
            c.add_soma(20);
            c.add_gap_junction({0,1});
            return {std::move(c)};
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return cell_kind::cable1d_neuron;
        }
        std::vector<gap_junction_connection> gap_junctions_on(cell_gid_type gid) const override {
            switch (gid) {
                case 0 :
                    return {
                            gap_junction_connection({2, 0}, {0, 0}, 0.1),
                            gap_junction_connection({3, 0}, {0, 0}, 0.1),
                            gap_junction_connection({5, 0}, {0, 0}, 0.1)
                    };
                case 2 :
                    return {
                            gap_junction_connection({0, 0}, {2, 0}, 0.1),
                            gap_junction_connection({3, 0}, {2, 0}, 0.1),
                            gap_junction_connection({5, 0}, {2, 0}, 0.1)
                    };
                case 3 :
                    return {
                            gap_junction_connection({0, 0}, {3, 0}, 0.1),
                            gap_junction_connection({2, 0}, {3, 0}, 0.1),
                            gap_junction_connection({5, 0}, {3, 0}, 0.1)
                    };
                case 5 :
                    return {
                            gap_junction_connection({2, 0}, {5, 0}, 0.1),
                            gap_junction_connection({3, 0}, {5, 0}, 0.1),
                            gap_junction_connection({0, 0}, {5, 0}, 0.1)
                    };
                default :
                    return {};
            }
        }

    private:
        cell_size_type size_ = 12;
    };

}

ACCESS_BIND(
    std::vector<cell_member_type> mc_cell_group::*,
    private_spike_sources_ptr,
    &mc_cell_group::spike_sources_)

TEST(mc_cell_group, get_kind) {
    mc_cell_group group{{0}, cable1d_recipe(make_cell()), lowered_cell()};

    EXPECT_EQ(cell_kind::cable1d_neuron, group.get_cell_kind());
}

TEST(mc_cell_group, test) {
    mc_cell_group group{{0}, cable1d_recipe(make_cell()), lowered_cell()};
    group.advance(epoch(0, 50), 0.01, {});

    // Model is expected to generate 4 spikes as a result of the
    // fixed stimulus over 50 ms.
    EXPECT_EQ(4u, group.spikes().size());
}

TEST(mc_cell_group, sources) {
    // Make twenty cells, with an extra detector on gids 0, 3 and 17
    // to make things more interesting.
    std::vector<mc_cell> cells;

    for (int i=0; i<20; ++i) {
        cells.push_back(make_cell());
        if (i==0 || i==3 || i==17) {
            cells.back().add_detector({1, 0.3}, 2.3);
        }

        EXPECT_EQ(1u + (i==0 || i==3 || i==17), cells.back().detectors().size());
    }

    std::vector<cell_gid_type> gids = {3u, 4u, 10u, 16u, 17u, 18u};
    mc_cell_group group{gids, cable1d_recipe(cells), lowered_cell()};

    // Expect group sources to be lexicographically sorted by source id
    // with gids in cell group's range and indices starting from zero.

    const auto& sources = group.*private_spike_sources_ptr;
    for (unsigned j = 0; j<sources.size(); ++j) {
        auto id = sources[j];
        if (j==0) {
            EXPECT_EQ(id.gid, gids[0]);
            EXPECT_EQ(id.index, 0u);
        }
        else {
            auto prev = sources[j-1];
            EXPECT_GT(id, prev);
            EXPECT_EQ(id.index, id.gid==prev.gid? prev.index+1: 0u);
        }
    }
}

TEST(mc_cell_group, generated_gids_deps_) {
    {
        std::vector<cell_gid_type> gids = {11u, 5u, 2u, 3u, 0u, 8u, 7u};
        mc_cell_group group{gids, gap_recipe_0(), lowered_cell()};

        std::vector<cell_gid_type> expected_gids = {0u, 5u, 2u, 3u, 7u, 8u, 11u};
        std::vector<int> expected_deps = {2, 0, 3, 0, 0, 0, 0};
        EXPECT_EQ(expected_gids, group.get_gids());
        EXPECT_EQ(expected_deps, group.get_dependencies());
    }
    {
        std::vector<cell_gid_type> gids = {11u, 5u, 2u, 3u, 0u, 8u, 7u};
        mc_cell_group group{gids, gap_recipe_1(), lowered_cell()};

        std::vector<cell_gid_type> expected_gids = {0u, 2u, 3u, 5u, 7u, 8u, 11u};
        std::vector<int> expected_deps = {0, 0, 0, 0, 0, 0, 0};
        EXPECT_EQ(expected_gids, group.get_gids());
        EXPECT_EQ(expected_deps, group.get_dependencies());
    }
    {
        std::vector<cell_gid_type> gids = {5u, 2u, 3u, 0u};
        mc_cell_group group{gids, gap_recipe_2(), lowered_cell()};

        std::vector<cell_gid_type> expected_gids = {0u, 2u, 3u, 5u};
        std::vector<int> expected_deps = {4, 0, 0, 0};
        EXPECT_EQ(expected_gids, group.get_gids());
        EXPECT_EQ(expected_deps, group.get_dependencies());
    }
}
