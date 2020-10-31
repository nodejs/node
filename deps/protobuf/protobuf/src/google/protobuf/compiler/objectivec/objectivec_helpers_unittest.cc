// Protocol Buffers - Google's data interchange format
// Copyright 2014 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <google/protobuf/compiler/objectivec/objectivec_helpers.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {
namespace {

TEST(ObjCHelper, TextFormatDecodeData_DecodeDataForString_RawStrings) {
  string input_for_decode("abcdefghIJ");
  string desired_output_for_decode;
  string expected;
  string result;

  // Different data, can't transform.

  desired_output_for_decode = "zbcdefghIJ";
  expected = string("\0zbcdefghIJ\0", 12);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  desired_output_for_decode = "abcdezghIJ";
  expected = string("\0abcdezghIJ\0", 12);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  // Shortened data, can't transform.

  desired_output_for_decode = "abcdefghI";
  expected = string("\0abcdefghI\0", 11);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  // Extra data, can't transform.

  desired_output_for_decode = "abcdefghIJz";
  expected = string("\0abcdefghIJz\0", 13);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);
}

TEST(ObjCHelper, TextFormatDecodeData_DecodeDataForString_ByteCodes) {
  string input_for_decode("abcdefghIJ");
  string desired_output_for_decode;
  string expected;
  string result;

  desired_output_for_decode = "abcdefghIJ";
  expected = string("\x0A\x0", 2);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  desired_output_for_decode = "_AbcdefghIJ";
  expected = string("\xCA\x0", 2);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  desired_output_for_decode = "ABCD__EfghI_j";
  expected = string("\x64\x80\xC5\xA1\x0", 5);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);

  // Long name so multiple decode ops are needed.

  input_for_decode =
      "longFieldNameIsLooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong1000";
  desired_output_for_decode =
      "long_field_name_is_looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_1000";
  expected = string("\x04\xA5\xA4\xA2\xBF\x1F\x0E\x84\x0", 9);
  result = TextFormatDecodeData::DecodeDataForString(input_for_decode,
                                                     desired_output_for_decode);
  EXPECT_EQ(expected, result);
}

