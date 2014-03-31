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

#include "double.h"
#include "factory.h"
#include "hydrogen-infer-representation.h"
#include "property-details-inl.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/lithium-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/lithium-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "arm64/lithium-arm64.h"
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


Isolate* HValue::isolate() const {
  ASSERT(block() != NULL);
  return block()->isolate();
}


void HValue::AssumeRepresentation(Representation r) {
  if (CheckFlag(kFlexibleRepresentation)) {
    ChangeRepresentation(r);
    // The representation of the value is dictated by type feedback and
    // will not be changed later.
    ClearFlag(kFlexibleRepresentation);
  }
}


void HValue::InferRepresentation(HInferRepresentationPhase* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
  if (representation().IsSmi() && HasNonSmiUse()) {
    UpdateRepresentation(
        Representation::Integer32(), h_infer, "use requirements");
  }
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
    use_count[rep.kind()] += 1;
  }
  if (IsPhi()) HPhi::cast(this)->AddIndirectUsesTo(&use_count[0]);
  int tagged_count = use_count[Representation::kTagged];
  int double_count = use_count[Representation::kDouble];
  int int32_count = use_count[Representation::kInteger32];
  int smi_count = use_count[Representation::kSmi];

  if (tagged_count > 0) return Representation::Tagged();
  if (double_count > 0) return Representation::Double();
  if (int32_count > 0) return Representation::Integer32();
  if (smi_count > 0) return Representation::Smi();

  return Representation::None();
}


void HValue::UpdateRepresentation(Representation new_rep,
                                  HInferRepresentationPhase* h_infer,
                                  const char* reason) {
  Representation r = representation();
  if (new_rep.is_more_general_than(r)) {
    if (CheckFlag(kCannotBeTagged) && new_rep.IsTagged()) return;
    if (FLAG_trace_representation) {
      PrintF("Changing #%d %s representation %s -> %s based on %s\n",
             id(), Mnemonic(), r.Mnemonic(), new_rep.Mnemonic(), reason);
    }
    ChangeRepresentation(new_rep);
    AddDependantsToWorklist(h_infer);
  }
}


void HValue::AddDependantsToWorklist(HInferRepresentationPhase* h_infer) {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    h_infer->AddToWorklist(it.value());
  }
  for (int i = 0; i < OperandCount(); ++i) {
    h_infer->AddToWorklist(OperandAt(i));
  }
}


static int32_t ConvertAndSetOverflow(Representation r,
                                     int64_t result,
                                     bool* overflow) {
  if (r.IsSmi()) {
    if (result > Smi::kMaxValue) {
      *overflow = true;
      return Smi::kMaxValue;
    }
    if (result < Smi::kMinValue) {
      *overflow = true;
      return Smi::kMinValue;
    }
  } else {
    if (result > kMaxInt) {
      *overflow = true;
      return kMaxInt;
    }
    if (result < kMinInt) {
      *overflow = true;
      return kMinInt;
    }
  }
  return static_cast<int32_t>(result);
}


static int32_t AddWithoutOverflow(Representation r,
                                  int32_t a,
                                  int32_t b,
                                  bool* overflow) {
  int64_t result = static_cast<int64_t>(a) + static_cast<int64_t>(b);
  return ConvertAndSetOverflow(r, result, overflow);
}


static int32_t SubWithoutOverflow(Representation r,
                                  int32_t a,
                                  int32_t b,
                                  bool* overflow) {
  int64_t result = static_cast<int64_t>(a) - static_cast<int64_t>(b);
  return ConvertAndSetOverflow(r, result, overflow);
}


