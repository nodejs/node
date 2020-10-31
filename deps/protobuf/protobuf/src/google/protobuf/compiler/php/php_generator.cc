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

#include <google/protobuf/compiler/php/php_generator.h>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>

#include <sstream>

const std::string kDescriptorFile = "google/protobuf/descriptor.proto";
const std::string kEmptyFile = "google/protobuf/empty.proto";
const std::string kEmptyMetadataFile = "GPBMetadata/Google/Protobuf/GPBEmpty.php";
const std::string kDescriptorMetadataFile =
    "GPBMetadata/Google/Protobuf/Internal/Descriptor.php";
const std::string kDescriptorDirName = "Google/Protobuf/Internal";
const std::string kDescriptorPackageName = "Google\\Protobuf\\Internal";
const char* const kReservedNames[] = {
    "abstract",   "and",        "array",        "as",           "break",
    "callable",   "case",       "catch",        "class",        "clone",
    "const",      "continue",   "declare",      "default",      "die",
    "do",         "echo",       "else",         "elseif",       "empty",
    "enddeclare", "endfor",     "endforeach",   "endif",        "endswitch",
    "endwhile",   "eval",       "exit",         "extends",      "final",
    "for",        "foreach",    "function",     "global",       "goto",
    "if",         "implements", "include",      "include_once", "instanceof",
    "insteadof",  "interface",  "isset",        "list",         "namespace",
    "new",        "or",         "print",        "private",      "protected",
    "public",     "require",    "require_once", "return",       "static",
    "switch",     "throw",      "trait",        "try",          "unset",
    "use",        "var",        "while",        "xor",          "int",
    "float",      "bool",       "string",       "true",         "false",
    "null",       "void",       "iterable"};
const char* const kValidConstantNames[] = {
    "int",   "float", "bool", "string",   "true",
    "false", "null",  "void", "iterable",
};
const int kReservedNamesSize = 73;
const int kValidConstantNamesSize = 9;
const int kFieldSetter = 1;
const int kFieldGetter = 2;
const int kFieldProperty = 3;

