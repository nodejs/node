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

#include <google/protobuf/compiler/annotation_test_util.h>

#include <memory>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace annotation_test_util {
namespace {

// A CodeGenerator that captures the FileDescriptor it's passed as a
// FileDescriptorProto.
class DescriptorCapturingGenerator : public CodeGenerator {
 public:
  // Does not own file; file must outlive the Generator.
  explicit DescriptorCapturingGenerator(FileDescriptorProto* file)
      : file_(file) {}

  virtual bool Generate(const FileDescriptor* file,
                        const std::string& parameter, GeneratorContext* context,
                        std::string* error) const {
    file->CopyTo(file_);
    return true;
  }

 private:
  FileDescriptorProto* file_;
};
}  // namespace

void AddFile(const std::string& filename, const std::string& data) {
  GOOGLE_CHECK_OK(File::SetContents(TestTempDir() + "/" + filename, data,
                             true));
}

bool RunProtoCompiler(const std::string& filename,
                      const std::string& plugin_specific_args,
                      CommandLineInterface* cli, FileDescriptorProto* file) {
  cli->SetInputsAreProtoPathRelative(true);

  DescriptorCapturingGenerator capturing_generator(file);
  cli->RegisterGenerator("--capture_out", &capturing_generator, "");

  std::string proto_path = "-I" + TestTempDir();
  std::string capture_out = "--capture_out=" + TestTempDir();

  const char* argv[] = {"protoc", proto_path.c_str(),
                        plugin_specific_args.c_str(), capture_out.c_str(),
                        filename.c_str()};

  return cli->Run(5, argv) == 0;
}

bool DecodeMetadata(const std::string& path, GeneratedCodeInfo* info) {
  std::string data;
  GOOGLE_CHECK_OK(File::GetContents(path, &data, true));
  io::ArrayInputStream input(data.data(), data.size());
  return info->ParseFromZeroCopyStream(&input);
}

void FindAnnotationsOnPath(
    const GeneratedCodeInfo& info, const std::string& source_file,
    const std::vector<int>& path,
    std::vector<const GeneratedCodeInfo::Annotation*>* annotations) {
  for (int i = 0; i < info.annotation_size(); ++i) {
    const GeneratedCodeInfo::Annotation* annotation = &info.annotation(i);
    if (annotation->source_file() != source_file ||
        annotation->path_size() != path.size()) {
      continue;
    }
    int node = 0;
    for (; node < path.size(); ++node) {
      if (annotation->path(node) != path[node]) {
        break;
      }
    }
    if (node == path.size()) {
      annotations->push_back(annotation);
    }
  }
}

const GeneratedCodeInfo::Annotation* FindAnnotationOnPath(
    const GeneratedCodeInfo& info, const std::string& source_file,
    const std::vector<int>& path) {
  std::vector<const GeneratedCodeInfo::Annotation*> annotations;
  FindAnnotationsOnPath(info, source_file, path, &annotations);
  if (annotations.empty()) {
    return NULL;
  }
  return annotations[0];
}

bool AtLeastOneAnnotationMatchesSubstring(
    const std::string& file_content,
    const std::vector<const GeneratedCodeInfo::Annotation*>& annotations,
    const std::string& expected_text) {
  for (std::vector<const GeneratedCodeInfo::Annotation*>::const_iterator
           i = annotations.begin(),
           e = annotations.end();
       i != e; ++i) {
    const GeneratedCodeInfo::Annotation* annotation = *i;
    uint32 begin = annotation->begin();
    uint32 end = annotation->end();
    if (end < begin || end > file_content.size()) {
      return false;
    }
    if (file_content.substr(begin, end - begin) == expected_text) {
      return true;
    }
  }
  return false;
}

bool AnnotationMatchesSubstring(const std::string& file_content,
                                const GeneratedCodeInfo::Annotation* annotation,
                                const std::string& expected_text) {
  std::vector<const GeneratedCodeInfo::Annotation*> annotations;
  annotations.push_back(annotation);
  return AtLeastOneAnnotationMatchesSubstring(file_content, annotations,
                                              expected_text);
}
}  // namespace annotation_test_util
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
