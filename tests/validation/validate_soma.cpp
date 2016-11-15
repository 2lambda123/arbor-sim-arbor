#include "validate_soma.hpp"

#include "../gtest.h"

using lowered_cell = nest::mc::fvm::fvm_multicell<multicore::backend>;

TEST(soma, numeric_ref) {
    validate_soma<lowered_cell>();
}
