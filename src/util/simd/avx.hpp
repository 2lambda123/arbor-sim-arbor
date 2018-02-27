#pragma once

// AVX/AVX2 SIMD intrinsics implementation.

#ifdef __AVX__

#include <cmath>
#include <cstdint>
#include <immintrin.h>

#include <util/simd/approx.hpp>

namespace arb {
namespace simd_detail {

struct avx_int4;
struct avx_double4;

template <>
struct simd_traits<avx_int4> {
    static constexpr unsigned width = 4;
    using scalar_type = std::int32_t;
    using vector_type = __m128i;
    using mask_impl = avx_int4;
};

template <>
struct simd_traits<avx_double4> {
    static constexpr unsigned width = 4;
    using scalar_type = double;
    using vector_type = __m256d;
    using mask_impl = avx_double4;
};

struct avx_int4: implbase<avx_int4> {
    // Use default implementations for:
    //     element, set_element, fma, div.

    using int32 = std::int32_t;

    static __m128i broadcast(double v) {
        return _mm_set1_epi32(v);
    }

    static void copy_to(const __m128i& v, int32* p) {
        _mm_storeu_si128((__m128i*)p, v);
    }

    static __m128i copy_from(const int32* p) {
        return _mm_loadu_si128((const __m128i*)p);
    }

    static __m128i negate(const __m128i& a) {
        __m128i zero = _mm_setzero_si128();
        return _mm_sub_epi32(zero, a);
    }

    static __m128i add(const __m128i& a, const __m128i& b) {
        return _mm_add_epi32(a, b);
    }

    static __m128i sub(const __m128i& a, const __m128i& b) {
        return _mm_sub_epi32(a, b);
    }

    static __m128i mul(const __m128i& a, const __m128i& b) {
        return _mm_mullo_epi32(a, b);
    }

    static __m128i fma(const __m128i& a, const __m128i& b, const __m128i& c) {
        return _mm_add_epi32(_mm_mullo_epi32(a, b), c);
    }

    static __m128i logical_not(const __m128i& a) {
        __m128i ones = {};
        return _mm_xor_si128(a, _mm_cmpeq_epi32(ones, ones));
    }

    static __m128i logical_and(const __m128i& a, const __m128i& b) {
        return _mm_and_si128(a, b);
    }

    static __m128i logical_or(const __m128i& a, const __m128i& b) {
        return _mm_or_si128(a, b);
    }

    static __m128i cmp_eq(const __m128i& a, const __m128i& b) {
        return _mm_cmpeq_epi32(a, b);
    }

    static __m128i cmp_neq(const __m128i& a, const __m128i& b) {
        return logical_not(cmp_eq(a, b));
    }

    static __m128i cmp_gt(const __m128i& a, const __m128i& b) {
        return _mm_cmpgt_epi32(a, b);
    }

    static __m128i cmp_geq(const __m128i& a, const __m128i& b) {
        return logical_not(cmp_gt(b, a));
    }

    static __m128i cmp_lt(const __m128i& a, const __m128i& b) {
        return cmp_gt(b, a);
    }

    static __m128i cmp_leq(const __m128i& a, const __m128i& b) {
        return logical_not(cmp_gt(a, b));
    }

    static __m128i ifelse(const __m128i& m, const __m128i& u, const __m128i& v) {
        return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(u), _mm_castsi128_ps(v), _mm_castsi128_ps(m)));
    }

    static __m128i mask_broadcast(bool b) {
        return _mm_set1_epi32(-(int32)b);
    }

    static bool mask_element(const __m128i& u, int i) {
        return static_cast<bool>(element(u, i));
    }

    static void mask_set_element(__m128i& u, int i, bool b) {
        set_element(u, i, -(int32)b);
    }

    static void mask_copy_to(const __m128i& m, bool* y) {
        __m128i s = _mm_setr_epi32(0x0c080400ul,0,0,0);
        __m128i p = _mm_shuffle_epi8(m, s);
        std::memcpy(y, &p, 4);
    }

    static __m128i mask_copy_from(const bool* w) {
        __m128i r;
        std::memcpy(&r, w, 4);

        __m128i s = _mm_setr_epi32(0x80808000ul, 0x80808001ul, 0x80808002ul, 0x80808003ul);
        return _mm_shuffle_epi8(r, s);
    }

