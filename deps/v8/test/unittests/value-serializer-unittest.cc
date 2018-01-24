// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/value-serializer.h"

#include <algorithm>
#include <string>

#include "include/v8.h"
#include "src/api.h"
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

  ~ValueSerializerTest() {
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
  virtual void AfterEncode() {}
  virtual ValueDeserializer::Delegate* GetDeserializerDelegate() {
    return nullptr;
  }
  virtual void BeforeDecode(ValueDeserializer*) {}

  template <typename InputFunctor, typename OutputFunctor>
  void RoundTripTest(const InputFunctor& input_functor,
                     const OutputFunctor& output_functor) {
    EncodeTest(input_functor,
               [this, &output_functor](const std::vector<uint8_t>& data) {
                 DecodeTest(data, output_functor);
               });
  }

  // Variant for the common case where a script is used to build the original
  // value.
  template <typename OutputFunctor>
  void RoundTripTest(const char* source, const OutputFunctor& output_functor) {
    RoundTripTest([this, source]() { return EvaluateScriptForInput(source); },
                  output_functor);
  }

  // Variant which uses JSON.parse/stringify to check the result.
  void RoundTripJSON(const char* source) {
    RoundTripTest(
        [this, source]() {
          return JSON::Parse(serialization_context_, StringFromUtf8(source))
              .ToLocalChecked();
        },
        [this, source](Local<Value> value) {
          ASSERT_TRUE(value->IsObject());
          EXPECT_EQ(source, Utf8Value(JSON::Stringify(deserialization_context_,
                                                      value.As<Object>())
                                          .ToLocalChecked()));
        });
  }

  Maybe<std::vector<uint8_t>> DoEncode(Local<Value> value) {
    Local<Context> context = serialization_context();
    ValueSerializer serializer(isolate(), GetSerializerDelegate());
    BeforeEncode(&serializer);
    serializer.WriteHeader();
    if (!serializer.WriteValue(context, value).FromMaybe(false)) {
      return Nothing<std::vector<uint8_t>>();
    }
    AfterEncode();
    std::pair<uint8_t*, size_t> buffer = serializer.Release();
    std::vector<uint8_t> result(buffer.first, buffer.first + buffer.second);
    free(buffer.first);
    return Just(std::move(result));
  }

  template <typename InputFunctor, typename EncodedDataFunctor>
  void EncodeTest(const InputFunctor& input_functor,
                  const EncodedDataFunctor& encoded_data_functor) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    Local<Value> input_value = input_functor();
    std::vector<uint8_t> buffer;
    ASSERT_TRUE(DoEncode(input_value).To(&buffer));
    ASSERT_FALSE(try_catch.HasCaught());
    encoded_data_functor(buffer);
  }

  template <typename InputFunctor, typename MessageFunctor>
  void InvalidEncodeTest(const InputFunctor& input_functor,
                         const MessageFunctor& functor) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    Local<Value> input_value = input_functor();
    ASSERT_TRUE(DoEncode(input_value).IsNothing());
    functor(try_catch.Message());
  }

  template <typename MessageFunctor>
  void InvalidEncodeTest(const char* source, const MessageFunctor& functor) {
    InvalidEncodeTest(
        [this, source]() { return EvaluateScriptForInput(source); }, functor);
  }

  void InvalidEncodeTest(const char* source) {
    InvalidEncodeTest(source, [](Local<Message>) {});
  }

  template <typename OutputFunctor>
  void DecodeTest(const std::vector<uint8_t>& data,
                  const OutputFunctor& output_functor) {
    Local<Context> context = deserialization_context();
    Context::Scope scope(context);
    TryCatch try_catch(isolate());
    ValueDeserializer deserializer(isolate(), &data[0],
                                   static_cast<int>(data.size()),
                                   GetDeserializerDelegate());
    deserializer.SetSupportsLegacyWireFormat(true);
    deserializer.SetExpectInlineWasm(ExpectInlineWasm());
    BeforeDecode(&deserializer);
    ASSERT_TRUE(deserializer.ReadHeader(context).FromMaybe(false));
    Local<Value> result;
    ASSERT_TRUE(deserializer.ReadValue(context).ToLocal(&result));
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_FALSE(try_catch.HasCaught());
    ASSERT_TRUE(
        context->Global()
            ->CreateDataProperty(context, StringFromUtf8("result"), result)
            .FromMaybe(false));
    output_functor(result);
    ASSERT_FALSE(try_catch.HasCaught());
  }

  template <typename OutputFunctor>
  void DecodeTestForVersion0(const std::vector<uint8_t>& data,
                             const OutputFunctor& output_functor) {
    Local<Context> context = deserialization_context();
    Context::Scope scope(context);
    TryCatch try_catch(isolate());
    ValueDeserializer deserializer(isolate(), &data[0],
                                   static_cast<int>(data.size()),
                                   GetDeserializerDelegate());
    deserializer.SetSupportsLegacyWireFormat(true);
    deserializer.SetExpectInlineWasm(ExpectInlineWasm());
    BeforeDecode(&deserializer);
    ASSERT_TRUE(deserializer.ReadHeader(context).FromMaybe(false));
    ASSERT_EQ(0u, deserializer.GetWireFormatVersion());
    Local<Value> result;
    ASSERT_TRUE(deserializer.ReadValue(context).ToLocal(&result));
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_FALSE(try_catch.HasCaught());
    ASSERT_TRUE(
        context->Global()
            ->CreateDataProperty(context, StringFromUtf8("result"), result)
            .FromMaybe(false));
    output_functor(result);
    ASSERT_FALSE(try_catch.HasCaught());
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
    ASSERT_TRUE(header_result.ToChecked());
    ASSERT_TRUE(deserializer.ReadValue(context).IsEmpty());
    EXPECT_TRUE(try_catch.HasCaught());
  }

  Local<Value> EvaluateScriptForInput(const char* utf8_source) {
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(serialization_context_, source).ToLocalChecked();
    return script->Run(serialization_context_).ToLocalChecked();
  }

  bool EvaluateScriptForResultBool(const char* utf8_source) {
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(deserialization_context_, source).ToLocalChecked();
    Local<Value> value = script->Run(deserialization_context_).ToLocalChecked();
    return value->BooleanValue(deserialization_context_).FromJust();
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
  InvalidDecodeTest({0xff});
  // Version too large.
  InvalidDecodeTest({0xff, 0x7f, 0x5f});
  // Nonsense tag.
  InvalidDecodeTest({0xff, 0x09, 0xdd});
}

TEST_F(ValueSerializerTest, RoundTripOddball) {
  RoundTripTest([this]() { return Undefined(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  RoundTripTest([this]() { return True(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  RoundTripTest([this]() { return False(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  RoundTripTest([this]() { return Null(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });
}

TEST_F(ValueSerializerTest, DecodeOddball) {
  // What this code is expected to generate.
  DecodeTest({0xff, 0x09, 0x5f},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0xff, 0x09, 0x54},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0xff, 0x09, 0x46},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0xff, 0x09, 0x30},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });

  // What v9 of the Blink code generates.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x5f, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x54, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x46, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x30, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });

  // v0 (with no explicit version).
  DecodeTest({0x5f, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0x54, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0x46, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0x30, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });
}

TEST_F(ValueSerializerTest, RoundTripNumber) {
  RoundTripTest([this]() { return Integer::New(isolate(), 42); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsInt32());
                  EXPECT_EQ(42, Int32::Cast(*value)->Value());
                });
  RoundTripTest([this]() { return Integer::New(isolate(), -31337); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsInt32());
                  EXPECT_EQ(-31337, Int32::Cast(*value)->Value());
                });
  RoundTripTest(
      [this]() {
        return Integer::New(isolate(), std::numeric_limits<int32_t>::min());
      },
      [](Local<Value> value) {
        ASSERT_TRUE(value->IsInt32());
        EXPECT_EQ(std::numeric_limits<int32_t>::min(),
                  Int32::Cast(*value)->Value());
      });
  RoundTripTest([this]() { return Number::New(isolate(), -0.25); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsNumber());
                  EXPECT_EQ(-0.25, Number::Cast(*value)->Value());
                });
  RoundTripTest(
      [this]() {
        return Number::New(isolate(), std::numeric_limits<double>::quiet_NaN());
      },
      [](Local<Value> value) {
        ASSERT_TRUE(value->IsNumber());
        EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
      });
}

TEST_F(ValueSerializerTest, DecodeNumber) {
  // 42 zig-zag encoded (signed)
  DecodeTest({0xff, 0x09, 0x49, 0x54},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               EXPECT_EQ(42, Int32::Cast(*value)->Value());
             });
  // 42 varint encoded (unsigned)
  DecodeTest({0xff, 0x09, 0x55, 0x2a},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               EXPECT_EQ(42, Int32::Cast(*value)->Value());
             });
  // 160 zig-zag encoded (signed)
  DecodeTest({0xff, 0x09, 0x49, 0xc0, 0x02},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               ASSERT_EQ(160, Int32::Cast(*value)->Value());
             });
  // 160 varint encoded (unsigned)
  DecodeTest({0xff, 0x09, 0x55, 0xa0, 0x01},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               ASSERT_EQ(160, Int32::Cast(*value)->Value());
             });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  // IEEE 754 doubles, little-endian byte order
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_EQ(-0.25, Number::Cast(*value)->Value());
             });
  // quiet NaN
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
             });
  // signaling NaN
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf4, 0x7f},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
             });
