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

// Author: kenton@google.com (Kenton Varda)
//
// TODO(kenton):  Share code with the versions of this test in other languages?
//   It seemed like parameterizing it would add more complexity than it is
//   worth.

#include <memory>

#include <google/protobuf/compiler/cpp/cpp_generator.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {
namespace {

class TestGenerator : public CodeGenerator {
 public:
  TestGenerator() {}
  ~TestGenerator() {}

  virtual bool Generate(const FileDescriptor* file,
                        const std::string& parameter, GeneratorContext* context,
                        std::string* error) const {
    TryInsert("test.pb.h", "includes", context);
    TryInsert("test.pb.h", "namespace_scope", context);
    TryInsert("test.pb.h", "global_scope", context);
    TryInsert("test.pb.h", "class_scope:foo.Bar", context);
    TryInsert("test.pb.h", "class_scope:foo.Bar.Baz", context);

    TryInsert("test.pb.cc", "includes", context);
    TryInsert("test.pb.cc", "namespace_scope", context);
    TryInsert("test.pb.cc", "global_scope", context);

    // Check field accessors for an optional int32:
    TryInsert("test.pb.h", "field_get:foo.Bar.optInt", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.optInt", context);

    // Check field accessors for a repeated int32:
    TryInsert("test.pb.h", "field_get:foo.Bar.repeatedInt", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.repeatedInt", context);

    // Check field accessors for a required string:
    TryInsert("test.pb.h", "field_get:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_set_allocated:foo.Bar.requiredString",
              context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.requiredString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.requiredString", context);

    // Check field accessors for a repeated string:
    TryInsert("test.pb.h", "field_get:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.repeatedString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.repeatedString", context);

    // Check field accessors for an int inside oneof{}:
    TryInsert("test.pb.h", "field_get:foo.Bar.oneOfInt", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.oneOfInt", context);

    // Check field accessors for a string inside oneof{}:
    TryInsert("test.pb.h", "field_get:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set_allocated:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set_char:foo.Bar.oneOfString", context);
    TryInsert("test.pb.h", "field_set_pointer:foo.Bar.oneOfString", context);

    // Check field accessors for an optional message:
    TryInsert("test.pb.h", "field_get:foo.Bar.optMessage", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.optMessage", context);
    TryInsert("test.pb.h", "field_set_allocated:foo.Bar.optMessage", context);

    // Check field accessors for a repeated message:
    TryInsert("test.pb.h", "field_add:foo.Bar.repeatedMessage", context);
    TryInsert("test.pb.h", "field_get:foo.Bar.repeatedMessage", context);
    TryInsert("test.pb.h", "field_list:foo.Bar.repeatedMessage", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.repeatedMessage", context);
    TryInsert("test.pb.h", "field_mutable_list:foo.Bar.repeatedMessage",
              context);

    // Check field accessors for a message inside oneof{}:
    TryInsert("test.pb.h", "field_get:foo.Bar.oneOfMessage", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.oneOfMessage", context);
    TryInsert("test.pb.cc", "field_set_allocated:foo.Bar.oneOfMessage",
              context);

    // Check field accessors for an optional enum:
    TryInsert("test.pb.h", "field_get:foo.Bar.optEnum", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.optEnum", context);

    // Check field accessors for a repeated enum:
    TryInsert("test.pb.h", "field_get:foo.Bar.repeatedEnum", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.repeatedEnum", context);
    TryInsert("test.pb.h", "field_add:foo.Bar.repeatedEnum", context);
    TryInsert("test.pb.h", "field_list:foo.Bar.repeatedEnum", context);
    TryInsert("test.pb.h", "field_mutable_list:foo.Bar.repeatedEnum", context);

    // Check field accessors for an enum inside oneof{}:
    TryInsert("test.pb.h", "field_get:foo.Bar.oneOfEnum", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.oneOfEnum", context);

    // Check field accessors for a required cord:
    TryInsert("test.pb.h", "field_get:foo.Bar.requiredCord", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.requiredCord", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.requiredCord", context);

    // Check field accessors for a repeated cord:
    TryInsert("test.pb.h", "field_get:foo.Bar.repeatedCord", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.repeatedCord", context);
    TryInsert("test.pb.h", "field_add:foo.Bar.repeatedCord", context);
    TryInsert("test.pb.h", "field_list:foo.Bar.repeatedCord", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.repeatedCord", context);
    TryInsert("test.pb.h", "field_mutable_list:foo.Bar.repeatedCord", context);

    // Check field accessors for a cord inside oneof{}:
    TryInsert("test.pb.h", "field_get:foo.Bar.oneOfCord", context);
    TryInsert("test.pb.h", "field_set:foo.Bar.oneOfCord", context);
    TryInsert("test.pb.h", "field_mutable:foo.Bar.oneOfCord", context);

    return true;
  }

  void TryInsert(const std::string& filename,
                 const std::string& insertion_point,
                 GeneratorContext* context) const {
    std::unique_ptr<io::ZeroCopyOutputStream> output(
        context->OpenForInsert(filename, insertion_point));
    io::Printer printer(output.get(), '$');
    printer.Print("// inserted $name$\n", "name", insertion_point);
  }
};

// This test verifies that all the expected insertion points exist.  It does
// not verify that they are correctly-placed; that would require actually
// compiling the output which is a bit more than I care to do for this test.
TEST(CppPluginTest, PluginTest) {
  GOOGLE_CHECK_OK(File::SetContents(TestTempDir() + "/test.proto",
                             "syntax = \"proto2\";\n"
                             "package foo;\n"
                             "\n"
                             "enum Thud { VALUE = 0; }\n"
                             "\n"
                             "message Bar {\n"
                             "  message Baz {}\n"
                             "  optional int32 optInt = 1;\n"
                             "  repeated int32 repeatedInt = 2;\n"
                             "\n"
                             "  required string requiredString = 3;\n"
                             "  repeated string repeatedString = 4;\n"
                             "\n"
                             "  optional Baz optMessage = 6;\n"
                             "  repeated Baz repeatedMessage = 7;\n"
                             "\n"
                             "  optional Thud optEnum = 8;\n"
                             "  repeated Thud repeatedEnum = 9;\n"
                             "\n"
                             "  required string requiredCord = 10 [\n"
                             "    ctype = CORD\n"
                             "  ];\n"
                             "  repeated string repeatedCord = 11 [\n"
                             "    ctype = CORD\n"
                             "  ];\n"
                             "\n"
                             "  oneof Qux {\n"
                             "    int64 oneOfInt = 20;\n"
                             "    string oneOfString = 21;\n"
                             "    Baz oneOfMessage = 22;\n"
                             "    Thud oneOfEnum = 23;"
                             "    string oneOfCord = 24 [\n"
                             "      ctype = CORD\n"
                             "    ];\n"
                             "  }\n"
                             "}\n",
                             true));

  CommandLineInterface cli;
  cli.SetInputsAreProtoPathRelative(true);

  CppGenerator cpp_generator;
  TestGenerator test_generator;
  cli.RegisterGenerator("--cpp_out", &cpp_generator, "");
  cli.RegisterGenerator("--test_out", &test_generator, "");

  std::string proto_path = "-I" + TestTempDir();
  std::string cpp_out = "--cpp_out=" + TestTempDir();
  std::string test_out = "--test_out=" + TestTempDir();

  const char* argv[] = {"protoc", proto_path.c_str(), cpp_out.c_str(),
                        test_out.c_str(), "test.proto"};

  EXPECT_EQ(0, cli.Run(5, argv));
}

}  // namespace
}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
