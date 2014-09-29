// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_TESTER_H_
#define V8_CCTEST_COMPILER_GRAPH_TESTER_H_

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

class GraphTester : public HandleAndZoneScope, public Graph {
 public:
  GraphTester() : Graph(main_zone()) {}
};


class GraphWithStartNodeTester : public GraphTester {
 public:
  explicit GraphWithStartNodeTester(int num_parameters = 0)
      : builder_(main_zone()),
        start_node_(NewNode(builder_.Start(num_parameters))) {
    SetStart(start_node_);
  }

  Node* start_node() { return start_node_; }

 private:
  CommonOperatorBuilder builder_;
  Node* start_node_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_CCTEST_COMPILER_GRAPH_TESTER_H_
