// Formatting library for C++ - the base API for char/UTF-8
//
// Copyright (c) 2012 - present, Victor Zverovich
// All rights reserved.
//
// For the license information refer to format.h.

#ifndef FMT_BASE_H_
#define FMT_BASE_H_

#if defined(FMT_IMPORT_STD) && !defined(FMT_MODULE)
#  define FMT_MODULE
#endif

#ifndef FMT_MODULE
#  include <limits.h>  // CHAR_BIT
#  include <stdio.h>   // FILE
#  include <string.h>  // memcmp

#  include <type_traits>  // std::enable_if
#endif

// The fmt library version in the form major * 10000 + minor * 100 + patch.
#define FMT_VERSION 110200

// Detect compiler versions.
#if defined(__clang__) && !defined(__ibmxl__)
#  define FMT_CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
#  define FMT_CLANG_VERSION 0
#endif
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  define FMT_GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#  define FMT_GCC_VERSION 0
#endif
#if defined(__ICL)
#  define FMT_ICC_VERSION __ICL
#elif defined(__INTEL_COMPILER)
#  define FMT_ICC_VERSION __INTEL_COMPILER
#else
#  define FMT_ICC_VERSION 0
#endif
#if defined(_MSC_VER)
#  define FMT_MSC_VERSION _MSC_VER
#else
#  define FMT_MSC_VERSION 0
#endif

// Detect standard library versions.
#ifdef _GLIBCXX_RELEASE
#  define FMT_GLIBCXX_RELEASE _GLIBCXX_RELEASE
#else
#  define FMT_GLIBCXX_RELEASE 0
#endif
#ifdef _LIBCPP_VERSION
#  define FMT_LIBCPP_VERSION _LIBCPP_VERSION
#else
#  define FMT_LIBCPP_VERSION 0
#endif

#ifdef _MSVC_LANG
#  define FMT_CPLUSPLUS _MSVC_LANG
#else
#  define FMT_CPLUSPLUS __cplusplus
#endif

// Detect __has_*.
#ifdef __has_feature
#  define FMT_HAS_FEATURE(x) __has_feature(x)
#else
#  define FMT_HAS_FEATURE(x) 0
#endif
#ifdef __has_include
#  define FMT_HAS_INCLUDE(x) __has_include(x)
#else
#  define FMT_HAS_INCLUDE(x) 0
#endif
#ifdef __has_builtin
#  define FMT_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define FMT_HAS_BUILTIN(x) 0
#endif
#ifdef __has_cpp_attribute
#  define FMT_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define FMT_HAS_CPP_ATTRIBUTE(x) 0
#endif

#define FMT_HAS_CPP14_ATTRIBUTE(attribute) \
  (FMT_CPLUSPLUS >= 201402L && FMT_HAS_CPP_ATTRIBUTE(attribute))

#define FMT_HAS_CPP17_ATTRIBUTE(attribute) \
  (FMT_CPLUSPLUS >= 201703L && FMT_HAS_CPP_ATTRIBUTE(attribute))

// Detect C++14 relaxed constexpr.
#ifdef FMT_USE_CONSTEXPR
// Use the provided definition.
#elif FMT_GCC_VERSION >= 702 && FMT_CPLUSPLUS >= 201402L
// GCC only allows constexpr member functions in non-literal types since 7.2:
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66297.
#  define FMT_USE_CONSTEXPR 1
#elif FMT_ICC_VERSION
#  define FMT_USE_CONSTEXPR 0  // https://github.com/fmtlib/fmt/issues/1628
#elif FMT_HAS_FEATURE(cxx_relaxed_constexpr) || FMT_MSC_VERSION >= 1912
#  define FMT_USE_CONSTEXPR 1
#else
#  define FMT_USE_CONSTEXPR 0
#endif
#if FMT_USE_CONSTEXPR
#  define FMT_CONSTEXPR constexpr
#else
#  define FMT_CONSTEXPR
#endif

// Detect consteval, C++20 constexpr extensions and std::is_constant_evaluated.
#if !defined(__cpp_lib_is_constant_evaluated)
#  define FMT_USE_CONSTEVAL 0
#elif FMT_CPLUSPLUS < 201709L
#  define FMT_USE_CONSTEVAL 0
#elif FMT_GLIBCXX_RELEASE && FMT_GLIBCXX_RELEASE < 10
#  define FMT_USE_CONSTEVAL 0
#elif FMT_LIBCPP_VERSION && FMT_LIBCPP_VERSION < 10000
#  define FMT_USE_CONSTEVAL 0
#elif defined(__apple_build_version__) && __apple_build_version__ < 14000029L
#  define FMT_USE_CONSTEVAL 0  // consteval is broken in Apple clang < 14.
#elif FMT_MSC_VERSION && FMT_MSC_VERSION < 1929
#  define FMT_USE_CONSTEVAL 0  // consteval is broken in MSVC VS2019 < 16.10.
#elif defined(__cpp_consteval)
#  define FMT_USE_CONSTEVAL 1
#elif FMT_GCC_VERSION >= 1002 || FMT_CLANG_VERSION >= 1101
#  define FMT_USE_CONSTEVAL 1
#else
#  define FMT_USE_CONSTEVAL 0
#endif
#if FMT_USE_CONSTEVAL
#  define FMT_CONSTEVAL consteval
#  define FMT_CONSTEXPR20 constexpr
#else
#  define FMT_CONSTEVAL
#  define FMT_CONSTEXPR20
#endif

// Check if exceptions are disabled.
#ifdef FMT_USE_EXCEPTIONS
// Use the provided definition.
#elif defined(__GNUC__) && !defined(__EXCEPTIONS)
#  define FMT_USE_EXCEPTIONS 0
#elif defined(__clang__) && !defined(__cpp_exceptions)
#  define FMT_USE_EXCEPTIONS 0
#elif FMT_MSC_VERSION && !_HAS_EXCEPTIONS
#  define FMT_USE_EXCEPTIONS 0
#else
#  define FMT_USE_EXCEPTIONS 1
#endif
#if FMT_USE_EXCEPTIONS
#  define FMT_TRY try
#  define FMT_CATCH(x) catch (x)
#else
#  define FMT_TRY if (true)
#  define FMT_CATCH(x) if (false)
#endif

#ifdef FMT_NO_UNIQUE_ADDRESS
// Use the provided definition.
#elif FMT_CPLUSPLUS < 202002L
// Not supported.
#elif FMT_HAS_CPP_ATTRIBUTE(no_unique_address)
#  define FMT_NO_UNIQUE_ADDRESS [[no_unique_address]]
// VS2019 v16.10 and later except clang-cl (https://reviews.llvm.org/D110485).
#elif FMT_MSC_VERSION >= 1929 && !FMT_CLANG_VERSION
#  define FMT_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#endif
#ifndef FMT_NO_UNIQUE_ADDRESS
#  define FMT_NO_UNIQUE_ADDRESS
#endif

#if FMT_HAS_CPP17_ATTRIBUTE(fallthrough)
#  define FMT_FALLTHROUGH [[fallthrough]]
#elif defined(__clang__)
#  define FMT_FALLTHROUGH [[clang::fallthrough]]
#elif FMT_GCC_VERSION >= 700 && \
    (!defined(__EDG_VERSION__) || __EDG_VERSION__ >= 520)
#  define FMT_FALLTHROUGH [[gnu::fallthrough]]
#else
#  define FMT_FALLTHROUGH
#endif

// Disable [[noreturn]] on MSVC/NVCC because of bogus unreachable code warnings.
#if FMT_HAS_CPP_ATTRIBUTE(noreturn) && !FMT_MSC_VERSION && !defined(__NVCC__)
#  define FMT_NORETURN [[noreturn]]
#else
#  define FMT_NORETURN
#endif

#ifdef FMT_NODISCARD
// Use the provided definition.
#elif FMT_HAS_CPP17_ATTRIBUTE(nodiscard)
#  define FMT_NODISCARD [[nodiscard]]
#else
#  define FMT_NODISCARD
#endif

#ifdef FMT_DEPRECATED
// Use the provided definition.
#elif FMT_HAS_CPP14_ATTRIBUTE(deprecated)
#  define FMT_DEPRECATED [[deprecated]]
#else
#  define FMT_DEPRECATED /* deprecated */
#endif

#if FMT_GCC_VERSION || FMT_CLANG_VERSION
#  define FMT_VISIBILITY(value) __attribute__((visibility(value)))
#else
#  define FMT_VISIBILITY(value)
#endif

// Detect pragmas.
#define FMT_PRAGMA_IMPL(x) _Pragma(#x)
#if FMT_GCC_VERSION >= 504 && !defined(__NVCOMPILER)
// Workaround a _Pragma bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59884
// and an nvhpc warning: https://github.com/fmtlib/fmt/pull/2582.
#  define FMT_PRAGMA_GCC(x) FMT_PRAGMA_IMPL(GCC x)
#else
#  define FMT_PRAGMA_GCC(x)
#endif
#if FMT_CLANG_VERSION
#  define FMT_PRAGMA_CLANG(x) FMT_PRAGMA_IMPL(clang x)
#else
#  define FMT_PRAGMA_CLANG(x)
#endif
#if FMT_MSC_VERSION
#  define FMT_MSC_WARNING(...) __pragma(warning(__VA_ARGS__))
#else
#  define FMT_MSC_WARNING(...)
#endif

// Enable minimal optimizations for more compact code in debug mode.
FMT_PRAGMA_GCC(push_options)
#if !defined(__OPTIMIZE__) && !defined(__CUDACC__) && !defined(FMT_MODULE)
FMT_PRAGMA_GCC(optimize("Og"))
#  define FMT_GCC_OPTIMIZED
#endif
FMT_PRAGMA_CLANG(diagnostic push)

#ifdef FMT_ALWAYS_INLINE
// Use the provided definition.
#elif FMT_GCC_VERSION || FMT_CLANG_VERSION
#  define FMT_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#  define FMT_ALWAYS_INLINE inline
#endif
// A version of FMT_ALWAYS_INLINE to prevent code bloat in debug mode.
#if defined(NDEBUG) || defined(FMT_GCC_OPTIMIZED)
#  define FMT_INLINE FMT_ALWAYS_INLINE
#else
#  define FMT_INLINE inline
#endif

#ifndef FMT_BEGIN_NAMESPACE
#  define FMT_BEGIN_NAMESPACE \
    namespace fmt {           \
    inline namespace v11 {
#  define FMT_END_NAMESPACE \
    }                       \
    }
#endif

#ifndef FMT_EXPORT
#  define FMT_EXPORT
#  define FMT_BEGIN_EXPORT
#  define FMT_END_EXPORT
#endif

#ifdef _WIN32
#  define FMT_WIN32 1
#else
#  define FMT_WIN32 0
#endif

#if !defined(FMT_HEADER_ONLY) && FMT_WIN32
#  if defined(FMT_LIB_EXPORT)
#    define FMT_API __declspec(dllexport)
#  elif defined(FMT_SHARED)
#    define FMT_API __declspec(dllimport)
#  endif
#elif defined(FMT_LIB_EXPORT) || defined(FMT_SHARED)
#  define FMT_API FMT_VISIBILITY("default")
#endif
#ifndef FMT_API
#  define FMT_API
#endif

#ifndef FMT_OPTIMIZE_SIZE
#  define FMT_OPTIMIZE_SIZE 0
#endif

// FMT_BUILTIN_TYPE=0 may result in smaller library size at the cost of higher
// per-call binary size by passing built-in types through the extension API.
#ifndef FMT_BUILTIN_TYPES
#  define FMT_BUILTIN_TYPES 1
#endif

#define FMT_APPLY_VARIADIC(expr) \
  using unused = int[];          \
  (void)unused { 0, (expr, 0)... }

FMT_BEGIN_NAMESPACE

// Implementations of enable_if_t and other metafunctions for older systems.
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
template <bool B, typename T, typename F>
using conditional_t = typename std::conditional<B, T, F>::type;
template <bool B> using bool_constant = std::integral_constant<bool, B>;
template <typename T>
using remove_reference_t = typename std::remove_reference<T>::type;
template <typename T>
using remove_const_t = typename std::remove_const<T>::type;
template <typename T>
using remove_cvref_t = typename std::remove_cv<remove_reference_t<T>>::type;
template <typename T>
using make_unsigned_t = typename std::make_unsigned<T>::type;
template <typename T>
using underlying_t = typename std::underlying_type<T>::type;
template <typename T> using decay_t = typename std::decay<T>::type;
using nullptr_t = decltype(nullptr);

#if (FMT_GCC_VERSION && FMT_GCC_VERSION < 500) || FMT_MSC_VERSION
// A workaround for gcc 4.9 & MSVC v141 to make void_t work in a SFINAE context.
template <typename...> struct void_t_impl {
  using type = void;
};
template <typename... T> using void_t = typename void_t_impl<T...>::type;
#else
template <typename...> using void_t = void;
#endif

struct monostate {
  constexpr monostate() {}
};

// An enable_if helper to be used in template parameters which results in much
// shorter symbols: https://godbolt.org/z/sWw4vP. Extra parentheses are needed
// to workaround a bug in MSVC 2019 (see #1140 and #1186).
#ifdef FMT_DOC
#  define FMT_ENABLE_IF(...)
#else
#  define FMT_ENABLE_IF(...) fmt::enable_if_t<(__VA_ARGS__), int> = 0
#endif

template <typename T> constexpr auto min_of(T a, T b) -> T {
  return a < b ? a : b;
}
template <typename T> constexpr auto max_of(T a, T b) -> T {
  return a > b ? a : b;
}

