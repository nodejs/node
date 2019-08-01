// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_ASSERT_SCOPE_H_
#define V8_COMMON_ASSERT_SCOPE_H_

#include <stdint.h>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/common/globals.h"
#include "src/utils/pointer-with-payload.h"

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
class PerThreadAssertScopeDebugOnly : public PerThreadAssertScope<type, allow> {
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
class PerIsolateAssertScopeDebugOnly
    : public PerIsolateAssertScope<type, allow> {
 public:
  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate)
      : PerIsolateAssertScope<type, allow>(isolate) {}
#else
class PerIsolateAssertScopeDebugOnly {
 public:
  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate) {}
#endif
};

// Per-thread assert scopes.

// Scope to document where we do not expect handles to be created.
using DisallowHandleAllocation =
    PerThreadAssertScopeDebugOnly<HANDLE_ALLOCATION_ASSERT, false>;

// Scope to introduce an exception to DisallowHandleAllocation.
using AllowHandleAllocation =
    PerThreadAssertScopeDebugOnly<HANDLE_ALLOCATION_ASSERT, true>;

// Scope to document where we do not expect any allocation and GC.
using DisallowHeapAllocation =
    PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, false>;
#ifdef DEBUG
#define DISALLOW_HEAP_ALLOCATION(name) DisallowHeapAllocation name;
#else
#define DISALLOW_HEAP_ALLOCATION(name)
#endif

// Scope to introduce an exception to DisallowHeapAllocation.
using AllowHeapAllocation =
    PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, true>;

// Scope to document where we do not expect any handle dereferences.
using DisallowHandleDereference =
    PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, false>;

// Scope to introduce an exception to DisallowHandleDereference.
using AllowHandleDereference =
    PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, true>;

// Scope to document where we do not expect deferred handles to be dereferenced.
using DisallowDeferredHandleDereference =
    PerThreadAssertScopeDebugOnly<DEFERRED_HANDLE_DEREFERENCE_ASSERT, false>;

// Scope to introduce an exception to DisallowDeferredHandleDereference.
using AllowDeferredHandleDereference =
    PerThreadAssertScopeDebugOnly<DEFERRED_HANDLE_DEREFERENCE_ASSERT, true>;

// Scope to document where we do not expect deferred handles to be dereferenced.
using DisallowCodeDependencyChange =
    PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, false>;

// Scope to introduce an exception to DisallowDeferredHandleDereference.
using AllowCodeDependencyChange =
    PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, true>;

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
using DisallowJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>;

// Scope to introduce an exception to DisallowJavascriptExecution.
using AllowJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>;

// Scope to document where we do not expect javascript execution (debug only)
using DisallowJavascriptExecutionDebugOnly =
    PerIsolateAssertScopeDebugOnly<JAVASCRIPT_EXECUTION_ASSERT, false>;

// Scope to introduce an exception to DisallowJavascriptExecutionDebugOnly.
using AllowJavascriptExecutionDebugOnly =
    PerIsolateAssertScopeDebugOnly<JAVASCRIPT_EXECUTION_ASSERT, true>;

// Scope in which javascript execution leads to exception being thrown.
using ThrowOnJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>;

// Scope to introduce an exception to ThrowOnJavascriptExecution.
using NoThrowOnJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>;

// Scope in which javascript execution causes dumps.
using DumpOnJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, false>;

// Scope in which javascript execution causes dumps.
using NoDumpOnJavascriptExecution =
    PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, true>;

// Scope to document where we do not expect deoptimization.
using DisallowDeoptimization =
    PerIsolateAssertScopeDebugOnly<DEOPTIMIZATION_ASSERT, false>;

// Scope to introduce an exception to DisallowDeoptimization.
using AllowDeoptimization =
    PerIsolateAssertScopeDebugOnly<DEOPTIMIZATION_ASSERT, true>;

// Scope to document where we do not expect deoptimization.
using DisallowCompilation =
    PerIsolateAssertScopeDebugOnly<COMPILATION_ASSERT, false>;

// Scope to introduce an exception to DisallowDeoptimization.
using AllowCompilation =
    PerIsolateAssertScopeDebugOnly<COMPILATION_ASSERT, true>;

// Scope to document where we do not expect exceptions.
using DisallowExceptions =
    PerIsolateAssertScopeDebugOnly<NO_EXCEPTION_ASSERT, false>;

// Scope to introduce an exception to DisallowExceptions.
using AllowExceptions =
    PerIsolateAssertScopeDebugOnly<NO_EXCEPTION_ASSERT, true>;

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

#endif  // V8_COMMON_ASSERT_SCOPE_H_
