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

#include <google/protobuf/compiler/cpp/cpp_file.h>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include <google/protobuf/compiler/cpp/cpp_enum.h>
#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_message.h>
#include <google/protobuf/compiler/cpp/cpp_service.h>
#include <google/protobuf/compiler/scc.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>



#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

// When we forward-declare things, we want to create a sorted order so our
// output is deterministic and minimizes namespace changes.
template <class T>
std::string GetSortKey(const T& val) {
  return val.full_name();
}

template <>
std::string GetSortKey<FileDescriptor>(const FileDescriptor& val) {
  return val.name();
}

template <>
std::string GetSortKey<SCC>(const SCC& val) {
  return val.GetRepresentative()->full_name();
}

template <class T>
bool CompareSortKeys(const T* a, const T* b) {
  return GetSortKey(*a) < GetSortKey(*b);
}

template <class T>
std::vector<const T*> Sorted(const std::unordered_set<const T*>& vals) {
  std::vector<const T*> sorted(vals.begin(), vals.end());
  std::sort(sorted.begin(), sorted.end(), CompareSortKeys<T>);
  return sorted;
}

}  // namespace

FileGenerator::FileGenerator(const FileDescriptor* file, const Options& options)
    : file_(file), options_(options), scc_analyzer_(options) {
  // These variables are the same on a file level
  SetCommonVars(options, &variables_);
  variables_["dllexport_decl"] = options.dllexport_decl;
  variables_["tablename"] = UniqueName("TableStruct", file_, options_);
  variables_["file_level_metadata"] =
      UniqueName("file_level_metadata", file_, options_);
  variables_["desc_table"] = DescriptorTableName(file_, options_);
  variables_["file_level_enum_descriptors"] =
      UniqueName("file_level_enum_descriptors", file_, options_);
  variables_["file_level_service_descriptors"] =
      UniqueName("file_level_service_descriptors", file_, options_);
  variables_["filename"] = file_->name();
  variables_["package_ns"] = Namespace(file_, options);

  std::vector<const Descriptor*> msgs = FlattenMessagesInFile(file);
  for (int i = 0; i < msgs.size(); i++) {
    // Deleted in destructor
    MessageGenerator* msg_gen =
        new MessageGenerator(msgs[i], variables_, i, options, &scc_analyzer_);
    message_generators_.emplace_back(msg_gen);
    msg_gen->AddGenerators(&enum_generators_, &extension_generators_);
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    enum_generators_.emplace_back(
        new EnumGenerator(file->enum_type(i), variables_, options));
  }

  for (int i = 0; i < file->service_count(); i++) {
    service_generators_.emplace_back(
        new ServiceGenerator(file->service(i), variables_, options));
  }
  if (HasGenericServices(file_, options_)) {
    for (int i = 0; i < service_generators_.size(); i++) {
      service_generators_[i]->index_in_metadata_ = i;
    }
  }
  for (int i = 0; i < file->extension_count(); i++) {
    extension_generators_.emplace_back(
        new ExtensionGenerator(file->extension(i), options));
  }
  for (int i = 0; i < file->weak_dependency_count(); ++i) {
    weak_deps_.insert(file->weak_dependency(i));
  }
  for (int i = 0; i < message_generators_.size(); i++) {
    if (IsSCCRepresentative(message_generators_[i]->descriptor_)) {
      sccs_.push_back(GetSCC(message_generators_[i]->descriptor_));
    }
  }

  std::sort(sccs_.begin(), sccs_.end(), CompareSortKeys<SCC>);
}

FileGenerator::~FileGenerator() = default;

void FileGenerator::GenerateMacroUndefs(io::Printer* printer) {
  Formatter format(printer, variables_);
  // Only do this for protobuf's own types. There are some google3 protos using
  // macros as field names and the generated code compiles after the macro
  // expansion. Undefing these macros actually breaks such code.
  if (file_->name() != "net/proto2/compiler/proto/plugin.proto" &&
      file_->name() != "google/protobuf/compiler/plugin.proto") {
    return;
  }
  std::vector<std::string> names_to_undef;
  std::vector<const FieldDescriptor*> fields;
  ListAllFields(file_, &fields);
  for (int i = 0; i < fields.size(); i++) {
    const std::string& name = fields[i]->name();
    static const char* kMacroNames[] = {"major", "minor"};
    for (int i = 0; i < GOOGLE_ARRAYSIZE(kMacroNames); ++i) {
      if (name == kMacroNames[i]) {
        names_to_undef.push_back(name);
        break;
      }
    }
  }
  for (int i = 0; i < names_to_undef.size(); ++i) {
    format(
        "#ifdef $1$\n"
        "#undef $1$\n"
        "#endif\n",
        names_to_undef[i]);
  }
}

void FileGenerator::GenerateHeader(io::Printer* printer) {
  Formatter format(printer, variables_);

  // port_def.inc must be included after all other includes.
  IncludeFile("net/proto2/public/port_def.inc", printer);
  format("#define $1$$ dllexport_decl$\n", FileDllExport(file_, options_));
  GenerateMacroUndefs(printer);

  // For Any support with lite protos, we need to friend AnyMetadata, so we
  // forward-declare it here.
  format(
      "PROTOBUF_NAMESPACE_OPEN\n"
      "namespace internal {\n"
      "class AnyMetadata;\n"
      "}  // namespace internal\n"
      "PROTOBUF_NAMESPACE_CLOSE\n");

  GenerateGlobalStateFunctionDeclarations(printer);

  GenerateForwardDeclarations(printer);

  {
    NamespaceOpener ns(Namespace(file_, options_), format);

    format("\n");

    GenerateEnumDefinitions(printer);

    format(kThickSeparator);
    format("\n");

    GenerateMessageDefinitions(printer);

    format("\n");
    format(kThickSeparator);
    format("\n");

    GenerateServiceDefinitions(printer);

    GenerateExtensionIdentifiers(printer);

    format("\n");
    format(kThickSeparator);
    format("\n");

    GenerateInlineFunctionDefinitions(printer);

    format(
        "\n"
        "// @@protoc_insertion_point(namespace_scope)\n"
        "\n");
  }

  // We need to specialize some templates in the ::google::protobuf namespace:
  GenerateProto2NamespaceEnumSpecializations(printer);

  format(
      "\n"
      "// @@protoc_insertion_point(global_scope)\n"
      "\n");
  IncludeFile("net/proto2/public/port_undef.inc", printer);
}

