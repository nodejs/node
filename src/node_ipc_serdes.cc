#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"

#include <cstring>
#include <utility>
#include <vector>

// Native implementation of the `advanced` child_process IPC serialization
// codec previously implemented in lib/internal/child_process/serialization.js
// (the ChildProcessSerializer / ChildProcessDeserializer classes). The wire
// format is preserved byte-for-byte:
//   [4-byte big-endian payload length][V8 ValueSerializer payload]
// ArrayBufferViews are treated as host objects (matching v8.DefaultSerializer)
// and tagged so that Node Buffers round-trip as Buffers rather than plain
// Uint8Arrays.

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt64Array;
using v8::BigUint64Array;
using v8::Context;
using v8::DataView;
using v8::Exception;
using v8::Float16Array;
using v8::Float32Array;
using v8::Float64Array;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Int16Array;
using v8::Int32Array;
using v8::Int8Array;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Name;
using v8::Nothing;
using v8::Object;
using v8::String;
using v8::Uint16Array;
using v8::Uint32Array;
using v8::Uint8Array;
using v8::Uint8ClampedArray;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;

namespace ipc_serdes {

// Tags written before each host object, matching serialization.js.
static constexpr uint32_t kArrayBufferViewTag = 0;
static constexpr uint32_t kNotArrayBufferViewTag = 1;

// ArrayBufferView type indices, matching arrayBufferViewTypeToIndex() in
// lib/v8.js. Index 10 is reserved for Node's Buffer (FastBuffer).
static constexpr uint32_t kInvalidViewIndex = 0xFFFFFFFF;
static constexpr uint32_t kBufferIndex = 10;

static uint32_t GetViewTypeIndex(Local<Object> view) {
  // Mirrors arrayBufferViewTypeToIndex() in lib/v8.js, classifying the view by
  // its class tag. Node Buffers are detected separately by the caller via the
  // `value.constructor === Buffer` check, matching DefaultSerializer.
  if (view->IsInt8Array()) return 0;
  if (view->IsUint8Array()) return 1;
  if (view->IsUint8ClampedArray()) return 2;
  if (view->IsInt16Array()) return 3;
  if (view->IsUint16Array()) return 4;
  if (view->IsInt32Array()) return 5;
  if (view->IsUint32Array()) return 6;
  if (view->IsFloat32Array()) return 7;
  if (view->IsFloat64Array()) return 8;
  if (view->IsDataView()) return 9;
  if (view->IsBigInt64Array()) return 11;
  if (view->IsBigUint64Array()) return 12;
  if (view->IsFloat16Array()) return 13;
  return kInvalidViewIndex;
}

static size_t BytesPerElement(uint32_t type_index) {
  switch (type_index) {
    case 3:   // Int16Array
    case 4:   // Uint16Array
    case 13:  // Float16Array
      return 2;
    case 5:  // Int32Array
    case 6:  // Uint32Array
    case 7:  // Float32Array
      return 4;
    case 8:   // Float64Array
    case 11:  // BigInt64Array
    case 12:  // BigUint64Array
      return 8;
    default:  // Int8/Uint8/Uint8Clamped/DataView/Buffer
      return 1;
  }
}

static MaybeLocal<Object> MakeView(Environment* env,
                                   uint32_t type_index,
                                   Local<ArrayBuffer> ab,
                                   size_t byte_offset,
                                   size_t byte_length) {
  const size_t length = byte_length / BytesPerElement(type_index);
  Local<Object> result;
  switch (type_index) {
    case 0:
      result = Int8Array::New(ab, byte_offset, length);
      break;
    case 1:
      result = Uint8Array::New(ab, byte_offset, length);
      break;
    case 2:
      result = Uint8ClampedArray::New(ab, byte_offset, length);
      break;
    case 3:
      result = Int16Array::New(ab, byte_offset, length);
      break;
    case 4:
      result = Uint16Array::New(ab, byte_offset, length);
      break;
    case 5:
      result = Int32Array::New(ab, byte_offset, length);
      break;
    case 6:
      result = Uint32Array::New(ab, byte_offset, length);
      break;
    case 7:
      result = Float32Array::New(ab, byte_offset, length);
      break;
    case 8:
      result = Float64Array::New(ab, byte_offset, length);
      break;
    case 9:
      result = DataView::New(ab, byte_offset, byte_length);
      break;
    case kBufferIndex: {
      Local<Uint8Array> buf;
      if (!Buffer::New(env, ab, byte_offset, byte_length).ToLocal(&buf)) {
        return {};
      }
      result = buf;
      break;
    }
    case 11:
      result = BigInt64Array::New(ab, byte_offset, length);
      break;
    case 12:
      result = BigUint64Array::New(ab, byte_offset, length);
      break;
    case 13:
      result = Float16Array::New(ab, byte_offset, length);
      break;
    default:
      THROW_ERR_INVALID_STATE(env, "Invalid host object type index");
      return {};
  }
  return result;
}

class IPCSerializerDelegate : public ValueSerializer::Delegate {
 public:
  IPCSerializerDelegate(Environment* env, Local<Value> buffer_constructor)
      : env_(env), buffer_constructor_(buffer_constructor) {}

