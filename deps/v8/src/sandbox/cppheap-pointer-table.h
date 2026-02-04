// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CPPHEAP_POINTER_TABLE_H_
#define V8_SANDBOX_CPPHEAP_POINTER_TABLE_H_

#include "include/v8-sandbox.h"
#include "include/v8config.h"
#include "src/base/atomicops.h"
#include "src/base/bounds.h"
#include "src/base/memory.h"
#include "src/common/globals.h"
#include "src/sandbox/compactible-external-entity-table.h"
#include "src/sandbox/tagged-payload.h"
#include "src/utils/allocation.h"

#ifdef V8_COMPRESS_POINTERS

namespace v8::internal {

class Isolate;
class Counters;

/**
 * The entries of an CppHeapPointerTable.
 *
 * Each entry consists of a single pointer-sized word containing the pointer,
 * the marking bit, and a type tag. An entry can either be:
 *  - A "regular" entry, containing the pointer together with a type tag and
 *    the marking bit, or
 *  - A freelist entry, tagged with the kFreeEntryTag and containing the index
 *    of the next free entry, or
 *  - An evacuation entry, tagged with the kEvacuationEntryTag and containing
 *    the address of the CppHeapPointerSlot referencing the entry that will be
 *    evacuated into this entry.
 */
struct CppHeapPointerTableEntry {
  // Make this entry a cpp heap pointer entry containing the given pointer
  // tagged with the given tag.
  inline void MakePointerEntry(Address value, CppHeapPointerTag tag,
                               bool mark_as_alive);

  // Load and untag the pointer stored in this entry.
  // This entry must be a pointer entry.
  // If the tag of the entry is not within the specified tag range, the
  // resulting pointer will be invalid and cannot be dereferenced.
  inline Address GetPointer(CppHeapPointerTagRange tag_range) const;

  // Tag and store the given pointer in this entry.
  // This entry must be a pointer entry.
  inline void SetPointer(Address value, CppHeapPointerTag tag);

  // Returns true if this entry contains a pointer whose tag is within the
  // specified tag range.
  inline bool HasPointer(CppHeapPointerTagRange tag_range) const;

  // Invalidate the entry. Any access to a zapped entry will result in an
  // invalid pointer that will crash upon dereference.
  inline void MakeZappedEntry();

  // Make this entry a freelist entry, containing the index of the next entry
  // on the freelist.
  inline void MakeFreelistEntry(uint32_t next_entry_index);

  // Get the index of the next entry on the freelist. This method may be
  // called even when the entry is not a freelist entry. However, the result
  // is only valid if this is a freelist entry. This behaviour is required
  // for efficient entry allocation, see TryAllocateEntryFromFreelist.
  inline uint32_t GetNextFreelistEntryIndex() const;

  // Make this entry an evacuation entry containing the address of the handle to
  // the entry being evacuated.
  inline void MakeEvacuationEntry(Address handle_location);

  // Returns true if this entry contains an evacuation entry.
  inline bool HasEvacuationEntry() const;

  // Move the content of this entry into the provided entry, possibly clearing
  // the marking bit. Used during table compaction and during promotion.
  // Invalidates the source entry.
  inline void Evacuate(CppHeapPointerTableEntry& dest);

  // Mark this entry as alive during table garbage collection.
  inline void Mark();

  static constexpr bool IsWriteProtected = false;

 private:
  friend class CppHeapPointerTable;
  friend class CppHeapPointerTableEntryPrinter;

  struct CppHeapPointerTablePayloadTaggingScheme {
    using TagType = CppHeapPointerTag;
    static constexpr uint64_t kMarkBit = kCppHeapPointerMarkBit;
    static constexpr uint64_t kTagShift = kCppHeapPointerTagShift;
    static constexpr uint64_t kTagMask = 0xfffe;
    static constexpr uint64_t kPayloadShift = kCppHeapPointerPayloadShift;
    static constexpr uint64_t kPayloadMask = ~0ULL;
    static constexpr TagType kFreeEntryTag = CppHeapPointerTag::kFreeEntryTag;
    static constexpr TagType kEvacuationEntryTag =
        CppHeapPointerTag::kEvacuationEntryTag;
  };
  using Payload = TaggedPayload<CppHeapPointerTablePayloadTaggingScheme>;

