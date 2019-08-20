// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-assembler.h"

#include <ostream>

#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/macro-assembler.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/execution/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/objects-inl.h"
#include "src/objects/smi.h"
#include "src/utils/memcopy.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

constexpr MachineType MachineTypeOf<Smi>::value;
constexpr MachineType MachineTypeOf<Object>::value;

namespace compiler {

static_assert(std::is_convertible<TNode<Number>, TNode<Object>>::value,
              "test subtyping");
static_assert(std::is_convertible<TNode<UnionT<Smi, HeapNumber>>,
                                  TNode<UnionT<Smi, HeapObject>>>::value,
              "test subtyping");
static_assert(
    !std::is_convertible<TNode<UnionT<Smi, HeapObject>>, TNode<Number>>::value,
    "test subtyping");

CodeAssemblerState::CodeAssemblerState(
    Isolate* isolate, Zone* zone, const CallInterfaceDescriptor& descriptor,
    Code::Kind kind, const char* name, PoisoningMitigationLevel poisoning_level,
    int32_t builtin_index)
    // TODO(rmcilroy): Should we use Linkage::GetBytecodeDispatchDescriptor for
    // bytecode handlers?
    : CodeAssemblerState(
          isolate, zone,
          Linkage::GetStubCallDescriptor(
              zone, descriptor, descriptor.GetStackParameterCount(),
              CallDescriptor::kNoFlags, Operator::kNoProperties),
          kind, name, poisoning_level, builtin_index) {}

CodeAssemblerState::CodeAssemblerState(Isolate* isolate, Zone* zone,
                                       int parameter_count, Code::Kind kind,
                                       const char* name,
                                       PoisoningMitigationLevel poisoning_level,
                                       int32_t builtin_index)
    : CodeAssemblerState(
          isolate, zone,
          Linkage::GetJSCallDescriptor(
              zone, false, parameter_count,
              (kind == Code::BUILTIN ? CallDescriptor::kPushArgumentCount
                                     : CallDescriptor::kNoFlags) |
                  CallDescriptor::kCanUseRoots),
          kind, name, poisoning_level, builtin_index) {}

CodeAssemblerState::CodeAssemblerState(Isolate* isolate, Zone* zone,
                                       CallDescriptor* call_descriptor,
                                       Code::Kind kind, const char* name,
                                       PoisoningMitigationLevel poisoning_level,
                                       int32_t builtin_index)
    : raw_assembler_(new RawMachineAssembler(
          isolate, new (zone) Graph(zone), call_descriptor,
          MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements(), poisoning_level)),
      kind_(kind),
      name_(name),
      builtin_index_(builtin_index),
      code_generated_(false),
      variables_(zone) {}

CodeAssemblerState::~CodeAssemblerState() = default;

int CodeAssemblerState::parameter_count() const {
  return static_cast<int>(raw_assembler_->call_descriptor()->ParameterCount());
}

CodeAssembler::~CodeAssembler() = default;

#if DEBUG
void CodeAssemblerState::PrintCurrentBlock(std::ostream& os) {
  raw_assembler_->PrintCurrentBlock(os);
}
#endif

bool CodeAssemblerState::InsideBlock() { return raw_assembler_->InsideBlock(); }

void CodeAssemblerState::SetInitialDebugInformation(const char* msg,
                                                    const char* file,
                                                    int line) {
#if DEBUG
  AssemblerDebugInfo debug_info = {msg, file, line};
  raw_assembler_->SetSourcePosition(file, line);
  raw_assembler_->SetInitialDebugInformation(debug_info);
#endif  // DEBUG
}

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

bool CodeAssembler::Word32ShiftIsSafe() const {
  return raw_assembler()->machine()->Word32ShiftIsSafe();
}

PoisoningMitigationLevel CodeAssembler::poisoning_level() const {
  return raw_assembler()->poisoning_level();
}

// static
Handle<Code> CodeAssembler::GenerateCode(CodeAssemblerState* state,
                                         const AssemblerOptions& options) {
  DCHECK(!state->code_generated_);

  RawMachineAssembler* rasm = state->raw_assembler_.get();

  Handle<Code> code;
  Graph* graph = rasm->ExportForOptimization();

  code = Pipeline::GenerateCodeForCodeStub(
             rasm->isolate(), rasm->call_descriptor(), graph,
             rasm->source_positions(), state->kind_, state->name_,
             state->builtin_index_, rasm->poisoning_level(), options)
             .ToHandleChecked();

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

bool CodeAssembler::IsInt32AbsWithOverflowSupported() const {
  return raw_assembler()->machine()->Int32AbsWithOverflow().IsSupported();
}

bool CodeAssembler::IsInt64AbsWithOverflowSupported() const {
  return raw_assembler()->machine()->Int64AbsWithOverflow().IsSupported();
}

bool CodeAssembler::IsIntPtrAbsWithOverflowSupported() const {
  return Is64() ? IsInt64AbsWithOverflowSupported()
                : IsInt32AbsWithOverflowSupported();
}

#ifdef DEBUG
void CodeAssembler::GenerateCheckMaybeObjectIsObject(Node* node,
                                                     const char* location) {
  Label ok(this);
  GotoIf(WordNotEqual(WordAnd(BitcastMaybeObjectToWord(node),
                              IntPtrConstant(kHeapObjectTagMask)),
                      IntPtrConstant(kWeakHeapObjectTag)),
         &ok);
  EmbeddedVector<char, 1024> message;
  SNPrintF(message, "no Object: %s", location);
  Node* message_node = StringConstant(message.begin());
  // This somewhat misuses the AbortCSAAssert runtime function. This will print
  // "abort: CSA_ASSERT failed: <message>", which is good enough.
  AbortCSAAssert(message_node);
  Unreachable();
  Bind(&ok);
}
#endif

TNode<Int32T> CodeAssembler::Int32Constant(int32_t value) {
  return UncheckedCast<Int32T>(raw_assembler()->Int32Constant(value));
}

TNode<Int64T> CodeAssembler::Int64Constant(int64_t value) {
  return UncheckedCast<Int64T>(raw_assembler()->Int64Constant(value));
}

TNode<IntPtrT> CodeAssembler::IntPtrConstant(intptr_t value) {
  return UncheckedCast<IntPtrT>(raw_assembler()->IntPtrConstant(value));
}

TNode<Number> CodeAssembler::NumberConstant(double value) {
  int smi_value;
  if (DoubleToSmiInteger(value, &smi_value)) {
    return UncheckedCast<Number>(SmiConstant(smi_value));
  } else {
    // We allocate the heap number constant eagerly at this point instead of
    // deferring allocation to code generation
    // (see AllocateAndInstallRequestedHeapObjects) since that makes it easier
    // to generate constant lookups for embedded builtins.
    return UncheckedCast<Number>(HeapConstant(
        isolate()->factory()->NewHeapNumber(value, AllocationType::kOld)));
  }
}

TNode<Smi> CodeAssembler::SmiConstant(Smi value) {
  return UncheckedCast<Smi>(BitcastWordToTaggedSigned(
      IntPtrConstant(static_cast<intptr_t>(value.ptr()))));
}

TNode<Smi> CodeAssembler::SmiConstant(int value) {
  return SmiConstant(Smi::FromInt(value));
}

TNode<HeapObject> CodeAssembler::UntypedHeapConstant(
    Handle<HeapObject> object) {
  return UncheckedCast<HeapObject>(raw_assembler()->HeapConstant(object));
}

TNode<String> CodeAssembler::StringConstant(const char* str) {
  Handle<String> internalized_string =
      factory()->InternalizeString(OneByteVector(str));
  return UncheckedCast<String>(HeapConstant(internalized_string));
}

TNode<Oddball> CodeAssembler::BooleanConstant(bool value) {
  Handle<Object> object = isolate()->factory()->ToBoolean(value);
  return UncheckedCast<Oddball>(
      raw_assembler()->HeapConstant(Handle<HeapObject>::cast(object)));
}

TNode<ExternalReference> CodeAssembler::ExternalConstant(
    ExternalReference address) {
  return UncheckedCast<ExternalReference>(
      raw_assembler()->ExternalConstant(address));
}

TNode<Float64T> CodeAssembler::Float64Constant(double value) {
  return UncheckedCast<Float64T>(raw_assembler()->Float64Constant(value));
}

TNode<HeapNumber> CodeAssembler::NaNConstant() {
  return UncheckedCast<HeapNumber>(LoadRoot(RootIndex::kNanValue));
}

bool CodeAssembler::ToInt32Constant(Node* node, int32_t& out_value) {
  {
    Int64Matcher m(node);
    if (m.HasValue() && m.IsInRange(std::numeric_limits<int32_t>::min(),
                                    std::numeric_limits<int32_t>::max())) {
      out_value = static_cast<int32_t>(m.Value());
      return true;
    }
  }

  {
    Int32Matcher m(node);
    if (m.HasValue()) {
      out_value = m.Value();
      return true;
    }
  }

  return false;
}

bool CodeAssembler::ToInt64Constant(Node* node, int64_t& out_value) {
  Int64Matcher m(node);
  if (m.HasValue()) out_value = m.Value();
  return m.HasValue();
}

bool CodeAssembler::ToSmiConstant(Node* node, Smi* out_value) {
  if (node->opcode() == IrOpcode::kBitcastWordToTaggedSigned) {
    node = node->InputAt(0);
  }
  IntPtrMatcher m(node);
  if (m.HasValue()) {
    intptr_t value = m.Value();
    // Make sure that the value is actually a smi
    CHECK_EQ(0, value & ((static_cast<intptr_t>(1) << kSmiShiftSize) - 1));
    *out_value = Smi(static_cast<Address>(value));
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

bool CodeAssembler::IsUndefinedConstant(TNode<Object> node) {
  compiler::HeapObjectMatcher m(node);
  return m.Is(isolate()->factory()->undefined_value());
}

bool CodeAssembler::IsNullConstant(TNode<Object> node) {
  compiler::HeapObjectMatcher m(node);
  return m.Is(isolate()->factory()->null_value());
}

Node* CodeAssembler::Parameter(int index) {
  if (index == kTargetParameterIndex) return raw_assembler()->TargetParameter();
  return raw_assembler()->Parameter(index);
}

bool CodeAssembler::IsJSFunctionCall() const {
  auto call_descriptor = raw_assembler()->call_descriptor();
  return call_descriptor->IsJSFunctionCall();
}

TNode<Context> CodeAssembler::GetJSContextParameter() {
  auto call_descriptor = raw_assembler()->call_descriptor();
  DCHECK(call_descriptor->IsJSFunctionCall());
  return CAST(Parameter(Linkage::GetJSCallContextParamIndex(
      static_cast<int>(call_descriptor->JSParameterCount()))));
}

void CodeAssembler::Return(SloppyTNode<Object> value) {
  return raw_assembler()->Return(value);
}

void CodeAssembler::Return(SloppyTNode<Object> value1,
                           SloppyTNode<Object> value2) {
  return raw_assembler()->Return(value1, value2);
}

void CodeAssembler::Return(SloppyTNode<Object> value1,
                           SloppyTNode<Object> value2,
                           SloppyTNode<Object> value3) {
  return raw_assembler()->Return(value1, value2, value3);
}

void CodeAssembler::PopAndReturn(Node* pop, Node* value) {
  return raw_assembler()->PopAndReturn(pop, value);
}

void CodeAssembler::ReturnIf(Node* condition, Node* value) {
  Label if_return(this), if_continue(this);
  Branch(condition, &if_return, &if_continue);
  Bind(&if_return);
  Return(value);
  Bind(&if_continue);
}

void CodeAssembler::ReturnRaw(Node* value) {
  return raw_assembler()->Return(value);
}

void CodeAssembler::AbortCSAAssert(Node* message) {
  raw_assembler()->AbortCSAAssert(message);
}

void CodeAssembler::DebugBreak() { raw_assembler()->DebugBreak(); }

void CodeAssembler::Unreachable() {
  DebugBreak();
  raw_assembler()->Unreachable();
}

void CodeAssembler::Comment(std::string str) {
  if (!FLAG_code_comments) return;
  raw_assembler()->Comment(str);
}

void CodeAssembler::StaticAssert(TNode<BoolT> value) {
  raw_assembler()->StaticAssert(value);
}

void CodeAssembler::SetSourcePosition(const char* file, int line) {
  raw_assembler()->SetSourcePosition(file, line);
}

void CodeAssembler::Bind(Label* label) { return label->Bind(); }

#if DEBUG
void CodeAssembler::Bind(Label* label, AssemblerDebugInfo debug_info) {
  return label->Bind(debug_info);
}
#endif  // DEBUG

TNode<RawPtrT> CodeAssembler::LoadFramePointer() {
  return UncheckedCast<RawPtrT>(raw_assembler()->LoadFramePointer());
}

TNode<RawPtrT> CodeAssembler::LoadParentFramePointer() {
  return UncheckedCast<RawPtrT>(raw_assembler()->LoadParentFramePointer());
}

TNode<RawPtrT> CodeAssembler::LoadStackPointer() {
  return UncheckedCast<RawPtrT>(raw_assembler()->LoadStackPointer());
}

TNode<Object> CodeAssembler::TaggedPoisonOnSpeculation(
    SloppyTNode<Object> value) {
  return UncheckedCast<Object>(
      raw_assembler()->TaggedPoisonOnSpeculation(value));
}

TNode<WordT> CodeAssembler::WordPoisonOnSpeculation(SloppyTNode<WordT> value) {
  return UncheckedCast<WordT>(raw_assembler()->WordPoisonOnSpeculation(value));
}

#define DEFINE_CODE_ASSEMBLER_BINARY_OP(name, ResType, Arg1Type, Arg2Type) \
  TNode<ResType> CodeAssembler::name(SloppyTNode<Arg1Type> a,              \
                                     SloppyTNode<Arg2Type> b) {            \
    return UncheckedCast<ResType>(raw_assembler()->name(a, b));            \
  }
CODE_ASSEMBLER_BINARY_OP_LIST(DEFINE_CODE_ASSEMBLER_BINARY_OP)
#undef DEFINE_CODE_ASSEMBLER_BINARY_OP

TNode<WordT> CodeAssembler::IntPtrAdd(SloppyTNode<WordT> left,
                                      SloppyTNode<WordT> right) {
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
  return UncheckedCast<WordT>(raw_assembler()->IntPtrAdd(left, right));
}

TNode<IntPtrT> CodeAssembler::IntPtrDiv(TNode<IntPtrT> left,
                                        TNode<IntPtrT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_right_constant) {
    if (is_left_constant) {
      return IntPtrConstant(left_constant / right_constant);
    }
    if (base::bits::IsPowerOfTwo(right_constant)) {
      return WordSar(left, WhichPowerOf2(right_constant));
    }
  }
  return UncheckedCast<IntPtrT>(raw_assembler()->IntPtrDiv(left, right));
}

TNode<WordT> CodeAssembler::IntPtrSub(SloppyTNode<WordT> left,
                                      SloppyTNode<WordT> right) {
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
  return UncheckedCast<IntPtrT>(raw_assembler()->IntPtrSub(left, right));
}

TNode<WordT> CodeAssembler::IntPtrMul(SloppyTNode<WordT> left,
                                      SloppyTNode<WordT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant * right_constant);
    }
    if (base::bits::IsPowerOfTwo(left_constant)) {
      return WordShl(right, WhichPowerOf2(left_constant));
    }
  } else if (is_right_constant) {
    if (base::bits::IsPowerOfTwo(right_constant)) {
      return WordShl(left, WhichPowerOf2(right_constant));
    }
  }
  return UncheckedCast<IntPtrT>(raw_assembler()->IntPtrMul(left, right));
}

