// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "factory.h"
#include "hydrogen.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/lithium-arm.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

#define DEFINE_COMPILE(type)                                         \
  LInstruction* H##type::CompileToLithium(LChunkBuilder* builder) {  \
    return builder->Do##type(this);                                  \
  }
HYDROGEN_CONCRETE_INSTRUCTION_LIST(DEFINE_COMPILE)
#undef DEFINE_COMPILE


const char* Representation::Mnemonic() const {
  switch (kind_) {
    case kNone: return "v";
    case kTagged: return "t";
    case kDouble: return "d";
    case kInteger32: return "i";
    default:
      UNREACHABLE();
      return NULL;
  }
}


static int32_t ConvertAndSetOverflow(int64_t result, bool* overflow) {
  if (result > kMaxInt) {
    *overflow = true;
    return kMaxInt;
  }
  if (result < kMinInt) {
    *overflow = true;
    return kMinInt;
  }
  return static_cast<int32_t>(result);
}


static int32_t AddWithoutOverflow(int32_t a, int32_t b, bool* overflow) {
  int64_t result = static_cast<int64_t>(a) + static_cast<int64_t>(b);
  return ConvertAndSetOverflow(result, overflow);
}


static int32_t SubWithoutOverflow(int32_t a, int32_t b, bool* overflow) {
  int64_t result = static_cast<int64_t>(a) - static_cast<int64_t>(b);
  return ConvertAndSetOverflow(result, overflow);
}


static int32_t MulWithoutOverflow(int32_t a, int32_t b, bool* overflow) {
  int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b);
  return ConvertAndSetOverflow(result, overflow);
}


int32_t Range::Mask() const {
  if (lower_ == upper_) return lower_;
  if (lower_ >= 0) {
    int32_t res = 1;
    while (res < upper_) {
      res = (res << 1) | 1;
    }
    return res;
  }
  return 0xffffffff;
}


void Range::AddConstant(int32_t value) {
  if (value == 0) return;
  bool may_overflow = false;  // Overflow is ignored here.
  lower_ = AddWithoutOverflow(lower_, value, &may_overflow);
  upper_ = AddWithoutOverflow(upper_, value, &may_overflow);
  Verify();
}


bool Range::AddAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  lower_ = AddWithoutOverflow(lower_, other->lower(), &may_overflow);
  upper_ = AddWithoutOverflow(upper_, other->upper(), &may_overflow);
  KeepOrder();
  Verify();
  return may_overflow;
}


bool Range::SubAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  lower_ = SubWithoutOverflow(lower_, other->upper(), &may_overflow);
  upper_ = SubWithoutOverflow(upper_, other->lower(), &may_overflow);
  KeepOrder();
  Verify();
  return may_overflow;
}


void Range::KeepOrder() {
  if (lower_ > upper_) {
    int32_t tmp = lower_;
    lower_ = upper_;
    upper_ = tmp;
  }
}


void Range::Verify() const {
  ASSERT(lower_ <= upper_);
}


bool Range::MulAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  int v1 = MulWithoutOverflow(lower_, other->lower(), &may_overflow);
  int v2 = MulWithoutOverflow(lower_, other->upper(), &may_overflow);
  int v3 = MulWithoutOverflow(upper_, other->lower(), &may_overflow);
  int v4 = MulWithoutOverflow(upper_, other->upper(), &may_overflow);
  lower_ = Min(Min(v1, v2), Min(v3, v4));
  upper_ = Max(Max(v1, v2), Max(v3, v4));
  Verify();
  return may_overflow;
}


const char* HType::ToString() {
  switch (type_) {
    case kTagged: return "tagged";
    case kTaggedPrimitive: return "primitive";
    case kTaggedNumber: return "number";
    case kSmi: return "smi";
    case kHeapNumber: return "heap-number";
    case kString: return "string";
    case kBoolean: return "boolean";
    case kNonPrimitive: return "non-primitive";
    case kJSArray: return "array";
    case kJSObject: return "object";
    case kUninitialized: return "uninitialized";
  }
  UNREACHABLE();
  return "Unreachable code";
}


const char* HType::ToShortString() {
  switch (type_) {
    case kTagged: return "t";
    case kTaggedPrimitive: return "p";
    case kTaggedNumber: return "n";
    case kSmi: return "m";
    case kHeapNumber: return "h";
    case kString: return "s";
    case kBoolean: return "b";
    case kNonPrimitive: return "r";
    case kJSArray: return "a";
    case kJSObject: return "o";
    case kUninitialized: return "z";
  }
  UNREACHABLE();
  return "Unreachable code";
}


