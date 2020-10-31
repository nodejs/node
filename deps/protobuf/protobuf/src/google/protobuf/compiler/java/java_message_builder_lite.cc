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

#include <google/protobuf/compiler/java/java_message_builder_lite.h>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <google/protobuf/compiler/java/java_context.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_enum.h>
#include <google/protobuf/compiler/java/java_extension.h>
#include <google/protobuf/compiler/java/java_generator_factory.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace java {

MessageBuilderLiteGenerator::MessageBuilderLiteGenerator(
    const Descriptor* descriptor, Context* context)
    : descriptor_(descriptor),
      context_(context),
      name_resolver_(context->GetNameResolver()),
      field_generators_(descriptor, context_) {
  GOOGLE_CHECK(!HasDescriptorMethods(descriptor->file(), context->EnforceLite()))
      << "Generator factory error: A lite message generator is used to "
         "generate non-lite messages.";
}

MessageBuilderLiteGenerator::~MessageBuilderLiteGenerator() {}

void MessageBuilderLiteGenerator::Generate(io::Printer* printer) {
  WriteMessageDocComment(printer, descriptor_);
  printer->Print(
      "public static final class Builder extends\n"
      "    com.google.protobuf.GeneratedMessageLite.$extendible$Builder<\n"
      "      $classname$, Builder> implements\n"
      "    $extra_interfaces$\n"
      "    $classname$OrBuilder {\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_),
      "extra_interfaces", ExtraBuilderInterfaces(descriptor_), "extendible",
      descriptor_->extension_range_count() > 0 ? "Extendable" : "");
  printer->Indent();

  GenerateCommonBuilderMethods(printer);

  // oneof
  std::map<std::string, std::string> vars;
  for (int i = 0; i < descriptor_->oneof_decl_count(); i++) {
    vars["oneof_name"] =
        context_->GetOneofGeneratorInfo(descriptor_->oneof_decl(i))->name;
    vars["oneof_capitalized_name"] =
        context_->GetOneofGeneratorInfo(descriptor_->oneof_decl(i))
            ->capitalized_name;
    vars["oneof_index"] = StrCat(descriptor_->oneof_decl(i)->index());

    // oneofCase() and clearOneof()
    printer->Print(vars,
                   "@java.lang.Override\n"
                   "public $oneof_capitalized_name$Case\n"
                   "    get$oneof_capitalized_name$Case() {\n"
                   "  return instance.get$oneof_capitalized_name$Case();\n"
                   "}\n"
                   "\n"
                   "public Builder clear$oneof_capitalized_name$() {\n"
                   "  copyOnWrite();\n"
                   "  instance.clear$oneof_capitalized_name$();\n"
                   "  return this;\n"
                   "}\n"
                   "\n");
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    printer->Print("\n");
    field_generators_.get(descriptor_->field(i))
        .GenerateBuilderMembers(printer);
  }

  printer->Print(
      "\n"
      "// @@protoc_insertion_point(builder_scope:$full_name$)\n",
      "full_name", descriptor_->full_name());

  printer->Outdent();
  printer->Print("}\n");
}

// ===================================================================

void MessageBuilderLiteGenerator::GenerateCommonBuilderMethods(
    io::Printer* printer) {
  printer->Print(
      "// Construct using $classname$.newBuilder()\n"
      "private Builder() {\n"
      "  super(DEFAULT_INSTANCE);\n"
      "}\n"
      "\n",
      "classname", name_resolver_->GetImmutableClassName(descriptor_));
}

// ===================================================================

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
