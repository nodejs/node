// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/value-serializer.h"

#include <algorithm>
#include <string>

#include "include/v8.h"
#include "src/api-inl.h"
#include "src/base/build_config.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-objects.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class ValueSerializerTest : public TestWithIsolate {
 protected:
  ValueSerializerTest()
      : serialization_context_(Context::New(isolate())),
        deserialization_context_(Context::New(isolate())) {
    // Create a host object type that can be tested through
    // serialization/deserialization delegates below.
    Local<FunctionTemplate> function_template = v8::FunctionTemplate::New(
        isolate(), [](const FunctionCallbackInfo<Value>& args) {
          args.Holder()->SetInternalField(0, args[0]);
          args.Holder()->SetInternalField(1, args[1]);
        });
    function_template->InstanceTemplate()->SetInternalFieldCount(2);
    function_template->InstanceTemplate()->SetAccessor(
        StringFromUtf8("value"),
        [](Local<String> property, const PropertyCallbackInfo<Value>& args) {
          args.GetReturnValue().Set(args.Holder()->GetInternalField(0));
        });
    function_template->InstanceTemplate()->SetAccessor(
        StringFromUtf8("value2"),
        [](Local<String> property, const PropertyCallbackInfo<Value>& args) {
          args.GetReturnValue().Set(args.Holder()->GetInternalField(1));
        });
    for (Local<Context> context :
         {serialization_context_, deserialization_context_}) {
      context->Global()
          ->CreateDataProperty(
              context, StringFromUtf8("ExampleHostObject"),
              function_template->GetFunction(context).ToLocalChecked())
          .ToChecked();
    }
    host_object_constructor_template_ = function_template;
    isolate_ = reinterpret_cast<i::Isolate*>(isolate());
  }

  ~ValueSerializerTest() override {
    // In some cases unhandled scheduled exceptions from current test produce
    // that Context::New(isolate()) from next test's constructor returns NULL.
    // In order to prevent that, we added destructor which will clear scheduled
    // exceptions just for the current test from test case.
    if (isolate_->has_scheduled_exception()) {
      isolate_->clear_scheduled_exception();
    }
  }

  const Local<Context>& serialization_context() {
    return serialization_context_;
  }
  const Local<Context>& deserialization_context() {
    return deserialization_context_;
  }

  bool ExpectInlineWasm() const { return expect_inline_wasm_; }
  void SetExpectInlineWasm(bool value) { expect_inline_wasm_ = value; }

  // Overridden in more specific fixtures.
  virtual ValueSerializer::Delegate* GetSerializerDelegate() { return nullptr; }
  virtual void BeforeEncode(ValueSerializer*) {}
  virtual ValueDeserializer::Delegate* GetDeserializerDelegate() {
    return nullptr;
  }
  virtual void BeforeDecode(ValueDeserializer*) {}

  Local<Value> RoundTripTest(Local<Value> input_value) {
    std::vector<uint8_t> encoded = EncodeTest(input_value);
    return DecodeTest(encoded);
  }

  // Variant for the common case where a script is used to build the original
  // value.
  Local<Value> RoundTripTest(const char* source) {
    return RoundTripTest(EvaluateScriptForInput(source));
  }

  // Variant which uses JSON.parse/stringify to check the result.
  void RoundTripJSON(const char* source) {
    Local<Value> input_value =
        JSON::Parse(serialization_context_, StringFromUtf8(source))
            .ToLocalChecked();
    Local<Value> result = RoundTripTest(input_value);
    ASSERT_TRUE(result->IsObject());
    EXPECT_EQ(source, Utf8Value(JSON::Stringify(deserialization_context_,
                                                result.As<Object>())
                                    .ToLocalChecked()));
  }

  Maybe<std::vector<uint8_t>> DoEncode(Local<Value> value) {
    Local<Context> context = serialization_context();
    ValueSerializer serializer(isolate(), GetSerializerDelegate());
    BeforeEncode(&serializer);
    serializer.WriteHeader();
    if (!serializer.WriteValue(context, value).FromMaybe(false)) {
      return Nothing<std::vector<uint8_t>>();
    }
    std::pair<uint8_t*, size_t> buffer = serializer.Release();
    std::vector<uint8_t> result(buffer.first, buffer.first + buffer.second);
    free(buffer.first);
    return Just(std::move(result));
  }

  std::vector<uint8_t> EncodeTest(Local<Value> input_value) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    std::vector<uint8_t> buffer;
    // Ideally we would use GTest's ASSERT_* macros here and below. However,
    // those only work in functions returning {void}, and they only terminate
    // the current function, but not the entire current test (so we would need
    // additional manual checks whether it is okay to proceed). Given that our
    // test driver starts a new process for each test anyway, it is acceptable
    // to just use a CHECK (which would kill the process on failure) instead.
    CHECK(DoEncode(input_value).To(&buffer));
    CHECK(!try_catch.HasCaught());
    return buffer;
  }

  v8::Local<v8::Message> InvalidEncodeTest(Local<Value> input_value) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    CHECK(DoEncode(input_value).IsNothing());
    return try_catch.Message();
  }

  v8::Local<v8::Message> InvalidEncodeTest(const char* source) {
    return InvalidEncodeTest(EvaluateScriptForInput(source));
  }

  Local<Value> DecodeTest(const std::vector<uint8_t>& data) {
    Local<Context> context = deserialization_context();
    Context::Scope scope(context);
    TryCatch try_catch(isolate());
    ValueDeserializer deserializer(isolate(), &data[0],
                                   static_cast<int>(data.size()),
                                   GetDeserializerDelegate());
    deserializer.SetSupportsLegacyWireFormat(true);
    deserializer.SetExpectInlineWasm(ExpectInlineWasm());
    BeforeDecode(&deserializer);
    CHECK(deserializer.ReadHeader(context).FromMaybe(false));
    Local<Value> result;
    CHECK(deserializer.ReadValue(context).ToLocal(&result));
    CHECK(!result.IsEmpty());
    CHECK(!try_catch.HasCaught());
    CHECK(context->Global()
              ->CreateDataProperty(context, StringFromUtf8("result"), result)
              .FromMaybe(false));
    CHECK(!try_catch.HasCaught());
    return result;
  }

  Local<Value> DecodeTestForVersion0(const std::vector<uint8_t>& data) {
    Local<Context> context = deserialization_context();
    Context::Scope scope(context);
    TryCatch try_catch(isolate());
    ValueDeserializer deserializer(isolate(), &data[0],
                                   static_cast<int>(data.size()),
                                   GetDeserializerDelegate());
    deserializer.SetSupportsLegacyWireFormat(true);
    deserializer.SetExpectInlineWasm(ExpectInlineWasm());
    BeforeDecode(&deserializer);
    CHECK(deserializer.ReadHeader(context).FromMaybe(false));
    CHECK_EQ(0u, deserializer.GetWireFormatVersion());
    Local<Value> result;
    CHECK(deserializer.ReadValue(context).ToLocal(&result));
    CHECK(!result.IsEmpty());
    CHECK(!try_catch.HasCaught());
    CHECK(context->Global()
              ->CreateDataProperty(context, StringFromUtf8("result"), result)
              .FromMaybe(false));
    CHECK(!try_catch.HasCaught());
    return result;
  }

  void InvalidDecodeTest(const std::vector<uint8_t>& data) {
    Local<Context> context = deserialization_context();
    Context::Scope scope(context);
    TryCatch try_catch(isolate());
    ValueDeserializer deserializer(isolate(), &data[0],
                                   static_cast<int>(data.size()),
                                   GetDeserializerDelegate());
    deserializer.SetSupportsLegacyWireFormat(true);
    deserializer.SetExpectInlineWasm(ExpectInlineWasm());
    BeforeDecode(&deserializer);
    Maybe<bool> header_result = deserializer.ReadHeader(context);
    if (header_result.IsNothing()) {
      EXPECT_TRUE(try_catch.HasCaught());
      return;
    }
    CHECK(header_result.ToChecked());
    CHECK(deserializer.ReadValue(context).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }

  Local<Value> EvaluateScriptForInput(const char* utf8_source) {
    Context::Scope scope(serialization_context_);
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(serialization_context_, source).ToLocalChecked();
    return script->Run(serialization_context_).ToLocalChecked();
  }

  void ExpectScriptTrue(const char* utf8_source) {
    Context::Scope scope(deserialization_context_);
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(deserialization_context_, source).ToLocalChecked();
    Local<Value> value = script->Run(deserialization_context_).ToLocalChecked();
    EXPECT_TRUE(value->BooleanValue(isolate()));
  }

  Local<String> StringFromUtf8(const char* source) {
    return String::NewFromUtf8(isolate(), source, NewStringType::kNormal)
        .ToLocalChecked();
  }

  std::string Utf8Value(Local<Value> value) {
    String::Utf8Value utf8(isolate(), value);
    return std::string(*utf8, utf8.length());
  }

  Local<Object> NewHostObject(Local<Context> context, int argc,
                              Local<Value> argv[]) {
    return host_object_constructor_template_->GetFunction(context)
        .ToLocalChecked()
        ->NewInstance(context, argc, argv)
        .ToLocalChecked();
  }

  Local<Object> NewDummyUint8Array() {
    static uint8_t data[] = {4, 5, 6};
    Local<ArrayBuffer> ab =
        ArrayBuffer::New(isolate(), static_cast<void*>(data), sizeof(data));
    return Uint8Array::New(ab, 0, sizeof(data));
  }

 private:
  Local<Context> serialization_context_;
  Local<Context> deserialization_context_;
  Local<FunctionTemplate> host_object_constructor_template_;
  i::Isolate* isolate_;
  bool expect_inline_wasm_ = false;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializerTest);
};

TEST_F(ValueSerializerTest, DecodeInvalid) {
  // Version tag but no content.
  InvalidDecodeTest({0xFF});
  // Version too large.
  InvalidDecodeTest({0xFF, 0x7F, 0x5F});
  // Nonsense tag.
  InvalidDecodeTest({0xFF, 0x09, 0xDD});
}

TEST_F(ValueSerializerTest, RoundTripOddball) {
  Local<Value> value = RoundTripTest(Undefined(isolate()));
  EXPECT_TRUE(value->IsUndefined());
  value = RoundTripTest(True(isolate()));
  EXPECT_TRUE(value->IsTrue());
  value = RoundTripTest(False(isolate()));
  EXPECT_TRUE(value->IsFalse());
  value = RoundTripTest(Null(isolate()));
  EXPECT_TRUE(value->IsNull());
}

TEST_F(ValueSerializerTest, DecodeOddball) {
  // What this code is expected to generate.
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x5F});
  EXPECT_TRUE(value->IsUndefined());
  value = DecodeTest({0xFF, 0x09, 0x54});
  EXPECT_TRUE(value->IsTrue());
  value = DecodeTest({0xFF, 0x09, 0x46});
  EXPECT_TRUE(value->IsFalse());
  value = DecodeTest({0xFF, 0x09, 0x30});
  EXPECT_TRUE(value->IsNull());

  // What v9 of the Blink code generates.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x5F, 0x00});
  EXPECT_TRUE(value->IsUndefined());
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x54, 0x00});
  EXPECT_TRUE(value->IsTrue());
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x46, 0x00});
  EXPECT_TRUE(value->IsFalse());
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x30, 0x00});
  EXPECT_TRUE(value->IsNull());

  // v0 (with no explicit version).
  value = DecodeTest({0x5F, 0x00});
  EXPECT_TRUE(value->IsUndefined());
  value = DecodeTest({0x54, 0x00});
  EXPECT_TRUE(value->IsTrue());
  value = DecodeTest({0x46, 0x00});
  EXPECT_TRUE(value->IsFalse());
  value = DecodeTest({0x30, 0x00});
  EXPECT_TRUE(value->IsNull());
}

TEST_F(ValueSerializerTest, RoundTripNumber) {
  Local<Value> value = RoundTripTest(Integer::New(isolate(), 42));
  ASSERT_TRUE(value->IsInt32());
  EXPECT_EQ(42, Int32::Cast(*value)->Value());

  value = RoundTripTest(Integer::New(isolate(), -31337));
  ASSERT_TRUE(value->IsInt32());
  EXPECT_EQ(-31337, Int32::Cast(*value)->Value());

  value = RoundTripTest(
      Integer::New(isolate(), std::numeric_limits<int32_t>::min()));
  ASSERT_TRUE(value->IsInt32());
  EXPECT_EQ(std::numeric_limits<int32_t>::min(), Int32::Cast(*value)->Value());

  value = RoundTripTest(Number::New(isolate(), -0.25));
  ASSERT_TRUE(value->IsNumber());
  EXPECT_EQ(-0.25, Number::Cast(*value)->Value());

  value = RoundTripTest(
      Number::New(isolate(), std::numeric_limits<double>::quiet_NaN()));
  ASSERT_TRUE(value->IsNumber());
  EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
}

TEST_F(ValueSerializerTest, DecodeNumber) {
  // 42 zig-zag encoded (signed)
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x49, 0x54});
  ASSERT_TRUE(value->IsInt32());
  EXPECT_EQ(42, Int32::Cast(*value)->Value());

  // 42 varint encoded (unsigned)
  value = DecodeTest({0xFF, 0x09, 0x55, 0x2A});
  ASSERT_TRUE(value->IsInt32());
  EXPECT_EQ(42, Int32::Cast(*value)->Value());

  // 160 zig-zag encoded (signed)
  value = DecodeTest({0xFF, 0x09, 0x49, 0xC0, 0x02});
  ASSERT_TRUE(value->IsInt32());
  ASSERT_EQ(160, Int32::Cast(*value)->Value());

  // 160 varint encoded (unsigned)
  value = DecodeTest({0xFF, 0x09, 0x55, 0xA0, 0x01});
  ASSERT_TRUE(value->IsInt32());
  ASSERT_EQ(160, Int32::Cast(*value)->Value());

