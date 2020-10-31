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

#include <google/protobuf/compiler/cpp/cpp_service.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void InitMethodVariables(const MethodDescriptor* method, const Options& options,
                         Formatter* format) {
  format->Set("name", method->name());
  format->Set("input_type", QualifiedClassName(method->input_type(), options));
  format->Set("output_type",
              QualifiedClassName(method->output_type(), options));
}

}  // namespace

ServiceGenerator::ServiceGenerator(
    const ServiceDescriptor* descriptor,
    const std::map<std::string, std::string>& vars, const Options& options)
    : descriptor_(descriptor), vars_(vars), options_(options) {
  vars_["classname"] = descriptor_->name();
  vars_["full_name"] = descriptor_->full_name();
}

ServiceGenerator::~ServiceGenerator() {}

void ServiceGenerator::GenerateDeclarations(io::Printer* printer) {
  Formatter format(printer, vars_);
  // Forward-declare the stub type.
  format(
      "class $classname$_Stub;\n"
      "\n");

  GenerateInterface(printer);
  GenerateStubDefinition(printer);
}

void ServiceGenerator::GenerateInterface(io::Printer* printer) {
  Formatter format(printer, vars_);
  format(
      "class $dllexport_decl $$classname$ : public ::$proto_ns$::Service {\n"
      " protected:\n"
      "  // This class should be treated as an abstract interface.\n"
      "  inline $classname$() {};\n"
      " public:\n"
      "  virtual ~$classname$();\n");
  printer->Indent();

  format(
      "\n"
      "typedef $classname$_Stub Stub;\n"
      "\n"
      "static const ::$proto_ns$::ServiceDescriptor* descriptor();\n"
      "\n");

  GenerateMethodSignatures(VIRTUAL, printer);

  format(
      "\n"
      "// implements Service ----------------------------------------------\n"
      "\n"
      "const ::$proto_ns$::ServiceDescriptor* GetDescriptor();\n"
      "void CallMethod(const ::$proto_ns$::MethodDescriptor* method,\n"
      "                ::$proto_ns$::RpcController* controller,\n"
      "                const ::$proto_ns$::Message* request,\n"
      "                ::$proto_ns$::Message* response,\n"
      "                ::google::protobuf::Closure* done);\n"
      "const ::$proto_ns$::Message& GetRequestPrototype(\n"
      "  const ::$proto_ns$::MethodDescriptor* method) const;\n"
      "const ::$proto_ns$::Message& GetResponsePrototype(\n"
      "  const ::$proto_ns$::MethodDescriptor* method) const;\n");

  printer->Outdent();
  format(
      "\n"
      " private:\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS($classname$);\n"
      "};\n"
      "\n");
}

void ServiceGenerator::GenerateStubDefinition(io::Printer* printer) {
  Formatter format(printer, vars_);
  format(
      "class $dllexport_decl $$classname$_Stub : public $classname$ {\n"
      " public:\n");

  printer->Indent();

  format(
      "$classname$_Stub(::$proto_ns$::RpcChannel* channel);\n"
      "$classname$_Stub(::$proto_ns$::RpcChannel* channel,\n"
      "                 ::$proto_ns$::Service::ChannelOwnership ownership);\n"
      "~$classname$_Stub();\n"
      "\n"
      "inline ::$proto_ns$::RpcChannel* channel() { return channel_; }\n"
      "\n"
      "// implements $classname$ ------------------------------------------\n"
      "\n");

  GenerateMethodSignatures(NON_VIRTUAL, printer);

  printer->Outdent();
  format(
      " private:\n"
      "  ::$proto_ns$::RpcChannel* channel_;\n"
      "  bool owns_channel_;\n"
      "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS($classname$_Stub);\n"
      "};\n"
      "\n");
}

void ServiceGenerator::GenerateMethodSignatures(VirtualOrNon virtual_or_non,
                                                io::Printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor* method = descriptor_->method(i);
    Formatter format(printer, vars_);
    InitMethodVariables(method, options_, &format);
    format.Set("virtual", virtual_or_non == VIRTUAL ? "virtual " : "");
    format(
        "$virtual$void $name$(::$proto_ns$::RpcController* controller,\n"
        "                     const $input_type$* request,\n"
        "                     $output_type$* response,\n"
        "                     ::google::protobuf::Closure* done);\n");
  }
}

// ===================================================================

