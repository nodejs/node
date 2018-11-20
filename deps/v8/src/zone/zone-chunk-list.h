// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/globals.h"
#include "src/utils.h"
#include "src/zone/zone.h"

#ifndef V8_ZONE_ZONE_CHUNK_LIST_H_
#define V8_ZONE_ZONE_CHUNK_LIST_H_

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
// * can be rewound without freeing the backing store.
// This list will maintain a doubly-linked list of chunks. When a chunk is
// filled up, a new one gets appended. New chunks appended at the end will
// grow in size up to a certain limit to avoid over-allocation and to keep
// the zone clean.
template <typename T>
class ZoneChunkList : public ZoneObject {
 public:
  using iterator = ZoneChunkListIterator<T, false, true>;
  using const_iterator = ZoneChunkListIterator<T, false, false>;
  using reverse_iterator = ZoneChunkListIterator<T, true, true>;
  using const_reverse_iterator = ZoneChunkListIterator<T, true, false>;

  enum class StartMode {
    // The list will not allocate a starting chunk. Use if you expect your
    // list to remain empty in many cases.
    kEmpty = 0,
    // The list will start with a small initial chunk. Subsequent chunks will
    // get bigger over time.
    kSmall = 8,
    // The list will start with one chunk at maximum size. Use this if you
    // expect your list to contain many items to avoid growing chunks.
    kBig = 256
  };

  explicit ZoneChunkList(Zone* zone, StartMode start_mode = StartMode::kEmpty)
      : zone_(zone) {
    if (start_mode != StartMode::kEmpty) {
      front_ = NewChunk(static_cast<uint32_t>(start_mode));
      back_ = front_;
    }
  }

  size_t size() const { return size_; }

  T& front() const;
  T& back() const;

  void push_back(const T& item);
  void pop_back();

  // Will push a separate chunk to the front of the chunk-list.
  // Very memory-inefficient. Do only use sparsely! If you have many items to
  // add in front, consider using 'push_front_many'.
  void push_front(const T& item);
  // TODO(heimbuef): Add 'push_front_many'.

  // Cuts the last list elements so at most 'limit' many remain. Does not
  // free the actual memory, since it is zone allocated.
  void Rewind(const size_t limit = 0);

  // Quickly scans the list to retrieve the element at the given index. Will
  // *not* check bounds.
  iterator Find(const size_t index);
  const_iterator Find(const size_t index) const;
  // TODO(heimbuef): Add 'rFind', seeking from the end and returning a
  // reverse iterator.

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

 private:
  template <typename S, bool backwards, bool modifiable>
  friend class ZoneChunkListIterator;

  static constexpr uint32_t kMaxChunkCapacity = 256u;

  STATIC_ASSERT(kMaxChunkCapacity == static_cast<uint32_t>(StartMode::kBig));

  struct Chunk {
    uint32_t capacity_ = 0;
    uint32_t position_ = 0;
    Chunk* next_ = nullptr;
    Chunk* previous_ = nullptr;
    T* items() { return reinterpret_cast<T*>(this + 1); }
    const T* items() const { return reinterpret_cast<const T*>(this + 1); }
  };

  Chunk* NewChunk(const uint32_t capacity) {
    Chunk* chunk =
        new (zone_->New(sizeof(Chunk) + capacity * sizeof(T))) Chunk();
    chunk->capacity_ = capacity;
    return chunk;
  }

  struct SeekResult {
    Chunk* chunk_;
    uint32_t chunk_index_;
  };

  // Returns the chunk and relative index of the element at the given global
  // index. Will skip entire chunks and is therefore faster than iterating.
  SeekResult SeekIndex(size_t index) const;

  Zone* zone_;

  size_t size_ = 0;
  Chunk* front_ = nullptr;
  Chunk* back_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ZoneChunkList);
};

template <typename T, bool backwards, bool modifiable>
class ZoneChunkListIterator {
 private:
  template <typename S>
  using maybe_const =
      typename std::conditional<modifiable, S,
                                typename std::add_const<S>::type>::type;
  using Chunk = maybe_const<typename ZoneChunkList<T>::Chunk>;
  using ChunkList = maybe_const<ZoneChunkList<T>>;

 public:
  maybe_const<T>& operator*() { return current_->items()[position_]; }
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

 private:
  friend class ZoneChunkList<T>;

