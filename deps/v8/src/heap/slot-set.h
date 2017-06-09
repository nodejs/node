// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SLOT_SET_H
#define V8_SLOT_SET_H

#include <map>
#include <stack>

#include "src/allocation.h"
#include "src/base/atomic-utils.h"
#include "src/base/bits.h"
#include "src/utils.h"

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
      bucket[i].SetValue(nullptr);
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
  void Insert(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    base::AtomicValue<uint32_t>* current_bucket = bucket[bucket_index].Value();
    if (current_bucket == nullptr) {
      current_bucket = AllocateBucket();
      bucket[bucket_index].SetValue(current_bucket);
    }
    if (!(current_bucket[cell_index].Value() & (1u << bit_index))) {
      current_bucket[cell_index].SetBit(bit_index);
    }
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  // Returns true if the set contains the slot.
  bool Contains(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    base::AtomicValue<uint32_t>* current_bucket = bucket[bucket_index].Value();
    if (current_bucket == nullptr) {
      return false;
    }
    return (current_bucket[cell_index].Value() & (1u << bit_index)) != 0;
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  void Remove(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    base::AtomicValue<uint32_t>* current_bucket = bucket[bucket_index].Value();
    if (current_bucket != nullptr) {
      uint32_t cell = current_bucket[cell_index].Value();
      if (cell) {
        uint32_t bit_mask = 1u << bit_index;
        if (cell & bit_mask) {
          current_bucket[cell_index].ClearBit(bit_index);
        }
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
    if (start_bucket == end_bucket && start_cell == end_cell) {
      ClearCell(start_bucket, start_cell, ~(start_mask | end_mask));
      return;
    }
    int current_bucket = start_bucket;
    int current_cell = start_cell;
    ClearCell(current_bucket, current_cell, ~start_mask);
    current_cell++;
    base::AtomicValue<uint32_t>* bucket_ptr = bucket[current_bucket].Value();
    if (current_bucket < end_bucket) {
      if (bucket_ptr != nullptr) {
        ClearBucket(bucket_ptr, current_cell, kCellsPerBucket);
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
        bucket_ptr = bucket[current_bucket].Value();
        if (bucket_ptr) {
          ClearBucket(bucket_ptr, 0, kCellsPerBucket);
        }
      }
      current_bucket++;
    }
    // All buckets between start_bucket and end_bucket are cleared.
    bucket_ptr = bucket[current_bucket].Value();
    DCHECK(current_bucket == end_bucket && current_cell <= end_cell);
    if (current_bucket == kBuckets || bucket_ptr == nullptr) {
      return;
    }
    while (current_cell < end_cell) {
      bucket_ptr[current_cell].SetValue(0);
      current_cell++;
    }
    // All cells between start_cell and end_cell are cleared.
    DCHECK(current_bucket == end_bucket && current_cell == end_cell);
    ClearCell(end_bucket, end_cell, ~end_mask);
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  bool Lookup(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    if (bucket[bucket_index].Value() != nullptr) {
      uint32_t cell = bucket[bucket_index].Value()[cell_index].Value();
      return (cell & (1u << bit_index)) != 0;
    }
    return false;
  }

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  // This method should only be called on the main thread.
  //
  // Sample usage:
  // Iterate([](Address slot_address) {
  //    if (good(slot_address)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  template <typename Callback>
  int Iterate(Callback callback, EmptyBucketMode mode) {
    int new_count = 0;
    for (int bucket_index = 0; bucket_index < kBuckets; bucket_index++) {
      base::AtomicValue<uint32_t>* current_bucket =
          bucket[bucket_index].Value();
      if (current_bucket != nullptr) {
        int in_bucket_count = 0;
        int cell_offset = bucket_index * kBitsPerBucket;
        for (int i = 0; i < kCellsPerBucket; i++, cell_offset += kBitsPerCell) {
          if (current_bucket[i].Value()) {
            uint32_t cell = current_bucket[i].Value();
            uint32_t old_cell = cell;
            uint32_t mask = 0;
            while (cell) {
              int bit_offset = base::bits::CountTrailingZeros32(cell);
              uint32_t bit_mask = 1u << bit_offset;
              uint32_t slot = (cell_offset + bit_offset) << kPointerSizeLog2;
              if (callback(page_start_ + slot) == KEEP_SLOT) {
                ++in_bucket_count;
              } else {
                mask |= bit_mask;
              }
              cell ^= bit_mask;
            }
            uint32_t new_cell = old_cell & ~mask;
            if (old_cell != new_cell) {
              while (!current_bucket[i].TrySetValue(old_cell, new_cell)) {
                // If TrySetValue fails, the cell must have changed. We just
                // have to read the current value of the cell, & it with the
                // computed value, and retry. We can do this, because this
                // method will only be called on the main thread and filtering
                // threads will only remove slots.
                old_cell = current_bucket[i].Value();
                new_cell = old_cell & ~mask;
              }
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

  void FreeToBeFreedBuckets() {
    base::LockGuard<base::Mutex> guard(&to_be_freed_buckets_mutex_);
    while (!to_be_freed_buckets_.empty()) {
      base::AtomicValue<uint32_t>* top = to_be_freed_buckets_.top();
      to_be_freed_buckets_.pop();
      DeleteArray<base::AtomicValue<uint32_t>>(top);
    }
  }

 private:
  static const int kMaxSlots = (1 << kPageSizeBits) / kPointerSize;
  static const int kCellsPerBucket = 32;
  static const int kCellsPerBucketLog2 = 5;
  static const int kBitsPerCell = 32;
  static const int kBitsPerCellLog2 = 5;
  static const int kBitsPerBucket = kCellsPerBucket * kBitsPerCell;
  static const int kBitsPerBucketLog2 = kCellsPerBucketLog2 + kBitsPerCellLog2;
  static const int kBuckets = kMaxSlots / kCellsPerBucket / kBitsPerCell;

  base::AtomicValue<uint32_t>* AllocateBucket() {
    base::AtomicValue<uint32_t>* result =
        NewArray<base::AtomicValue<uint32_t>>(kCellsPerBucket);
    for (int i = 0; i < kCellsPerBucket; i++) {
      result[i].SetValue(0);
    }
    return result;
  }

  void ClearBucket(base::AtomicValue<uint32_t>* bucket, int start_cell,
                   int end_cell) {
    DCHECK_GE(start_cell, 0);
    DCHECK_LE(end_cell, kCellsPerBucket);
    int current_cell = start_cell;
    while (current_cell < kCellsPerBucket) {
      bucket[current_cell].SetValue(0);
      current_cell++;
    }
  }

  void PreFreeEmptyBucket(int bucket_index) {
    base::AtomicValue<uint32_t>* bucket_ptr = bucket[bucket_index].Value();
    if (bucket_ptr != nullptr) {
      base::LockGuard<base::Mutex> guard(&to_be_freed_buckets_mutex_);
      to_be_freed_buckets_.push(bucket_ptr);
      bucket[bucket_index].SetValue(nullptr);
    }
  }

  void ReleaseBucket(int bucket_index) {
    DeleteArray<base::AtomicValue<uint32_t>>(bucket[bucket_index].Value());
    bucket[bucket_index].SetValue(nullptr);
  }

  void ClearCell(int bucket_index, int cell_index, uint32_t mask) {
    if (bucket_index < kBuckets) {
      base::AtomicValue<uint32_t>* cells = bucket[bucket_index].Value();
      if (cells != nullptr) {
        uint32_t cell = cells[cell_index].Value();
        if (cell) cells[cell_index].SetBits(0, mask);
      }
    } else {
      // GCC bug 59124: Emits wrong warnings
      // "array subscript is above array bounds"
      UNREACHABLE();
    }
  }

  // Converts the slot offset into bucket/cell/bit index.
  void SlotToIndices(int slot_offset, int* bucket_index, int* cell_index,
                     int* bit_index) {
    DCHECK_EQ(slot_offset % kPointerSize, 0);
    int slot = slot_offset >> kPointerSizeLog2;
    DCHECK(slot >= 0 && slot <= kMaxSlots);
    *bucket_index = slot >> kBitsPerBucketLog2;
    *cell_index = (slot >> kBitsPerCellLog2) & (kCellsPerBucket - 1);
    *bit_index = slot & (kBitsPerCell - 1);
  }

  base::AtomicValue<base::AtomicValue<uint32_t>*> bucket[kBuckets];
  Address page_start_;
  base::Mutex to_be_freed_buckets_mutex_;
  std::stack<base::AtomicValue<uint32_t>*> to_be_freed_buckets_;
};

enum SlotType {
  EMBEDDED_OBJECT_SLOT,
  OBJECT_SLOT,
  CELL_TARGET_SLOT,
  CODE_TARGET_SLOT,
  CODE_ENTRY_SLOT,
  DEBUG_TARGET_SLOT,
  CLEARED_SLOT
};

// Data structure for maintaining a multiset of typed slots in a page.
// Typed slots can only appear in Code and JSFunction objects, so
// the maximum possible offset is limited by the LargePage::kMaxCodePageSize.
// The implementation is a chain of chunks, where each chunks is an array of
// encoded (slot type, slot offset) pairs.
// There is no duplicate detection and we do not expect many duplicates because
// typed slots contain V8 internal pointers that are not directly exposed to JS.
class TypedSlotSet {
 public:
  enum IterationMode { PREFREE_EMPTY_CHUNKS, KEEP_EMPTY_CHUNKS };

  typedef std::pair<SlotType, uint32_t> TypeAndOffset;

  struct TypedSlot {
    TypedSlot() {
      type_and_offset_.SetValue(0);
      host_offset_.SetValue(0);
    }

    TypedSlot(SlotType type, uint32_t host_offset, uint32_t offset) {
      type_and_offset_.SetValue(TypeField::encode(type) |
                                OffsetField::encode(offset));
      host_offset_.SetValue(host_offset);
    }

    bool operator==(const TypedSlot other) {
      return type_and_offset_.Value() == other.type_and_offset_.Value() &&
             host_offset_.Value() == other.host_offset_.Value();
    }

    bool operator!=(const TypedSlot other) { return !(*this == other); }

    SlotType type() { return TypeField::decode(type_and_offset_.Value()); }

    uint32_t offset() { return OffsetField::decode(type_and_offset_.Value()); }

    TypeAndOffset GetTypeAndOffset() {
      uint32_t type_and_offset = type_and_offset_.Value();
      return std::make_pair(TypeField::decode(type_and_offset),
                            OffsetField::decode(type_and_offset));
    }

    uint32_t host_offset() { return host_offset_.Value(); }

    void Set(TypedSlot slot) {
      type_and_offset_.SetValue(slot.type_and_offset_.Value());
      host_offset_.SetValue(slot.host_offset_.Value());
    }

    void Clear() {
      type_and_offset_.SetValue(TypeField::encode(CLEARED_SLOT) |
                                OffsetField::encode(0));
      host_offset_.SetValue(0);
    }

    base::AtomicValue<uint32_t> type_and_offset_;
    base::AtomicValue<uint32_t> host_offset_;
  };
  static const int kMaxOffset = 1 << 29;

  explicit TypedSlotSet(Address page_start) : page_start_(page_start) {
    chunk_.SetValue(new Chunk(nullptr, kInitialBufferSize));
  }

  ~TypedSlotSet() {
    Chunk* chunk = chunk_.Value();
    while (chunk != nullptr) {
      Chunk* next = chunk->next.Value();
      delete chunk;
      chunk = next;
    }
    FreeToBeFreedChunks();
  }

  // The slot offset specifies a slot at address page_start_ + offset.
  // This method can only be called on the main thread.
  void Insert(SlotType type, uint32_t host_offset, uint32_t offset) {
    TypedSlot slot(type, host_offset, offset);
    Chunk* top_chunk = chunk_.Value();
    if (!top_chunk) {
      top_chunk = new Chunk(nullptr, kInitialBufferSize);
      chunk_.SetValue(top_chunk);
    }
    if (!top_chunk->AddSlot(slot)) {
      Chunk* new_top_chunk =
          new Chunk(top_chunk, NextCapacity(top_chunk->capacity.Value()));
      bool added = new_top_chunk->AddSlot(slot);
      chunk_.SetValue(new_top_chunk);
      DCHECK(added);
      USE(added);
    }
  }

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  //
  // Sample usage:
  // Iterate([](SlotType slot_type, Address slot_address) {
  //    if (good(slot_type, slot_address)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  template <typename Callback>
  int Iterate(Callback callback, IterationMode mode) {
    STATIC_ASSERT(CLEARED_SLOT < 8);
    Chunk* chunk = chunk_.Value();
    Chunk* previous = nullptr;
    int new_count = 0;
    while (chunk != nullptr) {
      TypedSlot* buffer = chunk->buffer.Value();
      int count = chunk->count.Value();
      bool empty = true;
      for (int i = 0; i < count; i++) {
        // Order is important here. We have to read out the slot type last to
        // observe the concurrent removal case consistently.
        Address host_addr = page_start_ + buffer[i].host_offset();
        TypeAndOffset type_and_offset = buffer[i].GetTypeAndOffset();
        SlotType type = type_and_offset.first;
        if (type != CLEARED_SLOT) {
          Address addr = page_start_ + type_and_offset.second;
          if (callback(type, host_addr, addr) == KEEP_SLOT) {
            new_count++;
            empty = false;
          } else {
            buffer[i].Clear();
          }
        }
      }

      Chunk* next = chunk->next.Value();
      if (mode == PREFREE_EMPTY_CHUNKS && empty) {
        // We remove the chunk from the list but let it still point its next
        // chunk to allow concurrent iteration.
        if (previous) {
          previous->next.SetValue(next);
        } else {
          chunk_.SetValue(next);
        }
        base::LockGuard<base::Mutex> guard(&to_be_freed_chunks_mutex_);
        to_be_freed_chunks_.push(chunk);
      } else {
        previous = chunk;
      }
      chunk = next;
    }
    return new_count;
  }

  void FreeToBeFreedChunks() {
    base::LockGuard<base::Mutex> guard(&to_be_freed_chunks_mutex_);
    while (!to_be_freed_chunks_.empty()) {
      Chunk* top = to_be_freed_chunks_.top();
      to_be_freed_chunks_.pop();
      delete top;
    }
  }

  void RemoveInvaldSlots(std::map<uint32_t, uint32_t>& invalid_ranges) {
    Chunk* chunk = chunk_.Value();
    while (chunk != nullptr) {
      TypedSlot* buffer = chunk->buffer.Value();
      int count = chunk->count.Value();
      for (int i = 0; i < count; i++) {
        uint32_t host_offset = buffer[i].host_offset();
        std::map<uint32_t, uint32_t>::iterator upper_bound =
            invalid_ranges.upper_bound(host_offset);
        if (upper_bound == invalid_ranges.begin()) continue;
        // upper_bounds points to the invalid range after the given slot. Hence,
        // we have to go to the previous element.
        upper_bound--;
        DCHECK_LE(upper_bound->first, host_offset);
        if (upper_bound->second > host_offset) {
          buffer[i].Clear();
        }
      }
      chunk = chunk->next.Value();
    }
  }

 private:
  static const int kInitialBufferSize = 100;
  static const int kMaxBufferSize = 16 * KB;

  static int NextCapacity(int capacity) {
    return Min(kMaxBufferSize, capacity * 2);
  }

  class OffsetField : public BitField<int, 0, 29> {};
  class TypeField : public BitField<SlotType, 29, 3> {};

  struct Chunk : Malloced {
    explicit Chunk(Chunk* next_chunk, int chunk_capacity) {
      count.SetValue(0);
      capacity.SetValue(chunk_capacity);
      buffer.SetValue(NewArray<TypedSlot>(chunk_capacity));
      next.SetValue(next_chunk);
    }
    bool AddSlot(TypedSlot slot) {
      int current_count = count.Value();
      if (current_count == capacity.Value()) return false;
      TypedSlot* current_buffer = buffer.Value();
      // Order is important here. We have to write the slot first before
      // increasing the counter to guarantee that a consistent state is
      // observed by concurrent threads.
      current_buffer[current_count].Set(slot);
      count.SetValue(current_count + 1);
      return true;
    }
    ~Chunk() { DeleteArray(buffer.Value()); }
    base::AtomicValue<Chunk*> next;
    base::AtomicValue<int> count;
    base::AtomicValue<int> capacity;
    base::AtomicValue<TypedSlot*> buffer;
  };

  Address page_start_;
  base::AtomicValue<Chunk*> chunk_;
  base::Mutex to_be_freed_chunks_mutex_;
  std::stack<Chunk*> to_be_freed_chunks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SLOT_SET_H