TNode<WordT> CodeAssembler::WordShl(SloppyTNode<WordT> value, int shift) {
  return (shift != 0) ? WordShl(value, IntPtrConstant(shift)) : value;
}

TNode<WordT> CodeAssembler::WordShr(SloppyTNode<WordT> value, int shift) {
  return (shift != 0) ? WordShr(value, IntPtrConstant(shift)) : value;
}

TNode<WordT> CodeAssembler::WordSar(SloppyTNode<WordT> value, int shift) {
  return (shift != 0) ? WordSar(value, IntPtrConstant(shift)) : value;
}

TNode<Word32T> CodeAssembler::Word32Shr(SloppyTNode<Word32T> value, int shift) {
  return (shift != 0) ? Word32Shr(value, Int32Constant(shift)) : value;
}

TNode<WordT> CodeAssembler::WordOr(SloppyTNode<WordT> left,
                                   SloppyTNode<WordT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant | right_constant);
    }
    if (left_constant == 0) {
      return right;
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordOr(left, right));
}

TNode<WordT> CodeAssembler::WordAnd(SloppyTNode<WordT> left,
                                    SloppyTNode<WordT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant & right_constant);
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordAnd(left, right));
}

TNode<WordT> CodeAssembler::WordXor(SloppyTNode<WordT> left,
                                    SloppyTNode<WordT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant ^ right_constant);
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordXor(left, right));
}

