// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
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

// Author: jschorr@google.com (Joseph Schorr)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/text_format.h>

#include <math.h>
#include <stdlib.h>
#include <limits>
#include <memory>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/map_unittest.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/test_util2.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/unittest_mset_wire_format.pb.h>
#include <google/protobuf/unittest_proto3.pb.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>


#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/mathlimits.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

// Can't use an anonymous namespace here due to brokenness of Tru64 compiler.
namespace text_format_unittest {

// A basic string with different escapable characters for testing.
const std::string kEscapeTestString =
    "\"A string with ' characters \n and \r newlines and \t tabs and \001 "
    "slashes \\ and  multiple   spaces";

// A representation of the above string with all the characters escaped.
const std::string kEscapeTestStringEscaped =
    "\"\\\"A string with \\' characters \\n and \\r newlines "
    "and \\t tabs and \\001 slashes \\\\ and  multiple   spaces\"";

class TextFormatTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    GOOGLE_CHECK_OK(File::GetContents(
        TestUtil::GetTestDataPath(
            "net/proto2/internal/"
            "testdata/text_format_unittest_data_oneof_implemented.txt"),
        &static_proto_debug_string_, true));
    CleanStringLineEndings(&static_proto_debug_string_, false);
  }

  TextFormatTest() : proto_debug_string_(static_proto_debug_string_) {}

 protected:
  // Debug string read from text_format_unittest_data.txt.
  const std::string proto_debug_string_;
  unittest::TestAllTypes proto_;

 private:
  static std::string static_proto_debug_string_;
};
std::string TextFormatTest::static_proto_debug_string_;

class TextFormatExtensionsTest : public testing::Test {
 public:
  static void SetUpTestSuite() {
    GOOGLE_CHECK_OK(File::GetContents(
        TestUtil::GetTestDataPath("net/proto2/internal/testdata/"
                                  "text_format_unittest_extensions_data.txt"),
        &static_proto_debug_string_, true));
    CleanStringLineEndings(&static_proto_debug_string_, false);
  }

  TextFormatExtensionsTest()
      : proto_debug_string_(static_proto_debug_string_) {}

 protected:
  // Debug string read from text_format_unittest_data.txt.
  const std::string proto_debug_string_;
  unittest::TestAllExtensions proto_;

 private:
  static std::string static_proto_debug_string_;
};
std::string TextFormatExtensionsTest::static_proto_debug_string_;

TEST_F(TextFormatTest, Basic) {
  TestUtil::SetAllFields(&proto_);
  EXPECT_EQ(proto_debug_string_, proto_.DebugString());
}

TEST_F(TextFormatExtensionsTest, Extensions) {
  TestUtil::SetAllExtensions(&proto_);
  EXPECT_EQ(proto_debug_string_, proto_.DebugString());
}

TEST_F(TextFormatTest, ShortDebugString) {
  proto_.set_optional_int32(1);
  proto_.set_optional_string("hello");
  proto_.mutable_optional_nested_message()->set_bb(2);
  proto_.mutable_optional_foreign_message();

  EXPECT_EQ(
      "optional_int32: 1 optional_string: \"hello\" "
      "optional_nested_message { bb: 2 } "
      "optional_foreign_message { }",
      proto_.ShortDebugString());
}

TEST_F(TextFormatTest, ShortPrimitiveRepeateds) {
  proto_.set_optional_int32(123);
  proto_.add_repeated_int32(456);
  proto_.add_repeated_int32(789);
  proto_.add_repeated_string("foo");
  proto_.add_repeated_string("bar");
  proto_.add_repeated_nested_message()->set_bb(2);
  proto_.add_repeated_nested_message()->set_bb(3);
  proto_.add_repeated_nested_enum(unittest::TestAllTypes::FOO);
  proto_.add_repeated_nested_enum(unittest::TestAllTypes::BAR);

  TextFormat::Printer printer;
  printer.SetUseShortRepeatedPrimitives(true);
  std::string text;
  EXPECT_TRUE(printer.PrintToString(proto_, &text));

  EXPECT_EQ(
      "optional_int32: 123\n"
      "repeated_int32: [456, 789]\n"
      "repeated_string: \"foo\"\n"
      "repeated_string: \"bar\"\n"
      "repeated_nested_message {\n  bb: 2\n}\n"
      "repeated_nested_message {\n  bb: 3\n}\n"
      "repeated_nested_enum: [FOO, BAR]\n",
      text);

  // Verify that any existing data in the string is cleared when PrintToString()
  // is called.
  text = "just some data here...\n\nblah blah";
  EXPECT_TRUE(printer.PrintToString(proto_, &text));

  EXPECT_EQ(
      "optional_int32: 123\n"
      "repeated_int32: [456, 789]\n"
      "repeated_string: \"foo\"\n"
      "repeated_string: \"bar\"\n"
      "repeated_nested_message {\n  bb: 2\n}\n"
      "repeated_nested_message {\n  bb: 3\n}\n"
      "repeated_nested_enum: [FOO, BAR]\n",
      text);

  // Try in single-line mode.
  printer.SetSingleLineMode(true);
  EXPECT_TRUE(printer.PrintToString(proto_, &text));

  EXPECT_EQ(
      "optional_int32: 123 "
      "repeated_int32: [456, 789] "
      "repeated_string: \"foo\" "
      "repeated_string: \"bar\" "
      "repeated_nested_message { bb: 2 } "
      "repeated_nested_message { bb: 3 } "
      "repeated_nested_enum: [FOO, BAR] ",
      text);
}


TEST_F(TextFormatTest, StringEscape) {
  // Set the string value to test.
  proto_.set_optional_string(kEscapeTestString);

  // Get the DebugString from the proto.
  std::string debug_string = proto_.DebugString();
  std::string utf8_debug_string = proto_.Utf8DebugString();

  // Hardcode a correct value to test against.
  std::string correct_string =
      "optional_string: " + kEscapeTestStringEscaped + "\n";

  // Compare.
  EXPECT_EQ(correct_string, debug_string);
  // UTF-8 string is the same as non-UTF-8 because
  // the protocol buffer contains no UTF-8 text.
  EXPECT_EQ(correct_string, utf8_debug_string);

  std::string expected_short_debug_string =
      "optional_string: " + kEscapeTestStringEscaped;
  EXPECT_EQ(expected_short_debug_string, proto_.ShortDebugString());
}

TEST_F(TextFormatTest, Utf8DebugString) {
  // Set the string value to test.
  proto_.set_optional_string("\350\260\267\346\255\214");
  proto_.set_optional_bytes("\350\260\267\346\255\214");

  // Get the DebugString from the proto.
  std::string debug_string = proto_.DebugString();
  std::string utf8_debug_string = proto_.Utf8DebugString();

  // Hardcode a correct value to test against.
  std::string correct_utf8_string =
      "optional_string: "
      "\"\350\260\267\346\255\214\""
      "\n"
      "optional_bytes: "
      "\"\\350\\260\\267\\346\\255\\214\""
      "\n";
  std::string correct_string =
      "optional_string: "
      "\"\\350\\260\\267\\346\\255\\214\""
      "\n"
      "optional_bytes: "
      "\"\\350\\260\\267\\346\\255\\214\""
      "\n";

  // Compare.
  EXPECT_EQ(correct_utf8_string, utf8_debug_string);
  EXPECT_EQ(correct_string, debug_string);
}

TEST_F(TextFormatTest, PrintUnknownFields) {
  // Test printing of unknown fields in a message.

  unittest::TestEmptyMessage message;
  UnknownFieldSet* unknown_fields = message.mutable_unknown_fields();

  unknown_fields->AddVarint(5, 1);
  unknown_fields->AddFixed32(5, 2);
  unknown_fields->AddFixed64(5, 3);
  unknown_fields->AddLengthDelimited(5, "4");
  unknown_fields->AddGroup(5)->AddVarint(10, 5);

  unknown_fields->AddVarint(8, 1);
  unknown_fields->AddVarint(8, 2);
  unknown_fields->AddVarint(8, 3);

  EXPECT_EQ(
      "5: 1\n"
      "5: 0x00000002\n"
      "5: 0x0000000000000003\n"
      "5: \"4\"\n"
      "5 {\n"
      "  10: 5\n"
      "}\n"
      "8: 1\n"
      "8: 2\n"
      "8: 3\n",
      message.DebugString());
}

TEST_F(TextFormatTest, PrintUnknownFieldsHidden) {
  // Test printing of unknown fields in a message when suppressed.

  unittest::OneString message;
  message.set_data("data");
  UnknownFieldSet* unknown_fields = message.mutable_unknown_fields();

  unknown_fields->AddVarint(5, 1);
  unknown_fields->AddFixed32(5, 2);
  unknown_fields->AddFixed64(5, 3);
  unknown_fields->AddLengthDelimited(5, "4");
  unknown_fields->AddGroup(5)->AddVarint(10, 5);

  unknown_fields->AddVarint(8, 1);
  unknown_fields->AddVarint(8, 2);
  unknown_fields->AddVarint(8, 3);

  TextFormat::Printer printer;
  printer.SetHideUnknownFields(true);
  std::string output;
  printer.PrintToString(message, &output);

  EXPECT_EQ("data: \"data\"\n", output);
}

TEST_F(TextFormatTest, PrintUnknownMessage) {
  // Test heuristic printing of messages in an UnknownFieldSet.

  protobuf_unittest::TestAllTypes message;

  // Cases which should not be interpreted as sub-messages.

  // 'a' is a valid FIXED64 tag, so for the string to be parseable as a message
  // it should be followed by 8 bytes.  Since this string only has two
  // subsequent bytes, it should be treated as a string.
  message.add_repeated_string("abc");

  // 'd' happens to be a valid ENDGROUP tag.  So,
  // UnknownFieldSet::MergeFromCodedStream() will successfully parse "def", but
  // the ConsumedEntireMessage() check should fail.
  message.add_repeated_string("def");

  // A zero-length string should never be interpreted as a message even though
  // it is technically valid as one.
  message.add_repeated_string("");

  // Case which should be interpreted as a sub-message.

  // An actual nested message with content should always be interpreted as a
  // nested message.
  message.add_repeated_nested_message()->set_bb(123);

  std::string data;
  message.SerializeToString(&data);

  std::string text;
  UnknownFieldSet unknown_fields;
  EXPECT_TRUE(unknown_fields.ParseFromString(data));
  EXPECT_TRUE(TextFormat::PrintUnknownFieldsToString(unknown_fields, &text));
  EXPECT_EQ(
      "44: \"abc\"\n"
      "44: \"def\"\n"
      "44: \"\"\n"
      "48 {\n"
      "  1: 123\n"
      "}\n",
      text);
}

