// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/compiler/access-builder.h"
#include "src/compiler/change-lowering.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/representation-change.h"
#include "src/compiler/simplified-lowering.h"
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
class SimplifiedLoweringTester : public GraphBuilderTester<ReturnType> {
 public:
  SimplifiedLoweringTester(MachineType p0 = kMachNone,
                           MachineType p1 = kMachNone,
                           MachineType p2 = kMachNone,
                           MachineType p3 = kMachNone,
                           MachineType p4 = kMachNone)
      : GraphBuilderTester<ReturnType>(p0, p1, p2, p3, p4),
        typer(this->graph(), MaybeHandle<Context>()),
        javascript(this->zone()),
        jsgraph(this->graph(), this->common(), &javascript, this->machine()),
        lowering(&jsgraph) {}

  Typer typer;
  JSOperatorBuilder javascript;
  JSGraph jsgraph;
  SimplifiedLowering lowering;

  void LowerAllNodes() {
    this->End();
    typer.Run();
    lowering.LowerAllNodes();
  }

  void LowerAllNodesAndLowerChanges() {
    this->End();
    typer.Run();
    lowering.LowerAllNodes();

    Zone* zone = this->zone();
    CompilationInfo info(zone->isolate(), zone);
    Linkage linkage(
        zone, Linkage::GetSimplifiedCDescriptor(zone, this->machine_sig_));
    ChangeLowering lowering(&jsgraph, &linkage);
    GraphReducer reducer(this->graph());
    reducer.AddReducer(&lowering);
    reducer.ReduceGraph();
    Verifier::Run(this->graph());
  }

  void CheckNumberCall(double expected, double input) {
    // TODO(titzer): make calls to NewNumber work in cctests.
    if (expected <= Smi::kMinValue) return;
    if (expected >= Smi::kMaxValue) return;
    Handle<Object> num = factory()->NewNumber(input);
    Object* result = this->Call(*num);
    CHECK(factory()->NewNumber(expected)->SameValue(result));
  }

  Factory* factory() { return this->isolate()->factory(); }
  Heap* heap() { return this->isolate()->heap(); }
};


// TODO(titzer): factor these tests out to test-run-simplifiedops.cc.
// TODO(titzer): test tagged representation for input to NumberToInt32.
TEST(RunNumberToInt32_float64) {
  // TODO(titzer): explicit load/stores here are only because of representations
  double input;
  int32_t result;
  SimplifiedLoweringTester<Object*> t;
  FieldAccess load = {kUntaggedBase, 0, Handle<Name>(), Type::Number(),
                      kMachFloat64};
  Node* loaded = t.LoadField(load, t.PointerConstant(&input));
  NodeProperties::SetBounds(loaded, Bounds(Type::Number()));
  Node* convert = t.NumberToInt32(loaded);
  FieldAccess store = {kUntaggedBase, 0, Handle<Name>(), Type::Signed32(),
                       kMachInt32};
  t.StoreField(store, t.PointerConstant(&result), convert);
  t.Return(t.jsgraph.TrueConstant());
  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    FOR_FLOAT64_INPUTS(i) {
      input = *i;
      int32_t expected = DoubleToInt32(*i);
      t.Call();
      CHECK_EQ(expected, result);
    }
  }
}


// TODO(titzer): test tagged representation for input to NumberToUint32.
TEST(RunNumberToUint32_float64) {
  // TODO(titzer): explicit load/stores here are only because of representations
  double input;
  uint32_t result;
  SimplifiedLoweringTester<Object*> t;
  FieldAccess load = {kUntaggedBase, 0, Handle<Name>(), Type::Number(),
                      kMachFloat64};
  Node* loaded = t.LoadField(load, t.PointerConstant(&input));
  NodeProperties::SetBounds(loaded, Bounds(Type::Number()));
  Node* convert = t.NumberToUint32(loaded);
  FieldAccess store = {kUntaggedBase, 0, Handle<Name>(), Type::Unsigned32(),
                       kMachUint32};
  t.StoreField(store, t.PointerConstant(&result), convert);
  t.Return(t.jsgraph.TrueConstant());
  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    FOR_FLOAT64_INPUTS(i) {
      input = *i;
      uint32_t expected = DoubleToUint32(*i);
      t.Call();
      CHECK_EQ(static_cast<int32_t>(expected), static_cast<int32_t>(result));
    }
  }
}


// Create a simple JSObject with a unique map.
static Handle<JSObject> TestObject() {
  static int index = 0;
  char buffer[50];
  v8::base::OS::SNPrintF(buffer, 50, "({'a_%d':1})", index++);
  return Handle<JSObject>::cast(v8::Utils::OpenHandle(*CompileRun(buffer)));
}


TEST(RunLoadMap) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  FieldAccess access = AccessBuilder::ForMap();
  Node* load = t.LoadField(access, t.Parameter(0));
  t.Return(load);

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<JSObject> src = TestObject();
    Handle<Map> src_map(src->map());
    Object* result = t.Call(*src);  // TODO(titzer): raw pointers in call
    CHECK_EQ(*src_map, result);
  }
}


TEST(RunStoreMap) {
  SimplifiedLoweringTester<int32_t> t(kMachAnyTagged, kMachAnyTagged);
  FieldAccess access = AccessBuilder::ForMap();
  t.StoreField(access, t.Parameter(1), t.Parameter(0));
  t.Return(t.jsgraph.TrueConstant());

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<JSObject> src = TestObject();
    Handle<Map> src_map(src->map());
    Handle<JSObject> dst = TestObject();
    CHECK(src->map() != dst->map());
    t.Call(*src_map, *dst);  // TODO(titzer): raw pointers in call
    CHECK(*src_map == dst->map());
  }
}


TEST(RunLoadProperties) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  FieldAccess access = AccessBuilder::ForJSObjectProperties();
  Node* load = t.LoadField(access, t.Parameter(0));
  t.Return(load);

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<JSObject> src = TestObject();
    Handle<FixedArray> src_props(src->properties());
    Object* result = t.Call(*src);  // TODO(titzer): raw pointers in call
    CHECK_EQ(*src_props, result);
  }
}


TEST(RunLoadStoreMap) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged, kMachAnyTagged);
  FieldAccess access = AccessBuilder::ForMap();
  Node* load = t.LoadField(access, t.Parameter(0));
  t.StoreField(access, t.Parameter(1), load);
  t.Return(load);

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<JSObject> src = TestObject();
    Handle<Map> src_map(src->map());
    Handle<JSObject> dst = TestObject();
    CHECK(src->map() != dst->map());
    Object* result = t.Call(*src, *dst);  // TODO(titzer): raw pointers in call
    CHECK(result->IsMap());
    CHECK_EQ(*src_map, result);
    CHECK(*src_map == dst->map());
  }
}


TEST(RunLoadStoreFixedArrayIndex) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  ElementAccess access = AccessBuilder::ForFixedArrayElement();
  Node* load = t.LoadElement(access, t.Parameter(0), t.Int32Constant(0),
                             t.Int32Constant(2));
  t.StoreElement(access, t.Parameter(0), t.Int32Constant(1), t.Int32Constant(2),
                 load);
  t.Return(load);

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<FixedArray> array = t.factory()->NewFixedArray(2);
    Handle<JSObject> src = TestObject();
    Handle<JSObject> dst = TestObject();
    array->set(0, *src);
    array->set(1, *dst);
    Object* result = t.Call(*array);
    CHECK_EQ(*src, result);
    CHECK_EQ(*src, array->get(0));
    CHECK_EQ(*src, array->get(1));
  }
}


TEST(RunLoadStoreArrayBuffer) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  const int index = 12;
  const int array_length = 2 * index;
  ElementAccess buffer_access =
      AccessBuilder::ForTypedArrayElement(v8::kExternalInt8Array, true);
  Node* backing_store = t.LoadField(
      AccessBuilder::ForJSArrayBufferBackingStore(), t.Parameter(0));
  Node* load =
      t.LoadElement(buffer_access, backing_store, t.Int32Constant(index),
                    t.Int32Constant(array_length));
  t.StoreElement(buffer_access, backing_store, t.Int32Constant(index + 1),
                 t.Int32Constant(array_length), load);
  t.Return(t.jsgraph.TrueConstant());

  t.LowerAllNodes();
  t.GenerateCode();

  if (Pipeline::SupportedTarget()) {
    Handle<JSArrayBuffer> array = t.factory()->NewJSArrayBuffer();
    Runtime::SetupArrayBufferAllocatingData(t.isolate(), array, array_length);
    uint8_t* data = reinterpret_cast<uint8_t*>(array->backing_store());
    for (int i = 0; i < array_length; i++) {
      data[i] = i;
    }

    // TODO(titzer): raw pointers in call
    Object* result = t.Call(*array);
    CHECK_EQ(t.isolate()->heap()->true_value(), result);
    for (int i = 0; i < array_length; i++) {
      uint8_t expected = i;
      if (i == (index + 1)) expected = index;
      CHECK_EQ(data[i], expected);
    }
  }
}


