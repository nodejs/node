#include "encoding_binding.h"
#include "ada.h"
#include "env-inl.h"
#include "node_buffer.h"
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
using v8::BackingStoreInitializationMode;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::ObjectTemplate;
using v8::SnapshotCreator;
using v8::String;
using v8::Uint8Array;
using v8::Value;

void BindingData::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("encode_into_results_buffer",
                      encode_into_results_buffer_);
}

BindingData::BindingData(Realm* realm,
                         Local<Object> object,
                         InternalFieldInfo* info)
    : SnapshotableObject(realm, object, type_int),
      encode_into_results_buffer_(
          realm->isolate(),
          kEncodeIntoResultsLength,
          MAYBE_FIELD_PTR(info, encode_into_results_buffer)) {
  if (info == nullptr) {
    object
        ->Set(realm->context(),
              FIXED_ONE_BYTE_STRING(realm->isolate(), "encodeIntoResults"),
              encode_into_results_buffer_.GetJSArray())
        .Check();
  } else {
    encode_into_results_buffer_.Deserialize(realm->context());
  }
  encode_into_results_buffer_.MakeWeak();
}

bool BindingData::PrepareForSerialization(Local<Context> context,
                                          SnapshotCreator* creator) {
  DCHECK_NULL(internal_field_info_);
  internal_field_info_ = InternalFieldInfoBase::New<InternalFieldInfo>(type());
  internal_field_info_->encode_into_results_buffer =
      encode_into_results_buffer_.Serialize(context, creator);
  // Return true because we need to maintain the reference to the binding from
  // JS land.
  return true;
}

InternalFieldInfoBase* BindingData::Serialize(int index) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  InternalFieldInfo* info = internal_field_info_;
  internal_field_info_ = nullptr;
  return info;
}

void BindingData::Deserialize(Local<Context> context,
                              Local<Object> holder,
                              int index,
                              InternalFieldInfoBase* info) {
  DCHECK_IS_SNAPSHOT_SLOT(index);
  HandleScope scope(Isolate::GetCurrent());
  Realm* realm = Realm::GetCurrent(context);
  // Recreate the buffer in the constructor.
  InternalFieldInfo* casted_info = static_cast<InternalFieldInfo*>(info);
  BindingData* binding =
      realm->AddBindingData<BindingData>(holder, casted_info);
  CHECK_NOT_NULL(binding);
}

void BindingData::EncodeInto(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsUint8Array());

  Realm* realm = Realm::GetCurrent(args);
  Isolate* isolate = realm->isolate();
  BindingData* binding_data = realm->GetBindingData<BindingData>();

  Local<String> source = args[0].As<String>();

  Local<Uint8Array> dest = args[1].As<Uint8Array>();
  Local<ArrayBuffer> buf = dest->Buffer();
  char* write_result = static_cast<char*>(buf->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();

  size_t nchars;
  size_t written = source->WriteUtf8V2(isolate,
                                       write_result,
                                       dest_length,
                                       String::WriteFlags::kReplaceInvalidUtf8,
                                       &nchars);

  binding_data->encode_into_results_buffer_[0] = nchars;
  binding_data->encode_into_results_buffer_[1] = written;
}

