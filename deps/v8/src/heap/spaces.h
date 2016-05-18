// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_H_
#define V8_HEAP_SPACES_H_

#include "src/allocation.h"
#include "src/atomic-utils.h"
#include "src/base/atomicops.h"
#include "src/base/bits.h"
#include "src/base/platform/mutex.h"
#include "src/flags.h"
#include "src/hashmap.h"
#include "src/list.h"
#include "src/objects.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class AllocationInfo;
class AllocationObserver;
class CompactionSpace;
class CompactionSpaceCollection;
class FreeList;
class Isolate;
class MemoryAllocator;
class MemoryChunk;
class PagedSpace;
class SemiSpace;
class SkipList;
class SlotsBuffer;
class SlotSet;
class Space;

// -----------------------------------------------------------------------------
// Heap structures:
//
// A JS heap consists of a young generation, an old generation, and a large
// object space. The young generation is divided into two semispaces. A
// scavenger implements Cheney's copying algorithm. The old generation is
// separated into a map space and an old object space. The map space contains
// all (and only) map objects, the rest of old objects go into the old space.
// The old generation is collected by a mark-sweep-compact collector.
//
// The semispaces of the young generation are contiguous.  The old and map
// spaces consists of a list of pages. A page has a page header and an object
// area.
//
// There is a separate large object space for objects larger than
// Page::kMaxRegularHeapObjectSize, so that they do not have to move during
// collection. The large object space is paged. Pages in large object space
// may be larger than the page size.
//
// A store-buffer based write barrier is used to keep track of intergenerational
// references.  See heap/store-buffer.h.
//
// During scavenges and mark-sweep collections we sometimes (after a store
// buffer overflow) iterate intergenerational pointers without decoding heap
// object maps so if the page belongs to old space or large object space
// it is essential to guarantee that the page does not contain any
// garbage pointers to new space: every pointer aligned word which satisfies
// the Heap::InNewSpace() predicate must be a pointer to a live heap object in
// new space. Thus objects in old space and large object spaces should have a
// special layout (e.g. no bare integer fields). This requirement does not
// apply to map space which is iterated in a special fashion. However we still
// require pointer fields of dead maps to be cleaned.
//
// To enable lazy cleaning of old space pages we can mark chunks of the page
// as being garbage.  Garbage sections are marked with a special map.  These
// sections are skipped when scanning the page, even if we are otherwise
// scanning without regard for object boundaries.  Garbage sections are chained
// together to form a free list after a GC.  Garbage sections created outside
// of GCs by object trunctation etc. may not be in the free list chain.  Very
// small free spaces are ignored, they need only be cleaned of bogus pointers
// into new space.
//
// Each page may have up to one special garbage section.  The start of this
// section is denoted by the top field in the space.  The end of the section
// is denoted by the limit field in the space.  This special garbage section
// is not marked with a free space map in the data.  The point of this section
// is to enable linear allocation without having to constantly update the byte
// array every time the top field is updated and a new object is created.  The
// special garbage section is not in the chain of garbage sections.
//
// Since the top and limit fields are in the space, not the page, only one page
// has a special garbage section, and if the top and limit are equal then there
// is no special garbage section.

// Some assertion macros used in the debugging mode.

#define DCHECK_PAGE_ALIGNED(address) \
  DCHECK((OffsetFrom(address) & Page::kPageAlignmentMask) == 0)

#define DCHECK_OBJECT_ALIGNED(address) \
  DCHECK((OffsetFrom(address) & kObjectAlignmentMask) == 0)

#define DCHECK_OBJECT_SIZE(size) \
  DCHECK((0 < size) && (size <= Page::kMaxRegularHeapObjectSize))

#define DCHECK_CODEOBJECT_SIZE(size, code_space) \
  DCHECK((0 < size) && (size <= code_space->AreaSize()))

#define DCHECK_PAGE_OFFSET(offset) \
  DCHECK((Page::kObjectStartOffset <= offset) && (offset <= Page::kPageSize))

#define DCHECK_MAP_PAGE_INDEX(index) \
  DCHECK((0 <= index) && (index <= MapSpace::kMaxMapPageIndex))


class MarkBit {
 public:
  typedef uint32_t CellType;

  inline MarkBit(CellType* cell, CellType mask) : cell_(cell), mask_(mask) {}

#ifdef DEBUG
  bool operator==(const MarkBit& other) {
    return cell_ == other.cell_ && mask_ == other.mask_;
  }
#endif

 private:
  inline CellType* cell() { return cell_; }
  inline CellType mask() { return mask_; }

  inline MarkBit Next() {
    CellType new_mask = mask_ << 1;
    if (new_mask == 0) {
      return MarkBit(cell_ + 1, 1);
    } else {
      return MarkBit(cell_, new_mask);
    }
  }

  inline void Set() { *cell_ |= mask_; }
  inline bool Get() { return (*cell_ & mask_) != 0; }
  inline void Clear() { *cell_ &= ~mask_; }

  CellType* cell_;
  CellType mask_;

  friend class Marking;
};


// Bitmap is a sequence of cells each containing fixed number of bits.
class Bitmap {
 public:
  static const uint32_t kBitsPerCell = 32;
  static const uint32_t kBitsPerCellLog2 = 5;
  static const uint32_t kBitIndexMask = kBitsPerCell - 1;
  static const uint32_t kBytesPerCell = kBitsPerCell / kBitsPerByte;
  static const uint32_t kBytesPerCellLog2 = kBitsPerCellLog2 - kBitsPerByteLog2;

  static const size_t kLength = (1 << kPageSizeBits) >> (kPointerSizeLog2);

  static const size_t kSize =
      (1 << kPageSizeBits) >> (kPointerSizeLog2 + kBitsPerByteLog2);


  static int CellsForLength(int length) {
    return (length + kBitsPerCell - 1) >> kBitsPerCellLog2;
  }

  int CellsCount() { return CellsForLength(kLength); }

  static int SizeFor(int cells_count) {
    return sizeof(MarkBit::CellType) * cells_count;
  }

  INLINE(static uint32_t IndexToCell(uint32_t index)) {
    return index >> kBitsPerCellLog2;
  }

  V8_INLINE static uint32_t IndexInCell(uint32_t index) {
    return index & kBitIndexMask;
  }

  INLINE(static uint32_t CellToIndex(uint32_t index)) {
    return index << kBitsPerCellLog2;
  }

  INLINE(static uint32_t CellAlignIndex(uint32_t index)) {
    return (index + kBitIndexMask) & ~kBitIndexMask;
  }

  INLINE(MarkBit::CellType* cells()) {
    return reinterpret_cast<MarkBit::CellType*>(this);
  }

  INLINE(Address address()) { return reinterpret_cast<Address>(this); }

  INLINE(static Bitmap* FromAddress(Address addr)) {
    return reinterpret_cast<Bitmap*>(addr);
  }

  inline MarkBit MarkBitFromIndex(uint32_t index) {
    MarkBit::CellType mask = 1u << IndexInCell(index);
    MarkBit::CellType* cell = this->cells() + (index >> kBitsPerCellLog2);
    return MarkBit(cell, mask);
  }

  static inline void Clear(MemoryChunk* chunk);

  static void PrintWord(uint32_t word, uint32_t himask = 0) {
    for (uint32_t mask = 1; mask != 0; mask <<= 1) {
      if ((mask & himask) != 0) PrintF("[");
      PrintF((mask & word) ? "1" : "0");
      if ((mask & himask) != 0) PrintF("]");
    }
  }

  class CellPrinter {
   public:
    CellPrinter() : seq_start(0), seq_type(0), seq_length(0) {}

    void Print(uint32_t pos, uint32_t cell) {
      if (cell == seq_type) {
        seq_length++;
        return;
      }

      Flush();

      if (IsSeq(cell)) {
        seq_start = pos;
        seq_length = 0;
        seq_type = cell;
        return;
      }

      PrintF("%d: ", pos);
      PrintWord(cell);
      PrintF("\n");
    }

    void Flush() {
      if (seq_length > 0) {
        PrintF("%d: %dx%d\n", seq_start, seq_type == 0 ? 0 : 1,
               seq_length * kBitsPerCell);
        seq_length = 0;
      }
    }

    static bool IsSeq(uint32_t cell) { return cell == 0 || cell == 0xFFFFFFFF; }

   private:
    uint32_t seq_start;
    uint32_t seq_type;
    uint32_t seq_length;
  };

  void Print() {
    CellPrinter printer;
    for (int i = 0; i < CellsCount(); i++) {
      printer.Print(i, cells()[i]);
    }
    printer.Flush();
    PrintF("\n");
  }

  bool IsClean() {
    for (int i = 0; i < CellsCount(); i++) {
      if (cells()[i] != 0) {
        return false;
      }
    }
    return true;
  }

  // Clears all bits starting from {cell_base_index} up to and excluding
  // {index}. Note that {cell_base_index} is required to be cell aligned.
  void ClearRange(uint32_t cell_base_index, uint32_t index) {
    DCHECK_EQ(IndexInCell(cell_base_index), 0u);
    DCHECK_GE(index, cell_base_index);
    uint32_t start_cell_index = IndexToCell(cell_base_index);
    uint32_t end_cell_index = IndexToCell(index);
    DCHECK_GE(end_cell_index, start_cell_index);
    // Clear all cells till the cell containing the last index.
    for (uint32_t i = start_cell_index; i < end_cell_index; i++) {
      cells()[i] = 0;
    }
    // Clear all bits in the last cell till the last bit before index.
    uint32_t clear_mask = ~((1u << IndexInCell(index)) - 1);
    cells()[end_cell_index] &= clear_mask;
  }
};


// MemoryChunk represents a memory region owned by a specific space.
// It is divided into the header and the body. Chunk start is always
// 1MB aligned. Start of the body is aligned so it can accommodate
// any heap object.
class MemoryChunk {
 public:
  enum MemoryChunkFlags {
    IS_EXECUTABLE,
    POINTERS_TO_HERE_ARE_INTERESTING,
    POINTERS_FROM_HERE_ARE_INTERESTING,
    IN_FROM_SPACE,  // Mutually exclusive with IN_TO_SPACE.
    IN_TO_SPACE,    // All pages in new space has one of these two set.
    NEW_SPACE_BELOW_AGE_MARK,
    EVACUATION_CANDIDATE,
    RESCAN_ON_EVACUATION,
    NEVER_EVACUATE,  // May contain immortal immutables.
    POPULAR_PAGE,    // Slots buffer of this page overflowed on the previous GC.

    // Large objects can have a progress bar in their page header. These object
    // are scanned in increments and will be kept black while being scanned.
    // Even if the mutator writes to them they will be kept black and a white
    // to grey transition is performed in the value.
    HAS_PROGRESS_BAR,

    // This flag is intended to be used for testing. Works only when both
    // FLAG_stress_compaction and FLAG_manual_evacuation_candidates_selection
    // are set. It forces the page to become an evacuation candidate at next
    // candidates selection cycle.
    FORCE_EVACUATION_CANDIDATE_FOR_TESTING,

    // This flag is intended to be used for testing.
    NEVER_ALLOCATE_ON_PAGE,

    // The memory chunk is already logically freed, however the actual freeing
    // still has to be performed.
    PRE_FREED,

    // |COMPACTION_WAS_ABORTED|: Indicates that the compaction in this page
    //   has been aborted and needs special handling by the sweeper.
    COMPACTION_WAS_ABORTED,

    // Last flag, keep at bottom.
    NUM_MEMORY_CHUNK_FLAGS
  };

  // |kCompactionDone|: Initial compaction state of a |MemoryChunk|.
  // |kCompactingInProgress|:  Parallel compaction is currently in progress.
  // |kCompactingFinalize|: Parallel compaction is done but the chunk needs to
  //   be finalized.
  // |kCompactingAborted|: Parallel compaction has been aborted, which should
  //   for now only happen in OOM scenarios.
  enum ParallelCompactingState {
    kCompactingDone,
    kCompactingInProgress,
    kCompactingFinalize,
    kCompactingAborted,
  };

  // |kSweepingDone|: The page state when sweeping is complete or sweeping must
  //   not be performed on that page. Sweeper threads that are done with their
  //   work will set this value and not touch the page anymore.
  // |kSweepingPending|: This page is ready for parallel sweeping.
  // |kSweepingInProgress|: This page is currently swept by a sweeper thread.
  enum ConcurrentSweepingState {
    kSweepingDone,
    kSweepingPending,
    kSweepingInProgress,
  };

