// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/compiler/change-lowering.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/typer.h"
#include "src/compiler/verifier.h"
#include "src/execution.h"
#include "src/globals.h"
#include "src/parser.h"
#include "src/rewriter.h"
#include "src/scopes.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

template <typename ReturnType>
class ChangesLoweringTester : public GraphBuilderTester<ReturnType> {
 public:
  explicit ChangesLoweringTester(MachineType p0 = kMachNone)
      : GraphBuilderTester<ReturnType>(p0),
        typer(this->zone()),
        javascript(this->zone()),
        jsgraph(this->graph(), this->common(), &javascript, &typer,
                this->machine()),
        function(Handle<JSFunction>::null()) {}

  Typer typer;
  JSOperatorBuilder javascript;
  JSGraph jsgraph;
  Handle<JSFunction> function;

  Node* start() { return this->graph()->start(); }

  template <typename T>
  T* CallWithPotentialGC() {
    // TODO(titzer): we need to wrap the code in a JSFunction and call it via
    // Execution::Call() so that the GC knows about the frame, can walk it,
    // relocate the code object if necessary, etc.
    // This is pretty ugly and at the least should be moved up to helpers.
    if (function.is_null()) {
      function =
          v8::Utils::OpenHandle(*v8::Handle<v8::Function>::Cast(CompileRun(
              "(function() { 'use strict'; return 2.7123; })")));
      CompilationInfoWithZone info(function);
      CHECK(Parser::Parse(&info));
      info.SetOptimizing(BailoutId::None(), Handle<Code>(function->code()));
      CHECK(Rewriter::Rewrite(&info));
      CHECK(Scope::Analyze(&info));
      CHECK_NE(NULL, info.scope());
      Handle<ScopeInfo> scope_info =
          ScopeInfo::Create(info.scope(), info.zone());
      info.shared_info()->set_scope_info(*scope_info);
      Pipeline pipeline(&info);
      Linkage linkage(&info);
      Handle<Code> code =
          pipeline.GenerateCodeForMachineGraph(&linkage, this->graph());
      CHECK(!code.is_null());
      function->ReplaceCode(*code);
    }
    Handle<Object>* args = NULL;
    MaybeHandle<Object> result =
        Execution::Call(this->isolate(), function, factory()->undefined_value(),
                        0, args, false);
    return T::cast(*result.ToHandleChecked());
  }

  void StoreFloat64(Node* node, double* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    this->Store(kMachFloat64, ptr_node, node);
  }

  Node* LoadInt32(int32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachInt32, ptr_node);
  }

  Node* LoadUint32(uint32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachUint32, ptr_node);
  }

  Node* LoadFloat64(double* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachFloat64, ptr_node);
  }

  void CheckNumber(double expected, Object* number) {
    CHECK(this->isolate()->factory()->NewNumber(expected)->SameValue(number));
  }

  void BuildAndLower(const Operator* op) {
    // We build a graph by hand here, because the raw machine assembler
    // does not add the correct control and effect nodes.
    Node* p0 = this->Parameter(0);
    Node* change = this->graph()->NewNode(op, p0);
    Node* ret = this->graph()->NewNode(this->common()->Return(), change,
                                       this->start(), this->start());
    Node* end = this->graph()->NewNode(this->common()->End(), ret);
    this->graph()->SetEnd(end);
    LowerChange(change);
  }

  void BuildStoreAndLower(const Operator* op, const Operator* store_op,
                          void* location) {
    // We build a graph by hand here, because the raw machine assembler
    // does not add the correct control and effect nodes.
    Node* p0 = this->Parameter(0);
    Node* change = this->graph()->NewNode(op, p0);
    Node* store = this->graph()->NewNode(
        store_op, this->PointerConstant(location), this->Int32Constant(0),
        change, this->start(), this->start());
    Node* ret = this->graph()->NewNode(
        this->common()->Return(), this->Int32Constant(0), store, this->start());
    Node* end = this->graph()->NewNode(this->common()->End(), ret);
    this->graph()->SetEnd(end);
    LowerChange(change);
  }

  void BuildLoadAndLower(const Operator* op, const Operator* load_op,
                         void* location) {
    // We build a graph by hand here, because the raw machine assembler
    // does not add the correct control and effect nodes.
    Node* load =
        this->graph()->NewNode(load_op, this->PointerConstant(location),
                               this->Int32Constant(0), this->start());
    Node* change = this->graph()->NewNode(op, load);
    Node* ret = this->graph()->NewNode(this->common()->Return(), change,
                                       this->start(), this->start());
    Node* end = this->graph()->NewNode(this->common()->End(), ret);
    this->graph()->SetEnd(end);
    LowerChange(change);
  }

  void LowerChange(Node* change) {
    // Run the graph reducer with changes lowering on a single node.
    CompilationInfo info(this->isolate(), this->zone());
    Linkage linkage(&info);
    ChangeLowering lowering(&jsgraph, &linkage);
    GraphReducer reducer(this->graph());
    reducer.AddReducer(&lowering);
    reducer.ReduceNode(change);
    Verifier::Run(this->graph());
  }

  Factory* factory() { return this->isolate()->factory(); }
  Heap* heap() { return this->isolate()->heap(); }
};


