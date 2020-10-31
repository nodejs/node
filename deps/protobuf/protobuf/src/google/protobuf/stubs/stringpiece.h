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

// A StringPiece points to part or all of a string, Cord, double-quoted string
// literal, or other string-like object.  A StringPiece does *not* own the
// string to which it points.  A StringPiece is not null-terminated.
//
// You can use StringPiece as a function or method parameter.  A StringPiece
// parameter can receive a double-quoted string literal argument, a "const
// char*" argument, a string argument, or a StringPiece argument with no data
// copying.  Systematic use of StringPiece for arguments reduces data
// copies and strlen() calls.
//
// Prefer passing StringPieces by value:
//   void MyFunction(StringPiece arg);
// If circumstances require, you may also pass by const reference:
//   void MyFunction(const StringPiece& arg);  // not preferred
// Both of these have the same lifetime semantics.  Passing by value
// generates slightly smaller code.  For more discussion, see the thread
// go/stringpiecebyvalue on c-users.
//
// StringPiece is also suitable for local variables if you know that
// the lifetime of the underlying object is longer than the lifetime
// of your StringPiece variable.
//
// Beware of binding a StringPiece to a temporary:
//   StringPiece sp = obj.MethodReturningString();  // BAD: lifetime problem
//
// This code is okay:
//   string str = obj.MethodReturningString();  // str owns its contents
//   StringPiece sp(str);  // GOOD, because str outlives sp
//
// StringPiece is sometimes a poor choice for a return value and usually a poor
// choice for a data member.  If you do use a StringPiece this way, it is your
// responsibility to ensure that the object pointed to by the StringPiece
// outlives the StringPiece.
//
// A StringPiece may represent just part of a string; thus the name "Piece".
// For example, when splitting a string, vector<StringPiece> is a natural data
// type for the output.  For another example, a Cord is a non-contiguous,
// potentially very long string-like object.  The Cord class has an interface
// that iteratively provides StringPiece objects that point to the
// successive pieces of a Cord object.
//
// A StringPiece is not null-terminated.  If you write code that scans a
// StringPiece, you must check its length before reading any characters.
// Common idioms that work on null-terminated strings do not work on
// StringPiece objects.
//
// There are several ways to create a null StringPiece:
//   StringPiece()
//   StringPiece(nullptr)
//   StringPiece(nullptr, 0)
// For all of the above, sp.data() == nullptr, sp.length() == 0,
// and sp.empty() == true.  Also, if you create a StringPiece with
// a non-null pointer then sp.data() != nullptr.  Once created,
// sp.data() will stay either nullptr or not-nullptr, except if you call
// sp.clear() or sp.set().
//
// Thus, you can use StringPiece(nullptr) to signal an out-of-band value
// that is different from other StringPiece values.  This is similar
// to the way that const char* p1 = nullptr; is different from
// const char* p2 = "";.
//
// There are many ways to create an empty StringPiece:
//   StringPiece()
//   StringPiece(nullptr)
//   StringPiece(nullptr, 0)
//   StringPiece("")
//   StringPiece("", 0)
//   StringPiece("abcdef", 0)
//   StringPiece("abcdef"+6, 0)
// For all of the above, sp.length() will be 0 and sp.empty() will be true.
// For some empty StringPiece values, sp.data() will be nullptr.
// For some empty StringPiece values, sp.data() will not be nullptr.
//
// Be careful not to confuse: null StringPiece and empty StringPiece.
// The set of empty StringPieces properly includes the set of null StringPieces.
// That is, every null StringPiece is an empty StringPiece,
// but some non-null StringPieces are empty Stringpieces too.
//
// All empty StringPiece values compare equal to each other.
// Even a null StringPieces compares equal to a non-null empty StringPiece:
//  StringPiece() == StringPiece("", 0)
//  StringPiece(nullptr) == StringPiece("abc", 0)
//  StringPiece(nullptr, 0) == StringPiece("abcdef"+6, 0)
//
// Look carefully at this example:
//   StringPiece("") == nullptr
// True or false?  TRUE, because StringPiece::operator== converts
// the right-hand side from nullptr to StringPiece(nullptr),
// and then compares two zero-length spans of characters.
// However, we are working to make this example produce a compile error.
//
// Suppose you want to write:
//   bool TestWhat?(StringPiece sp) { return sp == nullptr; }  // BAD
// Do not do that.  Write one of these instead:
//   bool TestNull(StringPiece sp) { return sp.data() == nullptr; }
//   bool TestEmpty(StringPiece sp) { return sp.empty(); }
// The intent of TestWhat? is unclear.  Did you mean TestNull or TestEmpty?
// Right now, TestWhat? behaves likes TestEmpty.
// We are working to make TestWhat? produce a compile error.
// TestNull is good to test for an out-of-band signal.
// TestEmpty is good to test for an empty StringPiece.
//
// Caveats (again):
// (1) The lifetime of the pointed-to string (or piece of a string)
//     must be longer than the lifetime of the StringPiece.
// (2) There may or may not be a '\0' character after the end of
//     StringPiece data.
// (3) A null StringPiece is empty.
//     An empty StringPiece may or may not be a null StringPiece.

