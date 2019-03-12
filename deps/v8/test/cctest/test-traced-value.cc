// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tracing/traced-value.h"
#include "test/cctest/cctest.h"

using v8::tracing::TracedValue;

TEST(FlatDictionary) {
  auto value = TracedValue::Create();
  value->SetInteger("int", 2014);
  value->SetDouble("double", 0.0);
  value->SetBoolean("bool", true);
  value->SetString("string", "string");
  std::string json = "PREFIX";
  value->AppendAsTraceFormat(&json);
  CHECK_EQ(
      "PREFIX{\"int\":2014,\"double\":0,\"bool\":true,\"string\":"
      "\"string\"}",
      json);
}

TEST(NoDotPathExpansion) {
  auto value = TracedValue::Create();
  value->SetInteger("in.t", 2014);
  value->SetDouble("doub.le", -20.25);
  value->SetBoolean("bo.ol", true);
  value->SetString("str.ing", "str.ing");
  std::string json;
  value->AppendAsTraceFormat(&json);
  CHECK_EQ(
      "{\"in.t\":2014,\"doub.le\":-20.25,\"bo.ol\":true,\"str.ing\":\"str."
      "ing\"}",
      json);
}

TEST(Hierarchy) {
  auto value = TracedValue::Create();
  value->SetInteger("i0", 2014);
  value->BeginDictionary("dict1");
  value->SetInteger("i1", 2014);
  value->BeginDictionary("dict2");
  value->SetBoolean("b2", false);
  value->EndDictionary();
  value->SetString("s1", "foo");
  value->EndDictionary();
  value->SetDouble("d0", 0.0);
  value->SetDouble("d1", 10.5);
  value->SetBoolean("b0", true);
  value->BeginArray("a1");
  value->AppendInteger(1);
  value->AppendBoolean(true);
  value->BeginDictionary();
  value->SetInteger("i2", 3);
  value->EndDictionary();
  value->EndArray();
  value->SetString("s0", "foo");

  value->BeginArray("arr1");
  value->BeginDictionary();
  value->EndDictionary();
  value->BeginArray();
  value->EndArray();
  value->BeginDictionary();
  value->EndDictionary();
  value->EndArray();

  std::string json;
  value->AppendAsTraceFormat(&json);
  CHECK_EQ(
      "{\"i0\":2014,\"dict1\":{\"i1\":2014,\"dict2\":{\"b2\":false},"
      "\"s1\":\"foo\"},\"d0\":0,\"d1\":10.5,\"b0\":true,\"a1\":[1,true,{\"i2\":"
      "3}],\"s0\":\"foo\",\"arr1\":[{},[],{}]}",
      json);
}

TEST(LongStrings) {
  std::string long_string = "supercalifragilisticexpialidocious";
  std::string long_string2 = "0123456789012345678901234567890123456789";
  char long_string3[4096];
  for (size_t i = 0; i < sizeof(long_string3); ++i)
    long_string3[i] = static_cast<char>('a' + (i % 26));
  long_string3[sizeof(long_string3) - 1] = '\0';

  auto value = TracedValue::Create();
  value->SetString("a", "short");
  value->SetString("b", long_string);
  value->BeginArray("c");
  value->AppendString(long_string2);
  value->AppendString("");
  value->BeginDictionary();
  value->SetString("a", long_string3);
  value->EndDictionary();
  value->EndArray();

  std::string json;
  value->AppendAsTraceFormat(&json);
  CHECK_EQ("{\"a\":\"short\",\"b\":\"" + long_string + "\",\"c\":[\"" +
               long_string2 + "\",\"\",{\"a\":\"" + long_string3 + "\"}]}",
           json);
}

TEST(Escaping) {
  const char* string1 = "abc\"\'\\\\x\"y\'z\n\x09\x17";
  std::string chars127;
  for (int i = 1; i <= 127; ++i) {
    chars127 += static_cast<char>(i);
  }
  auto value = TracedValue::Create();
  value->SetString("a", string1);
  value->SetString("b", chars127);

  std::string json;
  value->AppendAsTraceFormat(&json);
  // Cannot use the expected value literal directly in CHECK_EQ
  // as it fails to process the # character on Windows.
  const char* expected =
      R"({"a":"abc\"'\\\\x\"y'z\n\t\u0017","b":"\u0001\u0002\u0003\u0004\u0005)"
      R"(\u0006\u0007\b\t\n\u000B\f\r\u000E\u000F\u0010\u0011\u0012\u0013)"
      R"(\u0014\u0015\u0016\u0017\u0018\u0019\u001A\u001B\u001C\u001D\u001E)"
      R"(\u001F !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ)"
      R"([\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\u007F"})";
  CHECK_EQ(expected, json);
}

TEST(Utf8) {
  const char* string1 = "Люблю тебя, Петра творенье";
  const char* string2 = "☀\u2600\u26FF";
  auto value = TracedValue::Create();
  value->SetString("a", string1);
  value->SetString("b", string2);
  // Surrogate pair test. Smile emoji === U+1F601 === \xf0\x9f\x98\x81
  value->SetString("c", "\U0001F601");
  std::string json;
  value->AppendAsTraceFormat(&json);
  const char* expected =
      "{\"a\":\"\u041B\u044E\u0431\u043B\u044E \u0442\u0435\u0431\u044F, \u041F"
      "\u0435\u0442\u0440\u0430 \u0442\u0432\u043E\u0440\u0435\u043D\u044C"
      "\u0435\",\"b\":\"\u2600\u2600\u26FF\",\"c\":\"\xf0\x9f\x98\x81\"}";
  CHECK_EQ(expected, json);
}