  static ZoneChunkListIterator Begin(ChunkList* list) {
    // Forward iterator:
    if (!backwards) return ZoneChunkListIterator(list->front_, 0);

    // Backward iterator:
    if (list->back_ == nullptr) return End(list);
    if (list->back_->position_ == 0) {
      if (list->back_->previous_ != nullptr) {
        return ZoneChunkListIterator(list->back_->previous_,
                                     list->back_->previous_->capacity_ - 1);
      } else {
        return End(list);
      }
    }
    return ZoneChunkListIterator(list->back_, list->back_->position_ - 1);
  }

  static ZoneChunkListIterator End(ChunkList* list) {
    // Backward iterator:
    if (backwards) return ZoneChunkListIterator(nullptr, 0);

    // Forward iterator:
    if (list->back_ == nullptr) return Begin(list);

    DCHECK_LE(list->back_->position_, list->back_->capacity_);
    if (list->back_->position_ == list->back_->capacity_) {
      return ZoneChunkListIterator(list->back_->next_, 0);
    }

    return ZoneChunkListIterator(list->back_, list->back_->position_);
  }

  ZoneChunkListIterator(Chunk* current, size_t position)
      : current_(current), position_(position) {}

  template <bool move_backward>
  void Move() {
    if (move_backward) {
      // Move backwards.
      if (position_ == 0) {
        current_ = current_->previous_;
        position_ = current_ ? current_->capacity_ - 1 : 0;
      } else {
        --position_;
      }
    } else {
      // Move forwards.
      ++position_;
      if (position_ >= current_->capacity_) {
        current_ = current_->next_;
        position_ = 0;
      }
    }
  }

  Chunk* current_;
  size_t position_;
};

template <typename T>
T& ZoneChunkList<T>::front() const {
  DCHECK_LT(size_t(0), size());
  return front_->items()[0];
}

template <typename T>
T& ZoneChunkList<T>::back() const {
  DCHECK_LT(size_t(0), size());

  if (back_->position_ == 0) {
    return back_->previous_->items()[back_->previous_->position_ - 1];
  } else {
    return back_->items()[back_->position_ - 1];
  }
}

template <typename T>
void ZoneChunkList<T>::push_back(const T& item) {
  if (back_ == nullptr) {
    front_ = NewChunk(static_cast<uint32_t>(StartMode::kSmall));
    back_ = front_;
  }

  DCHECK_LE(back_->position_, back_->capacity_);
  if (back_->position_ == back_->capacity_) {
    if (back_->next_ == nullptr) {
      Chunk* chunk = NewChunk(Min(back_->capacity_ << 1, kMaxChunkCapacity));
      back_->next_ = chunk;
      chunk->previous_ = back_;
    }
    back_ = back_->next_;
  }
  back_->items()[back_->position_] = item;
  ++back_->position_;
  ++size_;
}

template <typename T>
void ZoneChunkList<T>::pop_back() {
  DCHECK_LT(size_t(0), size());
  if (back_->position_ == 0) {
    back_ = back_->previous_;
  }
  --back_->position_;
  --size_;
}

template <typename T>
void ZoneChunkList<T>::push_front(const T& item) {
  Chunk* chunk = NewChunk(1);  // Yes, this gets really inefficient.
  chunk->next_ = front_;
  if (front_) {
    front_->previous_ = chunk;
  } else {
    back_ = chunk;
  }
  front_ = chunk;

  chunk->items()[0] = item;
  chunk->position_ = 1;
  ++size_;
}

template <typename T>
typename ZoneChunkList<T>::SeekResult ZoneChunkList<T>::SeekIndex(
    size_t index) const {
  DCHECK_LT(index, size());
  Chunk* current = front_;
  while (index > current->capacity_) {
    index -= current->capacity_;
    current = current->next_;
  }
  return {current, static_cast<uint32_t>(index)};
}

template <typename T>
void ZoneChunkList<T>::Rewind(const size_t limit) {
  if (limit >= size()) return;

  SeekResult seek_result = SeekIndex(limit);
  DCHECK_NOT_NULL(seek_result.chunk_);

  // Do a partial rewind of the chunk containing the index.
  seek_result.chunk_->position_ = seek_result.chunk_index_;

  // Set back_ so iterators will work correctly.
  back_ = seek_result.chunk_;

  // Do full rewind of all subsequent chunks.
  for (Chunk* current = seek_result.chunk_->next_; current != nullptr;
       current = current->next_) {
    current->position_ = 0;
  }

  size_ = limit;
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