#if defined(V8_TARGET_LITTLE_ENDIAN)
  // IEEE 754 doubles, little-endian byte order
  value = DecodeTest(
      {0xFF, 0x09, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD0, 0xBF});
  ASSERT_TRUE(value->IsNumber());
  EXPECT_EQ(-0.25, Number::Cast(*value)->Value());

  // quiet NaN
  value = DecodeTest(
      {0xFF, 0x09, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x7F});
  ASSERT_TRUE(value->IsNumber());
  EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));

  // signaling NaN
  value = DecodeTest(
      {0xFF, 0x09, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF4, 0x7F});
  ASSERT_TRUE(value->IsNumber());
  EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
#endif
  // TODO(jbroman): Equivalent test for big-endian machines.
}

TEST_F(ValueSerializerTest, RoundTripBigInt) {
  Local<Value> value = RoundTripTest(BigInt::New(isolate(), -42));
  ASSERT_TRUE(value->IsBigInt());
  ExpectScriptTrue("result === -42n");

  value = RoundTripTest(BigInt::New(isolate(), 42));
  ExpectScriptTrue("result === 42n");

  value = RoundTripTest(BigInt::New(isolate(), 0));
  ExpectScriptTrue("result === 0n");

  value = RoundTripTest("0x1234567890abcdef777888999n");
  ExpectScriptTrue("result === 0x1234567890abcdef777888999n");

  value = RoundTripTest("-0x1234567890abcdef777888999123n");
  ExpectScriptTrue("result === -0x1234567890abcdef777888999123n");

  Context::Scope scope(serialization_context());
  value = RoundTripTest(BigIntObject::New(isolate(), 23));
  ASSERT_TRUE(value->IsBigIntObject());
  ExpectScriptTrue("result == 23n");
}

TEST_F(ValueSerializerTest, DecodeBigInt) {
  Local<Value> value = DecodeTest({
      0xFF, 0x0D,              // Version 13
      0x5A,                    // BigInt
      0x08,                    // Bitfield: sign = false, bytelength = 4
      0x2A, 0x00, 0x00, 0x00,  // Digit: 42
  });
  ASSERT_TRUE(value->IsBigInt());
  ExpectScriptTrue("result === 42n");

  value = DecodeTest({
      0xFF, 0x0D,  // Version 13
      0x7A,        // BigIntObject
      0x11,        // Bitfield: sign = true, bytelength = 8
      0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Digit: 42
  });
  ASSERT_TRUE(value->IsBigIntObject());
  ExpectScriptTrue("result == -42n");

  value = DecodeTest({
      0xFF, 0x0D,  // Version 13
      0x5A,        // BigInt
      0x10,        // Bitfield: sign = false, bytelength = 8
      0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12  // Digit(s).
  });
  ExpectScriptTrue("result === 0x1234567890abcdefn");

  value = DecodeTest({0xFF, 0x0D,  // Version 13
                      0x5A,        // BigInt
                      0x17,        // Bitfield: sign = true, bytelength = 11
                      0xEF, 0xCD, 0xAB, 0x90,  // Digits.
                      0x78, 0x56, 0x34, 0x12, 0x33, 0x44, 0x55});
  ExpectScriptTrue("result === -0x5544331234567890abcdefn");

  value = DecodeTest({
      0xFF, 0x0D,  // Version 13
      0x5A,        // BigInt
      0x02,        // Bitfield: sign = false, bytelength = 1
      0x2A,        // Digit: 42
  });
  ExpectScriptTrue("result === 42n");
}

// String constants (in UTF-8) used for string encoding tests.
static const char kHelloString[] = "Hello";
static const char kQuebecString[] = "\x51\x75\xC3\xA9\x62\x65\x63";
static const char kEmojiString[] = "\xF0\x9F\x91\x8A";

TEST_F(ValueSerializerTest, RoundTripString) {
  Local<Value> value = RoundTripTest(String::Empty(isolate()));
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(0, String::Cast(*value)->Length());

  // Inside ASCII.
  value = RoundTripTest(StringFromUtf8(kHelloString));
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(5, String::Cast(*value)->Length());
  EXPECT_EQ(kHelloString, Utf8Value(value));

  // Inside Latin-1 (i.e. one-byte string), but not ASCII.
  value = RoundTripTest(StringFromUtf8(kQuebecString));
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(6, String::Cast(*value)->Length());
  EXPECT_EQ(kQuebecString, Utf8Value(value));

  // An emoji (decodes to two 16-bit chars).
  value = RoundTripTest(StringFromUtf8(kEmojiString));
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(2, String::Cast(*value)->Length());
  EXPECT_EQ(kEmojiString, Utf8Value(value));
}

TEST_F(ValueSerializerTest, DecodeString) {
  // Decoding the strings above from UTF-8.
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x53, 0x00});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(0, String::Cast(*value)->Length());

  value = DecodeTest({0xFF, 0x09, 0x53, 0x05, 'H', 'e', 'l', 'l', 'o'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(5, String::Cast(*value)->Length());
  EXPECT_EQ(kHelloString, Utf8Value(value));

  value =
      DecodeTest({0xFF, 0x09, 0x53, 0x07, 'Q', 'u', 0xC3, 0xA9, 'b', 'e', 'c'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(6, String::Cast(*value)->Length());
  EXPECT_EQ(kQuebecString, Utf8Value(value));

  value = DecodeTest({0xFF, 0x09, 0x53, 0x04, 0xF0, 0x9F, 0x91, 0x8A});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(2, String::Cast(*value)->Length());
  EXPECT_EQ(kEmojiString, Utf8Value(value));

  // And from Latin-1 (for the ones that fit).
  value = DecodeTest({0xFF, 0x0A, 0x22, 0x00});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(0, String::Cast(*value)->Length());

  value = DecodeTest({0xFF, 0x0A, 0x22, 0x05, 'H', 'e', 'l', 'l', 'o'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(5, String::Cast(*value)->Length());
  EXPECT_EQ(kHelloString, Utf8Value(value));

  value = DecodeTest({0xFF, 0x0A, 0x22, 0x06, 'Q', 'u', 0xE9, 'b', 'e', 'c'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(6, String::Cast(*value)->Length());
  EXPECT_EQ(kQuebecString, Utf8Value(value));

// And from two-byte strings (endianness dependent).
#if defined(V8_TARGET_LITTLE_ENDIAN)
  value = DecodeTest({0xFF, 0x09, 0x63, 0x00});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(0, String::Cast(*value)->Length());

  value = DecodeTest({0xFF, 0x09, 0x63, 0x0A, 'H', '\0', 'e', '\0', 'l', '\0',
                      'l', '\0', 'o', '\0'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(5, String::Cast(*value)->Length());
  EXPECT_EQ(kHelloString, Utf8Value(value));

  value = DecodeTest({0xFF, 0x09, 0x63, 0x0C, 'Q', '\0', 'u', '\0', 0xE9, '\0',
                      'b', '\0', 'e', '\0', 'c', '\0'});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(6, String::Cast(*value)->Length());
  EXPECT_EQ(kQuebecString, Utf8Value(value));

  value = DecodeTest({0xFF, 0x09, 0x63, 0x04, 0x3D, 0xD8, 0x4A, 0xDC});
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(2, String::Cast(*value)->Length());
  EXPECT_EQ(kEmojiString, Utf8Value(value));
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, DecodeInvalidString) {
  // UTF-8 string with too few bytes available.
  InvalidDecodeTest({0xFF, 0x09, 0x53, 0x10, 'v', '8'});
  // One-byte string with too few bytes available.
  InvalidDecodeTest({0xFF, 0x0A, 0x22, 0x10, 'v', '8'});
#if defined(V8_TARGET_LITTLE_ENDIAN)
  // Two-byte string with too few bytes available.
  InvalidDecodeTest({0xFF, 0x09, 0x63, 0x10, 'v', '\0', '8', '\0'});
  // Two-byte string with an odd byte length.
  InvalidDecodeTest({0xFF, 0x09, 0x63, 0x03, 'v', '\0', '8'});
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, EncodeTwoByteStringUsesPadding) {
  // As long as the output has a version that Blink expects to be able to read,
  // we must respect its alignment requirements. It requires that two-byte
  // characters be aligned.
  // We need a string whose length will take two bytes to encode, so that
  // a padding byte is needed to keep the characters aligned. The string
  // must also have a two-byte character, so that it gets the two-byte
  // encoding.
  std::string string(200, ' ');
  string += kEmojiString;
  const std::vector<uint8_t> data = EncodeTest(StringFromUtf8(string.c_str()));
  // This is a sufficient but not necessary condition. This test assumes
  // that the wire format version is one byte long, but is flexible to
  // what that value may be.
  const uint8_t expected_prefix[] = {0x00, 0x63, 0x94, 0x03};
  ASSERT_GT(data.size(), sizeof(expected_prefix) + 2);
  EXPECT_EQ(0xFF, data[0]);
  EXPECT_GE(data[1], 0x09);
  EXPECT_LE(data[1], 0x7F);
  EXPECT_TRUE(std::equal(std::begin(expected_prefix), std::end(expected_prefix),
                         data.begin() + 2));
}

TEST_F(ValueSerializerTest, RoundTripDictionaryObject) {
  // Empty object.
  Local<Value> value = RoundTripTest("({})");
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Object.prototype");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 0");

  // String key.
  value = RoundTripTest("({ a: 42 })");
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result.hasOwnProperty('a')");
  ExpectScriptTrue("result.a === 42");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Integer key (treated as a string, but may be encoded differently).
  value = RoundTripTest("({ 42: 'a' })");
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result.hasOwnProperty('42')");
  ExpectScriptTrue("result[42] === 'a'");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Key order must be preserved.
  value = RoundTripTest("({ x: 1, y: 2, a: 3 })");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).toString() === 'x,y,a'");

  // A harder case of enumeration order.
  // Indexes first, in order (but not 2^32 - 1, which is not an index), then the
  // remaining (string) keys, in the order they were defined.
  value = RoundTripTest("({ a: 2, 0xFFFFFFFF: 1, 0xFFFFFFFE: 3, 1: 0 })");
  ExpectScriptTrue(
      "Object.getOwnPropertyNames(result).toString() === "
      "'1,4294967294,a,4294967295'");
  ExpectScriptTrue("result.a === 2");
  ExpectScriptTrue("result[0xFFFFFFFF] === 1");
  ExpectScriptTrue("result[0xFFFFFFFE] === 3");
  ExpectScriptTrue("result[1] === 0");

  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  value = RoundTripTest("var y = {}; y.self = y; y;");
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result === result.self");
}

TEST_F(ValueSerializerTest, DecodeDictionaryObject) {
  // Empty object.
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x7B, 0x00, 0x00});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Object.prototype");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 0");

  // String key.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x01,
                      0x61, 0x3F, 0x01, 0x49, 0x54, 0x7B, 0x01});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result.hasOwnProperty('a')");
  ExpectScriptTrue("result.a === 42");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Integer key (treated as a string, but may be encoded differently).
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x49, 0x54,
                      0x3F, 0x01, 0x53, 0x01, 0x61, 0x7B, 0x01});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result.hasOwnProperty('42')");
  ExpectScriptTrue("result[42] === 'a'");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Key order must be preserved.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x01,
                      0x78, 0x3F, 0x01, 0x49, 0x02, 0x3F, 0x01, 0x53, 0x01,
                      0x79, 0x3F, 0x01, 0x49, 0x04, 0x3F, 0x01, 0x53, 0x01,
                      0x61, 0x3F, 0x01, 0x49, 0x06, 0x7B, 0x03});
  ExpectScriptTrue("Object.getOwnPropertyNames(result).toString() === 'x,y,a'");

  // A harder case of enumeration order.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x49, 0x02,
                      0x3F, 0x01, 0x49, 0x00, 0x3F, 0x01, 0x55, 0xFE, 0xFF,
                      0xFF, 0xFF, 0x0F, 0x3F, 0x01, 0x49, 0x06, 0x3F, 0x01,
                      0x53, 0x01, 0x61, 0x3F, 0x01, 0x49, 0x04, 0x3F, 0x01,
                      0x53, 0x0A, 0x34, 0x32, 0x39, 0x34, 0x39, 0x36, 0x37,
                      0x32, 0x39, 0x35, 0x3F, 0x01, 0x49, 0x02, 0x7B, 0x04});
  ExpectScriptTrue(
      "Object.getOwnPropertyNames(result).toString() === "
      "'1,4294967294,a,4294967295'");
  ExpectScriptTrue("result.a === 2");
  ExpectScriptTrue("result[0xFFFFFFFF] === 1");
  ExpectScriptTrue("result[0xFFFFFFFE] === 3");
  ExpectScriptTrue("result[1] === 0");

  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x04, 0x73,
                  0x65, 0x6C, 0x66, 0x3F, 0x01, 0x5E, 0x00, 0x7B, 0x01, 0x00});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result === result.self");
}

