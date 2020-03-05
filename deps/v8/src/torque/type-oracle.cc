// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/type-oracle.h"
#include "src/torque/type-visitor.h"

namespace v8 {
namespace internal {
namespace torque {

DEFINE_CONTEXTUAL_VARIABLE(TypeOracle)

// static
const std::vector<std::unique_ptr<AggregateType>>&
TypeOracle::GetAggregateTypes() {
  return Get().aggregate_types_;
}

// static
const std::vector<std::unique_ptr<BitFieldStructType>>&
TypeOracle::GetBitFieldStructTypes() {
  return Get().bit_field_struct_types_;
}

// static
void TypeOracle::FinalizeAggregateTypes() {
  size_t current = 0;
  while (current != Get().aggregate_types_.size()) {
    auto& p = Get().aggregate_types_[current++];
    p->Finalize();
  }
}

// static
const Type* TypeOracle::GetGenericTypeInstance(GenericType* generic_type,
                                               TypeVector arg_types) {
  auto& params = generic_type->generic_parameters();

  if (params.size() != arg_types.size()) {
    ReportError("Generic struct takes ", params.size(), " parameters, but ",
                arg_types.size(), " were given");
  }

  if (auto specialization = generic_type->GetSpecialization(arg_types)) {
    return *specialization;
  } else {
    const Type* type = nullptr;
    // AddSpecialization can raise an error, which should be reported in the
    // scope of the code requesting the specialization, not the generic type's
    // parent scope, hence the following block.
    {
      v8::internal::torque::Scope* requester_scope = CurrentScope::Get();
      CurrentScope::Scope generic_scope(generic_type->ParentScope());
      type = TypeVisitor::ComputeType(generic_type->declaration(),
                                      {{generic_type, arg_types}},
                                      requester_scope);
    }
    generic_type->AddSpecialization(arg_types, type);
    return type;
  }
}

// static
Namespace* TypeOracle::CreateGenericTypeInstantiationNamespace() {
  Get().generic_type_instantiation_namespaces_.push_back(
      std::make_unique<Namespace>(GENERIC_TYPE_INSTANTIATION_NAMESPACE_STRING));
  return Get().generic_type_instantiation_namespaces_.back().get();
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
