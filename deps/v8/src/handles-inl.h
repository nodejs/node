// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "src/api.h"
#include "src/handles.h"
#include "src/heap/heap.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

template<typename T>
Handle<T>::Handle(T* obj) {
  location_ = HandleScope::CreateHandle(obj->GetIsolate(), obj);
}


template<typename T>
Handle<T>::Handle(T* obj, Isolate* isolate) {
  location_ = HandleScope::CreateHandle(isolate, obj);
}


template <typename T>
inline bool Handle<T>::is_identical_to(const Handle<T> o) const {
  // Dereferencing deferred handles to check object equality is safe.
  SLOW_DCHECK(
      (location_ == NULL || IsDereferenceAllowed(NO_DEFERRED_CHECK)) &&
      (o.location_ == NULL || o.IsDereferenceAllowed(NO_DEFERRED_CHECK)));
  if (location_ == o.location_) return true;
  if (location_ == NULL || o.location_ == NULL) return false;
  return *location_ == *o.location_;
}


template <typename T>
inline T* Handle<T>::operator*() const {
  SLOW_DCHECK(IsDereferenceAllowed(INCLUDE_DEFERRED_CHECK));
  return *BitCast<T**>(location_);
}

template <typename T>
inline T** Handle<T>::location() const {
  SLOW_DCHECK(location_ == NULL ||
              IsDereferenceAllowed(INCLUDE_DEFERRED_CHECK));
  return location_;
}

#ifdef DEBUG
template <typename T>
bool Handle<T>::IsDereferenceAllowed(DereferenceCheckMode mode) const {
  DCHECK(location_ != NULL);
  Object* object = *BitCast<T**>(location_);
  if (object->IsSmi()) return true;
  HeapObject* heap_object = HeapObject::cast(object);
  Heap* heap = heap_object->GetHeap();
  Object** handle = reinterpret_cast<Object**>(location_);
  Object** roots_array_start = heap->roots_array_start();
  if (roots_array_start <= handle &&
      handle < roots_array_start + Heap::kStrongRootListLength &&
      heap->RootCanBeTreatedAsConstant(
        static_cast<Heap::RootListIndex>(handle - roots_array_start))) {
    return true;
  }
  if (!AllowHandleDereference::IsAllowed()) return false;
  if (mode == INCLUDE_DEFERRED_CHECK &&
      !AllowDeferredHandleDereference::IsAllowed()) {
    // Accessing cells, maps and internalized strings is safe.
    if (heap_object->IsCell()) return true;
    if (heap_object->IsMap()) return true;
    if (heap_object->IsInternalizedString()) return true;
    return !heap->isolate()->IsDeferredHandle(handle);
  }
  return true;
}
#endif



HandleScope::HandleScope(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();
  isolate_ = isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}


HandleScope::~HandleScope() {
  CloseScope(isolate_, prev_next_, prev_limit_);
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
  Handle<T> result(CreateHandle<T>(isolate_, value));
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

  internal::Object** cur = current->next;
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

} }  // namespace v8::internal

#endif  // V8_HANDLES_INL_H_
