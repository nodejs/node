// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/pipeline.h"
#include "src/objects-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/optimized-compilation-info.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class StubTester {
 public:
  StubTester(Zone* zone, CodeStub* stub)
      : zone_(zone),
        info_(ArrayVector("test"), zone, Code::STUB),
        interface_descriptor_(stub->GetCallInterfaceDescriptor()),
        descriptor_(Linkage::GetStubCallDescriptor(
            zone, interface_descriptor_, stub->GetStackParameterCount(),
            CallDescriptor::kNoFlags, Operator::kNoProperties)),
        graph_(zone_),
        common_(zone_),
        tester_(InitializeFunctionTester(stub->GetCode()),
                GetParameterCountWithContext()) {}

  StubTester(Isolate* isolate, Zone* zone, Builtins::Name name)
      : zone_(zone),
        info_(ArrayVector("test"), zone, Code::STUB),
        interface_descriptor_(
            Builtins::CallableFor(isolate, name).descriptor()),
        descriptor_(Linkage::GetStubCallDescriptor(
            zone, interface_descriptor_,
            interface_descriptor_.GetStackParameterCount(),
            CallDescriptor::kNoFlags, Operator::kNoProperties)),
        graph_(zone_),
        common_(zone_),
        tester_(InitializeFunctionTester(
                    Handle<Code>(isolate->builtins()->builtin(name), isolate)),
                GetParameterCountWithContext()) {}

  template <typename... Args>
  Handle<Object> Call(Args... args) {
    DCHECK_EQ(interface_descriptor_.GetParameterCount(), sizeof...(args));
    MaybeHandle<Object> result =
        tester_
            .Call(args...,
                  Handle<HeapObject>(tester_.function->context(), ft().isolate))
            .ToHandleChecked();
    return result.ToHandleChecked();
  }

  FunctionTester& ft() { return tester_; }

 private:
  Graph* InitializeFunctionTester(Handle<Code> stub) {
    // Add target, effect and control.
    int node_count = GetParameterCountWithContext() + 3;
    // Add extra inputs for the JSFunction parameter and the receiver (which for
    // the tester is always undefined) to the start node.
    Node* start =
        graph_.NewNode(common_.Start(GetParameterCountWithContext() + 2));
    Node** node_array = zone_->NewArray<Node*>(node_count);
    node_array[0] = graph_.NewNode(common_.HeapConstant(stub));
    for (int i = 0; i < GetParameterCountWithContext(); ++i) {
      CHECK(IsAnyTagged(descriptor_->GetParameterType(i).representation()));
      node_array[i + 1] = graph_.NewNode(common_.Parameter(i + 1), start);
    }
    node_array[node_count - 2] = start;
    node_array[node_count - 1] = start;
    Node* call =
        graph_.NewNode(common_.Call(descriptor_), node_count, &node_array[0]);

    Node* zero = graph_.NewNode(common_.Int32Constant(0));
    Node* ret = graph_.NewNode(common_.Return(), zero, call, call, start);
    Node* end = graph_.NewNode(common_.End(1), ret);
    graph_.SetStart(start);
    graph_.SetEnd(end);
    return &graph_;
  }

  int GetParameterCountWithContext() {
    return interface_descriptor_.GetParameterCount() + 1;
  }

  Zone* zone_;
  OptimizedCompilationInfo info_;
  CallInterfaceDescriptor interface_descriptor_;
  CallDescriptor* descriptor_;
  Graph graph_;
  CommonOperatorBuilder common_;
  FunctionTester tester_;
};

TEST(RunStringWrapperLengthStub) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kLoadIC_StringWrapperLength);

  // Actuall call through to the stub, verifying its result.
  const char* testString = "Und das Lamm schrie HURZ!";
  Handle<Object> receiverArg =
      Object::ToObject(isolate, tester.ft().Val(testString)).ToHandleChecked();
  Handle<Object> nameArg = tester.ft().Val("length");
  Handle<Object> slot = tester.ft().Val(0.0);
  Handle<Object> vector = tester.ft().Val(0.0);
  Handle<Object> result = tester.Call(receiverArg, nameArg, slot, vector);
  CHECK_EQ(static_cast<int>(strlen(testString)), Smi::ToInt(*result));
}

