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

#include <iomanip>
#include <sstream>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <google/protobuf/compiler/ruby/ruby_generator.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace ruby {

// Forward decls.
template <class numeric_type>
std::string NumberToString(numeric_type value);
std::string GetRequireName(const std::string& proto_file);
std::string LabelForField(FieldDescriptor* field);
std::string TypeName(FieldDescriptor* field);
bool GenerateMessage(const Descriptor* message, io::Printer* printer,
                     std::string* error);
void GenerateEnum(const EnumDescriptor* en, io::Printer* printer);
void GenerateMessageAssignment(const std::string& prefix,
                               const Descriptor* message, io::Printer* printer);
void GenerateEnumAssignment(const std::string& prefix, const EnumDescriptor* en,
                            io::Printer* printer);
std::string DefaultValueForField(const FieldDescriptor* field);

template<class numeric_type>
std::string NumberToString(numeric_type value) {
  std::ostringstream os;
  os << value;
  return os.str();
}

std::string GetRequireName(const std::string& proto_file) {
  int lastindex = proto_file.find_last_of(".");
  return proto_file.substr(0, lastindex) + "_pb";
}

std::string GetOutputFilename(const std::string& proto_file) {
  return GetRequireName(proto_file) + ".rb";
}

std::string LabelForField(const FieldDescriptor* field) {
  switch (field->label()) {
    case FieldDescriptor::LABEL_OPTIONAL: return "optional";
    case FieldDescriptor::LABEL_REQUIRED: return "required";
    case FieldDescriptor::LABEL_REPEATED: return "repeated";
    default: assert(false); return "";
  }
}

std::string TypeName(const FieldDescriptor* field) {
  switch (field->type()) {
    case FieldDescriptor::TYPE_INT32: return "int32";
    case FieldDescriptor::TYPE_INT64: return "int64";
    case FieldDescriptor::TYPE_UINT32: return "uint32";
    case FieldDescriptor::TYPE_UINT64: return "uint64";
    case FieldDescriptor::TYPE_SINT32: return "sint32";
    case FieldDescriptor::TYPE_SINT64: return "sint64";
    case FieldDescriptor::TYPE_FIXED32: return "fixed32";
    case FieldDescriptor::TYPE_FIXED64: return "fixed64";
    case FieldDescriptor::TYPE_SFIXED32: return "sfixed32";
    case FieldDescriptor::TYPE_SFIXED64: return "sfixed64";
    case FieldDescriptor::TYPE_DOUBLE: return "double";
    case FieldDescriptor::TYPE_FLOAT: return "float";
    case FieldDescriptor::TYPE_BOOL: return "bool";
    case FieldDescriptor::TYPE_ENUM: return "enum";
    case FieldDescriptor::TYPE_STRING: return "string";
    case FieldDescriptor::TYPE_BYTES: return "bytes";
    case FieldDescriptor::TYPE_MESSAGE: return "message";
    case FieldDescriptor::TYPE_GROUP: return "group";
    default: assert(false); return "";
  }
}

string StringifySyntax(FileDescriptor::Syntax syntax) {
  switch (syntax) {
    case FileDescriptor::SYNTAX_PROTO2:
      return "proto2";
    case FileDescriptor::SYNTAX_PROTO3:
      return "proto3";
    case FileDescriptor::SYNTAX_UNKNOWN:
    default:
      GOOGLE_LOG(FATAL) << "Unsupported syntax; this generator only supports "
                           "proto2 and proto3 syntax.";
      return "";
  }
}

std::string DefaultValueForField(const FieldDescriptor* field) {
  switch(field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return NumberToString(field->default_value_int32());
    case FieldDescriptor::CPPTYPE_INT64:
      return NumberToString(field->default_value_int64());
    case FieldDescriptor::CPPTYPE_UINT32:
      return NumberToString(field->default_value_uint32());
    case FieldDescriptor::CPPTYPE_UINT64:
      return NumberToString(field->default_value_uint64());
    case FieldDescriptor::CPPTYPE_FLOAT:
      return NumberToString(field->default_value_float());
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return NumberToString(field->default_value_double());
    case FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() ? "true" : "false";
    case FieldDescriptor::CPPTYPE_ENUM:
      return NumberToString(field->default_value_enum()->number());
    case FieldDescriptor::CPPTYPE_STRING: {
      std::ostringstream os;
      string default_str = field->default_value_string();

      if (field->type() == FieldDescriptor::TYPE_STRING) {
        os << "\"" << default_str << "\"";
      } else if (field->type() == FieldDescriptor::TYPE_BYTES) {
        os << "\"";

        os.fill('0');
        for (int i = 0; i < default_str.length(); ++i) {
          // Write the hex form of each byte.
          os << "\\x" << std::hex << std::setw(2)
             << ((uint16)((unsigned char)default_str.at(i)));
        }
        os << "\".force_encoding(\"ASCII-8BIT\")";
      }

      return os.str();
    }
    default: assert(false); return "";
  }
}

