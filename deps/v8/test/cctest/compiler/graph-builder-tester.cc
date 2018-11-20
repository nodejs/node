// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/graph-builder-tester.h"

#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"

namespace v8 {
namespace internal {
namespace compiler {

MachineCallHelper::MachineCallHelper(Isolate* isolate,
                                     MachineSignature* machine_sig)
    : CallHelper(isolate, machine_sig),
      parameters_(NULL),
      isolate_(isolate),
      graph_(NULL) {}


void MachineCallHelper::InitParameters(GraphBuilder* builder,
                                       CommonOperatorBuilder* common) {
  DCHECK(!parameters_);
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
    CallDescriptor* desc =
        Linkage::GetSimplifiedCDescriptor(zone, machine_sig_);
    code_ = Pipeline::GenerateCodeForTesting(isolate_, desc, graph_);
  }
  return code_.ToHandleChecked()->entry();
}


Node* MachineCallHelper::Parameter(size_t index) {
  DCHECK(parameters_);
  DCHECK(index < parameter_count());
  return parameters_[index];
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
