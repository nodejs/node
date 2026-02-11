#include "encoding_binding.h"
#include "ada.h"
#include "env-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "simdutf.h"
#include "string_bytes.h"
#include "util-inl.h"
#include "v8.h"

#include <algorithm>
#include <cstdint>

namespace node {
namespace encoding_binding {

using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BackingStoreInitializationMode;
using v8::BackingStoreOnFailureMode;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::SnapshotCreator;
using v8::String;
using v8::Symbol;
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

// The following code is adapted from Cloudflare workers.
// Particularly from: https://github.com/cloudflare/workerd/pull/5448
//
// Copyright (c) 2017-2025 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0
namespace {
constexpr int MAX_SIZE_FOR_STACK_ALLOC = 4096;
constexpr int kEncodeIntoResultInternalFieldCount = 2;
constexpr int kEncodeIntoResultReadField = 0;
constexpr int kEncodeIntoResultWrittenField = 1;

constexpr bool isSurrogatePair(uint16_t lead, uint16_t trail) {
  return (lead & 0xfc00) == 0xd800 && (trail & 0xfc00) == 0xdc00;
}

constexpr size_t simpleUtfEncodingLength(uint16_t c) {
  if (c < 0x80) return 1;
  if (c < 0x400) return 2;
  return 3;
}

void EncodeIntoResultReadGetter(const FunctionCallbackInfo<Value>& args) {
  Local<Object> self = args.This().As<Object>();
  Local<Value> value =
      self->GetInternalField(kEncodeIntoResultReadField).As<Value>();
  args.GetReturnValue().Set(value);
}

void EncodeIntoResultWrittenGetter(const FunctionCallbackInfo<Value>& args) {
  Local<Object> self = args.This().As<Object>();
  Local<Value> value =
      self->GetInternalField(kEncodeIntoResultWrittenField).As<Value>();
  args.GetReturnValue().Set(value);
}

// Finds the maximum number of input characters (UTF-16 or Latin1) that can be
// encoded into a UTF-8 buffer of the given size.
//
// The challenge is that UTF-8 encoding expands characters by variable amounts:
// - ASCII (< 0x80): 1 byte
// - Code points < 0x800: 2 bytes
// - Other BMP characters: 3 bytes
// - Surrogate pairs (supplementary planes): 4 bytes total
//
// This function uses an adaptive chunking algorithm:
// 1. Process the input in chunks, estimating how many characters will fit
// 2. Calculate the actual UTF-8 length for each chunk using simdutf
// 3. Adjust the expansion factor based on observed encoding ratios
// 4. Fall back to character-by-character processing near the buffer boundary
// 5. Handle UTF-16 surrogate pairs to avoid splitting them across boundaries
//
// The algorithm starts with a conservative expansion estimate (1.15x) and
// dynamically adjusts based on actual character distribution, making it
// efficient for common ASCII-heavy text while remaining correct for
// multi-byte heavy content.
template <typename Char>
size_t findBestFit(const Char* data, size_t length, size_t bufferSize) {
  size_t pos = 0;
  size_t utf8Accumulated = 0;
  constexpr size_t CHUNK = 257;
  constexpr bool UTF16 = sizeof(Char) == 2;
  constexpr size_t MAX_FACTOR = UTF16 ? 3 : 2;

  double expansion = 1.15;

  while (pos < length && utf8Accumulated < bufferSize) {
    size_t remainingInput = length - pos;
    size_t spaceRemaining = bufferSize - utf8Accumulated;
    DCHECK_GE(expansion, 1.15);

    size_t guaranteedToFit = spaceRemaining / MAX_FACTOR;
    if (guaranteedToFit >= remainingInput) {
      return length;
    }
    size_t likelyToFit =
        std::min(static_cast<size_t>(spaceRemaining / expansion), CHUNK);
    size_t fitEstimate =
        std::max(size_t{1}, std::max(guaranteedToFit, likelyToFit));
    size_t chunkSize = std::min(remainingInput, fitEstimate);
    if (chunkSize == 1) break;
    CHECK_GT(chunkSize, 1);

    size_t chunkUtf8Len;
    if constexpr (UTF16) {
      // TODO(anonrig): Use utf8_length_from_utf16_with_replacement when
      // available For now, validate and use utf8_length_from_utf16
      size_t newPos = pos + chunkSize;
      if (newPos < length && isSurrogatePair(data[newPos - 1], data[newPos]))
        chunkSize--;
      chunkUtf8Len = simdutf::utf8_length_from_utf16(data + pos, chunkSize);
    } else {
      chunkUtf8Len = simdutf::utf8_length_from_latin1(data + pos, chunkSize);
    }

    if (utf8Accumulated + chunkUtf8Len > bufferSize) {
      DCHECK_GT(chunkSize, guaranteedToFit);
      expansion = std::max(expansion * 1.1, (chunkUtf8Len * 1.1) / chunkSize);
    } else {
      expansion = std::max(1.15, (chunkUtf8Len * 1.1) / chunkSize);
      pos += chunkSize;
      utf8Accumulated += chunkUtf8Len;
    }
  }

  while (pos < length && utf8Accumulated < bufferSize) {
    size_t extra = simpleUtfEncodingLength(data[pos]);
    if (utf8Accumulated + extra > bufferSize) break;
    pos++;
    utf8Accumulated += extra;
  }

  if (UTF16 && pos != 0 && pos != length &&
      isSurrogatePair(data[pos - 1], data[pos])) {
    if (utf8Accumulated < bufferSize) {
      pos++;
    } else {
      pos--;
    }
  }
  return pos;
}


MaybeLocal<Uint8Array> EncodeUtf8StringImpl(Isolate* isolate,
                                            Local<String> source) {
  // For small strings, use the V8 path
  static constexpr int kSmallStringThreshold = 16;
  if (source->Length() <= kSmallStringThreshold) {
    size_t length = source->Utf8LengthV2(isolate);
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        isolate,
        length,
        BackingStoreInitializationMode::kUninitialized,
        BackingStoreOnFailureMode::kReturnNull);

    if (!bs) [[unlikely]] {
      THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
      return MaybeLocal<Uint8Array>();
    }

    source->WriteUtf8V2(isolate,
                        static_cast<char*>(bs->Data()),
                        bs->MaxByteLength(),
                        String::WriteFlags::kReplaceInvalidUtf8);
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
    return Uint8Array::New(ab, 0, length);
  }

