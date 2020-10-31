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
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _MSC_VER
#include <unistd.h>
#endif
#include <memory>
#include <vector>

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/compiler/mock_code_generator.h>
#include <google/protobuf/compiler/subprocess.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/test_util2.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/io/io_win32.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/stubs/substitute.h>

#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {

#if defined(_WIN32)
// DO NOT include <io.h>, instead create functions in io_win32.{h,cc} and import
// them like we do below.
using google::protobuf::io::win32::access;
using google::protobuf::io::win32::close;
using google::protobuf::io::win32::dup;
using google::protobuf::io::win32::dup2;
using google::protobuf::io::win32::open;
using google::protobuf::io::win32::write;
#endif

// Disable the whole test when we use tcmalloc for "draconian" heap checks, in
// which case tcmalloc will print warnings that fail the plugin tests.
#if !GOOGLE_PROTOBUF_HEAP_CHECK_DRACONIAN


namespace {

bool FileExists(const std::string& path) {
  return File::Exists(path);
}

class CommandLineInterfaceTest : public testing::Test {
 protected:
  virtual void SetUp();
  virtual void TearDown();

  // Runs the CommandLineInterface with the given command line.  The
  // command is automatically split on spaces, and the string "$tmpdir"
  // is replaced with TestTempDir().
  void Run(const std::string& command);
  void RunWithArgs(std::vector<std::string> args);

  // -----------------------------------------------------------------
  // Methods to set up the test (called before Run()).

  class NullCodeGenerator;

  // Normally plugins are allowed for all tests.  Call this to explicitly
  // disable them.
  void DisallowPlugins() { disallow_plugins_ = true; }

  // Create a temp file within temp_directory_ with the given name.
  // The containing directory is also created if necessary.
  void CreateTempFile(const std::string& name, const std::string& contents);

  // Create a subdirectory within temp_directory_.
  void CreateTempDir(const std::string& name);

#ifdef PROTOBUF_OPENSOURCE
  // Change working directory to temp directory.
  void SwitchToTempDirectory() {
    File::ChangeWorkingDirectory(temp_directory_);
  }
#else   // !PROTOBUF_OPENSOURCE
  // TODO(teboring): Figure out how to change and get working directory in
  // google3.
#endif  // !PROTOBUF_OPENSOURCE

  // -----------------------------------------------------------------
  // Methods to check the test results (called after Run()).

  // Checks that no text was written to stderr during Run(), and Run()
  // returned 0.
  void ExpectNoErrors();

  // Checks that Run() returned non-zero and the stderr output is exactly
  // the text given.  expected_test may contain references to "$tmpdir",
  // which will be replaced by the temporary directory path.
  void ExpectErrorText(const std::string& expected_text);

  // Checks that Run() returned non-zero and the stderr contains the given
  // substring.
  void ExpectErrorSubstring(const std::string& expected_substring);

  // Checks that the captured stdout is the same as the expected_text.
  void ExpectCapturedStdout(const std::string& expected_text);

  // Checks that Run() returned zero and the stdout contains the given
  // substring.
  void ExpectCapturedStdoutSubstringWithZeroReturnCode(
      const std::string& expected_substring);

#if defined(_WIN32) && !defined(__CYGWIN__)
  // Returns true if ExpectErrorSubstring(expected_substring) would pass, but
  // does not fail otherwise.
  bool HasAlternateErrorSubstring(const std::string& expected_substring);
#endif  // _WIN32 && !__CYGWIN__

  // Checks that MockCodeGenerator::Generate() was called in the given
  // context (or the generator in test_plugin.cc, which produces the same
  // output).  That is, this tests if the generator with the given name
  // was called with the given parameter and proto file and produced the
  // given output file.  This is checked by reading the output file and
  // checking that it contains the content that MockCodeGenerator would
  // generate given these inputs.  message_name is the name of the first
  // message that appeared in the proto file; this is just to make extra
  // sure that the correct file was parsed.
  void ExpectGenerated(const std::string& generator_name,
                       const std::string& parameter,
                       const std::string& proto_name,
                       const std::string& message_name);
  void ExpectGenerated(const std::string& generator_name,
                       const std::string& parameter,
                       const std::string& proto_name,
                       const std::string& message_name,
                       const std::string& output_directory);
  void ExpectGeneratedWithMultipleInputs(const std::string& generator_name,
                                         const std::string& all_proto_names,
                                         const std::string& proto_name,
                                         const std::string& message_name);
  void ExpectGeneratedWithInsertions(const std::string& generator_name,
                                     const std::string& parameter,
                                     const std::string& insertions,
                                     const std::string& proto_name,
                                     const std::string& message_name);
  void CheckGeneratedAnnotations(const std::string& name,
                                 const std::string& file);

#if defined(_WIN32)
  void ExpectNullCodeGeneratorCalled(const std::string& parameter);
#endif  // _WIN32


  void ReadDescriptorSet(const std::string& filename,
                         FileDescriptorSet* descriptor_set);

  void WriteDescriptorSet(const std::string& filename,
                          const FileDescriptorSet* descriptor_set);

  void ExpectFileContent(const std::string& filename,
                         const std::string& content);

 private:
  // The object we are testing.
  CommandLineInterface cli_;

  // Was DisallowPlugins() called?
  bool disallow_plugins_;

  // We create a directory within TestTempDir() in order to add extra
  // protection against accidentally deleting user files (since we recursively
  // delete this directory during the test).  This is the full path of that
  // directory.
  std::string temp_directory_;

  // The result of Run().
  int return_code_;

  // The captured stderr output.
  std::string error_text_;

  // The captured stdout.
  std::string captured_stdout_;

  // Pointers which need to be deleted later.
  std::vector<CodeGenerator*> mock_generators_to_delete_;

  NullCodeGenerator* null_generator_;
};

class CommandLineInterfaceTest::NullCodeGenerator : public CodeGenerator {
 public:
  NullCodeGenerator() : called_(false) {}
  ~NullCodeGenerator() {}

  mutable bool called_;
  mutable std::string parameter_;

