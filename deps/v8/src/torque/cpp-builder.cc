// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/cpp-builder.h"

namespace v8 {
namespace internal {
namespace torque {
namespace cpp {

void Function::PrintDeclarationHeader(std::ostream& stream,
                                      int indentation) const {
  if (!description_.empty()) {
    stream << std::string(indentation, ' ') << "// " << description_ << "\n";
  }
  stream << std::string(indentation, ' ') << "// " << pos_ << "\n";
  stream << std::string(indentation, ' ');
  if (IsExport()) stream << "V8_EXPORT_PRIVATE ";
  if (IsV8Inline())
    stream << "V8_INLINE ";
  else if (IsInline())
    stream << "inline ";
  if (IsStatic()) stream << "static ";
  if (IsConstexpr()) stream << "constexpr ";
  stream << return_type_ << " " << name_ << "(";
  bool first = true;
  for (const auto& p : parameters_) {
    if (!first) stream << ", ";
    stream << p.type;
    if (!p.name.empty()) stream << " " << p.name;
    if (!p.default_value.empty()) stream << " = " << p.default_value;
    first = false;
  }
  stream << ")";
  if (IsConst()) stream << " const";
}

void Function::PrintDeclaration(std::ostream& stream, int indentation) const {
  if (indentation == kAutomaticIndentation) {
    indentation = owning_class_ ? 2 : 0;
  }
  PrintDeclarationHeader(stream, indentation);
  stream << ";\n";
}

void Function::PrintDefinition(
    std::ostream& stream, const std::function<void(std::ostream&)>& builder,
    int indentation) const {
  PrintBeginDefinition(stream, indentation);
  if (builder) {
    builder(stream);
  }
  PrintEndDefinition(stream, indentation);
}

void Function::PrintInlineDefinition(
    std::ostream& stream, const std::function<void(std::ostream&)>& builder,
    int indentation) const {
  PrintDeclarationHeader(stream, indentation);
  stream << " {\n";
  if (builder) {
    builder(stream);
  }
  PrintEndDefinition(stream, indentation);
}

void Function::PrintBeginDefinition(std::ostream& stream,
                                    int indentation) const {
  stream << std::string(indentation, ' ') << "// " << pos_ << "\n";
  std::string scope;
  if (owning_class_) {
    scope = owning_class_->GetName();
    const auto class_template_parameters =
        owning_class_->GetTemplateParameters();
    if (!class_template_parameters.empty()) {
      stream << std::string(indentation, ' ');
      stream << "template<";
      scope += "<";
      bool first = true;
      for (const auto& p : class_template_parameters) {
        if (!first) {
          stream << ", ";
          scope += ", ";
        }
        if (p.type.empty()) {
          stream << "class " << p.name;
        } else {
          stream << p.type << " " << p.name;
        }
        scope += p.name;
        first = false;
      }
      stream << ">\n";
      scope += ">";
    }
    scope += "::";
  }
  stream << std::string(indentation, ' ') << return_type_ << " " << scope
         << name_ << "(";
  bool first = true;
  for (const auto& p : parameters_) {
    if (!first) stream << ", ";
    stream << p.type;
    if (!p.name.empty()) stream << " " << p.name;
    first = false;
  }
  stream << ")";
  if (IsConst()) {
    stream << " const";
  }
  stream << " {\n";
}

void Function::PrintEndDefinition(std::ostream& stream, int indentation) const {
  stream << std::string(indentation, ' ');
  stream << "}\n\n";
}

void File::BeginIncludeGuard(const std::string& name) {
  s() << "#ifndef " << name
      << "\n"
         "#define "
      << name << "\n\n";
}

void File::EndIncludeGuard(const std::string& name) {
  s() << "#endif // " << name << "\n";
}

void File::BeginNamespace(std::string name) {
  DCHECK(!name.empty());
  DCHECK_EQ(name.find(':'), std::string::npos);
  s() << "namespace " << name << " {\n";
  namespaces_.push(std::move(name));
}

void File::BeginNamespace(std::string name0, std::string name1) {
  BeginNamespace(name0);
  BeginNamespace(name1);
}

void File::EndNamespace(const std::string& name) {
  DCHECK(!namespaces_.empty());
  DCHECK_EQ(namespaces_.top(), name);
  s() << "} // namespace " << namespaces_.top() << "\n";
  namespaces_.pop();
}

void File::EndNamespace(const std::string& name0, const std::string& name1) {
  EndNamespace(name1);
  EndNamespace(name0);
}

}  // namespace cpp
}  // namespace torque
}  // namespace internal
}  // namespace v8