  size_t length = source->Length();
  size_t utf8_length = 0;

  if (source->IsOneByte()) {
    bool is_ascii = false;
    {
      v8::String::ValueView view(isolate, source);
      auto data = reinterpret_cast<const char*>(view.data8());
      simdutf::result result =
          simdutf::validate_ascii_with_errors(data, length);
      if (result.error == simdutf::SUCCESS) {
        is_ascii = true;
      } else {
        utf8_length = simdutf::utf8_length_from_latin1(data, length);
      }
    }

    size_t output_length = is_ascii ? length : utf8_length;
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        isolate, output_length, BackingStoreInitializationMode::kUninitialized);
    CHECK(bs);

    {
      v8::String::ValueView view(isolate, source);
      auto data = reinterpret_cast<const char*>(view.data8());
      if (is_ascii) {
        memcpy(bs->Data(), data, length);
      } else {
        [[maybe_unused]] size_t written = simdutf::convert_latin1_to_utf8(
            data, length, static_cast<char*>(bs->Data()));
        DCHECK_EQ(written, output_length);
      }
    }

    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
    return Uint8Array::New(ab, 0, output_length);
  }

  bool needs_well_formed = false;
  MaybeStackBuffer<char16_t, MAX_SIZE_FOR_STACK_ALLOC> conversion_buffer;
  {
    v8::String::ValueView view(isolate, source);
    auto data = reinterpret_cast<const char16_t*>(view.data16());
    simdutf::result validation_result =
        simdutf::validate_utf16_with_errors(data, length);

    if (validation_result.error == simdutf::SUCCESS) {
      utf8_length = simdutf::utf8_length_from_utf16(data, length);
    } else {
      conversion_buffer.AllocateSufficientStorage(length);
      conversion_buffer.SetLength(length);
      simdutf::to_well_formed_utf16(data, length, conversion_buffer.out());
      utf8_length =
          simdutf::utf8_length_from_utf16(conversion_buffer.out(), length);
      needs_well_formed = true;
    }
  }

  std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
      isolate, utf8_length, BackingStoreInitializationMode::kUninitialized);
  CHECK(bs);

