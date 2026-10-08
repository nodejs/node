// Formatting library for C++ - chrono support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_CHRONO_H_
#define FMT_CHRONO_H_

#ifndef FMT_MODULE
#  include <algorithm>
#  include <chrono>
#  include <cmath>    // std::isfinite
#  include <cstring>  // std::memcpy
#  include <ctime>
#  include <iterator>
#  include <locale>
#  include <ostream>
#  include <type_traits>
#endif

#include "format.h"

FMT_BEGIN_NAMESPACE

// Enable safe chrono durations, unless explicitly disabled.
#ifndef FMT_SAFE_DURATION_CAST
#  define FMT_SAFE_DURATION_CAST 1
#endif
#if FMT_SAFE_DURATION_CAST

// For conversion between std::chrono::durations without undefined
// behaviour or erroneous results.
// This is a stripped down version of duration_cast, for inclusion in fmt.
// See https://github.com/pauldreik/safe_duration_cast
//
// Copyright Paul Dreik 2019
namespace safe_duration_cast {

// DEPRECATED!
template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value &&
                        std::numeric_limits<From>::is_signed ==
                            std::numeric_limits<To>::is_signed)>
FMT_CONSTEXPR auto lossless_integral_conversion(const From from, int& ec)
    -> To {
  ec = 0;
  using F = std::numeric_limits<From>;
  using T = std::numeric_limits<To>;
  static_assert(F::is_integer, "From must be integral");
  static_assert(T::is_integer, "To must be integral");

  // A and B are both signed, or both unsigned.
  if (detail::const_check(F::digits <= T::digits)) {
    // From fits in To without any problem.
  } else {
    // From does not always fit in To, resort to a dynamic check.
    if (from < (T::min)() || from > (T::max)()) {
      // outside range.
      ec = 1;
      return {};
    }
  }
  return static_cast<To>(from);
}

/// Converts From to To, without loss. If the dynamic value of from
/// can't be converted to To without loss, ec is set.
template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value &&
                        std::numeric_limits<From>::is_signed !=
                            std::numeric_limits<To>::is_signed)>
FMT_CONSTEXPR auto lossless_integral_conversion(const From from, int& ec)
    -> To {
  ec = 0;
  using F = std::numeric_limits<From>;
  using T = std::numeric_limits<To>;
  static_assert(F::is_integer, "From must be integral");
  static_assert(T::is_integer, "To must be integral");

  if (detail::const_check(F::is_signed && !T::is_signed)) {
    // From may be negative, not allowed!
    if (fmt::detail::is_negative(from)) {
      ec = 1;
      return {};
    }
    // From is positive. Can it always fit in To?
    if (detail::const_check(F::digits > T::digits) &&
        from > static_cast<From>(detail::max_value<To>())) {
      ec = 1;
      return {};
    }
  }

  if (detail::const_check(!F::is_signed && T::is_signed &&
                          F::digits >= T::digits) &&
      from > static_cast<From>(detail::max_value<To>())) {
    ec = 1;
    return {};
  }
  return static_cast<To>(from);  // Lossless conversion.
}

template <typename To, typename From,
          FMT_ENABLE_IF(std::is_same<From, To>::value)>
FMT_CONSTEXPR auto lossless_integral_conversion(const From from, int& ec)
    -> To {
  ec = 0;
  return from;
}  // function

// clang-format off
/**
 * converts From to To if possible, otherwise ec is set.
 *
 * input                            |    output
 * ---------------------------------|---------------
 * NaN                              | NaN
 * Inf                              | Inf
 * normal, fits in output           | converted (possibly lossy)
 * normal, does not fit in output   | ec is set
 * subnormal                        | best effort
 * -Inf                             | -Inf
 */
// clang-format on
template <typename To, typename From,
          FMT_ENABLE_IF(!std::is_same<From, To>::value)>
FMT_CONSTEXPR auto safe_float_conversion(const From from, int& ec) -> To {
  ec = 0;
  using T = std::numeric_limits<To>;
  static_assert(std::is_floating_point<From>::value, "From must be floating");
  static_assert(std::is_floating_point<To>::value, "To must be floating");

  // catch the only happy case
  if (std::isfinite(from)) {
    if (from >= T::lowest() && from <= (T::max)()) {
      return static_cast<To>(from);
    }
    // not within range.
    ec = 1;
    return {};
  }

  // nan and inf will be preserved
  return static_cast<To>(from);
}  // function

template <typename To, typename From,
          FMT_ENABLE_IF(std::is_same<From, To>::value)>
FMT_CONSTEXPR auto safe_float_conversion(const From from, int& ec) -> To {
  ec = 0;
  static_assert(std::is_floating_point<From>::value, "From must be floating");
  return from;
}

/// Safe duration_cast between floating point durations
template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(std::is_floating_point<FromRep>::value),
          FMT_ENABLE_IF(std::is_floating_point<typename To::rep>::value)>
auto safe_duration_cast(std::chrono::duration<FromRep, FromPeriod> from,
                        int& ec) -> To {
  using From = std::chrono::duration<FromRep, FromPeriod>;
  ec = 0;

  // the basic idea is that we need to convert from count() in the from type
  // to count() in the To type, by multiplying it with this:
  struct Factor
      : std::ratio_divide<typename From::period, typename To::period> {};

  static_assert(Factor::num > 0, "num must be positive");
  static_assert(Factor::den > 0, "den must be positive");

  // the conversion is like this: multiply from.count() with Factor::num
  // /Factor::den and convert it to To::rep, all this without
  // overflow/underflow. let's start by finding a suitable type that can hold
  // both To, From and Factor::num
  using IntermediateRep =
      typename std::common_type<typename From::rep, typename To::rep,
                                decltype(Factor::num)>::type;

  // force conversion of From::rep -> IntermediateRep to be safe,
  // even if it will never happen be narrowing in this context.
  IntermediateRep count =
      safe_float_conversion<IntermediateRep>(from.count(), ec);
  if (ec) {
    return {};
  }

  // multiply with Factor::num without overflow or underflow
  if (detail::const_check(Factor::num != 1)) {
    constexpr auto max1 = detail::max_value<IntermediateRep>() /
                          static_cast<IntermediateRep>(Factor::num);
    if (count > max1) {
      ec = 1;
      return {};
    }
    constexpr auto min1 = std::numeric_limits<IntermediateRep>::lowest() /
                          static_cast<IntermediateRep>(Factor::num);
    if (count < min1) {
      ec = 1;
      return {};
    }
    count *= static_cast<IntermediateRep>(Factor::num);
  }

  // this can't go wrong, right? den>0 is checked earlier.
  if (detail::const_check(Factor::den != 1)) {
    using common_t = typename std::common_type<IntermediateRep, intmax_t>::type;
    count /= static_cast<common_t>(Factor::den);
  }

  // convert to the to type, safely
  using ToRep = typename To::rep;

  const ToRep tocount = safe_float_conversion<ToRep>(count, ec);
  if (ec) {
    return {};
  }
  return To{tocount};
}
}  // namespace safe_duration_cast
#endif

namespace detail {

// Check if std::chrono::utc_time is available.
#ifdef FMT_USE_UTC_TIME
// Use the provided definition.
#elif defined(__cpp_lib_chrono)
#  define FMT_USE_UTC_TIME (__cpp_lib_chrono >= 201907L)
#else
#  define FMT_USE_UTC_TIME 0
#endif
#if FMT_USE_UTC_TIME
using utc_clock = std::chrono::utc_clock;
#else
struct utc_clock {
  template <typename T> void to_sys(T);
};
#endif

// Check if std::chrono::local_time is available.
#ifdef FMT_USE_LOCAL_TIME
// Use the provided definition.
#elif defined(__cpp_lib_chrono)
#  define FMT_USE_LOCAL_TIME (__cpp_lib_chrono >= 201907L)
#else
#  define FMT_USE_LOCAL_TIME 0
#endif
#if FMT_USE_LOCAL_TIME
using local_t = std::chrono::local_t;
#else
struct local_t {};
#endif

}  // namespace detail

template <typename Duration>
using sys_time = std::chrono::time_point<std::chrono::system_clock, Duration>;

template <typename Duration>
using utc_time = std::chrono::time_point<detail::utc_clock, Duration>;

template <class Duration>
using local_time = std::chrono::time_point<detail::local_t, Duration>;

