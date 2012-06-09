// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_SPACES_H_
#define V8_SPACES_H_

#include "allocation.h"
#include "hashmap.h"
#include "list.h"
#include "log.h"

namespace v8 {
namespace internal {

class Isolate;

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
// Page::kMaxHeapObjectSize, so that they do not have to move during
// collection. The large object space is paged. Pages in large object space
// may be larger than the page size.
//
// A store-buffer based write barrier is used to keep track of intergenerational
// references.  See store-buffer.h.
//
// During scavenges and mark-sweep collections we sometimes (after a store
// buffer overflow) iterate intergenerational pointers without decoding heap
// object maps so if the page belongs to old pointer space or large object
// space it is essential to guarantee that the page does not contain any
// garbage pointers to new space: every pointer aligned word which satisfies
// the Heap::InNewSpace() predicate must be a pointer to a live heap object in
// new space. Thus objects in old pointer and large object spaces should have a
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

#define ASSERT_PAGE_ALIGNED(address)                                           \
  ASSERT((OffsetFrom(address) & Page::kPageAlignmentMask) == 0)

#define ASSERT_OBJECT_ALIGNED(address)                                         \
  ASSERT((OffsetFrom(address) & kObjectAlignmentMask) == 0)

#define ASSERT_MAP_ALIGNED(address)                                            \
  ASSERT((OffsetFrom(address) & kMapAlignmentMask) == 0)

#define ASSERT_OBJECT_SIZE(size)                                               \
  ASSERT((0 < size) && (size <= Page::kMaxNonCodeHeapObjectSize))

#define ASSERT_PAGE_OFFSET(offset)                                             \
  ASSERT((Page::kObjectStartOffset <= offset)                                  \
      && (offset <= Page::kPageSize))

#define ASSERT_MAP_PAGE_INDEX(index)                                           \
  ASSERT((0 <= index) && (index <= MapSpace::kMaxMapPageIndex))


class PagedSpace;
class MemoryAllocator;
class AllocationInfo;
class Space;
class FreeList;
class MemoryChunk;

class MarkBit {
 public:
  typedef uint32_t CellType;

  inline MarkBit(CellType* cell, CellType mask, bool data_only)
      : cell_(cell), mask_(mask), data_only_(data_only) { }

  inline CellType* cell() { return cell_; }
  inline CellType mask() { return mask_; }

#ifdef DEBUG
  bool operator==(const MarkBit& other) {
    return cell_ == other.cell_ && mask_ == other.mask_;
  }
#endif

  inline void Set() { *cell_ |= mask_; }
  inline bool Get() { return (*cell_ & mask_) != 0; }
  inline void Clear() { *cell_ &= ~mask_; }

  inline bool data_only() { return data_only_; }

  inline MarkBit Next() {
    CellType new_mask = mask_ << 1;
    if (new_mask == 0) {
      return MarkBit(cell_ + 1, 1, data_only_);
    } else {
      return MarkBit(cell_, new_mask, data_only_);
    }
  }

 private:
  CellType* cell_;
  CellType mask_;
  // This boolean indicates that the object is in a data-only space with no
  // pointers.  This enables some optimizations when marking.
  // It is expected that this field is inlined and turned into control flow
  // at the place where the MarkBit object is created.
  bool data_only_;
};


// Bitmap is a sequence of cells each containing fixed number of bits.
class Bitmap {
 public:
  static const uint32_t kBitsPerCell = 32;
  static const uint32_t kBitsPerCellLog2 = 5;
  static const uint32_t kBitIndexMask = kBitsPerCell - 1;
  static const uint32_t kBytesPerCell = kBitsPerCell / kBitsPerByte;
  static const uint32_t kBytesPerCellLog2 = kBitsPerCellLog2 - kBitsPerByteLog2;

  static const size_t kLength =
    (1 << kPageSizeBits) >> (kPointerSizeLog2);

  static const size_t kSize =
    (1 << kPageSizeBits) >> (kPointerSizeLog2 + kBitsPerByteLog2);


  static int CellsForLength(int length) {
    return (length + kBitsPerCell - 1) >> kBitsPerCellLog2;
  }

  int CellsCount() {
    return CellsForLength(kLength);
  }

  static int SizeFor(int cells_count) {
    return sizeof(MarkBit::CellType) * cells_count;
  }

  INLINE(static uint32_t IndexToCell(uint32_t index)) {
    return index >> kBitsPerCellLog2;
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

  INLINE(Address address()) {
    return reinterpret_cast<Address>(this);
  }

  INLINE(static Bitmap* FromAddress(Address addr)) {
    return reinterpret_cast<Bitmap*>(addr);
  }

  inline MarkBit MarkBitFromIndex(uint32_t index, bool data_only = false) {
    MarkBit::CellType mask = 1 << (index & kBitIndexMask);
    MarkBit::CellType* cell = this->cells() + (index >> kBitsPerCellLog2);
    return MarkBit(cell, mask, data_only);
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
    CellPrinter() : seq_start(0), seq_type(0), seq_length(0) { }

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
        PrintF("%d: %dx%d\n",
               seq_start,
               seq_type == 0 ? 0 : 1,
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
      if (cells()[i] != 0) return false;
    }
    return true;
  }
};


class SkipList;
class SlotsBuffer;

// MemoryChunk represents a memory region owned by a specific space.
// It is divided into the header and the body. Chunk start is always
// 1MB aligned. Start of the body is aligned so it can accommodate
// any heap object.
class MemoryChunk {
 public:
  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static MemoryChunk* FromAddress(Address a) {
    return reinterpret_cast<MemoryChunk*>(OffsetFrom(a) & ~kAlignmentMask);
  }

  // Only works for addresses in pointer spaces, not data or code spaces.
  static inline MemoryChunk* FromAnyPointerAddress(Address addr);

  Address address() { return reinterpret_cast<Address>(this); }

  bool is_valid() { return address() != NULL; }

  MemoryChunk* next_chunk() const { return next_chunk_; }
  MemoryChunk* prev_chunk() const { return prev_chunk_; }

  void set_next_chunk(MemoryChunk* next) { next_chunk_ = next; }
  void set_prev_chunk(MemoryChunk* prev) { prev_chunk_ = prev; }

  Space* owner() const {
    if ((reinterpret_cast<intptr_t>(owner_) & kFailureTagMask) ==
        kFailureTag) {
      return reinterpret_cast<Space*>(owner_ - kFailureTag);
    } else {
      return NULL;
    }
  }

  void set_owner(Space* space) {
    ASSERT((reinterpret_cast<intptr_t>(space) & kFailureTagMask) == 0);
    owner_ = reinterpret_cast<Address>(space) + kFailureTag;
    ASSERT((reinterpret_cast<intptr_t>(owner_) & kFailureTagMask) ==
           kFailureTag);
  }

  VirtualMemory* reserved_memory() {
    return &reservation_;
  }

  void InitializeReservedMemory() {
    reservation_.Reset();
  }

  void set_reserved_memory(VirtualMemory* reservation) {
    ASSERT_NOT_NULL(reservation);
    reservation_.TakeControl(reservation);
  }

  bool scan_on_scavenge() { return IsFlagSet(SCAN_ON_SCAVENGE); }
  void initialize_scan_on_scavenge(bool scan) {
    if (scan) {
      SetFlag(SCAN_ON_SCAVENGE);
    } else {
      ClearFlag(SCAN_ON_SCAVENGE);
    }
  }
  inline void set_scan_on_scavenge(bool scan);

  int store_buffer_counter() { return store_buffer_counter_; }
  void set_store_buffer_counter(int counter) {
    store_buffer_counter_ = counter;
  }

