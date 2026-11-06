#include "encoding_binding.h"
#include "ada.h"
#include "env-inl.h"
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
using v8::BackingStoreOnFailureMode;
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

constexpr bool isHighSurrogate(uint16_t value) {
  return (value & 0xfc00) == 0xd800;
}

struct EncodeIntoResult {
  size_t read = 0;
  size_t written = 0;
};

constexpr size_t utf8CodePointLength(uint32_t code_point) {
  if (code_point < 0x80) return 1;
  if (code_point < 0x800) return 2;
  if (code_point < 0x10000) return 3;
  return 4;
}

void writeUtf8CodePoint(uint32_t code_point, char* output) {
  DCHECK_LE(code_point, 0x10ffff);
  if (code_point < 0x80) {
    output[0] = static_cast<char>(code_point);
  } else if (code_point < 0x800) {
    output[0] = static_cast<char>(0xc0 | (code_point >> 6));
    output[1] = static_cast<char>(0x80 | (code_point & 0x3f));
  } else if (code_point < 0x10000) {
    output[0] = static_cast<char>(0xe0 | (code_point >> 12));
    output[1] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3f));
    output[2] = static_cast<char>(0x80 | (code_point & 0x3f));
  } else {
    output[0] = static_cast<char>(0xf0 | (code_point >> 18));
    output[1] = static_cast<char>(0x80 | ((code_point >> 12) & 0x3f));
    output[2] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3f));
    output[3] = static_cast<char>(0x80 | (code_point & 0x3f));
  }
}

// The previous findBestFit() path was correct, but it first measured a prefix
// and then encoded that prefix in a separate pass. Its adaptive retries could
// also measure the same input more than once. encodeInto() needs both consumed
// input code units and written output bytes, so these bounded helpers track
// both while converting guaranteed-to-fit SIMD chunks, using scalar code only
// at the output boundary. Malformed UTF-16 is replaced incrementally instead
// of requiring a temporary well-formed copy.
//
// These helpers are adapted from the detailed-result APIs added upstream in
// https://github.com/simdutf/simdutf/pull/1035.
//
// TODO(XadillaX): Replace them with
// convert_latin1_to_utf8_safe_with_details() and
// convert_utf16_to_utf8_with_replacement_safe() once Node's vendored simdutf
// provides those APIs.
EncodeIntoResult convertLatin1ToUtf8SafeWithDetails(const uint8_t* input,
                                                    size_t length,
                                                    char* output,
                                                    size_t capacity) {
  EncodeIntoResult result;

  while (true) {
    // A Latin-1 code unit expands to at most two UTF-8 bytes, so this prefix
    // always fits and can use simdutf's unbounded SIMD converter.
    const size_t chunk =
        std::min(length - result.read, (capacity - result.written) / 2);
    if (chunk <= 16) break;

    const size_t written = simdutf::convert_latin1_to_utf8(
        reinterpret_cast<const char*>(input + result.read),
        chunk,
        output + result.written);
    result.read += chunk;
    result.written += written;
  }

  while (result.read < length) {
    const uint32_t code_point = input[result.read];
    const size_t width = utf8CodePointLength(code_point);
    if (width > capacity - result.written) break;

    writeUtf8CodePoint(code_point, output + result.written);
    result.read++;
    result.written += width;
  }

  return result;
}

