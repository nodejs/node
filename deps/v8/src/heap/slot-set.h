// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SLOT_SET_H_
#define V8_HEAP_SLOT_SET_H_

#include <map>
#include <memory>
#include <stack>

#include "src/base/atomic-utils.h"
#include "src/base/bit-field.h"
#include "src/base/bits.h"
#include "src/heap/worklist.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/slots.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

enum SlotCallbackResult { KEEP_SLOT, REMOVE_SLOT };

// Possibly empty buckets (buckets that do not contain any slots) are discovered
// by the scavenger. Buckets might become non-empty when promoting objects later
// or in another thread, so all those buckets need to be revisited.
// Track possibly empty buckets within a SlotSet in this data structure. The
// class contains a word-sized bitmap, in case more bits are needed the bitmap
// is replaced with a pointer to a malloc-allocated bitmap.
class PossiblyEmptyBuckets {
 public:
  PossiblyEmptyBuckets() : bitmap_(kNullAddress) {}
  PossiblyEmptyBuckets(PossiblyEmptyBuckets&& other) V8_NOEXCEPT
      : bitmap_(other.bitmap_) {
    other.bitmap_ = kNullAddress;
  }

  ~PossiblyEmptyBuckets() { Release(); }

  void Initialize() {
    bitmap_ = kNullAddress;
    DCHECK(!IsAllocated());
  }

  void Release() {
    if (IsAllocated()) {
      AlignedFree(BitmapArray());
    }
    bitmap_ = kNullAddress;
    DCHECK(!IsAllocated());
  }

  void Insert(size_t bucket_index, size_t buckets) {
    if (IsAllocated()) {
      InsertAllocated(bucket_index);
    } else if (bucket_index + 1 < kBitsPerWord) {
      bitmap_ |= static_cast<uintptr_t>(1) << (bucket_index + 1);
    } else {
      Allocate(buckets);
      InsertAllocated(bucket_index);
    }
  }

  bool Contains(size_t bucket_index) {
    if (IsAllocated()) {
      size_t word_idx = bucket_index / kBitsPerWord;
      uintptr_t* word = BitmapArray() + word_idx;
      return *word &
             (static_cast<uintptr_t>(1) << (bucket_index % kBitsPerWord));
    } else if (bucket_index + 1 < kBitsPerWord) {
      return bitmap_ & (static_cast<uintptr_t>(1) << (bucket_index + 1));
    } else {
      return false;
    }
  }

  bool IsEmpty() { return bitmap_ == kNullAddress; }

 private:
  Address bitmap_;
  static const Address kPointerTag = 1;
  static const int kWordSize = sizeof(uintptr_t);
  static const int kBitsPerWord = kWordSize * kBitsPerByte;

  bool IsAllocated() { return bitmap_ & kPointerTag; }

  void Allocate(size_t buckets) {
    DCHECK(!IsAllocated());
    size_t words = WordsForBuckets(buckets);
    uintptr_t* ptr = reinterpret_cast<uintptr_t*>(
        AlignedAlloc(words * kWordSize, kSystemPointerSize));
    ptr[0] = bitmap_ >> 1;

    for (size_t word_idx = 1; word_idx < words; word_idx++) {
      ptr[word_idx] = 0;
    }
    bitmap_ = reinterpret_cast<Address>(ptr) + kPointerTag;
    DCHECK(IsAllocated());
  }

  void InsertAllocated(size_t bucket_index) {
    DCHECK(IsAllocated());
    size_t word_idx = bucket_index / kBitsPerWord;
    uintptr_t* word = BitmapArray() + word_idx;
    *word |= static_cast<uintptr_t>(1) << (bucket_index % kBitsPerWord);
  }

  static size_t WordsForBuckets(size_t buckets) {
    return (buckets + kBitsPerWord - 1) / kBitsPerWord;
  }

  uintptr_t* BitmapArray() {
    DCHECK(IsAllocated());
    return reinterpret_cast<uintptr_t*>(bitmap_ & ~kPointerTag);
  }

  FRIEND_TEST(PossiblyEmptyBucketsTest, WordsForBuckets);

  DISALLOW_COPY_AND_ASSIGN(PossiblyEmptyBuckets);
};

STATIC_ASSERT(std::is_standard_layout<PossiblyEmptyBuckets>::value);
STATIC_ASSERT(sizeof(PossiblyEmptyBuckets) == kSystemPointerSize);

