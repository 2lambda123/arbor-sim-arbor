#include "../test/gtest.h"

#include <vector>

#include <arbor/morph/embed_pwlin.hpp>
#include <arbor/morph/locset.hpp>
#include <arbor/morph/morphexcept.hpp>
#include <arbor/morph/morphology.hpp>
#include <arbor/morph/mprovider.hpp>
#include <arbor/morph/primitives.hpp>
#include <arbor/morph/region.hpp>
#include <arbor/morph/sample_tree.hpp>

#include "util/span.hpp"
#include "util/strprintf.hpp"

using namespace arb;
using embedding = embed_pwlin;

::testing::AssertionResult cable_eq(mcable a, mcable b) {
    if (a.branch!=b.branch) {
        return ::testing::AssertionFailure()
            << "cables " << a << " and " << b << " differ";
    }

    using FP = testing::internal::FloatingPoint<double>;
    if (FP(a.prox_pos).AlmostEquals(FP(b.prox_pos)) && FP(a.dist_pos).AlmostEquals(FP(b.dist_pos))) {
        return ::testing::AssertionSuccess();
    }
    else {
        return ::testing::AssertionFailure()
            << "cables " << a << " and " << b << " differ";
    }
}

::testing::AssertionResult cablelist_eq(const mcable_list& as, const mcable_list& bs) {
    if (as.size()!=bs.size()) {
        return ::testing::AssertionFailure()
            << "cablelists " << as << " and " << bs << " differ";
    }

    for (auto i: util::count_along(as)) {
        auto result = cable_eq(as[i], bs[i]);
        if (!result) return ::testing::AssertionFailure()
            << "cablelists " << as << " and " << bs << " differ";
    }
    return ::testing::AssertionSuccess();
}

TEST(region, expr_repn) {
    using util::to_string;

    auto c1 = reg::cable({1, 0, 1});
    auto c2 = reg::cable({4, 0.125, 0.5});
    auto c3 = join(reg::cable({4, 0.125, 0.5}), reg::cable({3, 0, 1}));
    auto b1 = reg::branch(1);
    auto t1 = reg::tagged(1);
    auto t2 = reg::tagged(2);
    auto t3 = reg::tagged(3);
    auto all = reg::all();

    EXPECT_EQ(to_string(c1), "(cable 1 0 1)");
    EXPECT_EQ(to_string(c2), "(cable 4 0.125 0.5)");
    EXPECT_EQ(to_string(c3), "(join (cable 4 0.125 0.5) (cable 3 0 1))");
    EXPECT_EQ(to_string(b1), "(cable 1 0 1)");
    EXPECT_EQ(to_string(t1), "(tag 1)");
    EXPECT_EQ(to_string(t2), "(tag 2)");
    EXPECT_EQ(to_string(intersect(c1, t2)), "(intersect (cable 1 0 1) (tag 2))");
    EXPECT_EQ(to_string(join(c1, t2)),  "(join (cable 1 0 1) (tag 2))");
    EXPECT_EQ(to_string(join(t1, t2, t3)), "(join (join (tag 1) (tag 2)) (tag 3))");
    EXPECT_EQ(to_string(intersect(t1, t2, t3)), "(intersect (intersect (tag 1) (tag 2)) (tag 3))");
    EXPECT_EQ(to_string(intersect(join(c1, t2), c2)),  "(intersect (join (cable 1 0 1) (tag 2)) (cable 4 0.125 0.5))");
    EXPECT_EQ(to_string(all), "all");
}

TEST(region, invalid_mcable) {
    EXPECT_NO_THROW(reg::cable({123, 0.5, 0.8}));
    EXPECT_THROW(reg::cable({1, 0, 1.1}), invalid_mcable);
    EXPECT_THROW(reg::branch(-1), invalid_mcable);
}

