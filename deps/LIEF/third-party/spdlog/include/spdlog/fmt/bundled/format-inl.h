// Formatting library for C++ - implementation
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_FORMAT_INL_H_
#define FMT_FORMAT_INL_H_

#ifndef FMT_MODULE
#  include <algorithm>
#  include <cerrno>  // errno
#  include <climits>
#  include <cmath>
#  include <exception>
#endif

#if defined(_WIN32) && !defined(FMT_USE_WRITE_CONSOLE)
#  include <io.h>  // _isatty
#endif

#include "format.h"

#if FMT_USE_LOCALE
#  include <locale>
#endif

#ifndef FMT_FUNC
#  define FMT_FUNC
#endif

FMT_BEGIN_NAMESPACE
namespace detail {

FMT_FUNC void assert_fail(const char* file, int line, const char* message) {
  // Use unchecked std::fprintf to avoid triggering another assertion when
  // writing to stderr fails.
  fprintf(stderr, "%s:%d: assertion failed: %s", file, line, message);
  abort();
}

FMT_FUNC void format_error_code(detail::buffer<char>& out, int error_code,
                                string_view message) noexcept {
  // Report error code making sure that the output fits into
  // inline_buffer_size to avoid dynamic memory allocation and potential
  // bad_alloc.
  out.try_resize(0);
  static const char SEP[] = ": ";
  static const char ERROR_STR[] = "error ";
  // Subtract 2 to account for terminating null characters in SEP and ERROR_STR.
  size_t error_code_size = sizeof(SEP) + sizeof(ERROR_STR) - 2;
  auto abs_value = static_cast<uint32_or_64_or_128_t<int>>(error_code);
  if (detail::is_negative(error_code)) {
    abs_value = 0 - abs_value;
    ++error_code_size;
  }
  error_code_size += detail::to_unsigned(detail::count_digits(abs_value));
  auto it = appender(out);
  if (message.size() <= inline_buffer_size - error_code_size)
    fmt::format_to(it, FMT_STRING("{}{}"), message, SEP);
  fmt::format_to(it, FMT_STRING("{}{}"), ERROR_STR, error_code);
  FMT_ASSERT(out.size() <= inline_buffer_size, "");
}

FMT_FUNC void do_report_error(format_func func, int error_code,
                              const char* message) noexcept {
  memory_buffer full_message;
  func(full_message, error_code, message);
  // Don't use fwrite_all because the latter may throw.
  if (std::fwrite(full_message.data(), full_message.size(), 1, stderr) > 0)
    std::fputc('\n', stderr);
}

// A wrapper around fwrite that throws on error.
inline void fwrite_all(const void* ptr, size_t count, FILE* stream) {
  size_t written = std::fwrite(ptr, 1, count, stream);
  if (written < count)
    FMT_THROW(system_error(errno, FMT_STRING("cannot write to file")));
}

#if FMT_USE_LOCALE
using std::locale;
using std::numpunct;
using std::use_facet;

template <typename Locale>
locale_ref::locale_ref(const Locale& loc) : locale_(&loc) {
  static_assert(std::is_same<Locale, locale>::value, "");
}
#else
struct locale {};
template <typename Char> struct numpunct {
  auto grouping() const -> std::string { return "\03"; }
  auto thousands_sep() const -> Char { return ','; }
  auto decimal_point() const -> Char { return '.'; }
};
template <typename Facet> Facet use_facet(locale) { return {}; }
#endif  // FMT_USE_LOCALE

template <typename Locale> auto locale_ref::get() const -> Locale {
  static_assert(std::is_same<Locale, locale>::value, "");
#if FMT_USE_LOCALE
  if (locale_) return *static_cast<const locale*>(locale_);
#endif
  return locale();
}

template <typename Char>
FMT_FUNC auto thousands_sep_impl(locale_ref loc) -> thousands_sep_result<Char> {
  auto&& facet = use_facet<numpunct<Char>>(loc.get<locale>());
  auto grouping = facet.grouping();
  auto thousands_sep = grouping.empty() ? Char() : facet.thousands_sep();
  return {std::move(grouping), thousands_sep};
}
template <typename Char>
FMT_FUNC auto decimal_point_impl(locale_ref loc) -> Char {
  return use_facet<numpunct<Char>>(loc.get<locale>()).decimal_point();
}

#if FMT_USE_LOCALE
FMT_FUNC auto write_loc(appender out, loc_value value,
                        const format_specs& specs, locale_ref loc) -> bool {
  auto locale = loc.get<std::locale>();
  // We cannot use the num_put<char> facet because it may produce output in
  // a wrong encoding.
  using facet = format_facet<std::locale>;
  if (std::has_facet<facet>(locale))
    return use_facet<facet>(locale).put(out, value, specs);
  return facet(locale).put(out, value, specs);
}
#endif
}  // namespace detail

FMT_FUNC void report_error(const char* message) {
#if FMT_USE_EXCEPTIONS
  // Use FMT_THROW instead of throw to avoid bogus unreachable code warnings
  // from MSVC.
  FMT_THROW(format_error(message));
#else
  fputs(message, stderr);
  abort();
#endif
}

template <typename Locale> typename Locale::id format_facet<Locale>::id;

template <typename Locale> format_facet<Locale>::format_facet(Locale& loc) {
  auto& np = detail::use_facet<detail::numpunct<char>>(loc);
  grouping_ = np.grouping();
  if (!grouping_.empty()) separator_ = std::string(1, np.thousands_sep());
}

#if FMT_USE_LOCALE
template <>
FMT_API FMT_FUNC auto format_facet<std::locale>::do_put(
    appender out, loc_value val, const format_specs& specs) const -> bool {
  return val.visit(
      detail::loc_writer<>{out, specs, separator_, grouping_, decimal_point_});
}
#endif

FMT_FUNC auto vsystem_error(int error_code, string_view fmt, format_args args)
    -> std::system_error {
  auto ec = std::error_code(error_code, std::generic_category());
  return std::system_error(ec, vformat(fmt, args));
}

namespace detail {

template <typename F>
inline auto operator==(basic_fp<F> x, basic_fp<F> y) -> bool {
  return x.f == y.f && x.e == y.e;
}

// Compilers should be able to optimize this into the ror instruction.
FMT_CONSTEXPR inline auto rotr(uint32_t n, uint32_t r) noexcept -> uint32_t {
  r &= 31;
  return (n >> r) | (n << (32 - r));
}
FMT_CONSTEXPR inline auto rotr(uint64_t n, uint32_t r) noexcept -> uint64_t {
  r &= 63;
  return (n >> r) | (n << (64 - r));
}

// Implementation of Dragonbox algorithm: https://github.com/jk-jeon/dragonbox.
namespace dragonbox {
// Computes upper 64 bits of multiplication of a 32-bit unsigned integer and a
// 64-bit unsigned integer.
inline auto umul96_upper64(uint32_t x, uint64_t y) noexcept -> uint64_t {
  return umul128_upper64(static_cast<uint64_t>(x) << 32, y);
}

// Computes lower 128 bits of multiplication of a 64-bit unsigned integer and a
// 128-bit unsigned integer.
inline auto umul192_lower128(uint64_t x, uint128_fallback y) noexcept
    -> uint128_fallback {
  uint64_t high = x * y.high();
  uint128_fallback high_low = umul128(x, y.low());
  return {high + high_low.high(), high_low.low()};
}

// Computes lower 64 bits of multiplication of a 32-bit unsigned integer and a
// 64-bit unsigned integer.
inline auto umul96_lower64(uint32_t x, uint64_t y) noexcept -> uint64_t {
  return x * y;
}

// Various fast log computations.
inline auto floor_log10_pow2_minus_log10_4_over_3(int e) noexcept -> int {
  FMT_ASSERT(e <= 2936 && e >= -2985, "too large exponent");
  return (e * 631305 - 261663) >> 21;
}

FMT_INLINE_VARIABLE constexpr struct div_small_pow10_infos_struct {
  uint32_t divisor;
  int shift_amount;
} div_small_pow10_infos[] = {{10, 16}, {100, 16}};

// Replaces n by floor(n / pow(10, N)) returning true if and only if n is
// divisible by pow(10, N).
// Precondition: n <= pow(10, N + 1).
template <int N>
auto check_divisibility_and_divide_by_pow10(uint32_t& n) noexcept -> bool {
  // The numbers below are chosen such that:
  //   1. floor(n/d) = floor(nm / 2^k) where d=10 or d=100,
  //   2. nm mod 2^k < m if and only if n is divisible by d,
  // where m is magic_number, k is shift_amount
  // and d is divisor.
  //
  // Item 1 is a common technique of replacing division by a constant with
  // multiplication, see e.g. "Division by Invariant Integers Using
  // Multiplication" by Granlund and Montgomery (1994). magic_number (m) is set
  // to ceil(2^k/d) for large enough k.
  // The idea for item 2 originates from Schubfach.
  constexpr auto info = div_small_pow10_infos[N - 1];
  FMT_ASSERT(n <= info.divisor * 10, "n is too large");
  constexpr uint32_t magic_number =
      (1u << info.shift_amount) / info.divisor + 1;
  n *= magic_number;
  const uint32_t comparison_mask = (1u << info.shift_amount) - 1;
  bool result = (n & comparison_mask) < magic_number;
  n >>= info.shift_amount;
  return result;
}

// Computes floor(n / pow(10, N)) for small n and N.
// Precondition: n <= pow(10, N + 1).
template <int N> auto small_division_by_pow10(uint32_t n) noexcept -> uint32_t {
  constexpr auto info = div_small_pow10_infos[N - 1];
  FMT_ASSERT(n <= info.divisor * 10, "n is too large");
  constexpr uint32_t magic_number =
      (1u << info.shift_amount) / info.divisor + 1;
  return (n * magic_number) >> info.shift_amount;
}

// Computes floor(n / 10^(kappa + 1)) (float)
inline auto divide_by_10_to_kappa_plus_1(uint32_t n) noexcept -> uint32_t {
  // 1374389535 = ceil(2^37/100)
  return static_cast<uint32_t>((static_cast<uint64_t>(n) * 1374389535) >> 37);
}
// Computes floor(n / 10^(kappa + 1)) (double)
inline auto divide_by_10_to_kappa_plus_1(uint64_t n) noexcept -> uint64_t {
  // 2361183241434822607 = ceil(2^(64+7)/1000)
  return umul128_upper64(n, 2361183241434822607ull) >> 7;
}

// Various subroutines using pow10 cache
template <typename T> struct cache_accessor;

template <> struct cache_accessor<float> {
  using carrier_uint = float_info<float>::carrier_uint;
  using cache_entry_type = uint64_t;

  static auto get_cached_power(int k) noexcept -> uint64_t {
    FMT_ASSERT(k >= float_info<float>::min_k && k <= float_info<float>::max_k,
               "k is out of range");
    static constexpr const uint64_t pow10_significands[] = {
        0x81ceb32c4b43fcf5, 0xa2425ff75e14fc32, 0xcad2f7f5359a3b3f,
        0xfd87b5f28300ca0e, 0x9e74d1b791e07e49, 0xc612062576589ddb,
        0xf79687aed3eec552, 0x9abe14cd44753b53, 0xc16d9a0095928a28,
        0xf1c90080baf72cb2, 0x971da05074da7bef, 0xbce5086492111aeb,
        0xec1e4a7db69561a6, 0x9392ee8e921d5d08, 0xb877aa3236a4b44a,
        0xe69594bec44de15c, 0x901d7cf73ab0acda, 0xb424dc35095cd810,
        0xe12e13424bb40e14, 0x8cbccc096f5088cc, 0xafebff0bcb24aaff,
        0xdbe6fecebdedd5bf, 0x89705f4136b4a598, 0xabcc77118461cefd,
        0xd6bf94d5e57a42bd, 0x8637bd05af6c69b6, 0xa7c5ac471b478424,
        0xd1b71758e219652c, 0x83126e978d4fdf3c, 0xa3d70a3d70a3d70b,
        0xcccccccccccccccd, 0x8000000000000000, 0xa000000000000000,
        0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000,
        0xc350000000000000, 0xf424000000000000, 0x9896800000000000,
        0xbebc200000000000, 0xee6b280000000000, 0x9502f90000000000,
        0xba43b74000000000, 0xe8d4a51000000000, 0x9184e72a00000000,
        0xb5e620f480000000, 0xe35fa931a0000000, 0x8e1bc9bf04000000,
        0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000,
        0xad78ebc5ac620000, 0xd8d726b7177a8000, 0x878678326eac9000,
        0xa968163f0a57b400, 0xd3c21bcecceda100, 0x84595161401484a0,
        0xa56fa5b99019a5c8, 0xcecb8f27f4200f3a, 0x813f3978f8940985,
        0xa18f07d736b90be6, 0xc9f2c9cd04674edf, 0xfc6f7c4045812297,
        0x9dc5ada82b70b59e, 0xc5371912364ce306, 0xf684df56c3e01bc7,
        0x9a130b963a6c115d, 0xc097ce7bc90715b4, 0xf0bdc21abb48db21,
        0x96769950b50d88f5, 0xbc143fa4e250eb32, 0xeb194f8e1ae525fe,
        0x92efd1b8d0cf37bf, 0xb7abc627050305ae, 0xe596b7b0c643c71a,
        0x8f7e32ce7bea5c70, 0xb35dbf821ae4f38c, 0xe0352f62a19e306f};
    return pow10_significands[k - float_info<float>::min_k];
  }

  struct compute_mul_result {
    carrier_uint result;
    bool is_integer;
  };
  struct compute_mul_parity_result {
    bool parity;
    bool is_integer;
  };

  static auto compute_mul(carrier_uint u,
                          const cache_entry_type& cache) noexcept
      -> compute_mul_result {
    auto r = umul96_upper64(u, cache);
    return {static_cast<carrier_uint>(r >> 32),
            static_cast<carrier_uint>(r) == 0};
  }

  static auto compute_delta(const cache_entry_type& cache, int beta) noexcept
      -> uint32_t {
    return static_cast<uint32_t>(cache >> (64 - 1 - beta));
  }

  static auto compute_mul_parity(carrier_uint two_f,
                                 const cache_entry_type& cache,
                                 int beta) noexcept
      -> compute_mul_parity_result {
    FMT_ASSERT(beta >= 1, "");
    FMT_ASSERT(beta < 64, "");

    auto r = umul96_lower64(two_f, cache);
    return {((r >> (64 - beta)) & 1) != 0,
            static_cast<uint32_t>(r >> (32 - beta)) == 0};
  }

  static auto compute_left_endpoint_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>(
        (cache - (cache >> (num_significand_bits<float>() + 2))) >>
        (64 - num_significand_bits<float>() - 1 - beta));
  }

