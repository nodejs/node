// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2009-2013, International Business Machines
// Corporation and others. All Rights Reserved.
//
// Copyright 2001 and onwards Google Inc.
// Author: Sanjay Ghemawat

// This code is a contribution of Google code, and the style used here is
// a compromise between the original Google code and the ICU coding guidelines.
// For example, data types are ICU-ified (size_t,int->int32_t),
// and API comments doxygen-ified, but function names and behavior are
// as in the original, if possible.
// Assertion-style error handling, not available in ICU, was changed to
// parameter "pinning" similar to UnicodeString.
//
// In addition, this is only a partial port of the original Google code,
// limited to what was needed so far. The (nearly) complete original code
// is in the ICU svn repository at icuhtml/trunk/design/strings/contrib
// (see ICU ticket 6765, r25517).

#ifndef __STRINGPIECE_H__
#define __STRINGPIECE_H__

/**
 * \file 
 * \brief C++ API: StringPiece: Read-only byte string wrapper class.
 */

#include "unicode/utypes.h"

#if U_SHOW_CPLUSPLUS_API

#include <cstddef>
#include <type_traits>

#include "unicode/uobject.h"
#include "unicode/std_string.h"

// Arghh!  I wish C++ literals were "string".

U_NAMESPACE_BEGIN

/**
 * A string-like object that points to a sized piece of memory.
 *
 * We provide non-explicit singleton constructors so users can pass
 * in a "const char*" or a "string" wherever a "StringPiece" is
 * expected.
 *
 * Functions or methods may use StringPiece parameters to accept either a
 * "const char*" or a "string" value that will be implicitly converted to a
 * StringPiece.
 *
 * Systematic usage of StringPiece is encouraged as it will reduce unnecessary
 * conversions from "const char*" to "string" and back again.
 *
 * @stable ICU 4.2
 */
class U_COMMON_API StringPiece : public UMemory {
 private:
  const char*   ptr_;
  int32_t       length_;

 public:
  /**
   * Default constructor, creates an empty StringPiece.
   * @stable ICU 4.2
   */
  StringPiece() : ptr_(NULL), length_(0) { }
  /**
   * Constructs from a NUL-terminated const char * pointer.
   * @param str a NUL-terminated const char * pointer
   * @stable ICU 4.2
   */
  StringPiece(const char* str);
  /**
   * Constructs from a std::string.
   * @stable ICU 4.2
   */
  StringPiece(const std::string& str)
    : ptr_(str.data()), length_(static_cast<int32_t>(str.size())) { }
#ifndef U_HIDE_DRAFT_API
  /**
   * Constructs from some other implementation of a string piece class, from any
   * C++ record type that has these two methods:
   *
   * \code{.cpp}
   *
   *   struct OtherStringPieceClass {
   *     const char* data();
   *     size_t size();
   *   };
   *
   * \endcode
   *
   * The other string piece class will typically be std::string_view from C++17
   * or absl::string_view from Abseil.
   *
   * @param str the other string piece
   * @draft ICU 65
   */
  template <typename T,
            typename = typename std::enable_if<
                std::is_same<decltype(T().data()), const char*>::value &&
                std::is_same<decltype(T().size()), size_t>::value>::type>
  StringPiece(T str)
      : ptr_(str.data()), length_(static_cast<int32_t>(str.size())) {}
#endif  // U_HIDE_DRAFT_API
  /**
   * Constructs from a const char * pointer and a specified length.
   * @param offset a const char * pointer (need not be terminated)
   * @param len the length of the string; must be non-negative
   * @stable ICU 4.2
   */
  StringPiece(const char* offset, int32_t len) : ptr_(offset), length_(len) { }
  /**
   * Substring of another StringPiece.
   * @param x the other StringPiece
   * @param pos start position in x; must be non-negative and <= x.length().
   * @stable ICU 4.2
   */
  StringPiece(const StringPiece& x, int32_t pos);
  /**
   * Substring of another StringPiece.
   * @param x the other StringPiece
   * @param pos start position in x; must be non-negative and <= x.length().
   * @param len length of the substring;
   *            must be non-negative and will be pinned to at most x.length() - pos.
   * @stable ICU 4.2
   */
  StringPiece(const StringPiece& x, int32_t pos, int32_t len);

  /**
   * Returns the string pointer. May be NULL if it is empty.
   *
   * data() may return a pointer to a buffer with embedded NULs, and the
   * returned buffer may or may not be null terminated.  Therefore it is
   * typically a mistake to pass data() to a routine that expects a NUL
   * terminated string.
   * @return the string pointer
   * @stable ICU 4.2
   */
  const char* data() const { return ptr_; }
  /**
   * Returns the string length. Same as length().
   * @return the string length
   * @stable ICU 4.2
   */
  int32_t size() const { return length_; }
  /**
   * Returns the string length. Same as size().
   * @return the string length
   * @stable ICU 4.2
   */
  int32_t length() const { return length_; }
  /**
   * Returns whether the string is empty.
   * @return TRUE if the string is empty
   * @stable ICU 4.2
   */
  UBool empty() const { return length_ == 0; }

  /**
   * Sets to an empty string.
   * @stable ICU 4.2
   */
  void clear() { ptr_ = NULL; length_ = 0; }

  /**
   * Reset the stringpiece to refer to new data.
   * @param xdata pointer the new string data.  Need not be nul terminated.
   * @param len the length of the new data
   * @stable ICU 4.8
   */
  void set(const char* xdata, int32_t len) { ptr_ = xdata; length_ = len; }

  /**
   * Reset the stringpiece to refer to new data.
   * @param str a pointer to a NUL-terminated string. 
   * @stable ICU 4.8
   */
  void set(const char* str);

  /**
   * Removes the first n string units.
   * @param n prefix length, must be non-negative and <=length()
   * @stable ICU 4.2
   */
  void remove_prefix(int32_t n) {
    if (n >= 0) {
      if (n > length_) {
        n = length_;
      }
      ptr_ += n;
      length_ -= n;
    }
  }

  /**
   * Removes the last n string units.
   * @param n suffix length, must be non-negative and <=length()
   * @stable ICU 4.2
   */
  void remove_suffix(int32_t n) {
    if (n >= 0) {
      if (n <= length_) {
        length_ -= n;
      } else {
        length_ = 0;
      }
    }
  }

  /**
   * Maximum integer, used as a default value for substring methods.
   * @stable ICU 4.2
   */
  static const int32_t npos; // = 0x7fffffff;

  /**
   * Returns a substring of this StringPiece.
   * @param pos start position; must be non-negative and <= length().
   * @param len length of the substring;
   *            must be non-negative and will be pinned to at most length() - pos.
   * @return the substring StringPiece
   * @stable ICU 4.2
   */
  StringPiece substr(int32_t pos, int32_t len = npos) const {
    return StringPiece(*this, pos, len);
  }
};

/**
 * Global operator == for StringPiece
 * @param x The first StringPiece to compare.
 * @param y The second StringPiece to compare.
 * @return TRUE if the string data is equal
 * @stable ICU 4.8
 */
U_EXPORT UBool U_EXPORT2 
operator==(const StringPiece& x, const StringPiece& y);

/**
 * Global operator != for StringPiece
 * @param x The first StringPiece to compare.
 * @param y The second StringPiece to compare.
 * @return TRUE if the string data is not equal
 * @stable ICU 4.8
 */
inline UBool operator!=(const StringPiece& x, const StringPiece& y) {
  return !(x == y);
}

U_NAMESPACE_END

#endif /* U_SHOW_CPLUSPLUS_API */

#endif  // __STRINGPIECE_H__
