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
#include "src/zone/zone.h"

#define REPEAT_1_TO_2(V, T) V(T) V(T, T)
#define REPEAT_1_TO_3(V, T) REPEAT_1_TO_2(V, T) V(T, T, T)
#define REPEAT_1_TO_4(V, T) REPEAT_1_TO_3(V, T) V(T, T, T, T)
#define REPEAT_1_TO_5(V, T) REPEAT_1_TO_4(V, T) V(T, T, T, T, T)
#define REPEAT_1_TO_6(V, T) REPEAT_1_TO_5(V, T) V(T, T, T, T, T, T)
#define REPEAT_1_TO_7(V, T) REPEAT_1_TO_6(V, T) V(T, T, T, T, T, T, T)
#define REPEAT_1_TO_8(V, T) REPEAT_1_TO_7(V, T) V(T, T, T, T, T, T, T, T)
#define REPEAT_1_TO_9(V, T) REPEAT_1_TO_8(V, T) V(T, T, T, T, T, T, T, T, T)

namespace v8 {
namespace internal {
namespace compiler {

CodeAssemblerState::CodeAssemblerState(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    Code::Flags flags, const char* name, size_t result_size)
    : CodeAssemblerState(
          isolate, zone,
          Linkage::GetStubCallDescriptor(
              isolate, zone, descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kNoFlags, Operator::kNoProperties,
              MachineType::AnyTagged(), result_size),
          flags, name) {}

CodeAssemblerState::CodeAssemblerState(Isolate* isolate, Zone* zone,
                                       int parameter_count, Code::Flags flags,
                                       const char* name)
    : CodeAssemblerState(isolate, zone,
                         Linkage::GetJSCallDescriptor(
                             zone, false, parameter_count,
                             Code::ExtractKindFromFlags(flags) == Code::BUILTIN
                                 ? CallDescriptor::kPushArgumentCount
                                 : CallDescriptor::kNoFlags),
                         flags, name) {}

CodeAssemblerState::CodeAssemblerState(Isolate* isolate, Zone* zone,
                                       CallDescriptor* call_descriptor,
                                       Code::Flags flags, const char* name)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone), call_descriptor,
          MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements())),
      flags_(flags),
      name_(name),
      code_generated_(false),
      variables_(zone) {}

CodeAssemblerState::~CodeAssemblerState() {}

int CodeAssemblerState::parameter_count() const {
  return static_cast<int>(raw_assembler_->call_descriptor()->ParameterCount());
}

CodeAssembler::~CodeAssembler() {}

class BreakOnNodeDecorator final : public GraphDecorator {
 public:
  explicit BreakOnNodeDecorator(NodeId node_id) : node_id_(node_id) {}

  void Decorate(Node* node) final {
    if (node->id() == node_id_) {
      base::OS::DebugBreak();
    }
  }

 private:
  NodeId node_id_;
};

void CodeAssembler::BreakOnNode(int node_id) {
  Graph* graph = raw_assembler()->graph();
  Zone* zone = graph->zone();
  GraphDecorator* decorator =
      new (zone) BreakOnNodeDecorator(static_cast<NodeId>(node_id));
  graph->AddDecorator(decorator);
}

void CodeAssembler::RegisterCallGenerationCallbacks(
    const CodeAssemblerCallback& call_prologue,
    const CodeAssemblerCallback& call_epilogue) {
  // The callback can be registered only once.
  DCHECK(!state_->call_prologue_);
  DCHECK(!state_->call_epilogue_);
  state_->call_prologue_ = call_prologue;
  state_->call_epilogue_ = call_epilogue;
}

void CodeAssembler::UnregisterCallGenerationCallbacks() {
  state_->call_prologue_ = nullptr;
  state_->call_epilogue_ = nullptr;
}

void CodeAssembler::CallPrologue() {
  if (state_->call_prologue_) {
    state_->call_prologue_();
  }
}