#endif
  // TODO(jbroman): Equivalent test for big-endian machines.
}

// String constants (in UTF-8) used for string encoding tests.
static const char kHelloString[] = "Hello";
static const char kQuebecString[] = "\x51\x75\xC3\xA9\x62\x65\x63";
static const char kEmojiString[] = "\xF0\x9F\x91\x8A";

TEST_F(ValueSerializerTest, RoundTripString) {
  RoundTripTest([this]() { return String::Empty(isolate()); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(0, String::Cast(*value)->Length());
                });
  // Inside ASCII.
  RoundTripTest([this]() { return StringFromUtf8(kHelloString); },
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(5, String::Cast(*value)->Length());
                  EXPECT_EQ(kHelloString, Utf8Value(value));
                });
  // Inside Latin-1 (i.e. one-byte string), but not ASCII.
  RoundTripTest([this]() { return StringFromUtf8(kQuebecString); },
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(6, String::Cast(*value)->Length());
                  EXPECT_EQ(kQuebecString, Utf8Value(value));
                });
  // An emoji (decodes to two 16-bit chars).
  RoundTripTest([this]() { return StringFromUtf8(kEmojiString); },
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(2, String::Cast(*value)->Length());
                  EXPECT_EQ(kEmojiString, Utf8Value(value));
                });
}

TEST_F(ValueSerializerTest, DecodeString) {
  // Decoding the strings above from UTF-8.
  DecodeTest({0xff, 0x09, 0x53, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(0, String::Cast(*value)->Length());
             });
  DecodeTest({0xff, 0x09, 0x53, 0x05, 'H', 'e', 'l', 'l', 'o'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(5, String::Cast(*value)->Length());
               EXPECT_EQ(kHelloString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x53, 0x07, 'Q', 'u', 0xc3, 0xa9, 'b', 'e', 'c'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(6, String::Cast(*value)->Length());
               EXPECT_EQ(kQuebecString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x53, 0x04, 0xf0, 0x9f, 0x91, 0x8a},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(2, String::Cast(*value)->Length());
               EXPECT_EQ(kEmojiString, Utf8Value(value));
             });

  // And from Latin-1 (for the ones that fit).
  DecodeTest({0xff, 0x0a, 0x22, 0x00}, [](Local<Value> value) {
    ASSERT_TRUE(value->IsString());
    EXPECT_EQ(0, String::Cast(*value)->Length());
  });
  DecodeTest({0xff, 0x0a, 0x22, 0x05, 'H', 'e', 'l', 'l', 'o'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(5, String::Cast(*value)->Length());
               EXPECT_EQ(kHelloString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x0a, 0x22, 0x06, 'Q', 'u', 0xe9, 'b', 'e', 'c'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(6, String::Cast(*value)->Length());
               EXPECT_EQ(kQuebecString, Utf8Value(value));
             });

// And from two-byte strings (endianness dependent).
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x63, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(0, String::Cast(*value)->Length());
             });
  DecodeTest({0xff, 0x09, 0x63, 0x0a, 'H', '\0', 'e', '\0', 'l', '\0', 'l',
              '\0', 'o', '\0'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(5, String::Cast(*value)->Length());
               EXPECT_EQ(kHelloString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x63, 0x0c, 'Q', '\0', 'u', '\0', 0xe9, '\0', 'b',
              '\0', 'e', '\0', 'c', '\0'},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(6, String::Cast(*value)->Length());
               EXPECT_EQ(kQuebecString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x63, 0x04, 0x3d, 0xd8, 0x4a, 0xdc},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(2, String::Cast(*value)->Length());
               EXPECT_EQ(kEmojiString, Utf8Value(value));
             });
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, DecodeInvalidString) {
  // UTF-8 string with too few bytes available.
  InvalidDecodeTest({0xff, 0x09, 0x53, 0x10, 'v', '8'});
  // One-byte string with too few bytes available.
  InvalidDecodeTest({0xff, 0x0a, 0x22, 0x10, 'v', '8'});
#if defined(V8_TARGET_LITTLE_ENDIAN)
  // Two-byte string with too few bytes available.
  InvalidDecodeTest({0xff, 0x09, 0x63, 0x10, 'v', '\0', '8', '\0'});
  // Two-byte string with an odd byte length.
  InvalidDecodeTest({0xff, 0x09, 0x63, 0x03, 'v', '\0', '8'});
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, EncodeTwoByteStringUsesPadding) {
  // As long as the output has a version that Blink expects to be able to read,
  // we must respect its alignment requirements. It requires that two-byte
  // characters be aligned.
  EncodeTest(
      [this]() {
        // We need a string whose length will take two bytes to encode, so that
        // a padding byte is needed to keep the characters aligned. The string
        // must also have a two-byte character, so that it gets the two-byte
        // encoding.
        std::string string(200, ' ');
        string += kEmojiString;
        return StringFromUtf8(string.c_str());
      },
      [](const std::vector<uint8_t>& data) {
        // This is a sufficient but not necessary condition. This test assumes
        // that the wire format version is one byte long, but is flexible to
        // what that value may be.
        const uint8_t expected_prefix[] = {0x00, 0x63, 0x94, 0x03};
        ASSERT_GT(data.size(), sizeof(expected_prefix) + 2);
        EXPECT_EQ(0xff, data[0]);
        EXPECT_GE(data[1], 0x09);
        EXPECT_LE(data[1], 0x7f);
        EXPECT_TRUE(std::equal(std::begin(expected_prefix),
                               std::end(expected_prefix), data.begin() + 2));
      });
}

TEST_F(ValueSerializerTest, RoundTripDictionaryObject) {
  // Empty object.
  RoundTripTest("({})", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Object.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 0"));
  });
  // String key.
  RoundTripTest("({ a: 42 })", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 1"));
  });
  // Integer key (treated as a string, but may be encoded differently).
  RoundTripTest("({ 42: 'a' })", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 1"));
  });
  // Key order must be preserved.
  RoundTripTest("({ x: 1, y: 2, a: 3 })", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
  });
  // A harder case of enumeration order.
  // Indexes first, in order (but not 2^32 - 1, which is not an index), then the
  // remaining (string) keys, in the order they were defined.
  RoundTripTest(
      "({ a: 2, 0xFFFFFFFF: 1, 0xFFFFFFFE: 3, 1: 0 })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === "
            "'1,4294967294,a,4294967295'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 2"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFF] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFE] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 0"));
      });
  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  RoundTripTest(
      "(() => { var y = {}; y.self = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result === result.self"));
      });
}

TEST_F(ValueSerializerTest, DecodeDictionaryObject) {
  // Empty object.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6f, 0x7b, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsObject());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Object.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getOwnPropertyNames(result).length === 0"));
             });
  // String key.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01,
       0x49, 0x54, 0x7b, 0x01},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Integer key (treated as a string, but may be encoded differently).
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x49, 0x54, 0x3f, 0x01, 0x53,
       0x01, 0x61, 0x7b, 0x01},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Key order must be preserved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x78, 0x3f, 0x01,
       0x49, 0x02, 0x3f, 0x01, 0x53, 0x01, 0x79, 0x3f, 0x01, 0x49, 0x04, 0x3f,
       0x01, 0x53, 0x01, 0x61, 0x3f, 0x01, 0x49, 0x06, 0x7b, 0x03},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
      });
  // A harder case of enumeration order.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x49, 0x02, 0x3f, 0x01,
       0x49, 0x00, 0x3f, 0x01, 0x55, 0xfe, 0xff, 0xff, 0xff, 0x0f, 0x3f,
       0x01, 0x49, 0x06, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01, 0x49,
       0x04, 0x3f, 0x01, 0x53, 0x0a, 0x34, 0x32, 0x39, 0x34, 0x39, 0x36,
       0x37, 0x32, 0x39, 0x35, 0x3f, 0x01, 0x49, 0x02, 0x7b, 0x04},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === "
            "'1,4294967294,a,4294967295'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 2"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFF] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFE] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 0"));
      });
  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x04, 0x73,
       0x65, 0x6c, 0x66, 0x3f, 0x01, 0x5e, 0x00, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result === result.self"));
      });
}

