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

// Author: liujisi@google.com (Pherl Liu)

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_GENERATOR_FACTORY_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_GENERATOR_FACTORY_H__

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
class FieldDescriptor;    // descriptor.h
class Descriptor;         // descriptor.h
class ServiceDescriptor;  // descriptor.h
namespace compiler {
namespace java {
class MessageGenerator;    // message.h
class ExtensionGenerator;  // extension.h
class ServiceGenerator;    // service.h
class Context;             // context.h
}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

class GeneratorFactory {
 public:
  GeneratorFactory();
  virtual ~GeneratorFactory();

  virtual MessageGenerator* NewMessageGenerator(
      const Descriptor* descriptor) const = 0;

  virtual ExtensionGenerator* NewExtensionGenerator(
      const FieldDescriptor* descriptor) const = 0;

  virtual ServiceGenerator* NewServiceGenerator(
      const ServiceDescriptor* descriptor) const = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(GeneratorFactory);
};

// Factory that creates generators for immutable-default messages.
class ImmutableGeneratorFactory : public GeneratorFactory {
 public:
  ImmutableGeneratorFactory(Context* context);
  virtual ~ImmutableGeneratorFactory();

  virtual MessageGenerator* NewMessageGenerator(
      const Descriptor* descriptor) const;

  virtual ExtensionGenerator* NewExtensionGenerator(
      const FieldDescriptor* descriptor) const;

  virtual ServiceGenerator* NewServiceGenerator(
      const ServiceDescriptor* descriptor) const;

 private:
  Context* context_;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ImmutableGeneratorFactory);
};


}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_GENERATOR_FACTORY_H__
