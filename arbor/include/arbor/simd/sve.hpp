#pragma once

// SVE SIMD intrinsics implementation.

#ifdef __ARM_FEATURE_SVE

#include <arm_sve.h>
#include <cmath>
#include <cstdint>
#include <iostream>

#include "approx.hpp"

namespace arb {
namespace simd {
namespace detail {

struct sve_double8;
struct sve_int8;
struct sve_mask8;

template <>
struct simd_traits<sve_mask8> {
    static constexpr unsigned width = 8;
    using scalar_type = bool;
    using vector_type = svbool_t;
    using mask_impl = sve_mask8;
};

template <>
struct simd_traits<sve_double8> {
    static constexpr unsigned width = 8;
    using scalar_type = double;
    using vector_type = svfloat64_t;
    using mask_impl = sve_mask8;
};

template <>
struct simd_traits<sve_int8> {
    static constexpr unsigned width = 8;
    using scalar_type = int32_t;
    using vector_type = svint64_t;
    using mask_impl = sve_mask8;
};

struct sve_mask8 {
    static svbool_t broadcast(bool b) {
        return svdup_b64(-b);
    }

    static void copy_to(const svbool_t& k, bool* b) {
        svuint64_t a = svdup_u64_z(k, 1);
        svst1b_u64(svptrue_b64(), reinterpret_cast<uint8_t*>(b), a);
    }

    static void copy_to_masked(const svbool_t& k, bool* b, const svbool_t& mask) {
        svuint64_t a = svdup_u64_z(k, 1);
        svst1b_u64(mask, reinterpret_cast<uint8_t*>(b), a);
    }

    static svbool_t copy_from(const bool* p) {
        svuint64_t a = svld1ub_u64(svptrue_b64(), reinterpret_cast<const uint8_t*>(p));
        svuint64_t ones = svdup_n_u64(1);
        return svcmpeq_u64(svptrue_b64(), a, ones);
    }

    static svbool_t copy_from_masked(const bool* p, const svbool_t& mask) {
        svuint64_t a = svld1ub_u64(mask, reinterpret_cast<const uint8_t*>(p));
        svuint64_t ones = svdup_n_u64(1);
        return svcmpeq_u64(mask, a, ones);
    }

    static svbool_t logical_not(const svbool_t& k, const svbool_t& mask = svptrue_b64()) {
        return svnot_b_z(mask, k);
    }

    static svbool_t logical_and(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svand_b_z(mask, a, b);
    }

    static svbool_t logical_or(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svorr_b_z(mask, a, b);
    }

    // Arithmetic operations not necessarily appropriate for
    // packed bit mask, but implemented for completeness/testing,
    // with Z modulo 2 semantics:
    //     a + b   is equivalent to   a ^ b
    //     a * b                      a & b
    //     a / b                      a
    //     a - b                      a ^ b
    //     -a                         a
    //     max(a, b)                  a | b
    //     min(a, b)                  a & b

    static svbool_t negate(const svbool_t& a) {
        return a;
    }

    static svbool_t add(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return sveor_b_z(mask, a, b);
    }

    static svbool_t sub(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return sveor_b_z(mask, a, b);
    }

    static svbool_t mul(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svand_b_z(mask, a, b);
    }

    static svbool_t div(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return a;
    }

    static svbool_t fma(const svbool_t& a, const svbool_t& b, const svbool_t& c, const svbool_t& mask = svptrue_b64()) {
        return add(mul(a, b, mask), c, mask);
    }

    static svbool_t max(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svorr_b_z(mask, a, b);
    }

    static svbool_t min(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svand_b_z(mask, a, b);
    }

    // Comparison operators are also taken as operating on Z modulo 2,
    // with 1 > 0:
    //
    //     a > b    is equivalent to  a & ~b
    //     a >= b                     a | ~b,  ~(~a & b)
    //     a < b                      ~a & b
    //     a <= b                     ~a | b,  ~(a & ~b)
    //     a == b                     ~(a ^ b)
    //     a != b                     a ^ b

