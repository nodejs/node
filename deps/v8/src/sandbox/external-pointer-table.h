// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_
#define V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_

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
 * A table storing pointers to objects outside the V8 heap.
 *
 * When V8_ENABLE_SANDBOX, its primary use is for pointing to objects outside
 * the sandbox, as described below.
 *
 * An external pointer table provides the basic mechanisms to ensure
 * memory-safe access to objects located outside the sandbox, but referenced
 * from within it. When an external pointer table is used, objects located
 * inside the sandbox reference outside objects through indices into the table.
 *
 * Type safety can be ensured by using type-specific tags for the external
 * pointers. These tags will be ORed into the unused top bits of the pointer
 * when storing them and will be ANDed away when loading the pointer later
 * again. If a pointer of the wrong type is accessed, some of the top bits will
 * remain in place, rendering the pointer inaccessible.
 *
 * Temporal memory safety is achieved through garbage collection of the table,
 * which ensures that every entry is either an invalid pointer or a valid
 * pointer pointing to a live object.
 *
 * Spatial memory safety can, if necessary, be ensured by storing the size of a
 * referenced object together with the object itself outside the sandbox, and
 * referencing both through a single entry in the table.
 *
 * The garbage collection algorithm for the table works as follows:
 *  - The top bit of every entry is reserved for the marking bit.
 *  - Every store to an entry automatically sets the marking bit when ORing
 *    with the tag. This avoids the need for write barriers.
 *  - Every load of an entry automatically removes the marking bit when ANDing
 *    with the inverted tag.
 *  - When the GC marking visitor finds a live object with an external pointer,
 *    it marks the corresponding entry as alive through Mark(), which sets the
 *    marking bit using an atomic CAS operation.
 *  - When marking is finished, SweepAndCompact() iterates of the table once
 *    while the mutator is stopped and builds a freelist from all dead entries
 *    while also removing the marking bit from any live entry.
 *
 * The freelist is a singly-linked list, using the lower 32 bits of each entry
 * to store the index of the next free entry. When the freelist is empty and a
 * new entry is allocated, the table grows in place and the freelist is
 * re-populated from the newly added entries.
 *
 * When V8_COMPRESS_POINTERS, external pointer tables are also used to ease
 * alignment requirements in heap object fields via indirection.
 */
class V8_EXPORT_PRIVATE ExternalPointerTable {
 public:
  // Size of an ExternalPointerTable, for layout computation in IsolateData.
  // Asserted to be equal to the actual size in external-pointer-table.cc.
  static int constexpr kSize = 4 * kSystemPointerSize;

  ExternalPointerTable() = default;

  ExternalPointerTable(const ExternalPointerTable&) = delete;
  ExternalPointerTable& operator=(const ExternalPointerTable&) = delete;

  // Initializes this external pointer table by reserving the backing memory
  // and initializing the freelist.
  void Init(Isolate* isolate);

  // Resets this external pointer table and deletes all associated memory.
  void TearDown();

