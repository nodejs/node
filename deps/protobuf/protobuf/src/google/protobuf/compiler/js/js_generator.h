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

// Generates JavaScript code for a given .proto file.
//
#ifndef GOOGLE_PROTOBUF_COMPILER_JS_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_JS_GENERATOR_H__

#include <set>
#include <string>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/scc.h>
#include <google/protobuf/compiler/code_generator.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {

class Descriptor;
class EnumDescriptor;
class FieldDescriptor;
class OneofDescriptor;
class FileDescriptor;

namespace io {
class Printer;
}

namespace compiler {
namespace js {

struct GeneratorOptions {
  // Output path.
  std::string output_dir;
  // Namespace prefix.
  std::string namespace_prefix;
  // Enable binary-format support?
  bool binary;
  // What style of imports should be used.
  enum ImportStyle {
    kImportClosure,         // goog.require()
    kImportCommonJs,        // require()
    kImportCommonJsStrict,  // require() with no global export
    kImportBrowser,         // no import statements
    kImportEs6,             // import { member } from ''
  } import_style;

  GeneratorOptions()
      : output_dir("."),
        namespace_prefix(""),
        binary(false),
        import_style(kImportClosure),
        add_require_for_enums(false),
        testonly(false),
        library(""),
        error_on_name_conflict(false),
        extension(".js"),
        one_output_file_per_input_file(false),
        annotate_code(false) {}

  bool ParseFromOptions(
      const std::vector<std::pair<std::string, std::string> >& options,
      std::string* error);

  // Returns the file name extension to use for generated code.
  std::string GetFileNameExtension() const {
    return import_style == kImportClosure ? extension : "_pb.js";
  }

  enum OutputMode {
    // Create an output file for each input .proto file.
    kOneOutputFilePerInputFile,
    // Create an output file for each type.
    kOneOutputFilePerSCC,
    // Put everything in a single file named by the library option.
    kEverythingInOneFile,
  };

  // Indicates how to output the generated code based on the provided options.
  OutputMode output_mode() const;

  // The remaining options are only relevant when we are using kImportClosure.

  // Add a `goog.requires()` call for each enum type used. If not set, a
  // forward declaration with `goog.forwardDeclare` is produced instead.
  bool add_require_for_enums;
  // Set this as a test-only module via `goog.setTestOnly();`.
  bool testonly;
  // Create a library with name <name>_lib.js rather than a separate .js file
  // per type?
  std::string library;
  // Error if there are two types that would generate the same output file?
  bool error_on_name_conflict;
  // The extension to use for output file names.
  std::string extension;
  // Create a separate output file for each input file?
  bool one_output_file_per_input_file;
  // If true, we should append annotations as commen on the last line for
  // generated .js file. Annotations used by tools like https://kythe.io
  // to provide cross-references between .js and .proto files. Annotations
  // are enced as base64 proto of GeneratedCodeInfo message (see
  // descriptor.proto).
  bool annotate_code;
};

// CodeGenerator implementation which generates a JavaScript source file and
// header.  If you create your own protocol compiler binary and you want it to
// support JavaScript output, you can do so by registering an instance of this
// CodeGenerator with the CommandLineInterface in your main() function.
class PROTOC_EXPORT Generator : public CodeGenerator {
 public:
  Generator() {}
  virtual ~Generator() {}

  virtual bool Generate(const FileDescriptor* file,
                        const std::string& parameter, GeneratorContext* context,
                        std::string* error) const {
    *error = "Unimplemented Generate() method. Call GenerateAll() instead.";
    return false;
  }

  virtual bool HasGenerateAll() const { return true; }

  virtual bool GenerateAll(const std::vector<const FileDescriptor*>& files,
                           const std::string& parameter,
                           GeneratorContext* context, std::string* error) const;

 private:
  void GenerateHeader(const GeneratorOptions& options,
                      const FileDescriptor* file, io::Printer* printer) const;

