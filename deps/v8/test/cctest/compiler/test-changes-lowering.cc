// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/compiler/control-builders.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/simplified-node-factory.h"
#include "src/compiler/typer.h"
#include "src/compiler/verifier.h"
#include "src/execution.h"
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
  explicit ChangesLoweringTester(MachineType p0 = kMachineLast)
      : GraphBuilderTester<ReturnType>(p0),
        typer(this->zone()),
        source_positions(this->graph()),
        jsgraph(this->graph(), this->common(), &typer),
        lowering(&jsgraph, &source_positions),
        function(Handle<JSFunction>::null()) {}

  Typer typer;
  SourcePositionTable source_positions;
  JSGraph jsgraph;
  SimplifiedLowering lowering;
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
      StrictMode strict_mode = info.function()->strict_mode();
      info.SetStrictMode(strict_mode);
      info.SetOptimizing(BailoutId::None(), Handle<Code>(function->code()));
      CHECK(Rewriter::Rewrite(&info));
      CHECK(Scope::Analyze(&info));
      CHECK_NE(NULL, info.scope());
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
    this->Store(kMachineFloat64, ptr_node, node);
  }

  Node* LoadInt32(int32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachineWord32, ptr_node);
  }

  Node* LoadUint32(uint32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachineWord32, ptr_node);
  }

  Node* LoadFloat64(double* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(kMachineFloat64, ptr_node);
  }

  void CheckNumber(double expected, Object* number) {
    CHECK(this->isolate()->factory()->NewNumber(expected)->SameValue(number));
  }

  void BuildAndLower(Operator* op) {
    // We build a graph by hand here, because the raw machine assembler
    // does not add the correct control and effect nodes.
    Node* p0 = this->Parameter(0);
    Node* change = this->graph()->NewNode(op, p0);
    Node* ret = this->graph()->NewNode(this->common()->Return(), change,
                                       this->start(), this->start());
    Node* end = this->graph()->NewNode(this->common()->End(), ret);
    this->graph()->SetEnd(end);
    this->lowering.LowerChange(change, this->start(), this->start());
    Verifier::Run(this->graph());
  }

  void BuildStoreAndLower(Operator* op, Operator* store_op, void* location) {
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
    this->lowering.LowerChange(change, this->start(), this->start());
    Verifier::Run(this->graph());
  }

  void BuildLoadAndLower(Operator* op, Operator* load_op, void* location) {
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
    this->lowering.LowerChange(change, this->start(), this->start());
    Verifier::Run(this->graph());
  }

  Factory* factory() { return this->isolate()->factory(); }
  Heap* heap() { return this->isolate()->heap(); }
};


TEST(RunChangeTaggedToInt32) {
  // Build and lower a graph by hand.
  ChangesLoweringTester<int32_t> t(kMachineTagged);
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
  ChangesLoweringTester<uint32_t> t(kMachineTagged);
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
  ChangesLoweringTester<int32_t> t(kMachineTagged);
  double result;

  t.BuildStoreAndLower(t.simplified()->ChangeTaggedToFloat64(),
                       t.machine()->Store(kMachineFloat64, kNoWriteBarrier),
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
  ChangesLoweringTester<int32_t> t(kMachineTagged);
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
  ChangesLoweringTester<Object*> t(kMachineWord32);
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


bool TODO_INT32_TO_TAGGED_WILL_WORK(int32_t v) {
  // TODO(titzer): enable all UI32 -> Tagged checking when inline allocation
  // works.
  return Smi::IsValid(v);
}


bool TODO_UINT32_TO_TAGGED_WILL_WORK(uint32_t v) {
  // TODO(titzer): enable all UI32 -> Tagged checking when inline allocation
  // works.
  return v <= static_cast<uint32_t>(Smi::kMaxValue);
}


TEST(RunChangeInt32ToTagged) {
  ChangesLoweringTester<Object*> t;
  int32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeInt32ToTagged(),
                      t.machine()->Load(kMachineWord32), &input);

  if (Pipeline::SupportedTarget()) {
    FOR_INT32_INPUTS(i) {
      input = *i;
      Object* result = t.CallWithPotentialGC<Object>();
      if (TODO_INT32_TO_TAGGED_WILL_WORK(input)) {
        t.CheckNumber(static_cast<double>(input), result);
      }
    }
  }

  if (Pipeline::SupportedTarget()) {
    FOR_INT32_INPUTS(i) {
      input = *i;
      SimulateFullSpace(CcTest::heap()->new_space());
      Object* result = t.CallWithPotentialGC<Object>();
      if (TODO_INT32_TO_TAGGED_WILL_WORK(input)) {
        t.CheckNumber(static_cast<double>(input), result);
      }
    }
  }
}


TEST(RunChangeUint32ToTagged) {
  ChangesLoweringTester<Object*> t;
  uint32_t input;
  t.BuildLoadAndLower(t.simplified()->ChangeUint32ToTagged(),
                      t.machine()->Load(kMachineWord32), &input);

  if (Pipeline::SupportedTarget()) {
    FOR_UINT32_INPUTS(i) {
      input = *i;
      Object* result = t.CallWithPotentialGC<Object>();
      double expected = static_cast<double>(input);
      if (TODO_UINT32_TO_TAGGED_WILL_WORK(input)) {
        t.CheckNumber(expected, result);
      }
    }
  }

  if (Pipeline::SupportedTarget()) {
    FOR_UINT32_INPUTS(i) {
      input = *i;
      SimulateFullSpace(CcTest::heap()->new_space());
      Object* result = t.CallWithPotentialGC<Object>();
      double expected = static_cast<double>(static_cast<uint32_t>(input));
      if (TODO_UINT32_TO_TAGGED_WILL_WORK(input)) {
        t.CheckNumber(expected, result);
      }
    }
  }
}


// TODO(titzer): lowering of Float64->Tagged needs inline allocation.
#define TODO_FLOAT64_TO_TAGGED false

TEST(RunChangeFloat64ToTagged) {
  ChangesLoweringTester<Object*> t;
  double input;
  t.BuildLoadAndLower(t.simplified()->ChangeFloat64ToTagged(),
                      t.machine()->Load(kMachineFloat64), &input);

  // TODO(titzer): need inline allocation to change float to tagged.
  if (TODO_FLOAT64_TO_TAGGED && Pipeline::SupportedTarget()) {
    FOR_FLOAT64_INPUTS(i) {
      input = *i;
      Object* result = t.CallWithPotentialGC<Object>();
      t.CheckNumber(input, result);
    }
  }

  if (TODO_FLOAT64_TO_TAGGED && Pipeline::SupportedTarget()) {
    FOR_FLOAT64_INPUTS(i) {
      input = *i;
      SimulateFullSpace(CcTest::heap()->new_space());
      Object* result = t.CallWithPotentialGC<Object>();
      t.CheckNumber(input, result);
    }
  }
}
