// Formatting library for C++ - experimental format string compilation
//
// Copyright (c) 2012 - present, Victor Zverovich and fmt contributors
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_COMPILE_H_
#define FMT_COMPILE_H_

#ifndef FMT_MODULE
#  include <iterator>  // std::back_inserter
#endif

#include "format.h"

FMT_BEGIN_NAMESPACE

// A compile-time string which is compiled into fast formatting code.
FMT_EXPORT class compiled_string {};

template <typename S>
struct is_compiled_string : std::is_base_of<compiled_string, S> {};

namespace detail {

/**
 * Converts a string literal `s` into a format string that will be parsed at
 * compile time and converted into efficient formatting code. Requires C++17
 * `constexpr if` compiler support.
 *
 * **Example**:
 *
 *     // Converts 42 into std::string using the most efficient method and no
 *     // runtime format string processing.
 *     std::string s = fmt::format(FMT_COMPILE("{}"), 42);
 */
#if defined(__cpp_if_constexpr) && defined(__cpp_return_type_deduction)
#  define FMT_COMPILE(s) FMT_STRING_IMPL(s, fmt::compiled_string)
#else
#  define FMT_COMPILE(s) FMT_STRING(s)
#endif

template <typename T, typename... Tail>
auto first(const T& value, const Tail&...) -> const T& {
  return value;
}

#if defined(__cpp_if_constexpr) && defined(__cpp_return_type_deduction)
template <typename... Args> struct type_list {};

// Returns a reference to the argument at index N from [first, rest...].
template <int N, typename T, typename... Args>
constexpr const auto& get([[maybe_unused]] const T& first,
                          [[maybe_unused]] const Args&... rest) {
  static_assert(N < 1 + sizeof...(Args), "index is out of bounds");
  if constexpr (N == 0)
    return first;
  else
    return detail::get<N - 1>(rest...);
}

#  if FMT_USE_NONTYPE_TEMPLATE_ARGS
template <int N, typename T, typename... Args, typename Char>
constexpr auto get_arg_index_by_name(basic_string_view<Char> name) -> int {
  if constexpr (is_static_named_arg<T>()) {
    if (name == T::name) return N;
  }
  if constexpr (sizeof...(Args) > 0)
    return get_arg_index_by_name<N + 1, Args...>(name);
  (void)name;  // Workaround an MSVC bug about "unused" parameter.
  return -1;
}
#  endif

template <typename... Args, typename Char>
FMT_CONSTEXPR auto get_arg_index_by_name(basic_string_view<Char> name) -> int {
#  if FMT_USE_NONTYPE_TEMPLATE_ARGS
  if constexpr (sizeof...(Args) > 0)
    return get_arg_index_by_name<0, Args...>(name);
#  endif
  (void)name;
  return -1;
}

template <typename Char, typename... Args>
constexpr int get_arg_index_by_name(basic_string_view<Char> name,
                                    type_list<Args...>) {
  return get_arg_index_by_name<Args...>(name);
}

template <int N, typename> struct get_type_impl;

template <int N, typename... Args> struct get_type_impl<N, type_list<Args...>> {
  using type =
      remove_cvref_t<decltype(detail::get<N>(std::declval<Args>()...))>;
};

template <int N, typename T>
using get_type = typename get_type_impl<N, T>::type;

template <typename T> struct is_compiled_format : std::false_type {};

template <typename Char> struct text {
  basic_string_view<Char> data;
  using char_type = Char;

  template <typename OutputIt, typename... Args>
  constexpr OutputIt format(OutputIt out, const Args&...) const {
    return write<Char>(out, data);
  }
};

template <typename Char>
struct is_compiled_format<text<Char>> : std::true_type {};

template <typename Char>
constexpr text<Char> make_text(basic_string_view<Char> s, size_t pos,
                               size_t size) {
  return {{&s[pos], size}};
}

template <typename Char> struct code_unit {
  Char value;
  using char_type = Char;

  template <typename OutputIt, typename... Args>
  constexpr OutputIt format(OutputIt out, const Args&...) const {
    *out++ = value;
    return out;
  }
};

// This ensures that the argument type is convertible to `const T&`.
template <typename T, int N, typename... Args>
constexpr const T& get_arg_checked(const Args&... args) {
  const auto& arg = detail::get<N>(args...);
  if constexpr (detail::is_named_arg<remove_cvref_t<decltype(arg)>>()) {
    return arg.value;
  } else {
    return arg;
  }
}

template <typename Char>
struct is_compiled_format<code_unit<Char>> : std::true_type {};

// A replacement field that refers to argument N.
template <typename Char, typename T, int N> struct field {
  using char_type = Char;

  template <typename OutputIt, typename... Args>
  constexpr OutputIt format(OutputIt out, const Args&... args) const {
    const T& arg = get_arg_checked<T, N>(args...);
    if constexpr (std::is_convertible<T, basic_string_view<Char>>::value) {
      auto s = basic_string_view<Char>(arg);
      return copy<Char>(s.begin(), s.end(), out);
    } else {
      return write<Char>(out, arg);
    }
  }
};

template <typename Char, typename T, int N>
struct is_compiled_format<field<Char, T, N>> : std::true_type {};

// A replacement field that refers to argument with name.
template <typename Char> struct runtime_named_field {
  using char_type = Char;
  basic_string_view<Char> name;

  template <typename OutputIt, typename T>
  constexpr static bool try_format_argument(
      OutputIt& out,
      // [[maybe_unused]] due to unused-but-set-parameter warning in GCC 7,8,9
      [[maybe_unused]] basic_string_view<Char> arg_name, const T& arg) {
    if constexpr (is_named_arg<typename std::remove_cv<T>::type>::value) {
      if (arg_name == arg.name) {
        out = write<Char>(out, arg.value);
        return true;
      }
    }
    return false;
  }

  template <typename OutputIt, typename... Args>
  constexpr OutputIt format(OutputIt out, const Args&... args) const {
    bool found = (try_format_argument(out, name, args) || ...);
    if (!found) {
      FMT_THROW(format_error("argument with specified name is not found"));
    }
    return out;
  }
};

template <typename Char>
struct is_compiled_format<runtime_named_field<Char>> : std::true_type {};

// A replacement field that refers to argument N and has format specifiers.
template <typename Char, typename T, int N> struct spec_field {
  using char_type = Char;
  formatter<T, Char> fmt;

  template <typename OutputIt, typename... Args>
  constexpr FMT_INLINE OutputIt format(OutputIt out,
                                       const Args&... args) const {
    const auto& vargs =
        fmt::make_format_args<basic_format_context<OutputIt, Char>>(args...);
    basic_format_context<OutputIt, Char> ctx(out, vargs);
    return fmt.format(get_arg_checked<T, N>(args...), ctx);
  }
};

template <typename Char, typename T, int N>
struct is_compiled_format<spec_field<Char, T, N>> : std::true_type {};

template <typename L, typename R> struct concat {
  L lhs;
  R rhs;
  using char_type = typename L::char_type;

  template <typename OutputIt, typename... Args>
  constexpr OutputIt format(OutputIt out, const Args&... args) const {
    out = lhs.format(out, args...);
    return rhs.format(out, args...);
  }
};

template <typename L, typename R>
struct is_compiled_format<concat<L, R>> : std::true_type {};

template <typename L, typename R>
constexpr concat<L, R> make_concat(L lhs, R rhs) {
  return {lhs, rhs};
}

struct unknown_format {};

template <typename Char>
constexpr size_t parse_text(basic_string_view<Char> str, size_t pos) {
  for (size_t size = str.size(); pos != size; ++pos) {
    if (str[pos] == '{' || str[pos] == '}') break;
  }
  return pos;
}

template <typename Args, size_t POS, int ID, typename S>
constexpr auto compile_format_string(S fmt);

template <typename Args, size_t POS, int ID, typename T, typename S>
constexpr auto parse_tail(T head, S fmt) {
  if constexpr (POS != basic_string_view<typename S::char_type>(fmt).size()) {
    constexpr auto tail = compile_format_string<Args, POS, ID>(fmt);
    if constexpr (std::is_same<remove_cvref_t<decltype(tail)>,
                               unknown_format>())
      return tail;
    else
      return make_concat(head, tail);
  } else {
    return head;
  }
}

template <typename T, typename Char> struct parse_specs_result {
  formatter<T, Char> fmt;
  size_t end;
  int next_arg_id;
};

enum { manual_indexing_id = -1 };

template <typename T, typename Char>
constexpr parse_specs_result<T, Char> parse_specs(basic_string_view<Char> str,
                                                  size_t pos, int next_arg_id) {
  str.remove_prefix(pos);
  auto ctx =
      compile_parse_context<Char>(str, max_value<int>(), nullptr, next_arg_id);
  auto f = formatter<T, Char>();
  auto end = f.parse(ctx);
  return {f, pos + fmt::detail::to_unsigned(end - str.data()),
          next_arg_id == 0 ? manual_indexing_id : ctx.next_arg_id()};
}

template <typename Char> struct arg_id_handler {
  arg_id_kind kind;
  arg_ref<Char> arg_id;

  constexpr int on_auto() {
    FMT_ASSERT(false, "handler cannot be used with automatic indexing");
    return 0;
  }
  constexpr int on_index(int id) {
    kind = arg_id_kind::index;
    arg_id = arg_ref<Char>(id);
    return 0;
  }
  constexpr int on_name(basic_string_view<Char> id) {
    kind = arg_id_kind::name;
    arg_id = arg_ref<Char>(id);
    return 0;
  }
};

template <typename Char> struct parse_arg_id_result {
  arg_id_kind kind;
  arg_ref<Char> arg_id;
  const Char* arg_id_end;
};

template <int ID, typename Char>
constexpr auto parse_arg_id(const Char* begin, const Char* end) {
  auto handler = arg_id_handler<Char>{arg_id_kind::none, arg_ref<Char>{}};
  auto arg_id_end = parse_arg_id(begin, end, handler);
  return parse_arg_id_result<Char>{handler.kind, handler.arg_id, arg_id_end};
}

template <typename T, typename Enable = void> struct field_type {
  using type = remove_cvref_t<T>;
};

template <typename T>
struct field_type<T, enable_if_t<detail::is_named_arg<T>::value>> {
  using type = remove_cvref_t<decltype(T::value)>;
};

template <typename T, typename Args, size_t END_POS, int ARG_INDEX, int NEXT_ID,
          typename S>
constexpr auto parse_replacement_field_then_tail(S fmt) {
  using char_type = typename S::char_type;
  constexpr auto str = basic_string_view<char_type>(fmt);
  constexpr char_type c = END_POS != str.size() ? str[END_POS] : char_type();
  if constexpr (c == '}') {
    return parse_tail<Args, END_POS + 1, NEXT_ID>(
        field<char_type, typename field_type<T>::type, ARG_INDEX>(), fmt);
  } else if constexpr (c != ':') {
    FMT_THROW(format_error("expected ':'"));
  } else {
    constexpr auto result = parse_specs<typename field_type<T>::type>(
        str, END_POS + 1, NEXT_ID == manual_indexing_id ? 0 : NEXT_ID);
    if constexpr (result.end >= str.size() || str[result.end] != '}') {
      FMT_THROW(format_error("expected '}'"));
      return 0;
    } else {
      return parse_tail<Args, result.end + 1, result.next_arg_id>(
          spec_field<char_type, typename field_type<T>::type, ARG_INDEX>{
              result.fmt},
          fmt);
    }
  }
}

// Compiles a non-empty format string and returns the compiled representation
// or unknown_format() on unrecognized input.
template <typename Args, size_t POS, int ID, typename S>
constexpr auto compile_format_string(S fmt) {
  using char_type = typename S::char_type;
  constexpr auto str = basic_string_view<char_type>(fmt);
  if constexpr (str[POS] == '{') {
    if constexpr (POS + 1 == str.size())
      FMT_THROW(format_error("unmatched '{' in format string"));
    if constexpr (str[POS + 1] == '{') {
      return parse_tail<Args, POS + 2, ID>(make_text(str, POS, 1), fmt);
    } else if constexpr (str[POS + 1] == '}' || str[POS + 1] == ':') {
      static_assert(ID != manual_indexing_id,
                    "cannot switch from manual to automatic argument indexing");
      constexpr auto next_id =
          ID != manual_indexing_id ? ID + 1 : manual_indexing_id;
      return parse_replacement_field_then_tail<get_type<ID, Args>, Args,
                                               POS + 1, ID, next_id>(fmt);
    } else {
      constexpr auto arg_id_result =
          parse_arg_id<ID>(str.data() + POS + 1, str.data() + str.size());
      constexpr auto arg_id_end_pos = arg_id_result.arg_id_end - str.data();
      constexpr char_type c =
          arg_id_end_pos != str.size() ? str[arg_id_end_pos] : char_type();
      static_assert(c == '}' || c == ':', "missing '}' in format string");
      if constexpr (arg_id_result.kind == arg_id_kind::index) {
        static_assert(
            ID == manual_indexing_id || ID == 0,
            "cannot switch from automatic to manual argument indexing");
        constexpr auto arg_index = arg_id_result.arg_id.index;
        return parse_replacement_field_then_tail<get_type<arg_index, Args>,
                                                 Args, arg_id_end_pos,
                                                 arg_index, manual_indexing_id>(
            fmt);
      } else if constexpr (arg_id_result.kind == arg_id_kind::name) {
        constexpr auto arg_index =
            get_arg_index_by_name(arg_id_result.arg_id.name, Args{});
        if constexpr (arg_index >= 0) {
          constexpr auto next_id =
              ID != manual_indexing_id ? ID + 1 : manual_indexing_id;
          return parse_replacement_field_then_tail<
              decltype(get_type<arg_index, Args>::value), Args, arg_id_end_pos,
              arg_index, next_id>(fmt);
        } else if constexpr (c == '}') {
          return parse_tail<Args, arg_id_end_pos + 1, ID>(
              runtime_named_field<char_type>{arg_id_result.arg_id.name}, fmt);
        } else if constexpr (c == ':') {
          return unknown_format();  // no type info for specs parsing
        }
      }
    }
  } else if constexpr (str[POS] == '}') {
    if constexpr (POS + 1 == str.size())
      FMT_THROW(format_error("unmatched '}' in format string"));
    return parse_tail<Args, POS + 2, ID>(make_text(str, POS, 1), fmt);
  } else {
    constexpr auto end = parse_text(str, POS + 1);
    if constexpr (end - POS > 1) {
      return parse_tail<Args, end, ID>(make_text(str, POS, end - POS), fmt);
    } else {
      return parse_tail<Args, end, ID>(code_unit<char_type>{str[POS]}, fmt);
    }
  }
}

template <typename... Args, typename S,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
constexpr auto compile(S fmt) {
  constexpr auto str = basic_string_view<typename S::char_type>(fmt);
  if constexpr (str.size() == 0) {
    return detail::make_text(str, 0, 0);
  } else {
    constexpr auto result =
        detail::compile_format_string<detail::type_list<Args...>, 0, 0>(fmt);
    return result;
  }
}
#endif  // defined(__cpp_if_constexpr) && defined(__cpp_return_type_deduction)
}  // namespace detail

