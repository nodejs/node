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
  HANDLE_DEREFERENCE_ALL_THREADS_ASSERT,
  CODE_DEPENDENCY_CHANGE_ASSERT,
  CODE_ALLOCATION_ASSERT,
  // Dummy type for disabling GC mole.
  GC_MOLE,
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

// Per-isolate assert scopes.

#define PER_ISOLATE_DCHECK_TYPE(V, enable)                              \
  /* Scope to document where we do not expect javascript execution. */  \
  /* Scope to introduce an exception to DisallowJavascriptExecution. */ \
  V(AllowJavascriptExecution, DisallowJavascriptExecution,              \
    javascript_execution_assert, enable)                                \
  /* Scope to document where we do not expect deoptimization. */        \
  /* Scope to introduce an exception to DisallowDeoptimization. */      \
  V(AllowDeoptimization, DisallowDeoptimization, deoptimization_assert, \
    enable)                                                             \
  /* Scope to document where we do not expect deoptimization. */        \
  /* Scope to introduce an exception to DisallowDeoptimization. */      \
  V(AllowCompilation, DisallowCompilation, compilation_assert, enable)  \
  /* Scope to document where we do not expect exceptions. */            \
  /* Scope to introduce an exception to DisallowExceptions. */          \
  V(AllowExceptions, DisallowExceptions, no_exception_assert, enable)

#define PER_ISOLATE_CHECK_TYPE(V, enable)                                    \
  /* Scope in which javascript execution leads to exception being thrown. */ \
  /* Scope to introduce an exception to ThrowOnJavascriptExecution. */       \
  V(NoThrowOnJavascriptExecution, ThrowOnJavascriptExecution,                \
    javascript_execution_throws, enable)                                     \
  /* Scope in which javascript execution causes dumps. */                    \
  /* Scope in which javascript execution doesn't cause dumps. */             \
  V(NoDumpOnJavascriptExecution, DumpOnJavascriptExecution,                  \
    javascript_execution_dump, enable)

#define PER_ISOLATE_ASSERT_SCOPE_DECLARATION(ScopeType)              \
  class V8_NODISCARD ScopeType {                                     \
   public:                                                           \
    V8_EXPORT_PRIVATE explicit ScopeType(Isolate* isolate);          \
    ScopeType(const ScopeType&) = delete;                            \
    ScopeType& operator=(const ScopeType&) = delete;                 \
    V8_EXPORT_PRIVATE ~ScopeType();                                  \
                                                                     \
    static bool IsAllowed(Isolate* isolate);                         \
                                                                     \
    V8_EXPORT_PRIVATE static void Open(Isolate* isolate,             \
                                       bool* was_execution_allowed); \
    V8_EXPORT_PRIVATE static void Close(Isolate* isolate,            \
                                        bool was_execution_allowed); \
                                                                     \
   private:                                                          \
    Isolate* isolate_;                                               \
    bool old_data_;                                                  \
  };

#define PER_ISOLATE_ASSERT_ENABLE_SCOPE(EnableType, _1, _2, _3) \
  PER_ISOLATE_ASSERT_SCOPE_DECLARATION(EnableType)

#define PER_ISOLATE_ASSERT_DISABLE_SCOPE(_1, DisableType, _2, _3) \
  PER_ISOLATE_ASSERT_SCOPE_DECLARATION(DisableType)

PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_ASSERT_ENABLE_SCOPE, true)
PER_ISOLATE_CHECK_TYPE(PER_ISOLATE_ASSERT_ENABLE_SCOPE, true)
PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_ASSERT_DISABLE_SCOPE, false)
PER_ISOLATE_CHECK_TYPE(PER_ISOLATE_ASSERT_DISABLE_SCOPE, false)

#ifdef DEBUG
#define PER_ISOLATE_DCHECK_ENABLE_SCOPE(EnableType, DisableType, field, _)    \
  class EnableType##DebugOnly : public EnableType {                           \
   public:                                                                    \
    explicit EnableType##DebugOnly(Isolate* isolate) : EnableType(isolate) {} \
  };
#else
#define PER_ISOLATE_DCHECK_ENABLE_SCOPE(EnableType, DisableType, field, _) \
  class V8_NODISCARD EnableType##DebugOnly {                               \
   public:                                                                 \
    explicit EnableType##DebugOnly(Isolate* isolate) {}                    \
  };
#endif

#ifdef DEBUG
#define PER_ISOLATE_DCHECK_DISABLE_SCOPE(EnableType, DisableType, field, _) \
  class DisableType##DebugOnly : public DisableType {                       \
   public:                                                                  \
    explicit DisableType##DebugOnly(Isolate* isolate)                       \
        : DisableType(isolate) {}                                           \
  };
#else
#define PER_ISOLATE_DCHECK_DISABLE_SCOPE(EnableType, DisableType, field, _) \
  class V8_NODISCARD DisableType##DebugOnly {                               \
   public:                                                                  \
    explicit DisableType##DebugOnly(Isolate* isolate) {}                    \
  };
#endif

PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_DCHECK_ENABLE_SCOPE, true)
PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_DCHECK_DISABLE_SCOPE, false)

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
  PerThreadAssertScopeDebugOnly() {
    // Define a constructor to avoid unused variable warnings.
  }
  void Release() {}
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

// Explicitly allow handle dereference for all threads/isolates on one
// particular thread.
using AllowHandleDereferenceAllThreads =
    PerThreadAssertScopeDebugOnly<HANDLE_DEREFERENCE_ALL_THREADS_ASSERT, true>;

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

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_ASSERT_SCOPE_H_