  static auto compute_right_endpoint_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return static_cast<carrier_uint>(
        (cache + (cache >> (num_significand_bits<float>() + 1))) >>
        (64 - num_significand_bits<float>() - 1 - beta));
  }

  static auto compute_round_up_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return (static_cast<carrier_uint>(
                cache >> (64 - num_significand_bits<float>() - 2 - beta)) +
            1) /
           2;
  }
};

template <> struct cache_accessor<double> {
  using carrier_uint = float_info<double>::carrier_uint;
  using cache_entry_type = uint128_fallback;

  static auto get_cached_power(int k) noexcept -> uint128_fallback {
    FMT_ASSERT(k >= float_info<double>::min_k && k <= float_info<double>::max_k,
               "k is out of range");

    static constexpr const uint128_fallback pow10_significands[] = {
#if FMT_USE_FULL_CACHE_DRAGONBOX
      {0xff77b1fcbebcdc4f, 0x25e8e89c13bb0f7b},
      {0x9faacf3df73609b1, 0x77b191618c54e9ad},
      {0xc795830d75038c1d, 0xd59df5b9ef6a2418},
      {0xf97ae3d0d2446f25, 0x4b0573286b44ad1e},
      {0x9becce62836ac577, 0x4ee367f9430aec33},
      {0xc2e801fb244576d5, 0x229c41f793cda740},
      {0xf3a20279ed56d48a, 0x6b43527578c11110},
      {0x9845418c345644d6, 0x830a13896b78aaaa},
      {0xbe5691ef416bd60c, 0x23cc986bc656d554},
      {0xedec366b11c6cb8f, 0x2cbfbe86b7ec8aa9},
      {0x94b3a202eb1c3f39, 0x7bf7d71432f3d6aa},
      {0xb9e08a83a5e34f07, 0xdaf5ccd93fb0cc54},
      {0xe858ad248f5c22c9, 0xd1b3400f8f9cff69},
      {0x91376c36d99995be, 0x23100809b9c21fa2},
      {0xb58547448ffffb2d, 0xabd40a0c2832a78b},
      {0xe2e69915b3fff9f9, 0x16c90c8f323f516d},
      {0x8dd01fad907ffc3b, 0xae3da7d97f6792e4},
      {0xb1442798f49ffb4a, 0x99cd11cfdf41779d},
      {0xdd95317f31c7fa1d, 0x40405643d711d584},
      {0x8a7d3eef7f1cfc52, 0x482835ea666b2573},
      {0xad1c8eab5ee43b66, 0xda3243650005eed0},
      {0xd863b256369d4a40, 0x90bed43e40076a83},
      {0x873e4f75e2224e68, 0x5a7744a6e804a292},
      {0xa90de3535aaae202, 0x711515d0a205cb37},
      {0xd3515c2831559a83, 0x0d5a5b44ca873e04},
      {0x8412d9991ed58091, 0xe858790afe9486c3},
      {0xa5178fff668ae0b6, 0x626e974dbe39a873},
      {0xce5d73ff402d98e3, 0xfb0a3d212dc81290},
      {0x80fa687f881c7f8e, 0x7ce66634bc9d0b9a},
      {0xa139029f6a239f72, 0x1c1fffc1ebc44e81},
      {0xc987434744ac874e, 0xa327ffb266b56221},
      {0xfbe9141915d7a922, 0x4bf1ff9f0062baa9},
      {0x9d71ac8fada6c9b5, 0x6f773fc3603db4aa},
      {0xc4ce17b399107c22, 0xcb550fb4384d21d4},
      {0xf6019da07f549b2b, 0x7e2a53a146606a49},
      {0x99c102844f94e0fb, 0x2eda7444cbfc426e},
      {0xc0314325637a1939, 0xfa911155fefb5309},
      {0xf03d93eebc589f88, 0x793555ab7eba27cb},
      {0x96267c7535b763b5, 0x4bc1558b2f3458df},
      {0xbbb01b9283253ca2, 0x9eb1aaedfb016f17},
      {0xea9c227723ee8bcb, 0x465e15a979c1cadd},
      {0x92a1958a7675175f, 0x0bfacd89ec191eca},
      {0xb749faed14125d36, 0xcef980ec671f667c},
      {0xe51c79a85916f484, 0x82b7e12780e7401b},
      {0x8f31cc0937ae58d2, 0xd1b2ecb8b0908811},
      {0xb2fe3f0b8599ef07, 0x861fa7e6dcb4aa16},
      {0xdfbdcece67006ac9, 0x67a791e093e1d49b},
      {0x8bd6a141006042bd, 0xe0c8bb2c5c6d24e1},
      {0xaecc49914078536d, 0x58fae9f773886e19},
      {0xda7f5bf590966848, 0xaf39a475506a899f},
      {0x888f99797a5e012d, 0x6d8406c952429604},
      {0xaab37fd7d8f58178, 0xc8e5087ba6d33b84},
      {0xd5605fcdcf32e1d6, 0xfb1e4a9a90880a65},
      {0x855c3be0a17fcd26, 0x5cf2eea09a550680},
      {0xa6b34ad8c9dfc06f, 0xf42faa48c0ea481f},
      {0xd0601d8efc57b08b, 0xf13b94daf124da27},
      {0x823c12795db6ce57, 0x76c53d08d6b70859},
      {0xa2cb1717b52481ed, 0x54768c4b0c64ca6f},
      {0xcb7ddcdda26da268, 0xa9942f5dcf7dfd0a},
      {0xfe5d54150b090b02, 0xd3f93b35435d7c4d},
      {0x9efa548d26e5a6e1, 0xc47bc5014a1a6db0},
      {0xc6b8e9b0709f109a, 0x359ab6419ca1091c},
      {0xf867241c8cc6d4c0, 0xc30163d203c94b63},
      {0x9b407691d7fc44f8, 0x79e0de63425dcf1e},
      {0xc21094364dfb5636, 0x985915fc12f542e5},
      {0xf294b943e17a2bc4, 0x3e6f5b7b17b2939e},
      {0x979cf3ca6cec5b5a, 0xa705992ceecf9c43},
      {0xbd8430bd08277231, 0x50c6ff782a838354},
      {0xece53cec4a314ebd, 0xa4f8bf5635246429},
      {0x940f4613ae5ed136, 0x871b7795e136be9a},
      {0xb913179899f68584, 0x28e2557b59846e40},
      {0xe757dd7ec07426e5, 0x331aeada2fe589d0},
      {0x9096ea6f3848984f, 0x3ff0d2c85def7622},
      {0xb4bca50b065abe63, 0x0fed077a756b53aa},
      {0xe1ebce4dc7f16dfb, 0xd3e8495912c62895},
      {0x8d3360f09cf6e4bd, 0x64712dd7abbbd95d},
      {0xb080392cc4349dec, 0xbd8d794d96aacfb4},
      {0xdca04777f541c567, 0xecf0d7a0fc5583a1},
      {0x89e42caaf9491b60, 0xf41686c49db57245},
      {0xac5d37d5b79b6239, 0x311c2875c522ced6},
      {0xd77485cb25823ac7, 0x7d633293366b828c},
      {0x86a8d39ef77164bc, 0xae5dff9c02033198},
      {0xa8530886b54dbdeb, 0xd9f57f830283fdfd},
      {0xd267caa862a12d66, 0xd072df63c324fd7c},
      {0x8380dea93da4bc60, 0x4247cb9e59f71e6e},
      {0xa46116538d0deb78, 0x52d9be85f074e609},
      {0xcd795be870516656, 0x67902e276c921f8c},
      {0x806bd9714632dff6, 0x00ba1cd8a3db53b7},
      {0xa086cfcd97bf97f3, 0x80e8a40eccd228a5},
      {0xc8a883c0fdaf7df0, 0x6122cd128006b2ce},
      {0xfad2a4b13d1b5d6c, 0x796b805720085f82},
      {0x9cc3a6eec6311a63, 0xcbe3303674053bb1},
      {0xc3f490aa77bd60fc, 0xbedbfc4411068a9d},
      {0xf4f1b4d515acb93b, 0xee92fb5515482d45},
      {0x991711052d8bf3c5, 0x751bdd152d4d1c4b},
      {0xbf5cd54678eef0b6, 0xd262d45a78a0635e},
      {0xef340a98172aace4, 0x86fb897116c87c35},
      {0x9580869f0e7aac0e, 0xd45d35e6ae3d4da1},
      {0xbae0a846d2195712, 0x8974836059cca10a},
      {0xe998d258869facd7, 0x2bd1a438703fc94c},
      {0x91ff83775423cc06, 0x7b6306a34627ddd0},
      {0xb67f6455292cbf08, 0x1a3bc84c17b1d543},
      {0xe41f3d6a7377eeca, 0x20caba5f1d9e4a94},
      {0x8e938662882af53e, 0x547eb47b7282ee9d},
      {0xb23867fb2a35b28d, 0xe99e619a4f23aa44},
      {0xdec681f9f4c31f31, 0x6405fa00e2ec94d5},
      {0x8b3c113c38f9f37e, 0xde83bc408dd3dd05},
      {0xae0b158b4738705e, 0x9624ab50b148d446},
      {0xd98ddaee19068c76, 0x3badd624dd9b0958},
      {0x87f8a8d4cfa417c9, 0xe54ca5d70a80e5d7},
      {0xa9f6d30a038d1dbc, 0x5e9fcf4ccd211f4d},
      {0xd47487cc8470652b, 0x7647c32000696720},
      {0x84c8d4dfd2c63f3b, 0x29ecd9f40041e074},
      {0xa5fb0a17c777cf09, 0xf468107100525891},
      {0xcf79cc9db955c2cc, 0x7182148d4066eeb5},
      {0x81ac1fe293d599bf, 0xc6f14cd848405531},
      {0xa21727db38cb002f, 0xb8ada00e5a506a7d},
      {0xca9cf1d206fdc03b, 0xa6d90811f0e4851d},
      {0xfd442e4688bd304a, 0x908f4a166d1da664},
      {0x9e4a9cec15763e2e, 0x9a598e4e043287ff},
      {0xc5dd44271ad3cdba, 0x40eff1e1853f29fe},
      {0xf7549530e188c128, 0xd12bee59e68ef47d},
      {0x9a94dd3e8cf578b9, 0x82bb74f8301958cf},
      {0xc13a148e3032d6e7, 0xe36a52363c1faf02},
      {0xf18899b1bc3f8ca1, 0xdc44e6c3cb279ac2},
      {0x96f5600f15a7b7e5, 0x29ab103a5ef8c0ba},
      {0xbcb2b812db11a5de, 0x7415d448f6b6f0e8},
      {0xebdf661791d60f56, 0x111b495b3464ad22},
      {0x936b9fcebb25c995, 0xcab10dd900beec35},
      {0xb84687c269ef3bfb, 0x3d5d514f40eea743},
      {0xe65829b3046b0afa, 0x0cb4a5a3112a5113},
      {0x8ff71a0fe2c2e6dc, 0x47f0e785eaba72ac},
      {0xb3f4e093db73a093, 0x59ed216765690f57},
      {0xe0f218b8d25088b8, 0x306869c13ec3532d},
      {0x8c974f7383725573, 0x1e414218c73a13fc},
      {0xafbd2350644eeacf, 0xe5d1929ef90898fb},
      {0xdbac6c247d62a583, 0xdf45f746b74abf3a},
      {0x894bc396ce5da772, 0x6b8bba8c328eb784},
      {0xab9eb47c81f5114f, 0x066ea92f3f326565},
      {0xd686619ba27255a2, 0xc80a537b0efefebe},
      {0x8613fd0145877585, 0xbd06742ce95f5f37},
      {0xa798fc4196e952e7, 0x2c48113823b73705},
      {0xd17f3b51fca3a7a0, 0xf75a15862ca504c6},
      {0x82ef85133de648c4, 0x9a984d73dbe722fc},
      {0xa3ab66580d5fdaf5, 0xc13e60d0d2e0ebbb},
      {0xcc963fee10b7d1b3, 0x318df905079926a9},
      {0xffbbcfe994e5c61f, 0xfdf17746497f7053},
      {0x9fd561f1fd0f9bd3, 0xfeb6ea8bedefa634},
      {0xc7caba6e7c5382c8, 0xfe64a52ee96b8fc1},
      {0xf9bd690a1b68637b, 0x3dfdce7aa3c673b1},
      {0x9c1661a651213e2d, 0x06bea10ca65c084f},
      {0xc31bfa0fe5698db8, 0x486e494fcff30a63},
      {0xf3e2f893dec3f126, 0x5a89dba3c3efccfb},
      {0x986ddb5c6b3a76b7, 0xf89629465a75e01d},
      {0xbe89523386091465, 0xf6bbb397f1135824},
      {0xee2ba6c0678b597f, 0x746aa07ded582e2d},
      {0x94db483840b717ef, 0xa8c2a44eb4571cdd},
      {0xba121a4650e4ddeb, 0x92f34d62616ce414},
      {0xe896a0d7e51e1566, 0x77b020baf9c81d18},
      {0x915e2486ef32cd60, 0x0ace1474dc1d122f},
      {0xb5b5ada8aaff80b8, 0x0d819992132456bb},
      {0xe3231912d5bf60e6, 0x10e1fff697ed6c6a},
      {0x8df5efabc5979c8f, 0xca8d3ffa1ef463c2},
      {0xb1736b96b6fd83b3, 0xbd308ff8a6b17cb3},
      {0xddd0467c64bce4a0, 0xac7cb3f6d05ddbdf},
      {0x8aa22c0dbef60ee4, 0x6bcdf07a423aa96c},
      {0xad4ab7112eb3929d, 0x86c16c98d2c953c7},
      {0xd89d64d57a607744, 0xe871c7bf077ba8b8},
      {0x87625f056c7c4a8b, 0x11471cd764ad4973},
      {0xa93af6c6c79b5d2d, 0xd598e40d3dd89bd0},
      {0xd389b47879823479, 0x4aff1d108d4ec2c4},
      {0x843610cb4bf160cb, 0xcedf722a585139bb},
      {0xa54394fe1eedb8fe, 0xc2974eb4ee658829},
      {0xce947a3da6a9273e, 0x733d226229feea33},
      {0x811ccc668829b887, 0x0806357d5a3f5260},
      {0xa163ff802a3426a8, 0xca07c2dcb0cf26f8},
      {0xc9bcff6034c13052, 0xfc89b393dd02f0b6},
      {0xfc2c3f3841f17c67, 0xbbac2078d443ace3},
      {0x9d9ba7832936edc0, 0xd54b944b84aa4c0e},
      {0xc5029163f384a931, 0x0a9e795e65d4df12},
      {0xf64335bcf065d37d, 0x4d4617b5ff4a16d6},
      {0x99ea0196163fa42e, 0x504bced1bf8e4e46},
      {0xc06481fb9bcf8d39, 0xe45ec2862f71e1d7},
      {0xf07da27a82c37088, 0x5d767327bb4e5a4d},
      {0x964e858c91ba2655, 0x3a6a07f8d510f870},
      {0xbbe226efb628afea, 0x890489f70a55368c},
      {0xeadab0aba3b2dbe5, 0x2b45ac74ccea842f},
      {0x92c8ae6b464fc96f, 0x3b0b8bc90012929e},
      {0xb77ada0617e3bbcb, 0x09ce6ebb40173745},
      {0xe55990879ddcaabd, 0xcc420a6a101d0516},
      {0x8f57fa54c2a9eab6, 0x9fa946824a12232e},
      {0xb32df8e9f3546564, 0x47939822dc96abfa},
      {0xdff9772470297ebd, 0x59787e2b93bc56f8},
      {0x8bfbea76c619ef36, 0x57eb4edb3c55b65b},
      {0xaefae51477a06b03, 0xede622920b6b23f2},
      {0xdab99e59958885c4, 0xe95fab368e45ecee},
      {0x88b402f7fd75539b, 0x11dbcb0218ebb415},
      {0xaae103b5fcd2a881, 0xd652bdc29f26a11a},
      {0xd59944a37c0752a2, 0x4be76d3346f04960},
      {0x857fcae62d8493a5, 0x6f70a4400c562ddc},
      {0xa6dfbd9fb8e5b88e, 0xcb4ccd500f6bb953},
      {0xd097ad07a71f26b2, 0x7e2000a41346a7a8},
      {0x825ecc24c873782f, 0x8ed400668c0c28c9},
      {0xa2f67f2dfa90563b, 0x728900802f0f32fb},
      {0xcbb41ef979346bca, 0x4f2b40a03ad2ffba},
      {0xfea126b7d78186bc, 0xe2f610c84987bfa9},
      {0x9f24b832e6b0f436, 0x0dd9ca7d2df4d7ca},
      {0xc6ede63fa05d3143, 0x91503d1c79720dbc},
      {0xf8a95fcf88747d94, 0x75a44c6397ce912b},
      {0x9b69dbe1b548ce7c, 0xc986afbe3ee11abb},
      {0xc24452da229b021b, 0xfbe85badce996169},
      {0xf2d56790ab41c2a2, 0xfae27299423fb9c4},
      {0x97c560ba6b0919a5, 0xdccd879fc967d41b},
      {0xbdb6b8e905cb600f, 0x5400e987bbc1c921},
      {0xed246723473e3813, 0x290123e9aab23b69},
      {0x9436c0760c86e30b, 0xf9a0b6720aaf6522},
      {0xb94470938fa89bce, 0xf808e40e8d5b3e6a},
      {0xe7958cb87392c2c2, 0xb60b1d1230b20e05},
      {0x90bd77f3483bb9b9, 0xb1c6f22b5e6f48c3},
      {0xb4ecd5f01a4aa828, 0x1e38aeb6360b1af4},
      {0xe2280b6c20dd5232, 0x25c6da63c38de1b1},
      {0x8d590723948a535f, 0x579c487e5a38ad0f},
      {0xb0af48ec79ace837, 0x2d835a9df0c6d852},
      {0xdcdb1b2798182244, 0xf8e431456cf88e66},
      {0x8a08f0f8bf0f156b, 0x1b8e9ecb641b5900},
      {0xac8b2d36eed2dac5, 0xe272467e3d222f40},
      {0xd7adf884aa879177, 0x5b0ed81dcc6abb10},
      {0x86ccbb52ea94baea, 0x98e947129fc2b4ea},
      {0xa87fea27a539e9a5, 0x3f2398d747b36225},
      {0xd29fe4b18e88640e, 0x8eec7f0d19a03aae},
      {0x83a3eeeef9153e89, 0x1953cf68300424ad},
      {0xa48ceaaab75a8e2b, 0x5fa8c3423c052dd8},
      {0xcdb02555653131b6, 0x3792f412cb06794e},
      {0x808e17555f3ebf11, 0xe2bbd88bbee40bd1},
      {0xa0b19d2ab70e6ed6, 0x5b6aceaeae9d0ec5},
      {0xc8de047564d20a8b, 0xf245825a5a445276},
      {0xfb158592be068d2e, 0xeed6e2f0f0d56713},
      {0x9ced737bb6c4183d, 0x55464dd69685606c},
      {0xc428d05aa4751e4c, 0xaa97e14c3c26b887},
      {0xf53304714d9265df, 0xd53dd99f4b3066a9},
      {0x993fe2c6d07b7fab, 0xe546a8038efe402a},
      {0xbf8fdb78849a5f96, 0xde98520472bdd034},
      {0xef73d256a5c0f77c, 0x963e66858f6d4441},
      {0x95a8637627989aad, 0xdde7001379a44aa9},
      {0xbb127c53b17ec159, 0x5560c018580d5d53},
      {0xe9d71b689dde71af, 0xaab8f01e6e10b4a7},
      {0x9226712162ab070d, 0xcab3961304ca70e9},
      {0xb6b00d69bb55c8d1, 0x3d607b97c5fd0d23},
      {0xe45c10c42a2b3b05, 0x8cb89a7db77c506b},
      {0x8eb98a7a9a5b04e3, 0x77f3608e92adb243},
      {0xb267ed1940f1c61c, 0x55f038b237591ed4},
      {0xdf01e85f912e37a3, 0x6b6c46dec52f6689},
      {0x8b61313bbabce2c6, 0x2323ac4b3b3da016},
      {0xae397d8aa96c1b77, 0xabec975e0a0d081b},
      {0xd9c7dced53c72255, 0x96e7bd358c904a22},
      {0x881cea14545c7575, 0x7e50d64177da2e55},
      {0xaa242499697392d2, 0xdde50bd1d5d0b9ea},
      {0xd4ad2dbfc3d07787, 0x955e4ec64b44e865},
      {0x84ec3c97da624ab4, 0xbd5af13bef0b113f},
      {0xa6274bbdd0fadd61, 0xecb1ad8aeacdd58f},
      {0xcfb11ead453994ba, 0x67de18eda5814af3},
      {0x81ceb32c4b43fcf4, 0x80eacf948770ced8},
      {0xa2425ff75e14fc31, 0xa1258379a94d028e},
      {0xcad2f7f5359a3b3e, 0x096ee45813a04331},
      {0xfd87b5f28300ca0d, 0x8bca9d6e188853fd},
      {0x9e74d1b791e07e48, 0x775ea264cf55347e},
      {0xc612062576589dda, 0x95364afe032a819e},
      {0xf79687aed3eec551, 0x3a83ddbd83f52205},
      {0x9abe14cd44753b52, 0xc4926a9672793543},
      {0xc16d9a0095928a27, 0x75b7053c0f178294},
      {0xf1c90080baf72cb1, 0x5324c68b12dd6339},
      {0x971da05074da7bee, 0xd3f6fc16ebca5e04},
      {0xbce5086492111aea, 0x88f4bb1ca6bcf585},
      {0xec1e4a7db69561a5, 0x2b31e9e3d06c32e6},
      {0x9392ee8e921d5d07, 0x3aff322e62439fd0},
      {0xb877aa3236a4b449, 0x09befeb9fad487c3},
      {0xe69594bec44de15b, 0x4c2ebe687989a9b4},
      {0x901d7cf73ab0acd9, 0x0f9d37014bf60a11},
      {0xb424dc35095cd80f, 0x538484c19ef38c95},
      {0xe12e13424bb40e13, 0x2865a5f206b06fba},
      {0x8cbccc096f5088cb, 0xf93f87b7442e45d4},
      {0xafebff0bcb24aafe, 0xf78f69a51539d749},
      {0xdbe6fecebdedd5be, 0xb573440e5a884d1c},
      {0x89705f4136b4a597, 0x31680a88f8953031},
      {0xabcc77118461cefc, 0xfdc20d2b36ba7c3e},
      {0xd6bf94d5e57a42bc, 0x3d32907604691b4d},
      {0x8637bd05af6c69b5, 0xa63f9a49c2c1b110},
      {0xa7c5ac471b478423, 0x0fcf80dc33721d54},
      {0xd1b71758e219652b, 0xd3c36113404ea4a9},
      {0x83126e978d4fdf3b, 0x645a1cac083126ea},
      {0xa3d70a3d70a3d70a, 0x3d70a3d70a3d70a4},
      {0xcccccccccccccccc, 0xcccccccccccccccd},
      {0x8000000000000000, 0x0000000000000000},
      {0xa000000000000000, 0x0000000000000000},
      {0xc800000000000000, 0x0000000000000000},
      {0xfa00000000000000, 0x0000000000000000},
      {0x9c40000000000000, 0x0000000000000000},
      {0xc350000000000000, 0x0000000000000000},
      {0xf424000000000000, 0x0000000000000000},
      {0x9896800000000000, 0x0000000000000000},
      {0xbebc200000000000, 0x0000000000000000},
      {0xee6b280000000000, 0x0000000000000000},
      {0x9502f90000000000, 0x0000000000000000},
      {0xba43b74000000000, 0x0000000000000000},
      {0xe8d4a51000000000, 0x0000000000000000},
      {0x9184e72a00000000, 0x0000000000000000},
      {0xb5e620f480000000, 0x0000000000000000},
      {0xe35fa931a0000000, 0x0000000000000000},
      {0x8e1bc9bf04000000, 0x0000000000000000},
      {0xb1a2bc2ec5000000, 0x0000000000000000},
      {0xde0b6b3a76400000, 0x0000000000000000},
      {0x8ac7230489e80000, 0x0000000000000000},
      {0xad78ebc5ac620000, 0x0000000000000000},
      {0xd8d726b7177a8000, 0x0000000000000000},
      {0x878678326eac9000, 0x0000000000000000},
      {0xa968163f0a57b400, 0x0000000000000000},
      {0xd3c21bcecceda100, 0x0000000000000000},
      {0x84595161401484a0, 0x0000000000000000},
      {0xa56fa5b99019a5c8, 0x0000000000000000},
      {0xcecb8f27f4200f3a, 0x0000000000000000},
      {0x813f3978f8940984, 0x4000000000000000},
      {0xa18f07d736b90be5, 0x5000000000000000},
      {0xc9f2c9cd04674ede, 0xa400000000000000},
      {0xfc6f7c4045812296, 0x4d00000000000000},
      {0x9dc5ada82b70b59d, 0xf020000000000000},
      {0xc5371912364ce305, 0x6c28000000000000},
      {0xf684df56c3e01bc6, 0xc732000000000000},
      {0x9a130b963a6c115c, 0x3c7f400000000000},
      {0xc097ce7bc90715b3, 0x4b9f100000000000},
      {0xf0bdc21abb48db20, 0x1e86d40000000000},
      {0x96769950b50d88f4, 0x1314448000000000},
      {0xbc143fa4e250eb31, 0x17d955a000000000},
      {0xeb194f8e1ae525fd, 0x5dcfab0800000000},
      {0x92efd1b8d0cf37be, 0x5aa1cae500000000},
      {0xb7abc627050305ad, 0xf14a3d9e40000000},
      {0xe596b7b0c643c719, 0x6d9ccd05d0000000},
      {0x8f7e32ce7bea5c6f, 0xe4820023a2000000},
      {0xb35dbf821ae4f38b, 0xdda2802c8a800000},
      {0xe0352f62a19e306e, 0xd50b2037ad200000},
      {0x8c213d9da502de45, 0x4526f422cc340000},
      {0xaf298d050e4395d6, 0x9670b12b7f410000},
      {0xdaf3f04651d47b4c, 0x3c0cdd765f114000},
      {0x88d8762bf324cd0f, 0xa5880a69fb6ac800},
      {0xab0e93b6efee0053, 0x8eea0d047a457a00},
      {0xd5d238a4abe98068, 0x72a4904598d6d880},
      {0x85a36366eb71f041, 0x47a6da2b7f864750},
      {0xa70c3c40a64e6c51, 0x999090b65f67d924},
      {0xd0cf4b50cfe20765, 0xfff4b4e3f741cf6d},
      {0x82818f1281ed449f, 0xbff8f10e7a8921a5},
      {0xa321f2d7226895c7, 0xaff72d52192b6a0e},
      {0xcbea6f8ceb02bb39, 0x9bf4f8a69f764491},
      {0xfee50b7025c36a08, 0x02f236d04753d5b5},
      {0x9f4f2726179a2245, 0x01d762422c946591},
      {0xc722f0ef9d80aad6, 0x424d3ad2b7b97ef6},
      {0xf8ebad2b84e0d58b, 0xd2e0898765a7deb3},
      {0x9b934c3b330c8577, 0x63cc55f49f88eb30},
      {0xc2781f49ffcfa6d5, 0x3cbf6b71c76b25fc},
      {0xf316271c7fc3908a, 0x8bef464e3945ef7b},
      {0x97edd871cfda3a56, 0x97758bf0e3cbb5ad},
      {0xbde94e8e43d0c8ec, 0x3d52eeed1cbea318},
      {0xed63a231d4c4fb27, 0x4ca7aaa863ee4bde},
      {0x945e455f24fb1cf8, 0x8fe8caa93e74ef6b},
      {0xb975d6b6ee39e436, 0xb3e2fd538e122b45},
      {0xe7d34c64a9c85d44, 0x60dbbca87196b617},
      {0x90e40fbeea1d3a4a, 0xbc8955e946fe31ce},
      {0xb51d13aea4a488dd, 0x6babab6398bdbe42},
      {0xe264589a4dcdab14, 0xc696963c7eed2dd2},
      {0x8d7eb76070a08aec, 0xfc1e1de5cf543ca3},
      {0xb0de65388cc8ada8, 0x3b25a55f43294bcc},
      {0xdd15fe86affad912, 0x49ef0eb713f39ebf},
      {0x8a2dbf142dfcc7ab, 0x6e3569326c784338},
      {0xacb92ed9397bf996, 0x49c2c37f07965405},
      {0xd7e77a8f87daf7fb, 0xdc33745ec97be907},
      {0x86f0ac99b4e8dafd, 0x69a028bb3ded71a4},
      {0xa8acd7c0222311bc, 0xc40832ea0d68ce0d},
      {0xd2d80db02aabd62b, 0xf50a3fa490c30191},
      {0x83c7088e1aab65db, 0x792667c6da79e0fb},
      {0xa4b8cab1a1563f52, 0x577001b891185939},
      {0xcde6fd5e09abcf26, 0xed4c0226b55e6f87},
      {0x80b05e5ac60b6178, 0x544f8158315b05b5},
      {0xa0dc75f1778e39d6, 0x696361ae3db1c722},
      {0xc913936dd571c84c, 0x03bc3a19cd1e38ea},
      {0xfb5878494ace3a5f, 0x04ab48a04065c724},
      {0x9d174b2dcec0e47b, 0x62eb0d64283f9c77},
      {0xc45d1df942711d9a, 0x3ba5d0bd324f8395},
      {0xf5746577930d6500, 0xca8f44ec7ee3647a},
      {0x9968bf6abbe85f20, 0x7e998b13cf4e1ecc},
      {0xbfc2ef456ae276e8, 0x9e3fedd8c321a67f},
      {0xefb3ab16c59b14a2, 0xc5cfe94ef3ea101f},
      {0x95d04aee3b80ece5, 0xbba1f1d158724a13},
      {0xbb445da9ca61281f, 0x2a8a6e45ae8edc98},
      {0xea1575143cf97226, 0xf52d09d71a3293be},
      {0x924d692ca61be758, 0x593c2626705f9c57},
      {0xb6e0c377cfa2e12e, 0x6f8b2fb00c77836d},
      {0xe498f455c38b997a, 0x0b6dfb9c0f956448},
      {0x8edf98b59a373fec, 0x4724bd4189bd5ead},
      {0xb2977ee300c50fe7, 0x58edec91ec2cb658},
      {0xdf3d5e9bc0f653e1, 0x2f2967b66737e3ee},
      {0x8b865b215899f46c, 0xbd79e0d20082ee75},
      {0xae67f1e9aec07187, 0xecd8590680a3aa12},
      {0xda01ee641a708de9, 0xe80e6f4820cc9496},
      {0x884134fe908658b2, 0x3109058d147fdcde},
      {0xaa51823e34a7eede, 0xbd4b46f0599fd416},
      {0xd4e5e2cdc1d1ea96, 0x6c9e18ac7007c91b},
      {0x850fadc09923329e, 0x03e2cf6bc604ddb1},
      {0xa6539930bf6bff45, 0x84db8346b786151d},
      {0xcfe87f7cef46ff16, 0xe612641865679a64},
      {0x81f14fae158c5f6e, 0x4fcb7e8f3f60c07f},
      {0xa26da3999aef7749, 0xe3be5e330f38f09e},
      {0xcb090c8001ab551c, 0x5cadf5bfd3072cc6},
      {0xfdcb4fa002162a63, 0x73d9732fc7c8f7f7},
      {0x9e9f11c4014dda7e, 0x2867e7fddcdd9afb},
      {0xc646d63501a1511d, 0xb281e1fd541501b9},
      {0xf7d88bc24209a565, 0x1f225a7ca91a4227},
      {0x9ae757596946075f, 0x3375788de9b06959},
      {0xc1a12d2fc3978937, 0x0052d6b1641c83af},
      {0xf209787bb47d6b84, 0xc0678c5dbd23a49b},
      {0x9745eb4d50ce6332, 0xf840b7ba963646e1},
      {0xbd176620a501fbff, 0xb650e5a93bc3d899},
      {0xec5d3fa8ce427aff, 0xa3e51f138ab4cebf},
      {0x93ba47c980e98cdf, 0xc66f336c36b10138},
      {0xb8a8d9bbe123f017, 0xb80b0047445d4185},
      {0xe6d3102ad96cec1d, 0xa60dc059157491e6},
      {0x9043ea1ac7e41392, 0x87c89837ad68db30},
      {0xb454e4a179dd1877, 0x29babe4598c311fc},
      {0xe16a1dc9d8545e94, 0xf4296dd6fef3d67b},
      {0x8ce2529e2734bb1d, 0x1899e4a65f58660d},
      {0xb01ae745b101e9e4, 0x5ec05dcff72e7f90},
      {0xdc21a1171d42645d, 0x76707543f4fa1f74},
      {0x899504ae72497eba, 0x6a06494a791c53a9},
      {0xabfa45da0edbde69, 0x0487db9d17636893},
      {0xd6f8d7509292d603, 0x45a9d2845d3c42b7},
      {0x865b86925b9bc5c2, 0x0b8a2392ba45a9b3},
      {0xa7f26836f282b732, 0x8e6cac7768d7141f},
      {0xd1ef0244af2364ff, 0x3207d795430cd927},
      {0x8335616aed761f1f, 0x7f44e6bd49e807b9},
      {0xa402b9c5a8d3a6e7, 0x5f16206c9c6209a7},
      {0xcd036837130890a1, 0x36dba887c37a8c10},
      {0x802221226be55a64, 0xc2494954da2c978a},
      {0xa02aa96b06deb0fd, 0xf2db9baa10b7bd6d},
      {0xc83553c5c8965d3d, 0x6f92829494e5acc8},
      {0xfa42a8b73abbf48c, 0xcb772339ba1f17fa},
      {0x9c69a97284b578d7, 0xff2a760414536efc},
      {0xc38413cf25e2d70d, 0xfef5138519684abb},
      {0xf46518c2ef5b8cd1, 0x7eb258665fc25d6a},
      {0x98bf2f79d5993802, 0xef2f773ffbd97a62},
      {0xbeeefb584aff8603, 0xaafb550ffacfd8fb},
      {0xeeaaba2e5dbf6784, 0x95ba2a53f983cf39},
      {0x952ab45cfa97a0b2, 0xdd945a747bf26184},
      {0xba756174393d88df, 0x94f971119aeef9e5},
      {0xe912b9d1478ceb17, 0x7a37cd5601aab85e},
      {0x91abb422ccb812ee, 0xac62e055c10ab33b},
      {0xb616a12b7fe617aa, 0x577b986b314d600a},
      {0xe39c49765fdf9d94, 0xed5a7e85fda0b80c},
      {0x8e41ade9fbebc27d, 0x14588f13be847308},
      {0xb1d219647ae6b31c, 0x596eb2d8ae258fc9},
      {0xde469fbd99a05fe3, 0x6fca5f8ed9aef3bc},
      {0x8aec23d680043bee, 0x25de7bb9480d5855},
      {0xada72ccc20054ae9, 0xaf561aa79a10ae6b},
      {0xd910f7ff28069da4, 0x1b2ba1518094da05},
      {0x87aa9aff79042286, 0x90fb44d2f05d0843},
      {0xa99541bf57452b28, 0x353a1607ac744a54},
      {0xd3fa922f2d1675f2, 0x42889b8997915ce9},
      {0x847c9b5d7c2e09b7, 0x69956135febada12},
      {0xa59bc234db398c25, 0x43fab9837e699096},
      {0xcf02b2c21207ef2e, 0x94f967e45e03f4bc},
      {0x8161afb94b44f57d, 0x1d1be0eebac278f6},
      {0xa1ba1ba79e1632dc, 0x6462d92a69731733},
      {0xca28a291859bbf93, 0x7d7b8f7503cfdcff},
      {0xfcb2cb35e702af78, 0x5cda735244c3d43f},
      {0x9defbf01b061adab, 0x3a0888136afa64a8},
      {0xc56baec21c7a1916, 0x088aaa1845b8fdd1},
      {0xf6c69a72a3989f5b, 0x8aad549e57273d46},
      {0x9a3c2087a63f6399, 0x36ac54e2f678864c},
      {0xc0cb28a98fcf3c7f, 0x84576a1bb416a7de},
      {0xf0fdf2d3f3c30b9f, 0x656d44a2a11c51d6},
      {0x969eb7c47859e743, 0x9f644ae5a4b1b326},
      {0xbc4665b596706114, 0x873d5d9f0dde1fef},
      {0xeb57ff22fc0c7959, 0xa90cb506d155a7eb},
      {0x9316ff75dd87cbd8, 0x09a7f12442d588f3},
      {0xb7dcbf5354e9bece, 0x0c11ed6d538aeb30},
      {0xe5d3ef282a242e81, 0x8f1668c8a86da5fb},
      {0x8fa475791a569d10, 0xf96e017d694487bd},
      {0xb38d92d760ec4455, 0x37c981dcc395a9ad},
      {0xe070f78d3927556a, 0x85bbe253f47b1418},
      {0x8c469ab843b89562, 0x93956d7478ccec8f},
      {0xaf58416654a6babb, 0x387ac8d1970027b3},
      {0xdb2e51bfe9d0696a, 0x06997b05fcc0319f},
      {0x88fcf317f22241e2, 0x441fece3bdf81f04},
      {0xab3c2fddeeaad25a, 0xd527e81cad7626c4},
      {0xd60b3bd56a5586f1, 0x8a71e223d8d3b075},
      {0x85c7056562757456, 0xf6872d5667844e4a},
      {0xa738c6bebb12d16c, 0xb428f8ac016561dc},
      {0xd106f86e69d785c7, 0xe13336d701beba53},
      {0x82a45b450226b39c, 0xecc0024661173474},
      {0xa34d721642b06084, 0x27f002d7f95d0191},
      {0xcc20ce9bd35c78a5, 0x31ec038df7b441f5},
      {0xff290242c83396ce, 0x7e67047175a15272},
      {0x9f79a169bd203e41, 0x0f0062c6e984d387},
      {0xc75809c42c684dd1, 0x52c07b78a3e60869},
      {0xf92e0c3537826145, 0xa7709a56ccdf8a83},
      {0x9bbcc7a142b17ccb, 0x88a66076400bb692},
      {0xc2abf989935ddbfe, 0x6acff893d00ea436},
      {0xf356f7ebf83552fe, 0x0583f6b8c4124d44},
      {0x98165af37b2153de, 0xc3727a337a8b704b},
      {0xbe1bf1b059e9a8d6, 0x744f18c0592e4c5d},
      {0xeda2ee1c7064130c, 0x1162def06f79df74},
      {0x9485d4d1c63e8be7, 0x8addcb5645ac2ba9},
      {0xb9a74a0637ce2ee1, 0x6d953e2bd7173693},
      {0xe8111c87c5c1ba99, 0xc8fa8db6ccdd0438},
      {0x910ab1d4db9914a0, 0x1d9c9892400a22a3},
      {0xb54d5e4a127f59c8, 0x2503beb6d00cab4c},
      {0xe2a0b5dc971f303a, 0x2e44ae64840fd61e},
      {0x8da471a9de737e24, 0x5ceaecfed289e5d3},
      {0xb10d8e1456105dad, 0x7425a83e872c5f48},
      {0xdd50f1996b947518, 0xd12f124e28f7771a},
      {0x8a5296ffe33cc92f, 0x82bd6b70d99aaa70},
      {0xace73cbfdc0bfb7b, 0x636cc64d1001550c},
      {0xd8210befd30efa5a, 0x3c47f7e05401aa4f},
      {0x8714a775e3e95c78, 0x65acfaec34810a72},
      {0xa8d9d1535ce3b396, 0x7f1839a741a14d0e},
      {0xd31045a8341ca07c, 0x1ede48111209a051},
      {0x83ea2b892091e44d, 0x934aed0aab460433},
      {0xa4e4b66b68b65d60, 0xf81da84d56178540},
      {0xce1de40642e3f4b9, 0x36251260ab9d668f},
      {0x80d2ae83e9ce78f3, 0xc1d72b7c6b42601a},
      {0xa1075a24e4421730, 0xb24cf65b8612f820},
      {0xc94930ae1d529cfc, 0xdee033f26797b628},
      {0xfb9b7cd9a4a7443c, 0x169840ef017da3b2},
      {0x9d412e0806e88aa5, 0x8e1f289560ee864f},
      {0xc491798a08a2ad4e, 0xf1a6f2bab92a27e3},
      {0xf5b5d7ec8acb58a2, 0xae10af696774b1dc},
      {0x9991a6f3d6bf1765, 0xacca6da1e0a8ef2a},
      {0xbff610b0cc6edd3f, 0x17fd090a58d32af4},
      {0xeff394dcff8a948e, 0xddfc4b4cef07f5b1},
      {0x95f83d0a1fb69cd9, 0x4abdaf101564f98f},
      {0xbb764c4ca7a4440f, 0x9d6d1ad41abe37f2},
      {0xea53df5fd18d5513, 0x84c86189216dc5ee},
      {0x92746b9be2f8552c, 0x32fd3cf5b4e49bb5},
      {0xb7118682dbb66a77, 0x3fbc8c33221dc2a2},
      {0xe4d5e82392a40515, 0x0fabaf3feaa5334b},
      {0x8f05b1163ba6832d, 0x29cb4d87f2a7400f},
      {0xb2c71d5bca9023f8, 0x743e20e9ef511013},
      {0xdf78e4b2bd342cf6, 0x914da9246b255417},
      {0x8bab8eefb6409c1a, 0x1ad089b6c2f7548f},
      {0xae9672aba3d0c320, 0xa184ac2473b529b2},
      {0xda3c0f568cc4f3e8, 0xc9e5d72d90a2741f},
      {0x8865899617fb1871, 0x7e2fa67c7a658893},
      {0xaa7eebfb9df9de8d, 0xddbb901b98feeab8},
      {0xd51ea6fa85785631, 0x552a74227f3ea566},
      {0x8533285c936b35de, 0xd53a88958f872760},
      {0xa67ff273b8460356, 0x8a892abaf368f138},
      {0xd01fef10a657842c, 0x2d2b7569b0432d86},
      {0x8213f56a67f6b29b, 0x9c3b29620e29fc74},
      {0xa298f2c501f45f42, 0x8349f3ba91b47b90},
      {0xcb3f2f7642717713, 0x241c70a936219a74},
      {0xfe0efb53d30dd4d7, 0xed238cd383aa0111},
      {0x9ec95d1463e8a506, 0xf4363804324a40ab},
      {0xc67bb4597ce2ce48, 0xb143c6053edcd0d6},
      {0xf81aa16fdc1b81da, 0xdd94b7868e94050b},
      {0x9b10a4e5e9913128, 0xca7cf2b4191c8327},
      {0xc1d4ce1f63f57d72, 0xfd1c2f611f63a3f1},
      {0xf24a01a73cf2dccf, 0xbc633b39673c8ced},
      {0x976e41088617ca01, 0xd5be0503e085d814},
      {0xbd49d14aa79dbc82, 0x4b2d8644d8a74e19},
      {0xec9c459d51852ba2, 0xddf8e7d60ed1219f},
      {0x93e1ab8252f33b45, 0xcabb90e5c942b504},
      {0xb8da1662e7b00a17, 0x3d6a751f3b936244},
      {0xe7109bfba19c0c9d, 0x0cc512670a783ad5},
      {0x906a617d450187e2, 0x27fb2b80668b24c6},
      {0xb484f9dc9641e9da, 0xb1f9f660802dedf7},
      {0xe1a63853bbd26451, 0x5e7873f8a0396974},
      {0x8d07e33455637eb2, 0xdb0b487b6423e1e9},
      {0xb049dc016abc5e5f, 0x91ce1a9a3d2cda63},
      {0xdc5c5301c56b75f7, 0x7641a140cc7810fc},
      {0x89b9b3e11b6329ba, 0xa9e904c87fcb0a9e},
      {0xac2820d9623bf429, 0x546345fa9fbdcd45},
      {0xd732290fbacaf133, 0xa97c177947ad4096},
      {0x867f59a9d4bed6c0, 0x49ed8eabcccc485e},
      {0xa81f301449ee8c70, 0x5c68f256bfff5a75},
      {0xd226fc195c6a2f8c, 0x73832eec6fff3112},
      {0x83585d8fd9c25db7, 0xc831fd53c5ff7eac},
      {0xa42e74f3d032f525, 0xba3e7ca8b77f5e56},
      {0xcd3a1230c43fb26f, 0x28ce1bd2e55f35ec},
      {0x80444b5e7aa7cf85, 0x7980d163cf5b81b4},
      {0xa0555e361951c366, 0xd7e105bcc3326220},
      {0xc86ab5c39fa63440, 0x8dd9472bf3fefaa8},
      {0xfa856334878fc150, 0xb14f98f6f0feb952},
      {0x9c935e00d4b9d8d2, 0x6ed1bf9a569f33d4},
      {0xc3b8358109e84f07, 0x0a862f80ec4700c9},
      {0xf4a642e14c6262c8, 0xcd27bb612758c0fb},
      {0x98e7e9cccfbd7dbd, 0x8038d51cb897789d},
      {0xbf21e44003acdd2c, 0xe0470a63e6bd56c4},
      {0xeeea5d5004981478, 0x1858ccfce06cac75},
      {0x95527a5202df0ccb, 0x0f37801e0c43ebc9},
      {0xbaa718e68396cffd, 0xd30560258f54e6bb},
      {0xe950df20247c83fd, 0x47c6b82ef32a206a},
      {0x91d28b7416cdd27e, 0x4cdc331d57fa5442},
      {0xb6472e511c81471d, 0xe0133fe4adf8e953},
      {0xe3d8f9e563a198e5, 0x58180fddd97723a7},
      {0x8e679c2f5e44ff8f, 0x570f09eaa7ea7649},
      {0xb201833b35d63f73, 0x2cd2cc6551e513db},
      {0xde81e40a034bcf4f, 0xf8077f7ea65e58d2},
      {0x8b112e86420f6191, 0xfb04afaf27faf783},
      {0xadd57a27d29339f6, 0x79c5db9af1f9b564},
      {0xd94ad8b1c7380874, 0x18375281ae7822bd},
      {0x87cec76f1c830548, 0x8f2293910d0b15b6},
      {0xa9c2794ae3a3c69a, 0xb2eb3875504ddb23},
      {0xd433179d9c8cb841, 0x5fa60692a46151ec},
      {0x849feec281d7f328, 0xdbc7c41ba6bcd334},
      {0xa5c7ea73224deff3, 0x12b9b522906c0801},
      {0xcf39e50feae16bef, 0xd768226b34870a01},
      {0x81842f29f2cce375, 0xe6a1158300d46641},
      {0xa1e53af46f801c53, 0x60495ae3c1097fd1},
      {0xca5e89b18b602368, 0x385bb19cb14bdfc5},
      {0xfcf62c1dee382c42, 0x46729e03dd9ed7b6},
      {0x9e19db92b4e31ba9, 0x6c07a2c26a8346d2},
      {0xc5a05277621be293, 0xc7098b7305241886},
      {0xf70867153aa2db38, 0xb8cbee4fc66d1ea8},
      {0x9a65406d44a5c903, 0x737f74f1dc043329},
      {0xc0fe908895cf3b44, 0x505f522e53053ff3},
      {0xf13e34aabb430a15, 0x647726b9e7c68ff0},
      {0x96c6e0eab509e64d, 0x5eca783430dc19f6},
      {0xbc789925624c5fe0, 0xb67d16413d132073},
      {0xeb96bf6ebadf77d8, 0xe41c5bd18c57e890},
      {0x933e37a534cbaae7, 0x8e91b962f7b6f15a},
      {0xb80dc58e81fe95a1, 0x723627bbb5a4adb1},
      {0xe61136f2227e3b09, 0xcec3b1aaa30dd91d},
      {0x8fcac257558ee4e6, 0x213a4f0aa5e8a7b2},
      {0xb3bd72ed2af29e1f, 0xa988e2cd4f62d19e},
      {0xe0accfa875af45a7, 0x93eb1b80a33b8606},
      {0x8c6c01c9498d8b88, 0xbc72f130660533c4},
      {0xaf87023b9bf0ee6a, 0xeb8fad7c7f8680b5},
      {0xdb68c2ca82ed2a05, 0xa67398db9f6820e2},
#else
      {0xff77b1fcbebcdc4f, 0x25e8e89c13bb0f7b},
      {0xce5d73ff402d98e3, 0xfb0a3d212dc81290},
      {0xa6b34ad8c9dfc06f, 0xf42faa48c0ea481f},
      {0x86a8d39ef77164bc, 0xae5dff9c02033198},
      {0xd98ddaee19068c76, 0x3badd624dd9b0958},
      {0xafbd2350644eeacf, 0xe5d1929ef90898fb},
      {0x8df5efabc5979c8f, 0xca8d3ffa1ef463c2},
      {0xe55990879ddcaabd, 0xcc420a6a101d0516},
      {0xb94470938fa89bce, 0xf808e40e8d5b3e6a},
      {0x95a8637627989aad, 0xdde7001379a44aa9},
      {0xf1c90080baf72cb1, 0x5324c68b12dd6339},
      {0xc350000000000000, 0x0000000000000000},
      {0x9dc5ada82b70b59d, 0xf020000000000000},
      {0xfee50b7025c36a08, 0x02f236d04753d5b5},
      {0xcde6fd5e09abcf26, 0xed4c0226b55e6f87},
      {0xa6539930bf6bff45, 0x84db8346b786151d},
      {0x865b86925b9bc5c2, 0x0b8a2392ba45a9b3},
      {0xd910f7ff28069da4, 0x1b2ba1518094da05},
      {0xaf58416654a6babb, 0x387ac8d1970027b3},
      {0x8da471a9de737e24, 0x5ceaecfed289e5d3},
      {0xe4d5e82392a40515, 0x0fabaf3feaa5334b},
      {0xb8da1662e7b00a17, 0x3d6a751f3b936244},
      {0x95527a5202df0ccb, 0x0f37801e0c43ebc9},
      {0xf13e34aabb430a15, 0x647726b9e7c68ff0}
#endif
    };

#if FMT_USE_FULL_CACHE_DRAGONBOX
    return pow10_significands[k - float_info<double>::min_k];
#else
    static constexpr const uint64_t powers_of_5_64[] = {
        0x0000000000000001, 0x0000000000000005, 0x0000000000000019,
        0x000000000000007d, 0x0000000000000271, 0x0000000000000c35,
        0x0000000000003d09, 0x000000000001312d, 0x000000000005f5e1,
        0x00000000001dcd65, 0x00000000009502f9, 0x0000000002e90edd,
        0x000000000e8d4a51, 0x0000000048c27395, 0x000000016bcc41e9,
        0x000000071afd498d, 0x0000002386f26fc1, 0x000000b1a2bc2ec5,
        0x000003782dace9d9, 0x00001158e460913d, 0x000056bc75e2d631,
        0x0001b1ae4d6e2ef5, 0x000878678326eac9, 0x002a5a058fc295ed,
        0x00d3c21bcecceda1, 0x0422ca8b0a00a425, 0x14adf4b7320334b9};

    static const int compression_ratio = 27;

    // Compute base index.
    int cache_index = (k - float_info<double>::min_k) / compression_ratio;
    int kb = cache_index * compression_ratio + float_info<double>::min_k;
    int offset = k - kb;

    // Get base cache.
    uint128_fallback base_cache = pow10_significands[cache_index];
    if (offset == 0) return base_cache;

    // Compute the required amount of bit-shift.
    int alpha = floor_log2_pow10(kb + offset) - floor_log2_pow10(kb) - offset;
    FMT_ASSERT(alpha > 0 && alpha < 64, "shifting error detected");

    // Try to recover the real cache.
    uint64_t pow5 = powers_of_5_64[offset];
    uint128_fallback recovered_cache = umul128(base_cache.high(), pow5);
    uint128_fallback middle_low = umul128(base_cache.low(), pow5);

    recovered_cache += middle_low.high();

    uint64_t high_to_middle = recovered_cache.high() << (64 - alpha);
    uint64_t middle_to_low = recovered_cache.low() << (64 - alpha);

    recovered_cache =
        uint128_fallback{(recovered_cache.low() >> alpha) | high_to_middle,
                         ((middle_low.low() >> alpha) | middle_to_low)};
    FMT_ASSERT(recovered_cache.low() + 1 != 0, "");
    return {recovered_cache.high(), recovered_cache.low() + 1};
#endif
  }

  struct compute_mul_result {
    carrier_uint result;
    bool is_integer;
  };
  struct compute_mul_parity_result {
    bool parity;
    bool is_integer;
  };

  static auto compute_mul(carrier_uint u,
                          const cache_entry_type& cache) noexcept
      -> compute_mul_result {
    auto r = umul192_upper128(u, cache);
    return {r.high(), r.low() == 0};
  }

  static auto compute_delta(const cache_entry_type& cache, int beta) noexcept
      -> uint32_t {
    return static_cast<uint32_t>(cache.high() >> (64 - 1 - beta));
  }

  static auto compute_mul_parity(carrier_uint two_f,
                                 const cache_entry_type& cache,
                                 int beta) noexcept
      -> compute_mul_parity_result {
    FMT_ASSERT(beta >= 1, "");
    FMT_ASSERT(beta < 64, "");

    auto r = umul192_lower128(two_f, cache);
    return {((r.high() >> (64 - beta)) & 1) != 0,
            ((r.high() << beta) | (r.low() >> (64 - beta))) == 0};
  }

  static auto compute_left_endpoint_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return (cache.high() -
            (cache.high() >> (num_significand_bits<double>() + 2))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static auto compute_right_endpoint_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return (cache.high() +
            (cache.high() >> (num_significand_bits<double>() + 1))) >>
           (64 - num_significand_bits<double>() - 1 - beta);
  }

  static auto compute_round_up_for_shorter_interval_case(
      const cache_entry_type& cache, int beta) noexcept -> carrier_uint {
    return ((cache.high() >> (64 - num_significand_bits<double>() - 2 - beta)) +
            1) /
           2;
  }
};

