#include <gtest/gtest.h>
#include "test.hpp"

#include <tuple>
#include <vector>

#include <arbor/domain_decomposition.hpp>
#include <arbor/lif_cell.hpp>
#include <arbor/load_balance.hpp>
#include <arbor/spike_event.hpp>

#include "communication/communicator.hpp"
#include "execution_context.hpp"
#include "fvm_lowered_cell.hpp"
#include "lif_cell_group.hpp"
#include "cable_cell_group.hpp"
#include "util/filter.hpp"
#include "util/rangeutil.hpp"
#include "util/span.hpp"

using namespace arb;

TEST(communicator, policy_basics) {

    const int num_domains = g_context->distributed->size();
    const int rank = g_context->distributed->id();

    EXPECT_EQ((int)arb::num_ranks(g_context), num_domains);
    EXPECT_EQ((int)arb::rank(g_context), rank);

    EXPECT_EQ(g_context->distributed->min(rank), 0);
    EXPECT_EQ(g_context->distributed->max(rank), num_domains-1);
}

// Wrappers for creating and testing spikes used
// to test that spikes are correctly exchanged.
arb::spike gen_spike(int source, int value) {
    arb::spike s;
    s.source.gid = source;
    s.source.index = value;
    return s;
}

int get_source(const arb::spike& s) {
    return s.source.gid;
}

int get_value(const arb::spike& s) {
    return s.source.index;
}

// Test low level spike_gather function when each domain produces the same
// number of spikes in the pattern used by dry run mode.
TEST(communicator, gather_spikes_equal) {
    const auto num_domains = g_context->distributed->size();
    const auto rank = g_context->distributed->id();

    const auto n_local_spikes = 10;

    // Create local spikes for communication.
    std::vector<spike> local_spikes;
    for (auto i=0; i<n_local_spikes; ++i) {
        local_spikes.push_back(gen_spike(i+rank*n_local_spikes, rank));
    }

    // Perform exchange
    const auto global_spikes = g_context->distributed->gather_spikes(local_spikes);

    // Test that partition information is correct
    const auto& part = global_spikes.partition();
    EXPECT_EQ(num_domains+1u, part.size());
    for (auto i=0u; i<part.size(); ++i) {
        EXPECT_EQ(part[i], n_local_spikes*i);
    }

    // Test that spikes were correctly exchanged
    //
    // In dry run mode the local spikes had sources numbered 0:n_local_spikes-1.
    // The global exchange should replicate the local spikes and
    // shift their sources to make them local to the "dummy" source
    // domain.
    // We set the model up with n_local_cells==n_local_spikes with
    // one spike per local cell, so the result of the global exchange
    // is a list of num_domains*n_local_spikes spikes that have
    // contiguous source gid
    const auto& spikes = global_spikes.values();
    EXPECT_EQ(n_local_spikes*g_context->distributed->size(), int(spikes.size()));
    for (auto i=0u; i<spikes.size(); ++i) {
        const auto s = spikes[i];
        EXPECT_EQ(i, unsigned(s.source.gid));
        EXPECT_EQ(int(i)/n_local_spikes, get_value(s));
    }
}

// Test low level spike_gather function when the number of spikes per domain
// are not equal.
TEST(communicator, gather_spikes_variant) {
    const auto num_domains = g_context->distributed->size();
    const auto rank = g_context->distributed->id();

    // Parameter used to scale the number of spikes generated on successive
    // ranks.
    constexpr int scale = 10;
    // Calculates the number of spikes generated by the first n ranks.
    // Can be used to calculate the index of the range of spikes
    // generated by a given rank, and to determine the total number of
    // spikes generated globally.
    auto sumn = [](int n) {return scale*n*(n+1)/2;};
    const auto n_local_spikes = scale*rank;

    // Create local spikes for communication.
    // The ranks generate different numbers of spikes, with the ranks
    // generating the following number of spikes
    //      [ 0, scale, 2*scale, 3*scale, ..., (num_domains-1)*scale ]
    // i.e. 0 spikes on the first rank, scale spikes on the second, and so on.
    std::vector<spike> local_spikes;
    const auto local_start_id = sumn(rank-1);
    for (auto i=0; i<n_local_spikes; ++i) {
        local_spikes.push_back(gen_spike(local_start_id+i, rank));
    }

    // Perform exchange
    const auto global_spikes = g_context->distributed->gather_spikes(local_spikes);

    // Test that partition information is correct
    const auto& part =global_spikes.partition();
    EXPECT_EQ(unsigned(num_domains+1), part.size());
    EXPECT_EQ(0, (int)part[0]);
    for (auto i=1u; i<part.size(); ++i) {
        EXPECT_EQ(sumn(i-1), (int)part[i]);
    }

    // Test that spikes were correctly exchanged
    for (auto domain=0; domain<num_domains; ++domain) {
        auto source = sumn(domain-1);
        const auto first_spike = global_spikes.values().begin() + sumn(domain-1);
        const auto last_spike  = global_spikes.values().begin() + sumn(domain);
        const auto spikes = util::make_range(first_spike, last_spike);
        for (auto s: spikes) {
            EXPECT_EQ(get_value(s), domain);
            EXPECT_EQ(get_source(s), source++);
        }
    }
}

