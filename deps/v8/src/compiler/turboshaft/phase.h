// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_PHASE_H_
#define V8_COMPILER_TURBOSHAFT_PHASE_H_

#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/phase.h"
#include "src/compiler/turboshaft/graph.h"

#define DECL_TURBOSHAFT_PHASE_CONSTANTS(Name)                  \
  DECL_PIPELINE_PHASE_CONSTANTS_HELPER(Turboshaft##Name,       \
                                       PhaseKind::kTurboshaft, \
                                       RuntimeCallStats::kThreadSpecific)

namespace v8::internal::compiler {
class Schedule;
}  // namespace v8::internal::compiler

namespace v8::internal::compiler::turboshaft {

class PipelineData {
 public:
  explicit PipelineData(OptimizedCompilationInfo* const& info,
                        Schedule*& schedule, Zone*& graph_zone,
                        JSHeapBroker*& broker, Isolate* const& isolate,
                        SourcePositionTable*& source_positions,
                        NodeOriginTable*& node_origins)
      : info_(info),
        schedule_(schedule),
        graph_zone_(graph_zone),
        broker_(broker),
        isolate_(isolate),
        source_positions_(source_positions),
        node_origins_(node_origins) {}

  bool has_graph() const { return graph_ != nullptr; }
  turboshaft::Graph& graph() const { return *graph_; }

  OptimizedCompilationInfo* info() const { return info_; }
  Schedule* schedule() const { return schedule_; }
  Zone* graph_zone() const { return graph_zone_; }
  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() const { return isolate_; }
  SourcePositionTable* source_positions() const { return source_positions_; }
  NodeOriginTable* node_origins() const { return node_origins_; }

  void CreateTurboshaftGraph() {
    DCHECK_NULL(graph_);
    DCHECK(graph_zone_);
    graph_ = std::make_unique<turboshaft::Graph>(graph_zone_);
  }

  void reset_schedule() { schedule_ = nullptr; }

  void DeleteGraphZone() { graph_ = nullptr; }

 private:
  // Turbofan's PipelineData owns most of these objects. We only hold references
  // to them.
  // TODO(v8:12783, nicohartmann@): Change this once Turbofan pipeline is fully
  // replaced.
  OptimizedCompilationInfo* const& info_;
  Schedule*& schedule_;
  Zone*& graph_zone_;
  JSHeapBroker*& broker_;
  Isolate* const& isolate_;
  SourcePositionTable*& source_positions_;
  NodeOriginTable*& node_origins_;

  std::unique_ptr<turboshaft::Graph> graph_ = nullptr;
};

void PrintTurboshaftGraph(PipelineData* data, Zone* temp_zone,
                          CodeTracer* code_tracer, const char* phase_name);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_PHASE_H_