FMT_FUNC auto get_cached_power(int k) noexcept -> uint128_fallback {
  return cache_accessor<double>::get_cached_power(k);
}

// Various integer checks
template <typename T>
auto is_left_endpoint_integer_shorter_interval(int exponent) noexcept -> bool {
  const int case_shorter_interval_left_endpoint_lower_threshold = 2;
  const int case_shorter_interval_left_endpoint_upper_threshold = 3;
  return exponent >= case_shorter_interval_left_endpoint_lower_threshold &&
         exponent <= case_shorter_interval_left_endpoint_upper_threshold;
}

// Remove trailing zeros from n and return the number of zeros removed (float)
FMT_INLINE int remove_trailing_zeros(uint32_t& n, int s = 0) noexcept {
  FMT_ASSERT(n != 0, "");
  // Modular inverse of 5 (mod 2^32): (mod_inv_5 * 5) mod 2^32 = 1.
  constexpr uint32_t mod_inv_5 = 0xcccccccd;
  constexpr uint32_t mod_inv_25 = 0xc28f5c29;  // = mod_inv_5 * mod_inv_5

  while (true) {
    auto q = rotr(n * mod_inv_25, 2);
    if (q > max_value<uint32_t>() / 100) break;
    n = q;
    s += 2;
  }
  auto q = rotr(n * mod_inv_5, 1);
  if (q <= max_value<uint32_t>() / 10) {
    n = q;
    s |= 1;
  }
  return s;
}

