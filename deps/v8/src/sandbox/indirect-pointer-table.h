// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_INDIRECT_POINTER_TABLE_H_
#define V8_SANDBOX_INDIRECT_POINTER_TABLE_H_

#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/memory.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/sandbox/external-entity-table.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8 {
namespace internal {

class Isolate;
class Counters;

/**
 * The entries of an IndirectPointerTable.
 *
 * Each entry contains an (absolute) pointer to a HeapObject.
 */
struct IndirectPointerTableEntry {
  // Make this entry a "regular" entry, containing an absolute pointer to a
  // HeapObject.
  inline void MakeIndirectPointerEntry(Address value);

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
  friend class IndirectPointerTable;

  // Freelist entries contain the index of the next free entry in their lower 32
  // bits and this tag in the upper 32 bits.
  static constexpr Address kFreeEntryTag = 0xffffffffULL << 32;

  // The marking bit is stored in the content_ field, see below.
  static constexpr Address kMarkingBit = 1;

  // The pointer to the HeapObject stored in this entry. Also contains the
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

static_assert(sizeof(IndirectPointerTableEntry) ==
              kIndirectPointerTableEntrySize);

/**
 * A table containing (full) pointers to HeapObjects.
 *
 * When the sandbox is enabled, an indirect pointer table (IPT) is used to
 * safely reference HeapObjects located outside of the sandbox via indirect
 * pointers (indices into this table). In particular, it is used to reference
 * objects in one of the trusted heap spaces. The IPT then guarantees that
 * every access to an object via an indirect pointer either results in an
 * invalid pointer or a valid pointer to a valid (live) HeapObject of the
 * expected instance type.
 *
 * So in short, the IPT provides a mechanism for memory-safe access to
 * HeapObjects located outside of the sandbox.
 *
 * The IPT is very similar to the external pointer table (EPT), but is used to
 * reference V8 HeapObjects (located inside a V8 heap) rather than C++ objects
 * (typically located on one of the system heaps). As such, the garbage
 * collector needs to be aware of the IPT mechanism and must be able to mark
 * objects referenced through indirect pointers.
 */
class V8_EXPORT_PRIVATE IndirectPointerTable
    : public ExternalEntityTable<IndirectPointerTableEntry,
                                 kIndirectPointerTableReservationSize> {
 public:
  // Size of a IndirectPointerTable, for layout computation in IsolateData.
  static int constexpr kSize = 2 * kSystemPointerSize;
  static_assert(kMaxIndirectPointers == kMaxCapacity);

  IndirectPointerTable() = default;
  IndirectPointerTable(const IndirectPointerTable&) = delete;
  IndirectPointerTable& operator=(const IndirectPointerTable&) = delete;

  // The Spaces used by a IndirectPointerTable.
  using Space =
      ExternalEntityTable<IndirectPointerTableEntry,
                          kIndirectPointerTableReservationSize>::Space;

  // Retrieves the content of the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(IndirectPointerHandle handle) const;

  // Sets the content of the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(IndirectPointerHandle handle, Address value);

  // Allocates a new entry in the table and initialize it.
  //
  // This method is atomic and can be called from background threads.
  inline IndirectPointerHandle AllocateAndInitializeEntry(Space* space,
                                                          Address value);

  // Marks the specified entry as alive.
  //
  // This method is atomic and can be called from background threads.
  inline void Mark(Space* space, IndirectPointerHandle handle);

  // Frees all unmarked entries in the given space.
  //
  // This method must only be called while mutator threads are stopped as it is
  // not safe to allocate table entries while a space is being swept.
  //
  // Returns the number of live entries after sweeping.
  uint32_t Sweep(Space* space, Counters* counters);

  // The base address of this table, for use in JIT compilers.
  Address base_address() const { return base(); }

 private:
  inline uint32_t HandleToIndex(IndirectPointerHandle handle) const;
  inline IndirectPointerHandle IndexToHandle(uint32_t index) const;
};

static_assert(sizeof(IndirectPointerTable) == IndirectPointerTable::kSize);

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_INDIRECT_POINTER_TABLE_H_
