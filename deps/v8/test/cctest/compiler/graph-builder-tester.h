// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
#define V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_

#include "src/compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simplified-operator.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class GraphAndBuilders {
 public:
  explicit GraphAndBuilders(Zone* zone)
      : main_graph_(new (zone) Graph(zone)),
        main_common_(zone),
        main_machine_(zone, MachineType::PointerRepresentation(),
                      InstructionSelector::SupportedMachineOperatorFlags(),
                      InstructionSelector::AlignmentRequirements()),
        main_simplified_(zone) {}

  Graph* graph() const { return main_graph_; }
  Zone* zone() const { return graph()->zone(); }
  CommonOperatorBuilder* common() { return &main_common_; }
  MachineOperatorBuilder* machine() { return &main_machine_; }
  SimplifiedOperatorBuilder* simplified() { return &main_simplified_; }

 protected:
  // Prefixed with main_ to avoid naming conflicts.
  Graph* main_graph_;
  CommonOperatorBuilder main_common_;
  MachineOperatorBuilder main_machine_;
  SimplifiedOperatorBuilder main_simplified_;
};


template <typename ReturnType>
class GraphBuilderTester : public HandleAndZoneScope,
                           public GraphAndBuilders,
                           public CallHelper<ReturnType> {
 public:
  explicit GraphBuilderTester(MachineType p0 = MachineType::None(),
                              MachineType p1 = MachineType::None(),
                              MachineType p2 = MachineType::None(),
                              MachineType p3 = MachineType::None(),
                              MachineType p4 = MachineType::None())
      : GraphAndBuilders(main_zone()),
        CallHelper<ReturnType>(
            main_isolate(),
            CSignature::New(main_zone(), MachineTypeForC<ReturnType>(), p0, p1,
                            p2, p3, p4)),
        effect_(NULL),
        return_(NULL),
        parameters_(main_zone()->template NewArray<Node*>(parameter_count())) {
    Begin(static_cast<int>(parameter_count()));
    InitParameters();
  }
  virtual ~GraphBuilderTester() {}

  void GenerateCode() { Generate(); }
  Node* Parameter(size_t index) {
    CHECK_LT(index, parameter_count());
    return parameters_[index];
  }

  Isolate* isolate() { return main_isolate(); }
  Factory* factory() { return isolate()->factory(); }

  // Initialize graph and builder.
  void Begin(int num_parameters) {
    CHECK_NULL(graph()->start());
    Node* start = graph()->NewNode(common()->Start(num_parameters + 3));
    graph()->SetStart(start);
    effect_ = start;
  }

  void Return(Node* value) {
    return_ =
        graph()->NewNode(common()->Return(), value, effect_, graph()->start());
    effect_ = NULL;
  }

  // Close the graph.
  void End() {
    Node* end = graph()->NewNode(common()->End(1), return_);
    graph()->SetEnd(end);
  }

  Node* PointerConstant(void* value) {
    intptr_t intptr_value = reinterpret_cast<intptr_t>(value);
    return kPointerSize == 8 ? NewNode(common()->Int64Constant(intptr_value))
                             : Int32Constant(static_cast<int>(intptr_value));
  }
  Node* Int32Constant(int32_t value) {
    return NewNode(common()->Int32Constant(value));
  }
  Node* HeapConstant(Handle<HeapObject> object) {
    return NewNode(common()->HeapConstant(object));
  }

  Node* BooleanNot(Node* a) { return NewNode(simplified()->BooleanNot(), a); }

  Node* NumberEqual(Node* a, Node* b) {
    return NewNode(simplified()->NumberEqual(), a, b);
  }
  Node* NumberLessThan(Node* a, Node* b) {
    return NewNode(simplified()->NumberLessThan(), a, b);
  }
  Node* NumberLessThanOrEqual(Node* a, Node* b) {
    return NewNode(simplified()->NumberLessThanOrEqual(), a, b);
  }
  Node* NumberAdd(Node* a, Node* b) {
    return NewNode(simplified()->NumberAdd(), a, b);
  }
  Node* NumberSubtract(Node* a, Node* b) {
    return NewNode(simplified()->NumberSubtract(), a, b);
  }
  Node* NumberMultiply(Node* a, Node* b) {
    return NewNode(simplified()->NumberMultiply(), a, b);
  }
  Node* NumberDivide(Node* a, Node* b) {
    return NewNode(simplified()->NumberDivide(), a, b);
  }
  Node* NumberModulus(Node* a, Node* b) {
    return NewNode(simplified()->NumberModulus(), a, b);
  }
  Node* NumberToInt32(Node* a) {
    return NewNode(simplified()->NumberToInt32(), a);
  }
  Node* NumberToUint32(Node* a) {
    return NewNode(simplified()->NumberToUint32(), a);
  }

  Node* StringEqual(Node* a, Node* b) {
    return NewNode(simplified()->StringEqual(), a, b);
  }
  Node* StringLessThan(Node* a, Node* b) {
    return NewNode(simplified()->StringLessThan(), a, b);
  }
  Node* StringLessThanOrEqual(Node* a, Node* b) {
    return NewNode(simplified()->StringLessThanOrEqual(), a, b);
  }

  Node* ChangeTaggedToInt32(Node* a) {
    return NewNode(simplified()->ChangeTaggedToInt32(), a);
  }
  Node* ChangeTaggedToUint32(Node* a) {
    return NewNode(simplified()->ChangeTaggedToUint32(), a);
  }
  Node* ChangeTaggedToFloat64(Node* a) {
    return NewNode(simplified()->ChangeTaggedToFloat64(), a);
  }
  Node* ChangeInt32ToTagged(Node* a) {
    return NewNode(simplified()->ChangeInt32ToTagged(), a);
  }
  Node* ChangeUint32ToTagged(Node* a) {
    return NewNode(simplified()->ChangeUint32ToTagged(), a);
  }
  Node* ChangeFloat64ToTagged(Node* a) {
    return NewNode(simplified()->ChangeFloat64ToTagged(), a);
  }
  Node* ChangeTaggedToBit(Node* a) {
    return NewNode(simplified()->ChangeTaggedToBit(), a);
  }
  Node* ChangeBitToTagged(Node* a) {
    return NewNode(simplified()->ChangeBitToTagged(), a);
  }

  Node* LoadField(const FieldAccess& access, Node* object) {
    return NewNode(simplified()->LoadField(access), object);
  }
  Node* StoreField(const FieldAccess& access, Node* object, Node* value) {
    return NewNode(simplified()->StoreField(access), object, value);
  }
  Node* LoadElement(const ElementAccess& access, Node* object, Node* index) {
    return NewNode(simplified()->LoadElement(access), object, index);
  }
  Node* StoreElement(const ElementAccess& access, Node* object, Node* index,
                     Node* value) {
    return NewNode(simplified()->StoreElement(access), object, index, value);
  }

  Node* NewNode(const Operator* op) {
    return MakeNode(op, 0, static_cast<Node**>(NULL));
  }

  Node* NewNode(const Operator* op, Node* n1) { return MakeNode(op, 1, &n1); }

  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* buffer[] = {n1, n2};
    return MakeNode(op, arraysize(buffer), buffer);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* buffer[] = {n1, n2, n3};
    return MakeNode(op, arraysize(buffer), buffer);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* buffer[] = {n1, n2, n3, n4};
    return MakeNode(op, arraysize(buffer), buffer);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* buffer[] = {n1, n2, n3, n4, n5};
    return MakeNode(op, arraysize(buffer), buffer);
  }

  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return MakeNode(op, arraysize(nodes), nodes);
  }

  Node* NewNode(const Operator* op, int value_input_count,
                Node** value_inputs) {
    return MakeNode(op, value_input_count, value_inputs);
  }

  Handle<Code> GetCode() {
    Generate();
    return code_.ToHandleChecked();
  }

 protected:
  Node* MakeNode(const Operator* op, int value_input_count,
                 Node** value_inputs) {
    CHECK_EQ(op->ValueInputCount(), value_input_count);

    CHECK(!OperatorProperties::HasContextInput(op));
    CHECK(!OperatorProperties::HasFrameStateInput(op));
    bool has_control = op->ControlInputCount() == 1;
    bool has_effect = op->EffectInputCount() == 1;

    CHECK_LT(op->ControlInputCount(), 2);
    CHECK_LT(op->EffectInputCount(), 2);

    Node* result = NULL;
    if (!has_control && !has_effect) {
      result = graph()->NewNode(op, value_input_count, value_inputs);
    } else {
      int input_count_with_deps = value_input_count;
      if (has_control) ++input_count_with_deps;
      if (has_effect) ++input_count_with_deps;
      Node** buffer = zone()->template NewArray<Node*>(input_count_with_deps);
      memcpy(buffer, value_inputs, kPointerSize * value_input_count);
      Node** current_input = buffer + value_input_count;
      if (has_effect) {
        *current_input++ = effect_;
      }
      if (has_control) {
        *current_input++ = graph()->start();
      }
      result = graph()->NewNode(op, input_count_with_deps, buffer);
      if (has_effect) {
        effect_ = result;
      }
      // This graph builder does not support control flow.
      CHECK_EQ(0, op->ControlOutputCount());
    }

    return result;
  }

  virtual byte* Generate() {
    if (code_.is_null()) {
      Zone* zone = graph()->zone();
      CallDescriptor* desc =
          Linkage::GetSimplifiedCDescriptor(zone, this->csig_);
      CompilationInfo info(ArrayVector("testing"), main_isolate(), main_zone(),
                           Code::ComputeFlags(Code::STUB));
      code_ = Pipeline::GenerateCodeForTesting(&info, desc, graph());
#ifdef ENABLE_DISASSEMBLER
      if (!code_.is_null() && FLAG_print_opt_code) {
        OFStream os(stdout);
        code_.ToHandleChecked()->Disassemble("test code", os);
      }
#endif
    }
    return code_.ToHandleChecked()->entry();
  }

  void InitParameters() {
    int param_count = static_cast<int>(parameter_count());
    for (int i = 0; i < param_count; ++i) {
      parameters_[i] = this->NewNode(common()->Parameter(i), graph()->start());
    }
  }

  size_t parameter_count() const { return this->csig_->parameter_count(); }

 private:
  Node* effect_;
  Node* return_;
  Node** parameters_;
  MaybeHandle<Code> code_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_GRAPH_BUILDER_TESTER_H_