FMT_BEGIN_EXPORT

#if defined(__cpp_if_constexpr) && defined(__cpp_return_type_deduction)

template <typename CompiledFormat, typename... Args,
          typename Char = typename CompiledFormat::char_type,
          FMT_ENABLE_IF(detail::is_compiled_format<CompiledFormat>::value)>
FMT_INLINE std::basic_string<Char> format(const CompiledFormat& cf,
                                          const Args&... args) {
  auto s = std::basic_string<Char>();
  cf.format(std::back_inserter(s), args...);
  return s;
}

template <typename OutputIt, typename CompiledFormat, typename... Args,
          FMT_ENABLE_IF(detail::is_compiled_format<CompiledFormat>::value)>
constexpr FMT_INLINE OutputIt format_to(OutputIt out, const CompiledFormat& cf,
                                        const Args&... args) {
  return cf.format(out, args...);
}

template <typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
FMT_INLINE std::basic_string<typename S::char_type> format(const S&,
                                                           Args&&... args) {
  if constexpr (std::is_same<typename S::char_type, char>::value) {
    constexpr auto str = basic_string_view<typename S::char_type>(S());
    if constexpr (str.size() == 2 && str[0] == '{' && str[1] == '}') {
      const auto& first = detail::first(args...);
      if constexpr (detail::is_named_arg<
                        remove_cvref_t<decltype(first)>>::value) {
        return fmt::to_string(first.value);
      } else {
        return fmt::to_string(first);
      }
    }
  }
  constexpr auto compiled = detail::compile<Args...>(S());
  if constexpr (std::is_same<remove_cvref_t<decltype(compiled)>,
                             detail::unknown_format>()) {
    return fmt::format(
        static_cast<basic_string_view<typename S::char_type>>(S()),
        std::forward<Args>(args)...);
  } else {
    return fmt::format(compiled, std::forward<Args>(args)...);
  }
}

