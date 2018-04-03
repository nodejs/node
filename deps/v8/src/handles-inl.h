// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

HandleBase::HandleBase(Object* object, Isolate* isolate)
    : location_(HandleScope::GetHandle(isolate, object)) {}


template <typename T>
// Allocate a new handle for the object, do not canonicalize.
Handle<T> Handle<T>::New(T* object, Isolate* isolate) {
  return Handle(
      reinterpret_cast<T**>(HandleScope::CreateHandle(isolate, object)));
}


HandleScope::HandleScope(Isolate* isolate) {
  HandleScopeData* data = isolate->handle_scope_data();
  isolate_ = isolate;
  prev_next_ = data->next;
  prev_limit_ = data->limit;
  data->level++;
}

template <typename T>
Handle<T>::Handle(T* object) : Handle(object, object->GetIsolate()) {}

template <typename T>
Handle<T>::Handle(T* object, Isolate* isolate) : HandleBase(object, isolate) {}

template <typename T>
inline std::ostream& operator<<(std::ostream& os, Handle<T> handle) {
  return os << Brief(*handle);
}


HandleScope::~HandleScope() {
#ifdef DEBUG
  if (FLAG_check_handle_count) {
    int before = NumberOfHandles(isolate_);
    CloseScope(isolate_, prev_next_, prev_limit_);
    int after = NumberOfHandles(isolate_);
    DCHECK_LT(after - before, kCheckHandleThreshold);
    DCHECK_LT(before, kCheckHandleThreshold);
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
  DCHECK(current->level > current->sealed_level);
  Handle<T> result(value, isolate_);
  // Reinitialize the current scope (so that it's ready
  // to be used or closed again).
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
  return result;
}

Object** HandleScope::CreateHandle(Isolate* isolate, Object* value) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* data = isolate->handle_scope_data();

  Object** result = data->next;
  if (result == data->limit) result = Extend(isolate);
  // Update the current next field, set the value in the created
  // handle, and return the result.
  DCHECK(result < data->limit);
  data->next = result + 1;

  *result = value;
  return result;
}


Object** HandleScope::GetHandle(Isolate* isolate, Object* value) {
  DCHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* data = isolate->handle_scope_data();
  CanonicalHandleScope* canonical = data->canonical_scope;
  return canonical ? canonical->Lookup(value) : CreateHandle(isolate, value);
}


#ifdef DEBUG
inline SealHandleScope::SealHandleScope(Isolate* isolate) : isolate_(isolate) {
  // Make sure the current thread is allowed to create handles to begin with.
  DCHECK(AllowHandleAllocation::IsAllowed());
  HandleScopeData* current = isolate_->handle_scope_data();
  // Shrink the current handle scope to make it impossible to do
  // handle allocations without an explicit handle scope.
  prev_limit_ = current->limit;
  current->limit = current->next;
  prev_sealed_level_ = current->sealed_level;
  current->sealed_level = current->level;
}


inline SealHandleScope::~SealHandleScope() {
  // Restore state in current handle scope to re-enable handle
  // allocations.
  HandleScopeData* current = isolate_->handle_scope_data();
  DCHECK_EQ(current->next, current->limit);
  current->limit = prev_limit_;
  DCHECK_EQ(current->level, current->sealed_level);
  current->sealed_level = prev_sealed_level_;
}

#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_HANDLES_INL_H_
