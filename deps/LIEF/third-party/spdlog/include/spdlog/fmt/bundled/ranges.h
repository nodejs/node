// Formatting library for C++ - range and tuple support
//
// Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_RANGES_H_
#define FMT_RANGES_H_

#ifndef FMT_MODULE
#  include <initializer_list>
#  include <iterator>
#  include <string>
#  include <tuple>
#  include <type_traits>
#  include <utility>
#endif

#include "format.h"

FMT_BEGIN_NAMESPACE

FMT_EXPORT
enum class range_format { disabled, map, set, sequence, string, debug_string };

namespace detail {

template <typename T> class is_map {
  template <typename U> static auto check(U*) -> typename U::mapped_type;
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value;
};

template <typename T> class is_set {
  template <typename U> static auto check(U*) -> typename U::key_type;
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value && !is_map<T>::value;
};

// C array overload
template <typename T, std::size_t N>
auto range_begin(const T (&arr)[N]) -> const T* {
  return arr;
}
template <typename T, std::size_t N>
auto range_end(const T (&arr)[N]) -> const T* {
  return arr + N;
}

template <typename T, typename Enable = void>
struct has_member_fn_begin_end_t : std::false_type {};

template <typename T>
struct has_member_fn_begin_end_t<T, void_t<decltype(*std::declval<T>().begin()),
                                           decltype(std::declval<T>().end())>>
    : std::true_type {};

// Member function overloads.
template <typename T>
auto range_begin(T&& rng) -> decltype(static_cast<T&&>(rng).begin()) {
  return static_cast<T&&>(rng).begin();
}
template <typename T>
auto range_end(T&& rng) -> decltype(static_cast<T&&>(rng).end()) {
  return static_cast<T&&>(rng).end();
}

// ADL overloads. Only participate in overload resolution if member functions
// are not found.
template <typename T>
auto range_begin(T&& rng)
    -> enable_if_t<!has_member_fn_begin_end_t<T&&>::value,
                   decltype(begin(static_cast<T&&>(rng)))> {
  return begin(static_cast<T&&>(rng));
}
template <typename T>
auto range_end(T&& rng) -> enable_if_t<!has_member_fn_begin_end_t<T&&>::value,
                                       decltype(end(static_cast<T&&>(rng)))> {
  return end(static_cast<T&&>(rng));
}

template <typename T, typename Enable = void>
struct has_const_begin_end : std::false_type {};
template <typename T, typename Enable = void>
struct has_mutable_begin_end : std::false_type {};

template <typename T>
struct has_const_begin_end<
    T, void_t<decltype(*detail::range_begin(
                  std::declval<const remove_cvref_t<T>&>())),
              decltype(detail::range_end(
                  std::declval<const remove_cvref_t<T>&>()))>>
    : std::true_type {};

template <typename T>
struct has_mutable_begin_end<
    T, void_t<decltype(*detail::range_begin(std::declval<T&>())),
              decltype(detail::range_end(std::declval<T&>())),
              // the extra int here is because older versions of MSVC don't
              // SFINAE properly unless there are distinct types
              int>> : std::true_type {};

template <typename T, typename _ = void> struct is_range_ : std::false_type {};
template <typename T>
struct is_range_<T, void>
    : std::integral_constant<bool, (has_const_begin_end<T>::value ||
                                    has_mutable_begin_end<T>::value)> {};

// tuple_size and tuple_element check.
template <typename T> class is_tuple_like_ {
  template <typename U, typename V = typename std::remove_cv<U>::type>
  static auto check(U* p) -> decltype(std::tuple_size<V>::value, 0);
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value;
};

// Check for integer_sequence
#if defined(__cpp_lib_integer_sequence) || FMT_MSC_VERSION >= 1900
template <typename T, T... N>
using integer_sequence = std::integer_sequence<T, N...>;
template <size_t... N> using index_sequence = std::index_sequence<N...>;
template <size_t N> using make_index_sequence = std::make_index_sequence<N>;
#else
template <typename T, T... N> struct integer_sequence {
  using value_type = T;

  static FMT_CONSTEXPR auto size() -> size_t { return sizeof...(N); }
};

template <size_t... N> using index_sequence = integer_sequence<size_t, N...>;

template <typename T, size_t N, T... Ns>
struct make_integer_sequence : make_integer_sequence<T, N - 1, N - 1, Ns...> {};
template <typename T, T... Ns>
struct make_integer_sequence<T, 0, Ns...> : integer_sequence<T, Ns...> {};

template <size_t N>
using make_index_sequence = make_integer_sequence<size_t, N>;
#endif

template <typename T>
using tuple_index_sequence = make_index_sequence<std::tuple_size<T>::value>;

template <typename T, typename C, bool = is_tuple_like_<T>::value>
class is_tuple_formattable_ {
 public:
  static constexpr const bool value = false;
};
template <typename T, typename C> class is_tuple_formattable_<T, C, true> {
  template <size_t... Is>
  static auto all_true(index_sequence<Is...>,
                       integer_sequence<bool, (Is >= 0)...>) -> std::true_type;
  static auto all_true(...) -> std::false_type;

  template <size_t... Is>
  static auto check(index_sequence<Is...>) -> decltype(all_true(
      index_sequence<Is...>{},
      integer_sequence<bool,
                       (is_formattable<typename std::tuple_element<Is, T>::type,
                                       C>::value)...>{}));

 public:
  static constexpr const bool value =
      decltype(check(tuple_index_sequence<T>{}))::value;
};

template <typename Tuple, typename F, size_t... Is>
FMT_CONSTEXPR void for_each(index_sequence<Is...>, Tuple&& t, F&& f) {
  using std::get;
  // Using a free function get<Is>(Tuple) now.
  const int unused[] = {0, ((void)f(get<Is>(t)), 0)...};
  ignore_unused(unused);
}

template <typename Tuple, typename F>
FMT_CONSTEXPR void for_each(Tuple&& t, F&& f) {
  for_each(tuple_index_sequence<remove_cvref_t<Tuple>>(),
           std::forward<Tuple>(t), std::forward<F>(f));
}

template <typename Tuple1, typename Tuple2, typename F, size_t... Is>
void for_each2(index_sequence<Is...>, Tuple1&& t1, Tuple2&& t2, F&& f) {
  using std::get;
  const int unused[] = {0, ((void)f(get<Is>(t1), get<Is>(t2)), 0)...};
  ignore_unused(unused);
}

template <typename Tuple1, typename Tuple2, typename F>
void for_each2(Tuple1&& t1, Tuple2&& t2, F&& f) {
  for_each2(tuple_index_sequence<remove_cvref_t<Tuple1>>(),
            std::forward<Tuple1>(t1), std::forward<Tuple2>(t2),
            std::forward<F>(f));
}

namespace tuple {
// Workaround a bug in MSVC 2019 (v140).
template <typename Char, typename... T>
using result_t = std::tuple<formatter<remove_cvref_t<T>, Char>...>;

using std::get;
template <typename Tuple, typename Char, std::size_t... Is>
auto get_formatters(index_sequence<Is...>)
    -> result_t<Char, decltype(get<Is>(std::declval<Tuple>()))...>;
}  // namespace tuple

#if FMT_MSC_VERSION && FMT_MSC_VERSION < 1920
// Older MSVC doesn't get the reference type correctly for arrays.
template <typename R> struct range_reference_type_impl {
  using type = decltype(*detail::range_begin(std::declval<R&>()));
};

template <typename T, std::size_t N> struct range_reference_type_impl<T[N]> {
  using type = T&;
};

template <typename T>
using range_reference_type = typename range_reference_type_impl<T>::type;
#else
template <typename Range>
using range_reference_type =
    decltype(*detail::range_begin(std::declval<Range&>()));
#endif

// We don't use the Range's value_type for anything, but we do need the Range's
// reference type, with cv-ref stripped.
template <typename Range>
using uncvref_type = remove_cvref_t<range_reference_type<Range>>;

template <typename Formatter>
FMT_CONSTEXPR auto maybe_set_debug_format(Formatter& f, bool set)
    -> decltype(f.set_debug_format(set)) {
  f.set_debug_format(set);
}
template <typename Formatter>
FMT_CONSTEXPR void maybe_set_debug_format(Formatter&, ...) {}

template <typename T>
struct range_format_kind_
    : std::integral_constant<range_format,
                             std::is_same<uncvref_type<T>, T>::value
                                 ? range_format::disabled
                             : is_map<T>::value ? range_format::map
                             : is_set<T>::value ? range_format::set
                                                : range_format::sequence> {};

template <range_format K>
using range_format_constant = std::integral_constant<range_format, K>;

// These are not generic lambdas for compatibility with C++11.
template <typename Char> struct parse_empty_specs {
  template <typename Formatter> FMT_CONSTEXPR void operator()(Formatter& f) {
    f.parse(ctx);
    detail::maybe_set_debug_format(f, true);
  }
  parse_context<Char>& ctx;
};
template <typename FormatContext> struct format_tuple_element {
  using char_type = typename FormatContext::char_type;

  template <typename T>
  void operator()(const formatter<T, char_type>& f, const T& v) {
    if (i > 0) ctx.advance_to(detail::copy<char_type>(separator, ctx.out()));
    ctx.advance_to(f.format(v, ctx));
    ++i;
  }

  int i;
  FormatContext& ctx;
  basic_string_view<char_type> separator;
};

}  // namespace detail