    static svbool_t cmp_eq(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svnot_b_z(mask, sveor_b_z(mask, a, b));
    }

    static svbool_t cmp_neq(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return sveor_b_z(mask, a, b);
    }

    static svbool_t cmp_lt(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return svbic_b_z(mask, b, a);
    }

    static svbool_t cmp_gt(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return cmp_lt(b, a);
    }

    static svbool_t cmp_geq(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return logical_not(cmp_lt(a, b));
    }

    static svbool_t cmp_leq(const svbool_t& a, const svbool_t& b, const svbool_t& mask = svptrue_b64()) {
        return logical_not(cmp_gt(a, b));
    }

    static svbool_t ifelse(const svbool_t& m, const svbool_t& u, const svbool_t& v) {
        return svsel_b(m, u, v);
    }

    static svbool_t mask_broadcast(bool b) {
        return broadcast(b);
    }

    static void mask_copy_to(const svbool_t& m, bool* y) {
        copy_to(m, y);
    }

    static svbool_t mask_copy_from(const bool* y) {
        return copy_from(y);
    }

    static svbool_t true_mask(unsigned width) {
        //return svwhilelt_b64_u64(0, (uint64_t)width);
        unsigned len = svcntd();
        if (len == width) {
            return svptrue_b64();
        }
        bool n[len];
        for (unsigned i = 0; i< len; ++i){
            n[i] = i < width;
        }
        return copy_from(n);
    }
};

struct sve_int8 {
    // Use default implementations for:
    //     element, set_element.

    using int32 = std::int32_t;

    static svint64_t broadcast(int32 v) {
        return svreinterpret_s64_s32(svdup_n_s32(v));
    }

    static void copy_to(const svint64_t& v, int32* p) {
        svst1w_s64(svptrue_b64(), p, v);
    }

    static void copy_to_masked(const svint64_t& v, int32* p, const svbool_t& mask) {
        svst1w_s64(mask, p, v);
    }

    static svint64_t copy_from(const int32* p) {
        return svld1sw_s64(svptrue_b64(), p);
    }

    static svint64_t copy_from_masked(const int32* p, const svbool_t& mask) {
        return svld1sw_s64(mask, p);
    }

    static svint64_t copy_from_masked(const svint64_t& v, const int32* p, const svbool_t& mask) {
        return svsel_s64(mask, svld1sw_s64(mask, p), v);
    }

    static int element0(const svint64_t& a) {
        return svlasta_s64(svptrue_b64(), a);
    }

    static svint64_t negate(const svint64_t& a, const svbool_t& mask = svptrue_b64()) {
        return svneg_s64_z(mask, a);
    }

    static svint64_t add(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svadd_s64_z(mask, a, b);
    }

    static svint64_t sub(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svsub_s64_m(mask, a, b);
    }

    static svint64_t mul(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        //May overflow
        return svmul_s64_z(mask, a, b);
    }

    static svint64_t div(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svdiv_s64_z(mask, a, b);
    }

    static svint64_t fma(const svint64_t& a, const svint64_t& b, const svint64_t& c, const svbool_t& mask = svptrue_b64()) {
        return add(mul(a, b, mask), c, mask);
    }

    static svbool_t cmp_eq(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpeq_s64(mask, a, b);
    }

    static svbool_t cmp_neq(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpne_s64(mask, a, b);
    }

    static svbool_t cmp_gt(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpgt_s64(mask, a, b);
    }

    static svbool_t cmp_geq(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpge_s64(mask, a, b);
    }

    static svbool_t cmp_lt(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmplt_s64(mask, a, b);
    }

    static svbool_t cmp_leq(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmple_s64(mask, a, b);
    }

    static svint64_t ifelse(const svbool_t& m, const svint64_t& u, const svint64_t& v) {
        return svsel_s64(m, u, v);
    }

    static svint64_t max(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svmax_s64_x(mask, a, b);
    }

    static svint64_t min(const svint64_t& a, const svint64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svmin_s64_x(mask, a, b);
    }

    static svint64_t abs(const svint64_t& a, const svbool_t& mask = svptrue_b64()) {
        return svabs_s64_z(mask, a);
    }