  bool Contains(Address addr) {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether addr can be a limit of addresses in this page.
  // It's a limit if it's in the page, or if it's just after the
  // last byte of the page.
  bool ContainsLimit(Address addr) {
    return addr >= area_start() && addr <= area_end();
  }

  enum MemoryChunkFlags {
    IS_EXECUTABLE,
    ABOUT_TO_BE_FREED,
    POINTERS_TO_HERE_ARE_INTERESTING,
    POINTERS_FROM_HERE_ARE_INTERESTING,
    SCAN_ON_SCAVENGE,
    IN_FROM_SPACE,  // Mutually exclusive with IN_TO_SPACE.
    IN_TO_SPACE,    // All pages in new space has one of these two set.
    NEW_SPACE_BELOW_AGE_MARK,
    CONTAINS_ONLY_DATA,
    EVACUATION_CANDIDATE,
    RESCAN_ON_EVACUATION,

    // Pages swept precisely can be iterated, hitting only the live objects.
    // Whereas those swept conservatively cannot be iterated over. Both flags
    // indicate that marking bits have been cleared by the sweeper, otherwise
    // marking bits are still intact.
    WAS_SWEPT_PRECISELY,
    WAS_SWEPT_CONSERVATIVELY,

    // Last flag, keep at bottom.
    NUM_MEMORY_CHUNK_FLAGS
  };


  static const int kPointersToHereAreInterestingMask =
      1 << POINTERS_TO_HERE_ARE_INTERESTING;

  static const int kPointersFromHereAreInterestingMask =
      1 << POINTERS_FROM_HERE_ARE_INTERESTING;

  static const int kEvacuationCandidateMask =
      1 << EVACUATION_CANDIDATE;

  static const int kSkipEvacuationSlotsRecordingMask =
      (1 << EVACUATION_CANDIDATE) |
      (1 << RESCAN_ON_EVACUATION) |
      (1 << IN_FROM_SPACE) |
      (1 << IN_TO_SPACE);


  void SetFlag(int flag) {
    flags_ |= static_cast<uintptr_t>(1) << flag;
  }

  void ClearFlag(int flag) {
    flags_ &= ~(static_cast<uintptr_t>(1) << flag);
  }

  void SetFlagTo(int flag, bool value) {
    if (value) {
      SetFlag(flag);
    } else {
      ClearFlag(flag);
    }
  }

  bool IsFlagSet(int flag) {
    return (flags_ & (static_cast<uintptr_t>(1) << flag)) != 0;
  }

  // Set or clear multiple flags at a time. The flags in the mask
  // are set to the value in "flags", the rest retain the current value
  // in flags_.
  void SetFlags(intptr_t flags, intptr_t mask) {
    flags_ = (flags_ & ~mask) | (flags & mask);
  }

  // Return all current flags.
  intptr_t GetFlags() { return flags_; }

  // Manage live byte count (count of bytes known to be live,
  // because they are marked black).
  void ResetLiveBytes() {
    if (FLAG_gc_verbose) {
      PrintF("ResetLiveBytes:%p:%x->0\n",
             static_cast<void*>(this), live_byte_count_);
    }
    live_byte_count_ = 0;
  }
  void IncrementLiveBytes(int by) {
    if (FLAG_gc_verbose) {
      printf("UpdateLiveBytes:%p:%x%c=%x->%x\n",
             static_cast<void*>(this), live_byte_count_,
             ((by < 0) ? '-' : '+'), ((by < 0) ? -by : by),
             live_byte_count_ + by);
    }
    live_byte_count_ += by;
    ASSERT_LE(static_cast<unsigned>(live_byte_count_), size_);
  }
  int LiveBytes() {
    ASSERT(static_cast<unsigned>(live_byte_count_) <= size_);
    return live_byte_count_;
  }

  static void IncrementLiveBytesFromGC(Address address, int by) {
    MemoryChunk::FromAddress(address)->IncrementLiveBytes(by);
  }

  static void IncrementLiveBytesFromMutator(Address address, int by);

  static const intptr_t kAlignment =
      (static_cast<uintptr_t>(1) << kPageSizeBits);

  static const intptr_t kAlignmentMask = kAlignment - 1;

  static const intptr_t kSizeOffset = kPointerSize + kPointerSize;

  static const intptr_t kLiveBytesOffset =
     kSizeOffset + kPointerSize + kPointerSize + kPointerSize +
     kPointerSize + kPointerSize +
     kPointerSize + kPointerSize + kPointerSize + kIntSize;

  static const size_t kSlotsBufferOffset = kLiveBytesOffset + kIntSize;

  static const size_t kHeaderSize =
      kSlotsBufferOffset + kPointerSize + kPointerSize;

  static const int kBodyOffset =
    CODE_POINTER_ALIGN(MAP_POINTER_ALIGN(kHeaderSize + Bitmap::kSize));

  // The start offset of the object area in a page. Aligned to both maps and
  // code alignment to be suitable for both.  Also aligned to 32 words because
  // the marking bitmap is arranged in 32 bit chunks.
  static const int kObjectStartAlignment = 32 * kPointerSize;
  static const int kObjectStartOffset = kBodyOffset - 1 +
      (kObjectStartAlignment - (kBodyOffset - 1) % kObjectStartAlignment);

  size_t size() const { return size_; }

  void set_size(size_t size) {
    size_ = size;
  }

  void SetArea(Address area_start, Address area_end) {
    area_start_ = area_start;
    area_end_ = area_end;
  }

  Executability executable() {
    return IsFlagSet(IS_EXECUTABLE) ? EXECUTABLE : NOT_EXECUTABLE;
  }

  bool ContainsOnlyData() {
    return IsFlagSet(CONTAINS_ONLY_DATA);
  }

  bool InNewSpace() {
    return (flags_ & ((1 << IN_FROM_SPACE) | (1 << IN_TO_SPACE))) != 0;
  }

  bool InToSpace() {
    return IsFlagSet(IN_TO_SPACE);
  }

  bool InFromSpace() {
    return IsFlagSet(IN_FROM_SPACE);
  }

  // ---------------------------------------------------------------------
  // Markbits support

  inline Bitmap* markbits() {
    return Bitmap::FromAddress(address() + kHeaderSize);
  }

  void PrintMarkbits() { markbits()->Print(); }

  inline uint32_t AddressToMarkbitIndex(Address addr) {
    return static_cast<uint32_t>(addr - this->address()) >> kPointerSizeLog2;
  }

  inline static uint32_t FastAddressToMarkbitIndex(Address addr) {
    const intptr_t offset =
        reinterpret_cast<intptr_t>(addr) & kAlignmentMask;

    return static_cast<uint32_t>(offset) >> kPointerSizeLog2;
  }

  inline Address MarkbitIndexToAddress(uint32_t index) {
    return this->address() + (index << kPointerSizeLog2);
  }

  void InsertAfter(MemoryChunk* other);
  void Unlink();

  inline Heap* heap() { return heap_; }

  static const int kFlagsOffset = kPointerSize * 3;

  bool IsEvacuationCandidate() { return IsFlagSet(EVACUATION_CANDIDATE); }

  bool ShouldSkipEvacuationSlotRecording() {
    return (flags_ & kSkipEvacuationSlotsRecordingMask) != 0;
  }

  inline SkipList* skip_list() {
    return skip_list_;
  }

  inline void set_skip_list(SkipList* skip_list) {
    skip_list_ = skip_list;
  }

  inline SlotsBuffer* slots_buffer() {
    return slots_buffer_;
  }

  inline SlotsBuffer** slots_buffer_address() {
    return &slots_buffer_;
  }

  void MarkEvacuationCandidate() {
    ASSERT(slots_buffer_ == NULL);
    SetFlag(EVACUATION_CANDIDATE);
  }

  void ClearEvacuationCandidate() {
    ASSERT(slots_buffer_ == NULL);
    ClearFlag(EVACUATION_CANDIDATE);
  }

  Address area_start() { return area_start_; }
  Address area_end() { return area_end_; }
  int area_size() {
    return static_cast<int>(area_end() - area_start());
  }

 protected:
  MemoryChunk* next_chunk_;
  MemoryChunk* prev_chunk_;
  size_t size_;
  intptr_t flags_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;
  Address area_end_;

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;
  // The identity of the owning space.  This is tagged as a failure pointer, but
  // no failure can be in an object, so this can be distinguished from any entry
  // in a fixed array.
  Address owner_;
  Heap* heap_;
  // Used by the store buffer to keep track of which pages to mark scan-on-
  // scavenge.
  int store_buffer_counter_;
  // Count of bytes marked black on page.
  int live_byte_count_;
  SlotsBuffer* slots_buffer_;
  SkipList* skip_list_;

  static MemoryChunk* Initialize(Heap* heap,
                                 Address base,
                                 size_t size,
                                 Address area_start,
                                 Address area_end,
                                 Executability executable,
                                 Space* owner);

  friend class MemoryAllocator;
};

STATIC_CHECK(sizeof(MemoryChunk) <= MemoryChunk::kHeaderSize);

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