namespace detail {
// Suppresses "unused variable" warnings with the method described in
// https://herbsutter.com/2009/10/18/mailbag-shutting-up-compiler-warnings/.
// (void)var does not work on many Intel compilers.
template <typename... T> FMT_CONSTEXPR void ignore_unused(const T&...) {}

constexpr auto is_constant_evaluated(bool default_value = false) noexcept
    -> bool {
// Workaround for incompatibility between clang 14 and libstdc++ consteval-based
// std::is_constant_evaluated: https://github.com/fmtlib/fmt/issues/3247.
#if FMT_CPLUSPLUS >= 202002L && FMT_GLIBCXX_RELEASE >= 12 && \
    (FMT_CLANG_VERSION >= 1400 && FMT_CLANG_VERSION < 1500)
  ignore_unused(default_value);
  return __builtin_is_constant_evaluated();
#elif defined(__cpp_lib_is_constant_evaluated)
  ignore_unused(default_value);
  return std::is_constant_evaluated();
#else
  return default_value;
#endif
}

// Suppresses "conditional expression is constant" warnings.
template <typename T> FMT_ALWAYS_INLINE constexpr auto const_check(T val) -> T {
  return val;
}

FMT_NORETURN FMT_API void assert_fail(const char* file, int line,
                                      const char* message);

#if defined(FMT_ASSERT)
// Use the provided definition.
#elif defined(NDEBUG)
// FMT_ASSERT is not empty to avoid -Wempty-body.
#  define FMT_ASSERT(condition, message) \
    fmt::detail::ignore_unused((condition), (message))
#else
#  define FMT_ASSERT(condition, message)                                    \
    ((condition) /* void() fails with -Winvalid-constexpr on clang 4.0.1 */ \
         ? (void)0                                                          \
         : fmt::detail::assert_fail(__FILE__, __LINE__, (message)))
#endif

#ifdef FMT_USE_INT128
// Use the provided definition.
#elif defined(__SIZEOF_INT128__) && !defined(__NVCC__) && \
    !(FMT_CLANG_VERSION && FMT_MSC_VERSION)
#  define FMT_USE_INT128 1
using int128_opt = __int128_t;  // An optional native 128-bit integer.
using uint128_opt = __uint128_t;
inline auto map(int128_opt x) -> int128_opt { return x; }
inline auto map(uint128_opt x) -> uint128_opt { return x; }
#else
#  define FMT_USE_INT128 0
#endif
#if !FMT_USE_INT128
enum class int128_opt {};
enum class uint128_opt {};
// Reduce template instantiations.
inline auto map(int128_opt) -> monostate { return {}; }
inline auto map(uint128_opt) -> monostate { return {}; }
#endif

#ifndef FMT_USE_BITINT
#  define FMT_USE_BITINT (FMT_CLANG_VERSION >= 1500)
#endif

#if FMT_USE_BITINT
FMT_PRAGMA_CLANG(diagnostic ignored "-Wbit-int-extension")
template <int N> using bitint = _BitInt(N);
template <int N> using ubitint = unsigned _BitInt(N);
#else
template <int N> struct bitint {};
template <int N> struct ubitint {};
#endif  // FMT_USE_BITINT

// Casts a nonnegative integer to unsigned.
template <typename Int>
FMT_CONSTEXPR auto to_unsigned(Int value) -> make_unsigned_t<Int> {
  FMT_ASSERT(std::is_unsigned<Int>::value || value >= 0, "negative value");
  return static_cast<make_unsigned_t<Int>>(value);
}

template <typename Char>
using unsigned_char = conditional_t<sizeof(Char) == 1, unsigned char, unsigned>;

// A heuristic to detect std::string and std::[experimental::]string_view.
// It is mainly used to avoid dependency on <[experimental/]string_view>.
template <typename T, typename Enable = void>
struct is_std_string_like : std::false_type {};
template <typename T>
struct is_std_string_like<T, void_t<decltype(std::declval<T>().find_first_of(
                                 typename T::value_type(), 0))>>
    : std::is_convertible<decltype(std::declval<T>().data()),
                          const typename T::value_type*> {};

// Check if the literal encoding is UTF-8.
enum { is_utf8_enabled = "\u00A7"[1] == '\xA7' };
enum { use_utf8 = !FMT_WIN32 || is_utf8_enabled };

#ifndef FMT_UNICODE
#  define FMT_UNICODE 1
#endif

static_assert(!FMT_UNICODE || use_utf8,
              "Unicode support requires compiling with /utf-8");

template <typename T> constexpr const char* narrow(const T*) { return nullptr; }
constexpr FMT_ALWAYS_INLINE const char* narrow(const char* s) { return s; }

template <typename Char>
FMT_CONSTEXPR auto compare(const Char* s1, const Char* s2, std::size_t n)
    -> int {
  if (!is_constant_evaluated() && sizeof(Char) == 1) return memcmp(s1, s2, n);
  for (; n != 0; ++s1, ++s2, --n) {
    if (*s1 < *s2) return -1;
    if (*s1 > *s2) return 1;
  }
  return 0;
}

namespace adl {
using namespace std;

template <typename Container>
auto invoke_back_inserter()
    -> decltype(back_inserter(std::declval<Container&>()));
}  // namespace adl

template <typename It, typename Enable = std::true_type>
struct is_back_insert_iterator : std::false_type {};

template <typename It>
struct is_back_insert_iterator<
    It, bool_constant<std::is_same<
            decltype(adl::invoke_back_inserter<typename It::container_type>()),
            It>::value>> : std::true_type {};

// Extracts a reference to the container from *insert_iterator.
template <typename OutputIt>
inline FMT_CONSTEXPR20 auto get_container(OutputIt it) ->
    typename OutputIt::container_type& {
  struct accessor : OutputIt {
    FMT_CONSTEXPR20 accessor(OutputIt base) : OutputIt(base) {}
    using OutputIt::container;
  };
  return *accessor(it).container;
}
}  // namespace detail

// Parsing-related public API and forward declarations.
FMT_BEGIN_EXPORT

/**
 * An implementation of `std::basic_string_view` for pre-C++17. It provides a
 * subset of the API. `fmt::basic_string_view` is used for format strings even
 * if `std::basic_string_view` is available to prevent issues when a library is
 * compiled with a different `-std` option than the client code (which is not
 * recommended).
 */
template <typename Char> class basic_string_view {
 private:
  const Char* data_;
  size_t size_;

 public:
  using value_type = Char;
  using iterator = const Char*;

  constexpr basic_string_view() noexcept : data_(nullptr), size_(0) {}

  /// Constructs a string view object from a C string and a size.
  constexpr basic_string_view(const Char* s, size_t count) noexcept
      : data_(s), size_(count) {}

  constexpr basic_string_view(nullptr_t) = delete;

  /// Constructs a string view object from a C string.
#if FMT_GCC_VERSION
  FMT_ALWAYS_INLINE
#endif
  FMT_CONSTEXPR20 basic_string_view(const Char* s) : data_(s) {
#if FMT_HAS_BUILTIN(__builtin_strlen) || FMT_GCC_VERSION || FMT_CLANG_VERSION
    if (std::is_same<Char, char>::value && !detail::is_constant_evaluated()) {
      size_ = __builtin_strlen(detail::narrow(s));  // strlen is not costexpr.
      return;
    }
#endif
    size_t len = 0;
    while (*s++) ++len;
    size_ = len;
  }

  /// Constructs a string view from a `std::basic_string` or a
  /// `std::basic_string_view` object.
  template <typename S,
            FMT_ENABLE_IF(detail::is_std_string_like<S>::value&& std::is_same<
                          typename S::value_type, Char>::value)>
  FMT_CONSTEXPR basic_string_view(const S& s) noexcept
      : data_(s.data()), size_(s.size()) {}

  /// Returns a pointer to the string data.
  constexpr auto data() const noexcept -> const Char* { return data_; }

  /// Returns the string size.
  constexpr auto size() const noexcept -> size_t { return size_; }

  constexpr auto begin() const noexcept -> iterator { return data_; }
  constexpr auto end() const noexcept -> iterator { return data_ + size_; }

  constexpr auto operator[](size_t pos) const noexcept -> const Char& {
    return data_[pos];
  }

  FMT_CONSTEXPR void remove_prefix(size_t n) noexcept {
    data_ += n;
    size_ -= n;
  }

  FMT_CONSTEXPR auto starts_with(basic_string_view<Char> sv) const noexcept
      -> bool {
    return size_ >= sv.size_ && detail::compare(data_, sv.data_, sv.size_) == 0;
  }
  FMT_CONSTEXPR auto starts_with(Char c) const noexcept -> bool {
    return size_ >= 1 && *data_ == c;
  }
  FMT_CONSTEXPR auto starts_with(const Char* s) const -> bool {
    return starts_with(basic_string_view<Char>(s));
  }

  FMT_CONSTEXPR auto compare(basic_string_view other) const -> int {
    int result =
        detail::compare(data_, other.data_, min_of(size_, other.size_));
    if (result != 0) return result;
    return size_ == other.size_ ? 0 : (size_ < other.size_ ? -1 : 1);
  }

  FMT_CONSTEXPR friend auto operator==(basic_string_view lhs,
                                       basic_string_view rhs) -> bool {
    return lhs.compare(rhs) == 0;
  }
  friend auto operator!=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) != 0;
  }
  friend auto operator<(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) < 0;
  }
  friend auto operator<=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) <= 0;
  }
  friend auto operator>(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) > 0;
  }
  friend auto operator>=(basic_string_view lhs, basic_string_view rhs) -> bool {
    return lhs.compare(rhs) >= 0;
  }
};

using string_view = basic_string_view<char>;

// DEPRECATED! Will be merged with is_char and moved to detail.
template <typename T> struct is_xchar : std::false_type {};
template <> struct is_xchar<wchar_t> : std::true_type {};
template <> struct is_xchar<char16_t> : std::true_type {};
template <> struct is_xchar<char32_t> : std::true_type {};
#ifdef __cpp_char8_t
template <> struct is_xchar<char8_t> : std::true_type {};
#endif

// Specifies if `T` is a character (code unit) type.
template <typename T> struct is_char : is_xchar<T> {};
template <> struct is_char<char> : std::true_type {};

template <typename T> class basic_appender;
using appender = basic_appender<char>;

// Checks whether T is a container with contiguous storage.
template <typename T> struct is_contiguous : std::false_type {};

class context;
template <typename OutputIt, typename Char> class generic_context;
template <typename Char> class parse_context;

// Longer aliases for C++20 compatibility.
template <typename Char> using basic_format_parse_context = parse_context<Char>;
using format_parse_context = parse_context<char>;
template <typename OutputIt, typename Char>
using basic_format_context =
    conditional_t<std::is_same<OutputIt, appender>::value, context,
                  generic_context<OutputIt, Char>>;
using format_context = context;

template <typename Char>
using buffered_context =
    conditional_t<std::is_same<Char, char>::value, context,
                  generic_context<basic_appender<Char>, Char>>;

template <typename Context> class basic_format_arg;
template <typename Context> class basic_format_args;

// A separate type would result in shorter symbols but break ABI compatibility
// between clang and gcc on ARM (#1919).
using format_args = basic_format_args<context>;

// A formatter for objects of type T.
template <typename T, typename Char = char, typename Enable = void>
struct formatter {
  // A deleted default constructor indicates a disabled formatter.
  formatter() = delete;
};

/// Reports a format error at compile time or, via a `format_error` exception,
/// at runtime.
// This function is intentionally not constexpr to give a compile-time error.
FMT_NORETURN FMT_API void report_error(const char* message);

enum class presentation_type : unsigned char {
  // Common specifiers:
  none = 0,
  debug = 1,   // '?'
  string = 2,  // 's' (string, bool)

  // Integral, bool and character specifiers:
  dec = 3,  // 'd'
  hex,      // 'x' or 'X'
  oct,      // 'o'
  bin,      // 'b' or 'B'
  chr,      // 'c'

  // String and pointer specifiers:
  pointer = 3,  // 'p'

  // Floating-point specifiers:
  exp = 1,  // 'e' or 'E' (1 since there is no FP debug presentation)
  fixed,    // 'f' or 'F'
  general,  // 'g' or 'G'
  hexfloat  // 'a' or 'A'
};

enum class align { none, left, right, center, numeric };
enum class sign { none, minus, plus, space };
enum class arg_id_kind { none, index, name };

// Basic format specifiers for built-in and string types.
class basic_specs {
 private:
  // Data is arranged as follows:
  //
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |type |align| w | p | s |u|#|L|  f  |          unused           |
  // +-----+-----+---+---+---+-+-+-+-----+---------------------------+
  //
  //   w - dynamic width info
  //   p - dynamic precision info
  //   s - sign
  //   u - uppercase (e.g. 'X' for 'x')
  //   # - alternate form ('#')
  //   L - localized
  //   f - fill size
  //
  // Bitfields are not used because of compiler bugs such as gcc bug 61414.
  enum : unsigned {
    type_mask = 0x00007,
    align_mask = 0x00038,
    width_mask = 0x000C0,
    precision_mask = 0x00300,
    sign_mask = 0x00C00,
    uppercase_mask = 0x01000,
    alternate_mask = 0x02000,
    localized_mask = 0x04000,
    fill_size_mask = 0x38000,

    align_shift = 3,
    width_shift = 6,
    precision_shift = 8,
    sign_shift = 10,
    fill_size_shift = 15,

    max_fill_size = 4
  };

  unsigned data_ = 1 << fill_size_shift;
  static_assert(sizeof(basic_specs::data_) * CHAR_BIT >= 18, "");

  // Character (code unit) type is erased to prevent template bloat.
  char fill_data_[max_fill_size] = {' '};

  FMT_CONSTEXPR void set_fill_size(size_t size) {
    data_ = (data_ & ~fill_size_mask) |
            (static_cast<unsigned>(size) << fill_size_shift);
  }

 public:
  constexpr auto type() const -> presentation_type {
    return static_cast<presentation_type>(data_ & type_mask);
  }
  FMT_CONSTEXPR void set_type(presentation_type t) {
    data_ = (data_ & ~type_mask) | static_cast<unsigned>(t);
  }

  constexpr auto align() const -> align {
    return static_cast<fmt::align>((data_ & align_mask) >> align_shift);
  }
  FMT_CONSTEXPR void set_align(fmt::align a) {
    data_ = (data_ & ~align_mask) | (static_cast<unsigned>(a) << align_shift);
  }