    static int reduce_add(const svint64_t& a, const svbool_t& mask = svptrue_b64()) {
        return svaddv_s64(mask, a);
    }

    static  svint64_t pow(const svint64_t& x, const svint64_t& y, const svbool_t& mask = svptrue_b64()) {
         auto len = svlen_s64(x);
         int32 a[len], b[len], r[len];
         copy_to_masked(x, a, mask);
         copy_to_masked(y, b, mask);

        for (unsigned i = 0; i<len; ++i) {
            r[i] = std::pow(a[i], b[i]);
        }
        return copy_from_masked(r, mask);
    }

    static svint64_t gather(tag<sve_int8>, const int32* p, const svint64_t& index, const svbool_t& mask = svptrue_b64()) {
        return svld1sw_gather_s64index_s64(mask, p, index);
    }

    static svint64_t gather(tag<sve_int8>, svint64_t a, const int32* p, const svint64_t& index, const svbool_t& mask) {
        return svsel_s64(mask, svld1sw_gather_s64index_s64(mask, p, index), a);
    }

    static void scatter(tag<sve_int8>, const svint64_t& s, int32* p, const svint64_t& index, const svbool_t& mask = svptrue_b64()) {
        svst1w_scatter_s64index_s64(mask, p, index, s);
    }

    static unsigned simd_width(const svint64_t& m) {
        return svlen_s64(m);
    }
};

struct sve_double8 {
    // Use default implementations for:
    //     element, set_element.

    static svfloat64_t broadcast(double v) {
        return svdup_n_f64(v);
    }

    static void copy_to(const svfloat64_t& v, double* p) {
        svst1_f64(svptrue_b64(), p, v);
    }

    static void copy_to_masked(const svfloat64_t& v, double* p, const svbool_t& mask) {
        svst1_f64(mask, p, v);
    }

    static svfloat64_t copy_from(const double* p) {
        return svld1_f64(svptrue_b64(), p);
    }

    static svfloat64_t copy_from_masked(const double* p, const svbool_t& mask) {
        return svld1_f64(mask, p);
    }

    static svfloat64_t copy_from_masked(const svfloat64_t& v, const double* p, const svbool_t& mask) {
        return svsel_f64(mask, svld1_f64(mask, p), v);
    }

    static double element0(const svfloat64_t& a) {
        return svlasta_f64(svptrue_b64(), a);
    }

    static svfloat64_t negate(const svfloat64_t& a, const svbool_t& mask = svptrue_b64()) {
        return svneg_f64_z(mask, a);
    }

    static svfloat64_t add(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svadd_f64_z(mask, a, b);
    }

    static svfloat64_t sub(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svsub_f64_m(mask, a, b);
    }

    static svfloat64_t mul(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svmul_f64_z(mask, a, b);
    }

    static svfloat64_t div(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svdiv_f64_z(mask, a, b);
    }

    static svfloat64_t fma(const svfloat64_t& a, const svfloat64_t& b, const svfloat64_t& c, const svbool_t& mask = svptrue_b64()) {
        return svmad_f64_z(mask, a, b, c);
    }

    static svbool_t cmp_eq(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpeq_f64(mask, a, b);
    }

    static svbool_t cmp_neq(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpne_f64(mask, a, b);
    }

    static svbool_t cmp_gt(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpgt_f64(mask, a, b);
    }

    static svbool_t cmp_geq(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmpge_f64(mask, a, b);
    }

    static svbool_t cmp_lt(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmplt_f64(mask, a, b);
    }

    static svbool_t cmp_leq(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svcmple_f64(mask, a, b);
    }

    static svfloat64_t ifelse(const svbool_t& m, const svfloat64_t& u, const svfloat64_t& v) {
        return svsel_f64(m, u, v);
    }

    static svfloat64_t max(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svmax_f64_x(mask, a, b);
    }

    static svfloat64_t min(const svfloat64_t& a, const svfloat64_t& b, const svbool_t& mask = svptrue_b64()) {
        return svmin_f64_x(mask, a, b);
    }