void CodeAssembler::CallEpilogue() {
  if (state_->call_epilogue_) {
    state_->call_epilogue_();
  }
}

// static
Handle<Code> CodeAssembler::GenerateCode(CodeAssemblerState* state) {
  DCHECK(!state->code_generated_);

  RawMachineAssembler* rasm = state->raw_assembler_.get();
  Schedule* schedule = rasm->Export();
  Handle<Code> code = Pipeline::GenerateCodeForCodeStub(
      rasm->isolate(), rasm->call_descriptor(), rasm->graph(), schedule,
      state->flags_, state->name_);

  state->code_generated_ = true;
  return code;
}

bool CodeAssembler::Is64() const { return raw_assembler()->machine()->Is64(); }

bool CodeAssembler::IsFloat64RoundUpSupported() const {
  return raw_assembler()->machine()->Float64RoundUp().IsSupported();
}

bool CodeAssembler::IsFloat64RoundDownSupported() const {
  return raw_assembler()->machine()->Float64RoundDown().IsSupported();
}

bool CodeAssembler::IsFloat64RoundTiesEvenSupported() const {
  return raw_assembler()->machine()->Float64RoundTiesEven().IsSupported();
}

bool CodeAssembler::IsFloat64RoundTruncateSupported() const {
  return raw_assembler()->machine()->Float64RoundTruncate().IsSupported();
}

Node* CodeAssembler::Int32Constant(int32_t value) {
  return raw_assembler()->Int32Constant(value);
}

Node* CodeAssembler::Int64Constant(int64_t value) {
  return raw_assembler()->Int64Constant(value);
}

Node* CodeAssembler::IntPtrConstant(intptr_t value) {
  return raw_assembler()->IntPtrConstant(value);
}

Node* CodeAssembler::NumberConstant(double value) {
  return raw_assembler()->NumberConstant(value);
}

Node* CodeAssembler::SmiConstant(Smi* value) {
  return BitcastWordToTaggedSigned(IntPtrConstant(bit_cast<intptr_t>(value)));
}

Node* CodeAssembler::SmiConstant(int value) {
  return SmiConstant(Smi::FromInt(value));
}

Node* CodeAssembler::HeapConstant(Handle<HeapObject> object) {
  return raw_assembler()->HeapConstant(object);
}

Node* CodeAssembler::BooleanConstant(bool value) {
  return raw_assembler()->BooleanConstant(value);
}

Node* CodeAssembler::ExternalConstant(ExternalReference address) {
  return raw_assembler()->ExternalConstant(address);
}

Node* CodeAssembler::Float64Constant(double value) {
  return raw_assembler()->Float64Constant(value);
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

bool CodeAssembler::ToSmiConstant(Node* node, Smi*& out_value) {
  if (node->opcode() == IrOpcode::kBitcastWordToTaggedSigned) {
    node = node->InputAt(0);
  } else {
    return false;
  }
  IntPtrMatcher m(node);
  if (m.HasValue()) {
    out_value = Smi::cast(bit_cast<Object*>(m.Value()));
    return true;
  }
  return false;
}

bool CodeAssembler::ToIntPtrConstant(Node* node, intptr_t& out_value) {
  if (node->opcode() == IrOpcode::kBitcastWordToTaggedSigned ||
      node->opcode() == IrOpcode::kBitcastWordToTagged) {
    node = node->InputAt(0);
  }
  IntPtrMatcher m(node);
  if (m.HasValue()) out_value = m.Value();
  return m.HasValue();
}

Node* CodeAssembler::Parameter(int value) {
  return raw_assembler()->Parameter(value);
}

void CodeAssembler::Return(Node* value) {
  return raw_assembler()->Return(value);
}

void CodeAssembler::PopAndReturn(Node* pop, Node* value) {
  return raw_assembler()->PopAndReturn(pop, value);
}

void CodeAssembler::DebugBreak() { raw_assembler()->DebugBreak(); }

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
  raw_assembler()->Comment(copy);
}

