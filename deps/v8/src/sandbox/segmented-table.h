// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SEGMENTED_TABLE_H_
#define V8_SANDBOX_SEGMENTED_TABLE_H_

#include "include/v8-internal.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/common/code-memory-access.h"

namespace v8::internal {

/**
 * A table with a fixed maximum size split into segments.
 *
 * The table provides thread-safe methods to allocate and free segments. It is
 * completely agnostic to entries stored on the Segments. See specific
 * subclasses on how these tables are managed on Entry-level.
 *
 * The table also provides low-level allocation of read-only segments. Read-only
 * segment handling is assumed to run single-threaded.
 */
class V8_EXPORT_PRIVATE SegmentedTableBase {
 public:
  // Use this scope to temporarily unseal the read-only segment (i.e. change
  // permissions to RW).
  class UnsealReadOnlySegmentScope;

  static constexpr size_t BaseOffset();

 protected:
  // Base class for all segments that is agnostic to entry types.
  class SegmentBase;

#ifdef V8_TARGET_ARCH_64_BIT
  // On 64 bit, we use a large address space reservation for the table memory.
  static constexpr bool kUseContiguousMemory = true;
  static constexpr size_t kSegmentSize = 64 * KB;
#else
  // On 32 bit, segments are individually mapped.
  static constexpr bool kUseContiguousMemory = false;
#ifdef V8_TARGET_OS_WIN
  // On windows the allocation granularity is 64KB.
  static constexpr size_t kSegmentSize = 64 * KB;
#else
  static constexpr size_t kSegmentSize = 16 * KB;
#endif
#endif  // V8_TARGET_ARCH_64_BIT

  static constexpr uint32_t kInternalReadOnlySegmentsOffset = 0;
  static constexpr size_t kAlignment = kSegmentSize;

  // The sandbox relies on not being able to access any SegmentedTable out of
  // bounds.
  static_assert(kUseContiguousMemory || !V8_ENABLE_SANDBOX_BOOL);

  SegmentedTableBase() = default;
  SegmentedTableBase(const SegmentedTableBase&) = delete;
  SegmentedTableBase& operator=(const SegmentedTableBase&) = delete;

  // Initializes the table by reserving the backing memory.
  void Initialize(size_t reservation_size, size_t read_only_reservation_size,
                  bool wants_write_protection);

  // Deallocates all memory associated with this table.
  void TearDown();

  // Returns true if this table has been initialized.
  bool is_initialized() const {
    DCHECK_IMPLIES(base_, reinterpret_cast<Address>(base_) == vas_->base());
    return vas_ != nullptr;
  }

  // Returns the base address of this table.
  Address base() const {
    DCHECK(is_initialized());
    return reinterpret_cast<Address>(base_);
  }

  // Tries to allocate a segment from the table.
  std::optional<SegmentBase> TryAllocateSegment();

  // Free the specified segment of this table.
  //
  // The memory of this segment will afterwards be inaccessible which is a
  // requirement for sandbox security.
  void FreeTableSegment(SegmentBase segment);

  // Allocates a read-only segment.
  SegmentBase AllocateReadOnlySegment();

  // Resets the read-only segments. Doesn't actually free any of the pages.
  // Returns the number of segments that were used.
  uint32_t ResetReadOnlySegments();

  // Returns the number of read-only segments allocated and in-use.
  uint32_t read_only_segments_used() const { return read_only_segments_used_; }

 private:
  // Helpers to toggle the first segment's permissions between kRead (sealed)
  // and kReadWrite (unsealed).
  void SealReadOnlySegments();
  void UnsealReadOnlySegments();

  // The pointer to the base of the virtual address space backing this table.
  // All entry accesses happen through this pointer. It is equivalent to
  // `vas_->base()` and is effectively const after initialization since the
  // backing memory is never reallocated.
  void* base_ = nullptr;
  // The virtual address space backing this table. This is used to manage the
  // underlying OS pages, in particular to allocate and free the segments that
  // make up the table.
  VirtualAddressSpace* vas_ = nullptr;
  // Used to keep track of actually used RO segments.
  uint32_t read_only_segments_used_ = 0;
  // RO reservation size.
  size_t read_only_reservation_size_ = 0;
};

// static
constexpr size_t SegmentedTableBase::BaseOffset() {
  return offsetof(SegmentedTableBase, base_);
}

class SegmentedTableBase::SegmentBase {
 public:
  // Returns the segment starting at the specified offset from the base of the
  // table.
  static SegmentBase At(uint32_t offset) {
    DCHECK(IsAligned(offset, kSegmentSize));
    const uint32_t number = offset / kSegmentSize;
    return SegmentBase(number);
  }