// Test low level gids_gather function when the number of gids per domain
// are not equal.
TEST(communicator, gather_gids_variant) {
    const auto num_domains = g_context->distributed->size();
    const auto rank = g_context->distributed->id();

    constexpr int scale = 10;
    const auto n_local_gids = scale*rank;
    auto sumn = [](unsigned n) {return scale*n*(n+1)/2;};

    std::vector<cell_gid_type > local_gids;
    const auto local_start_id = sumn(rank-1);
    for (auto i=0; i<n_local_gids; ++i) {
        local_gids.push_back(local_start_id + i);
    }

    // Perform exchange
    const auto global_gids = g_context->distributed->gather_gids(local_gids);

    // Test that partition information is correct
    const auto& part =global_gids.partition();
    EXPECT_EQ(unsigned(num_domains+1), part.size());
    EXPECT_EQ(0, (int)part[0]);
    for (auto i=1u; i<part.size(); ++i) {
        EXPECT_EQ(sumn(i-1), part[i]);
    }

    // Test that gids were correctly exchanged
    for (auto domain=0; domain<num_domains; ++domain) {
        auto source = sumn(domain-1);
        const auto first_gid = global_gids.values().begin() + sumn(domain-1);
        const auto last_gid  = global_gids.values().begin() + sumn(domain);
        const auto gids = util::make_range(first_gid, last_gid);
        for (auto s: gids) {
            EXPECT_EQ(s, source++);
        }
    }
}

namespace {
    // Population of cable and rss cells with ring connection topology.
    // Even gid are rss, and odd gid are cable cells.
    class ring_recipe: public recipe {
    public:
        ring_recipe(cell_size_type s): size_(s) {}

        cell_size_type num_cells() const override {
            return size_;
        }

        util::unique_any get_cell_description(cell_gid_type gid) const override {
            if (gid%2) {
                arb::segment_tree tree;
                tree.append(arb::mnpos, {0, 0, 0.0, 1.0}, {0, 0, 200, 1.0}, 1);
                arb::decor decor;
                decor.set_default(arb::cv_policy_fixed_per_branch(10));
                decor.place(arb::mlocation{0, 0.5}, arb::threshold_detector{10}, "src");
                decor.place(arb::mlocation{0, 0.5}, arb::synapse("expsyn"), "tgt");
                return arb::cable_cell(arb::morphology(tree), {}, decor);
            }
            return arb::lif_cell("src", "tgt");
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return gid%2? cell_kind::cable: cell_kind::lif;
        }

        std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
            // a single connection from the preceding cell, i.e. a ring
            // weight is the destination gid
            // delay is 1
            cell_global_label_type src = {gid==0? size_-1: gid-1, "src"};
            cell_local_label_type dst = {"tgt"};
            return {cell_connection(
                        src, dst,   // end points
                        float(gid), // weight
                        1.0f)};     // delay
        }

        std::any get_global_properties(arb::cell_kind kind) const override {
            if (kind == arb::cell_kind::cable) {
                arb::cable_cell_global_properties gprop;
                gprop.default_parameters = arb::neuron_parameter_defaults;
                return gprop;
            }
            return {};
        }

