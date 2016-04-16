// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-stub-assembler.h"

#include <ostream>

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

CodeStubAssembler::CodeStubAssembler(Isolate* isolate, Zone* zone,
                                     const CallInterfaceDescriptor& descriptor,
                                     Code::Flags flags, const char* name,
                                     size_t result_size)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone),
          Linkage::GetStubCallDescriptor(
              isolate, zone, descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kNoFlags, Operator::kNoProperties,
              MachineType::AnyTagged(), result_size))),
      flags_(flags),
      name_(name),
      code_generated_(false),
      variables_(zone) {}

CodeStubAssembler::~CodeStubAssembler() {}

void CodeStubAssembler::CallPrologue() {}

void CodeStubAssembler::CallEpilogue() {}

Handle<Code> CodeStubAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  Schedule* schedule = raw_assembler_->Export();
  Handle<Code> code = Pipeline::GenerateCodeForCodeStub(
      isolate(), raw_assembler_->call_descriptor(), graph(), schedule, flags_,
      name_);

  code_generated_ = true;
  return code;
}


Node* CodeStubAssembler::Int32Constant(int value) {
  return raw_assembler_->Int32Constant(value);
}


Node* CodeStubAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler_->IntPtrConstant(value);
}


Node* CodeStubAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}


Node* CodeStubAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}


Node* CodeStubAssembler::BooleanConstant(bool value) {
  return raw_assembler_->BooleanConstant(value);
}

Node* CodeStubAssembler::ExternalConstant(ExternalReference address) {
  return raw_assembler_->ExternalConstant(address);
}

Node* CodeStubAssembler::Parameter(int value) {
  return raw_assembler_->Parameter(value);
}


void CodeStubAssembler::Return(Node* value) {
  return raw_assembler_->Return(value);
}

void CodeStubAssembler::Bind(CodeStubAssembler::Label* label) {
  return label->Bind();
}

Node* CodeStubAssembler::LoadFramePointer() {
  return raw_assembler_->LoadFramePointer();
}

Node* CodeStubAssembler::LoadParentFramePointer() {
  return raw_assembler_->LoadParentFramePointer();
}

Node* CodeStubAssembler::LoadStackPointer() {
  return raw_assembler_->LoadStackPointer();
}

Node* CodeStubAssembler::SmiShiftBitsConstant() {
  return Int32Constant(kSmiShiftSize + kSmiTagSize);
}


Node* CodeStubAssembler::SmiTag(Node* value) {
  return raw_assembler_->WordShl(value, SmiShiftBitsConstant());
}


Node* CodeStubAssembler::SmiUntag(Node* value) {
  return raw_assembler_->WordSar(value, SmiShiftBitsConstant());
}

#define DEFINE_CODE_STUB_ASSEMBER_BINARY_OP(name)   \
  Node* CodeStubAssembler::name(Node* a, Node* b) { \
    return raw_assembler_->name(a, b);              \
  }
CODE_STUB_ASSEMBLER_BINARY_OP_LIST(DEFINE_CODE_STUB_ASSEMBER_BINARY_OP)
#undef DEFINE_CODE_STUB_ASSEMBER_BINARY_OP

Node* CodeStubAssembler::ChangeInt32ToInt64(Node* value) {
  return raw_assembler_->ChangeInt32ToInt64(value);
}

Node* CodeStubAssembler::WordShl(Node* value, int shift) {
  return raw_assembler_->WordShl(value, Int32Constant(shift));
}

Node* CodeStubAssembler::WordIsSmi(Node* a) {
  return WordEqual(raw_assembler_->WordAnd(a, Int32Constant(kSmiTagMask)),
                   Int32Constant(0));
}

Node* CodeStubAssembler::LoadBufferObject(Node* buffer, int offset) {
  return raw_assembler_->Load(MachineType::AnyTagged(), buffer,
                              IntPtrConstant(offset));
}

