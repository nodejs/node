// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_CHUNK_LIST_H_
#define V8_ZONE_ZONE_CHUNK_LIST_H_

#include <algorithm>

#include "src/base/iterator.h"
#include "src/common/globals.h"
#include "src/utils/memcopy.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T, bool backwards, bool modifiable>
class ZoneChunkListIterator;

// A zone-backed hybrid of a vector and a linked list. Use it if you need a
// collection that
// * needs to grow indefinitely,
// * will mostly grow at the back, but may sometimes grow in front as well
// (preferably in batches),
// * needs to have very low overhead,
// * offers forward- and backwards-iteration,
// * offers relatively fast seeking,
// * offers bidirectional iterators,
// * can be rewound without freeing the backing store,
// * can be split and joined again efficiently.
// This list will maintain a doubly-linked list of chunks. When a chunk is
// filled up, a new one gets appended. New chunks appended at the end will
// grow in size up to a certain limit to avoid over-allocation and to keep
// the zone clean. Chunks may be partially filled. In particular, chunks may
// be empty after rewinding, such that they can be reused when inserting
// again at a later point in time.
template <typename T>
class ZoneChunkList : public ZoneObject {
 public:
  using iterator = ZoneChunkListIterator<T, false, true>;
  using const_iterator = ZoneChunkListIterator<T, false, false>;
  using reverse_iterator = ZoneChunkListIterator<T, true, true>;
  using const_reverse_iterator = ZoneChunkListIterator<T, true, false>;

  static constexpr uint32_t kInitialChunkCapacity = 8;
  static constexpr uint32_t kMaxChunkCapacity = 256;

  explicit ZoneChunkList(Zone* zone) : zone_(zone) {}

  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ZoneChunkList);

  size_t size() const { return size_; }
  bool empty() const { return size() == 0; }

  T& front();
  const T& front() const;
  T& back();
  const T& back() const;

  void push_back(const T& item);

  // If the first chunk has space, inserts into it at the front. Otherwise
  // allocate a new chunk with the same growth strategy as `push_back`.
  // This limits the amount of copying to O(`kMaxChunkCapacity`).
  void push_front(const T& item);

  // Cuts the last list elements so at most 'limit' many remain. Does not
  // free the actual memory, since it is zone allocated.
  void Rewind(const size_t limit = 0);

  // Quickly scans the list to retrieve the element at the given index. Will
  // *not* check bounds.
  iterator Find(const size_t index);
  const_iterator Find(const size_t index) const;
  // TODO(heimbuef): Add 'rFind', seeking from the end and returning a
  // reverse iterator.

  // Splits off a new list that contains the elements from `split_begin` to
  // `end()`. The current list is truncated to end just before `split_begin`.
  // This naturally invalidates all iterators, including `split_begin`.
  ZoneChunkList<T> SplitAt(iterator split_begin);
  void Append(ZoneChunkList<T>& other);

  void CopyTo(T* ptr);

  iterator begin() { return iterator::Begin(this); }
  iterator end() { return iterator::End(this); }
  reverse_iterator rbegin() { return reverse_iterator::Begin(this); }
  reverse_iterator rend() { return reverse_iterator::End(this); }
  const_iterator begin() const { return const_iterator::Begin(this); }
  const_iterator end() const { return const_iterator::End(this); }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator::Begin(this);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator::End(this);
  }

  void swap(ZoneChunkList<T>& other) {
    DCHECK_EQ(zone_, other.zone_);
    std::swap(size_, other.size_);
    std::swap(front_, other.front_);
    std::swap(last_nonempty_, other.last_nonempty_);
  }

 private:
  template <typename S, bool backwards, bool modifiable>
  friend class ZoneChunkListIterator;

  struct Chunk {
    uint32_t capacity_ = 0;
    uint32_t position_ = 0;
    Chunk* next_ = nullptr;
    Chunk* previous_ = nullptr;
    T* items() { return reinterpret_cast<T*>(this + 1); }
    const T* items() const { return reinterpret_cast<const T*>(this + 1); }
    uint32_t size() const {
      DCHECK_LE(position_, capacity_);
      return position_;
    }
    bool empty() const { return size() == 0; }
    bool full() const { return size() == capacity_; }
  };

  Chunk* NewChunk(const uint32_t capacity) {
    void* memory = zone_->Allocate<Chunk>(sizeof(Chunk) + capacity * sizeof(T));
    Chunk* chunk = new (memory) Chunk();
    chunk->capacity_ = capacity;
    return chunk;
  }

  static uint32_t NextChunkCapacity(uint32_t previous_capacity) {
    return std::min(previous_capacity * 2, kMaxChunkCapacity);
  }

  struct SeekResult {
    Chunk* chunk_;
    uint32_t chunk_index_;
  };

  // Returns the chunk and relative index of the element at the given global
  // index. Will skip entire chunks and is therefore faster than iterating.
  SeekResult SeekIndex(size_t index) const;