TNode<WordT> CodeAssembler::WordShl(SloppyTNode<WordT> left,
                                    SloppyTNode<IntegralT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant << right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordShl(left, right));
}

TNode<WordT> CodeAssembler::WordShr(SloppyTNode<WordT> left,
                                    SloppyTNode<IntegralT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(static_cast<uintptr_t>(left_constant) >>
                            right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordShr(left, right));
}

TNode<WordT> CodeAssembler::WordSar(SloppyTNode<WordT> left,
                                    SloppyTNode<IntegralT> right) {
  intptr_t left_constant;
  bool is_left_constant = ToIntPtrConstant(left, left_constant);
  intptr_t right_constant;
  bool is_right_constant = ToIntPtrConstant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return IntPtrConstant(left_constant >> right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<WordT>(raw_assembler()->WordSar(left, right));
}

TNode<Word32T> CodeAssembler::Word32Or(SloppyTNode<Word32T> left,
                                       SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(left_constant | right_constant);
    }
    if (left_constant == 0) {
      return right;
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32Or(left, right));
}

TNode<Word32T> CodeAssembler::Word32And(SloppyTNode<Word32T> left,
                                        SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(left_constant & right_constant);
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32And(left, right));
}

TNode<Word32T> CodeAssembler::Word32Xor(SloppyTNode<Word32T> left,
                                        SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(left_constant ^ right_constant);
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32Xor(left, right));
}

TNode<Word32T> CodeAssembler::Word32Shl(SloppyTNode<Word32T> left,
                                        SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(left_constant << right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32Shl(left, right));
}

TNode<Word32T> CodeAssembler::Word32Shr(SloppyTNode<Word32T> left,
                                        SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(static_cast<uint32_t>(left_constant) >>
                           right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32Shr(left, right));
}

TNode<Word32T> CodeAssembler::Word32Sar(SloppyTNode<Word32T> left,
                                        SloppyTNode<Word32T> right) {
  int32_t left_constant;
  bool is_left_constant = ToInt32Constant(left, left_constant);
  int32_t right_constant;
  bool is_right_constant = ToInt32Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int32Constant(left_constant >> right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word32T>(raw_assembler()->Word32Sar(left, right));
}

TNode<Word64T> CodeAssembler::Word64Or(SloppyTNode<Word64T> left,
                                       SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(left_constant | right_constant);
    }
    if (left_constant == 0) {
      return right;
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64Or(left, right));
}

TNode<Word64T> CodeAssembler::Word64And(SloppyTNode<Word64T> left,
                                        SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(left_constant & right_constant);
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64And(left, right));
}

TNode<Word64T> CodeAssembler::Word64Xor(SloppyTNode<Word64T> left,
                                        SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(left_constant ^ right_constant);
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64Xor(left, right));
}

TNode<Word64T> CodeAssembler::Word64Shl(SloppyTNode<Word64T> left,
                                        SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(left_constant << right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64Shl(left, right));
}

TNode<Word64T> CodeAssembler::Word64Shr(SloppyTNode<Word64T> left,
                                        SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(static_cast<uint64_t>(left_constant) >>
                           right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64Shr(left, right));
}

TNode<Word64T> CodeAssembler::Word64Sar(SloppyTNode<Word64T> left,
                                        SloppyTNode<Word64T> right) {
  int64_t left_constant;
  bool is_left_constant = ToInt64Constant(left, left_constant);
  int64_t right_constant;
  bool is_right_constant = ToInt64Constant(right, right_constant);
  if (is_left_constant) {
    if (is_right_constant) {
      return Int64Constant(left_constant >> right_constant);
    }
  } else if (is_right_constant) {
    if (right_constant == 0) {
      return left;
    }
  }
  return UncheckedCast<Word64T>(raw_assembler()->Word64Sar(left, right));
}

#define CODE_ASSEMBLER_COMPARE(Name, ArgT, VarT, ToConstant, op)     \
  TNode<BoolT> CodeAssembler::Name(SloppyTNode<ArgT> left,           \
                                   SloppyTNode<ArgT> right) {        \
    VarT lhs, rhs;                                                   \
    if (ToConstant(left, lhs) && ToConstant(right, rhs)) {           \
      return BoolConstant(lhs op rhs);                               \
    }                                                                \
    return UncheckedCast<BoolT>(raw_assembler()->Name(left, right)); \
  }

CODE_ASSEMBLER_COMPARE(IntPtrEqual, WordT, intptr_t, ToIntPtrConstant, ==)
CODE_ASSEMBLER_COMPARE(WordEqual, WordT, intptr_t, ToIntPtrConstant, ==)
CODE_ASSEMBLER_COMPARE(WordNotEqual, WordT, intptr_t, ToIntPtrConstant, !=)
CODE_ASSEMBLER_COMPARE(Word32Equal, Word32T, int32_t, ToInt32Constant, ==)
CODE_ASSEMBLER_COMPARE(Word32NotEqual, Word32T, int32_t, ToInt32Constant, !=)
CODE_ASSEMBLER_COMPARE(Word64Equal, Word64T, int64_t, ToInt64Constant, ==)
CODE_ASSEMBLER_COMPARE(Word64NotEqual, Word64T, int64_t, ToInt64Constant, !=)
#undef CODE_ASSEMBLER_COMPARE

TNode<UintPtrT> CodeAssembler::ChangeUint32ToWord(SloppyTNode<Word32T> value) {
  if (raw_assembler()->machine()->Is64()) {
    return UncheckedCast<UintPtrT>(
        raw_assembler()->ChangeUint32ToUint64(value));
  }
  return ReinterpretCast<UintPtrT>(value);
}

