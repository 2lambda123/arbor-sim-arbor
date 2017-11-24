#include "../gtest.h"

#include <ipss_cell.hpp>
#include <ipss_cell_group.hpp>

#include "../simple_recipes.hpp"
#include <random>

#include <iostream>


using namespace arb;
using namespace std;

using ipss_recipe = homogeneous_recipe<cell_kind::inhomogeneous_poisson_spike_source, ipss_cell>;








TEST(ipss_cell_group, basic_usage)
{
    // Create an array of spike times for 1000 ms of time using the same
    // seed for the random number generator (in one go)
    // Then let the cell_group generate the spikes, they should be the same
    time_type begin = 0;
    time_type end = 1000.0;

    time_type rate = 20;  // Hz
    std::vector<std::pair<time_type, double>> rates_per_time;
    rates_per_time.push_back({ 0.0, rate });
    time_type sample_delta = 0.1; // 0.1 ms

    constexpr time_type dt = 0.01; // dt is ignored by ipss_cell_group::advance().

    // Create the generator
    std::mt19937 gen(0);
    auto distribution = std::uniform_real_distribution<float>(0.f, 1.0f);
    auto prop_per_time_step = (rate / 1000.0) * sample_delta;

    // Create the spikes and store in the correct way
    std::vector<spike> spikes;
    for (time_type t = begin; t < end; t += sample_delta) {
        if (distribution(gen) < prop_per_time_step) {
            spikes.push_back({ { 0, 0 }, t });
        }
    }

    // Create the cell_group
    ipss_cell desc{ begin, end, sample_delta, rates_per_time, false};
    ipss_cell_group sut({0}, ipss_recipe(1u, desc));
    std::vector<spike> spikes_from_cell;
    for (int idx = 0; idx < 10; ++idx) {
        epoch ep(100.0 * idx, 100.0 * idx + 100.0);
        sut.advance(ep, dt, {});
        spikes_from_cell.insert(spikes_from_cell.end(), sut.spikes().begin(), sut.spikes().end());
        sut.clear_spikes();

    }

    // Check the output of the cell
    EXPECT_EQ(spikes.size(), spikes_from_cell.size());
    for (auto s1 = spikes.begin(), s2 = spikes_from_cell.begin(); s1 < spikes.end(); ++s1, ++s2) {
        ASSERT_FLOAT_EQ(s1->time, s2->time);
    }
}


TEST(ipss_cell_group, test_reset)
{
    // Create an array of spike times for 1000 ms of time using the same
    // seed for the random number generator (in one go)
    // Then let the cell_group generate the spikes, they should be the same
    time_type begin = 0;
    time_type end = 1000.0;
    time_type rate = 20;  // Hz
    std::vector<std::pair<time_type, double>> rates_per_time;
    rates_per_time.push_back({ 0.0, rate });
    time_type sample_delta = 0.1; // 0.1 ms

    constexpr time_type dt = 0.01; // dt is ignored by ipss_cell_group::advance().

                                   // Create the generator
    std::mt19937 gen(0);
    auto distribution = std::uniform_real_distribution<float>(0.f, 1.0f);
    auto prop_per_time_step = (rate / 1000.0) * sample_delta;

    // Create the spikes and store in the correct way
    std::vector<spike> spikes;
    for (time_type t = begin; t < end; t += sample_delta) {
        if (distribution(gen) < prop_per_time_step) {
            spikes.push_back({ { 0, 0 }, t });
        }
    }

    // Create the cell_group
    ipss_cell desc{ begin, end, sample_delta, rates_per_time, false };
    ipss_cell_group sut({ 0 }, ipss_recipe(1u, desc));

    // Run the cell_group for some time
    epoch ep(0, 10);
    sut.advance(ep, dt, {});

    // Reset the cell to zero
    sut.reset();

    // And now generate the same spikes as the basic test
    std::vector<spike> spikes_from_cell;
    for (int idx = 0; idx < 10; ++idx) {
        epoch ep(100.0 * idx, 100.0 * idx + 100.0);
        sut.advance(ep, dt, {});
        spikes_from_cell.insert(spikes_from_cell.end(), sut.spikes().begin(), sut.spikes().end());
        sut.clear_spikes();

    }

    // Check the output of the cell
    EXPECT_EQ(spikes.size(), spikes_from_cell.size());
    for (auto s1 = spikes.begin(), s2 = spikes_from_cell.begin(); s1 < spikes.end(); ++s1, ++s2) {
        ASSERT_FLOAT_EQ(s1->time, s2->time);
    }
}


TEST(ipss_cell_group, start_end_different_then_zero)
{
    // Create an array of spike times for 1000 ms of time using the same
    // seed for the random number generator (in one go)
    // Then let the cell_group generate the spikes, they should be the same
    time_type begin = 50;
    time_type end = 500.0;
    time_type rate = 20;  // Hz
    std::vector<std::pair<time_type, double>> rates_per_time;
    rates_per_time.push_back({ 0.0, rate });
    time_type sample_delta = 0.1; // 0.1 ms

    constexpr time_type dt = 0.01; // dt is ignored by ipss_cell_group::advance().

                                   // Create the generator
    std::mt19937 gen(0);
    auto distribution = std::uniform_real_distribution<float>(0.f, 1.0f);
    auto prop_per_time_step = (rate / 1000.0) * sample_delta;

    // Create the spikes and store in the correct way
    std::vector<spike> spikes;
    for (time_type t = begin; t < end; t += sample_delta) {
        if (distribution(gen) < prop_per_time_step) {
            spikes.push_back({ { 0, 0 }, t });
        }
    }

    // Create the cell_group
    ipss_cell desc{ begin, end, sample_delta, rates_per_time, false };
    ipss_cell_group sut({ 0 }, ipss_recipe(1u, desc));
    std::vector<spike> spikes_from_cell;
    for (int idx = 0; idx < 10; ++idx) {
        epoch ep(100.0 * idx, 100.0 * idx + 100.0);
        sut.advance(ep, dt, {});  // We advance past the end time
        spikes_from_cell.insert(spikes_from_cell.end(), sut.spikes().begin(), sut.spikes().end());
        sut.clear_spikes();

    }

    EXPECT_EQ(spikes.size(), spikes_from_cell.size());


    for (auto s1 = spikes.begin(), s2 = spikes_from_cell.begin(); s1 < spikes.end(); ++s1, ++s2) {
        ASSERT_FLOAT_EQ(s1->time, s2->time);
    }
}


TEST(ipss_cell_group, cell_kind_correct)
{
    std::vector<std::pair<time_type, double>> rates_per_time;
    rates_per_time.push_back({ 0.0, 20 });
    ipss_cell desc{0.1, 0.01, 0.2, rates_per_time};
    ipss_cell_group sut({0}, ipss_recipe(1u, desc));

    EXPECT_EQ(cell_kind::inhomogeneous_poisson_spike_source, sut.get_cell_kind());
}


TEST(ipss_cell_group, start_before_first_rate_change)
{
    std::vector<std::pair<time_type, double>> rates_per_time;
    rates_per_time.push_back({ 0.11, 20 });
    ipss_cell desc{ 0.1, 0.01, 0.2, rates_per_time };

    // Gtest does not have the expect_exception shorthand
    try {
        ipss_cell_group sut({ 0 }, ipss_recipe(1u, desc));
        FAIL() << "Expected a failure";
    }
    catch (std::logic_error const & err)
    {
        EXPECT_EQ(err.what(), std::string("The start time of the neuron is before the first time/rate pair"));
    }
    catch (...)
    {
        FAIL() << "Expected logic_error but different exception encountered";
    }
}