HType HType::TypeFromValue(Handle<Object> value) {
  HType result = HType::Tagged();
  if (value->IsSmi()) {
    result = HType::Smi();
  } else if (value->IsHeapNumber()) {
    result = HType::HeapNumber();
  } else if (value->IsString()) {
    result = HType::String();
  } else if (value->IsBoolean()) {
    result = HType::Boolean();
  } else if (value->IsJSObject()) {
    result = HType::JSObject();
  } else if (value->IsJSArray()) {
    result = HType::JSArray();
  }
  return result;
}


int HValue::LookupOperandIndex(int occurrence_index, HValue* op) const {
  for (int i = 0; i < OperandCount(); ++i) {
    if (OperandAt(i) == op) {
      if (occurrence_index == 0) return i;
      --occurrence_index;
    }
  }
  return -1;
}


bool HValue::IsDefinedAfter(HBasicBlock* other) const {
  return block()->block_id() > other->block_id();
}


bool HValue::UsesMultipleTimes(HValue* op) const {
  bool seen = false;
  for (int i = 0; i < OperandCount(); ++i) {
    if (OperandAt(i) == op) {
      if (seen) return true;
      seen = true;
    }
  }
  return false;
}


bool HValue::Equals(HValue* other) const {
  if (other->opcode() != opcode()) return false;
  if (!other->representation().Equals(representation())) return false;
  if (!other->type_.Equals(type_)) return false;
  if (OperandCount() != other->OperandCount()) return false;
  for (int i = 0; i < OperandCount(); ++i) {
    if (OperandAt(i)->id() != other->OperandAt(i)->id()) return false;
  }
  bool result = DataEquals(other);
  ASSERT(!result || Hashcode() == other->Hashcode());
  return result;
}


intptr_t HValue::Hashcode() const {
  intptr_t result = opcode();
  int count = OperandCount();
  for (int i = 0; i < count; ++i) {
    result = result * 19 + OperandAt(i)->id() + (result >> 7);
  }
  return result;
}


void HValue::SetOperandAt(int index, HValue* value) {
  ASSERT(value == NULL || !value->representation().IsNone());
  RegisterUse(index, value);
  InternalSetOperandAt(index, value);
}


void HValue::ReplaceAndDelete(HValue* other) {
  ReplaceValue(other);
  Delete();
}


void HValue::ReplaceValue(HValue* other) {
  ZoneList<HValue*> start_uses(2);
  for (int i = 0; i < uses_.length(); ++i) {
    HValue* use = uses_.at(i);
    if (!use->block()->IsStartBlock()) {
      InternalReplaceAtUse(use, other);
      other->uses_.Add(use);
    } else {
      start_uses.Add(use);
    }
  }
  uses_.Clear();
  uses_.AddAll(start_uses);
}


void HValue::ClearOperands() {
  for (int i = 0; i < OperandCount(); ++i) {
    SetOperandAt(i, NULL);
  }
}


void HValue::Delete() {
  ASSERT(HasNoUses());
  ClearOperands();
  DeleteFromGraph();
}


void HValue::ReplaceAtUse(HValue* use, HValue* other) {
  for (int i = 0; i < use->OperandCount(); ++i) {
    if (use->OperandAt(i) == this) {
      use->SetOperandAt(i, other);
    }
  }
}


void HValue::ReplaceFirstAtUse(HValue* use, HValue* other, Representation r) {
  for (int i = 0; i < use->OperandCount(); ++i) {
    if (use->RequiredInputRepresentation(i).Equals(r) &&
        use->OperandAt(i) == this) {
      use->SetOperandAt(i, other);
      return;
    }
  }
}


void HValue::InternalReplaceAtUse(HValue* use, HValue* other) {
  for (int i = 0; i < use->OperandCount(); ++i) {
    if (use->OperandAt(i) == this) {
      // Call internal method that does not update use lists. The caller is
      // responsible for doing so.
      use->InternalSetOperandAt(i, other);
    }
  }
}


void HValue::SetBlock(HBasicBlock* block) {
  ASSERT(block_ == NULL || block == NULL);
  block_ = block;
  if (id_ == kNoNumber && block != NULL) {
    id_ = block->graph()->GetNextValueID(this);
  }
}