namespace detail {

// Prevents expansion of a preceding token as a function-style macro.
// Usage: f FMT_NOMACRO()
#define FMT_NOMACRO

template <typename T = void> struct null {};
inline auto gmtime_r(...) -> null<> { return null<>(); }
inline auto gmtime_s(...) -> null<> { return null<>(); }

// It is defined here and not in ostream.h because the latter has expensive
// includes.
template <typename StreamBuf> class formatbuf : public StreamBuf {
 private:
  using char_type = typename StreamBuf::char_type;
  using streamsize = decltype(std::declval<StreamBuf>().sputn(nullptr, 0));
  using int_type = typename StreamBuf::int_type;
  using traits_type = typename StreamBuf::traits_type;

  buffer<char_type>& buffer_;

 public:
  explicit formatbuf(buffer<char_type>& buf) : buffer_(buf) {}

 protected:
  // The put area is always empty. This makes the implementation simpler and has
  // the advantage that the streambuf and the buffer are always in sync and
  // sputc never writes into uninitialized memory. A disadvantage is that each
  // call to sputc always results in a (virtual) call to overflow. There is no
  // disadvantage here for sputn since this always results in a call to xsputn.

  auto overflow(int_type ch) -> int_type override {
    if (!traits_type::eq_int_type(ch, traits_type::eof()))
      buffer_.push_back(static_cast<char_type>(ch));
    return ch;
  }

  auto xsputn(const char_type* s, streamsize count) -> streamsize override {
    buffer_.append(s, s + count);
    return count;
  }
};

inline auto get_classic_locale() -> const std::locale& {
  static const auto& locale = std::locale::classic();
  return locale;
}

template <typename CodeUnit> struct codecvt_result {
  static constexpr size_t max_size = 32;
  CodeUnit buf[max_size];
  CodeUnit* end;
};

template <typename CodeUnit>
void write_codecvt(codecvt_result<CodeUnit>& out, string_view in,
                   const std::locale& loc) {
  FMT_PRAGMA_CLANG(diagnostic push)
  FMT_PRAGMA_CLANG(diagnostic ignored "-Wdeprecated")
  auto& f = std::use_facet<std::codecvt<CodeUnit, char, std::mbstate_t>>(loc);
  FMT_PRAGMA_CLANG(diagnostic pop)
  auto mb = std::mbstate_t();
  const char* from_next = nullptr;
  auto result = f.in(mb, in.begin(), in.end(), from_next, std::begin(out.buf),
                     std::end(out.buf), out.end);
  if (result != std::codecvt_base::ok)
    FMT_THROW(format_error("failed to format time"));
}

template <typename OutputIt>
auto write_encoded_tm_str(OutputIt out, string_view in, const std::locale& loc)
    -> OutputIt {
  if (const_check(detail::use_utf8) && loc != get_classic_locale()) {
    // char16_t and char32_t codecvts are broken in MSVC (linkage errors) and
    // gcc-4.
#if FMT_MSC_VERSION != 0 ||  \
    (defined(__GLIBCXX__) && \
     (!defined(_GLIBCXX_USE_DUAL_ABI) || _GLIBCXX_USE_DUAL_ABI == 0))
    // The _GLIBCXX_USE_DUAL_ABI macro is always defined in libstdc++ from gcc-5
    // and newer.
    using code_unit = wchar_t;
#else
    using code_unit = char32_t;
#endif

    using unit_t = codecvt_result<code_unit>;
    unit_t unit;
    write_codecvt(unit, in, loc);
    // In UTF-8 is used one to four one-byte code units.
    auto u =
        to_utf8<code_unit, basic_memory_buffer<char, unit_t::max_size * 4>>();
    if (!u.convert({unit.buf, to_unsigned(unit.end - unit.buf)}))
      FMT_THROW(format_error("failed to format time"));
    return copy<char>(u.c_str(), u.c_str() + u.size(), out);
  }
  return copy<char>(in.data(), in.data() + in.size(), out);
}

template <typename Char, typename OutputIt,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
auto write_tm_str(OutputIt out, string_view sv, const std::locale& loc)
    -> OutputIt {
  codecvt_result<Char> unit;
  write_codecvt(unit, sv, loc);
  return copy<Char>(unit.buf, unit.end, out);
}

template <typename Char, typename OutputIt,
          FMT_ENABLE_IF(std::is_same<Char, char>::value)>
auto write_tm_str(OutputIt out, string_view sv, const std::locale& loc)
    -> OutputIt {
  return write_encoded_tm_str(out, sv, loc);
}

template <typename Char>
inline void do_write(buffer<Char>& buf, const std::tm& time,
                     const std::locale& loc, char format, char modifier) {
  auto&& format_buf = formatbuf<std::basic_streambuf<Char>>(buf);
  auto&& os = std::basic_ostream<Char>(&format_buf);
  os.imbue(loc);
  const auto& facet = std::use_facet<std::time_put<Char>>(loc);
  auto end = facet.put(os, os, Char(' '), &time, format, modifier);
  if (end.failed()) FMT_THROW(format_error("failed to format time"));
}

template <typename Char, typename OutputIt,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
auto write(OutputIt out, const std::tm& time, const std::locale& loc,
           char format, char modifier = 0) -> OutputIt {
  auto&& buf = get_buffer<Char>(out);
  do_write<Char>(buf, time, loc, format, modifier);
  return get_iterator(buf, out);
}

template <typename Char, typename OutputIt,
          FMT_ENABLE_IF(std::is_same<Char, char>::value)>
auto write(OutputIt out, const std::tm& time, const std::locale& loc,
           char format, char modifier = 0) -> OutputIt {
  auto&& buf = basic_memory_buffer<Char>();
  do_write<char>(buf, time, loc, format, modifier);
  return write_encoded_tm_str(out, string_view(buf.data(), buf.size()), loc);
}

template <typename T, typename U>
using is_similar_arithmetic_type =
    bool_constant<(std::is_integral<T>::value && std::is_integral<U>::value) ||
                  (std::is_floating_point<T>::value &&
                   std::is_floating_point<U>::value)>;

FMT_NORETURN inline void throw_duration_error() {
  FMT_THROW(format_error("cannot format duration"));
}

// Cast one integral duration to another with an overflow check.
template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(std::is_integral<FromRep>::value&&
                            std::is_integral<typename To::rep>::value)>
auto duration_cast(std::chrono::duration<FromRep, FromPeriod> from) -> To {
#if !FMT_SAFE_DURATION_CAST
  return std::chrono::duration_cast<To>(from);
#else
  // The conversion factor: to.count() == factor * from.count().
  using factor = std::ratio_divide<FromPeriod, typename To::period>;

  using common_rep = typename std::common_type<FromRep, typename To::rep,
                                               decltype(factor::num)>::type;
  common_rep count = from.count();  // This conversion is lossless.

  // Multiply from.count() by factor and check for overflow.
  if (const_check(factor::num != 1)) {
    if (count > max_value<common_rep>() / factor::num) throw_duration_error();
    const auto min = (std::numeric_limits<common_rep>::min)() / factor::num;
    if (const_check(!std::is_unsigned<common_rep>::value) && count < min)
      throw_duration_error();
    count *= factor::num;
  }
  if (const_check(factor::den != 1)) count /= factor::den;
  int ec = 0;
  auto to =
      To(safe_duration_cast::lossless_integral_conversion<typename To::rep>(
          count, ec));
  if (ec) throw_duration_error();
  return to;
#endif
}

template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(std::is_floating_point<FromRep>::value&&
                            std::is_floating_point<typename To::rep>::value)>
auto duration_cast(std::chrono::duration<FromRep, FromPeriod> from) -> To {
#if FMT_SAFE_DURATION_CAST
  // Preserve infinity and NaN.
  if (!isfinite(from.count())) return static_cast<To>(from.count());
  // Throwing version of safe_duration_cast is only available for
  // integer to integer or float to float casts.
  int ec;
  To to = safe_duration_cast::safe_duration_cast<To>(from, ec);
  if (ec) throw_duration_error();
  return to;
#else
  // Standard duration cast, may overflow.
  return std::chrono::duration_cast<To>(from);
#endif
}

template <typename To, typename FromRep, typename FromPeriod,
          FMT_ENABLE_IF(
              !is_similar_arithmetic_type<FromRep, typename To::rep>::value)>
auto duration_cast(std::chrono::duration<FromRep, FromPeriod> from) -> To {
  // Mixed integer <-> float cast is not supported by safe duration_cast.
  return std::chrono::duration_cast<To>(from);
}

template <typename Duration>
auto to_time_t(sys_time<Duration> time_point) -> std::time_t {
  // Cannot use std::chrono::system_clock::to_time_t since this would first
  // require a cast to std::chrono::system_clock::time_point, which could
  // overflow.
  return detail::duration_cast<std::chrono::duration<std::time_t>>(
             time_point.time_since_epoch())
      .count();
}

}  // namespace detail

FMT_BEGIN_EXPORT

/**
 * Converts given time since epoch as `std::time_t` value into calendar time,
 * expressed in Coordinated Universal Time (UTC). Unlike `std::gmtime`, this
 * function is thread-safe on most platforms.
 */
inline auto gmtime(std::time_t time) -> std::tm {
  struct dispatcher {
    std::time_t time_;
    std::tm tm_;

    inline dispatcher(std::time_t t) : time_(t) {}

    inline auto run() -> bool {
      using namespace fmt::detail;
      return handle(gmtime_r(&time_, &tm_));
    }

    inline auto handle(std::tm* tm) -> bool { return tm != nullptr; }

    inline auto handle(detail::null<>) -> bool {
      using namespace fmt::detail;
      return fallback(gmtime_s(&tm_, &time_));
    }

    inline auto fallback(int res) -> bool { return res == 0; }

#if !FMT_MSC_VERSION
    inline auto fallback(detail::null<>) -> bool {
      std::tm* tm = std::gmtime(&time_);
      if (tm) tm_ = *tm;
      return tm != nullptr;
    }
#endif
  };
  auto gt = dispatcher(time);
  // Too big time values may be unsupported.
  if (!gt.run()) FMT_THROW(format_error("time_t value out of range"));
  return gt.tm_;
}

template <typename Duration>
inline auto gmtime(sys_time<Duration> time_point) -> std::tm {
  return gmtime(detail::to_time_t(time_point));
}