TEST(RunLoadFieldFromUntaggedBase) {
  Smi* smis[] = {Smi::FromInt(1), Smi::FromInt(2), Smi::FromInt(3)};

  for (size_t i = 0; i < arraysize(smis); i++) {
    int offset = static_cast<int>(i * sizeof(Smi*));
    FieldAccess access = {kUntaggedBase, offset, Handle<Name>(),
                          Type::Integral32(), kMachAnyTagged};

    SimplifiedLoweringTester<Object*> t;
    Node* load = t.LoadField(access, t.PointerConstant(smis));
    t.Return(load);
    t.LowerAllNodes();

    if (!Pipeline::SupportedTarget()) continue;

    for (int j = -5; j <= 5; j++) {
      Smi* expected = Smi::FromInt(j);
      smis[i] = expected;
      CHECK_EQ(expected, t.Call());
    }
  }
}


TEST(RunStoreFieldToUntaggedBase) {
  Smi* smis[] = {Smi::FromInt(1), Smi::FromInt(2), Smi::FromInt(3)};

  for (size_t i = 0; i < arraysize(smis); i++) {
    int offset = static_cast<int>(i * sizeof(Smi*));
    FieldAccess access = {kUntaggedBase, offset, Handle<Name>(),
                          Type::Integral32(), kMachAnyTagged};

    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* p0 = t.Parameter(0);
    t.StoreField(access, t.PointerConstant(smis), p0);
    t.Return(p0);
    t.LowerAllNodes();

    if (!Pipeline::SupportedTarget()) continue;

    for (int j = -5; j <= 5; j++) {
      Smi* expected = Smi::FromInt(j);
      smis[i] = Smi::FromInt(-100);
      CHECK_EQ(expected, t.Call(expected));
      CHECK_EQ(expected, smis[i]);
    }
  }
}


TEST(RunLoadElementFromUntaggedBase) {
  Smi* smis[] = {Smi::FromInt(1), Smi::FromInt(2), Smi::FromInt(3),
                 Smi::FromInt(4), Smi::FromInt(5)};

  for (size_t i = 0; i < arraysize(smis); i++) {    // for header sizes
    for (size_t j = 0; (i + j) < arraysize(smis); j++) {  // for element index
      int offset = static_cast<int>(i * sizeof(Smi*));
      ElementAccess access = {kNoBoundsCheck, kUntaggedBase, offset,
                              Type::Integral32(), kMachAnyTagged};

      SimplifiedLoweringTester<Object*> t;
      Node* load = t.LoadElement(
          access, t.PointerConstant(smis), t.Int32Constant(static_cast<int>(j)),
          t.Int32Constant(static_cast<int>(arraysize(smis))));
      t.Return(load);
      t.LowerAllNodes();

      if (!Pipeline::SupportedTarget()) continue;

      for (int k = -5; k <= 5; k++) {
        Smi* expected = Smi::FromInt(k);
        smis[i + j] = expected;
        CHECK_EQ(expected, t.Call());
      }
    }
  }
}


TEST(RunStoreElementFromUntaggedBase) {
  Smi* smis[] = {Smi::FromInt(1), Smi::FromInt(2), Smi::FromInt(3),
                 Smi::FromInt(4), Smi::FromInt(5)};

  for (size_t i = 0; i < arraysize(smis); i++) {    // for header sizes
    for (size_t j = 0; (i + j) < arraysize(smis); j++) {  // for element index
      int offset = static_cast<int>(i * sizeof(Smi*));
      ElementAccess access = {kNoBoundsCheck, kUntaggedBase, offset,
                              Type::Integral32(), kMachAnyTagged};

      SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
      Node* p0 = t.Parameter(0);
      t.StoreElement(access, t.PointerConstant(smis),
                     t.Int32Constant(static_cast<int>(j)),
                     t.Int32Constant(static_cast<int>(arraysize(smis))), p0);
      t.Return(p0);
      t.LowerAllNodes();

      if (!Pipeline::SupportedTarget()) continue;

      for (int k = -5; k <= 5; k++) {
        Smi* expected = Smi::FromInt(k);
        smis[i + j] = Smi::FromInt(-100);
        CHECK_EQ(expected, t.Call(expected));
        CHECK_EQ(expected, smis[i + j]);
      }

      // TODO(titzer): assert the contents of the array.
    }
  }
}


// A helper class for accessing fields and elements of various types, on both
// tagged and untagged base pointers. Contains both tagged and untagged buffers
// for testing direct memory access from generated code.
template <typename E>
class AccessTester : public HandleAndZoneScope {
 public:
  bool tagged;
  MachineType rep;
  E* original_elements;
  size_t num_elements;
  E* untagged_array;
  Handle<ByteArray> tagged_array;  // TODO(titzer): use FixedArray for tagged.

  AccessTester(bool t, MachineType r, E* orig, size_t num)
      : tagged(t),
        rep(r),
        original_elements(orig),
        num_elements(num),
        untagged_array(static_cast<E*>(malloc(ByteSize()))),
        tagged_array(main_isolate()->factory()->NewByteArray(
            static_cast<int>(ByteSize()))) {
    Reinitialize();
  }

  ~AccessTester() { free(untagged_array); }

  size_t ByteSize() { return num_elements * sizeof(E); }

  // Nuke both {untagged_array} and {tagged_array} with {original_elements}.
  void Reinitialize() {
    memcpy(untagged_array, original_elements, ByteSize());
    CHECK_EQ(static_cast<int>(ByteSize()), tagged_array->length());
    E* raw = reinterpret_cast<E*>(tagged_array->GetDataStartAddress());
    memcpy(raw, original_elements, ByteSize());
  }

  // Create and run code that copies the element in either {untagged_array}
  // or {tagged_array} at index {from_index} to index {to_index}.
  void RunCopyElement(int from_index, int to_index) {
    // TODO(titzer): test element and field accesses where the base is not
    // a constant in the code.
    BoundsCheck(from_index);
    BoundsCheck(to_index);
    ElementAccess access = GetElementAccess();

    SimplifiedLoweringTester<Object*> t;
    Node* ptr = GetBaseNode(&t);
    Node* load = t.LoadElement(access, ptr, t.Int32Constant(from_index),
                               t.Int32Constant(static_cast<int>(num_elements)));
    t.StoreElement(access, ptr, t.Int32Constant(to_index),
                   t.Int32Constant(static_cast<int>(num_elements)), load);
    t.Return(t.jsgraph.TrueConstant());
    t.LowerAllNodes();
    t.GenerateCode();

    if (Pipeline::SupportedTarget()) {
      Object* result = t.Call();
      CHECK_EQ(t.isolate()->heap()->true_value(), result);
    }
  }

  // Create and run code that copies the field in either {untagged_array}
  // or {tagged_array} at index {from_index} to index {to_index}.
  void RunCopyField(int from_index, int to_index) {
    BoundsCheck(from_index);
    BoundsCheck(to_index);
    FieldAccess from_access = GetFieldAccess(from_index);
    FieldAccess to_access = GetFieldAccess(to_index);

    SimplifiedLoweringTester<Object*> t;
    Node* ptr = GetBaseNode(&t);
    Node* load = t.LoadField(from_access, ptr);
    t.StoreField(to_access, ptr, load);
    t.Return(t.jsgraph.TrueConstant());
    t.LowerAllNodes();
    t.GenerateCode();

    if (Pipeline::SupportedTarget()) {
      Object* result = t.Call();
      CHECK_EQ(t.isolate()->heap()->true_value(), result);
    }
  }

  // Create and run code that copies the elements from {this} to {that}.
  void RunCopyElements(AccessTester<E>* that) {
// TODO(titzer): Rewrite this test without StructuredGraphBuilder support.
#if 0
    SimplifiedLoweringTester<Object*> t;

    Node* one = t.Int32Constant(1);
    Node* index = t.Int32Constant(0);
    Node* limit = t.Int32Constant(static_cast<int>(num_elements));
    t.environment()->Push(index);
    Node* src = this->GetBaseNode(&t);
    Node* dst = that->GetBaseNode(&t);
    {
      LoopBuilder loop(&t);
      loop.BeginLoop();
      // Loop exit condition
      index = t.environment()->Top();
      Node* condition = t.Int32LessThan(index, limit);
      loop.BreakUnless(condition);
      // dst[index] = src[index]
      index = t.environment()->Pop();
      Node* load = t.LoadElement(this->GetElementAccess(), src, index);
      t.StoreElement(that->GetElementAccess(), dst, index, load);
      // index++
      index = t.Int32Add(index, one);
      t.environment()->Push(index);
      // continue
      loop.EndBody();
      loop.EndLoop();
    }
    index = t.environment()->Pop();
    t.Return(t.jsgraph.TrueConstant());
    t.LowerAllNodes();
    t.GenerateCode();

    if (Pipeline::SupportedTarget()) {
      Object* result = t.Call();
      CHECK_EQ(t.isolate()->heap()->true_value(), result);
    }
#endif
  }

  E GetElement(int index) {
    BoundsCheck(index);
    if (tagged) {
      E* raw = reinterpret_cast<E*>(tagged_array->GetDataStartAddress());
      return raw[index];
    } else {
      return untagged_array[index];
    }
  }

 private:
  ElementAccess GetElementAccess() {
    ElementAccess access = {
        kNoBoundsCheck, tagged ? kTaggedBase : kUntaggedBase,
        tagged ? FixedArrayBase::kHeaderSize : 0, Type::Any(), rep};
    return access;
  }

