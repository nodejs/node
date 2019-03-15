// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_THREAD_ID_H_
#define V8_THREAD_ID_H_

#include <atomic>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  ThreadId() : ThreadId(kInvalidId) {}

  ThreadId(const ThreadId& other) V8_NOEXCEPT : ThreadId(other.ToInteger()) {}

  ThreadId& operator=(const ThreadId& other) V8_NOEXCEPT {
    id_.store(other.ToInteger(), std::memory_order_relaxed);
    return *this;
  }

  bool operator==(const ThreadId& other) const { return Equals(other); }

  // Returns ThreadId for current thread if it exists or invalid id.
  static ThreadId TryGetCurrent();

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Compares ThreadIds for equality.
  V8_INLINE bool Equals(const ThreadId& other) const {
    return ToInteger() == other.ToInteger();
  }

  // Checks whether this ThreadId refers to any thread.
  V8_INLINE bool IsValid() const { return ToInteger() != kInvalidId; }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::GetCurrentThreadId).
  int ToInteger() const { return id_.load(std::memory_order_relaxed); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static constexpr int kInvalidId = -1;

  explicit ThreadId(int id) { id_.store(id, std::memory_order_relaxed); }

  static int GetCurrentThreadId();

  std::atomic<int> id_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_THREAD_ID_H_
