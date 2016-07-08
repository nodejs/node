// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/ast/scopes.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/select-lowering.h"
#include "src/compiler/simplified-lowering.h"
#include "src/compiler/typer.h"
#include "src/compiler/verifier.h"
#include "src/execution.h"
#include "src/globals.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/function-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename ReturnType>
class ChangesLoweringTester : public GraphBuilderTester<ReturnType> {
 public:
  explicit ChangesLoweringTester(MachineType p0 = MachineType::None())
      : GraphBuilderTester<ReturnType>(p0),
        javascript(this->zone()),
        jsgraph(this->isolate(), this->graph(), this->common(), &javascript,
                nullptr, this->machine()),
        function(Handle<JSFunction>::null()) {}

  JSOperatorBuilder javascript;
  JSGraph jsgraph;
  Handle<JSFunction> function;

  Node* start() { return this->graph()->start(); }

  template <typename T>
  T* CallWithPotentialGC() {
    // TODO(titzer): we wrap the code in a JSFunction here to reuse the
    // JSEntryStub; that could be done with a special prologue or other stub.
    if (function.is_null()) {
      function = FunctionTester::ForMachineGraph(this->graph());
    }
    Handle<Object>* args = NULL;
    MaybeHandle<Object> result =
        Execution::Call(this->isolate(), function, factory()->undefined_value(),
                        0, args, false);
    return T::cast(*result.ToHandleChecked());
  }

  void StoreFloat64(Node* node, double* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    this->Store(MachineType::Float64(), ptr_node, node);
  }

  Node* LoadInt32(int32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(MachineType::Int32(), ptr_node);
  }

  Node* LoadUint32(uint32_t* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(MachineType::Uint32(), ptr_node);
  }

  Node* LoadFloat64(double* ptr) {
    Node* ptr_node = this->PointerConstant(ptr);
    return this->Load(MachineType::Float64(), ptr_node);
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
    Node* end = this->graph()->NewNode(this->common()->End(1), ret);
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
    Node* end = this->graph()->NewNode(this->common()->End(1), ret);
    this->graph()->SetEnd(end);
    LowerChange(change);
  }

  void BuildLoadAndLower(const Operator* op, const Operator* load_op,
                         void* location) {
    // We build a graph by hand here, because the raw machine assembler
    // does not add the correct control and effect nodes.
    Node* load = this->graph()->NewNode(
        load_op, this->PointerConstant(location), this->Int32Constant(0),
        this->start(), this->start());
    Node* change = this->graph()->NewNode(op, load);
    Node* ret = this->graph()->NewNode(this->common()->Return(), change,
                                       this->start(), this->start());
    Node* end = this->graph()->NewNode(this->common()->End(1), ret);
    this->graph()->SetEnd(end);
    LowerChange(change);
  }

  void LowerChange(Node* change) {
    // Run the graph reducer with changes lowering on a single node.
    Typer typer(this->isolate(), this->graph());
    typer.Run();
    ChangeLowering change_lowering(&jsgraph);
    SelectLowering select_lowering(this->graph(), this->common());
    GraphReducer reducer(this->zone(), this->graph());
    reducer.AddReducer(&change_lowering);
    reducer.AddReducer(&select_lowering);
    reducer.ReduceNode(change);
    Verifier::Run(this->graph(), Verifier::UNTYPED);
  }

  Factory* factory() { return this->isolate()->factory(); }
  Heap* heap() { return this->isolate()->heap(); }
};


TEST(RunChangeTaggedToInt32) {
  // Build and lower a graph by hand.
  ChangesLoweringTester<int32_t> t(MachineType::AnyTagged());
  t.BuildAndLower(t.simplified()->ChangeTaggedToInt32());

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


TEST(RunChangeTaggedToUint32) {
  // Build and lower a graph by hand.
  ChangesLoweringTester<uint32_t> t(MachineType::AnyTagged());
  t.BuildAndLower(t.simplified()->ChangeTaggedToUint32());

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


TEST(RunChangeTaggedToFloat64) {
  ChangesLoweringTester<int32_t> t(MachineType::AnyTagged());
  double result;

  t.BuildStoreAndLower(t.simplified()->ChangeTaggedToFloat64(),
                       t.machine()->Store(StoreRepresentation(
                           MachineRepresentation::kFloat64, kNoWriteBarrier)),
                       &result);

  {
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

  {
    FOR_FLOAT64_INPUTS(i) {
      double input = *i;
      {
        Handle<Object> number = t.factory()->NewNumber(input);
        t.Call(*number);
        CHECK_DOUBLE_EQ(input, result);
      }

      {
        Handle<HeapNumber> number = t.factory()->NewHeapNumber(input);
        t.Call(*number);
        CHECK_DOUBLE_EQ(input, result);
      }
    }
  }
}


TEST(RunChangeBoolToBit) {
  ChangesLoweringTester<int32_t> t(MachineType::AnyTagged());
  t.BuildAndLower(t.simplified()->ChangeBoolToBit());

  {
    Object* true_obj = t.heap()->true_value();
    int32_t result = t.Call(true_obj);
    CHECK_EQ(1, result);
  }

  {
    Object* false_obj = t.heap()->false_value();
    int32_t result = t.Call(false_obj);
    CHECK_EQ(0, result);
  }
}


TEST(RunChangeBitToBool) {
  ChangesLoweringTester<Object*> t(MachineType::Int32());
  t.BuildAndLower(t.simplified()->ChangeBitToBool());

  {
    Object* result = t.Call(1);
    Object* true_obj = t.heap()->true_value();
    CHECK_EQ(true_obj, result);
  }

  {
    Object* result = t.Call(0);
    Object* false_obj = t.heap()->false_value();
    CHECK_EQ(false_obj, result);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
