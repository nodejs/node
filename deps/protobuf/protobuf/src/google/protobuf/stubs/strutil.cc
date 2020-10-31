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

// from google3/strings/strutil.cc

#include <google/protobuf/stubs/strutil.h>

#include <errno.h>
#include <float.h>    // FLT_DIG and DBL_DIG
#include <limits.h>
#include <stdio.h>
#include <cmath>
#include <iterator>
#include <limits>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/io/strtod.h>

#ifdef _WIN32
// MSVC has only _snprintf, not snprintf.
//
// MinGW has both snprintf and _snprintf, but they appear to be different
// functions.  The former is buggy.  When invoked like so:
//   char buffer[32];
//   snprintf(buffer, 32, "%.*g\n", FLT_DIG, 1.23e10f);
// it prints "1.23000e+10".  This is plainly wrong:  %g should never print
// trailing zeros after the decimal point.  For some reason this bug only
// occurs with some input values, not all.  In any case, _snprintf does the
// right thing, so we use it.
#define snprintf _snprintf
#endif

namespace google {
namespace protobuf {

// These are defined as macros on some platforms.  #undef them so that we can
// redefine them.
#undef isxdigit
#undef isprint

// The definitions of these in ctype.h change based on locale.  Since our
// string manipulation is all in relation to the protocol buffer and C++
// languages, we always want to use the C locale.  So, we re-define these
// exactly as we want them.
inline bool isxdigit(char c) {
  return ('0' <= c && c <= '9') ||
         ('a' <= c && c <= 'f') ||
         ('A' <= c && c <= 'F');
}

inline bool isprint(char c) {
  return c >= 0x20 && c <= 0x7E;
}

// ----------------------------------------------------------------------
// StripString
//    Replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
// ----------------------------------------------------------------------
void StripString(string* s, const char* remove, char replacewith) {
  const char * str_start = s->c_str();
  const char * str = str_start;
  for (str = strpbrk(str, remove);
       str != nullptr;
       str = strpbrk(str + 1, remove)) {
    (*s)[str - str_start] = replacewith;
  }
}

// ----------------------------------------------------------------------
// ReplaceCharacters
//    Replaces any occurrence of the character 'remove' (or the characters
//    in 'remove') with the character 'replacewith'.
// ----------------------------------------------------------------------
void ReplaceCharacters(string *s, const char *remove, char replacewith) {
  const char *str_start = s->c_str();
  const char *str = str_start;
  for (str = strpbrk(str, remove);
       str != nullptr;
       str = strpbrk(str + 1, remove)) {
    (*s)[str - str_start] = replacewith;
  }
}

void StripWhitespace(string* str) {
  int str_length = str->length();

  // Strip off leading whitespace.
  int first = 0;
  while (first < str_length && ascii_isspace(str->at(first))) {
    ++first;
  }
  // If entire string is white space.
  if (first == str_length) {
    str->clear();
    return;
  }
  if (first > 0) {
    str->erase(0, first);
    str_length -= first;
  }

  // Strip off trailing whitespace.
  int last = str_length - 1;
  while (last >= 0 && ascii_isspace(str->at(last))) {
    --last;
  }
  if (last != (str_length - 1) && last >= 0) {
    str->erase(last + 1, string::npos);
  }
}

// ----------------------------------------------------------------------
// StringReplace()
//    Replace the "old" pattern with the "new" pattern in a string,
//    and append the result to "res".  If replace_all is false,
//    it only replaces the first instance of "old."
// ----------------------------------------------------------------------

void StringReplace(const string& s, const string& oldsub,
                   const string& newsub, bool replace_all,
                   string* res) {
  if (oldsub.empty()) {
    res->append(s);  // if empty, append the given string.
    return;
  }

  string::size_type start_pos = 0;
  string::size_type pos;
  do {
    pos = s.find(oldsub, start_pos);
    if (pos == string::npos) {
      break;
    }
    res->append(s, start_pos, pos - start_pos);
    res->append(newsub);
    start_pos = pos + oldsub.size();  // start searching again after the "old"
  } while (replace_all);
  res->append(s, start_pos, s.length() - start_pos);
}

// ----------------------------------------------------------------------
// StringReplace()
//    Give me a string and two patterns "old" and "new", and I replace
//    the first instance of "old" in the string with "new", if it
//    exists.  If "global" is true; call this repeatedly until it
//    fails.  RETURN a new string, regardless of whether the replacement
//    happened or not.
// ----------------------------------------------------------------------

string StringReplace(const string& s, const string& oldsub,
                     const string& newsub, bool replace_all) {
  string ret;
  StringReplace(s, oldsub, newsub, replace_all, &ret);
  return ret;
}

// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.
//
// Note: For multi-character delimiters, this routine will split on *ANY* of
// the characters in the string, not the entire string as a single delimiter.
// ----------------------------------------------------------------------
template <typename ITR>
static inline
void SplitStringToIteratorUsing(const string& full,
                                const char* delim,
                                ITR& result) {
  // Optimize the common case where delim is a single character.
  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    const char* p = full.data();
    const char* end = p + full.size();
    while (p != end) {
      if (*p == c) {
        ++p;
      } else {
        const char* start = p;
        while (++p != end && *p != c);
        *result++ = string(start, p - start);
      }
    }
    return;
  }

