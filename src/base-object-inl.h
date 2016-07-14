#ifndef SRC_BASE_OBJECT_INL_H_
#define SRC_BASE_OBJECT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base-object.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

inline BaseObject::BaseObject(Environment* env, v8::Local<v8::Object> handle)
    : persistent_handle_(env->isolate(), handle),
      env_(env) {
  CHECK_EQ(false, handle.IsEmpty());
  // The zero field holds a pointer to the handle. Immediately set it to
  // nullptr in case it's accessed by the user before construction is complete.
  if (handle->InternalFieldCount() > 0)
    handle->SetAlignedPointerInInternalField(0, nullptr);
}


inline BaseObject::~BaseObject() {
  CHECK(persistent_handle_.IsEmpty());
}


inline v8::Persistent<v8::Object>& BaseObject::persistent() {
  return persistent_handle_;
}


inline v8::Local<v8::Object> BaseObject::object() {
  return PersistentToLocal(env_->isolate(), persistent_handle_);
}


inline Environment* BaseObject::env() const {
  return env_;
}


template <typename Type>
inline void BaseObject::WeakCallback(
    const v8::WeakCallbackInfo<Type>& data) {
  Type* self = data.GetParameter();
  self->persistent().Reset();
  delete self;
}


template <typename Type>
inline void BaseObject::MakeWeak(Type* ptr) {
  v8::HandleScope scope(env_->isolate());
  v8::Local<v8::Object> handle = object();
  CHECK_GT(handle->InternalFieldCount(), 0);
  Wrap(handle, ptr);
  persistent_handle_.MarkIndependent();
  persistent_handle_.SetWeak<Type>(ptr, WeakCallback<Type>,
                                   v8::WeakCallbackType::kParameter);
}


inline void BaseObject::ClearWeak() {
  persistent_handle_.ClearWeak();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_INL_H_
