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

#include <google/protobuf/compiler/cpp/cpp_message_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>

#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {
string ReinterpretCast(const string& type, const string& expression,
                       bool implicit_weak_field) {
  if (implicit_weak_field) {
    return "reinterpret_cast< " + type + " >(" + expression + ")";
  } else {
    return expression;
  }
}

void SetMessageVariables(const FieldDescriptor* descriptor,
                         const Options& options, bool implicit_weak,
                         std::map<std::string, std::string>* variables) {
  SetCommonFieldVariables(descriptor, variables, options);
  (*variables)["type"] = FieldMessageTypeName(descriptor, options);
  (*variables)["casted_member"] = ReinterpretCast(
      (*variables)["type"] + "*", (*variables)["name"] + "_", implicit_weak);
  (*variables)["type_default_instance"] =
      QualifiedDefaultInstanceName(descriptor->message_type(), options);
  (*variables)["type_reference_function"] =
      implicit_weak
          ? ("  " + ReferenceFunctionName(descriptor->message_type(), options) +
             "();\n")
          : "";
  (*variables)["stream_writer"] =
      (*variables)["declared_type"] +
      (HasFastArraySerialization(descriptor->message_type()->file(), options)
           ? "MaybeToArray"
           : "");
  // NOTE: Escaped here to unblock proto1->proto2 migration.
  // TODO(liujisi): Extend this to apply for other conflicting methods.
  (*variables)["release_name"] =
      SafeFunctionName(descriptor->containing_type(), descriptor, "release_");
  (*variables)["full_name"] = descriptor->full_name();
}

}  // namespace

// ===================================================================

MessageFieldGenerator::MessageFieldGenerator(const FieldDescriptor* descriptor,
                                             const Options& options,
                                             MessageSCCAnalyzer* scc_analyzer)
    : FieldGenerator(descriptor, options),
      implicit_weak_field_(
          IsImplicitWeakField(descriptor, options, scc_analyzer)) {
  SetMessageVariables(descriptor, options, implicit_weak_field_, &variables_);
}

MessageFieldGenerator::~MessageFieldGenerator() {}

void MessageFieldGenerator::GeneratePrivateMembers(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format("::$proto_ns$::MessageLite* $name$_;\n");
  } else {
    format("$type$* $name$_;\n");
  }
}

void MessageFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$const $type$& ${1$$name$$}$() const;\n"
      "$deprecated_attr$$type$* ${1$$release_name$$}$();\n"
      "$deprecated_attr$$type$* ${1$mutable_$name$$}$();\n"
      "$deprecated_attr$void ${1$set_allocated_$name$$}$"
      "($type$* $name$);\n",
      descriptor_);
  if (SupportsArenas(descriptor_)) {
    format(
        "$deprecated_attr$void "
        "${1$unsafe_arena_set_allocated_$name$$}$(\n"
        "    $type$* $name$);\n"
        "$deprecated_attr$$type$* ${1$unsafe_arena_release_$name$$}$();\n",
        descriptor_);
  }
}

void MessageFieldGenerator::GenerateNonInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_)) {
    format(
        "void $classname$::unsafe_arena_set_allocated_$name$(\n"
        "    $type$* $name$) {\n"
        // If we're not on an arena, free whatever we were holding before.
        // (If we are on arena, we can just forget the earlier pointer.)
        "  if (GetArenaNoVirtual() == nullptr) {\n"
        "    delete $name$_;\n"
        "  }\n"
        "  $name$_ = $name$;\n"
        "  if ($name$) {\n"
        "    $set_hasbit$\n"
        "  } else {\n"
        "    $clear_hasbit$\n"
        "  }\n"
        "  // @@protoc_insertion_point(field_unsafe_arena_set_allocated"
        ":$full_name$)\n"
        "}\n");
  }
}

void MessageFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline const $type$& $classname$::$name$() const {\n"
      "  const $type$* p = $casted_member$;\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return p != nullptr ? *p : *reinterpret_cast<const $type$*>(\n"
      "      &$type_default_instance$);\n"
      "}\n");

  format(
      "inline $type$* $classname$::$release_name$() {\n"
      "  // @@protoc_insertion_point(field_release:$full_name$)\n"
      "$type_reference_function$"
      "  $clear_hasbit$\n"
      "  $type$* temp = $casted_member$;\n");
  if (SupportsArenas(descriptor_)) {
    format(
        "  if (GetArenaNoVirtual() != nullptr) {\n"
        "    temp = ::$proto_ns$::internal::DuplicateIfNonNull(temp);\n"
        "  }\n");
  }
  format(
      "  $name$_ = nullptr;\n"
      "  return temp;\n"
      "}\n");

  if (SupportsArenas(descriptor_)) {
    format(
        "inline $type$* $classname$::unsafe_arena_release_$name$() {\n"
        "  // "
        "@@protoc_insertion_point(field_unsafe_arena_release:$full_name$)\n"
        "$type_reference_function$"
        "  $clear_hasbit$\n"
        "  $type$* temp = $casted_member$;\n"
        "  $name$_ = nullptr;\n"
        "  return temp;\n"
        "}\n");
  }

  format(
      "inline $type$* $classname$::mutable_$name$() {\n"
      "  $set_hasbit$\n"
      "  if ($name$_ == nullptr) {\n"
      "    auto* p = CreateMaybeMessage<$type$>(GetArenaNoVirtual());\n");
  if (implicit_weak_field_) {
    format("    $name$_ = reinterpret_cast<::$proto_ns$::MessageLite*>(p);\n");
  } else {
    format("    $name$_ = p;\n");
  }
  format(
      "  }\n"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return $casted_member$;\n"
      "}\n");

  // We handle the most common case inline, and delegate less common cases to
  // the slow fallback function.
  format(
      "inline void $classname$::set_allocated_$name$($type$* $name$) {\n"
      "  ::$proto_ns$::Arena* message_arena = GetArenaNoVirtual();\n");
  format("  if (message_arena == nullptr) {\n");
  if (IsCrossFileMessage(descriptor_)) {
    format(
        "    delete reinterpret_cast< ::$proto_ns$::MessageLite*>($name$_);\n");
  } else {
    format("    delete $name$_;\n");
  }
  format(
      "  }\n"
      "  if ($name$) {\n");
  if (SupportsArenas(descriptor_->message_type()) &&
      IsCrossFileMessage(descriptor_)) {
    // We have to read the arena through the virtual method, because the type
    // isn't defined in this file.
    format(
        "    ::$proto_ns$::Arena* submessage_arena =\n"
        "      "
        "reinterpret_cast<::$proto_ns$::MessageLite*>($name$)->GetArena();\n");
  } else if (!SupportsArenas(descriptor_->message_type())) {
    format("    ::$proto_ns$::Arena* submessage_arena = nullptr;\n");
  } else {
    format(
        "    ::$proto_ns$::Arena* submessage_arena =\n"
        "      ::$proto_ns$::Arena::GetArena($name$);\n");
  }
  format(
      "    if (message_arena != submessage_arena) {\n"
      "      $name$ = ::$proto_ns$::internal::GetOwnedMessage(\n"
      "          message_arena, $name$, submessage_arena);\n"
      "    }\n"
      "    $set_hasbit$\n"
      "  } else {\n"
      "    $clear_hasbit$\n"
      "  }\n");
  if (implicit_weak_field_) {
    format("  $name$_ = reinterpret_cast<MessageLite*>($name$);\n");
  } else {
    format("  $name$_ = $name$;\n");
  }
  format(
      "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
      "}\n");
}

void MessageFieldGenerator::GenerateInternalAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format(
        "static const ::$proto_ns$::MessageLite& $name$("
        "const $classname$* msg);\n"
        "static ::$proto_ns$::MessageLite* mutable_$name$("
        "$classname$* msg);\n");
  } else {
    format("static const $type$& $name$(const $classname$* msg);\n");
  }
}

