// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_GRAPH_VERIFIER_H_
#define V8_COMPILER_MACHINE_GRAPH_VERIFIER_H_

#include "src/codegen/machine-type.h"
namespace v8 {
namespace internal {
class Zone;
namespace compiler {

class Graph;
class Linkage;
class Schedule;
class Node;

// Verifies properties of a scheduled graph, such as that the nodes' inputs are
// of the correct type.
class MachineGraphVerifier {
 public:
  static void Run(Graph* graph, Schedule const* const schedule,
                  Linkage* linkage, bool is_stub, const char* name,
                  Zone* temp_zone);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_GRAPH_VERIFIER_H_