TEST(locset, expr_repn) {
    using util::to_string;

    auto root = ls::root();
    auto term = ls::terminal();
    auto samp = ls::sample(42);
    auto loc = ls::location({2, 0.5});

    EXPECT_EQ(to_string(root), "root");
    EXPECT_EQ(to_string(term), "terminal");
    EXPECT_EQ(to_string(sum(root, term)), "(sum root terminal)");
    EXPECT_EQ(to_string(sum(root, term, samp)), "(sum (sum root terminal) (sample 42))");
    EXPECT_EQ(to_string(sum(root, term, samp, loc)), "(sum (sum (sum root terminal) (sample 42)) (location 2 0.5))");
    EXPECT_EQ(to_string(samp), "(sample 42)");
    EXPECT_EQ(to_string(loc), "(location 2 0.5)");
}

TEST(region, invalid_mlocation) {
    // Location positions have to be in the range [0,1].
    EXPECT_NO_THROW(ls::location({123, 0.0}));
    EXPECT_NO_THROW(ls::location({123, 0.02}));
    EXPECT_NO_THROW(ls::location({123, 1.0}));

    EXPECT_THROW(ls::location({0, 1.5}), invalid_mlocation);
    EXPECT_THROW(ls::location({unsigned(-1), 0.}), invalid_mlocation);
}

// Name evaluation (thingify) tests:

TEST(locset, thingify_named) {
    using pvec = std::vector<msize_t>;
    using svec = std::vector<msample>;

    locset banana = ls::root();
    locset cake = ls::terminal();

    sample_tree sm(svec{ {{0,0,0,1},1}, {{10,0,0,1},1} }, pvec{mnpos, 0});
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);

        mprovider mp(morphology(sm, false), dict);
        EXPECT_EQ(thingify(locset("cake"), mp), thingify(cake, mp));
        EXPECT_EQ(thingify(locset("banana"), mp), thingify(banana, mp));

        EXPECT_THROW(thingify(locset("durian"), mp), unbound_name);
    }
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);
        dict.set("topping", locset("fruit"));
        dict.set("fruit", locset("strawberry"));

        EXPECT_THROW(mprovider(morphology(sm, false), dict), unbound_name);
    }
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);
        dict.set("topping", locset("fruit"));
        dict.set("fruit", sum(locset("banana"), locset("topping")));

        EXPECT_THROW(mprovider(morphology(sm, false), dict), circular_definition);
    }
}

TEST(region, thingify_named) {
    using pvec = std::vector<msize_t>;
    using svec = std::vector<msample>;

    region banana = reg::branch(0);
    region cake = reg::cable(mcable{0, 0.2, 0.3});

    // copy-paste ftw

    sample_tree sm(svec{ {{0,0,0,1},1}, {{10,0,0,1},1} }, pvec{mnpos, 0});
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);

        mprovider mp(morphology(sm, false), dict);
        EXPECT_EQ(thingify(region("cake"), mp), thingify(cake, mp));
        EXPECT_EQ(thingify(region("banana"), mp), thingify(banana, mp));

        EXPECT_THROW(thingify(region("durian"), mp), unbound_name);
    }
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);
        dict.set("topping", region("fruit"));
        dict.set("fruit", region("strawberry"));

        EXPECT_THROW(mprovider(morphology(sm, false), dict), unbound_name);
    }
    {
        label_dict dict;
        dict.set("banana", banana);
        dict.set("cake", cake);
        dict.set("topping", region("fruit"));
        dict.set("fruit", join(region("cake"), region("topping")));

        EXPECT_THROW(mprovider(morphology(sm, false), dict), circular_definition);
    }
}

// Embedded evaluation (thingify) tests:

