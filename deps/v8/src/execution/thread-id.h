// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_THREAD_ID_H_
#define V8_EXECUTION_THREAD_ID_H_

#include "src/base/macros.h"

namespace v8 {
namespace internal {

// Platform-independent, reliable thread identifier.
class ThreadId {
 public:
  // Creates an invalid ThreadId.
  constexpr ThreadId() noexcept : ThreadId(kInvalidId) {}

  bool operator==(const ThreadId& other) const { return id_ == other.id_; }
  bool operator!=(const ThreadId& other) const { return id_ != other.id_; }

  // Checks whether this ThreadId refers to any thread.
  bool IsValid() const { return id_ != kInvalidId; }

  // Converts ThreadId to an integer representation.
  constexpr int ToInteger() const { return id_; }

  // Returns ThreadId for current thread if it exists or invalid id.
  static ThreadId TryGetCurrent();

  // Returns ThreadId for current thread.
  static ThreadId Current() { return ThreadId(GetCurrentThreadId()); }

  // Returns invalid ThreadId (guaranteed not to be equal to any thread).
  static constexpr ThreadId Invalid() { return ThreadId(kInvalidId); }

  // Converts ThreadId to an integer representation
  // (required for public API: V8::V8::TerminateExecution).
  static constexpr ThreadId FromInteger(int id) { return ThreadId(id); }

 private:
  static constexpr int kInvalidId = -1;

  explicit constexpr ThreadId(int id) noexcept : id_(id) {}

  V8_EXPORT_PRIVATE static int GetCurrentThreadId();

  int id_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_THREAD_ID_H_
