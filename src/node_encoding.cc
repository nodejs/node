#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"

#include "simdutf.h"

namespace node {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::CFunction;
using v8::Context;
using v8::FastApiTypedArray;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Value;

// TODO(anonrig): Replace this with encoding when encoding enum is renamed.
namespace encoding_methods {

static void IsAscii(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> input = args[0].As<ArrayBuffer>();
  auto external_resource = static_cast<const char*>(input->Data());
  args.GetReturnValue().Set(
      simdutf::validate_ascii(external_resource, input->ByteLength()));
}

static void IsUtf8(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> input = args[0].As<ArrayBuffer>();
  auto external_resource = static_cast<const char*>(input->Data());
  args.GetReturnValue().Set(
      simdutf::validate_utf8(external_resource, input->ByteLength()));
}

static void CountUtf8(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> input = args[0].As<ArrayBuffer>();
  auto external_resource = static_cast<const char*>(input->Data());
  int count = simdutf::count_utf8(external_resource, input->ByteLength());
  args.GetReturnValue().Set(count);
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  SetMethodNoSideEffect(context, target, "isAscii", IsAscii);
  SetMethodNoSideEffect(context, target, "isUtf8", IsUtf8);
  SetMethodNoSideEffect(context, target, "countUtf8", CountUtf8);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(IsAscii);
  registry->Register(IsUtf8);
  registry->Register(CountUtf8);
}

}  // namespace encoding_methods
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(encoding_methods,
                                    node::encoding_methods::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    encoding_methods, node::encoding_methods::RegisterExternalReferences)