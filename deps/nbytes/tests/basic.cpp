#include <format>
#include <nbytes.h>
#include <vector>

#include <gtest/gtest.h>

TEST(basic, swap_bytes16) {
  std::vector<char> input = {1, 2, 3, 4, 5, 6, 7, 8};
  nbytes::SwapBytes16(input.data(), input.size());
  std::vector<char> expected = {2, 1, 4, 3, 6, 5, 8, 7};
  EXPECT_EQ(input, expected);
  SUCCEED();
}

TEST(basic, swap_bytes32) {
  std::vector<char> input = {1, 2, 3, 4, 5, 6, 7, 8};
  nbytes::SwapBytes32(input.data(), input.size());
  std::vector<char> expected = {4, 3, 2, 1, 8, 7, 6, 5};
  EXPECT_EQ(input, expected);
  SUCCEED();
}

TEST(basic, swap_bytes64) {
  std::vector<char> input = {1, 2, 3, 4, 5, 6, 7, 8};
  nbytes::SwapBytes64(input.data(), input.size());
  std::vector<char> expected = {8, 7, 6, 5, 4, 3, 2, 1};
  EXPECT_EQ(input, expected);
  SUCCEED();
}

TEST(base64, decode_two_byte_ignores_non_latin1) {
  // U+4E2D has the low byte 0x2D ('-'); it must not decode as a base64 digit.
  const char16_t input[] = u"QUJD\u4E2DREVG";
  char out[16] = {};
  size_t n = nbytes::Base64Decode(out, sizeof(out), input, 9);
  EXPECT_EQ(std::string(out, n), "ABCDEF");
}

TEST(base64, decode_two_byte_does_not_stop_on_low_byte_padding) {
  // U+013D has the low byte 0x3D ('='); it must not end the decoding.
  const char16_t input[] = u"QUJD\u013DREVG";
  char out[16] = {};
  size_t n = nbytes::Base64Decode(out, sizeof(out), input, 9);
  EXPECT_EQ(std::string(out, n), "ABCDEF");
}

TEST(base64, decode_two_byte_all_non_latin1_is_empty) {
  // Four U+4E2D units form a full group in the fast path; the output must
  // still be empty rather than the bytes of "----".
  const char16_t input[] = u"\u4E2D\u4E2D\u4E2D\u4E2D";
  char out[16] = {};
  size_t n = nbytes::Base64Decode(out, sizeof(out), input, 4);
  EXPECT_EQ(n, 0u);
}

TEST(hex, decode_two_byte_rejects_non_latin1) {
  // U+0141 has the low byte 0x41 ('A'); it is not a hex digit.
  const char16_t input[] = u"\u0141\u0141";
  char out[4] = {};
  size_t n = nbytes::HexDecode(out, sizeof(out), input, 2);
  EXPECT_EQ(n, 0u);
}
