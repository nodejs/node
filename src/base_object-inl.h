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
#include "util.h"

#if (__GNUC__ >= 8) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
#include "v8.h"
#if (__GNUC__ >= 8) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace node {

BaseObject::BaseObject(Environment* env, v8::Local<v8::Object> object)
    : persistent_handle_(env->isolate(), object), env_(env) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 0);
  object->SetAlignedPointerInInternalField(
      BaseObject::kSlot,
      static_cast<void*>(this));
  env->AddCleanupHook(DeleteMe, static_cast<void*>(this));
  env->modify_base_object_count(1);
}

BaseObject::~BaseObject() {
  env()->modify_base_object_count(-1);
  env()->RemoveCleanupHook(DeleteMe, static_cast<void*>(this));

  if (UNLIKELY(has_pointer_data())) {
    PointerData* metadata = pointer_data();
    CHECK_EQ(metadata->strong_ptr_count, 0);
    metadata->self = nullptr;
    if (metadata->weak_ptr_count == 0)
      delete metadata;
  }

  if (persistent_handle_.IsEmpty()) {
    // This most likely happened because the weak callback below cleared it.
    return;
  }

  {
    v8::HandleScope handle_scope(env()->isolate());
    object()->SetAlignedPointerInInternalField(BaseObject::kSlot, nullptr);
  }
}

void BaseObject::Detach() {
  CHECK_GT(pointer_data()->strong_ptr_count, 0);
  pointer_data()->is_detached = true;
}

v8::Global<v8::Object>& BaseObject::persistent() {
  return persistent_handle_;
}


v8::Local<v8::Object> BaseObject::object() const {
  return PersistentToLocal::Default(env()->isolate(), persistent_handle_);
}

v8::Local<v8::Object> BaseObject::object(v8::Isolate* isolate) const {
  v8::Local<v8::Object> handle = object();

  DCHECK_EQ(handle->CreationContext()->GetIsolate(), isolate);
  DCHECK_EQ(env()->isolate(), isolate);

  return handle;
}

Environment* BaseObject::env() const {
  return env_;
}

BaseObject* BaseObject::FromJSObject(v8::Local<v8::Object> obj) {
  CHECK_GT(obj->InternalFieldCount(), 0);
  return static_cast<BaseObject*>(
      obj->GetAlignedPointerFromInternalField(BaseObject::kSlot));
}


template <typename T>
T* BaseObject::FromJSObject(v8::Local<v8::Object> object) {
  return static_cast<T*>(FromJSObject(object));
}


void BaseObject::MakeWeak() {
  if (has_pointer_data()) {
    pointer_data()->wants_weak_jsobj = true;
    if (pointer_data()->strong_ptr_count > 0) return;
  }

  persistent_handle_.SetWeak(
      this,
      [](const v8::WeakCallbackInfo<BaseObject>& data) {
        BaseObject* obj = data.GetParameter();
        // Clear the persistent handle so that ~BaseObject() doesn't attempt
        // to mess with internal fields, since the JS object may have
        // transitioned into an invalid state.
        // Refs: https://github.com/nodejs/node/issues/18897
        obj->persistent_handle_.Reset();
        CHECK_IMPLIES(obj->has_pointer_data(),
                      obj->pointer_data()->strong_ptr_count == 0);
        obj->OnGCCollect();
      }, v8::WeakCallbackType::kParameter);
}

void BaseObject::OnGCCollect() {
  delete this;
}

void BaseObject::ClearWeak() {
  if (has_pointer_data())
    pointer_data()->wants_weak_jsobj = false;

  persistent_handle_.ClearWeak();
}


v8::Local<v8::FunctionTemplate>
BaseObject::MakeLazilyInitializedJSTemplate(Environment* env) {
  auto constructor = [](const v8::FunctionCallbackInfo<v8::Value>& args) {
    DCHECK(args.IsConstructCall());
    DCHECK_GT(args.This()->InternalFieldCount(), 0);
    args.This()->SetAlignedPointerInInternalField(BaseObject::kSlot, nullptr);
  };

  v8::Local<v8::FunctionTemplate> t = env->NewFunctionTemplate(constructor);
  t->InstanceTemplate()->SetInternalFieldCount(
      BaseObject::kInternalFieldCount);
  return t;
}

template <int Field>
void BaseObject::InternalFieldGet(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(info.This()->GetInternalField(Field));
}

template <int Field, bool (v8::Value::* typecheck)() const>
void BaseObject::InternalFieldSet(v8::Local<v8::String> property,
                                  v8::Local<v8::Value> value,
                                  const v8::PropertyCallbackInfo<void>& info) {
  // This could be e.g. value->IsFunction().
  CHECK(((*value)->*typecheck)());
  info.This()->SetInternalField(Field, value);
}

bool BaseObject::has_pointer_data() const {
  return pointer_data_ != nullptr;
}

BaseObject::PointerData* BaseObject::pointer_data() {
  if (!has_pointer_data()) {
    PointerData* metadata = new PointerData();
    metadata->wants_weak_jsobj = persistent_handle_.IsWeak();
    metadata->self = this;
    pointer_data_ = metadata;
  }
  CHECK(has_pointer_data());
  return pointer_data_;
}

