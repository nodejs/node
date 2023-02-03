// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_RECREATE_SCHEDULE_H_
#define V8_COMPILER_TURBOSHAFT_RECREATE_SCHEDULE_H_

#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/node-origin-table.h"

namespace v8::internal {
class Zone;
}
namespace v8::internal::compiler {
class Schedule;
class Graph;
class CallDescriptor;
}  // namespace v8::internal::compiler
namespace v8::internal::compiler::turboshaft {
class Graph;

struct RecreateScheduleResult {
  compiler::Graph* graph;
  Schedule* schedule;
};

RecreateScheduleResult RecreateSchedule(const Graph& graph,
                                        CallDescriptor* call_descriptor,
                                        Zone* graph_zone, Zone* phase_zone,
                                        SourcePositionTable* source_positions,
                                        NodeOriginTable* origins);

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_RECREATE_SCHEDULE_H_