#ifdef DEBUG
  // Check the invariants.
  void Verify() const {
    if (front_ == nullptr) {
      // Initial empty state.
      DCHECK_NULL(last_nonempty_);
      DCHECK_EQ(0, size());
    } else if (empty()) {
      // Special case: Fully rewound list, with only empty chunks.
      DCHECK_EQ(front_, last_nonempty_);
      DCHECK_EQ(0, size());
      for (Chunk* chunk = front_; chunk != nullptr; chunk = chunk->next_) {
        DCHECK(chunk->empty());
      }
    } else {
      // Normal state: Somewhat filled and (partially) rewound.
      DCHECK_NOT_NULL(last_nonempty_);

      size_t size_check = 0;
      bool in_empty_tail = false;
      for (Chunk* chunk = front_; chunk != nullptr; chunk = chunk->next_) {
        // Chunks from `front_` to `last_nonempty_` (inclusive) are non-empty.
        DCHECK_EQ(in_empty_tail, chunk->empty());
        size_check += chunk->size();

        if (chunk == last_nonempty_) {
          in_empty_tail = true;
        }
      }
      DCHECK_EQ(size_check, size());
    }
  }
#endif

  Zone* zone_;

  size_t size_ = 0;
  Chunk* front_ = nullptr;
  Chunk* last_nonempty_ = nullptr;
};

template <typename T, bool backwards, bool modifiable>
class ZoneChunkListIterator
    : public base::iterator<std::bidirectional_iterator_tag, T> {
 private:
  template <typename S>
  using maybe_const = std::conditional_t<modifiable, S, std::add_const_t<S>>;
  using Chunk = maybe_const<typename ZoneChunkList<T>::Chunk>;
  using ChunkList = maybe_const<ZoneChunkList<T>>;

 public:
  maybe_const<T>& operator*() const { return current_->items()[position_]; }
  maybe_const<T>* operator->() const { return &current_->items()[position_]; }
  bool operator==(const ZoneChunkListIterator& other) const {
    return other.current_ == current_ && other.position_ == position_;
  }
  bool operator!=(const ZoneChunkListIterator& other) const {
    return !operator==(other);
  }

  ZoneChunkListIterator& operator++() {
    Move<backwards>();
    return *this;
  }

  ZoneChunkListIterator operator++(int) {
    ZoneChunkListIterator clone(*this);
    Move<backwards>();
    return clone;
  }

  ZoneChunkListIterator& operator--() {
    Move<!backwards>();
    return *this;
  }

  ZoneChunkListIterator operator--(int) {
    ZoneChunkListIterator clone(*this);
    Move<!backwards>();
    return clone;
  }

  void Advance(uint32_t amount) {
    static_assert(!backwards, "Advance only works on forward iterators");

#ifdef DEBUG
    ZoneChunkListIterator clone(*this);
    for (uint32_t i = 0; i < amount; ++i) {
      ++clone;
    }
#endif

    CHECK(!base::bits::UnsignedAddOverflow32(position_, amount, &position_));
    while (position_ > 0 && position_ >= current_->position_) {
      auto overshoot = position_ - current_->position_;
      current_ = current_->next_;
      position_ = overshoot;

      DCHECK(position_ == 0 || current_);
    }

#ifdef DEBUG
    DCHECK_EQ(clone, *this);
#endif
  }

 private:
  friend class ZoneChunkList<T>;

  static ZoneChunkListIterator Begin(ChunkList* list) {
    // Forward iterator:
    if (!backwards) return ZoneChunkListIterator(list->front_, 0);

    // Backward iterator:
    if (list->empty()) return End(list);

    DCHECK(!list->last_nonempty_->empty());
    return ZoneChunkListIterator(list->last_nonempty_,
                                 list->last_nonempty_->position_ - 1);
  }

  static ZoneChunkListIterator End(ChunkList* list) {
    // Backward iterator:
    if (backwards) return ZoneChunkListIterator(nullptr, 0);

    // Forward iterator:
    if (list->empty()) return Begin(list);

    // NOTE: Decrementing `end()` is not supported if `last_nonempty_->next_`
    // is nullptr (in that case `Move` will crash on dereference).
    return ZoneChunkListIterator(list->last_nonempty_->next_, 0);
  }

  ZoneChunkListIterator(Chunk* current, uint32_t position)
      : current_(current), position_(position) {
    DCHECK(current == nullptr || position < current->capacity_);
  }

  template <bool move_backward>
  void Move() {
    if (move_backward) {
      // Move backwards.
      if (position_ == 0) {
        current_ = current_->previous_;
        position_ = current_ ? current_->position_ - 1 : 0;
      } else {
        --position_;
      }
    } else {
      // Move forwards.
      ++position_;
      if (position_ >= current_->position_) {
        current_ = current_->next_;
        position_ = 0;
      }
    }
  }

  Chunk* current_;
  uint32_t position_;
};