  // Generate goog.provides() calls.
  void FindProvides(const GeneratorOptions& options, io::Printer* printer,
                    const std::vector<const FileDescriptor*>& file,
                    std::set<std::string>* provided) const;
  void FindProvidesForFile(const GeneratorOptions& options,
                           io::Printer* printer, const FileDescriptor* file,
                           std::set<std::string>* provided) const;
  void FindProvidesForMessage(const GeneratorOptions& options,
                              io::Printer* printer, const Descriptor* desc,
                              std::set<std::string>* provided) const;
  void FindProvidesForEnum(const GeneratorOptions& options,
                           io::Printer* printer, const EnumDescriptor* enumdesc,
                           std::set<std::string>* provided) const;
  // For extension fields at file scope.
  void FindProvidesForFields(const GeneratorOptions& options,
                             io::Printer* printer,
                             const std::vector<const FieldDescriptor*>& fields,
                             std::set<std::string>* provided) const;
  // Print the goog.provides() found by the methods above.
  void GenerateProvides(const GeneratorOptions& options, io::Printer* printer,
                        std::set<std::string>* provided) const;

  // Generate goog.setTestOnly() if indicated.
  void GenerateTestOnly(const GeneratorOptions& options,
                        io::Printer* printer) const;

  // Generate goog.requires() calls.
  void GenerateRequiresForLibrary(
      const GeneratorOptions& options, io::Printer* printer,
      const std::vector<const FileDescriptor*>& files,
      std::set<std::string>* provided) const;
  void GenerateRequiresForSCC(const GeneratorOptions& options,
                              io::Printer* printer, const SCC* scc,
                              std::set<std::string>* provided) const;
  // For extension fields at file scope.
  void GenerateRequiresForExtensions(
      const GeneratorOptions& options, io::Printer* printer,
      const std::vector<const FieldDescriptor*>& fields,
      std::set<std::string>* provided) const;
  void GenerateRequiresImpl(const GeneratorOptions& options,
                            io::Printer* printer,
                            std::set<std::string>* required,
                            std::set<std::string>* forwards,
                            std::set<std::string>* provided, bool require_jspb,
                            bool require_extension, bool require_map) const;
  void FindRequiresForMessage(const GeneratorOptions& options,
                              const Descriptor* desc,
                              std::set<std::string>* required,
                              std::set<std::string>* forwards,
                              bool* have_message) const;
  void FindRequiresForField(const GeneratorOptions& options,
                            const FieldDescriptor* field,
                            std::set<std::string>* required,
                            std::set<std::string>* forwards) const;
  void FindRequiresForExtension(const GeneratorOptions& options,
                                const FieldDescriptor* field,
                                std::set<std::string>* required,
                                std::set<std::string>* forwards) const;
  // Generate all things in a proto file into one file.
  // If use_short_name is true, the generated file's name will only be short
  // name that without directory, otherwise filename equals file->name()
  bool GenerateFile(const FileDescriptor* file, const GeneratorOptions& options,
                    GeneratorContext* context, bool use_short_name) const;
  void GenerateFile(const GeneratorOptions& options, io::Printer* printer,
                    const FileDescriptor* file) const;

  // Generate definitions for all message classes and enums in all files,
  // processing the files in dependence order.
  void GenerateFilesInDepOrder(
      const GeneratorOptions& options, io::Printer* printer,
      const std::vector<const FileDescriptor*>& file) const;
  // Helper for above.
  void GenerateFileAndDeps(const GeneratorOptions& options,
                           io::Printer* printer, const FileDescriptor* root,
                           std::set<const FileDescriptor*>* all_files,
                           std::set<const FileDescriptor*>* generated) const;

  // Generate definitions for all message classes and enums.
  void GenerateClassesAndEnums(const GeneratorOptions& options,
                               io::Printer* printer,
                               const FileDescriptor* file) const;