void MessageFieldGenerator::GenerateInternalAccessorDefinitions(
    io::Printer* printer) const {
  // In theory, these accessors could be inline in _Internal. However, in
  // practice, the linker is then not able to throw them out making implicit
  // weak dependencies not work at all.
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    // These private accessors are used by MergeFrom and
    // MergePartialFromCodedStream, and their purpose is to provide access to
    // the field without creating a strong dependency on the message type.
    format(
        "const ::$proto_ns$::MessageLite& $classname$::_Internal::$name$(\n"
        "    const $classname$* msg) {\n"
        "  if (msg->$name$_ != nullptr) {\n"
        "    return *msg->$name$_;\n"
        "  } else if (&$type_default_instance$ != nullptr) {\n"
        "    return *reinterpret_cast<const ::$proto_ns$::MessageLite*>(\n"
        "        &$type_default_instance$);\n"
        "  } else {\n"
        "    return "
        "*::$proto_ns$::internal::ImplicitWeakMessage::default_instance();\n"
        "  }\n"
        "}\n");
    if (SupportsArenas(descriptor_)) {
      format(
          "::$proto_ns$::MessageLite*\n"
          "$classname$::_Internal::mutable_$name$($classname$* msg) {\n");
      if (HasFieldPresence(descriptor_->file())) {
        format("  msg->$set_hasbit$\n");
      }
      format(
          "  if (msg->$name$_ == nullptr) {\n"
          "    if (&$type_default_instance$ == nullptr) {\n"
          "      msg->$name$_ = ::$proto_ns$::Arena::CreateMessage<\n"
          "          ::$proto_ns$::internal::ImplicitWeakMessage>(\n"
          "              msg->GetArenaNoVirtual());\n"
          "    } else {\n"
          "      msg->$name$_ = reinterpret_cast<const "
          "::$proto_ns$::MessageLite*>(\n"
          "          &$type_default_instance$)->New("
          "msg->GetArenaNoVirtual());\n"
          "    }\n"
          "  }\n"
          "  return msg->$name$_;\n"
          "}\n");
    } else {
      format(
          "::$proto_ns$::MessageLite*\n"
          "$classname$::_Internal::mutable_$name$($classname$* msg) {\n");
      if (HasFieldPresence(descriptor_->file())) {
        format("  msg->$set_hasbit$\n");
      }
      format(
          "  if (msg->$name$_ == nullptr) {\n"
          "    if (&$type_default_instance$ == nullptr) {\n"
          "      msg->$name$_ = "
          "new ::$proto_ns$::internal::ImplicitWeakMessage;\n"
          "    } else {\n"
          "      msg->$name$_ = "
          "reinterpret_cast<const ::$proto_ns$::MessageLite*>(\n"
          "          &$type_default_instance$)->New();\n"
          "    }\n"
          "  }\n"
          "  return msg->$name$_;\n"
          "}\n");
    }
  } else {
    // This inline accessor directly returns member field and is used in
    // Serialize such that AFDO profile correctly captures access information to
    // message fields under serialize.
    format(
        "const $type$&\n"
        "$classname$::_Internal::$name$(const $classname$* msg) {\n"
        "  return *msg->$field_member$;\n"
        "}\n");
  }
}

void MessageFieldGenerator::GenerateClearingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!HasFieldPresence(descriptor_->file())) {
    // If we don't have has-bits, message presence is indicated only by ptr !=
    // NULL. Thus on clear, we need to delete the object.
    format(
        "if (GetArenaNoVirtual() == nullptr && $name$_ != nullptr) {\n"
        "  delete $name$_;\n"
        "}\n"
        "$name$_ = nullptr;\n");
  } else {
    format("if ($name$_ != nullptr) $name$_->Clear();\n");
  }
}

void MessageFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (!HasFieldPresence(descriptor_->file())) {
    // If we don't have has-bits, message presence is indicated only by ptr !=
    // NULL. Thus on clear, we need to delete the object.
    format(
        "if (GetArenaNoVirtual() == nullptr && $name$_ != nullptr) {\n"
        "  delete $name$_;\n"
        "}\n"
        "$name$_ = nullptr;\n");
  } else {
    format(
        "$DCHK$($name$_ != nullptr);\n"
        "$name$_->Clear();\n");
  }
}

void MessageFieldGenerator::GenerateMergingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format(
        "_Internal::mutable_$name$(this)->CheckTypeAndMergeFrom(\n"
        "    _Internal::$name$(&from));\n");
  } else {
    format("mutable_$name$()->$type$::MergeFrom(from.$name$());\n");
  }
}

void MessageFieldGenerator::GenerateSwappingCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("swap($name$_, other->$name$_);\n");
}

void MessageFieldGenerator::GenerateDestructorCode(io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (options_.opensource_runtime) {
    // TODO(gerbens) Remove this when we don't need to destruct default
    // instances.  In google3 a default instance will never get deleted so we
    // don't need to worry about that but in opensource protobuf default
    // instances are deleted in shutdown process and we need to take special
    // care when handling them.
    format("if (this != internal_default_instance()) ");
  }
  format("delete $name$_;\n");
}

void MessageFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("$name$_ = nullptr;\n");
}

