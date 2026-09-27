// Formatting library for C++ - legacy printf implementation
//
// Copyright (c) 2012 - 2016, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_PRINTF_H_
#define FMT_PRINTF_H_

#ifndef FMT_MODULE
#  include <algorithm>  // std::find
#  include <limits>     // std::numeric_limits
#endif

#include "format.h"

FMT_BEGIN_NAMESPACE
FMT_BEGIN_EXPORT

template <typename Char> class basic_printf_context {
 private:
  basic_appender<Char> out_;
  basic_format_args<basic_printf_context> args_;

  static_assert(std::is_same<Char, char>::value ||
                    std::is_same<Char, wchar_t>::value,
                "Unsupported code unit type.");

 public:
  using char_type = Char;
  enum { builtin_types = 1 };

  /// Constructs a `printf_context` object. References to the arguments are
  /// stored in the context object so make sure they have appropriate lifetimes.
  basic_printf_context(basic_appender<Char> out,
                       basic_format_args<basic_printf_context> args)
      : out_(out), args_(args) {}

  auto out() -> basic_appender<Char> { return out_; }
  void advance_to(basic_appender<Char>) {}

  auto locale() -> locale_ref { return {}; }

  auto arg(int id) const -> basic_format_arg<basic_printf_context> {
    return args_.get(id);
  }
};

