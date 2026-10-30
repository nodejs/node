// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_THREAD_ID_H_
#define V8_BASE_THREAD_ID_H_

#include "src/base/base-export.h"
#include "src/base/macros.h"

namespace v8::base {

// Platform-independent, unique thread identifier.
class ThreadId final {
 public:
  // Returns ThreadId for current thread if it exists or invalid id.
  V8_BASE_EXPORT static ThreadId TryGetCurrent();

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static constexpr ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static constexpr ThreadId FromInteger(int id) { return ThreadId(id); }

  constexpr bool operator==(const ThreadId& other) const {
    return id_ == other.id_;
  }
  constexpr bool operator!=(const ThreadId& other) const {
    return id_ != other.id_;
  }

  // Checks whether this ThreadId refers to any thread.
  constexpr bool IsValid() const { return id_ != kInvalidId; }

  // Converts ThreadId to an integer representation.
  constexpr int ToInteger() const { return id_; }

 private:
  static constexpr int kInvalidId = -1;

  explicit constexpr ThreadId(int id) noexcept : id_(id) {}

  V8_BASE_EXPORT static int GetCurrentThreadId();

  int id_;
};

}  // namespace v8::base

#endif  // V8_BASE_THREAD_ID_H_