  // Returns the page containing an allocation top. Because an allocation
  // top address can be the upper bound of the page, we need to subtract
  // it with kPointerSize first. The address ranges from
  // [page_addr + kObjectStartOffset .. page_addr + kPageSize].
  INLINE(static Page* FromAllocationTop(Address top)) {
    Page* p = FromAddress(top - kPointerSize);
    return p;
  }

  // Returns the next page in the chain of pages owned by a space.
  inline Page* next_page();
  inline Page* prev_page();
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
    ASSERT_PAGE_OFFSET(offset);
    return address() + offset;
  }

  // ---------------------------------------------------------------------

  // Page size in bytes.  This must be a multiple of the OS page size.
  static const int kPageSize = 1 << kPageSizeBits;

  // Object area size in bytes.
  static const int kNonCodeObjectAreaSize = kPageSize - kObjectStartOffset;

  // Maximum object size that fits in a page.
  static const int kMaxNonCodeHeapObjectSize = kNonCodeObjectAreaSize;

  // Page size mask.
  static const intptr_t kPageAlignmentMask = (1 << kPageSizeBits) - 1;

  inline void ClearGCFields();

  static inline Page* Initialize(Heap* heap,
                                 MemoryChunk* chunk,
                                 Executability executable,
                                 PagedSpace* owner);

  void InitializeAsAnchor(PagedSpace* owner);

  bool WasSweptPrecisely() { return IsFlagSet(WAS_SWEPT_PRECISELY); }
  bool WasSweptConservatively() { return IsFlagSet(WAS_SWEPT_CONSERVATIVELY); }
  bool WasSwept() { return WasSweptPrecisely() || WasSweptConservatively(); }

  void MarkSweptPrecisely() { SetFlag(WAS_SWEPT_PRECISELY); }
  void MarkSweptConservatively() { SetFlag(WAS_SWEPT_CONSERVATIVELY); }

  void ClearSweptPrecisely() { ClearFlag(WAS_SWEPT_PRECISELY); }
  void ClearSweptConservatively() { ClearFlag(WAS_SWEPT_CONSERVATIVELY); }

#ifdef DEBUG
  void Print();
#endif  // DEBUG

  friend class MemoryAllocator;
};


STATIC_CHECK(sizeof(Page) <= MemoryChunk::kHeaderSize);


class LargePage : public MemoryChunk {
 public:
  HeapObject* GetObject() {
    return HeapObject::FromAddress(area_start());
  }

  inline LargePage* next_page() const {
    return static_cast<LargePage*>(next_chunk());
  }

  inline void set_next_page(LargePage* page) {
    set_next_chunk(page);
  }
 private:
  static inline LargePage* Initialize(Heap* heap, MemoryChunk* chunk);

  friend class MemoryAllocator;
};

STATIC_CHECK(sizeof(LargePage) <= MemoryChunk::kHeaderSize);

// ----------------------------------------------------------------------------
// Space is the abstract superclass for all allocation spaces.
class Space : public Malloced {
 public:
  Space(Heap* heap, AllocationSpace id, Executability executable)
      : heap_(heap), id_(id), executable_(executable) {}

  virtual ~Space() {}

  Heap* heap() const { return heap_; }

  // Does the space need executable memory?
  Executability executable() { return executable_; }

  // Identity used in error reporting.
  AllocationSpace identity() { return id_; }

  // Returns allocated size.
  virtual intptr_t Size() = 0;

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see LargeObjectSpace).
  virtual intptr_t SizeOfObjects() { return Size(); }

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

  // After calling this we can allocate a certain number of bytes using only
  // linear allocation (with a LinearAllocationScope and an AlwaysAllocateScope)
  // without using freelists or causing a GC.  This is used by partial
  // snapshots.  It returns true of space was reserved or false if a GC is
  // needed.  For paged spaces the space requested must include the space wasted
  // at the end of each when allocating linearly.
  virtual bool ReserveSpace(int bytes) = 0;

 private:
  Heap* heap_;
  AllocationSpace id_;
  Executability executable_;
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
  bool SetUp(const size_t requested_size);

  // Frees the range of virtual memory, and frees the data structures used to
  // manage it.
  void TearDown();

  bool exists() { return this != NULL && code_range_ != NULL; }
  bool contains(Address address) {
    if (this == NULL || code_range_ == NULL) return false;
    Address start = static_cast<Address>(code_range_->address());
    return start <= address && address < start + code_range_->size();
  }

  // Allocates a chunk of memory from the large-object portion of
  // the code range.  On platforms with no separate code range, should
  // not be called.
  MUST_USE_RESULT Address AllocateRawMemory(const size_t requested,
                                            size_t* allocated);
  void FreeRawMemory(Address buf, size_t length);

 private:
  Isolate* isolate_;

  // The reserved range of virtual memory that all code objects are put in.
  VirtualMemory* code_range_;
  // Plain old data class, just a struct plus a constructor.
  class FreeBlock {
   public:
    FreeBlock(Address start_arg, size_t size_arg)
        : start(start_arg), size(size_arg) {
      ASSERT(IsAddressAligned(start, MemoryChunk::kAlignment));
      ASSERT(size >= static_cast<size_t>(Page::kPageSize));
    }
    FreeBlock(void* start_arg, size_t size_arg)
        : start(static_cast<Address>(start_arg)), size(size_arg) {
      ASSERT(IsAddressAligned(start, MemoryChunk::kAlignment));
      ASSERT(size >= static_cast<size_t>(Page::kPageSize));
    }

    Address start;
    size_t size;
  };

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
  // If none can be found, terminates V8 with FatalProcessOutOfMemory.
  void GetNextAllocationBlock(size_t requested);
  // Compares the start addresses of two free blocks.
  static int CompareFreeBlockAddress(const FreeBlock* left,
                                     const FreeBlock* right);

  DISALLOW_COPY_AND_ASSIGN(CodeRange);
};


class SkipList {
 public:
  SkipList() {
    Clear();
  }

  void Clear() {
    for (int idx = 0; idx < kSize; idx++) {
      starts_[idx] = reinterpret_cast<Address>(-1);
    }
  }

  Address StartFor(Address addr) {
    return starts_[RegionNumber(addr)];
  }

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

  Page* AllocatePage(PagedSpace* owner, Executability executable);

  LargePage* AllocateLargePage(intptr_t object_size,
                                      Executability executable,
                                      Space* owner);

  void Free(MemoryChunk* chunk);

  // Returns the maximum available bytes of heaps.
  intptr_t Available() { return capacity_ < size_ ? 0 : capacity_ - size_; }

  // Returns allocated spaces in bytes.
  intptr_t Size() { return size_; }

  // Returns the maximum available executable bytes of heaps.
  intptr_t AvailableExecutable() {
    if (capacity_executable_ < size_executable_) return 0;
    return capacity_executable_ - size_executable_;
  }

  // Returns allocated executable spaces in bytes.
  intptr_t SizeExecutable() { return size_executable_; }

  // Returns maximum available bytes that the old space can have.
  intptr_t MaxAvailable() {
    return (Available() / Page::kPageSize) * Page::kMaxNonCodeHeapObjectSize;
  }

#ifdef DEBUG
  // Reports statistic info of the space.
  void ReportStatistics();
#endif

  MemoryChunk* AllocateChunk(intptr_t body_size,
                             Executability executable,
                             Space* space);

  Address ReserveAlignedMemory(size_t requested,
                               size_t alignment,
                               VirtualMemory* controller);
  Address AllocateAlignedMemory(size_t requested,
                                size_t alignment,
                                Executability executable,
                                VirtualMemory* controller);

  void FreeMemory(VirtualMemory* reservation, Executability executable);
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

  void PerformAllocationCallback(ObjectSpace space,
                                 AllocationAction action,
                                 size_t size);

  void AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                          ObjectSpace space,
                                          AllocationAction action);

  void RemoveMemoryAllocationCallback(
      MemoryAllocationCallback callback);

  bool MemoryAllocationCallbackRegistered(
      MemoryAllocationCallback callback);

  static int CodePageGuardStartOffset();

  static int CodePageGuardSize();

  static int CodePageAreaStartOffset();

  static int CodePageAreaEndOffset();

  static int CodePageAreaSize() {
    return CodePageAreaEndOffset() - CodePageAreaStartOffset();
  }

  MUST_USE_RESULT static bool CommitCodePage(VirtualMemory* vm,
                                             Address start,
                                             size_t size);

 private:
  Isolate* isolate_;

  // Maximum space size in bytes.
  size_t capacity_;
  // Maximum subset of capacity_ that can be executable
  size_t capacity_executable_;

