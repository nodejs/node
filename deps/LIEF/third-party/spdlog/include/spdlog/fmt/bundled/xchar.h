// Formatting library for C++ - optional wchar_t and exotic character support
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_XCHAR_H_
#define FMT_XCHAR_H_

#include "color.h"
#include "format.h"
#include "ostream.h"
#include "ranges.h"

#ifndef FMT_MODULE
#  include <cwchar>
#  if FMT_USE_LOCALE
#    include <locale>
#  endif
#endif

FMT_BEGIN_NAMESPACE
namespace detail {

template <typename T>
using is_exotic_char = bool_constant<!std::is_same<T, char>::value>;

template <typename S, typename = void> struct format_string_char {};

template <typename S>
struct format_string_char<
    S, void_t<decltype(sizeof(detail::to_string_view(std::declval<S>())))>> {
  using type = char_t<S>;
};

template <typename S>
struct format_string_char<
    S, enable_if_t<std::is_base_of<detail::compile_string, S>::value>> {
  using type = typename S::char_type;
};

template <typename S>
using format_string_char_t = typename format_string_char<S>::type;

inline auto write_loc(basic_appender<wchar_t> out, loc_value value,
                      const format_specs& specs, locale_ref loc) -> bool {
#if FMT_USE_LOCALE
  auto& numpunct =
      std::use_facet<std::numpunct<wchar_t>>(loc.get<std::locale>());
  auto separator = std::wstring();
  auto grouping = numpunct.grouping();
  if (!grouping.empty()) separator = std::wstring(1, numpunct.thousands_sep());
  return value.visit(loc_writer<wchar_t>{out, specs, separator, grouping, {}});
#endif
  return false;
}

template <typename Char>
void vformat_to(buffer<Char>& buf, basic_string_view<Char> fmt,
                basic_format_args<buffered_context<Char>> args,
                locale_ref loc = {}) {
  static_assert(!std::is_same<Char, char>::value, "");
  auto out = basic_appender<Char>(buf);
  parse_format_string(
      fmt, format_handler<Char>{parse_context<Char>(fmt), {out, args, loc}});
}
}  // namespace detail

FMT_BEGIN_EXPORT

using wstring_view = basic_string_view<wchar_t>;
using wformat_parse_context = parse_context<wchar_t>;
using wformat_context = buffered_context<wchar_t>;
using wformat_args = basic_format_args<wformat_context>;
using wmemory_buffer = basic_memory_buffer<wchar_t>;

template <typename Char, typename... T> struct basic_fstring {
 private:
  basic_string_view<Char> str_;

  static constexpr int num_static_named_args =
      detail::count_static_named_args<T...>();

  using checker = detail::format_string_checker<
      Char, static_cast<int>(sizeof...(T)), num_static_named_args,
      num_static_named_args != detail::count_named_args<T...>()>;

  using arg_pack = detail::arg_pack<T...>;

 public:
  using t = basic_fstring;

  template <typename S,
            FMT_ENABLE_IF(
                std::is_convertible<const S&, basic_string_view<Char>>::value)>
  FMT_CONSTEVAL FMT_ALWAYS_INLINE basic_fstring(const S& s) : str_(s) {
    if (FMT_USE_CONSTEVAL)
      detail::parse_format_string<Char>(s, checker(s, arg_pack()));
  }
  template <typename S,
            FMT_ENABLE_IF(std::is_base_of<detail::compile_string, S>::value&&
                              std::is_same<typename S::char_type, Char>::value)>
  FMT_ALWAYS_INLINE basic_fstring(const S&) : str_(S()) {
    FMT_CONSTEXPR auto sv = basic_string_view<Char>(S());
    FMT_CONSTEXPR int ignore =
        (parse_format_string(sv, checker(sv, arg_pack())), 0);
    detail::ignore_unused(ignore);
  }
  basic_fstring(runtime_format_string<Char> fmt) : str_(fmt.str) {}

  operator basic_string_view<Char>() const { return str_; }
  auto get() const -> basic_string_view<Char> { return str_; }
};

template <typename Char, typename... T>
using basic_format_string = basic_fstring<Char, T...>;

template <typename... T>
using wformat_string = typename basic_format_string<wchar_t, T...>::t;
inline auto runtime(wstring_view s) -> runtime_format_string<wchar_t> {
  return {{s}};
}