TEST_F(ValueSerializerTest, InvalidDecodeObjectWithInvalidKeyType) {
  // Objects which would need conversion to string shouldn't be present as
  // object keys. The serializer would have obtained them from the own property
  // keys list, which should only contain names and indices.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x6F, 0x61, 0x00, 0x40, 0x00, 0x00, 0x7B, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripOnlyOwnEnumerableStringKeys) {
  // Only "own" properties should be serialized, not ones on the prototype.
  Local<Value> value = RoundTripTest("var x = {}; x.__proto__ = {a: 4}; x;");
  ExpectScriptTrue("!('a' in result)");

  // Only enumerable properties should be serialized.
  value = RoundTripTest(
      "var x = {};"
      "Object.defineProperty(x, 'a', {value: 1, enumerable: false});"
      "x;");
  ExpectScriptTrue("!('a' in result)");

  // Symbol keys should not be serialized.
  value = RoundTripTest("({ [Symbol()]: 4 })");
  ExpectScriptTrue("Object.getOwnPropertySymbols(result).length === 0");
}

TEST_F(ValueSerializerTest, RoundTripTrickyGetters) {
  // Keys are enumerated before any setters are called, but if there is no own
  // property when the value is to be read, then it should not be serialized.
  Local<Value> value =
      RoundTripTest("({ get a() { delete this.b; return 1; }, b: 2 })");
  ExpectScriptTrue("!('b' in result)");

  // Keys added after the property enumeration should not be serialized.
  value = RoundTripTest("({ get a() { this.b = 3; }})");
  ExpectScriptTrue("!('b' in result)");

  // But if you remove a key and add it back, that's fine. But it will appear in
  // the original place in enumeration order.
  value =
      RoundTripTest("({ get a() { delete this.b; this.b = 4; }, b: 2, c: 3 })");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).toString() === 'a,b,c'");
  ExpectScriptTrue("result.b === 4");

  // Similarly, it only matters if a property was enumerable when the
  // enumeration happened.
  value = RoundTripTest(
      "({ get a() {"
      "    Object.defineProperty(this, 'b', {value: 2, enumerable: false});"
      "}, b: 1})");
  ExpectScriptTrue("result.b === 2");

  value = RoundTripTest(
      "var x = {"
      "  get a() {"
      "    Object.defineProperty(this, 'b', {value: 2, enumerable: true});"
      "  }"
      "};"
      "Object.defineProperty(x, 'b',"
      "    {value: 1, enumerable: false, configurable: true});"
      "x;");
  ExpectScriptTrue("!('b' in result)");

  // The property also should not be read if it can only be found on the
  // prototype chain (but not as an own property) after enumeration.
  value = RoundTripTest(
      "var x = { get a() { delete this.b; }, b: 1 };"
      "x.__proto__ = { b: 0 };"
      "x;");
  ExpectScriptTrue("!('b' in result)");

  // If an exception is thrown by script, encoding must fail and the exception
  // must be thrown.
  Local<Message> message =
      InvalidEncodeTest("({ get a() { throw new Error('sentinel'); } })");
  ASSERT_FALSE(message.IsEmpty());
  EXPECT_NE(std::string::npos, Utf8Value(message->Get()).find("sentinel"));
}

TEST_F(ValueSerializerTest, RoundTripDictionaryObjectForTransitions) {
  // A case which should run on the fast path, and should reach all of the
  // different cases:
  // 1. no known transition (first time creating this kind of object)
  // 2. expected transitions match to end
  // 3. transition partially matches, but falls back due to new property 'w'
  // 4. transition to 'z' is now a full transition (needs to be looked up)
  // 5. same for 'w'
  // 6. new property after complex transition succeeded
  // 7. new property after complex transition failed (due to new property)
  RoundTripJSON(
      "[{\"x\":1,\"y\":2,\"z\":3}"
      ",{\"x\":4,\"y\":5,\"z\":6}"
      ",{\"x\":5,\"y\":6,\"w\":7}"
      ",{\"x\":6,\"y\":7,\"z\":8}"
      ",{\"x\":0,\"y\":0,\"w\":0}"
      ",{\"x\":3,\"y\":1,\"w\":4,\"z\":1}"
      ",{\"x\":5,\"y\":9,\"k\":2,\"z\":6}]");
  // A simpler case that uses two-byte strings.
  RoundTripJSON(
      "[{\"\xF0\x9F\x91\x8A\":1,\"\xF0\x9F\x91\x8B\":2}"
      ",{\"\xF0\x9F\x91\x8A\":3,\"\xF0\x9F\x91\x8C\":4}"
      ",{\"\xF0\x9F\x91\x8A\":5,\"\xF0\x9F\x91\x9B\":6}]");
}

TEST_F(ValueSerializerTest, DecodeDictionaryObjectVersion0) {
  // Empty object.
  Local<Value> value = DecodeTestForVersion0({0x7B, 0x00});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Object.prototype");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 0");

  // String key.
  value =
      DecodeTestForVersion0({0x53, 0x01, 0x61, 0x49, 0x54, 0x7B, 0x01, 0x00});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Object.prototype");
  ExpectScriptTrue("result.hasOwnProperty('a')");
  ExpectScriptTrue("result.a === 42");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Integer key (treated as a string, but may be encoded differently).
  value =
      DecodeTestForVersion0({0x49, 0x54, 0x53, 0x01, 0x61, 0x7B, 0x01, 0x00});
  ASSERT_TRUE(value->IsObject());
  ExpectScriptTrue("result.hasOwnProperty('42')");
  ExpectScriptTrue("result[42] === 'a'");
  ExpectScriptTrue("Object.getOwnPropertyNames(result).length === 1");

  // Key order must be preserved.
  value = DecodeTestForVersion0({0x53, 0x01, 0x78, 0x49, 0x02, 0x53, 0x01, 0x79,
                                 0x49, 0x04, 0x53, 0x01, 0x61, 0x49, 0x06, 0x7B,
                                 0x03, 0x00});
  ExpectScriptTrue("Object.getOwnPropertyNames(result).toString() === 'x,y,a'");

  // A property and an element.
  value = DecodeTestForVersion0(
      {0x49, 0x54, 0x53, 0x01, 0x61, 0x53, 0x01, 0x61, 0x49, 0x54, 0x7B, 0x02});
  ExpectScriptTrue("Object.getOwnPropertyNames(result).toString() === '42,a'");
  ExpectScriptTrue("result[42] === 'a'");
  ExpectScriptTrue("result.a === 42");
}

TEST_F(ValueSerializerTest, RoundTripArray) {
  // A simple array of integers.
  Local<Value> value = RoundTripTest("[1, 2, 3, 4, 5]");
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(5u, Array::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Array.prototype");
  ExpectScriptTrue("result.toString() === '1,2,3,4,5'");

  // A long (sparse) array.
  value = RoundTripTest("var x = new Array(1000); x[500] = 42; x;");
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[500] === 42");

  // Duplicate reference.
  value = RoundTripTest("var y = {}; [y, y];");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === result[1]");

  // Duplicate reference in a sparse array.
  value = RoundTripTest("var x = new Array(1000); x[1] = x[500] = {}; x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[1] === 'object'");
  ExpectScriptTrue("result[1] === result[500]");

  // Self reference.
  value = RoundTripTest("var y = []; y[0] = y; y;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === result");

  // Self reference in a sparse array.
  value = RoundTripTest("var y = new Array(1000); y[519] = y; y;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[519] === result");

  // Array with additional properties.
  value = RoundTripTest("var y = [1, 2]; y.foo = 'bar'; y;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.toString() === '1,2'");
  ExpectScriptTrue("result.foo === 'bar'");

  // Sparse array with additional properties.
  value = RoundTripTest("var y = new Array(1000); y.foo = 'bar'; y;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.toString() === ','.repeat(999)");
  ExpectScriptTrue("result.foo === 'bar'");

  // The distinction between holes and undefined elements must be maintained.
  value = RoundTripTest("[,undefined]");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[0] === 'undefined'");
  ExpectScriptTrue("typeof result[1] === 'undefined'");
  ExpectScriptTrue("!result.hasOwnProperty(0)");
  ExpectScriptTrue("result.hasOwnProperty(1)");
}

TEST_F(ValueSerializerTest, DecodeArray) {
  // A simple array of integers.
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x41, 0x05, 0x3F, 0x01, 0x49, 0x02,
                  0x3F, 0x01, 0x49, 0x04, 0x3F, 0x01, 0x49, 0x06, 0x3F, 0x01,
                  0x49, 0x08, 0x3F, 0x01, 0x49, 0x0A, 0x24, 0x00, 0x05, 0x00});
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(5u, Array::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Array.prototype");
  ExpectScriptTrue("result.toString() === '1,2,3,4,5'");

  // A long (sparse) array.
  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x61, 0xE8, 0x07, 0x3F, 0x01, 0x49,
                  0xE8, 0x07, 0x3F, 0x01, 0x49, 0x54, 0x40, 0x01, 0xE8, 0x07});
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[500] === 42");

  // Duplicate reference.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x41, 0x02, 0x3F, 0x01, 0x6F,
                      0x7B, 0x00, 0x3F, 0x02, 0x5E, 0x01, 0x24, 0x00, 0x02});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === result[1]");

  // Duplicate reference in a sparse array.
  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x61, 0xE8, 0x07, 0x3F, 0x01, 0x49,
                  0x02, 0x3F, 0x01, 0x6F, 0x7B, 0x00, 0x3F, 0x02, 0x49, 0xE8,
                  0x07, 0x3F, 0x02, 0x5E, 0x01, 0x40, 0x02, 0xE8, 0x07, 0x00});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[1] === 'object'");
  ExpectScriptTrue("result[1] === result[500]");

  // Self reference.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x41, 0x01, 0x3F, 0x01, 0x5E,
                      0x00, 0x24, 0x00, 0x01, 0x00});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === result");

  // Self reference in a sparse array.
  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x61, 0xE8, 0x07, 0x3F, 0x01, 0x49,
                  0x8E, 0x08, 0x3F, 0x01, 0x5E, 0x00, 0x40, 0x01, 0xE8, 0x07});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[519] === result");

  // Array with additional properties.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x41, 0x02, 0x3F, 0x01,
                      0x49, 0x02, 0x3F, 0x01, 0x49, 0x04, 0x3F, 0x01,
                      0x53, 0x03, 0x66, 0x6F, 0x6F, 0x3F, 0x01, 0x53,
                      0x03, 0x62, 0x61, 0x72, 0x24, 0x01, 0x02, 0x00});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.toString() === '1,2'");
  ExpectScriptTrue("result.foo === 'bar'");

  // Sparse array with additional properties.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x61, 0xE8, 0x07, 0x3F, 0x01,
                      0x53, 0x03, 0x66, 0x6F, 0x6F, 0x3F, 0x01, 0x53, 0x03,
                      0x62, 0x61, 0x72, 0x40, 0x01, 0xE8, 0x07, 0x00});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.toString() === ','.repeat(999)");
  ExpectScriptTrue("result.foo === 'bar'");

  // The distinction between holes and undefined elements must be maintained.
  // Note that since the previous output from Chrome fails this test, an
  // encoding using the sparse format was constructed instead.
  value =
      DecodeTest({0xFF, 0x09, 0x61, 0x02, 0x49, 0x02, 0x5F, 0x40, 0x01, 0x02});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[0] === 'undefined'");
  ExpectScriptTrue("typeof result[1] === 'undefined'");
  ExpectScriptTrue("!result.hasOwnProperty(0)");
  ExpectScriptTrue("result.hasOwnProperty(1)");
}

