/* auto-generated on 2025-02-26 20:29:12 -0500. Do not edit! */
/* begin file include/ada.h */
/**
 * @file ada.h
 * @brief Includes all definitions for Ada.
 */
#ifndef ADA_H
#define ADA_H

/* begin file include/ada/ada_idna.h */
/* auto-generated on 2024-12-18 09:44:34 -0500. Do not edit! */
/* begin file include/idna.h */
#ifndef ADA_IDNA_H
#define ADA_IDNA_H

/* begin file include/ada/idna/unicode_transcoding.h */
#ifndef ADA_IDNA_UNICODE_TRANSCODING_H
#define ADA_IDNA_UNICODE_TRANSCODING_H

#include <string>
#include <string_view>

namespace ada::idna {

size_t utf8_to_utf32(const char* buf, size_t len, char32_t* utf32_output);

size_t utf8_length_from_utf32(const char32_t* buf, size_t len);

size_t utf32_length_from_utf8(const char* buf, size_t len);

size_t utf32_to_utf8(const char32_t* buf, size_t len, char* utf8_output);

}  // namespace ada::idna

#endif  // ADA_IDNA_UNICODE_TRANSCODING_H
/* end file include/ada/idna/unicode_transcoding.h */
/* begin file include/ada/idna/mapping.h */
#ifndef ADA_IDNA_MAPPING_H
#define ADA_IDNA_MAPPING_H

#include <string>
#include <string_view>

namespace ada::idna {

// If the input is ascii, then the mapping is just -> lower case.
void ascii_map(char* input, size_t length);
// check whether an ascii string needs mapping
bool ascii_has_upper_case(char* input, size_t length);
// Map the characters according to IDNA, returning the empty string on error.
std::u32string map(std::u32string_view input);

}  // namespace ada::idna

#endif
/* end file include/ada/idna/mapping.h */
/* begin file include/ada/idna/normalization.h */
#ifndef ADA_IDNA_NORMALIZATION_H
#define ADA_IDNA_NORMALIZATION_H

#include <string>
#include <string_view>

namespace ada::idna {

// Normalize the characters according to IDNA (Unicode Normalization Form C).
void normalize(std::u32string& input);

}  // namespace ada::idna
#endif
/* end file include/ada/idna/normalization.h */
/* begin file include/ada/idna/punycode.h */
#ifndef ADA_IDNA_PUNYCODE_H
#define ADA_IDNA_PUNYCODE_H

#include <string>
#include <string_view>

namespace ada::idna {

bool punycode_to_utf32(std::string_view input, std::u32string& out);
bool verify_punycode(std::string_view input);
bool utf32_to_punycode(std::u32string_view input, std::string& out);

}  // namespace ada::idna

#endif  // ADA_IDNA_PUNYCODE_H
/* end file include/ada/idna/punycode.h */
/* begin file include/ada/idna/validity.h */
#ifndef ADA_IDNA_VALIDITY_H
#define ADA_IDNA_VALIDITY_H

#include <string>
#include <string_view>

namespace ada::idna {

/**
 * @see https://www.unicode.org/reports/tr46/#Validity_Criteria
 */
bool is_label_valid(std::u32string_view label);

}  // namespace ada::idna

#endif  // ADA_IDNA_VALIDITY_H
/* end file include/ada/idna/validity.h */
/* begin file include/ada/idna/to_ascii.h */
#ifndef ADA_IDNA_TO_ASCII_H
#define ADA_IDNA_TO_ASCII_H

#include <string>
#include <string_view>

namespace ada::idna {

// Converts a domain (e.g., www.google.com) possibly containing international
// characters to an ascii domain (with punycode). It will not do percent
// decoding: percent decoding should be done prior to calling this function. We
// do not remove tabs and spaces, they should have been removed prior to calling
// this function. We also do not trim control characters. We also assume that
// the input is not empty. We return "" on error.
//
//
// This function may accept or even produce invalid domains.
std::string to_ascii(std::string_view ut8_string);

// Returns true if the string contains a forbidden code point according to the
// WHATGL URL specification:
// https://url.spec.whatwg.org/#forbidden-domain-code-point
bool contains_forbidden_domain_code_point(std::string_view ascii_string);

bool constexpr is_ascii(std::u32string_view view);
bool constexpr is_ascii(std::string_view view);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_ASCII_H
/* end file include/ada/idna/to_ascii.h */
/* begin file include/ada/idna/to_unicode.h */

#ifndef ADA_IDNA_TO_UNICODE_H
#define ADA_IDNA_TO_UNICODE_H

#include <string_view>

namespace ada::idna {

std::string to_unicode(std::string_view input);

}  // namespace ada::idna

#endif  // ADA_IDNA_TO_UNICODE_H
/* end file include/ada/idna/to_unicode.h */
/* begin file include/ada/idna/identifier.h */
#ifndef ADA_IDNA_IDENTIFIER_H
#define ADA_IDNA_IDENTIFIER_H

#include <string>
#include <string_view>

namespace ada::idna {

// Access the first code point of the input string.
// Verify if it is valid name code point given a Unicode code point and a
// boolean first: If first is true return the result of checking if code point
// is contained in the IdentifierStart set of code points. Otherwise return the
// result of checking if code point is contained in the IdentifierPart set of
// code points. Returns false if the input is empty or the code point is not
// valid. There is minimal Unicode error handling: the input should be valid
// UTF-8. https://urlpattern.spec.whatwg.org/#is-a-valid-name-code-point
bool valid_name_code_point(char32_t input, bool first);

}  // namespace ada::idna

#endif
/* end file include/ada/idna/identifier.h */

#endif
/* end file include/idna.h */
/* end file include/ada/ada_idna.h */
/* begin file include/ada/character_sets.h */
/**
 * @file character_sets.h
 * @brief Declaration of the character sets used by unicode functions.
 * @author Node.js
 * @see https://github.com/nodejs/node/blob/main/src/node_url_tables.cc
 */
#ifndef ADA_CHARACTER_SETS_H
#define ADA_CHARACTER_SETS_H

/* begin file include/ada/common_defs.h */
/**
 * @file common_defs.h
 * @brief Common definitions for cross-platform compiler support.
 */
#ifndef ADA_COMMON_DEFS_H
#define ADA_COMMON_DEFS_H

// https://en.cppreference.com/w/cpp/feature_test#Library_features
// detect C++20 features
#include <version>

#ifdef _MSC_VER
#define ADA_VISUAL_STUDIO 1
/**
 * We want to differentiate carefully between
 * clang under visual studio and regular visual
 * studio.
 */
#ifdef __clang__
// clang under visual studio
#define ADA_CLANG_VISUAL_STUDIO 1
#else
// just regular visual studio (best guess)
#define ADA_REGULAR_VISUAL_STUDIO 1
#endif  // __clang__
#endif  // _MSC_VER

#if defined(__GNUC__)
// Marks a block with a name so that MCA analysis can see it.
#define ADA_BEGIN_DEBUG_BLOCK(name) __asm volatile("# LLVM-MCA-BEGIN " #name);
#define ADA_END_DEBUG_BLOCK(name) __asm volatile("# LLVM-MCA-END " #name);
#define ADA_DEBUG_BLOCK(name, block) \
  BEGIN_DEBUG_BLOCK(name);           \
  block;                             \
  END_DEBUG_BLOCK(name);
#else
#define ADA_BEGIN_DEBUG_BLOCK(name)
#define ADA_END_DEBUG_BLOCK(name)
#define ADA_DEBUG_BLOCK(name, block)
#endif

// Align to N-byte boundary
#define ADA_ROUNDUP_N(a, n) (((a) + ((n)-1)) & ~((n)-1))
#define ADA_ROUNDDOWN_N(a, n) ((a) & ~((n)-1))

#define ADA_ISALIGNED_N(ptr, n) (((uintptr_t)(ptr) & ((n)-1)) == 0)

#if defined(ADA_REGULAR_VISUAL_STUDIO)

#define ada_really_inline __forceinline
#define ada_never_inline __declspec(noinline)

#define ada_unused
#define ada_warn_unused

#define ADA_PUSH_DISABLE_WARNINGS __pragma(warning(push))
#define ADA_PUSH_DISABLE_ALL_WARNINGS __pragma(warning(push, 0))
#define ADA_DISABLE_VS_WARNING(WARNING_NUMBER) \
  __pragma(warning(disable : WARNING_NUMBER))
// Get rid of Intellisense-only warnings (Code Analysis)
// Though __has_include is C++17, it is supported in Visual Studio 2017 or
// better (_MSC_VER>=1910).
#ifdef __has_include
#if __has_include(<CppCoreCheck\Warnings.h>)
#include <CppCoreCheck\Warnings.h>
#define ADA_DISABLE_UNDESIRED_WARNINGS \
  ADA_DISABLE_VS_WARNING(ALL_CPPCORECHECK_WARNINGS)
#endif
#endif

#ifndef ADA_DISABLE_UNDESIRED_WARNINGS
#define ADA_DISABLE_UNDESIRED_WARNINGS
#endif

#define ADA_DISABLE_DEPRECATED_WARNING ADA_DISABLE_VS_WARNING(4996)
#define ADA_DISABLE_STRICT_OVERFLOW_WARNING
#define ADA_POP_DISABLE_WARNINGS __pragma(warning(pop))

#else  // ADA_REGULAR_VISUAL_STUDIO

#define ada_really_inline inline __attribute__((always_inline))
#define ada_never_inline inline __attribute__((noinline))

#define ada_unused __attribute__((unused))
#define ada_warn_unused __attribute__((warn_unused_result))

#define ADA_PUSH_DISABLE_WARNINGS _Pragma("GCC diagnostic push")
// gcc doesn't seem to disable all warnings with all and extra, add warnings
// here as necessary
#define ADA_PUSH_DISABLE_ALL_WARNINGS               \
  ADA_PUSH_DISABLE_WARNINGS                         \
  ADA_DISABLE_GCC_WARNING("-Weffc++")               \
  ADA_DISABLE_GCC_WARNING("-Wall")                  \
  ADA_DISABLE_GCC_WARNING("-Wconversion")           \
  ADA_DISABLE_GCC_WARNING("-Wextra")                \
  ADA_DISABLE_GCC_WARNING("-Wattributes")           \
  ADA_DISABLE_GCC_WARNING("-Wimplicit-fallthrough") \
  ADA_DISABLE_GCC_WARNING("-Wnon-virtual-dtor")     \
  ADA_DISABLE_GCC_WARNING("-Wreturn-type")          \
  ADA_DISABLE_GCC_WARNING("-Wshadow")               \
  ADA_DISABLE_GCC_WARNING("-Wunused-parameter")     \
  ADA_DISABLE_GCC_WARNING("-Wunused-variable")      \
  ADA_DISABLE_GCC_WARNING("-Wsign-compare")
#define ADA_PRAGMA(P) _Pragma(#P)
#define ADA_DISABLE_GCC_WARNING(WARNING) \
  ADA_PRAGMA(GCC diagnostic ignored WARNING)
#if defined(ADA_CLANG_VISUAL_STUDIO)
#define ADA_DISABLE_UNDESIRED_WARNINGS \
  ADA_DISABLE_GCC_WARNING("-Wmicrosoft-include")
#else
#define ADA_DISABLE_UNDESIRED_WARNINGS
#endif
#define ADA_DISABLE_DEPRECATED_WARNING \
  ADA_DISABLE_GCC_WARNING("-Wdeprecated-declarations")
#define ADA_DISABLE_STRICT_OVERFLOW_WARNING \
  ADA_DISABLE_GCC_WARNING("-Wstrict-overflow")
#define ADA_POP_DISABLE_WARNINGS _Pragma("GCC diagnostic pop")

#endif  // MSC_VER

#if defined(ADA_VISUAL_STUDIO)
/**
 * It does not matter here whether you are using
 * the regular visual studio or clang under visual
 * studio.
 */
#if ADA_USING_LIBRARY
#define ADA_DLLIMPORTEXPORT __declspec(dllimport)
#else
#define ADA_DLLIMPORTEXPORT __declspec(dllexport)
#endif
#else
#define ADA_DLLIMPORTEXPORT
#endif

/// If EXPR is an error, returns it.
#define ADA_TRY(EXPR)   \
  {                     \
    auto _err = (EXPR); \
    if (_err) {         \
      return _err;      \
    }                   \
  }

// __has_cpp_attribute is part of C++20
#if !defined(__has_cpp_attribute)
#define __has_cpp_attribute(x) 0
#endif

#if __has_cpp_attribute(gnu::noinline)
#define ADA_ATTRIBUTE_NOINLINE [[gnu::noinline]]
#else
#define ADA_ATTRIBUTE_NOINLINE
#endif

namespace ada {
[[noreturn]] inline void unreachable() {
#ifdef __GNUC__
  __builtin_unreachable();
#elif defined(_MSC_VER)
  __assume(false);
#else
#endif
}
}  // namespace ada

// Unless the programmer has already set ADA_DEVELOPMENT_CHECKS,
// we want to set it under debug builds. We detect a debug build
// under Visual Studio when the _DEBUG macro is set. Under the other
// compilers, we use the fact that they define __OPTIMIZE__ whenever
// they allow optimizations.
// It is possible that this could miss some cases where ADA_DEVELOPMENT_CHECKS
// is helpful, but the programmer can set the macro ADA_DEVELOPMENT_CHECKS.
// It could also wrongly set ADA_DEVELOPMENT_CHECKS (e.g., if the programmer
// sets _DEBUG in a release build under Visual Studio, or if some compiler fails
// to set the __OPTIMIZE__ macro).
#if !defined(ADA_DEVELOPMENT_CHECKS) && !defined(NDEBUG)
#ifdef _MSC_VER
// Visual Studio seems to set _DEBUG for debug builds.
#ifdef _DEBUG
#define ADA_DEVELOPMENT_CHECKS 1
#endif  // _DEBUG
#else   // _MSC_VER
// All other compilers appear to set __OPTIMIZE__ to a positive integer
// when the compiler is optimizing.
#ifndef __OPTIMIZE__
#define ADA_DEVELOPMENT_CHECKS 1
#endif  // __OPTIMIZE__
#endif  // _MSC_VER
#endif  // ADA_DEVELOPMENT_CHECKS

#define ADA_STR(x) #x

#if ADA_DEVELOPMENT_CHECKS
#define ADA_REQUIRE(EXPR) \
  {                       \
    if (!(EXPR) { abort(); }) }

#define ADA_FAIL(MESSAGE)                            \
  do {                                               \
    std::cerr << "FAIL: " << (MESSAGE) << std::endl; \
    abort();                                         \
  } while (0);
#define ADA_ASSERT_EQUAL(LHS, RHS, MESSAGE)                                    \
  do {                                                                         \
    if (LHS != RHS) {                                                          \
      std::cerr << "Mismatch: '" << LHS << "' - '" << RHS << "'" << std::endl; \
      ADA_FAIL(MESSAGE);                                                       \
    }                                                                          \
  } while (0);
#define ADA_ASSERT_TRUE(COND)                                               \
  do {                                                                      \
    if (!(COND)) {                                                          \
      std::cerr << "Assert at line " << __LINE__ << " of file " << __FILE__ \
                << std::endl;                                               \
      ADA_FAIL(ADA_STR(COND));                                              \
    }                                                                       \
  } while (0);
#else
#define ADA_FAIL(MESSAGE)
#define ADA_ASSERT_EQUAL(LHS, RHS, MESSAGE)
#define ADA_ASSERT_TRUE(COND)
#endif

#ifdef ADA_VISUAL_STUDIO
#define ADA_ASSUME(COND) __assume(COND)
#else
#define ADA_ASSUME(COND)       \
  do {                         \
    if (!(COND)) {             \
      __builtin_unreachable(); \
    }                          \
  } while (0)
#endif

#if defined(__SSE2__) || defined(__x86_64__) || defined(__x86_64) || \
    (defined(_M_AMD64) || defined(_M_X64) ||                         \
     (defined(_M_IX86_FP) && _M_IX86_FP == 2))
#define ADA_SSE2 1
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define ADA_NEON 1
#endif

#ifndef __has_cpp_attribute
#define ada_lifetime_bound
#elif __has_cpp_attribute(msvc::lifetimebound)
#define ada_lifetime_bound [[msvc::lifetimebound]]
#elif __has_cpp_attribute(clang::lifetimebound)
#define ada_lifetime_bound [[clang::lifetimebound]]
#elif __has_cpp_attribute(lifetimebound)
#define ada_lifetime_bound [[lifetimebound]]
#else
#define ada_lifetime_bound
#endif

#ifdef __cpp_lib_format
#if __cpp_lib_format >= 202110L
#include <format>
#define ADA_HAS_FORMAT 1
#endif
#endif

#endif  // ADA_COMMON_DEFS_H
/* end file include/ada/common_defs.h */
#include <cstdint>

/**
 * These functions are not part of our public API and may
 * change at any time.
 * @private
 * @namespace ada::character_sets
 * @brief Includes the definitions for unicode character sets.
 */
namespace ada::character_sets {
ada_really_inline constexpr bool bit_at(const uint8_t a[], uint8_t i);
}  // namespace ada::character_sets

#endif  // ADA_CHARACTER_SETS_H
/* end file include/ada/character_sets.h */
/* begin file include/ada/character_sets-inl.h */
/**
 * @file character_sets-inl.h
 * @brief Definitions of the character sets used by unicode functions.
 * @author Node.js
 * @see https://github.com/nodejs/node/blob/main/src/node_url_tables.cc
 */
#ifndef ADA_CHARACTER_SETS_INL_H
#define ADA_CHARACTER_SETS_INL_H


/**
 * These functions are not part of our public API and may
 * change at any time.
 * @private
 */
namespace ada::character_sets {

constexpr char hex[1024] =
    "%00\0%01\0%02\0%03\0%04\0%05\0%06\0%07\0"
    "%08\0%09\0%0A\0%0B\0%0C\0%0D\0%0E\0%0F\0"
    "%10\0%11\0%12\0%13\0%14\0%15\0%16\0%17\0"
    "%18\0%19\0%1A\0%1B\0%1C\0%1D\0%1E\0%1F\0"
    "%20\0%21\0%22\0%23\0%24\0%25\0%26\0%27\0"
    "%28\0%29\0%2A\0%2B\0%2C\0%2D\0%2E\0%2F\0"
    "%30\0%31\0%32\0%33\0%34\0%35\0%36\0%37\0"
    "%38\0%39\0%3A\0%3B\0%3C\0%3D\0%3E\0%3F\0"
    "%40\0%41\0%42\0%43\0%44\0%45\0%46\0%47\0"
    "%48\0%49\0%4A\0%4B\0%4C\0%4D\0%4E\0%4F\0"
    "%50\0%51\0%52\0%53\0%54\0%55\0%56\0%57\0"
    "%58\0%59\0%5A\0%5B\0%5C\0%5D\0%5E\0%5F\0"
    "%60\0%61\0%62\0%63\0%64\0%65\0%66\0%67\0"
    "%68\0%69\0%6A\0%6B\0%6C\0%6D\0%6E\0%6F\0"
    "%70\0%71\0%72\0%73\0%74\0%75\0%76\0%77\0"
    "%78\0%79\0%7A\0%7B\0%7C\0%7D\0%7E\0%7F\0"
    "%80\0%81\0%82\0%83\0%84\0%85\0%86\0%87\0"
    "%88\0%89\0%8A\0%8B\0%8C\0%8D\0%8E\0%8F\0"
    "%90\0%91\0%92\0%93\0%94\0%95\0%96\0%97\0"
    "%98\0%99\0%9A\0%9B\0%9C\0%9D\0%9E\0%9F\0"
    "%A0\0%A1\0%A2\0%A3\0%A4\0%A5\0%A6\0%A7\0"
    "%A8\0%A9\0%AA\0%AB\0%AC\0%AD\0%AE\0%AF\0"
    "%B0\0%B1\0%B2\0%B3\0%B4\0%B5\0%B6\0%B7\0"
    "%B8\0%B9\0%BA\0%BB\0%BC\0%BD\0%BE\0%BF\0"
    "%C0\0%C1\0%C2\0%C3\0%C4\0%C5\0%C6\0%C7\0"
    "%C8\0%C9\0%CA\0%CB\0%CC\0%CD\0%CE\0%CF\0"
    "%D0\0%D1\0%D2\0%D3\0%D4\0%D5\0%D6\0%D7\0"
    "%D8\0%D9\0%DA\0%DB\0%DC\0%DD\0%DE\0%DF\0"
    "%E0\0%E1\0%E2\0%E3\0%E4\0%E5\0%E6\0%E7\0"
    "%E8\0%E9\0%EA\0%EB\0%EC\0%ED\0%EE\0%EF\0"
    "%F0\0%F1\0%F2\0%F3\0%F4\0%F5\0%F6\0%F7\0"
    "%F8\0%F9\0%FA\0%FB\0%FC\0%FD\0%FE\0%FF";

constexpr uint8_t C0_CONTROL_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t SPECIAL_QUERY_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x80,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t QUERY_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t FRAGMENT_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
    // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t USERINFO_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 40     41     42     43     44     45     46     47
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x40 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t PATH_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x80,
    // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x00 | 0x20 | 0x00 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

constexpr uint8_t WWW_FORM_URLENCODED_PERCENT_ENCODE[32] = {
    // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 20     21     22     23     24     25     26     27
    0x00 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 28     29     2A     2B     2C     2D     2E     2F
    0x01 | 0x02 | 0x00 | 0x08 | 0x10 | 0x00 | 0x00 | 0x80,
    // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 40     41     42     43     44     45     46     47
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x08 | 0x00 | 0x20 | 0x40 | 0x00,
    // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
    // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
    // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80};

ada_really_inline constexpr bool bit_at(const uint8_t a[], const uint8_t i) {
  return !!(a[i >> 3] & (1 << (i & 7)));
}

}  // namespace ada::character_sets

#endif  // ADA_CHARACTER_SETS_INL_H
/* end file include/ada/character_sets-inl.h */
/* begin file include/ada/checkers-inl.h */
/**
 * @file checkers-inl.h
 * @brief Definitions for URL specific checkers used within Ada.
 */
#ifndef ADA_CHECKERS_INL_H
#define ADA_CHECKERS_INL_H

#include <bit>
#include <string_view>

namespace ada::checkers {

constexpr bool has_hex_prefix_unsafe(std::string_view input) {
  // This is actually efficient code, see has_hex_prefix for the assembly.
  constexpr bool is_little_endian = std::endian::native == std::endian::little;
  constexpr uint16_t word0x = 0x7830;
  uint16_t two_first_bytes =
      static_cast<uint16_t>(input[0]) |
      static_cast<uint16_t>((static_cast<uint16_t>(input[1]) << 8));
  if constexpr (is_little_endian) {
    two_first_bytes |= 0x2000;
  } else {
    two_first_bytes |= 0x020;
  }
  return two_first_bytes == word0x;
}

constexpr bool has_hex_prefix(std::string_view input) {
  return input.size() >= 2 && has_hex_prefix_unsafe(input);
}

constexpr bool is_digit(char x) noexcept { return (x >= '0') & (x <= '9'); }

constexpr char to_lower(char x) noexcept { return (x | 0x20); }

constexpr bool is_alpha(char x) noexcept {
  return (to_lower(x) >= 'a') && (to_lower(x) <= 'z');
}

constexpr bool is_windows_drive_letter(std::string_view input) noexcept {
  return input.size() >= 2 &&
         (is_alpha(input[0]) && ((input[1] == ':') || (input[1] == '|'))) &&
         ((input.size() == 2) || (input[2] == '/' || input[2] == '\\' ||
                                  input[2] == '?' || input[2] == '#'));
}

constexpr bool is_normalized_windows_drive_letter(
    std::string_view input) noexcept {
  return input.size() >= 2 && (is_alpha(input[0]) && (input[1] == ':'));
}

}  // namespace ada::checkers

#endif  // ADA_CHECKERS_INL_H
/* end file include/ada/checkers-inl.h */
/* begin file include/ada/log.h */
/**
 * @file log.h
 * @brief Includes the definitions for logging.
 * @private Excluded from docs through the doxygen file.
 */
#ifndef ADA_LOG_H
#define ADA_LOG_H

// To enable logging, set ADA_LOGGING to 1:
#ifndef ADA_LOGGING
#define ADA_LOGGING 0
#endif

#if ADA_LOGGING
#include <iostream>
#endif  // ADA_LOGGING

namespace ada {

/**
 * Log a message. If you want to have no overhead when logging is disabled, use
 * the ada_log macro.
 * @private
 */
template <typename... Args>
constexpr ada_really_inline void log([[maybe_unused]] Args... args) {
#if ADA_LOGGING
  ((std::cout << "ADA_LOG: ") << ... << args) << std::endl;
#endif  // ADA_LOGGING
}
}  // namespace ada

#if ADA_LOGGING
#ifndef ada_log
#define ada_log(...)       \
  do {                     \
    ada::log(__VA_ARGS__); \
  } while (0)
#endif  // ada_log
#else
#define ada_log(...)
#endif  // ADA_LOGGING

#endif  // ADA_LOG_H
/* end file include/ada/log.h */
/* begin file include/ada/encoding_type.h */
/**
 * @file encoding_type.h
 * @brief Definition for supported encoding types.
 */
#ifndef ADA_ENCODING_TYPE_H
#define ADA_ENCODING_TYPE_H

#include <string>

namespace ada {

/**
 * This specification defines three encodings with the same names as encoding
 * schemes defined in the Unicode standard: UTF-8, UTF-16LE, and UTF-16BE.
 *
 * @see https://encoding.spec.whatwg.org/#encodings
 */
enum class encoding_type {
  UTF8,
  UTF_16LE,
  UTF_16BE,
};

/**
 * Convert a encoding_type to string.
 */
ada_warn_unused std::string to_string(encoding_type type);

}  // namespace ada

#endif  // ADA_ENCODING_TYPE_H
/* end file include/ada/encoding_type.h */
/* begin file include/ada/helpers.h */
/**
 * @file helpers.h
 * @brief Definitions for helper functions used within Ada.
 */
#ifndef ADA_HELPERS_H
#define ADA_HELPERS_H

/* begin file include/ada/url_base.h */
/**
 * @file url_base.h
 * @brief Declaration for the basic URL definitions
 */
#ifndef ADA_URL_BASE_H
#define ADA_URL_BASE_H

/* begin file include/ada/scheme.h */
/**
 * @file scheme.h
 * @brief Declarations for the URL scheme.
 */
#ifndef ADA_SCHEME_H
#define ADA_SCHEME_H


#include <string>

/**
 * @namespace ada::scheme
 * @brief Includes the scheme declarations
 */
namespace ada::scheme {

/**
 * Type of the scheme as an enum.
 * Using strings to represent a scheme type is not ideal because
 * checking for types involves string comparisons. It is faster to use
 * a simple integer.
 * In C++11, we are allowed to specify the underlying type of the enum.
 * We pick an 8-bit integer (which allows up to 256 types). Specifying the
 * type of the enum may help integration with other systems if the type
 * variable is exposed (since its value will not depend on the compiler).
 */
enum type : uint8_t {
  HTTP = 0,
  NOT_SPECIAL = 1,
  HTTPS = 2,
  WS = 3,
  FTP = 4,
  WSS = 5,
  FILE = 6
};

/**
 * A special scheme is an ASCII string that is listed in the first column of the
 * following table. The default port for a special scheme is listed in the
 * second column on the same row. The default port for any other ASCII string is
 * null.
 *
 * @see https://url.spec.whatwg.org/#url-miscellaneous
 * @param scheme
 * @return If scheme is a special scheme
 */
ada_really_inline constexpr bool is_special(std::string_view scheme);

/**
 * A special scheme is an ASCII string that is listed in the first column of the
 * following table. The default port for a special scheme is listed in the
 * second column on the same row. The default port for any other ASCII string is
 * null.
 *
 * @see https://url.spec.whatwg.org/#url-miscellaneous
 * @param scheme
 * @return The special port
 */
constexpr uint16_t get_special_port(std::string_view scheme) noexcept;

/**
 * Returns the port number of a special scheme.
 * @see https://url.spec.whatwg.org/#special-scheme
 */
constexpr uint16_t get_special_port(ada::scheme::type type) noexcept;
/**
 * Returns the scheme of an input, or NOT_SPECIAL if it's not a special scheme
 * defined by the spec.
 */
constexpr ada::scheme::type get_scheme_type(std::string_view scheme) noexcept;

}  // namespace ada::scheme

#endif  // ADA_SCHEME_H
/* end file include/ada/scheme.h */

#include <string>
#include <string_view>

namespace ada {

/**
 * Type of URL host as an enum.
 */
enum url_host_type : uint8_t {
  /**
   * Represents common URLs such as "https://www.google.com"
   */
  DEFAULT = 0,
  /**
   * Represents ipv4 addresses such as "http://127.0.0.1"
   */
  IPV4 = 1,
  /**
   * Represents ipv6 addresses such as
   * "http://[2001:db8:3333:4444:5555:6666:7777:8888]"
   */
  IPV6 = 2,
};

/**
 * @brief Base class of URL implementations
 *
 * @details A url_base contains a few attributes: is_valid, has_opaque_path and
 * type. All non-trivial implementation details are in derived classes such as
 * ada::url and ada::url_aggregator.
 *
 * It is an abstract class that cannot be instantiated directly.
 */
struct url_base {
  virtual ~url_base() = default;

  /**
   * Used for returning the validity from the result of the URL parser.
   */
  bool is_valid{true};

  /**
   * A URL has an opaque path if its path is a string.
   */
  bool has_opaque_path{false};

  /**
   * URL hosts type
   */
  url_host_type host_type = url_host_type::DEFAULT;

  /**
   * @private
   */
  ada::scheme::type type{ada::scheme::type::NOT_SPECIAL};

  /**
   * A URL is special if its scheme is a special scheme. A URL is not special if
   * its scheme is not a special scheme.
   */
  [[nodiscard]] ada_really_inline constexpr bool is_special() const noexcept;

  /**
   * The origin getter steps are to return the serialization of this's URL's
   * origin. [HTML]
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#concept-url-origin
   */
  [[nodiscard]] virtual std::string get_origin() const noexcept = 0;

  /**
   * Returns true if this URL has a valid domain as per RFC 1034 and
   * corresponding specifications. Among other things, it requires
   * that the domain string has fewer than 255 octets.
   */
  [[nodiscard]] virtual bool has_valid_domain() const noexcept = 0;

  /**
   * @private
   *
   * Return the 'special port' if the URL is special and not 'file'.
   * Returns 0 otherwise.
   */
  [[nodiscard]] inline uint16_t get_special_port() const noexcept;

  /**
   * @private
   *
   * Get the default port if the url's scheme has one, returns 0 otherwise.
   */
  [[nodiscard]] ada_really_inline uint16_t scheme_default_port() const noexcept;

  /**
   * @private
   *
   * Parse a port (16-bit decimal digit) from the provided input.
   * We assume that the input does not contain spaces or tabs
   * within the ASCII digits.
   * It returns how many bytes were consumed when a number is successfully
   * parsed.
   * @return On failure, it returns zero.
   * @see https://url.spec.whatwg.org/#host-parsing
   */
  virtual size_t parse_port(std::string_view view,
                            bool check_trailing_content) noexcept = 0;

  virtual ada_really_inline size_t parse_port(std::string_view view) noexcept {
    return this->parse_port(view, false);
  }

  /**
   * Returns a JSON string representation of this URL.
   */
  [[nodiscard]] virtual std::string to_string() const = 0;

  /** @private */
  virtual inline void clear_pathname() = 0;

  /** @private */
  virtual inline void clear_search() = 0;

  /** @private */
  [[nodiscard]] virtual inline bool has_hash() const noexcept = 0;

  /** @private */
  [[nodiscard]] virtual inline bool has_search() const noexcept = 0;

};  // url_base

}  // namespace ada

#endif
/* end file include/ada/url_base.h */

#include <string>
#include <string_view>
#include <optional>

#if ADA_DEVELOPMENT_CHECKS
#include <iostream>
#endif  // ADA_DEVELOPMENT_CHECKS

/**
 * These functions are not part of our public API and may
 * change at any time.
 *
 * @private
 * @namespace ada::helpers
 * @brief Includes the definitions for helper functions
 */
namespace ada::helpers {

/**
 * @private
 */
template <typename out_iter>
void encode_json(std::string_view view, out_iter out);

/**
 * @private
 * This function is used to prune a fragment from a url, and returning the
 * removed string if input has fragment.
 *
 * @details prune_hash seeks the first '#' and returns everything after it
 * as a string_view, and modifies (in place) the input so that it points at
 * everything before the '#'. If no '#' is found, the input is left unchanged
 * and std::nullopt is returned.
 *
 * @attention The function is non-allocating and it does not throw.
 * @returns Note that the returned string_view might be empty!
 */
ada_really_inline std::optional<std::string_view> prune_hash(
    std::string_view& input) noexcept;

/**
 * @private
 * Defined by the URL specification, shorten a URLs paths.
 * @see https://url.spec.whatwg.org/#shorten-a-urls-path
 * @returns Returns true if path is shortened.
 */
ada_really_inline bool shorten_path(std::string& path,
                                    ada::scheme::type type) noexcept;

/**
 * @private
 * Defined by the URL specification, shorten a URLs paths.
 * @see https://url.spec.whatwg.org/#shorten-a-urls-path
 * @returns Returns true if path is shortened.
 */
ada_really_inline bool shorten_path(std::string_view& path,
                                    ada::scheme::type type) noexcept;

/**
 * @private
 *
 * Parse the path from the provided input and append to the existing
 * (possibly empty) path. The input cannot contain tabs and spaces: it
 * is the user's responsibility to check.
 *
 * The input is expected to be UTF-8.
 *
 * @see https://url.spec.whatwg.org/
 */
ada_really_inline void parse_prepared_path(std::string_view input,
                                           ada::scheme::type type,
                                           std::string& path);

/**
 * @private
 * Remove and mutate all ASCII tab or newline characters from an input.
 */
ada_really_inline void remove_ascii_tab_or_newline(std::string& input) noexcept;

/**
 * @private
 * Return the substring from input going from index pos to the end.
 * This function cannot throw.
 */
ada_really_inline constexpr std::string_view substring(std::string_view input,
                                                       size_t pos) noexcept;

/**
 * @private
 * Returns true if the string_view points within the string.
 */
bool overlaps(std::string_view input1, const std::string& input2) noexcept;

/**
 * @private
 * Return the substring from input going from index pos1 to the pos2 (non
 * included). The length of the substring is pos2 - pos1.
 */
ada_really_inline constexpr std::string_view substring(std::string_view input,
                                                       size_t pos1,
                                                       size_t pos2) noexcept {
#if ADA_DEVELOPMENT_CHECKS
  if (pos2 < pos1) {
    std::cerr << "Negative-length substring: [" << pos1 << " to " << pos2 << ")"
              << std::endl;
    abort();
  }
#endif
  return input.substr(pos1, pos2 - pos1);
}

/**
 * @private
 * Modify the string_view so that it has the new size pos, assuming that pos <=
 * input.size(). This function cannot throw.
 */
ada_really_inline void resize(std::string_view& input, size_t pos) noexcept;

/**
 * @private
 * Returns a host's delimiter location depending on the state of the instance,
 * and whether a colon was found outside brackets. Used by the host parser.
 */
ada_really_inline std::pair<size_t, bool> get_host_delimiter_location(
    bool is_special, std::string_view& view) noexcept;

/**
 * @private
 * Removes leading and trailing C0 control and whitespace characters from
 * string.
 */
void trim_c0_whitespace(std::string_view& input) noexcept;

/**
 * @private
 * @see
 * https://url.spec.whatwg.org/#potentially-strip-trailing-spaces-from-an-opaque-path
 */
template <class url_type>
ada_really_inline void strip_trailing_spaces_from_opaque_path(
    url_type& url) noexcept;

/**
 * @private
 * Finds the delimiter of a view in authority state.
 */
ada_really_inline size_t
find_authority_delimiter_special(std::string_view view) noexcept;

/**
 * @private
 * Finds the delimiter of a view in authority state.
 */
ada_really_inline size_t
find_authority_delimiter(std::string_view view) noexcept;

/**
 * @private
 */
template <typename T, typename... Args>
inline void inner_concat(std::string& buffer, T t) {
  buffer.append(t);
}

/**
 * @private
 */
template <typename T, typename... Args>
inline void inner_concat(std::string& buffer, T t, Args... args) {
  buffer.append(t);
  return inner_concat(buffer, args...);
}

/**
 * @private
 * Concatenate the arguments and return a string.
 * @returns a string
 */
template <typename... Args>
std::string concat(Args... args) {
  std::string answer;
  inner_concat(answer, args...);
  return answer;
}

/**
 * @private
 * @return Number of leading zeroes.
 */
inline int leading_zeroes(uint32_t input_num) noexcept {
#if ADA_REGULAR_VISUAL_STUDIO
  unsigned long leading_zero(0);
  unsigned long in(input_num);
  return _BitScanReverse(&leading_zero, in) ? int(31 - leading_zero) : 32;
#else
  return __builtin_clz(input_num);
#endif  // ADA_REGULAR_VISUAL_STUDIO
}

/**
 * @private
 * Counts the number of decimal digits necessary to represent x.
 * faster than std::to_string(x).size().
 * @return digit count
 */
inline int fast_digit_count(uint32_t x) noexcept {
  auto int_log2 = [](uint32_t z) -> int {
    return 31 - ada::helpers::leading_zeroes(z | 1);
  };
  // Compiles to very few instructions. Note that the
  // table is static and thus effectively a constant.
  // We leave it inside the function because it is meaningless
  // outside of it (this comes at no performance cost).
  const static uint64_t table[] = {
      4294967296,  8589934582,  8589934582,  8589934582,  12884901788,
      12884901788, 12884901788, 17179868184, 17179868184, 17179868184,
      21474826480, 21474826480, 21474826480, 21474826480, 25769703776,
      25769703776, 25769703776, 30063771072, 30063771072, 30063771072,
      34349738368, 34349738368, 34349738368, 34349738368, 38554705664,
      38554705664, 38554705664, 41949672960, 41949672960, 41949672960,
      42949672960, 42949672960};
  return int((x + table[int_log2(x)]) >> 32);
}
}  // namespace ada::helpers

#endif  // ADA_HELPERS_H
/* end file include/ada/helpers.h */
/* begin file include/ada/parser.h */
/**
 * @file parser.h
 * @brief Definitions for the parser.
 */
#ifndef ADA_PARSER_H
#define ADA_PARSER_H

#include <string_view>
#include <variant>

/* begin file include/ada/expected.h */
/**
 * @file expected.h
 * @brief Definitions for std::expected
 * @private Excluded from docs through the doxygen file.
 */
///
// expected - An implementation of std::expected with extensions
// Written in 2017 by Sy Brand (tartanllama@gmail.com, @TartanLlama)
//
// Documentation available at http://tl.tartanllama.xyz/
//
// To the extent possible under law, the author(s) have dedicated all
// copyright and related and neighboring rights to this software to the
// public domain worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication
// along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
///

#ifndef TL_EXPECTED_HPP
#define TL_EXPECTED_HPP

#define TL_EXPECTED_VERSION_MAJOR 1
#define TL_EXPECTED_VERSION_MINOR 1
#define TL_EXPECTED_VERSION_PATCH 0

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

#if defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#define TL_EXPECTED_EXCEPTIONS_ENABLED
#endif

#if (defined(_MSC_VER) && _MSC_VER == 1900)
#define TL_EXPECTED_MSVC2015
#define TL_EXPECTED_MSVC2015_CONSTEXPR
#else
#define TL_EXPECTED_MSVC2015_CONSTEXPR constexpr
#endif

#if (defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ <= 9 && \
     !defined(__clang__))
#define TL_EXPECTED_GCC49
#endif

#if (defined(__GNUC__) && __GNUC__ == 5 && __GNUC_MINOR__ <= 4 && \
     !defined(__clang__))
