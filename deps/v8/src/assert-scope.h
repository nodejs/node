// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASSERT_SCOPE_H_
#define V8_ASSERT_SCOPE_H_

#include <stdint.h>
#include "src/base/macros.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class PerThreadAssertData;


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
  DEOPTIMIZATION_ASSERT,
  COMPILATION_ASSERT,
  NO_EXCEPTION_ASSERT
};

template <PerThreadAssertType kType, bool kAllow>
class PerThreadAssertScope {
 public:
  V8_EXPORT_PRIVATE PerThreadAssertScope();
  V8_EXPORT_PRIVATE ~PerThreadAssertScope();

  V8_EXPORT_PRIVATE static bool IsAllowed();

  void Release();

 private:
  PerThreadAssertData* data_;
  bool old_state_;

  DISALLOW_COPY_AND_ASSIGN(PerThreadAssertScope);
};


template <PerIsolateAssertType type, bool allow>
class PerIsolateAssertScope {
 public:
  explicit PerIsolateAssertScope(Isolate* isolate);
  ~PerIsolateAssertScope();

  static bool IsAllowed(Isolate* isolate);

 private:
  class DataBit;

  Isolate* isolate_;
  uint32_t old_data_;

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
  void Release() {}
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

// Scope to document where we do not expect javascript execution (debug only)
typedef PerIsolateAssertScopeDebugOnly<JAVASCRIPT_EXECUTION_ASSERT, false>
    DisallowJavascriptExecutionDebugOnly;

// Scope to introduce an exception to DisallowJavascriptExecutionDebugOnly.
typedef PerIsolateAssertScopeDebugOnly<JAVASCRIPT_EXECUTION_ASSERT, true>
    AllowJavascriptExecutionDebugOnly;

// Scope in which javascript execution leads to exception being thrown.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>
    ThrowOnJavascriptExecution;

// Scope to introduce an exception to ThrowOnJavascriptExecution.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>
    NoThrowOnJavascriptExecution;

// Scope to document where we do not expect deoptimization.
typedef PerIsolateAssertScopeDebugOnly<DEOPTIMIZATION_ASSERT, false>
    DisallowDeoptimization;

// Scope to introduce an exception to DisallowDeoptimization.
typedef PerIsolateAssertScopeDebugOnly<DEOPTIMIZATION_ASSERT, true>
    AllowDeoptimization;

// Scope to document where we do not expect deoptimization.
typedef PerIsolateAssertScopeDebugOnly<COMPILATION_ASSERT, false>
    DisallowCompilation;

// Scope to introduce an exception to DisallowDeoptimization.
typedef PerIsolateAssertScopeDebugOnly<COMPILATION_ASSERT, true>
    AllowCompilation;

// Scope to document where we do not expect exceptions.
typedef PerIsolateAssertScopeDebugOnly<NO_EXCEPTION_ASSERT, false>
    DisallowExceptions;

// Scope to introduce an exception to DisallowExceptions.
typedef PerIsolateAssertScopeDebugOnly<NO_EXCEPTION_ASSERT, true>
    AllowExceptions;
}  // namespace internal
}  // namespace v8

#endif  // V8_ASSERT_SCOPE_H_
