// Formatting library for C++ - formatters for standard library types
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_STD_H_
#define FMT_STD_H_

#include "format.h"
#include "ostream.h"

#ifndef FMT_MODULE
#  include <atomic>
#  include <bitset>
#  include <complex>
#  include <cstdlib>
#  include <exception>
#  include <functional>
#  include <memory>
#  include <thread>
#  include <type_traits>
#  include <typeinfo>
#  include <utility>
#  include <vector>

// Check FMT_CPLUSPLUS to suppress a bogus warning in MSVC.
#  if FMT_CPLUSPLUS >= 201703L
#    if FMT_HAS_INCLUDE(<filesystem>) && \
        (!defined(FMT_CPP_LIB_FILESYSTEM) || FMT_CPP_LIB_FILESYSTEM != 0)
#      include <filesystem>
#    endif
#    if FMT_HAS_INCLUDE(<variant>)
#      include <variant>
#    endif
#    if FMT_HAS_INCLUDE(<optional>)
#      include <optional>
#    endif
#  endif
// Use > instead of >= in the version check because <source_location> may be
// available after C++17 but before C++20 is marked as implemented.
#  if FMT_CPLUSPLUS > 201703L && FMT_HAS_INCLUDE(<source_location>)
#    include <source_location>
#  endif
#  if FMT_CPLUSPLUS > 202002L && FMT_HAS_INCLUDE(<expected>)
#    include <expected>
#  endif
#endif  // FMT_MODULE

#if FMT_HAS_INCLUDE(<version>)
#  include <version>
#endif

// GCC 4 does not support FMT_HAS_INCLUDE.
#if FMT_HAS_INCLUDE(<cxxabi.h>) || defined(__GLIBCXX__)
#  include <cxxabi.h>
// Android NDK with gabi++ library on some architectures does not implement
// abi::__cxa_demangle().
#  ifndef __GABIXX_CXXABI_H__
#    define FMT_HAS_ABI_CXA_DEMANGLE
#  endif
#endif

// For older Xcode versions, __cpp_lib_xxx flags are inaccurately defined.
#ifndef FMT_CPP_LIB_FILESYSTEM
#  ifdef __cpp_lib_filesystem
#    define FMT_CPP_LIB_FILESYSTEM __cpp_lib_filesystem
#  else
#    define FMT_CPP_LIB_FILESYSTEM 0
#  endif
#endif

#ifndef FMT_CPP_LIB_VARIANT
#  ifdef __cpp_lib_variant
#    define FMT_CPP_LIB_VARIANT __cpp_lib_variant
#  else
#    define FMT_CPP_LIB_VARIANT 0
#  endif
#endif

#if FMT_CPP_LIB_FILESYSTEM
FMT_BEGIN_NAMESPACE

namespace detail {

template <typename Char, typename PathChar>
auto get_path_string(const std::filesystem::path& p,
                     const std::basic_string<PathChar>& native) {
  if constexpr (std::is_same_v<Char, char> && std::is_same_v<PathChar, wchar_t>)
    return to_utf8<wchar_t>(native, to_utf8_error_policy::replace);
  else
    return p.string<Char>();
}

template <typename Char, typename PathChar>
void write_escaped_path(basic_memory_buffer<Char>& quoted,
                        const std::filesystem::path& p,
                        const std::basic_string<PathChar>& native) {
  if constexpr (std::is_same_v<Char, char> &&
                std::is_same_v<PathChar, wchar_t>) {
    auto buf = basic_memory_buffer<wchar_t>();
    write_escaped_string<wchar_t>(std::back_inserter(buf), native);
    bool valid = to_utf8<wchar_t>::convert(quoted, {buf.data(), buf.size()});
    FMT_ASSERT(valid, "invalid utf16");
  } else if constexpr (std::is_same_v<Char, PathChar>) {
    write_escaped_string<std::filesystem::path::value_type>(
        std::back_inserter(quoted), native);
  } else {
    write_escaped_string<Char>(std::back_inserter(quoted), p.string<Char>());
  }
}

}  // namespace detail