template <typename T>
T& ZoneChunkList<T>::front() {
  DCHECK(!empty());
  return *begin();
}

template <typename T>
const T& ZoneChunkList<T>::front() const {
  DCHECK(!empty());
  return *begin();
}

template <typename T>
T& ZoneChunkList<T>::back() {
  DCHECK(!empty());
  // Avoid the branch in `ZoneChunkListIterator::Begin()`.
  V8_ASSUME(size_ != 0);
  return *rbegin();
}

template <typename T>
const T& ZoneChunkList<T>::back() const {
  DCHECK(!empty());
  // Avoid the branch in `ZoneChunkListIterator::Begin()`.
  V8_ASSUME(size_ != 0);
  return *rbegin();
}

template <typename T>
void ZoneChunkList<T>::push_back(const T& item) {
  if (last_nonempty_ == nullptr) {
    // Initially empty chunk list.
    front_ = NewChunk(kInitialChunkCapacity);
    last_nonempty_ = front_;
  } else if (last_nonempty_->full()) {
    // If there is an empty chunk following, reuse that, otherwise allocate.
    if (last_nonempty_->next_ == nullptr) {
      Chunk* chunk = NewChunk(NextChunkCapacity(last_nonempty_->capacity_));
      last_nonempty_->next_ = chunk;
      chunk->previous_ = last_nonempty_;
    }
    last_nonempty_ = last_nonempty_->next_;
    DCHECK(!last_nonempty_->full());
  }

  last_nonempty_->items()[last_nonempty_->position_] = item;
  ++last_nonempty_->position_;
  ++size_;
  DCHECK_LE(last_nonempty_->position_, last_nonempty_->capacity_);
}

template <typename T>
void ZoneChunkList<T>::push_front(const T& item) {
  if (front_ == nullptr) {
    // Initially empty chunk list.
    front_ = NewChunk(kInitialChunkCapacity);
    last_nonempty_ = front_;
  } else if (front_->full()) {
    // First chunk at capacity, so prepend a new chunk.
    DCHECK_NULL(front_->previous_);
    Chunk* chunk = NewChunk(NextChunkCapacity(front_->capacity_));
    front_->previous_ = chunk;
    chunk->next_ = front_;
    front_ = chunk;
  }
  DCHECK(!front_->full());

  T* end = front_->items() + front_->position_;
  std::move_backward(front_->items(), end, end + 1);
  front_->items()[0] = item;
  ++front_->position_;
  ++size_;
  DCHECK_LE(front_->position_, front_->capacity_);
}

template <typename T>
typename ZoneChunkList<T>::SeekResult ZoneChunkList<T>::SeekIndex(
    size_t index) const {
  DCHECK_LT(index, size());
  Chunk* current = front_;
  while (index >= current->capacity_) {
    index -= current->capacity_;
    current = current->next_;
  }
  DCHECK_LT(index, current->capacity_);
  return {current, static_cast<uint32_t>(index)};
}

