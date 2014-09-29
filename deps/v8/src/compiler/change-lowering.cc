// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/change-lowering.h"

#include "src/compiler/common-node-cache.h"
#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

ChangeLoweringBase::ChangeLoweringBase(Graph* graph, Linkage* linkage,
                                       CommonNodeCache* cache)
    : graph_(graph),
      isolate_(graph->zone()->isolate()),
      linkage_(linkage),
      cache_(cache),
      common_(graph->zone()),
      machine_(graph->zone()) {}


ChangeLoweringBase::~ChangeLoweringBase() {}


Node* ChangeLoweringBase::ExternalConstant(ExternalReference reference) {
  Node** loc = cache()->FindExternalConstant(reference);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}


Node* ChangeLoweringBase::HeapConstant(PrintableUnique<HeapObject> value) {
  // TODO(bmeurer): Use common node cache.
  return graph()->NewNode(common()->HeapConstant(value));
}


Node* ChangeLoweringBase::ImmovableHeapConstant(Handle<HeapObject> value) {
  return HeapConstant(
      PrintableUnique<HeapObject>::CreateImmovable(graph()->zone(), value));
}


Node* ChangeLoweringBase::Int32Constant(int32_t value) {
  Node** loc = cache()->FindInt32Constant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->Int32Constant(value));
  }
  return *loc;
}


Node* ChangeLoweringBase::NumberConstant(double value) {
  Node** loc = cache()->FindNumberConstant(value);
  if (*loc == NULL) {
    *loc = graph()->NewNode(common()->NumberConstant(value));
  }
  return *loc;
}


Node* ChangeLoweringBase::CEntryStubConstant() {
  if (!c_entry_stub_constant_.is_set()) {
    c_entry_stub_constant_.set(
        ImmovableHeapConstant(CEntryStub(isolate(), 1).GetCode()));
  }
  return c_entry_stub_constant_.get();
}


Node* ChangeLoweringBase::TrueConstant() {
  if (!true_constant_.is_set()) {
    true_constant_.set(
        ImmovableHeapConstant(isolate()->factory()->true_value()));
  }
  return true_constant_.get();
}


Node* ChangeLoweringBase::FalseConstant() {
  if (!false_constant_.is_set()) {
    false_constant_.set(
        ImmovableHeapConstant(isolate()->factory()->false_value()));
  }
  return false_constant_.get();
}


