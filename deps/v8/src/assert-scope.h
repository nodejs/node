// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ASSERT_SCOPE_H_
#define V8_ASSERT_SCOPE_H_

#include "allocation.h"
#include "platform.h"

namespace v8 {
namespace internal {

class Isolate;

enum PerThreadAssertType {
  HEAP_ALLOCATION_ASSERT,
  HANDLE_ALLOCATION_ASSERT,
  HANDLE_DEREFERENCE_ASSERT,
  DEFERRED_HANDLE_DEREFERENCE_ASSERT,
  LAST_PER_THREAD_ASSERT_TYPE
};


#ifdef DEBUG
class PerThreadAssertData {
 public:
  PerThreadAssertData() : nesting_level_(0) {
    for (int i = 0; i < LAST_PER_THREAD_ASSERT_TYPE; i++) {
      assert_states_[i] = true;
    }
  }

  void set(PerThreadAssertType type, bool allow) {
    assert_states_[type] = allow;
  }

  bool get(PerThreadAssertType type) const {
    return assert_states_[type];
  }

  void increment_level() { ++nesting_level_; }
  bool decrement_level() { return --nesting_level_ == 0; }

 private:
  bool assert_states_[LAST_PER_THREAD_ASSERT_TYPE];
  int nesting_level_;

  DISALLOW_COPY_AND_ASSIGN(PerThreadAssertData);
};
#endif  // DEBUG


class PerThreadAssertScopeBase {
#ifdef DEBUG

 protected:
  PerThreadAssertScopeBase() {
    data_ = AssertData();
    data_->increment_level();
  }

  ~PerThreadAssertScopeBase() {
    if (!data_->decrement_level()) return;
    for (int i = 0; i < LAST_PER_THREAD_ASSERT_TYPE; i++) {
      ASSERT(data_->get(static_cast<PerThreadAssertType>(i)));
    }
    delete data_;
    Thread::SetThreadLocal(thread_local_key, NULL);
  }

  static PerThreadAssertData* AssertData() {
    PerThreadAssertData* data = reinterpret_cast<PerThreadAssertData*>(
            Thread::GetThreadLocal(thread_local_key));
    if (data == NULL) {
      data = new PerThreadAssertData();
      Thread::SetThreadLocal(thread_local_key, data);
    }
    return data;
  }

  static Thread::LocalStorageKey thread_local_key;
  PerThreadAssertData* data_;
  friend class Isolate;
#endif  // DEBUG
};



template <PerThreadAssertType type, bool allow>
class PerThreadAssertScope : public PerThreadAssertScopeBase {
 public:
#ifndef DEBUG
  PerThreadAssertScope() { }
  static void SetIsAllowed(bool is_allowed) { }
#else
  PerThreadAssertScope() {
    old_state_ = data_->get(type);
    data_->set(type, allow);
  }

  ~PerThreadAssertScope() { data_->set(type, old_state_); }

  static bool IsAllowed() { return AssertData()->get(type); }

 private:
  bool old_state_;
#endif
};

// Scope to document where we do not expect handles to be created.
typedef PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>
    DisallowHandleAllocation;

// Scope to introduce an exception to DisallowHandleAllocation.
typedef PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>
    AllowHandleAllocation;

// Scope to document where we do not expect any allocation and GC.
typedef PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, false>
    DisallowHeapAllocation;

// Scope to introduce an exception to DisallowHeapAllocation.
typedef PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, true>
    AllowHeapAllocation;

// Scope to document where we do not expect any handle dereferences.
typedef PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>
    DisallowHandleDereference;

// Scope to introduce an exception to DisallowHandleDereference.
typedef PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>
    AllowHandleDereference;

// Scope to document where we do not expect deferred handles to be dereferenced.
typedef PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT, false>
    DisallowDeferredHandleDereference;

// Scope to introduce an exception to DisallowDeferredHandleDereference.
typedef PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT, true>
    AllowDeferredHandleDereference;

} }  // namespace v8::internal

#endif  // V8_ASSERT_SCOPE_H_
