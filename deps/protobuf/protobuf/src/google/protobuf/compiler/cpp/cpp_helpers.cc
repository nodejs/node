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

#include <google/protobuf/compiler/cpp/cpp_helpers.h>

#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <unordered_set>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/descriptor.h>

#include <google/protobuf/compiler/scc.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>


#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

static const char kAnyMessageName[] = "Any";
static const char kAnyProtoFile[] = "google/protobuf/any.proto";

std::string DotsToColons(const std::string& name) {
  return StringReplace(name, ".", "::", true);
}

static const char* const kKeywordList[] = {  //
    "NULL",
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "class",
    "compl",
    "const",
    "constexpr",
    "const_cast",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq"};

static std::unordered_set<std::string>* MakeKeywordsMap() {
  auto* result = new std::unordered_set<std::string>();
  for (const auto keyword : kKeywordList) {
    result->emplace(keyword);
  }
  return result;
}

static std::unordered_set<std::string>& kKeywords = *MakeKeywordsMap();

// Encode [0..63] as 'A'-'Z', 'a'-'z', '0'-'9', '_'
char Base63Char(int value) {
  GOOGLE_CHECK_GE(value, 0);
  if (value < 26) return 'A' + value;
  value -= 26;
  if (value < 26) return 'a' + value;
  value -= 26;
  if (value < 10) return '0' + value;
  GOOGLE_CHECK_EQ(value, 10);
  return '_';
}

// Given a c identifier has 63 legal characters we can't implement base64
// encoding. So we return the k least significant "digits" in base 63.
template <typename I>
std::string Base63(I n, int k) {
  std::string res;
  while (k-- > 0) {
    res += Base63Char(static_cast<int>(n % 63));
    n /= 63;
  }
  return res;
}

std::string IntTypeName(const Options& options, const std::string& type) {
  if (options.opensource_runtime) {
    return "::PROTOBUF_NAMESPACE_ID::" + type;
  } else {
    return "::" + type;
  }
}

void SetIntVar(const Options& options, const std::string& type,
               std::map<std::string, std::string>* variables) {
  (*variables)[type] = IntTypeName(options, type);
}

}  // namespace

void SetCommonVars(const Options& options,
                   std::map<std::string, std::string>* variables) {
  (*variables)["proto_ns"] = ProtobufNamespace(options);

  // Warning: there is some clever naming/splitting here to avoid extract script
  // rewrites.  The names of these variables must not be things that the extract
  // script will rewrite.  That's why we use "CHK" (for example) instead of
  // "GOOGLE_CHECK".
  if (options.opensource_runtime) {
    (*variables)["GOOGLE_PROTOBUF"] = "GOOGLE_PROTOBUF";
    (*variables)["CHK"] = "GOOGLE_CHECK";
    (*variables)["DCHK"] = "GOOGLE_DCHECK";
  } else {
    // These values are things the extract script would rewrite if we did not
    // split them.  It might not strictly matter since we don't generate google3
    // code in open-source.  But it's good to prevent surprising things from
    // happening.
    (*variables)["GOOGLE_PROTOBUF"] =
        "GOOGLE3"
        "_PROTOBUF";
    (*variables)["CHK"] =
        "CH"
        "ECK";
    (*variables)["DCHK"] =
        "DCH"
        "ECK";
  }

  SetIntVar(options, "int8", variables);
  SetIntVar(options, "uint8", variables);
  SetIntVar(options, "uint32", variables);
  SetIntVar(options, "uint64", variables);
  SetIntVar(options, "int32", variables);
  SetIntVar(options, "int64", variables);
  (*variables)["string"] = "std::string";
}

std::string UnderscoresToCamelCase(const std::string& input,
                                   bool cap_next_letter) {
  std::string result;
  // Note:  I distrust ctype.h due to locales.
  for (int i = 0; i < input.size(); i++) {
    if ('a' <= input[i] && input[i] <= 'z') {
      if (cap_next_letter) {
        result += input[i] + ('A' - 'a');
      } else {
        result += input[i];
      }
      cap_next_letter = false;
    } else if ('A' <= input[i] && input[i] <= 'Z') {
      // Capital letters are left as-is.
      result += input[i];
      cap_next_letter = false;
    } else if ('0' <= input[i] && input[i] <= '9') {
      result += input[i];
      cap_next_letter = true;
    } else {
      cap_next_letter = true;
    }
  }
  return result;
}

const char kThickSeparator[] =
    "// ===================================================================\n";
const char kThinSeparator[] =
    "// -------------------------------------------------------------------\n";

bool CanInitializeByZeroing(const FieldDescriptor* field) {
  if (field->is_repeated() || field->is_extension()) return false;
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_ENUM:
      return field->default_value_enum()->number() == 0;
    case FieldDescriptor::CPPTYPE_INT32:
      return field->default_value_int32() == 0;
    case FieldDescriptor::CPPTYPE_INT64:
      return field->default_value_int64() == 0;
    case FieldDescriptor::CPPTYPE_UINT32:
      return field->default_value_uint32() == 0;
    case FieldDescriptor::CPPTYPE_UINT64:
      return field->default_value_uint64() == 0;
    case FieldDescriptor::CPPTYPE_FLOAT:
      return field->default_value_float() == 0;
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return field->default_value_double() == 0;
    case FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() == false;
    default:
      return false;
  }
}

std::string ClassName(const Descriptor* descriptor) {
  const Descriptor* parent = descriptor->containing_type();
  std::string res;
  if (parent) res += ClassName(parent) + "_";
  res += descriptor->name();
  if (IsMapEntryMessage(descriptor)) res += "_DoNotUse";
  return ResolveKeyword(res);
}

std::string ClassName(const EnumDescriptor* enum_descriptor) {
  if (enum_descriptor->containing_type() == nullptr) {
    return ResolveKeyword(enum_descriptor->name());
  } else {
    return ClassName(enum_descriptor->containing_type()) + "_" +
           enum_descriptor->name();
  }
}

std::string QualifiedClassName(const Descriptor* d, const Options& options) {
  return QualifiedFileLevelSymbol(d->file(), ClassName(d), options);
}

std::string QualifiedClassName(const EnumDescriptor* d,
                               const Options& options) {
  return QualifiedFileLevelSymbol(d->file(), ClassName(d), options);
}

std::string QualifiedClassName(const Descriptor* d) {
  return QualifiedClassName(d, Options());
}

std::string QualifiedClassName(const EnumDescriptor* d) {
  return QualifiedClassName(d, Options());
}

std::string Namespace(const std::string& package) {
  if (package.empty()) return "";
  return "::" + DotsToColons(package);
}

std::string Namespace(const FileDescriptor* d, const Options& options) {
  std::string ret = Namespace(d->package());
  if (IsWellKnownMessage(d) && options.opensource_runtime) {
    // Written with string concatenation to prevent rewriting of
    // ::google::protobuf.
    ret = StringReplace(ret,
                        "::google::"
                        "protobuf",
                        "PROTOBUF_NAMESPACE_ID", false);
  }
  return ret;
}

std::string Namespace(const Descriptor* d, const Options& options) {
  return Namespace(d->file(), options);
}

std::string Namespace(const FieldDescriptor* d, const Options& options) {
  return Namespace(d->file(), options);
}

std::string Namespace(const EnumDescriptor* d, const Options& options) {
  return Namespace(d->file(), options);
}

