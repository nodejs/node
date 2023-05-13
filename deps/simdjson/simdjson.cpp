/* auto-generated on 2023-06-15 09:11:14 -0400. Do not edit! */
/* begin file src/simdjson.cpp */
#include "simdjson.h"

SIMDJSON_PUSH_DISABLE_WARNINGS
SIMDJSON_DISABLE_UNDESIRED_WARNINGS

/* begin file src/to_chars.cpp */
#include <cstring>
#include <cstdint>
#include <array>
#include <cmath>

namespace simdjson {
namespace internal {
/*!
implements the Grisu2 algorithm for binary to decimal floating-point
conversion.
Adapted from JSON for Modern C++

This implementation is a slightly modified version of the reference
implementation which may be obtained from
http://florian.loitsch.com/publications (bench.tar.gz).
The code is distributed under the MIT license, Copyright (c) 2009 Florian
Loitsch. For a detailed description of the algorithm see: [1] Loitsch, "Printing
Floating-Point Numbers Quickly and Accurately with Integers", Proceedings of the
ACM SIGPLAN 2010 Conference on Programming Language Design and Implementation,
PLDI 2010 [2] Burger, Dybvig, "Printing Floating-Point Numbers Quickly and
Accurately", Proceedings of the ACM SIGPLAN 1996 Conference on Programming
Language Design and Implementation, PLDI 1996
*/
namespace dtoa_impl {

template <typename Target, typename Source>
Target reinterpret_bits(const Source source) {
  static_assert(sizeof(Target) == sizeof(Source), "size mismatch");

  Target target;
  std::memcpy(&target, &source, sizeof(Source));
  return target;
}

struct diyfp // f * 2^e
{
  static constexpr int kPrecision = 64; // = q

  std::uint64_t f = 0;
  int e = 0;

  constexpr diyfp(std::uint64_t f_, int e_) noexcept : f(f_), e(e_) {}

  /*!
  @brief returns x - y
  @pre x.e == y.e and x.f >= y.f
  */
  static diyfp sub(const diyfp &x, const diyfp &y) noexcept {

    return {x.f - y.f, x.e};
  }

  /*!
  @brief returns x * y
  @note The result is rounded. (Only the upper q bits are returned.)
  */
  static diyfp mul(const diyfp &x, const diyfp &y) noexcept {
    static_assert(kPrecision == 64, "internal error");

    // Computes:
    //  f = round((x.f * y.f) / 2^q)
    //  e = x.e + y.e + q

    // Emulate the 64-bit * 64-bit multiplication:
    //
    // p = u * v
    //   = (u_lo + 2^32 u_hi) (v_lo + 2^32 v_hi)
    //   = (u_lo v_lo         ) + 2^32 ((u_lo v_hi         ) + (u_hi v_lo )) +
    //   2^64 (u_hi v_hi         ) = (p0                ) + 2^32 ((p1 ) + (p2 ))
    //   + 2^64 (p3                ) = (p0_lo + 2^32 p0_hi) + 2^32 ((p1_lo +
    //   2^32 p1_hi) + (p2_lo + 2^32 p2_hi)) + 2^64 (p3                ) =
    //   (p0_lo             ) + 2^32 (p0_hi + p1_lo + p2_lo ) + 2^64 (p1_hi +
    //   p2_hi + p3) = (p0_lo             ) + 2^32 (Q ) + 2^64 (H ) = (p0_lo ) +
    //   2^32 (Q_lo + 2^32 Q_hi                           ) + 2^64 (H )
    //
    // (Since Q might be larger than 2^32 - 1)
    //
    //   = (p0_lo + 2^32 Q_lo) + 2^64 (Q_hi + H)
    //
    // (Q_hi + H does not overflow a 64-bit int)
    //
    //   = p_lo + 2^64 p_hi

    const std::uint64_t u_lo = x.f & 0xFFFFFFFFu;
    const std::uint64_t u_hi = x.f >> 32u;
    const std::uint64_t v_lo = y.f & 0xFFFFFFFFu;
    const std::uint64_t v_hi = y.f >> 32u;

    const std::uint64_t p0 = u_lo * v_lo;
    const std::uint64_t p1 = u_lo * v_hi;
    const std::uint64_t p2 = u_hi * v_lo;
    const std::uint64_t p3 = u_hi * v_hi;

    const std::uint64_t p0_hi = p0 >> 32u;
    const std::uint64_t p1_lo = p1 & 0xFFFFFFFFu;
    const std::uint64_t p1_hi = p1 >> 32u;
    const std::uint64_t p2_lo = p2 & 0xFFFFFFFFu;
    const std::uint64_t p2_hi = p2 >> 32u;

    std::uint64_t Q = p0_hi + p1_lo + p2_lo;

    // The full product might now be computed as
    //
    // p_hi = p3 + p2_hi + p1_hi + (Q >> 32)
    // p_lo = p0_lo + (Q << 32)
    //
    // But in this particular case here, the full p_lo is not required.
    // Effectively we only need to add the highest bit in p_lo to p_hi (and
    // Q_hi + 1 does not overflow).

    Q += std::uint64_t{1} << (64u - 32u - 1u); // round, ties up

    const std::uint64_t h = p3 + p2_hi + p1_hi + (Q >> 32u);

    return {h, x.e + y.e + 64};
  }

  /*!
  @brief normalize x such that the significand is >= 2^(q-1)
  @pre x.f != 0
  */
  static diyfp normalize(diyfp x) noexcept {

    while ((x.f >> 63u) == 0) {
      x.f <<= 1u;
      x.e--;
    }

    return x;
  }

  /*!
  @brief normalize x such that the result has the exponent E
  @pre e >= x.e and the upper e - x.e bits of x.f must be zero.
  */
  static diyfp normalize_to(const diyfp &x,
                            const int target_exponent) noexcept {
    const int delta = x.e - target_exponent;

    return {x.f << delta, target_exponent};
  }
};

struct boundaries {
  diyfp w;
  diyfp minus;
  diyfp plus;
};

/*!
Compute the (normalized) diyfp representing the input number 'value' and its
boundaries.
@pre value must be finite and positive
*/
template <typename FloatType> boundaries compute_boundaries(FloatType value) {

  // Convert the IEEE representation into a diyfp.
  //
  // If v is denormal:
  //      value = 0.F * 2^(1 - bias) = (          F) * 2^(1 - bias - (p-1))
  // If v is normalized:
  //      value = 1.F * 2^(E - bias) = (2^(p-1) + F) * 2^(E - bias - (p-1))

  static_assert(std::numeric_limits<FloatType>::is_iec559,
                "internal error: dtoa_short requires an IEEE-754 "
                "floating-point implementation");

  constexpr int kPrecision =
      std::numeric_limits<FloatType>::digits; // = p (includes the hidden bit)
  constexpr int kBias =
      std::numeric_limits<FloatType>::max_exponent - 1 + (kPrecision - 1);
  constexpr int kMinExp = 1 - kBias;
  constexpr std::uint64_t kHiddenBit = std::uint64_t{1}
                                       << (kPrecision - 1); // = 2^(p-1)

  using bits_type = typename std::conditional<kPrecision == 24, std::uint32_t,
                                              std::uint64_t>::type;

  const std::uint64_t bits = reinterpret_bits<bits_type>(value);
  const std::uint64_t E = bits >> (kPrecision - 1);
  const std::uint64_t F = bits & (kHiddenBit - 1);

  const bool is_denormal = E == 0;
  const diyfp v = is_denormal
                      ? diyfp(F, kMinExp)
                      : diyfp(F + kHiddenBit, static_cast<int>(E) - kBias);

  // Compute the boundaries m- and m+ of the floating-point value
  // v = f * 2^e.
  //
  // Determine v- and v+, the floating-point predecessor and successor if v,
  // respectively.
  //
  //      v- = v - 2^e        if f != 2^(p-1) or e == e_min                (A)
  //         = v - 2^(e-1)    if f == 2^(p-1) and e > e_min                (B)
  //
  //      v+ = v + 2^e
  //
  // Let m- = (v- + v) / 2 and m+ = (v + v+) / 2. All real numbers _strictly_
  // between m- and m+ round to v, regardless of how the input rounding
  // algorithm breaks ties.
  //
  //      ---+-------------+-------------+-------------+-------------+---  (A)
  //         v-            m-            v             m+            v+
  //
  //      -----------------+------+------+-------------+-------------+---  (B)
  //                       v-     m-     v             m+            v+

  const bool lower_boundary_is_closer = F == 0 && E > 1;
  const diyfp m_plus = diyfp(2 * v.f + 1, v.e - 1);
  const diyfp m_minus = lower_boundary_is_closer
                            ? diyfp(4 * v.f - 1, v.e - 2)  // (B)
                            : diyfp(2 * v.f - 1, v.e - 1); // (A)

  // Determine the normalized w+ = m+.
  const diyfp w_plus = diyfp::normalize(m_plus);

  // Determine w- = m- such that e_(w-) = e_(w+).
  const diyfp w_minus = diyfp::normalize_to(m_minus, w_plus.e);

  return {diyfp::normalize(v), w_minus, w_plus};
}

// Given normalized diyfp w, Grisu needs to find a (normalized) cached
// power-of-ten c, such that the exponent of the product c * w = f * 2^e lies
// within a certain range [alpha, gamma] (Definition 3.2 from [1])
//
//      alpha <= e = e_c + e_w + q <= gamma
//
// or
//
//      f_c * f_w * 2^alpha <= f_c 2^(e_c) * f_w 2^(e_w) * 2^q
//                          <= f_c * f_w * 2^gamma
//
// Since c and w are normalized, i.e. 2^(q-1) <= f < 2^q, this implies
//
//      2^(q-1) * 2^(q-1) * 2^alpha <= c * w * 2^q < 2^q * 2^q * 2^gamma
//
// or
//
//      2^(q - 2 + alpha) <= c * w < 2^(q + gamma)
//
// The choice of (alpha,gamma) determines the size of the table and the form of
// the digit generation procedure. Using (alpha,gamma)=(-60,-32) works out well
// in practice:
//
// The idea is to cut the number c * w = f * 2^e into two parts, which can be
// processed independently: An integral part p1, and a fractional part p2:
//
//      f * 2^e = ( (f div 2^-e) * 2^-e + (f mod 2^-e) ) * 2^e
//              = (f div 2^-e) + (f mod 2^-e) * 2^e
//              = p1 + p2 * 2^e
//
// The conversion of p1 into decimal form requires a series of divisions and
// modulos by (a power of) 10. These operations are faster for 32-bit than for
// 64-bit integers, so p1 should ideally fit into a 32-bit integer. This can be
// achieved by choosing
//
//      -e >= 32   or   e <= -32 := gamma
//
// In order to convert the fractional part
//
//      p2 * 2^e = p2 / 2^-e = d[-1] / 10^1 + d[-2] / 10^2 + ...
//
// into decimal form, the fraction is repeatedly multiplied by 10 and the digits
// d[-i] are extracted in order:
//
//      (10 * p2) div 2^-e = d[-1]
//      (10 * p2) mod 2^-e = d[-2] / 10^1 + ...
//
// The multiplication by 10 must not overflow. It is sufficient to choose
//
//      10 * p2 < 16 * p2 = 2^4 * p2 <= 2^64.
//
// Since p2 = f mod 2^-e < 2^-e,
//
//      -e <= 60   or   e >= -60 := alpha

constexpr int kAlpha = -60;
constexpr int kGamma = -32;

struct cached_power // c = f * 2^e ~= 10^k
{
  std::uint64_t f;
  int e;
  int k;
};

/*!
For a normalized diyfp w = f * 2^e, this function returns a (normalized) cached
power-of-ten c = f_c * 2^e_c, such that the exponent of the product w * c
satisfies (Definition 3.2 from [1])
     alpha <= e_c + e + q <= gamma.
*/
inline cached_power get_cached_power_for_binary_exponent(int e) {
  // Now
  //
  //      alpha <= e_c + e + q <= gamma                                    (1)
  //      ==> f_c * 2^alpha <= c * 2^e * 2^q
  //
  // and since the c's are normalized, 2^(q-1) <= f_c,
  //
  //      ==> 2^(q - 1 + alpha) <= c * 2^(e + q)
  //      ==> 2^(alpha - e - 1) <= c
  //
  // If c were an exact power of ten, i.e. c = 10^k, one may determine k as
  //
  //      k = ceil( log_10( 2^(alpha - e - 1) ) )
  //        = ceil( (alpha - e - 1) * log_10(2) )
  //
  // From the paper:
  // "In theory the result of the procedure could be wrong since c is rounded,
  //  and the computation itself is approximated [...]. In practice, however,
  //  this simple function is sufficient."
  //
  // For IEEE double precision floating-point numbers converted into
  // normalized diyfp's w = f * 2^e, with q = 64,
  //
  //      e >= -1022      (min IEEE exponent)
  //           -52        (p - 1)
  //           -52        (p - 1, possibly normalize denormal IEEE numbers)
  //           -11        (normalize the diyfp)
  //         = -1137
  //
  // and
  //
  //      e <= +1023      (max IEEE exponent)
  //           -52        (p - 1)
  //           -11        (normalize the diyfp)
  //         = 960
  //
  // This binary exponent range [-1137,960] results in a decimal exponent
  // range [-307,324]. One does not need to store a cached power for each
  // k in this range. For each such k it suffices to find a cached power
  // such that the exponent of the product lies in [alpha,gamma].
  // This implies that the difference of the decimal exponents of adjacent
  // table entries must be less than or equal to
  //
  //      floor( (gamma - alpha) * log_10(2) ) = 8.
  //
  // (A smaller distance gamma-alpha would require a larger table.)

  // NB:
  // Actually this function returns c, such that -60 <= e_c + e + 64 <= -34.

  constexpr int kCachedPowersMinDecExp = -300;
  constexpr int kCachedPowersDecStep = 8;

  static constexpr std::array<cached_power, 79> kCachedPowers = {{
      {0xAB70FE17C79AC6CA, -1060, -300}, {0xFF77B1FCBEBCDC4F, -1034, -292},
      {0xBE5691EF416BD60C, -1007, -284}, {0x8DD01FAD907FFC3C, -980, -276},
      {0xD3515C2831559A83, -954, -268},  {0x9D71AC8FADA6C9B5, -927, -260},
      {0xEA9C227723EE8BCB, -901, -252},  {0xAECC49914078536D, -874, -244},
      {0x823C12795DB6CE57, -847, -236},  {0xC21094364DFB5637, -821, -228},
      {0x9096EA6F3848984F, -794, -220},  {0xD77485CB25823AC7, -768, -212},
      {0xA086CFCD97BF97F4, -741, -204},  {0xEF340A98172AACE5, -715, -196},
      {0xB23867FB2A35B28E, -688, -188},  {0x84C8D4DFD2C63F3B, -661, -180},
      {0xC5DD44271AD3CDBA, -635, -172},  {0x936B9FCEBB25C996, -608, -164},
      {0xDBAC6C247D62A584, -582, -156},  {0xA3AB66580D5FDAF6, -555, -148},
      {0xF3E2F893DEC3F126, -529, -140},  {0xB5B5ADA8AAFF80B8, -502, -132},
      {0x87625F056C7C4A8B, -475, -124},  {0xC9BCFF6034C13053, -449, -116},
      {0x964E858C91BA2655, -422, -108},  {0xDFF9772470297EBD, -396, -100},
      {0xA6DFBD9FB8E5B88F, -369, -92},   {0xF8A95FCF88747D94, -343, -84},
      {0xB94470938FA89BCF, -316, -76},   {0x8A08F0F8BF0F156B, -289, -68},
      {0xCDB02555653131B6, -263, -60},   {0x993FE2C6D07B7FAC, -236, -52},
      {0xE45C10C42A2B3B06, -210, -44},   {0xAA242499697392D3, -183, -36},
      {0xFD87B5F28300CA0E, -157, -28},   {0xBCE5086492111AEB, -130, -20},
      {0x8CBCCC096F5088CC, -103, -12},   {0xD1B71758E219652C, -77, -4},
      {0x9C40000000000000, -50, 4},      {0xE8D4A51000000000, -24, 12},
      {0xAD78EBC5AC620000, 3, 20},       {0x813F3978F8940984, 30, 28},
      {0xC097CE7BC90715B3, 56, 36},      {0x8F7E32CE7BEA5C70, 83, 44},
      {0xD5D238A4ABE98068, 109, 52},     {0x9F4F2726179A2245, 136, 60},
      {0xED63A231D4C4FB27, 162, 68},     {0xB0DE65388CC8ADA8, 189, 76},
      {0x83C7088E1AAB65DB, 216, 84},     {0xC45D1DF942711D9A, 242, 92},
      {0x924D692CA61BE758, 269, 100},    {0xDA01EE641A708DEA, 295, 108},
      {0xA26DA3999AEF774A, 322, 116},    {0xF209787BB47D6B85, 348, 124},
      {0xB454E4A179DD1877, 375, 132},    {0x865B86925B9BC5C2, 402, 140},
      {0xC83553C5C8965D3D, 428, 148},    {0x952AB45CFA97A0B3, 455, 156},
      {0xDE469FBD99A05FE3, 481, 164},    {0xA59BC234DB398C25, 508, 172},
      {0xF6C69A72A3989F5C, 534, 180},    {0xB7DCBF5354E9BECE, 561, 188},
      {0x88FCF317F22241E2, 588, 196},    {0xCC20CE9BD35C78A5, 614, 204},
      {0x98165AF37B2153DF, 641, 212},    {0xE2A0B5DC971F303A, 667, 220},
      {0xA8D9D1535CE3B396, 694, 228},    {0xFB9B7CD9A4A7443C, 720, 236},
      {0xBB764C4CA7A44410, 747, 244},    {0x8BAB8EEFB6409C1A, 774, 252},
      {0xD01FEF10A657842C, 800, 260},    {0x9B10A4E5E9913129, 827, 268},
      {0xE7109BFBA19C0C9D, 853, 276},    {0xAC2820D9623BF429, 880, 284},
      {0x80444B5E7AA7CF85, 907, 292},    {0xBF21E44003ACDD2D, 933, 300},
      {0x8E679C2F5E44FF8F, 960, 308},    {0xD433179D9C8CB841, 986, 316},
      {0x9E19DB92B4E31BA9, 1013, 324},
  }};

  // This computation gives exactly the same results for k as
  //      k = ceil((kAlpha - e - 1) * 0.30102999566398114)
  // for |e| <= 1500, but doesn't require floating-point operations.
  // NB: log_10(2) ~= 78913 / 2^18
  const int f = kAlpha - e - 1;
  const int k = (f * 78913) / (1 << 18) + static_cast<int>(f > 0);

  const int index = (-kCachedPowersMinDecExp + k + (kCachedPowersDecStep - 1)) /
                    kCachedPowersDecStep;

  const cached_power cached = kCachedPowers[static_cast<std::size_t>(index)];

  return cached;
}

/*!
For n != 0, returns k, such that pow10 := 10^(k-1) <= n < 10^k.
For n == 0, returns 1 and sets pow10 := 1.
*/
inline int find_largest_pow10(const std::uint32_t n, std::uint32_t &pow10) {
  // LCOV_EXCL_START
  if (n >= 1000000000) {
    pow10 = 1000000000;
    return 10;
  }
  // LCOV_EXCL_STOP
  else if (n >= 100000000) {
    pow10 = 100000000;
    return 9;
  } else if (n >= 10000000) {
    pow10 = 10000000;
    return 8;
  } else if (n >= 1000000) {
    pow10 = 1000000;
    return 7;
  } else if (n >= 100000) {
    pow10 = 100000;
    return 6;
  } else if (n >= 10000) {
    pow10 = 10000;
    return 5;
  } else if (n >= 1000) {
    pow10 = 1000;
    return 4;
  } else if (n >= 100) {
    pow10 = 100;
    return 3;
  } else if (n >= 10) {
    pow10 = 10;
    return 2;
  } else {
    pow10 = 1;
    return 1;
  }
}

inline void grisu2_round(char *buf, int len, std::uint64_t dist,
                         std::uint64_t delta, std::uint64_t rest,
                         std::uint64_t ten_k) {

  //               <--------------------------- delta ---->
  //                                  <---- dist --------->
  // --------------[------------------+-------------------]--------------
  //               M-                 w                   M+
  //
  //                                  ten_k
  //                                <------>
  //                                       <---- rest ---->
  // --------------[------------------+----+--------------]--------------
  //                                  w    V
  //                                       = buf * 10^k
  //
  // ten_k represents a unit-in-the-last-place in the decimal representation
  // stored in buf.
  // Decrement buf by ten_k while this takes buf closer to w.

  // The tests are written in this order to avoid overflow in unsigned
  // integer arithmetic.

  while (rest < dist && delta - rest >= ten_k &&
         (rest + ten_k < dist || dist - rest > rest + ten_k - dist)) {
    buf[len - 1]--;
    rest += ten_k;
  }
}

/*!
Generates V = buffer * 10^decimal_exponent, such that M- <= V <= M+.
M- and M+ must be normalized and share the same exponent -60 <= e <= -32.
*/
inline void grisu2_digit_gen(char *buffer, int &length, int &decimal_exponent,
                             diyfp M_minus, diyfp w, diyfp M_plus) {
  static_assert(kAlpha >= -60, "internal error");
  static_assert(kGamma <= -32, "internal error");

  // Generates the digits (and the exponent) of a decimal floating-point
  // number V = buffer * 10^decimal_exponent in the range [M-, M+]. The diyfp's
  // w, M- and M+ share the same exponent e, which satisfies alpha <= e <=
  // gamma.
  //
  //               <--------------------------- delta ---->
  //                                  <---- dist --------->
  // --------------[------------------+-------------------]--------------
  //               M-                 w                   M+
  //
  // Grisu2 generates the digits of M+ from left to right and stops as soon as
  // V is in [M-,M+].

  std::uint64_t delta =
      diyfp::sub(M_plus, M_minus)
          .f; // (significand of (M+ - M-), implicit exponent is e)
  std::uint64_t dist =
      diyfp::sub(M_plus, w)
          .f; // (significand of (M+ - w ), implicit exponent is e)

  // Split M+ = f * 2^e into two parts p1 and p2 (note: e < 0):
  //
  //      M+ = f * 2^e
  //         = ((f div 2^-e) * 2^-e + (f mod 2^-e)) * 2^e
  //         = ((p1        ) * 2^-e + (p2        )) * 2^e
  //         = p1 + p2 * 2^e

  const diyfp one(std::uint64_t{1} << -M_plus.e, M_plus.e);

  auto p1 = static_cast<std::uint32_t>(
      M_plus.f >>
      -one.e); // p1 = f div 2^-e (Since -e >= 32, p1 fits into a 32-bit int.)
  std::uint64_t p2 = M_plus.f & (one.f - 1); // p2 = f mod 2^-e

  // 1)
  //
  // Generate the digits of the integral part p1 = d[n-1]...d[1]d[0]

  std::uint32_t pow10;
  const int k = find_largest_pow10(p1, pow10);

  //      10^(k-1) <= p1 < 10^k, pow10 = 10^(k-1)
  //
  //      p1 = (p1 div 10^(k-1)) * 10^(k-1) + (p1 mod 10^(k-1))
  //         = (d[k-1]         ) * 10^(k-1) + (p1 mod 10^(k-1))
  //
  //      M+ = p1                                             + p2 * 2^e
  //         = d[k-1] * 10^(k-1) + (p1 mod 10^(k-1))          + p2 * 2^e
  //         = d[k-1] * 10^(k-1) + ((p1 mod 10^(k-1)) * 2^-e + p2) * 2^e
  //         = d[k-1] * 10^(k-1) + (                         rest) * 2^e
  //
  // Now generate the digits d[n] of p1 from left to right (n = k-1,...,0)
  //
  //      p1 = d[k-1]...d[n] * 10^n + d[n-1]...d[0]
  //
  // but stop as soon as
  //
  //      rest * 2^e = (d[n-1]...d[0] * 2^-e + p2) * 2^e <= delta * 2^e

  int n = k;
  while (n > 0) {
    // Invariants:
    //      M+ = buffer * 10^n + (p1 + p2 * 2^e)    (buffer = 0 for n = k)
    //      pow10 = 10^(n-1) <= p1 < 10^n
    //
    const std::uint32_t d = p1 / pow10; // d = p1 div 10^(n-1)
    const std::uint32_t r = p1 % pow10; // r = p1 mod 10^(n-1)
    //
    //      M+ = buffer * 10^n + (d * 10^(n-1) + r) + p2 * 2^e
    //         = (buffer * 10 + d) * 10^(n-1) + (r + p2 * 2^e)
    //
    buffer[length++] = static_cast<char>('0' + d); // buffer := buffer * 10 + d
    //
    //      M+ = buffer * 10^(n-1) + (r + p2 * 2^e)
    //
    p1 = r;
    n--;
    //
    //      M+ = buffer * 10^n + (p1 + p2 * 2^e)
    //      pow10 = 10^n
    //

    // Now check if enough digits have been generated.
    // Compute
    //
    //      p1 + p2 * 2^e = (p1 * 2^-e + p2) * 2^e = rest * 2^e
    //
    // Note:
    // Since rest and delta share the same exponent e, it suffices to
    // compare the significands.
    const std::uint64_t rest = (std::uint64_t{p1} << -one.e) + p2;
    if (rest <= delta) {
      // V = buffer * 10^n, with M- <= V <= M+.

      decimal_exponent += n;

      // We may now just stop. But instead look if the buffer could be
      // decremented to bring V closer to w.
      //
      // pow10 = 10^n is now 1 ulp in the decimal representation V.
      // The rounding procedure works with diyfp's with an implicit
      // exponent of e.
      //
      //      10^n = (10^n * 2^-e) * 2^e = ulp * 2^e
      //
      const std::uint64_t ten_n = std::uint64_t{pow10} << -one.e;
      grisu2_round(buffer, length, dist, delta, rest, ten_n);

      return;
    }

    pow10 /= 10;
    //
    //      pow10 = 10^(n-1) <= p1 < 10^n
    // Invariants restored.
  }

  // 2)
  //
  // The digits of the integral part have been generated:
  //
  //      M+ = d[k-1]...d[1]d[0] + p2 * 2^e
  //         = buffer            + p2 * 2^e
  //
  // Now generate the digits of the fractional part p2 * 2^e.
  //
  // Note:
  // No decimal point is generated: the exponent is adjusted instead.
  //
  // p2 actually represents the fraction
  //
  //      p2 * 2^e
  //          = p2 / 2^-e
  //          = d[-1] / 10^1 + d[-2] / 10^2 + ...
  //
  // Now generate the digits d[-m] of p1 from left to right (m = 1,2,...)
  //
  //      p2 * 2^e = d[-1]d[-2]...d[-m] * 10^-m
  //                      + 10^-m * (d[-m-1] / 10^1 + d[-m-2] / 10^2 + ...)
  //
  // using
  //
  //      10^m * p2 = ((10^m * p2) div 2^-e) * 2^-e + ((10^m * p2) mod 2^-e)
  //                = (                   d) * 2^-e + (                   r)
  //
  // or
  //      10^m * p2 * 2^e = d + r * 2^e
  //
  // i.e.
  //
  //      M+ = buffer + p2 * 2^e
  //         = buffer + 10^-m * (d + r * 2^e)
  //         = (buffer * 10^m + d) * 10^-m + 10^-m * r * 2^e
  //
  // and stop as soon as 10^-m * r * 2^e <= delta * 2^e

  int m = 0;
  for (;;) {
    // Invariant:
    //      M+ = buffer * 10^-m + 10^-m * (d[-m-1] / 10 + d[-m-2] / 10^2 + ...)
    //      * 2^e
    //         = buffer * 10^-m + 10^-m * (p2                                 )
    //         * 2^e = buffer * 10^-m + 10^-m * (1/10 * (10 * p2) ) * 2^e =
    //         buffer * 10^-m + 10^-m * (1/10 * ((10*p2 div 2^-e) * 2^-e +
    //         (10*p2 mod 2^-e)) * 2^e
    //
    p2 *= 10;
    const std::uint64_t d = p2 >> -one.e;     // d = (10 * p2) div 2^-e
    const std::uint64_t r = p2 & (one.f - 1); // r = (10 * p2) mod 2^-e
    //
    //      M+ = buffer * 10^-m + 10^-m * (1/10 * (d * 2^-e + r) * 2^e
    //         = buffer * 10^-m + 10^-m * (1/10 * (d + r * 2^e))
    //         = (buffer * 10 + d) * 10^(-m-1) + 10^(-m-1) * r * 2^e
    //
    buffer[length++] = static_cast<char>('0' + d); // buffer := buffer * 10 + d
    //
    //      M+ = buffer * 10^(-m-1) + 10^(-m-1) * r * 2^e
    //
    p2 = r;
    m++;
    //
    //      M+ = buffer * 10^-m + 10^-m * p2 * 2^e
    // Invariant restored.

    // Check if enough digits have been generated.
    //
    //      10^-m * p2 * 2^e <= delta * 2^e
    //              p2 * 2^e <= 10^m * delta * 2^e
    //                    p2 <= 10^m * delta
    delta *= 10;
    dist *= 10;
    if (p2 <= delta) {
      break;
    }
  }

  // V = buffer * 10^-m, with M- <= V <= M+.

  decimal_exponent -= m;

  // 1 ulp in the decimal representation is now 10^-m.
  // Since delta and dist are now scaled by 10^m, we need to do the
  // same with ulp in order to keep the units in sync.
  //
  //      10^m * 10^-m = 1 = 2^-e * 2^e = ten_m * 2^e
  //
  const std::uint64_t ten_m = one.f;
  grisu2_round(buffer, length, dist, delta, p2, ten_m);

  // By construction this algorithm generates the shortest possible decimal
  // number (Loitsch, Theorem 6.2) which rounds back to w.
  // For an input number of precision p, at least
  //
  //      N = 1 + ceil(p * log_10(2))
  //
  // decimal digits are sufficient to identify all binary floating-point
  // numbers (Matula, "In-and-Out conversions").
  // This implies that the algorithm does not produce more than N decimal
  // digits.
  //
  //      N = 17 for p = 53 (IEEE double precision)
  //      N = 9  for p = 24 (IEEE single precision)
}

/*!
v = buf * 10^decimal_exponent
len is the length of the buffer (number of decimal digits)
The buffer must be large enough, i.e. >= max_digits10.
*/
inline void grisu2(char *buf, int &len, int &decimal_exponent, diyfp m_minus,
                   diyfp v, diyfp m_plus) {

  //  --------(-----------------------+-----------------------)--------    (A)
  //          m-                      v                       m+
  //
  //  --------------------(-----------+-----------------------)--------    (B)
  //                      m-          v                       m+
  //
  // First scale v (and m- and m+) such that the exponent is in the range
  // [alpha, gamma].

  const cached_power cached = get_cached_power_for_binary_exponent(m_plus.e);

  const diyfp c_minus_k(cached.f, cached.e); // = c ~= 10^-k

  // The exponent of the products is = v.e + c_minus_k.e + q and is in the range
  // [alpha,gamma]
  const diyfp w = diyfp::mul(v, c_minus_k);
  const diyfp w_minus = diyfp::mul(m_minus, c_minus_k);
  const diyfp w_plus = diyfp::mul(m_plus, c_minus_k);

  //  ----(---+---)---------------(---+---)---------------(---+---)----
  //          w-                      w                       w+
  //          = c*m-                  = c*v                   = c*m+
  //
  // diyfp::mul rounds its result and c_minus_k is approximated too. w, w- and
  // w+ are now off by a small amount.
  // In fact:
  //
  //      w - v * 10^k < 1 ulp
  //
  // To account for this inaccuracy, add resp. subtract 1 ulp.
  //
  //  --------+---[---------------(---+---)---------------]---+--------
  //          w-  M-                  w                   M+  w+
  //
  // Now any number in [M-, M+] (bounds included) will round to w when input,
  // regardless of how the input rounding algorithm breaks ties.
  //
  // And digit_gen generates the shortest possible such number in [M-, M+].
  // Note that this does not mean that Grisu2 always generates the shortest
  // possible number in the interval (m-, m+).
  const diyfp M_minus(w_minus.f + 1, w_minus.e);
  const diyfp M_plus(w_plus.f - 1, w_plus.e);

  decimal_exponent = -cached.k; // = -(-k) = k

  grisu2_digit_gen(buf, len, decimal_exponent, M_minus, w, M_plus);
}

/*!
v = buf * 10^decimal_exponent
len is the length of the buffer (number of decimal digits)
The buffer must be large enough, i.e. >= max_digits10.
*/
template <typename FloatType>
void grisu2(char *buf, int &len, int &decimal_exponent, FloatType value) {
  static_assert(diyfp::kPrecision >= std::numeric_limits<FloatType>::digits + 3,
                "internal error: not enough precision");

  // If the neighbors (and boundaries) of 'value' are always computed for
  // double-precision numbers, all float's can be recovered using strtod (and
  // strtof). However, the resulting decimal representations are not exactly
  // "short".
  //
  // The documentation for 'std::to_chars'
  // (https://en.cppreference.com/w/cpp/utility/to_chars) says "value is
  // converted to a string as if by std::sprintf in the default ("C") locale"
  // and since sprintf promotes float's to double's, I think this is exactly
  // what 'std::to_chars' does. On the other hand, the documentation for
  // 'std::to_chars' requires that "parsing the representation using the
  // corresponding std::from_chars function recovers value exactly". That
  // indicates that single precision floating-point numbers should be recovered
  // using 'std::strtof'.
  //
  // NB: If the neighbors are computed for single-precision numbers, there is a
  // single float
  //     (7.0385307e-26f) which can't be recovered using strtod. The resulting
  //     double precision value is off by 1 ulp.
#if 0
    const boundaries w = compute_boundaries(static_cast<double>(value));
#else
  const boundaries w = compute_boundaries(value);
#endif

  grisu2(buf, len, decimal_exponent, w.minus, w.w, w.plus);
}

/*!
@brief appends a decimal representation of e to buf
@return a pointer to the element following the exponent.
@pre -1000 < e < 1000
*/
inline char *append_exponent(char *buf, int e) {

  if (e < 0) {
    e = -e;
    *buf++ = '-';
  } else {
    *buf++ = '+';
  }

  auto k = static_cast<std::uint32_t>(e);
  if (k < 10) {
    // Always print at least two digits in the exponent.
    // This is for compatibility with printf("%g").
    *buf++ = '0';
    *buf++ = static_cast<char>('0' + k);
  } else if (k < 100) {
    *buf++ = static_cast<char>('0' + k / 10);
    k %= 10;
    *buf++ = static_cast<char>('0' + k);
  } else {
    *buf++ = static_cast<char>('0' + k / 100);
    k %= 100;
    *buf++ = static_cast<char>('0' + k / 10);
    k %= 10;
    *buf++ = static_cast<char>('0' + k);
  }

  return buf;
}

/*!
@brief prettify v = buf * 10^decimal_exponent
If v is in the range [10^min_exp, 10^max_exp) it will be printed in fixed-point
notation. Otherwise it will be printed in exponential notation.
@pre min_exp < 0
@pre max_exp > 0
*/
inline char *format_buffer(char *buf, int len, int decimal_exponent,
                           int min_exp, int max_exp) {

  const int k = len;
  const int n = len + decimal_exponent;

  // v = buf * 10^(n-k)
  // k is the length of the buffer (number of decimal digits)
  // n is the position of the decimal point relative to the start of the buffer.

  if (k <= n && n <= max_exp) {
    // digits[000]
    // len <= max_exp + 2

    std::memset(buf + k, '0', static_cast<size_t>(n) - static_cast<size_t>(k));
    // Make it look like a floating-point number (#362, #378)
    buf[n + 0] = '.';
    buf[n + 1] = '0';
    return buf + (static_cast<size_t>(n)) + 2;
  }

  if (0 < n && n <= max_exp) {
    // dig.its
    // len <= max_digits10 + 1
    std::memmove(buf + (static_cast<size_t>(n) + 1), buf + n,
                 static_cast<size_t>(k) - static_cast<size_t>(n));
    buf[n] = '.';
    return buf + (static_cast<size_t>(k) + 1U);
  }

  if (min_exp < n && n <= 0) {
    // 0.[000]digits
    // len <= 2 + (-min_exp - 1) + max_digits10

    std::memmove(buf + (2 + static_cast<size_t>(-n)), buf,
                 static_cast<size_t>(k));
    buf[0] = '0';
    buf[1] = '.';
    std::memset(buf + 2, '0', static_cast<size_t>(-n));
    return buf + (2U + static_cast<size_t>(-n) + static_cast<size_t>(k));
  }

  if (k == 1) {
    // dE+123
    // len <= 1 + 5

    buf += 1;
  } else {
    // d.igitsE+123
    // len <= max_digits10 + 1 + 5

    std::memmove(buf + 2, buf + 1, static_cast<size_t>(k) - 1);
    buf[1] = '.';
    buf += 1 + static_cast<size_t>(k);
  }

  *buf++ = 'e';
  return append_exponent(buf, n - 1);
}

} // namespace dtoa_impl

/*!
The format of the resulting decimal representation is similar to printf's %g
format. Returns an iterator pointing past-the-end of the decimal representation.
@note The input number must be finite, i.e. NaN's and Inf's are not supported.
@note The buffer must be large enough.
@note The result is NOT null-terminated.
*/
char *to_chars(char *first, const char *last, double value) {
  static_cast<void>(last); // maybe unused - fix warning
  bool negative = std::signbit(value);
  if (negative) {
    value = -value;
    *first++ = '-';
  }

  if (value == 0) // +-0
  {
    *first++ = '0';
    // Make it look like a floating-point number (#362, #378)
    *first++ = '.';
    *first++ = '0';
    return first;
  }
  // Compute v = buffer * 10^decimal_exponent.
  // The decimal digits are stored in the buffer, which needs to be interpreted
  // as an unsigned decimal integer.
  // len is the length of the buffer, i.e. the number of decimal digits.
  int len = 0;
  int decimal_exponent = 0;
  dtoa_impl::grisu2(first, len, decimal_exponent, value);
  // Format the buffer like printf("%.*g", prec, value)
  constexpr int kMinExp = -4;
  constexpr int kMaxExp = std::numeric_limits<double>::digits10;

  return dtoa_impl::format_buffer(first, len, decimal_exponent, kMinExp,
                                  kMaxExp);
}
} // namespace internal
} // namespace simdjson
/* end file src/to_chars.cpp */
/* begin file src/from_chars.cpp */
#include <limits>
namespace simdjson {
namespace internal {

/**
 * The code in the internal::from_chars function is meant to handle the floating-point number parsing
 * when we have more than 19 digits in the decimal mantissa. This should only be seen
 * in adversarial scenarios: we do not expect production systems to even produce
 * such floating-point numbers.
 *
 * The parser is based on work by Nigel Tao (at https://github.com/google/wuffs/)
 * who credits Ken Thompson for the design (via a reference to the Go source
 * code). See
 * https://github.com/google/wuffs/blob/aa46859ea40c72516deffa1b146121952d6dfd3b/internal/cgen/base/floatconv-submodule-data.c
 * https://github.com/google/wuffs/blob/46cd8105f47ca07ae2ba8e6a7818ef9c0df6c152/internal/cgen/base/floatconv-submodule-code.c
 * It is probably not very fast but it is a fallback that should almost never be
 * called in real life. Google Wuffs is published under APL 2.0.
 **/

namespace {
constexpr uint32_t max_digits = 768;
constexpr int32_t decimal_point_range = 2047;
} // namespace

struct adjusted_mantissa {
  uint64_t mantissa;
  int power2;
  adjusted_mantissa() : mantissa(0), power2(0) {}
};

struct decimal {
  uint32_t num_digits;
  int32_t decimal_point;
  bool negative;
  bool truncated;
  uint8_t digits[max_digits];
};

template <typename T> struct binary_format {
  static constexpr int mantissa_explicit_bits();
  static constexpr int minimum_exponent();
  static constexpr int infinite_power();
  static constexpr int sign_index();
};

template <> constexpr int binary_format<double>::mantissa_explicit_bits() {
  return 52;
}

template <> constexpr int binary_format<double>::minimum_exponent() {
  return -1023;
}
template <> constexpr int binary_format<double>::infinite_power() {
  return 0x7FF;
}

template <> constexpr int binary_format<double>::sign_index() { return 63; }

bool is_integer(char c)  noexcept  { return (c >= '0' && c <= '9'); }

// This should always succeed since it follows a call to parse_number.
decimal parse_decimal(const char *&p) noexcept {
  decimal answer;
  answer.num_digits = 0;
  answer.decimal_point = 0;
  answer.truncated = false;
  answer.negative = (*p == '-');
  if ((*p == '-') || (*p == '+')) {
    ++p;
  }

  while (*p == '0') {
    ++p;
  }
  while (is_integer(*p)) {
    if (answer.num_digits < max_digits) {
      answer.digits[answer.num_digits] = uint8_t(*p - '0');
    }
    answer.num_digits++;
    ++p;
  }
  if (*p == '.') {
    ++p;
    const char *first_after_period = p;
    // if we have not yet encountered a zero, we have to skip it as well
    if (answer.num_digits == 0) {
      // skip zeros
      while (*p == '0') {
        ++p;
      }
    }
    while (is_integer(*p)) {
      if (answer.num_digits < max_digits) {
        answer.digits[answer.num_digits] = uint8_t(*p - '0');
      }
      answer.num_digits++;
      ++p;
    }
    answer.decimal_point = int32_t(first_after_period - p);
  }
  if(answer.num_digits > 0) {
    const char *preverse = p - 1;
    int32_t trailing_zeros = 0;
    while ((*preverse == '0') || (*preverse == '.')) {
      if(*preverse == '0') { trailing_zeros++; };
      --preverse;
    }
    answer.decimal_point += int32_t(answer.num_digits);
    answer.num_digits -= uint32_t(trailing_zeros);
  }
  if(answer.num_digits > max_digits ) {
    answer.num_digits = max_digits;
    answer.truncated = true;
  }
  if (('e' == *p) || ('E' == *p)) {
    ++p;
    bool neg_exp = false;
    if ('-' == *p) {
      neg_exp = true;
      ++p;
    } else if ('+' == *p) {
      ++p;
    }
    int32_t exp_number = 0; // exponential part
    while (is_integer(*p)) {
      uint8_t digit = uint8_t(*p - '0');
      if (exp_number < 0x10000) {
        exp_number = 10 * exp_number + digit;
      }
      ++p;
    }
    answer.decimal_point += (neg_exp ? -exp_number : exp_number);
  }
  return answer;
}

// This should always succeed since it follows a call to parse_number.
// Will not read at or beyond the "end" pointer.
decimal parse_decimal(const char *&p, const char * end) noexcept {
  decimal answer;
  answer.num_digits = 0;
  answer.decimal_point = 0;
  answer.truncated = false;
  if(p == end) { return answer; } // should never happen
  answer.negative = (*p == '-');
  if ((*p == '-') || (*p == '+')) {
    ++p;
  }

  while ((p != end) && (*p == '0')) {
    ++p;
  }
  while ((p != end) && is_integer(*p)) {
    if (answer.num_digits < max_digits) {
      answer.digits[answer.num_digits] = uint8_t(*p - '0');
    }
    answer.num_digits++;
    ++p;
  }
  if ((p != end) && (*p == '.')) {
    ++p;
    if(p == end) { return answer; } // should never happen
    const char *first_after_period = p;
    // if we have not yet encountered a zero, we have to skip it as well
    if (answer.num_digits == 0) {
      // skip zeros
      while (*p == '0') {
        ++p;
      }
    }
    while ((p != end) && is_integer(*p)) {
      if (answer.num_digits < max_digits) {
        answer.digits[answer.num_digits] = uint8_t(*p - '0');
      }
      answer.num_digits++;
      ++p;
    }
    answer.decimal_point = int32_t(first_after_period - p);
  }
  if(answer.num_digits > 0) {
    const char *preverse = p - 1;
    int32_t trailing_zeros = 0;
    while ((*preverse == '0') || (*preverse == '.')) {
      if(*preverse == '0') { trailing_zeros++; };
      --preverse;
    }
    answer.decimal_point += int32_t(answer.num_digits);
    answer.num_digits -= uint32_t(trailing_zeros);
  }
  if(answer.num_digits > max_digits ) {
    answer.num_digits = max_digits;
    answer.truncated = true;
  }
  if ((p != end) && (('e' == *p) || ('E' == *p))) {
    ++p;
    if(p == end) { return answer; } // should never happen
    bool neg_exp = false;
    if ('-' == *p) {
      neg_exp = true;
      ++p;
    } else if ('+' == *p) {
      ++p;
    }
    int32_t exp_number = 0; // exponential part
    while ((p != end) && is_integer(*p)) {
      uint8_t digit = uint8_t(*p - '0');
      if (exp_number < 0x10000) {
        exp_number = 10 * exp_number + digit;
      }
      ++p;
    }
    answer.decimal_point += (neg_exp ? -exp_number : exp_number);
  }
  return answer;
}

namespace {

// remove all final zeroes
inline void trim(decimal &h) {
  while ((h.num_digits > 0) && (h.digits[h.num_digits - 1] == 0)) {
    h.num_digits--;
  }
}

uint32_t number_of_digits_decimal_left_shift(decimal &h, uint32_t shift) {
  shift &= 63;
  const static uint16_t number_of_digits_decimal_left_shift_table[65] = {
      0x0000, 0x0800, 0x0801, 0x0803, 0x1006, 0x1009, 0x100D, 0x1812, 0x1817,
      0x181D, 0x2024, 0x202B, 0x2033, 0x203C, 0x2846, 0x2850, 0x285B, 0x3067,
      0x3073, 0x3080, 0x388E, 0x389C, 0x38AB, 0x38BB, 0x40CC, 0x40DD, 0x40EF,
      0x4902, 0x4915, 0x4929, 0x513E, 0x5153, 0x5169, 0x5180, 0x5998, 0x59B0,
      0x59C9, 0x61E3, 0x61FD, 0x6218, 0x6A34, 0x6A50, 0x6A6D, 0x6A8B, 0x72AA,
      0x72C9, 0x72E9, 0x7B0A, 0x7B2B, 0x7B4D, 0x8370, 0x8393, 0x83B7, 0x83DC,
      0x8C02, 0x8C28, 0x8C4F, 0x9477, 0x949F, 0x94C8, 0x9CF2, 0x051C, 0x051C,
      0x051C, 0x051C,
  };
  uint32_t x_a = number_of_digits_decimal_left_shift_table[shift];
  uint32_t x_b = number_of_digits_decimal_left_shift_table[shift + 1];
  uint32_t num_new_digits = x_a >> 11;
  uint32_t pow5_a = 0x7FF & x_a;
  uint32_t pow5_b = 0x7FF & x_b;
  const static uint8_t
      number_of_digits_decimal_left_shift_table_powers_of_5[0x051C] = {
          5, 2, 5, 1, 2, 5, 6, 2, 5, 3, 1, 2, 5, 1, 5, 6, 2, 5, 7, 8, 1, 2, 5,
          3, 9, 0, 6, 2, 5, 1, 9, 5, 3, 1, 2, 5, 9, 7, 6, 5, 6, 2, 5, 4, 8, 8,
          2, 8, 1, 2, 5, 2, 4, 4, 1, 4, 0, 6, 2, 5, 1, 2, 2, 0, 7, 0, 3, 1, 2,
          5, 6, 1, 0, 3, 5, 1, 5, 6, 2, 5, 3, 0, 5, 1, 7, 5, 7, 8, 1, 2, 5, 1,
          5, 2, 5, 8, 7, 8, 9, 0, 6, 2, 5, 7, 6, 2, 9, 3, 9, 4, 5, 3, 1, 2, 5,
          3, 8, 1, 4, 6, 9, 7, 2, 6, 5, 6, 2, 5, 1, 9, 0, 7, 3, 4, 8, 6, 3, 2,
          8, 1, 2, 5, 9, 5, 3, 6, 7, 4, 3, 1, 6, 4, 0, 6, 2, 5, 4, 7, 6, 8, 3,
          7, 1, 5, 8, 2, 0, 3, 1, 2, 5, 2, 3, 8, 4, 1, 8, 5, 7, 9, 1, 0, 1, 5,
          6, 2, 5, 1, 1, 9, 2, 0, 9, 2, 8, 9, 5, 5, 0, 7, 8, 1, 2, 5, 5, 9, 6,
          0, 4, 6, 4, 4, 7, 7, 5, 3, 9, 0, 6, 2, 5, 2, 9, 8, 0, 2, 3, 2, 2, 3,
          8, 7, 6, 9, 5, 3, 1, 2, 5, 1, 4, 9, 0, 1, 1, 6, 1, 1, 9, 3, 8, 4, 7,
          6, 5, 6, 2, 5, 7, 4, 5, 0, 5, 8, 0, 5, 9, 6, 9, 2, 3, 8, 2, 8, 1, 2,
          5, 3, 7, 2, 5, 2, 9, 0, 2, 9, 8, 4, 6, 1, 9, 1, 4, 0, 6, 2, 5, 1, 8,
          6, 2, 6, 4, 5, 1, 4, 9, 2, 3, 0, 9, 5, 7, 0, 3, 1, 2, 5, 9, 3, 1, 3,
          2, 2, 5, 7, 4, 6, 1, 5, 4, 7, 8, 5, 1, 5, 6, 2, 5, 4, 6, 5, 6, 6, 1,
          2, 8, 7, 3, 0, 7, 7, 3, 9, 2, 5, 7, 8, 1, 2, 5, 2, 3, 2, 8, 3, 0, 6,
          4, 3, 6, 5, 3, 8, 6, 9, 6, 2, 8, 9, 0, 6, 2, 5, 1, 1, 6, 4, 1, 5, 3,
          2, 1, 8, 2, 6, 9, 3, 4, 8, 1, 4, 4, 5, 3, 1, 2, 5, 5, 8, 2, 0, 7, 6,
          6, 0, 9, 1, 3, 4, 6, 7, 4, 0, 7, 2, 2, 6, 5, 6, 2, 5, 2, 9, 1, 0, 3,
          8, 3, 0, 4, 5, 6, 7, 3, 3, 7, 0, 3, 6, 1, 3, 2, 8, 1, 2, 5, 1, 4, 5,
          5, 1, 9, 1, 5, 2, 2, 8, 3, 6, 6, 8, 5, 1, 8, 0, 6, 6, 4, 0, 6, 2, 5,
          7, 2, 7, 5, 9, 5, 7, 6, 1, 4, 1, 8, 3, 4, 2, 5, 9, 0, 3, 3, 2, 0, 3,
          1, 2, 5, 3, 6, 3, 7, 9, 7, 8, 8, 0, 7, 0, 9, 1, 7, 1, 2, 9, 5, 1, 6,
          6, 0, 1, 5, 6, 2, 5, 1, 8, 1, 8, 9, 8, 9, 4, 0, 3, 5, 4, 5, 8, 5, 6,
          4, 7, 5, 8, 3, 0, 0, 7, 8, 1, 2, 5, 9, 0, 9, 4, 9, 4, 7, 0, 1, 7, 7,
          2, 9, 2, 8, 2, 3, 7, 9, 1, 5, 0, 3, 9, 0, 6, 2, 5, 4, 5, 4, 7, 4, 7,
          3, 5, 0, 8, 8, 6, 4, 6, 4, 1, 1, 8, 9, 5, 7, 5, 1, 9, 5, 3, 1, 2, 5,
          2, 2, 7, 3, 7, 3, 6, 7, 5, 4, 4, 3, 2, 3, 2, 0, 5, 9, 4, 7, 8, 7, 5,
          9, 7, 6, 5, 6, 2, 5, 1, 1, 3, 6, 8, 6, 8, 3, 7, 7, 2, 1, 6, 1, 6, 0,
          2, 9, 7, 3, 9, 3, 7, 9, 8, 8, 2, 8, 1, 2, 5, 5, 6, 8, 4, 3, 4, 1, 8,
          8, 6, 0, 8, 0, 8, 0, 1, 4, 8, 6, 9, 6, 8, 9, 9, 4, 1, 4, 0, 6, 2, 5,
          2, 8, 4, 2, 1, 7, 0, 9, 4, 3, 0, 4, 0, 4, 0, 0, 7, 4, 3, 4, 8, 4, 4,
          9, 7, 0, 7, 0, 3, 1, 2, 5, 1, 4, 2, 1, 0, 8, 5, 4, 7, 1, 5, 2, 0, 2,
          0, 0, 3, 7, 1, 7, 4, 2, 2, 4, 8, 5, 3, 5, 1, 5, 6, 2, 5, 7, 1, 0, 5,
          4, 2, 7, 3, 5, 7, 6, 0, 1, 0, 0, 1, 8, 5, 8, 7, 1, 1, 2, 4, 2, 6, 7,
          5, 7, 8, 1, 2, 5, 3, 5, 5, 2, 7, 1, 3, 6, 7, 8, 8, 0, 0, 5, 0, 0, 9,
          2, 9, 3, 5, 5, 6, 2, 1, 3, 3, 7, 8, 9, 0, 6, 2, 5, 1, 7, 7, 6, 3, 5,
          6, 8, 3, 9, 4, 0, 0, 2, 5, 0, 4, 6, 4, 6, 7, 7, 8, 1, 0, 6, 6, 8, 9,
          4, 5, 3, 1, 2, 5, 8, 8, 8, 1, 7, 8, 4, 1, 9, 7, 0, 0, 1, 2, 5, 2, 3,
          2, 3, 3, 8, 9, 0, 5, 3, 3, 4, 4, 7, 2, 6, 5, 6, 2, 5, 4, 4, 4, 0, 8,
          9, 2, 0, 9, 8, 5, 0, 0, 6, 2, 6, 1, 6, 1, 6, 9, 4, 5, 2, 6, 6, 7, 2,
          3, 6, 3, 2, 8, 1, 2, 5, 2, 2, 2, 0, 4, 4, 6, 0, 4, 9, 2, 5, 0, 3, 1,
          3, 0, 8, 0, 8, 4, 7, 2, 6, 3, 3, 3, 6, 1, 8, 1, 6, 4, 0, 6, 2, 5, 1,
          1, 1, 0, 2, 2, 3, 0, 2, 4, 6, 2, 5, 1, 5, 6, 5, 4, 0, 4, 2, 3, 6, 3,
          1, 6, 6, 8, 0, 9, 0, 8, 2, 0, 3, 1, 2, 5, 5, 5, 5, 1, 1, 1, 5, 1, 2,
          3, 1, 2, 5, 7, 8, 2, 7, 0, 2, 1, 1, 8, 1, 5, 8, 3, 4, 0, 4, 5, 4, 1,
          0, 1, 5, 6, 2, 5, 2, 7, 7, 5, 5, 5, 7, 5, 6, 1, 5, 6, 2, 8, 9, 1, 3,
          5, 1, 0, 5, 9, 0, 7, 9, 1, 7, 0, 2, 2, 7, 0, 5, 0, 7, 8, 1, 2, 5, 1,
          3, 8, 7, 7, 7, 8, 7, 8, 0, 7, 8, 1, 4, 4, 5, 6, 7, 5, 5, 2, 9, 5, 3,
          9, 5, 8, 5, 1, 1, 3, 5, 2, 5, 3, 9, 0, 6, 2, 5, 6, 9, 3, 8, 8, 9, 3,
          9, 0, 3, 9, 0, 7, 2, 2, 8, 3, 7, 7, 6, 4, 7, 6, 9, 7, 9, 2, 5, 5, 6,
          7, 6, 2, 6, 9, 5, 3, 1, 2, 5, 3, 4, 6, 9, 4, 4, 6, 9, 5, 1, 9, 5, 3,
          6, 1, 4, 1, 8, 8, 8, 2, 3, 8, 4, 8, 9, 6, 2, 7, 8, 3, 8, 1, 3, 4, 7,
          6, 5, 6, 2, 5, 1, 7, 3, 4, 7, 2, 3, 4, 7, 5, 9, 7, 6, 8, 0, 7, 0, 9,
          4, 4, 1, 1, 9, 2, 4, 4, 8, 1, 3, 9, 1, 9, 0, 6, 7, 3, 8, 2, 8, 1, 2,
          5, 8, 6, 7, 3, 6, 1, 7, 3, 7, 9, 8, 8, 4, 0, 3, 5, 4, 7, 2, 0, 5, 9,
          6, 2, 2, 4, 0, 6, 9, 5, 9, 5, 3, 3, 6, 9, 1, 4, 0, 6, 2, 5,
      };
  const uint8_t *pow5 =
      &number_of_digits_decimal_left_shift_table_powers_of_5[pow5_a];
  uint32_t i = 0;
  uint32_t n = pow5_b - pow5_a;
  for (; i < n; i++) {
    if (i >= h.num_digits) {
      return num_new_digits - 1;
    } else if (h.digits[i] == pow5[i]) {
      continue;
    } else if (h.digits[i] < pow5[i]) {
      return num_new_digits - 1;
    } else {
      return num_new_digits;
    }
  }
  return num_new_digits;
}

} // end of anonymous namespace

uint64_t round(decimal &h) {
  if ((h.num_digits == 0) || (h.decimal_point < 0)) {
    return 0;
  } else if (h.decimal_point > 18) {
    return UINT64_MAX;
  }
  // at this point, we know that h.decimal_point >= 0
  uint32_t dp = uint32_t(h.decimal_point);
  uint64_t n = 0;
  for (uint32_t i = 0; i < dp; i++) {
    n = (10 * n) + ((i < h.num_digits) ? h.digits[i] : 0);
  }
  bool round_up = false;
  if (dp < h.num_digits) {
    round_up = h.digits[dp] >= 5; // normally, we round up
    // but we may need to round to even!
    if ((h.digits[dp] == 5) && (dp + 1 == h.num_digits)) {
      round_up = h.truncated || ((dp > 0) && (1 & h.digits[dp - 1]));
    }
  }
  if (round_up) {
    n++;
  }
  return n;
}

// computes h * 2^-shift
void decimal_left_shift(decimal &h, uint32_t shift) {
  if (h.num_digits == 0) {
    return;
  }
  uint32_t num_new_digits = number_of_digits_decimal_left_shift(h, shift);
  int32_t read_index = int32_t(h.num_digits - 1);
  uint32_t write_index = h.num_digits - 1 + num_new_digits;
  uint64_t n = 0;

  while (read_index >= 0) {
    n += uint64_t(h.digits[read_index]) << shift;
    uint64_t quotient = n / 10;
    uint64_t remainder = n - (10 * quotient);
    if (write_index < max_digits) {
      h.digits[write_index] = uint8_t(remainder);
    } else if (remainder > 0) {
      h.truncated = true;
    }
    n = quotient;
    write_index--;
    read_index--;
  }
  while (n > 0) {
    uint64_t quotient = n / 10;
    uint64_t remainder = n - (10 * quotient);
    if (write_index < max_digits) {
      h.digits[write_index] = uint8_t(remainder);
    } else if (remainder > 0) {
      h.truncated = true;
    }
    n = quotient;
    write_index--;
  }
  h.num_digits += num_new_digits;
  if (h.num_digits > max_digits) {
    h.num_digits = max_digits;
  }
  h.decimal_point += int32_t(num_new_digits);
  trim(h);
}

// computes h * 2^shift
void decimal_right_shift(decimal &h, uint32_t shift) {
  uint32_t read_index = 0;
  uint32_t write_index = 0;

  uint64_t n = 0;

  while ((n >> shift) == 0) {
    if (read_index < h.num_digits) {
      n = (10 * n) + h.digits[read_index++];
    } else if (n == 0) {
      return;
    } else {
      while ((n >> shift) == 0) {
        n = 10 * n;
        read_index++;
      }
      break;
    }
  }
  h.decimal_point -= int32_t(read_index - 1);
  if (h.decimal_point < -decimal_point_range) { // it is zero
    h.num_digits = 0;
    h.decimal_point = 0;
    h.negative = false;
    h.truncated = false;
    return;
  }
  uint64_t mask = (uint64_t(1) << shift) - 1;
  while (read_index < h.num_digits) {
    uint8_t new_digit = uint8_t(n >> shift);
    n = (10 * (n & mask)) + h.digits[read_index++];
    h.digits[write_index++] = new_digit;
  }
  while (n > 0) {
    uint8_t new_digit = uint8_t(n >> shift);
    n = 10 * (n & mask);
    if (write_index < max_digits) {
      h.digits[write_index++] = new_digit;
    } else if (new_digit > 0) {
      h.truncated = true;
    }
  }
  h.num_digits = write_index;
  trim(h);
}

template <typename binary> adjusted_mantissa compute_float(decimal &d) {
  adjusted_mantissa answer;
  if (d.num_digits == 0) {
    // should be zero
    answer.power2 = 0;
    answer.mantissa = 0;
    return answer;
  }
  // At this point, going further, we can assume that d.num_digits > 0.
  // We want to guard against excessive decimal point values because
  // they can result in long running times. Indeed, we do
  // shifts by at most 60 bits. We have that log(10**400)/log(2**60) ~= 22
  // which is fine, but log(10**299995)/log(2**60) ~= 16609 which is not
  // fine (runs for a long time).
  //
  if(d.decimal_point < -324) {
    // We have something smaller than 1e-324 which is always zero
    // in binary64 and binary32.
    // It should be zero.
    answer.power2 = 0;
    answer.mantissa = 0;
    return answer;
  } else if(d.decimal_point >= 310) {
    // We have something at least as large as 0.1e310 which is
    // always infinite.
    answer.power2 = binary::infinite_power();
    answer.mantissa = 0;
    return answer;
  }

  static const uint32_t max_shift = 60;
  static const uint32_t num_powers = 19;
  static const uint8_t powers[19] = {
      0,  3,  6,  9,  13, 16, 19, 23, 26, 29, //
      33, 36, 39, 43, 46, 49, 53, 56, 59,     //
  };
  int32_t exp2 = 0;
  while (d.decimal_point > 0) {
    uint32_t n = uint32_t(d.decimal_point);
    uint32_t shift = (n < num_powers) ? powers[n] : max_shift;
    decimal_right_shift(d, shift);
    if (d.decimal_point < -decimal_point_range) {
      // should be zero
      answer.power2 = 0;
      answer.mantissa = 0;
      return answer;
    }
    exp2 += int32_t(shift);
  }
  // We shift left toward [1/2 ... 1].
  while (d.decimal_point <= 0) {
    uint32_t shift;
    if (d.decimal_point == 0) {
      if (d.digits[0] >= 5) {
        break;
      }
      shift = (d.digits[0] < 2) ? 2 : 1;
    } else {
      uint32_t n = uint32_t(-d.decimal_point);
      shift = (n < num_powers) ? powers[n] : max_shift;
    }
    decimal_left_shift(d, shift);
    if (d.decimal_point > decimal_point_range) {
      // we want to get infinity:
      answer.power2 = 0xFF;
      answer.mantissa = 0;
      return answer;
    }
    exp2 -= int32_t(shift);
  }
  // We are now in the range [1/2 ... 1] but the binary format uses [1 ... 2].
  exp2--;
  constexpr int32_t minimum_exponent = binary::minimum_exponent();
  while ((minimum_exponent + 1) > exp2) {
    uint32_t n = uint32_t((minimum_exponent + 1) - exp2);
    if (n > max_shift) {
      n = max_shift;
    }
    decimal_right_shift(d, n);
    exp2 += int32_t(n);
  }
  if ((exp2 - minimum_exponent) >= binary::infinite_power()) {
    answer.power2 = binary::infinite_power();
    answer.mantissa = 0;
    return answer;
  }

  const int mantissa_size_in_bits = binary::mantissa_explicit_bits() + 1;
  decimal_left_shift(d, mantissa_size_in_bits);

  uint64_t mantissa = round(d);
  // It is possible that we have an overflow, in which case we need
  // to shift back.
  if (mantissa >= (uint64_t(1) << mantissa_size_in_bits)) {
    decimal_right_shift(d, 1);
    exp2 += 1;
    mantissa = round(d);
    if ((exp2 - minimum_exponent) >= binary::infinite_power()) {
      answer.power2 = binary::infinite_power();
      answer.mantissa = 0;
      return answer;
    }
  }
  answer.power2 = exp2 - binary::minimum_exponent();
  if (mantissa < (uint64_t(1) << binary::mantissa_explicit_bits())) {
    answer.power2--;
  }
  answer.mantissa =
      mantissa & ((uint64_t(1) << binary::mantissa_explicit_bits()) - 1);
  return answer;
}

template <typename binary>
adjusted_mantissa parse_long_mantissa(const char *first) {
  decimal d = parse_decimal(first);
  return compute_float<binary>(d);
}

template <typename binary>
adjusted_mantissa parse_long_mantissa(const char *first, const char *end) {
  decimal d = parse_decimal(first, end);
  return compute_float<binary>(d);
}

double from_chars(const char *first) noexcept {
  bool negative = first[0] == '-';
  if (negative) {
    first++;
  }
  adjusted_mantissa am = parse_long_mantissa<binary_format<double>>(first);
  uint64_t word = am.mantissa;
  word |= uint64_t(am.power2)
          << binary_format<double>::mantissa_explicit_bits();
  word = negative ? word | (uint64_t(1) << binary_format<double>::sign_index())
                  : word;
  double value;
  std::memcpy(&value, &word, sizeof(double));
  return value;
}


double from_chars(const char *first, const char *end) noexcept {
  bool negative = first[0] == '-';
  if (negative) {
    first++;
  }
  adjusted_mantissa am = parse_long_mantissa<binary_format<double>>(first, end);
  uint64_t word = am.mantissa;
  word |= uint64_t(am.power2)
          << binary_format<double>::mantissa_explicit_bits();
  word = negative ? word | (uint64_t(1) << binary_format<double>::sign_index())
                  : word;
  double value;
  std::memcpy(&value, &word, sizeof(double));
  return value;
}

} // internal
} // simdjson
/* end file src/from_chars.cpp */
/* begin file src/internal/error_tables.cpp */

namespace simdjson {
namespace internal {

  SIMDJSON_DLLIMPORTEXPORT const error_code_info error_codes[] {
    { SUCCESS, "SUCCESS: No error" },
    { CAPACITY, "CAPACITY: This parser can't support a document that big" },
    { MEMALLOC, "MEMALLOC: Error allocating memory, we're most likely out of memory" },
    { TAPE_ERROR, "TAPE_ERROR: The JSON document has an improper structure: missing or superfluous commas, braces, missing keys, etc." },
    { DEPTH_ERROR, "DEPTH_ERROR: The JSON document was too deep (too many nested objects and arrays)" },
    { STRING_ERROR, "STRING_ERROR: Problem while parsing a string" },
    { T_ATOM_ERROR, "T_ATOM_ERROR: Problem while parsing an atom starting with the letter 't'" },
    { F_ATOM_ERROR, "F_ATOM_ERROR: Problem while parsing an atom starting with the letter 'f'" },
    { N_ATOM_ERROR, "N_ATOM_ERROR: Problem while parsing an atom starting with the letter 'n'" },
    { NUMBER_ERROR, "NUMBER_ERROR: Problem while parsing a number" },
    { UTF8_ERROR, "UTF8_ERROR: The input is not valid UTF-8" },
    { UNINITIALIZED, "UNINITIALIZED: Uninitialized" },
    { EMPTY, "EMPTY: no JSON found" },
    { UNESCAPED_CHARS, "UNESCAPED_CHARS: Within strings, some characters must be escaped, we found unescaped characters" },
    { UNCLOSED_STRING, "UNCLOSED_STRING: A string is opened, but never closed." },
    { UNSUPPORTED_ARCHITECTURE, "UNSUPPORTED_ARCHITECTURE: simdjson does not have an implementation supported by this CPU architecture. Please report this error to the core team as it should never happen." },
    { INCORRECT_TYPE, "INCORRECT_TYPE: The JSON element does not have the requested type." },
    { NUMBER_OUT_OF_RANGE, "NUMBER_OUT_OF_RANGE: The JSON number is too large or too small to fit within the requested type." },
    { INDEX_OUT_OF_BOUNDS, "INDEX_OUT_OF_BOUNDS: Attempted to access an element of a JSON array that is beyond its length." },
    { NO_SUCH_FIELD, "NO_SUCH_FIELD: The JSON field referenced does not exist in this object." },
    { IO_ERROR, "IO_ERROR: Error reading the file." },
    { INVALID_JSON_POINTER, "INVALID_JSON_POINTER: Invalid JSON pointer syntax." },
    { INVALID_URI_FRAGMENT, "INVALID_URI_FRAGMENT: Invalid URI fragment syntax." },
    { UNEXPECTED_ERROR, "UNEXPECTED_ERROR: Unexpected error, consider reporting this problem as you may have found a bug in simdjson" },
    { PARSER_IN_USE, "PARSER_IN_USE: Cannot parse a new document while a document is still in use." },
    { OUT_OF_ORDER_ITERATION, "OUT_OF_ORDER_ITERATION: Objects and arrays can only be iterated when they are first encountered." },
    { INSUFFICIENT_PADDING, "INSUFFICIENT_PADDING: simdjson requires the input JSON string to have at least SIMDJSON_PADDING extra bytes allocated, beyond the string's length. Consider using the simdjson::padded_string class if needed." },
    { INCOMPLETE_ARRAY_OR_OBJECT, "INCOMPLETE_ARRAY_OR_OBJECT: JSON document ended early in the middle of an object or array." },
    { SCALAR_DOCUMENT_AS_VALUE, "SCALAR_DOCUMENT_AS_VALUE: A JSON document made of a scalar (number, Boolean, null or string) is treated as a value. Use get_bool(), get_double(), etc. on the document instead. "},
    { OUT_OF_BOUNDS, "OUT_OF_BOUNDS: Attempt to access location outside of document."},
    { TRAILING_CONTENT, "TRAILING_CONTENT: Unexpected trailing content in the JSON input."}
  }; // error_messages[]

} // namespace internal
} // namespace simdjson
/* end file src/internal/error_tables.cpp */
/* begin file src/internal/jsoncharutils_tables.cpp */

namespace simdjson {
namespace internal {

// structural chars here are
// they are { 0x7b } 0x7d : 0x3a [ 0x5b ] 0x5d , 0x2c (and NULL)
// we are also interested in the four whitespace characters
// space 0x20, linefeed 0x0a, horizontal tab 0x09 and carriage return 0x0d

SIMDJSON_DLLIMPORTEXPORT const bool structural_or_whitespace_negated[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

SIMDJSON_DLLIMPORTEXPORT const bool structural_or_whitespace[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

SIMDJSON_DLLIMPORTEXPORT const uint32_t digit_to_val32[886] = {
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x0,        0x1,        0x2,        0x3,        0x4,        0x5,
    0x6,        0x7,        0x8,        0x9,        0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xa,
    0xb,        0xc,        0xd,        0xe,        0xf,        0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xa,        0xb,        0xc,        0xd,        0xe,
    0xf,        0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x0,        0x10,       0x20,       0x30,       0x40,       0x50,
    0x60,       0x70,       0x80,       0x90,       0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xa0,
    0xb0,       0xc0,       0xd0,       0xe0,       0xf0,       0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xa0,       0xb0,       0xc0,       0xd0,       0xe0,
    0xf0,       0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x0,        0x100,      0x200,      0x300,      0x400,      0x500,
    0x600,      0x700,      0x800,      0x900,      0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xa00,
    0xb00,      0xc00,      0xd00,      0xe00,      0xf00,      0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xa00,      0xb00,      0xc00,      0xd00,      0xe00,
    0xf00,      0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0x0,        0x1000,     0x2000,     0x3000,     0x4000,     0x5000,
    0x6000,     0x7000,     0x8000,     0x9000,     0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xa000,
    0xb000,     0xc000,     0xd000,     0xe000,     0xf000,     0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xa000,     0xb000,     0xc000,     0xd000,     0xe000,
    0xf000,     0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

} // namespace internal
} // namespace simdjson
/* end file src/internal/jsoncharutils_tables.cpp */
/* begin file src/internal/numberparsing_tables.cpp */

namespace simdjson {
namespace internal {

// Precomputed powers of ten from 10^0 to 10^22. These
// can be represented exactly using the double type.
SIMDJSON_DLLIMPORTEXPORT const double power_of_ten[] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

/**
 * When mapping numbers from decimal to binary,
 * we go from w * 10^q to m * 2^p but we have
 * 10^q = 5^q * 2^q, so effectively
 * we are trying to match
 * w * 2^q * 5^q to m * 2^p. Thus the powers of two
 * are not a concern since they can be represented
 * exactly using the binary notation, only the powers of five
 * affect the binary significand.
 */


// The truncated powers of five from 5^-342 all the way to 5^308
// The mantissa is truncated to 128 bits, and
// never rounded up. Uses about 10KB.
SIMDJSON_DLLIMPORTEXPORT const uint64_t power_of_five_128[]= {
        0xeef453d6923bd65a,0x113faa2906a13b3f,
        0x9558b4661b6565f8,0x4ac7ca59a424c507,
        0xbaaee17fa23ebf76,0x5d79bcf00d2df649,
        0xe95a99df8ace6f53,0xf4d82c2c107973dc,
        0x91d8a02bb6c10594,0x79071b9b8a4be869,
        0xb64ec836a47146f9,0x9748e2826cdee284,
        0xe3e27a444d8d98b7,0xfd1b1b2308169b25,
        0x8e6d8c6ab0787f72,0xfe30f0f5e50e20f7,
        0xb208ef855c969f4f,0xbdbd2d335e51a935,
        0xde8b2b66b3bc4723,0xad2c788035e61382,
        0x8b16fb203055ac76,0x4c3bcb5021afcc31,
        0xaddcb9e83c6b1793,0xdf4abe242a1bbf3d,
        0xd953e8624b85dd78,0xd71d6dad34a2af0d,
        0x87d4713d6f33aa6b,0x8672648c40e5ad68,
        0xa9c98d8ccb009506,0x680efdaf511f18c2,
        0xd43bf0effdc0ba48,0x212bd1b2566def2,
        0x84a57695fe98746d,0x14bb630f7604b57,
        0xa5ced43b7e3e9188,0x419ea3bd35385e2d,
        0xcf42894a5dce35ea,0x52064cac828675b9,
        0x818995ce7aa0e1b2,0x7343efebd1940993,
        0xa1ebfb4219491a1f,0x1014ebe6c5f90bf8,
        0xca66fa129f9b60a6,0xd41a26e077774ef6,
        0xfd00b897478238d0,0x8920b098955522b4,
        0x9e20735e8cb16382,0x55b46e5f5d5535b0,
        0xc5a890362fddbc62,0xeb2189f734aa831d,
        0xf712b443bbd52b7b,0xa5e9ec7501d523e4,
        0x9a6bb0aa55653b2d,0x47b233c92125366e,
        0xc1069cd4eabe89f8,0x999ec0bb696e840a,
        0xf148440a256e2c76,0xc00670ea43ca250d,
        0x96cd2a865764dbca,0x380406926a5e5728,
        0xbc807527ed3e12bc,0xc605083704f5ecf2,
        0xeba09271e88d976b,0xf7864a44c633682e,
        0x93445b8731587ea3,0x7ab3ee6afbe0211d,
        0xb8157268fdae9e4c,0x5960ea05bad82964,
        0xe61acf033d1a45df,0x6fb92487298e33bd,
        0x8fd0c16206306bab,0xa5d3b6d479f8e056,
        0xb3c4f1ba87bc8696,0x8f48a4899877186c,
        0xe0b62e2929aba83c,0x331acdabfe94de87,
        0x8c71dcd9ba0b4925,0x9ff0c08b7f1d0b14,
        0xaf8e5410288e1b6f,0x7ecf0ae5ee44dd9,
        0xdb71e91432b1a24a,0xc9e82cd9f69d6150,
        0x892731ac9faf056e,0xbe311c083a225cd2,
        0xab70fe17c79ac6ca,0x6dbd630a48aaf406,
        0xd64d3d9db981787d,0x92cbbccdad5b108,
        0x85f0468293f0eb4e,0x25bbf56008c58ea5,
        0xa76c582338ed2621,0xaf2af2b80af6f24e,
        0xd1476e2c07286faa,0x1af5af660db4aee1,
        0x82cca4db847945ca,0x50d98d9fc890ed4d,
        0xa37fce126597973c,0xe50ff107bab528a0,
        0xcc5fc196fefd7d0c,0x1e53ed49a96272c8,
        0xff77b1fcbebcdc4f,0x25e8e89c13bb0f7a,
        0x9faacf3df73609b1,0x77b191618c54e9ac,
        0xc795830d75038c1d,0xd59df5b9ef6a2417,
        0xf97ae3d0d2446f25,0x4b0573286b44ad1d,
        0x9becce62836ac577,0x4ee367f9430aec32,
        0xc2e801fb244576d5,0x229c41f793cda73f,
        0xf3a20279ed56d48a,0x6b43527578c1110f,
        0x9845418c345644d6,0x830a13896b78aaa9,
        0xbe5691ef416bd60c,0x23cc986bc656d553,
        0xedec366b11c6cb8f,0x2cbfbe86b7ec8aa8,
        0x94b3a202eb1c3f39,0x7bf7d71432f3d6a9,
        0xb9e08a83a5e34f07,0xdaf5ccd93fb0cc53,
        0xe858ad248f5c22c9,0xd1b3400f8f9cff68,
        0x91376c36d99995be,0x23100809b9c21fa1,
        0xb58547448ffffb2d,0xabd40a0c2832a78a,
        0xe2e69915b3fff9f9,0x16c90c8f323f516c,
        0x8dd01fad907ffc3b,0xae3da7d97f6792e3,
        0xb1442798f49ffb4a,0x99cd11cfdf41779c,
        0xdd95317f31c7fa1d,0x40405643d711d583,
        0x8a7d3eef7f1cfc52,0x482835ea666b2572,
        0xad1c8eab5ee43b66,0xda3243650005eecf,
        0xd863b256369d4a40,0x90bed43e40076a82,
        0x873e4f75e2224e68,0x5a7744a6e804a291,
        0xa90de3535aaae202,0x711515d0a205cb36,
        0xd3515c2831559a83,0xd5a5b44ca873e03,
        0x8412d9991ed58091,0xe858790afe9486c2,
        0xa5178fff668ae0b6,0x626e974dbe39a872,
        0xce5d73ff402d98e3,0xfb0a3d212dc8128f,
        0x80fa687f881c7f8e,0x7ce66634bc9d0b99,
        0xa139029f6a239f72,0x1c1fffc1ebc44e80,
        0xc987434744ac874e,0xa327ffb266b56220,
        0xfbe9141915d7a922,0x4bf1ff9f0062baa8,
        0x9d71ac8fada6c9b5,0x6f773fc3603db4a9,
        0xc4ce17b399107c22,0xcb550fb4384d21d3,
        0xf6019da07f549b2b,0x7e2a53a146606a48,
        0x99c102844f94e0fb,0x2eda7444cbfc426d,
        0xc0314325637a1939,0xfa911155fefb5308,
        0xf03d93eebc589f88,0x793555ab7eba27ca,
        0x96267c7535b763b5,0x4bc1558b2f3458de,
        0xbbb01b9283253ca2,0x9eb1aaedfb016f16,
        0xea9c227723ee8bcb,0x465e15a979c1cadc,
        0x92a1958a7675175f,0xbfacd89ec191ec9,
        0xb749faed14125d36,0xcef980ec671f667b,
        0xe51c79a85916f484,0x82b7e12780e7401a,
        0x8f31cc0937ae58d2,0xd1b2ecb8b0908810,
        0xb2fe3f0b8599ef07,0x861fa7e6dcb4aa15,
        0xdfbdcece67006ac9,0x67a791e093e1d49a,
        0x8bd6a141006042bd,0xe0c8bb2c5c6d24e0,
        0xaecc49914078536d,0x58fae9f773886e18,
        0xda7f5bf590966848,0xaf39a475506a899e,
        0x888f99797a5e012d,0x6d8406c952429603,
        0xaab37fd7d8f58178,0xc8e5087ba6d33b83,
        0xd5605fcdcf32e1d6,0xfb1e4a9a90880a64,
        0x855c3be0a17fcd26,0x5cf2eea09a55067f,
        0xa6b34ad8c9dfc06f,0xf42faa48c0ea481e,
        0xd0601d8efc57b08b,0xf13b94daf124da26,
        0x823c12795db6ce57,0x76c53d08d6b70858,
        0xa2cb1717b52481ed,0x54768c4b0c64ca6e,
        0xcb7ddcdda26da268,0xa9942f5dcf7dfd09,
        0xfe5d54150b090b02,0xd3f93b35435d7c4c,
        0x9efa548d26e5a6e1,0xc47bc5014a1a6daf,
        0xc6b8e9b0709f109a,0x359ab6419ca1091b,
        0xf867241c8cc6d4c0,0xc30163d203c94b62,
        0x9b407691d7fc44f8,0x79e0de63425dcf1d,
        0xc21094364dfb5636,0x985915fc12f542e4,
        0xf294b943e17a2bc4,0x3e6f5b7b17b2939d,
        0x979cf3ca6cec5b5a,0xa705992ceecf9c42,
        0xbd8430bd08277231,0x50c6ff782a838353,
        0xece53cec4a314ebd,0xa4f8bf5635246428,
        0x940f4613ae5ed136,0x871b7795e136be99,
        0xb913179899f68584,0x28e2557b59846e3f,
        0xe757dd7ec07426e5,0x331aeada2fe589cf,
        0x9096ea6f3848984f,0x3ff0d2c85def7621,
        0xb4bca50b065abe63,0xfed077a756b53a9,
        0xe1ebce4dc7f16dfb,0xd3e8495912c62894,
        0x8d3360f09cf6e4bd,0x64712dd7abbbd95c,
        0xb080392cc4349dec,0xbd8d794d96aacfb3,
        0xdca04777f541c567,0xecf0d7a0fc5583a0,
        0x89e42caaf9491b60,0xf41686c49db57244,
        0xac5d37d5b79b6239,0x311c2875c522ced5,
        0xd77485cb25823ac7,0x7d633293366b828b,
        0x86a8d39ef77164bc,0xae5dff9c02033197,
        0xa8530886b54dbdeb,0xd9f57f830283fdfc,
        0xd267caa862a12d66,0xd072df63c324fd7b,
        0x8380dea93da4bc60,0x4247cb9e59f71e6d,
        0xa46116538d0deb78,0x52d9be85f074e608,
        0xcd795be870516656,0x67902e276c921f8b,
        0x806bd9714632dff6,0xba1cd8a3db53b6,
        0xa086cfcd97bf97f3,0x80e8a40eccd228a4,
        0xc8a883c0fdaf7df0,0x6122cd128006b2cd,
        0xfad2a4b13d1b5d6c,0x796b805720085f81,
        0x9cc3a6eec6311a63,0xcbe3303674053bb0,
        0xc3f490aa77bd60fc,0xbedbfc4411068a9c,
        0xf4f1b4d515acb93b,0xee92fb5515482d44,
        0x991711052d8bf3c5,0x751bdd152d4d1c4a,
        0xbf5cd54678eef0b6,0xd262d45a78a0635d,
        0xef340a98172aace4,0x86fb897116c87c34,
        0x9580869f0e7aac0e,0xd45d35e6ae3d4da0,
        0xbae0a846d2195712,0x8974836059cca109,
        0xe998d258869facd7,0x2bd1a438703fc94b,
        0x91ff83775423cc06,0x7b6306a34627ddcf,
        0xb67f6455292cbf08,0x1a3bc84c17b1d542,
        0xe41f3d6a7377eeca,0x20caba5f1d9e4a93,
        0x8e938662882af53e,0x547eb47b7282ee9c,
        0xb23867fb2a35b28d,0xe99e619a4f23aa43,
        0xdec681f9f4c31f31,0x6405fa00e2ec94d4,
        0x8b3c113c38f9f37e,0xde83bc408dd3dd04,
        0xae0b158b4738705e,0x9624ab50b148d445,
        0xd98ddaee19068c76,0x3badd624dd9b0957,
        0x87f8a8d4cfa417c9,0xe54ca5d70a80e5d6,
        0xa9f6d30a038d1dbc,0x5e9fcf4ccd211f4c,
        0xd47487cc8470652b,0x7647c3200069671f,
        0x84c8d4dfd2c63f3b,0x29ecd9f40041e073,
        0xa5fb0a17c777cf09,0xf468107100525890,
        0xcf79cc9db955c2cc,0x7182148d4066eeb4,
        0x81ac1fe293d599bf,0xc6f14cd848405530,
        0xa21727db38cb002f,0xb8ada00e5a506a7c,
        0xca9cf1d206fdc03b,0xa6d90811f0e4851c,
        0xfd442e4688bd304a,0x908f4a166d1da663,
        0x9e4a9cec15763e2e,0x9a598e4e043287fe,
        0xc5dd44271ad3cdba,0x40eff1e1853f29fd,
        0xf7549530e188c128,0xd12bee59e68ef47c,
        0x9a94dd3e8cf578b9,0x82bb74f8301958ce,
        0xc13a148e3032d6e7,0xe36a52363c1faf01,
        0xf18899b1bc3f8ca1,0xdc44e6c3cb279ac1,
        0x96f5600f15a7b7e5,0x29ab103a5ef8c0b9,
        0xbcb2b812db11a5de,0x7415d448f6b6f0e7,
        0xebdf661791d60f56,0x111b495b3464ad21,
        0x936b9fcebb25c995,0xcab10dd900beec34,
        0xb84687c269ef3bfb,0x3d5d514f40eea742,
        0xe65829b3046b0afa,0xcb4a5a3112a5112,
        0x8ff71a0fe2c2e6dc,0x47f0e785eaba72ab,
        0xb3f4e093db73a093,0x59ed216765690f56,
        0xe0f218b8d25088b8,0x306869c13ec3532c,
        0x8c974f7383725573,0x1e414218c73a13fb,
        0xafbd2350644eeacf,0xe5d1929ef90898fa,
        0xdbac6c247d62a583,0xdf45f746b74abf39,
        0x894bc396ce5da772,0x6b8bba8c328eb783,
        0xab9eb47c81f5114f,0x66ea92f3f326564,
        0xd686619ba27255a2,0xc80a537b0efefebd,
        0x8613fd0145877585,0xbd06742ce95f5f36,
        0xa798fc4196e952e7,0x2c48113823b73704,
        0xd17f3b51fca3a7a0,0xf75a15862ca504c5,
        0x82ef85133de648c4,0x9a984d73dbe722fb,
        0xa3ab66580d5fdaf5,0xc13e60d0d2e0ebba,
        0xcc963fee10b7d1b3,0x318df905079926a8,
        0xffbbcfe994e5c61f,0xfdf17746497f7052,
        0x9fd561f1fd0f9bd3,0xfeb6ea8bedefa633,
        0xc7caba6e7c5382c8,0xfe64a52ee96b8fc0,
        0xf9bd690a1b68637b,0x3dfdce7aa3c673b0,
        0x9c1661a651213e2d,0x6bea10ca65c084e,
        0xc31bfa0fe5698db8,0x486e494fcff30a62,
        0xf3e2f893dec3f126,0x5a89dba3c3efccfa,
        0x986ddb5c6b3a76b7,0xf89629465a75e01c,
        0xbe89523386091465,0xf6bbb397f1135823,
        0xee2ba6c0678b597f,0x746aa07ded582e2c,
        0x94db483840b717ef,0xa8c2a44eb4571cdc,
        0xba121a4650e4ddeb,0x92f34d62616ce413,
        0xe896a0d7e51e1566,0x77b020baf9c81d17,
        0x915e2486ef32cd60,0xace1474dc1d122e,
        0xb5b5ada8aaff80b8,0xd819992132456ba,
        0xe3231912d5bf60e6,0x10e1fff697ed6c69,
        0x8df5efabc5979c8f,0xca8d3ffa1ef463c1,
        0xb1736b96b6fd83b3,0xbd308ff8a6b17cb2,
        0xddd0467c64bce4a0,0xac7cb3f6d05ddbde,
        0x8aa22c0dbef60ee4,0x6bcdf07a423aa96b,
        0xad4ab7112eb3929d,0x86c16c98d2c953c6,
        0xd89d64d57a607744,0xe871c7bf077ba8b7,
        0x87625f056c7c4a8b,0x11471cd764ad4972,
        0xa93af6c6c79b5d2d,0xd598e40d3dd89bcf,
        0xd389b47879823479,0x4aff1d108d4ec2c3,
        0x843610cb4bf160cb,0xcedf722a585139ba,
        0xa54394fe1eedb8fe,0xc2974eb4ee658828,
        0xce947a3da6a9273e,0x733d226229feea32,
        0x811ccc668829b887,0x806357d5a3f525f,
        0xa163ff802a3426a8,0xca07c2dcb0cf26f7,
        0xc9bcff6034c13052,0xfc89b393dd02f0b5,
        0xfc2c3f3841f17c67,0xbbac2078d443ace2,
        0x9d9ba7832936edc0,0xd54b944b84aa4c0d,
        0xc5029163f384a931,0xa9e795e65d4df11,
        0xf64335bcf065d37d,0x4d4617b5ff4a16d5,
        0x99ea0196163fa42e,0x504bced1bf8e4e45,
        0xc06481fb9bcf8d39,0xe45ec2862f71e1d6,
        0xf07da27a82c37088,0x5d767327bb4e5a4c,
        0x964e858c91ba2655,0x3a6a07f8d510f86f,
        0xbbe226efb628afea,0x890489f70a55368b,
        0xeadab0aba3b2dbe5,0x2b45ac74ccea842e,
        0x92c8ae6b464fc96f,0x3b0b8bc90012929d,
        0xb77ada0617e3bbcb,0x9ce6ebb40173744,
        0xe55990879ddcaabd,0xcc420a6a101d0515,
        0x8f57fa54c2a9eab6,0x9fa946824a12232d,
        0xb32df8e9f3546564,0x47939822dc96abf9,
        0xdff9772470297ebd,0x59787e2b93bc56f7,
        0x8bfbea76c619ef36,0x57eb4edb3c55b65a,
        0xaefae51477a06b03,0xede622920b6b23f1,
        0xdab99e59958885c4,0xe95fab368e45eced,
        0x88b402f7fd75539b,0x11dbcb0218ebb414,
        0xaae103b5fcd2a881,0xd652bdc29f26a119,
        0xd59944a37c0752a2,0x4be76d3346f0495f,
        0x857fcae62d8493a5,0x6f70a4400c562ddb,
        0xa6dfbd9fb8e5b88e,0xcb4ccd500f6bb952,
        0xd097ad07a71f26b2,0x7e2000a41346a7a7,
        0x825ecc24c873782f,0x8ed400668c0c28c8,
        0xa2f67f2dfa90563b,0x728900802f0f32fa,
        0xcbb41ef979346bca,0x4f2b40a03ad2ffb9,
        0xfea126b7d78186bc,0xe2f610c84987bfa8,
        0x9f24b832e6b0f436,0xdd9ca7d2df4d7c9,
        0xc6ede63fa05d3143,0x91503d1c79720dbb,
        0xf8a95fcf88747d94,0x75a44c6397ce912a,
        0x9b69dbe1b548ce7c,0xc986afbe3ee11aba,
        0xc24452da229b021b,0xfbe85badce996168,
        0xf2d56790ab41c2a2,0xfae27299423fb9c3,
        0x97c560ba6b0919a5,0xdccd879fc967d41a,
        0xbdb6b8e905cb600f,0x5400e987bbc1c920,
        0xed246723473e3813,0x290123e9aab23b68,
        0x9436c0760c86e30b,0xf9a0b6720aaf6521,
        0xb94470938fa89bce,0xf808e40e8d5b3e69,
        0xe7958cb87392c2c2,0xb60b1d1230b20e04,
        0x90bd77f3483bb9b9,0xb1c6f22b5e6f48c2,
        0xb4ecd5f01a4aa828,0x1e38aeb6360b1af3,
        0xe2280b6c20dd5232,0x25c6da63c38de1b0,
        0x8d590723948a535f,0x579c487e5a38ad0e,
        0xb0af48ec79ace837,0x2d835a9df0c6d851,
        0xdcdb1b2798182244,0xf8e431456cf88e65,
        0x8a08f0f8bf0f156b,0x1b8e9ecb641b58ff,
        0xac8b2d36eed2dac5,0xe272467e3d222f3f,
        0xd7adf884aa879177,0x5b0ed81dcc6abb0f,
        0x86ccbb52ea94baea,0x98e947129fc2b4e9,
        0xa87fea27a539e9a5,0x3f2398d747b36224,
        0xd29fe4b18e88640e,0x8eec7f0d19a03aad,
        0x83a3eeeef9153e89,0x1953cf68300424ac,
        0xa48ceaaab75a8e2b,0x5fa8c3423c052dd7,
        0xcdb02555653131b6,0x3792f412cb06794d,
        0x808e17555f3ebf11,0xe2bbd88bbee40bd0,
        0xa0b19d2ab70e6ed6,0x5b6aceaeae9d0ec4,
        0xc8de047564d20a8b,0xf245825a5a445275,
        0xfb158592be068d2e,0xeed6e2f0f0d56712,
        0x9ced737bb6c4183d,0x55464dd69685606b,
        0xc428d05aa4751e4c,0xaa97e14c3c26b886,
        0xf53304714d9265df,0xd53dd99f4b3066a8,
        0x993fe2c6d07b7fab,0xe546a8038efe4029,
        0xbf8fdb78849a5f96,0xde98520472bdd033,
        0xef73d256a5c0f77c,0x963e66858f6d4440,
        0x95a8637627989aad,0xdde7001379a44aa8,
        0xbb127c53b17ec159,0x5560c018580d5d52,
        0xe9d71b689dde71af,0xaab8f01e6e10b4a6,
        0x9226712162ab070d,0xcab3961304ca70e8,
        0xb6b00d69bb55c8d1,0x3d607b97c5fd0d22,
        0xe45c10c42a2b3b05,0x8cb89a7db77c506a,
        0x8eb98a7a9a5b04e3,0x77f3608e92adb242,
        0xb267ed1940f1c61c,0x55f038b237591ed3,
        0xdf01e85f912e37a3,0x6b6c46dec52f6688,
        0x8b61313bbabce2c6,0x2323ac4b3b3da015,
        0xae397d8aa96c1b77,0xabec975e0a0d081a,
        0xd9c7dced53c72255,0x96e7bd358c904a21,
        0x881cea14545c7575,0x7e50d64177da2e54,
        0xaa242499697392d2,0xdde50bd1d5d0b9e9,
        0xd4ad2dbfc3d07787,0x955e4ec64b44e864,
        0x84ec3c97da624ab4,0xbd5af13bef0b113e,
        0xa6274bbdd0fadd61,0xecb1ad8aeacdd58e,
        0xcfb11ead453994ba,0x67de18eda5814af2,
        0x81ceb32c4b43fcf4,0x80eacf948770ced7,
        0xa2425ff75e14fc31,0xa1258379a94d028d,
        0xcad2f7f5359a3b3e,0x96ee45813a04330,
        0xfd87b5f28300ca0d,0x8bca9d6e188853fc,
        0x9e74d1b791e07e48,0x775ea264cf55347e,
        0xc612062576589dda,0x95364afe032a81a0,
        0xf79687aed3eec551,0x3a83ddbd83f52210,
        0x9abe14cd44753b52,0xc4926a9672793580,
        0xc16d9a0095928a27,0x75b7053c0f178400,
        0xf1c90080baf72cb1,0x5324c68b12dd6800,
        0x971da05074da7bee,0xd3f6fc16ebca8000,
        0xbce5086492111aea,0x88f4bb1ca6bd0000,
        0xec1e4a7db69561a5,0x2b31e9e3d0700000,
        0x9392ee8e921d5d07,0x3aff322e62600000,
        0xb877aa3236a4b449,0x9befeb9fad487c3,
        0xe69594bec44de15b,0x4c2ebe687989a9b4,
        0x901d7cf73ab0acd9,0xf9d37014bf60a11,
        0xb424dc35095cd80f,0x538484c19ef38c95,
        0xe12e13424bb40e13,0x2865a5f206b06fba,
        0x8cbccc096f5088cb,0xf93f87b7442e45d4,
        0xafebff0bcb24aafe,0xf78f69a51539d749,
        0xdbe6fecebdedd5be,0xb573440e5a884d1c,
        0x89705f4136b4a597,0x31680a88f8953031,
        0xabcc77118461cefc,0xfdc20d2b36ba7c3e,
        0xd6bf94d5e57a42bc,0x3d32907604691b4d,
        0x8637bd05af6c69b5,0xa63f9a49c2c1b110,
        0xa7c5ac471b478423,0xfcf80dc33721d54,
        0xd1b71758e219652b,0xd3c36113404ea4a9,
        0x83126e978d4fdf3b,0x645a1cac083126ea,
        0xa3d70a3d70a3d70a,0x3d70a3d70a3d70a4,
        0xcccccccccccccccc,0xcccccccccccccccd,
        0x8000000000000000,0x0,
        0xa000000000000000,0x0,
        0xc800000000000000,0x0,
        0xfa00000000000000,0x0,
        0x9c40000000000000,0x0,
        0xc350000000000000,0x0,
        0xf424000000000000,0x0,
        0x9896800000000000,0x0,
        0xbebc200000000000,0x0,
        0xee6b280000000000,0x0,
        0x9502f90000000000,0x0,
        0xba43b74000000000,0x0,
        0xe8d4a51000000000,0x0,
        0x9184e72a00000000,0x0,
        0xb5e620f480000000,0x0,
        0xe35fa931a0000000,0x0,
        0x8e1bc9bf04000000,0x0,
        0xb1a2bc2ec5000000,0x0,
        0xde0b6b3a76400000,0x0,
        0x8ac7230489e80000,0x0,
        0xad78ebc5ac620000,0x0,
        0xd8d726b7177a8000,0x0,
        0x878678326eac9000,0x0,
        0xa968163f0a57b400,0x0,
        0xd3c21bcecceda100,0x0,
        0x84595161401484a0,0x0,
        0xa56fa5b99019a5c8,0x0,
        0xcecb8f27f4200f3a,0x0,
        0x813f3978f8940984,0x4000000000000000,
        0xa18f07d736b90be5,0x5000000000000000,
        0xc9f2c9cd04674ede,0xa400000000000000,
        0xfc6f7c4045812296,0x4d00000000000000,
        0x9dc5ada82b70b59d,0xf020000000000000,
        0xc5371912364ce305,0x6c28000000000000,
        0xf684df56c3e01bc6,0xc732000000000000,
        0x9a130b963a6c115c,0x3c7f400000000000,
        0xc097ce7bc90715b3,0x4b9f100000000000,
        0xf0bdc21abb48db20,0x1e86d40000000000,
        0x96769950b50d88f4,0x1314448000000000,
        0xbc143fa4e250eb31,0x17d955a000000000,
        0xeb194f8e1ae525fd,0x5dcfab0800000000,
        0x92efd1b8d0cf37be,0x5aa1cae500000000,
        0xb7abc627050305ad,0xf14a3d9e40000000,
        0xe596b7b0c643c719,0x6d9ccd05d0000000,
        0x8f7e32ce7bea5c6f,0xe4820023a2000000,
        0xb35dbf821ae4f38b,0xdda2802c8a800000,
        0xe0352f62a19e306e,0xd50b2037ad200000,
        0x8c213d9da502de45,0x4526f422cc340000,
        0xaf298d050e4395d6,0x9670b12b7f410000,
        0xdaf3f04651d47b4c,0x3c0cdd765f114000,
        0x88d8762bf324cd0f,0xa5880a69fb6ac800,
        0xab0e93b6efee0053,0x8eea0d047a457a00,
        0xd5d238a4abe98068,0x72a4904598d6d880,
        0x85a36366eb71f041,0x47a6da2b7f864750,
        0xa70c3c40a64e6c51,0x999090b65f67d924,
        0xd0cf4b50cfe20765,0xfff4b4e3f741cf6d,
        0x82818f1281ed449f,0xbff8f10e7a8921a4,
        0xa321f2d7226895c7,0xaff72d52192b6a0d,
        0xcbea6f8ceb02bb39,0x9bf4f8a69f764490,
        0xfee50b7025c36a08,0x2f236d04753d5b4,
        0x9f4f2726179a2245,0x1d762422c946590,
        0xc722f0ef9d80aad6,0x424d3ad2b7b97ef5,
        0xf8ebad2b84e0d58b,0xd2e0898765a7deb2,
        0x9b934c3b330c8577,0x63cc55f49f88eb2f,
        0xc2781f49ffcfa6d5,0x3cbf6b71c76b25fb,
        0xf316271c7fc3908a,0x8bef464e3945ef7a,
        0x97edd871cfda3a56,0x97758bf0e3cbb5ac,
        0xbde94e8e43d0c8ec,0x3d52eeed1cbea317,
        0xed63a231d4c4fb27,0x4ca7aaa863ee4bdd,
        0x945e455f24fb1cf8,0x8fe8caa93e74ef6a,
        0xb975d6b6ee39e436,0xb3e2fd538e122b44,
        0xe7d34c64a9c85d44,0x60dbbca87196b616,
        0x90e40fbeea1d3a4a,0xbc8955e946fe31cd,
        0xb51d13aea4a488dd,0x6babab6398bdbe41,
        0xe264589a4dcdab14,0xc696963c7eed2dd1,
        0x8d7eb76070a08aec,0xfc1e1de5cf543ca2,
        0xb0de65388cc8ada8,0x3b25a55f43294bcb,
        0xdd15fe86affad912,0x49ef0eb713f39ebe,
        0x8a2dbf142dfcc7ab,0x6e3569326c784337,
        0xacb92ed9397bf996,0x49c2c37f07965404,
        0xd7e77a8f87daf7fb,0xdc33745ec97be906,
        0x86f0ac99b4e8dafd,0x69a028bb3ded71a3,
        0xa8acd7c0222311bc,0xc40832ea0d68ce0c,
        0xd2d80db02aabd62b,0xf50a3fa490c30190,
        0x83c7088e1aab65db,0x792667c6da79e0fa,
        0xa4b8cab1a1563f52,0x577001b891185938,
        0xcde6fd5e09abcf26,0xed4c0226b55e6f86,
        0x80b05e5ac60b6178,0x544f8158315b05b4,
        0xa0dc75f1778e39d6,0x696361ae3db1c721,
        0xc913936dd571c84c,0x3bc3a19cd1e38e9,
        0xfb5878494ace3a5f,0x4ab48a04065c723,
        0x9d174b2dcec0e47b,0x62eb0d64283f9c76,
        0xc45d1df942711d9a,0x3ba5d0bd324f8394,
        0xf5746577930d6500,0xca8f44ec7ee36479,
        0x9968bf6abbe85f20,0x7e998b13cf4e1ecb,
        0xbfc2ef456ae276e8,0x9e3fedd8c321a67e,
        0xefb3ab16c59b14a2,0xc5cfe94ef3ea101e,
        0x95d04aee3b80ece5,0xbba1f1d158724a12,
        0xbb445da9ca61281f,0x2a8a6e45ae8edc97,
        0xea1575143cf97226,0xf52d09d71a3293bd,
        0x924d692ca61be758,0x593c2626705f9c56,
        0xb6e0c377cfa2e12e,0x6f8b2fb00c77836c,
        0xe498f455c38b997a,0xb6dfb9c0f956447,
        0x8edf98b59a373fec,0x4724bd4189bd5eac,
        0xb2977ee300c50fe7,0x58edec91ec2cb657,
        0xdf3d5e9bc0f653e1,0x2f2967b66737e3ed,
        0x8b865b215899f46c,0xbd79e0d20082ee74,
        0xae67f1e9aec07187,0xecd8590680a3aa11,
        0xda01ee641a708de9,0xe80e6f4820cc9495,
        0x884134fe908658b2,0x3109058d147fdcdd,
        0xaa51823e34a7eede,0xbd4b46f0599fd415,
        0xd4e5e2cdc1d1ea96,0x6c9e18ac7007c91a,
        0x850fadc09923329e,0x3e2cf6bc604ddb0,
        0xa6539930bf6bff45,0x84db8346b786151c,
        0xcfe87f7cef46ff16,0xe612641865679a63,
        0x81f14fae158c5f6e,0x4fcb7e8f3f60c07e,
        0xa26da3999aef7749,0xe3be5e330f38f09d,
        0xcb090c8001ab551c,0x5cadf5bfd3072cc5,
        0xfdcb4fa002162a63,0x73d9732fc7c8f7f6,
        0x9e9f11c4014dda7e,0x2867e7fddcdd9afa,
        0xc646d63501a1511d,0xb281e1fd541501b8,
        0xf7d88bc24209a565,0x1f225a7ca91a4226,
        0x9ae757596946075f,0x3375788de9b06958,
        0xc1a12d2fc3978937,0x52d6b1641c83ae,
        0xf209787bb47d6b84,0xc0678c5dbd23a49a,
        0x9745eb4d50ce6332,0xf840b7ba963646e0,
        0xbd176620a501fbff,0xb650e5a93bc3d898,
        0xec5d3fa8ce427aff,0xa3e51f138ab4cebe,
        0x93ba47c980e98cdf,0xc66f336c36b10137,
        0xb8a8d9bbe123f017,0xb80b0047445d4184,
        0xe6d3102ad96cec1d,0xa60dc059157491e5,
        0x9043ea1ac7e41392,0x87c89837ad68db2f,
        0xb454e4a179dd1877,0x29babe4598c311fb,
        0xe16a1dc9d8545e94,0xf4296dd6fef3d67a,
        0x8ce2529e2734bb1d,0x1899e4a65f58660c,
        0xb01ae745b101e9e4,0x5ec05dcff72e7f8f,
        0xdc21a1171d42645d,0x76707543f4fa1f73,
        0x899504ae72497eba,0x6a06494a791c53a8,
        0xabfa45da0edbde69,0x487db9d17636892,
        0xd6f8d7509292d603,0x45a9d2845d3c42b6,
        0x865b86925b9bc5c2,0xb8a2392ba45a9b2,
        0xa7f26836f282b732,0x8e6cac7768d7141e,
        0xd1ef0244af2364ff,0x3207d795430cd926,
        0x8335616aed761f1f,0x7f44e6bd49e807b8,
        0xa402b9c5a8d3a6e7,0x5f16206c9c6209a6,
        0xcd036837130890a1,0x36dba887c37a8c0f,
        0x802221226be55a64,0xc2494954da2c9789,
        0xa02aa96b06deb0fd,0xf2db9baa10b7bd6c,
        0xc83553c5c8965d3d,0x6f92829494e5acc7,
        0xfa42a8b73abbf48c,0xcb772339ba1f17f9,
        0x9c69a97284b578d7,0xff2a760414536efb,
        0xc38413cf25e2d70d,0xfef5138519684aba,
        0xf46518c2ef5b8cd1,0x7eb258665fc25d69,
        0x98bf2f79d5993802,0xef2f773ffbd97a61,
        0xbeeefb584aff8603,0xaafb550ffacfd8fa,
        0xeeaaba2e5dbf6784,0x95ba2a53f983cf38,
        0x952ab45cfa97a0b2,0xdd945a747bf26183,
        0xba756174393d88df,0x94f971119aeef9e4,
        0xe912b9d1478ceb17,0x7a37cd5601aab85d,
        0x91abb422ccb812ee,0xac62e055c10ab33a,
        0xb616a12b7fe617aa,0x577b986b314d6009,
        0xe39c49765fdf9d94,0xed5a7e85fda0b80b,
        0x8e41ade9fbebc27d,0x14588f13be847307,
        0xb1d219647ae6b31c,0x596eb2d8ae258fc8,
        0xde469fbd99a05fe3,0x6fca5f8ed9aef3bb,
        0x8aec23d680043bee,0x25de7bb9480d5854,
        0xada72ccc20054ae9,0xaf561aa79a10ae6a,
        0xd910f7ff28069da4,0x1b2ba1518094da04,
        0x87aa9aff79042286,0x90fb44d2f05d0842,
        0xa99541bf57452b28,0x353a1607ac744a53,
        0xd3fa922f2d1675f2,0x42889b8997915ce8,
        0x847c9b5d7c2e09b7,0x69956135febada11,
        0xa59bc234db398c25,0x43fab9837e699095,
        0xcf02b2c21207ef2e,0x94f967e45e03f4bb,
        0x8161afb94b44f57d,0x1d1be0eebac278f5,
        0xa1ba1ba79e1632dc,0x6462d92a69731732,
        0xca28a291859bbf93,0x7d7b8f7503cfdcfe,
        0xfcb2cb35e702af78,0x5cda735244c3d43e,
        0x9defbf01b061adab,0x3a0888136afa64a7,
        0xc56baec21c7a1916,0x88aaa1845b8fdd0,
        0xf6c69a72a3989f5b,0x8aad549e57273d45,
        0x9a3c2087a63f6399,0x36ac54e2f678864b,
        0xc0cb28a98fcf3c7f,0x84576a1bb416a7dd,
        0xf0fdf2d3f3c30b9f,0x656d44a2a11c51d5,
        0x969eb7c47859e743,0x9f644ae5a4b1b325,
        0xbc4665b596706114,0x873d5d9f0dde1fee,
        0xeb57ff22fc0c7959,0xa90cb506d155a7ea,
        0x9316ff75dd87cbd8,0x9a7f12442d588f2,
        0xb7dcbf5354e9bece,0xc11ed6d538aeb2f,
        0xe5d3ef282a242e81,0x8f1668c8a86da5fa,
        0x8fa475791a569d10,0xf96e017d694487bc,
        0xb38d92d760ec4455,0x37c981dcc395a9ac,
        0xe070f78d3927556a,0x85bbe253f47b1417,
        0x8c469ab843b89562,0x93956d7478ccec8e,
        0xaf58416654a6babb,0x387ac8d1970027b2,
        0xdb2e51bfe9d0696a,0x6997b05fcc0319e,
        0x88fcf317f22241e2,0x441fece3bdf81f03,
        0xab3c2fddeeaad25a,0xd527e81cad7626c3,
        0xd60b3bd56a5586f1,0x8a71e223d8d3b074,
        0x85c7056562757456,0xf6872d5667844e49,
        0xa738c6bebb12d16c,0xb428f8ac016561db,
        0xd106f86e69d785c7,0xe13336d701beba52,
        0x82a45b450226b39c,0xecc0024661173473,
        0xa34d721642b06084,0x27f002d7f95d0190,
        0xcc20ce9bd35c78a5,0x31ec038df7b441f4,
        0xff290242c83396ce,0x7e67047175a15271,
        0x9f79a169bd203e41,0xf0062c6e984d386,
        0xc75809c42c684dd1,0x52c07b78a3e60868,
        0xf92e0c3537826145,0xa7709a56ccdf8a82,
        0x9bbcc7a142b17ccb,0x88a66076400bb691,
        0xc2abf989935ddbfe,0x6acff893d00ea435,
        0xf356f7ebf83552fe,0x583f6b8c4124d43,
        0x98165af37b2153de,0xc3727a337a8b704a,
        0xbe1bf1b059e9a8d6,0x744f18c0592e4c5c,
        0xeda2ee1c7064130c,0x1162def06f79df73,
        0x9485d4d1c63e8be7,0x8addcb5645ac2ba8,
        0xb9a74a0637ce2ee1,0x6d953e2bd7173692,
        0xe8111c87c5c1ba99,0xc8fa8db6ccdd0437,
        0x910ab1d4db9914a0,0x1d9c9892400a22a2,
        0xb54d5e4a127f59c8,0x2503beb6d00cab4b,
        0xe2a0b5dc971f303a,0x2e44ae64840fd61d,
        0x8da471a9de737e24,0x5ceaecfed289e5d2,
        0xb10d8e1456105dad,0x7425a83e872c5f47,
        0xdd50f1996b947518,0xd12f124e28f77719,
        0x8a5296ffe33cc92f,0x82bd6b70d99aaa6f,
        0xace73cbfdc0bfb7b,0x636cc64d1001550b,
        0xd8210befd30efa5a,0x3c47f7e05401aa4e,
        0x8714a775e3e95c78,0x65acfaec34810a71,
        0xa8d9d1535ce3b396,0x7f1839a741a14d0d,
        0xd31045a8341ca07c,0x1ede48111209a050,
        0x83ea2b892091e44d,0x934aed0aab460432,
        0xa4e4b66b68b65d60,0xf81da84d5617853f,
        0xce1de40642e3f4b9,0x36251260ab9d668e,
        0x80d2ae83e9ce78f3,0xc1d72b7c6b426019,
        0xa1075a24e4421730,0xb24cf65b8612f81f,
        0xc94930ae1d529cfc,0xdee033f26797b627,
        0xfb9b7cd9a4a7443c,0x169840ef017da3b1,
        0x9d412e0806e88aa5,0x8e1f289560ee864e,
        0xc491798a08a2ad4e,0xf1a6f2bab92a27e2,
        0xf5b5d7ec8acb58a2,0xae10af696774b1db,
        0x9991a6f3d6bf1765,0xacca6da1e0a8ef29,
        0xbff610b0cc6edd3f,0x17fd090a58d32af3,
        0xeff394dcff8a948e,0xddfc4b4cef07f5b0,
        0x95f83d0a1fb69cd9,0x4abdaf101564f98e,
        0xbb764c4ca7a4440f,0x9d6d1ad41abe37f1,
        0xea53df5fd18d5513,0x84c86189216dc5ed,
        0x92746b9be2f8552c,0x32fd3cf5b4e49bb4,
        0xb7118682dbb66a77,0x3fbc8c33221dc2a1,
        0xe4d5e82392a40515,0xfabaf3feaa5334a,
        0x8f05b1163ba6832d,0x29cb4d87f2a7400e,
        0xb2c71d5bca9023f8,0x743e20e9ef511012,
        0xdf78e4b2bd342cf6,0x914da9246b255416,
        0x8bab8eefb6409c1a,0x1ad089b6c2f7548e,
        0xae9672aba3d0c320,0xa184ac2473b529b1,
        0xda3c0f568cc4f3e8,0xc9e5d72d90a2741e,
        0x8865899617fb1871,0x7e2fa67c7a658892,
        0xaa7eebfb9df9de8d,0xddbb901b98feeab7,
        0xd51ea6fa85785631,0x552a74227f3ea565,
        0x8533285c936b35de,0xd53a88958f87275f,
        0xa67ff273b8460356,0x8a892abaf368f137,
        0xd01fef10a657842c,0x2d2b7569b0432d85,
        0x8213f56a67f6b29b,0x9c3b29620e29fc73,
        0xa298f2c501f45f42,0x8349f3ba91b47b8f,
        0xcb3f2f7642717713,0x241c70a936219a73,
        0xfe0efb53d30dd4d7,0xed238cd383aa0110,
        0x9ec95d1463e8a506,0xf4363804324a40aa,
        0xc67bb4597ce2ce48,0xb143c6053edcd0d5,
        0xf81aa16fdc1b81da,0xdd94b7868e94050a,
        0x9b10a4e5e9913128,0xca7cf2b4191c8326,
        0xc1d4ce1f63f57d72,0xfd1c2f611f63a3f0,
        0xf24a01a73cf2dccf,0xbc633b39673c8cec,
        0x976e41088617ca01,0xd5be0503e085d813,
        0xbd49d14aa79dbc82,0x4b2d8644d8a74e18,
        0xec9c459d51852ba2,0xddf8e7d60ed1219e,
        0x93e1ab8252f33b45,0xcabb90e5c942b503,
        0xb8da1662e7b00a17,0x3d6a751f3b936243,
        0xe7109bfba19c0c9d,0xcc512670a783ad4,
        0x906a617d450187e2,0x27fb2b80668b24c5,
        0xb484f9dc9641e9da,0xb1f9f660802dedf6,
        0xe1a63853bbd26451,0x5e7873f8a0396973,
        0x8d07e33455637eb2,0xdb0b487b6423e1e8,
        0xb049dc016abc5e5f,0x91ce1a9a3d2cda62,
        0xdc5c5301c56b75f7,0x7641a140cc7810fb,
        0x89b9b3e11b6329ba,0xa9e904c87fcb0a9d,
        0xac2820d9623bf429,0x546345fa9fbdcd44,
        0xd732290fbacaf133,0xa97c177947ad4095,
        0x867f59a9d4bed6c0,0x49ed8eabcccc485d,
        0xa81f301449ee8c70,0x5c68f256bfff5a74,
        0xd226fc195c6a2f8c,0x73832eec6fff3111,
        0x83585d8fd9c25db7,0xc831fd53c5ff7eab,
        0xa42e74f3d032f525,0xba3e7ca8b77f5e55,
        0xcd3a1230c43fb26f,0x28ce1bd2e55f35eb,
        0x80444b5e7aa7cf85,0x7980d163cf5b81b3,
        0xa0555e361951c366,0xd7e105bcc332621f,
        0xc86ab5c39fa63440,0x8dd9472bf3fefaa7,
        0xfa856334878fc150,0xb14f98f6f0feb951,
        0x9c935e00d4b9d8d2,0x6ed1bf9a569f33d3,
        0xc3b8358109e84f07,0xa862f80ec4700c8,
        0xf4a642e14c6262c8,0xcd27bb612758c0fa,
        0x98e7e9cccfbd7dbd,0x8038d51cb897789c,
        0xbf21e44003acdd2c,0xe0470a63e6bd56c3,
        0xeeea5d5004981478,0x1858ccfce06cac74,
        0x95527a5202df0ccb,0xf37801e0c43ebc8,
        0xbaa718e68396cffd,0xd30560258f54e6ba,
        0xe950df20247c83fd,0x47c6b82ef32a2069,
        0x91d28b7416cdd27e,0x4cdc331d57fa5441,
        0xb6472e511c81471d,0xe0133fe4adf8e952,
        0xe3d8f9e563a198e5,0x58180fddd97723a6,
        0x8e679c2f5e44ff8f,0x570f09eaa7ea7648,};

} // namespace internal
} // namespace simdjson
/* end file src/internal/numberparsing_tables.cpp */
/* begin file src/internal/simdprune_tables.cpp */
#if SIMDJSON_IMPLEMENTATION_ARM64 || SIMDJSON_IMPLEMENTATION_ICELAKE || SIMDJSON_IMPLEMENTATION_HASWELL || SIMDJSON_IMPLEMENTATION_WESTMERE || SIMDJSON_IMPLEMENTATION_PPC64

#include <cstdint>

namespace simdjson { // table modified and copied from
namespace internal { // http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetTable
SIMDJSON_DLLIMPORTEXPORT  const unsigned char BitsSetTable256mul2[256] = {
    0,  2,  2,  4,  2,  4,  4,  6,  2,  4,  4,  6,  4,  6,  6,  8,  2,  4,  4,
    6,  4,  6,  6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 2,  4,  4,  6,  4,  6,
    6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10, 6,
    8,  8,  10, 8,  10, 10, 12, 2,  4,  4,  6,  4,  6,  6,  8,  4,  6,  6,  8,
    6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10,
    12, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10, 12, 6,  8,
    8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 2,  4,  4,  6,  4,
    6,  6,  8,  4,  6,  6,  8,  6,  8,  8,  10, 4,  6,  6,  8,  6,  8,  8,  10,
    6,  8,  8,  10, 8,  10, 10, 12, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,
    10, 8,  10, 10, 12, 6,  8,  8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12,
    12, 14, 4,  6,  6,  8,  6,  8,  8,  10, 6,  8,  8,  10, 8,  10, 10, 12, 6,
    8,  8,  10, 8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 6,  8,  8,  10,
    8,  10, 10, 12, 8,  10, 10, 12, 10, 12, 12, 14, 8,  10, 10, 12, 10, 12, 12,
    14, 10, 12, 12, 14, 12, 14, 14, 16};

SIMDJSON_DLLIMPORTEXPORT  const uint8_t pshufb_combine_table[272] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0x00, 0x01, 0x02, 0x03,
    0x04, 0x05, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x08,
    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x01, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x00, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

// 256 * 8 bytes = 2kB, easily fits in cache.
SIMDJSON_DLLIMPORTEXPORT  const uint64_t thintable_epi8[256] = {
    0x0706050403020100, 0x0007060504030201, 0x0007060504030200,
    0x0000070605040302, 0x0007060504030100, 0x0000070605040301,
    0x0000070605040300, 0x0000000706050403, 0x0007060504020100,
    0x0000070605040201, 0x0000070605040200, 0x0000000706050402,
    0x0000070605040100, 0x0000000706050401, 0x0000000706050400,
    0x0000000007060504, 0x0007060503020100, 0x0000070605030201,
    0x0000070605030200, 0x0000000706050302, 0x0000070605030100,
    0x0000000706050301, 0x0000000706050300, 0x0000000007060503,
    0x0000070605020100, 0x0000000706050201, 0x0000000706050200,
    0x0000000007060502, 0x0000000706050100, 0x0000000007060501,
    0x0000000007060500, 0x0000000000070605, 0x0007060403020100,
    0x0000070604030201, 0x0000070604030200, 0x0000000706040302,
    0x0000070604030100, 0x0000000706040301, 0x0000000706040300,
    0x0000000007060403, 0x0000070604020100, 0x0000000706040201,
    0x0000000706040200, 0x0000000007060402, 0x0000000706040100,
    0x0000000007060401, 0x0000000007060400, 0x0000000000070604,
    0x0000070603020100, 0x0000000706030201, 0x0000000706030200,
    0x0000000007060302, 0x0000000706030100, 0x0000000007060301,
    0x0000000007060300, 0x0000000000070603, 0x0000000706020100,
    0x0000000007060201, 0x0000000007060200, 0x0000000000070602,
    0x0000000007060100, 0x0000000000070601, 0x0000000000070600,
    0x0000000000000706, 0x0007050403020100, 0x0000070504030201,
    0x0000070504030200, 0x0000000705040302, 0x0000070504030100,
    0x0000000705040301, 0x0000000705040300, 0x0000000007050403,
    0x0000070504020100, 0x0000000705040201, 0x0000000705040200,
    0x0000000007050402, 0x0000000705040100, 0x0000000007050401,
    0x0000000007050400, 0x0000000000070504, 0x0000070503020100,
    0x0000000705030201, 0x0000000705030200, 0x0000000007050302,
    0x0000000705030100, 0x0000000007050301, 0x0000000007050300,
    0x0000000000070503, 0x0000000705020100, 0x0000000007050201,
    0x0000000007050200, 0x0000000000070502, 0x0000000007050100,
    0x0000000000070501, 0x0000000000070500, 0x0000000000000705,
    0x0000070403020100, 0x0000000704030201, 0x0000000704030200,
    0x0000000007040302, 0x0000000704030100, 0x0000000007040301,
    0x0000000007040300, 0x0000000000070403, 0x0000000704020100,
    0x0000000007040201, 0x0000000007040200, 0x0000000000070402,
    0x0000000007040100, 0x0000000000070401, 0x0000000000070400,
    0x0000000000000704, 0x0000000703020100, 0x0000000007030201,
    0x0000000007030200, 0x0000000000070302, 0x0000000007030100,
    0x0000000000070301, 0x0000000000070300, 0x0000000000000703,
    0x0000000007020100, 0x0000000000070201, 0x0000000000070200,
    0x0000000000000702, 0x0000000000070100, 0x0000000000000701,
    0x0000000000000700, 0x0000000000000007, 0x0006050403020100,
    0x0000060504030201, 0x0000060504030200, 0x0000000605040302,
    0x0000060504030100, 0x0000000605040301, 0x0000000605040300,
    0x0000000006050403, 0x0000060504020100, 0x0000000605040201,
    0x0000000605040200, 0x0000000006050402, 0x0000000605040100,
    0x0000000006050401, 0x0000000006050400, 0x0000000000060504,
    0x0000060503020100, 0x0000000605030201, 0x0000000605030200,
    0x0000000006050302, 0x0000000605030100, 0x0000000006050301,
    0x0000000006050300, 0x0000000000060503, 0x0000000605020100,
    0x0000000006050201, 0x0000000006050200, 0x0000000000060502,
    0x0000000006050100, 0x0000000000060501, 0x0000000000060500,
    0x0000000000000605, 0x0000060403020100, 0x0000000604030201,
    0x0000000604030200, 0x0000000006040302, 0x0000000604030100,
    0x0000000006040301, 0x0000000006040300, 0x0000000000060403,
    0x0000000604020100, 0x0000000006040201, 0x0000000006040200,
    0x0000000000060402, 0x0000000006040100, 0x0000000000060401,
    0x0000000000060400, 0x0000000000000604, 0x0000000603020100,
    0x0000000006030201, 0x0000000006030200, 0x0000000000060302,
    0x0000000006030100, 0x0000000000060301, 0x0000000000060300,
    0x0000000000000603, 0x0000000006020100, 0x0000000000060201,
    0x0000000000060200, 0x0000000000000602, 0x0000000000060100,
    0x0000000000000601, 0x0000000000000600, 0x0000000000000006,
    0x0000050403020100, 0x0000000504030201, 0x0000000504030200,
    0x0000000005040302, 0x0000000504030100, 0x0000000005040301,
    0x0000000005040300, 0x0000000000050403, 0x0000000504020100,
    0x0000000005040201, 0x0000000005040200, 0x0000000000050402,
    0x0000000005040100, 0x0000000000050401, 0x0000000000050400,
    0x0000000000000504, 0x0000000503020100, 0x0000000005030201,
    0x0000000005030200, 0x0000000000050302, 0x0000000005030100,
    0x0000000000050301, 0x0000000000050300, 0x0000000000000503,
    0x0000000005020100, 0x0000000000050201, 0x0000000000050200,
    0x0000000000000502, 0x0000000000050100, 0x0000000000000501,
    0x0000000000000500, 0x0000000000000005, 0x0000000403020100,
    0x0000000004030201, 0x0000000004030200, 0x0000000000040302,
    0x0000000004030100, 0x0000000000040301, 0x0000000000040300,
    0x0000000000000403, 0x0000000004020100, 0x0000000000040201,
    0x0000000000040200, 0x0000000000000402, 0x0000000000040100,
    0x0000000000000401, 0x0000000000000400, 0x0000000000000004,
    0x0000000003020100, 0x0000000000030201, 0x0000000000030200,
    0x0000000000000302, 0x0000000000030100, 0x0000000000000301,
    0x0000000000000300, 0x0000000000000003, 0x0000000000020100,
    0x0000000000000201, 0x0000000000000200, 0x0000000000000002,
    0x0000000000000100, 0x0000000000000001, 0x0000000000000000,
    0x0000000000000000,
}; //static uint64_t thintable_epi8[256]

} // namespace internal
} // namespace simdjson

#endif //  SIMDJSON_IMPLEMENTATION_ARM64 || SIMDJSON_IMPLEMENTATION_ICELAKE || SIMDJSON_IMPLEMENTATION_HASWELL || SIMDJSON_IMPLEMENTATION_WESTMERE || SIMDJSON_IMPLEMENTATION_PPC64
/* end file src/internal/simdprune_tables.cpp */
/* begin file src/implementation.cpp */
#include <initializer_list>

namespace simdjson {

bool implementation::supported_by_runtime_system() const {
  uint32_t required_instruction_sets = this->required_instruction_sets();
  uint32_t supported_instruction_sets = internal::detect_supported_architectures();
  return ((supported_instruction_sets & required_instruction_sets) == required_instruction_sets);
}

namespace internal {

// Static array of known implementations. We're hoping these get baked into the executable
// without requiring a static initializer.

#if SIMDJSON_IMPLEMENTATION_ICELAKE
static const icelake::implementation* get_icelake_singleton() {
  static const icelake::implementation icelake_singleton{};
  return &icelake_singleton;
}
#endif
#if SIMDJSON_IMPLEMENTATION_HASWELL
static const haswell::implementation* get_haswell_singleton() {
  static const haswell::implementation haswell_singleton{};
  return &haswell_singleton;
}
#endif
#if SIMDJSON_IMPLEMENTATION_WESTMERE
static const westmere::implementation* get_westmere_singleton() {
  static const westmere::implementation westmere_singleton{};
  return &westmere_singleton;
}
#endif // SIMDJSON_IMPLEMENTATION_WESTMERE
#if SIMDJSON_IMPLEMENTATION_ARM64
static const arm64::implementation* get_arm64_singleton() {
  static const arm64::implementation arm64_singleton{};
  return &arm64_singleton;
}
#endif // SIMDJSON_IMPLEMENTATION_ARM64
#if SIMDJSON_IMPLEMENTATION_PPC64
static const ppc64::implementation* get_ppc64_singleton() {
  static const ppc64::implementation ppc64_singleton{};
  return &ppc64_singleton;
}
#endif // SIMDJSON_IMPLEMENTATION_PPC64
#if SIMDJSON_IMPLEMENTATION_FALLBACK
static const fallback::implementation* get_fallback_singleton() {
  static const fallback::implementation fallback_singleton{};
  return &fallback_singleton;
}
#endif // SIMDJSON_IMPLEMENTATION_FALLBACK

/**
 * @private Detects best supported implementation on first use, and sets it
 */
class detect_best_supported_implementation_on_first_use final : public implementation {
public:
  const std::string &name() const noexcept final { return set_best()->name(); }
  const std::string &description() const noexcept final { return set_best()->description(); }
  uint32_t required_instruction_sets() const noexcept final { return set_best()->required_instruction_sets(); }
  simdjson_warn_unused error_code create_dom_parser_implementation(
    size_t capacity,
    size_t max_length,
    std::unique_ptr<internal::dom_parser_implementation>& dst
  ) const noexcept final {
    return set_best()->create_dom_parser_implementation(capacity, max_length, dst);
  }
  simdjson_warn_unused error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept final {
    return set_best()->minify(buf, len, dst, dst_len);
  }
  simdjson_warn_unused bool validate_utf8(const char * buf, size_t len) const noexcept final override {
    return set_best()->validate_utf8(buf, len);
  }
  simdjson_inline detect_best_supported_implementation_on_first_use() noexcept : implementation("best_supported_detector", "Detects the best supported implementation and sets it", 0) {}
private:
  const implementation *set_best() const noexcept;
};

static const std::initializer_list<const implementation *>& get_available_implementation_pointers() {
  static const std::initializer_list<const implementation *> available_implementation_pointers {
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    get_icelake_singleton(),
#endif
#if SIMDJSON_IMPLEMENTATION_HASWELL
    get_haswell_singleton(),
#endif
#if SIMDJSON_IMPLEMENTATION_WESTMERE
    get_westmere_singleton(),
#endif
#if SIMDJSON_IMPLEMENTATION_ARM64
    get_arm64_singleton(),
#endif
#if SIMDJSON_IMPLEMENTATION_PPC64
    get_ppc64_singleton(),
#endif
#if SIMDJSON_IMPLEMENTATION_FALLBACK
    get_fallback_singleton(),
#endif
  }; // available_implementation_pointers
  return available_implementation_pointers;
}

// So we can return UNSUPPORTED_ARCHITECTURE from the parser when there is no support
class unsupported_implementation final : public implementation {
public:
  simdjson_warn_unused error_code create_dom_parser_implementation(
    size_t,
    size_t,
    std::unique_ptr<internal::dom_parser_implementation>&
  ) const noexcept final {
    return UNSUPPORTED_ARCHITECTURE;
  }
  simdjson_warn_unused error_code minify(const uint8_t *, size_t, uint8_t *, size_t &) const noexcept final override {
    return UNSUPPORTED_ARCHITECTURE;
  }
  simdjson_warn_unused bool validate_utf8(const char *, size_t) const noexcept final override {
    return false; // Just refuse to validate. Given that we have a fallback implementation
    // it seems unlikely that unsupported_implementation will ever be used. If it is used,
    // then it will flag all strings as invalid. The alternative is to return an error_code
    // from which the user has to figure out whether the string is valid UTF-8... which seems
    // like a lot of work just to handle the very unlikely case that we have an unsupported
    // implementation. And, when it does happen (that we have an unsupported implementation),
    // what are the chances that the programmer has a fallback? Given that *we* provide the
    // fallback, it implies that the programmer would need a fallback for our fallback.
  }
  unsupported_implementation() : implementation("unsupported", "Unsupported CPU (no detected SIMD instructions)", 0) {}
};

const unsupported_implementation* get_unsupported_singleton() {
    static const unsupported_implementation unsupported_singleton{};
    return &unsupported_singleton;
}

size_t available_implementation_list::size() const noexcept {
  return internal::get_available_implementation_pointers().size();
}
const implementation * const *available_implementation_list::begin() const noexcept {
  return internal::get_available_implementation_pointers().begin();
}
const implementation * const *available_implementation_list::end() const noexcept {
  return internal::get_available_implementation_pointers().end();
}
const implementation *available_implementation_list::detect_best_supported() const noexcept {
  // They are prelisted in priority order, so we just go down the list
  uint32_t supported_instruction_sets = internal::detect_supported_architectures();
  for (const implementation *impl : internal::get_available_implementation_pointers()) {
    uint32_t required_instruction_sets = impl->required_instruction_sets();
    if ((supported_instruction_sets & required_instruction_sets) == required_instruction_sets) { return impl; }
  }
  return get_unsupported_singleton(); // this should never happen?
}

const implementation *detect_best_supported_implementation_on_first_use::set_best() const noexcept {
  SIMDJSON_PUSH_DISABLE_WARNINGS
  SIMDJSON_DISABLE_DEPRECATED_WARNING // Disable CRT_SECURE warning on MSVC: manually verified this is safe
  char *force_implementation_name = getenv("SIMDJSON_FORCE_IMPLEMENTATION");
  SIMDJSON_POP_DISABLE_WARNINGS

  if (force_implementation_name) {
    auto force_implementation = get_available_implementations()[force_implementation_name];
    if (force_implementation) {
      return get_active_implementation() = force_implementation;
    } else {
      // Note: abort() and stderr usage within the library is forbidden.
      return get_active_implementation() = get_unsupported_singleton();
    }
  }
  return get_active_implementation() = get_available_implementations().detect_best_supported();
}

} // namespace internal

SIMDJSON_DLLIMPORTEXPORT const internal::available_implementation_list& get_available_implementations() {
  static const internal::available_implementation_list available_implementations{};
  return available_implementations;
}

SIMDJSON_DLLIMPORTEXPORT internal::atomic_ptr<const implementation>& get_active_implementation() {
    static const internal::detect_best_supported_implementation_on_first_use detect_best_supported_implementation_on_first_use_singleton;
    static internal::atomic_ptr<const implementation> active_implementation{&detect_best_supported_implementation_on_first_use_singleton};
    return active_implementation;
}

simdjson_warn_unused error_code minify(const char *buf, size_t len, char *dst, size_t &dst_len) noexcept {
  return get_active_implementation()->minify(reinterpret_cast<const uint8_t *>(buf), len, reinterpret_cast<uint8_t *>(dst), dst_len);
}
simdjson_warn_unused bool validate_utf8(const char *buf, size_t len) noexcept {
  return get_active_implementation()->validate_utf8(buf, len);
}
const implementation * builtin_implementation() {
  static const implementation * builtin_impl = get_available_implementations()[SIMDJSON_STRINGIFY(SIMDJSON_BUILTIN_IMPLEMENTATION)];
  assert(builtin_impl);
  return builtin_impl;
}


} // namespace simdjson
/* end file src/implementation.cpp */

#if SIMDJSON_IMPLEMENTATION_ARM64
/* begin file src/arm64/implementation.cpp */
/* begin file include/simdjson/arm64/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "arm64"
// #define SIMDJSON_IMPLEMENTATION arm64
/* end file include/simdjson/arm64/begin.h */

namespace simdjson {
namespace arm64 {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace arm64
} // namespace simdjson

/* begin file include/simdjson/arm64/end.h */
/* end file include/simdjson/arm64/end.h */
/* end file src/arm64/implementation.cpp */
/* begin file src/arm64/dom_parser_implementation.cpp */
/* begin file include/simdjson/arm64/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "arm64"
// #define SIMDJSON_IMPLEMENTATION arm64
/* end file include/simdjson/arm64/begin.h */

//
// Stage 1
//
namespace simdjson {
namespace arm64 {
namespace {

using namespace simd;

struct json_character_block {
  static simdjson_inline json_character_block classify(const simd::simd8x64<uint8_t>& in);

  simdjson_inline uint64_t whitespace() const noexcept { return _whitespace; }
  simdjson_inline uint64_t op() const noexcept { return _op; }
  simdjson_inline uint64_t scalar() const noexcept { return ~(op() | whitespace()); }

  uint64_t _whitespace;
  uint64_t _op;
};

simdjson_inline json_character_block json_character_block::classify(const simd::simd8x64<uint8_t>& in) {
  // Functional programming causes trouble with Visual Studio.
  // Keeping this version in comments since it is much nicer:
  // auto v = in.map<uint8_t>([&](simd8<uint8_t> chunk) {
  //  auto nib_lo = chunk & 0xf;
  //  auto nib_hi = chunk.shr<4>();
  //  auto shuf_lo = nib_lo.lookup_16<uint8_t>(16, 0, 0, 0, 0, 0, 0, 0, 0, 8, 12, 1, 2, 9, 0, 0);
  //  auto shuf_hi = nib_hi.lookup_16<uint8_t>(8, 0, 18, 4, 0, 1, 0, 1, 0, 0, 0, 3, 2, 1, 0, 0);
  //  return shuf_lo & shuf_hi;
  // });
  const simd8<uint8_t> table1(16, 0, 0, 0, 0, 0, 0, 0, 0, 8, 12, 1, 2, 9, 0, 0);
  const simd8<uint8_t> table2(8, 0, 18, 4, 0, 1, 0, 1, 0, 0, 0, 3, 2, 1, 0, 0);

  simd8x64<uint8_t> v(
     (in.chunks[0] & 0xf).lookup_16(table1) & (in.chunks[0].shr<4>()).lookup_16(table2),
     (in.chunks[1] & 0xf).lookup_16(table1) & (in.chunks[1].shr<4>()).lookup_16(table2),
     (in.chunks[2] & 0xf).lookup_16(table1) & (in.chunks[2].shr<4>()).lookup_16(table2),
     (in.chunks[3] & 0xf).lookup_16(table1) & (in.chunks[3].shr<4>()).lookup_16(table2)
  );


  // We compute whitespace and op separately. If the code later only use one or the
  // other, given the fact that all functions are aggressively inlined, we can
  // hope that useless computations will be omitted. This is namely case when
  // minifying (we only need whitespace). *However* if we only need spaces,
  // it is likely that we will still compute 'v' above with two lookup_16: one
  // could do it a bit cheaper. This is in contrast with the x64 implementations
  // where we can, efficiently, do the white space and structural matching
  // separately. One reason for this difference is that on ARM NEON, the table
  // lookups either zero or leave unchanged the characters exceeding 0xF whereas
  // on x64, the equivalent instruction (pshufb) automatically applies a mask,
  // ignoring the 4 most significant bits. Thus the x64 implementation is
  // optimized differently. This being said, if you use this code strictly
  // just for minification (or just to identify the structural characters),
  // there is a small untaken optimization opportunity here. We deliberately
  // do not pick it up.

  uint64_t op = simd8x64<bool>(
        v.chunks[0].any_bits_set(0x7),
        v.chunks[1].any_bits_set(0x7),
        v.chunks[2].any_bits_set(0x7),
        v.chunks[3].any_bits_set(0x7)
  ).to_bitmask();

  uint64_t whitespace = simd8x64<bool>(
        v.chunks[0].any_bits_set(0x18),
        v.chunks[1].any_bits_set(0x18),
        v.chunks[2].any_bits_set(0x18),
        v.chunks[3].any_bits_set(0x18)
  ).to_bitmask();

  return { whitespace, op };
}

simdjson_inline bool is_ascii(const simd8x64<uint8_t>& input) {
    simd8<uint8_t> bits = input.reduce_or();
    return bits.max_val() < 0x80u;
}

simdjson_unused simdjson_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
    simd8<bool> is_second_byte = prev1 >= uint8_t(0xc0u);
    simd8<bool> is_third_byte  = prev2 >= uint8_t(0xe0u);
    simd8<bool> is_fourth_byte = prev3 >= uint8_t(0xf0u);
    // Use ^ instead of | for is_*_byte, because ^ is commutative, and the caller is using ^ as well.
    // This will work fine because we only have to report errors for cases with 0-1 lead bytes.
    // Multiple lead bytes implies 2 overlapping multibyte characters, and if that happens, there is
    // guaranteed to be at least *one* lead byte that is part of only 1 other multibyte character.
    // The error will be detected there.
    return is_second_byte ^ is_third_byte ^ is_fourth_byte;
}

simdjson_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
    simd8<bool> is_third_byte  = prev2 >= uint8_t(0xe0u);
    simd8<bool> is_fourth_byte = prev3 >= uint8_t(0xf0u);
    return is_third_byte ^ is_fourth_byte;
}

} // unnamed namespace
} // namespace arm64
} // namespace simdjson

/* begin file src/generic/stage1/utf8_lookup4_algorithm.h */
namespace simdjson {
namespace arm64 {
namespace {
namespace utf8_validation {

using namespace simd;

  simdjson_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdjson_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }

  //
  // Return nonzero if there are incomplete multibyte characters at the end of the block:
  // e.g. if there is a 4-byte character, but it's 3 bytes from the end.
  //
  simdjson_inline simd8<uint8_t> is_incomplete(const simd8<uint8_t> input) {
    // If the previous input's last 3 bytes match this, they're too short (they ended at EOF):
    // ... 1111____ 111_____ 11______
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    static const uint8_t max_array[64] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#else
    static const uint8_t max_array[32] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#endif
    const simd8<uint8_t> max_value(&max_array[sizeof(max_array)-sizeof(simd8<uint8_t>)]);
    return input.gt_bits(max_value);
  }

  struct utf8_checker {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;
    // The last input we received
    simd8<uint8_t> prev_input_block;
    // Whether the last input we received was incomplete (used for ASCII fast path)
    simd8<uint8_t> prev_incomplete;

    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdjson_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }

    // The only problem that can happen at EOF is that a multibyte character is too short
    // or a byte value too large in the last bytes: check_special_cases only checks for bytes
    // too large in the first of two bytes.
    simdjson_inline void check_eof() {
      // If the previous block had incomplete UTF-8 characters at the end, an ASCII block can't
      // possibly finish them.
      this->error |= this->prev_incomplete;
    }

#ifndef SIMDJSON_IF_CONSTEXPR
#if SIMDJSON_CPLUSPLUS17
#define SIMDJSON_IF_CONSTEXPR if constexpr
#else
#define SIMDJSON_IF_CONSTEXPR if
#endif
#endif

    simdjson_inline void check_next_input(const simd8x64<uint8_t>& input) {
      if(simdjson_likely(is_ascii(input))) {
        this->error |= this->prev_incomplete;
      } else {
        // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
        static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 1)
                ||(simd8x64<uint8_t>::NUM_CHUNKS == 2)
                || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
                "We support one, two or four chunks per 64-byte block.");
        SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 1) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 2) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 4) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
          this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
        }
        this->prev_incomplete = is_incomplete(input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1]);
        this->prev_input_block = input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1];
      }
    }
    // do not forget to call check_eof!
    simdjson_inline error_code errors() {
      return this->error.any_bits_set_anywhere() ? error_code::UTF8_ERROR : error_code::SUCCESS;
    }

  }; // struct utf8_checker
} // namespace utf8_validation

using utf8_validation::utf8_checker;

} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/utf8_lookup4_algorithm.h */
/* begin file src/generic/stage1/json_structural_indexer.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

/* begin file src/generic/stage1/buf_block_reader.h */
namespace simdjson {
namespace arm64 {
namespace {

// Walks through a buffer in block-sized increments, loading the last part with spaces
template<size_t STEP_SIZE>
struct buf_block_reader {
public:
  simdjson_inline buf_block_reader(const uint8_t *_buf, size_t _len);
  simdjson_inline size_t block_index();
  simdjson_inline bool has_full_block() const;
  simdjson_inline const uint8_t *full_block() const;
  /**
   * Get the last block, padded with spaces.
   *
   * There will always be a last block, with at least 1 byte, unless len == 0 (in which case this
   * function fills the buffer with spaces and returns 0. In particular, if len == STEP_SIZE there
   * will be 0 full_blocks and 1 remainder block with STEP_SIZE bytes and no spaces for padding.
   *
   * @return the number of effective characters in the last block.
   */
  simdjson_inline size_t get_remainder(uint8_t *dst) const;
  simdjson_inline void advance();
private:
  const uint8_t *buf;
  const size_t len;
  const size_t lenminusstep;
  size_t idx;
};

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text_64(const uint8_t *text) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    buf[i] = int8_t(text[i]) < ' ' ? '_' : int8_t(text[i]);
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text(const simd8x64<uint8_t>& in) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  in.store(reinterpret_cast<uint8_t*>(buf));
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    if (buf[i] < ' ') { buf[i] = '_'; }
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

simdjson_unused static char * format_mask(uint64_t mask) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<64; i++) {
    buf[i] = (mask & (size_t(1) << i)) ? 'X' : ' ';
  }
  buf[64] = '\0';
  return buf;
}

template<size_t STEP_SIZE>
simdjson_inline buf_block_reader<STEP_SIZE>::buf_block_reader(const uint8_t *_buf, size_t _len) : buf{_buf}, len{_len}, lenminusstep{len < STEP_SIZE ? 0 : len - STEP_SIZE}, idx{0} {}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::block_index() { return idx; }

template<size_t STEP_SIZE>
simdjson_inline bool buf_block_reader<STEP_SIZE>::has_full_block() const {
  return idx < lenminusstep;
}

template<size_t STEP_SIZE>
simdjson_inline const uint8_t *buf_block_reader<STEP_SIZE>::full_block() const {
  return &buf[idx];
}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::get_remainder(uint8_t *dst) const {
  if(len == idx) { return 0; } // memcpy(dst, null, 0) will trigger an error with some sanitizers
  std::memset(dst, 0x20, STEP_SIZE); // std::memset STEP_SIZE because it's more efficient to write out 8 or 16 bytes at once.
  std::memcpy(dst, buf + idx, len - idx);
  return len - idx;
}

template<size_t STEP_SIZE>
simdjson_inline void buf_block_reader<STEP_SIZE>::advance() {
  idx += STEP_SIZE;
}

} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/buf_block_reader.h */
/* begin file src/generic/stage1/json_string_scanner.h */
namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

struct json_string_block {
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_string_block(uint64_t backslash, uint64_t escaped, uint64_t quote, uint64_t in_string) :
  _backslash(backslash), _escaped(escaped), _quote(quote), _in_string(in_string) {}

  // Escaped characters (characters following an escape() character)
  simdjson_inline uint64_t escaped() const { return _escaped; }
  // Escape characters (backslashes that are not escaped--i.e. in \\, includes only the first \)
  simdjson_inline uint64_t escape() const { return _backslash & ~_escaped; }
  // Real (non-backslashed) quotes
  simdjson_inline uint64_t quote() const { return _quote; }
  // Start quotes of strings
  simdjson_inline uint64_t string_start() const { return _quote & _in_string; }
  // End quotes of strings
  simdjson_inline uint64_t string_end() const { return _quote & ~_in_string; }
  // Only characters inside the string (not including the quotes)
  simdjson_inline uint64_t string_content() const { return _in_string & ~_quote; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const { return mask & _in_string; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const { return mask & ~_in_string; }
  // Tail of string (everything except the start quote)
  simdjson_inline uint64_t string_tail() const { return _in_string ^ _quote; }

  // backslash characters
  uint64_t _backslash;
  // escaped characters (backslashed--does not include the hex characters after \u)
  uint64_t _escaped;
  // real quotes (non-backslashed ones)
  uint64_t _quote;
  // string characters (includes start quote but not end quote)
  uint64_t _in_string;
};

// Scans blocks for string characters, storing the state necessary to do so
class json_string_scanner {
public:
  simdjson_inline json_string_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Intended to be defined by the implementation
  simdjson_inline uint64_t find_escaped(uint64_t escape);
  simdjson_inline uint64_t find_escaped_branchless(uint64_t escape);

  // Whether the last iteration was still inside a string (all 1's = true, all 0's = false).
  uint64_t prev_in_string = 0ULL;
  // Whether the first character of the next iteration is escaped.
  uint64_t prev_escaped = 0ULL;
};

//
// Finds escaped characters (characters following \).
//
// Handles runs of backslashes like \\\" and \\\\" correctly (yielding 0101 and 01010, respectively).
//
// Does this by:
// - Shift the escape mask to get potentially escaped characters (characters after backslashes).
// - Mask escaped sequences that start on *even* bits with 1010101010 (odd bits are escaped, even bits are not)
// - Mask escaped sequences that start on *odd* bits with 0101010101 (even bits are escaped, odd bits are not)
//
// To distinguish between escaped sequences starting on even/odd bits, it finds the start of all
// escape sequences, filters out the ones that start on even bits, and adds that to the mask of
// escape sequences. This causes the addition to clear out the sequences starting on odd bits (since
// the start bit causes a carry), and leaves even-bit sequences alone.
//
// Example:
//
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
// escape         |  xxx |  xx xxx  xxx  xx xx  | Removed overflow backslash; will | it into follows_escape
// odd_starts     |  x   |  x       x       x   | escape & ~even_bits & ~follows_escape
// even_seq       |     c|    cxxx     c xx   c | c = carry bit -- will be masked out later
// invert_mask    |      |     cxxx     c xx   c| even_seq << 1
// follows_escape |   xx | x xx xxx  xxx  xx xx | Includes overflow bit
// escaped        |   x  | x x  x x  x x  x  x  |
// desired        |   x  | x x  x x  x x  x  x  |
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
//
simdjson_inline uint64_t json_string_scanner::find_escaped_branchless(uint64_t backslash) {
  // If there was overflow, pretend the first character isn't a backslash
  backslash &= ~prev_escaped;
  uint64_t follows_escape = backslash << 1 | prev_escaped;

  // Get sequences starting on even bits by clearing out the odd series using +
  const uint64_t even_bits = 0x5555555555555555ULL;
  uint64_t odd_sequence_starts = backslash & ~even_bits & ~follows_escape;
  uint64_t sequences_starting_on_even_bits;
  prev_escaped = add_overflow(odd_sequence_starts, backslash, &sequences_starting_on_even_bits);
  uint64_t invert_mask = sequences_starting_on_even_bits << 1; // The mask we want to return is the *escaped* bits, not escapes.

  // Mask every other backslashed character as an escaped character
  // Flip the mask for sequences that start on even bits, to correct them
  return (even_bits ^ invert_mask) & follows_escape;
}

//
// Return a mask of all string characters plus end quotes.
//
// prev_escaped is overflow saying whether the next character is escaped.
// prev_in_string is overflow saying whether we're still in a string.
//
// Backslash sequences outside of quotes will be detected in stage 2.
//
simdjson_inline json_string_block json_string_scanner::next(const simd::simd8x64<uint8_t>& in) {
  const uint64_t backslash = in.eq('\\');
  const uint64_t escaped = find_escaped(backslash);
  const uint64_t quote = in.eq('"') & ~escaped;

  //
  // prefix_xor flips on bits inside the string (and flips off the end quote).
  //
  // Then we xor with prev_in_string: if we were in a string already, its effect is flipped
  // (characters inside strings are outside, and characters outside strings are inside).
  //
  const uint64_t in_string = prefix_xor(quote) ^ prev_in_string;

  //
  // Check if we're still in a string at the end of the box so the next block will know
  //
  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20, John Regher from Utah U. says this is fine code
  //
  prev_in_string = uint64_t(static_cast<int64_t>(in_string) >> 63);

  // Use ^ to turn the beginning quote off, and the end quote on.

  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_string_block(
    backslash,
    escaped,
    quote,
    in_string
  );
}

simdjson_inline error_code json_string_scanner::finish() {
  if (prev_in_string) {
    return UNCLOSED_STRING;
  }
  return SUCCESS;
}

} // namespace stage1
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/json_string_scanner.h */
/* begin file src/generic/stage1/json_scanner.h */
namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

/**
 * A block of scanned json, with information on operators and scalars.
 *
 * We seek to identify pseudo-structural characters. Anything that is inside
 * a string must be omitted (hence  & ~_string.string_tail()).
 * Otherwise, pseudo-structural characters come in two forms.
 * 1. We have the structural characters ([,],{,},:, comma). The
 *    term 'structural character' is from the JSON RFC.
 * 2. We have the 'scalar pseudo-structural characters'.
 *    Scalars are quotes, and any character except structural characters and white space.
 *
 * To identify the scalar pseudo-structural characters, we must look at what comes
 * before them: it must be a space, a quote or a structural characters.
 * Starting with simdjson v0.3, we identify them by
 * negation: we identify everything that is followed by a non-quote scalar,
 * and we negate that. Whatever remains must be a 'scalar pseudo-structural character'.
 */
struct json_block {
public:
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_block(json_string_block&& string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(std::move(string)), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}
  simdjson_inline json_block(json_string_block string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(string), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}

  /**
   * The start of structurals.
   * In simdjson prior to v0.3, these were called the pseudo-structural characters.
   **/
  simdjson_inline uint64_t structural_start() const noexcept { return potential_structural_start() & ~_string.string_tail(); }
  /** All JSON whitespace (i.e. not in a string) */
  simdjson_inline uint64_t whitespace() const noexcept { return non_quote_outside_string(_characters.whitespace()); }

  // Helpers

  /** Whether the given characters are inside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const noexcept { return _string.non_quote_inside_string(mask); }
  /** Whether the given characters are outside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const noexcept { return _string.non_quote_outside_string(mask); }

  // string and escape characters
  json_string_block _string;
  // whitespace, structural characters ('operators'), scalars
  json_character_block _characters;
  // whether the previous character was a scalar
  uint64_t _follows_potential_nonquote_scalar;
private:
  // Potential structurals (i.e. disregarding strings)

  /**
   * structural elements ([,],{,},:, comma) plus scalar starts like 123, true and "abc".
   * They may reside inside a string.
   **/
  simdjson_inline uint64_t potential_structural_start() const noexcept { return _characters.op() | potential_scalar_start(); }
  /**
   * The start of non-operator runs, like 123, true and "abc".
   * It main reside inside a string.
   **/
  simdjson_inline uint64_t potential_scalar_start() const noexcept {
    // The term "scalar" refers to anything except structural characters and white space
    // (so letters, numbers, quotes).
    // Whenever it is preceded by something that is not a structural element ({,},[,],:, ") nor a white-space
    // then we know that it is irrelevant structurally.
    return _characters.scalar() & ~follows_potential_scalar();
  }
  /**
   * Whether the given character is immediately after a non-operator like 123, true.
   * The characters following a quote are not included.
   */
  simdjson_inline uint64_t follows_potential_scalar() const noexcept {
    // _follows_potential_nonquote_scalar: is defined as marking any character that follows a character
    // that is not a structural element ({,},[,],:, comma) nor a quote (") and that is not a
    // white space.
    // It is understood that within quoted region, anything at all could be marked (irrelevant).
    return _follows_potential_nonquote_scalar;
  }
};

/**
 * Scans JSON for important bits: structural characters or 'operators', strings, and scalars.
 *
 * The scanner starts by calculating two distinct things:
 * - string characters (taking \" into account)
 * - structural characters or 'operators' ([]{},:, comma)
 *   and scalars (runs of non-operators like 123, true and "abc")
 *
 * To minimize data dependency (a key component of the scanner's speed), it finds these in parallel:
 * in particular, the operator/scalar bit will find plenty of things that are actually part of
 * strings. When we're done, json_block will fuse the two together by masking out tokens that are
 * part of a string.
 */
class json_scanner {
public:
  json_scanner() = default;
  simdjson_inline json_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Whether the last character of the previous iteration is part of a scalar token
  // (anything except whitespace or a structural character/'operator').
  uint64_t prev_scalar = 0ULL;
  json_string_scanner string_scanner{};
};


//
// Check if the current character immediately follows a matching character.
//
// For example, this checks for quotes with backslashes in front of them:
//
//     const uint64_t backslashed_quote = in.eq('"') & immediately_follows(in.eq('\'), prev_backslash);
//
simdjson_inline uint64_t follows(const uint64_t match, uint64_t &overflow) {
  const uint64_t result = match << 1 | overflow;
  overflow = match >> 63;
  return result;
}

simdjson_inline json_block json_scanner::next(const simd::simd8x64<uint8_t>& in) {
  json_string_block strings = string_scanner.next(in);
  // identifies the white-space and the structural characters
  json_character_block characters = json_character_block::classify(in);
  // The term "scalar" refers to anything except structural characters and white space
  // (so letters, numbers, quotes).
  // We want follows_scalar to mark anything that follows a non-quote scalar (so letters and numbers).
  //
  // A terminal quote should either be followed by a structural character (comma, brace, bracket, colon)
  // or nothing. However, we still want ' "a string"true ' to mark the 't' of 'true' as a potential
  // pseudo-structural character just like we would if we had  ' "a string" true '; otherwise we
  // may need to add an extra check when parsing strings.
  //
  // Performance: there are many ways to skin this cat.
  const uint64_t nonquote_scalar = characters.scalar() & ~strings.quote();
  uint64_t follows_nonquote_scalar = follows(nonquote_scalar, prev_scalar);
  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_block(
    strings,// strings is a function-local object so either it moves or the copy is elided.
    characters,
    follows_nonquote_scalar
  );
}

simdjson_inline error_code json_scanner::finish() {
  return string_scanner.finish();
}

} // namespace stage1
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/json_scanner.h */
/* begin file src/generic/stage1/json_minifier.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

class json_minifier {
public:
  template<size_t STEP_SIZE>
  static error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept;

private:
  simdjson_inline json_minifier(uint8_t *_dst)
  : dst{_dst}
  {}
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block_buf, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block);
  simdjson_inline error_code finish(uint8_t *dst_start, size_t &dst_len);
  json_scanner scanner{};
  uint8_t *dst;
};

simdjson_inline void json_minifier::next(const simd::simd8x64<uint8_t>& in, const json_block& block) {
  uint64_t mask = block.whitespace();
  dst += in.compress(mask, dst);
}

simdjson_inline error_code json_minifier::finish(uint8_t *dst_start, size_t &dst_len) {
  error_code error = scanner.finish();
  if (error) { dst_len = 0; return error; }
  dst_len = dst - dst_start;
  return SUCCESS;
}

template<>
simdjson_inline void json_minifier::step<128>(const uint8_t *block_buf, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  simd::simd8x64<uint8_t> in_2(block_buf+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1);
  this->next(in_2, block_2);
  reader.advance();
}

template<>
simdjson_inline void json_minifier::step<64>(const uint8_t *block_buf, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  json_block block_1 = scanner.next(in_1);
  this->next(block_buf, block_1);
  reader.advance();
}

template<size_t STEP_SIZE>
error_code json_minifier::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_minifier minifier(dst);

  // Index the first n-1 blocks
  while (reader.has_full_block()) {
    minifier.step<STEP_SIZE>(reader.full_block(), reader);
  }

  // Index the last (remainder) block, padded with spaces
  uint8_t block[STEP_SIZE];
  size_t remaining_bytes = reader.get_remainder(block);
  if (remaining_bytes > 0) {
    // We do not want to write directly to the output stream. Rather, we write
    // to a local buffer (for safety).
    uint8_t out_block[STEP_SIZE];
    uint8_t * const guarded_dst{minifier.dst};
    minifier.dst = out_block;
    minifier.step<STEP_SIZE>(block, reader);
    size_t to_write = minifier.dst - out_block;
    // In some cases, we could be enticed to consider the padded spaces
    // as part of the string. This is fine as long as we do not write more
    // than we consumed.
    if(to_write > remaining_bytes) { to_write = remaining_bytes; }
    memcpy(guarded_dst, out_block, to_write);
    minifier.dst = guarded_dst + to_write;
  }
  return minifier.finish(dst, dst_len);
}

} // namespace stage1
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/json_minifier.h */
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace arm64 {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

class bit_indexer {
public:
  uint32_t *tail;

  simdjson_inline bit_indexer(uint32_t *index_buf) : tail(index_buf) {}

  // flatten out values in 'bits' assuming that they are are to have values of idx
  // plus their position in the bitvector, and store these indexes at
  // base_ptr[base] incrementing base as we go
  // will potentially store extra values beyond end of valid bits, so base_ptr
  // needs to be large enough to handle this
  //
  // If the kernel sets SIMDJSON_CUSTOM_BIT_INDEXER, then it will provide its own
  // version of the code.
#ifdef SIMDJSON_CUSTOM_BIT_INDEXER
  simdjson_inline void write(uint32_t idx, uint64_t bits);
#else
  simdjson_inline void write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0)
        return;
#if SIMDJSON_PREFER_REVERSE_BITS
    /**
     * ARM lacks a fast trailing zero instruction, but it has a fast
     * bit reversal instruction and a fast leading zero instruction.
     * Thus it may be profitable to reverse the bits (once) and then
     * to rely on a sequence of instructions that call the leading
     * zero instruction.
     *
     * Performance notes:
     * The chosen routine is not optimal in terms of data dependency
     * since zero_leading_bit might require two instructions. However,
     * it tends to minimize the total number of instructions which is
     * beneficial.
     */

    uint64_t rev_bits = reverse_bits(bits);
    int cnt = static_cast<int>(count_ones(bits));
    int i = 0;
    // Do the first 8 all together
    for (; i<8; i++) {
      int lz = leading_zeroes(rev_bits);
      this->tail[i] = static_cast<uint32_t>(idx) + lz;
      rev_bits = zero_leading_bit(rev_bits, lz);
    }
    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      i = 8;
      for (; i<16; i++) {
        int lz = leading_zeroes(rev_bits);
        this->tail[i] = static_cast<uint32_t>(idx) + lz;
        rev_bits = zero_leading_bit(rev_bits, lz);
      }


      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        i = 16;
        while (rev_bits != 0) {
          int lz = leading_zeroes(rev_bits);
          this->tail[i++] = static_cast<uint32_t>(idx) + lz;
          rev_bits = zero_leading_bit(rev_bits, lz);
        }
      }
    }
    this->tail += cnt;
#else // SIMDJSON_PREFER_REVERSE_BITS
    /**
     * Under recent x64 systems, we often have both a fast trailing zero
     * instruction and a fast 'clear-lower-bit' instruction so the following
     * algorithm can be competitive.
     */

    int cnt = static_cast<int>(count_ones(bits));
    // Do the first 8 all together
    for (int i=0; i<8; i++) {
      this->tail[i] = idx + trailing_zeroes(bits);
      bits = clear_lowest_bit(bits);
    }

    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      for (int i=8; i<16; i++) {
        this->tail[i] = idx + trailing_zeroes(bits);
        bits = clear_lowest_bit(bits);
      }

      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        int i = 16;
        do {
          this->tail[i] = idx + trailing_zeroes(bits);
          bits = clear_lowest_bit(bits);
          i++;
        } while (i < cnt);
      }
    }

    this->tail += cnt;
#endif
  }
#endif // SIMDJSON_CUSTOM_BIT_INDEXER

};

class json_structural_indexer {
public:
  /**
   * Find the important bits of JSON in a 128-byte chunk, and add them to structural_indexes.
   *
   * @param partial Setting the partial parameter to true allows the find_structural_bits to
   *   tolerate unclosed strings. The caller should still ensure that the input is valid UTF-8. If
   *   you are processing substrings, you may want to call on a function like trimmed_length_safe_utf8.
   */
  template<size_t STEP_SIZE>
  static error_code index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept;

private:
  simdjson_inline json_structural_indexer(uint32_t *structural_indexes);
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx);
  simdjson_inline error_code finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial);

  json_scanner scanner{};
  utf8_checker checker{};
  bit_indexer indexer;
  uint64_t prev_structurals = 0;
  uint64_t unescaped_chars_error = 0;
};

simdjson_inline json_structural_indexer::json_structural_indexer(uint32_t *structural_indexes) : indexer{structural_indexes} {}

// Skip the last character if it is partial
simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 2 bytes left
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        return len;
      case 0:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
  if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 1 byte left
  if (buf[len-3] >= 0xf0) { return len-3; } // 4-byte characters with only 3 bytes left
  return len;
}

//
// PERF NOTES:
// We pipe 2 inputs through these stages:
// 1. Load JSON into registers. This takes a long time and is highly parallelizable, so we load
//    2 inputs' worth at once so that by the time step 2 is looking for them input, it's available.
// 2. Scan the JSON for critical data: strings, scalars and operators. This is the critical path.
//    The output of step 1 depends entirely on this information. These functions don't quite use
//    up enough CPU: the second half of the functions is highly serial, only using 1 execution core
//    at a time. The second input's scans has some dependency on the first ones finishing it, but
//    they can make a lot of progress before they need that information.
// 3. Step 1 doesn't use enough capacity, so we run some extra stuff while we're waiting for that
//    to finish: utf-8 checks and generating the output from the last iteration.
//
// The reason we run 2 inputs at a time, is steps 2 and 3 are *still* not enough to soak up all
// available capacity with just one input. Running 2 at a time seems to give the CPU a good enough
// workout.
//
template<size_t STEP_SIZE>
error_code json_structural_indexer::index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept {
  if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
  // We guard the rest of the code so that we can assume that len > 0 throughout.
  if (len == 0) { return EMPTY; }
  if (is_streaming(partial)) {
    len = trim_partial_utf8(buf, len);
    // If you end up with an empty window after trimming
    // the partial UTF-8 bytes, then chances are good that you
    // have an UTF-8 formatting error.
    if(len == 0) { return UTF8_ERROR; }
  }
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_structural_indexer indexer(parser.structural_indexes.get());

  // Read all but the last block
  while (reader.has_full_block()) {
    indexer.step<STEP_SIZE>(reader.full_block(), reader);
  }
  // Take care of the last block (will always be there unless file is empty which is
  // not supposed to happen.)
  uint8_t block[STEP_SIZE];
  if (simdjson_unlikely(reader.get_remainder(block) == 0)) { return UNEXPECTED_ERROR; }
  indexer.step<STEP_SIZE>(block, reader);
  return indexer.finish(parser, reader.block_index(), len, partial);
}

template<>
simdjson_inline void json_structural_indexer::step<128>(const uint8_t *block, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  simd::simd8x64<uint8_t> in_2(block+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1, reader.block_index());
  this->next(in_2, block_2, reader.block_index()+64);
  reader.advance();
}

template<>
simdjson_inline void json_structural_indexer::step<64>(const uint8_t *block, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  json_block block_1 = scanner.next(in_1);
  this->next(in_1, block_1, reader.block_index());
  reader.advance();
}

simdjson_inline void json_structural_indexer::next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx) {
  uint64_t unescaped = in.lteq(0x1F);
#if SIMDJSON_UTF8VALIDATION
  checker.check_next_input(in);
#endif
  indexer.write(uint32_t(idx-64), prev_structurals); // Output *last* iteration's structurals to the parser
  prev_structurals = block.structural_start();
  unescaped_chars_error |= block.non_quote_inside_string(unescaped);
}

simdjson_inline error_code json_structural_indexer::finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial) {
  // Write out the final iteration's structurals
  indexer.write(uint32_t(idx-64), prev_structurals);
  error_code error = scanner.finish();
  // We deliberately break down the next expression so that it is
  // human readable.
  const bool should_we_exit = is_streaming(partial) ?
    ((error != SUCCESS) && (error != UNCLOSED_STRING)) // when partial we tolerate UNCLOSED_STRING
    : (error != SUCCESS); // if partial is false, we must have SUCCESS
  const bool have_unclosed_string = (error == UNCLOSED_STRING);
  if (simdjson_unlikely(should_we_exit)) { return error; }

  if (unescaped_chars_error) {
    return UNESCAPED_CHARS;
  }
  parser.n_structural_indexes = uint32_t(indexer.tail - parser.structural_indexes.get());
  /***
   * The On Demand API requires special padding.
   *
   * This is related to https://github.com/simdjson/simdjson/issues/906
   * Basically, we want to make sure that if the parsing continues beyond the last (valid)
   * structural character, it quickly stops.
   * Only three structural characters can be repeated without triggering an error in JSON:  [,] and }.
   * We repeat the padding character (at 'len'). We don't know what it is, but if the parsing
   * continues, then it must be [,] or }.
   * Suppose it is ] or }. We backtrack to the first character, what could it be that would
   * not trigger an error? It could be ] or } but no, because you can't start a document that way.
   * It can't be a comma, a colon or any simple value. So the only way we could continue is
   * if the repeated character is [. But if so, the document must start with [. But if the document
   * starts with [, it should end with ]. If we enforce that rule, then we would get
   * ][[ which is invalid.
   *
   * This is illustrated with the test array_iterate_unclosed_error() on the following input:
   * R"({ "a": [,,)"
   **/
  parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len); // used later in partial == stage1_mode::streaming_final
  parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
  parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
  parser.next_structural_index = 0;
  // a valid JSON file cannot have zero structural indexes - we should have found something
  if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
    return EMPTY;
  }
  if (simdjson_unlikely(parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
    return UNEXPECTED_ERROR;
  }
  if (partial == stage1_mode::streaming_partial) {
    // If we have an unclosed string, then the last structural
    // will be the quote and we want to make sure to omit it.
    if(have_unclosed_string) {
      parser.n_structural_indexes--;
      // a valid JSON file cannot have zero structural indexes - we should have found something
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }

    parser.n_structural_indexes = new_structural_indexes;
  } else if (partial == stage1_mode::streaming_final) {
    if(have_unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
        // We tolerate an unclosed string at the very end of the stream. Indeed, users
        // often load their data in bulk without being careful and they want us to ignore
        // the trailing garbage.
        return EMPTY;
    }
  }
  checker.check_eof();
  return checker.errors();
}

} // namespace stage1
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/json_structural_indexer.h */
/* begin file src/generic/stage1/utf8_validator.h */
namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

/**
 * Validates that the string is actual UTF-8.
 */
template<class checker>
bool generic_validate_utf8(const uint8_t * input, size_t length) {
    checker c{};
    buf_block_reader<64> reader(input, length);
    while (reader.has_full_block()) {
      simd::simd8x64<uint8_t> in(reader.full_block());
      c.check_next_input(in);
      reader.advance();
    }
    uint8_t block[64]{};
    reader.get_remainder(block);
    simd::simd8x64<uint8_t> in(block);
    c.check_next_input(in);
    reader.advance();
    c.check_eof();
    return c.errors() == error_code::SUCCESS;
}

bool generic_validate_utf8(const char * input, size_t length) {
    return generic_validate_utf8<utf8_checker>(reinterpret_cast<const uint8_t *>(input),length);
}

} // namespace stage1
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage1/utf8_validator.h */

//
// Stage 2
//

/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace arm64 {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace arm64 {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace arm64 {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace arm64 {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace arm64 {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace arm64
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

//
// Implementation-specific overrides
//
namespace simdjson {
namespace arm64 {
namespace {
namespace stage1 {

simdjson_inline uint64_t json_string_scanner::find_escaped(uint64_t backslash) {
  // On ARM, we don't short-circuit this if there are no backslashes, because the branch gives us no
  // benefit and therefore makes things worse.
  // if (!backslash) { uint64_t escaped = prev_escaped; prev_escaped = 0; return escaped; }
  return find_escaped_branchless(backslash);
}

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return arm64::stage1::json_minifier::minify<64>(buf, len, dst, dst_len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return arm64::stage1::json_structural_indexer::index<64>(buf, len, *this, streaming);
}

simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return arm64::stage1::generic_validate_utf8(buf,len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) const noexcept {
  return arm64::stringparsing::parse_string(src, dst, allow_replacement);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return arm64::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace arm64
} // namespace simdjson

/* begin file include/simdjson/arm64/end.h */
/* end file include/simdjson/arm64/end.h */
/* end file src/arm64/dom_parser_implementation.cpp */
#endif
#if SIMDJSON_IMPLEMENTATION_FALLBACK
/* begin file src/fallback/implementation.cpp */
/* begin file include/simdjson/fallback/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "fallback"
// #define SIMDJSON_IMPLEMENTATION fallback
/* end file include/simdjson/fallback/begin.h */
namespace simdjson {
namespace fallback {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace fallback
} // namespace simdjson

/* begin file include/simdjson/fallback/end.h */
/* end file include/simdjson/fallback/end.h */
/* end file src/fallback/implementation.cpp */
/* begin file src/fallback/dom_parser_implementation.cpp */
/* begin file include/simdjson/fallback/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "fallback"
// #define SIMDJSON_IMPLEMENTATION fallback
/* end file include/simdjson/fallback/begin.h */

//
// Stage 1
//
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace fallback {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace fallback {
namespace {
namespace stage1 {

class structural_scanner {
public:

simdjson_inline structural_scanner(dom_parser_implementation &_parser, stage1_mode _partial)
  : buf{_parser.buf},
    next_structural_index{_parser.structural_indexes.get()},
    parser{_parser},
    len{static_cast<uint32_t>(_parser.len)},
    partial{_partial} {
}

simdjson_inline void add_structural() {
  *next_structural_index = idx;
  next_structural_index++;
}

simdjson_inline bool is_continuation(uint8_t c) {
  return (c & 0xc0) == 0x80;
}

simdjson_inline void validate_utf8_character() {
  // Continuation
  if (simdjson_unlikely((buf[idx] & 0x40) == 0)) {
    // extra continuation
    error = UTF8_ERROR;
    idx++;
    return;
  }

  // 2-byte
  if ((buf[idx] & 0x20) == 0) {
    // missing continuation
    if (simdjson_unlikely(idx+1 > len || !is_continuation(buf[idx+1]))) {
      if (idx+1 > len && is_streaming(partial)) { idx = len; return; }
      error = UTF8_ERROR;
      idx++;
      return;
    }
    // overlong: 1100000_ 10______
    if (buf[idx] <= 0xc1) { error = UTF8_ERROR; }
    idx += 2;
    return;
  }

  // 3-byte
  if ((buf[idx] & 0x10) == 0) {
    // missing continuation
    if (simdjson_unlikely(idx+2 > len || !is_continuation(buf[idx+1]) || !is_continuation(buf[idx+2]))) {
      if (idx+2 > len && is_streaming(partial)) { idx = len; return; }
      error = UTF8_ERROR;
      idx++;
      return;
    }
    // overlong: 11100000 100_____ ________
    if (buf[idx] == 0xe0 && buf[idx+1] <= 0x9f) { error = UTF8_ERROR; }
    // surrogates: U+D800-U+DFFF 11101101 101_____
    if (buf[idx] == 0xed && buf[idx+1] >= 0xa0) { error = UTF8_ERROR; }
    idx += 3;
    return;
  }

  // 4-byte
  // missing continuation
  if (simdjson_unlikely(idx+3 > len || !is_continuation(buf[idx+1]) || !is_continuation(buf[idx+2]) || !is_continuation(buf[idx+3]))) {
    if (idx+2 > len && is_streaming(partial)) { idx = len; return; }
    error = UTF8_ERROR;
    idx++;
    return;
  }
  // overlong: 11110000 1000____ ________ ________
  if (buf[idx] == 0xf0 && buf[idx+1] <= 0x8f) { error = UTF8_ERROR; }
  // too large: > U+10FFFF:
  // 11110100 (1001|101_)____
  // 1111(1___|011_|0101) 10______
  // also includes 5, 6, 7 and 8 byte characters:
  // 11111___
  if (buf[idx] == 0xf4 && buf[idx+1] >= 0x90) { error = UTF8_ERROR; }
  if (buf[idx] >= 0xf5) { error = UTF8_ERROR; }
  idx += 4;
}

// Returns true if the string is unclosed.
simdjson_inline bool validate_string() {
  idx++; // skip first quote
  while (idx < len && buf[idx] != '"') {
    if (buf[idx] == '\\') {
      idx += 2;
    } else if (simdjson_unlikely(buf[idx] & 0x80)) {
      validate_utf8_character();
    } else {
      if (buf[idx] < 0x20) { error = UNESCAPED_CHARS; }
      idx++;
    }
  }
  if (idx >= len) { return true; }
  return false;
}

simdjson_inline bool is_whitespace_or_operator(uint8_t c) {
  switch (c) {
    case '{': case '}': case '[': case ']': case ',': case ':':
    case ' ': case '\r': case '\n': case '\t':
      return true;
    default:
      return false;
  }
}

//
// Parse the entire input in STEP_SIZE-byte chunks.
//
simdjson_inline error_code scan() {
  bool unclosed_string = false;
  for (;idx<len;idx++) {
    switch (buf[idx]) {
      // String
      case '"':
        add_structural();
        unclosed_string |= validate_string();
        break;
      // Operator
      case '{': case '}': case '[': case ']': case ',': case ':':
        add_structural();
        break;
      // Whitespace
      case ' ': case '\r': case '\n': case '\t':
        break;
      // Primitive or invalid character (invalid characters will be checked in stage 2)
      default:
        // Anything else, add the structural and go until we find the next one
        add_structural();
        while (idx+1<len && !is_whitespace_or_operator(buf[idx+1])) {
          idx++;
        };
        break;
    }
  }
  // We pad beyond.
  // https://github.com/simdjson/simdjson/issues/906
  // See json_structural_indexer.h for an explanation.
  *next_structural_index = len; // assumed later in partial == stage1_mode::streaming_final
  next_structural_index[1] = len;
  next_structural_index[2] = 0;
  parser.n_structural_indexes = uint32_t(next_structural_index - parser.structural_indexes.get());
  if (simdjson_unlikely(parser.n_structural_indexes == 0)) { return EMPTY; }
  parser.next_structural_index = 0;
  if (partial == stage1_mode::streaming_partial) {
    if(unclosed_string) {
      parser.n_structural_indexes--;
      if (simdjson_unlikely(parser.n_structural_indexes == 0)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }
    parser.n_structural_indexes = new_structural_indexes;
  } else if(partial == stage1_mode::streaming_final) {
    if(unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (parser.n_structural_indexes == 0) { return EMPTY; }
  } else if(unclosed_string) { error = UNCLOSED_STRING; }
  return error;
}

private:
  const uint8_t *buf;
  uint32_t *next_structural_index;
  dom_parser_implementation &parser;
  uint32_t len;
  uint32_t idx{0};
  error_code error{SUCCESS};
  stage1_mode partial;
}; // structural_scanner

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode partial) noexcept {
  this->buf = _buf;
  this->len = _len;
  stage1::structural_scanner scanner(*this, partial);
  return scanner.scan();
}

// big table for the minifier
static uint8_t jump_table[256 * 3] = {
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1,
    1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
    0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1,
};

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  size_t i = 0, pos = 0;
  uint8_t quote = 0;
  uint8_t nonescape = 1;

  while (i < len) {
    unsigned char c = buf[i];
    uint8_t *meta = jump_table + 3 * c;

    quote = quote ^ (meta[0] & nonescape);
    dst[pos] = c;
    pos += meta[2] | quote;

    i += 1;
    nonescape = uint8_t(~nonescape) | (meta[1]);
  }
  dst_len = pos; // we intentionally do not work with a reference
  // for fear of aliasing
  return quote ? UNCLOSED_STRING : SUCCESS;
}

// credit: based on code from Google Fuchsia (Apache Licensed)
simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  const uint8_t *data = reinterpret_cast<const uint8_t *>(buf);
  uint64_t pos = 0;
  uint32_t code_point = 0;
  while (pos < len) {
    // check of the next 8 bytes are ascii.
    uint64_t next_pos = pos + 16;
    if (next_pos <= len) { // if it is safe to read 8 more bytes, check that they are ascii
      uint64_t v1;
      memcpy(&v1, data + pos, sizeof(uint64_t));
      uint64_t v2;
      memcpy(&v2, data + pos + sizeof(uint64_t), sizeof(uint64_t));
      uint64_t v{v1 | v2};
      if ((v & 0x8080808080808080) == 0) {
        pos = next_pos;
        continue;
      }
    }
    unsigned char byte = data[pos];
    if (byte < 0x80) {
      pos++;
      continue;
    } else if ((byte & 0xe0) == 0xc0) {
      next_pos = pos + 2;
      if (next_pos > len) { return false; }
      if ((data[pos + 1] & 0xc0) != 0x80) { return false; }
      // range check
      code_point = (byte & 0x1f) << 6 | (data[pos + 1] & 0x3f);
      if (code_point < 0x80 || 0x7ff < code_point) { return false; }
    } else if ((byte & 0xf0) == 0xe0) {
      next_pos = pos + 3;
      if (next_pos > len) { return false; }
      if ((data[pos + 1] & 0xc0) != 0x80) { return false; }
      if ((data[pos + 2] & 0xc0) != 0x80) { return false; }
      // range check
      code_point = (byte & 0x0f) << 12 |
                   (data[pos + 1] & 0x3f) << 6 |
                   (data[pos + 2] & 0x3f);
      if (code_point < 0x800 || 0xffff < code_point ||
          (0xd7ff < code_point && code_point < 0xe000)) {
        return false;
      }
    } else if ((byte & 0xf8) == 0xf0) { // 0b11110000
      next_pos = pos + 4;
      if (next_pos > len) { return false; }
      if ((data[pos + 1] & 0xc0) != 0x80) { return false; }
      if ((data[pos + 2] & 0xc0) != 0x80) { return false; }
      if ((data[pos + 3] & 0xc0) != 0x80) { return false; }
      // range check
      code_point =
          (byte & 0x07) << 18 | (data[pos + 1] & 0x3f) << 12 |
          (data[pos + 2] & 0x3f) << 6 | (data[pos + 3] & 0x3f);
      if (code_point <= 0xffff || 0x10ffff < code_point) { return false; }
    } else {
      // we may have a continuation
      return false;
    }
    pos = next_pos;
  }
  return true;
}

} // namespace fallback
} // namespace simdjson

//
// Stage 2
//
/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace fallback {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace fallback {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace fallback {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace fallback {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace fallback {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace fallback
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

namespace simdjson {
namespace fallback {

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool replacement_char) const noexcept {
  return fallback::stringparsing::parse_string(src, dst, replacement_char);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return fallback::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace fallback
} // namespace simdjson

/* begin file include/simdjson/fallback/end.h */
/* end file include/simdjson/fallback/end.h */
/* end file src/fallback/dom_parser_implementation.cpp */
#endif
#if SIMDJSON_IMPLEMENTATION_ICELAKE
/* begin file src/icelake/implementation.cpp */
/* begin file include/simdjson/icelake/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "icelake"
// #define SIMDJSON_IMPLEMENTATION icelake
SIMDJSON_TARGET_ICELAKE
/* end file include/simdjson/icelake/begin.h */

namespace simdjson {
namespace icelake {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace icelake
} // namespace simdjson

/* begin file include/simdjson/icelake/end.h */
SIMDJSON_UNTARGET_ICELAKE
/* end file include/simdjson/icelake/end.h */

/* end file src/icelake/implementation.cpp */
/* begin file src/icelake/dom_parser_implementation.cpp */
/* begin file include/simdjson/icelake/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "icelake"
// #define SIMDJSON_IMPLEMENTATION icelake
SIMDJSON_TARGET_ICELAKE
/* end file include/simdjson/icelake/begin.h */

//
// Stage 1
//

namespace simdjson {
namespace icelake {
namespace {

using namespace simd;

struct json_character_block {
  static simdjson_inline json_character_block classify(const simd::simd8x64<uint8_t>& in);
  //  ASCII white-space ('\r','\n','\t',' ')
  simdjson_inline uint64_t whitespace() const noexcept;
  // non-quote structural characters (comma, colon, braces, brackets)
  simdjson_inline uint64_t op() const noexcept;
  // neither a structural character nor a white-space, so letters, numbers and quotes
  simdjson_inline uint64_t scalar() const noexcept;

  uint64_t _whitespace; // ASCII white-space ('\r','\n','\t',' ')
  uint64_t _op; // structural characters (comma, colon, braces, brackets but not quotes)
};

simdjson_inline uint64_t json_character_block::whitespace() const noexcept { return _whitespace; }
simdjson_inline uint64_t json_character_block::op() const noexcept { return _op; }
simdjson_inline uint64_t json_character_block::scalar() const noexcept { return ~(op() | whitespace()); }

// This identifies structural characters (comma, colon, braces, brackets),
// and ASCII white-space ('\r','\n','\t',' ').
simdjson_inline json_character_block json_character_block::classify(const simd::simd8x64<uint8_t>& in) {
  // These lookups rely on the fact that anything < 127 will match the lower 4 bits, which is why
  // we can't use the generic lookup_16.
  const auto whitespace_table = simd8<uint8_t>::repeat_16(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100);

  // The 6 operators (:,[]{}) have these values:
  //
  // , 2C
  // : 3A
  // [ 5B
  // { 7B
  // ] 5D
  // } 7D
  //
  // If you use | 0x20 to turn [ and ] into { and }, the lower 4 bits of each character is unique.
  // We exploit this, using a simd 4-bit lookup to tell us which character match against, and then
  // match it (against | 0x20).
  //
  // To prevent recognizing other characters, everything else gets compared with 0, which cannot
  // match due to the | 0x20.
  //
  // NOTE: Due to the | 0x20, this ALSO treats <FF> and <SUB> (control characters 0C and 1A) like ,
  // and :. This gets caught in stage 2, which checks the actual character to ensure the right
  // operators are in the right places.
  const auto op_table = simd8<uint8_t>::repeat_16(
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, ':', '{', // : = 3A, [ = 5B, { = 7B
    ',', '}', 0, 0  // , = 2C, ] = 5D, } = 7D
  );

  // We compute whitespace and op separately. If later code only uses one or the
  // other, given the fact that all functions are aggressively inlined, we can
  // hope that useless computations will be omitted. This is namely case when
  // minifying (we only need whitespace).

  const uint64_t whitespace = in.eq({
    _mm512_shuffle_epi8(whitespace_table, in.chunks[0])
  });
  // Turn [ and ] into { and }
  const simd8x64<uint8_t> curlified{
    in.chunks[0] | 0x20
  };
  const uint64_t op = curlified.eq({
    _mm512_shuffle_epi8(op_table, in.chunks[0])
  });

  return { whitespace, op };
}

simdjson_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

simdjson_unused simdjson_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_second_byte = prev1.saturating_sub(0xc0u-1); // Only 11______ will be > 0
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_second_byte | is_third_byte | is_fourth_byte) > int8_t(0);
}

simdjson_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_third_byte | is_fourth_byte) > int8_t(0);
}

} // unnamed namespace
} // namespace icelake
} // namespace simdjson

/* begin file src/generic/stage1/utf8_lookup4_algorithm.h */
namespace simdjson {
namespace icelake {
namespace {
namespace utf8_validation {

using namespace simd;

  simdjson_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdjson_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }

  //
  // Return nonzero if there are incomplete multibyte characters at the end of the block:
  // e.g. if there is a 4-byte character, but it's 3 bytes from the end.
  //
  simdjson_inline simd8<uint8_t> is_incomplete(const simd8<uint8_t> input) {
    // If the previous input's last 3 bytes match this, they're too short (they ended at EOF):
    // ... 1111____ 111_____ 11______
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    static const uint8_t max_array[64] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#else
    static const uint8_t max_array[32] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#endif
    const simd8<uint8_t> max_value(&max_array[sizeof(max_array)-sizeof(simd8<uint8_t>)]);
    return input.gt_bits(max_value);
  }

  struct utf8_checker {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;
    // The last input we received
    simd8<uint8_t> prev_input_block;
    // Whether the last input we received was incomplete (used for ASCII fast path)
    simd8<uint8_t> prev_incomplete;

    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdjson_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }

    // The only problem that can happen at EOF is that a multibyte character is too short
    // or a byte value too large in the last bytes: check_special_cases only checks for bytes
    // too large in the first of two bytes.
    simdjson_inline void check_eof() {
      // If the previous block had incomplete UTF-8 characters at the end, an ASCII block can't
      // possibly finish them.
      this->error |= this->prev_incomplete;
    }

#ifndef SIMDJSON_IF_CONSTEXPR
#if SIMDJSON_CPLUSPLUS17
#define SIMDJSON_IF_CONSTEXPR if constexpr
#else
#define SIMDJSON_IF_CONSTEXPR if
#endif
#endif

    simdjson_inline void check_next_input(const simd8x64<uint8_t>& input) {
      if(simdjson_likely(is_ascii(input))) {
        this->error |= this->prev_incomplete;
      } else {
        // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
        static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 1)
                ||(simd8x64<uint8_t>::NUM_CHUNKS == 2)
                || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
                "We support one, two or four chunks per 64-byte block.");
        SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 1) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 2) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 4) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
          this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
        }
        this->prev_incomplete = is_incomplete(input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1]);
        this->prev_input_block = input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1];
      }
    }
    // do not forget to call check_eof!
    simdjson_inline error_code errors() {
      return this->error.any_bits_set_anywhere() ? error_code::UTF8_ERROR : error_code::SUCCESS;
    }

  }; // struct utf8_checker
} // namespace utf8_validation

using utf8_validation::utf8_checker;

} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/utf8_lookup4_algorithm.h */
// defining SIMDJSON_CUSTOM_BIT_INDEXER allows us to provide our own bit_indexer::write
#define SIMDJSON_CUSTOM_BIT_INDEXER
/* begin file src/generic/stage1/json_structural_indexer.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

/* begin file src/generic/stage1/buf_block_reader.h */
namespace simdjson {
namespace icelake {
namespace {

// Walks through a buffer in block-sized increments, loading the last part with spaces
template<size_t STEP_SIZE>
struct buf_block_reader {
public:
  simdjson_inline buf_block_reader(const uint8_t *_buf, size_t _len);
  simdjson_inline size_t block_index();
  simdjson_inline bool has_full_block() const;
  simdjson_inline const uint8_t *full_block() const;
  /**
   * Get the last block, padded with spaces.
   *
   * There will always be a last block, with at least 1 byte, unless len == 0 (in which case this
   * function fills the buffer with spaces and returns 0. In particular, if len == STEP_SIZE there
   * will be 0 full_blocks and 1 remainder block with STEP_SIZE bytes and no spaces for padding.
   *
   * @return the number of effective characters in the last block.
   */
  simdjson_inline size_t get_remainder(uint8_t *dst) const;
  simdjson_inline void advance();
private:
  const uint8_t *buf;
  const size_t len;
  const size_t lenminusstep;
  size_t idx;
};

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text_64(const uint8_t *text) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    buf[i] = int8_t(text[i]) < ' ' ? '_' : int8_t(text[i]);
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text(const simd8x64<uint8_t>& in) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  in.store(reinterpret_cast<uint8_t*>(buf));
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    if (buf[i] < ' ') { buf[i] = '_'; }
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

simdjson_unused static char * format_mask(uint64_t mask) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<64; i++) {
    buf[i] = (mask & (size_t(1) << i)) ? 'X' : ' ';
  }
  buf[64] = '\0';
  return buf;
}

template<size_t STEP_SIZE>
simdjson_inline buf_block_reader<STEP_SIZE>::buf_block_reader(const uint8_t *_buf, size_t _len) : buf{_buf}, len{_len}, lenminusstep{len < STEP_SIZE ? 0 : len - STEP_SIZE}, idx{0} {}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::block_index() { return idx; }

template<size_t STEP_SIZE>
simdjson_inline bool buf_block_reader<STEP_SIZE>::has_full_block() const {
  return idx < lenminusstep;
}

template<size_t STEP_SIZE>
simdjson_inline const uint8_t *buf_block_reader<STEP_SIZE>::full_block() const {
  return &buf[idx];
}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::get_remainder(uint8_t *dst) const {
  if(len == idx) { return 0; } // memcpy(dst, null, 0) will trigger an error with some sanitizers
  std::memset(dst, 0x20, STEP_SIZE); // std::memset STEP_SIZE because it's more efficient to write out 8 or 16 bytes at once.
  std::memcpy(dst, buf + idx, len - idx);
  return len - idx;
}

template<size_t STEP_SIZE>
simdjson_inline void buf_block_reader<STEP_SIZE>::advance() {
  idx += STEP_SIZE;
}

} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/buf_block_reader.h */
/* begin file src/generic/stage1/json_string_scanner.h */
namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

struct json_string_block {
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_string_block(uint64_t backslash, uint64_t escaped, uint64_t quote, uint64_t in_string) :
  _backslash(backslash), _escaped(escaped), _quote(quote), _in_string(in_string) {}

  // Escaped characters (characters following an escape() character)
  simdjson_inline uint64_t escaped() const { return _escaped; }
  // Escape characters (backslashes that are not escaped--i.e. in \\, includes only the first \)
  simdjson_inline uint64_t escape() const { return _backslash & ~_escaped; }
  // Real (non-backslashed) quotes
  simdjson_inline uint64_t quote() const { return _quote; }
  // Start quotes of strings
  simdjson_inline uint64_t string_start() const { return _quote & _in_string; }
  // End quotes of strings
  simdjson_inline uint64_t string_end() const { return _quote & ~_in_string; }
  // Only characters inside the string (not including the quotes)
  simdjson_inline uint64_t string_content() const { return _in_string & ~_quote; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const { return mask & _in_string; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const { return mask & ~_in_string; }
  // Tail of string (everything except the start quote)
  simdjson_inline uint64_t string_tail() const { return _in_string ^ _quote; }

  // backslash characters
  uint64_t _backslash;
  // escaped characters (backslashed--does not include the hex characters after \u)
  uint64_t _escaped;
  // real quotes (non-backslashed ones)
  uint64_t _quote;
  // string characters (includes start quote but not end quote)
  uint64_t _in_string;
};

// Scans blocks for string characters, storing the state necessary to do so
class json_string_scanner {
public:
  simdjson_inline json_string_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Intended to be defined by the implementation
  simdjson_inline uint64_t find_escaped(uint64_t escape);
  simdjson_inline uint64_t find_escaped_branchless(uint64_t escape);

  // Whether the last iteration was still inside a string (all 1's = true, all 0's = false).
  uint64_t prev_in_string = 0ULL;
  // Whether the first character of the next iteration is escaped.
  uint64_t prev_escaped = 0ULL;
};

//
// Finds escaped characters (characters following \).
//
// Handles runs of backslashes like \\\" and \\\\" correctly (yielding 0101 and 01010, respectively).
//
// Does this by:
// - Shift the escape mask to get potentially escaped characters (characters after backslashes).
// - Mask escaped sequences that start on *even* bits with 1010101010 (odd bits are escaped, even bits are not)
// - Mask escaped sequences that start on *odd* bits with 0101010101 (even bits are escaped, odd bits are not)
//
// To distinguish between escaped sequences starting on even/odd bits, it finds the start of all
// escape sequences, filters out the ones that start on even bits, and adds that to the mask of
// escape sequences. This causes the addition to clear out the sequences starting on odd bits (since
// the start bit causes a carry), and leaves even-bit sequences alone.
//
// Example:
//
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
// escape         |  xxx |  xx xxx  xxx  xx xx  | Removed overflow backslash; will | it into follows_escape
// odd_starts     |  x   |  x       x       x   | escape & ~even_bits & ~follows_escape
// even_seq       |     c|    cxxx     c xx   c | c = carry bit -- will be masked out later
// invert_mask    |      |     cxxx     c xx   c| even_seq << 1
// follows_escape |   xx | x xx xxx  xxx  xx xx | Includes overflow bit
// escaped        |   x  | x x  x x  x x  x  x  |
// desired        |   x  | x x  x x  x x  x  x  |
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
//
simdjson_inline uint64_t json_string_scanner::find_escaped_branchless(uint64_t backslash) {
  // If there was overflow, pretend the first character isn't a backslash
  backslash &= ~prev_escaped;
  uint64_t follows_escape = backslash << 1 | prev_escaped;

  // Get sequences starting on even bits by clearing out the odd series using +
  const uint64_t even_bits = 0x5555555555555555ULL;
  uint64_t odd_sequence_starts = backslash & ~even_bits & ~follows_escape;
  uint64_t sequences_starting_on_even_bits;
  prev_escaped = add_overflow(odd_sequence_starts, backslash, &sequences_starting_on_even_bits);
  uint64_t invert_mask = sequences_starting_on_even_bits << 1; // The mask we want to return is the *escaped* bits, not escapes.

  // Mask every other backslashed character as an escaped character
  // Flip the mask for sequences that start on even bits, to correct them
  return (even_bits ^ invert_mask) & follows_escape;
}

//
// Return a mask of all string characters plus end quotes.
//
// prev_escaped is overflow saying whether the next character is escaped.
// prev_in_string is overflow saying whether we're still in a string.
//
// Backslash sequences outside of quotes will be detected in stage 2.
//
simdjson_inline json_string_block json_string_scanner::next(const simd::simd8x64<uint8_t>& in) {
  const uint64_t backslash = in.eq('\\');
  const uint64_t escaped = find_escaped(backslash);
  const uint64_t quote = in.eq('"') & ~escaped;

  //
  // prefix_xor flips on bits inside the string (and flips off the end quote).
  //
  // Then we xor with prev_in_string: if we were in a string already, its effect is flipped
  // (characters inside strings are outside, and characters outside strings are inside).
  //
  const uint64_t in_string = prefix_xor(quote) ^ prev_in_string;

  //
  // Check if we're still in a string at the end of the box so the next block will know
  //
  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20, John Regher from Utah U. says this is fine code
  //
  prev_in_string = uint64_t(static_cast<int64_t>(in_string) >> 63);

  // Use ^ to turn the beginning quote off, and the end quote on.

  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_string_block(
    backslash,
    escaped,
    quote,
    in_string
  );
}

simdjson_inline error_code json_string_scanner::finish() {
  if (prev_in_string) {
    return UNCLOSED_STRING;
  }
  return SUCCESS;
}

} // namespace stage1
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/json_string_scanner.h */
/* begin file src/generic/stage1/json_scanner.h */
namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

/**
 * A block of scanned json, with information on operators and scalars.
 *
 * We seek to identify pseudo-structural characters. Anything that is inside
 * a string must be omitted (hence  & ~_string.string_tail()).
 * Otherwise, pseudo-structural characters come in two forms.
 * 1. We have the structural characters ([,],{,},:, comma). The
 *    term 'structural character' is from the JSON RFC.
 * 2. We have the 'scalar pseudo-structural characters'.
 *    Scalars are quotes, and any character except structural characters and white space.
 *
 * To identify the scalar pseudo-structural characters, we must look at what comes
 * before them: it must be a space, a quote or a structural characters.
 * Starting with simdjson v0.3, we identify them by
 * negation: we identify everything that is followed by a non-quote scalar,
 * and we negate that. Whatever remains must be a 'scalar pseudo-structural character'.
 */
struct json_block {
public:
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_block(json_string_block&& string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(std::move(string)), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}
  simdjson_inline json_block(json_string_block string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(string), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}

  /**
   * The start of structurals.
   * In simdjson prior to v0.3, these were called the pseudo-structural characters.
   **/
  simdjson_inline uint64_t structural_start() const noexcept { return potential_structural_start() & ~_string.string_tail(); }
  /** All JSON whitespace (i.e. not in a string) */
  simdjson_inline uint64_t whitespace() const noexcept { return non_quote_outside_string(_characters.whitespace()); }

  // Helpers

  /** Whether the given characters are inside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const noexcept { return _string.non_quote_inside_string(mask); }
  /** Whether the given characters are outside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const noexcept { return _string.non_quote_outside_string(mask); }

  // string and escape characters
  json_string_block _string;
  // whitespace, structural characters ('operators'), scalars
  json_character_block _characters;
  // whether the previous character was a scalar
  uint64_t _follows_potential_nonquote_scalar;
private:
  // Potential structurals (i.e. disregarding strings)

  /**
   * structural elements ([,],{,},:, comma) plus scalar starts like 123, true and "abc".
   * They may reside inside a string.
   **/
  simdjson_inline uint64_t potential_structural_start() const noexcept { return _characters.op() | potential_scalar_start(); }
  /**
   * The start of non-operator runs, like 123, true and "abc".
   * It main reside inside a string.
   **/
  simdjson_inline uint64_t potential_scalar_start() const noexcept {
    // The term "scalar" refers to anything except structural characters and white space
    // (so letters, numbers, quotes).
    // Whenever it is preceded by something that is not a structural element ({,},[,],:, ") nor a white-space
    // then we know that it is irrelevant structurally.
    return _characters.scalar() & ~follows_potential_scalar();
  }
  /**
   * Whether the given character is immediately after a non-operator like 123, true.
   * The characters following a quote are not included.
   */
  simdjson_inline uint64_t follows_potential_scalar() const noexcept {
    // _follows_potential_nonquote_scalar: is defined as marking any character that follows a character
    // that is not a structural element ({,},[,],:, comma) nor a quote (") and that is not a
    // white space.
    // It is understood that within quoted region, anything at all could be marked (irrelevant).
    return _follows_potential_nonquote_scalar;
  }
};

/**
 * Scans JSON for important bits: structural characters or 'operators', strings, and scalars.
 *
 * The scanner starts by calculating two distinct things:
 * - string characters (taking \" into account)
 * - structural characters or 'operators' ([]{},:, comma)
 *   and scalars (runs of non-operators like 123, true and "abc")
 *
 * To minimize data dependency (a key component of the scanner's speed), it finds these in parallel:
 * in particular, the operator/scalar bit will find plenty of things that are actually part of
 * strings. When we're done, json_block will fuse the two together by masking out tokens that are
 * part of a string.
 */
class json_scanner {
public:
  json_scanner() = default;
  simdjson_inline json_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Whether the last character of the previous iteration is part of a scalar token
  // (anything except whitespace or a structural character/'operator').
  uint64_t prev_scalar = 0ULL;
  json_string_scanner string_scanner{};
};


//
// Check if the current character immediately follows a matching character.
//
// For example, this checks for quotes with backslashes in front of them:
//
//     const uint64_t backslashed_quote = in.eq('"') & immediately_follows(in.eq('\'), prev_backslash);
//
simdjson_inline uint64_t follows(const uint64_t match, uint64_t &overflow) {
  const uint64_t result = match << 1 | overflow;
  overflow = match >> 63;
  return result;
}

simdjson_inline json_block json_scanner::next(const simd::simd8x64<uint8_t>& in) {
  json_string_block strings = string_scanner.next(in);
  // identifies the white-space and the structural characters
  json_character_block characters = json_character_block::classify(in);
  // The term "scalar" refers to anything except structural characters and white space
  // (so letters, numbers, quotes).
  // We want follows_scalar to mark anything that follows a non-quote scalar (so letters and numbers).
  //
  // A terminal quote should either be followed by a structural character (comma, brace, bracket, colon)
  // or nothing. However, we still want ' "a string"true ' to mark the 't' of 'true' as a potential
  // pseudo-structural character just like we would if we had  ' "a string" true '; otherwise we
  // may need to add an extra check when parsing strings.
  //
  // Performance: there are many ways to skin this cat.
  const uint64_t nonquote_scalar = characters.scalar() & ~strings.quote();
  uint64_t follows_nonquote_scalar = follows(nonquote_scalar, prev_scalar);
  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_block(
    strings,// strings is a function-local object so either it moves or the copy is elided.
    characters,
    follows_nonquote_scalar
  );
}

simdjson_inline error_code json_scanner::finish() {
  return string_scanner.finish();
}

} // namespace stage1
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/json_scanner.h */
/* begin file src/generic/stage1/json_minifier.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

class json_minifier {
public:
  template<size_t STEP_SIZE>
  static error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept;

private:
  simdjson_inline json_minifier(uint8_t *_dst)
  : dst{_dst}
  {}
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block_buf, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block);
  simdjson_inline error_code finish(uint8_t *dst_start, size_t &dst_len);
  json_scanner scanner{};
  uint8_t *dst;
};

simdjson_inline void json_minifier::next(const simd::simd8x64<uint8_t>& in, const json_block& block) {
  uint64_t mask = block.whitespace();
  dst += in.compress(mask, dst);
}

simdjson_inline error_code json_minifier::finish(uint8_t *dst_start, size_t &dst_len) {
  error_code error = scanner.finish();
  if (error) { dst_len = 0; return error; }
  dst_len = dst - dst_start;
  return SUCCESS;
}

template<>
simdjson_inline void json_minifier::step<128>(const uint8_t *block_buf, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  simd::simd8x64<uint8_t> in_2(block_buf+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1);
  this->next(in_2, block_2);
  reader.advance();
}

template<>
simdjson_inline void json_minifier::step<64>(const uint8_t *block_buf, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  json_block block_1 = scanner.next(in_1);
  this->next(block_buf, block_1);
  reader.advance();
}

template<size_t STEP_SIZE>
error_code json_minifier::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_minifier minifier(dst);

  // Index the first n-1 blocks
  while (reader.has_full_block()) {
    minifier.step<STEP_SIZE>(reader.full_block(), reader);
  }

  // Index the last (remainder) block, padded with spaces
  uint8_t block[STEP_SIZE];
  size_t remaining_bytes = reader.get_remainder(block);
  if (remaining_bytes > 0) {
    // We do not want to write directly to the output stream. Rather, we write
    // to a local buffer (for safety).
    uint8_t out_block[STEP_SIZE];
    uint8_t * const guarded_dst{minifier.dst};
    minifier.dst = out_block;
    minifier.step<STEP_SIZE>(block, reader);
    size_t to_write = minifier.dst - out_block;
    // In some cases, we could be enticed to consider the padded spaces
    // as part of the string. This is fine as long as we do not write more
    // than we consumed.
    if(to_write > remaining_bytes) { to_write = remaining_bytes; }
    memcpy(guarded_dst, out_block, to_write);
    minifier.dst = guarded_dst + to_write;
  }
  return minifier.finish(dst, dst_len);
}

} // namespace stage1
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/json_minifier.h */
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace icelake {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

class bit_indexer {
public:
  uint32_t *tail;

  simdjson_inline bit_indexer(uint32_t *index_buf) : tail(index_buf) {}

  // flatten out values in 'bits' assuming that they are are to have values of idx
  // plus their position in the bitvector, and store these indexes at
  // base_ptr[base] incrementing base as we go
  // will potentially store extra values beyond end of valid bits, so base_ptr
  // needs to be large enough to handle this
  //
  // If the kernel sets SIMDJSON_CUSTOM_BIT_INDEXER, then it will provide its own
  // version of the code.
#ifdef SIMDJSON_CUSTOM_BIT_INDEXER
  simdjson_inline void write(uint32_t idx, uint64_t bits);
#else
  simdjson_inline void write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0)
        return;
#if SIMDJSON_PREFER_REVERSE_BITS
    /**
     * ARM lacks a fast trailing zero instruction, but it has a fast
     * bit reversal instruction and a fast leading zero instruction.
     * Thus it may be profitable to reverse the bits (once) and then
     * to rely on a sequence of instructions that call the leading
     * zero instruction.
     *
     * Performance notes:
     * The chosen routine is not optimal in terms of data dependency
     * since zero_leading_bit might require two instructions. However,
     * it tends to minimize the total number of instructions which is
     * beneficial.
     */

    uint64_t rev_bits = reverse_bits(bits);
    int cnt = static_cast<int>(count_ones(bits));
    int i = 0;
    // Do the first 8 all together
    for (; i<8; i++) {
      int lz = leading_zeroes(rev_bits);
      this->tail[i] = static_cast<uint32_t>(idx) + lz;
      rev_bits = zero_leading_bit(rev_bits, lz);
    }
    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      i = 8;
      for (; i<16; i++) {
        int lz = leading_zeroes(rev_bits);
        this->tail[i] = static_cast<uint32_t>(idx) + lz;
        rev_bits = zero_leading_bit(rev_bits, lz);
      }


      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        i = 16;
        while (rev_bits != 0) {
          int lz = leading_zeroes(rev_bits);
          this->tail[i++] = static_cast<uint32_t>(idx) + lz;
          rev_bits = zero_leading_bit(rev_bits, lz);
        }
      }
    }
    this->tail += cnt;
#else // SIMDJSON_PREFER_REVERSE_BITS
    /**
     * Under recent x64 systems, we often have both a fast trailing zero
     * instruction and a fast 'clear-lower-bit' instruction so the following
     * algorithm can be competitive.
     */

    int cnt = static_cast<int>(count_ones(bits));
    // Do the first 8 all together
    for (int i=0; i<8; i++) {
      this->tail[i] = idx + trailing_zeroes(bits);
      bits = clear_lowest_bit(bits);
    }

    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      for (int i=8; i<16; i++) {
        this->tail[i] = idx + trailing_zeroes(bits);
        bits = clear_lowest_bit(bits);
      }

      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        int i = 16;
        do {
          this->tail[i] = idx + trailing_zeroes(bits);
          bits = clear_lowest_bit(bits);
          i++;
        } while (i < cnt);
      }
    }

    this->tail += cnt;
#endif
  }
#endif // SIMDJSON_CUSTOM_BIT_INDEXER

};

class json_structural_indexer {
public:
  /**
   * Find the important bits of JSON in a 128-byte chunk, and add them to structural_indexes.
   *
   * @param partial Setting the partial parameter to true allows the find_structural_bits to
   *   tolerate unclosed strings. The caller should still ensure that the input is valid UTF-8. If
   *   you are processing substrings, you may want to call on a function like trimmed_length_safe_utf8.
   */
  template<size_t STEP_SIZE>
  static error_code index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept;

private:
  simdjson_inline json_structural_indexer(uint32_t *structural_indexes);
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx);
  simdjson_inline error_code finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial);

  json_scanner scanner{};
  utf8_checker checker{};
  bit_indexer indexer;
  uint64_t prev_structurals = 0;
  uint64_t unescaped_chars_error = 0;
};

simdjson_inline json_structural_indexer::json_structural_indexer(uint32_t *structural_indexes) : indexer{structural_indexes} {}

// Skip the last character if it is partial
simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 2 bytes left
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        return len;
      case 0:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
  if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 1 byte left
  if (buf[len-3] >= 0xf0) { return len-3; } // 4-byte characters with only 3 bytes left
  return len;
}

//
// PERF NOTES:
// We pipe 2 inputs through these stages:
// 1. Load JSON into registers. This takes a long time and is highly parallelizable, so we load
//    2 inputs' worth at once so that by the time step 2 is looking for them input, it's available.
// 2. Scan the JSON for critical data: strings, scalars and operators. This is the critical path.
//    The output of step 1 depends entirely on this information. These functions don't quite use
//    up enough CPU: the second half of the functions is highly serial, only using 1 execution core
//    at a time. The second input's scans has some dependency on the first ones finishing it, but
//    they can make a lot of progress before they need that information.
// 3. Step 1 doesn't use enough capacity, so we run some extra stuff while we're waiting for that
//    to finish: utf-8 checks and generating the output from the last iteration.
//
// The reason we run 2 inputs at a time, is steps 2 and 3 are *still* not enough to soak up all
// available capacity with just one input. Running 2 at a time seems to give the CPU a good enough
// workout.
//
template<size_t STEP_SIZE>
error_code json_structural_indexer::index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept {
  if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
  // We guard the rest of the code so that we can assume that len > 0 throughout.
  if (len == 0) { return EMPTY; }
  if (is_streaming(partial)) {
    len = trim_partial_utf8(buf, len);
    // If you end up with an empty window after trimming
    // the partial UTF-8 bytes, then chances are good that you
    // have an UTF-8 formatting error.
    if(len == 0) { return UTF8_ERROR; }
  }
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_structural_indexer indexer(parser.structural_indexes.get());

  // Read all but the last block
  while (reader.has_full_block()) {
    indexer.step<STEP_SIZE>(reader.full_block(), reader);
  }
  // Take care of the last block (will always be there unless file is empty which is
  // not supposed to happen.)
  uint8_t block[STEP_SIZE];
  if (simdjson_unlikely(reader.get_remainder(block) == 0)) { return UNEXPECTED_ERROR; }
  indexer.step<STEP_SIZE>(block, reader);
  return indexer.finish(parser, reader.block_index(), len, partial);
}

template<>
simdjson_inline void json_structural_indexer::step<128>(const uint8_t *block, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  simd::simd8x64<uint8_t> in_2(block+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1, reader.block_index());
  this->next(in_2, block_2, reader.block_index()+64);
  reader.advance();
}

template<>
simdjson_inline void json_structural_indexer::step<64>(const uint8_t *block, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  json_block block_1 = scanner.next(in_1);
  this->next(in_1, block_1, reader.block_index());
  reader.advance();
}

simdjson_inline void json_structural_indexer::next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx) {
  uint64_t unescaped = in.lteq(0x1F);
#if SIMDJSON_UTF8VALIDATION
  checker.check_next_input(in);
#endif
  indexer.write(uint32_t(idx-64), prev_structurals); // Output *last* iteration's structurals to the parser
  prev_structurals = block.structural_start();
  unescaped_chars_error |= block.non_quote_inside_string(unescaped);
}

simdjson_inline error_code json_structural_indexer::finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial) {
  // Write out the final iteration's structurals
  indexer.write(uint32_t(idx-64), prev_structurals);
  error_code error = scanner.finish();
  // We deliberately break down the next expression so that it is
  // human readable.
  const bool should_we_exit = is_streaming(partial) ?
    ((error != SUCCESS) && (error != UNCLOSED_STRING)) // when partial we tolerate UNCLOSED_STRING
    : (error != SUCCESS); // if partial is false, we must have SUCCESS
  const bool have_unclosed_string = (error == UNCLOSED_STRING);
  if (simdjson_unlikely(should_we_exit)) { return error; }

  if (unescaped_chars_error) {
    return UNESCAPED_CHARS;
  }
  parser.n_structural_indexes = uint32_t(indexer.tail - parser.structural_indexes.get());
  /***
   * The On Demand API requires special padding.
   *
   * This is related to https://github.com/simdjson/simdjson/issues/906
   * Basically, we want to make sure that if the parsing continues beyond the last (valid)
   * structural character, it quickly stops.
   * Only three structural characters can be repeated without triggering an error in JSON:  [,] and }.
   * We repeat the padding character (at 'len'). We don't know what it is, but if the parsing
   * continues, then it must be [,] or }.
   * Suppose it is ] or }. We backtrack to the first character, what could it be that would
   * not trigger an error? It could be ] or } but no, because you can't start a document that way.
   * It can't be a comma, a colon or any simple value. So the only way we could continue is
   * if the repeated character is [. But if so, the document must start with [. But if the document
   * starts with [, it should end with ]. If we enforce that rule, then we would get
   * ][[ which is invalid.
   *
   * This is illustrated with the test array_iterate_unclosed_error() on the following input:
   * R"({ "a": [,,)"
   **/
  parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len); // used later in partial == stage1_mode::streaming_final
  parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
  parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
  parser.next_structural_index = 0;
  // a valid JSON file cannot have zero structural indexes - we should have found something
  if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
    return EMPTY;
  }
  if (simdjson_unlikely(parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
    return UNEXPECTED_ERROR;
  }
  if (partial == stage1_mode::streaming_partial) {
    // If we have an unclosed string, then the last structural
    // will be the quote and we want to make sure to omit it.
    if(have_unclosed_string) {
      parser.n_structural_indexes--;
      // a valid JSON file cannot have zero structural indexes - we should have found something
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }

    parser.n_structural_indexes = new_structural_indexes;
  } else if (partial == stage1_mode::streaming_final) {
    if(have_unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
        // We tolerate an unclosed string at the very end of the stream. Indeed, users
        // often load their data in bulk without being careful and they want us to ignore
        // the trailing garbage.
        return EMPTY;
    }
  }
  checker.check_eof();
  return checker.errors();
}

} // namespace stage1
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/json_structural_indexer.h */
// We must not forget to undefine it now:
#undef SIMDJSON_CUSTOM_BIT_INDEXER

/**
 * We provide a custom version of bit_indexer::write using
 * naked intrinsics.
 * TODO: make this code more elegant.
 */
// Under GCC 12, the intrinsic _mm512_extracti32x4_epi32 may generate 'maybe uninitialized'.
// as a workaround, we disable warnings within the following function.
SIMDJSON_PUSH_DISABLE_ALL_WARNINGS
namespace simdjson { namespace icelake { namespace { namespace stage1 {
simdjson_inline void bit_indexer::write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0) { return; }

    const __m512i indexes = _mm512_maskz_compress_epi8(bits, _mm512_set_epi32(
      0x3f3e3d3c, 0x3b3a3938, 0x37363534, 0x33323130,
      0x2f2e2d2c, 0x2b2a2928, 0x27262524, 0x23222120,
      0x1f1e1d1c, 0x1b1a1918, 0x17161514, 0x13121110,
      0x0f0e0d0c, 0x0b0a0908, 0x07060504, 0x03020100
    ));
    const __m512i start_index = _mm512_set1_epi32(idx);

    const auto count = count_ones(bits);
    __m512i t0 = _mm512_cvtepu8_epi32(_mm512_castsi512_si128(indexes));
    _mm512_storeu_si512(this->tail, _mm512_add_epi32(t0, start_index));

    if(count > 16) {
      const __m512i t1 = _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(indexes, 1));
      _mm512_storeu_si512(this->tail + 16, _mm512_add_epi32(t1, start_index));
      if(count > 32) {
        const __m512i t2 = _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(indexes, 2));
        _mm512_storeu_si512(this->tail + 32, _mm512_add_epi32(t2, start_index));
        if(count > 48) {
          const __m512i t3 = _mm512_cvtepu8_epi32(_mm512_extracti32x4_epi32(indexes, 3));
          _mm512_storeu_si512(this->tail + 48, _mm512_add_epi32(t3, start_index));
        }
      }
    }
    this->tail += count;
}
}}}}
SIMDJSON_POP_DISABLE_WARNINGS

/* begin file src/generic/stage1/utf8_validator.h */
namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

/**
 * Validates that the string is actual UTF-8.
 */
template<class checker>
bool generic_validate_utf8(const uint8_t * input, size_t length) {
    checker c{};
    buf_block_reader<64> reader(input, length);
    while (reader.has_full_block()) {
      simd::simd8x64<uint8_t> in(reader.full_block());
      c.check_next_input(in);
      reader.advance();
    }
    uint8_t block[64]{};
    reader.get_remainder(block);
    simd::simd8x64<uint8_t> in(block);
    c.check_next_input(in);
    reader.advance();
    c.check_eof();
    return c.errors() == error_code::SUCCESS;
}

bool generic_validate_utf8(const char * input, size_t length) {
    return generic_validate_utf8<utf8_checker>(reinterpret_cast<const uint8_t *>(input),length);
}

} // namespace stage1
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage1/utf8_validator.h */

//
// Stage 2
//
/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace icelake {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace icelake {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace icelake {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace icelake {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace icelake {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace icelake
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

//
// Implementation-specific overrides
//
namespace simdjson {
namespace icelake {
namespace {
namespace stage1 {

simdjson_inline uint64_t json_string_scanner::find_escaped(uint64_t backslash) {
  if (!backslash) { uint64_t escaped = prev_escaped; prev_escaped = 0; return escaped; }
  return find_escaped_branchless(backslash);
}

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return icelake::stage1::json_minifier::minify<128>(buf, len, dst, dst_len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return icelake::stage1::json_structural_indexer::index<128>(_buf, _len, *this, streaming);
}

simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return icelake::stage1::generic_validate_utf8(buf,len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool replacement_char) const noexcept {
  return icelake::stringparsing::parse_string(src, dst, replacement_char);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return icelake::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace icelake
} // namespace simdjson

/* begin file include/simdjson/icelake/end.h */
SIMDJSON_UNTARGET_ICELAKE
/* end file include/simdjson/icelake/end.h */
/* end file src/icelake/dom_parser_implementation.cpp */
#endif
#if SIMDJSON_IMPLEMENTATION_HASWELL
/* begin file src/haswell/implementation.cpp */
/* begin file include/simdjson/haswell/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "haswell"
// #define SIMDJSON_IMPLEMENTATION haswell
SIMDJSON_TARGET_HASWELL
/* end file include/simdjson/haswell/begin.h */

namespace simdjson {
namespace haswell {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace haswell
} // namespace simdjson

/* begin file include/simdjson/haswell/end.h */
SIMDJSON_UNTARGET_HASWELL
/* end file include/simdjson/haswell/end.h */

/* end file src/haswell/implementation.cpp */
/* begin file src/haswell/dom_parser_implementation.cpp */
/* begin file include/simdjson/haswell/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "haswell"
// #define SIMDJSON_IMPLEMENTATION haswell
SIMDJSON_TARGET_HASWELL
/* end file include/simdjson/haswell/begin.h */

//
// Stage 1
//

namespace simdjson {
namespace haswell {
namespace {

using namespace simd;

struct json_character_block {
  static simdjson_inline json_character_block classify(const simd::simd8x64<uint8_t>& in);
  //  ASCII white-space ('\r','\n','\t',' ')
  simdjson_inline uint64_t whitespace() const noexcept;
  // non-quote structural characters (comma, colon, braces, brackets)
  simdjson_inline uint64_t op() const noexcept;
  // neither a structural character nor a white-space, so letters, numbers and quotes
  simdjson_inline uint64_t scalar() const noexcept;

  uint64_t _whitespace; // ASCII white-space ('\r','\n','\t',' ')
  uint64_t _op; // structural characters (comma, colon, braces, brackets but not quotes)
};

simdjson_inline uint64_t json_character_block::whitespace() const noexcept { return _whitespace; }
simdjson_inline uint64_t json_character_block::op() const noexcept { return _op; }
simdjson_inline uint64_t json_character_block::scalar() const noexcept { return ~(op() | whitespace()); }

// This identifies structural characters (comma, colon, braces, brackets),
// and ASCII white-space ('\r','\n','\t',' ').
simdjson_inline json_character_block json_character_block::classify(const simd::simd8x64<uint8_t>& in) {
  // These lookups rely on the fact that anything < 127 will match the lower 4 bits, which is why
  // we can't use the generic lookup_16.
  const auto whitespace_table = simd8<uint8_t>::repeat_16(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100);

  // The 6 operators (:,[]{}) have these values:
  //
  // , 2C
  // : 3A
  // [ 5B
  // { 7B
  // ] 5D
  // } 7D
  //
  // If you use | 0x20 to turn [ and ] into { and }, the lower 4 bits of each character is unique.
  // We exploit this, using a simd 4-bit lookup to tell us which character match against, and then
  // match it (against | 0x20).
  //
  // To prevent recognizing other characters, everything else gets compared with 0, which cannot
  // match due to the | 0x20.
  //
  // NOTE: Due to the | 0x20, this ALSO treats <FF> and <SUB> (control characters 0C and 1A) like ,
  // and :. This gets caught in stage 2, which checks the actual character to ensure the right
  // operators are in the right places.
  const auto op_table = simd8<uint8_t>::repeat_16(
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, ':', '{', // : = 3A, [ = 5B, { = 7B
    ',', '}', 0, 0  // , = 2C, ] = 5D, } = 7D
  );

  // We compute whitespace and op separately. If later code only uses one or the
  // other, given the fact that all functions are aggressively inlined, we can
  // hope that useless computations will be omitted. This is namely case when
  // minifying (we only need whitespace).

  const uint64_t whitespace = in.eq({
    _mm256_shuffle_epi8(whitespace_table, in.chunks[0]),
    _mm256_shuffle_epi8(whitespace_table, in.chunks[1])
  });
  // Turn [ and ] into { and }
  const simd8x64<uint8_t> curlified{
    in.chunks[0] | 0x20,
    in.chunks[1] | 0x20
  };
  const uint64_t op = curlified.eq({
    _mm256_shuffle_epi8(op_table, in.chunks[0]),
    _mm256_shuffle_epi8(op_table, in.chunks[1])
  });

  return { whitespace, op };
}

simdjson_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

simdjson_unused simdjson_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_second_byte = prev1.saturating_sub(0xc0u-1); // Only 11______ will be > 0
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_second_byte | is_third_byte | is_fourth_byte) > int8_t(0);
}

simdjson_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_third_byte | is_fourth_byte) > int8_t(0);
}

} // unnamed namespace
} // namespace haswell
} // namespace simdjson

/* begin file src/generic/stage1/utf8_lookup4_algorithm.h */
namespace simdjson {
namespace haswell {
namespace {
namespace utf8_validation {

using namespace simd;

  simdjson_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdjson_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }

  //
  // Return nonzero if there are incomplete multibyte characters at the end of the block:
  // e.g. if there is a 4-byte character, but it's 3 bytes from the end.
  //
  simdjson_inline simd8<uint8_t> is_incomplete(const simd8<uint8_t> input) {
    // If the previous input's last 3 bytes match this, they're too short (they ended at EOF):
    // ... 1111____ 111_____ 11______
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    static const uint8_t max_array[64] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#else
    static const uint8_t max_array[32] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#endif
    const simd8<uint8_t> max_value(&max_array[sizeof(max_array)-sizeof(simd8<uint8_t>)]);
    return input.gt_bits(max_value);
  }

  struct utf8_checker {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;
    // The last input we received
    simd8<uint8_t> prev_input_block;
    // Whether the last input we received was incomplete (used for ASCII fast path)
    simd8<uint8_t> prev_incomplete;

    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdjson_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }

    // The only problem that can happen at EOF is that a multibyte character is too short
    // or a byte value too large in the last bytes: check_special_cases only checks for bytes
    // too large in the first of two bytes.
    simdjson_inline void check_eof() {
      // If the previous block had incomplete UTF-8 characters at the end, an ASCII block can't
      // possibly finish them.
      this->error |= this->prev_incomplete;
    }

#ifndef SIMDJSON_IF_CONSTEXPR
#if SIMDJSON_CPLUSPLUS17
#define SIMDJSON_IF_CONSTEXPR if constexpr
#else
#define SIMDJSON_IF_CONSTEXPR if
#endif
#endif

    simdjson_inline void check_next_input(const simd8x64<uint8_t>& input) {
      if(simdjson_likely(is_ascii(input))) {
        this->error |= this->prev_incomplete;
      } else {
        // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
        static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 1)
                ||(simd8x64<uint8_t>::NUM_CHUNKS == 2)
                || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
                "We support one, two or four chunks per 64-byte block.");
        SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 1) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 2) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 4) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
          this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
        }
        this->prev_incomplete = is_incomplete(input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1]);
        this->prev_input_block = input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1];
      }
    }
    // do not forget to call check_eof!
    simdjson_inline error_code errors() {
      return this->error.any_bits_set_anywhere() ? error_code::UTF8_ERROR : error_code::SUCCESS;
    }

  }; // struct utf8_checker
} // namespace utf8_validation

using utf8_validation::utf8_checker;

} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/utf8_lookup4_algorithm.h */
/* begin file src/generic/stage1/json_structural_indexer.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

/* begin file src/generic/stage1/buf_block_reader.h */
namespace simdjson {
namespace haswell {
namespace {

// Walks through a buffer in block-sized increments, loading the last part with spaces
template<size_t STEP_SIZE>
struct buf_block_reader {
public:
  simdjson_inline buf_block_reader(const uint8_t *_buf, size_t _len);
  simdjson_inline size_t block_index();
  simdjson_inline bool has_full_block() const;
  simdjson_inline const uint8_t *full_block() const;
  /**
   * Get the last block, padded with spaces.
   *
   * There will always be a last block, with at least 1 byte, unless len == 0 (in which case this
   * function fills the buffer with spaces and returns 0. In particular, if len == STEP_SIZE there
   * will be 0 full_blocks and 1 remainder block with STEP_SIZE bytes and no spaces for padding.
   *
   * @return the number of effective characters in the last block.
   */
  simdjson_inline size_t get_remainder(uint8_t *dst) const;
  simdjson_inline void advance();
private:
  const uint8_t *buf;
  const size_t len;
  const size_t lenminusstep;
  size_t idx;
};

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text_64(const uint8_t *text) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    buf[i] = int8_t(text[i]) < ' ' ? '_' : int8_t(text[i]);
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text(const simd8x64<uint8_t>& in) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  in.store(reinterpret_cast<uint8_t*>(buf));
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    if (buf[i] < ' ') { buf[i] = '_'; }
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

simdjson_unused static char * format_mask(uint64_t mask) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<64; i++) {
    buf[i] = (mask & (size_t(1) << i)) ? 'X' : ' ';
  }
  buf[64] = '\0';
  return buf;
}

template<size_t STEP_SIZE>
simdjson_inline buf_block_reader<STEP_SIZE>::buf_block_reader(const uint8_t *_buf, size_t _len) : buf{_buf}, len{_len}, lenminusstep{len < STEP_SIZE ? 0 : len - STEP_SIZE}, idx{0} {}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::block_index() { return idx; }

template<size_t STEP_SIZE>
simdjson_inline bool buf_block_reader<STEP_SIZE>::has_full_block() const {
  return idx < lenminusstep;
}

template<size_t STEP_SIZE>
simdjson_inline const uint8_t *buf_block_reader<STEP_SIZE>::full_block() const {
  return &buf[idx];
}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::get_remainder(uint8_t *dst) const {
  if(len == idx) { return 0; } // memcpy(dst, null, 0) will trigger an error with some sanitizers
  std::memset(dst, 0x20, STEP_SIZE); // std::memset STEP_SIZE because it's more efficient to write out 8 or 16 bytes at once.
  std::memcpy(dst, buf + idx, len - idx);
  return len - idx;
}

template<size_t STEP_SIZE>
simdjson_inline void buf_block_reader<STEP_SIZE>::advance() {
  idx += STEP_SIZE;
}

} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/buf_block_reader.h */
/* begin file src/generic/stage1/json_string_scanner.h */
namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

struct json_string_block {
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_string_block(uint64_t backslash, uint64_t escaped, uint64_t quote, uint64_t in_string) :
  _backslash(backslash), _escaped(escaped), _quote(quote), _in_string(in_string) {}

  // Escaped characters (characters following an escape() character)
  simdjson_inline uint64_t escaped() const { return _escaped; }
  // Escape characters (backslashes that are not escaped--i.e. in \\, includes only the first \)
  simdjson_inline uint64_t escape() const { return _backslash & ~_escaped; }
  // Real (non-backslashed) quotes
  simdjson_inline uint64_t quote() const { return _quote; }
  // Start quotes of strings
  simdjson_inline uint64_t string_start() const { return _quote & _in_string; }
  // End quotes of strings
  simdjson_inline uint64_t string_end() const { return _quote & ~_in_string; }
  // Only characters inside the string (not including the quotes)
  simdjson_inline uint64_t string_content() const { return _in_string & ~_quote; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const { return mask & _in_string; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const { return mask & ~_in_string; }
  // Tail of string (everything except the start quote)
  simdjson_inline uint64_t string_tail() const { return _in_string ^ _quote; }

  // backslash characters
  uint64_t _backslash;
  // escaped characters (backslashed--does not include the hex characters after \u)
  uint64_t _escaped;
  // real quotes (non-backslashed ones)
  uint64_t _quote;
  // string characters (includes start quote but not end quote)
  uint64_t _in_string;
};

// Scans blocks for string characters, storing the state necessary to do so
class json_string_scanner {
public:
  simdjson_inline json_string_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Intended to be defined by the implementation
  simdjson_inline uint64_t find_escaped(uint64_t escape);
  simdjson_inline uint64_t find_escaped_branchless(uint64_t escape);

  // Whether the last iteration was still inside a string (all 1's = true, all 0's = false).
  uint64_t prev_in_string = 0ULL;
  // Whether the first character of the next iteration is escaped.
  uint64_t prev_escaped = 0ULL;
};

//
// Finds escaped characters (characters following \).
//
// Handles runs of backslashes like \\\" and \\\\" correctly (yielding 0101 and 01010, respectively).
//
// Does this by:
// - Shift the escape mask to get potentially escaped characters (characters after backslashes).
// - Mask escaped sequences that start on *even* bits with 1010101010 (odd bits are escaped, even bits are not)
// - Mask escaped sequences that start on *odd* bits with 0101010101 (even bits are escaped, odd bits are not)
//
// To distinguish between escaped sequences starting on even/odd bits, it finds the start of all
// escape sequences, filters out the ones that start on even bits, and adds that to the mask of
// escape sequences. This causes the addition to clear out the sequences starting on odd bits (since
// the start bit causes a carry), and leaves even-bit sequences alone.
//
// Example:
//
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
// escape         |  xxx |  xx xxx  xxx  xx xx  | Removed overflow backslash; will | it into follows_escape
// odd_starts     |  x   |  x       x       x   | escape & ~even_bits & ~follows_escape
// even_seq       |     c|    cxxx     c xx   c | c = carry bit -- will be masked out later
// invert_mask    |      |     cxxx     c xx   c| even_seq << 1
// follows_escape |   xx | x xx xxx  xxx  xx xx | Includes overflow bit
// escaped        |   x  | x x  x x  x x  x  x  |
// desired        |   x  | x x  x x  x x  x  x  |
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
//
simdjson_inline uint64_t json_string_scanner::find_escaped_branchless(uint64_t backslash) {
  // If there was overflow, pretend the first character isn't a backslash
  backslash &= ~prev_escaped;
  uint64_t follows_escape = backslash << 1 | prev_escaped;

  // Get sequences starting on even bits by clearing out the odd series using +
  const uint64_t even_bits = 0x5555555555555555ULL;
  uint64_t odd_sequence_starts = backslash & ~even_bits & ~follows_escape;
  uint64_t sequences_starting_on_even_bits;
  prev_escaped = add_overflow(odd_sequence_starts, backslash, &sequences_starting_on_even_bits);
  uint64_t invert_mask = sequences_starting_on_even_bits << 1; // The mask we want to return is the *escaped* bits, not escapes.

  // Mask every other backslashed character as an escaped character
  // Flip the mask for sequences that start on even bits, to correct them
  return (even_bits ^ invert_mask) & follows_escape;
}

//
// Return a mask of all string characters plus end quotes.
//
// prev_escaped is overflow saying whether the next character is escaped.
// prev_in_string is overflow saying whether we're still in a string.
//
// Backslash sequences outside of quotes will be detected in stage 2.
//
simdjson_inline json_string_block json_string_scanner::next(const simd::simd8x64<uint8_t>& in) {
  const uint64_t backslash = in.eq('\\');
  const uint64_t escaped = find_escaped(backslash);
  const uint64_t quote = in.eq('"') & ~escaped;

  //
  // prefix_xor flips on bits inside the string (and flips off the end quote).
  //
  // Then we xor with prev_in_string: if we were in a string already, its effect is flipped
  // (characters inside strings are outside, and characters outside strings are inside).
  //
  const uint64_t in_string = prefix_xor(quote) ^ prev_in_string;

  //
  // Check if we're still in a string at the end of the box so the next block will know
  //
  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20, John Regher from Utah U. says this is fine code
  //
  prev_in_string = uint64_t(static_cast<int64_t>(in_string) >> 63);

  // Use ^ to turn the beginning quote off, and the end quote on.

  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_string_block(
    backslash,
    escaped,
    quote,
    in_string
  );
}

simdjson_inline error_code json_string_scanner::finish() {
  if (prev_in_string) {
    return UNCLOSED_STRING;
  }
  return SUCCESS;
}

} // namespace stage1
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/json_string_scanner.h */
/* begin file src/generic/stage1/json_scanner.h */
namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

/**
 * A block of scanned json, with information on operators and scalars.
 *
 * We seek to identify pseudo-structural characters. Anything that is inside
 * a string must be omitted (hence  & ~_string.string_tail()).
 * Otherwise, pseudo-structural characters come in two forms.
 * 1. We have the structural characters ([,],{,},:, comma). The
 *    term 'structural character' is from the JSON RFC.
 * 2. We have the 'scalar pseudo-structural characters'.
 *    Scalars are quotes, and any character except structural characters and white space.
 *
 * To identify the scalar pseudo-structural characters, we must look at what comes
 * before them: it must be a space, a quote or a structural characters.
 * Starting with simdjson v0.3, we identify them by
 * negation: we identify everything that is followed by a non-quote scalar,
 * and we negate that. Whatever remains must be a 'scalar pseudo-structural character'.
 */
struct json_block {
public:
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_block(json_string_block&& string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(std::move(string)), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}
  simdjson_inline json_block(json_string_block string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(string), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}

  /**
   * The start of structurals.
   * In simdjson prior to v0.3, these were called the pseudo-structural characters.
   **/
  simdjson_inline uint64_t structural_start() const noexcept { return potential_structural_start() & ~_string.string_tail(); }
  /** All JSON whitespace (i.e. not in a string) */
  simdjson_inline uint64_t whitespace() const noexcept { return non_quote_outside_string(_characters.whitespace()); }

  // Helpers

  /** Whether the given characters are inside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const noexcept { return _string.non_quote_inside_string(mask); }
  /** Whether the given characters are outside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const noexcept { return _string.non_quote_outside_string(mask); }

  // string and escape characters
  json_string_block _string;
  // whitespace, structural characters ('operators'), scalars
  json_character_block _characters;
  // whether the previous character was a scalar
  uint64_t _follows_potential_nonquote_scalar;
private:
  // Potential structurals (i.e. disregarding strings)

  /**
   * structural elements ([,],{,},:, comma) plus scalar starts like 123, true and "abc".
   * They may reside inside a string.
   **/
  simdjson_inline uint64_t potential_structural_start() const noexcept { return _characters.op() | potential_scalar_start(); }
  /**
   * The start of non-operator runs, like 123, true and "abc".
   * It main reside inside a string.
   **/
  simdjson_inline uint64_t potential_scalar_start() const noexcept {
    // The term "scalar" refers to anything except structural characters and white space
    // (so letters, numbers, quotes).
    // Whenever it is preceded by something that is not a structural element ({,},[,],:, ") nor a white-space
    // then we know that it is irrelevant structurally.
    return _characters.scalar() & ~follows_potential_scalar();
  }
  /**
   * Whether the given character is immediately after a non-operator like 123, true.
   * The characters following a quote are not included.
   */
  simdjson_inline uint64_t follows_potential_scalar() const noexcept {
    // _follows_potential_nonquote_scalar: is defined as marking any character that follows a character
    // that is not a structural element ({,},[,],:, comma) nor a quote (") and that is not a
    // white space.
    // It is understood that within quoted region, anything at all could be marked (irrelevant).
    return _follows_potential_nonquote_scalar;
  }
};

/**
 * Scans JSON for important bits: structural characters or 'operators', strings, and scalars.
 *
 * The scanner starts by calculating two distinct things:
 * - string characters (taking \" into account)
 * - structural characters or 'operators' ([]{},:, comma)
 *   and scalars (runs of non-operators like 123, true and "abc")
 *
 * To minimize data dependency (a key component of the scanner's speed), it finds these in parallel:
 * in particular, the operator/scalar bit will find plenty of things that are actually part of
 * strings. When we're done, json_block will fuse the two together by masking out tokens that are
 * part of a string.
 */
class json_scanner {
public:
  json_scanner() = default;
  simdjson_inline json_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Whether the last character of the previous iteration is part of a scalar token
  // (anything except whitespace or a structural character/'operator').
  uint64_t prev_scalar = 0ULL;
  json_string_scanner string_scanner{};
};


//
// Check if the current character immediately follows a matching character.
//
// For example, this checks for quotes with backslashes in front of them:
//
//     const uint64_t backslashed_quote = in.eq('"') & immediately_follows(in.eq('\'), prev_backslash);
//
simdjson_inline uint64_t follows(const uint64_t match, uint64_t &overflow) {
  const uint64_t result = match << 1 | overflow;
  overflow = match >> 63;
  return result;
}

simdjson_inline json_block json_scanner::next(const simd::simd8x64<uint8_t>& in) {
  json_string_block strings = string_scanner.next(in);
  // identifies the white-space and the structural characters
  json_character_block characters = json_character_block::classify(in);
  // The term "scalar" refers to anything except structural characters and white space
  // (so letters, numbers, quotes).
  // We want follows_scalar to mark anything that follows a non-quote scalar (so letters and numbers).
  //
  // A terminal quote should either be followed by a structural character (comma, brace, bracket, colon)
  // or nothing. However, we still want ' "a string"true ' to mark the 't' of 'true' as a potential
  // pseudo-structural character just like we would if we had  ' "a string" true '; otherwise we
  // may need to add an extra check when parsing strings.
  //
  // Performance: there are many ways to skin this cat.
  const uint64_t nonquote_scalar = characters.scalar() & ~strings.quote();
  uint64_t follows_nonquote_scalar = follows(nonquote_scalar, prev_scalar);
  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_block(
    strings,// strings is a function-local object so either it moves or the copy is elided.
    characters,
    follows_nonquote_scalar
  );
}

simdjson_inline error_code json_scanner::finish() {
  return string_scanner.finish();
}

} // namespace stage1
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/json_scanner.h */
/* begin file src/generic/stage1/json_minifier.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

class json_minifier {
public:
  template<size_t STEP_SIZE>
  static error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept;

private:
  simdjson_inline json_minifier(uint8_t *_dst)
  : dst{_dst}
  {}
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block_buf, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block);
  simdjson_inline error_code finish(uint8_t *dst_start, size_t &dst_len);
  json_scanner scanner{};
  uint8_t *dst;
};

simdjson_inline void json_minifier::next(const simd::simd8x64<uint8_t>& in, const json_block& block) {
  uint64_t mask = block.whitespace();
  dst += in.compress(mask, dst);
}

simdjson_inline error_code json_minifier::finish(uint8_t *dst_start, size_t &dst_len) {
  error_code error = scanner.finish();
  if (error) { dst_len = 0; return error; }
  dst_len = dst - dst_start;
  return SUCCESS;
}

template<>
simdjson_inline void json_minifier::step<128>(const uint8_t *block_buf, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  simd::simd8x64<uint8_t> in_2(block_buf+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1);
  this->next(in_2, block_2);
  reader.advance();
}

template<>
simdjson_inline void json_minifier::step<64>(const uint8_t *block_buf, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  json_block block_1 = scanner.next(in_1);
  this->next(block_buf, block_1);
  reader.advance();
}

template<size_t STEP_SIZE>
error_code json_minifier::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_minifier minifier(dst);

  // Index the first n-1 blocks
  while (reader.has_full_block()) {
    minifier.step<STEP_SIZE>(reader.full_block(), reader);
  }

  // Index the last (remainder) block, padded with spaces
  uint8_t block[STEP_SIZE];
  size_t remaining_bytes = reader.get_remainder(block);
  if (remaining_bytes > 0) {
    // We do not want to write directly to the output stream. Rather, we write
    // to a local buffer (for safety).
    uint8_t out_block[STEP_SIZE];
    uint8_t * const guarded_dst{minifier.dst};
    minifier.dst = out_block;
    minifier.step<STEP_SIZE>(block, reader);
    size_t to_write = minifier.dst - out_block;
    // In some cases, we could be enticed to consider the padded spaces
    // as part of the string. This is fine as long as we do not write more
    // than we consumed.
    if(to_write > remaining_bytes) { to_write = remaining_bytes; }
    memcpy(guarded_dst, out_block, to_write);
    minifier.dst = guarded_dst + to_write;
  }
  return minifier.finish(dst, dst_len);
}

} // namespace stage1
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/json_minifier.h */
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace haswell {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

class bit_indexer {
public:
  uint32_t *tail;

  simdjson_inline bit_indexer(uint32_t *index_buf) : tail(index_buf) {}

  // flatten out values in 'bits' assuming that they are are to have values of idx
  // plus their position in the bitvector, and store these indexes at
  // base_ptr[base] incrementing base as we go
  // will potentially store extra values beyond end of valid bits, so base_ptr
  // needs to be large enough to handle this
  //
  // If the kernel sets SIMDJSON_CUSTOM_BIT_INDEXER, then it will provide its own
  // version of the code.
#ifdef SIMDJSON_CUSTOM_BIT_INDEXER
  simdjson_inline void write(uint32_t idx, uint64_t bits);
#else
  simdjson_inline void write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0)
        return;
#if SIMDJSON_PREFER_REVERSE_BITS
    /**
     * ARM lacks a fast trailing zero instruction, but it has a fast
     * bit reversal instruction and a fast leading zero instruction.
     * Thus it may be profitable to reverse the bits (once) and then
     * to rely on a sequence of instructions that call the leading
     * zero instruction.
     *
     * Performance notes:
     * The chosen routine is not optimal in terms of data dependency
     * since zero_leading_bit might require two instructions. However,
     * it tends to minimize the total number of instructions which is
     * beneficial.
     */

    uint64_t rev_bits = reverse_bits(bits);
    int cnt = static_cast<int>(count_ones(bits));
    int i = 0;
    // Do the first 8 all together
    for (; i<8; i++) {
      int lz = leading_zeroes(rev_bits);
      this->tail[i] = static_cast<uint32_t>(idx) + lz;
      rev_bits = zero_leading_bit(rev_bits, lz);
    }
    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      i = 8;
      for (; i<16; i++) {
        int lz = leading_zeroes(rev_bits);
        this->tail[i] = static_cast<uint32_t>(idx) + lz;
        rev_bits = zero_leading_bit(rev_bits, lz);
      }


      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        i = 16;
        while (rev_bits != 0) {
          int lz = leading_zeroes(rev_bits);
          this->tail[i++] = static_cast<uint32_t>(idx) + lz;
          rev_bits = zero_leading_bit(rev_bits, lz);
        }
      }
    }
    this->tail += cnt;
#else // SIMDJSON_PREFER_REVERSE_BITS
    /**
     * Under recent x64 systems, we often have both a fast trailing zero
     * instruction and a fast 'clear-lower-bit' instruction so the following
     * algorithm can be competitive.
     */

    int cnt = static_cast<int>(count_ones(bits));
    // Do the first 8 all together
    for (int i=0; i<8; i++) {
      this->tail[i] = idx + trailing_zeroes(bits);
      bits = clear_lowest_bit(bits);
    }

    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      for (int i=8; i<16; i++) {
        this->tail[i] = idx + trailing_zeroes(bits);
        bits = clear_lowest_bit(bits);
      }

      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        int i = 16;
        do {
          this->tail[i] = idx + trailing_zeroes(bits);
          bits = clear_lowest_bit(bits);
          i++;
        } while (i < cnt);
      }
    }

    this->tail += cnt;
#endif
  }
#endif // SIMDJSON_CUSTOM_BIT_INDEXER

};

class json_structural_indexer {
public:
  /**
   * Find the important bits of JSON in a 128-byte chunk, and add them to structural_indexes.
   *
   * @param partial Setting the partial parameter to true allows the find_structural_bits to
   *   tolerate unclosed strings. The caller should still ensure that the input is valid UTF-8. If
   *   you are processing substrings, you may want to call on a function like trimmed_length_safe_utf8.
   */
  template<size_t STEP_SIZE>
  static error_code index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept;

private:
  simdjson_inline json_structural_indexer(uint32_t *structural_indexes);
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx);
  simdjson_inline error_code finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial);

  json_scanner scanner{};
  utf8_checker checker{};
  bit_indexer indexer;
  uint64_t prev_structurals = 0;
  uint64_t unescaped_chars_error = 0;
};

simdjson_inline json_structural_indexer::json_structural_indexer(uint32_t *structural_indexes) : indexer{structural_indexes} {}

// Skip the last character if it is partial
simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 2 bytes left
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        return len;
      case 0:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
  if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 1 byte left
  if (buf[len-3] >= 0xf0) { return len-3; } // 4-byte characters with only 3 bytes left
  return len;
}

//
// PERF NOTES:
// We pipe 2 inputs through these stages:
// 1. Load JSON into registers. This takes a long time and is highly parallelizable, so we load
//    2 inputs' worth at once so that by the time step 2 is looking for them input, it's available.
// 2. Scan the JSON for critical data: strings, scalars and operators. This is the critical path.
//    The output of step 1 depends entirely on this information. These functions don't quite use
//    up enough CPU: the second half of the functions is highly serial, only using 1 execution core
//    at a time. The second input's scans has some dependency on the first ones finishing it, but
//    they can make a lot of progress before they need that information.
// 3. Step 1 doesn't use enough capacity, so we run some extra stuff while we're waiting for that
//    to finish: utf-8 checks and generating the output from the last iteration.
//
// The reason we run 2 inputs at a time, is steps 2 and 3 are *still* not enough to soak up all
// available capacity with just one input. Running 2 at a time seems to give the CPU a good enough
// workout.
//
template<size_t STEP_SIZE>
error_code json_structural_indexer::index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept {
  if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
  // We guard the rest of the code so that we can assume that len > 0 throughout.
  if (len == 0) { return EMPTY; }
  if (is_streaming(partial)) {
    len = trim_partial_utf8(buf, len);
    // If you end up with an empty window after trimming
    // the partial UTF-8 bytes, then chances are good that you
    // have an UTF-8 formatting error.
    if(len == 0) { return UTF8_ERROR; }
  }
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_structural_indexer indexer(parser.structural_indexes.get());

  // Read all but the last block
  while (reader.has_full_block()) {
    indexer.step<STEP_SIZE>(reader.full_block(), reader);
  }
  // Take care of the last block (will always be there unless file is empty which is
  // not supposed to happen.)
  uint8_t block[STEP_SIZE];
  if (simdjson_unlikely(reader.get_remainder(block) == 0)) { return UNEXPECTED_ERROR; }
  indexer.step<STEP_SIZE>(block, reader);
  return indexer.finish(parser, reader.block_index(), len, partial);
}

template<>
simdjson_inline void json_structural_indexer::step<128>(const uint8_t *block, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  simd::simd8x64<uint8_t> in_2(block+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1, reader.block_index());
  this->next(in_2, block_2, reader.block_index()+64);
  reader.advance();
}

template<>
simdjson_inline void json_structural_indexer::step<64>(const uint8_t *block, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  json_block block_1 = scanner.next(in_1);
  this->next(in_1, block_1, reader.block_index());
  reader.advance();
}

simdjson_inline void json_structural_indexer::next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx) {
  uint64_t unescaped = in.lteq(0x1F);
#if SIMDJSON_UTF8VALIDATION
  checker.check_next_input(in);
#endif
  indexer.write(uint32_t(idx-64), prev_structurals); // Output *last* iteration's structurals to the parser
  prev_structurals = block.structural_start();
  unescaped_chars_error |= block.non_quote_inside_string(unescaped);
}

simdjson_inline error_code json_structural_indexer::finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial) {
  // Write out the final iteration's structurals
  indexer.write(uint32_t(idx-64), prev_structurals);
  error_code error = scanner.finish();
  // We deliberately break down the next expression so that it is
  // human readable.
  const bool should_we_exit = is_streaming(partial) ?
    ((error != SUCCESS) && (error != UNCLOSED_STRING)) // when partial we tolerate UNCLOSED_STRING
    : (error != SUCCESS); // if partial is false, we must have SUCCESS
  const bool have_unclosed_string = (error == UNCLOSED_STRING);
  if (simdjson_unlikely(should_we_exit)) { return error; }

  if (unescaped_chars_error) {
    return UNESCAPED_CHARS;
  }
  parser.n_structural_indexes = uint32_t(indexer.tail - parser.structural_indexes.get());
  /***
   * The On Demand API requires special padding.
   *
   * This is related to https://github.com/simdjson/simdjson/issues/906
   * Basically, we want to make sure that if the parsing continues beyond the last (valid)
   * structural character, it quickly stops.
   * Only three structural characters can be repeated without triggering an error in JSON:  [,] and }.
   * We repeat the padding character (at 'len'). We don't know what it is, but if the parsing
   * continues, then it must be [,] or }.
   * Suppose it is ] or }. We backtrack to the first character, what could it be that would
   * not trigger an error? It could be ] or } but no, because you can't start a document that way.
   * It can't be a comma, a colon or any simple value. So the only way we could continue is
   * if the repeated character is [. But if so, the document must start with [. But if the document
   * starts with [, it should end with ]. If we enforce that rule, then we would get
   * ][[ which is invalid.
   *
   * This is illustrated with the test array_iterate_unclosed_error() on the following input:
   * R"({ "a": [,,)"
   **/
  parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len); // used later in partial == stage1_mode::streaming_final
  parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
  parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
  parser.next_structural_index = 0;
  // a valid JSON file cannot have zero structural indexes - we should have found something
  if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
    return EMPTY;
  }
  if (simdjson_unlikely(parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
    return UNEXPECTED_ERROR;
  }
  if (partial == stage1_mode::streaming_partial) {
    // If we have an unclosed string, then the last structural
    // will be the quote and we want to make sure to omit it.
    if(have_unclosed_string) {
      parser.n_structural_indexes--;
      // a valid JSON file cannot have zero structural indexes - we should have found something
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }

    parser.n_structural_indexes = new_structural_indexes;
  } else if (partial == stage1_mode::streaming_final) {
    if(have_unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
        // We tolerate an unclosed string at the very end of the stream. Indeed, users
        // often load their data in bulk without being careful and they want us to ignore
        // the trailing garbage.
        return EMPTY;
    }
  }
  checker.check_eof();
  return checker.errors();
}

} // namespace stage1
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/json_structural_indexer.h */
/* begin file src/generic/stage1/utf8_validator.h */
namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

/**
 * Validates that the string is actual UTF-8.
 */
template<class checker>
bool generic_validate_utf8(const uint8_t * input, size_t length) {
    checker c{};
    buf_block_reader<64> reader(input, length);
    while (reader.has_full_block()) {
      simd::simd8x64<uint8_t> in(reader.full_block());
      c.check_next_input(in);
      reader.advance();
    }
    uint8_t block[64]{};
    reader.get_remainder(block);
    simd::simd8x64<uint8_t> in(block);
    c.check_next_input(in);
    reader.advance();
    c.check_eof();
    return c.errors() == error_code::SUCCESS;
}

bool generic_validate_utf8(const char * input, size_t length) {
    return generic_validate_utf8<utf8_checker>(reinterpret_cast<const uint8_t *>(input),length);
}

} // namespace stage1
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage1/utf8_validator.h */

//
// Stage 2
//
/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace haswell {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace haswell {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace haswell {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace haswell {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace haswell {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace haswell
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

//
// Implementation-specific overrides
//
namespace simdjson {
namespace haswell {
namespace {
namespace stage1 {

simdjson_inline uint64_t json_string_scanner::find_escaped(uint64_t backslash) {
  if (!backslash) { uint64_t escaped = prev_escaped; prev_escaped = 0; return escaped; }
  return find_escaped_branchless(backslash);
}

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return haswell::stage1::json_minifier::minify<128>(buf, len, dst, dst_len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return haswell::stage1::json_structural_indexer::index<128>(_buf, _len, *this, streaming);
}

simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return haswell::stage1::generic_validate_utf8(buf,len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool replacement_char) const noexcept {
  return haswell::stringparsing::parse_string(src, dst, replacement_char);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return haswell::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace haswell
} // namespace simdjson

/* begin file include/simdjson/haswell/end.h */
SIMDJSON_UNTARGET_HASWELL
/* end file include/simdjson/haswell/end.h */
/* end file src/haswell/dom_parser_implementation.cpp */
#endif
#if SIMDJSON_IMPLEMENTATION_PPC64
/* begin file src/ppc64/implementation.cpp */
/* begin file include/simdjson/ppc64/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "ppc64"
// #define SIMDJSON_IMPLEMENTATION ppc64
/* end file include/simdjson/ppc64/begin.h */

namespace simdjson {
namespace ppc64 {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace ppc64
} // namespace simdjson

/* begin file include/simdjson/ppc64/end.h */
/* end file include/simdjson/ppc64/end.h */
/* end file src/ppc64/implementation.cpp */
/* begin file src/ppc64/dom_parser_implementation.cpp */
/* begin file include/simdjson/ppc64/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "ppc64"
// #define SIMDJSON_IMPLEMENTATION ppc64
/* end file include/simdjson/ppc64/begin.h */

//
// Stage 1
//
namespace simdjson {
namespace ppc64 {
namespace {

using namespace simd;

struct json_character_block {
  static simdjson_inline json_character_block classify(const simd::simd8x64<uint8_t>& in);

  simdjson_inline uint64_t whitespace() const noexcept { return _whitespace; }
  simdjson_inline uint64_t op() const noexcept { return _op; }
  simdjson_inline uint64_t scalar() const noexcept { return ~(op() | whitespace()); }

  uint64_t _whitespace;
  uint64_t _op;
};

simdjson_inline json_character_block json_character_block::classify(const simd::simd8x64<uint8_t>& in) {
  const simd8<uint8_t> table1(16, 0, 0, 0, 0, 0, 0, 0, 0, 8, 12, 1, 2, 9, 0, 0);
  const simd8<uint8_t> table2(8, 0, 18, 4, 0, 1, 0, 1, 0, 0, 0, 3, 2, 1, 0, 0);

  simd8x64<uint8_t> v(
     (in.chunks[0] & 0xf).lookup_16(table1) & (in.chunks[0].shr<4>()).lookup_16(table2),
     (in.chunks[1] & 0xf).lookup_16(table1) & (in.chunks[1].shr<4>()).lookup_16(table2),
     (in.chunks[2] & 0xf).lookup_16(table1) & (in.chunks[2].shr<4>()).lookup_16(table2),
     (in.chunks[3] & 0xf).lookup_16(table1) & (in.chunks[3].shr<4>()).lookup_16(table2)
  );

  uint64_t op = simd8x64<bool>(
        v.chunks[0].any_bits_set(0x7),
        v.chunks[1].any_bits_set(0x7),
        v.chunks[2].any_bits_set(0x7),
        v.chunks[3].any_bits_set(0x7)
  ).to_bitmask();

  uint64_t whitespace = simd8x64<bool>(
        v.chunks[0].any_bits_set(0x18),
        v.chunks[1].any_bits_set(0x18),
        v.chunks[2].any_bits_set(0x18),
        v.chunks[3].any_bits_set(0x18)
  ).to_bitmask();

  return { whitespace, op };
}

simdjson_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  // careful: 0x80 is not ascii.
  return input.reduce_or().saturating_sub(0x7fu).bits_not_set_anywhere();
}

simdjson_unused simdjson_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_second_byte = prev1.saturating_sub(0xc0u-1); // Only 11______ will be > 0
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_second_byte | is_third_byte | is_fourth_byte) > int8_t(0);
}

simdjson_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_third_byte | is_fourth_byte) > int8_t(0);
}

} // unnamed namespace
} // namespace ppc64
} // namespace simdjson

/* begin file src/generic/stage1/utf8_lookup4_algorithm.h */
namespace simdjson {
namespace ppc64 {
namespace {
namespace utf8_validation {

using namespace simd;

  simdjson_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdjson_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }

  //
  // Return nonzero if there are incomplete multibyte characters at the end of the block:
  // e.g. if there is a 4-byte character, but it's 3 bytes from the end.
  //
  simdjson_inline simd8<uint8_t> is_incomplete(const simd8<uint8_t> input) {
    // If the previous input's last 3 bytes match this, they're too short (they ended at EOF):
    // ... 1111____ 111_____ 11______
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    static const uint8_t max_array[64] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#else
    static const uint8_t max_array[32] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#endif
    const simd8<uint8_t> max_value(&max_array[sizeof(max_array)-sizeof(simd8<uint8_t>)]);
    return input.gt_bits(max_value);
  }

  struct utf8_checker {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;
    // The last input we received
    simd8<uint8_t> prev_input_block;
    // Whether the last input we received was incomplete (used for ASCII fast path)
    simd8<uint8_t> prev_incomplete;

    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdjson_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }

    // The only problem that can happen at EOF is that a multibyte character is too short
    // or a byte value too large in the last bytes: check_special_cases only checks for bytes
    // too large in the first of two bytes.
    simdjson_inline void check_eof() {
      // If the previous block had incomplete UTF-8 characters at the end, an ASCII block can't
      // possibly finish them.
      this->error |= this->prev_incomplete;
    }

#ifndef SIMDJSON_IF_CONSTEXPR
#if SIMDJSON_CPLUSPLUS17
#define SIMDJSON_IF_CONSTEXPR if constexpr
#else
#define SIMDJSON_IF_CONSTEXPR if
#endif
#endif

    simdjson_inline void check_next_input(const simd8x64<uint8_t>& input) {
      if(simdjson_likely(is_ascii(input))) {
        this->error |= this->prev_incomplete;
      } else {
        // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
        static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 1)
                ||(simd8x64<uint8_t>::NUM_CHUNKS == 2)
                || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
                "We support one, two or four chunks per 64-byte block.");
        SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 1) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 2) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 4) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
          this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
        }
        this->prev_incomplete = is_incomplete(input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1]);
        this->prev_input_block = input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1];
      }
    }
    // do not forget to call check_eof!
    simdjson_inline error_code errors() {
      return this->error.any_bits_set_anywhere() ? error_code::UTF8_ERROR : error_code::SUCCESS;
    }

  }; // struct utf8_checker
} // namespace utf8_validation

using utf8_validation::utf8_checker;

} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/utf8_lookup4_algorithm.h */
/* begin file src/generic/stage1/json_structural_indexer.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

/* begin file src/generic/stage1/buf_block_reader.h */
namespace simdjson {
namespace ppc64 {
namespace {

// Walks through a buffer in block-sized increments, loading the last part with spaces
template<size_t STEP_SIZE>
struct buf_block_reader {
public:
  simdjson_inline buf_block_reader(const uint8_t *_buf, size_t _len);
  simdjson_inline size_t block_index();
  simdjson_inline bool has_full_block() const;
  simdjson_inline const uint8_t *full_block() const;
  /**
   * Get the last block, padded with spaces.
   *
   * There will always be a last block, with at least 1 byte, unless len == 0 (in which case this
   * function fills the buffer with spaces and returns 0. In particular, if len == STEP_SIZE there
   * will be 0 full_blocks and 1 remainder block with STEP_SIZE bytes and no spaces for padding.
   *
   * @return the number of effective characters in the last block.
   */
  simdjson_inline size_t get_remainder(uint8_t *dst) const;
  simdjson_inline void advance();
private:
  const uint8_t *buf;
  const size_t len;
  const size_t lenminusstep;
  size_t idx;
};

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text_64(const uint8_t *text) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    buf[i] = int8_t(text[i]) < ' ' ? '_' : int8_t(text[i]);
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text(const simd8x64<uint8_t>& in) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  in.store(reinterpret_cast<uint8_t*>(buf));
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    if (buf[i] < ' ') { buf[i] = '_'; }
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

simdjson_unused static char * format_mask(uint64_t mask) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<64; i++) {
    buf[i] = (mask & (size_t(1) << i)) ? 'X' : ' ';
  }
  buf[64] = '\0';
  return buf;
}

template<size_t STEP_SIZE>
simdjson_inline buf_block_reader<STEP_SIZE>::buf_block_reader(const uint8_t *_buf, size_t _len) : buf{_buf}, len{_len}, lenminusstep{len < STEP_SIZE ? 0 : len - STEP_SIZE}, idx{0} {}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::block_index() { return idx; }

template<size_t STEP_SIZE>
simdjson_inline bool buf_block_reader<STEP_SIZE>::has_full_block() const {
  return idx < lenminusstep;
}

template<size_t STEP_SIZE>
simdjson_inline const uint8_t *buf_block_reader<STEP_SIZE>::full_block() const {
  return &buf[idx];
}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::get_remainder(uint8_t *dst) const {
  if(len == idx) { return 0; } // memcpy(dst, null, 0) will trigger an error with some sanitizers
  std::memset(dst, 0x20, STEP_SIZE); // std::memset STEP_SIZE because it's more efficient to write out 8 or 16 bytes at once.
  std::memcpy(dst, buf + idx, len - idx);
  return len - idx;
}

template<size_t STEP_SIZE>
simdjson_inline void buf_block_reader<STEP_SIZE>::advance() {
  idx += STEP_SIZE;
}

} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/buf_block_reader.h */
/* begin file src/generic/stage1/json_string_scanner.h */
namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

struct json_string_block {
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_string_block(uint64_t backslash, uint64_t escaped, uint64_t quote, uint64_t in_string) :
  _backslash(backslash), _escaped(escaped), _quote(quote), _in_string(in_string) {}

  // Escaped characters (characters following an escape() character)
  simdjson_inline uint64_t escaped() const { return _escaped; }
  // Escape characters (backslashes that are not escaped--i.e. in \\, includes only the first \)
  simdjson_inline uint64_t escape() const { return _backslash & ~_escaped; }
  // Real (non-backslashed) quotes
  simdjson_inline uint64_t quote() const { return _quote; }
  // Start quotes of strings
  simdjson_inline uint64_t string_start() const { return _quote & _in_string; }
  // End quotes of strings
  simdjson_inline uint64_t string_end() const { return _quote & ~_in_string; }
  // Only characters inside the string (not including the quotes)
  simdjson_inline uint64_t string_content() const { return _in_string & ~_quote; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const { return mask & _in_string; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const { return mask & ~_in_string; }
  // Tail of string (everything except the start quote)
  simdjson_inline uint64_t string_tail() const { return _in_string ^ _quote; }

  // backslash characters
  uint64_t _backslash;
  // escaped characters (backslashed--does not include the hex characters after \u)
  uint64_t _escaped;
  // real quotes (non-backslashed ones)
  uint64_t _quote;
  // string characters (includes start quote but not end quote)
  uint64_t _in_string;
};

// Scans blocks for string characters, storing the state necessary to do so
class json_string_scanner {
public:
  simdjson_inline json_string_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Intended to be defined by the implementation
  simdjson_inline uint64_t find_escaped(uint64_t escape);
  simdjson_inline uint64_t find_escaped_branchless(uint64_t escape);

  // Whether the last iteration was still inside a string (all 1's = true, all 0's = false).
  uint64_t prev_in_string = 0ULL;
  // Whether the first character of the next iteration is escaped.
  uint64_t prev_escaped = 0ULL;
};

//
// Finds escaped characters (characters following \).
//
// Handles runs of backslashes like \\\" and \\\\" correctly (yielding 0101 and 01010, respectively).
//
// Does this by:
// - Shift the escape mask to get potentially escaped characters (characters after backslashes).
// - Mask escaped sequences that start on *even* bits with 1010101010 (odd bits are escaped, even bits are not)
// - Mask escaped sequences that start on *odd* bits with 0101010101 (even bits are escaped, odd bits are not)
//
// To distinguish between escaped sequences starting on even/odd bits, it finds the start of all
// escape sequences, filters out the ones that start on even bits, and adds that to the mask of
// escape sequences. This causes the addition to clear out the sequences starting on odd bits (since
// the start bit causes a carry), and leaves even-bit sequences alone.
//
// Example:
//
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
// escape         |  xxx |  xx xxx  xxx  xx xx  | Removed overflow backslash; will | it into follows_escape
// odd_starts     |  x   |  x       x       x   | escape & ~even_bits & ~follows_escape
// even_seq       |     c|    cxxx     c xx   c | c = carry bit -- will be masked out later
// invert_mask    |      |     cxxx     c xx   c| even_seq << 1
// follows_escape |   xx | x xx xxx  xxx  xx xx | Includes overflow bit
// escaped        |   x  | x x  x x  x x  x  x  |
// desired        |   x  | x x  x x  x x  x  x  |
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
//
simdjson_inline uint64_t json_string_scanner::find_escaped_branchless(uint64_t backslash) {
  // If there was overflow, pretend the first character isn't a backslash
  backslash &= ~prev_escaped;
  uint64_t follows_escape = backslash << 1 | prev_escaped;

  // Get sequences starting on even bits by clearing out the odd series using +
  const uint64_t even_bits = 0x5555555555555555ULL;
  uint64_t odd_sequence_starts = backslash & ~even_bits & ~follows_escape;
  uint64_t sequences_starting_on_even_bits;
  prev_escaped = add_overflow(odd_sequence_starts, backslash, &sequences_starting_on_even_bits);
  uint64_t invert_mask = sequences_starting_on_even_bits << 1; // The mask we want to return is the *escaped* bits, not escapes.

  // Mask every other backslashed character as an escaped character
  // Flip the mask for sequences that start on even bits, to correct them
  return (even_bits ^ invert_mask) & follows_escape;
}

//
// Return a mask of all string characters plus end quotes.
//
// prev_escaped is overflow saying whether the next character is escaped.
// prev_in_string is overflow saying whether we're still in a string.
//
// Backslash sequences outside of quotes will be detected in stage 2.
//
simdjson_inline json_string_block json_string_scanner::next(const simd::simd8x64<uint8_t>& in) {
  const uint64_t backslash = in.eq('\\');
  const uint64_t escaped = find_escaped(backslash);
  const uint64_t quote = in.eq('"') & ~escaped;

  //
  // prefix_xor flips on bits inside the string (and flips off the end quote).
  //
  // Then we xor with prev_in_string: if we were in a string already, its effect is flipped
  // (characters inside strings are outside, and characters outside strings are inside).
  //
  const uint64_t in_string = prefix_xor(quote) ^ prev_in_string;

  //
  // Check if we're still in a string at the end of the box so the next block will know
  //
  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20, John Regher from Utah U. says this is fine code
  //
  prev_in_string = uint64_t(static_cast<int64_t>(in_string) >> 63);

  // Use ^ to turn the beginning quote off, and the end quote on.

  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_string_block(
    backslash,
    escaped,
    quote,
    in_string
  );
}

simdjson_inline error_code json_string_scanner::finish() {
  if (prev_in_string) {
    return UNCLOSED_STRING;
  }
  return SUCCESS;
}

} // namespace stage1
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/json_string_scanner.h */
/* begin file src/generic/stage1/json_scanner.h */
namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

/**
 * A block of scanned json, with information on operators and scalars.
 *
 * We seek to identify pseudo-structural characters. Anything that is inside
 * a string must be omitted (hence  & ~_string.string_tail()).
 * Otherwise, pseudo-structural characters come in two forms.
 * 1. We have the structural characters ([,],{,},:, comma). The
 *    term 'structural character' is from the JSON RFC.
 * 2. We have the 'scalar pseudo-structural characters'.
 *    Scalars are quotes, and any character except structural characters and white space.
 *
 * To identify the scalar pseudo-structural characters, we must look at what comes
 * before them: it must be a space, a quote or a structural characters.
 * Starting with simdjson v0.3, we identify them by
 * negation: we identify everything that is followed by a non-quote scalar,
 * and we negate that. Whatever remains must be a 'scalar pseudo-structural character'.
 */
struct json_block {
public:
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_block(json_string_block&& string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(std::move(string)), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}
  simdjson_inline json_block(json_string_block string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(string), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}

  /**
   * The start of structurals.
   * In simdjson prior to v0.3, these were called the pseudo-structural characters.
   **/
  simdjson_inline uint64_t structural_start() const noexcept { return potential_structural_start() & ~_string.string_tail(); }
  /** All JSON whitespace (i.e. not in a string) */
  simdjson_inline uint64_t whitespace() const noexcept { return non_quote_outside_string(_characters.whitespace()); }

  // Helpers

  /** Whether the given characters are inside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const noexcept { return _string.non_quote_inside_string(mask); }
  /** Whether the given characters are outside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const noexcept { return _string.non_quote_outside_string(mask); }

  // string and escape characters
  json_string_block _string;
  // whitespace, structural characters ('operators'), scalars
  json_character_block _characters;
  // whether the previous character was a scalar
  uint64_t _follows_potential_nonquote_scalar;
private:
  // Potential structurals (i.e. disregarding strings)

  /**
   * structural elements ([,],{,},:, comma) plus scalar starts like 123, true and "abc".
   * They may reside inside a string.
   **/
  simdjson_inline uint64_t potential_structural_start() const noexcept { return _characters.op() | potential_scalar_start(); }
  /**
   * The start of non-operator runs, like 123, true and "abc".
   * It main reside inside a string.
   **/
  simdjson_inline uint64_t potential_scalar_start() const noexcept {
    // The term "scalar" refers to anything except structural characters and white space
    // (so letters, numbers, quotes).
    // Whenever it is preceded by something that is not a structural element ({,},[,],:, ") nor a white-space
    // then we know that it is irrelevant structurally.
    return _characters.scalar() & ~follows_potential_scalar();
  }
  /**
   * Whether the given character is immediately after a non-operator like 123, true.
   * The characters following a quote are not included.
   */
  simdjson_inline uint64_t follows_potential_scalar() const noexcept {
    // _follows_potential_nonquote_scalar: is defined as marking any character that follows a character
    // that is not a structural element ({,},[,],:, comma) nor a quote (") and that is not a
    // white space.
    // It is understood that within quoted region, anything at all could be marked (irrelevant).
    return _follows_potential_nonquote_scalar;
  }
};

/**
 * Scans JSON for important bits: structural characters or 'operators', strings, and scalars.
 *
 * The scanner starts by calculating two distinct things:
 * - string characters (taking \" into account)
 * - structural characters or 'operators' ([]{},:, comma)
 *   and scalars (runs of non-operators like 123, true and "abc")
 *
 * To minimize data dependency (a key component of the scanner's speed), it finds these in parallel:
 * in particular, the operator/scalar bit will find plenty of things that are actually part of
 * strings. When we're done, json_block will fuse the two together by masking out tokens that are
 * part of a string.
 */
class json_scanner {
public:
  json_scanner() = default;
  simdjson_inline json_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Whether the last character of the previous iteration is part of a scalar token
  // (anything except whitespace or a structural character/'operator').
  uint64_t prev_scalar = 0ULL;
  json_string_scanner string_scanner{};
};


//
// Check if the current character immediately follows a matching character.
//
// For example, this checks for quotes with backslashes in front of them:
//
//     const uint64_t backslashed_quote = in.eq('"') & immediately_follows(in.eq('\'), prev_backslash);
//
simdjson_inline uint64_t follows(const uint64_t match, uint64_t &overflow) {
  const uint64_t result = match << 1 | overflow;
  overflow = match >> 63;
  return result;
}

simdjson_inline json_block json_scanner::next(const simd::simd8x64<uint8_t>& in) {
  json_string_block strings = string_scanner.next(in);
  // identifies the white-space and the structural characters
  json_character_block characters = json_character_block::classify(in);
  // The term "scalar" refers to anything except structural characters and white space
  // (so letters, numbers, quotes).
  // We want follows_scalar to mark anything that follows a non-quote scalar (so letters and numbers).
  //
  // A terminal quote should either be followed by a structural character (comma, brace, bracket, colon)
  // or nothing. However, we still want ' "a string"true ' to mark the 't' of 'true' as a potential
  // pseudo-structural character just like we would if we had  ' "a string" true '; otherwise we
  // may need to add an extra check when parsing strings.
  //
  // Performance: there are many ways to skin this cat.
  const uint64_t nonquote_scalar = characters.scalar() & ~strings.quote();
  uint64_t follows_nonquote_scalar = follows(nonquote_scalar, prev_scalar);
  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_block(
    strings,// strings is a function-local object so either it moves or the copy is elided.
    characters,
    follows_nonquote_scalar
  );
}

simdjson_inline error_code json_scanner::finish() {
  return string_scanner.finish();
}

} // namespace stage1
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/json_scanner.h */
/* begin file src/generic/stage1/json_minifier.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

class json_minifier {
public:
  template<size_t STEP_SIZE>
  static error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept;

private:
  simdjson_inline json_minifier(uint8_t *_dst)
  : dst{_dst}
  {}
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block_buf, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block);
  simdjson_inline error_code finish(uint8_t *dst_start, size_t &dst_len);
  json_scanner scanner{};
  uint8_t *dst;
};

simdjson_inline void json_minifier::next(const simd::simd8x64<uint8_t>& in, const json_block& block) {
  uint64_t mask = block.whitespace();
  dst += in.compress(mask, dst);
}

simdjson_inline error_code json_minifier::finish(uint8_t *dst_start, size_t &dst_len) {
  error_code error = scanner.finish();
  if (error) { dst_len = 0; return error; }
  dst_len = dst - dst_start;
  return SUCCESS;
}

template<>
simdjson_inline void json_minifier::step<128>(const uint8_t *block_buf, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  simd::simd8x64<uint8_t> in_2(block_buf+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1);
  this->next(in_2, block_2);
  reader.advance();
}

template<>
simdjson_inline void json_minifier::step<64>(const uint8_t *block_buf, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  json_block block_1 = scanner.next(in_1);
  this->next(block_buf, block_1);
  reader.advance();
}

template<size_t STEP_SIZE>
error_code json_minifier::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_minifier minifier(dst);

  // Index the first n-1 blocks
  while (reader.has_full_block()) {
    minifier.step<STEP_SIZE>(reader.full_block(), reader);
  }

  // Index the last (remainder) block, padded with spaces
  uint8_t block[STEP_SIZE];
  size_t remaining_bytes = reader.get_remainder(block);
  if (remaining_bytes > 0) {
    // We do not want to write directly to the output stream. Rather, we write
    // to a local buffer (for safety).
    uint8_t out_block[STEP_SIZE];
    uint8_t * const guarded_dst{minifier.dst};
    minifier.dst = out_block;
    minifier.step<STEP_SIZE>(block, reader);
    size_t to_write = minifier.dst - out_block;
    // In some cases, we could be enticed to consider the padded spaces
    // as part of the string. This is fine as long as we do not write more
    // than we consumed.
    if(to_write > remaining_bytes) { to_write = remaining_bytes; }
    memcpy(guarded_dst, out_block, to_write);
    minifier.dst = guarded_dst + to_write;
  }
  return minifier.finish(dst, dst_len);
}

} // namespace stage1
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/json_minifier.h */
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace ppc64 {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

class bit_indexer {
public:
  uint32_t *tail;

  simdjson_inline bit_indexer(uint32_t *index_buf) : tail(index_buf) {}

  // flatten out values in 'bits' assuming that they are are to have values of idx
  // plus their position in the bitvector, and store these indexes at
  // base_ptr[base] incrementing base as we go
  // will potentially store extra values beyond end of valid bits, so base_ptr
  // needs to be large enough to handle this
  //
  // If the kernel sets SIMDJSON_CUSTOM_BIT_INDEXER, then it will provide its own
  // version of the code.
#ifdef SIMDJSON_CUSTOM_BIT_INDEXER
  simdjson_inline void write(uint32_t idx, uint64_t bits);
#else
  simdjson_inline void write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0)
        return;
#if SIMDJSON_PREFER_REVERSE_BITS
    /**
     * ARM lacks a fast trailing zero instruction, but it has a fast
     * bit reversal instruction and a fast leading zero instruction.
     * Thus it may be profitable to reverse the bits (once) and then
     * to rely on a sequence of instructions that call the leading
     * zero instruction.
     *
     * Performance notes:
     * The chosen routine is not optimal in terms of data dependency
     * since zero_leading_bit might require two instructions. However,
     * it tends to minimize the total number of instructions which is
     * beneficial.
     */

    uint64_t rev_bits = reverse_bits(bits);
    int cnt = static_cast<int>(count_ones(bits));
    int i = 0;
    // Do the first 8 all together
    for (; i<8; i++) {
      int lz = leading_zeroes(rev_bits);
      this->tail[i] = static_cast<uint32_t>(idx) + lz;
      rev_bits = zero_leading_bit(rev_bits, lz);
    }
    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      i = 8;
      for (; i<16; i++) {
        int lz = leading_zeroes(rev_bits);
        this->tail[i] = static_cast<uint32_t>(idx) + lz;
        rev_bits = zero_leading_bit(rev_bits, lz);
      }


      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        i = 16;
        while (rev_bits != 0) {
          int lz = leading_zeroes(rev_bits);
          this->tail[i++] = static_cast<uint32_t>(idx) + lz;
          rev_bits = zero_leading_bit(rev_bits, lz);
        }
      }
    }
    this->tail += cnt;
#else // SIMDJSON_PREFER_REVERSE_BITS
    /**
     * Under recent x64 systems, we often have both a fast trailing zero
     * instruction and a fast 'clear-lower-bit' instruction so the following
     * algorithm can be competitive.
     */

    int cnt = static_cast<int>(count_ones(bits));
    // Do the first 8 all together
    for (int i=0; i<8; i++) {
      this->tail[i] = idx + trailing_zeroes(bits);
      bits = clear_lowest_bit(bits);
    }

    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      for (int i=8; i<16; i++) {
        this->tail[i] = idx + trailing_zeroes(bits);
        bits = clear_lowest_bit(bits);
      }

      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        int i = 16;
        do {
          this->tail[i] = idx + trailing_zeroes(bits);
          bits = clear_lowest_bit(bits);
          i++;
        } while (i < cnt);
      }
    }

    this->tail += cnt;
#endif
  }
#endif // SIMDJSON_CUSTOM_BIT_INDEXER

};

class json_structural_indexer {
public:
  /**
   * Find the important bits of JSON in a 128-byte chunk, and add them to structural_indexes.
   *
   * @param partial Setting the partial parameter to true allows the find_structural_bits to
   *   tolerate unclosed strings. The caller should still ensure that the input is valid UTF-8. If
   *   you are processing substrings, you may want to call on a function like trimmed_length_safe_utf8.
   */
  template<size_t STEP_SIZE>
  static error_code index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept;

private:
  simdjson_inline json_structural_indexer(uint32_t *structural_indexes);
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx);
  simdjson_inline error_code finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial);

  json_scanner scanner{};
  utf8_checker checker{};
  bit_indexer indexer;
  uint64_t prev_structurals = 0;
  uint64_t unescaped_chars_error = 0;
};

simdjson_inline json_structural_indexer::json_structural_indexer(uint32_t *structural_indexes) : indexer{structural_indexes} {}

// Skip the last character if it is partial
simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 2 bytes left
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        return len;
      case 0:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
  if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 1 byte left
  if (buf[len-3] >= 0xf0) { return len-3; } // 4-byte characters with only 3 bytes left
  return len;
}

//
// PERF NOTES:
// We pipe 2 inputs through these stages:
// 1. Load JSON into registers. This takes a long time and is highly parallelizable, so we load
//    2 inputs' worth at once so that by the time step 2 is looking for them input, it's available.
// 2. Scan the JSON for critical data: strings, scalars and operators. This is the critical path.
//    The output of step 1 depends entirely on this information. These functions don't quite use
//    up enough CPU: the second half of the functions is highly serial, only using 1 execution core
//    at a time. The second input's scans has some dependency on the first ones finishing it, but
//    they can make a lot of progress before they need that information.
// 3. Step 1 doesn't use enough capacity, so we run some extra stuff while we're waiting for that
//    to finish: utf-8 checks and generating the output from the last iteration.
//
// The reason we run 2 inputs at a time, is steps 2 and 3 are *still* not enough to soak up all
// available capacity with just one input. Running 2 at a time seems to give the CPU a good enough
// workout.
//
template<size_t STEP_SIZE>
error_code json_structural_indexer::index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept {
  if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
  // We guard the rest of the code so that we can assume that len > 0 throughout.
  if (len == 0) { return EMPTY; }
  if (is_streaming(partial)) {
    len = trim_partial_utf8(buf, len);
    // If you end up with an empty window after trimming
    // the partial UTF-8 bytes, then chances are good that you
    // have an UTF-8 formatting error.
    if(len == 0) { return UTF8_ERROR; }
  }
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_structural_indexer indexer(parser.structural_indexes.get());

  // Read all but the last block
  while (reader.has_full_block()) {
    indexer.step<STEP_SIZE>(reader.full_block(), reader);
  }
  // Take care of the last block (will always be there unless file is empty which is
  // not supposed to happen.)
  uint8_t block[STEP_SIZE];
  if (simdjson_unlikely(reader.get_remainder(block) == 0)) { return UNEXPECTED_ERROR; }
  indexer.step<STEP_SIZE>(block, reader);
  return indexer.finish(parser, reader.block_index(), len, partial);
}

template<>
simdjson_inline void json_structural_indexer::step<128>(const uint8_t *block, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  simd::simd8x64<uint8_t> in_2(block+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1, reader.block_index());
  this->next(in_2, block_2, reader.block_index()+64);
  reader.advance();
}

template<>
simdjson_inline void json_structural_indexer::step<64>(const uint8_t *block, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  json_block block_1 = scanner.next(in_1);
  this->next(in_1, block_1, reader.block_index());
  reader.advance();
}

simdjson_inline void json_structural_indexer::next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx) {
  uint64_t unescaped = in.lteq(0x1F);
#if SIMDJSON_UTF8VALIDATION
  checker.check_next_input(in);
#endif
  indexer.write(uint32_t(idx-64), prev_structurals); // Output *last* iteration's structurals to the parser
  prev_structurals = block.structural_start();
  unescaped_chars_error |= block.non_quote_inside_string(unescaped);
}

simdjson_inline error_code json_structural_indexer::finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial) {
  // Write out the final iteration's structurals
  indexer.write(uint32_t(idx-64), prev_structurals);
  error_code error = scanner.finish();
  // We deliberately break down the next expression so that it is
  // human readable.
  const bool should_we_exit = is_streaming(partial) ?
    ((error != SUCCESS) && (error != UNCLOSED_STRING)) // when partial we tolerate UNCLOSED_STRING
    : (error != SUCCESS); // if partial is false, we must have SUCCESS
  const bool have_unclosed_string = (error == UNCLOSED_STRING);
  if (simdjson_unlikely(should_we_exit)) { return error; }

  if (unescaped_chars_error) {
    return UNESCAPED_CHARS;
  }
  parser.n_structural_indexes = uint32_t(indexer.tail - parser.structural_indexes.get());
  /***
   * The On Demand API requires special padding.
   *
   * This is related to https://github.com/simdjson/simdjson/issues/906
   * Basically, we want to make sure that if the parsing continues beyond the last (valid)
   * structural character, it quickly stops.
   * Only three structural characters can be repeated without triggering an error in JSON:  [,] and }.
   * We repeat the padding character (at 'len'). We don't know what it is, but if the parsing
   * continues, then it must be [,] or }.
   * Suppose it is ] or }. We backtrack to the first character, what could it be that would
   * not trigger an error? It could be ] or } but no, because you can't start a document that way.
   * It can't be a comma, a colon or any simple value. So the only way we could continue is
   * if the repeated character is [. But if so, the document must start with [. But if the document
   * starts with [, it should end with ]. If we enforce that rule, then we would get
   * ][[ which is invalid.
   *
   * This is illustrated with the test array_iterate_unclosed_error() on the following input:
   * R"({ "a": [,,)"
   **/
  parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len); // used later in partial == stage1_mode::streaming_final
  parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
  parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
  parser.next_structural_index = 0;
  // a valid JSON file cannot have zero structural indexes - we should have found something
  if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
    return EMPTY;
  }
  if (simdjson_unlikely(parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
    return UNEXPECTED_ERROR;
  }
  if (partial == stage1_mode::streaming_partial) {
    // If we have an unclosed string, then the last structural
    // will be the quote and we want to make sure to omit it.
    if(have_unclosed_string) {
      parser.n_structural_indexes--;
      // a valid JSON file cannot have zero structural indexes - we should have found something
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }

    parser.n_structural_indexes = new_structural_indexes;
  } else if (partial == stage1_mode::streaming_final) {
    if(have_unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
        // We tolerate an unclosed string at the very end of the stream. Indeed, users
        // often load their data in bulk without being careful and they want us to ignore
        // the trailing garbage.
        return EMPTY;
    }
  }
  checker.check_eof();
  return checker.errors();
}

} // namespace stage1
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/json_structural_indexer.h */
/* begin file src/generic/stage1/utf8_validator.h */
namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

/**
 * Validates that the string is actual UTF-8.
 */
template<class checker>
bool generic_validate_utf8(const uint8_t * input, size_t length) {
    checker c{};
    buf_block_reader<64> reader(input, length);
    while (reader.has_full_block()) {
      simd::simd8x64<uint8_t> in(reader.full_block());
      c.check_next_input(in);
      reader.advance();
    }
    uint8_t block[64]{};
    reader.get_remainder(block);
    simd::simd8x64<uint8_t> in(block);
    c.check_next_input(in);
    reader.advance();
    c.check_eof();
    return c.errors() == error_code::SUCCESS;
}

bool generic_validate_utf8(const char * input, size_t length) {
    return generic_validate_utf8<utf8_checker>(reinterpret_cast<const uint8_t *>(input),length);
}

} // namespace stage1
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage1/utf8_validator.h */

//
// Stage 2
//
/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace ppc64 {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace ppc64 {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace ppc64 {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace ppc64 {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace ppc64 {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace ppc64
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

//
// Implementation-specific overrides
//
namespace simdjson {
namespace ppc64 {
namespace {
namespace stage1 {

simdjson_inline uint64_t json_string_scanner::find_escaped(uint64_t backslash) {
  // On PPC, we don't short-circuit this if there are no backslashes, because the branch gives us no
  // benefit and therefore makes things worse.
  // if (!backslash) { uint64_t escaped = prev_escaped; prev_escaped = 0; return escaped; }
  return find_escaped_branchless(backslash);
}

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return ppc64::stage1::json_minifier::minify<64>(buf, len, dst, dst_len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return ppc64::stage1::json_structural_indexer::index<64>(buf, len, *this, streaming);
}

simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return ppc64::stage1::generic_validate_utf8(buf,len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool replacement_char) const noexcept {
  return ppc64::stringparsing::parse_string(src, dst, replacement_char);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return ppc64::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace ppc64
} // namespace simdjson

/* begin file include/simdjson/ppc64/end.h */
/* end file include/simdjson/ppc64/end.h */
/* end file src/ppc64/dom_parser_implementation.cpp */
#endif
#if SIMDJSON_IMPLEMENTATION_WESTMERE
/* begin file src/westmere/implementation.cpp */
/* begin file include/simdjson/westmere/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "westmere"
// #define SIMDJSON_IMPLEMENTATION westmere
SIMDJSON_TARGET_WESTMERE
/* end file include/simdjson/westmere/begin.h */

namespace simdjson {
namespace westmere {

simdjson_warn_unused error_code implementation::create_dom_parser_implementation(
  size_t capacity,
  size_t max_depth,
  std::unique_ptr<internal::dom_parser_implementation>& dst
) const noexcept {
  dst.reset( new (std::nothrow) dom_parser_implementation() );
  if (!dst) { return MEMALLOC; }
  if (auto err = dst->set_capacity(capacity))
    return err;
  if (auto err = dst->set_max_depth(max_depth))
    return err;
  return SUCCESS;
}

} // namespace westmere
} // namespace simdjson

/* begin file include/simdjson/westmere/end.h */
SIMDJSON_UNTARGET_WESTMERE
/* end file include/simdjson/westmere/end.h */
/* end file src/westmere/implementation.cpp */
/* begin file src/westmere/dom_parser_implementation.cpp */
/* begin file include/simdjson/westmere/begin.h */
// redefining SIMDJSON_IMPLEMENTATION to "westmere"
// #define SIMDJSON_IMPLEMENTATION westmere
SIMDJSON_TARGET_WESTMERE
/* end file include/simdjson/westmere/begin.h */

//
// Stage 1
//

namespace simdjson {
namespace westmere {
namespace {

using namespace simd;

struct json_character_block {
  static simdjson_inline json_character_block classify(const simd::simd8x64<uint8_t>& in);

  simdjson_inline uint64_t whitespace() const noexcept { return _whitespace; }
  simdjson_inline uint64_t op() const noexcept { return _op; }
  simdjson_inline uint64_t scalar() const noexcept { return ~(op() | whitespace()); }

  uint64_t _whitespace;
  uint64_t _op;
};

simdjson_inline json_character_block json_character_block::classify(const simd::simd8x64<uint8_t>& in) {
  // These lookups rely on the fact that anything < 127 will match the lower 4 bits, which is why
  // we can't use the generic lookup_16.
  auto whitespace_table = simd8<uint8_t>::repeat_16(' ', 100, 100, 100, 17, 100, 113, 2, 100, '\t', '\n', 112, 100, '\r', 100, 100);

  // The 6 operators (:,[]{}) have these values:
  //
  // , 2C
  // : 3A
  // [ 5B
  // { 7B
  // ] 5D
  // } 7D
  //
  // If you use | 0x20 to turn [ and ] into { and }, the lower 4 bits of each character is unique.
  // We exploit this, using a simd 4-bit lookup to tell us which character match against, and then
  // match it (against | 0x20).
  //
  // To prevent recognizing other characters, everything else gets compared with 0, which cannot
  // match due to the | 0x20.
  //
  // NOTE: Due to the | 0x20, this ALSO treats <FF> and <SUB> (control characters 0C and 1A) like ,
  // and :. This gets caught in stage 2, which checks the actual character to ensure the right
  // operators are in the right places.
  const auto op_table = simd8<uint8_t>::repeat_16(
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, ':', '{', // : = 3A, [ = 5B, { = 7B
    ',', '}', 0, 0  // , = 2C, ] = 5D, } = 7D
  );

  // We compute whitespace and op separately. If the code later only use one or the
  // other, given the fact that all functions are aggressively inlined, we can
  // hope that useless computations will be omitted. This is namely case when
  // minifying (we only need whitespace).


  const uint64_t whitespace = in.eq({
    _mm_shuffle_epi8(whitespace_table, in.chunks[0]),
    _mm_shuffle_epi8(whitespace_table, in.chunks[1]),
    _mm_shuffle_epi8(whitespace_table, in.chunks[2]),
    _mm_shuffle_epi8(whitespace_table, in.chunks[3])
  });
  // Turn [ and ] into { and }
  const simd8x64<uint8_t> curlified{
    in.chunks[0] | 0x20,
    in.chunks[1] | 0x20,
    in.chunks[2] | 0x20,
    in.chunks[3] | 0x20
  };
  const uint64_t op = curlified.eq({
    _mm_shuffle_epi8(op_table, in.chunks[0]),
    _mm_shuffle_epi8(op_table, in.chunks[1]),
    _mm_shuffle_epi8(op_table, in.chunks[2]),
    _mm_shuffle_epi8(op_table, in.chunks[3])
  });
    return { whitespace, op };
}

simdjson_inline bool is_ascii(const simd8x64<uint8_t>& input) {
  return input.reduce_or().is_ascii();
}

simdjson_unused simdjson_inline simd8<bool> must_be_continuation(const simd8<uint8_t> prev1, const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_second_byte = prev1.saturating_sub(0xc0u-1); // Only 11______ will be > 0
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_second_byte | is_third_byte | is_fourth_byte) > int8_t(0);
}

simdjson_inline simd8<bool> must_be_2_3_continuation(const simd8<uint8_t> prev2, const simd8<uint8_t> prev3) {
  simd8<uint8_t> is_third_byte  = prev2.saturating_sub(0xe0u-1); // Only 111_____ will be > 0
  simd8<uint8_t> is_fourth_byte = prev3.saturating_sub(0xf0u-1); // Only 1111____ will be > 0
  // Caller requires a bool (all 1's). All values resulting from the subtraction will be <= 64, so signed comparison is fine.
  return simd8<int8_t>(is_third_byte | is_fourth_byte) > int8_t(0);
}

} // unnamed namespace
} // namespace westmere
} // namespace simdjson

/* begin file src/generic/stage1/utf8_lookup4_algorithm.h */
namespace simdjson {
namespace westmere {
namespace {
namespace utf8_validation {

using namespace simd;

  simdjson_inline simd8<uint8_t> check_special_cases(const simd8<uint8_t> input, const simd8<uint8_t> prev1) {
// Bit 0 = Too Short (lead byte/ASCII followed by lead byte/ASCII)
// Bit 1 = Too Long (ASCII followed by continuation)
// Bit 2 = Overlong 3-byte
// Bit 4 = Surrogate
// Bit 5 = Overlong 2-byte
// Bit 7 = Two Continuations
    constexpr const uint8_t TOO_SHORT   = 1<<0; // 11______ 0_______
                                                // 11______ 11______
    constexpr const uint8_t TOO_LONG    = 1<<1; // 0_______ 10______
    constexpr const uint8_t OVERLONG_3  = 1<<2; // 11100000 100_____
    constexpr const uint8_t SURROGATE   = 1<<4; // 11101101 101_____
    constexpr const uint8_t OVERLONG_2  = 1<<5; // 1100000_ 10______
    constexpr const uint8_t TWO_CONTS   = 1<<7; // 10______ 10______
    constexpr const uint8_t TOO_LARGE   = 1<<3; // 11110100 1001____
                                                // 11110100 101_____
                                                // 11110101 1001____
                                                // 11110101 101_____
                                                // 1111011_ 1001____
                                                // 1111011_ 101_____
                                                // 11111___ 1001____
                                                // 11111___ 101_____
    constexpr const uint8_t TOO_LARGE_1000 = 1<<6;
                                                // 11110101 1000____
                                                // 1111011_ 1000____
                                                // 11111___ 1000____
    constexpr const uint8_t OVERLONG_4  = 1<<6; // 11110000 1000____

    const simd8<uint8_t> byte_1_high = prev1.shr<4>().lookup_16<uint8_t>(
      // 0_______ ________ <ASCII in byte 1>
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      TOO_LONG, TOO_LONG, TOO_LONG, TOO_LONG,
      // 10______ ________ <continuation in byte 1>
      TWO_CONTS, TWO_CONTS, TWO_CONTS, TWO_CONTS,
      // 1100____ ________ <two byte lead in byte 1>
      TOO_SHORT | OVERLONG_2,
      // 1101____ ________ <two byte lead in byte 1>
      TOO_SHORT,
      // 1110____ ________ <three byte lead in byte 1>
      TOO_SHORT | OVERLONG_3 | SURROGATE,
      // 1111____ ________ <four+ byte lead in byte 1>
      TOO_SHORT | TOO_LARGE | TOO_LARGE_1000 | OVERLONG_4
    );
    constexpr const uint8_t CARRY = TOO_SHORT | TOO_LONG | TWO_CONTS; // These all have ____ in byte 1 .
    const simd8<uint8_t> byte_1_low = (prev1 & 0x0F).lookup_16<uint8_t>(
      // ____0000 ________
      CARRY | OVERLONG_3 | OVERLONG_2 | OVERLONG_4,
      // ____0001 ________
      CARRY | OVERLONG_2,
      // ____001_ ________
      CARRY,
      CARRY,

      // ____0100 ________
      CARRY | TOO_LARGE,
      // ____0101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____011_ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,

      // ____1___ ________
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      // ____1101 ________
      CARRY | TOO_LARGE | TOO_LARGE_1000 | SURROGATE,
      CARRY | TOO_LARGE | TOO_LARGE_1000,
      CARRY | TOO_LARGE | TOO_LARGE_1000
    );
    const simd8<uint8_t> byte_2_high = input.shr<4>().lookup_16<uint8_t>(
      // ________ 0_______ <ASCII in byte 2>
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT,

      // ________ 1000____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE_1000 | OVERLONG_4,
      // ________ 1001____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | OVERLONG_3 | TOO_LARGE,
      // ________ 101_____
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,
      TOO_LONG | OVERLONG_2 | TWO_CONTS | SURROGATE  | TOO_LARGE,

      // ________ 11______
      TOO_SHORT, TOO_SHORT, TOO_SHORT, TOO_SHORT
    );
    return (byte_1_high & byte_1_low & byte_2_high);
  }
  simdjson_inline simd8<uint8_t> check_multibyte_lengths(const simd8<uint8_t> input,
      const simd8<uint8_t> prev_input, const simd8<uint8_t> sc) {
    simd8<uint8_t> prev2 = input.prev<2>(prev_input);
    simd8<uint8_t> prev3 = input.prev<3>(prev_input);
    simd8<uint8_t> must23 = simd8<uint8_t>(must_be_2_3_continuation(prev2, prev3));
    simd8<uint8_t> must23_80 = must23 & uint8_t(0x80);
    return must23_80 ^ sc;
  }

  //
  // Return nonzero if there are incomplete multibyte characters at the end of the block:
  // e.g. if there is a 4-byte character, but it's 3 bytes from the end.
  //
  simdjson_inline simd8<uint8_t> is_incomplete(const simd8<uint8_t> input) {
    // If the previous input's last 3 bytes match this, they're too short (they ended at EOF):
    // ... 1111____ 111_____ 11______
#if SIMDJSON_IMPLEMENTATION_ICELAKE
    static const uint8_t max_array[64] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#else
    static const uint8_t max_array[32] = {
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 255, 255, 255,
      255, 255, 255, 255, 255, 0xf0u-1, 0xe0u-1, 0xc0u-1
    };
#endif
    const simd8<uint8_t> max_value(&max_array[sizeof(max_array)-sizeof(simd8<uint8_t>)]);
    return input.gt_bits(max_value);
  }

  struct utf8_checker {
    // If this is nonzero, there has been a UTF-8 error.
    simd8<uint8_t> error;
    // The last input we received
    simd8<uint8_t> prev_input_block;
    // Whether the last input we received was incomplete (used for ASCII fast path)
    simd8<uint8_t> prev_incomplete;

    //
    // Check whether the current bytes are valid UTF-8.
    //
    simdjson_inline void check_utf8_bytes(const simd8<uint8_t> input, const simd8<uint8_t> prev_input) {
      // Flip prev1...prev3 so we can easily determine if they are 2+, 3+ or 4+ lead bytes
      // (2, 3, 4-byte leads become large positive numbers instead of small negative numbers)
      simd8<uint8_t> prev1 = input.prev<1>(prev_input);
      simd8<uint8_t> sc = check_special_cases(input, prev1);
      this->error |= check_multibyte_lengths(input, prev_input, sc);
    }

    // The only problem that can happen at EOF is that a multibyte character is too short
    // or a byte value too large in the last bytes: check_special_cases only checks for bytes
    // too large in the first of two bytes.
    simdjson_inline void check_eof() {
      // If the previous block had incomplete UTF-8 characters at the end, an ASCII block can't
      // possibly finish them.
      this->error |= this->prev_incomplete;
    }

#ifndef SIMDJSON_IF_CONSTEXPR
#if SIMDJSON_CPLUSPLUS17
#define SIMDJSON_IF_CONSTEXPR if constexpr
#else
#define SIMDJSON_IF_CONSTEXPR if
#endif
#endif

    simdjson_inline void check_next_input(const simd8x64<uint8_t>& input) {
      if(simdjson_likely(is_ascii(input))) {
        this->error |= this->prev_incomplete;
      } else {
        // you might think that a for-loop would work, but under Visual Studio, it is not good enough.
        static_assert((simd8x64<uint8_t>::NUM_CHUNKS == 1)
                ||(simd8x64<uint8_t>::NUM_CHUNKS == 2)
                || (simd8x64<uint8_t>::NUM_CHUNKS == 4),
                "We support one, two or four chunks per 64-byte block.");
        SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 1) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 2) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
        } else SIMDJSON_IF_CONSTEXPR (simd8x64<uint8_t>::NUM_CHUNKS == 4) {
          this->check_utf8_bytes(input.chunks[0], this->prev_input_block);
          this->check_utf8_bytes(input.chunks[1], input.chunks[0]);
          this->check_utf8_bytes(input.chunks[2], input.chunks[1]);
          this->check_utf8_bytes(input.chunks[3], input.chunks[2]);
        }
        this->prev_incomplete = is_incomplete(input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1]);
        this->prev_input_block = input.chunks[simd8x64<uint8_t>::NUM_CHUNKS-1];
      }
    }
    // do not forget to call check_eof!
    simdjson_inline error_code errors() {
      return this->error.any_bits_set_anywhere() ? error_code::UTF8_ERROR : error_code::SUCCESS;
    }

  }; // struct utf8_checker
} // namespace utf8_validation

using utf8_validation::utf8_checker;

} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/utf8_lookup4_algorithm.h */
/* begin file src/generic/stage1/json_structural_indexer.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

/* begin file src/generic/stage1/buf_block_reader.h */
namespace simdjson {
namespace westmere {
namespace {

// Walks through a buffer in block-sized increments, loading the last part with spaces
template<size_t STEP_SIZE>
struct buf_block_reader {
public:
  simdjson_inline buf_block_reader(const uint8_t *_buf, size_t _len);
  simdjson_inline size_t block_index();
  simdjson_inline bool has_full_block() const;
  simdjson_inline const uint8_t *full_block() const;
  /**
   * Get the last block, padded with spaces.
   *
   * There will always be a last block, with at least 1 byte, unless len == 0 (in which case this
   * function fills the buffer with spaces and returns 0. In particular, if len == STEP_SIZE there
   * will be 0 full_blocks and 1 remainder block with STEP_SIZE bytes and no spaces for padding.
   *
   * @return the number of effective characters in the last block.
   */
  simdjson_inline size_t get_remainder(uint8_t *dst) const;
  simdjson_inline void advance();
private:
  const uint8_t *buf;
  const size_t len;
  const size_t lenminusstep;
  size_t idx;
};

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text_64(const uint8_t *text) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    buf[i] = int8_t(text[i]) < ' ' ? '_' : int8_t(text[i]);
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

// Routines to print masks and text for debugging bitmask operations
simdjson_unused static char * format_input_text(const simd8x64<uint8_t>& in) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  in.store(reinterpret_cast<uint8_t*>(buf));
  for (size_t i=0; i<sizeof(simd8x64<uint8_t>); i++) {
    if (buf[i] < ' ') { buf[i] = '_'; }
  }
  buf[sizeof(simd8x64<uint8_t>)] = '\0';
  return buf;
}

simdjson_unused static char * format_mask(uint64_t mask) {
  static char buf[sizeof(simd8x64<uint8_t>) + 1];
  for (size_t i=0; i<64; i++) {
    buf[i] = (mask & (size_t(1) << i)) ? 'X' : ' ';
  }
  buf[64] = '\0';
  return buf;
}

template<size_t STEP_SIZE>
simdjson_inline buf_block_reader<STEP_SIZE>::buf_block_reader(const uint8_t *_buf, size_t _len) : buf{_buf}, len{_len}, lenminusstep{len < STEP_SIZE ? 0 : len - STEP_SIZE}, idx{0} {}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::block_index() { return idx; }

template<size_t STEP_SIZE>
simdjson_inline bool buf_block_reader<STEP_SIZE>::has_full_block() const {
  return idx < lenminusstep;
}

template<size_t STEP_SIZE>
simdjson_inline const uint8_t *buf_block_reader<STEP_SIZE>::full_block() const {
  return &buf[idx];
}

template<size_t STEP_SIZE>
simdjson_inline size_t buf_block_reader<STEP_SIZE>::get_remainder(uint8_t *dst) const {
  if(len == idx) { return 0; } // memcpy(dst, null, 0) will trigger an error with some sanitizers
  std::memset(dst, 0x20, STEP_SIZE); // std::memset STEP_SIZE because it's more efficient to write out 8 or 16 bytes at once.
  std::memcpy(dst, buf + idx, len - idx);
  return len - idx;
}

template<size_t STEP_SIZE>
simdjson_inline void buf_block_reader<STEP_SIZE>::advance() {
  idx += STEP_SIZE;
}

} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/buf_block_reader.h */
/* begin file src/generic/stage1/json_string_scanner.h */
namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

struct json_string_block {
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_string_block(uint64_t backslash, uint64_t escaped, uint64_t quote, uint64_t in_string) :
  _backslash(backslash), _escaped(escaped), _quote(quote), _in_string(in_string) {}

  // Escaped characters (characters following an escape() character)
  simdjson_inline uint64_t escaped() const { return _escaped; }
  // Escape characters (backslashes that are not escaped--i.e. in \\, includes only the first \)
  simdjson_inline uint64_t escape() const { return _backslash & ~_escaped; }
  // Real (non-backslashed) quotes
  simdjson_inline uint64_t quote() const { return _quote; }
  // Start quotes of strings
  simdjson_inline uint64_t string_start() const { return _quote & _in_string; }
  // End quotes of strings
  simdjson_inline uint64_t string_end() const { return _quote & ~_in_string; }
  // Only characters inside the string (not including the quotes)
  simdjson_inline uint64_t string_content() const { return _in_string & ~_quote; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const { return mask & _in_string; }
  // Return a mask of whether the given characters are inside a string (only works on non-quotes)
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const { return mask & ~_in_string; }
  // Tail of string (everything except the start quote)
  simdjson_inline uint64_t string_tail() const { return _in_string ^ _quote; }

  // backslash characters
  uint64_t _backslash;
  // escaped characters (backslashed--does not include the hex characters after \u)
  uint64_t _escaped;
  // real quotes (non-backslashed ones)
  uint64_t _quote;
  // string characters (includes start quote but not end quote)
  uint64_t _in_string;
};

// Scans blocks for string characters, storing the state necessary to do so
class json_string_scanner {
public:
  simdjson_inline json_string_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Intended to be defined by the implementation
  simdjson_inline uint64_t find_escaped(uint64_t escape);
  simdjson_inline uint64_t find_escaped_branchless(uint64_t escape);

  // Whether the last iteration was still inside a string (all 1's = true, all 0's = false).
  uint64_t prev_in_string = 0ULL;
  // Whether the first character of the next iteration is escaped.
  uint64_t prev_escaped = 0ULL;
};

//
// Finds escaped characters (characters following \).
//
// Handles runs of backslashes like \\\" and \\\\" correctly (yielding 0101 and 01010, respectively).
//
// Does this by:
// - Shift the escape mask to get potentially escaped characters (characters after backslashes).
// - Mask escaped sequences that start on *even* bits with 1010101010 (odd bits are escaped, even bits are not)
// - Mask escaped sequences that start on *odd* bits with 0101010101 (even bits are escaped, odd bits are not)
//
// To distinguish between escaped sequences starting on even/odd bits, it finds the start of all
// escape sequences, filters out the ones that start on even bits, and adds that to the mask of
// escape sequences. This causes the addition to clear out the sequences starting on odd bits (since
// the start bit causes a carry), and leaves even-bit sequences alone.
//
// Example:
//
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
// escape         |  xxx |  xx xxx  xxx  xx xx  | Removed overflow backslash; will | it into follows_escape
// odd_starts     |  x   |  x       x       x   | escape & ~even_bits & ~follows_escape
// even_seq       |     c|    cxxx     c xx   c | c = carry bit -- will be masked out later
// invert_mask    |      |     cxxx     c xx   c| even_seq << 1
// follows_escape |   xx | x xx xxx  xxx  xx xx | Includes overflow bit
// escaped        |   x  | x x  x x  x x  x  x  |
// desired        |   x  | x x  x x  x x  x  x  |
// text           |  \\\ | \\\"\\\" \\\" \\"\\" |
//
simdjson_inline uint64_t json_string_scanner::find_escaped_branchless(uint64_t backslash) {
  // If there was overflow, pretend the first character isn't a backslash
  backslash &= ~prev_escaped;
  uint64_t follows_escape = backslash << 1 | prev_escaped;

  // Get sequences starting on even bits by clearing out the odd series using +
  const uint64_t even_bits = 0x5555555555555555ULL;
  uint64_t odd_sequence_starts = backslash & ~even_bits & ~follows_escape;
  uint64_t sequences_starting_on_even_bits;
  prev_escaped = add_overflow(odd_sequence_starts, backslash, &sequences_starting_on_even_bits);
  uint64_t invert_mask = sequences_starting_on_even_bits << 1; // The mask we want to return is the *escaped* bits, not escapes.

  // Mask every other backslashed character as an escaped character
  // Flip the mask for sequences that start on even bits, to correct them
  return (even_bits ^ invert_mask) & follows_escape;
}

//
// Return a mask of all string characters plus end quotes.
//
// prev_escaped is overflow saying whether the next character is escaped.
// prev_in_string is overflow saying whether we're still in a string.
//
// Backslash sequences outside of quotes will be detected in stage 2.
//
simdjson_inline json_string_block json_string_scanner::next(const simd::simd8x64<uint8_t>& in) {
  const uint64_t backslash = in.eq('\\');
  const uint64_t escaped = find_escaped(backslash);
  const uint64_t quote = in.eq('"') & ~escaped;

  //
  // prefix_xor flips on bits inside the string (and flips off the end quote).
  //
  // Then we xor with prev_in_string: if we were in a string already, its effect is flipped
  // (characters inside strings are outside, and characters outside strings are inside).
  //
  const uint64_t in_string = prefix_xor(quote) ^ prev_in_string;

  //
  // Check if we're still in a string at the end of the box so the next block will know
  //
  // right shift of a signed value expected to be well-defined and standard
  // compliant as of C++20, John Regher from Utah U. says this is fine code
  //
  prev_in_string = uint64_t(static_cast<int64_t>(in_string) >> 63);

  // Use ^ to turn the beginning quote off, and the end quote on.

  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_string_block(
    backslash,
    escaped,
    quote,
    in_string
  );
}

simdjson_inline error_code json_string_scanner::finish() {
  if (prev_in_string) {
    return UNCLOSED_STRING;
  }
  return SUCCESS;
}

} // namespace stage1
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/json_string_scanner.h */
/* begin file src/generic/stage1/json_scanner.h */
namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

/**
 * A block of scanned json, with information on operators and scalars.
 *
 * We seek to identify pseudo-structural characters. Anything that is inside
 * a string must be omitted (hence  & ~_string.string_tail()).
 * Otherwise, pseudo-structural characters come in two forms.
 * 1. We have the structural characters ([,],{,},:, comma). The
 *    term 'structural character' is from the JSON RFC.
 * 2. We have the 'scalar pseudo-structural characters'.
 *    Scalars are quotes, and any character except structural characters and white space.
 *
 * To identify the scalar pseudo-structural characters, we must look at what comes
 * before them: it must be a space, a quote or a structural characters.
 * Starting with simdjson v0.3, we identify them by
 * negation: we identify everything that is followed by a non-quote scalar,
 * and we negate that. Whatever remains must be a 'scalar pseudo-structural character'.
 */
struct json_block {
public:
  // We spell out the constructors in the hope of resolving inlining issues with Visual Studio 2017
  simdjson_inline json_block(json_string_block&& string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(std::move(string)), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}
  simdjson_inline json_block(json_string_block string, json_character_block characters, uint64_t follows_potential_nonquote_scalar) :
  _string(string), _characters(characters), _follows_potential_nonquote_scalar(follows_potential_nonquote_scalar) {}

  /**
   * The start of structurals.
   * In simdjson prior to v0.3, these were called the pseudo-structural characters.
   **/
  simdjson_inline uint64_t structural_start() const noexcept { return potential_structural_start() & ~_string.string_tail(); }
  /** All JSON whitespace (i.e. not in a string) */
  simdjson_inline uint64_t whitespace() const noexcept { return non_quote_outside_string(_characters.whitespace()); }

  // Helpers

  /** Whether the given characters are inside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_inside_string(uint64_t mask) const noexcept { return _string.non_quote_inside_string(mask); }
  /** Whether the given characters are outside a string (only works on non-quotes) */
  simdjson_inline uint64_t non_quote_outside_string(uint64_t mask) const noexcept { return _string.non_quote_outside_string(mask); }

  // string and escape characters
  json_string_block _string;
  // whitespace, structural characters ('operators'), scalars
  json_character_block _characters;
  // whether the previous character was a scalar
  uint64_t _follows_potential_nonquote_scalar;
private:
  // Potential structurals (i.e. disregarding strings)

  /**
   * structural elements ([,],{,},:, comma) plus scalar starts like 123, true and "abc".
   * They may reside inside a string.
   **/
  simdjson_inline uint64_t potential_structural_start() const noexcept { return _characters.op() | potential_scalar_start(); }
  /**
   * The start of non-operator runs, like 123, true and "abc".
   * It main reside inside a string.
   **/
  simdjson_inline uint64_t potential_scalar_start() const noexcept {
    // The term "scalar" refers to anything except structural characters and white space
    // (so letters, numbers, quotes).
    // Whenever it is preceded by something that is not a structural element ({,},[,],:, ") nor a white-space
    // then we know that it is irrelevant structurally.
    return _characters.scalar() & ~follows_potential_scalar();
  }
  /**
   * Whether the given character is immediately after a non-operator like 123, true.
   * The characters following a quote are not included.
   */
  simdjson_inline uint64_t follows_potential_scalar() const noexcept {
    // _follows_potential_nonquote_scalar: is defined as marking any character that follows a character
    // that is not a structural element ({,},[,],:, comma) nor a quote (") and that is not a
    // white space.
    // It is understood that within quoted region, anything at all could be marked (irrelevant).
    return _follows_potential_nonquote_scalar;
  }
};

/**
 * Scans JSON for important bits: structural characters or 'operators', strings, and scalars.
 *
 * The scanner starts by calculating two distinct things:
 * - string characters (taking \" into account)
 * - structural characters or 'operators' ([]{},:, comma)
 *   and scalars (runs of non-operators like 123, true and "abc")
 *
 * To minimize data dependency (a key component of the scanner's speed), it finds these in parallel:
 * in particular, the operator/scalar bit will find plenty of things that are actually part of
 * strings. When we're done, json_block will fuse the two together by masking out tokens that are
 * part of a string.
 */
class json_scanner {
public:
  json_scanner() = default;
  simdjson_inline json_block next(const simd::simd8x64<uint8_t>& in);
  // Returns either UNCLOSED_STRING or SUCCESS
  simdjson_inline error_code finish();

private:
  // Whether the last character of the previous iteration is part of a scalar token
  // (anything except whitespace or a structural character/'operator').
  uint64_t prev_scalar = 0ULL;
  json_string_scanner string_scanner{};
};


//
// Check if the current character immediately follows a matching character.
//
// For example, this checks for quotes with backslashes in front of them:
//
//     const uint64_t backslashed_quote = in.eq('"') & immediately_follows(in.eq('\'), prev_backslash);
//
simdjson_inline uint64_t follows(const uint64_t match, uint64_t &overflow) {
  const uint64_t result = match << 1 | overflow;
  overflow = match >> 63;
  return result;
}

simdjson_inline json_block json_scanner::next(const simd::simd8x64<uint8_t>& in) {
  json_string_block strings = string_scanner.next(in);
  // identifies the white-space and the structural characters
  json_character_block characters = json_character_block::classify(in);
  // The term "scalar" refers to anything except structural characters and white space
  // (so letters, numbers, quotes).
  // We want follows_scalar to mark anything that follows a non-quote scalar (so letters and numbers).
  //
  // A terminal quote should either be followed by a structural character (comma, brace, bracket, colon)
  // or nothing. However, we still want ' "a string"true ' to mark the 't' of 'true' as a potential
  // pseudo-structural character just like we would if we had  ' "a string" true '; otherwise we
  // may need to add an extra check when parsing strings.
  //
  // Performance: there are many ways to skin this cat.
  const uint64_t nonquote_scalar = characters.scalar() & ~strings.quote();
  uint64_t follows_nonquote_scalar = follows(nonquote_scalar, prev_scalar);
  // We are returning a function-local object so either we get a move constructor
  // or we get copy elision.
  return json_block(
    strings,// strings is a function-local object so either it moves or the copy is elided.
    characters,
    follows_nonquote_scalar
  );
}

simdjson_inline error_code json_scanner::finish() {
  return string_scanner.finish();
}

} // namespace stage1
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/json_scanner.h */
/* begin file src/generic/stage1/json_minifier.h */
// This file contains the common code every implementation uses in stage1
// It is intended to be included multiple times and compiled multiple times
// We assume the file in which it is included already includes
// "simdjson/stage1.h" (this simplifies amalgation)

namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

class json_minifier {
public:
  template<size_t STEP_SIZE>
  static error_code minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept;

private:
  simdjson_inline json_minifier(uint8_t *_dst)
  : dst{_dst}
  {}
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block_buf, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block);
  simdjson_inline error_code finish(uint8_t *dst_start, size_t &dst_len);
  json_scanner scanner{};
  uint8_t *dst;
};

simdjson_inline void json_minifier::next(const simd::simd8x64<uint8_t>& in, const json_block& block) {
  uint64_t mask = block.whitespace();
  dst += in.compress(mask, dst);
}

simdjson_inline error_code json_minifier::finish(uint8_t *dst_start, size_t &dst_len) {
  error_code error = scanner.finish();
  if (error) { dst_len = 0; return error; }
  dst_len = dst - dst_start;
  return SUCCESS;
}

template<>
simdjson_inline void json_minifier::step<128>(const uint8_t *block_buf, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  simd::simd8x64<uint8_t> in_2(block_buf+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1);
  this->next(in_2, block_2);
  reader.advance();
}

template<>
simdjson_inline void json_minifier::step<64>(const uint8_t *block_buf, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block_buf);
  json_block block_1 = scanner.next(in_1);
  this->next(block_buf, block_1);
  reader.advance();
}

template<size_t STEP_SIZE>
error_code json_minifier::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) noexcept {
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_minifier minifier(dst);

  // Index the first n-1 blocks
  while (reader.has_full_block()) {
    minifier.step<STEP_SIZE>(reader.full_block(), reader);
  }

  // Index the last (remainder) block, padded with spaces
  uint8_t block[STEP_SIZE];
  size_t remaining_bytes = reader.get_remainder(block);
  if (remaining_bytes > 0) {
    // We do not want to write directly to the output stream. Rather, we write
    // to a local buffer (for safety).
    uint8_t out_block[STEP_SIZE];
    uint8_t * const guarded_dst{minifier.dst};
    minifier.dst = out_block;
    minifier.step<STEP_SIZE>(block, reader);
    size_t to_write = minifier.dst - out_block;
    // In some cases, we could be enticed to consider the padded spaces
    // as part of the string. This is fine as long as we do not write more
    // than we consumed.
    if(to_write > remaining_bytes) { to_write = remaining_bytes; }
    memcpy(guarded_dst, out_block, to_write);
    minifier.dst = guarded_dst + to_write;
  }
  return minifier.finish(dst, dst_len);
}

} // namespace stage1
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/json_minifier.h */
/* begin file src/generic/stage1/find_next_document_index.h */
namespace simdjson {
namespace westmere {
namespace {

/**
  * This algorithm is used to quickly identify the last structural position that
  * makes up a complete document.
  *
  * It does this by going backwards and finding the last *document boundary* (a
  * place where one value follows another without a comma between them). If the
  * last document (the characters after the boundary) has an equal number of
  * start and end brackets, it is considered complete.
  *
  * Simply put, we iterate over the structural characters, starting from
  * the end. We consider that we found the end of a JSON document when the
  * first element of the pair is NOT one of these characters: '{' '[' ':' ','
  * and when the second element is NOT one of these characters: '}' ']' ':' ','.
  *
  * This simple comparison works most of the time, but it does not cover cases
  * where the batch's structural indexes contain a perfect amount of documents.
  * In such a case, we do not have access to the structural index which follows
  * the last document, therefore, we do not have access to the second element in
  * the pair, and that means we cannot identify the last document. To fix this
  * issue, we keep a count of the open and closed curly/square braces we found
  * while searching for the pair. When we find a pair AND the count of open and
  * closed curly/square braces is the same, we know that we just passed a
  * complete document, therefore the last json buffer location is the end of the
  * batch.
  */
simdjson_inline uint32_t find_next_document_index(dom_parser_implementation &parser) {
  // Variant: do not count separately, just figure out depth
  if(parser.n_structural_indexes == 0) { return 0; }
  auto arr_cnt = 0;
  auto obj_cnt = 0;
  for (auto i = parser.n_structural_indexes - 1; i > 0; i--) {
    auto idxb = parser.structural_indexes[i];
    switch (parser.buf[idxb]) {
    case ':':
    case ',':
      continue;
    case '}':
      obj_cnt--;
      continue;
    case ']':
      arr_cnt--;
      continue;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
    }
    auto idxa = parser.structural_indexes[i - 1];
    switch (parser.buf[idxa]) {
    case '{':
    case '[':
    case ':':
    case ',':
      continue;
    }
    // Last document is complete, so the next document will appear after!
    if (!arr_cnt && !obj_cnt) {
      return parser.n_structural_indexes;
    }
    // Last document is incomplete; mark the document at i + 1 as the next one
    return i;
  }
  // If we made it to the end, we want to finish counting to see if we have a full document.
  switch (parser.buf[parser.structural_indexes[0]]) {
    case '}':
      obj_cnt--;
      break;
    case ']':
      arr_cnt--;
      break;
    case '{':
      obj_cnt++;
      break;
    case '[':
      arr_cnt++;
      break;
  }
  if (!arr_cnt && !obj_cnt) {
    // We have a complete document.
    return parser.n_structural_indexes;
  }
  return 0;
}

} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/find_next_document_index.h */

namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

class bit_indexer {
public:
  uint32_t *tail;

  simdjson_inline bit_indexer(uint32_t *index_buf) : tail(index_buf) {}

  // flatten out values in 'bits' assuming that they are are to have values of idx
  // plus their position in the bitvector, and store these indexes at
  // base_ptr[base] incrementing base as we go
  // will potentially store extra values beyond end of valid bits, so base_ptr
  // needs to be large enough to handle this
  //
  // If the kernel sets SIMDJSON_CUSTOM_BIT_INDEXER, then it will provide its own
  // version of the code.
#ifdef SIMDJSON_CUSTOM_BIT_INDEXER
  simdjson_inline void write(uint32_t idx, uint64_t bits);
#else
  simdjson_inline void write(uint32_t idx, uint64_t bits) {
    // In some instances, the next branch is expensive because it is mispredicted.
    // Unfortunately, in other cases,
    // it helps tremendously.
    if (bits == 0)
        return;
#if SIMDJSON_PREFER_REVERSE_BITS
    /**
     * ARM lacks a fast trailing zero instruction, but it has a fast
     * bit reversal instruction and a fast leading zero instruction.
     * Thus it may be profitable to reverse the bits (once) and then
     * to rely on a sequence of instructions that call the leading
     * zero instruction.
     *
     * Performance notes:
     * The chosen routine is not optimal in terms of data dependency
     * since zero_leading_bit might require two instructions. However,
     * it tends to minimize the total number of instructions which is
     * beneficial.
     */

    uint64_t rev_bits = reverse_bits(bits);
    int cnt = static_cast<int>(count_ones(bits));
    int i = 0;
    // Do the first 8 all together
    for (; i<8; i++) {
      int lz = leading_zeroes(rev_bits);
      this->tail[i] = static_cast<uint32_t>(idx) + lz;
      rev_bits = zero_leading_bit(rev_bits, lz);
    }
    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      i = 8;
      for (; i<16; i++) {
        int lz = leading_zeroes(rev_bits);
        this->tail[i] = static_cast<uint32_t>(idx) + lz;
        rev_bits = zero_leading_bit(rev_bits, lz);
      }


      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        i = 16;
        while (rev_bits != 0) {
          int lz = leading_zeroes(rev_bits);
          this->tail[i++] = static_cast<uint32_t>(idx) + lz;
          rev_bits = zero_leading_bit(rev_bits, lz);
        }
      }
    }
    this->tail += cnt;
#else // SIMDJSON_PREFER_REVERSE_BITS
    /**
     * Under recent x64 systems, we often have both a fast trailing zero
     * instruction and a fast 'clear-lower-bit' instruction so the following
     * algorithm can be competitive.
     */

    int cnt = static_cast<int>(count_ones(bits));
    // Do the first 8 all together
    for (int i=0; i<8; i++) {
      this->tail[i] = idx + trailing_zeroes(bits);
      bits = clear_lowest_bit(bits);
    }

    // Do the next 8 all together (we hope in most cases it won't happen at all
    // and the branch is easily predicted).
    if (simdjson_unlikely(cnt > 8)) {
      for (int i=8; i<16; i++) {
        this->tail[i] = idx + trailing_zeroes(bits);
        bits = clear_lowest_bit(bits);
      }

      // Most files don't have 16+ structurals per block, so we take several basically guaranteed
      // branch mispredictions here. 16+ structurals per block means either punctuation ({} [] , :)
      // or the start of a value ("abc" true 123) every four characters.
      if (simdjson_unlikely(cnt > 16)) {
        int i = 16;
        do {
          this->tail[i] = idx + trailing_zeroes(bits);
          bits = clear_lowest_bit(bits);
          i++;
        } while (i < cnt);
      }
    }

    this->tail += cnt;
#endif
  }
#endif // SIMDJSON_CUSTOM_BIT_INDEXER

};

class json_structural_indexer {
public:
  /**
   * Find the important bits of JSON in a 128-byte chunk, and add them to structural_indexes.
   *
   * @param partial Setting the partial parameter to true allows the find_structural_bits to
   *   tolerate unclosed strings. The caller should still ensure that the input is valid UTF-8. If
   *   you are processing substrings, you may want to call on a function like trimmed_length_safe_utf8.
   */
  template<size_t STEP_SIZE>
  static error_code index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept;

private:
  simdjson_inline json_structural_indexer(uint32_t *structural_indexes);
  template<size_t STEP_SIZE>
  simdjson_inline void step(const uint8_t *block, buf_block_reader<STEP_SIZE> &reader) noexcept;
  simdjson_inline void next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx);
  simdjson_inline error_code finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial);

  json_scanner scanner{};
  utf8_checker checker{};
  bit_indexer indexer;
  uint64_t prev_structurals = 0;
  uint64_t unescaped_chars_error = 0;
};

simdjson_inline json_structural_indexer::json_structural_indexer(uint32_t *structural_indexes) : indexer{structural_indexes} {}

// Skip the last character if it is partial
simdjson_inline size_t trim_partial_utf8(const uint8_t *buf, size_t len) {
  if (simdjson_unlikely(len < 3)) {
    switch (len) {
      case 2:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 2 bytes left
        return len;
      case 1:
        if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
        return len;
      case 0:
        return len;
    }
  }
  if (buf[len-1] >= 0xc0) { return len-1; } // 2-, 3- and 4-byte characters with only 1 byte left
  if (buf[len-2] >= 0xe0) { return len-2; } // 3- and 4-byte characters with only 1 byte left
  if (buf[len-3] >= 0xf0) { return len-3; } // 4-byte characters with only 3 bytes left
  return len;
}

//
// PERF NOTES:
// We pipe 2 inputs through these stages:
// 1. Load JSON into registers. This takes a long time and is highly parallelizable, so we load
//    2 inputs' worth at once so that by the time step 2 is looking for them input, it's available.
// 2. Scan the JSON for critical data: strings, scalars and operators. This is the critical path.
//    The output of step 1 depends entirely on this information. These functions don't quite use
//    up enough CPU: the second half of the functions is highly serial, only using 1 execution core
//    at a time. The second input's scans has some dependency on the first ones finishing it, but
//    they can make a lot of progress before they need that information.
// 3. Step 1 doesn't use enough capacity, so we run some extra stuff while we're waiting for that
//    to finish: utf-8 checks and generating the output from the last iteration.
//
// The reason we run 2 inputs at a time, is steps 2 and 3 are *still* not enough to soak up all
// available capacity with just one input. Running 2 at a time seems to give the CPU a good enough
// workout.
//
template<size_t STEP_SIZE>
error_code json_structural_indexer::index(const uint8_t *buf, size_t len, dom_parser_implementation &parser, stage1_mode partial) noexcept {
  if (simdjson_unlikely(len > parser.capacity())) { return CAPACITY; }
  // We guard the rest of the code so that we can assume that len > 0 throughout.
  if (len == 0) { return EMPTY; }
  if (is_streaming(partial)) {
    len = trim_partial_utf8(buf, len);
    // If you end up with an empty window after trimming
    // the partial UTF-8 bytes, then chances are good that you
    // have an UTF-8 formatting error.
    if(len == 0) { return UTF8_ERROR; }
  }
  buf_block_reader<STEP_SIZE> reader(buf, len);
  json_structural_indexer indexer(parser.structural_indexes.get());

  // Read all but the last block
  while (reader.has_full_block()) {
    indexer.step<STEP_SIZE>(reader.full_block(), reader);
  }
  // Take care of the last block (will always be there unless file is empty which is
  // not supposed to happen.)
  uint8_t block[STEP_SIZE];
  if (simdjson_unlikely(reader.get_remainder(block) == 0)) { return UNEXPECTED_ERROR; }
  indexer.step<STEP_SIZE>(block, reader);
  return indexer.finish(parser, reader.block_index(), len, partial);
}

template<>
simdjson_inline void json_structural_indexer::step<128>(const uint8_t *block, buf_block_reader<128> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  simd::simd8x64<uint8_t> in_2(block+64);
  json_block block_1 = scanner.next(in_1);
  json_block block_2 = scanner.next(in_2);
  this->next(in_1, block_1, reader.block_index());
  this->next(in_2, block_2, reader.block_index()+64);
  reader.advance();
}

template<>
simdjson_inline void json_structural_indexer::step<64>(const uint8_t *block, buf_block_reader<64> &reader) noexcept {
  simd::simd8x64<uint8_t> in_1(block);
  json_block block_1 = scanner.next(in_1);
  this->next(in_1, block_1, reader.block_index());
  reader.advance();
}

simdjson_inline void json_structural_indexer::next(const simd::simd8x64<uint8_t>& in, const json_block& block, size_t idx) {
  uint64_t unescaped = in.lteq(0x1F);
#if SIMDJSON_UTF8VALIDATION
  checker.check_next_input(in);
#endif
  indexer.write(uint32_t(idx-64), prev_structurals); // Output *last* iteration's structurals to the parser
  prev_structurals = block.structural_start();
  unescaped_chars_error |= block.non_quote_inside_string(unescaped);
}

simdjson_inline error_code json_structural_indexer::finish(dom_parser_implementation &parser, size_t idx, size_t len, stage1_mode partial) {
  // Write out the final iteration's structurals
  indexer.write(uint32_t(idx-64), prev_structurals);
  error_code error = scanner.finish();
  // We deliberately break down the next expression so that it is
  // human readable.
  const bool should_we_exit = is_streaming(partial) ?
    ((error != SUCCESS) && (error != UNCLOSED_STRING)) // when partial we tolerate UNCLOSED_STRING
    : (error != SUCCESS); // if partial is false, we must have SUCCESS
  const bool have_unclosed_string = (error == UNCLOSED_STRING);
  if (simdjson_unlikely(should_we_exit)) { return error; }

  if (unescaped_chars_error) {
    return UNESCAPED_CHARS;
  }
  parser.n_structural_indexes = uint32_t(indexer.tail - parser.structural_indexes.get());
  /***
   * The On Demand API requires special padding.
   *
   * This is related to https://github.com/simdjson/simdjson/issues/906
   * Basically, we want to make sure that if the parsing continues beyond the last (valid)
   * structural character, it quickly stops.
   * Only three structural characters can be repeated without triggering an error in JSON:  [,] and }.
   * We repeat the padding character (at 'len'). We don't know what it is, but if the parsing
   * continues, then it must be [,] or }.
   * Suppose it is ] or }. We backtrack to the first character, what could it be that would
   * not trigger an error? It could be ] or } but no, because you can't start a document that way.
   * It can't be a comma, a colon or any simple value. So the only way we could continue is
   * if the repeated character is [. But if so, the document must start with [. But if the document
   * starts with [, it should end with ]. If we enforce that rule, then we would get
   * ][[ which is invalid.
   *
   * This is illustrated with the test array_iterate_unclosed_error() on the following input:
   * R"({ "a": [,,)"
   **/
  parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len); // used later in partial == stage1_mode::streaming_final
  parser.structural_indexes[parser.n_structural_indexes + 1] = uint32_t(len);
  parser.structural_indexes[parser.n_structural_indexes + 2] = 0;
  parser.next_structural_index = 0;
  // a valid JSON file cannot have zero structural indexes - we should have found something
  if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
    return EMPTY;
  }
  if (simdjson_unlikely(parser.structural_indexes[parser.n_structural_indexes - 1] > len)) {
    return UNEXPECTED_ERROR;
  }
  if (partial == stage1_mode::streaming_partial) {
    // If we have an unclosed string, then the last structural
    // will be the quote and we want to make sure to omit it.
    if(have_unclosed_string) {
      parser.n_structural_indexes--;
      // a valid JSON file cannot have zero structural indexes - we should have found something
      if (simdjson_unlikely(parser.n_structural_indexes == 0u)) { return CAPACITY; }
    }
    // We truncate the input to the end of the last complete document (or zero).
    auto new_structural_indexes = find_next_document_index(parser);
    if (new_structural_indexes == 0 && parser.n_structural_indexes > 0) {
      if(parser.structural_indexes[0] == 0) {
        // If the buffer is partial and we started at index 0 but the document is
        // incomplete, it's too big to parse.
        return CAPACITY;
      } else {
        // It is possible that the document could be parsed, we just had a lot
        // of white space.
        parser.n_structural_indexes = 0;
        return EMPTY;
      }
    }

    parser.n_structural_indexes = new_structural_indexes;
  } else if (partial == stage1_mode::streaming_final) {
    if(have_unclosed_string) { parser.n_structural_indexes--; }
    // We truncate the input to the end of the last complete document (or zero).
    // Because partial == stage1_mode::streaming_final, it means that we may
    // silently ignore trailing garbage. Though it sounds bad, we do it
    // deliberately because many people who have streams of JSON documents
    // will truncate them for processing. E.g., imagine that you are uncompressing
    // the data from a size file or receiving it in chunks from the network. You
    // may not know where exactly the last document will be. Meanwhile the
    // document_stream instances allow people to know the JSON documents they are
    // parsing (see the iterator.source() method).
    parser.n_structural_indexes = find_next_document_index(parser);
    // We store the initial n_structural_indexes so that the client can see
    // whether we used truncation. If initial_n_structural_indexes == parser.n_structural_indexes,
    // then this will query parser.structural_indexes[parser.n_structural_indexes] which is len,
    // otherwise, it will copy some prior index.
    parser.structural_indexes[parser.n_structural_indexes + 1] = parser.structural_indexes[parser.n_structural_indexes];
    // This next line is critical, do not change it unless you understand what you are
    // doing.
    parser.structural_indexes[parser.n_structural_indexes] = uint32_t(len);
    if (simdjson_unlikely(parser.n_structural_indexes == 0u)) {
        // We tolerate an unclosed string at the very end of the stream. Indeed, users
        // often load their data in bulk without being careful and they want us to ignore
        // the trailing garbage.
        return EMPTY;
    }
  }
  checker.check_eof();
  return checker.errors();
}

} // namespace stage1
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/json_structural_indexer.h */
/* begin file src/generic/stage1/utf8_validator.h */
namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

/**
 * Validates that the string is actual UTF-8.
 */
template<class checker>
bool generic_validate_utf8(const uint8_t * input, size_t length) {
    checker c{};
    buf_block_reader<64> reader(input, length);
    while (reader.has_full_block()) {
      simd::simd8x64<uint8_t> in(reader.full_block());
      c.check_next_input(in);
      reader.advance();
    }
    uint8_t block[64]{};
    reader.get_remainder(block);
    simd::simd8x64<uint8_t> in(block);
    c.check_next_input(in);
    reader.advance();
    c.check_eof();
    return c.errors() == error_code::SUCCESS;
}

bool generic_validate_utf8(const char * input, size_t length) {
    return generic_validate_utf8<utf8_checker>(reinterpret_cast<const uint8_t *>(input),length);
}

} // namespace stage1
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage1/utf8_validator.h */

//
// Stage 2
//
/* begin file src/generic/stage2/stringparsing.h */
// This file contains the common code every implementation uses
// It is intended to be included multiple times and compiled multiple times

namespace simdjson {
namespace westmere {
namespace {
/// @private
namespace stringparsing {

// begin copypasta
// These chars yield themselves: " \ /
// b -> backspace, f -> formfeed, n -> newline, r -> cr, t -> horizontal tab
// u not handled in this table as it's complex
static const uint8_t escape_map[256] = {
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x0.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0x22, 0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0x2f,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x4.
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0x5c, 0, 0,    0, // 0x5.
    0, 0, 0x08, 0, 0,    0, 0x0c, 0, 0, 0, 0, 0, 0,    0, 0x0a, 0, // 0x6.
    0, 0, 0x0d, 0, 0x09, 0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0, // 0x7.

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,

    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
    0, 0, 0,    0, 0,    0, 0,    0, 0, 0, 0, 0, 0,    0, 0,    0,
};

// handle a unicode codepoint
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr, bool allow_replacement) {
  // Use the default Unicode Character 'REPLACEMENT CHARACTER' (U+FFFD)
  constexpr uint32_t substitution_code_point = 0xfffd;
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;

  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) != ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
    } else {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);

      // We have already checked that the high surrogate is valid and
      // (code_point - 0xd800) < 1024.
      //
      // Check that code_point_2 is in the range 0xdc00..0xdfff
      // and that code_point_2 was parsed from valid hex.
      uint32_t low_bit = code_point_2 - 0xdc00;
      if (low_bit >> 10) {
        if(!allow_replacement) { return false; }
        code_point = substitution_code_point;
      } else {
        code_point =  (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }

    }
  } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
      // If we encounter a low surrogate (not preceded by a high surrogate)
      // then we have an error.
      if(!allow_replacement) { return false; }
      code_point = substitution_code_point;
  }
  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


// handle a unicode codepoint using the wobbly convention
// https://simonsapin.github.io/wtf-8/
// write appropriate values into dest
// src will advance 6 bytes or 12 bytes
// dest will advance a variable amount (return via pointer)
// return true if the unicode codepoint was valid
// We work in little-endian then swap at write time
simdjson_warn_unused
simdjson_inline bool handle_unicode_codepoint_wobbly(const uint8_t **src_ptr,
                                            uint8_t **dst_ptr) {
  // It is not ideal that this function is nearly identical to handle_unicode_codepoint.
  //
  // jsoncharutils::hex_to_u32_nocheck fills high 16 bits of the return value with 1s if the
  // conversion isn't valid; we defer the check for this to inside the
  // multilingual plane check
  uint32_t code_point = jsoncharutils::hex_to_u32_nocheck(*src_ptr + 2);
  *src_ptr += 6;
  // If we found a high surrogate, we must
  // check for low surrogate for characters
  // outside the Basic
  // Multilingual Plane.
  if (code_point >= 0xd800 && code_point < 0xdc00) {
    const uint8_t *src_data = *src_ptr;
    /* Compiler optimizations convert this to a single 16-bit load and compare on most platforms */
    if (((src_data[0] << 8) | src_data[1]) == ((static_cast<uint8_t> ('\\') << 8) | static_cast<uint8_t> ('u'))) {
      uint32_t code_point_2 = jsoncharutils::hex_to_u32_nocheck(src_data + 2);
      uint32_t low_bit = code_point_2 - 0xdc00;
      if ((low_bit >> 10) ==  0) {
        code_point =
          (((code_point - 0xd800) << 10) | low_bit) + 0x10000;
        *src_ptr += 6;
      }
    }
  }

  size_t offset = jsoncharutils::codepoint_to_utf8(code_point, *dst_ptr);
  *dst_ptr += offset;
  return offset > 0;
}


/**
 * Unescape a valid UTF-8 string from src to dst, stopping at a final unescaped quote. There
 * must be an unescaped quote terminating the string. It returns the final output
 * position as pointer. In case of error (e.g., the string has bad escaped codes),
 * then null_nullptrptr is returned. It is assumed that the output buffer is large
 * enough. E.g., if src points at 'joe"', then dst needs to have four free bytes +
 * SIMDJSON_PADDING bytes.
 */
simdjson_warn_unused simdjson_inline uint8_t *parse_string(const uint8_t *src, uint8_t *dst, bool allow_replacement) {
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint(&src, &dst, allow_replacement)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

simdjson_warn_unused simdjson_inline uint8_t *parse_wobbly_string(const uint8_t *src, uint8_t *dst) {
  // It is not ideal that this function is nearly identical to parse_string.
  while (1) {
    // Copy the next n bytes, and find the backslash and quote in them.
    auto bs_quote = backslash_and_quote::copy_and_find(src, dst);
    // If the next thing is the end quote, copy and return
    if (bs_quote.has_quote_first()) {
      // we encountered quotes first. Move dst to point to quotes and exit
      return dst + bs_quote.quote_index();
    }
    if (bs_quote.has_backslash()) {
      /* find out where the backspace is */
      auto bs_dist = bs_quote.backslash_index();
      uint8_t escape_char = src[bs_dist + 1];
      /* we encountered backslash first. Handle backslash */
      if (escape_char == 'u') {
        /* move src/dst up to the start; they will be further adjusted
           within the unicode codepoint handling code. */
        src += bs_dist;
        dst += bs_dist;
        if (!handle_unicode_codepoint_wobbly(&src, &dst)) {
          return nullptr;
        }
      } else {
        /* simple 1:1 conversion. Will eat bs_dist+2 characters in input and
         * write bs_dist+1 characters to output
         * note this may reach beyond the part of the buffer we've actually
         * seen. I think this is ok */
        uint8_t escape_result = escape_map[escape_char];
        if (escape_result == 0u) {
          return nullptr; /* bogus escape value is an error */
        }
        dst[bs_dist] = escape_result;
        src += bs_dist + 2;
        dst += bs_dist + 1;
      }
    } else {
      /* they are the same. Since they can't co-occur, it means we
       * encountered neither. */
      src += backslash_and_quote::BYTES_PROCESSED;
      dst += backslash_and_quote::BYTES_PROCESSED;
    }
  }
  /* can't be reached */
  return nullptr;
}

} // namespace stringparsing
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage2/stringparsing.h */
/* begin file src/generic/stage2/tape_builder.h */
/* begin file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/logger.h */
// This is for an internal-only stage 2 specific logger.
// Set LOG_ENABLED = true to log what stage 2 is doing!
namespace simdjson {
namespace westmere {
namespace {
namespace logger {

  static constexpr const char * DASHES = "----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------";

#if SIMDJSON_VERBOSE_LOGGING
  static constexpr const bool LOG_ENABLED = true;
#else
  static constexpr const bool LOG_ENABLED = false;
#endif
  static constexpr const int LOG_EVENT_LEN = 20;
  static constexpr const int LOG_BUFFER_LEN = 30;
  static constexpr const int LOG_SMALL_BUFFER_LEN = 10;
  static constexpr const int LOG_INDEX_LEN = 5;

  static int log_depth; // Not threadsafe. Log only.

  // Helper to turn unprintable or newline characters into spaces
  static simdjson_inline char printable_char(char c) {
    if (c >= 0x20) {
      return c;
    } else {
      return ' ';
    }
  }

  // Print the header and set up log_start
  static simdjson_inline void log_start() {
    if (LOG_ENABLED) {
      log_depth = 0;
      printf("\n");
      printf("| %-*s | %-*s | %-*s | %-*s | Detail |\n", LOG_EVENT_LEN, "Event", LOG_BUFFER_LEN, "Buffer", LOG_SMALL_BUFFER_LEN, "Next", 5, "Next#");
      printf("|%.*s|%.*s|%.*s|%.*s|--------|\n", LOG_EVENT_LEN+2, DASHES, LOG_BUFFER_LEN+2, DASHES, LOG_SMALL_BUFFER_LEN+2, DASHES, 5+2, DASHES);
    }
  }

  simdjson_unused static simdjson_inline void log_string(const char *message) {
    if (LOG_ENABLED) {
      printf("%s\n", message);
    }
  }

  // Logs a single line from the stage 2 DOM parser
  template<typename S>
  static simdjson_inline void log_line(S &structurals, const char *title_prefix, const char *title, const char *detail) {
    if (LOG_ENABLED) {
      printf("| %*s%s%-*s ", log_depth*2, "", title_prefix, LOG_EVENT_LEN - log_depth*2 - int(strlen(title_prefix)), title);
      auto current_index = structurals.at_beginning() ? nullptr : structurals.next_structural-1;
      auto next_index = structurals.next_structural;
      auto current = current_index ? &structurals.buf[*current_index] : reinterpret_cast<const uint8_t*>("                                                       ");
      auto next = &structurals.buf[*next_index];
      {
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_BUFFER_LEN;i++) {
          printf("%c", printable_char(current[i]));
        }
        printf(" ");
        // Print the next N characters in the buffer.
        printf("| ");
        // Otherwise, print the characters starting from the buffer position.
        // Print spaces for unprintable or newline characters.
        for (int i=0;i<LOG_SMALL_BUFFER_LEN;i++) {
          printf("%c", printable_char(next[i]));
        }
        printf(" ");
      }
      if (current_index) {
        printf("| %*u ", LOG_INDEX_LEN, *current_index);
      } else {
        printf("| %-*s ", LOG_INDEX_LEN, "");
      }
      // printf("| %*u ", LOG_INDEX_LEN, structurals.next_tape_index());
      printf("| %-s ", detail);
      printf("|\n");
    }
  }

} // namespace logger
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage2/logger.h */

namespace simdjson {
namespace westmere {
namespace {
namespace stage2 {

class json_iterator {
public:
  const uint8_t* const buf;
  uint32_t *next_structural;
  dom_parser_implementation &dom_parser;
  uint32_t depth{0};

  /**
   * Walk the JSON document.
   *
   * The visitor receives callbacks when values are encountered. All callbacks pass the iterator as
   * the first parameter; some callbacks have other parameters as well:
   *
   * - visit_document_start() - at the beginning.
   * - visit_document_end() - at the end (if things were successful).
   *
   * - visit_array_start() - at the start `[` of a non-empty array.
   * - visit_array_end() - at the end `]` of a non-empty array.
   * - visit_empty_array() - when an empty array is encountered.
   *
   * - visit_object_end() - at the start `]` of a non-empty object.
   * - visit_object_start() - at the end `]` of a non-empty object.
   * - visit_empty_object() - when an empty object is encountered.
   * - visit_key(const uint8_t *key) - when a key in an object field is encountered. key is
   *                                   guaranteed to point at the first quote of the string (`"key"`).
   * - visit_primitive(const uint8_t *value) - when a value is a string, number, boolean or null.
   * - visit_root_primitive(iter, uint8_t *value) - when the top-level value is a string, number, boolean or null.
   *
   * - increment_count(iter) - each time a value is found in an array or object.
   */
  template<bool STREAMING, typename V>
  simdjson_warn_unused simdjson_inline error_code walk_document(V &visitor) noexcept;

  /**
   * Create an iterator capable of walking a JSON document.
   *
   * The document must have already passed through stage 1.
   */
  simdjson_inline json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index);

  /**
   * Look at the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *peek() const noexcept;
  /**
   * Advance to the next token.
   *
   * Tokens can be strings, numbers, booleans, null, or operators (`[{]},:`)).
   *
   * They may include invalid JSON as well (such as `1.2.3` or `ture`).
   */
  simdjson_inline const uint8_t *advance() noexcept;
  /**
   * Get the remaining length of the document, from the start of the current token.
   */
  simdjson_inline size_t remaining_len() const noexcept;
  /**
   * Check if we are at the end of the document.
   *
   * If this is true, there are no more tokens.
   */
  simdjson_inline bool at_eof() const noexcept;
  /**
   * Check if we are at the beginning of the document.
   */
  simdjson_inline bool at_beginning() const noexcept;
  simdjson_inline uint8_t last_structural() const noexcept;

  /**
   * Log that a value has been found.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_value(const char *type) const noexcept;
  /**
   * Log the start of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_start_value(const char *type) const noexcept;
  /**
   * Log the end of a multipart value.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_end_value(const char *type) const noexcept;
  /**
   * Log an error.
   *
   * Set LOG_ENABLED=true in logger.h to see logging.
   */
  simdjson_inline void log_error(const char *error) const noexcept;

  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(V &visitor, const uint8_t *value) noexcept;
  template<typename V>
  simdjson_warn_unused simdjson_inline error_code visit_primitive(V &visitor, const uint8_t *value) noexcept;
};

template<bool STREAMING, typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::walk_document(V &visitor) noexcept {
  logger::log_start();

  //
  // Start the document
  //
  if (at_eof()) { return EMPTY; }
  log_start_value("document");
  SIMDJSON_TRY( visitor.visit_document_start(*this) );

  //
  // Read first value
  //
  {
    auto value = advance();

    // Make sure the outer object or array is closed before continuing; otherwise, there are ways we
    // could get into memory corruption. See https://github.com/simdjson/simdjson/issues/906
    if (!STREAMING) {
      switch (*value) {
        case '{': if (last_structural() != '}') { log_value("starting brace unmatched"); return TAPE_ERROR; }; break;
        case '[': if (last_structural() != ']') { log_value("starting bracket unmatched"); return TAPE_ERROR; }; break;
      }
    }

    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_root_primitive(*this, value) ); break;
    }
  }
  goto document_end;

//
// Object parser states
//
object_begin:
  log_start_value("object");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = false;
  SIMDJSON_TRY( visitor.visit_object_start(*this) );

  {
    auto key = advance();
    if (*key != '"') { log_error("Object does not start with a key"); return TAPE_ERROR; }
    SIMDJSON_TRY( visitor.increment_count(*this) );
    SIMDJSON_TRY( visitor.visit_key(*this, key) );
  }

object_field:
  if (simdjson_unlikely( *advance() != ':' )) { log_error("Missing colon after key in object"); return TAPE_ERROR; }
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

object_continue:
  switch (*advance()) {
    case ',':
      SIMDJSON_TRY( visitor.increment_count(*this) );
      {
        auto key = advance();
        if (simdjson_unlikely( *key != '"' )) { log_error("Key string missing at beginning of field in object"); return TAPE_ERROR; }
        SIMDJSON_TRY( visitor.visit_key(*this, key) );
      }
      goto object_field;
    case '}': log_end_value("object"); SIMDJSON_TRY( visitor.visit_object_end(*this) ); goto scope_end;
    default: log_error("No comma between object fields"); return TAPE_ERROR;
  }

scope_end:
  depth--;
  if (depth == 0) { goto document_end; }
  if (dom_parser.is_array[depth]) { goto array_continue; }
  goto object_continue;

//
// Array parser states
//
array_begin:
  log_start_value("array");
  depth++;
  if (depth >= dom_parser.max_depth()) { log_error("Exceeded max depth!"); return DEPTH_ERROR; }
  dom_parser.is_array[depth] = true;
  SIMDJSON_TRY( visitor.visit_array_start(*this) );
  SIMDJSON_TRY( visitor.increment_count(*this) );

array_value:
  {
    auto value = advance();
    switch (*value) {
      case '{': if (*peek() == '}') { advance(); log_value("empty object"); SIMDJSON_TRY( visitor.visit_empty_object(*this) ); break; } goto object_begin;
      case '[': if (*peek() == ']') { advance(); log_value("empty array"); SIMDJSON_TRY( visitor.visit_empty_array(*this) ); break; } goto array_begin;
      default: SIMDJSON_TRY( visitor.visit_primitive(*this, value) ); break;
    }
  }

array_continue:
  switch (*advance()) {
    case ',': SIMDJSON_TRY( visitor.increment_count(*this) ); goto array_value;
    case ']': log_end_value("array"); SIMDJSON_TRY( visitor.visit_array_end(*this) ); goto scope_end;
    default: log_error("Missing comma between array values"); return TAPE_ERROR;
  }

document_end:
  log_end_value("document");
  SIMDJSON_TRY( visitor.visit_document_end(*this) );

  dom_parser.next_structural_index = uint32_t(next_structural - &dom_parser.structural_indexes[0]);

  // If we didn't make it to the end, it's an error
  if ( !STREAMING && dom_parser.next_structural_index != dom_parser.n_structural_indexes ) {
    log_error("More than one JSON value at the root of the document, or extra characters at the end of the JSON!");
    return TAPE_ERROR;
  }

  return SUCCESS;

} // walk_document()

simdjson_inline json_iterator::json_iterator(dom_parser_implementation &_dom_parser, size_t start_structural_index)
  : buf{_dom_parser.buf},
    next_structural{&_dom_parser.structural_indexes[start_structural_index]},
    dom_parser{_dom_parser} {
}

simdjson_inline const uint8_t *json_iterator::peek() const noexcept {
  return &buf[*(next_structural)];
}
simdjson_inline const uint8_t *json_iterator::advance() noexcept {
  return &buf[*(next_structural++)];
}
simdjson_inline size_t json_iterator::remaining_len() const noexcept {
  return dom_parser.len - *(next_structural-1);
}

simdjson_inline bool json_iterator::at_eof() const noexcept {
  return next_structural == &dom_parser.structural_indexes[dom_parser.n_structural_indexes];
}
simdjson_inline bool json_iterator::at_beginning() const noexcept {
  return next_structural == dom_parser.structural_indexes.get();
}
simdjson_inline uint8_t json_iterator::last_structural() const noexcept {
  return buf[dom_parser.structural_indexes[dom_parser.n_structural_indexes - 1]];
}

simdjson_inline void json_iterator::log_value(const char *type) const noexcept {
  logger::log_line(*this, "", type, "");
}

simdjson_inline void json_iterator::log_start_value(const char *type) const noexcept {
  logger::log_line(*this, "+", type, "");
  if (logger::LOG_ENABLED) { logger::log_depth++; }
}

simdjson_inline void json_iterator::log_end_value(const char *type) const noexcept {
  if (logger::LOG_ENABLED) { logger::log_depth--; }
  logger::log_line(*this, "-", type, "");
}

simdjson_inline void json_iterator::log_error(const char *error) const noexcept {
  logger::log_line(*this, "", "ERROR", error);
}

template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_root_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_root_string(*this, value);
    case 't': return visitor.visit_root_true_atom(*this, value);
    case 'f': return visitor.visit_root_false_atom(*this, value);
    case 'n': return visitor.visit_root_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_root_number(*this, value);
    default:
      log_error("Document starts with a non-value character");
      return TAPE_ERROR;
  }
}
template<typename V>
simdjson_warn_unused simdjson_inline error_code json_iterator::visit_primitive(V &visitor, const uint8_t *value) noexcept {
  switch (*value) {
    case '"': return visitor.visit_string(*this, value);
    case 't': return visitor.visit_true_atom(*this, value);
    case 'f': return visitor.visit_false_atom(*this, value);
    case 'n': return visitor.visit_null_atom(*this, value);
    case '-':
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return visitor.visit_number(*this, value);
    default:
      log_error("Non-value found when value was expected!");
      return TAPE_ERROR;
  }
}

} // namespace stage2
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage2/json_iterator.h */
/* begin file src/generic/stage2/tape_writer.h */
namespace simdjson {
namespace westmere {
namespace {
namespace stage2 {

struct tape_writer {
  /** The next place to write to tape */
  uint64_t *next_tape_loc;

  /** Write a signed 64-bit value to tape. */
  simdjson_inline void append_s64(int64_t value) noexcept;

  /** Write an unsigned 64-bit value to tape. */
  simdjson_inline void append_u64(uint64_t value) noexcept;

  /** Write a double value to tape. */
  simdjson_inline void append_double(double value) noexcept;

  /**
   * Append a tape entry (an 8-bit type,and 56 bits worth of value).
   */
  simdjson_inline void append(uint64_t val, internal::tape_type t) noexcept;

  /**
   * Skip the current tape entry without writing.
   *
   * Used to skip the start of the container, since we'll come back later to fill it in when the
   * container ends.
   */
  simdjson_inline void skip() noexcept;

  /**
   * Skip the number of tape entries necessary to write a large u64 or i64.
   */
  simdjson_inline void skip_large_integer() noexcept;

  /**
   * Skip the number of tape entries necessary to write a double.
   */
  simdjson_inline void skip_double() noexcept;

  /**
   * Write a value to a known location on tape.
   *
   * Used to go back and write out the start of a container after the container ends.
   */
  simdjson_inline static void write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept;

private:
  /**
   * Append both the tape entry, and a supplementary value following it. Used for types that need
   * all 64 bits, such as double and uint64_t.
   */
  template<typename T>
  simdjson_inline void append2(uint64_t val, T val2, internal::tape_type t) noexcept;
}; // struct number_writer

simdjson_inline void tape_writer::append_s64(int64_t value) noexcept {
  append2(0, value, internal::tape_type::INT64);
}

simdjson_inline void tape_writer::append_u64(uint64_t value) noexcept {
  append(0, internal::tape_type::UINT64);
  *next_tape_loc = value;
  next_tape_loc++;
}

/** Write a double value to tape. */
simdjson_inline void tape_writer::append_double(double value) noexcept {
  append2(0, value, internal::tape_type::DOUBLE);
}

simdjson_inline void tape_writer::skip() noexcept {
  next_tape_loc++;
}

simdjson_inline void tape_writer::skip_large_integer() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::skip_double() noexcept {
  next_tape_loc += 2;
}

simdjson_inline void tape_writer::append(uint64_t val, internal::tape_type t) noexcept {
  *next_tape_loc = val | ((uint64_t(char(t))) << 56);
  next_tape_loc++;
}

template<typename T>
simdjson_inline void tape_writer::append2(uint64_t val, T val2, internal::tape_type t) noexcept {
  append(val, t);
  static_assert(sizeof(val2) == sizeof(*next_tape_loc), "Type is not 64 bits!");
  memcpy(next_tape_loc, &val2, sizeof(val2));
  next_tape_loc++;
}

simdjson_inline void tape_writer::write(uint64_t &tape_loc, uint64_t val, internal::tape_type t) noexcept {
  tape_loc = val | ((uint64_t(char(t))) << 56);
}

} // namespace stage2
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage2/tape_writer.h */

namespace simdjson {
namespace westmere {
namespace {
namespace stage2 {

struct tape_builder {
  template<bool STREAMING>
  simdjson_warn_unused static simdjson_inline error_code parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept;

  /** Called when a non-empty document starts. */
  simdjson_warn_unused simdjson_inline error_code visit_document_start(json_iterator &iter) noexcept;
  /** Called when a non-empty document ends without error. */
  simdjson_warn_unused simdjson_inline error_code visit_document_end(json_iterator &iter) noexcept;

  /** Called when a non-empty array starts. */
  simdjson_warn_unused simdjson_inline error_code visit_array_start(json_iterator &iter) noexcept;
  /** Called when a non-empty array ends. */
  simdjson_warn_unused simdjson_inline error_code visit_array_end(json_iterator &iter) noexcept;
  /** Called when an empty array is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_array(json_iterator &iter) noexcept;

  /** Called when a non-empty object starts. */
  simdjson_warn_unused simdjson_inline error_code visit_object_start(json_iterator &iter) noexcept;
  /**
   * Called when a key in a field is encountered.
   *
   * primitive, visit_object_start, visit_empty_object, visit_array_start, or visit_empty_array
   * will be called after this with the field value.
   */
  simdjson_warn_unused simdjson_inline error_code visit_key(json_iterator &iter, const uint8_t *key) noexcept;
  /** Called when a non-empty object ends. */
  simdjson_warn_unused simdjson_inline error_code visit_object_end(json_iterator &iter) noexcept;
  /** Called when an empty object is found. */
  simdjson_warn_unused simdjson_inline error_code visit_empty_object(json_iterator &iter) noexcept;

  /**
   * Called when a string, number, boolean or null is found.
   */
  simdjson_warn_unused simdjson_inline error_code visit_primitive(json_iterator &iter, const uint8_t *value) noexcept;
  /**
   * Called when a string, number, boolean or null is found at the top level of a document (i.e.
   * when there is no array or object and the entire document is a single string, number, boolean or
   * null.
   *
   * This is separate from primitive() because simdjson's normal primitive parsing routines assume
   * there is at least one more token after the value, which is only true in an array or object.
   */
  simdjson_warn_unused simdjson_inline error_code visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_string(json_iterator &iter, const uint8_t *value, bool key = false) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  simdjson_warn_unused simdjson_inline error_code visit_root_string(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_number(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept;
  simdjson_warn_unused simdjson_inline error_code visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept;

  /** Called each time a new field or element in an array or object is found. */
  simdjson_warn_unused simdjson_inline error_code increment_count(json_iterator &iter) noexcept;

  /** Next location to write to tape */
  tape_writer tape;
private:
  /** Next write location in the string buf for stage 2 parsing */
  uint8_t *current_string_buf_loc;

  simdjson_inline tape_builder(dom::document &doc) noexcept;

  simdjson_inline uint32_t next_tape_index(json_iterator &iter) const noexcept;
  simdjson_inline void start_container(json_iterator &iter) noexcept;
  simdjson_warn_unused simdjson_inline error_code end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_warn_unused simdjson_inline error_code empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept;
  simdjson_inline uint8_t *on_start_string(json_iterator &iter) noexcept;
  simdjson_inline void on_end_string(uint8_t *dst) noexcept;
}; // class tape_builder

template<bool STREAMING>
simdjson_warn_unused simdjson_inline error_code tape_builder::parse_document(
    dom_parser_implementation &dom_parser,
    dom::document &doc) noexcept {
  dom_parser.doc = &doc;
  json_iterator iter(dom_parser, STREAMING ? dom_parser.next_structural_index : 0);
  tape_builder builder(doc);
  return iter.walk_document<STREAMING>(builder);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_root_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_primitive(json_iterator &iter, const uint8_t *value) noexcept {
  return iter.visit_primitive(*this, value);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_object(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_empty_array(json_iterator &iter) noexcept {
  return empty_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_start(json_iterator &iter) noexcept {
  start_container(iter);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_object_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_OBJECT, internal::tape_type::END_OBJECT);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_array_end(json_iterator &iter) noexcept {
  return end_container(iter, internal::tape_type::START_ARRAY, internal::tape_type::END_ARRAY);
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_document_end(json_iterator &iter) noexcept {
  constexpr uint32_t start_tape_index = 0;
  tape.append(start_tape_index, internal::tape_type::ROOT);
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter), internal::tape_type::ROOT);
  return SUCCESS;
}
simdjson_warn_unused simdjson_inline error_code tape_builder::visit_key(json_iterator &iter, const uint8_t *key) noexcept {
  return visit_string(iter, key, true);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::increment_count(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].count++; // we have a key value pair in the object at parser.dom_parser.depth - 1
  return SUCCESS;
}

simdjson_inline tape_builder::tape_builder(dom::document &doc) noexcept : tape{doc.tape.get()}, current_string_buf_loc{doc.string_buf.get()} {}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_string(json_iterator &iter, const uint8_t *value, bool key) noexcept {
  iter.log_value(key ? "key" : "string");
  uint8_t *dst = on_start_string(iter);
  dst = stringparsing::parse_string(value+1, dst, false); // We do not allow replacement when the escape characters are invalid.
  if (dst == nullptr) {
    iter.log_error("Invalid escape in string");
    return STRING_ERROR;
  }
  on_end_string(dst);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_string(json_iterator &iter, const uint8_t *value) noexcept {
  return visit_string(iter, value);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_number(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("number");
  return numberparsing::parse_number(value, tape);
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_number(json_iterator &iter, const uint8_t *value) noexcept {
  //
  // We need to make a copy to make sure that the string is space terminated.
  // This is not about padding the input, which should already padded up
  // to len + SIMDJSON_PADDING. However, we have no control at this stage
  // on how the padding was done. What if the input string was padded with nulls?
  // It is quite common for an input string to have an extra null character (C string).
  // We do not want to allow 9\0 (where \0 is the null character) inside a JSON
  // document, but the string "9\0" by itself is fine. So we make a copy and
  // pad the input with spaces when we know that there is just one input element.
  // This copy is relatively expensive, but it will almost never be called in
  // practice unless you are in the strange scenario where you have many JSON
  // documents made of single atoms.
  //
  std::unique_ptr<uint8_t[]>copy(new (std::nothrow) uint8_t[iter.remaining_len() + SIMDJSON_PADDING]);
  if (copy.get() == nullptr) { return MEMALLOC; }
  std::memcpy(copy.get(), value, iter.remaining_len());
  std::memset(copy.get() + iter.remaining_len(), ' ', SIMDJSON_PADDING);
  error_code error = visit_number(iter, copy.get());
  return error;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value)) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_true_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("true");
  if (!atomparsing::is_valid_true_atom(value, iter.remaining_len())) { return T_ATOM_ERROR; }
  tape.append(0, internal::tape_type::TRUE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value)) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_false_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("false");
  if (!atomparsing::is_valid_false_atom(value, iter.remaining_len())) { return F_ATOM_ERROR; }
  tape.append(0, internal::tape_type::FALSE_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value)) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

simdjson_warn_unused simdjson_inline error_code tape_builder::visit_root_null_atom(json_iterator &iter, const uint8_t *value) noexcept {
  iter.log_value("null");
  if (!atomparsing::is_valid_null_atom(value, iter.remaining_len())) { return N_ATOM_ERROR; }
  tape.append(0, internal::tape_type::NULL_VALUE);
  return SUCCESS;
}

// private:

simdjson_inline uint32_t tape_builder::next_tape_index(json_iterator &iter) const noexcept {
  return uint32_t(tape.next_tape_loc - iter.dom_parser.doc->tape.get());
}

simdjson_warn_unused simdjson_inline error_code tape_builder::empty_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  auto start_index = next_tape_index(iter);
  tape.append(start_index+2, start);
  tape.append(start_index, end);
  return SUCCESS;
}

simdjson_inline void tape_builder::start_container(json_iterator &iter) noexcept {
  iter.dom_parser.open_containers[iter.depth].tape_index = next_tape_index(iter);
  iter.dom_parser.open_containers[iter.depth].count = 0;
  tape.skip(); // We don't actually *write* the start element until the end.
}

simdjson_warn_unused simdjson_inline error_code tape_builder::end_container(json_iterator &iter, internal::tape_type start, internal::tape_type end) noexcept {
  // Write the ending tape element, pointing at the start location
  const uint32_t start_tape_index = iter.dom_parser.open_containers[iter.depth].tape_index;
  tape.append(start_tape_index, end);
  // Write the start tape element, pointing at the end location (and including count)
  // count can overflow if it exceeds 24 bits... so we saturate
  // the convention being that a cnt of 0xffffff or more is undetermined in value (>=  0xffffff).
  const uint32_t count = iter.dom_parser.open_containers[iter.depth].count;
  const uint32_t cntsat = count > 0xFFFFFF ? 0xFFFFFF : count;
  tape_writer::write(iter.dom_parser.doc->tape[start_tape_index], next_tape_index(iter) | (uint64_t(cntsat) << 32), start);
  return SUCCESS;
}

simdjson_inline uint8_t *tape_builder::on_start_string(json_iterator &iter) noexcept {
  // we advance the point, accounting for the fact that we have a NULL termination
  tape.append(current_string_buf_loc - iter.dom_parser.doc->string_buf.get(), internal::tape_type::STRING);
  return current_string_buf_loc + sizeof(uint32_t);
}

simdjson_inline void tape_builder::on_end_string(uint8_t *dst) noexcept {
  uint32_t str_length = uint32_t(dst - (current_string_buf_loc + sizeof(uint32_t)));
  // TODO check for overflow in case someone has a crazy string (>=4GB?)
  // But only add the overflow check when the document itself exceeds 4GB
  // Currently unneeded because we refuse to parse docs larger or equal to 4GB.
  memcpy(current_string_buf_loc, &str_length, sizeof(uint32_t));
  // NULL termination is still handy if you expect all your strings to
  // be NULL terminated? It comes at a small cost
  *dst = 0;
  current_string_buf_loc = dst + 1;
}

} // namespace stage2
} // unnamed namespace
} // namespace westmere
} // namespace simdjson
/* end file src/generic/stage2/tape_builder.h */

//
// Implementation-specific overrides
//

namespace simdjson {
namespace westmere {
namespace {
namespace stage1 {

simdjson_inline uint64_t json_string_scanner::find_escaped(uint64_t backslash) {
  if (!backslash) { uint64_t escaped = prev_escaped; prev_escaped = 0; return escaped; }
  return find_escaped_branchless(backslash);
}

} // namespace stage1
} // unnamed namespace

simdjson_warn_unused error_code implementation::minify(const uint8_t *buf, size_t len, uint8_t *dst, size_t &dst_len) const noexcept {
  return westmere::stage1::json_minifier::minify<64>(buf, len, dst, dst_len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage1(const uint8_t *_buf, size_t _len, stage1_mode streaming) noexcept {
  this->buf = _buf;
  this->len = _len;
  return westmere::stage1::json_structural_indexer::index<64>(_buf, _len, *this, streaming);
}

simdjson_warn_unused bool implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  return westmere::stage1::generic_validate_utf8(buf,len);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<false>(*this, _doc);
}

simdjson_warn_unused error_code dom_parser_implementation::stage2_next(dom::document &_doc) noexcept {
  return stage2::tape_builder::parse_document<true>(*this, _doc);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_string(const uint8_t *src, uint8_t *dst, bool replacement_char) const noexcept {
  return westmere::stringparsing::parse_string(src, dst, replacement_char);
}

simdjson_warn_unused uint8_t *dom_parser_implementation::parse_wobbly_string(const uint8_t *src, uint8_t *dst) const noexcept {
  return westmere::stringparsing::parse_wobbly_string(src, dst);
}

simdjson_warn_unused error_code dom_parser_implementation::parse(const uint8_t *_buf, size_t _len, dom::document &_doc) noexcept {
  auto error = stage1(_buf, _len, stage1_mode::regular);
  if (error) { return error; }
  return stage2(_doc);
}

} // namespace westmere
} // namespace simdjson

/* begin file include/simdjson/westmere/end.h */
SIMDJSON_UNTARGET_WESTMERE
/* end file include/simdjson/westmere/end.h */
/* end file src/westmere/dom_parser_implementation.cpp */
#endif

SIMDJSON_POP_DISABLE_WARNINGS
/* end file src/simdjson.cpp */
