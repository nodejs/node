// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COLLECTOR_H_
#define V8_COLLECTOR_H_

#include <vector>

#include "src/base/iterator.h"
#include "src/base/unique-array.h"
#include "src/base/vector.h"
#include "src/common/checks.h"

namespace v8 {
namespace internal {

/*
 * A class that collects values into a backing store.
 * Specialized versions of the class can allow access to the backing store
 * in different ways.
 * There is no guarantee that the backing store is contiguous (and, as a
 * consequence, no guarantees that consecutively added elements are adjacent
 * in memory). The collector may move elements unless it has guaranteed not
 * to.
 */
template <typename T, int growth_factor = 2, int max_growth = 1 * MB>
class Collector {
 public:
  explicit Collector(int initial_capacity = kMinCapacity)
      : current_chunk_(base::UniqueArray<T>::NewForOverwrite(initial_capacity)),
        index_(0),
        size_(0) {}

  virtual ~Collector() = default;

  // Add a single element.
  inline void Add(T value) {
    if (index_ >= current_chunk_.as_vector().length()) {
      Grow(1);
    }
    current_chunk_[index_] = value;
    index_++;
    size_++;
  }

  // Add a block of contiguous elements and return a Vector backed by the
  // memory area.
  // A basic Collector will keep this vector valid as long as the Collector
  // is alive.
  inline base::Vector<T> AddBlock(int size, T initial_value) {
    DCHECK_GT(size, 0);
    if (size > current_chunk_.as_vector().length() - index_) {
      Grow(size);
    }
    T* position = current_chunk_.begin() + index_;
    index_ += size;
    size_ += size;
    for (int i = 0; i < size; i++) {
      position[i] = initial_value;
    }
    return base::Vector<T>(position, size);
  }

  // Add a contiguous block of elements and return a vector backed
  // by the added block.
  // A basic Collector will keep this vector valid as long as the Collector
  // is alive.
  inline base::Vector<T> AddBlock(base::Vector<const T> source) {
    if (source.length() > current_chunk_.as_vector().length() - index_) {
      Grow(source.length());
    }
    T* position = current_chunk_.begin() + index_;
    index_ += source.length();
    size_ += source.length();
    for (int i = 0; i < source.length(); i++) {
      position[i] = source[i];
    }
    return base::Vector<T>(position, source.length());
  }

  // Write the contents of the collector into the provided vector.
  void WriteTo(base::Vector<T> destination) {
    DCHECK(size_ <= destination.length());
    int position = 0;
    for (const Chunk& chunk : chunks_) {
      for (int j = 0; j < chunk.length; j++) {
        destination[position] = chunk.data[j];
        position++;
      }
    }
    for (int i = 0; i < index_; i++) {
      destination[position] = current_chunk_[i];
      position++;
    }
  }

  // Allocate a single contiguous vector, copy all the collected
  // elements to the vector, and return it.
  base::UniqueArray<T> ToVector() {
    base::UniqueArray<T> new_store =
        base::UniqueArray<T>::NewForOverwrite(size_);
    WriteTo(new_store.as_vector());
    return new_store;
  }

  // Resets the collector to be empty.
  virtual void Reset() {
    chunks_.clear();
    index_ = 0;
    size_ = 0;
  }

  // Total number of elements added to collector so far.
  inline int size() { return size_; }

 protected:
  struct Chunk {
    base::UniqueArray<T> data;
    int length;
  };

  static const int kMinCapacity = 16;
  std::vector<Chunk> chunks_;
  base::UniqueArray<T>
      current_chunk_;        // Block of memory currently being written into.
  int index_;                // Current index in current chunk.
  int size_;                 // Total number of elements in collector.