void GenerateField(const FieldDescriptor* field, io::Printer* printer) {
  if (field->is_map()) {
    const FieldDescriptor* key_field =
        field->message_type()->FindFieldByNumber(1);
    const FieldDescriptor* value_field =
        field->message_type()->FindFieldByNumber(2);

    printer->Print(
      "map :$name$, :$key_type$, :$value_type$, $number$",
      "name", field->name(),
      "key_type", TypeName(key_field),
      "value_type", TypeName(value_field),
      "number", NumberToString(field->number()));

    if (value_field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      printer->Print(
        ", \"$subtype$\"\n",
        "subtype", value_field->message_type()->full_name());
    } else if (value_field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      printer->Print(
        ", \"$subtype$\"\n",
        "subtype", value_field->enum_type()->full_name());
    } else {
      printer->Print("\n");
    }
  } else {

    printer->Print(
      "$label$ :$name$, ",
      "label", LabelForField(field),
      "name", field->name());
    printer->Print(
      ":$type$, $number$",
      "type", TypeName(field),
      "number", NumberToString(field->number()));

    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      printer->Print(
        ", \"$subtype$\"",
       "subtype", field->message_type()->full_name());
    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      printer->Print(
        ", \"$subtype$\"",
        "subtype", field->enum_type()->full_name());
    }

    if (field->has_default_value()) {
      printer->Print(", default: $default$", "default",
                     DefaultValueForField(field));
    }

    printer->Print("\n");
  }
}

void GenerateOneof(const OneofDescriptor* oneof, io::Printer* printer) {
  printer->Print(
      "oneof :$name$ do\n",
      "name", oneof->name());
  printer->Indent();

  for (int i = 0; i < oneof->field_count(); i++) {
    const FieldDescriptor* field = oneof->field(i);
    GenerateField(field, printer);
  }

  printer->Outdent();
  printer->Print("end\n");
}

bool GenerateMessage(const Descriptor* message, io::Printer* printer,
                     std::string* error) {
  if (message->extension_range_count() > 0 || message->extension_count() > 0) {
    *error = "Extensions are not yet supported for proto2 .proto files.";
    return false;
  }

  // Don't generate MapEntry messages -- we use the Ruby extension's native
  // support for map fields instead.
  if (message->options().map_entry()) {
    return true;
  }

  printer->Print(
    "add_message \"$name$\" do\n",
    "name", message->full_name());
  printer->Indent();

  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    if (!field->containing_oneof()) {
      GenerateField(field, printer);
    }
  }

  for (int i = 0; i < message->oneof_decl_count(); i++) {
    const OneofDescriptor* oneof = message->oneof_decl(i);
    GenerateOneof(oneof, printer);
  }

  printer->Outdent();
  printer->Print("end\n");

  for (int i = 0; i < message->nested_type_count(); i++) {
    if (!GenerateMessage(message->nested_type(i), printer, error)) {
      return false;
    }
  }
  for (int i = 0; i < message->enum_type_count(); i++) {
    GenerateEnum(message->enum_type(i), printer);
  }

  return true;
}

void GenerateEnum(const EnumDescriptor* en, io::Printer* printer) {
  printer->Print(
    "add_enum \"$name$\" do\n",
    "name", en->full_name());
  printer->Indent();

  for (int i = 0; i < en->value_count(); i++) {
    const EnumValueDescriptor* value = en->value(i);
    printer->Print(
      "value :$name$, $number$\n",
      "name", value->name(),
      "number", NumberToString(value->number()));
  }

  printer->Outdent();
  printer->Print(
    "end\n");
}

// Locale-agnostic utility functions.
bool IsLower(char ch) { return ch >= 'a' && ch <= 'z'; }

bool IsUpper(char ch) { return ch >= 'A' && ch <= 'Z'; }

bool IsAlpha(char ch) { return IsLower(ch) || IsUpper(ch); }

char UpperChar(char ch) { return IsLower(ch) ? (ch - 'a' + 'A') : ch; }


