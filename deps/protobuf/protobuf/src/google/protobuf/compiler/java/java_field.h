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

#ifndef GOOGLE_PROTOBUF_COMPILER_JAVA_FIELD_H__
#define GOOGLE_PROTOBUF_COMPILER_JAVA_FIELD_H__

#include <map>
#include <memory>
#include <string>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {
class Context;            // context.h
class ClassNameResolver;  // name_resolver.h
}  // namespace java
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

class ImmutableFieldGenerator {
 public:
  ImmutableFieldGenerator() {}
  virtual ~ImmutableFieldGenerator();

  virtual int GetNumBitsForMessage() const = 0;
  virtual int GetNumBitsForBuilder() const = 0;
  virtual void GenerateInterfaceMembers(io::Printer* printer) const = 0;
  virtual void GenerateMembers(io::Printer* printer) const = 0;
  virtual void GenerateBuilderMembers(io::Printer* printer) const = 0;
  virtual void GenerateInitializationCode(io::Printer* printer) const = 0;
  virtual void GenerateBuilderClearCode(io::Printer* printer) const = 0;
  virtual void GenerateMergingCode(io::Printer* printer) const = 0;
  virtual void GenerateBuildingCode(io::Printer* printer) const = 0;
  virtual void GenerateParsingCode(io::Printer* printer) const = 0;
  virtual void GenerateParsingCodeFromPacked(io::Printer* printer) const;
  virtual void GenerateParsingDoneCode(io::Printer* printer) const = 0;
  virtual void GenerateSerializationCode(io::Printer* printer) const = 0;
  virtual void GenerateSerializedSizeCode(io::Printer* printer) const = 0;
  virtual void GenerateFieldBuilderInitializationCode(
      io::Printer* printer) const = 0;

  virtual void GenerateEqualsCode(io::Printer* printer) const = 0;
  virtual void GenerateHashCode(io::Printer* printer) const = 0;

  virtual std::string GetBoxedType() const = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ImmutableFieldGenerator);
};

class ImmutableFieldLiteGenerator {
 public:
  ImmutableFieldLiteGenerator() {}
  virtual ~ImmutableFieldLiteGenerator();

  virtual int GetNumBitsForMessage() const = 0;
  virtual void GenerateInterfaceMembers(io::Printer* printer) const = 0;
  virtual void GenerateMembers(io::Printer* printer) const = 0;
  virtual void GenerateBuilderMembers(io::Printer* printer) const = 0;
  virtual void GenerateInitializationCode(io::Printer* printer) const = 0;
  virtual void GenerateFieldInfo(io::Printer* printer,
                                 std::vector<uint16>* output) const = 0;

  virtual std::string GetBoxedType() const = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ImmutableFieldLiteGenerator);
};


// Convenience class which constructs FieldGenerators for a Descriptor.
template <typename FieldGeneratorType>
class FieldGeneratorMap {
 public:
  explicit FieldGeneratorMap(const Descriptor* descriptor, Context* context);
  ~FieldGeneratorMap();

  const FieldGeneratorType& get(const FieldDescriptor* field) const;

 private:
  const Descriptor* descriptor_;
  std::vector<std::unique_ptr<FieldGeneratorType>> field_generators_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FieldGeneratorMap);
};

template <typename FieldGeneratorType>
inline const FieldGeneratorType& FieldGeneratorMap<FieldGeneratorType>::get(
    const FieldDescriptor* field) const {
  GOOGLE_CHECK_EQ(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}

// Instantiate template for mutable and immutable maps.
template <>
FieldGeneratorMap<ImmutableFieldGenerator>::FieldGeneratorMap(
    const Descriptor* descriptor, Context* context);

template <>
FieldGeneratorMap<ImmutableFieldGenerator>::~FieldGeneratorMap();


template <>
FieldGeneratorMap<ImmutableFieldLiteGenerator>::FieldGeneratorMap(
    const Descriptor* descriptor, Context* context);

template <>
FieldGeneratorMap<ImmutableFieldLiteGenerator>::~FieldGeneratorMap();


// Field information used in FieldGeneartors.
struct FieldGeneratorInfo {
  std::string name;
  std::string capitalized_name;
  std::string disambiguated_reason;
};

// Oneof information used in OneofFieldGenerators.
struct OneofGeneratorInfo {
  std::string name;
  std::string capitalized_name;
};

// Set some common variables used in variable FieldGenerators.
void SetCommonFieldVariables(const FieldDescriptor* descriptor,
                             const FieldGeneratorInfo* info,
                             std::map<std::string, std::string>* variables);

// Set some common oneof variables used in OneofFieldGenerators.
void SetCommonOneofVariables(const FieldDescriptor* descriptor,
                             const OneofGeneratorInfo* info,
                             std::map<std::string, std::string>* variables);

// Print useful comments before a field's accessors.
void PrintExtraFieldInfo(const std::map<std::string, std::string>& variables,
                         io::Printer* printer);

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_JAVA_FIELD_H__
