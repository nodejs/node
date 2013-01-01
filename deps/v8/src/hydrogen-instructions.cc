// Copyright 2012 the V8 project authors. All rights reserved.
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
#elif V8_TARGET_ARCH_MIPS
#include "mips/lithium-mips.h"
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
    case kExternal: return "x";
    default:
      UNREACHABLE();
      return NULL;
  }
}


int HValue::LoopWeight() const {
  const int w = FLAG_loop_weight;
  static const int weights[] = { 1, w, w*w, w*w*w, w*w*w*w };
  return weights[Min(block()->LoopNestingDepth(),
                     static_cast<int>(ARRAY_SIZE(weights)-1))];
}


void HValue::AssumeRepresentation(Representation r) {
  if (CheckFlag(kFlexibleRepresentation)) {
    ChangeRepresentation(r);
    // The representation of the value is dictated by type feedback and
    // will not be changed later.
    ClearFlag(kFlexibleRepresentation);
  }
}


void HValue::InferRepresentation(HInferRepresentation* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
}


Representation HValue::RepresentationFromUses() {
  if (HasNoUses()) return Representation::None();

  // Array of use counts for each representation.
  int use_count[Representation::kNumRepresentations] = { 0 };

  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    Representation rep = use->observed_input_representation(it.index());
    if (rep.IsNone()) continue;
    if (FLAG_trace_representation) {
      PrintF("#%d %s is used by #%d %s as %s%s\n",
             id(), Mnemonic(), use->id(), use->Mnemonic(), rep.Mnemonic(),
             (use->CheckFlag(kTruncatingToInt32) ? "-trunc" : ""));
    }
    use_count[rep.kind()] += use->LoopWeight();
  }
  if (IsPhi()) HPhi::cast(this)->AddIndirectUsesTo(&use_count[0]);
  int tagged_count = use_count[Representation::kTagged];
  int double_count = use_count[Representation::kDouble];
  int int32_count = use_count[Representation::kInteger32];

  if (tagged_count > 0) return Representation::Tagged();
  if (double_count > 0) return Representation::Double();
  if (int32_count > 0) return Representation::Integer32();

  return Representation::None();
}


void HValue::UpdateRepresentation(Representation new_rep,
                                  HInferRepresentation* h_infer,
                                  const char* reason) {
  Representation r = representation();
  if (new_rep.is_more_general_than(r)) {
    // When an HConstant is marked "not convertible to integer", then
    // never try to represent it as an integer.
    if (new_rep.IsInteger32() && !IsConvertibleToInteger()) {
      new_rep = Representation::Tagged();
      if (FLAG_trace_representation) {
        PrintF("Changing #%d %s representation %s -> %s because it's NCTI"
               " (%s want i)\n",
               id(), Mnemonic(), r.Mnemonic(), new_rep.Mnemonic(), reason);
      }
    } else {
      if (FLAG_trace_representation) {
        PrintF("Changing #%d %s representation %s -> %s based on %s\n",
               id(), Mnemonic(), r.Mnemonic(), new_rep.Mnemonic(), reason);
      }
    }
    ChangeRepresentation(new_rep);
    AddDependantsToWorklist(h_infer);
  }
}


void HValue::AddDependantsToWorklist(HInferRepresentation* h_infer) {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    h_infer->AddToWorklist(it.value());
  }
  for (int i = 0; i < OperandCount(); ++i) {
    h_infer->AddToWorklist(OperandAt(i));
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
#ifdef DEBUG
  Verify();
#endif
}


void Range::Intersect(Range* other) {
  upper_ = Min(upper_, other->upper_);
  lower_ = Max(lower_, other->lower_);
  bool b = CanBeMinusZero() && other->CanBeMinusZero();
  set_can_be_minus_zero(b);
}


void Range::Union(Range* other) {
  upper_ = Max(upper_, other->upper_);
  lower_ = Min(lower_, other->lower_);
  bool b = CanBeMinusZero() || other->CanBeMinusZero();
  set_can_be_minus_zero(b);
}


void Range::CombinedMax(Range* other) {
  upper_ = Max(upper_, other->upper_);
  lower_ = Max(lower_, other->lower_);
  set_can_be_minus_zero(CanBeMinusZero() || other->CanBeMinusZero());
}


void Range::CombinedMin(Range* other) {
  upper_ = Min(upper_, other->upper_);
  lower_ = Min(lower_, other->lower_);
  set_can_be_minus_zero(CanBeMinusZero() || other->CanBeMinusZero());
}


void Range::Sar(int32_t value) {
  int32_t bits = value & 0x1F;
  lower_ = lower_ >> bits;
  upper_ = upper_ >> bits;
  set_can_be_minus_zero(false);
}


void Range::Shl(int32_t value) {
  int32_t bits = value & 0x1F;
  int old_lower = lower_;
  int old_upper = upper_;
  lower_ = lower_ << bits;
  upper_ = upper_ << bits;
  if (old_lower != lower_ >> bits || old_upper != upper_ >> bits) {
    upper_ = kMaxInt;
    lower_ = kMinInt;
  }
  set_can_be_minus_zero(false);
}


bool Range::AddAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  lower_ = AddWithoutOverflow(lower_, other->lower(), &may_overflow);
  upper_ = AddWithoutOverflow(upper_, other->upper(), &may_overflow);
  KeepOrder();
#ifdef DEBUG
  Verify();
#endif
  return may_overflow;
}


bool Range::SubAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  lower_ = SubWithoutOverflow(lower_, other->upper(), &may_overflow);
  upper_ = SubWithoutOverflow(upper_, other->lower(), &may_overflow);
  KeepOrder();
#ifdef DEBUG
  Verify();
#endif
  return may_overflow;
}


void Range::KeepOrder() {
  if (lower_ > upper_) {
    int32_t tmp = lower_;
    lower_ = upper_;
    upper_ = tmp;
  }
}


#ifdef DEBUG
void Range::Verify() const {
  ASSERT(lower_ <= upper_);
}
#endif


bool Range::MulAndCheckOverflow(Range* other) {
  bool may_overflow = false;
  int v1 = MulWithoutOverflow(lower_, other->lower(), &may_overflow);
  int v2 = MulWithoutOverflow(lower_, other->upper(), &may_overflow);
  int v3 = MulWithoutOverflow(upper_, other->lower(), &may_overflow);
  int v4 = MulWithoutOverflow(upper_, other->upper(), &may_overflow);
  lower_ = Min(Min(v1, v2), Min(v3, v4));
  upper_ = Max(Max(v1, v2), Max(v3, v4));
#ifdef DEBUG
  Verify();
#endif
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


bool HValue::IsDefinedAfter(HBasicBlock* other) const {
  return block()->block_id() > other->block_id();
}


HUseListNode* HUseListNode::tail() {
  // Skip and remove dead items in the use list.
  while (tail_ != NULL && tail_->value()->CheckFlag(HValue::kIsDead)) {
    tail_ = tail_->tail_;
  }
  return tail_;
}


bool HValue::CheckUsesForFlag(Flag f) {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    if (it.value()->IsSimulate()) continue;
    if (!it.value()->CheckFlag(f)) return false;
  }
  return true;
}


HUseIterator::HUseIterator(HUseListNode* head) : next_(head) {
  Advance();
}


void HUseIterator::Advance() {
  current_ = next_;
  if (current_ != NULL) {
    next_ = current_->tail();
    value_ = current_->value();
    index_ = current_->index();
  }
}


int HValue::UseCount() const {
  int count = 0;
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) ++count;
  return count;
}


HUseListNode* HValue::RemoveUse(HValue* value, int index) {
  HUseListNode* previous = NULL;
  HUseListNode* current = use_list_;
  while (current != NULL) {
    if (current->value() == value && current->index() == index) {
      if (previous == NULL) {
        use_list_ = current->tail();
      } else {
        previous->set_tail(current->tail());
      }
      break;
    }

    previous = current;
    current = current->tail();
  }

#ifdef DEBUG
  // Do not reuse use list nodes in debug mode, zap them.
  if (current != NULL) {
    HUseListNode* temp =
        new(block()->zone())
        HUseListNode(current->value(), current->index(), NULL);
    current->Zap();
    current = temp;
  }
#endif
  return current;
}


