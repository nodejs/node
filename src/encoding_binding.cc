#include "encoding_binding.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "simdutf.h"
#include "string_bytes.h"
#include "v8.h"

#include <cstdint>

namespace node {
namespace encoding_binding {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("encode_into_results_buffer",
                      encode_into_results_buffer_);
}

BindingData::BindingData(Realm* realm, v8::Local<v8::Object> object)
    : SnapshotableObject(realm, object, type_int),
      encode_into_results_buffer_(realm->isolate(), kEncodeIntoResultsLength) {
  object
      ->Set(realm->context(),
            FIXED_ONE_BYTE_STRING(realm->isolate(), "encodeIntoResults"),
            encode_into_results_buffer_.GetJSArray())
      .Check();
}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          v8::SnapshotCreator* creator) {
  // We'll just re-initialize the buffers in the constructor since their
  // contents can be thrown away once consumed in the previous call.
  encode_into_results_buffer_.Release();
  // Return true because we need to maintain the reference to the binding from
  // JS land.
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  InternalFieldInfo* info =
      InternalFieldInfoBase::New<InternalFieldInfo>(type());
  return info;
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_EQ(index, BaseObject::kEmbedderType);
  v8::HandleScope scope(context->GetIsolate());
  Realm* realm = Realm::GetCurrent(context);
  // Recreate the buffer in the constructor.
  BindingData* binding = realm->AddBindingData<BindingData>(context, holder);
  CHECK_NOT_NULL(binding);
}

void BindingData::EncodeInto(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsUint8Array());
  BindingData* binding_data = Realm::GetBindingData<BindingData>(args);

  Local<String> source = args[0].As<String>();

  Local<Uint8Array> dest = args[1].As<Uint8Array>();
  Local<ArrayBuffer> buf = dest->Buffer();
  char* write_result = static_cast<char*>(buf->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();

  int nchars;
  int written = source->WriteUtf8(
      isolate,
      write_result,
      dest_length,
      &nchars,
      String::NO_NULL_TERMINATION | String::REPLACE_INVALID_UTF8);

  binding_data->encode_into_results_buffer_[0] = nchars;
  binding_data->encode_into_results_buffer_[1] = written;
}

// Encode a single string to a UTF-8 Uint8Array (not Buffer).
// Used in TextEncoder.prototype.encode.
void BindingData::EncodeUtf8String(const FunctionCallbackInfo<Value>& args) {
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

  auto array = Uint8Array::New(ab, 0, length);
  args.GetReturnValue().Set(array);
}

// Convert the input into an encoded string
void BindingData::DecodeUTF8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);  // list, flags

  CHECK_GE(args.Length(), 1);

  if (!(args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer() ||
        args[0]->IsArrayBufferView())) {
    return node::THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(),
        "The \"list\" argument must be an instance of SharedArrayBuffer, "
        "ArrayBuffer or ArrayBufferView.");
  }

  ArrayBufferViewContents<char> buffer(args[0]);

  bool ignore_bom = args[1]->IsTrue();
  bool has_fatal = args[2]->IsTrue();

  const char* data = buffer.data();
  size_t length = buffer.length();

  if (has_fatal) {
    auto result = simdutf::validate_utf8_with_errors(data, length);

    if (result.error) {
      return node::THROW_ERR_ENCODING_INVALID_ENCODED_DATA(
          env->isolate(), "The encoded data was not valid for encoding utf-8");
    }
  }

  if (!ignore_bom && length >= 3) {
    if (memcmp(data, "\xEF\xBB\xBF", 3) == 0) {
      data += 3;
      length -= 3;
    }
  }

  if (length == 0) return args.GetReturnValue().SetEmptyString();

  Local<Value> error;
  MaybeLocal<Value> maybe_ret =
      StringBytes::Encode(env->isolate(), data, length, UTF8, &error);
  Local<Value> ret;

  if (!maybe_ret.ToLocal(&ret)) {
    CHECK(!error.IsEmpty());
    env->isolate()->ThrowException(error);
    return;
  }

  args.GetReturnValue().Set(ret);
}

void BindingData::Initialize(Local<Object> target,
                             Local<Value> unused,
                             Local<Context> context,
                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  BindingData* const binding_data =
      realm->AddBindingData<BindingData>(context, target);
  if (binding_data == nullptr) return;

  SetMethod(context, target, "encodeInto", EncodeInto);
  SetMethodNoSideEffect(context, target, "encodeUtf8String", EncodeUtf8String);
  SetMethodNoSideEffect(context, target, "decodeUTF8", DecodeUTF8);
}

void BindingData::RegisterTimerExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(EncodeInto);
  registry->Register(EncodeUtf8String);
  registry->Register(DecodeUTF8);
}

}  // namespace encoding_binding
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    encoding_binding, node::encoding_binding::BindingData::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(
    encoding_binding,
    node::encoding_binding::BindingData::RegisterTimerExternalReferences)
