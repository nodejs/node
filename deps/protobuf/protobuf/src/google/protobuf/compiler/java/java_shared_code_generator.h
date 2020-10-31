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

// Author: xiaofeng@google.com (Feng Xiao)
//
// Generators that generate shared code between immutable API and mutable API.

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_SHARED_CODE_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_SHARED_CODE_GENERATOR_H__

#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/java/java_options.h>

namespace google {
namespace protobuf {
class FileDescriptor;  // descriptor.h
namespace compiler {
class GeneratorContext;  // code_generator.h
namespace java {
class ClassNameResolver;  // name_resolver.h
}
}  // namespace compiler
namespace io {
class Printer;  // printer.h
}
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

// A generator that generates code that are shared between immutable API
// and mutable API. Currently only descriptors are shared.
class SharedCodeGenerator {
 public:
  SharedCodeGenerator(const FileDescriptor* file, const Options& options);
  ~SharedCodeGenerator();

  void Generate(GeneratorContext* generator_context,
                std::vector<std::string>* file_list,
                std::vector<std::string>* annotation_file_list);

  void GenerateDescriptors(io::Printer* printer);

 private:
  std::unique_ptr<ClassNameResolver> name_resolver_;
  const FileDescriptor* file_;
  const Options options_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(SharedCodeGenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_SHARED_CODE_GENERATOR_H__