TEST_F(TextFormatTest, PrintMessageWithIndent) {
  // Test adding an initial indent to printing.

  protobuf_unittest::TestAllTypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");
  message.add_repeated_nested_message()->set_bb(123);

  std::string text;
  TextFormat::Printer printer;
  printer.SetInitialIndentLevel(1);
  EXPECT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ(
      "  repeated_string: \"abc\"\n"
      "  repeated_string: \"def\"\n"
      "  repeated_nested_message {\n"
      "    bb: 123\n"
      "  }\n",
      text);
}

TEST_F(TextFormatTest, PrintMessageSingleLine) {
  // Test printing a message on a single line.

  protobuf_unittest::TestAllTypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");
  message.add_repeated_nested_message()->set_bb(123);

  std::string text;
  TextFormat::Printer printer;
  printer.SetInitialIndentLevel(1);
  printer.SetSingleLineMode(true);
  EXPECT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ(
      "  repeated_string: \"abc\" repeated_string: \"def\" "
      "repeated_nested_message { bb: 123 } ",
      text);
}

TEST_F(TextFormatTest, PrintBufferTooSmall) {
  // Test printing a message to a buffer that is too small.

  protobuf_unittest::TestAllTypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");

  char buffer[1] = "";
  io::ArrayOutputStream output_stream(buffer, 1);
  EXPECT_FALSE(TextFormat::Print(message, &output_stream));
  EXPECT_EQ(buffer[0], 'r');
  EXPECT_EQ(output_stream.ByteCount(), 1);
}

// A printer that appends 'u' to all unsigned int32.
class CustomUInt32FieldValuePrinter : public TextFormat::FieldValuePrinter {
 public:
  virtual std::string PrintUInt32(uint32 val) const {
    return StrCat(FieldValuePrinter::PrintUInt32(val), "u");
  }
};

TEST_F(TextFormatTest, DefaultCustomFieldPrinter) {
  protobuf_unittest::TestAllTypes message;

  message.set_optional_uint32(42);
  message.add_repeated_uint32(1);
  message.add_repeated_uint32(2);
  message.add_repeated_uint32(3);

  TextFormat::Printer printer;
  printer.SetDefaultFieldValuePrinter(new CustomUInt32FieldValuePrinter());
  // Let's see if that works well together with the repeated primitives:
  printer.SetUseShortRepeatedPrimitives(true);
  std::string text;
  printer.PrintToString(message, &text);
  EXPECT_EQ("optional_uint32: 42u\nrepeated_uint32: [1u, 2u, 3u]\n", text);
}

class CustomInt32FieldValuePrinter : public TextFormat::FieldValuePrinter {
 public:
  virtual std::string PrintInt32(int32 val) const {
    return StrCat("value-is(", FieldValuePrinter::PrintInt32(val), ")");
  }
};

TEST_F(TextFormatTest, FieldSpecificCustomPrinter) {
  protobuf_unittest::TestAllTypes message;

  message.set_optional_int32(42);  // This will be handled by our Printer.
  message.add_repeated_int32(42);  // This will be printed as number.

  TextFormat::Printer printer;
  EXPECT_TRUE(printer.RegisterFieldValuePrinter(
      message.GetDescriptor()->FindFieldByName("optional_int32"),
      new CustomInt32FieldValuePrinter()));
  std::string text;
  printer.PrintToString(message, &text);
  EXPECT_EQ("optional_int32: value-is(42)\nrepeated_int32: 42\n", text);
}

TEST_F(TextFormatTest, FieldSpecificCustomPrinterRegisterSameFieldTwice) {
  protobuf_unittest::TestAllTypes message;
  TextFormat::Printer printer;
  const FieldDescriptor* const field =
      message.GetDescriptor()->FindFieldByName("optional_int32");
  ASSERT_TRUE(printer.RegisterFieldValuePrinter(
      field, new CustomInt32FieldValuePrinter()));
  const TextFormat::FieldValuePrinter* const rejected =
      new CustomInt32FieldValuePrinter();
  ASSERT_FALSE(printer.RegisterFieldValuePrinter(field, rejected));
  delete rejected;
}

TEST_F(TextFormatTest, ErrorCasesRegisteringFieldValuePrinterShouldFail) {
  protobuf_unittest::TestAllTypes message;
  TextFormat::Printer printer;
  // NULL printer.
  EXPECT_FALSE(printer.RegisterFieldValuePrinter(
      message.GetDescriptor()->FindFieldByName("optional_int32"),
      static_cast<const TextFormat::FieldValuePrinter*>(nullptr)));
  EXPECT_FALSE(printer.RegisterFieldValuePrinter(
      message.GetDescriptor()->FindFieldByName("optional_int32"),
      static_cast<const TextFormat::FastFieldValuePrinter*>(nullptr)));
  // Because registration fails, the ownership of this printer is never taken.
  TextFormat::FieldValuePrinter my_field_printer;
  // NULL field
  EXPECT_FALSE(printer.RegisterFieldValuePrinter(nullptr, &my_field_printer));
}

class CustomMessageFieldValuePrinter : public TextFormat::FieldValuePrinter {
 public:
  virtual std::string PrintInt32(int32 v) const {
    return StrCat(FieldValuePrinter::PrintInt32(v), "  # x",
                        strings::Hex(v));
  }

  virtual std::string PrintMessageStart(const Message& message, int field_index,
                                        int field_count,
                                        bool single_line_mode) const {
    if (single_line_mode) {
      return " { ";
    }
    return StrCat(" {  # ", message.GetDescriptor()->name(), ": ",
                        field_index, "\n");
  }
};

TEST_F(TextFormatTest, CustomPrinterForComments) {
  protobuf_unittest::TestAllTypes message;
  message.mutable_optional_nested_message();
  message.mutable_optional_import_message()->set_d(42);
  message.add_repeated_nested_message();
  message.add_repeated_nested_message();
  message.add_repeated_import_message()->set_d(43);
  message.add_repeated_import_message()->set_d(44);
  TextFormat::Printer printer;
  CustomMessageFieldValuePrinter my_field_printer;
  printer.SetDefaultFieldValuePrinter(new CustomMessageFieldValuePrinter());
  std::string text;
  printer.PrintToString(message, &text);
  EXPECT_EQ(
      "optional_nested_message {  # NestedMessage: -1\n"
      "}\n"
      "optional_import_message {  # ImportMessage: -1\n"
      "  d: 42  # x2a\n"
      "}\n"
      "repeated_nested_message {  # NestedMessage: 0\n"
      "}\n"
      "repeated_nested_message {  # NestedMessage: 1\n"
      "}\n"
      "repeated_import_message {  # ImportMessage: 0\n"
      "  d: 43  # x2b\n"
      "}\n"
      "repeated_import_message {  # ImportMessage: 1\n"
      "  d: 44  # x2c\n"
      "}\n",
      text);
}

class CustomMultilineCommentPrinter : public TextFormat::FieldValuePrinter {
 public:
  virtual std::string PrintMessageStart(const Message& message, int field_index,
                                        int field_count,
                                        bool single_line_comment) const {
    return StrCat(" {  # 1\n", "  # 2\n");
  }
};

TEST_F(TextFormatTest, CustomPrinterForMultilineComments) {
  protobuf_unittest::TestAllTypes message;
  message.mutable_optional_nested_message();
  message.mutable_optional_import_message()->set_d(42);
  TextFormat::Printer printer;
  CustomMessageFieldValuePrinter my_field_printer;
  printer.SetDefaultFieldValuePrinter(new CustomMultilineCommentPrinter());
  std::string text;
  printer.PrintToString(message, &text);
  EXPECT_EQ(
      "optional_nested_message {  # 1\n"
      "  # 2\n"
      "}\n"
      "optional_import_message {  # 1\n"
      "  # 2\n"
      "  d: 42\n"
      "}\n",
      text);
}

// Achieve effects similar to SetUseShortRepeatedPrimitives for messages, using
// RegisterFieldValuePrinter. Use this to test the version of PrintFieldName
// that accepts repeated field index and count.
class CompactRepeatedFieldPrinter : public TextFormat::FastFieldValuePrinter {
 public:
  void PrintFieldName(const Message& message, int field_index, int field_count,
                      const Reflection* reflection,
                      const FieldDescriptor* field,
                      TextFormat::BaseTextGenerator* generator) const override {
    if (field_index == 0 || field_index == -1) {
      generator->PrintString(field->name());
    }
  }
  // To prevent compiler complaining about Woverloaded-virtual
  void PrintFieldName(const Message& message, const Reflection* reflection,
                      const FieldDescriptor* field,
                      TextFormat::BaseTextGenerator* generator) const override {
  }
  void PrintMessageStart(
      const Message& message, int field_index, int field_count,
      bool single_line_mode,
      TextFormat::BaseTextGenerator* generator) const override {
    if (field_index == 0 || field_index == -1) {
      if (single_line_mode) {
        generator->PrintLiteral(" { ");
      } else {
        generator->PrintLiteral(" {\n");
      }
    }
  }
  void PrintMessageEnd(
      const Message& message, int field_index, int field_count,
      bool single_line_mode,
      TextFormat::BaseTextGenerator* generator) const override {
    if (field_index == field_count - 1 || field_index == -1) {
      if (single_line_mode) {
        generator->PrintLiteral("} ");
      } else {
        generator->PrintLiteral("}\n");
      }
    }
  }
};

