#include "env-inl.h"
#include "node.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"
#include "v8-fast-api-calls.h"
#include "v8.h"

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/utf8.h>
#endif  // NODE_HAVE_I18N_SUPPORT

namespace node {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::CFunction;
using v8::Context;
using v8::FastApiTypedArray;
using v8::FastOneByteString;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Value;

namespace encoding_methods {

static void EncodeUtf8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Local<String> str = args[0].As<String>();
  size_t length = str->Utf8Length(isolate);

  Local<ArrayBuffer> ab;
  {
    NoArrayBufferZeroFillScope no_zero_fill_scope(env->isolate_data());
    std::unique_ptr<BackingStore> bs =
        ArrayBuffer::NewBackingStore(isolate, length);

    CHECK(bs);

    str->WriteUtf8(isolate,
                   static_cast<char*>(bs->Data()),
                   -1,  // We are certain that `data` is sufficiently large
                   nullptr,
                   String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);

    ab = ArrayBuffer::New(isolate, std::move(bs));
  }

  args.GetReturnValue().Set(Uint8Array::New(ab, 0, length));
}

static void EncodeIntoUtf8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK_GE(args.Length(), 3);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsUint8Array());
  CHECK(args[2]->IsUint32Array());

  Local<String> source = args[0].As<String>();

  Local<Uint8Array> dest = args[1].As<Uint8Array>();
  Local<ArrayBuffer> buf = dest->Buffer();
  char* write_result = static_cast<char*>(buf->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();

  // results = [ read, written ]
  Local<Uint32Array> result_arr = args[2].As<Uint32Array>();
  uint32_t* results = reinterpret_cast<uint32_t*>(
      static_cast<char*>(result_arr->Buffer()->Data()) +
      result_arr->ByteOffset());

  int nchars;
  int written = source->WriteUtf8(
      isolate,
      write_result,
      dest_length,
      &nchars,
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);
  results[0] = nchars;
  results[1] = written;
}

#if defined(NODE_HAVE_I18N_SUPPORT)
static void FastEncodeIntoUtf8(Local<Value> receiver,
                               const FastOneByteString& source,
                               const FastApiTypedArray<uint8_t>& destination,
                               const FastApiTypedArray<uint32_t>& result) {
  uint8_t* destination_data;
  CHECK(destination.getStorageIfAligned(&destination_data));

  uint32_t* results;
  CHECK(result.getStorageIfAligned(&results));

  size_t source_length = source.length;
  size_t destination_length = destination.length();
  size_t min_length = std::min(source_length, destination_length);

  memcpy(destination_data, source.data, min_length);

  results[0] = min_length;
  results[1] = min_length;
}

CFunction fast_encode_into_utf8_(CFunction::Make(FastEncodeIntoUtf8));
#endif  // NODE_HAVE_I18N_SUPPORT

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  SetMethodNoSideEffect(context, target, "encodeUtf8", EncodeUtf8);
#if defined(NODE_HAVE_I18N_SUPPORT)
  SetFastMethod(context,
                target,
                "encodeIntoUtf8",
                EncodeIntoUtf8,
                &fast_encode_into_utf8_);
#else
  SetMethodNoSideEffect(context, target, "encodeIntoUtf8", EncodeIntoUtf8);
#endif  // NODE_HAVE_I18N_SUPPORT
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(EncodeUtf8);

  registry->Register(EncodeIntoUtf8);

#if defined(NODE_HAVE_I18N_SUPPORT)
  registry->Register(FastEncodeIntoUtf8);
  registry->Register(fast_encode_into_utf8_.GetTypeInfo());
#endif  // NODE_HAVE_I18N_SUPPORT
}

}  // namespace encoding_methods
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(encoding_methods,
                                    node::encoding_methods::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    encoding_methods, node::encoding_methods::RegisterExternalReferences)