  // Every n write barrier invocations we go to runtime even though
  // we could have handled it in generated code.  This lets us check
  // whether we have hit the limit and should do some more marking.
  static const int kWriteBarrierCounterGranularity = 500;

  static const int kPointersToHereAreInterestingMask =
      1 << POINTERS_TO_HERE_ARE_INTERESTING;

  static const int kPointersFromHereAreInterestingMask =
      1 << POINTERS_FROM_HERE_ARE_INTERESTING;

  static const int kEvacuationCandidateMask = 1 << EVACUATION_CANDIDATE;

  static const int kSkipEvacuationSlotsRecordingMask =
      (1 << EVACUATION_CANDIDATE) | (1 << RESCAN_ON_EVACUATION) |
      (1 << IN_FROM_SPACE) | (1 << IN_TO_SPACE);

  static const intptr_t kAlignment =
      (static_cast<uintptr_t>(1) << kPageSizeBits);

  static const intptr_t kAlignmentMask = kAlignment - 1;

  static const intptr_t kSizeOffset = 0;

  static const intptr_t kLiveBytesOffset =
      kSizeOffset + kPointerSize  // size_t size
      + kIntptrSize               // intptr_t flags_
      + kPointerSize              // Address area_start_
      + kPointerSize              // Address area_end_
      + 2 * kPointerSize          // base::VirtualMemory reservation_
      + kPointerSize              // Address owner_
      + kPointerSize              // Heap* heap_
      + kIntSize;                 // int progress_bar_

  static const size_t kSlotsBufferOffset =
      kLiveBytesOffset + kIntSize;  // int live_byte_count_

  static const size_t kWriteBarrierCounterOffset =
      kSlotsBufferOffset + kPointerSize  // SlotsBuffer* slots_buffer_;
      + kPointerSize                     // SlotSet* old_to_new_slots_;
      + kPointerSize                     // SlotSet* old_to_old_slots_;
      + kPointerSize;                    // SkipList* skip_list_;

  static const size_t kMinHeaderSize =
      kWriteBarrierCounterOffset +
      kIntptrSize         // intptr_t write_barrier_counter_
      + kPointerSize      // AtomicValue high_water_mark_
      + kPointerSize      // base::Mutex* mutex_
      + kPointerSize      // base::AtomicWord parallel_sweeping_
      + kPointerSize      // AtomicValue parallel_compaction_
      + 2 * kPointerSize  // AtomicNumber free-list statistics
      + kPointerSize      // AtomicValue next_chunk_
      + kPointerSize;     // AtomicValue prev_chunk_

  // We add some more space to the computed header size to amount for missing
  // alignment requirements in our computation.
  // Try to get kHeaderSize properly aligned on 32-bit and 64-bit machines.
  static const size_t kHeaderSize = kMinHeaderSize;

  static const int kBodyOffset =
      CODE_POINTER_ALIGN(kHeaderSize + Bitmap::kSize);

  // The start offset of the object area in a page. Aligned to both maps and
  // code alignment to be suitable for both.  Also aligned to 32 words because
  // the marking bitmap is arranged in 32 bit chunks.
  static const int kObjectStartAlignment = 32 * kPointerSize;
  static const int kObjectStartOffset =
      kBodyOffset - 1 +
      (kObjectStartAlignment - (kBodyOffset - 1) % kObjectStartAlignment);

  static const int kFlagsOffset = kPointerSize;

  static inline void IncrementLiveBytesFromMutator(HeapObject* object, int by);
  static inline void IncrementLiveBytesFromGC(HeapObject* object, int by);

  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromAddress(Address a) {
    return reinterpret_cast<MemoryChunk*>(OffsetFrom(a) & ~kAlignmentMask);
  }

  static inline MemoryChunk* FromAnyPointerAddress(Heap* heap, Address addr);

  static inline void UpdateHighWaterMark(Address mark) {
    if (mark == nullptr) return;
    // Need to subtract one from the mark because when a chunk is full the
    // top points to the next address after the chunk, which effectively belongs
    // to another chunk. See the comment to Page::FromAllocationTop.
    MemoryChunk* chunk = MemoryChunk::FromAddress(mark - 1);
    intptr_t new_mark = static_cast<intptr_t>(mark - chunk->address());
    intptr_t old_mark = 0;
    do {
      old_mark = chunk->high_water_mark_.Value();
    } while ((new_mark > old_mark) &&
             !chunk->high_water_mark_.TrySetValue(old_mark, new_mark));
  }

  static bool IsValid(MemoryChunk* chunk) { return chunk != nullptr; }

  Address address() { return reinterpret_cast<Address>(this); }

  base::Mutex* mutex() { return mutex_; }

  bool Contains(Address addr) {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether |addr| can be a limit of addresses in this page. It's a
  // limit if it's in the page, or if it's just after the last byte of the page.
  bool ContainsLimit(Address addr) {
    return addr >= area_start() && addr <= area_end();
  }

  AtomicValue<ConcurrentSweepingState>& concurrent_sweeping_state() {
    return concurrent_sweeping_;
  }

  AtomicValue<ParallelCompactingState>& parallel_compaction_state() {
    return parallel_compaction_;
  }

  // Manage live byte count, i.e., count of bytes in black objects.
  inline void ResetLiveBytes();
  inline void IncrementLiveBytes(int by);

  int LiveBytes() {
    DCHECK_LE(static_cast<size_t>(live_byte_count_), size_);
    return live_byte_count_;
  }

  void SetLiveBytes(int live_bytes) {
    DCHECK_GE(live_bytes, 0);
    DCHECK_LE(static_cast<size_t>(live_bytes), size_);
    live_byte_count_ = live_bytes;
  }

  int write_barrier_counter() {
    return static_cast<int>(write_barrier_counter_);
  }

  void set_write_barrier_counter(int counter) {
    write_barrier_counter_ = counter;
  }

  size_t size() const { return size_; }

  inline Heap* heap() const { return heap_; }

  inline SkipList* skip_list() { return skip_list_; }

  inline void set_skip_list(SkipList* skip_list) { skip_list_ = skip_list; }

  inline SlotsBuffer* slots_buffer() { return slots_buffer_; }

  inline SlotsBuffer** slots_buffer_address() { return &slots_buffer_; }

  inline SlotSet* old_to_new_slots() { return old_to_new_slots_; }
  inline SlotSet* old_to_old_slots() { return old_to_old_slots_; }

  void AllocateOldToNewSlots();
  void ReleaseOldToNewSlots();
  void AllocateOldToOldSlots();
  void ReleaseOldToOldSlots();

  Address area_start() { return area_start_; }
  Address area_end() { return area_end_; }
  int area_size() { return static_cast<int>(area_end() - area_start()); }

  bool CommitArea(size_t requested);

  // Approximate amount of physical memory committed for this chunk.
  size_t CommittedPhysicalMemory() { return high_water_mark_.Value(); }

  int progress_bar() {
    DCHECK(IsFlagSet(HAS_PROGRESS_BAR));
    return progress_bar_;
  }

  void set_progress_bar(int progress_bar) {
    DCHECK(IsFlagSet(HAS_PROGRESS_BAR));
    progress_bar_ = progress_bar;
  }

  void ResetProgressBar() {
    if (IsFlagSet(MemoryChunk::HAS_PROGRESS_BAR)) {
      set_progress_bar(0);
      ClearFlag(MemoryChunk::HAS_PROGRESS_BAR);
    }
  }

  inline Bitmap* markbits() {
    return Bitmap::FromAddress(address() + kHeaderSize);
  }

  inline uint32_t AddressToMarkbitIndex(Address addr) {
    return static_cast<uint32_t>(addr - this->address()) >> kPointerSizeLog2;
  }

  inline Address MarkbitIndexToAddress(uint32_t index) {
    return this->address() + (index << kPointerSizeLog2);
  }

  void PrintMarkbits() { markbits()->Print(); }

  void SetFlag(int flag) { flags_ |= static_cast<uintptr_t>(1) << flag; }

  void ClearFlag(int flag) { flags_ &= ~(static_cast<uintptr_t>(1) << flag); }

  bool IsFlagSet(int flag) {
    return (flags_ & (static_cast<uintptr_t>(1) << flag)) != 0;
  }

  // Set or clear multiple flags at a time. The flags in the mask are set to
  // the value in "flags", the rest retain the current value in |flags_|.
  void SetFlags(intptr_t flags, intptr_t mask) {
    flags_ = (flags_ & ~mask) | (flags & mask);
  }

  // Return all current flags.
  intptr_t GetFlags() { return flags_; }

  bool NeverEvacuate() { return IsFlagSet(NEVER_EVACUATE); }

  void MarkNeverEvacuate() { SetFlag(NEVER_EVACUATE); }

  bool IsEvacuationCandidate() {
    DCHECK(!(IsFlagSet(NEVER_EVACUATE) && IsFlagSet(EVACUATION_CANDIDATE)));
    return IsFlagSet(EVACUATION_CANDIDATE);
  }

  bool CanAllocate() {
    return !IsEvacuationCandidate() && !IsFlagSet(NEVER_ALLOCATE_ON_PAGE);
  }

  void MarkEvacuationCandidate() {
    DCHECK(!IsFlagSet(NEVER_EVACUATE));
    DCHECK_NULL(slots_buffer_);
    SetFlag(EVACUATION_CANDIDATE);
  }

  void ClearEvacuationCandidate() {
    DCHECK(slots_buffer_ == NULL);
    ClearFlag(EVACUATION_CANDIDATE);
  }

  bool ShouldSkipEvacuationSlotRecording() {
    return (flags_ & kSkipEvacuationSlotsRecordingMask) != 0;
  }

  Executability executable() {
    return IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
  }

  bool InNewSpace() {
    return (flags_ & ((1 << IN_FROM_SPACE) | (1 << IN_TO_SPACE))) != 0;
  }

  bool InToSpace() { return IsFlagSet(IN_TO_SPACE); }

  bool InFromSpace() { return IsFlagSet(IN_FROM_SPACE); }

  MemoryChunk* next_chunk() { return next_chunk_.Value(); }

  MemoryChunk* prev_chunk() { return prev_chunk_.Value(); }

  void set_next_chunk(MemoryChunk* next) { next_chunk_.SetValue(next); }

  void set_prev_chunk(MemoryChunk* prev) { prev_chunk_.SetValue(prev); }

  Space* owner() const {
    if ((reinterpret_cast<intptr_t>(owner_) & kPageHeaderTagMask) ==
        kPageHeaderTag) {
      return reinterpret_cast<Space*>(reinterpret_cast<intptr_t>(owner_) -
                                      kPageHeaderTag);
    } else {
      return nullptr;
    }
  }

  void set_owner(Space* space) {
    DCHECK((reinterpret_cast<intptr_t>(space) & kPageHeaderTagMask) == 0);
    owner_ = reinterpret_cast<Address>(space) + kPageHeaderTag;
    DCHECK((reinterpret_cast<intptr_t>(owner_) & kPageHeaderTagMask) ==
           kPageHeaderTag);
  }

  bool HasPageHeader() { return owner() != nullptr; }

  void InsertAfter(MemoryChunk* other);
  void Unlink();

 protected:
  static MemoryChunk* Initialize(Heap* heap, Address base, size_t size,
                                 Address area_start, Address area_end,
                                 Executability executable, Space* owner,
                                 base::VirtualMemory* reservation);

  // Should be called when memory chunk is about to be freed.
  void ReleaseAllocatedMemory();

  base::VirtualMemory* reserved_memory() { return &reservation_; }

  size_t size_;
  intptr_t flags_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;
  Address area_end_;

  // If the chunk needs to remember its memory reservation, it is stored here.
  base::VirtualMemory reservation_;

  // The identity of the owning space.  This is tagged as a failure pointer, but
  // no failure can be in an object, so this can be distinguished from any entry
  // in a fixed array.
  Address owner_;

  Heap* heap_;

  // Used by the incremental marker to keep track of the scanning progress in
  // large objects that have a progress bar and are scanned in increments.
  int progress_bar_;

  // Count of bytes marked black on page.
  int live_byte_count_;

  SlotsBuffer* slots_buffer_;

  // A single slot set for small pages (of size kPageSize) or an array of slot
  // set for large pages. In the latter case the number of entries in the array
  // is ceil(size() / kPageSize).
  SlotSet* old_to_new_slots_;
  SlotSet* old_to_old_slots_;

  SkipList* skip_list_;

  intptr_t write_barrier_counter_;

  // Assuming the initial allocation on a page is sequential,
  // count highest number of bytes ever allocated on the page.
  AtomicValue<intptr_t> high_water_mark_;

  base::Mutex* mutex_;

  AtomicValue<ConcurrentSweepingState> concurrent_sweeping_;
  AtomicValue<ParallelCompactingState> parallel_compaction_;

  // PagedSpace free-list statistics.
  AtomicNumber<intptr_t> available_in_free_list_;
  AtomicNumber<intptr_t> wasted_memory_;

  // next_chunk_ holds a pointer of type MemoryChunk
  AtomicValue<MemoryChunk*> next_chunk_;
  // prev_chunk_ holds a pointer of type MemoryChunk
  AtomicValue<MemoryChunk*> prev_chunk_;

 private:
  void InitializeReservedMemory() { reservation_.Reset(); }

  friend class MemoryAllocator;
  friend class MemoryChunkValidator;
};

enum FreeListCategoryType {
  kSmall,
  kMedium,
  kLarge,
  kHuge,