template <typename Char> struct formatter<std::filesystem::path, Char> {
 private:
  format_specs specs_;
  detail::arg_ref<Char> width_ref_;
  bool debug_ = false;
  char path_type_ = 0;

 public:
  FMT_CONSTEXPR void set_debug_format(bool set = true) { debug_ = set; }

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end) return it;

    it = detail::parse_align(it, end, specs_);
    if (it == end) return it;

    Char c = *it;
    if ((c >= '0' && c <= '9') || c == '{')
      it = detail::parse_width(it, end, specs_, width_ref_, ctx);
    if (it != end && *it == '?') {
      debug_ = true;
      ++it;
    }
    if (it != end && (*it == 'g')) path_type_ = detail::to_ascii(*it++);
    return it;
  }

  template <typename FormatContext>
  auto format(const std::filesystem::path& p, FormatContext& ctx) const {
    auto specs = specs_;
    auto path_string =
        !path_type_ ? p.native()
                    : p.generic_string<std::filesystem::path::value_type>();

    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width, width_ref_,
                                ctx);
    if (!debug_) {
      auto s = detail::get_path_string<Char>(p, path_string);
      return detail::write(ctx.out(), basic_string_view<Char>(s), specs);
    }
    auto quoted = basic_memory_buffer<Char>();
    detail::write_escaped_path(quoted, p, path_string);
    return detail::write(ctx.out(),
                         basic_string_view<Char>(quoted.data(), quoted.size()),
                         specs);
  }
};

class path : public std::filesystem::path {
 public:
  auto display_string() const -> std::string {
    const std::filesystem::path& base = *this;
    return fmt::format(FMT_STRING("{}"), base);
  }
  auto system_string() const -> std::string { return string(); }

  auto generic_display_string() const -> std::string {
    const std::filesystem::path& base = *this;
    return fmt::format(FMT_STRING("{:g}"), base);
  }
  auto generic_system_string() const -> std::string { return generic_string(); }
};

FMT_END_NAMESPACE
#endif  // FMT_CPP_LIB_FILESYSTEM

FMT_BEGIN_NAMESPACE
template <std::size_t N, typename Char>
struct formatter<std::bitset<N>, Char>
    : nested_formatter<basic_string_view<Char>, Char> {
 private:
  // Functor because C++11 doesn't support generic lambdas.
  struct writer {
    const std::bitset<N>& bs;

    template <typename OutputIt>
    FMT_CONSTEXPR auto operator()(OutputIt out) -> OutputIt {
      for (auto pos = N; pos > 0; --pos) {
        out = detail::write<Char>(out, bs[pos - 1] ? Char('1') : Char('0'));
      }

      return out;
    }
  };

 public:
  template <typename FormatContext>
  auto format(const std::bitset<N>& bs, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return this->write_padded(ctx, writer{bs});
  }
};

template <typename Char>
struct formatter<std::thread::id, Char> : basic_ostream_formatter<Char> {};
FMT_END_NAMESPACE

#ifdef __cpp_lib_optional
FMT_BEGIN_NAMESPACE
template <typename T, typename Char>
struct formatter<std::optional<T>, Char,
                 std::enable_if_t<is_formattable<T, Char>::value>> {
 private:
  formatter<T, Char> underlying_;
  static constexpr basic_string_view<Char> optional =
      detail::string_literal<Char, 'o', 'p', 't', 'i', 'o', 'n', 'a', 'l',
                             '('>{};
  static constexpr basic_string_view<Char> none =
      detail::string_literal<Char, 'n', 'o', 'n', 'e'>{};

  template <class U>
  FMT_CONSTEXPR static auto maybe_set_debug_format(U& u, bool set)
      -> decltype(u.set_debug_format(set)) {
    u.set_debug_format(set);
  }

  template <class U>
  FMT_CONSTEXPR static void maybe_set_debug_format(U&, ...) {}

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) {
    maybe_set_debug_format(underlying_, true);
    return underlying_.parse(ctx);
  }

  template <typename FormatContext>
  auto format(const std::optional<T>& opt, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    if (!opt) return detail::write<Char>(ctx.out(), none);

    auto out = ctx.out();
    out = detail::write<Char>(out, optional);
    ctx.advance_to(out);
    out = underlying_.format(*opt, ctx);
    return detail::write(out, ')');
  }
};
FMT_END_NAMESPACE
#endif  // __cpp_lib_optional

#if defined(__cpp_lib_expected) || FMT_CPP_LIB_VARIANT

FMT_BEGIN_NAMESPACE
namespace detail {

template <typename Char, typename OutputIt, typename T>
auto write_escaped_alternative(OutputIt out, const T& v) -> OutputIt {
  if constexpr (has_to_string_view<T>::value)
    return write_escaped_string<Char>(out, detail::to_string_view(v));
  if constexpr (std::is_same_v<T, Char>) return write_escaped_char(out, v);
  return write<Char>(out, v);
}

}  // namespace detail