TEST_F(ValueSerializerTest, InvalidDecodeObjectWithInvalidKeyType) {
  // Objects which would need conversion to string shouldn't be present as
  // object keys. The serializer would have obtained them from the own property
  // keys list, which should only contain names and indices.
  InvalidDecodeTest(
      {0xff, 0x09, 0x6f, 0x61, 0x00, 0x40, 0x00, 0x00, 0x7b, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripOnlyOwnEnumerableStringKeys) {
  // Only "own" properties should be serialized, not ones on the prototype.
  RoundTripTest("(() => { var x = {}; x.__proto__ = {a: 4}; return x; })()",
                [this](Local<Value> value) {
                  EXPECT_TRUE(EvaluateScriptForResultBool("!('a' in result)"));
                });
  // Only enumerable properties should be serialized.
  RoundTripTest(
      "(() => {"
      "  var x = {};"
      "  Object.defineProperty(x, 'a', {value: 1, enumerable: false});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('a' in result)"));
      });
  // Symbol keys should not be serialized.
  RoundTripTest("({ [Symbol()]: 4 })", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertySymbols(result).length === 0"));
  });
}

TEST_F(ValueSerializerTest, RoundTripTrickyGetters) {
  // Keys are enumerated before any setters are called, but if there is no own
  // property when the value is to be read, then it should not be serialized.
  RoundTripTest("({ get a() { delete this.b; return 1; }, b: 2 })",
                [this](Local<Value> value) {
                  EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
                });
  // Keys added after the property enumeration should not be serialized.
  RoundTripTest("({ get a() { this.b = 3; }})", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
  });
  // But if you remove a key and add it back, that's fine. But it will appear in
  // the original place in enumeration order.
  RoundTripTest(
      "({ get a() { delete this.b; this.b = 4; }, b: 2, c: 3 })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'a,b,c'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.b === 4"));
      });
  // Similarly, it only matters if a property was enumerable when the
  // enumeration happened.
  RoundTripTest(
      "({ get a() {"
      "    Object.defineProperty(this, 'b', {value: 2, enumerable: false});"
      "}, b: 1})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.b === 2"));
      });
  RoundTripTest(
      "(() => {"
      "  var x = {"
      "    get a() {"
      "      Object.defineProperty(this, 'b', {value: 2, enumerable: true});"
      "    }"
      "  };"
      "  Object.defineProperty(x, 'b',"
      "      {value: 1, enumerable: false, configurable: true});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
      });
  // The property also should not be read if it can only be found on the
  // prototype chain (but not as an own property) after enumeration.
  RoundTripTest(
      "(() => {"
      "  var x = { get a() { delete this.b; }, b: 1 };"
      "  x.__proto__ = { b: 0 };"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
      });
  // If an exception is thrown by script, encoding must fail and the exception
  // must be thrown.
  InvalidEncodeTest("({ get a() { throw new Error('sentinel'); } })",
                    [this](Local<Message> message) {
                      ASSERT_FALSE(message.IsEmpty());
                      EXPECT_NE(std::string::npos,
                                Utf8Value(message->Get()).find("sentinel"));
                    });
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
  DecodeTestForVersion0(
      {0x7b, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Object.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 0"));
      });
  // String key.
  DecodeTestForVersion0(
      {0x53, 0x01, 0x61, 0x49, 0x54, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Object.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Integer key (treated as a string, but may be encoded differently).
  DecodeTestForVersion0(
      {0x49, 0x54, 0x53, 0x01, 0x61, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Key order must be preserved.
  DecodeTestForVersion0(
      {0x53, 0x01, 0x78, 0x49, 0x02, 0x53, 0x01, 0x79, 0x49, 0x04, 0x53, 0x01,
       0x61, 0x49, 0x06, 0x7b, 0x03, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
      });
  // A property and an element.
  DecodeTestForVersion0(
      {0x49, 0x54, 0x53, 0x01, 0x61, 0x53, 0x01, 0x61, 0x49, 0x54, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === '42,a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArray) {
  // A simple array of integers.
  RoundTripTest("[1, 2, 3, 4, 5]", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArray());
    EXPECT_EQ(5u, Array::Cast(*value)->Length());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Array.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '1,2,3,4,5'"));
  });
  // A long (sparse) array.
  RoundTripTest(
      "(() => { var x = new Array(1000); x[500] = 42; return x; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[500] === 42"));
      });
  // Duplicate reference.
  RoundTripTest(
      "(() => { var y = {}; return [y, y]; })()", [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result[1]"));
      });
  // Duplicate reference in a sparse array.
  RoundTripTest(
      "(() => { var x = new Array(1000); x[1] = x[500] = {}; return x; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'object'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === result[500]"));
      });
  // Self reference.
  RoundTripTest(
      "(() => { var y = []; y[0] = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result"));
      });
  // Self reference in a sparse array.
  RoundTripTest(
      "(() => { var y = new Array(1000); y[519] = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[519] === result"));
      });
  // Array with additional properties.
  RoundTripTest(
      "(() => { var y = [1, 2]; y.foo = 'bar'; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '1,2'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // Sparse array with additional properties.
  RoundTripTest(
      "(() => { var y = new Array(1000); y.foo = 'bar'; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.toString() === ','.repeat(999)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // The distinction between holes and undefined elements must be maintained.
  RoundTripTest("[,undefined]", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArray());
    ASSERT_EQ(2u, Array::Cast(*value)->Length());
    EXPECT_TRUE(
        EvaluateScriptForResultBool("typeof result[0] === 'undefined'"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(0)"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty(1)"));
  });
}

TEST_F(ValueSerializerTest, DecodeArray) {
  // A simple array of integers.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x41, 0x05, 0x3f, 0x01, 0x49, 0x02,
              0x3f, 0x01, 0x49, 0x04, 0x3f, 0x01, 0x49, 0x06, 0x3f, 0x01,
              0x49, 0x08, 0x3f, 0x01, 0x49, 0x0a, 0x24, 0x00, 0x05, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               EXPECT_EQ(5u, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Array.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '1,2,3,4,5'"));
             });
  // A long (sparse) array.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
              0xe8, 0x07, 0x3f, 0x01, 0x49, 0x54, 0x40, 0x01, 0xe8, 0x07},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               EXPECT_EQ(1000u, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool("result[500] === 42"));
             });
  // Duplicate reference.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x41, 0x02, 0x3f, 0x01, 0x6f, 0x7b, 0x00, 0x3f,
       0x02, 0x5e, 0x01, 0x24, 0x00, 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result[1]"));
      });
  // Duplicate reference in a sparse array.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
       0x02, 0x3f, 0x01, 0x6f, 0x7b, 0x00, 0x3f, 0x02, 0x49, 0xe8,
       0x07, 0x3f, 0x02, 0x5e, 0x01, 0x40, 0x02, 0xe8, 0x07, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'object'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === result[500]"));
      });
  // Self reference.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x41, 0x01, 0x3f, 0x01, 0x5e, 0x00, 0x24,
              0x00, 0x01, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               ASSERT_EQ(1u, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result"));
             });
  // Self reference in a sparse array.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
       0x8e, 0x08, 0x3f, 0x01, 0x5e, 0x00, 0x40, 0x01, 0xe8, 0x07},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[519] === result"));
      });
  // Array with additional properties.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x41, 0x02, 0x3f, 0x01, 0x49, 0x02, 0x3f,
       0x01, 0x49, 0x04, 0x3f, 0x01, 0x53, 0x03, 0x66, 0x6f, 0x6f, 0x3f,
       0x01, 0x53, 0x03, 0x62, 0x61, 0x72, 0x24, 0x01, 0x02, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '1,2'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // Sparse array with additional properties.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01,
              0x53, 0x03, 0x66, 0x6f, 0x6f, 0x3f, 0x01, 0x53, 0x03,
              0x62, 0x61, 0x72, 0x40, 0x01, 0xe8, 0x07, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               ASSERT_EQ(1000u, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === ','.repeat(999)"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
             });
  // The distinction between holes and undefined elements must be maintained.
  // Note that since the previous output from Chrome fails this test, an
  // encoding using the sparse format was constructed instead.
  DecodeTest(
      {0xff, 0x09, 0x61, 0x02, 0x49, 0x02, 0x5f, 0x40, 0x01, 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[0] === 'undefined'"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(0)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty(1)"));
      });
}