EncodeIntoResult convertUtf16ToUtf8WithReplacementSafe(const char16_t* input,
                                                       size_t length,
                                                       char* output,
                                                       size_t capacity) {
  EncodeIntoResult result;

  while (true) {
    // A UTF-16 code unit expands to at most three UTF-8 bytes, including an
    // unpaired surrogate replaced by U+FFFD. Keep a valid pair in one chunk.
    size_t chunk =
        std::min(length - result.read, (capacity - result.written) / 3);
    if (chunk <= 16) break;
    if (chunk < length - result.read &&
        isHighSurrogate(input[result.read + chunk - 1])) {
      chunk--;
    }

    const simdutf::result conversion =
        simdutf::convert_utf16_to_utf8_with_errors(
            input + result.read, chunk, output + result.written);
    if (conversion.error == simdutf::SUCCESS) {
      result.read += chunk;
      result.written += conversion.count;
      continue;
    }

    // convert_utf16_to_utf8_with_errors() has already written the valid
    // prefix. Its error count is in input code units, so recover the number
    // of output bytes only on this malformed-input path.
    const size_t valid_output =
        simdutf::utf8_length_from_utf16(input + result.read, conversion.count);
    result.read += conversion.count;
    result.written += valid_output;
    if (conversion.error != simdutf::SURROGATE ||
        capacity - result.written < 3) {
      return result;
    }

    writeUtf8CodePoint(0xfffd, output + result.written);
    result.read++;
    result.written += 3;
  }

  // The remaining output capacity is small enough that a scalar tail avoids
  // another sizing pass while preserving complete UTF-8 characters.
  while (result.read < length) {
    const uint16_t lead = input[result.read];
    size_t code_units = 1;
    uint32_t code_point;
    if (result.read + 1 < length &&
        isSurrogatePair(lead, input[result.read + 1])) {
      const uint16_t trail = input[result.read + 1];
      code_units = 2;
      code_point = 0x10000 + ((lead - 0xd800) << 10) + (trail - 0xdc00);
    } else if ((lead & 0xf800) == 0xd800) {
      code_point = 0xfffd;
    } else {
      code_point = lead;
    }

    const size_t width = utf8CodePointLength(code_point);
    if (width > capacity - result.written) break;

    writeUtf8CodePoint(code_point, output + result.written);
    result.read += code_units;
    result.written += width;
  }

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
    // Keep V8's unsigned representation while doing scalar length checks.
    // simdutf accepts const char* but interprets the same bytes as Latin-1.
    const uint8_t* data = view.data8();
    simdutf::result result = simdutf::validate_ascii_with_errors(
        reinterpret_cast<const char*>(data), length_that_fits);
    written = read = result.count;
    memcpy(write_result, data, read);
    write_result += read;
    data += read;
    length_that_fits -= read;
    dest_length -= read;
    if (length_that_fits != 0 && dest_length != 0) {
      const EncodeIntoResult rest = convertLatin1ToUtf8SafeWithDetails(
          data, length_that_fits, write_result, dest_length);
      read += rest.read;
      written += rest.written;
    }
  } else {
    auto data = reinterpret_cast<const char16_t*>(view.data16());

    // Do not let the conservative input limit split a valid surrogate pair.
    if (length_that_fits > 0 && length_that_fits < view.length() &&
        isSurrogatePair(data[length_that_fits - 1], data[length_that_fits])) {
      length_that_fits--;
    }

    const EncodeIntoResult result = convertUtf16ToUtf8WithReplacementSafe(
        data, length_that_fits, write_result, dest_length);
    read = result.read;
    written = result.written;
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

  Local<String> source = args[0].As<String>();

  // For small strings, use the V8 path
  static constexpr int kSmallStringThreshold = 32;
  if (source->Length() <= kSmallStringThreshold) {
    size_t length = source->Utf8LengthV2(isolate);
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        isolate,
        length,
        BackingStoreInitializationMode::kUninitialized,
        BackingStoreOnFailureMode::kReturnNull);

    if (!bs) [[unlikely]] {
      THROW_ERR_MEMORY_ALLOCATION_FAILED(isolate);
      return;
    }

    source->WriteUtf8V2(isolate,
                        static_cast<char*>(bs->Data()),
                        bs->MaxByteLength(),
                        String::WriteFlags::kReplaceInvalidUtf8);
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
    args.GetReturnValue().Set(Uint8Array::New(ab, 0, length));
    return;
  }

  size_t length = source->Length();
  size_t utf8_length = 0;

  // Inspect the string's flat content directly to determine the encoding and
  // the exact UTF-8 output size, without copying it out of the V8 heap.
  //
  // v8::String::ValueView holds a DisallowGarbageCollection scope, so it must
  // be released before allocating the backing store below. Flattening is cached
  // on the string, so re-acquiring the view for the conversion pass is cheap.
  bool is_one_byte;
  bool is_ascii = false;
  bool is_well_formed = true;
  {
    v8::String::ValueView view(isolate, source);
    is_one_byte = view.is_one_byte();
    if (is_one_byte) {
      auto data = reinterpret_cast<const char*>(view.data8());
      is_ascii = simdutf::validate_ascii_with_errors(data, length).error ==
                 simdutf::SUCCESS;
      utf8_length =
          is_ascii ? length : simdutf::utf8_length_from_latin1(data, length);
    } else {
      auto data = reinterpret_cast<const char16_t*>(view.data16());
      is_well_formed =
          simdutf::validate_utf16_with_errors(data, length).error ==
          simdutf::SUCCESS;
      if (is_well_formed) {
        utf8_length = simdutf::utf8_length_from_utf16(data, length);
      }
    }
  }

  // Rare path: two-byte string with unpaired surrogates. Copy into a mutable
  // buffer, make it well-formed, then encode.
  if (!is_well_formed) {
    MaybeStackBuffer<uint16_t, MAX_SIZE_FOR_STACK_ALLOC> utf16_buffer(length);
    source->WriteV2(isolate, 0, length, utf16_buffer.out());
    auto data = reinterpret_cast<char16_t*>(utf16_buffer.out());
    simdutf::to_well_formed_utf16(data, length, data);

    utf8_length = simdutf::utf8_length_from_utf16(data, length);
    std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
        isolate, utf8_length, BackingStoreInitializationMode::kUninitialized);
    CHECK(bs);
    [[maybe_unused]] size_t written = simdutf::convert_utf16_to_utf8(
        data, length, static_cast<char*>(bs->Data()));
    DCHECK_EQ(written, utf8_length);
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
    args.GetReturnValue().Set(Uint8Array::New(ab, 0, utf8_length));
    return;
  }

  // Common path: allocate the exact-size output, then re-acquire the flat
  // content and encode directly into the backing store.
  std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
      isolate, utf8_length, BackingStoreInitializationMode::kUninitialized);
  CHECK(bs);
  char* out = static_cast<char*>(bs->Data());
  {
    v8::String::ValueView view(isolate, source);
    if (is_one_byte) {
      auto data = reinterpret_cast<const char*>(view.data8());
      if (is_ascii) {
        memcpy(out, data, length);
      } else {
        [[maybe_unused]] size_t written =
            simdutf::convert_latin1_to_utf8(data, length, out);
        DCHECK_EQ(written, utf8_length);
      }
    } else {
      auto data = reinterpret_cast<const char16_t*>(view.data16());
      [[maybe_unused]] size_t written =
          simdutf::convert_utf16_to_utf8(data, length, out);
      DCHECK_EQ(written, utf8_length);
    }
  }
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, std::move(bs));
  args.GetReturnValue().Set(Uint8Array::New(ab, 0, utf8_length));
}