TEST_F(ValueSerializerTest, DecodeInvalidOverLargeArray) {
  // So large it couldn't exist in the V8 heap, and its size couldn't fit in a
  // SMI on 32-bit systems (2^30).
  InvalidDecodeTest({0xFF, 0x09, 0x41, 0x80, 0x80, 0x80, 0x80, 0x04});
  // Not so large, but there isn't enough data left in the buffer.
  InvalidDecodeTest({0xFF, 0x09, 0x41, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripArrayWithNonEnumerableElement) {
  // Even though this array looks like [1,5,3], the 5 should be missing from the
  // perspective of structured clone, which only clones properties that were
  // enumerable.
  Local<Value> value = RoundTripTest(
      "var x = [1,2,3];"
      "Object.defineProperty(x, '1', {enumerable:false, value:5});"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(3u, Array::Cast(*value)->Length());
  ExpectScriptTrue("!result.hasOwnProperty('1')");
}

TEST_F(ValueSerializerTest, RoundTripArrayWithTrickyGetters) {
  // If an element is deleted before it is serialized, then it's deleted.
  Local<Value> value =
      RoundTripTest("var x = [{ get a() { delete x[1]; }}, 42]; x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[1] === 'undefined'");
  ExpectScriptTrue("!result.hasOwnProperty(1)");

  // Same for sparse arrays.
  value = RoundTripTest(
      "var x = [{ get a() { delete x[1]; }}, 42];"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("typeof result[1] === 'undefined'");
  ExpectScriptTrue("!result.hasOwnProperty(1)");

  // If the length is changed, then the resulting array still has the original
  // length, but elements that were not yet serialized are gone.
  value = RoundTripTest("var x = [1, { get a() { x.length = 0; }}, 3, 4]; x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(4u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === 1");
  ExpectScriptTrue("!result.hasOwnProperty(2)");

  // The same is true if the length is shortened, but there are still items
  // remaining.
  value = RoundTripTest("var x = [1, { get a() { x.length = 3; }}, 3, 4]; x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(4u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[2] === 3");
  ExpectScriptTrue("!result.hasOwnProperty(3)");

  // Same for sparse arrays.
  value = RoundTripTest(
      "var x = [1, { get a() { x.length = 0; }}, 3, 4];"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[0] === 1");
  ExpectScriptTrue("!result.hasOwnProperty(2)");

  value = RoundTripTest(
      "var x = [1, { get a() { x.length = 3; }}, 3, 4];"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[2] === 3");
  ExpectScriptTrue("!result.hasOwnProperty(3)");

  // If a getter makes a property non-enumerable, it should still be enumerated
  // as enumeration happens once before getters are invoked.
  value = RoundTripTest(
      "var x = [{ get a() {"
      "  Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "}}, 2];"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[1] === 3");

  // Same for sparse arrays.
  value = RoundTripTest(
      "var x = [{ get a() {"
      "  Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "}}, 2];"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[1] === 3");

  // Getters on the array itself must also run.
  value = RoundTripTest(
      "var x = [1, 2, 3];"
      "Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(3u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[1] === 4");

  // Same for sparse arrays.
  value = RoundTripTest(
      "var x = [1, 2, 3];"
      "Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result[1] === 4");

  // Even with a getter that deletes things, we don't read from the prototype.
  value = RoundTripTest(
      "var x = [{ get a() { delete x[1]; } }, 2];"
      "x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("!(1 in result)");

  // Same for sparse arrays.
  value = RoundTripTest(
      "var x = [{ get a() { delete x[1]; } }, 2];"
      "x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "x.length = 1000;"
      "x;");
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(1000u, Array::Cast(*value)->Length());
  ExpectScriptTrue("!(1 in result)");
}

TEST_F(ValueSerializerTest, DecodeSparseArrayVersion0) {
  // Empty (sparse) array.
  Local<Value> value = DecodeTestForVersion0({0x40, 0x00, 0x00, 0x00});
  ASSERT_TRUE(value->IsArray());
  ASSERT_EQ(0u, Array::Cast(*value)->Length());

  // Sparse array with a mixture of elements and properties.
  value = DecodeTestForVersion0({0x55, 0x00, 0x53, 0x01, 'a',  0x55, 0x02, 0x55,
                                 0x05, 0x53, 0x03, 'f',  'o',  'o',  0x53, 0x03,
                                 'b',  'a',  'r',  0x53, 0x03, 'b',  'a',  'z',
                                 0x49, 0x0B, 0x40, 0x04, 0x03, 0x00});
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(3u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.toString() === 'a,,5'");
  ExpectScriptTrue("!(1 in result)");
  ExpectScriptTrue("result.foo === 'bar'");
  ExpectScriptTrue("result.baz === -6");

  // Sparse array in a sparse array (sanity check of nesting).
  value = DecodeTestForVersion0(
      {0x55, 0x01, 0x55, 0x01, 0x54, 0x40, 0x01, 0x02, 0x40, 0x01, 0x02, 0x00});
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(2u, Array::Cast(*value)->Length());
  ExpectScriptTrue("!(0 in result)");
  ExpectScriptTrue("result[1] instanceof Array");
  ExpectScriptTrue("!(0 in result[1])");
  ExpectScriptTrue("result[1][1] === true");
}

TEST_F(ValueSerializerTest, RoundTripDenseArrayContainingUndefined) {
  // In previous serialization versions, this would be interpreted as an absent
  // property.
  Local<Value> value = RoundTripTest("[undefined]");
  ASSERT_TRUE(value->IsArray());
  EXPECT_EQ(1u, Array::Cast(*value)->Length());
  ExpectScriptTrue("result.hasOwnProperty(0)");
  ExpectScriptTrue("result[0] === undefined");
}

TEST_F(ValueSerializerTest, DecodeDenseArrayContainingUndefined) {
  // In previous versions, "undefined" in a dense array signified absence of the
  // element (for compatibility). In new versions, it has a separate encoding.
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x41, 0x01, 0x5F, 0x24, 0x00, 0x01});
  ExpectScriptTrue("!(0 in result)");

  value = DecodeTest({0xFF, 0x0B, 0x41, 0x01, 0x5F, 0x24, 0x00, 0x01});
  ExpectScriptTrue("0 in result");
  ExpectScriptTrue("result[0] === undefined");

  value = DecodeTest({0xFF, 0x0B, 0x41, 0x01, 0x2D, 0x24, 0x00, 0x01});
  ExpectScriptTrue("!(0 in result)");
}

TEST_F(ValueSerializerTest, RoundTripDate) {
  Local<Value> value = RoundTripTest("new Date(1e6)");
  ASSERT_TRUE(value->IsDate());
  EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Date.prototype");

  value = RoundTripTest("new Date(Date.UTC(1867, 6, 1))");
  ASSERT_TRUE(value->IsDate());
  ExpectScriptTrue("result.toISOString() === '1867-07-01T00:00:00.000Z'");

  value = RoundTripTest("new Date(NaN)");
  ASSERT_TRUE(value->IsDate());
  EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));

  value = RoundTripTest("({ a: new Date(), get b() { return this.a; } })");
  ExpectScriptTrue("result.a instanceof Date");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, DecodeDate) {
  Local<Value> value;
#if defined(V8_TARGET_LITTLE_ENDIAN)
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00,
                      0x80, 0x84, 0x2E, 0x41, 0x00});
  ASSERT_TRUE(value->IsDate());
  EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Date.prototype");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0x00, 0x00, 0x20, 0x45,
                      0x27, 0x89, 0x87, 0xC2, 0x00});
  ASSERT_TRUE(value->IsDate());
  ExpectScriptTrue("result.toISOString() === '1867-07-01T00:00:00.000Z'");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0xF8, 0x7F, 0x00});
  ASSERT_TRUE(value->IsDate());
  EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
#else
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0x41, 0x2E, 0x84, 0x80,
                      0x00, 0x00, 0x00, 0x00, 0x00});
  ASSERT_TRUE(value->IsDate());
  EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Date.prototype");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0xC2, 0x87, 0x89, 0x27,
                      0x45, 0x20, 0x00, 0x00, 0x00});
  ASSERT_TRUE(value->IsDate());
  ExpectScriptTrue("result.toISOString() === '1867-07-01T00:00:00.000Z'");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x44, 0x7F, 0xF8, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00});
  ASSERT_TRUE(value->IsDate());
  EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
#endif
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53,
                      0x01, 0x61, 0x3F, 0x01, 0x44, 0x00, 0x20, 0x39,
                      0x50, 0x37, 0x6A, 0x75, 0x42, 0x3F, 0x02, 0x53,
                      0x01, 0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02});
  ExpectScriptTrue("result.a instanceof Date");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, RoundTripValueObjects) {
  Local<Value> value = RoundTripTest("new Boolean(true)");
  ExpectScriptTrue("Object.getPrototypeOf(result) === Boolean.prototype");
  ExpectScriptTrue("result.valueOf() === true");

  value = RoundTripTest("new Boolean(false)");
  ExpectScriptTrue("Object.getPrototypeOf(result) === Boolean.prototype");
  ExpectScriptTrue("result.valueOf() === false");

  value =
      RoundTripTest("({ a: new Boolean(true), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof Boolean");
  ExpectScriptTrue("result.a === result.b");

  value = RoundTripTest("new Number(-42)");
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("result.valueOf() === -42");

  value = RoundTripTest("new Number(NaN)");
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("Number.isNaN(result.valueOf())");

  value = RoundTripTest("({ a: new Number(6), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof Number");
  ExpectScriptTrue("result.a === result.b");

  value = RoundTripTest("new String('Qu\\xe9bec')");
  ExpectScriptTrue("Object.getPrototypeOf(result) === String.prototype");
  ExpectScriptTrue("result.valueOf() === 'Qu\\xe9bec'");
  ExpectScriptTrue("result.length === 6");

  value = RoundTripTest("new String('\\ud83d\\udc4a')");
  ExpectScriptTrue("Object.getPrototypeOf(result) === String.prototype");
  ExpectScriptTrue("result.valueOf() === '\\ud83d\\udc4a'");
  ExpectScriptTrue("result.length === 2");

  value = RoundTripTest("({ a: new String(), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof String");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, RejectsOtherValueObjects) {
  // This is a roundabout way of getting an instance of Symbol.
  InvalidEncodeTest("Object.valueOf.apply(Symbol())");
}

TEST_F(ValueSerializerTest, DecodeValueObjects) {
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x79, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Boolean.prototype");
  ExpectScriptTrue("result.valueOf() === true");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x78, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Boolean.prototype");
  ExpectScriptTrue("result.valueOf() === false");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53,
                      0x01, 0x61, 0x3F, 0x01, 0x79, 0x3F, 0x02, 0x53,
                      0x01, 0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02});
  ExpectScriptTrue("result.a instanceof Boolean");
  ExpectScriptTrue("result.a === result.b");

#if defined(V8_TARGET_LITTLE_ENDIAN)
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x45, 0xC0, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("result.valueOf() === -42");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6E, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0xF8, 0x7F, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("Number.isNaN(result.valueOf())");
#else
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6E, 0xC0, 0x45, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("result.valueOf() === -42");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6E, 0x7F, 0xF8, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === Number.prototype");
  ExpectScriptTrue("Number.isNaN(result.valueOf())");
#endif
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53,
                      0x01, 0x61, 0x3F, 0x01, 0x6E, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x18, 0x40, 0x3F, 0x02, 0x53,
                      0x01, 0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02});
  ExpectScriptTrue("result.a instanceof Number");
  ExpectScriptTrue("result.a === result.b");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x73, 0x07, 0x51, 0x75, 0xC3,
                      0xA9, 0x62, 0x65, 0x63, 0x00});
  ExpectScriptTrue("Object.getPrototypeOf(result) === String.prototype");
  ExpectScriptTrue("result.valueOf() === 'Qu\\xe9bec'");
  ExpectScriptTrue("result.length === 6");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x73, 0x04, 0xF0, 0x9F, 0x91, 0x8A});
  ExpectScriptTrue("Object.getPrototypeOf(result) === String.prototype");
  ExpectScriptTrue("result.valueOf() === '\\ud83d\\udc4a'");
  ExpectScriptTrue("result.length === 2");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x01,
                      0x61, 0x3F, 0x01, 0x73, 0x00, 0x3F, 0x02, 0x53, 0x01,
                      0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02, 0x00});
  ExpectScriptTrue("result.a instanceof String");
  ExpectScriptTrue("result.a === result.b");

  // String object containing a Latin-1 string.
  value =
      DecodeTest({0xFF, 0x0C, 0x73, 0x22, 0x06, 'Q', 'u', 0xE9, 'b', 'e', 'c'});
  ExpectScriptTrue("Object.getPrototypeOf(result) === String.prototype");
  ExpectScriptTrue("result.valueOf() === 'Qu\\xe9bec'");
  ExpectScriptTrue("result.length === 6");
}

TEST_F(ValueSerializerTest, RoundTripRegExp) {
  Local<Value> value = RoundTripTest("/foo/g");
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("Object.getPrototypeOf(result) === RegExp.prototype");
  ExpectScriptTrue("result.toString() === '/foo/g'");

  value = RoundTripTest("new RegExp('Qu\\xe9bec', 'i')");
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("result.toString() === '/Qu\\xe9bec/i'");

  value = RoundTripTest("new RegExp('\\ud83d\\udc4a', 'ug')");
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("result.toString() === '/\\ud83d\\udc4a/gu'");

  value = RoundTripTest("({ a: /foo/gi, get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof RegExp");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, DecodeRegExp) {
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x52, 0x03, 0x66, 0x6F, 0x6F, 0x01});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("Object.getPrototypeOf(result) === RegExp.prototype");
  ExpectScriptTrue("result.toString() === '/foo/g'");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x52, 0x07, 0x51, 0x75, 0xC3,
                      0xA9, 0x62, 0x65, 0x63, 0x02});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("result.toString() === '/Qu\\xe9bec/i'");

  value = DecodeTest(
      {0xFF, 0x09, 0x3F, 0x00, 0x52, 0x04, 0xF0, 0x9F, 0x91, 0x8A, 0x11, 0x00});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("result.toString() === '/\\ud83d\\udc4a/gu'");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x01, 0x61,
                  0x3F, 0x01, 0x52, 0x03, 0x66, 0x6F, 0x6F, 0x03, 0x3F, 0x02,
                  0x53, 0x01, 0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02, 0x00});
  ExpectScriptTrue("result.a instanceof RegExp");
  ExpectScriptTrue("result.a === result.b");

  // RegExp containing a Latin-1 string.
  value = DecodeTest(
      {0xFF, 0x0C, 0x52, 0x22, 0x06, 'Q', 'u', 0xE9, 'b', 'e', 'c', 0x02});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("result.toString() === '/Qu\\xe9bec/i'");
}

// Tests that invalid flags are not accepted by the deserializer.
TEST_F(ValueSerializerTest, DecodeRegExpDotAll) {
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x52, 0x03, 0x66, 0x6F, 0x6F, 0x1F});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("Object.getPrototypeOf(result) === RegExp.prototype");
  ExpectScriptTrue("result.toString() === '/foo/gimuy'");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x52, 0x03, 0x66, 0x6F, 0x6F, 0x3F});
  ASSERT_TRUE(value->IsRegExp());
  ExpectScriptTrue("Object.getPrototypeOf(result) === RegExp.prototype");
  ExpectScriptTrue("result.toString() === '/foo/gimsuy'");

  InvalidDecodeTest(
      {0xFF, 0x09, 0x3F, 0x00, 0x52, 0x03, 0x66, 0x6F, 0x6F, 0x7F});
}