template <typename... T>
constexpr auto make_wformat_args(T&... args)
    -> decltype(fmt::make_format_args<wformat_context>(args...)) {
  return fmt::make_format_args<wformat_context>(args...);
}

#if !FMT_USE_NONTYPE_TEMPLATE_ARGS
inline namespace literals {
inline auto operator""_a(const wchar_t* s, size_t) -> detail::udl_arg<wchar_t> {
  return {s};
}
}  // namespace literals
#endif

template <typename It, typename Sentinel>
auto join(It begin, Sentinel end, wstring_view sep)
    -> join_view<It, Sentinel, wchar_t> {
  return {begin, end, sep};
}

template <typename Range, FMT_ENABLE_IF(!is_tuple_like<Range>::value)>
auto join(Range&& range, wstring_view sep)
    -> join_view<decltype(std::begin(range)), decltype(std::end(range)),
                 wchar_t> {
  return join(std::begin(range), std::end(range), sep);
}

template <typename T>
auto join(std::initializer_list<T> list, wstring_view sep)
    -> join_view<const T*, const T*, wchar_t> {
  return join(std::begin(list), std::end(list), sep);
}

template <typename Tuple, FMT_ENABLE_IF(is_tuple_like<Tuple>::value)>
auto join(const Tuple& tuple, basic_string_view<wchar_t> sep)
    -> tuple_join_view<Tuple, wchar_t> {
  return {tuple, sep};
}

template <typename Char, FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
auto vformat(basic_string_view<Char> fmt,
             basic_format_args<buffered_context<Char>> args)
    -> std::basic_string<Char> {
  auto buf = basic_memory_buffer<Char>();
  detail::vformat_to(buf, fmt, args);
  return {buf.data(), buf.size()};
}

template <typename... T>
auto format(wformat_string<T...> fmt, T&&... args) -> std::wstring {
  return vformat(fmt::wstring_view(fmt), fmt::make_wformat_args(args...));
}

template <typename OutputIt, typename... T>
auto format_to(OutputIt out, wformat_string<T...> fmt, T&&... args)
    -> OutputIt {
  return vformat_to(out, fmt::wstring_view(fmt),
                    fmt::make_wformat_args(args...));
}

// Pass char_t as a default template parameter instead of using
// std::basic_string<char_t<S>> to reduce the symbol size.
template <typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(!std::is_same<Char, char>::value &&
                        !std::is_same<Char, wchar_t>::value)>
auto format(const S& fmt, T&&... args) -> std::basic_string<Char> {
  return vformat(detail::to_string_view(fmt),
                 fmt::make_format_args<buffered_context<Char>>(args...));
}

template <typename S, typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_exotic_char<Char>::value)>
inline auto vformat(locale_ref loc, const S& fmt,
                    basic_format_args<buffered_context<Char>> args)
    -> std::basic_string<Char> {
  auto buf = basic_memory_buffer<Char>();
  detail::vformat_to(buf, detail::to_string_view(fmt), args, loc);
  return {buf.data(), buf.size()};
}

template <typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_exotic_char<Char>::value)>
inline auto format(locale_ref loc, const S& fmt, T&&... args)
    -> std::basic_string<Char> {
  return vformat(loc, detail::to_string_view(fmt),
                 fmt::make_format_args<buffered_context<Char>>(args...));
}

template <typename OutputIt, typename S,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
auto vformat_to(OutputIt out, const S& fmt,
                basic_format_args<buffered_context<Char>> args) -> OutputIt {
  auto&& buf = detail::get_buffer<Char>(out);
  detail::vformat_to(buf, detail::to_string_view(fmt), args);
  return detail::get_iterator(buf, out);
}

template <typename OutputIt, typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value &&
                        !std::is_same<Char, char>::value &&
                        !std::is_same<Char, wchar_t>::value)>
inline auto format_to(OutputIt out, const S& fmt, T&&... args) -> OutputIt {
  return vformat_to(out, detail::to_string_view(fmt),
                    fmt::make_format_args<buffered_context<Char>>(args...));
}

template <typename S, typename OutputIt, typename... Args,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto vformat_to(OutputIt out, locale_ref loc, const S& fmt,
                       basic_format_args<buffered_context<Char>> args)
    -> OutputIt {
  auto&& buf = detail::get_buffer<Char>(out);
  vformat_to(buf, detail::to_string_view(fmt), args, loc);
  return detail::get_iterator(buf, out);
}