    static __m128i max(const __m128i& a, const __m128i& b) {
        return _mm_max_epi32(a, b);
    }

    static __m128i min(const __m128i& a, const __m128i& b) {
        return _mm_min_epi32(a, b);
    }

};

struct avx_double4: implbase<avx_double4> {
    // Use default implementations for:
    //     element, set_element, fma.

    using int64 = std::int64_t;

    // CMPPD predicates:
    static constexpr int cmp_eq_oq =    0;
    static constexpr int cmp_unord_q =  3;
    static constexpr int cmp_neq_uq =   4;
    static constexpr int cmp_true_uq = 15;
    static constexpr int cmp_lt_oq =   17;
    static constexpr int cmp_le_oq =   18;
    static constexpr int cmp_ge_oq =   29;
    static constexpr int cmp_gt_oq =   30;

    static __m256d broadcast(double v) {
        return _mm256_set1_pd(v);
    }

    static void copy_to(const __m256d& v, double* p) {
        _mm256_storeu_pd(p, v);
    }

    static __m256d copy_from(const double* p) {
        return _mm256_loadu_pd(p);
    }

    static __m256d negate(const __m256d& a) {
        __m256d zero = _mm256_setzero_pd();
        return _mm256_sub_pd(zero, a);
    }

    static __m256d add(const __m256d& a, const __m256d& b) {
        return _mm256_add_pd(a, b);
    }

    static __m256d sub(const __m256d& a, const __m256d& b) {
        return _mm256_sub_pd(a, b);
    }

    static __m256d mul(const __m256d& a, const __m256d& b) {
        return _mm256_mul_pd(a, b);
    }

    static __m256d div(const __m256d& a, const __m256d& b) {
        return _mm256_div_pd(a, b);
    }

    static __m256d logical_not(const __m256d& a) {
        __m256d ones = {};
        return _mm256_xor_pd(a, _mm256_cmp_pd(ones, ones, 15));
    }

    static __m256d logical_and(const __m256d& a, const __m256d& b) {
        return _mm256_and_pd(a, b);
    }

    static __m256d logical_or(const __m256d& a, const __m256d& b) {
        return _mm256_or_pd(a, b);
    }