template <typename T> struct is_tuple_like {
  static constexpr const bool value =
      detail::is_tuple_like_<T>::value && !detail::is_range_<T>::value;
};

template <typename T, typename C> struct is_tuple_formattable {
  static constexpr const bool value =
      detail::is_tuple_formattable_<T, C>::value;
};

template <typename Tuple, typename Char>
struct formatter<Tuple, Char,
                 enable_if_t<fmt::is_tuple_like<Tuple>::value &&
                             fmt::is_tuple_formattable<Tuple, Char>::value>> {
 private:
  decltype(detail::tuple::get_formatters<Tuple, Char>(
      detail::tuple_index_sequence<Tuple>())) formatters_;

  basic_string_view<Char> separator_ = detail::string_literal<Char, ',', ' '>{};
  basic_string_view<Char> opening_bracket_ =
      detail::string_literal<Char, '('>{};
  basic_string_view<Char> closing_bracket_ =
      detail::string_literal<Char, ')'>{};

 public:
  FMT_CONSTEXPR formatter() {}

  FMT_CONSTEXPR void set_separator(basic_string_view<Char> sep) {
    separator_ = sep;
  }

  FMT_CONSTEXPR void set_brackets(basic_string_view<Char> open,
                                  basic_string_view<Char> close) {
    opening_bracket_ = open;
    closing_bracket_ = close;
  }

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it != end && detail::to_ascii(*it) == 'n') {
      ++it;
      set_brackets({}, {});
      set_separator({});
    }
    if (it != end && *it != '}') report_error("invalid format specifier");
    ctx.advance_to(it);
    detail::for_each(formatters_, detail::parse_empty_specs<Char>{ctx});
    return it;
  }

  template <typename FormatContext>
  auto format(const Tuple& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    ctx.advance_to(detail::copy<Char>(opening_bracket_, ctx.out()));
    detail::for_each2(
        formatters_, value,
        detail::format_tuple_element<FormatContext>{0, ctx, separator_});
    return detail::copy<Char>(closing_bracket_, ctx.out());
  }
};

