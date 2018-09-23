// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "src/api.h"
#include "src/handles.h"
#include "src/heap/heap.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

HandleBase::HandleBase(Object* object, Isolate* isolate)
    : location_(HandleScope::CreateHandle(isolate, object)) {}


HandleScope::HandleScope(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();
  isolate_ = isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}


HandleScope::~HandleScope() {
#ifdef DEBUG
  if (FLAG_check_handle_count) {
    int before = NumberOfHandles(isolate_);
    CloseScope(isolate_, prev_next_, prev_limit_);
    int after = NumberOfHandles(isolate_);
    DCHECK(after - before < kCheckHandleThreshold);
    DCHECK(before < kCheckHandleThreshold);
  } else {
#endif  // DEBUG
    CloseScope(isolate_, prev_next_, prev_limit_);
#ifdef DEBUG
  }
#endif  // DEBUG
}


void HandleScope::CloseScope(Isolate* isolate,
                             Object** prev_next,
                             Object** prev_limit) {
  HandleScopeData* current = isolate->handle_scope_data();

  std::swap(current->next, prev_next);
  current->level--;
  if (current->limit != prev_limit) {
    current->limit = prev_limit;
    DeleteExtensions(isolate);
#ifdef ENABLE_HANDLE_ZAPPING
    ZapRange(current->next, prev_limit);
  } else {
    ZapRange(current->next, prev_next);
#endif
  }
}


template <typename T>
Handle<T> HandleScope::CloseAndEscape(Handle<T> handle_value) {
  HandleScopeData* current = isolate_->handle_scope_data();

  T* value = *handle_value;
  // Throw away all handles in the current scope.
  CloseScope(isolate_, prev_next_, prev_limit_);
  // Allocate one handle in the parent scope.
  DCHECK(current->level > 0);
  Handle<T> result(value, isolate_);
  // Reinitialize the current scope (so that it's ready
  // to be used or closed again).
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
  return result;
}


template <typename T>
T** HandleScope::CreateHandle(Isolate* isolate, T* value) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* current = isolate->handle_scope_data();

  Object** cur = current->next;
  if (cur == current->limit) cur = Extend(isolate);
  // Update the current next field, set the value in the created
  // handle, and return the result.
  DCHECK(cur < current->limit);
  current->next = cur + 1;

  T** result = reinterpret_cast<T**>(cur);
  *result = value;
  return result;
}


#ifdef DEBUG
inline SealHandleScope::SealHandleScope(Isolate* isolate) : isolate_(isolate) {
  // Make sure the current thread is allowed to create handles to begin with.
  CHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* current = isolate_->handle_scope_data();
  // Shrink the current handle scope to make it impossible to do
  // handle allocations without an explicit handle scope.
  limit_ = current->limit;
  current->limit = current->next;
  level_ = current->level;
  current->level = 0;
}


inline SealHandleScope::~SealHandleScope() {
  // Restore state in current handle scope to re-enable handle
  // allocations.
  HandleScopeData* current = isolate_->handle_scope_data();
  DCHECK_EQ(0, current->level);
  current->level = level_;
  DCHECK_EQ(current->next, current->limit);
  current->limit = limit_;
}

#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_INL_H_