  void set_serializer(ValueSerializer* serializer) { serializer_ = serializer; }

  void ThrowDataCloneError(Local<String> message) override {
    env_->isolate()->ThrowException(Exception::Error(message));
  }

  Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object) override {
    if (object->IsArrayBufferView()) {
      serializer_->WriteUint32(kArrayBufferViewTag);

      // Matches v8.js DefaultSerializer._writeHostObject: a Node Buffer is
      // identified by `value.constructor === Buffer`. The comparison is made
      // against the stable Buffer reference captured by serialization.js
      // (`require('buffer').Buffer`), not a value read back from
      // Buffer.prototype, which is itself tamperable.
      Local<Context> context = env_->context();
      Local<Value> view_constructor;
      if (!object->Get(context, FIXED_ONE_BYTE_STRING(isolate, "constructor"))
               .ToLocal(&view_constructor)) {
        return Nothing<bool>();
      }
      uint32_t type_index;
      if (view_constructor->StrictEquals(buffer_constructor_)) {
        type_index = kBufferIndex;
      } else {
        type_index = GetViewTypeIndex(object);
        if (type_index == kInvalidViewIndex) {
          THROW_ERR_INVALID_STATE(env_, "Unserializable host object");
          return Nothing<bool>();
        }
      }
      ArrayBufferViewContents<char> contents(object);
      serializer_->WriteUint32(type_index);
      serializer_->WriteUint32(static_cast<uint32_t>(contents.length()));
      serializer_->WriteRawBytes(contents.data(), contents.length());
      return Just(true);
    }

    // Non-view host object: serialize a shallow copy of its own enumerable
    // properties, matching `writeValue({ ...object })` in serialization.js.
    // Use CreateDataProperty (not Set) so inherited setters are not invoked,
    // exactly as the object-spread did.
    serializer_->WriteUint32(kNotArrayBufferViewTag);
    Local<Context> context = env_->context();
    Local<v8::Array> names;
    if (!object->GetOwnPropertyNames(context).ToLocal(&names)) {
      return Nothing<bool>();
    }
    Local<Object> copy = Object::New(isolate);
    const uint32_t len = names->Length();
    for (uint32_t i = 0; i < len; i++) {
      Local<Value> key;
      if (!names->Get(context, i).ToLocal(&key)) return Nothing<bool>();
      Local<Value> val;
      if (!object->Get(context, key).ToLocal(&val)) return Nothing<bool>();
      if (copy->CreateDataProperty(context, key.As<Name>(), val).IsNothing())
        return Nothing<bool>();
    }
    return serializer_->WriteValue(context, copy);
  }

 private:
  Environment* env_;
  // Stable `require('buffer').Buffer` reference passed from serialization.js,
  // used to classify Node Buffers without reading Buffer.prototype.constructor
  // (which can be tampered with).
  Local<Value> buffer_constructor_;
  ValueSerializer* serializer_ = nullptr;
};

