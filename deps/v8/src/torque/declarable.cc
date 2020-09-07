// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "src/torque/declarable.h"
#include "src/torque/global-context.h"
#include "src/torque/type-inference.h"
#include "src/torque/type-visitor.h"

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

std::ostream& operator<<(std::ostream& os, const GenericCallable& g) {
  os << "generic " << g.name() << "<";
  PrintCommaSeparatedList(os, g.generic_parameters(),
                          [](const GenericParameter& identifier) {
                            return identifier.name->value;
                          });
  os << ">";

  return os;
}

SpecializationRequester::SpecializationRequester(SourcePosition position,
                                                 Scope* scope, std::string name)
    : position(position), name(std::move(name)) {
  // Skip scopes that are not related to template specializations, they might be
  // stack-allocated and not live for long enough.
  while (scope && scope->GetSpecializationRequester().IsNone())
    scope = scope->ParentScope();
  this->scope = scope;
}

std::vector<Declarable*> Scope::Lookup(const QualifiedName& name) {
  if (name.namespace_qualification.size() >= 1 &&
      name.namespace_qualification[0] == "") {
    return GlobalContext::GetDefaultNamespace()->Lookup(
        name.DropFirstNamespaceQualification());
  }
  std::vector<Declarable*> result;
  if (ParentScope()) {
    result = ParentScope()->Lookup(name);
  }
  for (Declarable* declarable : LookupShallow(name)) {
    result.push_back(declarable);
  }
  return result;
}

base::Optional<std::string> TypeConstraint::IsViolated(const Type* type) const {
  if (upper_bound && !type->IsSubtypeOf(*upper_bound)) {
    return {
        ToString("expected ", *type, " to be a subtype of ", **upper_bound)};
  }
  return base::nullopt;
}

base::Optional<std::string> FindConstraintViolation(
    const std::vector<const Type*>& types,
    const std::vector<TypeConstraint>& constraints) {
  DCHECK_EQ(constraints.size(), types.size());
  for (size_t i = 0; i < types.size(); ++i) {
    if (auto violation = constraints[i].IsViolated(types[i])) {
      return {"Could not instantiate generic, " + *violation + "."};
    }
  }
  return base::nullopt;
}

std::vector<TypeConstraint> ComputeConstraints(
    Scope* scope, const GenericParameters& parameters) {
  CurrentScope::Scope scope_scope(scope);
  std::vector<TypeConstraint> result;
  for (const GenericParameter& parameter : parameters) {
    if (parameter.constraint) {
      result.push_back(TypeConstraint::SubtypeConstraint(
          TypeVisitor::ComputeType(*parameter.constraint)));
    } else {
      result.push_back(TypeConstraint::Unconstrained());
    }
  }
  return result;
}

TypeArgumentInference GenericCallable::InferSpecializationTypes(
    const TypeVector& explicit_specialization_types,
    const TypeVector& arguments) {
  size_t implicit_count = declaration()->parameters.implicit_count;
  const std::vector<TypeExpression*>& parameters =
      declaration()->parameters.types;
  std::vector<TypeExpression*> explicit_parameters(
      parameters.begin() + implicit_count, parameters.end());

  CurrentScope::Scope generic_scope(ParentScope());
  TypeArgumentInference inference(generic_parameters(),
                                  explicit_specialization_types,
                                  explicit_parameters, arguments);
  if (!inference.HasFailed()) {
    if (auto violation =
            FindConstraintViolation(inference.GetResult(), Constraints())) {
      inference.Fail(*violation);
    }
  }
  return inference;
}

base::Optional<Statement*> GenericCallable::CallableBody() {
  if (auto* decl = TorqueMacroDeclaration::DynamicCast(declaration())) {
    return decl->body;
  } else if (auto* decl =
                 TorqueBuiltinDeclaration::DynamicCast(declaration())) {
    return decl->body;
  } else {
    return base::nullopt;
  }
}

bool Namespace::IsDefaultNamespace() const {
  return this == GlobalContext::GetDefaultNamespace();
}

bool Namespace::IsTestNamespace() const { return name() == kTestNamespaceName; }

const Type* TypeAlias::Resolve() const {
  if (!type_) {
    CurrentScope::Scope scope_activator(ParentScope());
    CurrentSourcePosition::Scope position_activator(Position());
    TypeDeclaration* decl = *delayed_;
    if (being_resolved_) {
      std::stringstream s;
      s << "Cannot create type " << decl->name->value
        << " due to circular dependencies.";
      ReportError(s.str());
    }
    type_ = TypeVisitor::ComputeType(decl);
  }
  return *type_;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