  // implements CodeGenerator ----------------------------------------
  bool Generate(const FileDescriptor* file, const std::string& parameter,
                GeneratorContext* context, std::string* error) const {
    called_ = true;
    parameter_ = parameter;
    return true;
  }
};

// ===================================================================

void CommandLineInterfaceTest::SetUp() {
  temp_directory_ = TestTempDir() + "/proto2_cli_test_temp";

  // If the temp directory already exists, it must be left over from a
  // previous run.  Delete it.
  if (FileExists(temp_directory_)) {
    File::DeleteRecursively(temp_directory_, NULL, NULL);
  }

  // Create the temp directory.
  GOOGLE_CHECK_OK(File::CreateDir(temp_directory_, 0777));

  // Register generators.
  CodeGenerator* generator = new MockCodeGenerator("test_generator");
  mock_generators_to_delete_.push_back(generator);
  cli_.RegisterGenerator("--test_out", "--test_opt", generator, "Test output.");
  cli_.RegisterGenerator("-t", generator, "Test output.");

  generator = new MockCodeGenerator("alt_generator");
  mock_generators_to_delete_.push_back(generator);
  cli_.RegisterGenerator("--alt_out", generator, "Alt output.");

  generator = null_generator_ = new NullCodeGenerator();
  mock_generators_to_delete_.push_back(generator);
  cli_.RegisterGenerator("--null_out", generator, "Null output.");


  disallow_plugins_ = false;
}

void CommandLineInterfaceTest::TearDown() {
  // Delete the temp directory.
  if (FileExists(temp_directory_)) {
    File::DeleteRecursively(temp_directory_, NULL, NULL);
  }

  // Delete all the MockCodeGenerators.
  for (int i = 0; i < mock_generators_to_delete_.size(); i++) {
    delete mock_generators_to_delete_[i];
  }
  mock_generators_to_delete_.clear();
}

void CommandLineInterfaceTest::Run(const std::string& command) {
  RunWithArgs(Split(command, " ", true));
}

void CommandLineInterfaceTest::RunWithArgs(std::vector<std::string> args) {
  if (!disallow_plugins_) {
    cli_.AllowPlugins("prefix-");
    std::string plugin_path;
#ifdef GOOGLE_PROTOBUF_TEST_PLUGIN_PATH
    plugin_path = GOOGLE_PROTOBUF_TEST_PLUGIN_PATH;
#else
    const char* possible_paths[] = {
        // When building with shared libraries, libtool hides the real
        // executable
        // in .libs and puts a fake wrapper in the current directory.
        // Unfortunately, due to an apparent bug on Cygwin/MinGW, if one program
        // wrapped in this way (e.g. protobuf-tests.exe) tries to execute
        // another
        // program wrapped in this way (e.g. test_plugin.exe), the latter fails
        // with error code 127 and no explanation message.  Presumably the
        // problem
        // is that the wrapper for protobuf-tests.exe set some environment
        // variables that confuse the wrapper for test_plugin.exe.  Luckily, it
        // turns out that if we simply invoke the wrapped test_plugin.exe
        // directly, it works -- I guess the environment variables set by the
        // protobuf-tests.exe wrapper happen to be correct for it too.  So we do
        // that.
        ".libs/test_plugin.exe",  // Win32 w/autotool (Cygwin / MinGW)
        "test_plugin.exe",        // Other Win32 (MSVC)
        "test_plugin",            // Unix
    };
    for (int i = 0; i < GOOGLE_ARRAYSIZE(possible_paths); i++) {
      if (access(possible_paths[i], F_OK) == 0) {
        plugin_path = possible_paths[i];
        break;
      }
    }
#endif

    if (plugin_path.empty()) {
      GOOGLE_LOG(ERROR)
          << "Plugin executable not found.  Plugin tests are likely to fail.";
    } else {
      args.push_back("--plugin=prefix-gen-plug=" + plugin_path);
    }
  }

  std::unique_ptr<const char*[]> argv(new const char*[args.size()]);

  for (int i = 0; i < args.size(); i++) {
    args[i] = StringReplace(args[i], "$tmpdir", temp_directory_, true);
    argv[i] = args[i].c_str();
  }

  // TODO(jieluo): Cygwin doesn't work well if we try to capture stderr and
  // stdout at the same time. Need to figure out why and add this capture back
  // for Cygwin.
#if !defined(__CYGWIN__)
  CaptureTestStdout();
#endif
  CaptureTestStderr();

  return_code_ = cli_.Run(args.size(), argv.get());

  error_text_ = GetCapturedTestStderr();
#if !defined(__CYGWIN__)
  captured_stdout_ = GetCapturedTestStdout();
#endif
}

// -------------------------------------------------------------------

void CommandLineInterfaceTest::CreateTempFile(const std::string& name,
                                              const std::string& contents) {
  // Create parent directory, if necessary.
  std::string::size_type slash_pos = name.find_last_of('/');
  if (slash_pos != std::string::npos) {
    std::string dir = name.substr(0, slash_pos);
    if (!FileExists(temp_directory_ + "/" + dir)) {
      GOOGLE_CHECK_OK(File::RecursivelyCreateDir(temp_directory_ + "/" + dir,
                                          0777));
    }
  }

  // Write file.
  std::string full_name = temp_directory_ + "/" + name;
  GOOGLE_CHECK_OK(File::SetContents(
      full_name, StringReplace(contents, "$tmpdir", temp_directory_, true),
      true));
}

void CommandLineInterfaceTest::CreateTempDir(const std::string& name) {
  GOOGLE_CHECK_OK(File::RecursivelyCreateDir(temp_directory_ + "/" + name,
                                      0777));
}

// -------------------------------------------------------------------

void CommandLineInterfaceTest::ExpectNoErrors() {
  EXPECT_EQ(0, return_code_);
  EXPECT_EQ("", error_text_);
}

void CommandLineInterfaceTest::ExpectErrorText(
    const std::string& expected_text) {
  EXPECT_NE(0, return_code_);
  EXPECT_EQ(StringReplace(expected_text, "$tmpdir", temp_directory_, true),
            error_text_);
}

void CommandLineInterfaceTest::ExpectErrorSubstring(
    const std::string& expected_substring) {
  EXPECT_NE(0, return_code_);
  EXPECT_PRED_FORMAT2(testing::IsSubstring, expected_substring, error_text_);
}

#if defined(_WIN32) && !defined(__CYGWIN__)
bool CommandLineInterfaceTest::HasAlternateErrorSubstring(
    const std::string& expected_substring) {
  EXPECT_NE(0, return_code_);
  return error_text_.find(expected_substring) != std::string::npos;
}
#endif  // _WIN32 && !__CYGWIN__

void CommandLineInterfaceTest::ExpectGenerated(
    const std::string& generator_name, const std::string& parameter,
    const std::string& proto_name, const std::string& message_name) {
  MockCodeGenerator::ExpectGenerated(generator_name, parameter, "", proto_name,
                                     message_name, proto_name, temp_directory_);
}

void CommandLineInterfaceTest::ExpectGenerated(
    const std::string& generator_name, const std::string& parameter,
    const std::string& proto_name, const std::string& message_name,
    const std::string& output_directory) {
  MockCodeGenerator::ExpectGenerated(generator_name, parameter, "", proto_name,
                                     message_name, proto_name,
                                     temp_directory_ + "/" + output_directory);
}

void CommandLineInterfaceTest::ExpectGeneratedWithMultipleInputs(
    const std::string& generator_name, const std::string& all_proto_names,
    const std::string& proto_name, const std::string& message_name) {
  MockCodeGenerator::ExpectGenerated(generator_name, "", "", proto_name,
                                     message_name, all_proto_names,
                                     temp_directory_);
}

void CommandLineInterfaceTest::ExpectGeneratedWithInsertions(
    const std::string& generator_name, const std::string& parameter,
    const std::string& insertions, const std::string& proto_name,
    const std::string& message_name) {
  MockCodeGenerator::ExpectGenerated(generator_name, parameter, insertions,
                                     proto_name, message_name, proto_name,
                                     temp_directory_);
}

void CommandLineInterfaceTest::CheckGeneratedAnnotations(
    const std::string& name, const std::string& file) {
  MockCodeGenerator::CheckGeneratedAnnotations(name, file, temp_directory_);
}

#if defined(_WIN32)
void CommandLineInterfaceTest::ExpectNullCodeGeneratorCalled(
    const std::string& parameter) {
  EXPECT_TRUE(null_generator_->called_);
  EXPECT_EQ(parameter, null_generator_->parameter_);
}
#endif  // _WIN32


void CommandLineInterfaceTest::ReadDescriptorSet(
    const std::string& filename, FileDescriptorSet* descriptor_set) {
  std::string path = temp_directory_ + "/" + filename;
  std::string file_contents;
  GOOGLE_CHECK_OK(File::GetContents(path, &file_contents, true));

  if (!descriptor_set->ParseFromString(file_contents)) {
    FAIL() << "Could not parse file contents: " << path;
  }
}

void CommandLineInterfaceTest::WriteDescriptorSet(
    const std::string& filename, const FileDescriptorSet* descriptor_set) {
  std::string binary_proto;
  GOOGLE_CHECK(descriptor_set->SerializeToString(&binary_proto));
  CreateTempFile(filename, binary_proto);
}

void CommandLineInterfaceTest::ExpectCapturedStdout(
    const std::string& expected_text) {
  EXPECT_EQ(expected_text, captured_stdout_);
}

void CommandLineInterfaceTest::ExpectCapturedStdoutSubstringWithZeroReturnCode(
    const std::string& expected_substring) {
  EXPECT_EQ(0, return_code_);
  EXPECT_PRED_FORMAT2(testing::IsSubstring, expected_substring,
                      captured_stdout_);
}

void CommandLineInterfaceTest::ExpectFileContent(const std::string& filename,
                                                 const std::string& content) {
  std::string path = temp_directory_ + "/" + filename;
  std::string file_contents;
  GOOGLE_CHECK_OK(File::GetContents(path, &file_contents, true));

  EXPECT_EQ(StringReplace(content, "$tmpdir", temp_directory_, true),
            file_contents);
}

// ===================================================================

TEST_F(CommandLineInterfaceTest, BasicOutput) {
  // Test that the common case works.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, BasicOutput_DescriptorSetIn) {
  // Test that the common case works.
  FileDescriptorSet file_descriptor_set;
  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  WriteDescriptorSet("foo.bin", &file_descriptor_set);

  Run("protocol_compiler --test_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, BasicPlugin) {
  // Test that basic plugins work.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_plugin", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, BasicPlugin_DescriptorSetIn) {
  // Test that basic plugins work.

  FileDescriptorSet file_descriptor_set;
  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  WriteDescriptorSet("foo.bin", &file_descriptor_set);

  Run("protocol_compiler --plug_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_plugin", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, GeneratorAndPlugin) {
  // Invoke a generator and a plugin at the same time.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
  ExpectGenerated("test_plugin", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, GeneratorAndPlugin_DescriptorSetIn) {
  // Invoke a generator and a plugin at the same time.

  FileDescriptorSet file_descriptor_set;
  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  WriteDescriptorSet("foo.bin", &file_descriptor_set);

  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
  ExpectGenerated("test_plugin", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, MultipleInputs) {
  // Test parsing multiple input files.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {}\n");

  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto bar.proto");

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
}

TEST_F(CommandLineInterfaceTest, MultipleInputs_DescriptorSetIn) {
  // Test parsing multiple input files.
  FileDescriptorSet file_descriptor_set;

  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("bar.proto");
  file_descriptor_proto->add_message_type()->set_name("Bar");

  WriteDescriptorSet("foo.bin", &file_descriptor_set);

  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto bar.proto");

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
}

TEST_F(CommandLineInterfaceTest, MultipleInputsWithImport) {
  // Test parsing multiple input files with an import of a separate file.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"baz.proto\";\n"
                 "message Bar {\n"
                 "  optional Baz a = 1;\n"
                 "}\n");
  CreateTempFile("baz.proto",
                 "syntax = \"proto2\";\n"
                 "message Baz {}\n");

  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto bar.proto");

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
}

TEST_F(CommandLineInterfaceTest, MultipleInputsWithImport_DescriptorSetIn) {
  // Test parsing multiple input files with an import of a separate file.
  FileDescriptorSet file_descriptor_set;

  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("bar.proto");
  file_descriptor_proto->add_dependency("baz.proto");
  DescriptorProto* message = file_descriptor_proto->add_message_type();
  message->set_name("Bar");
  FieldDescriptorProto* field = message->add_field();
  field->set_type_name("Baz");
  field->set_name("a");
  field->set_number(1);

  WriteDescriptorSet("foo_and_bar.bin", &file_descriptor_set);

  file_descriptor_set.clear_file();
  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("baz.proto");
  file_descriptor_proto->add_message_type()->set_name("Baz");

  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("bat.proto");
  file_descriptor_proto->add_dependency("baz.proto");
  message = file_descriptor_proto->add_message_type();
  message->set_name("Bat");
  field = message->add_field();
  field->set_type_name("Baz");
  field->set_name("a");
  field->set_number(1);

  WriteDescriptorSet("baz_and_bat.bin", &file_descriptor_set);
  Run(strings::Substitute(
      "protocol_compiler --test_out=$$tmpdir --plug_out=$$tmpdir "
      "--descriptor_set_in=$0 foo.proto bar.proto",
      std::string("$tmpdir/foo_and_bar.bin") +
          CommandLineInterface::kPathSeparator + "$tmpdir/baz_and_bat.bin"));

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");

  Run(strings::Substitute(
      "protocol_compiler --test_out=$$tmpdir --plug_out=$$tmpdir "
      "--descriptor_set_in=$0 baz.proto bat.proto",
      std::string("$tmpdir/foo_and_bar.bin") +
          CommandLineInterface::kPathSeparator + "$tmpdir/baz_and_bat.bin"));

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "baz.proto,bat.proto",
                                    "baz.proto", "Baz");
  ExpectGeneratedWithMultipleInputs("test_generator", "baz.proto,bat.proto",
                                    "bat.proto", "Bat");
  ExpectGeneratedWithMultipleInputs("test_plugin", "baz.proto,bat.proto",
                                    "baz.proto", "Baz");
  ExpectGeneratedWithMultipleInputs("test_plugin", "baz.proto,bat.proto",
                                    "bat.proto", "Bat");
}

TEST_F(CommandLineInterfaceTest,
       MultipleInputsWithImport_DescriptorSetIn_DuplicateFileDescriptor) {
  // Test parsing multiple input files with an import of a separate file.
  FileDescriptorSet file_descriptor_set;

  FileDescriptorProto foo_file_descriptor_proto;
  foo_file_descriptor_proto.set_name("foo.proto");
  foo_file_descriptor_proto.add_message_type()->set_name("Foo");

  file_descriptor_set.add_file()->CopyFrom(foo_file_descriptor_proto);

  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("bar.proto");
  file_descriptor_proto->add_dependency("baz.proto");
  file_descriptor_proto->add_dependency("foo.proto");
  DescriptorProto* message = file_descriptor_proto->add_message_type();
  message->set_name("Bar");
  FieldDescriptorProto* field = message->add_field();
  field->set_type_name("Baz");
  field->set_name("a");
  field->set_number(1);
  field = message->add_field();
  field->set_type_name("Foo");
  field->set_name("f");
  field->set_number(2);
  WriteDescriptorSet("foo_and_bar.bin", &file_descriptor_set);

  file_descriptor_set.clear_file();
  file_descriptor_set.add_file()->CopyFrom(foo_file_descriptor_proto);

  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("baz.proto");
  file_descriptor_proto->add_dependency("foo.proto");
  message = file_descriptor_proto->add_message_type();
  message->set_name("Baz");
  field = message->add_field();
  field->set_type_name("Foo");
  field->set_name("f");
  field->set_number(1);
  WriteDescriptorSet("foo_and_baz.bin", &file_descriptor_set);

  Run(strings::Substitute(
      "protocol_compiler --test_out=$$tmpdir --plug_out=$$tmpdir "
      "--descriptor_set_in=$0 bar.proto",
      std::string("$tmpdir/foo_and_bar.bin") +
          CommandLineInterface::kPathSeparator + "$tmpdir/foo_and_baz.bin"));

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "bar.proto", "Bar");
  ExpectGenerated("test_plugin", "", "bar.proto", "Bar");
}

TEST_F(CommandLineInterfaceTest,
       MultipleInputsWithImport_DescriptorSetIn_MissingImport) {
  // Test parsing multiple input files with an import of a separate file.
  FileDescriptorSet file_descriptor_set;

  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("bar.proto");
  file_descriptor_proto->add_dependency("baz.proto");
  DescriptorProto* message = file_descriptor_proto->add_message_type();
  message->set_name("Bar");
  FieldDescriptorProto* field = message->add_field();
  field->set_type_name("Baz");
  field->set_name("a");
  field->set_number(1);

  WriteDescriptorSet("foo_and_bar.bin", &file_descriptor_set);

  file_descriptor_set.clear_file();
  file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("baz.proto");
  file_descriptor_proto->add_message_type()->set_name("Baz");

  WriteDescriptorSet("baz.bin", &file_descriptor_set);
  Run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo_and_bar.bin "
      "foo.proto bar.proto");
  ExpectErrorSubstring(
      "bar.proto: Import \"baz.proto\" was not found or had errors.");
  ExpectErrorSubstring("bar.proto: \"Baz\" is not defined.");
}

TEST_F(CommandLineInterfaceTest, CreateDirectory) {
  // Test that when we output to a sub-directory, it is created.

  CreateTempFile("bar/baz/foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempDir("out");
  CreateTempDir("plugout");

  Run("protocol_compiler --test_out=$tmpdir/out --plug_out=$tmpdir/plugout "
      "--proto_path=$tmpdir bar/baz/foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "bar/baz/foo.proto", "Foo", "out");
  ExpectGenerated("test_plugin", "", "bar/baz/foo.proto", "Foo", "plugout");
}

TEST_F(CommandLineInterfaceTest, GeneratorParameters) {
  // Test that generator parameters are correctly parsed from the command line.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=TestParameter:$tmpdir "
      "--plug_out=TestPluginParameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "TestParameter", "foo.proto", "Foo");
  ExpectGenerated("test_plugin", "TestPluginParameter", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, ExtraGeneratorParameters) {
  // Test that generator parameters specified with the option flag are
  // correctly passed to the code generator.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  // Create the "a" and "b" sub-directories.
  CreateTempDir("a");
  CreateTempDir("b");

  Run("protocol_compiler "
      "--test_opt=foo1 "
      "--test_out=bar:$tmpdir/a "
      "--test_opt=foo2 "
      "--test_out=baz:$tmpdir/b "
      "--test_opt=foo3 "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "bar,foo1,foo2,foo3", "foo.proto", "Foo",
                  "a");
  ExpectGenerated("test_generator", "baz,foo1,foo2,foo3", "foo.proto", "Foo",
                  "b");
}

TEST_F(CommandLineInterfaceTest, ExtraPluginParameters) {
  // Test that generator parameters specified with the option flag are
  // correctly passed to the code generator.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  // Create the "a" and "b" sub-directories.
  CreateTempDir("a");
  CreateTempDir("b");

  Run("protocol_compiler "
      "--plug_opt=foo1 "
      "--plug_out=bar:$tmpdir/a "
      "--plug_opt=foo2 "
      "--plug_out=baz:$tmpdir/b "
      "--plug_opt=foo3 "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_plugin", "bar,foo1,foo2,foo3", "foo.proto", "Foo", "a");
  ExpectGenerated("test_plugin", "baz,foo1,foo2,foo3", "foo.proto", "Foo", "b");
}

TEST_F(CommandLineInterfaceTest, UnrecognizedExtraParameters) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--unknown_plug_a_opt=Foo "
      "--unknown_plug_b_opt=Bar "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("Unknown flag: --unknown_plug_a_opt");
  ExpectErrorSubstring("Unknown flag: --unknown_plug_b_opt");
}

TEST_F(CommandLineInterfaceTest, ExtraPluginParametersForOutParameters) {
  // This doesn't rely on the plugin having been registred and instead that
  // the existence of --[name]_out is enough to make the --[name]_opt valid.
  // However, running out of process plugins found via the search path (i.e. -
  // not pre registered with --plugin) isn't support in this test suite, so we
  // list the options pre/post the _out directive, and then include _opt that
  // will be unknown, and confirm the failure output is about the expected
  // unknown directive, which means the other were accepted.
  // NOTE: UnrecognizedExtraParameters confirms that if two unknown _opt
  // directives appear, they both are reported.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--xyz_opt=foo=bar --xyz_out=$tmpdir "
      "--abc_out=$tmpdir --abc_opt=foo=bar "
      "--unknown_plug_opt=Foo "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorText("Unknown flag: --unknown_plug_opt\n");
}

TEST_F(CommandLineInterfaceTest, Insert) {
  // Test running a generator that inserts code into another's output.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler "
      "--test_out=TestParameter:$tmpdir "
      "--plug_out=TestPluginParameter:$tmpdir "
      "--test_out=insert=test_generator,test_plugin:$tmpdir "
      "--plug_out=insert=test_generator,test_plugin:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGeneratedWithInsertions("test_generator", "TestParameter",
                                "test_generator,test_plugin", "foo.proto",
                                "Foo");
  ExpectGeneratedWithInsertions("test_plugin", "TestPluginParameter",
                                "test_generator,test_plugin", "foo.proto",
                                "Foo");
}

TEST_F(CommandLineInterfaceTest, InsertWithAnnotationFixup) {
  // Check that annotation spans are updated after insertions.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_Annotate {}\n");

  Run("protocol_compiler "
      "--test_out=TestParameter:$tmpdir "
      "--plug_out=TestPluginParameter:$tmpdir "
      "--test_out=insert=test_generator,test_plugin:$tmpdir "
      "--plug_out=insert=test_generator,test_plugin:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  CheckGeneratedAnnotations("test_generator", "foo.proto");
  CheckGeneratedAnnotations("test_plugin", "foo.proto");
}

#if defined(_WIN32)

TEST_F(CommandLineInterfaceTest, WindowsOutputPath) {
  // Test that the output path can be a Windows-style path.

  CreateTempFile("foo.proto", "syntax = \"proto2\";\n");

  Run("protocol_compiler --null_out=C:\\ "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectNullCodeGeneratorCalled("");
}

TEST_F(CommandLineInterfaceTest, WindowsOutputPathAndParameter) {
  // Test that we can have a windows-style output path and a parameter.

  CreateTempFile("foo.proto", "syntax = \"proto2\";\n");

  Run("protocol_compiler --null_out=bar:C:\\ "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectNullCodeGeneratorCalled("bar");
}

TEST_F(CommandLineInterfaceTest, TrailingBackslash) {
  // Test that the directories can end in backslashes.  Some users claim this
  // doesn't work on their system.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir\\ "
      "--proto_path=$tmpdir\\ foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, Win32ErrorMessage) {
  EXPECT_EQ("The system cannot find the file specified.\r\n",
            Subprocess::Win32ErrorMessage(ERROR_FILE_NOT_FOUND));
}

#endif  // defined(_WIN32) || defined(__CYGWIN__)

TEST_F(CommandLineInterfaceTest, PathLookup) {
  // Test that specifying multiple directories in the proto search path works.

  CreateTempFile("b/bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {}\n");
  CreateTempFile("a/foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "message Foo {\n"
                 "  optional Bar a = 1;\n"
                 "}\n");
  CreateTempFile("b/foo.proto", "this should not be parsed\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/a --proto_path=$tmpdir/b foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, ColonDelimitedPath) {
  // Same as PathLookup, but we provide the proto_path in a single flag.

  CreateTempFile("b/bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {}\n");
  CreateTempFile("a/foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "message Foo {\n"
                 "  optional Bar a = 1;\n"
                 "}\n");
  CreateTempFile("b/foo.proto", "this should not be parsed\n");

  Run(strings::Substitute(
      "protocol_compiler --test_out=$$tmpdir --proto_path=$0 foo.proto",
      std::string("$tmpdir/a") + CommandLineInterface::kPathSeparator +
          "$tmpdir/b"));

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, NonRootMapping) {
  // Test setting up a search path mapping a directory to a non-root location.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=bar=$tmpdir bar/foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "bar/foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, PathWithEqualsSign) {
  // Test setting up a search path which happens to have '=' in it.

  CreateTempDir("with=sign");
  CreateTempFile("with=sign/foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/with=sign foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, MultipleGenerators) {
  // Test that we can have multiple generators and use both in one invocation,
  // each with a different output directory.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  // Create the "a" and "b" sub-directories.
  CreateTempDir("a");
  CreateTempDir("b");

  Run("protocol_compiler "
      "--test_out=$tmpdir/a "
      "--alt_out=$tmpdir/b "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo", "a");
  ExpectGenerated("alt_generator", "", "foo.proto", "Foo", "b");
}

TEST_F(CommandLineInterfaceTest, DisallowServicesNoServices) {
  // Test that --disallow_services doesn't cause a problem when there are no
  // services.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --disallow_services --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, DisallowServicesHasService) {
  // Test that --disallow_services produces an error when there are services.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n"
                 "service Bar {}\n");

  Run("protocol_compiler --disallow_services --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("foo.proto: This file contains services");
}

TEST_F(CommandLineInterfaceTest, AllowServicesHasService) {
  // Test that services work fine as long as --disallow_services is not used.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n"
                 "service Bar {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_Missing_EmptyList) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "message Foo { optional Bar bar = 1; }");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar { optional string text = 1; }");

  Run("protocol_compiler --test_out=$tmpdir --proto_path=$tmpdir "
      "--direct_dependencies= foo.proto");

  ExpectErrorText(
      "foo.proto: File is imported but not declared in --direct_dependencies: "
      "bar.proto\n");
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_Missing) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "import \"bla.proto\";\n"
                 "message Foo { optional Bar bar = 1; optional Bla bla = 2; }");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar { optional string text = 1; }");
  CreateTempFile("bla.proto",
                 "syntax = \"proto2\";\n"
                 "message Bla { optional int64 number = 1; }");

  Run("protocol_compiler --test_out=$tmpdir --proto_path=$tmpdir "
      "--direct_dependencies=bla.proto foo.proto");

  ExpectErrorText(
      "foo.proto: File is imported but not declared in --direct_dependencies: "
      "bar.proto\n");
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_NoViolation) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "message Foo { optional Bar bar = 1; }");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar { optional string text = 1; }");

  Run("protocol_compiler --test_out=$tmpdir --proto_path=$tmpdir "
      "--direct_dependencies=bar.proto foo.proto");

  ExpectNoErrors();
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_NoViolation_MultiImports) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "import \"bla.proto\";\n"
                 "message Foo { optional Bar bar = 1; optional Bla bla = 2; }");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar { optional string text = 1; }");
  CreateTempFile("bla.proto",
                 "syntax = \"proto2\";\n"
                 "message Bla { optional int64 number = 1; }");

  Run("protocol_compiler --test_out=$tmpdir --proto_path=$tmpdir "
      "--direct_dependencies=bar.proto:bla.proto foo.proto");

  ExpectNoErrors();
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_ProvidedMultipleTimes) {
  CreateTempFile("foo.proto", "syntax = \"proto2\";\n");

  Run("protocol_compiler --test_out=$tmpdir --proto_path=$tmpdir "
      "--direct_dependencies=bar.proto --direct_dependencies=bla.proto "
      "foo.proto");

  ExpectErrorText(
      "--direct_dependencies may only be passed once. To specify multiple "
      "direct dependencies, pass them all as a single parameter separated by "
      "':'.\n");
}

TEST_F(CommandLineInterfaceTest, DirectDependencies_CustomErrorMessage) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "message Foo { optional Bar bar = 1; }");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar { optional string text = 1; }");

  std::vector<std::string> commands;
  commands.push_back("protocol_compiler");
  commands.push_back("--test_out=$tmpdir");
  commands.push_back("--proto_path=$tmpdir");
  commands.push_back("--direct_dependencies=");
  commands.push_back("--direct_dependencies_violation_msg=Bla \"%s\" Bla");
  commands.push_back("foo.proto");
  RunWithArgs(commands);

  ExpectErrorText("foo.proto: Bla \"bar.proto\" Bla\n");
}

TEST_F(CommandLineInterfaceTest, CwdRelativeInputs) {
  // Test that we can accept working-directory-relative input files.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir $tmpdir/foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, WriteDescriptorSet) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--proto_path=$tmpdir bar.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);
  if (HasFatalFailure()) return;
  EXPECT_EQ(1, descriptor_set.file_size());
  EXPECT_EQ("bar.proto", descriptor_set.file(0).name());
  // Descriptor set should not have source code info.
  EXPECT_FALSE(descriptor_set.file(0).has_source_code_info());
  // Descriptor set should have json_name.
  EXPECT_EQ("Bar", descriptor_set.file(0).message_type(0).name());
  EXPECT_EQ("foo", descriptor_set.file(0).message_type(0).field(0).name());
  EXPECT_TRUE(descriptor_set.file(0).message_type(0).field(0).has_json_name());
}

TEST_F(CommandLineInterfaceTest, WriteDescriptorSetWithDuplicates) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");
  CreateTempFile("baz.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Baz {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--proto_path=$tmpdir bar.proto foo.proto bar.proto baz.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);
  if (HasFatalFailure()) return;
  EXPECT_EQ(3, descriptor_set.file_size());
  // foo should come first since the output is in dependency order.
  // since bar and baz are unordered, they should be in command line order.
  EXPECT_EQ("foo.proto", descriptor_set.file(0).name());
  EXPECT_EQ("bar.proto", descriptor_set.file(1).name());
  EXPECT_EQ("baz.proto", descriptor_set.file(2).name());
  // Descriptor set should not have source code info.
  EXPECT_FALSE(descriptor_set.file(0).has_source_code_info());
  // Descriptor set should have json_name.
  EXPECT_EQ("Bar", descriptor_set.file(1).message_type(0).name());
  EXPECT_EQ("foo", descriptor_set.file(1).message_type(0).field(0).name());
  EXPECT_TRUE(descriptor_set.file(1).message_type(0).field(0).has_json_name());
}

TEST_F(CommandLineInterfaceTest, WriteDescriptorSetWithSourceInfo) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_source_info --proto_path=$tmpdir bar.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);
  if (HasFatalFailure()) return;
  EXPECT_EQ(1, descriptor_set.file_size());
  EXPECT_EQ("bar.proto", descriptor_set.file(0).name());
  // Source code info included.
  EXPECT_TRUE(descriptor_set.file(0).has_source_code_info());
}