// Removes trailing zeros and returns the number of zeros removed (double)
FMT_INLINE int remove_trailing_zeros(uint64_t& n) noexcept {
  FMT_ASSERT(n != 0, "");

  // This magic number is ceil(2^90 / 10^8).
  constexpr uint64_t magic_number = 12379400392853802749ull;
  auto nm = umul128(n, magic_number);

  // Is n is divisible by 10^8?
  if ((nm.high() & ((1ull << (90 - 64)) - 1)) == 0 && nm.low() < magic_number) {
    // If yes, work with the quotient...
    auto n32 = static_cast<uint32_t>(nm.high() >> (90 - 64));
    // ... and use the 32 bit variant of the function
    int s = remove_trailing_zeros(n32, 8);
    n = n32;
    return s;
  }

  // If n is not divisible by 10^8, work with n itself.
  constexpr uint64_t mod_inv_5 = 0xcccccccccccccccd;
  constexpr uint64_t mod_inv_25 = 0x8f5c28f5c28f5c29;  // mod_inv_5 * mod_inv_5

  int s = 0;
  while (true) {
    auto q = rotr(n * mod_inv_25, 2);
    if (q > max_value<uint64_t>() / 100) break;
    n = q;
    s += 2;
  }
  auto q = rotr(n * mod_inv_5, 1);
  if (q <= max_value<uint64_t>() / 10) {
    n = q;
    s |= 1;
  }

  return s;
}