bool HValue::Equals(HValue* other) {
  if (other->opcode() != opcode()) return false;
  if (!other->representation().Equals(representation())) return false;
  if (!other->type_.Equals(type_)) return false;
  if (other->flags() != flags()) return false;
  if (OperandCount() != other->OperandCount()) return false;
  for (int i = 0; i < OperandCount(); ++i) {
    if (OperandAt(i)->id() != other->OperandAt(i)->id()) return false;
  }
  bool result = DataEquals(other);
  ASSERT(!result || Hashcode() == other->Hashcode());
  return result;
}


intptr_t HValue::Hashcode() {
  intptr_t result = opcode();
  int count = OperandCount();
  for (int i = 0; i < count; ++i) {
    result = result * 19 + OperandAt(i)->id() + (result >> 7);
  }
  return result;
}


const char* HValue::Mnemonic() const {
  switch (opcode()) {
#define MAKE_CASE(type) case k##type: return #type;
    HYDROGEN_CONCRETE_INSTRUCTION_LIST(MAKE_CASE)
#undef MAKE_CASE
    case kPhi: return "Phi";
    default: return "";
  }
}


void HValue::SetOperandAt(int index, HValue* value) {
  RegisterUse(index, value);
  InternalSetOperandAt(index, value);
}


void HValue::DeleteAndReplaceWith(HValue* other) {
  // We replace all uses first, so Delete can assert that there are none.
  if (other != NULL) ReplaceAllUsesWith(other);
  ASSERT(HasNoUses());
  Kill();
  DeleteFromGraph();
}


void HValue::ReplaceAllUsesWith(HValue* other) {
  while (use_list_ != NULL) {
    HUseListNode* list_node = use_list_;
    HValue* value = list_node->value();
    ASSERT(!value->block()->IsStartBlock());
    value->InternalSetOperandAt(list_node->index(), other);
    use_list_ = list_node->tail();
    list_node->set_tail(other->use_list_);
    other->use_list_ = list_node;
  }
}


void HValue::Kill() {
  // Instead of going through the entire use list of each operand, we only
  // check the first item in each use list and rely on the tail() method to
  // skip dead items, removing them lazily next time we traverse the list.
  SetFlag(kIsDead);
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* operand = OperandAt(i);
    if (operand == NULL) continue;
    HUseListNode* first = operand->use_list_;
    if (first != NULL && first->value() == this && first->index() == i) {
      operand->use_list_ = first->tail();
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


void HValue::PrintTypeTo(StringStream* stream) {
  if (!representation().IsTagged() || type().Equals(HType::Tagged())) return;
  stream->Add(" type[%s]", type().ToString());
}


void HValue::PrintRangeTo(StringStream* stream) {
  if (range() == NULL || range()->IsMostGeneric()) return;
  stream->Add(" range[%d,%d,m0=%d]",
              range()->lower(),
              range()->upper(),
              static_cast<int>(range()->CanBeMinusZero()));
}


void HValue::PrintChangesTo(StringStream* stream) {
  GVNFlagSet changes_flags = ChangesFlags();
  if (changes_flags.IsEmpty()) return;
  stream->Add(" changes[");
  if (changes_flags == AllSideEffectsFlagSet()) {
    stream->Add("*");
  } else {
    bool add_comma = false;
#define PRINT_DO(type)                            \
    if (changes_flags.Contains(kChanges##type)) { \
      if (add_comma) stream->Add(",");            \
      add_comma = true;                           \
      stream->Add(#type);                         \
    }
    GVN_TRACKED_FLAG_LIST(PRINT_DO);
    GVN_UNTRACKED_FLAG_LIST(PRINT_DO);
#undef PRINT_DO
  }
  stream->Add("]");
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

  HUseListNode* removed = NULL;
  if (old_value != NULL) {
    removed = old_value->RemoveUse(this, index);
  }

  if (new_value != NULL) {
    if (removed == NULL) {
      new_value->use_list_ = new(new_value->block()->zone()) HUseListNode(
          this, index, new_value->use_list_);
    } else {
      removed->set_tail(new_value->use_list_);
      new_value->use_list_ = removed;
    }
  }
}


void HValue::AddNewRange(Range* r, Zone* zone) {
  if (!HasRange()) ComputeInitialRange(zone);
  if (!HasRange()) range_ = new(zone) Range();
  ASSERT(HasRange());
  r->StackUpon(range_);
  range_ = r;
}


void HValue::RemoveLastAddedRange() {
  ASSERT(HasRange());
  ASSERT(range_->next() != NULL);
  range_ = range_->next();
}


void HValue::ComputeInitialRange(Zone* zone) {
  ASSERT(!HasRange());
  range_ = InferRange(zone);
  ASSERT(HasRange());
}


void HInstruction::PrintTo(StringStream* stream) {
  PrintMnemonicTo(stream);
  PrintDataTo(stream);
  PrintRangeTo(stream);
  PrintChangesTo(stream);
  PrintTypeTo(stream);
}


void HInstruction::PrintMnemonicTo(StringStream* stream) {
  stream->Add("%s ", Mnemonic());
}


void HInstruction::Unlink() {
  ASSERT(IsLinked());
  ASSERT(!IsControlInstruction());  // Must never move control instructions.
  ASSERT(!IsBlockEntry());  // Doesn't make sense to delete these.
  ASSERT(previous_ != NULL);
  previous_->next_ = next_;
  if (next_ == NULL) {
    ASSERT(block()->last() == this);
    block()->set_last(previous_);
  } else {
    next_->previous_ = previous_;
  }
  clear_block();
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
  if (previous->HasObservableSideEffects() && next != NULL) {
    ASSERT(next->IsSimulate());
    previous = next;
    next = previous->next_;
  }

  previous_ = previous;
  next_ = next;
  SetBlock(block);
  previous->next_ = this;
  if (next != NULL) next->previous_ = this;
  if (block->last() == previous) {
    block->set_last(this);
  }
}


#ifdef DEBUG
void HInstruction::Verify() {
  // Verify that input operands are defined before use.
  HBasicBlock* cur_block = block();
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* other_operand = OperandAt(i);
    if (other_operand == NULL) continue;
    HBasicBlock* other_block = other_operand->block();
    if (cur_block == other_block) {
      if (!other_operand->IsPhi()) {
        HInstruction* cur = this->previous();
        while (cur != NULL) {
          if (cur == other_operand) break;
          cur = cur->previous();
        }
        // Must reach other operand in the same block!
        ASSERT(cur == other_operand);
      }
    } else {
      // If the following assert fires, you may have forgotten an
      // AddInstruction.
      ASSERT(other_block->Dominates(cur_block));
    }
  }

  // Verify that instructions that may have side-effects are followed
  // by a simulate instruction.
  if (HasObservableSideEffects() && !IsOsrEntry()) {
    ASSERT(next()->IsSimulate());
  }

  // Verify that instructions that can be eliminated by GVN have overridden
  // HValue::DataEquals.  The default implementation is UNREACHABLE.  We
  // don't actually care whether DataEquals returns true or false here.
  if (CheckFlag(kUseGVN)) DataEquals(this);
}
#endif


void HUnaryCall::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


void HBinaryCall::PrintDataTo(StringStream* stream) {
  first()->PrintNameTo(stream);
  stream->Add(" ");
  second()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


void HBoundsCheck::PrintDataTo(StringStream* stream) {
  index()->PrintNameTo(stream);
  stream->Add(" ");
  length()->PrintNameTo(stream);
}


void HCallConstantFunction::PrintDataTo(StringStream* stream) {
  if (IsApplyFunction()) {
    stream->Add("optimized apply ");
  } else {
    stream->Add("%o ", function()->shared()->DebugName());
  }
  stream->Add("#%d", argument_count());
}


void HCallNamed::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", *name());
  HUnaryCall::PrintDataTo(stream);
}


void HCallGlobal::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", *name());
  HUnaryCall::PrintDataTo(stream);
}


void HCallKnownGlobal::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", target()->shared()->DebugName());
  stream->Add("#%d", argument_count());
}


void HCallRuntime::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", *name());
  stream->Add("#%d", argument_count());
}


void HClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("class_of_test(");
  value()->PrintNameTo(stream);
  stream->Add(", \"%o\")", *class_name());
}


void HAccessArgumentsAt::PrintDataTo(StringStream* stream) {
  arguments()->PrintNameTo(stream);
  stream->Add("[");
  index()->PrintNameTo(stream);
  stream->Add("], length ");
  length()->PrintNameTo(stream);
}


void HControlInstruction::PrintDataTo(StringStream* stream) {
  stream->Add(" goto (");
  bool first_block = true;
  for (HSuccessorIterator it(this); !it.Done(); it.Advance()) {
    stream->Add(first_block ? "B%d" : ", B%d", it.Current()->block_id());
    first_block = false;
  }
  stream->Add(")");
}


void HUnaryControlInstruction::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  HControlInstruction::PrintDataTo(stream);
}


void HIsNilAndBranch::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(kind() == kStrictEquality ? " === " : " == ");
  stream->Add(nil() == kNullValue ? "null" : "undefined");
  HControlInstruction::PrintDataTo(stream);
}


void HReturn::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
}


Representation HBranch::observed_input_representation(int index) {
  static const ToBooleanStub::Types tagged_types(
      ToBooleanStub::UNDEFINED |
      ToBooleanStub::NULL_TYPE |
      ToBooleanStub::SPEC_OBJECT |
      ToBooleanStub::STRING);
  if (expected_input_types_.ContainsAnyOf(tagged_types)) {
    return Representation::Tagged();
  } else if (expected_input_types_.Contains(ToBooleanStub::HEAP_NUMBER)) {
    return Representation::Double();
  } else if (expected_input_types_.Contains(ToBooleanStub::SMI)) {
    return Representation::Integer32();
  } else {
    return Representation::None();
  }
}


void HCompareMap::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" (%p)", *map());
  HControlInstruction::PrintDataTo(stream);
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


void HUnaryMathOperation::PrintDataTo(StringStream* stream) {
  const char* name = OpName();
  stream->Add("%s ", name);
  value()->PrintNameTo(stream);
}


void HUnaryOperation::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
}


void HHasInstanceTypeAndBranch::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  switch (from_) {
    case FIRST_JS_RECEIVER_TYPE:
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


void HTypeofIsAndBranch::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" == %o", *type_literal_);
  HControlInstruction::PrintDataTo(stream);
}


void HCheckMapValue::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  map()->PrintNameTo(stream);
}


void HForInPrepareMap::PrintDataTo(StringStream* stream) {
  enumerable()->PrintNameTo(stream);
}


void HForInCacheArray::PrintDataTo(StringStream* stream) {
  enumerable()->PrintNameTo(stream);
  stream->Add(" ");
  map()->PrintNameTo(stream);
  stream->Add("[%d]", idx_);
}


void HLoadFieldByIndex::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(" ");
  index()->PrintNameTo(stream);
}


HValue* HBitwise::Canonicalize() {
  if (!representation().IsInteger32()) return this;
  // If x is an int32, then x & -1 == x, x | 0 == x and x ^ 0 == x.
  int32_t nop_constant = (op() == Token::BIT_AND) ? -1 : 0;
  if (left()->IsConstant() &&
      HConstant::cast(left())->HasInteger32Value() &&
      HConstant::cast(left())->Integer32Value() == nop_constant &&
      !right()->CheckFlag(kUint32)) {
    return right();
  }
  if (right()->IsConstant() &&
      HConstant::cast(right())->HasInteger32Value() &&
      HConstant::cast(right())->Integer32Value() == nop_constant &&
      !left()->CheckFlag(kUint32)) {
    return left();
  }
  return this;
}


HValue* HBitNot::Canonicalize() {
  // Optimize ~~x, a common pattern used for ToInt32(x).
  if (value()->IsBitNot()) {
    HValue* result = HBitNot::cast(value())->value();
    ASSERT(result->representation().IsInteger32());
    if (!result->CheckFlag(kUint32)) {
      return result;
    }
  }
  return this;
}


HValue* HAdd::Canonicalize() {
  if (!representation().IsInteger32()) return this;
  if (CheckUsesForFlag(kTruncatingToInt32)) ClearFlag(kCanOverflow);
  return this;
}


HValue* HSub::Canonicalize() {
  if (!representation().IsInteger32()) return this;
  if (CheckUsesForFlag(kTruncatingToInt32)) ClearFlag(kCanOverflow);
  return this;
}


HValue* HChange::Canonicalize() {
  return (from().Equals(to())) ? value() : this;
}


HValue* HWrapReceiver::Canonicalize() {
  if (HasNoUses()) return NULL;
  if (receiver()->type().IsJSObject()) {
    return receiver();
  }
  return this;
}


void HTypeof::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
}


void HChange::PrintDataTo(StringStream* stream) {
  HUnaryOperation::PrintDataTo(stream);
  stream->Add(" %s to %s", from().Mnemonic(), to().Mnemonic());

  if (CanTruncateToInt32()) stream->Add(" truncating-int32");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
  if (CheckFlag(kDeoptimizeOnUndefined)) stream->Add(" deopt-on-undefined");
}


void HJSArrayLength::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  typecheck()->PrintNameTo(stream);
}


HValue* HUnaryMathOperation::Canonicalize() {
  if (op() == kMathFloor) {
    // If the input is integer32 then we replace the floor instruction
    // with its input. This happens before the representation changes are
    // introduced.
    if (value()->representation().IsInteger32()) return value();

#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_IA32) || \
        defined(V8_TARGET_ARCH_X64)
    if (value()->IsDiv() && (value()->UseCount() == 1)) {
      // TODO(2038): Implement this optimization for non ARM architectures.
      HDiv* hdiv = HDiv::cast(value());
      HValue* left = hdiv->left();
      HValue* right = hdiv->right();
      // Try to simplify left and right values of the division.
      HValue* new_left =
        LChunkBuilder::SimplifiedDividendForMathFloorOfDiv(left);
      HValue* new_right =
        LChunkBuilder::SimplifiedDivisorForMathFloorOfDiv(right);

      // Return if left or right are not optimizable.
      if ((new_left == NULL) || (new_right == NULL)) return this;

      // Insert the new values in the graph.
      if (new_left->IsInstruction() &&
          !HInstruction::cast(new_left)->IsLinked()) {
        HInstruction::cast(new_left)->InsertBefore(this);
      }
      if (new_right->IsInstruction() &&
          !HInstruction::cast(new_right)->IsLinked()) {
        HInstruction::cast(new_right)->InsertBefore(this);
      }
      HMathFloorOfDiv* instr =  new(block()->zone()) HMathFloorOfDiv(context(),
          new_left,
          new_right);
      // Replace this HMathFloor instruction by the new HMathFloorOfDiv.
      instr->InsertBefore(this);
      ReplaceAllUsesWith(instr);
      Kill();
      // We know the division had no other uses than this HMathFloor. Delete it.
      // Also delete the arguments of the division if they are not used any
      // more.
      hdiv->DeleteAndReplaceWith(NULL);
      ASSERT(left->IsChange() || left->IsConstant());
      ASSERT(right->IsChange() || right->IsConstant());
      if (left->HasNoUses())  left->DeleteAndReplaceWith(NULL);
      if (right->HasNoUses())  right->DeleteAndReplaceWith(NULL);

      // Return NULL to remove this instruction from the graph.
      return NULL;
    }
#endif  // V8_TARGET_ARCH_ARM
  }
  return this;
}


HValue* HCheckInstanceType::Canonicalize() {
  if (check_ == IS_STRING &&
      !value()->type().IsUninitialized() &&
      value()->type().IsString()) {
    return NULL;
  }
  if (check_ == IS_SYMBOL &&
      value()->IsConstant() &&
      HConstant::cast(value())->handle()->IsSymbol()) {
    return NULL;
  }
  return this;
}


void HCheckInstanceType::GetCheckInterval(InstanceType* first,
                                          InstanceType* last) {
  ASSERT(is_interval_check());
  switch (check_) {
    case IS_SPEC_OBJECT:
      *first = FIRST_SPEC_OBJECT_TYPE;
      *last = LAST_SPEC_OBJECT_TYPE;
      return;
    case IS_JS_ARRAY:
      *first = *last = JS_ARRAY_TYPE;
      return;
    default:
      UNREACHABLE();
  }
}


