// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GENERIC_LOWERING_H_
#define V8_COMPILER_JS_GENERIC_LOWERING_H_

#include "src/v8.h"

#include "src/allocation.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/lowering-builder.h"
#include "src/compiler/opcodes.h"
#include "src/unique.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HydrogenCodeStub;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class MachineOperatorBuilder;
class Linkage;

// Lowers JS-level operators to runtime and IC calls in the "generic" case.
class JSGenericLowering : public LoweringBuilder {
 public:
  JSGenericLowering(CompilationInfo* info, JSGraph* graph,
                    MachineOperatorBuilder* machine,
                    SourcePositionTable* source_positions);
  virtual ~JSGenericLowering() {}

  virtual void Lower(Node* node);

 protected:
// Dispatched depending on opcode.
#define DECLARE_LOWER(x) Node* Lower##x(Node* node);
  ALL_OP_LIST(DECLARE_LOWER)
#undef DECLARE_LOWER

  // Helpers to create new constant nodes.
  Node* SmiConstant(int immediate);
  Node* Int32Constant(int immediate);
  Node* CodeConstant(Handle<Code> code);
  Node* FunctionConstant(Handle<JSFunction> function);
  Node* ExternalConstant(ExternalReference ref);

  // Helpers to patch existing nodes in the graph.
  void PatchOperator(Node* node, Operator* new_op);
  void PatchInsertInput(Node* node, int index, Node* input);

  // Helpers to replace existing nodes with a generic call.
  void ReplaceWithCompareIC(Node* node, Token::Value token, bool pure);
  void ReplaceWithICStubCall(Node* node, HydrogenCodeStub* stub);
  void ReplaceWithBuiltinCall(Node* node, Builtins::JavaScript id, int args);
  void ReplaceWithRuntimeCall(Node* node, Runtime::FunctionId f, int args = -1);

  Zone* zone() const { return graph()->zone(); }
  Isolate* isolate() const { return zone()->isolate(); }
  JSGraph* jsgraph() const { return jsgraph_; }
  Graph* graph() const { return jsgraph()->graph(); }
  Linkage* linkage() const { return linkage_; }
  CompilationInfo* info() const { return info_; }
  CommonOperatorBuilder* common() const { return jsgraph()->common(); }
  MachineOperatorBuilder* machine() const { return machine_; }

 private:
  CompilationInfo* info_;
  JSGraph* jsgraph_;
  Linkage* linkage_;
  MachineOperatorBuilder* machine_;
  SetOncePointer<Node> centrystub_constant_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_JS_GENERIC_LOWERING_H_