TEST_F(ValueSerializerTest, DecodeInvalidOverLargeArray) {
  // So large it couldn't exist in the V8 heap, and its size couldn't fit in a
  // SMI on 32-bit systems (2^30).
  InvalidDecodeTest({0xff, 0x09, 0x41, 0x80, 0x80, 0x80, 0x80, 0x04});
  // Not so large, but there isn't enough data left in the buffer.
  InvalidDecodeTest({0xff, 0x09, 0x41, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripArrayWithNonEnumerableElement) {
  // Even though this array looks like [1,5,3], the 5 should be missing from the
  // perspective of structured clone, which only clones properties that were
  // enumerable.
  RoundTripTest(
      "(() => {"
      "  var x = [1,2,3];"
      "  Object.defineProperty(x, '1', {enumerable:false, value:5});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(3u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty('1')"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArrayWithTrickyGetters) {
  // If an element is deleted before it is serialized, then it's deleted.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; }}, 42];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(1)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; }}, 42];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(1)"));
      });
  // If the length is changed, then the resulting array still has the original
  // length, but elements that were not yet serialized are gone.
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 0; }}, 3, 4];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(4u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(2)"));
      });
  // The same is true if the length is shortened, but there are still items
  // remaining.
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 3; }}, 3, 4];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(4u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[2] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(3)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 0; }}, 3, 4];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(2)"));
      });
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 3; }}, 3, 4];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[2] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(3)"));
      });
  // If a getter makes a property non-enumerable, it should still be enumerated
  // as enumeration happens once before getters are invoked.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() {"
      "    Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "  }}, 2];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 3"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() {"
      "    Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "  }}, 2];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 3"));
      });
  // Getters on the array itself must also run.
  RoundTripTest(
      "(() => {"
      "  var x = [1, 2, 3];"
      "  Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(3u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 4"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [1, 2, 3];"
      "  Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 4"));
      });
  // Even with a getter that deletes things, we don't read from the prototype.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; } }, 2];"
      "  x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; } }, 2];"
      "  x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
      });
}

TEST_F(ValueSerializerTest, DecodeSparseArrayVersion0) {
  // Empty (sparse) array.
  DecodeTestForVersion0({0x40, 0x00, 0x00, 0x00},
                        [](Local<Value> value) {
                          ASSERT_TRUE(value->IsArray());
                          ASSERT_EQ(0u, Array::Cast(*value)->Length());
                        });
  // Sparse array with a mixture of elements and properties.
  DecodeTestForVersion0(
      {0x55, 0x00, 0x53, 0x01, 'a',  0x55, 0x02, 0x55, 0x05, 0x53,
       0x03, 'f',  'o',  'o',  0x53, 0x03, 'b',  'a',  'r',  0x53,
       0x03, 'b',  'a',  'z',  0x49, 0x0b, 0x40, 0x04, 0x03, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(3u, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.toString() === 'a,,5'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.baz === -6"));
      });
  // Sparse array in a sparse array (sanity check of nesting).
  DecodeTestForVersion0(
      {0x55, 0x01, 0x55, 0x01, 0x54, 0x40, 0x01, 0x02, 0x40, 0x01, 0x02, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(2u, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] instanceof Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result[1])"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1][1] === true"));
      });
}

TEST_F(ValueSerializerTest, RoundTripDenseArrayContainingUndefined) {
  // In previous serialization versions, this would be interpreted as an absent
  // property.
  RoundTripTest("[undefined]", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArray());
    EXPECT_EQ(1u, Array::Cast(*value)->Length());
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty(0)"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === undefined"));
  });
}

TEST_F(ValueSerializerTest, DecodeDenseArrayContainingUndefined) {
  // In previous versions, "undefined" in a dense array signified absence of the
  // element (for compatibility). In new versions, it has a separate encoding.
  DecodeTest({0xff, 0x09, 0x41, 0x01, 0x5f, 0x24, 0x00, 0x01},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result)"));
             });
  DecodeTest(
      {0xff, 0x0b, 0x41, 0x01, 0x5f, 0x24, 0x00, 0x01},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("0 in result"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === undefined"));
      });
  DecodeTest({0xff, 0x0b, 0x41, 0x01, 0x2d, 0x24, 0x00, 0x01},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result)"));
             });
}

TEST_F(ValueSerializerTest, RoundTripDate) {
  RoundTripTest("new Date(1e6)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Date.prototype"));
  });
  RoundTripTest("new Date(Date.UTC(1867, 6, 1))", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "result.toISOString() === '1867-07-01T00:00:00.000Z'"));
  });
  RoundTripTest("new Date(NaN)", [](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
  });
  RoundTripTest(
      "({ a: new Date(), get b() { return this.a; } })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Date"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeDate) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x80, 0x84,
              0x2e, 0x41, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Date.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x20, 0x45, 0x27, 0x89,
              0x87, 0xc2, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toISOString() === '1867-07-01T00:00:00.000Z'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf8, 0x7f, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
             });
#else
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x41, 0x2e, 0x84, 0x80, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Date.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0xc2, 0x87, 0x89, 0x27, 0x45, 0x20,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toISOString() === '1867-07-01T00:00:00.000Z'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
             });
#endif
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f,
       0x01, 0x44, 0x00, 0x20, 0x39, 0x50, 0x37, 0x6a, 0x75, 0x42, 0x3f,
       0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Date"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, RoundTripValueObjects) {
  RoundTripTest("new Boolean(true)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Boolean.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === true"));
  });
  RoundTripTest("new Boolean(false)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Boolean.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === false"));
  });
  RoundTripTest(
      "({ a: new Boolean(true), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Boolean"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  RoundTripTest("new Number(-42)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Number.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
  });
  RoundTripTest("new Number(NaN)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Number.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("Number.isNaN(result.valueOf())"));
  });
  RoundTripTest(
      "({ a: new Number(6), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Number"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  RoundTripTest("new String('Qu\\xe9bec')", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === String.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.valueOf() === 'Qu\\xe9bec'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 6"));
  });
  RoundTripTest("new String('\\ud83d\\udc4a')", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === String.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.valueOf() === '\\ud83d\\udc4a'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 2"));
  });
  RoundTripTest(
      "({ a: new String(), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof String"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, RejectsOtherValueObjects) {
  // This is a roundabout way of getting an instance of Symbol.
  InvalidEncodeTest("Object.valueOf.apply(Symbol())");
}

TEST_F(ValueSerializerTest, DecodeValueObjects) {
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x79, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Boolean.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === true"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x78, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Boolean.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === false"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01,
       0x79, 0x3f, 0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Boolean"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45,
       0xc0, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Number.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf8, 0x7f, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Number.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Number.isNaN(result.valueOf())"));
             });
#else
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6e, 0xc0, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Number.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6e, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Number.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Number.isNaN(result.valueOf())"));
             });
#endif
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f,
       0x01, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x40, 0x3f,
       0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Number"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x73, 0x07, 0x51, 0x75, 0xc3, 0xa9, 0x62,
              0x65, 0x63, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === String.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.valueOf() === 'Qu\\xe9bec'"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 6"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x73, 0x04, 0xf0, 0x9f, 0x91, 0x8a},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === String.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.valueOf() === '\\ud83d\\udc4a'"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 2"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01,
       0x61, 0x3f, 0x01, 0x73, 0x00, 0x3f, 0x02, 0x53, 0x01,
       0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof String"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });

  // String object containing a Latin-1 string.
  DecodeTest({0xff, 0x0c, 0x73, 0x22, 0x06, 'Q', 'u', 0xe9, 'b', 'e', 'c'},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === String.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.valueOf() === 'Qu\\xe9bec'"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 6"));
             });
}