  kFirstCategory = kSmall,
  kLastCategory = kHuge,
  kNumberOfCategories = kLastCategory + 1
};

// -----------------------------------------------------------------------------
// A page is a memory chunk of a size 1MB. Large object pages may be larger.
//
// The only way to get a page pointer is by calling factory methods:
//   Page* p = Page::FromAddress(addr); or
//   Page* p = Page::FromAllocationTop(top);
class Page : public MemoryChunk {
 public:
  // Returns the page containing a given address. The address ranges
  // from [page_addr .. page_addr + kPageSize[
  // This only works if the object is in fact in a page.  See also MemoryChunk::
  // FromAddress() and FromAnyAddress().
  INLINE(static Page* FromAddress(Address a)) {
    return reinterpret_cast<Page*>(OffsetFrom(a) & ~kPageAlignmentMask);
  }

  // Only works for addresses in pointer spaces, not code space.
  inline static Page* FromAnyPointerAddress(Heap* heap, Address addr);

  // Returns the page containing an allocation top. Because an allocation
  // top address can be the upper bound of the page, we need to subtract
  // it with kPointerSize first. The address ranges from
  // [page_addr + kObjectStartOffset .. page_addr + kPageSize].
  INLINE(static Page* FromAllocationTop(Address top)) {
    Page* p = FromAddress(top - kPointerSize);
    return p;
  }

  // Returns the next page in the chain of pages owned by a space.
  inline Page* next_page() {
    DCHECK(next_chunk()->owner() == owner());
    return static_cast<Page*>(next_chunk());
  }
  inline Page* prev_page() {
    DCHECK(prev_chunk()->owner() == owner());
    return static_cast<Page*>(prev_chunk());
  }
  inline void set_next_page(Page* page);
  inline void set_prev_page(Page* page);

  // Checks whether an address is page aligned.
  static bool IsAlignedToPageSize(Address a) {
    return 0 == (OffsetFrom(a) & kPageAlignmentMask);
  }

  // Returns the offset of a given address to this page.
  INLINE(int Offset(Address a)) {
    int offset = static_cast<int>(a - address());
    return offset;
  }

  // Returns the address for a given offset to the this page.
  Address OffsetToAddress(int offset) {
    DCHECK_PAGE_OFFSET(offset);
    return address() + offset;
  }

  // ---------------------------------------------------------------------

  // Page size in bytes.  This must be a multiple of the OS page size.
  static const int kPageSize = 1 << kPageSizeBits;

  // Maximum object size that gets allocated into regular pages. Objects larger
  // than that size are allocated in large object space and are never moved in
  // memory. This also applies to new space allocation, since objects are never
  // migrated from new space to large object space. Takes double alignment into
  // account.
  // TODO(hpayer): This limit should be way smaller but we currently have
  // short living objects >256K.
  static const int kMaxRegularHeapObjectSize = 600 * KB;

  static const int kAllocatableMemory = kPageSize - kObjectStartOffset;

  // Page size mask.
  static const intptr_t kPageAlignmentMask = (1 << kPageSizeBits) - 1;

  inline void ClearGCFields();

  static inline Page* Initialize(Heap* heap, MemoryChunk* chunk,
                                 Executability executable, PagedSpace* owner);

  void InitializeAsAnchor(PagedSpace* owner);

  // WaitUntilSweepingCompleted only works when concurrent sweeping is in
  // progress. In particular, when we know that right before this call a
  // sweeper thread was sweeping this page.
  void WaitUntilSweepingCompleted() {
    mutex_->Lock();
    mutex_->Unlock();
    DCHECK(SweepingDone());
  }

  bool SweepingDone() {
    return concurrent_sweeping_state().Value() == kSweepingDone;
  }

  void ResetFreeListStatistics();

  int LiveBytesFromFreeList() {
    return static_cast<int>(area_size() - wasted_memory() -
                            available_in_free_list());
  }

#define FRAGMENTATION_STATS_ACCESSORS(type, name)        \
  type name() { return name##_.Value(); }                \
  void set_##name(type name) { name##_.SetValue(name); } \
  void add_##name(type name) { name##_.Increment(name); }

  FRAGMENTATION_STATS_ACCESSORS(intptr_t, wasted_memory)
  FRAGMENTATION_STATS_ACCESSORS(intptr_t, available_in_free_list)

#undef FRAGMENTATION_STATS_ACCESSORS

#ifdef DEBUG
  void Print();
#endif  // DEBUG

  friend class MemoryAllocator;
};


class LargePage : public MemoryChunk {
 public:
  HeapObject* GetObject() { return HeapObject::FromAddress(area_start()); }

  inline LargePage* next_page() {
    return static_cast<LargePage*>(next_chunk());
  }

  inline void set_next_page(LargePage* page) { set_next_chunk(page); }

 private:
  static inline LargePage* Initialize(Heap* heap, MemoryChunk* chunk);

  friend class MemoryAllocator;
};


// ----------------------------------------------------------------------------
// Space is the abstract superclass for all allocation spaces.
class Space : public Malloced {
 public:
  Space(Heap* heap, AllocationSpace id, Executability executable)
      : allocation_observers_(new List<AllocationObserver*>()),
        allocation_observers_paused_(false),
        heap_(heap),
        id_(id),
        executable_(executable),
        committed_(0),
        max_committed_(0) {}

  virtual ~Space() {}

  Heap* heap() const { return heap_; }

  // Does the space need executable memory?
  Executability executable() { return executable_; }

  // Identity used in error reporting.
  AllocationSpace identity() { return id_; }

  virtual void AddAllocationObserver(AllocationObserver* observer) {
    allocation_observers_->Add(observer);
  }

  virtual void RemoveAllocationObserver(AllocationObserver* observer) {
    bool removed = allocation_observers_->RemoveElement(observer);
    USE(removed);
    DCHECK(removed);
  }

  virtual void PauseAllocationObservers() {
    allocation_observers_paused_ = true;
  }

  virtual void ResumeAllocationObservers() {
    allocation_observers_paused_ = false;
  }

  void AllocationStep(Address soon_object, int size);

  // Return the total amount committed memory for this space, i.e., allocatable
  // memory and page headers.
  virtual intptr_t CommittedMemory() { return committed_; }

  virtual intptr_t MaximumCommittedMemory() { return max_committed_; }

  // Returns allocated size.
  virtual intptr_t Size() = 0;

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see LargeObjectSpace).
  virtual intptr_t SizeOfObjects() { return Size(); }

  // Approximate amount of physical memory committed for this space.
  virtual size_t CommittedPhysicalMemory() = 0;

  // Return the available bytes without growing.
  virtual intptr_t Available() = 0;

  virtual int RoundSizeDownToObjectAlignment(int size) {
    if (id_ == CODE_SPACE) {
      return RoundDown(size, kCodeAlignment);
    } else {
      return RoundDown(size, kPointerSize);
    }
  }

#ifdef DEBUG
  virtual void Print() = 0;
#endif

 protected:
  void AccountCommitted(intptr_t bytes) {
    DCHECK_GE(bytes, 0);
    committed_ += bytes;
    if (committed_ > max_committed_) {
      max_committed_ = committed_;
    }
  }

  void AccountUncommitted(intptr_t bytes) {
    DCHECK_GE(bytes, 0);
    committed_ -= bytes;
    DCHECK_GE(committed_, 0);
  }

  v8::base::SmartPointer<List<AllocationObserver*>> allocation_observers_;
  bool allocation_observers_paused_;

 private:
  Heap* heap_;
  AllocationSpace id_;
  Executability executable_;

  // Keeps track of committed memory in a space.
  intptr_t committed_;
  intptr_t max_committed_;
};


class MemoryChunkValidator {
  // Computed offsets should match the compiler generated ones.
  STATIC_ASSERT(MemoryChunk::kSizeOffset == offsetof(MemoryChunk, size_));
  STATIC_ASSERT(MemoryChunk::kLiveBytesOffset ==
                offsetof(MemoryChunk, live_byte_count_));
  STATIC_ASSERT(MemoryChunk::kSlotsBufferOffset ==
                offsetof(MemoryChunk, slots_buffer_));
  STATIC_ASSERT(MemoryChunk::kWriteBarrierCounterOffset ==
                offsetof(MemoryChunk, write_barrier_counter_));

  // Validate our estimates on the header size.
  STATIC_ASSERT(sizeof(MemoryChunk) <= MemoryChunk::kHeaderSize);
  STATIC_ASSERT(sizeof(LargePage) <= MemoryChunk::kHeaderSize);
  STATIC_ASSERT(sizeof(Page) <= MemoryChunk::kHeaderSize);
};


// ----------------------------------------------------------------------------
// All heap objects containing executable code (code objects) must be allocated
// from a 2 GB range of memory, so that they can call each other using 32-bit
// displacements.  This happens automatically on 32-bit platforms, where 32-bit
// displacements cover the entire 4GB virtual address space.  On 64-bit
// platforms, we support this using the CodeRange object, which reserves and
// manages a range of virtual memory.
class CodeRange {
 public:
  explicit CodeRange(Isolate* isolate);
  ~CodeRange() { TearDown(); }

  // Reserves a range of virtual memory, but does not commit any of it.
  // Can only be called once, at heap initialization time.
  // Returns false on failure.
  bool SetUp(size_t requested_size);

  bool valid() { return code_range_ != NULL; }
  Address start() {
    DCHECK(valid());
    return static_cast<Address>(code_range_->address());
  }
  size_t size() {
    DCHECK(valid());
    return code_range_->size();
  }
  bool contains(Address address) {
    if (!valid()) return false;
    Address start = static_cast<Address>(code_range_->address());
    return start <= address && address < start + code_range_->size();
  }

  // Allocates a chunk of memory from the large-object portion of
  // the code range.  On platforms with no separate code range, should
  // not be called.
  MUST_USE_RESULT Address AllocateRawMemory(const size_t requested_size,
                                            const size_t commit_size,
                                            size_t* allocated);
  bool CommitRawMemory(Address start, size_t length);
  bool UncommitRawMemory(Address start, size_t length);
  void FreeRawMemory(Address buf, size_t length);

 private:
  // Frees the range of virtual memory, and frees the data structures used to
  // manage it.
  void TearDown();

  Isolate* isolate_;

  // The reserved range of virtual memory that all code objects are put in.
  base::VirtualMemory* code_range_;
  // Plain old data class, just a struct plus a constructor.
  class FreeBlock {
   public:
    FreeBlock() : start(0), size(0) {}
    FreeBlock(Address start_arg, size_t size_arg)
        : start(start_arg), size(size_arg) {
      DCHECK(IsAddressAligned(start, MemoryChunk::kAlignment));
      DCHECK(size >= static_cast<size_t>(Page::kPageSize));
    }
    FreeBlock(void* start_arg, size_t size_arg)
        : start(static_cast<Address>(start_arg)), size(size_arg) {
      DCHECK(IsAddressAligned(start, MemoryChunk::kAlignment));
      DCHECK(size >= static_cast<size_t>(Page::kPageSize));
    }

    Address start;
    size_t size;
  };

  // The global mutex guards free_list_ and allocation_list_ as GC threads may
  // access both lists concurrently to the main thread.
  base::Mutex code_range_mutex_;