template <typename T, typename Char> struct is_range {
  static constexpr const bool value =
      detail::is_range_<T>::value && !detail::has_to_string_view<T>::value;
};

namespace detail {

template <typename Char, typename Element>
using range_formatter_type = formatter<remove_cvref_t<Element>, Char>;

template <typename R>
using maybe_const_range =
    conditional_t<has_const_begin_end<R>::value, const R, R>;

template <typename R, typename Char>
struct is_formattable_delayed
    : is_formattable<uncvref_type<maybe_const_range<R>>, Char> {};
}  // namespace detail

template <typename...> struct conjunction : std::true_type {};
template <typename P> struct conjunction<P> : P {};
template <typename P1, typename... Pn>
struct conjunction<P1, Pn...>
    : conditional_t<bool(P1::value), conjunction<Pn...>, P1> {};

template <typename T, typename Char, typename Enable = void>
struct range_formatter;

template <typename T, typename Char>
struct range_formatter<
    T, Char,
    enable_if_t<conjunction<std::is_same<T, remove_cvref_t<T>>,
                            is_formattable<T, Char>>::value>> {
 private:
  detail::range_formatter_type<Char, T> underlying_;
  basic_string_view<Char> separator_ = detail::string_literal<Char, ',', ' '>{};
  basic_string_view<Char> opening_bracket_ =
      detail::string_literal<Char, '['>{};
  basic_string_view<Char> closing_bracket_ =
      detail::string_literal<Char, ']'>{};
  bool is_debug = false;

  template <typename Output, typename It, typename Sentinel, typename U = T,
            FMT_ENABLE_IF(std::is_same<U, Char>::value)>
  auto write_debug_string(Output& out, It it, Sentinel end) const -> Output {
    auto buf = basic_memory_buffer<Char>();
    for (; it != end; ++it) buf.push_back(*it);
    auto specs = format_specs();
    specs.set_type(presentation_type::debug);
    return detail::write<Char>(
        out, basic_string_view<Char>(buf.data(), buf.size()), specs);
  }

  template <typename Output, typename It, typename Sentinel, typename U = T,
            FMT_ENABLE_IF(!std::is_same<U, Char>::value)>
  auto write_debug_string(Output& out, It, Sentinel) const -> Output {
    return out;
  }

 public:
  FMT_CONSTEXPR range_formatter() {}

  FMT_CONSTEXPR auto underlying() -> detail::range_formatter_type<Char, T>& {
    return underlying_;
  }

  FMT_CONSTEXPR void set_separator(basic_string_view<Char> sep) {
    separator_ = sep;
  }

  FMT_CONSTEXPR void set_brackets(basic_string_view<Char> open,
                                  basic_string_view<Char> close) {
    opening_bracket_ = open;
    closing_bracket_ = close;
  }

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin();
    auto end = ctx.end();
    detail::maybe_set_debug_format(underlying_, true);
    if (it == end) return underlying_.parse(ctx);

    switch (detail::to_ascii(*it)) {
    case 'n':
      set_brackets({}, {});
      ++it;
      break;
    case '?':
      is_debug = true;
      set_brackets({}, {});
      ++it;
      if (it == end || *it != 's') report_error("invalid format specifier");
      FMT_FALLTHROUGH;
    case 's':
      if (!std::is_same<T, Char>::value)
        report_error("invalid format specifier");
      if (!is_debug) {
        set_brackets(detail::string_literal<Char, '"'>{},
                     detail::string_literal<Char, '"'>{});
        set_separator({});
        detail::maybe_set_debug_format(underlying_, false);
      }
      ++it;
      return it;
    }

    if (it != end && *it != '}') {
      if (*it != ':') report_error("invalid format specifier");
      detail::maybe_set_debug_format(underlying_, false);
      ++it;
    }

    ctx.advance_to(it);
    return underlying_.parse(ctx);
  }

  template <typename R, typename FormatContext>
  auto format(R&& range, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    auto it = detail::range_begin(range);
    auto end = detail::range_end(range);
    if (is_debug) return write_debug_string(out, std::move(it), end);

    out = detail::copy<Char>(opening_bracket_, out);
    int i = 0;
    for (; it != end; ++it) {
      if (i > 0) out = detail::copy<Char>(separator_, out);
      ctx.advance_to(out);
      auto&& item = *it;  // Need an lvalue
      out = underlying_.format(item, ctx);
      ++i;
    }
    out = detail::copy<Char>(closing_bracket_, out);
    return out;
  }
};