static int32_t MulWithoutOverflow(const Representation& r,
                                  int32_t a,
                                  int32_t b,
                                  bool* overflow) {
  int64_t result = static_cast<int64_t>(a) * static_cast<int64_t>(b);
  return ConvertAndSetOverflow(r, result, overflow);
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
  Representation r = Representation::Integer32();
  lower_ = AddWithoutOverflow(r, lower_, value, &may_overflow);
  upper_ = AddWithoutOverflow(r, upper_, value, &may_overflow);
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


bool Range::AddAndCheckOverflow(const Representation& r, Range* other) {
  bool may_overflow = false;
  lower_ = AddWithoutOverflow(r, lower_, other->lower(), &may_overflow);
  upper_ = AddWithoutOverflow(r, upper_, other->upper(), &may_overflow);
  KeepOrder();
#ifdef DEBUG
  Verify();
#endif
  return may_overflow;
}


bool Range::SubAndCheckOverflow(const Representation& r, Range* other) {
  bool may_overflow = false;
  lower_ = SubWithoutOverflow(r, lower_, other->upper(), &may_overflow);
  upper_ = SubWithoutOverflow(r, upper_, other->lower(), &may_overflow);
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


bool Range::MulAndCheckOverflow(const Representation& r, Range* other) {
  bool may_overflow = false;
  int v1 = MulWithoutOverflow(r, lower_, other->lower(), &may_overflow);
  int v2 = MulWithoutOverflow(r, lower_, other->upper(), &may_overflow);
  int v3 = MulWithoutOverflow(r, upper_, other->lower(), &may_overflow);
  int v4 = MulWithoutOverflow(r, upper_, other->upper(), &may_overflow);
  lower_ = Min(Min(v1, v2), Min(v3, v4));
  upper_ = Max(Max(v1, v2), Max(v3, v4));
#ifdef DEBUG
  Verify();
#endif
  return may_overflow;
}


const char* HType::ToString() {
  // Note: The c1visualizer syntax for locals allows only a sequence of the
  // following characters: A-Za-z0-9_-|:
  switch (type_) {
    case kNone: return "none";
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
  }
  UNREACHABLE();
  return "unreachable";
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


bool HValue::CheckUsesForFlag(Flag f) const {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    if (it.value()->IsSimulate()) continue;
    if (!it.value()->CheckFlag(f)) return false;
  }
  return true;
}


bool HValue::CheckUsesForFlag(Flag f, HValue** value) const {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    if (it.value()->IsSimulate()) continue;
    if (!it.value()->CheckFlag(f)) {
      *value = it.value();
      return false;
    }
  }
  return true;
}


bool HValue::HasAtLeastOneUseWithFlagAndNoneWithout(Flag f) const {
  bool return_value = false;
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    if (it.value()->IsSimulate()) continue;
    if (!it.value()->CheckFlag(f)) return false;
    return_value = true;
  }
  return return_value;
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


bool HValue::CanReplaceWithDummyUses() {
  return FLAG_unreachable_code_elimination &&
      !(block()->IsReachable() ||
        IsBlockEntry() ||
        IsControlInstruction() ||
        IsSimulate() ||
        IsEnterInlined() ||
        IsLeaveInlined());
}


bool HValue::IsInteger32Constant() {
  return IsConstant() && HConstant::cast(this)->HasInteger32Value();
}


int32_t HValue::GetInteger32Constant() {
  return HConstant::cast(this)->Integer32Value();
}


bool HValue::EqualsInteger32Constant(int32_t value) {
  return IsInteger32Constant() && GetInteger32Constant() == value;
}


void HValue::SetOperandAt(int index, HValue* value) {
  RegisterUse(index, value);
  InternalSetOperandAt(index, value);
}


void HValue::DeleteAndReplaceWith(HValue* other) {
  // We replace all uses first, so Delete can assert that there are none.
  if (other != NULL) ReplaceAllUsesWith(other);
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
    if (first != NULL && first->value()->CheckFlag(kIsDead)) {
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
  stream->Add(" type:%s", type().ToString());
}


void HValue::PrintRangeTo(StringStream* stream) {
  if (range() == NULL || range()->IsMostGeneric()) return;
  // Note: The c1visualizer syntax for locals allows only a sequence of the
  // following characters: A-Za-z0-9_-|:
  stream->Add(" range:%d_%d%s",
              range()->lower(),
              range()->upper(),
              range()->CanBeMinusZero() ? "_m0" : "");
}


void HValue::PrintChangesTo(StringStream* stream) {
  GVNFlagSet changes_flags = ChangesFlags();
  if (changes_flags.IsEmpty()) return;
  stream->Add(" changes[");
  if (changes_flags == AllSideEffectsFlagSet()) {
    stream->Add("*");
  } else {
    bool add_comma = false;
#define PRINT_DO(Type)                      \
    if (changes_flags.Contains(k##Type)) {  \
      if (add_comma) stream->Add(",");      \
      add_comma = true;                     \
      stream->Add(#Type);                   \
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


bool HValue::HasMonomorphicJSObjectType() {
  return !GetMonomorphicJSObjectMap().is_null();
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


void HSourcePosition::PrintTo(FILE* out) {
  if (IsUnknown()) {
    PrintF(out, "<?>");
  } else {
    if (FLAG_hydrogen_track_positions) {
      PrintF(out, "<%d:%d>", inlining_id(), position());
    } else {
      PrintF(out, "<0:%d>", raw());
    }
  }
}


void HInstruction::PrintTo(StringStream* stream) {
  PrintMnemonicTo(stream);
  PrintDataTo(stream);
  PrintRangeTo(stream);
  PrintChangesTo(stream);
  PrintTypeTo(stream);
  if (CheckFlag(HValue::kHasNoObservableSideEffects)) {
    stream->Add(" [noOSE]");
  }
  if (CheckFlag(HValue::kIsDead)) {
    stream->Add(" [dead]");
  }
}


void HInstruction::PrintDataTo(StringStream *stream) {
  for (int i = 0; i < OperandCount(); ++i) {
    if (i > 0) stream->Add(" ");
    OperandAt(i)->PrintNameTo(stream);
  }
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
  if (!has_position() && next->has_position()) {
    set_position(next->position());
  }
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
  if (!has_position() && previous->has_position()) {
    set_position(previous->position());
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

  // Verify that all uses are in the graph.
  for (HUseIterator use = uses(); !use.Done(); use.Advance()) {
    if (use.value()->IsInstruction()) {
      ASSERT(HInstruction::cast(use.value())->IsLinked());
    }
  }
}
#endif


static bool HasPrimitiveRepresentation(HValue* instr) {
  return instr->representation().IsInteger32() ||
    instr->representation().IsDouble();
}


bool HInstruction::CanDeoptimize() {
  // TODO(titzer): make this a virtual method?
  switch (opcode()) {
    case HValue::kAccessArgumentsAt:
    case HValue::kApplyArguments:
    case HValue::kArgumentsElements:
    case HValue::kArgumentsLength:
    case HValue::kArgumentsObject:
    case HValue::kBoundsCheckBaseIndexInformation:
    case HValue::kCapturedObject:
    case HValue::kClampToUint8:
    case HValue::kConstant:
    case HValue::kContext:
    case HValue::kDateField:
    case HValue::kDebugBreak:
    case HValue::kDeclareGlobals:
    case HValue::kDiv:
    case HValue::kDummyUse:
    case HValue::kEnterInlined:
    case HValue::kEnvironmentMarker:
    case HValue::kForInCacheArray:
    case HValue::kForInPrepareMap:
    case HValue::kFunctionLiteral:
    case HValue::kGetCachedArrayIndex:
    case HValue::kGoto:
    case HValue::kInnerAllocatedObject:
    case HValue::kInstanceOf:
    case HValue::kInstanceOfKnownGlobal:
    case HValue::kInvokeFunction:
    case HValue::kLeaveInlined:
    case HValue::kLoadContextSlot:
    case HValue::kLoadFieldByIndex:
    case HValue::kLoadFunctionPrototype:
    case HValue::kLoadGlobalCell:
    case HValue::kLoadGlobalGeneric:
    case HValue::kLoadKeyed:
    case HValue::kLoadKeyedGeneric:
    case HValue::kLoadNamedField:
    case HValue::kLoadNamedGeneric:
    case HValue::kLoadRoot:
    case HValue::kMapEnumLength:
    case HValue::kMathFloorOfDiv:
    case HValue::kMathMinMax:
    case HValue::kMod:
    case HValue::kMul:
    case HValue::kOsrEntry:
    case HValue::kParameter:
    case HValue::kPower:
    case HValue::kPushArgument:
    case HValue::kRor:
    case HValue::kSar:
    case HValue::kSeqStringGetChar:
    case HValue::kSeqStringSetChar:
    case HValue::kShl:
    case HValue::kShr:
    case HValue::kSimulate:
    case HValue::kStackCheck:
    case HValue::kStoreCodeEntry:
    case HValue::kStoreContextSlot:
    case HValue::kStoreGlobalCell:
    case HValue::kStoreKeyed:
    case HValue::kStoreKeyedGeneric:
    case HValue::kStoreNamedField:
    case HValue::kStoreNamedGeneric:
    case HValue::kStringAdd:
    case HValue::kStringCharCodeAt:
    case HValue::kStringCharFromCode:
    case HValue::kSub:
    case HValue::kThisFunction:
    case HValue::kToFastProperties:
    case HValue::kTransitionElementsKind:
    case HValue::kTrapAllocationMemento:
    case HValue::kTypeof:
    case HValue::kUnaryMathOperation:
    case HValue::kUseConst:
    case HValue::kWrapReceiver:
      return false;
    case HValue::kForceRepresentation:
    case HValue::kAdd:
    case HValue::kBitwise:
    case HValue::kChange:
    case HValue::kCompareGeneric:
      // These instructions might deoptimize if they are not primitive.
      if (!HasPrimitiveRepresentation(this)) return true;
      for (int i = 0; i < OperandCount(); i++) {
        HValue* input = OperandAt(i);
        if (!HasPrimitiveRepresentation(input)) return true;
      }
      return false;
    default:
      return true;
  }
}


void HDummyUse::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
}


void HEnvironmentMarker::PrintDataTo(StringStream* stream) {
  stream->Add("%s var[%d]", kind() == BIND ? "bind" : "lookup", index());
}


void HUnaryCall::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


void HCallJSFunction::PrintDataTo(StringStream* stream) {
  function()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


HCallJSFunction* HCallJSFunction::New(
    Zone* zone,
    HValue* context,
    HValue* function,
    int argument_count,
    bool pass_argument_count) {
  bool has_stack_check = false;
  if (function->IsConstant()) {
    HConstant* fun_const = HConstant::cast(function);
    Handle<JSFunction> jsfun =
        Handle<JSFunction>::cast(fun_const->handle(zone->isolate()));
    has_stack_check = !jsfun.is_null() &&
        (jsfun->code()->kind() == Code::FUNCTION ||
         jsfun->code()->kind() == Code::OPTIMIZED_FUNCTION);
  }

  return new(zone) HCallJSFunction(
      function, argument_count, pass_argument_count,
      has_stack_check);
}




void HBinaryCall::PrintDataTo(StringStream* stream) {
  first()->PrintNameTo(stream);
  stream->Add(" ");
  second()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


void HBoundsCheck::ApplyIndexChange() {
  if (skip_check()) return;

  DecompositionResult decomposition;
  bool index_is_decomposable = index()->TryDecompose(&decomposition);
  if (index_is_decomposable) {
    ASSERT(decomposition.base() == base());
    if (decomposition.offset() == offset() &&
        decomposition.scale() == scale()) return;
  } else {
    return;
  }

  ReplaceAllUsesWith(index());

  HValue* current_index = decomposition.base();
  int actual_offset = decomposition.offset() + offset();
  int actual_scale = decomposition.scale() + scale();

  Zone* zone = block()->graph()->zone();
  HValue* context = block()->graph()->GetInvalidContext();
  if (actual_offset != 0) {
    HConstant* add_offset = HConstant::New(zone, context, actual_offset);
    add_offset->InsertBefore(this);
    HInstruction* add = HAdd::New(zone, context,
                                  current_index, add_offset);
    add->InsertBefore(this);
    add->AssumeRepresentation(index()->representation());
    add->ClearFlag(kCanOverflow);
    current_index = add;
  }

  if (actual_scale != 0) {
    HConstant* sar_scale = HConstant::New(zone, context, actual_scale);
    sar_scale->InsertBefore(this);
    HInstruction* sar = HSar::New(zone, context,
                                  current_index, sar_scale);
    sar->InsertBefore(this);
    sar->AssumeRepresentation(index()->representation());
    current_index = sar;
  }

  SetOperandAt(0, current_index);

  base_ = NULL;
  offset_ = 0;
  scale_ = 0;
}


void HBoundsCheck::PrintDataTo(StringStream* stream) {
  index()->PrintNameTo(stream);
  stream->Add(" ");
  length()->PrintNameTo(stream);
  if (base() != NULL && (offset() != 0 || scale() != 0)) {
    stream->Add(" base: ((");
    if (base() != index()) {
      index()->PrintNameTo(stream);
    } else {
      stream->Add("index");
    }
    stream->Add(" + %d) >> %d)", offset(), scale());
  }
  if (skip_check()) {
    stream->Add(" [DISABLED]");
  }
}


void HBoundsCheck::InferRepresentation(HInferRepresentationPhase* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  HValue* actual_index = index()->ActualValue();
  HValue* actual_length = length()->ActualValue();
  Representation index_rep = actual_index->representation();
  Representation length_rep = actual_length->representation();
  if (index_rep.IsTagged() && actual_index->type().IsSmi()) {
    index_rep = Representation::Smi();
  }
  if (length_rep.IsTagged() && actual_length->type().IsSmi()) {
    length_rep = Representation::Smi();
  }
  Representation r = index_rep.generalize(length_rep);
  if (r.is_more_general_than(Representation::Integer32())) {
    r = Representation::Integer32();
  }
  UpdateRepresentation(r, h_infer, "boundscheck");
}


Range* HBoundsCheck::InferRange(Zone* zone) {
  Representation r = representation();
  if (r.IsSmiOrInteger32() && length()->HasRange()) {
    int upper = length()->range()->upper() - (allow_equality() ? 0 : 1);
    int lower = 0;

    Range* result = new(zone) Range(lower, upper);
    if (index()->HasRange()) {
      result->Intersect(index()->range());
    }

    // In case of Smi representation, clamp result to Smi::kMaxValue.
    if (r.IsSmi()) result->ClampToSmi();
    return result;
  }
  return HValue::InferRange(zone);
}


void HBoundsCheckBaseIndexInformation::PrintDataTo(StringStream* stream) {
  stream->Add("base: ");
  base_index()->PrintNameTo(stream);
  stream->Add(", check: ");
  base_index()->PrintNameTo(stream);
}


void HCallWithDescriptor::PrintDataTo(StringStream* stream) {
  for (int i = 0; i < OperandCount(); i++) {
    OperandAt(i)->PrintNameTo(stream);
    stream->Add(" ");
  }
  stream->Add("#%d", argument_count());
}


void HCallNewArray::PrintDataTo(StringStream* stream) {
  stream->Add(ElementsKindToString(elements_kind()));
  stream->Add(" ");
  HBinaryCall::PrintDataTo(stream);
}


void HCallRuntime::PrintDataTo(StringStream* stream) {
  stream->Add("%o ", *name());
  if (save_doubles() == kSaveFPRegs) {
    stream->Add("[save doubles] ");
  }
  stream->Add("#%d", argument_count());
}


void HClassOfTestAndBranch::PrintDataTo(StringStream* stream) {
  stream->Add("class_of_test(");
  value()->PrintNameTo(stream);
  stream->Add(", \"%o\")", *class_name());
}


void HWrapReceiver::PrintDataTo(StringStream* stream) {
  receiver()->PrintNameTo(stream);
  stream->Add(" ");
  function()->PrintNameTo(stream);
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


void HReturn::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" (pop ");
  parameter_count()->PrintNameTo(stream);
  stream->Add(" values)");
}


Representation HBranch::observed_input_representation(int index) {
  static const ToBooleanStub::Types tagged_types(
      ToBooleanStub::NULL_TYPE |
      ToBooleanStub::SPEC_OBJECT |
      ToBooleanStub::STRING |
      ToBooleanStub::SYMBOL);
  if (expected_input_types_.ContainsAnyOf(tagged_types)) {
    return Representation::Tagged();
  }
  if (expected_input_types_.Contains(ToBooleanStub::UNDEFINED)) {
    if (expected_input_types_.Contains(ToBooleanStub::HEAP_NUMBER)) {
      return Representation::Double();
    }
    return Representation::Tagged();
  }
  if (expected_input_types_.Contains(ToBooleanStub::HEAP_NUMBER)) {
    return Representation::Double();
  }
  if (expected_input_types_.Contains(ToBooleanStub::SMI)) {
    return Representation::Smi();
  }
  return Representation::None();
}


bool HBranch::KnownSuccessorBlock(HBasicBlock** block) {
  HValue* value = this->value();
  if (value->EmitAtUses()) {
    ASSERT(value->IsConstant());
    ASSERT(!value->representation().IsDouble());
    *block = HConstant::cast(value)->BooleanValue()
        ? FirstSuccessor()
        : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


void HCompareMap::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" (%p)", *map().handle());
  HControlInstruction::PrintDataTo(stream);
  if (known_successor_index() == 0) {
    stream->Add(" [true]");
  } else if (known_successor_index() == 1) {
    stream->Add(" [false]");
  }
}


const char* HUnaryMathOperation::OpName() const {
  switch (op()) {
    case kMathFloor: return "floor";
    case kMathRound: return "round";
    case kMathAbs: return "abs";
    case kMathLog: return "log";
    case kMathExp: return "exp";
    case kMathSqrt: return "sqrt";
    case kMathPowHalf: return "pow-half";
    case kMathClz32: return "clz32";
    default:
      UNREACHABLE();
      return NULL;
  }
}


Range* HUnaryMathOperation::InferRange(Zone* zone) {
  Representation r = representation();
  if (op() == kMathClz32) return new(zone) Range(0, 32);
  if (r.IsSmiOrInteger32() && value()->HasRange()) {
    if (op() == kMathAbs) {
      int upper = value()->range()->upper();
      int lower = value()->range()->lower();
      bool spans_zero = value()->range()->CanBeZero();
      // Math.abs(kMinInt) overflows its representation, on which the
      // instruction deopts. Hence clamp it to kMaxInt.
      int abs_upper = upper == kMinInt ? kMaxInt : abs(upper);
      int abs_lower = lower == kMinInt ? kMaxInt : abs(lower);
      Range* result =
          new(zone) Range(spans_zero ? 0 : Min(abs_lower, abs_upper),
                          Max(abs_lower, abs_upper));
      // In case of Smi representation, clamp Math.abs(Smi::kMinValue) to
      // Smi::kMaxValue.
      if (r.IsSmi()) result->ClampToSmi();
      return result;
    }
  }
  return HValue::InferRange(zone);
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
  stream->Add(" == %o", *type_literal_.handle());
  HControlInstruction::PrintDataTo(stream);
}


static String* TypeOfString(HConstant* constant, Isolate* isolate) {
  Heap* heap = isolate->heap();
  if (constant->HasNumberValue()) return heap->number_string();
  if (constant->IsUndetectable()) return heap->undefined_string();
  if (constant->HasStringValue()) return heap->string_string();
  switch (constant->GetInstanceType()) {
    case ODDBALL_TYPE: {
      Unique<Object> unique = constant->GetUnique();
      if (unique.IsKnownGlobal(heap->true_value()) ||
          unique.IsKnownGlobal(heap->false_value())) {
        return heap->boolean_string();
      }
      if (unique.IsKnownGlobal(heap->null_value())) {
        return FLAG_harmony_typeof ? heap->null_string()
                                   : heap->object_string();
      }
      ASSERT(unique.IsKnownGlobal(heap->undefined_value()));
      return heap->undefined_string();
    }
    case SYMBOL_TYPE:
      return heap->symbol_string();
    case JS_FUNCTION_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return heap->function_string();
    default:
      return heap->object_string();
  }
}


bool HTypeofIsAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    HConstant* constant = HConstant::cast(value());
    String* type_string = TypeOfString(constant, isolate());
    bool same_type = type_literal_.IsKnownGlobal(type_string);
    *block = same_type ? FirstSuccessor() : SecondSuccessor();
    return true;
  } else if (value()->representation().IsSpecialization()) {
    bool number_type =
        type_literal_.IsKnownGlobal(isolate()->heap()->number_string());
    *block = number_type ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
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


static bool MatchLeftIsOnes(HValue* l, HValue* r, HValue** negated) {
  if (!l->EqualsInteger32Constant(~0)) return false;
  *negated = r;
  return true;
}


static bool MatchNegationViaXor(HValue* instr, HValue** negated) {
  if (!instr->IsBitwise()) return false;
  HBitwise* b = HBitwise::cast(instr);
  return (b->op() == Token::BIT_XOR) &&
      (MatchLeftIsOnes(b->left(), b->right(), negated) ||
       MatchLeftIsOnes(b->right(), b->left(), negated));
}


static bool MatchDoubleNegation(HValue* instr, HValue** arg) {
  HValue* negated;
  return MatchNegationViaXor(instr, &negated) &&
      MatchNegationViaXor(negated, arg);
}


HValue* HBitwise::Canonicalize() {
  if (!representation().IsSmiOrInteger32()) return this;
  // If x is an int32, then x & -1 == x, x | 0 == x and x ^ 0 == x.
  int32_t nop_constant = (op() == Token::BIT_AND) ? -1 : 0;
  if (left()->EqualsInteger32Constant(nop_constant) &&
      !right()->CheckFlag(kUint32)) {
    return right();
  }
  if (right()->EqualsInteger32Constant(nop_constant) &&
      !left()->CheckFlag(kUint32)) {
    return left();
  }
  // Optimize double negation, a common pattern used for ToInt32(x).
  HValue* arg;
  if (MatchDoubleNegation(this, &arg) && !arg->CheckFlag(kUint32)) {
    return arg;
  }
  return this;
}


Representation HAdd::RepresentationFromInputs() {
  Representation left_rep = left()->representation();
  if (left_rep.IsExternal()) {
    return Representation::External();
  }
  return HArithmeticBinaryOperation::RepresentationFromInputs();
}


Representation HAdd::RequiredInputRepresentation(int index) {
  if (index == 2) {
    Representation left_rep = left()->representation();
    if (left_rep.IsExternal()) {
      return Representation::Integer32();
    }
  }
  return HArithmeticBinaryOperation::RequiredInputRepresentation(index);
}


static bool IsIdentityOperation(HValue* arg1, HValue* arg2, int32_t identity) {
  return arg1->representation().IsSpecialization() &&
    arg2->EqualsInteger32Constant(identity);
}


HValue* HAdd::Canonicalize() {
  // Adding 0 is an identity operation except in case of -0: -0 + 0 = +0
  if (IsIdentityOperation(left(), right(), 0) &&
      !left()->representation().IsDouble()) {  // Left could be -0.
    return left();
  }
  if (IsIdentityOperation(right(), left(), 0) &&
      !left()->representation().IsDouble()) {  // Right could be -0.
    return right();
  }
  return this;
}


HValue* HSub::Canonicalize() {
  if (IsIdentityOperation(left(), right(), 0)) return left();
  return this;
}


HValue* HMul::Canonicalize() {
  if (IsIdentityOperation(left(), right(), 1)) return left();
  if (IsIdentityOperation(right(), left(), 1)) return right();
  return this;
}


bool HMul::MulMinusOne() {
  if (left()->EqualsInteger32Constant(-1) ||
      right()->EqualsInteger32Constant(-1)) {
    return true;
  }

  return false;
}


HValue* HMod::Canonicalize() {
  return this;
}


HValue* HDiv::Canonicalize() {
  if (IsIdentityOperation(left(), right(), 1)) return left();
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


HInstruction* HForceRepresentation::New(Zone* zone, HValue* context,
       HValue* value, Representation representation) {
  if (FLAG_fold_constants && value->IsConstant()) {
    HConstant* c = HConstant::cast(value);
    if (c->HasNumberValue()) {
      double double_res = c->DoubleValue();
      if (representation.CanContainDouble(double_res)) {
        return HConstant::New(zone, context,
                              static_cast<int32_t>(double_res),
                              representation);
      }
    }
  }
  return new(zone) HForceRepresentation(value, representation);
}


void HForceRepresentation::PrintDataTo(StringStream* stream) {
  stream->Add("%s ", representation().Mnemonic());
  value()->PrintNameTo(stream);
}


void HChange::PrintDataTo(StringStream* stream) {
  HUnaryOperation::PrintDataTo(stream);
  stream->Add(" %s to %s", from().Mnemonic(), to().Mnemonic());

  if (CanTruncateToInt32()) stream->Add(" truncating-int32");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
  if (CheckFlag(kAllowUndefinedAsNaN)) stream->Add(" allow-undefined-as-nan");
}


HValue* HUnaryMathOperation::Canonicalize() {
  if (op() == kMathRound || op() == kMathFloor) {
    HValue* val = value();
    if (val->IsChange()) val = HChange::cast(val)->value();
    if (val->representation().IsSmiOrInteger32()) {
      if (val->representation().Equals(representation())) return val;
      return Prepend(new(block()->zone()) HChange(
          val, representation(), false, false));
    }
  }
  if (op() == kMathFloor && value()->IsDiv() && value()->UseCount() == 1) {
    HDiv* hdiv = HDiv::cast(value());

    HValue* left = hdiv->left();
    if (left->representation().IsInteger32()) {
      // A value with an integer representation does not need to be transformed.
    } else if (left->IsChange() && HChange::cast(left)->from().IsInteger32()) {
      // A change from an integer32 can be replaced by the integer32 value.
      left = HChange::cast(left)->value();
    } else if (hdiv->observed_input_representation(1).IsSmiOrInteger32()) {
      left = Prepend(new(block()->zone()) HChange(
          left, Representation::Integer32(), false, false));
    } else {
      return this;
    }

    HValue* right = hdiv->right();
    if (right->IsInteger32Constant()) {
      right = Prepend(HConstant::cast(right)->CopyToRepresentation(
          Representation::Integer32(), right->block()->zone()));
    } else if (right->representation().IsInteger32()) {
      // A value with an integer representation does not need to be transformed.
    } else if (right->IsChange() &&
               HChange::cast(right)->from().IsInteger32()) {
      // A change from an integer32 can be replaced by the integer32 value.
      right = HChange::cast(right)->value();
    } else if (hdiv->observed_input_representation(2).IsSmiOrInteger32()) {
      right = Prepend(new(block()->zone()) HChange(
          right, Representation::Integer32(), false, false));
    } else {
      return this;
    }

    return Prepend(HMathFloorOfDiv::New(
        block()->zone(), context(), left, right));
  }
  return this;
}


HValue* HCheckInstanceType::Canonicalize() {
  if (check_ == IS_STRING && value()->type().IsString()) {
    return value();
  }

  if (check_ == IS_INTERNALIZED_STRING && value()->IsConstant()) {
    if (HConstant::cast(value())->HasInternalizedStringValue()) {
      return value();
    }
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
    case IS_INTERNALIZED_STRING:
      *mask = kIsNotStringMask | kIsNotInternalizedMask;
      *tag = kInternalizedTag;
      return;
    default:
      UNREACHABLE();
  }
}


bool HCheckMaps::HandleSideEffectDominator(GVNFlag side_effect,
                                           HValue* dominator) {
  ASSERT(side_effect == kMaps);
  // TODO(mstarzinger): For now we specialize on HStoreNamedField, but once
  // type information is rich enough we should generalize this to any HType
  // for which the map is known.
  if (HasNoUses() && dominator->IsStoreNamedField()) {
    HStoreNamedField* store = HStoreNamedField::cast(dominator);
    if (!store->has_transition() || store->object() != value()) return false;
    HConstant* transition = HConstant::cast(store->transition());
    if (map_set_.Contains(Unique<Map>::cast(transition->GetUnique()))) {
      DeleteAndReplaceWith(NULL);
      return true;
    }
  }
  return false;
}


void HCheckMaps::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" [%p", *map_set_.at(0).handle());
  for (int i = 1; i < map_set_.size(); ++i) {
    stream->Add(",%p", *map_set_.at(i).handle());
  }
  stream->Add("]%s", CanOmitMapChecks() ? "(omitted)" : "");
}


void HCheckValue::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" ");
  object().handle()->ShortPrint(stream);
}


HValue* HCheckValue::Canonicalize() {
  return (value()->IsConstant() &&
          HConstant::cast(value())->EqualsUnique(object_)) ? NULL : this;
}


const char* HCheckInstanceType::GetCheckName() {
  switch (check_) {
    case IS_SPEC_OBJECT: return "object";
    case IS_JS_ARRAY: return "array";
    case IS_STRING: return "string";
    case IS_INTERNALIZED_STRING: return "internalized_string";
  }
  UNREACHABLE();
  return "";
}


void HCheckInstanceType::PrintDataTo(StringStream* stream) {
  stream->Add("%s ", GetCheckName());
  HUnaryOperation::PrintDataTo(stream);
}


void HCallStub::PrintDataTo(StringStream* stream) {
  stream->Add("%s ",
              CodeStub::MajorName(major_key_, false));
  HUnaryCall::PrintDataTo(stream);
}


void HUnknownOSRValue::PrintDataTo(StringStream *stream) {
  const char* type = "expression";
  if (environment_->is_local_index(index_)) type = "local";
  if (environment_->is_special_index(index_)) type = "special";
  if (environment_->is_parameter_index(index_)) type = "parameter";
  stream->Add("%s @ %d", type, index_);
}


void HInstanceOf::PrintDataTo(StringStream* stream) {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  stream->Add(" ");
  context()->PrintNameTo(stream);
}


Range* HValue::InferRange(Zone* zone) {
  Range* result;
  if (representation().IsSmi() || type().IsSmi()) {
    result = new(zone) Range(Smi::kMinValue, Smi::kMaxValue);
    result->set_can_be_minus_zero(false);
  } else {
    result = new(zone) Range();
    result->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToInt32));
    // TODO(jkummerow): The range cannot be minus zero when the upper type
    // bound is Integer32.
  }
  return result;
}


Range* HChange::InferRange(Zone* zone) {
  Range* input_range = value()->range();
  if (from().IsInteger32() && !value()->CheckFlag(HInstruction::kUint32) &&
      (to().IsSmi() ||
       (to().IsTagged() &&
        input_range != NULL &&
        input_range->IsInSmiRange()))) {
    set_type(HType::Smi());
    ClearChangesFlag(kNewSpacePromotion);
  }
  if (to().IsSmiOrTagged() &&
      input_range != NULL &&
      input_range->IsInSmiRange() &&
      (!SmiValuesAre32Bits() ||
       !value()->CheckFlag(HValue::kUint32) ||
       input_range->upper() != kMaxInt)) {
    // The Range class can't express upper bounds in the (kMaxInt, kMaxUint32]
    // interval, so we treat kMaxInt as a sentinel for this entire interval.
    ClearFlag(kCanOverflow);
  }
  Range* result = (input_range != NULL)
      ? input_range->Copy(zone)
      : HValue::InferRange(zone);
  result->set_can_be_minus_zero(!to().IsSmiOrInteger32() ||
                                !(CheckFlag(kAllUsesTruncatingToInt32) ||
                                  CheckFlag(kAllUsesTruncatingToSmi)));
  if (to().IsSmi()) result->ClampToSmi();
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


HSourcePosition HPhi::position() const {
  return block()->first()->position();
}


Range* HPhi::InferRange(Zone* zone) {
  Representation r = representation();
  if (r.IsSmiOrInteger32()) {
    if (block()->IsLoopHeader()) {
      Range* range = r.IsSmi()
          ? new(zone) Range(Smi::kMinValue, Smi::kMaxValue)
          : new(zone) Range(kMinInt, kMaxInt);
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
  Representation r = representation();
  if (r.IsSmiOrInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->AddAndCheckOverflow(r, b) ||
        (r.IsInteger32() && CheckFlag(kAllUsesTruncatingToInt32)) ||
        (r.IsSmi() && CheckFlag(kAllUsesTruncatingToSmi))) {
      ClearFlag(kCanOverflow);
    }
    res->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToSmi) &&
                               !CheckFlag(kAllUsesTruncatingToInt32) &&
                               a->CanBeMinusZero() && b->CanBeMinusZero());
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HSub::InferRange(Zone* zone) {
  Representation r = representation();
  if (r.IsSmiOrInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->SubAndCheckOverflow(r, b) ||
        (r.IsInteger32() && CheckFlag(kAllUsesTruncatingToInt32)) ||
        (r.IsSmi() && CheckFlag(kAllUsesTruncatingToSmi))) {
      ClearFlag(kCanOverflow);
    }
    res->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToSmi) &&
                               !CheckFlag(kAllUsesTruncatingToInt32) &&
                               a->CanBeMinusZero() && b->CanBeZero());
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HMul::InferRange(Zone* zone) {
  Representation r = representation();
  if (r.IsSmiOrInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->MulAndCheckOverflow(r, b) ||
        (((r.IsInteger32() && CheckFlag(kAllUsesTruncatingToInt32)) ||
         (r.IsSmi() && CheckFlag(kAllUsesTruncatingToSmi))) &&
         MulMinusOne())) {
      // Truncated int multiplication is too precise and therefore not the
      // same as converting to Double and back.
      // Handle truncated integer multiplication by -1 special.
      ClearFlag(kCanOverflow);
    }
    res->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToSmi) &&
                               !CheckFlag(kAllUsesTruncatingToInt32) &&
                               ((a->CanBeZero() && b->CanBeNegative()) ||
                                (a->CanBeNegative() && b->CanBeZero())));
    return res;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HDiv::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* result = new(zone) Range();
    result->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToInt32) &&
                                  (a->CanBeMinusZero() ||
                                   (a->CanBeZero() && b->CanBeNegative())));
    if (!a->Includes(kMinInt) || !b->Includes(-1)) {
      ClearFlag(kCanOverflow);
    }

    if (!b->CanBeZero()) {
      ClearFlag(kCanBeDivByZero);
    }
    return result;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HMathFloorOfDiv::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* result = new(zone) Range();
    result->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToInt32) &&
                                  (a->CanBeMinusZero() ||
                                   (a->CanBeZero() && b->CanBeNegative())));
    if (!a->Includes(kMinInt)) {
      ClearFlag(kLeftCanBeMinInt);
    }

    if (!a->CanBeNegative()) {
      ClearFlag(HValue::kLeftCanBeNegative);
    }

    if (!a->CanBePositive()) {
      ClearFlag(HValue::kLeftCanBePositive);
    }

    if (!a->Includes(kMinInt) || !b->Includes(-1)) {
      ClearFlag(kCanOverflow);
    }

    if (!b->CanBeZero()) {
      ClearFlag(kCanBeDivByZero);
    }
    return result;
  } else {
    return HValue::InferRange(zone);
  }
}


