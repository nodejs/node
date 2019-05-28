// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ASSERT_SCOPE_H_
#define V8_ASSERT_SCOPE_H_

#include <stdint.h>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/globals.h"
#include "src/pointer-with-payload.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class PerThreadAssertData;

template <>
struct PointerWithPayloadTraits<PerThreadAssertData> {
  static constexpr int value = 1;
};

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
  JAVASCRIPT_EXECUTION_DUMP,
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
  PointerWithPayload<PerThreadAssertData, bool, 1> data_and_old_state_;

  V8_INLINE void set_data(PerThreadAssertData* data) {
    data_and_old_state_.SetPointer(data);
  }

  V8_INLINE PerThreadAssertData* data() const {
    return data_and_old_state_.GetPointer();
  }

  V8_INLINE void set_old_state(bool old_state) {
    return data_and_old_state_.SetPayload(old_state);
  }

  V8_INLINE bool old_state() const { return data_and_old_state_.GetPayload(); }

  DISALLOW_COPY_AND_ASSIGN(PerThreadAssertScope);
};


template <PerIsolateAssertType type, bool allow>
class PerIsolateAssertScope {
 public:
  V8_EXPORT_PRIVATE explicit PerIsolateAssertScope(Isolate* isolate);
  V8_EXPORT_PRIVATE ~PerIsolateAssertScope();

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
  PerThreadAssertScopeDebugOnly() {  // NOLINT (modernize-use-equals-default)
    // Define a constructor to avoid unused variable warnings.
  }
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
#ifdef DEBUG
#define DISALLOW_HEAP_ALLOCATION(name) DisallowHeapAllocation name;
#else
#define DISALLOW_HEAP_ALLOCATION(name)
#endif

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

class DisallowHeapAccess {
  DisallowCodeDependencyChange no_dependency_change_;
  DisallowHandleAllocation no_handle_allocation_;
  DisallowHandleDereference no_handle_dereference_;
  DisallowHeapAllocation no_heap_allocation_;
};

class DisallowHeapAccessIf {
 public:
  explicit DisallowHeapAccessIf(bool condition) {
    if (condition) maybe_disallow_.emplace();
  }

 private:
  base::Optional<DisallowHeapAccess> maybe_disallow_;
};

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

// Scope in which javascript execution causes dumps.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, false>
    DumpOnJavascriptExecution;

// Scope in which javascript execution causes dumps.
typedef PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, true>
    NoDumpOnJavascriptExecution;

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

// Explicit instantiation declarations.
extern template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, false>;
extern template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, true>;
extern template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>;
extern template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>;
extern template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>;
extern template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>;
extern template class PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT,
                                           false>;
extern template class PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT,
                                           true>;
extern template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT,
                                           false>;
extern template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;

extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>;
extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>;
extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>;
extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>;
extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, false>;
extern template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, true>;
extern template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, false>;
extern template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, true>;
extern template class PerIsolateAssertScope<COMPILATION_ASSERT, false>;
extern template class PerIsolateAssertScope<COMPILATION_ASSERT, true>;
extern template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, false>;
extern template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, true>;

}  // namespace internal
}  // namespace v8

#endif  // V8_ASSERT_SCOPE_H_
