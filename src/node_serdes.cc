#include "node.h"
#include "node_buffer.h"
#include "base-object.h"
#include "base-object-inl.h"
#include "env.h"
#include "env-inl.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::SharedArrayBuffer;
using v8::String;
using v8::Value;
using v8::ValueDeserializer;
using v8::ValueSerializer;

namespace {

class SerializerContext : public BaseObject,
                          public ValueSerializer::Delegate {
 public:
  SerializerContext(Environment* env,
                    Local<Object> wrap);

  ~SerializerContext() {}

  void ThrowDataCloneError(Local<String> message) override;
  Maybe<bool> WriteHostObject(Isolate* isolate, Local<Object> object) override;
  Maybe<uint32_t> GetSharedArrayBufferId(
      Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer) override;

  static void SetTreatArrayBufferViewsAsHostObjects(
      const FunctionCallbackInfo<Value>& args);

  static void New(const FunctionCallbackInfo<Value>& args);
  static void WriteHeader(const FunctionCallbackInfo<Value>& args);
  static void WriteValue(const FunctionCallbackInfo<Value>& args);
  static void ReleaseBuffer(const FunctionCallbackInfo<Value>& args);
  static void TransferArrayBuffer(const FunctionCallbackInfo<Value>& args);
  static void WriteUint32(const FunctionCallbackInfo<Value>& args);
  static void WriteUint64(const FunctionCallbackInfo<Value>& args);
  static void WriteDouble(const FunctionCallbackInfo<Value>& args);
  static void WriteRawBytes(const FunctionCallbackInfo<Value>& args);
 private:
  ValueSerializer serializer_;
};

class DeserializerContext : public BaseObject,
                            public ValueDeserializer::Delegate {
 public:
  DeserializerContext(Environment* env,
                      Local<Object> wrap,
                      Local<Value> buffer);

  ~DeserializerContext() {}

  MaybeLocal<Object> ReadHostObject(Isolate* isolate) override;

  static void New(const FunctionCallbackInfo<Value>& args);
  static void ReadHeader(const FunctionCallbackInfo<Value>& args);
  static void ReadValue(const FunctionCallbackInfo<Value>& args);
  static void TransferArrayBuffer(const FunctionCallbackInfo<Value>& args);
  static void GetWireFormatVersion(const FunctionCallbackInfo<Value>& args);
  static void ReadUint32(const FunctionCallbackInfo<Value>& args);
  static void ReadUint64(const FunctionCallbackInfo<Value>& args);
  static void ReadDouble(const FunctionCallbackInfo<Value>& args);
  static void ReadRawBytes(const FunctionCallbackInfo<Value>& args);
 private:
  const uint8_t* data_;
  const size_t length_;

  ValueDeserializer deserializer_;
};

SerializerContext::SerializerContext(Environment* env, Local<Object> wrap)
  : BaseObject(env, wrap),
    serializer_(env->isolate(), this) {
  MakeWeak<SerializerContext>(this);
}

void SerializerContext::ThrowDataCloneError(Local<String> message) {
  Local<Value> args[1] = { message };
  Local<Value> get_data_clone_error =
      object()->Get(env()->context(),
                    env()->get_data_clone_error_string())
                      .ToLocalChecked();

  CHECK(get_data_clone_error->IsFunction());
  MaybeLocal<Value> error =
      get_data_clone_error.As<Function>()->Call(env()->context(),
                                                object(),
                                                arraysize(args),
                                                args);

  if (error.IsEmpty()) return;

  env()->isolate()->ThrowException(error.ToLocalChecked());
}

Maybe<uint32_t> SerializerContext::GetSharedArrayBufferId(
    Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer) {
  Local<Value> args[1] = { shared_array_buffer };
  Local<Value> get_shared_array_buffer_id =
      object()->Get(env()->context(),
                    env()->get_shared_array_buffer_id_string())
                      .ToLocalChecked();

  if (!get_shared_array_buffer_id->IsFunction()) {
    return ValueSerializer::Delegate::GetSharedArrayBufferId(
        isolate, shared_array_buffer);
  }

  MaybeLocal<Value> id =
      get_shared_array_buffer_id.As<Function>()->Call(env()->context(),
                                                      object(),
                                                      arraysize(args),
                                                      args);

  if (id.IsEmpty()) return Nothing<uint32_t>();

  return id.ToLocalChecked()->Uint32Value(env()->context());
}

Maybe<bool> SerializerContext::WriteHostObject(Isolate* isolate,
                                               Local<Object> input) {
  MaybeLocal<Value> ret;
  Local<Value> args[1] = { input };

  Local<Value> write_host_object =
      object()->Get(env()->context(),
                    env()->write_host_object_string()).ToLocalChecked();

  if (!write_host_object->IsFunction()) {
    return ValueSerializer::Delegate::WriteHostObject(isolate, input);
  }

  ret = write_host_object.As<Function>()->Call(env()->context(),
                                               object(),
                                               arraysize(args),
                                               args);

  if (ret.IsEmpty())
    return Nothing<bool>();

  return Just(true);
}

void SerializerContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  new SerializerContext(env, args.This());
}