  FieldAccess GetFieldAccess(int field) {
    int offset = field * sizeof(E);
    FieldAccess access = {tagged ? kTaggedBase : kUntaggedBase,
                          offset + (tagged ? FixedArrayBase::kHeaderSize : 0),
                          Handle<Name>(), Type::Any(), rep};
    return access;
  }

  template <typename T>
  Node* GetBaseNode(SimplifiedLoweringTester<T>* t) {
    return tagged ? t->HeapConstant(tagged_array)
                  : t->PointerConstant(untagged_array);
  }

  void BoundsCheck(int index) {
    CHECK_GE(index, 0);
    CHECK_LT(index, static_cast<int>(num_elements));
    CHECK_EQ(static_cast<int>(ByteSize()), tagged_array->length());
  }
};


template <typename E>
static void RunAccessTest(MachineType rep, E* original_elements, size_t num) {
  int num_elements = static_cast<int>(num);

  for (int taggedness = 0; taggedness < 2; taggedness++) {
    AccessTester<E> a(taggedness == 1, rep, original_elements, num);
    for (int field = 0; field < 2; field++) {
      for (int i = 0; i < num_elements - 1; i++) {
        a.Reinitialize();
        if (field == 0) {
          a.RunCopyField(i, i + 1);  // Test field read/write.
        } else {
          a.RunCopyElement(i, i + 1);  // Test element read/write.
        }
        if (Pipeline::SupportedTarget()) {  // verify.
          for (int j = 0; j < num_elements; j++) {
            E expect =
                j == (i + 1) ? original_elements[i] : original_elements[j];
            CHECK_EQ(expect, a.GetElement(j));
          }
        }
      }
    }
  }
  // Test array copy.
  for (int tf = 0; tf < 2; tf++) {
    for (int tt = 0; tt < 2; tt++) {
      AccessTester<E> a(tf == 1, rep, original_elements, num);
      AccessTester<E> b(tt == 1, rep, original_elements, num);
      a.RunCopyElements(&b);
      if (Pipeline::SupportedTarget()) {  // verify.
        for (int i = 0; i < num_elements; i++) {
          CHECK_EQ(a.GetElement(i), b.GetElement(i));
        }
      }
    }
  }
}


TEST(RunAccessTests_uint8) {
  uint8_t data[] = {0x07, 0x16, 0x25, 0x34, 0x43, 0x99,
                    0xab, 0x78, 0x89, 0x19, 0x2b, 0x38};
  RunAccessTest<uint8_t>(kMachInt8, data, arraysize(data));
}


TEST(RunAccessTests_uint16) {
  uint16_t data[] = {0x071a, 0x162b, 0x253c, 0x344d, 0x435e, 0x7777};
  RunAccessTest<uint16_t>(kMachInt16, data, arraysize(data));
}


TEST(RunAccessTests_int32) {
  int32_t data[] = {-211, 211, 628347, 2000000000, -2000000000, -1, -100000034};
  RunAccessTest<int32_t>(kMachInt32, data, arraysize(data));
}


#define V8_2PART_INT64(a, b) (((static_cast<int64_t>(a) << 32) + 0x##b##u))


TEST(RunAccessTests_int64) {
  if (kPointerSize != 8) return;
  int64_t data[] = {V8_2PART_INT64(0x10111213, 14151617),
                    V8_2PART_INT64(0x20212223, 24252627),
                    V8_2PART_INT64(0x30313233, 34353637),
                    V8_2PART_INT64(0xa0a1a2a3, a4a5a6a7),
                    V8_2PART_INT64(0xf0f1f2f3, f4f5f6f7)};
  RunAccessTest<int64_t>(kMachInt64, data, arraysize(data));
}


TEST(RunAccessTests_float64) {
  double data[] = {1.25, -1.25, 2.75, 11.0, 11100.8};
  RunAccessTest<double>(kMachFloat64, data, arraysize(data));
}


TEST(RunAccessTests_Smi) {
  Smi* data[] = {Smi::FromInt(-1),    Smi::FromInt(-9),
                 Smi::FromInt(0),     Smi::FromInt(666),
                 Smi::FromInt(77777), Smi::FromInt(Smi::kMaxValue)};
  RunAccessTest<Smi*>(kMachAnyTagged, data, arraysize(data));
}


// Fills in most of the nodes of the graph in order to make tests shorter.
class TestingGraph : public HandleAndZoneScope, public GraphAndBuilders {
 public:
  Typer typer;
  JSOperatorBuilder javascript;
  JSGraph jsgraph;
  Node* p0;
  Node* p1;
  Node* p2;
  Node* start;
  Node* end;
  Node* ret;

  explicit TestingGraph(Type* p0_type, Type* p1_type = Type::None(),
                        Type* p2_type = Type::None())
      : GraphAndBuilders(main_zone()),
        typer(graph(), MaybeHandle<Context>()),
        javascript(main_zone()),
        jsgraph(graph(), common(), &javascript, machine()) {
    start = graph()->NewNode(common()->Start(2));
    graph()->SetStart(start);
    ret =
        graph()->NewNode(common()->Return(), jsgraph.Constant(0), start, start);
    end = graph()->NewNode(common()->End(), ret);
    graph()->SetEnd(end);
    p0 = graph()->NewNode(common()->Parameter(0), start);
    p1 = graph()->NewNode(common()->Parameter(1), start);
    p2 = graph()->NewNode(common()->Parameter(2), start);
    typer.Run();
    NodeProperties::SetBounds(p0, Bounds(p0_type));
    NodeProperties::SetBounds(p1, Bounds(p1_type));
    NodeProperties::SetBounds(p2, Bounds(p2_type));
  }

  void CheckLoweringBinop(IrOpcode::Value expected, const Operator* op) {
    Node* node = Return(graph()->NewNode(op, p0, p1));
    Lower();
    CHECK_EQ(expected, node->opcode());
  }

  void CheckLoweringTruncatedBinop(IrOpcode::Value expected, const Operator* op,
                                   const Operator* trunc) {
    Node* node = graph()->NewNode(op, p0, p1);
    Return(graph()->NewNode(trunc, node));
    Lower();
    CHECK_EQ(expected, node->opcode());
  }

  void Lower() {
    SimplifiedLowering(&jsgraph).LowerAllNodes();
  }

  // Inserts the node as the return value of the graph.
  Node* Return(Node* node) {
    ret->ReplaceInput(0, node);
    return node;
  }

  // Inserts the node as the effect input to the return of the graph.
  void Effect(Node* node) { ret->ReplaceInput(1, node); }

  Node* ExampleWithOutput(MachineType type) {
    // TODO(titzer): use parameters with guaranteed representations.
    if (type & kTypeInt32) {
      return graph()->NewNode(machine()->Int32Add(), jsgraph.Int32Constant(1),
                              jsgraph.Int32Constant(1));
    } else if (type & kTypeUint32) {
      return graph()->NewNode(machine()->Word32Shr(), jsgraph.Int32Constant(1),
                              jsgraph.Int32Constant(1));
    } else if (type & kRepFloat64) {
      return graph()->NewNode(machine()->Float64Add(),
                              jsgraph.Float64Constant(1),
                              jsgraph.Float64Constant(1));
    } else if (type & kRepBit) {
      return graph()->NewNode(machine()->Word32Equal(),
                              jsgraph.Int32Constant(1),
                              jsgraph.Int32Constant(1));
    } else if (type & kRepWord64) {
      return graph()->NewNode(machine()->Int64Add(), Int64Constant(1),
                              Int64Constant(1));
    } else {
      CHECK(type & kRepTagged);
      return p0;
    }
  }

  Node* Use(Node* node, MachineType type) {
    if (type & kTypeInt32) {
      return graph()->NewNode(machine()->Int32LessThan(), node,
                              jsgraph.Int32Constant(1));
    } else if (type & kTypeUint32) {
      return graph()->NewNode(machine()->Uint32LessThan(), node,
                              jsgraph.Int32Constant(1));
    } else if (type & kRepFloat64) {
      return graph()->NewNode(machine()->Float64Add(), node,
                              jsgraph.Float64Constant(1));
    } else if (type & kRepWord64) {
      return graph()->NewNode(machine()->Int64LessThan(), node,
                              Int64Constant(1));
    } else {
      return graph()->NewNode(simplified()->ReferenceEqual(Type::Any()), node,
                              jsgraph.TrueConstant());
    }
  }

  Node* Branch(Node* cond) {
    Node* br = graph()->NewNode(common()->Branch(), cond, start);
    Node* tb = graph()->NewNode(common()->IfTrue(), br);
    Node* fb = graph()->NewNode(common()->IfFalse(), br);
    Node* m = graph()->NewNode(common()->Merge(2), tb, fb);
    NodeProperties::ReplaceControlInput(ret, m);
    return br;
  }

  Node* Int64Constant(int64_t v) {
    return graph()->NewNode(common()->Int64Constant(v));
  }

  SimplifiedOperatorBuilder* simplified() { return &main_simplified_; }
  MachineOperatorBuilder* machine() { return &main_machine_; }
  CommonOperatorBuilder* common() { return &main_common_; }
  Graph* graph() { return main_graph_; }
};


