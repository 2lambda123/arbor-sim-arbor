#include "../gtest.h"
#include "test.hpp"

#include <stdexcept>
#include <vector>

#include <communication/communicator.hpp>
#include <communication/distributed_context.hpp>
#include <hardware/node_info.hpp>
#include <load_balance.hpp>
#include <util/filter.hpp>
#include <util/rangeutil.hpp>
#include <util/span.hpp>

using namespace arb;

static bool is_dry_run() {
    //return global_policy::kind() == global_policy_kind::dryrun;
    return false;
}

TEST(communicator, policy_basics) {
    const auto num_domains = g_context.size();
    const auto rank = g_context.id();

    EXPECT_EQ(g_context.min(rank), 0);
    if (!is_dry_run()) {
        EXPECT_EQ(g_context.max(rank), num_domains-1);
    }
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
    const auto num_domains = g_context.size();
    const auto rank = g_context.id();

    const auto n_local_spikes = 10;

    /*
    const auto n_local_cells = n_local_spikes;
    // Important: set up meta-data in dry run back end.
    if (is_dry_run()) {
        g_context.set_sizes(g_context.size(), n_local_cells);
    }
    */

    // Create local spikes for communication.
    std::vector<spike> local_spikes;
    for (auto i=0; i<n_local_spikes; ++i) {
        local_spikes.push_back(gen_spike(i+rank*n_local_spikes, rank));
    }

    // Perform exchange
    const auto global_spikes = g_context.gather_spikes(local_spikes);

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
    EXPECT_EQ(n_local_spikes*g_context.size(), int(spikes.size()));
    for (auto i=0u; i<spikes.size(); ++i) {
        const auto s = spikes[i];
        EXPECT_EQ(i, unsigned(s.source.gid));
        if (is_dry_run()) {
            EXPECT_EQ(0, get_value(s));
        }
        else {
            EXPECT_EQ(int(i)/n_local_spikes, get_value(s));
        }
    }
}