    private:
        cell_size_type size_;
    };

    cell_gid_type source_of(cell_gid_type gid, cell_size_type num_cells) {
        if (gid) {
            return gid-1;
        }
        return num_cells-1;
    }

    // gid expects an event from source_of(gid) with weight gid, and fired at
    // time source_of(gid).
    spike_event expected_event_ring(cell_gid_type gid, cell_size_type num_cells) {
        auto sid = source_of(gid, num_cells);
        return {
            0u,         // source
            sid+1.0f,   // time (all conns have delay 1 ms)
            float(gid)};// weight
    }

    // spike generated by cell gid
    spike make_spike(cell_gid_type gid) {
        return spike({gid, 0u}, time_type(gid));
    }

    // Population of cable and rss cells with all-to-all connection topology.
    // Even gid are rss, and odd gid are cable cells.
    class all2all_recipe: public recipe {
    public:
        all2all_recipe(cell_size_type s): size_(s) {}

        cell_size_type num_cells() const override {
            return size_;
        }

        util::unique_any get_cell_description(cell_gid_type gid) const override {
            arb::segment_tree tree;
            tree.append(arb::mnpos, {0, 0, 0.0, 1.0}, {0, 0, 200, 1.0}, 1);
            arb::decor decor;
            decor.set_default(arb::cv_policy_fixed_per_branch(10));
            decor.place(arb::mlocation{0, 0.5}, arb::threshold_detector{10}, "src");
            decor.place(arb::ls::uniform(arb::reg::all(), 0, size_, gid), arb::synapse("expsyn"), "tgt");
            return arb::cable_cell(arb::morphology(tree), {}, decor);
        }
        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return cell_kind::cable;
        }

        std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
            std::vector<cell_connection> cons;
            cons.reserve(size_);
            for (auto sid: util::make_span(0, size_)) {
                cell_connection con(
                        {sid, {"src", arb::lid_selection_policy::round_robin}}, // source
                        {"tgt", arb::lid_selection_policy::round_robin},        // destination
                        float(gid+sid), // weight
                        1.0f);          // delay
                cons.push_back(con);
            }
            return cons;
        }

        std::any get_global_properties(arb::cell_kind) const override {
            arb::cable_cell_global_properties gprop;
            gprop.default_parameters = arb::neuron_parameter_defaults;
            return gprop;
        }

    private:
        cell_size_type size_;
    };

    spike_event expected_event_all2all(cell_gid_type gid, cell_gid_type sid) {
        return {
            sid,      // target, event from sid goes to synapse with index sid
            sid+1.0f,        // time (all conns have delay 1 ms)
            float(gid+sid)}; // weight
    }

    // make a list of the gids on the local domain
    std::vector<cell_gid_type> get_gids(const domain_decomposition& D) {
        std::vector<cell_gid_type> gids;
        for (auto i: util::make_span(0, D.num_groups())) {
            util::append(gids, D.group(i).gids);
        }
        return gids;
    }

    // make a hash table mapping local gid to local cell_group index
    std::unordered_map<cell_gid_type, cell_gid_type>
    get_group_map(const domain_decomposition& D) {
        std::unordered_map<cell_gid_type, cell_gid_type> map;
        for (auto i: util::make_span(0, D.num_groups())) {
            for (auto gid: D.group(i).gids) {
                map[gid] = i;
            }
        }
        return map;
    }

    class mini_recipe: public recipe {
    public:
        mini_recipe(cell_size_type s): nranks_(s), ncells_(nranks_*3) {}

        cell_size_type num_cells() const override {
            return ncells_;
        }

        util::unique_any get_cell_description(cell_gid_type gid) const override {
            arb::segment_tree tree;
            tree.append(arb::mnpos, {0, 0, 0.0, 1.0}, {0, 0, 200, 1.0}, 1);
            arb::decor decor;
            if (gid%3 != 1) {
                decor.place(arb::ls::uniform(arb::reg::all(), 0, 1, gid), arb::synapse("expsyn"), "synapses_0");
                decor.place(arb::ls::uniform(arb::reg::all(), 2, 2, gid), arb::synapse("expsyn"), "synapses_1");
            }
            else {
                decor.place(arb::ls::uniform(arb::reg::all(), 0, 2, gid), arb::threshold_detector{10}, "detectors_0");
                decor.place(arb::ls::uniform(arb::reg::all(), 3, 3, gid), arb::threshold_detector{10}, "detectors_1");
            }
            return arb::cable_cell(arb::morphology(tree), {}, decor);
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return cell_kind::cable;
        }

        std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
            // Cells with gid%3 == 1 are senders, the others are receivers.
            // The following connections are formed; used to test out lid resolutions:
            // 7 from detectors_0 (round-robin) to synapses_0 (round-robin)
            // 1 from detectors_0 (round-robin) to synapses_1 (univalent)
            // 2 from detectors_1 (round-robin) to synapses_0 (round-robin)
            // 1 from detectors_1 (univalent)   to synapses_1 (round-robin)
            // These Should generate the following {src_gid, src_lid} -> {tgt_gid, tgt_lid} mappings (unsorted; 1 rank with 3 cells total):
            // cell 1 - > cell 0:
            //   {1, 0} -> {0, 0}
            //   {1, 1} -> {0, 1}
            //   {1, 2} -> {0, 0}
            //   {1, 0} -> {0, 1}
            //   {1, 1} -> {0, 0}
            //   {1, 2} -> {0, 1}
            //   {1, 0} -> {0, 0}
            //   {1, 1} -> {0, 2}
            //   {1, 3} -> {0, 1}
            //   {1, 3} -> {0, 0}
            //   {1, 3} -> {0, 2}

            // cell 1 - > cell 2:
            //   {1, 0} -> {2, 0}
            //   {1, 1} -> {2, 1}
            //   {1, 2} -> {2, 0}
            //   {1, 0} -> {2, 1}
            //   {1, 1} -> {2, 0}
            //   {1, 2} -> {2, 1}
            //   {1, 0} -> {2, 0}
            //   {1, 1} -> {2, 2}
            //   {1, 3} -> {2, 1}
            //   {1, 3} -> {2, 0}
            //   {1, 3} -> {2, 2}
            std::vector<cell_connection> cons;
            using pol = lid_selection_policy;
            if (gid%3 != 1) {
                for (auto sid: util::make_span(0, ncells_)) {
                    if (sid%3 == 1) {
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});

                        cons.push_back({{sid, "detectors_0", pol::round_robin}, {"synapses_1", pol::assert_univalent}, 1.0, 1.0});

                        cons.push_back({{sid, "detectors_1", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});
                        cons.push_back({{sid, "detectors_1", pol::round_robin}, {"synapses_0", pol::round_robin}, 1.0, 1.0});

                        cons.push_back({{sid, "detectors_1", pol::assert_univalent}, {"synapses_1", pol::round_robin}, 1.0, 1.0});
                    }
                }
            }
            return cons;
        }

        std::any get_global_properties(arb::cell_kind kind) const override {
            if (kind == arb::cell_kind::cable) {
                arb::cable_cell_global_properties gprop;
                gprop.default_parameters = arb::neuron_parameter_defaults;
                return gprop;
            }
            return {};
        }

    private:
        cell_size_type nranks_;
        cell_size_type ncells_;
    };
}