    static __m256d cmp_eq(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_eq_oq);
    }

    static __m256d cmp_neq(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_neq_uq);
    }

    static __m256d cmp_gt(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_gt_oq);
    }

    static __m256d cmp_geq(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_ge_oq);
    }

    static __m256d cmp_lt(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_lt_oq);
    }

    static __m256d cmp_leq(const __m256d& a, const __m256d& b) {
        return _mm256_cmp_pd(a, b, cmp_le_oq);
    }

    static __m256d ifelse(const __m256d& m, const __m256d& u, const __m256d& v) {
        return _mm256_blendv_pd(v, u, m);
    }

    static __m256d mask_broadcast(bool b) {
        return _mm256_castsi256_pd(_mm256_set1_epi64x(-(int64)b));
    }

    static bool mask_element(const __m256d& u, int i) {
        return static_cast<bool>(element(u, i));
    }

    static void mask_set_element(__m256d& u, int i, bool b) {
        char data[256];
        _mm256_storeu_pd((double*)data, u);
        ((int64*)data)[i] = -(int64)b;
        u = _mm256_loadu_pd((double*)data);
    }

    static void mask_copy_to(const __m256d& m, bool* y) {
        __m128i zero = _mm_setzero_si128();

        // Split into upper and lower 128-bits (two mask values
        // in each), translate 0xffffffffffffffff to 0x0000000000000001.

        __m128i ml = _mm_castpd_si128(_mm256_castpd256_pd128(m));
        ml = _mm_sub_epi64(zero, ml);

        __m128i mu = _mm_castpd_si128(_mm256_castpd256_pd128(_mm256_permute2f128_pd(m, m, 1)));
        mu = _mm_sub_epi64(zero, mu);

        // Move bytes with bool value to bytes 0 and 1 in lower half,
        // bytes 2 and 3 in upper half, and merge with bitwise-or.

        __m128i sl = _mm_setr_epi32(0x80800800ul,0,0,0);
        ml = _mm_shuffle_epi8(ml, sl);

        __m128i su = _mm_setr_epi32(0x08008080ul,0,0,0);
        mu = _mm_shuffle_epi8(mu, su);

        __m128i r = _mm_or_si128(mu, ml);
        std::memcpy(y, &r, 4);
    }

    static __m256d mask_copy_from(const bool* w) {
        __m128i zero = _mm_setzero_si128();

        __m128i r;
        std::memcpy(&r, w, 4);

        // Move bytes:
        //   rl: byte 0 to byte 0, byte 1 to byte 8, zero elsewhere.
        //   ru: byte 2 to byte 0, byte 3 to byte 8, zero elsewhere.
        //
        // Subtract from zero to translate
        // 0x0000000000000001 to 0xffffffffffffffff.

        __m128i sl = _mm_setr_epi32(0x80808000ul, 0x80808080ul, 0x80808001ul, 0x80808080ul);
        __m128i rl = _mm_sub_epi64(zero, _mm_shuffle_epi8(r, sl));

        __m128i su = _mm_setr_epi32(0x80808002ul, 0x80808080ul, 0x80808003ul, 0x80808080ul);
        __m128i ru = _mm_sub_epi64(zero, _mm_shuffle_epi8(r, su));

        return _mm256_castsi256_pd(combine_m128i(ru, rl));
    }

    static __m256d max(const __m256d& a, const __m256d& b) {
        return _mm256_max_pd(a, b);
    }

    static __m256d min(const __m256d& a, const __m256d& b) {
        return _mm256_min_pd(a, b);
    }

    static __m256d abs(const __m256d& x) {
        // NOTE: recent gcc, clang and icc will quite happily vectorize
        // a lane-wise std::abs, but not always optimally (specifically,
        // icc 18 targetting pre-Broadwell architectures, and gcc version 6
        // and earlier).
        //
        // Following instrinsics-based code compiles to essentially the same
        // output as the best-case from auto-vectorization (modulo use of
        // vbroadcastsd vs. indirect memory operand) with all three compilers.

        __m256i m = _mm256_set1_epi64x(0x7fffffffffffffffll);
        return _mm256_and_pd(x, _mm256_castsi256_pd(m));
    }

    //  Exponential is calculated as follows:
    //     e^x = e^g * 2^n,
    //
    //  where g in [-0.5, 0.5) and n is an integer. Obviously 2^n can be calculated
    //  fast with bit shift, whereas e^g is approximated using the order-6 rational
    //  approximation:
    //
    //     e^g = R(g)/R(-g)
    //
    //  with R(x) split into even and odd terms:
    //
    //     R(x) = Q(x^2) + x*P(x^2)
    //
    //  so that the ratio can be computed as:
    //
    //     e^g = 1 + 2*g*P(g^2) / (Q(g^2)-g*P(g^2))
    //
    //  Note that the coefficients for R are close to but not the same as those
    //  from the 6,6 Padé approximant to the exponential. 
    //
    //  The exponents n and g are calculated using the following formulas:
    //
    //  n = floor(x/ln(2) + 0.5)
    //  g = x - n*ln(2)
    //
    //  They can be derived as follows:
    //
    //    e^x = 2^(x/ln(2))
    //        = 2^-0.5 * 2^(x/ln(2) + 0.5)
    //        = 2^r'-0.5 * 2^floor(x/ln(2) + 0.5)     (1)
    //
    // Setting n = floor(x/ln(2) + 0.5),
    //
    //    r' = x/ln(2) - n, and r' in [0, 1)
    //
    // Substituting r' in (1) gives us
    //
    //    e^x = 2^(x/ln(2) - n) * 2^n, where x/ln(2) - n is now in [-0.5, 0.5)
    //        = e^(x-n*ln(2)) * 2^n
    //        = e^g * 2^n, where g = x - n*ln(2)      (2)
    //
    // NOTE: The calculation of ln(2) in (2) is split in two operations to
    // compensate for rounding errors:
    //
    //   ln(2) = C1 + C2, where
    //
    //   C1 = floor(2^k*ln(2))/2^k
    //   C2 = ln(2) - C1
    //
    // We use k=32, since this is what the Cephes library does historically.
    // Theoretically, we could use k=52 to match the IEEE-754 double accuracy, but
    // the standard library seems not to do that, so we are getting differences
    // compared to std::exp() for large exponents.

    static  __m256d exp(const __m256d& x) {
        // Masks for exceptional cases.

        auto is_large = cmp_gt(x, broadcast(exp_maxarg));
        auto is_small = cmp_lt(x, broadcast(exp_minarg));
        auto is_nan = _mm256_cmp_pd(x, x, cmp_unord_q);

        // Compute n and g.

        auto n = _mm256_floor_pd(add(mul(broadcast(ln2inv), x), broadcast(0.5)));

        auto g = sub(x, mul(n, broadcast(ln2C1)));
        g = sub(g, mul(n, broadcast(ln2C2)));

        auto gg = mul(g, g);

        // Compute the g*P(g^2) and Q(g^2).

        auto odd = mul(g, horner(gg, P0exp, P1exp, P2exp));
        auto even = horner(gg, Q0exp, Q1exp, Q2exp, Q3exp);

        // Compute R(g)/R(-g) = 1 + 2*g*P(g^2) / (Q(g^2)-g*P(g^2))

        auto expg = add(broadcast(1), mul(broadcast(2),
            div(odd, sub(even, odd))));

        // Finally, compute product with 2^n.
        // Note: can only achieve full range using the ldexp implementation,
        // rather than multiplying by 2^n directly.

        auto result = ldexp_positive(expg, _mm256_cvtpd_epi32(n));

        return
            ifelse(is_large, broadcast(HUGE_VAL),
            ifelse(is_small, broadcast(0),
            ifelse(is_nan, broadcast(NAN),
                   result)));
    }