Range* HMod::InferRange(Zone* zone) {
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();

    // The magnitude of the modulus is bounded by the right operand. Note that
    // apart for the cases involving kMinInt, the calculation below is the same
    // as Max(Abs(b->lower()), Abs(b->upper())) - 1.
    int32_t positive_bound = -(Min(NegAbs(b->lower()), NegAbs(b->upper())) + 1);

    // The result of the modulo operation has the sign of its left operand.
    bool left_can_be_negative = a->CanBeMinusZero() || a->CanBeNegative();
    Range* result = new(zone) Range(left_can_be_negative ? -positive_bound : 0,
                                    a->CanBePositive() ? positive_bound : 0);

    result->set_can_be_minus_zero(!CheckFlag(kAllUsesTruncatingToInt32) &&
                                  left_can_be_negative);

    if (!a->CanBeNegative()) {
      ClearFlag(HValue::kLeftCanBeNegative);
    }

    if (!a->Includes(kMinInt) || !b->Includes(-1)) {
      ClearFlag(HValue::kCanOverflow);
    }

    if (!b->CanBeZero()) {
      ClearFlag(HValue::kCanBeDivByZero);
    }
    return result;
  } else {
    return HValue::InferRange(zone);
  }
}


InductionVariableData* InductionVariableData::ExaminePhi(HPhi* phi) {
  if (phi->block()->loop_information() == NULL) return NULL;
  if (phi->OperandCount() != 2) return NULL;
  int32_t candidate_increment;

  candidate_increment = ComputeIncrement(phi, phi->OperandAt(0));
  if (candidate_increment != 0) {
    return new(phi->block()->graph()->zone())
        InductionVariableData(phi, phi->OperandAt(1), candidate_increment);
  }

  candidate_increment = ComputeIncrement(phi, phi->OperandAt(1));
  if (candidate_increment != 0) {
    return new(phi->block()->graph()->zone())
        InductionVariableData(phi, phi->OperandAt(0), candidate_increment);
  }

  return NULL;
}


/*
 * This function tries to match the following patterns (and all the relevant
 * variants related to |, & and + being commutative):
 * base | constant_or_mask
 * base & constant_and_mask
 * (base + constant_offset) & constant_and_mask
 * (base - constant_offset) & constant_and_mask
 */
void InductionVariableData::DecomposeBitwise(
    HValue* value,
    BitwiseDecompositionResult* result) {
  HValue* base = IgnoreOsrValue(value);
  result->base = value;

  if (!base->representation().IsInteger32()) return;

  if (base->IsBitwise()) {
    bool allow_offset = false;
    int32_t mask = 0;

    HBitwise* bitwise = HBitwise::cast(base);
    if (bitwise->right()->IsInteger32Constant()) {
      mask = bitwise->right()->GetInteger32Constant();
      base = bitwise->left();
    } else if (bitwise->left()->IsInteger32Constant()) {
      mask = bitwise->left()->GetInteger32Constant();
      base = bitwise->right();
    } else {
      return;
    }
    if (bitwise->op() == Token::BIT_AND) {
      result->and_mask = mask;
      allow_offset = true;
    } else if (bitwise->op() == Token::BIT_OR) {
      result->or_mask = mask;
    } else {
      return;
    }

    result->context = bitwise->context();

    if (allow_offset) {
      if (base->IsAdd()) {
        HAdd* add = HAdd::cast(base);
        if (add->right()->IsInteger32Constant()) {
          base = add->left();
        } else if (add->left()->IsInteger32Constant()) {
          base = add->right();
        }
      } else if (base->IsSub()) {
        HSub* sub = HSub::cast(base);
        if (sub->right()->IsInteger32Constant()) {
          base = sub->left();
        }
      }
    }

    result->base = base;
  }
}


void InductionVariableData::AddCheck(HBoundsCheck* check,
                                     int32_t upper_limit) {
  ASSERT(limit_validity() != NULL);
  if (limit_validity() != check->block() &&
      !limit_validity()->Dominates(check->block())) return;
  if (!phi()->block()->current_loop()->IsNestedInThisLoop(
      check->block()->current_loop())) return;

  ChecksRelatedToLength* length_checks = checks();
  while (length_checks != NULL) {
    if (length_checks->length() == check->length()) break;
    length_checks = length_checks->next();
  }
  if (length_checks == NULL) {
    length_checks = new(check->block()->zone())
        ChecksRelatedToLength(check->length(), checks());
    checks_ = length_checks;
  }

  length_checks->AddCheck(check, upper_limit);
}


void InductionVariableData::ChecksRelatedToLength::CloseCurrentBlock() {
  if (checks() != NULL) {
    InductionVariableCheck* c = checks();
    HBasicBlock* current_block = c->check()->block();
    while (c != NULL && c->check()->block() == current_block) {
      c->set_upper_limit(current_upper_limit_);
      c = c->next();
    }
  }
}