TEST(LowerBooleanNot_bit_bit) {
  // BooleanNot(x: kRepBit) used as kRepBit
  TestingGraph t(Type::Boolean());
  Node* b = t.ExampleWithOutput(kRepBit);
  Node* inv = t.graph()->NewNode(t.simplified()->BooleanNot(), b);
  Node* use = t.Branch(inv);
  t.Lower();
  Node* cmp = use->InputAt(0);
  CHECK_EQ(t.machine()->Word32Equal()->opcode(), cmp->opcode());
  CHECK(b == cmp->InputAt(0) || b == cmp->InputAt(1));
  Node* f = t.jsgraph.Int32Constant(0);
  CHECK(f == cmp->InputAt(0) || f == cmp->InputAt(1));
}


TEST(LowerBooleanNot_bit_tagged) {
  // BooleanNot(x: kRepBit) used as kRepTagged
  TestingGraph t(Type::Boolean());
  Node* b = t.ExampleWithOutput(kRepBit);
  Node* inv = t.graph()->NewNode(t.simplified()->BooleanNot(), b);
  Node* use = t.Use(inv, kRepTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(IrOpcode::kChangeBitToBool, use->InputAt(0)->opcode());
  Node* cmp = use->InputAt(0)->InputAt(0);
  CHECK_EQ(t.machine()->Word32Equal()->opcode(), cmp->opcode());
  CHECK(b == cmp->InputAt(0) || b == cmp->InputAt(1));
  Node* f = t.jsgraph.Int32Constant(0);
  CHECK(f == cmp->InputAt(0) || f == cmp->InputAt(1));
}


TEST(LowerBooleanNot_tagged_bit) {
  // BooleanNot(x: kRepTagged) used as kRepBit
  TestingGraph t(Type::Boolean());
  Node* b = t.p0;
  Node* inv = t.graph()->NewNode(t.simplified()->BooleanNot(), b);
  Node* use = t.Branch(inv);
  t.Lower();
  Node* cmp = use->InputAt(0);
  CHECK_EQ(t.machine()->WordEqual()->opcode(), cmp->opcode());
  CHECK(b == cmp->InputAt(0) || b == cmp->InputAt(1));
  Node* f = t.jsgraph.FalseConstant();
  CHECK(f == cmp->InputAt(0) || f == cmp->InputAt(1));
}


TEST(LowerBooleanNot_tagged_tagged) {
  // BooleanNot(x: kRepTagged) used as kRepTagged
  TestingGraph t(Type::Boolean());
  Node* b = t.p0;
  Node* inv = t.graph()->NewNode(t.simplified()->BooleanNot(), b);
  Node* use = t.Use(inv, kRepTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(IrOpcode::kChangeBitToBool, use->InputAt(0)->opcode());
  Node* cmp = use->InputAt(0)->InputAt(0);
  CHECK_EQ(t.machine()->WordEqual()->opcode(), cmp->opcode());
  CHECK(b == cmp->InputAt(0) || b == cmp->InputAt(1));
  Node* f = t.jsgraph.FalseConstant();
  CHECK(f == cmp->InputAt(0) || f == cmp->InputAt(1));
}


TEST(LowerBooleanToNumber_bit_int32) {
  // BooleanToNumber(x: kRepBit) used as kMachInt32
  TestingGraph t(Type::Boolean());
  Node* b = t.ExampleWithOutput(kRepBit);
  Node* cnv = t.graph()->NewNode(t.simplified()->BooleanToNumber(), b);
  Node* use = t.Use(cnv, kMachInt32);
  t.Return(use);
  t.Lower();
  CHECK_EQ(b, use->InputAt(0));
}


TEST(LowerBooleanToNumber_tagged_int32) {
  // BooleanToNumber(x: kRepTagged) used as kMachInt32
  TestingGraph t(Type::Boolean());
  Node* b = t.p0;
  Node* cnv = t.graph()->NewNode(t.simplified()->BooleanToNumber(), b);
  Node* use = t.Use(cnv, kMachInt32);
  t.Return(use);
  t.Lower();
  CHECK_EQ(t.machine()->WordEqual()->opcode(), cnv->opcode());
  CHECK(b == cnv->InputAt(0) || b == cnv->InputAt(1));
  Node* c = t.jsgraph.TrueConstant();
  CHECK(c == cnv->InputAt(0) || c == cnv->InputAt(1));
}


TEST(LowerBooleanToNumber_bit_tagged) {
  // BooleanToNumber(x: kRepBit) used as kMachAnyTagged
  TestingGraph t(Type::Boolean());
  Node* b = t.ExampleWithOutput(kRepBit);
  Node* cnv = t.graph()->NewNode(t.simplified()->BooleanToNumber(), b);
  Node* use = t.Use(cnv, kMachAnyTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(b, use->InputAt(0)->InputAt(0));
  CHECK_EQ(IrOpcode::kChangeInt32ToTagged, use->InputAt(0)->opcode());
}


TEST(LowerBooleanToNumber_tagged_tagged) {
  // BooleanToNumber(x: kRepTagged) used as kMachAnyTagged
  TestingGraph t(Type::Boolean());
  Node* b = t.p0;
  Node* cnv = t.graph()->NewNode(t.simplified()->BooleanToNumber(), b);
  Node* use = t.Use(cnv, kMachAnyTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(cnv, use->InputAt(0)->InputAt(0));
  CHECK_EQ(IrOpcode::kChangeInt32ToTagged, use->InputAt(0)->opcode());
  CHECK_EQ(t.machine()->WordEqual()->opcode(), cnv->opcode());
  CHECK(b == cnv->InputAt(0) || b == cnv->InputAt(1));
  Node* c = t.jsgraph.TrueConstant();
  CHECK(c == cnv->InputAt(0) || c == cnv->InputAt(1));
}


static Type* test_types[] = {Type::Signed32(), Type::Unsigned32(),
                             Type::Number(), Type::Any()};


TEST(LowerNumberCmp_to_int32) {
  TestingGraph t(Type::Signed32(), Type::Signed32());

  t.CheckLoweringBinop(IrOpcode::kWord32Equal, t.simplified()->NumberEqual());
  t.CheckLoweringBinop(IrOpcode::kInt32LessThan,
                       t.simplified()->NumberLessThan());
  t.CheckLoweringBinop(IrOpcode::kInt32LessThanOrEqual,
                       t.simplified()->NumberLessThanOrEqual());
}


TEST(LowerNumberCmp_to_uint32) {
  TestingGraph t(Type::Unsigned32(), Type::Unsigned32());

  t.CheckLoweringBinop(IrOpcode::kWord32Equal, t.simplified()->NumberEqual());
  t.CheckLoweringBinop(IrOpcode::kUint32LessThan,
                       t.simplified()->NumberLessThan());
  t.CheckLoweringBinop(IrOpcode::kUint32LessThanOrEqual,
                       t.simplified()->NumberLessThanOrEqual());
}


TEST(LowerNumberCmp_to_float64) {
  static Type* types[] = {Type::Number(), Type::Any()};

  for (size_t i = 0; i < arraysize(types); i++) {
    TestingGraph t(types[i], types[i]);

    t.CheckLoweringBinop(IrOpcode::kFloat64Equal,
                         t.simplified()->NumberEqual());
    t.CheckLoweringBinop(IrOpcode::kFloat64LessThan,
                         t.simplified()->NumberLessThan());
    t.CheckLoweringBinop(IrOpcode::kFloat64LessThanOrEqual,
                         t.simplified()->NumberLessThanOrEqual());
  }
}


TEST(LowerNumberAddSub_to_int32) {
  HandleAndZoneScope scope;
  Factory* f = scope.main_zone()->isolate()->factory();
  Type* small_range =
      Type::Range(f->NewNumber(1), f->NewNumber(10), scope.main_zone());
  Type* large_range =
      Type::Range(f->NewNumber(-1e+13), f->NewNumber(1e+14), scope.main_zone());
  static Type* types[] = {Type::Signed32(), Type::Integral32(), small_range,
                          large_range};

  for (size_t i = 0; i < arraysize(types); i++) {
    for (size_t j = 0; j < arraysize(types); j++) {
      TestingGraph t(types[i], types[j]);
      t.CheckLoweringTruncatedBinop(IrOpcode::kInt32Add,
                                    t.simplified()->NumberAdd(),
                                    t.simplified()->NumberToInt32());
      t.CheckLoweringTruncatedBinop(IrOpcode::kInt32Sub,
                                    t.simplified()->NumberSubtract(),
                                    t.simplified()->NumberToInt32());
    }
  }
}


TEST(LowerNumberAddSub_to_uint32) {
  HandleAndZoneScope scope;
  Factory* f = scope.main_zone()->isolate()->factory();
  Type* small_range =
      Type::Range(f->NewNumber(1), f->NewNumber(10), scope.main_zone());
  Type* large_range =
      Type::Range(f->NewNumber(-1e+13), f->NewNumber(1e+14), scope.main_zone());
  static Type* types[] = {Type::Signed32(), Type::Integral32(), small_range,
                          large_range};

  for (size_t i = 0; i < arraysize(types); i++) {
    for (size_t j = 0; j < arraysize(types); j++) {
      TestingGraph t(types[i], types[j]);
      t.CheckLoweringTruncatedBinop(IrOpcode::kInt32Add,
                                    t.simplified()->NumberAdd(),
                                    t.simplified()->NumberToUint32());
      t.CheckLoweringTruncatedBinop(IrOpcode::kInt32Sub,
                                    t.simplified()->NumberSubtract(),
                                    t.simplified()->NumberToUint32());
    }
  }
}


TEST(LowerNumberAddSub_to_float64) {
  for (size_t i = 0; i < arraysize(test_types); i++) {
    TestingGraph t(test_types[i], test_types[i]);

    t.CheckLoweringBinop(IrOpcode::kFloat64Add, t.simplified()->NumberAdd());
    t.CheckLoweringBinop(IrOpcode::kFloat64Sub,
                         t.simplified()->NumberSubtract());
  }
}


TEST(LowerNumberDivMod_to_float64) {
  for (size_t i = 0; i < arraysize(test_types); i++) {
    TestingGraph t(test_types[i], test_types[i]);

    t.CheckLoweringBinop(IrOpcode::kFloat64Div, t.simplified()->NumberDivide());
    if (!test_types[i]->Is(Type::Unsigned32())) {
      t.CheckLoweringBinop(IrOpcode::kFloat64Mod,
                           t.simplified()->NumberModulus());
    }
  }
}


static void CheckChangeOf(IrOpcode::Value change, Node* of, Node* node) {
  CHECK_EQ(change, node->opcode());
  CHECK_EQ(of, node->InputAt(0));
}


TEST(LowerNumberToInt32_to_nop) {
  // NumberToInt32(x: kRepTagged | kTypeInt32) used as kRepTagged
  TestingGraph t(Type::Signed32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), t.p0);
  Node* use = t.Use(trunc, kRepTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(t.p0, use->InputAt(0));
}


TEST(LowerNumberToInt32_to_ChangeTaggedToFloat64) {
  // NumberToInt32(x: kRepTagged | kTypeInt32) used as kRepFloat64
  TestingGraph t(Type::Signed32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), t.p0);
  Node* use = t.Use(trunc, kRepFloat64);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kChangeTaggedToFloat64, t.p0, use->InputAt(0));
}


TEST(LowerNumberToInt32_to_ChangeTaggedToInt32) {
  // NumberToInt32(x: kRepTagged | kTypeInt32) used as kRepWord32
  TestingGraph t(Type::Signed32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), t.p0);
  Node* use = t.Use(trunc, kTypeInt32);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kChangeTaggedToInt32, t.p0, use->InputAt(0));
}