void FileGenerator::GenerateProtoHeader(io::Printer* printer,
                                        const std::string& info_path) {
  Formatter format(printer, variables_);
  if (!options_.proto_h) {
    return;
  }

  GenerateTopHeaderGuard(printer, false);

  if (!options_.opensource_runtime) {
    format(
        "#ifdef SWIG\n"
        "#error \"Do not SWIG-wrap protobufs.\"\n"
        "#endif  // SWIG\n"
        "\n");
  }

  if (IsBootstrapProto(options_, file_)) {
    format("// IWYU pragma: private, include \"$1$.proto.h\"\n\n",
           StripProto(file_->name()));
  }

  GenerateLibraryIncludes(printer);

  for (int i = 0; i < file_->public_dependency_count(); i++) {
    const FileDescriptor* dep = file_->public_dependency(i);
    format("#include \"$1$.proto.h\"\n", StripProto(dep->name()));
  }

  format("// @@protoc_insertion_point(includes)\n");

  GenerateMetadataPragma(printer, info_path);

  GenerateHeader(printer);

  GenerateBottomHeaderGuard(printer, false);
}

void FileGenerator::GeneratePBHeader(io::Printer* printer,
                                     const std::string& info_path) {
  Formatter format(printer, variables_);
  GenerateTopHeaderGuard(printer, true);

  if (options_.proto_h) {
    std::string target_basename = StripProto(file_->name());
    if (!options_.opensource_runtime) {
      GetBootstrapBasename(options_, target_basename, &target_basename);
    }
    format("#include \"$1$.proto.h\"  // IWYU pragma: export\n",
           target_basename);
  } else {
    GenerateLibraryIncludes(printer);
  }

  if (options_.transitive_pb_h) {
    GenerateDependencyIncludes(printer);
  }

  // This is unfortunately necessary for some plugins. I don't see why we
  // need two of the same insertion points.
  // TODO(gerbens) remove this.
  format("// @@protoc_insertion_point(includes)\n");

  GenerateMetadataPragma(printer, info_path);

  if (!options_.proto_h) {
    GenerateHeader(printer);
  } else {
    {
      NamespaceOpener ns(Namespace(file_, options_), format);
      format(
          "\n"
          "// @@protoc_insertion_point(namespace_scope)\n");
    }
    format(
        "\n"
        "// @@protoc_insertion_point(global_scope)\n"
        "\n");
  }

  GenerateBottomHeaderGuard(printer, true);
}

void FileGenerator::DoIncludeFile(const std::string& google3_name,
                                  bool do_export, io::Printer* printer) {
  Formatter format(printer, variables_);
  const std::string prefix = "net/proto2/";
  GOOGLE_CHECK(google3_name.find(prefix) == 0) << google3_name;

  if (options_.opensource_runtime) {
    std::string path = google3_name.substr(prefix.size());

    path = StringReplace(path, "internal/", "", false);
    path = StringReplace(path, "proto/", "", false);
    path = StringReplace(path, "public/", "", false);
    if (options_.runtime_include_base.empty()) {
      format("#include <google/protobuf/$1$>", path);
    } else {
      format("#include \"$1$google/protobuf/$2$\"",
             options_.runtime_include_base, path);
    }
  } else {
    format("#include \"$1$\"", google3_name);
  }

  if (do_export) {
    format("  // IWYU pragma: export");
  }

  format("\n");
}

std::string FileGenerator::CreateHeaderInclude(const std::string& basename,
                                               const FileDescriptor* file) {
  bool use_system_include = false;
  std::string name = basename;

  if (options_.opensource_runtime) {
    if (IsWellKnownMessage(file)) {
      if (options_.runtime_include_base.empty()) {
        use_system_include = true;
      } else {
        name = options_.runtime_include_base + basename;
      }
    }
  }

  std::string left = "\"";
  std::string right = "\"";
  if (use_system_include) {
    left = "<";
    right = ">";
  }
  return left + name + right;
}

void FileGenerator::GenerateSourceIncludes(io::Printer* printer) {
  Formatter format(printer, variables_);
  std::string target_basename = StripProto(file_->name());
  if (!options_.opensource_runtime) {
    GetBootstrapBasename(options_, target_basename, &target_basename);
  }
  target_basename += options_.proto_h ? ".proto.h" : ".pb.h";
  format(
      "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "// source: $filename$\n"
      "\n"
      "#include $1$\n"
      "\n"
      "#include <algorithm>\n"  // for swap()
      "\n",
      CreateHeaderInclude(target_basename, file_));

  if (options_.opensource_runtime) {
    DoIncludeFile("net/proto2/public/stubs/common.h", false, printer);
  }

  IncludeFile("net/proto2/io/public/coded_stream.h", printer);
  // TODO(gerbens) This is to include parse_context.h, we need a better way
  IncludeFile("net/proto2/public/extension_set.h", printer);
  IncludeFile("net/proto2/public/wire_format_lite.h", printer);

  // Unknown fields implementation in lite mode uses StringOutputStream
  if (!UseUnknownFieldSet(file_, options_) && !message_generators_.empty()) {
    IncludeFile("net/proto2/io/public/zero_copy_stream_impl_lite.h", printer);
  }

  if (HasDescriptorMethods(file_, options_)) {
    IncludeFile("net/proto2/public/descriptor.h", printer);
    IncludeFile("net/proto2/public/generated_message_reflection.h", printer);
    IncludeFile("net/proto2/public/reflection_ops.h", printer);
    IncludeFile("net/proto2/public/wire_format.h", printer);
  }
  if (IsProto2MessageSetFile(file_, options_)) {
    format(
        // Implementation of proto1 MessageSet API methods.
        "#include \"net/proto2/bridge/internal/message_set_util.h\"\n");
  }

  if (options_.proto_h) {
    // Use the smaller .proto.h files.
    for (int i = 0; i < file_->dependency_count(); i++) {
      const FileDescriptor* dep = file_->dependency(i);
      // Do not import weak deps.
      if (!options_.opensource_runtime && IsDepWeak(dep)) continue;
      std::string basename = StripProto(dep->name());
      if (IsBootstrapProto(options_, file_)) {
        GetBootstrapBasename(options_, basename, &basename);
      }
      format("#include \"$1$.proto.h\"\n", basename);
    }
  }

  format("// @@protoc_insertion_point(includes)\n");
  IncludeFile("net/proto2/public/port_def.inc", printer);
}

