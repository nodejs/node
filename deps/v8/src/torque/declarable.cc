// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"
#include "src/torque/global-context.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(CurrentScope)

std::ostream& operator<<(std::ostream& os, const QualifiedName& name) {
  for (const std::string& qualifier : name.namespace_qualification) {
    os << qualifier << "::";
  }
  return os << name.name;
}

std::ostream& operator<<(std::ostream& os, const Callable& m) {
  os << "callable " << m.ReadableName() << "(";
  if (m.signature().implicit_count != 0) {
    os << "implicit ";
    TypeVector implicit_parameter_types(
        m.signature().parameter_types.types.begin(),
        m.signature().parameter_types.types.begin() +
            m.signature().implicit_count);
    os << implicit_parameter_types << ")(";
    TypeVector explicit_parameter_types(
        m.signature().parameter_types.types.begin() +
            m.signature().implicit_count,
        m.signature().parameter_types.types.end());
    os << explicit_parameter_types;
  } else {
    os << m.signature().parameter_types;
  }
  os << "): " << *m.signature().return_type;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Builtin& b) {
  os << "builtin " << *b.signature().return_type << " " << b.ReadableName()
     << b.signature().parameter_types;
  return os;
}

std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b) {
  os << "runtime function " << *b.signature().return_type << " "
     << b.ReadableName() << b.signature().parameter_types;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Generic& g) {
  os << "generic " << g.name() << "<";
  PrintCommaSeparatedList(
      os, g.declaration()->generic_parameters,
      [](const Identifier* identifier) { return identifier->value; });
  os << ">";

  return os;
}

base::Optional<const Type*> Generic::InferTypeArgument(
    size_t i, const TypeVector& arguments) {
  const std::string type_name = declaration()->generic_parameters[i]->value;
  const std::vector<TypeExpression*>& parameters =
      declaration()->callable->signature->parameters.types;
  size_t j = declaration()->callable->signature->parameters.implicit_count;
  for (size_t i = 0; i < arguments.size() && j < parameters.size(); ++i, ++j) {
    BasicTypeExpression* basic =
        BasicTypeExpression::DynamicCast(parameters[j]);
    if (basic && basic->namespace_qualification.empty() &&
        !basic->is_constexpr && basic->name == type_name) {
      return arguments[i];
    }
  }
  return base::nullopt;
}

base::Optional<TypeVector> Generic::InferSpecializationTypes(
    const TypeVector& explicit_specialization_types,
    const TypeVector& arguments) {
  TypeVector result = explicit_specialization_types;
  size_t type_parameter_count = declaration()->generic_parameters.size();
  if (explicit_specialization_types.size() > type_parameter_count) {
    return base::nullopt;
  }
  for (size_t i = explicit_specialization_types.size();
       i < type_parameter_count; ++i) {
    base::Optional<const Type*> inferred = InferTypeArgument(i, arguments);
    if (!inferred) return base::nullopt;
    result.push_back(*inferred);
  }
  return result;
}

bool Namespace::IsDefaultNamespace() const {
  return this == GlobalContext::GetDefaultNamespace();
}

bool Namespace::IsTestNamespace() const { return name() == kTestNamespaceName; }

}  // namespace torque
}  // namespace internal
}  // namespace v8
