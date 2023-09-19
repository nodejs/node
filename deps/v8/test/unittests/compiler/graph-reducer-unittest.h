// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_COMPILER_GRAPH_REDUCER_UNITTEST_H_
#define V8_UNITTESTS_COMPILER_GRAPH_REDUCER_UNITTEST_H_

#include "src/compiler/graph-reducer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace v8 {
namespace internal {
namespace compiler {

struct MockAdvancedReducerEditor : public AdvancedReducer::Editor {
  MOCK_METHOD(void, Revisit, (Node*), (override));
  MOCK_METHOD(void, Replace, (Node*, Node*), (override));
  MOCK_METHOD(void, Replace, (Node*, Node*, NodeId), (override));
  MOCK_METHOD(void, ReplaceWithValue, (Node*, Node*, Node*, Node*), (override));
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_COMPILER_GRAPH_REDUCER_UNITTEST_H_