TEST(LowerNumberToInt32_to_TruncateFloat64ToInt32) {
  // NumberToInt32(x: kRepFloat64) used as kMachInt32
  TestingGraph t(Type::Number());
  Node* p0 = t.ExampleWithOutput(kMachFloat64);
  // TODO(titzer): run the typer here, or attach machine type to param.
  NodeProperties::SetBounds(p0, Bounds(Type::Number()));
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), p0);
  Node* use = t.Use(trunc, kMachInt32);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kTruncateFloat64ToInt32, p0, use->InputAt(0));
}


TEST(LowerNumberToInt32_to_TruncateFloat64ToInt32_with_change) {
  // NumberToInt32(x: kTypeNumber | kRepTagged) used as kMachInt32
  TestingGraph t(Type::Number());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), t.p0);
  Node* use = t.Use(trunc, kMachInt32);
  t.Return(use);
  t.Lower();
  Node* node = use->InputAt(0);
  CHECK_EQ(IrOpcode::kTruncateFloat64ToInt32, node->opcode());
  Node* of = node->InputAt(0);
  CHECK_EQ(IrOpcode::kChangeTaggedToFloat64, of->opcode());
  CHECK_EQ(t.p0, of->InputAt(0));
}


TEST(LowerNumberToInt32_to_ChangeFloat64ToTagged) {
  // TODO(titzer): NumberToInt32(x: kRepFloat64 | kTypeInt32) used as kRepTagged
}


TEST(LowerNumberToInt32_to_ChangeFloat64ToInt32) {
  // TODO(titzer): NumberToInt32(x: kRepFloat64 | kTypeInt32) used as kRepWord32
  // | kTypeInt32
}


TEST(LowerNumberToUint32_to_nop) {
  // NumberToUint32(x: kRepTagged | kTypeUint32) used as kRepTagged
  TestingGraph t(Type::Unsigned32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), t.p0);
  Node* use = t.Use(trunc, kRepTagged);
  t.Return(use);
  t.Lower();
  CHECK_EQ(t.p0, use->InputAt(0));
}


TEST(LowerNumberToUint32_to_ChangeTaggedToFloat64) {
  // NumberToUint32(x: kRepTagged | kTypeUint32) used as kRepWord32
  TestingGraph t(Type::Unsigned32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), t.p0);
  Node* use = t.Use(trunc, kRepFloat64);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kChangeTaggedToFloat64, t.p0, use->InputAt(0));
}


TEST(LowerNumberToUint32_to_ChangeTaggedToUint32) {
  // NumberToUint32(x: kRepTagged | kTypeUint32) used as kRepWord32
  TestingGraph t(Type::Unsigned32());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), t.p0);
  Node* use = t.Use(trunc, kTypeUint32);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kChangeTaggedToUint32, t.p0, use->InputAt(0));
}


TEST(LowerNumberToUint32_to_TruncateFloat64ToInt32) {
  // NumberToUint32(x: kRepFloat64) used as kMachUint32
  TestingGraph t(Type::Number());
  Node* p0 = t.ExampleWithOutput(kMachFloat64);
  // TODO(titzer): run the typer here, or attach machine type to param.
  NodeProperties::SetBounds(p0, Bounds(Type::Number()));
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), p0);
  Node* use = t.Use(trunc, kMachUint32);
  t.Return(use);
  t.Lower();
  CheckChangeOf(IrOpcode::kTruncateFloat64ToInt32, p0, use->InputAt(0));
}


TEST(LowerNumberToUint32_to_TruncateFloat64ToInt32_with_change) {
  // NumberToInt32(x: kTypeNumber | kRepTagged) used as kMachUint32
  TestingGraph t(Type::Number());
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), t.p0);
  Node* use = t.Use(trunc, kMachUint32);
  t.Return(use);
  t.Lower();
  Node* node = use->InputAt(0);
  CHECK_EQ(IrOpcode::kTruncateFloat64ToInt32, node->opcode());
  Node* of = node->InputAt(0);
  CHECK_EQ(IrOpcode::kChangeTaggedToFloat64, of->opcode());
  CHECK_EQ(t.p0, of->InputAt(0));
}


TEST(LowerNumberToUint32_to_ChangeFloat64ToTagged) {
  // TODO(titzer): NumberToUint32(x: kRepFloat64 | kTypeUint32) used as
  // kRepTagged
}


TEST(LowerNumberToUint32_to_ChangeFloat64ToUint32) {
  // TODO(titzer): NumberToUint32(x: kRepFloat64 | kTypeUint32) used as
  // kRepWord32
}


TEST(LowerNumberToUint32_to_TruncateFloat64ToUint32) {
  // TODO(titzer): NumberToUint32(x: kRepFloat64) used as kRepWord32
}


TEST(LowerReferenceEqual_to_wordeq) {
  TestingGraph t(Type::Any(), Type::Any());
  IrOpcode::Value opcode =
      static_cast<IrOpcode::Value>(t.machine()->WordEqual()->opcode());
  t.CheckLoweringBinop(opcode, t.simplified()->ReferenceEqual(Type::Any()));
}


TEST(LowerStringOps_to_call_and_compare) {
  if (Pipeline::SupportedTarget()) {
    // These tests need linkage for the calls.
    TestingGraph t(Type::String(), Type::String());
    IrOpcode::Value compare_eq =
        static_cast<IrOpcode::Value>(t.machine()->WordEqual()->opcode());
    IrOpcode::Value compare_lt =
        static_cast<IrOpcode::Value>(t.machine()->IntLessThan()->opcode());
    IrOpcode::Value compare_le = static_cast<IrOpcode::Value>(
        t.machine()->IntLessThanOrEqual()->opcode());
    t.CheckLoweringBinop(compare_eq, t.simplified()->StringEqual());
    t.CheckLoweringBinop(compare_lt, t.simplified()->StringLessThan());
    t.CheckLoweringBinop(compare_le, t.simplified()->StringLessThanOrEqual());
    t.CheckLoweringBinop(IrOpcode::kCall, t.simplified()->StringAdd());
  }
}


void CheckChangeInsertion(IrOpcode::Value expected, MachineType from,
                          MachineType to) {
  TestingGraph t(Type::Any());
  Node* in = t.ExampleWithOutput(from);
  Node* use = t.Use(in, to);
  t.Return(use);
  t.Lower();
  CHECK_EQ(expected, use->InputAt(0)->opcode());
  CHECK_EQ(in, use->InputAt(0)->InputAt(0));
}


