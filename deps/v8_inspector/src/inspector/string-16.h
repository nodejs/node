// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_STRING16_H_
#define V8_INSPECTOR_STRING16_H_

#include <stdint.h>
#include <cctype>
#include <climits>
#include <cstring>
#include <string>
#include <vector>

namespace v8_inspector {

using UChar = uint16_t;

class String16 {
 public:
  static const size_t kNotFound = static_cast<size_t>(-1);

  String16() {}
  String16(const String16& other) : m_impl(other.m_impl) {}
  String16(const UChar* characters, size_t size) : m_impl(characters, size) {}
  String16(const UChar* characters)  // NOLINT(runtime/explicit)
      : m_impl(characters) {}
  String16(const char* characters)  // NOLINT(runtime/explicit)
      : String16(characters, std::strlen(characters)) {}
  String16(const char* characters, size_t size) {
    m_impl.resize(size);
    for (size_t i = 0; i < size; ++i) m_impl[i] = characters[i];
  }

  static String16 fromInteger(int);
  static String16 fromInteger(size_t);
  static String16 fromDouble(double);
  static String16 fromDoublePrecision3(double);
  static String16 fromDoublePrecision6(double);

  int toInteger(bool* ok = nullptr) const;
  String16 stripWhiteSpace() const;
  const UChar* characters16() const { return m_impl.c_str(); }
  size_t length() const { return m_impl.length(); }
  bool isEmpty() const { return !m_impl.length(); }
  UChar operator[](size_t index) const { return m_impl[index]; }
  String16 substring(size_t pos, size_t len = UINT_MAX) const {
    return String16(m_impl.substr(pos, len));
  }
  size_t find(const String16& str, size_t start = 0) const {
    return m_impl.find(str.m_impl, start);
  }
  size_t reverseFind(const String16& str, size_t start = UINT_MAX) const {
    return m_impl.rfind(str.m_impl, start);
  }
  void swap(String16& other) { m_impl.swap(other.m_impl); }

  // Convenience methods.
  std::string utf8() const;
  static String16 fromUTF8(const char* stringStart, size_t length);

  const std::basic_string<UChar>& impl() const { return m_impl; }
  explicit String16(const std::basic_string<UChar>& impl) : m_impl(impl) {}

  std::size_t hash() const {
    if (!has_hash) {
      size_t hash = 0;
      for (size_t i = 0; i < length(); ++i) hash = 31 * hash + m_impl[i];
      hash_code = hash;
      has_hash = true;
    }
    return hash_code;
  }

 private:
  std::basic_string<UChar> m_impl;
  mutable bool has_hash = false;
  mutable std::size_t hash_code = 0;
};

inline bool operator==(const String16& a, const String16& b) {
  return a.impl() == b.impl();
}
inline bool operator<(const String16& a, const String16& b) {
  return a.impl() < b.impl();
}
inline bool operator!=(const String16& a, const String16& b) {
  return a.impl() != b.impl();
}
inline bool operator==(const String16& a, const char* b) {
  return a.impl() == String16(b).impl();
}
inline String16 operator+(const String16& a, const char* b) {
  return String16(a.impl() + String16(b).impl());
}
inline String16 operator+(const char* a, const String16& b) {
  return String16(String16(a).impl() + b.impl());
}
inline String16 operator+(const String16& a, const String16& b) {
  return String16(a.impl() + b.impl());
}

class String16Builder {
 public:
  String16Builder();
  void append(const String16&);
  void append(UChar);
  void append(char);
  void append(const UChar*, size_t);
  void append(const char*, size_t);
  String16 toString();
  void reserveCapacity(size_t);

 private:
  std::vector<UChar> m_buffer;
};

}  // namespace v8_inspector

#if !defined(__APPLE__) || defined(_LIBCPP_VERSION)

namespace std {
template <>
struct hash<v8_inspector::String16> {
  std::size_t operator()(const v8_inspector::String16& string) const {
    return string.hash();
  }
};

}  // namespace std

#endif  // !defined(__APPLE__) || defined(_LIBCPP_VERSION)

#endif  // V8_INSPECTOR_STRING16_H_
