// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_BASIC_SLOT_SET_H_
#define V8_HEAP_BASE_BASIC_SLOT_SET_H_

#include <cstddef>
#include <memory>

#include "src/base/atomic-utils.h"
#include "src/base/bits.h"
#include "src/base/platform/memory.h"

namespace heap {
namespace base {

enum SlotCallbackResult { KEEP_SLOT, REMOVE_SLOT };

// Data structure for maintaining a set of slots in a standard (non-large)
// page.
// The data structure assumes that the slots are pointer size aligned and
// splits the valid slot offset range into buckets.
// Each bucket is a bitmap with a bit corresponding to a single slot offset.
template <size_t SlotGranularity>
class BasicSlotSet {
  static constexpr auto kSystemPointerSize = sizeof(void*);

 public:
  using Address = uintptr_t;

  enum AccessMode : uint8_t {
    ATOMIC,
    NON_ATOMIC,
  };

  enum EmptyBucketMode {
    FREE_EMPTY_BUCKETS,  // An empty bucket will be deallocated immediately.
    KEEP_EMPTY_BUCKETS   // An empty bucket will be kept.
  };

  BasicSlotSet() = delete;

  static BasicSlotSet* Allocate(size_t buckets) {
    //  BasicSlotSet* slot_set --+
    //                           |
    //                           v
    //         +-----------------+-------------------------+
    //         | initial buckets |     buckets array       |
    //         +-----------------+-------------------------+
    //            pointer-sized    pointer-sized * buckets
    //
    //
    // The BasicSlotSet pointer points to the beginning of the buckets array for
    // faster access in the write barrier. The number of buckets is needed for
    // calculating the size of this data structure.
    size_t buckets_size = buckets * sizeof(Bucket*);
    size_t size = kInitialBucketsSize + buckets_size;
    void* allocation = v8::base::AlignedAlloc(size, kSystemPointerSize);
    CHECK(allocation);
    BasicSlotSet* slot_set = reinterpret_cast<BasicSlotSet*>(
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

  static void Delete(BasicSlotSet* slot_set, size_t buckets) {
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

    v8::base::AlignedFree(reinterpret_cast<uint8_t*>(slot_set) -
                          kInitialBucketsSize);
  }

  constexpr static size_t BucketsForSize(size_t size) {
    return (size + (SlotGranularity * kBitsPerBucket) - 1) /
           (SlotGranularity * kBitsPerBucket);
  }

  // Converts the slot offset into bucket index.
  constexpr static size_t BucketForSlot(size_t slot_offset) {
    DCHECK(IsAligned(slot_offset, SlotGranularity));
    return slot_offset / (SlotGranularity * kBitsPerBucket);
  }

  // Converts bucket index into slot offset.
  constexpr static size_t OffsetForBucket(size_t bucket_index) {
    return bucket_index * SlotGranularity * kBitsPerBucket;
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
    if ((bucket->template LoadCell<access_mode>(cell_index) & mask) == 0) {
      bucket->template SetCellBits<access_mode>(cell_index, mask);
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
    CHECK_LE(end_offset, buckets * kBitsPerBucket * SlotGranularity);
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
  // Iterate([](Address slot) {
  //    if (good(slot)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  //
  // Releases memory for empty buckets with FREE_EMPTY_BUCKETS.
  template <AccessMode access_mode = AccessMode::ATOMIC, typename Callback>
  size_t Iterate(Address chunk_start, size_t start_bucket, size_t end_bucket,
                 Callback callback, EmptyBucketMode mode) {
    return Iterate<access_mode>(
        chunk_start, start_bucket, end_bucket, callback,
        [this, mode](size_t bucket_index) {
          if (mode == EmptyBucketMode::FREE_EMPTY_BUCKETS) {
            ReleaseBucket(bucket_index);
          }
        });
  }

  static constexpr int kCellsPerBucket = 32;
  static constexpr int kCellsPerBucketLog2 = 5;
  static constexpr int kCellSizeBytesLog2 = 2;
  static constexpr int kCellSizeBytes = 1 << kCellSizeBytesLog2;
  static constexpr int kBitsPerCell = 32;
  static constexpr int kBitsPerCellLog2 = 5;
  static constexpr int kBitsPerBucket = kCellsPerBucket * kBitsPerCell;
  static constexpr int kBitsPerBucketLog2 =
      kCellsPerBucketLog2 + kBitsPerCellLog2;

  class Bucket final {
    uint32_t cells_[kCellsPerBucket];

   public:
    Bucket() {
      for (int i = 0; i < kCellsPerBucket; i++) {
        cells_[i] = 0;
      }
    }

    uint32_t* cells() { return cells_; }
    const uint32_t* cells() const { return cells_; }
    uint32_t* cell(int cell_index) { return cells_ + cell_index; }
    const uint32_t* cell(int cell_index) const { return cells_ + cell_index; }

    template <AccessMode access_mode = AccessMode::ATOMIC>
    uint32_t LoadCell(int cell_index) {
      DCHECK_LT(cell_index, kCellsPerBucket);
      if constexpr (access_mode == AccessMode::ATOMIC)
        return v8::base::AsAtomic32::Acquire_Load(cell(cell_index));
      return *(cell(cell_index));
    }

    template <AccessMode access_mode = AccessMode::ATOMIC>
    void SetCellBits(int cell_index, uint32_t mask) {
      if constexpr (access_mode == AccessMode::ATOMIC) {
        v8::base::AsAtomic32::SetBits(cell(cell_index), mask, mask);
      } else {
        uint32_t* c = cell(cell_index);
        *c = (*c & ~mask) | mask;
      }
    }

    template <AccessMode access_mode = AccessMode::ATOMIC>
    void ClearCellBits(int cell_index, uint32_t mask) {
      if constexpr (access_mode == AccessMode::ATOMIC) {
        v8::base::AsAtomic32::SetBits(cell(cell_index), 0u, mask);
      } else {
        *cell(cell_index) &= ~mask;
      }
    }

    void StoreCell(int cell_index, uint32_t value) {
      v8::base::AsAtomic32::Release_Store(cell(cell_index), value);
    }

    bool IsEmpty() const {
      for (int i = 0; i < kCellsPerBucket; i++) {
        if (cells_[i] != 0) {
          return false;
        }
      }
      return true;
    }
  };

 protected:
  template <AccessMode access_mode = AccessMode::ATOMIC, typename Callback,
            typename EmptyBucketCallback>
  size_t Iterate(Address chunk_start, size_t start_bucket, size_t end_bucket,
                 Callback callback, EmptyBucketCallback empty_bucket_callback) {
    size_t new_count = 0;
    for (size_t bucket_index = start_bucket; bucket_index < end_bucket;
         bucket_index++) {
      Bucket* bucket = LoadBucket<access_mode>(bucket_index);
      if (bucket != nullptr) {
        size_t in_bucket_count = 0;
        size_t cell_offset = bucket_index << kBitsPerBucketLog2;
        for (int i = 0; i < kCellsPerBucket; i++, cell_offset += kBitsPerCell) {
          uint32_t cell = bucket->template LoadCell<access_mode>(i);
          if (cell) {
            uint32_t old_cell = cell;
            uint32_t mask = 0;
            while (cell) {
              int bit_offset = v8::base::bits::CountTrailingZeros(cell);
              uint32_t bit_mask = 1u << bit_offset;
              Address slot = (cell_offset + bit_offset) * SlotGranularity;
              if (callback(chunk_start + slot) == KEEP_SLOT) {
                ++in_bucket_count;
              } else {
                mask |= bit_mask;
              }
              cell ^= bit_mask;
            }
            uint32_t new_cell = old_cell & ~mask;
            if (old_cell != new_cell) {
              bucket->template ClearCellBits<access_mode>(i, mask);
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
      return v8::base::AsAtomicPointer::Acquire_Load(bucket);
    return *bucket;
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  Bucket* LoadBucket(size_t bucket_index) {
    return LoadBucket(bucket(bucket_index));
  }

  template <AccessMode access_mode = AccessMode::ATOMIC>
  void StoreBucket(Bucket** bucket, Bucket* value) {
    if (access_mode == AccessMode::ATOMIC) {
      v8::base::AsAtomicPointer::Release_Store(bucket, value);
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
      return v8::base::AsAtomicPointer::Release_CompareAndSwap(
                 b, nullptr, value) == nullptr;
    } else {
      DCHECK_NULL(*b);
      *b = value;
      return true;
    }
  }

  // Converts the slot offset into bucket/cell/bit index.
  static void SlotToIndices(size_t slot_offset, size_t* bucket_index,
                            int* cell_index, int* bit_index) {
    DCHECK(IsAligned(slot_offset, SlotGranularity));
    size_t slot = slot_offset / SlotGranularity;
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

}  // namespace base
}  // namespace heap

#endif  // V8_HEAP_BASE_BASIC_SLOT_SET_H_