void MessageFieldGenerator::GenerateCopyConstructorCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "if (from.has_$name$()) {\n"
      "  $name$_ = new $type$(*from.$name$_);\n"
      "} else {\n"
      "  $name$_ = nullptr;\n"
      "}\n");
}

void MessageFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::ReadMessage(\n"
        "     input, _Internal::mutable_$name$(this)));\n");
  } else if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE) {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::ReadMessage(\n"
        "     input, mutable_$name$()));\n");
  } else {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::ReadGroup(\n"
        "      $number$, input, mutable_$name$()));\n");
  }
}

void MessageFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "::$proto_ns$::internal::WireFormatLite::Write$stream_writer$(\n"
      "  $number$, _Internal::$name$(this), output);\n");
}

void MessageFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "target = ::$proto_ns$::internal::WireFormatLite::\n"
      "  InternalWrite$declared_type$ToArray(\n"
      "    $number$, _Internal::$name$(this), target);\n");
}

void MessageFieldGenerator::GenerateByteSize(io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "total_size += $tag_size$ +\n"
      "  ::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n"
      "    *$field_member$);\n");
}

// ===================================================================

MessageOneofFieldGenerator::MessageOneofFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options,
    MessageSCCAnalyzer* scc_analyzer)
    : MessageFieldGenerator(descriptor, options, scc_analyzer) {
  SetCommonOneofFieldVariables(descriptor, &variables_);
}

MessageOneofFieldGenerator::~MessageOneofFieldGenerator() {}

void MessageOneofFieldGenerator::GenerateNonInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "void $classname$::set_allocated_$name$($type$* $name$) {\n"
      "  ::$proto_ns$::Arena* message_arena = GetArenaNoVirtual();\n"
      "  clear_$oneof_name$();\n"
      "  if ($name$) {\n");
  if (SupportsArenas(descriptor_->message_type()) &&
      descriptor_->file() != descriptor_->message_type()->file()) {
    // We have to read the arena through the virtual method, because the type
    // isn't defined in this file.
    format(
        "    ::$proto_ns$::Arena* submessage_arena =\n"
        "      "
        "reinterpret_cast<::$proto_ns$::MessageLite*>($name$)->GetArena();\n");
  } else if (!SupportsArenas(descriptor_->message_type())) {
    format("    ::$proto_ns$::Arena* submessage_arena = nullptr;\n");
  } else {
    format(
        "    ::$proto_ns$::Arena* submessage_arena =\n"
        "      ::$proto_ns$::Arena::GetArena($name$);\n");
  }
  format(
      "    if (message_arena != submessage_arena) {\n"
      "      $name$ = ::$proto_ns$::internal::GetOwnedMessage(\n"
      "          message_arena, $name$, submessage_arena);\n"
      "    }\n"
      "    set_has_$name$();\n"
      "    $field_member$ = $name$;\n"
      "  }\n"
      "  // @@protoc_insertion_point(field_set_allocated:$full_name$)\n"
      "}\n");
}

void MessageOneofFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$* $classname$::$release_name$() {\n"
      "  // @@protoc_insertion_point(field_release:$full_name$)\n"
      "  if (has_$name$()) {\n"
      "    clear_has_$oneof_name$();\n"
      "      $type$* temp = $field_member$;\n");
  if (SupportsArenas(descriptor_)) {
    format(
        "    if (GetArenaNoVirtual() != nullptr) {\n"
        "      temp = ::$proto_ns$::internal::DuplicateIfNonNull(temp);\n"
        "    }\n");
  }
  format(
      "    $field_member$ = nullptr;\n"
      "    return temp;\n"
      "  } else {\n"
      "    return nullptr;\n"
      "  }\n"
      "}\n");

  format(
      "inline const $type$& $classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_get:$full_name$)\n"
      "  return has_$name$()\n"
      "      ? *$field_member$\n"
      "      : *reinterpret_cast< $type$*>(&$type_default_instance$);\n"
      "}\n");

  if (SupportsArenas(descriptor_)) {
    format(
        "inline $type$* $classname$::unsafe_arena_release_$name$() {\n"
        "  // @@protoc_insertion_point(field_unsafe_arena_release"
        ":$full_name$)\n"
        "  if (has_$name$()) {\n"
        "    clear_has_$oneof_name$();\n"
        "    $type$* temp = $field_member$;\n"
        "    $field_member$ = nullptr;\n"
        "    return temp;\n"
        "  } else {\n"
        "    return nullptr;\n"
        "  }\n"
        "}\n"
        "inline void $classname$::unsafe_arena_set_allocated_$name$"
        "($type$* $name$) {\n"
        // We rely on the oneof clear method to free the earlier contents of
        // this oneof. We can directly use the pointer we're given to set the
        // new value.
        "  clear_$oneof_name$();\n"
        "  if ($name$) {\n"
        "    set_has_$name$();\n"
        "    $field_member$ = $name$;\n"
        "  }\n"
        "  // @@protoc_insertion_point(field_unsafe_arena_set_allocated:"
        "$full_name$)\n"
        "}\n");
  }

  format(
      "inline $type$* $classname$::mutable_$name$() {\n"
      "  if (!has_$name$()) {\n"
      "    clear_$oneof_name$();\n"
      "    set_has_$name$();\n"
      "    $field_member$ = CreateMaybeMessage< $type$ >(\n"
      "        GetArenaNoVirtual());\n"
      "  }\n"
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "  return $field_member$;\n"
      "}\n");
}

void MessageOneofFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (SupportsArenas(descriptor_)) {
    format(
        "if (GetArenaNoVirtual() == nullptr) {\n"
        "  delete $field_member$;\n"
        "}\n");
  } else {
    format("delete $field_member$;\n");
  }
}

void MessageOneofFieldGenerator::GenerateMessageClearingCode(
    io::Printer* printer) const {
  GenerateClearingCode(printer);
}

void MessageOneofFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  // Don't print any swapping code. Swapping the union will swap this field.
}

void MessageOneofFieldGenerator::GenerateDestructorCode(
    io::Printer* printer) const {
  // We inherit from MessageFieldGenerator, so we need to override the default
  // behavior.
}

void MessageOneofFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Don't print any constructor code. The field is in a union. We allocate
  // space only when this field is used.
}

// ===================================================================

RepeatedMessageFieldGenerator::RepeatedMessageFieldGenerator(
    const FieldDescriptor* descriptor, const Options& options,
    MessageSCCAnalyzer* scc_analyzer)
    : FieldGenerator(descriptor, options),
      implicit_weak_field_(
          IsImplicitWeakField(descriptor, options, scc_analyzer)) {
  SetMessageVariables(descriptor, options, implicit_weak_field_, &variables_);
}

RepeatedMessageFieldGenerator::~RepeatedMessageFieldGenerator() {}

void RepeatedMessageFieldGenerator::GeneratePrivateMembers(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("::$proto_ns$::RepeatedPtrField< $type$ > $name$_;\n");
}

void RepeatedMessageFieldGenerator::GenerateAccessorDeclarations(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "$deprecated_attr$$type$* ${1$mutable_$name$$}$(int index);\n"
      "$deprecated_attr$::$proto_ns$::RepeatedPtrField< $type$ >*\n"
      "    ${1$mutable_$name$$}$();\n"
      "$deprecated_attr$const $type$& ${1$$name$$}$(int index) const;\n"
      "$deprecated_attr$$type$* ${1$add_$name$$}$();\n"
      "$deprecated_attr$const ::$proto_ns$::RepeatedPtrField< $type$ >&\n"
      "    ${1$$name$$}$() const;\n",
      descriptor_);
}

void RepeatedMessageFieldGenerator::GenerateInlineAccessorDefinitions(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "inline $type$* $classname$::mutable_$name$(int index) {\n"
      // TODO(dlj): move insertion points
      "  // @@protoc_insertion_point(field_mutable:$full_name$)\n"
      "$type_reference_function$"
      "  return $name$_.Mutable(index);\n"
      "}\n"
      "inline ::$proto_ns$::RepeatedPtrField< $type$ >*\n"
      "$classname$::mutable_$name$() {\n"
      "  // @@protoc_insertion_point(field_mutable_list:$full_name$)\n"
      "$type_reference_function$"
      "  return &$name$_;\n"
      "}\n");

  if (options_.safe_boundary_check) {
    format(
        "inline const $type$& $classname$::$name$(int index) const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "  return $name$_.InternalCheckedGet(index,\n"
        "      *reinterpret_cast<const $type$*>(&$type_default_instance$));\n"
        "}\n");
  } else {
    format(
        "inline const $type$& $classname$::$name$(int index) const {\n"
        "  // @@protoc_insertion_point(field_get:$full_name$)\n"
        "$type_reference_function$"
        "  return $name$_.Get(index);\n"
        "}\n");
  }

  format(
      "inline $type$* $classname$::add_$name$() {\n"
      "  // @@protoc_insertion_point(field_add:$full_name$)\n"
      "  return $name$_.Add();\n"
      "}\n");

  format(
      "inline const ::$proto_ns$::RepeatedPtrField< $type$ >&\n"
      "$classname$::$name$() const {\n"
      "  // @@protoc_insertion_point(field_list:$full_name$)\n"
      "$type_reference_function$"
      "  return $name$_;\n"
      "}\n");
}