void ServiceGenerator::GenerateImplementation(io::Printer* printer) {
  Formatter format(printer, vars_);
  format(
      "$classname$::~$classname$() {}\n"
      "\n"
      "const ::$proto_ns$::ServiceDescriptor* $classname$::descriptor() {\n"
      "  "
      "::$proto_ns$::internal::AssignDescriptors(&$desc_table$);\n"
      "  return $file_level_service_descriptors$[$1$];\n"
      "}\n"
      "\n"
      "const ::$proto_ns$::ServiceDescriptor* $classname$::GetDescriptor() {\n"
      "  return descriptor();\n"
      "}\n"
      "\n",
      index_in_metadata_);

  // Generate methods of the interface.
  GenerateNotImplementedMethods(printer);
  GenerateCallMethod(printer);
  GenerateGetPrototype(REQUEST, printer);
  GenerateGetPrototype(RESPONSE, printer);

  // Generate stub implementation.
  format(
      "$classname$_Stub::$classname$_Stub(::$proto_ns$::RpcChannel* channel)\n"
      "  : channel_(channel), owns_channel_(false) {}\n"
      "$classname$_Stub::$classname$_Stub(\n"
      "    ::$proto_ns$::RpcChannel* channel,\n"
      "    ::$proto_ns$::Service::ChannelOwnership ownership)\n"
      "  : channel_(channel),\n"
      "    owns_channel_(ownership == "
      "::$proto_ns$::Service::STUB_OWNS_CHANNEL) "
      "{}\n"
      "$classname$_Stub::~$classname$_Stub() {\n"
      "  if (owns_channel_) delete channel_;\n"
      "}\n"
      "\n");

  GenerateStubMethods(printer);
}

void ServiceGenerator::GenerateNotImplementedMethods(io::Printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor* method = descriptor_->method(i);
    Formatter format(printer, vars_);
    InitMethodVariables(method, options_, &format);
    format(
        "void $classname$::$name$(::$proto_ns$::RpcController* controller,\n"
        "                         const $input_type$*,\n"
        "                         $output_type$*,\n"
        "                         ::google::protobuf::Closure* done) {\n"
        "  controller->SetFailed(\"Method $name$() not implemented.\");\n"
        "  done->Run();\n"
        "}\n"
        "\n");
  }
}

void ServiceGenerator::GenerateCallMethod(io::Printer* printer) {
  Formatter format(printer, vars_);
  format(
      "void $classname$::CallMethod(const ::$proto_ns$::MethodDescriptor* "
      "method,\n"
      "                             ::$proto_ns$::RpcController* controller,\n"
      "                             const ::$proto_ns$::Message* request,\n"
      "                             ::$proto_ns$::Message* response,\n"
      "                             ::google::protobuf::Closure* done) {\n"
      "  GOOGLE_DCHECK_EQ(method->service(), $file_level_service_descriptors$[$1$]);\n"
      "  switch(method->index()) {\n",
      index_in_metadata_);

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor* method = descriptor_->method(i);
    Formatter format(printer, vars_);
    InitMethodVariables(method, options_, &format);

    // Note:  down_cast does not work here because it only works on pointers,
    //   not references.
    format(
        "    case $1$:\n"
        "      $name$(controller,\n"
        "             ::$proto_ns$::internal::DownCast<const $input_type$*>(\n"
        "                 request),\n"
        "             ::$proto_ns$::internal::DownCast<$output_type$*>(\n"
        "                 response),\n"
        "             done);\n"
        "      break;\n",
        i);
  }

  format(
      "    default:\n"
      "      GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n"
      "      break;\n"
      "  }\n"
      "}\n"
      "\n");
}

void ServiceGenerator::GenerateGetPrototype(RequestOrResponse which,
                                            io::Printer* printer) {
  Formatter format(printer, vars_);
  if (which == REQUEST) {
    format("const ::$proto_ns$::Message& $classname$::GetRequestPrototype(\n");
  } else {
    format("const ::$proto_ns$::Message& $classname$::GetResponsePrototype(\n");
  }

  format(
      "    const ::$proto_ns$::MethodDescriptor* method) const {\n"
      "  GOOGLE_DCHECK_EQ(method->service(), descriptor());\n"
      "  switch(method->index()) {\n");

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor* method = descriptor_->method(i);
    const Descriptor* type =
        (which == REQUEST) ? method->input_type() : method->output_type();

    format(
        "    case $1$:\n"
        "      return $2$::default_instance();\n",
        i, QualifiedClassName(type, options_));
  }

  format(
      "    default:\n"
      "      GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n"
      "      return *::$proto_ns$::MessageFactory::generated_factory()\n"
      "          ->GetPrototype(method->$1$_type());\n"
      "  }\n"
      "}\n"
      "\n",
      which == REQUEST ? "input" : "output");
}

void ServiceGenerator::GenerateStubMethods(io::Printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const MethodDescriptor* method = descriptor_->method(i);
    Formatter format(printer, vars_);
    InitMethodVariables(method, options_, &format);
    format(
        "void $classname$_Stub::$name$(::$proto_ns$::RpcController* "
        "controller,\n"
        "                              const $input_type$* request,\n"
        "                              $output_type$* response,\n"
        "                              ::google::protobuf::Closure* done) {\n"
        "  channel_->CallMethod(descriptor()->method($1$),\n"
        "                       controller, request, response, done);\n"
        "}\n",
        i);
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