  // Allocated space size in bytes.
  size_t size_;
  // Allocated executable space size in bytes.
  size_t size_executable_;

  struct MemoryAllocationCallbackRegistration {
    MemoryAllocationCallbackRegistration(MemoryAllocationCallback callback,
                                         ObjectSpace space,
                                         AllocationAction action)
        : callback(callback), space(space), action(action) {
    }
    MemoryAllocationCallback callback;
    ObjectSpace space;
    AllocationAction action;
  };

  // A List of callback that are triggered when memory is allocated or free'd
  List<MemoryAllocationCallbackRegistration>
      memory_allocation_callbacks_;

  // Initializes pages in a chunk. Returns the first page address.
  // This function and GetChunkId() are provided for the mark-compact
  // collector to rebuild page headers in the from space, which is
  // used as a marking stack and its page headers are destroyed.
  Page* InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                               PagedSpace* owner);

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
  virtual ~ObjectIterator() { }

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
class HeapObjectIterator: public ObjectIterator {
 public:
  // Creates a new object iterator in a given space.
  // If the size function is not given, the iterator calls the default
  // Object::Size().
  explicit HeapObjectIterator(PagedSpace* space);
  HeapObjectIterator(PagedSpace* space, HeapObjectCallback size_func);
  HeapObjectIterator(Page* page, HeapObjectCallback size_func);

  // Advance to the next object, skipping free spaces and other fillers and
  // skipping the special garbage section of which there is one per space.
  // Returns NULL when the iteration has ended.
  inline HeapObject* Next() {
    do {
      HeapObject* next_obj = FromCurrentPage();
      if (next_obj != NULL) return next_obj;
    } while (AdvanceToNextPage());
    return NULL;
  }

  virtual HeapObject* next_object() {
    return Next();
  }

 private:
  enum PageMode { kOnePageOnly, kAllPagesInSpace };

  Address cur_addr_;  // Current iteration point.
  Address cur_end_;   // End iteration point.
  HeapObjectCallback size_func_;  // Size function or NULL.
  PagedSpace* space_;
  PageMode page_mode_;

  // Fast (inlined) path of next().
  inline HeapObject* FromCurrentPage();

  // Slow path of next(), goes into the next page.  Returns false if the
  // iteration has ended.
  bool AdvanceToNextPage();

  // Initializes fields.
  inline void Initialize(PagedSpace* owner,
                         Address start,
                         Address end,
                         PageMode mode,
                         HeapObjectCallback size_func);
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
  AllocationInfo() : top(NULL), limit(NULL) {
  }

  Address top;  // Current allocation top.
  Address limit;  // Current allocation limit.

#ifdef DEBUG
  bool VerifyPagedAllocation() {
    return (Page::FromAllocationTop(top) == Page::FromAllocationTop(limit))
        && (top <= limit);
  }
#endif
};


// An abstraction of the accounting statistics of a page-structured space.
// The 'capacity' of a space is the number of object-area bytes (i.e., not
// including page bookkeeping structures) currently in the space. The 'size'
// of a space is the number of allocated bytes, the 'waste' in the space is
// the number of bytes that are not allocated and not available to
// allocation without reorganizing the space via a GC (e.g. small blocks due
// to internal fragmentation, top of page areas in map space), and the bytes
// 'available' is the number of unallocated bytes that are not waste.  The
// capacity is the sum of size, waste, and available.
//
// The stats are only set by functions that ensure they stay balanced. These
// functions increase or decrease one of the non-capacity stats in
// conjunction with capacity, or else they always balance increases and
// decreases to the non-capacity stats.
class AllocationStats BASE_EMBEDDED {
 public:
  AllocationStats() { Clear(); }

  // Zero out all the allocation statistics (i.e., no capacity).
  void Clear() {
    capacity_ = 0;
    size_ = 0;
    waste_ = 0;
  }

  void ClearSizeWaste() {
    size_ = capacity_;
    waste_ = 0;
  }

  // Reset the allocation statistics (i.e., available = capacity with no
  // wasted or allocated bytes).
  void Reset() {
    size_ = 0;
    waste_ = 0;
  }

  // Accessors for the allocation statistics.
  intptr_t Capacity() { return capacity_; }
  intptr_t Size() { return size_; }
  intptr_t Waste() { return waste_; }

  // Grow the space by adding available bytes.  They are initially marked as
  // being in use (part of the size), but will normally be immediately freed,
  // putting them on the free list and removing them from size_.
  void ExpandSpace(int size_in_bytes) {
    capacity_ += size_in_bytes;
    size_ += size_in_bytes;
    ASSERT(size_ >= 0);
  }

  // Shrink the space by removing available bytes.  Since shrinking is done
  // during sweeping, bytes have been marked as being in use (part of the size)
  // and are hereby freed.
  void ShrinkSpace(int size_in_bytes) {
    capacity_ -= size_in_bytes;
    size_ -= size_in_bytes;
    ASSERT(size_ >= 0);
  }

  // Allocate from available bytes (available -> size).
  void AllocateBytes(intptr_t size_in_bytes) {
    size_ += size_in_bytes;
    ASSERT(size_ >= 0);
  }

  // Free allocated bytes, making them available (size -> available).
  void DeallocateBytes(intptr_t size_in_bytes) {
    size_ -= size_in_bytes;
    ASSERT(size_ >= 0);
  }

  // Waste free bytes (available -> waste).
  void WasteBytes(int size_in_bytes) {
    size_ -= size_in_bytes;
    waste_ += size_in_bytes;
    ASSERT(size_ >= 0);
  }

 private:
  intptr_t capacity_;
  intptr_t size_;
  intptr_t waste_;
};


// -----------------------------------------------------------------------------
// Free lists for old object spaces
//
// Free-list nodes are free blocks in the heap.  They look like heap objects
// (free-list node pointers have the heap object tag, and they have a map like
// a heap object).  They have a size and a next pointer.  The next pointer is
// the raw address of the next free list node (or NULL).
class FreeListNode: public HeapObject {
 public:
  // Obtain a free-list node from a raw address.  This is not a cast because
  // it does not check nor require that the first word at the address is a map
  // pointer.
  static FreeListNode* FromAddress(Address address) {
    return reinterpret_cast<FreeListNode*>(HeapObject::FromAddress(address));
  }

  static inline bool IsFreeListNode(HeapObject* object);

  // Set the size in bytes, which can be read with HeapObject::Size().  This
  // function also writes a map to the first word of the block so that it
  // looks like a heap object to the garbage collector and heap iteration
  // functions.
  void set_size(Heap* heap, int size_in_bytes);

  // Accessors for the next field.
  inline FreeListNode* next();
  inline FreeListNode** next_address();
  inline void set_next(FreeListNode* next);

  inline void Zap();

 private:
  static const int kNextOffset = POINTER_SIZE_ALIGN(FreeSpace::kHeaderSize);

  DISALLOW_IMPLICIT_CONSTRUCTORS(FreeListNode);
};


// The free list for the old space.  The free list is organized in such a way
// as to encourage objects allocated around the same time to be near each
// other.  The normal way to allocate is intended to be by bumping a 'top'
// pointer until it hits a 'limit' pointer.  When the limit is hit we need to
// find a new space to allocate from.  This is done with the free list, which
// is divided up into rough categories to cut down on waste.  Having finer
// categories would scatter allocation more.

// The old space free list is organized in categories.
// 1-31 words:  Such small free areas are discarded for efficiency reasons.
//     They can be reclaimed by the compactor.  However the distance between top
//     and limit may be this small.
// 32-255 words: There is a list of spaces this large.  It is used for top and
//     limit when the object we need to allocate is 1-31 words in size.  These
//     spaces are called small.
// 256-2047 words: There is a list of spaces this large.  It is used for top and
//     limit when the object we need to allocate is 32-255 words in size.  These
//     spaces are called medium.
// 1048-16383 words: There is a list of spaces this large.  It is used for top
//     and limit when the object we need to allocate is 256-2047 words in size.
//     These spaces are call large.
// At least 16384 words.  This list is for objects of 2048 words or larger.
//     Empty pages are added to this list.  These spaces are called huge.
class FreeList BASE_EMBEDDED {
 public:
  explicit FreeList(PagedSpace* owner);

  // Clear the free list.
  void Reset();

  // Return the number of bytes available on the free list.
  intptr_t available() { return available_; }

