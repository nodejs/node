// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-operator.h"
#include "test/cctest/cctest.h"
#include "test/common/flag-utils.h"
#include "test/common/node-observer-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

TEST(TestVerifyType) {
  FlagScope<bool> allow_natives_syntax(&i::v8_flags.allow_natives_syntax, true);
  HandleAndZoneScope handle_scope;
  Isolate* isolate = handle_scope.main_isolate();
  Zone* zone = handle_scope.main_zone();

  const char* source =
      "function test(b) {\n"
      "  let x = -8;\n"
      "  if(b) x = 42;\n"
      "  return %ObserveNode(%VerifyType(x));\n"
      "}\n"
      "\n"
      "%PrepareFunctionForOptimization(test);\n"
      "test(false); test(true);\n"
      "%OptimizeFunctionOnNextCall(test);\n"
      "test(false);\n";

  bool js_intrinsic_lowering_happened = false;
  bool simplified_lowering_happened = false;

  ModificationObserver observer(
      [](const Node* node) {
        CHECK_EQ(node->opcode(), IrOpcode::kJSCallRuntime);
        CHECK_EQ(CallRuntimeParametersOf(node->op()).id(),
                 Runtime::kVerifyType);
      },
      [&](const Node* node, const ObservableNodeState& old_state) {
        if (old_state.opcode() == IrOpcode::kJSCallRuntime &&
            node->opcode() == IrOpcode::kVerifyType) {
          // CallRuntime is lowered to VerifyType in JSIntrinsicLowering.
          js_intrinsic_lowering_happened = true;
          return NodeObserver::Observation::kContinue;
        } else if (old_state.opcode() == IrOpcode::kVerifyType &&
                   node->opcode() == IrOpcode::kAssertType) {
          // VerifyType is lowered to AssertType in SimplifiedLowering.
          // AssertType asserts for the type of its value input.
          Type asserted_type = OpParameter<Type>(node->op());
          CHECK(asserted_type.Equals(Type::Range(-8, 42, zone)));
          simplified_lowering_happened = true;
          return NodeObserver::Observation::kStop;
        } else {
          // Every other lowering would be wrong, so fail the test.
          UNREACHABLE();
        }
      });

  compiler::ObserveNodeScope scope(isolate, &observer);
  CompileRun(source);

  CHECK(js_intrinsic_lowering_happened);
  CHECK(simplified_lowering_happened);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