TEST(InsertBasicChanges) {
  CheckChangeInsertion(IrOpcode::kChangeFloat64ToInt32, kRepFloat64,
                       kTypeInt32);
  CheckChangeInsertion(IrOpcode::kChangeFloat64ToUint32, kRepFloat64,
                       kTypeUint32);
  CheckChangeInsertion(IrOpcode::kChangeTaggedToInt32, kRepTagged, kTypeInt32);
  CheckChangeInsertion(IrOpcode::kChangeTaggedToUint32, kRepTagged,
                       kTypeUint32);

  CheckChangeInsertion(IrOpcode::kChangeFloat64ToTagged, kRepFloat64,
                       kRepTagged);
  CheckChangeInsertion(IrOpcode::kChangeTaggedToFloat64, kRepTagged,
                       kRepFloat64);

  CheckChangeInsertion(IrOpcode::kChangeInt32ToFloat64, kTypeInt32,
                       kRepFloat64);
  CheckChangeInsertion(IrOpcode::kChangeInt32ToTagged, kTypeInt32, kRepTagged);

  CheckChangeInsertion(IrOpcode::kChangeUint32ToFloat64, kTypeUint32,
                       kRepFloat64);
  CheckChangeInsertion(IrOpcode::kChangeUint32ToTagged, kTypeUint32,
                       kRepTagged);
}


static void CheckChangesAroundBinop(TestingGraph* t, const Operator* op,
                                    IrOpcode::Value input_change,
                                    IrOpcode::Value output_change) {
  Node* binop = t->graph()->NewNode(op, t->p0, t->p1);
  t->Return(binop);
  t->Lower();
  CHECK_EQ(input_change, binop->InputAt(0)->opcode());
  CHECK_EQ(input_change, binop->InputAt(1)->opcode());
  CHECK_EQ(t->p0, binop->InputAt(0)->InputAt(0));
  CHECK_EQ(t->p1, binop->InputAt(1)->InputAt(0));
  CHECK_EQ(output_change, t->ret->InputAt(0)->opcode());
  CHECK_EQ(binop, t->ret->InputAt(0)->InputAt(0));
}


TEST(InsertChangesAroundInt32Binops) {
  TestingGraph t(Type::Signed32(), Type::Signed32());

  const Operator* ops[] = {t.machine()->Int32Add(),  t.machine()->Int32Sub(),
                           t.machine()->Int32Mul(),  t.machine()->Int32Div(),
                           t.machine()->Int32Mod(),  t.machine()->Word32And(),
                           t.machine()->Word32Or(),  t.machine()->Word32Xor(),
                           t.machine()->Word32Shl(), t.machine()->Word32Sar()};

  for (size_t i = 0; i < arraysize(ops); i++) {
    CheckChangesAroundBinop(&t, ops[i], IrOpcode::kChangeTaggedToInt32,
                            IrOpcode::kChangeInt32ToTagged);
  }
}


TEST(InsertChangesAroundInt32Cmp) {
  TestingGraph t(Type::Signed32(), Type::Signed32());

  const Operator* ops[] = {t.machine()->Int32LessThan(),
                           t.machine()->Int32LessThanOrEqual()};

  for (size_t i = 0; i < arraysize(ops); i++) {
    CheckChangesAroundBinop(&t, ops[i], IrOpcode::kChangeTaggedToInt32,
                            IrOpcode::kChangeBitToBool);
  }
}


TEST(InsertChangesAroundUint32Cmp) {
  TestingGraph t(Type::Unsigned32(), Type::Unsigned32());

  const Operator* ops[] = {t.machine()->Uint32LessThan(),
                           t.machine()->Uint32LessThanOrEqual()};

  for (size_t i = 0; i < arraysize(ops); i++) {
    CheckChangesAroundBinop(&t, ops[i], IrOpcode::kChangeTaggedToUint32,
                            IrOpcode::kChangeBitToBool);
  }
}


TEST(InsertChangesAroundFloat64Binops) {
  TestingGraph t(Type::Number(), Type::Number());

  const Operator* ops[] = {
      t.machine()->Float64Add(), t.machine()->Float64Sub(),
      t.machine()->Float64Mul(), t.machine()->Float64Div(),
      t.machine()->Float64Mod(),
  };

  for (size_t i = 0; i < arraysize(ops); i++) {
    CheckChangesAroundBinop(&t, ops[i], IrOpcode::kChangeTaggedToFloat64,
                            IrOpcode::kChangeFloat64ToTagged);
  }
}


TEST(InsertChangesAroundFloat64Cmp) {
  TestingGraph t(Type::Number(), Type::Number());

  const Operator* ops[] = {t.machine()->Float64Equal(),
                           t.machine()->Float64LessThan(),
                           t.machine()->Float64LessThanOrEqual()};

  for (size_t i = 0; i < arraysize(ops); i++) {
    CheckChangesAroundBinop(&t, ops[i], IrOpcode::kChangeTaggedToFloat64,
                            IrOpcode::kChangeBitToBool);
  }
}


namespace {

void CheckFieldAccessArithmetic(FieldAccess access, Node* load_or_store) {
  IntPtrMatcher mindex(load_or_store->InputAt(1));
  CHECK(mindex.Is(access.offset - access.tag()));
}


Node* CheckElementAccessArithmetic(ElementAccess access, Node* load_or_store) {
  Node* index = load_or_store->InputAt(1);
  if (kPointerSize == 8) {
    CHECK_EQ(IrOpcode::kChangeUint32ToUint64, index->opcode());
    index = index->InputAt(0);
  }

  Int32BinopMatcher mindex(index);
  CHECK_EQ(IrOpcode::kInt32Add, mindex.node()->opcode());
  CHECK(mindex.right().Is(access.header_size - access.tag()));

  const int element_size_shift = ElementSizeLog2Of(access.machine_type);
  if (element_size_shift) {
    Int32BinopMatcher shl(mindex.left().node());
    CHECK_EQ(IrOpcode::kWord32Shl, shl.node()->opcode());
    CHECK(shl.right().Is(element_size_shift));
    return shl.left().node();
  } else {
    return mindex.left().node();
  }
}


const MachineType kMachineReps[] = {kRepBit,       kMachInt8,  kMachInt16,
                                    kMachInt32,    kMachInt64, kMachFloat64,
                                    kMachAnyTagged};

}  // namespace


TEST(LowerLoadField_to_load) {
  TestingGraph t(Type::Any(), Type::Signed32());

  for (size_t i = 0; i < arraysize(kMachineReps); i++) {
    FieldAccess access = {kTaggedBase, FixedArrayBase::kHeaderSize,
                          Handle<Name>::null(), Type::Any(), kMachineReps[i]};

    Node* load =
        t.graph()->NewNode(t.simplified()->LoadField(access), t.p0, t.start);
    Node* use = t.Use(load, kMachineReps[i]);
    t.Return(use);
    t.Lower();
    CHECK_EQ(IrOpcode::kLoad, load->opcode());
    CHECK_EQ(t.p0, load->InputAt(0));
    CheckFieldAccessArithmetic(access, load);

    MachineType rep = OpParameter<MachineType>(load);
    CHECK_EQ(kMachineReps[i], rep);
  }
}


TEST(LowerStoreField_to_store) {
  TestingGraph t(Type::Any(), Type::Signed32());

  for (size_t i = 0; i < arraysize(kMachineReps); i++) {
    FieldAccess access = {kTaggedBase, FixedArrayBase::kHeaderSize,
                          Handle<Name>::null(), Type::Any(), kMachineReps[i]};


    Node* val = t.ExampleWithOutput(kMachineReps[i]);
    Node* store = t.graph()->NewNode(t.simplified()->StoreField(access), t.p0,
                                     val, t.start, t.start);
    t.Effect(store);
    t.Lower();
    CHECK_EQ(IrOpcode::kStore, store->opcode());
    CHECK_EQ(val, store->InputAt(2));
    CheckFieldAccessArithmetic(access, store);

    StoreRepresentation rep = OpParameter<StoreRepresentation>(store);
    if (kMachineReps[i] & kRepTagged) {
      CHECK_EQ(kFullWriteBarrier, rep.write_barrier_kind());
    }
    CHECK_EQ(kMachineReps[i], rep.machine_type());
  }
}


TEST(LowerLoadElement_to_load) {
  TestingGraph t(Type::Any(), Type::Signed32());

  for (size_t i = 0; i < arraysize(kMachineReps); i++) {
    ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                            FixedArrayBase::kHeaderSize, Type::Any(),
                            kMachineReps[i]};

    Node* load =
        t.graph()->NewNode(t.simplified()->LoadElement(access), t.p0, t.p1,
                           t.jsgraph.Int32Constant(1024), t.start, t.start);
    Node* use = t.Use(load, kMachineReps[i]);
    t.Return(use);
    t.Lower();
    CHECK_EQ(IrOpcode::kLoad, load->opcode());
    CHECK_EQ(t.p0, load->InputAt(0));
    CheckElementAccessArithmetic(access, load);

    MachineType rep = OpParameter<MachineType>(load);
    CHECK_EQ(kMachineReps[i], rep);
  }
}