void HValue::PrintTypeTo(HType type, StringStream* stream) {
  stream->Add(type.ToShortString());
}


void HValue::PrintNameTo(StringStream* stream) {
  stream->Add("%s%d", representation_.Mnemonic(), id());
}


bool HValue::UpdateInferredType() {
  HType type = CalculateInferredType();
  bool result = (!type.Equals(type_));
  type_ = type;
  return result;
}


void HValue::RegisterUse(int index, HValue* new_value) {
  HValue* old_value = OperandAt(index);
  if (old_value == new_value) return;
  if (old_value != NULL) {
    ASSERT(old_value->uses_.Contains(this));
    old_value->uses_.RemoveElement(this);
  }
  if (new_value != NULL) {
    new_value->uses_.Add(this);
  }
}


void HValue::AddNewRange(Range* r) {
  if (!HasRange()) ComputeInitialRange();
  if (!HasRange()) range_ = new Range();
  ASSERT(HasRange());
  r->StackUpon(range_);
  range_ = r;
}


void HValue::RemoveLastAddedRange() {
  ASSERT(HasRange());
  ASSERT(range_->next() != NULL);
  range_ = range_->next();
}


void HValue::ComputeInitialRange() {
  ASSERT(!HasRange());
  range_ = InferRange();
  ASSERT(HasRange());
}


void HInstruction::PrintTo(StringStream* stream) const {
  stream->Add("%s", Mnemonic());
  if (HasSideEffects()) stream->Add("*");
  stream->Add(" ");
  PrintDataTo(stream);

  if (range() != NULL) {
    stream->Add(" range[%d,%d,m0=%d]",
                range()->lower(),
                range()->upper(),
                static_cast<int>(range()->CanBeMinusZero()));
  }

  int changes_flags = (flags() & HValue::ChangesFlagsMask());
  if (changes_flags != 0) {
    stream->Add(" changes[0x%x]", changes_flags);
  }

  if (representation().IsTagged() && !type().Equals(HType::Tagged())) {
    stream->Add(" type[%s]", type().ToString());
  }
}


void HInstruction::Unlink() {
  ASSERT(IsLinked());
  ASSERT(!IsControlInstruction());  // Must never move control instructions.
  clear_block();
  if (previous_ != NULL) previous_->next_ = next_;
  if (next_ != NULL) next_->previous_ = previous_;
}


void HInstruction::InsertBefore(HInstruction* next) {
  ASSERT(!IsLinked());
  ASSERT(!next->IsBlockEntry());
  ASSERT(!IsControlInstruction());
  ASSERT(!next->block()->IsStartBlock());
  ASSERT(next->previous_ != NULL);
  HInstruction* prev = next->previous();
  prev->next_ = this;
  next->previous_ = this;
  next_ = next;
  previous_ = prev;
  SetBlock(next->block());
}


void HInstruction::InsertAfter(HInstruction* previous) {
  ASSERT(!IsLinked());
  ASSERT(!previous->IsControlInstruction());
  ASSERT(!IsControlInstruction() || previous->next_ == NULL);
  HBasicBlock* block = previous->block();
  // Never insert anything except constants into the start block after finishing
  // it.
  if (block->IsStartBlock() && block->IsFinished() && !IsConstant()) {
    ASSERT(block->end()->SecondSuccessor() == NULL);
    InsertAfter(block->end()->FirstSuccessor()->first());
    return;
  }

  // If we're inserting after an instruction with side-effects that is
  // followed by a simulate instruction, we need to insert after the
  // simulate instruction instead.
  HInstruction* next = previous->next_;
  if (previous->HasSideEffects() && next != NULL) {
    ASSERT(next->IsSimulate());
    previous = next;
    next = previous->next_;
  }

  previous_ = previous;
  next_ = next;
  SetBlock(block);
  previous->next_ = this;
  if (next != NULL) next->previous_ = this;
}


#ifdef DEBUG
void HInstruction::Verify() const {
  // Verify that input operands are defined before use.
  HBasicBlock* cur_block = block();
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* other_operand = OperandAt(i);
    HBasicBlock* other_block = other_operand->block();
    if (cur_block == other_block) {
      if (!other_operand->IsPhi()) {
        HInstruction* cur = cur_block->first();
        while (cur != NULL) {
          ASSERT(cur != this);  // We should reach other_operand before!
          if (cur == other_operand) break;
          cur = cur->next();
        }
        // Must reach other operand in the same block!
        ASSERT(cur == other_operand);
      }
    } else {
      ASSERT(other_block->Dominates(cur_block));
    }
  }

  // Verify that instructions that may have side-effects are followed
  // by a simulate instruction.
  if (HasSideEffects() && !IsOsrEntry()) {
    ASSERT(next()->IsSimulate());
  }
}
#endif