std::string DefaultInstanceType(const Descriptor* descriptor,
                                const Options& options) {
  return ClassName(descriptor) + "DefaultTypeInternal";
}

std::string DefaultInstanceName(const Descriptor* descriptor,
                                const Options& options) {
  return "_" + ClassName(descriptor, false) + "_default_instance_";
}

std::string QualifiedDefaultInstanceName(const Descriptor* descriptor,
                                         const Options& options) {
  return QualifiedFileLevelSymbol(
      descriptor->file(), DefaultInstanceName(descriptor, options), options);
}

std::string DescriptorTableName(const FileDescriptor* file,
                                const Options& options) {
  return UniqueName("descriptor_table", file, options);
}

std::string FileDllExport(const FileDescriptor* file, const Options& options) {
  return UniqueName("PROTOBUF_INTERNAL_EXPORT", file, options);
}

std::string ReferenceFunctionName(const Descriptor* descriptor,
                                  const Options& options) {
  return QualifiedClassName(descriptor, options) + "_ReferenceStrong";
}

std::string SuperClassName(const Descriptor* descriptor,
                           const Options& options) {
  return "::" + ProtobufNamespace(options) +
         (HasDescriptorMethods(descriptor->file(), options) ? "::Message"
                                                            : "::MessageLite");
}

std::string ResolveKeyword(const string& name) {
  if (kKeywords.count(name) > 0) {
    return name + "_";
  }
  return name;
}

std::string FieldName(const FieldDescriptor* field) {
  std::string result = field->name();
  LowerString(&result);
  if (kKeywords.count(result) > 0) {
    result.append("_");
  }
  return result;
}

std::string EnumValueName(const EnumValueDescriptor* enum_value) {
  std::string result = enum_value->name();
  if (kKeywords.count(result) > 0) {
    result.append("_");
  }
  return result;
}

int EstimateAlignmentSize(const FieldDescriptor* field) {
  if (field == nullptr) return 0;
  if (field->is_repeated()) return 8;
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_BOOL:
      return 1;

    case FieldDescriptor::CPPTYPE_INT32:
    case FieldDescriptor::CPPTYPE_UINT32:
    case FieldDescriptor::CPPTYPE_ENUM:
    case FieldDescriptor::CPPTYPE_FLOAT:
      return 4;

    case FieldDescriptor::CPPTYPE_INT64:
    case FieldDescriptor::CPPTYPE_UINT64:
    case FieldDescriptor::CPPTYPE_DOUBLE:
    case FieldDescriptor::CPPTYPE_STRING:
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return 8;
  }
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return -1;  // Make compiler happy.
}

std::string FieldConstantName(const FieldDescriptor* field) {
  std::string field_name = UnderscoresToCamelCase(field->name(), true);
  std::string result = "k" + field_name + "FieldNumber";

  if (!field->is_extension() &&
      field->containing_type()->FindFieldByCamelcaseName(
          field->camelcase_name()) != field) {
    // This field's camelcase name is not unique.  As a hack, add the field
    // number to the constant name.  This makes the constant rather useless,
    // but what can we do?
    result += "_" + StrCat(field->number());
  }

  return result;
}

std::string FieldMessageTypeName(const FieldDescriptor* field,
                                 const Options& options) {
  // Note:  The Google-internal version of Protocol Buffers uses this function
  //   as a hook point for hacks to support legacy code.
  return QualifiedClassName(field->message_type(), options);
}

std::string StripProto(const std::string& filename) {
  if (HasSuffixString(filename, ".protodevel")) {
    return StripSuffixString(filename, ".protodevel");
  } else {
    return StripSuffixString(filename, ".proto");
  }
}

const char* PrimitiveTypeName(FieldDescriptor::CppType type) {
  switch (type) {
    case FieldDescriptor::CPPTYPE_INT32:
      return "::google::protobuf::int32";
    case FieldDescriptor::CPPTYPE_INT64:
      return "::google::protobuf::int64";
    case FieldDescriptor::CPPTYPE_UINT32:
      return "::google::protobuf::uint32";
    case FieldDescriptor::CPPTYPE_UINT64:
      return "::google::protobuf::uint64";
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return "double";
    case FieldDescriptor::CPPTYPE_FLOAT:
      return "float";
    case FieldDescriptor::CPPTYPE_BOOL:
      return "bool";
    case FieldDescriptor::CPPTYPE_ENUM:
      return "int";
    case FieldDescriptor::CPPTYPE_STRING:
      return "std::string";
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return nullptr;

      // No default because we want the compiler to complain if any new
      // CppTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return nullptr;
}

std::string PrimitiveTypeName(const Options& options,
                              FieldDescriptor::CppType type) {
  switch (type) {
    case FieldDescriptor::CPPTYPE_INT32:
      return IntTypeName(options, "int32");
    case FieldDescriptor::CPPTYPE_INT64:
      return IntTypeName(options, "int64");
    case FieldDescriptor::CPPTYPE_UINT32:
      return IntTypeName(options, "uint32");
    case FieldDescriptor::CPPTYPE_UINT64:
      return IntTypeName(options, "uint64");
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return "double";
    case FieldDescriptor::CPPTYPE_FLOAT:
      return "float";
    case FieldDescriptor::CPPTYPE_BOOL:
      return "bool";
    case FieldDescriptor::CPPTYPE_ENUM:
      return "int";
    case FieldDescriptor::CPPTYPE_STRING:
      return "std::string";
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return "";

      // No default because we want the compiler to complain if any new
      // CppTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return "";
}

const char* DeclaredTypeMethodName(FieldDescriptor::Type type) {
  switch (type) {
    case FieldDescriptor::TYPE_INT32:
      return "Int32";
    case FieldDescriptor::TYPE_INT64:
      return "Int64";
    case FieldDescriptor::TYPE_UINT32:
      return "UInt32";
    case FieldDescriptor::TYPE_UINT64:
      return "UInt64";
    case FieldDescriptor::TYPE_SINT32:
      return "SInt32";
    case FieldDescriptor::TYPE_SINT64:
      return "SInt64";
    case FieldDescriptor::TYPE_FIXED32:
      return "Fixed32";
    case FieldDescriptor::TYPE_FIXED64:
      return "Fixed64";
    case FieldDescriptor::TYPE_SFIXED32:
      return "SFixed32";
    case FieldDescriptor::TYPE_SFIXED64:
      return "SFixed64";
    case FieldDescriptor::TYPE_FLOAT:
      return "Float";
    case FieldDescriptor::TYPE_DOUBLE:
      return "Double";

    case FieldDescriptor::TYPE_BOOL:
      return "Bool";
    case FieldDescriptor::TYPE_ENUM:
      return "Enum";

    case FieldDescriptor::TYPE_STRING:
      return "String";
    case FieldDescriptor::TYPE_BYTES:
      return "Bytes";
    case FieldDescriptor::TYPE_GROUP:
      return "Group";
    case FieldDescriptor::TYPE_MESSAGE:
      return "Message";

      // No default because we want the compiler to complain if any new
      // types are added.
  }
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return "";
}

std::string Int32ToString(int number) {
  if (number == kint32min) {
    // This needs to be special-cased, see explanation here:
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52661
    return StrCat(number + 1, " - 1");
  } else {
    return StrCat(number);
  }
}

std::string Int64ToString(const std::string& macro_prefix, int64 number) {
  if (number == kint64min) {
    // This needs to be special-cased, see explanation here:
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52661
    return StrCat(macro_prefix, "_LONGLONG(", number + 1, ") - 1");
  }
  return StrCat(macro_prefix, "_LONGLONG(", number, ")");
}

std::string UInt64ToString(const std::string& macro_prefix, uint64 number) {
  return StrCat(macro_prefix, "_ULONGLONG(", number, ")");
}

std::string DefaultValue(const FieldDescriptor* field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT64:
      return Int64ToString("GG", field->default_value_int64());
    case FieldDescriptor::CPPTYPE_UINT64:
      return UInt64ToString("GG", field->default_value_uint64());
    default:
      return DefaultValue(Options(), field);
  }
}

std::string DefaultValue(const Options& options, const FieldDescriptor* field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return Int32ToString(field->default_value_int32());
    case FieldDescriptor::CPPTYPE_UINT32:
      return StrCat(field->default_value_uint32()) + "u";
    case FieldDescriptor::CPPTYPE_INT64:
      return Int64ToString("PROTOBUF", field->default_value_int64());
    case FieldDescriptor::CPPTYPE_UINT64:
      return UInt64ToString("PROTOBUF", field->default_value_uint64());
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      double value = field->default_value_double();
      if (value == std::numeric_limits<double>::infinity()) {
        return "std::numeric_limits<double>::infinity()";
      } else if (value == -std::numeric_limits<double>::infinity()) {
        return "-std::numeric_limits<double>::infinity()";
      } else if (value != value) {
        return "std::numeric_limits<double>::quiet_NaN()";
      } else {
        return SimpleDtoa(value);
      }
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      float value = field->default_value_float();
      if (value == std::numeric_limits<float>::infinity()) {
        return "std::numeric_limits<float>::infinity()";
      } else if (value == -std::numeric_limits<float>::infinity()) {
        return "-std::numeric_limits<float>::infinity()";
      } else if (value != value) {
        return "std::numeric_limits<float>::quiet_NaN()";
      } else {
        std::string float_value = SimpleFtoa(value);
        // If floating point value contains a period (.) or an exponent
        // (either E or e), then append suffix 'f' to make it a float
        // literal.
        if (float_value.find_first_of(".eE") != string::npos) {
          float_value.push_back('f');
        }
        return float_value;
      }
    }
    case FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() ? "true" : "false";
    case FieldDescriptor::CPPTYPE_ENUM:
      // Lazy:  Generate a static_cast because we don't have a helper function
      //   that constructs the full name of an enum value.
      return strings::Substitute(
          "static_cast< $0 >($1)", ClassName(field->enum_type(), true),
          Int32ToString(field->default_value_enum()->number()));
    case FieldDescriptor::CPPTYPE_STRING:
      return "\"" +
             EscapeTrigraphs(CEscape(field->default_value_string())) +
             "\"";
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return "*" + FieldMessageTypeName(field, options) +
             "::internal_default_instance()";
  }
  // Can't actually get here; make compiler happy.  (We could add a default
  // case above but then we wouldn't get the nice compiler warning when a
  // new type is added.)
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return "";
}