  constexpr auto dynamic_width() const -> arg_id_kind {
    return static_cast<arg_id_kind>((data_ & width_mask) >> width_shift);
  }
  FMT_CONSTEXPR void set_dynamic_width(arg_id_kind w) {
    data_ = (data_ & ~width_mask) | (static_cast<unsigned>(w) << width_shift);
  }

  FMT_CONSTEXPR auto dynamic_precision() const -> arg_id_kind {
    return static_cast<arg_id_kind>((data_ & precision_mask) >>
                                    precision_shift);
  }
  FMT_CONSTEXPR void set_dynamic_precision(arg_id_kind p) {
    data_ = (data_ & ~precision_mask) |
            (static_cast<unsigned>(p) << precision_shift);
  }

  constexpr bool dynamic() const {
    return (data_ & (width_mask | precision_mask)) != 0;
  }

  constexpr auto sign() const -> sign {
    return static_cast<fmt::sign>((data_ & sign_mask) >> sign_shift);
  }
  FMT_CONSTEXPR void set_sign(fmt::sign s) {
    data_ = (data_ & ~sign_mask) | (static_cast<unsigned>(s) << sign_shift);
  }

  constexpr auto upper() const -> bool { return (data_ & uppercase_mask) != 0; }
  FMT_CONSTEXPR void set_upper() { data_ |= uppercase_mask; }

  constexpr auto alt() const -> bool { return (data_ & alternate_mask) != 0; }
  FMT_CONSTEXPR void set_alt() { data_ |= alternate_mask; }
  FMT_CONSTEXPR void clear_alt() { data_ &= ~alternate_mask; }

  constexpr auto localized() const -> bool {
    return (data_ & localized_mask) != 0;
  }
  FMT_CONSTEXPR void set_localized() { data_ |= localized_mask; }

  constexpr auto fill_size() const -> size_t {
    return (data_ & fill_size_mask) >> fill_size_shift;
  }

  template <typename Char, FMT_ENABLE_IF(std::is_same<Char, char>::value)>
  constexpr auto fill() const -> const Char* {
    return fill_data_;
  }
  template <typename Char, FMT_ENABLE_IF(!std::is_same<Char, char>::value)>
  constexpr auto fill() const -> const Char* {
    return nullptr;
  }

  template <typename Char> constexpr auto fill_unit() const -> Char {
    using uchar = unsigned char;
    return static_cast<Char>(static_cast<uchar>(fill_data_[0]) |
                             (static_cast<uchar>(fill_data_[1]) << 8) |
                             (static_cast<uchar>(fill_data_[2]) << 16));
  }

  FMT_CONSTEXPR void set_fill(char c) {
    fill_data_[0] = c;
    set_fill_size(1);
  }

  template <typename Char>
  FMT_CONSTEXPR void set_fill(basic_string_view<Char> s) {
    auto size = s.size();
    set_fill_size(size);
    if (size == 1) {
      unsigned uchar = static_cast<detail::unsigned_char<Char>>(s[0]);
      fill_data_[0] = static_cast<char>(uchar);
      fill_data_[1] = static_cast<char>(uchar >> 8);
      fill_data_[2] = static_cast<char>(uchar >> 16);
      return;
    }
    FMT_ASSERT(size <= max_fill_size, "invalid fill");
    for (size_t i = 0; i < size; ++i)
      fill_data_[i & 3] = static_cast<char>(s[i]);
  }

  FMT_CONSTEXPR void copy_fill_from(const basic_specs& specs) {
    set_fill_size(specs.fill_size());
    for (size_t i = 0; i < max_fill_size; ++i)
      fill_data_[i] = specs.fill_data_[i];
  }
};

// Format specifiers for built-in and string types.
struct format_specs : basic_specs {
  int width;
  int precision;

  constexpr format_specs() : width(0), precision(-1) {}
};

/**
 * Parsing context consisting of a format string range being parsed and an
 * argument counter for automatic indexing.
 */
template <typename Char = char> class parse_context {
 private:
  basic_string_view<Char> fmt_;
  int next_arg_id_;

  enum { use_constexpr_cast = !FMT_GCC_VERSION || FMT_GCC_VERSION >= 1200 };

  FMT_CONSTEXPR void do_check_arg_id(int arg_id);

 public:
  using char_type = Char;
  using iterator = const Char*;

  constexpr explicit parse_context(basic_string_view<Char> fmt,
                                   int next_arg_id = 0)
      : fmt_(fmt), next_arg_id_(next_arg_id) {}

  /// Returns an iterator to the beginning of the format string range being
  /// parsed.
  constexpr auto begin() const noexcept -> iterator { return fmt_.begin(); }

  /// Returns an iterator past the end of the format string range being parsed.
  constexpr auto end() const noexcept -> iterator { return fmt_.end(); }

  /// Advances the begin iterator to `it`.
  FMT_CONSTEXPR void advance_to(iterator it) {
    fmt_.remove_prefix(detail::to_unsigned(it - begin()));
  }

  /// Reports an error if using the manual argument indexing; otherwise returns
  /// the next argument index and switches to the automatic indexing.
  FMT_CONSTEXPR auto next_arg_id() -> int {
    if (next_arg_id_ < 0) {
      report_error("cannot switch from manual to automatic argument indexing");
      return 0;
    }
    int id = next_arg_id_++;
    do_check_arg_id(id);
    return id;
  }

  /// Reports an error if using the automatic argument indexing; otherwise
  /// switches to the manual indexing.
  FMT_CONSTEXPR void check_arg_id(int id) {
    if (next_arg_id_ > 0) {
      report_error("cannot switch from automatic to manual argument indexing");
      return;
    }
    next_arg_id_ = -1;
    do_check_arg_id(id);
  }
  FMT_CONSTEXPR void check_arg_id(basic_string_view<Char>) {
    next_arg_id_ = -1;
  }
  FMT_CONSTEXPR void check_dynamic_spec(int arg_id);
};

FMT_END_EXPORT

namespace detail {

// Constructs fmt::basic_string_view<Char> from types implicitly convertible
// to it, deducing Char. Explicitly convertible types such as the ones returned
// from FMT_STRING are intentionally excluded.
template <typename Char, FMT_ENABLE_IF(is_char<Char>::value)>
constexpr auto to_string_view(const Char* s) -> basic_string_view<Char> {
  return s;
}
template <typename T, FMT_ENABLE_IF(is_std_string_like<T>::value)>
constexpr auto to_string_view(const T& s)
    -> basic_string_view<typename T::value_type> {
  return s;
}
template <typename Char>
constexpr auto to_string_view(basic_string_view<Char> s)
    -> basic_string_view<Char> {
  return s;
}

template <typename T, typename Enable = void>
struct has_to_string_view : std::false_type {};
// detail:: is intentional since to_string_view is not an extension point.
template <typename T>
struct has_to_string_view<
    T, void_t<decltype(detail::to_string_view(std::declval<T>()))>>
    : std::true_type {};

/// String's character (code unit) type. detail:: is intentional to prevent ADL.
template <typename S,
          typename V = decltype(detail::to_string_view(std::declval<S>()))>
using char_t = typename V::value_type;

enum class type {
  none_type,
  // Integer types should go first,
  int_type,
  uint_type,
  long_long_type,
  ulong_long_type,
  int128_type,
  uint128_type,
  bool_type,
  char_type,
  last_integer_type = char_type,
  // followed by floating-point types.
  float_type,
  double_type,
  long_double_type,
  last_numeric_type = long_double_type,
  cstring_type,
  string_type,
  pointer_type,
  custom_type
};

// Maps core type T to the corresponding type enum constant.
template <typename T, typename Char>
struct type_constant : std::integral_constant<type, type::custom_type> {};

#define FMT_TYPE_CONSTANT(Type, constant) \
  template <typename Char>                \
  struct type_constant<Type, Char>        \
      : std::integral_constant<type, type::constant> {}

FMT_TYPE_CONSTANT(int, int_type);
FMT_TYPE_CONSTANT(unsigned, uint_type);
FMT_TYPE_CONSTANT(long long, long_long_type);
FMT_TYPE_CONSTANT(unsigned long long, ulong_long_type);
FMT_TYPE_CONSTANT(int128_opt, int128_type);
FMT_TYPE_CONSTANT(uint128_opt, uint128_type);
FMT_TYPE_CONSTANT(bool, bool_type);
FMT_TYPE_CONSTANT(Char, char_type);
FMT_TYPE_CONSTANT(float, float_type);
FMT_TYPE_CONSTANT(double, double_type);
FMT_TYPE_CONSTANT(long double, long_double_type);
FMT_TYPE_CONSTANT(const Char*, cstring_type);
FMT_TYPE_CONSTANT(basic_string_view<Char>, string_type);
FMT_TYPE_CONSTANT(const void*, pointer_type);

constexpr auto is_integral_type(type t) -> bool {
  return t > type::none_type && t <= type::last_integer_type;
}
constexpr auto is_arithmetic_type(type t) -> bool {
  return t > type::none_type && t <= type::last_numeric_type;
}

constexpr auto set(type rhs) -> int { return 1 << static_cast<int>(rhs); }
constexpr auto in(type t, int set) -> bool {
  return ((set >> static_cast<int>(t)) & 1) != 0;
}

// Bitsets of types.
enum {
  sint_set =
      set(type::int_type) | set(type::long_long_type) | set(type::int128_type),
  uint_set = set(type::uint_type) | set(type::ulong_long_type) |
             set(type::uint128_type),
  bool_set = set(type::bool_type),
  char_set = set(type::char_type),
  float_set = set(type::float_type) | set(type::double_type) |
              set(type::long_double_type),
  string_set = set(type::string_type),
  cstring_set = set(type::cstring_type),
  pointer_set = set(type::pointer_type)
};

struct view {};

template <typename T, typename Enable = std::true_type>
struct is_view : std::false_type {};
template <typename T>
struct is_view<T, bool_constant<sizeof(T) != 0>> : std::is_base_of<view, T> {};

template <typename Char, typename T> struct named_arg;
template <typename T> struct is_named_arg : std::false_type {};
template <typename T> struct is_static_named_arg : std::false_type {};

template <typename Char, typename T>
struct is_named_arg<named_arg<Char, T>> : std::true_type {};

template <typename Char, typename T> struct named_arg : view {
  const Char* name;
  const T& value;

  named_arg(const Char* n, const T& v) : name(n), value(v) {}
  static_assert(!is_named_arg<T>::value, "nested named arguments");
};

template <bool B = false> constexpr auto count() -> int { return B ? 1 : 0; }
template <bool B1, bool B2, bool... Tail> constexpr auto count() -> int {
  return (B1 ? 1 : 0) + count<B2, Tail...>();
}

template <typename... Args> constexpr auto count_named_args() -> int {
  return count<is_named_arg<Args>::value...>();
}
template <typename... Args> constexpr auto count_static_named_args() -> int {
  return count<is_static_named_arg<Args>::value...>();
}

template <typename Char> struct named_arg_info {
  const Char* name;
  int id;
};

// named_args is non-const to suppress a bogus -Wmaybe-uninitalized in gcc 13.
template <typename Char>
FMT_CONSTEXPR void check_for_duplicate(named_arg_info<Char>* named_args,
                                       int named_arg_index,
                                       basic_string_view<Char> arg_name) {
  for (int i = 0; i < named_arg_index; ++i) {
    if (named_args[i].name == arg_name) report_error("duplicate named arg");
  }
}

template <typename Char, typename T, FMT_ENABLE_IF(!is_named_arg<T>::value)>
void init_named_arg(named_arg_info<Char>*, int& arg_index, int&, const T&) {
  ++arg_index;
}
template <typename Char, typename T, FMT_ENABLE_IF(is_named_arg<T>::value)>
void init_named_arg(named_arg_info<Char>* named_args, int& arg_index,
                    int& named_arg_index, const T& arg) {
  check_for_duplicate<Char>(named_args, named_arg_index, arg.name);
  named_args[named_arg_index++] = {arg.name, arg_index++};
}

template <typename T, typename Char,
          FMT_ENABLE_IF(!is_static_named_arg<T>::value)>
FMT_CONSTEXPR void init_static_named_arg(named_arg_info<Char>*, int& arg_index,
                                         int&) {
  ++arg_index;
}
template <typename T, typename Char,
          FMT_ENABLE_IF(is_static_named_arg<T>::value)>
FMT_CONSTEXPR void init_static_named_arg(named_arg_info<Char>* named_args,
                                         int& arg_index, int& named_arg_index) {
  check_for_duplicate<Char>(named_args, named_arg_index, T::name);
  named_args[named_arg_index++] = {T::name, arg_index++};
}

// To minimize the number of types we need to deal with, long is translated
// either to int or to long long depending on its size.
enum { long_short = sizeof(long) == sizeof(int) && FMT_BUILTIN_TYPES };
using long_type = conditional_t<long_short, int, long long>;
using ulong_type = conditional_t<long_short, unsigned, unsigned long long>;

template <typename T>
using format_as_result =
    remove_cvref_t<decltype(format_as(std::declval<const T&>()))>;
template <typename T>
using format_as_member_result =
    remove_cvref_t<decltype(formatter<T>::format_as(std::declval<const T&>()))>;

template <typename T, typename Enable = std::true_type>
struct use_format_as : std::false_type {};
// format_as member is only used to avoid injection into the std namespace.
template <typename T, typename Enable = std::true_type>
struct use_format_as_member : std::false_type {};

// Only map owning types because mapping views can be unsafe.
template <typename T>
struct use_format_as<
    T, bool_constant<std::is_arithmetic<format_as_result<T>>::value>>
    : std::true_type {};
template <typename T>
struct use_format_as_member<
    T, bool_constant<std::is_arithmetic<format_as_member_result<T>>::value>>
    : std::true_type {};

template <typename T, typename U = remove_const_t<T>>
using use_formatter =
    bool_constant<(std::is_class<T>::value || std::is_enum<T>::value ||
                   std::is_union<T>::value || std::is_array<T>::value) &&
                  !has_to_string_view<T>::value && !is_named_arg<T>::value &&
                  !use_format_as<T>::value && !use_format_as_member<U>::value>;

template <typename Char, typename T, typename U = remove_const_t<T>>
auto has_formatter_impl(T* p, buffered_context<Char>* ctx = nullptr)
    -> decltype(formatter<U, Char>().format(*p, *ctx), std::true_type());
template <typename Char> auto has_formatter_impl(...) -> std::false_type;

// T can be const-qualified to check if it is const-formattable.
template <typename T, typename Char> constexpr auto has_formatter() -> bool {
  return decltype(has_formatter_impl<Char>(static_cast<T*>(nullptr)))::value;
}

// Maps formatting argument types to natively supported types or user-defined
// types with formatters. Returns void on errors to be SFINAE-friendly.
template <typename Char> struct type_mapper {
  static auto map(signed char) -> int;
  static auto map(unsigned char) -> unsigned;
  static auto map(short) -> int;
  static auto map(unsigned short) -> unsigned;
  static auto map(int) -> int;
  static auto map(unsigned) -> unsigned;
  static auto map(long) -> long_type;
  static auto map(unsigned long) -> ulong_type;
  static auto map(long long) -> long long;
  static auto map(unsigned long long) -> unsigned long long;
  static auto map(int128_opt) -> int128_opt;
  static auto map(uint128_opt) -> uint128_opt;
  static auto map(bool) -> bool;

  template <int N>
  static auto map(bitint<N>) -> conditional_t<N <= 64, long long, void>;
  template <int N>
  static auto map(ubitint<N>)
      -> conditional_t<N <= 64, unsigned long long, void>;

  template <typename T, FMT_ENABLE_IF(is_char<T>::value)>
  static auto map(T) -> conditional_t<
      std::is_same<T, char>::value || std::is_same<T, Char>::value, Char, void>;

  static auto map(float) -> float;
  static auto map(double) -> double;
  static auto map(long double) -> long double;

  static auto map(Char*) -> const Char*;
  static auto map(const Char*) -> const Char*;
  template <typename T, typename C = char_t<T>,
            FMT_ENABLE_IF(!std::is_pointer<T>::value)>
  static auto map(const T&) -> conditional_t<std::is_same<C, Char>::value,
                                             basic_string_view<C>, void>;

  static auto map(void*) -> const void*;
  static auto map(const void*) -> const void*;
  static auto map(volatile void*) -> const void*;
  static auto map(const volatile void*) -> const void*;
  static auto map(nullptr_t) -> const void*;
  template <typename T, FMT_ENABLE_IF(std::is_pointer<T>::value ||
                                      std::is_member_pointer<T>::value)>
  static auto map(const T&) -> void;

  template <typename T, FMT_ENABLE_IF(use_format_as<T>::value)>
  static auto map(const T& x) -> decltype(map(format_as(x)));
  template <typename T, FMT_ENABLE_IF(use_format_as_member<T>::value)>
  static auto map(const T& x) -> decltype(map(formatter<T>::format_as(x)));

  template <typename T, FMT_ENABLE_IF(use_formatter<T>::value)>
  static auto map(T&) -> conditional_t<has_formatter<T, Char>(), T&, void>;

  template <typename T, FMT_ENABLE_IF(is_named_arg<T>::value)>
  static auto map(const T& named_arg) -> decltype(map(named_arg.value));
};

// detail:: is used to workaround a bug in MSVC 2017.
template <typename T, typename Char>
using mapped_t = decltype(detail::type_mapper<Char>::map(std::declval<T&>()));

// A type constant after applying type_mapper.
template <typename T, typename Char = char>
using mapped_type_constant = type_constant<mapped_t<T, Char>, Char>;

template <typename T, typename Context,
          type TYPE =
              mapped_type_constant<T, typename Context::char_type>::value>
using stored_type_constant = std::integral_constant<
    type, Context::builtin_types || TYPE == type::int_type ? TYPE
                                                           : type::custom_type>;
// A parse context with extra data used only in compile-time checks.
template <typename Char>
class compile_parse_context : public parse_context<Char> {
 private:
  int num_args_;
  const type* types_;
  using base = parse_context<Char>;