void FileGenerator::GenerateSourceDefaultInstance(int idx,
                                                  io::Printer* printer) {
  Formatter format(printer, variables_);
  format(
      "class $1$ {\n"
      " public:\n"
      "  ::$proto_ns$::internal::ExplicitlyConstructed<$2$> _instance;\n",
      DefaultInstanceType(message_generators_[idx]->descriptor_, options_),
      message_generators_[idx]->classname_);
  format.Indent();
  message_generators_[idx]->GenerateExtraDefaultFields(printer);
  format.Outdent();
  format("} $1$;\n",
         DefaultInstanceName(message_generators_[idx]->descriptor_, options_));
}

// A list of things defined in one .pb.cc file that we need to reference from
// another .pb.cc file.
struct FileGenerator::CrossFileReferences {
  // Populated if we are referencing from messages or files.
  std::unordered_set<const SCC*> strong_sccs;
  std::unordered_set<const SCC*> weak_sccs;
  std::unordered_set<const Descriptor*> weak_default_instances;

  // Only if we are referencing from files.
  std::unordered_set<const FileDescriptor*> strong_reflection_files;
  std::unordered_set<const FileDescriptor*> weak_reflection_files;
};

void FileGenerator::GetCrossFileReferencesForField(const FieldDescriptor* field,
                                                   CrossFileReferences* refs) {
  const Descriptor* msg = field->message_type();
  if (msg == nullptr) return;
  const SCC* scc = GetSCC(msg);

  if (IsImplicitWeakField(field, options_, &scc_analyzer_) ||
      IsWeak(field, options_)) {
    refs->weak_sccs.insert(scc);
    refs->weak_default_instances.insert(msg);
  } else {
    refs->strong_sccs.insert(scc);
    // We don't need to declare default instances, because it is declared in the
    // .proto.h file we imported.
  }
}

void FileGenerator::GetCrossFileReferencesForFile(const FileDescriptor* file,
                                                  CrossFileReferences* refs) {
  ForEachField(file, [this, refs](const FieldDescriptor* field) {
    GetCrossFileReferencesForField(field, refs);
  });

  if (!HasDescriptorMethods(file, options_)) return;

  for (int i = 0; i < file->dependency_count(); i++) {
    const FileDescriptor* dep = file->dependency(i);
    if (IsDepWeak(dep)) {
      refs->weak_reflection_files.insert(dep);
    } else {
      refs->strong_reflection_files.insert(dep);
    }
  }
}

// Generates references to variables defined in other files.
void FileGenerator::GenerateInternalForwardDeclarations(
    const CrossFileReferences& refs, io::Printer* printer) {
  Formatter format(printer, variables_);

  for (auto scc : Sorted(refs.strong_sccs)) {
    format("extern $1$ ::$proto_ns$::internal::SCCInfo<$2$> $3$;\n",
           FileDllExport(scc->GetFile(), options_), scc->children.size(),
           SccInfoSymbol(scc, options_));
  }

  for (auto scc : Sorted(refs.weak_sccs)) {
    format(
        "extern __attribute__((weak)) ::$proto_ns$::internal::SCCInfo<$1$> "
        "$2$;\n",
        scc->children.size(), SccInfoSymbol(scc, options_));
  }

  {
    NamespaceOpener ns(format);
    for (auto instance : Sorted(refs.weak_default_instances)) {
      ns.ChangeTo(Namespace(instance, options_));
      format("extern __attribute__((weak)) $1$ $2$;\n",
             DefaultInstanceType(instance, options_),
             DefaultInstanceName(instance, options_));
    }
  }

  for (auto file : Sorted(refs.weak_reflection_files)) {
    format(
        "extern __attribute__((weak)) const "
        "::$proto_ns$::internal::DescriptorTable $1$;\n",
        DescriptorTableName(file, options_));
  }
}

void FileGenerator::GenerateSourceForMessage(int idx, io::Printer* printer) {
  Formatter format(printer, variables_);
  GenerateSourceIncludes(printer);

  // Generate weak declarations. We do this for the whole strongly-connected
  // component (SCC), because we have a single InitDefaults* function for the
  // SCC.
  CrossFileReferences refs;
  for (const Descriptor* message :
       scc_analyzer_.GetSCC(message_generators_[idx]->descriptor_)
           ->descriptors) {
    ForEachField(message, [this, &refs](const FieldDescriptor* field) {
      GetCrossFileReferencesForField(field, &refs);
    });
  }
  GenerateInternalForwardDeclarations(refs, printer);

  if (IsSCCRepresentative(message_generators_[idx]->descriptor_)) {
    GenerateInitForSCC(GetSCC(message_generators_[idx]->descriptor_), printer);
  }

  {  // package namespace
    NamespaceOpener ns(Namespace(file_, options_), format);

    // Define default instances
    GenerateSourceDefaultInstance(idx, printer);
    if (options_.lite_implicit_weak_fields) {
      format("void $1$_ReferenceStrong() {}\n",
             message_generators_[idx]->classname_);
    }

    // Generate classes.
    format("\n");
    message_generators_[idx]->GenerateClassMethods(printer);

    format(
        "\n"
        "// @@protoc_insertion_point(namespace_scope)\n");
  }  // end package namespace

  {
    NamespaceOpener proto_ns(ProtobufNamespace(options_), format);
    message_generators_[idx]->GenerateSourceInProto2Namespace(printer);
  }

  format(
      "\n"
      "// @@protoc_insertion_point(global_scope)\n");
}

void FileGenerator::GenerateGlobalSource(io::Printer* printer) {
  Formatter format(printer, variables_);
  GenerateSourceIncludes(printer);

  {
    GenerateTables(printer);

    // Define the code to initialize reflection. This code uses a global
    // constructor to register reflection data with the runtime pre-main.
    if (HasDescriptorMethods(file_, options_)) {
      GenerateReflectionInitializationCode(printer);
    }
  }

  NamespaceOpener ns(Namespace(file_, options_), format);

  // Generate enums.
  for (int i = 0; i < enum_generators_.size(); i++) {
    enum_generators_[i]->GenerateMethods(i, printer);
  }

  // Define extensions.
  for (int i = 0; i < extension_generators_.size(); i++) {
    extension_generators_[i]->GenerateDefinition(printer);
  }

  if (HasGenericServices(file_, options_)) {
    // Generate services.
    for (int i = 0; i < service_generators_.size(); i++) {
      if (i == 0) format("\n");
      format(kThickSeparator);
      format("\n");
      service_generators_[i]->GenerateImplementation(printer);
    }
  }
}