TNode<IntPtrT> CodeAssembler::ChangeInt32ToIntPtr(SloppyTNode<Word32T> value) {
  if (raw_assembler()->machine()->Is64()) {
    return ReinterpretCast<IntPtrT>(raw_assembler()->ChangeInt32ToInt64(value));
  }
  return ReinterpretCast<IntPtrT>(value);
}

TNode<UintPtrT> CodeAssembler::ChangeFloat64ToUintPtr(
    SloppyTNode<Float64T> value) {
  if (raw_assembler()->machine()->Is64()) {
    return ReinterpretCast<UintPtrT>(
        raw_assembler()->ChangeFloat64ToUint64(value));
  }
  return ReinterpretCast<UintPtrT>(
      raw_assembler()->ChangeFloat64ToUint32(value));
}

TNode<Float64T> CodeAssembler::ChangeUintPtrToFloat64(TNode<UintPtrT> value) {
  if (raw_assembler()->machine()->Is64()) {
    // TODO(turbofan): Maybe we should introduce a ChangeUint64ToFloat64
    // machine operator to TurboFan here?
    return ReinterpretCast<Float64T>(
        raw_assembler()->RoundUint64ToFloat64(value));
  }
  return ReinterpretCast<Float64T>(
      raw_assembler()->ChangeUint32ToFloat64(value));
}

Node* CodeAssembler::RoundIntPtrToFloat64(Node* value) {
  if (raw_assembler()->machine()->Is64()) {
    return raw_assembler()->RoundInt64ToFloat64(value);
  }
  return raw_assembler()->ChangeInt32ToFloat64(value);
}

#define DEFINE_CODE_ASSEMBLER_UNARY_OP(name, ResType, ArgType) \
  TNode<ResType> CodeAssembler::name(SloppyTNode<ArgType> a) { \
    return UncheckedCast<ResType>(raw_assembler()->name(a));   \
  }
CODE_ASSEMBLER_UNARY_OP_LIST(DEFINE_CODE_ASSEMBLER_UNARY_OP)
#undef DEFINE_CODE_ASSEMBLER_UNARY_OP

Node* CodeAssembler::Load(MachineType type, Node* base,
                          LoadSensitivity needs_poisoning) {
  return raw_assembler()->Load(type, base, needs_poisoning);
}

Node* CodeAssembler::Load(MachineType type, Node* base, Node* offset,
                          LoadSensitivity needs_poisoning) {
  return raw_assembler()->Load(type, base, offset, needs_poisoning);
}

Node* CodeAssembler::LoadFullTagged(Node* base,
                                    LoadSensitivity needs_poisoning) {
  return BitcastWordToTagged(
      Load(MachineType::Pointer(), base, needs_poisoning));
}

Node* CodeAssembler::LoadFullTagged(Node* base, Node* offset,
                                    LoadSensitivity needs_poisoning) {
  return BitcastWordToTagged(
      Load(MachineType::Pointer(), base, offset, needs_poisoning));
}

Node* CodeAssembler::AtomicLoad(MachineType type, Node* base, Node* offset) {
  return raw_assembler()->AtomicLoad(type, base, offset);
}

Node* CodeAssembler::LoadFromObject(MachineType type, TNode<HeapObject> object,
                                    TNode<IntPtrT> offset) {
  return raw_assembler()->LoadFromObject(type, object, offset);
}

TNode<Object> CodeAssembler::LoadRoot(RootIndex root_index) {
  if (RootsTable::IsImmortalImmovable(root_index)) {
    Handle<Object> root = isolate()->root_handle(root_index);
    if (root->IsSmi()) {
      return SmiConstant(Smi::cast(*root));
    } else {
      return HeapConstant(Handle<HeapObject>::cast(root));
    }
  }

  // TODO(jgruber): In theory we could generate better code for this by
  // letting the macro assembler decide how to load from the roots list. In most
  // cases, it would boil down to loading from a fixed kRootRegister offset.
  Node* isolate_root =
      ExternalConstant(ExternalReference::isolate_root(isolate()));
  int offset = IsolateData::root_slot_offset(root_index);
  return UncheckedCast<Object>(
      LoadFullTagged(isolate_root, IntPtrConstant(offset)));
}

Node* CodeAssembler::Store(Node* base, Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, value,
                                kFullWriteBarrier);
}

void CodeAssembler::StoreToObject(MachineRepresentation rep,
                                  TNode<HeapObject> object,
                                  TNode<IntPtrT> offset, Node* value,
                                  StoreToObjectWriteBarrier write_barrier) {
  WriteBarrierKind write_barrier_kind;
  switch (write_barrier) {
    case StoreToObjectWriteBarrier::kFull:
      write_barrier_kind = WriteBarrierKind::kFullWriteBarrier;
      break;
    case StoreToObjectWriteBarrier::kMap:
      write_barrier_kind = WriteBarrierKind::kMapWriteBarrier;
      break;
    case StoreToObjectWriteBarrier::kNone:
      if (CanBeTaggedPointer(rep)) {
        write_barrier_kind = WriteBarrierKind::kAssertNoWriteBarrier;
      } else {
        write_barrier_kind = WriteBarrierKind::kNoWriteBarrier;
      }
      break;
  }
  raw_assembler()->StoreToObject(rep, object, offset, value,
                                 write_barrier_kind);
}

void CodeAssembler::OptimizedStoreField(MachineRepresentation rep,
                                        TNode<HeapObject> object, int offset,
                                        Node* value) {
  raw_assembler()->OptimizedStoreField(rep, object, offset, value,
                                       WriteBarrierKind::kFullWriteBarrier);
}

void CodeAssembler::OptimizedStoreFieldAssertNoWriteBarrier(
    MachineRepresentation rep, TNode<HeapObject> object, int offset,
    Node* value) {
  raw_assembler()->OptimizedStoreField(rep, object, offset, value,
                                       WriteBarrierKind::kAssertNoWriteBarrier);
}

void CodeAssembler::OptimizedStoreFieldUnsafeNoWriteBarrier(
    MachineRepresentation rep, TNode<HeapObject> object, int offset,
    Node* value) {
  raw_assembler()->OptimizedStoreField(rep, object, offset, value,
                                       WriteBarrierKind::kNoWriteBarrier);
}

void CodeAssembler::OptimizedStoreMap(TNode<HeapObject> object,
                                      TNode<Map> map) {
  raw_assembler()->OptimizedStoreMap(object, map);
}

Node* CodeAssembler::Store(Node* base, Node* offset, Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, offset,
                                value, kFullWriteBarrier);
}