TEST_F(ValueSerializerTest, RoundTripRegExp) {
  RoundTripTest("/foo/g", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsRegExp());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === RegExp.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '/foo/g'"));
  });
  RoundTripTest("new RegExp('Qu\\xe9bec', 'i')", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsRegExp());
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '/Qu\\xe9bec/i'"));
  });
  RoundTripTest("new RegExp('\\ud83d\\udc4a', 'ug')",
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsRegExp());
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "result.toString() === '/\\ud83d\\udc4a/gu'"));
                });
  RoundTripTest(
      "({ a: /foo/gi, get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof RegExp"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeRegExp) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x01},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === RegExp.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/foo/g'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x07, 0x51, 0x75, 0xc3, 0xa9, 0x62,
              0x65, 0x63, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/Qu\\xe9bec/i'"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x52, 0x04, 0xf0, 0x9f, 0x91, 0x8a, 0x11, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsRegExp());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.toString() === '/\\ud83d\\udc4a/gu'"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61,
       0x3f, 0x01, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x03, 0x3f, 0x02,
       0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof RegExp"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });

  // RegExp containing a Latin-1 string.
  DecodeTest(
      {0xff, 0x0c, 0x52, 0x22, 0x06, 'Q', 'u', 0xe9, 'b', 'e', 'c', 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsRegExp());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.toString() === '/Qu\\xe9bec/i'"));
      });
}

// Tests that invalid flags are not accepted by the deserializer.
TEST_F(ValueSerializerTest, DecodeRegExpDotAll) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x1f},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === RegExp.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/foo/gimuy'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x3f},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === RegExp.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/foo/gimsuy'"));
             });
  InvalidDecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x7f});
}

TEST_F(ValueSerializerTest, RoundTripMap) {
  RoundTripTest(
      "(() => { var m = new Map(); m.set(42, 'foo'); return m; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsMap());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Map.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.get(42) === 'foo'"));
      });
  RoundTripTest("(() => { var m = new Map(); m.set(m, m); return m; })()",
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsMap());
                  EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "result.get(result) === result"));
                });
  // Iteration order must be preserved.
  RoundTripTest(
      "(() => {"
      "  var m = new Map();"
      "  m.set(1, 0); m.set('a', 0); m.set(3, 0); m.set(2, 0);"
      "  return m;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsMap());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys()).toString() === '1,a,3,2'"));
      });
}

TEST_F(ValueSerializerTest, DecodeMap) {
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x3b, 0x3f, 0x01, 0x49, 0x54, 0x3f, 0x01, 0x53,
       0x03, 0x66, 0x6f, 0x6f, 0x3a, 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsMap());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Map.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.get(42) === 'foo'"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3b, 0x3f, 0x01, 0x5e, 0x00, 0x3f, 0x01,
              0x5e, 0x00, 0x3a, 0x02, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsMap());
               EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.get(result) === result"));
             });
  // Iteration order must be preserved.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3b, 0x3f, 0x01, 0x49, 0x02, 0x3f,
              0x01, 0x49, 0x00, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01,
              0x49, 0x00, 0x3f, 0x01, 0x49, 0x06, 0x3f, 0x01, 0x49, 0x00,
              0x3f, 0x01, 0x49, 0x04, 0x3f, 0x01, 0x49, 0x00, 0x3a, 0x08},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsMap());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Array.from(result.keys()).toString() === '1,a,3,2'"));
             });
}

TEST_F(ValueSerializerTest, RoundTripMapWithTrickyGetters) {
  // Even if an entry is removed or reassigned, the original key/value pair is
  // used.
  RoundTripTest(
      "(() => {"
      "  var m = new Map();"
      "  m.set(0, { get a() {"
      "    m.delete(1); m.set(2, 'baz'); m.set(3, 'quux');"
      "  }});"
      "  m.set(1, 'foo');"
      "  m.set(2, 'bar');"
      "  return m;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsMap());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys()).toString() === '0,1,2'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.get(1) === 'foo'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.get(2) === 'bar'"));
      });
  // However, deeper modifications of objects yet to be serialized still apply.
  RoundTripTest(
      "(() => {"
      "  var m = new Map();"
      "  var key = { get a() { value.foo = 'bar'; } };"
      "  var value = { get a() { key.baz = 'quux'; } };"
      "  m.set(key, value);"
      "  return m;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsMap());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "!('baz' in Array.from(result.keys())[0])"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.values())[0].foo === 'bar'"));
      });
}

TEST_F(ValueSerializerTest, RoundTripSet) {
  RoundTripTest(
      "(() => { var s = new Set(); s.add(42); s.add('foo'); return s; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Set.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 2"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.has(42)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.has('foo')"));
      });
  RoundTripTest(
      "(() => { var s = new Set(); s.add(s); return s; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.has(result)"));
      });
  // Iteration order must be preserved.
  RoundTripTest(
      "(() => {"
      "  var s = new Set();"
      "  s.add(1); s.add('a'); s.add(3); s.add(2);"
      "  return s;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys()).toString() === '1,a,3,2'"));
      });
}

TEST_F(ValueSerializerTest, DecodeSet) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x27, 0x3f, 0x01, 0x49, 0x54, 0x3f, 0x01,
              0x53, 0x03, 0x66, 0x6f, 0x6f, 0x2c, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsSet());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Set.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 2"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.has(42)"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.has('foo')"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x27, 0x3f, 0x01, 0x5e, 0x00, 0x2c, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.size === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.has(result)"));
      });
  // Iteration order must be preserved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x27, 0x3f, 0x01, 0x49, 0x02, 0x3f, 0x01, 0x53,
       0x01, 0x61, 0x3f, 0x01, 0x49, 0x06, 0x3f, 0x01, 0x49, 0x04, 0x2c, 0x04},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys()).toString() === '1,a,3,2'"));
      });
}

TEST_F(ValueSerializerTest, RoundTripSetWithTrickyGetters) {
  // Even if an element is added or removed during serialization, the original
  // set of elements is used.
  RoundTripTest(
      "(() => {"
      "  var s = new Set();"
      "  s.add({ get a() { s.delete(1); s.add(2); } });"
      "  s.add(1);"
      "  return s;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys()).toString() === '[object Object],1'"));
      });
  // However, deeper modifications of objects yet to be serialized still apply.
  RoundTripTest(
      "(() => {"
      "  var s = new Set();"
      "  var first = { get a() { second.foo = 'bar'; } };"
      "  var second = { get a() { first.baz = 'quux'; } };"
      "  s.add(first);"
      "  s.add(second);"
      "  return s;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsSet());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "!('baz' in Array.from(result.keys())[0])"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Array.from(result.keys())[1].foo === 'bar'"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArrayBuffer) {
  RoundTripTest("new ArrayBuffer()", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArrayBuffer());
    EXPECT_EQ(0u, ArrayBuffer::Cast(*value)->ByteLength());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === ArrayBuffer.prototype"));
  });
  RoundTripTest("new Uint8Array([0, 128, 255]).buffer",
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsArrayBuffer());
                  EXPECT_EQ(3u, ArrayBuffer::Cast(*value)->ByteLength());
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "new Uint8Array(result).toString() === '0,128,255'"));
                });
  RoundTripTest(
      "({ a: new ArrayBuffer(), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.a instanceof ArrayBuffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeArrayBuffer) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x42, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArrayBuffer());
               EXPECT_EQ(0u, ArrayBuffer::Cast(*value)->ByteLength());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === ArrayBuffer.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x42, 0x03, 0x00, 0x80, 0xff, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArrayBuffer());
               EXPECT_EQ(3u, ArrayBuffer::Cast(*value)->ByteLength());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "new Uint8Array(result).toString() === '0,128,255'"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01,
       0x61, 0x3f, 0x01, 0x42, 0x00, 0x3f, 0x02, 0x53, 0x01,
       0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.a instanceof ArrayBuffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeInvalidArrayBuffer) {
  InvalidDecodeTest({0xff, 0x09, 0x42, 0xff, 0xff, 0x00});
}

// An array buffer allocator that never has available memory.
class OOMArrayBufferAllocator : public ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t) override { return nullptr; }
  void* AllocateUninitialized(size_t) override { return nullptr; }
  void* Reserve(size_t length) override { return nullptr; }
  void Free(void* data, size_t length, AllocationMode mode) override {}
  void Free(void*, size_t) override {}
  void SetProtection(void* data, size_t length,
                     Protection protection) override {}
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

    const std::vector<uint8_t> data = {0xff, 0x09, 0x3f, 0x00, 0x42,
                                       0x03, 0x00, 0x80, 0xff, 0x00};
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
      const uint8_t data[kTestByteLength] = {0x00, 0x01, 0x80, 0xff};
      memcpy(output_buffer_->GetContents().Data(), data, kTestByteLength);
    }
  }

  const Local<ArrayBuffer>& input_buffer() { return input_buffer_; }
  const Local<ArrayBuffer>& output_buffer() { return output_buffer_; }

  void BeforeEncode(ValueSerializer* serializer) override {
    serializer->TransferArrayBuffer(0, input_buffer_);
  }

  void AfterEncode() override { input_buffer_->Neuter(); }

  void BeforeDecode(ValueDeserializer* deserializer) override {
    deserializer->TransferArrayBuffer(0, output_buffer_);
  }

 private:
  Local<ArrayBuffer> input_buffer_;
  Local<ArrayBuffer> output_buffer_;
};

