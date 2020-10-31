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

#ifndef INCLUDE_PERFETTO_TRACING_LOCKED_HANDLE_H_
#define INCLUDE_PERFETTO_TRACING_LOCKED_HANDLE_H_

#include <mutex>

namespace perfetto {

// This is used for GetDataSourceLocked(), in the (rare) case where the
// tracing code wants to access the state of its data source from the Trace()
// method.
template <typename T>
class LockedHandle {
 public:
  LockedHandle(std::recursive_mutex* mtx, T* obj) : lock_(*mtx), obj_(obj) {}
  LockedHandle() = default;  // For the invalid case.
  LockedHandle(LockedHandle&&) = default;
  LockedHandle& operator=(LockedHandle&&) = default;

  bool valid() const { return obj_; }
  explicit operator bool() const { return valid(); }

  T* operator->() {
    assert(valid());
    return obj_;
  }

  T& operator*() { return *(this->operator->()); }

 private:
  std::unique_lock<std::recursive_mutex> lock_;
  T* obj_ = nullptr;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_LOCKED_HANDLE_H_