void SerializerContext::WriteHeader(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
  ctx->serializer_.WriteHeader();
}

void SerializerContext::WriteValue(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
  Maybe<bool> ret =
      ctx->serializer_.WriteValue(ctx->env()->context(), args[0]);

  if (ret.IsJust()) args.GetReturnValue().Set(ret.FromJust());
}

void SerializerContext::SetTreatArrayBufferViewsAsHostObjects(
    const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<bool> value = args[0]->BooleanValue(ctx->env()->context());
  if (value.IsNothing()) return;
  ctx->serializer_.SetTreatArrayBufferViewsAsHostObjects(value.FromJust());
}

void SerializerContext::ReleaseBuffer(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  std::pair<uint8_t*, size_t> ret = ctx->serializer_.Release();
  auto buf = Buffer::New(ctx->env(),
                         reinterpret_cast<char*>(ret.first),
                         ret.second);

  if (!buf.IsEmpty()) {
    args.GetReturnValue().Set(buf.ToLocalChecked());
  }
}

void SerializerContext::TransferArrayBuffer(
    const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<uint32_t> id = args[0]->Uint32Value(ctx->env()->context());
  if (id.IsNothing()) return;

  if (!args[1]->IsArrayBuffer())
    return ctx->env()->ThrowTypeError("arrayBuffer must be an ArrayBuffer");

  Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
  ctx->serializer_.TransferArrayBuffer(id.FromJust(), ab);
  return;
}

void SerializerContext::WriteUint32(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<uint32_t> value = args[0]->Uint32Value(ctx->env()->context());
  if (value.IsNothing()) return;

  ctx->serializer_.WriteUint32(value.FromJust());
}

void SerializerContext::WriteUint64(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<uint32_t> arg0 = args[0]->Uint32Value(ctx->env()->context());
  Maybe<uint32_t> arg1 = args[1]->Uint32Value(ctx->env()->context());
  if (arg0.IsNothing() || arg1.IsNothing())
    return;

  uint64_t hi = arg0.FromJust();
  uint64_t lo = arg1.FromJust();
  ctx->serializer_.WriteUint64((hi << 32) | lo);
}

void SerializerContext::WriteDouble(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<double> value = args[0]->NumberValue(ctx->env()->context());
  if (value.IsNothing()) return;

  ctx->serializer_.WriteDouble(value.FromJust());
}

void SerializerContext::WriteRawBytes(const FunctionCallbackInfo<Value>& args) {
  SerializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  if (!args[0]->IsUint8Array()) {
    return ctx->env()->ThrowTypeError("source must be a Uint8Array");
  }

  ctx->serializer_.WriteRawBytes(Buffer::Data(args[0]),
                                 Buffer::Length(args[0]));
}

DeserializerContext::DeserializerContext(Environment* env,
                                         Local<Object> wrap,
                                         Local<Value> buffer)
  : BaseObject(env, wrap),
    data_(reinterpret_cast<const uint8_t*>(Buffer::Data(buffer))),
    length_(Buffer::Length(buffer)),
    deserializer_(env->isolate(), data_, length_, this) {
  object()->Set(env->context(), env->buffer_string(), buffer).FromJust();

  MakeWeak<DeserializerContext>(this);
}

MaybeLocal<Object> DeserializerContext::ReadHostObject(Isolate* isolate) {
  Local<Value> read_host_object =
      object()->Get(env()->context(),
                    env()->read_host_object_string()).ToLocalChecked();

  if (!read_host_object->IsFunction()) {
    return ValueDeserializer::Delegate::ReadHostObject(isolate);
  }

  MaybeLocal<Value> ret =
      read_host_object.As<Function>()->Call(env()->context(),
                                            object(),
                                            0,
                                            nullptr);

  if (ret.IsEmpty())
    return MaybeLocal<Object>();

  Local<Value> return_value = ret.ToLocalChecked();
  if (!return_value->IsObject()) {
    env()->ThrowTypeError("readHostObject must return an object");
    return MaybeLocal<Object>();
  }

  return return_value.As<Object>();
}

void DeserializerContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint8Array()) {
    return env->ThrowTypeError("buffer must be a Uint8Array");
  }

  new DeserializerContext(env, args.This(), args[0]);
}

void DeserializerContext::ReadHeader(const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<bool> ret = ctx->deserializer_.ReadHeader(ctx->env()->context());

  if (ret.IsJust()) args.GetReturnValue().Set(ret.FromJust());
}

void DeserializerContext::ReadValue(const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  MaybeLocal<Value> ret = ctx->deserializer_.ReadValue(ctx->env()->context());

  if (!ret.IsEmpty()) args.GetReturnValue().Set(ret.ToLocalChecked());
}