  // Creates a new current chunk, and stores the old chunk in the chunks_ list.
  void Grow(int min_capacity) {
    DCHECK_GT(growth_factor, 1);
    int new_capacity;
    int current_length = current_chunk_.as_vector().length();
    if (current_length < kMinCapacity) {
      // The collector started out as empty.
      new_capacity = min_capacity * growth_factor;
      if (new_capacity < kMinCapacity) new_capacity = kMinCapacity;
    } else {
      int growth = current_length * (growth_factor - 1);
      if (growth > max_growth) {
        growth = max_growth;
      }
      new_capacity = current_length + growth;
      if (new_capacity < min_capacity) {
        new_capacity = min_capacity + growth;
      }
    }
    NewChunk(new_capacity);
    DCHECK(index_ + min_capacity <= current_chunk_.as_vector().length());
  }

  // Before replacing the current chunk, give a subclass the option to move
  // some of the current data into the new chunk. The function may update
  // the current index_ value to represent data no longer in the current chunk.
  // Returns the initial index of the new chunk (after copied data).
  virtual void NewChunk(int new_capacity) {
    base::UniqueArray<T> new_chunk =
        base::UniqueArray<T>::NewForOverwrite(new_capacity);
    if (index_ > 0) {
      chunks_.push_back({std::move(current_chunk_), index_});
    }
    current_chunk_ = std::move(new_chunk);
    index_ = 0;
  }
};

/*
 * A collector that allows sequences of values to be guaranteed to
 * stay consecutive.
 * If the backing store grows while a sequence is active, the current
 * sequence might be moved, but after the sequence is ended, it will
 * not move again.
 * NOTICE: Blocks allocated using Collector::AddBlock(int) can move
 * as well, if inside an active sequence where another element is added.
 */
template <typename T, int growth_factor = 2, int max_growth = 1 * MB>
class SequenceCollector : public Collector<T, growth_factor, max_growth> {
 public:
  explicit SequenceCollector(int initial_capacity)
      : Collector<T, growth_factor, max_growth>(initial_capacity),
        sequence_start_(kNoSequence) {}

  ~SequenceCollector() override = default;

  void StartSequence() {
    DCHECK_EQ(sequence_start_, kNoSequence);
    sequence_start_ = this->index_;
  }

  base::Vector<T> EndSequence() {
    DCHECK_NE(sequence_start_, kNoSequence);
    int sequence_start = sequence_start_;
    sequence_start_ = kNoSequence;
    if (sequence_start == this->index_) return base::Vector<T>();
    return this->current_chunk_.as_vector().SubVector(sequence_start,
                                                      this->index_);
  }

  // Drops the currently added sequence, and all collected elements in it.
  void DropSequence() {
    DCHECK_NE(sequence_start_, kNoSequence);
    int sequence_length = this->index_ - sequence_start_;
    this->index_ = sequence_start_;
    this->size_ -= sequence_length;
    sequence_start_ = kNoSequence;
  }

  void Reset() override {
    sequence_start_ = kNoSequence;
    this->Collector<T, growth_factor, max_growth>::Reset();
  }

 private:
  static const int kNoSequence = -1;
  int sequence_start_;

  // Move the currently active sequence to the new chunk.
  void NewChunk(int new_capacity) override {
    if (sequence_start_ == kNoSequence) {
      // Fall back on default behavior if no sequence has been started.
      this->Collector<T, growth_factor, max_growth>::NewChunk(new_capacity);
      return;
    }
    int sequence_length = this->index_ - sequence_start_;
    base::UniqueArray<T> new_chunk =
        base::UniqueArray<T>::NewForOverwrite(sequence_length + new_capacity);
    DCHECK(sequence_length < new_chunk.as_vector().length());
    for (int i = 0; i < sequence_length; i++) {
      new_chunk[i] = this->current_chunk_[sequence_start_ + i];
    }
    if (sequence_start_ > 0) {
      this->chunks_.push_back(
          {std::move(this->current_chunk_), sequence_start_});
    }
    this->current_chunk_ = std::move(new_chunk);
    this->index_ = sequence_length;
    sequence_start_ = 0;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COLLECTOR_H_
