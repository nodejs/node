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
const std::vector<std::unique_ptr<AggregateType>>*
TypeOracle::GetAggregateTypes() {
  return &Get().aggregate_types_;
}

// static
void TypeOracle::FinalizeAggregateTypes() {
  for (const std::unique_ptr<AggregateType>& p : Get().aggregate_types_) {
    p->Finalize();
  }
}

// static
const StructType* TypeOracle::GetGenericStructTypeInstance(
    GenericStructType* generic_struct, TypeVector arg_types) {
  auto& params = generic_struct->generic_parameters();
  auto& specializations = generic_struct->specializations();

  if (params.size() != arg_types.size()) {
    ReportError("Generic struct takes ", params.size(), " parameters, but ",
                arg_types.size(), " were given");
  }

  if (auto specialization = specializations.Get(arg_types)) {
    return *specialization;
  } else {
    CurrentScope::Scope generic_scope(generic_struct->ParentScope());
    auto struct_type = TypeVisitor::ComputeType(generic_struct->declaration(),
                                                {{generic_struct, arg_types}});
    specializations.Add(arg_types, struct_type);
    return struct_type;
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
