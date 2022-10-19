#pragma once

#include <vector>

#include <arbor/export.hpp>
#include <arbor/common_types.hpp>
#include <arbor/recipe.hpp>
#include <arbor/sampling.hpp>
#include <arbor/schedule.hpp>
#include <arbor/spike.hpp>
#include <arbor/spike_source_cell.hpp>

#include "cell_group.hpp"
#include "epoch.hpp"
#include "label_resolution.hpp"

namespace arb {

class ARB_ARBOR_API spike_source_cell_group: public cell_group {
public:
    spike_source_cell_group(const std::vector<cell_gid_type>& gids, const recipe& rec, cell_label_range& cg_sources, cell_label_range& cg_targets);

    cell_kind get_cell_kind() const override;

    void advance(epoch ep, time_type dt, const event_lane_subrange& event_lanes) override;

    void reset() override;

    void set_binning_policy(binning_kind policy, time_type bin_interval) override {}

    const std::vector<spike>& spikes() const override;

    void clear_spikes() override;

    void add_sampler(sampler_association_handle h, cell_member_predicate probeset_ids, schedule sched, sampler_function fn, sampling_policy policy) override;

    void remove_sampler(sampler_association_handle h) override {}

    void remove_all_samplers() override {}

private:
    std::vector<spike> spikes_;
    std::vector<cell_gid_type> gids_;
    std::vector<std::vector<schedule>> time_sequences_;
};

cell_size_type ARB_ARBOR_API get_sources(cell_label_range& src, const spike_source_cell& c);

} // namespace arb

