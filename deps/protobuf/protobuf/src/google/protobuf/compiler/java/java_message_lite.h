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

// Author: dweis@google.com (Daniel Weis)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_MESSAGE_LITE_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_MESSAGE_LITE_H__

#include <map>
#include <string>
#include <google/protobuf/compiler/java/java_field.h>
#include <google/protobuf/compiler/java/java_message.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

class ImmutableMessageLiteGenerator : public MessageGenerator {
 public:
  ImmutableMessageLiteGenerator(const Descriptor* descriptor, Context* context);
  virtual ~ImmutableMessageLiteGenerator();

  virtual void Generate(io::Printer* printer);
  virtual void GenerateInterface(io::Printer* printer);
  virtual void GenerateExtensionRegistrationCode(io::Printer* printer);
  virtual void GenerateStaticVariables(io::Printer* printer,
                                       int* bytecode_estimate);
  virtual int GenerateStaticVariableInitializers(io::Printer* printer);

 private:
  void GenerateParseFromMethods(io::Printer* printer);

  void GenerateBuilder(io::Printer* printer);
  void GenerateDynamicMethodNewBuilder(io::Printer* printer);
  void GenerateInitializers(io::Printer* printer);
  void GenerateParser(io::Printer* printer);
  void GenerateConstructor(io::Printer* printer);
  void GenerateDynamicMethodNewBuildMessageInfo(io::Printer* printer);

  Context* context_;
  ClassNameResolver* name_resolver_;
  FieldGeneratorMap<ImmutableFieldLiteGenerator> field_generators_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ImmutableMessageLiteGenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_MESSAGE_LITE_H__
