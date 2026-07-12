#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory_tracker-inl.h>
#include <util.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace node::quic {

// ArenaPool<T> is a block-based arena allocator for fixed-size objects.
//
// Objects are allocated from contiguous memory blocks ("slabs"), reducing
// heap fragmentation and allocation overhead. Each block contains a fixed
// number of equally-sized slots. Free slots form an intrusive singly-linked
// list for O(1) acquire/release.
//
// Each slot can optionally hold extra bytes after T (e.g., a trailing data
// buffer). The extra_bytes parameter is set at pool construction time and
// applies uniformly to all slots. All slots are allocated at the same size
// regardless of how much of the extra space is actually used, to prevent
// fragmentation.
//
// ArenaPool<T>::Ptr is a move-only RAII smart pointer that releases slots
// back to the pool on destruction. Ptr::release() detaches ownership for
// handoff to external systems (e.g., libuv), after which the caller must
// eventually call ArenaPool<T>::Release(raw_ptr) to return the slot.
//
// The pool supports lazy GC: when the ratio of free slots to total slots
// exceeds a threshold, fully-unused blocks are reclaimed. At least one
// block is always retained.
template <typename T>
class ArenaPool final : public MemoryRetainer {
 public:
  class Ptr;

  // extra_bytes: additional memory available after T in each slot.
  //              All slots are sized identically regardless of how
  //              much extra space is actually used.
  // slots_per_block: number of T slots per allocation block.
  explicit ArenaPool(size_t extra_bytes = 0,
                     size_t slots_per_block = kDefaultSlotsPerBlock);
  ~ArenaPool();

  ArenaPool(const ArenaPool&) = delete;
  ArenaPool& operator=(const ArenaPool&) = delete;
  ArenaPool(ArenaPool&&) = delete;
  ArenaPool& operator=(ArenaPool&&) = delete;

  // Construct T in an acquired slot with forwarded args.
  // Returns an empty Ptr only on allocation failure.
  template <typename... Args>
  [[nodiscard]] Ptr Acquire(Args&&... args);

  // Construct T with (extra_data_ptr, extra_bytes, ...args).
  // Use this for types whose constructor accepts a trailing data
  // buffer as its first two parameters.
  template <typename... Args>
  [[nodiscard]] Ptr AcquireExtra(Args&&... args);

  // Release a raw T* previously detached via Ptr::release().
  // Calls ~T() and returns the slot to the pool's free list.
  // Recovers the pool instance from T*'s slot metadata.
  static void Release(T* obj);

  // Attempt to reclaim fully-unused blocks. Called automatically
  // from Release/ReleaseSlot when the pool is over-provisioned.
  void MaybeGC();

  // Free all unused blocks immediately, keeping at least one.
  void Flush();

  size_t extra_bytes() const { return extra_bytes_; }
  size_t slot_size() const { return slot_size_; }
  size_t total_slots() const { return total_slots_; }
  size_t in_use_count() const { return in_use_count_; }
  size_t block_count() const { return blocks_.size(); }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(ArenaPool)
  SET_SELF_SIZE(ArenaPool)

  static constexpr size_t kDefaultSlotsPerBlock = 128;

 private:
  // -------------------------------------------------------------------
  // Slot layout in memory:
  //
  //   [0, kObjectOffset)                        SlotHeader
  //   [kObjectOffset, kObjectOffset+sizeof(T))   T object
  //   [kObjectOffset+sizeof(T), slot_size_)      extra bytes + padding
  //
  // SlotHeader is only used when the slot is on the free list.
  // When the slot is acquired, the T object occupies its storage
  // and the header fields are not accessed.
  // -------------------------------------------------------------------

  struct Block;

  struct SlotHeader {
    SlotHeader* next_free;
    Block* block;
  };

  struct Block {
    std::unique_ptr<char[]> memory;
    ArenaPool* pool;
    size_t capacity;
    size_t in_use_count;
  };

  static constexpr size_t kObjectOffset =
      (sizeof(SlotHeader) + alignof(T) - 1) & ~(alignof(T) - 1);

