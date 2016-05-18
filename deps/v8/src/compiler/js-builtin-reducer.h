// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_BUILTIN_REDUCER_H_
#define V8_COMPILER_JS_BUILTIN_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class TypeCache;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;
class SimplifiedOperatorBuilder;


class JSBuiltinReducer final : public AdvancedReducer {
 public:
  explicit JSBuiltinReducer(Editor* editor, JSGraph* jsgraph);
  ~JSBuiltinReducer() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceFunctionCall(Node* node);
  Reduction ReduceMathMax(Node* node);
  Reduction ReduceMathImul(Node* node);
  Reduction ReduceMathFround(Node* node);
  Reduction ReduceMathRound(Node* node);

  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  SimplifiedOperatorBuilder* simplified() const;

  JSGraph* const jsgraph_;
  TypeCache const& type_cache_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_BUILTIN_REDUCER_H_