// The main algorithm for shorter interval case
template <typename T>
FMT_INLINE decimal_fp<T> shorter_interval_case(int exponent) noexcept {
  decimal_fp<T> ret_value;
  // Compute k and beta
  const int minus_k = floor_log10_pow2_minus_log10_4_over_3(exponent);
  const int beta = exponent + floor_log2_pow10(-minus_k);

  // Compute xi and zi
  using cache_entry_type = typename cache_accessor<T>::cache_entry_type;
  const cache_entry_type cache = cache_accessor<T>::get_cached_power(-minus_k);

  auto xi = cache_accessor<T>::compute_left_endpoint_for_shorter_interval_case(
      cache, beta);
  auto zi = cache_accessor<T>::compute_right_endpoint_for_shorter_interval_case(
      cache, beta);

  // If the left endpoint is not an integer, increase it
  if (!is_left_endpoint_integer_shorter_interval<T>(exponent)) ++xi;

  // Try bigger divisor
  ret_value.significand = zi / 10;

  // If succeed, remove trailing zeros if necessary and return
  if (ret_value.significand * 10 >= xi) {
    ret_value.exponent = minus_k + 1;
    ret_value.exponent += remove_trailing_zeros(ret_value.significand);
    return ret_value;
  }

  // Otherwise, compute the round-up of y
  ret_value.significand =
      cache_accessor<T>::compute_round_up_for_shorter_interval_case(cache,
                                                                    beta);
  ret_value.exponent = minus_k;

  // When tie occurs, choose one of them according to the rule
  if (exponent >= float_info<T>::shorter_interval_tie_lower_threshold &&
      exponent <= float_info<T>::shorter_interval_tie_upper_threshold) {
    ret_value.significand = ret_value.significand % 2 == 0
                                ? ret_value.significand
                                : ret_value.significand - 1;
  } else if (ret_value.significand < xi) {
    ++ret_value.significand;
  }
  return ret_value;
}

