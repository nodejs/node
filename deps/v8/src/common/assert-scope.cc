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

template <PerThreadAssertType kType>
using PerThreadDataBit = base::BitField<bool, kType, 1>;
template <PerIsolateAssertType kType>
using PerIsolateDataBit = base::BitField<bool, kType, 1>;

// Thread-local storage for assert data. Default all asserts to "allow".
thread_local uint32_t current_per_thread_assert_data(~0);

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

template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::PerIsolateAssertScope(Isolate* isolate)
    : isolate_(isolate), old_data_(isolate->per_isolate_assert_data()) {
  DCHECK_NOT_NULL(isolate);
  isolate_->set_per_isolate_assert_data(
      PerIsolateDataBit<kType>::update(old_data_, kAllow));
}

template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::~PerIsolateAssertScope() {
  isolate_->set_per_isolate_assert_data(old_data_);
}

// static
template <PerIsolateAssertType kType, bool kAllow>
bool PerIsolateAssertScope<kType, kAllow>::IsAllowed(Isolate* isolate) {
  return PerIsolateDataBit<kType>::decode(isolate->per_isolate_assert_data());
}

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
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, false>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<CODE_ALLOCATION_ASSERT, true>;

template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_DUMP, true>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, false>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, true>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, false>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, true>;
template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, false>;
template class PerIsolateAssertScope<NO_EXCEPTION_ASSERT, true>;

}  // namespace internal
}  // namespace v8
