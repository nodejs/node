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

#include <google/protobuf/compiler/cpp/cpp_string_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>



namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void SetStringVariables(const FieldDescriptor* descriptor,
                        std::map<std::string, std::string>* variables,
                        const Options& options) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["default"] = DefaultValue(options, descriptor);
  (*variables)["default_length"] =
      StrCat(descriptor->default_value_string().length());
  std::string default_variable_string = MakeDefaultName(descriptor);
  (*variables)["default_variable_name"] = default_variable_string;
  (*variables)["default_variable"] =
      descriptor->default_value_string().empty()
          ? "&::" + (*variables)["proto_ns"] +
                "::internal::GetEmptyStringAlreadyInited()"
          : "&" + QualifiedClassName(descriptor->containing_type(), options) +
                "::" + default_variable_string + ".get()";
  (*variables)["pointer_type"] =
      descriptor->type() == FieldDescriptor::TYPE_BYTES ? "void" : "char";
  (*variables)["null_check"] = (*variables)["DCHK"] + "(value != nullptr);\n";
  // NOTE: Escaped here to unblock proto1->proto2 migration.
  // TODO(liujisi): Extend this to apply for other conflicting methods.
  (*variables)["release_name"] =
      SafeFunctionName(descriptor->containing_type(), descriptor, "release_");
  (*variables)["full_name"] = descriptor->full_name();

  if (options.opensource_runtime) {
    (*variables)["string_piece"] = "::std::string";
  } else {
    (*variables)["string_piece"] = "::StringPiece";
  }

  (*variables)["lite"] =
      HasDescriptorMethods(descriptor->file(), options) ? "" : "Lite";
}

}  // namespace

// ===================================================================

StringFieldGenerator::StringFieldGenerator(const FieldDescriptor* descriptor,
                                           const Options& options)
    : FieldGenerator(descriptor, options),
      lite_(!HasDescriptorMethods(descriptor->file(), options)),
      inlined_(IsStringInlined(descriptor, options)) {
  SetStringVariables(descriptor, &variables_, options);
}

StringFieldGenerator::~StringFieldGenerator() {}

void StringFieldGenerator::GeneratePrivateMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    format("::$proto_ns$::internal::InlinedStringField $name$_;\n");
  } else {
    // N.B. that we continue to use |ArenaStringPtr| instead of |string*| for
    // string fields, even when SupportArenas(descriptor_) == false. Why?  The
    // simple answer is to avoid unmaintainable complexity. The reflection code
    // assumes ArenaStringPtrs. These are *almost* in-memory-compatible with
    // string*, except for the pointer tags and related ownership semantics. We
    // could modify the runtime code to use string* for the
    // not-supporting-arenas case, but this would require a way to detect which
    // type of class was generated (adding overhead and complexity to
    // GeneratedMessageReflection) and littering the runtime code paths with
    // conditionals. It's simpler to stick with this but use lightweight
    // accessors that assume arena == NULL.  There should be very little
    // overhead anyway because it's just a tagged pointer in-memory.
    format("::$proto_ns$::internal::ArenaStringPtr $name$_;\n");
  }
}

void StringFieldGenerator::GenerateStaticMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    // We make the default instance public, so it can be initialized by
    // non-friend code.
    format(
        "public:\n"
        "static ::$proto_ns$::internal::ExplicitlyConstructed<std::string>"
        " $default_variable_name$;\n"
        "private:\n");
  }
}

void StringFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // If we're using StringFieldGenerator for a field with a ctype, it's
  // because that ctype isn't actually implemented.  In particular, this is
  // true of ctype=CORD and ctype=STRING_PIECE in the open source release.
  // We aren't releasing Cord because it has too many Google-specific
  // dependencies and we aren't releasing StringPiece because it's hardly
  // useful outside of Google and because it would get confusing to have
  // multiple instances of the StringPiece class in different libraries (PCRE
  // already includes it for their C++ bindings, which came from Google).
  //
  // In any case, we make all the accessors private while still actually
  // using a string to represent the field internally.  This way, we can
  // guarantee that if we do ever implement the ctype, it won't break any
  // existing users who might be -- for whatever reason -- already using .proto
  // files that applied the ctype.  The field can still be accessed via the
  // reflection interface since the reflection interface is independent of
  // the string's underlying representation.

  bool unknown_ctype = descriptor_->options().ctype() !=
                       EffectiveStringCType(descriptor_, options_);

  if (unknown_ctype) {
    format.Outdent();
    format(
        " private:\n"
        "  // Hidden due to unknown ctype option.\n");
    format.Indent();
  }

  format(
      "$deprecated_attr$const std::string& ${1$$name$$}$() const;\n"
      "$deprecated_attr$void ${1$set_$name$$}$(const std::string& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(std::string&& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(const char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$set_$name$$}$(::StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$set_$name$$}$(const $pointer_type$* "
      "value, size_t size)"
      ";\n"
      "$deprecated_attr$std::string* ${1$mutable_$name$$}$();\n"
      "$deprecated_attr$std::string* ${1$$release_name$$}$();\n"
      "$deprecated_attr$void ${1$set_allocated_$name$$}$(std::string* "
      "$name$);\n",
      descriptor_);
  if (options_.opensource_runtime) {
    if (SupportsArenas(descriptor_)) {
      format(
          "$GOOGLE_PROTOBUF$_RUNTIME_DEPRECATED(\"The unsafe_arena_ accessors "
          "for\"\n"
          "\"    string fields are deprecated and will be removed in a\"\n"
          "\"    future release.\")\n"
          "std::string* ${1$unsafe_arena_release_$name$$}$();\n"
          "$GOOGLE_PROTOBUF$_RUNTIME_DEPRECATED(\"The unsafe_arena_ accessors "
          "for\"\n"
          "\"    string fields are deprecated and will be removed in a\"\n"
          "\"    future release.\")\n"
          "void ${1$unsafe_arena_set_allocated_$name$$}$(\n"
          "    std::string* $name$);\n",
          descriptor_);
    }
  }

  if (unknown_ctype) {
    format.Outdent();
    format(" public:\n");
    format.Indent();
  }
}

void StringFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_)) {
    format(
        "inline const std::string& $classname$::$name$() const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  return $name$_.Get();\n"
        "}\n"
        "inline void $classname$::set_$name$(const std::string& value) {\n"
        "  $set_hasbit$\n"
        "  $name$_.Set$lite$($default_variable$, value, GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(std::string&& value) {\n"
        "  $set_hasbit$\n"
        "  $name$_.Set$lite$(\n"
        "    $default_variable$, ::std::move(value), GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(const char* value) {\n"
        "  $null_check$"
        "  $set_hasbit$\n"
        "  $name$_.Set$lite$($default_variable$, $string_piece$(value),\n"
        "              GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
        "}\n");
    if (!options_.opensource_runtime) {
      format(
          "inline void $classname$::set_$name$(::StringPiece value) {\n"
          "  $set_hasbit$\n"
          "  $name$_.Set$lite$($default_variable$, value, "
          "GetArenaNoVirtual());\n"
          "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
          "}\n");
    }
    format(
        "inline "
        "void $classname$::set_$name$(const $pointer_type$* value,\n"
        "    size_t size) {\n"
        "  $set_hasbit$\n"
        "  $name$_.Set$lite$($default_variable$, $string_piece$(\n"
        "      reinterpret_cast<const char*>(value), size), "
        "GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
        "}\n"
        "inline std::string* $classname$::mutable_$name$() {\n"
        "  $set_hasbit$\n"
        "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
        "  return $name$_.Mutable($default_variable$, GetArenaNoVirtual());\n"
        "}\n"
        "inline std::string* $classname$::$release_name$() {\n"
        "  // @@protoc_insertion_point(field_release:$full_name$)\n");

    if (HasFieldPresence(descriptor_->file())) {
      format(
          "  if (!has_$name$()) {\n"
          "    return nullptr;\n"
          "  }\n"
          "  $clear_hasbit$\n"
          "  return $name$_.ReleaseNonDefault("
          "$default_variable$, GetArenaNoVirtual());\n");
    } else {
      format(
          "  $clear_hasbit$\n"
          "  return $name$_.Release($default_variable$, "
          "GetArenaNoVirtual());\n");
    }

    format(
        "}\n"
        "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
        "  if ($name$ != nullptr) {\n"
        "    $set_hasbit$\n"
        "  } else {\n"
        "    $clear_hasbit$\n"
        "  }\n"
        "  $name$_.SetAllocated($default_variable$, $name$,\n"
        "      GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
        "}\n");
    if (options_.opensource_runtime) {
      format(
          "inline std::string* $classname$::unsafe_arena_release_$name$() {\n"
          "  // "
          "@@protoc_insertion_point(field_unsafe_arena_release:$full_name$)\n"
          "  $DCHK$(GetArenaNoVirtual() != nullptr);\n"
          "  $clear_hasbit$\n"
          "  return $name$_.UnsafeArenaRelease($default_variable$,\n"
          "      GetArenaNoVirtual());\n"
          "}\n"
          "inline void $classname$::unsafe_arena_set_allocated_$name$(\n"
          "    std::string* $name$) {\n"
          "  $DCHK$(GetArenaNoVirtual() != nullptr);\n"
          "  if ($name$ != nullptr) {\n"
          "    $set_hasbit$\n"
          "  } else {\n"
          "    $clear_hasbit$\n"
          "  }\n"
          "  $name$_.UnsafeArenaSetAllocated($default_variable$,\n"
          "      $name$, GetArenaNoVirtual());\n"
          "  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:"
          "$full_name$)\n"
          "}\n");
    }
  } else {
    // No-arena case.
    format(
        "inline const std::string& $classname$::$name$() const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  return $name$_.GetNoArena();\n"
        "}\n"
        "inline void $classname$::set_$name$(const std::string& value) {\n"
        "  $set_hasbit$\n"
        "  $name$_.SetNoArena($default_variable$, value);\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(std::string&& value) {\n"
        "  $set_hasbit$\n"
        "  $name$_.SetNoArena(\n"
        "    $default_variable$, ::std::move(value));\n"
        "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(const char* value) {\n"
        "  $null_check$"
        "  $set_hasbit$\n"
        "  $name$_.SetNoArena($default_variable$, $string_piece$(value));\n"
        "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
        "}\n");
    if (!options_.opensource_runtime) {
      format(
          "inline void $classname$::set_$name$(::StringPiece value) {\n"
          "  $set_hasbit$\n"
          "  $name$_.SetNoArena($default_variable$, value);\n"
          "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
          "}\n");
    }
    format(
        "inline "
        "void $classname$::set_$name$(const $pointer_type$* value, "
        "size_t size) {\n"
        "  $set_hasbit$\n"
        "  $name$_.SetNoArena($default_variable$,\n"
        "      $string_piece$(reinterpret_cast<const char*>(value), size));\n"
        "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
        "}\n"
        "inline std::string* $classname$::mutable_$name$() {\n"
        "  $set_hasbit$\n"
        "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
        "  return $name$_.MutableNoArena($default_variable$);\n"
        "}\n"
        "inline std::string* $classname$::$release_name$() {\n"
        "  // @@protoc_insertion_point(field_release:$full_name$)\n");

    if (HasFieldPresence(descriptor_->file())) {
      format(
          "  if (!has_$name$()) {\n"
          "    return nullptr;\n"
          "  }\n"
          "  $clear_hasbit$\n"
          "  return $name$_.ReleaseNonDefaultNoArena($default_variable$);\n");
    } else {
      format(
          "  $clear_hasbit$\n"
          "  return $name$_.ReleaseNoArena($default_variable$);\n");
    }

    format(
        "}\n"
        "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
        "  if ($name$ != nullptr) {\n"
        "    $set_hasbit$\n"
        "  } else {\n"
        "    $clear_hasbit$\n"
        "  }\n"
        "  $name$_.SetAllocatedNoArena($default_variable$, $name$);\n"
        "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
        "}\n");
  }
}

void StringFieldGenerator::GenerateNonInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    // Initialized in GenerateDefaultInstanceAllocator.
    format(
        "::$proto_ns$::internal::ExplicitlyConstructed<std::string> "
        "$classname$::$default_variable_name$;\n");
  }
}

void StringFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  // Two-dimension specialization here: supporting arenas or not, and default
  // value is the empty string or not. Complexity here ensures the minimal
  // number of branches / amount of extraneous code at runtime (given that the
  // below methods are inlined one-liners)!
  if (SupportsArenas(descriptor_)) {
    if (descriptor_->default_value_string().empty()) {
      format(
          "$name$_.ClearToEmpty($default_variable$, GetArenaNoVirtual());\n");
    } else {
      format(
          "$name$_.ClearToDefault($default_variable$, GetArenaNoVirtual());\n");
    }
  } else {
    if (descriptor_->default_value_string().empty()) {
      format("$name$_.ClearToEmptyNoArena($default_variable$);\n");
    } else {
      format("$name$_.ClearToDefaultNoArena($default_variable$);\n");
    }
  }
}

void StringFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // Two-dimension specialization here: supporting arenas, field presence, or
  // not, and default value is the empty string or not. Complexity here ensures
  // the minimal number of branches / amount of extraneous code at runtime
  // (given that the below methods are inlined one-liners)!

  // If we have field presence, then the Clear() method of the protocol buffer
  // will have checked that this field is set.  If so, we can avoid redundant
  // checks against default_variable.
  const bool must_be_present = HasFieldPresence(descriptor_->file());

  if (inlined_ && must_be_present) {
    // Calling mutable_$name$() gives us a string reference and sets the has bit
    // for $name$ (in proto2).  We may get here when the string field is inlined
    // but the string's contents have not been changed by the user, so we cannot
    // make an assertion about the contents of the string and could never make
    // an assertion about the string instance.
    //
    // For non-inlined strings, we distinguish from non-default by comparing
    // instances, rather than contents.
    format("$DCHK$(!$name$_.IsDefault($default_variable$));\n");
  }

  if (SupportsArenas(descriptor_)) {
    if (descriptor_->default_value_string().empty()) {
      if (must_be_present) {
        format("$name$_.ClearNonDefaultToEmpty();\n");
      } else {
        format(
            "$name$_.ClearToEmpty($default_variable$, GetArenaNoVirtual());\n");
      }
    } else {
      // Clear to a non-empty default is more involved, as we try to use the
      // Arena if one is present and may need to reallocate the string.
      format(
          "$name$_.ClearToDefault($default_variable$, GetArenaNoVirtual());\n");
    }
  } else if (must_be_present) {
    // When Arenas are disabled and field presence has been checked, we can
    // safely treat the ArenaStringPtr as a string*.
    if (descriptor_->default_value_string().empty()) {
      format("$name$_.ClearNonDefaultToEmptyNoArena();\n");
    } else {
      format("$name$_.UnsafeMutablePointer()->assign(*$default_variable$);\n");
    }
  } else {
    if (descriptor_->default_value_string().empty()) {
      format("$name$_.ClearToEmptyNoArena($default_variable$);\n");
    } else {
      format("$name$_.ClearToDefaultNoArena($default_variable$);\n");
    }
  }
}

void StringFieldGenerator::GenerateMergingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_) || descriptor_->containing_oneof() != NULL) {
    // TODO(gpike): improve this
    format("set_$name$(from.$name$());\n");
  } else {
    format(
        "$set_hasbit$\n"
        "$name$_.AssignWithDefault($default_variable$, from.$name$_);\n");
  }
}

void StringFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    format("$name$_.Swap(&other->$name$_);\n");
  } else {
    format(
        "$name$_.Swap(&other->$name$_, $default_variable$,\n"
        "  GetArenaNoVirtual());\n");
  }
}

void StringFieldGenerator::GenerateConstructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  // TODO(ckennelly): Construct non-empty strings as part of the initializer
  // list.
  if (inlined_ && descriptor_->default_value_string().empty()) {
    // Automatic initialization will construct the string.
    return;
  }

  format("$name$_.UnsafeSetDefault($default_variable$);\n");
}

void StringFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  GenerateConstructorCode(printer);

  if (HasFieldPresence(descriptor_->file())) {
    format("if (from.has_$name$()) {\n");
  } else {
    format("if (!from.$name$().empty()) {\n");
  }

  format.Indent();

  if (SupportsArenas(descriptor_) || descriptor_->containing_oneof() != NULL) {
    // TODO(gpike): improve this
    format(
        "$name$_.Set$lite$($default_variable$, from.$name$(),\n"
        "  GetArenaNoVirtual());\n");
  } else {
    format("$name$_.AssignWithDefault($default_variable$, from.$name$_);\n");
  }

  format.Outdent();
  format("}\n");
}

void StringFieldGenerator::GenerateDestructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (inlined_) {
    // The destructor is automatically invoked.
    return;
  }

  format("$name$_.DestroyNoArena($default_variable$);\n");
}

bool StringFieldGenerator::GenerateArenaDestructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!inlined_) {
    return false;
  }

  format("_this->$name$_.DestroyNoArena($default_variable$);\n");
  return true;
}

void StringFieldGenerator::GenerateDefaultInstanceAllocator(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!descriptor_->default_value_string().empty()) {
    format(
        "$ns$::$classname$::$default_variable_name$.DefaultConstruct();\n"
        "*$ns$::$classname$::$default_variable_name$.get_mutable() = "
        "std::string($default$, $default_length$);\n"
        "::$proto_ns$::internal::OnShutdownDestroyString(\n"
        "    $ns$::$classname$::$default_variable_name$.get_mutable());\n");
  }
}

void StringFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // The google3 version of proto2 has ArenaStrings and parses into them
  // directly, but for the open-source release, we always parse into std::string
  // instances. Note that for lite, we do similarly to the open source release
  // and use std::string, not ArenaString.
  if (!options_.opensource_runtime && !inlined_ &&
      SupportsArenas(descriptor_) && !lite_) {
    // If arena != NULL, the current string is either an ArenaString (no
    // destructor necessary) or a materialized std::string (and is on the
    // Arena's destructor list).  No call to ArenaStringPtr::Destroy is needed.
    format(
        "if (arena != nullptr) {\n"
        "  ::$proto_ns$::internal::TaggedPtr<std::string> str =\n"
        "    ::$proto_ns$::internal::ReadArenaString(input, arena);\n"
        "  DO_(!str.IsNull());\n"
        "  $set_hasbit_io$\n"
        "  $name$_.UnsafeSetTaggedPointer(str);\n"
        "} else {\n"
        "  DO_(::$proto_ns$::internal::WireFormatLite::Read$declared_type$(\n"
        "        input, this->mutable_$name$()));\n"
        "}\n");
  } else {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::Read$declared_type$(\n"
        "      input, this->mutable_$name$()));\n");
  }

  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, true,
        "this->$name$().data(), static_cast<int>(this->$name$().length()),\n",
        format);
  }
}

bool StringFieldGenerator::MergeFromCodedStreamNeedsArena() const {
  return !lite_ && !inlined_ && !options_.opensource_runtime;
}

void StringFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, false,
        "this->$name$().data(), static_cast<int>(this->$name$().length()),\n",
        format);
  }
  format(
      "::$proto_ns$::internal::WireFormatLite::Write$declared_type$"
      "MaybeAliased(\n"
      "  $number$, this->$name$(), output);\n");
}

void StringFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, false,
        "this->$name$().data(), static_cast<int>(this->$name$().length()),\n",
        format);
  }
  format(
      "target =\n"
      "  ::$proto_ns$::internal::WireFormatLite::Write$declared_type$ToArray(\n"
      "    $number$, this->$name$(), target);\n");
}

void StringFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ +\n"
      "  ::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
      "    this->$name$());\n");
}

uint32 StringFieldGenerator::CalculateFieldTag() const {
  return inlined_ ? 1 : 0;
}

// ===================================================================

StringOneofFieldGenerator::StringOneofFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : StringFieldGenerator(descriptor, options) {
  inlined_ = false;

  SetCommonOneofFieldVariables(descriptor, &variables_);
}

StringOneofFieldGenerator::~StringOneofFieldGenerator() {}

void StringOneofFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_)) {
    format(
        "inline const std::string& $classname$::$name$() const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  if (has_$name$()) {\n"
        "    return $field_member$.Get();\n"
        "  }\n"
        "  return *$default_variable$;\n"
        "}\n"
        "inline void $classname$::set_$name$(const std::string& value) {\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.Set$lite$($default_variable$, value,\n"
        "      GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(std::string&& value) {\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.Set$lite$(\n"
        "    $default_variable$, ::std::move(value), GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(const char* value) {\n"
        "  $null_check$"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.Set$lite$($default_variable$,\n"
        "      $string_piece$(value), GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
        "}\n");
    if (!options_.opensource_runtime) {
      format(
          "inline void $classname$::set_$name$(::StringPiece value) {\n"
          "  if (!has_$name$()) {\n"
          "    clear_$oneof_name$();\n"
          "    set_has_$name$();\n"
          "    $field_member$.UnsafeSetDefault($default_variable$);\n"
          "  }\n"
          "  $field_member$.Set$lite$($default_variable$, value,\n"
          "      GetArenaNoVirtual());\n"
          "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
          "}\n");
    }
    format(
        "inline "
        "void $classname$::set_$name$(const $pointer_type$* value,\n"
        "                             size_t size) {\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.Set$lite$(\n"
        "      $default_variable$, $string_piece$(\n"
        "      reinterpret_cast<const char*>(value), size),\n"
        "      GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
        "}\n"
        "inline std::string* $classname$::mutable_$name$() {\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  return $field_member$.Mutable($default_variable$,\n"
        "      GetArenaNoVirtual());\n"
        "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
        "}\n"
        "inline std::string* $classname$::$release_name$() {\n"
        "  // @@protoc_insertion_point(field_release:$full_name$)\n"
        "  if (has_$name$()) {\n"
        "    clear_has_$oneof_name$();\n"
        "    return $field_member$.Release($default_variable$,\n"
        "        GetArenaNoVirtual());\n"
        "  } else {\n"
        "    return nullptr;\n"
        "  }\n"
        "}\n"
        "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
        "  if (has_$oneof_name$()) {\n"
        "    clear_$oneof_name$();\n"
        "  }\n"
        "  if ($name$ != nullptr) {\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($name$);\n"
        "  }\n"
        "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
        "}\n");
    if (options_.opensource_runtime) {
      format(
          "inline std::string* $classname$::unsafe_arena_release_$name$() {\n"
          "  // "
          "@@protoc_insertion_point(field_unsafe_arena_release:$full_name$)\n"
          "  $DCHK$(GetArenaNoVirtual() != nullptr);\n"
          "  if (has_$name$()) {\n"
          "    clear_has_$oneof_name$();\n"
          "    return $field_member$.UnsafeArenaRelease(\n"
          "        $default_variable$, GetArenaNoVirtual());\n"
          "  } else {\n"
          "    return nullptr;\n"
          "  }\n"
          "}\n"
          "inline void $classname$::unsafe_arena_set_allocated_$name$("
          "std::string* $name$) {\n"
          "  $DCHK$(GetArenaNoVirtual() != nullptr);\n"
          "  if (!has_$name$()) {\n"
          "    $field_member$.UnsafeSetDefault($default_variable$);\n"
          "  }\n"
          "  clear_$oneof_name$();\n"
          "  if ($name$) {\n"
          "    set_has_$name$();\n"
          "    $field_member$.UnsafeArenaSetAllocated($default_variable$, "
          "$name$, GetArenaNoVirtual());\n"
          "  }\n"
          "  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:"
          "$full_name$)\n"
          "}\n");
    }
  } else {
    // No-arena case.
    format(
        "inline const std::string& $classname$::$name$() const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  if (has_$name$()) {\n"
        "    return $field_member$.GetNoArena();\n"
        "  }\n"
        "  return *$default_variable$;\n"
        "}\n"
        "inline void $classname$::set_$name$(const std::string& value) {\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.SetNoArena($default_variable$, value);\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(std::string&& value) {\n"
        "  // @@protoc_insertion_point(field_set:$full_name$)\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.SetNoArena($default_variable$, ::std::move(value));\n"
        "  // @@protoc_insertion_point(field_set_rvalue:$full_name$)\n"
        "}\n"
        "inline void $classname$::set_$name$(const char* value) {\n"
        "  $null_check$"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.SetNoArena($default_variable$,\n"
        "      $string_piece$(value));\n"
        "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
        "}\n");
    if (!options_.opensource_runtime) {
      format(
          "inline void $classname$::set_$name$(::StringPiece value) {\n"
          "  if (!has_$name$()) {\n"
          "    clear_$oneof_name$();\n"
          "    set_has_$name$();\n"
          "    $field_member$.UnsafeSetDefault($default_variable$);\n"
          "  }\n"
          "  $field_member$.SetNoArena($default_variable$, value);\n"
          "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
          "}\n");
    }
    format(
        "inline "
        "void $classname$::set_$name$(const $pointer_type$* value, size_t "
        "size) {\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  $field_member$.SetNoArena($default_variable$, $string_piece$(\n"
        "      reinterpret_cast<const char*>(value), size));\n"
        "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
        "}\n"
        "inline std::string* $classname$::mutable_$name$() {\n"
        "  if (!has_$name$()) {\n"
        "    clear_$oneof_name$();\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "  }\n"
        "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
        "  return $field_member$.MutableNoArena($default_variable$);\n"
        "}\n"
        "inline std::string* $classname$::$release_name$() {\n"
        "  // @@protoc_insertion_point(field_release:$full_name$)\n"
        "  if (has_$name$()) {\n"
        "    clear_has_$oneof_name$();\n"
        "    return $field_member$.ReleaseNoArena($default_variable$);\n"
        "  } else {\n"
        "    return nullptr;\n"
        "  }\n"
        "}\n"
        "inline void $classname$::set_allocated_$name$(std::string* $name$) {\n"
        "  if (has_$oneof_name$()) {\n"
        "    clear_$oneof_name$();\n"
        "  }\n"
        "  if ($name$ != nullptr) {\n"
        "    set_has_$name$();\n"
        "    $field_member$.UnsafeSetDefault($name$);\n"
        "  }\n"
        "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
        "}\n");
  }
}

void StringOneofFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_)) {
    format(
        "$field_member$.Destroy($default_variable$,\n"
        "    GetArenaNoVirtual());\n");
  } else {
    format("$field_member$.DestroyNoArena($default_variable$);\n");
  }
}

void StringOneofFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  return GenerateClearingCode(printer);
}

void StringOneofFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void StringOneofFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$ns$::_$classname$_default_instance_.$name$_.UnsafeSetDefault(\n"
      "    $default_variable$);\n");
}

void StringOneofFieldGenerator::GenerateDestructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "if (has_$name$()) {\n"
      "  $field_member$.DestroyNoArena($default_variable$);\n"
      "}\n");
}

void StringOneofFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // See above: ArenaString is not included in the open-source release.
  if (!options_.opensource_runtime && SupportsArenas(descriptor_) && !lite_) {
    // If has_$name$(), then the current string is either an ArenaString (no
    // destructor necessary) or a materialized std::string (and is on the
    // Arena's destructor list).  No call to ArenaStringPtr::Destroy is needed.
    format(
        "if (arena != nullptr) {\n"
        "  clear_$oneof_name$();\n"
        "  if (!has_$name$()) {\n"
        "    $field_member$.UnsafeSetDefault($default_variable$);\n"
        "    set_has_$name$();\n"
        "  }\n"
        "  ::$proto_ns$::internal::TaggedPtr<std::string> new_value =\n"
        "    ::$proto_ns$::internal::ReadArenaString(input, arena);\n"
        "  DO_(!new_value.IsNull());\n"
        "  $field_member$.UnsafeSetTaggedPointer(new_value);\n"
        "} else {\n"
        "  DO_(::$proto_ns$::internal::WireFormatLite::Read$declared_type$(\n"
        "        input, this->mutable_$name$()));\n"
        "}\n");
  } else {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::Read$declared_type$(\n"
        "      input, this->mutable_$name$()));\n");
  }

  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, true,
        "this->$name$().data(), static_cast<int>(this->$name$().length()),\n",
        format);
  }
}

// ===================================================================

RepeatedStringFieldGenerator::RepeatedStringFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options)
    : FieldGenerator(descriptor, options) {
  SetStringVariables(descriptor, &variables_, options);
}

RepeatedStringFieldGenerator::~RepeatedStringFieldGenerator() {}

void RepeatedStringFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("::$proto_ns$::RepeatedPtrField<std::string> $name$_;\n");
}

void RepeatedStringFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  // See comment above about unknown ctypes.
  bool unknown_ctype = descriptor_->options().ctype() !=
                       EffectiveStringCType(descriptor_, options_);

  if (unknown_ctype) {
    format.Outdent();
    format(
        " private:\n"
        "  // Hidden due to unknown ctype option.\n");
    format.Indent();
  }

  format(
      "$deprecated_attr$const std::string& ${1$$name$$}$(int index) const;\n"
      "$deprecated_attr$std::string* ${1$mutable_$name$$}$(int index);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, const "
      "std::string& value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, std::string&& "
      "value);\n"
      "$deprecated_attr$void ${1$set_$name$$}$(int index, const "
      "char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$set_$name$$}$(int index, "
        "StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$set_$name$$}$("
      "int index, const $pointer_type$* value, size_t size);\n"
      "$deprecated_attr$std::string* ${1$add_$name$$}$();\n"
      "$deprecated_attr$void ${1$add_$name$$}$(const std::string& value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$(std::string&& value);\n"
      "$deprecated_attr$void ${1$add_$name$$}$(const char* value);\n",
      descriptor_);
  if (!options_.opensource_runtime) {
    format(
        "$deprecated_attr$void ${1$add_$name$$}$(StringPiece value);\n",
        descriptor_);
  }
  format(
      "$deprecated_attr$void ${1$add_$name$$}$(const $pointer_type$* "
      "value, size_t size)"
      ";\n"
      "$deprecated_attr$const ::$proto_ns$::RepeatedPtrField<std::string>& "
      "${1$$name$$}$() "
      "const;\n"
      "$deprecated_attr$::$proto_ns$::RepeatedPtrField<std::string>* "
      "${1$mutable_$name$$}$()"
      ";\n",
      descriptor_);

  if (unknown_ctype) {
    format.Outdent();
    format(" public:\n");
    format.Indent();
  }
}

void RepeatedStringFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (options_.safe_boundary_check) {
    format(
        "inline const std::string& $classname$::$name$(int index) const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  return $name$_.InternalCheckedGet(\n"
        "      index, ::$proto_ns$::internal::GetEmptyStringAlreadyInited());\n"
        "}\n");
  } else {
    format(
        "inline const std::string& $classname$::$name$(int index) const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  return $name$_.Get(index);\n"
        "}\n");
  }
  format(
      "inline std::string* $classname$::mutable_$name$(int index) {\n"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return $name$_.Mutable(index);\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, const std::string& "
      "value) "
      "{\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "  $name$_.Mutable(index)->assign(value);\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, std::string&& value) {\n"
      "  // @@protoc_insertion_point(field_set:$full_name$)\n"
      "  $name$_.Mutable(index)->assign(std::move(value));\n"
      "}\n"
      "inline void $classname$::set_$name$(int index, const char* value) {\n"
      "  $null_check$"
      "  $name$_.Mutable(index)->assign(value);\n"
      "  // @@protoc_insertion_point(field_set_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void "
        "$classname$::set_$name$(int index, StringPiece value) {\n"
        "  $name$_.Mutable(index)->assign(value.data(), value.size());\n"
        "  // @@protoc_insertion_point(field_set_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline void "
      "$classname$::set_$name$"
      "(int index, const $pointer_type$* value, size_t size) {\n"
      "  $name$_.Mutable(index)->assign(\n"
      "    reinterpret_cast<const char*>(value), size);\n"
      "  // @@protoc_insertion_point(field_set_pointer:$full_name$)\n"
      "}\n"
      "inline std::string* $classname$::add_$name$() {\n"
      "  // @@protoc_insertion_point(field_add_mutable:$full_name$)\n"
      "  return $name$_.Add();\n"
      "}\n"
      "inline void $classname$::add_$name$(const std::string& value) {\n"
      "  $name$_.Add()->assign(value);\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$(std::string&& value) {\n"
      "  $name$_.Add(std::move(value));\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "}\n"
      "inline void $classname$::add_$name$(const char* value) {\n"
      "  $null_check$"
      "  $name$_.Add()->assign(value);\n"
      "  // @@protoc_insertion_point(field_add_char:$full_name$)\n"
      "}\n");
  if (!options_.opensource_runtime) {
    format(
        "inline void $classname$::add_$name$(StringPiece value) {\n"
        "  $name$_.Add()->assign(value.data(), value.size());\n"
        "  // @@protoc_insertion_point(field_add_string_piece:$full_name$)\n"
        "}\n");
  }
  format(
      "inline void "
      "$classname$::add_$name$(const $pointer_type$* value, size_t size) {\n"
      "  $name$_.Add()->assign(reinterpret_cast<const char*>(value), size);\n"
      "  // @@protoc_insertion_point(field_add_pointer:$full_name$)\n"
      "}\n"
      "inline const ::$proto_ns$::RepeatedPtrField<std::string>&\n"
      "$classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_list:$full_name$)\n"
      "  return $name$_;\n"
      "}\n"
      "inline ::$proto_ns$::RepeatedPtrField<std::string>*\n"
      "$classname$::mutable_$name$() {\n"
      "  // @@protoc_insertion_point(field_mutable_list:$full_name$)\n"
      "  return &$name$_;\n"
      "}\n");
}

void RepeatedStringFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.Clear();\n");
}

void RepeatedStringFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.MergeFrom(from.$name$_);\n");
}

void RepeatedStringFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.InternalSwap(CastToBase(&other->$name$_));\n");
}

void RepeatedStringFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedStringFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_.CopyFrom(from.$name$_);");
}

void RepeatedStringFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "DO_(::$proto_ns$::internal::WireFormatLite::Read$declared_type$(\n"
      "      input, this->add_$name$()));\n");
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, true,
        "this->$name$(this->$name$_size() - 1).data(),\n"
        "static_cast<int>(this->$name$(this->$name$_size() - 1).length()),\n",
        format);
  }
}

void RepeatedStringFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("for (int i = 0, n = this->$name$_size(); i < n; i++) {\n");
  format.Indent();
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, false,
        "this->$name$(i).data(), static_cast<int>(this->$name$(i).length()),\n",
        format);
  }
  format.Outdent();
  format(
      "  ::$proto_ns$::internal::WireFormatLite::Write$declared_type$(\n"
      "    $number$, this->$name$(i), output);\n"
      "}\n");
}

void RepeatedStringFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("for (int i = 0, n = this->$name$_size(); i < n; i++) {\n");
  format.Indent();
  if (descriptor_->type() == FieldDescriptor::TYPE_STRING) {
    GenerateUtf8CheckCodeForString(
        descriptor_, options_, false,
        "this->$name$(i).data(), static_cast<int>(this->$name$(i).length()),\n",
        format);
  }
  format.Outdent();
  format(
      "  target = ::$proto_ns$::internal::WireFormatLite::\n"
      "    Write$declared_type$ToArray($number$, this->$name$(i), target);\n"
      "}\n");
}

void RepeatedStringFieldGenerator::GenerateByteSize(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ *\n"
      "    ::$proto_ns$::internal::FromIntSize(this->$name$_size());\n"
      "for (int i = 0, n = this->$name$_size(); i < n; i++) {\n"
      "  total_size += "
      "::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
      "    this->$name$(i));\n"
      "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