void InductionVariableData::ChecksRelatedToLength::UseNewIndexInCurrentBlock(
    Token::Value token,
    int32_t mask,
    HValue* index_base,
    HValue* context) {
  ASSERT(first_check_in_block() != NULL);
  HValue* previous_index = first_check_in_block()->index();
  ASSERT(context != NULL);

  Zone* zone = index_base->block()->graph()->zone();
  set_added_constant(HConstant::New(zone, context, mask));
  if (added_index() != NULL) {
    added_constant()->InsertBefore(added_index());
  } else {
    added_constant()->InsertBefore(first_check_in_block());
  }

  if (added_index() == NULL) {
    first_check_in_block()->ReplaceAllUsesWith(first_check_in_block()->index());
    HInstruction* new_index =  HBitwise::New(zone, context, token, index_base,
                                             added_constant());
    ASSERT(new_index->IsBitwise());
    new_index->ClearAllSideEffects();
    new_index->AssumeRepresentation(Representation::Integer32());
    set_added_index(HBitwise::cast(new_index));
    added_index()->InsertBefore(first_check_in_block());
  }
  ASSERT(added_index()->op() == token);

  added_index()->SetOperandAt(1, index_base);
  added_index()->SetOperandAt(2, added_constant());
  first_check_in_block()->SetOperandAt(0, added_index());
  if (previous_index->UseCount() == 0) {
    previous_index->DeleteAndReplaceWith(NULL);
  }
}

void InductionVariableData::ChecksRelatedToLength::AddCheck(
    HBoundsCheck* check,
    int32_t upper_limit) {
  BitwiseDecompositionResult decomposition;
  InductionVariableData::DecomposeBitwise(check->index(), &decomposition);

  if (first_check_in_block() == NULL ||
      first_check_in_block()->block() != check->block()) {
    CloseCurrentBlock();

    first_check_in_block_ = check;
    set_added_index(NULL);
    set_added_constant(NULL);
    current_and_mask_in_block_ = decomposition.and_mask;
    current_or_mask_in_block_ = decomposition.or_mask;
    current_upper_limit_ = upper_limit;

    InductionVariableCheck* new_check = new(check->block()->graph()->zone())
        InductionVariableCheck(check, checks_, upper_limit);
    checks_ = new_check;
    return;
  }

  if (upper_limit > current_upper_limit()) {
    current_upper_limit_ = upper_limit;
  }

  if (decomposition.and_mask != 0 &&
      current_or_mask_in_block() == 0) {
    if (current_and_mask_in_block() == 0 ||
        decomposition.and_mask > current_and_mask_in_block()) {
      UseNewIndexInCurrentBlock(Token::BIT_AND,
                                decomposition.and_mask,
                                decomposition.base,
                                decomposition.context);
      current_and_mask_in_block_ = decomposition.and_mask;
    }
    check->set_skip_check();
  }
  if (current_and_mask_in_block() == 0) {
    if (decomposition.or_mask > current_or_mask_in_block()) {
      UseNewIndexInCurrentBlock(Token::BIT_OR,
                                decomposition.or_mask,
                                decomposition.base,
                                decomposition.context);
      current_or_mask_in_block_ = decomposition.or_mask;
    }
    check->set_skip_check();
  }

  if (!check->skip_check()) {
    InductionVariableCheck* new_check = new(check->block()->graph()->zone())
        InductionVariableCheck(check, checks_, upper_limit);
    checks_ = new_check;
  }
}


/*
 * This method detects if phi is an induction variable, with phi_operand as
 * its "incremented" value (the other operand would be the "base" value).
 *
 * It cheks is phi_operand has the form "phi + constant".
 * If yes, the constant is the increment that the induction variable gets at
 * every loop iteration.
 * Otherwise it returns 0.
 */
int32_t InductionVariableData::ComputeIncrement(HPhi* phi,
                                                HValue* phi_operand) {
  if (!phi_operand->representation().IsInteger32()) return 0;

  if (phi_operand->IsAdd()) {
    HAdd* operation = HAdd::cast(phi_operand);
    if (operation->left() == phi &&
        operation->right()->IsInteger32Constant()) {
      return operation->right()->GetInteger32Constant();
    } else if (operation->right() == phi &&
               operation->left()->IsInteger32Constant()) {
      return operation->left()->GetInteger32Constant();
    }
  } else if (phi_operand->IsSub()) {
    HSub* operation = HSub::cast(phi_operand);
    if (operation->left() == phi &&
        operation->right()->IsInteger32Constant()) {
      return -operation->right()->GetInteger32Constant();
    }
  }

  return 0;
}


/*
 * Swaps the information in "update" with the one contained in "this".
 * The swapping is important because this method is used while doing a
 * dominator tree traversal, and "update" will retain the old data that
 * will be restored while backtracking.
 */
void InductionVariableData::UpdateAdditionalLimit(
    InductionVariableLimitUpdate* update) {
  ASSERT(update->updated_variable == this);
  if (update->limit_is_upper) {
    swap(&additional_upper_limit_, &update->limit);
    swap(&additional_upper_limit_is_included_, &update->limit_is_included);
  } else {
    swap(&additional_lower_limit_, &update->limit);
    swap(&additional_lower_limit_is_included_, &update->limit_is_included);
  }
}


int32_t InductionVariableData::ComputeUpperLimit(int32_t and_mask,
                                                 int32_t or_mask) {
  // Should be Smi::kMaxValue but it must fit 32 bits; lower is safe anyway.
  const int32_t MAX_LIMIT = 1 << 30;

  int32_t result = MAX_LIMIT;

  if (limit() != NULL &&
      limit()->IsInteger32Constant()) {
    int32_t limit_value = limit()->GetInteger32Constant();
    if (!limit_included()) {
      limit_value--;
    }
    if (limit_value < result) result = limit_value;
  }

  if (additional_upper_limit() != NULL &&
      additional_upper_limit()->IsInteger32Constant()) {
    int32_t limit_value = additional_upper_limit()->GetInteger32Constant();
    if (!additional_upper_limit_is_included()) {
      limit_value--;
    }
    if (limit_value < result) result = limit_value;
  }

  if (and_mask > 0 && and_mask < MAX_LIMIT) {
    if (and_mask < result) result = and_mask;
    return result;
  }

  // Add the effect of the or_mask.
  result |= or_mask;

  return result >= MAX_LIMIT ? kNoLimit : result;
}


HValue* InductionVariableData::IgnoreOsrValue(HValue* v) {
  if (!v->IsPhi()) return v;
  HPhi* phi = HPhi::cast(v);
  if (phi->OperandCount() != 2) return v;
  if (phi->OperandAt(0)->block()->is_osr_entry()) {
    return phi->OperandAt(1);
  } else if (phi->OperandAt(1)->block()->is_osr_entry()) {
    return phi->OperandAt(0);
  } else {
    return v;
  }
}


InductionVariableData* InductionVariableData::GetInductionVariableData(
    HValue* v) {
  v = IgnoreOsrValue(v);
  if (v->IsPhi()) {
    return HPhi::cast(v)->induction_variable_data();
  }
  return NULL;
}


/*
 * Check if a conditional branch to "current_branch" with token "token" is
 * the branch that keeps the induction loop running (and, conversely, will
 * terminate it if the "other_branch" is taken).
 *
 * Three conditions must be met:
 * - "current_branch" must be in the induction loop.
 * - "other_branch" must be out of the induction loop.
 * - "token" and the induction increment must be "compatible": the token should
 *   be a condition that keeps the execution inside the loop until the limit is
 *   reached.
 */
bool InductionVariableData::CheckIfBranchIsLoopGuard(
    Token::Value token,
    HBasicBlock* current_branch,
    HBasicBlock* other_branch) {
  if (!phi()->block()->current_loop()->IsNestedInThisLoop(
      current_branch->current_loop())) {
    return false;
  }

  if (phi()->block()->current_loop()->IsNestedInThisLoop(
      other_branch->current_loop())) {
    return false;
  }

  if (increment() > 0 && (token == Token::LT || token == Token::LTE)) {
    return true;
  }
  if (increment() < 0 && (token == Token::GT || token == Token::GTE)) {
    return true;
  }
  if (Token::IsInequalityOp(token) && (increment() == 1 || increment() == -1)) {
    return true;
  }

  return false;
}


void InductionVariableData::ComputeLimitFromPredecessorBlock(
    HBasicBlock* block,
    LimitFromPredecessorBlock* result) {
  if (block->predecessors()->length() != 1) return;
  HBasicBlock* predecessor = block->predecessors()->at(0);
  HInstruction* end = predecessor->last();

  if (!end->IsCompareNumericAndBranch()) return;
  HCompareNumericAndBranch* branch = HCompareNumericAndBranch::cast(end);

  Token::Value token = branch->token();
  if (!Token::IsArithmeticCompareOp(token)) return;

  HBasicBlock* other_target;
  if (block == branch->SuccessorAt(0)) {
    other_target = branch->SuccessorAt(1);
  } else {
    other_target = branch->SuccessorAt(0);
    token = Token::NegateCompareOp(token);
    ASSERT(block == branch->SuccessorAt(1));
  }

  InductionVariableData* data;

  data = GetInductionVariableData(branch->left());
  HValue* limit = branch->right();
  if (data == NULL) {
    data = GetInductionVariableData(branch->right());
    token = Token::ReverseCompareOp(token);
    limit = branch->left();
  }

  if (data != NULL) {
    result->variable = data;
    result->token = token;
    result->limit = limit;
    result->other_target = other_target;
  }
}


/*
 * Compute the limit that is imposed on an induction variable when entering
 * "block" (if any).
 * If the limit is the "proper" induction limit (the one that makes the loop
 * terminate when the induction variable reaches it) it is stored directly in
 * the induction variable data.
 * Otherwise the limit is written in "additional_limit" and the method
 * returns true.
 */
bool InductionVariableData::ComputeInductionVariableLimit(
    HBasicBlock* block,
    InductionVariableLimitUpdate* additional_limit) {
  LimitFromPredecessorBlock limit;
  ComputeLimitFromPredecessorBlock(block, &limit);
  if (!limit.LimitIsValid()) return false;

  if (limit.variable->CheckIfBranchIsLoopGuard(limit.token,
                                               block,
                                               limit.other_target)) {
    limit.variable->limit_ = limit.limit;
    limit.variable->limit_included_ = limit.LimitIsIncluded();
    limit.variable->limit_validity_ = block;
    limit.variable->induction_exit_block_ = block->predecessors()->at(0);
    limit.variable->induction_exit_target_ = limit.other_target;
    return false;
  } else {
    additional_limit->updated_variable = limit.variable;
    additional_limit->limit = limit.limit;
    additional_limit->limit_is_upper = limit.LimitIsUpper();
    additional_limit->limit_is_included = limit.LimitIsIncluded();
    return true;
  }
}


Range* HMathMinMax::InferRange(Zone* zone) {
  if (representation().IsSmiOrInteger32()) {
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
  stream->Add(" uses:%d_%ds_%di_%dd_%dt",
              UseCount(),
              smi_non_phi_uses() + smi_indirect_uses(),
              int32_non_phi_uses() + int32_indirect_uses(),
              double_non_phi_uses() + double_indirect_uses(),
              tagged_non_phi_uses() + tagged_indirect_uses());
  PrintRangeTo(stream);
  PrintTypeTo(stream);
  stream->Add("]");
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
  // Compute a conservative approximation of truncating uses before inferring
  // representations. The proper, exact computation will be done later, when
  // inserting representation changes.
  SetFlag(kTruncatingToSmi);
  SetFlag(kTruncatingToInt32);
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* value = it.value();
    if (!value->IsPhi()) {
      Representation rep = value->observed_input_representation(it.index());
      non_phi_uses_[rep.kind()] += 1;
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is used by real #%d %s as %s\n",
               id(), value->id(), value->Mnemonic(), rep.Mnemonic());
      }
      if (!value->IsSimulate()) {
        if (!value->CheckFlag(kTruncatingToSmi)) {
          ClearFlag(kTruncatingToSmi);
        }
        if (!value->CheckFlag(kTruncatingToInt32)) {
          ClearFlag(kTruncatingToInt32);
        }
      }
    }
  }
}