template <typename OutputIt, typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          bool enable = detail::is_output_iterator<OutputIt, Char>::value &&
                        detail::is_exotic_char<Char>::value>
inline auto format_to(OutputIt out, locale_ref loc, const S& fmt, T&&... args)
    -> typename std::enable_if<enable, OutputIt>::type {
  return vformat_to(out, loc, detail::to_string_view(fmt),
                    fmt::make_format_args<buffered_context<Char>>(args...));
}

template <typename OutputIt, typename Char, typename... Args,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto vformat_to_n(OutputIt out, size_t n, basic_string_view<Char> fmt,
                         basic_format_args<buffered_context<Char>> args)
    -> format_to_n_result<OutputIt> {
  using traits = detail::fixed_buffer_traits;
  auto buf = detail::iterator_buffer<OutputIt, Char, traits>(out, n);
  detail::vformat_to(buf, fmt, args);
  return {buf.out(), buf.count()};
}

template <typename OutputIt, typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, Char>::value&&
                            detail::is_exotic_char<Char>::value)>
inline auto format_to_n(OutputIt out, size_t n, const S& fmt, T&&... args)
    -> format_to_n_result<OutputIt> {
  return vformat_to_n(out, n, fmt::basic_string_view<Char>(fmt),
                      fmt::make_format_args<buffered_context<Char>>(args...));
}

template <typename S, typename... T,
          typename Char = detail::format_string_char_t<S>,
          FMT_ENABLE_IF(detail::is_exotic_char<Char>::value)>
inline auto formatted_size(const S& fmt, T&&... args) -> size_t {
  auto buf = detail::counting_buffer<Char>();
  detail::vformat_to(buf, detail::to_string_view(fmt),
                     fmt::make_format_args<buffered_context<Char>>(args...));
  return buf.count();
}

inline void vprint(std::FILE* f, wstring_view fmt, wformat_args args) {
  auto buf = wmemory_buffer();
  detail::vformat_to(buf, fmt, args);
  buf.push_back(L'\0');
  if (std::fputws(buf.data(), f) == -1)
    FMT_THROW(system_error(errno, FMT_STRING("cannot write to file")));
}

inline void vprint(wstring_view fmt, wformat_args args) {
  vprint(stdout, fmt, args);
}

template <typename... T>
void print(std::FILE* f, wformat_string<T...> fmt, T&&... args) {
  return vprint(f, wstring_view(fmt), fmt::make_wformat_args(args...));
}

template <typename... T> void print(wformat_string<T...> fmt, T&&... args) {
  return vprint(wstring_view(fmt), fmt::make_wformat_args(args...));
}

template <typename... T>
void println(std::FILE* f, wformat_string<T...> fmt, T&&... args) {
  return print(f, L"{}\n", fmt::format(fmt, std::forward<T>(args)...));
}

template <typename... T> void println(wformat_string<T...> fmt, T&&... args) {
  return print(L"{}\n", fmt::format(fmt, std::forward<T>(args)...));
}

inline auto vformat(text_style ts, wstring_view fmt, wformat_args args)
    -> std::wstring {
  auto buf = wmemory_buffer();
  detail::vformat_to(buf, ts, fmt, args);
  return {buf.data(), buf.size()};
}

template <typename... T>
inline auto format(text_style ts, wformat_string<T...> fmt, T&&... args)
    -> std::wstring {
  return fmt::vformat(ts, fmt, fmt::make_wformat_args(args...));
}

inline void vprint(std::wostream& os, wstring_view fmt, wformat_args args) {
  auto buffer = basic_memory_buffer<wchar_t>();
  detail::vformat_to(buffer, fmt, args);
  detail::write_buffer(os, buffer);
}

template <typename... T>
void print(std::wostream& os, wformat_string<T...> fmt, T&&... args) {
  vprint(os, fmt, fmt::make_format_args<buffered_context<wchar_t>>(args...));
}

template <typename... T>
void println(std::wostream& os, wformat_string<T...> fmt, T&&... args) {
  print(os, L"{}\n", fmt::format(fmt, std::forward<T>(args)...));
}

/// Converts `value` to `std::wstring` using the default format for type `T`.
template <typename T> inline auto to_wstring(const T& value) -> std::wstring {
  return format(FMT_STRING(L"{}"), value);
}
FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_XCHAR_H_