TEST(locset, thingify) {
    using pvec = std::vector<msize_t>;
    using svec = std::vector<msample>;
    using ll = mlocation_list;
    auto root = ls::root();
    auto term = ls::terminal();
    auto samp = ls::sample(4);
    auto midb2 = ls::location({2, 0.5});
    auto midb1 = ls::location({1, 0.5});
    auto begb0 = ls::location({0, 0});
    auto begb1 = ls::location({1, 0});
    auto begb2 = ls::location({2, 0});
    auto begb3 = ls::location({3, 0});
    auto begb4 = ls::location({4, 0});

    // Eight samples
    //
    //            0
    //           1 3
    //          2   4
    //             5 6
    //                7
    pvec parents = {mnpos, 0, 1, 0, 3, 4, 4, 6};
    svec samples = {
        {{  0,  0,  0,  2}, 3},
        {{ 10,  0,  0,  2}, 3},
        {{100,  0,  0,  2}, 3},
        {{  0, 10,  0,  2}, 3},
        {{  0,100,  0,  2}, 3},
        {{100,100,  0,  2}, 3},
        {{  0,200,  0,  2}, 3},
        {{  0,300,  0,  2}, 3},
    };
    sample_tree sm(samples, parents);

    {
        mprovider mp(morphology(sm, true));

        EXPECT_EQ(thingify(root, mp),  (ll{{0,0}}));
        EXPECT_EQ(thingify(term, mp),  (ll{{1,1},{3,1},{4,1}}));
        EXPECT_EQ(thingify(samp, mp),  (ll{{2,1}}));
        EXPECT_EQ(thingify(midb2, mp), (ll{{2,0.5}}));
        EXPECT_EQ(thingify(midb1, mp), (ll{{1,0.5}}));
        EXPECT_EQ(thingify(begb0, mp), (ll{{0,0}}));
        EXPECT_EQ(thingify(begb1, mp), (ll{{1,0}}));
        EXPECT_EQ(thingify(begb2, mp), (ll{{2,0}}));
        EXPECT_EQ(thingify(begb3, mp), (ll{{3,0}}));
        EXPECT_EQ(thingify(begb4, mp), (ll{{4,0}}));
    }
    {
        mprovider mp(morphology(sm, false));

        EXPECT_EQ(thingify(root, mp),  (ll{{0,0}}));
        EXPECT_EQ(thingify(term, mp),  (ll{{0,1},{2,1},{3,1}}));
        EXPECT_EQ(thingify(samp, mp),  (ll{{1,1}}));
        EXPECT_EQ(thingify(midb2, mp), (ll{{2,0.5}}));
        EXPECT_EQ(thingify(midb1, mp), (ll{{1,0.5}}));
        EXPECT_EQ(thingify(begb0, mp), (ll{{0,0}}));
        EXPECT_EQ(thingify(begb1, mp), (ll{{1,0}}));
        EXPECT_EQ(thingify(begb2, mp), (ll{{2,0}}));
        EXPECT_EQ(thingify(begb3, mp), (ll{{3,0}}));

        // In the absence of a spherical root, there is no branch 4.
        EXPECT_THROW(thingify(begb4, mp), no_such_branch);
    }
    {
        mprovider mp(morphology(sm, false));

        auto all = reg::all();
        auto ls0 = thingify(ls::uniform(all, 0, 10, 12), mp);
        auto ls1 = thingify(ls::uniform(all, 5,  7, 12), mp);
        auto ls2 = thingify(ls::uniform(all, 0, 10, 13), mp);
        auto ls3 = thingify(ls::uniform(all, 2,  6, 12), mp);
        auto ls4 = thingify(ls::uniform(all, 5, 12, 12), mp);

        bool found_all = true;
        for (auto l: ls1) {
            auto it = std::find(ls0.begin(), ls0.end(), l);
            if (it == ls0.end()) {
                found_all = false;
            }
        }
        EXPECT_TRUE(found_all);

        bool found_none = true;
        for (auto l: ls2) {
            auto it = std::find(ls0.begin(), ls0.end(), l);
            if (it != ls0.end()) {
                found_none = false;
            }
        }
        EXPECT_TRUE(found_none);

        int found = 0;
        for (auto l: ls3) {
            auto it = std::find(ls1.begin(), ls1.end(), l);
            if (it != ls1.end()) found++;
        }
        EXPECT_TRUE(found == 1);

        found = 0;
        for (auto l: ls4) {
            auto it = std::find(ls1.begin(), ls1.end(), l);
            if (it != ls1.end()) found++;
        }
        EXPECT_TRUE(found == 2);
    }
    {
        mprovider mp(morphology(sm, false));

        mcable c0{0, 0.2, 0.7};
        mcable c1{1, 0.1, 1};
        mcable c3{3, 0.5, 0.6};
        auto sub_reg = join(reg::cable(c0), reg::cable(c1), reg::cable(c3));

        auto ls0 = thingify(ls::uniform(sub_reg, 0, 10000, 72), mp);
        for (auto l: ls0) {
            switch(l.branch) {
                case 0: {
                    if (l.pos < c0.prox_pos || l.pos > c0.dist_pos) FAIL();
                    break;
                }
                case 1: {
                    if (l.pos < c1.prox_pos || l.pos > c1.dist_pos) FAIL();
                    break;
                }
                case 3: {
                    if (l.pos < c3.prox_pos || l.pos > c3.dist_pos) FAIL();
                    break;
                }
                default: {
                    FAIL();
                    break;
                }
            }
            SUCCEED();
        }
    }
}

