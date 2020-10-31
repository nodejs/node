// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// from google3/strings/strutil.h

#ifndef GOOGLE_PROTOBUF_STUBS_STRUTIL_H__
#define GOOGLE_PROTOBUF_STUBS_STRUTIL_H__

#include <stdlib.h>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringpiece.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

#if defined(_MSC_VER) && _MSC_VER < 1800
#define strtoll  _strtoi64
#define strtoull _strtoui64
#elif defined(__DECCXX) && defined(__osf__)
// HP C++ on Tru64 does not have strtoll, but strtol is already 64-bit.
#define strtoll strtol
#define strtoull strtoul
#endif

// ----------------------------------------------------------------------
// ascii_isalnum()
//    Check if an ASCII character is alphanumeric.  We can't use ctype's
//    isalnum() because it is affected by locale.  This function is applied
//    to identifiers in the protocol buffer language, not to natural-language
//    strings, so locale should not be taken into account.
// ascii_isdigit()
//    Like above, but only accepts digits.
// ascii_isspace()
//    Check if the character is a space character.
// ----------------------------------------------------------------------

inline bool ascii_isalnum(char c) {
  return ('a' <= c && c <= 'z') ||
         ('A' <= c && c <= 'Z') ||
         ('0' <= c && c <= '9');
}

inline bool ascii_isdigit(char c) {
  return ('0' <= c && c <= '9');
}

inline bool ascii_isspace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' ||
      c == '\r';
}

inline bool ascii_isupper(char c) {
  return c >= 'A' && c <= 'Z';
}

inline bool ascii_islower(char c) {
  return c >= 'a' && c <= 'z';
}

inline char ascii_toupper(char c) {
  return ascii_islower(c) ? c - ('a' - 'A') : c;
}

inline char ascii_tolower(char c) {
  return ascii_isupper(c) ? c + ('a' - 'A') : c;
}

inline int hex_digit_to_int(char c) {
  /* Assume ASCII. */
  int x = static_cast<unsigned char>(c);
  if (x > '9') {
    x += 9;
  }
  return x & 0xf;
}

// ----------------------------------------------------------------------
// HasPrefixString()
//    Check if a string begins with a given prefix.
// StripPrefixString()
//    Given a string and a putative prefix, returns the string minus the
//    prefix string if the prefix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool HasPrefixString(const string& str,
                            const string& prefix) {
  return str.size() >= prefix.size() &&
         str.compare(0, prefix.size(), prefix) == 0;
}

inline string StripPrefixString(const string& str, const string& prefix) {
  if (HasPrefixString(str, prefix)) {
    return str.substr(prefix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// HasSuffixString()
//    Return true if str ends in suffix.
// StripSuffixString()
//    Given a string and a putative suffix, returns the string minus the
//    suffix string if the suffix matches, otherwise the original
//    string.
// ----------------------------------------------------------------------
inline bool HasSuffixString(const string& str,
                            const string& suffix) {
  return str.size() >= suffix.size() &&
         str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline string StripSuffixString(const string& str, const string& suffix) {
  if (HasSuffixString(str, suffix)) {
    return str.substr(0, str.size() - suffix.size());
  } else {
    return str;
  }
}

// ----------------------------------------------------------------------
// ReplaceCharacters
//    Replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
//    Good for keeping html characters or protocol characters (\t) out
//    of places where they might cause a problem.
// StripWhitespace
//    Removes whitespaces from both ends of the given string.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT void ReplaceCharacters(string* s, const char* remove,
                                       char replacewith);
PROTOBUF_EXPORT void StripString(string* s, const char* remove,
                                 char replacewith);

PROTOBUF_EXPORT void StripWhitespace(string* s);

// ----------------------------------------------------------------------
// LowerString()
// UpperString()
// ToUpper()
//    Convert the characters in "s" to lowercase or uppercase.  ASCII-only:
//    these functions intentionally ignore locale because they are applied to
//    identifiers used in the Protocol Buffer language, not to natural-language
//    strings.
// ----------------------------------------------------------------------

inline void LowerString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // tolower() changes based on locale.  We don't want this!
    if ('A' <= *i && *i <= 'Z') *i += 'a' - 'A';
  }
}

inline void UpperString(string * s) {
  string::iterator end = s->end();
  for (string::iterator i = s->begin(); i != end; ++i) {
    // toupper() changes based on locale.  We don't want this!
    if ('a' <= *i && *i <= 'z') *i += 'A' - 'a';
  }
}

inline string ToUpper(const string& s) {
  string out = s;
  UpperString(&out);
  return out;
}

// ----------------------------------------------------------------------
// StringReplace()
//    Give me a string and two patterns "old" and "new", and I replace
//    the first instance of "old" in the string with "new", if it
//    exists.  RETURN a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

PROTOBUF_EXPORT string StringReplace(const string& s, const string& oldsub,
                                     const string& newsub, bool replace_all);

// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.  If there are consecutive delimiters, this function skips
//    over all of them.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT void SplitStringUsing(const string& full, const char* delim,
                                      std::vector<string>* res);