HCall::HCall(int count) : arguments_(Zone::NewArray<HValue*>(count), count) {
  for (int i = 0; i < count; ++i) arguments_[i] = NULL;
  set_representation(Representation::Tagged());
  SetFlagMask(AllSideEffects());
}


void HCall::PrintDataTo(StringStream* stream) const {
  stream->Add("(");
  for (int i = 0; i < arguments_.length(); ++i) {
    if (i != 0) stream->Add(", ");
    arguments_.at(i)->PrintNameTo(stream);
  }
  stream->Add(")");
}


void HClassOfTest::PrintDataTo(StringStream* stream) const {
  stream->Add("class_of_test(");
  value()->PrintTo(stream);
  stream->Add(", \"%o\")", *class_name());
}


void HAccessArgumentsAt::PrintDataTo(StringStream* stream) const {
  arguments()->PrintNameTo(stream);
  stream->Add("[");
  index()->PrintNameTo(stream);
  stream->Add("], length ");
  length()->PrintNameTo(stream);
}


void HCall::SetArgumentAt(int index, HPushArgument* push_argument) {
  push_argument->set_argument_index(index);
  SetOperandAt(index, push_argument);
}


void HCallConstantFunction::PrintDataTo(StringStream* stream) const {
  if (IsApplyFunction()) {
    stream->Add("SPECIAL function: apply");
  } else {
    stream->Add("%s", *(function()->shared()->DebugName()->ToCString()));
  }
  HCall::PrintDataTo(stream);
}


void HBranch::PrintDataTo(StringStream* stream) const {
  int first_id = FirstSuccessor()->block_id();
  int second_id = SecondSuccessor()->block_id();
  stream->Add("on ");
  value()->PrintNameTo(stream);
  stream->Add(" (B%d, B%d)", first_id, second_id);
}


void HCompareMapAndBranch::PrintDataTo(StringStream* stream) const {
  stream->Add("on ");
  value()->PrintNameTo(stream);
  stream->Add(" (%p)", *map());
}


void HGoto::PrintDataTo(StringStream* stream) const {
  stream->Add("B%d", FirstSuccessor()->block_id());
}


void HReturn::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
}


void HThrow::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
}


const char* HUnaryMathOperation::OpName() const {
  switch (op()) {
    case kMathFloor: return "floor";
    case kMathRound: return "round";
    case kMathCeil: return "ceil";
    case kMathAbs: return "abs";
    case kMathLog: return "log";
    case kMathSin: return "sin";
    case kMathCos: return "cos";
    case kMathTan: return "tan";
    case kMathASin: return "asin";
    case kMathACos: return "acos";
    case kMathATan: return "atan";
    case kMathExp: return "exp";
    case kMathSqrt: return "sqrt";
    default: break;
  }
  return "(unknown operation)";
}


void HUnaryMathOperation::PrintDataTo(StringStream* stream) const {
  const char* name = OpName();
  stream->Add("%s ", name);
  value()->PrintNameTo(stream);
}


void HUnaryOperation::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
}


void HHasInstanceType::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
  switch (from_) {
    case FIRST_JS_OBJECT_TYPE:
      if (to_ == LAST_TYPE) stream->Add(" spec_object");
      break;
    case JS_REGEXP_TYPE:
      if (to_ == JS_REGEXP_TYPE) stream->Add(" reg_exp");
      break;
    case JS_ARRAY_TYPE:
      if (to_ == JS_ARRAY_TYPE) stream->Add(" array");
      break;
    case JS_FUNCTION_TYPE:
      if (to_ == JS_FUNCTION_TYPE) stream->Add(" function");
      break;
    default:
      break;
  }
}


void HTypeofIs::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
  stream->Add(" == ");
  stream->Add(type_literal_->ToAsciiVector());
}


void HPushArgument::PrintDataTo(StringStream* stream) const {
  HUnaryOperation::PrintDataTo(stream);
  if (argument_index() != -1) {
    stream->Add(" [%d]", argument_index_);
  }
}


