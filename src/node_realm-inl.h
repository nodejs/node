#ifndef SRC_NODE_REALM_INL_H_
#define SRC_NODE_REALM_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "cleanup_queue-inl.h"
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

inline IsolateData* Realm::isolate_data() const {
  return env_->isolate_data();
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

// static
template <typename T, typename U>
inline T* Realm::GetBindingData(const v8::PropertyCallbackInfo<U>& info) {
  return GetBindingData<T>(info.GetIsolate()->GetCurrentContext());
}

// static
template <typename T>
inline T* Realm::GetBindingData(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  return GetBindingData<T>(info.GetIsolate()->GetCurrentContext());
}

// static
template <typename T>
inline T* Realm::GetBindingData(v8::Local<v8::Context> context) {
  BindingDataStore* map =
      static_cast<BindingDataStore*>(context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kBindingDataStoreIndex));
  DCHECK_NOT_NULL(map);
  constexpr size_t binding_index = static_cast<size_t>(T::binding_type_int);
  static_assert(binding_index < std::tuple_size_v<BindingDataStore>);
  auto ptr = (*map)[binding_index];
  if (UNLIKELY(!ptr)) return nullptr;
  T* result = static_cast<T*>(ptr.get());
  DCHECK_NOT_NULL(result);
  DCHECK_EQ(result->realm(), GetCurrent(context));
  return result;
}

template <typename T>
inline T* Realm::AddBindingData(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> target) {
  DCHECK_EQ(GetCurrent(context), this);
  // This won't compile if T is not a BaseObject subclass.
  BaseObjectPtr<T> item = MakeDetachedBaseObject<T>(this, target);
  BindingDataStore* map =
      static_cast<BindingDataStore*>(context->GetAlignedPointerFromEmbedderData(
          ContextEmbedderIndex::kBindingDataStoreIndex));
  DCHECK_NOT_NULL(map);
  constexpr size_t binding_index = static_cast<size_t>(T::binding_type_int);
  static_assert(binding_index < std::tuple_size_v<BindingDataStore>);
  CHECK(!(*map)[binding_index]);  // Should not insert the binding twice.
  (*map)[binding_index] = item;
  DCHECK_EQ(GetBindingData<T>(context), item.get());
  return item.get();
}

inline BindingDataStore* Realm::binding_data_store() {
  return &binding_data_store_;
}

template <typename T>
void Realm::ForEachBaseObject(T&& iterator) const {
  cleanup_queue_.ForEachBaseObject(std::forward<T>(iterator));
}

void Realm::modify_base_object_count(int64_t delta) {
  base_object_count_ += delta;
}

int64_t Realm::base_object_created_after_bootstrap() const {
  return base_object_count_ - base_object_created_by_bootstrap_;
}

int64_t Realm::base_object_count() const {
  return base_object_count_;
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

void Realm::AddCleanupHook(CleanupQueue::Callback fn, void* arg) {
  cleanup_queue_.Add(fn, arg);
}

void Realm::RemoveCleanupHook(CleanupQueue::Callback fn, void* arg) {
  cleanup_queue_.Remove(fn, arg);
}

bool Realm::HasCleanupHooks() const {
  return !cleanup_queue_.empty();
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_REALM_INL_H_