namespace detail {

// Writes two-digit numbers a, b and c separated by sep to buf.
// The method by Pavel Novikov based on
// https://johnnylee-sde.github.io/Fast-unsigned-integer-to-time-string/.
inline void write_digit2_separated(char* buf, unsigned a, unsigned b,
                                   unsigned c, char sep) {
  unsigned long long digits =
      a | (b << 24) | (static_cast<unsigned long long>(c) << 48);
  // Convert each value to BCD.
  // We have x = a * 10 + b and we want to convert it to BCD y = a * 16 + b.
  // The difference is
  //   y - x = a * 6
  // a can be found from x:
  //   a = floor(x / 10)
  // then
  //   y = x + a * 6 = x + floor(x / 10) * 6
  // floor(x / 10) is (x * 205) >> 11 (needs 16 bits).
  digits += (((digits * 205) >> 11) & 0x000f00000f00000f) * 6;
  // Put low nibbles to high bytes and high nibbles to low bytes.
  digits = ((digits & 0x00f00000f00000f0) >> 4) |
           ((digits & 0x000f00000f00000f) << 8);
  auto usep = static_cast<unsigned long long>(sep);
  // Add ASCII '0' to each digit byte and insert separators.
  digits |= 0x3030003030003030 | (usep << 16) | (usep << 40);

  constexpr size_t len = 8;
  if (const_check(is_big_endian())) {
    char tmp[len];
    std::memcpy(tmp, &digits, len);
    std::reverse_copy(tmp, tmp + len, buf);
  } else {
    std::memcpy(buf, &digits, len);
  }
}

template <typename Period>
FMT_CONSTEXPR inline auto get_units() -> const char* {
  if (std::is_same<Period, std::atto>::value) return "as";
  if (std::is_same<Period, std::femto>::value) return "fs";
  if (std::is_same<Period, std::pico>::value) return "ps";
  if (std::is_same<Period, std::nano>::value) return "ns";
  if (std::is_same<Period, std::micro>::value)
    return detail::use_utf8 ? "µs" : "us";
  if (std::is_same<Period, std::milli>::value) return "ms";
  if (std::is_same<Period, std::centi>::value) return "cs";
  if (std::is_same<Period, std::deci>::value) return "ds";
  if (std::is_same<Period, std::ratio<1>>::value) return "s";
  if (std::is_same<Period, std::deca>::value) return "das";
  if (std::is_same<Period, std::hecto>::value) return "hs";
  if (std::is_same<Period, std::kilo>::value) return "ks";
  if (std::is_same<Period, std::mega>::value) return "Ms";
  if (std::is_same<Period, std::giga>::value) return "Gs";
  if (std::is_same<Period, std::tera>::value) return "Ts";
  if (std::is_same<Period, std::peta>::value) return "Ps";
  if (std::is_same<Period, std::exa>::value) return "Es";
  if (std::is_same<Period, std::ratio<60>>::value) return "min";
  if (std::is_same<Period, std::ratio<3600>>::value) return "h";
  if (std::is_same<Period, std::ratio<86400>>::value) return "d";
  return nullptr;
}

enum class numeric_system {
  standard,
  // Alternative numeric system, e.g. 十二 instead of 12 in ja_JP locale.
  alternative
};

// Glibc extensions for formatting numeric values.
enum class pad_type {
  // Pad a numeric result string with zeros (the default).
  zero,
  // Do not pad a numeric result string.
  none,
  // Pad a numeric result string with spaces.
  space,
};

template <typename OutputIt>
auto write_padding(OutputIt out, pad_type pad, int width) -> OutputIt {
  if (pad == pad_type::none) return out;
  return detail::fill_n(out, width, pad == pad_type::space ? ' ' : '0');
}

template <typename OutputIt>
auto write_padding(OutputIt out, pad_type pad) -> OutputIt {
  if (pad != pad_type::none) *out++ = pad == pad_type::space ? ' ' : '0';
  return out;
}

// Parses a put_time-like format string and invokes handler actions.
template <typename Char, typename Handler>
FMT_CONSTEXPR auto parse_chrono_format(const Char* begin, const Char* end,
                                       Handler&& handler) -> const Char* {
  if (begin == end || *begin == '}') return begin;
  if (*begin != '%') FMT_THROW(format_error("invalid format"));
  auto ptr = begin;
  while (ptr != end) {
    pad_type pad = pad_type::zero;
    auto c = *ptr;
    if (c == '}') break;
    if (c != '%') {
      ++ptr;
      continue;
    }
    if (begin != ptr) handler.on_text(begin, ptr);
    ++ptr;  // consume '%'
    if (ptr == end) FMT_THROW(format_error("invalid format"));
    c = *ptr;
    switch (c) {
    case '_':
      pad = pad_type::space;
      ++ptr;
      break;
    case '-':
      pad = pad_type::none;
      ++ptr;
      break;
    }
    if (ptr == end) FMT_THROW(format_error("invalid format"));
    c = *ptr++;
    switch (c) {
    case '%': handler.on_text(ptr - 1, ptr); break;
    case 'n': {
      const Char newline[] = {'\n'};
      handler.on_text(newline, newline + 1);
      break;
    }
    case 't': {
      const Char tab[] = {'\t'};
      handler.on_text(tab, tab + 1);
      break;
    }
    // Year:
    case 'Y': handler.on_year(numeric_system::standard, pad); break;
    case 'y': handler.on_short_year(numeric_system::standard); break;
    case 'C': handler.on_century(numeric_system::standard); break;
    case 'G': handler.on_iso_week_based_year(); break;
    case 'g': handler.on_iso_week_based_short_year(); break;
    // Day of the week:
    case 'a': handler.on_abbr_weekday(); break;
    case 'A': handler.on_full_weekday(); break;
    case 'w': handler.on_dec0_weekday(numeric_system::standard); break;
    case 'u': handler.on_dec1_weekday(numeric_system::standard); break;
    // Month:
    case 'b':
    case 'h': handler.on_abbr_month(); break;
    case 'B': handler.on_full_month(); break;
    case 'm': handler.on_dec_month(numeric_system::standard, pad); break;
    // Day of the year/month:
    case 'U':
      handler.on_dec0_week_of_year(numeric_system::standard, pad);
      break;
    case 'W':
      handler.on_dec1_week_of_year(numeric_system::standard, pad);
      break;
    case 'V': handler.on_iso_week_of_year(numeric_system::standard, pad); break;
    case 'j': handler.on_day_of_year(pad); break;
    case 'd': handler.on_day_of_month(numeric_system::standard, pad); break;
    case 'e':
      handler.on_day_of_month(numeric_system::standard, pad_type::space);
      break;
    // Hour, minute, second:
    case 'H': handler.on_24_hour(numeric_system::standard, pad); break;
    case 'I': handler.on_12_hour(numeric_system::standard, pad); break;
    case 'M': handler.on_minute(numeric_system::standard, pad); break;
    case 'S': handler.on_second(numeric_system::standard, pad); break;
    // Other:
    case 'c': handler.on_datetime(numeric_system::standard); break;
    case 'x': handler.on_loc_date(numeric_system::standard); break;
    case 'X': handler.on_loc_time(numeric_system::standard); break;
    case 'D': handler.on_us_date(); break;
    case 'F': handler.on_iso_date(); break;
    case 'r': handler.on_12_hour_time(); break;
    case 'R': handler.on_24_hour_time(); break;
    case 'T': handler.on_iso_time(); break;
    case 'p': handler.on_am_pm(); break;
    case 'Q': handler.on_duration_value(); break;
    case 'q': handler.on_duration_unit(); break;
    case 'z': handler.on_utc_offset(numeric_system::standard); break;
    case 'Z': handler.on_tz_name(); break;
    // Alternative representation:
    case 'E': {
      if (ptr == end) FMT_THROW(format_error("invalid format"));
      c = *ptr++;
      switch (c) {
      case 'Y': handler.on_year(numeric_system::alternative, pad); break;
      case 'y': handler.on_offset_year(); break;
      case 'C': handler.on_century(numeric_system::alternative); break;
      case 'c': handler.on_datetime(numeric_system::alternative); break;
      case 'x': handler.on_loc_date(numeric_system::alternative); break;
      case 'X': handler.on_loc_time(numeric_system::alternative); break;
      case 'z': handler.on_utc_offset(numeric_system::alternative); break;
      default:  FMT_THROW(format_error("invalid format"));
      }
      break;
    }
    case 'O':
      if (ptr == end) FMT_THROW(format_error("invalid format"));
      c = *ptr++;
      switch (c) {
      case 'y': handler.on_short_year(numeric_system::alternative); break;
      case 'm': handler.on_dec_month(numeric_system::alternative, pad); break;
      case 'U':
        handler.on_dec0_week_of_year(numeric_system::alternative, pad);
        break;
      case 'W':
        handler.on_dec1_week_of_year(numeric_system::alternative, pad);
        break;
      case 'V':
        handler.on_iso_week_of_year(numeric_system::alternative, pad);
        break;
      case 'd':
        handler.on_day_of_month(numeric_system::alternative, pad);
        break;
      case 'e':
        handler.on_day_of_month(numeric_system::alternative, pad_type::space);
        break;
      case 'w': handler.on_dec0_weekday(numeric_system::alternative); break;
      case 'u': handler.on_dec1_weekday(numeric_system::alternative); break;
      case 'H': handler.on_24_hour(numeric_system::alternative, pad); break;
      case 'I': handler.on_12_hour(numeric_system::alternative, pad); break;
      case 'M': handler.on_minute(numeric_system::alternative, pad); break;
      case 'S': handler.on_second(numeric_system::alternative, pad); break;
      case 'z': handler.on_utc_offset(numeric_system::alternative); break;
      default:  FMT_THROW(format_error("invalid format"));
      }
      break;
    default: FMT_THROW(format_error("invalid format"));
    }
    begin = ptr;
  }
  if (begin != ptr) handler.on_text(begin, ptr);
  return ptr;
}

template <typename Derived> struct null_chrono_spec_handler {
  FMT_CONSTEXPR void unsupported() {
    static_cast<Derived*>(this)->unsupported();
  }
  FMT_CONSTEXPR void on_year(numeric_system, pad_type) { unsupported(); }
  FMT_CONSTEXPR void on_short_year(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_offset_year() { unsupported(); }
  FMT_CONSTEXPR void on_century(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_iso_week_based_year() { unsupported(); }
  FMT_CONSTEXPR void on_iso_week_based_short_year() { unsupported(); }
  FMT_CONSTEXPR void on_abbr_weekday() { unsupported(); }
  FMT_CONSTEXPR void on_full_weekday() { unsupported(); }
  FMT_CONSTEXPR void on_dec0_weekday(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_dec1_weekday(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_abbr_month() { unsupported(); }
  FMT_CONSTEXPR void on_full_month() { unsupported(); }
  FMT_CONSTEXPR void on_dec_month(numeric_system, pad_type) { unsupported(); }
  FMT_CONSTEXPR void on_dec0_week_of_year(numeric_system, pad_type) {
    unsupported();
  }
  FMT_CONSTEXPR void on_dec1_week_of_year(numeric_system, pad_type) {
    unsupported();
  }
  FMT_CONSTEXPR void on_iso_week_of_year(numeric_system, pad_type) {
    unsupported();
  }
  FMT_CONSTEXPR void on_day_of_year(pad_type) { unsupported(); }
  FMT_CONSTEXPR void on_day_of_month(numeric_system, pad_type) {
    unsupported();
  }
  FMT_CONSTEXPR void on_24_hour(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_12_hour(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_minute(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_second(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_datetime(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_loc_date(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_loc_time(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_us_date() { unsupported(); }
  FMT_CONSTEXPR void on_iso_date() { unsupported(); }
  FMT_CONSTEXPR void on_12_hour_time() { unsupported(); }
  FMT_CONSTEXPR void on_24_hour_time() { unsupported(); }
  FMT_CONSTEXPR void on_iso_time() { unsupported(); }
  FMT_CONSTEXPR void on_am_pm() { unsupported(); }
  FMT_CONSTEXPR void on_duration_value() { unsupported(); }
  FMT_CONSTEXPR void on_duration_unit() { unsupported(); }
  FMT_CONSTEXPR void on_utc_offset(numeric_system) { unsupported(); }
  FMT_CONSTEXPR void on_tz_name() { unsupported(); }
};

class tm_format_checker : public null_chrono_spec_handler<tm_format_checker> {
 private:
  bool has_timezone_ = false;

 public:
  constexpr explicit tm_format_checker(bool has_timezone)
      : has_timezone_(has_timezone) {}

  FMT_NORETURN inline void unsupported() {
    FMT_THROW(format_error("no format"));
  }

  template <typename Char>
  FMT_CONSTEXPR void on_text(const Char*, const Char*) {}
  FMT_CONSTEXPR void on_year(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_short_year(numeric_system) {}
  FMT_CONSTEXPR void on_offset_year() {}
  FMT_CONSTEXPR void on_century(numeric_system) {}
  FMT_CONSTEXPR void on_iso_week_based_year() {}
  FMT_CONSTEXPR void on_iso_week_based_short_year() {}
  FMT_CONSTEXPR void on_abbr_weekday() {}
  FMT_CONSTEXPR void on_full_weekday() {}
  FMT_CONSTEXPR void on_dec0_weekday(numeric_system) {}
  FMT_CONSTEXPR void on_dec1_weekday(numeric_system) {}
  FMT_CONSTEXPR void on_abbr_month() {}
  FMT_CONSTEXPR void on_full_month() {}
  FMT_CONSTEXPR void on_dec_month(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_dec0_week_of_year(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_dec1_week_of_year(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_iso_week_of_year(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_day_of_year(pad_type) {}
  FMT_CONSTEXPR void on_day_of_month(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_24_hour(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_12_hour(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_minute(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_second(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_datetime(numeric_system) {}
  FMT_CONSTEXPR void on_loc_date(numeric_system) {}
  FMT_CONSTEXPR void on_loc_time(numeric_system) {}
  FMT_CONSTEXPR void on_us_date() {}
  FMT_CONSTEXPR void on_iso_date() {}
  FMT_CONSTEXPR void on_12_hour_time() {}
  FMT_CONSTEXPR void on_24_hour_time() {}
  FMT_CONSTEXPR void on_iso_time() {}
  FMT_CONSTEXPR void on_am_pm() {}
  FMT_CONSTEXPR void on_utc_offset(numeric_system) {
    if (!has_timezone_) FMT_THROW(format_error("no timezone"));
  }
  FMT_CONSTEXPR void on_tz_name() {
    if (!has_timezone_) FMT_THROW(format_error("no timezone"));
  }
};

inline auto tm_wday_full_name(int wday) -> const char* {
  static constexpr const char* full_name_list[] = {
      "Sunday",   "Monday", "Tuesday", "Wednesday",
      "Thursday", "Friday", "Saturday"};
  return wday >= 0 && wday <= 6 ? full_name_list[wday] : "?";
}
inline auto tm_wday_short_name(int wday) -> const char* {
  static constexpr const char* short_name_list[] = {"Sun", "Mon", "Tue", "Wed",
                                                    "Thu", "Fri", "Sat"};
  return wday >= 0 && wday <= 6 ? short_name_list[wday] : "???";
}

inline auto tm_mon_full_name(int mon) -> const char* {
  static constexpr const char* full_name_list[] = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};
  return mon >= 0 && mon <= 11 ? full_name_list[mon] : "?";
}
inline auto tm_mon_short_name(int mon) -> const char* {
  static constexpr const char* short_name_list[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  return mon >= 0 && mon <= 11 ? short_name_list[mon] : "???";
}

template <typename T, typename = void>
struct has_tm_gmtoff : std::false_type {};
template <typename T>
struct has_tm_gmtoff<T, void_t<decltype(T::tm_gmtoff)>> : std::true_type {};

template <typename T, typename = void> struct has_tm_zone : std::false_type {};
template <typename T>
struct has_tm_zone<T, void_t<decltype(T::tm_zone)>> : std::true_type {};

template <typename T, FMT_ENABLE_IF(has_tm_zone<T>::value)>
auto set_tm_zone(T& time, char* tz) -> bool {
  time.tm_zone = tz;
  return true;
}
template <typename T, FMT_ENABLE_IF(!has_tm_zone<T>::value)>
auto set_tm_zone(T&, char*) -> bool {
  return false;
}

inline auto utc() -> char* {
  static char tz[] = "UTC";
  return tz;
}

// Converts value to Int and checks that it's in the range [0, upper).
template <typename T, typename Int, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline auto to_nonnegative_int(T value, Int upper) -> Int {
  if (!std::is_unsigned<Int>::value &&
      (value < 0 || to_unsigned(value) > to_unsigned(upper))) {
    FMT_THROW(format_error("chrono value is out of range"));
  }
  return static_cast<Int>(value);
}
template <typename T, typename Int, FMT_ENABLE_IF(!std::is_integral<T>::value)>
inline auto to_nonnegative_int(T value, Int upper) -> Int {
  auto int_value = static_cast<Int>(value);
  if (int_value < 0 || value > static_cast<T>(upper))
    FMT_THROW(format_error("invalid value"));
  return int_value;
}

constexpr auto pow10(std::uint32_t n) -> long long {
  return n == 0 ? 1 : 10 * pow10(n - 1);
}

// Counts the number of fractional digits in the range [0, 18] according to the
// C++20 spec. If more than 18 fractional digits are required then returns 6 for
// microseconds precision.
template <long long Num, long long Den, int N = 0,
          bool Enabled = (N < 19) && (Num <= max_value<long long>() / 10)>
struct count_fractional_digits {
  static constexpr int value =
      Num % Den == 0 ? N : count_fractional_digits<Num * 10, Den, N + 1>::value;
};

// Base case that doesn't instantiate any more templates
// in order to avoid overflow.
template <long long Num, long long Den, int N>
struct count_fractional_digits<Num, Den, N, false> {
  static constexpr int value = (Num % Den == 0) ? N : 6;
};

// Format subseconds which are given as an integer type with an appropriate
// number of digits.
template <typename Char, typename OutputIt, typename Duration>
void write_fractional_seconds(OutputIt& out, Duration d, int precision = -1) {
  constexpr auto num_fractional_digits =
      count_fractional_digits<Duration::period::num,
                              Duration::period::den>::value;

  using subsecond_precision = std::chrono::duration<
      typename std::common_type<typename Duration::rep,
                                std::chrono::seconds::rep>::type,
      std::ratio<1, pow10(num_fractional_digits)>>;

  const auto fractional = d - detail::duration_cast<std::chrono::seconds>(d);
  const auto subseconds =
      std::chrono::treat_as_floating_point<
          typename subsecond_precision::rep>::value
          ? fractional.count()
          : detail::duration_cast<subsecond_precision>(fractional).count();
  auto n = static_cast<uint32_or_64_or_128_t<long long>>(subseconds);
  const int num_digits = count_digits(n);

  int leading_zeroes = (std::max)(0, num_fractional_digits - num_digits);
  if (precision < 0) {
    FMT_ASSERT(!std::is_floating_point<typename Duration::rep>::value, "");
    if (std::ratio_less<typename subsecond_precision::period,
                        std::chrono::seconds::period>::value) {
      *out++ = '.';
      out = detail::fill_n(out, leading_zeroes, '0');
      out = format_decimal<Char>(out, n, num_digits);
    }
  } else if (precision > 0) {
    *out++ = '.';
    leading_zeroes = min_of(leading_zeroes, precision);
    int remaining = precision - leading_zeroes;
    out = detail::fill_n(out, leading_zeroes, '0');
    if (remaining < num_digits) {
      int num_truncated_digits = num_digits - remaining;
      n /= to_unsigned(pow10(to_unsigned(num_truncated_digits)));
      if (n != 0) out = format_decimal<Char>(out, n, remaining);
      return;
    }
    if (n != 0) {
      out = format_decimal<Char>(out, n, num_digits);
      remaining -= num_digits;
    }
    out = detail::fill_n(out, remaining, '0');
  }
}

// Format subseconds which are given as a floating point type with an
// appropriate number of digits. We cannot pass the Duration here, as we
// explicitly need to pass the Rep value in the duration_formatter.
template <typename Duration>
void write_floating_seconds(memory_buffer& buf, Duration duration,
                            int num_fractional_digits = -1) {
  using rep = typename Duration::rep;
  FMT_ASSERT(std::is_floating_point<rep>::value, "");

  auto val = duration.count();

  if (num_fractional_digits < 0) {
    // For `std::round` with fallback to `round`:
    // On some toolchains `std::round` is not available (e.g. GCC 6).
    using namespace std;
    num_fractional_digits =
        count_fractional_digits<Duration::period::num,
                                Duration::period::den>::value;
    if (num_fractional_digits < 6 && static_cast<rep>(round(val)) != val)
      num_fractional_digits = 6;
  }

  fmt::format_to(std::back_inserter(buf), FMT_STRING("{:.{}f}"),
                 std::fmod(val * static_cast<rep>(Duration::period::num) /
                               static_cast<rep>(Duration::period::den),
                           static_cast<rep>(60)),
                 num_fractional_digits);
}

template <typename OutputIt, typename Char,
          typename Duration = std::chrono::seconds>
class tm_writer {
 private:
  static constexpr int days_per_week = 7;

  const std::locale& loc_;
  bool is_classic_;
  OutputIt out_;
  const Duration* subsecs_;
  const std::tm& tm_;

  auto tm_sec() const noexcept -> int {
    FMT_ASSERT(tm_.tm_sec >= 0 && tm_.tm_sec <= 61, "");
    return tm_.tm_sec;
  }
  auto tm_min() const noexcept -> int {
    FMT_ASSERT(tm_.tm_min >= 0 && tm_.tm_min <= 59, "");
    return tm_.tm_min;
  }
  auto tm_hour() const noexcept -> int {
    FMT_ASSERT(tm_.tm_hour >= 0 && tm_.tm_hour <= 23, "");
    return tm_.tm_hour;
  }
  auto tm_mday() const noexcept -> int {
    FMT_ASSERT(tm_.tm_mday >= 1 && tm_.tm_mday <= 31, "");
    return tm_.tm_mday;
  }
  auto tm_mon() const noexcept -> int {
    FMT_ASSERT(tm_.tm_mon >= 0 && tm_.tm_mon <= 11, "");
    return tm_.tm_mon;
  }
  auto tm_year() const noexcept -> long long { return 1900ll + tm_.tm_year; }
  auto tm_wday() const noexcept -> int {
    FMT_ASSERT(tm_.tm_wday >= 0 && tm_.tm_wday <= 6, "");
    return tm_.tm_wday;
  }
  auto tm_yday() const noexcept -> int {
    FMT_ASSERT(tm_.tm_yday >= 0 && tm_.tm_yday <= 365, "");
    return tm_.tm_yday;
  }

  auto tm_hour12() const noexcept -> int {
    auto h = tm_hour();
    auto z = h < 12 ? h : h - 12;
    return z == 0 ? 12 : z;
  }

  // POSIX and the C Standard are unclear or inconsistent about what %C and %y
  // do if the year is negative or exceeds 9999. Use the convention that %C
  // concatenated with %y yields the same output as %Y, and that %Y contains at
  // least 4 characters, with more only if necessary.
  auto split_year_lower(long long year) const noexcept -> int {
    auto l = year % 100;
    if (l < 0) l = -l;  // l in [0, 99]
    return static_cast<int>(l);
  }

  // Algorithm: https://en.wikipedia.org/wiki/ISO_week_date.
  auto iso_year_weeks(long long curr_year) const noexcept -> int {
    auto prev_year = curr_year - 1;
    auto curr_p =
        (curr_year + curr_year / 4 - curr_year / 100 + curr_year / 400) %
        days_per_week;
    auto prev_p =
        (prev_year + prev_year / 4 - prev_year / 100 + prev_year / 400) %
        days_per_week;
    return 52 + ((curr_p == 4 || prev_p == 3) ? 1 : 0);
  }
  auto iso_week_num(int tm_yday, int tm_wday) const noexcept -> int {
    return (tm_yday + 11 - (tm_wday == 0 ? days_per_week : tm_wday)) /
           days_per_week;
  }
  auto tm_iso_week_year() const noexcept -> long long {
    auto year = tm_year();
    auto w = iso_week_num(tm_yday(), tm_wday());
    if (w < 1) return year - 1;
    if (w > iso_year_weeks(year)) return year + 1;
    return year;
  }
  auto tm_iso_week_of_year() const noexcept -> int {
    auto year = tm_year();
    auto w = iso_week_num(tm_yday(), tm_wday());
    if (w < 1) return iso_year_weeks(year - 1);
    if (w > iso_year_weeks(year)) return 1;
    return w;
  }

  void write1(int value) {
    *out_++ = static_cast<char>('0' + to_unsigned(value) % 10);
  }
  void write2(int value) {
    const char* d = digits2(to_unsigned(value) % 100);
    *out_++ = *d++;
    *out_++ = *d;
  }
  void write2(int value, pad_type pad) {
    unsigned int v = to_unsigned(value) % 100;
    if (v >= 10) {
      const char* d = digits2(v);
      *out_++ = *d++;
      *out_++ = *d;
    } else {
      out_ = detail::write_padding(out_, pad);
      *out_++ = static_cast<char>('0' + v);
    }
  }

  void write_year_extended(long long year, pad_type pad) {
    // At least 4 characters.
    int width = 4;
    bool negative = year < 0;
    if (negative) {
      year = 0 - year;
      --width;
    }
    uint32_or_64_or_128_t<long long> n = to_unsigned(year);
    const int num_digits = count_digits(n);
    if (negative && pad == pad_type::zero) *out_++ = '-';
    if (width > num_digits)
      out_ = detail::write_padding(out_, pad, width - num_digits);
    if (negative && pad != pad_type::zero) *out_++ = '-';
    out_ = format_decimal<Char>(out_, n, num_digits);
  }
  void write_year(long long year, pad_type pad) {
    write_year_extended(year, pad);
  }

  void write_utc_offset(long long offset, numeric_system ns) {
    if (offset < 0) {
      *out_++ = '-';
      offset = -offset;
    } else {
      *out_++ = '+';
    }
    offset /= 60;
    write2(static_cast<int>(offset / 60));
    if (ns != numeric_system::standard) *out_++ = ':';
    write2(static_cast<int>(offset % 60));
  }

  template <typename T, FMT_ENABLE_IF(has_tm_gmtoff<T>::value)>
  void format_utc_offset(const T& tm, numeric_system ns) {
    write_utc_offset(tm.tm_gmtoff, ns);
  }
  template <typename T, FMT_ENABLE_IF(!has_tm_gmtoff<T>::value)>
  void format_utc_offset(const T&, numeric_system ns) {
    write_utc_offset(0, ns);
  }

  template <typename T, FMT_ENABLE_IF(has_tm_zone<T>::value)>
  void format_tz_name(const T& tm) {
    out_ = write_tm_str<Char>(out_, tm.tm_zone, loc_);
  }
  template <typename T, FMT_ENABLE_IF(!has_tm_zone<T>::value)>
  void format_tz_name(const T&) {
    out_ = std::copy_n(utc(), 3, out_);
  }

  void format_localized(char format, char modifier = 0) {
    out_ = write<Char>(out_, tm_, loc_, format, modifier);
  }

 public:
  tm_writer(const std::locale& loc, OutputIt out, const std::tm& tm,
            const Duration* subsecs = nullptr)
      : loc_(loc),
        is_classic_(loc_ == get_classic_locale()),
        out_(out),
        subsecs_(subsecs),
        tm_(tm) {}

  auto out() const -> OutputIt { return out_; }

  FMT_CONSTEXPR void on_text(const Char* begin, const Char* end) {
    out_ = copy<Char>(begin, end, out_);
  }

  void on_abbr_weekday() {
    if (is_classic_)
      out_ = write(out_, tm_wday_short_name(tm_wday()));
    else
      format_localized('a');
  }
  void on_full_weekday() {
    if (is_classic_)
      out_ = write(out_, tm_wday_full_name(tm_wday()));
    else
      format_localized('A');
  }
  void on_dec0_weekday(numeric_system ns) {
    if (is_classic_ || ns == numeric_system::standard) return write1(tm_wday());
    format_localized('w', 'O');
  }
  void on_dec1_weekday(numeric_system ns) {
    if (is_classic_ || ns == numeric_system::standard) {
      auto wday = tm_wday();
      write1(wday == 0 ? days_per_week : wday);
    } else {
      format_localized('u', 'O');
    }
  }

  void on_abbr_month() {
    if (is_classic_)
      out_ = write(out_, tm_mon_short_name(tm_mon()));
    else
      format_localized('b');
  }
  void on_full_month() {
    if (is_classic_)
      out_ = write(out_, tm_mon_full_name(tm_mon()));
    else
      format_localized('B');
  }

  void on_datetime(numeric_system ns) {
    if (is_classic_) {
      on_abbr_weekday();
      *out_++ = ' ';
      on_abbr_month();
      *out_++ = ' ';
      on_day_of_month(numeric_system::standard, pad_type::space);
      *out_++ = ' ';
      on_iso_time();
      *out_++ = ' ';
      on_year(numeric_system::standard, pad_type::space);
    } else {
      format_localized('c', ns == numeric_system::standard ? '\0' : 'E');
    }
  }
  void on_loc_date(numeric_system ns) {
    if (is_classic_)
      on_us_date();
    else
      format_localized('x', ns == numeric_system::standard ? '\0' : 'E');
  }
  void on_loc_time(numeric_system ns) {
    if (is_classic_)
      on_iso_time();
    else
      format_localized('X', ns == numeric_system::standard ? '\0' : 'E');
  }
  void on_us_date() {
    char buf[8];
    write_digit2_separated(buf, to_unsigned(tm_mon() + 1),
                           to_unsigned(tm_mday()),
                           to_unsigned(split_year_lower(tm_year())), '/');
    out_ = copy<Char>(std::begin(buf), std::end(buf), out_);
  }
  void on_iso_date() {
    auto year = tm_year();
    char buf[10];
    size_t offset = 0;
    if (year >= 0 && year < 10000) {
      write2digits(buf, static_cast<size_t>(year / 100));
    } else {
      offset = 4;
      write_year_extended(year, pad_type::zero);
      year = 0;
    }
    write_digit2_separated(buf + 2, static_cast<unsigned>(year % 100),
                           to_unsigned(tm_mon() + 1), to_unsigned(tm_mday()),
                           '-');
    out_ = copy<Char>(std::begin(buf) + offset, std::end(buf), out_);
  }

  void on_utc_offset(numeric_system ns) { format_utc_offset(tm_, ns); }
  void on_tz_name() { format_tz_name(tm_); }

  void on_year(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write_year(tm_year(), pad);
    format_localized('Y', 'E');
  }
  void on_short_year(numeric_system ns) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(split_year_lower(tm_year()));
    format_localized('y', 'O');
  }
  void on_offset_year() {
    if (is_classic_) return write2(split_year_lower(tm_year()));
    format_localized('y', 'E');
  }

  void on_century(numeric_system ns) {
    if (is_classic_ || ns == numeric_system::standard) {
      auto year = tm_year();
      auto upper = year / 100;
      if (year >= -99 && year < 0) {
        // Zero upper on negative year.
        *out_++ = '-';
        *out_++ = '0';
      } else if (upper >= 0 && upper < 100) {
        write2(static_cast<int>(upper));
      } else {
        out_ = write<Char>(out_, upper);
      }
    } else {
      format_localized('C', 'E');
    }
  }

  void on_dec_month(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_mon() + 1, pad);
    format_localized('m', 'O');
  }

  void on_dec0_week_of_year(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2((tm_yday() + days_per_week - tm_wday()) / days_per_week,
                    pad);
    format_localized('U', 'O');
  }
  void on_dec1_week_of_year(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard) {
      auto wday = tm_wday();
      write2((tm_yday() + days_per_week -
              (wday == 0 ? (days_per_week - 1) : (wday - 1))) /
                 days_per_week,
             pad);
    } else {
      format_localized('W', 'O');
    }
  }
  void on_iso_week_of_year(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_iso_week_of_year(), pad);
    format_localized('V', 'O');
  }

  void on_iso_week_based_year() {
    write_year(tm_iso_week_year(), pad_type::zero);
  }
  void on_iso_week_based_short_year() {
    write2(split_year_lower(tm_iso_week_year()));
  }

  void on_day_of_year(pad_type pad) {
    auto yday = tm_yday() + 1;
    auto digit1 = yday / 100;
    if (digit1 != 0)
      write1(digit1);
    else
      out_ = detail::write_padding(out_, pad);
    write2(yday % 100, pad);
  }

  void on_day_of_month(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_mday(), pad);
    format_localized('d', 'O');
  }

  void on_24_hour(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_hour(), pad);
    format_localized('H', 'O');
  }
  void on_12_hour(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_hour12(), pad);
    format_localized('I', 'O');
  }
  void on_minute(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard)
      return write2(tm_min(), pad);
    format_localized('M', 'O');
  }

  void on_second(numeric_system ns, pad_type pad) {
    if (is_classic_ || ns == numeric_system::standard) {
      write2(tm_sec(), pad);
      if (subsecs_) {
        if (std::is_floating_point<typename Duration::rep>::value) {
          auto buf = memory_buffer();
          write_floating_seconds(buf, *subsecs_);
          if (buf.size() > 1) {
            // Remove the leading "0", write something like ".123".
            out_ = copy<Char>(buf.begin() + 1, buf.end(), out_);
          }
        } else {
          write_fractional_seconds<Char>(out_, *subsecs_);
        }
      }
    } else {
      // Currently no formatting of subseconds when a locale is set.
      format_localized('S', 'O');
    }
  }

  void on_12_hour_time() {
    if (is_classic_) {
      char buf[8];
      write_digit2_separated(buf, to_unsigned(tm_hour12()),
                             to_unsigned(tm_min()), to_unsigned(tm_sec()), ':');
      out_ = copy<Char>(std::begin(buf), std::end(buf), out_);
      *out_++ = ' ';
      on_am_pm();
    } else {
      format_localized('r');
    }
  }
  void on_24_hour_time() {
    write2(tm_hour());
    *out_++ = ':';
    write2(tm_min());
  }
  void on_iso_time() {
    on_24_hour_time();
    *out_++ = ':';
    on_second(numeric_system::standard, pad_type::zero);
  }

  void on_am_pm() {
    if (is_classic_) {
      *out_++ = tm_hour() < 12 ? 'A' : 'P';
      *out_++ = 'M';
    } else {
      format_localized('p');
    }
  }

  // These apply to chrono durations but not tm.
  void on_duration_value() {}
  void on_duration_unit() {}
};

struct chrono_format_checker : null_chrono_spec_handler<chrono_format_checker> {
  bool has_precision_integral = false;

  FMT_NORETURN inline void unsupported() { FMT_THROW(format_error("no date")); }

  template <typename Char>
  FMT_CONSTEXPR void on_text(const Char*, const Char*) {}
  FMT_CONSTEXPR void on_day_of_year(pad_type) {}
  FMT_CONSTEXPR void on_24_hour(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_12_hour(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_minute(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_second(numeric_system, pad_type) {}
  FMT_CONSTEXPR void on_12_hour_time() {}
  FMT_CONSTEXPR void on_24_hour_time() {}
  FMT_CONSTEXPR void on_iso_time() {}
  FMT_CONSTEXPR void on_am_pm() {}
  FMT_CONSTEXPR void on_duration_value() const {
    if (has_precision_integral)
      FMT_THROW(format_error("precision not allowed for this argument type"));
  }
  FMT_CONSTEXPR void on_duration_unit() {}
};

template <typename T,
          FMT_ENABLE_IF(std::is_integral<T>::value&& has_isfinite<T>::value)>
inline auto isfinite(T) -> bool {
  return true;
}

template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
inline auto mod(T x, int y) -> T {
  return x % static_cast<T>(y);
}
template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
inline auto mod(T x, int y) -> T {
  return std::fmod(x, static_cast<T>(y));
}

// If T is an integral type, maps T to its unsigned counterpart, otherwise
// leaves it unchanged (unlike std::make_unsigned).
template <typename T, bool INTEGRAL = std::is_integral<T>::value>
struct make_unsigned_or_unchanged {
  using type = T;
};

template <typename T> struct make_unsigned_or_unchanged<T, true> {
  using type = typename std::make_unsigned<T>::type;
};

template <typename Rep, typename Period,
          FMT_ENABLE_IF(std::is_integral<Rep>::value)>
inline auto get_milliseconds(std::chrono::duration<Rep, Period> d)
    -> std::chrono::duration<Rep, std::milli> {
  // This may overflow and/or the result may not fit in the target type.
#if FMT_SAFE_DURATION_CAST
  using common_seconds_type =
      typename std::common_type<decltype(d), std::chrono::seconds>::type;
  auto d_as_common = detail::duration_cast<common_seconds_type>(d);
  auto d_as_whole_seconds =
      detail::duration_cast<std::chrono::seconds>(d_as_common);
  // This conversion should be nonproblematic.
  auto diff = d_as_common - d_as_whole_seconds;
  auto ms = detail::duration_cast<std::chrono::duration<Rep, std::milli>>(diff);
  return ms;
#else
  auto s = detail::duration_cast<std::chrono::seconds>(d);
  return detail::duration_cast<std::chrono::milliseconds>(d - s);
#endif
}

template <typename Char, typename Rep, typename OutputIt,
          FMT_ENABLE_IF(std::is_integral<Rep>::value)>
auto format_duration_value(OutputIt out, Rep val, int) -> OutputIt {
  return write<Char>(out, val);
}

template <typename Char, typename Rep, typename OutputIt,
          FMT_ENABLE_IF(std::is_floating_point<Rep>::value)>
auto format_duration_value(OutputIt out, Rep val, int precision) -> OutputIt {
  auto specs = format_specs();
  specs.precision = precision;
  specs.set_type(precision >= 0 ? presentation_type::fixed
                                : presentation_type::general);
  return write<Char>(out, val, specs);
}

template <typename Char, typename OutputIt>
auto copy_unit(string_view unit, OutputIt out, Char) -> OutputIt {
  return copy<Char>(unit.begin(), unit.end(), out);
}

template <typename OutputIt>
auto copy_unit(string_view unit, OutputIt out, wchar_t) -> OutputIt {
  // This works when wchar_t is UTF-32 because units only contain characters
  // that have the same representation in UTF-16 and UTF-32.
  utf8_to_utf16 u(unit);
  return copy<wchar_t>(u.c_str(), u.c_str() + u.size(), out);
}

template <typename Char, typename Period, typename OutputIt>
auto format_duration_unit(OutputIt out) -> OutputIt {
  if (const char* unit = get_units<Period>())
    return copy_unit(string_view(unit), out, Char());
  *out++ = '[';
  out = write<Char>(out, Period::num);
  if (const_check(Period::den != 1)) {
    *out++ = '/';
    out = write<Char>(out, Period::den);
  }
  *out++ = ']';
  *out++ = 's';
  return out;
}

class get_locale {
 private:
  union {
    std::locale locale_;
  };
  bool has_locale_ = false;

 public:
  inline get_locale(bool localized, locale_ref loc) : has_locale_(localized) {
    if (!localized) return;
    ignore_unused(loc);
    ::new (&locale_) std::locale(
#if FMT_USE_LOCALE
        loc.template get<std::locale>()
#endif
    );
  }
  inline ~get_locale() {
    if (has_locale_) locale_.~locale();
  }
  inline operator const std::locale&() const {
    return has_locale_ ? locale_ : get_classic_locale();
  }
};

template <typename Char, typename Rep, typename Period>
struct duration_formatter {
  using iterator = basic_appender<Char>;
  iterator out;
  // rep is unsigned to avoid overflow.
  using rep =
      conditional_t<std::is_integral<Rep>::value && sizeof(Rep) < sizeof(int),
                    unsigned, typename make_unsigned_or_unchanged<Rep>::type>;
  rep val;
  int precision;
  locale_ref locale;
  bool localized = false;
  using seconds = std::chrono::duration<rep>;
  seconds s;
  using milliseconds = std::chrono::duration<rep, std::milli>;
  bool negative;

  using tm_writer_type = tm_writer<iterator, Char>;

  duration_formatter(iterator o, std::chrono::duration<Rep, Period> d,
                     locale_ref loc)
      : out(o), val(static_cast<rep>(d.count())), locale(loc), negative(false) {
    if (d.count() < 0) {
      val = 0 - val;
      negative = true;
    }

    // this may overflow and/or the result may not fit in the
    // target type.
    // might need checked conversion (rep!=Rep)
    s = detail::duration_cast<seconds>(std::chrono::duration<rep, Period>(val));
  }

  // returns true if nan or inf, writes to out.
  auto handle_nan_inf() -> bool {
    if (isfinite(val)) return false;
    if (isnan(val)) {
      write_nan();
      return true;
    }
    // must be +-inf
    if (val > 0)
      std::copy_n("inf", 3, out);
    else
      std::copy_n("-inf", 4, out);
    return true;
  }

  auto days() const -> Rep { return static_cast<Rep>(s.count() / 86400); }
  auto hour() const -> Rep {
    return static_cast<Rep>(mod((s.count() / 3600), 24));
  }

  auto hour12() const -> Rep {
    Rep hour = static_cast<Rep>(mod((s.count() / 3600), 12));
    return hour <= 0 ? 12 : hour;
  }

  auto minute() const -> Rep {
    return static_cast<Rep>(mod((s.count() / 60), 60));
  }
  auto second() const -> Rep { return static_cast<Rep>(mod(s.count(), 60)); }

  auto time() const -> std::tm {
    auto time = std::tm();
    time.tm_hour = to_nonnegative_int(hour(), 24);
    time.tm_min = to_nonnegative_int(minute(), 60);
    time.tm_sec = to_nonnegative_int(second(), 60);
    return time;
  }

  void write_sign() {
    if (!negative) return;
    *out++ = '-';
    negative = false;
  }

  void write(Rep value, int width, pad_type pad = pad_type::zero) {
    write_sign();
    if (isnan(value)) return write_nan();
    uint32_or_64_or_128_t<int> n =
        to_unsigned(to_nonnegative_int(value, max_value<int>()));
    int num_digits = detail::count_digits(n);
    if (width > num_digits) {
      out = detail::write_padding(out, pad, width - num_digits);
    }
    out = format_decimal<Char>(out, n, num_digits);
  }

  void write_nan() { std::copy_n("nan", 3, out); }

  template <typename Callback, typename... Args>
  void format_tm(const tm& time, Callback cb, Args... args) {
    if (isnan(val)) return write_nan();
    get_locale loc(localized, locale);
    auto w = tm_writer_type(loc, out, time);
    (w.*cb)(args...);
    out = w.out();
  }

  void on_text(const Char* begin, const Char* end) {
    copy<Char>(begin, end, out);
  }

  // These are not implemented because durations don't have date information.
  void on_abbr_weekday() {}
  void on_full_weekday() {}
  void on_dec0_weekday(numeric_system) {}
  void on_dec1_weekday(numeric_system) {}
  void on_abbr_month() {}
  void on_full_month() {}
  void on_datetime(numeric_system) {}
  void on_loc_date(numeric_system) {}
  void on_loc_time(numeric_system) {}
  void on_us_date() {}
  void on_iso_date() {}
  void on_utc_offset(numeric_system) {}
  void on_tz_name() {}
  void on_year(numeric_system, pad_type) {}
  void on_short_year(numeric_system) {}
  void on_offset_year() {}
  void on_century(numeric_system) {}
  void on_iso_week_based_year() {}
  void on_iso_week_based_short_year() {}
  void on_dec_month(numeric_system, pad_type) {}
  void on_dec0_week_of_year(numeric_system, pad_type) {}
  void on_dec1_week_of_year(numeric_system, pad_type) {}
  void on_iso_week_of_year(numeric_system, pad_type) {}
  void on_day_of_month(numeric_system, pad_type) {}

  void on_day_of_year(pad_type) {
    if (handle_nan_inf()) return;
    write(days(), 0);
  }

  void on_24_hour(numeric_system ns, pad_type pad) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(hour(), 2, pad);
    auto time = tm();
    time.tm_hour = to_nonnegative_int(hour(), 24);
    format_tm(time, &tm_writer_type::on_24_hour, ns, pad);
  }

  void on_12_hour(numeric_system ns, pad_type pad) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(hour12(), 2, pad);
    auto time = tm();
    time.tm_hour = to_nonnegative_int(hour12(), 12);
    format_tm(time, &tm_writer_type::on_12_hour, ns, pad);
  }

  void on_minute(numeric_system ns, pad_type pad) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) return write(minute(), 2, pad);
    auto time = tm();
    time.tm_min = to_nonnegative_int(minute(), 60);
    format_tm(time, &tm_writer_type::on_minute, ns, pad);
  }

  void on_second(numeric_system ns, pad_type pad) {
    if (handle_nan_inf()) return;

    if (ns == numeric_system::standard) {
      if (std::is_floating_point<rep>::value) {
        auto buf = memory_buffer();
        write_floating_seconds(buf, std::chrono::duration<rep, Period>(val),
                               precision);
        if (negative) *out++ = '-';
        if (buf.size() < 2 || buf[1] == '.')
          out = detail::write_padding(out, pad);
        out = copy<Char>(buf.begin(), buf.end(), out);
      } else {
        write(second(), 2, pad);
        write_fractional_seconds<Char>(
            out, std::chrono::duration<rep, Period>(val), precision);
      }
      return;
    }
    auto time = tm();
    time.tm_sec = to_nonnegative_int(second(), 60);
    format_tm(time, &tm_writer_type::on_second, ns, pad);
  }

  void on_12_hour_time() {
    if (handle_nan_inf()) return;
    format_tm(time(), &tm_writer_type::on_12_hour_time);
  }

  void on_24_hour_time() {
    if (handle_nan_inf()) {
      *out++ = ':';
      handle_nan_inf();
      return;
    }

    write(hour(), 2);
    *out++ = ':';
    write(minute(), 2);
  }

  void on_iso_time() {
    on_24_hour_time();
    *out++ = ':';
    if (handle_nan_inf()) return;
    on_second(numeric_system::standard, pad_type::zero);
  }

  void on_am_pm() {
    if (handle_nan_inf()) return;
    format_tm(time(), &tm_writer_type::on_am_pm);
  }

  void on_duration_value() {
    if (handle_nan_inf()) return;
    write_sign();
    out = format_duration_value<Char>(out, val, precision);
  }

  void on_duration_unit() { out = format_duration_unit<Char, Period>(out); }
};

}  // namespace detail

#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907
using weekday = std::chrono::weekday;
using day = std::chrono::day;
using month = std::chrono::month;
using year = std::chrono::year;
using year_month_day = std::chrono::year_month_day;
#else
// A fallback version of weekday.
class weekday {
 private:
  unsigned char value_;

 public:
  weekday() = default;
  constexpr explicit weekday(unsigned wd) noexcept
      : value_(static_cast<unsigned char>(wd != 7 ? wd : 0)) {}
  constexpr auto c_encoding() const noexcept -> unsigned { return value_; }
};

class day {
 private:
  unsigned char value_;

 public:
  day() = default;
  constexpr explicit day(unsigned d) noexcept
      : value_(static_cast<unsigned char>(d)) {}
  constexpr explicit operator unsigned() const noexcept { return value_; }
};

class month {
 private:
  unsigned char value_;

 public:
  month() = default;
  constexpr explicit month(unsigned m) noexcept
      : value_(static_cast<unsigned char>(m)) {}
  constexpr explicit operator unsigned() const noexcept { return value_; }
};

class year {
 private:
  int value_;

 public:
  year() = default;
  constexpr explicit year(int y) noexcept : value_(y) {}
  constexpr explicit operator int() const noexcept { return value_; }
};

class year_month_day {
 private:
  fmt::year year_;
  fmt::month month_;
  fmt::day day_;

 public:
  year_month_day() = default;
  constexpr year_month_day(const year& y, const month& m, const day& d) noexcept
      : year_(y), month_(m), day_(d) {}
  constexpr auto year() const noexcept -> fmt::year { return year_; }
  constexpr auto month() const noexcept -> fmt::month { return month_; }
  constexpr auto day() const noexcept -> fmt::day { return day_; }
};
#endif  // __cpp_lib_chrono >= 201907

template <typename Char>
struct formatter<weekday, Char> : private formatter<std::tm, Char> {
 private:
  bool use_tm_formatter_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it == 'L') {
      ++it;
      this->set_localized();
    }
    use_tm_formatter_ = it != end && *it != '}';
    return use_tm_formatter_ ? formatter<std::tm, Char>::parse(ctx) : it;
  }

  template <typename FormatContext>
  auto format(weekday wd, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto time = std::tm();
    time.tm_wday = static_cast<int>(wd.c_encoding());
    if (use_tm_formatter_) return formatter<std::tm, Char>::format(time, ctx);
    detail::get_locale loc(this->localized(), ctx.locale());
    auto w = detail::tm_writer<decltype(ctx.out()), Char>(loc, ctx.out(), time);
    w.on_abbr_weekday();
    return w.out();
  }
};

template <typename Char>
struct formatter<day, Char> : private formatter<std::tm, Char> {
 private:
  bool use_tm_formatter_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    use_tm_formatter_ = it != end && *it != '}';
    return use_tm_formatter_ ? formatter<std::tm, Char>::parse(ctx) : it;
  }

  template <typename FormatContext>
  auto format(day d, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto time = std::tm();
    time.tm_mday = static_cast<int>(static_cast<unsigned>(d));
    if (use_tm_formatter_) return formatter<std::tm, Char>::format(time, ctx);
    detail::get_locale loc(false, ctx.locale());
    auto w = detail::tm_writer<decltype(ctx.out()), Char>(loc, ctx.out(), time);
    w.on_day_of_month(detail::numeric_system::standard, detail::pad_type::zero);
    return w.out();
  }
};

template <typename Char>
struct formatter<month, Char> : private formatter<std::tm, Char> {
 private:
  bool use_tm_formatter_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it == 'L') {
      ++it;
      this->set_localized();
    }
    use_tm_formatter_ = it != end && *it != '}';
    return use_tm_formatter_ ? formatter<std::tm, Char>::parse(ctx) : it;
  }

  template <typename FormatContext>
  auto format(month m, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto time = std::tm();
    time.tm_mon = static_cast<int>(static_cast<unsigned>(m)) - 1;
    if (use_tm_formatter_) return formatter<std::tm, Char>::format(time, ctx);
    detail::get_locale loc(this->localized(), ctx.locale());
    auto w = detail::tm_writer<decltype(ctx.out()), Char>(loc, ctx.out(), time);
    w.on_abbr_month();
    return w.out();
  }
};

template <typename Char>
struct formatter<year, Char> : private formatter<std::tm, Char> {
 private:
  bool use_tm_formatter_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    use_tm_formatter_ = it != end && *it != '}';
    return use_tm_formatter_ ? formatter<std::tm, Char>::parse(ctx) : it;
  }

  template <typename FormatContext>
  auto format(year y, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto time = std::tm();
    time.tm_year = static_cast<int>(y) - 1900;
    if (use_tm_formatter_) return formatter<std::tm, Char>::format(time, ctx);
    detail::get_locale loc(false, ctx.locale());
    auto w = detail::tm_writer<decltype(ctx.out()), Char>(loc, ctx.out(), time);
    w.on_year(detail::numeric_system::standard, detail::pad_type::zero);
    return w.out();
  }
};

template <typename Char>
struct formatter<year_month_day, Char> : private formatter<std::tm, Char> {
 private:
  bool use_tm_formatter_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    use_tm_formatter_ = it != end && *it != '}';
    return use_tm_formatter_ ? formatter<std::tm, Char>::parse(ctx) : it;
  }

  template <typename FormatContext>
  auto format(year_month_day val, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto time = std::tm();
    time.tm_year = static_cast<int>(val.year()) - 1900;
    time.tm_mon = static_cast<int>(static_cast<unsigned>(val.month())) - 1;
    time.tm_mday = static_cast<int>(static_cast<unsigned>(val.day()));
    if (use_tm_formatter_) return formatter<std::tm, Char>::format(time, ctx);
    detail::get_locale loc(true, ctx.locale());
    auto w = detail::tm_writer<decltype(ctx.out()), Char>(loc, ctx.out(), time);
    w.on_iso_date();
    return w.out();
  }
};

template <typename Rep, typename Period, typename Char>
struct formatter<std::chrono::duration<Rep, Period>, Char> {
 private:
  format_specs specs_;
  detail::arg_ref<Char> width_ref_;
  detail::arg_ref<Char> precision_ref_;
  basic_string_view<Char> fmt_;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end || *it == '}') return it;

    it = detail::parse_align(it, end, specs_);
    if (it == end) return it;

    Char c = *it;
    if ((c >= '0' && c <= '9') || c == '{') {
      it = detail::parse_width(it, end, specs_, width_ref_, ctx);
      if (it == end) return it;
    }

    auto checker = detail::chrono_format_checker();
    if (*it == '.') {
      checker.has_precision_integral = !std::is_floating_point<Rep>::value;
      it = detail::parse_precision(it, end, specs_, precision_ref_, ctx);
    }
    if (it != end && *it == 'L') {
      specs_.set_localized();
      ++it;
    }
    end = detail::parse_chrono_format(it, end, checker);
    fmt_ = {it, detail::to_unsigned(end - it)};
    return end;
  }

  template <typename FormatContext>
  auto format(std::chrono::duration<Rep, Period> d, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto specs = specs_;
    auto precision = specs.precision;
    specs.precision = -1;
    auto begin = fmt_.begin(), end = fmt_.end();
    // As a possible future optimization, we could avoid extra copying if width
    // is not specified.
    auto buf = basic_memory_buffer<Char>();
    auto out = basic_appender<Char>(buf);
    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width, width_ref_,
                                ctx);
    detail::handle_dynamic_spec(specs.dynamic_precision(), precision,
                                precision_ref_, ctx);
    if (begin == end || *begin == '}') {
      out = detail::format_duration_value<Char>(out, d.count(), precision);
      detail::format_duration_unit<Char, Period>(out);
    } else {
      auto f =
          detail::duration_formatter<Char, Rep, Period>(out, d, ctx.locale());
      f.precision = precision;
      f.localized = specs_.localized();
      detail::parse_chrono_format(begin, end, f);
    }
    return detail::write(
        ctx.out(), basic_string_view<Char>(buf.data(), buf.size()), specs);
  }
};

template <typename Char> struct formatter<std::tm, Char> {
 private:
  format_specs specs_;
  detail::arg_ref<Char> width_ref_;
  basic_string_view<Char> fmt_ =
      detail::string_literal<Char, '%', 'F', ' ', '%', 'T'>();

 protected:
  auto localized() const -> bool { return specs_.localized(); }
  FMT_CONSTEXPR void set_localized() { specs_.set_localized(); }

  FMT_CONSTEXPR auto do_parse(parse_context<Char>& ctx, bool has_timezone)
      -> const Char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end || *it == '}') return it;

    it = detail::parse_align(it, end, specs_);
    if (it == end) return it;

    Char c = *it;
    if ((c >= '0' && c <= '9') || c == '{') {
      it = detail::parse_width(it, end, specs_, width_ref_, ctx);
      if (it == end) return it;
    }

    if (*it == 'L') {
      specs_.set_localized();
      ++it;
    }

    end = detail::parse_chrono_format(it, end,
                                      detail::tm_format_checker(has_timezone));
    // Replace the default format string only if the new spec is not empty.
    if (end != it) fmt_ = {it, detail::to_unsigned(end - it)};
    return end;
  }

