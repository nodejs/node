#include "encoding_binding.h"
#include "ada.h"
#include "env-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "simdutf.h"
#include "string_bytes.h"
#include "util.h"
#include "v8.h"

#include <algorithm>
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

// The following code is adapted from Cloudflare workers.
// Particularly from: https://github.com/cloudflare/workerd/pull/5448
//
// Copyright (c) 2017-2025 Cloudflare, Inc.
// Licensed under the Apache 2.0 license found in the LICENSE file or at:
//     https://opensource.org/licenses/Apache-2.0
namespace {
constexpr int MAX_SIZE_FOR_STACK_ALLOC = 4096;

constexpr bool isSurrogatePair(uint16_t lead, uint16_t trail) {
  return (lead & 0xfc00) == 0xd800 && (trail & 0xfc00) == 0xdc00;
}

constexpr size_t simpleUtfEncodingLength(uint16_t c) {
  if (c < 0x80) return 1;
  if (c < 0x400) return 2;
  return 3;
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
  Isolate* isolate = realm->isolate();
  BindingData* binding_data = realm->GetBindingData<BindingData>();

  Local<String> source = args[0].As<String>();

  Local<Uint8Array> dest = args[1].As<Uint8Array>();
  Local<ArrayBuffer> buf = dest->Buffer();

  // Handle detached buffers - return {read: 0, written: 0}
  if (buf->Data() == nullptr) {
    binding_data->encode_into_results_buffer_[0] = 0;
    binding_data->encode_into_results_buffer_[1] = 0;
    return;
  }

  char* write_result = static_cast<char*>(buf->Data()) + dest->ByteOffset();
  size_t dest_length = dest->ByteLength();
  size_t read = 0;
  size_t written = 0;

  // For small strings (length <= 32), use the old V8 path for better
  // performance
  static constexpr int kSmallStringThreshold = 32;
  if (source->Length() <= kSmallStringThreshold) {
    written = source->WriteUtf8V2(isolate,
                                  write_result,
                                  dest_length,
                                  String::WriteFlags::kReplaceInvalidUtf8,
                                  &read);
    binding_data->encode_into_results_buffer_[0] = static_cast<double>(read);
    binding_data->encode_into_results_buffer_[1] = static_cast<double>(written);
    return;
  }

  v8::String::ValueView view(isolate, source);
  size_t length_that_fits =
      std::min(static_cast<size_t>(view.length()), dest_length);

  if (view.is_one_byte()) {
    auto data = reinterpret_cast<const char*>(view.data8());
    simdutf::result result =
        simdutf::validate_ascii_with_errors(data, length_that_fits);
    written = read = result.count;
    memcpy(write_result, data, read);
    write_result += read;
    data += read;
    length_that_fits -= read;
    dest_length -= read;
    if (length_that_fits != 0 && dest_length != 0) {
      if (size_t rest = findBestFit(data, length_that_fits, dest_length)) {
        DCHECK_LE(simdutf::utf8_length_from_latin1(data, rest), dest_length);
        written += simdutf::convert_latin1_to_utf8(data, rest, write_result);
        read += rest;
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
      read = findBestFit(data, length_that_fits, dest_length);
      if (read != 0) {
        DCHECK_LE(simdutf::utf8_length_from_utf16(data, read), dest_length);
        written = simdutf::convert_utf16_to_utf8(data, read, write_result);
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
      read =
          findBestFit(conversion_buffer.out(), length_that_fits, dest_length);
      if (read != 0) {
        DCHECK_LE(
            simdutf::utf8_length_from_utf16(conversion_buffer.out(), read),
            dest_length);
        written = simdutf::convert_utf16_to_utf8(
            conversion_buffer.out(), read, write_result);
      }
    }
  }
  DCHECK_LE(written, dest->ByteLength());

  binding_data->encode_into_results_buffer_[0] = static_cast<double>(read);
  binding_data->encode_into_results_buffer_[1] = static_cast<double>(written);
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
  SetMethodNoSideEffect(isolate, target, "decodeLatin1", DecodeLatin1);
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
  registry->Register(DecodeLatin1);
}

void BindingData::DecodeLatin1(const FunctionCallbackInfo<Value>& args) {
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
  bool has_fatal = args[2]->IsTrue();

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

  std::string result(length * 2, '\0');

  size_t written = simdutf::convert_latin1_to_utf8(
      reinterpret_cast<const char*>(data), length, result.data());

  if (has_fatal && written == 0) {
    return node::THROW_ERR_ENCODING_INVALID_ENCODED_DATA(
        env->isolate(), "The encoded data was not valid for encoding latin1");
  }

  std::string_view view(result.c_str(), written);

  Local<Value> ret;
  if (ToV8Value(env->context(), view, env->isolate()).ToLocal(&ret)) {
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
