// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/type-inference.h"

namespace v8 {
namespace internal {
namespace torque {

TypeArgumentInference::TypeArgumentInference(
    const NameVector& type_parameters,
    const TypeVector& explicit_type_arguments,
    const std::vector<TypeExpression*>& term_parameters,
    const TypeVector& term_argument_types)
    : num_explicit_(explicit_type_arguments.size()),
      type_parameter_from_name_(type_parameters.size()),
      inferred_(type_parameters.size()) {
  if (num_explicit_ > type_parameters.size()) {
    Fail("more explicit type arguments than expected");
    return;
  }
  if (term_parameters.size() != term_argument_types.size()) {
    Fail("number of term parameters does not match number of term arguments!");
    return;
  }

  for (size_t i = 0; i < type_parameters.size(); i++) {
    type_parameter_from_name_[type_parameters[i]->value] = i;
  }
  for (size_t i = 0; i < num_explicit_; i++) {
    inferred_[i] = {explicit_type_arguments[i]};
  }

  for (size_t i = 0; i < term_parameters.size(); i++) {
    Match(term_parameters[i], term_argument_types[i]);
    if (HasFailed()) return;
  }

  for (size_t i = 0; i < type_parameters.size(); i++) {
    if (!inferred_[i]) {
      Fail("failed to infer arguments for all type parameters");
      return;
    }
  }
}

TypeVector TypeArgumentInference::GetResult() const {
  CHECK(!HasFailed());
  TypeVector result(inferred_.size());
  std::transform(
      inferred_.begin(), inferred_.end(), result.begin(),
      [](base::Optional<const Type*> maybe_type) { return *maybe_type; });
  return result;
}

void TypeArgumentInference::Match(TypeExpression* parameter,
                                  const Type* argument_type) {
  if (BasicTypeExpression* basic =
          BasicTypeExpression::DynamicCast(parameter)) {
    // If the parameter is referring to one of the type parameters, substitute
    if (basic->namespace_qualification.empty() && !basic->is_constexpr) {
      auto result = type_parameter_from_name_.find(basic->name);
      if (result != type_parameter_from_name_.end()) {
        size_t type_parameter_index = result->second;
        if (type_parameter_index < num_explicit_) {
          return;
        }
        base::Optional<const Type*>& maybe_inferred =
            inferred_[type_parameter_index];
        if (maybe_inferred && *maybe_inferred != argument_type) {
          Fail("found conflicting types for generic parameter");
        } else {
          inferred_[type_parameter_index] = {argument_type};
        }
        return;
      }
    }
    // Try to recurse in case of generic types
    if (!basic->generic_arguments.empty()) {
      auto* argument_struct_type = StructType::DynamicCast(argument_type);
      if (argument_struct_type) {
        MatchGeneric(basic, argument_struct_type);
      }
    }
    // NOTE: We could also check whether ground parameter types match the
    // argument types, but we are only interested in inferring type arguments
    // here
  } else {
    // TODO(gsps): Perform inference on function and union types
  }
}

void TypeArgumentInference::MatchGeneric(BasicTypeExpression* parameter,
                                         const StructType* argument_type) {
  QualifiedName qualified_name{parameter->namespace_qualification,
                               parameter->name};
  GenericStructType* generic_struct =
      Declarations::LookupUniqueGenericStructType(qualified_name);
  auto& specialized_from = argument_type->GetSpecializedFrom();
  if (!specialized_from || specialized_from->generic != generic_struct) {
    return Fail("found conflicting generic type constructors");
  }
  auto& parameters = parameter->generic_arguments;
  auto& argument_types = specialized_from->specialized_types;
  if (parameters.size() != argument_types.size()) {
    Error(
        "cannot infer types from generic-struct-typed parameter with "
        "incompatible number of arguments")
        .Position(parameter->pos)
        .Throw();
  }
  for (size_t i = 0; i < parameters.size(); i++) {
    Match(parameters[i], argument_types[i]);
    if (HasFailed()) return;
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