template <typename T> auto to_decimal(T x) noexcept -> decimal_fp<T> {
  // Step 1: integer promotion & Schubfach multiplier calculation.

  using carrier_uint = typename float_info<T>::carrier_uint;
  using cache_entry_type = typename cache_accessor<T>::cache_entry_type;
  auto br = bit_cast<carrier_uint>(x);

  // Extract significand bits and exponent bits.
  const carrier_uint significand_mask =
      (static_cast<carrier_uint>(1) << num_significand_bits<T>()) - 1;
  carrier_uint significand = (br & significand_mask);
  int exponent =
      static_cast<int>((br & exponent_mask<T>()) >> num_significand_bits<T>());

  if (exponent != 0) {  // Check if normal.
    exponent -= exponent_bias<T>() + num_significand_bits<T>();

    // Shorter interval case; proceed like Schubfach.
    // In fact, when exponent == 1 and significand == 0, the interval is
    // regular. However, it can be shown that the end-results are anyway same.
    if (significand == 0) return shorter_interval_case<T>(exponent);

    significand |= (static_cast<carrier_uint>(1) << num_significand_bits<T>());
  } else {
    // Subnormal case; the interval is always regular.
    if (significand == 0) return {0, 0};
    exponent =
        std::numeric_limits<T>::min_exponent - num_significand_bits<T>() - 1;
  }

  const bool include_left_endpoint = (significand % 2 == 0);
  const bool include_right_endpoint = include_left_endpoint;

  // Compute k and beta.
  const int minus_k = floor_log10_pow2(exponent) - float_info<T>::kappa;
  const cache_entry_type cache = cache_accessor<T>::get_cached_power(-minus_k);
  const int beta = exponent + floor_log2_pow10(-minus_k);

  // Compute zi and deltai.
  // 10^kappa <= deltai < 10^(kappa + 1)
  const uint32_t deltai = cache_accessor<T>::compute_delta(cache, beta);
  const carrier_uint two_fc = significand << 1;

  // For the case of binary32, the result of integer check is not correct for
  // 29711844 * 2^-82
  // = 6.1442653300000000008655037797566933477355632930994033813476... * 10^-18
  // and 29711844 * 2^-81
  // = 1.2288530660000000001731007559513386695471126586198806762695... * 10^-17,
  // and they are the unique counterexamples. However, since 29711844 is even,
  // this does not cause any problem for the endpoints calculations; it can only
  // cause a problem when we need to perform integer check for the center.
  // Fortunately, with these inputs, that branch is never executed, so we are
  // fine.
  const typename cache_accessor<T>::compute_mul_result z_mul =
      cache_accessor<T>::compute_mul((two_fc | 1) << beta, cache);

  // Step 2: Try larger divisor; remove trailing zeros if necessary.

  // Using an upper bound on zi, we might be able to optimize the division
  // better than the compiler; we are computing zi / big_divisor here.
  decimal_fp<T> ret_value;
  ret_value.significand = divide_by_10_to_kappa_plus_1(z_mul.result);
  uint32_t r = static_cast<uint32_t>(z_mul.result - float_info<T>::big_divisor *
                                                        ret_value.significand);

  if (r < deltai) {
    // Exclude the right endpoint if necessary.
    if (r == 0 && (z_mul.is_integer & !include_right_endpoint)) {
      --ret_value.significand;
      r = float_info<T>::big_divisor;
      goto small_divisor_case_label;
    }
  } else if (r > deltai) {
    goto small_divisor_case_label;
  } else {
    // r == deltai; compare fractional parts.
    const typename cache_accessor<T>::compute_mul_parity_result x_mul =
        cache_accessor<T>::compute_mul_parity(two_fc - 1, cache, beta);

    if (!(x_mul.parity | (x_mul.is_integer & include_left_endpoint)))
      goto small_divisor_case_label;
  }
  ret_value.exponent = minus_k + float_info<T>::kappa + 1;

  // We may need to remove trailing zeros.
  ret_value.exponent += remove_trailing_zeros(ret_value.significand);
  return ret_value;

  // Step 3: Find the significand with the smaller divisor.

small_divisor_case_label:
  ret_value.significand *= 10;
  ret_value.exponent = minus_k + float_info<T>::kappa;

  uint32_t dist = r - (deltai / 2) + (float_info<T>::small_divisor / 2);
  const bool approx_y_parity =
      ((dist ^ (float_info<T>::small_divisor / 2)) & 1) != 0;

  // Is dist divisible by 10^kappa?
  const bool divisible_by_small_divisor =
      check_divisibility_and_divide_by_pow10<float_info<T>::kappa>(dist);

  // Add dist / 10^kappa to the significand.
  ret_value.significand += dist;

  if (!divisible_by_small_divisor) return ret_value;

  // Check z^(f) >= epsilon^(f).
  // We have either yi == zi - epsiloni or yi == (zi - epsiloni) - 1,
  // where yi == zi - epsiloni if and only if z^(f) >= epsilon^(f).
  // Since there are only 2 possibilities, we only need to care about the
  // parity. Also, zi and r should have the same parity since the divisor
  // is an even number.
  const auto y_mul = cache_accessor<T>::compute_mul_parity(two_fc, cache, beta);

  // If z^(f) >= epsilon^(f), we might have a tie when z^(f) == epsilon^(f),
  // or equivalently, when y is an integer.
  if (y_mul.parity != approx_y_parity)
    --ret_value.significand;
  else if (y_mul.is_integer & (ret_value.significand % 2 != 0))
    --ret_value.significand;
  return ret_value;
}
}  // namespace dragonbox
}  // namespace detail

template <> struct formatter<detail::bigint> {
  FMT_CONSTEXPR auto parse(format_parse_context& ctx)
      -> format_parse_context::iterator {
    return ctx.begin();
  }

  auto format(const detail::bigint& n, format_context& ctx) const
      -> format_context::iterator {
    auto out = ctx.out();
    bool first = true;
    for (auto i = n.bigits_.size(); i > 0; --i) {
      auto value = n.bigits_[i - 1u];
      if (first) {
        out = fmt::format_to(out, FMT_STRING("{:x}"), value);
        first = false;
        continue;
      }
      out = fmt::format_to(out, FMT_STRING("{:08x}"), value);
    }
    if (n.exp_ > 0)
      out = fmt::format_to(out, FMT_STRING("p{}"),
                           n.exp_ * detail::bigint::bigit_bits);
    return out;
  }
};

FMT_FUNC detail::utf8_to_utf16::utf8_to_utf16(string_view s) {
  for_each_codepoint(s, [this](uint32_t cp, string_view) {
    if (cp == invalid_code_point) FMT_THROW(std::runtime_error("invalid utf8"));
    if (cp <= 0xFFFF) {
      buffer_.push_back(static_cast<wchar_t>(cp));
    } else {
      cp -= 0x10000;
      buffer_.push_back(static_cast<wchar_t>(0xD800 + (cp >> 10)));
      buffer_.push_back(static_cast<wchar_t>(0xDC00 + (cp & 0x3FF)));
    }
    return true;
  });
  buffer_.push_back(0);
}

FMT_FUNC void format_system_error(detail::buffer<char>& out, int error_code,
                                  const char* message) noexcept {
  FMT_TRY {
    auto ec = std::error_code(error_code, std::generic_category());
    detail::write(appender(out), std::system_error(ec, message).what());
    return;
  }
  FMT_CATCH(...) {}
  format_error_code(out, error_code, message);
}

FMT_FUNC void report_system_error(int error_code,
                                  const char* message) noexcept {
  do_report_error(format_system_error, error_code, message);
}

FMT_FUNC auto vformat(string_view fmt, format_args args) -> std::string {
  // Don't optimize the "{}" case to keep the binary size small and because it
  // can be better optimized in fmt::format anyway.
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt, args);
  return to_string(buffer);
}

