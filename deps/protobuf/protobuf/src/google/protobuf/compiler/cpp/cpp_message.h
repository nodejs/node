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

#ifndef GOOGLE_PROTOBUF_COMPILER_CPP_MESSAGE_H__
#define GOOGLE_PROTOBUF_COMPILER_CPP_MESSAGE_H__

#include <memory>
#include <set>
#include <string>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_message_layout_helper.h>
#include <google/protobuf/compiler/cpp/cpp_options.h>

namespace google {
namespace protobuf {
namespace io {
class Printer;  // printer.h
}
}  // namespace protobuf
}  // namespace google

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

class EnumGenerator;       // enum.h
class ExtensionGenerator;  // extension.h

class MessageGenerator {
 public:
  // See generator.cc for the meaning of dllexport_decl.
  MessageGenerator(const Descriptor* descriptor,
                   const std::map<std::string, std::string>& vars,
                   int index_in_file_messages, const Options& options,
                   MessageSCCAnalyzer* scc_analyzer);
  ~MessageGenerator();

  // Append the two types of nested generators to the corresponding vector.
  void AddGenerators(
      std::vector<std::unique_ptr<EnumGenerator>>* enum_generators,
      std::vector<std::unique_ptr<ExtensionGenerator>>* extension_generators);

  // Generate definitions for this class and all its nested types.
  void GenerateClassDefinition(io::Printer* printer);

  // Generate definitions of inline methods (placed at the end of the header
  // file).
  void GenerateInlineMethods(io::Printer* printer);

  // Source file stuff.

  // Generate extra fields
  void GenerateExtraDefaultFields(io::Printer* printer);

  // Generates code that creates default instances for fields.
  void GenerateFieldDefaultInstances(io::Printer* printer);

  // Generates code that initializes the message's default instance.  This
  // is separate from allocating because all default instances must be
  // allocated before any can be initialized.
  void GenerateDefaultInstanceInitializer(io::Printer* printer);

  // Generate all non-inline methods for this class.
  void GenerateClassMethods(io::Printer* printer);

  // Generate source file code that should go outside any namespace.
  void GenerateSourceInProto2Namespace(io::Printer* printer);

 private:
  // Generate declarations and definitions of accessors for fields.
  void GenerateFieldAccessorDeclarations(io::Printer* printer);
  void GenerateFieldAccessorDefinitions(io::Printer* printer);

  // Generate the table-driven parsing array.  Returns the number of entries
  // generated.
  size_t GenerateParseOffsets(io::Printer* printer);
  size_t GenerateParseAuxTable(io::Printer* printer);
  // Generates a ParseTable entry.  Returns whether the proto uses
  // table-driven parsing.
  bool GenerateParseTable(io::Printer* printer, size_t offset,
                          size_t aux_offset);

  // Generate the field offsets array.  Returns the a pair of the total numer
  // of entries generated and the index of the first has_bit entry.
  std::pair<size_t, size_t> GenerateOffsets(io::Printer* printer);
  void GenerateSchema(io::Printer* printer, int offset, int has_offset);
  // For each field generates a table entry describing the field for the
  // table driven serializer.
  int GenerateFieldMetadata(io::Printer* printer);

  // Generate constructors and destructor.
  void GenerateStructors(io::Printer* printer);

  // The compiler typically generates multiple copies of each constructor and
  // destructor: http://gcc.gnu.org/bugs.html#nonbugs_cxx
  // Placing common code in a separate method reduces the generated code size.
  //
  // Generate the shared constructor code.
  void GenerateSharedConstructorCode(io::Printer* printer);
  // Generate the shared destructor code.
  void GenerateSharedDestructorCode(io::Printer* printer);
  // Generate the arena-specific destructor code.
  void GenerateArenaDestructorCode(io::Printer* printer);

  // Generate standard Message methods.
  void GenerateClear(io::Printer* printer);
  void GenerateOneofClear(io::Printer* printer);
  void GenerateMergeFromCodedStream(io::Printer* printer);
  void GenerateSerializeWithCachedSizes(io::Printer* printer);
  void GenerateSerializeWithCachedSizesToArray(io::Printer* printer);
  void GenerateSerializeWithCachedSizesBody(io::Printer* printer,
                                            bool to_array);
  void GenerateByteSize(io::Printer* printer);
  void GenerateMergeFrom(io::Printer* printer);
  void GenerateCopyFrom(io::Printer* printer);
  void GenerateSwap(io::Printer* printer);
  void GenerateIsInitialized(io::Printer* printer);

  // Helpers for GenerateSerializeWithCachedSizes().
  //
  // cached_has_bit_index maintains that:
  //   cached_has_bits = _has_bits_[cached_has_bit_index]
  // for cached_has_bit_index >= 0
  void GenerateSerializeOneField(io::Printer* printer,
                                 const FieldDescriptor* field, bool unbounded,
                                 int cached_has_bits_index);
  // Generate a switch statement to serialize 2+ fields from the same oneof.
  // Or, if fields.size() == 1, just call GenerateSerializeOneField().
  void GenerateSerializeOneofFields(
      io::Printer* printer, const std::vector<const FieldDescriptor*>& fields,
      bool to_array);
  void GenerateSerializeOneExtensionRange(
      io::Printer* printer, const Descriptor::ExtensionRange* range,
      bool unbounded);

  // Generates has_foo() functions and variables for singular field has-bits.
  void GenerateSingularFieldHasBits(const FieldDescriptor* field,
                                    Formatter format);
  // Generates has_foo() functions and variables for oneof field has-bits.
  void GenerateOneofHasBits(io::Printer* printer);
  // Generates has_foo_bar() functions for oneof members.
  void GenerateOneofMemberHasBits(const FieldDescriptor* field,
                                  const Formatter& format);
  // Generates the clear_foo() method for a field.
  void GenerateFieldClear(const FieldDescriptor* field, bool is_inline,
                          Formatter format);

  void GenerateConstructorBody(io::Printer* printer,
                               std::vector<bool> already_processed,
                               bool copy_constructor) const;

  size_t HasBitsSize() const;
  std::vector<uint32> RequiredFieldsBitMask() const;

  const Descriptor* descriptor_;
  int index_in_file_messages_;
  std::string classname_;
  Options options_;
  FieldGeneratorMap field_generators_;
  // optimized_order_ is the order we layout the message's fields in the
  // class. This is reused to initialize the fields in-order for cache
  // efficiency.
  //
  // optimized_order_ excludes oneof fields and weak fields.
  std::vector<const FieldDescriptor*> optimized_order_;
  std::vector<int> has_bit_indices_;
  int max_has_bit_index_;
  std::vector<const EnumGenerator*> enum_generators_;
  std::vector<const ExtensionGenerator*> extension_generators_;
  int num_required_fields_;
  int num_weak_fields_;
  // table_driven_ indicates the generated message uses table-driven parsing.
  bool table_driven_;

  std::unique_ptr<MessageLayoutHelper> message_layout_helper_;

  MessageSCCAnalyzer* scc_analyzer_;

  std::map<std::string, std::string> variables_;

  friend class FileGenerator;
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MessageGenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_CPP_MESSAGE_H__
