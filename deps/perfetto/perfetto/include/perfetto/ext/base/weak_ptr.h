/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_EXT_BASE_WEAK_PTR_H_
#define INCLUDE_PERFETTO_EXT_BASE_WEAK_PTR_H_

#include "perfetto/ext/base/thread_checker.h"

#include <memory>

namespace perfetto {
namespace base {

// A simple WeakPtr for single-threaded cases.
// Generally keep the WeakPtrFactory as last fields in classes: it makes the
// WeakPtr(s) invalidate as first thing in the class dtor.
// Usage:
// class MyClass {
//  MyClass() : weak_factory_(this) {}
//  WeakPtr<MyClass> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }
//
// private:
//  WeakPtrFactory<MyClass> weak_factory_;
// }
//
// int main() {
//  std::unique_ptr<MyClass> foo(new MyClass);
//  auto wptr = foo.GetWeakPtr();
//  ASSERT_TRUE(wptr);
//  ASSERT_EQ(foo.get(), wptr->get());
//  foo.reset();
//  ASSERT_FALSE(wptr);
//  ASSERT_EQ(nullptr, wptr->get());
// }

template <typename T>
class WeakPtrFactory;  // Forward declaration, defined below.

template <typename T>
class WeakPtr {
 public:
  WeakPtr() {}
  WeakPtr(const WeakPtr&) = default;
  WeakPtr& operator=(const WeakPtr&) = default;
  WeakPtr(WeakPtr&&) = default;
  WeakPtr& operator=(WeakPtr&&) = default;

  T* get() const {
    PERFETTO_DCHECK_THREAD(thread_checker);
    return handle_ ? *handle_.get() : nullptr;
  }
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }

  explicit operator bool() const { return !!get(); }

 private:
  friend class WeakPtrFactory<T>;
  explicit WeakPtr(const std::shared_ptr<T*>& handle) : handle_(handle) {}

  std::shared_ptr<T*> handle_;
  PERFETTO_THREAD_CHECKER(thread_checker)
};

template <typename T>
class WeakPtrFactory {
 public:
  explicit WeakPtrFactory(T* owner)
      : weak_ptr_(std::shared_ptr<T*>(new T* {owner})) {
    PERFETTO_DCHECK_THREAD(thread_checker);
  }

  ~WeakPtrFactory() {
    PERFETTO_DCHECK_THREAD(thread_checker);
    *(weak_ptr_.handle_.get()) = nullptr;
  }

  // Can be safely called on any thread, since it simply copies |weak_ptr_|.
  // Note that any accesses to the returned pointer need to be made on the
  // thread that created/reset the factory.
  WeakPtr<T> GetWeakPtr() const { return weak_ptr_; }

  // Reset the factory to a new owner & thread. May only be called before any
  // weak pointers were passed out. Future weak pointers will be valid on the
  // calling thread.
  void Reset(T* owner) {
    // Reset thread checker to current thread.
    PERFETTO_DETACH_FROM_THREAD(thread_checker);
    PERFETTO_DCHECK_THREAD(thread_checker);

    // We should not have passed out any weak pointers yet at this point.
    PERFETTO_DCHECK(weak_ptr_.handle_.use_count() == 1);

    weak_ptr_ = WeakPtr<T>(std::shared_ptr<T*>(new T* {owner}));
  }

 private:
  WeakPtrFactory(const WeakPtrFactory&) = delete;
  WeakPtrFactory& operator=(const WeakPtrFactory&) = delete;

  WeakPtr<T> weak_ptr_;
  PERFETTO_THREAD_CHECKER(thread_checker)
};

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_EXT_BASE_WEAK_PTR_H_