// Split a string using one or more byte delimiters, presented
// as a nul-terminated c string. Append the components to 'result'.
// If there are consecutive delimiters, this function will return
// corresponding empty strings.  If you want to drop the empty
// strings, try SplitStringUsing().
//
// If "full" is the empty string, yields an empty string as the only value.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT void SplitStringAllowEmpty(const string& full,
                                           const char* delim,
                                           std::vector<string>* result);

// ----------------------------------------------------------------------
// Split()
//    Split a string using a character delimiter.
// ----------------------------------------------------------------------
inline std::vector<string> Split(
    const string& full, const char* delim, bool skip_empty = true) {
  std::vector<string> result;
  if (skip_empty) {
    SplitStringUsing(full, delim, &result);
  } else {
    SplitStringAllowEmpty(full, delim, &result);
  }
  return result;
}

// ----------------------------------------------------------------------
// JoinStrings()
//    These methods concatenate a vector of strings into a C++ string, using
//    the C-string "delim" as a separator between components. There are two
//    flavors of the function, one flavor returns the concatenated string,
//    another takes a pointer to the target string. In the latter case the
//    target string is cleared and overwritten.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT void JoinStrings(const std::vector<string>& components,
                                 const char* delim, string* result);

inline string JoinStrings(const std::vector<string>& components,
                          const char* delim) {
  string result;
  JoinStrings(components, delim, &result);
  return result;
}

// ----------------------------------------------------------------------
// UnescapeCEscapeSequences()
//    Copies "source" to "dest", rewriting C-style escape sequences
//    -- '\n', '\r', '\\', '\ooo', etc -- to their ASCII
//    equivalents.  "dest" must be sufficiently large to hold all
//    the characters in the rewritten string (i.e. at least as large
//    as strlen(source) + 1 should be safe, since the replacements
//    are always shorter than the original escaped sequences).  It's
//    safe for source and dest to be the same.  RETURNS the length
//    of dest.
//
//    It allows hex sequences \xhh, or generally \xhhhhh with an
//    arbitrary number of hex digits, but all of them together must
//    specify a value of a single byte (e.g. \x0045 is equivalent
//    to \x45, and \x1234 is erroneous).
//
//    It also allows escape sequences of the form \uhhhh (exactly four
//    hex digits, upper or lower case) or \Uhhhhhhhh (exactly eight
//    hex digits, upper or lower case) to specify a Unicode code
//    point. The dest array will contain the UTF8-encoded version of
//    that code-point (e.g., if source contains \u2019, then dest will
//    contain the three bytes 0xE2, 0x80, and 0x99).
//
//    Errors: In the first form of the call, errors are reported with
//    LOG(ERROR). The same is true for the second form of the call if
//    the pointer to the string std::vector is nullptr; otherwise, error
//    messages are stored in the std::vector. In either case, the effect on
//    the dest array is not defined, but rest of the source will be
//    processed.
//    ----------------------------------------------------------------------

PROTOBUF_EXPORT int UnescapeCEscapeSequences(const char* source, char* dest);
PROTOBUF_EXPORT int UnescapeCEscapeSequences(const char* source, char* dest,
                                             std::vector<string>* errors);