template <typename T>
void ZoneChunkList<T>::Rewind(const size_t limit) {
  if (limit >= size()) return;

  SeekResult seek_result = SeekIndex(limit);
  DCHECK_NOT_NULL(seek_result.chunk_);

  // Do a partial rewind of the chunk containing the index.
  seek_result.chunk_->position_ = seek_result.chunk_index_;

  // Set last_nonempty_ so iterators will work correctly.
  last_nonempty_ = seek_result.chunk_;

  // Do full rewind of all subsequent chunks.
  for (Chunk* current = seek_result.chunk_->next_; current != nullptr;
       current = current->next_) {
    current->position_ = 0;
  }

  size_ = limit;

#ifdef DEBUG
  Verify();
#endif
}

template <typename T>
typename ZoneChunkList<T>::iterator ZoneChunkList<T>::Find(const size_t index) {
  SeekResult seek_result = SeekIndex(index);
  return typename ZoneChunkList<T>::iterator(seek_result.chunk_,
                                             seek_result.chunk_index_);
}

template <typename T>
typename ZoneChunkList<T>::const_iterator ZoneChunkList<T>::Find(
    const size_t index) const {
  SeekResult seek_result = SeekIndex(index);
  return typename ZoneChunkList<T>::const_iterator(seek_result.chunk_,
                                                   seek_result.chunk_index_);
}

template <typename T>
ZoneChunkList<T> ZoneChunkList<T>::SplitAt(iterator split_begin) {
  ZoneChunkList<T> result(zone_);

  // `result` is an empty freshly-constructed list.
  if (split_begin == end()) return result;

  // `this` is empty after the split and `result` contains everything.
  if (split_begin == begin()) {
    this->swap(result);
    return result;
  }

  // There is at least one element in both `this` and `result`.

  // Split the chunk.
  Chunk* split_chunk = split_begin.current_;
  DCHECK_LE(split_begin.position_, split_chunk->position_);
  T* chunk_split_begin = split_chunk->items() + split_begin.position_;
  T* chunk_split_end = split_chunk->items() + split_chunk->position_;
  uint32_t new_chunk_size =
      static_cast<uint32_t>(chunk_split_end - chunk_split_begin);
  uint32_t new_chunk_capacity = std::max(
      kInitialChunkCapacity, base::bits::RoundUpToPowerOfTwo32(new_chunk_size));
  CHECK_LE(new_chunk_size, new_chunk_capacity);
  Chunk* new_chunk = NewChunk(new_chunk_capacity);
  std::copy(chunk_split_begin, chunk_split_end, new_chunk->items());
  new_chunk->position_ = new_chunk_size;
  split_chunk->position_ = split_begin.position_;

  // Split the linked list.
  result.front_ = new_chunk;
  result.last_nonempty_ =
      (last_nonempty_ == split_chunk) ? new_chunk : last_nonempty_;
  new_chunk->next_ = split_chunk->next_;
  if (new_chunk->next_) {
    new_chunk->next_->previous_ = new_chunk;
  }

  last_nonempty_ = split_chunk;
  split_chunk->next_ = nullptr;

  // Compute the new size.
  size_t new_size = 0;
  for (Chunk* chunk = front_; chunk != split_chunk; chunk = chunk->next_) {
    DCHECK(!chunk->empty());
    new_size += chunk->size();
  }
  new_size += split_chunk->size();
  DCHECK_LT(new_size, size());
  result.size_ = size() - new_size;
  size_ = new_size;

#ifdef DEBUG
  Verify();
  result.Verify();
#endif

  return result;
}

template <typename T>
void ZoneChunkList<T>::Append(ZoneChunkList<T>& other) {
  DCHECK_EQ(zone_, other.zone_);

  if (other.front_ == nullptr) return;

  last_nonempty_->next_ = other.front_;
  other.front_->previous_ = last_nonempty_;

  last_nonempty_ = other.last_nonempty_;

  size_ += other.size_;
#ifdef DEBUG
  Verify();
#endif

  // Leave `other` in empty, but valid state.
  other.front_ = nullptr;
  other.last_nonempty_ = nullptr;
  other.size_ = 0;
}

template <typename T>
void ZoneChunkList<T>::CopyTo(T* ptr) {
  for (Chunk* current = front_; current != nullptr; current = current->next_) {
    void* start = current->items();
    void* end = current->items() + current->position_;
    size_t bytes = static_cast<size_t>(reinterpret_cast<uintptr_t>(end) -
                                       reinterpret_cast<uintptr_t>(start));

    MemCopy(ptr, current->items(), bytes);
    ptr += current->position_;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_CHUNK_LIST_H_