void FileGenerator::GenerateSource(io::Printer* printer) {
  Formatter format(printer, variables_);
  GenerateSourceIncludes(printer);
  CrossFileReferences refs;
  GetCrossFileReferencesForFile(file_, &refs);
  GenerateInternalForwardDeclarations(refs, printer);

  {
    NamespaceOpener ns(Namespace(file_, options_), format);

    // Define default instances
    for (int i = 0; i < message_generators_.size(); i++) {
      GenerateSourceDefaultInstance(i, printer);
      if (options_.lite_implicit_weak_fields) {
        format("void $1$_ReferenceStrong() {}\n",
               message_generators_[i]->classname_);
      }
    }
  }

  {
    GenerateTables(printer);

    // Now generate the InitDefaults for each SCC.
    for (auto scc : sccs_) {
      GenerateInitForSCC(scc, printer);
    }

    if (HasDescriptorMethods(file_, options_)) {
      // Define the code to initialize reflection. This code uses a global
      // constructor to register reflection data with the runtime pre-main.
      GenerateReflectionInitializationCode(printer);
    }
  }

  {
    NamespaceOpener ns(Namespace(file_, options_), format);

    // Actually implement the protos

    // Generate enums.
    for (int i = 0; i < enum_generators_.size(); i++) {
      enum_generators_[i]->GenerateMethods(i, printer);
    }

    // Generate classes.
    for (int i = 0; i < message_generators_.size(); i++) {
      format("\n");
      format(kThickSeparator);
      format("\n");
      message_generators_[i]->GenerateClassMethods(printer);
    }

    if (HasGenericServices(file_, options_)) {
      // Generate services.
      for (int i = 0; i < service_generators_.size(); i++) {
        if (i == 0) format("\n");
        format(kThickSeparator);
        format("\n");
        service_generators_[i]->GenerateImplementation(printer);
      }
    }

    // Define extensions.
    for (int i = 0; i < extension_generators_.size(); i++) {
      extension_generators_[i]->GenerateDefinition(printer);
    }

    format(
        "\n"
        "// @@protoc_insertion_point(namespace_scope)\n");
  }

  {
    NamespaceOpener proto_ns(ProtobufNamespace(options_), format);
    for (int i = 0; i < message_generators_.size(); i++) {
      message_generators_[i]->GenerateSourceInProto2Namespace(printer);
    }
  }

  format(
      "\n"
      "// @@protoc_insertion_point(global_scope)\n");

  IncludeFile("net/proto2/public/port_undef.inc", printer);
}