  // Freed blocks of memory are added to the free list.  When the allocation
  // list is exhausted, the free list is sorted and merged to make the new
  // allocation list.
  List<FreeBlock> free_list_;

  // Memory is allocated from the free blocks on the allocation list.
  // The block at current_allocation_block_index_ is the current block.
  List<FreeBlock> allocation_list_;
  int current_allocation_block_index_;

  // Finds a block on the allocation list that contains at least the
  // requested amount of memory.  If none is found, sorts and merges
  // the existing free memory blocks, and searches again.
  // If none can be found, returns false.
  bool GetNextAllocationBlock(size_t requested);
  // Compares the start addresses of two free blocks.
  static int CompareFreeBlockAddress(const FreeBlock* left,
                                     const FreeBlock* right);
  bool ReserveBlock(const size_t requested_size, FreeBlock* block);
  void ReleaseBlock(const FreeBlock* block);

  DISALLOW_COPY_AND_ASSIGN(CodeRange);
};


class SkipList {
 public:
  SkipList() { Clear(); }

  void Clear() {
    for (int idx = 0; idx < kSize; idx++) {
      starts_[idx] = reinterpret_cast<Address>(-1);
    }
  }

  Address StartFor(Address addr) { return starts_[RegionNumber(addr)]; }

  void AddObject(Address addr, int size) {
    int start_region = RegionNumber(addr);
    int end_region = RegionNumber(addr + size - kPointerSize);
    for (int idx = start_region; idx <= end_region; idx++) {
      if (starts_[idx] > addr) starts_[idx] = addr;
    }
  }

  static inline int RegionNumber(Address addr) {
    return (OffsetFrom(addr) & Page::kPageAlignmentMask) >> kRegionSizeLog2;
  }

  static void Update(Address addr, int size) {
    Page* page = Page::FromAddress(addr);
    SkipList* list = page->skip_list();
    if (list == NULL) {
      list = new SkipList();
      page->set_skip_list(list);
    }

    list->AddObject(addr, size);
  }

 private:
  static const int kRegionSizeLog2 = 13;
  static const int kRegionSize = 1 << kRegionSizeLog2;
  static const int kSize = Page::kPageSize / kRegionSize;

  STATIC_ASSERT(Page::kPageSize % kRegionSize == 0);

  Address starts_[kSize];
};


// ----------------------------------------------------------------------------
// A space acquires chunks of memory from the operating system. The memory
// allocator allocated and deallocates pages for the paged heap spaces and large
// pages for large object space.
//
// Each space has to manage it's own pages.
//
class MemoryAllocator {
 public:
  explicit MemoryAllocator(Isolate* isolate);

  // Initializes its internal bookkeeping structures.
  // Max capacity of the total space and executable memory limit.
  bool SetUp(intptr_t max_capacity, intptr_t capacity_executable);

  void TearDown();

  Page* AllocatePage(intptr_t size, PagedSpace* owner,
                     Executability executable);

  LargePage* AllocateLargePage(intptr_t object_size, Space* owner,
                               Executability executable);

  // PreFree logically frees the object, i.e., it takes care of the size
  // bookkeeping and calls the allocation callback.
  void PreFreeMemory(MemoryChunk* chunk);

  // FreeMemory can be called concurrently when PreFree was executed before.
  void PerformFreeMemory(MemoryChunk* chunk);

  // Free is a wrapper method, which calls PreFree and PerformFreeMemory
  // together.
  void Free(MemoryChunk* chunk);

  // Returns allocated spaces in bytes.
  intptr_t Size() { return size_.Value(); }

  // Returns allocated executable spaces in bytes.
  intptr_t SizeExecutable() { return size_executable_.Value(); }

  // Returns the maximum available bytes of heaps.
  intptr_t Available() {
    intptr_t size = Size();
    return capacity_ < size ? 0 : capacity_ - size;
  }

  // Returns the maximum available executable bytes of heaps.
  intptr_t AvailableExecutable() {
    intptr_t executable_size = SizeExecutable();
    if (capacity_executable_ < executable_size) return 0;
    return capacity_executable_ - executable_size;
  }

  // Returns maximum available bytes that the old space can have.
  intptr_t MaxAvailable() {
    return (Available() / Page::kPageSize) * Page::kAllocatableMemory;
  }

  // Returns an indication of whether a pointer is in a space that has
  // been allocated by this MemoryAllocator.
  V8_INLINE bool IsOutsideAllocatedSpace(const void* address) {
    return address < lowest_ever_allocated_.Value() ||
           address >= highest_ever_allocated_.Value();
  }

#ifdef DEBUG
  // Reports statistic info of the space.
  void ReportStatistics();
#endif

  // Returns a MemoryChunk in which the memory region from commit_area_size to
  // reserve_area_size of the chunk area is reserved but not committed, it
  // could be committed later by calling MemoryChunk::CommitArea.
  MemoryChunk* AllocateChunk(intptr_t reserve_area_size,
                             intptr_t commit_area_size,
                             Executability executable, Space* space);

  Address ReserveAlignedMemory(size_t requested, size_t alignment,
                               base::VirtualMemory* controller);
  Address AllocateAlignedMemory(size_t reserve_size, size_t commit_size,
                                size_t alignment, Executability executable,
                                base::VirtualMemory* controller);

  bool CommitMemory(Address addr, size_t size, Executability executable);

  void FreeNewSpaceMemory(Address addr, base::VirtualMemory* reservation,
                          Executability executable);
  void FreeMemory(base::VirtualMemory* reservation, Executability executable);
  void FreeMemory(Address addr, size_t size, Executability executable);

  // Commit a contiguous block of memory from the initial chunk.  Assumes that
  // the address is not NULL, the size is greater than zero, and that the
  // block is contained in the initial chunk.  Returns true if it succeeded
  // and false otherwise.
  bool CommitBlock(Address start, size_t size, Executability executable);

  // Uncommit a contiguous block of memory [start..(start+size)[.
  // start is not NULL, the size is greater than zero, and the
  // block is contained in the initial chunk.  Returns true if it succeeded
  // and false otherwise.
  bool UncommitBlock(Address start, size_t size);

  // Zaps a contiguous block of memory [start..(start+size)[ thus
  // filling it up with a recognizable non-NULL bit pattern.
  void ZapBlock(Address start, size_t size);

  void PerformAllocationCallback(ObjectSpace space, AllocationAction action,
                                 size_t size);

  void AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                   ObjectSpace space, AllocationAction action);

  void RemoveMemoryAllocationCallback(MemoryAllocationCallback callback);

  bool MemoryAllocationCallbackRegistered(MemoryAllocationCallback callback);

  static int CodePageGuardStartOffset();

  static int CodePageGuardSize();

  static int CodePageAreaStartOffset();

  static int CodePageAreaEndOffset();

  static int CodePageAreaSize() {
    return CodePageAreaEndOffset() - CodePageAreaStartOffset();
  }

  static int PageAreaSize(AllocationSpace space) {
    DCHECK_NE(LO_SPACE, space);
    return (space == CODE_SPACE) ? CodePageAreaSize()
                                 : Page::kAllocatableMemory;
  }

  MUST_USE_RESULT bool CommitExecutableMemory(base::VirtualMemory* vm,
                                              Address start, size_t commit_size,
                                              size_t reserved_size);

 private:
  Isolate* isolate_;

  // Maximum space size in bytes.
  intptr_t capacity_;
  // Maximum subset of capacity_ that can be executable
  intptr_t capacity_executable_;

  // Allocated space size in bytes.
  AtomicNumber<intptr_t> size_;
  // Allocated executable space size in bytes.
  AtomicNumber<intptr_t> size_executable_;

  // We keep the lowest and highest addresses allocated as a quick way
  // of determining that pointers are outside the heap. The estimate is
  // conservative, i.e. not all addrsses in 'allocated' space are allocated
  // to our heap. The range is [lowest, highest[, inclusive on the low end
  // and exclusive on the high end.
  AtomicValue<void*> lowest_ever_allocated_;
  AtomicValue<void*> highest_ever_allocated_;

  struct MemoryAllocationCallbackRegistration {
    MemoryAllocationCallbackRegistration(MemoryAllocationCallback callback,
                                         ObjectSpace space,
                                         AllocationAction action)
        : callback(callback), space(space), action(action) {}
    MemoryAllocationCallback callback;
    ObjectSpace space;
    AllocationAction action;
  };

  // A List of callback that are triggered when memory is allocated or free'd
  List<MemoryAllocationCallbackRegistration> memory_allocation_callbacks_;

  // Initializes pages in a chunk. Returns the first page address.
  // This function and GetChunkId() are provided for the mark-compact
  // collector to rebuild page headers in the from space, which is
  // used as a marking stack and its page headers are destroyed.
  Page* InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                               PagedSpace* owner);

  void UpdateAllocatedSpaceLimits(void* low, void* high) {
    // The use of atomic primitives does not guarantee correctness (wrt.
    // desired semantics) by default. The loop here ensures that we update the
    // values only if they did not change in between.
    void* ptr = nullptr;
    do {
      ptr = lowest_ever_allocated_.Value();
    } while ((low < ptr) && !lowest_ever_allocated_.TrySetValue(ptr, low));
    do {
      ptr = highest_ever_allocated_.Value();
    } while ((high > ptr) && !highest_ever_allocated_.TrySetValue(ptr, high));
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryAllocator);
};


// -----------------------------------------------------------------------------
// Interface for heap object iterator to be implemented by all object space
// object iterators.
//
// NOTE: The space specific object iterators also implements the own next()
//       method which is used to avoid using virtual functions
//       iterating a specific space.

class ObjectIterator : public Malloced {
 public:
  virtual ~ObjectIterator() {}

  virtual HeapObject* next_object() = 0;
};


// -----------------------------------------------------------------------------
// Heap object iterator in new/old/map spaces.
//
// A HeapObjectIterator iterates objects from the bottom of the given space
// to its top or from the bottom of the given page to its top.
//
// If objects are allocated in the page during iteration the iterator may
// or may not iterate over those objects.  The caller must create a new
// iterator in order to be sure to visit these new objects.
class HeapObjectIterator : public ObjectIterator {
 public:
  // Creates a new object iterator in a given space.
  explicit HeapObjectIterator(PagedSpace* space);
  explicit HeapObjectIterator(Page* page);

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns NULL when the iteration has ended.
  inline HeapObject* Next();
  inline HeapObject* next_object() override;

 private:
  enum PageMode { kOnePageOnly, kAllPagesInSpace };

  Address cur_addr_;              // Current iteration point.
  Address cur_end_;               // End iteration point.
  PagedSpace* space_;
  PageMode page_mode_;

  // Fast (inlined) path of next().
  inline HeapObject* FromCurrentPage();

  // Slow path of next(), goes into the next page.  Returns false if the
  // iteration has ended.
  bool AdvanceToNextPage();

  // Initializes fields.
  inline void Initialize(PagedSpace* owner, Address start, Address end,
                         PageMode mode);
};


// -----------------------------------------------------------------------------
// A PageIterator iterates the pages in a paged space.

class PageIterator BASE_EMBEDDED {
 public:
  explicit inline PageIterator(PagedSpace* space);

  inline bool has_next();
  inline Page* next();

 private:
  PagedSpace* space_;
  Page* prev_page_;  // Previous page returned.
  // Next page that will be returned.  Cached here so that we can use this
  // iterator for operations that deallocate pages.
  Page* next_page_;
};


// -----------------------------------------------------------------------------
// A space has a circular list of pages. The next page can be accessed via
// Page::next_page() call.

// An abstraction of allocation and relocation pointers in a page-structured
// space.
class AllocationInfo {
 public:
  AllocationInfo() : top_(nullptr), limit_(nullptr) {}
  AllocationInfo(Address top, Address limit) : top_(top), limit_(limit) {}

  void Reset(Address top, Address limit) {
    set_top(top);
    set_limit(limit);
  }

  INLINE(void set_top(Address top)) {
    SLOW_DCHECK(top == NULL ||
                (reinterpret_cast<intptr_t>(top) & kHeapObjectTagMask) == 0);
    top_ = top;
  }

  INLINE(Address top()) const {
    SLOW_DCHECK(top_ == NULL ||
                (reinterpret_cast<intptr_t>(top_) & kHeapObjectTagMask) == 0);
    return top_;
  }

  Address* top_address() { return &top_; }

  INLINE(void set_limit(Address limit)) {
    limit_ = limit;
  }