// Convert a file name into a valid identifier.
std::string FilenameIdentifier(const std::string& filename) {
  std::string result;
  for (int i = 0; i < filename.size(); i++) {
    if (ascii_isalnum(filename[i])) {
      result.push_back(filename[i]);
    } else {
      // Not alphanumeric.  To avoid any possibility of name conflicts we
      // use the hex code for the character.
      StrAppend(&result, "_", strings::Hex(static_cast<uint8>(filename[i])));
    }
  }
  return result;
}

string UniqueName(const std::string& name, const std::string& filename,
                  const Options& options) {
  return name + "_" + FilenameIdentifier(filename);
}

// Return the qualified C++ name for a file level symbol.
std::string QualifiedFileLevelSymbol(const FileDescriptor* file,
                                     const std::string& name,
                                     const Options& options) {
  if (file->package().empty()) {
    return StrCat("::", name);
  }
  return StrCat(Namespace(file, options), "::", name);
}

// Escape C++ trigraphs by escaping question marks to \?
std::string EscapeTrigraphs(const std::string& to_escape) {
  return StringReplace(to_escape, "?", "\\?", true);
}

// Escaped function name to eliminate naming conflict.
std::string SafeFunctionName(const Descriptor* descriptor,
                             const FieldDescriptor* field,
                             const std::string& prefix) {
  // Do not use FieldName() since it will escape keywords.
  std::string name = field->name();
  LowerString(&name);
  std::string function_name = prefix + name;
  if (descriptor->FindFieldByName(function_name)) {
    // Single underscore will also make it conflicting with the private data
    // member. We use double underscore to escape function names.
    function_name.append("__");
  } else if (kKeywords.count(name) > 0) {
    // If the field name is a keyword, we append the underscore back to keep it
    // consistent with other function names.
    function_name.append("_");
  }
  return function_name;
}

bool IsStringInlined(const FieldDescriptor* descriptor,
                     const Options& options) {
  if (options.opensource_runtime) return false;

  // TODO(ckennelly): Handle inlining for any.proto.
  if (IsAnyMessage(descriptor->containing_type(), options)) return false;
  if (descriptor->containing_type()->options().map_entry()) return false;

  // Limit to proto2, as we rely on has bits to distinguish field presence for
  // release_$name$.  On proto3, we cannot use the address of the string
  // instance when the field has been inlined.
  if (!HasFieldPresence(descriptor->file())) return false;

  if (options.access_info_map) {
    if (descriptor->is_required()) return true;
  }
  return false;
}

static bool HasLazyFields(const Descriptor* descriptor,
                          const Options& options) {
  for (int field_idx = 0; field_idx < descriptor->field_count(); field_idx++) {
    if (IsLazy(descriptor->field(field_idx), options)) {
      return true;
    }
  }
  for (int idx = 0; idx < descriptor->extension_count(); idx++) {
    if (IsLazy(descriptor->extension(idx), options)) {
      return true;
    }
  }
  for (int idx = 0; idx < descriptor->nested_type_count(); idx++) {
    if (HasLazyFields(descriptor->nested_type(idx), options)) {
      return true;
    }
  }
  return false;
}

// Does the given FileDescriptor use lazy fields?
bool HasLazyFields(const FileDescriptor* file, const Options& options) {
  for (int i = 0; i < file->message_type_count(); i++) {
    const Descriptor* descriptor(file->message_type(i));
    if (HasLazyFields(descriptor, options)) {
      return true;
    }
  }
  for (int field_idx = 0; field_idx < file->extension_count(); field_idx++) {
    if (IsLazy(file->extension(field_idx), options)) {
      return true;
    }
  }
  return false;
}

static bool HasRepeatedFields(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (descriptor->field(i)->label() == FieldDescriptor::LABEL_REPEATED) {
      return true;
    }
  }
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (HasRepeatedFields(descriptor->nested_type(i))) return true;
  }
  return false;
}

bool HasRepeatedFields(const FileDescriptor* file) {
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasRepeatedFields(file->message_type(i))) return true;
  }
  return false;
}

static bool IsStringPieceField(const FieldDescriptor* field,
                               const Options& options) {
  return field->cpp_type() == FieldDescriptor::CPPTYPE_STRING &&
         EffectiveStringCType(field, options) == FieldOptions::STRING_PIECE;
}