void CodeAssembler::Bind(Label* label) { return label->Bind(); }

Node* CodeAssembler::LoadFramePointer() {
  return raw_assembler()->LoadFramePointer();
}

Node* CodeAssembler::LoadParentFramePointer() {
  return raw_assembler()->LoadParentFramePointer();
}

Node* CodeAssembler::LoadStackPointer() {
  return raw_assembler()->LoadStackPointer();
}

#define DEFINE_CODE_ASSEMBLER_BINARY_OP(name)   \
  Node* CodeAssembler::name(Node* a, Node* b) { \
    return raw_assembler()->name(a, b);         \
  }
CODE_ASSEMBLER_BINARY_OP_LIST(DEFINE_CODE_ASSEMBLER_BINARY_OP)
#undef DEFINE_CODE_ASSEMBLER_BINARY_OP

Node* CodeAssembler::IntPtrAdd(Node* left, Node* right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant + right_constant);
    }
    if (left_constant == 0) {
      return right;
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return raw_assembler()->IntPtrAdd(left, right);
}

Node* CodeAssembler::IntPtrSub(Node* left, Node* right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant - right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return raw_assembler()->IntPtrSub(left, right);
}

Node* CodeAssembler::WordShl(Node* value, int shift) {
  return (shift != 0) ? raw_assembler()->WordShl(value, IntPtrConstant(shift))
                      : value;
}

Node* CodeAssembler::WordShr(Node* value, int shift) {
  return (shift != 0) ? raw_assembler()->WordShr(value, IntPtrConstant(shift))
                      : value;
}

Node* CodeAssembler::Word32Shr(Node* value, int shift) {
  return (shift != 0) ? raw_assembler()->Word32Shr(value, Int32Constant(shift))
                      : value;
}

Node* CodeAssembler::ChangeUint32ToWord(Node* value) {
  if (raw_assembler()->machine()->Is64()) {
    value = raw_assembler()->ChangeUint32ToUint64(value);
  }
  return value;
}

Node* CodeAssembler::ChangeInt32ToIntPtr(Node* value) {
  if (raw_assembler()->machine()->Is64()) {
    value = raw_assembler()->ChangeInt32ToInt64(value);
  }
  return value;
}

Node* CodeAssembler::RoundIntPtrToFloat64(Node* value) {
  if (raw_assembler()->machine()->Is64()) {
    return raw_assembler()->RoundInt64ToFloat64(value);
  }
  return raw_assembler()->ChangeInt32ToFloat64(value);
}

#define DEFINE_CODE_ASSEMBLER_UNARY_OP(name) \
  Node* CodeAssembler::name(Node* a) { return raw_assembler()->name(a); }
CODE_ASSEMBLER_UNARY_OP_LIST(DEFINE_CODE_ASSEMBLER_UNARY_OP)
#undef DEFINE_CODE_ASSEMBLER_UNARY_OP

Node* CodeAssembler::Load(MachineType rep, Node* base) {
  return raw_assembler()->Load(rep, base);
}

Node* CodeAssembler::Load(MachineType rep, Node* base, Node* offset) {
  return raw_assembler()->Load(rep, base, offset);
}

Node* CodeAssembler::AtomicLoad(MachineType rep, Node* base, Node* offset) {
  return raw_assembler()->AtomicLoad(rep, base, offset);
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

Node* CodeAssembler::Store(Node* base, Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, value,
                                kFullWriteBarrier);
}

Node* CodeAssembler::Store(Node* base, Node* offset, Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, offset,
                                value, kFullWriteBarrier);
}