TEST(RunChangeTaggedToInt32) {
  // Build and lower a graph by hand.
  ChangesLoweringTester<int32_t> t(kMachAnyTagged);
  t.BuildAndLower(t.simplified()->ChangeTaggedToInt32());

  if (Pipeline::SupportedTarget()) {
    FOR_INT32_INPUTS(i) {
      int32_t input = *i;

      if (Smi::IsValid(input)) {
        int32_t result = t.Call(Smi::FromInt(input));
        CHECK_EQ(input, result);
      }

      {
        Handle<Object> number = t.factory()->NewNumber(input);
        int32_t result = t.Call(*number);
        CHECK_EQ(input, result);
      }

      {
        Handle<HeapNumber> number = t.factory()->NewHeapNumber(input);
        int32_t result = t.Call(*number);
        CHECK_EQ(input, result);
      }
    }
  }
}


TEST(RunChangeTaggedToUint32) {
  // Build and lower a graph by hand.
  ChangesLoweringTester<uint32_t> t(kMachAnyTagged);
  t.BuildAndLower(t.simplified()->ChangeTaggedToUint32());

  if (Pipeline::SupportedTarget()) {
    FOR_UINT32_INPUTS(i) {
      uint32_t input = *i;

      if (Smi::IsValid(input)) {
        uint32_t result = t.Call(Smi::FromInt(input));
        CHECK_EQ(static_cast<int32_t>(input), static_cast<int32_t>(result));
      }

      {
        Handle<Object> number = t.factory()->NewNumber(input);
        uint32_t result = t.Call(*number);
        CHECK_EQ(static_cast<int32_t>(input), static_cast<int32_t>(result));
      }

      {
        Handle<HeapNumber> number = t.factory()->NewHeapNumber(input);
        uint32_t result = t.Call(*number);
        CHECK_EQ(static_cast<int32_t>(input), static_cast<int32_t>(result));
      }
    }
  }
}


TEST(RunChangeTaggedToFloat64) {
  ChangesLoweringTester<int32_t> t(kMachAnyTagged);
  double result;

  t.BuildStoreAndLower(
      t.simplified()->ChangeTaggedToFloat64(),
      t.machine()->Store(StoreRepresentation(kMachFloat64, kNoWriteBarrier)),
      &result);

  if (Pipeline::SupportedTarget()) {
    FOR_INT32_INPUTS(i) {
      int32_t input = *i;

      if (Smi::IsValid(input)) {
        t.Call(Smi::FromInt(input));
        CHECK_EQ(input, static_cast<int32_t>(result));
      }

      {
        Handle<Object> number = t.factory()->NewNumber(input);
        t.Call(*number);
        CHECK_EQ(input, static_cast<int32_t>(result));
      }

      {
        Handle<HeapNumber> number = t.factory()->NewHeapNumber(input);
        t.Call(*number);
        CHECK_EQ(input, static_cast<int32_t>(result));
      }
    }
  }

  if (Pipeline::SupportedTarget()) {
    FOR_FLOAT64_INPUTS(i) {
      double input = *i;
      {
        Handle<Object> number = t.factory()->NewNumber(input);
        t.Call(*number);
        CHECK_EQ(input, result);
      }

      {
        Handle<HeapNumber> number = t.factory()->NewHeapNumber(input);
        t.Call(*number);
        CHECK_EQ(input, result);
      }
    }
  }
}