  void GenerateFieldValueExpression(io::Printer* printer,
                                    const char* obj_reference,
                                    const FieldDescriptor* field,
                                    bool use_default) const;

  // Generate definition for one class.
  void GenerateClass(const GeneratorOptions& options, io::Printer* printer,
                     const Descriptor* desc) const;
  void GenerateClassConstructor(const GeneratorOptions& options,
                                io::Printer* printer,
                                const Descriptor* desc) const;
  void GenerateClassFieldInfo(const GeneratorOptions& options,
                              io::Printer* printer,
                              const Descriptor* desc) const;
  void GenerateClassConstructorAndDeclareExtensionFieldInfo(
      const GeneratorOptions& options, io::Printer* printer,
      const Descriptor* desc) const;
  void GenerateClassXid(const GeneratorOptions& options, io::Printer* printer,
                        const Descriptor* desc) const;
  void GenerateOneofCaseDefinition(const GeneratorOptions& options,
                                   io::Printer* printer,
                                   const OneofDescriptor* oneof) const;
  void GenerateObjectTypedef(const GeneratorOptions& options,
                             io::Printer* printer,
                             const Descriptor* desc) const;
  void GenerateClassToObject(const GeneratorOptions& options,
                             io::Printer* printer,
                             const Descriptor* desc) const;
  void GenerateClassFieldToObject(const GeneratorOptions& options,
                                  io::Printer* printer,
                                  const FieldDescriptor* field) const;
  void GenerateClassFromObject(const GeneratorOptions& options,
                               io::Printer* printer,
                               const Descriptor* desc) const;
  void GenerateClassFieldFromObject(const GeneratorOptions& options,
                                    io::Printer* printer,
                                    const FieldDescriptor* field) const;
  void GenerateClassRegistration(const GeneratorOptions& options,
                                 io::Printer* printer,
                                 const Descriptor* desc) const;
  void GenerateClassFields(const GeneratorOptions& options,
                           io::Printer* printer, const Descriptor* desc) const;
  void GenerateClassField(const GeneratorOptions& options, io::Printer* printer,
                          const FieldDescriptor* desc) const;
  void GenerateClassExtensionFieldInfo(const GeneratorOptions& options,
                                       io::Printer* printer,
                                       const Descriptor* desc) const;
  void GenerateClassDeserialize(const GeneratorOptions& options,
                                io::Printer* printer,
                                const Descriptor* desc) const;
  void GenerateClassDeserializeBinary(const GeneratorOptions& options,
                                      io::Printer* printer,
                                      const Descriptor* desc) const;
  void GenerateClassDeserializeBinaryField(const GeneratorOptions& options,
                                           io::Printer* printer,
                                           const FieldDescriptor* field) const;
  void GenerateClassSerializeBinary(const GeneratorOptions& options,
                                    io::Printer* printer,
                                    const Descriptor* desc) const;
  void GenerateClassSerializeBinaryField(const GeneratorOptions& options,
                                         io::Printer* printer,
                                         const FieldDescriptor* field) const;

  // Generate definition for one enum.
  void GenerateEnum(const GeneratorOptions& options, io::Printer* printer,
                    const EnumDescriptor* enumdesc) const;

  // Generate an extension definition.
  void GenerateExtension(const GeneratorOptions& options, io::Printer* printer,
                         const FieldDescriptor* field) const;

  // Generate addFoo() method for repeated primitive fields.
  void GenerateRepeatedPrimitiveHelperMethods(const GeneratorOptions& options,
                                              io::Printer* printer,
                                              const FieldDescriptor* field,
                                              bool untyped) const;

  // Generate addFoo() method for repeated message fields.
  void GenerateRepeatedMessageHelperMethods(const GeneratorOptions& options,
                                            io::Printer* printer,
                                            const FieldDescriptor* field) const;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Generator);
};

}  // namespace js
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_COMPILER_JS_GENERATOR_H__
