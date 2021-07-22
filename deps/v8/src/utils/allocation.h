// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_ALLOCATION_H_
#define V8_UTILS_ALLOCATION_H_

#include "include/v8-platform.h"
#include "src/base/address-region.h"
#include "src/base/compiler-specific.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/init/v8.h"

namespace v8 {

namespace base {
class BoundedPageAllocator;
}  // namespace base

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
  static void* operator new(size_t size);
  static void operator delete(void* p);
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

template <typename T>
struct ArrayDeleter {
  void operator()(T* array) { DeleteArray(array); }
};

template <typename T>
using ArrayUniquePtr = std::unique_ptr<T, ArrayDeleter<T>>;

// The normal strdup functions use malloc.  These versions of StrDup
// and StrNDup uses new and calls the FatalProcessOutOfMemory handler
// if allocation fails.
V8_EXPORT_PRIVATE char* StrDup(const char* str);
char* StrNDup(const char* str, int n);

// Allocation policy for allocating in the C free store using malloc
// and free. Used as the default policy for lists.
class FreeStoreAllocationPolicy {
 public:
  template <typename T, typename TypeTag = T[]>
  V8_INLINE T* NewArray(size_t length) {
    return static_cast<T*>(Malloced::operator new(length * sizeof(T)));
  }
  template <typename T, typename TypeTag = T[]>
  V8_INLINE void DeleteArray(T* p, size_t length) {
    Malloced::operator delete(p);
  }
};

// Performs a malloc, with retry logic on failure. Returns nullptr on failure.
// Call free to release memory allocated with this function.
void* AllocWithRetry(size_t size);

V8_EXPORT_PRIVATE void* AlignedAlloc(size_t size, size_t alignment);
V8_EXPORT_PRIVATE void AlignedFree(void* ptr);

// Returns platfrom page allocator instance. Guaranteed to be a valid pointer.
V8_EXPORT_PRIVATE v8::PageAllocator* GetPlatformPageAllocator();

// Sets the given page allocator as the platform page allocator and returns
// the current one. This function *must* be used only for testing purposes.
// It is not thread-safe and the testing infrastructure should ensure that
// the tests do not modify the value simultaneously.
V8_EXPORT_PRIVATE v8::PageAllocator* SetPlatformPageAllocatorForTesting(
    v8::PageAllocator* page_allocator);

// Gets the page granularity for AllocatePages and FreePages. Addresses returned
// by AllocatePages are aligned to this size.
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
V8_WARN_UNUSED_RESULT void* AllocatePages(v8::PageAllocator* page_allocator,
                                          void* address, size_t size,
                                          size_t alignment,
                                          PageAllocator::Permission access);

// Frees memory allocated by a call to AllocatePages. |address| and |size| must
// be multiples of AllocatePageSize(). Returns true on success, otherwise false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool FreePages(v8::PageAllocator* page_allocator,
                                     void* address, const size_t size);

// Releases memory that is no longer needed. The range specified by |address|
// and |size| must be an allocated memory region. |size| and |new_size| must be
// multiples of CommitPageSize(). Memory from |new_size| to |size| is released.
// Released memory is left in an undefined state, so it should not be accessed.
// Returns true on success, otherwise false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool ReleasePages(v8::PageAllocator* page_allocator,
                                        void* address, size_t size,
                                        size_t new_size);

// Sets permissions according to |access|. |address| and |size| must be
// multiples of CommitPageSize(). Setting permission to kNoAccess may
// cause the memory contents to be lost. Returns true on success, otherwise
// false.
V8_EXPORT_PRIVATE
V8_WARN_UNUSED_RESULT bool SetPermissions(v8::PageAllocator* page_allocator,
                                          void* address, size_t size,
                                          PageAllocator::Permission access);
inline bool SetPermissions(v8::PageAllocator* page_allocator, Address address,
                           size_t size, PageAllocator::Permission access) {
  return SetPermissions(page_allocator, reinterpret_cast<void*>(address), size,
                        access);
}

// Function that may release reserved memory regions to allow failed allocations
// to succeed. |length| is the amount of memory needed. Returns |true| if memory
// could be released, false otherwise.
V8_EXPORT_PRIVATE bool OnCriticalMemoryPressure(size_t length);

// Represents and controls an area of reserved memory.
class VirtualMemory final {
 public:
  enum JitPermission { kNoJit, kMapAsJittable };

  // Empty VirtualMemory object, controlling no reserved memory.
  V8_EXPORT_PRIVATE VirtualMemory();

  VirtualMemory(const VirtualMemory&) = delete;
  VirtualMemory& operator=(const VirtualMemory&) = delete;

  // Reserves virtual memory containing an area of the given size that is
  // aligned per |alignment| rounded up to the |page_allocator|'s allocate page
  // size. The |size| must be aligned with |page_allocator|'s commit page size.
  // This may not be at the position returned by address().
  V8_EXPORT_PRIVATE VirtualMemory(v8::PageAllocator* page_allocator,
                                  size_t size, void* hint, size_t alignment = 1,
                                  JitPermission jit = kNoJit);

  // Construct a virtual memory by assigning it some already mapped address
  // and size.
  VirtualMemory(v8::PageAllocator* page_allocator, Address address, size_t size)
      : page_allocator_(page_allocator), region_(address, size) {
    DCHECK_NOT_NULL(page_allocator);
    DCHECK(IsAligned(address, page_allocator->AllocatePageSize()));
    DCHECK(IsAligned(size, page_allocator->CommitPageSize()));
  }

  // Releases the reserved memory, if any, controlled by this VirtualMemory
  // object.
  V8_EXPORT_PRIVATE ~VirtualMemory();