void HChange::PrintDataTo(StringStream* stream) const {
  HUnaryOperation::PrintDataTo(stream);
  stream->Add(" %s to %s", from_.Mnemonic(), to_.Mnemonic());

  if (CanTruncateToInt32()) stream->Add(" truncating-int32");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
}


HCheckInstanceType* HCheckInstanceType::NewIsJSObjectOrJSFunction(
    HValue* value)  {
  STATIC_ASSERT((LAST_JS_OBJECT_TYPE + 1) == JS_FUNCTION_TYPE);
  return new HCheckInstanceType(value, FIRST_JS_OBJECT_TYPE, JS_FUNCTION_TYPE);
}


void HCheckMap::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
  stream->Add(" %p", *map());
}


void HCheckFunction::PrintDataTo(StringStream* stream) const {
  value()->PrintNameTo(stream);
  stream->Add(" %p", *target());
}


void HCallKeyed::PrintDataTo(StringStream* stream) const {
  stream->Add("[");
  key()->PrintNameTo(stream);
  stream->Add("](");
  for (int i = 1; i < arguments_.length(); ++i) {
    if (i != 1) stream->Add(", ");
    arguments_.at(i)->PrintNameTo(stream);
  }
  stream->Add(")");
}


void HCallNamed::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s ", *name_string);
  HCall::PrintDataTo(stream);
}


void HCallGlobal::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s ", *name_string);
  HCall::PrintDataTo(stream);
}


void HCallRuntime::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name_string = name()->ToCString();
  stream->Add("%s ", *name_string);
  HCall::PrintDataTo(stream);
}

void HCallStub::PrintDataTo(StringStream* stream) const {
  stream->Add("%s(%d)",
              CodeStub::MajorName(major_key_, false),
              argument_count_);
}


Range* HValue::InferRange() {
  if (representation().IsTagged()) {
    // Tagged values are always in int32 range when converted to integer,
    // but they can contain -0.
    Range* result = new Range();
    result->set_can_be_minus_zero(true);
    return result;
  } else if (representation().IsNone()) {
    return NULL;
  } else {
    return new Range();
  }
}


Range* HConstant::InferRange() {
  if (has_int32_value_) {
    Range* result = new Range(int32_value_, int32_value_);
    result->set_can_be_minus_zero(false);
    return result;
  }
  return HInstruction::InferRange();
}


Range* HPhi::InferRange() {
  if (representation().IsInteger32()) {
    if (block()->IsLoopHeader()) {
      Range* range = new Range(kMinInt, kMaxInt);
      return range;
    } else {
      Range* range = OperandAt(0)->range()->Copy();
      for (int i = 1; i < OperandCount(); ++i) {
        range->Union(OperandAt(i)->range());
      }
      return range;
    }
  } else {
    return HValue::InferRange();
  }
}


Range* HAdd::InferRange() {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy();
    if (!res->AddAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    bool m0 = a->CanBeMinusZero() && b->CanBeMinusZero();
    res->set_can_be_minus_zero(m0);
    return res;
  } else {
    return HArithmeticBinaryOperation::InferRange();
  }
}


Range* HSub::InferRange() {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy();
    if (!res->SubAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    res->set_can_be_minus_zero(a->CanBeMinusZero() && b->CanBeZero());
    return res;
  } else {
    return HArithmeticBinaryOperation::InferRange();
  }
}


Range* HMul::InferRange() {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy();
    if (!res->MulAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    bool m0 = (a->CanBeZero() && b->CanBeNegative()) ||
        (a->CanBeNegative() && b->CanBeZero());
    res->set_can_be_minus_zero(m0);
    return res;
  } else {
    return HArithmeticBinaryOperation::InferRange();
  }
}


Range* HDiv::InferRange() {
  if (representation().IsInteger32()) {
    Range* result = new Range();
    if (left()->range()->CanBeMinusZero()) {
      result->set_can_be_minus_zero(true);
    }

    if (left()->range()->CanBeZero() && right()->range()->CanBeNegative()) {
      result->set_can_be_minus_zero(true);
    }

    if (right()->range()->Includes(-1) && left()->range()->Includes(kMinInt)) {
      SetFlag(HValue::kCanOverflow);
    }

    if (!right()->range()->CanBeZero()) {
      ClearFlag(HValue::kCanBeDivByZero);
    }
    return result;
  } else {
    return HArithmeticBinaryOperation::InferRange();
  }
}