template <typename F>
::testing::AssertionResult
test_ring(const domain_decomposition& D, communicator& C, F&& f) {
    using util::transform_view;
    using util::assign_from;
    using util::filter;

    auto gids = get_gids(D);
    auto group_map = get_group_map(D);

    std::vector<spike> local_spikes = assign_from(transform_view(filter(gids, f), make_spike));
    // Reverse the order of spikes so that they are "unsorted" in terms
    // of source gid.
    std::reverse(local_spikes.begin(), local_spikes.end());

    // gather the global set of spikes
    auto global_spikes = C.exchange(local_spikes);
    if (global_spikes.size()!=g_context->distributed->sum(local_spikes.size())) {
        return ::testing::AssertionFailure() << "the number of gathered spikes "
            << global_spikes.size() << " doesn't match the expected "
            << g_context->distributed->sum(local_spikes.size());
    }

    // generate the events
    std::vector<arb::pse_vector> queues(C.num_local_cells());
    C.make_event_queues(global_spikes, queues);

    // Assert that all the correct events were generated.
    // Iterate over each local gid, and testing whether an event is expected for
    // that gid. If so, look up the event queue of the cell_group of gid, and
    // search for the expected event.
    int expected_count = 0;
    for (auto gid: gids) {
        auto src = source_of(gid, D.num_global_cells());
        if (f(src)) {
            auto expected = expected_event_ring(gid, D.num_global_cells());
            auto grp = group_map[gid];
            auto& q = queues[grp];
            if (std::find(q.begin(), q.end(), expected)==q.end()) {
                return ::testing::AssertionFailure()
                    << "expected event " << expected << " was not found";
            }
            ++expected_count;
        }
    }

    // Assert that only the expected events were produced. The preceding test
    // showed that all expected events were generated, so this only requires
    // that the number of generated events is as expected.
    int num_events = std::accumulate(queues.begin(), queues.end(), 0,
            [](int l, decltype(queues.front())& r){return l + r.size();});

    if (expected_count!=num_events) {
        return ::testing::AssertionFailure() <<
            "the number of events " << num_events <<
            " does not match expected count " << expected_count;
    }

    return ::testing::AssertionSuccess();
}