TEST_F(ValueSerializerTest, RoundTripMap) {
  Local<Value> value = RoundTripTest("var m = new Map(); m.set(42, 'foo'); m;");
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Map.prototype");
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.get(42) === 'foo'");

  value = RoundTripTest("var m = new Map(); m.set(m, m); m;");
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.get(result) === result");

  // Iteration order must be preserved.
  value = RoundTripTest(
      "var m = new Map();"
      "m.set(1, 0); m.set('a', 0); m.set(3, 0); m.set(2, 0);"
      "m;");
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("Array.from(result.keys()).toString() === '1,a,3,2'");
}

TEST_F(ValueSerializerTest, DecodeMap) {
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3B, 0x3F, 0x01, 0x49, 0x54, 0x3F,
                  0x01, 0x53, 0x03, 0x66, 0x6F, 0x6F, 0x3A, 0x02});
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Map.prototype");
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.get(42) === 'foo'");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3B, 0x3F, 0x01, 0x5E, 0x00,
                      0x3F, 0x01, 0x5E, 0x00, 0x3A, 0x02, 0x00});
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.get(result) === result");

  // Iteration order must be preserved.
  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3B, 0x3F, 0x01, 0x49, 0x02, 0x3F,
                  0x01, 0x49, 0x00, 0x3F, 0x01, 0x53, 0x01, 0x61, 0x3F, 0x01,
                  0x49, 0x00, 0x3F, 0x01, 0x49, 0x06, 0x3F, 0x01, 0x49, 0x00,
                  0x3F, 0x01, 0x49, 0x04, 0x3F, 0x01, 0x49, 0x00, 0x3A, 0x08});
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("Array.from(result.keys()).toString() === '1,a,3,2'");
}

TEST_F(ValueSerializerTest, RoundTripMapWithTrickyGetters) {
  // Even if an entry is removed or reassigned, the original key/value pair is
  // used.
  Local<Value> value = RoundTripTest(
      "var m = new Map();"
      "m.set(0, { get a() {"
      "  m.delete(1); m.set(2, 'baz'); m.set(3, 'quux');"
      "}});"
      "m.set(1, 'foo');"
      "m.set(2, 'bar');"
      "m;");
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("Array.from(result.keys()).toString() === '0,1,2'");
  ExpectScriptTrue("result.get(1) === 'foo'");
  ExpectScriptTrue("result.get(2) === 'bar'");

  // However, deeper modifications of objects yet to be serialized still apply.
  value = RoundTripTest(
      "var m = new Map();"
      "var key = { get a() { value.foo = 'bar'; } };"
      "var value = { get a() { key.baz = 'quux'; } };"
      "m.set(key, value);"
      "m;");
  ASSERT_TRUE(value->IsMap());
  ExpectScriptTrue("!('baz' in Array.from(result.keys())[0])");
  ExpectScriptTrue("Array.from(result.values())[0].foo === 'bar'");
}

TEST_F(ValueSerializerTest, RoundTripSet) {
  Local<Value> value =
      RoundTripTest("var s = new Set(); s.add(42); s.add('foo'); s;");
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Set.prototype");
  ExpectScriptTrue("result.size === 2");
  ExpectScriptTrue("result.has(42)");
  ExpectScriptTrue("result.has('foo')");

  value = RoundTripTest("var s = new Set(); s.add(s); s;");
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.has(result)");

  // Iteration order must be preserved.
  value = RoundTripTest(
      "var s = new Set();"
      "s.add(1); s.add('a'); s.add(3); s.add(2);"
      "s;");
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("Array.from(result.keys()).toString() === '1,a,3,2'");
}

TEST_F(ValueSerializerTest, DecodeSet) {
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x27, 0x3F, 0x01, 0x49, 0x54, 0x3F,
                  0x01, 0x53, 0x03, 0x66, 0x6F, 0x6F, 0x2C, 0x02});
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Set.prototype");
  ExpectScriptTrue("result.size === 2");
  ExpectScriptTrue("result.has(42)");
  ExpectScriptTrue("result.has('foo')");

  value = DecodeTest(
      {0xFF, 0x09, 0x3F, 0x00, 0x27, 0x3F, 0x01, 0x5E, 0x00, 0x2C, 0x01, 0x00});
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("result.size === 1");
  ExpectScriptTrue("result.has(result)");

  // Iteration order must be preserved.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x27, 0x3F, 0x01, 0x49,
                      0x02, 0x3F, 0x01, 0x53, 0x01, 0x61, 0x3F, 0x01,
                      0x49, 0x06, 0x3F, 0x01, 0x49, 0x04, 0x2C, 0x04});
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("Array.from(result.keys()).toString() === '1,a,3,2'");
}

TEST_F(ValueSerializerTest, RoundTripSetWithTrickyGetters) {
  // Even if an element is added or removed during serialization, the original
  // set of elements is used.
  Local<Value> value = RoundTripTest(
      "var s = new Set();"
      "s.add({ get a() { s.delete(1); s.add(2); } });"
      "s.add(1);"
      "s;");
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue(
      "Array.from(result.keys()).toString() === '[object Object],1'");

  // However, deeper modifications of objects yet to be serialized still apply.
  value = RoundTripTest(
      "var s = new Set();"
      "var first = { get a() { second.foo = 'bar'; } };"
      "var second = { get a() { first.baz = 'quux'; } };"
      "s.add(first);"
      "s.add(second);"
      "s;");
  ASSERT_TRUE(value->IsSet());
  ExpectScriptTrue("!('baz' in Array.from(result.keys())[0])");
  ExpectScriptTrue("Array.from(result.keys())[1].foo === 'bar'");
}

TEST_F(ValueSerializerTest, RoundTripArrayBuffer) {
  Local<Value> value = RoundTripTest("new ArrayBuffer()");
  ASSERT_TRUE(value->IsArrayBuffer());
  EXPECT_EQ(0u, ArrayBuffer::Cast(*value)->ByteLength());
  ExpectScriptTrue("Object.getPrototypeOf(result) === ArrayBuffer.prototype");

  value = RoundTripTest("new Uint8Array([0, 128, 255]).buffer");
  ASSERT_TRUE(value->IsArrayBuffer());
  EXPECT_EQ(3u, ArrayBuffer::Cast(*value)->ByteLength());
  ExpectScriptTrue("new Uint8Array(result).toString() === '0,128,255'");

  value =
      RoundTripTest("({ a: new ArrayBuffer(), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof ArrayBuffer");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, DecodeArrayBuffer) {
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x42, 0x00});
  ASSERT_TRUE(value->IsArrayBuffer());
  EXPECT_EQ(0u, ArrayBuffer::Cast(*value)->ByteLength());
  ExpectScriptTrue("Object.getPrototypeOf(result) === ArrayBuffer.prototype");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x42, 0x03, 0x00, 0x80, 0xFF, 0x00});
  ASSERT_TRUE(value->IsArrayBuffer());
  EXPECT_EQ(3u, ArrayBuffer::Cast(*value)->ByteLength());
  ExpectScriptTrue("new Uint8Array(result).toString() === '0,128,255'");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x01,
                      0x61, 0x3F, 0x01, 0x42, 0x00, 0x3F, 0x02, 0x53, 0x01,
                      0x62, 0x3F, 0x02, 0x5E, 0x01, 0x7B, 0x02, 0x00});
  ExpectScriptTrue("result.a instanceof ArrayBuffer");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTest, DecodeInvalidArrayBuffer) {
  InvalidDecodeTest({0xFF, 0x09, 0x42, 0xFF, 0xFF, 0x00});
}

// An array buffer allocator that never has available memory.
class OOMArrayBufferAllocator : public ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t) override { return nullptr; }
  void* AllocateUninitialized(size_t) override { return nullptr; }
  void Free(void*, size_t) override {}
};

TEST_F(ValueSerializerTest, DecodeArrayBufferOOM) {
  // This test uses less of the harness, because it has to customize the
  // isolate.
  OOMArrayBufferAllocator allocator;
  Isolate::CreateParams params;
  params.array_buffer_allocator = &allocator;
  Isolate* isolate = Isolate::New(params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> context = Context::New(isolate);
    Context::Scope context_scope(context);
    TryCatch try_catch(isolate);

    const std::vector<uint8_t> data = {0xFF, 0x09, 0x3F, 0x00, 0x42,
                                       0x03, 0x00, 0x80, 0xFF, 0x00};
    ValueDeserializer deserializer(isolate, &data[0],
                                   static_cast<int>(data.size()), nullptr);
    deserializer.SetSupportsLegacyWireFormat(true);
    ASSERT_TRUE(deserializer.ReadHeader(context).FromMaybe(false));
    ASSERT_FALSE(try_catch.HasCaught());
    EXPECT_TRUE(deserializer.ReadValue(context).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }
  isolate->Dispose();
}

// Includes an ArrayBuffer wrapper marked for transfer from the serialization
// context to the deserialization context.
class ValueSerializerTestWithArrayBufferTransfer : public ValueSerializerTest {
 protected:
  static const size_t kTestByteLength = 4;

  ValueSerializerTestWithArrayBufferTransfer() {
    {
      Context::Scope scope(serialization_context());
      input_buffer_ = ArrayBuffer::New(isolate(), nullptr, 0);
    }
    {
      Context::Scope scope(deserialization_context());
      output_buffer_ = ArrayBuffer::New(isolate(), kTestByteLength);
      const uint8_t data[kTestByteLength] = {0x00, 0x01, 0x80, 0xFF};
      memcpy(output_buffer_->GetContents().Data(), data, kTestByteLength);
    }
  }

  const Local<ArrayBuffer>& input_buffer() { return input_buffer_; }
  const Local<ArrayBuffer>& output_buffer() { return output_buffer_; }

  void BeforeEncode(ValueSerializer* serializer) override {
    serializer->TransferArrayBuffer(0, input_buffer_);
  }

  void BeforeDecode(ValueDeserializer* deserializer) override {
    deserializer->TransferArrayBuffer(0, output_buffer_);
  }

 private:
  Local<ArrayBuffer> input_buffer_;
  Local<ArrayBuffer> output_buffer_;
};

TEST_F(ValueSerializerTestWithArrayBufferTransfer,
       RoundTripArrayBufferTransfer) {
  Local<Value> value = RoundTripTest(input_buffer());
  ASSERT_TRUE(value->IsArrayBuffer());
  EXPECT_EQ(output_buffer(), value);
  ExpectScriptTrue("new Uint8Array(result).toString() === '0,1,128,255'");

  Local<Object> object;
  {
    Context::Scope scope(serialization_context());
    object = Object::New(isolate());
    EXPECT_TRUE(object
                    ->CreateDataProperty(serialization_context(),
                                         StringFromUtf8("a"), input_buffer())
                    .FromMaybe(false));
    EXPECT_TRUE(object
                    ->CreateDataProperty(serialization_context(),
                                         StringFromUtf8("b"), input_buffer())
                    .FromMaybe(false));
  }
  value = RoundTripTest(object);
  ExpectScriptTrue("result.a instanceof ArrayBuffer");
  ExpectScriptTrue("result.a === result.b");
  ExpectScriptTrue("new Uint8Array(result.a).toString() === '0,1,128,255'");
}

TEST_F(ValueSerializerTest, RoundTripTypedArray) {
  // Check that the right type comes out the other side for every kind of typed
  // array.
  Local<Value> value;
#define TYPED_ARRAY_ROUND_TRIP_TEST(Type, type, TYPE, ctype)             \
  value = RoundTripTest("new " #Type "Array(2)");                        \
  ASSERT_TRUE(value->Is##Type##Array());                                 \
  EXPECT_EQ(2u * sizeof(ctype), TypedArray::Cast(*value)->ByteLength()); \
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());                     \
  ExpectScriptTrue("Object.getPrototypeOf(result) === " #Type            \
                   "Array.prototype");

  TYPED_ARRAYS(TYPED_ARRAY_ROUND_TRIP_TEST)
#undef TYPED_ARRAY_ROUND_TRIP_TEST

  // Check that values of various kinds are suitably preserved.
  value = RoundTripTest("new Uint8Array([1, 128, 255])");
  ExpectScriptTrue("result.toString() === '1,128,255'");

  value = RoundTripTest("new Int16Array([0, 256, -32768])");
  ExpectScriptTrue("result.toString() === '0,256,-32768'");

  value = RoundTripTest("new Float32Array([0, -0.5, NaN, Infinity])");
  ExpectScriptTrue("result.toString() === '0,-0.5,NaN,Infinity'");

  // Array buffer views sharing a buffer should do so on the other side.
  // Similarly, multiple references to the same typed array should be resolved.
  value = RoundTripTest(
      "var buffer = new ArrayBuffer(32);"
      "({"
      "  u8: new Uint8Array(buffer),"
      "  get u8_2() { return this.u8; },"
      "  f32: new Float32Array(buffer, 4, 5),"
      "  b: buffer,"
      "});");
  ExpectScriptTrue("result.u8 instanceof Uint8Array");
  ExpectScriptTrue("result.u8 === result.u8_2");
  ExpectScriptTrue("result.f32 instanceof Float32Array");
  ExpectScriptTrue("result.u8.buffer === result.f32.buffer");
  ExpectScriptTrue("result.f32.byteOffset === 4");
  ExpectScriptTrue("result.f32.length === 5");
}

TEST_F(ValueSerializerTest, DecodeTypedArray) {
  // Check that the right type comes out the other side for every kind of typed
  // array.
  Local<Value> value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42,
                                   0x02, 0x00, 0x00, 0x56, 0x42, 0x00, 0x02});
  ASSERT_TRUE(value->IsUint8Array());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Uint8Array.prototype");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x02, 0x00,
                      0x00, 0x56, 0x62, 0x00, 0x02});
  ASSERT_TRUE(value->IsInt8Array());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Int8Array.prototype");

