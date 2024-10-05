#include "encoding_binding.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "v8.h"

namespace node {
namespace encoding_binding {

bool RunDecodeLatin1(Environment* env,
                     Local<Value> args[],
                     Local<Value>* result) {
  Isolate* isolate = env->isolate();
  TryCatch try_catch(isolate);

  BindingData::DecodeLatin1(FunctionCallbackInfo<Value>(args));

  if (try_catch.HasCaught()) {
    return false;
  }

  *result = try_catch.Exception();
  return true;
}

class EncodingBindingTest : public NodeTestFixture {};

TEST_F(EncodingBindingTest, DecodeLatin1_ValidInput) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t latin1_data[] = {0xC1, 0xE9, 0xF3};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(latin1_data));
  memcpy(ab->GetBackingStore()->Data(), latin1_data, sizeof(latin1_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(latin1_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_TRUE(RunDecodeLatin1(env, args, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "Áéó");
}

TEST_F(EncodingBindingTest, DecodeLatin1_EmptyInput) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, 0);
  Local<Uint8Array> array = Uint8Array::New(ab, 0, 0);
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_TRUE(RunDecodeLatin1(env, args, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "");
}

TEST_F(EncodingBindingTest, DecodeLatin1_InvalidInput) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  Local<Value> args[] = {String::NewFromUtf8Literal(isolate, "Invalid input")};

  Local<Value> result;
  EXPECT_FALSE(RunDecodeLatin1(env, args, &result));
}

}  // namespace encoding_binding
}  // namespace node
