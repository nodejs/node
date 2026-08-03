#include "tracing/traced_value.h"

#include <cmath>
#include <cstddef>
#include <cstring>

#include "gtest/gtest.h"

using node::tracing::TracedValue;

TEST(TracedValue, Object) {
  auto traced_value = TracedValue::Create();
  traced_value->SetString("a", "b");
  traced_value->SetInteger("b", 1);
  traced_value->SetDouble("c", 1.234);
  traced_value->SetDouble("d", NAN);
  traced_value->SetDouble("e", INFINITY);
  traced_value->SetDouble("f", -INFINITY);
  traced_value->SetDouble("g", 1.23e7);
  traced_value->SetBoolean("h", false);
  traced_value->SetBoolean("i", true);
  traced_value->SetNull("j");
  traced_value->BeginDictionary("k");
  traced_value->SetString("l", "m");
  traced_value->EndDictionary();

  std::string string;
  traced_value->AppendAsTraceFormat(&string);

  static const char* check = "{\"a\":\"b\",\"b\":1,\"c\":1.234,\"d\":\"NaN\","
                             "\"e\":\"Infinity\",\"f\":\"-Infinity\",\"g\":"
                             "1.23e+07,\"h\":false,\"i\":true,\"j\":null,\"k\":"
                             "{\"l\":\"m\"}}";

  EXPECT_EQ(check, string);
}

TEST(TracedValue, Array) {
  auto traced_value = TracedValue::CreateArray();
  traced_value->AppendString("a");
  traced_value->AppendInteger(1);
  traced_value->AppendDouble(1.234);
  traced_value->AppendDouble(NAN);
  traced_value->AppendDouble(INFINITY);
  traced_value->AppendDouble(-INFINITY);
  traced_value->AppendDouble(1.23e7);
  traced_value->AppendBoolean(false);
  traced_value->AppendBoolean(true);
  traced_value->AppendNull();
  traced_value->BeginDictionary();
  traced_value->BeginArray("foo");
  traced_value->EndArray();
  traced_value->EndDictionary();

  std::string string;
  traced_value->AppendAsTraceFormat(&string);

  static const char* check = "[\"a\",1,1.234,\"NaN\",\"Infinity\","
                             "\"-Infinity\",1.23e+07,false,true,null,"
                             "{\"foo\":[]}]";

  EXPECT_EQ(check, string);
}

#define UTF8_SEQUENCE "1" "\xE2\x82\xAC" "23\"\x01\b\f\n\r\t\\"
#if defined(NODE_HAVE_I18N_SUPPORT)
# define UTF8_RESULT                                                          \
  "\"1\\u20AC23\\\"\\u0001\\b\\f\\n\\r\\t\\\\\""
#else
# define UTF8_RESULT                                                          \
  "\"1\\u00E2\\u0082\\u00AC23\\\"\\u0001\\b\\f\\n\\r\\t\\\\\""
#endif

TEST(TracedValue, EscapingObject) {
  auto traced_value = TracedValue::Create();
  traced_value->SetString("a", UTF8_SEQUENCE);

  std::string string;
  traced_value->AppendAsTraceFormat(&string);

  static const char* check = "{\"a\":" UTF8_RESULT "}";

  EXPECT_EQ(check, string);
}

TEST(TracedValue, EscapingArray) {
  auto traced_value = TracedValue::CreateArray();
  traced_value->AppendString(UTF8_SEQUENCE);

  std::string string;
  traced_value->AppendAsTraceFormat(&string);

  static const char* check = "[" UTF8_RESULT "]";

  EXPECT_EQ(check, string);
}

TEST(TracedValue, EnvironmentArgs) {
  std::vector<std::string> args{"a", "bb", "ccc"};
  std::vector<std::string> exec_args{"--inspect", "--a-long-arg"};
  node::tracing::EnvironmentArgs env_args(args, exec_args);

  std::string string;
  env_args.Cast()->AppendAsTraceFormat(&string);

  static const char* check = "{\"args\":[\"a\",\"bb\",\"ccc\"],"
                             "\"exec_args\":[\"--inspect\",\"--a-long-arg\"]}";

  EXPECT_EQ(check, string);
}

TEST(TracedValue, AsyncWrapArgs) {
  node::tracing::AsyncWrapArgs aw_args(1, 1);

  std::string string;
  aw_args.Cast()->AppendAsTraceFormat(&string);

  static const char* check = "{\"executionAsyncId\":1,"
                             "\"triggerAsyncId\":1}";

  EXPECT_EQ(check, string);
}
