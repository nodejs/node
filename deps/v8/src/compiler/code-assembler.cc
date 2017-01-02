// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-assembler.h"

#include <ostream>

#include "src/code-factory.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/frames.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/machine-type.h"
#include "src/macro-assembler.h"
#include "src/utils.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

CodeAssembler::CodeAssembler(Isolate* isolate, Zone* zone,
                             const CallInterfaceDescriptor& descriptor,
                             Code::Flags flags, const char* name,
                             size_t result_size)
    : CodeAssembler(
          isolate, zone,
          Linkage::GetStubCallDescriptor(
              isolate, zone, descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kNoFlags, Operator::kNoProperties,
              MachineType::AnyTagged(), result_size),
          flags, name) {}

CodeAssembler::CodeAssembler(Isolate* isolate, Zone* zone, int parameter_count,
                             Code::Flags flags, const char* name)
    : CodeAssembler(isolate, zone,
                    Linkage::GetJSCallDescriptor(zone, false, parameter_count,
                                                 CallDescriptor::kNoFlags),
                    flags, name) {}

CodeAssembler::CodeAssembler(Isolate* isolate, Zone* zone,
                             CallDescriptor* call_descriptor, Code::Flags flags,
                             const char* name)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone), call_descriptor,
          MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements())),
      flags_(flags),
      name_(name),
      code_generated_(false),
      variables_(zone) {}

CodeAssembler::~CodeAssembler() {}

void CodeAssembler::CallPrologue() {}

void CodeAssembler::CallEpilogue() {}

Handle<Code> CodeAssembler::GenerateCode() {
  DCHECK(!code_generated_);

  Schedule* schedule = raw_assembler_->Export();
  Handle<Code> code = Pipeline::GenerateCodeForCodeStub(
      isolate(), raw_assembler_->call_descriptor(), raw_assembler_->graph(),
      schedule, flags_, name_);

  code_generated_ = true;
  return code;
}

bool CodeAssembler::Is64() const { return raw_assembler_->machine()->Is64(); }

bool CodeAssembler::IsFloat64RoundUpSupported() const {
  return raw_assembler_->machine()->Float64RoundUp().IsSupported();
}

bool CodeAssembler::IsFloat64RoundDownSupported() const {
  return raw_assembler_->machine()->Float64RoundDown().IsSupported();
}

bool CodeAssembler::IsFloat64RoundTruncateSupported() const {
  return raw_assembler_->machine()->Float64RoundTruncate().IsSupported();
}

Node* CodeAssembler::Int32Constant(int32_t value) {
  return raw_assembler_->Int32Constant(value);
}

Node* CodeAssembler::Int64Constant(int64_t value) {
  return raw_assembler_->Int64Constant(value);
}

Node* CodeAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler_->IntPtrConstant(value);
}

Node* CodeAssembler::NumberConstant(double value) {
  return raw_assembler_->NumberConstant(value);
}

Node* CodeAssembler::SmiConstant(Smi* value) {
  return IntPtrConstant(bit_cast<intptr_t>(value));
}

Node* CodeAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler_->HeapConstant(object);
}

Node* CodeAssembler::BooleanConstant(bool value) {
  return raw_assembler_->BooleanConstant(value);
}

Node* CodeAssembler::ExternalConstant(ExternalReference address) {
  return raw_assembler_->ExternalConstant(address);
}

Node* CodeAssembler::Float64Constant(double value) {
  return raw_assembler_->Float64Constant(value);
}

Node* CodeAssembler::NaNConstant() {
  return LoadRoot(Heap::kNanValueRootIndex);
}

bool CodeAssembler::ToInt32Constant(Node* node, int32_t& out_value) {
  Int64Matcher m(node);
  if (m.HasValue() &&
      m.IsInRange(std::numeric_limits<int32_t>::min(),
                  std::numeric_limits<int32_t>::max())) {
    out_value = static_cast<int32_t>(m.Value());
    return true;
  }

  return false;
}

