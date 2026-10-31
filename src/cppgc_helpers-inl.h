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
  v8::Object::Wrap<T::kCppHeapPointerTag>(isolate, obj, wrappable);
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
  // The isolate is not passed in to keep the signature consistent with
  // BaseObject::Unwrap() and the ASSIGN_OR_RETURN_UNWRAP_CPPGC macro.
  if (obj->InternalFieldCount() != T::kInternalFieldCount) {
    return nullptr;
  }
  return v8::Object::Unwrap<T::kCppHeapPointerTag, T>(v8::Isolate::GetCurrent(),
                                                      obj);
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