// Encode a single string to a UTF-8 Uint8Array (not Buffer).
// Used in TextEncoder.prototype.encode.
void BindingData::EncodeUtf8String(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Local<String> str = args[0].As<String>();
  size_t length = str->Utf8LengthV2(isolate);

  Local<ArrayBuffer> ab;
  {
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        isolate, length, BackingStoreInitializationMode::kUninitialized);

    CHECK(bs);

    // We are certain that `data` is sufficiently large
    str->WriteUtf8V2(isolate,
                     static_cast<char*>(bs->Data()),
                     bs->MaxByteLength(),
                     String::WriteFlags::kReplaceInvalidUtf8);

    ab = ArrayBuffer::New(isolate, std::move(bs));
  }

  args.GetReturnValue().Set(Uint8Array::New(ab, 0, length));
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

  Local<Value> ret;
  if (StringBytes::Encode(env->isolate(), data, length, UTF8).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void BindingData::ToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Utf8Value input(env->isolate(), args[0]);
  auto out = ada::idna::to_ascii(input.ToStringView());
  Local<Value> ret;
  if (ToV8Value(env->context(), out, env->isolate()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void BindingData::ToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Utf8Value input(env->isolate(), args[0]);
  auto out = ada::idna::to_unicode(input.ToStringView());
  Local<Value> ret;
  if (ToV8Value(env->context(), out, env->isolate()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void BindingData::CreatePerIsolateProperties(IsolateData* isolate_data,
                                             Local<ObjectTemplate> target) {
  Isolate* isolate = isolate_data->isolate();
  SetMethod(isolate, target, "encodeInto", EncodeInto);
  SetMethodNoSideEffect(isolate, target, "encodeUtf8String", EncodeUtf8String);
  SetMethodNoSideEffect(isolate, target, "decodeUTF8", DecodeUTF8);
  SetMethodNoSideEffect(isolate, target, "toASCII", ToASCII);
  SetMethodNoSideEffect(isolate, target, "toUnicode", ToUnicode);
  SetMethodNoSideEffect(
      isolate, target, "decodeWindows1252", DecodeWindows1252);
}

void BindingData::CreatePerContextProperties(Local<Object> target,
                                             Local<Value> unused,
                                             Local<Context> context,
                                             void* priv) {
  Realm* realm = Realm::GetCurrent(context);
  realm->AddBindingData<BindingData>(target);
}

void BindingData::RegisterTimerExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(EncodeInto);
  registry->Register(EncodeUtf8String);
  registry->Register(DecodeUTF8);
  registry->Register(ToASCII);
  registry->Register(ToUnicode);
  registry->Register(DecodeWindows1252);
}

void BindingData::DecodeWindows1252(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 1);
  if (!(args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer() ||
        args[0]->IsArrayBufferView())) {
    return node::THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(),
        "The \"input\" argument must be an instance of ArrayBuffer, "
        "SharedArrayBuffer, or ArrayBufferView.");
  }

  bool ignore_bom = args[1]->IsTrue();

  ArrayBufferViewContents<uint8_t> buffer(args[0]);
  const uint8_t* data = buffer.data();
  size_t length = buffer.length();

  if (ignore_bom && length > 0 && data[0] == 0xFF) {
    data++;
    length--;
  }

  if (length == 0) {
    return args.GetReturnValue().SetEmptyString();
  }

  // Windows-1252 specific mapping for bytes 128-159
  // These differ from Latin-1/ISO-8859-1
  static const uint16_t windows1252_mapping[32] = {
      0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,  // 80-87
      0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,  // 88-8F
      0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,  // 90-97
      0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178   // 98-9F
  };

  std::string result;
  result.reserve(length * 3);  // Reserve space for UTF-8 output

  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];
    uint32_t codepoint;

    // Check if byte is in the special Windows-1252 range (128-159)
    if (byte >= 0x80 && byte <= 0x9F) {
      codepoint = windows1252_mapping[byte - 0x80];
    } else {
      // For all other bytes, Windows-1252 is identical to Latin-1
      codepoint = byte;
    }

    // Convert codepoint to UTF-8
    if (codepoint < 0x80) {
      result.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
      result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  Local<Value> ret;
  if (ToV8Value(env->context(), result, env->isolate()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

}  // namespace encoding_binding
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(
    encoding_binding,
    node::encoding_binding::BindingData::CreatePerContextProperties)
NODE_BINDING_PER_ISOLATE_INIT(
    encoding_binding,
    node::encoding_binding::BindingData::CreatePerIsolateProperties)
NODE_BINDING_EXTERNAL_REFERENCE(
    encoding_binding,
    node::encoding_binding::BindingData::RegisterTimerExternalReferences)
