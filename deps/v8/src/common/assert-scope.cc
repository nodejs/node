// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/assert-scope.h"

#include "src/base/bit-field.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"
#include "src/execution/isolate.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

namespace {

// All asserts are allowed by default except for one.
constexpr uint32_t kInitialValue =
    ~(1 << HANDLE_DEREFERENCE_ALL_THREADS_ASSERT);

template <PerThreadAssertType kType>
using PerThreadDataBit = base::BitField<bool, kType, 1>;

// Thread-local storage for assert data.
thread_local uint32_t current_per_thread_assert_data(kInitialValue);

}  // namespace

template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::PerThreadAssertScope()
    : old_data_(current_per_thread_assert_data) {
  current_per_thread_assert_data =
      PerThreadDataBit<kType>::update(old_data_.value(), kAllow);
}

template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::~PerThreadAssertScope() {
  if (!old_data_.has_value()) return;
  Release();
}

template <PerThreadAssertType kType, bool kAllow>
void PerThreadAssertScope<kType, kAllow>::Release() {
  current_per_thread_assert_data = old_data_.value();
  old_data_.reset();
}

// static
template <PerThreadAssertType kType, bool kAllow>
bool PerThreadAssertScope<kType, kAllow>::IsAllowed() {
  return PerThreadDataBit<kType>::decode(current_per_thread_assert_data);
}

#define PER_ISOLATE_ASSERT_SCOPE_DEFINITION(ScopeType, field, enable)      \
  ScopeType::ScopeType(Isolate* isolate)                                   \
      : isolate_(isolate), old_data_(isolate->field()) {                   \
    DCHECK_NOT_NULL(isolate);                                              \
    isolate_->set_##field(enable);                                         \
  }                                                                        \
                                                                           \
  ScopeType::~ScopeType() { isolate_->set_##field(old_data_); }            \
                                                                           \
  /* static */                                                             \
  bool ScopeType::IsAllowed(Isolate* isolate) { return isolate->field(); } \
                                                                           \
  /* static */                                                             \
  void ScopeType::Open(Isolate* isolate, bool* was_execution_allowed) {    \
    DCHECK_NOT_NULL(isolate);                                              \
    DCHECK_NOT_NULL(was_execution_allowed);                                \
    *was_execution_allowed = isolate->field();                             \
    isolate->set_##field(enable);                                          \
  }                                                                        \
  /* static */                                                             \
  void ScopeType::Close(Isolate* isolate, bool was_execution_allowed) {    \
    DCHECK_NOT_NULL(isolate);                                              \
    isolate->set_##field(was_execution_allowed);                           \
  }

#define PER_ISOLATE_ASSERT_ENABLE_SCOPE_DEFINITION(EnableType, _, field, \
                                                   enable)               \
  PER_ISOLATE_ASSERT_SCOPE_DEFINITION(EnableType, field, enable)

#define PER_ISOLATE_ASSERT_DISABLE_SCOPE_DEFINITION(_, DisableType, field, \
                                                    enable)                \
  PER_ISOLATE_ASSERT_SCOPE_DEFINITION(DisableType, field, enable)

PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_ASSERT_ENABLE_SCOPE_DEFINITION, true)
PER_ISOLATE_CHECK_TYPE(PER_ISOLATE_ASSERT_ENABLE_SCOPE_DEFINITION, true)
PER_ISOLATE_DCHECK_TYPE(PER_ISOLATE_ASSERT_DISABLE_SCOPE_DEFINITION, false)
PER_ISOLATE_CHECK_TYPE(PER_ISOLATE_ASSERT_DISABLE_SCOPE_DEFINITION, false)

// -----------------------------------------------------------------------------
// Instantiations.

template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<SAFEPOINTS_ASSERT, false>;
template class PerThreadAssertScope<SAFEPOINTS_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ALL_THREADS_ASSERT,
                                    true>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, false>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<GC_MOLE, false>;
template class PerThreadAssertScope<POSITION_INFO_SLOW_ASSERT, false>;
template class PerThreadAssertScope<POSITION_INFO_SLOW_ASSERT, true>;

}  // namespace internal
}  // namespace v8