#if defined(V8_TARGET_LITTLE_ENDIAN)
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x04, 0x00,
                      0x00, 0x00, 0x00, 0x56, 0x57, 0x00, 0x04});
  ASSERT_TRUE(value->IsUint16Array());
  EXPECT_EQ(4u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Uint16Array.prototype");

  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x04, 0x00,
                      0x00, 0x00, 0x00, 0x56, 0x77, 0x00, 0x04});
  ASSERT_TRUE(value->IsInt16Array());
  EXPECT_EQ(4u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Int16Array.prototype");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x08, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x44, 0x00, 0x08});
  ASSERT_TRUE(value->IsUint32Array());
  EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Uint32Array.prototype");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x08, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x64, 0x00, 0x08});
  ASSERT_TRUE(value->IsInt32Array());
  EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Int32Array.prototype");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x08, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x66, 0x00, 0x08});
  ASSERT_TRUE(value->IsFloat32Array());
  EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Float32Array.prototype");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x10, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x56, 0x46, 0x00, 0x10});
  ASSERT_TRUE(value->IsFloat64Array());
  EXPECT_EQ(16u, TypedArray::Cast(*value)->ByteLength());
  EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
  ExpectScriptTrue("Object.getPrototypeOf(result) === Float64Array.prototype");

#endif  // V8_TARGET_LITTLE_ENDIAN

  // Check that values of various kinds are suitably preserved.
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x03, 0x01,
                      0x80, 0xFF, 0x56, 0x42, 0x00, 0x03, 0x00});
  ExpectScriptTrue("result.toString() === '1,128,255'");

#if defined(V8_TARGET_LITTLE_ENDIAN)
  value = DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x06, 0x00,
                      0x00, 0x00, 0x01, 0x00, 0x80, 0x56, 0x77, 0x00, 0x06});
  ExpectScriptTrue("result.toString() === '0,256,-32768'");

  value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x10, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0xBF, 0x00, 0x00, 0xC0, 0x7F,
                  0x00, 0x00, 0x80, 0x7F, 0x56, 0x66, 0x00, 0x10});
  ExpectScriptTrue("result.toString() === '0,-0.5,NaN,Infinity'");

#endif  // V8_TARGET_LITTLE_ENDIAN

  // Array buffer views sharing a buffer should do so on the other side.
  // Similarly, multiple references to the same typed array should be resolved.
  value = DecodeTest(
      {0xFF, 0x09, 0x3F, 0x00, 0x6F, 0x3F, 0x01, 0x53, 0x02, 0x75, 0x38, 0x3F,
       0x01, 0x3F, 0x01, 0x42, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x56, 0x42, 0x00, 0x20, 0x3F, 0x03, 0x53, 0x04, 0x75, 0x38, 0x5F,
       0x32, 0x3F, 0x03, 0x5E, 0x02, 0x3F, 0x03, 0x53, 0x03, 0x66, 0x33, 0x32,
       0x3F, 0x03, 0x3F, 0x03, 0x5E, 0x01, 0x56, 0x66, 0x04, 0x14, 0x3F, 0x04,
       0x53, 0x01, 0x62, 0x3F, 0x04, 0x5E, 0x01, 0x7B, 0x04, 0x00});
  ExpectScriptTrue("result.u8 instanceof Uint8Array");
  ExpectScriptTrue("result.u8 === result.u8_2");
  ExpectScriptTrue("result.f32 instanceof Float32Array");
  ExpectScriptTrue("result.u8.buffer === result.f32.buffer");
  ExpectScriptTrue("result.f32.byteOffset === 4");
  ExpectScriptTrue("result.f32.length === 5");
}

TEST_F(ValueSerializerTest, DecodeInvalidTypedArray) {
  // Byte offset out of range.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x42, 0x03, 0x01});
  // Byte offset in range, offset + length out of range.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x42, 0x01, 0x03});
  // Byte offset not divisible by element size.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0x56, 0x77, 0x01, 0x02});
  // Byte length not divisible by element size.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0x56, 0x77, 0x02, 0x01});
  // Invalid view type (0xFF).
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0xFF, 0x01, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripDataView) {
  Local<Value> value = RoundTripTest("new DataView(new ArrayBuffer(4), 1, 2)");
  ASSERT_TRUE(value->IsDataView());
  EXPECT_EQ(1u, DataView::Cast(*value)->ByteOffset());
  EXPECT_EQ(2u, DataView::Cast(*value)->ByteLength());
  EXPECT_EQ(4u, DataView::Cast(*value)->Buffer()->ByteLength());
  ExpectScriptTrue("Object.getPrototypeOf(result) === DataView.prototype");
}

TEST_F(ValueSerializerTest, DecodeDataView) {
  Local<Value> value =
      DecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x3F, 0x00, 0x42, 0x04, 0x00, 0x00,
                  0x00, 0x00, 0x56, 0x3F, 0x01, 0x02});
  ASSERT_TRUE(value->IsDataView());
  EXPECT_EQ(1u, DataView::Cast(*value)->ByteOffset());
  EXPECT_EQ(2u, DataView::Cast(*value)->ByteLength());
  EXPECT_EQ(4u, DataView::Cast(*value)->Buffer()->ByteLength());
  ExpectScriptTrue("Object.getPrototypeOf(result) === DataView.prototype");
}

TEST_F(ValueSerializerTest, DecodeArrayWithLengthProperty1) {
  ASSERT_DEATH_IF_SUPPORTED(
      DecodeTest({0xff, 0x0d, 0x41, 0x03, 0x49, 0x02, 0x49, 0x04,
                  0x49, 0x06, 0x22, 0x06, 0x6c, 0x65, 0x6e, 0x67,
                  0x74, 0x68, 0x49, 0x02, 0x24, 0x01, 0x03}),
      ".*LookupIterator::NOT_FOUND == it.state\\(\\).*");
}

TEST_F(ValueSerializerTest, DecodeArrayWithLengthProperty2) {
  ASSERT_DEATH_IF_SUPPORTED(
      DecodeTest({0xff, 0x0d, 0x41, 0x03, 0x49, 0x02, 0x49, 0x04,
                  0x49, 0x06, 0x22, 0x06, 0x6c, 0x65, 0x6e, 0x67,
                  0x74, 0x68, 0x6f, 0x7b, 0x00, 0x24, 0x01, 0x03}),
      ".*LookupIterator::NOT_FOUND == it.state\\(\\).*");
}

TEST_F(ValueSerializerTest, DecodeInvalidDataView) {
  // Byte offset out of range.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x3F, 0x03, 0x01});
  // Byte offset in range, offset + length out of range.
  InvalidDecodeTest(
      {0xFF, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x3F, 0x01, 0x03});
}

class ValueSerializerTestWithSharedArrayBufferClone
    : public ValueSerializerTest {
 protected:
  ValueSerializerTestWithSharedArrayBufferClone()
      : serializer_delegate_(this), deserializer_delegate_(this) {}

  void InitializeData(const std::vector<uint8_t>& data) {
    data_ = data;
    {
      Context::Scope scope(serialization_context());
      input_buffer_ =
          SharedArrayBuffer::New(isolate(), data_.data(), data_.size());
    }
    {
      Context::Scope scope(deserialization_context());
      output_buffer_ =
          SharedArrayBuffer::New(isolate(), data_.data(), data_.size());
    }
  }

  const Local<SharedArrayBuffer>& input_buffer() { return input_buffer_; }
  const Local<SharedArrayBuffer>& output_buffer() { return output_buffer_; }

  static void SetUpTestCase() {
    flag_was_enabled_ = i::FLAG_harmony_sharedarraybuffer;
    i::FLAG_harmony_sharedarraybuffer = true;
    ValueSerializerTest::SetUpTestCase();
  }

  static void TearDownTestCase() {
    ValueSerializerTest::TearDownTestCase();
    i::FLAG_harmony_sharedarraybuffer = flag_was_enabled_;
    flag_was_enabled_ = false;
  }

 protected:
// GMock doesn't use the "override" keyword.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

  class SerializerDelegate : public ValueSerializer::Delegate {
   public:
    explicit SerializerDelegate(
        ValueSerializerTestWithSharedArrayBufferClone* test)
        : test_(test) {}
    MOCK_METHOD2(GetSharedArrayBufferId,
                 Maybe<uint32_t>(Isolate* isolate,
                                 Local<SharedArrayBuffer> shared_array_buffer));
    MOCK_METHOD2(GetSharedArrayBufferFromId,
                 MaybeLocal<SharedArrayBuffer>(Isolate* isolate, uint32_t id));
    void ThrowDataCloneError(Local<String> message) override {
      test_->isolate()->ThrowException(Exception::Error(message));
    }

   private:
    ValueSerializerTestWithSharedArrayBufferClone* test_;
  };

  class DeserializerDelegate : public ValueDeserializer::Delegate {
   public:
    explicit DeserializerDelegate(
        ValueSerializerTestWithSharedArrayBufferClone* test) {}
    MOCK_METHOD2(GetSharedArrayBufferFromId,
                 MaybeLocal<SharedArrayBuffer>(Isolate* isolate, uint32_t id));
  };

#if __clang__
#pragma clang diagnostic pop
#endif

  ValueSerializer::Delegate* GetSerializerDelegate() override {
    return &serializer_delegate_;
  }

  ValueDeserializer::Delegate* GetDeserializerDelegate() override {
    return &deserializer_delegate_;
  }

  SerializerDelegate serializer_delegate_;
  DeserializerDelegate deserializer_delegate_;

 private:
  static bool flag_was_enabled_;
  std::vector<uint8_t> data_;
  Local<SharedArrayBuffer> input_buffer_;
  Local<SharedArrayBuffer> output_buffer_;
};

bool ValueSerializerTestWithSharedArrayBufferClone::flag_was_enabled_ = false;

TEST_F(ValueSerializerTestWithSharedArrayBufferClone,
       RoundTripSharedArrayBufferClone) {
  InitializeData({0x00, 0x01, 0x80, 0xFF});

  EXPECT_CALL(serializer_delegate_,
              GetSharedArrayBufferId(isolate(), input_buffer()))
      .WillRepeatedly(Return(Just(0U)));
  EXPECT_CALL(deserializer_delegate_, GetSharedArrayBufferFromId(isolate(), 0U))
      .WillRepeatedly(Return(output_buffer()));

  Local<Value> value = RoundTripTest(input_buffer());
  ASSERT_TRUE(value->IsSharedArrayBuffer());
  EXPECT_EQ(output_buffer(), value);
  ExpectScriptTrue("new Uint8Array(result).toString() === '0,1,128,255'");

  Local<Object> object;
  {
    Context::Scope scope(serialization_context());
    object = Object::New(isolate());
    EXPECT_TRUE(object
                    ->CreateDataProperty(serialization_context(),
                                         StringFromUtf8("a"), input_buffer())
                    .FromMaybe(false));
    EXPECT_TRUE(object
                    ->CreateDataProperty(serialization_context(),
                                         StringFromUtf8("b"), input_buffer())
                    .FromMaybe(false));
  }
  value = RoundTripTest(object);
  ExpectScriptTrue("result.a instanceof SharedArrayBuffer");
  ExpectScriptTrue("result.a === result.b");
  ExpectScriptTrue("new Uint8Array(result.a).toString() === '0,1,128,255'");
}

TEST_F(ValueSerializerTestWithSharedArrayBufferClone,
       RoundTripWebAssemblyMemory) {
  bool flag_was_enabled = i::FLAG_experimental_wasm_threads;
  i::FLAG_experimental_wasm_threads = true;

  std::vector<uint8_t> data = {0x00, 0x01, 0x80, 0xFF};
  data.resize(65536);
  InitializeData(data);

  EXPECT_CALL(serializer_delegate_,
              GetSharedArrayBufferId(isolate(), input_buffer()))
      .WillRepeatedly(Return(Just(0U)));
  EXPECT_CALL(deserializer_delegate_, GetSharedArrayBufferFromId(isolate(), 0U))
      .WillRepeatedly(Return(output_buffer()));

  Local<Value> input;
  {
    Context::Scope scope(serialization_context());
    const int32_t kMaxPages = 1;
    i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate());
    i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(*input_buffer());
    input = Utils::Convert<i::WasmMemoryObject, Value>(
        i::WasmMemoryObject::New(i_isolate, obj, kMaxPages));
  }
  RoundTripTest(input);
  ExpectScriptTrue("result instanceof WebAssembly.Memory");
  ExpectScriptTrue("result.buffer.byteLength === 65536");
  ExpectScriptTrue(
      "new Uint8Array(result.buffer, 0, 4).toString() === '0,1,128,255'");

  i::FLAG_experimental_wasm_threads = flag_was_enabled;
}

TEST_F(ValueSerializerTest, UnsupportedHostObject) {
  InvalidEncodeTest("new ExampleHostObject()");
  InvalidEncodeTest("({ a: new ExampleHostObject() })");
}

class ValueSerializerTestWithHostObject : public ValueSerializerTest {
 protected:
  ValueSerializerTestWithHostObject() : serializer_delegate_(this) {}

  static const uint8_t kExampleHostObjectTag;

  void WriteExampleHostObjectTag() {
    serializer_->WriteRawBytes(&kExampleHostObjectTag, 1);
  }