  // Place a node on the free list.  The block of size 'size_in_bytes'
  // starting at 'start' is placed on the free list.  The return value is the
  // number of bytes that have been lost due to internal fragmentation by
  // freeing the block.  Bookkeeping information will be written to the block,
  // i.e., its contents will be destroyed.  The start address should be word
  // aligned, and the size should be a non-zero multiple of the word size.
  int Free(Address start, int size_in_bytes);

  // Allocate a block of size 'size_in_bytes' from the free list.  The block
  // is unitialized.  A failure is returned if no block is available.  The
  // number of bytes lost to fragmentation is returned in the output parameter
  // 'wasted_bytes'.  The size should be a non-zero multiple of the word size.
  MUST_USE_RESULT HeapObject* Allocate(int size_in_bytes);

#ifdef DEBUG
  void Zap();
  static intptr_t SumFreeList(FreeListNode* node);
  static int FreeListLength(FreeListNode* cur);
  intptr_t SumFreeLists();
  bool IsVeryLong();
#endif

  struct SizeStats {
    intptr_t Total() {
      return small_size_ + medium_size_ + large_size_ + huge_size_;
    }

    intptr_t small_size_;
    intptr_t medium_size_;
    intptr_t large_size_;
    intptr_t huge_size_;
  };

  void CountFreeListItems(Page* p, SizeStats* sizes);

  intptr_t EvictFreeListItems(Page* p);

 private:
  // The size range of blocks, in bytes.
  static const int kMinBlockSize = 3 * kPointerSize;
  static const int kMaxBlockSize = Page::kMaxNonCodeHeapObjectSize;

  FreeListNode* PickNodeFromList(FreeListNode** list, int* node_size);

  FreeListNode* FindNodeFor(int size_in_bytes, int* node_size);

  PagedSpace* owner_;
  Heap* heap_;

  // Total available bytes in all blocks on this free list.
  int available_;

  static const int kSmallListMin = 0x20 * kPointerSize;
  static const int kSmallListMax = 0xff * kPointerSize;
  static const int kMediumListMax = 0x7ff * kPointerSize;
  static const int kLargeListMax = 0x3fff * kPointerSize;
  static const int kSmallAllocationMax = kSmallListMin - kPointerSize;
  static const int kMediumAllocationMax = kSmallListMax;
  static const int kLargeAllocationMax = kMediumListMax;
  FreeListNode* small_list_;
  FreeListNode* medium_list_;
  FreeListNode* large_list_;
  FreeListNode* huge_list_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FreeList);
};


class PagedSpace : public Space {
 public:
  // Creates a space with a maximum capacity, and an id.
  PagedSpace(Heap* heap,
             intptr_t max_capacity,
             AllocationSpace id,
             Executability executable);

  virtual ~PagedSpace() {}

  // Set up the space using the given address range of virtual memory (from
  // the memory allocator's initial chunk) if possible.  If the block of
  // addresses is not big enough to contain a single page-aligned page, a
  // fresh chunk will be allocated.
  bool SetUp();

  // Returns true if the space has been successfully set up and not
  // subsequently torn down.
  bool HasBeenSetUp();

  // Cleans up the space, frees all pages in this space except those belonging
  // to the initial chunk, uncommits addresses in the initial chunk.
  void TearDown();

  // Checks whether an object/address is in this space.
  inline bool Contains(Address a);
  bool Contains(HeapObject* o) { return Contains(o->address()); }

  // Given an address occupied by a live object, return that object if it is
  // in this space, or Failure::Exception() if it is not. The implementation
  // iterates over objects in the page containing the address, the cost is
  // linear in the number of objects in the page. It may be slow.
  MUST_USE_RESULT MaybeObject* FindObject(Address addr);

  // Prepares for a mark-compact GC.
  virtual void PrepareForMarkCompact();

  // Current capacity without growing (Size() + Available()).
  intptr_t Capacity() { return accounting_stats_.Capacity(); }

  // Total amount of memory committed for this space.  For paged
  // spaces this equals the capacity.
  intptr_t CommittedMemory() { return Capacity(); }

  // Sets the capacity, the available space and the wasted space to zero.
  // The stats are rebuilt during sweeping by adding each page to the
  // capacity and the size when it is encountered.  As free spaces are
  // discovered during the sweeping they are subtracted from the size and added
  // to the available and wasted totals.
  void ClearStats() {
    accounting_stats_.ClearSizeWaste();
  }

  // Available bytes without growing.  These are the bytes on the free list.
  // The bytes in the linear allocation area are not included in this total
  // because updating the stats would slow down allocation.  New pages are
  // immediately added to the free list so they show up here.
  intptr_t Available() { return free_list_.available(); }

  // Allocated bytes in this space.  Garbage bytes that were not found due to
  // lazy sweeping are counted as being allocated!  The bytes in the current
  // linear allocation area (between top and limit) are also counted here.
  virtual intptr_t Size() { return accounting_stats_.Size(); }

  // As size, but the bytes in lazily swept pages are estimated and the bytes
  // in the current linear allocation area are not included.
  virtual intptr_t SizeOfObjects() {
    ASSERT(!IsSweepingComplete() || (unswept_free_bytes_ == 0));
    return Size() - unswept_free_bytes_ - (limit() - top());
  }

  // Wasted bytes in this space.  These are just the bytes that were thrown away
  // due to being too small to use for allocation.  They do not include the
  // free bytes that were not found at all due to lazy sweeping.
  virtual intptr_t Waste() { return accounting_stats_.Waste(); }

  // Returns the allocation pointer in this space.
  Address top() { return allocation_info_.top; }
  Address limit() { return allocation_info_.limit; }

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not.
  MUST_USE_RESULT inline MaybeObject* AllocateRaw(int size_in_bytes);

  virtual bool ReserveSpace(int bytes);

  // Give a block of memory to the space's free list.  It might be added to
  // the free list or accounted as waste.
  // If add_to_freelist is false then just accounting stats are updated and
  // no attempt to add area to free list is made.
  int Free(Address start, int size_in_bytes) {
    int wasted = free_list_.Free(start, size_in_bytes);
    accounting_stats_.DeallocateBytes(size_in_bytes - wasted);
    return size_in_bytes - wasted;
  }

  // Set space allocation info.
  void SetTop(Address top, Address limit) {
    ASSERT(top == limit ||
           Page::FromAddress(top) == Page::FromAddress(limit - 1));
    allocation_info_.top = top;
    allocation_info_.limit = limit;
  }

  void Allocate(int bytes) {
    accounting_stats_.AllocateBytes(bytes);
  }

  void IncreaseCapacity(int size) {
    accounting_stats_.ExpandSpace(size);
  }

  // Releases an unused page and shrinks the space.
  void ReleasePage(Page* page);

  // Releases all of the unused pages.
  void ReleaseAllUnusedPages();

  // The dummy page that anchors the linked list of pages.
  Page* anchor() { return &anchor_; }

#ifdef DEBUG
  // Print meta info and objects in this space.
  virtual void Print();

  // Verify integrity of this space.
  virtual void Verify(ObjectVisitor* visitor);

  // Reports statistics for the space
  void ReportStatistics();

  // Overridden by subclasses to verify space-specific object
  // properties (e.g., only maps or free-list nodes are in map space).
  virtual void VerifyObject(HeapObject* obj) {}

  // Report code object related statistics
  void CollectCodeStatistics();
  static void ReportCodeStatistics();
  static void ResetCodeStatistics();
#endif

  bool was_swept_conservatively() { return was_swept_conservatively_; }
  void set_was_swept_conservatively(bool b) { was_swept_conservatively_ = b; }

  // Evacuation candidates are swept by evacuator.  Needs to return a valid
  // result before _and_ after evacuation has finished.
  static bool ShouldBeSweptLazily(Page* p) {
    return !p->IsEvacuationCandidate() &&
           !p->IsFlagSet(Page::RESCAN_ON_EVACUATION) &&
           !p->WasSweptPrecisely();
  }

  void SetPagesToSweep(Page* first) {
    ASSERT(unswept_free_bytes_ == 0);
    if (first == &anchor_) first = NULL;
    first_unswept_page_ = first;
  }

  void IncrementUnsweptFreeBytes(int by) {
    unswept_free_bytes_ += by;
  }

  void IncreaseUnsweptFreeBytes(Page* p) {
    ASSERT(ShouldBeSweptLazily(p));
    unswept_free_bytes_ += (p->area_size() - p->LiveBytes());
  }