#ifndef GOOGLE_PROTOBUF_STUBS_STRINGPIECE_H_
#define GOOGLE_PROTOBUF_STUBS_STRINGPIECE_H_

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <iosfwd>
#include <limits>
#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
// StringPiece has *two* size types.
// StringPiece::size_type
//   is unsigned
//   is 32 bits in LP32, 64 bits in LP64, 64 bits in LLP64
//   no future changes intended
// stringpiece_ssize_type
//   is signed
//   is 32 bits in LP32, 64 bits in LP64, 64 bits in LLP64
//   future changes intended: http://go/64BitStringPiece
//
typedef string::difference_type stringpiece_ssize_type;

// STRINGPIECE_CHECK_SIZE protects us from 32-bit overflows.
// TODO(mec): delete this after stringpiece_ssize_type goes 64 bit.
#if !defined(NDEBUG)
#define STRINGPIECE_CHECK_SIZE 1
#elif defined(_FORTIFY_SOURCE) && _FORTIFY_SOURCE > 0
#define STRINGPIECE_CHECK_SIZE 1
#else
#define STRINGPIECE_CHECK_SIZE 0
#endif

class PROTOBUF_EXPORT StringPiece {
 private:
  const char* ptr_;
  stringpiece_ssize_type length_;

  // Prevent overflow in debug mode or fortified mode.
  // sizeof(stringpiece_ssize_type) may be smaller than sizeof(size_t).
  static stringpiece_ssize_type CheckedSsizeTFromSizeT(size_t size) {
#if STRINGPIECE_CHECK_SIZE > 0
#ifdef max
#undef max
#endif
    if (size > static_cast<size_t>(
        std::numeric_limits<stringpiece_ssize_type>::max())) {
      // Some people grep for this message in logs
      // so take care if you ever change it.
      LogFatalSizeTooBig(size, "size_t to int conversion");
    }
#endif
    return static_cast<stringpiece_ssize_type>(size);
  }

  // Out-of-line error path.
  static void LogFatalSizeTooBig(size_t size, const char* details);

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected.
  //
  // Style guide exception granted:
  // http://goto/style-guide-exception-20978288
  StringPiece() : ptr_(nullptr), length_(0) {}

  StringPiece(const char* str)  // NOLINT(runtime/explicit)
      : ptr_(str), length_(0) {
    if (str != nullptr) {
      length_ = CheckedSsizeTFromSizeT(strlen(str));
    }
  }

  template <class Allocator>
  StringPiece(  // NOLINT(runtime/explicit)
      const std::basic_string<char, std::char_traits<char>, Allocator>& str)
      : ptr_(str.data()), length_(0) {
    length_ = CheckedSsizeTFromSizeT(str.size());
  }