FMT_EXPORT
template <typename T, typename Char, typename Enable = void>
struct range_format_kind
    : conditional_t<
          is_range<T, Char>::value, detail::range_format_kind_<T>,
          std::integral_constant<range_format, range_format::disabled>> {};

template <typename R, typename Char>
struct formatter<
    R, Char,
    enable_if_t<conjunction<
        bool_constant<
            range_format_kind<R, Char>::value != range_format::disabled &&
            range_format_kind<R, Char>::value != range_format::map &&
            range_format_kind<R, Char>::value != range_format::string &&
            range_format_kind<R, Char>::value != range_format::debug_string>,
        detail::is_formattable_delayed<R, Char>>::value>> {
 private:
  using range_type = detail::maybe_const_range<R>;
  range_formatter<detail::uncvref_type<range_type>, Char> range_formatter_;

 public:
  using nonlocking = void;

  FMT_CONSTEXPR formatter() {
    if (detail::const_check(range_format_kind<R, Char>::value !=
                            range_format::set))
      return;
    range_formatter_.set_brackets(detail::string_literal<Char, '{'>{},
                                  detail::string_literal<Char, '}'>{});
  }

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return range_formatter_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(range_type& range, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return range_formatter_.format(range, ctx);
  }
};

// A map formatter.
template <typename R, typename Char>
struct formatter<
    R, Char,
    enable_if_t<conjunction<
        bool_constant<range_format_kind<R, Char>::value == range_format::map>,
        detail::is_formattable_delayed<R, Char>>::value>> {
 private:
  using map_type = detail::maybe_const_range<R>;
  using element_type = detail::uncvref_type<map_type>;

  decltype(detail::tuple::get_formatters<element_type, Char>(
      detail::tuple_index_sequence<element_type>())) formatters_;
  bool no_delimiters_ = false;

 public:
  FMT_CONSTEXPR formatter() {}

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it != end) {
      if (detail::to_ascii(*it) == 'n') {
        no_delimiters_ = true;
        ++it;
      }
      if (it != end && *it != '}') {
        if (*it != ':') report_error("invalid format specifier");
        ++it;
      }
      ctx.advance_to(it);
    }
    detail::for_each(formatters_, detail::parse_empty_specs<Char>{ctx});
    return it;
  }

  template <typename FormatContext>
  auto format(map_type& map, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto out = ctx.out();
    basic_string_view<Char> open = detail::string_literal<Char, '{'>{};
    if (!no_delimiters_) out = detail::copy<Char>(open, out);
    int i = 0;
    basic_string_view<Char> sep = detail::string_literal<Char, ',', ' '>{};
    for (auto&& value : map) {
      if (i > 0) out = detail::copy<Char>(sep, out);
      ctx.advance_to(out);
      detail::for_each2(formatters_, value,
                        detail::format_tuple_element<FormatContext>{
                            0, ctx, detail::string_literal<Char, ':', ' '>{}});
      ++i;
    }
    basic_string_view<Char> close = detail::string_literal<Char, '}'>{};
    if (!no_delimiters_) out = detail::copy<Char>(close, out);
    return out;
  }
};