void HCheckInstanceType::GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag) {
  ASSERT(!is_interval_check());
  switch (check_) {
    case IS_STRING:
      *mask = kIsNotStringMask;
      *tag = kStringTag;
      return;
    case IS_SYMBOL:
      *mask = kIsSymbolMask;
      *tag = kSymbolTag;
      return;
    default:
      UNREACHABLE();
  }
}


void HLoadElements::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  typecheck()->PrintNameTo(stream);
}


void HCheckMaps::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" [%p", *map_set()->first());
  for (int i = 1; i < map_set()->length(); ++i) {
    stream->Add(",%p", *map_set()->at(i));
  }
  stream->Add("]");
}


void HCheckFunction::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" %p", *target());
}


const char* HCheckInstanceType::GetCheckName() {
  switch (check_) {
    case IS_SPEC_OBJECT: return "object";
    case IS_JS_ARRAY: return "array";
    case IS_STRING: return "string";
    case IS_SYMBOL: return "symbol";
  }
  UNREACHABLE();
  return "";
}

void HCheckInstanceType::PrintDataTo(StringStream* stream) {
  stream->Add("%s ", GetCheckName());
  HUnaryOperation::PrintDataTo(stream);
}


void HCheckPrototypeMaps::PrintDataTo(StringStream* stream) {
  stream->Add("[receiver_prototype=%p,holder=%p]", *prototype(), *holder());
}


void HCallStub::PrintDataTo(StringStream* stream) {
  stream->Add("%s ",
              CodeStub::MajorName(major_key_, false));
  HUnaryCall::PrintDataTo(stream);
}


void HInstanceOf::PrintDataTo(StringStream* stream) {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  stream->Add(" ");
  context()->PrintNameTo(stream);
}


Range* HValue::InferRange(Zone* zone) {
  // Untagged integer32 cannot be -0, all other representations can.
  Range* result = new(zone) Range();
  result->set_can_be_minus_zero(!representation().IsInteger32());
  return result;
}


Range* HChange::InferRange(Zone* zone) {
  Range* input_range = value()->range();
  if (from().IsInteger32() &&
      to().IsTagged() &&
      !value()->CheckFlag(HInstruction::kUint32) &&
      input_range != NULL && input_range->IsInSmiRange()) {
    set_type(HType::Smi());
  }
  Range* result = (input_range != NULL)
      ? input_range->Copy(zone)
      : HValue::InferRange(zone);
  if (to().IsInteger32()) result->set_can_be_minus_zero(false);
  return result;
}


Range* HConstant::InferRange(Zone* zone) {
  if (has_int32_value_) {
    Range* result = new(zone) Range(int32_value_, int32_value_);
    result->set_can_be_minus_zero(false);
    return result;
  }
  return HValue::InferRange(zone);
}


Range* HPhi::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    if (block()->IsLoopHeader()) {
      Range* range = new(zone) Range(kMinInt, kMaxInt);
      return range;
    } else {
      Range* range = OperandAt(0)->range()->Copy(zone);
      for (int i = 1; i < OperandCount(); ++i) {
        range->Union(OperandAt(i)->range());
      }
      return range;
    }
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HAdd::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->AddAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    bool m0 = a->CanBeMinusZero() && b->CanBeMinusZero();
    res->set_can_be_minus_zero(m0);
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HSub::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->SubAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    res->set_can_be_minus_zero(a->CanBeMinusZero() && b->CanBeZero());
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HMul::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->MulAndCheckOverflow(b)) {
      ClearFlag(kCanOverflow);
    }
    bool m0 = (a->CanBeZero() && b->CanBeNegative()) ||
        (a->CanBeNegative() && b->CanBeZero());
    res->set_can_be_minus_zero(m0);
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HDiv::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* result = new(zone) Range();
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
    return HValue::InferRange(zone);
  }
}


Range* HMod::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* result = new(zone) Range();
    if (a->CanBeMinusZero() || a->CanBeNegative()) {
      result->set_can_be_minus_zero(true);
    }
    if (!right()->range()->CanBeZero()) {
      ClearFlag(HValue::kCanBeDivByZero);
    }
    return result;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HMathMinMax::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (operation_ == kMathMax) {
      res->CombinedMax(b);
    } else {
      ASSERT(operation_ == kMathMin);
      res->CombinedMin(b);
    }
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


void HPhi::PrintTo(StringStream* stream) {
  stream->Add("[");
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* value = OperandAt(i);
    stream->Add(" ");
    value->PrintNameTo(stream);
    stream->Add(" ");
  }
  stream->Add(" uses%d_%di_%dd_%dt",
              UseCount(),
              int32_non_phi_uses() + int32_indirect_uses(),
              double_non_phi_uses() + double_indirect_uses(),
              tagged_non_phi_uses() + tagged_indirect_uses());
  stream->Add("%s%s]",
              is_live() ? "_live" : "",
              IsConvertibleToInteger() ? "" : "_ncti");
}


void HPhi::AddInput(HValue* value) {
  inputs_.Add(NULL, value->block()->zone());
  SetOperandAt(OperandCount() - 1, value);
  // Mark phis that may have 'arguments' directly or indirectly as an operand.
  if (!CheckFlag(kIsArguments) && value->CheckFlag(kIsArguments)) {
    SetFlag(kIsArguments);
  }
}


bool HPhi::HasRealUses() {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    if (!it.value()->IsPhi()) return true;
  }
  return false;
}


HValue* HPhi::GetRedundantReplacement() {
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
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* value = it.value();
    if (!value->IsPhi()) {
      Representation rep = value->observed_input_representation(it.index());
      non_phi_uses_[rep.kind()] += value->LoopWeight();
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is used by real #%d %s as %s\n",
               id(), value->id(), value->Mnemonic(), rep.Mnemonic());
      }
    }
  }
}


void HPhi::AddNonPhiUsesFrom(HPhi* other) {
  if (FLAG_trace_representation) {
    PrintF("adding to #%d Phi uses of #%d Phi: i%d d%d t%d\n",
           id(), other->id(),
           other->non_phi_uses_[Representation::kInteger32],
           other->non_phi_uses_[Representation::kDouble],
           other->non_phi_uses_[Representation::kTagged]);
  }

  for (int i = 0; i < Representation::kNumRepresentations; i++) {
    indirect_uses_[i] += other->non_phi_uses_[i];
  }
}


void HPhi::AddIndirectUsesTo(int* dest) {
  for (int i = 0; i < Representation::kNumRepresentations; i++) {
    dest[i] += indirect_uses_[i];
  }
}


void HSimulate::MergeInto(HSimulate* other) {
  for (int i = 0; i < values_.length(); ++i) {
    HValue* value = values_[i];
    if (HasAssignedIndexAt(i)) {
      other->AddAssignedValue(GetAssignedIndexAt(i), value);
    } else {
      if (other->pop_count_ > 0) {
        other->pop_count_--;
      } else {
        other->AddPushedValue(value);
      }
    }
  }
  other->pop_count_ += pop_count();
}


void HSimulate::PrintDataTo(StringStream* stream) {
  stream->Add("id=%d", ast_id().ToInt());
  if (pop_count_ > 0) stream->Add(" pop %d", pop_count_);
  if (values_.length() > 0) {
    if (pop_count_ > 0) stream->Add(" /");
    for (int i = values_.length() - 1; i >= 0; --i) {
      if (i > 0) stream->Add(",");
      if (HasAssignedIndexAt(i)) {
        stream->Add(" var[%d] = ", GetAssignedIndexAt(i));
      } else {
        stream->Add(" push ");
      }
      values_[i]->PrintNameTo(stream);
    }
  }
}


void HDeoptimize::PrintDataTo(StringStream* stream) {
  if (OperandCount() == 0) return;
  OperandAt(0)->PrintNameTo(stream);
  for (int i = 1; i < OperandCount(); ++i) {
    stream->Add(" ");
    OperandAt(i)->PrintNameTo(stream);
  }
}


void HEnterInlined::PrintDataTo(StringStream* stream) {
  SmartArrayPointer<char> name = function()->debug_name()->ToCString();
  stream->Add("%s, id=%d", *name, function()->id().ToInt());
}