// Test low level spike_gather function when the number of spikes per domain
// are not equal.
TEST(communicator, gather_spikes_variant) {
    // This test does not apply if in dry run mode.
    // Because dry run mode requires that each domain have the same
    // number of spikes.
    if (is_dry_run()) return;

    const auto num_domains = g_context.size();
    const auto rank = g_context.id();

    // Parameter used to scale the number of spikes generated on successive
    // ranks.
    const auto scale = 10;
    // Calculates the number of spikes generated by the first n ranks.
    // Can be used to calculate the index of the range of spikes
    // generated by a given rank, and to determine the total number of
    // spikes generated globally.
    auto sumn = [scale](int n) {return scale*n*(n+1)/2;};
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
    const auto global_spikes = g_context.gather_spikes(local_spikes);

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

namespace {
    // Population of cable and rss cells with ring connection topology.
    // Even gid are rss, and odd gid are cable cells.
    class ring_recipe: public recipe {
    public:
        ring_recipe(cell_size_type s):
            size_(s),
            ranks_(g_context.size())
        {}

        cell_size_type num_cells() const override {
            return size_;
        }

        util::unique_any get_cell_description(cell_gid_type) const override {
            return {};
        }

        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return gid%2? cell_kind::cable1d_neuron: cell_kind::regular_spike_source;
        }

        cell_size_type num_sources(cell_gid_type) const override { return 1; }
        cell_size_type num_targets(cell_gid_type) const override { return 1; }
        cell_size_type num_probes(cell_gid_type) const override { return 0; }

        std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
            // a single connection from the preceding cell, i.e. a ring
            // weight is the destination gid
            // delay is 1
            cell_member_type src = {gid==0? size_-1: gid-1, 0};
            cell_member_type dst = {gid, 0};
            return {cell_connection(
                        src, dst,   // end points
                        float(gid), // weight
                        1.0f)};     // delay
        }

    private:
        cell_size_type size_;
        cell_size_type ranks_;
    };

    cell_gid_type source_of(cell_gid_type gid, cell_size_type num_cells) {
        if (gid) {
            return gid-1;
        }
        return num_cells-1;
    }

    // gid expects an event from source_of(gid) with weight gid, and fired at
    // time source_of(gid).
    postsynaptic_spike_event expected_event_ring(cell_gid_type gid, cell_size_type num_cells) {
        auto sid = source_of(gid, num_cells);
        return {
            {gid, 0u},  // source
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
        all2all_recipe(cell_size_type s):
            size_(s),
            ranks_(g_context.size())
        {}

        cell_size_type num_cells() const override {
            return size_;
        }

        util::unique_any get_cell_description(cell_gid_type) const override {
            return {};
        }
        cell_kind get_cell_kind(cell_gid_type gid) const override {
            return gid%2? cell_kind::cable1d_neuron: cell_kind::regular_spike_source;
        }

        cell_size_type num_sources(cell_gid_type) const override { return 1; }
        cell_size_type num_targets(cell_gid_type) const override { return size_; }
        cell_size_type num_probes(cell_gid_type) const override { return 0; }

        std::vector<cell_connection> connections_on(cell_gid_type gid) const override {
            std::vector<cell_connection> cons;
            cons.reserve(size_);
            for (auto sid: util::make_span(0, size_)) {
                cell_connection con(
                        {sid, 0},       // source
                        {gid, sid},     // destination
                        float(gid+sid), // weight
                        1.0f);          // delay
                cons.push_back(con);
            }
            return cons;
        }

    private:
        cell_size_type size_;
        cell_size_type ranks_;
    };

    postsynaptic_spike_event expected_event_all2all(cell_gid_type gid, cell_gid_type sid) {
        return {
            {gid, sid},      // target, event from sid goes to synapse with index sid
            sid+1.0f,        // time (all conns have delay 1 ms)
            float(gid+sid)}; // weight
    }

    // make a list of the gids on the local domain
    std::vector<cell_gid_type> get_gids(const domain_decomposition& D) {
        std::vector<cell_gid_type> gids;
        for (auto i: util::make_span(0, D.groups.size())) {
            util::append(gids, D.groups[i].gids);
        }
        return gids;
    }

    // make a hash table mapping local gid to local cell_group index
    std::unordered_map<cell_gid_type, cell_gid_type>
    get_group_map(const domain_decomposition& D) {
        std::unordered_map<cell_gid_type, cell_gid_type> map;
        for (auto i: util::make_span(0, D.groups.size())) {
            for (auto gid: D.groups[i].gids) {
                map[gid] = i;
            }
        }
        return map;
    }
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
    if (global_spikes.size()!=g_context.sum(local_spikes.size())) {
        return ::testing::AssertionFailure() << "the number of gathered spikes "
            << global_spikes.size() << " doesn't match the expected "
            << g_context.sum(local_spikes.size());
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
        auto src = source_of(gid, D.num_global_cells);
        if (f(src)) {
            auto expected = expected_event_ring(gid, D.num_global_cells);
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
    unsigned N = g_context.size();

    unsigned n_local = 10u;
    unsigned n_global = n_local*N;

    auto R = ring_recipe(n_global);
    // use a node decomposition that reflects the resources available
    // on the node that the test is running on, including gpus.
    const auto D = partition_load_balance(R, hw::node_info(), &g_context);
    auto C = communicator(R, D, &g_context);

    // every cell fires
    EXPECT_TRUE(test_ring(D, C, [](cell_gid_type g){return true;}));
    // last cell in each domain fires
    EXPECT_TRUE(test_ring(D, C, [n_local](cell_gid_type g){return (g+1)%n_local == 0u;}));
    // even-numbered cells fire
    EXPECT_TRUE(test_ring(D, C, [n_local](cell_gid_type g){return g%2==0;}));
    // odd-numbered cells fire
    EXPECT_TRUE(test_ring(D, C, [n_local](cell_gid_type g){return g%2==1;}));
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
        filter(make_span(0, D.num_global_cells), f));

    // gather the global set of spikes
    auto global_spikes = C.exchange(local_spikes);
    if (global_spikes.size()!=g_context.sum(local_spikes.size())) {
        return ::testing::AssertionFailure() << "the number of gathered spikes "
            << global_spikes.size() << " doesn't match the expected "
            << g_context.sum(local_spikes.size());
    }

    // generate the events
    std::vector<arb::pse_vector> queues(C.num_local_cells());
    C.make_event_queues(global_spikes, queues);
    if (queues.size() != D.groups.size()) { // one queue for each cell group
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
    unsigned N = g_context.size();

    unsigned n_local = 10u;
    unsigned n_global = n_local*N;

    auto R = all2all_recipe(n_global);
    // use a node decomposition that reflects the resources available
    // on the node that the test is running on, including gpus.
    const auto D = partition_load_balance(R, hw::node_info(), &g_context);
    auto C = communicator(R, D, &g_context);

    // every cell fires
    EXPECT_TRUE(test_all2all(D, C, [](cell_gid_type g){return true;}));
    // only cell 0 fires
    EXPECT_TRUE(test_all2all(D, C, [n_local](cell_gid_type g){return g==0u;}));
    // even-numbered cells fire
    EXPECT_TRUE(test_all2all(D, C, [n_local](cell_gid_type g){return g%2==0;}));
    // odd-numbered cells fire
    EXPECT_TRUE(test_all2all(D, C, [n_local](cell_gid_type g){return g%2==1;}));
}
