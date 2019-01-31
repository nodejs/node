// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_THREAD_ID_H_
#define V8_THREAD_ID_H_

#include "src/base/atomicops.h"

namespace v8 {
namespace internal {

// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  ThreadId() { base::Relaxed_Store(&id_, kInvalidId); }

  ThreadId& operator=(const ThreadId& other) V8_NOEXCEPT {
    base::Relaxed_Store(&id_, base::Relaxed_Load(&other.id_));
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
    return base::Relaxed_Load(&id_) == base::Relaxed_Load(&other.id_);
  }

  // Checks whether this ThreadId refers to any thread.
  V8_INLINE bool IsValid() const {
    return base::Relaxed_Load(&id_) != kInvalidId;
  }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::GetCurrentThreadId).
  int ToInteger() const { return static_cast<int>(base::Relaxed_Load(&id_)); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static const int kInvalidId = -1;

  explicit ThreadId(int id) { base::Relaxed_Store(&id_, id); }

  static int AllocateThreadId() {
    int new_id = base::Relaxed_AtomicIncrement(&highest_thread_id_, 1);
    return new_id;
  }

  static int GetCurrentThreadId();

  base::Atomic32 id_;

  static base::Atomic32 highest_thread_id_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_THREAD_ID_H_