TEST_F(CommandLineInterfaceTest, WriteTransitiveDescriptorSet) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_imports --proto_path=$tmpdir bar.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);
  if (HasFatalFailure()) return;
  EXPECT_EQ(2, descriptor_set.file_size());
  if (descriptor_set.file(0).name() == "bar.proto") {
    std::swap(descriptor_set.mutable_file()->mutable_data()[0],
              descriptor_set.mutable_file()->mutable_data()[1]);
  }
  EXPECT_EQ("foo.proto", descriptor_set.file(0).name());
  EXPECT_EQ("bar.proto", descriptor_set.file(1).name());
  // Descriptor set should not have source code info.
  EXPECT_FALSE(descriptor_set.file(0).has_source_code_info());
  EXPECT_FALSE(descriptor_set.file(1).has_source_code_info());
}

TEST_F(CommandLineInterfaceTest, WriteTransitiveDescriptorSetWithSourceInfo) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_imports --include_source_info --proto_path=$tmpdir bar.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);
  if (HasFatalFailure()) return;
  EXPECT_EQ(2, descriptor_set.file_size());
  if (descriptor_set.file(0).name() == "bar.proto") {
    std::swap(descriptor_set.mutable_file()->mutable_data()[0],
              descriptor_set.mutable_file()->mutable_data()[1]);
  }
  EXPECT_EQ("foo.proto", descriptor_set.file(0).name());
  EXPECT_EQ("bar.proto", descriptor_set.file(1).name());
  // Source code info included.
  EXPECT_TRUE(descriptor_set.file(0).has_source_code_info());
  EXPECT_TRUE(descriptor_set.file(1).has_source_code_info());
}

