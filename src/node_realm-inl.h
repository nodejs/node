#ifndef SRC_NODE_REALM_INL_H_
#define SRC_NODE_REALM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_realm.h"

namespace node {

inline Realm* Realm::GetCurrent(v8::Isolate* isolate) {
  if (UNLIKELY(!isolate->InContext())) return nullptr;
  v8::HandleScope handle_scope(isolate);
  return GetCurrent(isolate->GetCurrentContext());
}

inline Realm* Realm::GetCurrent(v8::Local<v8::Context> context) {
  if (UNLIKELY(!ContextEmbedderTag::IsNodeContext(context))) return nullptr;
  return static_cast<Realm*>(
      context->GetAlignedPointerFromEmbedderData(ContextEmbedderIndex::kRealm));
}

inline Realm* Realm::GetCurrent(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

template <typename T>
inline Realm* Realm::GetCurrent(const v8::PropertyCallbackInfo<T>& info) {
  return GetCurrent(info.GetIsolate()->GetCurrentContext());
}

inline Environment* Realm::env() const {
  return env_;
}

inline v8::Isolate* Realm::isolate() const {
  return isolate_;
}

inline bool Realm::has_run_bootstrapping_code() const {
  return has_run_bootstrapping_code_;
}

#define V(PropertyName, TypeName)                                              \
  inline v8::Local<TypeName> Realm::PropertyName() const {                     \
    return PersistentToLocal::Strong(PropertyName##_);                         \
  }                                                                            \
  inline void Realm::set_##PropertyName(v8::Local<TypeName> value) {           \
    PropertyName##_.Reset(isolate(), value);                                   \
  }
PER_REALM_STRONG_PERSISTENT_VALUES(V)
#undef V

v8::Local<v8::Context> Realm::context() const {
  return PersistentToLocal::Strong(context_);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_REALM_INL_H_
