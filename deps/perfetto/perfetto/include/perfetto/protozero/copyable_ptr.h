/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_PROTOZERO_COPYABLE_PTR_H_
#define INCLUDE_PERFETTO_PROTOZERO_COPYABLE_PTR_H_

#include <memory>

namespace protozero {

// This class is essentially a std::vector<T> of fixed size = 1.
// It's a pointer wrapper with deep copying and deep equality comparison.
// At all effects this wrapper behaves like the underlying T, with the exception
// of the heap indirection.
// Conversely to a std::unique_ptr, the pointer will be always valid, never
// null. The problem it solves is the following: when generating C++ classes
// from proto files, we want to keep each header hermetic (i.e. not #include
// headers of dependent types). As such we can't directly instantiate T
// field members but we can instead rely on pointers, so only the .cc file needs
// to see the actual definition of T. If the generated classes were move-only we
// could just use a unique_ptr there. But they aren't, hence this wrapper.
// Converesely to unique_ptr, this wrapper:
// - Default constructs the T instance in its constructor.
// - Implements deep comparison in operator== instead of pointer comparison.
template <typename T>
class CopyablePtr {
 public:
  CopyablePtr() : ptr_(new T()) {}
  ~CopyablePtr() = default;

  // Copy operators.
  CopyablePtr(const CopyablePtr& other) : ptr_(new T(*other.ptr_)) {}
  CopyablePtr& operator=(const CopyablePtr& other) {
    *ptr_ = *other.ptr_;
    return *this;
  }

  // Move operators.
  CopyablePtr(CopyablePtr&& other) noexcept : ptr_(std::move(other.ptr_)) {
    other.ptr_.reset(new T());
  }

  CopyablePtr& operator=(CopyablePtr&& other) {
    ptr_ = std::move(other.ptr_);
    other.ptr_.reset(new T());
    return *this;
  }

  T* get() { return ptr_.get(); }
  const T* get() const { return ptr_.get(); }

  T* operator->() { return ptr_.get(); }
  const T* operator->() const { return ptr_.get(); }

  T& operator*() { return *ptr_; }
  const T& operator*() const { return *ptr_; }

  friend bool operator==(const CopyablePtr& lhs, const CopyablePtr& rhs) {
    return *lhs == *rhs;
  }

  friend bool operator!=(const CopyablePtr& lhs, const CopyablePtr& rhs) {
    // In theory the underlying type might have a special operator!=
    // implementation which is not just !(x == y). Respect that.
    return *lhs != *rhs;
  }

 private:
  std::unique_ptr<T> ptr_;
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_COPYABLE_PTR_H_