// Data structure for maintaining a set of slots in a standard (non-large)
// page.
// The data structure assumes that the slots are pointer size aligned and
// splits the valid slot offset range into buckets.
// Each bucket is a bitmap with a bit corresponding to a single slot offset.
class SlotSet {
 public:
  enum EmptyBucketMode {
    FREE_EMPTY_BUCKETS,  // An empty bucket will be deallocated immediately.
    KEEP_EMPTY_BUCKETS   // An empty bucket will be kept.
  };

  SlotSet() = delete;

  static SlotSet* Allocate(size_t buckets) {
    //  SlotSet* slot_set --+
    //                      |
    //                      v
    //    +-----------------+-------------------------+
    //    | initial buckets |     buckets array       |
    //    +-----------------+-------------------------+
    //       pointer-sized    pointer-sized * buckets
    //
    //
    // The SlotSet pointer points to the beginning of the buckets array for
    // faster access in the write barrier. The number of buckets is needed for
    // calculating the size of this data structure.
    size_t buckets_size = buckets * sizeof(Bucket*);
    size_t size = kInitialBucketsSize + buckets_size;
    void* allocation = AlignedAlloc(size, kSystemPointerSize);
    SlotSet* slot_set = reinterpret_cast<SlotSet*>(
        reinterpret_cast<uint8_t*>(allocation) + kInitialBucketsSize);
    DCHECK(
        IsAligned(reinterpret_cast<uintptr_t>(slot_set), kSystemPointerSize));
#ifdef DEBUG
    *slot_set->initial_buckets() = buckets;
#endif
    for (size_t i = 0; i < buckets; i++) {
      *slot_set->bucket(i) = nullptr;
    }
    return slot_set;
  }

  static void Delete(SlotSet* slot_set, size_t buckets) {
    if (slot_set == nullptr) return;

    for (size_t i = 0; i < buckets; i++) {
      slot_set->ReleaseBucket(i);
    }

#ifdef DEBUG
    size_t initial_buckets = *slot_set->initial_buckets();

    for (size_t i = buckets; i < initial_buckets; i++) {
      DCHECK_NULL(*slot_set->bucket(i));
    }
#endif

    AlignedFree(reinterpret_cast<uint8_t*>(slot_set) - kInitialBucketsSize);
  }