void HPhi::AddNonPhiUsesFrom(HPhi* other) {
  if (FLAG_trace_representation) {
    PrintF("adding to #%d Phi uses of #%d Phi: s%d i%d d%d t%d\n",
           id(), other->id(),
           other->non_phi_uses_[Representation::kSmi],
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


void HSimulate::MergeWith(ZoneList<HSimulate*>* list) {
  while (!list->is_empty()) {
    HSimulate* from = list->RemoveLast();
    ZoneList<HValue*>* from_values = &from->values_;
    for (int i = 0; i < from_values->length(); ++i) {
      if (from->HasAssignedIndexAt(i)) {
        int index = from->GetAssignedIndexAt(i);
        if (HasValueForIndex(index)) continue;
        AddAssignedValue(index, from_values->at(i));
      } else {
        if (pop_count_ > 0) {
          pop_count_--;
        } else {
          AddPushedValue(from_values->at(i));
        }
      }
    }
    pop_count_ += from->pop_count_;
    from->DeleteAndReplaceWith(NULL);
  }
}


void HSimulate::PrintDataTo(StringStream* stream) {
  stream->Add("id=%d", ast_id().ToInt());
  if (pop_count_ > 0) stream->Add(" pop %d", pop_count_);
  if (values_.length() > 0) {
    if (pop_count_ > 0) stream->Add(" /");
    for (int i = values_.length() - 1; i >= 0; --i) {
      if (HasAssignedIndexAt(i)) {
        stream->Add(" var[%d] = ", GetAssignedIndexAt(i));
      } else {
        stream->Add(" push ");
      }
      values_[i]->PrintNameTo(stream);
      if (i > 0) stream->Add(",");
    }
  }
}


void HSimulate::ReplayEnvironment(HEnvironment* env) {
  if (done_with_replay_) return;
  ASSERT(env != NULL);
  env->set_ast_id(ast_id());
  env->Drop(pop_count());
  for (int i = values()->length() - 1; i >= 0; --i) {
    HValue* value = values()->at(i);
    if (HasAssignedIndexAt(i)) {
      env->Bind(GetAssignedIndexAt(i), value);
    } else {
      env->Push(value);
    }
  }
  done_with_replay_ = true;
}


static void ReplayEnvironmentNested(const ZoneList<HValue*>* values,
                                    HCapturedObject* other) {
  for (int i = 0; i < values->length(); ++i) {
    HValue* value = values->at(i);
    if (value->IsCapturedObject()) {
      if (HCapturedObject::cast(value)->capture_id() == other->capture_id()) {
        values->at(i) = other;
      } else {
        ReplayEnvironmentNested(HCapturedObject::cast(value)->values(), other);
      }
    }
  }
}


// Replay captured objects by replacing all captured objects with the
// same capture id in the current and all outer environments.
void HCapturedObject::ReplayEnvironment(HEnvironment* env) {
  ASSERT(env != NULL);
  while (env != NULL) {
    ReplayEnvironmentNested(env->values(), this);
    env = env->outer();
  }
}


void HCapturedObject::PrintDataTo(StringStream* stream) {
  stream->Add("#%d ", capture_id());
  HDematerializedObject::PrintDataTo(stream);
}


void HEnterInlined::RegisterReturnTarget(HBasicBlock* return_target,
                                         Zone* zone) {
  ASSERT(return_target->IsInlineReturnTarget());
  return_targets_.Add(return_target, zone);
}


void HEnterInlined::PrintDataTo(StringStream* stream) {
  SmartArrayPointer<char> name = function()->debug_name()->ToCString();
  stream->Add("%s, id=%d", name.get(), function()->id().ToInt());
}


static bool IsInteger32(double value) {
  double roundtrip_value = static_cast<double>(static_cast<int32_t>(value));
  return BitCast<int64_t>(roundtrip_value) == BitCast<int64_t>(value);
}


HConstant::HConstant(Handle<Object> handle, Representation r)
  : HTemplateInstruction<0>(HType::TypeFromValue(handle)),
    object_(Unique<Object>::CreateUninitialized(handle)),
    has_smi_value_(false),
    has_int32_value_(false),
    has_double_value_(false),
    has_external_reference_value_(false),
    is_not_in_new_space_(true),
    boolean_value_(handle->BooleanValue()),
    is_undetectable_(false),
    instance_type_(kUnknownInstanceType) {
  if (handle->IsHeapObject()) {
    Handle<HeapObject> heap_obj = Handle<HeapObject>::cast(handle);
    Heap* heap = heap_obj->GetHeap();
    is_not_in_new_space_ = !heap->InNewSpace(*handle);
    instance_type_ = heap_obj->map()->instance_type();
    is_undetectable_ = heap_obj->map()->is_undetectable();
  }
  if (handle->IsNumber()) {
    double n = handle->Number();
    has_int32_value_ = IsInteger32(n);
    int32_value_ = DoubleToInt32(n);
    has_smi_value_ = has_int32_value_ && Smi::IsValid(int32_value_);
    double_value_ = n;
    has_double_value_ = true;
    // TODO(titzer): if this heap number is new space, tenure a new one.
  }

  Initialize(r);
}


HConstant::HConstant(Unique<Object> unique,
                     Representation r,
                     HType type,
                     bool is_not_in_new_space,
                     bool boolean_value,
                     bool is_undetectable,
                     InstanceType instance_type)
  : HTemplateInstruction<0>(type),
    object_(unique),
    has_smi_value_(false),
    has_int32_value_(false),
    has_double_value_(false),
    has_external_reference_value_(false),
    is_not_in_new_space_(is_not_in_new_space),
    boolean_value_(boolean_value),
    is_undetectable_(is_undetectable),
    instance_type_(instance_type) {
  ASSERT(!unique.handle().is_null());
  ASSERT(!type.IsTaggedNumber());
  Initialize(r);
}


HConstant::HConstant(int32_t integer_value,
                     Representation r,
                     bool is_not_in_new_space,
                     Unique<Object> object)
  : object_(object),
    has_smi_value_(Smi::IsValid(integer_value)),
    has_int32_value_(true),
    has_double_value_(true),
    has_external_reference_value_(false),
    is_not_in_new_space_(is_not_in_new_space),
    boolean_value_(integer_value != 0),
    is_undetectable_(false),
    int32_value_(integer_value),
    double_value_(FastI2D(integer_value)),
    instance_type_(kUnknownInstanceType) {
  // It's possible to create a constant with a value in Smi-range but stored
  // in a (pre-existing) HeapNumber. See crbug.com/349878.
  bool could_be_heapobject = r.IsTagged() && !object.handle().is_null();
  bool is_smi = has_smi_value_ && !could_be_heapobject;
  set_type(is_smi ? HType::Smi() : HType::TaggedNumber());
  Initialize(r);
}


HConstant::HConstant(double double_value,
                     Representation r,
                     bool is_not_in_new_space,
                     Unique<Object> object)
  : object_(object),
    has_int32_value_(IsInteger32(double_value)),
    has_double_value_(true),
    has_external_reference_value_(false),
    is_not_in_new_space_(is_not_in_new_space),
    boolean_value_(double_value != 0 && !std::isnan(double_value)),
    is_undetectable_(false),
    int32_value_(DoubleToInt32(double_value)),
    double_value_(double_value),
    instance_type_(kUnknownInstanceType) {
  has_smi_value_ = has_int32_value_ && Smi::IsValid(int32_value_);
  // It's possible to create a constant with a value in Smi-range but stored
  // in a (pre-existing) HeapNumber. See crbug.com/349878.
  bool could_be_heapobject = r.IsTagged() && !object.handle().is_null();
  bool is_smi = has_smi_value_ && !could_be_heapobject;
  set_type(is_smi ? HType::Smi() : HType::TaggedNumber());
  Initialize(r);
}


HConstant::HConstant(ExternalReference reference)
  : HTemplateInstruction<0>(HType::None()),
    object_(Unique<Object>(Handle<Object>::null())),
    has_smi_value_(false),
    has_int32_value_(false),
    has_double_value_(false),
    has_external_reference_value_(true),
    is_not_in_new_space_(true),
    boolean_value_(true),
    is_undetectable_(false),
    external_reference_value_(reference),
    instance_type_(kUnknownInstanceType) {
  Initialize(Representation::External());
}


void HConstant::Initialize(Representation r) {
  if (r.IsNone()) {
    if (has_smi_value_ && SmiValuesAre31Bits()) {
      r = Representation::Smi();
    } else if (has_int32_value_) {
      r = Representation::Integer32();
    } else if (has_double_value_) {
      r = Representation::Double();
    } else if (has_external_reference_value_) {
      r = Representation::External();
    } else {
      Handle<Object> object = object_.handle();
      if (object->IsJSObject()) {
        // Try to eagerly migrate JSObjects that have deprecated maps.
        Handle<JSObject> js_object = Handle<JSObject>::cast(object);
        if (js_object->map()->is_deprecated()) {
          JSObject::TryMigrateInstance(js_object);
        }
      }
      r = Representation::Tagged();
    }
  }
  set_representation(r);
  SetFlag(kUseGVN);
}


bool HConstant::ImmortalImmovable() const {
  if (has_int32_value_) {
    return false;
  }
  if (has_double_value_) {
    if (IsSpecialDouble()) {
      return true;
    }
    return false;
  }
  if (has_external_reference_value_) {
    return false;
  }

  ASSERT(!object_.handle().is_null());
  Heap* heap = isolate()->heap();
  ASSERT(!object_.IsKnownGlobal(heap->minus_zero_value()));
  ASSERT(!object_.IsKnownGlobal(heap->nan_value()));
  return
#define IMMORTAL_IMMOVABLE_ROOT(name) \
      object_.IsKnownGlobal(heap->name()) ||
      IMMORTAL_IMMOVABLE_ROOT_LIST(IMMORTAL_IMMOVABLE_ROOT)
#undef IMMORTAL_IMMOVABLE_ROOT
#define INTERNALIZED_STRING(name, value) \
      object_.IsKnownGlobal(heap->name()) ||
      INTERNALIZED_STRING_LIST(INTERNALIZED_STRING)
#undef INTERNALIZED_STRING
#define STRING_TYPE(NAME, size, name, Name) \
      object_.IsKnownGlobal(heap->name##_map()) ||
      STRING_TYPE_LIST(STRING_TYPE)
#undef STRING_TYPE
      false;
}


bool HConstant::EmitAtUses() {
  ASSERT(IsLinked());
  if (block()->graph()->has_osr() &&
      block()->graph()->IsStandardConstant(this)) {
    // TODO(titzer): this seems like a hack that should be fixed by custom OSR.
    return true;
  }
  if (UseCount() == 0) return true;
  if (IsCell()) return false;
  if (representation().IsDouble()) return false;
  return true;
}


HConstant* HConstant::CopyToRepresentation(Representation r, Zone* zone) const {
  if (r.IsSmi() && !has_smi_value_) return NULL;
  if (r.IsInteger32() && !has_int32_value_) return NULL;
  if (r.IsDouble() && !has_double_value_) return NULL;
  if (r.IsExternal() && !has_external_reference_value_) return NULL;
  if (has_int32_value_) {
    return new(zone) HConstant(int32_value_, r, is_not_in_new_space_, object_);
  }
  if (has_double_value_) {
    return new(zone) HConstant(double_value_, r, is_not_in_new_space_, object_);
  }
  if (has_external_reference_value_) {
    return new(zone) HConstant(external_reference_value_);
  }
  ASSERT(!object_.handle().is_null());
  return new(zone) HConstant(object_,
                             r,
                             type_,
                             is_not_in_new_space_,
                             boolean_value_,
                             is_undetectable_,
                             instance_type_);
}


Maybe<HConstant*> HConstant::CopyToTruncatedInt32(Zone* zone) {
  HConstant* res = NULL;
  if (has_int32_value_) {
    res = new(zone) HConstant(int32_value_,
                              Representation::Integer32(),
                              is_not_in_new_space_,
                              object_);
  } else if (has_double_value_) {
    res = new(zone) HConstant(DoubleToInt32(double_value_),
                              Representation::Integer32(),
                              is_not_in_new_space_,
                              object_);
  }
  return Maybe<HConstant*>(res != NULL, res);
}


Maybe<HConstant*> HConstant::CopyToTruncatedNumber(Zone* zone) {
  HConstant* res = NULL;
  Handle<Object> handle = this->handle(zone->isolate());
  if (handle->IsBoolean()) {
    res = handle->BooleanValue() ?
      new(zone) HConstant(1) : new(zone) HConstant(0);
  } else if (handle->IsUndefined()) {
    res = new(zone) HConstant(OS::nan_value());
  } else if (handle->IsNull()) {
    res = new(zone) HConstant(0);
  }
  return Maybe<HConstant*>(res != NULL, res);
}


void HConstant::PrintDataTo(StringStream* stream) {
  if (has_int32_value_) {
    stream->Add("%d ", int32_value_);
  } else if (has_double_value_) {
    stream->Add("%f ", FmtElm(double_value_));
  } else if (has_external_reference_value_) {
    stream->Add("%p ", reinterpret_cast<void*>(
            external_reference_value_.address()));
  } else {
    handle(Isolate::Current())->ShortPrint(stream);
  }
  if (!is_not_in_new_space_) {
    stream->Add("[new space] ");
  }
}


void HBinaryOperation::PrintDataTo(StringStream* stream) {
  left()->PrintNameTo(stream);
  stream->Add(" ");
  right()->PrintNameTo(stream);
  if (CheckFlag(kCanOverflow)) stream->Add(" !");
  if (CheckFlag(kBailoutOnMinusZero)) stream->Add(" -0?");
}


void HBinaryOperation::InferRepresentation(HInferRepresentationPhase* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");

  if (representation().IsSmi() && HasNonSmiUse()) {
    UpdateRepresentation(
        Representation::Integer32(), h_infer, "use requirements");
  }

  if (observed_output_representation_.IsNone()) {
    new_rep = RepresentationFromUses();
    UpdateRepresentation(new_rep, h_infer, "uses");
  } else {
    new_rep = RepresentationFromOutput();
    UpdateRepresentation(new_rep, h_infer, "output");
  }
}


Representation HBinaryOperation::RepresentationFromInputs() {
  // Determine the worst case of observed input representations and
  // the currently assumed output representation.
  Representation rep = representation();
  for (int i = 1; i <= 2; ++i) {
    rep = rep.generalize(observed_input_representation(i));
  }
  // If any of the actual input representation is more general than what we
  // have so far but not Tagged, use that representation instead.
  Representation left_rep = left()->representation();
  Representation right_rep = right()->representation();
  if (!left_rep.IsTagged()) rep = rep.generalize(left_rep);
  if (!right_rep.IsTagged()) rep = rep.generalize(right_rep);

  return rep;
}


bool HBinaryOperation::IgnoreObservedOutputRepresentation(
    Representation current_rep) {
  return ((current_rep.IsInteger32() && CheckUsesForFlag(kTruncatingToInt32)) ||
          (current_rep.IsSmi() && CheckUsesForFlag(kTruncatingToSmi))) &&
         // Mul in Integer32 mode would be too precise.
         (!this->IsMul() || HMul::cast(this)->MulMinusOne());
}


Representation HBinaryOperation::RepresentationFromOutput() {
  Representation rep = representation();
  // Consider observed output representation, but ignore it if it's Double,
  // this instruction is not a division, and all its uses are truncating
  // to Integer32.
  if (observed_output_representation_.is_more_general_than(rep) &&
      !IgnoreObservedOutputRepresentation(rep)) {
    return observed_output_representation_;
  }
  return Representation::None();
}


void HBinaryOperation::AssumeRepresentation(Representation r) {
  set_observed_input_representation(1, r);
  set_observed_input_representation(2, r);
  HValue::AssumeRepresentation(r);
}


void HMathMinMax::InferRepresentation(HInferRepresentationPhase* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  // Do not care about uses.
}


Range* HBitwise::InferRange(Zone* zone) {
  if (op() == Token::BIT_XOR) {
    if (left()->HasRange() && right()->HasRange()) {
      // The maximum value has the high bit, and all bits below, set:
      // (1 << high) - 1.
      // If the range can be negative, the minimum int is a negative number with
      // the high bit, and all bits below, unset:
      // -(1 << high).
      // If it cannot be negative, conservatively choose 0 as minimum int.
      int64_t left_upper = left()->range()->upper();
      int64_t left_lower = left()->range()->lower();
      int64_t right_upper = right()->range()->upper();
      int64_t right_lower = right()->range()->lower();

      if (left_upper < 0) left_upper = ~left_upper;
      if (left_lower < 0) left_lower = ~left_lower;
      if (right_upper < 0) right_upper = ~right_upper;
      if (right_lower < 0) right_lower = ~right_lower;

      int high = MostSignificantBit(
          static_cast<uint32_t>(
              left_upper | left_lower | right_upper | right_lower));

      int64_t limit = 1;
      limit <<= high;
      int32_t min = (left()->range()->CanBeNegative() ||
                     right()->range()->CanBeNegative())
                    ? static_cast<int32_t>(-limit) : 0;
      return new(zone) Range(min, static_cast<int32_t>(limit - 1));
    }
    Range* result = HValue::InferRange(zone);
    result->set_can_be_minus_zero(false);
    return result;
  }
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
  if (result_mask >= 0) return new(zone) Range(0, result_mask);

  Range* result = HValue::InferRange(zone);
  result->set_can_be_minus_zero(false);
  return result;
}


Range* HSar::InferRange(Zone* zone) {
  if (right()->IsConstant()) {
    HConstant* c = HConstant::cast(right());
    if (c->HasInteger32Value()) {
      Range* result = (left()->range() != NULL)
          ? left()->range()->Copy(zone)
          : new(zone) Range();
      result->Sar(c->Integer32Value());
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
      return result;
    }
  }
  return HValue::InferRange(zone);
}


Range* HLoadNamedField::InferRange(Zone* zone) {
  if (access().representation().IsInteger8()) {
    return new(zone) Range(kMinInt8, kMaxInt8);
  }
  if (access().representation().IsUInteger8()) {
    return new(zone) Range(kMinUInt8, kMaxUInt8);
  }
  if (access().representation().IsInteger16()) {
    return new(zone) Range(kMinInt16, kMaxInt16);
  }
  if (access().representation().IsUInteger16()) {
    return new(zone) Range(kMinUInt16, kMaxUInt16);
  }
  if (access().IsStringLength()) {
    return new(zone) Range(0, String::kMaxLength);
  }
  return HValue::InferRange(zone);
}


Range* HLoadKeyed::InferRange(Zone* zone) {
  switch (elements_kind()) {
    case EXTERNAL_INT8_ELEMENTS:
      return new(zone) Range(kMinInt8, kMaxInt8);
    case EXTERNAL_UINT8_ELEMENTS:
    case EXTERNAL_UINT8_CLAMPED_ELEMENTS:
      return new(zone) Range(kMinUInt8, kMaxUInt8);
    case EXTERNAL_INT16_ELEMENTS:
      return new(zone) Range(kMinInt16, kMaxInt16);
    case EXTERNAL_UINT16_ELEMENTS:
      return new(zone) Range(kMinUInt16, kMaxUInt16);
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


void HCompareNumericAndBranch::PrintDataTo(StringStream* stream) {
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


bool HCompareObjectEqAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (known_successor_index() != kNoKnownSuccessorIndex) {
    *block = SuccessorAt(known_successor_index());
    return true;
  }
  if (FLAG_fold_constants && left()->IsConstant() && right()->IsConstant()) {
    *block = HConstant::cast(left())->DataEquals(HConstant::cast(right()))
        ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


bool ConstantIsObject(HConstant* constant, Isolate* isolate) {
  if (constant->HasNumberValue()) return false;
  if (constant->GetUnique().IsKnownGlobal(isolate->heap()->null_value())) {
    return true;
  }
  if (constant->IsUndetectable()) return false;
  InstanceType type = constant->GetInstanceType();
  return (FIRST_NONCALLABLE_SPEC_OBJECT_TYPE <= type) &&
         (type <= LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
}


bool HIsObjectAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    *block = ConstantIsObject(HConstant::cast(value()), isolate())
        ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


bool HIsStringAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    *block = HConstant::cast(value())->HasStringValue()
        ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


bool HIsUndetectableAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    *block = HConstant::cast(value())->IsUndetectable()
        ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


bool HHasInstanceTypeAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    InstanceType type = HConstant::cast(value())->GetInstanceType();
    *block = (from_ <= type) && (type <= to_)
        ? FirstSuccessor() : SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


void HCompareHoleAndBranch::InferRepresentation(
    HInferRepresentationPhase* h_infer) {
  ChangeRepresentation(value()->representation());
}


bool HCompareMinusZeroAndBranch::KnownSuccessorBlock(HBasicBlock** block) {
  if (FLAG_fold_constants && value()->IsConstant()) {
    HConstant* constant = HConstant::cast(value());
    if (constant->HasDoubleValue()) {
      *block = IsMinusZero(constant->DoubleValue())
          ? FirstSuccessor() : SecondSuccessor();
      return true;
    }
  }
  if (value()->representation().IsSmiOrInteger32()) {
    // A Smi or Integer32 cannot contain minus zero.
    *block = SecondSuccessor();
    return true;
  }
  *block = NULL;
  return false;
}


void HCompareMinusZeroAndBranch::InferRepresentation(
    HInferRepresentationPhase* h_infer) {
  ChangeRepresentation(value()->representation());
}



void HGoto::PrintDataTo(StringStream* stream) {
  stream->Add("B%d", SuccessorAt(0)->block_id());
}


void HCompareNumericAndBranch::InferRepresentation(
    HInferRepresentationPhase* h_infer) {
  Representation left_rep = left()->representation();
  Representation right_rep = right()->representation();
  Representation observed_left = observed_input_representation(0);
  Representation observed_right = observed_input_representation(1);

  Representation rep = Representation::None();
  rep = rep.generalize(observed_left);
  rep = rep.generalize(observed_right);
  if (rep.IsNone() || rep.IsSmiOrInteger32()) {
    if (!left_rep.IsTagged()) rep = rep.generalize(left_rep);
    if (!right_rep.IsTagged()) rep = rep.generalize(right_rep);
  } else {
    rep = Representation::Double();
  }

  if (rep.IsDouble()) {
    // According to the ES5 spec (11.9.3, 11.8.5), Equality comparisons (==, ===
    // and !=) have special handling of undefined, e.g. undefined == undefined
    // is 'true'. Relational comparisons have a different semantic, first
    // calling ToPrimitive() on their arguments.  The standard Crankshaft
    // tagged-to-double conversion to ensure the HCompareNumericAndBranch's
    // inputs are doubles caused 'undefined' to be converted to NaN. That's
    // compatible out-of-the box with ordered relational comparisons (<, >, <=,
    // >=). However, for equality comparisons (and for 'in' and 'instanceof'),
    // it is not consistent with the spec. For example, it would cause undefined
    // == undefined (should be true) to be evaluated as NaN == NaN
    // (false). Therefore, any comparisons other than ordered relational
    // comparisons must cause a deopt when one of their arguments is undefined.
    // See also v8:1434
    if (Token::IsOrderedRelationalCompareOp(token_)) {
      SetFlag(kAllowUndefinedAsNaN);
    }
  }
  ChangeRepresentation(rep);
}


void HParameter::PrintDataTo(StringStream* stream) {
  stream->Add("%u", index());
}


void HLoadNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  access_.PrintTo(stream);

  if (HasDependency()) {
    stream->Add(" ");
    dependency()->PrintNameTo(stream);
  }
}


HCheckMaps* HCheckMaps::New(Zone* zone,
                            HValue* context,
                            HValue* value,
                            Handle<Map> map,
                            CompilationInfo* info,
                            HValue* typecheck) {
  HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, typecheck);
  check_map->Add(map, zone);
  if (map->CanOmitMapChecks() &&
      value->IsConstant() &&
      HConstant::cast(value)->HasMap(map)) {
    // TODO(titzer): collect dependent map checks into a list.
    check_map->omit_ = true;
    if (map->CanTransition()) {
      map->AddDependentCompilationInfo(
          DependentCode::kPrototypeCheckGroup, info);
    }
  }
  return check_map;
}


void HLoadNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  stream->Add(String::cast(*name())->ToCString().get());
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
    stream->Add(" + %d]", index_offset());
  } else {
    stream->Add("]");
  }

  if (HasDependency()) {
    stream->Add(" ");
    dependency()->PrintNameTo(stream);
  }

  if (RequiresHoleCheck()) {
    stream->Add(" check_hole");
  }
}


bool HLoadKeyed::UsesMustHandleHole() const {
  if (IsFastPackedElementsKind(elements_kind())) {
    return false;
  }

  if (IsExternalArrayElementsKind(elements_kind())) {
    return false;
  }

  if (hole_mode() == ALLOW_RETURN_HOLE) {
    if (IsFastDoubleElementsKind(elements_kind())) {
      return AllUsesCanTreatHoleAsNaN();
    }
    return true;
  }

  if (IsFastDoubleElementsKind(elements_kind())) {
    return false;
  }

  // Holes are only returned as tagged values.
  if (!representation().IsTagged()) {
    return false;
  }

  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (!use->IsChange()) return false;
  }

  return true;
}


bool HLoadKeyed::AllUsesCanTreatHoleAsNaN() const {
  return IsFastDoubleElementsKind(elements_kind()) &&
      CheckUsesForFlag(HValue::kAllowUndefinedAsNaN);
}


bool HLoadKeyed::RequiresHoleCheck() const {
  if (IsFastPackedElementsKind(elements_kind())) {
    return false;
  }

  if (IsExternalArrayElementsKind(elements_kind())) {
    return false;
  }

  return !UsesMustHandleHole();
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
            HCheckMapValue::New(block()->graph()->zone(),
                                block()->graph()->GetInvalidContext(),
                                object(),
                                names_cache->map());
        HInstruction* index = HLoadKeyed::New(
            block()->graph()->zone(),
            block()->graph()->GetInvalidContext(),
            index_cache,
            key_load->key(),
            key_load->key(),
            key_load->elements_kind());
        map_check->InsertBefore(this);
        index->InsertBefore(this);
        return Prepend(new(block()->zone()) HLoadFieldByIndex(
            object(), index));
      }
    }
  }

  return this;
}


void HStoreNamedGeneric::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  stream->Add(".");
  ASSERT(name()->IsString());
  stream->Add(String::cast(*name())->ToCString().get());
  stream->Add(" = ");
  value()->PrintNameTo(stream);
}