  // Retrieves the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline Address Get(ExternalPointerHandle handle,
                     ExternalPointerTag tag) const;

  // Sets the entry referenced by the given handle.
  //
  // This method is atomic and can be called from background threads.
  inline void Set(ExternalPointerHandle handle, Address value,
                  ExternalPointerTag tag);

  // Exchanges the entry referenced by the given handle with the given value,
  // returning the previous value. The same tag is applied both to decode the
  // previous value and encode the given value.
  //
  // This method is atomic and can be called from background threads.
  inline Address Exchange(ExternalPointerHandle handle, Address value,
                          ExternalPointerTag tag);

  // Allocates a new entry in the external pointer table. The caller must
  // provide the initial value and tag.
  //
  // This method is atomic and can be called from background threads.
  inline ExternalPointerHandle AllocateAndInitializeEntry(
      Isolate* isolate, Address initial_value, ExternalPointerTag tag);

  // Determines the number of entries currently on the freelist.
  // The freelist entries encode the freelist size and the next entry on the
  // list, so this routine fetches the first entry on the freelist and returns
  // the size encoded in it.
  // As table entries can be allocated from other threads, the freelist size
  // may have changed by the time this method returns. As such, the returned
  // value should only be treated as an approximation.
  inline uint32_t FreelistSize();

  // Marks the specified entry as alive.
  //
  // If the table is currently being compacted, this may also mark the entry
  // for compaction for which the location of the handle is required. See the
  // comments about table compaction below for more details.
  //
  // This method is atomic and can be called from background threads.
  inline void Mark(ExternalPointerHandle handle, Address handle_location);

  // Frees unmarked entries and finishes table compaction (if running).
  //
  // This method must only be called while mutator threads are stopped as it is
  // not safe to allocate table entries while the table is being swept.
  //
  // Returns the number of live entries after sweeping.
  uint32_t SweepAndCompact(Isolate* isolate);

  // Table compaction.
  //
  // The table is to some degree self-compacting: since the freelist is
  // sorted in ascending order (see SweepAndCompact()), empty slots at the start
  // of the table will usually quickly be filled. Further, empty blocks at the
  // end of the table will be decommitted to reduce memory usage. However, live
  // entries at the end of the table can prevent this decommitting and cause
  // fragmentation. The following simple algorithm is therefore used to
  // compact the table if that is deemed necessary:
  //  - At the start of the GC marking phase, determine if the table needs to be
  //    compacted. This decisiont is mostly based on the absolute and relative
  //    size of the freelist.
  //  - If compaction is needed, this algorithm attempts to shrink the table by
  //    FreelistSize/2 entries during compaction by moving all live entries out
  //    of the evacuation area (the last FreelistSize/2 entries of the table),
  //    then decommitting those blocks at the end of SweepAndCompact.
  //  - During marking, whenever a live entry inside the evacuation area is
  //    found, a new "evacuation entry" is allocated from the freelist (which is
  //    assumed to have enough free slots) and the address of the handle is
  //    written into it.
  //  - During sweeping, these evacuation entries are resolved: the content of
  //    the old entry is copied into the new entry and the handle in the object
  //    is updated to point to the new entry.
  //
  // When compacting, it is expected that the evacuation area contains few live
  // entries and that the freelist will be able to serve all evacuation entry
  // allocations. In that case, compaction is essentially free (very little
  // marking overhead, no memory overhead). However, it can happen that the
  // application allocates a large number of entries from the table during
  // marking, in which case the freelist would no longer be able to serve all
  // allocation without growing. If that situation is detected, compaction is
  // aborted during marking.
  //
  // This algorithm assumes that table entries (except for the null entry) are
  // never shared between multiple objects. Otherwise, the following could
  // happen: object A initially has handle H1 and is scanned during incremental
  // marking. Next, object B with handle H2 is scanned and marked for
  // evacuation. Afterwards, object A copies the handle H2 from object B.
  // During sweeping, only object B's handle will be updated to point to the
  // new entry while object A's handle is now dangling. If shared entries ever
  // become necessary, setting external pointer handles would have to be
  // guarded by write barriers to avoid this scenario.

  // Use heuristics to determine if table compaction is needed and if so start
  // the compaction. This is expected to be called at the start of the GC
  // marking phase.
  void StartCompactingIfNeeded();

  // Returns true if table compaction is currently running.
  inline bool IsCompacting();

  // Returns true if table compaction was aborted during the GC marking phase.
  inline bool CompactingWasAbortedDuringMarking();

 private:
  // Required for Isolate::CheckIsolateLayout().
  friend class Isolate;

  //
  // ExternalPointerTable constants.
  //
  // An external pointer table grows and shrinks in blocks of this size. This
  // is also the initial size of the table.
#if V8_TARGET_ARCH_PPC64
  // PPC64 uses 64KB pages, and this must be a multiple of the page size.
  static constexpr size_t kBlockSize = 64 * KB;
#else
  static constexpr size_t kBlockSize = 16 * KB;