Range* HMod::InferRange() {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* result = new Range();
    if (a->CanBeMinusZero() || a->CanBeNegative()) {
      result->set_can_be_minus_zero(true);
    }
    if (!right()->range()->CanBeZero()) {
      ClearFlag(HValue::kCanBeDivByZero);
    }
    return result;
  } else {
    return HArithmeticBinaryOperation::InferRange();
  }
}


void HPhi::PrintTo(StringStream* stream) const {
  stream->Add("[");
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* value = OperandAt(i);
    stream->Add(" ");
    value->PrintNameTo(stream);
    stream->Add(" ");
  }
  stream->Add(" uses%d_%di_%dd_%dt]",
              uses()->length(),
              int32_non_phi_uses() + int32_indirect_uses(),
              double_non_phi_uses() + double_indirect_uses(),
              tagged_non_phi_uses() + tagged_indirect_uses());
}


void HPhi::AddInput(HValue* value) {
  inputs_.Add(NULL);
  SetOperandAt(OperandCount() - 1, value);
  // Mark phis that may have 'arguments' directly or indirectly as an operand.
  if (!CheckFlag(kIsArguments) && value->CheckFlag(kIsArguments)) {
    SetFlag(kIsArguments);
  }
}


bool HPhi::HasReceiverOperand() {
  for (int i = 0; i < OperandCount(); i++) {
    if (OperandAt(i)->IsParameter() &&
        HParameter::cast(OperandAt(i))->index() == 0) {
      return true;
    }
  }
  return false;
}


HValue* HPhi::GetRedundantReplacement() const {
  HValue* candidate = NULL;
  int count = OperandCount();
  int position = 0;
  while (position < count && candidate == NULL) {
    HValue* current = OperandAt(position++);
    if (current != this) candidate = current;
  }
  while (position < count) {
    HValue* current = OperandAt(position++);
    if (current != this && current != candidate) return NULL;
  }
  ASSERT(candidate != this);
  return candidate;
}


void HPhi::DeleteFromGraph() {
  ASSERT(block() != NULL);
  block()->RemovePhi(this);
  ASSERT(block() == NULL);
}


void HPhi::InitRealUses(int phi_id) {
  // Initialize real uses.
  phi_id_ = phi_id;
  for (int j = 0; j < uses()->length(); j++) {
    HValue* use = uses()->at(j);
    if (!use->IsPhi()) {
      int index = use->LookupOperandIndex(0, this);
      Representation req_rep = use->RequiredInputRepresentation(index);
      non_phi_uses_[req_rep.kind()]++;
    }
  }
}


void HPhi::AddNonPhiUsesFrom(HPhi* other) {
  for (int i = 0; i < Representation::kNumRepresentations; i++) {
    indirect_uses_[i] += other->non_phi_uses_[i];
  }
}


void HPhi::AddIndirectUsesTo(int* dest) {
  for (int i = 0; i < Representation::kNumRepresentations; i++) {
    dest[i] += indirect_uses_[i];
  }
}


void HSimulate::PrintDataTo(StringStream* stream) const {
  stream->Add("id=%d ", ast_id());
  if (pop_count_ > 0) stream->Add("pop %d", pop_count_);
  if (values_.length() > 0) {
    if (pop_count_ > 0) stream->Add(" /");
    for (int i = 0; i < values_.length(); ++i) {
      if (!HasAssignedIndexAt(i)) {
        stream->Add(" push ");
      } else {
        stream->Add(" var[%d] = ", GetAssignedIndexAt(i));
      }
      values_[i]->PrintNameTo(stream);
    }
  }
}


void HEnterInlined::PrintDataTo(StringStream* stream) const {
  SmartPointer<char> name = function()->debug_name()->ToCString();
  stream->Add("%s, id=%d", *name, function()->id());
}


HConstant::HConstant(Handle<Object> handle, Representation r)
    : handle_(handle),
      constant_type_(HType::TypeFromValue(handle)),
      has_int32_value_(false),
      int32_value_(0),
      has_double_value_(false),
      double_value_(0)  {
  set_representation(r);
  SetFlag(kUseGVN);
  if (handle_->IsNumber()) {
    double n = handle_->Number();
    has_int32_value_ = static_cast<double>(static_cast<int32_t>(n)) == n;
    if (has_int32_value_) int32_value_ = static_cast<int32_t>(n);
    double_value_ = n;
    has_double_value_ = true;
  }
}


HConstant* HConstant::CopyToRepresentation(Representation r) const {
  if (r.IsInteger32() && !has_int32_value_) return NULL;
  if (r.IsDouble() && !has_double_value_) return NULL;
  return new HConstant(handle_, r);
}