  if (needs_well_formed) {
    [[maybe_unused]] size_t written = simdutf::convert_utf16_to_utf8(
        conversion_buffer.out(), length, static_cast<char*>(bs->Data()));
    DCHECK_EQ(written, utf8_length);
  } else {
    v8::String::ValueView view(isolate, source);
    auto data = reinterpret_cast<const char16_t*>(view.data16());
    [[maybe_unused]] size_t written = simdutf::convert_utf16_to_utf8(
        data, length, static_cast<char*>(bs->Data()));
    DCHECK_EQ(written, utf8_length);
  }

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
  return Uint8Array::New(ab, 0, utf8_length);
}

struct EncodeIntoResult {
  size_t read = 0;
  size_t written = 0;
};

EncodeIntoResult EncodeIntoImpl(Isolate* isolate,
                                Local<String> source,
                                Local<Uint8Array> dest) {
  EncodeIntoResult result;
  Local<ArrayBuffer> buf = dest->Buffer();

  // Handle detached buffers - return {read: 0, written: 0}
  if (buf->Data() == nullptr) return result;

  char* write_result = static_cast<char*>(buf->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();

  // For small strings (length <= 16), use the old V8 path for better
  // performance
  static constexpr int kSmallStringThreshold = 16;
  if (source->Length() <= kSmallStringThreshold) {
    result.written = source->WriteUtf8V2(
        isolate,
        write_result,
        dest_length,
        String::WriteFlags::kReplaceInvalidUtf8,
        &result.read);
    return result;
  }

  v8::String::ValueView view(isolate, source);
  size_t length_that_fits =
      std::min(static_cast<size_t>(view.length()), dest_length);

  if (view.is_one_byte()) {
    auto data = reinterpret_cast<const char*>(view.data8());
    simdutf::result validation_result =
        simdutf::validate_ascii_with_errors(data, length_that_fits);
    result.written = result.read = validation_result.count;
    memcpy(write_result, data, result.read);
    write_result += result.read;
    data += result.read;
    length_that_fits -= result.read;
    dest_length -= result.read;
    if (length_that_fits != 0 && dest_length != 0) {
      if (size_t rest = findBestFit(data, length_that_fits, dest_length)) {
        DCHECK_LE(simdutf::utf8_length_from_latin1(data, rest), dest_length);
        result.written +=
            simdutf::convert_latin1_to_utf8(data, rest, write_result);
        result.read += rest;
      }
    }
  } else {
    auto data = reinterpret_cast<const char16_t*>(view.data16());

    // Limit conversion to what could fit in destination, avoiding splitting
    // a valid surrogate pair at the boundary, which could cause a spurious call
    // of simdutf::to_well_formed_utf16()
    if (length_that_fits > 0 && length_that_fits < view.length() &&
        isSurrogatePair(data[length_that_fits - 1], data[length_that_fits])) {
      length_that_fits--;
    }

    // Check if input has unpaired surrogates - if so, convert to well-formed
    // first
    simdutf::result validation_result =
        simdutf::validate_utf16_with_errors(data, length_that_fits);

    if (validation_result.error == simdutf::SUCCESS) {
      // Valid UTF-16 - use the fast path
      result.read = findBestFit(data, length_that_fits, dest_length);
      if (result.read != 0) {
        DCHECK_LE(simdutf::utf8_length_from_utf16(data, result.read),
                  dest_length);
        result.written =
            simdutf::convert_utf16_to_utf8(data, result.read, write_result);
      }
    } else {
      // Invalid UTF-16 with unpaired surrogates - convert to well-formed first
      // TODO(anonrig): Use utf8_length_from_utf16_with_replacement when
      // available
      MaybeStackBuffer<char16_t, MAX_SIZE_FOR_STACK_ALLOC> conversion_buffer(
          length_that_fits);
      simdutf::to_well_formed_utf16(
          data, length_that_fits, conversion_buffer.out());

      // Now use findBestFit with the well-formed data
      result.read =
          findBestFit(conversion_buffer.out(), length_that_fits, dest_length);
      if (result.read != 0) {
        DCHECK_LE(simdutf::utf8_length_from_utf16(conversion_buffer.out(),
                                                  result.read),
                  dest_length);
        result.written = simdutf::convert_utf16_to_utf8(
            conversion_buffer.out(), result.read, write_result);
      }
    }
  }

  DCHECK_LE(result.written, dest->ByteLength());
  return result;
}
}  // namespace

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
  BindingData* binding_data = realm->GetBindingData<BindingData>();

  Local<String> source = args[0].As<String>();
  Local<Uint8Array> dest = args[1].As<Uint8Array>();

  EncodeIntoResult result = EncodeIntoImpl(realm->isolate(), source, dest);
  binding_data->encode_into_results_buffer_[0] =
      static_cast<double>(result.read);
  binding_data->encode_into_results_buffer_[1] =
      static_cast<double>(result.written);
}

