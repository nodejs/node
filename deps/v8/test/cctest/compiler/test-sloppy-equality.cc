// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/compiler/node-observer-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

struct TestCase {
  TestCase(const char* l, const char* r, NodeObserver* observer)
      : warmup{std::make_pair(l, r)}, observer(observer) {
    DCHECK_NOT_NULL(observer);
  }
  std::vector<std::pair<const char*, const char*>> warmup;
  NodeObserver* observer;
};

class TestSloppyEqualityFactory {
 public:
  explicit TestSloppyEqualityFactory(Zone* zone) : zone_(zone) {}

  NodeObserver* SpeculativeNumberEqual(NumberOperationHint hint) {
    return zone_->New<CreationObserver>([hint](const Node* node) {
      CHECK_EQ(IrOpcode::kSpeculativeNumberEqual, node->opcode());
      CHECK_EQ(hint, NumberOperationHintOf(node->op()));
    });
  }

  NodeObserver* JSEqual(CompareOperationHint /*hint*/) {
    return zone_->New<CreationObserver>([](const Node* node) {
      CHECK_EQ(IrOpcode::kJSEqual, node->opcode());
      // TODO(paolosev): compare hint
    });
  }

  NodeObserver* OperatorChange(IrOpcode::Value created_op,
                               IrOpcode::Value modified_op) {
    return zone_->New<ModificationObserver>(
        [created_op](const Node* node) {
          CHECK_EQ(created_op, node->opcode());
        },
        [modified_op](const Node* node, const ObservableNodeState& old_state)
            -> NodeObserver::Observation {
          if (old_state.opcode() != node->opcode()) {
            CHECK_EQ(modified_op, node->opcode());
            return NodeObserver::Observation::kStop;
          }
          return NodeObserver::Observation::kContinue;
        });
  }

 private:
  Zone* zone_;
};

TEST(TestSloppyEquality) {
  FlagScope<bool> allow_natives_syntax(&i::FLAG_allow_natives_syntax, true);
  FlagScope<bool> always_opt(&i::FLAG_always_opt, false);
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone zone(isolate->allocator(), ZONE_NAME);
  TestSloppyEqualityFactory f(&zone);
  // TODO(nicohartmann@, v8:5660): Collect more precise feedback for some useful
  // cases.
  TestCase cases[] = {
      {"3", "8", f.SpeculativeNumberEqual(NumberOperationHint::kSignedSmall)},
      //{"3", "null",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"3", "undefined",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"3", "true",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      {"3", "\"abc\"", f.JSEqual(CompareOperationHint::kAny)},
      {"3.14", "3", f.SpeculativeNumberEqual(NumberOperationHint::kNumber)},
      //{"3.14", "null",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"3.14", "undefined",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"3.14", "true",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      {"3.14", "\"abc\"", f.JSEqual(CompareOperationHint::kAny)},
      {"\"abc\"", "3", f.JSEqual(CompareOperationHint::kAny)},
      {"\"abc\"", "null", f.JSEqual(CompareOperationHint::kAny)},
      {"\"abc\"", "undefined", f.JSEqual(CompareOperationHint::kAny)},
      {"\"abc\"", "true", f.JSEqual(CompareOperationHint::kAny)},
      {"\"abc\"", "\"xy\"",
       f.JSEqual(CompareOperationHint::kInternalizedString)},
      //{"true", "3",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"true", "null",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"true", "undefined",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      //{"true", "true",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      {"true", "\"abc\"", f.JSEqual(CompareOperationHint::kAny)},
      //{"undefined", "3",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      {"undefined", "null",
       f.JSEqual(CompareOperationHint::kReceiverOrNullOrUndefined)},
      {"undefined", "undefined",
       f.JSEqual(CompareOperationHint::kReceiverOrNullOrUndefined)},
      //{"undefined", "true",
      // f.SpeculativeNumberEqual(NumberOperationHint::kNumberOrOddball)},
      {"undefined", "\"abc\"", f.JSEqual(CompareOperationHint::kAny)},
      {"{}", "3", f.JSEqual(CompareOperationHint::kAny)},
      {"{}", "null",
       f.JSEqual(CompareOperationHint::kReceiverOrNullOrUndefined)},
      {"{}", "undefined",
       f.JSEqual(CompareOperationHint::kReceiverOrNullOrUndefined)},
      {"{}", "true", f.JSEqual(CompareOperationHint::kAny)},
      {"{}", "\"abc\"", f.JSEqual(CompareOperationHint::kAny)},

      {"3.14", "3",
       f.OperatorChange(IrOpcode::kSpeculativeNumberEqual,
                        IrOpcode::kFloat64Equal)}};

  for (const auto& c : cases) {
    std::ostringstream src;
    src << "function test(a, b) {\n"
        << "  return %ObserveNode(a == b);\n"
        << "}\n"
        << "%PrepareFunctionForOptimization(test);\n";
    for (const auto& args : c.warmup) {
      src << "test(" << args.first << ", " << args.second << ");\n"
          << "%OptimizeFunctionOnNextCall(test);"
          << "test(" << args.first << ", " << args.second << ");\n";
    }

    {
      compiler::ObserveNodeScope scope(isolate, c.observer);
      CompileRun(src.str().c_str());
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
