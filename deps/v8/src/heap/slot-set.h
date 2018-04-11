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
  // Iterate([](Address slot_address) {
  //    if (good(slot_address)) return KEEP_SLOT;
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
    base::LockGuard<base::Mutex> guard(&to_be_freed_buckets_mutex_);
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
    base::LockGuard<base::Mutex> guard(&to_be_freed_buckets_mutex_);
    while (!to_be_freed_buckets_.empty()) {
      Bucket top = to_be_freed_buckets_.top();
      to_be_freed_buckets_.pop();
      DeleteArray<uint32_t>(top);
    }
    DCHECK_EQ(0u, to_be_freed_buckets_.size());
  }

 private:
  typedef uint32_t* Bucket;
  static const int kMaxSlots = (1 << kPageSizeBits) / kPointerSize;
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
      base::LockGuard<base::Mutex> guard(&to_be_freed_buckets_mutex_);
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
    DCHECK_EQ(slot_offset % kPointerSize, 0);
    int slot = slot_offset >> kPointerSizeLog2;
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
    TypedSlot() : type_and_offset_(0), host_offset_(0) {}

    TypedSlot(SlotType type, uint32_t host_offset, uint32_t offset)
        : type_and_offset_(TypeField::encode(type) |
                           OffsetField::encode(offset)),
          host_offset_(host_offset) {}

    bool operator==(const TypedSlot other) {
      return type_and_offset() == other.type_and_offset() &&
             host_offset() == other.host_offset();
    }

    bool operator!=(const TypedSlot other) { return !(*this == other); }

    SlotType type() const { return TypeField::decode(type_and_offset()); }

    uint32_t offset() const { return OffsetField::decode(type_and_offset()); }

    TypeAndOffset GetTypeAndOffset() const {
      uint32_t t_and_o = type_and_offset();
      return std::make_pair(TypeField::decode(t_and_o),
                            OffsetField::decode(t_and_o));
    }

    uint32_t type_and_offset() const {
      return base::AsAtomic32::Acquire_Load(&type_and_offset_);
    }

    uint32_t host_offset() const {
      return base::AsAtomic32::Acquire_Load(&host_offset_);
    }

    void Set(TypedSlot slot) {
      base::AsAtomic32::Release_Store(&type_and_offset_,
                                      slot.type_and_offset());
      base::AsAtomic32::Release_Store(&host_offset_, slot.host_offset());
    }

    void Clear() {
      base::AsAtomic32::Release_Store(
          &type_and_offset_,
          TypeField::encode(CLEARED_SLOT) | OffsetField::encode(0));
      base::AsAtomic32::Release_Store(&host_offset_, 0);
    }

    uint32_t type_and_offset_;
    uint32_t host_offset_;
  };
  static const int kMaxOffset = 1 << 29;

  explicit TypedSlotSet(Address page_start)
      : page_start_(page_start), top_(new Chunk(nullptr, kInitialBufferSize)) {}

  ~TypedSlotSet() {
    Chunk* chunk = load_top();
    while (chunk != nullptr) {
      Chunk* n = chunk->next();
      delete chunk;
      chunk = n;
    }
    FreeToBeFreedChunks();
  }

  // The slot offset specifies a slot at address page_start_ + offset.
  // This method can only be called on the main thread.
  void Insert(SlotType type, uint32_t host_offset, uint32_t offset) {
    TypedSlot slot(type, host_offset, offset);
    Chunk* top_chunk = load_top();
    if (!top_chunk) {
      top_chunk = new Chunk(nullptr, kInitialBufferSize);
      set_top(top_chunk);
    }
    if (!top_chunk->AddSlot(slot)) {
      Chunk* new_top_chunk =
          new Chunk(top_chunk, NextCapacity(top_chunk->capacity()));
      bool added = new_top_chunk->AddSlot(slot);
      set_top(new_top_chunk);
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
    Chunk* chunk = load_top();
    Chunk* previous = nullptr;
    int new_count = 0;
    while (chunk != nullptr) {
      TypedSlot* buf = chunk->buffer();
      bool empty = true;
      for (int i = 0; i < chunk->count(); i++) {
        // Order is important here. We have to read out the slot type last to
        // observe the concurrent removal case consistently.
        Address host_addr = page_start_ + buf[i].host_offset();
        TypeAndOffset type_and_offset = buf[i].GetTypeAndOffset();
        SlotType type = type_and_offset.first;
        if (type != CLEARED_SLOT) {
          Address addr = page_start_ + type_and_offset.second;
          if (callback(type, host_addr, addr) == KEEP_SLOT) {
            new_count++;
            empty = false;
          } else {
            buf[i].Clear();
          }
        }
      }

      Chunk* n = chunk->next();
      if (mode == PREFREE_EMPTY_CHUNKS && empty) {
        // We remove the chunk from the list but let it still point its next
        // chunk to allow concurrent iteration.
        if (previous) {
          previous->set_next(n);
        } else {
          set_top(n);
        }
        base::LockGuard<base::Mutex> guard(&to_be_freed_chunks_mutex_);
        to_be_freed_chunks_.push(chunk);
      } else {
        previous = chunk;
      }
      chunk = n;
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
    Chunk* chunk = load_top();
    while (chunk != nullptr) {
      TypedSlot* buf = chunk->buffer();
      for (int i = 0; i < chunk->count(); i++) {
        uint32_t host_offset = buf[i].host_offset();
        std::map<uint32_t, uint32_t>::iterator upper_bound =
            invalid_ranges.upper_bound(host_offset);
        if (upper_bound == invalid_ranges.begin()) continue;
        // upper_bounds points to the invalid range after the given slot. Hence,
        // we have to go to the previous element.
        upper_bound--;
        DCHECK_LE(upper_bound->first, host_offset);
        if (upper_bound->second > host_offset) {
          buf[i].Clear();
        }
      }
      chunk = chunk->next();
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
      next_ = next_chunk;
      buffer_ = NewArray<TypedSlot>(chunk_capacity);
      capacity_ = chunk_capacity;
      count_ = 0;
    }

    ~Chunk() { DeleteArray(buffer_); }

    bool AddSlot(TypedSlot slot) {
      int current_count = count();
      if (current_count == capacity()) return false;
      TypedSlot* current_buffer = buffer();
      // Order is important here. We have to write the slot first before
      // increasing the counter to guarantee that a consistent state is
      // observed by concurrent threads.
      current_buffer[current_count].Set(slot);
      set_count(current_count + 1);
      return true;
    }

    Chunk* next() const { return base::AsAtomicPointer::Acquire_Load(&next_); }

    void set_next(Chunk* n) {
      return base::AsAtomicPointer::Release_Store(&next_, n);
    }

    TypedSlot* buffer() const { return buffer_; }

    int32_t capacity() const { return capacity_; }

    int32_t count() const { return base::AsAtomic32::Acquire_Load(&count_); }

    void set_count(int32_t new_value) {
      base::AsAtomic32::Release_Store(&count_, new_value);
    }

   private:
    Chunk* next_;
    TypedSlot* buffer_;
    int32_t capacity_;
    int32_t count_;
  };

  Chunk* load_top() { return base::AsAtomicPointer::Acquire_Load(&top_); }

  void set_top(Chunk* c) { base::AsAtomicPointer::Release_Store(&top_, c); }

  Address page_start_;
  Chunk* top_;
  base::Mutex to_be_freed_chunks_mutex_;
  std::stack<Chunk*> to_be_freed_chunks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SLOT_SET_H_