void HStoreNamedField::PrintDataTo(StringStream* stream) {
  object()->PrintNameTo(stream);
  access_.PrintTo(stream);
  stream->Add(" = ");
  value()->PrintNameTo(stream);
  if (NeedsWriteBarrier()) {
    stream->Add(" (write-barrier)");
  }
  if (has_transition()) {
    stream->Add(" (transition map %p)", *transition_map());
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
  ElementsKind from_kind = original_map().handle()->elements_kind();
  ElementsKind to_kind = transitioned_map().handle()->elements_kind();
  stream->Add(" %p [%s] -> %p [%s]",
              *original_map().handle(),
              ElementsAccessor::ForKind(from_kind)->name(),
              *transitioned_map().handle(),
              ElementsAccessor::ForKind(to_kind)->name());
  if (IsSimpleMapChangeTransition(from_kind, to_kind)) stream->Add(" (simple)");
}


void HLoadGlobalCell::PrintDataTo(StringStream* stream) {
  stream->Add("[%p]", *cell().handle());
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


void HInnerAllocatedObject::PrintDataTo(StringStream* stream) {
  base_object()->PrintNameTo(stream);
  stream->Add(" offset ");
  offset()->PrintTo(stream);
}


void HStoreGlobalCell::PrintDataTo(StringStream* stream) {
  stream->Add("[%p] = ", *cell().handle());
  value()->PrintNameTo(stream);
  if (!details_.IsDontDelete()) stream->Add(" (deleteable)");
  if (details_.IsReadOnly()) stream->Add(" (read-only)");
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


HType HPhi::CalculateInferredType() {
  if (OperandCount() == 0) return HType::Tagged();
  HType result = OperandAt(0)->type();
  for (int i = 1; i < OperandCount(); ++i) {
    HType current = OperandAt(i)->type();
    result = result.Combine(current);
  }
  return result;
}


HType HChange::CalculateInferredType() {
  if (from().IsDouble() && to().IsTagged()) return HType::HeapNumber();
  return type();
}


Representation HUnaryMathOperation::RepresentationFromInputs() {
  Representation rep = representation();
  // If any of the actual input representation is more general than what we
  // have so far but not Tagged, use that representation instead.
  Representation input_rep = value()->representation();
  if (!input_rep.IsTagged()) {
    rep = rep.generalize(input_rep);
  }
  return rep;
}


bool HAllocate::HandleSideEffectDominator(GVNFlag side_effect,
                                          HValue* dominator) {
  ASSERT(side_effect == kNewSpacePromotion);
  Zone* zone = block()->zone();
  if (!FLAG_use_allocation_folding) return false;

  // Try to fold allocations together with their dominating allocations.
  if (!dominator->IsAllocate()) {
    if (FLAG_trace_allocation_folding) {
      PrintF("#%d (%s) cannot fold into #%d (%s)\n",
          id(), Mnemonic(), dominator->id(), dominator->Mnemonic());
    }
    return false;
  }

  // Check whether we are folding within the same block for local folding.
  if (FLAG_use_local_allocation_folding && dominator->block() != block()) {
    if (FLAG_trace_allocation_folding) {
      PrintF("#%d (%s) cannot fold into #%d (%s), crosses basic blocks\n",
          id(), Mnemonic(), dominator->id(), dominator->Mnemonic());
    }
    return false;
  }

  HAllocate* dominator_allocate = HAllocate::cast(dominator);
  HValue* dominator_size = dominator_allocate->size();
  HValue* current_size = size();

  // TODO(hpayer): Add support for non-constant allocation in dominator.
  if (!current_size->IsInteger32Constant() ||
      !dominator_size->IsInteger32Constant()) {
    if (FLAG_trace_allocation_folding) {
      PrintF("#%d (%s) cannot fold into #%d (%s), dynamic allocation size\n",
          id(), Mnemonic(), dominator->id(), dominator->Mnemonic());
    }
    return false;
  }

  dominator_allocate = GetFoldableDominator(dominator_allocate);
  if (dominator_allocate == NULL) {
    return false;
  }

  ASSERT((IsNewSpaceAllocation() &&
         dominator_allocate->IsNewSpaceAllocation()) ||
         (IsOldDataSpaceAllocation() &&
         dominator_allocate->IsOldDataSpaceAllocation()) ||
         (IsOldPointerSpaceAllocation() &&
         dominator_allocate->IsOldPointerSpaceAllocation()));

  // First update the size of the dominator allocate instruction.
  dominator_size = dominator_allocate->size();
  int32_t original_object_size =
      HConstant::cast(dominator_size)->GetInteger32Constant();
  int32_t dominator_size_constant = original_object_size;
  int32_t current_size_constant =
      HConstant::cast(current_size)->GetInteger32Constant();
  int32_t new_dominator_size = dominator_size_constant + current_size_constant;

  if (MustAllocateDoubleAligned()) {
    if (!dominator_allocate->MustAllocateDoubleAligned()) {
      dominator_allocate->MakeDoubleAligned();
    }
    if ((dominator_size_constant & kDoubleAlignmentMask) != 0) {
      dominator_size_constant += kDoubleSize / 2;
      new_dominator_size += kDoubleSize / 2;
    }
  }

  // Since we clear the first word after folded memory, we cannot use the
  // whole Page::kMaxRegularHeapObjectSize memory.
  if (new_dominator_size > Page::kMaxRegularHeapObjectSize - kPointerSize) {
    if (FLAG_trace_allocation_folding) {
      PrintF("#%d (%s) cannot fold into #%d (%s) due to size: %d\n",
          id(), Mnemonic(), dominator_allocate->id(),
          dominator_allocate->Mnemonic(), new_dominator_size);
    }
    return false;
  }

  HInstruction* new_dominator_size_constant = HConstant::CreateAndInsertBefore(
      zone,
      context(),
      new_dominator_size,
      Representation::None(),
      dominator_allocate);
  dominator_allocate->UpdateSize(new_dominator_size_constant);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap && dominator_allocate->IsNewSpaceAllocation()) {
    dominator_allocate->MakePrefillWithFiller();
  } else {
    // TODO(hpayer): This is a short-term hack to make allocation mementos
    // work again in new space.
    dominator_allocate->ClearNextMapWord(original_object_size);
  }
#else
  // TODO(hpayer): This is a short-term hack to make allocation mementos
  // work again in new space.
  dominator_allocate->ClearNextMapWord(original_object_size);
#endif

  dominator_allocate->UpdateClearNextMapWord(MustClearNextMapWord());

  // After that replace the dominated allocate instruction.
  HInstruction* inner_offset = HConstant::CreateAndInsertBefore(
      zone,
      context(),
      dominator_size_constant,
      Representation::None(),
      this);

  HInstruction* dominated_allocate_instr =
      HInnerAllocatedObject::New(zone,
                                 context(),
                                 dominator_allocate,
                                 inner_offset,
                                 type());
  dominated_allocate_instr->InsertBefore(this);
  DeleteAndReplaceWith(dominated_allocate_instr);
  if (FLAG_trace_allocation_folding) {
    PrintF("#%d (%s) folded into #%d (%s)\n",
        id(), Mnemonic(), dominator_allocate->id(),
        dominator_allocate->Mnemonic());
  }
  return true;
}


HAllocate* HAllocate::GetFoldableDominator(HAllocate* dominator) {
  if (!IsFoldable(dominator)) {
    // We cannot hoist old space allocations over new space allocations.
    if (IsNewSpaceAllocation() || dominator->IsNewSpaceAllocation()) {
      if (FLAG_trace_allocation_folding) {
        PrintF("#%d (%s) cannot fold into #%d (%s), new space hoisting\n",
            id(), Mnemonic(), dominator->id(), dominator->Mnemonic());
      }
      return NULL;
    }

    HAllocate* dominator_dominator = dominator->dominating_allocate_;

    // We can hoist old data space allocations over an old pointer space
    // allocation and vice versa. For that we have to check the dominator
    // of the dominator allocate instruction.
    if (dominator_dominator == NULL) {
      dominating_allocate_ = dominator;
      if (FLAG_trace_allocation_folding) {
        PrintF("#%d (%s) cannot fold into #%d (%s), different spaces\n",
            id(), Mnemonic(), dominator->id(), dominator->Mnemonic());
      }
      return NULL;
    }

    // We can just fold old space allocations that are in the same basic block,
    // since it is not guaranteed that we fill up the whole allocated old
    // space memory.
    // TODO(hpayer): Remove this limitation and add filler maps for each each
    // allocation as soon as we have store elimination.
    if (block()->block_id() != dominator_dominator->block()->block_id()) {
      if (FLAG_trace_allocation_folding) {
        PrintF("#%d (%s) cannot fold into #%d (%s), different basic blocks\n",
            id(), Mnemonic(), dominator_dominator->id(),
            dominator_dominator->Mnemonic());
      }
      return NULL;
    }

    ASSERT((IsOldDataSpaceAllocation() &&
           dominator_dominator->IsOldDataSpaceAllocation()) ||
           (IsOldPointerSpaceAllocation() &&
           dominator_dominator->IsOldPointerSpaceAllocation()));

    int32_t current_size = HConstant::cast(size())->GetInteger32Constant();
    HStoreNamedField* dominator_free_space_size =
        dominator->filler_free_space_size_;
    if (dominator_free_space_size != NULL) {
      // We already hoisted one old space allocation, i.e., we already installed
      // a filler map. Hence, we just have to update the free space size.
      dominator->UpdateFreeSpaceFiller(current_size);
    } else {
      // This is the first old space allocation that gets hoisted. We have to
      // install a filler map since the follwing allocation may cause a GC.
      dominator->CreateFreeSpaceFiller(current_size);
    }

    // We can hoist the old space allocation over the actual dominator.
    return dominator_dominator;
  }
  return dominator;
}


void HAllocate::UpdateFreeSpaceFiller(int32_t free_space_size) {
  ASSERT(filler_free_space_size_ != NULL);
  Zone* zone = block()->zone();
  // We must explicitly force Smi representation here because on x64 we
  // would otherwise automatically choose int32, but the actual store
  // requires a Smi-tagged value.
  HConstant* new_free_space_size = HConstant::CreateAndInsertBefore(
      zone,
      context(),
      filler_free_space_size_->value()->GetInteger32Constant() +
          free_space_size,
      Representation::Smi(),
      filler_free_space_size_);
  filler_free_space_size_->UpdateValue(new_free_space_size);
}


void HAllocate::CreateFreeSpaceFiller(int32_t free_space_size) {
  ASSERT(filler_free_space_size_ == NULL);
  Zone* zone = block()->zone();
  HInstruction* free_space_instr =
      HInnerAllocatedObject::New(zone, context(), dominating_allocate_,
      dominating_allocate_->size(), type());
  free_space_instr->InsertBefore(this);
  HConstant* filler_map = HConstant::New(
      zone,
      context(),
      isolate()->factory()->free_space_map());
  filler_map->FinalizeUniqueness();  // TODO(titzer): should be init'd a'ready
  filler_map->InsertAfter(free_space_instr);
  HInstruction* store_map = HStoreNamedField::New(zone, context(),
      free_space_instr, HObjectAccess::ForMap(), filler_map);
  store_map->SetFlag(HValue::kHasNoObservableSideEffects);
  store_map->InsertAfter(filler_map);

  // We must explicitly force Smi representation here because on x64 we
  // would otherwise automatically choose int32, but the actual store
  // requires a Smi-tagged value.
  HConstant* filler_size = HConstant::CreateAndInsertAfter(
      zone, context(), free_space_size, Representation::Smi(), store_map);
  // Must force Smi representation for x64 (see comment above).
  HObjectAccess access =
      HObjectAccess::ForMapAndOffset(isolate()->factory()->free_space_map(),
                                     FreeSpace::kSizeOffset,
                                     Representation::Smi());
  HStoreNamedField* store_size = HStoreNamedField::New(zone, context(),
      free_space_instr, access, filler_size);
  store_size->SetFlag(HValue::kHasNoObservableSideEffects);
  store_size->InsertAfter(filler_size);
  filler_free_space_size_ = store_size;
}


void HAllocate::ClearNextMapWord(int offset) {
  if (MustClearNextMapWord()) {
    Zone* zone = block()->zone();
    HObjectAccess access =
        HObjectAccess::ForObservableJSObjectOffset(offset);
    HStoreNamedField* clear_next_map =
        HStoreNamedField::New(zone, context(), this, access,
            block()->graph()->GetConstant0());
    clear_next_map->ClearAllSideEffects();
    clear_next_map->InsertAfter(this);
  }
}


void HAllocate::PrintDataTo(StringStream* stream) {
  size()->PrintNameTo(stream);
  stream->Add(" (");
  if (IsNewSpaceAllocation()) stream->Add("N");
  if (IsOldPointerSpaceAllocation()) stream->Add("P");
  if (IsOldDataSpaceAllocation()) stream->Add("D");
  if (MustAllocateDoubleAligned()) stream->Add("A");
  if (MustPrefillWithFiller()) stream->Add("F");
  stream->Add(")");
}


bool HStoreKeyed::NeedsCanonicalization() {
  // If value is an integer or smi or comes from the result of a keyed load or
  // constant then it is either be a non-hole value or in the case of a constant
  // the hole is only being stored explicitly: no need for canonicalization.
  //
  // The exception to that is keyed loads from external float or double arrays:
  // these can load arbitrary representation of NaN.

  if (value()->IsConstant()) {
    return false;
  }

  if (value()->IsLoadKeyed()) {
    return IsExternalFloatOrDoubleElementsKind(
        HLoadKeyed::cast(value())->elements_kind());
  }

  if (value()->IsChange()) {
    if (HChange::cast(value())->from().IsSmiOrInteger32()) {
      return false;
    }
    if (HChange::cast(value())->value()->type().IsSmi()) {
      return false;
    }
  }
  return true;
}


#define H_CONSTANT_INT(val)                                                    \
HConstant::New(zone, context, static_cast<int32_t>(val))
#define H_CONSTANT_DOUBLE(val)                                                 \
HConstant::New(zone, context, static_cast<double>(val))

#define DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HInstr, op)                       \
HInstruction* HInstr::New(                                                     \
    Zone* zone, HValue* context, HValue* left, HValue* right) {                \
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {      \
    HConstant* c_left = HConstant::cast(left);                                 \
    HConstant* c_right = HConstant::cast(right);                               \
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {             \
      double double_res = c_left->DoubleValue() op c_right->DoubleValue();     \
      if (IsInt32Double(double_res)) {                                         \
        return H_CONSTANT_INT(double_res);                                     \
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


HInstruction* HStringAdd::New(Zone* zone,
                              HValue* context,
                              HValue* left,
                              HValue* right,
                              PretenureFlag pretenure_flag,
                              StringAddFlags flags,
                              Handle<AllocationSite> allocation_site) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_right = HConstant::cast(right);
    HConstant* c_left = HConstant::cast(left);
    if (c_left->HasStringValue() && c_right->HasStringValue()) {
      Handle<String> left_string = c_left->StringValue();
      Handle<String> right_string = c_right->StringValue();
      // Prevent possible exception by invalid string length.
      if (left_string->length() + right_string->length() < String::kMaxLength) {
        Handle<String> concat = zone->isolate()->factory()->NewFlatConcatString(
            c_left->StringValue(), c_right->StringValue());
        ASSERT(!concat.is_null());
        return HConstant::New(zone, context, concat);
      }
    }
  }
  return new(zone) HStringAdd(
      context, left, right, pretenure_flag, flags, allocation_site);
}


void HStringAdd::PrintDataTo(StringStream* stream) {
  if ((flags() & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_BOTH) {
    stream->Add("_CheckBoth");
  } else if ((flags() & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_LEFT) {
    stream->Add("_CheckLeft");
  } else if ((flags() & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_RIGHT) {
    stream->Add("_CheckRight");
  }
  HBinaryOperation::PrintDataTo(stream);
  stream->Add(" (");
  if (pretenure_flag() == NOT_TENURED) stream->Add("N");
  else if (pretenure_flag() == TENURED) stream->Add("D");
  stream->Add(")");
}


HInstruction* HStringCharFromCode::New(
    Zone* zone, HValue* context, HValue* char_code) {
  if (FLAG_fold_constants && char_code->IsConstant()) {
    HConstant* c_code = HConstant::cast(char_code);
    Isolate* isolate = zone->isolate();
    if (c_code->HasNumberValue()) {
      if (std::isfinite(c_code->DoubleValue())) {
        uint32_t code = c_code->NumberValueAsInteger32() & 0xffff;
        return HConstant::New(zone, context,
            LookupSingleCharacterStringFromCode(isolate, code));
      }
      return HConstant::New(zone, context, isolate->factory()->empty_string());
    }
  }
  return new(zone) HStringCharFromCode(context, char_code);
}


HInstruction* HUnaryMathOperation::New(
    Zone* zone, HValue* context, HValue* value, BuiltinFunctionId op) {
  do {
    if (!FLAG_fold_constants) break;
    if (!value->IsConstant()) break;
    HConstant* constant = HConstant::cast(value);
    if (!constant->HasNumberValue()) break;
    double d = constant->DoubleValue();
    if (std::isnan(d)) {  // NaN poisons everything.
      return H_CONSTANT_DOUBLE(OS::nan_value());
    }
    if (std::isinf(d)) {  // +Infinity and -Infinity.
      switch (op) {
        case kMathExp:
          return H_CONSTANT_DOUBLE((d > 0.0) ? d : 0.0);
        case kMathLog:
        case kMathSqrt:
          return H_CONSTANT_DOUBLE((d > 0.0) ? d : OS::nan_value());
        case kMathPowHalf:
        case kMathAbs:
          return H_CONSTANT_DOUBLE((d > 0.0) ? d : -d);
        case kMathRound:
        case kMathFloor:
          return H_CONSTANT_DOUBLE(d);
        case kMathClz32:
          return H_CONSTANT_INT(32);
        default:
          UNREACHABLE();
          break;
      }
    }
    switch (op) {
      case kMathExp:
        return H_CONSTANT_DOUBLE(fast_exp(d));
      case kMathLog:
        return H_CONSTANT_DOUBLE(std::log(d));
      case kMathSqrt:
        return H_CONSTANT_DOUBLE(fast_sqrt(d));
      case kMathPowHalf:
        return H_CONSTANT_DOUBLE(power_double_double(d, 0.5));
      case kMathAbs:
        return H_CONSTANT_DOUBLE((d >= 0.0) ? d + 0.0 : -d);
      case kMathRound:
        // -0.5 .. -0.0 round to -0.0.
        if ((d >= -0.5 && Double(d).Sign() < 0)) return H_CONSTANT_DOUBLE(-0.0);
        // Doubles are represented as Significant * 2 ^ Exponent. If the
        // Exponent is not negative, the double value is already an integer.
        if (Double(d).Exponent() >= 0) return H_CONSTANT_DOUBLE(d);
        return H_CONSTANT_DOUBLE(std::floor(d + 0.5));
      case kMathFloor:
        return H_CONSTANT_DOUBLE(std::floor(d));
      case kMathClz32: {
        uint32_t i = DoubleToUint32(d);
        return H_CONSTANT_INT(
            (i == 0) ? 32 : CompilerIntrinsics::CountLeadingZeros(i));
      }
      default:
        UNREACHABLE();
        break;
    }
  } while (false);
  return new(zone) HUnaryMathOperation(context, value, op);
}


HInstruction* HPower::New(Zone* zone,
                          HValue* context,
                          HValue* left,
                          HValue* right) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if (c_left->HasNumberValue() && c_right->HasNumberValue()) {
      double result = power_helper(c_left->DoubleValue(),
                                   c_right->DoubleValue());
      return H_CONSTANT_DOUBLE(std::isnan(result) ?  OS::nan_value() : result);
    }
  }
  return new(zone) HPower(left, right);
}


HInstruction* HMathMinMax::New(
    Zone* zone, HValue* context, HValue* left, HValue* right, Operation op) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if (c_left->HasNumberValue() && c_right->HasNumberValue()) {
      double d_left = c_left->DoubleValue();
      double d_right = c_right->DoubleValue();
      if (op == kMathMin) {
        if (d_left > d_right) return H_CONSTANT_DOUBLE(d_right);
        if (d_left < d_right) return H_CONSTANT_DOUBLE(d_left);
        if (d_left == d_right) {
          // Handle +0 and -0.
          return H_CONSTANT_DOUBLE((Double(d_left).Sign() == -1) ? d_left
                                                                 : d_right);
        }
      } else {
        if (d_left < d_right) return H_CONSTANT_DOUBLE(d_right);
        if (d_left > d_right) return H_CONSTANT_DOUBLE(d_left);
        if (d_left == d_right) {
          // Handle +0 and -0.
          return H_CONSTANT_DOUBLE((Double(d_left).Sign() == -1) ? d_right
                                                                 : d_left);
        }
      }
      // All comparisons failed, must be NaN.
      return H_CONSTANT_DOUBLE(OS::nan_value());
    }
  }
  return new(zone) HMathMinMax(context, left, right, op);
}


HInstruction* HMod::New(Zone* zone,
                        HValue* context,
                        HValue* left,
                        HValue* right) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if (c_left->HasInteger32Value() && c_right->HasInteger32Value()) {
      int32_t dividend = c_left->Integer32Value();
      int32_t divisor = c_right->Integer32Value();
      if (dividend == kMinInt && divisor == -1) {
        return H_CONSTANT_DOUBLE(-0.0);
      }
      if (divisor != 0) {
        int32_t res = dividend % divisor;
        if ((res == 0) && (dividend < 0)) {
          return H_CONSTANT_DOUBLE(-0.0);
        }
        return H_CONSTANT_INT(res);
      }
    }
  }
  return new(zone) HMod(context, left, right);
}


HInstruction* HDiv::New(
    Zone* zone, HValue* context, HValue* left, HValue* right) {
  // If left and right are constant values, try to return a constant value.
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {
      if (c_right->DoubleValue() != 0) {
        double double_res = c_left->DoubleValue() / c_right->DoubleValue();
        if (IsInt32Double(double_res)) {
          return H_CONSTANT_INT(double_res);
        }
        return H_CONSTANT_DOUBLE(double_res);
      } else {
        int sign = Double(c_left->DoubleValue()).Sign() *
                   Double(c_right->DoubleValue()).Sign();  // Right could be -0.
        return H_CONSTANT_DOUBLE(sign * V8_INFINITY);
      }
    }
  }
  return new(zone) HDiv(context, left, right);
}