    static svfloat64_t abs(const svfloat64_t& x, const svbool_t& mask = svptrue_b64()) {
        return svabs_f64_x(mask, x);
    }

    static double reduce_add(const svfloat64_t& a, const svbool_t& mask = svptrue_b64()) {
        return svaddv_f64(mask, a);
    }

    static svfloat64_t gather(tag<sve_int8>, const double* p, const svint64_t& index, const svbool_t& mask = svptrue_b64()) {
        return svld1_gather_s64index_f64(mask, p, index);
    }

    static svfloat64_t gather(tag<sve_int8>, svfloat64_t a, const double* p, const svint64_t& index, const svbool_t& mask) {
        return svsel_f64(mask, svld1_gather_s64index_f64(mask, p, index), a);
    }

    static void scatter(tag<sve_int8>, const svfloat64_t& s, double* p, const svint64_t& index, const svbool_t& mask = svptrue_b64()) {
        svst1_scatter_s64index_f64(mask, p, index, s);
    }

    // Refer to avx/avx2 code for details of the exponential and log
    // implementations.

    static  svfloat64_t exp(const svfloat64_t& x) {
        // Masks for exceptional cases.

        auto is_large = cmp_gt(x, broadcast(exp_maxarg));
        auto is_small = cmp_lt(x, broadcast(exp_minarg));

        // Compute n and g.

        auto n = svrintz_f64_z(svptrue_b64(), add(mul(broadcast(ln2inv), x), broadcast(0.5)));

        auto g = fma(n, broadcast(-ln2C1), x);
        g = fma(n, broadcast(-ln2C2), g);

        auto gg = mul(g, g);

        // Compute the g*P(g^2) and Q(g^2).
        auto odd = mul(g, horner(gg, P0exp, P1exp, P2exp));
        auto even = horner(gg, Q0exp, Q1exp, Q2exp, Q3exp);

        // Compute R(g)/R(-g) = 1 + 2*g*P(g^2) / (Q(g^2)-g*P(g^2))

        auto expg = fma(broadcast(2), div(odd, sub(even, odd)), broadcast(1));

        // Scale by 2^n, propogating NANs.

        auto result = svscale_f64_z(svptrue_b64(), expg, svcvt_s64_f64_z(svptrue_b64(), n));

        return
            ifelse(is_large, broadcast(HUGE_VAL),
            ifelse(is_small, broadcast(0),
                   result));
    }

    static  svfloat64_t expm1(const svfloat64_t& x) {
        auto is_large = cmp_gt(x, broadcast(exp_maxarg));
        auto is_small = cmp_lt(x, broadcast(expm1_minarg));

        auto half = broadcast(0.5);
        auto one = broadcast(1.);

        auto nnz = cmp_gt(abs(x), half);
        auto n = svrinta_f64_z(nnz, mul(broadcast(ln2inv), x));

        auto g = fma(n, broadcast(-ln2C1), x);
        g = fma(n, broadcast(-ln2C2), g);

        auto gg = mul(g, g);

        auto odd = mul(g, horner(gg, P0exp, P1exp, P2exp));
        auto even = horner(gg, Q0exp, Q1exp, Q2exp, Q3exp);

        // Compute R(g)/R(-g) -1 = 2*g*P(g^2) / (Q(g^2)-g*P(g^2))

        auto expgm1 = div(mul(broadcast(2), odd), sub(even, odd));

        // For small x (n zero), bypass scaling step to avoid underflow.
        // Otherwise, compute result 2^n * expgm1 + (2^n-1) by:
        //     result = 2 * ( 2^(n-1)*expgm1 + (2^(n-1)+0.5) )
        // to avoid overflow when n=1024.

        auto nm1 = svcvt_s64_f64_z(svptrue_b64(), sub(n, one));

        auto result =
            svscale_f64_z(svptrue_b64(),
                add(sub(svscale_f64_z(svptrue_b64(),one, nm1), half),
                    svscale_f64_z(svptrue_b64(),expgm1, nm1)),
                svcvt_s64_f64_z(svptrue_b64(), one));

        return
            ifelse(is_large, broadcast(HUGE_VAL),
            ifelse(is_small, broadcast(-1),
            ifelse(nnz, result, expgm1)));
    }