namespace google {
namespace protobuf {
namespace compiler {
namespace php {

// Forward decls.
std::string PhpName(const std::string& full_name, bool is_descriptor);
std::string DefaultForField(FieldDescriptor* field);
std::string IntToString(int32 value);
std::string FilenameToClassname(const string& filename);
std::string GeneratedMetadataFileName(const FileDescriptor* file,
                                      bool is_descriptor);
std::string LabelForField(FieldDescriptor* field);
std::string TypeName(FieldDescriptor* field);
std::string UnderscoresToCamelCase(const string& name, bool cap_first_letter);
std::string EscapeDollor(const string& to_escape);
std::string BinaryToHex(const string& binary);
void Indent(io::Printer* printer);
void Outdent(io::Printer* printer);
void GenerateMessageDocComment(io::Printer* printer, const Descriptor* message,
                               int is_descriptor);
void GenerateMessageConstructorDocComment(io::Printer* printer,
                                          const Descriptor* message,
                                          int is_descriptor);
void GenerateFieldDocComment(io::Printer* printer, const FieldDescriptor* field,
                             int is_descriptor, int function_type);
void GenerateWrapperFieldGetterDocComment(io::Printer* printer,
                                          const FieldDescriptor* field);
void GenerateWrapperFieldSetterDocComment(io::Printer* printer,
                                          const FieldDescriptor* field);
void GenerateEnumDocComment(io::Printer* printer, const EnumDescriptor* enum_,
                            int is_descriptor);
void GenerateEnumValueDocComment(io::Printer* printer,
                                 const EnumValueDescriptor* value);
void GenerateServiceDocComment(io::Printer* printer,
                               const ServiceDescriptor* service);
void GenerateServiceMethodDocComment(io::Printer* printer,
                              const MethodDescriptor* method);


std::string ReservedNamePrefix(const string& classname,
                                const FileDescriptor* file) {
  bool is_reserved = false;

  string lower = classname;
  transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  for (int i = 0; i < kReservedNamesSize; i++) {
    if (lower == kReservedNames[i]) {
      is_reserved = true;
      break;
    }
  }

  if (is_reserved) {
    if (file->package() == "google.protobuf") {
      return "GPB";
    } else {
      return "PB";
    }
  }

  return "";
}

template <typename DescriptorType>
std::string DescriptorFullName(const DescriptorType* desc, bool is_descriptor) {
  if (is_descriptor) {
    return StringReplace(desc->full_name(),
                         "google.protobuf",
                         "google.protobuf.internal", false);
  } else {
    return desc->full_name();
  }
}

template <typename DescriptorType>
std::string ClassNamePrefix(const string& classname,
                            const DescriptorType* desc) {
  const string& prefix = (desc->file()->options()).php_class_prefix();
  if (prefix != "") {
    return prefix;
  }

  return ReservedNamePrefix(classname, desc->file());
}

template <typename DescriptorType>
std::string GeneratedClassNameImpl(const DescriptorType* desc) {
  std::string classname = ClassNamePrefix(desc->name(), desc) + desc->name();
  const Descriptor* containing = desc->containing_type();
  while (containing != NULL) {
    classname = ClassNamePrefix(containing->name(), desc) + containing->name()
       + '\\' + classname;
    containing = containing->containing_type();
  }
  return classname;
}

std::string GeneratedClassNameImpl(const ServiceDescriptor* desc) {
  std::string classname = desc->name();
  return ClassNamePrefix(classname, desc) + classname;
}

std::string GeneratedClassName(const Descriptor* desc) {
  return GeneratedClassNameImpl(desc);
}

std::string GeneratedClassName(const EnumDescriptor* desc) {
  return GeneratedClassNameImpl(desc);
}

std::string GeneratedClassName(const ServiceDescriptor* desc) {
  return GeneratedClassNameImpl(desc);
}

template <typename DescriptorType>
std::string LegacyGeneratedClassName(const DescriptorType* desc) {
  std::string classname = desc->name();
  const Descriptor* containing = desc->containing_type();
  while (containing != NULL) {
    classname = containing->name() + '_' + classname;
    containing = containing->containing_type();
  }
  return ClassNamePrefix(classname, desc) + classname;
}

std::string ClassNamePrefix(const string& classname) {
  string lower = classname;
  transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  for (int i = 0; i < kReservedNamesSize; i++) {
    if (lower == kReservedNames[i]) {
      return "PB";
    }
  }

  return "";
}

std::string ConstantNamePrefix(const string& classname) {
  bool is_reserved = false;

  string lower = classname;
  transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  for (int i = 0; i < kReservedNamesSize; i++) {
    if (lower == kReservedNames[i]) {
      is_reserved = true;
      break;
    }
  }

  for (int i = 0; i < kValidConstantNamesSize; i++) {
    if (lower == kValidConstantNames[i]) {
      is_reserved = false;
      break;
    }
  }

  if (is_reserved) {
    return "PB";
  }

  return "";
}

template <typename DescriptorType>
std::string RootPhpNamespace(const DescriptorType* desc, bool is_descriptor) {
  if (desc->file()->options().has_php_namespace()) {
    const string& php_namespace = desc->file()->options().php_namespace();
    if (php_namespace != "") {
      return php_namespace;
    }
    return "";
  }

  if (desc->file()->package() != "") {
    return PhpName(desc->file()->package(), is_descriptor);
  }
  return "";
}

template <typename DescriptorType>
std::string FullClassName(const DescriptorType* desc, bool is_descriptor) {
  string classname = GeneratedClassNameImpl(desc);
  string php_namespace = RootPhpNamespace(desc, is_descriptor);
  if (php_namespace != "") {
    return php_namespace + "\\" + classname;
  }
  return classname;
}

template <typename DescriptorType>
std::string LegacyFullClassName(const DescriptorType* desc, bool is_descriptor) {
  string classname = LegacyGeneratedClassName(desc);
  string php_namespace = RootPhpNamespace(desc, is_descriptor);
  if (php_namespace != "") {
    return php_namespace + "\\" + classname;
  }
  return classname;
}

std::string PhpName(const std::string& full_name, bool is_descriptor) {
  if (is_descriptor) {
    return kDescriptorPackageName;
  }

  std::string segment;
  std::string result;
  bool cap_next_letter = true;
  for (int i = 0; i < full_name.size(); i++) {
    if ('a' <= full_name[i] && full_name[i] <= 'z' && cap_next_letter) {
      segment += full_name[i] + ('A' - 'a');
      cap_next_letter = false;
    } else if (full_name[i] == '.') {
      result += ClassNamePrefix(segment) + segment + '\\';
      segment = "";
      cap_next_letter = true;
    } else {
      segment += full_name[i];
      cap_next_letter = false;
    }
  }
  result += ClassNamePrefix(segment) + segment;
  return result;
}

std::string DefaultForField(const FieldDescriptor* field) {
  switch (field->type()) {
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_SFIXED64:
    case FieldDescriptor::TYPE_ENUM: return "0";
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_FLOAT: return "0.0";
    case FieldDescriptor::TYPE_BOOL: return "false";
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_BYTES: return "''";
    case FieldDescriptor::TYPE_MESSAGE:
    case FieldDescriptor::TYPE_GROUP: return "null";
    default: assert(false); return "";
  }
}

std::string GeneratedMetadataFileName(const FileDescriptor* file,
                                      bool is_descriptor) {
  const string& proto_file = file->name();
  int start_index = 0;
  int first_index = proto_file.find_first_of("/", start_index);
  std::string result = "";
  std::string segment = "";

  if (proto_file == kEmptyFile) {
    return kEmptyMetadataFile;
  }
  if (is_descriptor) {
    return kDescriptorMetadataFile;
  }

  // Append directory name.
  std::string file_no_suffix;
  int lastindex = proto_file.find_last_of(".");
  if (proto_file == kEmptyFile) {
    return kEmptyMetadataFile;
  } else {
    file_no_suffix = proto_file.substr(0, lastindex);
  }

  if (file->options().has_php_metadata_namespace()) {
    const string& php_metadata_namespace =
        file->options().php_metadata_namespace();
    if (php_metadata_namespace != "" && php_metadata_namespace != "\\") {
      result += php_metadata_namespace;
      std::replace(result.begin(), result.end(), '\\', '/');
      if (result.at(result.size() - 1) != '/') {
        result += "/";
      }
    }
  } else {
    result += "GPBMetadata/";
    while (first_index != string::npos) {
      segment = UnderscoresToCamelCase(
          file_no_suffix.substr(start_index, first_index - start_index), true);
      result += ReservedNamePrefix(segment, file) + segment + "/";
      start_index = first_index + 1;
      first_index = file_no_suffix.find_first_of("/", start_index);
    }
  }

  // Append file name.
  int file_name_start = file_no_suffix.find_last_of("/");
  if (file_name_start == string::npos) {
    file_name_start = 0;
  } else {
    file_name_start += 1;
  }
  segment = UnderscoresToCamelCase(
      file_no_suffix.substr(file_name_start, first_index - file_name_start), true);

  return result + ReservedNamePrefix(segment, file) + segment + ".php";
}

template <typename DescriptorType>
std::string GeneratedClassFileName(const DescriptorType* desc,
                                     bool is_descriptor) {
  std::string result = FullClassName(desc, is_descriptor);
  for (int i = 0; i < result.size(); i++) {
    if (result[i] == '\\') {
      result[i] = '/';
    }
  }
  return result + ".php";
}

template <typename DescriptorType>
std::string LegacyGeneratedClassFileName(const DescriptorType* desc,
                                     bool is_descriptor) {
  std::string result = LegacyFullClassName(desc, is_descriptor);

  for (int i = 0; i < result.size(); i++) {
    if (result[i] == '\\') {
      result[i] = '/';
    }
  }
  return result + ".php";
}

std::string GeneratedServiceFileName(const ServiceDescriptor* service,
                                    bool is_descriptor) {
  std::string result = FullClassName(service, is_descriptor) + "Interface";
  for (int i = 0; i < result.size(); i++) {
    if (result[i] == '\\') {
      result[i] = '/';
    }
  }
  return result + ".php";
}

std::string IntToString(int32 value) {
  std::ostringstream os;
  os << value;
  return os.str();
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

std::string PhpSetterTypeName(const FieldDescriptor* field, bool is_descriptor) {
  if (field->is_map()) {
    return "array|\\Google\\Protobuf\\Internal\\MapField";
  }
  string type;
  switch (field->type()) {
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_ENUM:
      type = "int";
      break;
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED64:
      type = "int|string";
      break;
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_FLOAT:
      type = "float";
      break;
    case FieldDescriptor::TYPE_BOOL:
      type = "bool";
      break;
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_BYTES:
      type = "string";
      break;
    case FieldDescriptor::TYPE_MESSAGE:
      type = "\\" + FullClassName(field->message_type(), is_descriptor);
      break;
    case FieldDescriptor::TYPE_GROUP:
      return "null";
    default: assert(false); return "";
  }
  if (field->is_repeated()) {
    // accommodate for edge case with multiple types.
    size_t start_pos = type.find("|");
    if (start_pos != std::string::npos) {
      type.replace(start_pos, 1, "[]|");
    }
    type += "[]|\\Google\\Protobuf\\Internal\\RepeatedField";
  }
  return type;
}

std::string PhpGetterTypeName(const FieldDescriptor* field, bool is_descriptor) {
  if (field->is_map()) {
    return "\\Google\\Protobuf\\Internal\\MapField";
  }
  if (field->is_repeated()) {
    return "\\Google\\Protobuf\\Internal\\RepeatedField";
  }
  switch (field->type()) {
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SFIXED32:
    case FieldDescriptor::TYPE_ENUM: return "int";
    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED64: return "int|string";
    case FieldDescriptor::TYPE_DOUBLE:
    case FieldDescriptor::TYPE_FLOAT: return "float";
    case FieldDescriptor::TYPE_BOOL: return "bool";
    case FieldDescriptor::TYPE_STRING:
    case FieldDescriptor::TYPE_BYTES: return "string";
    case FieldDescriptor::TYPE_MESSAGE:
      return "\\" + FullClassName(field->message_type(), is_descriptor);
    case FieldDescriptor::TYPE_GROUP: return "null";
    default: assert(false); return "";
  }
}

std::string EnumOrMessageSuffix(
    const FieldDescriptor* field, bool is_descriptor) {
  if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
    return ", '" + DescriptorFullName(field->message_type(), is_descriptor) + "'";
  }
  if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
    return ", '" + DescriptorFullName(field->enum_type(), is_descriptor) + "'";
  }
  return "";
}

// Converts a name to camel-case. If cap_first_letter is true, capitalize the
// first letter.
std::string UnderscoresToCamelCase(const string& input, bool cap_first_letter) {
  std::string result;
  for (int i = 0; i < input.size(); i++) {
    if ('a' <= input[i] && input[i] <= 'z') {
      if (cap_first_letter) {
        result += input[i] + ('A' - 'a');
      } else {
        result += input[i];
      }
      cap_first_letter = false;
    } else if ('A' <= input[i] && input[i] <= 'Z') {
      if (i == 0 && !cap_first_letter) {
        // Force first letter to lower-case unless explicitly told to
        // capitalize it.
        result += input[i] + ('a' - 'A');
      } else {
        // Capital letters after the first are left as-is.
        result += input[i];
      }
      cap_first_letter = false;
    } else if ('0' <= input[i] && input[i] <= '9') {
      result += input[i];
      cap_first_letter = true;
    } else {
      cap_first_letter = true;
    }
  }
  // Add a trailing "_" if the name should be altered.
  if (input[input.size() - 1] == '#') {
    result += '_';
  }
  return result;
}

std::string EscapeDollor(const string& to_escape) {
  return StringReplace(to_escape, "$", "\\$", true);
}

std::string BinaryToHex(const string& src) {
  string dest;
  size_t i;
  unsigned char symbol[16] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'a', 'b',
    'c', 'd', 'e', 'f',
  };

