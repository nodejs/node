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

#ifndef GOOGLE_PROTOBUF_COMPILER_CPP_FILE_H__
#define GOOGLE_PROTOBUF_COMPILER_CPP_FILE_H__

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_options.h>
#include <google/protobuf/compiler/scc.h>

namespace google {
namespace protobuf {
class FileDescriptor;  // descriptor.h
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
class MessageGenerator;    // message.h
class ServiceGenerator;    // service.h
class ExtensionGenerator;  // extension.h

class FileGenerator {
 public:
  // See generator.cc for the meaning of dllexport_decl.
  FileGenerator(const FileDescriptor* file, const Options& options);
  ~FileGenerator();

  // Shared code between the two header generators below.
  void GenerateHeader(io::Printer* printer);

  // info_path, if non-empty, should be the path (relative to printer's
  // output) to the metadata file describing this proto header.
  void GenerateProtoHeader(io::Printer* printer, const std::string& info_path);
  // info_path, if non-empty, should be the path (relative to printer's
  // output) to the metadata file describing this PB header.
  void GeneratePBHeader(io::Printer* printer, const std::string& info_path);
  void GenerateSource(io::Printer* printer);

  int NumMessages() const { return message_generators_.size(); }
  // Similar to GenerateSource but generates only one message
  void GenerateSourceForMessage(int idx, io::Printer* printer);
  void GenerateGlobalSource(io::Printer* printer);

 private:
  // Internal type used by GenerateForwardDeclarations (defined in file.cc).
  class ForwardDeclarations;
  struct CrossFileReferences;

  void IncludeFile(const std::string& google3_name, io::Printer* printer) {
    DoIncludeFile(google3_name, false, printer);
  }
  void IncludeFileAndExport(const std::string& google3_name,
                            io::Printer* printer) {
    DoIncludeFile(google3_name, true, printer);
  }
  void DoIncludeFile(const std::string& google3_name, bool do_export,
                     io::Printer* printer);

  std::string CreateHeaderInclude(const std::string& basename,
                                  const FileDescriptor* file);
  void GetCrossFileReferencesForField(const FieldDescriptor* field,
                                      CrossFileReferences* refs);
  void GetCrossFileReferencesForFile(const FileDescriptor* file,
                                     CrossFileReferences* refs);
  void GenerateInternalForwardDeclarations(const CrossFileReferences& refs,
                                           io::Printer* printer);
  void GenerateSourceIncludes(io::Printer* printer);
  void GenerateSourceDefaultInstance(int idx, io::Printer* printer);

  void GenerateInitForSCC(const SCC* scc, io::Printer* printer);
  void GenerateTables(io::Printer* printer);
  void GenerateReflectionInitializationCode(io::Printer* printer);

  // For other imports, generates their forward-declarations.
  void GenerateForwardDeclarations(io::Printer* printer);

  // Generates top or bottom of a header file.
  void GenerateTopHeaderGuard(io::Printer* printer, bool pb_h);
  void GenerateBottomHeaderGuard(io::Printer* printer, bool pb_h);

  // Generates #include directives.
  void GenerateLibraryIncludes(io::Printer* printer);
  void GenerateDependencyIncludes(io::Printer* printer);

  // Generate a pragma to pull in metadata using the given info_path (if
  // non-empty). info_path should be relative to printer's output.
  void GenerateMetadataPragma(io::Printer* printer,
                              const std::string& info_path);

  // Generates a couple of different pieces before definitions:
  void GenerateGlobalStateFunctionDeclarations(io::Printer* printer);

  // Generates types for classes.
  void GenerateMessageDefinitions(io::Printer* printer);

  void GenerateEnumDefinitions(io::Printer* printer);

  // Generates generic service definitions.
  void GenerateServiceDefinitions(io::Printer* printer);

  // Generates extension identifiers.
  void GenerateExtensionIdentifiers(io::Printer* printer);

  // Generates inline function defintions.
  void GenerateInlineFunctionDefinitions(io::Printer* printer);

  void GenerateProto2NamespaceEnumSpecializations(io::Printer* printer);

  // Sometimes the names we use in a .proto file happen to be defined as
  // macros on some platforms (e.g., macro/minor used in plugin.proto are
  // defined as macros in sys/types.h on FreeBSD and a few other platforms).
  // To make the generated code compile on these platforms, we either have to
  // undef the macro for these few platforms, or rename the field name for all
  // platforms. Since these names are part of protobuf public API, renaming is
  // generally a breaking change so we prefer the #undef approach.
  void GenerateMacroUndefs(io::Printer* printer);

  bool IsSCCRepresentative(const Descriptor* d) {
    return GetSCCRepresentative(d) == d;
  }
  const Descriptor* GetSCCRepresentative(const Descriptor* d) {
    return GetSCC(d)->GetRepresentative();
  }
  const SCC* GetSCC(const Descriptor* d) { return scc_analyzer_.GetSCC(d); }

  bool IsDepWeak(const FileDescriptor* dep) const {
    if (weak_deps_.count(dep) != 0) {
      GOOGLE_CHECK(!options_.opensource_runtime);
      return true;
    }
    return false;
  }

  std::set<const FileDescriptor*> weak_deps_;
  std::vector<const SCC*> sccs_;

  const FileDescriptor* file_;
  const Options options_;

  MessageSCCAnalyzer scc_analyzer_;

  std::map<std::string, std::string> variables_;

  // Contains the post-order walk of all the messages (and child messages) in
  // this file. If you need a pre-order walk just reverse iterate.
  std::vector<std::unique_ptr<MessageGenerator>> message_generators_;
  std::vector<std::unique_ptr<EnumGenerator>> enum_generators_;
  std::vector<std::unique_ptr<ServiceGenerator>> service_generators_;
  std::vector<std::unique_ptr<ExtensionGenerator>> extension_generators_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FileGenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_COMPILER_CPP_FILE_H__
