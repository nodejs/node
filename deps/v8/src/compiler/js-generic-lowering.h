// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_GENERIC_LOWERING_H_
#define V8_COMPILER_JS_GENERIC_LOWERING_H_

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
class JSGenericLowering final : public AdvancedReducer {
 public:
  JSGenericLowering(JSGraph* jsgraph, Editor* editor, JSHeapBroker* broker);
  ~JSGenericLowering() final;

  const char* reducer_name() const override { return "JSGenericLowering"; }

  Reduction Reduce(Node* node) final;

 protected:
#define DECLARE_LOWER(x, ...) void Lower##x(Node* node);
  // Dispatched depending on opcode.
  JS_OP_LIST(DECLARE_LOWER)
#undef DECLARE_LOWER

  // Helpers to replace existing nodes with a generic call.
  void ReplaceWithBuiltinCall(Node* node, Builtin builtin);
  void ReplaceWithBuiltinCall(Node* node, Callable c,
                              CallDescriptor::Flags flags);
  void ReplaceWithBuiltinCall(Node* node, Callable c,
                              CallDescriptor::Flags flags,
                              Operator::Properties properties);
  void ReplaceWithRuntimeCall(Node* node, Runtime::FunctionId f, int args = -1);

  void ReplaceUnaryOpWithBuiltinCall(Node* node,
                                     Builtin builtin_without_feedback,
                                     Builtin builtin_with_feedback);
  void ReplaceBinaryOpWithBuiltinCall(Node* node,
                                      Builtin builtin_without_feedback,
                                      Builtin builtin_with_feedback);

  Zone* zone() const;
  Isolate* isolate() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  TFGraph* graph() const;
  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
  JSHeapBroker* broker() const { return broker_; }

 private:
  JSGraph* const jsgraph_;
  JSHeapBroker* const broker_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_GENERIC_LOWERING_H_