void FileGenerator::GenerateReflectionInitializationCode(io::Printer* printer) {
  Formatter format(printer, variables_);

  if (!message_generators_.empty()) {
    format("static ::$proto_ns$::Metadata $file_level_metadata$[$1$];\n",
           message_generators_.size());
  } else {
    format(
        "static "
        "constexpr ::$proto_ns$::Metadata* $file_level_metadata$ = nullptr;\n");
  }
  if (!enum_generators_.empty()) {
    format(
        "static "
        "const ::$proto_ns$::EnumDescriptor* "
        "$file_level_enum_descriptors$[$1$];\n",
        enum_generators_.size());
  } else {
    format(
        "static "
        "constexpr ::$proto_ns$::EnumDescriptor const** "
        "$file_level_enum_descriptors$ = nullptr;\n");
  }
  if (HasGenericServices(file_, options_) && file_->service_count() > 0) {
    format(
        "static "
        "const ::$proto_ns$::ServiceDescriptor* "
        "$file_level_service_descriptors$[$1$];\n",
        file_->service_count());
  } else {
    format(
        "static "
        "constexpr ::$proto_ns$::ServiceDescriptor const** "
        "$file_level_service_descriptors$ = nullptr;\n");
  }

  if (!message_generators_.empty()) {
    format(
        "\n"
        "const $uint32$ $tablename$::offsets[] "
        "PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {\n");
    format.Indent();
    std::vector<std::pair<size_t, size_t> > pairs;
    pairs.reserve(message_generators_.size());
    for (int i = 0; i < message_generators_.size(); i++) {
      pairs.push_back(message_generators_[i]->GenerateOffsets(printer));
    }
    format.Outdent();
    format(
        "};\n"
        "static const ::$proto_ns$::internal::MigrationSchema schemas[] "
        "PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {\n");
    format.Indent();
    {
      int offset = 0;
      for (int i = 0; i < message_generators_.size(); i++) {
        message_generators_[i]->GenerateSchema(printer, offset,
                                               pairs[i].second);
        offset += pairs[i].first;
      }
    }
    format.Outdent();
    format(
        "};\n"
        "\nstatic "
        "::$proto_ns$::Message const * const file_default_instances[] = {\n");
    format.Indent();
    for (int i = 0; i < message_generators_.size(); i++) {
      const Descriptor* descriptor = message_generators_[i]->descriptor_;
      format(
          "reinterpret_cast<const "
          "::$proto_ns$::Message*>(&$1$::_$2$_default_instance_),\n",
          Namespace(descriptor, options_),  // 1
          ClassName(descriptor));           // 2
    }
    format.Outdent();
    format(
        "};\n"
        "\n");
  } else {
    // we still need these symbols to exist
    format(
        // MSVC doesn't like empty arrays, so we add a dummy.
        "const $uint32$ $tablename$::offsets[1] = {};\n"
        "static constexpr ::$proto_ns$::internal::MigrationSchema* schemas = "
        "nullptr;"
        "\n"
        "static constexpr ::$proto_ns$::Message* const* "
        "file_default_instances = nullptr;\n"
        "\n");
  }

  // ---------------------------------------------------------------

  // Embed the descriptor.  We simply serialize the entire
  // FileDescriptorProto/ and embed it as a string literal, which is parsed and
  // built into real descriptors at initialization time.
  const std::string protodef_name =
      UniqueName("descriptor_table_protodef", file_, options_);
  format("const char $1$[] PROTOBUF_SECTION_VARIABLE(protodesc_cold) =\n",
         protodef_name);
  format.Indent();
  FileDescriptorProto file_proto;
  file_->CopyTo(&file_proto);
  std::string file_data;
  file_proto.SerializeToString(&file_data);

  {
    if (file_data.size() > 65535) {
      // Workaround for MSVC: "Error C1091: compiler limit: string exceeds
      // 65535 bytes in length". Declare a static array of chars rather than
      // use a string literal. Only write 25 bytes per line.
      static const int kBytesPerLine = 25;
      format("{ ");
      for (int i = 0; i < file_data.size();) {
        for (int j = 0; j < kBytesPerLine && i < file_data.size(); ++i, ++j) {
          format("'$1$', ", CEscape(file_data.substr(i, 1)));
        }
        format("\n");
      }
      format("'\\0' }");  // null-terminate
    } else {
      // Only write 40 bytes per line.
      static const int kBytesPerLine = 40;
      for (int i = 0; i < file_data.size(); i += kBytesPerLine) {
        format(
            "\"$1$\"\n",
            EscapeTrigraphs(CEscape(file_data.substr(i, kBytesPerLine))));
      }
    }
    format(";\n");
  }
  format.Outdent();

  CrossFileReferences refs;
  GetCrossFileReferencesForFile(file_, &refs);
  int num_deps =
      refs.strong_reflection_files.size() + refs.weak_reflection_files.size();

  // Build array of DescriptorTable deps.
  format(
      "static const ::$proto_ns$::internal::DescriptorTable*const "
      "$desc_table$_deps[$1$] = {\n",
      std::max(num_deps, 1));

  for (auto dep : Sorted(refs.strong_reflection_files)) {
    format("  &::$1$,\n", DescriptorTableName(dep, options_));
  }
  for (auto dep : Sorted(refs.weak_reflection_files)) {
    format("  &::$1$,\n", DescriptorTableName(dep, options_));
  }

  format("};\n");

  // Build array of SCCs from this file.
  format(
      "static ::$proto_ns$::internal::SCCInfoBase*const "
      "$desc_table$_sccs[$1$] = {\n",
      std::max<int>(sccs_.size(), 1));

  for (auto scc : sccs_) {
    format("  &$1$.base,\n", SccInfoSymbol(scc, options_));
  }

  format("};\n");

  // The DescriptorTable itself.
  format(
      "static ::$proto_ns$::internal::once_flag $desc_table$_once;\n"
      "static bool $desc_table$_initialized = false;\n"
      "const ::$proto_ns$::internal::DescriptorTable $desc_table$ = {\n"
      "  &$desc_table$_initialized, $1$, \"$filename$\", $2$,\n"
      "  &$desc_table$_once, $desc_table$_sccs, $desc_table$_deps, $3$, $4$,\n"
      "  schemas, file_default_instances, $tablename$::offsets,\n"
      "  $file_level_metadata$, $5$, $file_level_enum_descriptors$, "
      "$file_level_service_descriptors$,\n"
      "};\n\n",
      protodef_name, file_data.size(), sccs_.size(), num_deps,
      message_generators_.size());

  // For descriptor.proto we want to avoid doing any dynamic initialization,
  // because in some situations that would otherwise pull in a lot of
  // unnecessary code that can't be stripped by --gc-sections. Descriptor
  // initialization will still be performed lazily when it's needed.
  if (file_->name() != "net/proto2/proto/descriptor.proto") {
    format(
        "// Force running AddDescriptors() at dynamic initialization time.\n"
        "static bool $1$ = ("
        "  ::$proto_ns$::internal::AddDescriptors(&$desc_table$), true);\n",
        UniqueName("dynamic_init_dummy", file_, options_));
  }
}

void FileGenerator::GenerateInitForSCC(const SCC* scc, io::Printer* printer) {
  Formatter format(printer, variables_);
  // We use static and not anonymous namespace because symbol names are
  // substantially shorter.
  format("static void InitDefaults$1$() {\n", SccInfoSymbol(scc, options_));

  if (options_.opensource_runtime) {
    format("  GOOGLE_PROTOBUF_VERIFY_VERSION;\n\n");
  }

  format.Indent();

  // First construct all the necessary default instances.
  for (int i = 0; i < message_generators_.size(); i++) {
    if (scc_analyzer_.GetSCC(message_generators_[i]->descriptor_) != scc) {
      continue;
    }
    // TODO(gerbens) This requires this function to be friend. Remove
    // the need for this.
    message_generators_[i]->GenerateFieldDefaultInstances(printer);
    format(
        "{\n"
        "  void* ptr = &$1$;\n"
        "  new (ptr) $2$();\n",
        QualifiedDefaultInstanceName(message_generators_[i]->descriptor_,
                                     options_),
        QualifiedClassName(message_generators_[i]->descriptor_, options_));
    if (options_.opensource_runtime &&
        !IsMapEntryMessage(message_generators_[i]->descriptor_)) {
      format(
          "  "
          "::PROTOBUF_NAMESPACE_ID::internal::OnShutdownDestroyMessage(ptr);"
          "\n");
    }
    format("}\n");
  }

  // TODO(gerbens) make default instances be the same as normal instances.
  // Default instances differ from normal instances because they have cross
  // linked message fields.
  for (int i = 0; i < message_generators_.size(); i++) {
    if (scc_analyzer_.GetSCC(message_generators_[i]->descriptor_) != scc) {
      continue;
    }
    format("$1$::InitAsDefaultInstance();\n",
           QualifiedClassName(message_generators_[i]->descriptor_, options_));
  }
  format.Outdent();
  format("}\n\n");

  format(
      "$dllexport_decl $::$proto_ns$::internal::SCCInfo<$1$> $2$ =\n"
      "    "
      "{{ATOMIC_VAR_INIT(::$proto_ns$::internal::SCCInfoBase::kUninitialized), "
      "$1$, InitDefaults$2$}, {",
      scc->children.size(),  // 1
      SccInfoSymbol(scc, options_));
  for (const SCC* child : scc->children) {
    format("\n      &$1$.base,", SccInfoSymbol(child, options_));
  }
  format("}};\n\n");
}