    static  svfloat64_t exprelr(const svfloat64_t& x) {
        auto ones = broadcast(1);
        return ifelse(cmp_eq(ones, add(ones, x)), ones, div(x, expm1(x)));
    }

    static svfloat64_t log(const svfloat64_t& x) {
        // Masks for exceptional cases.

        auto is_large = cmp_geq(x, broadcast(HUGE_VAL));
        auto is_small = cmp_lt(x, broadcast(log_minarg));
        auto is_domainerr = cmp_lt(x, broadcast(0));

        auto is_nan = svnot_b_z(svptrue_b64(), cmp_eq(x, x));
        is_domainerr = svorr_b_z(svptrue_b64(), is_nan, is_domainerr);

        svfloat64_t g = svcvt_f64_s32_z(svptrue_b64(), logb_normal(x));
        svfloat64_t u = fraction_normal(x);

        svfloat64_t one = broadcast(1.);
        svfloat64_t half = broadcast(0.5);
        auto gtsqrt2 = cmp_geq(u, broadcast(sqrt2));
        g = ifelse(gtsqrt2, add(g, one), g);
        u = ifelse(gtsqrt2, mul(u, half), u);

        auto z = sub(u, one);
        auto pz = horner(z, P0log, P1log, P2log, P3log, P4log, P5log);
        auto qz = horner1(z, Q0log, Q1log, Q2log, Q3log, Q4log);

        auto z2 = mul(z, z);
        auto z3 = mul(z2, z);

        auto r = div(mul(z3, pz), qz);
        r = add(r, mul(g,  broadcast(ln2C4)));
        r = sub(r, mul(z2, half));
        r = add(r, z);
        r = add(r, mul(g,  broadcast(ln2C3)));

        // r is alrady NaN if x is NaN or negative, otherwise
        // return  +inf if x is +inf, or -inf if zero or (positive) denormal.

        return ifelse(is_domainerr, broadcast(NAN),
                      ifelse(is_large, broadcast(HUGE_VAL),
                             ifelse(is_small, broadcast(-HUGE_VAL), r)));
    }

    static svfloat64_t pow(const svfloat64_t& x, const svfloat64_t& y) {
         auto len = svlen_f64(x);
         double a[len], b[len], r[len];
         copy_to(x, a);
         copy_to(y, b);

        for (unsigned i = 0; i<len; ++i) {
            r[i] = std::pow(a[i], b[i]);
        }
        return copy_from(r);
    }

    static unsigned simd_width(const svfloat64_t& m) {
        return svlen_f64(m);
    }

protected:
    // Compute n and f such that x = 2^n·f, with |f| ∈ [1,2), given x is finite and normal.
    static svint32_t logb_normal(const svfloat64_t& x) {
        svuint32_t xw    = svtrn2_u32(svreinterpret_u32_f64(x), svreinterpret_u32_f64(x));
        svuint64_t lmask = svdup_n_u64(0x00000000ffffffff);
        svuint64_t xt = svand_u64_z(svptrue_b64(), svreinterpret_u64_u32(xw), lmask);
        svuint32_t xhi = svreinterpret_u32_u64(xt);
        auto emask = svdup_n_u32(0x7ff00000);
        auto ebiased = svlsr_n_u32_z(svptrue_b64(), svand_u32_z(svptrue_b64(), xhi, emask), 20);

        return svsub_s32_z(svptrue_b64(), svreinterpret_s32_u32(ebiased), svdup_n_s32(1023));
    }

    static svfloat64_t fraction_normal(const svfloat64_t& x) {
        svuint64_t emask = svdup_n_u64(-0x7ff0000000000001);
        svuint64_t bias =  svdup_n_u64(0x3ff0000000000000);
        return svreinterpret_f64_u64(
            svorr_u64_z(svptrue_b64(), bias, svand_u64_z(svptrue_b64(), emask, svreinterpret_u64_f64(x))));
    }

