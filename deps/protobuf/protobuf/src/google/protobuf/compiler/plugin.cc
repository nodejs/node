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

#include <google/protobuf/compiler/plugin.h>

#include <iostream>
#include <set>

#ifdef _WIN32
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/io_win32.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>


namespace google {
namespace protobuf {
namespace compiler {

#if defined(_WIN32)
// DO NOT include <io.h>, instead create functions in io_win32.{h,cc} and import
// them like we do below.
using google::protobuf::io::win32::setmode;
#endif

class GeneratorResponseContext : public GeneratorContext {
 public:
  GeneratorResponseContext(
      const Version& compiler_version, CodeGeneratorResponse* response,
      const std::vector<const FileDescriptor*>& parsed_files)
      : compiler_version_(compiler_version),
        response_(response),
        parsed_files_(parsed_files) {}
  virtual ~GeneratorResponseContext() {}

  // implements GeneratorContext --------------------------------------

  virtual io::ZeroCopyOutputStream* Open(const std::string& filename) {
    CodeGeneratorResponse::File* file = response_->add_file();
    file->set_name(filename);
    return new io::StringOutputStream(file->mutable_content());
  }

  virtual io::ZeroCopyOutputStream* OpenForInsert(
      const std::string& filename, const std::string& insertion_point) {
    CodeGeneratorResponse::File* file = response_->add_file();
    file->set_name(filename);
    file->set_insertion_point(insertion_point);
    return new io::StringOutputStream(file->mutable_content());
  }

  void ListParsedFiles(std::vector<const FileDescriptor*>* output) {
    *output = parsed_files_;
  }

  void GetCompilerVersion(Version* version) const {
    *version = compiler_version_;
  }

 private:
  Version compiler_version_;
  CodeGeneratorResponse* response_;
  const std::vector<const FileDescriptor*>& parsed_files_;
};

bool GenerateCode(const CodeGeneratorRequest& request,
                  const CodeGenerator& generator,
                  CodeGeneratorResponse* response, std::string* error_msg) {
  DescriptorPool pool;
  for (int i = 0; i < request.proto_file_size(); i++) {
    const FileDescriptor* file = pool.BuildFile(request.proto_file(i));
    if (file == NULL) {
      // BuildFile() already wrote an error message.
      return false;
    }
  }

  std::vector<const FileDescriptor*> parsed_files;
  for (int i = 0; i < request.file_to_generate_size(); i++) {
    parsed_files.push_back(pool.FindFileByName(request.file_to_generate(i)));
    if (parsed_files.back() == NULL) {
      *error_msg =
          "protoc asked plugin to generate a file but "
          "did not provide a descriptor for the file: " +
          request.file_to_generate(i);
      return false;
    }
  }

  GeneratorResponseContext context(request.compiler_version(), response,
                                   parsed_files);


  std::string error;
  bool succeeded = generator.GenerateAll(parsed_files, request.parameter(),
                                         &context, &error);

  if (!succeeded && error.empty()) {
    error =
        "Code generator returned false but provided no error "
        "description.";
  }
  if (!error.empty()) {
    response->set_error(error);
  }

  return true;
}

int PluginMain(int argc, char* argv[], const CodeGenerator* generator) {

  if (argc > 1) {
    std::cerr << argv[0] << ": Unknown option: " << argv[1] << std::endl;
    return 1;
  }

#ifdef _WIN32
  setmode(STDIN_FILENO, _O_BINARY);
  setmode(STDOUT_FILENO, _O_BINARY);
#endif

  CodeGeneratorRequest request;
  if (!request.ParseFromFileDescriptor(STDIN_FILENO)) {
    std::cerr << argv[0] << ": protoc sent unparseable request to plugin."
              << std::endl;
    return 1;
  }

  std::string error_msg;
  CodeGeneratorResponse response;

  if (GenerateCode(request, *generator, &response, &error_msg)) {
    if (!response.SerializeToFileDescriptor(STDOUT_FILENO)) {
      std::cerr << argv[0] << ": Error writing to stdout." << std::endl;
      return 1;
    }
  } else {
    if (!error_msg.empty()) {
      std::cerr << argv[0] << ": " << error_msg << std::endl;
    }
    return 1;
  }

  return 0;
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