void FileGenerator::GenerateTables(io::Printer* printer) {
  Formatter format(printer, variables_);
  if (options_.table_driven_parsing) {
    // TODO(ckennelly): Gate this with the same options flag to enable
    // table-driven parsing.
    format(
        "PROTOBUF_CONSTEXPR_VAR ::$proto_ns$::internal::ParseTableField\n"
        "    const $tablename$::entries[] "
        "PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {\n");
    format.Indent();

    std::vector<size_t> entries;
    size_t count = 0;
    for (int i = 0; i < message_generators_.size(); i++) {
      size_t value = message_generators_[i]->GenerateParseOffsets(printer);
      entries.push_back(value);
      count += value;
    }

    // We need these arrays to exist, and MSVC does not like empty arrays.
    if (count == 0) {
      format("{0, 0, 0, ::$proto_ns$::internal::kInvalidMask, 0, 0},\n");
    }

    format.Outdent();
    format(
        "};\n"
        "\n"
        "PROTOBUF_CONSTEXPR_VAR "
        "::$proto_ns$::internal::AuxillaryParseTableField\n"
        "    const $tablename$::aux[] "
        "PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {\n");
    format.Indent();

    std::vector<size_t> aux_entries;
    count = 0;
    for (int i = 0; i < message_generators_.size(); i++) {
      size_t value = message_generators_[i]->GenerateParseAuxTable(printer);
      aux_entries.push_back(value);
      count += value;
    }

    if (count == 0) {
      format("::$proto_ns$::internal::AuxillaryParseTableField(),\n");
    }

    format.Outdent();
    format(
        "};\n"
        "PROTOBUF_CONSTEXPR_VAR ::$proto_ns$::internal::ParseTable const\n"
        "    $tablename$::schema[] "
        "PROTOBUF_SECTION_VARIABLE(protodesc_cold) = {\n");
    format.Indent();

    size_t offset = 0;
    size_t aux_offset = 0;
    for (int i = 0; i < message_generators_.size(); i++) {
      message_generators_[i]->GenerateParseTable(printer, offset, aux_offset);
      offset += entries[i];
      aux_offset += aux_entries[i];
    }

    if (message_generators_.empty()) {
      format("{ nullptr, nullptr, 0, -1, -1, false },\n");
    }

    format.Outdent();
    format(
        "};\n"
        "\n");
  }

  if (!message_generators_.empty() && options_.table_driven_serialization) {
    format(
        "const ::$proto_ns$::internal::FieldMetadata "
        "$tablename$::field_metadata[] "
        "= {\n");
    format.Indent();
    std::vector<int> field_metadata_offsets;
    int idx = 0;
    for (int i = 0; i < message_generators_.size(); i++) {
      field_metadata_offsets.push_back(idx);
      idx += message_generators_[i]->GenerateFieldMetadata(printer);
    }
    field_metadata_offsets.push_back(idx);
    format.Outdent();
    format(
        "};\n"
        "const ::$proto_ns$::internal::SerializationTable "
        "$tablename$::serialization_table[] = {\n");
    format.Indent();
    // We rely on the order we layout the tables to match the order we
    // calculate them with FlattenMessagesInFile, so we check here that
    // these match exactly.
    std::vector<const Descriptor*> calculated_order =
        FlattenMessagesInFile(file_);
    GOOGLE_CHECK_EQ(calculated_order.size(), message_generators_.size());
    for (int i = 0; i < message_generators_.size(); i++) {
      GOOGLE_CHECK_EQ(calculated_order[i], message_generators_[i]->descriptor_);
      format("{$1$, $tablename$::field_metadata + $2$},\n",
             field_metadata_offsets[i + 1] - field_metadata_offsets[i],  // 1
             field_metadata_offsets[i]);                                 // 2
    }
    format.Outdent();
    format(
        "};\n"
        "\n");
  }
}

class FileGenerator::ForwardDeclarations {
 public:
  void AddMessage(const Descriptor* d) { classes_[ClassName(d)] = d; }
  void AddEnum(const EnumDescriptor* d) { enums_[ClassName(d)] = d; }

  void Print(const Formatter& format, const Options& options) const {
    for (const auto& p : enums_) {
      const std::string& enumname = p.first;
      const EnumDescriptor* enum_desc = p.second;
      format(
          "enum ${1$$2$$}$ : int;\n"
          "bool $2$_IsValid(int value);\n",
          enum_desc, enumname);
    }
    for (const auto& p : classes_) {
      const std::string& classname = p.first;
      const Descriptor* class_desc = p.second;
      format(
          "class ${1$$2$$}$;\n"
          "class $3$;\n"
          "$dllexport_decl $extern $3$ $4$;\n",
          class_desc, classname, DefaultInstanceType(class_desc, options),
          DefaultInstanceName(class_desc, options));
      if (options.lite_implicit_weak_fields) {
        format("void $1$_ReferenceStrong();\n", classname);
      }
    }
  }

  void PrintTopLevelDecl(const Formatter& format,
                         const Options& options) const {
    for (const auto& pair : classes_) {
      format(
          "template<> $dllexport_decl $"
          "$1$* Arena::CreateMaybeMessage<$1$>(Arena*);\n",
          QualifiedClassName(pair.second, options));
    }
  }

 private:
  std::map<std::string, const Descriptor*> classes_;
  std::map<std::string, const EnumDescriptor*> enums_;
};

static void PublicImportDFS(const FileDescriptor* fd,
                            std::unordered_set<const FileDescriptor*>* fd_set) {
  for (int i = 0; i < fd->public_dependency_count(); i++) {
    const FileDescriptor* dep = fd->public_dependency(i);
    if (fd_set->insert(dep).second) PublicImportDFS(dep, fd_set);
  }
}

