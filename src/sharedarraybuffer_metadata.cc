#include "sharedarraybuffer_metadata.h"

#include "base_object-inl.h"
#include "memory_tracker-inl.h"
#include "node_errors.h"
#include "node_worker.h"
#include "util-inl.h"

#include <utility>

using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::SharedArrayBuffer;
using v8::Value;

namespace node {
namespace worker {

namespace {

// Yield a JS constructor for SABLifetimePartner objects in the form of a
// standard API object, that has a single field for containing the raw
// SABLifetimePartner* pointer.
Local<Function> GetSABLifetimePartnerConstructor(
    Environment* env, Local<Context> context) {
  Local<FunctionTemplate> templ;
  templ = env->sab_lifetimepartner_constructor_template();
  if (!templ.IsEmpty())
    return templ->GetFunction(context).ToLocalChecked();

  templ = BaseObject::MakeLazilyInitializedJSTemplate(env);
  templ->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(),
                                            "SABLifetimePartner"));
  env->set_sab_lifetimepartner_constructor_template(templ);

  return GetSABLifetimePartnerConstructor(env, context);
}

class SABLifetimePartner : public BaseObject {
 public:
  SABLifetimePartner(Environment* env,
                     Local<Object> obj,
                     SharedArrayBufferMetadataReference r)
    : BaseObject(env, obj),
      reference(std::move(r)) {
    MakeWeak();
    env->AddCleanupHook(CleanupHook, static_cast<void*>(this));
  }

  ~SABLifetimePartner() {
    env()->RemoveCleanupHook(CleanupHook, static_cast<void*>(this));
  }

  static void CleanupHook(void* data) {
    // There is another cleanup hook attached to this object because it is a
    // BaseObject. Cleanup hooks are triggered in reverse order of addition,
    // so if this object is destroyed through GC, the destructor removes all
    // hooks associated with this object, meaning that this cleanup hook
    // only runs at the end of the Environment’s lifetime.
    // In that case, V8 still knows about the SharedArrayBuffer and tries to
    // free it when the last Isolate with access to it is disposed; for that,
    // the ArrayBuffer::Allocator needs to be kept alive longer than this
    // object and longer than the Environment instance.
    //
    // This is a workaround for https://github.com/nodejs/node-v8/issues/115
    // (introduced in V8 7.9) and we should be able to remove it once V8
    // ArrayBuffer::Allocator refactoring/removal is complete.
    SABLifetimePartner* self = static_cast<SABLifetimePartner*>(data);
    self->env()->AddArrayBufferAllocatorToKeepAliveUntilIsolateDispose(
        self->reference->allocator());
  }

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(SABLifetimePartner)
  SET_SELF_SIZE(SABLifetimePartner)

  SharedArrayBufferMetadataReference reference;
};

}  // anonymous namespace

SharedArrayBufferMetadataReference
SharedArrayBufferMetadata::ForSharedArrayBuffer(
    Environment* env,
    Local<Context> context,
    Local<SharedArrayBuffer> source) {
  Local<Value> lifetime_partner;

  if (!source->GetPrivate(context,
                          env->sab_lifetimepartner_symbol())
                              .ToLocal(&lifetime_partner)) {
    return nullptr;
  }

  if (lifetime_partner->IsObject() &&
      env->sab_lifetimepartner_constructor_template()
         ->HasInstance(lifetime_partner)) {
    CHECK(source->IsExternal());
    SABLifetimePartner* partner =
        Unwrap<SABLifetimePartner>(lifetime_partner.As<Object>());
    CHECK_NOT_NULL(partner);
    return partner->reference;
  }

  if (source->IsExternal()) {
    // If this is an external SharedArrayBuffer but we do not see a lifetime
    // partner object, it was not us who externalized it. In that case, there
    // is no way to serialize it, because it's unclear how the memory
    // is actually owned.
    THROW_ERR_TRANSFERRING_EXTERNALIZED_SHAREDARRAYBUFFER(env);
    return nullptr;
  }

  // If the SharedArrayBuffer is coming from a Worker, we need to make sure
  // that the corresponding ArrayBuffer::Allocator lives at least as long as
  // the SharedArrayBuffer itself.
  worker::Worker* w = env->worker_context();
  std::shared_ptr<v8::ArrayBuffer::Allocator> allocator =
      w != nullptr ? w->array_buffer_allocator() : nullptr;

  SharedArrayBuffer::Contents contents = source->Externalize();
  SharedArrayBufferMetadataReference r(
      new SharedArrayBufferMetadata(contents, allocator));
  if (r->AssignToSharedArrayBuffer(env, context, source).IsNothing())
    return nullptr;
  return r;
}

Maybe<bool> SharedArrayBufferMetadata::AssignToSharedArrayBuffer(
    Environment* env, Local<Context> context,
    Local<SharedArrayBuffer> target) {
  CHECK(target->IsExternal());
  Local<Function> ctor = GetSABLifetimePartnerConstructor(env, context);
  Local<Object> obj;
  if (!ctor->NewInstance(context).ToLocal(&obj))
    return Nothing<bool>();

  new SABLifetimePartner(env, obj, shared_from_this());
  return target->SetPrivate(context,
                            env->sab_lifetimepartner_symbol(),
                            obj);
}

SharedArrayBufferMetadata::SharedArrayBufferMetadata(
    const SharedArrayBuffer::Contents& contents,
    std::shared_ptr<v8::ArrayBuffer::Allocator> allocator)
  : contents_(contents), allocator_(allocator) { }

SharedArrayBufferMetadata::~SharedArrayBufferMetadata() {
  contents_.Deleter()(contents_.Data(),
                      contents_.ByteLength(),
                      contents_.DeleterData());
}

MaybeLocal<SharedArrayBuffer> SharedArrayBufferMetadata::GetSharedArrayBuffer(
    Environment* env, Local<Context> context) {
  Local<SharedArrayBuffer> obj =
      SharedArrayBuffer::New(env->isolate(),
                             contents_.Data(),
                             contents_.ByteLength());

  if (AssignToSharedArrayBuffer(env, context, obj).IsNothing())
    return MaybeLocal<SharedArrayBuffer>();

  return obj;
}

}  // namespace worker
}  // namespace node
