#include "streams/streams_binding.h"
#include "streams/readable_stream.h"
#include "streams/writable_stream.h"
#include "streams/transform_stream.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
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

using v8::FunctionCallback;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::NewStringType;
using v8::Promise;
using v8::Signature;
using v8::String;

namespace {
Local<String> InternalizedString(Isolate* isolate, const char* s) {
  return String::NewFromUtf8(isolate, s, NewStringType::kInternalized)
      .ToLocalChecked();
}

// Shared start-fulfilled reaction: receives the controller wrapper as the
// promise's fulfilment value and dispatches by kind tag. One function per
// realm replaces two per-creation Function allocations on the common path.
void ReactStartFulfilledShared(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (!args[0]->IsObject()) return;
  BaseObject* base =
      BaseObject::FromJSObject(args[0].As<v8::Object>());
  if (base == nullptr) return;
  auto* s = static_cast<StreamBaseObject*>(base);
  switch (s->stream_kind()) {
    case StreamBaseObject::Kind::kDefaultController:
      static_cast<ReadableStreamDefaultController*>(s)->OnStartFulfilled();
      break;
    case StreamBaseObject::Kind::kByteController:
      static_cast<ReadableByteStreamController*>(s)->OnStartFulfilled();
      break;
    case StreamBaseObject::Kind::kWritableStreamDefaultController:
      static_cast<WritableStreamDefaultController*>(s)->OnStartFulfilled();
      break;
    default:
      break;
  }
}

void ThenSharedReaction(Environment* env,
                        Local<Promise> promise,
                        v8::FunctionCallback cb,
                        v8::Global<v8::Function>* slot) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Local<v8::Function> fn = slot->Get(isolate);
  if (fn.IsEmpty()) {
    if (!v8::Function::New(context, cb, Local<Value>(), 0,
                           v8::ConstructorBehavior::kThrow)
             .ToLocal(&fn)) {
      return;
    }
    slot->Reset(isolate, fn);
  }
  USE(promise->Then(context, fn));
}
}  // namespace

namespace {
void NoopCallback(const v8::FunctionCallbackInfo<Value>& args) {}
}  // namespace

Local<v8::Function> NoopFunction(Environment* env) {
  BindingData* bd = BindingData::Get(env);
  Isolate* isolate = env->isolate();
  Local<v8::Function> fn = bd->noop_function.Get(isolate);
  if (fn.IsEmpty()) {
    if (!v8::Function::New(env->context(), NoopCallback, Local<Value>(), 0,
                           v8::ConstructorBehavior::kThrow)
             .ToLocal(&fn)) {
      return Local<v8::Function>();
    }
    bd->noop_function.Reset(isolate, fn);
  }
  return fn;
}

void ThenStartFulfilled(Environment* env, Local<Promise> promise) {
  BindingData* bd = BindingData::Get(env);
  ThenSharedReaction(env, promise, ReactStartFulfilledShared,
                     &bd->start_fulfilled_reaction);
}

Local<FunctionTemplate> NewGetter(Isolate* isolate,
                                  const char* prop,
                                  FunctionCallback cb) {
  Local<FunctionTemplate> t =
      FunctionTemplate::New(isolate, cb, Local<Value>(), Local<Signature>(), 0,
                            v8::ConstructorBehavior::kThrow,
                            v8::SideEffectType::kHasNoSideEffect);
  std::string name = std::string("get ") + prop;
  t->SetClassName(InternalizedString(isolate, name.c_str()));
  return t;
}

Local<FunctionTemplate> NewPromiseGetter(Isolate* isolate,
                                         const char* prop,
                                         FunctionCallback cb) {
  Local<FunctionTemplate> t =
      FunctionTemplate::New(isolate, cb, Local<Value>(), Local<Signature>(), 0,
                            v8::ConstructorBehavior::kThrow,
                            v8::SideEffectType::kHasNoSideEffect);
  std::string name = std::string("get ") + prop;
  t->SetClassName(InternalizedString(isolate, name.c_str()));
  return t;
}

void SetProtoMethodLen(Isolate* isolate,
                       Local<FunctionTemplate> tmpl,
                       const char* name,
                       FunctionCallback cb,
                       int length) {
  Local<FunctionTemplate> t =
      FunctionTemplate::New(isolate, cb, Local<Value>(), Local<Signature>(),
                            length, v8::ConstructorBehavior::kThrow,
                            v8::SideEffectType::kHasSideEffect);
  Local<String> name_string = InternalizedString(isolate, name);
  tmpl->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);
}

void SetProtoMethodPromise(Isolate* isolate,
                           Local<FunctionTemplate> tmpl,
                           const char* name,
                           FunctionCallback cb,
                           int length) {
  // No receiver signature: a Promise-returning operation must reject (not throw)
  // when invoked on a foreign receiver, so the callback must run regardless.
  Local<FunctionTemplate> t =
      FunctionTemplate::New(isolate, cb, Local<Value>(), Local<Signature>(),
                            length, v8::ConstructorBehavior::kThrow,
                            v8::SideEffectType::kHasSideEffect);
  Local<String> name_string = InternalizedString(isolate, name);
  tmpl->PrototypeTemplate()->Set(name_string, t);
  t->SetClassName(name_string);
}

Local<Value> IllegalInvocationRejection(Local<Context> context) {
  Isolate* isolate = Isolate::GetCurrent();
  Local<Value> error =
      ERR_INVALID_THIS(isolate, "Value of \"this\" is the wrong type");
  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(context).ToLocal(&resolver)) {
    return Local<Value>();
  }
  USE(resolver->Reject(context, error));
  return resolver->GetPromise();
}

bool CheckReceiverInvalidThis(Environment* env,
                              Local<FunctionTemplate> ctor,
                              Local<Value> receiver,
                              const char* type_name) {
  if (ctor->HasInstance(receiver)) return true;
  THROW_ERR_INVALID_THIS(
      env, "Value of \"this\" must be of type %s", type_name);
  return false;
}

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
  webstreams::InitializeTransformStream(isolate_data->isolate(), target);
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(target);
  Environment* env = realm->env();
  webstreams::ExposeReadableStreamConstructors(env, target);
  webstreams::ExposeWritableStreamConstructors(env, target);
  webstreams::ExposeTransformStreamConstructors(env, target);
}

void BindingData::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  webstreams::RegisterReadableStreamExternalReferences(registry);
  webstreams::RegisterWritableStreamExternalReferences(registry);
  webstreams::RegisterTransformStreamExternalReferences(registry);
}

}  // namespace webstreams
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    webstreams, node::webstreams::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    webstreams, node::webstreams::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    webstreams, node::webstreams::BindingData::RegisterExternalReferences)