#ifdef _WIN32
// TODO(teboring): Figure out how to write test on windows.
#else
TEST_F(CommandLineInterfaceTest, WriteDependencyManifestFileGivenTwoInputs) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --dependency_out=$tmpdir/manifest "
      "--test_out=$tmpdir --proto_path=$tmpdir bar.proto foo.proto");

  ExpectErrorText(
      "Can only process one input file when using --dependency_out=FILE.\n");
}

#ifdef PROTOBUF_OPENSOURCE
TEST_F(CommandLineInterfaceTest, WriteDependencyManifestFile) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  std::string current_working_directory = getcwd(NULL, 0);
  SwitchToTempDirectory();

  Run("protocol_compiler --dependency_out=manifest --test_out=. "
      "bar.proto");

  ExpectNoErrors();

  ExpectFileContent("manifest",
                    "bar.proto.MockCodeGenerator.test_generator: "
                    "foo.proto\\\n bar.proto");

  File::ChangeWorkingDirectory(current_working_directory);
}
#else  // !PROTOBUF_OPENSOURCE
// TODO(teboring): Figure out how to change and get working directory in
// google3.
#endif  // !PROTOBUF_OPENSOURCE

TEST_F(CommandLineInterfaceTest, WriteDependencyManifestFileForAbsolutePath) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n"
                 "message Bar {\n"
                 "  optional Foo foo = 1;\n"
                 "}\n");

  Run("protocol_compiler --dependency_out=$tmpdir/manifest "
      "--test_out=$tmpdir --proto_path=$tmpdir bar.proto");

  ExpectNoErrors();

  ExpectFileContent("manifest",
                    "$tmpdir/bar.proto.MockCodeGenerator.test_generator: "
                    "$tmpdir/foo.proto\\\n $tmpdir/bar.proto");
}
#endif  // !_WIN32