Node* CodeAssembler::StoreWithMapWriteBarrier(Node* base, Node* offset,
                                              Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, offset,
                                value, kMapWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* value) {
  return raw_assembler()->Store(rep, base, value, kNoWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* offset, Node* value) {
  return raw_assembler()->Store(rep, base, offset, value, kNoWriteBarrier);
}

Node* CodeAssembler::AtomicStore(MachineRepresentation rep, Node* base,
                                 Node* offset, Node* value) {
  return raw_assembler()->AtomicStore(rep, base, offset, value);
}

Node* CodeAssembler::StoreRoot(Heap::RootListIndex root_index, Node* value) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(root_index));
  Node* roots_array_start =
      ExternalConstant(ExternalReference::roots_array_start(isolate()));
  return StoreNoWriteBarrier(MachineRepresentation::kTagged, roots_array_start,
                             IntPtrConstant(root_index * kPointerSize), value);
}

Node* CodeAssembler::Retain(Node* value) {
  return raw_assembler()->Retain(value);
}

Node* CodeAssembler::Projection(int index, Node* value) {
  return raw_assembler()->Projection(index, value);
}

void CodeAssembler::GotoIfException(Node* node, Label* if_exception,
                                    Variable* exception_var) {
  Label success(this), exception(this, Label::kDeferred);
  success.MergeVariables();
  exception.MergeVariables();
  DCHECK(!node->op()->HasProperty(Operator::kNoThrow));

  raw_assembler()->Continuations(node, success.label_, exception.label_);

  Bind(&exception);
  const Operator* op = raw_assembler()->common()->IfException();
  Node* exception_value = raw_assembler()->AddNode(op, node, node);
  if (exception_var != nullptr) {
    exception_var->Bind(exception_value);
  }
  Goto(if_exception);

  Bind(&success);
}

template <class... TArgs>
Node* CodeAssembler::CallRuntime(Runtime::FunctionId function, Node* context,
                                 TArgs... args) {
  int argc = static_cast<int>(sizeof...(args));
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      zone(), function, argc, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  int return_count = static_cast<int>(desc->ReturnCount());

  Node* centry =
      HeapConstant(CodeFactory::RuntimeCEntry(isolate(), return_count));
  Node* ref = ExternalConstant(ExternalReference(function, isolate()));
  Node* arity = Int32Constant(argc);

  Node* nodes[] = {centry, args..., ref, arity, context};

  CallPrologue();
  Node* return_value = raw_assembler()->CallN(desc, arraysize(nodes), nodes);
  CallEpilogue();
  return return_value;
}

// Instantiate CallRuntime() with up to 6 arguments.
#define INSTANTIATE(...)                                       \
  template V8_EXPORT_PRIVATE Node* CodeAssembler::CallRuntime( \
      Runtime::FunctionId, __VA_ARGS__);
REPEAT_1_TO_7(INSTANTIATE, Node*)
#undef INSTANTIATE

template <class... TArgs>
Node* CodeAssembler::TailCallRuntime(Runtime::FunctionId function,
                                     Node* context, TArgs... args) {
  int argc = static_cast<int>(sizeof...(args));
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      zone(), function, argc, Operator::kNoProperties,
      CallDescriptor::kSupportsTailCalls);
  int return_count = static_cast<int>(desc->ReturnCount());

  Node* centry =
      HeapConstant(CodeFactory::RuntimeCEntry(isolate(), return_count));
  Node* ref = ExternalConstant(ExternalReference(function, isolate()));
  Node* arity = Int32Constant(argc);

  Node* nodes[] = {centry, args..., ref, arity, context};

  return raw_assembler()->TailCallN(desc, arraysize(nodes), nodes);
}

// Instantiate TailCallRuntime() with up to 6 arguments.
#define INSTANTIATE(...)                                           \
  template V8_EXPORT_PRIVATE Node* CodeAssembler::TailCallRuntime( \
      Runtime::FunctionId, __VA_ARGS__);
REPEAT_1_TO_7(INSTANTIATE, Node*)
#undef INSTANTIATE

template <class... TArgs>
Node* CodeAssembler::CallStubR(const CallInterfaceDescriptor& descriptor,
                               size_t result_size, Node* target, Node* context,
                               TArgs... args) {
  Node* nodes[] = {target, args..., context};
  return CallStubN(descriptor, result_size, arraysize(nodes), nodes);
}

