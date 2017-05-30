#include "../gtest.h"

#include "fs_cell.hpp"

using namespace  nest::mc;

TEST(fs_cell, constructor)
{
    fs_cell test(0.0, 0.01, 1.0);
}


TEST(fs_cell, basic_usage)
{
    fs_cell sut(0.1, 0.01, 0.2);


    // no spikes in this time frame
    auto spikes = sut.spikes_until(0.09);
    EXPECT_EQ(size_t(0), spikes.size());

    //only on in this time frame
    spikes = sut.spikes_until(0.11);
    EXPECT_EQ(size_t(1), spikes.size());

    // Reset the internal state to null
    sut.reset();

    // Expect 10 excluding the 0.2
    spikes = sut.spikes_until(0.2);
    EXPECT_EQ(size_t(10), spikes.size());
}


TEST(fs_cell, poll_time_after_end_time)
{
    fs_cell sut(0.1, 0.01, 0.2);

    // no spikes in this time frame
    auto spikes = sut.spikes_until(0.3);
    EXPECT_EQ(size_t(10), spikes.size());

    // now ask for spikes for a time slot already passed.
    spikes = sut.spikes_until(0.2);
    // It should result in zero spikes because of the internal state!
    EXPECT_EQ(size_t(0), spikes.size());

    sut.reset();

    // Expect 10 excluding the 0.2
    spikes = sut.spikes_until(0.2);
    EXPECT_EQ(size_t(10), spikes.size());
}

TEST(fs_cell, cell_kind_correct)
{
    fs_cell sut(0.1, 0.01, 0.2);

    EXPECT_EQ(cell_kind::regular_spike_source, sut.get_cell_kind());
}