TEST_F(CommandLineInterfaceTest, TestArgumentFile) {
  // Test parsing multiple input files using an argument file.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {}\n");
  CreateTempFile("arguments.txt",
                 "--test_out=$tmpdir\n"
                 "--plug_out=$tmpdir\n"
                 "--proto_path=$tmpdir\n"
                 "--direct_dependencies_violation_msg=%s is not imported\n"
                 "foo.proto\n"
                 "bar.proto");

  Run("protocol_compiler @$tmpdir/arguments.txt");

  ExpectNoErrors();
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "Foo");
  ExpectGeneratedWithMultipleInputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "Bar");
}


// -------------------------------------------------------------------

TEST_F(CommandLineInterfaceTest, ParseErrors) {
  // Test that parse errors are reported.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "badsyntax\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorText(
      "foo.proto:2:1: Expected top-level statement (e.g. \"message\").\n");
}

TEST_F(CommandLineInterfaceTest, ParseErrors_DescriptorSetIn) {
  // Test that parse errors are reported.
  CreateTempFile("foo.bin", "not a FileDescriptorSet");

  Run("protocol_compiler --test_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");

  ExpectErrorText("$tmpdir/foo.bin: Unable to parse.\n");
}

TEST_F(CommandLineInterfaceTest, ParseErrorsMultipleFiles) {
  // Test that parse errors are reported from multiple files.

  // We set up files such that foo.proto actually depends on bar.proto in
  // two ways:  Directly and through baz.proto.  bar.proto's errors should
  // only be reported once.
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "badsyntax\n");
  CreateTempFile("baz.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n");
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"bar.proto\";\n"
                 "import \"baz.proto\";\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorText(
      "bar.proto:2:1: Expected top-level statement (e.g. \"message\").\n"
      "baz.proto:2:1: Import \"bar.proto\" was not found or had errors.\n"
      "foo.proto:2:1: Import \"bar.proto\" was not found or had errors.\n"
      "foo.proto:3:1: Import \"baz.proto\" was not found or had errors.\n");
}