static bool IsInteger32(double value) {
  double roundtrip_value = static_cast<double>(static_cast<int32_t>(value));
  return BitCast<int64_t>(roundtrip_value) == BitCast<int64_t>(value);
}


HConstant::HConstant(Handle<Object> handle, Representation r)
    : handle_(handle),
      has_int32_value_(false),
      has_double_value_(false) {
  SetFlag(kUseGVN);
  if (handle_->IsNumber()) {
    double n = handle_->Number();
    has_int32_value_ = IsInteger32(n);
    int32_value_ = DoubleToInt32(n);
    double_value_ = n;
    has_double_value_ = true;
  }
  if (r.IsNone()) {
    if (has_int32_value_) {
      r = Representation::Integer32();
    } else if (has_double_value_) {
      r = Representation::Double();
    } else {
      r = Representation::Tagged();
    }
  }
  set_representation(r);
}


HConstant::HConstant(int32_t integer_value, Representation r)
    : has_int32_value_(true),
      has_double_value_(true),
      int32_value_(integer_value),
      double_value_(FastI2D(integer_value)) {
  set_representation(r);
  SetFlag(kUseGVN);
}


HConstant::HConstant(double double_value, Representation r)
    : has_int32_value_(IsInteger32(double_value)),
      has_double_value_(true),
      int32_value_(DoubleToInt32(double_value)),
      double_value_(double_value) {
  set_representation(r);
  SetFlag(kUseGVN);
}


HConstant* HConstant::CopyToRepresentation(Representation r, Zone* zone) const {
  if (r.IsInteger32() && !has_int32_value_) return NULL;
  if (r.IsDouble() && !has_double_value_) return NULL;
  if (handle_.is_null()) {
    ASSERT(has_int32_value_ || has_double_value_);
    if (has_int32_value_) return new(zone) HConstant(int32_value_, r);
    return new(zone) HConstant(double_value_, r);
  }
  return new(zone) HConstant(handle_, r);
}


HConstant* HConstant::CopyToTruncatedInt32(Zone* zone) const {
  if (has_int32_value_) {
    if (handle_.is_null()) {
      return new(zone) HConstant(int32_value_, Representation::Integer32());
    } else {
      // Re-use the existing Handle if possible.
      return new(zone) HConstant(handle_, Representation::Integer32());
    }
  } else if (has_double_value_) {
    return new(zone) HConstant(DoubleToInt32(double_value_),
                               Representation::Integer32());
  } else {
    return NULL;
  }
}


bool HConstant::ToBoolean() {
  // Converts the constant's boolean value according to
  // ECMAScript section 9.2 ToBoolean conversion.
  if (HasInteger32Value()) return Integer32Value() != 0;
  if (HasDoubleValue()) {
    double v = DoubleValue();
    return v != 0 && !isnan(v);
  }
  Handle<Object> literal = handle();
  if (literal->IsTrue()) return true;
  if (literal->IsFalse()) return false;
  if (literal->IsUndefined()) return false;
  if (literal->IsNull()) return false;
  if (literal->IsString() && String::cast(*literal)->length() == 0) {
    return false;
  }
  return true;
}

void HConstant::PrintDataTo(StringStream* stream) {
  if (has_int32_value_) {
    stream->Add("%d ", int32_value_);
  } else if (has_double_value_) {
    stream->Add("%f ", FmtElm(double_value_));
  } else {
    handle()->ShortPrint(stream);
  }
}


bool HArrayLiteral::IsCopyOnWrite() const {
  if (!boilerplate_object_->IsJSObject()) return false;
  return Handle<JSObject>::cast(boilerplate_object_)->elements()->map() ==
      HEAP->fixed_cow_array_map();
}


void HBinaryOperation::PrintDataTo(StringStream* stream) {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  if (CheckFlag(kCanOverflow)) stream->Add(" !");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
}


void HBinaryOperation::InferRepresentation(HInferRepresentation* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  // When the operation has information about its own output type, don't look
  // at uses.
  if (!observed_output_representation_.IsNone()) return;
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
}


Representation HBinaryOperation::RepresentationFromInputs() {
  // Determine the worst case of observed input representations and
  // the currently assumed output representation.
  Representation rep = representation();
  if (observed_output_representation_.is_more_general_than(rep)) {
    rep = observed_output_representation_;
  }
  for (int i = 1; i <= 2; ++i) {
    Representation input_rep = observed_input_representation(i);
    if (input_rep.is_more_general_than(rep)) rep = input_rep;
  }
  // If any of the actual input representation is more general than what we
  // have so far but not Tagged, use that representation instead.
  Representation left_rep = left()->representation();
  Representation right_rep = right()->representation();

  if (left_rep.is_more_general_than(rep) &&
      left()->CheckFlag(kFlexibleRepresentation)) {
    rep = left_rep;
  }
  if (right_rep.is_more_general_than(rep) &&
      right()->CheckFlag(kFlexibleRepresentation)) {
    rep = right_rep;
  }
  return rep;
}


void HBinaryOperation::AssumeRepresentation(Representation r) {
  set_observed_input_representation(r, r);
  HValue::AssumeRepresentation(r);
}


void HMathMinMax::InferRepresentation(HInferRepresentation* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  // Do not care about uses.
}


Range* HBitwise::InferRange(Zone* zone) {
  if (op() == Token::BIT_XOR) return HValue::InferRange(zone);
  const int32_t kDefaultMask = static_cast<int32_t>(0xffffffff);
  int32_t left_mask = (left()->range() != NULL)
      ? left()->range()->Mask()
      : kDefaultMask;
  int32_t right_mask = (right()->range() != NULL)
      ? right()->range()->Mask()
      : kDefaultMask;
  int32_t result_mask = (op() == Token::BIT_AND)
      ? left_mask & right_mask
      : left_mask | right_mask;
  return (result_mask >= 0)
      ? new(zone) Range(0, result_mask)
      : HValue::InferRange(zone);
}


Range* HSar::InferRange(Zone* zone) {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      Range* result = (left()->range() != NULL)
          ? left()->range()->Copy(zone)
          : new(zone) Range();
      result->Sar(c->Integer32Value());
      result->set_can_be_minus_zero(false);
      return result;
    }
  }
  return HValue::InferRange(zone);
}


Range* HShr::InferRange(Zone* zone) {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      int shift_count = c->Integer32Value() & 0x1f;
      if (left()->range()->CanBeNegative()) {
        // Only compute bounds if the result always fits into an int32.
        return (shift_count >= 1)
            ? new(zone) Range(0,
                              static_cast<uint32_t>(0xffffffff) >> shift_count)
            : new(zone) Range();
      } else {
        // For positive inputs we can use the >> operator.
        Range* result = (left()->range() != NULL)
            ? left()->range()->Copy(zone)
            : new(zone) Range();
        result->Sar(c->Integer32Value());
        result->set_can_be_minus_zero(false);
        return result;
      }
    }
  }
  return HValue::InferRange(zone);
}


Range* HShl::InferRange(Zone* zone) {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      Range* result = (left()->range() != NULL)
          ? left()->range()->Copy(zone)
          : new(zone) Range();
      result->Shl(c->Integer32Value());
      result->set_can_be_minus_zero(false);
      return result;
    }
  }
  return HValue::InferRange(zone);
}


Range* HLoadKeyed::InferRange(Zone* zone) {
  switch (elements_kind()) {
    case EXTERNAL_PIXEL_ELEMENTS:
      return new(zone) Range(0, 255);
    case EXTERNAL_BYTE_ELEMENTS:
      return new(zone) Range(-128, 127);
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      return new(zone) Range(0, 255);
    case EXTERNAL_SHORT_ELEMENTS:
      return new(zone) Range(-32768, 32767);
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      return new(zone) Range(0, 65535);
    default:
      return HValue::InferRange(zone);
  }
}


void HCompareGeneric::PrintDataTo(StringStream* stream) {
  stream->Add(Token::Name(token()));
  stream->Add(" ");
  HBinaryOperation::PrintDataTo(stream);
}


void HStringCompareAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add(Token::Name(token()));
  stream->Add(" ");
  HControlInstruction::PrintDataTo(stream);
}


void HCompareIDAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add(Token::Name(token()));
  stream->Add(" ");
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  HControlInstruction::PrintDataTo(stream);
}


void HCompareObjectEqAndBranch::PrintDataTo(StringStream* stream) {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  HControlInstruction::PrintDataTo(stream);
}


void HGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", SuccessorAt(0)->block_id());
}


void HCompareIDAndBranch::InferRepresentation(HInferRepresentation* h_infer) {
  Representation rep = Representation::None();
  Representation left_rep = left()->representation();
  Representation right_rep = right()->representation();
  bool observed_integers =
      observed_input_representation(0).IsInteger32() &&
      observed_input_representation(1).IsInteger32();
  bool inputs_are_not_doubles =
      !left_rep.IsDouble() && !right_rep.IsDouble();
  if (observed_integers && inputs_are_not_doubles) {
    rep = Representation::Integer32();
  } else {
    rep = Representation::Double();
    // According to the ES5 spec (11.9.3, 11.8.5), Equality comparisons (==, ===
    // and !=) have special handling of undefined, e.g. undefined == undefined
    // is 'true'. Relational comparisons have a different semantic, first
    // calling ToPrimitive() on their arguments.  The standard Crankshaft
    // tagged-to-double conversion to ensure the HCompareIDAndBranch's inputs
    // are doubles caused 'undefined' to be converted to NaN. That's compatible
    // out-of-the box with ordered relational comparisons (<, >, <=,
    // >=). However, for equality comparisons (and for 'in' and 'instanceof'),
    // it is not consistent with the spec. For example, it would cause undefined
    // == undefined (should be true) to be evaluated as NaN == NaN
    // (false). Therefore, any comparisons other than ordered relational
    // comparisons must cause a deopt when one of their arguments is undefined.
    // See also v8:1434
    if (!Token::IsOrderedRelationalCompareOp(token_)) {
      SetFlag(kDeoptimizeOnUndefined);
    }
  }
  ChangeRepresentation(rep);
}


void HParameter::PrintDataTo(StringStream* stream) {
  stream->Add("%u", index());
}


void HLoadNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(" @%d%s", offset(), is_in_object() ? "[in-object]" : "");
}


// Returns true if an instance of this map can never find a property with this
// name in its prototype chain.  This means all prototypes up to the top are
// fast and don't have the name in them.  It would be good if we could optimize
// polymorphic loads where the property is sometimes found in the prototype
// chain.
static bool PrototypeChainCanNeverResolve(
    Handle<Map> map, Handle<String> name) {
  Isolate* isolate = map->GetIsolate();
  Object* current = map->prototype();
  while (current != isolate->heap()->null_value()) {
    if (current->IsJSGlobalProxy() ||
        current->IsGlobalObject() ||
        !current->IsJSObject() ||
        JSObject::cast(current)->map()->has_named_interceptor() ||
        JSObject::cast(current)->IsAccessCheckNeeded() ||
        !JSObject::cast(current)->HasFastProperties()) {
      return false;
    }

    LookupResult lookup(isolate);
    Map* map = JSObject::cast(current)->map();
    map->LookupDescriptor(NULL, *name, &lookup);
    if (lookup.IsFound()) return false;
    if (!lookup.IsCacheable()) return false;
    current = JSObject::cast(current)->GetPrototype();
  }
  return true;
}


HLoadNamedFieldPolymorphic::HLoadNamedFieldPolymorphic(HValue* context,
                                                       HValue* object,
                                                       SmallMapList* types,
                                                       Handle<String> name,
                                                       Zone* zone)
    : types_(Min(types->length(), kMaxLoadPolymorphism), zone),
      name_(name),
      need_generic_(false) {
  SetOperandAt(0, context);
  SetOperandAt(1, object);
  set_representation(Representation::Tagged());
  SetGVNFlag(kDependsOnMaps);
  SmallMapList negative_lookups;
  for (int i = 0;
       i < types->length() && types_.length() < kMaxLoadPolymorphism;
       ++i) {
    Handle<Map> map = types->at(i);
    LookupResult lookup(map->GetIsolate());
    map->LookupDescriptor(NULL, *name, &lookup);
    if (lookup.IsFound()) {
      switch (lookup.type()) {
        case FIELD: {
          int index = lookup.GetLocalFieldIndexFromMap(*map);
          if (index < 0) {
            SetGVNFlag(kDependsOnInobjectFields);
          } else {
            SetGVNFlag(kDependsOnBackingStoreFields);
          }
          types_.Add(types->at(i), zone);
          break;
        }
        case CONSTANT_FUNCTION:
          types_.Add(types->at(i), zone);
          break;
        case CALLBACKS:
          break;
        case TRANSITION:
        case INTERCEPTOR:
        case NONEXISTENT:
        case NORMAL:
        case HANDLER:
          UNREACHABLE();
          break;
      }
    } else if (lookup.IsCacheable() &&
               // For dicts the lookup on the map will fail, but the object may
               // contain the property so we cannot generate a negative lookup
               // (which would just be a map check and return undefined).
               !map->is_dictionary_map() &&
               !map->has_named_interceptor() &&
               PrototypeChainCanNeverResolve(map, name)) {
      negative_lookups.Add(types->at(i), zone);
    }
  }

  bool need_generic =
      (types->length() != negative_lookups.length() + types_.length());
  if (!need_generic && FLAG_deoptimize_uncommon_cases) {
    SetFlag(kUseGVN);
    for (int i = 0; i < negative_lookups.length(); i++) {
      types_.Add(negative_lookups.at(i), zone);
    }
  } else {
    // We don't have an easy way to handle both a call (to the generic stub) and
    // a deopt in the same hydrogen instruction, so in this case we don't add
    // the negative lookups which can deopt - just let the generic stub handle
    // them.
    SetAllSideEffects();
    need_generic_ = true;
  }
}


bool HLoadNamedFieldPolymorphic::DataEquals(HValue* value) {
  HLoadNamedFieldPolymorphic* other = HLoadNamedFieldPolymorphic::cast(value);
  if (types_.length() != other->types()->length()) return false;
  if (!name_.is_identical_to(other->name())) return false;
  if (need_generic_ != other->need_generic_) return false;
  for (int i = 0; i < types_.length(); i++) {
    bool found = false;
    for (int j = 0; j < types_.length(); j++) {
      if (types_.at(j).is_identical_to(other->types()->at(i))) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}


void HLoadNamedFieldPolymorphic::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
}


void HLoadNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
}


void HLoadKeyed::PrintDataTo(StringStream* stream) {
  if (!is_external()) {
    elements()->PrintNameTo(stream);
  } else {
    ASSERT(elements_kind() >= FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND &&
           elements_kind() <= LAST_EXTERNAL_ARRAY_ELEMENTS_KIND);
    elements()->PrintNameTo(stream);
    stream->Add(".");
    stream->Add(ElementsKindToString(elements_kind()));
  }

  stream->Add("[");
  key()->PrintNameTo(stream);
  if (IsDehoisted()) {
    stream->Add(" + %d] ", index_offset());
  } else {
    stream->Add("] ");
  }

  dependency()->PrintNameTo(stream);
  if (RequiresHoleCheck()) {
    stream->Add(" check_hole");
  }
}


bool HLoadKeyed::RequiresHoleCheck() const {
  if (IsFastPackedElementsKind(elements_kind())) {
    return false;
  }

  if (IsFastDoubleElementsKind(elements_kind())) {
    return true;
  }

  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (!use->IsChange()) {
      return true;
    }
  }

  return false;
}


void HLoadKeyedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add("[");
  key()->PrintNameTo(stream);
  stream->Add("]");
}


HValue* HLoadKeyedGeneric::Canonicalize() {
  // Recognize generic keyed loads that use property name generated
  // by for-in statement as a key and rewrite them into fast property load
  // by index.
  if (key()->IsLoadKeyed()) {
    HLoadKeyed* key_load = HLoadKeyed::cast(key());
    if (key_load->elements()->IsForInCacheArray()) {
      HForInCacheArray* names_cache =
          HForInCacheArray::cast(key_load->elements());

      if (names_cache->enumerable() == object()) {
        HForInCacheArray* index_cache =
            names_cache->index_cache();
        HCheckMapValue* map_check =
            new(block()->zone()) HCheckMapValue(object(), names_cache->map());
        HInstruction* index = new(block()->zone()) HLoadKeyed(
            index_cache,
            key_load->key(),
            key_load->key(),
            key_load->elements_kind());
        map_check->InsertBefore(this);
        index->InsertBefore(this);
        HLoadFieldByIndex* load = new(block()->zone()) HLoadFieldByIndex(
            object(), index);
        load->InsertBefore(this);
        return load;
      }
    }
  }

  return this;
}


void HStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  ASSERT(name()->IsString());
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" = ");
  value()->PrintNameTo(stream);
}


void HStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  stream->Add(*String::cast(*name())->ToCString());
  stream->Add(" = ");
  value()->PrintNameTo(stream);
  stream->Add(" @%d%s", offset(), is_in_object() ? "[in-object]" : "");
  if (NeedsWriteBarrier()) {
    stream->Add(" (write-barrier)");
  }
  if (!transition().is_null()) {
    stream->Add(" (transition map %p)", *transition());
  }
}


void HStoreKeyed::PrintDataTo(StringStream* stream) {
  if (!is_external()) {
    elements()->PrintNameTo(stream);
  } else {
    elements()->PrintNameTo(stream);
    stream->Add(".");
    stream->Add(ElementsKindToString(elements_kind()));
    ASSERT(elements_kind() >= FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND &&
           elements_kind() <= LAST_EXTERNAL_ARRAY_ELEMENTS_KIND);
  }

  stream->Add("[");
  key()->PrintNameTo(stream);
  if (IsDehoisted()) {
    stream->Add(" + %d] = ", index_offset());
  } else {
    stream->Add("] = ");
  }

  value()->PrintNameTo(stream);
}


void HStoreKeyedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add("[");
  key()->PrintNameTo(stream);
  stream->Add("] = ");
  value()->PrintNameTo(stream);
}


void HTransitionElementsKind::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  ElementsKind from_kind = original_map()->elements_kind();
  ElementsKind to_kind = transitioned_map()->elements_kind();
  stream->Add(" %p [%s] -> %p [%s]",
              *original_map(),
              ElementsAccessor::ForKind(from_kind)->name(),
              *transitioned_map(),
              ElementsAccessor::ForKind(to_kind)->name());
}


void HLoadGlobalCell::PrintDataTo(StringStream* stream) {
  stream->Add("[%p]", *cell());
  if (!details_.IsDontDelete()) stream->Add(" (deleteable)");
  if (details_.IsReadOnly()) stream->Add(" (read-only)");
}


bool HLoadGlobalCell::RequiresHoleCheck() const {
  if (details_.IsDontDelete() && !details_.IsReadOnly()) return false;
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (!use->IsChange()) return true;
  }
  return false;
}


void HLoadGlobalGeneric::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", *name());
}


void HStoreGlobalCell::PrintDataTo(StringStream* stream) {
  stream->Add("[%p] = ", *cell());
  value()->PrintNameTo(stream);
  if (!details_.IsDontDelete()) stream->Add(" (deleteable)");
  if (details_.IsReadOnly()) stream->Add(" (read-only)");
}


void HStoreGlobalGeneric::PrintDataTo(StringStream* stream) {
  stream->Add("%o = ", *name());
  value()->PrintNameTo(stream);
}


void HLoadContextSlot::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add("[%d]", slot_index());
}


void HStoreContextSlot::PrintDataTo(StringStream* stream) {
  context()->PrintNameTo(stream);
  stream->Add("[%d] = ", slot_index());
  value()->PrintNameTo(stream);
}


// Implementation of type inference and type conversions. Calculates
// the inferred type of this instruction based on the input operands.

HType HValue::CalculateInferredType() {
  return type_;
}


HType HCheckMaps::CalculateInferredType() {
  return value()->type();
}


HType HCheckFunction::CalculateInferredType() {
  return value()->type();
}


HType HCheckNonSmi::CalculateInferredType() {
  // TODO(kasperl): Is there any way to signal that this isn't a smi?
  return HType::Tagged();
}


HType HCheckSmi::CalculateInferredType() {
  return HType::Smi();
}


HType HPhi::CalculateInferredType() {
  HType result = HType::Uninitialized();
  for (int i = 0; i < OperandCount(); ++i) {
    HType current = OperandAt(i)->type();
    result = result.Combine(current);
  }
  return result;
}


HType HConstant::CalculateInferredType() {
  if (has_int32_value_) {
    return Smi::IsValid(int32_value_) ? HType::Smi() : HType::HeapNumber();
  }
  if (has_double_value_) return HType::HeapNumber();
  return HType::TypeFromValue(handle_);
}


HType HCompareGeneric::CalculateInferredType() {
  return HType::Boolean();
}


HType HInstanceOf::CalculateInferredType() {
  return HType::Boolean();
}


HType HDeleteProperty::CalculateInferredType() {
  return HType::Boolean();
}


HType HInstanceOfKnownGlobal::CalculateInferredType() {
  return HType::Boolean();
}


HType HChange::CalculateInferredType() {
  if (from().IsDouble() && to().IsTagged()) return HType::HeapNumber();
  return type();
}


HType HBitwiseBinaryOperation::CalculateInferredType() {
  return HType::TaggedNumber();
}


HType HArithmeticBinaryOperation::CalculateInferredType() {
  return HType::TaggedNumber();
}


HType HAdd::CalculateInferredType() {
  return HType::Tagged();
}


HType HBitNot::CalculateInferredType() {
  return HType::TaggedNumber();
}


HType HUnaryMathOperation::CalculateInferredType() {
  return HType::TaggedNumber();
}


HType HStringCharFromCode::CalculateInferredType() {
  return HType::String();
}


HType HAllocateObject::CalculateInferredType() {
  return HType::JSObject();
}


HType HFastLiteral::CalculateInferredType() {
  // TODO(mstarzinger): Be smarter, could also be JSArray here.
  return HType::JSObject();
}


HType HArrayLiteral::CalculateInferredType() {
  return HType::JSArray();
}


HType HObjectLiteral::CalculateInferredType() {
  return HType::JSObject();
}


HType HRegExpLiteral::CalculateInferredType() {
  return HType::JSObject();
}


HType HFunctionLiteral::CalculateInferredType() {
  return HType::JSObject();
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


HValue* HForceRepresentation::EnsureAndPropagateNotMinusZero(
    BitVector* visited) {
  visited->Add(id());
  return value();
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


HValue* HMathFloorOfDiv::EnsureAndPropagateNotMinusZero(BitVector* visited) {
  visited->Add(id());
  SetFlag(kBailoutOnMinusZero);
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


bool HStoreKeyed::NeedsCanonicalization() {
  // If value is an integer or comes from the result of a keyed load
  // then it will be a non-hole value: no need for canonicalization.
  if (value()->IsLoadKeyed() ||
      (value()->IsChange() && HChange::cast(value())->from().IsInteger32())) {
    return false;
  }
  return true;
}


#define H_CONSTANT_INT32(val)                                                  \
new(zone) HConstant(FACTORY->NewNumberFromInt(val, TENURED),                   \
                    Representation::Integer32())
#define H_CONSTANT_DOUBLE(val)                                                 \
new(zone) HConstant(FACTORY->NewNumber(val, TENURED),                          \
                    Representation::Double())

#define DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HInstr, op)                       \
HInstruction* HInstr::New##HInstr(Zone* zone,                                  \
                                  HValue* context,                             \
                                  HValue* left,                                \
                                  HValue* right) {                             \
  if (left->IsConstant() && right->IsConstant()) {                             \
    HConstant* c_left = HConstant::cast(left);                                 \
    HConstant* c_right = HConstant::cast(right);                               \
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {             \
      double double_res = c_left->DoubleValue() op c_right->DoubleValue();     \
      if (TypeInfo::IsInt32Double(double_res)) {                               \
        return H_CONSTANT_INT32(static_cast<int32_t>(double_res));             \
      }                                                                        \
      return H_CONSTANT_DOUBLE(double_res);                                    \
    }                                                                          \
  }                                                                            \
  return new(zone) HInstr(context, left, right);                               \
}


DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HAdd, +)
DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HMul, *)
DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HSub, -)

