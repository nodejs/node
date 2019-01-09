// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SLOT_SET_H_
#define V8_HEAP_SLOT_SET_H_

#include <map>
#include <stack>

#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/bits.h"
#include "src/objects/slots.h"
#include "src/utils.h"

#ifdef V8_COMPRESS_POINTERS
#include "src/ptr-compr.h"
#endif

namespace v8 {
namespace internal {

enum SlotCallbackResult { KEEP_SLOT, REMOVE_SLOT };

// Data structure for maintaining a set of slots in a standard (non-large)
// page. The base address of the page must be set with SetPageStart before any
// operation.
// The data structure assumes that the slots are pointer size aligned and
// splits the valid slot offset range into kBuckets buckets.
// Each bucket is a bitmap with a bit corresponding to a single slot offset.
class SlotSet : public Malloced {
 public:
  enum EmptyBucketMode {
    FREE_EMPTY_BUCKETS,     // An empty bucket will be deallocated immediately.
    PREFREE_EMPTY_BUCKETS,  // An empty bucket will be unlinked from the slot
                            // set, but deallocated on demand by a sweeper
                            // thread.
    KEEP_EMPTY_BUCKETS      // An empty bucket will be kept.
  };

  SlotSet() {
    for (int i = 0; i < kBuckets; i++) {
      StoreBucket(&buckets_[i], nullptr);
    }
  }

  ~SlotSet() {
    for (int i = 0; i < kBuckets; i++) {
      ReleaseBucket(i);
    }
    FreeToBeFreedBuckets();
  }

