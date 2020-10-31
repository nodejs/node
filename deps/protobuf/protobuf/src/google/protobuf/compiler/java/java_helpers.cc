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

#include <algorithm>
#include <limits>
#include <unordered_set>
#include <vector>

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_name_resolver.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>



#include <google/protobuf/stubs/hash.h>  // for hash<T *>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

using internal::WireFormat;
using internal::WireFormatLite;

const char kThickSeparator[] =
    "// ===================================================================\n";
const char kThinSeparator[] =
    "// -------------------------------------------------------------------\n";

namespace {

const char* kDefaultPackage = "";

// Names that should be avoided as field names.
// Using them will cause the compiler to generate accessors whose names are
// colliding with methods defined in base classes.
const char* kForbiddenWordList[] = {
    // message base class:
    "cached_size",
    "serialized_size",
    // java.lang.Object:
    "class",
};

const std::unordered_set<string>* kReservedNames =
    new std::unordered_set<string>({
        "abstract",   "assert",       "boolean",   "break",      "byte",
        "case",       "catch",        "char",      "class",      "const",
        "continue",   "default",      "do",        "double",     "else",
        "enum",       "extends",      "final",     "finally",    "float",
        "for",        "goto",         "if",        "implements", "import",
        "instanceof", "int",          "interface", "long",       "native",
        "new",        "package",      "private",   "protected",  "public",
        "return",     "short",        "static",    "strictfp",   "super",
        "switch",     "synchronized", "this",      "throw",      "throws",
        "transient",  "try",          "void",      "volatile",   "while",
    });

bool IsForbidden(const std::string& field_name) {
  for (int i = 0; i < GOOGLE_ARRAYSIZE(kForbiddenWordList); ++i) {
    if (field_name == kForbiddenWordList[i]) {
      return true;
    }
  }
  return false;
}

std::string FieldName(const FieldDescriptor* field) {
  std::string field_name;
  // Groups are hacky:  The name of the field is just the lower-cased name
  // of the group type.  In Java, though, we would like to retain the original
  // capitalization of the type name.
  if (GetType(field) == FieldDescriptor::TYPE_GROUP) {
    field_name = field->message_type()->name();
  } else {
    field_name = field->name();
  }
  if (IsForbidden(field_name)) {
    // Append a trailing "#" to indicate that the name should be decorated to
    // avoid collision with other names.
    field_name += "#";
  }
  return field_name;
}


}  // namespace

void PrintGeneratedAnnotation(io::Printer* printer, char delimiter,
                              const std::string& annotation_file) {
  if (annotation_file.empty()) {
    return;
  }
  std::string ptemplate =
      "@javax.annotation.Generated(value=\"protoc\", comments=\"annotations:";
  ptemplate.push_back(delimiter);
  ptemplate.append("annotation_file");
  ptemplate.push_back(delimiter);
  ptemplate.append("\")\n");
  printer->Print(ptemplate.c_str(), "annotation_file", annotation_file);
}

void PrintEnumVerifierLogic(io::Printer* printer,
                            const FieldDescriptor* descriptor,
                            const std::map<std::string, std::string>& variables,
                            const char* var_name,
                            const char* terminating_string, bool enforce_lite) {
  std::string enum_verifier_string =
      enforce_lite ? StrCat(var_name, ".internalGetVerifier()")
                   : StrCat(
                         "new com.google.protobuf.Internal.EnumVerifier() {\n"
                         "        @java.lang.Override\n"
                         "        public boolean isInRange(int number) {\n"
                         "          return ",
                         var_name,
                         ".forNumber(number) != null;\n"
                         "        }\n"
                         "      }");
  printer->Print(
      variables,
      StrCat(enum_verifier_string, terminating_string).c_str());
}