  string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

void SplitStringUsing(const string& full,
                      const char* delim,
                      std::vector<string>* result) {
  std::back_insert_iterator< std::vector<string> > it(*result);
  SplitStringToIteratorUsing(full, delim, it);
}

// Split a string using a character delimiter. Append the components
// to 'result'.  If there are consecutive delimiters, this function
// will return corresponding empty strings. The string is split into
// at most the specified number of pieces greedily. This means that the
// last piece may possibly be split further. To split into as many pieces
// as possible, specify 0 as the number of pieces.
//
// If "full" is the empty string, yields an empty string as the only value.
//
// If "pieces" is negative for some reason, it returns the whole string
// ----------------------------------------------------------------------
template <typename StringType, typename ITR>
static inline
void SplitStringToIteratorAllowEmpty(const StringType& full,
                                     const char* delim,
                                     int pieces,
                                     ITR& result) {
  string::size_type begin_index, end_index;
  begin_index = 0;

  for (int i = 0; (i < pieces-1) || (pieces == 0); i++) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = end_index + 1;
  }
  *result++ = full.substr(begin_index);
}

void SplitStringAllowEmpty(const string& full, const char* delim,
                           std::vector<string>* result) {
  std::back_insert_iterator<std::vector<string> > it(*result);
  SplitStringToIteratorAllowEmpty(full, delim, 0, it);
}

// ----------------------------------------------------------------------
// JoinStrings()
//    This merges a vector of string components with delim inserted
//    as separaters between components.
//
// ----------------------------------------------------------------------
template <class ITERATOR>
static void JoinStringsIterator(const ITERATOR& start,
                                const ITERATOR& end,
                                const char* delim,
                                string* result) {
  GOOGLE_CHECK(result != nullptr);
  result->clear();
  int delim_length = strlen(delim);

  // Precompute resulting length so we can reserve() memory in one shot.
  int length = 0;
  for (ITERATOR iter = start; iter != end; ++iter) {
    if (iter != start) {
      length += delim_length;
    }
    length += iter->size();
  }
  result->reserve(length);

  // Now combine everything.
  for (ITERATOR iter = start; iter != end; ++iter) {
    if (iter != start) {
      result->append(delim, delim_length);
    }
    result->append(iter->data(), iter->size());
  }
}

void JoinStrings(const std::vector<string>& components,
                 const char* delim,
                 string * result) {
  JoinStringsIterator(components.begin(), components.end(), delim, result);
}

// ----------------------------------------------------------------------
// UnescapeCEscapeSequences()
//    This does all the unescaping that C does: \ooo, \r, \n, etc
//    Returns length of resulting string.
//    The implementation of \x parses any positive number of hex digits,
//    but it is an error if the value requires more than 8 bits, and the
//    result is truncated to 8 bits.
//
//    The second call stores its errors in a supplied string vector.
//    If the string vector pointer is nullptr, it reports the errors with LOG().
// ----------------------------------------------------------------------

#define IS_OCTAL_DIGIT(c) (((c) >= '0') && ((c) <= '7'))

// Protocol buffers doesn't ever care about errors, but I don't want to remove
// the code.
#define LOG_STRING(LEVEL, VECTOR) GOOGLE_LOG_IF(LEVEL, false)

int UnescapeCEscapeSequences(const char* source, char* dest) {
  return UnescapeCEscapeSequences(source, dest, nullptr);
}

int UnescapeCEscapeSequences(const char* source, char* dest,
                             std::vector<string> *errors) {
  GOOGLE_DCHECK(errors == nullptr) << "Error reporting not implemented.";

  char* d = dest;
  const char* p = source;

  // Small optimization for case where source = dest and there's no escaping
  while ( p == d && *p != '\0' && *p != '\\' )
    p++, d++;

  while (*p != '\0') {
    if (*p != '\\') {
      *d++ = *p++;
    } else {
      switch ( *++p ) {                    // skip past the '\\'
        case '\0':
          LOG_STRING(ERROR, errors) << "String cannot end with \\";
          *d = '\0';
          return d - dest;   // we're done with p
        case 'a':  *d++ = '\a';  break;
        case 'b':  *d++ = '\b';  break;
        case 'f':  *d++ = '\f';  break;
        case 'n':  *d++ = '\n';  break;
        case 'r':  *d++ = '\r';  break;
        case 't':  *d++ = '\t';  break;
        case 'v':  *d++ = '\v';  break;
        case '\\': *d++ = '\\';  break;
        case '?':  *d++ = '\?';  break;    // \?  Who knew?
        case '\'': *d++ = '\'';  break;
        case '"':  *d++ = '\"';  break;
        case '0': case '1': case '2': case '3':  // octal digit: 1 to 3 digits
        case '4': case '5': case '6': case '7': {
          char ch = *p - '0';
          if ( IS_OCTAL_DIGIT(p[1]) )
            ch = ch * 8 + *++p - '0';
          if ( IS_OCTAL_DIGIT(p[1]) )      // safe (and easy) to do this twice
            ch = ch * 8 + *++p - '0';      // now points at last digit
          *d++ = ch;
          break;
        }
        case 'x': case 'X': {
          if (!isxdigit(p[1])) {
            if (p[1] == '\0') {
              LOG_STRING(ERROR, errors) << "String cannot end with \\x";
            } else {
              LOG_STRING(ERROR, errors) <<
                "\\x cannot be followed by non-hex digit: \\" << *p << p[1];
            }
            break;
          }
          unsigned int ch = 0;
          const char *hex_start = p;
          while (isxdigit(p[1]))  // arbitrarily many hex digits
            ch = (ch << 4) + hex_digit_to_int(*++p);
          if (ch > 0xFF)
            LOG_STRING(ERROR, errors) << "Value of " <<
              "\\" << string(hex_start, p+1-hex_start) << " exceeds 8 bits";
          *d++ = ch;
          break;
        }
#if 0  // TODO(kenton):  Support \u and \U?  Requires runetochar().
        case 'u': {
          // \uhhhh => convert 4 hex digits to UTF-8
          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 4; ++i) {
            if (isxdigit(p[1])) {  // Look one char ahead.
              rune = (rune << 4) + hex_digit_to_int(*++p);  // Advance p.
            } else {
              LOG_STRING(ERROR, errors)
                << "\\u must be followed by 4 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
        case 'U': {
          // \Uhhhhhhhh => convert 8 hex digits to UTF-8
          char32 rune = 0;
          const char *hex_start = p;
          for (int i = 0; i < 8; ++i) {
            if (isxdigit(p[1])) {  // Look one char ahead.
              // Don't change rune until we're sure this
              // is within the Unicode limit, but do advance p.
              char32 newrune = (rune << 4) + hex_digit_to_int(*++p);
              if (newrune > 0x10FFFF) {
                LOG_STRING(ERROR, errors)
                  << "Value of \\"
                  << string(hex_start, p + 1 - hex_start)
                  << " exceeds Unicode limit (0x10FFFF)";
                break;
              } else {
                rune = newrune;
              }
            } else {
              LOG_STRING(ERROR, errors)
                << "\\U must be followed by 8 hex digits: \\"
                <<  string(hex_start, p+1-hex_start);
              break;
            }
          }
          d += runetochar(d, &rune);
          break;
        }
#endif
        default:
          LOG_STRING(ERROR, errors) << "Unknown escape sequence: \\" << *p;
      }
      p++;                                 // read past letter we escaped
    }
  }
  *d = '\0';
  return d - dest;
}

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
int UnescapeCEscapeString(const string& src, string* dest) {
  return UnescapeCEscapeString(src, dest, nullptr);
}

int UnescapeCEscapeString(const string& src, string* dest,
                          std::vector<string> *errors) {
  std::unique_ptr<char[]> unescaped(new char[src.size() + 1]);
  int len = UnescapeCEscapeSequences(src.c_str(), unescaped.get(), errors);
  GOOGLE_CHECK(dest);
  dest->assign(unescaped.get(), len);
  return len;
}

string UnescapeCEscapeString(const string& src) {
  std::unique_ptr<char[]> unescaped(new char[src.size() + 1]);
  int len = UnescapeCEscapeSequences(src.c_str(), unescaped.get(), nullptr);
  return string(unescaped.get(), len);
}

// ----------------------------------------------------------------------
// CEscapeString()
// CHexEscapeString()
//    Copies 'src' to 'dest', escaping dangerous characters using
//    C-style escape sequences. This is very useful for preparing query
//    flags. 'src' and 'dest' should not overlap. The 'Hex' version uses
//    hexadecimal rather than octal sequences.
//    Returns the number of bytes written to 'dest' (not including the \0)
//    or -1 if there was insufficient space.
//
//    Currently only \n, \r, \t, ", ', \ and !isprint() chars are escaped.
// ----------------------------------------------------------------------
int CEscapeInternal(const char* src, int src_len, char* dest,
                    int dest_len, bool use_hex, bool utf8_safe) {
  const char* src_end = src + src_len;
  int used = 0;
  bool last_hex_escape = false; // true if last output char was \xNN

  for (; src < src_end; src++) {
    if (dest_len - used < 2)   // Need space for two letter escape
      return -1;

    bool is_hex_escape = false;
    switch (*src) {
      case '\n': dest[used++] = '\\'; dest[used++] = 'n';  break;
      case '\r': dest[used++] = '\\'; dest[used++] = 'r';  break;
      case '\t': dest[used++] = '\\'; dest[used++] = 't';  break;
      case '\"': dest[used++] = '\\'; dest[used++] = '\"'; break;
      case '\'': dest[used++] = '\\'; dest[used++] = '\''; break;
      case '\\': dest[used++] = '\\'; dest[used++] = '\\'; break;
      default:
        // Note that if we emit \xNN and the src character after that is a hex
        // digit then that digit must be escaped too to prevent it being
        // interpreted as part of the character code by C.
        if ((!utf8_safe || static_cast<uint8>(*src) < 0x80) &&
            (!isprint(*src) ||
             (last_hex_escape && isxdigit(*src)))) {
          if (dest_len - used < 4) // need space for 4 letter escape
            return -1;
          sprintf(dest + used, (use_hex ? "\\x%02x" : "\\%03o"),
                  static_cast<uint8>(*src));
          is_hex_escape = use_hex;
          used += 4;
        } else {
          dest[used++] = *src; break;
        }
    }
    last_hex_escape = is_hex_escape;
  }

  if (dest_len - used < 1)   // make sure that there is room for \0
    return -1;

  dest[used] = '\0';   // doesn't count towards return value though
  return used;
}

// Calculates the length of the C-style escaped version of 'src'.
// Assumes that non-printable characters are escaped using octal sequences, and
// that UTF-8 bytes are not handled specially.
static inline size_t CEscapedLength(StringPiece src) {
  static char c_escaped_len[256] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 2, 4, 4,  // \t, \n, \r
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,  // ", '
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // '0'..'9'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'A'..'O'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1,  // 'P'..'Z', '\'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 'a'..'o'
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4,  // 'p'..'z', DEL
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  };

  size_t escaped_len = 0;
  for (int i = 0; i < src.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    escaped_len += c_escaped_len[c];
  }
  return escaped_len;
}

// ----------------------------------------------------------------------
// Escapes 'src' using C-style escape sequences, and appends the escaped string
// to 'dest'. This version is faster than calling CEscapeInternal as it computes
// the required space using a lookup table, and also does not do any special
// handling for Hex or UTF-8 characters.
// ----------------------------------------------------------------------
void CEscapeAndAppend(StringPiece src, string* dest) {
  size_t escaped_len = CEscapedLength(src);
  if (escaped_len == src.size()) {
    dest->append(src.data(), src.size());
    return;
  }

  size_t cur_dest_len = dest->size();
  dest->resize(cur_dest_len + escaped_len);
  char* append_ptr = &(*dest)[cur_dest_len];

  for (int i = 0; i < src.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    switch (c) {
      case '\n': *append_ptr++ = '\\'; *append_ptr++ = 'n'; break;
      case '\r': *append_ptr++ = '\\'; *append_ptr++ = 'r'; break;
      case '\t': *append_ptr++ = '\\'; *append_ptr++ = 't'; break;
      case '\"': *append_ptr++ = '\\'; *append_ptr++ = '\"'; break;
      case '\'': *append_ptr++ = '\\'; *append_ptr++ = '\''; break;
      case '\\': *append_ptr++ = '\\'; *append_ptr++ = '\\'; break;
      default:
        if (!isprint(c)) {
          *append_ptr++ = '\\';
          *append_ptr++ = '0' + c / 64;
          *append_ptr++ = '0' + (c % 64) / 8;
          *append_ptr++ = '0' + c % 8;
        } else {
          *append_ptr++ = c;
        }
        break;
    }
  }
}

string CEscape(const string& src) {
  string dest;
  CEscapeAndAppend(src, &dest);
  return dest;
}

namespace strings {

string Utf8SafeCEscape(const string& src) {
  const int dest_length = src.size() * 4 + 1; // Maximum possible expansion
  std::unique_ptr<char[]> dest(new char[dest_length]);
  const int len = CEscapeInternal(src.data(), src.size(),
                                  dest.get(), dest_length, false, true);
  GOOGLE_DCHECK_GE(len, 0);
  return string(dest.get(), len);
}

string CHexEscape(const string& src) {
  const int dest_length = src.size() * 4 + 1; // Maximum possible expansion
  std::unique_ptr<char[]> dest(new char[dest_length]);
  const int len = CEscapeInternal(src.data(), src.size(),
                                  dest.get(), dest_length, true, false);
  GOOGLE_DCHECK_GE(len, 0);
  return string(dest.get(), len);
}

}  // namespace strings

// ----------------------------------------------------------------------
// strto32_adaptor()
// strtou32_adaptor()
//    Implementation of strto[u]l replacements that have identical
//    overflow and underflow characteristics for both ILP-32 and LP-64
//    platforms, including errno preservation in error-free calls.
// ----------------------------------------------------------------------

int32 strto32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const long result = strtol(nptr, endptr, base);
  if (errno == ERANGE && result == LONG_MIN) {
    return kint32min;
  } else if (errno == ERANGE && result == LONG_MAX) {
    return kint32max;
  } else if (errno == 0 && result < kint32min) {
    errno = ERANGE;
    return kint32min;
  } else if (errno == 0 && result > kint32max) {
    errno = ERANGE;
    return kint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<int32>(result);
}

uint32 strtou32_adaptor(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const unsigned long result = strtoul(nptr, endptr, base);
  if (errno == ERANGE && result == ULONG_MAX) {
    return kuint32max;
  } else if (errno == 0 && result > kuint32max) {
    errno = ERANGE;
    return kuint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<uint32>(result);
}

inline bool safe_parse_sign(string* text  /*inout*/,
                            bool* negative_ptr  /*output*/) {
  const char* start = text->data();
  const char* end = start + text->size();

  // Consume whitespace.
  while (start < end && (start[0] == ' ')) {
    ++start;
  }
  while (start < end && (end[-1] == ' ')) {
    --end;
  }
  if (start >= end) {
    return false;
  }

  // Consume sign.
  *negative_ptr = (start[0] == '-');
  if (*negative_ptr || start[0] == '+') {
    ++start;
    if (start >= end) {
      return false;
    }
  }
  *text = text->substr(start - text->data(), end - start);
  return true;
}

template<typename IntType>
bool safe_parse_positive_int(
    string text, IntType* value_p) {
  int base = 10;
  IntType value = 0;
  const IntType vmax = std::numeric_limits<IntType>::max();
  assert(vmax > 0);
  assert(vmax >= base);
  const IntType vmax_over_base = vmax / base;
  const char* start = text.data();
  const char* end = start + text.size();
  // loop over digits
  for (; start < end; ++start) {
    unsigned char c = static_cast<unsigned char>(start[0]);
    int digit = c - '0';
    if (digit >= base || digit < 0) {
      *value_p = value;
      return false;
    }
    if (value > vmax_over_base) {
      *value_p = vmax;
      return false;
    }
    value *= base;
    if (value > vmax - digit) {
      *value_p = vmax;
      return false;
    }
    value += digit;
  }
  *value_p = value;
  return true;
}

template<typename IntType>
bool safe_parse_negative_int(
    const string& text, IntType* value_p) {
  int base = 10;
  IntType value = 0;
  const IntType vmin = std::numeric_limits<IntType>::min();
  assert(vmin < 0);
  assert(vmin <= 0 - base);
  IntType vmin_over_base = vmin / base;
  // 2003 c++ standard [expr.mul]
  // "... the sign of the remainder is implementation-defined."
  // Although (vmin/base)*base + vmin%base is always vmin.
  // 2011 c++ standard tightens the spec but we cannot rely on it.
  if (vmin % base > 0) {
    vmin_over_base += 1;
  }
  const char* start = text.data();
  const char* end = start + text.size();
  // loop over digits
  for (; start < end; ++start) {
    unsigned char c = static_cast<unsigned char>(start[0]);
    int digit = c - '0';
    if (digit >= base || digit < 0) {
      *value_p = value;
      return false;
    }
    if (value < vmin_over_base) {
      *value_p = vmin;
      return false;
    }
    value *= base;
    if (value < vmin + digit) {
      *value_p = vmin;
      return false;
    }
    value -= digit;
  }
  *value_p = value;
  return true;
}

template<typename IntType>
bool safe_int_internal(string text, IntType* value_p) {
  *value_p = 0;
  bool negative;
  if (!safe_parse_sign(&text, &negative)) {
    return false;
  }
  if (!negative) {
    return safe_parse_positive_int(text, value_p);
  } else {
    return safe_parse_negative_int(text, value_p);
  }
}

template<typename IntType>
bool safe_uint_internal(string text, IntType* value_p) {
  *value_p = 0;
  bool negative;
  if (!safe_parse_sign(&text, &negative) || negative) {
    return false;
  }
  return safe_parse_positive_int(text, value_p);
}

// ----------------------------------------------------------------------
// FastIntToBuffer()
// FastInt64ToBuffer()
// FastHexToBuffer()
// FastHex64ToBuffer()
// FastHex32ToBuffer()
// ----------------------------------------------------------------------

// Offset into buffer where FastInt64ToBuffer places the end of string
// null character.  Also used by FastInt64ToBufferLeft.
static const int kFastInt64ToBufferOffset = 21;

char *FastInt64ToBuffer(int64 i, char* buffer) {
  // We could collapse the positive and negative sections, but that
  // would be slightly slower for positive numbers...
  // 22 bytes is enough to store -2**64, -18446744073709551616.
  char* p = buffer + kFastInt64ToBufferOffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {
    // On different platforms, % and / have different behaviors for
    // negative numbers, so we need to jump through hoops to make sure
    // we don't divide negative numbers.
    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {
      // Make sure we aren't at MIN_INT, in which case we can't say i = -i
      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;
      // Undo what we did a moment ago
      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

// Offset into buffer where FastInt32ToBuffer places the end of string
// null character.  Also used by FastInt32ToBufferLeft
static const int kFastInt32ToBufferOffset = 11;

// Yes, this is a duplicate of FastInt64ToBuffer.  But, we need this for the
// compiler to generate 32 bit arithmetic instructions.  It's much faster, at
// least with 32 bit binaries.
char *FastInt32ToBuffer(int32 i, char* buffer) {
  // We could collapse the positive and negative sections, but that
  // would be slightly slower for positive numbers...
  // 12 bytes is enough to store -2**32, -4294967296.
  char* p = buffer + kFastInt32ToBufferOffset;
  *p-- = '\0';
  if (i >= 0) {
    do {
      *p-- = '0' + i % 10;
      i /= 10;
    } while (i > 0);
    return p + 1;
  } else {
    // On different platforms, % and / have different behaviors for
    // negative numbers, so we need to jump through hoops to make sure
    // we don't divide negative numbers.
    if (i > -10) {
      i = -i;
      *p-- = '0' + i;
      *p = '-';
      return p;
    } else {
      // Make sure we aren't at MIN_INT, in which case we can't say i = -i
      i = i + 10;
      i = -i;
      *p-- = '0' + i % 10;
      // Undo what we did a moment ago
      i = i / 10 + 1;
      do {
        *p-- = '0' + i % 10;
        i /= 10;
      } while (i > 0);
      *p = '-';
      return p;
    }
  }
}

char *FastHexToBuffer(int i, char* buffer) {
  GOOGLE_CHECK(i >= 0) << "FastHexToBuffer() wants non-negative integers, not " << i;

  static const char *hexdigits = "0123456789abcdef";
  char *p = buffer + 21;
  *p-- = '\0';
  do {
    *p-- = hexdigits[i & 15];   // mod by 16
    i >>= 4;                    // divide by 16
  } while (i > 0);
  return p + 1;
}

char *InternalFastHexToBuffer(uint64 value, char* buffer, int num_byte) {
  static const char *hexdigits = "0123456789abcdef";
  buffer[num_byte] = '\0';
  for (int i = num_byte - 1; i >= 0; i--) {
#ifdef _M_X64
    // MSVC x64 platform has a bug optimizing the uint32(value) in the #else
    // block. Given that the uint32 cast was to improve performance on 32-bit
    // platforms, we use 64-bit '&' directly.
    buffer[i] = hexdigits[value & 0xf];
#else
    buffer[i] = hexdigits[uint32(value) & 0xf];
#endif
    value >>= 4;
  }
  return buffer;
}

char *FastHex64ToBuffer(uint64 value, char* buffer) {
  return InternalFastHexToBuffer(value, buffer, 16);
}

char *FastHex32ToBuffer(uint32 value, char* buffer) {
  return InternalFastHexToBuffer(value, buffer, 8);
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

static const char two_ASCII_digits[100][2] = {
  {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'},
  {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
  {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'},
  {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
  {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'},
  {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
  {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'},
  {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
  {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'},
  {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
  {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'},
  {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
  {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'},
  {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
  {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'},
  {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
  {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'},
  {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
  {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'},
  {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
};

char* FastUInt32ToBufferLeft(uint32 u, char* buffer) {
  uint32 digits;
  const char *ASCII_digits = nullptr;
  // The idea of this implementation is to trim the number of divides to as few
  // as possible by using multiplication and subtraction rather than mod (%),
  // and by outputting two digits at a time rather than one.
  // The huge-number case is first, in the hopes that the compiler will output
  // that case in one branch-free block of code, and only output conditional
  // branches into it from below.
  if (u >= 1000000000) {  // >= 1,000,000,000
    digits = u / 100000000;  // 100,000,000
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt100_000_000:
    u -= digits * 100000000;  // 100,000,000
lt100_000_000:
    digits = u / 1000000;  // 1,000,000
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt1_000_000:
    u -= digits * 1000000;  // 1,000,000
lt1_000_000:
    digits = u / 10000;  // 10,000
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt10_000:
    u -= digits * 10000;  // 10,000
lt10_000:
    digits = u / 100;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
sublt100:
    u -= digits * 100;
lt100:
    digits = u;
    ASCII_digits = two_ASCII_digits[digits];
    buffer[0] = ASCII_digits[0];
    buffer[1] = ASCII_digits[1];
    buffer += 2;
done:
    *buffer = 0;
    return buffer;
  }

  if (u < 100) {
    digits = u;
    if (u >= 10) goto lt100;
    *buffer++ = '0' + digits;
    goto done;
  }
  if (u  <  10000) {   // 10,000
    if (u >= 1000) goto lt10_000;
    digits = u / 100;
    *buffer++ = '0' + digits;
    goto sublt100;
  }
  if (u  <  1000000) {   // 1,000,000
    if (u >= 100000) goto lt1_000_000;
    digits = u / 10000;  //    10,000
    *buffer++ = '0' + digits;
    goto sublt10_000;
  }
  if (u  <  100000000) {   // 100,000,000
    if (u >= 10000000) goto lt100_000_000;
    digits = u / 1000000;  //   1,000,000
    *buffer++ = '0' + digits;
    goto sublt1_000_000;
  }
  // we already know that u < 1,000,000,000
  digits = u / 100000000;   // 100,000,000
  *buffer++ = '0' + digits;
  goto sublt100_000_000;
}

char* FastInt32ToBufferLeft(int32 i, char* buffer) {
  uint32 u = i;
  if (i < 0) {
    *buffer++ = '-';
    u = -i;
  }
  return FastUInt32ToBufferLeft(u, buffer);
}

char* FastUInt64ToBufferLeft(uint64 u64, char* buffer) {
  int digits;
  const char *ASCII_digits = nullptr;

  uint32 u = static_cast<uint32>(u64);
  if (u == u64) return FastUInt32ToBufferLeft(u, buffer);

  uint64 top_11_digits = u64 / 1000000000;
  buffer = FastUInt64ToBufferLeft(top_11_digits, buffer);
  u = u64 - (top_11_digits * 1000000000);

  digits = u / 10000000;  // 10,000,000
  GOOGLE_DCHECK_LT(digits, 100);
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 10000000;  // 10,000,000
  digits = u / 100000;  // 100,000
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 100000;  // 100,000
  digits = u / 1000;  // 1,000
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 1000;  // 1,000
  digits = u / 10;
  ASCII_digits = two_ASCII_digits[digits];
  buffer[0] = ASCII_digits[0];
  buffer[1] = ASCII_digits[1];
  buffer += 2;
  u -= digits * 10;
  digits = u;
  *buffer++ = '0' + digits;
  *buffer = 0;
  return buffer;
}

char* FastInt64ToBufferLeft(int64 i, char* buffer) {
  uint64 u = 0;
  if (i < 0) {
    *buffer++ = '-';
    u -= i;
  } else {
    u = i;
  }
  return FastUInt64ToBufferLeft(u, buffer);
}

// ----------------------------------------------------------------------
// SimpleItoa()
//    Description: converts an integer to a string.
//
//    Return value: string
// ----------------------------------------------------------------------

string SimpleItoa(int i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned int i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

string SimpleItoa(long i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned long i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

string SimpleItoa(long long i) {
  char buffer[kFastToBufferSize];
  return (sizeof(i) == 4) ?
    FastInt32ToBuffer(i, buffer) :
    FastInt64ToBuffer(i, buffer);
}

string SimpleItoa(unsigned long long i) {
  char buffer[kFastToBufferSize];
  return string(buffer, (sizeof(i) == 4) ?
    FastUInt32ToBufferLeft(i, buffer) :
    FastUInt64ToBufferLeft(i, buffer));
}

// ----------------------------------------------------------------------
// SimpleDtoa()
// SimpleFtoa()
// DoubleToBuffer()
// FloatToBuffer()
//    We want to print the value without losing precision, but we also do
//    not want to print more digits than necessary.  This turns out to be
//    trickier than it sounds.  Numbers like 0.2 cannot be represented
//    exactly in binary.  If we print 0.2 with a very large precision,
//    e.g. "%.50g", we get "0.2000000000000000111022302462515654042363167".
//    On the other hand, if we set the precision too low, we lose
//    significant digits when printing numbers that actually need them.
//    It turns out there is no precision value that does the right thing
//    for all numbers.
//
//    Our strategy is to first try printing with a precision that is never
//    over-precise, then parse the result with strtod() to see if it
//    matches.  If not, we print again with a precision that will always
//    give a precise result, but may use more digits than necessary.
//
//    An arguably better strategy would be to use the algorithm described
//    in "How to Print Floating-Point Numbers Accurately" by Steele &
//    White, e.g. as implemented by David M. Gay's dtoa().  It turns out,
//    however, that the following implementation is about as fast as
//    DMG's code.  Furthermore, DMG's code locks mutexes, which means it
//    will not scale well on multi-core machines.  DMG's code is slightly
//    more accurate (in that it will never use more digits than
//    necessary), but this is probably irrelevant for most users.
//
//    Rob Pike and Ken Thompson also have an implementation of dtoa() in
//    third_party/fmt/fltfmt.cc.  Their implementation is similar to this
//    one in that it makes guesses and then uses strtod() to check them.
//    Their implementation is faster because they use their own code to
//    generate the digits in the first place rather than use snprintf(),
//    thus avoiding format string parsing overhead.  However, this makes
//    it considerably more complicated than the following implementation,
//    and it is embedded in a larger library.  If speed turns out to be
//    an issue, we could re-implement this in terms of their
//    implementation.
// ----------------------------------------------------------------------

string SimpleDtoa(double value) {
  char buffer[kDoubleToBufferSize];
  return DoubleToBuffer(value, buffer);
}

string SimpleFtoa(float value) {
  char buffer[kFloatToBufferSize];
  return FloatToBuffer(value, buffer);
}

static inline bool IsValidFloatChar(char c) {
  return ('0' <= c && c <= '9') ||
         c == 'e' || c == 'E' ||
         c == '+' || c == '-';
}

void DelocalizeRadix(char* buffer) {
  // Fast check:  if the buffer has a normal decimal point, assume no
  // translation is needed.
  if (strchr(buffer, '.') != nullptr) return;

  // Find the first unknown character.
  while (IsValidFloatChar(*buffer)) ++buffer;

  if (*buffer == '\0') {
    // No radix character found.
    return;
  }

  // We are now pointing at the locale-specific radix character.  Replace it
  // with '.'.
  *buffer = '.';
  ++buffer;

  if (!IsValidFloatChar(*buffer) && *buffer != '\0') {
    // It appears the radix was a multi-byte character.  We need to remove the
    // extra bytes.
    char* target = buffer;
    do { ++buffer; } while (!IsValidFloatChar(*buffer) && *buffer != '\0');
    memmove(target, buffer, strlen(buffer) + 1);
  }
}

char* DoubleToBuffer(double value, char* buffer) {
  // DBL_DIG is 15 for IEEE-754 doubles, which are used on almost all
  // platforms these days.  Just in case some system exists where DBL_DIG
  // is significantly larger -- and risks overflowing our buffer -- we have
  // this assert.
  GOOGLE_COMPILE_ASSERT(DBL_DIG < 20, DBL_DIG_is_too_big);

  if (value == std::numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -std::numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (std::isnan(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kDoubleToBufferSize, "%.*g", DBL_DIG, value);

  // The snprintf should never overflow because the buffer is significantly
  // larger than the precision we asked for.
  GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kDoubleToBufferSize);

  // We need to make parsed_value volatile in order to force the compiler to
  // write it out to the stack.  Otherwise, it may keep the value in a
  // register, and if it does that, it may keep it as a long double instead
  // of a double.  This long double may have extra bits that make it compare
  // unequal to "value" even though it would be exactly equal if it were
  // truncated to a double.
  volatile double parsed_value = io::NoLocaleStrtod(buffer, nullptr);
  if (parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kDoubleToBufferSize, "%.*g", DBL_DIG+2, value);

    // Should never overflow; see above.
    GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kDoubleToBufferSize);
  }

  DelocalizeRadix(buffer);
  return buffer;
}

static int memcasecmp(const char *s1, const char *s2, size_t len) {
  const unsigned char *us1 = reinterpret_cast<const unsigned char *>(s1);
  const unsigned char *us2 = reinterpret_cast<const unsigned char *>(s2);

  for ( int i = 0; i < len; i++ ) {
    const int diff =
      static_cast<int>(static_cast<unsigned char>(ascii_tolower(us1[i]))) -
      static_cast<int>(static_cast<unsigned char>(ascii_tolower(us2[i])));
    if (diff != 0) return diff;
  }
  return 0;
}

inline bool CaseEqual(StringPiece s1, StringPiece s2) {
  if (s1.size() != s2.size()) return false;
  return memcasecmp(s1.data(), s2.data(), s1.size()) == 0;
}

bool safe_strtob(StringPiece str, bool* value) {
  GOOGLE_CHECK(value != nullptr) << "nullptr output boolean given.";
  if (CaseEqual(str, "true") || CaseEqual(str, "t") ||
      CaseEqual(str, "yes") || CaseEqual(str, "y") ||
      CaseEqual(str, "1")) {
    *value = true;
    return true;
  }
  if (CaseEqual(str, "false") || CaseEqual(str, "f") ||
      CaseEqual(str, "no") || CaseEqual(str, "n") ||
      CaseEqual(str, "0")) {
    *value = false;
    return true;
  }
  return false;
}

bool safe_strtof(const char* str, float* value) {
  char* endptr;
  errno = 0;  // errno only gets set on errors
#if defined(_WIN32) || defined (__hpux)  // has no strtof()
  *value = io::NoLocaleStrtod(str, &endptr);
#else
  *value = strtof(str, &endptr);
#endif
  return *str != 0 && *endptr == 0 && errno == 0;
}

bool safe_strtod(const char* str, double* value) {
  char* endptr;
  *value = io::NoLocaleStrtod(str, &endptr);
  if (endptr != str) {
    while (ascii_isspace(*endptr)) ++endptr;
  }
  // Ignore range errors from strtod.  The values it
  // returns on underflow and overflow are the right
  // fallback in a robust setting.
  return *str != '\0' && *endptr == '\0';
}

bool safe_strto32(const string& str, int32* value) {
  return safe_int_internal(str, value);
}

bool safe_strtou32(const string& str, uint32* value) {
  return safe_uint_internal(str, value);
}

bool safe_strto64(const string& str, int64* value) {
  return safe_int_internal(str, value);
}

bool safe_strtou64(const string& str, uint64* value) {
  return safe_uint_internal(str, value);
}

char* FloatToBuffer(float value, char* buffer) {
  // FLT_DIG is 6 for IEEE-754 floats, which are used on almost all
  // platforms these days.  Just in case some system exists where FLT_DIG
  // is significantly larger -- and risks overflowing our buffer -- we have
  // this assert.
  GOOGLE_COMPILE_ASSERT(FLT_DIG < 10, FLT_DIG_is_too_big);

  if (value == std::numeric_limits<double>::infinity()) {
    strcpy(buffer, "inf");
    return buffer;
  } else if (value == -std::numeric_limits<double>::infinity()) {
    strcpy(buffer, "-inf");
    return buffer;
  } else if (std::isnan(value)) {
    strcpy(buffer, "nan");
    return buffer;
  }

  int snprintf_result =
    snprintf(buffer, kFloatToBufferSize, "%.*g", FLT_DIG, value);

  // The snprintf should never overflow because the buffer is significantly
  // larger than the precision we asked for.
  GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kFloatToBufferSize);

  float parsed_value;
  if (!safe_strtof(buffer, &parsed_value) || parsed_value != value) {
    int snprintf_result =
      snprintf(buffer, kFloatToBufferSize, "%.*g", FLT_DIG+3, value);

    // Should never overflow; see above.
    GOOGLE_DCHECK(snprintf_result > 0 && snprintf_result < kFloatToBufferSize);
  }

  DelocalizeRadix(buffer);
  return buffer;
}

namespace strings {

AlphaNum::AlphaNum(strings::Hex hex) {
  char *const end = &digits[kFastToBufferSize];
  char *writer = end;
  uint64 value = hex.value;
  uint64 width = hex.spec;
  // We accomplish minimum width by OR'ing in 0x10000 to the user's value,
  // where 0x10000 is the smallest hex number that is as wide as the user
  // asked for.
  uint64 mask = ((static_cast<uint64>(1) << (width - 1) * 4)) | value;
  static const char hexdigits[] = "0123456789abcdef";
  do {
    *--writer = hexdigits[value & 0xF];
    value >>= 4;
    mask >>= 4;
  } while (mask != 0);
  piece_data_ = writer;
  piece_size_ = end - writer;
}

}  // namespace strings

// ----------------------------------------------------------------------
// StrCat()
//    This merges the given strings or integers, with no delimiter.  This
//    is designed to be the fastest possible way to construct a string out
//    of a mix of raw C strings, C++ strings, and integer values.
// ----------------------------------------------------------------------

// Append is merely a version of memcpy that returns the address of the byte
// after the area just overwritten.  It comes in multiple flavors to minimize
// call overhead.
static char *Append1(char *out, const AlphaNum &x) {
  memcpy(out, x.data(), x.size());
  return out + x.size();
}

static char *Append2(char *out, const AlphaNum &x1, const AlphaNum &x2) {
  memcpy(out, x1.data(), x1.size());
  out += x1.size();

  memcpy(out, x2.data(), x2.size());
  return out + x2.size();
}

static char *Append4(char *out,
                     const AlphaNum &x1, const AlphaNum &x2,
                     const AlphaNum &x3, const AlphaNum &x4) {
  memcpy(out, x1.data(), x1.size());
  out += x1.size();

  memcpy(out, x2.data(), x2.size());
  out += x2.size();

  memcpy(out, x3.data(), x3.size());
  out += x3.size();

  memcpy(out, x4.data(), x4.size());
  return out + x4.size();
}

string StrCat(const AlphaNum &a, const AlphaNum &b) {
  string result;
  result.resize(a.size() + b.size());
  char *const begin = &*result.begin();
  char *out = Append2(begin, a, b);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c) {
  string result;
  result.resize(a.size() + b.size() + c.size());
  char *const begin = &*result.begin();
  char *out = Append2(begin, a, b);
  out = Append1(out, c);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d, const AlphaNum &e) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size() + e.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  out = Append1(out, e);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d, const AlphaNum &e, const AlphaNum &f) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size() + e.size() +
                f.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  out = Append2(out, e, f);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d, const AlphaNum &e, const AlphaNum &f,
              const AlphaNum &g) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size() + e.size() +
                f.size() + g.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  out = Append2(out, e, f);
  out = Append1(out, g);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d, const AlphaNum &e, const AlphaNum &f,
              const AlphaNum &g, const AlphaNum &h) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size() + e.size() +
                f.size() + g.size() + h.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  out = Append4(out, e, f, g, h);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

string StrCat(const AlphaNum &a, const AlphaNum &b, const AlphaNum &c,
              const AlphaNum &d, const AlphaNum &e, const AlphaNum &f,
              const AlphaNum &g, const AlphaNum &h, const AlphaNum &i) {
  string result;
  result.resize(a.size() + b.size() + c.size() + d.size() + e.size() +
                f.size() + g.size() + h.size() + i.size());
  char *const begin = &*result.begin();
  char *out = Append4(begin, a, b, c, d);
  out = Append4(out, e, f, g, h);
  out = Append1(out, i);
  GOOGLE_DCHECK_EQ(out, begin + result.size());
  return result;
}

// It's possible to call StrAppend with a char * pointer that is partway into
// the string we're appending to.  However the results of this are random.
// Therefore, check for this in debug mode.  Use unsigned math so we only have
// to do one comparison.
#define GOOGLE_DCHECK_NO_OVERLAP(dest, src) \
    GOOGLE_DCHECK_GT(uintptr_t((src).data() - (dest).data()), \
                     uintptr_t((dest).size()))

void StrAppend(string *result, const AlphaNum &a) {
  GOOGLE_DCHECK_NO_OVERLAP(*result, a);
  result->append(a.data(), a.size());
}

void StrAppend(string *result, const AlphaNum &a, const AlphaNum &b) {
  GOOGLE_DCHECK_NO_OVERLAP(*result, a);
  GOOGLE_DCHECK_NO_OVERLAP(*result, b);
  string::size_type old_size = result->size();
  result->resize(old_size + a.size() + b.size());
  char *const begin = &*result->begin();
  char *out = Append2(begin + old_size, a, b);
  GOOGLE_DCHECK_EQ(out, begin + result->size());
}

void StrAppend(string *result,
               const AlphaNum &a, const AlphaNum &b, const AlphaNum &c) {
  GOOGLE_DCHECK_NO_OVERLAP(*result, a);
  GOOGLE_DCHECK_NO_OVERLAP(*result, b);
  GOOGLE_DCHECK_NO_OVERLAP(*result, c);
  string::size_type old_size = result->size();
  result->resize(old_size + a.size() + b.size() + c.size());
  char *const begin = &*result->begin();
  char *out = Append2(begin + old_size, a, b);
  out = Append1(out, c);
  GOOGLE_DCHECK_EQ(out, begin + result->size());
}

void StrAppend(string *result,
               const AlphaNum &a, const AlphaNum &b,
               const AlphaNum &c, const AlphaNum &d) {
  GOOGLE_DCHECK_NO_OVERLAP(*result, a);
  GOOGLE_DCHECK_NO_OVERLAP(*result, b);
  GOOGLE_DCHECK_NO_OVERLAP(*result, c);
  GOOGLE_DCHECK_NO_OVERLAP(*result, d);
  string::size_type old_size = result->size();
  result->resize(old_size + a.size() + b.size() + c.size() + d.size());
  char *const begin = &*result->begin();
  char *out = Append4(begin + old_size, a, b, c, d);
  GOOGLE_DCHECK_EQ(out, begin + result->size());
}

int GlobalReplaceSubstring(const string& substring,
                           const string& replacement,
                           string* s) {
  GOOGLE_CHECK(s != nullptr);
  if (s->empty() || substring.empty())
    return 0;
  string tmp;
  int num_replacements = 0;
  int pos = 0;
  for (int match_pos = s->find(substring.data(), pos, substring.length());
       match_pos != string::npos;
       pos = match_pos + substring.length(),
           match_pos = s->find(substring.data(), pos, substring.length())) {
    ++num_replacements;
    // Append the original content before the match.
    tmp.append(*s, pos, match_pos - pos);
    // Append the replacement for the match.
    tmp.append(replacement.begin(), replacement.end());
  }
  // Append the content after the last match. If no replacements were made, the
  // original string is left untouched.
  if (num_replacements > 0) {
    tmp.append(*s, pos, s->length() - pos);
    s->swap(tmp);
  }
  return num_replacements;
}

int CalculateBase64EscapedLen(int input_len, bool do_padding) {
  // Base64 encodes three bytes of input at a time. If the input is not
  // divisible by three, we pad as appropriate.
  //
  // (from http://tools.ietf.org/html/rfc3548)
  // Special processing is performed if fewer than 24 bits are available
  // at the end of the data being encoded.  A full encoding quantum is
  // always completed at the end of a quantity.  When fewer than 24 input
  // bits are available in an input group, zero bits are added (on the
  // right) to form an integral number of 6-bit groups.  Padding at the
  // end of the data is performed using the '=' character.  Since all base
  // 64 input is an integral number of octets, only the following cases
  // can arise:


  // Base64 encodes each three bytes of input into four bytes of output.
  int len = (input_len / 3) * 4;

  if (input_len % 3 == 0) {
    // (from http://tools.ietf.org/html/rfc3548)
    // (1) the final quantum of encoding input is an integral multiple of 24
    // bits; here, the final unit of encoded output will be an integral
    // multiple of 4 characters with no "=" padding,
  } else if (input_len % 3 == 1) {
    // (from http://tools.ietf.org/html/rfc3548)
    // (2) the final quantum of encoding input is exactly 8 bits; here, the
    // final unit of encoded output will be two characters followed by two
    // "=" padding characters, or
    len += 2;
    if (do_padding) {
      len += 2;
    }
  } else {  // (input_len % 3 == 2)
    // (from http://tools.ietf.org/html/rfc3548)
    // (3) the final quantum of encoding input is exactly 16 bits; here, the
    // final unit of encoded output will be three characters followed by one
    // "=" padding character.
    len += 3;
    if (do_padding) {
      len += 1;
    }
  }

  assert(len >= input_len);  // make sure we didn't overflow
  return len;
}

// Base64Escape does padding, so this calculation includes padding.
int CalculateBase64EscapedLen(int input_len) {
  return CalculateBase64EscapedLen(input_len, true);
}

// ----------------------------------------------------------------------
// int Base64Unescape() - base64 decoder
// int Base64Escape() - base64 encoder
// int WebSafeBase64Unescape() - Google's variation of base64 decoder
// int WebSafeBase64Escape() - Google's variation of base64 encoder
//
// Check out
// http://tools.ietf.org/html/rfc2045 for formal description, but what we
// care about is that...
//   Take the encoded stuff in groups of 4 characters and turn each
//   character into a code 0 to 63 thus:
//           A-Z map to 0 to 25
//           a-z map to 26 to 51
//           0-9 map to 52 to 61
//           +(- for WebSafe) maps to 62
//           /(_ for WebSafe) maps to 63
//   There will be four numbers, all less than 64 which can be represented
//   by a 6 digit binary number (aaaaaa, bbbbbb, cccccc, dddddd respectively).
//   Arrange the 6 digit binary numbers into three bytes as such:
//   aaaaaabb bbbbcccc ccdddddd
//   Equals signs (one or two) are used at the end of the encoded block to
//   indicate that the text was not an integer multiple of three bytes long.
// ----------------------------------------------------------------------

int Base64UnescapeInternal(const char *src_param, int szsrc,
                           char *dest, int szdest,
                           const signed char* unbase64) {
  static const char kPad64Equals = '=';
  static const char kPad64Dot = '.';

  int decode = 0;
  int destidx = 0;
  int state = 0;
  unsigned int ch = 0;
  unsigned int temp = 0;

  // If "char" is signed by default, using *src as an array index results in
  // accessing negative array elements. Treat the input as a pointer to
  // unsigned char to avoid this.
  const unsigned char *src = reinterpret_cast<const unsigned char*>(src_param);

  // The GET_INPUT macro gets the next input character, skipping
  // over any whitespace, and stopping when we reach the end of the
  // string or when we read any non-data character.  The arguments are
  // an arbitrary identifier (used as a label for goto) and the number
  // of data bytes that must remain in the input to avoid aborting the
  // loop.
#define GET_INPUT(label, remain)                 \
  label:                                         \
    --szsrc;                                     \
    ch = *src++;                                 \
    decode = unbase64[ch];                       \
    if (decode < 0) {                            \
      if (ascii_isspace(ch) && szsrc >= remain)  \
        goto label;                              \
      state = 4 - remain;                        \
      break;                                     \
    }

  // if dest is null, we're just checking to see if it's legal input
  // rather than producing output.  (I suspect this could just be done
  // with a regexp...).  We duplicate the loop so this test can be
  // outside it instead of in every iteration.

  if (dest) {
    // This loop consumes 4 input bytes and produces 3 output bytes
    // per iteration.  We can't know at the start that there is enough
    // data left in the string for a full iteration, so the loop may
    // break out in the middle; if so 'state' will be set to the
    // number of input bytes read.

    while (szsrc >= 4)  {
      // We'll start by optimistically assuming that the next four
      // bytes of the string (src[0..3]) are four good data bytes
      // (that is, no nulls, whitespace, padding chars, or illegal
      // chars).  We need to test src[0..2] for nulls individually
      // before constructing temp to preserve the property that we
      // never read past a null in the string (no matter how long
      // szsrc claims the string is).

      if (!src[0] || !src[1] || !src[2] ||
          (temp = ((unsigned(unbase64[src[0]]) << 18) |
                   (unsigned(unbase64[src[1]]) << 12) |
                   (unsigned(unbase64[src[2]]) << 6) |
                   (unsigned(unbase64[src[3]])))) & 0x80000000) {
        // Iff any of those four characters was bad (null, illegal,
        // whitespace, padding), then temp's high bit will be set
        // (because unbase64[] is -1 for all bad characters).
        //
        // We'll back up and resort to the slower decoder, which knows
        // how to handle those cases.

        GET_INPUT(first, 4);
        temp = decode;
        GET_INPUT(second, 3);
        temp = (temp << 6) | decode;
        GET_INPUT(third, 2);
        temp = (temp << 6) | decode;
        GET_INPUT(fourth, 1);
        temp = (temp << 6) | decode;
      } else {
        // We really did have four good data bytes, so advance four
        // characters in the string.

        szsrc -= 4;
        src += 4;
        decode = -1;
        ch = '\0';
      }

      // temp has 24 bits of input, so write that out as three bytes.

      if (destidx+3 > szdest) return -1;
      dest[destidx+2] = temp;
      temp >>= 8;
      dest[destidx+1] = temp;
      temp >>= 8;
      dest[destidx] = temp;
      destidx += 3;
    }
  } else {
    while (szsrc >= 4)  {
      if (!src[0] || !src[1] || !src[2] ||
          (temp = ((unsigned(unbase64[src[0]]) << 18) |
                   (unsigned(unbase64[src[1]]) << 12) |
                   (unsigned(unbase64[src[2]]) << 6) |
                   (unsigned(unbase64[src[3]])))) & 0x80000000) {
        GET_INPUT(first_no_dest, 4);
        GET_INPUT(second_no_dest, 3);
        GET_INPUT(third_no_dest, 2);
        GET_INPUT(fourth_no_dest, 1);
      } else {
        szsrc -= 4;
        src += 4;
        decode = -1;
        ch = '\0';
      }
      destidx += 3;
    }
  }

#undef GET_INPUT

  // if the loop terminated because we read a bad character, return
  // now.
  if (decode < 0 && ch != '\0' &&
      ch != kPad64Equals && ch != kPad64Dot && !ascii_isspace(ch))
    return -1;

  if (ch == kPad64Equals || ch == kPad64Dot) {
    // if we stopped by hitting an '=' or '.', un-read that character -- we'll
    // look at it again when we count to check for the proper number of
    // equals signs at the end.
    ++szsrc;
    --src;
  } else {
    // This loop consumes 1 input byte per iteration.  It's used to
    // clean up the 0-3 input bytes remaining when the first, faster
    // loop finishes.  'temp' contains the data from 'state' input
    // characters read by the first loop.
    while (szsrc > 0)  {
      --szsrc;
      ch = *src++;
      decode = unbase64[ch];
      if (decode < 0) {
        if (ascii_isspace(ch)) {
          continue;
        } else if (ch == '\0') {
          break;
        } else if (ch == kPad64Equals || ch == kPad64Dot) {
          // back up one character; we'll read it again when we check
          // for the correct number of pad characters at the end.
          ++szsrc;
          --src;
          break;
        } else {
          return -1;
        }
      }

      // Each input character gives us six bits of output.
      temp = (temp << 6) | decode;
      ++state;
      if (state == 4) {
        // If we've accumulated 24 bits of output, write that out as
        // three bytes.
        if (dest) {
          if (destidx+3 > szdest) return -1;
          dest[destidx+2] = temp;
          temp >>= 8;
          dest[destidx+1] = temp;
          temp >>= 8;
          dest[destidx] = temp;
        }
        destidx += 3;
        state = 0;
        temp = 0;
      }
    }
  }

  // Process the leftover data contained in 'temp' at the end of the input.
  int expected_equals = 0;
  switch (state) {
    case 0:
      // Nothing left over; output is a multiple of 3 bytes.
      break;

    case 1:
      // Bad input; we have 6 bits left over.
      return -1;

    case 2:
      // Produce one more output byte from the 12 input bits we have left.
      if (dest) {
        if (destidx+1 > szdest) return -1;
        temp >>= 4;
        dest[destidx] = temp;
      }
      ++destidx;
      expected_equals = 2;
      break;

    case 3:
      // Produce two more output bytes from the 18 input bits we have left.
      if (dest) {
        if (destidx+2 > szdest) return -1;
        temp >>= 2;
        dest[destidx+1] = temp;
        temp >>= 8;
        dest[destidx] = temp;
      }
      destidx += 2;
      expected_equals = 1;
      break;

    default:
      // state should have no other values at this point.
      GOOGLE_LOG(FATAL) << "This can't happen; base64 decoder state = " << state;
  }

  // The remainder of the string should be all whitespace, mixed with
  // exactly 0 equals signs, or exactly 'expected_equals' equals
  // signs.  (Always accepting 0 equals signs is a google extension
  // not covered in the RFC, as is accepting dot as the pad character.)

  int equals = 0;
  while (szsrc > 0 && *src) {
    if (*src == kPad64Equals || *src == kPad64Dot)
      ++equals;
    else if (!ascii_isspace(*src))
      return -1;
    --szsrc;
    ++src;
  }

  return (equals == 0 || equals == expected_equals) ? destidx : -1;
}

// The arrays below were generated by the following code
// #include <sys/time.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdio.h>
// main()
// {
//   static const char Base64[] =
//     "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
//   const char *pos;
//   int idx, i, j;
//   printf("    ");
//   for (i = 0; i < 255; i += 8) {
//     for (j = i; j < i + 8; j++) {
//       pos = strchr(Base64, j);
//       if ((pos == nullptr) || (j == 0))
//         idx = -1;
//       else
//         idx = pos - Base64;
//       if (idx == -1)
//         printf(" %2d,     ", idx);
//       else
//         printf(" %2d/""*%c*""/,", idx, j);
//     }
//     printf("\n    ");
//   }
// }
//
// where the value of "Base64[]" was replaced by one of the base-64 conversion
// tables from the functions below.
static const signed char kUnBase64[] = {
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      62/*+*/, -1,      -1,      -1,      63/*/ */,
  52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
  60/*8*/, 61/*9*/, -1,      -1,      -1,      -1,      -1,      -1,
  -1,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
   7/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
  15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
  23/*X*/, 24/*Y*/, 25/*Z*/, -1,      -1,      -1,      -1,      -1,
  -1,      26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
  33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
  41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
  49/*x*/, 50/*y*/, 51/*z*/, -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1
};
static const signed char kUnWebSafeBase64[] = {
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      62/*-*/, -1,      -1,
  52/*0*/, 53/*1*/, 54/*2*/, 55/*3*/, 56/*4*/, 57/*5*/, 58/*6*/, 59/*7*/,
  60/*8*/, 61/*9*/, -1,      -1,      -1,      -1,      -1,      -1,
  -1,       0/*A*/,  1/*B*/,  2/*C*/,  3/*D*/,  4/*E*/,  5/*F*/,  6/*G*/,
   7/*H*/,  8/*I*/,  9/*J*/, 10/*K*/, 11/*L*/, 12/*M*/, 13/*N*/, 14/*O*/,
  15/*P*/, 16/*Q*/, 17/*R*/, 18/*S*/, 19/*T*/, 20/*U*/, 21/*V*/, 22/*W*/,
  23/*X*/, 24/*Y*/, 25/*Z*/, -1,      -1,      -1,      -1,      63/*_*/,
  -1,      26/*a*/, 27/*b*/, 28/*c*/, 29/*d*/, 30/*e*/, 31/*f*/, 32/*g*/,
  33/*h*/, 34/*i*/, 35/*j*/, 36/*k*/, 37/*l*/, 38/*m*/, 39/*n*/, 40/*o*/,
  41/*p*/, 42/*q*/, 43/*r*/, 44/*s*/, 45/*t*/, 46/*u*/, 47/*v*/, 48/*w*/,
  49/*x*/, 50/*y*/, 51/*z*/, -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1,
  -1,      -1,      -1,      -1,      -1,      -1,      -1,      -1
};

int WebSafeBase64Unescape(const char *src, int szsrc, char *dest, int szdest) {
  return Base64UnescapeInternal(src, szsrc, dest, szdest, kUnWebSafeBase64);
}

static bool Base64UnescapeInternal(const char* src, int slen, string* dest,
                                   const signed char* unbase64) {
  // Determine the size of the output string.  Base64 encodes every 3 bytes into
  // 4 characters.  any leftover chars are added directly for good measure.
  // This is documented in the base64 RFC: http://tools.ietf.org/html/rfc3548
  const int dest_len = 3 * (slen / 4) + (slen % 4);

  dest->resize(dest_len);

  // We are getting the destination buffer by getting the beginning of the
  // string and converting it into a char *.
  const int len = Base64UnescapeInternal(src, slen, string_as_array(dest),
                                         dest_len, unbase64);
  if (len < 0) {
    dest->clear();
    return false;
  }

  // could be shorter if there was padding
  GOOGLE_DCHECK_LE(len, dest_len);
  dest->erase(len);

  return true;
}

bool Base64Unescape(StringPiece src, string* dest) {
  return Base64UnescapeInternal(src.data(), src.size(), dest, kUnBase64);
}

bool WebSafeBase64Unescape(StringPiece src, string* dest) {
  return Base64UnescapeInternal(src.data(), src.size(), dest, kUnWebSafeBase64);
}

int Base64EscapeInternal(const unsigned char *src, int szsrc,
                         char *dest, int szdest, const char *base64,
                         bool do_padding) {
  static const char kPad64 = '=';

  if (szsrc <= 0) return 0;

  if (szsrc * 4 > szdest * 3) return 0;

  char *cur_dest = dest;
  const unsigned char *cur_src = src;

  char *limit_dest = dest + szdest;
  const unsigned char *limit_src = src + szsrc;

  // Three bytes of data encodes to four characters of cyphertext.
  // So we can pump through three-byte chunks atomically.
  while (cur_src < limit_src - 3) {  // keep going as long as we have >= 32 bits
    uint32 in = BigEndian::Load32(cur_src) >> 8;

    cur_dest[0] = base64[in >> 18];
    in &= 0x3FFFF;
    cur_dest[1] = base64[in >> 12];
    in &= 0xFFF;
    cur_dest[2] = base64[in >> 6];
    in &= 0x3F;
    cur_dest[3] = base64[in];

    cur_dest += 4;
    cur_src += 3;
  }
  // To save time, we didn't update szdest or szsrc in the loop.  So do it now.
  szdest = limit_dest - cur_dest;
  szsrc = limit_src - cur_src;

  /* now deal with the tail (<=3 bytes) */
  switch (szsrc) {
    case 0:
      // Nothing left; nothing more to do.
      break;
    case 1: {
      // One byte left: this encodes to two characters, and (optionally)
      // two pad characters to round out the four-character cypherblock.
      if ((szdest -= 2) < 0) return 0;
      uint32 in = cur_src[0];
      cur_dest[0] = base64[in >> 2];
      in &= 0x3;
      cur_dest[1] = base64[in << 4];
      cur_dest += 2;
      if (do_padding) {
        if ((szdest -= 2) < 0) return 0;
        cur_dest[0] = kPad64;
        cur_dest[1] = kPad64;
        cur_dest += 2;
      }
      break;
    }
    case 2: {
      // Two bytes left: this encodes to three characters, and (optionally)
      // one pad character to round out the four-character cypherblock.
      if ((szdest -= 3) < 0) return 0;
      uint32 in = BigEndian::Load16(cur_src);
      cur_dest[0] = base64[in >> 10];
      in &= 0x3FF;
      cur_dest[1] = base64[in >> 4];
      in &= 0x00F;
      cur_dest[2] = base64[in << 2];
      cur_dest += 3;
      if (do_padding) {
        if ((szdest -= 1) < 0) return 0;
        cur_dest[0] = kPad64;
        cur_dest += 1;
      }
      break;
    }
    case 3: {
      // Three bytes left: same as in the big loop above.  We can't do this in
      // the loop because the loop above always reads 4 bytes, and the fourth
      // byte is past the end of the input.
      if ((szdest -= 4) < 0) return 0;
      uint32 in = (cur_src[0] << 16) + BigEndian::Load16(cur_src + 1);
      cur_dest[0] = base64[in >> 18];
      in &= 0x3FFFF;
      cur_dest[1] = base64[in >> 12];
      in &= 0xFFF;
      cur_dest[2] = base64[in >> 6];
      in &= 0x3F;
      cur_dest[3] = base64[in];
      cur_dest += 4;
      break;
    }
    default:
      // Should not be reached: blocks of 4 bytes are handled
      // in the while loop before this switch statement.
      GOOGLE_LOG(FATAL) << "Logic problem? szsrc = " << szsrc;
      break;
  }
  return (cur_dest - dest);
}

static const char kBase64Chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char kWebSafeBase64Chars[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

int Base64Escape(const unsigned char *src, int szsrc, char *dest, int szdest) {
  return Base64EscapeInternal(src, szsrc, dest, szdest, kBase64Chars, true);
}
int WebSafeBase64Escape(const unsigned char *src, int szsrc, char *dest,
                        int szdest, bool do_padding) {
  return Base64EscapeInternal(src, szsrc, dest, szdest,
                              kWebSafeBase64Chars, do_padding);
}

void Base64EscapeInternal(const unsigned char* src, int szsrc,
                          string* dest, bool do_padding,
                          const char* base64_chars) {
  const int calc_escaped_size =
    CalculateBase64EscapedLen(szsrc, do_padding);
  dest->resize(calc_escaped_size);
  const int escaped_len = Base64EscapeInternal(src, szsrc,
                                               string_as_array(dest),
                                               dest->size(),
                                               base64_chars,
                                               do_padding);
  GOOGLE_DCHECK_EQ(calc_escaped_size, escaped_len);
  dest->erase(escaped_len);
}

void Base64Escape(const unsigned char *src, int szsrc,
                  string* dest, bool do_padding) {
  Base64EscapeInternal(src, szsrc, dest, do_padding, kBase64Chars);
}

void WebSafeBase64Escape(const unsigned char *src, int szsrc,
                         string *dest, bool do_padding) {
  Base64EscapeInternal(src, szsrc, dest, do_padding, kWebSafeBase64Chars);
}

void Base64Escape(StringPiece src, string* dest) {
  Base64Escape(reinterpret_cast<const unsigned char*>(src.data()),
               src.size(), dest, true);
}

void WebSafeBase64Escape(StringPiece src, string* dest) {
  WebSafeBase64Escape(reinterpret_cast<const unsigned char*>(src.data()),
                      src.size(), dest, false);
}

void WebSafeBase64EscapeWithPadding(StringPiece src, string* dest) {
  WebSafeBase64Escape(reinterpret_cast<const unsigned char*>(src.data()),
                      src.size(), dest, true);
}

// Helper to append a Unicode code point to a string as UTF8, without bringing
// in any external dependencies.
int EncodeAsUTF8Char(uint32 code_point, char* output) {
  uint32 tmp = 0;
  int len = 0;
  if (code_point <= 0x7f) {
    tmp = code_point;
    len = 1;
  } else if (code_point <= 0x07ff) {
    tmp = 0x0000c080 |
        ((code_point & 0x07c0) << 2) |
        (code_point & 0x003f);
    len = 2;
  } else if (code_point <= 0xffff) {
    tmp = 0x00e08080 |
        ((code_point & 0xf000) << 4) |
        ((code_point & 0x0fc0) << 2) |
        (code_point & 0x003f);
    len = 3;
  } else {
    // UTF-16 is only defined for code points up to 0x10FFFF, and UTF-8 is
    // normally only defined up to there as well.
    tmp = 0xf0808080 |
        ((code_point & 0x1c0000) << 6) |
        ((code_point & 0x03f000) << 4) |
        ((code_point & 0x000fc0) << 2) |
        (code_point & 0x003f);
    len = 4;
  }
  tmp = ghtonl(tmp);
  memcpy(output, reinterpret_cast<const char*>(&tmp) + sizeof(tmp) - len, len);
  return len;
}

// Table of UTF-8 character lengths, based on first byte
static const unsigned char kUTF8LenTbl[256] = {
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,

  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4
};

// Return length of a single UTF-8 source character
int UTF8FirstLetterNumBytes(const char* src, int len) {
  if (len == 0) {
    return 0;
  }
  return kUTF8LenTbl[*reinterpret_cast<const uint8*>(src)];
}

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

void CleanStringLineEndings(const string &src, string *dst,
                            bool auto_end_last_line) {
  if (dst->empty()) {
    dst->append(src);
    CleanStringLineEndings(dst, auto_end_last_line);
  } else {
    string tmp = src;
    CleanStringLineEndings(&tmp, auto_end_last_line);
    dst->append(tmp);
  }
}

void CleanStringLineEndings(string *str, bool auto_end_last_line) {
  ptrdiff_t output_pos = 0;
  bool r_seen = false;
  ptrdiff_t len = str->size();

  char *p = &(*str)[0];

  for (ptrdiff_t input_pos = 0; input_pos < len;) {
    if (!r_seen && input_pos + 8 < len) {
      uint64_t v = GOOGLE_UNALIGNED_LOAD64(p + input_pos);
      // Loop over groups of 8 bytes at a time until we come across
      // a word that has a byte whose value is less than or equal to
      // '\r' (i.e. could contain a \n (0x0a) or a \r (0x0d) ).
      //
      // We use a has_less macro that quickly tests a whole 64-bit
      // word to see if any of the bytes has a value < N.
      //
      // For more details, see:
      //   http://graphics.stanford.edu/~seander/bithacks.html#HasLessInWord
#define has_less(x, n) (((x) - ~0ULL / 255 * (n)) & ~(x) & ~0ULL / 255 * 128)
      if (!has_less(v, '\r' + 1)) {
#undef has_less
        // No byte in this word has a value that could be a \r or a \n
        if (output_pos != input_pos) {
          GOOGLE_UNALIGNED_STORE64(p + output_pos, v);
        }
        input_pos += 8;
        output_pos += 8;
        continue;
      }
    }
    string::const_reference in = p[input_pos];
    if (in == '\r') {
      if (r_seen) p[output_pos++] = '\n';
      r_seen = true;
    } else if (in == '\n') {
      if (input_pos != output_pos)
        p[output_pos++] = '\n';
      else
        output_pos++;
      r_seen = false;
    } else {
      if (r_seen) p[output_pos++] = '\n';
      r_seen = false;
      if (input_pos != output_pos)
        p[output_pos++] = in;
      else
        output_pos++;
    }
    input_pos++;
  }
  if (r_seen ||
      (auto_end_last_line && output_pos > 0 && p[output_pos - 1] != '\n')) {
    str->resize(output_pos + 1);
    str->operator[](output_pos) = '\n';
  } else if (output_pos < len) {
    str->resize(output_pos);
  }
}

}  // namespace protobuf
}  // namespace google