// Package names in protobuf are snake_case by convention, but Ruby module
// names must be PascalCased.
//
//   foo_bar_baz -> FooBarBaz
std::string PackageToModule(const std::string& name) {
  bool next_upper = true;
  std::string result;
  result.reserve(name.size());

  for (int i = 0; i < name.size(); i++) {
    if (name[i] == '_') {
      next_upper = true;
    } else {
      if (next_upper) {
        result.push_back(UpperChar(name[i]));
      } else {
        result.push_back(name[i]);
      }
      next_upper = false;
    }
  }

  return result;
}

// Class and enum names in protobuf should be PascalCased by convention, but
// since there is nothing enforcing this we need to ensure that they are valid
// Ruby constants.  That mainly means making sure that the first character is
// an upper-case letter.
std::string RubifyConstant(const std::string& name) {
  std::string ret = name;
  if (!ret.empty()) {
    if (IsLower(ret[0])) {
      // If it starts with a lowercase letter, capitalize it.
      ret[0] = UpperChar(ret[0]);
    } else if (!IsAlpha(ret[0])) {
      // Otherwise (e.g. if it begins with an underscore), we need to come up
      // with some prefix that starts with a capital letter. We could be smarter
      // here, e.g. try to strip leading underscores, but this may cause other
      // problems if the user really intended the name. So let's just prepend a
      // well-known suffix.
      ret = "PB_" + ret;
    }
  }

  return ret;
}

void GenerateMessageAssignment(const std::string& prefix,
                               const Descriptor* message,
                               io::Printer* printer) {
  // Don't generate MapEntry messages -- we use the Ruby extension's native
  // support for map fields instead.
  if (message->options().map_entry()) {
    return;
  }

  printer->Print(
    "$prefix$$name$ = ",
    "prefix", prefix,
    "name", RubifyConstant(message->name()));
  printer->Print(
    "Google::Protobuf::DescriptorPool.generated_pool."
    "lookup(\"$full_name$\").msgclass\n",
    "full_name", message->full_name());

  std::string nested_prefix = prefix + RubifyConstant(message->name()) + "::";
  for (int i = 0; i < message->nested_type_count(); i++) {
    GenerateMessageAssignment(nested_prefix, message->nested_type(i), printer);
  }
  for (int i = 0; i < message->enum_type_count(); i++) {
    GenerateEnumAssignment(nested_prefix, message->enum_type(i), printer);
  }
}

void GenerateEnumAssignment(const std::string& prefix, const EnumDescriptor* en,
                            io::Printer* printer) {
  printer->Print(
    "$prefix$$name$ = ",
    "prefix", prefix,
    "name", RubifyConstant(en->name()));
  printer->Print(
    "Google::Protobuf::DescriptorPool.generated_pool."
    "lookup(\"$full_name$\").enummodule\n",
    "full_name", en->full_name());
}

int GeneratePackageModules(const FileDescriptor* file, io::Printer* printer) {
  int levels = 0;
  bool need_change_to_module = true;
  std::string package_name;

  // Determine the name to use in either format:
  //   proto package:         one.two.three
  //   option ruby_package:   One::Two::Three
  if (file->options().has_ruby_package()) {
    package_name = file->options().ruby_package();

    // If :: is in the package use the Ruby formated name as-is
    //    -> A::B::C
    // otherwise, use the dot seperator
    //    -> A.B.C
    if (package_name.find("::") != std::string::npos) {
      need_change_to_module = false;
    } else {
      GOOGLE_LOG(WARNING) << "ruby_package option should be in the form of:"
                          << " 'A::B::C' and not 'A.B.C'";
    }
  } else {
    package_name = file->package();
  }

  // Use the appropriate delimter
  string delimiter = need_change_to_module ? "." : "::";
  int delimiter_size = need_change_to_module ? 1 : 2;

  // Extract each module name and indent
  while (!package_name.empty()) {
    size_t dot_index = package_name.find(delimiter);
    string component;
    if (dot_index == string::npos) {
      component = package_name;
      package_name = "";
    } else {
      component = package_name.substr(0, dot_index);
      package_name = package_name.substr(dot_index + delimiter_size);
    }
    if (need_change_to_module) {
      component = PackageToModule(component);
    }
    printer->Print(
      "module $name$\n",
      "name", component);
    printer->Indent();
    levels++;
  }
  return levels;
}

void EndPackageModules(int levels, io::Printer* printer) {
  while (levels > 0) {
    levels--;
    printer->Outdent();
    printer->Print(
      "end\n");
  }
}