  INLINE(Address limit()) const {
    return limit_;
  }

  Address* limit_address() { return &limit_; }

#ifdef DEBUG
  bool VerifyPagedAllocation() {
    return (Page::FromAllocationTop(top_) == Page::FromAllocationTop(limit_)) &&
           (top_ <= limit_);
  }
#endif

 private:
  // Current allocation top.
  Address top_;
  // Current allocation limit.
  Address limit_;
};


// An abstraction of the accounting statistics of a page-structured space.
//
// The stats are only set by functions that ensure they stay balanced. These
// functions increase or decrease one of the non-capacity stats in conjunction
// with capacity, or else they always balance increases and decreases to the
// non-capacity stats.
class AllocationStats BASE_EMBEDDED {
 public:
  AllocationStats() { Clear(); }

  // Zero out all the allocation statistics (i.e., no capacity).
  void Clear() {
    capacity_ = 0;
    max_capacity_ = 0;
    size_ = 0;
  }

  void ClearSize() { size_ = capacity_; }

  // Reset the allocation statistics (i.e., available = capacity with no wasted
  // or allocated bytes).
  void Reset() {
    size_ = 0;
  }

  // Accessors for the allocation statistics.
  intptr_t Capacity() { return capacity_; }
  intptr_t MaxCapacity() { return max_capacity_; }
  intptr_t Size() {
    CHECK_GE(size_, 0);
    return size_;
  }

  // Grow the space by adding available bytes.  They are initially marked as
  // being in use (part of the size), but will normally be immediately freed,
  // putting them on the free list and removing them from size_.
  void ExpandSpace(int size_in_bytes) {
    capacity_ += size_in_bytes;
    size_ += size_in_bytes;
    if (capacity_ > max_capacity_) {
      max_capacity_ = capacity_;
    }
    CHECK(size_ >= 0);
  }

  // Shrink the space by removing available bytes.  Since shrinking is done
  // during sweeping, bytes have been marked as being in use (part of the size)
  // and are hereby freed.
  void ShrinkSpace(int size_in_bytes) {
    capacity_ -= size_in_bytes;
    size_ -= size_in_bytes;
    CHECK(size_ >= 0);
  }

  // Allocate from available bytes (available -> size).
  void AllocateBytes(intptr_t size_in_bytes) {
    size_ += size_in_bytes;
    CHECK(size_ >= 0);
  }

  // Free allocated bytes, making them available (size -> available).
  void DeallocateBytes(intptr_t size_in_bytes) {
    size_ -= size_in_bytes;
    CHECK_GE(size_, 0);
  }

  // Merge {other} into {this}.
  void Merge(const AllocationStats& other) {
    capacity_ += other.capacity_;
    size_ += other.size_;
    if (other.max_capacity_ > max_capacity_) {
      max_capacity_ = other.max_capacity_;
    }
    CHECK_GE(size_, 0);
  }

  void DecreaseCapacity(intptr_t size_in_bytes) {
    capacity_ -= size_in_bytes;
    CHECK_GE(capacity_, 0);
    CHECK_GE(capacity_, size_);
  }

  void IncreaseCapacity(intptr_t size_in_bytes) { capacity_ += size_in_bytes; }

 private:
  // |capacity_|: The number of object-area bytes (i.e., not including page
  // bookkeeping structures) currently in the space.
  intptr_t capacity_;

  // |max_capacity_|: The maximum capacity ever observed.
  intptr_t max_capacity_;

  // |size_|: The number of allocated bytes.
  intptr_t size_;
};


// A free list category maintains a linked list of free memory blocks.
class FreeListCategory {
 public:
  FreeListCategory() : top_(nullptr), end_(nullptr), available_(0) {}

  void Initialize(FreeList* owner, FreeListCategoryType type) {
    owner_ = owner;
    type_ = type;
  }

  // Concatenates {category} into {this}.
  //
  // Note: Thread-safe.
  intptr_t Concatenate(FreeListCategory* category);

  void Reset();

  void Free(FreeSpace* node, int size_in_bytes);

  // Pick a node from the list.
  FreeSpace* PickNodeFromList(int* node_size);

  // Pick a node from the list and compare it against {size_in_bytes}. If the
  // node's size is greater or equal return the node and null otherwise.
  FreeSpace* PickNodeFromList(int size_in_bytes, int* node_size);

  // Search for a node of size {size_in_bytes}.
  FreeSpace* SearchForNodeInList(int size_in_bytes, int* node_size);

  intptr_t EvictFreeListItemsInList(Page* p);
  bool ContainsPageFreeListItemsInList(Page* p);

  void RepairFreeList(Heap* heap);

  bool IsEmpty() { return top() == nullptr; }

  FreeList* owner() { return owner_; }
  int available() const { return available_; }

#ifdef DEBUG
  intptr_t SumFreeList();
  int FreeListLength();
  bool IsVeryLong();
#endif

 private:
  // For debug builds we accurately compute free lists lengths up until
  // {kVeryLongFreeList} by manually walking the list.
  static const int kVeryLongFreeList = 500;

  FreeSpace* top() { return top_.Value(); }
  void set_top(FreeSpace* top) { top_.SetValue(top); }

  FreeSpace* end() const { return end_; }
  void set_end(FreeSpace* end) { end_ = end; }

  // |type_|: The type of this free list category.
  FreeListCategoryType type_;

  // |top_|: Points to the top FreeSpace* in the free list category.
  AtomicValue<FreeSpace*> top_;

  // |end_|: Points to the end FreeSpace* in the free list category.
  FreeSpace* end_;

  // |available_|: Total available bytes in all blocks of this free list
  //   category.
  int available_;

  // |owner_|: The owning free list of this category.
  FreeList* owner_;
};

// A free list maintaining free blocks of memory. The free list is organized in
// a way to encourage objects allocated around the same time to be near each
// other. The normal way to allocate is intended to be by bumping a 'top'
// pointer until it hits a 'limit' pointer.  When the limit is hit we need to
// find a new space to allocate from. This is done with the free list, which is
// divided up into rough categories to cut down on waste. Having finer
// categories would scatter allocation more.

// The free list is organized in categories as follows:
// 1-31 words (too small): Such small free areas are discarded for efficiency
//   reasons. They can be reclaimed by the compactor. However the distance
//   between top and limit may be this small.
// 32-255 words (small): Used for allocating free space between 1-31 words in
//   size.
// 256-2047 words (medium): Used for allocating free space between 32-255 words
//   in size.
// 1048-16383 words (large): Used for allocating free space between 256-2047
//   words in size.
// At least 16384 words (huge): This list is for objects of 2048 words or
//   larger. Empty pages are also added to this list.
class FreeList {
 public:
  // This method returns how much memory can be allocated after freeing
  // maximum_freed memory.
  static inline int GuaranteedAllocatable(int maximum_freed) {
    if (maximum_freed <= kSmallListMin) {
      return 0;
    } else if (maximum_freed <= kSmallListMax) {
      return kSmallAllocationMax;
    } else if (maximum_freed <= kMediumListMax) {
      return kMediumAllocationMax;
    } else if (maximum_freed <= kLargeListMax) {
      return kLargeAllocationMax;
    }
    return maximum_freed;
  }

  explicit FreeList(PagedSpace* owner);

  // The method concatenates {other} into {this} and returns the added bytes,
  // including waste.
  //
  // Note: Thread-safe.
  intptr_t Concatenate(FreeList* other);

  // Adds a node on the free list. The block of size {size_in_bytes} starting
  // at {start} is placed on the free list. The return value is the number of
  // bytes that were not added to the free list, because they freed memory block
  // was too small. Bookkeeping information will be written to the block, i.e.,
  // its contents will be destroyed. The start address should be word aligned,
  // and the size should be a non-zero multiple of the word size.
  int Free(Address start, int size_in_bytes);

  // Allocate a block of size {size_in_bytes} from the free list. The block is
  // unitialized. A failure is returned if no block is available. The size
  // should be a non-zero multiple of the word size.
  MUST_USE_RESULT HeapObject* Allocate(int size_in_bytes);

  // Clear the free list.
  void Reset();

  void ResetStats() { wasted_bytes_ = 0; }

  // Return the number of bytes available on the free list.
  intptr_t Available() {
    intptr_t available = 0;
    for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
      available += category_[i].available();
    }
    return available;
  }

  // The method tries to find a {FreeSpace} node of at least {size_in_bytes}
  // size in the free list category exactly matching the size. If no suitable
  // node could be found, the method falls back to retrieving a {FreeSpace}
  // from the large or huge free list category.
  //
  // Can be used concurrently.
  MUST_USE_RESULT FreeSpace* TryRemoveMemory(intptr_t hint_size_in_bytes);

  bool IsEmpty() {
    for (int i = kFirstCategory; i < kNumberOfCategories; i++) {
      if (!category_[i].IsEmpty()) return false;
    }
    return true;
  }

  // Used after booting the VM.
  void RepairLists(Heap* heap);

  intptr_t EvictFreeListItems(Page* p);
  bool ContainsPageFreeListItems(Page* p);

  PagedSpace* owner() { return owner_; }
  intptr_t wasted_bytes() { return wasted_bytes_; }
  base::Mutex* mutex() { return &mutex_; }

#ifdef DEBUG
  void Zap();
  intptr_t SumFreeLists();
  bool IsVeryLong();
#endif

 private:
  // The size range of blocks, in bytes.
  static const int kMinBlockSize = 3 * kPointerSize;
  static const int kMaxBlockSize = Page::kAllocatableMemory;

  static const int kSmallListMin = 0x1f * kPointerSize;
  static const int kSmallListMax = 0xff * kPointerSize;
  static const int kMediumListMax = 0x7ff * kPointerSize;
  static const int kLargeListMax = 0x3fff * kPointerSize;
  static const int kSmallAllocationMax = kSmallListMin;
  static const int kMediumAllocationMax = kSmallListMax;
  static const int kLargeAllocationMax = kMediumListMax;

  FreeSpace* FindNodeFor(int size_in_bytes, int* node_size);
  FreeSpace* FindNodeIn(FreeListCategoryType category, int* node_size);

  FreeListCategory* GetFreeListCategory(FreeListCategoryType category) {
    return &category_[category];
  }

  FreeListCategoryType SelectFreeListCategoryType(size_t size_in_bytes) {
    if (size_in_bytes <= kSmallListMax) {
      return kSmall;
    } else if (size_in_bytes <= kMediumListMax) {
      return kMedium;
    } else if (size_in_bytes <= kLargeListMax) {
      return kLarge;
    }
    return kHuge;
  }

  FreeListCategoryType SelectFastAllocationFreeListCategoryType(
      size_t size_in_bytes) {
    if (size_in_bytes <= kSmallAllocationMax) {
      return kSmall;
    } else if (size_in_bytes <= kMediumAllocationMax) {
      return kMedium;
    } else if (size_in_bytes <= kLargeAllocationMax) {
      return kLarge;
    }
    return kHuge;
  }

  PagedSpace* owner_;
  base::Mutex mutex_;
  intptr_t wasted_bytes_;
  FreeListCategory category_[kNumberOfCategories];

  DISALLOW_IMPLICIT_CONSTRUCTORS(FreeList);
};


class AllocationResult {
 public:
  // Implicit constructor from Object*.
  AllocationResult(Object* object)  // NOLINT
      : object_(object) {
    // AllocationResults can't return Smis, which are used to represent
    // failure and the space to retry in.
    CHECK(!object->IsSmi());
  }

  AllocationResult() : object_(Smi::FromInt(NEW_SPACE)) {}

  static inline AllocationResult Retry(AllocationSpace space = NEW_SPACE) {
    return AllocationResult(space);
  }

  inline bool IsRetry() { return object_->IsSmi(); }

  template <typename T>
  bool To(T** obj) {
    if (IsRetry()) return false;
    *obj = T::cast(object_);
    return true;
  }

  Object* ToObjectChecked() {
    CHECK(!IsRetry());
    return object_;
  }

  inline AllocationSpace RetrySpace();

 private:
  explicit AllocationResult(AllocationSpace space)
      : object_(Smi::FromInt(static_cast<int>(space))) {}

  Object* object_;
};


STATIC_ASSERT(sizeof(AllocationResult) == kPointerSize);