namespace detail {

FMT_FUNC void vformat_to(buffer<char>& buf, string_view fmt, format_args args,
                         locale_ref loc) {
  auto out = appender(buf);
  if (fmt.size() == 2 && equal2(fmt.data(), "{}"))
    return args.get(0).visit(default_arg_formatter<char>{out});
  parse_format_string(
      fmt, format_handler<char>{parse_context<char>(fmt), {out, args, loc}});
}

template <typename T> struct span {
  T* data;
  size_t size;
};

template <typename F> auto flockfile(F* f) -> decltype(_lock_file(f)) {
  _lock_file(f);
}
template <typename F> auto funlockfile(F* f) -> decltype(_unlock_file(f)) {
  _unlock_file(f);
}

#ifndef getc_unlocked
template <typename F> auto getc_unlocked(F* f) -> decltype(_fgetc_nolock(f)) {
  return _fgetc_nolock(f);
}
#endif

template <typename F = FILE, typename Enable = void>
struct has_flockfile : std::false_type {};

template <typename F>
struct has_flockfile<F, void_t<decltype(flockfile(&std::declval<F&>()))>>
    : std::true_type {};

// A FILE wrapper. F is FILE defined as a template parameter to make system API
// detection work.
template <typename F> class file_base {
 public:
  F* file_;

 public:
  file_base(F* file) : file_(file) {}
  operator F*() const { return file_; }

  // Reads a code unit from the stream.
  auto get() -> int {
    int result = getc_unlocked(file_);
    if (result == EOF && ferror(file_) != 0)
      FMT_THROW(system_error(errno, FMT_STRING("getc failed")));
    return result;
  }

  // Puts the code unit back into the stream buffer.
  void unget(char c) {
    if (ungetc(c, file_) == EOF)
      FMT_THROW(system_error(errno, FMT_STRING("ungetc failed")));
  }

  void flush() { fflush(this->file_); }
};

// A FILE wrapper for glibc.
template <typename F> class glibc_file : public file_base<F> {
 private:
  enum {
    line_buffered = 0x200,  // _IO_LINE_BUF
    unbuffered = 2          // _IO_UNBUFFERED
  };

 public:
  using file_base<F>::file_base;

  auto is_buffered() const -> bool {
    return (this->file_->_flags & unbuffered) == 0;
  }

  void init_buffer() {
    if (this->file_->_IO_write_ptr < this->file_->_IO_write_end) return;
    // Force buffer initialization by placing and removing a char in a buffer.
    putc_unlocked(0, this->file_);
    --this->file_->_IO_write_ptr;
  }

  // Returns the file's read buffer.
  auto get_read_buffer() const -> span<const char> {
    auto ptr = this->file_->_IO_read_ptr;
    return {ptr, to_unsigned(this->file_->_IO_read_end - ptr)};
  }

  // Returns the file's write buffer.
  auto get_write_buffer() const -> span<char> {
    auto ptr = this->file_->_IO_write_ptr;
    return {ptr, to_unsigned(this->file_->_IO_buf_end - ptr)};
  }

  void advance_write_buffer(size_t size) { this->file_->_IO_write_ptr += size; }

  bool needs_flush() const {
    if ((this->file_->_flags & line_buffered) == 0) return false;
    char* end = this->file_->_IO_write_end;
    return memchr(end, '\n', to_unsigned(this->file_->_IO_write_ptr - end));
  }

  void flush() { fflush_unlocked(this->file_); }
};

// A FILE wrapper for Apple's libc.
template <typename F> class apple_file : public file_base<F> {
 private:
  enum {
    line_buffered = 1,  // __SNBF
    unbuffered = 2      // __SLBF
  };

 public:
  using file_base<F>::file_base;

  auto is_buffered() const -> bool {
    return (this->file_->_flags & unbuffered) == 0;
  }

  void init_buffer() {
    if (this->file_->_p) return;
    // Force buffer initialization by placing and removing a char in a buffer.
    putc_unlocked(0, this->file_);
    --this->file_->_p;
    ++this->file_->_w;
  }

  auto get_read_buffer() const -> span<const char> {
    return {reinterpret_cast<char*>(this->file_->_p),
            to_unsigned(this->file_->_r)};
  }

  auto get_write_buffer() const -> span<char> {
    return {reinterpret_cast<char*>(this->file_->_p),
            to_unsigned(this->file_->_bf._base + this->file_->_bf._size -
                        this->file_->_p)};
  }

  void advance_write_buffer(size_t size) {
    this->file_->_p += size;
    this->file_->_w -= size;
  }

  bool needs_flush() const {
    if ((this->file_->_flags & line_buffered) == 0) return false;
    return memchr(this->file_->_p + this->file_->_w, '\n',
                  to_unsigned(-this->file_->_w));
  }
};

// A fallback FILE wrapper.
template <typename F> class fallback_file : public file_base<F> {
 private:
  char next_;  // The next unconsumed character in the buffer.
  bool has_next_ = false;

 public:
  using file_base<F>::file_base;

  auto is_buffered() const -> bool { return false; }
  auto needs_flush() const -> bool { return false; }
  void init_buffer() {}

  auto get_read_buffer() const -> span<const char> {
    return {&next_, has_next_ ? 1u : 0u};
  }

  auto get_write_buffer() const -> span<char> { return {nullptr, 0}; }

  void advance_write_buffer(size_t) {}

  auto get() -> int {
    has_next_ = false;
    return file_base<F>::get();
  }

  void unget(char c) {
    file_base<F>::unget(c);
    next_ = c;
    has_next_ = true;
  }
};

#ifndef FMT_USE_FALLBACK_FILE
#  define FMT_USE_FALLBACK_FILE 0
#endif

template <typename F,
          FMT_ENABLE_IF(sizeof(F::_p) != 0 && !FMT_USE_FALLBACK_FILE)>
auto get_file(F* f, int) -> apple_file<F> {
  return f;
}
template <typename F,
          FMT_ENABLE_IF(sizeof(F::_IO_read_ptr) != 0 && !FMT_USE_FALLBACK_FILE)>
inline auto get_file(F* f, int) -> glibc_file<F> {
  return f;
}

inline auto get_file(FILE* f, ...) -> fallback_file<FILE> { return f; }

using file_ref = decltype(get_file(static_cast<FILE*>(nullptr), 0));

template <typename F = FILE, typename Enable = void>
class file_print_buffer : public buffer<char> {
 public:
  explicit file_print_buffer(F*) : buffer(nullptr, size_t()) {}
};

template <typename F>
class file_print_buffer<F, enable_if_t<has_flockfile<F>::value>>
    : public buffer<char> {
 private:
  file_ref file_;

  static void grow(buffer<char>& base, size_t) {
    auto& self = static_cast<file_print_buffer&>(base);
    self.file_.advance_write_buffer(self.size());
    if (self.file_.get_write_buffer().size == 0) self.file_.flush();
    auto buf = self.file_.get_write_buffer();
    FMT_ASSERT(buf.size > 0, "");
    self.set(buf.data, buf.size);
    self.clear();
  }

 public:
  explicit file_print_buffer(F* f) : buffer(grow, size_t()), file_(f) {
    flockfile(f);
    file_.init_buffer();
    auto buf = file_.get_write_buffer();
    set(buf.data, buf.size);
  }
  ~file_print_buffer() {
    file_.advance_write_buffer(size());
    bool flush = file_.needs_flush();
    F* f = file_;    // Make funlockfile depend on the template parameter F
    funlockfile(f);  // for the system API detection to work.
    if (flush) fflush(file_);
  }
};

#if !defined(_WIN32) || defined(FMT_USE_WRITE_CONSOLE)
FMT_FUNC auto write_console(int, string_view) -> bool { return false; }
#else
using dword = conditional_t<sizeof(long) == 4, unsigned long, unsigned>;
extern "C" __declspec(dllimport) int __stdcall WriteConsoleW(  //
    void*, const void*, dword, dword*, void*);

FMT_FUNC bool write_console(int fd, string_view text) {
  auto u16 = utf8_to_utf16(text);
  return WriteConsoleW(reinterpret_cast<void*>(_get_osfhandle(fd)), u16.c_str(),
                       static_cast<dword>(u16.size()), nullptr, nullptr) != 0;
}
#endif

#ifdef _WIN32
// Print assuming legacy (non-Unicode) encoding.
FMT_FUNC void vprint_mojibake(std::FILE* f, string_view fmt, format_args args,
                              bool newline) {
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt, args);
  if (newline) buffer.push_back('\n');
  fwrite_all(buffer.data(), buffer.size(), f);
}
#endif

FMT_FUNC void print(std::FILE* f, string_view text) {
#if defined(_WIN32) && !defined(FMT_USE_WRITE_CONSOLE)
  int fd = _fileno(f);
  if (_isatty(fd)) {
    std::fflush(f);
    if (write_console(fd, text)) return;
  }
#endif
  fwrite_all(text.data(), text.size(), f);
}
}  // namespace detail

FMT_FUNC void vprint_buffered(std::FILE* f, string_view fmt, format_args args) {
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt, args);
  detail::print(f, {buffer.data(), buffer.size()});
}

FMT_FUNC void vprint(std::FILE* f, string_view fmt, format_args args) {
  if (!detail::file_ref(f).is_buffered() || !detail::has_flockfile<>())
    return vprint_buffered(f, fmt, args);
  auto&& buffer = detail::file_print_buffer<>(f);
  return detail::vformat_to(buffer, fmt, args);
}

FMT_FUNC void vprintln(std::FILE* f, string_view fmt, format_args args) {
  auto buffer = memory_buffer();
  detail::vformat_to(buffer, fmt, args);
  buffer.push_back('\n');
  detail::print(f, {buffer.data(), buffer.size()});
}

FMT_FUNC void vprint(string_view fmt, format_args args) {
  vprint(stdout, fmt, args);
}

