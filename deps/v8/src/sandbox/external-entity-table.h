// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_ENTITY_TABLE_H_
#define V8_SANDBOX_EXTERNAL_ENTITY_TABLE_H_

#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

class Isolate;

/**
 * A thread-safe table with a fixed maximum size for storing references to
 * objects located outside of the sandbox.
 *
 * An external entity table provides the basic mechanisms to ensure
 * safe access to objects located outside the sandbox, but referenced
 * from within it. When an external entity table is used, objects located
 * inside the sandbox reference outside objects through indices into the table.
 *
 * The ExternalEntityTable class should be seen an an incomplete class that
 * needs to be extended by a concrete implementation class, such as the
 * ExternalPointerTable class, as it is lacking some functionality. In
 * particular, while the ExternalEntityTable implements the reserving, growing,
 * and shrinking of the backing memory as well as entry allocation routines, it
 * does not implement any logic for reclaiming entries such as garbage
 * collection. This must be done by the child classes.
 */
template <typename Entry, size_t size>
class V8_EXPORT_PRIVATE ExternalEntityTable {
 protected:
  static constexpr int kEntrySize = sizeof(Entry);
  static constexpr size_t kReservationSize = size;
  static constexpr size_t kMaxCapacity = kReservationSize / kEntrySize;

  ExternalEntityTable() = default;
  ExternalEntityTable(const ExternalEntityTable&) = delete;
  ExternalEntityTable& operator=(const ExternalEntityTable&) = delete;

  // Access the entry at the specified index.
  // The index must be less than the current capacity.
  Entry& at(uint32_t index);
  const Entry& at(uint32_t index) const;

  // Returns true if this table has been initialized.
  bool is_initialized() const;

  // Returns the current capacity of the table, expressed as number of entries.
  //
  // The capacity of the table may increase during entry allocation (if the
  // table is grown) and may decrease during sweeping (if blocks at the end are
  // free). As the former may happen concurrently, the capacity can only be
  // used reliably if either the table mutex is held or if all mutator threads
  // are currently stopped. However, it is fine to use this value to
  // sanity-check incoming ExternalPointerHandles in debug builds (there's no
  // need for actual bounds-checks because out-of-bounds accesses are guaranteed
  // to result in a harmless crash).
  uint32_t capacity() const;

  // Determines the number of entries currently on the freelist.
  // As table entries can be allocated from other threads, the freelist size
  // may have changed by the time this method returns. As such, the returned
  // value should only be treated as an approximation.
  uint32_t freelist_length() const;

  // Initializes the table by reserving the backing memory, allocating an
  // initial block, and populating the freelist.
  void InitializeTable();

  // Deallocates all memory associated with this table.
  void TearDownTable();

  // Allocates a new entry and return its index.
  //
  // If there are no free entries, then this will grow the table.
  // This method is atomic and can be called from background threads.
  uint32_t AllocateEntry();

  // Attempts to allocate an entry below the specified index.
  //
  // If there are no free entries at a lower index, this method will fail and
  // return zero. The threshold index must be at or below the current capacity.
  // This method will therefore never grow the table.
  // This method is atomic and can be called from background threads.
  uint32_t AllocateEntryBelow(uint32_t threshold_index);

  // Struct representing the head of the freelist of a table.
  //
  // An external entity table uses a simple, singly-linked list to manage free
  // entries. Each entry on the freelist contains the 32-bit index of the next
  // entry. The last entry points to zero.
  struct FreelistHead {
    constexpr FreelistHead() : next_(0), length_(0) {}
    constexpr FreelistHead(uint32_t next, uint32_t length)
        : next_(next), length_(length) {}

    // Returns the index of the next entry on the freelist.
    // If the freelist is empty, this returns zero.
    uint32_t next() const { return next_; }

    // Returns the total length of the freelist.
    uint32_t length() const { return length_; }

    bool is_empty() const {
      // It would be enough to just check that the size is zero. However, when
      // the size is zero, the next entry must also be zero, and checking that
      // both values are zero allows the compiler to insert a single 64-bit
      // comparison against zero.
      DCHECK_EQ(next_ == 0, length_ == 0);
      return next_ == 0 && length_ == 0;
    }

   private:
    uint32_t next_;
    uint32_t length_;
  };

  // Try to allocate the first entry of the freelist.
  //
  // This method is mostly a wrapper around an atomic compare-and-swap which
  // replaces the current freelist head with the next entry in the freelist,
  // thereby allocating the entry at the start of the freelist.
  bool TryAllocateEntryFromFreelist(FreelistHead freelist);

  // Extends the table and adds newly created entries to the freelist.
  // Returns the new freelist head.
  // When calling this method, mutex_ must be locked.
  // If the table cannot be grown, either because it is already at its maximum
  // size or because the memory for it could not be allocated, this method will
  // fail with an OOM crash.
  FreelistHead Grow();

  // Shrink the table to the new capacity.
  // The new capacity must be less than the current capacity and must be a
  // multiple of the block size. The now-unused blocks at the end of the table
  // are decommitted from memory. It is therefore guaranteed that they will be
  // inaccessible afterwards, and that they will be zero-initialized when they
  // are "brought back".
  void Shrink(uint32_t new_capacity);

  // Marker value for the freelist_head_ member to indicate that entry
  // allocation is currently forbidden, for example because the table is being
  // swept as part of a mark+sweep garbage collection. This value should never
  // occur as freelist_head_ value during normal operations and should be easy
  // to recognize.
  static constexpr FreelistHead kEntryAllocationIsForbiddenMarker =
      FreelistHead(-1, -1);

  // The table grows and shrinks in blocks of this size. This is also the
  // initial size of the table.
#if V8_TARGET_ARCH_PPC64
  // PPC64 uses 64KB pages, and this must be a multiple of the page size.
  static constexpr size_t kBlockSize = 64 * KB;
#else
  static constexpr size_t kBlockSize = 16 * KB;
#endif
  static constexpr size_t kEntriesPerBlock = kBlockSize / kEntrySize;

  // The buffer backing this table.
  // This is effectively const after initialization: the underlying buffer is
  // never reallocated, only grown/shrunk in place.
  Entry* buffer_ = nullptr;

  // Lock protecting the slow path for entry allocation, in particular Grow().
  // As the size of this class must be predictable (it is e.g. part of
  // IsolateData), it cannot directly contain a Mutex and so instead contains a
  // pointer to one.
  base::Mutex* mutex_ = nullptr;

  // The freelist used by this table.
  // This contains both the index of the first entry in the freelist and the
  // total length of the freelist as both values need to be updated together in
  // a single atomic operation to stay consistent in the case of concurrent
  // entry allocations.
  // We expect the FreelistHead struct to fit into a single atomic word.
  // Otherwise, access to it would be slow.
  static_assert(std::atomic<FreelistHead>::is_always_lock_free);
  std::atomic<FreelistHead> freelist_head_ = FreelistHead();

  // The current capacity of this table, as number of entries.
  std::atomic<uint32_t> capacity_{0};

  // An additional 32-bit atomic word that derived classes can use. For example,
  // the ExternalPointerTable uses this for the table compaction algorithm. This
  // is stored in this class so that std::is_standard_layout is true for derived
  // classes (for a class to have standard layout, only one class in the
  // inheritance hierarchy must have non-static data properties).
  std::atomic<uint32_t> extra_{0};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_ENTITY_TABLE_H_