// LocalAllocationBuffer represents a linear allocation area that is created
// from a given {AllocationResult} and can be used to allocate memory without
// synchronization.
//
// The buffer is properly closed upon destruction and reassignment.
// Example:
//   {
//     AllocationResult result = ...;
//     LocalAllocationBuffer a(heap, result, size);
//     LocalAllocationBuffer b = a;
//     CHECK(!a.IsValid());
//     CHECK(b.IsValid());
//     // {a} is invalid now and cannot be used for further allocations.
//   }
//   // Since {b} went out of scope, the LAB is closed, resulting in creating a
//   // filler object for the remaining area.
class LocalAllocationBuffer {
 public:
  // Indicates that a buffer cannot be used for allocations anymore. Can result
  // from either reassigning a buffer, or trying to construct it from an
  // invalid {AllocationResult}.
  static inline LocalAllocationBuffer InvalidBuffer();

  // Creates a new LAB from a given {AllocationResult}. Results in
  // InvalidBuffer if the result indicates a retry.
  static inline LocalAllocationBuffer FromResult(Heap* heap,
                                                 AllocationResult result,
                                                 intptr_t size);

  ~LocalAllocationBuffer() { Close(); }

  // Convert to C++11 move-semantics once allowed by the style guide.
  LocalAllocationBuffer(const LocalAllocationBuffer& other);
  LocalAllocationBuffer& operator=(const LocalAllocationBuffer& other);

  MUST_USE_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);

  inline bool IsValid() { return allocation_info_.top() != nullptr; }

  // Try to merge LABs, which is only possible when they are adjacent in memory.
  // Returns true if the merge was successful, false otherwise.
  inline bool TryMerge(LocalAllocationBuffer* other);

 private:
  LocalAllocationBuffer(Heap* heap, AllocationInfo allocation_info);

  void Close();

  Heap* heap_;
  AllocationInfo allocation_info_;
};


class PagedSpace : public Space {
 public:
  static const intptr_t kCompactionMemoryWanted = 500 * KB;

  // Creates a space with an id.
  PagedSpace(Heap* heap, AllocationSpace id, Executability executable);

  ~PagedSpace() override { TearDown(); }

  // Set up the space using the given address range of virtual memory (from
  // the memory allocator's initial chunk) if possible.  If the block of
  // addresses is not big enough to contain a single page-aligned page, a
  // fresh chunk will be allocated.
  bool SetUp();

  // Returns true if the space has been successfully set up and not
  // subsequently torn down.
  bool HasBeenSetUp();

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a);
  inline bool Contains(Object* o);
  bool ContainsSlow(Address addr);

  // Given an address occupied by a live object, return that object if it is
  // in this space, or a Smi if it is not.  The implementation iterates over
  // objects in the page containing the address, the cost is linear in the
  // number of objects in the page.  It may be slow.
  Object* FindObject(Address addr);

  // During boot the free_space_map is created, and afterwards we may need
  // to write it into the free list nodes that were already created.
  void RepairFreeListsAfterDeserialization();

  // Prepares for a mark-compact GC.
  void PrepareForMarkCompact();

  // Current capacity without growing (Size() + Available()).
  intptr_t Capacity() { return accounting_stats_.Capacity(); }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  void ResetFreeListStatistics();

  // Sets the capacity, the available space and the wasted space to zero.
  // The stats are rebuilt during sweeping by adding each page to the
  // capacity and the size when it is encountered.  As free spaces are
  // discovered during the sweeping they are subtracted from the size and added
  // to the available and wasted totals.
  void ClearStats() {
    accounting_stats_.ClearSize();
    free_list_.ResetStats();
    ResetFreeListStatistics();
  }

  // Increases the number of available bytes of that space.
  void AddToAccountingStats(intptr_t bytes) {
    accounting_stats_.DeallocateBytes(bytes);
  }

  // Available bytes without growing.  These are the bytes on the free list.
  // The bytes in the linear allocation area are not included in this total
  // because updating the stats would slow down allocation.  New pages are
  // immediately added to the free list so they show up here.
  intptr_t Available() override { return free_list_.Available(); }

  // Allocated bytes in this space.  Garbage bytes that were not found due to
  // concurrent sweeping are counted as being allocated!  The bytes in the
  // current linear allocation area (between top and limit) are also counted
  // here.
  intptr_t Size() override { return accounting_stats_.Size(); }

  // As size, but the bytes in lazily swept pages are estimated and the bytes
  // in the current linear allocation area are not included.
  intptr_t SizeOfObjects() override;

  // Wasted bytes in this space.  These are just the bytes that were thrown away
  // due to being too small to use for allocation.
  virtual intptr_t Waste() { return free_list_.wasted_bytes(); }

  // Returns the allocation pointer in this space.
  Address top() { return allocation_info_.top(); }
  Address limit() { return allocation_info_.limit(); }

  // The allocation top address.
  Address* allocation_top_address() { return allocation_info_.top_address(); }

  // The allocation limit address.
  Address* allocation_limit_address() {
    return allocation_info_.limit_address();
  }

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not.
  MUST_USE_RESULT inline AllocationResult AllocateRawUnaligned(
      int size_in_bytes);

  MUST_USE_RESULT inline AllocationResult AllocateRawUnalignedSynchronized(
      int size_in_bytes);

  // Allocate the requested number of bytes in the space double aligned if
  // possible, return a failure object if not.
  MUST_USE_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);

  // Allocate the requested number of bytes in the space and consider allocation
  // alignment if needed.
  MUST_USE_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationAlignment alignment);

  // Give a block of memory to the space's free list.  It might be added to
  // the free list or accounted as waste.
  // If add_to_freelist is false then just accounting stats are updated and
  // no attempt to add area to free list is made.
  int Free(Address start, int size_in_bytes) {
    int wasted = free_list_.Free(start, size_in_bytes);
    accounting_stats_.DeallocateBytes(size_in_bytes);
    return size_in_bytes - wasted;
  }

  void ResetFreeList() { free_list_.Reset(); }

  // Set space allocation info.
  void SetTopAndLimit(Address top, Address limit) {
    DCHECK(top == limit ||
           Page::FromAddress(top) == Page::FromAddress(limit - 1));
    MemoryChunk::UpdateHighWaterMark(allocation_info_.top());
    allocation_info_.Reset(top, limit);
  }

  // Empty space allocation info, returning unused area to free list.
  void EmptyAllocationInfo() {
    // Mark the old linear allocation area with a free space map so it can be
    // skipped when scanning the heap.
    int old_linear_size = static_cast<int>(limit() - top());
    Free(top(), old_linear_size);
    SetTopAndLimit(NULL, NULL);
  }

  void Allocate(int bytes) { accounting_stats_.AllocateBytes(bytes); }

  void IncreaseCapacity(int size);

  // Releases an unused page and shrinks the space.
  void ReleasePage(Page* page, bool evict_free_list_items);

  // The dummy page that anchors the linked list of pages.
  Page* anchor() { return &anchor_; }

#ifdef VERIFY_HEAP
  // Verify integrity of this space.
  virtual void Verify(ObjectVisitor* visitor);

  // Overridden by subclasses to verify space-specific object
  // properties (e.g., only maps or free-list nodes are in map space).
  virtual void VerifyObject(HeapObject* obj) {}
#endif

#ifdef DEBUG
  // Print meta info and objects in this space.
  void Print() override;

  // Reports statistics for the space
  void ReportStatistics();

  // Report code object related statistics
  void CollectCodeStatistics();
  static void ReportCodeStatistics(Isolate* isolate);
  static void ResetCodeStatistics(Isolate* isolate);
#endif

  // This function tries to steal size_in_bytes memory from the sweeper threads
  // free-lists. If it does not succeed stealing enough memory, it will wait
  // for the sweeper threads to finish sweeping.
  // It returns true when sweeping is completed and false otherwise.
  bool EnsureSweeperProgress(intptr_t size_in_bytes);

  Page* FirstPage() { return anchor_.next_page(); }
  Page* LastPage() { return anchor_.prev_page(); }

  void EvictEvacuationCandidatesFromLinearAllocationArea();

  bool CanExpand(size_t size);

  // Returns the number of total pages in this space.
  int CountTotalPages();

  // Return size of allocatable area on a page in this space.
  inline int AreaSize() { return area_size_; }

  virtual bool is_local() { return false; }

  // Merges {other} into the current space. Note that this modifies {other},
  // e.g., removes its bump pointer area and resets statistics.
  void MergeCompactionSpace(CompactionSpace* other);

  // Refills the free list from the corresponding free list filled by the
  // sweeper.
  virtual void RefillFreeList();

 protected:
  void AddMemory(Address start, intptr_t size);

  void MoveOverFreeMemory(PagedSpace* other);

  // PagedSpaces that should be included in snapshots have different, i.e.,
  // smaller, initial pages.
  virtual bool snapshotable() { return true; }

  FreeList* free_list() { return &free_list_; }

  bool HasPages() { return anchor_.next_page() != &anchor_; }

  // Cleans up the space, frees all pages in this space except those belonging
  // to the initial chunk, uncommits addresses in the initial chunk.
  void TearDown();

  // Expands the space by allocating a fixed number of pages. Returns false if
  // it cannot allocate requested number of pages from OS, or if the hard heap
  // size limit has been hit.
  bool Expand();

  // Generic fast case allocation function that tries linear allocation at the
  // address denoted by top in allocation_info_.
  inline HeapObject* AllocateLinearly(int size_in_bytes);

  // Generic fast case allocation function that tries aligned linear allocation
  // at the address denoted by top in allocation_info_. Writes the aligned
  // allocation size, which includes the filler size, to size_in_bytes.
  inline HeapObject* AllocateLinearlyAligned(int* size_in_bytes,
                                             AllocationAlignment alignment);

  // If sweeping is still in progress try to sweep unswept pages. If that is
  // not successful, wait for the sweeper threads and re-try free-list
  // allocation.
  MUST_USE_RESULT virtual HeapObject* SweepAndRetryAllocation(
      int size_in_bytes);

  // Slow path of AllocateRaw.  This function is space-dependent.
  MUST_USE_RESULT HeapObject* SlowAllocateRaw(int size_in_bytes);

  int area_size_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // The dummy page that anchors the double linked list of pages.
  Page anchor_;

  // The space's free list.
  FreeList free_list_;

  // Normal allocation information.
  AllocationInfo allocation_info_;

  // Mutex guarding any concurrent access to the space.
  base::Mutex space_mutex_;

  friend class MarkCompactCollector;
  friend class PageIterator;

  // Used in cctest.
  friend class HeapTester;
};


class NumberAndSizeInfo BASE_EMBEDDED {
 public:
  NumberAndSizeInfo() : number_(0), bytes_(0) {}

  int number() const { return number_; }
  void increment_number(int num) { number_ += num; }

  int bytes() const { return bytes_; }
  void increment_bytes(int size) { bytes_ += size; }

  void clear() {
    number_ = 0;
    bytes_ = 0;
  }

 private:
  int number_;
  int bytes_;
};


// HistogramInfo class for recording a single "bar" of a histogram.  This
// class is used for collecting statistics to print to the log file.
class HistogramInfo : public NumberAndSizeInfo {
 public:
  HistogramInfo() : NumberAndSizeInfo() {}

  const char* name() { return name_; }
  void set_name(const char* name) { name_ = name; }

 private:
  const char* name_;
};


enum SemiSpaceId { kFromSpace = 0, kToSpace = 1 };


class NewSpacePage : public MemoryChunk {
 public:
  // GC related flags copied from from-space to to-space when
  // flipping semispaces.
  static const intptr_t kCopyOnFlipFlagsMask =
      (1 << MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING) |
      (1 << MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING);

  static const int kAreaSize = Page::kAllocatableMemory;

  inline NewSpacePage* next_page() {
    return static_cast<NewSpacePage*>(next_chunk());
  }

  inline void set_next_page(NewSpacePage* page) { set_next_chunk(page); }

  inline NewSpacePage* prev_page() {
    return static_cast<NewSpacePage*>(prev_chunk());
  }

  inline void set_prev_page(NewSpacePage* page) { set_prev_chunk(page); }

  SemiSpace* semi_space() { return reinterpret_cast<SemiSpace*>(owner()); }

  bool is_anchor() { return !this->InNewSpace(); }

  static bool IsAtStart(Address addr) {
    return (reinterpret_cast<intptr_t>(addr) & Page::kPageAlignmentMask) ==
           kObjectStartOffset;
  }

