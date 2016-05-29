// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Parser.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace protocol {

TEST(ParserTest, Reading)
{
    protocol::Value* tmpValue;
    std::unique_ptr<protocol::Value> root;
    std::unique_ptr<protocol::Value> root2;
    String16 strVal;
    int intVal = 0;

    // some whitespace checking
    root = parseJSON("    null    ");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNull, root->type());

    // Invalid JSON string
    root = parseJSON("nu");
    EXPECT_FALSE(root.get());

    // Simple bool
    root = parseJSON("true  ");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeBoolean, root->type());

    // Embedded comment
    root = parseJSON("40 /*/");
    EXPECT_FALSE(root.get());
    root = parseJSON("/* comment */null");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNull, root->type());
    root = parseJSON("40 /* comment */");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(40, intVal);
    root = parseJSON("/**/ 40 /* multi-line\n comment */ // more comment");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(40, intVal);
    root = parseJSON("true // comment");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeBoolean, root->type());
    root = parseJSON("/* comment */\"sample string\"");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("sample string", strVal);
    root = parseJSON("[1, /* comment, 2 ] */ \n 3]");
    ASSERT_TRUE(root.get());
    protocol::ListValue* list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(2u, list->size());
    tmpValue = list->at(0);
    ASSERT_TRUE(tmpValue);
    EXPECT_TRUE(tmpValue->asNumber(&intVal));
    EXPECT_EQ(1, intVal);
    tmpValue = list->at(1);
    ASSERT_TRUE(tmpValue);
    EXPECT_TRUE(tmpValue->asNumber(&intVal));
    EXPECT_EQ(3, intVal);
    root = parseJSON("[1, /*a*/2, 3]");
    ASSERT_TRUE(root.get());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(3u, list->size());
    root = parseJSON("/* comment **/42");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(42, intVal);
    root = parseJSON(
        "/* comment **/\n"
        "// */ 43\n"
        "44");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(44, intVal);

    // Test number formats
    root = parseJSON("43");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(43, intVal);

    // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
    root = parseJSON("043");
    EXPECT_FALSE(root.get());
    root = parseJSON("0x43");
    EXPECT_FALSE(root.get());
    root = parseJSON("00");
    EXPECT_FALSE(root.get());

    // Test 0 (which needs to be special cased because of the leading zero
    // clause).
    root = parseJSON("0");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    intVal = 1;
    EXPECT_TRUE(root->asNumber(&intVal));
    EXPECT_EQ(0, intVal);

    // Numbers that overflow ints should succeed, being internally promoted to
    // storage as doubles
    root = parseJSON("2147483648");
    ASSERT_TRUE(root.get());
    double doubleVal;
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(2147483648.0, doubleVal);
    root = parseJSON("-2147483649");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(-2147483649.0, doubleVal);

    // Parse a double
    root = parseJSON("43.1");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(43.1, doubleVal);

    root = parseJSON("4.3e-1");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(.43, doubleVal);

    root = parseJSON("2.1e0");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(2.1, doubleVal);

    root = parseJSON("2.1e+0001");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(21.0, doubleVal);

    root = parseJSON("0.01");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(0.01, doubleVal);

    root = parseJSON("1.00");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeNumber, root->type());
    doubleVal = 0.0;
    EXPECT_TRUE(root->asNumber(&doubleVal));
    EXPECT_DOUBLE_EQ(1.0, doubleVal);

    // Fractional parts must have a digit before and after the decimal point.
    root = parseJSON("1.");
    EXPECT_FALSE(root.get());
    root = parseJSON(".1");
    EXPECT_FALSE(root.get());
    root = parseJSON("1.e10");
    EXPECT_FALSE(root.get());

    // Exponent must have a digit following the 'e'.
    root = parseJSON("1e");
    EXPECT_FALSE(root.get());
    root = parseJSON("1E");
    EXPECT_FALSE(root.get());
    root = parseJSON("1e1.");
    EXPECT_FALSE(root.get());
    root = parseJSON("1e1.0");
    EXPECT_FALSE(root.get());

    // INF/-INF/NaN are not valid
    root = parseJSON("NaN");
    EXPECT_FALSE(root.get());
    root = parseJSON("nan");
    EXPECT_FALSE(root.get());
    root = parseJSON("inf");
    EXPECT_FALSE(root.get());

    // Invalid number formats
    root = parseJSON("4.3.1");
    EXPECT_FALSE(root.get());
    root = parseJSON("4e3.1");
    EXPECT_FALSE(root.get());

    // Test string parser
    root = parseJSON("\"hello world\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("hello world", strVal);

    // Empty string
    root = parseJSON("\"\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("", strVal);

    // Test basic string escapes
    root = parseJSON("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ(" \"\\/\b\f\n\r\t\v", strVal);

    // Test hex and unicode escapes including the null character.
    root = parseJSON("\"\\x41\\x00\\u1234\"");
    EXPECT_FALSE(root.get());

    // Test invalid strings
    root = parseJSON("\"no closing quote");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"\\z invalid escape char\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"not enough escape chars\\u123\"");
    EXPECT_FALSE(root.get());
    root = parseJSON("\"extra backslash at end of input\\\"");
    EXPECT_FALSE(root.get());

    // Basic array
    root = parseJSON("[true, false, null]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(3U, list->size());

    // Empty array
    root = parseJSON("[]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(0U, list->size());

    // Nested arrays
    root = parseJSON("[[true], [], [false, [], [null]], null]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(4U, list->size());

    // Invalid, missing close brace.
    root = parseJSON("[[true], [], [false, [], [null]], null");
    EXPECT_FALSE(root.get());

    // Invalid, too many commas
    root = parseJSON("[true,, null]");
    EXPECT_FALSE(root.get());

    // Invalid, no commas
    root = parseJSON("[true null]");
    EXPECT_FALSE(root.get());

    // Invalid, trailing comma
    root = parseJSON("[true,]");
    EXPECT_FALSE(root.get());

    root = parseJSON("[true]");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(1U, list->size());
    tmpValue = list->at(0);
    ASSERT_TRUE(tmpValue);
    EXPECT_EQ(Value::TypeBoolean, tmpValue->type());
    bool boolValue = false;
    EXPECT_TRUE(tmpValue->asBoolean(&boolValue));
    EXPECT_TRUE(boolValue);

    // Don't allow empty elements.
    root = parseJSON("[,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[true,,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[,true,]");
    EXPECT_FALSE(root.get());
    root = parseJSON("[true,,false]");
    EXPECT_FALSE(root.get());

    // Test objects
    root = parseJSON("{}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());

    root = parseJSON("{\"number\":9.87654321, \"null\":null , \"S\" : \"str\" }");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    protocol::DictionaryValue* objectVal = DictionaryValue::cast(root.get());
    ASSERT_TRUE(objectVal);
    doubleVal = 0.0;
    EXPECT_TRUE(objectVal->getNumber("number", &doubleVal));
    EXPECT_DOUBLE_EQ(9.87654321, doubleVal);
    protocol::Value* nullVal = objectVal->get("null");
    ASSERT_TRUE(nullVal);
    EXPECT_EQ(Value::TypeNull, nullVal->type());
    EXPECT_TRUE(objectVal->getString("S", &strVal));
    EXPECT_EQ("str", strVal);

    // Test newline equivalence.
    root2 = parseJSON(
        "{\n"
        "  \"number\":9.87654321,\n"
        "  \"null\":null,\n"
        "  \"S\":\"str\"\n"
        "}\n");
    ASSERT_TRUE(root2.get());
    EXPECT_EQ(root->toJSONString(), root2->toJSONString());

    root2 = parseJSON(
        "{\r\n"
        "  \"number\":9.87654321,\r\n"
        "  \"null\":null,\r\n"
        "  \"S\":\"str\"\r\n"
        "}\r\n");
    ASSERT_TRUE(root2.get());
    EXPECT_EQ(root->toJSONString(), root2->toJSONString());

    // Test nesting
    root = parseJSON("{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root.get());
    ASSERT_TRUE(objectVal);
    protocol::DictionaryValue* innerObject = objectVal->getObject("inner");
    ASSERT_TRUE(innerObject);
    protocol::ListValue* innerArray = innerObject->getArray("array");
    ASSERT_TRUE(innerArray);
    EXPECT_EQ(1U, innerArray->size());
    boolValue = true;
    EXPECT_TRUE(objectVal->getBoolean("false", &boolValue));
    EXPECT_FALSE(boolValue);
    innerObject = objectVal->getObject("d");
    EXPECT_TRUE(innerObject);

    // Test keys with periods
    root = parseJSON("{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root.get());
    ASSERT_TRUE(objectVal);
    int integerValue = 0;
    EXPECT_TRUE(objectVal->getNumber("a.b", &integerValue));
    EXPECT_EQ(3, integerValue);
    EXPECT_TRUE(objectVal->getNumber("c", &integerValue));
    EXPECT_EQ(2, integerValue);
    innerObject = objectVal->getObject("d.e.f");
    ASSERT_TRUE(innerObject);
    EXPECT_EQ(1U, innerObject->size());
    EXPECT_TRUE(innerObject->getNumber("g.h.i.j", &integerValue));
    EXPECT_EQ(1, integerValue);

    root = parseJSON("{\"a\":{\"b\":2},\"a.b\":1}");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeObject, root->type());
    objectVal = DictionaryValue::cast(root.get());
    ASSERT_TRUE(objectVal);
    innerObject = objectVal->getObject("a");
    ASSERT_TRUE(innerObject);
    EXPECT_TRUE(innerObject->getNumber("b", &integerValue));
    EXPECT_EQ(2, integerValue);
    EXPECT_TRUE(objectVal->getNumber("a.b", &integerValue));
    EXPECT_EQ(1, integerValue);

    // Invalid, no closing brace
    root = parseJSON("{\"a\": true");
    EXPECT_FALSE(root.get());

    // Invalid, keys must be quoted
    root = parseJSON("{foo:true}");
    EXPECT_FALSE(root.get());

    // Invalid, trailing comma
    root = parseJSON("{\"a\":true,}");
    EXPECT_FALSE(root.get());

    // Invalid, too many commas
    root = parseJSON("{\"a\":true,,\"b\":false}");
    EXPECT_FALSE(root.get());

    // Invalid, no separator
    root = parseJSON("{\"a\" \"b\"}");
    EXPECT_FALSE(root.get());

    // Invalid, lone comma.
    root = parseJSON("{,}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{\"a\":true,,}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{,\"a\":true}");
    EXPECT_FALSE(root.get());
    root = parseJSON("{\"a\":true,,\"b\":false}");
    EXPECT_FALSE(root.get());

    // Test stack overflow
    String16Builder evil;
    evil.reserveCapacity(2000000);
    for (int i = 0; i < 1000000; ++i)
        evil.append('[');
    for (int i = 0; i < 1000000; ++i)
        evil.append(']');
    root = parseJSON(evil.toString());
    EXPECT_FALSE(root.get());

    // A few thousand adjacent lists is fine.
    String16Builder notEvil;
    notEvil.reserveCapacity(15010);
    notEvil.append('[');
    for (int i = 0; i < 5000; ++i)
        notEvil.append("[],");
    notEvil.append("[]]");
    root = parseJSON(notEvil.toString());
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeArray, root->type());
    list = ListValue::cast(root.get());
    ASSERT_TRUE(list);
    EXPECT_EQ(5001U, list->size());

    // Test utf8 encoded input
    root = parseJSON("\"\\xe7\\xbd\\x91\\xe9\\xa1\\xb5\"");
    ASSERT_FALSE(root.get());

    // Test utf16 encoded strings.
    root = parseJSON("\"\\u20ac3,14\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp2[] = {0x20ac, 0x33, 0x2c, 0x31, 0x34};
    EXPECT_EQ(String16(tmp2, 5), strVal);

    root = parseJSON("\"\\ud83d\\udca9\\ud83d\\udc6c\"");
    ASSERT_TRUE(root.get());
    EXPECT_EQ(Value::TypeString, root->type());
    EXPECT_TRUE(root->asString(&strVal));
    UChar tmp3[] = {0xd83d, 0xdca9, 0xd83d, 0xdc6c};
    EXPECT_EQ(String16(tmp3, 4), strVal);

    // Test literal root objects.
    root = parseJSON("null");
    EXPECT_EQ(Value::TypeNull, root->type());

    root = parseJSON("true");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asBoolean(&boolValue));
    EXPECT_TRUE(boolValue);

    root = parseJSON("10");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asNumber(&integerValue));
    EXPECT_EQ(10, integerValue);

    root = parseJSON("\"root\"");
    ASSERT_TRUE(root.get());
    EXPECT_TRUE(root->asString(&strVal));
    EXPECT_EQ("root", strVal);
}

TEST(ParserTest, InvalidSanity)
{
    const char* const invalidJson[] = {
        "/* test *",
        "{\"foo\"",
        "{\"foo\":",
        "  [",
        "\"\\u123g\"",
        "{\n\"eh:\n}",
        "////",
        "*/**/",
        "/**/",
        "/*/",
        "//**/"
    };

    for (size_t i = 0; i < 11; ++i) {
        std::unique_ptr<protocol::Value> result = parseJSON(invalidJson[i]);
        EXPECT_FALSE(result.get());
    }
}

} // namespace protocol
} // namespace blink