static bool HasStringPieceFields(const Descriptor* descriptor,
                                 const Options& options) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (IsStringPieceField(descriptor->field(i), options)) return true;
  }
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (HasStringPieceFields(descriptor->nested_type(i), options)) return true;
  }
  return false;
}

bool HasStringPieceFields(const FileDescriptor* file, const Options& options) {
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasStringPieceFields(file->message_type(i), options)) return true;
  }
  return false;
}

static bool IsCordField(const FieldDescriptor* field, const Options& options) {
  return field->cpp_type() == FieldDescriptor::CPPTYPE_STRING &&
         EffectiveStringCType(field, options) == FieldOptions::CORD;
}

static bool HasCordFields(const Descriptor* descriptor,
                          const Options& options) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (IsCordField(descriptor->field(i), options)) return true;
  }
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (HasCordFields(descriptor->nested_type(i), options)) return true;
  }
  return false;
}

bool HasCordFields(const FileDescriptor* file, const Options& options) {
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasCordFields(file->message_type(i), options)) return true;
  }
  return false;
}

static bool HasExtensionsOrExtendableMessage(const Descriptor* descriptor) {
  if (descriptor->extension_range_count() > 0) return true;
  if (descriptor->extension_count() > 0) return true;
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (HasExtensionsOrExtendableMessage(descriptor->nested_type(i))) {
      return true;
    }
  }
  return false;
}

bool HasExtensionsOrExtendableMessage(const FileDescriptor* file) {
  if (file->extension_count() > 0) return true;
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasExtensionsOrExtendableMessage(file->message_type(i))) return true;
  }
  return false;
}

static bool HasMapFields(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    if (descriptor->field(i)->is_map()) {
      return true;
    }
  }
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (HasMapFields(descriptor->nested_type(i))) return true;
  }
  return false;
}

bool HasMapFields(const FileDescriptor* file) {
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasMapFields(file->message_type(i))) return true;
  }
  return false;
}

static bool HasEnumDefinitions(const Descriptor* message_type) {
  if (message_type->enum_type_count() > 0) return true;
  for (int i = 0; i < message_type->nested_type_count(); ++i) {
    if (HasEnumDefinitions(message_type->nested_type(i))) return true;
  }
  return false;
}

bool HasEnumDefinitions(const FileDescriptor* file) {
  if (file->enum_type_count() > 0) return true;
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasEnumDefinitions(file->message_type(i))) return true;
  }
  return false;
}

bool IsStringOrMessage(const FieldDescriptor* field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
    case FieldDescriptor::CPPTYPE_INT64:
    case FieldDescriptor::CPPTYPE_UINT32:
    case FieldDescriptor::CPPTYPE_UINT64:
    case FieldDescriptor::CPPTYPE_DOUBLE:
    case FieldDescriptor::CPPTYPE_FLOAT:
    case FieldDescriptor::CPPTYPE_BOOL:
    case FieldDescriptor::CPPTYPE_ENUM:
      return false;
    case FieldDescriptor::CPPTYPE_STRING:
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return true;
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return false;
}

FieldOptions::CType EffectiveStringCType(const FieldDescriptor* field,
                                         const Options& options) {
  GOOGLE_DCHECK(field->cpp_type() == FieldDescriptor::CPPTYPE_STRING);
  if (options.opensource_runtime) {
    // Open-source protobuf release only supports STRING ctype.
    return FieldOptions::STRING;
  } else {
    // Google-internal supports all ctypes.
    return field->options().ctype();
  }
}

bool IsAnyMessage(const FileDescriptor* descriptor, const Options& options) {
  return descriptor->name() == kAnyProtoFile;
}

bool IsAnyMessage(const Descriptor* descriptor, const Options& options) {
  return descriptor->name() == kAnyMessageName &&
         IsAnyMessage(descriptor->file(), options);
}

bool IsWellKnownMessage(const FileDescriptor* file) {
  static const std::unordered_set<std::string> well_known_files{
      "google/protobuf/any.proto",
      "google/protobuf/api.proto",
      "google/protobuf/compiler/plugin.proto",
      "google/protobuf/descriptor.proto",
      "google/protobuf/duration.proto",
      "google/protobuf/empty.proto",
      "google/protobuf/field_mask.proto",
      "google/protobuf/source_context.proto",
      "google/protobuf/struct.proto",
      "google/protobuf/timestamp.proto",
      "google/protobuf/type.proto",
      "google/protobuf/wrappers.proto",
  };
  return well_known_files.find(file->name()) != well_known_files.end();
}

enum Utf8CheckMode {
  STRICT = 0,  // Parsing will fail if non UTF-8 data is in string fields.
  VERIFY = 1,  // Only log an error but parsing will succeed.
  NONE = 2,    // No UTF-8 check.
};

static bool FieldEnforceUtf8(const FieldDescriptor* field,
                             const Options& options) {
  return true;
}

static bool FileUtf8Verification(const FileDescriptor* file,
                                 const Options& options) {
  return true;
}

// Which level of UTF-8 enforcemant is placed on this file.
static Utf8CheckMode GetUtf8CheckMode(const FieldDescriptor* field,
                                      const Options& options) {
  if (field->file()->syntax() == FileDescriptor::SYNTAX_PROTO3 &&
      FieldEnforceUtf8(field, options)) {
    return STRICT;
  } else if (GetOptimizeFor(field->file(), options) !=
                 FileOptions::LITE_RUNTIME &&
             FileUtf8Verification(field->file(), options)) {
    return VERIFY;
  } else {
    return NONE;
  }
}

std::string GetUtf8Suffix(const FieldDescriptor* field,
                          const Options& options) {
  switch (GetUtf8CheckMode(field, options)) {
    case STRICT:
      return "UTF8";
    case VERIFY:
      return "UTF8Verify";
    case NONE:
    default:  // Some build configs warn on missing return without default.
      return "";
  }
}

static void GenerateUtf8CheckCode(const FieldDescriptor* field,
                                  const Options& options, bool for_parse,
                                  const char* parameters,
                                  const char* strict_function,
                                  const char* verify_function,
                                  const Formatter& format) {
  switch (GetUtf8CheckMode(field, options)) {
    case STRICT: {
      if (for_parse) {
        format("DO_(");
      }
      format("::$proto_ns$::internal::WireFormatLite::$1$(\n", strict_function);
      format.Indent();
      format(parameters);
      if (for_parse) {
        format("::$proto_ns$::internal::WireFormatLite::PARSE,\n");
      } else {
        format("::$proto_ns$::internal::WireFormatLite::SERIALIZE,\n");
      }
      format("\"$1$\")", field->full_name());
      if (for_parse) {
        format(")");
      }
      format(";\n");
      format.Outdent();
      break;
    }
    case VERIFY: {
      format("::$proto_ns$::internal::WireFormat::$1$(\n", verify_function);
      format.Indent();
      format(parameters);
      if (for_parse) {
        format("::$proto_ns$::internal::WireFormat::PARSE,\n");
      } else {
        format("::$proto_ns$::internal::WireFormat::SERIALIZE,\n");
      }
      format("\"$1$\");\n", field->full_name());
      format.Outdent();
      break;
    }
    case NONE:
      break;
  }
}

