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

#ifndef SRC_PROFILING_MEMORY_UNHOOKED_ALLOCATOR_H_
#define SRC_PROFILING_MEMORY_UNHOOKED_ALLOCATOR_H_

#include <stdlib.h>

#include <atomic>
#include <exception>

namespace perfetto {
namespace profiling {

// An allocator that uses the given |malloc| & |free| function pointers to
// allocate/deallocate memory. Used in the heapprofd_client library, which hooks
// the malloc functions in a target process, and therefore needs to use this
// allocator to not have its own heap usage enter the hooks.
//
// See https://en.cppreference.com/w/cpp/named_req/Allocator.
template <typename T>
class UnhookedAllocator {
 public:
  template <typename U>
  friend class UnhookedAllocator;

  // to satisfy the Allocator interface
  using value_type = T;

  // for implementation convenience
  using unhooked_malloc_t = void* (*)(size_t);
  using unhooked_free_t = void (*)(void*);

  UnhookedAllocator(unhooked_malloc_t unhooked_malloc,
                    unhooked_free_t unhooked_free)
      : unhooked_malloc_(unhooked_malloc), unhooked_free_(unhooked_free) {}

  template <typename U>
  constexpr UnhookedAllocator(const UnhookedAllocator<U>& other) noexcept
      : unhooked_malloc_(other.unhooked_malloc_),
        unhooked_free_(other.unhooked_free_) {}

  T* allocate(size_t n) {
    if (n > size_t(-1) / sizeof(T))
      std::terminate();
    if (T* p = static_cast<T*>(unhooked_malloc_(n * sizeof(T))))
      return p;
    std::terminate();
  }

  void deallocate(T* p, size_t) noexcept { unhooked_free_(p); }

 private:
  unhooked_malloc_t unhooked_malloc_ = nullptr;
  unhooked_free_t unhooked_free_ = nullptr;
};

template <typename T, typename U>
bool operator==(const UnhookedAllocator<T>& first,
                const UnhookedAllocator<U>& second) {
  return first.unhooked_malloc_ == second.unhooked_malloc_ &&
         first.unhooked_free_ == second.unhooked_free_;
}

template <typename T, typename U>
bool operator!=(const UnhookedAllocator<T>& first,
                const UnhookedAllocator<U>& second) {
  return !(first == second);
}

}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_MEMORY_UNHOOKED_ALLOCATOR_H_