#define TL_EXPECTED_GCC54
#endif

#if (defined(__GNUC__) && __GNUC__ == 5 && __GNUC_MINOR__ <= 5 && \
     !defined(__clang__))
#define TL_EXPECTED_GCC55
#endif

#if !defined(TL_ASSERT)
// can't have assert in constexpr in C++11 and GCC 4.9 has a compiler bug
#if (__cplusplus > 201103L) && !defined(TL_EXPECTED_GCC49)
#include <cassert>
#define TL_ASSERT(x) assert(x)
#else
#define TL_ASSERT(x)
#endif
#endif

#if (defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ <= 9 && \
     !defined(__clang__))
// GCC < 5 doesn't support overloading on const&& for member functions

#define TL_EXPECTED_NO_CONSTRR
// GCC < 5 doesn't support some standard C++11 type traits
#define TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(T) \
  std::has_trivial_copy_constructor<T>
#define TL_EXPECTED_IS_TRIVIALLY_COPY_ASSIGNABLE(T) \
  std::has_trivial_copy_assign<T>

// This one will be different for GCC 5.7 if it's ever supported
#define TL_EXPECTED_IS_TRIVIALLY_DESTRUCTIBLE(T) \
  std::is_trivially_destructible<T>

// GCC 5 < v < 8 has a bug in is_trivially_copy_constructible which breaks
// std::vector for non-copyable types
#elif (defined(__GNUC__) && __GNUC__ < 8 && !defined(__clang__))
#ifndef TL_GCC_LESS_8_TRIVIALLY_COPY_CONSTRUCTIBLE_MUTEX
#define TL_GCC_LESS_8_TRIVIALLY_COPY_CONSTRUCTIBLE_MUTEX
namespace tl {
namespace detail {
template <class T>
struct is_trivially_copy_constructible
    : std::is_trivially_copy_constructible<T> {};
#ifdef _GLIBCXX_VECTOR
template <class T, class A>
struct is_trivially_copy_constructible<std::vector<T, A>> : std::false_type {};
#endif
}  // namespace detail
}  // namespace tl
#endif

#define TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(T) \
  tl::detail::is_trivially_copy_constructible<T>
#define TL_EXPECTED_IS_TRIVIALLY_COPY_ASSIGNABLE(T) \
  std::is_trivially_copy_assignable<T>
#define TL_EXPECTED_IS_TRIVIALLY_DESTRUCTIBLE(T) \
  std::is_trivially_destructible<T>
#else
#define TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(T) \
  std::is_trivially_copy_constructible<T>
#define TL_EXPECTED_IS_TRIVIALLY_COPY_ASSIGNABLE(T) \
  std::is_trivially_copy_assignable<T>
#define TL_EXPECTED_IS_TRIVIALLY_DESTRUCTIBLE(T) \
  std::is_trivially_destructible<T>
#endif

#if __cplusplus > 201103L
#define TL_EXPECTED_CXX14
#endif

#ifdef TL_EXPECTED_GCC49
#define TL_EXPECTED_GCC49_CONSTEXPR
#else
#define TL_EXPECTED_GCC49_CONSTEXPR constexpr
#endif

#if (__cplusplus == 201103L || defined(TL_EXPECTED_MSVC2015) || \
     defined(TL_EXPECTED_GCC49))
#define TL_EXPECTED_11_CONSTEXPR
#else
#define TL_EXPECTED_11_CONSTEXPR constexpr
#endif

namespace tl {
template <class T, class E>
class expected;

#ifndef TL_MONOSTATE_INPLACE_MUTEX
#define TL_MONOSTATE_INPLACE_MUTEX
class monostate {};

struct in_place_t {
  explicit in_place_t() = default;
};
static constexpr in_place_t in_place{};
#endif

template <class E>
class unexpected {
 public:
  static_assert(!std::is_same<E, void>::value, "E must not be void");

  unexpected() = delete;
  constexpr explicit unexpected(const E &e) : m_val(e) {}

  constexpr explicit unexpected(E &&e) : m_val(std::move(e)) {}

  template <class... Args, typename std::enable_if<std::is_constructible<
                               E, Args &&...>::value>::type * = nullptr>
  constexpr explicit unexpected(Args &&...args)
      : m_val(std::forward<Args>(args)...) {}
  template <
      class U, class... Args,
      typename std::enable_if<std::is_constructible<
          E, std::initializer_list<U> &, Args &&...>::value>::type * = nullptr>
  constexpr explicit unexpected(std::initializer_list<U> l, Args &&...args)
      : m_val(l, std::forward<Args>(args)...) {}

  constexpr const E &value() const & { return m_val; }
  TL_EXPECTED_11_CONSTEXPR E &value() & { return m_val; }
  TL_EXPECTED_11_CONSTEXPR E &&value() && { return std::move(m_val); }
  constexpr const E &&value() const && { return std::move(m_val); }

 private:
  E m_val;
};

#ifdef __cpp_deduction_guides
template <class E>
unexpected(E) -> unexpected<E>;
#endif

template <class E>
constexpr bool operator==(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() == rhs.value();
}
template <class E>
constexpr bool operator!=(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() != rhs.value();
}
template <class E>
constexpr bool operator<(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() < rhs.value();
}
template <class E>
constexpr bool operator<=(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() <= rhs.value();
}
template <class E>
constexpr bool operator>(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() > rhs.value();
}
template <class E>
constexpr bool operator>=(const unexpected<E> &lhs, const unexpected<E> &rhs) {
  return lhs.value() >= rhs.value();
}

template <class E>
unexpected<typename std::decay<E>::type> make_unexpected(E &&e) {
  return unexpected<typename std::decay<E>::type>(std::forward<E>(e));
}

struct unexpect_t {
  unexpect_t() = default;
};
static constexpr unexpect_t unexpect{};

namespace detail {
template <typename E>
[[noreturn]] TL_EXPECTED_11_CONSTEXPR void throw_exception(E &&e) {
#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
  throw std::forward<E>(e);
#else
  (void)e;
#ifdef _MSC_VER
  __assume(0);
#else
  __builtin_unreachable();
#endif
#endif
}

#ifndef TL_TRAITS_MUTEX
#define TL_TRAITS_MUTEX
// C++14-style aliases for brevity
template <class T>
using remove_const_t = typename std::remove_const<T>::type;
template <class T>
using remove_reference_t = typename std::remove_reference<T>::type;
template <class T>
using decay_t = typename std::decay<T>::type;
template <bool E, class T = void>
using enable_if_t = typename std::enable_if<E, T>::type;
template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;

// std::conjunction from C++17
template <class...>
struct conjunction : std::true_type {};
template <class B>
struct conjunction<B> : B {};
template <class B, class... Bs>
struct conjunction<B, Bs...>
    : std::conditional<bool(B::value), conjunction<Bs...>, B>::type {};

#if defined(_LIBCPP_VERSION) && __cplusplus == 201103L
#define TL_TRAITS_LIBCXX_MEM_FN_WORKAROUND
#endif

// In C++11 mode, there's an issue in libc++'s std::mem_fn
// which results in a hard-error when using it in a noexcept expression
// in some cases. This is a check to workaround the common failing case.
#ifdef TL_TRAITS_LIBCXX_MEM_FN_WORKAROUND
template <class T>
struct is_pointer_to_non_const_member_func : std::false_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...)>
    : std::true_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...) &>
    : std::true_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...) &&>
    : std::true_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...) volatile>
    : std::true_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...) volatile &>
    : std::true_type {};
template <class T, class Ret, class... Args>
struct is_pointer_to_non_const_member_func<Ret (T::*)(Args...) volatile &&>
    : std::true_type {};

template <class T>
struct is_const_or_const_ref : std::false_type {};
template <class T>
struct is_const_or_const_ref<T const &> : std::true_type {};
template <class T>
struct is_const_or_const_ref<T const> : std::true_type {};
#endif

// std::invoke from C++17
// https://stackoverflow.com/questions/38288042/c11-14-invoke-workaround
template <
    typename Fn, typename... Args,
#ifdef TL_TRAITS_LIBCXX_MEM_FN_WORKAROUND
    typename = enable_if_t<!(is_pointer_to_non_const_member_func<Fn>::value &&
                             is_const_or_const_ref<Args...>::value)>,
#endif
    typename = enable_if_t<std::is_member_pointer<decay_t<Fn>>::value>, int = 0>
constexpr auto invoke(Fn &&f, Args &&...args) noexcept(
    noexcept(std::mem_fn(f)(std::forward<Args>(args)...)))
    -> decltype(std::mem_fn(f)(std::forward<Args>(args)...)) {
  return std::mem_fn(f)(std::forward<Args>(args)...);
}

template <typename Fn, typename... Args,
          typename = enable_if_t<!std::is_member_pointer<decay_t<Fn>>::value>>
constexpr auto invoke(Fn &&f, Args &&...args) noexcept(
    noexcept(std::forward<Fn>(f)(std::forward<Args>(args)...)))
    -> decltype(std::forward<Fn>(f)(std::forward<Args>(args)...)) {
  return std::forward<Fn>(f)(std::forward<Args>(args)...);
}

// std::invoke_result from C++17
template <class F, class, class... Us>
struct invoke_result_impl;

template <class F, class... Us>
struct invoke_result_impl<
    F,
    decltype(detail::invoke(std::declval<F>(), std::declval<Us>()...), void()),
    Us...> {
  using type =
      decltype(detail::invoke(std::declval<F>(), std::declval<Us>()...));
};

template <class F, class... Us>
using invoke_result = invoke_result_impl<F, void, Us...>;

template <class F, class... Us>
using invoke_result_t = typename invoke_result<F, Us...>::type;

#if defined(_MSC_VER) && _MSC_VER <= 1900
// TODO make a version which works with MSVC 2015
template <class T, class U = T>
struct is_swappable : std::true_type {};

template <class T, class U = T>
struct is_nothrow_swappable : std::true_type {};
#else
// https://stackoverflow.com/questions/26744589/what-is-a-proper-way-to-implement-is-swappable-to-test-for-the-swappable-concept
namespace swap_adl_tests {
// if swap ADL finds this then it would call std::swap otherwise (same
// signature)
struct tag {};

template <class T>
tag swap(T &, T &);
template <class T, std::size_t N>
tag swap(T (&a)[N], T (&b)[N]);

// helper functions to test if an unqualified swap is possible, and if it
// becomes std::swap
template <class, class>
std::false_type can_swap(...) noexcept(false);
template <class T, class U,
          class = decltype(swap(std::declval<T &>(), std::declval<U &>()))>
std::true_type can_swap(int) noexcept(noexcept(swap(std::declval<T &>(),
                                                    std::declval<U &>())));

template <class, class>
std::false_type uses_std(...);
template <class T, class U>
std::is_same<decltype(swap(std::declval<T &>(), std::declval<U &>())), tag>
uses_std(int);

template <class T>
struct is_std_swap_noexcept
    : std::integral_constant<bool,
                             std::is_nothrow_move_constructible<T>::value &&
                                 std::is_nothrow_move_assignable<T>::value> {};

template <class T, std::size_t N>
struct is_std_swap_noexcept<T[N]> : is_std_swap_noexcept<T> {};

template <class T, class U>
struct is_adl_swap_noexcept
    : std::integral_constant<bool, noexcept(can_swap<T, U>(0))> {};
}  // namespace swap_adl_tests

template <class T, class U = T>
struct is_swappable
    : std::integral_constant<
          bool,
          decltype(detail::swap_adl_tests::can_swap<T, U>(0))::value &&
              (!decltype(detail::swap_adl_tests::uses_std<T, U>(0))::value ||
               (std::is_move_assignable<T>::value &&
                std::is_move_constructible<T>::value))> {};

template <class T, std::size_t N>
struct is_swappable<T[N], T[N]>
    : std::integral_constant<
          bool,
          decltype(detail::swap_adl_tests::can_swap<T[N], T[N]>(0))::value &&
              (!decltype(detail::swap_adl_tests::uses_std<T[N], T[N]>(
                   0))::value ||
               is_swappable<T, T>::value)> {};

template <class T, class U = T>
struct is_nothrow_swappable
    : std::integral_constant<
          bool,
          is_swappable<T, U>::value &&
              ((decltype(detail::swap_adl_tests::uses_std<T, U>(0))::value &&
                detail::swap_adl_tests::is_std_swap_noexcept<T>::value) ||
               (!decltype(detail::swap_adl_tests::uses_std<T, U>(0))::value &&
                detail::swap_adl_tests::is_adl_swap_noexcept<T, U>::value))> {};
#endif
#endif

// Trait for checking if a type is a tl::expected
template <class T>
struct is_expected_impl : std::false_type {};
template <class T, class E>
struct is_expected_impl<expected<T, E>> : std::true_type {};
template <class T>
using is_expected = is_expected_impl<decay_t<T>>;

template <class T, class E, class U>
using expected_enable_forward_value = detail::enable_if_t<
    std::is_constructible<T, U &&>::value &&
    !std::is_same<detail::decay_t<U>, in_place_t>::value &&
    !std::is_same<expected<T, E>, detail::decay_t<U>>::value &&
    !std::is_same<unexpected<E>, detail::decay_t<U>>::value>;

template <class T, class E, class U, class G, class UR, class GR>
using expected_enable_from_other = detail::enable_if_t<
    std::is_constructible<T, UR>::value &&
    std::is_constructible<E, GR>::value &&
    !std::is_constructible<T, expected<U, G> &>::value &&
    !std::is_constructible<T, expected<U, G> &&>::value &&
    !std::is_constructible<T, const expected<U, G> &>::value &&
    !std::is_constructible<T, const expected<U, G> &&>::value &&
    !std::is_convertible<expected<U, G> &, T>::value &&
    !std::is_convertible<expected<U, G> &&, T>::value &&
    !std::is_convertible<const expected<U, G> &, T>::value &&
    !std::is_convertible<const expected<U, G> &&, T>::value>;

template <class T, class U>
using is_void_or = conditional_t<std::is_void<T>::value, std::true_type, U>;

template <class T>
using is_copy_constructible_or_void =
    is_void_or<T, std::is_copy_constructible<T>>;

template <class T>
using is_move_constructible_or_void =
    is_void_or<T, std::is_move_constructible<T>>;

template <class T>
using is_copy_assignable_or_void = is_void_or<T, std::is_copy_assignable<T>>;

template <class T>
using is_move_assignable_or_void = is_void_or<T, std::is_move_assignable<T>>;

}  // namespace detail

namespace detail {
struct no_init_t {};
static constexpr no_init_t no_init{};

// Implements the storage of the values, and ensures that the destructor is
// trivial if it can be.
//
// This specialization is for where neither `T` or `E` is trivially
// destructible, so the destructors must be called on destruction of the
// `expected`
template <class T, class E, bool = std::is_trivially_destructible<T>::value,
          bool = std::is_trivially_destructible<E>::value>
struct expected_storage_base {
  constexpr expected_storage_base() : m_val(T{}), m_has_val(true) {}
  constexpr expected_storage_base(no_init_t) : m_no_init(), m_has_val(false) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
                nullptr>
  constexpr expected_storage_base(in_place_t, Args &&...args)
      : m_val(std::forward<Args>(args)...), m_has_val(true) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr expected_storage_base(in_place_t, std::initializer_list<U> il,
                                  Args &&...args)
      : m_val(il, std::forward<Args>(args)...), m_has_val(true) {}
  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() {
    if (m_has_val) {
      m_val.~T();
    } else {
      m_unexpect.~unexpected<E>();
    }
  }
  union {
    T m_val;
    unexpected<E> m_unexpect;
    char m_no_init;
  };
  bool m_has_val;
};

// This specialization is for when both `T` and `E` are trivially-destructible,
// so the destructor of the `expected` can be trivial.
template <class T, class E>
struct expected_storage_base<T, E, true, true> {
  constexpr expected_storage_base() : m_val(T{}), m_has_val(true) {}
  constexpr expected_storage_base(no_init_t) : m_no_init(), m_has_val(false) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
                nullptr>
  constexpr expected_storage_base(in_place_t, Args &&...args)
      : m_val(std::forward<Args>(args)...), m_has_val(true) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr expected_storage_base(in_place_t, std::initializer_list<U> il,
                                  Args &&...args)
      : m_val(il, std::forward<Args>(args)...), m_has_val(true) {}
  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() = default;
  union {
    T m_val;
    unexpected<E> m_unexpect;
    char m_no_init;
  };
  bool m_has_val;
};

// T is trivial, E is not.
template <class T, class E>
struct expected_storage_base<T, E, true, false> {
  constexpr expected_storage_base() : m_val(T{}), m_has_val(true) {}
  TL_EXPECTED_MSVC2015_CONSTEXPR expected_storage_base(no_init_t)
      : m_no_init(), m_has_val(false) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
                nullptr>
  constexpr expected_storage_base(in_place_t, Args &&...args)
      : m_val(std::forward<Args>(args)...), m_has_val(true) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr expected_storage_base(in_place_t, std::initializer_list<U> il,
                                  Args &&...args)
      : m_val(il, std::forward<Args>(args)...), m_has_val(true) {}
  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() {
    if (!m_has_val) {
      m_unexpect.~unexpected<E>();
    }
  }

  union {
    T m_val;
    unexpected<E> m_unexpect;
    char m_no_init;
  };
  bool m_has_val;
};

// E is trivial, T is not.
template <class T, class E>
struct expected_storage_base<T, E, false, true> {
  constexpr expected_storage_base() : m_val(T{}), m_has_val(true) {}
  constexpr expected_storage_base(no_init_t) : m_no_init(), m_has_val(false) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
                nullptr>
  constexpr expected_storage_base(in_place_t, Args &&...args)
      : m_val(std::forward<Args>(args)...), m_has_val(true) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr expected_storage_base(in_place_t, std::initializer_list<U> il,
                                  Args &&...args)
      : m_val(il, std::forward<Args>(args)...), m_has_val(true) {}
  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() {
    if (m_has_val) {
      m_val.~T();
    }
  }
  union {
    T m_val;
    unexpected<E> m_unexpect;
    char m_no_init;
  };
  bool m_has_val;
};

// `T` is `void`, `E` is trivially-destructible
template <class E>
struct expected_storage_base<void, E, false, true> {
#if __GNUC__ <= 5
// no constexpr for GCC 4/5 bug
#else
  TL_EXPECTED_MSVC2015_CONSTEXPR
#endif
  expected_storage_base() : m_has_val(true) {}

  constexpr expected_storage_base(no_init_t) : m_val(), m_has_val(false) {}

  constexpr expected_storage_base(in_place_t) : m_has_val(true) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() = default;
  struct dummy {};
  union {
    unexpected<E> m_unexpect;
    dummy m_val;
  };
  bool m_has_val;
};

// `T` is `void`, `E` is not trivially-destructible
template <class E>
struct expected_storage_base<void, E, false, false> {
  constexpr expected_storage_base() : m_dummy(), m_has_val(true) {}
  constexpr expected_storage_base(no_init_t) : m_dummy(), m_has_val(false) {}

  constexpr expected_storage_base(in_place_t) : m_dummy(), m_has_val(true) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected_storage_base(unexpect_t, Args &&...args)
      : m_unexpect(std::forward<Args>(args)...), m_has_val(false) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected_storage_base(unexpect_t,
                                           std::initializer_list<U> il,
                                           Args &&...args)
      : m_unexpect(il, std::forward<Args>(args)...), m_has_val(false) {}

  ~expected_storage_base() {
    if (!m_has_val) {
      m_unexpect.~unexpected<E>();
    }
  }

  union {
    unexpected<E> m_unexpect;
    char m_dummy;
  };
  bool m_has_val;
};

// This base class provides some handy member functions which can be used in
// further derived classes
template <class T, class E>
struct expected_operations_base : expected_storage_base<T, E> {
  using expected_storage_base<T, E>::expected_storage_base;

  template <class... Args>
  void construct(Args &&...args) noexcept {
    new (std::addressof(this->m_val)) T(std::forward<Args>(args)...);
    this->m_has_val = true;
  }

  template <class Rhs>
  void construct_with(Rhs &&rhs) noexcept {
    new (std::addressof(this->m_val)) T(std::forward<Rhs>(rhs).get());
    this->m_has_val = true;
  }

  template <class... Args>
  void construct_error(Args &&...args) noexcept {
    new (std::addressof(this->m_unexpect))
        unexpected<E>(std::forward<Args>(args)...);
    this->m_has_val = false;
  }

#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED

  // These assign overloads ensure that the most efficient assignment
  // implementation is used while maintaining the strong exception guarantee.
  // The problematic case is where rhs has a value, but *this does not.
  //
  // This overload handles the case where we can just copy-construct `T`
  // directly into place without throwing.
  template <class U = T,
            detail::enable_if_t<std::is_nothrow_copy_constructible<U>::value>
                * = nullptr>
  void assign(const expected_operations_base &rhs) noexcept {
    if (!this->m_has_val && rhs.m_has_val) {
      geterr().~unexpected<E>();
      construct(rhs.get());
    } else {
      assign_common(rhs);
    }
  }

  // This overload handles the case where we can attempt to create a copy of
  // `T`, then no-throw move it into place if the copy was successful.
  template <class U = T,
            detail::enable_if_t<!std::is_nothrow_copy_constructible<U>::value &&
                                std::is_nothrow_move_constructible<U>::value>
                * = nullptr>
  void assign(const expected_operations_base &rhs) noexcept {
    if (!this->m_has_val && rhs.m_has_val) {
      T tmp = rhs.get();
      geterr().~unexpected<E>();
      construct(std::move(tmp));
    } else {
      assign_common(rhs);
    }
  }

  // This overload is the worst-case, where we have to move-construct the
  // unexpected value into temporary storage, then try to copy the T into place.
  // If the construction succeeds, then everything is fine, but if it throws,
  // then we move the old unexpected value back into place before rethrowing the
  // exception.
  template <class U = T,
            detail::enable_if_t<!std::is_nothrow_copy_constructible<U>::value &&
                                !std::is_nothrow_move_constructible<U>::value>
                * = nullptr>
  void assign(const expected_operations_base &rhs) {
    if (!this->m_has_val && rhs.m_has_val) {
      auto tmp = std::move(geterr());
      geterr().~unexpected<E>();

#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
      try {
        construct(rhs.get());
      } catch (...) {
        geterr() = std::move(tmp);
        throw;
      }
#else
      construct(rhs.get());
#endif
    } else {
      assign_common(rhs);
    }
  }

  // These overloads do the same as above, but for rvalues
  template <class U = T,
            detail::enable_if_t<std::is_nothrow_move_constructible<U>::value>
                * = nullptr>
  void assign(expected_operations_base &&rhs) noexcept {
    if (!this->m_has_val && rhs.m_has_val) {
      geterr().~unexpected<E>();
      construct(std::move(rhs).get());
    } else {
      assign_common(std::move(rhs));
    }
  }

  template <class U = T,
            detail::enable_if_t<!std::is_nothrow_move_constructible<U>::value>
                * = nullptr>
  void assign(expected_operations_base &&rhs) {
    if (!this->m_has_val && rhs.m_has_val) {
      auto tmp = std::move(geterr());
      geterr().~unexpected<E>();
#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
      try {
        construct(std::move(rhs).get());
      } catch (...) {
        geterr() = std::move(tmp);
        throw;
      }
#else
      construct(std::move(rhs).get());
#endif
    } else {
      assign_common(std::move(rhs));
    }
  }

#else

  // If exceptions are disabled then we can just copy-construct
  void assign(const expected_operations_base &rhs) noexcept {
    if (!this->m_has_val && rhs.m_has_val) {
      geterr().~unexpected<E>();
      construct(rhs.get());
    } else {
      assign_common(rhs);
    }
  }

  void assign(expected_operations_base &&rhs) noexcept {
    if (!this->m_has_val && rhs.m_has_val) {
      geterr().~unexpected<E>();
      construct(std::move(rhs).get());
    } else {
      assign_common(std::move(rhs));
    }
  }

#endif

  // The common part of move/copy assigning
  template <class Rhs>
  void assign_common(Rhs &&rhs) {
    if (this->m_has_val) {
      if (rhs.m_has_val) {
        get() = std::forward<Rhs>(rhs).get();
      } else {
        destroy_val();
        construct_error(std::forward<Rhs>(rhs).geterr());
      }
    } else {
      if (!rhs.m_has_val) {
        geterr() = std::forward<Rhs>(rhs).geterr();
      }
    }
  }

  bool has_value() const { return this->m_has_val; }

  TL_EXPECTED_11_CONSTEXPR T &get() & { return this->m_val; }
  constexpr const T &get() const & { return this->m_val; }
  TL_EXPECTED_11_CONSTEXPR T &&get() && { return std::move(this->m_val); }
#ifndef TL_EXPECTED_NO_CONSTRR
  constexpr const T &&get() const && { return std::move(this->m_val); }
#endif

  TL_EXPECTED_11_CONSTEXPR unexpected<E> &geterr() & {
    return this->m_unexpect;
  }
  constexpr const unexpected<E> &geterr() const & { return this->m_unexpect; }
  TL_EXPECTED_11_CONSTEXPR unexpected<E> &&geterr() && {
    return std::move(this->m_unexpect);
  }
#ifndef TL_EXPECTED_NO_CONSTRR
  constexpr const unexpected<E> &&geterr() const && {
    return std::move(this->m_unexpect);
  }
#endif

  TL_EXPECTED_11_CONSTEXPR void destroy_val() { get().~T(); }
};

// This base class provides some handy member functions which can be used in
// further derived classes
template <class E>
struct expected_operations_base<void, E> : expected_storage_base<void, E> {
  using expected_storage_base<void, E>::expected_storage_base;

  template <class... Args>
  void construct() noexcept {
    this->m_has_val = true;
  }

  // This function doesn't use its argument, but needs it so that code in
  // levels above this can work independently of whether T is void
  template <class Rhs>
  void construct_with(Rhs &&) noexcept {
    this->m_has_val = true;
  }

  template <class... Args>
  void construct_error(Args &&...args) noexcept {
    new (std::addressof(this->m_unexpect))
        unexpected<E>(std::forward<Args>(args)...);
    this->m_has_val = false;
  }

  template <class Rhs>
  void assign(Rhs &&rhs) noexcept {
    if (!this->m_has_val) {
      if (rhs.m_has_val) {
        geterr().~unexpected<E>();
        construct();
      } else {
        geterr() = std::forward<Rhs>(rhs).geterr();
      }
    } else {
      if (!rhs.m_has_val) {
        construct_error(std::forward<Rhs>(rhs).geterr());
      }
    }
  }

  bool has_value() const { return this->m_has_val; }

  TL_EXPECTED_11_CONSTEXPR unexpected<E> &geterr() & {
    return this->m_unexpect;
  }
  constexpr const unexpected<E> &geterr() const & { return this->m_unexpect; }
  TL_EXPECTED_11_CONSTEXPR unexpected<E> &&geterr() && {
    return std::move(this->m_unexpect);
  }
#ifndef TL_EXPECTED_NO_CONSTRR
  constexpr const unexpected<E> &&geterr() const && {
    return std::move(this->m_unexpect);
  }
#endif

  TL_EXPECTED_11_CONSTEXPR void destroy_val() {
    // no-op
  }
};

// This class manages conditionally having a trivial copy constructor
// This specialization is for when T and E are trivially copy constructible
template <class T, class E,
          bool = is_void_or<T, TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(
                                   T)>::value &&
                 TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(E)::value>
struct expected_copy_base : expected_operations_base<T, E> {
  using expected_operations_base<T, E>::expected_operations_base;
};

// This specialization is for when T or E are not trivially copy constructible
template <class T, class E>
struct expected_copy_base<T, E, false> : expected_operations_base<T, E> {
  using expected_operations_base<T, E>::expected_operations_base;

  expected_copy_base() = default;
  expected_copy_base(const expected_copy_base &rhs)
      : expected_operations_base<T, E>(no_init) {
    if (rhs.has_value()) {
      this->construct_with(rhs);
    } else {
      this->construct_error(rhs.geterr());
    }
  }

  expected_copy_base(expected_copy_base &&rhs) = default;
  expected_copy_base &operator=(const expected_copy_base &rhs) = default;
  expected_copy_base &operator=(expected_copy_base &&rhs) = default;
};

// This class manages conditionally having a trivial move constructor
// Unfortunately there's no way to achieve this in GCC < 5 AFAIK, since it
// doesn't implement an analogue to std::is_trivially_move_constructible. We
// have to make do with a non-trivial move constructor even if T is trivially
// move constructible
#ifndef TL_EXPECTED_GCC49
template <class T, class E,
          bool =
              is_void_or<T, std::is_trivially_move_constructible<T>>::value &&
              std::is_trivially_move_constructible<E>::value>
struct expected_move_base : expected_copy_base<T, E> {
  using expected_copy_base<T, E>::expected_copy_base;
};
#else
template <class T, class E, bool = false>
struct expected_move_base;
#endif
template <class T, class E>
struct expected_move_base<T, E, false> : expected_copy_base<T, E> {
  using expected_copy_base<T, E>::expected_copy_base;

  expected_move_base() = default;
  expected_move_base(const expected_move_base &rhs) = default;

  expected_move_base(expected_move_base &&rhs) noexcept(
      std::is_nothrow_move_constructible<T>::value)
      : expected_copy_base<T, E>(no_init) {
    if (rhs.has_value()) {
      this->construct_with(std::move(rhs));
    } else {
      this->construct_error(std::move(rhs.geterr()));
    }
  }
  expected_move_base &operator=(const expected_move_base &rhs) = default;
  expected_move_base &operator=(expected_move_base &&rhs) = default;
};

// This class manages conditionally having a trivial copy assignment operator
template <
    class T, class E,
    bool =
        is_void_or<
            T, conjunction<TL_EXPECTED_IS_TRIVIALLY_COPY_ASSIGNABLE(T),
                           TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(T),
                           TL_EXPECTED_IS_TRIVIALLY_DESTRUCTIBLE(T)>>::value &&
        TL_EXPECTED_IS_TRIVIALLY_COPY_ASSIGNABLE(E)::value &&
        TL_EXPECTED_IS_TRIVIALLY_COPY_CONSTRUCTIBLE(E)::value &&
        TL_EXPECTED_IS_TRIVIALLY_DESTRUCTIBLE(E)::value>
struct expected_copy_assign_base : expected_move_base<T, E> {
  using expected_move_base<T, E>::expected_move_base;
};

template <class T, class E>
struct expected_copy_assign_base<T, E, false> : expected_move_base<T, E> {
  using expected_move_base<T, E>::expected_move_base;

  expected_copy_assign_base() = default;
  expected_copy_assign_base(const expected_copy_assign_base &rhs) = default;

  expected_copy_assign_base(expected_copy_assign_base &&rhs) = default;
  expected_copy_assign_base &operator=(const expected_copy_assign_base &rhs) {
    this->assign(rhs);
    return *this;
  }
  expected_copy_assign_base &operator=(expected_copy_assign_base &&rhs) =
      default;
};

// This class manages conditionally having a trivial move assignment operator
// Unfortunately there's no way to achieve this in GCC < 5 AFAIK, since it
// doesn't implement an analogue to std::is_trivially_move_assignable. We have
// to make do with a non-trivial move assignment operator even if T is trivially
// move assignable
#ifndef TL_EXPECTED_GCC49
template <
    class T, class E,
    bool = is_void_or<
               T, conjunction<std::is_trivially_destructible<T>,
                              std::is_trivially_move_constructible<T>,
                              std::is_trivially_move_assignable<T>>>::value &&
           std::is_trivially_destructible<E>::value &&
           std::is_trivially_move_constructible<E>::value &&
           std::is_trivially_move_assignable<E>::value>
struct expected_move_assign_base : expected_copy_assign_base<T, E> {
  using expected_copy_assign_base<T, E>::expected_copy_assign_base;
};
#else
template <class T, class E, bool = false>
struct expected_move_assign_base;
#endif

template <class T, class E>
struct expected_move_assign_base<T, E, false>
    : expected_copy_assign_base<T, E> {
  using expected_copy_assign_base<T, E>::expected_copy_assign_base;

  expected_move_assign_base() = default;
  expected_move_assign_base(const expected_move_assign_base &rhs) = default;

  expected_move_assign_base(expected_move_assign_base &&rhs) = default;

  expected_move_assign_base &operator=(const expected_move_assign_base &rhs) =
      default;

  expected_move_assign_base &operator=(
      expected_move_assign_base
          &&rhs) noexcept(std::is_nothrow_move_constructible<T>::value &&
                          std::is_nothrow_move_assignable<T>::value) {
    this->assign(std::move(rhs));
    return *this;
  }
};

// expected_delete_ctor_base will conditionally delete copy and move
// constructors depending on whether T is copy/move constructible
template <class T, class E,
          bool EnableCopy = (is_copy_constructible_or_void<T>::value &&
                             std::is_copy_constructible<E>::value),
          bool EnableMove = (is_move_constructible_or_void<T>::value &&
                             std::is_move_constructible<E>::value)>
struct expected_delete_ctor_base {
  expected_delete_ctor_base() = default;
  expected_delete_ctor_base(const expected_delete_ctor_base &) = default;
  expected_delete_ctor_base(expected_delete_ctor_base &&) noexcept = default;
  expected_delete_ctor_base &operator=(const expected_delete_ctor_base &) =
      default;
  expected_delete_ctor_base &operator=(expected_delete_ctor_base &&) noexcept =
      default;
};

template <class T, class E>
struct expected_delete_ctor_base<T, E, true, false> {
  expected_delete_ctor_base() = default;
  expected_delete_ctor_base(const expected_delete_ctor_base &) = default;
  expected_delete_ctor_base(expected_delete_ctor_base &&) noexcept = delete;
  expected_delete_ctor_base &operator=(const expected_delete_ctor_base &) =
      default;
  expected_delete_ctor_base &operator=(expected_delete_ctor_base &&) noexcept =
      default;
};

template <class T, class E>
struct expected_delete_ctor_base<T, E, false, true> {
  expected_delete_ctor_base() = default;
  expected_delete_ctor_base(const expected_delete_ctor_base &) = delete;
  expected_delete_ctor_base(expected_delete_ctor_base &&) noexcept = default;
  expected_delete_ctor_base &operator=(const expected_delete_ctor_base &) =
      default;
  expected_delete_ctor_base &operator=(expected_delete_ctor_base &&) noexcept =
      default;
};

template <class T, class E>
struct expected_delete_ctor_base<T, E, false, false> {
  expected_delete_ctor_base() = default;
  expected_delete_ctor_base(const expected_delete_ctor_base &) = delete;
  expected_delete_ctor_base(expected_delete_ctor_base &&) noexcept = delete;
  expected_delete_ctor_base &operator=(const expected_delete_ctor_base &) =
      default;
  expected_delete_ctor_base &operator=(expected_delete_ctor_base &&) noexcept =
      default;
};

// expected_delete_assign_base will conditionally delete copy and move
// constructors depending on whether T and E are copy/move constructible +
// assignable
template <class T, class E,
          bool EnableCopy = (is_copy_constructible_or_void<T>::value &&
                             std::is_copy_constructible<E>::value &&
                             is_copy_assignable_or_void<T>::value &&
                             std::is_copy_assignable<E>::value),
          bool EnableMove = (is_move_constructible_or_void<T>::value &&
                             std::is_move_constructible<E>::value &&
                             is_move_assignable_or_void<T>::value &&
                             std::is_move_assignable<E>::value)>
struct expected_delete_assign_base {
  expected_delete_assign_base() = default;
  expected_delete_assign_base(const expected_delete_assign_base &) = default;
  expected_delete_assign_base(expected_delete_assign_base &&) noexcept =
      default;
  expected_delete_assign_base &operator=(const expected_delete_assign_base &) =
      default;
  expected_delete_assign_base &operator=(
      expected_delete_assign_base &&) noexcept = default;
};

template <class T, class E>
struct expected_delete_assign_base<T, E, true, false> {
  expected_delete_assign_base() = default;
  expected_delete_assign_base(const expected_delete_assign_base &) = default;
  expected_delete_assign_base(expected_delete_assign_base &&) noexcept =
      default;
  expected_delete_assign_base &operator=(const expected_delete_assign_base &) =
      default;
  expected_delete_assign_base &operator=(
      expected_delete_assign_base &&) noexcept = delete;
};

template <class T, class E>
struct expected_delete_assign_base<T, E, false, true> {
  expected_delete_assign_base() = default;
  expected_delete_assign_base(const expected_delete_assign_base &) = default;
  expected_delete_assign_base(expected_delete_assign_base &&) noexcept =
      default;
  expected_delete_assign_base &operator=(const expected_delete_assign_base &) =
      delete;
  expected_delete_assign_base &operator=(
      expected_delete_assign_base &&) noexcept = default;
};

template <class T, class E>
struct expected_delete_assign_base<T, E, false, false> {
  expected_delete_assign_base() = default;
  expected_delete_assign_base(const expected_delete_assign_base &) = default;
  expected_delete_assign_base(expected_delete_assign_base &&) noexcept =
      default;
  expected_delete_assign_base &operator=(const expected_delete_assign_base &) =
      delete;
  expected_delete_assign_base &operator=(
      expected_delete_assign_base &&) noexcept = delete;
};

// This is needed to be able to construct the expected_default_ctor_base which
// follows, while still conditionally deleting the default constructor.
struct default_constructor_tag {
  explicit constexpr default_constructor_tag() = default;
};

// expected_default_ctor_base will ensure that expected has a deleted default
// consturctor if T is not default constructible.
// This specialization is for when T is default constructible
template <class T, class E,
          bool Enable =
              std::is_default_constructible<T>::value || std::is_void<T>::value>
struct expected_default_ctor_base {
  constexpr expected_default_ctor_base() noexcept = default;
  constexpr expected_default_ctor_base(
      expected_default_ctor_base const &) noexcept = default;
  constexpr expected_default_ctor_base(expected_default_ctor_base &&) noexcept =
      default;
  expected_default_ctor_base &operator=(
      expected_default_ctor_base const &) noexcept = default;
  expected_default_ctor_base &operator=(
      expected_default_ctor_base &&) noexcept = default;

  constexpr explicit expected_default_ctor_base(default_constructor_tag) {}
};

// This specialization is for when T is not default constructible
template <class T, class E>
struct expected_default_ctor_base<T, E, false> {
  constexpr expected_default_ctor_base() noexcept = delete;
  constexpr expected_default_ctor_base(
      expected_default_ctor_base const &) noexcept = default;
  constexpr expected_default_ctor_base(expected_default_ctor_base &&) noexcept =
      default;
  expected_default_ctor_base &operator=(
      expected_default_ctor_base const &) noexcept = default;
  expected_default_ctor_base &operator=(
      expected_default_ctor_base &&) noexcept = default;

  constexpr explicit expected_default_ctor_base(default_constructor_tag) {}
};
}  // namespace detail

template <class E>
class bad_expected_access : public std::exception {
 public:
  explicit bad_expected_access(E e) : m_val(std::move(e)) {}

  virtual const char *what() const noexcept override {
    return "Bad expected access";
  }

  const E &error() const & { return m_val; }
  E &error() & { return m_val; }
  const E &&error() const && { return std::move(m_val); }
  E &&error() && { return std::move(m_val); }

 private:
  E m_val;
};