TEST(RunChangeBoolToBit) {
  ChangesLoweringTester<int32_t> t(kMachAnyTagged);
  t.BuildAndLower(t.simplified()->ChangeBoolToBit());

  if (Pipeline::SupportedTarget()) {
    Object* true_obj = t.heap()->true_value();
    int32_t result = t.Call(true_obj);
    CHECK_EQ(1, result);
  }

  if (Pipeline::SupportedTarget()) {
    Object* false_obj = t.heap()->false_value();
    int32_t result = t.Call(false_obj);
    CHECK_EQ(0, result);
  }
}


TEST(RunChangeBitToBool) {
  ChangesLoweringTester<Object*> t(kMachInt32);
  t.BuildAndLower(t.simplified()->ChangeBitToBool());

  if (Pipeline::SupportedTarget()) {
    Object* result = t.Call(1);
    Object* true_obj = t.heap()->true_value();
    CHECK_EQ(true_obj, result);
  }

  if (Pipeline::SupportedTarget()) {
    Object* result = t.Call(0);
    Object* false_obj = t.heap()->false_value();
    CHECK_EQ(false_obj, result);
  }
}


#if V8_TURBOFAN_BACKEND
// TODO(titzer): disabled on ARM

TEST(RunChangeInt32ToTaggedSmi) {
  ChangesLoweringTester<Object*> t;
  int32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeInt32ToTagged(),
                      t.machine()->Load(kMachInt32), &input);

  if (Pipeline::SupportedTarget()) {
    FOR_INT32_INPUTS(i) {
      input = *i;
      if (!Smi::IsValid(input)) continue;
      Object* result = t.Call();
      t.CheckNumber(static_cast<double>(input), result);
    }
  }
}


TEST(RunChangeUint32ToTaggedSmi) {
  ChangesLoweringTester<Object*> t;
  uint32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeUint32ToTagged(),
                      t.machine()->Load(kMachUint32), &input);

  if (Pipeline::SupportedTarget()) {
    FOR_UINT32_INPUTS(i) {
      input = *i;
      if (input > static_cast<uint32_t>(Smi::kMaxValue)) continue;
      Object* result = t.Call();
      double expected = static_cast<double>(input);
      t.CheckNumber(expected, result);
    }
  }
}


TEST(RunChangeInt32ToTagged) {
  ChangesLoweringTester<Object*> t;
  int32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeInt32ToTagged(),
                      t.machine()->Load(kMachInt32), &input);

  if (Pipeline::SupportedTarget()) {
    for (int m = 0; m < 3; m++) {  // Try 3 GC modes.
      FOR_INT32_INPUTS(i) {
        if (m == 0) CcTest::heap()->EnableInlineAllocation();
        if (m == 1) CcTest::heap()->DisableInlineAllocation();
        if (m == 2) SimulateFullSpace(CcTest::heap()->new_space());

        input = *i;
        Object* result = t.CallWithPotentialGC<Object>();
        t.CheckNumber(static_cast<double>(input), result);
      }
    }
  }
}


TEST(RunChangeUint32ToTagged) {
  ChangesLoweringTester<Object*> t;
  uint32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeUint32ToTagged(),
                      t.machine()->Load(kMachUint32), &input);

  if (Pipeline::SupportedTarget()) {
    for (int m = 0; m < 3; m++) {  // Try 3 GC modes.
      FOR_UINT32_INPUTS(i) {
        if (m == 0) CcTest::heap()->EnableInlineAllocation();
        if (m == 1) CcTest::heap()->DisableInlineAllocation();
        if (m == 2) SimulateFullSpace(CcTest::heap()->new_space());

        input = *i;
        Object* result = t.CallWithPotentialGC<Object>();
        double expected = static_cast<double>(input);
        t.CheckNumber(expected, result);
      }
    }
  }
}


TEST(RunChangeFloat64ToTagged) {
  ChangesLoweringTester<Object*> t;
  double input;
  t.BuildLoadAndLower(t.simplified()->ChangeFloat64ToTagged(),
                      t.machine()->Load(kMachFloat64), &input);

  if (Pipeline::SupportedTarget()) {
    for (int m = 0; m < 3; m++) {  // Try 3 GC modes.
      FOR_FLOAT64_INPUTS(i) {
        if (m == 0) CcTest::heap()->EnableInlineAllocation();
        if (m == 1) CcTest::heap()->DisableInlineAllocation();
        if (m == 2) SimulateFullSpace(CcTest::heap()->new_space());

        input = *i;
        Object* result = t.CallWithPotentialGC<Object>();
        t.CheckNumber(input, result);
      }
    }
  }
}

#endif  // V8_TURBOFAN_BACKEND
