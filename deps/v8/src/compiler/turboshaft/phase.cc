// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/phase.h"

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/graph-visualizer.h"
#include "src/diagnostics/code-tracer.h"

namespace v8::internal::compiler::turboshaft {

void PrintTurboshaftGraph(Zone* temp_zone, CodeTracer* code_tracer,
                          const char* phase_name) {
  const PipelineData& data = PipelineData::Get();
  if (data.info()->trace_turbo_json()) {
    UnparkedScopeIfNeeded scope(data.broker());
    AllowHandleDereference allow_deref;
    turboshaft::Graph& graph = data.graph();

    {
      TurboJsonFile json_of(data.info(), std::ios_base::app);
      json_of << "{\"name\":\"" << phase_name
              << "\",\"type\":\"turboshaft_graph\",\"data\":"
              << AsJSON(graph, data.node_origins(), temp_zone) << "},\n";
    }
    PrintTurboshaftCustomDataPerOperation(
        data.info(), "Properties", graph,
        [](std::ostream& stream, const turboshaft::Graph& graph,
           turboshaft::OpIndex index) -> bool {
          const auto& op = graph.Get(index);
          op.PrintOptions(stream);
          return true;
        });
    PrintTurboshaftCustomDataPerOperation(
        data.info(), "Types", graph,
        [](std::ostream& stream, const turboshaft::Graph& graph,
           turboshaft::OpIndex index) -> bool {
          turboshaft::Type type = graph.operation_types()[index];
          if (!type.IsInvalid() && !type.IsNone()) {
            type.PrintTo(stream);
            return true;
          }
          return false;
        });
    PrintTurboshaftCustomDataPerOperation(
        data.info(), "Use Count (saturated)", graph,
        [](std::ostream& stream, const turboshaft::Graph& graph,
           turboshaft::OpIndex index) -> bool {
          stream << static_cast<int>(
              graph.Get(index).saturated_use_count.Get());
          return true;
        });
#ifdef DEBUG
    PrintTurboshaftCustomDataPerBlock(
        data.info(), "Type Refinements", graph,
        [](std::ostream& stream, const turboshaft::Graph& graph,
           turboshaft::BlockIndex index) -> bool {
          const std::vector<std::pair<turboshaft::OpIndex, turboshaft::Type>>&
              refinements = graph.block_type_refinement()[index];
          if (refinements.empty()) return false;
          stream << "\\n";
          for (const auto& [op, type] : refinements) {
            stream << op << " : " << type << "\\n";
          }
          return true;
        });
#endif  // DEBUG
  }

  if (data.info()->trace_turbo_graph()) {
    DCHECK(code_tracer);
    UnparkedScopeIfNeeded scope(data.broker());
    AllowHandleDereference allow_deref;

    CodeTracer::StreamScope tracing_scope(code_tracer);
    tracing_scope.stream() << "\n----- " << phase_name << " -----\n"
                           << data.graph();
  }
}

}  // namespace v8::internal::compiler::turboshaft