  // Slot ↔ T* conversion using the compile-time offset.
  static T* ObjectFromSlot(SlotHeader* slot) {
    return reinterpret_cast<T*>(reinterpret_cast<char*>(slot) + kObjectOffset);
  }

  static SlotHeader* SlotFromObject(T* obj) {
    return reinterpret_cast<SlotHeader*>(reinterpret_cast<char*>(obj) -
                                         kObjectOffset);
  }

  static uint8_t* ExtraDataFromSlot(SlotHeader* slot) {
    return reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(slot) +
                                      kObjectOffset + sizeof(T));
  }

  SlotHeader* SlotAt(Block* block, size_t index) {
    return reinterpret_cast<SlotHeader*>(block->memory.get() +
                                         index * slot_size_);
  }

  SlotHeader* AcquireSlot();
  void ReleaseSlot(SlotHeader* slot);
  bool Grow();
  void FreeEmptyBlocks();
  void RemoveBlockFromFreeList(Block* block);

  const size_t extra_bytes_;
  const size_t slots_per_block_;
  const size_t slot_size_;

  SlotHeader* free_list_ = nullptr;
  std::vector<std::unique_ptr<Block>> blocks_;
  size_t total_slots_ = 0;
  size_t in_use_count_ = 0;
};

// =====================================================================
// ArenaPool<T>::Ptr — Move-only RAII smart pointer
// =====================================================================

template <typename T>
class ArenaPool<T>::Ptr final {
 public:
  Ptr() = default;
  ~Ptr() { reset(); }

  Ptr(Ptr&& other) noexcept : slot_(other.slot_) { other.slot_ = nullptr; }

  Ptr& operator=(Ptr&& other) noexcept {
    if (this != &other) {
      reset();
      slot_ = other.slot_;
      other.slot_ = nullptr;
    }
    return *this;
  }

  Ptr(const Ptr&) = delete;
  Ptr& operator=(const Ptr&) = delete;

  T* get() const { return slot_ ? ObjectFromSlot(slot_) : nullptr; }
  T* operator->() const {
    DCHECK(slot_);
    return ObjectFromSlot(slot_);
  }
  T& operator*() const {
    DCHECK(slot_);
    return *ObjectFromSlot(slot_);
  }
  explicit operator bool() const { return slot_ != nullptr; }

  // Access the extra data region after T in the slot.
  uint8_t* extra_data() const {
    return slot_ ? ExtraDataFromSlot(slot_) : nullptr;
  }
  size_t extra_bytes() const {
    return slot_ ? slot_->block->pool->extra_bytes_ : 0;
  }

  // Detach ownership. The caller takes responsibility for eventually
  // calling ArenaPool<T>::Release(ptr) to destruct T and return
  // the slot to the pool.
  [[nodiscard]] T* release() noexcept {
    if (!slot_) return nullptr;
    T* obj = ObjectFromSlot(slot_);
    slot_ = nullptr;
    return obj;
  }

  // Destruct T and return the slot to the pool. Ptr becomes empty.
  void reset() {
    if (slot_) {
      ObjectFromSlot(slot_)->~T();
      slot_->block->pool->ReleaseSlot(slot_);
      slot_ = nullptr;
    }
  }

 private:
  friend class ArenaPool<T>;
  explicit Ptr(SlotHeader* slot) : slot_(slot) {}
  SlotHeader* slot_ = nullptr;
};

// =====================================================================
// ArenaPool<T> implementation
// =====================================================================

template <typename T>
ArenaPool<T>::ArenaPool(size_t extra_bytes, size_t slots_per_block)
    : extra_bytes_(extra_bytes),
      slots_per_block_(slots_per_block),
      slot_size_(((kObjectOffset + sizeof(T) + extra_bytes) +
                  alignof(std::max_align_t) - 1) &
                 ~(alignof(std::max_align_t) - 1)) {
  DCHECK_GT(slots_per_block, 0);
}

template <typename T>
ArenaPool<T>::~ArenaPool() {
  DCHECK_EQ(in_use_count_, 0);
}

