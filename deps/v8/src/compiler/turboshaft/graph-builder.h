// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_
#define V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_

#include "src/codegen/bailout-reason.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/turboshaft/graph.h"

namespace v8::internal::compiler {
class Schedule;
class SourcePositionTable;
}
namespace v8::internal::compiler::turboshaft {
base::Optional<BailoutReason> BuildGraph(Schedule* schedule, Zone* phase_zone,
                                         Linkage* linkage);
}

#endif  // V8_COMPILER_TURBOSHAFT_GRAPH_BUILDER_H_