Node* CodeAssembler::StoreEphemeronKey(Node* base, Node* offset, Node* value) {
  return raw_assembler()->Store(MachineRepresentation::kTagged, base, offset,
                                value, kEphemeronKeyWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* value) {
  return raw_assembler()->Store(
      rep, base, value,
      CanBeTaggedPointer(rep) ? kAssertNoWriteBarrier : kNoWriteBarrier);
}

Node* CodeAssembler::StoreNoWriteBarrier(MachineRepresentation rep, Node* base,
                                         Node* offset, Node* value) {
  return raw_assembler()->Store(
      rep, base, offset, value,
      CanBeTaggedPointer(rep) ? kAssertNoWriteBarrier : kNoWriteBarrier);
}

Node* CodeAssembler::UnsafeStoreNoWriteBarrier(MachineRepresentation rep,
                                               Node* base, Node* value) {
  return raw_assembler()->Store(rep, base, value, kNoWriteBarrier);
}

Node* CodeAssembler::UnsafeStoreNoWriteBarrier(MachineRepresentation rep,
                                               Node* base, Node* offset,
                                               Node* value) {
  return raw_assembler()->Store(rep, base, offset, value, kNoWriteBarrier);
}

Node* CodeAssembler::StoreFullTaggedNoWriteBarrier(Node* base,
                                                   Node* tagged_value) {
  return StoreNoWriteBarrier(MachineType::PointerRepresentation(), base,
                             BitcastTaggedToWord(tagged_value));
}

Node* CodeAssembler::StoreFullTaggedNoWriteBarrier(Node* base, Node* offset,
                                                   Node* tagged_value) {
  return StoreNoWriteBarrier(MachineType::PointerRepresentation(), base, offset,
                             BitcastTaggedToWord(tagged_value));
}

Node* CodeAssembler::AtomicStore(MachineRepresentation rep, Node* base,
                                 Node* offset, Node* value, Node* value_high) {
  return raw_assembler()->AtomicStore(rep, base, offset, value, value_high);
}

#define ATOMIC_FUNCTION(name)                                       \
  Node* CodeAssembler::Atomic##name(MachineType type, Node* base,   \
                                    Node* offset, Node* value,      \
                                    Node* value_high) {             \
    return raw_assembler()->Atomic##name(type, base, offset, value, \
                                         value_high);               \
  }
ATOMIC_FUNCTION(Exchange)
ATOMIC_FUNCTION(Add)
ATOMIC_FUNCTION(Sub)
ATOMIC_FUNCTION(And)
ATOMIC_FUNCTION(Or)
ATOMIC_FUNCTION(Xor)
#undef ATOMIC_FUNCTION

Node* CodeAssembler::AtomicCompareExchange(MachineType type, Node* base,
                                           Node* offset, Node* old_value,
                                           Node* new_value,
                                           Node* old_value_high,
                                           Node* new_value_high) {
  return raw_assembler()->AtomicCompareExchange(
      type, base, offset, old_value, old_value_high, new_value, new_value_high);
}

Node* CodeAssembler::StoreRoot(RootIndex root_index, Node* value) {
  DCHECK(!RootsTable::IsImmortalImmovable(root_index));
  Node* isolate_root =
      ExternalConstant(ExternalReference::isolate_root(isolate()));
  int offset = IsolateData::root_slot_offset(root_index);
  return StoreFullTaggedNoWriteBarrier(isolate_root, IntPtrConstant(offset),
                                       value);
}

Node* CodeAssembler::Retain(Node* value) {
  return raw_assembler()->Retain(value);
}

Node* CodeAssembler::Projection(int index, Node* value) {
  DCHECK_LT(index, value->op()->ValueOutputCount());
  return raw_assembler()->Projection(index, value);
}

void CodeAssembler::GotoIfException(Node* node, Label* if_exception,
                                    Variable* exception_var) {
  if (if_exception == nullptr) {
    // If no handler is supplied, don't add continuations
    return;
  }

  // No catch handlers should be active if we're using catch labels
  DCHECK_EQ(state()->exception_handler_labels_.size(), 0);
  DCHECK(!node->op()->HasProperty(Operator::kNoThrow));

  Label success(this), exception(this, Label::kDeferred);
  success.MergeVariables();
  exception.MergeVariables();

  raw_assembler()->Continuations(node, success.label_, exception.label_);

  Bind(&exception);
  const Operator* op = raw_assembler()->common()->IfException();
  Node* exception_value = raw_assembler()->AddNode(op, node, node);
  if (exception_var != nullptr) {
    exception_var->Bind(exception_value);
  }
  Goto(if_exception);

  Bind(&success);
  raw_assembler()->AddNode(raw_assembler()->common()->IfSuccess(), node);
}

TNode<HeapObject> CodeAssembler::OptimizedAllocate(
    TNode<IntPtrT> size, AllocationType allocation,
    AllowLargeObjects allow_large_objects) {
  return UncheckedCast<HeapObject>(raw_assembler()->OptimizedAllocate(
      size, allocation, allow_large_objects));
}

void CodeAssembler::HandleException(Node* node) {
  if (state_->exception_handler_labels_.size() == 0) return;
  CodeAssemblerExceptionHandlerLabel* label =
      state_->exception_handler_labels_.back();

  if (node->op()->HasProperty(Operator::kNoThrow)) {
    return;
  }

  Label success(this), exception(this, Label::kDeferred);
  success.MergeVariables();
  exception.MergeVariables();

  raw_assembler()->Continuations(node, success.label_, exception.label_);

  Bind(&exception);
  const Operator* op = raw_assembler()->common()->IfException();
  Node* exception_value = raw_assembler()->AddNode(op, node, node);
  label->AddInputs({UncheckedCast<Object>(exception_value)});
  Goto(label->plain_label());

  Bind(&success);
  raw_assembler()->AddNode(raw_assembler()->common()->IfSuccess(), node);
}

namespace {
template <size_t kMaxSize>
class NodeArray {
 public:
  void Add(Node* node) {
    DCHECK_GT(kMaxSize, size());
    *ptr_++ = node;
  }

  Node* const* data() const { return arr_; }
  int size() const { return static_cast<int>(ptr_ - arr_); }

 private:
  Node* arr_[kMaxSize];
  Node** ptr_ = arr_;
};
}  // namespace

TNode<Object> CodeAssembler::CallRuntimeImpl(
    Runtime::FunctionId function, TNode<Object> context,
    std::initializer_list<TNode<Object>> args) {
  int result_size = Runtime::FunctionForId(function)->result_size;
  TNode<Code> centry =
      HeapConstant(CodeFactory::RuntimeCEntry(isolate(), result_size));
  return CallRuntimeWithCEntryImpl(function, centry, context, args);
}

TNode<Object> CodeAssembler::CallRuntimeWithCEntryImpl(
    Runtime::FunctionId function, TNode<Code> centry, TNode<Object> context,
    std::initializer_list<TNode<Object>> args) {
  constexpr size_t kMaxNumArgs = 6;
  DCHECK_GE(kMaxNumArgs, args.size());
  int argc = static_cast<int>(args.size());
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      zone(), function, argc, Operator::kNoProperties,
      Runtime::MayAllocate(function) ? CallDescriptor::kNoFlags
                                     : CallDescriptor::kNoAllocate);

  Node* ref = ExternalConstant(ExternalReference::Create(function));
  Node* arity = Int32Constant(argc);

  NodeArray<kMaxNumArgs + 4> inputs;
  inputs.Add(centry);
  for (auto arg : args) inputs.Add(arg);
  inputs.Add(ref);
  inputs.Add(arity);
  inputs.Add(context);

  CallPrologue();
  Node* return_value =
      raw_assembler()->CallN(call_descriptor, inputs.size(), inputs.data());
  HandleException(return_value);
  CallEpilogue();
  return UncheckedCast<Object>(return_value);
}

void CodeAssembler::TailCallRuntimeImpl(
    Runtime::FunctionId function, TNode<Int32T> arity, TNode<Object> context,
    std::initializer_list<TNode<Object>> args) {
  int result_size = Runtime::FunctionForId(function)->result_size;
  TNode<Code> centry =
      HeapConstant(CodeFactory::RuntimeCEntry(isolate(), result_size));
  return TailCallRuntimeWithCEntryImpl(function, arity, centry, context, args);
}

