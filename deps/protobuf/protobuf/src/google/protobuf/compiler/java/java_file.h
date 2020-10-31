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

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_FILE_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_FILE_H__

#include <memory>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/java/java_options.h>

namespace google {
namespace protobuf {
class FileDescriptor;  // descriptor.h
namespace io {
class Printer;  // printer.h
}
namespace compiler {
class GeneratorContext;  // code_generator.h
namespace java {
class Context;             // context.h
class MessageGenerator;    // message.h
class GeneratorFactory;    // generator_factory.h
class ExtensionGenerator;  // extension.h
class ClassNameResolver;   // name_resolver.h
}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

class FileGenerator {
 public:
  FileGenerator(const FileDescriptor* file, const Options& options,
                bool immutable_api = true);
  ~FileGenerator();

  // Checks for problems that would otherwise lead to cryptic compile errors.
  // Returns true if there are no problems, or writes an error description to
  // the given string and returns false otherwise.
  bool Validate(std::string* error);

  void Generate(io::Printer* printer);

  // If we aren't putting everything into one file, this will write all the
  // files other than the outer file (i.e. one for each message, enum, and
  // service type).
  void GenerateSiblings(const std::string& package_dir,
                        GeneratorContext* generator_context,
                        std::vector<std::string>* file_list,
                        std::vector<std::string>* annotation_list);

  const std::string& java_package() { return java_package_; }
  const std::string& classname() { return classname_; }

 private:
  void GenerateDescriptorInitializationCodeForImmutable(io::Printer* printer);
  void GenerateDescriptorInitializationCodeForMutable(io::Printer* printer);

  bool ShouldIncludeDependency(const FileDescriptor* descriptor,
                               bool immutable_api_);

  const FileDescriptor* file_;
  std::string java_package_;
  std::string classname_;

  std::vector<std::unique_ptr<MessageGenerator>> message_generators_;
  std::vector<std::unique_ptr<ExtensionGenerator>> extension_generators_;
  std::unique_ptr<GeneratorFactory> generator_factory_;
  std::unique_ptr<Context> context_;
  ClassNameResolver* name_resolver_;
  const Options options_;
  bool immutable_api_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FileGenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_FILE_H__