// ----------------------------------------------------------------------
// UnescapeCEscapeString()
//    This does the same thing as UnescapeCEscapeSequences, but creates
//    a new string. The caller does not need to worry about allocating
//    a dest buffer. This should be used for non performance critical
//    tasks such as printing debug messages. It is safe for src and dest
//    to be the same.
//
//    The second call stores its errors in a supplied string vector.
//    If the string vector pointer is nullptr, it reports the errors with LOG().
//
//    In the first and second calls, the length of dest is returned. In the
//    the third call, the new string is returned.
// ----------------------------------------------------------------------

PROTOBUF_EXPORT int UnescapeCEscapeString(const string& src, string* dest);
PROTOBUF_EXPORT int UnescapeCEscapeString(const string& src, string* dest,
                                          std::vector<string>* errors);
PROTOBUF_EXPORT string UnescapeCEscapeString(const string& src);

// ----------------------------------------------------------------------
// CEscape()
//    Escapes 'src' using C-style escape sequences and returns the resulting
//    string.
//
//    Escaped chars: \n, \r, \t, ", ', \, and !isprint().
// ----------------------------------------------------------------------
PROTOBUF_EXPORT string CEscape(const string& src);

// ----------------------------------------------------------------------
// CEscapeAndAppend()
//    Escapes 'src' using C-style escape sequences, and appends the escaped
//    string to 'dest'.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT void CEscapeAndAppend(StringPiece src, string* dest);

namespace strings {
// Like CEscape() but does not escape bytes with the upper bit set.
PROTOBUF_EXPORT string Utf8SafeCEscape(const string& src);

// Like CEscape() but uses hex (\x) escapes instead of octals.
PROTOBUF_EXPORT string CHexEscape(const string& src);
}  // namespace strings

// ----------------------------------------------------------------------
// strto32()
// strtou32()
// strto64()
// strtou64()
//    Architecture-neutral plug compatible replacements for strtol() and
//    strtoul().  Long's have different lengths on ILP-32 and LP-64
//    platforms, so using these is safer, from the point of view of
//    overflow behavior, than using the standard libc functions.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int32 strto32_adaptor(const char* nptr, char** endptr,
                                      int base);
PROTOBUF_EXPORT uint32 strtou32_adaptor(const char* nptr, char** endptr,
                                        int base);

inline int32 strto32(const char *nptr, char **endptr, int base) {
  if (sizeof(int32) == sizeof(long))
    return strtol(nptr, endptr, base);
  else
    return strto32_adaptor(nptr, endptr, base);
}

inline uint32 strtou32(const char *nptr, char **endptr, int base) {
  if (sizeof(uint32) == sizeof(unsigned long))
    return strtoul(nptr, endptr, base);
  else
    return strtou32_adaptor(nptr, endptr, base);
}

// For now, long long is 64-bit on all the platforms we care about, so these
// functions can simply pass the call to strto[u]ll.
inline int64 strto64(const char *nptr, char **endptr, int base) {
  GOOGLE_COMPILE_ASSERT(sizeof(int64) == sizeof(long long),
                        sizeof_int64_is_not_sizeof_long_long);
  return strtoll(nptr, endptr, base);
}

inline uint64 strtou64(const char *nptr, char **endptr, int base) {
  GOOGLE_COMPILE_ASSERT(sizeof(uint64) == sizeof(unsigned long long),
                        sizeof_uint64_is_not_sizeof_long_long);
  return strtoull(nptr, endptr, base);
}

// ----------------------------------------------------------------------
// safe_strtob()
// safe_strto32()
// safe_strtou32()
// safe_strto64()
// safe_strtou64()
// safe_strtof()
// safe_strtod()
// ----------------------------------------------------------------------
PROTOBUF_EXPORT bool safe_strtob(StringPiece str, bool* value);

PROTOBUF_EXPORT bool safe_strto32(const string& str, int32* value);
PROTOBUF_EXPORT bool safe_strtou32(const string& str, uint32* value);
inline bool safe_strto32(const char* str, int32* value) {
  return safe_strto32(string(str), value);
}
inline bool safe_strto32(StringPiece str, int32* value) {
  return safe_strto32(str.ToString(), value);
}
inline bool safe_strtou32(const char* str, uint32* value) {
  return safe_strtou32(string(str), value);
}
inline bool safe_strtou32(StringPiece str, uint32* value) {
  return safe_strtou32(str.ToString(), value);
}

