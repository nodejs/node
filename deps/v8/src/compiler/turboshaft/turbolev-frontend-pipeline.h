// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_
#define V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_

#include "src/compiler/turboshaft/phase.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph.h"

namespace v8::internal::compiler::turboshaft {

class TurbolevFrontendPipeline {
 public:
  explicit TurbolevFrontendPipeline(PipelineData* data, Linkage* linkage);

  std::optional<maglev::Graph*> Run();

  maglev::MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }

 private:
  PipelineData& data_;
  std::unique_ptr<maglev::MaglevCompilationInfo> compilation_info_;
  maglev::Graph* graph_;

  bool ShouldPrintMaglevGraph() {
    return data_.info()->trace_turbo_graph() ||
           v8_flags.print_turbolev_frontend;
  }

  void PrintBytecode();
  void PrintMaglevGraph(const char* msg);
  void PrintInliningTreeDebugInfo();

  template <typename Phase, typename... Args>
  auto Run(Args&&... args);
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_