FMT_END_NAMESPACE
#endif

#ifdef __cpp_lib_expected
FMT_BEGIN_NAMESPACE

template <typename T, typename E, typename Char>
struct formatter<std::expected<T, E>, Char,
                 std::enable_if_t<(std::is_void<T>::value ||
                                   is_formattable<T, Char>::value) &&
                                  is_formattable<E, Char>::value>> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const std::expected<T, E>& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();

    if (value.has_value()) {
      out = detail::write<Char>(out, "expected(");
      if constexpr (!std::is_void<T>::value)
        out = detail::write_escaped_alternative<Char>(out, *value);
    } else {
      out = detail::write<Char>(out, "unexpected(");
      out = detail::write_escaped_alternative<Char>(out, value.error());
    }
    *out++ = ')';
    return out;
  }
};
FMT_END_NAMESPACE
#endif  // __cpp_lib_expected

#ifdef __cpp_lib_source_location
FMT_BEGIN_NAMESPACE
template <> struct formatter<std::source_location> {
  FMT_CONSTEXPR auto parse(parse_context<>& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const std::source_location& loc, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();
    out = detail::write(out, loc.file_name());
    out = detail::write(out, ':');
    out = detail::write<char>(out, loc.line());
    out = detail::write(out, ':');
    out = detail::write<char>(out, loc.column());
    out = detail::write(out, ": ");
    out = detail::write(out, loc.function_name());
    return out;
  }
};
FMT_END_NAMESPACE
#endif

#if FMT_CPP_LIB_VARIANT
FMT_BEGIN_NAMESPACE
namespace detail {

template <typename T>
using variant_index_sequence =
    std::make_index_sequence<std::variant_size<T>::value>;

template <typename> struct is_variant_like_ : std::false_type {};
template <typename... Types>
struct is_variant_like_<std::variant<Types...>> : std::true_type {};

// formattable element check.
template <typename T, typename C> class is_variant_formattable_ {
  template <std::size_t... Is>
  static std::conjunction<
      is_formattable<std::variant_alternative_t<Is, T>, C>...>
      check(std::index_sequence<Is...>);

 public:
  static constexpr const bool value =
      decltype(check(variant_index_sequence<T>{}))::value;
};

}  // namespace detail

template <typename T> struct is_variant_like {
  static constexpr const bool value = detail::is_variant_like_<T>::value;
};

template <typename T, typename C> struct is_variant_formattable {
  static constexpr const bool value =
      detail::is_variant_formattable_<T, C>::value;
};

template <typename Char> struct formatter<std::monostate, Char> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const std::monostate&, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return detail::write<Char>(ctx.out(), "monostate");
  }
};

template <typename Variant, typename Char>
struct formatter<
    Variant, Char,
    std::enable_if_t<std::conjunction_v<
        is_variant_like<Variant>, is_variant_formattable<Variant, Char>>>> {
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return ctx.begin();
  }

  template <typename FormatContext>
  auto format(const Variant& value, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();

    out = detail::write<Char>(out, "variant(");
    FMT_TRY {
      std::visit(
          [&](const auto& v) {
            out = detail::write_escaped_alternative<Char>(out, v);
          },
          value);
    }
    FMT_CATCH(const std::bad_variant_access&) {
      detail::write<Char>(out, "valueless by exception");
    }
    *out++ = ')';
    return out;
  }
};
FMT_END_NAMESPACE
#endif  // FMT_CPP_LIB_VARIANT

FMT_BEGIN_NAMESPACE
template <> struct formatter<std::error_code> {
 private:
  format_specs specs_;
  detail::arg_ref<char> width_ref_;
  bool debug_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<>& ctx) -> const char* {
    auto it = ctx.begin(), end = ctx.end();
    if (it == end) return it;

    it = detail::parse_align(it, end, specs_);

    char c = *it;
    if (it != end && ((c >= '0' && c <= '9') || c == '{'))
      it = detail::parse_width(it, end, specs_, width_ref_, ctx);

    if (it != end && *it == '?') {
      debug_ = true;
      ++it;
    }
    if (it != end && *it == 's') {
      specs_.set_type(presentation_type::string);
      ++it;
    }
    return it;
  }

  template <typename FormatContext>
  FMT_CONSTEXPR20 auto format(const std::error_code& ec,
                              FormatContext& ctx) const -> decltype(ctx.out()) {
    auto specs = specs_;
    detail::handle_dynamic_spec(specs.dynamic_width(), specs.width, width_ref_,
                                ctx);
    auto buf = memory_buffer();
    if (specs_.type() == presentation_type::string) {
      buf.append(ec.message());
    } else {
      buf.append(string_view(ec.category().name()));
      buf.push_back(':');
      detail::write<char>(appender(buf), ec.value());
    }
    auto quoted = memory_buffer();
    auto str = string_view(buf.data(), buf.size());
    if (debug_) {
      detail::write_escaped_string<char>(std::back_inserter(quoted), str);
      str = string_view(quoted.data(), quoted.size());
    }
    return detail::write<char>(ctx.out(), str, specs);
  }
};