  template <typename Duration, typename FormatContext>
  auto do_format(const std::tm& tm, FormatContext& ctx,
                 const Duration* subsecs) const -> decltype(ctx.out()) {
    auto specs = specs_;
    auto buf = basic_memory_buffer<Char>();
    auto out = basic_appender<Char>(buf);
    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width, width_ref_,
                                ctx);

    auto loc_ref = specs.localized() ? ctx.locale() : locale_ref();
    detail::get_locale loc(static_cast<bool>(loc_ref), loc_ref);
    auto w = detail::tm_writer<basic_appender<Char>, Char, Duration>(
        loc, out, tm, subsecs);
    detail::parse_chrono_format(fmt_.begin(), fmt_.end(), w);
    return detail::write(
        ctx.out(), basic_string_view<Char>(buf.data(), buf.size()), specs);
  }

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return do_parse(ctx, detail::has_tm_gmtoff<std::tm>::value);
  }

  template <typename FormatContext>
  auto format(const std::tm& tm, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return do_format<std::chrono::seconds>(tm, ctx, nullptr);
  }
};

// DEPRECATED! Reversed order of template parameters.
template <typename Char, typename Duration>
struct formatter<sys_time<Duration>, Char> : private formatter<std::tm, Char> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return this->do_parse(ctx, true);
  }

  template <typename FormatContext>
  auto format(sys_time<Duration> val, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    std::tm tm = gmtime(val);
    using period = typename Duration::period;
    if (detail::const_check(
            period::num == 1 && period::den == 1 &&
            !std::is_floating_point<typename Duration::rep>::value)) {
      detail::set_tm_zone(tm, detail::utc());
      return formatter<std::tm, Char>::format(tm, ctx);
    }
    Duration epoch = val.time_since_epoch();
    Duration subsecs = detail::duration_cast<Duration>(
        epoch - detail::duration_cast<std::chrono::seconds>(epoch));
    if (subsecs.count() < 0) {
      auto second = detail::duration_cast<Duration>(std::chrono::seconds(1));
      if (tm.tm_sec != 0) {
        --tm.tm_sec;
      } else {
        tm = gmtime(val - second);
        detail::set_tm_zone(tm, detail::utc());
      }
      subsecs += second;
    }
    return formatter<std::tm, Char>::do_format(tm, ctx, &subsecs);
  }
};

