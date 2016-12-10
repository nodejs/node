// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SLOT_SET_H
#define V8_SLOT_SET_H

#include "src/allocation.h"
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
  SlotSet() {
    for (int i = 0; i < kBuckets; i++) {
      bucket[i] = nullptr;
    }
  }

  ~SlotSet() {
    for (int i = 0; i < kBuckets; i++) {
      ReleaseBucket(i);
    }
  }

  void SetPageStart(Address page_start) { page_start_ = page_start; }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  void Insert(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    if (bucket[bucket_index] == nullptr) {
      bucket[bucket_index] = AllocateBucket();
    }
    bucket[bucket_index][cell_index] |= 1u << bit_index;
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  void Remove(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    if (bucket[bucket_index] != nullptr) {
      uint32_t cell = bucket[bucket_index][cell_index];
      if (cell) {
        uint32_t bit_mask = 1u << bit_index;
        if (cell & bit_mask) {
          bucket[bucket_index][cell_index] ^= bit_mask;
        }
      }
    }
  }

  // The slot offsets specify a range of slots at addresses:
  // [page_start_ + start_offset ... page_start_ + end_offset).
  void RemoveRange(int start_offset, int end_offset) {
    CHECK_LE(end_offset, 1 << kPageSizeBits);
    DCHECK_LE(start_offset, end_offset);
    int start_bucket, start_cell, start_bit;
    SlotToIndices(start_offset, &start_bucket, &start_cell, &start_bit);
    int end_bucket, end_cell, end_bit;
    SlotToIndices(end_offset, &end_bucket, &end_cell, &end_bit);
    uint32_t start_mask = (1u << start_bit) - 1;
    uint32_t end_mask = ~((1u << end_bit) - 1);
    if (start_bucket == end_bucket && start_cell == end_cell) {
      MaskCell(start_bucket, start_cell, start_mask | end_mask);
      return;
    }
    int current_bucket = start_bucket;
    int current_cell = start_cell;
    MaskCell(current_bucket, current_cell, start_mask);
    current_cell++;
    if (current_bucket < end_bucket) {
      if (bucket[current_bucket] != nullptr) {
        while (current_cell < kCellsPerBucket) {
          bucket[current_bucket][current_cell] = 0;
          current_cell++;
        }
      }
      // The rest of the current bucket is cleared.
      // Move on to the next bucket.
      current_bucket++;
      current_cell = 0;
    }
    DCHECK(current_bucket == end_bucket ||
           (current_bucket < end_bucket && current_cell == 0));
    while (current_bucket < end_bucket) {
      ReleaseBucket(current_bucket);
      current_bucket++;
    }
    // All buckets between start_bucket and end_bucket are cleared.
    DCHECK(current_bucket == end_bucket && current_cell <= end_cell);
    if (current_bucket == kBuckets || bucket[current_bucket] == nullptr) {
      return;
    }
    while (current_cell < end_cell) {
      bucket[current_bucket][current_cell] = 0;
      current_cell++;
    }
    // All cells between start_cell and end_cell are cleared.
    DCHECK(current_bucket == end_bucket && current_cell == end_cell);
    MaskCell(end_bucket, end_cell, end_mask);
  }

  // The slot offset specifies a slot at address page_start_ + slot_offset.
  bool Lookup(int slot_offset) {
    int bucket_index, cell_index, bit_index;
    SlotToIndices(slot_offset, &bucket_index, &cell_index, &bit_index);
    if (bucket[bucket_index] != nullptr) {
      uint32_t cell = bucket[bucket_index][cell_index];
      return (cell & (1u << bit_index)) != 0;
    }
    return false;
  }

  // Iterate over all slots in the set and for each slot invoke the callback.
  // If the callback returns REMOVE_SLOT then the slot is removed from the set.
  // Returns the new number of slots.
  //
  // Sample usage:
  // Iterate([](Address slot_address) {
  //    if (good(slot_address)) return KEEP_SLOT;
  //    else return REMOVE_SLOT;
  // });
  template <typename Callback>
  int Iterate(Callback callback) {
    int new_count = 0;
    for (int bucket_index = 0; bucket_index < kBuckets; bucket_index++) {
      if (bucket[bucket_index] != nullptr) {
        int in_bucket_count = 0;
        uint32_t* current_bucket = bucket[bucket_index];
        int cell_offset = bucket_index * kBitsPerBucket;
        for (int i = 0; i < kCellsPerBucket; i++, cell_offset += kBitsPerCell) {
          if (current_bucket[i]) {
            uint32_t cell = current_bucket[i];
            uint32_t old_cell = cell;
            uint32_t new_cell = cell;
            while (cell) {
              int bit_offset = base::bits::CountTrailingZeros32(cell);
              uint32_t bit_mask = 1u << bit_offset;
              uint32_t slot = (cell_offset + bit_offset) << kPointerSizeLog2;
              if (callback(page_start_ + slot) == KEEP_SLOT) {
                ++in_bucket_count;
              } else {
                new_cell ^= bit_mask;
              }
              cell ^= bit_mask;
            }
            if (old_cell != new_cell) {
              current_bucket[i] = new_cell;
            }
          }
        }
        if (in_bucket_count == 0) {
          ReleaseBucket(bucket_index);
        }
        new_count += in_bucket_count;
      }
    }
    return new_count;
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

  uint32_t* AllocateBucket() {
    uint32_t* result = NewArray<uint32_t>(kCellsPerBucket);
    for (int i = 0; i < kCellsPerBucket; i++) {
      result[i] = 0;
    }
    return result;
  }

  void ReleaseBucket(int bucket_index) {
    DeleteArray<uint32_t>(bucket[bucket_index]);
    bucket[bucket_index] = nullptr;
  }

  void MaskCell(int bucket_index, int cell_index, uint32_t mask) {
    if (bucket_index < kBuckets) {
      uint32_t* cells = bucket[bucket_index];
      if (cells != nullptr && cells[cell_index] != 0) {
        cells[cell_index] &= mask;
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

  uint32_t* bucket[kBuckets];
  Address page_start_;
};

enum SlotType {
  EMBEDDED_OBJECT_SLOT,
  OBJECT_SLOT,
  CELL_TARGET_SLOT,
  CODE_TARGET_SLOT,
  CODE_ENTRY_SLOT,
  DEBUG_TARGET_SLOT,
  NUMBER_OF_SLOT_TYPES
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
  struct TypedSlot {
    TypedSlot() : type_and_offset_(0), host_offset_(0) {}

    TypedSlot(SlotType type, uint32_t host_offset, uint32_t offset)
        : type_and_offset_(TypeField::encode(type) |
                           OffsetField::encode(offset)),
          host_offset_(host_offset) {}

    bool operator==(const TypedSlot other) {
      return type_and_offset_ == other.type_and_offset_ &&
             host_offset_ == other.host_offset_;
    }

    bool operator!=(const TypedSlot other) { return !(*this == other); }

    SlotType type() { return TypeField::decode(type_and_offset_); }

    uint32_t offset() { return OffsetField::decode(type_and_offset_); }

    uint32_t host_offset() { return host_offset_; }

    uint32_t type_and_offset_;
    uint32_t host_offset_;
  };
  static const int kMaxOffset = 1 << 29;

  explicit TypedSlotSet(Address page_start) : page_start_(page_start) {
    chunk_ = new Chunk(nullptr, kInitialBufferSize);
  }

  ~TypedSlotSet() {
    Chunk* chunk = chunk_;
    while (chunk != nullptr) {
      Chunk* next = chunk->next;
      delete chunk;
      chunk = next;
    }
  }

  // The slot offset specifies a slot at address page_start_ + offset.
  void Insert(SlotType type, uint32_t host_offset, uint32_t offset) {
    TypedSlot slot(type, host_offset, offset);
    if (!chunk_->AddSlot(slot)) {
      chunk_ = new Chunk(chunk_, NextCapacity(chunk_->capacity));
      bool added = chunk_->AddSlot(slot);
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
  int Iterate(Callback callback) {
    STATIC_ASSERT(NUMBER_OF_SLOT_TYPES < 8);
    const TypedSlot kRemovedSlot(NUMBER_OF_SLOT_TYPES, 0, 0);
    Chunk* chunk = chunk_;
    int new_count = 0;
    while (chunk != nullptr) {
      TypedSlot* buffer = chunk->buffer;
      int count = chunk->count;
      for (int i = 0; i < count; i++) {
        TypedSlot slot = buffer[i];
        if (slot != kRemovedSlot) {
          SlotType type = slot.type();
          Address addr = page_start_ + slot.offset();
          Address host_addr = page_start_ + slot.host_offset();
          if (callback(type, host_addr, addr) == KEEP_SLOT) {
            new_count++;
          } else {
            buffer[i] = kRemovedSlot;
          }
        }
      }
      chunk = chunk->next;
    }
    return new_count;
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
    explicit Chunk(Chunk* next_chunk, int capacity)
        : next(next_chunk), count(0), capacity(capacity) {
      buffer = NewArray<TypedSlot>(capacity);
    }
    bool AddSlot(TypedSlot slot) {
      if (count == capacity) return false;
      buffer[count++] = slot;
      return true;
    }
    ~Chunk() { DeleteArray(buffer); }
    Chunk* next;
    int count;
    int capacity;
    TypedSlot* buffer;
  };

  Address page_start_;
  Chunk* chunk_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SLOT_SET_H