PROTOBUF_EXPORT bool safe_strto64(const string& str, int64* value);
PROTOBUF_EXPORT bool safe_strtou64(const string& str, uint64* value);
inline bool safe_strto64(const char* str, int64* value) {
  return safe_strto64(string(str), value);
}
inline bool safe_strto64(StringPiece str, int64* value) {
  return safe_strto64(str.ToString(), value);
}
inline bool safe_strtou64(const char* str, uint64* value) {
  return safe_strtou64(string(str), value);
}
inline bool safe_strtou64(StringPiece str, uint64* value) {
  return safe_strtou64(str.ToString(), value);
}

PROTOBUF_EXPORT bool safe_strtof(const char* str, float* value);
PROTOBUF_EXPORT bool safe_strtod(const char* str, double* value);
inline bool safe_strtof(const string& str, float* value) {
  return safe_strtof(str.c_str(), value);
}
inline bool safe_strtod(const string& str, double* value) {
  return safe_strtod(str.c_str(), value);
}
inline bool safe_strtof(StringPiece str, float* value) {
  return safe_strtof(str.ToString(), value);
}
inline bool safe_strtod(StringPiece str, double* value) {
  return safe_strtod(str.ToString(), value);
}

// ----------------------------------------------------------------------
// FastIntToBuffer()
// FastHexToBuffer()
// FastHex64ToBuffer()
// FastHex32ToBuffer()
// FastTimeToBuffer()
//    These are intended for speed.  FastIntToBuffer() assumes the
//    integer is non-negative.  FastHexToBuffer() puts output in
//    hex rather than decimal.  FastTimeToBuffer() puts the output
//    into RFC822 format.
//
//    FastHex64ToBuffer() puts a 64-bit unsigned value in hex-format,
//    padded to exactly 16 bytes (plus one byte for '\0')
//
//    FastHex32ToBuffer() puts a 32-bit unsigned value in hex-format,
//    padded to exactly 8 bytes (plus one byte for '\0')
//
//       All functions take the output buffer as an arg.
//    They all return a pointer to the beginning of the output,
//    which may not be the beginning of the input buffer.
// ----------------------------------------------------------------------

// Suggested buffer size for FastToBuffer functions.  Also works with
// DoubleToBuffer() and FloatToBuffer().
static const int kFastToBufferSize = 32;

PROTOBUF_EXPORT char* FastInt32ToBuffer(int32 i, char* buffer);
PROTOBUF_EXPORT char* FastInt64ToBuffer(int64 i, char* buffer);
char* FastUInt32ToBuffer(uint32 i, char* buffer);  // inline below
char* FastUInt64ToBuffer(uint64 i, char* buffer);  // inline below
PROTOBUF_EXPORT char* FastHexToBuffer(int i, char* buffer);
PROTOBUF_EXPORT char* FastHex64ToBuffer(uint64 i, char* buffer);
PROTOBUF_EXPORT char* FastHex32ToBuffer(uint32 i, char* buffer);

// at least 22 bytes long
inline char* FastIntToBuffer(int i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastInt32ToBuffer(i, buffer) : FastInt64ToBuffer(i, buffer));
}
inline char* FastUIntToBuffer(unsigned int i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastUInt32ToBuffer(i, buffer) : FastUInt64ToBuffer(i, buffer));
}
inline char* FastLongToBuffer(long i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastInt32ToBuffer(i, buffer) : FastInt64ToBuffer(i, buffer));
}
inline char* FastULongToBuffer(unsigned long i, char* buffer) {
  return (sizeof(i) == 4 ?
          FastUInt32ToBuffer(i, buffer) : FastUInt64ToBuffer(i, buffer));
}

// ----------------------------------------------------------------------
// FastInt32ToBufferLeft()
// FastUInt32ToBufferLeft()
// FastInt64ToBufferLeft()
// FastUInt64ToBufferLeft()
//
// Like the Fast*ToBuffer() functions above, these are intended for speed.
// Unlike the Fast*ToBuffer() functions, however, these functions write
// their output to the beginning of the buffer (hence the name, as the
// output is left-aligned).  The caller is responsible for ensuring that
// the buffer has enough space to hold the output.
//
// Returns a pointer to the end of the string (i.e. the null character
// terminating the string).
// ----------------------------------------------------------------------