#undef DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR


HInstruction* HMod::NewHMod(Zone* zone,
                            HValue* context,
                            HValue* left,
                            HValue* right) {
  if (left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if (c_left->HasInteger32Value() && c_right->HasInteger32Value()) {
      int32_t dividend = c_left->Integer32Value();
      int32_t divisor = c_right->Integer32Value();
      if (divisor != 0) {
        int32_t res = dividend % divisor;
        if ((res == 0) && (dividend < 0)) {
          return H_CONSTANT_DOUBLE(-0.0);
        }
        return H_CONSTANT_INT32(res);
      }
    }
  }
  return new(zone) HMod(context, left, right);
}


HInstruction* HDiv::NewHDiv(Zone* zone,
                            HValue* context,
                            HValue* left,
                            HValue* right) {
  // If left and right are constant values, try to return a constant value.
  if (left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {
      if (c_right->DoubleValue() != 0) {
        double double_res = c_left->DoubleValue() / c_right->DoubleValue();
        if (TypeInfo::IsInt32Double(double_res)) {
          return H_CONSTANT_INT32(static_cast<int32_t>(double_res));
        }
        return H_CONSTANT_DOUBLE(double_res);
      }
    }
  }
  return new(zone) HDiv(context, left, right);
}


HInstruction* HBitwise::NewHBitwise(Zone* zone,
                                    Token::Value op,
                                    HValue* context,
                                    HValue* left,
                                    HValue* right) {
  if (left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {
      int32_t result;
      int32_t v_left = c_left->NumberValueAsInteger32();
      int32_t v_right = c_right->NumberValueAsInteger32();
      switch (op) {
        case Token::BIT_XOR:
          result = v_left ^ v_right;
          break;
        case Token::BIT_AND:
          result = v_left & v_right;
          break;
        case Token::BIT_OR:
          result = v_left | v_right;
          break;
        default:
          result = 0;  // Please the compiler.
          UNREACHABLE();
      }
      return H_CONSTANT_INT32(result);
    }
  }
  return new(zone) HBitwise(op, context, left, right);
}


#define DEFINE_NEW_H_BITWISE_INSTR(HInstr, result)                             \
HInstruction* HInstr::New##HInstr(Zone* zone,                                  \
                                  HValue* context,                             \
                                  HValue* left,                                \
                                  HValue* right) {                             \
  if (left->IsConstant() && right->IsConstant()) {                             \
    HConstant* c_left = HConstant::cast(left);                                 \
    HConstant* c_right = HConstant::cast(right);                               \
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {             \
      return H_CONSTANT_INT32(result);                                         \
    }                                                                          \
  }                                                                            \
  return new(zone) HInstr(context, left, right);                               \
}


DEFINE_NEW_H_BITWISE_INSTR(HSar,
c_left->NumberValueAsInteger32() >> (c_right->NumberValueAsInteger32() & 0x1f))
DEFINE_NEW_H_BITWISE_INSTR(HShl,
c_left->NumberValueAsInteger32() << (c_right->NumberValueAsInteger32() & 0x1f))

#undef DEFINE_NEW_H_BITWISE_INSTR


HInstruction* HShr::NewHShr(Zone* zone,
                            HValue* context,
                            HValue* left,
                            HValue* right) {
  if (left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {
      int32_t left_val = c_left->NumberValueAsInteger32();
      int32_t right_val = c_right->NumberValueAsInteger32() & 0x1f;
      if ((right_val == 0) && (left_val < 0)) {
        return H_CONSTANT_DOUBLE(
            static_cast<double>(static_cast<uint32_t>(left_val)));
      }
      return H_CONSTANT_INT32(static_cast<uint32_t>(left_val) >> right_val);
    }
  }
  return new(zone) HShr(context, left, right);
}


#undef H_CONSTANT_INT32
#undef H_CONSTANT_DOUBLE


void HIn::PrintDataTo(StringStream* stream) {
  key()->PrintNameTo(stream);
  stream->Add(" ");
  object()->PrintNameTo(stream);
}


void HBitwise::PrintDataTo(StringStream* stream) {
  stream->Add(Token::Name(op_));
  stream->Add(" ");
  HBitwiseBinaryOperation::PrintDataTo(stream);
}


void HPhi::InferRepresentation(HInferRepresentation* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  // If there are non-Phi uses, and all of them have observed the same
  // representation, than that's what this Phi is going to use.
  Representation new_rep = RepresentationObservedByAllNonPhiUses();
  if (!new_rep.IsNone()) {
    UpdateRepresentation(new_rep, h_infer, "unanimous use observations");
    return;
  }
  new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
  new_rep = RepresentationFromUseRequirements();
  UpdateRepresentation(new_rep, h_infer, "use requirements");
}


Representation HPhi::RepresentationObservedByAllNonPhiUses() {
  int non_phi_use_count = 0;
  for (int i = Representation::kInteger32;
       i < Representation::kNumRepresentations; ++i) {
    non_phi_use_count += non_phi_uses_[i];
  }
  if (non_phi_use_count <= 1) return Representation::None();
  for (int i = 0; i < Representation::kNumRepresentations; ++i) {
    if (non_phi_uses_[i] == non_phi_use_count) {
      return Representation::FromKind(static_cast<Representation::Kind>(i));
    }
  }
  return Representation::None();
}


Representation HPhi::RepresentationFromInputs() {
  bool double_occurred = false;
  bool int32_occurred = false;
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* value = OperandAt(i);
    if (value->IsUnknownOSRValue()) {
      HPhi* hint_value = HUnknownOSRValue::cast(value)->incoming_value();
      if (hint_value != NULL) {
        Representation hint = hint_value->representation();
        if (hint.IsTagged()) return hint;
        if (hint.IsDouble()) double_occurred = true;
        if (hint.IsInteger32()) int32_occurred = true;
      }
      continue;
    }
    if (value->representation().IsDouble()) double_occurred = true;
    if (value->representation().IsInteger32()) int32_occurred = true;
    if (value->representation().IsTagged()) {
      if (value->IsConstant()) {
        HConstant* constant = HConstant::cast(value);
        if (constant->IsConvertibleToInteger()) {
          int32_occurred = true;
        } else if (constant->HasNumberValue()) {
          double_occurred = true;
        } else {
          return Representation::Tagged();
        }
      } else {
        if (value->IsPhi() && !IsConvertibleToInteger()) {
          return Representation::Tagged();
        }
      }
    }
  }

  if (double_occurred) return Representation::Double();

  if (int32_occurred) return Representation::Integer32();

  return Representation::None();
}


Representation HPhi::RepresentationFromUseRequirements() {
  Representation all_uses_require = Representation::None();
  bool all_uses_require_the_same = true;
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    // We check for observed_input_representation elsewhere.
    Representation use_rep =
        it.value()->RequiredInputRepresentation(it.index());
    // No useful info from this use -> look at the next one.
    if (use_rep.IsNone()) {
      continue;
    }
    if (use_rep.Equals(all_uses_require)) {
      continue;
    }
    // This use's representation contradicts what we've seen so far.
    if (!all_uses_require.IsNone()) {
      ASSERT(!use_rep.Equals(all_uses_require));
      all_uses_require_the_same = false;
      break;
    }
    // Otherwise, initialize observed representation.
    all_uses_require = use_rep;
  }
  if (all_uses_require_the_same) {
    return all_uses_require;
  }

  return Representation::None();
}


// Node-specific verification code is only included in debug mode.
#ifdef DEBUG

void HPhi::Verify() {
  ASSERT(OperandCount() == block()->predecessors()->length());
  for (int i = 0; i < OperandCount(); ++i) {
    HValue* value = OperandAt(i);
    HBasicBlock* defining_block = value->block();
    HBasicBlock* predecessor_block = block()->predecessors()->at(i);
    ASSERT(defining_block == predecessor_block ||
           defining_block->Dominates(predecessor_block));
  }
}


void HSimulate::Verify() {
  HInstruction::Verify();
  ASSERT(HasAstId());
}


void HCheckSmi::Verify() {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckNonSmi::Verify() {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckFunction::Verify() {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}

#endif

} }  // namespace v8::internal
