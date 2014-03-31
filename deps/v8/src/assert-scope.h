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
#include "utils.h"

namespace v8 {
namespace internal {

class Isolate;

enum PerThreadAssertType {
  HEAP_ALLOCATION_ASSERT,
  HANDLE_ALLOCATION_ASSERT,
  HANDLE_DEREFERENCE_ASSERT,
  DEFERRED_HANDLE_DEREFERENCE_ASSERT,
  CODE_DEPENDENCY_CHANGE_ASSERT,
  LAST_PER_THREAD_ASSERT_TYPE
};


enum PerIsolateAssertType {
  JAVASCRIPT_EXECUTION_ASSERT,
  JAVASCRIPT_EXECUTION_THROWS,
  ALLOCATION_FAILURE_ASSERT
};


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


class PerThreadAssertScopeBase {
 protected:
  PerThreadAssertScopeBase() {
    data_ = GetAssertData();
    if (data_ == NULL) {
      data_ = new PerThreadAssertData();
      SetThreadLocalData(data_);
    }
    data_->increment_level();
  }

  ~PerThreadAssertScopeBase() {
    if (!data_->decrement_level()) return;
    for (int i = 0; i < LAST_PER_THREAD_ASSERT_TYPE; i++) {
      ASSERT(data_->get(static_cast<PerThreadAssertType>(i)));
    }
    delete data_;
    SetThreadLocalData(NULL);
  }

  static PerThreadAssertData* GetAssertData() {
    return reinterpret_cast<PerThreadAssertData*>(
        Thread::GetThreadLocal(thread_local_key));
  }

  static Thread::LocalStorageKey thread_local_key;
  PerThreadAssertData* data_;
  friend class Isolate;

 private:
  static void SetThreadLocalData(PerThreadAssertData* data) {
    Thread::SetThreadLocal(thread_local_key, data);
  }
};


template <PerThreadAssertType type, bool allow>
class PerThreadAssertScope : public PerThreadAssertScopeBase {
 public:
  PerThreadAssertScope() {
    old_state_ = data_->get(type);
    data_->set(type, allow);
  }

  ~PerThreadAssertScope() { data_->set(type, old_state_); }

  static bool IsAllowed() {
    PerThreadAssertData* data = GetAssertData();
    return data == NULL || data->get(type);
  }

 private:
  bool old_state_;

  DISALLOW_COPY_AND_ASSIGN(PerThreadAssertScope);
};


class PerIsolateAssertBase {
 protected:
  static uint32_t GetData(Isolate* isolate);
  static void SetData(Isolate* isolate, uint32_t data);
};


template <PerIsolateAssertType type, bool allow>
class PerIsolateAssertScope : public PerIsolateAssertBase {
 public:
  explicit PerIsolateAssertScope(Isolate* isolate) : isolate_(isolate) {
    STATIC_ASSERT(type < 32);
    old_data_ = GetData(isolate_);
    SetData(isolate_, DataBit::update(old_data_, allow));
  }

  ~PerIsolateAssertScope() {
    SetData(isolate_, old_data_);
  }

  static bool IsAllowed(Isolate* isolate) {
    return DataBit::decode(GetData(isolate));
  }

 private:
  typedef BitField<bool, type, 1> DataBit;

  uint32_t old_data_;
  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(PerIsolateAssertScope);
};


template <PerThreadAssertType type, bool allow>
#ifdef DEBUG
class PerThreadAssertScopeDebugOnly : public
    PerThreadAssertScope<type, allow> {
#else
class PerThreadAssertScopeDebugOnly {
 public:
  PerThreadAssertScopeDebugOnly() { }
#endif
};


template <PerIsolateAssertType type, bool allow>
#ifdef DEBUG
class PerIsolateAssertScopeDebugOnly : public
    PerIsolateAssertScope<type, allow> {
 public:
  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate)
      : PerIsolateAssertScope<type, allow>(isolate) { }
#else
class PerIsolateAssertScopeDebugOnly {
 public:
  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate) { }
#endif
};

// Per-thread assert scopes.

// Scope to document where we do not expect handles to be created.
typedef PerThreadAssertScopeDebugOnly<HANDLE_ALLOCATION_ASSERT, false>
    DisallowHandleAllocation;

// Scope to introduce an exception to DisallowHandleAllocation.
typedef PerThreadAssertScopeDebugOnly<HANDLE_ALLOCATION_ASSERT, true>
    AllowHandleAllocation;

// Scope to document where we do not expect any allocation and GC.
typedef PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, false>
    DisallowHeapAllocation;

// Scope to introduce an exception to DisallowHeapAllocation.
typedef PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, true>
    AllowHeapAllocation;

// Scope to document where we do not expect any handle dereferences.
typedef PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, false>
    DisallowHandleDereference;

// Scope to introduce an exception to DisallowHandleDereference.
typedef PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, true>
    AllowHandleDereference;

// Scope to document where we do not expect deferred handles to be dereferenced.
typedef PerThreadAssertScopeDebugOnly<DEFERRED_HANDLE_DEREFERENCE_ASSERT, false>
    DisallowDeferredHandleDereference;

// Scope to introduce an exception to DisallowDeferredHandleDereference.
typedef PerThreadAssertScopeDebugOnly<DEFERRED_HANDLE_DEREFERENCE_ASSERT, true>
    AllowDeferredHandleDereference;

// Scope to document where we do not expect deferred handles to be dereferenced.
typedef PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, false>
    DisallowCodeDependencyChange;

// Scope to introduce an exception to DisallowDeferredHandleDereference.
typedef PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, true>
    AllowCodeDependencyChange;


// Per-isolate assert scopes.

// Scope to document where we do not expect javascript execution.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>
    DisallowJavascriptExecution;

// Scope to introduce an exception to DisallowJavascriptExecution.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>
    AllowJavascriptExecution;

// Scope in which javascript execution leads to exception being thrown.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>
    ThrowOnJavascriptExecution;

// Scope to introduce an exception to ThrowOnJavascriptExecution.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>
    NoThrowOnJavascriptExecution;

// Scope to document where we do not expect an allocation failure.
typedef PerIsolateAssertScopeDebugOnly<ALLOCATION_FAILURE_ASSERT, false>
    DisallowAllocationFailure;

// Scope to introduce an exception to DisallowAllocationFailure.
typedef PerIsolateAssertScopeDebugOnly<ALLOCATION_FAILURE_ASSERT, true>
    AllowAllocationFailure;

} }  // namespace v8::internal

#endif  // V8_ASSERT_SCOPE_H_