void FileGenerator::GenerateForwardDeclarations(io::Printer* printer) {
  Formatter format(printer, variables_);
  std::vector<const Descriptor*> classes;
  std::vector<const EnumDescriptor*> enums;

  FlattenMessagesInFile(file_, &classes);  // All messages need forward decls.

  if (options_.proto_h) {  // proto.h needs extra forward declarations.
    // All classes / enums refered to as field members
    std::vector<const FieldDescriptor*> fields;
    ListAllFields(file_, &fields);
    for (int i = 0; i < fields.size(); i++) {
      classes.push_back(fields[i]->containing_type());
      classes.push_back(fields[i]->message_type());
      enums.push_back(fields[i]->enum_type());
    }
    ListAllTypesForServices(file_, &classes);
  }

  // Calculate the set of files whose definitions we get through include.
  // No need to forward declare types that are defined in these.
  std::unordered_set<const FileDescriptor*> public_set;
  PublicImportDFS(file_, &public_set);

  std::map<std::string, ForwardDeclarations> decls;
  for (int i = 0; i < classes.size(); i++) {
    const Descriptor* d = classes[i];
    if (d && !public_set.count(d->file()))
      decls[Namespace(d, options_)].AddMessage(d);
  }
  for (int i = 0; i < enums.size(); i++) {
    const EnumDescriptor* d = enums[i];
    if (d && !public_set.count(d->file()))
      decls[Namespace(d, options_)].AddEnum(d);
  }


  {
    NamespaceOpener ns(format);
    for (const auto& pair : decls) {
      ns.ChangeTo(pair.first);
      pair.second.Print(format, options_);
    }
  }
  format("PROTOBUF_NAMESPACE_OPEN\n");
  for (const auto& pair : decls) {
    pair.second.PrintTopLevelDecl(format, options_);
  }
  format("PROTOBUF_NAMESPACE_CLOSE\n");
}

void FileGenerator::GenerateTopHeaderGuard(io::Printer* printer, bool pb_h) {
  Formatter format(printer, variables_);
  // Generate top of header.
  format(
      "// Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
      "// source: $filename$\n"
      "\n"
      "#ifndef $1$\n"
      "#define $1$\n"
      "\n"
      "#include <limits>\n"
      "#include <string>\n",
      IncludeGuard(file_, pb_h, options_));
  if (!options_.opensource_runtime && !enum_generators_.empty()) {
    // Add header to provide std::is_integral for safe Enum_Name() function.
    format("#include <type_traits>\n");
  }
  format("\n");
}

void FileGenerator::GenerateBottomHeaderGuard(io::Printer* printer, bool pb_h) {
  Formatter format(printer, variables_);
  format("#endif  // $GOOGLE_PROTOBUF$_INCLUDED_$1$\n",
         IncludeGuard(file_, pb_h, options_));
}

void FileGenerator::GenerateLibraryIncludes(io::Printer* printer) {
  Formatter format(printer, variables_);
  if (UsingImplicitWeakFields(file_, options_)) {
    IncludeFile("net/proto2/public/implicit_weak_message.h", printer);
  }
  if (HasWeakFields(file_, options_)) {
    GOOGLE_CHECK(!options_.opensource_runtime);
    IncludeFile("net/proto2/public/weak_field_map.h", printer);
  }
  if (HasLazyFields(file_, options_)) {
    GOOGLE_CHECK(!options_.opensource_runtime);
    IncludeFile("net/proto2/public/lazy_field.h", printer);
  }

  if (options_.opensource_runtime) {
    // Verify the protobuf library header version is compatible with the protoc
    // version before going any further.
    IncludeFile("net/proto2/public/port_def.inc", printer);
    format(
        "#if PROTOBUF_VERSION < $1$\n"
        "#error This file was generated by a newer version of protoc which is\n"
        "#error incompatible with your Protocol Buffer headers. Please update\n"
        "#error your headers.\n"
        "#endif\n"
        "#if $2$ < PROTOBUF_MIN_PROTOC_VERSION\n"
        "#error This file was generated by an older version of protoc which "
        "is\n"
        "#error incompatible with your Protocol Buffer headers. Please\n"
        "#error regenerate this file with a newer version of protoc.\n"
        "#endif\n"
        "\n",
        PROTOBUF_MIN_HEADER_VERSION_FOR_PROTOC,  // 1
        PROTOBUF_VERSION);                       // 2
    IncludeFile("net/proto2/public/port_undef.inc", printer);
  }

  // OK, it's now safe to #include other files.
  IncludeFile("net/proto2/io/public/coded_stream.h", printer);
  IncludeFile("net/proto2/public/arena.h", printer);
  IncludeFile("net/proto2/public/arenastring.h", printer);
  IncludeFile("net/proto2/public/generated_message_table_driven.h", printer);
  IncludeFile("net/proto2/public/generated_message_util.h", printer);
  IncludeFile("net/proto2/public/inlined_string_field.h", printer);

  if (HasDescriptorMethods(file_, options_)) {
    IncludeFile("net/proto2/public/metadata.h", printer);
    IncludeFile("net/proto2/public/generated_message_reflection.h", printer);
  } else {
    IncludeFile("net/proto2/public/metadata_lite.h", printer);
  }

  if (!message_generators_.empty()) {
    if (HasDescriptorMethods(file_, options_)) {
      IncludeFile("net/proto2/public/message.h", printer);
    } else {
      IncludeFile("net/proto2/public/message_lite.h", printer);
    }
  }
  if (options_.opensource_runtime) {
    // Open-source relies on unconditional includes of these.
    IncludeFileAndExport("net/proto2/public/repeated_field.h", printer);
    IncludeFileAndExport("net/proto2/public/extension_set.h", printer);
  } else {
    // Google3 includes these files only when they are necessary.
    if (HasExtensionsOrExtendableMessage(file_)) {
      IncludeFileAndExport("net/proto2/public/extension_set.h", printer);
    }
    if (HasRepeatedFields(file_)) {
      IncludeFileAndExport("net/proto2/public/repeated_field.h", printer);
    }
    if (HasStringPieceFields(file_, options_)) {
      IncludeFile("net/proto2/public/string_piece_field_support.h", printer);
    }
    if (HasCordFields(file_, options_)) {
      format("#include \"third_party/absl/strings/cord.h\"\n");
    }
  }
  if (HasMapFields(file_)) {
    IncludeFileAndExport("net/proto2/public/map.h", printer);
    if (HasDescriptorMethods(file_, options_)) {
      IncludeFile("net/proto2/public/map_entry.h", printer);
      IncludeFile("net/proto2/public/map_field_inl.h", printer);
    } else {
      IncludeFile("net/proto2/public/map_entry_lite.h", printer);
      IncludeFile("net/proto2/public/map_field_lite.h", printer);
    }
  }

  if (HasEnumDefinitions(file_)) {
    if (HasDescriptorMethods(file_, options_)) {
      IncludeFile("net/proto2/public/generated_enum_reflection.h", printer);
    } else {
      IncludeFile("net/proto2/public/generated_enum_util.h", printer);
    }
  }

  if (HasGenericServices(file_, options_)) {
    IncludeFile("net/proto2/public/service.h", printer);
  }

  if (UseUnknownFieldSet(file_, options_) && !message_generators_.empty()) {
    IncludeFile("net/proto2/public/unknown_field_set.h", printer);
  }

  if (IsAnyMessage(file_, options_)) {
    IncludeFile("net/proto2/internal/any.h", printer);
  }
}