TEST_F(ValueSerializerTestWithArrayBufferTransfer,
       RoundTripArrayBufferTransfer) {
  RoundTripTest([this]() { return input_buffer(); },
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsArrayBuffer());
                  EXPECT_EQ(output_buffer(), value);
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "new Uint8Array(result).toString() === '0,1,128,255'"));
                });
  RoundTripTest(
      [this]() {
        Local<Object> object = Object::New(isolate());
        EXPECT_TRUE(object
                        ->CreateDataProperty(serialization_context(),
                                             StringFromUtf8("a"),
                                             input_buffer())
                        .FromMaybe(false));
        EXPECT_TRUE(object
                        ->CreateDataProperty(serialization_context(),
                                             StringFromUtf8("b"),
                                             input_buffer())
                        .FromMaybe(false));
        return object;
      },
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.a instanceof ArrayBuffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "new Uint8Array(result.a).toString() === '0,1,128,255'"));
      });
}

TEST_F(ValueSerializerTest, RoundTripTypedArray) {
// Check that the right type comes out the other side for every kind of typed
// array.
#define TYPED_ARRAY_ROUND_TRIP_TEST(Type, type, TYPE, ctype, size)      \
  RoundTripTest("new " #Type "Array(2)", [this](Local<Value> value) {   \
    ASSERT_TRUE(value->Is##Type##Array());                              \
    EXPECT_EQ(2u * size, TypedArray::Cast(*value)->ByteLength());       \
    EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());                  \
    EXPECT_TRUE(EvaluateScriptForResultBool(                            \
        "Object.getPrototypeOf(result) === " #Type "Array.prototype")); \
  });
  TYPED_ARRAYS(TYPED_ARRAY_ROUND_TRIP_TEST)
#undef TYPED_ARRAY_CASE

  // Check that values of various kinds are suitably preserved.
  RoundTripTest("new Uint8Array([1, 128, 255])", [this](Local<Value> value) {
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '1,128,255'"));
  });
  RoundTripTest("new Int16Array([0, 256, -32768])", [this](Local<Value> value) {
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '0,256,-32768'"));
  });
  RoundTripTest("new Float32Array([0, -0.5, NaN, Infinity])",
                [this](Local<Value> value) {
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "result.toString() === '0,-0.5,NaN,Infinity'"));
                });

  // Array buffer views sharing a buffer should do so on the other side.
  // Similarly, multiple references to the same typed array should be resolved.
  RoundTripTest(
      "(() => {"
      "  var buffer = new ArrayBuffer(32);"
      "  return {"
      "    u8: new Uint8Array(buffer),"
      "    get u8_2() { return this.u8; },"
      "    f32: new Float32Array(buffer, 4, 5),"
      "    b: buffer,"
      "  };"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.u8 instanceof Uint8Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.u8 === result.u8_2"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.f32 instanceof Float32Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.u8.buffer === result.f32.buffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.f32.byteOffset === 4"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.f32.length === 5"));
      });
}

TEST_F(ValueSerializerTest, DecodeTypedArray) {
  // Check that the right type comes out the other side for every kind of typed
  // array.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x02, 0x00, 0x00, 0x56,
              0x42, 0x00, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsUint8Array());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Uint8Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x02, 0x00, 0x00, 0x56,
              0x62, 0x00, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsInt8Array());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Int8Array.prototype"));
             });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x04, 0x00, 0x00, 0x00,
              0x00, 0x56, 0x57, 0x00, 0x04},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsUint16Array());
               EXPECT_EQ(4u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Uint16Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x04, 0x00, 0x00, 0x00,
              0x00, 0x56, 0x77, 0x00, 0x04},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsInt16Array());
               EXPECT_EQ(4u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Int16Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x08, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x44, 0x00, 0x08},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsUint32Array());
               EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Uint32Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x08, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x64, 0x00, 0x08},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32Array());
               EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Int32Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x08, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x66, 0x00, 0x08},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsFloat32Array());
               EXPECT_EQ(8u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Float32Array.prototype"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x10, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x56, 0x46, 0x00, 0x10},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsFloat64Array());
               EXPECT_EQ(16u, TypedArray::Cast(*value)->ByteLength());
               EXPECT_EQ(2u, TypedArray::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Float64Array.prototype"));
             });
#endif  // V8_TARGET_LITTLE_ENDIAN

  // Check that values of various kinds are suitably preserved.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x03, 0x01, 0x80, 0xff,
              0x56, 0x42, 0x00, 0x03, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '1,128,255'"));
             });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x06, 0x00, 0x00, 0x00,
              0x01, 0x00, 0x80, 0x56, 0x77, 0x00, 0x06},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '0,256,-32768'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x10, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0xc0, 0x7f,
              0x00, 0x00, 0x80, 0x7f, 0x56, 0x66, 0x00, 0x10},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '0,-0.5,NaN,Infinity'"));
             });
#endif  // V8_TARGET_LITTLE_ENDIAN

  // Array buffer views sharing a buffer should do so on the other side.
  // Similarly, multiple references to the same typed array should be resolved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x02, 0x75, 0x38, 0x3f,
       0x01, 0x3f, 0x01, 0x42, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x56, 0x42, 0x00, 0x20, 0x3f, 0x03, 0x53, 0x04, 0x75, 0x38, 0x5f,
       0x32, 0x3f, 0x03, 0x5e, 0x02, 0x3f, 0x03, 0x53, 0x03, 0x66, 0x33, 0x32,
       0x3f, 0x03, 0x3f, 0x03, 0x5e, 0x01, 0x56, 0x66, 0x04, 0x14, 0x3f, 0x04,
       0x53, 0x01, 0x62, 0x3f, 0x04, 0x5e, 0x01, 0x7b, 0x04, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.u8 instanceof Uint8Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.u8 === result.u8_2"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.f32 instanceof Float32Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.u8.buffer === result.f32.buffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.f32.byteOffset === 4"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.f32.length === 5"));
      });
}

TEST_F(ValueSerializerTest, DecodeInvalidTypedArray) {
  // Byte offset out of range.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x42, 0x03, 0x01});
  // Byte offset in range, offset + length out of range.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x42, 0x01, 0x03});
  // Byte offset not divisible by element size.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0x56, 0x77, 0x01, 0x02});
  // Byte length not divisible by element size.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0x56, 0x77, 0x02, 0x01});
  // Invalid view type (0xff).
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0xff, 0x01, 0x01});
}

TEST_F(ValueSerializerTest, RoundTripDataView) {
  RoundTripTest("new DataView(new ArrayBuffer(4), 1, 2)",
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsDataView());
                  EXPECT_EQ(1u, DataView::Cast(*value)->ByteOffset());
                  EXPECT_EQ(2u, DataView::Cast(*value)->ByteLength());
                  EXPECT_EQ(4u, DataView::Cast(*value)->Buffer()->ByteLength());
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "Object.getPrototypeOf(result) === DataView.prototype"));
                });
}

TEST_F(ValueSerializerTest, DecodeDataView) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x3f, 0x00, 0x42, 0x04, 0x00, 0x00, 0x00,
              0x00, 0x56, 0x3f, 0x01, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDataView());
               EXPECT_EQ(1u, DataView::Cast(*value)->ByteOffset());
               EXPECT_EQ(2u, DataView::Cast(*value)->ByteLength());
               EXPECT_EQ(4u, DataView::Cast(*value)->Buffer()->ByteLength());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === DataView.prototype"));
             });
}

TEST_F(ValueSerializerTest, DecodeInvalidDataView) {
  // Byte offset out of range.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x3f, 0x03, 0x01});
  // Byte offset in range, offset + length out of range.
  InvalidDecodeTest(
      {0xff, 0x09, 0x42, 0x02, 0x00, 0x00, 0x56, 0x3f, 0x01, 0x03});
}

