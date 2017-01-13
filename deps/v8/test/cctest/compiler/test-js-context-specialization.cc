// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-context-specialization.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/source-position.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/function-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class ContextSpecializationTester : public HandleAndZoneScope {
 public:
  ContextSpecializationTester()
      : graph_(new (main_zone()) Graph(main_zone())),
        common_(main_zone()),
        javascript_(main_zone()),
        machine_(main_zone()),
        simplified_(main_zone()),
        jsgraph_(main_isolate(), graph(), common(), &javascript_, &simplified_,
                 &machine_),
        reducer_(main_zone(), graph()),
        spec_(&reducer_, jsgraph(), MaybeHandle<Context>()) {}

  JSContextSpecialization* spec() { return &spec_; }
  Factory* factory() { return main_isolate()->factory(); }
  CommonOperatorBuilder* common() { return &common_; }
  JSOperatorBuilder* javascript() { return &javascript_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }
  JSGraph* jsgraph() { return &jsgraph_; }
  Graph* graph() { return graph_; }

 private:
  Graph* graph_;
  CommonOperatorBuilder common_;
  JSOperatorBuilder javascript_;
  MachineOperatorBuilder machine_;
  SimplifiedOperatorBuilder simplified_;
  JSGraph jsgraph_;
  GraphReducer reducer_;
  JSContextSpecialization spec_;
};


