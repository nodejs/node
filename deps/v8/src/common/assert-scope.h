// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_ASSERT_SCOPE_H_
#define V8_COMMON_ASSERT_SCOPE_H_

#include <stdint.h>

#include <memory>

#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;

enum PerThreadAssertType {
  SAFEPOINTS_ASSERT,
  HEAP_ALLOCATION_ASSERT,
  HANDLE_ALLOCATION_ASSERT,
  HANDLE_DEREFERENCE_ASSERT,
  CODE_DEPENDENCY_CHANGE_ASSERT,
  CODE_ALLOCATION_ASSERT,
  // Dummy type for disabling GC mole.
  GC_MOLE,
};

enum PerIsolateAssertType {
  JAVASCRIPT_EXECUTION_ASSERT,
  JAVASCRIPT_EXECUTION_THROWS,
  JAVASCRIPT_EXECUTION_DUMP,
  DEOPTIMIZATION_ASSERT,
  COMPILATION_ASSERT,
  NO_EXCEPTION_ASSERT,
};

template <PerThreadAssertType kType, bool kAllow>
class V8_NODISCARD PerThreadAssertScope {
 public:
  V8_EXPORT_PRIVATE PerThreadAssertScope();
  V8_EXPORT_PRIVATE ~PerThreadAssertScope();

  PerThreadAssertScope(const PerThreadAssertScope&) = delete;
  PerThreadAssertScope& operator=(const PerThreadAssertScope&) = delete;

  V8_EXPORT_PRIVATE static bool IsAllowed();

  void Release();

 private:
  base::Optional<uint32_t> old_data_;
};

template <PerIsolateAssertType kType, bool kAllow>
class V8_NODISCARD PerIsolateAssertScope {
 public:
  V8_EXPORT_PRIVATE explicit PerIsolateAssertScope(Isolate* isolate);
  PerIsolateAssertScope(const PerIsolateAssertScope&) = delete;
  PerIsolateAssertScope& operator=(const PerIsolateAssertScope&) = delete;
  V8_EXPORT_PRIVATE ~PerIsolateAssertScope();

  static bool IsAllowed(Isolate* isolate);

  V8_EXPORT_PRIVATE static void Open(Isolate* isolate,
                                     bool* was_execution_allowed);
  V8_EXPORT_PRIVATE static void Close(Isolate* isolate,
                                      bool was_execution_allowed);

 private:
  Isolate* isolate_;
  uint32_t old_data_;
};

template <typename... Scopes>
class CombinationAssertScope;

// Base case for CombinationAssertScope (equivalent to Scope).
template <typename Scope>
class V8_NODISCARD CombinationAssertScope<Scope> : public Scope {
 public:
  V8_EXPORT_PRIVATE static bool IsAllowed() {
    // Define IsAllowed() explicitly rather than with using Scope::IsAllowed, to
    // allow SFINAE removal of IsAllowed() when it's not defined (under debug).
    return Scope::IsAllowed();
  }
  using Scope::Release;
  using Scope::Scope;
};

// Inductive case for CombinationAssertScope.
template <typename Scope, typename... Scopes>
class CombinationAssertScope<Scope, Scopes...>
    : public Scope, public CombinationAssertScope<Scopes...> {
  using NextScopes = CombinationAssertScope<Scopes...>;

 public:
  // Constructor for per-thread scopes.
  V8_EXPORT_PRIVATE CombinationAssertScope() : Scope(), NextScopes() {}
  // Constructor for per-isolate scopes.
  V8_EXPORT_PRIVATE explicit CombinationAssertScope(Isolate* isolate)
      : Scope(isolate), NextScopes(isolate) {}

  V8_EXPORT_PRIVATE static bool IsAllowed() {
    return Scope::IsAllowed() && NextScopes::IsAllowed();
  }

  void Release() {
    // Release in reverse order.
    NextScopes::Release();
    Scope::Release();
  }
};