  dest.resize(src.size() * 2);
  char* append_ptr = &dest[0];

  for (i = 0; i < src.size(); i++) {
    *append_ptr++ = symbol[(src[i] & 0xf0) >> 4];
    *append_ptr++ = symbol[src[i] & 0x0f];
  }

  return dest;
}

void Indent(io::Printer* printer) {
  printer->Indent();
  printer->Indent();
}
void Outdent(io::Printer* printer) {
  printer->Outdent();
  printer->Outdent();
}

void GenerateField(const FieldDescriptor* field, io::Printer* printer,
                   bool is_descriptor) {
  if (field->is_repeated()) {
    GenerateFieldDocComment(printer, field, is_descriptor, kFieldProperty);
    printer->Print(
        "private $^name^;\n",
        "name", field->name());
  } else if (field->containing_oneof()) {
    // Oneof fields are handled by GenerateOneofField.
    return;
  } else {
    GenerateFieldDocComment(printer, field, is_descriptor, kFieldProperty);
    printer->Print(
        "private $^name^ = ^default^;\n",
        "name", field->name(),
        "default", DefaultForField(field));
  }

  if (is_descriptor) {
    printer->Print(
        "private $has_^name^ = false;\n",
        "name", field->name());
  }
}

void GenerateOneofField(const OneofDescriptor* oneof, io::Printer* printer) {
  // Oneof property needs to be protected in order to be accessed by parent
  // class in implementation.
  printer->Print(
      "protected $^name^;\n",
      "name", oneof->name());
}

void GenerateFieldAccessor(const FieldDescriptor* field, bool is_descriptor,
                           io::Printer* printer) {
  const OneofDescriptor* oneof = field->containing_oneof();

  // Generate getter.
  if (oneof != NULL) {
    GenerateFieldDocComment(printer, field, is_descriptor, kFieldGetter);
    printer->Print(
        "public function get^camel_name^()\n"
        "{\n"
        "    return $this->readOneof(^number^);\n"
        "}\n\n",
        "camel_name", UnderscoresToCamelCase(field->name(), true),
        "number", IntToString(field->number()));
  } else {
    GenerateFieldDocComment(printer, field, is_descriptor, kFieldGetter);
    printer->Print(
        "public function get^camel_name^()\n"
        "{\n"
        "    return $this->^name^;\n"
        "}\n\n",
        "camel_name", UnderscoresToCamelCase(field->name(), true), "name",
        field->name());
  }

  // For wrapper types, generate an additional getXXXUnwrapped getter
  if (!field->is_map() &&
      !field->is_repeated() &&
      field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
      IsWrapperType(field)) {
    GenerateWrapperFieldGetterDocComment(printer, field);
    printer->Print(
        "public function get^camel_name^Unwrapped()\n"
        "{\n"
        "    $wrapper = $this->get^camel_name^();\n"
        "    return is_null($wrapper) ? null : $wrapper->getValue();\n"
        "}\n\n",
        "camel_name", UnderscoresToCamelCase(field->name(), true));
  }

  // Generate setter.
  GenerateFieldDocComment(printer, field, is_descriptor, kFieldSetter);
  printer->Print(
      "public function set^camel_name^($var)\n"
      "{\n",
      "camel_name", UnderscoresToCamelCase(field->name(), true));

  Indent(printer);

  // Type check.
  if (field->is_map()) {
    const Descriptor* map_entry = field->message_type();
    const FieldDescriptor* key = map_entry->FindFieldByName("key");
    const FieldDescriptor* value = map_entry->FindFieldByName("value");
    printer->Print(
        "$arr = GPBUtil::checkMapField($var, "
        "\\Google\\Protobuf\\Internal\\GPBType::^key_type^, "
        "\\Google\\Protobuf\\Internal\\GPBType::^value_type^",
        "key_type", ToUpper(key->type_name()),
        "value_type", ToUpper(value->type_name()));
    if (value->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      printer->Print(
          ", \\^class_name^);\n",
          "class_name",
          FullClassName(value->message_type(), is_descriptor) + "::class");
    } else if (value->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      printer->Print(
          ", \\^class_name^);\n",
          "class_name",
          FullClassName(value->enum_type(), is_descriptor) + "::class");
    } else {
      printer->Print(");\n");
    }
  } else if (field->is_repeated()) {
    printer->Print(
        "$arr = GPBUtil::checkRepeatedField($var, "
        "\\Google\\Protobuf\\Internal\\GPBType::^type^",
        "type", ToUpper(field->type_name()));
    if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
      printer->Print(
          ", \\^class_name^);\n",
          "class_name",
          FullClassName(field->message_type(), is_descriptor) + "::class");
    } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
      printer->Print(
          ", \\^class_name^);\n",
          "class_name",
          FullClassName(field->enum_type(), is_descriptor) + "::class");
    } else {
      printer->Print(");\n");
    }
  } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE) {
    printer->Print(
        "GPBUtil::checkMessage($var, \\^class_name^::class);\n",
        "class_name", LegacyFullClassName(field->message_type(), is_descriptor));
  } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_ENUM) {
    printer->Print(
        "GPBUtil::checkEnum($var, \\^class_name^::class);\n",
        "class_name", LegacyFullClassName(field->enum_type(), is_descriptor));
  } else if (field->cpp_type() == FieldDescriptor::CPPTYPE_STRING) {
    printer->Print(
        "GPBUtil::checkString($var, ^utf8^);\n",
        "utf8",
        field->type() == FieldDescriptor::TYPE_STRING ? "True": "False");
  } else {
    printer->Print(
        "GPBUtil::check^type^($var);\n",
        "type", UnderscoresToCamelCase(field->cpp_type_name(), true));
  }

  if (oneof != NULL) {
    printer->Print(
        "$this->writeOneof(^number^, $var);\n",
        "number", IntToString(field->number()));
  } else if (field->is_repeated()) {
    printer->Print(
        "$this->^name^ = $arr;\n",
        "name", field->name());
  } else {
    printer->Print(
        "$this->^name^ = $var;\n",
        "name", field->name());
  }

  // Set has bit for proto2 only.
  if (is_descriptor) {
    printer->Print(
        "$this->has_^field_name^ = true;\n",
        "field_name", field->name());
  }

  printer->Print("\nreturn $this;\n");

  Outdent(printer);

  printer->Print(
      "}\n\n");

  // For wrapper types, generate an additional setXXXValue getter
  if (!field->is_map() &&
      !field->is_repeated() &&
      field->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE &&
      IsWrapperType(field)) {
    GenerateWrapperFieldSetterDocComment(printer, field);
    printer->Print(
        "public function set^camel_name^Unwrapped($var)\n"
        "{\n"
        "    $wrappedVar = is_null($var) ? null : new \\^wrapper_type^(['value' => $var]);\n"
        "    return $this->set^camel_name^($wrappedVar);\n"
        "}\n\n",
        "camel_name", UnderscoresToCamelCase(field->name(), true),
        "wrapper_type", LegacyFullClassName(field->message_type(), is_descriptor));
  }

  // Generate has method for proto2 only.
  if (is_descriptor) {
    printer->Print(
        "public function has^camel_name^()\n"
        "{\n"
        "    return $this->has_^field_name^;\n"
        "}\n\n",
        "camel_name", UnderscoresToCamelCase(field->name(), true),
        "field_name", field->name());
  }
}

