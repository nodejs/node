// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef V8_HANDLES_INL_H_
#define V8_HANDLES_INL_H_

#include "api.h"
#include "apiutils.h"
#include "handles.h"
#include "isolate.h"

namespace v8 {
namespace internal {

inline Isolate* GetIsolateForHandle(Object* obj) {
  return Isolate::Current();
}

inline Isolate* GetIsolateForHandle(HeapObject* obj) {
  return obj->GetIsolate();
}

template<typename T>
Handle<T>::Handle(T* obj) {
  ASSERT(!obj->IsFailure());
  location_ = HandleScope::CreateHandle(obj, GetIsolateForHandle(obj));
}


template<typename T>
Handle<T>::Handle(T* obj, Isolate* isolate) {
  ASSERT(!obj->IsFailure());
  location_ = HandleScope::CreateHandle(obj, isolate);
}


template <typename T>
inline T* Handle<T>::operator*() const {
  ASSERT(location_ != NULL);
  ASSERT(reinterpret_cast<Address>(*location_) != kHandleZapValue);
  return *BitCast<T**>(location_);
}


HandleScope::HandleScope() {
  Isolate* isolate = Isolate::Current();
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();
  isolate_ = isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}


HandleScope::HandleScope(Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();
  isolate_ = isolate;
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
}


HandleScope::~HandleScope() {
  CloseScope();
}

void HandleScope::CloseScope() {
  ASSERT(isolate_ == Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  current->next = prev_next_;
  current->level--;
  if (current->limit != prev_limit_) {
    current->limit = prev_limit_;
    DeleteExtensions(isolate_);
  }
#ifdef DEBUG
  ZapRange(prev_next_, prev_limit_);
#endif
}


template <typename T>
Handle<T> HandleScope::CloseAndEscape(Handle<T> handle_value) {
  T* value = *handle_value;
  // Throw away all handles in the current scope.
  CloseScope();
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate_->handle_scope_data();
  // Allocate one handle in the parent scope.
  ASSERT(current->level > 0);
  Handle<T> result(CreateHandle<T>(value, isolate_));
  // Reinitialize the current scope (so that it's ready
  // to be used or closed again).
  prev_next_ = current->next;
  prev_limit_ = current->limit;
  current->level++;
  return result;
}


template <typename T>
T** HandleScope::CreateHandle(T* value, Isolate* isolate) {
  ASSERT(isolate == Isolate::Current());
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();

  internal::Object** cur = current->next;
  if (cur == current->limit) cur = Extend();
  // Update the current next field, set the value in the created
  // handle, and return the result.
  ASSERT(cur < current->limit);
  current->next = cur + 1;

  T** result = reinterpret_cast<T**>(cur);
  *result = value;
  return result;
}


#ifdef DEBUG
inline NoHandleAllocation::NoHandleAllocation() {
  Isolate* isolate = Isolate::Current();
  v8::ImplementationUtilities::HandleScopeData* current =
      isolate->handle_scope_data();

  active_ = !isolate->optimizing_compiler_thread()->IsOptimizerThread();
  if (active_) {
    // Shrink the current handle scope to make it impossible to do
    // handle allocations without an explicit handle scope.
    current->limit = current->next;

    level_ = current->level;
    current->level = 0;
  }
}


inline NoHandleAllocation::~NoHandleAllocation() {
  if (active_) {
    // Restore state in current handle scope to re-enable handle
    // allocations.
    v8::ImplementationUtilities::HandleScopeData* data =
        Isolate::Current()->handle_scope_data();
    ASSERT_EQ(0, data->level);
    data->level = level_;
  }
}
#endif


} }  // namespace v8::internal

#endif  // V8_HANDLES_INL_H_