bool CodeAssembler::ToInt64Constant(Node* node, int64_t& out_value) {
  Int64Matcher m(node);
  if (m.HasValue()) out_value = m.Value();
  return m.HasValue();
}

bool CodeAssembler::ToIntPtrConstant(Node* node, intptr_t& out_value) {
  IntPtrMatcher m(node);
  if (m.HasValue()) out_value = m.Value();
  return m.HasValue();
}

Node* CodeAssembler::Parameter(int value) {
  return raw_assembler_->Parameter(value);
}

void CodeAssembler::Return(Node* value) {
  return raw_assembler_->Return(value);
}

void CodeAssembler::DebugBreak() { raw_assembler_->DebugBreak(); }

void CodeAssembler::Comment(const char* format, ...) {
  if (!FLAG_code_comments) return;
  char buffer[4 * KB];
  StringBuilder builder(buffer, arraysize(buffer));
  va_list arguments;
  va_start(arguments, format);
  builder.AddFormattedList(format, arguments);
  va_end(arguments);

  // Copy the string before recording it in the assembler to avoid
  // issues when the stack allocated buffer goes out of scope.
  const int prefix_len = 2;
  int length = builder.position() + 1;
  char* copy = reinterpret_cast<char*>(malloc(length + prefix_len));
  MemCopy(copy + prefix_len, builder.Finalize(), length);
  copy[0] = ';';
  copy[1] = ' ';
  raw_assembler_->Comment(copy);
}

void CodeAssembler::Bind(CodeAssembler::Label* label) { return label->Bind(); }

Node* CodeAssembler::LoadFramePointer() {
  return raw_assembler_->LoadFramePointer();
}

Node* CodeAssembler::LoadParentFramePointer() {
  return raw_assembler_->LoadParentFramePointer();
}

Node* CodeAssembler::LoadStackPointer() {
  return raw_assembler_->LoadStackPointer();
}

#define DEFINE_CODE_ASSEMBLER_BINARY_OP(name)   \
  Node* CodeAssembler::name(Node* a, Node* b) { \
    return raw_assembler_->name(a, b);          \
  }
CODE_ASSEMBLER_BINARY_OP_LIST(DEFINE_CODE_ASSEMBLER_BINARY_OP)
#undef DEFINE_CODE_ASSEMBLER_BINARY_OP

Node* CodeAssembler::WordShl(Node* value, int shift) {
  return (shift != 0) ? raw_assembler_->WordShl(value, IntPtrConstant(shift))
                      : value;
}

Node* CodeAssembler::WordShr(Node* value, int shift) {
  return (shift != 0) ? raw_assembler_->WordShr(value, IntPtrConstant(shift))
                      : value;
}

Node* CodeAssembler::Word32Shr(Node* value, int shift) {
  return (shift != 0) ? raw_assembler_->Word32Shr(value, IntPtrConstant(shift))
                      : value;
}

Node* CodeAssembler::ChangeUint32ToWord(Node* value) {
  if (raw_assembler_->machine()->Is64()) {
    value = raw_assembler_->ChangeUint32ToUint64(value);
  }
  return value;
}

Node* CodeAssembler::ChangeInt32ToIntPtr(Node* value) {
  if (raw_assembler_->machine()->Is64()) {
    value = raw_assembler_->ChangeInt32ToInt64(value);
  }
  return value;
}

#define DEFINE_CODE_ASSEMBLER_UNARY_OP(name) \
  Node* CodeAssembler::name(Node* a) { return raw_assembler_->name(a); }
CODE_ASSEMBLER_UNARY_OP_LIST(DEFINE_CODE_ASSEMBLER_UNARY_OP)
#undef DEFINE_CODE_ASSEMBLER_UNARY_OP

Node* CodeAssembler::Load(MachineType rep, Node* base) {
  return raw_assembler_->Load(rep, base);
}

