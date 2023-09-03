#pragma once

#include <vector>
#include <unordered_map>

#include <arbor/common_types.hpp>
#include <arbor/context.hpp>
#include <arbor/domain_decomposition.hpp>
#include <arbor/export.hpp>
#include <arbor/recipe.hpp>
#include <arbor/spike.hpp>

#include "communication/gathered_vector.hpp"
#include "connection.hpp"
#include "epoch.hpp"
#include "execution_context.hpp"
#include "util/partition.hpp"

namespace arb {

// When the communicator is constructed the number of target groups and targets
// is specified, along with a mapping between local cell id and local
// target id.
//
// The user can add connections to an existing communicator object, where
// each connection is between any global cell and any local target.
//
// Once all connections have been specified, the construct() method can be used
// to build the data structures required for efficient spike communication and
// event generation.

class ARB_ARBOR_API communicator {
public:

    struct spikes {
        gathered_vector<spike> from_local;
        std::vector<spike> from_remote;
    };

    communicator() = default;

    explicit communicator(const recipe& rec,
                          const domain_decomposition& dom_dec,
                          context ctx);

    /// The range of event queues that belong to cells in group i.
    std::pair<cell_size_type, cell_size_type> group_queue_range(cell_size_type i);

    /// The minimum delay of all connections in the global network.
    time_type min_delay();

    /// Perform exchange of spikes.
    ///
    /// Takes as input the list of local_spikes that were generated on the calling domain.
    /// Returns
    /// * full global set of vectors, along with meta data about their partition
    /// * a list of spikes received from remote simulations
    spikes exchange(std::vector<spike> local_spikes);

    /// Check each global spike in turn to see it generates local events.
    /// If so, make the events and insert them into the appropriate event list.
    ///
    /// Takes reference to a vector of event lists as an argument, with one list
    /// for each local cell group. On completion, the events in each list are
    /// all events that must be delivered to targets in that cell group as a
    /// result of the global spike exchange, plus any events that were already
    /// in the list.
    void make_event_queues(
            const gathered_vector<spike>& global_spikes,
            std::vector<pse_vector>& queues,
            const std::vector<spike>& external_spikes={});

    /// Returns the total number of global spikes over the duration of the simulation
    std::uint64_t num_spikes() const;
    void set_num_spikes(std::uint64_t n);

    cell_size_type num_local_cells() const;

    const std::vector<connection>& connections() const;

    void reset();

    // used for commmunicate to coupled simulations
    void remote_ctrl_send_continue(const epoch&);
    void remote_ctrl_send_done();

    void update_connections(const recipe& rec,
                            const domain_decomposition& dom_dec,
                            const label_resolution_map& source_resolution_map,
                            const label_resolution_map& target_resolution_map);

    void set_remote_spike_filter(const spike_predicate&);

private:
    cell_size_type num_total_cells_ = 0;
    cell_size_type num_local_cells_ = 0;
    cell_size_type num_local_groups_ = 0;
    cell_size_type num_domains_ = 0;
    // Arbor internal connections
    std::vector<connection> connections_;
    // partition of connections over the domains of the sources' ids.
    std::vector<cell_size_type> connection_part_;
    std::vector<cell_size_type> index_divisions_;
    util::partition_view_type<std::vector<cell_size_type>> index_part_;

    spike_predicate remote_spike_filter_;

    // Connections from external simulators into Arbor.
    // Currently we have no partitions/indices/acceleration structures
    std::vector<connection> ext_connections_;

    std::uint64_t num_spikes_ = 0u;
    std::uint64_t num_local_events_ = 0u;
    context ctx_;
};

} // namespace arb