void CodeAssembler::TailCallRuntimeWithCEntryImpl(
    Runtime::FunctionId function, TNode<Int32T> arity, TNode<Code> centry,
    TNode<Object> context, std::initializer_list<TNode<Object>> args) {
  constexpr size_t kMaxNumArgs = 6;
  DCHECK_GE(kMaxNumArgs, args.size());
  int argc = static_cast<int>(args.size());
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      zone(), function, argc, Operator::kNoProperties,
      CallDescriptor::kNoFlags);

  Node* ref = ExternalConstant(ExternalReference::Create(function));

  NodeArray<kMaxNumArgs + 4> inputs;
  inputs.Add(centry);
  for (auto arg : args) inputs.Add(arg);
  inputs.Add(ref);
  inputs.Add(arity);
  inputs.Add(context);

  raw_assembler()->TailCallN(call_descriptor, inputs.size(), inputs.data());
}

Node* CodeAssembler::CallStubN(StubCallMode call_mode,
                               const CallInterfaceDescriptor& descriptor,
                               size_t result_size, int input_count,
                               Node* const* inputs) {
  DCHECK(call_mode == StubCallMode::kCallCodeObject ||
         call_mode == StubCallMode::kCallBuiltinPointer);

  // implicit nodes are target and optionally context.
  int implicit_nodes = descriptor.HasContextParameter() ? 2 : 1;
  DCHECK_LE(implicit_nodes, input_count);
  int argc = input_count - implicit_nodes;
  DCHECK_LE(descriptor.GetParameterCount(), argc);
  // Extra arguments not mentioned in the descriptor are passed on the stack.
  int stack_parameter_count = argc - descriptor.GetRegisterParameterCount();
  DCHECK_LE(descriptor.GetStackParameterCount(), stack_parameter_count);
  DCHECK_EQ(result_size, descriptor.GetReturnCount());

  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), descriptor, stack_parameter_count, CallDescriptor::kNoFlags,
      Operator::kNoProperties, call_mode);

  CallPrologue();
  Node* return_value =
      raw_assembler()->CallN(call_descriptor, input_count, inputs);
  HandleException(return_value);
  CallEpilogue();
  return return_value;
}

void CodeAssembler::TailCallStubImpl(const CallInterfaceDescriptor& descriptor,
                                     TNode<Code> target, TNode<Object> context,
                                     std::initializer_list<Node*> args) {
  constexpr size_t kMaxNumArgs = 11;
  DCHECK_GE(kMaxNumArgs, args.size());
  DCHECK_EQ(descriptor.GetParameterCount(), args.size());
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kNoFlags, Operator::kNoProperties);

  NodeArray<kMaxNumArgs + 2> inputs;
  inputs.Add(target);
  for (auto arg : args) inputs.Add(arg);
  if (descriptor.HasContextParameter()) {
    inputs.Add(context);
  }

  raw_assembler()->TailCallN(call_descriptor, inputs.size(), inputs.data());
}

Node* CodeAssembler::CallStubRImpl(StubCallMode call_mode,
                                   const CallInterfaceDescriptor& descriptor,
                                   size_t result_size, Node* target,
                                   SloppyTNode<Object> context,
                                   std::initializer_list<Node*> args) {
  DCHECK(call_mode == StubCallMode::kCallCodeObject ||
         call_mode == StubCallMode::kCallBuiltinPointer);

  constexpr size_t kMaxNumArgs = 10;
  DCHECK_GE(kMaxNumArgs, args.size());

  NodeArray<kMaxNumArgs + 2> inputs;
  inputs.Add(target);
  for (auto arg : args) inputs.Add(arg);
  if (descriptor.HasContextParameter()) {
    inputs.Add(context);
  }

  return CallStubN(call_mode, descriptor, result_size, inputs.size(),
                   inputs.data());
}

Node* CodeAssembler::TailCallStubThenBytecodeDispatchImpl(
    const CallInterfaceDescriptor& descriptor, Node* target, Node* context,
    std::initializer_list<Node*> args) {
  constexpr size_t kMaxNumArgs = 6;
  DCHECK_GE(kMaxNumArgs, args.size());

  DCHECK_LE(descriptor.GetParameterCount(), args.size());
  int argc = static_cast<int>(args.size());
  // Extra arguments not mentioned in the descriptor are passed on the stack.
  int stack_parameter_count = argc - descriptor.GetRegisterParameterCount();
  DCHECK_LE(descriptor.GetStackParameterCount(), stack_parameter_count);
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), descriptor, stack_parameter_count, CallDescriptor::kNoFlags,
      Operator::kNoProperties);

  NodeArray<kMaxNumArgs + 2> inputs;
  inputs.Add(target);
  for (auto arg : args) inputs.Add(arg);
  inputs.Add(context);

  return raw_assembler()->TailCallN(call_descriptor, inputs.size(),
                                    inputs.data());
}

template <class... TArgs>
Node* CodeAssembler::TailCallBytecodeDispatch(
    const CallInterfaceDescriptor& descriptor, Node* target, TArgs... args) {
  DCHECK_EQ(descriptor.GetParameterCount(), sizeof...(args));
  auto call_descriptor = Linkage::GetBytecodeDispatchCallDescriptor(
      zone(), descriptor, descriptor.GetStackParameterCount());

  Node* nodes[] = {target, args...};
  CHECK_EQ(descriptor.GetParameterCount() + 1, arraysize(nodes));
  return raw_assembler()->TailCallN(call_descriptor, arraysize(nodes), nodes);
}

// Instantiate TailCallBytecodeDispatch() for argument counts used by
// CSA-generated code
template V8_EXPORT_PRIVATE Node* CodeAssembler::TailCallBytecodeDispatch(
    const CallInterfaceDescriptor& descriptor, Node* target, Node*, Node*,
    Node*, Node*);

TNode<Object> CodeAssembler::TailCallJSCode(TNode<Code> code,
                                            TNode<Context> context,
                                            TNode<JSFunction> function,
                                            TNode<Object> new_target,
                                            TNode<Int32T> arg_count) {
  JSTrampolineDescriptor descriptor;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      zone(), descriptor, descriptor.GetStackParameterCount(),
      CallDescriptor::kFixedTargetRegister, Operator::kNoProperties);

  Node* nodes[] = {code, function, new_target, arg_count, context};
  CHECK_EQ(descriptor.GetParameterCount() + 2, arraysize(nodes));
  return UncheckedCast<Object>(
      raw_assembler()->TailCallN(call_descriptor, arraysize(nodes), nodes));
}

Node* CodeAssembler::CallCFunctionN(Signature<MachineType>* signature,
                                    int input_count, Node* const* inputs) {
  auto call_descriptor = Linkage::GetSimplifiedCDescriptor(zone(), signature);
  return raw_assembler()->CallN(call_descriptor, input_count, inputs);
}

Node* CodeAssembler::CallCFunction(
    Node* function, MachineType return_type,
    std::initializer_list<CodeAssembler::CFunctionArg> args) {
  return raw_assembler()->CallCFunction(function, return_type, args);
}

Node* CodeAssembler::CallCFunctionWithCallerSavedRegisters(
    Node* function, MachineType return_type, SaveFPRegsMode mode,
    std::initializer_list<CodeAssembler::CFunctionArg> args) {
  DCHECK(return_type.LessThanOrEqualPointerSize());
  return raw_assembler()->CallCFunctionWithCallerSavedRegisters(
      function, return_type, mode, args);
}

void CodeAssembler::Goto(Label* label) {
  label->MergeVariables();
  raw_assembler()->Goto(label->label_);
}

void CodeAssembler::GotoIf(SloppyTNode<IntegralT> condition,
                           Label* true_label) {
  Label false_label(this);
  Branch(condition, true_label, &false_label);
  Bind(&false_label);
}

