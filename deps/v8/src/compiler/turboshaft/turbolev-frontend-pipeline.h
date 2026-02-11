// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_
#define V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_

#include "src/compiler/turboshaft/phase.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"

namespace v8::internal::compiler::turboshaft {

bool RunMaglevOptimizations(PipelineData& data,
                            maglev::MaglevCompilationInfo* compilation_info,
                            maglev::Graph* maglev_graph);

void PrintMaglevGraph(PipelineData& data, maglev::Graph* maglev_graph,
                      const char* msg);

inline bool ShouldPrintMaglevGraph(PipelineData& data) {
  return data.info()->trace_turbo_graph() || v8_flags.print_turbolev_frontend;
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_TURBOLEV_FRONTEND_PIPELINE_H_