    static inline svfloat64_t horner1(svfloat64_t x, double a0) {
        return add(x, broadcast(a0));
    }

    static inline svfloat64_t horner(svfloat64_t x, double a0) {
        return broadcast(a0);
    }

    template <typename... T>
    static svfloat64_t horner(svfloat64_t x, double a0, T... tail) {
        return fma(x, horner(x, tail...), broadcast(a0));
    }

    template <typename... T>
    static svfloat64_t horner1(svfloat64_t x, double a0, T... tail) {
        return fma(x, horner1(x, tail...), broadcast(a0));
    }

    static svfloat64_t fms(const svfloat64_t& a, const svfloat64_t& b, const svfloat64_t& c) {
        return svnmsb_f64_z(svptrue_b64(), a, b, c);
    }
};

}  // namespace detail

namespace simd_abi {
template <typename T, unsigned N> struct sve;
template <> struct sve<double, 0> {using type = detail::sve_double8;};
template <> struct sve<int, 0> {using type = detail::sve_int8;};

template <typename T> struct reg_type;
template <> struct reg_type<int> { using type = svint64_t;};
template <> struct reg_type<double> { using type = svfloat64_t;};

template <typename T> struct mask_type;
template <> struct mask_type<int> { using type = svbool_t;};
template <> struct mask_type<double> { using type = svbool_t;};

template<typename Type> struct type_to_impl;
template<> struct type_to_impl<svint64_t> { using type = detail::sve_int8;};
template<> struct type_to_impl<svfloat64_t> { using type = detail::sve_double8;};
template<> struct type_to_impl<svbool_t> { using type = detail::sve_mask8;};
};  // namespace simd_abi

using namespace arb::simd::simd_abi;
using arb::simd::detail::simd_traits;

template <typename Value, unsigned N, template <class, unsigned> class Abi>
struct simd_wrap;

template <typename Value, template <class, unsigned> class Abi>
struct simd_wrap<Value, (unsigned)0, Abi> { using type = typename simd_abi::reg_type<Value>::type; };

template <typename Value, unsigned N>
struct simd_mask_wrap;

template <typename Value>
struct simd_mask_wrap<Value, 0> { using type = typename simd_abi::mask_type<Value>::type; };

template <typename T>
T neg(const T& a) {
    return type_to_impl<T>::type::negate(a);
}

template <typename T>
T add(const T& a, const T& b) {
    return type_to_impl<T>::type::add(a, b);
}

template <typename T>
T add(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return add(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T add(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return add(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T sub(const T& a, const T& b) {
    return type_to_impl<T>::type::sub(a, b);
}

template <typename T>
T sub(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return sub(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T sub(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return sub(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T mul(const T& a, const T& b) {
    return type_to_impl<T>::type::mul(a, b);
}

template <typename T>
T mul(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return mul(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T mul(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return mul(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T div(const T& a, const T& b) {
    return type_to_impl<T>::type::div(a, b);
}

template <typename T>
T div(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return div(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T div(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return div(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T fma(const T& a, T b, T c) {
    return type_to_impl<T>::type::fma(a, b, c);
}

template <typename T>
T fma(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return fma(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T fma(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return fma(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_eq(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_eq(a, b);
}

template <typename T>
T cmp_eq(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_eq(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_eq(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_eq(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_neq(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_neq(a, b);
}

template <typename T>
T cmp_neq(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_neq(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_neq(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_neq(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_leq(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_leq(a, b);
}

template <typename T>
T cmp_leq(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_leq(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_leq(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_leq(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_lt(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_lt(a, b);
}

template <typename T>
T cmp_lt(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_lt(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_lt(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_lt(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_geq(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_geq(a, b);
}

template <typename T>
T cmp_geq(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_geq(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_geq(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_geq(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto cmp_gt(const T& a, const T& b) {
    return type_to_impl<T>::type::cmp_gt(a, b);
}

template <typename T>
T cmp_gt(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return cmp_gt(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T cmp_gt(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return cmp_gt(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
auto sum(const T& a) {
    return type_to_impl<T>::type::reduce_add(a);
}

template <typename T>
T abs(const T& s) {
    return type_to_impl<T>::type::abs(s);
}

template <typename T>
T exp(const T& s) {
    return type_to_impl<T>::type::exp(s);
}

template <typename T>
T log(const T& s) {
    return type_to_impl<T>::type::log(s);
}

template <typename T>
T expm1(const T& s) {
    return type_to_impl<T>::type::expm1(s);
}

template <typename T>
T min(const T& a, const T& b) {
    return type_to_impl<T>::type::min(a, b);
}

template <typename T>
T min(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return min(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T min(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return min(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T max(const T& a, const T& b) {
    return type_to_impl<T>::type::max(a, b);
}

template <typename T>
T max(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return max(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T max(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return max(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T>
T exprelr(const T& a) {
    return type_to_impl<T>::type::exprelr(a);
}

template <typename T>
T pow(const T& a, const T& b) {
    return type_to_impl<T>::type::pow(a, b);
}

template <typename T>
T pow(const T& a, const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& b) {
    return pow(a, type_to_impl<T>::type::broadcast(b));
}

template <typename T>
T pow(const typename simd_traits<typename type_to_impl<T>::type>::scalar_type& a, const T& b) {
    return pow(type_to_impl<T>::type::broadcast(a), b);
}

template <typename T, typename V>
static void indirect_copy_to(const T& s, V* p, unsigned width) {
    using Impl     = typename type_to_impl<T>::type;
    using ImplMask = typename detail::simd_traits<Impl>::mask_impl;
    Impl::copy_to_masked(s, p, ImplMask::true_mask(width));
}

template <typename T, typename M, typename V>
static void indirect_copy_to(const T& data, const M& mask, V* p, unsigned width) {
    using Impl     = typename type_to_impl<T>::type;
    using ImplMask = typename type_to_impl<M>::type;

    Impl::copy_to_masked(data, p, ImplMask::logical_and(mask, ImplMask::true_mask(width)));
}

template <typename T, typename I, typename V>
static void indirect_indexed_copy_to(const T& s, V* p, const I& index, unsigned width) {
    using Impl      = typename type_to_impl<T>::type;
    using ImplIndex = typename type_to_impl<I>::type;
    using ImplMask  = typename detail::simd_traits<Impl>::mask_impl;

    Impl::scatter(detail::tag<ImplIndex>{}, s, p, index, ImplMask::true_mask(width));
}

template <typename T, typename I, typename M, typename V>
static void indirect_indexed_copy_to(const T& data, const M& mask, V* p, const I& index, unsigned width) {
    using Impl      = typename type_to_impl<T>::type;
    using ImplIndex = typename type_to_impl<I>::type;
    using ImplMask = typename type_to_impl<M>::type;

    Impl::scatter(detail::tag<ImplIndex>{}, data, p, index, ImplMask::logical_and(mask, ImplMask::true_mask(width)));
}

template <typename T, typename M>
static void where_copy_to(const M& mask, T& f, const T& t) {
    f = type_to_impl<T>::type::ifelse(mask, t, f);
}

template <typename T, typename M, typename V>
static void where_copy_to(const M& mask, T& f, const V* p, unsigned width) {
    using Impl     = typename type_to_impl<T>::type;
    using ImplMask = typename type_to_impl<M>::type;

    auto m = ImplMask::logical_and(mask, ImplMask::true_mask(width));
    f = Impl::ifelse(mask, Impl::copy_from_masked(p, m), f);
}

template <typename T, typename I, typename M, typename V>
static void where_copy_to(const M& mask, T& f, const V* p, const I& index, unsigned width) {
    using Impl      = typename type_to_impl<T>::type;
    using IndexImpl = typename type_to_impl<I>::type;
    using ImplMask  = typename type_to_impl<M>::type;
 
    auto m = ImplMask::logical_and(mask, ImplMask::true_mask(width));
    T temp = Impl::gather(detail::tag<IndexImpl>{}, p, index, m);
    f = Impl::ifelse(mask, temp, f);
}

template <typename I, typename T, typename V>
void compound_indexed_add(
    const T& s,
    V* p,
    const I& index,
    unsigned width,
    index_constraint constraint)
{
    using Impl      = typename type_to_impl<T>::type;
    using ImplIndex = typename type_to_impl<I>::type;
    using ImplMask  = typename detail::simd_traits<Impl>::mask_impl;

    auto mask = ImplMask::true_mask(width);
    switch (constraint) {
        case index_constraint::none:
        {
            typename detail::simd_traits<ImplIndex>::scalar_type o[width];
            ImplIndex::copy_to_masked(index, o, mask);

            V a[width];
            Impl::copy_to_masked(s, a, mask);

            V temp = 0;
            for (unsigned i = 0; i<width-1; ++i) {
                temp += a[i];
                if (o[i] != o[i+1]) {
                    p[o[i]] += temp;
                    temp = 0;
                }
            }
            temp += a[width-1];
            p[o[width-1]] += temp;
        }
            break;
        case index_constraint::independent:
        {
            auto v = Impl::add(Impl::gather(detail::tag<ImplIndex>{}, p, index, mask), s, mask);
            Impl::scatter(detail::tag<ImplIndex>{}, v, p, index, mask);
        }
            break;
        case index_constraint::contiguous:
        {
            p += ImplIndex::element0(index);
            auto v = Impl::add(Impl::copy_from_masked(p, mask), s, mask);
            Impl::copy_to_masked(v, p, mask);
        }
            break;
        case index_constraint::constant:
            p += ImplIndex::element0(index);
            *p += Impl::reduce_add(s, mask);
            break;
    }
}

namespace detail {

template <typename I, typename V>
class indirect_indexed_expression;

template <typename V>
class indirect_expression;

template <typename T, typename M>
class where_expression;

template <typename T, typename M>
class const_where_expression;

template <typename To>
struct simd_cast_impl {
    template <typename V>
    static To cast(const V& a) {
        return type_to_impl<To>::type::broadcast(a);
    }

    template <typename V>
    static To cast(const indirect_expression<V>& a) {
        using Impl     = typename type_to_impl<To>::type;
        using ImplMask = typename detail::simd_traits<Impl>::mask_impl;

        return Impl::copy_from_masked(a.p, ImplMask::true_mask(a.width));
    }

    template <typename I, typename V>
    static To cast(const indirect_indexed_expression<I,V>& a) {
        using Impl      = typename type_to_impl<To>::type;
        using IndexImpl = typename type_to_impl<I>::type;
        using ImplMask  = typename detail::simd_traits<Impl>::mask_impl;

        To r;
        auto mask = ImplMask::true_mask(a.width);
        switch (a.constraint) {
            case index_constraint::none:
                r = Impl::gather(tag<IndexImpl>{}, a.p, a.index, mask);
                break;
            case index_constraint::independent:
                r = Impl::gather(tag<IndexImpl>{}, a.p, a.index, mask);
                break;
            case index_constraint::contiguous:
            {
                const auto* p = IndexImpl::element0(a.index) + a.p;
                r = Impl::copy_from_masked(p, mask);
            }
                break;
            case index_constraint::constant:
            {
                const auto *p = IndexImpl::element0(a.index) + a.p;
                auto l = (*p);
                r = Impl::broadcast(l);
            }
                break;
        }
        return r;
    }

    template <typename T, typename V>
    static To cast(const const_where_expression<T,V>& a) {
        auto r = type_to_impl<To>::type::broadcast(0);
        r = type_to_impl<To>::type::ifelse(a.mask_, a.data_, r);
        return r;
    }

    template <typename T, typename V>
    static To cast(const where_expression<T,V>& a) {
        auto r = type_to_impl<To>::type::broadcast(0);
        r = type_to_impl<To>::type::ifelse(a.mask_, a.data_, r);
        return r;
    }
};

}//namespace detail

}  // namespace simd
}  // namespace arb

#endif  // def __ARM_FEATURE_SVE
