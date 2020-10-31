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
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This file contains miscellaneous helper code used by generated code --
// including lite types -- but which should not be used directly by users.

#ifndef GOOGLE_PROTOBUF_GENERATED_MESSAGE_UTIL_H__
#define GOOGLE_PROTOBUF_GENERATED_MESSAGE_UTIL_H__

#include <assert.h>
#include <atomic>
#include <climits>
#include <string>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/has_bits.h>
#include <google/protobuf/implicit_weak_message.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/stubs/once.h>  // Add direct dep on port for pb.cc
#include <google/protobuf/port.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/casts.h>

#include <google/protobuf/port_def.inc>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {

class Arena;

namespace io {
class CodedInputStream;
}

namespace internal {

template <typename To, typename From>
inline To DownCast(From* f) {
  return PROTOBUF_NAMESPACE_ID::internal::down_cast<To>(f);
}
template <typename To, typename From>
inline To DownCast(From& f) {
  return PROTOBUF_NAMESPACE_ID::internal::down_cast<To>(f);
}


PROTOBUF_EXPORT void InitProtobufDefaults();

// This used by proto1
PROTOBUF_EXPORT inline const std::string& GetEmptyString() {
  InitProtobufDefaults();
  return GetEmptyStringAlreadyInited();
}


// True if IsInitialized() is true for all elements of t.  Type is expected
// to be a RepeatedPtrField<some message type>.  It's useful to have this
// helper here to keep the protobuf compiler from ever having to emit loops in
// IsInitialized() methods.  We want the C++ compiler to inline this or not
// as it sees fit.
template <class Type>
bool AllAreInitialized(const Type& t) {
  for (int i = t.size(); --i >= 0;) {
    if (!t.Get(i).IsInitialized()) return false;
  }
  return true;
}

// "Weak" variant of AllAreInitialized, used to implement implicit weak fields.
// This version operates on MessageLite to avoid introducing a dependency on the
// concrete message type.
template <class T>
bool AllAreInitializedWeak(const RepeatedPtrField<T>& t) {
  for (int i = t.size(); --i >= 0;) {
    if (!reinterpret_cast<const RepeatedPtrFieldBase&>(t)
             .Get<ImplicitWeakTypeHandler<T> >(i)
             .IsInitialized()) {
      return false;
    }
  }
  return true;
}

inline bool IsPresent(const void* base, uint32 hasbit) {
  const uint32* has_bits_array = static_cast<const uint32*>(base);
  return (has_bits_array[hasbit / 32] & (1u << (hasbit & 31))) != 0;
}

inline bool IsOneofPresent(const void* base, uint32 offset, uint32 tag) {
  const uint32* oneof =
      reinterpret_cast<const uint32*>(static_cast<const uint8*>(base) + offset);
  return *oneof == tag >> 3;
}

typedef void (*SpecialSerializer)(const uint8* base, uint32 offset, uint32 tag,
                                  uint32 has_offset,
                                  io::CodedOutputStream* output);

PROTOBUF_EXPORT void ExtensionSerializer(const uint8* base, uint32 offset,
                                         uint32 tag, uint32 has_offset,
                                         io::CodedOutputStream* output);
PROTOBUF_EXPORT void UnknownFieldSerializerLite(const uint8* base,
                                                uint32 offset, uint32 tag,
                                                uint32 has_offset,
                                                io::CodedOutputStream* output);

PROTOBUF_EXPORT MessageLite* DuplicateIfNonNullInternal(MessageLite* message);
PROTOBUF_EXPORT MessageLite* GetOwnedMessageInternal(Arena* message_arena,
                                                     MessageLite* submessage,
                                                     Arena* submessage_arena);
PROTOBUF_EXPORT void GenericSwap(MessageLite* m1, MessageLite* m2);

template <typename T>
T* DuplicateIfNonNull(T* message) {
  // The casts must be reinterpret_cast<> because T might be a forward-declared
  // type that the compiler doesn't know is related to MessageLite.
  return reinterpret_cast<T*>(
      DuplicateIfNonNullInternal(reinterpret_cast<MessageLite*>(message)));
}

template <typename T>
T* GetOwnedMessage(Arena* message_arena, T* submessage,
                   Arena* submessage_arena) {
  // The casts must be reinterpret_cast<> because T might be a forward-declared
  // type that the compiler doesn't know is related to MessageLite.
  return reinterpret_cast<T*>(GetOwnedMessageInternal(
      message_arena, reinterpret_cast<MessageLite*>(submessage),
      submessage_arena));
}

// Hide atomic from the public header and allow easy change to regular int
// on platforms where the atomic might have a perf impact.
class PROTOBUF_EXPORT CachedSize {
 public:
  int Get() const { return size_.load(std::memory_order_relaxed); }
  void Set(int size) { size_.store(size, std::memory_order_relaxed); }

 private:
  std::atomic<int> size_{0};
};

// SCCInfo represents information of a strongly connected component of
// mutual dependent messages.
struct PROTOBUF_EXPORT SCCInfoBase {
  // We use 0 for the Initialized state, because test eax,eax, jnz is smaller
  // and is subject to macro fusion.
  enum {
    kInitialized = 0,  // final state
    kRunning = 1,
    kUninitialized = -1,  // initial state
  };
#if defined(_MSC_VER) && !defined(__clang__)
  // MSVC doesnt make std::atomic constant initialized. This union trick
  // makes it so.
  union {
    int visit_status_to_make_linker_init;
    std::atomic<int> visit_status;
  };
#else
  std::atomic<int> visit_status;
#endif
  int num_deps;
  void (*init_func)();
  // This is followed by an array  of num_deps
  // const SCCInfoBase* deps[];
};

template <int N>
struct SCCInfo {
  SCCInfoBase base;
  // Semantically this is const SCCInfo<T>* which is is a templated type.
  // The obvious inheriting from SCCInfoBase mucks with struct initialization.
  // Attempts showed the compiler was generating dynamic initialization code.
  // Zero length arrays produce warnings with MSVC.
  SCCInfoBase* deps[N ? N : 1];
};

PROTOBUF_EXPORT void InitSCCImpl(SCCInfoBase* scc);

inline void InitSCC(SCCInfoBase* scc) {
  auto status = scc->visit_status.load(std::memory_order_acquire);
  if (PROTOBUF_PREDICT_FALSE(status != SCCInfoBase::kInitialized))
    InitSCCImpl(scc);
}

PROTOBUF_EXPORT void DestroyMessage(const void* message);
PROTOBUF_EXPORT void DestroyString(const void* s);
// Destroy (not delete) the message
inline void OnShutdownDestroyMessage(const void* ptr) {
  OnShutdownRun(DestroyMessage, ptr);
}
// Destroy the string (call std::string destructor)
inline void OnShutdownDestroyString(const std::string* ptr) {
  OnShutdownRun(DestroyString, ptr);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_GENERATED_MESSAGE_UTIL_H__