// A (debug_)string formatter.
template <typename R, typename Char>
struct formatter<
    R, Char,
    enable_if_t<range_format_kind<R, Char>::value == range_format::string ||
                range_format_kind<R, Char>::value ==
                    range_format::debug_string>> {
 private:
  using range_type = detail::maybe_const_range<R>;
  using string_type =
      conditional_t<std::is_constructible<
                        detail::std_string_view<Char>,
                        decltype(detail::range_begin(std::declval<R>())),
                        decltype(detail::range_end(std::declval<R>()))>::value,
                    detail::std_string_view<Char>, std::basic_string<Char>>;

  formatter<string_type, Char> underlying_;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return underlying_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(range_type& range, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();
    if (detail::const_check(range_format_kind<R, Char>::value ==
                            range_format::debug_string))
      *out++ = '"';
    out = underlying_.format(
        string_type{detail::range_begin(range), detail::range_end(range)}, ctx);
    if (detail::const_check(range_format_kind<R, Char>::value ==
                            range_format::debug_string))
      *out++ = '"';
    return out;
  }
};

template <typename It, typename Sentinel, typename Char = char>
struct join_view : detail::view {
  It begin;
  Sentinel end;
  basic_string_view<Char> sep;

  join_view(It b, Sentinel e, basic_string_view<Char> s)
      : begin(std::move(b)), end(e), sep(s) {}
};

