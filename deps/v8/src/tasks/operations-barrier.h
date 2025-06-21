// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TASKS_OPERATIONS_BARRIER_H_
#define V8_TASKS_OPERATIONS_BARRIER_H_

#include <cstdint>

#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

// A thread-safe barrier to manage lifetime of muti-threaded operations.
//
// The barrier is used to determine if operations are allowed, and to keep track
// of how many are currently active. Users will call TryLock() before starting
// such operations. If the call succeeds the user can run the operation and the
// barrier will keep track of it until the user signals that the operation is
// completed. No operations are allowed after CancelAndWait() is called.
//
// There is no explicit way of telling the barrier when an operation is
// completed, instead for convenience TryLock() will return a RAII
// like object that will do so on destruction.
//
// For example:
//
// OperationsBarrier barrier_;
//
// void TearDown() {
//   barrier_.CancelAndWait();
// }
//
// void MaybeRunOperation() {
//   if (token = barrier_.TryLock()) Process();
// }
//
class V8_EXPORT_PRIVATE OperationsBarrier {
 public:
  // The owner of a Token which evaluates to true can safely perform an
  // operation while being certain it happens-before CancelAndWait(). Releasing
  // this Token relinquishes this right.
  //
  // This class is thread-safe.
  class Token {
   public:
    Token() = default;
    ~Token() {
      if (outer_) outer_->Release();
    }
    Token(const Token&) = delete;
    Token(Token&& other) V8_NOEXCEPT : outer_(other.outer_) {
      other.outer_ = nullptr;
    }

    Token& operator=(const Token&) = delete;
    Token& operator=(Token&& other) V8_NOEXCEPT {
      DCHECK_NE(this, &other);
      if (outer_) outer_->Release();
      outer_ = other.outer_;
      other.outer_ = nullptr;
      return *this;
    }

    operator bool() const { return !!outer_; }

   private:
    friend class OperationsBarrier;
    explicit Token(OperationsBarrier* outer) : outer_(outer) {
      DCHECK_NOT_NULL(outer_);
    }
    OperationsBarrier* outer_ = nullptr;
  };

  OperationsBarrier() = default;

  // Users must call CancelAndWait() before destroying an instance of this
  // class.
  ~OperationsBarrier() { DCHECK(cancelled_); }

  OperationsBarrier(const OperationsBarrier&) = delete;
  OperationsBarrier& operator=(const OperationsBarrier&) = delete;

  // Returns a RAII like object that implicitly converts to true if operations
  // are allowed i.e. if this call happens-before CancelAndWait(), otherwise the
  // object will convert to false. On successful return, this OperationsBarrier
  // will keep track of the operation until the returned object goes out of
  // scope.
  Token TryLock();

  // Prevents further calls to TryLock() from succeeding and waits for
  // all the ongoing operations to complete.
  //
  // Attention: Can only be called once.
  void CancelAndWait();

  bool cancelled() const { return cancelled_; }

 private:
  void Release();

  // Mutex and condition variable enabling concurrent register and removing, as
  // well as waiting for background tasks on {CancelAndWait}.
  base::Mutex mutex_;
  base::ConditionVariable release_condition_;
  bool cancelled_ = false;
  size_t operations_count_{0};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TASKS_OPERATIONS_BARRIER_H_
