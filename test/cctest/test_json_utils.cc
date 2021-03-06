#include "json_utils.h"

#include "gtest/gtest.h"

TEST(JSONUtilsTest, EscapeJsonChars) {
  using node::EscapeJsonChars;
  EXPECT_EQ("abc", EscapeJsonChars("abc"));
  EXPECT_EQ("abc\\n", EscapeJsonChars("abc\n"));
  EXPECT_EQ("abc\\nabc", EscapeJsonChars("abc\nabc"));
  EXPECT_EQ("abc\\\\", EscapeJsonChars("abc\\"));
  EXPECT_EQ("abc\\\"", EscapeJsonChars("abc\""));

  const std::string expected[0x20] = {
      "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005",
      "\\u0006", "\\u0007", "\\b", "\\t", "\\n", "\\v", "\\f", "\\r",
      "\\u000e", "\\u000f", "\\u0010", "\\u0011", "\\u0012", "\\u0013",
      "\\u0014", "\\u0015", "\\u0016", "\\u0017", "\\u0018", "\\u0019",
      "\\u001a", "\\u001b", "\\u001c", "\\u001d", "\\u001e", "\\u001f"
  };
  for (int i = 0; i < 0x20; ++i) {
    char symbols[1] = { static_cast<char>(i) };
    std::string input(symbols, 1);
    EXPECT_EQ(expected[i], EscapeJsonChars(input));
    EXPECT_EQ("a" + expected[i], EscapeJsonChars("a" + input));
  }
}