HInstruction* HBitwise::New(
    Zone* zone, HValue* context, Token::Value op, HValue* left, HValue* right) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
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
      return H_CONSTANT_INT(result);
    }
  }
  return new(zone) HBitwise(context, op, left, right);
}


#define DEFINE_NEW_H_BITWISE_INSTR(HInstr, result)                             \
HInstruction* HInstr::New(                                                     \
    Zone* zone, HValue* context, HValue* left, HValue* right) {                \
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {      \
    HConstant* c_left = HConstant::cast(left);                                 \
    HConstant* c_right = HConstant::cast(right);                               \
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {             \
      return H_CONSTANT_INT(result);                                           \
    }                                                                          \
  }                                                                            \
  return new(zone) HInstr(context, left, right);                               \
}


DEFINE_NEW_H_BITWISE_INSTR(HSar,
c_left->NumberValueAsInteger32() >> (c_right->NumberValueAsInteger32() & 0x1f))
DEFINE_NEW_H_BITWISE_INSTR(HShl,
c_left->NumberValueAsInteger32() << (c_right->NumberValueAsInteger32() & 0x1f))

#undef DEFINE_NEW_H_BITWISE_INSTR


HInstruction* HShr::New(
    Zone* zone, HValue* context, HValue* left, HValue* right) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_left = HConstant::cast(left);
    HConstant* c_right = HConstant::cast(right);
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {
      int32_t left_val = c_left->NumberValueAsInteger32();
      int32_t right_val = c_right->NumberValueAsInteger32() & 0x1f;
      if ((right_val == 0) && (left_val < 0)) {
        return H_CONSTANT_DOUBLE(static_cast<uint32_t>(left_val));
      }
      return H_CONSTANT_INT(static_cast<uint32_t>(left_val) >> right_val);
    }
  }
  return new(zone) HShr(context, left, right);
}