  void DecreaseUnsweptFreeBytes(Page* p) {
    ASSERT(ShouldBeSweptLazily(p));
    unswept_free_bytes_ -= (p->area_size() - p->LiveBytes());
  }

  bool AdvanceSweeper(intptr_t bytes_to_sweep);

  bool IsSweepingComplete() {
    return !first_unswept_page_->is_valid();
  }

  Page* FirstPage() { return anchor_.next_page(); }
  Page* LastPage() { return anchor_.prev_page(); }

  void CountFreeListItems(Page* p, FreeList::SizeStats* sizes) {
    free_list_.CountFreeListItems(p, sizes);
  }

  void EvictEvacuationCandidatesFromFreeLists();

  bool CanExpand();

  // Returns the number of total pages in this space.
  int CountTotalPages();

  // Return size of allocatable area on a page in this space.
  inline int AreaSize() {
    return area_size_;
  }

 protected:
  int area_size_;

  // Maximum capacity of this space.
  intptr_t max_capacity_;

  // Accounting information for this space.
  AllocationStats accounting_stats_;

  // The dummy page that anchors the double linked list of pages.
  Page anchor_;

  // The space's free list.
  FreeList free_list_;

  // Normal allocation information.
  AllocationInfo allocation_info_;

  // Bytes of each page that cannot be allocated.  Possibly non-zero
  // for pages in spaces with only fixed-size objects.  Always zero
  // for pages in spaces with variable sized objects (those pages are
  // padded with free-list nodes).
  int page_extra_;

  bool was_swept_conservatively_;

  // The first page to be swept when the lazy sweeper advances. Is set
  // to NULL when all pages have been swept.
  Page* first_unswept_page_;

  // The number of free bytes which could be reclaimed by advancing the
  // lazy sweeper.  This is only an estimation because lazy sweeping is
  // done conservatively.
  intptr_t unswept_free_bytes_;

  // Expands the space by allocating a fixed number of pages. Returns false if
  // it cannot allocate requested number of pages from OS, or if the hard heap
  // size limit has been hit.
  bool Expand();

  // Generic fast case allocation function that tries linear allocation at the
  // address denoted by top in allocation_info_.
  inline HeapObject* AllocateLinearly(int size_in_bytes);

  // Slow path of AllocateRaw.  This function is space-dependent.
  MUST_USE_RESULT virtual HeapObject* SlowAllocateRaw(int size_in_bytes);

  friend class PageIterator;
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
class HistogramInfo: public NumberAndSizeInfo {
 public:
  HistogramInfo() : NumberAndSizeInfo() {}

  const char* name() { return name_; }
  void set_name(const char* name) { name_ = name; }

 private:
  const char* name_;
};


enum SemiSpaceId {
  kFromSpace = 0,
  kToSpace = 1
};


class SemiSpace;


class NewSpacePage : public MemoryChunk {
 public:
  // GC related flags copied from from-space to to-space when
  // flipping semispaces.
  static const intptr_t kCopyOnFlipFlagsMask =
    (1 << MemoryChunk::POINTERS_TO_HERE_ARE_INTERESTING) |
    (1 << MemoryChunk::POINTERS_FROM_HERE_ARE_INTERESTING) |
    (1 << MemoryChunk::SCAN_ON_SCAVENGE);

  static const int kAreaSize = Page::kNonCodeObjectAreaSize;

  inline NewSpacePage* next_page() const {
    return static_cast<NewSpacePage*>(next_chunk());
  }

  inline void set_next_page(NewSpacePage* page) {
    set_next_chunk(page);
  }

  inline NewSpacePage* prev_page() const {
    return static_cast<NewSpacePage*>(prev_chunk());
  }

  inline void set_prev_page(NewSpacePage* page) {
    set_prev_chunk(page);
  }

  SemiSpace* semi_space() {
    return reinterpret_cast<SemiSpace*>(owner());
  }

  bool is_anchor() { return !this->InNewSpace(); }

  static bool IsAtStart(Address addr) {
    return (reinterpret_cast<intptr_t>(addr) & Page::kPageAlignmentMask)
        == kObjectStartOffset;
  }

  static bool IsAtEnd(Address addr) {
    return (reinterpret_cast<intptr_t>(addr) & Page::kPageAlignmentMask) == 0;
  }

  Address address() {
    return reinterpret_cast<Address>(this);
  }

  // Finds the NewSpacePage containg the given address.
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

 private:
  // Create a NewSpacePage object that is only used as anchor
  // for the doubly-linked list of real pages.
  explicit NewSpacePage(SemiSpace* owner) {
    InitializeAsAnchor(owner);
  }

  static NewSpacePage* Initialize(Heap* heap,
                                  Address start,
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
// A semispace is a contiguous chunk of memory holding page-like memory
// chunks. The mark-compact collector  uses the memory of the first page in
// the from space as a marking stack when tracing live objects.

class SemiSpace : public Space {
 public:
  // Constructor.
  SemiSpace(Heap* heap, SemiSpaceId semispace)
    : Space(heap, NEW_SPACE, NOT_EXECUTABLE),
      start_(NULL),
      age_mark_(NULL),
      id_(semispace),
      anchor_(this),
      current_page_(NULL) { }

  // Sets up the semispace using the given chunk.
  void SetUp(Address start, int initial_capacity, int maximum_capacity);

  // Tear down the space.  Heap memory was not allocated by the space, so it
  // is not deallocated here.
  void TearDown();

  // True if the space has been set up but not torn down.
  bool HasBeenSetUp() { return start_ != NULL; }

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
    ASSERT(anchor_.next_page() != &anchor_);
    return anchor_.next_page()->area_start();
  }

  // Returns the start address of the current page of the space.
  Address page_low() {
    return current_page_->area_start();
  }

  // Returns one past the end address of the space.
  Address space_end() {
    return anchor_.prev_page()->area_end();
  }

  // Returns one past the end address of the current page of the space.
  Address page_high() {
    return current_page_->area_end();
  }

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

  // True if the address is in the address range of this semispace (not
  // necessarily below the allocation pointer).
  bool Contains(Address a) {
    return (reinterpret_cast<uintptr_t>(a) & address_mask_)
           == reinterpret_cast<uintptr_t>(start_);
  }

  // True if the object is a heap object in the address range of this
  // semispace (not necessarily below the allocation pointer).
  bool Contains(Object* o) {
    return (reinterpret_cast<uintptr_t>(o) & object_mask_) == object_expected_;
  }

  // If we don't have these here then SemiSpace will be abstract.  However
  // they should never be called.
  virtual intptr_t Size() {
    UNREACHABLE();
    return 0;
  }

  virtual bool ReserveSpace(int bytes) {
    UNREACHABLE();
    return false;
  }

  bool is_committed() { return committed_; }
  bool Commit();
  bool Uncommit();

  NewSpacePage* first_page() { return anchor_.next_page(); }
  NewSpacePage* current_page() { return current_page_; }

#ifdef DEBUG
  virtual void Print();
  virtual void Verify();
  // Validate a range of of addresses in a SemiSpace.
  // The "from" address must be on a page prior to the "to" address,
  // in the linked page order, or it must be earlier on the same page.
  static void AssertValidRange(Address from, Address to);
#else
  // Do nothing.
  inline static void AssertValidRange(Address from, Address to) {}
#endif

  // Returns the current capacity of the semi space.
  int Capacity() { return capacity_; }

  // Returns the maximum capacity of the semi space.
  int MaximumCapacity() { return maximum_capacity_; }

  // Returns the initial capacity of the semi space.
  int InitialCapacity() { return initial_capacity_; }

  SemiSpaceId id() { return id_; }

  static void Swap(SemiSpace* from, SemiSpace* to);

 private:
  // Flips the semispace between being from-space and to-space.
  // Copies the flags into the masked positions on all pages in the space.
  void FlipPages(intptr_t flags, intptr_t flag_mask);

  NewSpacePage* anchor() { return &anchor_; }

  // The current and maximum capacity of the space.
  int capacity_;
  int maximum_capacity_;
  int initial_capacity_;

  // The start address of the space.
  Address start_;
  // Used to govern object promotion during mark-compact collection.
  Address age_mark_;

  // Masks and comparison values to test for containment in this semispace.
  uintptr_t address_mask_;
  uintptr_t object_mask_;
  uintptr_t object_expected_;

  bool committed_;
  SemiSpaceId id_;

  NewSpacePage anchor_;
  NewSpacePage* current_page_;

  friend class SemiSpaceIterator;
  friend class NewSpacePageIterator;
 public:
  TRACK_MEMORY("SemiSpace")
};


// A SemiSpaceIterator is an ObjectIterator that iterates over the active
// semispace of the heap's new space.  It iterates over the objects in the
// semispace from a given start address (defaulting to the bottom of the
// semispace) to the top of the semispace.  New objects allocated after the
// iterator is created are not iterated.
class SemiSpaceIterator : public ObjectIterator {
 public:
  // Create an iterator over the objects in the given space.  If no start
  // address is given, the iterator starts from the bottom of the space.  If
  // no size function is given, the iterator calls Object::Size().

