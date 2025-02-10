#include "encoding_binding.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_test_fixture.h"
#include "v8.h"

namespace node {
namespace encoding_binding {

bool RunDecodeLatin1(Environment* env,
                     Local<Value> args[],
                     bool ignore_bom,
                     bool has_fatal,
                     Local<Value>* result) {
  Isolate* isolate = env->isolate();
  TryCatch try_catch(isolate);

  Local<Boolean> ignoreBOMValue = Boolean::New(isolate, ignore_bom);
  Local<Boolean> fatalValue = Boolean::New(isolate, has_fatal);

  Local<Value> updatedArgs[] = {args[0], ignoreBOMValue, fatalValue};

  BindingData::DecodeLatin1(FunctionCallbackInfo<Value>(updatedArgs));

  if (try_catch.HasCaught()) {
    return false;
  }

  *result = args[0];
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
  EXPECT_TRUE(RunDecodeLatin1(env, args, false, false, &result));

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
  EXPECT_TRUE(RunDecodeLatin1(env, args, false, false, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "");
}

TEST_F(EncodingBindingTest, DecodeLatin1_InvalidInput) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  Local<Value> args[] = {String::NewFromUtf8Literal(isolate, "Invalid input")};

  Local<Value> result;
  EXPECT_FALSE(RunDecodeLatin1(env, args, false, false, &result));
}

TEST_F(EncodingBindingTest, DecodeLatin1_IgnoreBOM) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t latin1_data[] = {0xFE, 0xFF, 0xC1, 0xE9, 0xF3};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(latin1_data));
  memcpy(ab->GetBackingStore()->Data(), latin1_data, sizeof(latin1_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(latin1_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_TRUE(RunDecodeLatin1(env, args, true, false, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "Áéó");
}

TEST_F(EncodingBindingTest, DecodeLatin1_FatalInvalidInput) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t invalid_data[] = {0xFF, 0xFF, 0xFF};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(invalid_data));
  memcpy(ab->GetBackingStore()->Data(), invalid_data, sizeof(invalid_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(invalid_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_FALSE(RunDecodeLatin1(env, args, false, true, &result));
}

TEST_F(EncodingBindingTest, DecodeLatin1_IgnoreBOMAndFatal) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t latin1_data[] = {0xFE, 0xFF, 0xC1, 0xE9, 0xF3};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(latin1_data));
  memcpy(ab->GetBackingStore()->Data(), latin1_data, sizeof(latin1_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(latin1_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_TRUE(RunDecodeLatin1(env, args, true, true, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "Áéó");
}

TEST_F(EncodingBindingTest, DecodeLatin1_BOMPresent) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t latin1_data[] = {0xFF, 0xC1, 0xE9, 0xF3};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(latin1_data));
  memcpy(ab->GetBackingStore()->Data(), latin1_data, sizeof(latin1_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(latin1_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  EXPECT_TRUE(RunDecodeLatin1(env, args, true, false, &result));

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "Áéó");
}

TEST_F(EncodingBindingTest, DecodeLatin1_ReturnsString) {
  Environment* env = CreateEnvironment();
  Isolate* isolate = env->isolate();
  HandleScope handle_scope(isolate);

  const uint8_t latin1_data[] = {0xC1, 0xE9, 0xF3};
  Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sizeof(latin1_data));
  memcpy(ab->GetBackingStore()->Data(), latin1_data, sizeof(latin1_data));

  Local<Uint8Array> array = Uint8Array::New(ab, 0, sizeof(latin1_data));
  Local<Value> args[] = {array};

  Local<Value> result;
  ASSERT_TRUE(RunDecodeLatin1(env, args, false, false, &result));

  ASSERT_TRUE(result->IsString());

  String::Utf8Value utf8_result(isolate, result);
  EXPECT_STREQ(*utf8_result, "Áéó");
}

}  // namespace encoding_binding
}  // namespace node
