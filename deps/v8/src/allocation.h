// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ALLOCATION_H_
#define V8_ALLOCATION_H_

#include "include/v8-platform.h"
#include "src/base/compiler-specific.h"
#include "src/base/platform/platform.h"
#include "src/globals.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

// This file defines memory allocation functions. If a first attempt at an
// allocation fails, these functions call back into the embedder, then attempt
// the allocation a second time. The embedder callback must not reenter V8.

// Called when allocation routines fail to allocate, even with a possible retry.
// This function should not return, but should terminate the current processing.
V8_EXPORT_PRIVATE void FatalProcessOutOfMemory(const char* message);

// Superclass for classes managed with new & delete.
class V8_EXPORT_PRIVATE Malloced {
 public:
  void* operator new(size_t size) { return New(size); }
  void  operator delete(void* p) { Delete(p); }

  static void* New(size_t size);
  static void Delete(void* p);
};

template <typename T>
T* NewArray(size_t size) {
  T* result = new (std::nothrow) T[size];
  if (result == nullptr) {
    V8::GetCurrentPlatform()->OnCriticalMemoryPressure();
    result = new (std::nothrow) T[size];
    if (result == nullptr) FatalProcessOutOfMemory("NewArray");
  }
  return result;
}

template <typename T,
          typename = typename std::enable_if<IS_TRIVIALLY_COPYABLE(T)>::type>
T* NewArray(size_t size, T default_val) {
  T* result = reinterpret_cast<T*>(NewArray<uint8_t>(sizeof(T) * size));
  for (size_t i = 0; i < size; ++i) result[i] = default_val;
  return result;
}

template <typename T>
void DeleteArray(T* array) {
  delete[] array;
}


// The normal strdup functions use malloc.  These versions of StrDup
// and StrNDup uses new and calls the FatalProcessOutOfMemory handler
// if allocation fails.
V8_EXPORT_PRIVATE char* StrDup(const char* str);
char* StrNDup(const char* str, int n);


// Allocation policy for allocating in the C free store using malloc
// and free. Used as the default policy for lists.
class FreeStoreAllocationPolicy {
 public:
  INLINE(void* New(size_t size)) { return Malloced::New(size); }
  INLINE(static void Delete(void* p)) { Malloced::Delete(p); }
};


void* AlignedAlloc(size_t size, size_t alignment);
void AlignedFree(void *ptr);

// Allocates a single system memory page with read/write permissions. The
// address parameter is a hint. Returns the base address of the memory, or null
// on failure. Permissions can be changed on the base address.
byte* AllocateSystemPage(void* address, size_t* allocated);

// Represents and controls an area of reserved memory.
class V8_EXPORT_PRIVATE VirtualMemory {
 public:
  // Empty VirtualMemory object, controlling no reserved memory.
  VirtualMemory();

  // Reserves virtual memory containing an area of the given size that is
  // aligned per alignment. This may not be at the position returned by
  // address().
  VirtualMemory(size_t size, void* hint,
                size_t alignment = base::OS::AllocatePageSize());

  // Construct a virtual memory by assigning it some already mapped address
  // and size.
  VirtualMemory(void* address, size_t size) : address_(address), size_(size) {}

  // Releases the reserved memory, if any, controlled by this VirtualMemory
  // object.
  ~VirtualMemory();

  // Returns whether the memory has been reserved.
  bool IsReserved() const { return address_ != nullptr; }

  // Initialize or resets an embedded VirtualMemory object.
  void Reset();

  // Returns the start address of the reserved memory.
  // If the memory was reserved with an alignment, this address is not
  // necessarily aligned. The user might need to round it up to a multiple of
  // the alignment to get the start of the aligned block.
  void* address() const {
    DCHECK(IsReserved());
    return address_;
  }

  void* end() const {
    DCHECK(IsReserved());
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address_) +
                                   size_);
  }

  // Returns the size of the reserved memory. The returned value is only
  // meaningful when IsReserved() returns true.
  // If the memory was reserved with an alignment, this size may be larger
  // than the requested size.
  size_t size() const { return size_; }

  // Sets permissions according to the access argument. address and size must be
  // multiples of CommitPageSize(). Returns true on success, otherwise false.
  bool SetPermissions(void* address, size_t size,
                      base::OS::MemoryPermission access);

  // Releases memory after |free_start|. Returns the number of bytes released.
  size_t Release(void* free_start);

  // Frees all memory.
  void Free();

  // Assign control of the reserved region to a different VirtualMemory object.
  // The old object is no longer functional (IsReserved() returns false).
  void TakeControl(VirtualMemory* from);

  bool InVM(void* address, size_t size) {
    return (reinterpret_cast<uintptr_t>(address_) <=
            reinterpret_cast<uintptr_t>(address)) &&
           ((reinterpret_cast<uintptr_t>(address_) + size_) >=
            (reinterpret_cast<uintptr_t>(address) + size));
  }

 private:
  void* address_;  // Start address of the virtual memory.
  size_t size_;    // Size of the virtual memory.
};

bool AllocVirtualMemory(size_t size, void* hint, VirtualMemory* result);
bool AlignedAllocVirtualMemory(size_t size, size_t alignment, void* hint,
                               VirtualMemory* result);

}  // namespace internal
}  // namespace v8

#endif  // V8_ALLOCATION_H_