Reduction ChangeLoweringBase::ChangeBitToBool(Node* val, Node* control) {
  Node* branch = graph()->NewNode(common()->Branch(), val, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* true_value = TrueConstant();

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* false_value = FalseConstant();

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi =
      graph()->NewNode(common()->Phi(2), true_value, false_value, merge);

  return Replace(phi);
}


template <size_t kPointerSize>
ChangeLowering<kPointerSize>::ChangeLowering(Graph* graph, Linkage* linkage)
    : ChangeLoweringBase(graph, linkage,
                         new (graph->zone()) CommonNodeCache(graph->zone())) {}


template <size_t kPointerSize>
Reduction ChangeLowering<kPointerSize>::Reduce(Node* node) {
  Node* control = graph()->start();
  Node* effect = control;
  switch (node->opcode()) {
    case IrOpcode::kChangeBitToBool:
      return ChangeBitToBool(node->InputAt(0), control);
    case IrOpcode::kChangeBoolToBit:
      return ChangeBoolToBit(node->InputAt(0));
    case IrOpcode::kChangeInt32ToTagged:
      return ChangeInt32ToTagged(node->InputAt(0), effect, control);
    case IrOpcode::kChangeTaggedToFloat64:
      return ChangeTaggedToFloat64(node->InputAt(0), effect, control);
    default:
      return NoChange();
  }
  UNREACHABLE();
  return NoChange();
}


template <>
Reduction ChangeLowering<4>::ChangeBoolToBit(Node* val) {
  return Replace(
      graph()->NewNode(machine()->Word32Equal(), val, TrueConstant()));
}


template <>
Reduction ChangeLowering<8>::ChangeBoolToBit(Node* val) {
  return Replace(
      graph()->NewNode(machine()->Word64Equal(), val, TrueConstant()));
}


template <>
Reduction ChangeLowering<4>::ChangeInt32ToTagged(Node* val, Node* effect,
                                                 Node* control) {
  Node* context = NumberConstant(0);

  Node* add = graph()->NewNode(machine()->Int32AddWithOverflow(), val, val);
  Node* ovf = graph()->NewNode(common()->Projection(1), add);

  Node* branch = graph()->NewNode(common()->Branch(), ovf, control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* number = graph()->NewNode(machine()->ChangeInt32ToFloat64(), val);

  // TODO(bmeurer): Inline allocation if possible.
  const Runtime::Function* fn =
      Runtime::FunctionForId(Runtime::kAllocateHeapNumber);
  DCHECK_EQ(0, fn->nargs);
  CallDescriptor* desc = linkage()->GetRuntimeCallDescriptor(
      fn->function_id, 0, Operator::kNoProperties);
  Node* heap_number =
      graph()->NewNode(common()->Call(desc), CEntryStubConstant(),
                       ExternalConstant(ExternalReference(fn, isolate())),
                       Int32Constant(0), context, effect, if_true);

  Node* store = graph()->NewNode(
      machine()->Store(kMachineFloat64, kNoWriteBarrier), heap_number,
      Int32Constant(HeapNumber::kValueOffset - kHeapObjectTag), number, effect,
      heap_number);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* smi = graph()->NewNode(common()->Projection(0), add);

  Node* merge = graph()->NewNode(common()->Merge(2), store, if_false);
  Node* phi = graph()->NewNode(common()->Phi(2), heap_number, smi, merge);

  return Replace(phi);
}


template <>
Reduction ChangeLowering<8>::ChangeInt32ToTagged(Node* val, Node* effect,
                                                 Node* control) {
  return Replace(graph()->NewNode(
      machine()->Word64Shl(), val,
      Int32Constant(SmiTagging<8>::kSmiShiftSize + kSmiTagSize)));
}


template <>
Reduction ChangeLowering<4>::ChangeTaggedToFloat64(Node* val, Node* effect,
                                                   Node* control) {
  Node* branch = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word32And(), val, Int32Constant(kSmiTagMask)),
      control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* load = graph()->NewNode(
      machine()->Load(kMachineFloat64), val,
      Int32Constant(HeapNumber::kValueOffset - kHeapObjectTag), if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* number = graph()->NewNode(
      machine()->ChangeInt32ToFloat64(),
      graph()->NewNode(
          machine()->Word32Sar(), val,
          Int32Constant(SmiTagging<4>::kSmiShiftSize + kSmiTagSize)));

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(2), load, number, merge);

  return Replace(phi);
}


template <>
Reduction ChangeLowering<8>::ChangeTaggedToFloat64(Node* val, Node* effect,
                                                   Node* control) {
  Node* branch = graph()->NewNode(
      common()->Branch(),
      graph()->NewNode(machine()->Word64And(), val, Int32Constant(kSmiTagMask)),
      control);

  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* load = graph()->NewNode(
      machine()->Load(kMachineFloat64), val,
      Int32Constant(HeapNumber::kValueOffset - kHeapObjectTag), if_true);

  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* number = graph()->NewNode(
      machine()->ChangeInt32ToFloat64(),
      graph()->NewNode(
          machine()->ConvertInt64ToInt32(),
          graph()->NewNode(
              machine()->Word64Sar(), val,
              Int32Constant(SmiTagging<8>::kSmiShiftSize + kSmiTagSize))));

  Node* merge = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common()->Phi(2), load, number, merge);

  return Replace(phi);
}


template class ChangeLowering<4>;
template class ChangeLowering<8>;

}  // namespace compiler
}  // namespace internal
}  // namespace v8
