// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GENERIC_LOWERING_H_
#define V8_COMPILER_JS_GENERIC_LOWERING_H_

#include "src/code-factory.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/linkage.h"
#include "src/compiler/opcodes.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class JSGraph;
class MachineOperatorBuilder;
class Linkage;


// Lowers JS-level operators to runtime and IC calls in the "generic" case.
class JSGenericLowering final : public Reducer {
 public:
  JSGenericLowering(bool is_typing_enabled, JSGraph* jsgraph);
  ~JSGenericLowering() final;

  Reduction Reduce(Node* node) final;

 protected:
#define DECLARE_LOWER(x) void Lower##x(Node* node);
  // Dispatched depending on opcode.
  JS_OP_LIST(DECLARE_LOWER)
#undef DECLARE_LOWER

  // Helpers to replace existing nodes with a generic call.
  void ReplaceWithCompareIC(Node* node, Token::Value token);
  void ReplaceWithStubCall(Node* node, Callable c, CallDescriptor::Flags flags);
  void ReplaceWithBuiltinCall(Node* node, Builtins::JavaScript id, int args);
  void ReplaceWithRuntimeCall(Node* node, Runtime::FunctionId f, int args = -1);

  // Helper for optimization of JSCallFunction.
  bool TryLowerDirectJSCall(Node* node);

  Zone* zone() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Graph* graph() const;
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;

 private:
  bool const is_typing_enabled_;
  JSGraph* const jsgraph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_GENERIC_LOWERING_H_
