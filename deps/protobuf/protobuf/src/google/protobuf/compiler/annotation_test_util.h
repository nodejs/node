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

#ifndef GOOGLE_PROTOBUF_COMPILER_ANNOTATION_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_COMPILER_ANNOTATION_TEST_UTIL_H__

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

// Utilities that assist in writing tests for generator annotations.
// See java/internal/annotation_unittest.cc for an example.
namespace google {
namespace protobuf {
namespace compiler {
namespace annotation_test_util {

// Struct that contains the file generated from a .proto file and its
// GeneratedCodeInfo. For example, the Java generator will fill this struct
// (for some 'foo.proto') with:
//   file_path = "Foo.java"
//   file_content = content of Foo.java
//   file_info = parsed content of Foo.java.pb.meta
struct ExpectedOutput {
  std::string file_path;
  std::string file_content;
  GeneratedCodeInfo file_info;
  explicit ExpectedOutput(const std::string& file_path)
      : file_path(file_path) {}
};

// Creates a file with name `filename` and content `data` in temp test
// directory.
void AddFile(const std::string& filename, const std::string& data);

// Runs proto compiler. Captures proto file structrue in FileDescriptorProto.
// Files will be generated in TestTempDir() folder. Callers of this
// function must read generated files themselves.
//
// filename: source .proto file used to generate code.
// plugin_specific_args: command line arguments specific to current generator.
//     For Java, this value might be "--java_out=annotate_code:test_temp_dir"
// cli: instance of command line interface to run generator. See Java's
//     annotation_unittest.cc for an example of how to initialize it.
// file: output parameter, will be set to the descriptor of the proto file
//     specified in filename.
bool RunProtoCompiler(const std::string& filename,
                      const std::string& plugin_specific_args,
                      CommandLineInterface* cli, FileDescriptorProto* file);

bool DecodeMetadata(const std::string& path, GeneratedCodeInfo* info);

// Finds all of the Annotations for a given source file and path.
// See Location.path in http://google3/net/proto2/proto/descriptor.proto for
// explanation of what path vector is.
void FindAnnotationsOnPath(
    const GeneratedCodeInfo& info, const std::string& source_file,
    const std::vector<int>& path,
    std::vector<const GeneratedCodeInfo::Annotation*>* annotations);

// Finds the Annotation for a given source file and path (or returns null if it
// couldn't). If there are several annotations for given path, returns the first
// one. See Location.path in
// http://google3/net/proto2/proto/descriptor.proto for explanation of what path
// vector is.
const GeneratedCodeInfo::Annotation* FindAnnotationOnPath(
    const GeneratedCodeInfo& info, const std::string& source_file,
    const std::vector<int>& path);

// Returns true if at least one of the provided annotations covers a given
// substring in file_content.
bool AtLeastOneAnnotationMatchesSubstring(
    const std::string& file_content,
    const std::vector<const GeneratedCodeInfo::Annotation*>& annotations,
    const std::string& expected_text);

// Returns true if the provided annotation covers a given substring in
// file_content.
bool AnnotationMatchesSubstring(const std::string& file_content,
                                const GeneratedCodeInfo::Annotation* annotation,
                                const std::string& expected_text);

}  // namespace annotation_test_util
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_ANNOTATION_TEST_UTIL_H__