// Instantiate CallStubR() with up to 6 arguments.
#define INSTANTIATE(...)                                     \
  template V8_EXPORT_PRIVATE Node* CodeAssembler::CallStubR( \
      const CallInterfaceDescriptor& descriptor, size_t, Node*, __VA_ARGS__);
REPEAT_1_TO_7(INSTANTIATE, Node*)
#undef INSTANTIATE

Node* CodeAssembler::CallStubN(const CallInterfaceDescriptor& descriptor,
                               size_t result_size, int input_count,
                               Node* const* inputs) {
  // 2 is for target and context.
  DCHECK_LE(2, input_count);
  int argc = input_count - 2;
  DCHECK_LE(descriptor.GetParameterCount(), argc);
  // Extra arguments not mentioned in the descriptor are passed on the stack.
  int stack_parameter_count = argc - descriptor.GetRegisterParameterCount();
  DCHECK_LE(descriptor.GetStackParameterCount(), stack_parameter_count);
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, stack_parameter_count,
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  CallPrologue();
  Node* return_value = raw_assembler()->CallN(desc, input_count, inputs);
  CallEpilogue();
  return return_value;
}

template <class... TArgs>
Node* CodeAssembler::TailCallStub(const CallInterfaceDescriptor& descriptor,
                                  Node* target, Node* context, TArgs... args) {
  DCHECK_EQ(descriptor.GetParameterCount(), sizeof...(args));
  size_t result_size = 1;
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kSupportsTailCalls, Operator::kNoProperties,
      MachineType::AnyTagged(), result_size);

  Node* nodes[] = {target, args..., context};

  return raw_assembler()->TailCallN(desc, arraysize(nodes), nodes);
}

// Instantiate TailCallStub() with up to 6 arguments.
#define INSTANTIATE(...)                                        \
  template V8_EXPORT_PRIVATE Node* CodeAssembler::TailCallStub( \
      const CallInterfaceDescriptor& descriptor, Node*, __VA_ARGS__);
REPEAT_1_TO_7(INSTANTIATE, Node*)
#undef INSTANTIATE

template <class... TArgs>
Node* CodeAssembler::TailCallBytecodeDispatch(
    const CallInterfaceDescriptor& descriptor, Node* target, TArgs... args) {
  DCHECK_EQ(descriptor.GetParameterCount(), sizeof...(args));
  CallDescriptor* desc = Linkage::GetBytecodeDispatchCallDescriptor(
      isolate(), zone(), descriptor, descriptor.GetStackParameterCount());

  Node* nodes[] = {target, args...};
  return raw_assembler()->TailCallN(desc, arraysize(nodes), nodes);
}

// Instantiate TailCallBytecodeDispatch() with 4 arguments.
template V8_EXPORT_PRIVATE Node* CodeAssembler::TailCallBytecodeDispatch(
    const CallInterfaceDescriptor& descriptor, Node* target, Node*, Node*,
    Node*, Node*);

Node* CodeAssembler::CallCFunction2(MachineType return_type,
                                    MachineType arg0_type,
                                    MachineType arg1_type, Node* function,
                                    Node* arg0, Node* arg1) {
  return raw_assembler()->CallCFunction2(return_type, arg0_type, arg1_type,
                                         function, arg0, arg1);
}

Node* CodeAssembler::CallCFunction3(MachineType return_type,
                                    MachineType arg0_type,
                                    MachineType arg1_type,
                                    MachineType arg2_type, Node* function,
                                    Node* arg0, Node* arg1, Node* arg2) {
  return raw_assembler()->CallCFunction3(return_type, arg0_type, arg1_type,
                                         arg2_type, function, arg0, arg1, arg2);
}