TEST(ReduceJSLoadContext) {
  ContextSpecializationTester t;

  Node* start = t.graph()->NewNode(t.common()->Start(0));
  t.graph()->SetStart(start);

  // Make a context and initialize it a bit for this test.
  Handle<Context> native = t.factory()->NewNativeContext();
  Handle<Context> subcontext1 = t.factory()->NewNativeContext();
  Handle<Context> subcontext2 = t.factory()->NewNativeContext();
  subcontext2->set_previous(*subcontext1);
  subcontext1->set_previous(*native);
  Handle<Object> expected = t.factory()->InternalizeUtf8String("gboy!");
  const int slot = Context::NATIVE_CONTEXT_INDEX;
  native->set(slot, *expected);

  Node* const_context = t.jsgraph()->Constant(native);
  Node* deep_const_context = t.jsgraph()->Constant(subcontext2);
  Node* param_context = t.graph()->NewNode(t.common()->Parameter(0), start);

  {
    // Mutable slot, constant context, depth = 0 => do nothing.
    Node* load = t.graph()->NewNode(t.javascript()->LoadContext(0, 0, false),
                                    const_context, const_context, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(!r.Changed());
  }

  {
    // Mutable slot, non-constant context, depth = 0 => do nothing.
    Node* load = t.graph()->NewNode(t.javascript()->LoadContext(0, 0, false),
                                    param_context, param_context, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(!r.Changed());
  }

  {
    // Mutable slot, constant context, depth > 0 => fold-in parent context.
    Node* load = t.graph()->NewNode(
        t.javascript()->LoadContext(2, Context::GLOBAL_EVAL_FUN_INDEX, false),
        deep_const_context, deep_const_context, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(r.Changed());
    Node* new_context_input = NodeProperties::GetValueInput(r.replacement(), 0);
    CHECK_EQ(IrOpcode::kHeapConstant, new_context_input->opcode());
    HeapObjectMatcher match(new_context_input);
    CHECK_EQ(*native, *match.Value());
    ContextAccess access = OpParameter<ContextAccess>(r.replacement());
    CHECK_EQ(Context::GLOBAL_EVAL_FUN_INDEX, static_cast<int>(access.index()));
    CHECK_EQ(0, static_cast<int>(access.depth()));
    CHECK_EQ(false, access.immutable());
  }

  {
    // Immutable slot, constant context, depth = 0 => specialize.
    Node* load = t.graph()->NewNode(t.javascript()->LoadContext(0, slot, true),
                                    const_context, const_context, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(r.Changed());
    CHECK(r.replacement() != load);

    HeapObjectMatcher match(r.replacement());
    CHECK(match.HasValue());
    CHECK_EQ(*expected, *match.Value());
  }

  // TODO(titzer): test with other kinds of contexts, e.g. a function context.
  // TODO(sigurds): test that loads below create context are not optimized
}


TEST(ReduceJSStoreContext) {
  ContextSpecializationTester t;

  Node* start = t.graph()->NewNode(t.common()->Start(0));
  t.graph()->SetStart(start);

  // Make a context and initialize it a bit for this test.
  Handle<Context> native = t.factory()->NewNativeContext();
  Handle<Context> subcontext1 = t.factory()->NewNativeContext();
  Handle<Context> subcontext2 = t.factory()->NewNativeContext();
  subcontext2->set_previous(*subcontext1);
  subcontext1->set_previous(*native);
  Handle<Object> expected = t.factory()->InternalizeUtf8String("gboy!");
  const int slot = Context::NATIVE_CONTEXT_INDEX;
  native->set(slot, *expected);

  Node* const_context = t.jsgraph()->Constant(native);
  Node* deep_const_context = t.jsgraph()->Constant(subcontext2);
  Node* param_context = t.graph()->NewNode(t.common()->Parameter(0), start);

  {
    // Mutable slot, constant context, depth = 0 => do nothing.
    Node* load =
        t.graph()->NewNode(t.javascript()->StoreContext(0, 0), const_context,
                           const_context, const_context, start, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(!r.Changed());
  }

  {
    // Mutable slot, non-constant context, depth = 0 => do nothing.
    Node* load =
        t.graph()->NewNode(t.javascript()->StoreContext(0, 0), param_context,
                           param_context, const_context, start, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(!r.Changed());
  }

  {
    // Immutable slot, constant context, depth = 0 => do nothing.
    Node* load =
        t.graph()->NewNode(t.javascript()->StoreContext(0, slot), const_context,
                           const_context, const_context, start, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(!r.Changed());
  }

  {
    // Mutable slot, constant context, depth > 0 => fold-in parent context.
    Node* load = t.graph()->NewNode(
        t.javascript()->StoreContext(2, Context::GLOBAL_EVAL_FUN_INDEX),
        deep_const_context, deep_const_context, const_context, start, start);
    Reduction r = t.spec()->Reduce(load);
    CHECK(r.Changed());
    Node* new_context_input = NodeProperties::GetValueInput(r.replacement(), 0);
    CHECK_EQ(IrOpcode::kHeapConstant, new_context_input->opcode());
    HeapObjectMatcher match(new_context_input);
    CHECK_EQ(*native, *match.Value());
    ContextAccess access = OpParameter<ContextAccess>(r.replacement());
    CHECK_EQ(Context::GLOBAL_EVAL_FUN_INDEX, static_cast<int>(access.index()));
    CHECK_EQ(0, static_cast<int>(access.depth()));
    CHECK_EQ(false, access.immutable());
  }
}


TEST(SpecializeJSFunction_ToConstant1) {
  FunctionTester T(
      "(function() { var x = 1; function inc(a)"
      " { return a + x; } return inc; })()");

  T.CheckCall(1.0, 0.0, 0.0);
  T.CheckCall(2.0, 1.0, 0.0);
  T.CheckCall(2.1, 1.1, 0.0);
}


TEST(SpecializeJSFunction_ToConstant2) {
  FunctionTester T(
      "(function() { var x = 1.5; var y = 2.25; var z = 3.75;"
      " function f(a) { return a - x + y - z; } return f; })()");

  T.CheckCall(-3.0, 0.0, 0.0);
  T.CheckCall(-2.0, 1.0, 0.0);
  T.CheckCall(-1.9, 1.1, 0.0);
}


TEST(SpecializeJSFunction_ToConstant3) {
  FunctionTester T(
      "(function() { var x = -11.5; function inc()"
      " { return (function(a) { return a + x; }); }"
      " return inc(); })()");

  T.CheckCall(-11.5, 0.0, 0.0);
  T.CheckCall(-10.5, 1.0, 0.0);
  T.CheckCall(-10.4, 1.1, 0.0);
}


TEST(SpecializeJSFunction_ToConstant_uninit) {
  {
    FunctionTester T(
        "(function() { if (false) { var x = 1; } function inc(a)"
        " { return x; } return inc; })()");  // x is undefined!
    i::Isolate* isolate = CcTest::i_isolate();
    CHECK(
        T.Call(T.Val(0.0), T.Val(0.0)).ToHandleChecked()->IsUndefined(isolate));
    CHECK(
        T.Call(T.Val(2.0), T.Val(0.0)).ToHandleChecked()->IsUndefined(isolate));
    CHECK(T.Call(T.Val(-2.1), T.Val(0.0))
              .ToHandleChecked()
              ->IsUndefined(isolate));
  }

  {
    FunctionTester T(
        "(function() { if (false) { var x = 1; } function inc(a)"
        " { return a + x; } return inc; })()");  // x is undefined!

    CHECK(T.Call(T.Val(0.0), T.Val(0.0)).ToHandleChecked()->IsNaN());
    CHECK(T.Call(T.Val(2.0), T.Val(0.0)).ToHandleChecked()->IsNaN());
    CHECK(T.Call(T.Val(-2.1), T.Val(0.0)).ToHandleChecked()->IsNaN());
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
