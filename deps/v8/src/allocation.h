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

class Isolate;

// This file defines memory allocation functions. If a first attempt at an
// allocation fails, these functions call back into the embedder, then attempt
// the allocation a second time. The embedder callback must not reenter V8.

// Called when allocation routines fail to allocate, even with a possible retry.
// This function should not return, but should terminate the current processing.
[[noreturn]] V8_EXPORT_PRIVATE void FatalProcessOutOfMemory(
    Isolate* isolate, const char* message);

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
    if (result == nullptr) FatalProcessOutOfMemory(nullptr, "NewArray");
  }
  return result;
}

template <typename T, typename = typename std::enable_if<
                          base::is_trivially_copyable<T>::value>::type>
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
  V8_INLINE void* New(size_t size) { return Malloced::New(size); }
  V8_INLINE static void Delete(void* p) { Malloced::Delete(p); }
};

// Performs a malloc, with retry logic on failure. Returns nullptr on failure.
// Call free to release memory allocated with this function.
void* AllocWithRetry(size_t size);

void* AlignedAlloc(size_t size, size_t alignment);
void AlignedFree(void *ptr);

// Gets the page granularity for AllocatePages and FreePages. Addresses returned
// by AllocatePages and AllocatePage are aligned to this size.
V8_EXPORT_PRIVATE size_t AllocatePageSize();

// Gets the granularity at which the permissions and release calls can be made.
V8_EXPORT_PRIVATE size_t CommitPageSize();

// Sets the random seed so that GetRandomMmapAddr() will generate repeatable
// sequences of random mmap addresses.
V8_EXPORT_PRIVATE void SetRandomMmapSeed(int64_t seed);

// Generate a random address to be used for hinting allocation calls.
V8_EXPORT_PRIVATE void* GetRandomMmapAddr();

// Allocates memory. Permissions are set according to the access argument.
// |address| is a hint. |size| and |alignment| must be multiples of
// AllocatePageSize(). Returns the address of the allocated memory, with the
// specified size and alignment, or nullptr on failure.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT void* AllocatePages(void* address, size_t size,
                                          size_t alignment,
                                          PageAllocator::Permission access);

// Frees memory allocated by a call to AllocatePages. |address| and |size| must
// be multiples of AllocatePageSize(). Returns true on success, otherwise false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool FreePages(void* address, const size_t size);

// Releases memory that is no longer needed. The range specified by |address|
// and |size| must be an allocated memory region. |size| and |new_size| must be
// multiples of CommitPageSize(). Memory from |new_size| to |size| is released.
// Released memory is left in an undefined state, so it should not be accessed.
// Returns true on success, otherwise false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool ReleasePages(void* address, size_t size,
                                        size_t new_size);

// Sets permissions according to |access|. |address| and |size| must be
// multiples of CommitPageSize(). Setting permission to kNoAccess may
// cause the memory contents to be lost. Returns true on success, otherwise
// false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool SetPermissions(void* address, size_t size,
                                          PageAllocator::Permission access);
inline bool SetPermissions(Address address, size_t size,
                           PageAllocator::Permission access) {
  return SetPermissions(reinterpret_cast<void*>(address), size, access);
}

// Convenience function that allocates a single system page with read and write
// permissions. |address| is a hint. Returns the base address of the memory and
// the page size via |allocated| on success. Returns nullptr on failure.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT byte* AllocatePage(void* address, size_t* allocated);

// Function that may release reserved memory regions to allow failed allocations
// to succeed. |length| is the amount of memory needed. Returns |true| if memory
// could be released, false otherwise.
V8_EXPORT_PRIVATE bool OnCriticalMemoryPressure(size_t length);

// Represents and controls an area of reserved memory.
class V8_EXPORT_PRIVATE VirtualMemory {
 public:
  // Empty VirtualMemory object, controlling no reserved memory.
  VirtualMemory();

  // Reserves virtual memory containing an area of the given size that is
  // aligned per alignment. This may not be at the position returned by
  // address().
  VirtualMemory(size_t size, void* hint, size_t alignment = AllocatePageSize());

  // Construct a virtual memory by assigning it some already mapped address
  // and size.
  VirtualMemory(Address address, size_t size)
      : address_(address), size_(size) {}

  // Releases the reserved memory, if any, controlled by this VirtualMemory
  // object.
  ~VirtualMemory();

  // Returns whether the memory has been reserved.
  bool IsReserved() const { return address_ != kNullAddress; }

  // Initialize or resets an embedded VirtualMemory object.
  void Reset();

  // Returns the start address of the reserved memory.
  // If the memory was reserved with an alignment, this address is not
  // necessarily aligned. The user might need to round it up to a multiple of
  // the alignment to get the start of the aligned block.
  Address address() const {
    DCHECK(IsReserved());
    return address_;
  }

  Address end() const {
    DCHECK(IsReserved());
    return address_ + size_;
  }

  // Returns the size of the reserved memory. The returned value is only
  // meaningful when IsReserved() returns true.
  // If the memory was reserved with an alignment, this size may be larger
  // than the requested size.
  size_t size() const { return size_; }

  // Sets permissions according to the access argument. address and size must be
  // multiples of CommitPageSize(). Returns true on success, otherwise false.
  bool SetPermissions(Address address, size_t size,
                      PageAllocator::Permission access);

  // Releases memory after |free_start|. Returns the number of bytes released.
  size_t Release(Address free_start);

  // Frees all memory.
  void Free();

  // Assign control of the reserved region to a different VirtualMemory object.
  // The old object is no longer functional (IsReserved() returns false).
  void TakeControl(VirtualMemory* from);

  bool InVM(Address address, size_t size) {
    return (address_ <= address) && ((address_ + size_) >= (address + size));
  }

 private:
  Address address_;  // Start address of the virtual memory.
  size_t size_;    // Size of the virtual memory.
};

bool AllocVirtualMemory(size_t size, void* hint, VirtualMemory* result);
bool AlignedAllocVirtualMemory(size_t size, size_t alignment, void* hint,
                               VirtualMemory* result);

}  // namespace internal
}  // namespace v8

#endif  // V8_ALLOCATION_H_