  inline Payload GetRawPayload() const {
    return payload_.load(std::memory_order_relaxed);
  }
  inline void SetRawPayload(Payload new_payload) {
    return payload_.store(new_payload, std::memory_order_relaxed);
  }

  // CppHeapPointerTable entries consist of a single pointer-sized word
  // containing a tag and marking bit together with the actual content.
  std::atomic<Payload> payload_;
};

//  We expect CppHeapPointerTable entries to consist of a single 64-bit word.
static_assert(sizeof(CppHeapPointerTableEntry) == 8);

/**
 * A table storing pointers to objects in the CppHeap
 *
 * This table is essentially a specialized version of the ExternalPointerTable
 * used for CppHeap objects. It uses a different type tagging scheme which
 * supports significantly more types and also supports type hierarchies. See
 * the CppHeapPointerTag enum for more details.
 *
 * Apart from that, this table mostly behaves like the external pointer table
 * and so uses a simple garbage collection algorithm to detect and free unused
 * entries and also supports table compaction.
 *
 */
class V8_EXPORT_PRIVATE CppHeapPointerTable
    : public CompactibleExternalEntityTable<
          CppHeapPointerTableEntry, kCppHeapPointerTableReservationSize> {
  using Base =
      CompactibleExternalEntityTable<CppHeapPointerTableEntry,
                                     kCppHeapPointerTableReservationSize>;
  static_assert(kMaxCppHeapPointers == kMaxCapacity);

 public:
  CppHeapPointerTable() = default;
  CppHeapPointerTable(const CppHeapPointerTable&) = delete;
  CppHeapPointerTable& operator=(const CppHeapPointerTable&) = delete;

  using Space = Base::Space;

  // Retrieves the entry referenced by the given handle.
  //
  // The tag of the entry must be within the specified range of tags.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(CppHeapPointerHandle handle,
                     CppHeapPointerTagRange tag_range) const;

  // Sets the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(CppHeapPointerHandle handle, Address value,
                  CppHeapPointerTag tag);

  // Allocates a new entry in the given space. The caller must provide the
  // initial value and tag for the entry.
  //
  // This method is atomic and can be called from background threads.
  inline CppHeapPointerHandle AllocateAndInitializeEntry(Space* space,
                                                         Address initial_value,
                                                         CppHeapPointerTag tag);

  // Marks the specified entry as alive.
  //
  // If the space to which the entry belongs is currently being compacted, this
  // may also mark the entry for evacuation for which the location of the
  // handle is required. See the comments about the compaction algorithm for
  // more details.
  //
  // This method is atomic and can be called from background threads.
  inline void Mark(Space* space, CppHeapPointerHandle handle,
                   Address handle_location);

  uint32_t SweepAndCompact(Space* space, Counters* counters);

  inline bool Contains(Space* space, CppHeapPointerHandle handle) const;

 private:
  static inline bool IsValidHandle(CppHeapPointerHandle handle);
  static inline uint32_t HandleToIndex(CppHeapPointerHandle handle);
  static inline CppHeapPointerHandle IndexToHandle(uint32_t index);

  void ResolveEvacuationEntryDuringSweeping(
      uint32_t index, CppHeapPointerHandle* handle_location,
      uint32_t start_of_evacuation_area);
};

#ifdef OBJECT_PRINT

class CppHeapPointerTableEntryPrinter {
 public:
  static void PrintHeader(const char* space_name);
  static void PrintIfInUse(
      CppHeapPointerHandle handle, const CppHeapPointerTableEntry& entry,
      std::function<bool(CppHeapPointerTag)> entry_callback);
  static void PrintFooter();
};

template <>
class TableEntryPrinter<CppHeapPointerTableEntry>
    : public CppHeapPointerTableEntryPrinter {};

#endif  // OBJECT_PRINT

}  // namespace v8::internal

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_CPPHEAP_POINTER_TABLE_H_