  // The segments of a table are numbered sequentially. This method returns
  // the number of this segment.
  constexpr uint32_t number() const { return number_; }

  // Returns the offset of this segment from the table base.
  constexpr uint32_t offset() const {
    return number_ * SegmentedTableBase::kSegmentSize;
  }

  // Segments are ordered by their `number()`.
  constexpr bool operator<(const SegmentBase& other) const {
    return number_ < other.number_;
  }
  constexpr bool operator==(const SegmentBase& other) const {
    return number_ == other.number_;
  }
  constexpr bool operator!=(const SegmentBase& other) const {
    return number_ != other.number_;
  }

 protected:
  // Initialize a segment given its number.
  constexpr explicit SegmentBase(uint32_t number) : number_(number) {}

  // A segment is identified by its number, which is its offset from the base
  // of the table divided by the segment size.
  const uint32_t number_;
};

class SegmentedTableBase::UnsealReadOnlySegmentScope final {
 public:
  explicit UnsealReadOnlySegmentScope(SegmentedTableBase* table)
      : table_(table) {
    table_->UnsealReadOnlySegments();
  }

  ~UnsealReadOnlySegmentScope() { table_->SealReadOnlySegments(); }

 private:
  SegmentedTableBase* const table_;
};

/**
 * A thread-safe table with a fixed maximum size split into segments.
 *
 * The table provides thread-safe methods to allocate and free of segments and
 * an inline freelist implementation. Allocation and Freeing of entries is
 * implemented in subclasses since it depends on if the table is manually
 * managed or GCed.
 *
 * For the purpose of memory management, the table is partitioned into Segments
 * (for example 64kb memory chunks) that are grouped together in "Spaces". All
 * segments in a space share a freelist, and so entry allocation and garbage
 * collection happen on the level of spaces.
 *
 * The Entry type defines how the freelist is represented. For that, it must
 * implement the following methods:
 * - void MakeFreelistEntry(uint32_t next_entry_index)
 * - uint32_t GetNextFreelistEntry()
 */
template <typename Entry, size_t size>
class V8_EXPORT_PRIVATE SegmentedTable : public SegmentedTableBase {
 protected:
  // The table is organized into segments where a segment that holds entries.
  struct Segment;

  // Head of the freelist.
  //
  // A segmented table uses simple, singly-linked lists to manage free entries.
  // Each entry on the freelist contains the 32-bit index of the next entry. The
  // last entry points to zero.
  struct FreelistHead;
  // We expect the FreelistHead struct to fit into a single atomic word.
  // Otherwise, access to it would be slow.
  static_assert(std::atomic<FreelistHead>::is_always_lock_free);

  class WriteIterator;
  class ReverseWriteIterator;
  // Iterator range that acts as a scope object to temporarily lift any
  // write-protection that may be otherwise enforced when kIsWriteProtected is
  // true.
  class WritableRange;

  static constexpr bool kIsWriteProtected = Entry::IsWriteProtected;
  static constexpr int kEntrySize = sizeof(Entry);
  static constexpr size_t kEntriesPerSegment = kSegmentSize / kEntrySize;

#ifdef V8_TARGET_ARCH_64_BIT
  static constexpr size_t kReservationSize = size;
  static constexpr size_t kMaxCapacity = kReservationSize / kEntrySize;
#endif  // V8_TARGET_ARCH_64_BIT

  SegmentedTable() = default;
  SegmentedTable(const SegmentedTable&) = delete;
  SegmentedTable& operator=(const SegmentedTable&) = delete;

  // Access the entry at the specified index.
  Entry& at(uint32_t index);
  const Entry& at(uint32_t index) const;

  // Returns a range that can be used to perform multiple write operations
  // on the given segment without switching write-protections continuously (if
  // kIsWriteProtected is true).
  WritableRange GetWritableRange(Segment segment);

  // Allocate a new segment in this table.
  //
  // The segment is initialized with freelist entries.
  std::pair<Segment, FreelistHead> AllocateAndInitializeSegment();
  // Same as above but fails if there is no space left.
  std::optional<std::pair<Segment, FreelistHead>>
  TryAllocateAndInitializeSegment();