void GenerateEnumToPool(const EnumDescriptor* en, io::Printer* printer) {
  printer->Print(
      "$pool->addEnum('^name^', "
      "\\Google\\Protobuf\\Internal\\^class_name^::class)\n",
      "name", DescriptorFullName(en, true),
      "class_name", en->name());
  Indent(printer);

  for (int i = 0; i < en->value_count(); i++) {
    const EnumValueDescriptor* value = en->value(i);
    printer->Print(
        "->value(\"^name^\", ^number^)\n",
        "name", ConstantNamePrefix(value->name()) + value->name(),
        "number", IntToString(value->number()));
  }
  printer->Print("->finalizeToPool();\n\n");
  Outdent(printer);
}

void GenerateServiceMethod(const MethodDescriptor* method,
                           io::Printer* printer) {
  printer->Print(
        "public function ^camel_name^(\\^request_name^ $request);\n\n",
        "camel_name", UnderscoresToCamelCase(method->name(), false),
        "request_name", FullClassName(
          method->input_type(), false)
  );
}

void GenerateMessageToPool(const string& name_prefix, const Descriptor* message,
                           io::Printer* printer) {
  // Don't generate MapEntry messages -- we use the PHP extension's native
  // support for map fields instead.
  if (message->options().map_entry()) {
    return;
  }
  string class_name = (name_prefix.empty() ? "" : name_prefix + "\\") +
    ReservedNamePrefix(message->name(), message->file()) + message->name();

  printer->Print(
      "$pool->addMessage('^message^', "
      "\\Google\\Protobuf\\Internal\\^class_name^::class)\n",
      "message", DescriptorFullName(message, true),
      "class_name", class_name);

  Indent(printer);

  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    if (field->is_map()) {
      const FieldDescriptor* key =
          field->message_type()->FindFieldByName("key");
      const FieldDescriptor* val =
          field->message_type()->FindFieldByName("value");
      printer->Print(
          "->map('^field^', \\Google\\Protobuf\\Internal\\GPBType::^key^, "
          "\\Google\\Protobuf\\Internal\\GPBType::^value^, ^number^^other^)\n",
          "field", field->name(),
          "key", ToUpper(key->type_name()),
          "value", ToUpper(val->type_name()),
          "number", StrCat(field->number()),
          "other", EnumOrMessageSuffix(val, true));
    } else if (!field->containing_oneof()) {
      printer->Print(
          "->^label^('^field^', "
          "\\Google\\Protobuf\\Internal\\GPBType::^type^, ^number^^other^)\n",
          "field", field->name(),
          "label", LabelForField(field),
          "type", ToUpper(field->type_name()),
          "number", StrCat(field->number()),
          "other", EnumOrMessageSuffix(field, true));
    }
  }

  // oneofs.
  for (int i = 0; i < message->oneof_decl_count(); i++) {
    const OneofDescriptor* oneof = message->oneof_decl(i);
    printer->Print("->oneof(^name^)\n",
                   "name", oneof->name());
    Indent(printer);
    for (int index = 0; index < oneof->field_count(); index++) {
      const FieldDescriptor* field = oneof->field(index);
      printer->Print(
          "->value('^field^', "
          "\\Google\\Protobuf\\Internal\\GPBType::^type^, ^number^^other^)\n",
          "field", field->name(),
          "type", ToUpper(field->type_name()),
          "number", StrCat(field->number()),
          "other", EnumOrMessageSuffix(field, true));
    }
    printer->Print("->finish()\n");
    Outdent(printer);
  }

  printer->Print(
      "->finalizeToPool();\n");

  Outdent(printer);

  printer->Print(
      "\n");

  for (int i = 0; i < message->nested_type_count(); i++) {
    GenerateMessageToPool(class_name, message->nested_type(i), printer);
  }
  for (int i = 0; i < message->enum_type_count(); i++) {
    GenerateEnumToPool(message->enum_type(i), printer);
  }
}

