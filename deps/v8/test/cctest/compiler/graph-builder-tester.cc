// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/graph-builder-tester.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineCallHelper::MachineCallHelper(Zone* zone,
                                     MachineCallDescriptorBuilder* builder)
    : CallHelper(zone->isolate()),
      call_descriptor_builder_(builder),
      parameters_(NULL),
      graph_(NULL) {}


void MachineCallHelper::InitParameters(GraphBuilder* builder,
                                       CommonOperatorBuilder* common) {
  DCHECK_EQ(NULL, parameters_);
  graph_ = builder->graph();
  if (parameter_count() == 0) return;
  parameters_ = graph_->zone()->NewArray<Node*>(parameter_count());
  for (int i = 0; i < parameter_count(); ++i) {
    parameters_[i] = builder->NewNode(common->Parameter(i), graph_->start());
  }
}


byte* MachineCallHelper::Generate() {
  DCHECK(parameter_count() == 0 || parameters_ != NULL);
  if (!Pipeline::SupportedBackend()) return NULL;
  if (code_.is_null()) {
    Zone* zone = graph_->zone();
    CompilationInfo info(zone->isolate(), zone);
    Linkage linkage(&info, call_descriptor_builder_->BuildCallDescriptor(zone));
    Pipeline pipeline(&info);
    code_ = pipeline.GenerateCodeForMachineGraph(&linkage, graph_);
  }
  return code_.ToHandleChecked()->entry();
}


void MachineCallHelper::VerifyParameters(int parameter_count,
                                         MachineType* parameter_types) {
  CHECK_EQ(this->parameter_count(), parameter_count);
  const MachineType* expected_types =
      call_descriptor_builder_->parameter_types();
  for (int i = 0; i < parameter_count; i++) {
    CHECK_EQ(expected_types[i], parameter_types[i]);
  }
}


Node* MachineCallHelper::Parameter(int offset) {
  DCHECK_NE(NULL, parameters_);
  DCHECK(0 <= offset && offset < parameter_count());
  return parameters_[offset];
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