PROTOBUF_EXPORT char* FastInt32ToBufferLeft(int32 i, char* buffer);
PROTOBUF_EXPORT char* FastUInt32ToBufferLeft(uint32 i, char* buffer);
PROTOBUF_EXPORT char* FastInt64ToBufferLeft(int64 i, char* buffer);
PROTOBUF_EXPORT char* FastUInt64ToBufferLeft(uint64 i, char* buffer);

// Just define these in terms of the above.
inline char* FastUInt32ToBuffer(uint32 i, char* buffer) {
  FastUInt32ToBufferLeft(i, buffer);
  return buffer;
}
inline char* FastUInt64ToBuffer(uint64 i, char* buffer) {
  FastUInt64ToBufferLeft(i, buffer);
  return buffer;
}

inline string SimpleBtoa(bool value) {
  return value ? "true" : "false";
}

// ----------------------------------------------------------------------
// SimpleItoa()
//    Description: converts an integer to a string.
//
//    Return value: string
// ----------------------------------------------------------------------
PROTOBUF_EXPORT string SimpleItoa(int i);
PROTOBUF_EXPORT string SimpleItoa(unsigned int i);
PROTOBUF_EXPORT string SimpleItoa(long i);
PROTOBUF_EXPORT string SimpleItoa(unsigned long i);
PROTOBUF_EXPORT string SimpleItoa(long long i);
PROTOBUF_EXPORT string SimpleItoa(unsigned long long i);

// ----------------------------------------------------------------------
// SimpleDtoa()
// SimpleFtoa()
// DoubleToBuffer()
// FloatToBuffer()
//    Description: converts a double or float to a string which, if
//    passed to NoLocaleStrtod(), will produce the exact same original double
//    (except in case of NaN; all NaNs are considered the same value).
//    We try to keep the string short but it's not guaranteed to be as
//    short as possible.
//
//    DoubleToBuffer() and FloatToBuffer() write the text to the given
//    buffer and return it.  The buffer must be at least
//    kDoubleToBufferSize bytes for doubles and kFloatToBufferSize
//    bytes for floats.  kFastToBufferSize is also guaranteed to be large
//    enough to hold either.
//
//    Return value: string
// ----------------------------------------------------------------------
PROTOBUF_EXPORT string SimpleDtoa(double value);
PROTOBUF_EXPORT string SimpleFtoa(float value);

PROTOBUF_EXPORT char* DoubleToBuffer(double i, char* buffer);
PROTOBUF_EXPORT char* FloatToBuffer(float i, char* buffer);

// In practice, doubles should never need more than 24 bytes and floats
// should never need more than 14 (including null terminators), but we
// overestimate to be safe.
static const int kDoubleToBufferSize = 32;
static const int kFloatToBufferSize = 24;

namespace strings {

enum PadSpec {
  NO_PAD = 1,
  ZERO_PAD_2,
  ZERO_PAD_3,
  ZERO_PAD_4,
  ZERO_PAD_5,
  ZERO_PAD_6,
  ZERO_PAD_7,
  ZERO_PAD_8,
  ZERO_PAD_9,
  ZERO_PAD_10,
  ZERO_PAD_11,
  ZERO_PAD_12,
  ZERO_PAD_13,
  ZERO_PAD_14,
  ZERO_PAD_15,
  ZERO_PAD_16,
};

struct Hex {
  uint64 value;
  enum PadSpec spec;
  template <class Int>
  explicit Hex(Int v, PadSpec s = NO_PAD)
      : spec(s) {
    // Prevent sign-extension by casting integers to
    // their unsigned counterparts.
#ifdef LANG_CXX11
    static_assert(
        sizeof(v) == 1 || sizeof(v) == 2 || sizeof(v) == 4 || sizeof(v) == 8,
        "Unknown integer type");
#endif
    value = sizeof(v) == 1 ? static_cast<uint8>(v)
          : sizeof(v) == 2 ? static_cast<uint16>(v)
          : sizeof(v) == 4 ? static_cast<uint32>(v)
          : static_cast<uint64>(v);
  }
};

struct PROTOBUF_EXPORT AlphaNum {
  const char *piece_data_;  // move these to string_ref eventually
  size_t piece_size_;       // move these to string_ref eventually