  // Iterate over all of allocated to-space.
  explicit SemiSpaceIterator(NewSpace* space);
  // Iterate over all of allocated to-space, with a custome size function.
  SemiSpaceIterator(NewSpace* space, HeapObjectCallback size_func);
  // Iterate over part of allocated to-space, from start to the end
  // of allocation.
  SemiSpaceIterator(NewSpace* space, Address start);
  // Iterate from one address to another in the same semi-space.
  SemiSpaceIterator(Address from, Address to);

  HeapObject* Next() {
    if (current_ == limit_) return NULL;
    if (NewSpacePage::IsAtEnd(current_)) {
      NewSpacePage* page = NewSpacePage::FromLimit(current_);
      page = page->next_page();
      ASSERT(!page->is_anchor());
      current_ = page->area_start();
      if (current_ == limit_) return NULL;
    }

    HeapObject* object = HeapObject::FromAddress(current_);
    int size = (size_func_ == NULL) ? object->Size() : size_func_(object);

    current_ += size;
    return object;
  }

  // Implementation of the ObjectIterator functions.
  virtual HeapObject* next_object() { return Next(); }

 private:
  void Initialize(Address start,
                  Address end,
                  HeapObjectCallback size_func);

  // The current iteration point.
  Address current_;
  // The end of iteration.
  Address limit_;
  // The callback function.
  HeapObjectCallback size_func_;
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
      inline_allocation_limit_step_(0) {}

  // Sets up the new space using the given chunk.
  bool SetUp(int reserved_semispace_size_, int max_semispace_size);

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

  // True if the address or object lies in the address range of either
  // semispace (not necessarily below the allocation pointer).
  bool Contains(Address a) {
    return (reinterpret_cast<uintptr_t>(a) & address_mask_)
        == reinterpret_cast<uintptr_t>(start_);
  }

  bool Contains(Object* o) {
    Address a = reinterpret_cast<Address>(o);
    return (reinterpret_cast<uintptr_t>(a) & object_mask_) == object_expected_;
  }

  // Return the allocated bytes in the active semispace.
  virtual intptr_t Size() {
    return pages_used_ * NewSpacePage::kAreaSize +
        static_cast<int>(top() - to_space_.page_low());
  }

  // The same, but returning an int.  We have to have the one that returns
  // intptr_t because it is inherited, but if we know we are dealing with the
  // new space, which can't get as big as the other spaces then this is useful:
  int SizeAsInt() { return static_cast<int>(Size()); }

  // Return the current capacity of a semispace.
  intptr_t EffectiveCapacity() {
    SLOW_ASSERT(to_space_.Capacity() == from_space_.Capacity());
    return (to_space_.Capacity() / Page::kPageSize) * NewSpacePage::kAreaSize;
  }

  // Return the current capacity of a semispace.
  intptr_t Capacity() {
    ASSERT(to_space_.Capacity() == from_space_.Capacity());
    return to_space_.Capacity();
  }

  // Return the total amount of memory committed for new space.
  intptr_t CommittedMemory() {
    if (from_space_.is_committed()) return 2 * Capacity();
    return Capacity();
  }

  // Return the available bytes without growing.
  intptr_t Available() {
    return Capacity() - Size();
  }

  // Return the maximum capacity of a semispace.
  int MaximumCapacity() {
    ASSERT(to_space_.MaximumCapacity() == from_space_.MaximumCapacity());
    return to_space_.MaximumCapacity();
  }

  // Returns the initial capacity of a semispace.
  int InitialCapacity() {
    ASSERT(to_space_.InitialCapacity() == from_space_.InitialCapacity());
    return to_space_.InitialCapacity();
  }

  // Return the address of the allocation pointer in the active semispace.
  Address top() {
    ASSERT(to_space_.current_page()->ContainsLimit(allocation_info_.top));
    return allocation_info_.top;
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
  uintptr_t mask() { return address_mask_; }

  INLINE(uint32_t AddressToMarkbitIndex(Address addr)) {
    ASSERT(Contains(addr));
    ASSERT(IsAligned(OffsetFrom(addr), kPointerSize) ||
           IsAligned(OffsetFrom(addr) - 1, kPointerSize));
    return static_cast<uint32_t>(addr - start_) >> kPointerSizeLog2;
  }

  INLINE(Address MarkbitIndexToAddress(uint32_t index)) {
    return reinterpret_cast<Address>(index << kPointerSizeLog2);
  }

  // The allocation top and limit addresses.
  Address* allocation_top_address() { return &allocation_info_.top; }
  Address* allocation_limit_address() { return &allocation_info_.limit; }

  MUST_USE_RESULT INLINE(MaybeObject* AllocateRaw(int size_in_bytes));

  // Reset the allocation pointer to the beginning of the active semispace.
  void ResetAllocationInfo();

  void LowerInlineAllocationLimit(intptr_t step) {
    inline_allocation_limit_step_ = step;
    if (step == 0) {
      allocation_info_.limit = to_space_.page_high();
    } else {
      allocation_info_.limit = Min(
          allocation_info_.top + inline_allocation_limit_step_,
          allocation_info_.limit);
    }
    top_on_previous_step_ = allocation_info_.top;
  }

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

  inline bool ToSpaceContains(Address address) {
    return to_space_.Contains(address);
  }
  inline bool FromSpaceContains(Address address) {
    return from_space_.Contains(address);
  }

  // True if the object is a heap object in the address range of the
  // respective semispace (not necessarily below the allocation pointer of the
  // semispace).
  inline bool ToSpaceContains(Object* o) { return to_space_.Contains(o); }
  inline bool FromSpaceContains(Object* o) { return from_space_.Contains(o); }

  // Try to switch the active semispace to a new, empty, page.
  // Returns false if this isn't possible or reasonable (i.e., there
  // are no pages, or the current page is already empty), or true
  // if successful.
  bool AddFreshPage();

  virtual bool ReserveSpace(int bytes);

  // Resizes a sequential string which must be the most recent thing that was
  // allocated in new space.
  template <typename StringType>
  inline void ShrinkStringAtAllocationBoundary(String* string, int len);

#ifdef DEBUG
  // Verify the active semispace.
  virtual void Verify();
  // Print the active semispace.
  virtual void Print() { to_space_.Print(); }
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

  inline intptr_t inline_allocation_limit_step() {
    return inline_allocation_limit_step_;
  }

  SemiSpace* active_space() { return &to_space_; }

 private:
  // Update allocation info to match the current to-space page.
  void UpdateAllocationInfo();

  Address chunk_base_;
  uintptr_t chunk_size_;

  // The semispaces.
  SemiSpace to_space_;
  SemiSpace from_space_;
  VirtualMemory reservation_;
  int pages_used_;

  // Start address and bit mask for containment testing.
  Address start_;
  uintptr_t address_mask_;
  uintptr_t object_mask_;
  uintptr_t object_expected_;

  // Allocation pointer and limit for normal allocation and allocation during
  // mark-compact collection.
  AllocationInfo allocation_info_;

  // When incremental marking is active we will set allocation_info_.limit
  // to be lower than actual limit and then will gradually increase it
  // in steps to guarantee that we do incremental marking steps even
  // when all allocation is performed from inlined generated code.
  intptr_t inline_allocation_limit_step_;

  Address top_on_previous_step_;

  HistogramInfo* allocated_histogram_;
  HistogramInfo* promoted_histogram_;

  MUST_USE_RESULT MaybeObject* SlowAllocateRaw(int size_in_bytes);

  friend class SemiSpaceIterator;

 public:
  TRACK_MEMORY("NewSpace")
};


// -----------------------------------------------------------------------------
// Old object space (excluding map objects)

class OldSpace : public PagedSpace {
 public:
  // Creates an old space object with a given maximum capacity.
  // The constructor does not allocate pages from OS.
  OldSpace(Heap* heap,
           intptr_t max_capacity,
           AllocationSpace id,
           Executability executable)
      : PagedSpace(heap, max_capacity, id, executable) {
    page_extra_ = 0;
  }