void GenerateUtf8CheckCodeForString(const FieldDescriptor* field,
                                    const Options& options, bool for_parse,
                                    const char* parameters,
                                    const Formatter& format) {
  GenerateUtf8CheckCode(field, options, for_parse, parameters,
                        "VerifyUtf8String", "VerifyUTF8StringNamedField",
                        format);
}

void GenerateUtf8CheckCodeForCord(const FieldDescriptor* field,
                                  const Options& options, bool for_parse,
                                  const char* parameters,
                                  const Formatter& format) {
  GenerateUtf8CheckCode(field, options, for_parse, parameters, "VerifyUtf8Cord",
                        "VerifyUTF8CordNamedField", format);
}

namespace {

void Flatten(const Descriptor* descriptor,
             std::vector<const Descriptor*>* flatten) {
  for (int i = 0; i < descriptor->nested_type_count(); i++)
    Flatten(descriptor->nested_type(i), flatten);
  flatten->push_back(descriptor);
}

}  // namespace

void FlattenMessagesInFile(const FileDescriptor* file,
                           std::vector<const Descriptor*>* result) {
  for (int i = 0; i < file->message_type_count(); i++) {
    Flatten(file->message_type(i), result);
  }
}

bool HasWeakFields(const Descriptor* descriptor, const Options& options) {
  for (int i = 0; i < descriptor->field_count(); i++) {
    if (IsWeak(descriptor->field(i), options)) return true;
  }
  return false;
}

bool HasWeakFields(const FileDescriptor* file, const Options& options) {
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (HasWeakFields(file->message_type(i), options)) return true;
  }
  return false;
}

bool UsingImplicitWeakFields(const FileDescriptor* file,
                             const Options& options) {
  return options.lite_implicit_weak_fields &&
         GetOptimizeFor(file, options) == FileOptions::LITE_RUNTIME;
}

bool IsImplicitWeakField(const FieldDescriptor* field, const Options& options,
                         MessageSCCAnalyzer* scc_analyzer) {
  return UsingImplicitWeakFields(field->file(), options) &&
         field->type() == FieldDescriptor::TYPE_MESSAGE &&
         !field->is_required() && !field->is_map() &&
         field->containing_oneof() == nullptr &&
         !IsWellKnownMessage(field->message_type()->file()) &&
         field->message_type()->file()->name() !=
             "net/proto2/proto/descriptor.proto" &&
         // We do not support implicit weak fields between messages in the same
         // strongly-connected component.
         scc_analyzer->GetSCC(field->containing_type()) !=
             scc_analyzer->GetSCC(field->message_type());
}

MessageAnalysis MessageSCCAnalyzer::GetSCCAnalysis(const SCC* scc) {
  if (analysis_cache_.count(scc)) return analysis_cache_[scc];
  MessageAnalysis result{};
  for (int i = 0; i < scc->descriptors.size(); i++) {
    const Descriptor* descriptor = scc->descriptors[i];
    if (descriptor->extension_range_count() > 0) {
      result.contains_extension = true;
      // Extensions are found by looking up default_instance and extension
      // number in a map. So you'd maybe expect here
      // result.constructor_requires_initialization = true;
      // However the extension registration mechanism already makes sure
      // the default will be initialized.
    }
    for (int i = 0; i < descriptor->field_count(); i++) {
      const FieldDescriptor* field = descriptor->field(i);
      if (field->is_required()) {
        result.contains_required = true;
      }
      switch (field->type()) {
        case FieldDescriptor::TYPE_STRING:
        case FieldDescriptor::TYPE_BYTES: {
          result.constructor_requires_initialization = true;
          if (field->options().ctype() == FieldOptions::CORD) {
            result.contains_cord = true;
          }
          break;
        }
        case FieldDescriptor::TYPE_GROUP:
        case FieldDescriptor::TYPE_MESSAGE: {
          result.constructor_requires_initialization = true;
          const SCC* child = analyzer_.GetSCC(field->message_type());
          if (child != scc) {
            MessageAnalysis analysis = GetSCCAnalysis(child);
            result.contains_cord |= analysis.contains_cord;
            result.contains_extension |= analysis.contains_extension;
            if (!ShouldIgnoreRequiredFieldCheck(field, options_)) {
              result.contains_required |= analysis.contains_required;
            }
          } else {
            // This field points back into the same SCC hence the messages
            // in the SCC are recursive. Note if SCC contains more than two
            // nodes it has to be recursive, however this test also works for
            // a single node that is recursive.
            result.is_recursive = true;
          }
          break;
        }
        default:
          break;
      }
    }
  }
  // We deliberately only insert the result here. After we contracted the SCC
  // in the graph, the graph should be a DAG. Hence we shouldn't need to mark
  // nodes visited as we can never return to them. By inserting them here
  // we will go in an infinite loop if the SCC is not correct.
  return analysis_cache_[scc] = result;
}

void ListAllFields(const Descriptor* d,
                   std::vector<const FieldDescriptor*>* fields) {
  // Collect sub messages
  for (int i = 0; i < d->nested_type_count(); i++) {
    ListAllFields(d->nested_type(i), fields);
  }
  // Collect message level extensions.
  for (int i = 0; i < d->extension_count(); i++) {
    fields->push_back(d->extension(i));
  }
  // Add types of fields necessary
  for (int i = 0; i < d->field_count(); i++) {
    fields->push_back(d->field(i));
  }
}

void ListAllFields(const FileDescriptor* d,
                   std::vector<const FieldDescriptor*>* fields) {
  // Collect file level message.
  for (int i = 0; i < d->message_type_count(); i++) {
    ListAllFields(d->message_type(i), fields);
  }
  // Collect message level extensions.
  for (int i = 0; i < d->extension_count(); i++) {
    fields->push_back(d->extension(i));
  }
}

void ListAllTypesForServices(const FileDescriptor* fd,
                             std::vector<const Descriptor*>* types) {
  for (int i = 0; i < fd->service_count(); i++) {
    const ServiceDescriptor* sd = fd->service(i);
    for (int j = 0; j < sd->method_count(); j++) {
      const MethodDescriptor* method = sd->method(j);
      types->push_back(method->input_type());
      types->push_back(method->output_type());
    }
  }
}

bool GetBootstrapBasename(const Options& options, const std::string& basename,
                          std::string* bootstrap_basename) {
  if (options.opensource_runtime) {
    return false;
  }

  std::unordered_map<std::string, std::string> bootstrap_mapping{
      {"net/proto2/proto/descriptor",
       "net/proto2/internal/descriptor"},
      {"net/proto2/compiler/proto/plugin",
       "net/proto2/compiler/proto/plugin"},
      {"net/proto2/compiler/proto/profile",
       "net/proto2/compiler/proto/profile_bootstrap"},
  };
  auto iter = bootstrap_mapping.find(basename);
  if (iter == bootstrap_mapping.end()) {
    *bootstrap_basename = basename;
    return false;
  } else {
    *bootstrap_basename = iter->second;
    return true;
  }
}

bool IsBootstrapProto(const Options& options, const FileDescriptor* file) {
  std::string my_name = StripProto(file->name());
  return GetBootstrapBasename(options, my_name, &my_name);
}