  char digits[kFastToBufferSize];

  // No bool ctor -- bools convert to an integral type.
  // A bool ctor would also convert incoming pointers (bletch).

  AlphaNum(int i32)
      : piece_data_(digits),
        piece_size_(FastInt32ToBufferLeft(i32, digits) - &digits[0]) {}
  AlphaNum(unsigned int u32)
      : piece_data_(digits),
        piece_size_(FastUInt32ToBufferLeft(u32, digits) - &digits[0]) {}
  AlphaNum(long long i64)
      : piece_data_(digits),
        piece_size_(FastInt64ToBufferLeft(i64, digits) - &digits[0]) {}
  AlphaNum(unsigned long long u64)
      : piece_data_(digits),
        piece_size_(FastUInt64ToBufferLeft(u64, digits) - &digits[0]) {}

  // Note: on some architectures, "long" is only 32 bits, not 64, but the
  // performance hit of using FastInt64ToBufferLeft to handle 32-bit values
  // is quite minor.
  AlphaNum(long i64)
      : piece_data_(digits),
        piece_size_(FastInt64ToBufferLeft(i64, digits) - &digits[0]) {}
  AlphaNum(unsigned long u64)
      : piece_data_(digits),
        piece_size_(FastUInt64ToBufferLeft(u64, digits) - &digits[0]) {}

  AlphaNum(float f)
    : piece_data_(digits), piece_size_(strlen(FloatToBuffer(f, digits))) {}
  AlphaNum(double f)
    : piece_data_(digits), piece_size_(strlen(DoubleToBuffer(f, digits))) {}

  AlphaNum(Hex hex);

  AlphaNum(const char* c_str)
      : piece_data_(c_str), piece_size_(strlen(c_str)) {}
  // TODO: Add a string_ref constructor, eventually
  // AlphaNum(const StringPiece &pc) : piece(pc) {}

  AlphaNum(const string& str)
      : piece_data_(str.data()), piece_size_(str.size()) {}

  AlphaNum(StringPiece str)
      : piece_data_(str.data()), piece_size_(str.size()) {}

  AlphaNum(internal::StringPiecePod str)
      : piece_data_(str.data()), piece_size_(str.size()) {}

  size_t size() const { return piece_size_; }
  const char *data() const { return piece_data_; }

 private:
  // Use ":" not ':'
  AlphaNum(char c);  // NOLINT(runtime/explicit)

  // Disallow copy and assign.
  AlphaNum(const AlphaNum&);
  void operator=(const AlphaNum&);
};

}  // namespace strings

using strings::AlphaNum;

// ----------------------------------------------------------------------
// StrCat()
//    This merges the given strings or numbers, with no delimiter.  This
//    is designed to be the fastest possible way to construct a string out
//    of a mix of raw C strings, strings, bool values,
//    and numeric values.
//
//    Don't use this for user-visible strings.  The localization process
//    works poorly on strings built up out of fragments.
//
//    For clarity and performance, don't use StrCat when appending to a
//    string.  In particular, avoid using any of these (anti-)patterns:
//      str.append(StrCat(...)
//      str += StrCat(...)
//      str = StrCat(str, ...)
//    where the last is the worse, with the potential to change a loop
//    from a linear time operation with O(1) dynamic allocations into a
//    quadratic time operation with O(n) dynamic allocations.  StrAppend
//    is a better choice than any of the above, subject to the restriction
//    of StrAppend(&str, a, b, c, ...) that none of the a, b, c, ... may
//    be a reference into str.
// ----------------------------------------------------------------------

PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d,
                              const AlphaNum& e);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d,
                              const AlphaNum& e, const AlphaNum& f);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d,
                              const AlphaNum& e, const AlphaNum& f,
                              const AlphaNum& g);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d,
                              const AlphaNum& e, const AlphaNum& f,
                              const AlphaNum& g, const AlphaNum& h);
