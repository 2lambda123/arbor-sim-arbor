/*
 * A miniapp that demonstrates how to use dry_run mode
 *
 */

#include <fstream>
#include <iomanip>
#include <iostream>

#include <nlohmann/json.hpp>

#include <arbor/assert_macro.hpp>
#include <arbor/cable_cell.hpp>
#include <arbor/common_types.hpp>
#include <arbor/context.hpp>
#include <arbor/benchmark_cell.hpp>
#include <arbor/load_balance.hpp>
#include <arbor/morph/primitives.hpp>
#include <arbor/profile/meter_manager.hpp>
#include <arbor/profile/profiler.hpp>
#include <arbor/simple_sampler.hpp>
#include <arbor/simulation.hpp>
#include <arbor/symmetric_recipe.hpp>
#include <arbor/recipe.hpp>
#include <arbor/version.hpp>

#include <arborenv/concurrency.hpp>

#include <sup/ioutil.hpp>
#include <sup/json_meter.hpp>
#include <sup/json_params.hpp>

struct bench_params {
    struct cell_params {
        double spike_freq_hz = 20;   // Frequency in hz that cell will generate (poisson) spikes.
        double realtime_ratio = 0.1; // Integration speed relative to real time, e.g. 10 implies
                                     // that a cell is integrated 10 times slower than real time.
    };
    struct network_params {
        unsigned fan_in = 5000;      // Number of incoming connections on each cell.
        double min_delay = 10;       // Used as the delay on all connections.
    };
    int num_ranks = 1;               // Number of simulated MPI ranks.
    int num_threads = 1;             // Number of threads per rank.
    std::string name = "default";    // Name of the model.
    unsigned num_cells = 100;        // Number of cells _per rank_.
    arb::time_type duration = 100;   // Simulation duration in ms.

    cell_params cell;                // Cell parameters for all cells in model.
    network_params network;          // Description of the network.

    // Expected simulation performance properties based on model parameters.

    // Time to finish simulation if only cell overheads are counted.
    double expected_advance_time() const {
        return cell.realtime_ratio * duration*1e-3 * num_cells;
    }
    // Total expected number of spikes generated by simulation.
    unsigned expected_spikes() const {
        return num_cells * duration*1e-3 * cell.spike_freq_hz * num_ranks;
    }
    // Expected number of spikes generated per min_delay/2 interval.
    unsigned expected_spikes_per_interval() const {
        return num_cells * network.min_delay*1e-3/2 * cell.spike_freq_hz;
    }
    // Expected number of post-synaptic events delivered over simulation.
    unsigned expected_events() const {
        return expected_spikes() * network.fan_in * num_ranks;
    }
    // Expected number of post-synaptic events delivered per min_delay/2 interval.
    unsigned expected_events_per_interval() const {
        return expected_spikes_per_interval() * network.fan_in * num_ranks;
    }
};

bench_params read_options(int argc, char** argv);
std::ostream& operator<<(std::ostream& o, const bench_params& p);

using arb::cell_gid_type;
using arb::cell_lid_type;
using arb::cell_size_type;
using arb::cell_member_type;
using arb::cell_kind;
using arb::time_type;

class tile_desc: public arb::tile {
public:
    tile_desc(bench_params params):
            params_(params),
            num_cells_(params.num_cells),
            num_tiles_(params.num_ranks)
    {}
        //auto tile = std::make_unique<tile_desc>(params.num_cells_per_rank,
                //params.num_ranks, params.cell, params.min_delay);

    cell_size_type num_cells() const override {
        return num_cells_;
    }

    cell_size_type num_tiles() const override {
        return num_tiles_;
    }

    arb::util::unique_any get_cell_description(cell_gid_type gid) const override {
        using RNG = std::mt19937_64;
        auto gen = arb::poisson_schedule(params_.cell.spike_freq_hz/1000, RNG(gid));
        return arb::benchmark_cell("src", "tgt", std::move(gen), params_.cell.realtime_ratio);
    }

    cell_kind get_cell_kind(cell_gid_type gid) const override {
        return cell_kind::benchmark;
    }

    // Each cell has num_synapses incoming connections, from any cell in the
    // network spanning all ranks, src gid in {0, ..., num_cells_*num_tiles_ - 1}.
    std::vector<arb::cell_connection> connections_on(cell_gid_type gid) const override {
        std::uniform_int_distribution<cell_gid_type>
            source_distribution(0, num_cells_*num_tiles_ - 2);

        std::vector<arb::cell_connection> conns;
        auto src_gen = std::mt19937(gid);
        for (unsigned i=0; i<params_.network.fan_in; ++i) {
            auto src = source_distribution(src_gen);
            if (src>=gid) ++src;
            conns.push_back(arb::cell_connection({src, "src"}, {"tgt"}, 1.f, params_.network.min_delay));
        }

        return conns;
    }