TEST(LowerStoreElement_to_store) {
  TestingGraph t(Type::Any(), Type::Signed32());

  for (size_t i = 0; i < arraysize(kMachineReps); i++) {
    ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                            FixedArrayBase::kHeaderSize, Type::Any(),
                            kMachineReps[i]};

    Node* val = t.ExampleWithOutput(kMachineReps[i]);
    Node* store = t.graph()->NewNode(t.simplified()->StoreElement(access), t.p0,
                                     t.p1, t.jsgraph.Int32Constant(1024), val,
                                     t.start, t.start);
    t.Effect(store);
    t.Lower();
    CHECK_EQ(IrOpcode::kStore, store->opcode());
    CHECK_EQ(val, store->InputAt(2));
    CheckElementAccessArithmetic(access, store);

    StoreRepresentation rep = OpParameter<StoreRepresentation>(store);
    if (kMachineReps[i] & kRepTagged) {
      CHECK_EQ(kFullWriteBarrier, rep.write_barrier_kind());
    }
    CHECK_EQ(kMachineReps[i], rep.machine_type());
  }
}


TEST(InsertChangeForLoadElementIndex) {
  // LoadElement(obj: Tagged, index: kTypeInt32 | kRepTagged, length) =>
  //   Load(obj, Int32Add(Int32Mul(ChangeTaggedToInt32(index), #k), #k))
  TestingGraph t(Type::Any(), Type::Signed32(), Type::Any());
  ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                          FixedArrayBase::kHeaderSize, Type::Any(),
                          kMachAnyTagged};

  Node* load = t.graph()->NewNode(t.simplified()->LoadElement(access), t.p0,
                                  t.p1, t.p2, t.start, t.start);
  t.Return(load);
  t.Lower();
  CHECK_EQ(IrOpcode::kLoad, load->opcode());
  CHECK_EQ(t.p0, load->InputAt(0));

  Node* index = CheckElementAccessArithmetic(access, load);
  CheckChangeOf(IrOpcode::kChangeTaggedToInt32, t.p1, index);
}


TEST(InsertChangeForStoreElementIndex) {
  // StoreElement(obj: Tagged, index: kTypeInt32 | kRepTagged, length, val) =>
  //   Store(obj, Int32Add(Int32Mul(ChangeTaggedToInt32(index), #k), #k), val)
  TestingGraph t(Type::Any(), Type::Signed32(), Type::Any());
  ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                          FixedArrayBase::kHeaderSize, Type::Any(),
                          kMachAnyTagged};

  Node* store =
      t.graph()->NewNode(t.simplified()->StoreElement(access), t.p0, t.p1, t.p2,
                         t.jsgraph.TrueConstant(), t.start, t.start);
  t.Effect(store);
  t.Lower();
  CHECK_EQ(IrOpcode::kStore, store->opcode());
  CHECK_EQ(t.p0, store->InputAt(0));

  Node* index = CheckElementAccessArithmetic(access, store);
  CheckChangeOf(IrOpcode::kChangeTaggedToInt32, t.p1, index);
}


TEST(InsertChangeForLoadElement) {
  // TODO(titzer): test all load/store representation change insertions.
  TestingGraph t(Type::Any(), Type::Signed32(), Type::Any());
  ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                          FixedArrayBase::kHeaderSize, Type::Any(),
                          kMachFloat64};

  Node* load = t.graph()->NewNode(t.simplified()->LoadElement(access), t.p0,
                                  t.p1, t.p1, t.start, t.start);
  t.Return(load);
  t.Lower();
  CHECK_EQ(IrOpcode::kLoad, load->opcode());
  CHECK_EQ(t.p0, load->InputAt(0));
  CheckChangeOf(IrOpcode::kChangeFloat64ToTagged, load, t.ret->InputAt(0));
}


TEST(InsertChangeForLoadField) {
  // TODO(titzer): test all load/store representation change insertions.
  TestingGraph t(Type::Any(), Type::Signed32());
  FieldAccess access = {kTaggedBase, FixedArrayBase::kHeaderSize,
                        Handle<Name>::null(), Type::Any(), kMachFloat64};

  Node* load =
      t.graph()->NewNode(t.simplified()->LoadField(access), t.p0, t.start);
  t.Return(load);
  t.Lower();
  CHECK_EQ(IrOpcode::kLoad, load->opcode());
  CHECK_EQ(t.p0, load->InputAt(0));
  CheckChangeOf(IrOpcode::kChangeFloat64ToTagged, load, t.ret->InputAt(0));
}


TEST(InsertChangeForStoreElement) {
  // TODO(titzer): test all load/store representation change insertions.
  TestingGraph t(Type::Any(), Type::Signed32(), Type::Any());
  ElementAccess access = {kNoBoundsCheck, kTaggedBase,
                          FixedArrayBase::kHeaderSize, Type::Any(),
                          kMachFloat64};

  Node* store = t.graph()->NewNode(t.simplified()->StoreElement(access), t.p0,
                                   t.jsgraph.Int32Constant(0), t.p2, t.p1,
                                   t.start, t.start);
  t.Effect(store);
  t.Lower();

  CHECK_EQ(IrOpcode::kStore, store->opcode());
  CHECK_EQ(t.p0, store->InputAt(0));
  CheckChangeOf(IrOpcode::kChangeTaggedToFloat64, t.p1, store->InputAt(2));
}


TEST(InsertChangeForStoreField) {
  // TODO(titzer): test all load/store representation change insertions.
  TestingGraph t(Type::Any(), Type::Signed32());
  FieldAccess access = {kTaggedBase, FixedArrayBase::kHeaderSize,
                        Handle<Name>::null(), Type::Any(), kMachFloat64};

  Node* store = t.graph()->NewNode(t.simplified()->StoreField(access), t.p0,
                                   t.p1, t.start, t.start);
  t.Effect(store);
  t.Lower();

  CHECK_EQ(IrOpcode::kStore, store->opcode());
  CHECK_EQ(t.p0, store->InputAt(0));
  CheckChangeOf(IrOpcode::kChangeTaggedToFloat64, t.p1, store->InputAt(2));
}


TEST(UpdatePhi) {
  TestingGraph t(Type::Any(), Type::Signed32());
  static const MachineType kMachineTypes[] = {kMachInt32, kMachUint32,
                                              kMachFloat64};
  Type* kTypes[] = {Type::Signed32(), Type::Unsigned32(), Type::Number()};

  for (size_t i = 0; i < arraysize(kMachineTypes); i++) {
    FieldAccess access = {kTaggedBase, FixedArrayBase::kHeaderSize,
                          Handle<Name>::null(), kTypes[i], kMachineTypes[i]};

    Node* load0 =
        t.graph()->NewNode(t.simplified()->LoadField(access), t.p0, t.start);
    Node* load1 =
        t.graph()->NewNode(t.simplified()->LoadField(access), t.p1, t.start);
    Node* phi = t.graph()->NewNode(t.common()->Phi(kMachAnyTagged, 2), load0,
                                   load1, t.start);
    t.Return(t.Use(phi, kMachineTypes[i]));
    t.Lower();

    CHECK_EQ(IrOpcode::kPhi, phi->opcode());
    CHECK_EQ(RepresentationOf(kMachineTypes[i]),
             RepresentationOf(OpParameter<MachineType>(phi)));
  }
}


TEST(RunNumberDivide_minus_1_TruncatingToInt32) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  Node* num = t.NumberToInt32(t.Parameter(0));
  Node* div = t.NumberDivide(num, t.jsgraph.Constant(-1));
  Node* trunc = t.NumberToInt32(div);
  t.Return(trunc);

  if (Pipeline::SupportedTarget()) {
    t.LowerAllNodesAndLowerChanges();
    t.GenerateCode();

    FOR_INT32_INPUTS(i) {
      int32_t x = 0 - *i;
      t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
    }
  }
}


TEST(NumberMultiply_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, -1, 0, 1, 100, 1000};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* mul = t.graph()->NewNode(t.simplified()->NumberMultiply(), t.p0, k);
    Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), mul);
    t.Return(trunc);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Mul, mul->opcode());
  }
}