protected:
    static __m256i combine_m128i(__m128i hi, __m128i lo) {
        return _mm256_insertf128_si256(_mm256_castsi128_si256(lo), hi, 1);
    }

    static inline __m256d horner(__m256d x, double a0) {
        return broadcast(a0);
    }

    template <typename... T>
    static __m256d horner(__m256d x, double a0, T... tail) {
        return add(mul(x, horner(x, tail...)), broadcast(a0));
    }

    static __m256d exp2int(__m128i n) {
        n = _mm_slli_epi32(n, 20);
        n = _mm_add_epi32(n, _mm_set1_epi32(1023<<20));

        auto nl = _mm_shuffle_epi32(n, 0x50);
        auto nh = _mm_shuffle_epi32(n, 0xfa);
        __m256i nhnl = combine_m128i(nh, nl);

        return _mm256_castps_pd(
            _mm256_blend_ps(_mm256_set1_ps(0),
            _mm256_castsi256_ps(nhnl),0xaa));
    }

    // Compute 2^n*x when both x and 2^n*x are normal, finite and strictly positive doubles.
    static __m256d ldexp_positive(__m256d x, __m128i n) {
        __m256d smask = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7fffffffffffffffll));

        n = _mm_slli_epi32(n, 20);
        auto zero = _mm_set1_epi32(0);
        auto nl = _mm_unpacklo_epi32(zero, n);
        auto nh = _mm_unpackhi_epi32(zero, n);

        __m128d xl = _mm256_castpd256_pd128(x);
        __m128d xh = _mm256_extractf128_pd(x, 1);

        __m128i suml = _mm_add_epi64(nl, _mm_castpd_si128(xl));
        __m128i sumh = _mm_add_epi64(nh, _mm_castpd_si128(xh));
        __m256i sumhl = combine_m128i(sumh, suml);

        return _mm256_and_pd(_mm256_castsi256_pd(sumhl), smask);
    }
};


#ifdef __AVX2__
using avx2_int4 = avx_int4;

struct avx2_double4;

template <>
struct simd_traits<avx2_double4> {
    static constexpr unsigned width = 4;
    using scalar_type = double;
    using vector_type = __m256d;
    using mask_impl = avx2_double4;
};

struct avx2_double4: avx_double4 {
    static __m256d fma(const __m256d& a, const __m256d& b, const __m256d& c) {
        return _mm256_fmadd_pd(a, b, c);
    }

    static vector_type logical_not(const vector_type& a) {
        __m256i ones = {};
        return _mm256_xor_pd(a, _mm256_castsi256_pd(_mm256_cmpeq_epi32(ones, ones)));
    }