  static bool IsAtEnd(Address addr) {
    return (reinterpret_cast<intptr_t>(addr) & Page::kPageAlignmentMask) == 0;
  }

  Address address() { return reinterpret_cast<Address>(this); }

  // Finds the NewSpacePage containing the given address.
  static inline NewSpacePage* FromAddress(Address address_in_page) {
    Address page_start =
        reinterpret_cast<Address>(reinterpret_cast<uintptr_t>(address_in_page) &
                                  ~Page::kPageAlignmentMask);
    NewSpacePage* page = reinterpret_cast<NewSpacePage*>(page_start);
    return page;
  }

  // Find the page for a limit address. A limit address is either an address
  // inside a page, or the address right after the last byte of a page.
  static inline NewSpacePage* FromLimit(Address address_limit) {
    return NewSpacePage::FromAddress(address_limit - 1);
  }

  // Checks if address1 and address2 are on the same new space page.
  static inline bool OnSamePage(Address address1, Address address2) {
    return NewSpacePage::FromAddress(address1) ==
           NewSpacePage::FromAddress(address2);
  }

 private:
  // Create a NewSpacePage object that is only used as anchor
  // for the doubly-linked list of real pages.
  explicit NewSpacePage(SemiSpace* owner) { InitializeAsAnchor(owner); }

  static NewSpacePage* Initialize(Heap* heap, Address start,
                                  SemiSpace* semi_space);

  // Intialize a fake NewSpacePage used as sentinel at the ends
  // of a doubly-linked list of real NewSpacePages.
  // Only uses the prev/next links, and sets flags to not be in new-space.
  void InitializeAsAnchor(SemiSpace* owner);

  friend class SemiSpace;
  friend class SemiSpaceIterator;
};


// -----------------------------------------------------------------------------
// SemiSpace in young generation
//
// A SemiSpace is a contiguous chunk of memory holding page-like memory chunks.
// The mark-compact collector  uses the memory of the first page in the from
// space as a marking stack when tracing live objects.
class SemiSpace : public Space {
 public:
  static void Swap(SemiSpace* from, SemiSpace* to);

  SemiSpace(Heap* heap, SemiSpaceId semispace)
      : Space(heap, NEW_SPACE, NOT_EXECUTABLE),
        current_capacity_(0),
        maximum_capacity_(0),
        minimum_capacity_(0),
        start_(nullptr),
        age_mark_(nullptr),
        committed_(false),
        id_(semispace),
        anchor_(this),
        current_page_(nullptr) {}

  inline bool Contains(HeapObject* o);
  inline bool Contains(Object* o);
  inline bool ContainsSlow(Address a);

  // Creates a space in the young generation. The constructor does not
  // allocate memory from the OS.
  void SetUp(Address start, int initial_capacity, int maximum_capacity);

  // Tear down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  void TearDown();

  // True if the space has been set up but not torn down.
  bool HasBeenSetUp() { return start_ != nullptr; }

  // Grow the semispace to the new capacity.  The new capacity
  // requested must be larger than the current capacity and less than
  // the maximum capacity.
  bool GrowTo(int new_capacity);

  // Shrinks the semispace to the new capacity.  The new capacity
  // requested must be more than the amount of used memory in the
  // semispace and less than the current capacity.
  bool ShrinkTo(int new_capacity);

  // Returns the start address of the first page of the space.
  Address space_start() {
    DCHECK_NE(anchor_.next_page(), &anchor_);
    return anchor_.next_page()->area_start();
  }

  // Returns the start address of the current page of the space.
  Address page_low() { return current_page_->area_start(); }

  // Returns one past the end address of the space.
  Address space_end() { return anchor_.prev_page()->area_end(); }

  // Returns one past the end address of the current page of the space.
  Address page_high() { return current_page_->area_end(); }

  bool AdvancePage() {
    NewSpacePage* next_page = current_page_->next_page();
    if (next_page == anchor()) return false;
    current_page_ = next_page;
    return true;
  }

  // Resets the space to using the first page.
  void Reset();

  // Age mark accessors.
  Address age_mark() { return age_mark_; }
  void set_age_mark(Address mark);

  bool is_committed() { return committed_; }
  bool Commit();
  bool Uncommit();

  NewSpacePage* first_page() { return anchor_.next_page(); }
  NewSpacePage* current_page() { return current_page_; }

  // Returns the current total capacity of the semispace.
  int current_capacity() { return current_capacity_; }

  // Returns the maximum total capacity of the semispace.
  int maximum_capacity() { return maximum_capacity_; }

  // Returns the initial capacity of the semispace.
  int minimum_capacity() { return minimum_capacity_; }

  SemiSpaceId id() { return id_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  // If we don't have these here then SemiSpace will be abstract.  However
  // they should never be called:

  intptr_t Size() override {
    UNREACHABLE();
    return 0;
  }

  intptr_t SizeOfObjects() override { return Size(); }

  intptr_t Available() override {
    UNREACHABLE();
    return 0;
  }

#ifdef DEBUG
  void Print() override;
  // Validate a range of of addresses in a SemiSpace.
  // The "from" address must be on a page prior to the "to" address,
  // in the linked page order, or it must be earlier on the same page.
  static void AssertValidRange(Address from, Address to);
#else
  // Do nothing.
  inline static void AssertValidRange(Address from, Address to) {}
#endif

#ifdef VERIFY_HEAP
  virtual void Verify();
#endif

 private:
  NewSpacePage* anchor() { return &anchor_; }

  void set_current_capacity(int new_capacity) {
    current_capacity_ = new_capacity;
  }

  // Copies the flags into the masked positions on all pages in the space.
  void FixPagesFlags(intptr_t flags, intptr_t flag_mask);

  // The currently committed space capacity.
  int current_capacity_;

  // The maximum capacity that can be used by this space.
  int maximum_capacity_;

  // The mimnimum capacity for the space. A space cannot shrink below this size.
  int minimum_capacity_;

  // The start address of the space.
  Address start_;
  // Used to govern object promotion during mark-compact collection.
  Address age_mark_;

  bool committed_;
  SemiSpaceId id_;

  NewSpacePage anchor_;
  NewSpacePage* current_page_;

  friend class SemiSpaceIterator;
  friend class NewSpacePageIterator;
};


// A SemiSpaceIterator is an ObjectIterator that iterates over the active
// semispace of the heap's new space.  It iterates over the objects in the
// semispace from a given start address (defaulting to the bottom of the
// semispace) to the top of the semispace.  New objects allocated after the
// iterator is created are not iterated.
class SemiSpaceIterator : public ObjectIterator {
 public:
  // Create an iterator over the allocated objects in the given to-space.
  explicit SemiSpaceIterator(NewSpace* space);

  inline HeapObject* Next();

  // Implementation of the ObjectIterator functions.
  inline HeapObject* next_object() override;

 private:
  void Initialize(Address start, Address end);

  // The current iteration point.
  Address current_;
  // The end of iteration.
  Address limit_;
};


// -----------------------------------------------------------------------------
// A PageIterator iterates the pages in a semi-space.
class NewSpacePageIterator BASE_EMBEDDED {
 public:
  // Make an iterator that runs over all pages in to-space.
  explicit inline NewSpacePageIterator(NewSpace* space);

  // Make an iterator that runs over all pages in the given semispace,
  // even those not used in allocation.
  explicit inline NewSpacePageIterator(SemiSpace* space);

  // Make iterator that iterates from the page containing start
  // to the page that contains limit in the same semispace.
  inline NewSpacePageIterator(Address start, Address limit);

  inline bool has_next();
  inline NewSpacePage* next();

 private:
  NewSpacePage* prev_page_;  // Previous page returned.
  // Next page that will be returned.  Cached here so that we can use this
  // iterator for operations that deallocate pages.
  NewSpacePage* next_page_;
  // Last page returned.
  NewSpacePage* last_page_;
};


// -----------------------------------------------------------------------------
// The young generation space.
//
// The new space consists of a contiguous pair of semispaces.  It simply
// forwards most functions to the appropriate semispace.

class NewSpace : public Space {
 public:
  // Constructor.
  explicit NewSpace(Heap* heap)
      : Space(heap, NEW_SPACE, NOT_EXECUTABLE),
        to_space_(heap, kToSpace),
        from_space_(heap, kFromSpace),
        reservation_(),
        top_on_previous_step_(0) {}

  inline bool Contains(HeapObject* o);
  inline bool ContainsSlow(Address a);
  inline bool Contains(Object* o);

  // Sets up the new space using the given chunk.
  bool SetUp(int reserved_semispace_size_, int max_semi_space_size);

  // Tears down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  void TearDown();

  // True if the space has been set up but not torn down.
  bool HasBeenSetUp() {
    return to_space_.HasBeenSetUp() && from_space_.HasBeenSetUp();
  }

  // Flip the pair of spaces.
  void Flip();

  // Grow the capacity of the semispaces.  Assumes that they are not at
  // their maximum capacity.
  void Grow();

  // Shrink the capacity of the semispaces.
  void Shrink();

  // Return the allocated bytes in the active semispace.
  intptr_t Size() override {
    return pages_used_ * NewSpacePage::kAreaSize +
           static_cast<int>(top() - to_space_.page_low());
  }

  // The same, but returning an int.  We have to have the one that returns
  // intptr_t because it is inherited, but if we know we are dealing with the
  // new space, which can't get as big as the other spaces then this is useful:
  int SizeAsInt() { return static_cast<int>(Size()); }

  // Return the allocatable capacity of a semispace.
  intptr_t Capacity() {
    SLOW_DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return (to_space_.current_capacity() / Page::kPageSize) *
           NewSpacePage::kAreaSize;
  }

  // Return the current size of a semispace, allocatable and non-allocatable
  // memory.
  intptr_t TotalCapacity() {
    DCHECK(to_space_.current_capacity() == from_space_.current_capacity());
    return to_space_.current_capacity();
  }

  // Committed memory for NewSpace is the committed memory of both semi-spaces
  // combined.
  intptr_t CommittedMemory() override {
    return from_space_.CommittedMemory() + to_space_.CommittedMemory();
  }

  intptr_t MaximumCommittedMemory() override {
    return from_space_.MaximumCommittedMemory() +
           to_space_.MaximumCommittedMemory();
  }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  // Return the available bytes without growing.
  intptr_t Available() override { return Capacity() - Size(); }

  intptr_t PagesFromStart(Address addr) {
    return static_cast<intptr_t>(addr - bottom()) / Page::kPageSize;
  }

  size_t AllocatedSinceLastGC() {
    intptr_t allocated = top() - to_space_.age_mark();
    if (allocated < 0) {
      // Runtime has lowered the top below the age mark.
      return 0;
    }
    // Correctly account for non-allocatable regions at the beginning of
    // each page from the age_mark() to the top().
    intptr_t pages =
        PagesFromStart(top()) - PagesFromStart(to_space_.age_mark());
    allocated -= pages * (NewSpacePage::kObjectStartOffset);
    DCHECK(0 <= allocated && allocated <= Size());
    return static_cast<size_t>(allocated);
  }

  // Return the maximum capacity of a semispace.
  int MaximumCapacity() {
    DCHECK(to_space_.maximum_capacity() == from_space_.maximum_capacity());
    return to_space_.maximum_capacity();
  }

  bool IsAtMaximumCapacity() { return TotalCapacity() == MaximumCapacity(); }

  // Returns the initial capacity of a semispace.
  int InitialTotalCapacity() {
    DCHECK(to_space_.minimum_capacity() == from_space_.minimum_capacity());
    return to_space_.minimum_capacity();
  }

  // Return the address of the allocation pointer in the active semispace.
  Address top() {
    DCHECK(to_space_.current_page()->ContainsLimit(allocation_info_.top()));
    return allocation_info_.top();
  }

  // Return the address of the allocation pointer limit in the active semispace.
  Address limit() {
    DCHECK(to_space_.current_page()->ContainsLimit(allocation_info_.limit()));
    return allocation_info_.limit();
  }

  // Return the address of the first object in the active semispace.
  Address bottom() { return to_space_.space_start(); }

  // Get the age mark of the inactive semispace.
  Address age_mark() { return from_space_.age_mark(); }
  // Set the age mark in the active semispace.
  void set_age_mark(Address mark) { to_space_.set_age_mark(mark); }

  // The start address of the space and a bit mask. Anding an address in the
  // new space with the mask will result in the start address.
  Address start() { return start_; }