Node* CodeAssembler::Load(MachineType rep, Node* base, Node* index) {
  return raw_assembler_->Load(rep, base, index);
}

Node* CodeAssembler::AtomicLoad(MachineType rep, Node* base, Node* index) {
  return raw_assembler_->AtomicLoad(rep, base, index);
}

Node* CodeAssembler::LoadRoot(Heap::RootListIndex root_index) {
  if (isolate()->heap()->RootCanBeTreatedAsConstant(root_index)) {
    Handle<Object> root = isolate()->heap()->root_handle(root_index);
    if (root->IsSmi()) {
      return SmiConstant(Smi::cast(*root));
    } else {
      return HeapConstant(Handle<HeapObject>::cast(root));
    }
  }

  Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  return Load(MachineType::AnyTagged(), roots_array_start,
              IntPtrConstant(root_index * kPointerSize));
}

Node* CodeAssembler::Store(MachineRepresentation rep, Node* base, Node* value) {
  return raw_assembler_->Store(rep, base, value, kFullWriteBarrier);
}

Node* CodeAssembler::Store(MachineRepresentation rep, Node* base, Node* index,
                           Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kFullWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* value) {
  return raw_assembler_->Store(rep, base, value, kNoWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* index, Node* value) {
  return raw_assembler_->Store(rep, base, index, value, kNoWriteBarrier);
}

Node* CodeAssembler::AtomicStore(MachineRepresentation rep, Node* base,
                                 Node* index, Node* value) {
  return raw_assembler_->AtomicStore(rep, base, index, value);
}

Node* CodeAssembler::StoreRoot(Heap::RootListIndex root_index, Node* value) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(root_index));
  Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  return StoreNoWriteBarrier(MachineRepresentation::kTagged, roots_array_start,
                             IntPtrConstant(root_index * kPointerSize), value);
}

Node* CodeAssembler::Projection(int index, Node* value) {
  return raw_assembler_->Projection(index, value);
}

void CodeAssembler::BranchIf(Node* condition, Label* if_true, Label* if_false) {
  Label if_condition_is_true(this), if_condition_is_false(this);
  Branch(condition, &if_condition_is_true, &if_condition_is_false);
  Bind(&if_condition_is_true);
  Goto(if_true);
  Bind(&if_condition_is_false);
  Goto(if_false);
}

void CodeAssembler::GotoIfException(Node* node, Label* if_exception,
                                    Variable* exception_var) {
  Label success(this), exception(this, Label::kDeferred);
  success.MergeVariables();
  exception.MergeVariables();
  DCHECK(!node->op()->HasProperty(Operator::kNoThrow));

  raw_assembler_->Continuations(node, success.label_, exception.label_);

  Bind(&exception);
  const Operator* op = raw_assembler_->common()->IfException();
  Node* exception_value = raw_assembler_->AddNode(op, node, node);
  if (exception_var != nullptr) {
    exception_var->Bind(exception_value);
  }
  Goto(if_exception);

  Bind(&success);
}

Node* CodeAssembler::CallN(CallDescriptor* descriptor, Node* code_target,
                           Node** args) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallN(descriptor, code_target, args);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::TailCallN(CallDescriptor* descriptor, Node* code_target,
                               Node** args) {
  return raw_assembler_->TailCallN(descriptor, code_target, args);
}

