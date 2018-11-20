// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DETACHABLE_VECTOR_H_
#define V8_DETACHABLE_VECTOR_H_

#include <vector>

namespace v8 {
namespace internal {

// This class wraps a std::vector and provides a few of the common member
// functions for accessing the data. It acts as a lazy wrapper of the vector,
// not initiliazing the backing store until push_back() is first called. Two
// extra methods are also provided: free() and detach(), which allow for manual
// control of the backing store. This is currently required for use in the
// HandleScopeImplementer. Any other class should just use a std::vector
// directly.
template <typename T>
class DetachableVector {
 public:
  DetachableVector() : vector_(nullptr) {}

  ~DetachableVector() { delete vector_; }

  void push_back(const T& value) {
    ensureAttached();
    vector_->push_back(value);
  }

  // Free the backing store and clear our reference to it.
  void free() {
    delete vector_;
    vector_ = nullptr;
  }

  // Clear our reference to the backing store. Does not delete it!
  void detach() { vector_ = nullptr; }

  T& at(typename std::vector<T>::size_type i) const { return vector_->at(i); }

  T& back() const { return vector_->back(); }

  T& front() const { return vector_->front(); }

  void pop_back() { vector_->pop_back(); }

  typename std::vector<T>::size_type size() const {
    if (vector_) return vector_->size();
    return 0;
  }

  bool empty() const {
    if (vector_) return vector_->empty();
    return true;
  }

 private:
  std::vector<T>* vector_;

  // Attach a vector backing store if not present.
  void ensureAttached() {
    if (vector_ == nullptr) {
      vector_ = new std::vector<T>();
    }
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DETACHABLE_VECTOR_H_