template <typename It, typename Sentinel, typename Char>
struct formatter<join_view<It, Sentinel, Char>, Char> {
 private:
  using value_type =
#ifdef __cpp_lib_ranges
      std::iter_value_t<It>;
#else
      typename std::iterator_traits<It>::value_type;
#endif
  formatter<remove_cvref_t<value_type>, Char> value_formatter_;

  using view = conditional_t<std::is_copy_constructible<It>::value,
                             const join_view<It, Sentinel, Char>,
                             join_view<It, Sentinel, Char>>;

 public:
  using nonlocking = void;

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return value_formatter_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(view& value, FormatContext& ctx) const -> decltype(ctx.out()) {
    using iter =
        conditional_t<std::is_copy_constructible<view>::value, It, It&>;
    iter it = value.begin;
    auto out = ctx.out();
    if (it == value.end) return out;
    out = value_formatter_.format(*it, ctx);
    ++it;
    while (it != value.end) {
      out = detail::copy<Char>(value.sep.begin(), value.sep.end(), out);
      ctx.advance_to(out);
      out = value_formatter_.format(*it, ctx);
      ++it;
    }
    return out;
  }
};

template <typename Char, typename Tuple> struct tuple_join_view : detail::view {
  const Tuple& tuple;
  basic_string_view<Char> sep;

  tuple_join_view(const Tuple& t, basic_string_view<Char> s)
      : tuple(t), sep{s} {}
};

// Define FMT_TUPLE_JOIN_SPECIFIERS to enable experimental format specifiers
// support in tuple_join. It is disabled by default because of issues with
// the dynamic width and precision.
#ifndef FMT_TUPLE_JOIN_SPECIFIERS
#  define FMT_TUPLE_JOIN_SPECIFIERS 0
#endif

template <typename Char, typename Tuple>
struct formatter<tuple_join_view<Char, Tuple>, Char,
                 enable_if_t<is_tuple_like<Tuple>::value>> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return do_parse(ctx, std::tuple_size<Tuple>());
  }

  template <typename FormatContext>
  auto format(const tuple_join_view<Char, Tuple>& value,
              FormatContext& ctx) const -> typename FormatContext::iterator {
    return do_format(value, ctx, std::tuple_size<Tuple>());
  }

 private:
  decltype(detail::tuple::get_formatters<Tuple, Char>(
      detail::tuple_index_sequence<Tuple>())) formatters_;

  FMT_CONSTEXPR auto do_parse(parse_context<Char>& ctx,
                              std::integral_constant<size_t, 0>)
      -> const Char* {
    return ctx.begin();
  }

  template <size_t N>
  FMT_CONSTEXPR auto do_parse(parse_context<Char>& ctx,
                              std::integral_constant<size_t, N>)
      -> const Char* {
    auto end = ctx.begin();
#if FMT_TUPLE_JOIN_SPECIFIERS
    end = std::get<std::tuple_size<Tuple>::value - N>(formatters_).parse(ctx);
    if (N > 1) {
      auto end1 = do_parse(ctx, std::integral_constant<size_t, N - 1>());
      if (end != end1)
        report_error("incompatible format specs for tuple elements");
    }
#endif
    return end;
  }

  template <typename FormatContext>
  auto do_format(const tuple_join_view<Char, Tuple>&, FormatContext& ctx,
                 std::integral_constant<size_t, 0>) const ->
      typename FormatContext::iterator {
    return ctx.out();
  }

  template <typename FormatContext, size_t N>
  auto do_format(const tuple_join_view<Char, Tuple>& value, FormatContext& ctx,
                 std::integral_constant<size_t, N>) const ->
      typename FormatContext::iterator {
    using std::get;
    auto out =
        std::get<std::tuple_size<Tuple>::value - N>(formatters_)
            .format(get<std::tuple_size<Tuple>::value - N>(value.tuple), ctx);
    if (N <= 1) return out;
    out = detail::copy<Char>(value.sep, out);
    ctx.advance_to(out);
    return do_format(value, ctx, std::integral_constant<size_t, N - 1>());
  }
};

