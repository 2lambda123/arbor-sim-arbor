#include <utility>
#include <vector>

#include <arbor/assert.hpp>
#include <arbor/common_types.hpp>
#include <arbor/domain_decomposition.hpp>
#include <arbor/recipe.hpp>
#include <arbor/spike.hpp>

#include "algorithms.hpp"
#include "communication/gathered_vector.hpp"
#include "connection.hpp"
#include "distributed_context.hpp"
#include "execution_context.hpp"
#include "profile/profiler_macro.hpp"
#include "threading/threading.hpp"
#include "util/partition.hpp"
#include "util/rangeutil.hpp"
#include "util/span.hpp"
#include "util/prefetch.hpp"

#include "communication/communicator.hpp"

namespace arb {

communicator::communicator(const recipe& rec,
                           const domain_decomposition& dom_dec,
                           execution_context& ctx)
{
    distributed_ = ctx.distributed;
    thread_pool_ = ctx.thread_pool;

    num_domains_ = distributed_->size();
    num_local_groups_ = dom_dec.groups.size();
    num_local_cells_ = dom_dec.num_local_cells;

    // For caching information about each cell
    struct gid_info {
        using connection_list = decltype(std::declval<recipe>().connections_on(0));
        cell_gid_type gid;              // global identifier of cell
        cell_size_type index_on_domain; // index of cell in this domain
        connection_list conns;          // list of connections terminating at this cell
        gid_info() = default;           // so we can in a std::vector
        gid_info(cell_gid_type g, cell_size_type di, connection_list c):
            gid(g), index_on_domain(di), conns(std::move(c)) {}
    };

    // Make a list of local gid with their group index and connections
    //   -> gid_infos
    // Count the number of local connections (i.e. connections terminating on this domain)
    //   -> n_cons: scalar
    // Calculate and store domain id of the presynaptic cell on each local connection
    //   -> src_domains: array with one entry for every local connection
    // Also the count of presynaptic sources from each domain
    //   -> src_counts: array with one entry for each domain

    // Record all the gid in a flat vector.
    // These are used to map from local index to gid in the parallel loop
    // that populates gid_infos.
    std::vector<cell_gid_type> gids;
    gids.reserve(num_local_cells_);
    for (auto g: dom_dec.groups) {
        util::append(gids, g.gids);
    }
    // Build the connection information for local cells in parallel.
    std::vector<gid_info> gid_infos;
    gid_infos.resize(num_local_cells_);
    threading::parallel_for::apply(0, gids.size(), thread_pool_.get(),
        [&](cell_size_type i) {
            auto gid = gids[i];
            gid_infos[i] = gid_info(gid, i, rec.connections_on(gid));
        });

    cell_local_size_type n_cons =
        util::sum_by(gid_infos, [](const gid_info& g){ return g.conns.size(); });
    std::vector<unsigned> src_domains;
    src_domains.reserve(n_cons);
    std::vector<cell_size_type> src_counts(num_domains_);
    
    for (const auto& cell: gid_infos) {
        for (auto c: cell.conns) {
            auto src = dom_dec.gid_domain(c.source.gid);
            src_domains.push_back(src);
            src_counts[src]++;
        }
    }

    // Construct the connections.
    // The loop above gave the information required to construct in place
    // the connections as partitioned by the domain of their source gid.
    connections_.resize(n_cons);
    connection_part_ = algorithms::make_index(src_counts);
    auto offsets = connection_part_;
    std::size_t pos = 0;
    for (const auto& cell: gid_infos) {
        for (auto c: cell.conns) {
            const auto i = offsets[src_domains[pos]]++;
            connections_[i] = {c.source, c.dest, c.weight, c.delay, cell.index_on_domain};
            ++pos;
        }
    }

    // Build cell partition by group for passing events to cell groups
    index_part_ = util::make_partition(index_divisions_,
        util::transform_view(
            dom_dec.groups,
            [](const group_description& g){return g.gids.size();}));

    // Sort the connections for each domain.
    // This is num_domains_ independent sorts, so it can be parallelized trivially.
    const auto& cp = connection_part_;
    threading::parallel_for::apply(0, num_domains_, thread_pool_.get(),
        [&](cell_size_type i) {
            util::sort(util::subrange_view(connections_, cp[i], cp[i+1]));
        });
}

std::pair<cell_size_type, cell_size_type> communicator::group_queue_range(cell_size_type i) {
    arb_assert(i<num_local_groups_);
    return index_part_[i];
}

time_type communicator::min_delay() {
    auto local_min = std::numeric_limits<time_type>::max();
    for (auto& con : connections_) {
        local_min = std::min(local_min, con.delay());
    }

    return distributed_->min(local_min);
}

gathered_vector<spike> communicator::exchange(std::vector<spike> local_spikes) {
    PE(communication_exchange_sort);
    // sort the spikes in ascending order of source gid
    util::sort_by(local_spikes, [](spike s){return s.source;});
    PL();

    PE(communication_exchange_gather);
    // global all-to-all to gather a local copy of the global spike list on each node.
    auto global_spikes = distributed_->gather_spikes(local_spikes);
    num_spikes_ += global_spikes.size();
    PL();

    return global_spikes;
}

static void make_queues_by_conns(
    std::vector<pse_vector>& queues,
    std::vector<connection>::iterator cn,
    const std::vector<connection>::iterator cend,
    std::vector<spike>::const_iterator sp,
    const std::vector<spike>::const_iterator send,
    communicator::prefetch_spike_range_buffer& prefetch_buffer)
{
    using util::make_range;
    struct spike_pred {
        bool operator()(const spike& spk, const cell_member_type& src)
        {return spk.source<src;}
        bool operator()(const cell_member_type& src, const spike& spk)
        {return src<spk.source;}
    };

    auto&& d = prefetch::make_prefetch(
        prefetch::write,
        prefetch_buffer,
        [] (auto&& buf) {
            auto q = std::get<0>(buf);
            auto b = std::get<1>(buf);
            auto e = std::get<2>(buf);
            auto c = std::get<3>(buf);
            for (auto s: make_range(b, e)) {
                q->push_back(c->make_event(s));
            }
        });
    
    auto&& p = prefetch::make_prefetch(
        prefetch::write,
        prefetch_buffer,
        [&d] (auto&& buf) { // speculate that the next append is simple
            auto q = std::get<0>(buf);
            d.store(q->data()+q->size(), std::forward<decltype(buf)>(buf));
        });
    
    while (cn != cend && sp!=send) {
        auto spikes = std::equal_range(sp, send, cn->source(), spike_pred());
        if (spikes.first != spikes.second) {
            auto q = queues.begin() + cn->index_on_domain();
            p.store(q, q, spikes.first, spikes.second, cn);
        }
        
        sp = spikes.first; // should be first, range of connections may have same source
        ++cn;
    }
}

static void make_queues_by_spikes(
    std::vector<pse_vector>& queues,
    std::vector<connection>::iterator cn,
    const std::vector<connection>::iterator cend,
    std::vector<spike>::const_iterator sp,
    const std::vector<spike>::const_iterator send,
    communicator::prefetch_spike_buffer& prefetch_buffer)
{
    auto&& d = prefetch::make_prefetch(
        prefetch::write,
        prefetch_buffer,
        [] (auto&& buf) {
            auto q = std::get<0>(buf);
            auto s = std::get<1>(buf);
            auto c = std::get<2>(buf);
            q->push_back(c->make_event(*s)); 
        });

    auto&& p = prefetch::make_prefetch(
        prefetch::write,
        prefetch_buffer,
        [&d] (auto&& buf) {// speculate that the next append is simple
            auto q = std::get<0>(buf);
            d.store(q->data()+q->size(), std::forward<decltype(buf)>(buf));
        });

    while (cn!=cend && sp!=send) {
        auto targets = std::equal_range(cn, cend, sp->source);
        for (auto c = targets.first; c != targets.second; c++) {
            auto q = queues.begin() + c->index_on_domain();
            p.store(q, q, sp, c);
        }

        cn = targets.second; // range of connections with this source handled
        ++sp;
    }
}

void communicator::make_event_queues(
        const gathered_vector<spike>& global_spikes,
        std::vector<pse_vector>& queues)
{
    arb_assert(queues.size()==num_local_cells_);

    using util::subrange_view;
    using util::make_span;

    const auto& sp = global_spikes.partition();
    const auto& cp = connection_part_;
    for (auto dom: make_span(num_domains_)) {
        auto cons = subrange_view(connections_, cp[dom], cp[dom+1]);
        auto spks = subrange_view(global_spikes.values(), sp[dom], sp[dom+1]);

        // We have a choice of whether to walk spikes or connections:
        // i.e., we can iterate over the spikes, and for each spike search
        // the for connections that have the same source; or alternatively
        // for each connection, we can search the list of spikes for spikes
        // with the same source.
        //
        // We iterate over whichever set is the smallest, which has
        // complexity of order max(S log(C), C log(S)), where S is the
        // number of spikes, and C is the number of connections.
        if (cons.size()<spks.size()) {
            make_queues_by_conns(queues, cons.begin(), cons.end(), spks.begin(), spks.end(), prefetch_spike_range_buffer_); 
        }
        else {
            make_queues_by_spikes(queues, cons.begin(), cons.end(), spks.begin(), spks.end(), prefetch_spike_buffer_);
        }
    }
}

std::uint64_t communicator::num_spikes() const {
    return num_spikes_;
}

cell_size_type communicator::num_local_cells() const {
    return num_local_cells_;
}

const std::vector<connection>& communicator::connections() const {
    return connections_;
}

void communicator::reset() {
    num_spikes_ = 0;
}

} // namespace arb