void CodeAssembler::Goto(Label* label) {
  label->MergeVariables();
  raw_assembler()->Goto(label->label_);
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

void CodeAssembler::Branch(Node* condition, Label* true_label,
                           Label* false_label) {
  true_label->MergeVariables();
  false_label->MergeVariables();
  return raw_assembler()->Branch(condition, true_label->label_,
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
  return raw_assembler()->Switch(index, default_label->label_, case_values,
                                 labels, case_count);
}

// RawMachineAssembler delegate helpers:
Isolate* CodeAssembler::isolate() const { return raw_assembler()->isolate(); }

Factory* CodeAssembler::factory() const { return isolate()->factory(); }

Zone* CodeAssembler::zone() const { return raw_assembler()->zone(); }

RawMachineAssembler* CodeAssembler::raw_assembler() const {
  return state_->raw_assembler_.get();
}

// The core implementation of Variable is stored through an indirection so
// that it can outlive the often block-scoped Variable declarations. This is
// needed to ensure that variable binding and merging through phis can
// properly be verified.
class CodeAssemblerVariable::Impl : public ZoneObject {
 public:
  explicit Impl(MachineRepresentation rep) : value_(nullptr), rep_(rep) {}
  Node* value_;
  MachineRepresentation rep_;
};

CodeAssemblerVariable::CodeAssemblerVariable(CodeAssembler* assembler,
                                             MachineRepresentation rep)
    : impl_(new (assembler->zone()) Impl(rep)), state_(assembler->state()) {
  state_->variables_.insert(impl_);
}

CodeAssemblerVariable::~CodeAssemblerVariable() {
  state_->variables_.erase(impl_);
}

void CodeAssemblerVariable::Bind(Node* value) { impl_->value_ = value; }

Node* CodeAssemblerVariable::value() const {
  DCHECK_NOT_NULL(impl_->value_);
  return impl_->value_;
}

MachineRepresentation CodeAssemblerVariable::rep() const { return impl_->rep_; }

bool CodeAssemblerVariable::IsBound() const { return impl_->value_ != nullptr; }

CodeAssemblerLabel::CodeAssemblerLabel(CodeAssembler* assembler,
                                       size_t vars_count,
                                       CodeAssemblerVariable** vars,
                                       CodeAssemblerLabel::Type type)
    : bound_(false),
      merge_count_(0),
      state_(assembler->state()),
      label_(nullptr) {
  void* buffer = assembler->zone()->New(sizeof(RawMachineLabel));
  label_ = new (buffer)
      RawMachineLabel(type == kDeferred ? RawMachineLabel::kDeferred
                                        : RawMachineLabel::kNonDeferred);
  for (size_t i = 0; i < vars_count; ++i) {
    variable_phis_[vars[i]->impl_] = nullptr;
  }
}

void CodeAssemblerLabel::MergeVariables() {
  ++merge_count_;
  for (auto var : state_->variables_) {
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
        state_->raw_assembler_->AppendPhiInput(phi->second, node);
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

void CodeAssemblerLabel::Bind() {
  DCHECK(!bound_);
  state_->raw_assembler_->Bind(label_);

  // Make sure that all variables that have changed along any path up to this
  // point are marked as merge variables.
  for (auto var : state_->variables_) {
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
    CodeAssemblerVariable::Impl* var_impl = var.first;
    auto i = variable_merges_.find(var_impl);
    // If the following asserts fire, then a variable that has been marked as
    // being merged at the label--either by explicitly marking it so in the
    // label constructor or by having seen different bound values at branches
    // into the label--doesn't have a bound value along all of the paths that
    // have been merged into the label up to this point.
    DCHECK(i != variable_merges_.end());
    DCHECK_EQ(i->second.size(), merge_count_);
    Node* phi = state_->raw_assembler_->Phi(
        var.first->rep_, static_cast<int>(merge_count_), &(i->second[0]));
    variable_phis_[var_impl] = phi;
  }

  // Bind all variables to a merge phi, the common value along all paths or
  // null.
  for (auto var : state_->variables_) {
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
