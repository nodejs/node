#include "node.h"
#include "node_binding.h"
#include "node_buffer.h"
#include "node_test_fixture.h"

#include <cstring>
#include <string>
#include <vector>

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

// --- IPCChannelFramer (native read-path framing) ---

// Serializes `input` into a complete `advanced` frame (4-byte length prefix
// included), as written on the wire by writeChannelMessage().
v8::Local<v8::Value> SerializeFrame(v8::Local<v8::Context> context,
                                    v8::Local<v8::Function> serialize,
                                    v8::Local<v8::Value> input) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Value> buffer_ctor =
      context->Global()
          ->Get(context, v8::String::NewFromUtf8Literal(isolate, "Buffer"))
          .ToLocalChecked();
  v8::Local<v8::Value> args[] = {input, buffer_ctor};
  return serialize->Call(context, v8::Undefined(isolate), 2, args)
      .ToLocalChecked();
}

v8::Local<v8::Object> NewFramer(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> binding) {
  v8::Local<v8::Function> ctor =
      GetFunction(context, binding, "IPCChannelFramer");
  return ctor->NewInstance(context, 0, nullptr).ToLocalChecked();
}

v8::Local<v8::Uint8Array> MakeChunk(v8::Local<v8::Context> context,
                                    const void* data,
                                    size_t length) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, length);
  if (length > 0) memcpy(ab->Data(), data, length);
  return v8::Uint8Array::New(ab, 0, length);
}

v8::Local<v8::Array> FramerRead(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> framer,
                                v8::Local<v8::Value> chunk) {
  v8::Local<v8::Function> read = GetFunction(context, framer, "read");
  return read->Call(context, framer, 1, &chunk)
      .ToLocalChecked()
      .As<v8::Array>();
}

bool FramerBuffering(v8::Local<v8::Context> context,
                     v8::Local<v8::Object> framer) {
  v8::Local<v8::Function> buffering = GetFunction(context, framer, "buffering");
  return buffering->Call(context, framer, 0, nullptr)
      .ToLocalChecked()
      ->BooleanValue(v8::Isolate::GetCurrent());
}

TEST_F(IPCSerdesTest, FramerRoundTripsSingleMessage) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");

  v8::Local<v8::Value> input = v8::Number::New(isolate_, 42);
  v8::Local<v8::Value> frame = SerializeFrame(context, serialize, input);

  v8::Local<v8::Object> framer = NewFramer(context, binding);
  v8::Local<v8::Array> messages = FramerRead(context, framer, frame);

  ASSERT_EQ(messages->Length(), 1u);
  EXPECT_TRUE(messages->Get(context, 0).ToLocalChecked()->StrictEquals(input));
  EXPECT_FALSE(FramerBuffering(context, framer));
}

TEST_F(IPCSerdesTest, FramerSplitsMultipleMessagesInOneRead) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");

  v8::Local<v8::Value> frame1 =
      SerializeFrame(context, serialize, v8::Number::New(isolate_, 1));
  v8::Local<v8::Value> frame2 = SerializeFrame(
      context, serialize, v8::String::NewFromUtf8Literal(isolate_, "two"));

  node::ArrayBufferViewContents<uint8_t> b1(frame1);
  node::ArrayBufferViewContents<uint8_t> b2(frame2);
  std::vector<uint8_t> combined;
  combined.insert(combined.end(), b1.data(), b1.data() + b1.length());
  combined.insert(combined.end(), b2.data(), b2.data() + b2.length());

  v8::Local<v8::Object> framer = NewFramer(context, binding);
  v8::Local<v8::Array> messages = FramerRead(
      context, framer, MakeChunk(context, combined.data(), combined.size()));

  ASSERT_EQ(messages->Length(), 2u);
  EXPECT_EQ(messages->Get(context, 0)
                .ToLocalChecked()
                ->Int32Value(context)
                .FromJust(),
            1);
  v8::String::Utf8Value second(isolate_,
                               messages->Get(context, 1).ToLocalChecked());
  EXPECT_STREQ(*second, "two");
  EXPECT_FALSE(FramerBuffering(context, framer));
}

TEST_F(IPCSerdesTest, FramerBuffersPartialAdvancedFrame) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env{handle_scope, argv};
  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  v8::Local<v8::Object> binding = GetIpcSerdesBinding(*test_env, context);
  v8::Local<v8::Function> serialize =
      GetFunction(context, binding, "serialize");

  v8::Local<v8::Value> frame = SerializeFrame(
      context, serialize, v8::String::NewFromUtf8Literal(isolate_, "spanning"));
  node::ArrayBufferViewContents<uint8_t> bytes(frame);
  const size_t total = bytes.length();
  // Split inside the 4-byte length header to exercise that path too.
  const size_t split = 2;
  ASSERT_GT(total, split);

  v8::Local<v8::Object> framer = NewFramer(context, binding);

  v8::Local<v8::Array> first =
      FramerRead(context, framer, MakeChunk(context, bytes.data(), split));
  EXPECT_EQ(first->Length(), 0u);
  EXPECT_TRUE(FramerBuffering(context, framer));

  v8::Local<v8::Array> second = FramerRead(
      context, framer, MakeChunk(context, bytes.data() + split, total - split));
  ASSERT_EQ(second->Length(), 1u);
  v8::String::Utf8Value value(isolate_,
                              second->Get(context, 0).ToLocalChecked());
  EXPECT_STREQ(*value, "spanning");
  EXPECT_FALSE(FramerBuffering(context, framer));
}

}  // namespace