TEST(communicator, ring)
{
    using util::make_span;

    // construct a homogeneous network of 10*n_domain identical cells in a ring
    unsigned N = g_context->distributed->size();

    unsigned n_local = 10u;
    unsigned n_global = n_local*N;

    auto R = ring_recipe(n_global);
    // use a node decomposition that reflects the resources available
    // on the node that the test is running on, including gpus.
    const auto D = partition_load_balance(R, g_context);

    // set up source and target label->lid resolvers
    // from cable_cell_group and lif_cell_group
    std::vector<cell_gid_type> mc_gids, lif_gids;
    for (auto g: D.groups()) {
        if (g.kind == cell_kind::cable) {
            mc_gids.insert(mc_gids.end(), g.gids.begin(), g.gids.end());
        }
        else if (g.kind == cell_kind::lif) {
            lif_gids.insert(lif_gids.end(), g.gids.begin(), g.gids.end());
        }
    }
    cell_label_range mc_srcs, mc_tgts, lif_srcs, lif_tgts;
    auto mc_group = cable_cell_group(mc_gids, R, mc_srcs, mc_tgts, make_fvm_lowered_cell(backend_kind::multicore, *g_context));
    auto lif_group = lif_cell_group(lif_gids, R, lif_srcs, lif_tgts);

    auto local_sources = cell_labels_and_gids(mc_srcs, mc_gids);
    auto local_targets = cell_labels_and_gids(mc_tgts, mc_gids);
    local_sources.append({lif_srcs, lif_gids});
    local_targets.append({lif_tgts, lif_gids});

    auto global_sources = g_context->distributed->gather_cell_labels_and_gids(local_sources);

    // construct the communicator
    auto C = communicator(R, D, *g_context);
    C.update_connections(R, D, label_resolution_map(local_targets));
    // every cell fires
    EXPECT_TRUE(test_ring(D, C, [](cell_gid_type g){return true;}));
    // last cell in each domain fires
    EXPECT_TRUE(test_ring(D, C, [n_local](cell_gid_type g){return (g+1)%n_local == 0u;}));
    // even-numbered cells fire
    EXPECT_TRUE(test_ring(D, C, [](cell_gid_type g){return g%2==0;}));
    // odd-numbered cells fire
    EXPECT_TRUE(test_ring(D, C, [](cell_gid_type g){return g%2==1;}));
}

template <typename F>
::testing::AssertionResult
test_all2all(const domain_decomposition& D, communicator& C, F&& f) {
    using util::transform_view;
    using util::assign_from;
    using util::filter;
    using util::make_span;

    auto gids = get_gids(D);
    auto group_map = get_group_map(D);

    std::vector<spike> local_spikes = assign_from(transform_view(filter(gids, f), make_spike));
    // Reverse the order of spikes so that they are "unsorted" in terms
    // of source gid.
    std::reverse(local_spikes.begin(), local_spikes.end());

    std::vector<cell_gid_type> spike_gids = assign_from(
        filter(make_span(0, D.num_global_cells()), f));

    // gather the global set of spikes
    auto global_spikes = C.exchange(local_spikes);
    if (global_spikes.size()!=g_context->distributed->sum(local_spikes.size())) {
        return ::testing::AssertionFailure() << "the number of gathered spikes "
            << global_spikes.size() << " doesn't match the expected "
            << g_context->distributed->sum(local_spikes.size());
    }

    // generate the events
    std::vector<arb::pse_vector> queues(C.num_local_cells());
    C.make_event_queues(global_spikes, queues);
    if (queues.size() != D.num_groups()) { // one queue for each cell group
        return ::testing::AssertionFailure()
            << "expect one event queue for each cell group";
    }

    // Assert that all the correct events were generated.
    // Iterate over each local gid, and testing whether an event is expected for
    // that gid. If so, look up the event queue of the cell_group of gid, and
    // search for the expected event.
    int expected_count = 0;
    for (auto gid: gids) {
        // get the event queue that this gid belongs to
        auto& q = queues[group_map[gid]];
        for (auto src: spike_gids) {
            auto expected = expected_event_all2all(gid, src);
            if (std::find(q.begin(), q.end(), expected)==q.end()) {
                return ::testing::AssertionFailure()
                    << "expected event " << expected
                    << " from " << src << " was not found";
            }
            ++expected_count;
        }
    }

    // Assert that only the expected events were produced. The preceding test
    // showed that all expected events were generated, so this only requires
    // that the number of generated events is as expected.
    int num_events = std::accumulate(queues.begin(), queues.end(), 0,
            [](int l, decltype(queues.front())& r){return l + r.size();});

    if (expected_count!=num_events) {
        return ::testing::AssertionFailure() <<
            "the number of events " << num_events <<
            " does not match expected count " << expected_count;
    }

    return ::testing::AssertionSuccess();
}