  static size_t BucketsForSize(size_t size) {
    return (size + (kTaggedSize * kBitsPerBucket) - 1) >>
           (kTaggedSizeLog2 + kBitsPerBucketLog2);
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  // AccessMode defines whether there can be concurrent access on the buckets
  // or not.
  template <AccessMode access_mode>
  void Insert(size_t slot_offset) {
    size_t bucket_index;
    int cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket* bucket = LoadBucket<access_mode>(bucket_index);
    if (bucket == nullptr) {
      bucket = new Bucket;
      if (!SwapInNewBucket<access_mode>(bucket_index, bucket)) {
        delete bucket;
        bucket = LoadBucket<access_mode>(bucket_index);
      }
    }
    // Check that monotonicity is preserved, i.e., once a bucket is set we do
    // not free it concurrently.
    DCHECK(bucket != nullptr);
    DCHECK_EQ(bucket->cells(), LoadBucket<access_mode>(bucket_index)->cells());
    uint32_t mask = 1u << bit_index;
    if ((bucket->LoadCell<access_mode>(cell_index) & mask) == 0) {
      bucket->SetCellBits<access_mode>(cell_index, mask);
    }
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  // Returns true if the set contains the slot.
  bool Contains(size_t slot_offset) {
    size_t bucket_index;
    int cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket* bucket = LoadBucket(bucket_index);
    if (bucket == nullptr) return false;
    return (bucket->LoadCell(cell_index) & (1u << bit_index)) != 0;
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  void Remove(size_t slot_offset) {
    size_t bucket_index;
    int cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket* bucket = LoadBucket(bucket_index);
    if (bucket != nullptr) {
      uint32_t cell = bucket->LoadCell(cell_index);
      uint32_t bit_mask = 1u << bit_index;
      if (cell & bit_mask) {
        bucket->ClearCellBits(cell_index, bit_mask);
      }
    }
  }

  // The slot offsets specify a range of slots at addresses:
  // [page_start_ + start_offset ... page_start_ + end_offset).
  void RemoveRange(size_t start_offset, size_t end_offset, size_t buckets,
                   EmptyBucketMode mode) {
    CHECK_LE(end_offset, buckets * kBitsPerBucket * kTaggedSize);
    DCHECK_LE(start_offset, end_offset);
    size_t start_bucket;
    int start_cell, start_bit;
    SlotToIndices(start_offset, &start_bucket, &start_cell, &start_bit);
    size_t end_bucket;
    int end_cell, end_bit;
    SlotToIndices(end_offset, &end_bucket, &end_cell, &end_bit);
    uint32_t start_mask = (1u << start_bit) - 1;
    uint32_t end_mask = ~((1u << end_bit) - 1);
    Bucket* bucket;
    if (start_bucket == end_bucket && start_cell == end_cell) {
      bucket = LoadBucket(start_bucket);
      if (bucket != nullptr) {
        bucket->ClearCellBits(start_cell, ~(start_mask | end_mask));
      }
      return;
    }
    size_t current_bucket = start_bucket;
    int current_cell = start_cell;
    bucket = LoadBucket(current_bucket);
    if (bucket != nullptr) {
      bucket->ClearCellBits(current_cell, ~start_mask);
    }
    current_cell++;
    if (current_bucket < end_bucket) {
      if (bucket != nullptr) {
        ClearBucket(bucket, current_cell, kCellsPerBucket);
      }
      // The rest of the current bucket is cleared.
      // Move on to the next bucket.
      current_bucket++;
      current_cell = 0;
    }
    DCHECK(current_bucket == end_bucket ||
           (current_bucket < end_bucket && current_cell == 0));
    while (current_bucket < end_bucket) {
      if (mode == FREE_EMPTY_BUCKETS) {
        ReleaseBucket(current_bucket);
      } else {
        DCHECK(mode == KEEP_EMPTY_BUCKETS);
        bucket = LoadBucket(current_bucket);
        if (bucket != nullptr) {
          ClearBucket(bucket, 0, kCellsPerBucket);
        }
      }
      current_bucket++;
    }
    // All buckets between start_bucket and end_bucket are cleared.
    DCHECK(current_bucket == end_bucket);
    if (current_bucket == buckets) return;
    bucket = LoadBucket(current_bucket);
    DCHECK(current_cell <= end_cell);
    if (bucket == nullptr) return;
    while (current_cell < end_cell) {
      bucket->StoreCell(current_cell, 0);
      current_cell++;
    }
    // All cells between start_cell and end_cell are cleared.
    DCHECK(current_bucket == end_bucket && current_cell == end_cell);
    bucket->ClearCellBits(end_cell, ~end_mask);
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  bool Lookup(size_t slot_offset) {
    size_t bucket_index;
    int cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket* bucket = LoadBucket(bucket_index);
    if (bucket == nullptr) return false;
    return (bucket->LoadCell(cell_index) & (1u << bit_index)) != 0;
  }

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  //
  // Iteration can be performed concurrently with other operations that use
  // atomic access mode such as insertion and removal. However there is no
  // guarantee about ordering and linearizability.
  //
  // Sample usage:
  // Iterate([](MaybeObjectSlot slot) {
  //    if (good(slot)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  //
  // Releases memory for empty buckets with FREE_EMPTY_BUCKETS.
  template <typename Callback>
  size_t Iterate(Address chunk_start, size_t buckets, Callback callback,
                 EmptyBucketMode mode) {
    return Iterate(chunk_start, buckets, callback,
                   [this, mode](size_t bucket_index) {
                     if (mode == EmptyBucketMode::FREE_EMPTY_BUCKETS) {
                       ReleaseBucket(bucket_index);
                     }
                   });
  }

  // Similar to Iterate but marks potentially empty buckets internally. Stores
  // true in empty_bucket_found in case a potentially empty bucket was found.
  // Assumes that the possibly empty-array was already cleared by
  // CheckPossiblyEmptyBuckets.
  template <typename Callback>
  size_t IterateAndTrackEmptyBuckets(
      Address chunk_start, size_t buckets, Callback callback,
      PossiblyEmptyBuckets* possibly_empty_buckets) {
    return Iterate(chunk_start, buckets, callback,
                   [possibly_empty_buckets, buckets](size_t bucket_index) {
                     possibly_empty_buckets->Insert(bucket_index, buckets);
                   });
  }

  bool FreeEmptyBuckets(size_t buckets) {
    bool empty = true;
    for (size_t bucket_index = 0; bucket_index < buckets; bucket_index++) {
      if (!FreeBucketIfEmpty(bucket_index)) {
        empty = false;
      }
    }

    return empty;
  }

  // Check whether possibly empty buckets are really empty. Empty buckets are
  // freed and the possibly empty state is cleared for all buckets.
  bool CheckPossiblyEmptyBuckets(size_t buckets,
                                 PossiblyEmptyBuckets* possibly_empty_buckets) {
    bool empty = true;
    for (size_t bucket_index = 0; bucket_index < buckets; bucket_index++) {
      Bucket* bucket = LoadBucket<AccessMode::NON_ATOMIC>(bucket_index);
      if (bucket) {
        if (possibly_empty_buckets->Contains(bucket_index)) {
          if (bucket->IsEmpty()) {
            ReleaseBucket<AccessMode::NON_ATOMIC>(bucket_index);
          } else {
            empty = false;
          }
        } else {
          empty = false;
        }
      } else {
        // Unfortunately we cannot DCHECK here that the corresponding bit in
        // possibly_empty_buckets is not set. After scavenge, the
        // MergeOldToNewRememberedSets operation might remove a recorded bucket.
      }
    }

    possibly_empty_buckets->Release();

    return empty;
  }

  static const int kCellsPerBucket = 32;
  static const int kCellsPerBucketLog2 = 5;
  static const int kCellSizeBytesLog2 = 2;
  static const int kCellSizeBytes = 1 << kCellSizeBytesLog2;
  static const int kBitsPerCell = 32;
  static const int kBitsPerCellLog2 = 5;
  static const int kBitsPerBucket = kCellsPerBucket * kBitsPerCell;
  static const int kBitsPerBucketLog2 = kCellsPerBucketLog2 + kBitsPerCellLog2;
  static const int kBucketsRegularPage =
      (1 << kPageSizeBits) / kTaggedSize / kCellsPerBucket / kBitsPerCell;

  class Bucket : public Malloced {
    uint32_t cells_[kCellsPerBucket];

   public:
    Bucket() {
      for (int i = 0; i < kCellsPerBucket; i++) {
        cells_[i] = 0;
      }
    }

    uint32_t* cells() { return cells_; }
    uint32_t* cell(int cell_index) { return cells() + cell_index; }

    template <AccessMode access_mode = AccessMode::ATOMIC>
    uint32_t LoadCell(int cell_index) {
      DCHECK_LT(cell_index, kCellsPerBucket);
      if (access_mode == AccessMode::ATOMIC)
        return base::AsAtomic32::Acquire_Load(cells() + cell_index);
      return *(cells() + cell_index);
    }

    template <AccessMode access_mode = AccessMode::ATOMIC>
    void SetCellBits(int cell_index, uint32_t mask) {
      if (access_mode == AccessMode::ATOMIC) {
        base::AsAtomic32::SetBits(cell(cell_index), mask, mask);
      } else {
        uint32_t* c = cell(cell_index);
        *c = (*c & ~mask) | mask;
      }
    }

    void ClearCellBits(int cell_index, uint32_t mask) {
      base::AsAtomic32::SetBits(cell(cell_index), 0u, mask);
    }

    void StoreCell(int cell_index, uint32_t value) {
      base::AsAtomic32::Release_Store(cell(cell_index), value);
    }

    bool IsEmpty() {
      for (int i = 0; i < kCellsPerBucket; i++) {
        if (cells_[i] != 0) {
          return false;
        }
      }
      return true;
    }
  };

 private:
  template <typename Callback, typename EmptyBucketCallback>
  size_t Iterate(Address chunk_start, size_t buckets, Callback callback,
                 EmptyBucketCallback empty_bucket_callback) {
    size_t new_count = 0;
    for (size_t bucket_index = 0; bucket_index < buckets; bucket_index++) {
      Bucket* bucket = LoadBucket(bucket_index);
      if (bucket != nullptr) {
        size_t in_bucket_count = 0;
        size_t cell_offset = bucket_index << kBitsPerBucketLog2;
        for (int i = 0; i < kCellsPerBucket; i++, cell_offset += kBitsPerCell) {
          uint32_t cell = bucket->LoadCell(i);
          if (cell) {
            uint32_t old_cell = cell;
            uint32_t mask = 0;
            while (cell) {
              int bit_offset = base::bits::CountTrailingZeros(cell);
              uint32_t bit_mask = 1u << bit_offset;
              Address slot = (cell_offset + bit_offset) << kTaggedSizeLog2;
              if (callback(MaybeObjectSlot(chunk_start + slot)) == KEEP_SLOT) {
                ++in_bucket_count;
              } else {
                mask |= bit_mask;
              }
              cell ^= bit_mask;
            }
            uint32_t new_cell = old_cell & ~mask;
            if (old_cell != new_cell) {
              bucket->ClearCellBits(i, mask);
            }
          }
        }
        if (in_bucket_count == 0) {
          empty_bucket_callback(bucket_index);
        }
        new_count += in_bucket_count;
      }
    }
    return new_count;
  }

  bool FreeBucketIfEmpty(size_t bucket_index) {
    Bucket* bucket = LoadBucket<AccessMode::NON_ATOMIC>(bucket_index);
    if (bucket != nullptr) {
      if (bucket->IsEmpty()) {
        ReleaseBucket<AccessMode::NON_ATOMIC>(bucket_index);
      } else {
        return false;
      }
    }

    return true;
  }

  void ClearBucket(Bucket* bucket, int start_cell, int end_cell) {
    DCHECK_GE(start_cell, 0);
    DCHECK_LE(end_cell, kCellsPerBucket);
    int current_cell = start_cell;
    while (current_cell < kCellsPerBucket) {
      bucket->StoreCell(current_cell, 0);
      current_cell++;
    }
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void ReleaseBucket(size_t bucket_index) {
    Bucket* bucket = LoadBucket<access_mode>(bucket_index);
    StoreBucket<access_mode>(bucket_index, nullptr);
    delete bucket;
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  Bucket* LoadBucket(Bucket** bucket) {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(bucket);
    return *bucket;
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  Bucket* LoadBucket(size_t bucket_index) {
    return LoadBucket(bucket(bucket_index));
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void StoreBucket(Bucket** bucket, Bucket* value) {
    if (access_mode == AccessMode::ATOMIC) {
      base::AsAtomicPointer::Release_Store(bucket, value);
    } else {
      *bucket = value;
    }
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void StoreBucket(size_t bucket_index, Bucket* value) {
    StoreBucket(bucket(bucket_index), value);
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  bool SwapInNewBucket(size_t bucket_index, Bucket* value) {
    Bucket** b = bucket(bucket_index);
    if (access_mode == AccessMode::ATOMIC) {
      return base::AsAtomicPointer::Release_CompareAndSwap(b, nullptr, value) ==
             nullptr;
    } else {
      DCHECK_NULL(*b);
      *b = value;
      return true;
    }
  }

  // Converts the slot offset into bucket/cell/bit index.
  static void SlotToIndices(size_t slot_offset, size_t* bucket_index,
                            int* cell_index, int* bit_index) {
    DCHECK(IsAligned(slot_offset, kTaggedSize));
    size_t slot = slot_offset >> kTaggedSizeLog2;
    *bucket_index = slot >> kBitsPerBucketLog2;
    *cell_index =
        static_cast<int>((slot >> kBitsPerCellLog2) & (kCellsPerBucket - 1));
    *bit_index = static_cast<int>(slot & (kBitsPerCell - 1));
  }

  Bucket** buckets() { return reinterpret_cast<Bucket**>(this); }
  Bucket** bucket(size_t bucket_index) { return buckets() + bucket_index; }

#ifdef DEBUG
  size_t* initial_buckets() { return reinterpret_cast<size_t*>(this) - 1; }
  static const int kInitialBucketsSize = sizeof(size_t);
#else
  static const int kInitialBucketsSize = 0;
#endif
};

STATIC_ASSERT(std::is_standard_layout<SlotSet>::value);
STATIC_ASSERT(std::is_standard_layout<SlotSet::Bucket>::value);

enum SlotType {
  FULL_EMBEDDED_OBJECT_SLOT,
  COMPRESSED_EMBEDDED_OBJECT_SLOT,
  OBJECT_SLOT,
  CODE_TARGET_SLOT,
  CODE_ENTRY_SLOT,
  CLEARED_SLOT
};

// Data structure for maintaining a list of typed slots in a page.
// Typed slots can only appear in Code objects, so
// the maximum possible offset is limited by the LargePage::kMaxCodePageSize.
// The implementation is a chain of chunks, where each chunk is an array of
// encoded (slot type, slot offset) pairs.
// There is no duplicate detection and we do not expect many duplicates because
// typed slots contain V8 internal pointers that are not directly exposed to JS.
class V8_EXPORT_PRIVATE TypedSlots {
 public:
  static const int kMaxOffset = 1 << 29;
  TypedSlots() = default;
  virtual ~TypedSlots();
  void Insert(SlotType type, uint32_t offset);
  void Merge(TypedSlots* other);

 protected:
  using OffsetField = base::BitField<int, 0, 29>;
  using TypeField = base::BitField<SlotType, 29, 3>;
  struct TypedSlot {
    uint32_t type_and_offset;
  };
  struct Chunk {
    Chunk* next;
    std::vector<TypedSlot> buffer;
  };
  static const size_t kInitialBufferSize = 100;
  static const size_t kMaxBufferSize = 16 * KB;
  static size_t NextCapacity(size_t capacity) {
    return Min(kMaxBufferSize, capacity * 2);
  }
  Chunk* EnsureChunk();
  Chunk* NewChunk(Chunk* next, size_t capacity);
  Chunk* head_ = nullptr;
  Chunk* tail_ = nullptr;
};

// A multiset of per-page typed slots that allows concurrent iteration
// clearing of invalid slots.
class V8_EXPORT_PRIVATE TypedSlotSet : public TypedSlots {
 public:
  enum IterationMode { FREE_EMPTY_CHUNKS, KEEP_EMPTY_CHUNKS };

  explicit TypedSlotSet(Address page_start) : page_start_(page_start) {}

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  //
  // Sample usage:
  // Iterate([](SlotType slot_type, Address slot_address) {
  //    if (good(slot_type, slot_address)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  // This can run concurrently to ClearInvalidSlots().
  template <typename Callback>
  int Iterate(Callback callback, IterationMode mode) {
    STATIC_ASSERT(CLEARED_SLOT < 8);
    Chunk* chunk = head_;
    Chunk* previous = nullptr;
    int new_count = 0;
    while (chunk != nullptr) {
      bool empty = true;
      for (TypedSlot& slot : chunk->buffer) {
        SlotType type = TypeField::decode(slot.type_and_offset);
        if (type != CLEARED_SLOT) {
          uint32_t offset = OffsetField::decode(slot.type_and_offset);
          Address addr = page_start_ + offset;
          if (callback(type, addr) == KEEP_SLOT) {
            new_count++;
            empty = false;
          } else {
            slot = ClearedTypedSlot();
          }
        }
      }
      Chunk* next = chunk->next;
      if (mode == FREE_EMPTY_CHUNKS && empty) {
        // We remove the chunk from the list but let it still point its next
        // chunk to allow concurrent iteration.
        if (previous) {
          StoreNext(previous, next);
        } else {
          StoreHead(next);
        }

        delete chunk;
      } else {
        previous = chunk;
      }
      chunk = next;
    }
    return new_count;
  }

  // Clears all slots that have the offset in the specified ranges.
  // This can run concurrently to Iterate().
  void ClearInvalidSlots(const std::map<uint32_t, uint32_t>& invalid_ranges);

  // Frees empty chunks accumulated by PREFREE_EMPTY_CHUNKS.
  void FreeToBeFreedChunks();

 private:
  // Atomic operations used by Iterate and ClearInvalidSlots;
  Chunk* LoadNext(Chunk* chunk) {
    return base::AsAtomicPointer::Relaxed_Load(&chunk->next);
  }
  void StoreNext(Chunk* chunk, Chunk* next) {
    return base::AsAtomicPointer::Relaxed_Store(&chunk->next, next);
  }
  Chunk* LoadHead() { return base::AsAtomicPointer::Relaxed_Load(&head_); }
  void StoreHead(Chunk* chunk) {
    base::AsAtomicPointer::Relaxed_Store(&head_, chunk);
  }
  static TypedSlot ClearedTypedSlot() {
    return TypedSlot{TypeField::encode(CLEARED_SLOT) | OffsetField::encode(0)};
  }

  Address page_start_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SLOT_SET_H_
