// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_BASE_OBJECT_INL_H_
#define SRC_BASE_OBJECT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

BaseObject::BaseObject(Environment* env, v8::Local<v8::Object> object)
    : persistent_handle_(env->isolate(), object),
      env_(env) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 0);
  object->SetAlignedPointerInInternalField(0, static_cast<void*>(this));
  env_->AddCleanupHook(DeleteMe, static_cast<void*>(this));
}


BaseObject::~BaseObject() {
  env_->RemoveCleanupHook(DeleteMe, static_cast<void*>(this));

  if (persistent_handle_.IsEmpty()) {
    // This most likely happened because the weak callback below cleared it.
    return;
  }

  {
    v8::HandleScope handle_scope(env_->isolate());
    object()->SetAlignedPointerInInternalField(0, nullptr);
  }
}


Persistent<v8::Object>& BaseObject::persistent() {
  return persistent_handle_;
}


v8::Local<v8::Object> BaseObject::object() {
  return PersistentToLocal(env_->isolate(), persistent_handle_);
}


Environment* BaseObject::env() const {
  return env_;
}


BaseObject* BaseObject::FromJSObject(v8::Local<v8::Object> obj) {
  CHECK_GT(obj->InternalFieldCount(), 0);
  return static_cast<BaseObject*>(obj->GetAlignedPointerFromInternalField(0));
}


template <typename T>
T* BaseObject::FromJSObject(v8::Local<v8::Object> object) {
  return static_cast<T*>(FromJSObject(object));
}


void BaseObject::DeleteMe(void* data) {
  BaseObject* self = static_cast<BaseObject*>(data);
  delete self;
}


void BaseObject::MakeWeak() {
  persistent_handle_.SetWeak(
      this,
      [](const v8::WeakCallbackInfo<BaseObject>& data) {
        BaseObject* obj = data.GetParameter();
        // Clear the persistent handle so that ~BaseObject() doesn't attempt
        // to mess with internal fields, since the JS object may have
        // transitioned into an invalid state.
        // Refs: https://github.com/nodejs/node/issues/18897
        obj->persistent_handle_.Reset();
        delete obj;
      }, v8::WeakCallbackType::kParameter);
}


void BaseObject::ClearWeak() {
  persistent_handle_.ClearWeak();
}


v8::Local<v8::FunctionTemplate>
BaseObject::MakeLazilyInitializedJSTemplate(Environment* env) {
  auto constructor = [](const v8::FunctionCallbackInfo<v8::Value>& args) {
#ifdef DEBUG
    CHECK(args.IsConstructCall());
    CHECK_GT(args.This()->InternalFieldCount(), 0);
#endif
    args.This()->SetAlignedPointerInInternalField(0, nullptr);
  };

  v8::Local<v8::FunctionTemplate> t = env->NewFunctionTemplate(constructor);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  return t;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_INL_H_
