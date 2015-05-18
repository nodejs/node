#ifndef SRC_BASE_OBJECT_INL_H_
#define SRC_BASE_OBJECT_INL_H_

#include "base-object.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

namespace node {

inline BaseObject::BaseObject(Environment* env,
                              v8::Local<v8::Object> handle,
                              const uint16_t class_id)
    : handle_(env->isolate(), handle),
      env_(env) {
  CHECK_EQ(false, handle.IsEmpty());
  // Shift value 8 bits over to try avoiding conflict with anything else.
  if (class_id != 0)
    handle_.SetWrapperClassId(class_id << 8);
}


inline BaseObject::~BaseObject() {
  CHECK(handle_.IsEmpty());
}


inline v8::Persistent<v8::Object>& BaseObject::persistent() {
  return handle_;
}


inline v8::Local<v8::Object> BaseObject::object() {
  return PersistentToLocal(env_->isolate(), handle_);
}


inline Environment* BaseObject::env() const {
  return env_;
}


template <typename Type>
inline void BaseObject::WeakCallback(
    const v8::WeakCallbackData<v8::Object, Type>& data) {
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
  handle_.MarkIndependent();
  handle_.SetWeak<Type>(ptr, WeakCallback<Type>);
}


inline void BaseObject::ClearWeak() {
  handle_.ClearWeak();
}

}  // namespace node

#endif  // SRC_BASE_OBJECT_INL_H_
