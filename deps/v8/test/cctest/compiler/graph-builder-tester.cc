// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/graph-builder-tester.h"

#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineCallHelper::MachineCallHelper(Zone* zone, MachineSignature* machine_sig)
    : CallHelper(zone->isolate(), machine_sig),
      parameters_(NULL),
      graph_(NULL) {}


void MachineCallHelper::InitParameters(GraphBuilder* builder,
                                       CommonOperatorBuilder* common) {
  DCHECK_EQ(NULL, parameters_);
  graph_ = builder->graph();
  int param_count = static_cast<int>(parameter_count());
  if (param_count == 0) return;
  parameters_ = graph_->zone()->NewArray<Node*>(param_count);
  for (int i = 0; i < param_count; ++i) {
    parameters_[i] = builder->NewNode(common->Parameter(i), graph_->start());
  }
}


byte* MachineCallHelper::Generate() {
  DCHECK(parameter_count() == 0 || parameters_ != NULL);
  if (!Pipeline::SupportedBackend()) return NULL;
  if (code_.is_null()) {
    Zone* zone = graph_->zone();
    CompilationInfo info(zone->isolate(), zone);
    Linkage linkage(zone,
                    Linkage::GetSimplifiedCDescriptor(zone, machine_sig_));
    Pipeline pipeline(&info);
    code_ = pipeline.GenerateCodeForMachineGraph(&linkage, graph_);
  }
  return code_.ToHandleChecked()->entry();
}


Node* MachineCallHelper::Parameter(size_t index) {
  DCHECK_NE(NULL, parameters_);
  DCHECK(index < parameter_count());
  return parameters_[index];
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