std::string UnderscoresToCamelCase(const std::string& input,
                                   bool cap_next_letter) {
  GOOGLE_CHECK(!input.empty());
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
      if (i == 0 && !cap_next_letter) {
        // Force first letter to lower-case unless explicitly told to
        // capitalize it.
        result += input[i] + ('a' - 'A');
      } else {
        // Capital letters after the first are left as-is.
        result += input[i];
      }
      cap_next_letter = false;
    } else if ('0' <= input[i] && input[i] <= '9') {
      result += input[i];
      cap_next_letter = true;
    } else {
      cap_next_letter = true;
    }
  }
  // Add a trailing "_" if the name should be altered.
  if (input[input.size() - 1] == '#') {
    result += '_';
  }
  return result;
}

std::string UnderscoresToCamelCase(const FieldDescriptor* field) {
  return UnderscoresToCamelCase(FieldName(field), false);
}

std::string UnderscoresToCapitalizedCamelCase(const FieldDescriptor* field) {
  return UnderscoresToCamelCase(FieldName(field), true);
}

std::string CapitalizedFieldName(const FieldDescriptor* field) {
  return UnderscoresToCapitalizedCamelCase(field);
}

std::string UnderscoresToCamelCase(const MethodDescriptor* method) {
  return UnderscoresToCamelCase(method->name(), false);
}

std::string UnderscoresToCamelCaseCheckReserved(const FieldDescriptor* field) {
  std::string name = UnderscoresToCamelCase(field);
  if (kReservedNames->find(name) != kReservedNames->end()) {
    return name + "_";
  }
  return name;
}

std::string UniqueFileScopeIdentifier(const Descriptor* descriptor) {
  return "static_" + StringReplace(descriptor->full_name(), ".", "_", true);
}

std::string CamelCaseFieldName(const FieldDescriptor* field) {
  std::string fieldName = UnderscoresToCamelCase(field);
  if ('0' <= fieldName[0] && fieldName[0] <= '9') {
    return '_' + fieldName;
  }
  return fieldName;
}

std::string StripProto(const std::string& filename) {
  if (HasSuffixString(filename, ".protodevel")) {
    return StripSuffixString(filename, ".protodevel");
  } else {
    return StripSuffixString(filename, ".proto");
  }
}

std::string FileClassName(const FileDescriptor* file, bool immutable) {
  ClassNameResolver name_resolver;
  return name_resolver.GetFileClassName(file, immutable);
}

std::string FileJavaPackage(const FileDescriptor* file, bool immutable) {
  std::string result;

  if (file->options().has_java_package()) {
    result = file->options().java_package();
  } else {
    result = kDefaultPackage;
    if (!file->package().empty()) {
      if (!result.empty()) result += '.';
      result += file->package();
    }
  }

  return result;
}

std::string FileJavaPackage(const FileDescriptor* file) {
  return FileJavaPackage(file, true /* immutable */);
}

std::string JavaPackageToDir(std::string package_name) {
  std::string package_dir = StringReplace(package_name, ".", "/", true);
  if (!package_dir.empty()) package_dir += "/";
  return package_dir;
}

std::string ClassName(const Descriptor* descriptor) {
  ClassNameResolver name_resolver;
  return name_resolver.GetClassName(descriptor, true);
}

std::string ClassName(const EnumDescriptor* descriptor) {
  ClassNameResolver name_resolver;
  return name_resolver.GetClassName(descriptor, true);
}

std::string ClassName(const ServiceDescriptor* descriptor) {
  ClassNameResolver name_resolver;
  return name_resolver.GetClassName(descriptor, true);
}

std::string ClassName(const FileDescriptor* descriptor) {
  ClassNameResolver name_resolver;
  return name_resolver.GetClassName(descriptor, true);
}


std::string ExtraMessageInterfaces(const Descriptor* descriptor) {
  std::string interfaces = "// @@protoc_insertion_point(message_implements:" +
                           descriptor->full_name() + ")";
  return interfaces;
}


std::string ExtraBuilderInterfaces(const Descriptor* descriptor) {
  std::string interfaces = "// @@protoc_insertion_point(builder_implements:" +
                           descriptor->full_name() + ")";
  return interfaces;
}

std::string ExtraMessageOrBuilderInterfaces(const Descriptor* descriptor) {
  std::string interfaces = "// @@protoc_insertion_point(interface_extends:" +
                           descriptor->full_name() + ")";
  return interfaces;
}

