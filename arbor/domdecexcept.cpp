#include <string>

#include <arbor/domdecexcept.hpp>

#include "util/strprintf.hpp"

#include <fmt/format.h>

namespace arb {

using util::to_string;

invalid_gj_cell_group::invalid_gj_cell_group(cell_gid_type gid_0, cell_gid_type gid_1):
    dom_dec_exception(fmt::format("cell {} needs to be in the same group as cell {} because they are connected via gap-junction.",
                              gid_0, gid_1)),
    gid_0(gid_0),
    gid_1(gid_1)
{}

invalid_sum_local_cells::invalid_sum_local_cells(unsigned gc_wrong, unsigned gc_right):
    dom_dec_exception(fmt::format("sum of local cells on the individual ranks ({}) is not equal to the total number of cells in the recipe ({}).",
                              gc_wrong, gc_right)),
    gc_wrong(gc_wrong),
    gc_right(gc_right)
{}

duplicate_gid::duplicate_gid(cell_gid_type gid):
    dom_dec_exception(fmt::format("gid {} is present in multiple cell-groups or multiple times in the same cell group.",
                              gid)),
    gid(gid)
{}

out_of_bounds::out_of_bounds(cell_gid_type gid, unsigned num_cells):
    dom_dec_exception(fmt::format("cell {} is out-of-bounds of the allowed gids in the simulation which has {} total cells.",
                              gid, num_cells)),
    gid(gid),
    num_cells(num_cells)
{}

invalid_backend::invalid_backend(int rank):
    dom_dec_exception(fmt::format("rank {} contains a group meant to run on GPU, but no GPU backend was detected in the context.",
                              rank)),
    rank(rank)
{}

incompatible_backend::incompatible_backend(int rank, cell_kind kind):
    dom_dec_exception(fmt::format("rank {} contains a group with cells of kind {1} meant to run on the GPU backend, "
                                  "but no GPU backend support exists for {1}",
                                  rank, to_string(kind))),
    rank(rank),
    kind(kind)
{}

} // namespace arb