Node* CodeStubAssembler::LoadObjectField(Node* object, int offset) {
  return raw_assembler_->Load(MachineType::AnyTagged(), object,
                              IntPtrConstant(offset - kHeapObjectTag));
}

Node* CodeStubAssembler::LoadFixedArrayElementSmiIndex(Node* object,
                                                       Node* smi_index,
                                                       int additional_offset) {
  Node* header_size = raw_assembler_->Int32Constant(
      additional_offset + FixedArray::kHeaderSize - kHeapObjectTag);
  Node* scaled_index =
      (kSmiShiftSize == 0)
          ? raw_assembler_->Word32Shl(
                smi_index, Int32Constant(kPointerSizeLog2 - kSmiTagSize))
          : raw_assembler_->Word32Shl(SmiUntag(smi_index),
                                      Int32Constant(kPointerSizeLog2));
  Node* offset = raw_assembler_->Int32Add(scaled_index, header_size);
  return raw_assembler_->Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadFixedArrayElementConstantIndex(Node* object,
                                                            int index) {
  Node* offset = raw_assembler_->Int32Constant(
      FixedArray::kHeaderSize - kHeapObjectTag + index * kPointerSize);
  return raw_assembler_->Load(MachineType::AnyTagged(), object, offset);
}

Node* CodeStubAssembler::LoadRoot(Heap::RootListIndex root_index) {
  if (isolate()->heap()->RootCanBeTreatedAsConstant(root_index)) {
    Handle<Object> root = isolate()->heap()->root_handle(root_index);
    if (root->IsSmi()) {
      return Int32Constant(Handle<Smi>::cast(root)->value());
    } else {
      return HeapConstant(Handle<HeapObject>::cast(root));
    }
  }

  compiler::Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  USE(roots_array_start);

  // TODO(danno): Implement thee root-access case where the root is not constant
  // and must be loaded from the root array.
  UNIMPLEMENTED();
  return nullptr;
}

Node* CodeStubAssembler::Load(MachineType rep, Node* base) {
  return raw_assembler_->Load(rep, base);
}

Node* CodeStubAssembler::Load(MachineType rep, Node* base, Node* index) {
  return raw_assembler_->Load(rep, base, index);
}

Node* CodeStubAssembler::Store(MachineRepresentation rep, Node* base,
                               Node* value) {
  return raw_assembler_->Store(rep, base, value, kFullWriteBarrier);
}

Node* CodeStubAssembler::Store(MachineRepresentation rep, Node* base,
                               Node* index, Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kFullWriteBarrier);
}

Node* CodeStubAssembler::StoreNoWriteBarrier(MachineRepresentation rep,
                                             Node* base, Node* value) {
  return raw_assembler_->Store(rep, base, value, kNoWriteBarrier);
}

Node* CodeStubAssembler::StoreNoWriteBarrier(MachineRepresentation rep,
                                             Node* base, Node* index,
                                             Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kNoWriteBarrier);
}

Node* CodeStubAssembler::Projection(int index, Node* value) {
  return raw_assembler_->Projection(index, value);
}

Node* CodeStubAssembler::CallN(CallDescriptor* descriptor, Node* code_target,
                               Node** args) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallN(descriptor, code_target, args);
  CallEpilogue();
  return return_value;
}


