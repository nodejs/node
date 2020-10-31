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

#include <google/protobuf/compiler/objectivec/objectivec_file.h>
#include <google/protobuf/compiler/objectivec/objectivec_enum.h>
#include <google/protobuf/compiler/objectivec/objectivec_extension.h>
#include <google/protobuf/compiler/objectivec/objectivec_message.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/strutil.h>
#include <algorithm> // std::find()
#include <iostream>
#include <sstream>

// NOTE: src/google/protobuf/compiler/plugin.cc makes use of cerr for some
// error cases, so it seems to be ok to use as a back door for errors.

namespace google {
namespace protobuf {
namespace compiler {
namespace objectivec {

namespace {

// This is also found in GPBBootstrap.h, and needs to be kept in sync.
const int32 GOOGLE_PROTOBUF_OBJC_VERSION = 30002;

const char* kHeaderExtension = ".pbobjc.h";

// Checks if a message contains any enums definitions (on the message or
// a nested message under it).
bool MessageContainsEnums(const Descriptor* message) {
  if (message->enum_type_count() > 0) {
    return true;
  }
  for (int i = 0; i < message->nested_type_count(); i++) {
    if (MessageContainsEnums(message->nested_type(i))) {
      return true;
    }
  }
  return false;
}

// Checks if a message contains any extension definitions (on the message or
// a nested message under it).
bool MessageContainsExtensions(const Descriptor* message) {
  if (message->extension_count() > 0) {
    return true;
  }
  for (int i = 0; i < message->nested_type_count(); i++) {
    if (MessageContainsExtensions(message->nested_type(i))) {
      return true;
    }
  }
  return false;
}

// Checks if the file contains any enum definitions (at the root or
// nested under a message).
bool FileContainsEnums(const FileDescriptor* file) {
  if (file->enum_type_count() > 0) {
    return true;
  }
  for (int i = 0; i < file->message_type_count(); i++) {
    if (MessageContainsEnums(file->message_type(i))) {
      return true;
    }
  }
  return false;
}

// Checks if the file contains any extensions definitions (at the root or
// nested under a message).
bool FileContainsExtensions(const FileDescriptor* file) {
  if (file->extension_count() > 0) {
    return true;
  }
  for (int i = 0; i < file->message_type_count(); i++) {
    if (MessageContainsExtensions(file->message_type(i))) {
      return true;
    }
  }
  return false;
}

// Helper for CollectMinimalFileDepsContainingExtensionsWorker that marks all
// deps as visited and prunes them from the needed files list.
void PruneFileAndDepsMarkingAsVisited(
    const FileDescriptor* file,
    std::vector<const FileDescriptor*>* files,
    std::set<const FileDescriptor*>* files_visited) {
  std::vector<const FileDescriptor*>::iterator iter =
      std::find(files->begin(), files->end(), file);
  if (iter != files->end()) {
    files->erase(iter);
  }
  files_visited->insert(file);
  for (int i = 0; i < file->dependency_count(); i++) {
    PruneFileAndDepsMarkingAsVisited(file->dependency(i), files, files_visited);
  }
}

// Helper for CollectMinimalFileDepsContainingExtensions.
void CollectMinimalFileDepsContainingExtensionsWorker(
    const FileDescriptor* file,
    std::vector<const FileDescriptor*>* files,
    std::set<const FileDescriptor*>* files_visited) {
  if (files_visited->find(file) != files_visited->end()) {
    return;
  }
  files_visited->insert(file);

  if (FileContainsExtensions(file)) {
    files->push_back(file);
    for (int i = 0; i < file->dependency_count(); i++) {
      const FileDescriptor* dep = file->dependency(i);
      PruneFileAndDepsMarkingAsVisited(dep, files, files_visited);
    }
  } else {
    for (int i = 0; i < file->dependency_count(); i++) {
      const FileDescriptor* dep = file->dependency(i);
      CollectMinimalFileDepsContainingExtensionsWorker(dep, files,
                                                       files_visited);
    }
  }
}

// Collect the deps of the given file that contain extensions. This can be used to
// create the chain of roots that need to be wired together.
//
// NOTE: If any changes are made to this and the supporting functions, you will
// need to manually validate what the generated code is for the test files:
//   objectivec/Tests/unittest_extension_chain_*.proto
// There are comments about what the expected code should be line and limited
// testing objectivec/Tests/GPBUnittestProtos2.m around compilation (#imports
// specifically).
void CollectMinimalFileDepsContainingExtensions(
    const FileDescriptor* file,
    std::vector<const FileDescriptor*>* files) {
  std::set<const FileDescriptor*> files_visited;
  for (int i = 0; i < file->dependency_count(); i++) {
    const FileDescriptor* dep = file->dependency(i);
    CollectMinimalFileDepsContainingExtensionsWorker(dep, files,
                                                     &files_visited);
  }
}

bool IsDirectDependency(const FileDescriptor* dep, const FileDescriptor* file) {
  for (int i = 0; i < file->dependency_count(); i++) {
    if (dep == file->dependency(i)) {
      return true;
    }
  }
  return false;
}

}  // namespace

FileGenerator::FileGenerator(const FileDescriptor *file, const Options& options)
    : file_(file),
      root_class_name_(FileClassName(file)),
      is_bundled_proto_(IsProtobufLibraryBundledProtoFile(file)),
      options_(options) {
  for (int i = 0; i < file_->enum_type_count(); i++) {
    EnumGenerator *generator = new EnumGenerator(file_->enum_type(i));
    enum_generators_.emplace_back(generator);
  }
  for (int i = 0; i < file_->message_type_count(); i++) {
    MessageGenerator *generator =
        new MessageGenerator(root_class_name_, file_->message_type(i), options_);
    message_generators_.emplace_back(generator);
  }
  for (int i = 0; i < file_->extension_count(); i++) {
    ExtensionGenerator *generator =
        new ExtensionGenerator(root_class_name_, file_->extension(i));
    extension_generators_.emplace_back(generator);
  }
}

FileGenerator::~FileGenerator() {}

void FileGenerator::GenerateHeader(io::Printer *printer) {
  std::set<string> headers;
  // Generated files bundled with the library get minimal imports, everything
  // else gets the wrapper so everything is usable.
  if (is_bundled_proto_) {
    headers.insert("GPBRootObject.h");
    headers.insert("GPBMessage.h");
    headers.insert("GPBDescriptor.h");
  } else {
    headers.insert("GPBProtocolBuffers.h");
  }
  PrintFileRuntimePreamble(printer, headers);

  // Add some verification that the generated code matches the source the
  // code is being compiled with.
  // NOTE: This captures the raw numeric values at the time the generator was
  // compiled, since that will be the versions for the ObjC runtime at that
  // time.  The constants in the generated code will then get their values at
  // at compile time (so checking against the headers being used to compile).
  printer->Print(
      "#if GOOGLE_PROTOBUF_OBJC_VERSION < $google_protobuf_objc_version$\n"
      "#error This file was generated by a newer version of protoc which is incompatible with your Protocol Buffer library sources.\n"
      "#endif\n"
      "#if $google_protobuf_objc_version$ < GOOGLE_PROTOBUF_OBJC_MIN_SUPPORTED_VERSION\n"
      "#error This file was generated by an older version of protoc which is incompatible with your Protocol Buffer library sources.\n"
      "#endif\n"
      "\n",
      "google_protobuf_objc_version", StrCat(GOOGLE_PROTOBUF_OBJC_VERSION));

  // #import any headers for "public imports" in the proto file.
  {
    ImportWriter import_writer(
        options_.generate_for_named_framework,
        options_.named_framework_to_proto_path_mappings_path,
        is_bundled_proto_);
    const string header_extension(kHeaderExtension);
    for (int i = 0; i < file_->public_dependency_count(); i++) {
      import_writer.AddFile(file_->public_dependency(i), header_extension);
    }
    import_writer.Print(printer);
  }

  // Note:
  //  deprecated-declarations suppression is only needed if some place in this
  //    proto file is something deprecated or if it references something from
  //    another file that is deprecated.
  printer->Print(
      "// @@protoc_insertion_point(imports)\n"
      "\n"
      "#pragma clang diagnostic push\n"
      "#pragma clang diagnostic ignored \"-Wdeprecated-declarations\"\n"
      "\n"
      "CF_EXTERN_C_BEGIN\n"
      "\n");

  std::set<string> fwd_decls;
  for (const auto& generator : message_generators_) {
    generator->DetermineForwardDeclarations(&fwd_decls);
  }
  for (std::set<string>::const_iterator i(fwd_decls.begin());
       i != fwd_decls.end(); ++i) {
    printer->Print("$value$;\n", "value", *i);
  }
  if (fwd_decls.begin() != fwd_decls.end()) {
    printer->Print("\n");
  }

  printer->Print(
      "NS_ASSUME_NONNULL_BEGIN\n"
      "\n");

  // need to write out all enums first
  for (const auto& generator : enum_generators_) {
    generator->GenerateHeader(printer);
  }

  for (const auto& generator : message_generators_) {
    generator->GenerateEnumHeader(printer);
  }

  // For extensions to chain together, the Root gets created even if there
  // are no extensions.
  printer->Print(
      "#pragma mark - $root_class_name$\n"
      "\n"
      "/**\n"
      " * Exposes the extension registry for this file.\n"
      " *\n"
      " * The base class provides:\n"
      " * @code\n"
      " *   + (GPBExtensionRegistry *)extensionRegistry;\n"
      " * @endcode\n"
      " * which is a @c GPBExtensionRegistry that includes all the extensions defined by\n"
      " * this file and all files that it depends on.\n"
      " **/\n"
      "@interface $root_class_name$ : GPBRootObject\n"
      "@end\n"
      "\n",
      "root_class_name", root_class_name_);

  if (extension_generators_.size() > 0) {
    // The dynamic methods block is only needed if there are extensions.
    printer->Print(
        "@interface $root_class_name$ (DynamicMethods)\n",
        "root_class_name", root_class_name_);

    for (const auto& generator : extension_generators_) {
      generator->GenerateMembersHeader(printer);
    }

    printer->Print("@end\n\n");
  }  // extension_generators_.size() > 0

  for (const auto& generator : message_generators_) {
    generator->GenerateMessageHeader(printer);
  }

  printer->Print(
      "NS_ASSUME_NONNULL_END\n"
      "\n"
      "CF_EXTERN_C_END\n"
      "\n"
      "#pragma clang diagnostic pop\n"
      "\n"
      "// @@protoc_insertion_point(global_scope)\n");
}

void FileGenerator::GenerateSource(io::Printer *printer) {
  // #import the runtime support.
  std::set<string> headers;
  headers.insert("GPBProtocolBuffers_RuntimeSupport.h");
  PrintFileRuntimePreamble(printer, headers);

  // Enums use atomic in the generated code, so add the system import as needed.
  if (FileContainsEnums(file_)) {
    printer->Print(
        "#import <stdatomic.h>\n"
        "\n");
  }

  std::vector<const FileDescriptor*> deps_with_extensions;
  CollectMinimalFileDepsContainingExtensions(file_, &deps_with_extensions);

  {
    ImportWriter import_writer(
        options_.generate_for_named_framework,
        options_.named_framework_to_proto_path_mappings_path,
        is_bundled_proto_);
    const string header_extension(kHeaderExtension);

    // #import the header for this proto file.
    import_writer.AddFile(file_, header_extension);

    // #import the headers for anything that a plain dependency of this proto
    // file (that means they were just an include, not a "public" include).
    std::set<string> public_import_names;
    for (int i = 0; i < file_->public_dependency_count(); i++) {
      public_import_names.insert(file_->public_dependency(i)->name());
    }
    for (int i = 0; i < file_->dependency_count(); i++) {
      const FileDescriptor *dep = file_->dependency(i);
      bool public_import = (public_import_names.count(dep->name()) != 0);
      if (!public_import) {
        import_writer.AddFile(dep, header_extension);
      }
    }

    // If any indirect dependency provided extensions, it needs to be directly
    // imported so it can get merged into the root's extensions registry.
    // See the Note by CollectMinimalFileDepsContainingExtensions before
    // changing this.
    for (std::vector<const FileDescriptor *>::iterator iter =
             deps_with_extensions.begin();
         iter != deps_with_extensions.end(); ++iter) {
      if (!IsDirectDependency(*iter, file_)) {
        import_writer.AddFile(*iter, header_extension);
      }
    }

    import_writer.Print(printer);
  }

  bool includes_oneof = false;
  for (const auto& generator : message_generators_) {
    if (generator->IncludesOneOfDefinition()) {
      includes_oneof = true;
      break;
    }
  }

  // Note:
  //  deprecated-declarations suppression is only needed if some place in this
  //    proto file is something deprecated or if it references something from
  //    another file that is deprecated.
  printer->Print(
      "// @@protoc_insertion_point(imports)\n"
      "\n"
      "#pragma clang diagnostic push\n"
      "#pragma clang diagnostic ignored \"-Wdeprecated-declarations\"\n");
  if (includes_oneof) {
    // The generated code for oneof's uses direct ivar access, suppress the
    // warning incase developer turn that on in the context they compile the
    // generated code.
    printer->Print(
        "#pragma clang diagnostic ignored \"-Wdirect-ivar-access\"\n");
  }

  printer->Print(
      "\n"
      "#pragma mark - $root_class_name$\n"
      "\n"
      "@implementation $root_class_name$\n\n",
      "root_class_name", root_class_name_);

  const bool file_contains_extensions = FileContainsExtensions(file_);

  // If there were any extensions or this file has any dependencies, output
  // a registry to override to create the file specific registry.
  if (file_contains_extensions || !deps_with_extensions.empty()) {
    printer->Print(
        "+ (GPBExtensionRegistry*)extensionRegistry {\n"
        "  // This is called by +initialize so there is no need to worry\n"
        "  // about thread safety and initialization of registry.\n"
        "  static GPBExtensionRegistry* registry = nil;\n"
        "  if (!registry) {\n"
        "    GPB_DEBUG_CHECK_RUNTIME_VERSIONS();\n"
        "    registry = [[GPBExtensionRegistry alloc] init];\n");

    printer->Indent();
    printer->Indent();

    if (file_contains_extensions) {
      printer->Print(
          "static GPBExtensionDescription descriptions[] = {\n");
      printer->Indent();
      for (const auto& generator : extension_generators_) {
        generator->GenerateStaticVariablesInitialization(printer);
      }
      for (const auto& generator : message_generators_) {
        generator->GenerateStaticVariablesInitialization(printer);
      }
      printer->Outdent();
      printer->Print(
          "};\n"
          "for (size_t i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); ++i) {\n"
          "  GPBExtensionDescriptor *extension =\n"
          "      [[GPBExtensionDescriptor alloc] initWithExtensionDescription:&descriptions[i]];\n"
          "  [registry addExtension:extension];\n"
          "  [self globallyRegisterExtension:extension];\n"
          "  [extension release];\n"
          "}\n");
    }

    if (deps_with_extensions.empty()) {
      printer->Print(
          "// None of the imports (direct or indirect) defined extensions, so no need to add\n"
          "// them to this registry.\n");
    } else {
      printer->Print(
          "// Merge in the imports (direct or indirect) that defined extensions.\n");
      for (std::vector<const FileDescriptor *>::iterator iter =
               deps_with_extensions.begin();
           iter != deps_with_extensions.end(); ++iter) {
        const string root_class_name(FileClassName((*iter)));
        printer->Print(
            "[registry addExtensions:[$dependency$ extensionRegistry]];\n",
            "dependency", root_class_name);
      }
    }

    printer->Outdent();
    printer->Outdent();

    printer->Print(
        "  }\n"
        "  return registry;\n"
        "}\n");
  } else {
    if (file_->dependency_count() > 0) {
      printer->Print(
          "// No extensions in the file and none of the imports (direct or indirect)\n"
          "// defined extensions, so no need to generate +extensionRegistry.\n");
    } else {
      printer->Print(
          "// No extensions in the file and no imports, so no need to generate\n"
          "// +extensionRegistry.\n");
    }
  }

  printer->Print("\n@end\n\n");

  // File descriptor only needed if there are messages to use it.
  if (message_generators_.size() > 0) {
    std::map<string, string> vars;
    vars["root_class_name"] = root_class_name_;
    vars["package"] = file_->package();
    vars["objc_prefix"] = FileClassPrefix(file_);
    switch (file_->syntax()) {
      case FileDescriptor::SYNTAX_UNKNOWN:
        vars["syntax"] = "GPBFileSyntaxUnknown";
        break;
      case FileDescriptor::SYNTAX_PROTO2:
        vars["syntax"] = "GPBFileSyntaxProto2";
        break;
      case FileDescriptor::SYNTAX_PROTO3:
        vars["syntax"] = "GPBFileSyntaxProto3";
        break;
    }
    printer->Print(vars,
        "#pragma mark - $root_class_name$_FileDescriptor\n"
        "\n"
        "static GPBFileDescriptor *$root_class_name$_FileDescriptor(void) {\n"
        "  // This is called by +initialize so there is no need to worry\n"
        "  // about thread safety of the singleton.\n"
        "  static GPBFileDescriptor *descriptor = NULL;\n"
        "  if (!descriptor) {\n"
        "    GPB_DEBUG_CHECK_RUNTIME_VERSIONS();\n");
    if (vars["objc_prefix"].size() > 0) {
      printer->Print(
          vars,
          "    descriptor = [[GPBFileDescriptor alloc] initWithPackage:@\"$package$\"\n"
          "                                                 objcPrefix:@\"$objc_prefix$\"\n"
          "                                                     syntax:$syntax$];\n");
    } else {
      printer->Print(
          vars,
          "    descriptor = [[GPBFileDescriptor alloc] initWithPackage:@\"$package$\"\n"
          "                                                     syntax:$syntax$];\n");
    }
    printer->Print(
        "  }\n"
        "  return descriptor;\n"
        "}\n"
        "\n");
  }

  for (const auto& generator : enum_generators_) {
    generator->GenerateSource(printer);
  }
  for (const auto& generator : message_generators_) {
    generator->GenerateSource(printer);
  }

  printer->Print(
    "\n"
    "#pragma clang diagnostic pop\n"
    "\n"
    "// @@protoc_insertion_point(global_scope)\n");
}

// Helper to print the import of the runtime support at the top of generated
// files. This currently only supports the runtime coming from a framework
// as defined by the official CocoaPod.
void FileGenerator::PrintFileRuntimePreamble(
    io::Printer* printer, const std::set<string>& headers_to_import) const {
  printer->Print(
      "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "// source: $filename$\n"
      "\n",
      "filename", file_->name());

  const string framework_name(ProtobufLibraryFrameworkName);
  const string cpp_symbol(ProtobufFrameworkImportSymbol(framework_name));

  printer->Print(
      "// This CPP symbol can be defined to use imports that match up to the framework\n"
      "// imports needed when using CocoaPods.\n"
      "#if !defined($cpp_symbol$)\n"
      " #define $cpp_symbol$ 0\n"
      "#endif\n"
      "\n"
      "#if $cpp_symbol$\n",
      "cpp_symbol", cpp_symbol);


  for (std::set<string>::const_iterator iter = headers_to_import.begin();
       iter != headers_to_import.end(); ++iter) {
    printer->Print(
        " #import <$framework_name$/$header$>\n",
        "header", *iter,
        "framework_name", framework_name);
  }

  printer->Print(
      "#else\n");

  for (std::set<string>::const_iterator iter = headers_to_import.begin();
       iter != headers_to_import.end(); ++iter) {
    printer->Print(
        " #import \"$header$\"\n",
        "header", *iter);
  }

  printer->Print(
      "#endif\n"
      "\n");
}

}  // namespace objectivec
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