bool MaybeBootstrap(const Options& options, GeneratorContext* generator_context,
                    bool bootstrap_flag, std::string* basename) {
  std::string bootstrap_basename;
  if (!GetBootstrapBasename(options, *basename, &bootstrap_basename)) {
    return false;
  }

  if (bootstrap_flag) {
    // Adjust basename, but don't abort code generation.
    *basename = bootstrap_basename;
    return false;
  } else {
    std::string forward_to_basename = bootstrap_basename;

    // Generate forwarding headers and empty .pb.cc.
    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->Open(*basename + ".pb.h"));
      io::Printer printer(output.get(), '$', nullptr);
      printer.Print(
          "#ifndef PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PB_H\n"
          "#define PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PB_H\n"
          "#include \"$forward_to_basename$.pb.h\"  // IWYU pragma: export\n"
          "#endif  // PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PB_H\n",
          "forward_to_basename", forward_to_basename, "filename_identifier",
          FilenameIdentifier(*basename));

      if (!options.opensource_runtime) {
        // HACK HACK HACK, tech debt from the deeps of proto1 and SWIG
        // protocoltype is SWIG'ed and we need to forward
        if (*basename == "net/proto/protocoltype") {
          printer.Print(
              "#ifdef SWIG\n"
              "%include \"$forward_to_basename$.pb.h\"\n"
              "#endif  // SWIG\n",
              "forward_to_basename", forward_to_basename);
        }
      }
    }

    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->Open(*basename + ".proto.h"));
      io::Printer printer(output.get(), '$', nullptr);
      printer.Print(
          "#ifndef PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PROTO_H\n"
          "#define PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PROTO_H\n"
          "#include \"$forward_to_basename$.proto.h\"  // IWYU pragma: "
          "export\n"
          "#endif  // "
          "PROTOBUF_INCLUDED_$filename_identifier$_FORWARD_PROTO_H\n",
          "forward_to_basename", forward_to_basename, "filename_identifier",
          FilenameIdentifier(*basename));
    }

    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->Open(*basename + ".pb.cc"));
      io::Printer printer(output.get(), '$', nullptr);
      printer.Print("\n");
    }

    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->Open(*basename + ".pb.h.meta"));
    }

    {
      std::unique_ptr<io::ZeroCopyOutputStream> output(
          generator_context->Open(*basename + ".proto.h.meta"));
    }

    // Abort code generation.
    return true;
  }
}

class ParseLoopGenerator {
 public:
  ParseLoopGenerator(int num_hasbits, const Options& options,
                     MessageSCCAnalyzer* scc_analyzer, io::Printer* printer)
      : scc_analyzer_(scc_analyzer),
        options_(options),
        format_(printer),
        num_hasbits_(num_hasbits) {}

  void GenerateParserLoop(const Descriptor* descriptor) {
    format_.Set("classname", ClassName(descriptor));
    format_.Set("p_ns", "::" + ProtobufNamespace(options_));
    format_.Set("pi_ns",
                StrCat("::", ProtobufNamespace(options_), "::internal"));
    format_.Set("GOOGLE_PROTOBUF", MacroPrefix(options_));
    std::map<std::string, std::string> vars;
    SetCommonVars(options_, &vars);
    format_.AddMap(vars);

    std::vector<const FieldDescriptor*> ordered_fields;
    for (auto field : FieldRange(descriptor)) {
      ordered_fields.push_back(field);
    }
    std::sort(ordered_fields.begin(), ordered_fields.end(),
              [](const FieldDescriptor* a, const FieldDescriptor* b) {
                return a->number() < b->number();
              });

    format_(
        "const char* $classname$::_InternalParse(const char* ptr, "
        "$pi_ns$::ParseContext* ctx) {\n"
        "#define CHK_(x) if (PROTOBUF_PREDICT_FALSE(!(x))) goto failure\n");
    format_.Indent();
    int hasbits_size = 0;
    if (HasFieldPresence(descriptor->file())) {
      hasbits_size = (num_hasbits_ + 31) / 32;
    }
    // For now only optimize small hasbits.
    if (hasbits_size != 1) hasbits_size = 0;
    if (hasbits_size) {
      format_("_Internal::HasBits has_bits{};\n");
      format_.Set("has_bits", "has_bits");
    } else {
      format_.Set("has_bits", "_has_bits_");
    }

    if (descriptor->file()->options().cc_enable_arenas()) {
      format_("$p_ns$::Arena* arena = GetArenaNoVirtual(); (void)arena;\n");
    }
    GenerateParseLoop(descriptor, ordered_fields);
    format_.Outdent();
    format_("success:\n");
    if (hasbits_size) format_("  _has_bits_.Or(has_bits);\n");
    format_(
        "  return ptr;\n"
        "failure:\n"
        "  ptr = nullptr;\n"
        "  goto success;\n"
        "#undef CHK_\n"
        "}\n");
  }

 private:
  MessageSCCAnalyzer* scc_analyzer_;
  const Options& options_;
  Formatter format_;
  int num_hasbits_;

  using WireFormat = internal::WireFormat;
  using WireFormatLite = internal::WireFormatLite;

  void GenerateArenaString(const FieldDescriptor* field,
                           const std::string& utf8, std::string field_name) {
    if (!field_name.empty()) {
      format_("static const char kFieldName[] = $1$;\n",
              field_name.substr(2));  // remove ", "
      field_name = ", kFieldName";
    }
    if (HasFieldPresence(field->file())) {
      format_("_Internal::set_has_$1$(&$has_bits$);\n", FieldName(field));
    }
    string default_string =
        field->default_value_string().empty()
            ? "::" + ProtobufNamespace(options_) +
                  "::internal::GetEmptyStringAlreadyInited()"
            : QualifiedClassName(field->containing_type(), options_) +
                  "::" + MakeDefaultName(field) + ".get()";
    format_(
        "if (arena != nullptr) {\n"
        "  ptr = $pi_ns$::InlineCopyIntoArenaString$1$(&$2$_, ptr, ctx, "
        "  arena$3$);\n"
        "} else {\n"
        "  ptr = "
        "$pi_ns$::InlineGreedyStringParser$1$($2$_.MutableNoArenaNoDefault(&$4$"
        "), ptr, ctx$3$);"
        "\n}\n",
        utf8, FieldName(field), field_name, default_string);
  }

  void GenerateStrings(const FieldDescriptor* field, bool check_utf8) {
    std::string utf8;
    std::string field_name;
    if (check_utf8) {
      utf8 = GetUtf8Suffix(field, options_);
      if (!utf8.empty()) {
        field_name = ", nullptr";
        if (HasDescriptorMethods(field->file(), options_)) {
          field_name = StrCat(", \"", field->full_name(), "\"");
        }
      }
    }
    FieldOptions::CType ctype = FieldOptions::STRING;
    if (!options_.opensource_runtime) {
      // Open source doesn't support other ctypes;
      ctype = field->options().ctype();
    }
    if (field->file()->options().cc_enable_arenas() && !field->is_repeated() &&
        !options_.opensource_runtime &&
        GetOptimizeFor(field->file(), options_) != FileOptions::LITE_RUNTIME &&
        // For now only use arena string for strings with empty defaults.
        field->default_value_string().empty() &&
        !IsStringInlined(field, options_) &&
        field->containing_oneof() == nullptr && ctype == FieldOptions::STRING) {
      GenerateArenaString(field, utf8, field_name);
      return;
    }
    std::string name;
    switch (ctype) {
      case FieldOptions::STRING:
        name = "GreedyStringParser" + utf8;
        break;
      case FieldOptions::CORD:
        name = "CordParser" + utf8;
        break;
      case FieldOptions::STRING_PIECE:
        name = "StringPieceParser" + utf8;
        break;
    }
    format_("ptr = $pi_ns$::Inline$1$($2$_$3$(), ptr, ctx$4$);\n", name,
            field->is_repeated() && !field->is_packable() ? "add" : "mutable",
            FieldName(field), field_name);
  }