#endif
  static constexpr size_t kEntriesPerBlock = kBlockSize / kSystemPointerSize;

  // When the table is swept, it first sets the freelist_ to this special value
  // to better catch any violation of the "don't-alloc-while-sweeping"
  // requirement (see SweepAndCompact()). This value should never occur as
  // freelist_ value during normal operations and should be easy to recognize.
  static constexpr uint64_t kTableIsCurrentlySweepingMarker =
      static_cast<uint64_t>(-1);

  // This value is used for start_of_evacuation_area to indicate that the table
  // is not currently being compacted. It is set to uint32_t max so that
  // determining whether an entry should be evacuated becomes a single
  // comparison: `bool should_be_evacuated = index >= start_of_evacuation_area`.
  static constexpr uint32_t kNotCompactingMarker =
      std::numeric_limits<uint32_t>::max();

  // This value may be ORed into the start_of_evacuation_area value during the
  // GC marking phase to indicate that table compaction has been aborted
  // because the freelist grew to short. This will prevent further evacuation
  // attempts as `should_be_evacuated` (see above) will always be false.
  static constexpr uint32_t kCompactionAbortedMarker = 0xf0000000;

  // In debug builds during GC marking, this value is ORed into
  // ExternalPointerHandles whose entries are marked for evacuation. During
  // sweeping, the Handles for evacuated entries are checked to have this
  // marker value. This allows detecting re-initialized entries, which are
  // problematic for table compaction. This is only possible for entries marked
  // for evacuation as the location of the Handle is only known for those.
  static constexpr uint32_t kVisitedHandleMarker = 0x1;
  static_assert(kExternalPointerIndexShift >= 1);

  //
  // Internal methods.
  //
  // Returns true if this external pointer table has been initialized.
  bool is_initialized() { return buffer_ != kNullAddress; }

  // Table capacity accesors.
  // The capacity is expressed in number of entries.
  //
  // The capacity of the table may increase during entry allocation (if the
  // table is grown) and may decrease during sweeping (if blocks at the end are
  // free). As the former may happen concurrently, the capacity can only be
  // used reliably if either the table mutex is held or if all mutator threads
  // are currently stopped. However, it is fine to use this value to
  // sanity-check incoming ExternalPointerHandles in debug builds (there's no
  // need for actual bounds-checks because out-of-bounds accesses are guaranteed
  // to result in a harmless crash).
  uint32_t capacity() const { return base::Relaxed_Load(&capacity_); }
  void set_capacity(uint32_t new_capacity) {
    base::Relaxed_Store(&capacity_, new_capacity);
  }

  // Start of evacuation area accessors.
  uint32_t start_of_evacuation_area() const {
    return base::Relaxed_Load(&start_of_evacuation_area_);
  }
  void set_start_of_evacuation_area(uint32_t value) {
    base::Relaxed_Store(&start_of_evacuation_area_, value);
  }

  // Struct to represent the freelist of a table.
  // In it's encoded form, this is stored in the freelist_ member of the table.
  class Freelist {
   public:
    Freelist() : encoded_(0) {}
    Freelist(uint32_t head, uint32_t size)
        : encoded_((static_cast<uint64_t>(size) << 32) | head) {}

    uint32_t Head() const { return static_cast<uint32_t>(encoded_); }
    uint32_t Size() const { return static_cast<uint32_t>(encoded_ >> 32); }

    bool IsEmpty() const {
      DCHECK_EQ(Head() == 0, Size() == 0);
      return encoded_ == 0;
    }

    uint64_t Encode() const { return encoded_; }

    static Freelist Decode(uint64_t encoded_form) {
      DCHECK_NE(encoded_form, kTableIsCurrentlySweepingMarker);
      return Freelist(encoded_form);
    }

   private:
    explicit Freelist(uint64_t encoded_form) : encoded_(encoded_form) {}

    uint64_t encoded_;
  };

  // Freelist accessors.
  Freelist Relaxed_GetFreelist() {
    return Freelist::Decode(base::Relaxed_Load(&freelist_));
  }
  Freelist Acquire_GetFreelist() {
    return Freelist::Decode(base::Acquire_Load(&freelist_));
  }
  void Relaxed_SetFreelist(Freelist new_freelist) {
    base::Relaxed_Store(&freelist_, new_freelist.Encode());
  }
  void Release_SetFreelist(Freelist new_freelist) {
    base::Release_Store(&freelist_, new_freelist.Encode());
  }
  bool Relaxed_CompareAndSwapFreelist(Freelist old_freelist,
                                      Freelist new_freelist) {
    uint64_t old_val = base::Relaxed_CompareAndSwap(
        &freelist_, old_freelist.Encode(), new_freelist.Encode());
    return old_val == old_freelist.Encode();
  }

  // Allocate an entry suitable as evacuation entry during table compaction.
  //
  // This method will always return an entry before the start of the evacuation
  // area or fail by returning kNullExternalPointerHandle. It expects the
  // current start of the evacuation area to be passed as parameter (instead of
  // loading it from memory) as that value may be modified by another marking
  // thread when compaction is aborted. See the explanation of the compaction
  // algorithm for more details.
  //
  // The caller is responsible for initializing the entry.
  //
  // This method is atomic and can be called from background threads.
  inline ExternalPointerHandle AllocateEvacuationEntry(
      uint32_t start_of_evacuation_area);

  // Try to allocate the entry at the start of the freelist.
  //
  // This method is mostly a wrapper around an atomic compare-and-swap which
  // replaces the current freelist_head with the next entry in the freelist,
  // thereby allocating the entry at the start of the freelist.
  inline bool TryAllocateEntryFromFreelist(Freelist freelist);

  // Extends the table and adds newly created entries to the freelist. Returns
  // the new freelist.
  // When calling this method, mutex_ must be locked.
  // If the table cannot be grown, either because it is already at its maximum
  // size or because the memory for it could not be allocated, this method will
  // fail with an OOM crash.
  Freelist Grow(Isolate* isolate);

  // Stop compacting at the end of sweeping.
  void StopCompacting();

  // Outcome of external pointer table compaction to use for the
  // ExternalPointerTableCompactionOutcome histogram.
  enum class TableCompactionOutcome {
    // Table compaction was successful.
    kSuccess = 0,
    // Table compaction was partially successful: marking finished successfully,
    // but not all blocks that we wanted to free could be freed because some new
    // entries had already been allocated in them again.
    kPartialSuccess = 1,
    // Table compaction was aborted during marking because the freelist grew to
    // short.
    kAbortedDuringMarking = 2,
  };

  // Handle <-> Table index conversion.
  inline uint32_t HandleToIndex(ExternalPointerHandle handle) const {
    uint32_t index = handle >> kExternalPointerIndexShift;
    DCHECK_EQ(handle & ~kVisitedHandleMarker,
              index << kExternalPointerIndexShift);
    DCHECK_LT(index, capacity());
    return index;
  }

  inline ExternalPointerHandle IndexToHandle(uint32_t index) const {
    ExternalPointerHandle handle = index << kExternalPointerIndexShift;
    DCHECK_EQ(index, handle >> kExternalPointerIndexShift);
    return handle;
  }