void RepeatedMessageFieldGenerator::GenerateClearingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format(
        "CastToBase(&$name$_)->Clear<"
        "::$proto_ns$::internal::ImplicitWeakTypeHandler<$type$>>();\n");
  } else {
    format("$name$_.Clear();\n");
  }
}

void RepeatedMessageFieldGenerator::GenerateMergingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (implicit_weak_field_) {
    format(
        "CastToBase(&$name$_)->MergeFrom<"
        "::$proto_ns$::internal::ImplicitWeakTypeHandler<$type$>>(CastToBase("
        "from.$name$_));\n");
  } else {
    format("$name$_.MergeFrom(from.$name$_);\n");
  }
}

void RepeatedMessageFieldGenerator::GenerateSwappingCode(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format("CastToBase(&$name$_)->InternalSwap(CastToBase(&other->$name$_));\n");
}

void RepeatedMessageFieldGenerator::GenerateConstructorCode(
    io::Printer* printer) const {
  // Not needed for repeated fields.
}

void RepeatedMessageFieldGenerator::GenerateMergeFromCodedStream(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  if (descriptor_->type() == FieldDescriptor::TYPE_MESSAGE) {
    if (implicit_weak_field_) {
      format(
          "DO_(::$proto_ns$::internal::WireFormatLite::"
          "ReadMessage(input, CastToBase(&$name$_)->AddWeak(\n"
          "    reinterpret_cast<const ::$proto_ns$::MessageLite*>(\n"
          "        &$type_default_instance$))));\n");
    } else {
      format(
          "DO_(::$proto_ns$::internal::WireFormatLite::"
          "ReadMessage(\n"
          "      input, add_$name$()));\n");
    }
  } else {
    format(
        "DO_(::$proto_ns$::internal::WireFormatLite::"
        "ReadGroup($number$, input, add_$name$()));\n");
  }
}

void RepeatedMessageFieldGenerator::GenerateSerializeWithCachedSizes(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "for (unsigned int i = 0,\n"
      "    n = static_cast<unsigned int>(this->$name$_size()); i < n; i++) {\n"
      "  ::$proto_ns$::internal::WireFormatLite::Write$stream_writer$(\n"
      "    $number$,\n");
  if (implicit_weak_field_) {
    format(
        "    CastToBase($name$_).Get<"
        "::$proto_ns$::internal::ImplicitWeakTypeHandler<$type$>>("
        "static_cast<int>(i)),\n");
  } else {
    format("    this->$name$(static_cast<int>(i)),\n");
  }
  format(
      "    output);\n"
      "}\n");
}

void RepeatedMessageFieldGenerator::GenerateSerializeWithCachedSizesToArray(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "for (unsigned int i = 0,\n"
      "    n = static_cast<unsigned int>(this->$name$_size()); i < n; i++) {\n"
      "  target = ::$proto_ns$::internal::WireFormatLite::\n"
      "    InternalWrite$declared_type$ToArray(\n"
      "      $number$, this->$name$(static_cast<int>(i)), target);\n"
      "}\n");
}

void RepeatedMessageFieldGenerator::GenerateByteSize(
    io::Printer* printer) const {
  Formatter format(printer, variables_);
  format(
      "{\n"
      "  unsigned int count = static_cast<unsigned "
      "int>(this->$name$_size());\n");
  format.Indent();
  format(
      "total_size += $tag_size$UL * count;\n"
      "for (unsigned int i = 0; i < count; i++) {\n"
      "  total_size +=\n"
      "    ::$proto_ns$::internal::WireFormatLite::$declared_type$Size(\n");
  if (implicit_weak_field_) {
    format(
        "      CastToBase($name$_).Get<"
        "::$proto_ns$::internal::ImplicitWeakTypeHandler<$type$>>("
        "static_cast<int>(i)));\n");
  } else {
    format("      this->$name$(static_cast<int>(i)));\n");
  }
  format("}\n");
  format.Outdent();
  format("}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