namespace detail {
// Check if T has an interface like a container adaptor (e.g. std::stack,
// std::queue, std::priority_queue).
template <typename T> class is_container_adaptor_like {
  template <typename U> static auto check(U* p) -> typename U::container_type;
  template <typename> static void check(...);

 public:
  static constexpr const bool value =
      !std::is_void<decltype(check<T>(nullptr))>::value;
};

template <typename Container> struct all {
  const Container& c;
  auto begin() const -> typename Container::const_iterator { return c.begin(); }
  auto end() const -> typename Container::const_iterator { return c.end(); }
};
}  // namespace detail

template <typename T, typename Char>
struct formatter<
    T, Char,
    enable_if_t<conjunction<detail::is_container_adaptor_like<T>,
                            bool_constant<range_format_kind<T, Char>::value ==
                                          range_format::disabled>>::value>>
    : formatter<detail::all<typename T::container_type>, Char> {
  using all = detail::all<typename T::container_type>;
  template <typename FormatContext>
  auto format(const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
    struct getter : T {
      static auto get(const T& v) -> all {
        return {v.*(&getter::c)};  // Access c through the derived class.
      }
    };
    return formatter<all>::format(getter::get(value), ctx);
  }
};

FMT_BEGIN_EXPORT

/// Returns a view that formats the iterator range `[begin, end)` with elements
/// separated by `sep`.
template <typename It, typename Sentinel>
auto join(It begin, Sentinel end, string_view sep) -> join_view<It, Sentinel> {
  return {std::move(begin), end, sep};
}

/**
 * Returns a view that formats `range` with elements separated by `sep`.
 *
 * **Example**:
 *
 *     auto v = std::vector<int>{1, 2, 3};
 *     fmt::print("{}", fmt::join(v, ", "));
 *     // Output: 1, 2, 3
 *
 * `fmt::join` applies passed format specifiers to the range elements:
 *
 *     fmt::print("{:02}", fmt::join(v, ", "));
 *     // Output: 01, 02, 03
 */
template <typename Range, FMT_ENABLE_IF(!is_tuple_like<Range>::value)>
auto join(Range&& r, string_view sep)
    -> join_view<decltype(detail::range_begin(r)),
                 decltype(detail::range_end(r))> {
  return {detail::range_begin(r), detail::range_end(r), sep};
}

/**
 * Returns an object that formats `std::tuple` with elements separated by `sep`.
 *
 * **Example**:
 *
 *     auto t = std::tuple<int, char>{1, 'a'};
 *     fmt::print("{}", fmt::join(t, ", "));
 *     // Output: 1, a
 */
template <typename Tuple, FMT_ENABLE_IF(is_tuple_like<Tuple>::value)>
FMT_CONSTEXPR auto join(const Tuple& tuple, string_view sep)
    -> tuple_join_view<char, Tuple> {
  return {tuple, sep};
}

/**
 * Returns an object that formats `std::initializer_list` with elements
 * separated by `sep`.
 *
 * **Example**:
 *
 *     fmt::print("{}", fmt::join({1, 2, 3}, ", "));
 *     // Output: "1, 2, 3"
 */
template <typename T>
auto join(std::initializer_list<T> list, string_view sep)
    -> join_view<const T*, const T*> {
  return join(std::begin(list), std::end(list), sep);
}

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_RANGES_H_