HConstant* HConstant::CopyToTruncatedInt32() const {
  if (!has_double_value_) return NULL;
  int32_t truncated = NumberToInt32(*handle_);
  return new HConstant(Factory::NewNumberFromInt(truncated),
                       Representation::Integer32());
}


void HConstant::PrintDataTo(StringStream* stream) const {
  handle()->ShortPrint(stream);
}


bool HArrayLiteral::IsCopyOnWrite() const {
  return constant_elements()->map() == Heap::fixed_cow_array_map();
}


void HBinaryOperation::PrintDataTo(StringStream* stream) const {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  if (CheckFlag(kCanOverflow)) stream->Add(" !");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
}


Range* HBitAnd::InferRange() {
  Range* a = left()->range();
  Range* b = right()->range();
  int32_t a_mask = 0xffffffff;
  int32_t b_mask = 0xffffffff;
  if (a != NULL) a_mask = a->Mask();
  if (b != NULL) b_mask = b->Mask();
  int32_t result_mask = a_mask & b_mask;
  if (result_mask >= 0) {
    return new Range(0, result_mask);
  } else {
    return HBinaryOperation::InferRange();
  }
}


Range* HBitOr::InferRange() {
  Range* a = left()->range();
  Range* b = right()->range();
  int32_t a_mask = 0xffffffff;
  int32_t b_mask = 0xffffffff;
  if (a != NULL) a_mask = a->Mask();
  if (b != NULL) b_mask = b->Mask();
  int32_t result_mask = a_mask | b_mask;
  if (result_mask >= 0) {
    return new Range(0, result_mask);
  } else {
    return HBinaryOperation::InferRange();
  }
}


Range* HSar::InferRange() {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      int32_t val = c->Integer32Value();
      Range* result = NULL;
      Range* left_range = left()->range();
      if (left_range == NULL) {
        result = new Range();
      } else {
        result = left_range->Copy();
      }
      result->Sar(val);
      return result;
    }
  }

  return HBinaryOperation::InferRange();
}


Range* HShl::InferRange() {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      int32_t val = c->Integer32Value();
      Range* result = NULL;
      Range* left_range = left()->range();
      if (left_range == NULL) {
        result = new Range();
      } else {
        result = left_range->Copy();
      }
      result->Shl(val);
      return result;
    }
  }

  return HBinaryOperation::InferRange();
}



void HCompare::PrintDataTo(StringStream* stream) const {
  stream->Add(Token::Name(token()));
  stream->Add(" ");
  HBinaryOperation::PrintDataTo(stream);
}


void HCompare::SetInputRepresentation(Representation r) {
  input_representation_ = r;
  if (r.IsTagged()) {
    SetFlagMask(AllSideEffects());
    ClearFlag(kUseGVN);
  } else {
    ClearFlagMask(AllSideEffects());
    SetFlag(kUseGVN);
  }
}


void HParameter::PrintDataTo(StringStream* stream) const {
  stream->Add("%u", index());
}


void HLoadNamedField::PrintDataTo(StringStream* stream) const {
  object()->PrintNameTo(stream);
  stream->Add(" @%d%s", offset(), is_in_object() ? "[in-object]" : "");
}


void HLoadKeyed::PrintDataTo(StringStream* stream) const {
  object()->PrintNameTo(stream);
  stream->Add("[");
  key()->PrintNameTo(stream);
  stream->Add("]");
}


void HStoreNamed::PrintDataTo(StringStream* stream) const {
  object()->PrintNameTo(stream);
  stream->Add(".");
  ASSERT(name()->IsString());
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" = ");
  value()->PrintNameTo(stream);
}


void HStoreNamedField::PrintDataTo(StringStream* stream) const {
  HStoreNamed::PrintDataTo(stream);
  if (!transition().is_null()) {
    stream->Add(" (transition map %p)", *transition());
  }
}


void HStoreKeyed::PrintDataTo(StringStream* stream) const {
  object()->PrintNameTo(stream);
  stream->Add("[");
  key()->PrintNameTo(stream);
  stream->Add("] = ");
  value()->PrintNameTo(stream);
}


void HLoadGlobal::PrintDataTo(StringStream* stream) const {
  stream->Add("[%p]", *cell());
  if (check_hole_value()) stream->Add(" (deleteable/read-only)");
}