TEST_F(CommandLineInterfaceTest, RecursiveImportFails) {
  // Create a proto file that imports itself.
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "import \"foo.proto\";\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "foo.proto:2:1: File recursively imports itself: "
      "foo.proto -> foo.proto\n");
}

TEST_F(CommandLineInterfaceTest, InputNotFoundError) {
  // Test what happens if the input file is not found.

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorText("foo.proto: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, InputNotFoundError_DescriptorSetIn) {
  // Test what happens if the input file is not found.

  Run("protocol_compiler --test_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");

  ExpectErrorText("$tmpdir/foo.bin: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, CwdRelativeInputNotFoundError) {
  // Test what happens when a working-directory-relative input file is not
  // found.

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir $tmpdir/foo.proto");

  ExpectErrorText("$tmpdir/foo.proto: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, CwdRelativeInputNotMappedError) {
  // Test what happens when a working-directory-relative input file is not
  // mapped to a virtual path.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  // Create a directory called "bar" so that we can point --proto_path at it.
  CreateTempFile("bar/dummy", "");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/bar $tmpdir/foo.proto");

  ExpectErrorText(
      "$tmpdir/foo.proto: File does not reside within any path "
      "specified using --proto_path (or -I).  You must specify a "
      "--proto_path which encompasses this file.  Note that the "
      "proto_path must be an exact prefix of the .proto file "
      "names -- protoc is too dumb to figure out when two paths "
      "(e.g. absolute and relative) are equivalent (it's harder "
      "than you think).\n");
}

TEST_F(CommandLineInterfaceTest, CwdRelativeInputNotFoundAndNotMappedError) {
  // Check what happens if the input file is not found *and* is not mapped
  // in the proto_path.

  // Create a directory called "bar" so that we can point --proto_path at it.
  CreateTempFile("bar/dummy", "");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/bar $tmpdir/foo.proto");

  ExpectErrorText("$tmpdir/foo.proto: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, CwdRelativeInputShadowedError) {
  // Test what happens when a working-directory-relative input file is shadowed
  // by another file in the virtual path.

  CreateTempFile("foo/foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");
  CreateTempFile("bar/foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/foo --proto_path=$tmpdir/bar "
      "$tmpdir/bar/foo.proto");

  ExpectErrorText(
      "$tmpdir/bar/foo.proto: Input is shadowed in the --proto_path "
      "by \"$tmpdir/foo/foo.proto\".  Either use the latter "
      "file as your input or reorder the --proto_path so that the "
      "former file's location comes first.\n");
}

TEST_F(CommandLineInterfaceTest, ProtoPathNotFoundError) {
  // Test what happens if the input file is not found.

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/foo foo.proto");

  ExpectErrorText(
      "$tmpdir/foo: warning: directory does not exist.\n"
      "foo.proto: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, ProtoPathAndDescriptorSetIn) {
  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --descriptor_set_in=$tmpdir/foo.bin foo.proto");
  ExpectErrorText("$tmpdir/foo.bin: No such file or directory\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin --proto_path=$tmpdir foo.proto");
  ExpectErrorText("$tmpdir/foo.bin: No such file or directory\n");
}

TEST_F(CommandLineInterfaceTest, ProtoPathAndDescriptorSetIn_CompileFiles) {
  // Test what happens if a proto is in a --descriptor_set_in and also exists
  // on disk.
  FileDescriptorSet file_descriptor_set;

  // NOTE: This file desc SHOULD be different from the one created as a temp
  //       to make it easier to test that the file was output instead of the
  //       contents of the --descriptor_set_in file.
  FileDescriptorProto* file_descriptor_proto = file_descriptor_set.add_file();
  file_descriptor_proto->set_name("foo.proto");
  file_descriptor_proto->add_message_type()->set_name("Foo");

  WriteDescriptorSet("foo.bin", &file_descriptor_set);

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message FooBar { required string foo_message = 1; }\n");

  Run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--descriptor_set_in=$tmpdir/foo.bin "
      "--include_source_info "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();

  FileDescriptorSet descriptor_set;
  ReadDescriptorSet("descriptor_set", &descriptor_set);

  EXPECT_EQ(1, descriptor_set.file_size());
  EXPECT_EQ("foo.proto", descriptor_set.file(0).name());
  // Descriptor set SHOULD have source code info.
  EXPECT_TRUE(descriptor_set.file(0).has_source_code_info());

  EXPECT_EQ("FooBar", descriptor_set.file(0).message_type(0).name());
  EXPECT_EQ("foo_message",
            descriptor_set.file(0).message_type(0).field(0).name());
}

TEST_F(CommandLineInterfaceTest, ProtoPathAndDependencyOut) {
  Run("protocol_compiler --test_out=$tmpdir "
      "--dependency_out=$tmpdir/manifest "
      "--descriptor_set_in=$tmpdir/foo.bin foo.proto");
  ExpectErrorText(
      "--descriptor_set_in cannot be used with --dependency_out.\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--descriptor_set_in=$tmpdir/foo.bin "
      "--dependency_out=$tmpdir/manifest foo.proto");
  ExpectErrorText(
      "--dependency_out cannot be used with --descriptor_set_in.\n");
}

TEST_F(CommandLineInterfaceTest, MissingInputError) {
  // Test that we get an error if no inputs are given.

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir");

  ExpectErrorText("Missing input file.\n");
}

TEST_F(CommandLineInterfaceTest, MissingOutputError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --proto_path=$tmpdir foo.proto");

  ExpectErrorText("Missing output directives.\n");
}

TEST_F(CommandLineInterfaceTest, OutputWriteError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  std::string output_file =
      MockCodeGenerator::GetOutputFileName("test_generator", "foo.proto");

  // Create a directory blocking our output location.
  CreateTempDir(output_file);

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  // MockCodeGenerator no longer detects an error because we actually write to
  // an in-memory location first, then dump to disk at the end.  This is no
  // big deal.
  //   ExpectErrorSubstring("MockCodeGenerator detected write error.");

#if defined(_WIN32) && !defined(__CYGWIN__)
  // Windows with MSVCRT.dll produces EPERM instead of EISDIR.
  if (HasAlternateErrorSubstring(output_file + ": Permission denied")) {
    return;
  }
#endif

  ExpectErrorSubstring(output_file + ": Is a directory");
}

TEST_F(CommandLineInterfaceTest, PluginOutputWriteError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  std::string output_file =
      MockCodeGenerator::GetOutputFileName("test_plugin", "foo.proto");

  // Create a directory blocking our output location.
  CreateTempDir(output_file);

  Run("protocol_compiler --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

#if defined(_WIN32) && !defined(__CYGWIN__)
  // Windows with MSVCRT.dll produces EPERM instead of EISDIR.
  if (HasAlternateErrorSubstring(output_file + ": Permission denied")) {
    return;
  }
#endif

  ExpectErrorSubstring(output_file + ": Is a directory");
}

TEST_F(CommandLineInterfaceTest, OutputDirectoryNotFoundError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir/nosuchdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("nosuchdir/: No such file or directory");
}

TEST_F(CommandLineInterfaceTest, PluginOutputDirectoryNotFoundError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --plug_out=$tmpdir/nosuchdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("nosuchdir/: No such file or directory");
}

TEST_F(CommandLineInterfaceTest, OutputDirectoryIsFileError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out=$tmpdir/foo.proto "
      "--proto_path=$tmpdir foo.proto");

#if defined(_WIN32) && !defined(__CYGWIN__)
  // Windows with MSVCRT.dll produces EINVAL instead of ENOTDIR.
  if (HasAlternateErrorSubstring("foo.proto/: Invalid argument")) {
    return;
  }
#endif

  ExpectErrorSubstring("foo.proto/: Not a directory");
}

TEST_F(CommandLineInterfaceTest, GeneratorError) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_Error {}\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "--test_out: foo.proto: Saw message type MockCodeGenerator_Error.");
}

TEST_F(CommandLineInterfaceTest, GeneratorPluginError) {
  // Test a generator plugin that returns an error.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_Error {}\n");

  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "--plug_out: foo.proto: Saw message type MockCodeGenerator_Error.");
}

TEST_F(CommandLineInterfaceTest, GeneratorPluginFail) {
  // Test a generator plugin that exits with an error code.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_Exit {}\n");

  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("Saw message type MockCodeGenerator_Exit.");
  ExpectErrorSubstring(
      "--plug_out: prefix-gen-plug: Plugin failed with status code 123.");
}

TEST_F(CommandLineInterfaceTest, GeneratorPluginCrash) {
  // Test a generator plugin that crashes.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_Abort {}\n");

  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("Saw message type MockCodeGenerator_Abort.");

#ifdef _WIN32
  // Windows doesn't have signals.  It looks like abort()ing causes the process
  // to exit with status code 3, but let's not depend on the exact number here.
  ExpectErrorSubstring(
      "--plug_out: prefix-gen-plug: Plugin failed with status code");
#else
  // Don't depend on the exact signal number.
  ExpectErrorSubstring("--plug_out: prefix-gen-plug: Plugin killed by signal");
#endif
}

TEST_F(CommandLineInterfaceTest, PluginReceivesSourceCodeInfo) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_HasSourceCodeInfo {}\n");

  Run("protocol_compiler --plug_out=$tmpdir --proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring(
      "Saw message type MockCodeGenerator_HasSourceCodeInfo: 1.");
}

TEST_F(CommandLineInterfaceTest, PluginReceivesJsonName) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_HasJsonName {\n"
                 "  optional int32 value = 1;\n"
                 "}\n");

  Run("protocol_compiler --plug_out=$tmpdir --proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring("Saw json_name: 1");
}

TEST_F(CommandLineInterfaceTest, PluginReceivesCompilerVersion) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message MockCodeGenerator_ShowVersionNumber {\n"
                 "  optional int32 value = 1;\n"
                 "}\n");

  Run("protocol_compiler --plug_out=$tmpdir --proto_path=$tmpdir foo.proto");

  ExpectErrorSubstring(StringPrintf("Saw compiler_version: %d %s",
                                    GOOGLE_PROTOBUF_VERSION,
                                    GOOGLE_PROTOBUF_VERSION_SUFFIX));
}