TEST_F(TextFormatTest, CompactRepeatedFieldPrinter) {
  TextFormat::Printer printer;
  ASSERT_TRUE(printer.RegisterFieldValuePrinter(
      unittest::TestAllTypes::default_instance()
          .descriptor()
          ->FindFieldByNumber(
              unittest::TestAllTypes::kRepeatedNestedMessageFieldNumber),
      new CompactRepeatedFieldPrinter));

  protobuf_unittest::TestAllTypes message;
  message.add_repeated_nested_message()->set_bb(1);
  message.add_repeated_nested_message()->set_bb(2);
  message.add_repeated_nested_message()->set_bb(3);

  std::string text;
  ASSERT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ(
      "repeated_nested_message {\n"
      "  bb: 1\n"
      "  bb: 2\n"
      "  bb: 3\n"
      "}\n",
      text);
}

// Print strings into multiple line, with indention. Use this to test
// BaseTextGenerator::Indent and BaseTextGenerator::Outdent.
class MultilineStringPrinter : public TextFormat::FastFieldValuePrinter {
 public:
  void PrintString(const std::string& val,
                   TextFormat::BaseTextGenerator* generator) const override {
    generator->Indent();
    int last_pos = 0;
    int newline_pos = val.find('\n');
    while (newline_pos != std::string::npos) {
      generator->PrintLiteral("\n");
      TextFormat::FastFieldValuePrinter::PrintString(
          val.substr(last_pos, newline_pos + 1 - last_pos), generator);
      last_pos = newline_pos + 1;
      newline_pos = val.find('\n', last_pos);
    }
    if (last_pos < val.size()) {
      generator->PrintLiteral("\n");
      TextFormat::FastFieldValuePrinter::PrintString(val.substr(last_pos),
                                                     generator);
    }
    generator->Outdent();
  }
};

TEST_F(TextFormatTest, MultilineStringPrinter) {
  TextFormat::Printer printer;
  ASSERT_TRUE(printer.RegisterFieldValuePrinter(
      unittest::TestAllTypes::default_instance()
          .descriptor()
          ->FindFieldByNumber(
              unittest::TestAllTypes::kOptionalStringFieldNumber),
      new MultilineStringPrinter));

  protobuf_unittest::TestAllTypes message;
  message.set_optional_string("first line\nsecond line\nthird line");

  std::string text;
  ASSERT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ(
      "optional_string: \n"
      "  \"first line\\n\"\n"
      "  \"second line\\n\"\n"
      "  \"third line\"\n",
      text);
}

class CustomNestedMessagePrinter : public TextFormat::MessagePrinter {
 public:
  CustomNestedMessagePrinter() {}
  ~CustomNestedMessagePrinter() override {}
  void Print(const Message& message, bool single_line_mode,
             TextFormat::BaseTextGenerator* generator) const override {
    generator->PrintLiteral("custom");
  }
};

TEST_F(TextFormatTest, CustomMessagePrinter) {
  TextFormat::Printer printer;
  printer.RegisterMessagePrinter(
      unittest::TestAllTypes::NestedMessage::default_instance().descriptor(),
      new CustomNestedMessagePrinter);

  unittest::TestAllTypes message;
  std::string text;
  EXPECT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ("", text);

  message.mutable_optional_nested_message()->set_bb(1);
  EXPECT_TRUE(printer.PrintToString(message, &text));
  EXPECT_EQ("optional_nested_message {\n  custom}\n", text);
}

TEST_F(TextFormatTest, ParseBasic) {
  io::ArrayInputStream input_stream(proto_debug_string_.data(),
                                    proto_debug_string_.size());
  TextFormat::Parse(&input_stream, &proto_);
  TestUtil::ExpectAllFieldsSet(proto_);
}

TEST_F(TextFormatExtensionsTest, ParseExtensions) {
  io::ArrayInputStream input_stream(proto_debug_string_.data(),
                                    proto_debug_string_.size());
  TextFormat::Parse(&input_stream, &proto_);
  TestUtil::ExpectAllExtensionsSet(proto_);
}

TEST_F(TextFormatTest, ParseEnumFieldFromNumber) {
  // Create a parse string with a numerical value for an enum field.
  std::string parse_string = strings::Substitute("optional_nested_enum: $0",
                                                 unittest::TestAllTypes::BAZ);
  EXPECT_TRUE(TextFormat::ParseFromString(parse_string, &proto_));
  EXPECT_TRUE(proto_.has_optional_nested_enum());
  EXPECT_EQ(unittest::TestAllTypes::BAZ, proto_.optional_nested_enum());
}

TEST_F(TextFormatTest, ParseEnumFieldFromNegativeNumber) {
  ASSERT_LT(unittest::SPARSE_E, 0);
  std::string parse_string =
      strings::Substitute("sparse_enum: $0", unittest::SPARSE_E);
  unittest::SparseEnumMessage proto;
  EXPECT_TRUE(TextFormat::ParseFromString(parse_string, &proto));
  EXPECT_TRUE(proto.has_sparse_enum());
  EXPECT_EQ(unittest::SPARSE_E, proto.sparse_enum());
}

TEST_F(TextFormatTest, PrintUnknownEnumFieldProto3) {
  proto3_unittest::TestAllTypes proto;

  proto.add_repeated_nested_enum(
      static_cast<proto3_unittest::TestAllTypes::NestedEnum>(10));
  proto.add_repeated_nested_enum(
      static_cast<proto3_unittest::TestAllTypes::NestedEnum>(-10));
  proto.add_repeated_nested_enum(
      static_cast<proto3_unittest::TestAllTypes::NestedEnum>(2147483647));
  proto.add_repeated_nested_enum(
      static_cast<proto3_unittest::TestAllTypes::NestedEnum>(-2147483648));

  EXPECT_EQ(
      "repeated_nested_enum: 10\n"
      "repeated_nested_enum: -10\n"
      "repeated_nested_enum: 2147483647\n"
      "repeated_nested_enum: -2147483648\n",
      proto.DebugString());
}

TEST_F(TextFormatTest, ParseUnknownEnumFieldProto3) {
  proto3_unittest::TestAllTypes proto;
  std::string parse_string =
      "repeated_nested_enum: [10, -10, 2147483647, -2147483648]";
  EXPECT_TRUE(TextFormat::ParseFromString(parse_string, &proto));
  ASSERT_EQ(4, proto.repeated_nested_enum_size());
  EXPECT_EQ(10, proto.repeated_nested_enum(0));
  EXPECT_EQ(-10, proto.repeated_nested_enum(1));
  EXPECT_EQ(2147483647, proto.repeated_nested_enum(2));
  EXPECT_EQ(-2147483648, proto.repeated_nested_enum(3));
}

TEST_F(TextFormatTest, ParseStringEscape) {
  // Create a parse string with escpaed characters in it.
  std::string parse_string =
      "optional_string: " + kEscapeTestStringEscaped + "\n";

  io::ArrayInputStream input_stream(parse_string.data(), parse_string.size());
  TextFormat::Parse(&input_stream, &proto_);

  // Compare.
  EXPECT_EQ(kEscapeTestString, proto_.optional_string());
}

TEST_F(TextFormatTest, ParseConcatenatedString) {
  // Create a parse string with multiple parts on one line.
  std::string parse_string = "optional_string: \"foo\" \"bar\"\n";

  io::ArrayInputStream input_stream1(parse_string.data(), parse_string.size());
  TextFormat::Parse(&input_stream1, &proto_);

  // Compare.
  EXPECT_EQ("foobar", proto_.optional_string());

  // Create a parse string with multiple parts on separate lines.
  parse_string =
      "optional_string: \"foo\"\n"
      "\"bar\"\n";

  io::ArrayInputStream input_stream2(parse_string.data(), parse_string.size());
  TextFormat::Parse(&input_stream2, &proto_);

  // Compare.
  EXPECT_EQ("foobar", proto_.optional_string());
}

TEST_F(TextFormatTest, ParseFloatWithSuffix) {
  // Test that we can parse a floating-point value with 'f' appended to the
  // end.  This is needed for backwards-compatibility with proto1.

  // Have it parse a float with the 'f' suffix.
  std::string parse_string = "optional_float: 1.0f\n";

  io::ArrayInputStream input_stream(parse_string.data(), parse_string.size());

  TextFormat::Parse(&input_stream, &proto_);

  // Compare.
  EXPECT_EQ(1.0, proto_.optional_float());
}

TEST_F(TextFormatTest, ParseShortRepeatedForm) {
  std::string parse_string =
      // Mixed short-form and long-form are simply concatenated.
      "repeated_int32: 1\n"
      "repeated_int32: [456, 789]\n"
      "repeated_nested_enum: [  FOO ,BAR, # comment\n"
      "                         3]\n"
      // Note that while the printer won't print repeated strings in short-form,
      // the parser will accept them.
      "repeated_string: [ \"foo\", 'bar' ]\n"
      // Repeated message
      "repeated_nested_message: [ { bb: 1 }, { bb : 2 }]\n"
      // Repeated group
      "RepeatedGroup [{ a: 3 },{ a: 4 }]\n";

  ASSERT_TRUE(TextFormat::ParseFromString(parse_string, &proto_));

  ASSERT_EQ(3, proto_.repeated_int32_size());
  EXPECT_EQ(1, proto_.repeated_int32(0));
  EXPECT_EQ(456, proto_.repeated_int32(1));
  EXPECT_EQ(789, proto_.repeated_int32(2));

  ASSERT_EQ(3, proto_.repeated_nested_enum_size());
  EXPECT_EQ(unittest::TestAllTypes::FOO, proto_.repeated_nested_enum(0));
  EXPECT_EQ(unittest::TestAllTypes::BAR, proto_.repeated_nested_enum(1));
  EXPECT_EQ(unittest::TestAllTypes::BAZ, proto_.repeated_nested_enum(2));

  ASSERT_EQ(2, proto_.repeated_string_size());
  EXPECT_EQ("foo", proto_.repeated_string(0));
  EXPECT_EQ("bar", proto_.repeated_string(1));

  ASSERT_EQ(2, proto_.repeated_nested_message_size());
  EXPECT_EQ(1, proto_.repeated_nested_message(0).bb());
  EXPECT_EQ(2, proto_.repeated_nested_message(1).bb());

  ASSERT_EQ(2, proto_.repeatedgroup_size());
  EXPECT_EQ(3, proto_.repeatedgroup(0).a());
  EXPECT_EQ(4, proto_.repeatedgroup(1).a());
}