  void SetPageStart(Address page_start) { page_start_ = page_start; }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  // This method should only be called on the main thread because concurrent
  // allocation of the bucket is not thread-safe.
  //
  // AccessMode defines whether there can be concurrent access on the buckets
  // or not.
  template <AccessMode access_mode = AccessMode::ATOMIC>
  void Insert(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket bucket = LoadBucket<access_mode>(&buckets_[bucket_index]);
    if (bucket == nullptr) {
      bucket = AllocateBucket();
      if (!SwapInNewBucket<access_mode>(&buckets_[bucket_index], bucket)) {
        DeleteArray<uint32_t>(bucket);
        bucket = LoadBucket<access_mode>(&buckets_[bucket_index]);
      }
    }
    // Check that monotonicity is preserved, i.e., once a bucket is set we do
    // not free it concurrently.
    DCHECK_NOT_NULL(bucket);
    DCHECK_EQ(bucket, LoadBucket<access_mode>(&buckets_[bucket_index]));
    uint32_t mask = 1u << bit_index;
    if ((LoadCell<access_mode>(&bucket[cell_index]) & mask) == 0) {
      SetCellBits<access_mode>(&bucket[cell_index], mask);
    }
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  // Returns true if the set contains the slot.
  bool Contains(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket bucket = LoadBucket(&buckets_[bucket_index]);
    if (bucket == nullptr) return false;
    return (LoadCell(&bucket[cell_index]) & (1u << bit_index)) != 0;
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  void Remove(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket bucket = LoadBucket(&buckets_[bucket_index]);
    if (bucket != nullptr) {
      uint32_t cell = LoadCell(&bucket[cell_index]);
      uint32_t bit_mask = 1u << bit_index;
      if (cell & bit_mask) {
        ClearCellBits(&bucket[cell_index], bit_mask);
      }
    }
  }

  // The slot offsets specify a range of slots at addresses:
  // [page_start_ + start_offset ... page_start_ + end_offset).
  void RemoveRange(int start_offset, int end_offset, EmptyBucketMode mode) {
    CHECK_LE(end_offset, 1 << kPageSizeBits);
    DCHECK_LE(start_offset, end_offset);
    int start_bucket, start_cell, start_bit;
    SlotToIndices(start_offset, &start_bucket, &start_cell, &start_bit);
    int end_bucket, end_cell, end_bit;
    SlotToIndices(end_offset, &end_bucket, &end_cell, &end_bit);
    uint32_t start_mask = (1u << start_bit) - 1;
    uint32_t end_mask = ~((1u << end_bit) - 1);
    Bucket bucket;
    if (start_bucket == end_bucket && start_cell == end_cell) {
      bucket = LoadBucket(&buckets_[start_bucket]);
      if (bucket != nullptr) {
        ClearCellBits(&bucket[start_cell], ~(start_mask | end_mask));
      }
      return;
    }
    int current_bucket = start_bucket;
    int current_cell = start_cell;
    bucket = LoadBucket(&buckets_[current_bucket]);
    if (bucket != nullptr) {
      ClearCellBits(&bucket[current_cell], ~start_mask);
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
      if (mode == PREFREE_EMPTY_BUCKETS) {
        PreFreeEmptyBucket(current_bucket);
      } else if (mode == FREE_EMPTY_BUCKETS) {
        ReleaseBucket(current_bucket);
      } else {
        DCHECK(mode == KEEP_EMPTY_BUCKETS);
        bucket = LoadBucket(&buckets_[current_bucket]);
        if (bucket != nullptr) {
          ClearBucket(bucket, 0, kCellsPerBucket);
        }
      }
      current_bucket++;
    }
    // All buckets between start_bucket and end_bucket are cleared.
    bucket = LoadBucket(&buckets_[current_bucket]);
    DCHECK(current_bucket == end_bucket && current_cell <= end_cell);
    if (current_bucket == kBuckets || bucket == nullptr) {
      return;
    }
    while (current_cell < end_cell) {
      StoreCell(&bucket[current_cell], 0);
      current_cell++;
    }
    // All cells between start_cell and end_cell are cleared.
    DCHECK(current_bucket == end_bucket && current_cell == end_cell);
    ClearCellBits(&bucket[end_cell], ~end_mask);
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  bool Lookup(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    Bucket bucket = LoadBucket(&buckets_[bucket_index]);
    if (bucket == nullptr) return false;
    return (LoadCell(&bucket[cell_index]) & (1u << bit_index)) != 0;
  }

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  // This method should only be called on the main thread.
  //
  // Sample usage:
  // Iterate([](MaybeObjectSlot slot) {
  //    if (good(slot)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  template <typename Callback>
  int Iterate(Callback callback, EmptyBucketMode mode) {
    int new_count = 0;
    for (int bucket_index = 0; bucket_index < kBuckets; bucket_index++) {
      Bucket bucket = LoadBucket(&buckets_[bucket_index]);
      if (bucket != nullptr) {
        int in_bucket_count = 0;
        int cell_offset = bucket_index * kBitsPerBucket;
        for (int i = 0; i < kCellsPerBucket; i++, cell_offset += kBitsPerCell) {
          uint32_t cell = LoadCell(&bucket[i]);
          if (cell) {
            uint32_t old_cell = cell;
            uint32_t mask = 0;
            while (cell) {
              int bit_offset = base::bits::CountTrailingZeros(cell);
              uint32_t bit_mask = 1u << bit_offset;
              uint32_t slot = (cell_offset + bit_offset) << kTaggedSizeLog2;
              if (callback(MaybeObjectSlot(page_start_ + slot)) == KEEP_SLOT) {
                ++in_bucket_count;
              } else {
                mask |= bit_mask;
              }
              cell ^= bit_mask;
            }
            uint32_t new_cell = old_cell & ~mask;
            if (old_cell != new_cell) {
              ClearCellBits(&bucket[i], mask);
            }
          }
        }
        if (mode == PREFREE_EMPTY_BUCKETS && in_bucket_count == 0) {
          PreFreeEmptyBucket(bucket_index);
        }
        new_count += in_bucket_count;
      }
    }
    return new_count;
  }

  int NumberOfPreFreedEmptyBuckets() {
    base::MutexGuard guard(&to_be_freed_buckets_mutex_);
    return static_cast<int>(to_be_freed_buckets_.size());
  }

  void PreFreeEmptyBuckets() {
    for (int bucket_index = 0; bucket_index < kBuckets; bucket_index++) {
      Bucket bucket = LoadBucket(&buckets_[bucket_index]);
      if (bucket != nullptr) {
        if (IsEmptyBucket(bucket)) {
          PreFreeEmptyBucket(bucket_index);
        }
      }
    }
  }

  void FreeEmptyBuckets() {
    for (int bucket_index = 0; bucket_index < kBuckets; bucket_index++) {
      Bucket bucket = LoadBucket(&buckets_[bucket_index]);
      if (bucket != nullptr) {
        if (IsEmptyBucket(bucket)) {
          ReleaseBucket(bucket_index);
        }
      }
    }
  }

  void FreeToBeFreedBuckets() {
    base::MutexGuard guard(&to_be_freed_buckets_mutex_);
    while (!to_be_freed_buckets_.empty()) {
      Bucket top = to_be_freed_buckets_.top();
      to_be_freed_buckets_.pop();
      DeleteArray<uint32_t>(top);
    }
    DCHECK_EQ(0u, to_be_freed_buckets_.size());
  }

 private:
  typedef uint32_t* Bucket;
  static const int kMaxSlots = (1 << kPageSizeBits) / kTaggedSize;
  static const int kCellsPerBucket = 32;
  static const int kCellsPerBucketLog2 = 5;
  static const int kBitsPerCell = 32;
  static const int kBitsPerCellLog2 = 5;
  static const int kBitsPerBucket = kCellsPerBucket * kBitsPerCell;
  static const int kBitsPerBucketLog2 = kCellsPerBucketLog2 + kBitsPerCellLog2;
  static const int kBuckets = kMaxSlots / kCellsPerBucket / kBitsPerCell;

  Bucket AllocateBucket() {
    Bucket result = NewArray<uint32_t>(kCellsPerBucket);
    for (int i = 0; i < kCellsPerBucket; i++) {
      result[i] = 0;
    }
    return result;
  }

  void ClearBucket(Bucket bucket, int start_cell, int end_cell) {
    DCHECK_GE(start_cell, 0);
    DCHECK_LE(end_cell, kCellsPerBucket);
    int current_cell = start_cell;
    while (current_cell < kCellsPerBucket) {
      StoreCell(&bucket[current_cell], 0);
      current_cell++;
    }
  }

  void PreFreeEmptyBucket(int bucket_index) {
    Bucket bucket = LoadBucket(&buckets_[bucket_index]);
    if (bucket != nullptr) {
      base::MutexGuard guard(&to_be_freed_buckets_mutex_);
      to_be_freed_buckets_.push(bucket);
      StoreBucket(&buckets_[bucket_index], nullptr);
    }
  }

  void ReleaseBucket(int bucket_index) {
    Bucket bucket = LoadBucket(&buckets_[bucket_index]);
    StoreBucket(&buckets_[bucket_index], nullptr);
    DeleteArray<uint32_t>(bucket);
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  Bucket LoadBucket(Bucket* bucket) {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomicPointer::Acquire_Load(bucket);
    return *bucket;
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void StoreBucket(Bucket* bucket, Bucket value) {
    if (access_mode == AccessMode::ATOMIC) {
      base::AsAtomicPointer::Release_Store(bucket, value);
    } else {
      *bucket = value;
    }
  }

  bool IsEmptyBucket(Bucket bucket) {
    for (int i = 0; i < kCellsPerBucket; i++) {
      if (LoadCell(&bucket[i])) {
        return false;
      }
    }
    return true;
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  bool SwapInNewBucket(Bucket* bucket, Bucket value) {
    if (access_mode == AccessMode::ATOMIC) {
      return base::AsAtomicPointer::Release_CompareAndSwap(bucket, nullptr,
                                                           value) == nullptr;
    } else {
      DCHECK_NULL(*bucket);
      *bucket = value;
      return true;
    }
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  uint32_t LoadCell(uint32_t* cell) {
    if (access_mode == AccessMode::ATOMIC)
      return base::AsAtomic32::Acquire_Load(cell);
    return *cell;
  }

  void StoreCell(uint32_t* cell, uint32_t value) {
    base::AsAtomic32::Release_Store(cell, value);
  }

  void ClearCellBits(uint32_t* cell, uint32_t mask) {
    base::AsAtomic32::SetBits(cell, 0u, mask);
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void SetCellBits(uint32_t* cell, uint32_t mask) {
    if (access_mode == AccessMode::ATOMIC) {
      base::AsAtomic32::SetBits(cell, mask, mask);
    } else {
      *cell = (*cell & ~mask) | mask;
    }
  }

  // Converts the slot offset into bucket/cell/bit index.
  void SlotToIndices(int slot_offset, int* bucket_index, int* cell_index,
                     int* bit_index) {
    DCHECK(IsAligned(slot_offset, kTaggedSize));
    int slot = slot_offset >> kTaggedSizeLog2;
    DCHECK(slot >= 0 && slot <= kMaxSlots);
    *bucket_index = slot >> kBitsPerBucketLog2;
    *cell_index = (slot >> kBitsPerCellLog2) & (kCellsPerBucket - 1);
    *bit_index = slot & (kBitsPerCell - 1);
  }

  Bucket buckets_[kBuckets];
  Address page_start_;
  base::Mutex to_be_freed_buckets_mutex_;
  std::stack<uint32_t*> to_be_freed_buckets_;
};

enum SlotType {
  EMBEDDED_OBJECT_SLOT,
  OBJECT_SLOT,
  CODE_TARGET_SLOT,
  CODE_ENTRY_SLOT,
  CLEARED_SLOT
};

// Data structure for maintaining a list of typed slots in a page.
// Typed slots can only appear in Code and JSFunction objects, so
// the maximum possible offset is limited by the LargePage::kMaxCodePageSize.
// The implementation is a chain of chunks, where each chunks is an array of
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
  class OffsetField : public BitField<int, 0, 29> {};
  class TypeField : public BitField<SlotType, 29, 3> {};
  struct TypedSlot {
    uint32_t type_and_offset;
  };
  struct Chunk {
    Chunk* next;
    TypedSlot* buffer;
    int32_t capacity;
    int32_t count;
  };
  static const int kInitialBufferSize = 100;
  static const int kMaxBufferSize = 16 * KB;
  static int NextCapacity(int capacity) {
    return Min(kMaxBufferSize, capacity * 2);
  }
  Chunk* EnsureChunk();
  Chunk* NewChunk(Chunk* next, int capacity);
  Chunk* head_ = nullptr;
  Chunk* tail_ = nullptr;
};

// A multiset of per-page typed slots that allows concurrent iteration
// clearing of invalid slots.
class V8_EXPORT_PRIVATE TypedSlotSet : public TypedSlots {
 public:
  // The PREFREE_EMPTY_CHUNKS indicates that chunks detected as empty
  // during the iteration are queued in to_be_freed_chunks_, which are
  // then freed in FreeToBeFreedChunks.
  enum IterationMode { PREFREE_EMPTY_CHUNKS, KEEP_EMPTY_CHUNKS };

  explicit TypedSlotSet(Address page_start) : page_start_(page_start) {}

  ~TypedSlotSet() override;

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
      TypedSlot* buffer = chunk->buffer;
      int count = chunk->count;
      bool empty = true;
      for (int i = 0; i < count; i++) {
        TypedSlot slot = LoadTypedSlot(buffer + i);
        SlotType type = TypeField::decode(slot.type_and_offset);
        if (type != CLEARED_SLOT) {
          uint32_t offset = OffsetField::decode(slot.type_and_offset);
          Address addr = page_start_ + offset;
          if (callback(type, addr) == KEEP_SLOT) {
            new_count++;
            empty = false;
          } else {
            ClearTypedSlot(buffer + i);
          }
        }
      }
      Chunk* next = chunk->next;
      if (mode == PREFREE_EMPTY_CHUNKS && empty) {
        // We remove the chunk from the list but let it still point its next
        // chunk to allow concurrent iteration.
        if (previous) {
          StoreNext(previous, next);
        } else {
          StoreHead(next);
        }
        base::MutexGuard guard(&to_be_freed_chunks_mutex_);
        to_be_freed_chunks_.push(std::unique_ptr<Chunk>(chunk));
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
  TypedSlot LoadTypedSlot(TypedSlot* slot) {
    return TypedSlot{base::AsAtomic32::Relaxed_Load(&slot->type_and_offset)};
  }
  void ClearTypedSlot(TypedSlot* slot) {
    // Order is important here and should match that of LoadTypedSlot.
    base::AsAtomic32::Relaxed_Store(
        &slot->type_and_offset,
        TypeField::encode(CLEARED_SLOT) | OffsetField::encode(0));
  }

  Address page_start_;
  base::Mutex to_be_freed_chunks_mutex_;
  std::stack<std::unique_ptr<Chunk>> to_be_freed_chunks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SLOT_SET_H_