TEST_F(CommandLineInterfaceTest, GeneratorPluginNotFound) {
  // Test what happens if the plugin isn't found.

  CreateTempFile("error.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --badplug_out=TestParameter:$tmpdir "
      "--plugin=prefix-gen-badplug=no_such_file "
      "--proto_path=$tmpdir error.proto");

#ifdef _WIN32
  ExpectErrorSubstring("--badplug_out: prefix-gen-badplug: " +
                       Subprocess::Win32ErrorMessage(ERROR_FILE_NOT_FOUND));
#else
  // Error written to stdout by child process after exec() fails.
  ExpectErrorSubstring("no_such_file: program not found or is not executable");

  ExpectErrorSubstring(
      "Please specify a program using absolute path or make sure "
      "the program is available in your PATH system variable");

  // Error written by parent process when child fails.
  ExpectErrorSubstring(
      "--badplug_out: prefix-gen-badplug: Plugin failed with status code 1.");
#endif
}

TEST_F(CommandLineInterfaceTest, GeneratorPluginNotAllowed) {
  // Test what happens if plugins aren't allowed.

  CreateTempFile("error.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  DisallowPlugins();
  Run("protocol_compiler --plug_out=TestParameter:$tmpdir "
      "--proto_path=$tmpdir error.proto");

  ExpectErrorSubstring("Unknown flag: --plug_out");
}

TEST_F(CommandLineInterfaceTest, HelpText) {
  Run("test_exec_name --help");

  ExpectCapturedStdoutSubstringWithZeroReturnCode("Usage: test_exec_name ");
  ExpectCapturedStdoutSubstringWithZeroReturnCode("--test_out=OUT_DIR");
  ExpectCapturedStdoutSubstringWithZeroReturnCode("Test output.");
  ExpectCapturedStdoutSubstringWithZeroReturnCode("--alt_out=OUT_DIR");
  ExpectCapturedStdoutSubstringWithZeroReturnCode("Alt output.");
}

TEST_F(CommandLineInterfaceTest, GccFormatErrors) {
  // Test --error_format=gcc (which is the default, but we want to verify
  // that it can be set explicitly).

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "badsyntax\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=gcc foo.proto");

  ExpectErrorText(
      "foo.proto:2:1: Expected top-level statement (e.g. \"message\").\n");
}

TEST_F(CommandLineInterfaceTest, MsvsFormatErrors) {
  // Test --error_format=msvs

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "badsyntax\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=msvs foo.proto");

  ExpectErrorText(
      "$tmpdir/foo.proto(2) : error in column=1: Expected top-level statement "
      "(e.g. \"message\").\n");
}

TEST_F(CommandLineInterfaceTest, InvalidErrorFormat) {
  // Test --error_format=msvs

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "badsyntax\n");

  Run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=invalid foo.proto");

  ExpectErrorText("Unknown error format: invalid\n");
}

// -------------------------------------------------------------------
// Flag parsing tests

TEST_F(CommandLineInterfaceTest, ParseSingleCharacterFlag) {
  // Test that a single-character flag works.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler -t$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, ParseSpaceDelimitedValue) {
  // Test that separating the flag value with a space works.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler --test_out $tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, ParseSingleCharacterSpaceDelimitedValue) {
  // Test that separating the flag value with a space works for
  // single-character flags.

  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "message Foo {}\n");

  Run("protocol_compiler -t $tmpdir "
      "--proto_path=$tmpdir foo.proto");

  ExpectNoErrors();
  ExpectGenerated("test_generator", "", "foo.proto", "Foo");
}

TEST_F(CommandLineInterfaceTest, MissingValueError) {
  // Test that we get an error if a flag is missing its value.

  Run("protocol_compiler --test_out --proto_path=$tmpdir foo.proto");

  ExpectErrorText("Missing value for flag: --test_out\n");
}

TEST_F(CommandLineInterfaceTest, MissingValueAtEndError) {
  // Test that we get an error if the last argument is a flag requiring a
  // value.

  Run("protocol_compiler --test_out");

  ExpectErrorText("Missing value for flag: --test_out\n");
}

TEST_F(CommandLineInterfaceTest, PrintFreeFieldNumbers) {
  CreateTempFile("foo.proto",
                 "syntax = \"proto2\";\n"
                 "package foo;\n"
                 "message Foo {\n"
                 "  optional int32 a = 2;\n"
                 "  optional string b = 4;\n"
                 "  optional string c = 5;\n"
                 "  optional int64 d = 8;\n"
                 "  optional double e = 10;\n"
                 "}\n");
  CreateTempFile("bar.proto",
                 "syntax = \"proto2\";\n"
                 "message Bar {\n"
                 "  optional int32 a = 2;\n"
                 "  extensions 4 to 5;\n"
                 "  optional int64 d = 8;\n"
                 "  extensions 10;\n"
                 "}\n");
  CreateTempFile("baz.proto",
                 "syntax = \"proto2\";\n"
                 "message Baz {\n"
                 "  optional int32 a = 2;\n"
                 "  optional int64 d = 8;\n"
                 "  extensions 15 to max;\n"  // unordered.
                 "  extensions 13;\n"
                 "  extensions 10 to 12;\n"
                 "  extensions 5;\n"
                 "  extensions 4;\n"
                 "}\n");
  CreateTempFile(
      "quz.proto",
      "syntax = \"proto2\";\n"
      "message Quz {\n"
      "  message Foo {}\n"  // nested message
      "  optional int32 a = 2;\n"
      "  optional group C = 4 {\n"
      "    optional int32 d = 5;\n"
      "  }\n"
      "  extensions 8 to 10;\n"
      "  optional group E = 11 {\n"
      "    optional int32 f = 9;\n"    // explicitly reuse extension range 8-10
      "    optional group G = 15 {\n"  // nested group
      "      message Foo {}\n"         // nested message inside nested group
      "    }\n"
      "  }\n"
      "}\n");

  Run("protocol_compiler --print_free_field_numbers --proto_path=$tmpdir "
      "foo.proto bar.proto baz.proto quz.proto");

  ExpectNoErrors();

  // TODO(jieluo): Cygwin doesn't work well if we try to capture stderr and
  // stdout at the same time. Need to figure out why and add this test back
  // for Cygwin.
#if !defined(__CYGWIN__)
  ExpectCapturedStdout(
      "foo.Foo                             free: 1 3 6-7 9 11-INF\n"
      "Bar                                 free: 1 3 6-7 9 11-INF\n"
      "Baz                                 free: 1 3 6-7 9 14\n"
      "Quz.Foo                             free: 1-INF\n"
      "Quz.E.G.Foo                         free: 1-INF\n"
      "Quz                                 free: 1 3 6-7 12-14 16-INF\n");
#endif
}