class IPCDeserializerDelegate : public ValueDeserializer::Delegate {
 public:
  IPCDeserializerDelegate(Environment* env, Local<ArrayBuffer> ab)
      : env_(env), ab_(ab) {}

  void set_deserializer(ValueDeserializer* deserializer) {
    deserializer_ = deserializer;
  }

  MaybeLocal<Object> ReadHostObject(Isolate* isolate) override {
    uint32_t tag;
    if (!deserializer_->ReadUint32(&tag)) return {};

    if (tag == kNotArrayBufferViewTag) {
      Local<Value> value;
      if (!deserializer_->ReadValue(env_->context()).ToLocal(&value)) {
        return {};
      }
      if (!value->IsObject()) {
        THROW_ERR_INVALID_STATE(env_, "Host object must be an object");
        return {};
      }
      return value.As<Object>();
    }

    // Only the two tags written by WriteHostObject are valid. Reject anything
    // else, matching `assert(tag === kNotArrayBufferViewTag)` in the JS codec.
    if (tag != kArrayBufferViewTag) {
      THROW_ERR_INVALID_STATE(env_, "Invalid host object tag");
      return {};
    }

    uint32_t type_index;
    uint32_t byte_length;
    if (!deserializer_->ReadUint32(&type_index) ||
        !deserializer_->ReadUint32(&byte_length)) {
      return {};
    }
    const void* data;
    if (!deserializer_->ReadRawBytes(byte_length, &data)) return {};

    const size_t bytes_per_element = BytesPerElement(type_index);

    // Reuse the backing ArrayBuffer when the data is suitably aligned,
    // otherwise copy into a fresh aligned buffer. Mirrors _readHostObject()
    // in lib/v8.js. A frame reassembled across reads has no standalone
    // ArrayBuffer to borrow from (`ab_` is empty), so always copy.
    if (!ab_.IsEmpty()) {
      const size_t offset_in_ab = static_cast<const uint8_t*>(data) -
                                  static_cast<const uint8_t*>(ab_->Data());
      if (offset_in_ab % bytes_per_element == 0) {
        return MakeView(env_, type_index, ab_, offset_in_ab, byte_length);
      }
    }
    std::shared_ptr<BackingStore> store =
        ArrayBuffer::NewBackingStore(isolate, byte_length);
    memcpy(store->Data(), data, byte_length);
    Local<ArrayBuffer> copy = ArrayBuffer::New(isolate, std::move(store));
    return MakeView(env_, type_index, copy, 0, byte_length);
  }

 private:
  Environment* env_;
  Local<ArrayBuffer> ab_;
  ValueDeserializer* deserializer_ = nullptr;
};

static void Serialize(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  IPCSerializerDelegate delegate(env, args[1]);
  ValueSerializer serializer(isolate, &delegate);
  delegate.set_serializer(&serializer);
  serializer.SetTreatArrayBufferViewsAsHostObjects(true);

  // Reserve 4 bytes for the big-endian payload length, then write the
  // standard V8 header and the value, matching serialization.js.
  const uint8_t length_placeholder[4] = {0, 0, 0, 0};
  serializer.WriteRawBytes(length_placeholder, sizeof(length_placeholder));
  serializer.WriteHeader();

  bool wrote;
  if (!serializer.WriteValue(context, args[0]).To(&wrote) || !wrote) {
    // A pending exception was set by the delegate or V8.
    return;
  }

  std::pair<uint8_t*, size_t> result = serializer.Release();
  uint8_t* buf = result.first;
  const size_t size = result.second;
  const uint32_t payload_length = static_cast<uint32_t>(size - 4);
  buf[0] = (payload_length >> 24) & 0xFF;
  buf[1] = (payload_length >> 16) & 0xFF;
  buf[2] = (payload_length >> 8) & 0xFF;
  buf[3] = payload_length & 0xFF;

  Local<Object> out;
  if (Buffer::New(env, reinterpret_cast<char*>(buf), size).ToLocal(&out)) {
    args.GetReturnValue().Set(out);
  }
}

