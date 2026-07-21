#include "node.h"
#include "node_binding.h"
#include "node_buffer.h"
#include "node_test_fixture.h"

#include <string>

using node::Environment;

namespace {

// Bootstraps the environment (so Buffer and friends are initialized) and
// returns the `ipc_serdes` internal binding object.
v8::Local<v8::Object> GetIpcSerdesBinding(Environment* env,
                                          v8::Local<v8::Context> context) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  node::LoadEnvironment(env, "");
  v8::Local<v8::Function> get_internal_binding =
      v8::Function::New(context, node::binding::GetInternalBinding)
          .ToLocalChecked();
  v8::Local<v8::Value> binding_name =
      v8::String::NewFromUtf8Literal(isolate, "ipc_serdes");
  v8::Local<v8::Value> binding =
      get_internal_binding
          ->Call(context, v8::Undefined(isolate), 1, &binding_name)
          .ToLocalChecked();
  return binding.As<v8::Object>();
}

v8::Local<v8::Function> GetFunction(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> object,
                                    const char* name) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> value =
      object
          ->Get(context,
                v8::String::NewFromUtf8(isolate, name).ToLocalChecked())
          .ToLocalChecked();
  return value.As<v8::Function>();
}

// Serializes `input`, validates the 4-byte big-endian length prefix, strips it
// (as parseChannelMessages does), and returns the deserialized value.
v8::Local<v8::Value> RoundTrip(v8::Local<v8::Context> context,
                               v8::Local<v8::Function> serialize,
                               v8::Local<v8::Function> deserialize,
                               v8::Local<v8::Value> input) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  // serialize(message, Buffer): the second argument is the stable Buffer
  // constructor used to classify Node Buffers.
  v8::Local<v8::Value> buffer_ctor =
      context->Global()
          ->Get(context, v8::String::NewFromUtf8Literal(isolate, "Buffer"))
          .ToLocalChecked();
  v8::Local<v8::Value> serialize_args[] = {input, buffer_ctor};
  v8::Local<v8::Value> serialized =
      serialize->Call(context, v8::Undefined(isolate), 2, serialize_args)
          .ToLocalChecked();
  EXPECT_TRUE(serialized->IsUint8Array());
  v8::Local<v8::Uint8Array> buf = serialized.As<v8::Uint8Array>();
  EXPECT_GE(buf->ByteLength(), static_cast<size_t>(4));

  node::ArrayBufferViewContents<uint8_t> bytes(serialized);
  const uint32_t payload_length =
      (static_cast<uint32_t>(bytes.data()[0]) << 24) |
      (static_cast<uint32_t>(bytes.data()[1]) << 16) |
      (static_cast<uint32_t>(bytes.data()[2]) << 8) |
      static_cast<uint32_t>(bytes.data()[3]);
  EXPECT_EQ(payload_length, buf->ByteLength() - 4);

  v8::Local<v8::Value> payload = v8::Uint8Array::New(
      buf->Buffer(), buf->ByteOffset() + 4, buf->ByteLength() - 4);
  return deserialize->Call(context, v8::Undefined(isolate), 1, &payload)
      .ToLocalChecked();
}

class IPCSerdesTest : public EnvironmentTestFixture {};

TEST_F(IPCSerdesTest, RoundTripsPrimitives) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");
  v8::Local<v8::Function> deserialize =
      GetFunction(context, binding, "deserialize");

  v8::Local<v8::Value> inputs[] = {
      v8::Number::New(isolate_, 42),
      v8::Number::New(isolate_, -3.14),
      v8::String::NewFromUtf8Literal(isolate_, "hello world"),
      v8::Boolean::New(isolate_, true),
      v8::Null(isolate_),
  };

  for (v8::Local<v8::Value> input : inputs) {
    v8::Local<v8::Value> result =
        RoundTrip(context, serialize, deserialize, input);
    EXPECT_TRUE(result->StrictEquals(input));
  }
}

TEST_F(IPCSerdesTest, RoundTripsObject) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");
  v8::Local<v8::Function> deserialize =
      GetFunction(context, binding, "deserialize");

  v8::Local<v8::Object> input = v8::Object::New(isolate_);
  input
      ->Set(context,
            v8::String::NewFromUtf8Literal(isolate_, "foo"),
            v8::Number::New(isolate_, 42))
      .Check();
  input
      ->Set(context,
            v8::String::NewFromUtf8Literal(isolate_, "bar"),
            v8::String::NewFromUtf8Literal(isolate_, "baz"))
      .Check();

  v8::Local<v8::Value> result =
      RoundTrip(context, serialize, deserialize, input);
  ASSERT_TRUE(result->IsObject());
  v8::Local<v8::Object> obj = result.As<v8::Object>();

  v8::Local<v8::Value> foo =
      obj->Get(context, v8::String::NewFromUtf8Literal(isolate_, "foo"))
          .ToLocalChecked();
  EXPECT_EQ(foo->Int32Value(context).FromJust(), 42);

  v8::Local<v8::Value> bar =
      obj->Get(context, v8::String::NewFromUtf8Literal(isolate_, "bar"))
          .ToLocalChecked();
  v8::String::Utf8Value bar_utf8(isolate_, bar);
  EXPECT_STREQ(*bar_utf8, "baz");
}

TEST_F(IPCSerdesTest, RoundTripsTypedArray) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");
  v8::Local<v8::Function> deserialize =
      GetFunction(context, binding, "deserialize");

  v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate_, 4);
  uint8_t* data = static_cast<uint8_t*>(ab->Data());
  for (uint8_t i = 0; i < 4; i++) data[i] = i + 1;
  v8::Local<v8::Uint8Array> input = v8::Uint8Array::New(ab, 0, 4);

  v8::Local<v8::Value> result =
      RoundTrip(context, serialize, deserialize, input);
  ASSERT_TRUE(result->IsUint8Array());
  v8::Local<v8::Uint8Array> out = result.As<v8::Uint8Array>();
  ASSERT_EQ(out->ByteLength(), static_cast<size_t>(4));

  node::ArrayBufferViewContents<uint8_t> out_bytes(result);
  for (uint8_t i = 0; i < 4; i++) {
    EXPECT_EQ(out_bytes.data()[i], i + 1);
  }

  // A plain Uint8Array must not come back as a Node Buffer.
  Environment* env = *test_env;
  EXPECT_FALSE(
      out->GetPrototypeV2()->StrictEquals(env->buffer_prototype_object()));
}

TEST_F(IPCSerdesTest, BufferRoundTripsAsBuffer) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");
  v8::Local<v8::Function> deserialize =
      GetFunction(context, binding, "deserialize");

  v8::Local<v8::Object> input =
      node::Buffer::Copy(isolate_, "Hello!", 6).ToLocalChecked();

  v8::Local<v8::Value> result =
      RoundTrip(context, serialize, deserialize, input);
  ASSERT_TRUE(result->IsUint8Array());
  v8::Local<v8::Object> out = result.As<v8::Object>();

  // The key correctness property: a Buffer round-trips as a Buffer, not a
  // plain Uint8Array.
  Environment* env = *test_env;
  EXPECT_TRUE(
      out->GetPrototypeV2()->StrictEquals(env->buffer_prototype_object()));
  EXPECT_EQ(node::Buffer::Length(out), static_cast<size_t>(6));
  EXPECT_EQ(memcmp(node::Buffer::Data(out), "Hello!", 6), 0);
}

}  // namespace