bool UsesTypeFromFile(const Descriptor* message, const FileDescriptor* file,
                      string* error) {
  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    if ((field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
         field->message_type()->file() == file) ||
        (field->type() == FieldDescriptor::TYPE_ENUM &&
         field->enum_type()->file() == file)) {
      *error = "proto3 message field " + field->full_name() + " in file " +
               file->name() + " has a dependency on a type from proto2 file " +
               file->name() +
               ".  Ruby doesn't support proto2 yet, so we must fail.";
      return true;
    }
  }

  for (int i = 0; i < message->nested_type_count(); i++) {
    if (UsesTypeFromFile(message->nested_type(i), file, error)) {
      return true;
    }
  }

  return false;
}

// Ruby doesn't currently support proto2.  This causes a failure even for proto3
// files that import proto2.  But in some cases, the proto2 file is only being
// imported to extend another proto2 message.  The prime example is declaring
// custom options by extending FileOptions/FieldOptions/etc.
//
// If the proto3 messages don't have any proto2 submessages, it is safe to omit
// the dependency completely.  Users won't be able to use any proto2 extensions,
// but they already couldn't because proto2 messages aren't supported.
//
// If/when we add proto2 support, we should remove this.
bool MaybeEmitDependency(const FileDescriptor* import,
                         const FileDescriptor* from,
                         io::Printer* printer,
                         string* error) {
  if (from->syntax() == FileDescriptor::SYNTAX_PROTO3 &&
      import->syntax() == FileDescriptor::SYNTAX_PROTO2) {
    for (int i = 0; i < from->message_type_count(); i++) {
      if (UsesTypeFromFile(from->message_type(i), import, error)) {
        // Error text was already set by UsesTypeFromFile().
        return false;
      }
    }

    // Ok to omit this proto2 dependency -- so we won't print anything.
    GOOGLE_LOG(WARNING) << "Omitting proto2 dependency '" << import->name()
                        << "' from proto3 output file '"
                        << GetOutputFilename(from->name())
                        << "' because we don't support proto2 and no proto2 "
                           "types from that file are being used.";
    return true;
  } else {
    printer->Print(
      "require '$name$'\n", "name", GetRequireName(import->name()));
    return true;
  }
}

bool GenerateFile(const FileDescriptor* file, io::Printer* printer,
                  string* error) {
  printer->Print(
    "# Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "# source: $filename$\n"
    "\n",
    "filename", file->name());

  printer->Print(
    "require 'google/protobuf'\n\n");

  for (int i = 0; i < file->dependency_count(); i++) {
    if (!MaybeEmitDependency(file->dependency(i), file, printer, error)) {
      return false;
    }
  }

  // TODO: Remove this when ruby supports extensions for proto2 syntax.
  if (file->extension_count() > 0) {
    *error = "Extensions are not yet supported for proto2 .proto files.";
    return false;
  }

  printer->Print("Google::Protobuf::DescriptorPool.generated_pool.build do\n");
  printer->Indent();
  printer->Print("add_file(\"$filename$\", :syntax => :$syntax$) do\n",
		 "filename", file->name(), "syntax",
		 StringifySyntax(file->syntax()));
  printer->Indent();
  for (int i = 0; i < file->message_type_count(); i++) {
    if (!GenerateMessage(file->message_type(i), printer, error)) {
      return false;
    }
  }
  for (int i = 0; i < file->enum_type_count(); i++) {
    GenerateEnum(file->enum_type(i), printer);
  }
  printer->Outdent();
  printer->Print("end\n");
  printer->Outdent();
  printer->Print(
    "end\n\n");

  int levels = GeneratePackageModules(file, printer);
  for (int i = 0; i < file->message_type_count(); i++) {
    GenerateMessageAssignment("", file->message_type(i), printer);
  }
  for (int i = 0; i < file->enum_type_count(); i++) {
    GenerateEnumAssignment("", file->enum_type(i), printer);
  }
  EndPackageModules(levels, printer);
  return true;
}

bool Generator::Generate(
    const FileDescriptor* file,
    const string& parameter,
    GeneratorContext* generator_context,
    string* error) const {

  if (file->syntax() != FileDescriptor::SYNTAX_PROTO3 &&
      file->syntax() != FileDescriptor::SYNTAX_PROTO2) {
    *error = "Invalid or unsupported proto syntax";
    return false;
  }

  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(GetOutputFilename(file->name())));
  io::Printer printer(output.get(), '$');

  return GenerateFile(file, &printer, error);
}

}  // namespace ruby
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