// Deserializes a single `advanced` payload: the V8 wire header followed by the
// value, without the 4-byte big-endian length prefix. When `ab` is non-empty
// it is the ArrayBuffer backing `data`, which the delegate may borrow for
// zero-copy ArrayBufferView host objects; when it is empty (a frame reassembled
// across reads) host objects are copied into fresh buffers.
static MaybeLocal<Value> DeserializeAdvancedPayload(Environment* env,
                                                    Local<ArrayBuffer> ab,
                                                    const uint8_t* data,
                                                    size_t length) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  IPCDeserializerDelegate delegate(env, ab);
  ValueDeserializer deserializer(isolate, data, length, &delegate);
  delegate.set_deserializer(&deserializer);

  bool read_header;
  if (!deserializer.ReadHeader(context).To(&read_header)) return {};
  return deserializer.ReadValue(context);
}

static inline uint32_t ReadUInt32BE(const char* p) {
  const uint8_t* b = reinterpret_cast<const uint8_t*>(p);
  return (static_cast<uint32_t>(b[0]) << 24) |
         (static_cast<uint32_t>(b[1]) << 16) |
         (static_cast<uint32_t>(b[2]) << 8) | static_cast<uint32_t>(b[3]);
}

static void Deserialize(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsArrayBufferView());
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<ArrayBuffer> ab = view->Buffer();
  const size_t byte_offset = view->ByteOffset();
  const size_t byte_length = view->ByteLength();
  const uint8_t* data = static_cast<const uint8_t*>(ab->Data()) + byte_offset;

  Local<Value> value;
  if (DeserializeAdvancedPayload(env, ab, data, byte_length).ToLocal(&value)) {
    args.GetReturnValue().Set(value);
  }
}

// Per-channel native framer for the `advanced` codec. It turns the raw IPC
// byte stream into complete, deserialized messages, owning the cross-read
// accumulation buffer that used to live in serialization.js (kMessageBuffer),
// so JavaScript receives whole messages and never reframes partial reads or
// concatenates partial frames itself.
//
// The `json` codec is intentionally not framed here: its StringDecoder +
// split('\n') pipeline in serialization.js already avoids copies (V8 rope
// concatenation and O(1) substrings) and is faster than reassembling the bytes
// in C++.
class IPCChannelFramer : public BaseObject {
 public:
  IPCChannelFramer(Environment* env, Local<Object> wrap)
      : BaseObject(env, wrap) {
    MakeWeak();
  }

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Read(const FunctionCallbackInfo<Value>& args);
  static void Buffering(const FunctionCallbackInfo<Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("buffered", buffered_);
  }
  SET_MEMORY_INFO_NAME(IPCChannelFramer)
  SET_SELF_SIZE(IPCChannelFramer)

 private:
  // Append complete deserialized messages found in `data` to `out`, buffering
  // any trailing partial frame for the next read. Return false (with a pending
  // exception) if deserialization fails.
  bool ReadAdvanced(Local<ArrayBuffer> ab,
                    const uint8_t* data,
                    size_t length,
                    LocalVector<Value>* out);

  std::vector<char> buffered_;
};

void IPCChannelFramer::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!args.IsConstructCall()) {
    return THROW_ERR_CONSTRUCT_CALL_REQUIRED(
        env,
        "Class constructor IPCChannelFramer cannot be invoked without 'new'");
  }
  new IPCChannelFramer(env, args.This());
}

