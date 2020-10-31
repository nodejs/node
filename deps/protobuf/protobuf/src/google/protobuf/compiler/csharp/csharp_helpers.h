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

#ifndef GOOGLE_PROTOBUF_COMPILER_CSHARP_HELPERS_H__
#define GOOGLE_PROTOBUF_COMPILER_CSHARP_HELPERS_H__

#include <string>
#include <google/protobuf/stubs/port.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace csharp {

struct Options;
class FieldGeneratorBase;

// TODO: start using this enum.
enum CSharpType {
  CSHARPTYPE_INT32 = 1,
  CSHARPTYPE_INT64 = 2,
  CSHARPTYPE_UINT32 = 3,
  CSHARPTYPE_UINT64 = 4,
  CSHARPTYPE_FLOAT = 5,
  CSHARPTYPE_DOUBLE = 6,
  CSHARPTYPE_BOOL = 7,
  CSHARPTYPE_STRING = 8,
  CSHARPTYPE_BYTESTRING = 9,
  CSHARPTYPE_MESSAGE = 10,
  CSHARPTYPE_ENUM = 11,
  MAX_CSHARPTYPE = 11
};

// Converts field type to corresponding C# type.
CSharpType GetCSharpType(FieldDescriptor::Type type);

std::string StripDotProto(const std::string& proto_file);

// Gets unqualified name of the reflection class
std::string GetReflectionClassUnqualifiedName(const FileDescriptor* descriptor);
// Gets unqualified name of the extension class
std::string GetExtensionClassUnqualifiedName(const FileDescriptor* descriptor);

std::string GetClassName(const EnumDescriptor* descriptor);

std::string GetFieldName(const FieldDescriptor* descriptor);

std::string GetFieldConstantName(const FieldDescriptor* field);

std::string GetPropertyName(const FieldDescriptor* descriptor);

int GetFixedSize(FieldDescriptor::Type type);

std::string UnderscoresToCamelCase(const std::string& input,
                                   bool cap_next_letter,
                                   bool preserve_period);

inline std::string UnderscoresToCamelCase(const std::string& input, bool cap_next_letter) {
  return UnderscoresToCamelCase(input, cap_next_letter, false);
}

std::string UnderscoresToPascalCase(const std::string& input);

// Note that we wouldn't normally want to export this (we're not expecting
// it to be used outside libprotoc itself) but this exposes it for testing.
std::string PROTOC_EXPORT GetEnumValueName(const std::string& enum_name,
                                           const std::string& enum_value_name);

// TODO(jtattermusch): perhaps we could move this to strutil
std::string StringToBase64(const std::string& input);

std::string FileDescriptorToBase64(const FileDescriptor* descriptor);

FieldGeneratorBase* CreateFieldGenerator(const FieldDescriptor* descriptor,
                                         int presenceIndex,
                                         const Options* options);

std::string GetFullExtensionName(const FieldDescriptor* descriptor);

bool IsNullable(const FieldDescriptor* descriptor);

// Determines whether the given message is a map entry message,
// i.e. one implicitly created by protoc due to a map<key, value> field.
inline bool IsMapEntryMessage(const Descriptor* descriptor) {
  return descriptor->options().map_entry();
}

// Checks if this descriptor is for a group and gets its end tag or 0 if it's not a group
uint GetGroupEndTag(const Descriptor* descriptor);

// Determines whether we're generating code for the proto representation of
// descriptors etc, for use in the runtime. This is the only type which is
// allowed to use proto2 syntax, and it generates internal classes.
inline bool IsDescriptorProto(const FileDescriptor* descriptor) {
  return descriptor->name() == "google/protobuf/descriptor.proto";
}

// Determines whether the given message is an options message within descriptor.proto.
inline bool IsDescriptorOptionMessage(const Descriptor* descriptor) {
  if (!IsDescriptorProto(descriptor->file())) {
    return false;
  }
  const string name = descriptor->full_name();
  return name == "google.protobuf.FileOptions" ||
      name == "google.protobuf.MessageOptions" ||
      name == "google.protobuf.FieldOptions" ||
      name == "google.protobuf.OneofOptions" ||
      name == "google.protobuf.EnumOptions" ||
      name == "google.protobuf.EnumValueOptions" ||
      name == "google.protobuf.ServiceOptions" ||
      name == "google.protobuf.MethodOptions";
}

inline bool IsWrapperType(const FieldDescriptor* descriptor) {
  return descriptor->type() == FieldDescriptor::TYPE_MESSAGE &&
      descriptor->message_type()->file()->name() == "google/protobuf/wrappers.proto";
}

inline bool IsProto2(const FileDescriptor* descriptor) {
  return descriptor->syntax() == FileDescriptor::SYNTAX_PROTO2;
}

}  // namespace csharp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMPILER_CSHARP_HELPERS_H__
