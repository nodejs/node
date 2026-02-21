// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_
#define V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {

namespace compiler {
class Schedule;

void AddTypeAssertions(JSGraph* jsgraph, Schedule* schedule, Zone* phase_zone);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ADD_TYPE_ASSERTIONS_REDUCER_H_