std::string FieldConstantName(const FieldDescriptor* field) {
  std::string name = field->name() + "_FIELD_NUMBER";
  UpperString(&name);
  return name;
}

FieldDescriptor::Type GetType(const FieldDescriptor* field) {
  return field->type();
}

JavaType GetJavaType(const FieldDescriptor* field) {
  switch (GetType(field)) {
    case FieldDescriptor::TYPE_INT32:
    case FieldDescriptor::TYPE_UINT32:
    case FieldDescriptor::TYPE_SINT32:
    case FieldDescriptor::TYPE_FIXED32:
    case FieldDescriptor::TYPE_SFIXED32:
      return JAVATYPE_INT;

    case FieldDescriptor::TYPE_INT64:
    case FieldDescriptor::TYPE_UINT64:
    case FieldDescriptor::TYPE_SINT64:
    case FieldDescriptor::TYPE_FIXED64:
    case FieldDescriptor::TYPE_SFIXED64:
      return JAVATYPE_LONG;

    case FieldDescriptor::TYPE_FLOAT:
      return JAVATYPE_FLOAT;

    case FieldDescriptor::TYPE_DOUBLE:
      return JAVATYPE_DOUBLE;

    case FieldDescriptor::TYPE_BOOL:
      return JAVATYPE_BOOLEAN;

    case FieldDescriptor::TYPE_STRING:
      return JAVATYPE_STRING;

    case FieldDescriptor::TYPE_BYTES:
      return JAVATYPE_BYTES;

    case FieldDescriptor::TYPE_ENUM:
      return JAVATYPE_ENUM;

    case FieldDescriptor::TYPE_GROUP:
    case FieldDescriptor::TYPE_MESSAGE:
      return JAVATYPE_MESSAGE;

      // No default because we want the compiler to complain if any new
      // types are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return JAVATYPE_INT;
}

const char* PrimitiveTypeName(JavaType type) {
  switch (type) {
    case JAVATYPE_INT:
      return "int";
    case JAVATYPE_LONG:
      return "long";
    case JAVATYPE_FLOAT:
      return "float";
    case JAVATYPE_DOUBLE:
      return "double";
    case JAVATYPE_BOOLEAN:
      return "boolean";
    case JAVATYPE_STRING:
      return "java.lang.String";
    case JAVATYPE_BYTES:
      return "com.google.protobuf.ByteString";
    case JAVATYPE_ENUM:
      return NULL;
    case JAVATYPE_MESSAGE:
      return NULL;

      // No default because we want the compiler to complain if any new
      // JavaTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

const char* PrimitiveTypeName(const FieldDescriptor* descriptor) {
  return PrimitiveTypeName(GetJavaType(descriptor));
}

const char* BoxedPrimitiveTypeName(JavaType type) {
  switch (type) {
    case JAVATYPE_INT:
      return "java.lang.Integer";
    case JAVATYPE_LONG:
      return "java.lang.Long";
    case JAVATYPE_FLOAT:
      return "java.lang.Float";
    case JAVATYPE_DOUBLE:
      return "java.lang.Double";
    case JAVATYPE_BOOLEAN:
      return "java.lang.Boolean";
    case JAVATYPE_STRING:
      return "java.lang.String";
    case JAVATYPE_BYTES:
      return "com.google.protobuf.ByteString";
    case JAVATYPE_ENUM:
      return NULL;
    case JAVATYPE_MESSAGE:
      return NULL;

      // No default because we want the compiler to complain if any new
      // JavaTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

const char* BoxedPrimitiveTypeName(const FieldDescriptor* descriptor) {
  return BoxedPrimitiveTypeName(GetJavaType(descriptor));
}

std::string GetOneofStoredType(const FieldDescriptor* field) {
  const JavaType javaType = GetJavaType(field);
  switch (javaType) {
    case JAVATYPE_ENUM:
      return "java.lang.Integer";
    case JAVATYPE_MESSAGE:
      return ClassName(field->message_type());
    default:
      return BoxedPrimitiveTypeName(javaType);
  }
}

const char* FieldTypeName(FieldDescriptor::Type field_type) {
  switch (field_type) {
    case FieldDescriptor::TYPE_INT32:
      return "INT32";
    case FieldDescriptor::TYPE_UINT32:
      return "UINT32";
    case FieldDescriptor::TYPE_SINT32:
      return "SINT32";
    case FieldDescriptor::TYPE_FIXED32:
      return "FIXED32";
    case FieldDescriptor::TYPE_SFIXED32:
      return "SFIXED32";
    case FieldDescriptor::TYPE_INT64:
      return "INT64";
    case FieldDescriptor::TYPE_UINT64:
      return "UINT64";
    case FieldDescriptor::TYPE_SINT64:
      return "SINT64";
    case FieldDescriptor::TYPE_FIXED64:
      return "FIXED64";
    case FieldDescriptor::TYPE_SFIXED64:
      return "SFIXED64";
    case FieldDescriptor::TYPE_FLOAT:
      return "FLOAT";
    case FieldDescriptor::TYPE_DOUBLE:
      return "DOUBLE";
    case FieldDescriptor::TYPE_BOOL:
      return "BOOL";
    case FieldDescriptor::TYPE_STRING:
      return "STRING";
    case FieldDescriptor::TYPE_BYTES:
      return "BYTES";
    case FieldDescriptor::TYPE_ENUM:
      return "ENUM";
    case FieldDescriptor::TYPE_GROUP:
      return "GROUP";
    case FieldDescriptor::TYPE_MESSAGE:
      return "MESSAGE";

      // No default because we want the compiler to complain if any new
      // types are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

bool AllAscii(const std::string& text) {
  for (int i = 0; i < text.size(); i++) {
    if ((text[i] & 0x80) != 0) {
      return false;
    }
  }
  return true;
}

std::string DefaultValue(const FieldDescriptor* field, bool immutable,
                         ClassNameResolver* name_resolver) {
  // Switch on CppType since we need to know which default_value_* method
  // of FieldDescriptor to call.
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return StrCat(field->default_value_int32());
    case FieldDescriptor::CPPTYPE_UINT32:
      // Need to print as a signed int since Java has no unsigned.
      return StrCat(static_cast<int32>(field->default_value_uint32()));
    case FieldDescriptor::CPPTYPE_INT64:
      return StrCat(field->default_value_int64()) + "L";
    case FieldDescriptor::CPPTYPE_UINT64:
      return StrCat(static_cast<int64>(field->default_value_uint64())) +
             "L";
    case FieldDescriptor::CPPTYPE_DOUBLE: {
      double value = field->default_value_double();
      if (value == std::numeric_limits<double>::infinity()) {
        return "Double.POSITIVE_INFINITY";
      } else if (value == -std::numeric_limits<double>::infinity()) {
        return "Double.NEGATIVE_INFINITY";
      } else if (value != value) {
        return "Double.NaN";
      } else {
        return SimpleDtoa(value) + "D";
      }
    }
    case FieldDescriptor::CPPTYPE_FLOAT: {
      float value = field->default_value_float();
      if (value == std::numeric_limits<float>::infinity()) {
        return "Float.POSITIVE_INFINITY";
      } else if (value == -std::numeric_limits<float>::infinity()) {
        return "Float.NEGATIVE_INFINITY";
      } else if (value != value) {
        return "Float.NaN";
      } else {
        return SimpleFtoa(value) + "F";
      }
    }
    case FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() ? "true" : "false";
    case FieldDescriptor::CPPTYPE_STRING:
      if (GetType(field) == FieldDescriptor::TYPE_BYTES) {
        if (field->has_default_value()) {
          // See comments in Internal.java for gory details.
          return strings::Substitute(
              "com.google.protobuf.Internal.bytesDefaultValue(\"$0\")",
              CEscape(field->default_value_string()));
        } else {
          return "com.google.protobuf.ByteString.EMPTY";
        }
      } else {
        if (AllAscii(field->default_value_string())) {
          // All chars are ASCII.  In this case CEscape() works fine.
          return "\"" + CEscape(field->default_value_string()) + "\"";
        } else {
          // See comments in Internal.java for gory details.
          return strings::Substitute(
              "com.google.protobuf.Internal.stringDefaultValue(\"$0\")",
              CEscape(field->default_value_string()));
        }
      }

    case FieldDescriptor::CPPTYPE_ENUM:
      return name_resolver->GetClassName(field->enum_type(), immutable) + "." +
             field->default_value_enum()->name();

    case FieldDescriptor::CPPTYPE_MESSAGE:
      return name_resolver->GetClassName(field->message_type(), immutable) +
             ".getDefaultInstance()";

      // No default because we want the compiler to complain if any new
      // types are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return "";
}

bool IsDefaultValueJavaDefault(const FieldDescriptor* field) {
  // Switch on CppType since we need to know which default_value_* method
  // of FieldDescriptor to call.
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return field->default_value_int32() == 0;
    case FieldDescriptor::CPPTYPE_UINT32:
      return field->default_value_uint32() == 0;
    case FieldDescriptor::CPPTYPE_INT64:
      return field->default_value_int64() == 0L;
    case FieldDescriptor::CPPTYPE_UINT64:
      return field->default_value_uint64() == 0L;
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return field->default_value_double() == 0.0;
    case FieldDescriptor::CPPTYPE_FLOAT:
      return field->default_value_float() == 0.0;
    case FieldDescriptor::CPPTYPE_BOOL:
      return field->default_value_bool() == false;
    case FieldDescriptor::CPPTYPE_ENUM:
      return field->default_value_enum()->number() == 0;
    case FieldDescriptor::CPPTYPE_STRING:
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return false;

      // No default because we want the compiler to complain if any new
      // types are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return false;
}

bool IsByteStringWithCustomDefaultValue(const FieldDescriptor* field) {
  return GetJavaType(field) == JAVATYPE_BYTES &&
         field->default_value_string() != "";
}

const char* bit_masks[] = {
    "0x00000001", "0x00000002", "0x00000004", "0x00000008",
    "0x00000010", "0x00000020", "0x00000040", "0x00000080",

    "0x00000100", "0x00000200", "0x00000400", "0x00000800",
    "0x00001000", "0x00002000", "0x00004000", "0x00008000",

    "0x00010000", "0x00020000", "0x00040000", "0x00080000",
    "0x00100000", "0x00200000", "0x00400000", "0x00800000",

    "0x01000000", "0x02000000", "0x04000000", "0x08000000",
    "0x10000000", "0x20000000", "0x40000000", "0x80000000",
};

std::string GetBitFieldName(int index) {
  std::string varName = "bitField";
  varName += StrCat(index);
  varName += "_";
  return varName;
}

std::string GetBitFieldNameForBit(int bitIndex) {
  return GetBitFieldName(bitIndex / 32);
}

namespace {

std::string GenerateGetBitInternal(const std::string& prefix, int bitIndex) {
  std::string varName = prefix + GetBitFieldNameForBit(bitIndex);
  int bitInVarIndex = bitIndex % 32;

  std::string mask = bit_masks[bitInVarIndex];
  std::string result = "((" + varName + " & " + mask + ") != 0)";
  return result;
}

std::string GenerateSetBitInternal(const std::string& prefix, int bitIndex) {
  std::string varName = prefix + GetBitFieldNameForBit(bitIndex);
  int bitInVarIndex = bitIndex % 32;

  std::string mask = bit_masks[bitInVarIndex];
  std::string result = varName + " |= " + mask;
  return result;
}

}  // namespace

std::string GenerateGetBit(int bitIndex) {
  return GenerateGetBitInternal("", bitIndex);
}

std::string GenerateSetBit(int bitIndex) {
  return GenerateSetBitInternal("", bitIndex);
}

std::string GenerateClearBit(int bitIndex) {
  std::string varName = GetBitFieldNameForBit(bitIndex);
  int bitInVarIndex = bitIndex % 32;

  std::string mask = bit_masks[bitInVarIndex];
  std::string result = varName + " = (" + varName + " & ~" + mask + ")";
  return result;
}

std::string GenerateGetBitFromLocal(int bitIndex) {
  return GenerateGetBitInternal("from_", bitIndex);
}

std::string GenerateSetBitToLocal(int bitIndex) {
  return GenerateSetBitInternal("to_", bitIndex);
}

std::string GenerateGetBitMutableLocal(int bitIndex) {
  return GenerateGetBitInternal("mutable_", bitIndex);
}

std::string GenerateSetBitMutableLocal(int bitIndex) {
  return GenerateSetBitInternal("mutable_", bitIndex);
}

bool IsReferenceType(JavaType type) {
  switch (type) {
    case JAVATYPE_INT:
      return false;
    case JAVATYPE_LONG:
      return false;
    case JAVATYPE_FLOAT:
      return false;
    case JAVATYPE_DOUBLE:
      return false;
    case JAVATYPE_BOOLEAN:
      return false;
    case JAVATYPE_STRING:
      return true;
    case JAVATYPE_BYTES:
      return true;
    case JAVATYPE_ENUM:
      return true;
    case JAVATYPE_MESSAGE:
      return true;

      // No default because we want the compiler to complain if any new
      // JavaTypes are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return false;
}

const char* GetCapitalizedType(const FieldDescriptor* field, bool immutable) {
  switch (GetType(field)) {
    case FieldDescriptor::TYPE_INT32:
      return "Int32";
    case FieldDescriptor::TYPE_UINT32:
      return "UInt32";
    case FieldDescriptor::TYPE_SINT32:
      return "SInt32";
    case FieldDescriptor::TYPE_FIXED32:
      return "Fixed32";
    case FieldDescriptor::TYPE_SFIXED32:
      return "SFixed32";
    case FieldDescriptor::TYPE_INT64:
      return "Int64";
    case FieldDescriptor::TYPE_UINT64:
      return "UInt64";
    case FieldDescriptor::TYPE_SINT64:
      return "SInt64";
    case FieldDescriptor::TYPE_FIXED64:
      return "Fixed64";
    case FieldDescriptor::TYPE_SFIXED64:
      return "SFixed64";
    case FieldDescriptor::TYPE_FLOAT:
      return "Float";
    case FieldDescriptor::TYPE_DOUBLE:
      return "Double";
    case FieldDescriptor::TYPE_BOOL:
      return "Bool";
    case FieldDescriptor::TYPE_STRING:
      return "String";
    case FieldDescriptor::TYPE_BYTES: {
      return "Bytes";
    }
    case FieldDescriptor::TYPE_ENUM:
      return "Enum";
    case FieldDescriptor::TYPE_GROUP:
      return "Group";
    case FieldDescriptor::TYPE_MESSAGE:
      return "Message";

      // No default because we want the compiler to complain if any new
      // types are added.
  }

  GOOGLE_LOG(FATAL) << "Can't get here.";
  return NULL;
}

// For encodings with fixed sizes, returns that size in bytes.  Otherwise
// returns -1.
int FixedSize(FieldDescriptor::Type type) {
  switch (type) {
    case FieldDescriptor::TYPE_INT32:
      return -1;
    case FieldDescriptor::TYPE_INT64:
      return -1;
    case FieldDescriptor::TYPE_UINT32:
      return -1;
    case FieldDescriptor::TYPE_UINT64:
      return -1;
    case FieldDescriptor::TYPE_SINT32:
      return -1;
    case FieldDescriptor::TYPE_SINT64:
      return -1;
    case FieldDescriptor::TYPE_FIXED32:
      return WireFormatLite::kFixed32Size;
    case FieldDescriptor::TYPE_FIXED64:
      return WireFormatLite::kFixed64Size;
    case FieldDescriptor::TYPE_SFIXED32:
      return WireFormatLite::kSFixed32Size;
    case FieldDescriptor::TYPE_SFIXED64:
      return WireFormatLite::kSFixed64Size;
    case FieldDescriptor::TYPE_FLOAT:
      return WireFormatLite::kFloatSize;
    case FieldDescriptor::TYPE_DOUBLE:
      return WireFormatLite::kDoubleSize;

    case FieldDescriptor::TYPE_BOOL:
      return WireFormatLite::kBoolSize;
    case FieldDescriptor::TYPE_ENUM:
      return -1;

    case FieldDescriptor::TYPE_STRING:
      return -1;
    case FieldDescriptor::TYPE_BYTES:
      return -1;
    case FieldDescriptor::TYPE_GROUP:
      return -1;
    case FieldDescriptor::TYPE_MESSAGE:
      return -1;

      // No default because we want the compiler to complain if any new
      // types are added.
  }
  GOOGLE_LOG(FATAL) << "Can't get here.";
  return -1;
}

// Sort the fields of the given Descriptor by number into a new[]'d array
// and return it. The caller should delete the returned array.
const FieldDescriptor** SortFieldsByNumber(const Descriptor* descriptor) {
  const FieldDescriptor** fields =
      new const FieldDescriptor*[descriptor->field_count()];
  for (int i = 0; i < descriptor->field_count(); i++) {
    fields[i] = descriptor->field(i);
  }
  std::sort(fields, fields + descriptor->field_count(),
            FieldOrderingByNumber());
  return fields;
}

// Returns true if the message type has any required fields.  If it doesn't,
// we can optimize out calls to its isInitialized() method.
//
// already_seen is used to avoid checking the same type multiple times
// (and also to protect against recursion).
bool HasRequiredFields(const Descriptor* type,
                       std::unordered_set<const Descriptor*>* already_seen) {
  if (already_seen->count(type) > 0) {
    // The type is already in cache.  This means that either:
    // a. The type has no required fields.
    // b. We are in the midst of checking if the type has required fields,
    //    somewhere up the stack.  In this case, we know that if the type
    //    has any required fields, they'll be found when we return to it,
    //    and the whole call to HasRequiredFields() will return true.
    //    Therefore, we don't have to check if this type has required fields
    //    here.
    return false;
  }
  already_seen->insert(type);

  // If the type has extensions, an extension with message type could contain
  // required fields, so we have to be conservative and assume such an
  // extension exists.
  if (type->extension_range_count() > 0) return true;

  for (int i = 0; i < type->field_count(); i++) {
    const FieldDescriptor* field = type->field(i);
    if (field->is_required()) {
      return true;
    }
    if (GetJavaType(field) == JAVATYPE_MESSAGE) {
      if (HasRequiredFields(field->message_type(), already_seen)) {
        return true;
      }
    }
  }

  return false;
}

bool HasRequiredFields(const Descriptor* type) {
  std::unordered_set<const Descriptor*> already_seen;
  return HasRequiredFields(type, &already_seen);
}

bool HasRepeatedFields(const Descriptor* descriptor) {
  for (int i = 0; i < descriptor->field_count(); ++i) {
    const FieldDescriptor* field = descriptor->field(i);
    if (field->is_repeated()) {
      return true;
    }
  }
  return false;
}

// Encode an unsigned 32-bit value into a sequence of UTF-16 characters.
//
// If the value is in [0x0000, 0xD7FF], we encode it with a single character
// with the same numeric value.
//
// If the value is larger than 0xD7FF, we encode its lowest 13 bits into a
// character in the range [0xE000, 0xFFFF] by combining these 13 bits with
// 0xE000 using logic-or. Then we shift the value to the right by 13 bits, and
// encode the remaining value by repeating this same process until we get to
// a value in [0x0000, 0xD7FF] where we will encode it using a character with
// the same numeric value.
//
// Note that we only use code points in [0x0000, 0xD7FF] and [0xE000, 0xFFFF].
// There will be no surrogate pairs in the encoded character sequence.
void WriteUInt32ToUtf16CharSequence(uint32 number,
                                    std::vector<uint16>* output) {
  // For values in [0x0000, 0xD7FF], only use one char to encode it.
  if (number < 0xD800) {
    output->push_back(static_cast<uint16>(number));
    return;
  }
  // Encode into multiple chars. All except the last char will be in the range
  // [0xE000, 0xFFFF], and the last char will be in the range [0x0000, 0xD7FF].
  // Note that we don't use any value in range [0xD800, 0xDFFF] because they
  // have to come in pairs and the encoding is just more space-efficient w/o
  // them.
  while (number >= 0xD800) {
    // [0xE000, 0xFFFF] can represent 13 bits of info.
    output->push_back(static_cast<uint16>(0xE000 | (number & 0x1FFF)));
    number >>= 13;
  }
  output->push_back(static_cast<uint16>(number));
}

int GetExperimentalJavaFieldTypeForSingular(const FieldDescriptor* field) {
  // j/c/g/protobuf/FieldType.java lists field types in a slightly different
  // order from FieldDescriptor::Type so we can't do a simple cast.
  //
  // TODO(xiaofeng): Make j/c/g/protobuf/FieldType.java follow the same order.
  int result = field->type();
  if (result == FieldDescriptor::TYPE_GROUP) {
    return 17;
  } else if (result < FieldDescriptor::TYPE_GROUP) {
    return result - 1;
  } else {
    return result - 2;
  }
}

int GetExperimentalJavaFieldTypeForRepeated(const FieldDescriptor* field) {
  if (field->type() == FieldDescriptor::TYPE_GROUP) {
    return 49;
  } else {
    return GetExperimentalJavaFieldTypeForSingular(field) + 18;
  }
}

int GetExperimentalJavaFieldTypeForPacked(const FieldDescriptor* field) {
  int result = field->type();
  if (result < FieldDescriptor::TYPE_STRING) {
    return result + 34;
  } else if (result > FieldDescriptor::TYPE_BYTES) {
    return result + 30;
  } else {
    GOOGLE_LOG(FATAL) << field->full_name() << " can't be packed.";
    return 0;
  }
}

int GetExperimentalJavaFieldType(const FieldDescriptor* field) {
  static const int kMapFieldType = 50;
  static const int kOneofFieldTypeOffset = 51;
  static const int kRequiredBit = 0x100;
  static const int kUtf8CheckBit = 0x200;
  static const int kCheckInitialized = 0x400;
  static const int kMapWithProto2EnumValue = 0x800;
  int extra_bits = field->is_required() ? kRequiredBit : 0;
  if (field->type() == FieldDescriptor::TYPE_STRING && CheckUtf8(field)) {
    extra_bits |= kUtf8CheckBit;
  }
  if (field->is_required() || (GetJavaType(field) == JAVATYPE_MESSAGE &&
                               HasRequiredFields(field->message_type()))) {
    extra_bits |= kCheckInitialized;
  }

  if (field->is_map()) {
    if (SupportFieldPresence(field->file())) {
      const FieldDescriptor* value =
          field->message_type()->FindFieldByName("value");
      if (GetJavaType(value) == JAVATYPE_ENUM) {
        extra_bits |= kMapWithProto2EnumValue;
      }
    }
    return kMapFieldType | extra_bits;
  } else if (field->is_packed()) {
    return GetExperimentalJavaFieldTypeForPacked(field);
  } else if (field->is_repeated()) {
    return GetExperimentalJavaFieldTypeForRepeated(field) | extra_bits;
  } else if (field->containing_oneof() != NULL) {
    return (GetExperimentalJavaFieldTypeForSingular(field) +
            kOneofFieldTypeOffset) |
           extra_bits;
  } else {
    return GetExperimentalJavaFieldTypeForSingular(field) | extra_bits;
  }
}

// Escape a UTF-16 character to be embedded in a Java string.
void EscapeUtf16ToString(uint16 code, std::string* output) {
  if (code == '\t') {
    output->append("\\t");
  } else if (code == '\b') {
    output->append("\\b");
  } else if (code == '\n') {
    output->append("\\n");
  } else if (code == '\r') {
    output->append("\\r");
  } else if (code == '\f') {
    output->append("\\f");
  } else if (code == '\'') {
    output->append("\\'");
  } else if (code == '\"') {
    output->append("\\\"");
  } else if (code == '\\') {
    output->append("\\\\");
  } else if (code >= 0x20 && code <= 0x7f) {
    output->push_back(static_cast<char>(code));
  } else {
    output->append(StringPrintf("\\u%04x", code));
  }
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