TEST(RunNumberMultiply_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, -1, 0, 1, 100, 1000, 3000999};

  for (size_t i = 0; i < arraysize(constants); i++) {
    double k = static_cast<double>(constants[i]);
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToInt32(t.Parameter(0));
    Node* mul = t.NumberMultiply(num, t.jsgraph.Constant(k));
    Node* trunc = t.NumberToInt32(mul);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_INT32_INPUTS(i) {
        int32_t x = DoubleToInt32(static_cast<double>(*i) * k);
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(RunNumberMultiply_TruncatingToUint32) {
  uint32_t constants[] = {0, 1, 2, 3, 4, 100, 1000, 1024, 2048, 3000999};

  for (size_t i = 0; i < arraysize(constants); i++) {
    double k = static_cast<double>(constants[i]);
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToUint32(t.Parameter(0));
    Node* mul = t.NumberMultiply(num, t.jsgraph.Constant(k));
    Node* trunc = t.NumberToUint32(mul);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_UINT32_INPUTS(i) {
        uint32_t x = DoubleToUint32(static_cast<double>(*i) * k);
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(RunNumberDivide_2_TruncatingToUint32) {
  SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
  Node* num = t.NumberToUint32(t.Parameter(0));
  Node* div = t.NumberDivide(num, t.jsgraph.Constant(2));
  Node* trunc = t.NumberToUint32(div);
  t.Return(trunc);

  if (Pipeline::SupportedTarget()) {
    t.LowerAllNodesAndLowerChanges();
    t.GenerateCode();

    FOR_UINT32_INPUTS(i) {
      uint32_t x = DoubleToUint32(static_cast<double>(*i / 2.0));
      t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
    }
  }
}


TEST(NumberMultiply_ConstantOutOfRange) {
  TestingGraph t(Type::Signed32());
  Node* k = t.jsgraph.Constant(1000000023);
  Node* mul = t.graph()->NewNode(t.simplified()->NumberMultiply(), t.p0, k);
  Node* trunc = t.graph()->NewNode(t.simplified()->NumberToInt32(), mul);
  t.Return(trunc);
  t.Lower();

  CHECK_EQ(IrOpcode::kFloat64Mul, mul->opcode());
}


TEST(NumberMultiply_NonTruncating) {
  TestingGraph t(Type::Signed32());
  Node* k = t.jsgraph.Constant(111);
  Node* mul = t.graph()->NewNode(t.simplified()->NumberMultiply(), t.p0, k);
  t.Return(mul);
  t.Lower();

  CHECK_EQ(IrOpcode::kFloat64Mul, mul->opcode());
}


TEST(NumberDivide_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, 1, 4, 100, 1000};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* div = t.graph()->NewNode(t.simplified()->NumberDivide(), t.p0, k);
    Node* use = t.Use(div, kMachInt32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Div, use->InputAt(0)->opcode());
  }
}


TEST(RunNumberDivide_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, -1, 1, 2, 100, 1000, 1024, 2048};

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToInt32(t.Parameter(0));
    Node* div = t.NumberDivide(num, t.jsgraph.Constant(k));
    Node* trunc = t.NumberToInt32(div);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_INT32_INPUTS(i) {
        if (*i == INT_MAX) continue;  // exclude max int.
        int32_t x = DoubleToInt32(static_cast<double>(*i) / k);
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(NumberDivide_TruncatingToUint32) {
  double constants[] = {1, 3, 100, 1000, 100998348};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Unsigned32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* div = t.graph()->NewNode(t.simplified()->NumberDivide(), t.p0, k);
    Node* use = t.Use(div, kMachUint32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kUint32Div, use->InputAt(0)->opcode());
  }
}


TEST(RunNumberDivide_TruncatingToUint32) {
  uint32_t constants[] = {100, 10, 1, 1, 2, 4, 1000, 1024, 2048};

  for (size_t i = 0; i < arraysize(constants); i++) {
    uint32_t k = constants[i];
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToUint32(t.Parameter(0));
    Node* div = t.NumberDivide(num, t.jsgraph.Constant(static_cast<double>(k)));
    Node* trunc = t.NumberToUint32(div);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_UINT32_INPUTS(i) {
        uint32_t x = *i / k;
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(NumberDivide_BadConstants) {
  {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(-1);
    Node* div = t.graph()->NewNode(t.simplified()->NumberDivide(), t.p0, k);
    Node* use = t.Use(div, kMachInt32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Sub, use->InputAt(0)->opcode());
  }

  {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(0);
    Node* div = t.graph()->NewNode(t.simplified()->NumberDivide(), t.p0, k);
    Node* use = t.Use(div, kMachInt32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Constant, use->InputAt(0)->opcode());
    CHECK_EQ(0, OpParameter<int32_t>(use->InputAt(0)));
  }

  {
    TestingGraph t(Type::Unsigned32());
    Node* k = t.jsgraph.Constant(0);
    Node* div = t.graph()->NewNode(t.simplified()->NumberDivide(), t.p0, k);
    Node* use = t.Use(div, kMachUint32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Constant, use->InputAt(0)->opcode());
    CHECK_EQ(0, OpParameter<int32_t>(use->InputAt(0)));
  }
}


TEST(NumberModulus_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, 1, 4, 100, 1000};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* mod = t.graph()->NewNode(t.simplified()->NumberModulus(), t.p0, k);
    Node* use = t.Use(mod, kMachInt32);
    t.Return(use);
    t.Lower();

    CHECK_EQ(IrOpcode::kInt32Mod, use->InputAt(0)->opcode());
  }
}


TEST(RunNumberModulus_TruncatingToInt32) {
  int32_t constants[] = {-100, -10, -1, 1, 2, 100, 1000, 1024, 2048};

  for (size_t i = 0; i < arraysize(constants); i++) {
    int32_t k = constants[i];
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToInt32(t.Parameter(0));
    Node* mod = t.NumberModulus(num, t.jsgraph.Constant(k));
    Node* trunc = t.NumberToInt32(mod);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_INT32_INPUTS(i) {
        if (*i == INT_MAX) continue;  // exclude max int.
        int32_t x = DoubleToInt32(std::fmod(static_cast<double>(*i), k));
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(NumberModulus_TruncatingToUint32) {
  double constants[] = {1, 3, 100, 1000, 100998348};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Unsigned32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* mod = t.graph()->NewNode(t.simplified()->NumberModulus(), t.p0, k);
    Node* trunc = t.graph()->NewNode(t.simplified()->NumberToUint32(), mod);
    Node* ret = t.Return(trunc);
    t.Lower();

    CHECK_EQ(IrOpcode::kUint32Mod, ret->InputAt(0)->opcode());
  }
}


TEST(RunNumberModulus_TruncatingToUint32) {
  uint32_t constants[] = {1, 2, 100, 1000, 1024, 2048};

  for (size_t i = 0; i < arraysize(constants); i++) {
    uint32_t k = constants[i];
    SimplifiedLoweringTester<Object*> t(kMachAnyTagged);
    Node* num = t.NumberToUint32(t.Parameter(0));
    Node* mod =
        t.NumberModulus(num, t.jsgraph.Constant(static_cast<double>(k)));
    Node* trunc = t.NumberToUint32(mod);
    t.Return(trunc);

    if (Pipeline::SupportedTarget()) {
      t.LowerAllNodesAndLowerChanges();
      t.GenerateCode();

      FOR_UINT32_INPUTS(i) {
        uint32_t x = *i % k;
        t.CheckNumberCall(static_cast<double>(x), static_cast<double>(*i));
      }
    }
  }
}


TEST(NumberModulus_Int32) {
  int32_t constants[] = {-100, -10, 1, 4, 100, 1000};

  for (size_t i = 0; i < arraysize(constants); i++) {
    TestingGraph t(Type::Signed32());
    Node* k = t.jsgraph.Constant(constants[i]);
    Node* mod = t.graph()->NewNode(t.simplified()->NumberModulus(), t.p0, k);
    t.Return(mod);
    t.Lower();

    CHECK_EQ(IrOpcode::kFloat64Mod, mod->opcode());  // Pesky -0 behavior.
  }
}


TEST(NumberModulus_Uint32) {
  const double kConstants[] = {2, 100, 1000, 1024, 2048};
  const MachineType kTypes[] = {kMachInt32, kMachUint32};

  for (auto const type : kTypes) {
    for (auto const c : kConstants) {
      TestingGraph t(Type::Unsigned32());
      Node* k = t.jsgraph.Constant(c);
      Node* mod = t.graph()->NewNode(t.simplified()->NumberModulus(), t.p0, k);
      Node* use = t.Use(mod, type);
      t.Return(use);
      t.Lower();

      CHECK_EQ(IrOpcode::kUint32Mod, use->InputAt(0)->opcode());
    }
  }
}


TEST(PhiRepresentation) {
  HandleAndZoneScope scope;
  Zone* z = scope.main_zone();

  Factory* f = z->isolate()->factory();
  Handle<Object> range_min = f->NewNumber(-1e13);
  Handle<Object> range_max = f->NewNumber(1e+15);
  Type* range = Type::Range(range_min, range_max, z);

  struct TestData {
    Type* arg1;
    Type* arg2;
    MachineType use;
    MachineTypeUnion expected;
  };

  TestData test_data[] = {
      {Type::Signed32(), Type::Unsigned32(), kMachInt32,
       kRepWord32 | kTypeNumber},
      {range, range, kMachUint32, kRepWord32 | kTypeNumber},
      {Type::Signed32(), Type::Signed32(), kMachInt32, kMachInt32},
      {Type::Unsigned32(), Type::Unsigned32(), kMachInt32, kMachUint32},
      {Type::Number(), Type::Signed32(), kMachInt32, kMachFloat64},
      {Type::Signed32(), Type::String(), kMachInt32, kMachAnyTagged}};

  for (auto const d : test_data) {
    TestingGraph t(d.arg1, d.arg2, Type::Boolean());

    Node* br = t.graph()->NewNode(t.common()->Branch(), t.p2, t.start);
    Node* tb = t.graph()->NewNode(t.common()->IfTrue(), br);
    Node* fb = t.graph()->NewNode(t.common()->IfFalse(), br);
    Node* m = t.graph()->NewNode(t.common()->Merge(2), tb, fb);

    Node* phi =
        t.graph()->NewNode(t.common()->Phi(kMachAnyTagged, 2), t.p0, t.p1, m);

    Bounds phi_bounds = Bounds::Either(Bounds(d.arg1), Bounds(d.arg2), z);
    NodeProperties::SetBounds(phi, phi_bounds);

    Node* use = t.Use(phi, d.use);
    t.Return(use);
    t.Lower();

    CHECK_EQ(d.expected, OpParameter<MachineType>(phi));
  }
}