template <typename T>
template <typename... Args>
typename ArenaPool<T>::Ptr ArenaPool<T>::Acquire(Args&&... args) {
  SlotHeader* slot = AcquireSlot();
  if (!slot) return Ptr();
  T* obj = new (ObjectFromSlot(slot)) T(std::forward<Args>(args)...);
  CHECK_EQ(obj, ObjectFromSlot(slot));
  return Ptr(slot);
}

template <typename T>
template <typename... Args>
typename ArenaPool<T>::Ptr ArenaPool<T>::AcquireExtra(Args&&... args) {
  SlotHeader* slot = AcquireSlot();
  if (!slot) return Ptr();
  T* obj = new (ObjectFromSlot(slot))
      T(ExtraDataFromSlot(slot), extra_bytes_, std::forward<Args>(args)...);
  CHECK_EQ(obj, ObjectFromSlot(slot));
  return Ptr(slot);
}

template <typename T>
void ArenaPool<T>::Release(T* obj) {
  DCHECK_NOT_NULL(obj);
  SlotHeader* slot = SlotFromObject(obj);
  DCHECK_NOT_NULL(slot->block);
  DCHECK_NOT_NULL(slot->block->pool);
  obj->~T();
  slot->block->pool->ReleaseSlot(slot);
}

template <typename T>
typename ArenaPool<T>::SlotHeader* ArenaPool<T>::AcquireSlot() {
  if (!free_list_) {
    if (!Grow()) return nullptr;
  }
  DCHECK_NOT_NULL(free_list_);
  SlotHeader* slot = free_list_;
  free_list_ = slot->next_free;
  slot->next_free = nullptr;
  slot->block->in_use_count++;
  in_use_count_++;
  return slot;
}

template <typename T>
void ArenaPool<T>::ReleaseSlot(SlotHeader* slot) {
  DCHECK_NOT_NULL(slot);
  DCHECK_NOT_NULL(slot->block);
  DCHECK_GT(slot->block->in_use_count, 0);
  DCHECK_GT(in_use_count_, 0);

  slot->block->in_use_count--;
  in_use_count_--;
  slot->next_free = free_list_;
  free_list_ = slot;

  MaybeGC();
}

template <typename T>
bool ArenaPool<T>::Grow() {
  auto block = std::make_unique<Block>();
  block->pool = this;
  block->capacity = slots_per_block_;
  block->in_use_count = 0;
  block->memory.reset(new char[slots_per_block_ * slot_size_]());

  // Initialize slot headers and chain onto free list.
  for (size_t i = 0; i < slots_per_block_; i++) {
    SlotHeader* slot = SlotAt(block.get(), i);
    slot->block = block.get();
    slot->next_free = free_list_;
    free_list_ = slot;
  }

  total_slots_ += slots_per_block_;
  blocks_.push_back(std::move(block));
  return true;
}

template <typename T>
void ArenaPool<T>::MaybeGC() {
  // Only GC when we have excess capacity: more than one block and
  // less than half the slots are in use.
  if (blocks_.size() <= 1 || in_use_count_ >= total_slots_ / 2) return;
  FreeEmptyBlocks();
}

template <typename T>
void ArenaPool<T>::Flush() {
  FreeEmptyBlocks();
}

template <typename T>
void ArenaPool<T>::FreeEmptyBlocks() {
  for (auto it = blocks_.begin(); it != blocks_.end();) {
    if ((*it)->in_use_count == 0 && blocks_.size() > 1) {
      RemoveBlockFromFreeList(it->get());
      total_slots_ -= (*it)->capacity;
      it = blocks_.erase(it);
    } else {
      ++it;
    }
  }
}

template <typename T>
void ArenaPool<T>::RemoveBlockFromFreeList(Block* block) {
  char* block_start = block->memory.get();
  char* block_end = block_start + block->capacity * slot_size_;

  SlotHeader** pp = &free_list_;
  while (*pp) {
    char* addr = reinterpret_cast<char*>(*pp);
    if (addr >= block_start && addr < block_end) {
      *pp = (*pp)->next_free;
    } else {
      pp = &(*pp)->next_free;
    }
  }
}

template <typename T>
void ArenaPool<T>::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("blocks", total_slots_ * slot_size_);
}

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