TEST(communicator, all2all)
{
    using util::make_span;

    // construct a homogeneous network of 10*n_domain identical cells in a ring
    unsigned N = g_context->distributed->size();

    unsigned n_local = 10u;
    unsigned n_global = n_local*N;

    auto R = all2all_recipe(n_global);
    // use a node decomposition that reflects the resources available
    // on the node that the test is running on, including gpus.
    const auto D = partition_load_balance(R, g_context);

    // set up source and target label->lid resolvers
    // from cable_cell_group
    std::vector<cell_gid_type> mc_gids;
    for (auto g: D.groups()) {
        mc_gids.insert(mc_gids.end(), g.gids.begin(), g.gids.end());
    }
    cell_label_range local_sources, local_targets;
    auto mc_group = cable_cell_group(mc_gids, R, local_sources, local_targets, make_fvm_lowered_cell(backend_kind::multicore, *g_context));

    // construct the communicator
    auto C = communicator(R, D, *g_context);
    C.update_connections(R, D, label_resolution_map({local_targets, mc_gids}));
    auto connections = C.connections();

    for (auto i: util::make_span(0, n_global)) {
        for (auto j: util::make_span(0, n_local)) {
            auto c = connections[i*n_local+j];
            EXPECT_EQ(i, c.source.gid);
            EXPECT_EQ(0u, c.source.index);
            EXPECT_EQ(i, c.destination);
            EXPECT_LT(c.index_on_domain, n_local);
        }
    }

    // every cell fires
    EXPECT_TRUE(test_all2all(D, C, [](cell_gid_type g){return true;}));
    // only cell 0 fires
    EXPECT_TRUE(test_all2all(D, C, [](cell_gid_type g){return g==0u;}));
    // even-numbered cells fire
    EXPECT_TRUE(test_all2all(D, C, [](cell_gid_type g){return g%2==0;}));
    // odd-numbered cells fire
    EXPECT_TRUE(test_all2all(D, C, [](cell_gid_type g){return g%2==1;}));
}

TEST(communicator, mini_network)
{
    using util::make_span;

    // construct a homogeneous network of 10*n_domain identical cells in a ring
    unsigned N = g_context->distributed->size();

    auto R = mini_recipe(N);
    // use a node decomposition that reflects the resources available
    // on the node that the test is running on, including gpus.
    const auto D = partition_load_balance(R, g_context);

    // set up source and target label->lid resolvers
    // from cable_cell_group
    std::vector<cell_gid_type> gids;
    for (auto g: D.groups()) {
        gids.insert(gids.end(), g.gids.begin(), g.gids.end());
    }
    cell_label_range local_sources, local_targets;
    auto mc_group = cable_cell_group(gids, R, local_sources, local_targets, make_fvm_lowered_cell(backend_kind::multicore, *g_context));

    // construct the communicator
    auto C = communicator(R, D, *g_context);
    C.update_connections(R, D, label_resolution_map({local_targets, gids}));

    // sort connections by source then target
    auto connections = C.connections();
    util::sort(connections, [](const connection& lhs, const connection& rhs) {
      return std::forward_as_tuple(lhs.source, lhs.index_on_domain, lhs.destination) < std::forward_as_tuple(rhs.source, rhs.index_on_domain, rhs.destination);
    });

    // Expect one set of 22 connections from every rank: these have been sorted.
    std::vector<cell_lid_type> ex_source_lids =  {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3};
    std::vector<std::vector<cell_lid_type>> ex_target_lids = {{0, 0, 1, 0, 0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 1, 0, 1, 2, 0, 1, 2},
                                                              {0, 1, 1, 0, 1, 1, 0, 1, 2, 0, 1, 2, 0, 1, 0, 1, 0, 1, 2, 0, 1, 2}};

    for (auto i: util::make_span(0, N)) {
        std::vector<cell_gid_type> ex_source_gids(22u, i*3 + 1);
        for (unsigned j = 0; j < 22u; ++j) {
            auto c = connections[i*22 + j];
            EXPECT_EQ(ex_source_gids[j], c.source.gid);
            EXPECT_EQ(ex_source_lids[j], c.source.index);
            EXPECT_EQ(ex_target_lids[i%2][j], c.destination);
        }
    }
}