void GenerateAddFileToPool(const FileDescriptor* file, bool is_descriptor,
                           io::Printer* printer) {
    printer->Print(
        "public static $is_initialized = false;\n\n"
        "public static function initOnce() {\n");
    Indent(printer);

    printer->Print(
        "$pool = \\Google\\Protobuf\\Internal\\"
        "DescriptorPool::getGeneratedPool();\n\n"
        "if (static::$is_initialized == true) {\n"
        "  return;\n"
        "}\n");

  if (is_descriptor) {
    for (int i = 0; i < file->message_type_count(); i++) {
      GenerateMessageToPool("", file->message_type(i), printer);
    }
    for (int i = 0; i < file->enum_type_count(); i++) {
      GenerateEnumToPool(file->enum_type(i), printer);
    }

    printer->Print(
        "$pool->finish();\n");
  } else {
    for (int i = 0; i < file->dependency_count(); i++) {
      const std::string& name = file->dependency(i)->name();
      // Currently, descriptor.proto is not ready for external usage. Skip to
      // import it for now, so that its dependencies can still work as long as
      // they don't use protos defined in descriptor.proto.
      if (name == kDescriptorFile) {
        continue;
      }
      std::string dependency_filename =
          GeneratedMetadataFileName(file->dependency(i), is_descriptor);
      printer->Print(
          "\\^name^::initOnce();\n",
          "name", FilenameToClassname(dependency_filename));
    }

    // Add messages and enums to descriptor pool.
    FileDescriptorSet files;
    FileDescriptorProto* file_proto = files.add_file();
    file->CopyTo(file_proto);

    // Filter out descriptor.proto as it cannot be depended on for now.
    RepeatedPtrField<string>* dependency = file_proto->mutable_dependency();
    for (RepeatedPtrField<string>::iterator it = dependency->begin();
         it != dependency->end(); ++it) {
      if (*it != kDescriptorFile) {
        dependency->erase(it);
        break;
      }
    }

    // Filter out all extensions, since we do not support extension yet.
    file_proto->clear_extension();
    RepeatedPtrField<DescriptorProto>* message_type =
        file_proto->mutable_message_type();
    for (RepeatedPtrField<DescriptorProto>::iterator it = message_type->begin();
         it != message_type->end(); ++it) {
      it->clear_extension();
    }

    string files_data;
    files.SerializeToString(&files_data);

    printer->Print("$pool->internalAddGeneratedFile(hex2bin(\n");
    Indent(printer);

    // Only write 30 bytes per line.
    static const int kBytesPerLine = 30;
    for (int i = 0; i < files_data.size(); i += kBytesPerLine) {
      printer->Print(
          "\"^data^\"^dot^\n",
          "data", BinaryToHex(files_data.substr(i, kBytesPerLine)),
          "dot", i + kBytesPerLine < files_data.size() ? " ." : "");
    }

    Outdent(printer);
    printer->Print(
        "), true);\n\n");
  }
  printer->Print(
      "static::$is_initialized = true;\n");
  Outdent(printer);
  printer->Print("}\n");
}

void GenerateUseDeclaration(bool is_descriptor, io::Printer* printer) {
  if (!is_descriptor) {
    printer->Print(
        "use Google\\Protobuf\\Internal\\GPBType;\n"
        "use Google\\Protobuf\\Internal\\RepeatedField;\n"
        "use Google\\Protobuf\\Internal\\GPBUtil;\n\n");
  } else {
    printer->Print(
        "use Google\\Protobuf\\Internal\\GPBType;\n"
        "use Google\\Protobuf\\Internal\\GPBWire;\n"
        "use Google\\Protobuf\\Internal\\RepeatedField;\n"
        "use Google\\Protobuf\\Internal\\InputStream;\n"
        "use Google\\Protobuf\\Internal\\GPBUtil;\n\n");
  }
}

void GenerateHead(const FileDescriptor* file, io::Printer* printer) {
  printer->Print(
    "<?php\n"
    "# Generated by the protocol buffer compiler.  DO NOT EDIT!\n"
    "# source: ^filename^\n"
    "\n",
    "filename", file->name());
}

std::string FilenameToClassname(const string& filename) {
  int lastindex = filename.find_last_of(".");
  std::string result = filename.substr(0, lastindex);
  for (int i = 0; i < result.size(); i++) {
    if (result[i] == '/') {
      result[i] = '\\';
    }
  }
  return result;
}

void GenerateMetadataFile(const FileDescriptor* file,
                          bool is_descriptor,
                          GeneratorContext* generator_context) {
  std::string filename = GeneratedMetadataFileName(file, is_descriptor);
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '^');

  GenerateHead(file, &printer);

  std::string fullname = FilenameToClassname(filename);
  int lastindex = fullname.find_last_of("\\");

  if (lastindex != string::npos) {
    printer.Print(
        "namespace ^name^;\n\n",
        "name", fullname.substr(0, lastindex));

    printer.Print(
        "class ^name^\n"
        "{\n",
        "name", fullname.substr(lastindex + 1));
  } else {
    printer.Print(
        "class ^name^\n"
        "{\n",
        "name", fullname);
  }
  Indent(&printer);

  GenerateAddFileToPool(file, is_descriptor, &printer);

  Outdent(&printer);
  printer.Print("}\n\n");
}

