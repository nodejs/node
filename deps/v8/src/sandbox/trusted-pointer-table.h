// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRUSTED_POINTER_TABLE_H_
#define V8_SANDBOX_TRUSTED_POINTER_TABLE_H_

#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/sandbox/external-entity-table.h"
#include "src/sandbox/indirect-pointer-tag.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8 {
namespace internal {

class Isolate;
class Counters;

/**
 * The entries of a TrustedPointerTable.
 *
 * Each entry contains an (absolute) pointer to a TrustedObject.
 */
struct TrustedPointerTableEntry {
  // Make this entry a "regular" entry, containing an absolute pointer to a
  // TrustedObject.
  inline void MakeTrustedPointerEntry(Address value, bool mark_as_alive);

  // Make this entry a freelist entry, containing the index of the next entry
  // on the freelist.
  inline void MakeFreelistEntry(uint32_t next_entry_index);

  // Retrieve the pointer stored in this entry.
  // This entry must not be a freelist entry.
  inline Address GetContent() const;

  // Store the given pointer in this entry.
  // This entry must not be a freelist entry.
  inline void SetContent(Address value);

  // Returns true if this entry is a freelist entry.
  inline bool IsFreelistEntry() const;

  // Get the index of the next entry on the freelist. This method may be
  // called even when the entry is not a freelist entry. However, the result
  // is only valid if this is a freelist entry. This behaviour is required
  // for efficient entry allocation, see TryAllocateEntryFromFreelist.
  inline uint32_t GetNextFreelistEntryIndex() const;

  // Mark this entry as alive during garbage collection.
  inline void Mark();

  // Unmark this entry during sweeping.
  inline void Unmark();

  // Test whether this entry is currently marked as alive.
  inline bool IsMarked() const;

 private:
  friend class TrustedPointerTable;

  // Freelist entries contain the index of the next free entry in their lower 32
  // bits and this tag in the upper 32 bits.
  static constexpr Address kFreeEntryTag = 0xffffffffULL << 32;

  // The marking bit is stored in the content_ field, see below.
  static constexpr Address kMarkingBit = 1;

  // The pointer to the TrustedObject stored in this entry. Also contains the
  // marking bit: since this is a tagged pointer to a V8 HeapObject, we know
  // that it will be 4-byte aligned and that the LSB should always be set. We
  // therefore use the LSB as marking bit. In this way:
  //  - When loading the pointer, we only need to perform an unconditional OR 1
  //    to get the correctly tagged pointer
  //  - When storing the pointer we don't need to do anything since the tagged
  //    pointer will automatically be marked
  static_assert(kMarkingBit == kHeapObjectTag);
  std::atomic<Address> content_;
};

static_assert(sizeof(TrustedPointerTableEntry) ==
              kTrustedPointerTableEntrySize);

/**
 * A table containing (full) pointers to TrustedObjects.
 *
 * When the sandbox is enabled, a trusted pointer table (TPT) is used to safely
 * reference trusted heap objects located in one of the trusted spaces outside
 * of the sandbox. The TPT guarantees that every access to an object via a
 * trusted pointer (an index into the table) either results in an invalid
 * pointer or a valid pointer to a valid (live) object of the expected type.
 *
 * The TPT is very similar to the external pointer table (EPT), but is used to
 * reference V8 HeapObjects (located inside a V8 heap) rather than C++ objects
 * (typically located on one of the system heaps). As such, the garbage
 * collector needs to be aware of the table indirection.
 */
class V8_EXPORT_PRIVATE TrustedPointerTable
    : public ExternalEntityTable<TrustedPointerTableEntry,
                                 kTrustedPointerTableReservationSize> {
 public:
  // Size of a TrustedPointerTable, for layout computation in IsolateData.
  static int constexpr kSize = 2 * kSystemPointerSize;
  static_assert(kMaxTrustedPointers == kMaxCapacity);

  TrustedPointerTable() = default;
  TrustedPointerTable(const TrustedPointerTable&) = delete;
  TrustedPointerTable& operator=(const TrustedPointerTable&) = delete;

  // The Spaces used by a TrustedPointerTable.
  using Space = ExternalEntityTable<
      TrustedPointerTableEntry,
      kTrustedPointerTableReservationSize>::SpaceWithBlackAllocationSupport;

  // Retrieves the content of the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(TrustedPointerHandle handle) const;

  // Sets the content of the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(TrustedPointerHandle handle, Address pointer,
                  IndirectPointerTag tag);

  // Allocates a new entry in the table and initialize it.
  //
  // This method is atomic and can be called from background threads.
  inline TrustedPointerHandle AllocateAndInitializeEntry(
      Space* space, Address pointer, IndirectPointerTag tag);

  // Marks the specified entry as alive.
  //
  // This method is atomic and can be called from background threads.
  inline void Mark(Space* space, TrustedPointerHandle handle);

  // Frees all unmarked entries in the given space.
  //
  // This method must only be called while mutator threads are stopped as it is
  // not safe to allocate table entries while a space is being swept.
  //
  // Returns the number of live entries after sweeping.
  uint32_t Sweep(Space* space, Counters* counters);

  // Iterate over all active entries in the given space.
  //
  // The callback function will be invoked once for every entry that is
  // currently in use, i.e. has been allocated and not yet freed, and will
  // receive the handle and content of that entry.
  template <typename Callback>
  void IterateActiveEntriesIn(Space* space, Callback callback);

  // The base address of this table, for use in JIT compilers.
  Address base_address() const { return base(); }

 private:
  inline uint32_t HandleToIndex(TrustedPointerHandle handle) const;
  inline TrustedPointerHandle IndexToHandle(uint32_t index) const;

  // Ensure that the value is valid before storing it into this table.
  inline void Validate(Address pointer, IndirectPointerTag tag);
};

static_assert(sizeof(TrustedPointerTable) == TrustedPointerTable::kSize);

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_SANDBOX

#endif  // V8_SANDBOX_TRUSTED_POINTER_TABLE_H_