// Encode a single string to a UTF-8 Uint8Array (not Buffer).
// Used in TextEncoder.prototype.encode.
void BindingData::EncodeUtf8String(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  Local<String> source = args[0].As<String>();
  MaybeLocal<Uint8Array> result = EncodeUtf8StringImpl(isolate, source);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

void TextEncoderConstructor(const FunctionCallbackInfo<Value>& args) {
  if (!args.IsConstructCall()) {
    Isolate* isolate = args.GetIsolate();
    isolate->ThrowException(v8::Exception::TypeError(
        OneByteString(isolate,
                      "Class constructor TextEncoder cannot be invoked without "
                      "'new'")));
    return;
  }
}

void TextEncoderEncode(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  Local<Context> context = isolate->GetCurrentContext();
  Local<String> input;
  if (args.Length() < 1 || args[0]->IsUndefined()) {
    input = String::Empty(isolate);
  } else {
    if (!args[0]->ToString(context).ToLocal(&input)) return;
  }

  MaybeLocal<Uint8Array> result = EncodeUtf8StringImpl(isolate, input);
  if (!result.IsEmpty()) {
    args.GetReturnValue().Set(result.ToLocalChecked());
  }
}

void TextEncoderEncodeInto(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  if (args.Length() < 2 || !args[0]->IsString() ||
      args[0]->IsStringObject()) {
    THROW_ERR_INVALID_ARG_TYPE(env,
                               "The \"src\" argument must be of type string");
    return;
  }

  if (!args[1]->IsUint8Array()) {
    THROW_ERR_INVALID_ARG_TYPE(
        env,
        "The \"dest\" argument must be an instance of Uint8Array");
    return;
  }

  Local<String> source = args[0].As<String>();
  Local<Uint8Array> dest = args[1].As<Uint8Array>();

  EncodeIntoResult result = EncodeIntoImpl(isolate, source, dest);

  // Use a cached ObjectTemplate so every result shares the same hidden class
  // (map). This gives V8 fast-properties objects with inline storage, unlike
  // DictionaryTemplate which creates slow dictionary-mode objects.
  Local<Context> context = env->context();
  auto result_template = env->text_encoder_encode_into_result_template();
  if (result_template.IsEmpty()) {
    result_template = ObjectTemplate::New(isolate);
    result_template->SetInternalFieldCount(kEncodeIntoResultInternalFieldCount);
    Local<FunctionTemplate> read_getter =
        FunctionTemplate::New(isolate, EncodeIntoResultReadGetter);
    Local<FunctionTemplate> written_getter =
        FunctionTemplate::New(isolate, EncodeIntoResultWrittenGetter);
    read_getter->SetLength(0);
    written_getter->SetLength(0);
    result_template->SetAccessorProperty(
        env->read_string(),
        read_getter,
        Local<FunctionTemplate>(),
        v8::PropertyAttribute::None);
    result_template->SetAccessorProperty(
        env->written_string(),
        written_getter,
        Local<FunctionTemplate>(),
        v8::PropertyAttribute::None);
    env->set_text_encoder_encode_into_result_template(result_template);
  }

  Local<Object> out;
  if (!result_template->NewInstance(context).ToLocal(&out)) return;
  out->SetInternalField(
      kEncodeIntoResultReadField,
      v8::Integer::NewFromUnsigned(isolate,
                                   static_cast<uint32_t>(result.read)));
  out->SetInternalField(
      kEncodeIntoResultWrittenField,
      v8::Integer::NewFromUnsigned(isolate,
                                   static_cast<uint32_t>(result.written)));
  args.GetReturnValue().Set(out);
}

void TextEncoderEncodingAccessor(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(isolate, "utf-8"));
}