template <typename DescriptorType>
void LegacyGenerateClassFile(const FileDescriptor* file, const DescriptorType* desc,
                         bool is_descriptor,
                         GeneratorContext* generator_context) {

  std::string filename = LegacyGeneratedClassFileName(desc, is_descriptor);
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '^');

  GenerateHead(file, &printer);

  std::string php_namespace = RootPhpNamespace(desc, is_descriptor);
  if (php_namespace != "") {
    printer.Print(
        "namespace ^name^;\n\n",
        "name", php_namespace);
  }
  std::string newname = FullClassName(desc, is_descriptor);
  printer.Print("if (false) {\n");
  Indent(&printer);
  printer.Print("/**\n");
  printer.Print(" * This class is deprecated. Use ^new^ instead.\n",
      "new", newname);
  printer.Print(" * @deprecated\n");
  printer.Print(" */\n");
  printer.Print("class ^old^ {}\n",
      "old", LegacyGeneratedClassName(desc));
  Outdent(&printer);
  printer.Print("}\n");
  printer.Print("class_exists(^new^::class);\n",
      "new", GeneratedClassNameImpl(desc));
  printer.Print("@trigger_error('^old^ is deprecated and will be removed in "
      "the next major release. Use ^fullname^ instead', E_USER_DEPRECATED);\n\n",
      "old", LegacyFullClassName(desc, is_descriptor),
      "fullname", newname);
}

void GenerateEnumFile(const FileDescriptor* file, const EnumDescriptor* en,
                      bool is_descriptor, GeneratorContext* generator_context) {
  std::string filename = GeneratedClassFileName(en, is_descriptor);
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '^');

  GenerateHead(file, &printer);

  std::string fullname = FilenameToClassname(filename);
  int lastindex = fullname.find_last_of("\\");

  if (lastindex != string::npos) {
    printer.Print(
        "namespace ^name^;\n\n",
        "name", fullname.substr(0, lastindex));

    // We only need this 'use' statement if the enum has a namespace.
    // Otherwise, we get a warning that the use statement has no effect.
    printer.Print("use UnexpectedValueException;\n\n");
  }

  GenerateEnumDocComment(&printer, en, is_descriptor);

  if (lastindex != string::npos) {
    fullname = fullname.substr(lastindex + 1);
  }

  printer.Print(
      "class ^name^\n"
      "{\n",
      "name", fullname);
  Indent(&printer);

  for (int i = 0; i < en->value_count(); i++) {
    const EnumValueDescriptor* value = en->value(i);
    GenerateEnumValueDocComment(&printer, value);
    printer.Print("const ^name^ = ^number^;\n",
                  "name", ConstantNamePrefix(value->name()) + value->name(),
                  "number", IntToString(value->number()));
  }

  printer.Print("\nprivate static $valueToName = [\n");
  Indent(&printer);
  for (int i = 0; i < en->value_count(); i++) {
    const EnumValueDescriptor* value = en->value(i);
    printer.Print("self::^name^ => '^name^',\n",
                  "name", ConstantNamePrefix(value->name()) + value->name());
  }
  Outdent(&printer);
  printer.Print("];\n");

  printer.Print(
      "\npublic static function name($value)\n"
      "{\n");
  Indent(&printer);
  printer.Print("if (!isset(self::$valueToName[$value])) {\n");
  Indent(&printer);
  printer.Print("throw new UnexpectedValueException(sprintf(\n");
  Indent(&printer);
  Indent(&printer);
  printer.Print("'Enum %s has no name defined for value %s', __CLASS__, $value));\n");
  Outdent(&printer);
  Outdent(&printer);
  Outdent(&printer);
  printer.Print("}\n"
                "return self::$valueToName[$value];\n");
  Outdent(&printer);
  printer.Print("}\n\n");

  printer.Print(
      "\npublic static function value($name)\n"
      "{\n");
  Indent(&printer);
  printer.Print("$const = __CLASS__ . '::' . strtoupper($name);\n"
                "if (!defined($const)) {\n");
  Indent(&printer);
  printer.Print("throw new UnexpectedValueException(sprintf(\n");
  Indent(&printer);
  Indent(&printer);
  printer.Print("'Enum %s has no value defined for name %s', __CLASS__, $name));\n");
  Outdent(&printer);
  Outdent(&printer);
  Outdent(&printer);
  printer.Print("}\n"
                "return constant($const);\n");
  Outdent(&printer);
  printer.Print("}\n");

  Outdent(&printer);
  printer.Print("}\n\n");

  // write legacy file for backwards compatiblity with nested messages and enums
  if (en->containing_type() != NULL) {
    printer.Print(
        "// Adding a class alias for backwards compatibility with the previous class name.\n");
    printer.Print(
        "class_alias(^new^::class, \\^old^::class);\n\n",
        "new", fullname,
        "old", LegacyFullClassName(en, is_descriptor));
    LegacyGenerateClassFile(file, en, is_descriptor, generator_context);
  }
}

void GenerateMessageFile(const FileDescriptor* file, const Descriptor* message,
                         bool is_descriptor,
                         GeneratorContext* generator_context) {
  // Don't generate MapEntry messages -- we use the PHP extension's native
  // support for map fields instead.
  if (message->options().map_entry()) {
    return;
  }

  std::string filename = GeneratedClassFileName(message, is_descriptor);
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '^');

  GenerateHead(file, &printer);

  std::string fullname = FilenameToClassname(filename);
  int lastindex = fullname.find_last_of("\\");

  if (lastindex != string::npos) {
    printer.Print(
        "namespace ^name^;\n\n",
        "name", fullname.substr(0, lastindex));
  }

  GenerateUseDeclaration(is_descriptor, &printer);

  GenerateMessageDocComment(&printer, message, is_descriptor);
  if (lastindex != string::npos) {
    fullname = fullname.substr(lastindex + 1);
  }

  printer.Print(
      "class ^name^ extends \\Google\\Protobuf\\Internal\\Message\n"
      "{\n",
      "name", fullname);
  Indent(&printer);

  // Field and oneof definitions.
  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    GenerateField(field, &printer, is_descriptor);
  }
  for (int i = 0; i < message->oneof_decl_count(); i++) {
    const OneofDescriptor* oneof = message->oneof_decl(i);
    GenerateOneofField(oneof, &printer);
  }
  printer.Print("\n");

  GenerateMessageConstructorDocComment(&printer, message, is_descriptor);
  printer.Print(
      "public function __construct($data = NULL) {\n");
  Indent(&printer);

  std::string metadata_filename =
      GeneratedMetadataFileName(file, is_descriptor);
  std::string metadata_fullname = FilenameToClassname(metadata_filename);
  printer.Print(
      "\\^fullname^::initOnce();\n"
      "parent::__construct($data);\n",
      "fullname", metadata_fullname);

  Outdent(&printer);
  printer.Print("}\n\n");

  // Field and oneof accessors.
  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    GenerateFieldAccessor(field, is_descriptor, &printer);
  }
  for (int i = 0; i < message->oneof_decl_count(); i++) {
    const OneofDescriptor* oneof = message->oneof_decl(i);
    printer.Print(
      "/**\n"
      " * @return string\n"
      " */\n"
      "public function get^camel_name^()\n"
      "{\n"
      "    return $this->whichOneof(\"^name^\");\n"
      "}\n\n",
      "camel_name", UnderscoresToCamelCase(oneof->name(), true), "name",
      oneof->name());
  }

  Outdent(&printer);
  printer.Print("}\n\n");

  // write legacy file for backwards compatiblity with nested messages and enums
  if (message->containing_type() != NULL) {
    printer.Print(
        "// Adding a class alias for backwards compatibility with the previous class name.\n");
    printer.Print(
        "class_alias(^new^::class, \\^old^::class);\n\n",
        "new", fullname,
        "old", LegacyFullClassName(message, is_descriptor));
    LegacyGenerateClassFile(file, message, is_descriptor, generator_context);
  }

  // Nested messages and enums.
  for (int i = 0; i < message->nested_type_count(); i++) {
    GenerateMessageFile(file, message->nested_type(i), is_descriptor,
                        generator_context);
  }
  for (int i = 0; i < message->enum_type_count(); i++) {
    GenerateEnumFile(file, message->enum_type(i), is_descriptor,
                     generator_context);
  }
}