  bool ReadExampleHostObjectTag() {
    const void* tag;
    return deserializer_->ReadRawBytes(1, &tag) &&
           *reinterpret_cast<const uint8_t*>(tag) == kExampleHostObjectTag;
  }

// GMock doesn't use the "override" keyword.
#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

  class SerializerDelegate : public ValueSerializer::Delegate {
   public:
    explicit SerializerDelegate(ValueSerializerTestWithHostObject* test)
        : test_(test) {}
    MOCK_METHOD2(WriteHostObject,
                 Maybe<bool>(Isolate* isolate, Local<Object> object));
    void ThrowDataCloneError(Local<String> message) override {
      test_->isolate()->ThrowException(Exception::Error(message));
    }

   private:
    ValueSerializerTestWithHostObject* test_;
  };

  class DeserializerDelegate : public ValueDeserializer::Delegate {
   public:
    MOCK_METHOD1(ReadHostObject, MaybeLocal<Object>(Isolate* isolate));
  };

#if __clang__
#pragma clang diagnostic pop
#endif

  ValueSerializer::Delegate* GetSerializerDelegate() override {
    return &serializer_delegate_;
  }
  void BeforeEncode(ValueSerializer* serializer) override {
    serializer_ = serializer;
  }
  ValueDeserializer::Delegate* GetDeserializerDelegate() override {
    return &deserializer_delegate_;
  }
  void BeforeDecode(ValueDeserializer* deserializer) override {
    deserializer_ = deserializer;
  }

  SerializerDelegate serializer_delegate_;
  DeserializerDelegate deserializer_delegate_;
  ValueSerializer* serializer_;
  ValueDeserializer* deserializer_;

  friend class SerializerDelegate;
  friend class DeserializerDelegate;
};

// This is a tag that is used in V8. Using this ensures that we have separate
// tag namespaces.
const uint8_t ValueSerializerTestWithHostObject::kExampleHostObjectTag = 'T';

TEST_F(ValueSerializerTestWithHostObject, RoundTripUint32) {
  // The host can serialize data as uint32_t.
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillRepeatedly(Invoke([this](Isolate*, Local<Object> object) {
        uint32_t value = 0;
        EXPECT_TRUE(object->GetInternalField(0)
                        ->Uint32Value(serialization_context())
                        .To(&value));
        WriteExampleHostObjectTag();
        serializer_->WriteUint32(value);
        return Just(true);
      }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        uint32_t value = 0;
        EXPECT_TRUE(deserializer_->ReadUint32(&value));
        Local<Value> argv[] = {Integer::NewFromUnsigned(isolate(), value)};
        return NewHostObject(deserialization_context(), arraysize(argv), argv);
      }));
  Local<Value> value = RoundTripTest("new ExampleHostObject(42)");
  ASSERT_TRUE(value->IsObject());
  ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
  ExpectScriptTrue(
      "Object.getPrototypeOf(result) === ExampleHostObject.prototype");
  ExpectScriptTrue("result.value === 42");

  value = RoundTripTest("new ExampleHostObject(0xCAFECAFE)");
  ExpectScriptTrue("result.value === 0xCAFECAFE");
}

TEST_F(ValueSerializerTestWithHostObject, RoundTripUint64) {
  // The host can serialize data as uint64_t.
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillRepeatedly(Invoke([this](Isolate*, Local<Object> object) {
        uint32_t value = 0, value2 = 0;
        EXPECT_TRUE(object->GetInternalField(0)
                        ->Uint32Value(serialization_context())
                        .To(&value));
        EXPECT_TRUE(object->GetInternalField(1)
                        ->Uint32Value(serialization_context())
                        .To(&value2));
        WriteExampleHostObjectTag();
        serializer_->WriteUint64((static_cast<uint64_t>(value) << 32) | value2);
        return Just(true);
      }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        uint64_t value_packed;
        EXPECT_TRUE(deserializer_->ReadUint64(&value_packed));
        Local<Value> argv[] = {
            Integer::NewFromUnsigned(isolate(),
                                     static_cast<uint32_t>(value_packed >> 32)),
            Integer::NewFromUnsigned(isolate(),
                                     static_cast<uint32_t>(value_packed))};
        return NewHostObject(deserialization_context(), arraysize(argv), argv);
      }));
  Local<Value> value = RoundTripTest("new ExampleHostObject(42, 0)");
  ASSERT_TRUE(value->IsObject());
  ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
  ExpectScriptTrue(
      "Object.getPrototypeOf(result) === ExampleHostObject.prototype");
  ExpectScriptTrue("result.value === 42");
  ExpectScriptTrue("result.value2 === 0");

  value = RoundTripTest("new ExampleHostObject(0xFFFFFFFF, 0x12345678)");
  ExpectScriptTrue("result.value === 0xFFFFFFFF");
  ExpectScriptTrue("result.value2 === 0x12345678");
}

TEST_F(ValueSerializerTestWithHostObject, RoundTripDouble) {
  // The host can serialize data as double.
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillRepeatedly(Invoke([this](Isolate*, Local<Object> object) {
        double value = 0;
        EXPECT_TRUE(object->GetInternalField(0)
                        ->NumberValue(serialization_context())
                        .To(&value));
        WriteExampleHostObjectTag();
        serializer_->WriteDouble(value);
        return Just(true);
      }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        double value = 0;
        EXPECT_TRUE(deserializer_->ReadDouble(&value));
        Local<Value> argv[] = {Number::New(isolate(), value)};
        return NewHostObject(deserialization_context(), arraysize(argv), argv);
      }));
  Local<Value> value = RoundTripTest("new ExampleHostObject(-3.5)");
  ASSERT_TRUE(value->IsObject());
  ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
  ExpectScriptTrue(
      "Object.getPrototypeOf(result) === ExampleHostObject.prototype");
  ExpectScriptTrue("result.value === -3.5");

  value = RoundTripTest("new ExampleHostObject(NaN)");
  ExpectScriptTrue("Number.isNaN(result.value)");

  value = RoundTripTest("new ExampleHostObject(Infinity)");
  ExpectScriptTrue("result.value === Infinity");

  value = RoundTripTest("new ExampleHostObject(-0)");
  ExpectScriptTrue("1/result.value === -Infinity");
}

TEST_F(ValueSerializerTestWithHostObject, RoundTripRawBytes) {
  // The host can serialize arbitrary raw bytes.
  const struct {
    uint64_t u64;
    uint32_t u32;
    char str[12];
  } sample_data = {0x1234567812345678, 0x87654321, "Hello world"};
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillRepeatedly(
          Invoke([this, &sample_data](Isolate*, Local<Object> object) {
            WriteExampleHostObjectTag();
            serializer_->WriteRawBytes(&sample_data, sizeof(sample_data));
            return Just(true);
          }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this, &sample_data](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        const void* copied_data = nullptr;
        EXPECT_TRUE(
            deserializer_->ReadRawBytes(sizeof(sample_data), &copied_data));
        if (copied_data) {
          EXPECT_EQ(0, memcmp(&sample_data, copied_data, sizeof(sample_data)));
        }
        return NewHostObject(deserialization_context(), 0, nullptr);
      }));
  Local<Value> value = RoundTripTest("new ExampleHostObject()");
  ASSERT_TRUE(value->IsObject());
  ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
  ExpectScriptTrue(
      "Object.getPrototypeOf(result) === ExampleHostObject.prototype");
}

TEST_F(ValueSerializerTestWithHostObject, RoundTripSameObject) {
  // If the same object exists in two places, the delegate should be invoked
  // only once, and the objects should be the same (by reference equality) on
  // the other side.
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillOnce(Invoke([this](Isolate*, Local<Object> object) {
        WriteExampleHostObjectTag();
        return Just(true);
      }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillOnce(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        return NewHostObject(deserialization_context(), 0, nullptr);
      }));
  RoundTripTest("({ a: new ExampleHostObject(), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof ExampleHostObject");
  ExpectScriptTrue("result.a === result.b");
}

TEST_F(ValueSerializerTestWithHostObject, DecodeSimpleHostObject) {
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        return NewHostObject(deserialization_context(), 0, nullptr);
      }));
  DecodeTest({0xFF, 0x0D, 0x5C, kExampleHostObjectTag});
  ExpectScriptTrue(
      "Object.getPrototypeOf(result) === ExampleHostObject.prototype");
}

class ValueSerializerTestWithHostArrayBufferView
    : public ValueSerializerTestWithHostObject {
 protected:
  void BeforeEncode(ValueSerializer* serializer) override {
    ValueSerializerTestWithHostObject::BeforeEncode(serializer);
    serializer_->SetTreatArrayBufferViewsAsHostObjects(true);
  }
};

TEST_F(ValueSerializerTestWithHostArrayBufferView, RoundTripUint8ArrayInput) {
  EXPECT_CALL(serializer_delegate_, WriteHostObject(isolate(), _))
      .WillOnce(Invoke([this](Isolate*, Local<Object> object) {
        EXPECT_TRUE(object->IsUint8Array());
        WriteExampleHostObjectTag();
        return Just(true);
      }));
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillOnce(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        return NewDummyUint8Array();
      }));
  RoundTripTest(
      "({ a: new Uint8Array([1, 2, 3]), get b() { return this.a; }})");
  ExpectScriptTrue("result.a instanceof Uint8Array");
  ExpectScriptTrue("result.a.toString() === '4,5,6'");
  ExpectScriptTrue("result.a === result.b");
}

// It's expected that WebAssembly has more exhaustive tests elsewhere; this
// mostly checks that the logic to embed it in structured clone serialization
// works correctly.

// A simple module which exports an "increment" function.
// Copied from test/mjsunit/wasm/incrementer.wasm.
const unsigned char kIncrementerWasm[] = {
    0,   97, 115, 109, 1, 0,  0, 0, 1,   6,   1,  96,  1,   127, 1,   127,
    3,   2,  1,   0,   7, 13, 1, 9, 105, 110, 99, 114, 101, 109, 101, 110,
    116, 0,  0,   10,  9, 1,  7, 0, 32,  0,   65, 1,   106, 11,
};

class ValueSerializerTestWithWasm : public ValueSerializerTest {
 public:
  static const char* kUnsupportedSerialization;

  ValueSerializerTestWithWasm()
      : serialize_delegate_(&transfer_modules_),
        deserialize_delegate_(&transfer_modules_) {}

  void Reset() {
    current_serializer_delegate_ = nullptr;
    transfer_modules_.clear();
    SetExpectInlineWasm(false);
  }

  void EnableTransferSerialization() {
    current_serializer_delegate_ = &serialize_delegate_;
  }

  void EnableTransferDeserialization() {
    current_deserializer_delegate_ = &deserialize_delegate_;
  }

  void EnableThrowingSerializer() {
    current_serializer_delegate_ = &throwing_serializer_;
  }

  void EnableDefaultDeserializer() {
    current_deserializer_delegate_ = &default_deserializer_;
  }

 protected:
  static void SetUpTestCase() {
    g_saved_flag = i::FLAG_expose_wasm;
    i::FLAG_expose_wasm = true;
    ValueSerializerTest::SetUpTestCase();
  }

  static void TearDownTestCase() {
    ValueSerializerTest::TearDownTestCase();
    i::FLAG_expose_wasm = g_saved_flag;
    g_saved_flag = false;
  }

  class ThrowingSerializer : public ValueSerializer::Delegate {
   public:
    Maybe<uint32_t> GetWasmModuleTransferId(
        Isolate* isolate, Local<WasmCompiledModule> module) override {
      isolate->ThrowException(Exception::Error(
          String::NewFromOneByte(
              isolate,
              reinterpret_cast<const uint8_t*>(kUnsupportedSerialization),
              NewStringType::kNormal)
              .ToLocalChecked()));
      return Nothing<uint32_t>();
    }

    void ThrowDataCloneError(Local<String> message) override { UNREACHABLE(); }
  };

  class SerializeToTransfer : public ValueSerializer::Delegate {
   public:
    SerializeToTransfer(
        std::vector<WasmCompiledModule::TransferrableModule>* modules)
        : modules_(modules) {}
    Maybe<uint32_t> GetWasmModuleTransferId(
        Isolate* isolate, Local<WasmCompiledModule> module) override {
      modules_->push_back(module->GetTransferrableModule());
      return Just(static_cast<uint32_t>(modules_->size()) - 1);
    }

    void ThrowDataCloneError(Local<String> message) override { UNREACHABLE(); }

   private:
    std::vector<WasmCompiledModule::TransferrableModule>* modules_;
  };

  class DeserializeFromTransfer : public ValueDeserializer::Delegate {
   public:
    DeserializeFromTransfer(
        std::vector<WasmCompiledModule::TransferrableModule>* modules)
        : modules_(modules) {}

    MaybeLocal<WasmCompiledModule> GetWasmModuleFromId(Isolate* isolate,
                                                       uint32_t id) override {
      return WasmCompiledModule::FromTransferrableModule(isolate,
                                                         modules_->at(id));
    }

   private:
    std::vector<WasmCompiledModule::TransferrableModule>* modules_;
  };

  ValueSerializer::Delegate* GetSerializerDelegate() override {
    return current_serializer_delegate_;
  }

  ValueDeserializer::Delegate* GetDeserializerDelegate() override {
    return current_deserializer_delegate_;
  }

  Local<WasmCompiledModule> MakeWasm() {
    Context::Scope scope(serialization_context());
    return WasmCompiledModule::DeserializeOrCompile(
               isolate(), {nullptr, 0},
               {kIncrementerWasm, sizeof(kIncrementerWasm)})
        .ToLocalChecked();
  }