#ifdef DEBUG
  inline bool HandleWasVisitedDuringMarking(ExternalPointerHandle handle) {
    return (handle & kVisitedHandleMarker) == kVisitedHandleMarker;
  }
#endif  // DEBUG

  //
  // Entries of an ExternalPointerTable.
  //
  class Entry {
   public:
    // Construct a null entry.
    Entry() : value_(kNullAddress) {}

    // Returns the payload of this entry. If the provided tag does not match
    // the tag of the entry, the returned pointer cannot be dereferenced as
    // some of its top bits will be set.
    Address Untag(ExternalPointerTag tag) { return value_ & ~tag; }

    // Return the payload of this entry without performing a tag check. The
    // caller must ensure that the pointer is not used in an unsafe way.
    Address UncheckedUntag() { return value_ & ~kExternalPointerTagMask; }

    // Returns true if this entry is tagged with the given tag.
    bool HasTag(ExternalPointerTag tag) const {
      return (value_ & kExternalPointerTagMask) == tag;
    }

    // Check, set, and clear the marking bit of this entry.
    bool IsMarked() const { return (value_ & kExternalPointerMarkBit) != 0; }
    void SetMarkBit() { value_ |= kExternalPointerMarkBit; }
    void ClearMarkBit() { value_ &= ~kExternalPointerMarkBit; }

    // Returns true if this entry is part of the freelist.
    bool IsFreelistEntry() const {
      return HasTag(kExternalPointerFreeEntryTag);
    }

    // Returns true if this is a evacuation entry, in which case
    // ExtractHandleLocation may be used.
    bool IsEvacuationEntry() const {
      return HasTag(kExternalPointerEvacuationEntryTag);
    }

    // Returns true if this is neither a freelist- nor an evacuation entry.
    bool IsRegularEntry() const {
      return !IsFreelistEntry() && !IsEvacuationEntry();
    }

    // Extract the index of the next entry on the freelist. This method may be
    // called even when the entry is not a freelist entry. However, the result
    // is only valid if this is a freelist entry. This behaviour is required
    // for efficient entry allocation, see TryAllocateEntryFromFreelist.
    uint32_t ExtractNextFreelistEntry() const {
      return static_cast<uint32_t>(value_);
    }

    // An evacuation entry contains the address of the Handle to a (regular)
    // entry that is to be evacuated (into this entry). This method extracts
    // that address. Must only be called if this is an evacuation entry.
    Address ExtractHandleLocation() {
      DCHECK(IsEvacuationEntry());
      return Untag(kExternalPointerEvacuationEntryTag);
    }

    // Constructs an entry containing all zeroes.
    static Entry MakeNullEntry() { return Entry(kNullAddress); }

    // Constructs a regular entry by tagging the pointer with the tag.
    static Entry MakeRegularEntry(Address value, ExternalPointerTag tag) {
      DCHECK_NE(tag, kExternalPointerFreeEntryTag);
      DCHECK_NE(tag, kExternalPointerEvacuationEntryTag);
      return Entry(value | tag);
    }

    // Constructs a freelist entry given the current freelist head and size.
    static Entry MakeFreelistEntry(uint32_t next_freelist_entry) {
      // The next freelist entry is stored in the lower bits of the entry.
      static_assert(kMaxExternalPointers < (1ULL << kExternalPointerTagShift));
      DCHECK_LT(next_freelist_entry, kMaxExternalPointers);
      return Entry(next_freelist_entry | kExternalPointerFreeEntryTag);
    }

    // Constructs an evacuation entry containing the given handle location.
    static Entry MakeEvacuationEntry(Address handle_location) {
      return Entry(handle_location | kExternalPointerEvacuationEntryTag);
    }

    // Encodes this entry into a pointer-sized word for storing it in the
    // external pointer table.
    Address Encode() const { return value_; }

    // Decodes a previously encoded entry.
    static Entry Decode(Address value) { return Entry(value); }

    bool operator==(Entry other) const { return value_ == other.value_; }
    bool operator!=(Entry other) const { return value_ != other.value_; }

   private:
    explicit Entry(Address value) : value_(value) {}

    // The raw content of an entry. The top bits will contain the tag and
    // marking bit, the lower bits contain the pointer payload. Refer to the
    // ExternalPointerTag and kExternalPointerMarkBit definitions to see which
    // bits are used for what purpose.
    Address value_;
  };

  //
  // Low-level entry accessors.
  //
  // Computes the address of the specified entry.
  inline Address entry_address(uint32_t index) const {
    return buffer_ + index * sizeof(Address);
  }

  // When LeakSanitizer is enabled, this method will write the untagged (raw)
  // pointer into the shadow table (located after the real table) at the given
  // index. This is necessary because LSan is unable to scan the pointers in
  // the main table due to the pointer tagging scheme (the values don't "look
  // like" pointers). So instead it can scan the pointers in the shadow table.
  // This only works because the payload part of an external pointer is only
  // modified on one thread (but e.g. the marking bit may be modified from
  // background threads). Otherwise this would always be racy as the Store
  // methods below are no longer atomic.
  inline void RecordEntryForLSan(uint32_t index, Entry entry) {
#if defined(LEAK_SANITIZER)
    auto addr = entry_address(index) + kExternalPointerTableReservationSize;
    base::Memory<Address>(addr) = entry.UncheckedUntag();
#endif  // LEAK_SANITIZER
  }

  // Loads the entry at the given index. This method is non-atomic, only use it
  // when no other threads can currently access the table.
  Entry Load(uint32_t index) const {
    auto raw_value = base::Memory<Address>(entry_address(index));
    return Entry::Decode(raw_value);
  }

  // Stores the provided entry at the given index. This method is non-atomic,
  // only use it when no other threads can currently access the table.
  void Store(uint32_t index, Entry entry) {
    RecordEntryForLSan(index, entry);
    base::Memory<Address>(entry_address(index)) = entry.Encode();
  }

  // Atomically loads the entry at the given index.
  Entry RelaxedLoad(uint32_t index) const {
    auto addr = reinterpret_cast<base::Atomic64*>(entry_address(index));
    auto raw_value = base::Relaxed_Load(addr);
    return Entry::Decode(raw_value);
  }

  // Atomically stores the provided entry at the given index.
  void RelaxedStore(uint32_t index, Entry entry) {
    RecordEntryForLSan(index, entry);
    auto addr = reinterpret_cast<base::Atomic64*>(entry_address(index));
    base::Relaxed_Store(addr, entry.Encode());
  }

  Entry RelaxedCompareAndSwap(uint32_t index, Entry old_entry,
                              Entry new_entry) {
    // This is not calling RecordEntryForLSan as that would be racy. This is ok
    // however because this this method is only used to set the marking bit.
    auto addr = reinterpret_cast<base::Atomic64*>(entry_address(index));
    auto raw_value = base::Relaxed_CompareAndSwap(addr, old_entry.Encode(),
                                                  new_entry.Encode());
    return Entry::Decode(raw_value);
  }

  // Atomically exchanges the entry at the given index with the provided entry.
  Entry RelaxedExchange(uint32_t index, Entry entry) {
    RecordEntryForLSan(index, entry);
    auto addr = reinterpret_cast<base::Atomic64*>(entry_address(index));
    auto raw_value = base::Relaxed_AtomicExchange(addr, entry.Encode());
    return Entry::Decode(raw_value);
  }

  //
  // ExternalPointerTable fields.
  //
  // The buffer backing this table. Essentially an array of Entry instances.
  // This is const after initialization. Should only be accessed using the
  // Load() and Store() methods, which take care of atomicicy if necessary.
  Address buffer_ = kNullAddress;

  // The current capacity of this table, which is the number of usable entries.
  base::Atomic32 capacity_ = 0;

  // When compacting the table, this value contains the index of the first
  // entry in the evacuation area. The evacuation area is the region at the end
  // of the table from which entries are moved out of so that the underyling
  // memory pages can be freed after sweeping.
  // This field can have the following values:
  // - kNotCompactingMarker: compaction is not currently running.
  // - A kEntriesPerBlock aligned value within (0, capacity): table compaction
  //   is running and all entries after this value should be evacuated.
  // - A value that has kCompactionAbortedMarker in its top bits: table
  //   compaction has been aborted during marking. The original start of the
  //   evacuation area is still contained in the lower bits.
  // This field must be accessed atomically as it may be written to from
  // background threads during GC marking (for example to abort compaction).
  base::Atomic32 start_of_evacuation_area_ = kNotCompactingMarker;

  // The freelist used by this table.
  // This field stores an (encoded) Freelist struct, i.e. the index of the
  // current head of the freelist and the current size of the freelist. These
  // two values need to be updated together (in a single atomic word) so they
  // stay correctly synchronized when entries are allocated from the freelist
  // from multiple threads.
  base::Atomic64 freelist_ = 0;

  // Lock protecting the slow path for entry allocation, in particular Grow().
  // As the size of this structure must be predictable (it's part of
  // IsolateData), it cannot directly contain a Mutex and so instead contains a
  // pointer to one.
  base::Mutex* mutex_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPRESS_POINTERS

#endif  // V8_SANDBOX_EXTERNAL_POINTER_TABLE_H_