#if FMT_USE_RTTI
namespace detail {

template <typename Char, typename OutputIt>
auto write_demangled_name(OutputIt out, const std::type_info& ti) -> OutputIt {
#  ifdef FMT_HAS_ABI_CXA_DEMANGLE
  int status = 0;
  std::size_t size = 0;
  std::unique_ptr<char, void (*)(void*)> demangled_name_ptr(
      abi::__cxa_demangle(ti.name(), nullptr, &size, &status), &std::free);

  string_view demangled_name_view;
  if (demangled_name_ptr) {
    demangled_name_view = demangled_name_ptr.get();

    // Normalization of stdlib inline namespace names.
    // libc++ inline namespaces.
    //  std::__1::*       -> std::*
    //  std::__1::__fs::* -> std::*
    // libstdc++ inline namespaces.
    //  std::__cxx11::*             -> std::*
    //  std::filesystem::__cxx11::* -> std::filesystem::*
    if (demangled_name_view.starts_with("std::")) {
      char* begin = demangled_name_ptr.get();
      char* to = begin + 5;  // std::
      for (char *from = to, *end = begin + demangled_name_view.size();
           from < end;) {
        // This is safe, because demangled_name is NUL-terminated.
        if (from[0] == '_' && from[1] == '_') {
          char* next = from + 1;
          while (next < end && *next != ':') next++;
          if (next[0] == ':' && next[1] == ':') {
            from = next + 2;
            continue;
          }
        }
        *to++ = *from++;
      }
      demangled_name_view = {begin, detail::to_unsigned(to - begin)};
    }
  } else {
    demangled_name_view = string_view(ti.name());
  }
  return detail::write_bytes<Char>(out, demangled_name_view);
#  elif FMT_MSC_VERSION
  const string_view demangled_name(ti.name());
  for (std::size_t i = 0; i < demangled_name.size(); ++i) {
    auto sub = demangled_name;
    sub.remove_prefix(i);
    if (sub.starts_with("enum ")) {
      i += 4;
      continue;
    }
    if (sub.starts_with("class ") || sub.starts_with("union ")) {
      i += 5;
      continue;
    }
    if (sub.starts_with("struct ")) {
      i += 6;
      continue;
    }
    if (*sub.begin() != ' ') *out++ = *sub.begin();
  }
  return out;
#  else
  return detail::write_bytes<Char>(out, string_view(ti.name()));
#  endif
}

}  // namespace detail

template <typename Char>
struct formatter<std::type_info, Char  // DEPRECATED! Mixing code unit types.
                 > {
 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    return ctx.begin();
  }

  template <typename Context>
  auto format(const std::type_info& ti, Context& ctx) const
      -> decltype(ctx.out()) {
    return detail::write_demangled_name<Char>(ctx.out(), ti);
  }
};
#endif

template <typename T, typename Char>
struct formatter<
    T, Char,  // DEPRECATED! Mixing code unit types.
    typename std::enable_if<std::is_base_of<std::exception, T>::value>::type> {
 private:
  bool with_typename_ = false;

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    auto it = ctx.begin();
    auto end = ctx.end();
    if (it == end || *it == '}') return it;
    if (*it == 't') {
      ++it;
      with_typename_ = FMT_USE_RTTI != 0;
    }
    return it;
  }

  template <typename Context>
  auto format(const std::exception& ex, Context& ctx) const
      -> decltype(ctx.out()) {
    auto out = ctx.out();
#if FMT_USE_RTTI
    if (with_typename_) {
      out = detail::write_demangled_name<Char>(out, typeid(ex));
      *out++ = ':';
      *out++ = ' ';
    }
#endif
    return detail::write_bytes<Char>(out, string_view(ex.what()));
  }
};