void HStoreGlobal::PrintDataTo(StringStream* stream) const {
  stream->Add("[%p] = ", *cell());
  value()->PrintNameTo(stream);
}


void HLoadContextSlot::PrintDataTo(StringStream* stream) const {
  stream->Add("(%d, %d)", context_chain_length(), slot_index());
}


// Implementation of type inference and type conversions. Calculates
// the inferred type of this instruction based on the input operands.

HType HValue::CalculateInferredType() const {
  return type_;
}


HType HCheckMap::CalculateInferredType() const {
  return value()->type();
}


HType HCheckFunction::CalculateInferredType() const {
  return value()->type();
}


HType HCheckNonSmi::CalculateInferredType() const {
  // TODO(kasperl): Is there any way to signal that this isn't a smi?
  return HType::Tagged();
}


HType HCheckSmi::CalculateInferredType() const {
  return HType::Smi();
}


HType HPhi::CalculateInferredType() const {
  HType result = HType::Uninitialized();
  for (int i = 0; i < OperandCount(); ++i) {
    HType current = OperandAt(i)->type();
    result = result.Combine(current);
  }
  return result;
}


HType HConstant::CalculateInferredType() const {
  return constant_type_;
}


HType HCompare::CalculateInferredType() const {
  return HType::Boolean();
}


HType HCompareJSObjectEq::CalculateInferredType() const {
  return HType::Boolean();
}


HType HUnaryPredicate::CalculateInferredType() const {
  return HType::Boolean();
}


HType HArithmeticBinaryOperation::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HAdd::CalculateInferredType() const {
  return HType::Tagged();
}


HType HBitAnd::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HBitXor::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HBitOr::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HBitNot::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HUnaryMathOperation::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HShl::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HShr::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HType HSar::CalculateInferredType() const {
  return HType::TaggedNumber();
}


HValue* HUnaryMathOperation::EnsureAndPropagateNotMinusZero(
    BitVector* visited) {
  visited->Add(id());
  if (representation().IsInteger32() &&
      !value()->representation().IsInteger32()) {
    if (value()->range() == NULL || value()->range()->CanBeMinusZero()) {
      SetFlag(kBailoutOnMinusZero);
    }
  }
  if (RequiredInputRepresentation(0).IsInteger32() &&
      representation().IsInteger32()) {
    return value();
  }
  return NULL;
}



HValue* HChange::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  if (from().IsInteger32()) return NULL;
  if (CanTruncateToInt32()) return NULL;
  if (value()->range() == NULL || value()->range()->CanBeMinusZero()) {
    SetFlag(kBailoutOnMinusZero);
  }
  ASSERT(!from().IsInteger32() || !to().IsInteger32());
  return NULL;
}


HValue* HMod::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  if (range() == NULL || range()->CanBeMinusZero()) {
    SetFlag(kBailoutOnMinusZero);
    return left();
  }
  return NULL;
}


HValue* HDiv::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  if (range() == NULL || range()->CanBeMinusZero()) {
    SetFlag(kBailoutOnMinusZero);
  }
  return NULL;
}


HValue* HMul::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  if (range() == NULL || range()->CanBeMinusZero()) {
    SetFlag(kBailoutOnMinusZero);
  }
  return NULL;
}


HValue* HSub::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  // Propagate to the left argument. If the left argument cannot be -0, then
  // the result of the add operation cannot be either.
  if (range() == NULL || range()->CanBeMinusZero()) {
    return left();
  }
  return NULL;
}


HValue* HAdd::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  // Propagate to the left argument. If the left argument cannot be -0, then
  // the result of the sub operation cannot be either.
  if (range() == NULL || range()->CanBeMinusZero()) {
    return left();
  }
  return NULL;
}


// Node-specific verification code is only included in debug mode.
#ifdef DEBUG

void HPhi::Verify() const {
  ASSERT(OperandCount() == block()->predecessors()->length());
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* value = OperandAt(i);
    HBasicBlock* defining_block = value->block();
    HBasicBlock* predecessor_block = block()->predecessors()->at(i);
    ASSERT(defining_block == predecessor_block ||
           defining_block->Dominates(predecessor_block));
  }
}


void HSimulate::Verify() const {
  HInstruction::Verify();
  ASSERT(HasAstId());
}


void HBoundsCheck::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckSmi::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckNonSmi::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckInstanceType::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckMap::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckFunction::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckPrototypeMaps::Verify() const {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}

#endif

} }  // namespace v8::internal