// Convert the input into an encoded string
void BindingData::DecodeUTF8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);  // list, flags

  CHECK_GE(args.Length(), 1);
  auto isShared = args[0]->IsSharedArrayBuffer();

  if (!(args[0]->IsArrayBuffer() || isShared || args[0]->IsArrayBufferView())) {
    return node::THROW_ERR_INVALID_ARG_TYPE(
        env->isolate(),
        "The \"list\" argument must be an instance of SharedArrayBuffer, "
        "ArrayBuffer or ArrayBufferView.");
  }

  if (args[0]->IsArrayBufferView()) {
    Local<v8::ArrayBufferView> view = args[0].As<v8::ArrayBufferView>();
    isShared = view->Buffer()->IsSharedArrayBuffer();
  }

  ArrayBufferViewContents<char> buffer(args[0]);

  bool ignore_bom = args[1]->IsTrue();
  bool has_fatal = args[2]->IsTrue();

  const char* data = buffer.data();
  size_t length = buffer.length();

  std::unique_ptr<char[]> data_copy;
  if (isShared && length != 0) {
    data_copy = std::make_unique_for_overwrite<char[]>(length);
    memcpy(data_copy.get(), data, length);
    data = data_copy.get();
  }

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
  }

  if (length == 0) return args.GetReturnValue().SetEmptyString();

  Local<Value> ret;
  v8::MaybeLocal<Value> encoded =
      has_fatal ? StringBytes::EncodeValidUtf8(env->isolate(), data, length)
                : StringBytes::Encode(env->isolate(), data, length, UTF8);
  if (encoded.ToLocal(&ret)) {
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
