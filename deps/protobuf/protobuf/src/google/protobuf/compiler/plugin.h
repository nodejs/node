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
// Front-end for protoc code generator plugins written in C++.
//
// To implement a protoc plugin in C++, simply write an implementation of
// CodeGenerator, then create a main() function like:
//   int main(int argc, char* argv[]) {
//     MyCodeGenerator generator;
//     return google::protobuf::compiler::PluginMain(argc, argv, &generator);
//   }
// You must link your plugin against libprotobuf and libprotoc.
//
// The core part of PluginMain is to invoke the given CodeGenerator on a
// CodeGeneratorRequest to generate a CodeGeneratorResponse. This part is
// abstracted out and made into function GenerateCode so that it can be reused,
// for example, to implement a variant of PluginMain that does some
// preprocessing on the input CodeGeneratorRequest before feeding the request
// to the given code generator.
//
// To get protoc to use the plugin, do one of the following:
// * Place the plugin binary somewhere in the PATH and give it the name
//   "protoc-gen-NAME" (replacing "NAME" with the name of your plugin).  If you
//   then invoke protoc with the parameter --NAME_out=OUT_DIR (again, replace
//   "NAME" with your plugin's name), protoc will invoke your plugin to generate
//   the output, which will be placed in OUT_DIR.
// * Place the plugin binary anywhere, with any name, and pass the --plugin
//   parameter to protoc to direct it to your plugin like so:
//     protoc --plugin=protoc-gen-NAME=path/to/mybinary --NAME_out=OUT_DIR
//   On Windows, make sure to include the .exe suffix:
//     protoc --plugin=protoc-gen-NAME=path/to/mybinary.exe --NAME_out=OUT_DIR

#ifndef GOOGLE_PROTOBUF_COMPILER_PLUGIN_H__
#define GOOGLE_PROTOBUF_COMPILER_PLUGIN_H__

#include <string>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {

class CodeGenerator;  // code_generator.h
class CodeGeneratorRequest;
class CodeGeneratorResponse;

// Implements main() for a protoc plugin exposing the given code generator.
PROTOC_EXPORT int PluginMain(int argc, char* argv[],
                             const CodeGenerator* generator);

// Generates code using the given code generator. Returns true if the code
// generation is successful. If the code geneartion fails, error_msg may be
// populated to describe the failure cause.
bool GenerateCode(const CodeGeneratorRequest& request,
                  const CodeGenerator& generator,
                  CodeGeneratorResponse* response, std::string* error_msg);

}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMPILER_PLUGIN_H__