Node* CodeAssembler::CallRuntime(Runtime::FunctionId function_id,
                                 Node* context) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime0(function_id, context);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::CallRuntime(Runtime::FunctionId function_id, Node* context,
                                 Node* arg1) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime1(function_id, arg1, context);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::CallRuntime(Runtime::FunctionId function_id, Node* context,
                                 Node* arg1, Node* arg2) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime2(function_id, arg1, arg2, context);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::CallRuntime(Runtime::FunctionId function_id, Node* context,
                                 Node* arg1, Node* arg2, Node* arg3) {
  CallPrologue();
  Node* return_value =
      raw_assembler_->CallRuntime3(function_id, arg1, arg2, arg3, context);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::CallRuntime(Runtime::FunctionId function_id, Node* context,
                                 Node* arg1, Node* arg2, Node* arg3,
                                 Node* arg4) {
  CallPrologue();
  Node* return_value = raw_assembler_->CallRuntime4(function_id, arg1, arg2,
                                                    arg3, arg4, context);
  CallEpilogue();
  return return_value;
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context) {
  return raw_assembler_->TailCallRuntime0(function_id, context);
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1) {
  return raw_assembler_->TailCallRuntime1(function_id, arg1, context);
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2) {
  return raw_assembler_->TailCallRuntime2(function_id, arg1, arg2, context);
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3) {
  return raw_assembler_->TailCallRuntime3(function_id, arg1, arg2, arg3,
                                          context);
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3, Node* arg4) {
  return raw_assembler_->TailCallRuntime4(function_id, arg1, arg2, arg3, arg4,
                                          context);
}

Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function_id,
                                     Node* context, Node* arg1, Node* arg2,
                                     Node* arg3, Node* arg4, Node* arg5) {
  return raw_assembler_->TailCallRuntime5(function_id, arg1, arg2, arg3, arg4,
                                          arg5, context);
}

Node* CodeAssembler::CallStub(Callable const& callable, Node* context,
                              Node* arg1, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), target, context, arg1, result_size);
}

Node* CodeAssembler::CallStub(Callable const& callable, Node* context,
                              Node* arg1, Node* arg2, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), target, context, arg1, arg2,
                  result_size);
}

Node* CodeAssembler::CallStub(Callable const& callable, Node* context,
                              Node* arg1, Node* arg2, Node* arg3,
                              size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return CallStub(callable.descriptor(), target, context, arg1, arg2, arg3,
                  result_size);
}