  void GenerateLengthDelim(const FieldDescriptor* field) {
    if (field->is_packable()) {
      std::string enum_validator;
      if (field->type() == FieldDescriptor::TYPE_ENUM &&
          !HasPreservingUnknownEnumSemantics(field)) {
        enum_validator =
            StrCat(", ", QualifiedClassName(field->enum_type(), options_),
                         "_IsValid, &_internal_metadata_, ", field->number());
      }
      format_("ptr = $pi_ns$::Packed$1$Parser(mutable_$2$(), ptr, ctx$3$);\n",
              DeclaredTypeMethodName(field->type()), FieldName(field),
              enum_validator);
    } else {
      auto field_type = field->type();
      switch (field_type) {
        case FieldDescriptor::TYPE_STRING:
          GenerateStrings(field, true /* utf8 */);
          break;
        case FieldDescriptor::TYPE_BYTES:
          GenerateStrings(field, false /* utf8 */);
          break;
        case FieldDescriptor::TYPE_MESSAGE: {
          if (field->is_map()) {
            const FieldDescriptor* val =
                field->message_type()->FindFieldByName("value");
            GOOGLE_CHECK(val);
            if (HasFieldPresence(field->file()) &&
                val->type() == FieldDescriptor::TYPE_ENUM) {
              format_(
                  "auto object = ::$proto_ns$::internal::InitEnumParseWrapper("
                  "&$1$_, $2$_IsValid, $3$, &_internal_metadata_);\n"
                  "ptr = ctx->ParseMessage(&object, ptr);\n",
                  FieldName(field), QualifiedClassName(val->enum_type()),
                  field->number());
            } else {
              format_("ptr = ctx->ParseMessage(&$1$_, ptr);\n",
                      FieldName(field));
            }
          } else if (IsLazy(field, options_)) {
            if (field->containing_oneof() != nullptr) {
              format_(
                  "if (!has_$1$()) {\n"
                  "  clear_$1$();\n"
                  "  $2$_.$1$_ = ::$proto_ns$::Arena::CreateMessage<\n"
                  "      $pi_ns$::LazyField>("
                  "GetArenaNoVirtual());\n"
                  "  set_has_$1$();\n"
                  "}\n"
                  "ptr = ctx->ParseMessage($2$_.$1$_, ptr);\n",
                  FieldName(field), field->containing_oneof()->name());
            } else if (HasFieldPresence(field->file())) {
              format_(
                  "_Internal::set_has_$1$(&$has_bits$);\n"
                  "ptr = ctx->ParseMessage(&$1$_, ptr);\n",
                  FieldName(field));
            } else {
              format_("ptr = ctx->ParseMessage(&$1$_, ptr);\n",
                      FieldName(field));
            }
          } else if (IsImplicitWeakField(field, options_, scc_analyzer_)) {
            if (!field->is_repeated()) {
              format_(
                  "ptr = ctx->ParseMessage(_Internal::mutable_$1$(this), "
                  "ptr);\n",
                  FieldName(field));
            } else {
              format_(
                  "ptr = ctx->ParseMessage("
                  "CastToBase(&$1$_)->AddWeak(reinterpret_cast<const "
                  "::$proto_ns$::MessageLite*>(&$2$::_$3$_default_instance_)), "
                  "ptr);\n",
                  FieldName(field), Namespace(field->message_type(), options_),
                  ClassName(field->message_type()));
            }
          } else if (IsWeak(field, options_)) {
            format_(
                "ptr = ctx->ParseMessage(_weak_field_map_.MutableMessage($1$,"
                " _$classname$_default_instance_.$2$_), ptr);\n",
                field->number(), FieldName(field));
          } else {
            format_("ptr = ctx->ParseMessage($1$_$2$(), ptr);\n",
                    field->is_repeated() ? "add" : "mutable", FieldName(field));
          }
          break;
        }
        default:
          GOOGLE_LOG(FATAL) << "Illegal combination for length delimited wiretype "
                     << " filed type is " << field->type();
      }
    }
  }

  // Convert a 1 or 2 byte varint into the equivalent value upon a direct load.
  static uint32 SmallVarintValue(uint32 x) {
    GOOGLE_DCHECK(x < 128 * 128);
    if (x >= 128) x += (x & 0xFF80) + 128;
    return x;
  }

