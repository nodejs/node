#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "util-inl.h"

#include <algorithm>
#include <cstring>
#include <memory>
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
using v8::TryCatch;
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
    // in lib/v8.js. A small frame reassembled across reads lives in a buffer
    // the framer reuses, with no ArrayBuffer to borrow (`ab_` is empty), so
    // its views are always copied.
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
// zero-copy ArrayBufferView host objects; when it is empty, host objects are
// copied into fresh buffers.
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

static inline uint32_t ReadUInt32BE(const uint8_t* b) {
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

// Frames that span reads and whose payload is at most this many bytes are
// accumulated into a reused buffer rather than a fresh BackingStore.
static constexpr size_t kMaxSmallPayload = 64 * 1024;

// Bits of the value returned by IPCChannelFramer.prototype.status().
static constexpr int kFramerBuffering = 1;  // a partial frame is buffered
static constexpr int kFramerError = 2;      // takeError() has an error to throw

// Per-channel native framer for the `advanced` codec. It turns the raw IPC
// byte stream into complete, deserialized messages, so JavaScript receives
// whole messages and never reframes partial reads itself.
//
// Frames contained entirely within one read are deserialized in place, and
// their ArrayBufferView host objects borrow the read's ArrayBuffer. A small
// frame that spans reads is accumulated into a buffer the framer reuses, whose
// size is bounded by kMaxSmallPayload. A larger one is accumulated into a
// BackingStore sized from its length prefix; once complete, that store becomes
// the frame's ArrayBuffer, so its host objects borrow it instead of copying,
// and the memory is released by the GC with them.
//
// Each frame is consumed before it is deserialized, so a frame that fails to
// deserialize never desynchronizes the stream. Its error is held until the
// rest of the read has been processed, and JavaScript throws it after
// delivering the messages decoded from the same read.
class IPCChannelFramer : public BaseObject {
 public:
  IPCChannelFramer(Environment* env, Local<Object> wrap)
      : BaseObject(env, wrap) {
    MakeWeak();
  }

  static void New(const FunctionCallbackInfo<Value>& args);
  static void Read(const FunctionCallbackInfo<Value>& args);
  static void Status(const FunctionCallbackInfo<Value>& args);
  static void TakeError(const FunctionCallbackInfo<Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("payload",
                                payload_ ? payload_->ByteLength() : 0);
    tracker->TrackFieldWithSize("small_payload", small_payload_.capacity());
    tracker->TrackField("pending_error", pending_error_);
  }
  SET_MEMORY_INFO_NAME(IPCChannelFramer)
  SET_SELF_SIZE(IPCChannelFramer)

 private:
  // Append the messages decoded from `data` to `out`, buffering any trailing
  // partial frame for the next read. Return false, with a pending exception,
  // only if execution is terminating; other errors are held in
  // `pending_error_`.
  bool ReadAdvanced(Local<ArrayBuffer> ab,
                    const uint8_t* data,
                    size_t length,
                    LocalVector<Value>* out);
  // Deserialize one payload and append it to `out`.
  bool Deliver(Local<ArrayBuffer> ab,
               const uint8_t* data,
               size_t length,
               LocalVector<Value>* out,
               TryCatch* try_catch);
  // Start accumulating a payload of `size` bytes that spans reads.
  bool StartPayload(size_t size, TryCatch* try_catch);
  // Keep the first caught error for takeError(). Return false if execution is
  // terminating, in which case the exception is rethrown.
  bool RecordError(TryCatch* try_catch);

  // Bytes of a length prefix split across reads.
  uint8_t header_[4];
  size_t header_size_ = 0;
  // Payload of a frame that spans reads, written to `payload_data_`: either
  // `small_payload_` or, for a payload larger than kMaxSmallPayload,
  // `payload_`. If allocating `payload_` failed, `discarding_` is set and the
  // payload bytes are counted and dropped.
  bool in_payload_ = false;
  bool discarding_ = false;
  uint8_t* payload_data_ = nullptr;
  std::vector<uint8_t> small_payload_;
  std::unique_ptr<BackingStore> payload_;
  size_t payload_size_ = 0;
  size_t payload_filled_ = 0;
  v8::Global<Value> pending_error_;
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

bool IPCChannelFramer::RecordError(TryCatch* try_catch) {
  if (try_catch->HasTerminated() || !try_catch->CanContinue()) {
    try_catch->ReThrow();
    return false;
  }
  if (pending_error_.IsEmpty()) {
    Isolate* isolate = env()->isolate();
    Local<Value> exception = try_catch->Exception();
    if (exception.IsEmpty()) {
      exception = ERR_INVALID_STATE(isolate, "Invalid IPC message");
    }
    pending_error_.Reset(isolate, exception);
  }
  try_catch->Reset();
  return true;
}

bool IPCChannelFramer::Deliver(Local<ArrayBuffer> ab,
                               const uint8_t* data,
                               size_t length,
                               LocalVector<Value>* out,
                               TryCatch* try_catch) {
  Local<Value> message;
  if (DeserializeAdvancedPayload(env(), ab, data, length).ToLocal(&message)) {
    out->push_back(message);
    return true;
  }
  return RecordError(try_catch);
}

bool IPCChannelFramer::StartPayload(size_t size, TryCatch* try_catch) {
  in_payload_ = true;
  payload_size_ = size;
  payload_filled_ = 0;
  discarding_ = false;
  if (size <= kMaxSmallPayload) {
    small_payload_.resize(size);
    payload_data_ = small_payload_.data();
    return true;
  }
  payload_ = ArrayBuffer::NewBackingStore(
      env()->isolate(),
      size,
      v8::BackingStoreInitializationMode::kUninitialized,
      v8::BackingStoreOnFailureMode::kReturnNull);
  if (payload_) {
    payload_data_ = static_cast<uint8_t*>(payload_->Data());
    return true;
  }
  discarding_ = true;
  payload_data_ = nullptr;
  THROW_ERR_MEMORY_ALLOCATION_FAILED(env());
  return RecordError(try_catch);
}

bool IPCChannelFramer::ReadAdvanced(Local<ArrayBuffer> ab,
                                    const uint8_t* data,
                                    size_t length,
                                    LocalVector<Value>* out) {
  Isolate* isolate = env()->isolate();
  TryCatch try_catch(isolate);
  size_t pos = 0;

  for (;;) {
    if (in_payload_) {
      // Continue a payload that started in an earlier read.
      const size_t n = std::min(payload_size_ - payload_filled_, length - pos);
      if (!discarding_ && n > 0) {
        memcpy(payload_data_ + payload_filled_, data + pos, n);
      }
      payload_filled_ += n;
      pos += n;
      if (payload_filled_ < payload_size_) return true;

      in_payload_ = false;
      payload_data_ = nullptr;
      if (discarding_) continue;
      Local<ArrayBuffer> frame;
      const uint8_t* frame_data = small_payload_.data();
      if (payload_) {
        frame = ArrayBuffer::New(isolate, std::move(payload_));
        frame_data = static_cast<const uint8_t*>(frame->Data());
      }
      if (!Deliver(frame, frame_data, payload_size_, out, &try_catch)) {
        return false;
      }
      continue;
    }

    const size_t available = length - pos;
    if (available == 0) return true;

    if (header_size_ == 0 && available >= sizeof(header_)) {
      const size_t size = ReadUInt32BE(data + pos);
      if (available - sizeof(header_) >= size) {
        // The whole frame is in this read: deserialize it in place.
        pos += sizeof(header_);
        if (!Deliver(ab, data + pos, size, out, &try_catch)) return false;
        pos += size;
        continue;
      }
    }

    // Accumulate the length prefix, which may itself span reads.
    const size_t n = std::min(sizeof(header_) - header_size_, available);
    memcpy(header_ + header_size_, data + pos, n);
    header_size_ += n;
    pos += n;
    if (header_size_ < sizeof(header_)) return true;
    header_size_ = 0;
    if (!StartPayload(ReadUInt32BE(header_), &try_catch)) return false;
  }
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
    return;  // execution is terminating
  }

  args.GetReturnValue().Set(
      Array::New(isolate, messages.data(), messages.size()));
}

void IPCChannelFramer::Status(const FunctionCallbackInfo<Value>& args) {
  IPCChannelFramer* framer;
  ASSIGN_OR_RETURN_UNWRAP(&framer, args.This());
  int status = 0;
  if (framer->in_payload_ || framer->header_size_ > 0) {
    status |= kFramerBuffering;
  }
  if (!framer->pending_error_.IsEmpty()) status |= kFramerError;
  args.GetReturnValue().Set(status);
}

void IPCChannelFramer::TakeError(const FunctionCallbackInfo<Value>& args) {
  IPCChannelFramer* framer;
  ASSIGN_OR_RETURN_UNWRAP(&framer, args.This());
  if (framer->pending_error_.IsEmpty()) return;
  args.GetReturnValue().Set(
      framer->pending_error_.Get(framer->env()->isolate()));
  framer->pending_error_.Reset();
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
  SetProtoMethod(isolate, framer, "status", IPCChannelFramer::Status);
  SetProtoMethod(isolate, framer, "takeError", IPCChannelFramer::TakeError);
  framer->ReadOnlyPrototype();
  SetConstructorFunction(context, target, "IPCChannelFramer", framer);
  NODE_DEFINE_CONSTANT(target, kFramerBuffering);
  NODE_DEFINE_CONSTANT(target, kFramerError);
}

static void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Serialize);
  registry->Register(Deserialize);
  registry->Register(IPCChannelFramer::New);
  registry->Register(IPCChannelFramer::Read);
  registry->Register(IPCChannelFramer::Status);
  registry->Register(IPCChannelFramer::TakeError);
}

}  // namespace ipc_serdes
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ipc_serdes, node::ipc_serdes::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(ipc_serdes,
                                node::ipc_serdes::RegisterExternalReferences)
