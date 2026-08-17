#include "env-inl.h"
#include "node.h"
#include "node_debug.h"
#include "node_errors.h"
#include "node_external_reference.h"

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BackingStoreInitializationMode;
using v8::BackingStoreOnFailureMode;
using v8::CFunction;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Uint8Array;
using v8::Value;

namespace node {
namespace webstreams {

// True when `value` cannot be a thenable: null, undefined, or a
// non-object non-function primitive. Objects and functions are treated
// as maybe-thenable without looking up `.then` (that lookup is
// observable). Proxies of objects/functions take the maybe-thenable
// path; a Proxy around a primitive is still an object.
static bool IsNonThenableValue(Local<Value> value) {
  return value->IsNullOrUndefined() ||
         (!value->IsObject() && !value->IsFunction());
}

static void IsNonThenable(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(IsNonThenableValue(args[0]));
}

static bool FastIsNonThenable(Local<Value> unused, Local<Value> value) {
  TRACK_V8_FAST_API_CALL("webstreams.isNonThenable");
  return IsNonThenableValue(value);
}

static CFunction fast_is_non_thenable(CFunction::Make(FastIsNonThenable));

// Clone an ArrayBufferView into a fresh Uint8Array. Used by the
// byte-stream / tee paths in place of ArrayBuffer.prototype.slice +
// `new Uint8Array`, so the copy is a single memcpy.
static void CloneAsUint8Array(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  if (!args[0]->IsArrayBufferView()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env, "The \"view\" argument must be an ArrayBufferView");
    return;
  }

  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> source = view->Buffer();
  if (source->WasDetached()) {
    THROW_ERR_INVALID_STATE(env, "Cannot clone a detached ArrayBuffer");
    return;
  }

  const size_t byte_length = view->ByteLength();
  std::unique_ptr<BackingStore> store = ArrayBuffer::NewBackingStore(
      isolate,
      byte_length,
      BackingStoreInitializationMode::kUninitialized,
      BackingStoreOnFailureMode::kReturnNull);
  if (!store) {
    THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
    return;
  }

  if (byte_length > 0) {
    view->CopyContents(store->Data(), byte_length);
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(store));
  args.GetReturnValue().Set(Uint8Array::New(ab, 0, byte_length));
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  SetFastMethodNoSideEffect(
      context, target, "isNonThenable", IsNonThenable, &fast_is_non_thenable);
  SetMethod(context, target, "cloneAsUint8Array", CloneAsUint8Array);
}

static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsNonThenable);
  registry->Register(fast_is_non_thenable);
  registry->Register(CloneAsUint8Array);
}

}  // namespace webstreams
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(webstreams, node::webstreams::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(webstreams,
                                node::webstreams::RegisterExternalReferences)