TEST_F(TextFormatTest, ParseShortRepeatedWithTrailingComma) {
  std::string parse_string = "repeated_int32: [456,]\n";
  ASSERT_FALSE(TextFormat::ParseFromString(parse_string, &proto_));
  parse_string = "repeated_nested_enum: [  FOO , ]";
  ASSERT_FALSE(TextFormat::ParseFromString(parse_string, &proto_));
  parse_string = "repeated_string: [ \"foo\", ]";
  ASSERT_FALSE(TextFormat::ParseFromString(parse_string, &proto_));
  parse_string = "repeated_nested_message: [ { bb: 1 }, ]";
  ASSERT_FALSE(TextFormat::ParseFromString(parse_string, &proto_));
  parse_string = "RepeatedGroup [{ a: 3 },]\n";
}

TEST_F(TextFormatTest, ParseShortRepeatedEmpty) {
  std::string parse_string =
      "repeated_int32: []\n"
      "repeated_nested_enum: []\n"
      "repeated_string: []\n"
      "repeated_nested_message: []\n"
      "RepeatedGroup []\n";

  ASSERT_TRUE(TextFormat::ParseFromString(parse_string, &proto_));

  EXPECT_EQ(0, proto_.repeated_int32_size());
  EXPECT_EQ(0, proto_.repeated_nested_enum_size());
  EXPECT_EQ(0, proto_.repeated_string_size());
  EXPECT_EQ(0, proto_.repeated_nested_message_size());
  EXPECT_EQ(0, proto_.repeatedgroup_size());
}

TEST_F(TextFormatTest, ParseShortRepeatedConcatenatedWithEmpty) {
  std::string parse_string =
      // Starting with empty [] should have no impact.
      "repeated_int32: []\n"
      "repeated_nested_enum: []\n"
      "repeated_string: []\n"
      "repeated_nested_message: []\n"
      "RepeatedGroup []\n"
      // Mixed short-form and long-form are simply concatenated.
      "repeated_int32: 1\n"
      "repeated_int32: [456, 789]\n"
      "repeated_nested_enum: [  FOO ,BAR, # comment\n"
      "                         3]\n"
      // Note that while the printer won't print repeated strings in short-form,
      // the parser will accept them.
      "repeated_string: [ \"foo\", 'bar' ]\n"
      // Repeated message
      "repeated_nested_message: [ { bb: 1 }, { bb : 2 }]\n"
      // Repeated group
      "RepeatedGroup [{ a: 3 },{ a: 4 }]\n"
      // Adding empty [] should have no impact.
      "repeated_int32: []\n"
      "repeated_nested_enum: []\n"
      "repeated_string: []\n"
      "repeated_nested_message: []\n"
      "RepeatedGroup []\n";

  ASSERT_TRUE(TextFormat::ParseFromString(parse_string, &proto_));

  ASSERT_EQ(3, proto_.repeated_int32_size());
  EXPECT_EQ(1, proto_.repeated_int32(0));
  EXPECT_EQ(456, proto_.repeated_int32(1));
  EXPECT_EQ(789, proto_.repeated_int32(2));

  ASSERT_EQ(3, proto_.repeated_nested_enum_size());
  EXPECT_EQ(unittest::TestAllTypes::FOO, proto_.repeated_nested_enum(0));
  EXPECT_EQ(unittest::TestAllTypes::BAR, proto_.repeated_nested_enum(1));
  EXPECT_EQ(unittest::TestAllTypes::BAZ, proto_.repeated_nested_enum(2));

  ASSERT_EQ(2, proto_.repeated_string_size());
  EXPECT_EQ("foo", proto_.repeated_string(0));
  EXPECT_EQ("bar", proto_.repeated_string(1));

  ASSERT_EQ(2, proto_.repeated_nested_message_size());
  EXPECT_EQ(1, proto_.repeated_nested_message(0).bb());
  EXPECT_EQ(2, proto_.repeated_nested_message(1).bb());

  ASSERT_EQ(2, proto_.repeatedgroup_size());
  EXPECT_EQ(3, proto_.repeatedgroup(0).a());
  EXPECT_EQ(4, proto_.repeatedgroup(1).a());
}


TEST_F(TextFormatTest, Comments) {
  // Test that comments are ignored.

  std::string parse_string =
      "optional_int32: 1  # a comment\n"
      "optional_int64: 2  # another comment";

  io::ArrayInputStream input_stream(parse_string.data(), parse_string.size());

  TextFormat::Parse(&input_stream, &proto_);

  // Compare.
  EXPECT_EQ(1, proto_.optional_int32());
  EXPECT_EQ(2, proto_.optional_int64());
}

TEST_F(TextFormatTest, OptionalColon) {
  // Test that we can place a ':' after the field name of a nested message,
  // even though we don't have to.

  std::string parse_string = "optional_nested_message: { bb: 1}\n";

  io::ArrayInputStream input_stream(parse_string.data(), parse_string.size());

  TextFormat::Parse(&input_stream, &proto_);

  // Compare.
  EXPECT_TRUE(proto_.has_optional_nested_message());
  EXPECT_EQ(1, proto_.optional_nested_message().bb());
}

// Some platforms (e.g. Windows) insist on padding the exponent to three
// digits when one or two would be just fine.
static std::string RemoveRedundantZeros(std::string text) {
  text = StringReplace(text, "e+0", "e+", true);
  text = StringReplace(text, "e-0", "e-", true);
  return text;
}

TEST_F(TextFormatTest, PrintExotic) {
  unittest::TestAllTypes message;

  // Note:  In C, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and
  //   9223372036854775808 is outside the range of int64.  However, it is not
  //   outside the range of uint64.  Confusingly, this means that everything
  //   works if we make the literal unsigned, even though we are negating it.
  message.add_repeated_int64(-PROTOBUF_ULONGLONG(9223372036854775808));
  message.add_repeated_uint64(PROTOBUF_ULONGLONG(18446744073709551615));
  message.add_repeated_double(123.456);
  message.add_repeated_double(1.23e21);
  message.add_repeated_double(1.23e-18);
  message.add_repeated_double(std::numeric_limits<double>::infinity());
  message.add_repeated_double(-std::numeric_limits<double>::infinity());
  message.add_repeated_double(std::numeric_limits<double>::quiet_NaN());
  message.add_repeated_string(std::string("\000\001\a\b\f\n\r\t\v\\\'\"", 12));

  // Fun story:  We used to use 1.23e22 instead of 1.23e21 above, but this
  //   seemed to trigger an odd case on MinGW/GCC 3.4.5 where GCC's parsing of
  //   the value differed from strtod()'s parsing.  That is to say, the
  //   following assertion fails on MinGW:
  //     assert(1.23e22 == strtod("1.23e22", NULL));
  //   As a result, SimpleDtoa() would print the value as
  //   "1.2300000000000001e+22" to make sure strtod() produce the exact same
  //   result.  Our goal is to test runtime parsing, not compile-time parsing,
  //   so this wasn't our problem.  It was found that using 1.23e21 did not
  //   have this problem, so we switched to that instead.

  EXPECT_EQ(
      "repeated_int64: -9223372036854775808\n"
      "repeated_uint64: 18446744073709551615\n"
      "repeated_double: 123.456\n"
      "repeated_double: 1.23e+21\n"
      "repeated_double: 1.23e-18\n"
      "repeated_double: inf\n"
      "repeated_double: -inf\n"
      "repeated_double: nan\n"
      "repeated_string: "
      "\"\\000\\001\\007\\010\\014\\n\\r\\t\\013\\\\\\'\\\"\"\n",
      RemoveRedundantZeros(message.DebugString()));
}

TEST_F(TextFormatTest, PrintFloatPrecision) {
  unittest::TestAllTypes message;

  message.add_repeated_float(1.0);
  message.add_repeated_float(1.2);
  message.add_repeated_float(1.23);
  message.add_repeated_float(1.234);
  message.add_repeated_float(1.2345);
  message.add_repeated_float(1.23456);
  message.add_repeated_float(1.2e10);
  message.add_repeated_float(1.23e10);
  message.add_repeated_float(1.234e10);
  message.add_repeated_float(1.2345e10);
  message.add_repeated_float(1.23456e10);
  message.add_repeated_double(1.2);
  message.add_repeated_double(1.23);
  message.add_repeated_double(1.234);
  message.add_repeated_double(1.2345);
  message.add_repeated_double(1.23456);
  message.add_repeated_double(1.234567);
  message.add_repeated_double(1.2345678);
  message.add_repeated_double(1.23456789);
  message.add_repeated_double(1.234567898);
  message.add_repeated_double(1.2345678987);
  message.add_repeated_double(1.23456789876);
  message.add_repeated_double(1.234567898765);
  message.add_repeated_double(1.2345678987654);
  message.add_repeated_double(1.23456789876543);
  message.add_repeated_double(1.2e100);
  message.add_repeated_double(1.23e100);
  message.add_repeated_double(1.234e100);
  message.add_repeated_double(1.2345e100);
  message.add_repeated_double(1.23456e100);
  message.add_repeated_double(1.234567e100);
  message.add_repeated_double(1.2345678e100);
  message.add_repeated_double(1.23456789e100);
  message.add_repeated_double(1.234567898e100);
  message.add_repeated_double(1.2345678987e100);
  message.add_repeated_double(1.23456789876e100);
  message.add_repeated_double(1.234567898765e100);
  message.add_repeated_double(1.2345678987654e100);
  message.add_repeated_double(1.23456789876543e100);

  EXPECT_EQ(
      "repeated_float: 1\n"
      "repeated_float: 1.2\n"
      "repeated_float: 1.23\n"
      "repeated_float: 1.234\n"
      "repeated_float: 1.2345\n"
      "repeated_float: 1.23456\n"
      "repeated_float: 1.2e+10\n"
      "repeated_float: 1.23e+10\n"
      "repeated_float: 1.234e+10\n"
      "repeated_float: 1.2345e+10\n"
      "repeated_float: 1.23456e+10\n"
      "repeated_double: 1.2\n"
      "repeated_double: 1.23\n"
      "repeated_double: 1.234\n"
      "repeated_double: 1.2345\n"
      "repeated_double: 1.23456\n"
      "repeated_double: 1.234567\n"
      "repeated_double: 1.2345678\n"
      "repeated_double: 1.23456789\n"
      "repeated_double: 1.234567898\n"
      "repeated_double: 1.2345678987\n"
      "repeated_double: 1.23456789876\n"
      "repeated_double: 1.234567898765\n"
      "repeated_double: 1.2345678987654\n"
      "repeated_double: 1.23456789876543\n"
      "repeated_double: 1.2e+100\n"
      "repeated_double: 1.23e+100\n"
      "repeated_double: 1.234e+100\n"
      "repeated_double: 1.2345e+100\n"
      "repeated_double: 1.23456e+100\n"
      "repeated_double: 1.234567e+100\n"
      "repeated_double: 1.2345678e+100\n"
      "repeated_double: 1.23456789e+100\n"
      "repeated_double: 1.234567898e+100\n"
      "repeated_double: 1.2345678987e+100\n"
      "repeated_double: 1.23456789876e+100\n"
      "repeated_double: 1.234567898765e+100\n"
      "repeated_double: 1.2345678987654e+100\n"
      "repeated_double: 1.23456789876543e+100\n",
      RemoveRedundantZeros(message.DebugString()));
}

