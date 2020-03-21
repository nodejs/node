#ifndef SRC_SNAPSHOT_SUPPORT_INL_H_
#define SRC_SNAPSHOT_SUPPORT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "snapshot_support.h"

namespace node {

const std::vector<SnapshotDataBase::Error>& SnapshotDataBase::errors() const {
  return state_.errors;
}

std::vector<uint8_t> SnapshotDataBase::storage() {
  return storage_;
}

SnapshotDataBase::SnapshotDataBase(std::vector<uint8_t>&& storage)
  : storage_(storage) {}


template <typename T>
void SnapshotCreateData::WriteContextIndependentObject(v8::Local<T> data) {
  WriteTag(kContextIndependentObjectTag);
  WriteIndex(data.IsEmpty() ? kEmptyIndex : creator()->AddData(data));
}

template <typename T>
void SnapshotCreateData::WriteObject(
    v8::Local<v8::Context> context, v8::Local<T> data) {
  WriteTag(kObjectTag);
  WriteIndex(data.IsEmpty() ? kEmptyIndex : creator()->AddData(context, data));
}

template <typename T>
void SnapshotCreateData::WriteBaseObjectPtr(
    v8::Local<v8::Context> context, BaseObjectPtrImpl<T, false> ptr) {
  WriteObject(context, ptr ? ptr->object() : v8::Local<v8::Object>());
}

v8::Isolate* SnapshotCreateData::isolate() {
  return creator()->GetIsolate();
}

v8::SnapshotCreator* SnapshotCreateData::creator() {
  return creator_;
}

SnapshotCreateData::SnapshotCreateData(v8::SnapshotCreator* creator)
  : creator_(creator) {}

template <typename T>
v8::Maybe<v8::Local<T>> SnapshotReadData::ReadContextIndependentObject(
    EmptyHandleMode mode) {
  if (!ReadTag(kContextIndependentObjectTag))
    return v8::Nothing<v8::Local<T>>();
  V8SnapshotIndex index;
  if (!ReadIndex().To(&index)) return v8::Nothing<v8::Local<T>>();
  if (index == kEmptyIndex) {
    if (mode == kAllowEmpty) return v8::Just(v8::Local<T>());
    add_error("Empty handle in serialized data was rejected");
    return v8::Nothing<v8::Local<T>>();
  }
  v8::MaybeLocal<T> ret = isolate()->GetDataFromSnapshotOnce<T>(index);
  if (ret.IsEmpty()) {
    add_error("Could not get context-independent object from snapshot");
    return v8::Nothing<v8::Local<T>>();
  }
  return v8::Just(ret.ToLocalChecked());
}

template <typename T>
v8::Maybe<v8::Local<T>> SnapshotReadData::ReadObject(
    v8::Local<v8::Context> context, EmptyHandleMode mode) {
  if (!ReadTag(kObjectTag)) return v8::Nothing<v8::Local<T>>();
  V8SnapshotIndex index;
  if (!ReadIndex().To(&index)) return v8::Nothing<v8::Local<T>>();
  if (index == kEmptyIndex) {
    if (mode == kAllowEmpty) return v8::Just(v8::Local<T>());
    add_error("Empty handle in serialized data was rejected");
    return v8::Nothing<v8::Local<T>>();
  }
  v8::MaybeLocal<T> ret = context->GetDataFromSnapshotOnce<T>(index);
  if (ret.IsEmpty()) {
    add_error("Could not get context-dependent object from snapshot");
    return v8::Nothing<v8::Local<T>>();
  }
  return v8::Just(ret.ToLocalChecked());
}

template <typename T>
v8::Maybe<BaseObjectPtrImpl<T, false>> SnapshotReadData::ReadBaseObjectPtr(
    v8::Local<v8::Context> context, EmptyHandleMode mode) {
  v8::Local<v8::Object> obj;
  if (!ReadObject<v8::Object>(context, mode).To(&obj)) {
    return v8::Nothing<BaseObjectPtrImpl<T, false>>();
  }
  if (obj.IsEmpty()) return v8::Just(BaseObjectPtrImpl<T, false>());
  BaseObject* base_obj;
  if (!GetBaseObjectFromV8Object(context, obj).To(&base_obj)) {
    return v8::Nothing<BaseObjectPtrImpl<T, false>>();
  }
  return v8::Just(BaseObjectPtrImpl<T, false>(base_obj));
}

v8::Isolate* SnapshotReadData::isolate() {
  return isolate_;
}

void SnapshotReadData::set_isolate(v8::Isolate* isolate) {
  isolate_ = isolate;
}

SnapshotReadData::SnapshotReadData(std::vector<uint8_t>&& storage)
  : SnapshotDataBase(std::move(storage)) {}

template <typename... Args>
ExternalReferences::ExternalReferences(const char* id, Args*... args) {
  Register(id, this);
  HandleArgs(args...);
}

void ExternalReferences::HandleArgs() {}

template <typename T, typename... Args>
void ExternalReferences::HandleArgs(T* ptr, Args*... args) {
  AddPointer(reinterpret_cast<intptr_t>(ptr));
  HandleArgs(args...);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_SNAPSHOT_SUPPORT_INL_H_