Node* CodeAssembler::CallStubN(Callable const& callable, Node** args,
                               size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return CallStubN(callable.descriptor(), target, args, result_size);
}

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(1);
  args[0] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
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

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
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

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
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

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
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

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, Node* arg1,
                              Node* arg2, Node* arg3, Node* arg4, Node* arg5,
                              size_t result_size) {
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

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, const Arg& arg1,
                              const Arg& arg2, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 3;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, const Arg& arg1,
                              const Arg& arg2, const Arg& arg3,
                              size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 4;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[arg3.index] = arg3.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, const Arg& arg1,
                              const Arg& arg2, const Arg& arg3, const Arg& arg4,
                              size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 5;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[arg3.index] = arg3.value;
  args[arg4.index] = arg4.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallStub(const CallInterfaceDescriptor& descriptor,
                              Node* target, Node* context, const Arg& arg1,
                              const Arg& arg2, const Arg& arg3, const Arg& arg4,
                              const Arg& arg5, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 6;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[arg3.index] = arg3.value;
  args[arg4.index] = arg4.value;
  args[arg5.index] = arg5.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallStubN(const CallInterfaceDescriptor& descriptor,
                               Node* target, Node** args, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(Callable const& callable, Node* context,
                                  Node* arg1, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return TailCallStub(callable.descriptor(), target, context, arg1,
                      result_size);
}

Node* CodeAssembler::TailCallStub(Callable const& callable, Node* context,
                                  Node* arg1, Node* arg2, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return TailCallStub(callable.descriptor(), target, context, arg1, arg2,
                      result_size);
}

Node* CodeAssembler::TailCallStub(Callable const& callable, Node* context,
                                  Node* arg1, Node* arg2, Node* arg3,
                                  size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return TailCallStub(callable.descriptor(), target, context, arg1, arg2, arg3,
                      result_size);
}

Node* CodeAssembler::TailCallStub(Callable const& callable, Node* context,
                                  Node* arg1, Node* arg2, Node* arg3,
                                  Node* arg4, size_t result_size) {
  Node* target = HeapConstant(callable.code());
  return TailCallStub(callable.descriptor(), target, context, arg1, arg2, arg3,
                      arg4, result_size);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(2);
  args[0] = arg1;
  args[1] = context;

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(3);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = context;

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(4);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = context;

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, Node* arg1,
                                  Node* arg2, Node* arg3, Node* arg4,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node** args = zone()->NewArray<Node*>(5);
  args[0] = arg1;
  args[1] = arg2;
  args[2] = arg3;
  args[3] = arg4;
  args[4] = context;

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, const Arg& arg1,
                                  const Arg& arg2, const Arg& arg3,
                                  const Arg& arg4, size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 5;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[arg3.index] = arg3.value;
  args[arg4.index] = arg4.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, const Arg& arg1,
                                  const Arg& arg2, const Arg& arg3,
                                  const Arg& arg4, const Arg& arg5,
                                  size_t result_size) {
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  const int kArgsCount = 6;
  Node** args = zone()->NewArray<Node*>(kArgsCount);
  DCHECK((std::fill(&args[0], &args[kArgsCount], nullptr), true));
  args[arg1.index] = arg1.value;
  args[arg2.index] = arg2.value;
  args[arg3.index] = arg3.value;
  args[arg4.index] = arg4.value;
  args[arg5.index] = arg5.value;
  args[kArgsCount - 1] = context;
  DCHECK_EQ(0, std::count(&args[0], &args[kArgsCount], nullptr));

  return raw_assembler_->TailCallN(call_descriptor, target, args);
}

Node* CodeAssembler::TailCallBytecodeDispatch(
    const CallInterfaceDescriptor& interface_descriptor,
    Node* code_target_address, Node** args) {
  CallDescriptor* descriptor = Linkage::GetBytecodeDispatchCallDescriptor(
      isolate(), zone(), interface_descriptor,
      interface_descriptor.GetStackParameterCount());
  return raw_assembler_->TailCallN(descriptor, code_target_address, args);
}

Node* CodeAssembler::CallJS(Callable const& callable, Node* context,
                            Node* function, Node* receiver,
                            size_t result_size) {
  const int argc = 0;
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), argc + 1,
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);
  Node* target = HeapConstant(callable.code());

  Node** args = zone()->NewArray<Node*>(argc + 4);
  args[0] = function;
  args[1] = Int32Constant(argc);
  args[2] = receiver;
  args[3] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallJS(Callable const& callable, Node* context,
                            Node* function, Node* receiver, Node* arg1,
                            size_t result_size) {
  const int argc = 1;
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), argc + 1,
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);
  Node* target = HeapConstant(callable.code());

  Node** args = zone()->NewArray<Node*>(argc + 4);
  args[0] = function;
  args[1] = Int32Constant(argc);
  args[2] = receiver;
  args[3] = arg1;
  args[4] = context;

  return CallN(call_descriptor, target, args);
}

Node* CodeAssembler::CallJS(Callable const& callable, Node* context,
                            Node* function, Node* receiver, Node* arg1,
                            Node* arg2, size_t result_size) {
  const int argc = 2;
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), zone(), callable.descriptor(), argc + 1,
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);
  Node* target = HeapConstant(callable.code());

  Node** args = zone()->NewArray<Node*>(argc + 4);
  args[0] = function;
  args[1] = Int32Constant(argc);
  args[2] = receiver;
  args[3] = arg1;
  args[4] = arg2;
  args[5] = context;

  return CallN(call_descriptor, target, args);
}

void CodeAssembler::Goto(CodeAssembler::Label* label) {
  label->MergeVariables();
  raw_assembler_->Goto(label->label_);
}

void CodeAssembler::GotoIf(Node* condition, Label* true_label) {
  Label false_label(this);
  Branch(condition, true_label, &false_label);
  Bind(&false_label);
}

