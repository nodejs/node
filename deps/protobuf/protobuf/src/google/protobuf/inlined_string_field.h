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

#ifndef GOOGLE_PROTOBUF_INLINED_STRING_FIELD_H__
#define GOOGLE_PROTOBUF_INLINED_STRING_FIELD_H__

#include <string>
#include <utility>

#include <google/protobuf/port.h>
#include <google/protobuf/stubs/stringpiece.h>

#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {

class Arena;

namespace internal {

// InlinedStringField wraps a std::string instance and exposes an API similar to
// ArenaStringPtr's wrapping of a std::string* instance.  As std::string is
// never allocated on the Arena, we expose only the *NoArena methods of
// ArenaStringPtr.
//
// default_value parameters are taken for consistency with ArenaStringPtr, but
// are not used for most methods.  With inlining, these should be removed from
// the generated binary.
class PROTOBUF_EXPORT InlinedStringField {
 public:
  InlinedStringField() PROTOBUF_ALWAYS_INLINE;
  explicit InlinedStringField(const std::string& default_value);

  void AssignWithDefault(const std::string* default_value,
                         const InlinedStringField& from) PROTOBUF_ALWAYS_INLINE;

  void ClearToEmpty(const std::string* default_value,
                    Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    ClearToEmptyNoArena(default_value);
  }
  void ClearNonDefaultToEmpty() PROTOBUF_ALWAYS_INLINE {
    ClearNonDefaultToEmptyNoArena();
  }
  void ClearToEmptyNoArena(const std::string* /*default_value*/)
      PROTOBUF_ALWAYS_INLINE {
    ClearNonDefaultToEmptyNoArena();
  }
  void ClearNonDefaultToEmptyNoArena() PROTOBUF_ALWAYS_INLINE;

  void ClearToDefault(const std::string* default_value,
                      Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    ClearToDefaultNoArena(default_value);
  }
  void ClearToDefaultNoArena(const std::string* default_value)
      PROTOBUF_ALWAYS_INLINE;

  void Destroy(const std::string* default_value,
               Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    DestroyNoArena(default_value);
  }
  void DestroyNoArena(const std::string* default_value) PROTOBUF_ALWAYS_INLINE;

  const std::string& Get() const PROTOBUF_ALWAYS_INLINE { return GetNoArena(); }
  const std::string& GetNoArena() const PROTOBUF_ALWAYS_INLINE;

  std::string* Mutable(const std::string* default_value,
                       Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    return MutableNoArena(default_value);
  }
  std::string* MutableNoArena(const std::string* default_value)
      PROTOBUF_ALWAYS_INLINE;

  std::string* Release(const std::string* default_value, Arena* /*arena*/) {
    return ReleaseNoArena(default_value);
  }
  std::string* ReleaseNonDefault(const std::string* default_value,
                                 Arena* /*arena*/) {
    return ReleaseNonDefaultNoArena(default_value);
  }
  std::string* ReleaseNoArena(const std::string* default_value) {
    return ReleaseNonDefaultNoArena(default_value);
  }
  std::string* ReleaseNonDefaultNoArena(const std::string* default_value);

  void Set(const std::string* default_value, StringPiece value,
           Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    SetNoArena(default_value, value);
  }
  void SetLite(const std::string* default_value, StringPiece value,
               Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    SetNoArena(default_value, value);
  }
  void SetNoArena(const std::string* default_value,
                  StringPiece value) PROTOBUF_ALWAYS_INLINE;

  void Set(const std::string* default_value, const std::string& value,
           Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    SetNoArena(default_value, value);
  }
  void SetLite(const std::string* default_value, const std::string& value,
               Arena* /*arena*/) PROTOBUF_ALWAYS_INLINE {
    SetNoArena(default_value, value);
  }
  void SetNoArena(const std::string* default_value,
                  const std::string& value) PROTOBUF_ALWAYS_INLINE;

  void SetNoArena(const std::string* default_value,
                  std::string&& value) PROTOBUF_ALWAYS_INLINE;
  void SetAllocated(const std::string* default_value, std::string* value,
                    Arena* /*arena*/) {
    SetAllocatedNoArena(default_value, value);
  }
  void SetAllocatedNoArena(const std::string* default_value,
                           std::string* value);
  void Swap(InlinedStringField* from) PROTOBUF_ALWAYS_INLINE;
  std::string* UnsafeMutablePointer();
  void UnsafeSetDefault(const std::string* default_value);
  std::string* UnsafeArenaRelease(const std::string* default_value,
                                  Arena* arena);
  void UnsafeArenaSetAllocated(const std::string* default_value,
                               std::string* value, Arena* arena);

  bool IsDefault(const std::string* /*default_value*/) { return false; }

 private:
  std::string value_;
};

inline InlinedStringField::InlinedStringField() {}

inline InlinedStringField::InlinedStringField(const std::string& default_value)
    : value_(default_value) {}

inline void InlinedStringField::AssignWithDefault(
    const std::string* /*default_value*/, const InlinedStringField& from) {
  value_ = from.value_;
}

inline const std::string& InlinedStringField::GetNoArena() const {
  return value_;
}

inline std::string* InlinedStringField::MutableNoArena(const std::string*) {
  return &value_;
}

inline void InlinedStringField::SetAllocatedNoArena(
    const std::string* default_value, std::string* value) {
  if (value == NULL) {
    value_.assign(*default_value);
  } else {
    value_.assign(std::move(*value));
    delete value;
  }
}

inline void InlinedStringField::DestroyNoArena(const std::string*) {
  // This is invoked from the generated message's ArenaDtor, which is used to
  // clean up objects not allocated on the Arena.
  this->~InlinedStringField();
}

inline void InlinedStringField::ClearNonDefaultToEmptyNoArena() {
  value_.clear();
}

inline void InlinedStringField::ClearToDefaultNoArena(
    const std::string* default_value) {
  value_.assign(*default_value);
}

inline std::string* InlinedStringField::ReleaseNonDefaultNoArena(
    const std::string* default_value) {
  std::string* released = new std::string(*default_value);
  value_.swap(*released);
  return released;
}

inline void InlinedStringField::SetNoArena(const std::string* /*default_value*/,
                                           StringPiece value) {
  value_.assign(value.data(), value.length());
}

inline void InlinedStringField::SetNoArena(const std::string* /*default_value*/,
                                           const std::string& value) {
  value_.assign(value);
}

inline void InlinedStringField::SetNoArena(const std::string* /*default_value*/,
                                           std::string&& value) {
  value_.assign(std::move(value));
}

inline void InlinedStringField::Swap(InlinedStringField* from) {
  value_.swap(from->value_);
}

inline std::string* InlinedStringField::UnsafeMutablePointer() {
  return &value_;
}

inline void InlinedStringField::UnsafeSetDefault(
    const std::string* default_value) {
  value_.assign(*default_value);
}

inline std::string* InlinedStringField::UnsafeArenaRelease(
    const std::string* default_value, Arena* /*arena*/) {
  return ReleaseNoArena(default_value);
}

inline void InlinedStringField::UnsafeArenaSetAllocated(
    const std::string* default_value, std::string* value, Arena* /*arena*/) {
  if (value == NULL) {
    value_.assign(*default_value);
  } else {
    value_.assign(*value);
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_INLINED_STRING_FIELD_H__
