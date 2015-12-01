// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assert-scope.h"

#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"
#include "src/debug/debug.h"
#include "src/isolate.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace {

struct PerThreadAssertKeyConstructTrait final {
  static void Construct(base::Thread::LocalStorageKey* key) {
    *key = base::Thread::CreateThreadLocalKey();
  }
};


typedef base::LazyStaticInstance<base::Thread::LocalStorageKey,
                                 PerThreadAssertKeyConstructTrait>::type
    PerThreadAssertKey;


PerThreadAssertKey kPerThreadAssertKey;

}  // namespace


class PerThreadAssertData final {
 public:
  PerThreadAssertData() : nesting_level_(0) {
    for (int i = 0; i < LAST_PER_THREAD_ASSERT_TYPE; i++) {
      assert_states_[i] = true;
    }
  }

  ~PerThreadAssertData() {
    for (int i = 0; i < LAST_PER_THREAD_ASSERT_TYPE; ++i) {
      DCHECK(assert_states_[i]);
    }
  }

  bool Get(PerThreadAssertType type) const { return assert_states_[type]; }
  void Set(PerThreadAssertType type, bool x) { assert_states_[type] = x; }

  void IncrementLevel() { ++nesting_level_; }
  bool DecrementLevel() { return --nesting_level_ == 0; }

  static PerThreadAssertData* GetCurrent() {
    return reinterpret_cast<PerThreadAssertData*>(
        base::Thread::GetThreadLocal(kPerThreadAssertKey.Get()));
  }
  static void SetCurrent(PerThreadAssertData* data) {
    base::Thread::SetThreadLocal(kPerThreadAssertKey.Get(), data);
  }

 private:
  bool assert_states_[LAST_PER_THREAD_ASSERT_TYPE];
  int nesting_level_;

  DISALLOW_COPY_AND_ASSIGN(PerThreadAssertData);
};


template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::PerThreadAssertScope()
    : data_(PerThreadAssertData::GetCurrent()) {
  if (data_ == NULL) {
    data_ = new PerThreadAssertData();
    PerThreadAssertData::SetCurrent(data_);
  }
  data_->IncrementLevel();
  old_state_ = data_->Get(kType);
  data_->Set(kType, kAllow);
}


template <PerThreadAssertType kType, bool kAllow>
PerThreadAssertScope<kType, kAllow>::~PerThreadAssertScope() {
  DCHECK_NOT_NULL(data_);
  data_->Set(kType, old_state_);
  if (data_->DecrementLevel()) {
    PerThreadAssertData::SetCurrent(NULL);
    delete data_;
  }
}


// static
template <PerThreadAssertType kType, bool kAllow>
bool PerThreadAssertScope<kType, kAllow>::IsAllowed() {
  PerThreadAssertData* data = PerThreadAssertData::GetCurrent();
  return data == NULL || data->Get(kType);
}


template <PerIsolateAssertType kType, bool kAllow>
class PerIsolateAssertScope<kType, kAllow>::DataBit
    : public BitField<bool, kType, 1> {};


template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::PerIsolateAssertScope(Isolate* isolate)
    : isolate_(isolate), old_data_(isolate->per_isolate_assert_data()) {
  DCHECK_NOT_NULL(isolate);
  STATIC_ASSERT(kType < 32);
  isolate_->set_per_isolate_assert_data(DataBit::update(old_data_, kAllow));
}


template <PerIsolateAssertType kType, bool kAllow>
PerIsolateAssertScope<kType, kAllow>::~PerIsolateAssertScope() {
  isolate_->set_per_isolate_assert_data(old_data_);
}


// static
template <PerIsolateAssertType kType, bool kAllow>
bool PerIsolateAssertScope<kType, kAllow>::IsAllowed(Isolate* isolate) {
  return DataBit::decode(isolate->per_isolate_assert_data());
}


// -----------------------------------------------------------------------------
// Instantiations.

template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HEAP_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_ALLOCATION_ASSERT, true>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, false>;
template class PerThreadAssertScope<HANDLE_DEREFERENCE_ASSERT, true>;
template class PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT, false>;
template class PerThreadAssertScope<DEFERRED_HANDLE_DEREFERENCE_ASSERT, true>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, false>;
template class PerThreadAssertScope<CODE_DEPENDENCY_CHANGE_ASSERT, true>;

template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_ASSERT, true>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, false>;
template class PerIsolateAssertScope<JAVASCRIPT_EXECUTION_THROWS, true>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, false>;
template class PerIsolateAssertScope<DEOPTIMIZATION_ASSERT, true>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, false>;
template class PerIsolateAssertScope<COMPILATION_ASSERT, true>;

}  // namespace internal
}  // namespace v8