template <typename Duration, typename Char>
struct formatter<utc_time<Duration>, Char>
    : formatter<sys_time<Duration>, Char> {
  template <typename FormatContext>
  auto format(utc_time<Duration> val, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<sys_time<Duration>, Char>::format(
        detail::utc_clock::to_sys(val), ctx);
  }
};

template <typename Duration, typename Char>
struct formatter<local_time<Duration>, Char>
    : private formatter<std::tm, Char> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return this->do_parse(ctx, false);
  }

  template <typename FormatContext>
  auto format(local_time<Duration> val, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto time_since_epoch = val.time_since_epoch();
    auto seconds_since_epoch =
        detail::duration_cast<std::chrono::seconds>(time_since_epoch);
    // Use gmtime to prevent time zone conversion since local_time has an
    // unspecified time zone.
    std::tm t = gmtime(seconds_since_epoch.count());
    using period = typename Duration::period;
    if (period::num == 1 && period::den == 1 &&
        !std::is_floating_point<typename Duration::rep>::value) {
      return formatter<std::tm, Char>::format(t, ctx);
    }
    auto subsecs =
        detail::duration_cast<Duration>(time_since_epoch - seconds_since_epoch);
    return formatter<std::tm, Char>::do_format(t, ctx, &subsecs);
  }
};

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_CHRONO_H_