// ===================================================================

// Test for --encode and --decode.  Note that it would be easier to do this
// test as a shell script, but we'd like to be able to run the test on
// platforms that don't have a Bourne-compatible shell available (especially
// Windows/MSVC).

enum EncodeDecodeTestMode { PROTO_PATH, DESCRIPTOR_SET_IN };

class EncodeDecodeTest : public testing::TestWithParam<EncodeDecodeTestMode> {
 protected:
  virtual void SetUp() {
    WriteUnittestProtoDescriptorSet();
    duped_stdin_ = dup(STDIN_FILENO);
  }

  virtual void TearDown() {
    dup2(duped_stdin_, STDIN_FILENO);
    close(duped_stdin_);
  }

  void RedirectStdinFromText(const std::string& input) {
    std::string filename = TestTempDir() + "/test_stdin";
    GOOGLE_CHECK_OK(File::SetContents(filename, input, true));
    GOOGLE_CHECK(RedirectStdinFromFile(filename));
  }

  bool RedirectStdinFromFile(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) return false;
    dup2(fd, STDIN_FILENO);
    close(fd);
    return true;
  }

  // Remove '\r' characters from text.
  std::string StripCR(const std::string& text) {
    std::string result;

    for (int i = 0; i < text.size(); i++) {
      if (text[i] != '\r') {
        result.push_back(text[i]);
      }
    }

    return result;
  }

  enum Type { TEXT, BINARY };
  enum ReturnCode { SUCCESS, ERROR };

  bool Run(const std::string& command) {
    std::vector<std::string> args;
    args.push_back("protoc");
    SplitStringUsing(command, " ", &args);
    switch (GetParam()) {
      case PROTO_PATH:
        args.push_back("--proto_path=" + TestUtil::TestSourceDir());
        break;
      case DESCRIPTOR_SET_IN:
        args.push_back(StrCat("--descriptor_set_in=",
                                    unittest_proto_descriptor_set_filename_));
        break;
      default:
        ADD_FAILURE() << "unexpected EncodeDecodeTestMode: " << GetParam();
    }

    std::unique_ptr<const char*[]> argv(new const char*[args.size()]);
    for (int i = 0; i < args.size(); i++) {
      argv[i] = args[i].c_str();
    }

    CommandLineInterface cli;

    CaptureTestStdout();
    CaptureTestStderr();

    int result = cli.Run(args.size(), argv.get());

    captured_stdout_ = GetCapturedTestStdout();
    captured_stderr_ = GetCapturedTestStderr();

    return result == 0;
  }

  void ExpectStdoutMatchesBinaryFile(const std::string& filename) {
    std::string expected_output;
    GOOGLE_CHECK_OK(File::GetContents(filename, &expected_output, true));

    // Don't use EXPECT_EQ because we don't want to print raw binary data to
    // stdout on failure.
    EXPECT_TRUE(captured_stdout_ == expected_output);
  }

  void ExpectStdoutMatchesTextFile(const std::string& filename) {
    std::string expected_output;
    GOOGLE_CHECK_OK(File::GetContents(filename, &expected_output, true));

    ExpectStdoutMatchesText(expected_output);
  }

  void ExpectStdoutMatchesText(const std::string& expected_text) {
    EXPECT_EQ(StripCR(expected_text), StripCR(captured_stdout_));
  }

  void ExpectStderrMatchesText(const std::string& expected_text) {
    EXPECT_EQ(StripCR(expected_text), StripCR(captured_stderr_));
  }

 private:
  void WriteUnittestProtoDescriptorSet() {
    unittest_proto_descriptor_set_filename_ =
        TestTempDir() + "/unittest_proto_descriptor_set.bin";
    FileDescriptorSet file_descriptor_set;
    protobuf_unittest::TestAllTypes test_all_types;
    test_all_types.descriptor()->file()->CopyTo(file_descriptor_set.add_file());

    protobuf_unittest_import::ImportMessage import_message;
    import_message.descriptor()->file()->CopyTo(file_descriptor_set.add_file());

    protobuf_unittest_import::PublicImportMessage public_import_message;
    public_import_message.descriptor()->file()->CopyTo(
        file_descriptor_set.add_file());
    GOOGLE_DCHECK(file_descriptor_set.IsInitialized());

    std::string binary_proto;
    GOOGLE_CHECK(file_descriptor_set.SerializeToString(&binary_proto));
    GOOGLE_CHECK_OK(File::SetContents(unittest_proto_descriptor_set_filename_,
                               binary_proto, true));
  }

  int duped_stdin_;
  std::string captured_stdout_;
  std::string captured_stderr_;
  std::string unittest_proto_descriptor_set_filename_;
};

TEST_P(EncodeDecodeTest, Encode) {
  RedirectStdinFromFile(TestUtil::GetTestDataPath(
      "net/proto2/internal/"
      "testdata/text_format_unittest_data_oneof_implemented.txt"));
  EXPECT_TRUE(
      Run(TestUtil::MaybeTranslatePath("net/proto2/internal/unittest.proto") +
          " --encode=protobuf_unittest.TestAllTypes"));
  ExpectStdoutMatchesBinaryFile(TestUtil::GetTestDataPath(
      "net/proto2/internal/testdata/golden_message_oneof_implemented"));
  ExpectStderrMatchesText("");
}

TEST_P(EncodeDecodeTest, Decode) {
  RedirectStdinFromFile(TestUtil::GetTestDataPath(
      "net/proto2/internal/testdata/golden_message_oneof_implemented"));
  EXPECT_TRUE(
      Run(TestUtil::MaybeTranslatePath("net/proto2/internal/unittest.proto") +
          " --decode=protobuf_unittest.TestAllTypes"));
  ExpectStdoutMatchesTextFile(TestUtil::GetTestDataPath(
      "net/proto2/internal/"
      "testdata/text_format_unittest_data_oneof_implemented.txt"));
  ExpectStderrMatchesText("");
}

TEST_P(EncodeDecodeTest, Partial) {
  RedirectStdinFromText("");
  EXPECT_TRUE(
      Run(TestUtil::MaybeTranslatePath("net/proto2/internal/unittest.proto") +
          " --encode=protobuf_unittest.TestRequired"));
  ExpectStdoutMatchesText("");
  ExpectStderrMatchesText(
      "warning:  Input message is missing required fields:  a, b, c\n");
}

TEST_P(EncodeDecodeTest, DecodeRaw) {
  protobuf_unittest::TestAllTypes message;
  message.set_optional_int32(123);
  message.set_optional_string("foo");
  std::string data;
  message.SerializeToString(&data);

  RedirectStdinFromText(data);
  EXPECT_TRUE(Run("--decode_raw"));
  ExpectStdoutMatchesText(
      "1: 123\n"
      "14: \"foo\"\n");
  ExpectStderrMatchesText("");
}

TEST_P(EncodeDecodeTest, UnknownType) {
  EXPECT_FALSE(
      Run(TestUtil::MaybeTranslatePath("net/proto2/internal/unittest.proto") +
          " --encode=NoSuchType"));
  ExpectStdoutMatchesText("");
  ExpectStderrMatchesText("Type not defined: NoSuchType\n");
}

TEST_P(EncodeDecodeTest, ProtoParseError) {
  EXPECT_FALSE(
      Run("net/proto2/internal/no_such_file.proto "
          "--encode=NoSuchType"));
  ExpectStdoutMatchesText("");
  ExpectStderrMatchesText(
      "net/proto2/internal/no_such_file.proto: No such file or directory\n");
}

INSTANTIATE_TEST_SUITE_P(FileDescriptorSetSource, EncodeDecodeTest,
                         testing::Values(PROTO_PATH, DESCRIPTOR_SET_IN));
}  // anonymous namespace

#endif  // !GOOGLE_PROTOBUF_HEAP_CHECK_DRACONIAN

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