template <typename OutputIt, typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
FMT_CONSTEXPR OutputIt format_to(OutputIt out, const S&, Args&&... args) {
  constexpr auto compiled = detail::compile<Args...>(S());
  if constexpr (std::is_same<remove_cvref_t<decltype(compiled)>,
                             detail::unknown_format>()) {
    return fmt::format_to(
        out, static_cast<basic_string_view<typename S::char_type>>(S()),
        std::forward<Args>(args)...);
  } else {
    return fmt::format_to(out, compiled, std::forward<Args>(args)...);
  }
}
#endif

template <typename OutputIt, typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
auto format_to_n(OutputIt out, size_t n, const S& fmt, Args&&... args)
    -> format_to_n_result<OutputIt> {
  using traits = detail::fixed_buffer_traits;
  auto buf = detail::iterator_buffer<OutputIt, char, traits>(out, n);
  fmt::format_to(std::back_inserter(buf), fmt, std::forward<Args>(args)...);
  return {buf.out(), buf.count()};
}

template <typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
FMT_CONSTEXPR20 auto formatted_size(const S& fmt, const Args&... args)
    -> size_t {
  auto buf = detail::counting_buffer<>();
  fmt::format_to(appender(buf), fmt, args...);
  return buf.count();
}

template <typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
void print(std::FILE* f, const S& fmt, const Args&... args) {
  auto buf = memory_buffer();
  fmt::format_to(appender(buf), fmt, args...);
  detail::print(f, {buf.data(), buf.size()});
}

template <typename S, typename... Args,
          FMT_ENABLE_IF(is_compiled_string<S>::value)>
void print(const S& fmt, const Args&... args) {
  print(stdout, fmt, args...);
}

#if FMT_USE_NONTYPE_TEMPLATE_ARGS
inline namespace literals {
template <detail::fixed_string Str> constexpr auto operator""_cf() {
  return FMT_COMPILE(Str.data);
}
}  // namespace literals
#endif

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_COMPILE_H_