  // The allocation top and limit address.
  Address* allocation_top_address() { return allocation_info_.top_address(); }

  // The allocation limit address.
  Address* allocation_limit_address() {
    return allocation_info_.limit_address();
  }

  MUST_USE_RESULT INLINE(AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment));

  MUST_USE_RESULT INLINE(
      AllocationResult AllocateRawUnaligned(int size_in_bytes));

  MUST_USE_RESULT INLINE(AllocationResult AllocateRaw(
      int size_in_bytes, AllocationAlignment alignment));

  MUST_USE_RESULT inline AllocationResult AllocateRawSynchronized(
      int size_in_bytes, AllocationAlignment alignment);

  // Reset the allocation pointer to the beginning of the active semispace.
  void ResetAllocationInfo();

  // When inline allocation stepping is active, either because of incremental
  // marking, idle scavenge, or allocation statistics gathering, we 'interrupt'
  // inline allocation every once in a while. This is done by setting
  // allocation_info_.limit to be lower than the actual limit and and increasing
  // it in steps to guarantee that the observers are notified periodically.
  void UpdateInlineAllocationLimit(int size_in_bytes);

  void DisableInlineAllocationSteps() {
    top_on_previous_step_ = 0;
    UpdateInlineAllocationLimit(0);
  }

  // Allows observation of inline allocation. The observer->Step() method gets
  // called after every step_size bytes have been allocated (approximately).
  // This works by adjusting the allocation limit to a lower value and adjusting
  // it after each step.
  void AddAllocationObserver(AllocationObserver* observer) override;

  void RemoveAllocationObserver(AllocationObserver* observer) override;

  // Get the extent of the inactive semispace (for use as a marking stack,
  // or to zap it). Notice: space-addresses are not necessarily on the
  // same page, so FromSpaceStart() might be above FromSpaceEnd().
  Address FromSpacePageLow() { return from_space_.page_low(); }
  Address FromSpacePageHigh() { return from_space_.page_high(); }
  Address FromSpaceStart() { return from_space_.space_start(); }
  Address FromSpaceEnd() { return from_space_.space_end(); }

  // Get the extent of the active semispace's pages' memory.
  Address ToSpaceStart() { return to_space_.space_start(); }
  Address ToSpaceEnd() { return to_space_.space_end(); }

  inline bool ToSpaceContainsSlow(Address a);
  inline bool FromSpaceContainsSlow(Address a);
  inline bool ToSpaceContains(Object* o);
  inline bool FromSpaceContains(Object* o);

  // Try to switch the active semispace to a new, empty, page.
  // Returns false if this isn't possible or reasonable (i.e., there
  // are no pages, or the current page is already empty), or true
  // if successful.
  bool AddFreshPage();
  bool AddFreshPageSynchronized();

#ifdef VERIFY_HEAP
  // Verify the active semispace.
  virtual void Verify();
#endif

#ifdef DEBUG
  // Print the active semispace.
  void Print() override { to_space_.Print(); }
#endif

  // Iterates the active semispace to collect statistics.
  void CollectStatistics();
  // Reports previously collected statistics of the active semispace.
  void ReportStatistics();
  // Clears previously collected statistics.
  void ClearHistograms();

  // Record the allocation or promotion of a heap object.  Note that we don't
  // record every single allocation, but only those that happen in the
  // to space during a scavenge GC.
  void RecordAllocation(HeapObject* obj);
  void RecordPromotion(HeapObject* obj);

  // Return whether the operation succeded.
  bool CommitFromSpaceIfNeeded() {
    if (from_space_.is_committed()) return true;
    return from_space_.Commit();
  }

  bool UncommitFromSpace() {
    if (!from_space_.is_committed()) return true;
    return from_space_.Uncommit();
  }

  bool IsFromSpaceCommitted() { return from_space_.is_committed(); }

  SemiSpace* active_space() { return &to_space_; }

  void PauseAllocationObservers() override;
  void ResumeAllocationObservers() override;

 private:
  // Update allocation info to match the current to-space page.
  void UpdateAllocationInfo();

  base::Mutex mutex_;

  Address chunk_base_;
  uintptr_t chunk_size_;

  // The semispaces.
  SemiSpace to_space_;
  SemiSpace from_space_;
  base::VirtualMemory reservation_;
  int pages_used_;

  // Start address and bit mask for containment testing.
  Address start_;

  // Allocation pointer and limit for normal allocation and allocation during
  // mark-compact collection.
  AllocationInfo allocation_info_;

  Address top_on_previous_step_;

  HistogramInfo* allocated_histogram_;
  HistogramInfo* promoted_histogram_;

  bool EnsureAllocation(int size_in_bytes, AllocationAlignment alignment);

  // If we are doing inline allocation in steps, this method performs the 'step'
  // operation. top is the memory address of the bump pointer at the last
  // inline allocation (i.e. it determines the numbers of bytes actually
  // allocated since the last step.) new_top is the address of the bump pointer
  // where the next byte is going to be allocated from. top and new_top may be
  // different when we cross a page boundary or reset the space.
  void InlineAllocationStep(Address top, Address new_top, Address soon_object,
                            size_t size);
  intptr_t GetNextInlineAllocationStepSize();
  void StartNextInlineAllocationStep();

  friend class SemiSpaceIterator;
};

class PauseAllocationObserversScope {
 public:
  explicit PauseAllocationObserversScope(Heap* heap);
  ~PauseAllocationObserversScope();

 private:
  Heap* heap_;
  DISALLOW_COPY_AND_ASSIGN(PauseAllocationObserversScope);
};

// -----------------------------------------------------------------------------
// Compaction space that is used temporarily during compaction.

class CompactionSpace : public PagedSpace {
 public:
  CompactionSpace(Heap* heap, AllocationSpace id, Executability executable)
      : PagedSpace(heap, id, executable) {}

  bool is_local() override { return true; }

  void RefillFreeList() override;

 protected:
  // The space is temporary and not included in any snapshots.
  bool snapshotable() override { return false; }

  MUST_USE_RESULT HeapObject* SweepAndRetryAllocation(
      int size_in_bytes) override;
};


// A collection of |CompactionSpace|s used by a single compaction task.
class CompactionSpaceCollection : public Malloced {
 public:
  explicit CompactionSpaceCollection(Heap* heap)
      : old_space_(heap, OLD_SPACE, Executability::NOT_EXECUTABLE),
        code_space_(heap, CODE_SPACE, Executability::EXECUTABLE) {}

  CompactionSpace* Get(AllocationSpace space) {
    switch (space) {
      case OLD_SPACE:
        return &old_space_;
      case CODE_SPACE:
        return &code_space_;
      default:
        UNREACHABLE();
    }
    UNREACHABLE();
    return nullptr;
  }

 private:
  CompactionSpace old_space_;
  CompactionSpace code_space_;
};


// -----------------------------------------------------------------------------
// Old object space (includes the old space of objects and code space)

class OldSpace : public PagedSpace {
 public:
  // Creates an old space object. The constructor does not allocate pages
  // from OS.
  OldSpace(Heap* heap, AllocationSpace id, Executability executable)
      : PagedSpace(heap, id, executable) {}
};


// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define DCHECK_SEMISPACE_ALLOCATION_INFO(info, space) \
  SLOW_DCHECK((space).page_low() <= (info).top() &&   \
              (info).top() <= (space).page_high() &&  \
              (info).limit() <= (space).page_high())


// -----------------------------------------------------------------------------
// Old space for all map objects

class MapSpace : public PagedSpace {
 public:
  // Creates a map space object.
  MapSpace(Heap* heap, AllocationSpace id)
      : PagedSpace(heap, id, NOT_EXECUTABLE),
        max_map_space_pages_(kMaxMapPageIndex - 1) {}

  // Given an index, returns the page address.
  // TODO(1600): this limit is artifical just to keep code compilable
  static const int kMaxMapPageIndex = 1 << 16;

  int RoundSizeDownToObjectAlignment(int size) override {
    if (base::bits::IsPowerOfTwo32(Map::kSize)) {
      return RoundDown(size, Map::kSize);
    } else {
      return (size / Map::kSize) * Map::kSize;
    }
  }

#ifdef VERIFY_HEAP
  void VerifyObject(HeapObject* obj) override;
#endif

 private:
  static const int kMapsPerPage = Page::kAllocatableMemory / Map::kSize;

  // Do map space compaction if there is a page gap.
  int CompactionThreshold() {
    return kMapsPerPage * (max_map_space_pages_ - 1);
  }

  const int max_map_space_pages_;
};


// -----------------------------------------------------------------------------
// Large objects ( > Page::kMaxRegularHeapObjectSize ) are allocated and
// managed by the large object space. A large object is allocated from OS
// heap with extra padding bytes (Page::kPageSize + Page::kObjectStartOffset).
// A large object always starts at Page::kObjectStartOffset to a page.
// Large objects do not move during garbage collections.

class LargeObjectSpace : public Space {
 public:
  LargeObjectSpace(Heap* heap, AllocationSpace id);
  virtual ~LargeObjectSpace();

  // Initializes internal data structures.
  bool SetUp();

  // Releases internal resources, frees objects in this space.
  void TearDown();

  static intptr_t ObjectSizeFor(intptr_t chunk_size) {
    if (chunk_size <= (Page::kPageSize + Page::kObjectStartOffset)) return 0;
    return chunk_size - Page::kPageSize - Page::kObjectStartOffset;
  }

  // Shared implementation of AllocateRaw, AllocateRawCode and
  // AllocateRawFixedArray.
  MUST_USE_RESULT AllocationResult
      AllocateRaw(int object_size, Executability executable);

  // Available bytes for objects in this space.
  inline intptr_t Available() override;

  intptr_t Size() override { return size_; }

  intptr_t SizeOfObjects() override { return objects_size_; }

  // Approximate amount of physical memory committed for this space.
  size_t CommittedPhysicalMemory() override;

  int PageCount() { return page_count_; }

  // Finds an object for a given address, returns a Smi if it is not found.
  // The function iterates through all objects in this space, may be slow.
  Object* FindObject(Address a);

  // Finds a large object page containing the given address, returns NULL
  // if such a page doesn't exist.
  LargePage* FindPage(Address a);

  // Clears the marking state of live objects.
  void ClearMarkingStateOfLiveObjects();

  // Frees unmarked objects.
  void FreeUnmarkedObjects();

  // Checks whether a heap object is in this space; O(1).
  bool Contains(HeapObject* obj);
  // Checks whether an address is in the object area in this space. Iterates
  // all objects in the space. May be slow.
  bool ContainsSlow(Address addr) { return FindObject(addr)->IsHeapObject(); }

  // Checks whether the space is empty.
  bool IsEmpty() { return first_page_ == NULL; }

  LargePage* first_page() { return first_page_; }

#ifdef VERIFY_HEAP
  virtual void Verify();
#endif

#ifdef DEBUG
  void Print() override;
  void ReportStatistics();
  void CollectCodeStatistics();
#endif

 private:
  // The head of the linked list of large object chunks.
  LargePage* first_page_;
  intptr_t size_;          // allocated bytes
  int page_count_;         // number of chunks
  intptr_t objects_size_;  // size of objects
  // Map MemoryChunk::kAlignment-aligned chunks to large pages covering them
  HashMap chunk_map_;

  friend class LargeObjectIterator;
};


class LargeObjectIterator : public ObjectIterator {
 public:
  explicit LargeObjectIterator(LargeObjectSpace* space);

  HeapObject* Next();

  // implementation of ObjectIterator.
  virtual HeapObject* next_object() { return Next(); }

 private:
  LargePage* current_;
};


// Iterates over the chunks (pages and large object pages) that can contain
// pointers to new space.
class PointerChunkIterator BASE_EMBEDDED {
 public:
  inline explicit PointerChunkIterator(Heap* heap);

  // Return NULL when the iterator is done.
  inline MemoryChunk* next();

 private:
  enum State { kOldSpaceState, kMapState, kLargeObjectState, kFinishedState };
  State state_;
  PageIterator old_iterator_;
  PageIterator map_iterator_;
  LargeObjectIterator lo_iterator_;
};


#ifdef DEBUG
struct CommentStatistic {
  const char* comment;
  int size;
  int count;
  void Clear() {
    comment = NULL;
    size = 0;
    count = 0;
  }
  // Must be small, since an iteration is used for lookup.
  static const int kMaxComments = 64;
};
#endif
}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_H_