HInstruction* HSeqStringGetChar::New(Zone* zone,
                                     HValue* context,
                                     String::Encoding encoding,
                                     HValue* string,
                                     HValue* index) {
  if (FLAG_fold_constants && string->IsConstant() && index->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    HConstant* c_index = HConstant::cast(index);
    if (c_string->HasStringValue() && c_index->HasInteger32Value()) {
      Handle<String> s = c_string->StringValue();
      int32_t i = c_index->Integer32Value();
      ASSERT_LE(0, i);
      ASSERT_LT(i, s->length());
      return H_CONSTANT_INT(s->Get(i));
    }
  }
  return new(zone) HSeqStringGetChar(encoding, string, index);
}


#undef H_CONSTANT_INT
#undef H_CONSTANT_DOUBLE


void HBitwise::PrintDataTo(StringStream* stream) {
  stream->Add(Token::Name(op_));
  stream->Add(" ");
  HBitwiseBinaryOperation::PrintDataTo(stream);
}


void HPhi::SimplifyConstantInputs() {
  // Convert constant inputs to integers when all uses are truncating.
  // This must happen before representation inference takes place.
  if (!CheckUsesForFlag(kTruncatingToInt32)) return;
  for (int i = 0; i < OperandCount(); ++i) {
    if (!OperandAt(i)->IsConstant()) return;
  }
  HGraph* graph = block()->graph();
  for (int i = 0; i < OperandCount(); ++i) {
    HConstant* operand = HConstant::cast(OperandAt(i));
    if (operand->HasInteger32Value()) {
      continue;
    } else if (operand->HasDoubleValue()) {
      HConstant* integer_input =
          HConstant::New(graph->zone(), graph->GetInvalidContext(),
                         DoubleToInt32(operand->DoubleValue()));
      integer_input->InsertAfter(operand);
      SetOperandAt(i, integer_input);
    } else if (operand->HasBooleanValue()) {
      SetOperandAt(i, operand->BooleanValue() ? graph->GetConstant1()
                                              : graph->GetConstant0());
    } else if (operand->ImmortalImmovable()) {
      SetOperandAt(i, graph->GetConstant0());
    }
  }
  // Overwrite observed input representations because they are likely Tagged.
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (use->IsBinaryOperation()) {
      HBinaryOperation::cast(use)->set_observed_input_representation(
          it.index(), Representation::Smi());
    }
  }
}


void HPhi::InferRepresentation(HInferRepresentationPhase* h_infer) {
  ASSERT(CheckFlag(kFlexibleRepresentation));
  Representation new_rep = RepresentationFromInputs();
  UpdateRepresentation(new_rep, h_infer, "inputs");
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
  new_rep = RepresentationFromUseRequirements();
  UpdateRepresentation(new_rep, h_infer, "use requirements");
}


Representation HPhi::RepresentationFromInputs() {
  Representation r = Representation::None();
  for (int i = 0; i < OperandCount(); ++i) {
    r = r.generalize(OperandAt(i)->KnownOptimalRepresentation());
  }
  return r;
}


// Returns a representation if all uses agree on the same representation.
// Integer32 is also returned when some uses are Smi but others are Integer32.
Representation HValue::RepresentationFromUseRequirements() {
  Representation rep = Representation::None();
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    // Ignore the use requirement from never run code
    if (it.value()->block()->IsUnreachable()) continue;

    // We check for observed_input_representation elsewhere.
    Representation use_rep =
        it.value()->RequiredInputRepresentation(it.index());
    if (rep.IsNone()) {
      rep = use_rep;
      continue;
    }
    if (use_rep.IsNone() || rep.Equals(use_rep)) continue;
    if (rep.generalize(use_rep).IsInteger32()) {
      rep = Representation::Integer32();
      continue;
    }
    return Representation::None();
  }
  return rep;
}


bool HValue::HasNonSmiUse() {
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    // We check for observed_input_representation elsewhere.
    Representation use_rep =
        it.value()->RequiredInputRepresentation(it.index());
    if (!use_rep.IsNone() &&
        !use_rep.IsSmi() &&
        !use_rep.IsTagged()) {
      return true;
    }
  }
  return false;
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


void HCheckHeapObject::Verify() {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}


void HCheckValue::Verify() {
  HInstruction::Verify();
  ASSERT(HasNoUses());
}

#endif


HObjectAccess HObjectAccess::ForFixedArrayHeader(int offset) {
  ASSERT(offset >= 0);
  ASSERT(offset < FixedArray::kHeaderSize);
  if (offset == FixedArray::kLengthOffset) return ForFixedArrayLength();
  return HObjectAccess(kInobject, offset);
}


HObjectAccess HObjectAccess::ForMapAndOffset(Handle<Map> map, int offset,
    Representation representation) {
  ASSERT(offset >= 0);
  Portion portion = kInobject;

  if (offset == JSObject::kElementsOffset) {
    portion = kElementsPointer;
  } else if (offset == JSObject::kMapOffset) {
    portion = kMaps;
  }
  bool existing_inobject_property = true;
  if (!map.is_null()) {
    existing_inobject_property = (offset <
        map->instance_size() - map->unused_property_fields() * kPointerSize);
  }
  return HObjectAccess(portion, offset, representation, Handle<String>::null(),
                       false, existing_inobject_property);
}


HObjectAccess HObjectAccess::ForAllocationSiteOffset(int offset) {
  switch (offset) {
    case AllocationSite::kTransitionInfoOffset:
      return HObjectAccess(kInobject, offset, Representation::Tagged());
    case AllocationSite::kNestedSiteOffset:
      return HObjectAccess(kInobject, offset, Representation::Tagged());
    case AllocationSite::kPretenureDataOffset:
      return HObjectAccess(kInobject, offset, Representation::Smi());
    case AllocationSite::kPretenureCreateCountOffset:
      return HObjectAccess(kInobject, offset, Representation::Smi());
    case AllocationSite::kDependentCodeOffset:
      return HObjectAccess(kInobject, offset, Representation::Tagged());
    case AllocationSite::kWeakNextOffset:
      return HObjectAccess(kInobject, offset, Representation::Tagged());
    default:
      UNREACHABLE();
  }
  return HObjectAccess(kInobject, offset);
}


HObjectAccess HObjectAccess::ForContextSlot(int index) {
  ASSERT(index >= 0);
  Portion portion = kInobject;
  int offset = Context::kHeaderSize + index * kPointerSize;
  ASSERT_EQ(offset, Context::SlotOffset(index) + kHeapObjectTag);
  return HObjectAccess(portion, offset, Representation::Tagged());
}


HObjectAccess HObjectAccess::ForJSArrayOffset(int offset) {
  ASSERT(offset >= 0);
  Portion portion = kInobject;

  if (offset == JSObject::kElementsOffset) {
    portion = kElementsPointer;
  } else if (offset == JSArray::kLengthOffset) {
    portion = kArrayLengths;
  } else if (offset == JSObject::kMapOffset) {
    portion = kMaps;
  }
  return HObjectAccess(portion, offset);
}


HObjectAccess HObjectAccess::ForBackingStoreOffset(int offset,
    Representation representation) {
  ASSERT(offset >= 0);
  return HObjectAccess(kBackingStore, offset, representation,
                       Handle<String>::null(), false, false);
}


HObjectAccess HObjectAccess::ForField(Handle<Map> map,
                                      LookupResult* lookup,
                                      Handle<String> name) {
  ASSERT(lookup->IsField() || lookup->IsTransitionToField());
  int index;
  Representation representation;
  if (lookup->IsField()) {
    index = lookup->GetLocalFieldIndexFromMap(*map);
    representation = lookup->representation();
  } else {
    Map* transition = lookup->GetTransitionTarget();
    int descriptor = transition->LastAdded();
    index = transition->instance_descriptors()->GetFieldIndex(descriptor) -
        map->inobject_properties();
    PropertyDetails details =
        transition->instance_descriptors()->GetDetails(descriptor);
    representation = details.representation();
  }
  if (index < 0) {
    // Negative property indices are in-object properties, indexed
    // from the end of the fixed part of the object.
    int offset = (index * kPointerSize) + map->instance_size();
    return HObjectAccess(kInobject, offset, representation, name, false, true);
  } else {
    // Non-negative property indices are in the properties array.
    int offset = (index * kPointerSize) + FixedArray::kHeaderSize;
    return HObjectAccess(kBackingStore, offset, representation, name,
                         false, false);
  }
}


HObjectAccess HObjectAccess::ForCellPayload(Isolate* isolate) {
  return HObjectAccess(
      kInobject, Cell::kValueOffset, Representation::Tagged(),
      Handle<String>(isolate->heap()->cell_value_string()));
}


void HObjectAccess::SetGVNFlags(HValue *instr, PropertyAccessType access_type) {
  // set the appropriate GVN flags for a given load or store instruction
  if (access_type == STORE) {
    // track dominating allocations in order to eliminate write barriers
    instr->SetDependsOnFlag(::v8::internal::kNewSpacePromotion);
    instr->SetFlag(HValue::kTrackSideEffectDominators);
  } else {
    // try to GVN loads, but don't hoist above map changes
    instr->SetFlag(HValue::kUseGVN);
    instr->SetDependsOnFlag(::v8::internal::kMaps);
  }

  switch (portion()) {
    case kArrayLengths:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kArrayLengths);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kArrayLengths);
      }
      break;
    case kStringLengths:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kStringLengths);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kStringLengths);
      }
      break;
    case kInobject:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kInobjectFields);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kInobjectFields);
      }
      break;
    case kDouble:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kDoubleFields);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kDoubleFields);
      }
      break;
    case kBackingStore:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kBackingStoreFields);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kBackingStoreFields);
      }
      break;
    case kElementsPointer:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kElementsPointer);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kElementsPointer);
      }
      break;
    case kMaps:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kMaps);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kMaps);
      }
      break;
    case kExternalMemory:
      if (access_type == STORE) {
        instr->SetChangesFlag(::v8::internal::kExternalMemory);
      } else {
        instr->SetDependsOnFlag(::v8::internal::kExternalMemory);
      }
      break;
  }
}


void HObjectAccess::PrintTo(StringStream* stream) const {
  stream->Add(".");

  switch (portion()) {
    case kArrayLengths:
    case kStringLengths:
      stream->Add("%length");
      break;
    case kElementsPointer:
      stream->Add("%elements");
      break;
    case kMaps:
      stream->Add("%map");
      break;
    case kDouble:  // fall through
    case kInobject:
      if (!name_.is_null()) {
        stream->Add(String::cast(*name_)->ToCString().get());
      }
      stream->Add("[in-object]");
      break;
    case kBackingStore:
      if (!name_.is_null()) {
        stream->Add(String::cast(*name_)->ToCString().get());
      }
      stream->Add("[backing-store]");
      break;
    case kExternalMemory:
      stream->Add("[external-memory]");
      break;
  }

  stream->Add("@%d", offset());
}

} }  // namespace v8::internal
