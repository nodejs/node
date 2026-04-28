// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SANDBOX_MALLOC_H_
#define V8_SANDBOX_SANDBOX_MALLOC_H_

#include "src/base/numerics/checked_math.h"
#include "src/init/isolate-group.h"

namespace v8 {
namespace internal {

// General-purpose memory allocator for in-sandbox memory.
//
// If the sandbox is not enabled, this will simply use base::Malloc.
//
// NOTE: This currently uses V8's own array buffer allocator (which is mainly
// used by d8). However, that is a fairly inefficient allocator. In the future
// it would be nice to have a "real" allocator for this purpose instead.
//
// NOTE: this allocator must only be used for UNTRUSTED memory as an attacker
// can corrupt the contents of the returned buffers arbitrarily. As such, it is
// for example generally unsafe to store any pointers in the returned memory.

template <typename T>
T* SandboxAlloc() {
  // It is generally unsafe to allocate non-trivial objects inside the sandbox,
  // in particular anything containing pointers. The static_assert is a
  // best-effort attempt to catch such cases, but it doesn't work for all
  // unsafe cases, for example raw pointer fields or Address values.
  // TODO(427464384): this is a little too strict, e.g. for std::atomic.
  static_assert(std::is_trivial<T>::value,
                "Must only allocate trivial C++ types inside the sandbox");

  size_t size = sizeof(T);

#ifdef V8_ENABLE_SANDBOX
  auto allocator =
      IsolateGroup::GetDefault()->GetSandboxedArrayBufferAllocator();
  void* raw_memory = allocator->Allocate(size);
#else
  void* raw_memory = base::Malloc(size);
#endif  // V8_ENABLE_SANDBOX

  memset(raw_memory, 0, size);
  return static_cast<T*>(raw_memory);
}

template <typename T>
T* SandboxAllocArray(size_t num_elements) {
  // It is generally unsafe to allocate non-trivial objects inside the sandbox,
  // in particular anything containing pointers. The static_assert is a
  // best-effort attempt to catch such cases, but it doesn't work for all
  // unsafe cases, for example raw pointer fields or Address values.
  // TODO(427464384): this is a little too strict, e.g. for std::atomic.
  static_assert(std::is_trivial<T>::value,
                "Must only allocate trivial C++ types inside the sandbox");

  size_t size = base::CheckMul(num_elements, sizeof(T)).ValueOrDie();

#ifdef V8_ENABLE_SANDBOX
  auto allocator =
      IsolateGroup::GetDefault()->GetSandboxedArrayBufferAllocator();
  void* raw_memory = allocator->Allocate(size);
#else
  void* raw_memory = base::Malloc(size);
#endif  // V8_ENABLE_SANDBOX

  memset(raw_memory, 0, size);
  return static_cast<T*>(raw_memory);
}

inline void SandboxFree(void* ptr) {
#ifdef V8_ENABLE_SANDBOX
  auto allocator =
      IsolateGroup::GetDefault()->GetSandboxedArrayBufferAllocator();
  allocator->Free(ptr);
#else
  return base::Free(ptr);
#endif  // V8_ENABLE_SANDBOX
}

// Deleter struct for use with std::unique_ptr:
//
// std::unique_ptr<int, SandboxFreeDeleter> foo_ptr(
//     static_cast<int*>(SandboxAlloc(sizeof(int))));
struct SandboxFreeDeleter {
  inline void operator()(void* p) const { SandboxFree(p); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_SANDBOX_MALLOC_H_