 public:
  FMT_CONSTEXPR explicit compile_parse_context(basic_string_view<Char> fmt,
                                               int num_args, const type* types,
                                               int next_arg_id = 0)
      : base(fmt, next_arg_id), num_args_(num_args), types_(types) {}

  constexpr auto num_args() const -> int { return num_args_; }
  constexpr auto arg_type(int id) const -> type { return types_[id]; }

  FMT_CONSTEXPR auto next_arg_id() -> int {
    int id = base::next_arg_id();
    if (id >= num_args_) report_error("argument not found");
    return id;
  }

  FMT_CONSTEXPR void check_arg_id(int id) {
    base::check_arg_id(id);
    if (id >= num_args_) report_error("argument not found");
  }
  using base::check_arg_id;

  FMT_CONSTEXPR void check_dynamic_spec(int arg_id) {
    ignore_unused(arg_id);
    if (arg_id < num_args_ && types_ && !is_integral_type(types_[arg_id]))
      report_error("width/precision is not integer");
  }
};

// An argument reference.
template <typename Char> union arg_ref {
  FMT_CONSTEXPR arg_ref(int idx = 0) : index(idx) {}
  FMT_CONSTEXPR arg_ref(basic_string_view<Char> n) : name(n) {}

  int index;
  basic_string_view<Char> name;
};

// Format specifiers with width and precision resolved at formatting rather
// than parsing time to allow reusing the same parsed specifiers with
// different sets of arguments (precompilation of format strings).
template <typename Char = char> struct dynamic_format_specs : format_specs {
  arg_ref<Char> width_ref;
  arg_ref<Char> precision_ref;
};

// Converts a character to ASCII. Returns '\0' on conversion failure.
template <typename Char, FMT_ENABLE_IF(std::is_integral<Char>::value)>
constexpr auto to_ascii(Char c) -> char {
  return c <= 0xff ? static_cast<char>(c) : '\0';
}

// Returns the number of code units in a code point or 1 on error.
template <typename Char>
FMT_CONSTEXPR auto code_point_length(const Char* begin) -> int {
  if (const_check(sizeof(Char) != 1)) return 1;
  auto c = static_cast<unsigned char>(*begin);
  return static_cast<int>((0x3a55000000000000ull >> (2 * (c >> 3))) & 3) + 1;
}

// Parses the range [begin, end) as an unsigned integer. This function assumes
// that the range is non-empty and the first character is a digit.
template <typename Char>
FMT_CONSTEXPR auto parse_nonnegative_int(const Char*& begin, const Char* end,
                                         int error_value) noexcept -> int {
  FMT_ASSERT(begin != end && '0' <= *begin && *begin <= '9', "");
  unsigned value = 0, prev = 0;
  auto p = begin;
  do {
    prev = value;
    value = value * 10 + unsigned(*p - '0');
    ++p;
  } while (p != end && '0' <= *p && *p <= '9');
  auto num_digits = p - begin;
  begin = p;
  int digits10 = static_cast<int>(sizeof(int) * CHAR_BIT * 3 / 10);
  if (num_digits <= digits10) return static_cast<int>(value);
  // Check for overflow.
  unsigned max = INT_MAX;
  return num_digits == digits10 + 1 &&
                 prev * 10ull + unsigned(p[-1] - '0') <= max
             ? static_cast<int>(value)
             : error_value;
}

FMT_CONSTEXPR inline auto parse_align(char c) -> align {
  switch (c) {
  case '<': return align::left;
  case '>': return align::right;
  case '^': return align::center;
  }
  return align::none;
}

template <typename Char> constexpr auto is_name_start(Char c) -> bool {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

template <typename Char, typename Handler>
FMT_CONSTEXPR auto parse_arg_id(const Char* begin, const Char* end,
                                Handler&& handler) -> const Char* {
  Char c = *begin;
  if (c >= '0' && c <= '9') {
    int index = 0;
    if (c != '0')
      index = parse_nonnegative_int(begin, end, INT_MAX);
    else
      ++begin;
    if (begin == end || (*begin != '}' && *begin != ':'))
      report_error("invalid format string");
    else
      handler.on_index(index);
    return begin;
  }
  if (FMT_OPTIMIZE_SIZE > 1 || !is_name_start(c)) {
    report_error("invalid format string");
    return begin;
  }
  auto it = begin;
  do {
    ++it;
  } while (it != end && (is_name_start(*it) || ('0' <= *it && *it <= '9')));
  handler.on_name({begin, to_unsigned(it - begin)});
  return it;
}

template <typename Char> struct dynamic_spec_handler {
  parse_context<Char>& ctx;
  arg_ref<Char>& ref;
  arg_id_kind& kind;

  FMT_CONSTEXPR void on_index(int id) {
    ref = id;
    kind = arg_id_kind::index;
    ctx.check_arg_id(id);
    ctx.check_dynamic_spec(id);
  }
  FMT_CONSTEXPR void on_name(basic_string_view<Char> id) {
    ref = id;
    kind = arg_id_kind::name;
    ctx.check_arg_id(id);
  }
};

template <typename Char> struct parse_dynamic_spec_result {
  const Char* end;
  arg_id_kind kind;
};

// Parses integer | "{" [arg_id] "}".
template <typename Char>
FMT_CONSTEXPR auto parse_dynamic_spec(const Char* begin, const Char* end,
                                      int& value, arg_ref<Char>& ref,
                                      parse_context<Char>& ctx)
    -> parse_dynamic_spec_result<Char> {
  FMT_ASSERT(begin != end, "");
  auto kind = arg_id_kind::none;
  if ('0' <= *begin && *begin <= '9') {
    int val = parse_nonnegative_int(begin, end, -1);
    if (val == -1) report_error("number is too big");
    value = val;
  } else {
    if (*begin == '{') {
      ++begin;
      if (begin != end) {
        Char c = *begin;
        if (c == '}' || c == ':') {
          int id = ctx.next_arg_id();
          ref = id;
          kind = arg_id_kind::index;
          ctx.check_dynamic_spec(id);
        } else {
          begin = parse_arg_id(begin, end,
                               dynamic_spec_handler<Char>{ctx, ref, kind});
        }
      }
      if (begin != end && *begin == '}') return {++begin, kind};
    }
    report_error("invalid format string");
  }
  return {begin, kind};
}

template <typename Char>
FMT_CONSTEXPR auto parse_width(const Char* begin, const Char* end,
                               format_specs& specs, arg_ref<Char>& width_ref,
                               parse_context<Char>& ctx) -> const Char* {
  auto result = parse_dynamic_spec(begin, end, specs.width, width_ref, ctx);
  specs.set_dynamic_width(result.kind);
  return result.end;
}

template <typename Char>
FMT_CONSTEXPR auto parse_precision(const Char* begin, const Char* end,
                                   format_specs& specs,
                                   arg_ref<Char>& precision_ref,
                                   parse_context<Char>& ctx) -> const Char* {
  ++begin;
  if (begin == end) {
    report_error("invalid precision");
    return begin;
  }
  auto result =
      parse_dynamic_spec(begin, end, specs.precision, precision_ref, ctx);
  specs.set_dynamic_precision(result.kind);
  return result.end;
}

enum class state { start, align, sign, hash, zero, width, precision, locale };

// Parses standard format specifiers.
template <typename Char>
FMT_CONSTEXPR auto parse_format_specs(const Char* begin, const Char* end,
                                      dynamic_format_specs<Char>& specs,
                                      parse_context<Char>& ctx, type arg_type)
    -> const Char* {
  auto c = '\0';
  if (end - begin > 1) {
    auto next = to_ascii(begin[1]);
    c = parse_align(next) == align::none ? to_ascii(*begin) : '\0';
  } else {
    if (begin == end) return begin;
    c = to_ascii(*begin);
  }

  struct {
    state current_state = state::start;
    FMT_CONSTEXPR void operator()(state s, bool valid = true) {
      if (current_state >= s || !valid)
        report_error("invalid format specifier");
      current_state = s;
    }
  } enter_state;

  using pres = presentation_type;
  constexpr auto integral_set = sint_set | uint_set | bool_set | char_set;
  struct {
    const Char*& begin;
    format_specs& specs;
    type arg_type;

    FMT_CONSTEXPR auto operator()(pres pres_type, int set) -> const Char* {
      if (!in(arg_type, set)) report_error("invalid format specifier");
      specs.set_type(pres_type);
      return begin + 1;
    }
  } parse_presentation_type{begin, specs, arg_type};

  for (;;) {
    switch (c) {
    case '<':
    case '>':
    case '^':
      enter_state(state::align);
      specs.set_align(parse_align(c));
      ++begin;
      break;
    case '+':
    case ' ':
      specs.set_sign(c == ' ' ? sign::space : sign::plus);
      FMT_FALLTHROUGH;
    case '-':
      enter_state(state::sign, in(arg_type, sint_set | float_set));
      ++begin;
      break;
    case '#':
      enter_state(state::hash, is_arithmetic_type(arg_type));
      specs.set_alt();
      ++begin;
      break;
    case '0':
      enter_state(state::zero);
      if (!is_arithmetic_type(arg_type))
        report_error("format specifier requires numeric argument");
      if (specs.align() == align::none) {
        // Ignore 0 if align is specified for compatibility with std::format.
        specs.set_align(align::numeric);
        specs.set_fill('0');
      }
      ++begin;
      break;
      // clang-format off
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': case '{':
      // clang-format on
      enter_state(state::width);
      begin = parse_width(begin, end, specs, specs.width_ref, ctx);
      break;
    case '.':
      enter_state(state::precision,
                  in(arg_type, float_set | string_set | cstring_set));
      begin = parse_precision(begin, end, specs, specs.precision_ref, ctx);
      break;
    case 'L':
      enter_state(state::locale, is_arithmetic_type(arg_type));
      specs.set_localized();
      ++begin;
      break;
    case 'd': return parse_presentation_type(pres::dec, integral_set);
    case 'X': specs.set_upper(); FMT_FALLTHROUGH;
    case 'x': return parse_presentation_type(pres::hex, integral_set);
    case 'o': return parse_presentation_type(pres::oct, integral_set);
    case 'B': specs.set_upper(); FMT_FALLTHROUGH;
    case 'b': return parse_presentation_type(pres::bin, integral_set);
    case 'E': specs.set_upper(); FMT_FALLTHROUGH;
    case 'e': return parse_presentation_type(pres::exp, float_set);
    case 'F': specs.set_upper(); FMT_FALLTHROUGH;
    case 'f': return parse_presentation_type(pres::fixed, float_set);
    case 'G': specs.set_upper(); FMT_FALLTHROUGH;
    case 'g': return parse_presentation_type(pres::general, float_set);
    case 'A': specs.set_upper(); FMT_FALLTHROUGH;
    case 'a': return parse_presentation_type(pres::hexfloat, float_set);
    case 'c':
      if (arg_type == type::bool_type) report_error("invalid format specifier");
      return parse_presentation_type(pres::chr, integral_set);
    case 's':
      return parse_presentation_type(pres::string,
                                     bool_set | string_set | cstring_set);
    case 'p':
      return parse_presentation_type(pres::pointer, pointer_set | cstring_set);
    case '?':
      return parse_presentation_type(pres::debug,
                                     char_set | string_set | cstring_set);
    case '}': return begin;
    default:  {
      if (*begin == '}') return begin;
      // Parse fill and alignment.
      auto fill_end = begin + code_point_length(begin);
      if (end - fill_end <= 0) {
        report_error("invalid format specifier");
        return begin;
      }
      if (*begin == '{') {
        report_error("invalid fill character '{'");
        return begin;
      }
      auto alignment = parse_align(to_ascii(*fill_end));
      enter_state(state::align, alignment != align::none);
      specs.set_fill(
          basic_string_view<Char>(begin, to_unsigned(fill_end - begin)));
      specs.set_align(alignment);
      begin = fill_end + 1;
    }
    }
    if (begin == end) return begin;
    c = to_ascii(*begin);
  }
}

template <typename Char, typename Handler>
FMT_CONSTEXPR FMT_INLINE auto parse_replacement_field(const Char* begin,
                                                      const Char* end,
                                                      Handler&& handler)
    -> const Char* {
  ++begin;
  if (begin == end) {
    handler.on_error("invalid format string");
    return end;
  }
  int arg_id = 0;
  switch (*begin) {
  case '}':
    handler.on_replacement_field(handler.on_arg_id(), begin);
    return begin + 1;
  case '{': handler.on_text(begin, begin + 1); return begin + 1;
  case ':': arg_id = handler.on_arg_id(); break;
  default:  {
    struct id_adapter {
      Handler& handler;
      int arg_id;

      FMT_CONSTEXPR void on_index(int id) { arg_id = handler.on_arg_id(id); }
      FMT_CONSTEXPR void on_name(basic_string_view<Char> id) {
        arg_id = handler.on_arg_id(id);
      }
    } adapter = {handler, 0};
    begin = parse_arg_id(begin, end, adapter);
    arg_id = adapter.arg_id;
    Char c = begin != end ? *begin : Char();
    if (c == '}') {
      handler.on_replacement_field(arg_id, begin);
      return begin + 1;
    }
    if (c != ':') {
      handler.on_error("missing '}' in format string");
      return end;
    }
    break;
  }
  }
  begin = handler.on_format_specs(arg_id, begin + 1, end);
  if (begin == end || *begin != '}')
    return handler.on_error("unknown format specifier"), end;
  return begin + 1;
}

template <typename Char, typename Handler>
FMT_CONSTEXPR void parse_format_string(basic_string_view<Char> fmt,
                                       Handler&& handler) {
  auto begin = fmt.data(), end = begin + fmt.size();
  auto p = begin;
  while (p != end) {
    auto c = *p++;
    if (c == '{') {
      handler.on_text(begin, p - 1);
      begin = p = parse_replacement_field(p - 1, end, handler);
    } else if (c == '}') {
      if (p == end || *p != '}')
        return handler.on_error("unmatched '}' in format string");
      handler.on_text(begin, p);
      begin = ++p;
    }
  }
  handler.on_text(begin, end);
}

// Checks char specs and returns true iff the presentation type is char-like.
FMT_CONSTEXPR inline auto check_char_specs(const format_specs& specs) -> bool {
  auto type = specs.type();
  if (type != presentation_type::none && type != presentation_type::chr &&
      type != presentation_type::debug) {
    return false;
  }
  if (specs.align() == align::numeric || specs.sign() != sign::none ||
      specs.alt()) {
    report_error("invalid format specifier for char");
  }
  return true;
}

// A base class for compile-time strings.
struct compile_string {};

template <typename T, typename Char>
FMT_VISIBILITY("hidden")  // Suppress an ld warning on macOS (#3769).
FMT_CONSTEXPR auto invoke_parse(parse_context<Char>& ctx) -> const Char* {
  using mapped_type = remove_cvref_t<mapped_t<T, Char>>;
  constexpr bool formattable =
      std::is_constructible<formatter<mapped_type, Char>>::value;
  if (!formattable) return ctx.begin();  // Error is reported in the value ctor.
  using formatted_type = conditional_t<formattable, mapped_type, int>;
  return formatter<formatted_type, Char>().parse(ctx);
}

template <typename... T> struct arg_pack {};

template <typename Char, int NUM_ARGS, int NUM_NAMED_ARGS, bool DYNAMIC_NAMES>
class format_string_checker {
 private:
  type types_[max_of(1, NUM_ARGS)];
  named_arg_info<Char> named_args_[max_of(1, NUM_NAMED_ARGS)];
  compile_parse_context<Char> context_;

  using parse_func = auto (*)(parse_context<Char>&) -> const Char*;
  parse_func parse_funcs_[max_of(1, NUM_ARGS)];

 public:
  template <typename... T>
  FMT_CONSTEXPR explicit format_string_checker(basic_string_view<Char> fmt,
                                               arg_pack<T...>)
      : types_{mapped_type_constant<T, Char>::value...},
        named_args_{},
        context_(fmt, NUM_ARGS, types_),
        parse_funcs_{&invoke_parse<T, Char>...} {
    int arg_index = 0, named_arg_index = 0;
    FMT_APPLY_VARIADIC(
        init_static_named_arg<T>(named_args_, arg_index, named_arg_index));
    ignore_unused(arg_index, named_arg_index);
  }

  FMT_CONSTEXPR void on_text(const Char*, const Char*) {}

  FMT_CONSTEXPR auto on_arg_id() -> int { return context_.next_arg_id(); }
  FMT_CONSTEXPR auto on_arg_id(int id) -> int {
    context_.check_arg_id(id);
    return id;
  }
  FMT_CONSTEXPR auto on_arg_id(basic_string_view<Char> id) -> int {
    for (int i = 0; i < NUM_NAMED_ARGS; ++i) {
      if (named_args_[i].name == id) return named_args_[i].id;
    }
    if (!DYNAMIC_NAMES) on_error("argument not found");
    return -1;
  }

  FMT_CONSTEXPR void on_replacement_field(int id, const Char* begin) {
    on_format_specs(id, begin, begin);  // Call parse() on empty specs.
  }

  FMT_CONSTEXPR auto on_format_specs(int id, const Char* begin, const Char* end)
      -> const Char* {
    context_.advance_to(begin);
    if (id >= 0 && id < NUM_ARGS) return parse_funcs_[id](context_);

    // If id is out of range, it means we do not know the type and cannot parse
    // the format at compile time. Instead, skip over content until we finish
    // the format spec, accounting for any nested replacements.
    for (int bracket_count = 0;
         begin != end && (bracket_count > 0 || *begin != '}'); ++begin) {
      if (*begin == '{')
        ++bracket_count;
      else if (*begin == '}')
        --bracket_count;
    }
    return begin;
  }

  FMT_NORETURN FMT_CONSTEXPR void on_error(const char* message) {
    report_error(message);
  }
};

/// A contiguous memory buffer with an optional growing ability. It is an
/// internal class and shouldn't be used directly, only via `memory_buffer`.
template <typename T> class buffer {
 private:
  T* ptr_;
  size_t size_;
  size_t capacity_;

  using grow_fun = void (*)(buffer& buf, size_t capacity);
  grow_fun grow_;

 protected:
  // Don't initialize ptr_ since it is not accessed to save a few cycles.
  FMT_MSC_WARNING(suppress : 26495)
  FMT_CONSTEXPR buffer(grow_fun grow, size_t sz) noexcept
      : size_(sz), capacity_(sz), grow_(grow) {}

  constexpr buffer(grow_fun grow, T* p = nullptr, size_t sz = 0,
                   size_t cap = 0) noexcept
      : ptr_(p), size_(sz), capacity_(cap), grow_(grow) {}

  FMT_CONSTEXPR20 ~buffer() = default;
  buffer(buffer&&) = default;

  /// Sets the buffer data and capacity.
  FMT_CONSTEXPR void set(T* buf_data, size_t buf_capacity) noexcept {
    ptr_ = buf_data;
    capacity_ = buf_capacity;
  }

 public:
  using value_type = T;
  using const_reference = const T&;

  buffer(const buffer&) = delete;
  void operator=(const buffer&) = delete;

  auto begin() noexcept -> T* { return ptr_; }
  auto end() noexcept -> T* { return ptr_ + size_; }

  auto begin() const noexcept -> const T* { return ptr_; }
  auto end() const noexcept -> const T* { return ptr_ + size_; }

  /// Returns the size of this buffer.
  constexpr auto size() const noexcept -> size_t { return size_; }

  /// Returns the capacity of this buffer.
  constexpr auto capacity() const noexcept -> size_t { return capacity_; }

  /// Returns a pointer to the buffer data (not null-terminated).
  FMT_CONSTEXPR auto data() noexcept -> T* { return ptr_; }
  FMT_CONSTEXPR auto data() const noexcept -> const T* { return ptr_; }

  /// Clears this buffer.
  FMT_CONSTEXPR void clear() { size_ = 0; }

  // Tries resizing the buffer to contain `count` elements. If T is a POD type
  // the new elements may not be initialized.
  FMT_CONSTEXPR void try_resize(size_t count) {
    try_reserve(count);
    size_ = min_of(count, capacity_);
  }

  // Tries increasing the buffer capacity to `new_capacity`. It can increase the
  // capacity by a smaller amount than requested but guarantees there is space
  // for at least one additional element either by increasing the capacity or by
  // flushing the buffer if it is full.
  FMT_CONSTEXPR void try_reserve(size_t new_capacity) {
    if (new_capacity > capacity_) grow_(*this, new_capacity);
  }

  FMT_CONSTEXPR void push_back(const T& value) {
    try_reserve(size_ + 1);
    ptr_[size_++] = value;
  }

  /// Appends data to the end of the buffer.
  template <typename U>
// Workaround for MSVC2019 to fix error C2893: Failed to specialize function
// template 'void fmt::v11::detail::buffer<T>::append(const U *,const U *)'.
#if !FMT_MSC_VERSION || FMT_MSC_VERSION >= 1940
  FMT_CONSTEXPR20
#endif
      void
      append(const U* begin, const U* end) {
    while (begin != end) {
      auto count = to_unsigned(end - begin);
      try_reserve(size_ + count);
      auto free_cap = capacity_ - size_;
      if (free_cap < count) count = free_cap;
      // A loop is faster than memcpy on small sizes.
      T* out = ptr_ + size_;
      for (size_t i = 0; i < count; ++i) out[i] = begin[i];
      size_ += count;
      begin += count;
    }
  }

  template <typename Idx> FMT_CONSTEXPR auto operator[](Idx index) -> T& {
    return ptr_[index];
  }
  template <typename Idx>
  FMT_CONSTEXPR auto operator[](Idx index) const -> const T& {
    return ptr_[index];
  }
};

struct buffer_traits {
  constexpr explicit buffer_traits(size_t) {}
  constexpr auto count() const -> size_t { return 0; }
  constexpr auto limit(size_t size) const -> size_t { return size; }
};

class fixed_buffer_traits {
 private:
  size_t count_ = 0;
  size_t limit_;

 public:
  constexpr explicit fixed_buffer_traits(size_t limit) : limit_(limit) {}
  constexpr auto count() const -> size_t { return count_; }
  FMT_CONSTEXPR auto limit(size_t size) -> size_t {
    size_t n = limit_ > count_ ? limit_ - count_ : 0;
    count_ += size;
    return min_of(size, n);
  }
};

// A buffer that writes to an output iterator when flushed.
template <typename OutputIt, typename T, typename Traits = buffer_traits>
class iterator_buffer : public Traits, public buffer<T> {
 private:
  OutputIt out_;
  enum { buffer_size = 256 };
  T data_[buffer_size];

  static FMT_CONSTEXPR void grow(buffer<T>& buf, size_t) {
    if (buf.size() == buffer_size) static_cast<iterator_buffer&>(buf).flush();
  }

  void flush() {
    auto size = this->size();
    this->clear();
    const T* begin = data_;
    const T* end = begin + this->limit(size);
    while (begin != end) *out_++ = *begin++;
  }

 public:
  explicit iterator_buffer(OutputIt out, size_t n = buffer_size)
      : Traits(n), buffer<T>(grow, data_, 0, buffer_size), out_(out) {}
  iterator_buffer(iterator_buffer&& other) noexcept
      : Traits(other),
        buffer<T>(grow, data_, 0, buffer_size),
        out_(other.out_) {}
  ~iterator_buffer() {
    // Don't crash if flush fails during unwinding.
    FMT_TRY { flush(); }
    FMT_CATCH(...) {}
  }

  auto out() -> OutputIt {
    flush();
    return out_;
  }
  auto count() const -> size_t { return Traits::count() + this->size(); }
};

template <typename T>
class iterator_buffer<T*, T, fixed_buffer_traits> : public fixed_buffer_traits,
                                                    public buffer<T> {
 private:
  T* out_;
  enum { buffer_size = 256 };
  T data_[buffer_size];

  static FMT_CONSTEXPR void grow(buffer<T>& buf, size_t) {
    if (buf.size() == buf.capacity())
      static_cast<iterator_buffer&>(buf).flush();
  }

  void flush() {
    size_t n = this->limit(this->size());
    if (this->data() == out_) {
      out_ += n;
      this->set(data_, buffer_size);
    }
    this->clear();
  }

 public:
  explicit iterator_buffer(T* out, size_t n = buffer_size)
      : fixed_buffer_traits(n), buffer<T>(grow, out, 0, n), out_(out) {}
  iterator_buffer(iterator_buffer&& other) noexcept
      : fixed_buffer_traits(other),
        buffer<T>(static_cast<iterator_buffer&&>(other)),
        out_(other.out_) {
    if (this->data() != out_) {
      this->set(data_, buffer_size);
      this->clear();
    }
  }
  ~iterator_buffer() { flush(); }

  auto out() -> T* {
    flush();
    return out_;
  }
  auto count() const -> size_t {
    return fixed_buffer_traits::count() + this->size();
  }
};

template <typename T> class iterator_buffer<T*, T> : public buffer<T> {
 public:
  explicit iterator_buffer(T* out, size_t = 0)
      : buffer<T>([](buffer<T>&, size_t) {}, out, 0, ~size_t()) {}

  auto out() -> T* { return &*this->end(); }
};

template <typename Container>
class container_buffer : public buffer<typename Container::value_type> {
 private:
  using value_type = typename Container::value_type;

  static FMT_CONSTEXPR void grow(buffer<value_type>& buf, size_t capacity) {
    auto& self = static_cast<container_buffer&>(buf);
    self.container.resize(capacity);
    self.set(&self.container[0], capacity);
  }

 public:
  Container& container;

  explicit container_buffer(Container& c)
      : buffer<value_type>(grow, c.size()), container(c) {}
};

// A buffer that writes to a container with the contiguous storage.
template <typename OutputIt>
class iterator_buffer<
    OutputIt,
    enable_if_t<is_back_insert_iterator<OutputIt>::value &&
                    is_contiguous<typename OutputIt::container_type>::value,
                typename OutputIt::container_type::value_type>>
    : public container_buffer<typename OutputIt::container_type> {
 private:
  using base = container_buffer<typename OutputIt::container_type>;

 public:
  explicit iterator_buffer(typename OutputIt::container_type& c) : base(c) {}
  explicit iterator_buffer(OutputIt out, size_t = 0)
      : base(get_container(out)) {}

  auto out() -> OutputIt { return OutputIt(this->container); }
};

// A buffer that counts the number of code units written discarding the output.
template <typename T = char> class counting_buffer : public buffer<T> {
 private:
  enum { buffer_size = 256 };
  T data_[buffer_size];
  size_t count_ = 0;

  static FMT_CONSTEXPR void grow(buffer<T>& buf, size_t) {
    if (buf.size() != buffer_size) return;
    static_cast<counting_buffer&>(buf).count_ += buf.size();
    buf.clear();
  }

 public:
  FMT_CONSTEXPR counting_buffer() : buffer<T>(grow, data_, 0, buffer_size) {}

  constexpr auto count() const noexcept -> size_t {
    return count_ + this->size();
  }
};

template <typename T>
struct is_back_insert_iterator<basic_appender<T>> : std::true_type {};

template <typename OutputIt, typename InputIt, typename = void>
struct has_back_insert_iterator_container_append : std::false_type {};
template <typename OutputIt, typename InputIt>
struct has_back_insert_iterator_container_append<
    OutputIt, InputIt,
    void_t<decltype(get_container(std::declval<OutputIt>())
                        .append(std::declval<InputIt>(),
                                std::declval<InputIt>()))>> : std::true_type {};

// An optimized version of std::copy with the output value type (T).
template <typename T, typename InputIt, typename OutputIt,
          FMT_ENABLE_IF(is_back_insert_iterator<OutputIt>::value&&
                            has_back_insert_iterator_container_append<
                                OutputIt, InputIt>::value)>
FMT_CONSTEXPR20 auto copy(InputIt begin, InputIt end, OutputIt out)
    -> OutputIt {
  get_container(out).append(begin, end);
  return out;
}

template <typename T, typename InputIt, typename OutputIt,
          FMT_ENABLE_IF(is_back_insert_iterator<OutputIt>::value &&
                        !has_back_insert_iterator_container_append<
                            OutputIt, InputIt>::value)>
FMT_CONSTEXPR20 auto copy(InputIt begin, InputIt end, OutputIt out)
    -> OutputIt {
  auto& c = get_container(out);
  c.insert(c.end(), begin, end);
  return out;
}

template <typename T, typename InputIt, typename OutputIt,
          FMT_ENABLE_IF(!is_back_insert_iterator<OutputIt>::value)>
FMT_CONSTEXPR auto copy(InputIt begin, InputIt end, OutputIt out) -> OutputIt {
  while (begin != end) *out++ = static_cast<T>(*begin++);
  return out;
}

template <typename T, typename V, typename OutputIt>
FMT_CONSTEXPR auto copy(basic_string_view<V> s, OutputIt out) -> OutputIt {
  return copy<T>(s.begin(), s.end(), out);
}

template <typename It, typename Enable = std::true_type>
struct is_buffer_appender : std::false_type {};
template <typename It>
struct is_buffer_appender<
    It, bool_constant<
            is_back_insert_iterator<It>::value &&
            std::is_base_of<buffer<typename It::container_type::value_type>,
                            typename It::container_type>::value>>
    : std::true_type {};

// Maps an output iterator to a buffer.
template <typename T, typename OutputIt,
          FMT_ENABLE_IF(!is_buffer_appender<OutputIt>::value)>
auto get_buffer(OutputIt out) -> iterator_buffer<OutputIt, T> {
  return iterator_buffer<OutputIt, T>(out);
}
template <typename T, typename OutputIt,
          FMT_ENABLE_IF(is_buffer_appender<OutputIt>::value)>
auto get_buffer(OutputIt out) -> buffer<T>& {
  return get_container(out);
}

template <typename Buf, typename OutputIt>
auto get_iterator(Buf& buf, OutputIt) -> decltype(buf.out()) {
  return buf.out();
}
template <typename T, typename OutputIt>
auto get_iterator(buffer<T>&, OutputIt out) -> OutputIt {
  return out;
}

// This type is intentionally undefined, only used for errors.
template <typename T, typename Char> struct type_is_unformattable_for;

template <typename Char> struct string_value {
  const Char* data;
  size_t size;
  auto str() const -> basic_string_view<Char> { return {data, size}; }
};

template <typename Context> struct custom_value {
  using char_type = typename Context::char_type;
  void* value;
  void (*format)(void* arg, parse_context<char_type>& parse_ctx, Context& ctx);
};

template <typename Char> struct named_arg_value {
  const named_arg_info<Char>* data;
  size_t size;
};

struct custom_tag {};

#if !FMT_BUILTIN_TYPES
#  define FMT_BUILTIN , monostate
#else
#  define FMT_BUILTIN
#endif

// A formatting argument value.
template <typename Context> class value {
 public:
  using char_type = typename Context::char_type;

  union {
    monostate no_value;
    int int_value;
    unsigned uint_value;
    long long long_long_value;
    unsigned long long ulong_long_value;
    int128_opt int128_value;
    uint128_opt uint128_value;
    bool bool_value;
    char_type char_value;
    float float_value;
    double double_value;
    long double long_double_value;
    const void* pointer;
    string_value<char_type> string;
    custom_value<Context> custom;
    named_arg_value<char_type> named_args;
  };

  constexpr FMT_INLINE value() : no_value() {}
  constexpr FMT_INLINE value(signed char x) : int_value(x) {}
  constexpr FMT_INLINE value(unsigned char x FMT_BUILTIN) : uint_value(x) {}
  constexpr FMT_INLINE value(signed short x) : int_value(x) {}
  constexpr FMT_INLINE value(unsigned short x FMT_BUILTIN) : uint_value(x) {}
  constexpr FMT_INLINE value(int x) : int_value(x) {}
  constexpr FMT_INLINE value(unsigned x FMT_BUILTIN) : uint_value(x) {}
  FMT_CONSTEXPR FMT_INLINE value(long x FMT_BUILTIN) : value(long_type(x)) {}
  FMT_CONSTEXPR FMT_INLINE value(unsigned long x FMT_BUILTIN)
      : value(ulong_type(x)) {}
  constexpr FMT_INLINE value(long long x FMT_BUILTIN) : long_long_value(x) {}
  constexpr FMT_INLINE value(unsigned long long x FMT_BUILTIN)
      : ulong_long_value(x) {}
  FMT_INLINE value(int128_opt x FMT_BUILTIN) : int128_value(x) {}
  FMT_INLINE value(uint128_opt x FMT_BUILTIN) : uint128_value(x) {}
  constexpr FMT_INLINE value(bool x FMT_BUILTIN) : bool_value(x) {}

  template <int N>
  constexpr FMT_INLINE value(bitint<N> x FMT_BUILTIN) : long_long_value(x) {
    static_assert(N <= 64, "unsupported _BitInt");
  }
  template <int N>
  constexpr FMT_INLINE value(ubitint<N> x FMT_BUILTIN) : ulong_long_value(x) {
    static_assert(N <= 64, "unsupported _BitInt");
  }

  template <typename T, FMT_ENABLE_IF(is_char<T>::value)>
  constexpr FMT_INLINE value(T x FMT_BUILTIN) : char_value(x) {
    static_assert(
        std::is_same<T, char>::value || std::is_same<T, char_type>::value,
        "mixing character types is disallowed");
  }

  constexpr FMT_INLINE value(float x FMT_BUILTIN) : float_value(x) {}
  constexpr FMT_INLINE value(double x FMT_BUILTIN) : double_value(x) {}
  FMT_INLINE value(long double x FMT_BUILTIN) : long_double_value(x) {}

  FMT_CONSTEXPR FMT_INLINE value(char_type* x FMT_BUILTIN) {
    string.data = x;
    if (is_constant_evaluated()) string.size = 0;
  }
  FMT_CONSTEXPR FMT_INLINE value(const char_type* x FMT_BUILTIN) {
    string.data = x;
    if (is_constant_evaluated()) string.size = 0;
  }
  template <typename T, typename C = char_t<T>,
            FMT_ENABLE_IF(!std::is_pointer<T>::value)>
  FMT_CONSTEXPR value(const T& x FMT_BUILTIN) {
    static_assert(std::is_same<C, char_type>::value,
                  "mixing character types is disallowed");
    auto sv = to_string_view(x);
    string.data = sv.data();
    string.size = sv.size();
  }
  FMT_INLINE value(void* x FMT_BUILTIN) : pointer(x) {}
  FMT_INLINE value(const void* x FMT_BUILTIN) : pointer(x) {}
  FMT_INLINE value(volatile void* x FMT_BUILTIN)
      : pointer(const_cast<const void*>(x)) {}
  FMT_INLINE value(const volatile void* x FMT_BUILTIN)
      : pointer(const_cast<const void*>(x)) {}
  FMT_INLINE value(nullptr_t) : pointer(nullptr) {}

  template <typename T, FMT_ENABLE_IF(std::is_pointer<T>::value ||
                                      std::is_member_pointer<T>::value)>
  value(const T&) {
    // Formatting of arbitrary pointers is disallowed. If you want to format a
    // pointer cast it to `void*` or `const void*`. In particular, this forbids
    // formatting of `[const] volatile char*` printed as bool by iostreams.
    static_assert(sizeof(T) == 0,
                  "formatting of non-void pointers is disallowed");
  }

  template <typename T, FMT_ENABLE_IF(use_format_as<T>::value)>
  value(const T& x) : value(format_as(x)) {}
  template <typename T, FMT_ENABLE_IF(use_format_as_member<T>::value)>
  value(const T& x) : value(formatter<T>::format_as(x)) {}

  template <typename T, FMT_ENABLE_IF(is_named_arg<T>::value)>
  value(const T& named_arg) : value(named_arg.value) {}

  template <typename T,
            FMT_ENABLE_IF(use_formatter<T>::value || !FMT_BUILTIN_TYPES)>
  FMT_CONSTEXPR20 FMT_INLINE value(T& x) : value(x, custom_tag()) {}

  FMT_ALWAYS_INLINE value(const named_arg_info<char_type>* args, size_t size)
      : named_args{args, size} {}

 private:
  template <typename T, FMT_ENABLE_IF(has_formatter<T, char_type>())>
  FMT_CONSTEXPR value(T& x, custom_tag) {
    using value_type = remove_const_t<T>;
    // T may overload operator& e.g. std::vector<bool>::reference in libc++.
    if (!is_constant_evaluated()) {
      custom.value =
          const_cast<char*>(&reinterpret_cast<const volatile char&>(x));
    } else {
      custom.value = nullptr;
#if defined(__cpp_if_constexpr)
      if constexpr (std::is_same<decltype(&x), remove_reference_t<T>*>::value)
        custom.value = const_cast<value_type*>(&x);
#endif
    }
    custom.format = format_custom<value_type, formatter<value_type, char_type>>;
  }

  template <typename T, FMT_ENABLE_IF(!has_formatter<T, char_type>())>
  FMT_CONSTEXPR value(const T&, custom_tag) {
    // Cannot format an argument; to make type T formattable provide a
    // formatter<T> specialization: https://fmt.dev/latest/api.html#udt.
    type_is_unformattable_for<T, char_type> _;
  }

  // Formats an argument of a custom type, such as a user-defined class.
  template <typename T, typename Formatter>
  static void format_custom(void* arg, parse_context<char_type>& parse_ctx,
                            Context& ctx) {
    auto f = Formatter();
    parse_ctx.advance_to(f.parse(parse_ctx));
    using qualified_type =
        conditional_t<has_formatter<const T, char_type>(), const T, T>;
    // format must be const for compatibility with std::format and compilation.
    const auto& cf = f;
    ctx.advance_to(cf.format(*static_cast<qualified_type*>(arg), ctx));
  }
};

enum { packed_arg_bits = 4 };
// Maximum number of arguments with packed types.
enum { max_packed_args = 62 / packed_arg_bits };
enum : unsigned long long { is_unpacked_bit = 1ULL << 63 };
enum : unsigned long long { has_named_args_bit = 1ULL << 62 };

template <typename It, typename T, typename Enable = void>
struct is_output_iterator : std::false_type {};

template <> struct is_output_iterator<appender, char> : std::true_type {};

template <typename It, typename T>
struct is_output_iterator<
    It, T,
    enable_if_t<std::is_assignable<decltype(*std::declval<decay_t<It>&>()++),
                                   T>::value>> : std::true_type {};

#ifndef FMT_USE_LOCALE
#  define FMT_USE_LOCALE (FMT_OPTIMIZE_SIZE <= 1)
#endif

// A type-erased reference to an std::locale to avoid a heavy <locale> include.
class locale_ref {
#if FMT_USE_LOCALE
 private:
  const void* locale_;  // A type-erased pointer to std::locale.

 public:
  constexpr locale_ref() : locale_(nullptr) {}
  template <typename Locale> locale_ref(const Locale& loc);

  inline explicit operator bool() const noexcept { return locale_ != nullptr; }
#endif  // FMT_USE_LOCALE

 public:
  template <typename Locale> auto get() const -> Locale;
};

template <typename> constexpr auto encode_types() -> unsigned long long {
  return 0;
}

template <typename Context, typename Arg, typename... Args>
constexpr auto encode_types() -> unsigned long long {
  return static_cast<unsigned>(stored_type_constant<Arg, Context>::value) |
         (encode_types<Context, Args...>() << packed_arg_bits);
}

template <typename Context, typename... T, size_t NUM_ARGS = sizeof...(T)>
constexpr auto make_descriptor() -> unsigned long long {
  return NUM_ARGS <= max_packed_args ? encode_types<Context, T...>()
                                     : is_unpacked_bit | NUM_ARGS;
}

template <typename Context, int NUM_ARGS>
using arg_t = conditional_t<NUM_ARGS <= max_packed_args, value<Context>,
                            basic_format_arg<Context>>;

template <typename Context, int NUM_ARGS, int NUM_NAMED_ARGS,
          unsigned long long DESC>
struct named_arg_store {
  // args_[0].named_args points to named_args to avoid bloating format_args.
  arg_t<Context, NUM_ARGS> args[1 + NUM_ARGS];
  named_arg_info<typename Context::char_type> named_args[NUM_NAMED_ARGS];

  template <typename... T>
  FMT_CONSTEXPR FMT_ALWAYS_INLINE named_arg_store(T&... values)
      : args{{named_args, NUM_NAMED_ARGS}, values...} {
    int arg_index = 0, named_arg_index = 0;
    FMT_APPLY_VARIADIC(
        init_named_arg(named_args, arg_index, named_arg_index, values));
  }

  named_arg_store(named_arg_store&& rhs) {
    args[0] = {named_args, NUM_NAMED_ARGS};
    for (size_t i = 1; i < sizeof(args) / sizeof(*args); ++i)
      args[i] = rhs.args[i];
    for (size_t i = 0; i < NUM_NAMED_ARGS; ++i)
      named_args[i] = rhs.named_args[i];
  }

  named_arg_store(const named_arg_store& rhs) = delete;
  named_arg_store& operator=(const named_arg_store& rhs) = delete;
  named_arg_store& operator=(named_arg_store&& rhs) = delete;
  operator const arg_t<Context, NUM_ARGS>*() const { return args + 1; }
};

// An array of references to arguments. It can be implicitly converted to
// `basic_format_args` for passing into type-erased formatting functions
// such as `vformat`. It is a plain struct to reduce binary size in debug mode.
template <typename Context, int NUM_ARGS, int NUM_NAMED_ARGS,
          unsigned long long DESC>
struct format_arg_store {
  // +1 to workaround a bug in gcc 7.5 that causes duplicated-branches warning.
  using type =
      conditional_t<NUM_NAMED_ARGS == 0,
                    arg_t<Context, NUM_ARGS>[max_of(1, NUM_ARGS)],
                    named_arg_store<Context, NUM_ARGS, NUM_NAMED_ARGS, DESC>>;
  type args;
};

// TYPE can be different from type_constant<T>, e.g. for __float128.
template <typename T, typename Char, type TYPE> struct native_formatter {
 private:
  dynamic_format_specs<Char> specs_;

 public:
  using nonlocking = void;

  FMT_CONSTEXPR auto parse(parse_context<Char>& ctx) -> const Char* {
    if (ctx.begin() == ctx.end() || *ctx.begin() == '}') return ctx.begin();
    auto end = parse_format_specs(ctx.begin(), ctx.end(), specs_, ctx, TYPE);
    if (const_check(TYPE == type::char_type)) check_char_specs(specs_);
    return end;
  }

  template <type U = TYPE,
            FMT_ENABLE_IF(U == type::string_type || U == type::cstring_type ||
                          U == type::char_type)>
  FMT_CONSTEXPR void set_debug_format(bool set = true) {
    specs_.set_type(set ? presentation_type::debug : presentation_type::none);
  }

  FMT_PRAGMA_CLANG(diagnostic ignored "-Wundefined-inline")
  template <typename FormatContext>
  FMT_CONSTEXPR auto format(const T& val, FormatContext& ctx) const
      -> decltype(ctx.out());
};

template <typename T, typename Enable = void>
struct locking
    : bool_constant<mapped_type_constant<T>::value == type::custom_type> {};
template <typename T>
struct locking<T, void_t<typename formatter<remove_cvref_t<T>>::nonlocking>>
    : std::false_type {};

template <typename T = int> FMT_CONSTEXPR inline auto is_locking() -> bool {
  return locking<T>::value;
}
template <typename T1, typename T2, typename... Tail>
FMT_CONSTEXPR inline auto is_locking() -> bool {
  return locking<T1>::value || is_locking<T2, Tail...>();
}

FMT_API void vformat_to(buffer<char>& buf, string_view fmt, format_args args,
                        locale_ref loc = {});

#if FMT_WIN32
FMT_API void vprint_mojibake(FILE*, string_view, format_args, bool);
#else  // format_args is passed by reference since it is defined later.
inline void vprint_mojibake(FILE*, string_view, const format_args&, bool) {}
#endif
}  // namespace detail

// The main public API.

template <typename Char>
FMT_CONSTEXPR void parse_context<Char>::do_check_arg_id(int arg_id) {
  // Argument id is only checked at compile time during parsing because
  // formatting has its own validation.
  if (detail::is_constant_evaluated() && use_constexpr_cast) {
    auto ctx = static_cast<detail::compile_parse_context<Char>*>(this);
    if (arg_id >= ctx->num_args()) report_error("argument not found");
  }
}

template <typename Char>
FMT_CONSTEXPR void parse_context<Char>::check_dynamic_spec(int arg_id) {
  using detail::compile_parse_context;
  if (detail::is_constant_evaluated() && use_constexpr_cast)
    static_cast<compile_parse_context<Char>*>(this)->check_dynamic_spec(arg_id);
}

FMT_BEGIN_EXPORT

// An output iterator that appends to a buffer. It is used instead of
// back_insert_iterator to reduce symbol sizes and avoid <iterator> dependency.
template <typename T> class basic_appender {
 protected:
  detail::buffer<T>* container;

 public:
  using container_type = detail::buffer<T>;

  FMT_CONSTEXPR basic_appender(detail::buffer<T>& buf) : container(&buf) {}

  FMT_CONSTEXPR20 auto operator=(T c) -> basic_appender& {
    container->push_back(c);
    return *this;
  }
  FMT_CONSTEXPR20 auto operator*() -> basic_appender& { return *this; }
  FMT_CONSTEXPR20 auto operator++() -> basic_appender& { return *this; }
  FMT_CONSTEXPR20 auto operator++(int) -> basic_appender { return *this; }
};

// A formatting argument. Context is a template parameter for the compiled API
// where output can be unbuffered.
template <typename Context> class basic_format_arg {
 private:
  detail::value<Context> value_;
  detail::type type_;

  friend class basic_format_args<Context>;

  using char_type = typename Context::char_type;

 public:
  class handle {
   private:
    detail::custom_value<Context> custom_;

   public:
    explicit handle(detail::custom_value<Context> custom) : custom_(custom) {}

    void format(parse_context<char_type>& parse_ctx, Context& ctx) const {
      custom_.format(custom_.value, parse_ctx, ctx);
    }
  };

  constexpr basic_format_arg() : type_(detail::type::none_type) {}
  basic_format_arg(const detail::named_arg_info<char_type>* args, size_t size)
      : value_(args, size) {}
  template <typename T>
  basic_format_arg(T&& val)
      : value_(val), type_(detail::stored_type_constant<T, Context>::value) {}

  constexpr explicit operator bool() const noexcept {
    return type_ != detail::type::none_type;
  }
  auto type() const -> detail::type { return type_; }

  /**
   * Visits an argument dispatching to the appropriate visit method based on
   * the argument type. For example, if the argument type is `double` then
   * `vis(value)` will be called with the value of type `double`.
   */
  template <typename Visitor>
  FMT_CONSTEXPR FMT_INLINE auto visit(Visitor&& vis) const -> decltype(vis(0)) {
    using detail::map;
    switch (type_) {
    case detail::type::none_type:        break;
    case detail::type::int_type:         return vis(value_.int_value);
    case detail::type::uint_type:        return vis(value_.uint_value);
    case detail::type::long_long_type:   return vis(value_.long_long_value);
    case detail::type::ulong_long_type:  return vis(value_.ulong_long_value);
    case detail::type::int128_type:      return vis(map(value_.int128_value));
    case detail::type::uint128_type:     return vis(map(value_.uint128_value));
    case detail::type::bool_type:        return vis(value_.bool_value);
    case detail::type::char_type:        return vis(value_.char_value);
    case detail::type::float_type:       return vis(value_.float_value);
    case detail::type::double_type:      return vis(value_.double_value);
    case detail::type::long_double_type: return vis(value_.long_double_value);
    case detail::type::cstring_type:     return vis(value_.string.data);
    case detail::type::string_type:      return vis(value_.string.str());
    case detail::type::pointer_type:     return vis(value_.pointer);
    case detail::type::custom_type:      return vis(handle(value_.custom));
    }
    return vis(monostate());
  }

  auto format_custom(const char_type* parse_begin,
                     parse_context<char_type>& parse_ctx, Context& ctx)
      -> bool {
    if (type_ != detail::type::custom_type) return false;
    parse_ctx.advance_to(parse_begin);
    value_.custom.format(value_.custom.value, parse_ctx, ctx);
    return true;
  }
};

/**
 * A view of a collection of formatting arguments. To avoid lifetime issues it
 * should only be used as a parameter type in type-erased functions such as
 * `vformat`:
 *
 *     void vlog(fmt::string_view fmt, fmt::format_args args);  // OK
 *     fmt::format_args args = fmt::make_format_args();  // Dangling reference
 */
template <typename Context> class basic_format_args {
 private:
  // A descriptor that contains information about formatting arguments.
  // If the number of arguments is less or equal to max_packed_args then
  // argument types are passed in the descriptor. This reduces binary code size
  // per formatting function call.
  unsigned long long desc_;
  union {
    // If is_packed() returns true then argument values are stored in values_;
    // otherwise they are stored in args_. This is done to improve cache
    // locality and reduce compiled code size since storing larger objects
    // may require more code (at least on x86-64) even if the same amount of
    // data is actually copied to stack. It saves ~10% on the bloat test.
    const detail::value<Context>* values_;
    const basic_format_arg<Context>* args_;
  };

  constexpr auto is_packed() const -> bool {
    return (desc_ & detail::is_unpacked_bit) == 0;
  }
  constexpr auto has_named_args() const -> bool {
    return (desc_ & detail::has_named_args_bit) != 0;
  }

  FMT_CONSTEXPR auto type(int index) const -> detail::type {
    int shift = index * detail::packed_arg_bits;
    unsigned mask = (1 << detail::packed_arg_bits) - 1;
    return static_cast<detail::type>((desc_ >> shift) & mask);
  }

  template <int NUM_ARGS, int NUM_NAMED_ARGS, unsigned long long DESC>
  using store =
      detail::format_arg_store<Context, NUM_ARGS, NUM_NAMED_ARGS, DESC>;

 public:
  using format_arg = basic_format_arg<Context>;

  constexpr basic_format_args() : desc_(0), args_(nullptr) {}

  /// Constructs a `basic_format_args` object from `format_arg_store`.
  template <int NUM_ARGS, int NUM_NAMED_ARGS, unsigned long long DESC,
            FMT_ENABLE_IF(NUM_ARGS <= detail::max_packed_args)>
  constexpr FMT_ALWAYS_INLINE basic_format_args(
      const store<NUM_ARGS, NUM_NAMED_ARGS, DESC>& s)
      : desc_(DESC | (NUM_NAMED_ARGS != 0 ? +detail::has_named_args_bit : 0)),
        values_(s.args) {}

  template <int NUM_ARGS, int NUM_NAMED_ARGS, unsigned long long DESC,
            FMT_ENABLE_IF(NUM_ARGS > detail::max_packed_args)>
  constexpr basic_format_args(const store<NUM_ARGS, NUM_NAMED_ARGS, DESC>& s)
      : desc_(DESC | (NUM_NAMED_ARGS != 0 ? +detail::has_named_args_bit : 0)),
        args_(s.args) {}

  /// Constructs a `basic_format_args` object from a dynamic list of arguments.
  constexpr basic_format_args(const format_arg* args, int count,
                              bool has_named = false)
      : desc_(detail::is_unpacked_bit | detail::to_unsigned(count) |
              (has_named ? +detail::has_named_args_bit : 0)),
        args_(args) {}

  /// Returns the argument with the specified id.
  FMT_CONSTEXPR auto get(int id) const -> format_arg {
    auto arg = format_arg();
    if (!is_packed()) {
      if (id < max_size()) arg = args_[id];
      return arg;
    }
    if (static_cast<unsigned>(id) >= detail::max_packed_args) return arg;
    arg.type_ = type(id);
    if (arg.type_ != detail::type::none_type) arg.value_ = values_[id];
    return arg;
  }

  template <typename Char>
  auto get(basic_string_view<Char> name) const -> format_arg {
    int id = get_id(name);
    return id >= 0 ? get(id) : format_arg();
  }

  template <typename Char>
  FMT_CONSTEXPR auto get_id(basic_string_view<Char> name) const -> int {
    if (!has_named_args()) return -1;
    const auto& named_args =
        (is_packed() ? values_[-1] : args_[-1].value_).named_args;
    for (size_t i = 0; i < named_args.size; ++i) {
      if (named_args.data[i].name == name) return named_args.data[i].id;
    }
    return -1;
  }

  auto max_size() const -> int {
    unsigned long long max_packed = detail::max_packed_args;
    return static_cast<int>(is_packed() ? max_packed
                                        : desc_ & ~detail::is_unpacked_bit);
  }
};

// A formatting context.
class context {
 private:
  appender out_;
  format_args args_;
  FMT_NO_UNIQUE_ADDRESS detail::locale_ref loc_;

 public:
  /// The character type for the output.
  using char_type = char;

  using iterator = appender;
  using format_arg = basic_format_arg<context>;
  using parse_context_type FMT_DEPRECATED = parse_context<>;
  template <typename T> using formatter_type FMT_DEPRECATED = formatter<T>;
  enum { builtin_types = FMT_BUILTIN_TYPES };

  /// Constructs a `context` object. References to the arguments are stored
  /// in the object so make sure they have appropriate lifetimes.
  FMT_CONSTEXPR context(iterator out, format_args args,
                        detail::locale_ref loc = {})
      : out_(out), args_(args), loc_(loc) {}
  context(context&&) = default;
  context(const context&) = delete;
  void operator=(const context&) = delete;

  FMT_CONSTEXPR auto arg(int id) const -> format_arg { return args_.get(id); }
  inline auto arg(string_view name) const -> format_arg {
    return args_.get(name);
  }
  FMT_CONSTEXPR auto arg_id(string_view name) const -> int {
    return args_.get_id(name);
  }
  auto args() const -> const format_args& { return args_; }

  // Returns an iterator to the beginning of the output range.
  FMT_CONSTEXPR auto out() const -> iterator { return out_; }

  // Advances the begin iterator to `it`.
  FMT_CONSTEXPR void advance_to(iterator) {}

  FMT_CONSTEXPR auto locale() const -> detail::locale_ref { return loc_; }
};

template <typename Char = char> struct runtime_format_string {
  basic_string_view<Char> str;
};

/**
 * Creates a runtime format string.
 *
 * **Example**:
 *
 *     // Check format string at runtime instead of compile-time.
 *     fmt::print(fmt::runtime("{:d}"), "I am not a number");
 */
inline auto runtime(string_view s) -> runtime_format_string<> { return {{s}}; }

/// A compile-time format string. Use `format_string` in the public API to
/// prevent type deduction.
template <typename... T> struct fstring {
 private:
  static constexpr int num_static_named_args =
      detail::count_static_named_args<T...>();

  using checker = detail::format_string_checker<
      char, static_cast<int>(sizeof...(T)), num_static_named_args,
      num_static_named_args != detail::count_named_args<T...>()>;

  using arg_pack = detail::arg_pack<T...>;

 public:
  string_view str;
  using t = fstring;

  // Reports a compile-time error if S is not a valid format string for T.
  template <size_t N>
  FMT_CONSTEVAL FMT_ALWAYS_INLINE fstring(const char (&s)[N]) : str(s, N - 1) {
    using namespace detail;
    static_assert(count<(is_view<remove_cvref_t<T>>::value &&
                         std::is_reference<T>::value)...>() == 0,
                  "passing views as lvalues is disallowed");
    if (FMT_USE_CONSTEVAL) parse_format_string<char>(s, checker(s, arg_pack()));
#ifdef FMT_ENFORCE_COMPILE_STRING
    static_assert(
        FMT_USE_CONSTEVAL && sizeof(s) != 0,
        "FMT_ENFORCE_COMPILE_STRING requires format strings to use FMT_STRING");
#endif
  }
  template <typename S,
            FMT_ENABLE_IF(std::is_convertible<const S&, string_view>::value)>
  FMT_CONSTEVAL FMT_ALWAYS_INLINE fstring(const S& s) : str(s) {
    auto sv = string_view(str);
    if (FMT_USE_CONSTEVAL)
      detail::parse_format_string<char>(sv, checker(sv, arg_pack()));
#ifdef FMT_ENFORCE_COMPILE_STRING
    static_assert(
        FMT_USE_CONSTEVAL && sizeof(s) != 0,
        "FMT_ENFORCE_COMPILE_STRING requires format strings to use FMT_STRING");
#endif
  }
  template <typename S,
            FMT_ENABLE_IF(std::is_base_of<detail::compile_string, S>::value&&
                              std::is_same<typename S::char_type, char>::value)>
  FMT_ALWAYS_INLINE fstring(const S&) : str(S()) {
    FMT_CONSTEXPR auto sv = string_view(S());
    FMT_CONSTEXPR int unused =
        (parse_format_string(sv, checker(sv, arg_pack())), 0);
    detail::ignore_unused(unused);
  }
  fstring(runtime_format_string<> fmt) : str(fmt.str) {}

  // Returning by reference generates better code in debug mode.
  FMT_ALWAYS_INLINE operator const string_view&() const { return str; }
  auto get() const -> string_view { return str; }
};

template <typename... T> using format_string = typename fstring<T...>::t;

template <typename T, typename Char = char>
using is_formattable = bool_constant<!std::is_same<
    detail::mapped_t<conditional_t<std::is_void<T>::value, int*, T>, Char>,
    void>::value>;
#ifdef __cpp_concepts
template <typename T, typename Char = char>
concept formattable = is_formattable<remove_reference_t<T>, Char>::value;
#endif

template <typename T, typename Char>
using has_formatter FMT_DEPRECATED = std::is_constructible<formatter<T, Char>>;

// A formatter specialization for natively supported types.
template <typename T, typename Char>
struct formatter<T, Char,
                 enable_if_t<detail::type_constant<T, Char>::value !=
                             detail::type::custom_type>>
    : detail::native_formatter<T, Char, detail::type_constant<T, Char>::value> {
};

/**
 * Constructs an object that stores references to arguments and can be
 * implicitly converted to `format_args`. `Context` can be omitted in which case
 * it defaults to `context`. See `arg` for lifetime considerations.
 */
// Take arguments by lvalue references to avoid some lifetime issues, e.g.
//   auto args = make_format_args(std::string());
template <typename Context = context, typename... T,
          int NUM_ARGS = sizeof...(T),
          int NUM_NAMED_ARGS = detail::count_named_args<T...>(),
          unsigned long long DESC = detail::make_descriptor<Context, T...>()>
constexpr FMT_ALWAYS_INLINE auto make_format_args(T&... args)
    -> detail::format_arg_store<Context, NUM_ARGS, NUM_NAMED_ARGS, DESC> {
  // Suppress warnings for pathological types convertible to detail::value.
  FMT_PRAGMA_GCC(diagnostic ignored "-Wconversion")
  return {{args...}};
}

template <typename... T>
using vargs =
    detail::format_arg_store<context, sizeof...(T),
                             detail::count_named_args<T...>(),
                             detail::make_descriptor<context, T...>()>;

/**
 * Returns a named argument to be used in a formatting function.
 * It should only be used in a call to a formatting function.
 *
 * **Example**:
 *
 *     fmt::print("The answer is {answer}.", fmt::arg("answer", 42));
 */
template <typename Char, typename T>
inline auto arg(const Char* name, const T& arg) -> detail::named_arg<Char, T> {
  return {name, arg};
}

/// Formats a string and writes the output to `out`.
template <typename OutputIt,
          FMT_ENABLE_IF(detail::is_output_iterator<remove_cvref_t<OutputIt>,
                                                   char>::value)>
auto vformat_to(OutputIt&& out, string_view fmt, format_args args)
    -> remove_cvref_t<OutputIt> {
  auto&& buf = detail::get_buffer<char>(out);
  detail::vformat_to(buf, fmt, args, {});
  return detail::get_iterator(buf, out);
}

/**
 * Formats `args` according to specifications in `fmt`, writes the result to
 * the output iterator `out` and returns the iterator past the end of the output
 * range. `format_to` does not append a terminating null character.
 *
 * **Example**:
 *
 *     auto out = std::vector<char>();
 *     fmt::format_to(std::back_inserter(out), "{}", 42);
 */
template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<remove_cvref_t<OutputIt>,
                                                   char>::value)>
FMT_INLINE auto format_to(OutputIt&& out, format_string<T...> fmt, T&&... args)
    -> remove_cvref_t<OutputIt> {
  return vformat_to(out, fmt.str, vargs<T...>{{args...}});
}

template <typename OutputIt> struct format_to_n_result {
  /// Iterator past the end of the output range.
  OutputIt out;
  /// Total (not truncated) output size.
  size_t size;
};

template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
auto vformat_to_n(OutputIt out, size_t n, string_view fmt, format_args args)
    -> format_to_n_result<OutputIt> {
  using traits = detail::fixed_buffer_traits;
  auto buf = detail::iterator_buffer<OutputIt, char, traits>(out, n);
  detail::vformat_to(buf, fmt, args, {});
  return {buf.out(), buf.count()};
}

/**
 * Formats `args` according to specifications in `fmt`, writes up to `n`
 * characters of the result to the output iterator `out` and returns the total
 * (not truncated) output size and the iterator past the end of the output
 * range. `format_to_n` does not append a terminating null character.
 */
template <typename OutputIt, typename... T,
          FMT_ENABLE_IF(detail::is_output_iterator<OutputIt, char>::value)>
FMT_INLINE auto format_to_n(OutputIt out, size_t n, format_string<T...> fmt,
                            T&&... args) -> format_to_n_result<OutputIt> {
  return vformat_to_n(out, n, fmt.str, vargs<T...>{{args...}});
}

struct format_to_result {
  /// Pointer to just after the last successful write in the array.
  char* out;
  /// Specifies if the output was truncated.
  bool truncated;

