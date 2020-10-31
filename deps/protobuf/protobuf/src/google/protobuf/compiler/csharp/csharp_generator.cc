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

#include <sstream>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/compiler/csharp/csharp_generator.h>
#include <google/protobuf/compiler/csharp/csharp_helpers.h>
#include <google/protobuf/compiler/csharp/csharp_names.h>
#include <google/protobuf/compiler/csharp/csharp_options.h>
#include <google/protobuf/compiler/csharp/csharp_reflection_class.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

void GenerateFile(const FileDescriptor* file, io::Printer* printer,
                  const Options* options) {
  ReflectionClassGenerator reflectionClassGenerator(file, options);
  reflectionClassGenerator.Generate(printer);
}

bool Generator::Generate(
    const FileDescriptor* file,
    const string& parameter,
    GeneratorContext* generator_context,
    string* error) const {

  std::vector<std::pair<string, string> > options;
  ParseGeneratorParameter(parameter, &options);

  // We only support proto3 - but we make an exception for descriptor.proto.
  if (file->syntax() != FileDescriptor::SYNTAX_PROTO3 && !IsDescriptorProto(file)) {
    *error = "C# code generation only supports proto3 syntax";
    return false;
  }

  struct Options cli_options;

  for (int i = 0; i < options.size(); i++) {
    if (options[i].first == "file_extension") {
      cli_options.file_extension = options[i].second;
    } else if (options[i].first == "base_namespace") {
      cli_options.base_namespace = options[i].second;
      cli_options.base_namespace_specified = true;
    } else if (options[i].first == "internal_access") {
      cli_options.internal_access = true;
    } else if (options[i].first == "serializable") {
      cli_options.serializable = true;
    } else {
      *error = "Unknown generator option: " + options[i].first;
      return false;
    }
  }

  string filename_error = "";
  std::string filename = GetOutputFile(file,
      cli_options.file_extension,
      cli_options.base_namespace_specified,
      cli_options.base_namespace,
      &filename_error);

  if (filename.empty()) {
    *error = filename_error;
    return false;
  }
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '$');

  GenerateFile(file, &printer, &cli_options);

  return true;
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
