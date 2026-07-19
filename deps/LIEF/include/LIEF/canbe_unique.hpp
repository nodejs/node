/* Copyright 2024 - 2025 R. Thomas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_CAN_BE_UNIQUE_H
#define LIEF_CAN_BE_UNIQUE_H
#include <memory>

namespace LIEF {
namespace details {
template<class T>
class canbe_unique {
  public:
  canbe_unique() = default;
  canbe_unique(const canbe_unique&) = delete;
  canbe_unique& operator=(const canbe_unique&) = delete;

  canbe_unique& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  canbe_unique(canbe_unique&& other) noexcept {
    std::swap(ptr_, other.ptr_);
    owned_ = other.owned_;
  }

  canbe_unique& operator=(canbe_unique&& other) noexcept {
    if (&other != this) {
      std::swap(ptr_, other.ptr_);
      owned_ = other.owned_;
    }
    return *this;
  }

  canbe_unique(T& ptr) :
    ptr_(&ptr),
    owned_(false)
  {}

  canbe_unique(const T& ptr) :
    ptr_(const_cast<T*>(&ptr)),
    owned_(false)
  {}

  canbe_unique(std::unique_ptr<T> unique_ptr) :
    ptr_(unique_ptr.release()),
    owned_(true)
  {}

  T* get() {
    return ptr_;
  }

  const T* get() const {
    return ptr_;
  }

  T* operator->() {
    return ptr_;
  }

  const T* operator->() const {
    return ptr_;
  }

  T& operator*() {
    return *ptr_;
  }

  const T& operator*() const {
    return *ptr_;
  }

  void reset() {
    if (!owned_) {
      return;
    }
    if (ptr_ != nullptr) {
      delete ptr_;
    }
    ptr_ = nullptr;
  }

  operator bool() const {
    return ptr_ != nullptr;
  }

  ~canbe_unique() {
    reset();
  }

  private:
  T* ptr_ = nullptr;
  bool owned_ = false;
};

template<class T>
inline bool operator==(const canbe_unique<T>& lhs, std::nullptr_t) {
  return !lhs;
}

template<class T>
inline bool operator==(std::nullptr_t, const canbe_unique<T>& lhs) {
  return !lhs;
}

}
}
#endif