PROTOBUF_EXPORT string StrCat(const AlphaNum& a, const AlphaNum& b,
                              const AlphaNum& c, const AlphaNum& d,
                              const AlphaNum& e, const AlphaNum& f,
                              const AlphaNum& g, const AlphaNum& h,
                              const AlphaNum& i);

inline string StrCat(const AlphaNum& a) { return string(a.data(), a.size()); }

// ----------------------------------------------------------------------
// StrAppend()
//    Same as above, but adds the output to the given string.
//    WARNING: For speed, StrAppend does not try to check each of its input
//    arguments to be sure that they are not a subset of the string being
//    appended to.  That is, while this will work:
//
//    string s = "foo";
//    s += s;
//
//    This will not (necessarily) work:
//
//    string s = "foo";
//    StrAppend(&s, s);
//
//    Note: while StrCat supports appending up to 9 arguments, StrAppend
//    is currently limited to 4.  That's rarely an issue except when
//    automatically transforming StrCat to StrAppend, and can easily be
//    worked around as consecutive calls to StrAppend are quite efficient.
// ----------------------------------------------------------------------

PROTOBUF_EXPORT void StrAppend(string* dest, const AlphaNum& a);
PROTOBUF_EXPORT void StrAppend(string* dest, const AlphaNum& a,
                               const AlphaNum& b);
PROTOBUF_EXPORT void StrAppend(string* dest, const AlphaNum& a,
                               const AlphaNum& b, const AlphaNum& c);
PROTOBUF_EXPORT void StrAppend(string* dest, const AlphaNum& a,
                               const AlphaNum& b, const AlphaNum& c,
                               const AlphaNum& d);

// ----------------------------------------------------------------------
// Join()
//    These methods concatenate a range of components into a C++ string, using
//    the C-string "delim" as a separator between components.
// ----------------------------------------------------------------------
template <typename Iterator>
void Join(Iterator start, Iterator end,
          const char* delim, string* result) {
  for (Iterator it = start; it != end; ++it) {
    if (it != start) {
      result->append(delim);
    }
    StrAppend(result, *it);
  }
}

template <typename Range>
string Join(const Range& components,
            const char* delim) {
  string result;
  Join(components.begin(), components.end(), delim, &result);
  return result;
}

// ----------------------------------------------------------------------
// ToHex()
//    Return a lower-case hex string representation of the given integer.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT string ToHex(uint64 num);

// ----------------------------------------------------------------------
// GlobalReplaceSubstring()
//    Replaces all instances of a substring in a string.  Does nothing
//    if 'substring' is empty.  Returns the number of replacements.
//
//    NOTE: The string pieces must not overlap s.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int GlobalReplaceSubstring(const string& substring,
                                           const string& replacement,
                                           string* s);

// ----------------------------------------------------------------------
// Base64Unescape()
//    Converts "src" which is encoded in Base64 to its binary equivalent and
//    writes it to "dest". If src contains invalid characters, dest is cleared
//    and the function returns false. Returns true on success.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT bool Base64Unescape(StringPiece src, string* dest);

// ----------------------------------------------------------------------
// WebSafeBase64Unescape()
//    This is a variation of Base64Unescape which uses '-' instead of '+', and
//    '_' instead of '/'. src is not null terminated, instead specify len. I
//    recommend that slen<szdest, but we honor szdest anyway.
//    RETURNS the length of dest, or -1 if src contains invalid chars.

//    The variation that stores into a string clears the string first, and
//    returns false (with dest empty) if src contains invalid chars; for
//    this version src and dest must be different strings.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int WebSafeBase64Unescape(const char* src, int slen, char* dest,
                                          int szdest);
PROTOBUF_EXPORT bool WebSafeBase64Unescape(StringPiece src, string* dest);

// Return the length to use for the output buffer given to the base64 escape
// routines. Make sure to use the same value for do_padding in both.
// This function may return incorrect results if given input_len values that
// are extremely high, which should happen rarely.
PROTOBUF_EXPORT int CalculateBase64EscapedLen(int input_len, bool do_padding);
// Use this version when calling Base64Escape without a do_padding arg.
PROTOBUF_EXPORT int CalculateBase64EscapedLen(int input_len);