    /*
    // Return an event generator on every 20th gid. This function needs to generate events
    // for ALL cells on ALL ranks. This is because the symmetric recipe can not easily
    // translate the src gid of an event generator
    std::vector<arb::event_generator> event_generators(cell_gid_type gid) const override {
        std::vector<arb::event_generator> gens;
        if (gid%20 == 0) {
            gens.push_back(arb::explicit_generator(arb::pse_vector{{{gid, 0}, 0.1, 1.0}}));
        }
        return gens;
    }
    */

private:
    bench_params params_;
    cell_size_type num_cells_;
    cell_size_type num_tiles_;
};

int main(int argc, char** argv) {
    try {
        auto params = read_options(argc, argv);

        std::cout << params << "\n";

        auto resources = arb::proc_allocation();
        resources.num_threads = params.num_threads;
        auto ctx = arb::make_context(resources);

        ctx = arb::make_context(resources, arb::dry_run_info(params.num_ranks, params.num_cells));
        arb_assert(arb::num_ranks(ctx)==params.num_ranks);

#ifdef ARB_PROFILE_ENABLED
        arb::profile::profiler_initialize(ctx);
#endif

        arb::profile::meter_manager meters;
        meters.start(ctx);

        // Create an instance of our tile and use it to make a symmetric_recipe.
        auto tile = std::make_unique<tile_desc>(params);
        arb::symmetric_recipe recipe(std::move(tile));

        auto decomp = arb::partition_load_balance(recipe, ctx);

        // Construct the model.
        arb::simulation sim(recipe, decomp, ctx);

        meters.checkpoint("model-init", ctx);

        // Run the simulation for 100 ms, with time steps of 0.025 ms.
        sim.run(params.duration, 0.025);

        meters.checkpoint("model-run", ctx);

        auto ns = sim.num_spikes();
        auto total_cells = params.num_ranks*params.num_cells;
        std::cout << "\n" << ns << " spikes generated at rate of "
                  << ns/total_cells << " spikes per cell\n\n";

        auto profile = arb::profile::profiler_summary();
        std::cout << profile << "\n";

        auto report = arb::profile::make_meter_report(meters, ctx);
        std::cout << report;
    }
    catch (std::exception& e) {
        std::cerr << "exception caught in benchmark: \n" << e.what() << "\n";
        return 1;
    }

    return 0;
}

std::ostream& operator<<(std::ostream& o, const bench_params& p) {
    o << "benchmark parameters:\n"
      << "  name:           " << p.name << "\n"
      << "  cells per rank: " << p.num_cells << "\n"
      << "  duration:       " << p.duration << " ms\n"
      << "  fan in:         " << p.network.fan_in << " connections/cell\n"
      << "  min delay:      " << p.network.min_delay << " ms\n"
      << "  spike freq:     " << p.cell.spike_freq_hz << " Hz\n"
      << "  cell overhead:  " << p.cell.realtime_ratio << " ms to advance 1 ms\n";
    o << "expected:\n"
      << "  cell advance:   " << p.expected_advance_time() << " s\n"
      << "  spikes:         " << p.expected_spikes() << "\n"
      << "  events:         " << p.expected_events() << "\n"
      << "  spikes:         " << p.expected_spikes_per_interval() << " per interval\n"
      << "  events:         " << p.expected_events_per_interval()/p.num_cells << " per cell per interval\n";
    o << "HW resources:\n"
      << "  threads:        " << p.num_threads << "\n"
      << "  ranks:          " << p.num_ranks;

    return o;
}

bench_params read_options(int argc, char** argv) {
    using sup::param_from_json;

    bench_params params;

    // Set default number of threads to that provided by system
    if (auto nt = arbenv::get_env_num_threads()) {
        params.num_threads = nt;
    }
    else {
        params.num_threads = arbenv::thread_concurrency();
    }

    if (argc<2) {
        std::cout << "Using default parameters.\n";
        return params;
    }
    if (argc>2) {
        throw std::runtime_error("More than one command line option is not permitted.");
    }

    std::string fname = argv[1];
    std::cout << "Loading parameters from file: " << fname << "\n";
    std::ifstream f(fname);

    if (!f.good()) {
        throw std::runtime_error("Unable to open input parameter file: "+fname);
    }

    nlohmann::json json;
    f >> json;

    param_from_json(params.name, "name", json);
    param_from_json(params.num_cells, "num-cells", json);
    param_from_json(params.duration, "duration", json);
    param_from_json(params.network.min_delay, "min-delay", json);
    param_from_json(params.network.fan_in, "fan-in", json);
    param_from_json(params.cell.realtime_ratio, "realtime-ratio", json);
    param_from_json(params.cell.spike_freq_hz, "spike-frequency", json);
    param_from_json(params.num_threads, "threads", json);
    param_from_json(params.num_ranks, "ranks", json);

    for (auto it=json.begin(); it!=json.end(); ++it) {
        std::cout << "  Warning: unused input parameter: \"" << it.key() << "\"\n";
    }
    std::cout << "\n";

    return params;
}