TEST(region, thingify) {
    using pvec = std::vector<msize_t>;
    using svec = std::vector<msample>;
    using cl = mcable_list;

    // A single unbranched cable with 5 sample points.
    // The cable has length 10 μm, with samples located at
    // 0 μm, 1 μm, 3 μm, 7 μm and 10 μm.
    {
        pvec parents = {mnpos, 0, 1, 2, 3};
        svec samples = {
            {{  0,  0,  0,  2}, 1},
            {{  1,  0,  0,  2}, 1},
            {{  3,  0,  0,  2}, 2},
            {{  7,  0,  0,  2}, 1},
            {{ 10,  0,  0,  2}, 2},
        };
        sample_tree sm(samples, parents);
        mprovider mp(morphology(sm, false));

        auto h1  = reg::cable({0, 0, 0.5});
        auto h2  = reg::cable({0, 0.5, 1});
        auto t1  = reg::tagged(1);
        auto t2  = reg::tagged(2);
        auto all = reg::all();

        // Concrete cable lists
        cl h1_{{0, 0.0, 0.5}};
        cl h2_{{0, 0.5, 1.0}};
        cl t1_{{0, 0.0, 0.1}, {0, 0.3, 0.7}};
        cl t2_{{0, 0.1, 0.3}, {0, 0.7, 1.0}};
        cl all_{{0, 0, 1}};
        cl empty_{};

        EXPECT_EQ(thingify(h1, mp), h1_);
        EXPECT_EQ(thingify(h2, mp), h2_);
        EXPECT_EQ(thingify(join(h1, h2), mp), all_);
        EXPECT_EQ(thingify(intersect(h1, h2), mp), (cl{{0, 0.5, 0.5}}));

        EXPECT_TRUE(cablelist_eq(thingify(t1, mp), t1_));
        EXPECT_TRUE(cablelist_eq(thingify(t2, mp), t2_));
        EXPECT_TRUE(cablelist_eq(thingify(intersect(h1, h1), mp), h1_));
        EXPECT_TRUE(cablelist_eq(thingify(intersect(t1, t1), mp), t1_));
        EXPECT_TRUE(cablelist_eq(thingify(join(t1, t2), mp), all_));
        EXPECT_TRUE(cablelist_eq(thingify(intersect(all, t1), mp), t1_));
        EXPECT_TRUE(cablelist_eq(thingify(intersect(all, t2), mp), t2_));
        EXPECT_TRUE(cablelist_eq(thingify(join(all, t1), mp), all_));
        EXPECT_TRUE(cablelist_eq(thingify(join(all, t2), mp), all_));
        EXPECT_TRUE(cablelist_eq(thingify(join(h1, t1), mp), (cl{{0, 0, 0.7}})));
        EXPECT_TRUE(cablelist_eq(thingify(join(h1, t2), mp), (cl{{0, 0, 0.5}, {0, 0.7, 1}})));
        EXPECT_TRUE(cablelist_eq(thingify(intersect(h2, t1), mp), (cl{{0, 0.5, 0.7}})));
    }


    // Test handling of spherical soma on multi-branch morphologies
    //
    //  sample ids:
    //              0           |
    //            1   3         |
    //          2       4       |
    //  tags:
    //              1           |
    //            3   2         |
    //          3       2       |
    {
        pvec parents = {mnpos, 0, 1, 0, 3};
        svec samples = {
            {{  0,  0,  0,  2}, 1},
            {{ 10,  0,  0,  2}, 3},
            {{100,  0,  0,  2}, 3},
            {{  0, 10,  0,  2}, 2},
            {{  0,100,  0,  2}, 2},
        };

        // with a spherical root
        sample_tree sm(samples, parents);
        mprovider mp(morphology(sm, true));

        using reg::tagged;
        using reg::distal_interval;
        using reg::proximal_interval;
        using reg::branch;
        using reg::cable;
        using reg::all;

        mcable start1_{1,0,0};
        mcable end1_{1,1,1};

        auto reg0_ = distal_interval(cable(start1_), 45);
        auto reg1_ = proximal_interval(cable(end1_), 45);
        auto reg2_ = proximal_interval(cable(end1_), 91);

        EXPECT_EQ(thingify(tagged(1), mp), (mcable_list{{0,0,1}}));
        EXPECT_EQ(thingify(tagged(2), mp), (mcable_list{{2,0,1}}));
        EXPECT_EQ(thingify(tagged(3), mp), (mcable_list{{1,0,1}}));
        EXPECT_EQ(thingify(join(tagged(1), tagged(2), tagged(3)), mp), (mcable_list{{0,0,1}, {1,0,1}, {2,0,1}}));
        EXPECT_EQ(thingify(join(tagged(1), tagged(2), tagged(3)), mp), thingify(all(), mp));
        EXPECT_EQ(thingify(reg0_, mp), (mcable_list{{1,0,0.5}}));
        EXPECT_EQ(thingify(reg1_, mp), (mcable_list{{1,0.5,1}}));
        EXPECT_EQ(thingify(join(reg0_, reg1_), mp), (mcable_list{{1,0,1}}));
        EXPECT_EQ(thingify(reg2_, mp), (mcable_list{{0, 0.75, 1}, {1,0,1}}));
    }

    // Test multi-level morphologies.
    //
    // Eight samples
    //
    //  sample ids:
    //            0
    //           1 3
    //          2   4
    //             5 6
    //                7
    //  tags:
    //            1
    //           3 2
    //          3   2
    //             4 3
    //                3
    {
        pvec parents = {mnpos, 0, 1, 0, 3, 4, 4, 6};
        svec samples = {
            {{  0,  0,  0,  1}, 1},
            {{ 10,  0,  0,  1}, 3},
            {{100,  0,  0,  3}, 3},
            {{  0, 10,  0,  1}, 2},
            {{  0,100,  0,  5}, 2},
            {{100,100,  0,  2}, 4},
            {{  0,200,  0,  1}, 3},
            {{  0,300,  0,  3}, 3},
        };
        sample_tree sm(samples, parents);

        // Without spherical root
        mprovider mp(morphology(sm, false));

        using reg::tagged;
        using reg::distal_interval;
        using reg::proximal_interval;
        using reg::radius_lt;
        using reg::radius_gt;
        using reg::branch;
        using reg::all;
        using reg::cable;

        auto soma = tagged(1);
        auto axon = tagged(2);
        auto dend = tagged(3);
        auto apic = tagged(4);
        auto b1  = branch(1);
        auto b3  = branch(3);
        auto b13 = join(b1, b3);

        cl empty_{};
        cl soma_ = empty_;

        // Whole branches:
        mcable b0_{0,0,1};
        mcable b1_{1,0,1};
        mcable b2_{2,0,1};
        mcable b3_{3,0,1};
        cl all_  = {b0_,b1_,b2_,b3_};

        mcable end1_{1,1,1};
        mcable root_{0,0,0};

        EXPECT_EQ(thingify(all(), mp), all_);
        EXPECT_EQ(thingify(axon, mp), (cl{b1_}));
        EXPECT_EQ(thingify(dend, mp), (cl{b0_,b3_}));
        EXPECT_EQ(thingify(apic, mp), (cl{b2_}));
        EXPECT_EQ(thingify(join(dend, apic), mp), (cl{b0_,b2_,b3_}));
        EXPECT_EQ(thingify(join(axon, join(dend, apic)), mp), all_);

        // Test that intersection correctly generates zero-length cables at
        // parent-child interfaces.
        EXPECT_EQ(thingify(intersect(apic, dend), mp), (cl{end1_}));
        EXPECT_EQ(thingify(intersect(apic, axon), mp), (cl{end1_}));
        EXPECT_EQ(thingify(intersect(axon, dend), mp), (cl{root_, end1_}));

        // Test distal and proximal interavls
        auto mid1_           = cable({1, 0.5, 0.5 });
        auto quar_interval1_ = cable({1, 0,   0.25});
        auto mid2_           = cable({2, 0.5, 0.5 });
        auto end2_           = cable({2, 1,   1   });
        auto mid3_           = cable({3, 0.5, 0.5 });
        auto quar_interval3_ = cable({3, 0.4, 0.65});

        // Distal from point and/or interval
        EXPECT_TRUE(cablelist_eq(thingify(distal_interval(mid1_, 1000), mp), (mcable_list{{1,0.5,1}, {2,0,1}, {3,0,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(distal_interval(mid1_, 150), mp), (mcable_list{{1,0.5,1}, {2,0,1}, {3,0,0.5}})));
        EXPECT_TRUE(cablelist_eq(thingify(distal_interval(quar_interval1_, 150), mp), (mcable_list{{1,0.25,1}, {2,0,0.75}, {3,0,0.375}})));
        EXPECT_TRUE(cablelist_eq(thingify(distal_interval(join(quar_interval1_, mid1_), 150), mp), (mcable_list{{1,0.25,1}, {2,0,1}, {3,0,0.5}})));

        // Proximal from point and/or interval
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(mid3_, 100), mp), (mcable_list{{3,0,0.5}})));
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(mid3_, 150), mp), (mcable_list{{1,0.5,1}, {3,0,0.5}})));
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(end2_, 150), mp), (mcable_list{{1,0.5,1}, {2,0,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(end2_, 500), mp), (mcable_list{{1,0,1}, {2,0,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(quar_interval3_, 100), mp), (mcable_list{{1,0.8,1}, {3,0,0.4}})));
        EXPECT_TRUE(cablelist_eq(thingify(proximal_interval(join(quar_interval3_, mid2_), 120), mp), (mcable_list{{1,0.3,1}, {2,0,0.5}, {3, 0, 0.4}})));

        // Test radius_le
        EXPECT_TRUE(cablelist_eq(thingify(radius_lt(all(), 2), mp), (mcable_list{{0,0,0.55}, {1,0,0.325}, {3,0.375,0.75}})));
        EXPECT_TRUE(cablelist_eq(thingify(radius_lt(all(), 3), mp), (mcable_list{{0,0,1}, {1,0,0.55}, {2,6.0/9.0,1}, {3,0.25,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(radius_gt(all(), 2), mp), (mcable_list{{0,0.55,1}, {1,0.325,1}, {2,0,1}, {3,0,0.375}, {3,0.75,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(radius_gt(all(), 3), mp), (mcable_list{{1,0.55,1}, {2,0,6.0/9.0}, {3,0,0.25}})));

        // Test some more interesting intersections and unions.

        //    123456789 123456789
        //   |---------|---------| lhs
        //   |  -----  |   ---   | rhs
        //   |  xxxxx  |   xxx   | rand
        //   |xxxxxxxxx|xxxxxxxxx| ror
        auto lhs  = b13;
        auto rhs  = join(cable({1,.2,.7}), cable({3,.3,.6}));
        auto rand = cl{         {1,.2,.7}, {3,.3,.6}};
        auto ror  = cl{         {1,.0,1.}, {3,.0,1.}};
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        // Assert communtativity
        std::swap(lhs, rhs);
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //    123456789 123456789
        //   |   ----- | ----    | lhs
        //   |  -----  |   ---   | rhs
        //   |   xxxx  |   xx    | rand
        //   |  xxxxxx | xxxxx   | ror
        lhs  = join(cable({1,.3,.8}), cable({3,.1,.5}));
        rhs  = join(cable({1,.2,.7}), cable({3,.3,.6}));
        rand = cl{         {1,.3,.7}, {3,.3,.5}};
        ror  = cl{         {1,.2,.8}, {3,.1,.6}};
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        // Assert communtativity
        std::swap(lhs, rhs);
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //    123456789 123456789
        //   | -- -    | --- --- | lhs
        //   |  -----  |   ---   | rhs
        //   |  x x    |   x x   | rand
        //   | xxxxxx  | xxxxxxx | ror
        lhs  = join(cable({1,.1,.3}), cable({1,.4,.5}), cable({3,.1,.4}), cable({3,.5,.9}));
        rhs  = join(cable({1,.2,.7}), cable({3,.3,.6}));
        rand = cl{         {1,.2,.3}, {1,.4,.5}, {3,.3,.4}, {3,.5,.6}};
        ror  = cl{         {1,.1,.7},            {3,.1,.9}};
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        // Assert communtativity
        std::swap(lhs, rhs);
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //       b1
        //    123456789
        //   |-----    | lhs
        //   |-----    | rhs
        //   |xxxxx    | rand
        //   |xxxxx    | ror
        lhs  = cable({1,0,.5});
        rhs  = cable({1,0,.5});
        rand = cl{{1,0,.5}};
        ror  = cl{{1,0,.5}};
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //       b3
        //    123456789
        //   |-----    | lhs
        //   |-----    | rhs
        //   |xxxxx    | rand
        //   |xxxxx    | ror
        lhs  = cable({3,0,.5});
        rhs  = cable({3,0,.5});
        rand = cl{{3,0,.5}};
        ror  = cl{{3,0,.5}};
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //       b0        b1
        //    123456789 123456789
        //   |xxxxx    |         | lhs
        //   |         |xxxxx    | rhs
        //   x         |         | rand
        //   |xxxxx    |xxxxx    | ror
        lhs  = cable({0,0,.5});
        rhs  = cable({1,0,.5});
        rand = cl{{0,0,0}};
        ror  = cl{{0,0,.5},{1,0,.5}};;
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        // Assert communtativity
        std::swap(lhs, rhs);
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        //       b2        b3
        //    123456789 123456789
        //   |xxxxx    |         | lhs
        //   |         |xxxxx    | rhs
        //   x         |         | rand
        //   |xxxxx    |xxxxx    | ror
        lhs  = cable({2,0,.5});
        rhs  = cable({3,0,.5});
        rand = cl{{1,1,1}};
        ror  = cl{{2,0,.5},{3,0,.5}};;
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);

        // Assert communtativity
        std::swap(lhs, rhs);
        EXPECT_EQ(thingify(intersect(lhs, rhs), mp), rand);
        EXPECT_EQ(thingify(join(lhs, rhs), mp), ror);
    }

    // Test multi-level morphologies.
    //
    // Eight samples
    //
    //  sample ids:
    //            0
    //            1
    //           2 3
    //          4   5
    {
        pvec parents = {mnpos, 0, 1, 1, 2, 3};
        svec samples = {
                {{  0, 10, 10,  1}, 1},
                {{  0, 30, 30,  1}, 2},
                {{  0, 60,-20,  1}, 2},
                {{  0, 90, 70,  1}, 2},
                {{  0, 80,-10,  1}, 2},
                {{  0,100,-40,  1}, 2}
        };
        sample_tree sm(samples, parents);

        // Without spherical root
        mprovider mp(morphology(sm, false));

        using reg::all;
        using reg::projection_lt;
        using reg::projection_gt;
        using reg::cable;

        // Test radius_le
        EXPECT_TRUE(cablelist_eq(thingify(projection_lt(20), mp), (mcable_list{{0,0,0.5}, {1,0.14456272544548071,1}, {2,0.6699940078464377,0.88999800261547934}})));
        EXPECT_TRUE(cablelist_eq(thingify(projection_gt(20), mp), (mcable_list{{0,0.5,1}, {1,0,0.14456272544548071}, {2,0,0.6699940078464377}, {2,0.88999800261547934,1}})));
        EXPECT_TRUE(cablelist_eq(thingify(join(projection_lt(20), projection_gt(20)), mp), thingify(all(), mp)));
    }
}