TEST_F(TextFormatTest, AllowPartial) {
  unittest::TestRequired message;
  TextFormat::Parser parser;
  parser.AllowPartialMessage(true);
  EXPECT_TRUE(parser.ParseFromString("a: 1", &message));
  EXPECT_EQ(1, message.a());
  EXPECT_FALSE(message.has_b());
  EXPECT_FALSE(message.has_c());
}

TEST_F(TextFormatTest, ParseExotic) {
  unittest::TestAllTypes message;
  ASSERT_TRUE(TextFormat::ParseFromString(
      "repeated_int32: -1\n"
      "repeated_int32: -2147483648\n"
      "repeated_int64: -1\n"
      "repeated_int64: -9223372036854775808\n"
      "repeated_uint32: 4294967295\n"
      "repeated_uint32: 2147483648\n"
      "repeated_uint64: 18446744073709551615\n"
      "repeated_uint64: 9223372036854775808\n"
      "repeated_double: 123.0\n"
      "repeated_double: 123.5\n"
      "repeated_double: 0.125\n"
      "repeated_double: 1.23E17\n"
      "repeated_double: 1.235E+22\n"
      "repeated_double: 1.235e-18\n"
      "repeated_double: 123.456789\n"
      "repeated_double: inf\n"
      "repeated_double: Infinity\n"
      "repeated_double: -inf\n"
      "repeated_double: -Infinity\n"
      "repeated_double: nan\n"
      "repeated_double: NaN\n"
      "repeated_string: \"\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\"\n",
      &message));

  ASSERT_EQ(2, message.repeated_int32_size());
  EXPECT_EQ(-1, message.repeated_int32(0));
  // Note:  In C, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and 2147483648 is
  //   outside the range of int32.  However, it is not outside the range of
  //   uint32.  Confusingly, this means that everything works if we make the
  //   literal unsigned, even though we are negating it.
  EXPECT_EQ(-2147483648u, message.repeated_int32(1));

  ASSERT_EQ(2, message.repeated_int64_size());
  EXPECT_EQ(-1, message.repeated_int64(0));
  // Note:  In C, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and
  //   9223372036854775808 is outside the range of int64.  However, it is not
  //   outside the range of uint64.  Confusingly, this means that everything
  //   works if we make the literal unsigned, even though we are negating it.
  EXPECT_EQ(-PROTOBUF_ULONGLONG(9223372036854775808),
            message.repeated_int64(1));

  ASSERT_EQ(2, message.repeated_uint32_size());
  EXPECT_EQ(4294967295u, message.repeated_uint32(0));
  EXPECT_EQ(2147483648u, message.repeated_uint32(1));

  ASSERT_EQ(2, message.repeated_uint64_size());
  EXPECT_EQ(PROTOBUF_ULONGLONG(18446744073709551615),
            message.repeated_uint64(0));
  EXPECT_EQ(PROTOBUF_ULONGLONG(9223372036854775808),
            message.repeated_uint64(1));

  ASSERT_EQ(13, message.repeated_double_size());
  EXPECT_EQ(123.0, message.repeated_double(0));
  EXPECT_EQ(123.5, message.repeated_double(1));
  EXPECT_EQ(0.125, message.repeated_double(2));
  EXPECT_EQ(1.23E17, message.repeated_double(3));
  EXPECT_EQ(1.235E22, message.repeated_double(4));
  EXPECT_EQ(1.235E-18, message.repeated_double(5));
  EXPECT_EQ(123.456789, message.repeated_double(6));
  EXPECT_EQ(message.repeated_double(7),
            std::numeric_limits<double>::infinity());
  EXPECT_EQ(message.repeated_double(8),
            std::numeric_limits<double>::infinity());
  EXPECT_EQ(message.repeated_double(9),
            -std::numeric_limits<double>::infinity());
  EXPECT_EQ(message.repeated_double(10),
            -std::numeric_limits<double>::infinity());
  EXPECT_TRUE(MathLimits<double>::IsNaN(message.repeated_double(11)));
  EXPECT_TRUE(MathLimits<double>::IsNaN(message.repeated_double(12)));

  // Note:  Since these string literals have \0's in them, we must explicitly
  // pass their sizes to string's constructor.
  ASSERT_EQ(1, message.repeated_string_size());
  EXPECT_EQ(std::string("\000\001\a\b\f\n\r\t\v\\\'\"", 12),
            message.repeated_string(0));

  ASSERT_TRUE(
      TextFormat::ParseFromString("repeated_float: 3.4028235e+38\n"
                                  "repeated_float: -3.4028235e+38\n"
                                  "repeated_float: 3.402823567797337e+38\n"
                                  "repeated_float: -3.402823567797337e+38\n",
                                  &message));
  EXPECT_EQ(message.repeated_float(0), std::numeric_limits<float>::max());
  EXPECT_EQ(message.repeated_float(1), -std::numeric_limits<float>::max());
  EXPECT_EQ(message.repeated_float(2), std::numeric_limits<float>::infinity());
  EXPECT_EQ(message.repeated_float(3), -std::numeric_limits<float>::infinity());

}

TEST_F(TextFormatTest, PrintFieldsInIndexOrder) {
  protobuf_unittest::TestFieldOrderings message;
  // Fields are listed in index order instead of field number.
  message.set_my_string("str");  // Field number 11
  message.set_my_int(12345);     // Field number 1
  message.set_my_float(0.999);   // Field number 101
  // Extensions are listed based on the order of extension number.
  // Extension number 12.
  message
      .MutableExtension(
          protobuf_unittest::TestExtensionOrderings2::test_ext_orderings2)
      ->set_my_string("ext_str2");
  // Extension number 13.
  message
      .MutableExtension(
          protobuf_unittest::TestExtensionOrderings1::test_ext_orderings1)
      ->set_my_string("ext_str1");
  // Extension number 14.
  message
      .MutableExtension(protobuf_unittest::TestExtensionOrderings2::
                            TestExtensionOrderings3::test_ext_orderings3)
      ->set_my_string("ext_str3");
  // Extension number 50.
  *message.MutableExtension(protobuf_unittest::my_extension_string) = "ext_str0";

  TextFormat::Printer printer;
  std::string text;

  // By default, print in field number order.
  // my_int: 12345
  // my_string: "str"
  // [protobuf_unittest.TestExtensionOrderings2.test_ext_orderings2] {
  //   my_string: "ext_str2"
  // }
  // [protobuf_unittest.TestExtensionOrderings1.test_ext_orderings1] {
  //   my_string: "ext_str1"
  // }
  // [protobuf_unittest.TestExtensionOrderings2.TestExtensionOrderings3.test_ext_orderings3]
  // {
  //   my_string: "ext_str3"
  // }
  // [protobuf_unittest.my_extension_string]: "ext_str0"
  // my_float: 0.999
  printer.PrintToString(message, &text);
  EXPECT_EQ(
      "my_int: 12345\nmy_string: "
      "\"str\"\n[protobuf_unittest.TestExtensionOrderings2.test_ext_orderings2] "
      "{\n  my_string: "
      "\"ext_str2\"\n}\n[protobuf_unittest.TestExtensionOrderings1.test_ext_"
      "orderings1] {\n  my_string: "
      "\"ext_str1\"\n}\n[protobuf_unittest.TestExtensionOrderings2."
      "TestExtensionOrderings3.test_ext_orderings3] {\n  my_string: "
      "\"ext_str3\"\n}\n[protobuf_unittest.my_extension_string]: "
      "\"ext_str0\"\nmy_float: 0.999\n",
      text);

  // Print in index order.
  // my_string: "str"
  // my_int: 12345
  // my_float: 0.999
  // [protobuf_unittest.TestExtensionOrderings2.test_ext_orderings2] {
  //   my_string: "ext_str2"
  // }
  // [protobuf_unittest.TestExtensionOrderings1.test_ext_orderings1] {
  //   my_string: "ext_str1"
  // }
  // [protobuf_unittest.TestExtensionOrderings2.TestExtensionOrderings3.test_ext_orderings3]
  // {
  //   my_string: "ext_str3"
  // }
  // [protobuf_unittest.my_extension_string]: "ext_str0"
  printer.SetPrintMessageFieldsInIndexOrder(true);
  printer.PrintToString(message, &text);
  EXPECT_EQ(
      "my_string: \"str\"\nmy_int: 12345\nmy_float: "
      "0.999\n[protobuf_unittest.TestExtensionOrderings2.test_ext_orderings2] "
      "{\n  my_string: "
      "\"ext_str2\"\n}\n[protobuf_unittest.TestExtensionOrderings1.test_ext_"
      "orderings1] {\n  my_string: "
      "\"ext_str1\"\n}\n[protobuf_unittest.TestExtensionOrderings2."
      "TestExtensionOrderings3.test_ext_orderings3] {\n  my_string: "
      "\"ext_str3\"\n}\n[protobuf_unittest.my_extension_string]: \"ext_str0\"\n",
      text);
}

class TextFormatParserTest : public testing::Test {
 protected:
  void ExpectFailure(const std::string& input, const std::string& message,
                     int line, int col) {
    std::unique_ptr<unittest::TestAllTypes> proto(new unittest::TestAllTypes);
    ExpectFailure(input, message, line, col, proto.get());
  }