void GenerateServiceFile(const FileDescriptor* file,
  const ServiceDescriptor* service, bool is_descriptor,
  GeneratorContext* generator_context) {
  std::string filename = GeneratedServiceFileName(service, is_descriptor);
  std::unique_ptr<io::ZeroCopyOutputStream> output(
      generator_context->Open(filename));
  io::Printer printer(output.get(), '^');

  GenerateHead(file, &printer);

  std::string fullname = FilenameToClassname(filename);
  int lastindex = fullname.find_last_of("\\");

  if (!file->options().php_namespace().empty() ||
      (!file->options().has_php_namespace() && !file->package().empty()) ||
      lastindex != string::npos) {
    printer.Print(
        "namespace ^name^;\n\n",
        "name", fullname.substr(0, lastindex));
  }

  GenerateServiceDocComment(&printer, service);

  if (lastindex != string::npos) {
      printer.Print(
        "interface ^name^\n"
        "{\n",
        "name", fullname.substr(lastindex + 1));
  } else {
      printer.Print(
        "interface ^name^\n"
        "{\n",
        "name", fullname);
  }

  Indent(&printer);

  for (int i = 0; i < service->method_count(); i++) {
    const MethodDescriptor* method = service->method(i);
    GenerateServiceMethodDocComment(&printer, method);
    GenerateServiceMethod(method, &printer);
  }

  Outdent(&printer);
  printer.Print("}\n\n");
}

void GenerateFile(const FileDescriptor* file, bool is_descriptor,
                  GeneratorContext* generator_context) {
  GenerateMetadataFile(file, is_descriptor, generator_context);
  for (int i = 0; i < file->message_type_count(); i++) {
    GenerateMessageFile(file, file->message_type(i), is_descriptor,
                        generator_context);
  }
  for (int i = 0; i < file->enum_type_count(); i++) {
    GenerateEnumFile(file, file->enum_type(i), is_descriptor,
                     generator_context);
  }
  if (file->options().php_generic_services()) {
    for (int i = 0; i < file->service_count(); i++) {
      GenerateServiceFile(file, file->service(i), is_descriptor,
                       generator_context);
    }
  }
}

static string EscapePhpdoc(const string& input) {
  string result;
  result.reserve(input.size() * 2);

  char prev = '*';

  for (string::size_type i = 0; i < input.size(); i++) {
    char c = input[i];
    switch (c) {
      case '*':
        // Avoid "/*".
        if (prev == '/') {
          result.append("&#42;");
        } else {
          result.push_back(c);
        }
        break;
      case '/':
        // Avoid "*/".
        if (prev == '*') {
          result.append("&#47;");
        } else {
          result.push_back(c);
        }
        break;
      case '@':
        // '@' starts phpdoc tags including the @deprecated tag, which will
        // cause a compile-time error if inserted before a declaration that
        // does not have a corresponding @Deprecated annotation.
        result.append("&#64;");
        break;
      default:
        result.push_back(c);
        break;
    }

    prev = c;
  }

  return result;
}

static void GenerateDocCommentBodyForLocation(
    io::Printer* printer, const SourceLocation& location, bool trailingNewline,
    int indentCount) {
  string comments = location.leading_comments.empty() ?
      location.trailing_comments : location.leading_comments;
  if (!comments.empty()) {
    // TODO(teboring):  Ideally we should parse the comment text as Markdown and
    //   write it back as HTML, but this requires a Markdown parser.  For now
    //   we just use the proto comments unchanged.

    // If the comment itself contains block comment start or end markers,
    // HTML-escape them so that they don't accidentally close the doc comment.
    comments = EscapePhpdoc(comments);

    std::vector<string> lines = Split(comments, "\n");
    while (!lines.empty() && lines.back().empty()) {
      lines.pop_back();
    }

    for (int i = 0; i < lines.size(); i++) {
      // Most lines should start with a space.  Watch out for lines that start
      // with a /, since putting that right after the leading asterisk will
      // close the comment.
      if (indentCount == 0 && !lines[i].empty() && lines[i][0] == '/') {
        printer->Print(" * ^line^\n", "line", lines[i]);
      } else {
        std::string indent = std::string(indentCount, ' ');
        printer->Print(" *^ind^^line^\n", "ind", indent, "line", lines[i]);
      }
    }
    if (trailingNewline) {
      printer->Print(" *\n");
    }
  }
}

template <typename DescriptorType>
static void GenerateDocCommentBody(
    io::Printer* printer, const DescriptorType* descriptor) {
  SourceLocation location;
  if (descriptor->GetSourceLocation(&location)) {
    GenerateDocCommentBodyForLocation(printer, location, true, 0);
  }
}

static string FirstLineOf(const string& value) {
  string result = value;

  string::size_type pos = result.find_first_of('\n');
  if (pos != string::npos) {
    result.erase(pos);
  }

  return result;
}