  static bool ShouldRepeat(const FieldDescriptor* descriptor,
                           internal::WireFormatLite::WireType wiretype) {
    constexpr int kMaxTwoByteFieldNumber = 16 * 128;
    return descriptor->number() < kMaxTwoByteFieldNumber &&
           descriptor->is_repeated() &&
           (!descriptor->is_packable() ||
            wiretype != internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
  }

  void GenerateFieldBody(internal::WireFormatLite::WireType wiretype,
                         const FieldDescriptor* field) {
    uint32 tag = WireFormatLite::MakeTag(field->number(), wiretype);
    switch (wiretype) {
      case WireFormatLite::WIRETYPE_VARINT: {
        std::string type = PrimitiveTypeName(options_, field->cpp_type());
        std::string prefix = field->is_repeated() ? "add" : "set";
        if (field->type() == FieldDescriptor::TYPE_ENUM) {
          format_(
              "$uint64$ val = $pi_ns$::ReadVarint(&ptr);\n"
              "CHK_(ptr);\n");
          if (!HasPreservingUnknownEnumSemantics(field)) {
            format_("if (PROTOBUF_PREDICT_TRUE($1$_IsValid(val))) {\n",
                    QualifiedClassName(field->enum_type(), options_));
            format_.Indent();
          }
          format_("$1$_$2$(static_cast<$3$>(val));\n", prefix, FieldName(field),
                  QualifiedClassName(field->enum_type(), options_));
          if (!HasPreservingUnknownEnumSemantics(field)) {
            format_.Outdent();
            format_(
                "} else {\n"
                "  $pi_ns$::WriteVarint($1$, val, mutable_unknown_fields());\n"
                "}\n",
                field->number());
          }
        } else {
          int size = field->type() == FieldDescriptor::TYPE_SINT32 ? 32 : 64;
          std::string zigzag;
          if ((field->type() == FieldDescriptor::TYPE_SINT32 ||
               field->type() == FieldDescriptor::TYPE_SINT64)) {
            zigzag = StrCat("ZigZag", size);
          }
          if (field->is_repeated() || field->containing_oneof()) {
            string prefix = field->is_repeated() ? "add" : "set";
            format_(
                "$1$_$2$($pi_ns$::ReadVarint$3$(&ptr));\n"
                "CHK_(ptr);\n",
                prefix, FieldName(field), zigzag);
          } else {
            if (HasFieldPresence(field->file())) {
              format_("_Internal::set_has_$1$(&$has_bits$);\n",
                      FieldName(field));
            }
            format_(
                "$1$_ = $pi_ns$::ReadVarint$2$(&ptr);\n"
                "CHK_(ptr);\n",
                FieldName(field), zigzag);
          }
        }
        break;
      }
      case WireFormatLite::WIRETYPE_FIXED32:
      case WireFormatLite::WIRETYPE_FIXED64: {
        std::string type = PrimitiveTypeName(options_, field->cpp_type());
        if (field->is_repeated() || field->containing_oneof()) {
          string prefix = field->is_repeated() ? "add" : "set";
          format_(
              "$1$_$2$($pi_ns$::UnalignedLoad<$3$>(ptr));\n"
              "ptr += sizeof($3$);\n",
              prefix, FieldName(field), type);
        } else {
          if (HasFieldPresence(field->file())) {
            format_("_Internal::set_has_$1$(&$has_bits$);\n", FieldName(field));
          }
          format_(
              "$1$_ = $pi_ns$::UnalignedLoad<$2$>(ptr);\n"
              "ptr += sizeof($2$);\n",
              FieldName(field), type);
        }
        break;
      }
      case WireFormatLite::WIRETYPE_LENGTH_DELIMITED: {
        GenerateLengthDelim(field);
        format_("CHK_(ptr);\n");
        break;
      }
      case WireFormatLite::WIRETYPE_START_GROUP: {
        format_(
            "ptr = ctx->ParseGroup($1$_$2$(), ptr, $3$);\n"
            "CHK_(ptr);\n",
            field->is_repeated() ? "add" : "mutable", FieldName(field), tag);
        break;
      }
      case WireFormatLite::WIRETYPE_END_GROUP: {
        GOOGLE_LOG(FATAL) << "Can't have end group field\n";
        break;
      }
    }  // switch (wire_type)
  }

  // Returns the tag for this field and in case of repeated packable fields,
  // sets a fallback tag in fallback_tag_ptr.
  static uint32 ExpectedTag(const FieldDescriptor* field,
                            uint32* fallback_tag_ptr) {
    uint32 expected_tag;
    if (field->is_packable()) {
      auto expected_wiretype = WireFormat::WireTypeForFieldType(field->type());
      expected_tag =
          WireFormatLite::MakeTag(field->number(), expected_wiretype);
      GOOGLE_CHECK(expected_wiretype != WireFormatLite::WIRETYPE_LENGTH_DELIMITED);
      auto fallback_wiretype = WireFormatLite::WIRETYPE_LENGTH_DELIMITED;
      uint32 fallback_tag =
          WireFormatLite::MakeTag(field->number(), fallback_wiretype);

      if (field->is_packed()) std::swap(expected_tag, fallback_tag);
      *fallback_tag_ptr = fallback_tag;
    } else {
      auto expected_wiretype = WireFormat::WireTypeForField(field);
      expected_tag =
          WireFormatLite::MakeTag(field->number(), expected_wiretype);
    }
    return expected_tag;
  }

  void GenerateParseLoop(
      const Descriptor* descriptor,
      const std::vector<const FieldDescriptor*>& ordered_fields) {
    format_(
        "while (!ctx->Done(&ptr)) {\n"
        "  $uint32$ tag;\n"
        "  ptr = $pi_ns$::ReadTag(ptr, &tag);\n"
        "  CHK_(ptr);\n"
        "  switch (tag >> 3) {\n");

    format_.Indent();
    format_.Indent();

    for (const auto* field : ordered_fields) {
      // Print the field's (or oneof's) proto-syntax definition as a comment.
      // We don't want to print group bodies so we cut off after the first
      // line.
      std::string def;
      {
        DebugStringOptions options;
        options.elide_group_body = true;
        options.elide_oneof_body = true;
        def = field->DebugStringWithOptions(options);
        def = def.substr(0, def.find_first_of('\n'));
      }
      format_(
          "// $1$\n"
          "case $2$:\n",
          def, field->number());
      format_.Indent();
      uint32 fallback_tag = 0;
      uint32 expected_tag = ExpectedTag(field, &fallback_tag);
      format_(
          "if (PROTOBUF_PREDICT_TRUE(static_cast<$uint8$>(tag) == $1$)) {\n",
          expected_tag & 0xFF);
      format_.Indent();
      auto wiretype = WireFormatLite::GetTagWireType(expected_tag);
      uint32 tag = WireFormatLite::MakeTag(field->number(), wiretype);
      int tag_size = io::CodedOutputStream::VarintSize32(tag);
      bool is_repeat = ShouldRepeat(field, wiretype);
      if (is_repeat) {
        format_(
            "ptr -= $1$;\n"
            "do {\n"
            "  ptr += $1$;\n",
            tag_size);
        format_.Indent();
      }
      GenerateFieldBody(wiretype, field);
      if (is_repeat) {
        string type = tag_size == 2 ? "uint16" : "uint8";
        format_.Outdent();
        format_(
            "  if (!ctx->DataAvailable(ptr)) break;\n"
            "} while ($pi_ns$::UnalignedLoad<$1$>(ptr) == $2$);\n",
            IntTypeName(options_, type), SmallVarintValue(tag));
      }
      format_.Outdent();
      if (fallback_tag) {
        format_("} else if (static_cast<$uint8$>(tag) == $1$) {\n",
                fallback_tag & 0xFF);
        format_.Indent();
        GenerateFieldBody(WireFormatLite::GetTagWireType(fallback_tag), field);
        format_.Outdent();
      }
      format_.Outdent();
      format_(
          "  } else goto handle_unusual;\n"
          "  continue;\n");
    }  // for loop over ordered fields

    // Default case
    format_("default: {\n");
    if (!ordered_fields.empty()) format_("handle_unusual:\n");
    format_(
        "  if ((tag & 7) == 4 || tag == 0) {\n"
        "    ctx->SetLastTag(tag);\n"
        "    goto success;\n"
        "  }\n");
    if (IsMapEntryMessage(descriptor)) {
      format_("  continue;\n");
    } else {
      if (descriptor->extension_range_count() > 0) {
        format_("if (");
        for (int i = 0; i < descriptor->extension_range_count(); i++) {
          const Descriptor::ExtensionRange* range =
              descriptor->extension_range(i);
          if (i > 0) format_(" ||\n    ");

          uint32 start_tag = WireFormatLite::MakeTag(
              range->start, static_cast<WireFormatLite::WireType>(0));
          uint32 end_tag = WireFormatLite::MakeTag(
              range->end, static_cast<WireFormatLite::WireType>(0));

          if (range->end > FieldDescriptor::kMaxNumber) {
            format_("($1$u <= tag)", start_tag);
          } else {
            format_("($1$u <= tag && tag < $2$u)", start_tag, end_tag);
          }
        }
        format_(") {\n");
        format_(
            "  ptr = _extensions_.ParseField(tag, ptr,\n"
            "      internal_default_instance(), &_internal_metadata_, ctx);\n"
            "  CHK_(ptr != nullptr);\n"
            "  continue;\n"
            "}\n");
      }
      format_(
          "  ptr = UnknownFieldParse(tag, &_internal_metadata_, ptr, ctx);\n"
          "  CHK_(ptr != nullptr);\n"
          "  continue;\n");
    }
    format_("}\n");  // default case
    format_.Outdent();
    format_.Outdent();
    format_(
        "  }  // switch\n"
        "}  // while\n");
  }
};

void GenerateParserLoop(const Descriptor* descriptor, int num_hasbits,
                        const Options& options,
                        MessageSCCAnalyzer* scc_analyzer,
                        io::Printer* printer) {
  ParseLoopGenerator generator(num_hasbits, options, scc_analyzer, printer);
  generator.GenerateParserLoop(descriptor);
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