namespace detail {

struct singleton {
  unsigned char upper;
  unsigned char lower_count;
};

inline auto is_printable(uint16_t x, const singleton* singletons,
                         size_t singletons_size,
                         const unsigned char* singleton_lowers,
                         const unsigned char* normal, size_t normal_size)
    -> bool {
  auto upper = x >> 8;
  auto lower_start = 0;
  for (size_t i = 0; i < singletons_size; ++i) {
    auto s = singletons[i];
    auto lower_end = lower_start + s.lower_count;
    if (upper < s.upper) break;
    if (upper == s.upper) {
      for (auto j = lower_start; j < lower_end; ++j) {
        if (singleton_lowers[j] == (x & 0xff)) return false;
      }
    }
    lower_start = lower_end;
  }

  auto xsigned = static_cast<int>(x);
  auto current = true;
  for (size_t i = 0; i < normal_size; ++i) {
    auto v = static_cast<int>(normal[i]);
    auto len = (v & 0x80) != 0 ? (v & 0x7f) << 8 | normal[++i] : v;
    xsigned -= len;
    if (xsigned < 0) break;
    current = !current;
  }
  return current;
}

// This code is generated by support/printable.py.
FMT_FUNC auto is_printable(uint32_t cp) -> bool {
  static constexpr singleton singletons0[] = {
      {0x00, 1},  {0x03, 5},  {0x05, 6},  {0x06, 3},  {0x07, 6},  {0x08, 8},
      {0x09, 17}, {0x0a, 28}, {0x0b, 25}, {0x0c, 20}, {0x0d, 16}, {0x0e, 13},
      {0x0f, 4},  {0x10, 3},  {0x12, 18}, {0x13, 9},  {0x16, 1},  {0x17, 5},
      {0x18, 2},  {0x19, 3},  {0x1a, 7},  {0x1c, 2},  {0x1d, 1},  {0x1f, 22},
      {0x20, 3},  {0x2b, 3},  {0x2c, 2},  {0x2d, 11}, {0x2e, 1},  {0x30, 3},
      {0x31, 2},  {0x32, 1},  {0xa7, 2},  {0xa9, 2},  {0xaa, 4},  {0xab, 8},
      {0xfa, 2},  {0xfb, 5},  {0xfd, 4},  {0xfe, 3},  {0xff, 9},
  };
  static constexpr unsigned char singletons0_lower[] = {
      0xad, 0x78, 0x79, 0x8b, 0x8d, 0xa2, 0x30, 0x57, 0x58, 0x8b, 0x8c, 0x90,
      0x1c, 0x1d, 0xdd, 0x0e, 0x0f, 0x4b, 0x4c, 0xfb, 0xfc, 0x2e, 0x2f, 0x3f,
      0x5c, 0x5d, 0x5f, 0xb5, 0xe2, 0x84, 0x8d, 0x8e, 0x91, 0x92, 0xa9, 0xb1,
      0xba, 0xbb, 0xc5, 0xc6, 0xc9, 0xca, 0xde, 0xe4, 0xe5, 0xff, 0x00, 0x04,
      0x11, 0x12, 0x29, 0x31, 0x34, 0x37, 0x3a, 0x3b, 0x3d, 0x49, 0x4a, 0x5d,
      0x84, 0x8e, 0x92, 0xa9, 0xb1, 0xb4, 0xba, 0xbb, 0xc6, 0xca, 0xce, 0xcf,
      0xe4, 0xe5, 0x00, 0x04, 0x0d, 0x0e, 0x11, 0x12, 0x29, 0x31, 0x34, 0x3a,
      0x3b, 0x45, 0x46, 0x49, 0x4a, 0x5e, 0x64, 0x65, 0x84, 0x91, 0x9b, 0x9d,
      0xc9, 0xce, 0xcf, 0x0d, 0x11, 0x29, 0x45, 0x49, 0x57, 0x64, 0x65, 0x8d,
      0x91, 0xa9, 0xb4, 0xba, 0xbb, 0xc5, 0xc9, 0xdf, 0xe4, 0xe5, 0xf0, 0x0d,
      0x11, 0x45, 0x49, 0x64, 0x65, 0x80, 0x84, 0xb2, 0xbc, 0xbe, 0xbf, 0xd5,
      0xd7, 0xf0, 0xf1, 0x83, 0x85, 0x8b, 0xa4, 0xa6, 0xbe, 0xbf, 0xc5, 0xc7,
      0xce, 0xcf, 0xda, 0xdb, 0x48, 0x98, 0xbd, 0xcd, 0xc6, 0xce, 0xcf, 0x49,
      0x4e, 0x4f, 0x57, 0x59, 0x5e, 0x5f, 0x89, 0x8e, 0x8f, 0xb1, 0xb6, 0xb7,
      0xbf, 0xc1, 0xc6, 0xc7, 0xd7, 0x11, 0x16, 0x17, 0x5b, 0x5c, 0xf6, 0xf7,
      0xfe, 0xff, 0x80, 0x0d, 0x6d, 0x71, 0xde, 0xdf, 0x0e, 0x0f, 0x1f, 0x6e,
      0x6f, 0x1c, 0x1d, 0x5f, 0x7d, 0x7e, 0xae, 0xaf, 0xbb, 0xbc, 0xfa, 0x16,
      0x17, 0x1e, 0x1f, 0x46, 0x47, 0x4e, 0x4f, 0x58, 0x5a, 0x5c, 0x5e, 0x7e,
      0x7f, 0xb5, 0xc5, 0xd4, 0xd5, 0xdc, 0xf0, 0xf1, 0xf5, 0x72, 0x73, 0x8f,
      0x74, 0x75, 0x96, 0x2f, 0x5f, 0x26, 0x2e, 0x2f, 0xa7, 0xaf, 0xb7, 0xbf,
      0xc7, 0xcf, 0xd7, 0xdf, 0x9a, 0x40, 0x97, 0x98, 0x30, 0x8f, 0x1f, 0xc0,
      0xc1, 0xce, 0xff, 0x4e, 0x4f, 0x5a, 0x5b, 0x07, 0x08, 0x0f, 0x10, 0x27,
      0x2f, 0xee, 0xef, 0x6e, 0x6f, 0x37, 0x3d, 0x3f, 0x42, 0x45, 0x90, 0x91,
      0xfe, 0xff, 0x53, 0x67, 0x75, 0xc8, 0xc9, 0xd0, 0xd1, 0xd8, 0xd9, 0xe7,
      0xfe, 0xff,
  };
  static constexpr singleton singletons1[] = {
      {0x00, 6},  {0x01, 1}, {0x03, 1},  {0x04, 2}, {0x08, 8},  {0x09, 2},
      {0x0a, 5},  {0x0b, 2}, {0x0e, 4},  {0x10, 1}, {0x11, 2},  {0x12, 5},
      {0x13, 17}, {0x14, 1}, {0x15, 2},  {0x17, 2}, {0x19, 13}, {0x1c, 5},
      {0x1d, 8},  {0x24, 1}, {0x6a, 3},  {0x6b, 2}, {0xbc, 2},  {0xd1, 2},
      {0xd4, 12}, {0xd5, 9}, {0xd6, 2},  {0xd7, 2}, {0xda, 1},  {0xe0, 5},
      {0xe1, 2},  {0xe8, 2}, {0xee, 32}, {0xf0, 4}, {0xf8, 2},  {0xf9, 2},
      {0xfa, 2},  {0xfb, 1},
  };
  static constexpr unsigned char singletons1_lower[] = {
      0x0c, 0x27, 0x3b, 0x3e, 0x4e, 0x4f, 0x8f, 0x9e, 0x9e, 0x9f, 0x06, 0x07,
      0x09, 0x36, 0x3d, 0x3e, 0x56, 0xf3, 0xd0, 0xd1, 0x04, 0x14, 0x18, 0x36,
      0x37, 0x56, 0x57, 0x7f, 0xaa, 0xae, 0xaf, 0xbd, 0x35, 0xe0, 0x12, 0x87,
      0x89, 0x8e, 0x9e, 0x04, 0x0d, 0x0e, 0x11, 0x12, 0x29, 0x31, 0x34, 0x3a,
      0x45, 0x46, 0x49, 0x4a, 0x4e, 0x4f, 0x64, 0x65, 0x5c, 0xb6, 0xb7, 0x1b,
      0x1c, 0x07, 0x08, 0x0a, 0x0b, 0x14, 0x17, 0x36, 0x39, 0x3a, 0xa8, 0xa9,
      0xd8, 0xd9, 0x09, 0x37, 0x90, 0x91, 0xa8, 0x07, 0x0a, 0x3b, 0x3e, 0x66,
      0x69, 0x8f, 0x92, 0x6f, 0x5f, 0xee, 0xef, 0x5a, 0x62, 0x9a, 0x9b, 0x27,
      0x28, 0x55, 0x9d, 0xa0, 0xa1, 0xa3, 0xa4, 0xa7, 0xa8, 0xad, 0xba, 0xbc,
      0xc4, 0x06, 0x0b, 0x0c, 0x15, 0x1d, 0x3a, 0x3f, 0x45, 0x51, 0xa6, 0xa7,
      0xcc, 0xcd, 0xa0, 0x07, 0x19, 0x1a, 0x22, 0x25, 0x3e, 0x3f, 0xc5, 0xc6,
      0x04, 0x20, 0x23, 0x25, 0x26, 0x28, 0x33, 0x38, 0x3a, 0x48, 0x4a, 0x4c,
      0x50, 0x53, 0x55, 0x56, 0x58, 0x5a, 0x5c, 0x5e, 0x60, 0x63, 0x65, 0x66,
      0x6b, 0x73, 0x78, 0x7d, 0x7f, 0x8a, 0xa4, 0xaa, 0xaf, 0xb0, 0xc0, 0xd0,
      0xae, 0xaf, 0x79, 0xcc, 0x6e, 0x6f, 0x93,
  };
  static constexpr unsigned char normal0[] = {
      0x00, 0x20, 0x5f, 0x22, 0x82, 0xdf, 0x04, 0x82, 0x44, 0x08, 0x1b, 0x04,
      0x06, 0x11, 0x81, 0xac, 0x0e, 0x80, 0xab, 0x35, 0x28, 0x0b, 0x80, 0xe0,
      0x03, 0x19, 0x08, 0x01, 0x04, 0x2f, 0x04, 0x34, 0x04, 0x07, 0x03, 0x01,
      0x07, 0x06, 0x07, 0x11, 0x0a, 0x50, 0x0f, 0x12, 0x07, 0x55, 0x07, 0x03,
      0x04, 0x1c, 0x0a, 0x09, 0x03, 0x08, 0x03, 0x07, 0x03, 0x02, 0x03, 0x03,
      0x03, 0x0c, 0x04, 0x05, 0x03, 0x0b, 0x06, 0x01, 0x0e, 0x15, 0x05, 0x3a,
      0x03, 0x11, 0x07, 0x06, 0x05, 0x10, 0x07, 0x57, 0x07, 0x02, 0x07, 0x15,
      0x0d, 0x50, 0x04, 0x43, 0x03, 0x2d, 0x03, 0x01, 0x04, 0x11, 0x06, 0x0f,
      0x0c, 0x3a, 0x04, 0x1d, 0x25, 0x5f, 0x20, 0x6d, 0x04, 0x6a, 0x25, 0x80,
      0xc8, 0x05, 0x82, 0xb0, 0x03, 0x1a, 0x06, 0x82, 0xfd, 0x03, 0x59, 0x07,
      0x15, 0x0b, 0x17, 0x09, 0x14, 0x0c, 0x14, 0x0c, 0x6a, 0x06, 0x0a, 0x06,
      0x1a, 0x06, 0x59, 0x07, 0x2b, 0x05, 0x46, 0x0a, 0x2c, 0x04, 0x0c, 0x04,
      0x01, 0x03, 0x31, 0x0b, 0x2c, 0x04, 0x1a, 0x06, 0x0b, 0x03, 0x80, 0xac,
      0x06, 0x0a, 0x06, 0x21, 0x3f, 0x4c, 0x04, 0x2d, 0x03, 0x74, 0x08, 0x3c,
      0x03, 0x0f, 0x03, 0x3c, 0x07, 0x38, 0x08, 0x2b, 0x05, 0x82, 0xff, 0x11,
      0x18, 0x08, 0x2f, 0x11, 0x2d, 0x03, 0x20, 0x10, 0x21, 0x0f, 0x80, 0x8c,
      0x04, 0x82, 0x97, 0x19, 0x0b, 0x15, 0x88, 0x94, 0x05, 0x2f, 0x05, 0x3b,
      0x07, 0x02, 0x0e, 0x18, 0x09, 0x80, 0xb3, 0x2d, 0x74, 0x0c, 0x80, 0xd6,
      0x1a, 0x0c, 0x05, 0x80, 0xff, 0x05, 0x80, 0xdf, 0x0c, 0xee, 0x0d, 0x03,
      0x84, 0x8d, 0x03, 0x37, 0x09, 0x81, 0x5c, 0x14, 0x80, 0xb8, 0x08, 0x80,
      0xcb, 0x2a, 0x38, 0x03, 0x0a, 0x06, 0x38, 0x08, 0x46, 0x08, 0x0c, 0x06,
      0x74, 0x0b, 0x1e, 0x03, 0x5a, 0x04, 0x59, 0x09, 0x80, 0x83, 0x18, 0x1c,
      0x0a, 0x16, 0x09, 0x4c, 0x04, 0x80, 0x8a, 0x06, 0xab, 0xa4, 0x0c, 0x17,
      0x04, 0x31, 0xa1, 0x04, 0x81, 0xda, 0x26, 0x07, 0x0c, 0x05, 0x05, 0x80,
      0xa5, 0x11, 0x81, 0x6d, 0x10, 0x78, 0x28, 0x2a, 0x06, 0x4c, 0x04, 0x80,
      0x8d, 0x04, 0x80, 0xbe, 0x03, 0x1b, 0x03, 0x0f, 0x0d,
  };
  static constexpr unsigned char normal1[] = {
      0x5e, 0x22, 0x7b, 0x05, 0x03, 0x04, 0x2d, 0x03, 0x66, 0x03, 0x01, 0x2f,
      0x2e, 0x80, 0x82, 0x1d, 0x03, 0x31, 0x0f, 0x1c, 0x04, 0x24, 0x09, 0x1e,
      0x05, 0x2b, 0x05, 0x44, 0x04, 0x0e, 0x2a, 0x80, 0xaa, 0x06, 0x24, 0x04,
      0x24, 0x04, 0x28, 0x08, 0x34, 0x0b, 0x01, 0x80, 0x90, 0x81, 0x37, 0x09,
      0x16, 0x0a, 0x08, 0x80, 0x98, 0x39, 0x03, 0x63, 0x08, 0x09, 0x30, 0x16,
      0x05, 0x21, 0x03, 0x1b, 0x05, 0x01, 0x40, 0x38, 0x04, 0x4b, 0x05, 0x2f,
      0x04, 0x0a, 0x07, 0x09, 0x07, 0x40, 0x20, 0x27, 0x04, 0x0c, 0x09, 0x36,
      0x03, 0x3a, 0x05, 0x1a, 0x07, 0x04, 0x0c, 0x07, 0x50, 0x49, 0x37, 0x33,
      0x0d, 0x33, 0x07, 0x2e, 0x08, 0x0a, 0x81, 0x26, 0x52, 0x4e, 0x28, 0x08,
      0x2a, 0x56, 0x1c, 0x14, 0x17, 0x09, 0x4e, 0x04, 0x1e, 0x0f, 0x43, 0x0e,
      0x19, 0x07, 0x0a, 0x06, 0x48, 0x08, 0x27, 0x09, 0x75, 0x0b, 0x3f, 0x41,
      0x2a, 0x06, 0x3b, 0x05, 0x0a, 0x06, 0x51, 0x06, 0x01, 0x05, 0x10, 0x03,
      0x05, 0x80, 0x8b, 0x62, 0x1e, 0x48, 0x08, 0x0a, 0x80, 0xa6, 0x5e, 0x22,
      0x45, 0x0b, 0x0a, 0x06, 0x0d, 0x13, 0x39, 0x07, 0x0a, 0x36, 0x2c, 0x04,
      0x10, 0x80, 0xc0, 0x3c, 0x64, 0x53, 0x0c, 0x48, 0x09, 0x0a, 0x46, 0x45,
      0x1b, 0x48, 0x08, 0x53, 0x1d, 0x39, 0x81, 0x07, 0x46, 0x0a, 0x1d, 0x03,
      0x47, 0x49, 0x37, 0x03, 0x0e, 0x08, 0x0a, 0x06, 0x39, 0x07, 0x0a, 0x81,
      0x36, 0x19, 0x80, 0xb7, 0x01, 0x0f, 0x32, 0x0d, 0x83, 0x9b, 0x66, 0x75,
      0x0b, 0x80, 0xc4, 0x8a, 0xbc, 0x84, 0x2f, 0x8f, 0xd1, 0x82, 0x47, 0xa1,
      0xb9, 0x82, 0x39, 0x07, 0x2a, 0x04, 0x02, 0x60, 0x26, 0x0a, 0x46, 0x0a,
      0x28, 0x05, 0x13, 0x82, 0xb0, 0x5b, 0x65, 0x4b, 0x04, 0x39, 0x07, 0x11,
      0x40, 0x05, 0x0b, 0x02, 0x0e, 0x97, 0xf8, 0x08, 0x84, 0xd6, 0x2a, 0x09,
      0xa2, 0xf7, 0x81, 0x1f, 0x31, 0x03, 0x11, 0x04, 0x08, 0x81, 0x8c, 0x89,
      0x04, 0x6b, 0x05, 0x0d, 0x03, 0x09, 0x07, 0x10, 0x93, 0x60, 0x80, 0xf6,
      0x0a, 0x73, 0x08, 0x6e, 0x17, 0x46, 0x80, 0x9a, 0x14, 0x0c, 0x57, 0x09,
      0x19, 0x80, 0x87, 0x81, 0x47, 0x03, 0x85, 0x42, 0x0f, 0x15, 0x85, 0x50,
      0x2b, 0x80, 0xd5, 0x2d, 0x03, 0x1a, 0x04, 0x02, 0x81, 0x70, 0x3a, 0x05,
      0x01, 0x85, 0x00, 0x80, 0xd7, 0x29, 0x4c, 0x04, 0x0a, 0x04, 0x02, 0x83,
      0x11, 0x44, 0x4c, 0x3d, 0x80, 0xc2, 0x3c, 0x06, 0x01, 0x04, 0x55, 0x05,
      0x1b, 0x34, 0x02, 0x81, 0x0e, 0x2c, 0x04, 0x64, 0x0c, 0x56, 0x0a, 0x80,
      0xae, 0x38, 0x1d, 0x0d, 0x2c, 0x04, 0x09, 0x07, 0x02, 0x0e, 0x06, 0x80,
      0x9a, 0x83, 0xd8, 0x08, 0x0d, 0x03, 0x0d, 0x03, 0x74, 0x0c, 0x59, 0x07,
      0x0c, 0x14, 0x0c, 0x04, 0x38, 0x08, 0x0a, 0x06, 0x28, 0x08, 0x22, 0x4e,
      0x81, 0x54, 0x0c, 0x15, 0x03, 0x03, 0x05, 0x07, 0x09, 0x19, 0x07, 0x07,
      0x09, 0x03, 0x0d, 0x07, 0x29, 0x80, 0xcb, 0x25, 0x0a, 0x84, 0x06,
  };
  auto lower = static_cast<uint16_t>(cp);
  if (cp < 0x10000) {
    return is_printable(lower, singletons0,
                        sizeof(singletons0) / sizeof(*singletons0),
                        singletons0_lower, normal0, sizeof(normal0));
  }
  if (cp < 0x20000) {
    return is_printable(lower, singletons1,
                        sizeof(singletons1) / sizeof(*singletons1),
                        singletons1_lower, normal1, sizeof(normal1));
  }
  if (0x2a6de <= cp && cp < 0x2a700) return false;
  if (0x2b735 <= cp && cp < 0x2b740) return false;
  if (0x2b81e <= cp && cp < 0x2b820) return false;
  if (0x2cea2 <= cp && cp < 0x2ceb0) return false;
  if (0x2ebe1 <= cp && cp < 0x2f800) return false;
  if (0x2fa1e <= cp && cp < 0x30000) return false;
  if (0x3134b <= cp && cp < 0xe0100) return false;
  if (0xe01f0 <= cp && cp < 0x110000) return false;
  return cp < 0x110000;
}

}  // namespace detail

FMT_END_NAMESPACE

#endif  // FMT_FORMAT_INL_H_