// Death tests do not work on Windows as of yet.
#ifdef PROTOBUF_HAS_DEATH_TEST
TEST(ObjCHelperDeathTest, TextFormatDecodeData_DecodeDataForString_Failures) {
  // Empty inputs.

  EXPECT_EXIT(TextFormatDecodeData::DecodeDataForString("", ""),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");
  EXPECT_EXIT(TextFormatDecodeData::DecodeDataForString("a", ""),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");
  EXPECT_EXIT(TextFormatDecodeData::DecodeDataForString("", "a"),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");

  // Null char in the string.

  string str_with_null_char("ab\0c", 4);
  EXPECT_EXIT(
      TextFormatDecodeData::DecodeDataForString(str_with_null_char, "def"),
      ::testing::KilledBySignal(SIGABRT),
      "error: got a null char in a string for making TextFormat data, input:");
  EXPECT_EXIT(
      TextFormatDecodeData::DecodeDataForString("def", str_with_null_char),
      ::testing::KilledBySignal(SIGABRT),
      "error: got a null char in a string for making TextFormat data, input:");
}
#endif  // PROTOBUF_HAS_DEATH_TEST

TEST(ObjCHelper, TextFormatDecodeData_RawStrings) {
  TextFormatDecodeData decode_data;

  // Different data, can't transform.
  decode_data.AddString(1, "abcdefghIJ", "zbcdefghIJ");
  decode_data.AddString(3, "abcdefghIJ", "abcdezghIJ");
  // Shortened data, can't transform.
  decode_data.AddString(2, "abcdefghIJ", "abcdefghI");
  // Extra data, can't transform.
  decode_data.AddString(4, "abcdefghIJ", "abcdefghIJz");

  EXPECT_EQ(4, decode_data.num_entries());

  uint8 expected_data[] = {
      0x4,
      0x1, 0x0, 'z', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'I', 'J', 0x0,
      0x3, 0x0, 'a', 'b', 'c', 'd', 'e', 'z', 'g', 'h', 'I', 'J', 0x0,
      0x2, 0x0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'I', 0x0,
      0x4, 0x0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'I', 'J', 'z', 0x0,
  };
  string expected((const char*)expected_data, sizeof(expected_data));

  EXPECT_EQ(expected, decode_data.Data());
}

TEST(ObjCHelper, TextFormatDecodeData_ByteCodes) {
  TextFormatDecodeData decode_data;

  decode_data.AddString(1, "abcdefghIJ", "abcdefghIJ");
  decode_data.AddString(3, "abcdefghIJ", "_AbcdefghIJ");
  decode_data.AddString(2, "abcdefghIJ", "Abcd_EfghIJ");
  decode_data.AddString(4, "abcdefghIJ", "ABCD__EfghI_j");
  decode_data.AddString(1000,
                        "longFieldNameIsLooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong1000",
                        "long_field_name_is_looooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong_1000");

  EXPECT_EQ(5, decode_data.num_entries());

  uint8 expected_data[] = {
      0x5,
      // All as is (00 op)
      0x1,  0x0A, 0x0,
      // Underscore, upper + 9 (10 op)
      0x3,  0xCA, 0x0,
      //  Upper + 3 (10 op), underscore, upper + 5 (10 op)
      0x2,  0x44, 0xC6, 0x0,
      // All Upper for 4 (11 op), underscore, underscore, upper + 5 (10 op),
      // underscore, lower + 0 (01 op)
      0x4,  0x64, 0x80, 0xC5, 0xA1, 0x0,
      // 2 byte key: as is + 3 (00 op), underscore, lower + 4 (01 op),
      //   underscore, lower + 3 (01 op), underscore, lower + 1 (01 op),
      //   underscore, lower + 30 (01 op), as is + 30 (00 op), as is + 13 (00
      //   op),
      //   underscore, as is + 3 (00 op)
      0xE8, 0x07, 0x04, 0xA5, 0xA4, 0xA2, 0xBF, 0x1F, 0x0E, 0x84, 0x0,
  };
  string expected((const char*)expected_data, sizeof(expected_data));

  EXPECT_EQ(expected, decode_data.Data());
}


// Death tests do not work on Windows as of yet.
#ifdef PROTOBUF_HAS_DEATH_TEST
TEST(ObjCHelperDeathTest, TextFormatDecodeData_Failures) {
  TextFormatDecodeData decode_data;

  // Empty inputs.

  EXPECT_EXIT(decode_data.AddString(1, "", ""),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");
  EXPECT_EXIT(decode_data.AddString(1, "a", ""),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");
  EXPECT_EXIT(decode_data.AddString(1, "", "a"),
              ::testing::KilledBySignal(SIGABRT),
              "error: got empty string for making TextFormat data, input:");

  // Null char in the string.

  string str_with_null_char("ab\0c", 4);
  EXPECT_EXIT(
      decode_data.AddString(1, str_with_null_char, "def"),
      ::testing::KilledBySignal(SIGABRT),
      "error: got a null char in a string for making TextFormat data, input:");
  EXPECT_EXIT(
      decode_data.AddString(1, "def", str_with_null_char),
      ::testing::KilledBySignal(SIGABRT),
      "error: got a null char in a string for making TextFormat data, input:");

  // Duplicate keys

  decode_data.AddString(1, "abcdefghIJ", "abcdefghIJ");
  decode_data.AddString(3, "abcdefghIJ", "_AbcdefghIJ");
  decode_data.AddString(2, "abcdefghIJ", "Abcd_EfghIJ");
  EXPECT_EXIT(decode_data.AddString(2, "xyz", "x_yz"),
              ::testing::KilledBySignal(SIGABRT),
              "error: duplicate key \\(2\\) making TextFormat data, input:");
}
#endif  // PROTOBUF_HAS_DEATH_TEST

// TODO(thomasvl): Should probably add some unittests for all the special cases
// of name mangling (class name, field name, enum names).  Rather than doing
// this with an ObjC test in the objectivec directory, we should be able to
// use src/google/protobuf/compiler/importer* (like other tests) to support a
// virtual file system to feed in protos, once we have the Descriptor tree, the
// tests could use the helper methods for generating names and validate the
// right things are happening.

}  // namespace
}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
