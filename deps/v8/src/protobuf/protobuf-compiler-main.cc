// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/protobuf/src/google/protobuf/compiler/command_line_interface.h"
#include "third_party/protobuf/src/google/protobuf/compiler/cpp/cpp_generator.h"

namespace v8 {
namespace internal {

int ProtobufCompilerMain(int argc, char* argv[]) {
  google::protobuf::compiler::CommandLineInterface cli;
  cli.AllowPlugins("protoc-");

  // Proto2 C++
  google::protobuf::compiler::cpp::CppGenerator cpp_generator;
  cli.RegisterGenerator("--cpp_out", "--cpp_opt", &cpp_generator,
                        "Generate C++ header and source.");

  return cli.Run(argc, argv);
}

}  // namespace internal
}  // namespace v8

int main(int argc, char* argv[]) {
  return v8::internal::ProtobufCompilerMain(argc, argv);
}