  FMT_CONSTEXPR operator char*() const {
    // Report truncation to prevent silent data loss.
    if (truncated) report_error("output is truncated");
    return out;
  }
};

template <size_t N>
auto vformat_to(char (&out)[N], string_view fmt, format_args args)
    -> format_to_result {
  auto result = vformat_to_n(out, N, fmt, args);
  return {result.out, result.size > N};
}

template <size_t N, typename... T>
FMT_INLINE auto format_to(char (&out)[N], format_string<T...> fmt, T&&... args)
    -> format_to_result {
  auto result = vformat_to_n(out, N, fmt.str, vargs<T...>{{args...}});
  return {result.out, result.size > N};
}

/// Returns the number of chars in the output of `format(fmt, args...)`.
template <typename... T>
FMT_NODISCARD FMT_INLINE auto formatted_size(format_string<T...> fmt,
                                             T&&... args) -> size_t {
  auto buf = detail::counting_buffer<>();
  detail::vformat_to(buf, fmt.str, vargs<T...>{{args...}}, {});
  return buf.count();
}

FMT_API void vprint(string_view fmt, format_args args);
FMT_API void vprint(FILE* f, string_view fmt, format_args args);
FMT_API void vprintln(FILE* f, string_view fmt, format_args args);
FMT_API void vprint_buffered(FILE* f, string_view fmt, format_args args);

/**
 * Formats `args` according to specifications in `fmt` and writes the output
 * to `stdout`.
 *
 * **Example**:
 *
 *     fmt::print("The answer is {}.", 42);
 */
template <typename... T>
FMT_INLINE void print(format_string<T...> fmt, T&&... args) {
  vargs<T...> va = {{args...}};
  if (detail::const_check(!detail::use_utf8))
    return detail::vprint_mojibake(stdout, fmt.str, va, false);
  return detail::is_locking<T...>() ? vprint_buffered(stdout, fmt.str, va)
                                    : vprint(fmt.str, va);
}

/**
 * Formats `args` according to specifications in `fmt` and writes the
 * output to the file `f`.
 *
 * **Example**:
 *
 *     fmt::print(stderr, "Don't {}!", "panic");
 */
template <typename... T>
FMT_INLINE void print(FILE* f, format_string<T...> fmt, T&&... args) {
  vargs<T...> va = {{args...}};
  if (detail::const_check(!detail::use_utf8))
    return detail::vprint_mojibake(f, fmt.str, va, false);
  return detail::is_locking<T...>() ? vprint_buffered(f, fmt.str, va)
                                    : vprint(f, fmt.str, va);
}

/// Formats `args` according to specifications in `fmt` and writes the output
/// to the file `f` followed by a newline.
template <typename... T>
FMT_INLINE void println(FILE* f, format_string<T...> fmt, T&&... args) {
  vargs<T...> va = {{args...}};
  return detail::const_check(detail::use_utf8)
             ? vprintln(f, fmt.str, va)
             : detail::vprint_mojibake(f, fmt.str, va, true);
}

/// Formats `args` according to specifications in `fmt` and writes the output
/// to `stdout` followed by a newline.
template <typename... T>
FMT_INLINE void println(format_string<T...> fmt, T&&... args) {
  return fmt::println(stdout, fmt, static_cast<T&&>(args)...);
}

FMT_END_EXPORT
FMT_PRAGMA_CLANG(diagnostic pop)
FMT_PRAGMA_GCC(pop_options)
FMT_END_NAMESPACE

#ifdef FMT_HEADER_ONLY
#  include "format.h"
#endif
#endif  // FMT_BASE_H_