  StringPiece(const char* offset, stringpiece_ssize_type len)
      : ptr_(offset), length_(len) {
    assert(len >= 0);
  }

  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  StringPiece(StringPiece x, stringpiece_ssize_type pos);
  // Substring of another StringPiece.
  // pos must be non-negative and <= x.length().
  // len must be non-negative and will be pinned to at most x.length() - pos.
  StringPiece(StringPiece x,
              stringpiece_ssize_type pos,
              stringpiece_ssize_type len);

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const char* data() const { return ptr_; }
  stringpiece_ssize_type size() const { return length_; }
  stringpiece_ssize_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = nullptr;
    length_ = 0;
  }

  void set(const char* data, stringpiece_ssize_type len) {
    assert(len >= 0);
    ptr_ = data;
    length_ = len;
  }

  void set(const char* str) {
    ptr_ = str;
    if (str != nullptr)
      length_ = CheckedSsizeTFromSizeT(strlen(str));
    else
      length_ = 0;
  }

  void set(const void* data, stringpiece_ssize_type len) {
    ptr_ = reinterpret_cast<const char*>(data);
    length_ = len;
  }

  char operator[](stringpiece_ssize_type i) const {
    assert(0 <= i);
    assert(i < length_);
    return ptr_[i];
  }

  void remove_prefix(stringpiece_ssize_type n) {
    assert(length_ >= n);
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(stringpiece_ssize_type n) {
    assert(length_ >= n);
    length_ -= n;
  }

  // returns {-1, 0, 1}
  int compare(StringPiece x) const {
    const stringpiece_ssize_type min_size =
        length_ < x.length_ ? length_ : x.length_;
    int r = memcmp(ptr_, x.ptr_, static_cast<size_t>(min_size));
    if (r < 0) return -1;
    if (r > 0) return 1;
    if (length_ < x.length_) return -1;
    if (length_ > x.length_) return 1;
    return 0;
  }

  string as_string() const {
    return ToString();
  }
  // We also define ToString() here, since many other string-like
  // interfaces name the routine that converts to a C++ string
  // "ToString", and it's confusing to have the method that does that
  // for a StringPiece be called "as_string()".  We also leave the
  // "as_string()" method defined here for existing code.
  string ToString() const {
    if (ptr_ == nullptr) return string();
    return string(data(), static_cast<size_type>(size()));
  }

  operator string() const {
    return ToString();
  }

  void CopyToString(string* target) const;
  void AppendToString(string* target) const;

  bool starts_with(StringPiece x) const {
    return (length_ >= x.length_) &&
           (memcmp(ptr_, x.ptr_, static_cast<size_t>(x.length_)) == 0);
  }

  bool ends_with(StringPiece x) const {
    return ((length_ >= x.length_) &&
            (memcmp(ptr_ + (length_-x.length_), x.ptr_,
                 static_cast<size_t>(x.length_)) == 0));
  }

  // Checks whether StringPiece starts with x and if so advances the beginning
  // of it to past the match.  It's basically a shortcut for starts_with
  // followed by remove_prefix.
  bool Consume(StringPiece x);
  // Like above but for the end of the string.
  bool ConsumeFromEnd(StringPiece x);

  // standard STL container boilerplate
  typedef char value_type;
  typedef const char* pointer;
  typedef const char& reference;
  typedef const char& const_reference;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  static const size_type npos;
  typedef const char* const_iterator;
  typedef const char* iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  iterator begin() const { return ptr_; }
  iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }
  stringpiece_ssize_type max_size() const { return length_; }
  stringpiece_ssize_type capacity() const { return length_; }

  // cpplint.py emits a false positive [build/include_what_you_use]
  stringpiece_ssize_type copy(char* buf, size_type n, size_type pos = 0) const;  // NOLINT

  bool contains(StringPiece s) const;

  stringpiece_ssize_type find(StringPiece s, size_type pos = 0) const;
  stringpiece_ssize_type find(char c, size_type pos = 0) const;
  stringpiece_ssize_type rfind(StringPiece s, size_type pos = npos) const;
  stringpiece_ssize_type rfind(char c, size_type pos = npos) const;

  stringpiece_ssize_type find_first_of(StringPiece s, size_type pos = 0) const;
  stringpiece_ssize_type find_first_of(char c, size_type pos = 0) const {
    return find(c, pos);
  }
  stringpiece_ssize_type find_first_not_of(StringPiece s,
                                           size_type pos = 0) const;
  stringpiece_ssize_type find_first_not_of(char c, size_type pos = 0) const;
  stringpiece_ssize_type find_last_of(StringPiece s,
                                      size_type pos = npos) const;
  stringpiece_ssize_type find_last_of(char c, size_type pos = npos) const {
    return rfind(c, pos);
  }
  stringpiece_ssize_type find_last_not_of(StringPiece s,
                                          size_type pos = npos) const;
  stringpiece_ssize_type find_last_not_of(char c, size_type pos = npos) const;

  StringPiece substr(size_type pos, size_type n = npos) const;
};

