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

#ifndef GOOGLE_PROTOBUF_METADATA_LITE_H__
#define GOOGLE_PROTOBUF_METADATA_LITE_H__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/port.h>

#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
namespace internal {

// This is the representation for messages that support arena allocation. It
// uses a tagged pointer to either store the Arena pointer, if there are no
// unknown fields, or a pointer to a block of memory with both the Arena pointer
// and the UnknownFieldSet, if there are unknown fields. This optimization
// allows for "zero-overhead" storage of the Arena pointer, relative to the
// above baseline implementation.
//
// The tagged pointer uses the LSB to disambiguate cases, and uses bit 0 == 0 to
// indicate an arena pointer and bit 0 == 1 to indicate a UFS+Arena-container
// pointer.
template <class T, class Derived>
class InternalMetadataWithArenaBase {
 public:
  InternalMetadataWithArenaBase() : ptr_(NULL) {}
  explicit InternalMetadataWithArenaBase(Arena* arena) : ptr_(arena) {}

  ~InternalMetadataWithArenaBase() {
    if (have_unknown_fields() && arena() == NULL) {
      delete PtrValue<Container>();
    }
    ptr_ = NULL;
  }

  PROTOBUF_ALWAYS_INLINE const T& unknown_fields() const {
    if (PROTOBUF_PREDICT_FALSE(have_unknown_fields())) {
      return PtrValue<Container>()->unknown_fields;
    } else {
      return Derived::default_instance();
    }
  }

  PROTOBUF_ALWAYS_INLINE T* mutable_unknown_fields() {
    if (PROTOBUF_PREDICT_TRUE(have_unknown_fields())) {
      return &PtrValue<Container>()->unknown_fields;
    } else {
      return mutable_unknown_fields_slow();
    }
  }

  PROTOBUF_ALWAYS_INLINE Arena* arena() const {
    if (PROTOBUF_PREDICT_FALSE(have_unknown_fields())) {
      return PtrValue<Container>()->arena;
    } else {
      return PtrValue<Arena>();
    }
  }

  PROTOBUF_ALWAYS_INLINE bool have_unknown_fields() const {
    return PtrTag() == kTagContainer;
  }

  PROTOBUF_ALWAYS_INLINE void Swap(Derived* other) {
    // Semantics here are that we swap only the unknown fields, not the arena
    // pointer. We cannot simply swap ptr_ with other->ptr_ because we need to
    // maintain our own arena ptr. Also, our ptr_ and other's ptr_ may be in
    // different states (direct arena pointer vs. container with UFS) so we
    // cannot simply swap ptr_ and then restore the arena pointers. We reuse
    // UFS's swap implementation instead.
    if (have_unknown_fields() || other->have_unknown_fields()) {
      static_cast<Derived*>(this)->DoSwap(other->mutable_unknown_fields());
    }
  }

  PROTOBUF_ALWAYS_INLINE void MergeFrom(const Derived& other) {
    if (other.have_unknown_fields()) {
      static_cast<Derived*>(this)->DoMergeFrom(other.unknown_fields());
    }
  }

  PROTOBUF_ALWAYS_INLINE void Clear() {
    if (have_unknown_fields()) {
      static_cast<Derived*>(this)->DoClear();
    }
  }

  PROTOBUF_ALWAYS_INLINE void* raw_arena_ptr() const { return ptr_; }

 private:
  void* ptr_;

  // Tagged pointer implementation.
  enum {
    // ptr_ is an Arena*.
    kTagArena = 0,
    // ptr_ is a Container*.
    kTagContainer = 1,
  };
  static const intptr_t kPtrTagMask = 1;
  static const intptr_t kPtrValueMask = ~kPtrTagMask;

  // Accessors for pointer tag and pointer value.
  PROTOBUF_ALWAYS_INLINE int PtrTag() const {
    return reinterpret_cast<intptr_t>(ptr_) & kPtrTagMask;
  }

  template <typename U>
  U* PtrValue() const {
    return reinterpret_cast<U*>(reinterpret_cast<intptr_t>(ptr_) &
                                kPtrValueMask);
  }

  // If ptr_'s tag is kTagContainer, it points to an instance of this struct.
  struct Container {
    T unknown_fields;
    Arena* arena;
  };

  PROTOBUF_NOINLINE T* mutable_unknown_fields_slow() {
    Arena* my_arena = arena();
    Container* container = Arena::Create<Container>(my_arena);
    // Two-step assignment works around a bug in clang's static analyzer:
    // https://bugs.llvm.org/show_bug.cgi?id=34198.
    ptr_ = container;
    ptr_ = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(ptr_) |
                                   kTagContainer);
    container->arena = my_arena;
    return &(container->unknown_fields);
  }
};

// We store unknown fields as a std::string right now, because there is
// currently no good interface for reading unknown fields into an ArenaString.
// We may want to revisit this to allow unknown fields to be parsed onto the
// Arena.
class InternalMetadataWithArenaLite
    : public InternalMetadataWithArenaBase<std::string,
                                           InternalMetadataWithArenaLite> {
 public:
  InternalMetadataWithArenaLite() {}

  explicit InternalMetadataWithArenaLite(Arena* arena)
      : InternalMetadataWithArenaBase<std::string,
                                      InternalMetadataWithArenaLite>(arena) {}

  void DoSwap(std::string* other) { mutable_unknown_fields()->swap(*other); }

  void DoMergeFrom(const std::string& other) {
    mutable_unknown_fields()->append(other);
  }

  void DoClear() { mutable_unknown_fields()->clear(); }

  static const std::string& default_instance() {
    // Can't use GetEmptyStringAlreadyInited() here because empty string
    // may not have been initalized yet. This happens when protocol compiler
    // statically determines the user can't access defaults and omits init code
    // from proto constructors. However unknown fields are always part of a
    // proto so it needs to be lazily initailzed. See b/112613846.
    return GetEmptyString();
  }
};

// This helper RAII class is needed to efficiently parse unknown fields. We
// should only call mutable_unknown_fields if there are actual unknown fields.
// The obvious thing to just use a stack string and swap it at the end of
// the parse won't work, because the destructor of StringOutputStream needs to
// be called before we can modify the string (it check-fails). Using
// LiteUnknownFieldSetter setter(&_internal_metadata_);
// StringOutputStream stream(setter.buffer());
// guarantees that the string is only swapped after stream is destroyed.
class PROTOBUF_EXPORT LiteUnknownFieldSetter {
 public:
  explicit LiteUnknownFieldSetter(InternalMetadataWithArenaLite* metadata)
      : metadata_(metadata) {
    if (metadata->have_unknown_fields()) {
      buffer_.swap(*metadata->mutable_unknown_fields());
    }
  }
  ~LiteUnknownFieldSetter() {
    if (!buffer_.empty()) metadata_->mutable_unknown_fields()->swap(buffer_);
  }
  std::string* buffer() { return &buffer_; }

 private:
  InternalMetadataWithArenaLite* metadata_;
  std::string buffer_;
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_METADATA_LITE_H__
