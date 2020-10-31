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

// Author: kenton@google.com (Kenton Varda)

#ifndef GOOGLE_PROTOBUF_STUBS_HASH_H__
#define GOOGLE_PROTOBUF_STUBS_HASH_H__

#include <string.h>
#include <google/protobuf/stubs/common.h>

#include <unordered_map>
#include <unordered_set>

# define GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_START \
  namespace google {                                      \
  namespace protobuf {
# define GOOGLE_PROTOBUF_HASH_NAMESPACE_DECLARATION_END }}

namespace google {
namespace protobuf {

template <typename Key>
struct hash : public std::hash<Key> {};

template <typename Key>
struct hash<const Key*> {
  inline size_t operator()(const Key* key) const {
    return reinterpret_cast<size_t>(key);
  }
};

// Unlike the old SGI version, the TR1 "hash" does not special-case char*.  So,
// we go ahead and provide our own implementation.
template <>
struct hash<const char*> {
  inline size_t operator()(const char* str) const {
    size_t result = 0;
    for (; *str != '\0'; str++) {
      result = 5 * result + static_cast<size_t>(*str);
    }
    return result;
  }
};

template<>
struct hash<bool> {
  size_t operator()(bool x) const {
    return static_cast<size_t>(x);
  }
};

template <>
struct hash<string> {
  inline size_t operator()(const string& key) const {
    return hash<const char*>()(key.c_str());
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline bool operator()(const string& a, const string& b) const {
    return a < b;
  }
};

template <typename First, typename Second>
struct hash<std::pair<First, Second> > {
  inline size_t operator()(const std::pair<First, Second>& key) const {
    size_t first_hash = hash<First>()(key.first);
    size_t second_hash = hash<Second>()(key.second);

    // FIXME(kenton):  What is the best way to compute this hash?  I have
    // no idea!  This seems a bit better than an XOR.
    return first_hash * ((1 << 16) - 1) + second_hash;
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline bool operator()(const std::pair<First, Second>& a,
                           const std::pair<First, Second>& b) const {
    return a < b;
  }
};

// Used by GCC/SGI STL only.  (Why isn't this provided by the standard
// library?  :( )
struct streq {
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) == 0;
  }
};

}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_STUBS_HASH_H__