template <PerThreadAssertType kType, bool kAllow>
#ifdef DEBUG
class PerThreadAssertScopeDebugOnly
    : public PerThreadAssertScope<kType, kAllow> {
#else
class V8_NODISCARD PerThreadAssertScopeDebugOnly {
 public:
  PerThreadAssertScopeDebugOnly() {  // NOLINT (modernize-use-equals-default)
    // Define a constructor to avoid unused variable warnings.
  }
  void Release() {}
#endif
};

template <PerIsolateAssertType kType, bool kAllow>
#ifdef DEBUG
class PerIsolateAssertScopeDebugOnly
    : public PerIsolateAssertScope<kType, kAllow> {
 public:
  explicit PerIsolateAssertScopeDebugOnly(Isolate* isolate)
      : PerIsolateAssertScope<kType, kAllow>(isolate) {}
#else
class V8_NODISCARD PerIsolateAssertScopeDebugOnly {
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

// Scope to document where we do not expect safepoints to be entered.
using DisallowSafepoints =
    PerThreadAssertScopeDebugOnly<SAFEPOINTS_ASSERT, false>;

// Scope to introduce an exception to DisallowSafepoints.
using AllowSafepoints = PerThreadAssertScopeDebugOnly<SAFEPOINTS_ASSERT, true>;

// Scope to document where we do not expect any allocation.
using DisallowHeapAllocation =
    PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, false>;

// Scope to introduce an exception to DisallowHeapAllocation.
using AllowHeapAllocation =
    PerThreadAssertScopeDebugOnly<HEAP_ALLOCATION_ASSERT, true>;

// Scope to document where we do not expect any handle dereferences.
using DisallowHandleDereference =
    PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, false>;

// Scope to introduce an exception to DisallowHandleDereference.
using AllowHandleDereference =
    PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ASSERT, true>;

// Scope to document where we do not expect code dependencies to change.
using DisallowCodeDependencyChange =
    PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, false>;

// Scope to introduce an exception to DisallowCodeDependencyChange.
using AllowCodeDependencyChange =
    PerThreadAssertScopeDebugOnly<CODE_DEPENDENCY_CHANGE_ASSERT, true>;

// Scope to document where we do not expect code to be allocated.
using DisallowCodeAllocation =
    PerThreadAssertScopeDebugOnly<CODE_ALLOCATION_ASSERT, false>;

// Scope to introduce an exception to DisallowCodeAllocation.
using AllowCodeAllocation =
    PerThreadAssertScopeDebugOnly<CODE_ALLOCATION_ASSERT, true>;

// Scope to document where we do not expect garbage collections. It differs from
// DisallowHeapAllocation by also forbidding safepoints.
using DisallowGarbageCollection =
    CombinationAssertScope<DisallowSafepoints, DisallowHeapAllocation>;

// Scope to skip gc mole verification in places where we do tricky raw
// work.
using DisableGCMole = PerThreadAssertScopeDebugOnly<GC_MOLE, false>;

// The DISALLOW_GARBAGE_COLLECTION macro can be used to define a
// DisallowGarbageCollection field in classes that isn't present in release
// builds.
#ifdef DEBUG
#define DISALLOW_GARBAGE_COLLECTION(name) DisallowGarbageCollection name;
#else
#define DISALLOW_GARBAGE_COLLECTION(name)
#endif

// Scope to introduce an exception to DisallowGarbageCollection.
using AllowGarbageCollection =
    CombinationAssertScope<AllowSafepoints, AllowHeapAllocation>;

// Scope to document where we do not expect any access to the heap.
using DisallowHeapAccess =
    CombinationAssertScope<DisallowCodeDependencyChange,
                           DisallowHandleDereference, DisallowHandleAllocation,
                           DisallowHeapAllocation>;

// Scope to introduce an exception to DisallowHeapAccess.
using AllowHeapAccess =
    CombinationAssertScope<AllowCodeDependencyChange, AllowHandleDereference,
                           AllowHandleAllocation, AllowHeapAllocation>;

class DisallowHeapAccessIf {
 public:
  explicit DisallowHeapAccessIf(bool condition) {
    if (condition) maybe_disallow_.emplace();
  }

 private:
  base::Optional<DisallowHeapAccess> maybe_disallow_;
};

// Like MutexGuard but also asserts that no garbage collection happens while
// we're holding the mutex.
class V8_NODISCARD NoGarbageCollectionMutexGuard {
 public:
  explicit NoGarbageCollectionMutexGuard(base::Mutex* mutex)
      : guard_(mutex), mutex_(mutex), no_gc_(new DisallowGarbageCollection()) {}

  void Unlock() {
    mutex_->Unlock();
    no_gc_.reset();
  }
  void Lock() {
    mutex_->Lock();
    no_gc_.reset(new DisallowGarbageCollection());
  }

 private:
  base::MutexGuard guard_;
  base::Mutex* mutex_;
  std::unique_ptr<DisallowGarbageCollection> no_gc_;
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
extern template class PerThreadAssertScope<SAFEPOINTS_ASSERT, false>;
extern template class PerThreadAssertScope<SAFEPOINTS_ASSERT, true>;
extern template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>;
extern template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>;
extern template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>;
extern template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>;
extern template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT,
                                           false>;
extern template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;
extern template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, false>;
extern template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, true>;
extern template class PerThreadAssertScope<GC_MOLE, false>;

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