Node* CodeStubAssembler::TailCallN(CallDescriptor* descriptor,
                                   Node* code_target, Node** args) {
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime0(function_id, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime1(function_id, arg1, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime2(function_id, arg1, arg2, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime3(function_id, arg1, arg2, arg3, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::CallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3, Node* arg4) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime4(function_id, arg1, arg2,
                                                    arg3, arg4, context);
  CallEpilogue();
  return return_value;
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1) {
  return raw_assembler_->TailCallRuntime1(function_id, arg1, context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1,
                                         Node* arg2) {
  return raw_assembler_->TailCallRuntime2(function_id, arg1, arg2, context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1, Node* arg2,
                                         Node* arg3) {
  return raw_assembler_->TailCallRuntime3(function_id, arg1, arg2, arg3,
                                          context);
}

Node* CodeStubAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                         Node* context, Node* arg1, Node* arg2,
                                         Node* arg3, Node* arg4) {
  return raw_assembler_->TailCallRuntime4(function_id, arg1, arg2, arg3, arg4,
                                          context);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(2);
  args[0] = arg1;
  args[1] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(3);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, Node* arg4,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, Node* arg4,
                                  Node* arg5, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(6);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = arg5;
  args[5] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeStubAssembler::TailCallStub(CodeStub& stub, Node** args) {
  Node* code_target = HeapConstant(stub.GetCode());
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), stub.GetCallInterfaceDescriptor(),
      stub.GetStackParameterCount(), CallDescriptor::kSupportsTailCalls);
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

Node* CodeStubAssembler::TailCall(
    const CallInterfaceDescriptor& interface_descriptor, Node* code_target,
    Node** args, size_t result_size) {
  CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), interface_descriptor,
      interface_descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

void CodeStubAssembler::Goto(CodeStubAssembler::Label* label) {
  label->MergeVariables();
  raw_assembler_->Goto(label->label_);
}

void CodeStubAssembler::Branch(Node* condition,
                               CodeStubAssembler::Label* true_label,
                               CodeStubAssembler::Label* false_label) {
  true_label->MergeVariables();
  false_label->MergeVariables();
  return raw_assembler_->Branch(condition, true_label->label_,
                                false_label->label_);
}

void CodeStubAssembler::Switch(Node* index, Label* default_label,
                               int32_t* case_values, Label** case_labels,
                               size_t case_count) {
  RawMachineLabel** labels =
      new (zone()->New(sizeof(RawMachineLabel*) * case_count))
          RawMachineLabel*[case_count];
  for (size_t i = 0; i < case_count; ++i) {
    labels[i] = case_labels[i]->label_;
    case_labels[i]->MergeVariables();
    default_label->MergeVariables();
  }
  return raw_assembler_->Switch(index, default_label->label_, case_values,
                                labels, case_count);
}

// RawMachineAssembler delegate helpers:
Isolate* CodeStubAssembler::isolate() { return raw_assembler_->isolate(); }

Graph* CodeStubAssembler::graph() { return raw_assembler_->graph(); }

Zone* CodeStubAssembler::zone() { return raw_assembler_->zone(); }

// The core implementation of Variable is stored through an indirection so
// that it can outlive the often block-scoped Variable declarations. This is
// needed to ensure that variable binding and merging through phis can
// properly be verified.
class CodeStubAssembler::Variable::Impl : public ZoneObject {
 public:
  explicit Impl(MachineRepresentation rep) : value_(nullptr), rep_(rep) {}
  Node* value_;
  MachineRepresentation rep_;
};

CodeStubAssembler::Variable::Variable(CodeStubAssembler* assembler,
                                      MachineRepresentation rep)
    : impl_(new (assembler->zone()) Impl(rep)) {
  assembler->variables_.push_back(impl_);
}

void CodeStubAssembler::Variable::Bind(Node* value) { impl_->value_ = value; }

Node* CodeStubAssembler::Variable::value() const {
  DCHECK_NOT_NULL(impl_->value_);
  return impl_->value_;
}

MachineRepresentation CodeStubAssembler::Variable::rep() const {
  return impl_->rep_;
}

bool CodeStubAssembler::Variable::IsBound() const {
  return impl_->value_ != nullptr;
}

CodeStubAssembler::Label::Label(CodeStubAssembler* assembler)
    : bound_(false), merge_count_(0), assembler_(assembler), label_(nullptr) {
  void* buffer = assembler->zone()->New(sizeof(RawMachineLabel));
  label_ = new (buffer) RawMachineLabel();
}

CodeStubAssembler::Label::Label(CodeStubAssembler* assembler,
                                int merged_value_count,
                                CodeStubAssembler::Variable** merged_variables)
    : bound_(false), merge_count_(0), assembler_(assembler), label_(nullptr) {
  void* buffer = assembler->zone()->New(sizeof(RawMachineLabel));
  label_ = new (buffer) RawMachineLabel();
  for (int i = 0; i < merged_value_count; ++i) {
    variable_phis_[merged_variables[i]->impl_] = nullptr;
  }
}

CodeStubAssembler::Label::Label(CodeStubAssembler* assembler,
                                CodeStubAssembler::Variable* merged_variable)
    : CodeStubAssembler::Label(assembler, 1, &merged_variable) {}

void CodeStubAssembler::Label::MergeVariables() {
  ++merge_count_;
  for (auto var : assembler_->variables_) {
    size_t count = 0;
    Node* node = var->value_;
    if (node != nullptr) {
      auto i = variable_merges_.find(var);
      if (i != variable_merges_.end()) {
        i->second.push_back(node);
        count = i->second.size();
      } else {
        count = 1;
        variable_merges_[var] = std::vector<Node*>(1, node);
      }
    }
    // If the following asserts, then you've jumped to a label without a bound
    // variable along that path that expects to merge its value into a phi.
    DCHECK(variable_phis_.find(var) == variable_phis_.end() ||
           count == merge_count_);
    USE(count);

    // If the label is already bound, we already know the set of variables to
    // merge and phi nodes have already been created.
    if (bound_) {
      auto phi = variable_phis_.find(var);
      if (phi != variable_phis_.end()) {
        DCHECK_NOT_NULL(phi->second);
        assembler_->raw_assembler_->AppendPhiInput(phi->second, node);
      } else {
        auto i = variable_merges_.find(var);
        USE(i);
        // If the following assert fires, then you've declared a variable that
        // has the same bound value along all paths up until the point you bound
        // this label, but then later merged a path with a new value for the
        // variable after the label bind (it's not possible to add phis to the
        // bound label after the fact, just make sure to list the variable in
        // the label's constructor's list of merged variables).
        DCHECK(find_if(i->second.begin(), i->second.end(),
                       [node](Node* e) -> bool { return node != e; }) ==
               i->second.end());
      }
    }
  }
}

void CodeStubAssembler::Label::Bind() {
  DCHECK(!bound_);
  assembler_->raw_assembler_->Bind(label_);

  // Make sure that all variables that have changed along any path up to this
  // point are marked as merge variables.
  for (auto var : assembler_->variables_) {
    Node* shared_value = nullptr;
    auto i = variable_merges_.find(var);
    if (i != variable_merges_.end()) {
      for (auto value : i->second) {
        DCHECK(value != nullptr);
        if (value != shared_value) {
          if (shared_value == nullptr) {
            shared_value = value;
          } else {
            variable_phis_[var] = nullptr;
          }
        }
      }
    }
  }

  for (auto var : variable_phis_) {
    CodeStubAssembler::Variable::Impl* var_impl = var.first;
    auto i = variable_merges_.find(var_impl);
    // If the following assert fires, then a variable that has been marked as
    // being merged at the label--either by explicitly marking it so in the
    // label constructor or by having seen different bound values at branches
    // into the label--doesn't have a bound value along all of the paths that
    // have been merged into the label up to this point.
    DCHECK(i != variable_merges_.end() && i->second.size() == merge_count_);
    Node* phi = assembler_->raw_assembler_->Phi(
        var.first->rep_, static_cast<int>(merge_count_), &(i->second[0]));
    variable_phis_[var_impl] = phi;
  }

  // Bind all variables to a merge phi, the common value along all paths or
  // null.
  for (auto var : assembler_->variables_) {
    auto i = variable_phis_.find(var);
    if (i != variable_phis_.end()) {
      var->value_ = i->second;
    } else {
      auto j = variable_merges_.find(var);
      if (j != variable_merges_.end() && j->second.size() == merge_count_) {
        var->value_ = j->second.back();
      } else {
        var->value_ = nullptr;
      }
    }
  }

  bound_ = true;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