namespace detail {

template <typename T, typename Enable = void>
struct has_flip : std::false_type {};

template <typename T>
struct has_flip<T, void_t<decltype(std::declval<T>().flip())>>
    : std::true_type {};

template <typename T> struct is_bit_reference_like {
  static constexpr const bool value =
      std::is_convertible<T, bool>::value &&
      std::is_nothrow_assignable<T, bool>::value && has_flip<T>::value;
};

#ifdef _LIBCPP_VERSION

// Workaround for libc++ incompatibility with C++ standard.
// According to the Standard, `bitset::operator[] const` returns bool.
template <typename C>
struct is_bit_reference_like<std::__bit_const_reference<C>> {
  static constexpr const bool value = true;
};

#endif

}  // namespace detail

// We can't use std::vector<bool, Allocator>::reference and
// std::bitset<N>::reference because the compiler can't deduce Allocator and N
// in partial specialization.
template <typename BitRef, typename Char>
struct formatter<BitRef, Char,
                 enable_if_t<detail::is_bit_reference_like<BitRef>::value>>
    : formatter<bool, Char> {
  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const BitRef& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(v, ctx);
  }
};

template <typename T, typename Deleter>
auto ptr(const std::unique_ptr<T, Deleter>& p) -> const void* {
  return p.get();
}
template <typename T> auto ptr(const std::shared_ptr<T>& p) -> const void* {
  return p.get();
}

template <typename T, typename Char>
struct formatter<std::atomic<T>, Char,
                 enable_if_t<is_formattable<T, Char>::value>>
    : formatter<T, Char> {
  template <typename FormatContext>
  auto format(const std::atomic<T>& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<T, Char>::format(v.load(), ctx);
  }
};

#ifdef __cpp_lib_atomic_flag_test
template <typename Char>
struct formatter<std::atomic_flag, Char> : formatter<bool, Char> {
  template <typename FormatContext>
  auto format(const std::atomic_flag& v, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<bool, Char>::format(v.test(), ctx);
  }
};
#endif  // __cpp_lib_atomic_flag_test

template <typename T, typename Char> struct formatter<std::complex<T>, Char> {
 private:
  detail::dynamic_format_specs<Char> specs_;

  template <typename FormatContext, typename OutputIt>
  FMT_CONSTEXPR auto do_format(const std::complex<T>& c,
                               detail::dynamic_format_specs<Char>& specs,
                               FormatContext& ctx, OutputIt out) const
      -> OutputIt {
    if (c.real() != 0) {
      *out++ = Char('(');
      out = detail::write<Char>(out, c.real(), specs, ctx.locale());
      specs.set_sign(sign::plus);
      out = detail::write<Char>(out, c.imag(), specs, ctx.locale());
      if (!detail::isfinite(c.imag())) *out++ = Char(' ');
      *out++ = Char('i');
      *out++ = Char(')');
      return out;
    }
    out = detail::write<Char>(out, c.imag(), specs, ctx.locale());
    if (!detail::isfinite(c.imag())) *out++ = Char(' ');
    *out++ = Char('i');
    return out;
  }

 public:
  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    if (ctx.begin() == ctx.end() || *ctx.begin() == '}') return ctx.begin();
    return parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx,
                              detail::type_constant<T, Char>::value);
  }

  template <typename FormatContext>
  auto format(const std::complex<T>& c, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    auto specs = specs_;
    if (specs.dynamic()) {
      detail::handle_dynamic_spec(specs.dynamic_width(), specs.width,
                                  specs.width_ref, ctx);
      detail::handle_dynamic_spec(specs.dynamic_precision(), specs.precision,
                                  specs.precision_ref, ctx);
    }

    if (specs.width == 0) return do_format(c, specs, ctx, ctx.out());
    auto buf = basic_memory_buffer<Char>();

    auto outer_specs = format_specs();
    outer_specs.width = specs.width;
    outer_specs.copy_fill_from(specs);
    outer_specs.set_align(specs.align());

    specs.width = 0;
    specs.set_fill({});
    specs.set_align(align::none);

    do_format(c, specs, ctx, basic_appender<Char>(buf));
    return detail::write<Char>(ctx.out(),
                               basic_string_view<Char>(buf.data(), buf.size()),
                               outer_specs);
  }
};

template <typename T, typename Char>
struct formatter<std::reference_wrapper<T>, Char,
                 enable_if_t<is_formattable<remove_cvref_t<T>, Char>::value>>
    : formatter<remove_cvref_t<T>, Char> {
  template <typename FormatContext>
  auto format(std::reference_wrapper<T> ref, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<remove_cvref_t<T>, Char>::format(ref.get(), ctx);
  }
};

FMT_END_NAMESPACE
#endif  // FMT_STD_H_