namespace detail {

// Return the result via the out param to workaround gcc bug 77539.
template <bool IS_CONSTEXPR, typename T, typename Ptr = const T*>
FMT_CONSTEXPR auto find(Ptr first, Ptr last, T value, Ptr& out) -> bool {
  for (out = first; out != last; ++out) {
    if (*out == value) return true;
  }
  return false;
}

template <>
inline auto find<false, char>(const char* first, const char* last, char value,
                              const char*& out) -> bool {
  out =
      static_cast<const char*>(memchr(first, value, to_unsigned(last - first)));
  return out != nullptr;
}

// Checks if a value fits in int - used to avoid warnings about comparing
// signed and unsigned integers.
template <bool IS_SIGNED> struct int_checker {
  template <typename T> static auto fits_in_int(T value) -> bool {
    return value <= to_unsigned(max_value<int>());
  }
  inline static auto fits_in_int(bool) -> bool { return true; }
};

template <> struct int_checker<true> {
  template <typename T> static auto fits_in_int(T value) -> bool {
    return value >= (std::numeric_limits<int>::min)() &&
           value <= max_value<int>();
  }
  inline static auto fits_in_int(int) -> bool { return true; }
};

struct printf_precision_handler {
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> int {
    if (!int_checker<std::numeric_limits<T>::is_signed>::fits_in_int(value))
      report_error("number is too big");
    return max_of(static_cast<int>(value), 0);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> int {
    report_error("precision is not integer");
    return 0;
  }
};

// An argument visitor that returns true iff arg is a zero integer.
struct is_zero_int {
  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> bool {
    return value == 0;
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> bool {
    return false;
  }
};

template <typename T> struct make_unsigned_or_bool : std::make_unsigned<T> {};

template <> struct make_unsigned_or_bool<bool> {
  using type = bool;
};

template <typename T, typename Context> class arg_converter {
 private:
  using char_type = typename Context::char_type;

  basic_format_arg<Context>& arg_;
  char_type type_;

 public:
  arg_converter(basic_format_arg<Context>& arg, char_type type)
      : arg_(arg), type_(type) {}

  void operator()(bool value) {
    if (type_ != 's') operator()<bool>(value);
  }

  template <typename U, FMT_ENABLE_IF(std::is_integral<U>::value)>
  void operator()(U value) {
    bool is_signed = type_ == 'd' || type_ == 'i';
    using target_type = conditional_t<std::is_same<T, void>::value, U, T>;
    if (const_check(sizeof(target_type) <= sizeof(int))) {
      // Extra casts are used to silence warnings.
      using unsigned_type = typename make_unsigned_or_bool<target_type>::type;
      if (is_signed)
        arg_ = static_cast<int>(static_cast<target_type>(value));
      else
        arg_ = static_cast<unsigned>(static_cast<unsigned_type>(value));
    } else {
      // glibc's printf doesn't sign extend arguments of smaller types:
      //   std::printf("%lld", -42);  // prints "4294967254"
      // but we don't have to do the same because it's a UB.
      if (is_signed)
        arg_ = static_cast<long long>(value);
      else
        arg_ = static_cast<typename make_unsigned_or_bool<U>::type>(value);
    }
  }

  template <typename U, FMT_ENABLE_IF(!std::is_integral<U>::value)>
  void operator()(U) {}  // No conversion needed for non-integral types.
};

// Converts an integer argument to T for printf, if T is an integral type.
// If T is void, the argument is converted to corresponding signed or unsigned
// type depending on the type specifier: 'd' and 'i' - signed, other -
// unsigned).
template <typename T, typename Context, typename Char>
void convert_arg(basic_format_arg<Context>& arg, Char type) {
  arg.visit(arg_converter<T, Context>(arg, type));
}

// Converts an integer argument to char for printf.
template <typename Context> class char_converter {
 private:
  basic_format_arg<Context>& arg_;

 public:
  explicit char_converter(basic_format_arg<Context>& arg) : arg_(arg) {}

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  void operator()(T value) {
    arg_ = static_cast<typename Context::char_type>(value);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  void operator()(T) {}  // No conversion needed for non-integral types.
};

// An argument visitor that return a pointer to a C string if argument is a
// string or null otherwise.
template <typename Char> struct get_cstring {
  template <typename T> auto operator()(T) -> const Char* { return nullptr; }
  auto operator()(const Char* s) -> const Char* { return s; }
};

// Checks if an argument is a valid printf width specifier and sets
// left alignment if it is negative.
class printf_width_handler {
 private:
  format_specs& specs_;

 public:
  inline explicit printf_width_handler(format_specs& specs) : specs_(specs) {}

  template <typename T, FMT_ENABLE_IF(std::is_integral<T>::value)>
  auto operator()(T value) -> unsigned {
    auto width = static_cast<uint32_or_64_or_128_t<T>>(value);
    if (detail::is_negative(value)) {
      specs_.set_align(align::left);
      width = 0 - width;
    }
    unsigned int_max = to_unsigned(max_value<int>());
    if (width > int_max) report_error("number is too big");
    return static_cast<unsigned>(width);
  }

  template <typename T, FMT_ENABLE_IF(!std::is_integral<T>::value)>
  auto operator()(T) -> unsigned {
    report_error("width is not integer");
    return 0;
  }
};

// Workaround for a bug with the XL compiler when initializing
// printf_arg_formatter's base class.
template <typename Char>
auto make_arg_formatter(basic_appender<Char> iter, format_specs& s)
    -> arg_formatter<Char> {
  return {iter, s, locale_ref()};
}

// The `printf` argument formatter.
template <typename Char>
class printf_arg_formatter : public arg_formatter<Char> {
 private:
  using base = arg_formatter<Char>;
  using context_type = basic_printf_context<Char>;

  context_type& context_;

  void write_null_pointer(bool is_string = false) {
    auto s = this->specs;
    s.set_type(presentation_type::none);
    write_bytes<Char>(this->out, is_string ? "(null)" : "(nil)", s);
  }

  template <typename T> void write(T value) {
    detail::write<Char>(this->out, value, this->specs, this->locale);
  }

 public:
  printf_arg_formatter(basic_appender<Char> iter, format_specs& s,
                       context_type& ctx)
      : base(make_arg_formatter(iter, s)), context_(ctx) {}

  void operator()(monostate value) { write(value); }

  template <typename T, FMT_ENABLE_IF(detail::is_integral<T>::value)>
  void operator()(T value) {
    // MSVC2013 fails to compile separate overloads for bool and Char so use
    // std::is_same instead.
    if (!std::is_same<T, Char>::value) {
      write(value);
      return;
    }
    format_specs s = this->specs;
    if (s.type() != presentation_type::none &&
        s.type() != presentation_type::chr) {
      return (*this)(static_cast<int>(value));
    }
    s.set_sign(sign::none);
    s.clear_alt();
    s.set_fill(' ');  // Ignore '0' flag for char types.
    // align::numeric needs to be overwritten here since the '0' flag is
    // ignored for non-numeric types
    if (s.align() == align::none || s.align() == align::numeric)
      s.set_align(align::right);
    detail::write<Char>(this->out, static_cast<Char>(value), s);
  }

  template <typename T, FMT_ENABLE_IF(std::is_floating_point<T>::value)>
  void operator()(T value) {
    write(value);
  }

  void operator()(const char* value) {
    if (value)
      write(value);
    else
      write_null_pointer(this->specs.type() != presentation_type::pointer);
  }

  void operator()(const wchar_t* value) {
    if (value)
      write(value);
    else
      write_null_pointer(this->specs.type() != presentation_type::pointer);
  }

  void operator()(basic_string_view<Char> value) { write(value); }

  void operator()(const void* value) {
    if (value)
      write(value);
    else
      write_null_pointer();
  }

  void operator()(typename basic_format_arg<context_type>::handle handle) {
    auto parse_ctx = parse_context<Char>({});
    handle.format(parse_ctx, context_);
  }
};

template <typename Char>
void parse_flags(format_specs& specs, const Char*& it, const Char* end) {
  for (; it != end; ++it) {
    switch (*it) {
    case '-': specs.set_align(align::left); break;
    case '+': specs.set_sign(sign::plus); break;
    case '0': specs.set_fill('0'); break;
    case ' ':
      if (specs.sign() != sign::plus) specs.set_sign(sign::space);
      break;
    case '#': specs.set_alt(); break;
    default:  return;
    }
  }
}

template <typename Char, typename GetArg>
auto parse_header(const Char*& it, const Char* end, format_specs& specs,
                  GetArg get_arg) -> int {
  int arg_index = -1;
  Char c = *it;
  if (c >= '0' && c <= '9') {
    // Parse an argument index (if followed by '$') or a width possibly
    // preceded with '0' flag(s).
    int value = parse_nonnegative_int(it, end, -1);
    if (it != end && *it == '$') {  // value is an argument index
      ++it;
      arg_index = value != -1 ? value : max_value<int>();
    } else {
      if (c == '0') specs.set_fill('0');
      if (value != 0) {
        // Nonzero value means that we parsed width and don't need to
        // parse it or flags again, so return now.
        if (value == -1) report_error("number is too big");
        specs.width = value;
        return arg_index;
      }
    }
  }
  parse_flags(specs, it, end);
  // Parse width.
  if (it != end) {
    if (*it >= '0' && *it <= '9') {
      specs.width = parse_nonnegative_int(it, end, -1);
      if (specs.width == -1) report_error("number is too big");
    } else if (*it == '*') {
      ++it;
      specs.width = static_cast<int>(
          get_arg(-1).visit(detail::printf_width_handler(specs)));
    }
  }
  return arg_index;
}

inline auto parse_printf_presentation_type(char c, type t, bool& upper)
    -> presentation_type {
  using pt = presentation_type;
  constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
  switch (c) {
  case 'd': return in(t, integral_set) ? pt::dec : pt::none;
  case 'o': return in(t, integral_set) ? pt::oct : pt::none;
  case 'X': upper = true; FMT_FALLTHROUGH;
  case 'x': return in(t, integral_set) ? pt::hex : pt::none;
  case 'E': upper = true; FMT_FALLTHROUGH;
  case 'e': return in(t, float_set) ? pt::exp : pt::none;
  case 'F': upper = true; FMT_FALLTHROUGH;
  case 'f': return in(t, float_set) ? pt::fixed : pt::none;
  case 'G': upper = true; FMT_FALLTHROUGH;
  case 'g': return in(t, float_set) ? pt::general : pt::none;
  case 'A': upper = true; FMT_FALLTHROUGH;
  case 'a': return in(t, float_set) ? pt::hexfloat : pt::none;
  case 'c': return in(t, integral_set) ? pt::chr : pt::none;
  case 's': return in(t, string_set | cstring_set) ? pt::string : pt::none;
  case 'p': return in(t, pointer_set | cstring_set) ? pt::pointer : pt::none;
  default:  return pt::none;
  }
}

template <typename Char, typename Context>
void vprintf(buffer<Char>& buf, basic_string_view<Char> format,
             basic_format_args<Context> args) {
  using iterator = basic_appender<Char>;
  auto out = iterator(buf);
  auto context = basic_printf_context<Char>(out, args);
  auto parse_ctx = parse_context<Char>(format);

  // Returns the argument with specified index or, if arg_index is -1, the next
  // argument.
  auto get_arg = [&](int arg_index) {
    if (arg_index < 0)
      arg_index = parse_ctx.next_arg_id();
    else
      parse_ctx.check_arg_id(--arg_index);
    auto arg = context.arg(arg_index);
    if (!arg) report_error("argument not found");
    return arg;
  };

  const Char* start = parse_ctx.begin();
  const Char* end = parse_ctx.end();
  auto it = start;
  while (it != end) {
    if (!find<false, Char>(it, end, '%', it)) {
      it = end;  // find leaves it == nullptr if it doesn't find '%'.
      break;
    }
    Char c = *it++;
    if (it != end && *it == c) {
      write(out, basic_string_view<Char>(start, to_unsigned(it - start)));
      start = ++it;
      continue;
    }
    write(out, basic_string_view<Char>(start, to_unsigned(it - 1 - start)));

    auto specs = format_specs();
    specs.set_align(align::right);

    // Parse argument index, flags and width.
    int arg_index = parse_header(it, end, specs, get_arg);
    if (arg_index == 0) report_error("argument not found");

    // Parse precision.
    if (it != end && *it == '.') {
      ++it;
      c = it != end ? *it : 0;
      if ('0' <= c && c <= '9') {
        specs.precision = parse_nonnegative_int(it, end, 0);
      } else if (c == '*') {
        ++it;
        specs.precision =
            static_cast<int>(get_arg(-1).visit(printf_precision_handler()));
      } else {
        specs.precision = 0;
      }
    }

    auto arg = get_arg(arg_index);
    // For d, i, o, u, x, and X conversion specifiers, if a precision is
    // specified, the '0' flag is ignored
    if (specs.precision >= 0 && is_integral_type(arg.type())) {
      // Ignore '0' for non-numeric types or if '-' present.
      specs.set_fill(' ');
    }
    if (specs.precision >= 0 && arg.type() == type::cstring_type) {
      auto str = arg.visit(get_cstring<Char>());
      auto str_end = str + specs.precision;
      auto nul = std::find(str, str_end, Char());
      auto sv = basic_string_view<Char>(
          str, to_unsigned(nul != str_end ? nul - str : specs.precision));
      arg = sv;
    }
    if (specs.alt() && arg.visit(is_zero_int())) specs.clear_alt();
    if (specs.fill_unit<Char>() == '0') {
      if (is_arithmetic_type(arg.type()) && specs.align() != align::left) {
        specs.set_align(align::numeric);
      } else {
        // Ignore '0' flag for non-numeric types or if '-' flag is also present.
        specs.set_fill(' ');
      }
    }

    // Parse length and convert the argument to the required type.
    c = it != end ? *it++ : 0;
    Char t = it != end ? *it : 0;
    switch (c) {
    case 'h':
      if (t == 'h') {
        ++it;
        t = it != end ? *it : 0;
        convert_arg<signed char>(arg, t);
      } else {
        convert_arg<short>(arg, t);
      }
      break;
    case 'l':
      if (t == 'l') {
        ++it;
        t = it != end ? *it : 0;
        convert_arg<long long>(arg, t);
      } else {
        convert_arg<long>(arg, t);
      }
      break;
    case 'j': convert_arg<intmax_t>(arg, t); break;
    case 'z': convert_arg<size_t>(arg, t); break;
    case 't': convert_arg<std::ptrdiff_t>(arg, t); break;
    case 'L':
      // printf produces garbage when 'L' is omitted for long double, no
      // need to do the same.
      break;
    default: --it; convert_arg<void>(arg, c);
    }

    // Parse type.
    if (it == end) report_error("invalid format string");
    char type = static_cast<char>(*it++);
    if (is_integral_type(arg.type())) {
      // Normalize type.
      switch (type) {
      case 'i':
      case 'u': type = 'd'; break;
      case 'c':
        arg.visit(char_converter<basic_printf_context<Char>>(arg));
        break;
      }
    }
    bool upper = false;
    specs.set_type(parse_printf_presentation_type(type, arg.type(), upper));
    if (specs.type() == presentation_type::none)
      report_error("invalid format specifier");
    if (upper) specs.set_upper();

    start = it;

    // Format argument.
    arg.visit(printf_arg_formatter<Char>(out, specs, context));
  }
  write(out, basic_string_view<Char>(start, to_unsigned(it - start)));
}
}  // namespace detail

using printf_context = basic_printf_context<char>;
using wprintf_context = basic_printf_context<wchar_t>;

using printf_args = basic_format_args<printf_context>;
using wprintf_args = basic_format_args<wprintf_context>;

/// Constructs an `format_arg_store` object that contains references to
/// arguments and can be implicitly converted to `printf_args`.
template <typename Char = char, typename... T>
inline auto make_printf_args(T&... args)
    -> decltype(fmt::make_format_args<basic_printf_context<Char>>(args...)) {
  return fmt::make_format_args<basic_printf_context<Char>>(args...);
}

template <typename Char> struct vprintf_args {
  using type = basic_format_args<basic_printf_context<Char>>;
};

template <typename Char>
inline auto vsprintf(basic_string_view<Char> fmt,
                     typename vprintf_args<Char>::type args)
    -> std::basic_string<Char> {
  auto buf = basic_memory_buffer<Char>();
  detail::vprintf(buf, fmt, args);
  return {buf.data(), buf.size()};
}

/**
 * Formats `args` according to specifications in `fmt` and returns the result
 * as as string.
 *
 * **Example**:
 *
 *     std::string message = fmt::sprintf("The answer is %d", 42);
 */
template <typename... T>
inline auto sprintf(string_view fmt, const T&... args) -> std::string {
  return vsprintf(fmt, make_printf_args(args...));
}
template <typename... T>
FMT_DEPRECATED auto sprintf(basic_string_view<wchar_t> fmt, const T&... args)
    -> std::wstring {
  return vsprintf(fmt, make_printf_args<wchar_t>(args...));
}

template <typename Char>
auto vfprintf(std::FILE* f, basic_string_view<Char> fmt,
              typename vprintf_args<Char>::type args) -> int {
  auto buf = basic_memory_buffer<Char>();
  detail::vprintf(buf, fmt, args);
  size_t size = buf.size();
  return std::fwrite(buf.data(), sizeof(Char), size, f) < size
             ? -1
             : static_cast<int>(size);
}

/**
 * Formats `args` according to specifications in `fmt` and writes the output
 * to `f`.
 *
 * **Example**:
 *
 *     fmt::fprintf(stderr, "Don't %s!", "panic");
 */
template <typename... T>
inline auto fprintf(std::FILE* f, string_view fmt, const T&... args) -> int {
  return vfprintf(f, fmt, make_printf_args(args...));
}
template <typename... T>
FMT_DEPRECATED auto fprintf(std::FILE* f, basic_string_view<wchar_t> fmt,
                            const T&... args) -> int {
  return vfprintf(f, fmt, make_printf_args<wchar_t>(args...));
}

/**
 * Formats `args` according to specifications in `fmt` and writes the output
 * to `stdout`.
 *
 * **Example**:
 *
 *   fmt::printf("Elapsed time: %.2f seconds", 1.23);
 */
template <typename... T>
inline auto printf(string_view fmt, const T&... args) -> int {
  return vfprintf(stdout, fmt, make_printf_args(args...));
}

FMT_END_EXPORT
FMT_END_NAMESPACE

#endif  // FMT_PRINTF_H_