  // Move constructor.
  VirtualMemory(VirtualMemory&& other) V8_NOEXCEPT { *this = std::move(other); }

  // Move assignment operator.
  VirtualMemory& operator=(VirtualMemory&& other) V8_NOEXCEPT {
    DCHECK(!IsReserved());
    page_allocator_ = other.page_allocator_;
    region_ = other.region_;
    other.Reset();
    return *this;
  }

  // Returns whether the memory has been reserved.
  bool IsReserved() const { return region_.begin() != kNullAddress; }

  // Initialize or resets an embedded VirtualMemory object.
  V8_EXPORT_PRIVATE void Reset();

  v8::PageAllocator* page_allocator() { return page_allocator_; }

  const base::AddressRegion& region() const { return region_; }

  // Returns the start address of the reserved memory.
  // If the memory was reserved with an alignment, this address is not
  // necessarily aligned. The user might need to round it up to a multiple of
  // the alignment to get the start of the aligned block.
  Address address() const {
    DCHECK(IsReserved());
    return region_.begin();
  }

  Address end() const {
    DCHECK(IsReserved());
    return region_.end();
  }

  // Returns the size of the reserved memory. The returned value is only
  // meaningful when IsReserved() returns true.
  // If the memory was reserved with an alignment, this size may be larger
  // than the requested size.
  size_t size() const { return region_.size(); }

  // Sets permissions according to the access argument. address and size must be
  // multiples of CommitPageSize(). Returns true on success, otherwise false.
  V8_EXPORT_PRIVATE bool SetPermissions(Address address, size_t size,
                                        PageAllocator::Permission access);

  // Releases memory after |free_start|. Returns the number of bytes released.
  V8_EXPORT_PRIVATE size_t Release(Address free_start);

  // Frees all memory.
  V8_EXPORT_PRIVATE void Free();

  // As with Free but does not write to the VirtualMemory object itself so it
  // can be called on a VirtualMemory that is itself not writable.
  V8_EXPORT_PRIVATE void FreeReadOnly();

  bool InVM(Address address, size_t size) const {
    return region_.contains(address, size);
  }

 private:
  // Page allocator that controls the virtual memory.
  v8::PageAllocator* page_allocator_ = nullptr;
  base::AddressRegion region_;
};

// Represents a VirtualMemory reservation along with a BoundedPageAllocator that
// can be used to allocate within the reservation.
//
// Virtual memory cages are used for both the pointer compression cage and code
// ranges (on platforms that require code ranges) and are configurable via
// ReservationParams.
//
// +------------+-----------+-----------  ~~~  -+
// |     ...    |    ...    |   ...             |
// +------------+-----------+------------ ~~~  -+
// ^            ^           ^
// start        cage base   allocatable base
//
// <------------>           <------------------->
// base bias size              allocatable size
// <-------------------------------------------->
//             reservation size
//
// - The reservation is made using ReservationParams::page_allocator.
// - start is the start of the virtual memory reservation.
// - cage base is the base address of the cage.
// - allocatable base is the cage base rounded up to the nearest
//   ReservationParams::page_size, and is the start of the allocatable area for
//   the BoundedPageAllocator.
//
// - The base bias is configured by ReservationParams::base_bias_size.
// - The reservation size is configured by ReservationParams::reservation_size.
//
// Additionally,
// - The alignment of the cage base is configured by
//   ReservationParams::base_alignment.
// - The page size of the BoundedPageAllocator is configured by
//   ReservationParams::page_size.
// - A hint for the value of start can be passed by
//   ReservationParams::requested_start_hint.
//
// The configuration is subject to the following alignment requirements.
// Below, AllocatePageSize is short for
// ReservationParams::page_allocator->AllocatePageSize().
//
// - The reservation size must be AllocatePageSize-aligned.
// - If the base alignment is not kAnyBaseAlignment, both the base alignment
//   and the base bias size must be AllocatePageSize-aligned.
// - The base alignment may be kAnyBaseAlignment to denote any alignment is
//   acceptable. In this case the base bias size does not need to be aligned.
class VirtualMemoryCage {
 public:
  VirtualMemoryCage();
  virtual ~VirtualMemoryCage();

  VirtualMemoryCage(const VirtualMemoryCage&) = delete;
  VirtualMemoryCage& operator=(VirtualMemoryCage&) = delete;

  VirtualMemoryCage(VirtualMemoryCage&& other) V8_NOEXCEPT;
  VirtualMemoryCage& operator=(VirtualMemoryCage&& other) V8_NOEXCEPT;

  Address base() const { return base_; }

  base::BoundedPageAllocator* page_allocator() const {
    return page_allocator_.get();
  }

  VirtualMemory* reservation() { return &reservation_; }
  const VirtualMemory* reservation() const { return &reservation_; }

  bool IsReserved() const {
    DCHECK_EQ(base_ != kNullAddress, reservation_.IsReserved());
    return reservation_.IsReserved();
  }

  struct ReservationParams {
    // The allocator to use to reserve the virtual memory.
    v8::PageAllocator* page_allocator;
    // See diagram above.
    size_t reservation_size;
    size_t base_alignment;
    size_t base_bias_size;
    size_t page_size;
    Address requested_start_hint;

    static constexpr size_t kAnyBaseAlignment = 1;
  };

  // A number of attempts is made to try to reserve a region that satisfies the
  // constraints in params, but this may fail. The base address may be different
  // than the one requested.
  bool InitReservation(const ReservationParams& params);

  void Free();

 protected:
  Address base_ = kNullAddress;
  std::unique_ptr<base::BoundedPageAllocator> page_allocator_;
  VirtualMemory reservation_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_ALLOCATION_H_
