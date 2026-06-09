#include "streams/streams_binding.h"
#include "streams/readable_stream.h"
#include "streams/writable_stream.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_external_reference.h"
#include "node_realm-inl.h"
#include "node_snapshotable.h"
#include "util.h"
#include "v8.h"

namespace node {

using v8::Context;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::SnapshotCreator;
using v8::Value;

namespace webstreams {

BindingData::BindingData(Realm* realm, Local<Object> object)
    : SnapshotableObject(realm, object, type_int) {}

BindingData* BindingData::Get(Environment* env) {
  return Realm::GetCurrent(env->context())->GetBindingData<BindingData>();
}

void BindingData::MemoryInfo(MemoryTracker* tracker) const {}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          SnapshotCreator* creator) {
  // Return true to keep the binding reachable from JS land across the snapshot.
  // No native state needs to be persisted yet.
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  return info;
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  Realm* realm = Realm::GetCurrent(context);
  BindingData* binding = realm->AddBindingData<BindingData>(holder);
  CHECK_NOT_NULL(binding);
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<ObjectTemplate> target) {
  webstreams::InitializeReadableStream(isolate_data->isolate(), target);
  webstreams::InitializeWritableStream(isolate_data->isolate(), target);
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(target);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  webstreams::RegisterReadableStreamExternalReferences(registry);
  webstreams::RegisterWritableStreamExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    webstreams, node::webstreams::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    webstreams, node::webstreams::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    webstreams, node::webstreams::BindingData::RegisterExternalReferences)