void CodeAssembler::GotoIfNot(SloppyTNode<IntegralT> condition,
                              Label* false_label) {
  Label true_label(this);
  Branch(condition, &true_label, false_label);
  Bind(&true_label);
}

void CodeAssembler::Branch(SloppyTNode<IntegralT> condition, Label* true_label,
                           Label* false_label) {
  int32_t constant;
  if (ToInt32Constant(condition, constant)) {
    if ((true_label->is_used() || true_label->is_bound()) &&
        (false_label->is_used() || false_label->is_bound())) {
      return Goto(constant ? true_label : false_label);
    }
  }
  true_label->MergeVariables();
  false_label->MergeVariables();
  return raw_assembler()->Branch(condition, true_label->label_,
                                 false_label->label_);
}

void CodeAssembler::Branch(TNode<BoolT> condition,
                           const std::function<void()>& true_body,
                           const std::function<void()>& false_body) {
  int32_t constant;
  if (ToInt32Constant(condition, constant)) {
    return constant ? true_body() : false_body();
  }

  Label vtrue(this), vfalse(this);
  Branch(condition, &vtrue, &vfalse);

  Bind(&vtrue);
  true_body();

  Bind(&vfalse);
  false_body();
}

void CodeAssembler::Branch(TNode<BoolT> condition, Label* true_label,
                           const std::function<void()>& false_body) {
  int32_t constant;
  if (ToInt32Constant(condition, constant)) {
    return constant ? Goto(true_label) : false_body();
  }

  Label vfalse(this);
  Branch(condition, true_label, &vfalse);
  Bind(&vfalse);
  false_body();
}

void CodeAssembler::Branch(TNode<BoolT> condition,
                           const std::function<void()>& true_body,
                           Label* false_label) {
  int32_t constant;
  if (ToInt32Constant(condition, constant)) {
    return constant ? true_body() : Goto(false_label);
  }

  Label vtrue(this);
  Branch(condition, &vtrue, false_label);
  Bind(&vtrue);
  true_body();
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
  }
  default_label->MergeVariables();
  return raw_assembler()->Switch(index, default_label->label_, case_values,
                                 labels, case_count);
}

bool CodeAssembler::UnalignedLoadSupported(MachineRepresentation rep) const {
  return raw_assembler()->machine()->UnalignedLoadSupported(rep);
}
bool CodeAssembler::UnalignedStoreSupported(MachineRepresentation rep) const {
  return raw_assembler()->machine()->UnalignedStoreSupported(rep);
}

// RawMachineAssembler delegate helpers:
Isolate* CodeAssembler::isolate() const { return raw_assembler()->isolate(); }

Factory* CodeAssembler::factory() const { return isolate()->factory(); }

Zone* CodeAssembler::zone() const { return raw_assembler()->zone(); }

bool CodeAssembler::IsExceptionHandlerActive() const {
  return state_->exception_handler_labels_.size() != 0;
}

RawMachineAssembler* CodeAssembler::raw_assembler() const {
  return state_->raw_assembler_.get();
}

// The core implementation of Variable is stored through an indirection so
// that it can outlive the often block-scoped Variable declarations. This is
// needed to ensure that variable binding and merging through phis can
// properly be verified.
class CodeAssemblerVariable::Impl : public ZoneObject {
 public:
  explicit Impl(MachineRepresentation rep, CodeAssemblerState::VariableId id)
      :
#if DEBUG
        debug_info_(AssemblerDebugInfo(nullptr, nullptr, -1)),
#endif
        value_(nullptr),
        rep_(rep),
        var_id_(id) {
  }

#if DEBUG
  AssemblerDebugInfo debug_info() const { return debug_info_; }
  void set_debug_info(AssemblerDebugInfo debug_info) {
    debug_info_ = debug_info;
  }

  AssemblerDebugInfo debug_info_;
#endif  // DEBUG
  bool operator<(const CodeAssemblerVariable::Impl& other) const {
    return var_id_ < other.var_id_;
  }
  Node* value_;
  MachineRepresentation rep_;
  CodeAssemblerState::VariableId var_id_;
};

bool CodeAssemblerVariable::ImplComparator::operator()(
    const CodeAssemblerVariable::Impl* a,
    const CodeAssemblerVariable::Impl* b) const {
  return *a < *b;
}

CodeAssemblerVariable::CodeAssemblerVariable(CodeAssembler* assembler,
                                             MachineRepresentation rep)
    : impl_(new (assembler->zone())
                Impl(rep, assembler->state()->NextVariableId())),
      state_(assembler->state()) {
  state_->variables_.insert(impl_);
}

CodeAssemblerVariable::CodeAssemblerVariable(CodeAssembler* assembler,
                                             MachineRepresentation rep,
                                             Node* initial_value)
    : CodeAssemblerVariable(assembler, rep) {
  Bind(initial_value);
}

#if DEBUG
CodeAssemblerVariable::CodeAssemblerVariable(CodeAssembler* assembler,
                                             AssemblerDebugInfo debug_info,
                                             MachineRepresentation rep)
    : impl_(new (assembler->zone())
                Impl(rep, assembler->state()->NextVariableId())),
      state_(assembler->state()) {
  impl_->set_debug_info(debug_info);
  state_->variables_.insert(impl_);
}

CodeAssemblerVariable::CodeAssemblerVariable(CodeAssembler* assembler,
                                             AssemblerDebugInfo debug_info,
                                             MachineRepresentation rep,
                                             Node* initial_value)
    : CodeAssemblerVariable(assembler, debug_info, rep) {
  impl_->set_debug_info(debug_info);
  Bind(initial_value);
}
#endif  // DEBUG

CodeAssemblerVariable::~CodeAssemblerVariable() {
  state_->variables_.erase(impl_);
}

void CodeAssemblerVariable::Bind(Node* value) { impl_->value_ = value; }

Node* CodeAssemblerVariable::value() const {
#if DEBUG
  if (!IsBound()) {
    std::stringstream str;
    str << "#Use of unbound variable:"
        << "#\n    Variable:      " << *this << "#\n    Current Block: ";
    state_->PrintCurrentBlock(str);
    FATAL("%s", str.str().c_str());
  }
  if (!state_->InsideBlock()) {
    std::stringstream str;
    str << "#Accessing variable value outside a block:"
        << "#\n    Variable:      " << *this;
    FATAL("%s", str.str().c_str());
  }
#endif  // DEBUG
  return impl_->value_;
}

MachineRepresentation CodeAssemblerVariable::rep() const { return impl_->rep_; }

bool CodeAssemblerVariable::IsBound() const { return impl_->value_ != nullptr; }

std::ostream& operator<<(std::ostream& os,
                         const CodeAssemblerVariable::Impl& impl) {
#if DEBUG
  AssemblerDebugInfo info = impl.debug_info();
  if (info.name) os << "V" << info;
#endif  // DEBUG
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const CodeAssemblerVariable& variable) {
  os << *variable.impl_;
  return os;
}

CodeAssemblerLabel::CodeAssemblerLabel(CodeAssembler* assembler,
                                       size_t vars_count,
                                       CodeAssemblerVariable* const* vars,
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

CodeAssemblerLabel::~CodeAssemblerLabel() { label_->~RawMachineLabel(); }

void CodeAssemblerLabel::MergeVariables() {
  ++merge_count_;
  for (CodeAssemblerVariable::Impl* var : state_->variables_) {
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
#if DEBUG
          if (find_if(i->second.begin(), i->second.end(),
                      [node](Node* e) -> bool { return node != e; }) !=
              i->second.end()) {
            std::stringstream str;
            str << "Unmerged variable found when jumping to block. \n"
                << "#    Variable:      " << *var;
            if (bound_) {
              str << "\n#    Target block:  " << *label_->block();
            }
            str << "\n#    Current Block: ";
            state_->PrintCurrentBlock(str);
            FATAL("%s", str.str().c_str());
          }
#endif  // DEBUG
        }
      }
    }
  }
}