// ----------------------------------------------------------------------
// Base64Escape()
// WebSafeBase64Escape()
//    Encode "src" to "dest" using base64 encoding.
//    src is not null terminated, instead specify len.
//    'dest' should have at least CalculateBase64EscapedLen() length.
//    RETURNS the length of dest.
//    The WebSafe variation use '-' instead of '+' and '_' instead of '/'
//    so that we can place the out in the URL or cookies without having
//    to escape them.  It also has an extra parameter "do_padding",
//    which when set to false will prevent padding with "=".
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int Base64Escape(const unsigned char* src, int slen, char* dest,
                                 int szdest);
PROTOBUF_EXPORT int WebSafeBase64Escape(const unsigned char* src, int slen,
                                        char* dest, int szdest,
                                        bool do_padding);
// Encode src into dest with padding.
PROTOBUF_EXPORT void Base64Escape(StringPiece src, string* dest);
// Encode src into dest web-safely without padding.
PROTOBUF_EXPORT void WebSafeBase64Escape(StringPiece src, string* dest);
// Encode src into dest web-safely with padding.
PROTOBUF_EXPORT void WebSafeBase64EscapeWithPadding(StringPiece src,
                                                    string* dest);

PROTOBUF_EXPORT void Base64Escape(const unsigned char* src, int szsrc,
                                  string* dest, bool do_padding);
PROTOBUF_EXPORT void WebSafeBase64Escape(const unsigned char* src, int szsrc,
                                         string* dest, bool do_padding);

inline bool IsValidCodePoint(uint32 code_point) {
  return code_point < 0xD800 ||
         (code_point >= 0xE000 && code_point <= 0x10FFFF);
}

static const int UTFmax = 4;
// ----------------------------------------------------------------------
// EncodeAsUTF8Char()
//  Helper to append a Unicode code point to a string as UTF8, without bringing
//  in any external dependencies. The output buffer must be as least 4 bytes
//  large.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int EncodeAsUTF8Char(uint32 code_point, char* output);

// ----------------------------------------------------------------------
// UTF8FirstLetterNumBytes()
//   Length of the first UTF-8 character.
// ----------------------------------------------------------------------
PROTOBUF_EXPORT int UTF8FirstLetterNumBytes(const char* src, int len);

// From google3/third_party/absl/strings/escaping.h

// ----------------------------------------------------------------------
// CleanStringLineEndings()
//   Clean up a multi-line string to conform to Unix line endings.
//   Reads from src and appends to dst, so usually dst should be empty.
//
//   If there is no line ending at the end of a non-empty string, it can
//   be added automatically.
//
//   Four different types of input are correctly handled:
//
//     - Unix/Linux files: line ending is LF: pass through unchanged
//
//     - DOS/Windows files: line ending is CRLF: convert to LF
//
//     - Legacy Mac files: line ending is CR: convert to LF
//
//     - Garbled files: random line endings: convert gracefully
//                      lonely CR, lonely LF, CRLF: convert to LF
//
//   @param src The multi-line string to convert
//   @param dst The converted string is appended to this string
//   @param auto_end_last_line Automatically terminate the last line
//
//   Limitations:
//
//     This does not do the right thing for CRCRLF files created by
//     broken programs that do another Unix->DOS conversion on files
//     that are already in CRLF format.  For this, a two-pass approach
//     brute-force would be needed that
//
//       (1) determines the presence of LF (first one is ok)
//       (2) if yes, removes any CR, else convert every CR to LF
PROTOBUF_EXPORT void CleanStringLineEndings(const string& src, string* dst,
                                            bool auto_end_last_line);

// Same as above, but transforms the argument in place.
PROTOBUF_EXPORT void CleanStringLineEndings(string* str,
                                            bool auto_end_last_line);

namespace strings {
inline bool EndsWith(StringPiece text, StringPiece suffix) {
  return suffix.empty() ||
      (text.size() >= suffix.size() &&
       memcmp(text.data() + (text.size() - suffix.size()), suffix.data(),
              suffix.size()) == 0);
}
}  // namespace strings

}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_STUBS_STRUTIL_H__