  // The limit of allocation for a page in this space.
  virtual Address PageAllocationLimit(Page* page) {
    return page->area_end();
  }

 public:
  TRACK_MEMORY("OldSpace")
};


// For contiguous spaces, top should be in the space (or at the end) and limit
// should be the end of the space.
#define ASSERT_SEMISPACE_ALLOCATION_INFO(info, space) \
  SLOW_ASSERT((space).page_low() <= (info).top             \
              && (info).top <= (space).page_high()         \
              && (info).limit <= (space).page_high())


// -----------------------------------------------------------------------------
// Old space for objects of a fixed size

class FixedSpace : public PagedSpace {
 public:
  FixedSpace(Heap* heap,
             intptr_t max_capacity,
             AllocationSpace id,
             int object_size_in_bytes,
             const char* name)
      : PagedSpace(heap, max_capacity, id, NOT_EXECUTABLE),
        object_size_in_bytes_(object_size_in_bytes),
        name_(name) {
    page_extra_ = Page::kNonCodeObjectAreaSize % object_size_in_bytes;
  }

  // The limit of allocation for a page in this space.
  virtual Address PageAllocationLimit(Page* page) {
    return page->area_end() - page_extra_;
  }

  int object_size_in_bytes() { return object_size_in_bytes_; }

  // Prepares for a mark-compact GC.
  virtual void PrepareForMarkCompact();

 protected:
  void ResetFreeList() {
    free_list_.Reset();
  }

 private:
  // The size of objects in this space.
  int object_size_in_bytes_;

  // The name of this space.
  const char* name_;
};


// -----------------------------------------------------------------------------
// Old space for all map objects

class MapSpace : public FixedSpace {
 public:
  // Creates a map space object with a maximum capacity.
  MapSpace(Heap* heap, intptr_t max_capacity, AllocationSpace id)
      : FixedSpace(heap, max_capacity, id, Map::kSize, "map"),
        max_map_space_pages_(kMaxMapPageIndex - 1) {
  }

  // Given an index, returns the page address.
  // TODO(1600): this limit is artifical just to keep code compilable
  static const int kMaxMapPageIndex = 1 << 16;

  virtual int RoundSizeDownToObjectAlignment(int size) {
    if (IsPowerOf2(Map::kSize)) {
      return RoundDown(size, Map::kSize);
    } else {
      return (size / Map::kSize) * Map::kSize;
    }
  }

 protected:
#ifdef DEBUG
  virtual void VerifyObject(HeapObject* obj);
#endif

 private:
  static const int kMapsPerPage = Page::kNonCodeObjectAreaSize / Map::kSize;

  // Do map space compaction if there is a page gap.
  int CompactionThreshold() {
    return kMapsPerPage * (max_map_space_pages_ - 1);
  }

  const int max_map_space_pages_;

 public:
  TRACK_MEMORY("MapSpace")
};


// -----------------------------------------------------------------------------
// Old space for all global object property cell objects

class CellSpace : public FixedSpace {
 public:
  // Creates a property cell space object with a maximum capacity.
  CellSpace(Heap* heap, intptr_t max_capacity, AllocationSpace id)
      : FixedSpace(heap, max_capacity, id, JSGlobalPropertyCell::kSize, "cell")
  {}

  virtual int RoundSizeDownToObjectAlignment(int size) {
    if (IsPowerOf2(JSGlobalPropertyCell::kSize)) {
      return RoundDown(size, JSGlobalPropertyCell::kSize);
    } else {
      return (size / JSGlobalPropertyCell::kSize) * JSGlobalPropertyCell::kSize;
    }
  }

 protected:
#ifdef DEBUG
  virtual void VerifyObject(HeapObject* obj);
#endif

 public:
  TRACK_MEMORY("CellSpace")
};


// -----------------------------------------------------------------------------
// Large objects ( > Page::kMaxHeapObjectSize ) are allocated and managed by
// the large object space. A large object is allocated from OS heap with
// extra padding bytes (Page::kPageSize + Page::kObjectStartOffset).
// A large object always starts at Page::kObjectStartOffset to a page.
// Large objects do not move during garbage collections.

class LargeObjectSpace : public Space {
 public:
  LargeObjectSpace(Heap* heap, intptr_t max_capacity, AllocationSpace id);
  virtual ~LargeObjectSpace() {}

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
  MUST_USE_RESULT MaybeObject* AllocateRaw(int object_size,
                                           Executability executable);

  // Available bytes for objects in this space.
  inline intptr_t Available();

  virtual intptr_t Size() {
    return size_;
  }

  virtual intptr_t SizeOfObjects() {
    return objects_size_;
  }

  int PageCount() {
    return page_count_;
  }

  // Finds an object for a given address, returns Failure::Exception()
  // if it is not found. The function iterates through all objects in this
  // space, may be slow.
  MaybeObject* FindObject(Address a);

  // Finds a large object page containing the given address, returns NULL
  // if such a page doesn't exist.
  LargePage* FindPage(Address a);

  // Frees unmarked objects.
  void FreeUnmarkedObjects();

  // Checks whether a heap object is in this space; O(1).
  bool Contains(HeapObject* obj);

  // Checks whether the space is empty.
  bool IsEmpty() { return first_page_ == NULL; }

  // See the comments for ReserveSpace in the Space class.  This has to be
  // called after ReserveSpace has been called on the paged spaces, since they
  // may use some memory, leaving less for large objects.
  virtual bool ReserveSpace(int bytes);

  LargePage* first_page() { return first_page_; }

#ifdef DEBUG
  virtual void Verify();
  virtual void Print();
  void ReportStatistics();
  void CollectCodeStatistics();
#endif
  // Checks whether an address is in the object area in this space.  It
  // iterates all objects in the space. May be slow.
  bool SlowContains(Address addr) { return !FindObject(addr)->IsFailure(); }

 private:
  intptr_t max_capacity_;
  // The head of the linked list of large object chunks.
  LargePage* first_page_;
  intptr_t size_;  // allocated bytes
  int page_count_;  // number of chunks
  intptr_t objects_size_;  // size of objects
  // Map MemoryChunk::kAlignment-aligned chunks to large pages covering them
  HashMap chunk_map_;

  friend class LargeObjectIterator;

 public:
  TRACK_MEMORY("LargeObjectSpace")
};


class LargeObjectIterator: public ObjectIterator {
 public:
  explicit LargeObjectIterator(LargeObjectSpace* space);
  LargeObjectIterator(LargeObjectSpace* space, HeapObjectCallback size_func);

  HeapObject* Next();

  // implementation of ObjectIterator.
  virtual HeapObject* next_object() { return Next(); }

 private:
  LargePage* current_;
  HeapObjectCallback size_func_;
};


// Iterates over the chunks (pages and large object pages) that can contain
// pointers to new space.
class PointerChunkIterator BASE_EMBEDDED {
 public:
  inline explicit PointerChunkIterator(Heap* heap);

  // Return NULL when the iterator is done.
  MemoryChunk* next() {
    switch (state_) {
      case kOldPointerState: {
        if (old_pointer_iterator_.has_next()) {
          return old_pointer_iterator_.next();
        }
        state_ = kMapState;
        // Fall through.
      }
      case kMapState: {
        if (map_iterator_.has_next()) {
          return map_iterator_.next();
        }
        state_ = kLargeObjectState;
        // Fall through.
      }
      case kLargeObjectState: {
        HeapObject* heap_object;
        do {
          heap_object = lo_iterator_.Next();
          if (heap_object == NULL) {
            state_ = kFinishedState;
            return NULL;
          }
          // Fixed arrays are the only pointer-containing objects in large
          // object space.
        } while (!heap_object->IsFixedArray());
        MemoryChunk* answer = MemoryChunk::FromAddress(heap_object->address());
        return answer;
      }
      case kFinishedState:
        return NULL;
      default:
        break;
    }
    UNREACHABLE();
    return NULL;
  }


 private:
  enum State {
    kOldPointerState,
    kMapState,
    kLargeObjectState,
    kFinishedState
  };
  State state_;
  PageIterator old_pointer_iterator_;
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


} }  // namespace v8::internal

#endif  // V8_SPACES_H_