void GenerateMessageDocComment(io::Printer* printer,
                               const Descriptor* message, int is_descriptor) {
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, message);
  printer->Print(
    " * Generated from protobuf message <code>^messagename^</code>\n"
    " */\n",
    "fullname", EscapePhpdoc(FullClassName(message, is_descriptor)),
    "messagename", EscapePhpdoc(message->full_name()));
}

void GenerateMessageConstructorDocComment(io::Printer* printer,
                                          const Descriptor* message,
                                          int is_descriptor) {
  // In theory we should have slightly different comments for setters, getters,
  // etc., but in practice everyone already knows the difference between these
  // so it's redundant information.

  // We start the comment with the main body based on the comments from the
  // .proto file (if present). We then end with the field declaration, e.g.:
  //   optional string foo = 5;
  // If the field is a group, the debug string might end with {.
  printer->Print("/**\n");
  printer->Print(" * Constructor.\n");
  printer->Print(" *\n");
  printer->Print(" * @param array $data {\n");
  printer->Print(" *     Optional. Data for populating the Message object.\n");
  printer->Print(" *\n");
  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);
    printer->Print(" *     @type ^php_type^ $^var^\n",
      "php_type", PhpSetterTypeName(field, is_descriptor),
      "var", field->name());
    SourceLocation location;
    if (field->GetSourceLocation(&location)) {
      GenerateDocCommentBodyForLocation(printer, location, false, 10);
    }
  }
  printer->Print(" * }\n");
  printer->Print(" */\n");
}

void GenerateServiceDocComment(io::Printer* printer,
                               const ServiceDescriptor* service) {
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, service);
  printer->Print(
    " * Protobuf type <code>^fullname^</code>\n"
    " */\n",
    "fullname", EscapePhpdoc(service->full_name()));
}

void GenerateFieldDocComment(io::Printer* printer, const FieldDescriptor* field,
                             int is_descriptor, int function_type) {
  // In theory we should have slightly different comments for setters, getters,
  // etc., but in practice everyone already knows the difference between these
  // so it's redundant information.

  // We start the comment with the main body based on the comments from the
  // .proto file (if present). We then end with the field declaration, e.g.:
  //   optional string foo = 5;
  // If the field is a group, the debug string might end with {.
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, field);
  printer->Print(
    " * Generated from protobuf field <code>^def^</code>\n",
    "def", EscapePhpdoc(FirstLineOf(field->DebugString())));
  if (function_type == kFieldSetter) {
    printer->Print(" * @param ^php_type^ $var\n",
      "php_type", PhpSetterTypeName(field, is_descriptor));
    printer->Print(" * @return $this\n");
  } else if (function_type == kFieldGetter) {
    printer->Print(" * @return ^php_type^\n",
      "php_type", PhpGetterTypeName(field, is_descriptor));
  }
  printer->Print(" */\n");
}

void GenerateWrapperFieldGetterDocComment(io::Printer* printer, const FieldDescriptor* field) {
  // Generate a doc comment for the special getXXXValue methods that are
  // generated for wrapper types.
  const FieldDescriptor* primitiveField = field->message_type()->FindFieldByName("value");
  printer->Print("/**\n");
  printer->Print(
      " * Returns the unboxed value from <code>get^camel_name^()</code>\n\n",
      "camel_name", UnderscoresToCamelCase(field->name(), true));
  GenerateDocCommentBody(printer, field);
  printer->Print(
    " * Generated from protobuf field <code>^def^</code>\n",
    "def", EscapePhpdoc(FirstLineOf(field->DebugString())));
  printer->Print(" * @return ^php_type^|null\n",
        "php_type", PhpGetterTypeName(primitiveField, false));
  printer->Print(" */\n");
}

void GenerateWrapperFieldSetterDocComment(io::Printer* printer, const FieldDescriptor* field) {
  // Generate a doc comment for the special setXXXValue methods that are
  // generated for wrapper types.
  const FieldDescriptor* primitiveField = field->message_type()->FindFieldByName("value");
  printer->Print("/**\n");
  printer->Print(
      " * Sets the field by wrapping a primitive type in a ^message_name^ object.\n\n",
      "message_name", LegacyFullClassName(field->message_type(), false));
  GenerateDocCommentBody(printer, field);
  printer->Print(
    " * Generated from protobuf field <code>^def^</code>\n",
    "def", EscapePhpdoc(FirstLineOf(field->DebugString())));
  printer->Print(" * @param ^php_type^|null $var\n",
        "php_type", PhpSetterTypeName(primitiveField, false));
  printer->Print(" * @return $this\n");
  printer->Print(" */\n");
}

void GenerateEnumDocComment(io::Printer* printer, const EnumDescriptor* enum_,
                            int is_descriptor) {
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, enum_);
  printer->Print(
    " * Protobuf type <code>^fullname^</code>\n"
    " */\n",
    "fullname", EscapePhpdoc(enum_->full_name()));
}

void GenerateEnumValueDocComment(io::Printer* printer,
                                 const EnumValueDescriptor* value) {
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, value);
  printer->Print(
    " * Generated from protobuf enum <code>^def^</code>\n"
    " */\n",
    "def", EscapePhpdoc(FirstLineOf(value->DebugString())));
}

void GenerateServiceMethodDocComment(io::Printer* printer,
                              const MethodDescriptor* method) {
  printer->Print("/**\n");
  GenerateDocCommentBody(printer, method);
  printer->Print(
    " * Method <code>^method_name^</code>\n"
    " *\n",
    "method_name", EscapePhpdoc(UnderscoresToCamelCase(method->name(), false)));
  printer->Print(
    " * @param \\^input_type^ $request\n",
    "input_type", EscapePhpdoc(FullClassName(method->input_type(), false)));
  printer->Print(
    " * @return \\^return_type^\n"
    " */\n",
    "return_type", EscapePhpdoc(FullClassName(method->output_type(), false)));
}

bool Generator::Generate(const FileDescriptor* file, const string& parameter,
                         GeneratorContext* generator_context,
                         string* error) const {
  bool is_descriptor = parameter == "internal";

  if (is_descriptor && file->name() != kDescriptorFile) {
    *error =
        "Can only generate PHP code for google/protobuf/descriptor.proto.\n";
    return false;
  }

  if (!is_descriptor && file->syntax() != FileDescriptor::SYNTAX_PROTO3) {
    *error =
        "Can only generate PHP code for proto3 .proto files.\n"
        "Please add 'syntax = \"proto3\";' to the top of your .proto file.\n";
    return false;
  }

  GenerateFile(file, is_descriptor, generator_context);

  return true;
}

}  // namespace php
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
