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

#ifndef GOOGLE_PROTOBUF_HAS_BITS_H__
#define GOOGLE_PROTOBUF_HAS_BITS_H__

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/port.h>

#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
namespace internal {

template <size_t doublewords>
class HasBits {
 public:
  HasBits() PROTOBUF_ALWAYS_INLINE { Clear(); }

  void Clear() PROTOBUF_ALWAYS_INLINE {
    memset(has_bits_, 0, sizeof(has_bits_));
  }

  uint32& operator[](int index) PROTOBUF_ALWAYS_INLINE {
    return has_bits_[index];
  }

  const uint32& operator[](int index) const PROTOBUF_ALWAYS_INLINE {
    return has_bits_[index];
  }

  bool operator==(const HasBits<doublewords>& rhs) const {
    return memcmp(has_bits_, rhs.has_bits_, sizeof(has_bits_)) == 0;
  }

  bool operator!=(const HasBits<doublewords>& rhs) const {
    return !(*this == rhs);
  }

  void Or(const HasBits<doublewords>& rhs) {
    for (int i = 0; i < doublewords; i++) has_bits_[i] |= rhs[i];
  }

  bool empty() const;

 private:
  uint32 has_bits_[doublewords];
};

template <>
inline bool HasBits<1>::empty() const {
  return !has_bits_[0];
}

template <>
inline bool HasBits<2>::empty() const {
  return !(has_bits_[0] | has_bits_[1]);
}

template <>
inline bool HasBits<3>::empty() const {
  return !(has_bits_[0] | has_bits_[1] | has_bits_[2]);
}

template <>
inline bool HasBits<4>::empty() const {
  return !(has_bits_[0] | has_bits_[1] | has_bits_[2] | has_bits_[3]);
}

template <size_t doublewords>
inline bool HasBits<doublewords>::empty() const {
  for (size_t i = 0; i < doublewords; ++i) {
    if (has_bits_[i]) return false;
  }
  return true;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_HAS_BITS_H__
