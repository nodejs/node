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

#include <memory>
#include <list>

#include <google/protobuf/compiler/ruby/ruby_generator.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/printer.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/testing/file.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace ruby {
namespace {

string FindRubyTestDir() {
  return TestSourceDir() + "/google/protobuf/compiler/ruby";
}

// This test is a simple golden-file test over the output of the Ruby code
// generator. When we make changes to the Ruby extension and alter the Ruby code
// generator to use those changes, we should (i) manually test the output of the
// code generator with the extension, and (ii) update the golden output above.
// Some day, we may integrate build systems between protoc and the language
// extensions to the point where we can do this test in a more automated way.

void RubyTest(string proto_file) {
  string ruby_tests = FindRubyTestDir();

  google::protobuf::compiler::CommandLineInterface cli;
  cli.SetInputsAreProtoPathRelative(true);

  ruby::Generator ruby_generator;
  cli.RegisterGenerator("--ruby_out", &ruby_generator, "");

  // Copy generated_code.proto to the temporary test directory.
  string test_input;
  GOOGLE_CHECK_OK(File::GetContents(
      ruby_tests + proto_file + ".proto",
      &test_input,
      true));
  GOOGLE_CHECK_OK(File::SetContents(
      TestTempDir() + proto_file + ".proto",
      test_input,
      true));

  // Invoke the proto compiler (we will be inside TestTempDir() at this point).
  string ruby_out = "--ruby_out=" + TestTempDir();
  string proto_path = "--proto_path=" + TestTempDir();
  string proto_target = TestTempDir() + proto_file + ".proto";
  const char* argv[] = {
    "protoc",
    ruby_out.c_str(),
    proto_path.c_str(),
    proto_target.c_str(),
  };

  EXPECT_EQ(0, cli.Run(4, argv));

  // Load the generated output and compare to the expected result.
  string output;
  GOOGLE_CHECK_OK(File::GetContentsAsText(
      TestTempDir() + proto_file + "_pb.rb",
      &output,
      true));
  string expected_output;
  GOOGLE_CHECK_OK(File::GetContentsAsText(
      ruby_tests + proto_file + "_pb.rb",
      &expected_output,
      true));
  EXPECT_EQ(expected_output, output);
}

TEST(RubyGeneratorTest, Proto3GeneratorTest) {
  RubyTest("/ruby_generated_code");
}

TEST(RubyGeneratorTest, Proto2GeneratorTest) {
    RubyTest("/ruby_generated_code_proto2");
}

TEST(RubyGeneratorTest, Proto3ImplicitPackageTest) {
    RubyTest("/ruby_generated_pkg_implicit");
}

TEST(RubyGeneratorTest, Proto3ExplictPackageTest) {
    RubyTest("/ruby_generated_pkg_explicit");
}

TEST(RubyGeneratorTest, Proto3ExplictLegacyPackageTest) {
    RubyTest("/ruby_generated_pkg_explicit_legacy");
}

}  // namespace
}  // namespace ruby
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