    static void mask_copy_to(const __m256d& m, bool* y) {
        __m256i zero = _mm256_setzero_si256();

        // Translate 0xffffffffffffffff scalars to 0x0000000000000001.

        __m256i x = _mm256_castpd_si256(m);
        x = _mm256_sub_epi64(zero, x);

        // Move lower 32-bits of each field to lower 128-bit half of x.

        __m256i s1 = _mm256_setr_epi32(0,2,4,6,0,0,0,0);
        x = _mm256_permutevar8x32_epi32(x, s1);

        // Move the lowest byte from each 32-bit field to bottom bytes.

        __m128i s2 = _mm_setr_epi32(0x0c080400ul,0,0,0);
        __m128i r = _mm_shuffle_epi8(_mm256_castsi256_si128(x), s2);
        std::memcpy(y, &r, 4);
    }

    static __m256d mask_copy_from(const bool* w) {
        __m256i zero = _mm256_setzero_si256();

        __m128i r;
        std::memcpy(&r, w, 4);
        return _mm256_castsi256_pd(_mm256_sub_epi64(zero, _mm256_cvtepi8_epi64(r)));
    }

    static __m256d gather(avx2_int4, const double* p, const __m128i& index) {
        return _mm256_i32gather_pd(p, index, 8);
    }

    static __m256d gather(avx2_int4, __m256d a, const double* p, const __m128i& index, const __m256d& mask) {
        return  _mm256_mask_i32gather_pd(a, p, index, mask, 8);
    };

    // Same algorithm as for AVX, but use FMA and more efficient bit twiddling.
    static  __m256d exp(const __m256d& x) {
        // Masks for exceptional cases.

        auto is_large = cmp_gt(x, broadcast(exp_maxarg));
        auto is_small = cmp_lt(x, broadcast(exp_minarg));
        auto is_nan = _mm256_cmp_pd(x, x, cmp_unord_q);

        // Compute n and g.

        auto n = _mm256_floor_pd(fma(broadcast(ln2inv), x, broadcast(0.5)));

        auto g = fma(n, broadcast(-ln2C1), x);
        g = fma(n, broadcast(-ln2C2), g);

        auto gg = mul(g, g);

        // Compute the g*P(g^2) and Q(g^2).

        auto odd = mul(g, horner(gg, P0exp, P1exp, P2exp));
        auto even = horner(gg, Q0exp, Q1exp, Q2exp, Q3exp);

        // Compute R(g)/R(-g) = 1 + 2*g*P(g^2) / (Q(g^2)-g*P(g^2))

        auto expg = fma(broadcast(2), div(odd, sub(even, odd)), broadcast(1));

        // Finally, compute product with 2^n.
        // Note: can only achieve full range using the ldexp implementation,
        // rather than multiplying by 2^n directly.

        auto result = ldexp_positive(expg, _mm256_cvtpd_epi32(n));

        return
            ifelse(is_large, broadcast(HUGE_VAL),
            ifelse(is_small, broadcast(0),
            ifelse(is_nan, broadcast(NAN),
                   result)));
    }

protected:
    static inline __m256d horner(__m256d x, double a0) {
        return broadcast(a0);
    }

    template <typename... T>
    static __m256d horner(__m256d x, double a0, T... tail) {
        return fma(x, horner(x, tail...), broadcast(a0));
    }

    // Compute 2^n*x when both x and 2^n*x are normal, finite and strictly positive doubles.
    static __m256d ldexp_positive(__m256d x, __m128i n) {
        __m256d smask = _mm256_castsi256_pd(_mm256_set1_epi64x(0x7fffffffffffffffll));
        __m256i nshift = _mm256_slli_epi64(_mm256_cvtepi32_epi64(n), 52);
        __m256i sum = _mm256_add_epi64(nshift, _mm256_castpd_si256(x));
        return _mm256_and_pd(_mm256_castsi256_pd(sum), smask);
    }
};
#endif // def __AVX2__

} // namespace simd_detail

namespace simd_abi {
    template <typename T, unsigned N> struct avx;

    template <> struct avx<int, 4> { using type = simd_detail::avx_int4; };
    template <> struct avx<double, 4> { using type = simd_detail::avx_double4; };

#ifdef __AVX2__
    template <typename T, unsigned N> struct avx2;

    template <> struct avx2<int, 4> { using type = simd_detail::avx2_int4; };
    template <> struct avx2<double, 4> { using type = simd_detail::avx2_double4; };
#endif
} // namespace simd_abi;

} // namespace arb

#endif // def __AVX__
