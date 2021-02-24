// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILS_DETACHABLE_VECTOR_H_
#define V8_UTILS_DETACHABLE_VECTOR_H_

#include <stddef.h>

#include <algorithm>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/wrappers.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE DetachableVectorBase {
 public:
  // Clear our reference to the backing store. Does not delete it!
  void detach() {
    data_ = nullptr;
    capacity_ = 0;
    size_ = 0;
  }

  void pop_back() { --size_; }
  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  static const size_t kMinimumCapacity;
  static const size_t kDataOffset;
  static const size_t kCapacityOffset;
  static const size_t kSizeOffset;

 protected:
  void* data_ = nullptr;
  size_t capacity_ = 0;
  size_t size_ = 0;
};

// This class wraps an array and provides a few of the common member
// functions for accessing the data. Two extra methods are also provided: free()
// and detach(), which allow for manual control of the backing store. This is
// currently required for use in the HandleScopeImplementer. Any other class
// should just use a std::vector.
template <typename T>
class DetachableVector : public DetachableVectorBase {
 public:
  DetachableVector() = default;
  ~DetachableVector() { delete[] data(); }

  void push_back(const T& value) {
    if (size_ == capacity_) {
      size_t new_capacity = std::max(kMinimumCapacity, 2 * capacity_);
      Resize(new_capacity);
    }

    data()[size_] = value;
    ++size_;
  }

  // Free the backing store and clear our reference to it.
  void free() {
    delete[] data();
    data_ = nullptr;
    capacity_ = 0;
    size_ = 0;
  }

  T& at(size_t i) const {
    DCHECK_LT(i, size_);
    return data()[i];
  }
  T& back() const { return at(size_ - 1); }
  T& front() const { return at(0); }

  void shrink_to_fit() {
    size_t new_capacity = std::max(size_, kMinimumCapacity);
    if (new_capacity < capacity_ / 2) {
      Resize(new_capacity);
    }
  }

 private:
  T* data() const { return static_cast<T*>(data_); }

  void Resize(size_t new_capacity) {
    DCHECK_LE(size_, new_capacity);
    T* new_data_ = new T[new_capacity];

    std::copy(data(), data() + size_, new_data_);
    delete[] data();

    data_ = new_data_;
    capacity_ = new_capacity;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UTILS_DETACHABLE_VECTOR_H_