void DeserializerContext::TransferArrayBuffer(
    const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<uint32_t> id = args[0]->Uint32Value(ctx->env()->context());
  if (id.IsNothing()) return;

  if (args[1]->IsArrayBuffer()) {
    Local<ArrayBuffer> ab = args[1].As<ArrayBuffer>();
    ctx->deserializer_.TransferArrayBuffer(id.FromJust(), ab);
    return;
  }

  if (args[1]->IsSharedArrayBuffer()) {
    Local<SharedArrayBuffer> sab = args[1].As<SharedArrayBuffer>();
    ctx->deserializer_.TransferSharedArrayBuffer(id.FromJust(), sab);
    return;
  }

  return ctx->env()->ThrowTypeError("arrayBuffer must be an ArrayBuffer or "
                                    "SharedArrayBuffer");
}

void DeserializerContext::GetWireFormatVersion(
    const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  args.GetReturnValue().Set(ctx->deserializer_.GetWireFormatVersion());
}

void DeserializerContext::ReadUint32(const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  uint32_t value;
  bool ok = ctx->deserializer_.ReadUint32(&value);
  if (!ok) return ctx->env()->ThrowError("ReadUint32() failed");
  return args.GetReturnValue().Set(value);
}

void DeserializerContext::ReadUint64(const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  uint64_t value;
  bool ok = ctx->deserializer_.ReadUint64(&value);
  if (!ok) return ctx->env()->ThrowError("ReadUint64() failed");

  uint32_t hi = static_cast<uint32_t>(value >> 32);
  uint32_t lo = static_cast<uint32_t>(value);

  Isolate* isolate = ctx->env()->isolate();
  Local<Context> context = ctx->env()->context();

  Local<Array> ret = Array::New(isolate, 2);
  ret->Set(context, 0, Integer::NewFromUnsigned(isolate, hi)).FromJust();
  ret->Set(context, 1, Integer::NewFromUnsigned(isolate, lo)).FromJust();
  return args.GetReturnValue().Set(ret);
}

void DeserializerContext::ReadDouble(const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  double value;
  bool ok = ctx->deserializer_.ReadDouble(&value);
  if (!ok) return ctx->env()->ThrowError("ReadDouble() failed");
  return args.GetReturnValue().Set(value);
}

void DeserializerContext::ReadRawBytes(
    const FunctionCallbackInfo<Value>& args) {
  DeserializerContext* ctx;
  ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

  Maybe<int64_t> length_arg = args[0]->IntegerValue(ctx->env()->context());
  if (length_arg.IsNothing()) return;
  size_t length = length_arg.FromJust();

  const void* data;
  bool ok = ctx->deserializer_.ReadRawBytes(length, &data);
  if (!ok) return ctx->env()->ThrowError("ReadRawBytes() failed");

  const uint8_t* position = reinterpret_cast<const uint8_t*>(data);
  CHECK_GE(position, ctx->data_);
  CHECK_LE(position + length, ctx->data_ + ctx->length_);

  const uint32_t offset = position - ctx->data_;
  CHECK_EQ(ctx->data_ + offset, position);

  args.GetReturnValue().Set(offset);
}

void InitializeSerdesBindings(Local<Object> target,
                              Local<Value> unused,
                              Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> ser =
      env->NewFunctionTemplate(SerializerContext::New);

  ser->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(ser, "writeHeader", SerializerContext::WriteHeader);
  env->SetProtoMethod(ser, "writeValue", SerializerContext::WriteValue);
  env->SetProtoMethod(ser, "releaseBuffer", SerializerContext::ReleaseBuffer);
  env->SetProtoMethod(ser,
                      "transferArrayBuffer",
                      SerializerContext::TransferArrayBuffer);
  env->SetProtoMethod(ser, "writeUint32", SerializerContext::WriteUint32);
  env->SetProtoMethod(ser, "writeUint64", SerializerContext::WriteUint64);
  env->SetProtoMethod(ser, "writeDouble", SerializerContext::WriteDouble);
  env->SetProtoMethod(ser, "writeRawBytes", SerializerContext::WriteRawBytes);
  env->SetProtoMethod(ser,
                      "_setTreatArrayBufferViewsAsHostObjects",
                      SerializerContext::SetTreatArrayBufferViewsAsHostObjects);

  ser->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Serializer"));
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "Serializer"),
              ser->GetFunction(env->context()).ToLocalChecked()).FromJust();

  Local<FunctionTemplate> des =
      env->NewFunctionTemplate(DeserializerContext::New);

  des->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(des, "readHeader", DeserializerContext::ReadHeader);
  env->SetProtoMethod(des, "readValue", DeserializerContext::ReadValue);
  env->SetProtoMethod(des,
                      "getWireFormatVersion",
                      DeserializerContext::GetWireFormatVersion);
  env->SetProtoMethod(des,
                      "transferArrayBuffer",
                      DeserializerContext::TransferArrayBuffer);
  env->SetProtoMethod(des, "readUint32", DeserializerContext::ReadUint32);
  env->SetProtoMethod(des, "readUint64", DeserializerContext::ReadUint64);
  env->SetProtoMethod(des, "readDouble", DeserializerContext::ReadDouble);
  env->SetProtoMethod(des, "_readRawBytes", DeserializerContext::ReadRawBytes);

  des->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Deserializer"));
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(env->isolate(), "Deserializer"),
              des->GetFunction(env->context()).ToLocalChecked()).FromJust();
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(serdes, node::InitializeSerdesBindings)
