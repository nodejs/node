#include "json_utils.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <limits>
#include <sstream>

TEST(JSONUtilsTest, EscapeJsonChars) {
  using node::EscapeJsonChars;
  EXPECT_EQ("abc", EscapeJsonChars("abc"));
  EXPECT_EQ("abc\\n", EscapeJsonChars("abc\n"));
  EXPECT_EQ("abc\\nabc", EscapeJsonChars("abc\nabc"));
  EXPECT_EQ("abc\\\\", EscapeJsonChars("abc\\"));
  EXPECT_EQ("abc\\\"", EscapeJsonChars("abc\""));

  const std::string expected[0x20] = {
      "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005",
      "\\u0006", "\\u0007", "\\b",     "\\t",     "\\n",     "\\u000b",
      "\\f",     "\\r",     "\\u000e", "\\u000f", "\\u0010", "\\u0011",
      "\\u0012", "\\u0013", "\\u0014", "\\u0015", "\\u0016", "\\u0017",
      "\\u0018", "\\u0019", "\\u001a", "\\u001b", "\\u001c", "\\u001d",
      "\\u001e", "\\u001f"};
  for (int i = 0; i < 0x20; ++i) {
    char symbols[1] = { static_cast<char>(i) };
    std::string input(symbols, 1);
    EXPECT_EQ(expected[i], EscapeJsonChars(input));
    EXPECT_EQ("a" + expected[i], EscapeJsonChars("a" + input));
  }
}

TEST(JSONUtilsTest, StringWriter) {
  auto write = [](node::JSONWriter* writer) {
    writer->json_start();
    writer->json_keyvalue("text", std::string_view("\"\\\n\0", 4));
    writer->json_keyvalue("utf8", "caf\xc3\xa9");
    writer->json_arraystart("values");
    writer->json_element(true);
    writer->json_element(false);
    writer->json_element(node::JSONWriter::Null{});
    writer->json_start();
    writer->json_objectstart("empty");
    writer->json_objectend();
    writer->json_end();
    writer->json_arrayend();
    writer->json_objectend();
  };

  for (bool compact : {false, true}) {
    std::ostringstream stream;
    node::JSONWriter stream_writer(stream, compact);
    write(&stream_writer);
    std::string output;
    node::JSONWriter string_writer(output, compact);
    write(&string_writer);
    EXPECT_EQ(stream.str(), output);
    if (compact) {
      EXPECT_EQ("{\"text\":\"\\\"\\\\\\n\\u0000\",\"utf8\":\"caf\xc3\xa9\","
                "\"values\":[true,false,null,{\"empty\":{}}]}\n",
                output);
    }
  }
}

TEST(JSONUtilsTest, StringWriterNumbers) {
  std::string output;
  node::JSONWriter writer(output, true);
  writer.json_start();
  writer.json_arraystart("numbers");
  writer.json_element(std::numeric_limits<int64_t>::min());
  writer.json_element(std::numeric_limits<uint64_t>::max());
  writer.json_element(0);
  writer.json_element(-0.0);
  writer.json_element(1.23456789);
  writer.json_element(1000000.0);
  writer.json_element(0.00001);
  writer.json_element(std::numeric_limits<double>::max());
  writer.json_element(std::numeric_limits<double>::denorm_min());
  writer.json_arrayend();
  writer.json_end();
  EXPECT_EQ("{\"numbers\":[-9223372036854775808,18446744073709551615,0,-0,"
            "1.23457,1e+06,1e-05,1.79769e+308,4.94066e-324]}",
            output);
}