class ValueSerializerTestWithSharedArrayBufferTransfer
    : public ValueSerializerTest {
 protected:
  ValueSerializerTestWithSharedArrayBufferTransfer()
      : serializer_delegate_(this) {}

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

  void BeforeDecode(ValueDeserializer* deserializer) override {
    deserializer->TransferSharedArrayBuffer(0, output_buffer_);
  }

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
        ValueSerializerTestWithSharedArrayBufferTransfer* test)
        : test_(test) {}
    MOCK_METHOD2(GetSharedArrayBufferId,
                 Maybe<uint32_t>(Isolate* isolate,
                                 Local<SharedArrayBuffer> shared_array_buffer));
    void ThrowDataCloneError(Local<String> message) override {
      test_->isolate()->ThrowException(Exception::Error(message));
    }

   private:
    ValueSerializerTestWithSharedArrayBufferTransfer* test_;
  };

#if __clang__
#pragma clang diagnostic pop
#endif

  ValueSerializer::Delegate* GetSerializerDelegate() override {
    return &serializer_delegate_;
  }

  SerializerDelegate serializer_delegate_;

 private:
  static bool flag_was_enabled_;
  std::vector<uint8_t> data_;
  Local<SharedArrayBuffer> input_buffer_;
  Local<SharedArrayBuffer> output_buffer_;
};

bool ValueSerializerTestWithSharedArrayBufferTransfer::flag_was_enabled_ =
    false;

TEST_F(ValueSerializerTestWithSharedArrayBufferTransfer,
       RoundTripSharedArrayBufferTransfer) {
  InitializeData({0x00, 0x01, 0x80, 0xff});

  EXPECT_CALL(serializer_delegate_,
              GetSharedArrayBufferId(isolate(), input_buffer()))
      .WillRepeatedly(Return(Just(0U)));

  RoundTripTest([this]() { return input_buffer(); },
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsSharedArrayBuffer());
                  EXPECT_EQ(output_buffer(), value);
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "new Uint8Array(result).toString() === '0,1,128,255'"));
                });
  RoundTripTest(
      [this]() {
        Local<Object> object = Object::New(isolate());
        EXPECT_TRUE(object
                        ->CreateDataProperty(serialization_context(),
                                             StringFromUtf8("a"),
                                             input_buffer())
                        .FromMaybe(false));
        EXPECT_TRUE(object
                        ->CreateDataProperty(serialization_context(),
                                             StringFromUtf8("b"),
                                             input_buffer())
                        .FromMaybe(false));
        return object;
      },
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.a instanceof SharedArrayBuffer"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "new Uint8Array(result.a).toString() === '0,1,128,255'"));
      });
}

TEST_F(ValueSerializerTestWithSharedArrayBufferTransfer,
       RoundTripWebAssemblyMemory) {
  bool flag_was_enabled = i::FLAG_experimental_wasm_threads;
  i::FLAG_experimental_wasm_threads = true;

  std::vector<uint8_t> data = {0x00, 0x01, 0x80, 0xff};
  data.resize(65536);
  InitializeData(data);

  EXPECT_CALL(serializer_delegate_,
              GetSharedArrayBufferId(isolate(), input_buffer()))
      .WillRepeatedly(Return(Just(0U)));

  RoundTripTest(
      [this]() -> Local<Value> {
        const int32_t kMaxPages = 1;
        auto i_isolate = reinterpret_cast<i::Isolate*>(isolate());
        i::Handle<i::JSArrayBuffer> obj = Utils::OpenHandle(*input_buffer());
        return Utils::Convert<i::WasmMemoryObject, Value>(
            i::WasmMemoryObject::New(i_isolate, obj, kMaxPages));
      },
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result instanceof WebAssembly.Memory"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.buffer.byteLength === 65536"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("new Uint8Array(result.buffer, 0, "
                                        "4).toString() === '0,1,128,255'"));
      });

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
  RoundTripTest("new ExampleHostObject(42)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === ExampleHostObject.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.value === 42"));
  });
  RoundTripTest(
      "new ExampleHostObject(0xCAFECAFE)", [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.value === 0xCAFECAFE"));
      });
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
  RoundTripTest("new ExampleHostObject(42, 0)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === ExampleHostObject.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.value === 42"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.value2 === 0"));
  });
  RoundTripTest(
      "new ExampleHostObject(0xFFFFFFFF, 0x12345678)",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.value === 0xFFFFFFFF"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.value2 === 0x12345678"));
      });
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
  RoundTripTest("new ExampleHostObject(-3.5)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === ExampleHostObject.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.value === -3.5"));
  });
  RoundTripTest("new ExampleHostObject(NaN)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool("Number.isNaN(result.value)"));
  });
  RoundTripTest("new ExampleHostObject(Infinity)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool("result.value === Infinity"));
  });
  RoundTripTest("new ExampleHostObject(-0)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool("1/result.value === -Infinity"));
  });
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
  RoundTripTest("new ExampleHostObject()", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    ASSERT_TRUE(Object::Cast(*value)->InternalFieldCount());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === ExampleHostObject.prototype"));
  });
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
  RoundTripTest(
      "({ a: new ExampleHostObject(), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.a instanceof ExampleHostObject"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTestWithHostObject, DecodeSimpleHostObject) {
  EXPECT_CALL(deserializer_delegate_, ReadHostObject(isolate()))
      .WillRepeatedly(Invoke([this](Isolate*) {
        EXPECT_TRUE(ReadExampleHostObjectTag());
        return NewHostObject(deserialization_context(), 0, nullptr);
      }));
  DecodeTest(
      {0xff, 0x0d, 0x5c, kExampleHostObjectTag}, [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === ExampleHostObject.prototype"));
      });
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
      "({ a: new Uint8Array([1, 2, 3]), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.a instanceof Uint8Array"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.a.toString() === '4,5,6'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
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
    return WasmCompiledModule::DeserializeOrCompile(
               isolate(), {nullptr, 0},
               {kIncrementerWasm, sizeof(kIncrementerWasm)})
        .ToLocalChecked();
  }

  void ExpectPass() {
    RoundTripTest(
        [this]() { return MakeWasm(); },
        [this](Local<Value> value) {
          ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
          EXPECT_TRUE(EvaluateScriptForResultBool(
              "new WebAssembly.Instance(result).exports.increment(8) === 9"));
        });
  }

  void ExpectFail() {
    EncodeTest(
        [this]() { return MakeWasm(); },
        [this](const std::vector<uint8_t>& data) { InvalidDecodeTest(data); });
  }

  Local<Value> GetComplexObjectWithDuplicate() {
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
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "result.mod1 instanceof WebAssembly.Module"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "result.mod2 instanceof WebAssembly.Module"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.num === 2"));
  }

  Local<Value> GetComplexObjectWithMany() {
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
  InvalidEncodeTest(
      [this]() { return MakeWasm(); },
      [](Local<Message> message) {
        size_t msg_len = static_cast<size_t>(message->Get()->Length());
        std::unique_ptr<char[]> buff(new char[msg_len + 1]);
        message->Get()->WriteOneByte(reinterpret_cast<uint8_t*>(buff.get()));
        // the message ends with the custom error string
        size_t custom_msg_len = strlen(kUnsupportedSerialization);
        ASSERT_GE(msg_len, custom_msg_len);
        size_t start_pos = msg_len - custom_msg_len;
        ASSERT_EQ(strcmp(&buff.get()[start_pos], kUnsupportedSerialization), 0);
      });
}

// The default deserializer throws if wasm transfer is attempted
TEST_F(ValueSerializerTestWithWasm, DefaultDeserializationDelegate) {
  EnableTransferSerialization();
  EnableDefaultDeserializer();
  EncodeTest(
      [this]() { return MakeWasm(); },
      [this](const std::vector<uint8_t>& data) { InvalidDecodeTest(data); });
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
  RoundTripTest(
      [this]() { return GetComplexObjectWithDuplicate(); },
      [this](Local<Value> value) {
        VerifyComplexObject(value);
        EXPECT_TRUE(EvaluateScriptForResultBool("result.mod1 === result.mod2"));
      });
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectDuplicateInline) {
  SetExpectInlineWasm(true);
  RoundTripTest(
      [this]() { return GetComplexObjectWithDuplicate(); },
      [this](Local<Value> value) {
        VerifyComplexObject(value);
        EXPECT_TRUE(EvaluateScriptForResultBool("result.mod1 === result.mod2"));
      });
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectWithManyTransfer) {
  EnableTransferSerialization();
  EnableTransferDeserialization();
  RoundTripTest(
      [this]() { return GetComplexObjectWithMany(); },
      [this](Local<Value> value) {
        VerifyComplexObject(value);
        EXPECT_TRUE(EvaluateScriptForResultBool("result.mod1 != result.mod2"));
      });
}