  void ExpectPass() {
    Local<Value> value = RoundTripTest(MakeWasm());
    Context::Scope scope(deserialization_context());
    ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
    ExpectScriptTrue(
        "new WebAssembly.Instance(result).exports.increment(8) === 9");
  }

  void ExpectFail() {
    const std::vector<uint8_t> data = EncodeTest(MakeWasm());
    InvalidDecodeTest(data);
  }

  Local<Value> GetComplexObjectWithDuplicate() {
    Context::Scope scope(serialization_context());
    Local<Value> wasm_module = MakeWasm();
    serialization_context()
        ->Global()
        ->CreateDataProperty(serialization_context(),
                             StringFromUtf8("wasm_module"), wasm_module)
        .FromMaybe(false);
    Local<Script> script =
        Script::Compile(
            serialization_context(),
            StringFromUtf8("({mod1: wasm_module, num: 2, mod2: wasm_module})"))
            .ToLocalChecked();
    return script->Run(serialization_context()).ToLocalChecked();
  }

  void VerifyComplexObject(Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    ExpectScriptTrue("result.mod1 instanceof WebAssembly.Module");
    ExpectScriptTrue("result.mod2 instanceof WebAssembly.Module");
    ExpectScriptTrue("result.num === 2");
  }

  Local<Value> GetComplexObjectWithMany() {
    Context::Scope scope(serialization_context());
    Local<Value> wasm_module1 = MakeWasm();
    Local<Value> wasm_module2 = MakeWasm();
    serialization_context()
        ->Global()
        ->CreateDataProperty(serialization_context(),
                             StringFromUtf8("wasm_module1"), wasm_module1)
        .FromMaybe(false);
    serialization_context()
        ->Global()
        ->CreateDataProperty(serialization_context(),
                             StringFromUtf8("wasm_module2"), wasm_module2)
        .FromMaybe(false);
    Local<Script> script =
        Script::Compile(
            serialization_context(),
            StringFromUtf8(
                "({mod1: wasm_module1, num: 2, mod2: wasm_module2})"))
            .ToLocalChecked();
    return script->Run(serialization_context()).ToLocalChecked();
  }

 private:
  static bool g_saved_flag;
  std::vector<WasmCompiledModule::TransferrableModule> transfer_modules_;
  SerializeToTransfer serialize_delegate_;
  DeserializeFromTransfer deserialize_delegate_;
  ValueSerializer::Delegate* current_serializer_delegate_ = nullptr;
  ValueDeserializer::Delegate* current_deserializer_delegate_ = nullptr;
  ThrowingSerializer throwing_serializer_;
  ValueDeserializer::Delegate default_deserializer_;
};

bool ValueSerializerTestWithWasm::g_saved_flag = false;
const char* ValueSerializerTestWithWasm::kUnsupportedSerialization =
    "Wasm Serialization Not Supported";

// The default implementation of the serialization
// delegate throws when trying to serialize wasm. The
// embedder must decide serialization policy.
TEST_F(ValueSerializerTestWithWasm, DefaultSerializationDelegate) {
  EnableThrowingSerializer();
  Local<Message> message = InvalidEncodeTest(MakeWasm());
  size_t msg_len = static_cast<size_t>(message->Get()->Length());
  std::unique_ptr<char[]> buff(new char[msg_len + 1]);
  message->Get()->WriteOneByte(isolate(),
                               reinterpret_cast<uint8_t*>(buff.get()));
  // the message ends with the custom error string
  size_t custom_msg_len = strlen(kUnsupportedSerialization);
  ASSERT_GE(msg_len, custom_msg_len);
  size_t start_pos = msg_len - custom_msg_len;
  ASSERT_EQ(strcmp(&buff.get()[start_pos], kUnsupportedSerialization), 0);
}

// The default deserializer throws if wasm transfer is attempted
TEST_F(ValueSerializerTestWithWasm, DefaultDeserializationDelegate) {
  EnableTransferSerialization();
  EnableDefaultDeserializer();
  ExpectFail();
}

// We only want to allow deserialization through
// transferred modules - which requres both serializer
// and deserializer to understand that - or through
// explicitly allowing inlined data, which requires
// deserializer opt-in (we default the serializer to
// inlined data because we don't trust that data on the
// receiving end anyway).

TEST_F(ValueSerializerTestWithWasm, RoundtripWasmTransfer) {
  EnableTransferSerialization();
  EnableTransferDeserialization();
  ExpectPass();
}

TEST_F(ValueSerializerTestWithWasm, RountripWasmInline) {
  SetExpectInlineWasm(true);
  ExpectPass();
}

TEST_F(ValueSerializerTestWithWasm, CannotDeserializeWasmInlineData) {
  ExpectFail();
}

TEST_F(ValueSerializerTestWithWasm, CannotTransferWasmWhenExpectingInline) {
  EnableTransferSerialization();
  SetExpectInlineWasm(true);
  ExpectFail();
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectDuplicateTransfer) {
  EnableTransferSerialization();
  EnableTransferDeserialization();
  Local<Value> value = RoundTripTest(GetComplexObjectWithDuplicate());
  VerifyComplexObject(value);
  ExpectScriptTrue("result.mod1 === result.mod2");
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectDuplicateInline) {
  SetExpectInlineWasm(true);
  Local<Value> value = RoundTripTest(GetComplexObjectWithDuplicate());
  VerifyComplexObject(value);
  ExpectScriptTrue("result.mod1 === result.mod2");
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectWithManyTransfer) {
  EnableTransferSerialization();
  EnableTransferDeserialization();
  Local<Value> value = RoundTripTest(GetComplexObjectWithMany());
  VerifyComplexObject(value);
  ExpectScriptTrue("result.mod1 != result.mod2");
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectWithManyInline) {
  SetExpectInlineWasm(true);
  Local<Value> value = RoundTripTest(GetComplexObjectWithMany());
  VerifyComplexObject(value);
  ExpectScriptTrue("result.mod1 != result.mod2");
}

// As produced around Chrome 56.
const unsigned char kSerializedIncrementerWasm[] = {
    0xFF, 0x09, 0x3F, 0x00, 0x57, 0x79, 0x2D, 0x00, 0x61, 0x73, 0x6D, 0x0D,
    0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7F, 0x01, 0x7F, 0x03,
    0x02, 0x01, 0x00, 0x07, 0x0D, 0x01, 0x09, 0x69, 0x6E, 0x63, 0x72, 0x65,
    0x6D, 0x65, 0x6E, 0x74, 0x00, 0x00, 0x0A, 0x08, 0x01, 0x06, 0x00, 0x20,
    0x00, 0x41, 0x01, 0x6A, 0xF8, 0x04, 0xA1, 0x06, 0xDE, 0xC0, 0xC6, 0x44,
    0x3C, 0x29, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x02, 0x00, 0x00, 0x81, 0x4E,
    0xCE, 0x7C, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x30, 0x02,
    0x00, 0x00, 0xB0, 0x25, 0x30, 0xE3, 0xF2, 0xDB, 0x2E, 0x48, 0x00, 0x00,
    0x00, 0x80, 0xE8, 0x00, 0x00, 0x80, 0xE0, 0x01, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x07, 0x08, 0x00, 0x00, 0x09, 0x04,
    0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3C, 0x8C, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x01, 0x10, 0x8C, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x70, 0x94, 0x01, 0x0C, 0x8B,
    0xC1, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x25, 0xDC, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x9E, 0x01, 0x10, 0x8C, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x7D, 0x01, 0x1A, 0xE1, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x23, 0x88, 0x42, 0x32, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x02, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x49, 0x3B, 0xA5, 0x60, 0x0C, 0x00,
    0x00, 0x0F, 0x86, 0x04, 0x00, 0x00, 0x00, 0x83, 0xC0, 0x01, 0xC3, 0x55,
    0x48, 0x89, 0xE5, 0x49, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
    0x00, 0x41, 0x52, 0x48, 0x83, 0xEC, 0x08, 0x48, 0x89, 0x45, 0xF0, 0x48,
    0xBB, 0xB0, 0x67, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xC0, 0x48,
    0xBE, 0xE1, 0x57, 0x81, 0x85, 0xF6, 0x14, 0x00, 0x00, 0xE8, 0xFC, 0x3C,
    0xEA, 0xFF, 0x48, 0x8B, 0x45, 0xF0, 0x48, 0x8B, 0xE5, 0x5D, 0xEB, 0xBF,
    0x66, 0x90, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x44, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0F, 0x20, 0x84, 0x0F, 0x7D, 0x01, 0x0D, 0x00, 0x0F, 0x04,
    0x6D, 0x08, 0x0F, 0xF0, 0x02, 0x80, 0x94, 0x01, 0x0C, 0x8B, 0xC1, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xED, 0xA9, 0x2D, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x9E, 0xE0, 0x38, 0x1A, 0x61, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x23, 0x88, 0x42, 0x32, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x02, 0xF9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x55, 0x48, 0x89, 0xE5, 0x56, 0x57, 0x48,
    0x8B, 0x45, 0x10, 0xE8, 0x11, 0xED, 0xED, 0xFF, 0xA8, 0x01, 0x0F, 0x85,
    0x2D, 0x00, 0x00, 0x00, 0x48, 0xC1, 0xE8, 0x20, 0xC5, 0xF9, 0x57, 0xC0,
    0xC5, 0xFB, 0x2A, 0xC0, 0xC4, 0xE1, 0xFB, 0x2C, 0xC0, 0x48, 0x83, 0xF8,
    0x01, 0x0F, 0x80, 0x34, 0x00, 0x00, 0x00, 0x8B, 0xC0, 0xE8, 0x27, 0xFE,
    0xFF, 0xFF, 0x48, 0xC1, 0xE0, 0x20, 0x48, 0x8B, 0xE5, 0x5D, 0xC2, 0x10,
    0x00, 0x49, 0x39, 0x45, 0xA0, 0x0F, 0x84, 0x07, 0x00, 0x00, 0x00, 0xC5,
    0xFB, 0x10, 0x40, 0x07, 0xEB, 0xCE, 0x49, 0xBA, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xF8, 0x7F, 0xC4, 0xC1, 0xF9, 0x6E, 0xC2, 0xEB, 0xBD, 0x48,
    0x83, 0xEC, 0x08, 0xC5, 0xFB, 0x11, 0x04, 0x24, 0xE8, 0xCC, 0xFE, 0xFF,
    0xFF, 0x48, 0x83, 0xC4, 0x08, 0xEB, 0xB8, 0x66, 0x90, 0x02, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0x0F, 0x39, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x20, 0x84,
    0x0F, 0xCC, 0x6E, 0x7D, 0x01, 0x72, 0x98, 0x00, 0x0F, 0xDC, 0x6D, 0x0C,
    0x0F, 0xB0, 0x84, 0x0D, 0x04, 0x84, 0xE3, 0xC0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x84, 0xE0, 0x84, 0x84, 0x18, 0x2F, 0x2F, 0x2F,
    0x2F, 0x2F};

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModule) {
  if ((true)) return;  // TODO(mtrofin): fix this test
  std::vector<uint8_t> raw(
      kSerializedIncrementerWasm,
      kSerializedIncrementerWasm + sizeof(kSerializedIncrementerWasm));
  Local<Value> value = DecodeTest(raw);
  ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
  ExpectScriptTrue(
      "new WebAssembly.Instance(result).exports.increment(8) === 9");
}

// As above, but with empty compiled data. Should work due to fallback to wire
// data.
const unsigned char kSerializedIncrementerWasmWithInvalidCompiledData[] = {
    0xFF, 0x09, 0x3F, 0x00, 0x57, 0x79, 0x2D, 0x00, 0x61, 0x73, 0x6D,
    0x0D, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7F, 0x01,
    0x7F, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0D, 0x01, 0x09, 0x69, 0x6E,
    0x63, 0x72, 0x65, 0x6D, 0x65, 0x6E, 0x74, 0x00, 0x00, 0x0A, 0x08,
    0x01, 0x06, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6A, 0x00};

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModuleWithInvalidCompiledData) {
  if ((true)) return;  // TODO(titzer): regenerate this test
  std::vector<uint8_t> raw(
      kSerializedIncrementerWasmWithInvalidCompiledData,
      kSerializedIncrementerWasmWithInvalidCompiledData +
          sizeof(kSerializedIncrementerWasmWithInvalidCompiledData));
  Local<Value> value = DecodeTest(raw);
  ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
  ExpectScriptTrue(
      "new WebAssembly.Instance(result).exports.increment(8) === 9");
}

// As above, but also with empty wire data. Should fail.
const unsigned char kSerializedIncrementerWasmInvalid[] = {
    0xFF, 0x09, 0x3F, 0x00, 0x57, 0x79, 0x00, 0x00};

TEST_F(ValueSerializerTestWithWasm,
       DecodeWasmModuleWithInvalidCompiledAndWireData) {
  std::vector<uint8_t> raw(kSerializedIncrementerWasmInvalid,
                           kSerializedIncrementerWasmInvalid +
                               sizeof(kSerializedIncrementerWasmInvalid));
  InvalidDecodeTest(raw);
}

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModuleWithInvalidDataLength) {
  InvalidDecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x57, 0x79, 0x7F, 0x00});
  InvalidDecodeTest({0xFF, 0x09, 0x3F, 0x00, 0x57, 0x79, 0x00, 0x7F});
}

}  // namespace
}  // namespace v8