  void ExpectFailure(const std::string& input, const std::string& message,
                     int line, int col, Message* proto) {
    ExpectMessage(input, message, line, col, proto, false);
  }

  void ExpectMessage(const std::string& input, const std::string& message,
                     int line, int col, Message* proto, bool expected_result) {
    MockErrorCollector error_collector;
    parser_.RecordErrorsTo(&error_collector);
    EXPECT_EQ(expected_result, parser_.ParseFromString(input, proto))
        << input << " -> " << proto->DebugString();
    EXPECT_EQ(
        StrCat(line) + ":" + StrCat(col) + ": " + message + "\n",
        error_collector.text_);
    parser_.RecordErrorsTo(nullptr);
  }

  void ExpectSuccessAndTree(const std::string& input, Message* proto,
                            TextFormat::ParseInfoTree* info_tree) {
    MockErrorCollector error_collector;
    parser_.RecordErrorsTo(&error_collector);
    parser_.WriteLocationsTo(info_tree);
    EXPECT_TRUE(parser_.ParseFromString(input, proto));
    parser_.WriteLocationsTo(nullptr);
    parser_.RecordErrorsTo(nullptr);
  }

  void ExpectLocation(TextFormat::ParseInfoTree* tree, const Descriptor* d,
                      const std::string& field_name, int index, int line,
                      int column) {
    TextFormat::ParseLocation location =
        tree->GetLocation(d->FindFieldByName(field_name), index);
    EXPECT_EQ(line, location.line);
    EXPECT_EQ(column, location.column);
  }

  // An error collector which simply concatenates all its errors into a big
  // block of text which can be checked.
  class MockErrorCollector : public io::ErrorCollector {
   public:
    MockErrorCollector() {}
    ~MockErrorCollector() {}

    std::string text_;

    // implements ErrorCollector -------------------------------------
    void AddError(int line, int column, const std::string& message) {
      strings::SubstituteAndAppend(&text_, "$0:$1: $2\n", line + 1, column + 1,
                                   message);
    }

    void AddWarning(int line, int column, const std::string& message) {
      AddError(line, column, "WARNING:" + message);
    }
  };

  TextFormat::Parser parser_;
};

TEST_F(TextFormatParserTest, ParseInfoTreeBuilding) {
  std::unique_ptr<unittest::TestAllTypes> message(new unittest::TestAllTypes);
  const Descriptor* d = message->GetDescriptor();

  std::string stringData =
      "optional_int32: 1\n"
      "optional_int64: 2\n"
      "  optional_double: 2.4\n"
      "repeated_int32: 5\n"
      "repeated_int32: 10\n"
      "optional_nested_message <\n"
      "  bb: 78\n"
      ">\n"
      "repeated_nested_message <\n"
      "  bb: 79\n"
      ">\n"
      "repeated_nested_message <\n"
      "  bb: 80\n"
      ">";

  TextFormat::ParseInfoTree tree;
  ExpectSuccessAndTree(stringData, message.get(), &tree);

  // Verify that the tree has the correct positions.
  ExpectLocation(&tree, d, "optional_int32", -1, 0, 0);
  ExpectLocation(&tree, d, "optional_int64", -1, 1, 0);
  ExpectLocation(&tree, d, "optional_double", -1, 2, 2);

  ExpectLocation(&tree, d, "repeated_int32", 0, 3, 0);
  ExpectLocation(&tree, d, "repeated_int32", 1, 4, 0);

  ExpectLocation(&tree, d, "optional_nested_message", -1, 5, 0);
  ExpectLocation(&tree, d, "repeated_nested_message", 0, 8, 0);
  ExpectLocation(&tree, d, "repeated_nested_message", 1, 11, 0);

  // Check for fields not set. For an invalid field, the location returned
  // should be -1, -1.
  ExpectLocation(&tree, d, "repeated_int64", 0, -1, -1);
  ExpectLocation(&tree, d, "repeated_int32", 6, -1, -1);
  ExpectLocation(&tree, d, "some_unknown_field", -1, -1, -1);

  // Verify inside the nested message.
  const FieldDescriptor* nested_field =
      d->FindFieldByName("optional_nested_message");

  TextFormat::ParseInfoTree* nested_tree =
      tree.GetTreeForNested(nested_field, -1);
  ExpectLocation(nested_tree, nested_field->message_type(), "bb", -1, 6, 2);

  // Verify inside another nested message.
  nested_field = d->FindFieldByName("repeated_nested_message");
  nested_tree = tree.GetTreeForNested(nested_field, 0);
  ExpectLocation(nested_tree, nested_field->message_type(), "bb", -1, 9, 2);

  nested_tree = tree.GetTreeForNested(nested_field, 1);
  ExpectLocation(nested_tree, nested_field->message_type(), "bb", -1, 12, 2);

  // Verify a NULL tree for an unknown nested field.
  TextFormat::ParseInfoTree* unknown_nested_tree =
      tree.GetTreeForNested(nested_field, 2);

  EXPECT_EQ(NULL, unknown_nested_tree);
}