void CodeAssembler::GotoUnless(Node* condition, Label* false_label) {
  Label true_label(this);
  Branch(condition, &true_label, false_label);
  Bind(&true_label);
}

void CodeAssembler::Branch(Node* condition, CodeAssembler::Label* true_label,
                           CodeAssembler::Label* false_label) {
  true_label->MergeVariables();
  false_label->MergeVariables();
  return raw_assembler_->Branch(condition, true_label->label_,
                                false_label->label_);
}

void CodeAssembler::Switch(Node* index, Label* default_label,
                           const int32_t* case_values, Label** case_labels,
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

Node* CodeAssembler::Select(Node* condition, Node* true_value,
                            Node* false_value, MachineRepresentation rep) {
  Variable value(this, rep);
  Label vtrue(this), vfalse(this), end(this);
  Branch(condition, &vtrue, &vfalse);

  Bind(&vtrue);
  {
    value.Bind(true_value);
    Goto(&end);
  }
  Bind(&vfalse);
  {
    value.Bind(false_value);
    Goto(&end);
  }

  Bind(&end);
  return value.value();
}

// RawMachineAssembler delegate helpers:
Isolate* CodeAssembler::isolate() const { return raw_assembler_->isolate(); }

Factory* CodeAssembler::factory() const { return isolate()->factory(); }

Zone* CodeAssembler::zone() const { return raw_assembler_->zone(); }

// The core implementation of Variable is stored through an indirection so
// that it can outlive the often block-scoped Variable declarations. This is
// needed to ensure that variable binding and merging through phis can
// properly be verified.
class CodeAssembler::Variable::Impl : public ZoneObject {
 public:
  explicit Impl(MachineRepresentation rep) : value_(nullptr), rep_(rep) {}
  Node* value_;
  MachineRepresentation rep_;
};

CodeAssembler::Variable::Variable(CodeAssembler* assembler,
                                  MachineRepresentation rep)
    : impl_(new (assembler->zone()) Impl(rep)), assembler_(assembler) {
  assembler->variables_.insert(impl_);
}

CodeAssembler::Variable::~Variable() { assembler_->variables_.erase(impl_); }

void CodeAssembler::Variable::Bind(Node* value) { impl_->value_ = value; }

Node* CodeAssembler::Variable::value() const {
  DCHECK_NOT_NULL(impl_->value_);
  return impl_->value_;
}

MachineRepresentation CodeAssembler::Variable::rep() const {
  return impl_->rep_;
}

bool CodeAssembler::Variable::IsBound() const {
  return impl_->value_ != nullptr;
}

CodeAssembler::Label::Label(CodeAssembler* assembler, int merged_value_count,
                            CodeAssembler::Variable** merged_variables,
                            CodeAssembler::Label::Type type)
    : bound_(false), merge_count_(0), assembler_(assembler), label_(nullptr) {
  void* buffer = assembler->zone()->New(sizeof(RawMachineLabel));
  label_ = new (buffer)
      RawMachineLabel(type == kDeferred ? RawMachineLabel::kDeferred
                                        : RawMachineLabel::kNonDeferred);
  for (int i = 0; i < merged_value_count; ++i) {
    variable_phis_[merged_variables[i]->impl_] = nullptr;
  }
}

void CodeAssembler::Label::MergeVariables() {
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
        if (i != variable_merges_.end()) {
          // If the following assert fires, then you've declared a variable that
          // has the same bound value along all paths up until the point you
          // bound this label, but then later merged a path with a new value for
          // the variable after the label bind (it's not possible to add phis to
          // the bound label after the fact, just make sure to list the variable
          // in the label's constructor's list of merged variables).
          DCHECK(find_if(i->second.begin(), i->second.end(),
                         [node](Node* e) -> bool { return node != e; }) ==
                 i->second.end());
        }
      }
    }
  }
}

void CodeAssembler::Label::Bind() {
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
    CodeAssembler::Variable::Impl* var_impl = var.first;
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