/// An `expected<T, E>` object is an object that contains the storage for
/// another object and manages the lifetime of this contained object `T`.
/// Alternatively it could contain the storage for another unexpected object
/// `E`. The contained object may not be initialized after the expected object
/// has been initialized, and may not be destroyed before the expected object
/// has been destroyed. The initialization state of the contained object is
/// tracked by the expected object.
template <class T, class E>
class expected : private detail::expected_move_assign_base<T, E>,
                 private detail::expected_delete_ctor_base<T, E>,
                 private detail::expected_delete_assign_base<T, E>,
                 private detail::expected_default_ctor_base<T, E> {
  static_assert(!std::is_reference<T>::value, "T must not be a reference");
  static_assert(!std::is_same<T, std::remove_cv<in_place_t>::type>::value,
                "T must not be in_place_t");
  static_assert(!std::is_same<T, std::remove_cv<unexpect_t>::type>::value,
                "T must not be unexpect_t");
  static_assert(
      !std::is_same<T, typename std::remove_cv<unexpected<E>>::type>::value,
      "T must not be unexpected<E>");
  static_assert(!std::is_reference<E>::value, "E must not be a reference");

  T *valptr() { return std::addressof(this->m_val); }
  const T *valptr() const { return std::addressof(this->m_val); }
  unexpected<E> *errptr() { return std::addressof(this->m_unexpect); }
  const unexpected<E> *errptr() const {
    return std::addressof(this->m_unexpect);
  }

  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR U &val() {
    return this->m_val;
  }
  TL_EXPECTED_11_CONSTEXPR unexpected<E> &err() { return this->m_unexpect; }

  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  constexpr const U &val() const {
    return this->m_val;
  }
  constexpr const unexpected<E> &err() const { return this->m_unexpect; }

  using impl_base = detail::expected_move_assign_base<T, E>;
  using ctor_base = detail::expected_default_ctor_base<T, E>;

 public:
  typedef T value_type;
  typedef E error_type;
  typedef unexpected<E> unexpected_type;

#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto and_then(F &&f) & {
    return and_then_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto and_then(F &&f) && {
    return and_then_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto and_then(F &&f) const & {
    return and_then_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr auto and_then(F &&f) const && {
    return and_then_impl(std::move(*this), std::forward<F>(f));
  }
#endif

#else
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto and_then(F &&f) & -> decltype(and_then_impl(
      std::declval<expected &>(), std::forward<F>(f))) {
    return and_then_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto and_then(F &&f) && -> decltype(and_then_impl(
      std::declval<expected &&>(), std::forward<F>(f))) {
    return and_then_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto and_then(F &&f) const & -> decltype(and_then_impl(
      std::declval<expected const &>(), std::forward<F>(f))) {
    return and_then_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr auto and_then(F &&f) const && -> decltype(and_then_impl(
      std::declval<expected const &&>(), std::forward<F>(f))) {
    return and_then_impl(std::move(*this), std::forward<F>(f));
  }
#endif
#endif

#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto map(F &&f) & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto map(F &&f) && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto map(F &&f) const & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  constexpr auto map(F &&f) const && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
#else
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(expected_map_impl(
      std::declval<expected &>(), std::declval<F &&>()))
  map(F &&f) & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(expected_map_impl(std::declval<expected>(),
                                                      std::declval<F &&>()))
  map(F &&f) && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr decltype(expected_map_impl(std::declval<const expected &>(),
                                       std::declval<F &&>()))
  map(F &&f) const & {
    return expected_map_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr decltype(expected_map_impl(std::declval<const expected &&>(),
                                       std::declval<F &&>()))
  map(F &&f) const && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
#endif
#endif

#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto transform(F &&f) & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto transform(F &&f) && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto transform(F &&f) const & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  constexpr auto transform(F &&f) const && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
#else
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(expected_map_impl(
      std::declval<expected &>(), std::declval<F &&>()))
  transform(F &&f) & {
    return expected_map_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(expected_map_impl(std::declval<expected>(),
                                                      std::declval<F &&>()))
  transform(F &&f) && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr decltype(expected_map_impl(std::declval<const expected &>(),
                                       std::declval<F &&>()))
  transform(F &&f) const & {
    return expected_map_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr decltype(expected_map_impl(std::declval<const expected &&>(),
                                       std::declval<F &&>()))
  transform(F &&f) const && {
    return expected_map_impl(std::move(*this), std::forward<F>(f));
  }
#endif
#endif

#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto map_error(F &&f) & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto map_error(F &&f) && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto map_error(F &&f) const & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  constexpr auto map_error(F &&f) const && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
#else
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(map_error_impl(std::declval<expected &>(),
                                                   std::declval<F &&>()))
  map_error(F &&f) & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(map_error_impl(std::declval<expected &&>(),
                                                   std::declval<F &&>()))
  map_error(F &&f) && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr decltype(map_error_impl(std::declval<const expected &>(),
                                    std::declval<F &&>()))
  map_error(F &&f) const & {
    return map_error_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr decltype(map_error_impl(std::declval<const expected &&>(),
                                    std::declval<F &&>()))
  map_error(F &&f) const && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
#endif
#endif
#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto transform_error(F &&f) & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR auto transform_error(F &&f) && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr auto transform_error(F &&f) const & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  constexpr auto transform_error(F &&f) const && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
#else
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(map_error_impl(std::declval<expected &>(),
                                                   std::declval<F &&>()))
  transform_error(F &&f) & {
    return map_error_impl(*this, std::forward<F>(f));
  }
  template <class F>
  TL_EXPECTED_11_CONSTEXPR decltype(map_error_impl(std::declval<expected &&>(),
                                                   std::declval<F &&>()))
  transform_error(F &&f) && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
  template <class F>
  constexpr decltype(map_error_impl(std::declval<const expected &>(),
                                    std::declval<F &&>()))
  transform_error(F &&f) const & {
    return map_error_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  constexpr decltype(map_error_impl(std::declval<const expected &&>(),
                                    std::declval<F &&>()))
  transform_error(F &&f) const && {
    return map_error_impl(std::move(*this), std::forward<F>(f));
  }
#endif
#endif
  template <class F>
  expected TL_EXPECTED_11_CONSTEXPR or_else(F &&f) & {
    return or_else_impl(*this, std::forward<F>(f));
  }

  template <class F>
  expected TL_EXPECTED_11_CONSTEXPR or_else(F &&f) && {
    return or_else_impl(std::move(*this), std::forward<F>(f));
  }

  template <class F>
  expected constexpr or_else(F &&f) const & {
    return or_else_impl(*this, std::forward<F>(f));
  }

#ifndef TL_EXPECTED_NO_CONSTRR
  template <class F>
  expected constexpr or_else(F &&f) const && {
    return or_else_impl(std::move(*this), std::forward<F>(f));
  }
#endif
  constexpr expected() = default;
  constexpr expected(const expected &rhs) = default;
  constexpr expected(expected &&rhs) = default;
  expected &operator=(const expected &rhs) = default;
  expected &operator=(expected &&rhs) = default;

  template <class... Args,
            detail::enable_if_t<std::is_constructible<T, Args &&...>::value> * =
                nullptr>
  constexpr expected(in_place_t, Args &&...args)
      : impl_base(in_place, std::forward<Args>(args)...),
        ctor_base(detail::default_constructor_tag{}) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr expected(in_place_t, std::initializer_list<U> il, Args &&...args)
      : impl_base(in_place, il, std::forward<Args>(args)...),
        ctor_base(detail::default_constructor_tag{}) {}

  template <class G = E,
            detail::enable_if_t<std::is_constructible<E, const G &>::value> * =
                nullptr,
            detail::enable_if_t<!std::is_convertible<const G &, E>::value> * =
                nullptr>
  explicit constexpr expected(const unexpected<G> &e)
      : impl_base(unexpect, e.value()),
        ctor_base(detail::default_constructor_tag{}) {}

  template <
      class G = E,
      detail::enable_if_t<std::is_constructible<E, const G &>::value> * =
          nullptr,
      detail::enable_if_t<std::is_convertible<const G &, E>::value> * = nullptr>
  constexpr expected(unexpected<G> const &e)
      : impl_base(unexpect, e.value()),
        ctor_base(detail::default_constructor_tag{}) {}

  template <
      class G = E,
      detail::enable_if_t<std::is_constructible<E, G &&>::value> * = nullptr,
      detail::enable_if_t<!std::is_convertible<G &&, E>::value> * = nullptr>
  explicit constexpr expected(unexpected<G> &&e) noexcept(
      std::is_nothrow_constructible<E, G &&>::value)
      : impl_base(unexpect, std::move(e.value())),
        ctor_base(detail::default_constructor_tag{}) {}

  template <
      class G = E,
      detail::enable_if_t<std::is_constructible<E, G &&>::value> * = nullptr,
      detail::enable_if_t<std::is_convertible<G &&, E>::value> * = nullptr>
  constexpr expected(unexpected<G> &&e) noexcept(
      std::is_nothrow_constructible<E, G &&>::value)
      : impl_base(unexpect, std::move(e.value())),
        ctor_base(detail::default_constructor_tag{}) {}

  template <class... Args,
            detail::enable_if_t<std::is_constructible<E, Args &&...>::value> * =
                nullptr>
  constexpr explicit expected(unexpect_t, Args &&...args)
      : impl_base(unexpect, std::forward<Args>(args)...),
        ctor_base(detail::default_constructor_tag{}) {}

  template <class U, class... Args,
            detail::enable_if_t<std::is_constructible<
                E, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  constexpr explicit expected(unexpect_t, std::initializer_list<U> il,
                              Args &&...args)
      : impl_base(unexpect, il, std::forward<Args>(args)...),
        ctor_base(detail::default_constructor_tag{}) {}

  template <class U, class G,
            detail::enable_if_t<!(std::is_convertible<U const &, T>::value &&
                                  std::is_convertible<G const &, E>::value)> * =
                nullptr,
            detail::expected_enable_from_other<T, E, U, G, const U &, const G &>
                * = nullptr>
  explicit TL_EXPECTED_11_CONSTEXPR expected(const expected<U, G> &rhs)
      : ctor_base(detail::default_constructor_tag{}) {
    if (rhs.has_value()) {
      this->construct(*rhs);
    } else {
      this->construct_error(rhs.error());
    }
  }

  template <class U, class G,
            detail::enable_if_t<(std::is_convertible<U const &, T>::value &&
                                 std::is_convertible<G const &, E>::value)> * =
                nullptr,
            detail::expected_enable_from_other<T, E, U, G, const U &, const G &>
                * = nullptr>
  TL_EXPECTED_11_CONSTEXPR expected(const expected<U, G> &rhs)
      : ctor_base(detail::default_constructor_tag{}) {
    if (rhs.has_value()) {
      this->construct(*rhs);
    } else {
      this->construct_error(rhs.error());
    }
  }

  template <
      class U, class G,
      detail::enable_if_t<!(std::is_convertible<U &&, T>::value &&
                            std::is_convertible<G &&, E>::value)> * = nullptr,
      detail::expected_enable_from_other<T, E, U, G, U &&, G &&> * = nullptr>
  explicit TL_EXPECTED_11_CONSTEXPR expected(expected<U, G> &&rhs)
      : ctor_base(detail::default_constructor_tag{}) {
    if (rhs.has_value()) {
      this->construct(std::move(*rhs));
    } else {
      this->construct_error(std::move(rhs.error()));
    }
  }

  template <
      class U, class G,
      detail::enable_if_t<(std::is_convertible<U &&, T>::value &&
                           std::is_convertible<G &&, E>::value)> * = nullptr,
      detail::expected_enable_from_other<T, E, U, G, U &&, G &&> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR expected(expected<U, G> &&rhs)
      : ctor_base(detail::default_constructor_tag{}) {
    if (rhs.has_value()) {
      this->construct(std::move(*rhs));
    } else {
      this->construct_error(std::move(rhs.error()));
    }
  }

  template <
      class U = T,
      detail::enable_if_t<!std::is_convertible<U &&, T>::value> * = nullptr,
      detail::expected_enable_forward_value<T, E, U> * = nullptr>
  explicit TL_EXPECTED_MSVC2015_CONSTEXPR expected(U &&v)
      : expected(in_place, std::forward<U>(v)) {}

  template <
      class U = T,
      detail::enable_if_t<std::is_convertible<U &&, T>::value> * = nullptr,
      detail::expected_enable_forward_value<T, E, U> * = nullptr>
  TL_EXPECTED_MSVC2015_CONSTEXPR expected(U &&v)
      : expected(in_place, std::forward<U>(v)) {}

  template <
      class U = T, class G = T,
      detail::enable_if_t<std::is_nothrow_constructible<T, U &&>::value> * =
          nullptr,
      detail::enable_if_t<!std::is_void<G>::value> * = nullptr,
      detail::enable_if_t<
          (!std::is_same<expected<T, E>, detail::decay_t<U>>::value &&
           !detail::conjunction<std::is_scalar<T>,
                                std::is_same<T, detail::decay_t<U>>>::value &&
           std::is_constructible<T, U>::value &&
           std::is_assignable<G &, U>::value &&
           std::is_nothrow_move_constructible<E>::value)> * = nullptr>
  expected &operator=(U &&v) {
    if (has_value()) {
      val() = std::forward<U>(v);
    } else {
      err().~unexpected<E>();
      ::new (valptr()) T(std::forward<U>(v));
      this->m_has_val = true;
    }

    return *this;
  }

  template <
      class U = T, class G = T,
      detail::enable_if_t<!std::is_nothrow_constructible<T, U &&>::value> * =
          nullptr,
      detail::enable_if_t<!std::is_void<U>::value> * = nullptr,
      detail::enable_if_t<
          (!std::is_same<expected<T, E>, detail::decay_t<U>>::value &&
           !detail::conjunction<std::is_scalar<T>,
                                std::is_same<T, detail::decay_t<U>>>::value &&
           std::is_constructible<T, U>::value &&
           std::is_assignable<G &, U>::value &&
           std::is_nothrow_move_constructible<E>::value)> * = nullptr>
  expected &operator=(U &&v) {
    if (has_value()) {
      val() = std::forward<U>(v);
    } else {
      auto tmp = std::move(err());
      err().~unexpected<E>();

#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
      try {
        ::new (valptr()) T(std::forward<U>(v));
        this->m_has_val = true;
      } catch (...) {
        err() = std::move(tmp);
        throw;
      }
#else
      ::new (valptr()) T(std::forward<U>(v));
      this->m_has_val = true;
#endif
    }

    return *this;
  }

  template <class G = E,
            detail::enable_if_t<std::is_nothrow_copy_constructible<G>::value &&
                                std::is_assignable<G &, G>::value> * = nullptr>
  expected &operator=(const unexpected<G> &rhs) {
    if (!has_value()) {
      err() = rhs;
    } else {
      this->destroy_val();
      ::new (errptr()) unexpected<E>(rhs);
      this->m_has_val = false;
    }

    return *this;
  }

  template <class G = E,
            detail::enable_if_t<std::is_nothrow_move_constructible<G>::value &&
                                std::is_move_assignable<G>::value> * = nullptr>
  expected &operator=(unexpected<G> &&rhs) noexcept {
    if (!has_value()) {
      err() = std::move(rhs);
    } else {
      this->destroy_val();
      ::new (errptr()) unexpected<E>(std::move(rhs));
      this->m_has_val = false;
    }

    return *this;
  }

  template <class... Args, detail::enable_if_t<std::is_nothrow_constructible<
                               T, Args &&...>::value> * = nullptr>
  void emplace(Args &&...args) {
    if (has_value()) {
      val().~T();
    } else {
      err().~unexpected<E>();
      this->m_has_val = true;
    }
    ::new (valptr()) T(std::forward<Args>(args)...);
  }

  template <class... Args, detail::enable_if_t<!std::is_nothrow_constructible<
                               T, Args &&...>::value> * = nullptr>
  void emplace(Args &&...args) {
    if (has_value()) {
      val().~T();
      ::new (valptr()) T(std::forward<Args>(args)...);
    } else {
      auto tmp = std::move(err());
      err().~unexpected<E>();

#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
      try {
        ::new (valptr()) T(std::forward<Args>(args)...);
        this->m_has_val = true;
      } catch (...) {
        err() = std::move(tmp);
        throw;
      }
#else
      ::new (valptr()) T(std::forward<Args>(args)...);
      this->m_has_val = true;
#endif
    }
  }

  template <class U, class... Args,
            detail::enable_if_t<std::is_nothrow_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  void emplace(std::initializer_list<U> il, Args &&...args) {
    if (has_value()) {
      T t(il, std::forward<Args>(args)...);
      val() = std::move(t);
    } else {
      err().~unexpected<E>();
      ::new (valptr()) T(il, std::forward<Args>(args)...);
      this->m_has_val = true;
    }
  }

  template <class U, class... Args,
            detail::enable_if_t<!std::is_nothrow_constructible<
                T, std::initializer_list<U> &, Args &&...>::value> * = nullptr>
  void emplace(std::initializer_list<U> il, Args &&...args) {
    if (has_value()) {
      T t(il, std::forward<Args>(args)...);
      val() = std::move(t);
    } else {
      auto tmp = std::move(err());
      err().~unexpected<E>();

#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
      try {
        ::new (valptr()) T(il, std::forward<Args>(args)...);
        this->m_has_val = true;
      } catch (...) {
        err() = std::move(tmp);
        throw;
      }
#else
      ::new (valptr()) T(il, std::forward<Args>(args)...);
      this->m_has_val = true;
#endif
    }
  }

 private:
  using t_is_void = std::true_type;
  using t_is_not_void = std::false_type;
  using t_is_nothrow_move_constructible = std::true_type;
  using move_constructing_t_can_throw = std::false_type;
  using e_is_nothrow_move_constructible = std::true_type;
  using move_constructing_e_can_throw = std::false_type;

  void swap_where_both_have_value(expected & /*rhs*/, t_is_void) noexcept {
    // swapping void is a no-op
  }

  void swap_where_both_have_value(expected &rhs, t_is_not_void) {
    using std::swap;
    swap(val(), rhs.val());
  }

  void swap_where_only_one_has_value(expected &rhs, t_is_void) noexcept(
      std::is_nothrow_move_constructible<E>::value) {
    ::new (errptr()) unexpected_type(std::move(rhs.err()));
    rhs.err().~unexpected_type();
    std::swap(this->m_has_val, rhs.m_has_val);
  }

  void swap_where_only_one_has_value(expected &rhs, t_is_not_void) {
    swap_where_only_one_has_value_and_t_is_not_void(
        rhs, typename std::is_nothrow_move_constructible<T>::type{},
        typename std::is_nothrow_move_constructible<E>::type{});
  }

  void swap_where_only_one_has_value_and_t_is_not_void(
      expected &rhs, t_is_nothrow_move_constructible,
      e_is_nothrow_move_constructible) noexcept {
    auto temp = std::move(val());
    val().~T();
    ::new (errptr()) unexpected_type(std::move(rhs.err()));
    rhs.err().~unexpected_type();
    ::new (rhs.valptr()) T(std::move(temp));
    std::swap(this->m_has_val, rhs.m_has_val);
  }

  void swap_where_only_one_has_value_and_t_is_not_void(
      expected &rhs, t_is_nothrow_move_constructible,
      move_constructing_e_can_throw) {
    auto temp = std::move(val());
    val().~T();
#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
    try {
      ::new (errptr()) unexpected_type(std::move(rhs.err()));
      rhs.err().~unexpected_type();
      ::new (rhs.valptr()) T(std::move(temp));
      std::swap(this->m_has_val, rhs.m_has_val);
    } catch (...) {
      val() = std::move(temp);
      throw;
    }
#else
    ::new (errptr()) unexpected_type(std::move(rhs.err()));
    rhs.err().~unexpected_type();
    ::new (rhs.valptr()) T(std::move(temp));
    std::swap(this->m_has_val, rhs.m_has_val);
#endif
  }

  void swap_where_only_one_has_value_and_t_is_not_void(
      expected &rhs, move_constructing_t_can_throw,
      e_is_nothrow_move_constructible) {
    auto temp = std::move(rhs.err());
    rhs.err().~unexpected_type();
#ifdef TL_EXPECTED_EXCEPTIONS_ENABLED
    try {
      ::new (rhs.valptr()) T(std::move(val()));
      val().~T();
      ::new (errptr()) unexpected_type(std::move(temp));
      std::swap(this->m_has_val, rhs.m_has_val);
    } catch (...) {
      rhs.err() = std::move(temp);
      throw;
    }
#else
    ::new (rhs.valptr()) T(std::move(val()));
    val().~T();
    ::new (errptr()) unexpected_type(std::move(temp));
    std::swap(this->m_has_val, rhs.m_has_val);
#endif
  }

 public:
  template <class OT = T, class OE = E>
  detail::enable_if_t<detail::is_swappable<OT>::value &&
                      detail::is_swappable<OE>::value &&
                      (std::is_nothrow_move_constructible<OT>::value ||
                       std::is_nothrow_move_constructible<OE>::value)>
  swap(expected &rhs) noexcept(std::is_nothrow_move_constructible<T>::value &&
                               detail::is_nothrow_swappable<T>::value &&
                               std::is_nothrow_move_constructible<E>::value &&
                               detail::is_nothrow_swappable<E>::value) {
    if (has_value() && rhs.has_value()) {
      swap_where_both_have_value(rhs, typename std::is_void<T>::type{});
    } else if (!has_value() && rhs.has_value()) {
      rhs.swap(*this);
    } else if (has_value()) {
      swap_where_only_one_has_value(rhs, typename std::is_void<T>::type{});
    } else {
      using std::swap;
      swap(err(), rhs.err());
    }
  }

  constexpr const T *operator->() const {
    TL_ASSERT(has_value());
    return valptr();
  }
  TL_EXPECTED_11_CONSTEXPR T *operator->() {
    TL_ASSERT(has_value());
    return valptr();
  }

  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  constexpr const U &operator*() const & {
    TL_ASSERT(has_value());
    return val();
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR U &operator*() & {
    TL_ASSERT(has_value());
    return val();
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  constexpr const U &&operator*() const && {
    TL_ASSERT(has_value());
    return std::move(val());
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR U &&operator*() && {
    TL_ASSERT(has_value());
    return std::move(val());
  }

  constexpr bool has_value() const noexcept { return this->m_has_val; }
  constexpr explicit operator bool() const noexcept { return this->m_has_val; }

  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR const U &value() const & {
    if (!has_value())
      detail::throw_exception(bad_expected_access<E>(err().value()));
    return val();
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR U &value() & {
    if (!has_value())
      detail::throw_exception(bad_expected_access<E>(err().value()));
    return val();
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR const U &&value() const && {
    if (!has_value())
      detail::throw_exception(bad_expected_access<E>(std::move(err()).value()));
    return std::move(val());
  }
  template <class U = T,
            detail::enable_if_t<!std::is_void<U>::value> * = nullptr>
  TL_EXPECTED_11_CONSTEXPR U &&value() && {
    if (!has_value())
      detail::throw_exception(bad_expected_access<E>(std::move(err()).value()));
    return std::move(val());
  }

  constexpr const E &error() const & {
    TL_ASSERT(!has_value());
    return err().value();
  }
  TL_EXPECTED_11_CONSTEXPR E &error() & {
    TL_ASSERT(!has_value());
    return err().value();
  }
  constexpr const E &&error() const && {
    TL_ASSERT(!has_value());
    return std::move(err().value());
  }
  TL_EXPECTED_11_CONSTEXPR E &&error() && {
    TL_ASSERT(!has_value());
    return std::move(err().value());
  }

  template <class U>
  constexpr T value_or(U &&v) const & {
    static_assert(std::is_copy_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be copy-constructible and convertible to from U&&");
    return bool(*this) ? **this : static_cast<T>(std::forward<U>(v));
  }
  template <class U>
  TL_EXPECTED_11_CONSTEXPR T value_or(U &&v) && {
    static_assert(std::is_move_constructible<T>::value &&
                      std::is_convertible<U &&, T>::value,
                  "T must be move-constructible and convertible to from U&&");
    return bool(*this) ? std::move(**this) : static_cast<T>(std::forward<U>(v));
  }
};

namespace detail {
template <class Exp>
using exp_t = typename detail::decay_t<Exp>::value_type;
template <class Exp>
using err_t = typename detail::decay_t<Exp>::error_type;
template <class Exp, class Ret>
using ret_t = expected<Ret, err_t<Exp>>;

#ifdef TL_EXPECTED_CXX14
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>()))>
constexpr auto and_then_impl(Exp &&exp, F &&f) {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");

  return exp.has_value()
             ? detail::invoke(std::forward<F>(f), *std::forward<Exp>(exp))
             : Ret(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>()))>
constexpr auto and_then_impl(Exp &&exp, F &&f) {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");

  return exp.has_value() ? detail::invoke(std::forward<F>(f))
                         : Ret(unexpect, std::forward<Exp>(exp).error());
}
#else
template <class>
struct TC;
template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>())),
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr>
auto and_then_impl(Exp &&exp, F &&f) -> Ret {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");

  return exp.has_value()
             ? detail::invoke(std::forward<F>(f), *std::forward<Exp>(exp))
             : Ret(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>())),
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr>
constexpr auto and_then_impl(Exp &&exp, F &&f) -> Ret {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");

  return exp.has_value() ? detail::invoke(std::forward<F>(f))
                         : Ret(unexpect, std::forward<Exp>(exp).error());
}
#endif

#ifdef TL_EXPECTED_CXX14
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto expected_map_impl(Exp &&exp, F &&f) {
  using result = ret_t<Exp, detail::decay_t<Ret>>;
  return exp.has_value() ? result(detail::invoke(std::forward<F>(f),
                                                 *std::forward<Exp>(exp)))
                         : result(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto expected_map_impl(Exp &&exp, F &&f) {
  using result = expected<void, err_t<Exp>>;
  if (exp.has_value()) {
    detail::invoke(std::forward<F>(f), *std::forward<Exp>(exp));
    return result();
  }

  return result(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto expected_map_impl(Exp &&exp, F &&f) {
  using result = ret_t<Exp, detail::decay_t<Ret>>;
  return exp.has_value() ? result(detail::invoke(std::forward<F>(f)))
                         : result(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto expected_map_impl(Exp &&exp, F &&f) {
  using result = expected<void, err_t<Exp>>;
  if (exp.has_value()) {
    detail::invoke(std::forward<F>(f));
    return result();
  }

  return result(unexpect, std::forward<Exp>(exp).error());
}
#else
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>

constexpr auto expected_map_impl(Exp &&exp, F &&f)
    -> ret_t<Exp, detail::decay_t<Ret>> {
  using result = ret_t<Exp, detail::decay_t<Ret>>;

  return exp.has_value() ? result(detail::invoke(std::forward<F>(f),
                                                 *std::forward<Exp>(exp)))
                         : result(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              *std::declval<Exp>())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>

auto expected_map_impl(Exp &&exp, F &&f) -> expected<void, err_t<Exp>> {
  if (exp.has_value()) {
    detail::invoke(std::forward<F>(f), *std::forward<Exp>(exp));
    return {};
  }

  return unexpected<err_t<Exp>>(std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>

constexpr auto expected_map_impl(Exp &&exp, F &&f)
    -> ret_t<Exp, detail::decay_t<Ret>> {
  using result = ret_t<Exp, detail::decay_t<Ret>>;

  return exp.has_value() ? result(detail::invoke(std::forward<F>(f)))
                         : result(unexpect, std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>

auto expected_map_impl(Exp &&exp, F &&f) -> expected<void, err_t<Exp>> {
  if (exp.has_value()) {
    detail::invoke(std::forward<F>(f));
    return {};
  }

  return unexpected<err_t<Exp>>(std::forward<Exp>(exp).error());
}
#endif

#if defined(TL_EXPECTED_CXX14) && !defined(TL_EXPECTED_GCC49) && \
    !defined(TL_EXPECTED_GCC54) && !defined(TL_EXPECTED_GCC55)
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto map_error_impl(Exp &&exp, F &&f) {
  using result = expected<exp_t<Exp>, detail::decay_t<Ret>>;
  return exp.has_value()
             ? result(*std::forward<Exp>(exp))
             : result(unexpect, detail::invoke(std::forward<F>(f),
                                               std::forward<Exp>(exp).error()));
}
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto map_error_impl(Exp &&exp, F &&f) {
  using result = expected<exp_t<Exp>, monostate>;
  if (exp.has_value()) {
    return result(*std::forward<Exp>(exp));
  }

  detail::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  return result(unexpect, monostate{});
}
template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto map_error_impl(Exp &&exp, F &&f) {
  using result = expected<exp_t<Exp>, detail::decay_t<Ret>>;
  return exp.has_value()
             ? result()
             : result(unexpect, detail::invoke(std::forward<F>(f),
                                               std::forward<Exp>(exp).error()));
}
template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto map_error_impl(Exp &&exp, F &&f) {
  using result = expected<exp_t<Exp>, monostate>;
  if (exp.has_value()) {
    return result();
  }

  detail::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  return result(unexpect, monostate{});
}
#else
template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto map_error_impl(Exp &&exp, F &&f)
    -> expected<exp_t<Exp>, detail::decay_t<Ret>> {
  using result = expected<exp_t<Exp>, detail::decay_t<Ret>>;

  return exp.has_value()
             ? result(*std::forward<Exp>(exp))
             : result(unexpect, detail::invoke(std::forward<F>(f),
                                               std::forward<Exp>(exp).error()));
}

template <class Exp, class F,
          detail::enable_if_t<!std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto map_error_impl(Exp &&exp, F &&f) -> expected<exp_t<Exp>, monostate> {
  using result = expected<exp_t<Exp>, monostate>;
  if (exp.has_value()) {
    return result(*std::forward<Exp>(exp));
  }

  detail::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  return result(unexpect, monostate{});
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto map_error_impl(Exp &&exp, F &&f)
    -> expected<exp_t<Exp>, detail::decay_t<Ret>> {
  using result = expected<exp_t<Exp>, detail::decay_t<Ret>>;

  return exp.has_value()
             ? result()
             : result(unexpect, detail::invoke(std::forward<F>(f),
                                               std::forward<Exp>(exp).error()));
}

template <class Exp, class F,
          detail::enable_if_t<std::is_void<exp_t<Exp>>::value> * = nullptr,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
auto map_error_impl(Exp &&exp, F &&f) -> expected<exp_t<Exp>, monostate> {
  using result = expected<exp_t<Exp>, monostate>;
  if (exp.has_value()) {
    return result();
  }

  detail::invoke(std::forward<F>(f), std::forward<Exp>(exp).error());
  return result(unexpect, monostate{});
}
#endif

#ifdef TL_EXPECTED_CXX14
template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
constexpr auto or_else_impl(Exp &&exp, F &&f) {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");
  return exp.has_value() ? std::forward<Exp>(exp)
                         : detail::invoke(std::forward<F>(f),
                                          std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
detail::decay_t<Exp> or_else_impl(Exp &&exp, F &&f) {
  return exp.has_value() ? std::forward<Exp>(exp)
                         : (detail::invoke(std::forward<F>(f),
                                           std::forward<Exp>(exp).error()),
                            std::forward<Exp>(exp));
}
#else
template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<!std::is_void<Ret>::value> * = nullptr>
auto or_else_impl(Exp &&exp, F &&f) -> Ret {
  static_assert(detail::is_expected<Ret>::value, "F must return an expected");
  return exp.has_value() ? std::forward<Exp>(exp)
                         : detail::invoke(std::forward<F>(f),
                                          std::forward<Exp>(exp).error());
}

template <class Exp, class F,
          class Ret = decltype(detail::invoke(std::declval<F>(),
                                              std::declval<Exp>().error())),
          detail::enable_if_t<std::is_void<Ret>::value> * = nullptr>
detail::decay_t<Exp> or_else_impl(Exp &&exp, F &&f) {
  return exp.has_value() ? std::forward<Exp>(exp)
                         : (detail::invoke(std::forward<F>(f),
                                           std::forward<Exp>(exp).error()),
                            std::forward<Exp>(exp));
}
#endif
}  // namespace detail

template <class T, class E, class U, class F>
constexpr bool operator==(const expected<T, E> &lhs,
                          const expected<U, F> &rhs) {
  return (lhs.has_value() != rhs.has_value())
             ? false
             : (!lhs.has_value() ? lhs.error() == rhs.error() : *lhs == *rhs);
}
template <class T, class E, class U, class F>
constexpr bool operator!=(const expected<T, E> &lhs,
                          const expected<U, F> &rhs) {
  return (lhs.has_value() != rhs.has_value())
             ? true
             : (!lhs.has_value() ? lhs.error() != rhs.error() : *lhs != *rhs);
}
template <class E, class F>
constexpr bool operator==(const expected<void, E> &lhs,
                          const expected<void, F> &rhs) {
  return (lhs.has_value() != rhs.has_value())
             ? false
             : (!lhs.has_value() ? lhs.error() == rhs.error() : true);
}
template <class E, class F>
constexpr bool operator!=(const expected<void, E> &lhs,
                          const expected<void, F> &rhs) {
  return (lhs.has_value() != rhs.has_value())
             ? true
             : (!lhs.has_value() ? lhs.error() == rhs.error() : false);
}

template <class T, class E, class U>
constexpr bool operator==(const expected<T, E> &x, const U &v) {
  return x.has_value() ? *x == v : false;
}
template <class T, class E, class U>
constexpr bool operator==(const U &v, const expected<T, E> &x) {
  return x.has_value() ? *x == v : false;
}
template <class T, class E, class U>
constexpr bool operator!=(const expected<T, E> &x, const U &v) {
  return x.has_value() ? *x != v : true;
}
template <class T, class E, class U>
constexpr bool operator!=(const U &v, const expected<T, E> &x) {
  return x.has_value() ? *x != v : true;
}

template <class T, class E>
constexpr bool operator==(const expected<T, E> &x, const unexpected<E> &e) {
  return x.has_value() ? false : x.error() == e.value();
}
template <class T, class E>
constexpr bool operator==(const unexpected<E> &e, const expected<T, E> &x) {
  return x.has_value() ? false : x.error() == e.value();
}
template <class T, class E>
constexpr bool operator!=(const expected<T, E> &x, const unexpected<E> &e) {
  return x.has_value() ? true : x.error() != e.value();
}
template <class T, class E>
constexpr bool operator!=(const unexpected<E> &e, const expected<T, E> &x) {
  return x.has_value() ? true : x.error() != e.value();
}

template <class T, class E,
          detail::enable_if_t<(std::is_void<T>::value ||
                               std::is_move_constructible<T>::value) &&
                              detail::is_swappable<T>::value &&
                              std::is_move_constructible<E>::value &&
                              detail::is_swappable<E>::value> * = nullptr>
void swap(expected<T, E> &lhs,
          expected<T, E> &rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}
}  // namespace tl

#endif
/* end file include/ada/expected.h */
/* begin file include/ada/url_pattern_regex.h */
/**
 * @file url_search_params.h
 * @brief Declaration for the URL Search Params
 */
#ifndef ADA_URL_PATTERN_REGEX_H
#define ADA_URL_PATTERN_REGEX_H

#include <string>
#include <string_view>

#ifdef ADA_USE_UNSAFE_STD_REGEX_PROVIDER
#include <regex>
#endif  // ADA_USE_UNSAFE_STD_REGEX_PROVIDER

namespace ada::url_pattern_regex {

template <typename T>
concept regex_concept = requires(T t, std::string_view pattern,
                                 bool ignore_case, std::string_view input) {
  // Ensure the class has a type alias 'regex_type'
  typename T::regex_type;

  // Function to create a regex instance
  {
    T::create_instance(pattern, ignore_case)
  } -> std::same_as<std::optional<typename T::regex_type>>;

  // Function to perform regex search
  {
    T::regex_search(input, std::declval<typename T::regex_type&>())
  } -> std::same_as<std::optional<std::vector<std::optional<std::string>>>>;

  // Function to match regex pattern
  {
    T::regex_match(input, std::declval<typename T::regex_type&>())
  } -> std::same_as<bool>;

  // Copy constructor
  { T(std::declval<const T&>()) } -> std::same_as<T>;

  // Move constructor
  { T(std::declval<T&&>()) } -> std::same_as<T>;
};

#ifdef ADA_USE_UNSAFE_STD_REGEX_PROVIDER
class std_regex_provider final {
 public:
  std_regex_provider() = default;
  using regex_type = std::regex;
  static std::optional<regex_type> create_instance(std::string_view pattern,
                                                   bool ignore_case);
  static std::optional<std::vector<std::optional<std::string>>> regex_search(
      std::string_view input, const regex_type& pattern);
  static bool regex_match(std::string_view input, const regex_type& pattern);
};
#endif  // ADA_USE_UNSAFE_STD_REGEX_PROVIDER

}  // namespace ada::url_pattern_regex

#endif  // ADA_URL_PATTERN_REGEX_H
/* end file include/ada/url_pattern_regex.h */
/* begin file include/ada/url_pattern_init.h */
/**
 * @file url_pattern_init.h
 * @brief Declaration for the url_pattern_init implementation.
 */
#ifndef ADA_URL_PATTERN_INIT_H
#define ADA_URL_PATTERN_INIT_H

/* begin file include/ada/errors.h */
/**
 * @file errors.h
 * @brief Definitions for the errors.
 */
#ifndef ADA_ERRORS_H
#define ADA_ERRORS_H

#include <cstdint>
namespace ada {
enum class errors : uint8_t { type_error };
}  // namespace ada
#endif  // ADA_ERRORS_H
/* end file include/ada/errors.h */

#include <string_view>
#include <string>
#include <optional>

#if ADA_TESTING
#include <iostream>
#endif  // ADA_TESTING

namespace ada {

// Important: C++20 allows us to use concept rather than `using` or `typedef
// and allows functions with second argument, which is optional (using either
// std::nullopt or a parameter with default value)
template <typename F>
concept url_pattern_encoding_callback = requires(F f, std::string_view sv) {
  { f(sv) } -> std::same_as<tl::expected<std::string, errors>>;
};

// A structure providing matching patterns for individual components
// of a URL. When a URLPattern is created, or when a URLPattern is
// used to match or test against a URL, the input can be given as
// either a string or a URLPatternInit struct. If a string is given,
// it will be parsed to create a URLPatternInit. The URLPatternInit
// API is defined as part of the URLPattern specification.
// All provided strings must be valid UTF-8.
struct url_pattern_init {
  enum class process_type : uint8_t {
    url,
    pattern,
  };

  // All strings must be valid UTF-8.
  // @see https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
  static tl::expected<url_pattern_init, errors> process(
      url_pattern_init init, process_type type,
      std::optional<std::string_view> protocol = std::nullopt,
      std::optional<std::string_view> username = std::nullopt,
      std::optional<std::string_view> password = std::nullopt,
      std::optional<std::string_view> hostname = std::nullopt,
      std::optional<std::string_view> port = std::nullopt,
      std::optional<std::string_view> pathname = std::nullopt,
      std::optional<std::string_view> search = std::nullopt,
      std::optional<std::string_view> hash = std::nullopt);

  // @see https://urlpattern.spec.whatwg.org/#process-protocol-for-init
  static tl::expected<std::string, errors> process_protocol(
      std::string_view value, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-username-for-init
  static tl::expected<std::string, errors> process_username(
      std::string_view value, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-password-for-init
  static tl::expected<std::string, errors> process_password(
      std::string_view value, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-hostname-for-init
  static tl::expected<std::string, errors> process_hostname(
      std::string_view value, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-port-for-init
  static tl::expected<std::string, errors> process_port(
      std::string_view port, std::string_view protocol, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-pathname-for-init
  static tl::expected<std::string, errors> process_pathname(
      std::string_view value, std::string_view protocol, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-search-for-init
  static tl::expected<std::string, errors> process_search(
      std::string_view value, process_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-hash-for-init
  static tl::expected<std::string, errors> process_hash(std::string_view value,
                                                        process_type type);

#if ADA_TESTING
  friend void PrintTo(const url_pattern_init& init, std::ostream* os) {
    *os << "protocol: '" << init.protocol.value_or("undefined") << "', ";
    *os << "username: '" << init.username.value_or("undefined") << "', ";
    *os << "password: '" << init.password.value_or("undefined") << "', ";
    *os << "hostname: '" << init.hostname.value_or("undefined") << "', ";
    *os << "port: '" << init.port.value_or("undefined") << "', ";
    *os << "pathname: '" << init.pathname.value_or("undefined") << "', ";
    *os << "search: '" << init.search.value_or("undefined") << "', ";
    *os << "hash: '" << init.hash.value_or("undefined") << "', ";
    *os << "base_url: '" << init.base_url.value_or("undefined") << "', ";
  }
#endif  // ADA_TESTING

  bool operator==(const url_pattern_init&) const;
  // If present, must be valid UTF-8.
  std::optional<std::string> protocol{};
  // If present, must be valid UTF-8.
  std::optional<std::string> username{};
  // If present, must be valid UTF-8.
  std::optional<std::string> password{};
  // If present, must be valid UTF-8.
  std::optional<std::string> hostname{};
  // If present, must be valid UTF-8.
  std::optional<std::string> port{};
  // If present, must be valid UTF-8.
  std::optional<std::string> pathname{};
  // If present, must be valid UTF-8.
  std::optional<std::string> search{};
  // If present, must be valid UTF-8.
  std::optional<std::string> hash{};
  // If present, must be valid UTF-8.
  std::optional<std::string> base_url{};
};
}  // namespace ada

#endif  // ADA_URL_PATTERN_INIT_H
/* end file include/ada/url_pattern_init.h */

/**
 * @private
 */
namespace ada {
struct url_aggregator;
struct url;
template <url_pattern_regex::regex_concept regex_provider>
class url_pattern;
struct url_pattern_options;
enum class errors : uint8_t;
}  // namespace ada

/**
 * @namespace ada::parser
 * @brief Includes the definitions for supported parsers
 */
namespace ada::parser {
/**
 * Parses a url. The parameter user_input is the input to be parsed:
 * it should be a valid UTF-8 string. The parameter base_url is an optional
 * parameter that can be used to resolve relative URLs. If the base_url is
 * provided, the user_input is resolved against the base_url.
 */
template <typename result_type = url_aggregator>
result_type parse_url(std::string_view user_input,
                      const result_type* base_url = nullptr);

extern template url_aggregator parse_url<url_aggregator>(
    std::string_view user_input, const url_aggregator* base_url);
extern template url parse_url<url>(std::string_view user_input,
                                   const url* base_url);

template <typename result_type = url_aggregator, bool store_values = true>
result_type parse_url_impl(std::string_view user_input,
                           const result_type* base_url = nullptr);

extern template url_aggregator parse_url_impl<url_aggregator>(
    std::string_view user_input, const url_aggregator* base_url);
extern template url parse_url_impl<url>(std::string_view user_input,
                                        const url* base_url);

template <url_pattern_regex::regex_concept regex_provider>
tl::expected<url_pattern<regex_provider>, errors> parse_url_pattern_impl(
    std::variant<std::string_view, url_pattern_init> input,
    const std::string_view* base_url, const url_pattern_options* options);

}  // namespace ada::parser

#endif  // ADA_PARSER_H
/* end file include/ada/parser.h */
/* begin file include/ada/parser-inl.h */
/**
 * @file parser-inl.h
 */
#ifndef ADA_PARSER_INL_H
#define ADA_PARSER_INL_H

/* begin file include/ada/url_pattern.h */
/**
 * @file url_pattern.h
 * @brief Declaration for the URLPattern implementation.
 */
#ifndef ADA_URL_PATTERN_H
#define ADA_URL_PATTERN_H

/* begin file include/ada/implementation.h */
/**
 * @file implementation.h
 * @brief Definitions for user facing functions for parsing URL and it's
 * components.
 */
#ifndef ADA_IMPLEMENTATION_H
#define ADA_IMPLEMENTATION_H

#include <string>
#include <string_view>
#include <optional>

/* begin file include/ada/url.h */
/**
 * @file url.h
 * @brief Declaration for the URL
 */
#ifndef ADA_URL_H
#define ADA_URL_H

#include <algorithm>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

/* begin file include/ada/checkers.h */
/**
 * @file checkers.h
 * @brief Declarations for URL specific checkers used within Ada.
 */
#ifndef ADA_CHECKERS_H
#define ADA_CHECKERS_H


#include <cstring>
#include <string_view>

/**
 * These functions are not part of our public API and may
 * change at any time.
 * @private
 * @namespace ada::checkers
 * @brief Includes the definitions for validation functions
 */
namespace ada::checkers {

/**
 * @private
 * Assuming that x is an ASCII letter, this function returns the lower case
 * equivalent.
 * @details More likely to be inlined by the compiler and constexpr.
 */
constexpr char to_lower(char x) noexcept;

/**
 * @private
 * Returns true if the character is an ASCII letter. Equivalent to std::isalpha
 * but more likely to be inlined by the compiler.
 *
 * @attention std::isalpha is not constexpr generally.
 */
constexpr bool is_alpha(char x) noexcept;

/**
 * @private
 * Check whether a string starts with 0x or 0X. The function is only
 * safe if input.size() >=2.
 *
 * @see has_hex_prefix
 */
constexpr bool has_hex_prefix_unsafe(std::string_view input);
/**
 * @private
 * Check whether a string starts with 0x or 0X.
 */
constexpr bool has_hex_prefix(std::string_view input);

/**
 * @private
 * Check whether x is an ASCII digit. More likely to be inlined than
 * std::isdigit.
 */
constexpr bool is_digit(char x) noexcept;

/**
 * @private
 * @details A string starts with a Windows drive letter if all of the following
 * are true:
 *
 *   - its length is greater than or equal to 2
 *   - its first two code points are a Windows drive letter
 *   - its length is 2 or its third code point is U+002F (/), U+005C (\), U+003F
 * (?), or U+0023 (#).
 *
 * https://url.spec.whatwg.org/#start-with-a-windows-drive-letter
 */
inline constexpr bool is_windows_drive_letter(std::string_view input) noexcept;

/**
 * @private
 * @details A normalized Windows drive letter is a Windows drive letter of which
 * the second code point is U+003A (:).
 */
inline constexpr bool is_normalized_windows_drive_letter(
    std::string_view input) noexcept;

/**
 * @private
 * Returns true if an input is an ipv4 address. It is assumed that the string
 * does not contain uppercase ASCII characters (the input should have been
 * lowered cased before calling this function) and is not empty.
 */
ada_really_inline constexpr bool is_ipv4(std::string_view view) noexcept;

/**
 * @private
 * Returns a bitset. If the first bit is set, then at least one character needs
 * percent encoding. If the second bit is set, a \\ is found. If the third bit
 * is set then we have a dot. If the fourth bit is set, then we have a percent
 * character.
 */
ada_really_inline constexpr uint8_t path_signature(
    std::string_view input) noexcept;

/**
 * @private
 * Returns true if the length of the domain name and its labels are according to
 * the specifications. The length of the domain must be 255 octets (253
 * characters not including the last 2 which are the empty label reserved at the
 * end). When the empty label is included (a dot at the end), the domain name
 * can have 254 characters. The length of a label must be at least 1 and at most
 * 63 characters.
 * @see section 3.1. of https://www.rfc-editor.org/rfc/rfc1034
 * @see https://www.unicode.org/reports/tr46/#ToASCII
 */
ada_really_inline constexpr bool verify_dns_length(
    std::string_view input) noexcept;

}  // namespace ada::checkers

#endif  // ADA_CHECKERS_H
/* end file include/ada/checkers.h */
/* begin file include/ada/url_components.h */
/**
 * @file url_components.h
 * @brief Declaration for the URL Components
 */
#ifndef ADA_URL_COMPONENTS_H
#define ADA_URL_COMPONENTS_H

namespace ada {

/**
 * @brief URL Component representations using offsets.
 *
 * @details We design the url_components struct so that it is as small
 * and simple as possible. This version uses 32 bytes.
 *
 * This struct is used to extract components from a single 'href'.
 */
struct url_components {
  constexpr static uint32_t omitted = uint32_t(-1);

  url_components() = default;
  url_components(const url_components &u) = default;
  url_components(url_components &&u) noexcept = default;
  url_components &operator=(url_components &&u) noexcept = default;
  url_components &operator=(const url_components &u) = default;
  ~url_components() = default;

  /*
   * By using 32-bit integers, we implicitly assume that the URL string
   * cannot exceed 4 GB.
   *
   * https://user:pass@example.com:1234/foo/bar?baz#quux
   *       |     |    |          | ^^^^|       |   |
   *       |     |    |          | |   |       |   `----- hash_start
   *       |     |    |          | |   |       `--------- search_start
   *       |     |    |          | |   `----------------- pathname_start
   *       |     |    |          | `--------------------- port
   *       |     |    |          `----------------------- host_end
   *       |     |    `---------------------------------- host_start
   *       |     `--------------------------------------- username_end
   *       `--------------------------------------------- protocol_end
   */
  uint32_t protocol_end{0};
  /**
   * Username end is not `omitted` by default to make username and password
   * getters less costly to implement.
   */
  uint32_t username_end{0};
  uint32_t host_start{0};
  uint32_t host_end{0};
  uint32_t port{omitted};
  uint32_t pathname_start{0};
  uint32_t search_start{omitted};
  uint32_t hash_start{omitted};

  /**
   * Check the following conditions:
   * protocol_end < username_end < ... < hash_start,
   * expect when a value is omitted. It also computes
   * a lower bound on  the possible string length that may match these
   * offsets.
   * @return true if the offset values are
   *  consistent with a possible URL string
   */
  [[nodiscard]] constexpr bool check_offset_consistency() const noexcept;

  /**
   * Converts a url_components to JSON stringified version.
   */
  [[nodiscard]] std::string to_string() const;

};  // struct url_components
}  // namespace ada
#endif
/* end file include/ada/url_components.h */

namespace ada {

struct url_aggregator;

// namespace parser {
// template <typename result_type>
// result_type parse_url(std::string_view user_input,
//                       const result_type* base_url = nullptr);
// template <typename result_type, bool store_values>
// result_type parse_url_impl(std::string_view user_input,
//                            const result_type* base_url = nullptr);
// }

/**
 * @brief Generic URL struct reliant on std::string instantiation.
 *
 * @details To disambiguate from a valid URL string it can also be referred to
 * as a URL record. A URL is a struct that represents a universal identifier.
 * Unlike the url_aggregator, the ada::url represents the different components
 * of a parsed URL as independent std::string instances. This makes the
 * structure heavier and more reliant on memory allocations. When getting
 * components from the parsed URL, a new std::string is typically constructed.
 *
 * @see https://url.spec.whatwg.org/#url-representation
 */
struct url : url_base {
  url() = default;
  url(const url &u) = default;
  url(url &&u) noexcept = default;
  url &operator=(url &&u) noexcept = default;
  url &operator=(const url &u) = default;
  ~url() override = default;

  /**
   * @private
   * A URL's username is an ASCII string identifying a username. It is initially
   * the empty string.
   */
  std::string username{};

  /**
   * @private
   * A URL's password is an ASCII string identifying a password. It is initially
   * the empty string.
   */
  std::string password{};

  /**
   * @private
   * A URL's host is null or a host. It is initially null.
   */
  std::optional<std::string> host{};

  /**
   * @private
   * A URL's port is either null or a 16-bit unsigned integer that identifies a
   * networking port. It is initially null.
   */
  std::optional<uint16_t> port{};

  /**
   * @private
   * A URL's path is either an ASCII string or a list of zero or more ASCII
   * strings, usually identifying a location.
   */
  std::string path{};

  /**
   * @private
   * A URL's query is either null or an ASCII string. It is initially null.
   */
  std::optional<std::string> query{};

  /**
   * @private
   * A URL's fragment is either null or an ASCII string that can be used for
   * further processing on the resource the URL's other components identify. It
   * is initially null.
   */
  std::optional<std::string> hash{};

  /** @return true if it has an host but it is the empty string */
  [[nodiscard]] inline bool has_empty_hostname() const noexcept;
  /** @return true if the URL has a (non default) port */
  [[nodiscard]] inline bool has_port() const noexcept;
  /** @return true if it has a host (included an empty host) */
  [[nodiscard]] inline bool has_hostname() const noexcept;
  [[nodiscard]] bool has_valid_domain() const noexcept override;

  /**
   * Returns a JSON string representation of this URL.
   */
  [[nodiscard]] std::string to_string() const override;

  /**
   * @see https://url.spec.whatwg.org/#dom-url-href
   * @see https://url.spec.whatwg.org/#concept-url-serializer
   */
  [[nodiscard]] ada_really_inline std::string get_href() const noexcept;

  /**
   * The origin getter steps are to return the serialization of this's URL's
   * origin. [HTML]
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#concept-url-origin
   */
  [[nodiscard]] std::string get_origin() const noexcept override;

  /**
   * The protocol getter steps are to return this's URL's scheme, followed by
   * U+003A (:).
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#dom-url-protocol
   */
  [[nodiscard]] std::string get_protocol() const noexcept;

  /**
   * Return url's host, serialized, followed by U+003A (:) and url's port,
   * serialized.
   * When there is no host, this function returns the empty string.
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#dom-url-host
   */
  [[nodiscard]] std::string get_host() const noexcept;

  /**
   * Return this's URL's host, serialized.
   * When there is no host, this function returns the empty string.
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#dom-url-hostname
   */
  [[nodiscard]] std::string get_hostname() const noexcept;

  /**
   * The pathname getter steps are to return the result of URL path serializing
   * this's URL.
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#dom-url-pathname
   */
  [[nodiscard]] constexpr std::string_view get_pathname() const noexcept;

  /**
   * Compute the pathname length in bytes without instantiating a view or a
   * string.
   * @return size of the pathname in bytes
   * @see https://url.spec.whatwg.org/#dom-url-pathname
   */
  [[nodiscard]] ada_really_inline size_t get_pathname_length() const noexcept;

  /**
   * Return U+003F (?), followed by this's URL's query.
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#dom-url-search
   */
  [[nodiscard]] std::string get_search() const noexcept;

  /**
   * The username getter steps are to return this's URL's username.
   * @return a constant reference to the underlying string.
   * @see https://url.spec.whatwg.org/#dom-url-username
   */
  [[nodiscard]] const std::string &get_username() const noexcept;

  /**
   * @return Returns true on successful operation.
   * @see https://url.spec.whatwg.org/#dom-url-username
   */
  bool set_username(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-password
   */
  bool set_password(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-port
   */
  bool set_port(std::string_view input);

  /**
   * This function always succeeds.
   * @see https://url.spec.whatwg.org/#dom-url-hash
   */
  void set_hash(std::string_view input);

  /**
   * This function always succeeds.
   * @see https://url.spec.whatwg.org/#dom-url-search
   */
  void set_search(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-search
   */
  bool set_pathname(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-host
   */
  bool set_host(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-hostname
   */
  bool set_hostname(std::string_view input);

  /**
   * @return Returns true on success.
   * @see https://url.spec.whatwg.org/#dom-url-protocol
   */
  bool set_protocol(std::string_view input);

  /**
   * @see https://url.spec.whatwg.org/#dom-url-href
   */
  bool set_href(std::string_view input);

  /**
   * The password getter steps are to return this's URL's password.
   * @return a constant reference to the underlying string.
   * @see https://url.spec.whatwg.org/#dom-url-password
   */
  [[nodiscard]] const std::string &get_password() const noexcept;

  /**
   * Return this's URL's port, serialized.
   * @return a newly constructed string representing the port.
   * @see https://url.spec.whatwg.org/#dom-url-port
   */
  [[nodiscard]] std::string get_port() const noexcept;

  /**
   * Return U+0023 (#), followed by this's URL's fragment.
   * @return a newly constructed string representing the hash.
   * @see https://url.spec.whatwg.org/#dom-url-hash
   */
  [[nodiscard]] std::string get_hash() const noexcept;

  /**
   * A URL includes credentials if its username or password is not the empty
   * string.
   */
  [[nodiscard]] ada_really_inline bool has_credentials() const noexcept;

  /**
   * Useful for implementing efficient serialization for the URL.
   *
   * https://user:pass@example.com:1234/foo/bar?baz#quux
   *       |     |    |          | ^^^^|       |   |
   *       |     |    |          | |   |       |   `----- hash_start
   *       |     |    |          | |   |       `--------- search_start
   *       |     |    |          | |   `----------------- pathname_start
   *       |     |    |          | `--------------------- port
   *       |     |    |          `----------------------- host_end
   *       |     |    `---------------------------------- host_start
   *       |     `--------------------------------------- username_end
   *       `--------------------------------------------- protocol_end
   *
   * Inspired after servo/url
   *
   * @return a newly constructed component.
   *
   * @see
   * https://github.com/servo/rust-url/blob/b65a45515c10713f6d212e6726719a020203cc98/url/src/quirks.rs#L31
   */
  [[nodiscard]] ada_really_inline ada::url_components get_components()
      const noexcept;
  /** @return true if the URL has a hash component */
  [[nodiscard]] constexpr bool has_hash() const noexcept override;
  /** @return true if the URL has a search component */
  [[nodiscard]] constexpr bool has_search() const noexcept override;

 private:
  friend ada::url ada::parser::parse_url<ada::url>(std::string_view,
                                                   const ada::url *);
  friend ada::url_aggregator ada::parser::parse_url<ada::url_aggregator>(
      std::string_view, const ada::url_aggregator *);
  friend void ada::helpers::strip_trailing_spaces_from_opaque_path<ada::url>(
      ada::url &url) noexcept;

  friend ada::url ada::parser::parse_url_impl<ada::url, true>(std::string_view,
                                                              const ada::url *);
  friend ada::url_aggregator ada::parser::parse_url_impl<
      ada::url_aggregator, true>(std::string_view, const ada::url_aggregator *);

  inline void update_unencoded_base_hash(std::string_view input);
  inline void update_base_hostname(std::string_view input);
  inline void update_base_search(std::string_view input,
                                 const uint8_t query_percent_encode_set[]);
  inline void update_base_search(std::optional<std::string> &&input);
  inline void update_base_pathname(std::string_view input);
  inline void update_base_username(std::string_view input);
  inline void update_base_password(std::string_view input);
  inline void update_base_port(std::optional<uint16_t> input);

  /**
   * Sets the host or hostname according to override condition.
   * Return true on success.
   * @see https://url.spec.whatwg.org/#hostname-state
   */
  template <bool override_hostname = false>
  bool set_host_or_hostname(std::string_view input);

  /**
   * Return true on success.
   * @see https://url.spec.whatwg.org/#concept-ipv4-parser
   */
  [[nodiscard]] bool parse_ipv4(std::string_view input);

  /**
   * Return true on success.
   * @see https://url.spec.whatwg.org/#concept-ipv6-parser
   */
  [[nodiscard]] bool parse_ipv6(std::string_view input);

  /**
   * Return true on success.
   * @see https://url.spec.whatwg.org/#concept-opaque-host-parser
   */
  [[nodiscard]] bool parse_opaque_host(std::string_view input);

  /**
   * A URL's scheme is an ASCII string that identifies the type of URL and can
   * be used to dispatch a URL for further processing after parsing. It is
   * initially the empty string. We only set non_special_scheme when the scheme
   * is non-special, otherwise we avoid constructing string.
   *
   * Special schemes are stored in ada::scheme::details::is_special_list so we
   * typically do not need to store them in each url instance.
   */
  std::string non_special_scheme{};

  /**
   * A URL cannot have a username/password/port if its host is null or the empty
   * string, or its scheme is "file".
   */
  [[nodiscard]] inline bool cannot_have_credentials_or_port() const;

  ada_really_inline size_t parse_port(
      std::string_view view, bool check_trailing_content) noexcept override;

  ada_really_inline size_t parse_port(std::string_view view) noexcept override {
    return this->parse_port(view, false);
  }

  /**
   * Parse the host from the provided input. We assume that
   * the input does not contain spaces or tabs. Control
   * characters and spaces are not trimmed (they should have
   * been removed if needed).
   * Return true on success.
   * @see https://url.spec.whatwg.org/#host-parsing
   */
  [[nodiscard]] ada_really_inline bool parse_host(std::string_view input);

  template <bool has_state_override = false>
  [[nodiscard]] ada_really_inline bool parse_scheme(std::string_view input);

  constexpr void clear_pathname() override;
  constexpr void clear_search() override;
  constexpr void set_protocol_as_file();

  /**
   * Parse the path from the provided input.
   * Return true on success. Control characters not
   * trimmed from the ends (they should have
   * been removed if needed).
   *
   * The input is expected to be UTF-8.
   *
   * @see https://url.spec.whatwg.org/
   */
  ada_really_inline void parse_path(std::string_view input);

  /**
   * Set the scheme for this URL. The provided scheme should be a valid
   * scheme string, be lower-cased, not contain spaces or tabs. It should
   * have no spurious trailing or leading content.
   */
  inline void set_scheme(std::string &&new_scheme) noexcept;

  /**
   * Take the scheme from another URL. The scheme string is moved from the
   * provided url.
   */
  constexpr void copy_scheme(ada::url &&u) noexcept;

  /**
   * Take the scheme from another URL. The scheme string is copied from the
   * provided url.
   */
  constexpr void copy_scheme(const ada::url &u);

};  // struct url

inline std::ostream &operator<<(std::ostream &out, const ada::url &u);
}  // namespace ada

#endif  // ADA_URL_H
/* end file include/ada/url.h */

namespace ada {

template <class result_type = ada::url_aggregator>
using result = tl::expected<result_type, ada::errors>;

/**
 * The URL parser takes a scalar value string input, with an optional null or
 * base URL base (default null). The parser assumes the input is a valid ASCII
 * or UTF-8 string.
 *
 * @param input the string input to analyze (must be valid ASCII or UTF-8)
 * @param base_url the optional URL input to use as a base url.
 * @return a parsed URL.
 */
template <class result_type = ada::url_aggregator>
ada_warn_unused ada::result<result_type> parse(
    std::string_view input, const result_type* base_url = nullptr);

extern template ada::result<url> parse<url>(std::string_view input,
                                            const url* base_url);
extern template ada::result<url_aggregator> parse<url_aggregator>(
    std::string_view input, const url_aggregator* base_url);

/**
 * Verifies whether the URL strings can be parsed. The function assumes
 * that the inputs are valid ASCII or UTF-8 strings.
 * @see https://url.spec.whatwg.org/#dom-url-canparse
 * @return If URL can be parsed or not.
 */
bool can_parse(std::string_view input,
               const std::string_view* base_input = nullptr);

/**
 * Implementation of the URL pattern parsing algorithm.
 * @see https://urlpattern.spec.whatwg.org
 *
 * @param input valid UTF-8 string or URLPatternInit struct
 * @param base_url an optional valid UTF-8 string
 * @param options an optional url_pattern_options struct
 * @return url_pattern instance
 */
template <url_pattern_regex::regex_concept regex_provider>
ada_warn_unused tl::expected<url_pattern<regex_provider>, errors>
parse_url_pattern(std::variant<std::string_view, url_pattern_init> input,
                  const std::string_view* base_url = nullptr,
                  const url_pattern_options* options = nullptr);

/**
 * Computes a href string from a file path. The function assumes
 * that the input is a valid ASCII or UTF-8 string.
 * @return a href string (starts with file:://)
 */
std::string href_from_file(std::string_view path);
}  // namespace ada

#endif  // ADA_IMPLEMENTATION_H
/* end file include/ada/implementation.h */

#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#if ADA_TESTING
#include <iostream>
#endif  // ADA_TESTING

namespace ada {

enum class url_pattern_part_type : uint8_t {
  // The part represents a simple fixed text string.
  FIXED_TEXT,
  // The part represents a matching group with a custom regular expression.
  REGEXP,
  // The part represents a matching group that matches code points up to the
  // next separator code point. This is typically used for a named group like
  // ":foo" that does not have a custom regular expression.
  SEGMENT_WILDCARD,
  // The part represents a matching group that greedily matches all code points.
  // This is typically used for the "*" wildcard matching group.
  FULL_WILDCARD,
};

enum class url_pattern_part_modifier : uint8_t {
  // The part does not have a modifier.
  none,
  // The part has an optional modifier indicated by the U+003F (?) code point.
  optional,
  // The part has a "zero or more" modifier indicated by the U+002A (*) code
  // point.
  zero_or_more,
  // The part has a "one or more" modifier indicated by the U+002B (+) code
  // point.
  one_or_more,
};

// @see https://urlpattern.spec.whatwg.org/#part
class url_pattern_part {
 public:
  url_pattern_part(url_pattern_part_type _type, std::string&& _value,
                   url_pattern_part_modifier _modifier)
      : type(_type), value(_value), modifier(_modifier) {}

  url_pattern_part(url_pattern_part_type _type, std::string&& _value,
                   url_pattern_part_modifier _modifier, std::string&& _name,
                   std::string&& _prefix, std::string&& _suffix)
      : type(_type),
        value(_value),
        modifier(_modifier),
        name(_name),
        prefix(_prefix),
        suffix(_suffix) {}
  // A part has an associated type, a string, which must be set upon creation.
  url_pattern_part_type type;
  // A part has an associated value, a string, which must be set upon creation.
  std::string value;
  // A part has an associated modifier a string, which must be set upon
  // creation.
  url_pattern_part_modifier modifier;
  // A part has an associated name, a string, initially the empty string.
  std::string name{};
  // A part has an associated prefix, a string, initially the empty string.
  std::string prefix{};
  // A part has an associated suffix, a string, initially the empty string.
  std::string suffix{};

  inline bool is_regexp() const noexcept;
};

// @see https://urlpattern.spec.whatwg.org/#options-header
struct url_pattern_compile_component_options {
  url_pattern_compile_component_options() = default;
  explicit url_pattern_compile_component_options(
      std::optional<char> new_delimiter = std::nullopt,
      std::optional<char> new_prefix = std::nullopt)
      : delimiter(new_delimiter), prefix(new_prefix) {}

  inline std::string_view get_delimiter() const ada_warn_unused;
  inline std::string_view get_prefix() const ada_warn_unused;

  // @see https://urlpattern.spec.whatwg.org/#options-ignore-case
  bool ignore_case = false;

  static url_pattern_compile_component_options DEFAULT;
  static url_pattern_compile_component_options HOSTNAME;
  static url_pattern_compile_component_options PATHNAME;

 private:
  // @see https://urlpattern.spec.whatwg.org/#options-delimiter-code-point
  std::optional<char> delimiter{};
  // @see https://urlpattern.spec.whatwg.org/#options-prefix-code-point
  std::optional<char> prefix{};
};

// The default options is an options struct with delimiter code point set to
// the empty string and prefix code point set to the empty string.
inline url_pattern_compile_component_options
    url_pattern_compile_component_options::DEFAULT(std::nullopt, std::nullopt);

// The hostname options is an options struct with delimiter code point set
// "." and prefix code point set to the empty string.
inline url_pattern_compile_component_options
    url_pattern_compile_component_options::HOSTNAME('.', std::nullopt);

// The pathname options is an options struct with delimiter code point set
// "/" and prefix code point set to "/".
inline url_pattern_compile_component_options
    url_pattern_compile_component_options::PATHNAME('/', '/');

// A struct providing the URLPattern matching results for a single
// URL component. The URLPatternComponentResult is only ever used
// as a member attribute of a URLPatternResult struct. The
// URLPatternComponentResult API is defined as part of the URLPattern
// specification.
struct url_pattern_component_result {
  std::string input;
  std::unordered_map<std::string, std::optional<std::string>> groups;

  bool operator==(const url_pattern_component_result&) const;

#if ADA_TESTING
  friend void PrintTo(const url_pattern_component_result& result,
                      std::ostream* os) {
    *os << "input: '" << result.input << "', group: ";
    for (const auto& group : result.groups) {
      *os << "(" << group.first << ", " << group.second.value_or("undefined")
          << ") ";
    }
  }
#endif  // ADA_TESTING
};

template <url_pattern_regex::regex_concept regex_provider>
class url_pattern_component {
 public:
  url_pattern_component() = default;

  // This function explicitly takes a std::string because it is moved.
  // To avoid unnecessary copy, move each value while calling the constructor.
  url_pattern_component(std::string&& new_pattern,
                        typename regex_provider::regex_type&& new_regexp,
                        std::vector<std::string>&& new_group_name_list,
                        bool new_has_regexp_groups)
      : regexp(std::move(new_regexp)),
        pattern(std::move(new_pattern)),
        group_name_list(new_group_name_list),
        has_regexp_groups(new_has_regexp_groups) {}

  // @see https://urlpattern.spec.whatwg.org/#compile-a-component
  template <url_pattern_encoding_callback F>
  static tl::expected<url_pattern_component, errors> compile(
      std::string_view input, F& encoding_callback,
      url_pattern_compile_component_options& options);

  // @see https://urlpattern.spec.whatwg.org/#create-a-component-match-result
  url_pattern_component_result create_component_match_result(
      std::string&& input,
      std::vector<std::optional<std::string>>&& exec_result);

#if ADA_TESTING
  friend void PrintTo(const url_pattern_component& component,
                      std::ostream* os) {
    *os << "pattern: '" << component.pattern
        << "', has_regexp_groups: " << component.has_regexp_groups
        << "group_name_list: ";
    for (const auto& name : component.group_name_list) {
      *os << name << ", ";
    }
  }
#endif  // ADA_TESTING

  typename regex_provider::regex_type regexp{};
  std::string pattern{};
  std::vector<std::string> group_name_list{};
  bool has_regexp_groups = false;
};

// A URLPattern input can be either a string or a URLPatternInit object.
// If it is a string, it must be a valid UTF-8 string.
using url_pattern_input = std::variant<std::string_view, url_pattern_init>;

// A struct providing the URLPattern matching results for all
// components of a URL. The URLPatternResult API is defined as
// part of the URLPattern specification.
struct url_pattern_result {
  std::vector<url_pattern_input> inputs;
  url_pattern_component_result protocol;
  url_pattern_component_result username;
  url_pattern_component_result password;
  url_pattern_component_result hostname;
  url_pattern_component_result port;
  url_pattern_component_result pathname;
  url_pattern_component_result search;
  url_pattern_component_result hash;
};

struct url_pattern_options {
  bool ignore_case = false;

#if ADA_TESTING
  friend void PrintTo(const url_pattern_options& options, std::ostream* os) {
    *os << "ignore_case: '" << options.ignore_case;
  }
#endif  // ADA_TESTING
};

// URLPattern is a Web Platform standard API for matching URLs against a
// pattern syntax (think of it as a regular expression for URLs). It is
// defined in https://wicg.github.io/urlpattern.
// More information about the URL Pattern syntax can be found at
// https://developer.mozilla.org/en-US/docs/Web/API/URL_Pattern_API
//
// We require all strings to be valid UTF-8: it is the user's responsibility
// to ensure that the provided strings are valid UTF-8.
template <url_pattern_regex::regex_concept regex_provider>
class url_pattern {
 public:
  url_pattern() = default;

  /**
   * If non-null, base_url must pointer at a valid UTF-8 string.
   * @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-exec
   */
  result<std::optional<url_pattern_result>> exec(
      const url_pattern_input& input,
      const std::string_view* base_url = nullptr);

  /**
   * If non-null, base_url must pointer at a valid UTF-8 string.
   * @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-test
   */
  result<bool> test(const url_pattern_input& input,
                    const std::string_view* base_url = nullptr);

  /**
   * @see https://urlpattern.spec.whatwg.org/#url-pattern-match
   * This function expects a valid UTF-8 string if input is a string.
   */
  result<std::optional<url_pattern_result>> match(
      const url_pattern_input& input,
      const std::string_view* base_url_string = nullptr);

  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-protocol
  [[nodiscard]] std::string_view get_protocol() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-username
  [[nodiscard]] std::string_view get_username() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-password
  [[nodiscard]] std::string_view get_password() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-hostname
  [[nodiscard]] std::string_view get_hostname() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-port
  [[nodiscard]] std::string_view get_port() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-pathname
  [[nodiscard]] std::string_view get_pathname() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-search
  [[nodiscard]] std::string_view get_search() const ada_lifetime_bound;
  // @see https://urlpattern.spec.whatwg.org/#dom-urlpattern-hash
  [[nodiscard]] std::string_view get_hash() const ada_lifetime_bound;

  // If ignoreCase is true, the JavaScript regular expression created for each
  // pattern must use the `vi` flag. Otherwise, they must use the `v` flag.
  [[nodiscard]] bool ignore_case() const;

  // @see https://urlpattern.spec.whatwg.org/#url-pattern-has-regexp-groups
  [[nodiscard]] bool has_regexp_groups() const;

#if ADA_TESTING
  friend void PrintTo(const url_pattern& c, std::ostream* os) {
    *os << "protocol_component: '" << c.get_protocol() << ", ";
    *os << "username_component: '" << c.get_username() << ", ";
    *os << "password_component: '" << c.get_password() << ", ";
    *os << "hostname_component: '" << c.get_hostname() << ", ";
    *os << "port_component: '" << c.get_port() << ", ";
    *os << "pathname_component: '" << c.get_pathname() << ", ";
    *os << "search_component: '" << c.get_search() << ", ";
    *os << "hash_component: '" << c.get_hash();
  }
#endif  // ADA_TESTING

  template <url_pattern_regex::regex_concept P>
  friend tl::expected<url_pattern<P>, errors> parser::parse_url_pattern_impl(
      std::variant<std::string_view, url_pattern_init> input,
      const std::string_view* base_url, const url_pattern_options* options);

  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> protocol_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> username_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> password_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> hostname_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> port_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> pathname_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> search_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  url_pattern_component<regex_provider> hash_component{};
  /**
   * @private
   * We can not make this private due to a LLVM bug.
   * Ref: https://github.com/ada-url/ada/pull/859
   */
  bool ignore_case_ = false;
};

}  // namespace ada

#endif
/* end file include/ada/url_pattern.h */
/* begin file include/ada/url_pattern_helpers.h */
/**
 * @file url_pattern_helpers.h
 * @brief Declaration for the URLPattern helpers.
 */
#ifndef ADA_URL_PATTERN_HELPERS_H
#define ADA_URL_PATTERN_HELPERS_H


#include <string>
#include <tuple>
#include <vector>

namespace ada {
enum class errors : uint8_t;
}

namespace ada::url_pattern_helpers {

// @see https://urlpattern.spec.whatwg.org/#token
enum class token_type : uint8_t {
  INVALID_CHAR,    // 0
  OPEN,            // 1
  CLOSE,           // 2
  REGEXP,          // 3
  NAME,            // 4
  CHAR,            // 5
  ESCAPED_CHAR,    // 6
  OTHER_MODIFIER,  // 7
  ASTERISK,        // 8
  END,             // 9
};

#ifdef ADA_TESTING
std::string to_string(token_type type);
#endif  // ADA_TESTING

// @see https://urlpattern.spec.whatwg.org/#tokenize-policy
enum class token_policy {
  strict,
  lenient,
};

// @see https://urlpattern.spec.whatwg.org/#tokens
class token {
 public:
  token(token_type _type, size_t _index, std::string&& _value)
      : type(_type), index(_index), value(std::move(_value)) {}

  // A token has an associated type, a string, initially "invalid-char".
  token_type type = token_type::INVALID_CHAR;

  // A token has an associated index, a number, initially 0. It is the position
  // of the first code point in the pattern string represented by the token.
  size_t index = 0;

  // A token has an associated value, a string, initially the empty string. It
  // contains the code points from the pattern string represented by the token.
  std::string value{};
};

// @see https://urlpattern.spec.whatwg.org/#pattern-parser
template <url_pattern_encoding_callback F>
class url_pattern_parser {
 public:
  url_pattern_parser(F& encoding_callback_,
                     std::string_view segment_wildcard_regexp_)
      : encoding_callback(encoding_callback_),
        segment_wildcard_regexp(segment_wildcard_regexp_) {}

  bool can_continue() const { return index < tokens.size(); }

  // @see https://urlpattern.spec.whatwg.org/#try-to-consume-a-token
  token* try_consume_token(token_type type);
  // @see https://urlpattern.spec.whatwg.org/#try-to-consume-a-modifier-token
  token* try_consume_modifier_token();
  // @see
  // https://urlpattern.spec.whatwg.org/#try-to-consume-a-regexp-or-wildcard-token
  token* try_consume_regexp_or_wildcard_token(const token* name_token);
  // @see https://urlpattern.spec.whatwg.org/#consume-text
  std::string consume_text();
  // @see https://urlpattern.spec.whatwg.org/#consume-a-required-token
  bool consume_required_token(token_type type);
  // @see
  // https://urlpattern.spec.whatwg.org/#maybe-add-a-part-from-the-pending-fixed-value
  std::optional<errors> maybe_add_part_from_the_pending_fixed_value()
      ada_warn_unused;
  // @see https://urlpattern.spec.whatwg.org/#add-a-part
  std::optional<errors> add_part(std::string_view prefix, token* name_token,
                                 token* regexp_or_wildcard_token,
                                 std::string_view suyffix,
                                 token* modifier_token) ada_warn_unused;

  std::vector<token> tokens{};
  F& encoding_callback;
  std::string segment_wildcard_regexp;
  std::vector<url_pattern_part> parts{};
  std::string pending_fixed_value{};
  size_t index = 0;
  size_t next_numeric_name = 0;
};

// @see https://urlpattern.spec.whatwg.org/#tokenizer
class Tokenizer {
 public:
  explicit Tokenizer(std::string_view new_input, token_policy new_policy)
      : input(new_input), policy(new_policy) {}

  // @see https://urlpattern.spec.whatwg.org/#get-the-next-code-point
  constexpr void get_next_code_point();

  // @see https://urlpattern.spec.whatwg.org/#seek-and-get-the-next-code-point
  constexpr void seek_and_get_next_code_point(size_t index);

  // @see https://urlpattern.spec.whatwg.org/#add-a-token

  void add_token(token_type type, size_t next_position, size_t value_position,
                 size_t value_length);

  // @see https://urlpattern.spec.whatwg.org/#add-a-token-with-default-length
  void add_token_with_default_length(token_type type, size_t next_position,
                                     size_t value_position);

  // @see
  // https://urlpattern.spec.whatwg.org/#add-a-token-with-default-position-and-length
  void add_token_with_defaults(token_type type);

  // @see https://urlpattern.spec.whatwg.org/#process-a-tokenizing-error
  std::optional<errors> process_tokenizing_error(
      size_t next_position, size_t value_position) ada_warn_unused;

  friend tl::expected<std::vector<token>, errors> tokenize(
      std::string_view input, token_policy policy);

 private:
  // has an associated input, a pattern string, initially the empty string.
  std::string input;
  // has an associated policy, a tokenize policy, initially "strict".
  token_policy policy;
  // has an associated token list, a token list, initially an empty list.
  std::vector<token> token_list{};
  // has an associated index, a number, initially 0.
  size_t index = 0;
  // has an associated next index, a number, initially 0.
  size_t next_index = 0;
  // has an associated code point, a Unicode code point, initially null.
  char32_t code_point{};
};

// @see https://urlpattern.spec.whatwg.org/#constructor-string-parser
template <url_pattern_regex::regex_concept regex_provider>
struct constructor_string_parser {
  explicit constructor_string_parser(std::string_view new_input,
                                     std::vector<token>&& new_token_list)
      : input(new_input), token_list(std::move(new_token_list)) {}
  // @see https://urlpattern.spec.whatwg.org/#parse-a-constructor-string
  static tl::expected<url_pattern_init, errors> parse(std::string_view input);

  // @see https://urlpattern.spec.whatwg.org/#constructor-string-parser-state
  enum class State {
    INIT,
    PROTOCOL,
    AUTHORITY,
    USERNAME,
    PASSWORD,
    HOSTNAME,
    PORT,
    PATHNAME,
    SEARCH,
    HASH,
    DONE,
  };

  // @see
  // https://urlpattern.spec.whatwg.org/#compute-protocol-matches-a-special-scheme-flag
  std::optional<errors> compute_protocol_matches_special_scheme_flag();

 private:
  // @see https://urlpattern.spec.whatwg.org/#rewind
  constexpr void rewind();

  // @see https://urlpattern.spec.whatwg.org/#is-a-hash-prefix
  constexpr bool is_hash_prefix();

  // @see https://urlpattern.spec.whatwg.org/#is-a-search-prefix
  constexpr bool is_search_prefix();

  // @see https://urlpattern.spec.whatwg.org/#change-state
  void change_state(State state, size_t skip);

  // @see https://urlpattern.spec.whatwg.org/#is-a-group-open
  constexpr bool is_group_open() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-group-close
  constexpr bool is_group_close() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-protocol-suffix
  constexpr bool is_protocol_suffix() const;

  // @see https://urlpattern.spec.whatwg.org/#next-is-authority-slashes
  constexpr bool next_is_authority_slashes() const;

  // @see https://urlpattern.spec.whatwg.org/#is-an-identity-terminator
  constexpr bool is_an_identity_terminator() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-pathname-start
  constexpr bool is_pathname_start() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-password-prefix
  constexpr bool is_password_prefix() const;

  // @see https://urlpattern.spec.whatwg.org/#is-an-ipv6-open
  constexpr bool is_an_ipv6_open() const;

  // @see https://urlpattern.spec.whatwg.org/#is-an-ipv6-close
  constexpr bool is_an_ipv6_close() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-port-prefix
  constexpr bool is_port_prefix() const;

  // @see https://urlpattern.spec.whatwg.org/#is-a-non-special-pattern-char
  constexpr bool is_non_special_pattern_char(size_t index,
                                             uint32_t value) const;

  // @see https://urlpattern.spec.whatwg.org/#get-a-safe-token
  constexpr const token* get_safe_token(size_t index) const;

  // @see https://urlpattern.spec.whatwg.org/#make-a-component-string
  std::string make_component_string();
  // has an associated input, a string, which must be set upon creation.
  std::string input;
  // has an associated token list, a token list, which must be set upon
  // creation.
  std::vector<token> token_list;
  // has an associated result, a URLPatternInit, initially set to a new
  // URLPatternInit.
  url_pattern_init result{};
  // has an associated component start, a number, initially set to 0.
  size_t component_start = 0;
  // has an associated token index, a number, initially set to 0.
  size_t token_index = 0;
  // has an associated token increment, a number, initially set to 1.
  size_t token_increment = 1;
  // has an associated group depth, a number, initially set to 0.
  size_t group_depth = 0;
  // has an associated hostname IPv6 bracket depth, a number, initially set to
  // 0.
  size_t hostname_ipv6_bracket_depth = 0;
  // has an associated protocol matches a special scheme flag, a boolean,
  // initially set to false.
  bool protocol_matches_a_special_scheme_flag = false;
  // has an associated state, a string, initially set to "init".
  State state = State::INIT;
};

// @see https://urlpattern.spec.whatwg.org/#canonicalize-a-protocol
tl::expected<std::string, errors> canonicalize_protocol(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-username
tl::expected<std::string, errors> canonicalize_username(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-password
tl::expected<std::string, errors> canonicalize_password(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-password
tl::expected<std::string, errors> canonicalize_hostname(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-an-ipv6-hostname
tl::expected<std::string, errors> canonicalize_ipv6_hostname(
    std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-port
tl::expected<std::string, errors> canonicalize_port(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-port
tl::expected<std::string, errors> canonicalize_port_with_protocol(
    std::string_view input, std::string_view protocol);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-pathname
tl::expected<std::string, errors> canonicalize_pathname(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-an-opaque-pathname
tl::expected<std::string, errors> canonicalize_opaque_pathname(
    std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-search
tl::expected<std::string, errors> canonicalize_search(std::string_view input);

// @see https://wicg.github.io/urlpattern/#canonicalize-a-hash
tl::expected<std::string, errors> canonicalize_hash(std::string_view input);

// @see https://urlpattern.spec.whatwg.org/#tokenize
tl::expected<std::vector<token>, errors> tokenize(std::string_view input,
                                                  token_policy policy);

// @see https://urlpattern.spec.whatwg.org/#process-a-base-url-string
std::string process_base_url_string(std::string_view input,
                                    url_pattern_init::process_type type);

// @see https://urlpattern.spec.whatwg.org/#escape-a-pattern-string
std::string escape_pattern_string(std::string_view input);

// @see https://urlpattern.spec.whatwg.org/#escape-a-regexp-string
std::string escape_regexp_string(std::string_view input);

// @see https://urlpattern.spec.whatwg.org/#is-an-absolute-pathname
constexpr bool is_absolute_pathname(
    std::string_view input, url_pattern_init::process_type type) noexcept;

// @see https://urlpattern.spec.whatwg.org/#parse-a-pattern-string
template <url_pattern_encoding_callback F>
tl::expected<std::vector<url_pattern_part>, errors> parse_pattern_string(
    std::string_view input, url_pattern_compile_component_options& options,
    F& encoding_callback);

// @see https://urlpattern.spec.whatwg.org/#generate-a-pattern-string
std::string generate_pattern_string(
    std::vector<url_pattern_part>& part_list,
    url_pattern_compile_component_options& options);

// @see
// https://urlpattern.spec.whatwg.org/#generate-a-regular-expression-and-name-list
std::tuple<std::string, std::vector<std::string>>
generate_regular_expression_and_name_list(
    const std::vector<url_pattern_part>& part_list,
    url_pattern_compile_component_options options);

// @see https://urlpattern.spec.whatwg.org/#hostname-pattern-is-an-ipv6-address
bool is_ipv6_address(std::string_view input) noexcept;

// @see
// https://urlpattern.spec.whatwg.org/#protocol-component-matches-a-special-scheme
template <url_pattern_regex::regex_concept regex_provider>
bool protocol_component_matches_special_scheme(
    ada::url_pattern_component<regex_provider>& input);

// @see https://urlpattern.spec.whatwg.org/#convert-a-modifier-to-a-string
std::string convert_modifier_to_string(url_pattern_part_modifier modifier);

// @see https://urlpattern.spec.whatwg.org/#generate-a-segment-wildcard-regexp
std::string generate_segment_wildcard_regexp(
    url_pattern_compile_component_options options);

}  // namespace ada::url_pattern_helpers

#endif
/* end file include/ada/url_pattern_helpers.h */

#include <string>
#include <string_view>
#include <variant>

namespace ada::parser {
template <url_pattern_regex::regex_concept regex_provider>
tl::expected<url_pattern<regex_provider>, errors> parse_url_pattern_impl(
    std::variant<std::string_view, url_pattern_init> input,
    const std::string_view* base_url, const url_pattern_options* options) {
  // Let init be null.
  url_pattern_init init;

  // If input is a scalar value string then:
  if (std::holds_alternative<std::string_view>(input)) {
    // Set init to the result of running parse a constructor string given input.
    auto parse_result =
        url_pattern_helpers::constructor_string_parser<regex_provider>::parse(
            std::get<std::string_view>(input));
    if (!parse_result) {
      ada_log("constructor_string_parser::parse failed");
      return tl::unexpected(parse_result.error());
    }
    init = std::move(*parse_result);
    // If baseURL is null and init["protocol"] does not exist, then throw a
    // TypeError.
    if (!base_url && !init.protocol) {
      ada_log("base url is null and protocol is not set");
      return tl::unexpected(errors::type_error);
    }

    // If baseURL is not null, set init["baseURL"] to baseURL.
    if (base_url) {
      init.base_url = std::string(*base_url);
    }
  } else {
    // Assert: input is a URLPatternInit.
    ADA_ASSERT_TRUE(std::holds_alternative<url_pattern_init>(input));
    // If baseURL is not null, then throw a TypeError.
    if (base_url) {
      ada_log("base url is not null");
      return tl::unexpected(errors::type_error);
    }
    // Optimization: Avoid copy by moving the input value.
    // Set init to input.
    init = std::move(std::get<url_pattern_init>(input));
  }

  // Let processedInit be the result of process a URLPatternInit given init,
  // "pattern", null, null, null, null, null, null, null, and null.
  auto processed_init =
      url_pattern_init::process(init, url_pattern_init::process_type::pattern);
  if (!processed_init) {
    ada_log("url_pattern_init::process failed for init and 'pattern'");
    return tl::unexpected(processed_init.error());
  }

  // For each componentName of « "protocol", "username", "password", "hostname",
  // "port", "pathname", "search", "hash" If processedInit[componentName] does
  // not exist, then set processedInit[componentName] to "*".
  ADA_ASSERT_TRUE(processed_init.has_value());
  if (!processed_init->protocol) processed_init->protocol = "*";
  if (!processed_init->username) processed_init->username = "*";
  if (!processed_init->password) processed_init->password = "*";
  if (!processed_init->hostname) processed_init->hostname = "*";
  if (!processed_init->port) processed_init->port = "*";
  if (!processed_init->pathname) processed_init->pathname = "*";
  if (!processed_init->search) processed_init->search = "*";
  if (!processed_init->hash) processed_init->hash = "*";

  ada_log("-- processed_init->protocol: ", processed_init->protocol.value());
  ada_log("-- processed_init->username: ", processed_init->username.value());
  ada_log("-- processed_init->password: ", processed_init->password.value());
  ada_log("-- processed_init->hostname: ", processed_init->hostname.value());
  ada_log("-- processed_init->port: ", processed_init->port.value());
  ada_log("-- processed_init->pathname: ", processed_init->pathname.value());
  ada_log("-- processed_init->search: ", processed_init->search.value());
  ada_log("-- processed_init->hash: ", processed_init->hash.value());

  // If processedInit["protocol"] is a special scheme and processedInit["port"]
  // is a string which represents its corresponding default port in radix-10
  // using ASCII digits then set processedInit["port"] to the empty string.
  // TODO: Optimization opportunity.
  if (scheme::is_special(*processed_init->protocol)) {
    std::string_view port = processed_init->port.value();
    helpers::trim_c0_whitespace(port);
    if (std::to_string(scheme::get_special_port(*processed_init->protocol)) ==
        port) {
      processed_init->port->clear();
    }
  }

  // Let urlPattern be a new URL pattern.
  url_pattern<regex_provider> url_pattern_{};

  // Set urlPattern’s protocol component to the result of compiling a component
  // given processedInit["protocol"], canonicalize a protocol, and default
  // options.
  auto protocol_component = url_pattern_component<regex_provider>::compile(
      processed_init->protocol.value(),
      url_pattern_helpers::canonicalize_protocol,
      url_pattern_compile_component_options::DEFAULT);
  if (!protocol_component) {
    ada_log("url_pattern_component::compile failed for protocol ",
            processed_init->protocol.value());
    return tl::unexpected(protocol_component.error());
  }
  url_pattern_.protocol_component = std::move(*protocol_component);

  // Set urlPattern’s username component to the result of compiling a component
  // given processedInit["username"], canonicalize a username, and default
  // options.
  auto username_component = url_pattern_component<regex_provider>::compile(
      processed_init->username.value(),
      url_pattern_helpers::canonicalize_username,
      url_pattern_compile_component_options::DEFAULT);
  if (!username_component) {
    ada_log("url_pattern_component::compile failed for username ",
            processed_init->username.value());
    return tl::unexpected(username_component.error());
  }
  url_pattern_.username_component = std::move(*username_component);

  // Set urlPattern’s password component to the result of compiling a component
  // given processedInit["password"], canonicalize a password, and default
  // options.
  auto password_component = url_pattern_component<regex_provider>::compile(
      processed_init->password.value(),
      url_pattern_helpers::canonicalize_password,
      url_pattern_compile_component_options::DEFAULT);
  if (!password_component) {
    ada_log("url_pattern_component::compile failed for password ",
            processed_init->password.value());
    return tl::unexpected(password_component.error());
  }
  url_pattern_.password_component = std::move(*password_component);

  // TODO: Optimization opportunity. The following if statement can be
  // simplified.
  // If the result running hostname pattern is an IPv6 address given
  // processedInit["hostname"] is true, then set urlPattern’s hostname component
  // to the result of compiling a component given processedInit["hostname"],
  // canonicalize an IPv6 hostname, and hostname options.
  if (url_pattern_helpers::is_ipv6_address(processed_init->hostname.value())) {
    ada_log("processed_init->hostname is ipv6 address");
    // then set urlPattern’s hostname component to the result of compiling a
    // component given processedInit["hostname"], canonicalize an IPv6 hostname,
    // and hostname options.
    auto hostname_component = url_pattern_component<regex_provider>::compile(
        processed_init->hostname.value(),
        url_pattern_helpers::canonicalize_ipv6_hostname,
        url_pattern_compile_component_options::DEFAULT);
    if (!hostname_component) {
      ada_log("url_pattern_component::compile failed for ipv6 hostname ",
              processed_init->hostname.value());
      return tl::unexpected(hostname_component.error());
    }
    url_pattern_.hostname_component = std::move(*hostname_component);
  } else {
    // Otherwise, set urlPattern’s hostname component to the result of compiling
    // a component given processedInit["hostname"], canonicalize a hostname, and
    // hostname options.
    auto hostname_component = url_pattern_component<regex_provider>::compile(
        processed_init->hostname.value(),
        url_pattern_helpers::canonicalize_hostname,
        url_pattern_compile_component_options::HOSTNAME);
    if (!hostname_component) {
      ada_log("url_pattern_component::compile failed for hostname ",
              processed_init->hostname.value());
      return tl::unexpected(hostname_component.error());
    }
    url_pattern_.hostname_component = std::move(*hostname_component);
  }

  // Set urlPattern’s port component to the result of compiling a component
  // given processedInit["port"], canonicalize a port, and default options.
  auto port_component = url_pattern_component<regex_provider>::compile(
      processed_init->port.value(), url_pattern_helpers::canonicalize_port,
      url_pattern_compile_component_options::DEFAULT);
  if (!port_component) {
    ada_log("url_pattern_component::compile failed for port ",
            processed_init->port.value());
    return tl::unexpected(port_component.error());
  }
  url_pattern_.port_component = std::move(*port_component);

  // Let compileOptions be a copy of the default options with the ignore case
  // property set to options["ignoreCase"].
  auto compile_options = url_pattern_compile_component_options::DEFAULT;
  if (options) {
    compile_options.ignore_case = options->ignore_case;
  }

  // TODO: Optimization opportunity: Simplify this if statement.
  // If the result of running protocol component matches a special scheme given
  // urlPattern’s protocol component is true, then:
  if (url_pattern_helpers::protocol_component_matches_special_scheme<
          regex_provider>(url_pattern_.protocol_component)) {
    // Let pathCompileOptions be copy of the pathname options with the ignore
    // case property set to options["ignoreCase"].
    auto path_compile_options = url_pattern_compile_component_options::PATHNAME;
    if (options) {
      path_compile_options.ignore_case = options->ignore_case;
    }

    // Set urlPattern’s pathname component to the result of compiling a
    // component given processedInit["pathname"], canonicalize a pathname, and
    // pathCompileOptions.
    auto pathname_component = url_pattern_component<regex_provider>::compile(
        processed_init->pathname.value(),
        url_pattern_helpers::canonicalize_pathname, path_compile_options);
    if (!pathname_component) {
      ada_log("url_pattern_component::compile failed for pathname ",
              processed_init->pathname.value());
      return tl::unexpected(pathname_component.error());
    }
    url_pattern_.pathname_component = std::move(*pathname_component);
  } else {
    // Otherwise set urlPattern’s pathname component to the result of compiling
    // a component given processedInit["pathname"], canonicalize an opaque
    // pathname, and compileOptions.
    auto pathname_component = url_pattern_component<regex_provider>::compile(
        processed_init->pathname.value(),
        url_pattern_helpers::canonicalize_opaque_pathname, compile_options);
    if (!pathname_component) {
      ada_log("url_pattern_component::compile failed for opaque pathname ",
              processed_init->pathname.value());
      return tl::unexpected(pathname_component.error());
    }
    url_pattern_.pathname_component = std::move(*pathname_component);
  }

  // Set urlPattern’s search component to the result of compiling a component
  // given processedInit["search"], canonicalize a search, and compileOptions.
  auto search_component = url_pattern_component<regex_provider>::compile(
      processed_init->search.value(), url_pattern_helpers::canonicalize_search,
      compile_options);
  if (!search_component) {
    ada_log("url_pattern_component::compile failed for search ",
            processed_init->search.value());
    return tl::unexpected(search_component.error());
  }
  url_pattern_.search_component = std::move(*search_component);

  // Set urlPattern’s hash component to the result of compiling a component
  // given processedInit["hash"], canonicalize a hash, and compileOptions.
  auto hash_component = url_pattern_component<regex_provider>::compile(
      processed_init->hash.value(), url_pattern_helpers::canonicalize_hash,
      compile_options);
  if (!hash_component) {
    ada_log("url_pattern_component::compile failed for hash ",
            processed_init->hash.value());
    return tl::unexpected(hash_component.error());
  }
  url_pattern_.hash_component = std::move(*hash_component);

  // Return urlPattern.
  return url_pattern_;
}

}  // namespace ada::parser

#endif  // ADA_PARSER_INL_H
/* end file include/ada/parser-inl.h */
/* begin file include/ada/scheme-inl.h */
/**
 * @file scheme-inl.h
 * @brief Definitions for the URL scheme.
 */
#ifndef ADA_SCHEME_INL_H
#define ADA_SCHEME_INL_H


namespace ada::scheme {

/**
 * @namespace ada::scheme::details
 * @brief Includes the definitions for scheme specific entities
 */
namespace details {
// for use with is_special and get_special_port
// Spaces, if present, are removed from URL.
constexpr std::string_view is_special_list[] = {"http", " ",   "https", "ws",
                                                "ftp",  "wss", "file",  " "};
// for use with get_special_port
constexpr uint16_t special_ports[] = {80, 0, 443, 80, 21, 443, 0, 0};
}  // namespace details

/****
 * @private
 * In is_special, get_scheme_type, and get_special_port, we
 * use a standard hashing technique to find the index of the scheme in
 * the is_special_list. The hashing technique is based on the size of
 * the scheme and the first character of the scheme. It ensures that we
 * do at most one string comparison per call. If the protocol is
 * predictible (e.g., it is always "http"), we can get a better average
 * performance by using a simpler approach where we loop and compare
 * scheme with all possible protocols starting with the most likely
 * protocol. Doing multiple comparisons may have a poor worst case
 * performance, however. In this instance, we choose a potentially
 * slightly lower best-case performance for a better worst-case
 * performance. We can revisit this choice at any time.
 *
 * Reference:
 * Schmidt, Douglas C. "Gperf: A perfect hash function generator."
 * More C++ gems 17 (2000).
 *
 * Reference: https://en.wikipedia.org/wiki/Perfect_hash_function
 *
 * Reference: https://github.com/ada-url/ada/issues/617
 ****/

ada_really_inline constexpr bool is_special(std::string_view scheme) {
  if (scheme.empty()) {
    return false;
  }
  int hash_value = (2 * scheme.size() + (unsigned)(scheme[0])) & 7;
  const std::string_view target = details::is_special_list[hash_value];
  return (target[0] == scheme[0]) && (target.substr(1) == scheme.substr(1));
}
constexpr uint16_t get_special_port(std::string_view scheme) noexcept {
  if (scheme.empty()) {
    return 0;
  }
  int hash_value = (2 * scheme.size() + (unsigned)(scheme[0])) & 7;
  const std::string_view target = details::is_special_list[hash_value];
  if ((target[0] == scheme[0]) && (target.substr(1) == scheme.substr(1))) {
    return details::special_ports[hash_value];
  } else {
    return 0;
  }
}
constexpr uint16_t get_special_port(ada::scheme::type type) noexcept {
  return details::special_ports[int(type)];
}
constexpr ada::scheme::type get_scheme_type(std::string_view scheme) noexcept {
  if (scheme.empty()) {
    return ada::scheme::NOT_SPECIAL;
  }
  int hash_value = (2 * scheme.size() + (unsigned)(scheme[0])) & 7;
  const std::string_view target = details::is_special_list[hash_value];
  if ((target[0] == scheme[0]) && (target.substr(1) == scheme.substr(1))) {
    return ada::scheme::type(hash_value);
  } else {
    return ada::scheme::NOT_SPECIAL;
  }
}

}  // namespace ada::scheme

#endif  // ADA_SCHEME_INL_H
/* end file include/ada/scheme-inl.h */
/* begin file include/ada/serializers.h */
/**
 * @file serializers.h
 * @brief Definitions for the URL serializers.
 */
#ifndef ADA_SERIALIZERS_H
#define ADA_SERIALIZERS_H


#include <array>
#include <string>

/**
 * @namespace ada::serializers
 * @brief Includes the definitions for URL serializers
 */
namespace ada::serializers {

/**
 * Finds and returns the longest sequence of 0 values in a ipv6 input.
 */
void find_longest_sequence_of_ipv6_pieces(
    const std::array<uint16_t, 8>& address, size_t& compress,
    size_t& compress_length) noexcept;

/**
 * Serializes an ipv6 address.
 * @details An IPv6 address is a 128-bit unsigned integer that identifies a
 * network address.
 * @see https://url.spec.whatwg.org/#concept-ipv6-serializer
 */
std::string ipv6(const std::array<uint16_t, 8>& address) noexcept;

/**
 * Serializes an ipv4 address.
 * @details An IPv4 address is a 32-bit unsigned integer that identifies a
 * network address.
 * @see https://url.spec.whatwg.org/#concept-ipv4-serializer
 */
std::string ipv4(uint64_t address) noexcept;

}  // namespace ada::serializers

#endif  // ADA_SERIALIZERS_H
/* end file include/ada/serializers.h */
/* begin file include/ada/state.h */
/**
 * @file state.h
 * @brief Definitions for the states of the URL state machine.
 */
#ifndef ADA_STATE_H
#define ADA_STATE_H


#include <string>

namespace ada {

/**
 * @see https://url.spec.whatwg.org/#url-parsing
 */
enum class state {
  /**
   * @see https://url.spec.whatwg.org/#authority-state
   */
  AUTHORITY,

  /**
   * @see https://url.spec.whatwg.org/#scheme-start-state
   */
  SCHEME_START,

  /**
   * @see https://url.spec.whatwg.org/#scheme-state
   */
  SCHEME,

  /**
   * @see https://url.spec.whatwg.org/#host-state
   */
  HOST,

  /**
   * @see https://url.spec.whatwg.org/#no-scheme-state
   */
  NO_SCHEME,

  /**
   * @see https://url.spec.whatwg.org/#fragment-state
   */
  FRAGMENT,

  /**
   * @see https://url.spec.whatwg.org/#relative-state
   */
  RELATIVE_SCHEME,

  /**
   * @see https://url.spec.whatwg.org/#relative-slash-state
   */
  RELATIVE_SLASH,

  /**
   * @see https://url.spec.whatwg.org/#file-state
   */
  FILE,

  /**
   * @see https://url.spec.whatwg.org/#file-host-state
   */
  FILE_HOST,

  /**
   * @see https://url.spec.whatwg.org/#file-slash-state
   */
  FILE_SLASH,

  /**
   * @see https://url.spec.whatwg.org/#path-or-authority-state
   */
  PATH_OR_AUTHORITY,

  /**
   * @see https://url.spec.whatwg.org/#special-authority-ignore-slashes-state
   */
  SPECIAL_AUTHORITY_IGNORE_SLASHES,

  /**
   * @see https://url.spec.whatwg.org/#special-authority-slashes-state
   */
  SPECIAL_AUTHORITY_SLASHES,

  /**
   * @see https://url.spec.whatwg.org/#special-relative-or-authority-state
   */
  SPECIAL_RELATIVE_OR_AUTHORITY,

  /**
   * @see https://url.spec.whatwg.org/#query-state
   */
  QUERY,

  /**
   * @see https://url.spec.whatwg.org/#path-state
   */
  PATH,

  /**
   * @see https://url.spec.whatwg.org/#path-start-state
   */
  PATH_START,

  /**
   * @see https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
   */
  OPAQUE_PATH,

  /**
   * @see https://url.spec.whatwg.org/#port-state
   */
  PORT,
};

/**
 * Stringify a URL state machine state.
 */
ada_warn_unused std::string to_string(ada::state s);

}  // namespace ada

#endif  // ADA_STATE_H
/* end file include/ada/state.h */
/* begin file include/ada/unicode.h */
/**
 * @file unicode.h
 * @brief Definitions for all unicode specific functions.
 */
#ifndef ADA_UNICODE_H
#define ADA_UNICODE_H


#include <string>
#include <string_view>
#include <optional>

/**
 * Unicode operations. These functions are not part of our public API and may
 * change at any time.
 *
 * @private
 * @namespace ada::unicode
 * @brief Includes the definitions for unicode operations
 */
namespace ada::unicode {

/**
 * @private
 * We receive a UTF-8 string representing a domain name.
 * If the string is percent encoded, we apply percent decoding.
 *
 * Given a domain, we need to identify its labels.
 * They are separated by label-separators:
 *
 * U+002E (.) FULL STOP
 * U+FF0E FULLWIDTH FULL STOP
 * U+3002 IDEOGRAPHIC FULL STOP
 * U+FF61 HALFWIDTH IDEOGRAPHIC FULL STOP
 *
 * They are all mapped to U+002E.
 *
 * We process each label into a string that should not exceed 63 octets.
 * If the string is already punycode (starts with "xn--"), then we must
 * scan it to look for unallowed code points.
 * Otherwise, if the string is not pure ASCII, we need to transcode it
 * to punycode by following RFC 3454 which requires us to
 * - Map characters  (see section 3),
 * - Normalize (see section 4),
 * - Reject forbidden characters,
 * - Check for right-to-left characters and if so, check all requirements (see
 * section 6),
 * - Optionally reject based on unassigned code points (section 7).
 *
 * The Unicode standard provides a table of code points with a mapping, a list
 * of forbidden code points and so forth. This table is subject to change and
 * will vary based on the implementation. For Unicode 15, the table is at
 * https://www.unicode.org/Public/idna/15.0.0/IdnaMappingTable.txt
 * If you use ICU, they parse this table and map it to code using a Python
 * script.
 *
 * The resulting strings should not exceed 255 octets according to RFC 1035
 * section 2.3.4. ICU checks for label size and domain size, but these errors
 * are ignored.
 *
 * @see https://url.spec.whatwg.org/#concept-domain-to-ascii
 *
 */
bool to_ascii(std::optional<std::string>& out, std::string_view plain,
              size_t first_percent);

/**
 * @private
 * Checks if the input has tab or newline characters.
 *
 * @attention The has_tabs_or_newline function is a bottleneck and it is simple
 * enough that compilers like GCC can 'autovectorize it'.
 */
ada_really_inline bool has_tabs_or_newline(
    std::string_view user_input) noexcept;

/**
 * @private
 * Checks if the input is a forbidden host code point.
 * @see https://url.spec.whatwg.org/#forbidden-host-code-point
 */
ada_really_inline constexpr bool is_forbidden_host_code_point(char c) noexcept;

/**
 * @private
 * Checks if the input contains a forbidden domain code point.
 * @see https://url.spec.whatwg.org/#forbidden-domain-code-point
 */
ada_really_inline constexpr bool contains_forbidden_domain_code_point(
    const char* input, size_t length) noexcept;

/**
 * @private
 * Checks if the input contains a forbidden domain code point in which case
 * the first bit is set to 1. If the input contains an upper case ASCII letter,
 * then the second bit is set to 1.
 * @see https://url.spec.whatwg.org/#forbidden-domain-code-point
 */
ada_really_inline constexpr uint8_t
contains_forbidden_domain_code_point_or_upper(const char* input,
                                              size_t length) noexcept;

/**
 * @private
 * Checks if the input is a forbidden domain code point.
 * @see https://url.spec.whatwg.org/#forbidden-domain-code-point
 */
ada_really_inline constexpr bool is_forbidden_domain_code_point(
    char c) noexcept;

/**
 * @private
 * Checks if the input is alphanumeric, '+', '-' or '.'
 */
ada_really_inline constexpr bool is_alnum_plus(char c) noexcept;

/**
 * @private
 * @details An ASCII hex digit is an ASCII upper hex digit or ASCII lower hex
 * digit. An ASCII upper hex digit is an ASCII digit or a code point in the
 * range U+0041 (A) to U+0046 (F), inclusive. An ASCII lower hex digit is an
 * ASCII digit or a code point in the range U+0061 (a) to U+0066 (f), inclusive.
 */
ada_really_inline constexpr bool is_ascii_hex_digit(char c) noexcept;

/**
 * @private
 * An ASCII digit is a code point in the range U+0030 (0) to U+0039 (9),
 * inclusive.
 */
ada_really_inline constexpr bool is_ascii_digit(char c) noexcept;

/**
 * @private
 * @details If a char is between U+0000 and U+007F inclusive, then it's an ASCII
 * character.
 */
ada_really_inline constexpr bool is_ascii(char32_t c) noexcept;

/**
 * @private
 * Checks if the input is a C0 control or space character.
 *
 * @details A C0 control or space is a C0 control or U+0020 SPACE.
 * A C0 control is a code point in the range U+0000 NULL to U+001F INFORMATION
 * SEPARATOR ONE, inclusive.
 */
ada_really_inline constexpr bool is_c0_control_or_space(char c) noexcept;

/**
 * @private
 * Checks if the input is a ASCII tab or newline character.
 *
 * @details An ASCII tab or newline is U+0009 TAB, U+000A LF, or U+000D CR.
 */
ada_really_inline constexpr bool is_ascii_tab_or_newline(char c) noexcept;

/**
 * @private
 * @details A double-dot path segment must be ".." or an ASCII case-insensitive
 * match for ".%2e", "%2e.", or "%2e%2e".
 */
ada_really_inline constexpr bool is_double_dot_path_segment(
    std::string_view input) noexcept;

/**
 * @private
 * @details A single-dot path segment must be "." or an ASCII case-insensitive
 * match for "%2e".
 */
ada_really_inline constexpr bool is_single_dot_path_segment(
    std::string_view input) noexcept;

/**
 * @private
 * @details ipv4 character might contain 0-9 or a-f character ranges.
 */
ada_really_inline constexpr bool is_lowercase_hex(char c) noexcept;

/**
 * @private
 * @details Convert hex to binary. Caller is responsible to ensure that
 * the parameter is an hexadecimal digit (0-9, A-F, a-f).
 */
ada_really_inline unsigned constexpr convert_hex_to_binary(char c) noexcept;

/**
 * @private
 * first_percent should be  = input.find('%')
 *
 * @todo It would be faster as noexcept maybe, but it could be unsafe since.
 * @author Node.js
 * @see https://github.com/nodejs/node/blob/main/src/node_url.cc#L245
 * @see https://encoding.spec.whatwg.org/#utf-8-decode-without-bom
 */
std::string percent_decode(std::string_view input, size_t first_percent);

/**
 * @private
 * Returns a percent-encoding string whether percent encoding was needed or not.
 * @see https://github.com/nodejs/node/blob/main/src/node_url.cc#L226
 */
std::string percent_encode(std::string_view input,
                           const uint8_t character_set[]);
/**
 * @private
 * Returns a percent-encoded string version of input, while starting the percent
 * encoding at the provided index.
 * @see https://github.com/nodejs/node/blob/main/src/node_url.cc#L226
 */
std::string percent_encode(std::string_view input,
                           const uint8_t character_set[], size_t index);
/**
 * @private
 * Returns true if percent encoding was needed, in which case, we store
 * the percent-encoded content in 'out'. If the boolean 'append' is set to
 * true, the content is appended to 'out'.
 * If percent encoding is not needed, out is left unchanged.
 * @see https://github.com/nodejs/node/blob/main/src/node_url.cc#L226
 */
template <bool append>
bool percent_encode(std::string_view input, const uint8_t character_set[],
                    std::string& out);
/**
 * @private
 * Returns the index at which percent encoding should start, or (equivalently),
 * the length of the prefix that does not require percent encoding.
 */
ada_really_inline size_t percent_encode_index(std::string_view input,
                                              const uint8_t character_set[]);
/**
 * @private
 * Lowers the string in-place, assuming that the content is ASCII.
 * Return true if the content was ASCII.
 */
constexpr bool to_lower_ascii(char* input, size_t length) noexcept;
}  // namespace ada::unicode

#endif  // ADA_UNICODE_H
/* end file include/ada/unicode.h */
/* begin file include/ada/url_base-inl.h */
/**
 * @file url_base-inl.h
 * @brief Inline functions for url base
 */
#ifndef ADA_URL_BASE_INL_H
#define ADA_URL_BASE_INL_H


#include <string>
#if ADA_REGULAR_VISUAL_STUDIO
#include <intrin.h>
#endif  // ADA_REGULAR_VISUAL_STUDIO

namespace ada {

[[nodiscard]] ada_really_inline constexpr bool url_base::is_special()
    const noexcept {
  return type != ada::scheme::NOT_SPECIAL;
}

[[nodiscard]] inline uint16_t url_base::get_special_port() const noexcept {
  return ada::scheme::get_special_port(type);
}

[[nodiscard]] ada_really_inline uint16_t
url_base::scheme_default_port() const noexcept {
  return scheme::get_special_port(type);
}

}  // namespace ada

#endif  // ADA_URL_BASE_INL_H
/* end file include/ada/url_base-inl.h */
/* begin file include/ada/url-inl.h */
/**
 * @file url-inl.h
 * @brief Definitions for the URL
 */
#ifndef ADA_URL_INL_H
#define ADA_URL_INL_H


#include <charconv>
#include <optional>
#include <string>
#if ADA_REGULAR_VISUAL_STUDIO
#include <intrin.h>
#endif  // ADA_REGULAR_VISUAL_STUDIO

namespace ada {
[[nodiscard]] ada_really_inline bool url::has_credentials() const noexcept {
  return !username.empty() || !password.empty();
}
[[nodiscard]] ada_really_inline bool url::has_port() const noexcept {
  return port.has_value();
}
[[nodiscard]] inline bool url::cannot_have_credentials_or_port() const {
  return !host.has_value() || host.value().empty() ||
         type == ada::scheme::type::FILE;
}
[[nodiscard]] inline bool url::has_empty_hostname() const noexcept {
  if (!host.has_value()) {
    return false;
  }
  return host.value().empty();
}
[[nodiscard]] inline bool url::has_hostname() const noexcept {
  return host.has_value();
}
inline std::ostream &operator<<(std::ostream &out, const ada::url &u) {
  return out << u.to_string();
}

[[nodiscard]] size_t url::get_pathname_length() const noexcept {
  return path.size();
}

[[nodiscard]] constexpr std::string_view url::get_pathname() const noexcept {
  return path;
}

[[nodiscard]] ada_really_inline ada::url_components url::get_components()
    const noexcept {
  url_components out{};

  // protocol ends with ':'. for example: "https:"
  out.protocol_end = uint32_t(get_protocol().size());

  // Trailing index is always the next character of the current one.
  size_t running_index = out.protocol_end;

  if (host.has_value()) {
    // 2 characters for "//" and 1 character for starting index
    out.host_start = out.protocol_end + 2;

    if (has_credentials()) {
      out.username_end = uint32_t(out.host_start + username.size());

      out.host_start += uint32_t(username.size());

      if (!password.empty()) {
        out.host_start += uint32_t(password.size() + 1);
      }

      out.host_end = uint32_t(out.host_start + host.value().size());
    } else {
      out.username_end = out.host_start;

      // Host does not start with "@" if it does not include credentials.
      out.host_end = uint32_t(out.host_start + host.value().size()) - 1;
    }

    running_index = out.host_end + 1;
  } else {
    // Update host start and end date to the same index, since it does not
    // exist.
    out.host_start = out.protocol_end;
    out.host_end = out.host_start;

    if (!has_opaque_path && path.starts_with("//")) {
      // If url's host is null, url does not have an opaque path, url's path's
      // size is greater than 1, and url's path[0] is the empty string, then
      // append U+002F (/) followed by U+002E (.) to output.
      running_index = out.protocol_end + 2;
    } else {
      running_index = out.protocol_end;
    }
  }

  if (port.has_value()) {
    out.port = *port;
    running_index += helpers::fast_digit_count(*port) + 1;  // Port omits ':'
  }

  out.pathname_start = uint32_t(running_index);

  running_index += path.size();

  if (query.has_value()) {
    out.search_start = uint32_t(running_index);
    running_index += get_search().size();
    if (get_search().empty()) {
      running_index++;
    }
  }

  if (hash.has_value()) {
    out.hash_start = uint32_t(running_index);
  }

  return out;
}

inline void url::update_base_hostname(std::string_view input) { host = input; }

inline void url::update_unencoded_base_hash(std::string_view input) {
  // We do the percent encoding
  hash = unicode::percent_encode(input,
                                 ada::character_sets::FRAGMENT_PERCENT_ENCODE);
}

inline void url::update_base_search(std::string_view input,
                                    const uint8_t query_percent_encode_set[]) {
  query = ada::unicode::percent_encode(input, query_percent_encode_set);
}

inline void url::update_base_search(std::optional<std::string> &&input) {
  query = std::move(input);
}

inline void url::update_base_pathname(const std::string_view input) {
  path = input;
}

inline void url::update_base_username(const std::string_view input) {
  username = input;
}

inline void url::update_base_password(const std::string_view input) {
  password = input;
}

inline void url::update_base_port(std::optional<uint16_t> input) {
  port = input;
}

constexpr void url::clear_pathname() { path.clear(); }

constexpr void url::clear_search() { query = std::nullopt; }

[[nodiscard]] constexpr bool url::has_hash() const noexcept {
  return hash.has_value();
}

[[nodiscard]] constexpr bool url::has_search() const noexcept {
  return query.has_value();
}

constexpr void url::set_protocol_as_file() { type = ada::scheme::type::FILE; }

inline void url::set_scheme(std::string &&new_scheme) noexcept {
  type = ada::scheme::get_scheme_type(new_scheme);
  // We only move the 'scheme' if it is non-special.
  if (!is_special()) {
    non_special_scheme = std::move(new_scheme);
  }
}

constexpr void url::copy_scheme(ada::url &&u) noexcept {
  non_special_scheme = u.non_special_scheme;
  type = u.type;
}

constexpr void url::copy_scheme(const ada::url &u) {
  non_special_scheme = u.non_special_scheme;
  type = u.type;
}

[[nodiscard]] ada_really_inline std::string url::get_href() const noexcept {
  std::string output = get_protocol();

  if (host.has_value()) {
    output += "//";
    if (has_credentials()) {
      output += username;
      if (!password.empty()) {
        output += ":" + get_password();
      }
      output += "@";
    }
    output += host.value();
    if (port.has_value()) {
      output += ":" + get_port();
    }
  } else if (!has_opaque_path && path.starts_with("//")) {
    // If url's host is null, url does not have an opaque path, url's path's
    // size is greater than 1, and url's path[0] is the empty string, then
    // append U+002F (/) followed by U+002E (.) to output.
    output += "/.";
  }
  output += path;
  if (query.has_value()) {
    output += "?" + query.value();
  }
  if (hash.has_value()) {
    output += "#" + hash.value();
  }
  return output;
}

ada_really_inline size_t url::parse_port(std::string_view view,
                                         bool check_trailing_content) noexcept {
  ada_log("parse_port('", view, "') ", view.size());
  if (!view.empty() && view[0] == '-') {
    ada_log("parse_port: view[0] == '0' && view.size() > 1");
    is_valid = false;
    return 0;
  }
  uint16_t parsed_port{};
  auto r = std::from_chars(view.data(), view.data() + view.size(), parsed_port);
  if (r.ec == std::errc::result_out_of_range) {
    ada_log("parse_port: r.ec == std::errc::result_out_of_range");
    is_valid = false;
    return 0;
  }
  ada_log("parse_port: ", parsed_port);
  const auto consumed = size_t(r.ptr - view.data());
  ada_log("parse_port: consumed ", consumed);
  if (check_trailing_content) {
    is_valid &=
        (consumed == view.size() || view[consumed] == '/' ||
         view[consumed] == '?' || (is_special() && view[consumed] == '\\'));
  }
  ada_log("parse_port: is_valid = ", is_valid);
  if (is_valid) {
    // scheme_default_port can return 0, and we should allow 0 as a base port.
    auto default_port = scheme_default_port();
    bool is_port_valid = (default_port == 0 && parsed_port == 0) ||
                         (default_port != parsed_port);
    port = (r.ec == std::errc() && is_port_valid) ? std::optional(parsed_port)
                                                  : std::nullopt;
  }
  return consumed;
}

}  // namespace ada

#endif  // ADA_URL_H
/* end file include/ada/url-inl.h */
/* begin file include/ada/url_components-inl.h */
/**
 * @file url_components.h
 * @brief Declaration for the URL Components
 */
#ifndef ADA_URL_COMPONENTS_INL_H
#define ADA_URL_COMPONENTS_INL_H


namespace ada {

[[nodiscard]] constexpr bool url_components::check_offset_consistency()
    const noexcept {
  /**
   * https://user:pass@example.com:1234/foo/bar?baz#quux
   *       |     |    |          | ^^^^|       |   |
   *       |     |    |          | |   |       |   `----- hash_start
   *       |     |    |          | |   |       `--------- search_start
   *       |     |    |          | |   `----------------- pathname_start
   *       |     |    |          | `--------------------- port
   *       |     |    |          `----------------------- host_end
   *       |     |    `---------------------------------- host_start
   *       |     `--------------------------------------- username_end
   *       `--------------------------------------------- protocol_end
   */
  // These conditions can be made more strict.
  if (protocol_end == url_components::omitted) {
    return false;
  }
  uint32_t index = protocol_end;

  if (username_end == url_components::omitted) {
    return false;
  }
  if (username_end < index) {
    return false;
  }
  index = username_end;

  if (host_start == url_components::omitted) {
    return false;
  }
  if (host_start < index) {
    return false;
  }
  index = host_start;

  if (port != url_components::omitted) {
    if (port > 0xffff) {
      return false;
    }
    uint32_t port_length = helpers::fast_digit_count(port) + 1;
    if (index + port_length < index) {
      return false;
    }
    index += port_length;
  }

  if (pathname_start == url_components::omitted) {
    return false;
  }
  if (pathname_start < index) {
    return false;
  }
  index = pathname_start;

  if (search_start != url_components::omitted) {
    if (search_start < index) {
      return false;
    }
    index = search_start;
  }

  if (hash_start != url_components::omitted) {
    if (hash_start < index) {
      return false;
    }
  }

  return true;
}

}  // namespace ada
#endif
/* end file include/ada/url_components-inl.h */
/* begin file include/ada/url_aggregator.h */
/**
 * @file url_aggregator.h
 * @brief Declaration for the basic URL definitions
 */
#ifndef ADA_URL_AGGREGATOR_H
#define ADA_URL_AGGREGATOR_H

#include <ostream>
#include <string>
#include <string_view>
#include <variant>


namespace ada {

namespace parser {}

/**
 * @brief Lightweight URL struct.
 *
 * @details The url_aggregator class aims to minimize temporary memory
 * allocation while representing a parsed URL. Internally, it contains a single
 * normalized URL (the href), and it makes available the components, mostly
 * using std::string_view.
 */
struct url_aggregator : url_base {
  url_aggregator() = default;
  url_aggregator(const url_aggregator &u) = default;
  url_aggregator(url_aggregator &&u) noexcept = default;
  url_aggregator &operator=(url_aggregator &&u) noexcept = default;
  url_aggregator &operator=(const url_aggregator &u) = default;
  ~url_aggregator() override = default;

  bool set_href(std::string_view input);
  bool set_host(std::string_view input);
  bool set_hostname(std::string_view input);
  bool set_protocol(std::string_view input);
  bool set_username(std::string_view input);
  bool set_password(std::string_view input);
  bool set_port(std::string_view input);
  bool set_pathname(std::string_view input);
  void set_search(std::string_view input);
  void set_hash(std::string_view input);

  [[nodiscard]] bool has_valid_domain() const noexcept override;
  /**
   * The origin getter steps are to return the serialization of this's URL's
   * origin. [HTML]
   * @return a newly allocated string.
   * @see https://url.spec.whatwg.org/#concept-url-origin
   */
  [[nodiscard]] std::string get_origin() const noexcept override;
  /**
   * Return the normalized string.
   * This function does not allocate memory.
   * It is highly efficient.
   * @return a constant reference to the underlying normalized URL.
   * @see https://url.spec.whatwg.org/#dom-url-href
   * @see https://url.spec.whatwg.org/#concept-url-serializer
   */
  [[nodiscard]] constexpr std::string_view get_href() const noexcept
      ada_lifetime_bound;
  /**
   * The username getter steps are to return this's URL's username.
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-username
   */
  [[nodiscard]] std::string_view get_username() const noexcept
      ada_lifetime_bound;
  /**
   * The password getter steps are to return this's URL's password.
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-password
   */
  [[nodiscard]] std::string_view get_password() const noexcept
      ada_lifetime_bound;
  /**
   * Return this's URL's port, serialized.
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-port
   */
  [[nodiscard]] std::string_view get_port() const noexcept ada_lifetime_bound;
  /**
   * Return U+0023 (#), followed by this's URL's fragment.
   * This function does not allocate memory.
   * @return a lightweight std::string_view..
   * @see https://url.spec.whatwg.org/#dom-url-hash
   */
  [[nodiscard]] std::string_view get_hash() const noexcept ada_lifetime_bound;
  /**
   * Return url's host, serialized, followed by U+003A (:) and url's port,
   * serialized.
   * This function does not allocate memory.
   * When there is no host, this function returns the empty view.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-host
   */
  [[nodiscard]] std::string_view get_host() const noexcept ada_lifetime_bound;
  /**
   * Return this's URL's host, serialized.
   * This function does not allocate memory.
   * When there is no host, this function returns the empty view.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-hostname
   */
  [[nodiscard]] std::string_view get_hostname() const noexcept
      ada_lifetime_bound;
  /**
   * The pathname getter steps are to return the result of URL path serializing
   * this's URL.
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-pathname
   */
  [[nodiscard]] constexpr std::string_view get_pathname() const noexcept
      ada_lifetime_bound;
  /**
   * Compute the pathname length in bytes without instantiating a view or a
   * string.
   * @return size of the pathname in bytes
   * @see https://url.spec.whatwg.org/#dom-url-pathname
   */
  [[nodiscard]] ada_really_inline uint32_t get_pathname_length() const noexcept;
  /**
   * Return U+003F (?), followed by this's URL's query.
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-search
   */
  [[nodiscard]] std::string_view get_search() const noexcept ada_lifetime_bound;
  /**
   * The protocol getter steps are to return this's URL's scheme, followed by
   * U+003A (:).
   * This function does not allocate memory.
   * @return a lightweight std::string_view.
   * @see https://url.spec.whatwg.org/#dom-url-protocol
   */
  [[nodiscard]] std::string_view get_protocol() const noexcept
      ada_lifetime_bound;

  /**
   * A URL includes credentials if its username or password is not the empty
   * string.
   */
  [[nodiscard]] ada_really_inline constexpr bool has_credentials()
      const noexcept;

  /**
   * Useful for implementing efficient serialization for the URL.
   *
   * https://user:pass@example.com:1234/foo/bar?baz#quux
   *       |     |    |          | ^^^^|       |   |
   *       |     |    |          | |   |       |   `----- hash_start
   *       |     |    |          | |   |       `--------- search_start
   *       |     |    |          | |   `----------------- pathname_start
   *       |     |    |          | `--------------------- port
   *       |     |    |          `----------------------- host_end
   *       |     |    `---------------------------------- host_start
   *       |     `--------------------------------------- username_end
   *       `--------------------------------------------- protocol_end
   *
   * Inspired after servo/url
   *
   * @return a constant reference to the underlying component attribute.
   *
   * @see
   * https://github.com/servo/rust-url/blob/b65a45515c10713f6d212e6726719a020203cc98/url/src/quirks.rs#L31
   */
  [[nodiscard]] ada_really_inline const url_components &get_components()
      const noexcept;
  /**
   * Returns a string representation of this URL.
   */
  [[nodiscard]] std::string to_string() const override;
  /**
   * Returns a string diagram of this URL.
   */
  [[nodiscard]] std::string to_diagram() const;

  /**
   * Verifies that the parsed URL could be valid. Useful for debugging purposes.
   * @return true if the URL is valid, otherwise return true of the offsets are
   * possible.
   */
  [[nodiscard]] constexpr bool validate() const noexcept;

  /** @return true if it has an host but it is the empty string */
  [[nodiscard]] constexpr bool has_empty_hostname() const noexcept;
  /** @return true if it has a host (included an empty host) */
  [[nodiscard]] constexpr bool has_hostname() const noexcept;
  /** @return true if the URL has a non-empty username */
  [[nodiscard]] constexpr bool has_non_empty_username() const noexcept;
  /** @return true if the URL has a non-empty password */
  [[nodiscard]] constexpr bool has_non_empty_password() const noexcept;
  /** @return true if the URL has a (non default) port */
  [[nodiscard]] constexpr bool has_port() const noexcept;
  /** @return true if the URL has a password */
  [[nodiscard]] constexpr bool has_password() const noexcept;
  /** @return true if the URL has a hash component */
  [[nodiscard]] constexpr bool has_hash() const noexcept override;
  /** @return true if the URL has a search component */
  [[nodiscard]] constexpr bool has_search() const noexcept override;

  inline void clear_port();
  inline void clear_hash();
  inline void clear_search() override;

 private:
  // helper methods
  friend void helpers::strip_trailing_spaces_from_opaque_path<url_aggregator>(
      url_aggregator &url) noexcept;
  // parse_url methods
  friend url_aggregator parser::parse_url<url_aggregator>(
      std::string_view, const url_aggregator *);

  friend url_aggregator parser::parse_url_impl<url_aggregator, true>(
      std::string_view, const url_aggregator *);
  friend url_aggregator parser::parse_url_impl<url_aggregator, false>(
      std::string_view, const url_aggregator *);
  // url_pattern methods
  template <url_pattern_regex::regex_concept regex_provider>
  friend tl::expected<url_pattern<regex_provider>, errors>
  parse_url_pattern_impl(std::variant<std::string_view, url_pattern_init> input,
                         const std::string_view *base_url,
                         const url_pattern_options *options);

  std::string buffer{};
  url_components components{};

  /**
   * Returns true if neither the search, nor the hash nor the pathname
   * have been set.
   * @return true if the buffer is ready to receive the path.
   */
  [[nodiscard]] ada_really_inline bool is_at_path() const noexcept;

  inline void add_authority_slashes_if_needed() noexcept;

  /**
   * To optimize performance, you may indicate how much memory to allocate
   * within this instance.
   */
  constexpr void reserve(uint32_t capacity);

  ada_really_inline size_t parse_port(
      std::string_view view, bool check_trailing_content) noexcept override;

  ada_really_inline size_t parse_port(std::string_view view) noexcept override {
    return this->parse_port(view, false);
  }

  /**
   * Return true on success. The 'in_place' parameter indicates whether the
   * the string_view input is pointing in the buffer. When in_place is false,
   * we must nearly always update the buffer.
   * @see https://url.spec.whatwg.org/#concept-ipv4-parser
   */
  [[nodiscard]] bool parse_ipv4(std::string_view input, bool in_place);

  /**
   * Return true on success.
   * @see https://url.spec.whatwg.org/#concept-ipv6-parser
   */
  [[nodiscard]] bool parse_ipv6(std::string_view input);

  /**
   * Return true on success.
   * @see https://url.spec.whatwg.org/#concept-opaque-host-parser
   */
  [[nodiscard]] bool parse_opaque_host(std::string_view input);

  ada_really_inline void parse_path(std::string_view input);

  /**
   * A URL cannot have a username/password/port if its host is null or the empty
   * string, or its scheme is "file".
   */
  [[nodiscard]] constexpr bool cannot_have_credentials_or_port() const;

  template <bool override_hostname = false>
  bool set_host_or_hostname(std::string_view input);

  ada_really_inline bool parse_host(std::string_view input);

  inline void update_base_authority(std::string_view base_buffer,
                                    const url_components &base);
  inline void update_unencoded_base_hash(std::string_view input);
  inline void update_base_hostname(std::string_view input);
  inline void update_base_search(std::string_view input);
  inline void update_base_search(std::string_view input,
                                 const uint8_t *query_percent_encode_set);
  inline void update_base_pathname(std::string_view input);
  inline void update_base_username(std::string_view input);
  inline void append_base_username(std::string_view input);
  inline void update_base_password(std::string_view input);
  inline void append_base_password(std::string_view input);
  inline void update_base_port(uint32_t input);
  inline void append_base_pathname(std::string_view input);
  [[nodiscard]] inline uint32_t retrieve_base_port() const;
  constexpr void clear_hostname();
  constexpr void clear_password();
  constexpr void clear_pathname() override;
  [[nodiscard]] constexpr bool has_dash_dot() const noexcept;
  void delete_dash_dot();
  inline void consume_prepared_path(std::string_view input);
  template <bool has_state_override = false>
  [[nodiscard]] ada_really_inline bool parse_scheme_with_colon(
      std::string_view input);
  ada_really_inline uint32_t replace_and_resize(uint32_t start, uint32_t end,
                                                std::string_view input);
  [[nodiscard]] constexpr bool has_authority() const noexcept;
  constexpr void set_protocol_as_file();
  inline void set_scheme(std::string_view new_scheme) noexcept;
  /**
   * Fast function to set the scheme from a view with a colon in the
   * buffer, does not change type.
   */
  inline void set_scheme_from_view_with_colon(
      std::string_view new_scheme_with_colon) noexcept;
  inline void copy_scheme(const url_aggregator &u) noexcept;

  inline void update_host_to_base_host(const std::string_view input) noexcept;

};  // url_aggregator

inline std::ostream &operator<<(std::ostream &out, const url &u);
}  // namespace ada

#endif
/* end file include/ada/url_aggregator.h */
/* begin file include/ada/url_aggregator-inl.h */
/**
 * @file url_aggregator-inl.h
 * @brief Inline functions for url aggregator
 */
#ifndef ADA_URL_AGGREGATOR_INL_H
#define ADA_URL_AGGREGATOR_INL_H

/* begin file include/ada/unicode-inl.h */
/**
 * @file unicode-inl.h
 * @brief Definitions for unicode operations.
 */
#ifndef ADA_UNICODE_INL_H
#define ADA_UNICODE_INL_H

/**
 * Unicode operations. These functions are not part of our public API and may
 * change at any time.
 *
 * private
 * @namespace ada::unicode
 * @brief Includes the declarations for unicode operations
 */
namespace ada::unicode {
ada_really_inline size_t percent_encode_index(const std::string_view input,
                                              const uint8_t character_set[]) {
  const char* data = input.data();
  const size_t size = input.size();

  // Process 8 bytes at a time using unrolled loop
  size_t i = 0;
  for (; i + 8 <= size; i += 8) {
    unsigned char chunk[8];
    std::memcpy(&chunk, data + i,
                8);  // entices compiler to unconditionally process 8 characters

    // Check 8 characters at once
    for (size_t j = 0; j < 8; j++) {
      if (character_sets::bit_at(character_set, chunk[j])) {
        return i + j;
      }
    }
  }

  // Handle remaining bytes
  for (; i < size; i++) {
    if (character_sets::bit_at(character_set, data[i])) {
      return i;
    }
  }

  return size;
}
}  // namespace ada::unicode

#endif  // ADA_UNICODE_INL_H
/* end file include/ada/unicode-inl.h */

#include <charconv>
#include <ostream>
#include <string_view>

namespace ada {

inline void url_aggregator::update_base_authority(
    std::string_view base_buffer, const ada::url_components &base) {
  std::string_view input = base_buffer.substr(
      base.protocol_end, base.host_start - base.protocol_end);
  ada_log("url_aggregator::update_base_authority ", input);

  bool input_starts_with_dash = input.starts_with("//");
  uint32_t diff = components.host_start - components.protocol_end;

  buffer.erase(components.protocol_end,
               components.host_start - components.protocol_end);
  components.username_end = components.protocol_end;

  if (input_starts_with_dash) {
    input.remove_prefix(2);
    diff += 2;  // add "//"
    buffer.insert(components.protocol_end, "//");
    components.username_end += 2;
  }

  size_t password_delimiter = input.find(':');

  // Check if input contains both username and password by checking the
  // delimiter: ":" A typical input that contains authority would be "user:pass"
  if (password_delimiter != std::string_view::npos) {
    // Insert both username and password
    std::string_view username = input.substr(0, password_delimiter);
    std::string_view password = input.substr(password_delimiter + 1);

    buffer.insert(components.protocol_end + diff, username);
    diff += uint32_t(username.size());
    buffer.insert(components.protocol_end + diff, ":");
    components.username_end = components.protocol_end + diff;
    buffer.insert(components.protocol_end + diff + 1, password);
    diff += uint32_t(password.size()) + 1;
  } else if (!input.empty()) {
    // Insert only username
    buffer.insert(components.protocol_end + diff, input);
    components.username_end =
        components.protocol_end + diff + uint32_t(input.size());
    diff += uint32_t(input.size());
  }

  components.host_start += diff;

  if (buffer.size() > base.host_start && buffer[base.host_start] != '@') {
    buffer.insert(components.host_start, "@");
    diff++;
  }
  components.host_end += diff;
  components.pathname_start += diff;
  if (components.search_start != url_components::omitted) {
    components.search_start += diff;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += diff;
  }
}

inline void url_aggregator::update_unencoded_base_hash(std::string_view input) {
  ada_log("url_aggregator::update_unencoded_base_hash ", input, " [",
          input.size(), " bytes], buffer is '", buffer, "' [", buffer.size(),
          " bytes] components.hash_start = ", components.hash_start);
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
  if (components.hash_start != url_components::omitted) {
    buffer.resize(components.hash_start);
  }
  components.hash_start = uint32_t(buffer.size());
  buffer += "#";
  bool encoding_required = unicode::percent_encode<true>(
      input, ada::character_sets::FRAGMENT_PERCENT_ENCODE, buffer);
  // When encoding_required is false, then buffer is left unchanged, and percent
  // encoding was not deemed required.
  if (!encoding_required) {
    buffer.append(input);
  }
  ada_log("url_aggregator::update_unencoded_base_hash final buffer is '",
          buffer, "' [", buffer.size(), " bytes]");
  ADA_ASSERT_TRUE(validate());
}

ada_really_inline uint32_t url_aggregator::replace_and_resize(
    uint32_t start, uint32_t end, std::string_view input) {
  uint32_t current_length = end - start;
  uint32_t input_size = uint32_t(input.size());
  uint32_t new_difference = input_size - current_length;

  if (current_length == 0) {
    buffer.insert(start, input);
  } else if (input_size == current_length) {
    buffer.replace(start, input_size, input);
  } else if (input_size < current_length) {
    buffer.erase(start, current_length - input_size);
    buffer.replace(start, input_size, input);
  } else {
    buffer.replace(start, current_length, input.substr(0, current_length));
    buffer.insert(start + current_length, input.substr(current_length));
  }

  return new_difference;
}

inline void url_aggregator::update_base_hostname(const std::string_view input) {
  ada_log("url_aggregator::update_base_hostname ", input, " [", input.size(),
          " bytes], buffer is '", buffer, "' [", buffer.size(), " bytes]");
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));

  // This next line is required for when parsing a URL like `foo://`
  add_authority_slashes_if_needed();

  bool has_credentials = components.protocol_end + 2 < components.host_start;
  uint32_t new_difference =
      replace_and_resize(components.host_start, components.host_end, input);

  if (has_credentials) {
    buffer.insert(components.host_start, "@");
    new_difference++;
  }
  components.host_end += new_difference;
  components.pathname_start += new_difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += new_difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += new_difference;
  }
  ADA_ASSERT_TRUE(validate());
}

[[nodiscard]] ada_really_inline uint32_t
url_aggregator::get_pathname_length() const noexcept {
  ada_log("url_aggregator::get_pathname_length");
  uint32_t ending_index = uint32_t(buffer.size());
  if (components.search_start != url_components::omitted) {
    ending_index = components.search_start;
  } else if (components.hash_start != url_components::omitted) {
    ending_index = components.hash_start;
  }
  return ending_index - components.pathname_start;
}

[[nodiscard]] ada_really_inline bool url_aggregator::is_at_path()
    const noexcept {
  return buffer.size() == components.pathname_start;
}

inline void url_aggregator::update_base_search(std::string_view input) {
  ada_log("url_aggregator::update_base_search ", input);
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
  if (input.empty()) {
    clear_search();
    return;
  }

  if (input[0] == '?') {
    input.remove_prefix(1);
  }

  if (components.hash_start == url_components::omitted) {
    if (components.search_start == url_components::omitted) {
      components.search_start = uint32_t(buffer.size());
      buffer += "?";
    } else {
      buffer.resize(components.search_start + 1);
    }

    buffer.append(input);
  } else {
    if (components.search_start == url_components::omitted) {
      components.search_start = components.hash_start;
    } else {
      buffer.erase(components.search_start,
                   components.hash_start - components.search_start);
      components.hash_start = components.search_start;
    }

    buffer.insert(components.search_start, "?");
    buffer.insert(components.search_start + 1, input);
    components.hash_start += uint32_t(input.size() + 1);  // Do not forget `?`
  }

  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::update_base_search(
    std::string_view input, const uint8_t query_percent_encode_set[]) {
  ada_log("url_aggregator::update_base_search ", input,
          " with encoding parameter ", to_string(), "\n", to_diagram());
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));

  if (components.hash_start == url_components::omitted) {
    if (components.search_start == url_components::omitted) {
      components.search_start = uint32_t(buffer.size());
      buffer += "?";
    } else {
      buffer.resize(components.search_start + 1);
    }

    bool encoding_required =
        unicode::percent_encode<true>(input, query_percent_encode_set, buffer);
    // When encoding_required is false, then buffer is left unchanged, and
    // percent encoding was not deemed required.
    if (!encoding_required) {
      buffer.append(input);
    }
  } else {
    if (components.search_start == url_components::omitted) {
      components.search_start = components.hash_start;
    } else {
      buffer.erase(components.search_start,
                   components.hash_start - components.search_start);
      components.hash_start = components.search_start;
    }

    buffer.insert(components.search_start, "?");
    size_t idx =
        ada::unicode::percent_encode_index(input, query_percent_encode_set);
    if (idx == input.size()) {
      buffer.insert(components.search_start + 1, input);
      components.hash_start += uint32_t(input.size() + 1);  // Do not forget `?`
    } else {
      buffer.insert(components.search_start + 1, input, 0, idx);
      input.remove_prefix(idx);
      // We only create a temporary string if we need percent encoding and
      // we attempt to create as small a temporary string as we can.
      std::string encoded =
          ada::unicode::percent_encode(input, query_percent_encode_set);
      buffer.insert(components.search_start + idx + 1, encoded);
      components.hash_start +=
          uint32_t(encoded.size() + idx + 1);  // Do not forget `?`
    }
  }

  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::update_base_pathname(const std::string_view input) {
  ada_log("url_aggregator::update_base_pathname '", input, "' [", input.size(),
          " bytes] \n", to_diagram());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
  ADA_ASSERT_TRUE(validate());

  const bool begins_with_dashdash = input.starts_with("//");
  if (!begins_with_dashdash && has_dash_dot()) {
    // We must delete the ./
    delete_dash_dot();
  }

  if (begins_with_dashdash && !has_opaque_path && !has_authority() &&
      !has_dash_dot()) {
    // If url's host is null, url does not have an opaque path, url's path's
    // size is greater than 1, then append U+002F (/) followed by U+002E (.) to
    // output.
    buffer.insert(components.pathname_start, "/.");
    components.pathname_start += 2;
  }

  uint32_t difference = replace_and_resize(
      components.pathname_start,
      components.pathname_start + get_pathname_length(), input);
  if (components.search_start != url_components::omitted) {
    components.search_start += difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += difference;
  }
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::append_base_pathname(const std::string_view input) {
  ada_log("url_aggregator::append_base_pathname ", input, " ", to_string(),
          "\n", to_diagram());
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
#if ADA_DEVELOPMENT_CHECKS
  // computing the expected password.
  std::string path_expected(get_pathname());
  path_expected.append(input);
#endif  // ADA_DEVELOPMENT_CHECKS
  uint32_t ending_index = uint32_t(buffer.size());
  if (components.search_start != url_components::omitted) {
    ending_index = components.search_start;
  } else if (components.hash_start != url_components::omitted) {
    ending_index = components.hash_start;
  }
  buffer.insert(ending_index, input);

  if (components.search_start != url_components::omitted) {
    components.search_start += uint32_t(input.size());
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += uint32_t(input.size());
  }
#if ADA_DEVELOPMENT_CHECKS
  std::string path_after = std::string(get_pathname());
  ADA_ASSERT_EQUAL(
      path_expected, path_after,
      "append_base_pathname problem after inserting " + std::string(input));
#endif  // ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::update_base_username(const std::string_view input) {
  ada_log("url_aggregator::update_base_username '", input, "' ", to_string(),
          "\n", to_diagram());
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));

  add_authority_slashes_if_needed();

  bool has_password = has_non_empty_password();
  bool host_starts_with_at = buffer.size() > components.host_start &&
                             buffer[components.host_start] == '@';
  uint32_t diff = replace_and_resize(components.protocol_end + 2,
                                     components.username_end, input);

  components.username_end += diff;
  components.host_start += diff;

  if (!input.empty() && !host_starts_with_at) {
    buffer.insert(components.host_start, "@");
    diff++;
  } else if (input.empty() && host_starts_with_at && !has_password) {
    // Input is empty, there is no password, and we need to remove "@" from
    // hostname
    buffer.erase(components.host_start, 1);
    diff--;
  }

  components.host_end += diff;
  components.pathname_start += diff;
  if (components.search_start != url_components::omitted) {
    components.search_start += diff;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += diff;
  }
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::append_base_username(const std::string_view input) {
  ada_log("url_aggregator::append_base_username ", input);
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
#if ADA_DEVELOPMENT_CHECKS
  // computing the expected password.
  std::string username_expected(get_username());
  username_expected.append(input);
#endif  // ADA_DEVELOPMENT_CHECKS
  add_authority_slashes_if_needed();

  // If input is empty, do nothing.
  if (input.empty()) {
    return;
  }

  uint32_t difference = uint32_t(input.size());
  buffer.insert(components.username_end, input);
  components.username_end += difference;
  components.host_start += difference;

  if (buffer[components.host_start] != '@' &&
      components.host_start != components.host_end) {
    buffer.insert(components.host_start, "@");
    difference++;
  }

  components.host_end += difference;
  components.pathname_start += difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += difference;
  }
#if ADA_DEVELOPMENT_CHECKS
  std::string username_after(get_username());
  ADA_ASSERT_EQUAL(
      username_expected, username_after,
      "append_base_username problem after inserting " + std::string(input));
#endif  // ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_TRUE(validate());
}

constexpr void url_aggregator::clear_password() {
  ada_log("url_aggregator::clear_password ", to_string());
  ADA_ASSERT_TRUE(validate());
  if (!has_password()) {
    return;
  }

  uint32_t diff = components.host_start - components.username_end;
  buffer.erase(components.username_end, diff);
  components.host_start -= diff;
  components.host_end -= diff;
  components.pathname_start -= diff;
  if (components.search_start != url_components::omitted) {
    components.search_start -= diff;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start -= diff;
  }
}

inline void url_aggregator::update_base_password(const std::string_view input) {
  ada_log("url_aggregator::update_base_password ", input);
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));

  add_authority_slashes_if_needed();

  // TODO: Optimization opportunity. Merge the following removal functions.
  if (input.empty()) {
    clear_password();

    // Remove username too, if it is empty.
    if (!has_non_empty_username()) {
      update_base_username("");
    }

    return;
  }

  bool password_exists = has_password();
  uint32_t difference = uint32_t(input.size());

  if (password_exists) {
    uint32_t current_length =
        components.host_start - components.username_end - 1;
    buffer.erase(components.username_end + 1, current_length);
    difference -= current_length;
  } else {
    buffer.insert(components.username_end, ":");
    difference++;
  }

  buffer.insert(components.username_end + 1, input);
  components.host_start += difference;

  // The following line is required to add "@" to hostname. When updating
  // password if hostname does not start with "@", it is "update_base_password"s
  // responsibility to set it.
  if (buffer[components.host_start] != '@') {
    buffer.insert(components.host_start, "@");
    difference++;
  }

  components.host_end += difference;
  components.pathname_start += difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += difference;
  }
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::append_base_password(const std::string_view input) {
  ada_log("url_aggregator::append_base_password ", input, " ", to_string(),
          "\n", to_diagram());
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
#if ADA_DEVELOPMENT_CHECKS
  // computing the expected password.
  std::string password_expected = std::string(get_password());
  password_expected.append(input);
#endif  // ADA_DEVELOPMENT_CHECKS
  add_authority_slashes_if_needed();

  // If input is empty, do nothing.
  if (input.empty()) {
    return;
  }

  uint32_t difference = uint32_t(input.size());
  if (has_password()) {
    buffer.insert(components.host_start, input);
  } else {
    difference++;  // Increment for ":"
    buffer.insert(components.username_end, ":");
    buffer.insert(components.username_end + 1, input);
  }
  components.host_start += difference;

  // The following line is required to add "@" to hostname. When updating
  // password if hostname does not start with "@", it is "append_base_password"s
  // responsibility to set it.
  if (buffer[components.host_start] != '@') {
    buffer.insert(components.host_start, "@");
    difference++;
  }

  components.host_end += difference;
  components.pathname_start += difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += difference;
  }
#if ADA_DEVELOPMENT_CHECKS
  std::string password_after(get_password());
  ADA_ASSERT_EQUAL(
      password_expected, password_after,
      "append_base_password problem after inserting " + std::string(input));
#endif  // ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::update_base_port(uint32_t input) {
  ada_log("url_aggregator::update_base_port");
  ADA_ASSERT_TRUE(validate());
  if (input == url_components::omitted) {
    clear_port();
    return;
  }
  // calling std::to_string(input.value()) is unfortunate given that the port
  // value is probably already available as a string.
  std::string value = helpers::concat(":", std::to_string(input));
  uint32_t difference = uint32_t(value.size());

  if (components.port != url_components::omitted) {
    difference -= components.pathname_start - components.host_end;
    buffer.erase(components.host_end,
                 components.pathname_start - components.host_end);
  }

  buffer.insert(components.host_end, value);
  components.pathname_start += difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += difference;
  }
  components.port = input;
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::clear_port() {
  ada_log("url_aggregator::clear_port");
  ADA_ASSERT_TRUE(validate());
  if (components.port == url_components::omitted) {
    return;
  }
  uint32_t length = components.pathname_start - components.host_end;
  buffer.erase(components.host_end, length);
  components.pathname_start -= length;
  if (components.search_start != url_components::omitted) {
    components.search_start -= length;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start -= length;
  }
  components.port = url_components::omitted;
  ADA_ASSERT_TRUE(validate());
}

[[nodiscard]] inline uint32_t url_aggregator::retrieve_base_port() const {
  ada_log("url_aggregator::retrieve_base_port");
  return components.port;
}

inline void url_aggregator::clear_search() {
  ada_log("url_aggregator::clear_search");
  ADA_ASSERT_TRUE(validate());
  if (components.search_start == url_components::omitted) {
    return;
  }

  if (components.hash_start == url_components::omitted) {
    buffer.resize(components.search_start);
  } else {
    buffer.erase(components.search_start,
                 components.hash_start - components.search_start);
    components.hash_start = components.search_start;
  }

  components.search_start = url_components::omitted;

#if ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_EQUAL(get_search(), "",
                   "search should have been cleared on buffer=" + buffer +
                       " with " + components.to_string() + "\n" + to_diagram());
#endif
  ADA_ASSERT_TRUE(validate());
}

inline void url_aggregator::clear_hash() {
  ada_log("url_aggregator::clear_hash");
  ADA_ASSERT_TRUE(validate());
  if (components.hash_start == url_components::omitted) {
    return;
  }
  buffer.resize(components.hash_start);
  components.hash_start = url_components::omitted;

#if ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_EQUAL(get_hash(), "",
                   "hash should have been cleared on buffer=" + buffer +
                       " with " + components.to_string() + "\n" + to_diagram());
#endif
  ADA_ASSERT_TRUE(validate());
}

constexpr void url_aggregator::clear_pathname() {
  ada_log("url_aggregator::clear_pathname");
  ADA_ASSERT_TRUE(validate());
  uint32_t ending_index = uint32_t(buffer.size());
  if (components.search_start != url_components::omitted) {
    ending_index = components.search_start;
  } else if (components.hash_start != url_components::omitted) {
    ending_index = components.hash_start;
  }
  uint32_t pathname_length = ending_index - components.pathname_start;
  buffer.erase(components.pathname_start, pathname_length);
  uint32_t difference = pathname_length;
  if (components.pathname_start == components.host_end + 2 &&
      buffer[components.host_end] == '/' &&
      buffer[components.host_end + 1] == '.') {
    components.pathname_start -= 2;
    buffer.erase(components.host_end, 2);
    difference += 2;
  }
  if (components.search_start != url_components::omitted) {
    components.search_start -= difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start -= difference;
  }
  ada_log("url_aggregator::clear_pathname completed, running checks...");
#if ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_EQUAL(get_pathname(), "",
                   "pathname should have been cleared on buffer=" + buffer +
                       " with " + components.to_string() + "\n" + to_diagram());
#endif
  ADA_ASSERT_TRUE(validate());
  ada_log("url_aggregator::clear_pathname completed, running checks... ok");
}

constexpr void url_aggregator::clear_hostname() {
  ada_log("url_aggregator::clear_hostname");
  ADA_ASSERT_TRUE(validate());
  if (!has_authority()) {
    return;
  }
  ADA_ASSERT_TRUE(has_authority());

  uint32_t hostname_length = components.host_end - components.host_start;
  uint32_t start = components.host_start;

  // If hostname starts with "@", we should not remove that character.
  if (hostname_length > 0 && buffer[start] == '@') {
    start++;
    hostname_length--;
  }
  buffer.erase(start, hostname_length);
  components.host_end = start;
  components.pathname_start -= hostname_length;
  if (components.search_start != url_components::omitted) {
    components.search_start -= hostname_length;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start -= hostname_length;
  }
#if ADA_DEVELOPMENT_CHECKS
  ADA_ASSERT_EQUAL(get_hostname(), "",
                   "hostname should have been cleared on buffer=" + buffer +
                       " with " + components.to_string() + "\n" + to_diagram());
#endif
  ADA_ASSERT_TRUE(has_authority());
  ADA_ASSERT_EQUAL(has_empty_hostname(), true,
                   "hostname should have been cleared on buffer=" + buffer +
                       " with " + components.to_string() + "\n" + to_diagram());
  ADA_ASSERT_TRUE(validate());
}

[[nodiscard]] constexpr bool url_aggregator::has_hash() const noexcept {
  ada_log("url_aggregator::has_hash");
  return components.hash_start != url_components::omitted;
}

[[nodiscard]] constexpr bool url_aggregator::has_search() const noexcept {
  ada_log("url_aggregator::has_search");
  return components.search_start != url_components::omitted;
}

constexpr bool url_aggregator::has_credentials() const noexcept {
  ada_log("url_aggregator::has_credentials");
  return has_non_empty_username() || has_non_empty_password();
}

constexpr bool url_aggregator::cannot_have_credentials_or_port() const {
  ada_log("url_aggregator::cannot_have_credentials_or_port");
  return type == ada::scheme::type::FILE ||
         components.host_start == components.host_end;
}

[[nodiscard]] ada_really_inline const ada::url_components &
url_aggregator::get_components() const noexcept {
  return components;
}

[[nodiscard]] constexpr bool ada::url_aggregator::has_authority()
    const noexcept {
  ada_log("url_aggregator::has_authority");
  // Performance: instead of doing this potentially expensive check, we could
  // have a boolean in the struct.
  return components.protocol_end + 2 <= components.host_start &&
         helpers::substring(buffer, components.protocol_end,
                            components.protocol_end + 2) == "//";
}

inline void ada::url_aggregator::add_authority_slashes_if_needed() noexcept {
  ada_log("url_aggregator::add_authority_slashes_if_needed");
  ADA_ASSERT_TRUE(validate());
  // Protocol setter will insert `http:` to the URL. It is up to hostname setter
  // to insert
  // `//` initially to the buffer, since it depends on the hostname existence.
  if (has_authority()) {
    return;
  }
  // Performance: the common case is components.protocol_end == buffer.size()
  // Optimization opportunity: in many cases, the "//" is part of the input and
  // the insert could be fused with another insert.
  buffer.insert(components.protocol_end, "//");
  components.username_end += 2;
  components.host_start += 2;
  components.host_end += 2;
  components.pathname_start += 2;
  if (components.search_start != url_components::omitted) {
    components.search_start += 2;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += 2;
  }
  ADA_ASSERT_TRUE(validate());
}

constexpr void ada::url_aggregator::reserve(uint32_t capacity) {
  buffer.reserve(capacity);
}

constexpr bool url_aggregator::has_non_empty_username() const noexcept {
  ada_log("url_aggregator::has_non_empty_username");
  return components.protocol_end + 2 < components.username_end;
}

constexpr bool url_aggregator::has_non_empty_password() const noexcept {
  ada_log("url_aggregator::has_non_empty_password");
  return components.host_start - components.username_end > 0;
}

constexpr bool url_aggregator::has_password() const noexcept {
  ada_log("url_aggregator::has_password");
  // This function does not care about the length of the password
  return components.host_start > components.username_end &&
         buffer[components.username_end] == ':';
}

constexpr bool url_aggregator::has_empty_hostname() const noexcept {
  if (!has_hostname()) {
    return false;
  }
  if (components.host_start == components.host_end) {
    return true;
  }
  if (components.host_end > components.host_start + 1) {
    return false;
  }
  return components.username_end != components.host_start;
}

constexpr bool url_aggregator::has_hostname() const noexcept {
  return has_authority();
}

constexpr bool url_aggregator::has_port() const noexcept {
  ada_log("url_aggregator::has_port");
  // A URL cannot have a username/password/port if its host is null or the empty
  // string, or its scheme is "file".
  return has_hostname() && components.pathname_start != components.host_end;
}

[[nodiscard]] constexpr bool url_aggregator::has_dash_dot() const noexcept {
  // If url's host is null, url does not have an opaque path, url's path's size
  // is greater than 1, and url's path[0] is the empty string, then append
  // U+002F (/) followed by U+002E (.) to output.
  ada_log("url_aggregator::has_dash_dot");
#if ADA_DEVELOPMENT_CHECKS
  // If pathname_start and host_end are exactly two characters apart, then we
  // either have a one-digit port such as http://test.com:5?param=1 or else we
  // have a /.: sequence such as "non-spec:/.//". We test that this is the case.
  if (components.pathname_start == components.host_end + 2) {
    ADA_ASSERT_TRUE((buffer[components.host_end] == '/' &&
                     buffer[components.host_end + 1] == '.') ||
                    (buffer[components.host_end] == ':' &&
                     checkers::is_digit(buffer[components.host_end + 1])));
  }
  if (components.pathname_start == components.host_end + 2 &&
      buffer[components.host_end] == '/' &&
      buffer[components.host_end + 1] == '.') {
    ADA_ASSERT_TRUE(components.pathname_start + 1 < buffer.size());
    ADA_ASSERT_TRUE(buffer[components.pathname_start] == '/');
    ADA_ASSERT_TRUE(buffer[components.pathname_start + 1] == '/');
  }
#endif
  // Performance: it should be uncommon for components.pathname_start ==
  // components.host_end + 2 to be true. So we put this check first in the
  // sequence. Most times, we do not have an opaque path. Checking for '/.' is
  // more expensive, but should be uncommon.
  return components.pathname_start == components.host_end + 2 &&
         !has_opaque_path && buffer[components.host_end] == '/' &&
         buffer[components.host_end + 1] == '.';
}

[[nodiscard]] constexpr std::string_view url_aggregator::get_href()
    const noexcept ada_lifetime_bound {
  ada_log("url_aggregator::get_href");
  return buffer;
}

ada_really_inline size_t url_aggregator::parse_port(
    std::string_view view, bool check_trailing_content) noexcept {
  ada_log("url_aggregator::parse_port('", view, "') ", view.size());
  if (!view.empty() && view[0] == '-') {
    ada_log("parse_port: view[0] == '0' && view.size() > 1");
    is_valid = false;
    return 0;
  }
  uint16_t parsed_port{};
  auto r = std::from_chars(view.data(), view.data() + view.size(), parsed_port);
  if (r.ec == std::errc::result_out_of_range) {
    ada_log("parse_port: r.ec == std::errc::result_out_of_range");
    is_valid = false;
    return 0;
  }
  ada_log("parse_port: ", parsed_port);
  const size_t consumed = size_t(r.ptr - view.data());
  ada_log("parse_port: consumed ", consumed);
  if (check_trailing_content) {
    is_valid &=
        (consumed == view.size() || view[consumed] == '/' ||
         view[consumed] == '?' || (is_special() && view[consumed] == '\\'));
  }
  ada_log("parse_port: is_valid = ", is_valid);
  if (is_valid) {
    ada_log("parse_port", r.ec == std::errc());
    // scheme_default_port can return 0, and we should allow 0 as a base port.
    auto default_port = scheme_default_port();
    bool is_port_valid = (default_port == 0 && parsed_port == 0) ||
                         (default_port != parsed_port);
    if (r.ec == std::errc() && is_port_valid) {
      update_base_port(parsed_port);
    } else {
      clear_port();
    }
  }
  return consumed;
}

constexpr void url_aggregator::set_protocol_as_file() {
  ada_log("url_aggregator::set_protocol_as_file ");
  ADA_ASSERT_TRUE(validate());
  type = ada::scheme::type::FILE;
  // next line could overflow but unsigned arithmetic has well-defined
  // overflows.
  uint32_t new_difference = 5 - components.protocol_end;

  if (buffer.empty()) {
    buffer.append("file:");
  } else {
    buffer.erase(0, components.protocol_end);
    buffer.insert(0, "file:");
  }
  components.protocol_end = 5;

  // Update the rest of the components.
  components.username_end += new_difference;
  components.host_start += new_difference;
  components.host_end += new_difference;
  components.pathname_start += new_difference;
  if (components.search_start != url_components::omitted) {
    components.search_start += new_difference;
  }
  if (components.hash_start != url_components::omitted) {
    components.hash_start += new_difference;
  }
  ADA_ASSERT_TRUE(validate());
}

[[nodiscard]] constexpr bool url_aggregator::validate() const noexcept {
  if (!is_valid) {
    return true;
  }
  if (!components.check_offset_consistency()) {
    ada_log("url_aggregator::validate inconsistent components \n",
            to_diagram());
    return false;
  }
  // We have a credible components struct, but let us investivate more
  // carefully:
  /**
   * https://user:pass@example.com:1234/foo/bar?baz#quux
   *       |     |    |          | ^^^^|       |   |
   *       |     |    |          | |   |       |   `----- hash_start
   *       |     |    |          | |   |       `--------- search_start
   *       |     |    |          | |   `----------------- pathname_start
   *       |     |    |          | `--------------------- port
   *       |     |    |          `----------------------- host_end
   *       |     |    `---------------------------------- host_start
   *       |     `--------------------------------------- username_end
   *       `--------------------------------------------- protocol_end
   */
  if (components.protocol_end == url_components::omitted) {
    ada_log("url_aggregator::validate omitted protocol_end \n", to_diagram());
    return false;
  }
  if (components.username_end == url_components::omitted) {
    ada_log("url_aggregator::validate omitted username_end \n", to_diagram());
    return false;
  }
  if (components.host_start == url_components::omitted) {
    ada_log("url_aggregator::validate omitted host_start \n", to_diagram());
    return false;
  }
  if (components.host_end == url_components::omitted) {
    ada_log("url_aggregator::validate omitted host_end \n", to_diagram());
    return false;
  }
  if (components.pathname_start == url_components::omitted) {
    ada_log("url_aggregator::validate omitted pathname_start \n", to_diagram());
    return false;
  }

  if (components.protocol_end > buffer.size()) {
    ada_log("url_aggregator::validate protocol_end overflow \n", to_diagram());
    return false;
  }
  if (components.username_end > buffer.size()) {
    ada_log("url_aggregator::validate username_end overflow \n", to_diagram());
    return false;
  }
  if (components.host_start > buffer.size()) {
    ada_log("url_aggregator::validate host_start overflow \n", to_diagram());
    return false;
  }
  if (components.host_end > buffer.size()) {
    ada_log("url_aggregator::validate host_end overflow \n", to_diagram());
    return false;
  }
  if (components.pathname_start > buffer.size()) {
    ada_log("url_aggregator::validate pathname_start overflow \n",
            to_diagram());
    return false;
  }

  if (components.protocol_end > 0) {
    if (buffer[components.protocol_end - 1] != ':') {
      ada_log(
          "url_aggregator::validate missing : at the end of the protocol \n",
          to_diagram());
      return false;
    }
  }

  if (components.username_end != buffer.size() &&
      components.username_end > components.protocol_end + 2) {
    if (buffer[components.username_end] != ':' &&
        buffer[components.username_end] != '@') {
      ada_log(
          "url_aggregator::validate missing : or @ at the end of the username "
          "\n",
          to_diagram());
      return false;
    }
  }

  if (components.host_start != buffer.size()) {
    if (components.host_start > components.username_end) {
      if (buffer[components.host_start] != '@') {
        ada_log(
            "url_aggregator::validate missing @ at the end of the password \n",
            to_diagram());
        return false;
      }
    } else if (components.host_start == components.username_end &&
               components.host_end > components.host_start) {
      if (components.host_start == components.protocol_end + 2) {
        if (buffer[components.protocol_end] != '/' ||
            buffer[components.protocol_end + 1] != '/') {
          ada_log(
              "url_aggregator::validate missing // between protocol and host "
              "\n",
              to_diagram());
          return false;
        }
      } else {
        if (components.host_start > components.protocol_end &&
            buffer[components.host_start] != '@') {
          ada_log(
              "url_aggregator::validate missing @ at the end of the username "
              "\n",
              to_diagram());
          return false;
        }
      }
    } else {
      if (components.host_end != components.host_start) {
        ada_log("url_aggregator::validate expected omitted host \n",
                to_diagram());
        return false;
      }
    }
  }
  if (components.host_end != buffer.size() &&
      components.pathname_start > components.host_end) {
    if (components.pathname_start == components.host_end + 2 &&
        buffer[components.host_end] == '/' &&
        buffer[components.host_end + 1] == '.') {
      if (components.pathname_start + 1 >= buffer.size() ||
          buffer[components.pathname_start] != '/' ||
          buffer[components.pathname_start + 1] != '/') {
        ada_log(
            "url_aggregator::validate expected the path to begin with // \n",
            to_diagram());
        return false;
      }
    } else if (buffer[components.host_end] != ':') {
      ada_log("url_aggregator::validate missing : at the port \n",
              to_diagram());
      return false;
    }
  }
  if (components.pathname_start != buffer.size() &&
      components.pathname_start < components.search_start &&
      components.pathname_start < components.hash_start && !has_opaque_path) {
    if (buffer[components.pathname_start] != '/') {
      ada_log("url_aggregator::validate missing / at the path \n",
              to_diagram());
      return false;
    }
  }
  if (components.search_start != url_components::omitted) {
    if (buffer[components.search_start] != '?') {
      ada_log("url_aggregator::validate missing ? at the search \n",
              to_diagram());
      return false;
    }
  }
  if (components.hash_start != url_components::omitted) {
    if (buffer[components.hash_start] != '#') {
      ada_log("url_aggregator::validate missing # at the hash \n",
              to_diagram());
      return false;
    }
  }

  return true;
}

[[nodiscard]] constexpr std::string_view url_aggregator::get_pathname()
    const noexcept ada_lifetime_bound {
  ada_log("url_aggregator::get_pathname pathname_start = ",
          components.pathname_start, " buffer.size() = ", buffer.size(),
          " components.search_start = ", components.search_start,
          " components.hash_start = ", components.hash_start);
  auto ending_index = uint32_t(buffer.size());
  if (components.search_start != url_components::omitted) {
    ending_index = components.search_start;
  } else if (components.hash_start != url_components::omitted) {
    ending_index = components.hash_start;
  }
  return helpers::substring(buffer, components.pathname_start, ending_index);
}

inline std::ostream &operator<<(std::ostream &out,
                                const ada::url_aggregator &u) {
  return out << u.to_string();
}

void url_aggregator::update_host_to_base_host(
    const std::string_view input) noexcept {
  ada_log("url_aggregator::update_host_to_base_host ", input);
  ADA_ASSERT_TRUE(validate());
  ADA_ASSERT_TRUE(!helpers::overlaps(input, buffer));
  if (type != ada::scheme::type::FILE) {
    // Let host be the result of host parsing host_view with url is not special.
    if (input.empty() && !is_special()) {
      if (has_hostname()) {
        clear_hostname();
      } else if (has_dash_dot()) {
        add_authority_slashes_if_needed();
        delete_dash_dot();
      }
      return;
    }
  }
  update_base_hostname(input);
  ADA_ASSERT_TRUE(validate());
  return;
}
}  // namespace ada

#endif  // ADA_URL_AGGREGATOR_INL_H
/* end file include/ada/url_aggregator-inl.h */
/* begin file include/ada/url_search_params.h */
/**
 * @file url_search_params.h
 * @brief Declaration for the URL Search Params
 */
#ifndef ADA_URL_SEARCH_PARAMS_H
#define ADA_URL_SEARCH_PARAMS_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ada {

enum class url_search_params_iter_type {
  KEYS,
  VALUES,
  ENTRIES,
};

template <typename T, url_search_params_iter_type Type>
struct url_search_params_iter;

typedef std::pair<std::string_view, std::string_view> key_value_view_pair;

using url_search_params_keys_iter =
    url_search_params_iter<std::string_view, url_search_params_iter_type::KEYS>;
using url_search_params_values_iter =
    url_search_params_iter<std::string_view,
                           url_search_params_iter_type::VALUES>;
using url_search_params_entries_iter =
    url_search_params_iter<key_value_view_pair,
                           url_search_params_iter_type::ENTRIES>;

/**
 * We require all strings to be valid UTF-8. It is the user's responsibility to
 * ensure that the provided strings are valid UTF-8.
 * @see https://url.spec.whatwg.org/#interface-urlsearchparams
 */
struct url_search_params {
  url_search_params() = default;

  /**
   * @see
   * https://github.com/web-platform-tests/wpt/blob/master/url/urlsearchparams-constructor.any.js
   */
  explicit url_search_params(const std::string_view input) {
    initialize(input);
  }

  url_search_params(const url_search_params &u) = default;
  url_search_params(url_search_params &&u) noexcept = default;
  url_search_params &operator=(url_search_params &&u) noexcept = default;
  url_search_params &operator=(const url_search_params &u) = default;
  ~url_search_params() = default;

  [[nodiscard]] inline size_t size() const noexcept;

  /**
   * Both key and value must be valid UTF-8.
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-append
   */
  inline void append(std::string_view key, std::string_view value);

  /**
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-delete
   */
  inline void remove(std::string_view key);
  inline void remove(std::string_view key, std::string_view value);

  /**
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-get
   */
  inline std::optional<std::string_view> get(std::string_view key);

  /**
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-getall
   */
  inline std::vector<std::string> get_all(std::string_view key);

  /**
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-has
   */
  inline bool has(std::string_view key) noexcept;
  inline bool has(std::string_view key, std::string_view value) noexcept;

  /**
   * Both key and value must be valid UTF-8.
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-set
   */
  inline void set(std::string_view key, std::string_view value);

  /**
   * @see https://url.spec.whatwg.org/#dom-urlsearchparams-sort
   */
  inline void sort();

  /**
   * @see https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
   */
  inline std::string to_string() const;

  /**
   * Returns a simple JS-style iterator over all of the keys in this
   * url_search_params. The keys in the iterator are not unique. The valid
   * lifespan of the iterator is tied to the url_search_params. The iterator
   * must be freed when you're done with it.
   * @see https://url.spec.whatwg.org/#interface-urlsearchparams
   */
  inline url_search_params_keys_iter get_keys();

  /**
   * Returns a simple JS-style iterator over all of the values in this
   * url_search_params. The valid lifespan of the iterator is tied to the
   * url_search_params. The iterator must be freed when you're done with it.
   * @see https://url.spec.whatwg.org/#interface-urlsearchparams
   */
  inline url_search_params_values_iter get_values();

  /**
   * Returns a simple JS-style iterator over all of the entries in this
   * url_search_params. The entries are pairs of keys and corresponding values.
   * The valid lifespan of the iterator is tied to the url_search_params. The
   * iterator must be freed when you're done with it.
   * @see https://url.spec.whatwg.org/#interface-urlsearchparams
   */
  inline url_search_params_entries_iter get_entries();

  /**
   * C++ style conventional iterator support. const only because we
   * do not really want the params to be modified via the iterator.
   */
  inline auto begin() const { return params.begin(); }
  inline auto end() const { return params.end(); }
  inline auto front() const { return params.front(); }
  inline auto back() const { return params.back(); }
  inline auto operator[](size_t index) const { return params[index]; }

  /**
   * @private
   * Used to reset the search params to a new input.
   * Used primarily for C API.
   * @param input
   */
  void reset(std::string_view input);

 private:
  typedef std::pair<std::string, std::string> key_value_pair;
  std::vector<key_value_pair> params{};

  /**
   * The init parameter must be valid UTF-8.
   * @see https://url.spec.whatwg.org/#concept-urlencoded-parser
   */
  void initialize(std::string_view init);

  template <typename T, url_search_params_iter_type Type>
  friend struct url_search_params_iter;
};  // url_search_params

/**
 * Implements a non-conventional iterator pattern that is closer in style to
 * JavaScript's definition of an iterator.
 *
 * @see https://webidl.spec.whatwg.org/#idl-iterable
 */
template <typename T, url_search_params_iter_type Type>
struct url_search_params_iter {
  inline url_search_params_iter() : params(EMPTY) {}
  url_search_params_iter(const url_search_params_iter &u) = default;
  url_search_params_iter(url_search_params_iter &&u) noexcept = default;
  url_search_params_iter &operator=(url_search_params_iter &&u) noexcept =
      default;
  url_search_params_iter &operator=(const url_search_params_iter &u) = default;
  ~url_search_params_iter() = default;

  /**
   * Return the next item in the iterator or std::nullopt if done.
   */
  inline std::optional<T> next();

  inline bool has_next() const;

 private:
  static url_search_params EMPTY;
  inline url_search_params_iter(url_search_params &params_) : params(params_) {}

  url_search_params &params;
  size_t pos = 0;

  friend struct url_search_params;
};

}  // namespace ada
#endif
/* end file include/ada/url_search_params.h */
/* begin file include/ada/url_search_params-inl.h */
/**
 * @file url_search_params-inl.h
 * @brief Inline declarations for the URL Search Params
 */
#ifndef ADA_URL_SEARCH_PARAMS_INL_H
#define ADA_URL_SEARCH_PARAMS_INL_H


#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace ada {

// A default, empty url_search_params for use with empty iterators.
template <typename T, ada::url_search_params_iter_type Type>
url_search_params url_search_params_iter<T, Type>::EMPTY;

inline void url_search_params::reset(std::string_view input) {
  params.clear();
  initialize(input);
}

inline void url_search_params::initialize(std::string_view input) {
  if (!input.empty() && input.front() == '?') {
    input.remove_prefix(1);
  }

  auto process_key_value = [&](const std::string_view current) {
    auto equal = current.find('=');

    if (equal == std::string_view::npos) {
      std::string name(current);
      std::ranges::replace(name, '+', ' ');
      params.emplace_back(unicode::percent_decode(name, name.find('%')), "");
    } else {
      std::string name(current.substr(0, equal));
      std::string value(current.substr(equal + 1));

      std::ranges::replace(name, '+', ' ');
      std::ranges::replace(value, '+', ' ');

      params.emplace_back(unicode::percent_decode(name, name.find('%')),
                          unicode::percent_decode(value, value.find('%')));
    }
  };

  while (!input.empty()) {
    auto ampersand_index = input.find('&');

    if (ampersand_index == std::string_view::npos) {
      if (!input.empty()) {
        process_key_value(input);
      }
      break;
    } else if (ampersand_index != 0) {
      process_key_value(input.substr(0, ampersand_index));
    }

    input.remove_prefix(ampersand_index + 1);
  }
}

inline void url_search_params::append(const std::string_view key,
                                      const std::string_view value) {
  params.emplace_back(key, value);
}

inline size_t url_search_params::size() const noexcept { return params.size(); }

inline std::optional<std::string_view> url_search_params::get(
    const std::string_view key) {
  auto entry = std::ranges::find_if(
      params, [&key](const auto &param) { return param.first == key; });

  if (entry == params.end()) {
    return std::nullopt;
  }

  return entry->second;
}

inline std::vector<std::string> url_search_params::get_all(
    const std::string_view key) {
  std::vector<std::string> out{};

  for (auto &param : params) {
    if (param.first == key) {
      out.emplace_back(param.second);
    }
  }

  return out;
}

inline bool url_search_params::has(const std::string_view key) noexcept {
  auto entry = std::ranges::find_if(
      params, [&key](const auto &param) { return param.first == key; });
  return entry != params.end();
}

inline bool url_search_params::has(std::string_view key,
                                   std::string_view value) noexcept {
  auto entry = std::ranges::find_if(params, [&key, &value](const auto &param) {
    return param.first == key && param.second == value;
  });
  return entry != params.end();
}

inline std::string url_search_params::to_string() const {
  auto character_set = ada::character_sets::WWW_FORM_URLENCODED_PERCENT_ENCODE;
  std::string out{};
  for (size_t i = 0; i < params.size(); i++) {
    auto key = ada::unicode::percent_encode(params[i].first, character_set);
    auto value = ada::unicode::percent_encode(params[i].second, character_set);

    // Performance optimization: Move this inside percent_encode.
    std::ranges::replace(key, ' ', '+');
    std::ranges::replace(value, ' ', '+');

    if (i != 0) {
      out += "&";
    }
    out.append(key);
    out += "=";
    out.append(value);
  }
  return out;
}

inline void url_search_params::set(const std::string_view key,
                                   const std::string_view value) {
  const auto find = [&key](const auto &param) { return param.first == key; };

  auto it = std::ranges::find_if(params, find);

  if (it == params.end()) {
    params.emplace_back(key, value);
  } else {
    it->second = value;
    params.erase(std::remove_if(std::next(it), params.end(), find),
                 params.end());
  }
}

inline void url_search_params::remove(const std::string_view key) {
  std::erase_if(params,
                [&key](const auto &param) { return param.first == key; });
}

inline void url_search_params::remove(const std::string_view key,
                                      const std::string_view value) {
  std::erase_if(params, [&key, &value](const auto &param) {
    return param.first == key && param.second == value;
  });
}

inline void url_search_params::sort() {
  // We rely on the fact that the content is valid UTF-8.
  std::ranges::stable_sort(params, [](const key_value_pair &lhs,
                                      const key_value_pair &rhs) {
    size_t i = 0, j = 0;
    uint32_t low_surrogate1 = 0, low_surrogate2 = 0;
    while ((i < lhs.first.size() || low_surrogate1 != 0) &&
           (j < rhs.first.size() || low_surrogate2 != 0)) {
      uint32_t codePoint1 = 0, codePoint2 = 0;

      if (low_surrogate1 != 0) {
        codePoint1 = low_surrogate1;
        low_surrogate1 = 0;
      } else {
        uint8_t c1 = uint8_t(lhs.first[i]);
        if (c1 <= 0x7F) {
          codePoint1 = c1;
          i++;
        } else if (c1 <= 0xDF) {
          codePoint1 = ((c1 & 0x1F) << 6) | (uint8_t(lhs.first[i + 1]) & 0x3F);
          i += 2;
        } else if (c1 <= 0xEF) {
          codePoint1 = ((c1 & 0x0F) << 12) |
                       ((uint8_t(lhs.first[i + 1]) & 0x3F) << 6) |
                       (uint8_t(lhs.first[i + 2]) & 0x3F);
          i += 3;
        } else {
          codePoint1 = ((c1 & 0x07) << 18) |
                       ((uint8_t(lhs.first[i + 1]) & 0x3F) << 12) |
                       ((uint8_t(lhs.first[i + 2]) & 0x3F) << 6) |
                       (uint8_t(lhs.first[i + 3]) & 0x3F);
          i += 4;

          codePoint1 -= 0x10000;
          uint16_t high_surrogate = uint16_t(0xD800 + (codePoint1 >> 10));
          low_surrogate1 = uint16_t(0xDC00 + (codePoint1 & 0x3FF));
          codePoint1 = high_surrogate;
        }
      }

      if (low_surrogate2 != 0) {
        codePoint2 = low_surrogate2;
        low_surrogate2 = 0;
      } else {
        uint8_t c2 = uint8_t(rhs.first[j]);
        if (c2 <= 0x7F) {
          codePoint2 = c2;
          j++;
        } else if (c2 <= 0xDF) {
          codePoint2 = ((c2 & 0x1F) << 6) | (uint8_t(rhs.first[j + 1]) & 0x3F);
          j += 2;
        } else if (c2 <= 0xEF) {
          codePoint2 = ((c2 & 0x0F) << 12) |
                       ((uint8_t(rhs.first[j + 1]) & 0x3F) << 6) |
                       (uint8_t(rhs.first[j + 2]) & 0x3F);
          j += 3;
        } else {
          codePoint2 = ((c2 & 0x07) << 18) |
                       ((uint8_t(rhs.first[j + 1]) & 0x3F) << 12) |
                       ((uint8_t(rhs.first[j + 2]) & 0x3F) << 6) |
                       (uint8_t(rhs.first[j + 3]) & 0x3F);
          j += 4;
          codePoint2 -= 0x10000;
          uint16_t high_surrogate = uint16_t(0xD800 + (codePoint2 >> 10));
          low_surrogate2 = uint16_t(0xDC00 + (codePoint2 & 0x3FF));
          codePoint2 = high_surrogate;
        }
      }

      if (codePoint1 != codePoint2) {
        return (codePoint1 < codePoint2);
      }
    }
    return (j < rhs.first.size() || low_surrogate2 != 0);
  });
}

inline url_search_params_keys_iter url_search_params::get_keys() {
  return url_search_params_keys_iter(*this);
}

/**
 * @see https://url.spec.whatwg.org/#interface-urlsearchparams
 */
inline url_search_params_values_iter url_search_params::get_values() {
  return url_search_params_values_iter(*this);
}

/**
 * @see https://url.spec.whatwg.org/#interface-urlsearchparams
 */
inline url_search_params_entries_iter url_search_params::get_entries() {
  return url_search_params_entries_iter(*this);
}

template <typename T, url_search_params_iter_type Type>
inline bool url_search_params_iter<T, Type>::has_next() const {
  return pos < params.params.size();
}

template <>
inline std::optional<std::string_view> url_search_params_keys_iter::next() {
  if (!has_next()) {
    return std::nullopt;
  }
  return params.params[pos++].first;
}

template <>
inline std::optional<std::string_view> url_search_params_values_iter::next() {
  if (!has_next()) {
    return std::nullopt;
  }
  return params.params[pos++].second;
}

template <>
inline std::optional<key_value_view_pair>
url_search_params_entries_iter::next() {
  if (!has_next()) {
    return std::nullopt;
  }
  return params.params[pos++];
}

}  // namespace ada

#endif  // ADA_URL_SEARCH_PARAMS_INL_H
/* end file include/ada/url_search_params-inl.h */
/* begin file include/ada/url_pattern-inl.h */
/**
 * @file url_pattern-inl.h
 * @brief Declaration for the URLPattern inline functions.
 */
#ifndef ADA_URL_PATTERN_INL_H
#define ADA_URL_PATTERN_INL_H


#include <algorithm>
#include <string_view>
#include <utility>

namespace ada {

inline bool url_pattern_init::operator==(const url_pattern_init& other) const {
  return protocol == other.protocol && username == other.username &&
         password == other.password && hostname == other.hostname &&
         port == other.port && search == other.search && hash == other.hash &&
         pathname == other.pathname;
}

inline bool url_pattern_component_result::operator==(
    const url_pattern_component_result& other) const {
  return input == other.input && groups == other.groups;
}

template <url_pattern_regex::regex_concept regex_provider>
url_pattern_component_result
url_pattern_component<regex_provider>::create_component_match_result(
    std::string&& input,
    std::vector<std::optional<std::string>>&& exec_result) {
  // Let result be a new URLPatternComponentResult.
  // Set result["input"] to input.
  // Let groups be a record<USVString, (USVString or undefined)>.
  auto result =
      url_pattern_component_result{.input = std::move(input), .groups = {}};

  // Optimization: Let's reserve the size.
  result.groups.reserve(exec_result.size());

  // We explicitly start iterating from 0 even though the spec
  // says we should start from 1. This case is handled by the
  // std_regex_provider.
  for (size_t index = 0; index < exec_result.size(); index++) {
    result.groups.insert({
        group_name_list[index],
        std::move(exec_result[index]),
    });
  }
  return result;
}

template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_protocol() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's protocol component's pattern string.
  return protocol_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_username() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's username component's pattern string.
  return username_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_password() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's password component's pattern string.
  return password_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_hostname() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's hostname component's pattern string.
  return hostname_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_port() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's port component's pattern string.
  return port_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_pathname() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's pathname component's pattern string.
  return pathname_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_search() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's search component's pattern string.
  return search_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
std::string_view url_pattern<regex_provider>::get_hash() const
    ada_lifetime_bound {
  // Return this's associated URL pattern's hash component's pattern string.
  return hash_component.pattern;
}
template <url_pattern_regex::regex_concept regex_provider>
bool url_pattern<regex_provider>::ignore_case() const {
  return ignore_case_;
}
template <url_pattern_regex::regex_concept regex_provider>
bool url_pattern<regex_provider>::has_regexp_groups() const {
  // If this's associated URL pattern's has regexp groups, then return true.
  return protocol_component.has_regexp_groups ||
         username_component.has_regexp_groups ||
         password_component.has_regexp_groups ||
         hostname_component.has_regexp_groups ||
         port_component.has_regexp_groups ||
         pathname_component.has_regexp_groups ||
         search_component.has_regexp_groups || hash_component.has_regexp_groups;
}

inline bool url_pattern_part::is_regexp() const noexcept {
  return type == url_pattern_part_type::REGEXP;
}

inline std::string_view url_pattern_compile_component_options::get_delimiter()
    const {
  if (delimiter) {
    return {&delimiter.value(), 1};
  }
  return {};
}

inline std::string_view url_pattern_compile_component_options::get_prefix()
    const {
  if (prefix) {
    return {&prefix.value(), 1};
  }
  return {};
}

template <url_pattern_regex::regex_concept regex_provider>
template <url_pattern_encoding_callback F>
tl::expected<url_pattern_component<regex_provider>, errors>
url_pattern_component<regex_provider>::compile(
    std::string_view input, F& encoding_callback,
    url_pattern_compile_component_options& options) {
  ada_log("url_pattern_component::compile input: ", input);
  // Let part list be the result of running parse a pattern string given input,
  // options, and encoding callback.
  auto part_list = url_pattern_helpers::parse_pattern_string(input, options,
                                                             encoding_callback);

  if (!part_list) {
    ada_log("parse_pattern_string failed");
    return tl::unexpected(part_list.error());
  }

  // Let (regular expression string, name list) be the result of running
  // generate a regular expression and name list given part list and options.
  auto [regular_expression_string, name_list] =
      url_pattern_helpers::generate_regular_expression_and_name_list(*part_list,
                                                                     options);

  ada_log("regular expression string: ", regular_expression_string);

  // Let pattern string be the result of running generate a pattern
  // string given part list and options.
  auto pattern_string =
      url_pattern_helpers::generate_pattern_string(*part_list, options);

  // Let regular expression be RegExpCreate(regular expression string,
  // flags). If this throws an exception, catch it, and throw a
  // TypeError.
  std::optional<typename regex_provider::regex_type> regular_expression =
      regex_provider::create_instance(regular_expression_string,
                                      options.ignore_case);

  if (!regular_expression) {
    return tl::unexpected(errors::type_error);
  }

  // For each part of part list:
  // - If part’s type is "regexp", then set has regexp groups to true.
  const auto has_regexp = [](const auto& part) { return part.is_regexp(); };
  const bool has_regexp_groups = std::ranges::any_of(*part_list, has_regexp);

  ada_log("has regexp groups: ", has_regexp_groups);

  // Return a new component whose pattern string is pattern string, regular
  // expression is regular expression, group name list is name list, and has
  // regexp groups is has regexp groups.
  return url_pattern_component<regex_provider>(
      std::move(pattern_string), std::move(*regular_expression),
      std::move(name_list), has_regexp_groups);
}

template <url_pattern_regex::regex_concept regex_provider>
result<std::optional<url_pattern_result>> url_pattern<regex_provider>::exec(
    const url_pattern_input& input, const std::string_view* base_url) {
  // Return the result of match given this's associated URL pattern, input, and
  // baseURL if given.
  return match(input, base_url);
}

template <url_pattern_regex::regex_concept regex_provider>
result<bool> url_pattern<regex_provider>::test(
    const url_pattern_input& input, const std::string_view* base_url) {
  // TODO: Optimization opportunity. Rather than returning `url_pattern_result`
  // Implement a fast path just like `can_parse()` in ada_url.
  // Let result be the result of match given this's associated URL pattern,
  // input, and baseURL if given.
  // If result is null, return false.
  if (auto result = match(input, base_url); result.has_value()) {
    return result->has_value();
  }
  return tl::unexpected(errors::type_error);
}

template <url_pattern_regex::regex_concept regex_provider>
result<std::optional<url_pattern_result>> url_pattern<regex_provider>::match(
    const url_pattern_input& input, const std::string_view* base_url_string) {
  std::string protocol{};
  std::string username{};
  std::string password{};
  std::string hostname{};
  std::string port{};
  std::string pathname{};
  std::string search{};
  std::string hash{};

  // Let inputs be an empty list.
  // Append input to inputs.
  std::vector inputs{input};

  // If input is a URLPatternInit then:
  if (std::holds_alternative<url_pattern_init>(input)) {
    ada_log(
        "url_pattern::match called with url_pattern_init and base_url_string=",
        base_url_string);
    // If baseURLString was given, throw a TypeError.
    if (base_url_string) {
      ada_log("failed to match because base_url_string was given");
      return tl::unexpected(errors::type_error);
    }

    // Let applyResult be the result of process a URLPatternInit given input,
    // "url", protocol, username, password, hostname, port, pathname, search,
    // and hash.
    auto apply_result = url_pattern_init::process(
        std::get<url_pattern_init>(input), url_pattern_init::process_type::url,
        protocol, username, password, hostname, port, pathname, search, hash);

    // If this throws an exception, catch it, and return null.
    if (!apply_result.has_value()) {
      ada_log("match returned std::nullopt because process threw");
      return std::nullopt;
    }

    // Set protocol to applyResult["protocol"].
    ADA_ASSERT_TRUE(apply_result->protocol.has_value());
    protocol = std::move(apply_result->protocol.value());

    // Set username to applyResult["username"].
    ADA_ASSERT_TRUE(apply_result->username.has_value());
    username = std::move(apply_result->username.value());

    // Set password to applyResult["password"].
    ADA_ASSERT_TRUE(apply_result->password.has_value());
    password = std::move(apply_result->password.value());

    // Set hostname to applyResult["hostname"].
    ADA_ASSERT_TRUE(apply_result->hostname.has_value());
    hostname = std::move(apply_result->hostname.value());

    // Set port to applyResult["port"].
    ADA_ASSERT_TRUE(apply_result->port.has_value());
    port = std::move(apply_result->port.value());

    // Set pathname to applyResult["pathname"].
    ADA_ASSERT_TRUE(apply_result->pathname.has_value());
    pathname = std::move(apply_result->pathname.value());

    // Set search to applyResult["search"].
    ADA_ASSERT_TRUE(apply_result->search.has_value());
    if (apply_result->search->starts_with("?")) {
      search = apply_result->search->substr(1);
    } else {
      search = std::move(apply_result->search.value());
    }

    // Set hash to applyResult["hash"].
    ADA_ASSERT_TRUE(apply_result->hash.has_value());
    ADA_ASSERT_TRUE(!apply_result->hash->starts_with("#"));
    hash = std::move(apply_result->hash.value());
  } else {
    ADA_ASSERT_TRUE(std::holds_alternative<std::string_view>(input));

    // Let baseURL be null.
    result<url_aggregator> base_url;

    // If baseURLString was given, then:
    if (base_url_string) {
      // Let baseURL be the result of parsing baseURLString.
      base_url = ada::parse<url_aggregator>(*base_url_string, nullptr);

      // If baseURL is failure, return null.
      if (!base_url) {
        ada_log("match returned std::nullopt because failed to parse base_url=",
                *base_url_string);
        return std::nullopt;
      }

      // Append baseURLString to inputs.
      inputs.emplace_back(*base_url_string);
    }

    url_aggregator* base_url_value =
        base_url.has_value() ? &*base_url : nullptr;

    // Set url to the result of parsing input given baseURL.
    auto url = ada::parse<url_aggregator>(std::get<std::string_view>(input),
                                          base_url_value);

    // If url is failure, return null.
    if (!url) {
      ada_log("match returned std::nullopt because url failed");
      return std::nullopt;
    }

    // Set protocol to url’s scheme.
    // IMPORTANT: Not documented on the URLPattern spec, but protocol suffix ':'
    // is removed. Similar work was done on workerd:
    // https://github.com/cloudflare/workerd/blob/8620d14012513a6ce04d079e401d3becac3c67bd/src/workerd/jsg/url.c%2B%2B#L2038
    protocol = url->get_protocol().substr(0, url->get_protocol().size() - 1);
    // Set username to url’s username.
    username = url->get_username();
    // Set password to url’s password.
    password = url->get_password();
    // Set hostname to url’s host, serialized, or the empty string if the value
    // is null.
    hostname = url->get_hostname();
    // Set port to url’s port, serialized, or the empty string if the value is
    // null.
    port = url->get_port();
    // Set pathname to the result of URL path serializing url.
    pathname = url->get_pathname();
    // Set search to url’s query or the empty string if the value is null.
    // IMPORTANT: Not documented on the URLPattern spec, but search prefix '?'
    // is removed. Similar work was done on workerd:
    // https://github.com/cloudflare/workerd/blob/8620d14012513a6ce04d079e401d3becac3c67bd/src/workerd/jsg/url.c%2B%2B#L2232
    if (url->has_search()) {
      auto view = url->get_search();
      search = view.starts_with("?") ? url->get_search().substr(1) : view;
    }
    // Set hash to url’s fragment or the empty string if the value is null.
    // IMPORTANT: Not documented on the URLPattern spec, but hash prefix '#' is
    // removed. Similar work was done on workerd:
    // https://github.com/cloudflare/workerd/blob/8620d14012513a6ce04d079e401d3becac3c67bd/src/workerd/jsg/url.c%2B%2B#L2242
    if (url->has_hash()) {
      auto view = url->get_hash();
      hash = view.starts_with("#") ? url->get_hash().substr(1) : view;
    }
  }

  // Let protocolExecResult be RegExpBuiltinExec(urlPattern’s protocol
  // component's regular expression, protocol).
  auto protocol_exec_result =
      regex_provider::regex_search(protocol, protocol_component.regexp);

  if (!protocol_exec_result) {
    return std::nullopt;
  }

  // Let usernameExecResult be RegExpBuiltinExec(urlPattern’s username
  // component's regular expression, username).
  auto username_exec_result =
      regex_provider::regex_search(username, username_component.regexp);

  if (!username_exec_result) {
    return std::nullopt;
  }

  // Let passwordExecResult be RegExpBuiltinExec(urlPattern’s password
  // component's regular expression, password).
  auto password_exec_result =
      regex_provider::regex_search(password, password_component.regexp);

  if (!password_exec_result) {
    return std::nullopt;
  }

  // Let hostnameExecResult be RegExpBuiltinExec(urlPattern’s hostname
  // component's regular expression, hostname).
  auto hostname_exec_result =
      regex_provider::regex_search(hostname, hostname_component.regexp);

  if (!hostname_exec_result) {
    return std::nullopt;
  }

  // Let portExecResult be RegExpBuiltinExec(urlPattern’s port component's
  // regular expression, port).
  auto port_exec_result =
      regex_provider::regex_search(port, port_component.regexp);

  if (!port_exec_result) {
    return std::nullopt;
  }

  // Let pathnameExecResult be RegExpBuiltinExec(urlPattern’s pathname
  // component's regular expression, pathname).
  auto pathname_exec_result =
      regex_provider::regex_search(pathname, pathname_component.regexp);

  if (!pathname_exec_result) {
    return std::nullopt;
  }

  // Let searchExecResult be RegExpBuiltinExec(urlPattern’s search component's
  // regular expression, search).
  auto search_exec_result =
      regex_provider::regex_search(search, search_component.regexp);

  if (!search_exec_result) {
    return std::nullopt;
  }

  // Let hashExecResult be RegExpBuiltinExec(urlPattern’s hash component's
  // regular expression, hash).
  auto hash_exec_result =
      regex_provider::regex_search(hash, hash_component.regexp);

  if (!hash_exec_result) {
    return std::nullopt;
  }

  // Let result be a new URLPatternResult.
  auto result = url_pattern_result{};
  // Set result["inputs"] to inputs.
  result.inputs = std::move(inputs);
  // Set result["protocol"] to the result of creating a component match result
  // given urlPattern’s protocol component, protocol, and protocolExecResult.
  result.protocol = protocol_component.create_component_match_result(
      std::move(protocol), std::move(*protocol_exec_result));

  // Set result["username"] to the result of creating a component match result
  // given urlPattern’s username component, username, and usernameExecResult.
  result.username = username_component.create_component_match_result(
      std::move(username), std::move(*username_exec_result));

  // Set result["password"] to the result of creating a component match result
  // given urlPattern’s password component, password, and passwordExecResult.
  result.password = password_component.create_component_match_result(
      std::move(password), std::move(*password_exec_result));

  // Set result["hostname"] to the result of creating a component match result
  // given urlPattern’s hostname component, hostname, and hostnameExecResult.
  result.hostname = hostname_component.create_component_match_result(
      std::move(hostname), std::move(*hostname_exec_result));

  // Set result["port"] to the result of creating a component match result given
  // urlPattern’s port component, port, and portExecResult.
  result.port = port_component.create_component_match_result(
      std::move(port), std::move(*port_exec_result));

  // Set result["pathname"] to the result of creating a component match result
  // given urlPattern’s pathname component, pathname, and pathnameExecResult.
  result.pathname = pathname_component.create_component_match_result(
      std::move(pathname), std::move(*pathname_exec_result));

  // Set result["search"] to the result of creating a component match result
  // given urlPattern’s search component, search, and searchExecResult.
  result.search = search_component.create_component_match_result(
      std::move(search), std::move(*search_exec_result));

  // Set result["hash"] to the result of creating a component match result given
  // urlPattern’s hash component, hash, and hashExecResult.
  result.hash = hash_component.create_component_match_result(
      std::move(hash), std::move(*hash_exec_result));

  return result;
}

}  // namespace ada

#endif
/* end file include/ada/url_pattern-inl.h */
/* begin file include/ada/url_pattern_helpers-inl.h */
/**
 * @file url_pattern_helpers-inl.h
 * @brief Declaration for the URLPattern helpers.
 */
#ifndef ADA_URL_PATTERN_HELPERS_INL_H
#define ADA_URL_PATTERN_HELPERS_INL_H

#include <optional>
#include <string_view>


namespace ada::url_pattern_helpers {
#ifdef ADA_TESTING
inline std::string to_string(token_type type) {
  switch (type) {
    case token_type::INVALID_CHAR:
      return "INVALID_CHAR";
    case token_type::OPEN:
      return "OPEN";
    case token_type::CLOSE:
      return "CLOSE";
    case token_type::REGEXP:
      return "REGEXP";
    case token_type::NAME:
      return "NAME";
    case token_type::CHAR:
      return "CHAR";
    case token_type::ESCAPED_CHAR:
      return "ESCAPED_CHAR";
    case token_type::OTHER_MODIFIER:
      return "OTHER_MODIFIER";
    case token_type::ASTERISK:
      return "ASTERISK";
    case token_type::END:
      return "END";
    default:
      ada::unreachable();
  }
}
#endif  // ADA_TESTING

template <url_pattern_regex::regex_concept regex_provider>
constexpr void constructor_string_parser<regex_provider>::rewind() {
  // Set parser’s token index to parser’s component start.
  token_index = component_start;
  // Set parser’s token increment to 0.
  token_increment = 0;
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_hash_prefix() {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index and "#".
  return is_non_special_pattern_char(token_index, '#');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_search_prefix() {
  // If result of running is a non-special pattern char given parser, parser’s
  // token index and "?" is true, then return true.
  if (is_non_special_pattern_char(token_index, '?')) {
    return true;
  }

  // If parser’s token list[parser’s token index]'s value is not "?", then
  // return false.
  if (token_list[token_index].value != "?") {
    return false;
  }

  // If previous index is less than 0, then return true.
  if (token_index == 0) return true;
  // Let previous index be parser’s token index − 1.
  auto previous_index = token_index - 1;
  // Let previous token be the result of running get a safe token given parser
  // and previous index.
  auto previous_token = get_safe_token(previous_index);
  ADA_ASSERT_TRUE(previous_token);
  // If any of the following are true, then return false:
  // - previous token’s type is "name".
  // - previous token’s type is "regexp".
  // - previous token’s type is "close".
  // - previous token’s type is "asterisk".
  return !(previous_token->type == token_type::NAME ||
           previous_token->type == token_type::REGEXP ||
           previous_token->type == token_type::CLOSE ||
           previous_token->type == token_type::ASTERISK);
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool
constructor_string_parser<regex_provider>::is_non_special_pattern_char(
    size_t index, uint32_t value) const {
  // Let token be the result of running get a safe token given parser and index.
  auto token = get_safe_token(index);
  ADA_ASSERT_TRUE(token);

  // If token’s value is not value, then return false.
  // TODO: Remove this once we make sure get_safe_token returns a non-empty
  // string.
  if (!token->value.empty() &&
      static_cast<uint32_t>(token->value[0]) != value) {
    return false;
  }

  // If any of the following are true:
  // - token’s type is "char";
  // - token’s type is "escaped-char"; or
  // - token’s type is "invalid-char",
  // - then return true.
  return token->type == token_type::CHAR ||
         token->type == token_type::ESCAPED_CHAR ||
         token->type == token_type::INVALID_CHAR;
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr const token*
constructor_string_parser<regex_provider>::get_safe_token(size_t index) const {
  // If index is less than parser’s token list's size, then return parser’s
  // token list[index].
  if (index < token_list.size()) [[likely]] {
    return &token_list[index];
  }

  // Assert: parser’s token list's size is greater than or equal to 1.
  ADA_ASSERT_TRUE(!token_list.empty());

  // Let token be parser’s token list[last index].
  // Assert: token’s type is "end".
  ADA_ASSERT_TRUE(token_list.back().type == token_type::END);

  // Return token.
  return &token_list.back();
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_group_open()
    const {
  // If parser’s token list[parser’s token index]'s type is "open", then return
  // true.
  return token_list[token_index].type == token_type::OPEN;
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_group_close()
    const {
  // If parser’s token list[parser’s token index]'s type is "close", then return
  // true.
  return token_list[token_index].type == token_type::CLOSE;
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool
constructor_string_parser<regex_provider>::next_is_authority_slashes() const {
  // If the result of running is a non-special pattern char given parser,
  // parser’s token index + 1, and "/" is false, then return false.
  if (!is_non_special_pattern_char(token_index + 1, '/')) {
    return false;
  }
  // If the result of running is a non-special pattern char given parser,
  // parser’s token index + 2, and "/" is false, then return false.
  if (!is_non_special_pattern_char(token_index + 2, '/')) {
    return false;
  }
  return true;
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_protocol_suffix()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and ":".
  return is_non_special_pattern_char(token_index, ':');
}

template <url_pattern_regex::regex_concept regex_provider>
void constructor_string_parser<regex_provider>::change_state(State new_state,
                                                             size_t skip) {
  // If parser’s state is not "init", not "authority", and not "done", then set
  // parser’s result[parser’s state] to the result of running make a component
  // string given parser.
  if (state != State::INIT && state != State::AUTHORITY &&
      state != State::DONE) {
    auto value = make_component_string();
    // TODO: Simplify this.
    switch (state) {
      case State::PROTOCOL: {
        result.protocol = value;
        break;
      }
      case State::USERNAME: {
        result.username = value;
        break;
      }
      case State::PASSWORD: {
        result.password = value;
        break;
      }
      case State::HOSTNAME: {
        result.hostname = value;
        break;
      }
      case State::PORT: {
        result.port = value;
        break;
      }
      case State::PATHNAME: {
        result.pathname = value;
        break;
      }
      case State::SEARCH: {
        result.search = value;
        break;
      }
      case State::HASH: {
        result.hash = value;
        break;
      }
      default:
        ada::unreachable();
    }
  }

  // If parser’s state is not "init" and new state is not "done", then:
  if (state != State::INIT && new_state != State::DONE) {
    // If parser’s state is "protocol", "authority", "username", or "password";
    // new state is "port", "pathname", "search", or "hash"; and parser’s
    // result["hostname"] does not exist, then set parser’s result["hostname"]
    // to the empty string.
    if ((state == State::PROTOCOL || state == State::AUTHORITY ||
         state == State::USERNAME || state == State::PASSWORD) &&
        (new_state == State::PORT || new_state == State::PATHNAME ||
         new_state == State::SEARCH || new_state == State::HASH) &&
        !result.hostname)
      result.hostname = "";
  }

  // If parser’s state is "protocol", "authority", "username", "password",
  // "hostname", or "port"; new state is "search" or "hash"; and parser’s
  // result["pathname"] does not exist, then:
  if ((state == State::PROTOCOL || state == State::AUTHORITY ||
       state == State::USERNAME || state == State::PASSWORD ||
       state == State::HOSTNAME || state == State::PORT) &&
      (new_state == State::SEARCH || new_state == State::HASH) &&
      !result.pathname) {
    if (protocol_matches_a_special_scheme_flag) {
      result.pathname = "/";
    } else {
      // Otherwise, set parser’s result["pathname"] to the empty string.
      result.pathname = "";
    }
  }

  // If parser’s state is "protocol", "authority", "username", "password",
  // "hostname", "port", or "pathname"; new state is "hash"; and parser’s
  // result["search"] does not exist, then set parser’s result["search"] to
  // the empty string.
  if ((state == State::PROTOCOL || state == State::AUTHORITY ||
       state == State::USERNAME || state == State::PASSWORD ||
       state == State::HOSTNAME || state == State::PORT ||
       state == State::PATHNAME) &&
      new_state == State::HASH && !result.search) {
    result.search = "";
  }

  // Set parser’s state to new state.
  state = new_state;
  // Increment parser’s token index by skip.
  token_index += skip;
  // Set parser’s component start to parser’s token index.
  component_start = token_index;
  // Set parser’s token increment to 0.
  token_increment = 0;
}

template <url_pattern_regex::regex_concept regex_provider>
std::string constructor_string_parser<regex_provider>::make_component_string() {
  // Assert: parser’s token index is less than parser’s token list's size.
  ADA_ASSERT_TRUE(token_index < token_list.size());

  // Let token be parser’s token list[parser’s token index].
  // Let end index be token’s index.
  const auto end_index = token_list[token_index].index;
  // Let component start token be the result of running get a safe token given
  // parser and parser’s component start.
  const auto component_start_token = get_safe_token(component_start);
  ADA_ASSERT_TRUE(component_start_token);
  // Let component start input index be component start token’s index.
  const auto component_start_input_index = component_start_token->index;
  // Return the code point substring from component start input index to end
  // index within parser’s input.
  return input.substr(component_start_input_index,
                      end_index - component_start_input_index);
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool
constructor_string_parser<regex_provider>::is_an_identity_terminator() const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and "@".
  return is_non_special_pattern_char(token_index, '@');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_pathname_start()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and "/".
  return is_non_special_pattern_char(token_index, '/');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_password_prefix()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and ":".
  return is_non_special_pattern_char(token_index, ':');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_an_ipv6_open()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and "[".
  return is_non_special_pattern_char(token_index, '[');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_an_ipv6_close()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and "]".
  return is_non_special_pattern_char(token_index, ']');
}

template <url_pattern_regex::regex_concept regex_provider>
constexpr bool constructor_string_parser<regex_provider>::is_port_prefix()
    const {
  // Return the result of running is a non-special pattern char given parser,
  // parser’s token index, and ":".
  return is_non_special_pattern_char(token_index, ':');
}

constexpr void Tokenizer::get_next_code_point() {
  ada_log("Tokenizer::get_next_code_point called with index=", next_index);
  ADA_ASSERT_TRUE(next_index < input.size());
  // this assumes that we have a valid, non-truncated UTF-8 stream.
  code_point = 0;
  size_t number_bytes = 0;
  unsigned char first_byte = input[next_index];

  if ((first_byte & 0x80) == 0) {
    // 1-byte character (ASCII)
    next_index++;
    code_point = first_byte;
    ada_log("Tokenizer::get_next_code_point returning ASCII code point=",
            uint32_t(code_point));
    ada_log("Tokenizer::get_next_code_point next_index =", next_index,
            " input.size()=", input.size());
    return;
  }
  ada_log("Tokenizer::get_next_code_point read first byte=",
          uint32_t(first_byte));
  if ((first_byte & 0xE0) == 0xC0) {
    code_point = first_byte & 0x1F;
    number_bytes = 2;
    ada_log("Tokenizer::get_next_code_point two bytes");
  } else if ((first_byte & 0xF0) == 0xE0) {
    code_point = first_byte & 0x0F;
    number_bytes = 3;
    ada_log("Tokenizer::get_next_code_point three bytes");
  } else if ((first_byte & 0xF8) == 0xF0) {
    code_point = first_byte & 0x07;
    number_bytes = 4;
    ada_log("Tokenizer::get_next_code_point four bytes");
  }
  ADA_ASSERT_TRUE(number_bytes + next_index <= input.size());

  for (size_t i = 1 + next_index; i < number_bytes + next_index; ++i) {
    unsigned char byte = input[i];
    ada_log("Tokenizer::get_next_code_point read byte=", uint32_t(byte));
    code_point = (code_point << 6) | (byte & 0x3F);
  }
  ada_log("Tokenizer::get_next_code_point returning non-ASCII code point=",
          uint32_t(code_point));
  ada_log("Tokenizer::get_next_code_point next_index =", next_index,
          " input.size()=", input.size());
  next_index += number_bytes;
}

constexpr void Tokenizer::seek_and_get_next_code_point(size_t new_index) {
  ada_log("Tokenizer::seek_and_get_next_code_point called with new_index=",
          new_index);
  // Set tokenizer’s next index to index.
  next_index = new_index;
  // Run get the next code point given tokenizer.
  get_next_code_point();
}

inline void Tokenizer::add_token(token_type type, size_t next_position,
                                 size_t value_position, size_t value_length) {
  ada_log("Tokenizer::add_token called with type=", to_string(type),
          " next_position=", next_position, " value_position=", value_position);
  ADA_ASSERT_TRUE(next_position >= value_position);

  // Let token be a new token.
  // Set token’s type to type.
  // Set token’s index to tokenizer’s index.
  // Set token’s value to the code point substring from value position with
  // length value length within tokenizer’s input.
  // Append token to the back of tokenizer’s token list.
  token_list.emplace_back(type, index,
                          input.substr(value_position, value_length));
  // Set tokenizer’s index to next position.
  index = next_position;
}

inline void Tokenizer::add_token_with_default_length(token_type type,
                                                     size_t next_position,
                                                     size_t value_position) {
  // Let computed length be next position − value position.
  auto computed_length = next_position - value_position;
  // Run add a token given tokenizer, type, next position, value position, and
  // computed length.
  add_token(type, next_position, value_position, computed_length);
}

inline void Tokenizer::add_token_with_defaults(token_type type) {
  ada_log("Tokenizer::add_token_with_defaults called with type=",
          to_string(type));
  // Run add a token with default length given tokenizer, type, tokenizer’s next
  // index, and tokenizer’s index.
  add_token_with_default_length(type, next_index, index);
}

inline ada_warn_unused std::optional<errors>
Tokenizer::process_tokenizing_error(size_t next_position,
                                    size_t value_position) {
  // If tokenizer’s policy is "strict", then throw a TypeError.
  if (policy == token_policy::strict) {
    ada_log("process_tokenizing_error failed with next_position=",
            next_position, " value_position=", value_position);
    return errors::type_error;
  }
  // Assert: tokenizer’s policy is "lenient".
  ADA_ASSERT_TRUE(policy == token_policy::lenient);
  // Run add a token with default length given tokenizer, "invalid-char", next
  // position, and value position.
  add_token_with_default_length(token_type::INVALID_CHAR, next_position,
                                value_position);
  return std::nullopt;
}

template <url_pattern_encoding_callback F>
token* url_pattern_parser<F>::try_consume_modifier_token() {
  // Let token be the result of running try to consume a token given parser and
  // "other-modifier".
  auto token = try_consume_token(token_type::OTHER_MODIFIER);
  // If token is not null, then return token.
  if (token) return token;
  // Set token to the result of running try to consume a token given parser and
  // "asterisk".
  // Return token.
  return try_consume_token(token_type::ASTERISK);
}

template <url_pattern_encoding_callback F>
token* url_pattern_parser<F>::try_consume_regexp_or_wildcard_token(
    const token* name_token) {
  // Let token be the result of running try to consume a token given parser and
  // "regexp".
  auto token = try_consume_token(token_type::REGEXP);
  // If name token is null and token is null, then set token to the result of
  // running try to consume a token given parser and "asterisk".
  if (!name_token && !token) {
    token = try_consume_token(token_type::ASTERISK);
  }
  // Return token.
  return token;
}

template <url_pattern_encoding_callback F>
token* url_pattern_parser<F>::try_consume_token(token_type type) {
  ada_log("url_pattern_parser::try_consume_token called with type=",
          to_string(type));
  // Assert: parser’s index is less than parser’s token list size.
  ADA_ASSERT_TRUE(index < tokens.size());
  // Let next token be parser’s token list[parser’s index].
  auto& next_token = tokens[index];
  // If next token’s type is not type return null.
  if (next_token.type != type) return nullptr;
  // Increase parser’s index by 1.
  index++;
  // Return next token.
  return &next_token;
}

template <url_pattern_encoding_callback F>
std::string url_pattern_parser<F>::consume_text() {
  // Let result be the empty string.
  std::string result{};
  // While true:
  while (true) {
    // Let token be the result of running try to consume a token given parser
    // and "char".
    auto token = try_consume_token(token_type::CHAR);
    // If token is null, then set token to the result of running try to consume
    // a token given parser and "escaped-char".
    if (!token) token = try_consume_token(token_type::ESCAPED_CHAR);
    // If token is null, then break.
    if (!token) break;
    // Append token’s value to the end of result.
    result.append(token->value);
  }
  // Return result.
  return result;
}

template <url_pattern_encoding_callback F>
bool url_pattern_parser<F>::consume_required_token(token_type type) {
  ada_log("url_pattern_parser::consume_required_token called with type=",
          to_string(type));
  // Let result be the result of running try to consume a token given parser and
  // type.
  return try_consume_token(type) != nullptr;
}

template <url_pattern_encoding_callback F>
std::optional<errors>
url_pattern_parser<F>::maybe_add_part_from_the_pending_fixed_value() {
  // If parser’s pending fixed value is the empty string, then return.
  if (pending_fixed_value.empty()) {
    ada_log("pending_fixed_value is empty");
    return std::nullopt;
  }
  // Let encoded value be the result of running parser’s encoding callback given
  // parser’s pending fixed value.
  auto encoded_value = encoding_callback(pending_fixed_value);
  if (!encoded_value) {
    ada_log("failed to encode pending_fixed_value: ", pending_fixed_value);
    return encoded_value.error();
  }
  // Set parser’s pending fixed value to the empty string.
  pending_fixed_value.clear();
  // Let part be a new part whose type is "fixed-text", value is encoded value,
  // and modifier is "none".
  // Append part to parser’s part list.
  parts.emplace_back(url_pattern_part_type::FIXED_TEXT,
                     std::move(*encoded_value),
                     url_pattern_part_modifier::none);
  return std::nullopt;
}

template <url_pattern_encoding_callback F>
std::optional<errors> url_pattern_parser<F>::add_part(
    std::string_view prefix, token* name_token, token* regexp_or_wildcard_token,
    std::string_view suffix, token* modifier_token) {
  // Let modifier be "none".
  auto modifier = url_pattern_part_modifier::none;
  // If modifier token is not null:
  if (modifier_token) {
    // If modifier token’s value is "?" then set modifier to "optional".
    if (modifier_token->value == "?") {
      modifier = url_pattern_part_modifier::optional;
    } else if (modifier_token->value == "*") {
      // Otherwise if modifier token’s value is "*" then set modifier to
      // "zero-or-more".
      modifier = url_pattern_part_modifier::zero_or_more;
    } else if (modifier_token->value == "+") {
      // Otherwise if modifier token’s value is "+" then set modifier to
      // "one-or-more".
      modifier = url_pattern_part_modifier::one_or_more;
    }
  }
  // If name token is null and regexp or wildcard token is null and modifier
  // is "none":
  if (!name_token && !regexp_or_wildcard_token &&
      modifier == url_pattern_part_modifier::none) {
    // Append prefix to the end of parser’s pending fixed value.
    pending_fixed_value.append(prefix);
    return std::nullopt;
  }
  // Run maybe add a part from the pending fixed value given parser.
  if (auto error = maybe_add_part_from_the_pending_fixed_value()) {
    return *error;
  }
  // If name token is null and regexp or wildcard token is null:
  if (!name_token && !regexp_or_wildcard_token) {
    // Assert: suffix is the empty string.
    ADA_ASSERT_TRUE(suffix.empty());
    // If prefix is the empty string, then return.
    if (prefix.empty()) return std::nullopt;
    // Let encoded value be the result of running parser’s encoding callback
    // given prefix.
    auto encoded_value = encoding_callback(prefix);
    if (!encoded_value) {
      return encoded_value.error();
    }
    // Let part be a new part whose type is "fixed-text", value is encoded
    // value, and modifier is modifier.
    // Append part to parser’s part list.
    parts.emplace_back(url_pattern_part_type::FIXED_TEXT,
                       std::move(*encoded_value), modifier);
    return std::nullopt;
  }
  // Let regexp value be the empty string.
  std::string regexp_value{};
  // If regexp or wildcard token is null, then set regexp value to parser’s
  // segment wildcard regexp.
  if (!regexp_or_wildcard_token) {
    regexp_value = segment_wildcard_regexp;
  } else if (regexp_or_wildcard_token->type == token_type::ASTERISK) {
    // Otherwise if regexp or wildcard token’s type is "asterisk", then set
    // regexp value to the full wildcard regexp value.
    regexp_value = ".*";
  } else {
    // Otherwise set regexp value to regexp or wildcard token’s value.
    regexp_value = regexp_or_wildcard_token->value;
  }
  // Let type be "regexp".
  auto type = url_pattern_part_type::REGEXP;
  // If regexp value is parser’s segment wildcard regexp:
  if (regexp_value == segment_wildcard_regexp) {
    // Set type to "segment-wildcard".
    type = url_pattern_part_type::SEGMENT_WILDCARD;
    // Set regexp value to the empty string.
    regexp_value.clear();
  } else if (regexp_value == ".*") {
    // Otherwise if regexp value is the full wildcard regexp value:
    // Set type to "full-wildcard".
    type = url_pattern_part_type::FULL_WILDCARD;
    // Set regexp value to the empty string.
    regexp_value.clear();
  }
  // Let name be the empty string.
  std::string name{};
  // If name token is not null, then set name to name token’s value.
  if (name_token) {
    name = name_token->value;
  } else if (regexp_or_wildcard_token != nullptr) {
    // Otherwise if regexp or wildcard token is not null:
    // Set name to parser’s next numeric name, serialized.
    name = std::to_string(next_numeric_name);
    // Increment parser’s next numeric name by 1.
    next_numeric_name++;
  }
  // If the result of running is a duplicate name given parser and name is
  // true, then throw a TypeError.
  if (std::ranges::any_of(
          parts, [&name](const auto& part) { return part.name == name; })) {
    return errors::type_error;
  }
  // Let encoded prefix be the result of running parser’s encoding callback
  // given prefix.
  auto encoded_prefix = encoding_callback(prefix);
  if (!encoded_prefix) return encoded_prefix.error();
  // Let encoded suffix be the result of running parser’s encoding callback
  // given suffix.
  auto encoded_suffix = encoding_callback(suffix);
  if (!encoded_suffix) return encoded_suffix.error();
  // Let part be a new part whose type is type, value is regexp value,
  // modifier is modifier, name is name, prefix is encoded prefix, and suffix
  // is encoded suffix.
  // Append part to parser’s part list.
  parts.emplace_back(type, std::move(regexp_value), modifier, std::move(name),
                     std::move(*encoded_prefix), std::move(*encoded_suffix));
  return std::nullopt;
}

template <url_pattern_encoding_callback F>
tl::expected<std::vector<url_pattern_part>, errors> parse_pattern_string(
    std::string_view input, url_pattern_compile_component_options& options,
    F& encoding_callback) {
  ada_log("parse_pattern_string input=", input);
  // Let parser be a new pattern parser whose encoding callback is encoding
  // callback and segment wildcard regexp is the result of running generate a
  // segment wildcard regexp given options.
  auto parser = url_pattern_parser<F>(
      encoding_callback, generate_segment_wildcard_regexp(options));
  // Set parser’s token list to the result of running tokenize given input and
  // "strict".
  auto tokenize_result = tokenize(input, token_policy::strict);
  if (!tokenize_result) {
    ada_log("parse_pattern_string tokenize failed");
    return tl::unexpected(tokenize_result.error());
  }
  parser.tokens = std::move(*tokenize_result);

  // While parser’s index is less than parser’s token list's size:
  while (parser.can_continue()) {
    // Let char token be the result of running try to consume a token given
    // parser and "char".
    auto char_token = parser.try_consume_token(token_type::CHAR);
    // Let name token be the result of running try to consume a token given
    // parser and "name".
    auto name_token = parser.try_consume_token(token_type::NAME);
    // Let regexp or wildcard token be the result of running try to consume a
    // regexp or wildcard token given parser and name token.
    auto regexp_or_wildcard_token =
        parser.try_consume_regexp_or_wildcard_token(name_token);
    // If name token is not null or regexp or wildcard token is not null:
    if (name_token || regexp_or_wildcard_token) {
      // Let prefix be the empty string.
      std::string prefix{};
      // If char token is not null then set prefix to char token’s value.
      if (char_token) prefix = char_token->value;
      // If prefix is not the empty string and not options’s prefix code point:
      if (!prefix.empty() && prefix != options.get_prefix()) {
        // Append prefix to the end of parser’s pending fixed value.
        parser.pending_fixed_value.append(prefix);
        // Set prefix to the empty string.
        prefix.clear();
      }
      // Run maybe add a part from the pending fixed value given parser.
      if (auto error = parser.maybe_add_part_from_the_pending_fixed_value()) {
        ada_log("maybe_add_part_from_the_pending_fixed_value failed");
        return tl::unexpected(*error);
      }
      // Let modifier token be the result of running try to consume a modifier
      // token given parser.
      auto modifier_token = parser.try_consume_modifier_token();
      // Run add a part given parser, prefix, name token, regexp or wildcard
      // token, the empty string, and modifier token.
      if (auto error =
              parser.add_part(prefix, name_token, regexp_or_wildcard_token, "",
                              modifier_token)) {
        ada_log("parser.add_part failed");
        return tl::unexpected(*error);
      }
      // Continue.
      continue;
    }

    // Let fixed token be char token.
    auto fixed_token = char_token;
    // If fixed token is null, then set fixed token to the result of running try
    // to consume a token given parser and "escaped-char".
    if (!fixed_token)
      fixed_token = parser.try_consume_token(token_type::ESCAPED_CHAR);
    // If fixed token is not null:
    if (fixed_token) {
      // Append fixed token’s value to parser’s pending fixed value.
      parser.pending_fixed_value.append(fixed_token->value);
      // Continue.
      continue;
    }
    // Let open token be the result of running try to consume a token given
    // parser and "open".
    auto open_token = parser.try_consume_token(token_type::OPEN);
    // If open token is not null:
    if (open_token) {
      // Set prefix be the result of running consume text given parser.
      auto prefix_ = parser.consume_text();
      // Set name token to the result of running try to consume a token given
      // parser and "name".
      name_token = parser.try_consume_token(token_type::NAME);
      // Set regexp or wildcard token to the result of running try to consume a
      // regexp or wildcard token given parser and name token.
      regexp_or_wildcard_token =
          parser.try_consume_regexp_or_wildcard_token(name_token);
      // Let suffix be the result of running consume text given parser.
      auto suffix_ = parser.consume_text();
      // Run consume a required token given parser and "close".
      if (!parser.consume_required_token(token_type::CLOSE)) {
        ada_log("parser.consume_required_token failed");
        return tl::unexpected(errors::type_error);
      }
      // Set modifier token to the result of running try to consume a modifier
      // token given parser.
      auto modifier_token = parser.try_consume_modifier_token();
      // Run add a part given parser, prefix, name token, regexp or wildcard
      // token, suffix, and modifier token.
      if (auto error =
              parser.add_part(prefix_, name_token, regexp_or_wildcard_token,
                              suffix_, modifier_token)) {
        return tl::unexpected(*error);
      }
      // Continue.
      continue;
    }
    // Run maybe add a part from the pending fixed value given parser.
    if (auto error = parser.maybe_add_part_from_the_pending_fixed_value()) {
      ada_log("maybe_add_part_from_the_pending_fixed_value failed on line 992");
      return tl::unexpected(*error);
    }
    // Run consume a required token given parser and "end".
    if (!parser.consume_required_token(token_type::END)) {
      return tl::unexpected(errors::type_error);
    }
  }
  ada_log("parser.parts size is: ", parser.parts.size());
  // Return parser’s part list.
  return parser.parts;
}

template <url_pattern_regex::regex_concept regex_provider>
bool protocol_component_matches_special_scheme(
    url_pattern_component<regex_provider>& component) {
  // let's avoid unnecessary copy here.
  auto& regex = component.regexp;
  return regex_provider::regex_match("http", regex) ||
         regex_provider::regex_match("https", regex) ||
         regex_provider::regex_match("ws", regex) ||
         regex_provider::regex_match("wss", regex) ||
         regex_provider::regex_match("ftp", regex);
}

template <url_pattern_regex::regex_concept regex_provider>
inline std::optional<errors> constructor_string_parser<
    regex_provider>::compute_protocol_matches_special_scheme_flag() {
  ada_log(
      "constructor_string_parser::compute_protocol_matches_special_scheme_"
      "flag");
  // Let protocol string be the result of running make a component string given
  // parser.
  auto protocol_string = make_component_string();
  // Let protocol component be the result of compiling a component given
  // protocol string, canonicalize a protocol, and default options.
  auto protocol_component = url_pattern_component<regex_provider>::compile(
      protocol_string, canonicalize_protocol,
      url_pattern_compile_component_options::DEFAULT);
  if (!protocol_component) {
    ada_log("url_pattern_component::compile failed for protocol_string ",
            protocol_string);
    return protocol_component.error();
  }
  // If the result of running protocol component matches a special scheme given
  // protocol component is true, then set parser’s protocol matches a special
  // scheme flag to true.
  if (protocol_component_matches_special_scheme(*protocol_component)) {
    protocol_matches_a_special_scheme_flag = true;
  }
  return std::nullopt;
}

template <url_pattern_regex::regex_concept regex_provider>
tl::expected<url_pattern_init, errors>
constructor_string_parser<regex_provider>::parse(std::string_view input) {
  ada_log("constructor_string_parser::parse input=", input);
  // Let parser be a new constructor string parser whose input is input and
  // token list is the result of running tokenize given input and "lenient".
  auto token_list = tokenize(input, token_policy::lenient);
  if (!token_list) {
    return tl::unexpected(token_list.error());
  }
  auto parser = constructor_string_parser(input, std::move(*token_list));

  // While parser’s token index is less than parser’s token list size:
  while (parser.token_index < parser.token_list.size()) {
    // Set parser’s token increment to 1.
    parser.token_increment = 1;

    // If parser’s token list[parser’s token index]'s type is "end" then:
    if (parser.token_list[parser.token_index].type == token_type::END) {
      // If parser’s state is "init":
      if (parser.state == State::INIT) {
        // Run rewind given parser.
        parser.rewind();
        // If the result of running is a hash prefix given parser is true, then
        // run change state given parser, "hash" and 1.
        if (parser.is_hash_prefix()) {
          parser.change_state(State::HASH, 1);
        } else if (parser.is_search_prefix()) {
          // Otherwise if the result of running is a search prefix given parser
          // is true: Run change state given parser, "search" and 1.
          parser.change_state(State::SEARCH, 1);
        } else {
          // Run change state given parser, "pathname" and 0.
          parser.change_state(State::PATHNAME, 0);
        }
        // Increment parser’s token index by parser’s token increment.
        parser.token_index += parser.token_increment;
        // Continue.
        continue;
      }

      if (parser.state == State::AUTHORITY) {
        // If parser’s state is "authority":
        // Run rewind and set state given parser, and "hostname".
        parser.rewind();
        parser.change_state(State::HOSTNAME, 0);
        // Increment parser’s token index by parser’s token increment.
        parser.token_index += parser.token_increment;
        // Continue.
        continue;
      }

      // Run change state given parser, "done" and 0.
      parser.change_state(State::DONE, 0);
      // Break.
      break;
    }

    // If the result of running is a group open given parser is true:
    if (parser.is_group_open()) {
      // Increment parser’s group depth by 1.
      parser.group_depth += 1;
      // Increment parser’s token index by parser’s token increment.
      parser.token_index += parser.token_increment;
    }

    // If parser’s group depth is greater than 0:
    if (parser.group_depth > 0) {
      // If the result of running is a group close given parser is true, then
      // decrement parser’s group depth by 1.
      if (parser.is_group_close()) {
        parser.group_depth -= 1;
      } else {
        // Increment parser’s token index by parser’s token increment.
        parser.token_index += parser.token_increment;
        continue;
      }
    }

    // Switch on parser’s state and run the associated steps:
    switch (parser.state) {
      case State::INIT: {
        // If the result of running is a protocol suffix given parser is true:
        if (parser.is_protocol_suffix()) {
          // Run rewind and set state given parser and "protocol".
          parser.rewind();
          parser.change_state(State::PROTOCOL, 0);
        }
        break;
      }
      case State::PROTOCOL: {
        // If the result of running is a protocol suffix given parser is true:
        if (parser.is_protocol_suffix()) {
          // Run compute protocol matches a special scheme flag given parser.
          if (const auto error =
                  parser.compute_protocol_matches_special_scheme_flag()) {
            ada_log("compute_protocol_matches_special_scheme_flag failed");
            return tl::unexpected(*error);
          }
          // Let next state be "pathname".
          auto next_state = State::PATHNAME;
          // Let skip be 1.
          auto skip = 1;
          // If the result of running next is authority slashes given parser is
          // true:
          if (parser.next_is_authority_slashes()) {
            // Set next state to "authority".
            next_state = State::AUTHORITY;
            // Set skip to 3.
            skip = 3;
          } else if (parser.protocol_matches_a_special_scheme_flag) {
            // Otherwise if parser’s protocol matches a special scheme flag is
            // true, then set next state to "authority".
            next_state = State::AUTHORITY;
          }

          // Run change state given parser, next state, and skip.
          parser.change_state(next_state, skip);
        }
        break;
      }
      case State::AUTHORITY: {
        // If the result of running is an identity terminator given parser is
        // true, then run rewind and set state given parser and "username".
        if (parser.is_an_identity_terminator()) {
          parser.rewind();
          parser.change_state(State::USERNAME, 0);
        } else if (parser.is_pathname_start() || parser.is_search_prefix() ||
                   parser.is_hash_prefix()) {
          // Otherwise if any of the following are true:
          // - the result of running is a pathname start given parser;
          // - the result of running is a search prefix given parser; or
          // - the result of running is a hash prefix given parser,
          // then run rewind and set state given parser and "hostname".
          parser.rewind();
          parser.change_state(State::HOSTNAME, 0);
        }
        break;
      }
      case State::USERNAME: {
        // If the result of running is a password prefix given parser is true,
        // then run change state given parser, "password", and 1.
        if (parser.is_password_prefix()) {
          parser.change_state(State::PASSWORD, 1);
        } else if (parser.is_an_identity_terminator()) {
          // Otherwise if the result of running is an identity terminator given
          // parser is true, then run change state given parser, "hostname",
          // and 1.
          parser.change_state(State::HOSTNAME, 1);
        }
        break;
      }
      case State::PASSWORD: {
        // If the result of running is an identity terminator given parser is
        // true, then run change state given parser, "hostname", and 1.
        if (parser.is_an_identity_terminator()) {
          parser.change_state(State::HOSTNAME, 1);
        }
        break;
      }
      case State::HOSTNAME: {
        // If the result of running is an IPv6 open given parser is true, then
        // increment parser’s hostname IPv6 bracket depth by 1.
        if (parser.is_an_ipv6_open()) {
          parser.hostname_ipv6_bracket_depth += 1;
        } else if (parser.is_an_ipv6_close()) {
          // Otherwise if the result of running is an IPv6 close given parser is
          // true, then decrement parser’s hostname IPv6 bracket depth by 1.
          parser.hostname_ipv6_bracket_depth -= 1;
        } else if (parser.is_port_prefix() &&
                   parser.hostname_ipv6_bracket_depth == 0) {
          // Otherwise if the result of running is a port prefix given parser is
          // true and parser’s hostname IPv6 bracket depth is zero, then run
          // change state given parser, "port", and 1.
          parser.change_state(State::PORT, 1);
        } else if (parser.is_pathname_start()) {
          // Otherwise if the result of running is a pathname start given parser
          // is true, then run change state given parser, "pathname", and 0.
          parser.change_state(State::PATHNAME, 0);
        } else if (parser.is_search_prefix()) {
          // Otherwise if the result of running is a search prefix given parser
          // is true, then run change state given parser, "search", and 1.
          parser.change_state(State::SEARCH, 1);
        } else if (parser.is_hash_prefix()) {
          // Otherwise if the result of running is a hash prefix given parser is
          // true, then run change state given parser, "hash", and 1.
          parser.change_state(State::HASH, 1);
        }

        break;
      }
      case State::PORT: {
        // If the result of running is a pathname start given parser is true,
        // then run change state given parser, "pathname", and 0.
        if (parser.is_pathname_start()) {
          parser.change_state(State::PATHNAME, 0);
        } else if (parser.is_search_prefix()) {
          // Otherwise if the result of running is a search prefix given parser
          // is true, then run change state given parser, "search", and 1.
          parser.change_state(State::SEARCH, 1);
        } else if (parser.is_hash_prefix()) {
          // Otherwise if the result of running is a hash prefix given parser is
          // true, then run change state given parser, "hash", and 1.
          parser.change_state(State::HASH, 1);
        }
        break;
      }
      case State::PATHNAME: {
        // If the result of running is a search prefix given parser is true,
        // then run change state given parser, "search", and 1.
        if (parser.is_search_prefix()) {
          parser.change_state(State::SEARCH, 1);
        } else if (parser.is_hash_prefix()) {
          // Otherwise if the result of running is a hash prefix given parser is
          // true, then run change state given parser, "hash", and 1.
          parser.change_state(State::HASH, 1);
        }
        break;
      }
      case State::SEARCH: {
        // If the result of running is a hash prefix given parser is true, then
        // run change state given parser, "hash", and 1.
        if (parser.is_hash_prefix()) {
          parser.change_state(State::HASH, 1);
        }
      }
      case State::HASH: {
        // Do nothing
        break;
      }
      default: {
        // Assert: This step is never reached.
        unreachable();
      }
    }

    // Increment parser’s token index by parser’s token increment.
    parser.token_index += parser.token_increment;
  }

  // If parser’s result contains "hostname" and not "port", then set parser’s
  // result["port"] to the empty string.
  if (parser.result.hostname && !parser.result.port) {
    parser.result.port = "";
  }

  // Return parser’s result.
  return parser.result;
}

}  // namespace ada::url_pattern_helpers

#endif
/* end file include/ada/url_pattern_helpers-inl.h */

// Public API
/* begin file include/ada/ada_version.h */
/**
 * @file ada_version.h
 * @brief Definitions for Ada's version number.
 */
#ifndef ADA_ADA_VERSION_H
#define ADA_ADA_VERSION_H

#define ADA_VERSION "3.1.3"

namespace ada {

enum {
  ADA_VERSION_MAJOR = 3,
  ADA_VERSION_MINOR = 1,
  ADA_VERSION_REVISION = 3,
};

}  // namespace ada

#endif  // ADA_ADA_VERSION_H
/* end file include/ada/ada_version.h */
/* begin file include/ada/implementation-inl.h */
/**
 * @file implementation-inl.h
 */
#ifndef ADA_IMPLEMENTATION_INL_H
#define ADA_IMPLEMENTATION_INL_H


#include <variant>
#include <string_view>

namespace ada {

template <url_pattern_regex::regex_concept regex_provider>
ada_warn_unused tl::expected<url_pattern<regex_provider>, errors>
parse_url_pattern(std::variant<std::string_view, url_pattern_init> input,
                  const std::string_view* base_url,
                  const url_pattern_options* options) {
  return parser::parse_url_pattern_impl<regex_provider>(std::move(input),
                                                        base_url, options);
}

}  // namespace ada

#endif  // ADA_IMPLEMENTATION_INL_H
/* end file include/ada/implementation-inl.h */

#endif  // ADA_H
/* end file include/ada.h */