// This large function is defined inline so that in a fairly common case where
// one of the arguments is a literal, the compiler can elide a lot of the
// following comparisons.
inline bool operator==(StringPiece x, StringPiece y) {
  stringpiece_ssize_type len = x.size();
  if (len != y.size()) {
    return false;
  }

  return x.data() == y.data() || len <= 0 ||
      memcmp(x.data(), y.data(), static_cast<size_t>(len)) == 0;
}

inline bool operator!=(StringPiece x, StringPiece y) {
  return !(x == y);
}

inline bool operator<(StringPiece x, StringPiece y) {
  const stringpiece_ssize_type min_size =
      x.size() < y.size() ? x.size() : y.size();
  const int r = memcmp(x.data(), y.data(), static_cast<size_t>(min_size));
  return (r < 0) || (r == 0 && x.size() < y.size());
}

inline bool operator>(StringPiece x, StringPiece y) {
  return y < x;
}

inline bool operator<=(StringPiece x, StringPiece y) {
  return !(x > y);
}

inline bool operator>=(StringPiece x, StringPiece y) {
  return !(x < y);
}

// allow StringPiece to be logged
extern std::ostream& operator<<(std::ostream& o, StringPiece piece);

namespace internal {
// StringPiece is not a POD and can not be used in an union (pre C++11). We
// need a POD version of it.
struct StringPiecePod {
  // Create from a StringPiece.
  static StringPiecePod CreateFromStringPiece(StringPiece str) {
    StringPiecePod pod;
    pod.data_ = str.data();
    pod.size_ = str.size();
    return pod;
  }

  // Cast to StringPiece.
  operator StringPiece() const { return StringPiece(data_, size_); }

  bool operator==(const char* value) const {
    return StringPiece(data_, size_) == StringPiece(value);
  }

  char operator[](stringpiece_ssize_type i) const {
    assert(0 <= i);
    assert(i < size_);
    return data_[i];
  }

  const char* data() const { return data_; }

  stringpiece_ssize_type size() const {
    return size_;
  }

  std::string ToString() const {
    return std::string(data_, static_cast<size_t>(size_));
  }

  operator string() const { return ToString(); }

 private:
  const char* data_;
  stringpiece_ssize_type size_;
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_START
template<> struct hash<StringPiece> {
  size_t operator()(const StringPiece& s) const {
    size_t result = 0;
    for (const char *str = s.data(), *end = str + s.size(); str < end; str++) {
      result = 5 * result + static_cast<size_t>(*str);
    }
    return result;
  }
};
GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_END

#include <google/protobuf/port_undef.inc>

#endif  // STRINGS_STRINGPIECE_H_