TEST(RunArrayExtractStubSimple) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kExtractFastJSArray);

  // Actuall call through to the stub, verifying its result.
  Handle<JSArray> source_array = isolate->factory()->NewJSArray(
      PACKED_ELEMENTS, 5, 10, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
  static_cast<FixedArray*>(source_array->elements())->set(0, Smi::FromInt(5));
  static_cast<FixedArray*>(source_array->elements())->set(1, Smi::FromInt(4));
  static_cast<FixedArray*>(source_array->elements())->set(2, Smi::FromInt(3));
  static_cast<FixedArray*>(source_array->elements())->set(3, Smi::FromInt(2));
  static_cast<FixedArray*>(source_array->elements())->set(4, Smi::FromInt(1));
  Handle<JSArray> result = Handle<JSArray>::cast(
      tester.Call(source_array, Handle<Smi>(Smi::FromInt(0), isolate),
                  Handle<Smi>(Smi::FromInt(5), isolate)));
  CHECK_NE(*source_array, *result);
  CHECK_EQ(result->GetElementsKind(), PACKED_ELEMENTS);
  CHECK_EQ(static_cast<FixedArray*>(result->elements())->get(0),
           Smi::FromInt(5));
  CHECK_EQ(static_cast<FixedArray*>(result->elements())->get(1),
           Smi::FromInt(4));
  CHECK_EQ(static_cast<FixedArray*>(result->elements())->get(2),
           Smi::FromInt(3));
  CHECK_EQ(static_cast<FixedArray*>(result->elements())->get(3),
           Smi::FromInt(2));
  CHECK_EQ(static_cast<FixedArray*>(result->elements())->get(4),
           Smi::FromInt(1));
}

TEST(RunArrayExtractDoubleStubSimple) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kExtractFastJSArray);

  // Actuall call through to the stub, verifying its result.
  Handle<JSArray> source_array = isolate->factory()->NewJSArray(
      PACKED_DOUBLE_ELEMENTS, 5, 10, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
  static_cast<FixedDoubleArray*>(source_array->elements())->set(0, 5);
  static_cast<FixedDoubleArray*>(source_array->elements())->set(1, 4);
  static_cast<FixedDoubleArray*>(source_array->elements())->set(2, 3);
  static_cast<FixedDoubleArray*>(source_array->elements())->set(3, 2);
  static_cast<FixedDoubleArray*>(source_array->elements())->set(4, 1);
  Handle<JSArray> result = Handle<JSArray>::cast(
      tester.Call(source_array, Handle<Smi>(Smi::FromInt(0), isolate),
                  Handle<Smi>(Smi::FromInt(5), isolate)));
  CHECK_NE(*source_array, *result);
  CHECK_EQ(result->GetElementsKind(), PACKED_DOUBLE_ELEMENTS);
  CHECK_EQ(static_cast<FixedDoubleArray*>(result->elements())->get_scalar(0),
           5);
  CHECK_EQ(static_cast<FixedDoubleArray*>(result->elements())->get_scalar(1),
           4);
  CHECK_EQ(static_cast<FixedDoubleArray*>(result->elements())->get_scalar(2),
           3);
  CHECK_EQ(static_cast<FixedDoubleArray*>(result->elements())->get_scalar(3),
           2);
  CHECK_EQ(static_cast<FixedDoubleArray*>(result->elements())->get_scalar(4),
           1);
}

TEST(RunArrayExtractStubTooBigForNewSpace) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kExtractFastJSArray);

  // Actuall call through to the stub, verifying its result.
  Handle<JSArray> source_array = isolate->factory()->NewJSArray(
      PACKED_ELEMENTS, 500000, 500000, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
  for (int i = 0; i < 500000; ++i) {
    static_cast<FixedArray*>(source_array->elements())->set(i, Smi::FromInt(i));
  }
  Handle<JSArray> result = Handle<JSArray>::cast(
      tester.Call(source_array, Handle<Smi>(Smi::FromInt(0), isolate),
                  Handle<Smi>(Smi::FromInt(500000), isolate)));
  CHECK_NE(*source_array, *result);
  CHECK_EQ(result->GetElementsKind(), PACKED_ELEMENTS);
  for (int i = 0; i < 500000; ++i) {
    CHECK_EQ(static_cast<FixedArray*>(source_array->elements())->get(i),
             static_cast<FixedArray*>(result->elements())->get(i));
  }
}

TEST(RunArrayExtractDoubleStubTooBigForNewSpace) {
  HandleAndZoneScope scope;
  Isolate* isolate = scope.main_isolate();
  Zone* zone = scope.main_zone();

  StubTester tester(isolate, zone, Builtins::kExtractFastJSArray);

  // Actuall call through to the stub, verifying its result.
  Handle<JSArray> source_array = isolate->factory()->NewJSArray(
      PACKED_DOUBLE_ELEMENTS, 500000, 500000,
      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, TENURED);
  for (int i = 0; i < 500000; ++i) {
    static_cast<FixedDoubleArray*>(source_array->elements())->set(i, i);
  }
  Handle<JSArray> result = Handle<JSArray>::cast(
      tester.Call(source_array, Handle<Smi>(Smi::FromInt(0), isolate),
                  Handle<Smi>(Smi::FromInt(500000), isolate)));
  CHECK_NE(*source_array, *result);
  CHECK_EQ(result->GetElementsKind(), PACKED_DOUBLE_ELEMENTS);
  for (int i = 0; i < 500000; ++i) {
    CHECK_EQ(
        static_cast<FixedDoubleArray*>(source_array->elements())->get_scalar(i),
        static_cast<FixedDoubleArray*>(result->elements())->get_scalar(i));
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