bool IPCChannelFramer::ReadAdvanced(Local<ArrayBuffer> ab,
                                    const uint8_t* data,
                                    size_t length,
                                    LocalVector<Value>* out) {
  Environment* env = this->env();
  size_t pos = 0;

  // Phase 1: finish a frame whose bytes started in a previous read. It is
  // reassembled in `buffered_`, so it has no borrowable ArrayBuffer and its
  // host objects are copied.
  if (!buffered_.empty()) {
    while (buffered_.size() < 4 && pos < length) {
      buffered_.push_back(static_cast<char>(data[pos++]));
    }
    if (buffered_.size() < 4) return true;  // still buffering the length header

    const size_t frame_total =
        static_cast<size_t>(ReadUInt32BE(buffered_.data())) + 4;
    while (buffered_.size() < frame_total && pos < length) {
      buffered_.push_back(static_cast<char>(data[pos++]));
    }
    if (buffered_.size() < frame_total) return true;  // still buffering payload

    const uint8_t* payload =
        reinterpret_cast<const uint8_t*>(buffered_.data()) + 4;
    Local<Value> message;
    if (!DeserializeAdvancedPayload(
             env, Local<ArrayBuffer>(), payload, frame_total - 4)
             .ToLocal(&message)) {
      return false;
    }
    out->push_back(message);
    buffered_.clear();
  }

  // Phase 2: messages contained entirely within this chunk are deserialized in
  // place, letting host objects borrow the chunk's ArrayBuffer (zero copy).
  while (length - pos >= 4) {
    const size_t frame_total = static_cast<size_t>(ReadUInt32BE(
                                   reinterpret_cast<const char*>(data + pos))) +
                               4;
    if (length - pos < frame_total) break;  // incomplete trailing frame

    Local<Value> message;
    if (!DeserializeAdvancedPayload(env, ab, data + pos + 4, frame_total - 4)
             .ToLocal(&message)) {
      return false;
    }
    out->push_back(message);
    pos += frame_total;
  }

  if (pos < length) {
    buffered_.assign(reinterpret_cast<const char*>(data + pos),
                     reinterpret_cast<const char*>(data + length));
  }
  return true;
}

void IPCChannelFramer::Read(const FunctionCallbackInfo<Value>& args) {
  IPCChannelFramer* framer;
  ASSIGN_OR_RETURN_UNWRAP(&framer, args.This());
  Isolate* isolate = framer->env()->isolate();

  CHECK(args[0]->IsUint8Array());
  Local<Uint8Array> chunk = args[0].As<Uint8Array>();
  Local<ArrayBuffer> ab = chunk->Buffer();
  const size_t offset = chunk->ByteOffset();
  const size_t length = chunk->ByteLength();
  const uint8_t* data = static_cast<const uint8_t*>(ab->Data()) + offset;

  LocalVector<Value> messages(isolate);
  if (!framer->ReadAdvanced(ab, data, length, &messages)) {
    return;  // a pending exception propagates to JavaScript
  }

  args.GetReturnValue().Set(
      Array::New(isolate, messages.data(), messages.size()));
}

void IPCChannelFramer::Buffering(const FunctionCallbackInfo<Value>& args) {
  IPCChannelFramer* framer;
  ASSIGN_OR_RETURN_UNWRAP(&framer, args.This());
  args.GetReturnValue().Set(!framer->buffered_.empty());
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  SetMethod(context, target, "serialize", Serialize);
  SetMethod(context, target, "deserialize", Deserialize);

  Local<FunctionTemplate> framer =
      NewFunctionTemplate(isolate, IPCChannelFramer::New);
  framer->InstanceTemplate()->SetInternalFieldCount(
      IPCChannelFramer::kInternalFieldCount);
  SetProtoMethod(isolate, framer, "read", IPCChannelFramer::Read);
  SetProtoMethod(isolate, framer, "buffering", IPCChannelFramer::Buffering);
  framer->ReadOnlyPrototype();
  SetConstructorFunction(context, target, "IPCChannelFramer", framer);
}

static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Serialize);
  registry->Register(Deserialize);
  registry->Register(IPCChannelFramer::New);
  registry->Register(IPCChannelFramer::Read);
  registry->Register(IPCChannelFramer::Buffering);
}

}  // namespace ipc_serdes
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ipc_serdes, node::ipc_serdes::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(ipc_serdes,
                                node::ipc_serdes::RegisterExternalReferences)