void TextEncoderInspect(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  if (args.Length() > 0 && args[0]->IsNumber()) {
    double depth = args[0].As<v8::Number>()->Value();
    if (depth < 0) {
      args.GetReturnValue().Set(args.This());
      return;
    }
  }

  args.GetReturnValue().Set(
      OneByteString(isolate, "TextEncoder { encoding: 'utf-8' }"));
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

  if (!ignore_bom && length >= 3) {
    if (memcmp(data, "\xEF\xBB\xBF", 3) == 0) {
      data += 3;
      length -= 3;
    }
  }

  if (has_fatal) {
    // Are we perhaps ASCII? Then we won't have to check for UTF-8
    if (!simdutf::validate_ascii_with_errors(data, length).error) {
      Local<Value> ret;
      if (StringBytes::Encode(env->isolate(), data, length, LATIN1)
              .ToLocal(&ret)) {
        args.GetReturnValue().Set(ret);
      }
      return;
    }

    auto result = simdutf::validate_utf8_with_errors(data, length);

    if (result.error) {
      return node::THROW_ERR_ENCODING_INVALID_ENCODED_DATA(
          env->isolate(), "The encoded data was not valid for encoding utf-8");
    }

    // TODO(chalker): save on utf8 validity recheck in StringBytes::Encode()
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

  Local<FunctionTemplate> text_encoder =
      NewFunctionTemplate(isolate, TextEncoderConstructor);
  text_encoder->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "TextEncoder"));
  Local<v8::Signature> signature = v8::Signature::New(isolate, text_encoder);

  Local<FunctionTemplate> encode =
      NewFunctionTemplate(isolate,
                          TextEncoderEncode,
                          signature,
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasSideEffect);
  encode->SetLength(0);
  Local<String> encode_name = FIXED_ONE_BYTE_STRING(isolate, "encode");
  text_encoder->PrototypeTemplate()->Set(encode_name, encode);
  encode->SetClassName(encode_name);

  Local<FunctionTemplate> encode_into =
      NewFunctionTemplate(isolate,
                          TextEncoderEncodeInto,
                          signature,
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasSideEffect);
  encode_into->SetLength(2);
  Local<String> encode_into_name =
      FIXED_ONE_BYTE_STRING(isolate, "encodeInto");
  text_encoder->PrototypeTemplate()->Set(encode_into_name, encode_into);
  encode_into->SetClassName(encode_into_name);

  Local<FunctionTemplate> encoding_getter =
      NewFunctionTemplate(isolate,
                          TextEncoderEncodingAccessor,
                          signature,
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect);
  encoding_getter->SetClassName(
      FIXED_ONE_BYTE_STRING(isolate, "get encoding"));
  encoding_getter->SetLength(0);
  text_encoder->PrototypeTemplate()->SetAccessorProperty(
      FIXED_ONE_BYTE_STRING(isolate, "encoding"),
      encoding_getter,
      Local<FunctionTemplate>(),
      v8::PropertyAttribute::None);

  Local<FunctionTemplate> inspect =
      NewFunctionTemplate(isolate,
                          TextEncoderInspect,
                          signature,
                          v8::ConstructorBehavior::kThrow,
                          v8::SideEffectType::kHasNoSideEffect);
  inspect->SetLength(2);
  Local<Symbol> inspect_symbol =
      Symbol::For(isolate,
                  FIXED_ONE_BYTE_STRING(isolate,
                                        "nodejs.util.inspect.custom"));
  text_encoder->PrototypeTemplate()->Set(
      inspect_symbol, inspect, v8::PropertyAttribute::DontEnum);

  text_encoder->PrototypeTemplate()->Set(
      Symbol::GetToStringTag(isolate),
      FIXED_ONE_BYTE_STRING(isolate, "TextEncoder"),
      static_cast<v8::PropertyAttribute>(
          v8::PropertyAttribute::ReadOnly | v8::PropertyAttribute::DontEnum));

  text_encoder->ReadOnlyPrototype();
  SetConstructorFunction(isolate, target, "TextEncoder", text_encoder);
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
  registry->Register(TextEncoderConstructor);
  registry->Register(TextEncoderEncode);
  registry->Register(TextEncoderEncodeInto);
  registry->Register(TextEncoderEncodingAccessor);
  registry->Register(TextEncoderInspect);
  registry->Register(ToASCII);
  registry->Register(ToUnicode);
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