TEST_F(ValueSerializerTestWithWasm, ComplexObjectWithManyInline) {
  SetExpectInlineWasm(true);
  RoundTripTest(
      [this]() { return GetComplexObjectWithMany(); },
      [this](Local<Value> value) {
        VerifyComplexObject(value);
        EXPECT_TRUE(EvaluateScriptForResultBool("result.mod1 != result.mod2"));
      });
}

// As produced around Chrome 56.
const unsigned char kSerializedIncrementerWasm[] = {
    0xff, 0x09, 0x3f, 0x00, 0x57, 0x79, 0x2d, 0x00, 0x61, 0x73, 0x6d, 0x0d,
    0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01, 0x7f, 0x03,
    0x02, 0x01, 0x00, 0x07, 0x0d, 0x01, 0x09, 0x69, 0x6e, 0x63, 0x72, 0x65,
    0x6d, 0x65, 0x6e, 0x74, 0x00, 0x00, 0x0a, 0x08, 0x01, 0x06, 0x00, 0x20,
    0x00, 0x41, 0x01, 0x6a, 0xf8, 0x04, 0xa1, 0x06, 0xde, 0xc0, 0xc6, 0x44,
    0x3c, 0x29, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x02, 0x00, 0x00, 0x81, 0x4e,
    0xce, 0x7c, 0x05, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x30, 0x02,
    0x00, 0x00, 0xb0, 0x25, 0x30, 0xe3, 0xf2, 0xdb, 0x2e, 0x48, 0x00, 0x00,
    0x00, 0x80, 0xe8, 0x00, 0x00, 0x80, 0xe0, 0x01, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x07, 0x08, 0x00, 0x00, 0x09, 0x04,
    0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3c, 0x8c, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x01, 0x10, 0x8c, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x70, 0x94, 0x01, 0x0c, 0x8b,
    0xc1, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x25, 0xdc, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x9e, 0x01, 0x10, 0x8c, 0xc0, 0x00, 0x00,
    0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x84, 0xc0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x7d, 0x01, 0x1a, 0xe1, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x23, 0x88, 0x42, 0x32, 0x03,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x04, 0x00,
    0x00, 0x02, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x49, 0x3b, 0xa5, 0x60, 0x0c, 0x00,
    0x00, 0x0f, 0x86, 0x04, 0x00, 0x00, 0x00, 0x83, 0xc0, 0x01, 0xc3, 0x55,
    0x48, 0x89, 0xe5, 0x49, 0xba, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
    0x00, 0x41, 0x52, 0x48, 0x83, 0xec, 0x08, 0x48, 0x89, 0x45, 0xf0, 0x48,
    0xbb, 0xb0, 0x67, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xc0, 0x48,
    0xbe, 0xe1, 0x57, 0x81, 0x85, 0xf6, 0x14, 0x00, 0x00, 0xe8, 0xfc, 0x3c,
    0xea, 0xff, 0x48, 0x8b, 0x45, 0xf0, 0x48, 0x8b, 0xe5, 0x5d, 0xeb, 0xbf,
    0x66, 0x90, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x44, 0x00,
    0x00, 0x00, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0x20, 0x84, 0x0f, 0x7d, 0x01, 0x0d, 0x00, 0x0f, 0x04,
    0x6d, 0x08, 0x0f, 0xf0, 0x02, 0x80, 0x94, 0x01, 0x0c, 0x8b, 0xc1, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xed, 0xa9, 0x2d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x9e, 0xe0, 0x38, 0x1a, 0x61, 0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x23, 0x88, 0x42, 0x32, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x02, 0xf9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x55, 0x48, 0x89, 0xe5, 0x56, 0x57, 0x48,
    0x8b, 0x45, 0x10, 0xe8, 0x11, 0xed, 0xed, 0xff, 0xa8, 0x01, 0x0f, 0x85,
    0x2d, 0x00, 0x00, 0x00, 0x48, 0xc1, 0xe8, 0x20, 0xc5, 0xf9, 0x57, 0xc0,
    0xc5, 0xfb, 0x2a, 0xc0, 0xc4, 0xe1, 0xfb, 0x2c, 0xc0, 0x48, 0x83, 0xf8,
    0x01, 0x0f, 0x80, 0x34, 0x00, 0x00, 0x00, 0x8b, 0xc0, 0xe8, 0x27, 0xfe,
    0xff, 0xff, 0x48, 0xc1, 0xe0, 0x20, 0x48, 0x8b, 0xe5, 0x5d, 0xc2, 0x10,
    0x00, 0x49, 0x39, 0x45, 0xa0, 0x0f, 0x84, 0x07, 0x00, 0x00, 0x00, 0xc5,
    0xfb, 0x10, 0x40, 0x07, 0xeb, 0xce, 0x49, 0xba, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf8, 0x7f, 0xc4, 0xc1, 0xf9, 0x6e, 0xc2, 0xeb, 0xbd, 0x48,
    0x83, 0xec, 0x08, 0xc5, 0xfb, 0x11, 0x04, 0x24, 0xe8, 0xcc, 0xfe, 0xff,
    0xff, 0x48, 0x83, 0xc4, 0x08, 0xeb, 0xb8, 0x66, 0x90, 0x02, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
    0x0f, 0x39, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0x00,
    0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x20, 0x84,
    0x0f, 0xcc, 0x6e, 0x7d, 0x01, 0x72, 0x98, 0x00, 0x0f, 0xdc, 0x6d, 0x0c,
    0x0f, 0xb0, 0x84, 0x0d, 0x04, 0x84, 0xe3, 0xc0, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x84, 0xe0, 0x84, 0x84, 0x18, 0x2f, 0x2f, 0x2f,
    0x2f, 0x2f};

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModule) {
  if (true) return;  // TODO(mtrofin): fix this test
  std::vector<uint8_t> raw(
      kSerializedIncrementerWasm,
      kSerializedIncrementerWasm + sizeof(kSerializedIncrementerWasm));
  DecodeTest(raw, [this](Local<Value> value) {
    ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "new WebAssembly.Instance(result).exports.increment(8) === 9"));
  });
}

// As above, but with empty compiled data. Should work due to fallback to wire
// data.
const unsigned char kSerializedIncrementerWasmWithInvalidCompiledData[] = {
    0xff, 0x09, 0x3f, 0x00, 0x57, 0x79, 0x2d, 0x00, 0x61, 0x73, 0x6d,
    0x0d, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01, 0x7f, 0x01,
    0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x0d, 0x01, 0x09, 0x69, 0x6e,
    0x63, 0x72, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x00, 0x00, 0x0a, 0x08,
    0x01, 0x06, 0x00, 0x20, 0x00, 0x41, 0x01, 0x6a, 0x00};

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModuleWithInvalidCompiledData) {
  if (true) return;  // TODO(titzer): regenerate this test
  std::vector<uint8_t> raw(
      kSerializedIncrementerWasmWithInvalidCompiledData,
      kSerializedIncrementerWasmWithInvalidCompiledData +
          sizeof(kSerializedIncrementerWasmWithInvalidCompiledData));
  DecodeTest(raw, [this](Local<Value> value) {
    ASSERT_TRUE(value->IsWebAssemblyCompiledModule());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "new WebAssembly.Instance(result).exports.increment(8) === 9"));
  });
}

// As above, but also with empty wire data. Should fail.
const unsigned char kSerializedIncrementerWasmInvalid[] = {
    0xff, 0x09, 0x3f, 0x00, 0x57, 0x79, 0x00, 0x00};

TEST_F(ValueSerializerTestWithWasm,
       DecodeWasmModuleWithInvalidCompiledAndWireData) {
  std::vector<uint8_t> raw(kSerializedIncrementerWasmInvalid,
                           kSerializedIncrementerWasmInvalid +
                               sizeof(kSerializedIncrementerWasmInvalid));
  InvalidDecodeTest(raw);
}

TEST_F(ValueSerializerTestWithWasm, DecodeWasmModuleWithInvalidDataLength) {
  InvalidDecodeTest({0xff, 0x09, 0x3f, 0x00, 0x57, 0x79, 0x7f, 0x00});
  InvalidDecodeTest({0xff, 0x09, 0x3f, 0x00, 0x57, 0x79, 0x00, 0x7f});
}

}  // namespace
}  // namespace v8