  // Initialize a table segment with a freelist.
  //
  // Note that you don't need to call this function on segments allocated with
  // `AllocateAndInitializeSegment()` since those already get initialized.
  FreelistHead InitializeFreeList(Segment segment);
};

template <typename Entry, size_t size>
struct SegmentedTable<Entry, size>::Segment
    : public SegmentedTableBase::SegmentBase {
 public:
  // Returns the segment starting at the specified offset from the base of the
  // table.
  static Segment At(uint32_t offset);

  // Returns the segment containing the entry at the given index.
  static Segment Containing(uint32_t entry_index);

  // Returns the segment from the base class.
  static Segment From(SegmentBase base) { return Segment(base); }

  // Returns the index of the first entry in this segment.
  constexpr uint32_t first_entry() const {
    return number_ * SegmentedTable::kEntriesPerSegment;
  }

  // Return the index of the last entry in this segment.
  constexpr uint32_t last_entry() const {
    return first_entry() + SegmentedTable::kEntriesPerSegment - 1;
  }

 private:
  // Initialize a segment given its number.
  constexpr explicit Segment(uint32_t number) : SegmentBase(number) {}
  // Create from untyped base class.
  constexpr explicit Segment(SegmentBase base) : SegmentBase(base.number()) {}
};

template <typename Entry, size_t size>
struct SegmentedTable<Entry, size>::FreelistHead {
  constexpr FreelistHead() = default;
  constexpr FreelistHead(uint32_t next, uint32_t length)
      : next_(next), length_(length) {}

  // Returns the index of the next entry on the freelist. Returns zero for an
  // empty freelist.
  uint32_t next() const { return next_; }

  // Returns the total length of the freelist.
  uint32_t length() const { return length_; }

  // Returns whether the freelist is empty, i.e., it's length is zero.
  bool is_empty() const { return length_ == 0; }

 private:
  uint32_t next_ = 0;
  uint32_t length_ = 0;
};

template <typename Entry, size_t size>
class SegmentedTable<Entry, size>::WriteIterator {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = Entry;
  using difference_type = std::ptrdiff_t;
  using pointer = Entry*;
  using reference = Entry&;

  WriteIterator(Entry* base, Segment segment, uint32_t index)
      : base_(base), segment_(segment), index_(index) {
    DCHECK_GE(index_, segment_.first_entry());
    DCHECK_LE(index_, segment_.last_entry() + 1);
  }

  uint32_t index() const { return index_; }

  Entry* operator->() const {
    DCHECK_GE(index_, segment_.first_entry());
    DCHECK_LE(index_, segment_.last_entry());
    return &base_[index_];
  }
  Entry& operator*() const {
    DCHECK_GE(index_, segment_.first_entry());
    DCHECK_LE(index_, segment_.last_entry());
    return base_[index_];
  }

  WriteIterator& operator++() {
    DCHECK_LE(index_, segment_.last_entry());
    index_++;
    return *this;
  }
  WriteIterator operator++(int) {
    WriteIterator tmp = *this;
    ++(*this);
    return tmp;
  }
  WriteIterator& operator--() {
    DCHECK_GT(index_, segment_.first_entry());
    index_--;
    return *this;
  }
  WriteIterator operator--(int) {
    WriteIterator tmp = *this;
    --(*this);
    return tmp;
  }

  bool operator==(const WriteIterator& other) const {
    DCHECK_EQ(segment_, other.segment_);
    return index_ == other.index_;
  }
  bool operator!=(const WriteIterator& other) const {
    DCHECK_EQ(segment_, other.segment_);
    return index_ != other.index_;
  }

 private:
  Entry* base_;
  Segment segment_;
  uint32_t index_;
};

template <typename Entry, size_t size>
class SegmentedTable<Entry, size>::ReverseWriteIterator
    : public std::reverse_iterator<WriteIterator> {
 public:
  using Base = std::reverse_iterator<WriteIterator>;
  using Base::Base;

  uint32_t index() const { return std::prev(this->base()).index(); }
};

template <typename Entry, size_t size>
class SegmentedTable<Entry, size>::WritableRange {
 public:
  using iterator = WriteIterator;
  using const_iterator = WriteIterator;
  using reverse_iterator = ReverseWriteIterator;
  using const_reverse_iterator = ReverseWriteIterator;

  WritableRange(Entry* base, Segment segment)
      : base_(base), segment_(segment), write_scope_("pointer table write") {}

  WritableRange(const WritableRange&) = delete;
  WritableRange& operator=(const WritableRange&) = delete;

  iterator begin() const {
    return WriteIterator(base_, segment_, segment_.first_entry());
  }
  iterator end() const {
    return WriteIterator(base_, segment_, segment_.last_entry() + 1);
  }

  reverse_iterator rbegin() const { return ReverseWriteIterator(end()); }
  reverse_iterator rend() const { return ReverseWriteIterator(begin()); }

 private:
  Entry* base_;
  Segment segment_;
  std::conditional_t<kIsWriteProtected, CFIMetadataWriteScope,
                     NopRwxMemoryWriteScope>
      write_scope_;
};

}  // namespace v8::internal

#endif  // V8_SANDBOX_SEGMENTED_TABLE_H_