void FileGenerator::GenerateMetadataPragma(io::Printer* printer,
                                           const std::string& info_path) {
  Formatter format(printer, variables_);
  if (!info_path.empty() && !options_.annotation_pragma_name.empty() &&
      !options_.annotation_guard_name.empty()) {
    format.Set("guard", options_.annotation_guard_name);
    format.Set("pragma", options_.annotation_pragma_name);
    format.Set("info_path", info_path);
    format(
        "#ifdef $guard$\n"
        "#pragma $pragma$ \"$info_path$\"\n"
        "#endif  // $guard$\n");
  }
}

void FileGenerator::GenerateDependencyIncludes(io::Printer* printer) {
  Formatter format(printer, variables_);
  for (int i = 0; i < file_->dependency_count(); i++) {
    std::string basename = StripProto(file_->dependency(i)->name());

    // Do not import weak deps.
    if (IsDepWeak(file_->dependency(i))) continue;

    if (IsBootstrapProto(options_, file_)) {
      GetBootstrapBasename(options_, basename, &basename);
    }

    format("#include $1$\n",
           CreateHeaderInclude(basename + ".pb.h", file_->dependency(i)));
  }
}

void FileGenerator::GenerateGlobalStateFunctionDeclarations(
    io::Printer* printer) {
  Formatter format(printer, variables_);
  // Forward-declare the DescriptorTable because this is referenced by .pb.cc
  // files depending on this file.
  //
  // The TableStruct is also outputted in weak_message_field.cc, because the
  // weak fields must refer to table struct but cannot include the header.
  // Also it annotates extra weak attributes.
  // TODO(gerbens) make sure this situation is handled better.
  format(
      "\n"
      "// Internal implementation detail -- do not use these members.\n"
      "struct $dllexport_decl $$tablename$ {\n"
      // These tables describe how to serialize and parse messages. Used
      // for table driven code.
      "  static const ::$proto_ns$::internal::ParseTableField entries[]\n"
      "    PROTOBUF_SECTION_VARIABLE(protodesc_cold);\n"
      "  static const ::$proto_ns$::internal::AuxillaryParseTableField aux[]\n"
      "    PROTOBUF_SECTION_VARIABLE(protodesc_cold);\n"
      "  static const ::$proto_ns$::internal::ParseTable schema[$1$]\n"
      "    PROTOBUF_SECTION_VARIABLE(protodesc_cold);\n"
      "  static const ::$proto_ns$::internal::FieldMetadata field_metadata[];\n"
      "  static const ::$proto_ns$::internal::SerializationTable "
      "serialization_table[];\n"
      "  static const $uint32$ offsets[];\n"
      "};\n",
      std::max(size_t(1), message_generators_.size()));
  if (HasDescriptorMethods(file_, options_)) {
    format(
        "extern $dllexport_decl $const ::$proto_ns$::internal::DescriptorTable "
        "$desc_table$;\n");
  }
}

void FileGenerator::GenerateMessageDefinitions(io::Printer* printer) {
  Formatter format(printer, variables_);
  // Generate class definitions.
  for (int i = 0; i < message_generators_.size(); i++) {
    if (i > 0) {
      format("\n");
      format(kThinSeparator);
      format("\n");
    }
    message_generators_[i]->GenerateClassDefinition(printer);
  }
}

void FileGenerator::GenerateEnumDefinitions(io::Printer* printer) {
  // Generate enum definitions.
  for (int i = 0; i < enum_generators_.size(); i++) {
    enum_generators_[i]->GenerateDefinition(printer);
  }
}

void FileGenerator::GenerateServiceDefinitions(io::Printer* printer) {
  Formatter format(printer, variables_);
  if (HasGenericServices(file_, options_)) {
    // Generate service definitions.
    for (int i = 0; i < service_generators_.size(); i++) {
      if (i > 0) {
        format("\n");
        format(kThinSeparator);
        format("\n");
      }
      service_generators_[i]->GenerateDeclarations(printer);
    }

    format("\n");
    format(kThickSeparator);
    format("\n");
  }
}

void FileGenerator::GenerateExtensionIdentifiers(io::Printer* printer) {
  // Declare extension identifiers. These are in global scope and so only
  // the global scope extensions.
  for (auto& extension_generator : extension_generators_) {
    if (extension_generator->IsScoped()) continue;
    extension_generator->GenerateDeclaration(printer);
  }
}

void FileGenerator::GenerateInlineFunctionDefinitions(io::Printer* printer) {
  Formatter format(printer, variables_);
  // TODO(gerbens) remove pragmas when gcc is no longer used. Current version
  // of gcc fires a bogus error when compiled with strict-aliasing.
  format(
      "#ifdef __GNUC__\n"
      "  #pragma GCC diagnostic push\n"
      "  #pragma GCC diagnostic ignored \"-Wstrict-aliasing\"\n"
      "#endif  // __GNUC__\n");
  // Generate class inline methods.
  for (int i = 0; i < message_generators_.size(); i++) {
    if (i > 0) {
      format(kThinSeparator);
      format("\n");
    }
    message_generators_[i]->GenerateInlineMethods(printer);
  }
  format(
      "#ifdef __GNUC__\n"
      "  #pragma GCC diagnostic pop\n"
      "#endif  // __GNUC__\n");

  for (int i = 0; i < message_generators_.size(); i++) {
    if (i > 0) {
      format(kThinSeparator);
      format("\n");
    }
  }
}

void FileGenerator::GenerateProto2NamespaceEnumSpecializations(
    io::Printer* printer) {
  Formatter format(printer, variables_);
  // Emit GetEnumDescriptor specializations into google::protobuf namespace:
  if (HasEnumDefinitions(file_)) {
    format("\n");
    {
      NamespaceOpener proto_ns(ProtobufNamespace(options_), format);
      format("\n");
      for (int i = 0; i < enum_generators_.size(); i++) {
        enum_generators_[i]->GenerateGetEnumDescriptorSpecializations(printer);
      }
      format("\n");
    }
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