#if DEBUG
void CodeAssemblerLabel::Bind(AssemblerDebugInfo debug_info) {
  if (bound_) {
    std::stringstream str;
    str << "Cannot bind the same label twice:"
        << "\n#    current:  " << debug_info
        << "\n#    previous: " << *label_->block();
    FATAL("%s", str.str().c_str());
  }
  if (FLAG_enable_source_at_csa_bind) {
    state_->raw_assembler_->SetSourcePosition(debug_info.file, debug_info.line);
  }
  state_->raw_assembler_->Bind(label_, debug_info);
  UpdateVariablesAfterBind();
}
#endif  // DEBUG

void CodeAssemblerLabel::Bind() {
  DCHECK(!bound_);
  state_->raw_assembler_->Bind(label_);
  UpdateVariablesAfterBind();
}

void CodeAssemblerLabel::UpdateVariablesAfterBind() {
  // Make sure that all variables that have changed along any path up to this
  // point are marked as merge variables.
  for (auto var : state_->variables_) {
    Node* shared_value = nullptr;
    auto i = variable_merges_.find(var);
    if (i != variable_merges_.end()) {
      for (auto value : i->second) {
        DCHECK_NOT_NULL(value);
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
#if DEBUG
    bool not_found = i == variable_merges_.end();
    if (not_found || i->second.size() != merge_count_) {
      std::stringstream str;
      str << "A variable that has been marked as beeing merged at the label"
          << "\n# doesn't have a bound value along all of the paths that "
          << "\n# have been merged into the label up to this point."
          << "\n#"
          << "\n# This can happen in the following cases:"
          << "\n# - By explicitly marking it so in the label constructor"
          << "\n# - By having seen different bound values at branches"
          << "\n#"
          << "\n# Merge count:     expected=" << merge_count_
          << " vs. found=" << (not_found ? 0 : i->second.size())
          << "\n# Variable:      " << *var_impl
          << "\n# Current Block: " << *label_->block();
      FATAL("%s", str.str().c_str());
    }
#endif  // DEBUG
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

void CodeAssemblerParameterizedLabelBase::AddInputs(std::vector<Node*> inputs) {
  if (!phi_nodes_.empty()) {
    DCHECK_EQ(inputs.size(), phi_nodes_.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
      // We use {nullptr} as a sentinel for an uninitialized value.
      if (phi_nodes_[i] == nullptr) continue;
      state_->raw_assembler_->AppendPhiInput(phi_nodes_[i], inputs[i]);
    }
  } else {
    DCHECK_EQ(inputs.size(), phi_inputs_.size());
    for (size_t i = 0; i < inputs.size(); ++i) {
      phi_inputs_[i].push_back(inputs[i]);
    }
  }
}

Node* CodeAssemblerParameterizedLabelBase::CreatePhi(
    MachineRepresentation rep, const std::vector<Node*>& inputs) {
  for (Node* input : inputs) {
    // We use {nullptr} as a sentinel for an uninitialized value. We must not
    // create phi nodes for these.
    if (input == nullptr) return nullptr;
  }
  return state_->raw_assembler_->Phi(rep, static_cast<int>(inputs.size()),
                                     &inputs.front());
}

const std::vector<Node*>& CodeAssemblerParameterizedLabelBase::CreatePhis(
    std::vector<MachineRepresentation> representations) {
  DCHECK(is_used());
  DCHECK(phi_nodes_.empty());
  phi_nodes_.reserve(phi_inputs_.size());
  DCHECK_EQ(representations.size(), phi_inputs_.size());
  for (size_t i = 0; i < phi_inputs_.size(); ++i) {
    phi_nodes_.push_back(CreatePhi(representations[i], phi_inputs_[i]));
  }
  return phi_nodes_;
}

void CodeAssemblerState::PushExceptionHandler(
    CodeAssemblerExceptionHandlerLabel* label) {
  exception_handler_labels_.push_back(label);
}

void CodeAssemblerState::PopExceptionHandler() {
  exception_handler_labels_.pop_back();
}

CodeAssemblerScopedExceptionHandler::CodeAssemblerScopedExceptionHandler(
    CodeAssembler* assembler, CodeAssemblerExceptionHandlerLabel* label)
    : has_handler_(label != nullptr),
      assembler_(assembler),
      compatibility_label_(nullptr),
      exception_(nullptr) {
  if (has_handler_) {
    assembler_->state()->PushExceptionHandler(label);
  }
}

CodeAssemblerScopedExceptionHandler::CodeAssemblerScopedExceptionHandler(
    CodeAssembler* assembler, CodeAssemblerLabel* label,
    TypedCodeAssemblerVariable<Object>* exception)
    : has_handler_(label != nullptr),
      assembler_(assembler),
      compatibility_label_(label),
      exception_(exception) {
  if (has_handler_) {
    label_ = base::make_unique<CodeAssemblerExceptionHandlerLabel>(
        assembler, CodeAssemblerLabel::kDeferred);
    assembler_->state()->PushExceptionHandler(label_.get());
  }
}

CodeAssemblerScopedExceptionHandler::~CodeAssemblerScopedExceptionHandler() {
  if (has_handler_) {
    assembler_->state()->PopExceptionHandler();
  }
  if (label_ && label_->is_used()) {
    CodeAssembler::Label skip(assembler_);
    bool inside_block = assembler_->state()->InsideBlock();
    if (inside_block) {
      assembler_->Goto(&skip);
    }
    TNode<Object> e;
    assembler_->Bind(label_.get(), &e);
    *exception_ = e;
    assembler_->Goto(compatibility_label_);
    if (inside_block) {
      assembler_->Bind(&skip);
    }
  }
}

}  // namespace compiler

Address CheckObjectType(Address raw_value, Address raw_type,
                        Address raw_location) {
#ifdef DEBUG
  Object value(raw_value);
  Smi type(raw_type);
  String location = String::cast(Object(raw_location));
  const char* expected;
  switch (static_cast<ObjectType>(type.value())) {
#define TYPE_CASE(Name)                                 \
  case ObjectType::k##Name:                             \
    if (value.Is##Name()) return Smi::FromInt(0).ptr(); \
    expected = #Name;                                   \
    break;
#define TYPE_STRUCT_CASE(NAME, Name, name)              \
  case ObjectType::k##Name:                             \
    if (value.Is##Name()) return Smi::FromInt(0).ptr(); \
    expected = #Name;                                   \
    break;

    TYPE_CASE(Object)
    TYPE_CASE(Smi)
    TYPE_CASE(HeapObject)
    OBJECT_TYPE_LIST(TYPE_CASE)
    HEAP_OBJECT_TYPE_LIST(TYPE_CASE)
    STRUCT_LIST(TYPE_STRUCT_CASE)
#undef TYPE_CASE
#undef TYPE_STRUCT_CASE
  }
  std::stringstream value_description;
  value.Print(value_description);
  FATAL(
      "Type cast failed in %s\n"
      "  Expected %s but found %s",
      location.ToAsciiArray(), expected, value_description.str().c_str());
#else
  UNREACHABLE();
#endif
}

}  // namespace internal
}  // namespace v8
