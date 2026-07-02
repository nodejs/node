#ifndef SRC_NODE_V8_SANDBOX_H_
#define SRC_NODE_V8_SANDBOX_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

#include <cstring>
#include <memory>

namespace node {

#ifdef V8_ENABLE_SANDBOX

// When V8_ENABLE_SANDBOX is enabled, all ArrayBuffer backing stores must live
// inside the V8 memory cage. External pointers cannot be wrapped directly via
// ArrayBuffer::NewBackingStore(data, ...). These helpers centralise the
// allocate-in-cage / free-in-cage pattern so that call sites don't need to
// repeat #ifdef guards.

// Allocate memory inside the V8 sandbox cage.
// When zero_fill is true the memory is zeroed (Allocate); otherwise it is
// left uninitialised (AllocateUninitialized).
inline void* SandboxAllocate(size_t size, bool zero_fill = true) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  if (zero_fill) {
    return allocator->Allocate(size);
  }
  return allocator->AllocateUninitialized(size);
}

// Free memory that was allocated with SandboxAllocate.
inline void SandboxFree(void* data, size_t size) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  allocator->Free(data, size);
}

// Reallocate memory inside the V8 sandbox cage. Copies min(old, new) bytes
// from the old buffer and zero-fills any additional bytes. Frees the old
// buffer on success. Returns nullptr on allocation failure (old buffer is
// NOT freed in that case).
inline void* SandboxReallocate(void* data,
                               size_t old_length,
                               size_t new_length) {
  if (old_length == new_length) return data;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  void* new_data = allocator->AllocateUninitialized(new_length);
  if (new_data == nullptr) return nullptr;
  size_t bytes_to_copy = std::min(old_length, new_length);
  memcpy(new_data, data, bytes_to_copy);
  if (new_length > bytes_to_copy) {
    memset(static_cast<uint8_t*>(new_data) + bytes_to_copy,
           0,
           new_length - bytes_to_copy);
  }
  allocator->Free(data, old_length);
  return new_data;
}

// BackingStore deleter that frees cage-allocated memory.
inline void SandboxBackingStoreDeleter(void* data, size_t length, void*) {
  SandboxFree(data, length);
}

// Allocate cage memory, copy |source| into it, and wrap in a BackingStore.
// The caller is responsible for freeing the original |source| memory.
inline std::unique_ptr<v8::BackingStore> CopyCageBackingStore(
    const void* source, size_t size) {
  // No need to zero-fill since we overwrite immediately.
  void* cage_data = SandboxAllocate(size, /* zero_fill */ false);
  if (cage_data == nullptr) return nullptr;
  memcpy(cage_data, source, size);
  return v8::ArrayBuffer::NewBackingStore(
      cage_data, size, SandboxBackingStoreDeleter, nullptr);
}

#else  // !V8_ENABLE_SANDBOX

// When the sandbox is disabled these are thin wrappers around malloc/free.

inline void* SandboxAllocate(size_t size, bool zero_fill = true) {
  if (zero_fill) {
    return calloc(1, size);
  }
  return malloc(size);
}

inline void SandboxFree(void* data, size_t size) {
  free(data);
}

inline void* SandboxReallocate(void* data,
                               size_t old_length,
                               size_t new_length) {
  return realloc(data, new_length);
}

inline void SandboxBackingStoreDeleter(void* data, size_t length, void*) {
  free(data);
}

inline std::unique_ptr<v8::BackingStore> CopyCageBackingStore(
    const void* source, size_t size) {
  void* copy = malloc(size);
  if (copy == nullptr) return nullptr;
  memcpy(copy, source, size);
  return v8::ArrayBuffer::NewBackingStore(
      copy, size, SandboxBackingStoreDeleter, nullptr);
}

#endif  // V8_ENABLE_SANDBOX

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_SANDBOX_H_