TEST_F(TextFormatParserTest, ParseFieldValueFromString) {
  std::unique_ptr<unittest::TestAllTypes> message(new unittest::TestAllTypes);
  const Descriptor* d = message->GetDescriptor();

#define EXPECT_FIELD(name, value, valuestring)                             \
  EXPECT_TRUE(TextFormat::ParseFieldValueFromString(                       \
      valuestring, d->FindFieldByName("optional_" #name), message.get())); \
  EXPECT_EQ(value, message->optional_##name());                            \
  EXPECT_TRUE(message->has_optional_##name());

#define EXPECT_BOOL_FIELD(name, value, valuestring)                        \
  EXPECT_TRUE(TextFormat::ParseFieldValueFromString(                       \
      valuestring, d->FindFieldByName("optional_" #name), message.get())); \
  EXPECT_TRUE(message->optional_##name() == value);                        \
  EXPECT_TRUE(message->has_optional_##name());

#define EXPECT_FLOAT_FIELD(name, value, valuestring)                       \
  EXPECT_TRUE(TextFormat::ParseFieldValueFromString(                       \
      valuestring, d->FindFieldByName("optional_" #name), message.get())); \
  EXPECT_FLOAT_EQ(value, message->optional_##name());                      \
  EXPECT_TRUE(message->has_optional_##name());

#define EXPECT_DOUBLE_FIELD(name, value, valuestring)                      \
  EXPECT_TRUE(TextFormat::ParseFieldValueFromString(                       \
      valuestring, d->FindFieldByName("optional_" #name), message.get())); \
  EXPECT_DOUBLE_EQ(value, message->optional_##name());                     \
  EXPECT_TRUE(message->has_optional_##name());

#define EXPECT_INVALID(name, valuestring)             \
  EXPECT_FALSE(TextFormat::ParseFieldValueFromString( \
      valuestring, d->FindFieldByName("optional_" #name), message.get()));

  // int32
  EXPECT_FIELD(int32, 1, "1");
  EXPECT_FIELD(int32, -1, "-1");
  EXPECT_FIELD(int32, 0x1234, "0x1234");
  EXPECT_INVALID(int32, "a");
  EXPECT_INVALID(int32, "999999999999999999999999999999999999");
  EXPECT_INVALID(int32, "1,2");

  // int64
  EXPECT_FIELD(int64, 1, "1");
  EXPECT_FIELD(int64, -1, "-1");
  EXPECT_FIELD(int64, 0x1234567812345678LL, "0x1234567812345678");
  EXPECT_INVALID(int64, "a");
  EXPECT_INVALID(int64, "999999999999999999999999999999999999");
  EXPECT_INVALID(int64, "1,2");

  // uint64
  EXPECT_FIELD(uint64, 1, "1");
  EXPECT_FIELD(uint64, 0xf234567812345678ULL, "0xf234567812345678");
  EXPECT_INVALID(uint64, "-1");
  EXPECT_INVALID(uint64, "a");
  EXPECT_INVALID(uint64, "999999999999999999999999999999999999");
  EXPECT_INVALID(uint64, "1,2");

  // fixed32
  EXPECT_FIELD(fixed32, 1, "1");
  EXPECT_FIELD(fixed32, 0x12345678, "0x12345678");
  EXPECT_INVALID(fixed32, "-1");
  EXPECT_INVALID(fixed32, "a");
  EXPECT_INVALID(fixed32, "999999999999999999999999999999999999");
  EXPECT_INVALID(fixed32, "1,2");

  // fixed64
  EXPECT_FIELD(fixed64, 1, "1");
  EXPECT_FIELD(fixed64, 0x1234567812345678ULL, "0x1234567812345678");
  EXPECT_INVALID(fixed64, "-1");
  EXPECT_INVALID(fixed64, "a");
  EXPECT_INVALID(fixed64, "999999999999999999999999999999999999");
  EXPECT_INVALID(fixed64, "1,2");

  // bool
  EXPECT_BOOL_FIELD(bool, true, "true");
  EXPECT_BOOL_FIELD(bool, false, "false");
  EXPECT_BOOL_FIELD(bool, true, "1");
  EXPECT_BOOL_FIELD(bool, true, "t");
  EXPECT_BOOL_FIELD(bool, false, "0");
  EXPECT_BOOL_FIELD(bool, false, "f");
  EXPECT_FIELD(bool, true, "True");
  EXPECT_FIELD(bool, false, "False");
  EXPECT_INVALID(bool, "tRue");
  EXPECT_INVALID(bool, "faLse");
  EXPECT_INVALID(bool, "2");
  EXPECT_INVALID(bool, "-0");
  EXPECT_INVALID(bool, "on");
  EXPECT_INVALID(bool, "a");

  // float
  EXPECT_FIELD(float, 1, "1");
  EXPECT_FLOAT_FIELD(float, 1.5, "1.5");
  EXPECT_FLOAT_FIELD(float, 1.5e3, "1.5e3");
  EXPECT_FLOAT_FIELD(float, -4.55, "-4.55");
  EXPECT_INVALID(float, "a");
  EXPECT_INVALID(float, "1,2");

  // double
  EXPECT_FIELD(double, 1, "1");
  EXPECT_FIELD(double, -1, "-1");
  EXPECT_DOUBLE_FIELD(double, 2.3, "2.3");
  EXPECT_DOUBLE_FIELD(double, 3e5, "3e5");
  EXPECT_INVALID(double, "a");
  EXPECT_INVALID(double, "1,2");
  // Rejects hex and oct numbers for a double field.
  EXPECT_INVALID(double, "0xf");
  EXPECT_INVALID(double, "012");

  // string
  EXPECT_FIELD(string, "hello", "\"hello\"");
  EXPECT_FIELD(string, "-1.87", "'-1.87'");
  EXPECT_INVALID(string, "hello");  // without quote for value

  // enum
  EXPECT_FIELD(nested_enum, unittest::TestAllTypes::BAR, "BAR");
  EXPECT_FIELD(nested_enum, unittest::TestAllTypes::BAZ,
               StrCat(unittest::TestAllTypes::BAZ));
  EXPECT_INVALID(nested_enum, "FOOBAR");

  // message
  EXPECT_TRUE(TextFormat::ParseFieldValueFromString(
      "<bb:12>", d->FindFieldByName("optional_nested_message"), message.get()));
  EXPECT_EQ(12, message->optional_nested_message().bb());
  EXPECT_TRUE(message->has_optional_nested_message());
  EXPECT_INVALID(nested_message, "any");

#undef EXPECT_FIELD
#undef EXPECT_BOOL_FIELD
#undef EXPECT_FLOAT_FIELD
#undef EXPECT_DOUBLE_FIELD
#undef EXPECT_INVALID
}

TEST_F(TextFormatParserTest, InvalidToken) {
  ExpectFailure("optional_bool: true\n-5\n", "Expected identifier, got: -", 2,
                1);

  ExpectFailure("optional_bool: true!\n", "Expected identifier, got: !", 1, 20);
  ExpectFailure("\"some string\"", "Expected identifier, got: \"some string\"",
                1, 1);
}

TEST_F(TextFormatParserTest, InvalidFieldName) {
  ExpectFailure(
      "invalid_field: somevalue\n",
      "Message type \"protobuf_unittest.TestAllTypes\" has no field named "
      "\"invalid_field\".",
      1, 14);
}

TEST_F(TextFormatParserTest, InvalidCapitalization) {
  // We require that group names be exactly as they appear in the .proto.
  ExpectFailure(
      "optionalgroup {\na: 15\n}\n",
      "Message type \"protobuf_unittest.TestAllTypes\" has no field named "
      "\"optionalgroup\".",
      1, 15);
  ExpectFailure(
      "OPTIONALgroup {\na: 15\n}\n",
      "Message type \"protobuf_unittest.TestAllTypes\" has no field named "
      "\"OPTIONALgroup\".",
      1, 15);
  ExpectFailure(
      "Optional_Double: 10.0\n",
      "Message type \"protobuf_unittest.TestAllTypes\" has no field named "
      "\"Optional_Double\".",
      1, 16);
}

TEST_F(TextFormatParserTest, AllowIgnoreCapitalizationError) {
  TextFormat::Parser parser;
  protobuf_unittest::TestAllTypes proto;

  // These fields have a mismatching case.
  EXPECT_FALSE(parser.ParseFromString("Optional_Double: 10.0", &proto));
  EXPECT_FALSE(parser.ParseFromString("oPtIoNaLgRoUp { a: 15 }", &proto));

  // ... but are parsed correctly if we match case insensitive.
  parser.AllowCaseInsensitiveField(true);
  EXPECT_TRUE(parser.ParseFromString("Optional_Double: 10.0", &proto));
  EXPECT_EQ(10.0, proto.optional_double());
  EXPECT_TRUE(parser.ParseFromString("oPtIoNaLgRoUp { a: 15 }", &proto));
  EXPECT_EQ(15, proto.optionalgroup().a());
}

TEST_F(TextFormatParserTest, InvalidFieldValues) {
  // Invalid values for a double/float field.
  ExpectFailure("optional_double: \"hello\"\n",
                "Expected double, got: \"hello\"", 1, 18);
  ExpectFailure("optional_double: true\n", "Expected double, got: true", 1, 18);
  ExpectFailure("optional_double: !\n", "Expected double, got: !", 1, 18);
  ExpectFailure("optional_double {\n  \n}\n", "Expected \":\", found \"{\".", 1,
                17);

  // Invalid values for a signed integer field.
  ExpectFailure("optional_int32: \"hello\"\n",
                "Expected integer, got: \"hello\"", 1, 17);
  ExpectFailure("optional_int32: true\n", "Expected integer, got: true", 1, 17);
  ExpectFailure("optional_int32: 4.5\n", "Expected integer, got: 4.5", 1, 17);
  ExpectFailure("optional_int32: !\n", "Expected integer, got: !", 1, 17);
  ExpectFailure("optional_int32 {\n \n}\n", "Expected \":\", found \"{\".", 1,
                16);
  ExpectFailure("optional_int32: 0x80000000\n",
                "Integer out of range (0x80000000)", 1, 17);
  ExpectFailure("optional_int64: 0x8000000000000000\n",
                "Integer out of range (0x8000000000000000)", 1, 17);
  ExpectFailure("optional_int32: -0x80000001\n",
                "Integer out of range (0x80000001)", 1, 18);
  ExpectFailure("optional_int64: -0x8000000000000001\n",
                "Integer out of range (0x8000000000000001)", 1, 18);

  // Invalid values for an unsigned integer field.
  ExpectFailure("optional_uint64: \"hello\"\n",
                "Expected integer, got: \"hello\"", 1, 18);
  ExpectFailure("optional_uint64: true\n", "Expected integer, got: true", 1,
                18);
  ExpectFailure("optional_uint64: 4.5\n", "Expected integer, got: 4.5", 1, 18);
  ExpectFailure("optional_uint64: -5\n", "Expected integer, got: -", 1, 18);
  ExpectFailure("optional_uint64: !\n", "Expected integer, got: !", 1, 18);
  ExpectFailure("optional_uint64 {\n \n}\n", "Expected \":\", found \"{\".", 1,
                17);
  ExpectFailure("optional_uint32: 0x100000000\n",
                "Integer out of range (0x100000000)", 1, 18);
  ExpectFailure("optional_uint64: 0x10000000000000000\n",
                "Integer out of range (0x10000000000000000)", 1, 18);

  // Invalid values for a boolean field.
  ExpectFailure("optional_bool: \"hello\"\n",
                "Expected identifier, got: \"hello\"", 1, 16);
  ExpectFailure("optional_bool: 5\n", "Integer out of range (5)", 1, 16);
  ExpectFailure("optional_bool: -7.5\n", "Expected identifier, got: -", 1, 16);
  ExpectFailure("optional_bool: !\n", "Expected identifier, got: !", 1, 16);

  ExpectFailure(
      "optional_bool: meh\n",
      "Invalid value for boolean field \"optional_bool\". Value: \"meh\".", 2,
      1);

  ExpectFailure("optional_bool {\n \n}\n", "Expected \":\", found \"{\".", 1,
                15);

  // Invalid values for a string field.
  ExpectFailure("optional_string: true\n", "Expected string, got: true", 1, 18);
  ExpectFailure("optional_string: 5\n", "Expected string, got: 5", 1, 18);
  ExpectFailure("optional_string: -7.5\n", "Expected string, got: -", 1, 18);
  ExpectFailure("optional_string: !\n", "Expected string, got: !", 1, 18);
  ExpectFailure("optional_string {\n \n}\n", "Expected \":\", found \"{\".", 1,
                17);

  // Invalid values for an enumeration field.
  ExpectFailure("optional_nested_enum: \"hello\"\n",
                "Expected integer or identifier, got: \"hello\"", 1, 23);

  // Valid token, but enum value is not defined.
  ExpectFailure("optional_nested_enum: 5\n",
                "Unknown enumeration value of \"5\" for field "
                "\"optional_nested_enum\".",
                2, 1);
  // We consume the negative sign, so the error position starts one character
  // later.
  ExpectFailure("optional_nested_enum: -7.5\n", "Expected integer, got: 7.5", 1,
                24);
  ExpectFailure("optional_nested_enum: !\n",
                "Expected integer or identifier, got: !", 1, 23);

  ExpectFailure("optional_nested_enum: grah\n",
                "Unknown enumeration value of \"grah\" for field "
                "\"optional_nested_enum\".",
                2, 1);

  ExpectFailure("optional_nested_enum {\n \n}\n",
                "Expected \":\", found \"{\".", 1, 22);
}

TEST_F(TextFormatParserTest, MessageDelimiters) {
  // Non-matching delimiters.
  ExpectFailure("OptionalGroup <\n \n}\n", "Expected \">\", found \"}\".", 3,
                1);

  // Invalid delimiters.
  ExpectFailure("OptionalGroup [\n \n]\n", "Expected \"{\", found \"[\".", 1,
                15);

  // Unending message.
  ExpectFailure("optional_nested_message {\n \nbb: 118\n",
                "Expected identifier, got: ", 4, 1);
}

TEST_F(TextFormatParserTest, UnknownExtension) {
  // Non-matching delimiters.
  ExpectFailure("[blahblah]: 123",
                "Extension \"blahblah\" is not defined or is not an "
                "extension of \"protobuf_unittest.TestAllTypes\".",
                1, 11);
}

TEST_F(TextFormatParserTest, MissingRequired) {
  unittest::TestRequired message;
  ExpectFailure("a: 1", "Message missing required fields: b, c", 0, 1,
                &message);
}

TEST_F(TextFormatParserTest, ParseDuplicateRequired) {
  unittest::TestRequired message;
  ExpectFailure("a: 1 b: 2 c: 3 a: 1",
                "Non-repeated field \"a\" is specified multiple times.", 1, 17,
                &message);
}

TEST_F(TextFormatParserTest, ParseDuplicateOptional) {
  unittest::ForeignMessage message;
  ExpectFailure("c: 1 c: 2",
                "Non-repeated field \"c\" is specified multiple times.", 1, 7,
                &message);
}

TEST_F(TextFormatParserTest, MergeDuplicateRequired) {
  unittest::TestRequired message;
  TextFormat::Parser parser;
  EXPECT_TRUE(parser.MergeFromString("a: 1 b: 2 c: 3 a: 4", &message));
  EXPECT_EQ(4, message.a());
}

TEST_F(TextFormatParserTest, MergeDuplicateOptional) {
  unittest::ForeignMessage message;
  TextFormat::Parser parser;
  EXPECT_TRUE(parser.MergeFromString("c: 1 c: 2", &message));
  EXPECT_EQ(2, message.c());
}

TEST_F(TextFormatParserTest, ExplicitDelimiters) {
  unittest::TestRequired message;
  EXPECT_TRUE(TextFormat::ParseFromString("a:1,b:2;c:3", &message));
  EXPECT_EQ(1, message.a());
  EXPECT_EQ(2, message.b());
  EXPECT_EQ(3, message.c());
}

TEST_F(TextFormatParserTest, PrintErrorsToStderr) {
  std::vector<std::string> errors;

  {
    ScopedMemoryLog log;
    unittest::TestAllTypes proto;
    EXPECT_FALSE(TextFormat::ParseFromString("no_such_field: 1", &proto));
    errors = log.GetMessages(ERROR);
  }

  ASSERT_EQ(1, errors.size());
  EXPECT_EQ(
      "Error parsing text-format protobuf_unittest.TestAllTypes: "
      "1:14: Message type \"protobuf_unittest.TestAllTypes\" has no field "
      "named \"no_such_field\".",
      errors[0]);
}

TEST_F(TextFormatParserTest, FailsOnTokenizationError) {
  std::vector<std::string> errors;

  {
    ScopedMemoryLog log;
    unittest::TestAllTypes proto;
    EXPECT_FALSE(TextFormat::ParseFromString("\020", &proto));
    errors = log.GetMessages(ERROR);
  }

  ASSERT_EQ(1, errors.size());
  EXPECT_EQ(
      "Error parsing text-format protobuf_unittest.TestAllTypes: "
      "1:1: Invalid control characters encountered in text.",
      errors[0]);
}

TEST_F(TextFormatParserTest, ParseDeprecatedField) {
  unittest::TestDeprecatedFields message;
  ExpectMessage("deprecated_int32: 42",
                "WARNING:text format contains deprecated field "
                "\"deprecated_int32\"",
                1, 21, &message, true);
}

TEST_F(TextFormatParserTest, SetRecursionLimit) {
  const char* format = "child: { $0 }";
  std::string input;
  for (int i = 0; i < 100; ++i) input = strings::Substitute(format, input);

  unittest::NestedTestAllTypes message;
  ExpectSuccessAndTree(input, &message, nullptr);

  input = strings::Substitute(format, input);
  parser_.SetRecursionLimit(100);
  ExpectMessage(input, "Message is too deep", 1, 908, &message, false);

  parser_.SetRecursionLimit(101);
  ExpectSuccessAndTree(input, &message, nullptr);
}

class TextFormatMessageSetTest : public testing::Test {
 protected:
  static const char proto_debug_string_[];
};
const char TextFormatMessageSetTest::proto_debug_string_[] =
    "message_set {\n"
    "  [protobuf_unittest.TestMessageSetExtension1] {\n"
    "    i: 23\n"
    "  }\n"
    "  [protobuf_unittest.TestMessageSetExtension2] {\n"
    "    str: \"foo\"\n"
    "  }\n"
    "}\n";

TEST_F(TextFormatMessageSetTest, Serialize) {
  protobuf_unittest::TestMessageSetContainer proto;
  protobuf_unittest::TestMessageSetExtension1* item_a =
      proto.mutable_message_set()->MutableExtension(
          protobuf_unittest::TestMessageSetExtension1::message_set_extension);
  item_a->set_i(23);
  protobuf_unittest::TestMessageSetExtension2* item_b =
      proto.mutable_message_set()->MutableExtension(
          protobuf_unittest::TestMessageSetExtension2::message_set_extension);
  item_b->set_str("foo");
  EXPECT_EQ(proto_debug_string_, proto.DebugString());
}

TEST_F(TextFormatMessageSetTest, Deserialize) {
  protobuf_unittest::TestMessageSetContainer proto;
  ASSERT_TRUE(TextFormat::ParseFromString(proto_debug_string_, &proto));
  EXPECT_EQ(
      23,
      proto.message_set()
          .GetExtension(
              protobuf_unittest::TestMessageSetExtension1::message_set_extension)
          .i());
  EXPECT_EQ(
      "foo",
      proto.message_set()
          .GetExtension(
              protobuf_unittest::TestMessageSetExtension2::message_set_extension)
          .str());

  // Ensure that these are the only entries present.
  std::vector<const FieldDescriptor*> descriptors;
  proto.message_set().GetReflection()->ListFields(proto.message_set(),
                                                  &descriptors);
  EXPECT_EQ(2, descriptors.size());
}

TEST(TextFormatUnknownFieldTest, TestUnknownField) {
  protobuf_unittest::TestAllTypes proto;
  TextFormat::Parser parser;
  // Unknown field is not permitted by default.
  EXPECT_FALSE(parser.ParseFromString("unknown_field: 12345", &proto));
  EXPECT_FALSE(parser.ParseFromString("12345678: 12345", &proto));

  parser.AllowUnknownField(true);
  EXPECT_TRUE(parser.ParseFromString("unknown_field: 12345", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: -12345", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: 1.2345", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: -1.2345", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: 1.2345f", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: -1.2345f", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: inf", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: -inf", &proto));
  EXPECT_TRUE(parser.ParseFromString("unknown_field: TYPE_STRING", &proto));
  EXPECT_TRUE(
      parser.ParseFromString("unknown_field: \"string value\"", &proto));
  // Invalid field value
  EXPECT_FALSE(parser.ParseFromString("unknown_field: -TYPE_STRING", &proto));
  // Two or more unknown fields
  EXPECT_TRUE(
      parser.ParseFromString("unknown_field1: TYPE_STRING\n"
                             "unknown_field2: 12345",
                             &proto));
  // Unknown nested message
  EXPECT_TRUE(
      parser.ParseFromString("unknown_message1: {}\n"
                             "unknown_message2 {\n"
                             "  unknown_field: 12345\n"
                             "}\n"
                             "unknown_message3 <\n"
                             "  unknown_nested_message {\n"
                             "    unknown_field: 12345\n"
                             "  }\n"
                             ">",
                             &proto));
  // Unmatched delimeters for message body
  EXPECT_FALSE(parser.ParseFromString("unknown_message: {>", &proto));
  // Unknown extension
  EXPECT_TRUE(
      parser.ParseFromString("[somewhere.unknown_extension1]: 12345\n"
                             "[somewhere.unknown_extension2] {\n"
                             "  unknown_field: 12345\n"
                             "}",
                             &proto));
  // Unknown fields between known fields
  ASSERT_TRUE(
      parser.ParseFromString("optional_int32: 1\n"
                             "unknown_field: 12345\n"
                             "optional_string: \"string\"\n"
                             "unknown_message { unknown: 0 }\n"
                             "optional_nested_message { bb: 2 }",
                             &proto));
  EXPECT_EQ(1, proto.optional_int32());
  EXPECT_EQ("string", proto.optional_string());
  EXPECT_EQ(2, proto.optional_nested_message().bb());

  // Unknown field with numeric tag number instead of identifier.
  EXPECT_TRUE(parser.ParseFromString("12345678: 12345", &proto));

  // Nested unknown extensions.
  EXPECT_TRUE(
      parser.ParseFromString("[test.extension1] <\n"
                             "  unknown_nested_message <\n"
                             "    [test.extension2] <\n"
                             "      unknown_field: 12345\n"
                             "    >\n"
                             "  >\n"
                             ">",
                             &proto));
  EXPECT_TRUE(
      parser.ParseFromString("[test.extension1] {\n"
                             "  unknown_nested_message {\n"
                             "    [test.extension2] {\n"
                             "      unknown_field: 12345\n"
                             "    }\n"
                             "  }\n"
                             "}",
                             &proto));
  EXPECT_TRUE(
      parser.ParseFromString("[test.extension1] <\n"
                             "  some_unknown_fields: <\n"
                             "    unknown_field: 12345\n"
                             "  >\n"
                             ">",
                             &proto));
  EXPECT_TRUE(
      parser.ParseFromString("[test.extension1] {\n"
                             "  some_unknown_fields: {\n"
                             "    unknown_field: 12345\n"
                             "  }\n"
                             "}",
                             &proto));

  // Unknown field with compact repetition.
  EXPECT_TRUE(parser.ParseFromString("unknown_field: [1, 2]", &proto));
  // Unknown field with compact repetition of some unknown enum.
  EXPECT_TRUE(parser.ParseFromString("unknown_field: [VAL1, VAL2]", &proto));
  // Unknown field with compact repetition with sub-message.
  EXPECT_TRUE(parser.ParseFromString("unknown_field: [{a:1}, <b:2>]", &proto));
}

TEST(TextFormatUnknownFieldTest, TestAnyInUnknownField) {
  protobuf_unittest::TestAllTypes proto;
  TextFormat::Parser parser;
  parser.AllowUnknownField(true);
  EXPECT_TRUE(
      parser.ParseFromString("unknown {\n"
                             "  [type.googleapis.com/foo.bar] {\n"
                             "  }\n"
                             "}",
                             &proto));
}

TEST(TextFormatUnknownFieldTest, TestUnknownExtension) {
  protobuf_unittest::TestAllTypes proto;
  TextFormat::Parser parser;
  std::string message_with_ext =
      "[test.extension1] {\n"
      "  some_unknown_fields: {\n"
      "    unknown_field: 12345\n"
      "  }\n"
      "}";
  // Unknown extensions are not permitted by default.
  EXPECT_FALSE(parser.ParseFromString(message_with_ext, &proto));
  // AllowUnknownField implies AllowUnknownExtension.
  parser.AllowUnknownField(true);
  EXPECT_TRUE(parser.ParseFromString(message_with_ext, &proto));

  parser.AllowUnknownField(false);
  EXPECT_FALSE(parser.ParseFromString(message_with_ext, &proto));
  parser.AllowUnknownExtension(true);
  EXPECT_TRUE(parser.ParseFromString(message_with_ext, &proto));
  // Unknown fields are still not accepted.
  EXPECT_FALSE(parser.ParseFromString("unknown_field: 1", &proto));
}


}  // namespace text_format_unittest
}  // namespace protobuf
}  // namespace google