void BaseObject::decrease_refcount() {
  CHECK(has_pointer_data());
  PointerData* metadata = pointer_data();
  CHECK_GT(metadata->strong_ptr_count, 0);
  unsigned int new_refcount = --metadata->strong_ptr_count;
  if (new_refcount == 0) {
    if (metadata->is_detached) {
      delete this;
    } else if (metadata->wants_weak_jsobj && !persistent_handle_.IsEmpty()) {
      MakeWeak();
    }
  }
}

void BaseObject::increase_refcount() {
  unsigned int prev_refcount = pointer_data()->strong_ptr_count++;
  if (prev_refcount == 0 && !persistent_handle_.IsEmpty())
    persistent_handle_.ClearWeak();
}

template <typename T, bool kIsWeak>
BaseObject::PointerData*
BaseObjectPtrImpl<T, kIsWeak>::pointer_data() const {
  if (kIsWeak) {
    return data_.pointer_data;
  }
  if (get_base_object() == nullptr) {
    return nullptr;
  }
  return get_base_object()->pointer_data();
}

template <typename T, bool kIsWeak>
BaseObject* BaseObjectPtrImpl<T, kIsWeak>::get_base_object() const {
  if (kIsWeak) {
    if (pointer_data() == nullptr) {
      return nullptr;
    }
    return pointer_data()->self;
  }
  return data_.target;
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::~BaseObjectPtrImpl() {
  if (kIsWeak) {
    if (pointer_data() != nullptr &&
        --pointer_data()->weak_ptr_count == 0 &&
        pointer_data()->self == nullptr) {
      delete pointer_data();
    }
  } else if (get() != nullptr) {
    get()->decrease_refcount();
  }
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::BaseObjectPtrImpl() {
  data_.target = nullptr;
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::BaseObjectPtrImpl(T* target)
  : BaseObjectPtrImpl() {
  if (target == nullptr) return;
  if (kIsWeak) {
    data_.pointer_data = target->pointer_data();
    CHECK_NOT_NULL(pointer_data());
    pointer_data()->weak_ptr_count++;
  } else {
    data_.target = target;
    CHECK_NOT_NULL(pointer_data());
    get()->increase_refcount();
  }
}

template <typename T, bool kIsWeak>
template <typename U, bool kW>
BaseObjectPtrImpl<T, kIsWeak>::BaseObjectPtrImpl(
    const BaseObjectPtrImpl<U, kW>& other)
  : BaseObjectPtrImpl(other.get()) {}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::BaseObjectPtrImpl(const BaseObjectPtrImpl& other)
  : BaseObjectPtrImpl(other.get()) {}

template <typename T, bool kIsWeak>
template <typename U, bool kW>
BaseObjectPtrImpl<T, kIsWeak>& BaseObjectPtrImpl<T, kIsWeak>::operator=(
    const BaseObjectPtrImpl<U, kW>& other) {
  if (other.get() == get()) return *this;
  this->~BaseObjectPtrImpl();
  return *new (this) BaseObjectPtrImpl(other);
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>& BaseObjectPtrImpl<T, kIsWeak>::operator=(
    const BaseObjectPtrImpl& other) {
  if (other.get() == get()) return *this;
  this->~BaseObjectPtrImpl();
  return *new (this) BaseObjectPtrImpl(other);
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::BaseObjectPtrImpl(BaseObjectPtrImpl&& other)
  : data_(other.data_) {
  if (kIsWeak)
    other.data_.target = nullptr;
  else
    other.data_.pointer_data = nullptr;
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>& BaseObjectPtrImpl<T, kIsWeak>::operator=(
    BaseObjectPtrImpl&& other) {
  if (&other == this) return *this;
  this->~BaseObjectPtrImpl();
  return *new (this) BaseObjectPtrImpl(std::move(other));
}

template <typename T, bool kIsWeak>
void BaseObjectPtrImpl<T, kIsWeak>::reset(T* ptr) {
  *this = BaseObjectPtrImpl(ptr);
}

template <typename T, bool kIsWeak>
T* BaseObjectPtrImpl<T, kIsWeak>::get() const {
  return static_cast<T*>(get_base_object());
}

template <typename T, bool kIsWeak>
T& BaseObjectPtrImpl<T, kIsWeak>::operator*() const {
  return *get();
}

template <typename T, bool kIsWeak>
T* BaseObjectPtrImpl<T, kIsWeak>::operator->() const {
  return get();
}

template <typename T, bool kIsWeak>
BaseObjectPtrImpl<T, kIsWeak>::operator bool() const {
  return get() != nullptr;
}

template <typename T, typename... Args>
BaseObjectPtr<T> MakeBaseObject(Args&&... args) {
  return BaseObjectPtr<T>(new T(std::forward<Args>(args)...));
}

template <typename T, typename... Args>
BaseObjectPtr<T> MakeDetachedBaseObject(Args&&... args) {
  BaseObjectPtr<T> target = MakeBaseObject<T>(std::forward<Args>(args)...);
  target->Detach();
  return target;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_INL_H_
