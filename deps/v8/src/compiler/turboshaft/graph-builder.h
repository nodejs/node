// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_
#define V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_

#include "src/compiler/turboshaft/graph.h"

namespace v8::internal::compiler {
class Schedule;
}
namespace v8::internal::compiler::turboshaft {
void BuildGraph(Schedule* schedule, Zone* graph_zone, Zone* phase_zone,
                Graph* graph);
}

#endif  // V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_
