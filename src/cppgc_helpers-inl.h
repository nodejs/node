#ifndef SRC_CPPGC_HELPERS_INL_H_
#define SRC_CPPGC_HELPERS_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "cppgc_helpers.h"
#include "env-inl.h"

namespace node {

template <typename T>
void CppgcMixin::Wrap(T* ptr, Realm* realm, v8::Local<v8::Object> obj) {
  CHECK_GE(obj->InternalFieldCount(), T::kInternalFieldCount);
  v8::Isolate* isolate = realm->isolate();
  ptr->traced_reference_ = v8::TracedReference<v8::Object>(isolate, obj);
  // Note that ptr must be of concrete type T in Wrap.
  auto* wrappable = static_cast<v8::Object::Wrappable*>(ptr);
  v8::Object::Wrap<v8::CppHeapPointerTag::kDefaultTag>(isolate, obj, wrappable);
  // Keep the layout consistent with BaseObjects.
  obj->SetAlignedPointerInInternalField(
      kEmbedderType,
      realm->isolate_data()->embedder_id_for_cppgc(),
      EmbedderDataTag::kEmbedderType);
  obj->SetAlignedPointerInInternalField(kSlot, ptr, EmbedderDataTag::kDefault);
  ptr->list_node_ = realm->TrackCppgcWrapper(ptr);
}

template <typename T>
void CppgcMixin::Wrap(T* ptr, Environment* env, v8::Local<v8::Object> obj) {
  Wrap(ptr, env->principal_realm(), obj);
}

template <typename T>
T* CppgcMixin::Unwrap(v8::Local<v8::Object> obj) {
  // We are not using v8::Object::Unwrap currently because that requires
  // access to isolate which the ASSIGN_OR_RETURN_UNWRAP macro that we'll shim
  // with ASSIGN_OR_RETURN_UNWRAP_GC doesn't take, and we also want a
  // signature consistent with BaseObject::Unwrap() to avoid churn. Since
  // cppgc-managed objects share the same layout as BaseObjects, just unwrap
  // from the pointer in the internal field, which should be valid as long as
  // the object is still alive.
  if (obj->InternalFieldCount() != T::kInternalFieldCount) {
    return nullptr;
  }
  T* ptr = static_cast<T*>(obj->GetAlignedPointerFromInternalField(
      T::kSlot, EmbedderDataTag::kDefault));
  return ptr;
}

v8::Local<v8::Object> CppgcMixin::object() const {
  return traced_reference_.Get(realm()->isolate());
}

Environment* CppgcMixin::env() const {
  return realm()->env();
}

Realm* CppgcMixin::realm() const {
  return list_node_ == nullptr ? nullptr : list_node_->realm;
}

void CppgcMixin::Finalize() {
  Realm* current_realm = realm();
  if (current_realm == nullptr) return;
  this->Clean(current_realm);
  list_node_->realm = nullptr;
}

CppgcMixin::~CppgcMixin() {
  delete list_node_;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_CPPGC_HELPERS_INL_H_
