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


int HValue::LoopWeight() const {
  const int w = FLAG_loop_weight;
  static const int weights[] = { 1, w, w*w, w*w*w, w*w*w*w };
  return weights[Min(block()->LoopNestingDepth(),
                     static_cast<int>(ARRAY_SIZE(weights)-1))];
}


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
  new_rep = RepresentationFromUseRequirements();
  if (new_rep.fits_into(Representation::Integer32())) {
    UpdateRepresentation(new_rep, h_infer, "use requirements");
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
    use_count[rep.kind()] += use->LoopWeight();
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


// This method is recursive but it is guaranteed to terminate because
// RedefinedOperand() always dominates "this".
bool HValue::IsRelationTrue(NumericRelation relation,
                            HValue* other,
                            int offset,
                            int scale) {
  if (this == other) {
    return scale == 0 && relation.IsExtendable(offset);
  }

  // Test the direct relation.
  if (IsRelationTrueInternal(relation, other, offset, scale)) return true;

  // If scale is 0 try the reversed relation.
  if (scale == 0 &&
      // TODO(mmassi): do we need the full, recursive IsRelationTrue?
      other->IsRelationTrueInternal(relation.Reversed(), this, -offset)) {
    return true;
  }

  // Try decomposition (but do not accept scaled compounds).
  DecompositionResult decomposition;
  if (TryDecompose(&decomposition) &&
      decomposition.scale() == 0 &&
      decomposition.base()->IsRelationTrue(relation, other,
                                           offset + decomposition.offset(),
                                           scale)) {
    return true;
  }

  // Pass the request to the redefined value.
  HValue* redefined = RedefinedOperand();
  return redefined != NULL && redefined->IsRelationTrue(relation, other,
                                                        offset, scale);
}


bool HValue::TryGuaranteeRange(HValue* upper_bound) {
  RangeEvaluationContext context = RangeEvaluationContext(this, upper_bound);
  TryGuaranteeRangeRecursive(&context);
  bool result = context.is_range_satisfied();
  if (result) {
    context.lower_bound_guarantee()->SetResponsibilityForRange(DIRECTION_LOWER);
    context.upper_bound_guarantee()->SetResponsibilityForRange(DIRECTION_UPPER);
  }
  return result;
}


void HValue::TryGuaranteeRangeRecursive(RangeEvaluationContext* context) {
  // Check if we already know that this value satisfies the lower bound.
  if (context->lower_bound_guarantee() == NULL) {
    if (IsRelationTrueInternal(NumericRelation::Ge(), context->lower_bound(),
                               context->offset(), context->scale())) {
      context->set_lower_bound_guarantee(this);
    }
  }

  // Check if we already know that this value satisfies the upper bound.
  if (context->upper_bound_guarantee() == NULL) {
    if (IsRelationTrueInternal(NumericRelation::Lt(), context->upper_bound(),
                               context->offset(), context->scale()) ||
        (context->scale() == 0 &&
         context->upper_bound()->IsRelationTrue(NumericRelation::Gt(),
                                                this, -context->offset()))) {
      context->set_upper_bound_guarantee(this);
    }
  }

  if (context->is_range_satisfied()) return;

  // See if our RedefinedOperand() satisfies the constraints.
  if (RedefinedOperand() != NULL) {
    RedefinedOperand()->TryGuaranteeRangeRecursive(context);
  }
  if (context->is_range_satisfied()) return;

  // See if the constraints can be satisfied by decomposition.
  DecompositionResult decomposition;
  if (TryDecompose(&decomposition)) {
    context->swap_candidate(&decomposition);
    context->candidate()->TryGuaranteeRangeRecursive(context);
    context->swap_candidate(&decomposition);
  }
  if (context->is_range_satisfied()) return;

  // Try to modify this to satisfy the constraint.

  TryGuaranteeRangeChanging(context);
}


RangeEvaluationContext::RangeEvaluationContext(HValue* value, HValue* upper)
    : lower_bound_(upper->block()->graph()->GetConstant0()),
      lower_bound_guarantee_(NULL),
      candidate_(value),
      upper_bound_(upper),
      upper_bound_guarantee_(NULL),
      offset_(0),
      scale_(0) {
}


HValue* RangeEvaluationContext::ConvertGuarantee(HValue* guarantee) {
  return guarantee->IsBoundsCheckBaseIndexInformation()
      ? HBoundsCheckBaseIndexInformation::cast(guarantee)->bounds_check()
      : guarantee;
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
  // Note: The c1visualizer syntax for locals allows only a sequence of the
  // following characters: A-Za-z0-9_-|:
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


bool HValue::Dominates(HValue* dominator, HValue* dominated) {
  if (dominator->block() != dominated->block()) {
    // If they are in different blocks we can use the dominance relation
    // between the blocks.
    return dominator->block()->Dominates(dominated->block());
  } else {
    // Otherwise we must see which instruction comes first, considering
    // that phis always precede regular instructions.
    if (dominator->IsInstruction()) {
      if (dominated->IsInstruction()) {
        for (HInstruction* next = HInstruction::cast(dominator)->next();
             next != NULL;
             next = next->next()) {
          if (next == dominated) return true;
        }
        return false;
      } else if (dominated->IsPhi()) {
        return false;
      } else {
        UNREACHABLE();
      }
    } else if (dominator->IsPhi()) {
      if (dominated->IsInstruction()) {
        return true;
      } else {
        // We cannot compare which phi comes first.
        UNREACHABLE();
      }
    } else {
      UNREACHABLE();
    }
    return false;
  }
}


bool HValue::TestDominanceUsingProcessedFlag(HValue* dominator,
                                             HValue* dominated) {
  if (dominator->block() != dominated->block()) {
    return dominator->block()->Dominates(dominated->block());
  } else {
    // If both arguments are in the same block we check if dominator is a phi
    // or if dominated has not already been processed: in either case we know
    // that dominator precedes dominated.
    return dominator->IsPhi() || !dominated->CheckFlag(kIDefsProcessingDone);
  }
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


bool HValue::HasAtLeastOneUseWithFlagAndNoneWithout(Flag f) {
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


void HInstruction::PrintTo(StringStream* stream) {
  PrintMnemonicTo(stream);
  PrintDataTo(stream);
  PrintRangeTo(stream);
  PrintChangesTo(stream);
  PrintTypeTo(stream);
  if (CheckFlag(HValue::kHasNoObservableSideEffects)) {
    stream->Add(" [noOSE]");
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

  // Verify that all uses are in the graph.
  for (HUseIterator use = uses(); !use.Done(); use.Advance()) {
    if (use.value()->IsInstruction()) {
      ASSERT(HInstruction::cast(use.value())->IsLinked());
    }
  }
}
#endif


HNumericConstraint* HNumericConstraint::AddToGraph(
    HValue* constrained_value,
    NumericRelation relation,
    HValue* related_value,
    HInstruction* insertion_point) {
  if (insertion_point == NULL) {
    if (constrained_value->IsInstruction()) {
      insertion_point = HInstruction::cast(constrained_value);
    } else if (constrained_value->IsPhi()) {
      insertion_point = constrained_value->block()->first();
    } else {
      UNREACHABLE();
    }
  }
  HNumericConstraint* result =
      new(insertion_point->block()->zone()) HNumericConstraint(
          constrained_value, relation, related_value);
  result->InsertAfter(insertion_point);
  return result;
}


void HNumericConstraint::PrintDataTo(StringStream* stream) {
  stream->Add("(");
  constrained_value()->PrintNameTo(stream);
  stream->Add(" %s ", relation().Mnemonic());
  related_value()->PrintNameTo(stream);
  stream->Add(")");
}


HInductionVariableAnnotation* HInductionVariableAnnotation::AddToGraph(
    HPhi* phi,
    NumericRelation relation,
    int operand_index) {
  HInductionVariableAnnotation* result =
      new(phi->block()->zone()) HInductionVariableAnnotation(phi, relation,
                                                             operand_index);
  result->InsertAfter(phi->block()->first());
  return result;
}


void HInductionVariableAnnotation::PrintDataTo(StringStream* stream) {
  stream->Add("(");
  RedefinedOperand()->PrintNameTo(stream);
  stream->Add(" %s ", relation().Mnemonic());
  induction_base()->PrintNameTo(stream);
  stream->Add(")");
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


void HBinaryCall::PrintDataTo(StringStream* stream) {
  first()->PrintNameTo(stream);
  stream->Add(" ");
  second()->PrintNameTo(stream);
  stream->Add(" ");
  stream->Add("#%d", argument_count());
}


void HBoundsCheck::TryGuaranteeRangeChanging(RangeEvaluationContext* context) {
  if (context->candidate()->ActualValue() != base()->ActualValue() ||
      context->scale() < scale()) {
    return;
  }

  // TODO(mmassi)
  // Instead of checking for "same basic block" we should check for
  // "dominates and postdominates".
  if (context->upper_bound() == length() &&
      context->lower_bound_guarantee() != NULL &&
      context->lower_bound_guarantee() != this &&
      context->lower_bound_guarantee()->block() != block() &&
      offset() < context->offset() &&
      index_can_increase() &&
      context->upper_bound_guarantee() == NULL) {
    offset_ = context->offset();
    SetResponsibilityForRange(DIRECTION_UPPER);
    context->set_upper_bound_guarantee(this);
  } else if (context->upper_bound_guarantee() != NULL &&
             context->upper_bound_guarantee() != this &&
             context->upper_bound_guarantee()->block() != block() &&
             offset() > context->offset() &&
             index_can_decrease() &&
             context->lower_bound_guarantee() == NULL) {
    offset_ = context->offset();
    SetResponsibilityForRange(DIRECTION_LOWER);
    context->set_lower_bound_guarantee(this);
  }
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

  if (actual_offset != 0) {
    HConstant* add_offset = new(block()->graph()->zone()) HConstant(
        actual_offset, index()->representation());
    add_offset->InsertBefore(this);
    HInstruction* add = HAdd::New(block()->graph()->zone(),
        block()->graph()->GetInvalidContext(), current_index, add_offset);
    add->InsertBefore(this);
    add->AssumeRepresentation(index()->representation());
    add->ClearFlag(kCanOverflow);
    current_index = add;
  }

  if (actual_scale != 0) {
    HConstant* sar_scale = new(block()->graph()->zone()) HConstant(
        actual_scale, index()->representation());
    sar_scale->InsertBefore(this);
    HInstruction* sar = HSar::New(block()->graph()->zone(),
        block()->graph()->GetInvalidContext(), current_index, sar_scale);
    sar->InsertBefore(this);
    sar->AssumeRepresentation(index()->representation());
    current_index = sar;
  }

  SetOperandAt(0, current_index);

  base_ = NULL;
  offset_ = 0;
  scale_ = 0;
  responsibility_direction_ = DIRECTION_NONE;
}


void HBoundsCheck::AddInformativeDefinitions() {
  // TODO(mmassi): Executing this code during AddInformativeDefinitions
  // is a hack. Move it to some other HPhase.
  if (FLAG_array_bounds_checks_elimination) {
    if (index()->TryGuaranteeRange(length())) {
      set_skip_check(true);
    }
    if (DetectCompoundIndex()) {
      HBoundsCheckBaseIndexInformation* base_index_info =
          new(block()->graph()->zone())
          HBoundsCheckBaseIndexInformation(this);
      base_index_info->InsertAfter(this);
    }
  }
}


bool HBoundsCheck::IsRelationTrueInternal(NumericRelation relation,
                                          HValue* related_value,
                                          int offset,
                                          int scale) {
  if (related_value == length()) {
    // A HBoundsCheck is smaller than the length it compared against.
    return NumericRelation::Lt().CompoundImplies(relation, 0, 0, offset, scale);
  } else if (related_value == block()->graph()->GetConstant0()) {
    // A HBoundsCheck is greater than or equal to zero.
    return NumericRelation::Ge().CompoundImplies(relation, 0, 0, offset, scale);
  } else {
    return false;
  }
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


bool HBoundsCheckBaseIndexInformation::IsRelationTrueInternal(
    NumericRelation relation,
    HValue* related_value,
    int offset,
    int scale) {
  if (related_value == bounds_check()->length()) {
    return NumericRelation::Lt().CompoundImplies(
        relation,
        bounds_check()->offset(), bounds_check()->scale(), offset, scale);
  } else if (related_value == block()->graph()->GetConstant0()) {
    return NumericRelation::Ge().CompoundImplies(
        relation,
        bounds_check()->offset(), bounds_check()->scale(), offset, scale);
  } else {
    return false;
  }
}


void HBoundsCheckBaseIndexInformation::PrintDataTo(StringStream* stream) {
  stream->Add("base: ");
  base_index()->PrintNameTo(stream);
  stream->Add(", check: ");
  base_index()->PrintNameTo(stream);
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


void HCallNewArray::PrintDataTo(StringStream* stream) {
  stream->Add(ElementsKindToString(elements_kind()));
  stream->Add(" ");
  HBinaryCall::PrintDataTo(stream);
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


void HCompareMap::PrintDataTo(StringStream* stream) {
  value()->PrintNameTo(stream);
  stream->Add(" (%p)", *map());
  HControlInstruction::PrintDataTo(stream);
}


const char* HUnaryMathOperation::OpName() const {
  switch (op()) {
    case kMathFloor: return "floor";
    case kMathRound: return "round";
    case kMathAbs: return "abs";
    case kMathLog: return "log";
    case kMathSin: return "sin";
    case kMathCos: return "cos";
    case kMathTan: return "tan";
    case kMathExp: return "exp";
    case kMathSqrt: return "sqrt";
    case kMathPowHalf: return "pow-half";
    default:
      UNREACHABLE();
      return NULL;
  }
}


Range* HUnaryMathOperation::InferRange(Zone* zone) {
  Representation r = representation();
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
  if (left()->EqualsInteger32Constant(nop_constant) &&
      !right()->CheckFlag(kUint32)) {
    return right();
  }
  if (right()->EqualsInteger32Constant(nop_constant) &&
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


static bool IsIdentityOperation(HValue* arg1, HValue* arg2, int32_t identity) {
  return arg1->representation().IsSpecialization() &&
    arg2->EqualsInteger32Constant(identity);
}


HValue* HAdd::Canonicalize() {
  if (IsIdentityOperation(left(), right(), 0)) return left();
  if (IsIdentityOperation(right(), left(), 0)) return right();
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


HValue* HMod::Canonicalize() {
  return this;
}


HValue* HDiv::Canonicalize() {
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


static HValue* SimplifiedDividendForMathFloorOfDiv(HValue* dividend) {
  // A value with an integer representation does not need to be transformed.
  if (dividend->representation().IsInteger32()) {
    return dividend;
  }
  // A change from an integer32 can be replaced by the integer32 value.
  if (dividend->IsChange() &&
      HChange::cast(dividend)->from().IsInteger32()) {
    return HChange::cast(dividend)->value();
  }
  return NULL;
}


HValue* HUnaryMathOperation::Canonicalize() {
  if (op() == kMathFloor) {
    HValue* val = value();
    if (val->IsChange()) val = HChange::cast(val)->value();

    // If the input is integer32 then we replace the floor instruction
    // with its input.
    if (val->representation().IsInteger32()) return val;

    if (val->IsDiv() && (val->UseCount() == 1)) {
      HDiv* hdiv = HDiv::cast(val);
      HValue* left = hdiv->left();
      HValue* right = hdiv->right();
      // Try to simplify left and right values of the division.
      HValue* new_left = SimplifiedDividendForMathFloorOfDiv(left);
      if (new_left == NULL &&
          hdiv->observed_input_representation(1).IsSmiOrInteger32()) {
        new_left = new(block()->zone())
            HChange(left, Representation::Integer32(), false, false);
        HChange::cast(new_left)->InsertBefore(this);
      }
      HValue* new_right =
          LChunkBuilder::SimplifiedDivisorForMathFloorOfDiv(right);
      if (new_right == NULL &&
#if V8_TARGET_ARCH_ARM
          CpuFeatures::IsSupported(SUDIV) &&
#endif
          hdiv->observed_input_representation(2).IsSmiOrInteger32()) {
        new_right = new(block()->zone())
            HChange(right, Representation::Integer32(), false, false);
        HChange::cast(new_right)->InsertBefore(this);
      }

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
      HMathFloorOfDiv* instr = new(block()->zone())
          HMathFloorOfDiv(context(), new_left, new_right);
      // Replace this HMathFloor instruction by the new HMathFloorOfDiv.
      instr->InsertBefore(this);
      ReplaceAllUsesWith(instr);
      Kill();
      // We know the division had no other uses than this HMathFloor. Delete it.
      // Dead code elimination will deal with |left| and |right| if
      // appropriate.
      hdiv->DeleteAndReplaceWith(NULL);

      // Return NULL to remove this instruction from the graph.
      return NULL;
    }
  }
  return this;
}


HValue* HCheckInstanceType::Canonicalize() {
  if (check_ == IS_STRING &&
      !value()->type().IsUninitialized() &&
      value()->type().IsString()) {
    return NULL;
  }

  if (check_ == IS_INTERNALIZED_STRING && value()->IsConstant()) {
    if (HConstant::cast(value())->HasInternalizedStringValue()) return NULL;
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
      *mask = kIsInternalizedMask;
      *tag = kInternalizedTag;
      return;
    default:
      UNREACHABLE();
  }
}


void HCheckMaps::SetSideEffectDominator(GVNFlag side_effect,
                                        HValue* dominator) {
  ASSERT(side_effect == kChangesMaps);
  // TODO(mstarzinger): For now we specialize on HStoreNamedField, but once
  // type information is rich enough we should generalize this to any HType
  // for which the map is known.
  if (HasNoUses() && dominator->IsStoreNamedField()) {
    HStoreNamedField* store = HStoreNamedField::cast(dominator);
    UniqueValueId map_unique_id = store->transition_unique_id();
    if (!map_unique_id.IsInitialized() || store->object() != value()) return;
    for (int i = 0; i < map_set()->length(); i++) {
      if (map_unique_id == map_unique_ids_.at(i)) {
        DeleteAndReplaceWith(NULL);
        return;
      }
    }
  }
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
    case IS_INTERNALIZED_STRING: return "internalized_string";
  }
  UNREACHABLE();
  return "";
}

void HCheckInstanceType::PrintDataTo(StringStream* stream) {
  stream->Add("%s ", GetCheckName());
  HUnaryOperation::PrintDataTo(stream);
}


void HCheckPrototypeMaps::PrintDataTo(StringStream* stream) {
  stream->Add("[receiver_prototype=%p,holder=%p]%s",
              *prototypes_.first(), *prototypes_.last(),
              CanOmitPrototypeChecks() ? " (omitted)" : "");
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
  Range* result;
  if (type().IsSmi()) {
    result = new(zone) Range(Smi::kMinValue, Smi::kMaxValue);
    result->set_can_be_minus_zero(false);
  } else {
    // Untagged integer32 cannot be -0, all other representations can.
    result = new(zone) Range();
    result->set_can_be_minus_zero(!representation().IsInteger32());
  }
  return result;
}


Range* HChange::InferRange(Zone* zone) {
  Range* input_range = value()->range();
  if (from().IsInteger32() &&
      to().IsSmiOrTagged() &&
      !value()->CheckFlag(HInstruction::kUint32) &&
      input_range != NULL && input_range->IsInSmiRange()) {
    set_type(HType::Smi());
    ClearGVNFlag(kChangesNewSpacePromotion);
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
  if (representation().IsInteger32()) {
    Range* a = left()->range();
    Range* b = right()->range();
    Range* res = a->Copy(zone);
    if (!res->AddAndCheckOverflow(b) ||
        CheckFlag(kAllUsesTruncatingToInt32)) {
      ClearFlag(kCanOverflow);
    }
    if (!CheckFlag(kAllUsesTruncatingToInt32)) {
      res->set_can_be_minus_zero(a->CanBeMinusZero() && b->CanBeMinusZero());
    }
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
    if (!res->SubAndCheckOverflow(b) ||
        CheckFlag(kAllUsesTruncatingToInt32)) {
      ClearFlag(kCanOverflow);
    }
    if (!CheckFlag(kAllUsesTruncatingToInt32)) {
      res->set_can_be_minus_zero(a->CanBeMinusZero() && b->CanBeZero());
    }
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
      // Clearing the kCanOverflow flag when kAllUsesAreTruncatingToInt32
      // would be wrong, because truncated integer multiplication is too
      // precise and therefore not the same as converting to Double and back.
      ClearFlag(kCanOverflow);
    }
    if (!CheckFlag(kAllUsesTruncatingToInt32)) {
      bool m0 = (a->CanBeZero() && b->CanBeNegative()) ||
          (a->CanBeNegative() && b->CanBeZero());
      res->set_can_be_minus_zero(m0);
    }
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
    if (!CheckFlag(kAllUsesTruncatingToInt32)) {
      if (a->CanBeMinusZero()) {
        result->set_can_be_minus_zero(true);
      }

      if (a->CanBeZero() && b->CanBeNegative()) {
        result->set_can_be_minus_zero(true);
      }
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

    if (left_can_be_negative && !CheckFlag(kAllUsesTruncatingToInt32)) {
      result->set_can_be_minus_zero(true);
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


void HPhi::AddInformativeDefinitions() {
  if (OperandCount() == 2) {
    // If one of the operands is an OSR block give up (this cannot be an
    // induction variable).
    if (OperandAt(0)->block()->is_osr_entry() ||
        OperandAt(1)->block()->is_osr_entry()) return;

    for (int operand_index = 0; operand_index < 2; operand_index++) {
      int other_operand_index = (operand_index + 1) % 2;

      static NumericRelation relations[] = {
        NumericRelation::Ge(),
        NumericRelation::Le()
      };

      // Check if this phi is an induction variable. If, e.g., we know that
      // its first input is greater than the phi itself, then that must be
      // the back edge, and the phi is always greater than its second input.
      for (int relation_index = 0; relation_index < 2; relation_index++) {
        if (OperandAt(operand_index)->IsRelationTrue(relations[relation_index],
                                                     this)) {
          HInductionVariableAnnotation::AddToGraph(this,
                                                   relations[relation_index],
                                                   other_operand_index);
        }
      }
    }
  }
}


bool HPhi::IsRelationTrueInternal(NumericRelation relation,
                                  HValue* other,
                                  int offset,
                                  int scale) {
  if (CheckFlag(kNumericConstraintEvaluationInProgress)) return false;

  SetFlag(kNumericConstraintEvaluationInProgress);
  bool result = true;
  for (int i = 0; i < OperandCount(); i++) {
    // Skip OSR entry blocks
    if (OperandAt(i)->block()->is_osr_entry()) continue;

    if (!OperandAt(i)->IsRelationTrue(relation, other, offset, scale)) {
      result = false;
      break;
    }
  }
  ClearFlag(kNumericConstraintEvaluationInProgress);

  return result;
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
  SetFlag(kTruncatingToInt32);
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* value = it.value();
    if (!value->IsPhi()) {
      Representation rep = value->observed_input_representation(it.index());
      non_phi_uses_[rep.kind()] += value->LoopWeight();
      if (FLAG_trace_representation) {
        PrintF("#%d Phi is used by real #%d %s as %s\n",
               id(), value->id(), value->Mnemonic(), rep.Mnemonic());
      }
      if (!value->IsSimulate() && !value->CheckFlag(kTruncatingToInt32)) {
        ClearFlag(kTruncatingToInt32);
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


void HDeoptimize::PrintDataTo(StringStream* stream) {
  if (OperandCount() == 0) return;
  OperandAt(0)->PrintNameTo(stream);
  for (int i = 1; i < OperandCount(); ++i) {
    stream->Add(" ");
    OperandAt(i)->PrintNameTo(stream);
  }
}


void HEnterInlined::RegisterReturnTarget(HBasicBlock* return_target,
                                         Zone* zone) {
  ASSERT(return_target->IsInlineReturnTarget());
  return_targets_.Add(return_target, zone);
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
    unique_id_(),
    has_smi_value_(false),
    has_int32_value_(false),
    has_double_value_(false),
    is_internalized_string_(false),
    is_not_in_new_space_(true),
    boolean_value_(handle->BooleanValue()) {
  if (handle_->IsHeapObject()) {
    Heap* heap = Handle<HeapObject>::cast(handle)->GetHeap();
    is_not_in_new_space_ = !heap->InNewSpace(*handle);
  }
  if (handle_->IsNumber()) {
    double n = handle_->Number();
    has_int32_value_ = IsInteger32(n);
    int32_value_ = DoubleToInt32(n);
    has_smi_value_ = has_int32_value_ && Smi::IsValid(int32_value_);
    double_value_ = n;
    has_double_value_ = true;
  } else {
    type_from_value_ = HType::TypeFromValue(handle_);
    is_internalized_string_ = handle_->IsInternalizedString();
  }
  Initialize(r);
}


HConstant::HConstant(Handle<Object> handle,
                     UniqueValueId unique_id,
                     Representation r,
                     HType type,
                     bool is_internalize_string,
                     bool is_not_in_new_space,
                     bool boolean_value)
    : handle_(handle),
      unique_id_(unique_id),
      has_smi_value_(false),
      has_int32_value_(false),
      has_double_value_(false),
      is_internalized_string_(is_internalize_string),
      is_not_in_new_space_(is_not_in_new_space),
      boolean_value_(boolean_value),
      type_from_value_(type) {
  ASSERT(!handle.is_null());
  ASSERT(!type.IsUninitialized());
  ASSERT(!type.IsTaggedNumber());
  Initialize(r);
}


HConstant::HConstant(int32_t integer_value,
                     Representation r,
                     bool is_not_in_new_space,
                     Handle<Object> optional_handle)
    : handle_(optional_handle),
      unique_id_(),
      has_int32_value_(true),
      has_double_value_(true),
      is_internalized_string_(false),
      is_not_in_new_space_(is_not_in_new_space),
      boolean_value_(integer_value != 0),
      int32_value_(integer_value),
      double_value_(FastI2D(integer_value)) {
  has_smi_value_ = Smi::IsValid(int32_value_);
  Initialize(r);
}


HConstant::HConstant(double double_value,
                     Representation r,
                     bool is_not_in_new_space,
                     Handle<Object> optional_handle)
    : handle_(optional_handle),
      unique_id_(),
      has_int32_value_(IsInteger32(double_value)),
      has_double_value_(true),
      is_internalized_string_(false),
      is_not_in_new_space_(is_not_in_new_space),
      boolean_value_(double_value != 0 && !std::isnan(double_value)),
      int32_value_(DoubleToInt32(double_value)),
      double_value_(double_value) {
  has_smi_value_ = has_int32_value_ && Smi::IsValid(int32_value_);
  Initialize(r);
}


void HConstant::Initialize(Representation r) {
  if (r.IsNone()) {
    if (has_smi_value_) {
      r = Representation::Smi();
    } else if (has_int32_value_) {
      r = Representation::Integer32();
    } else if (has_double_value_) {
      r = Representation::Double();
    } else {
      r = Representation::Tagged();
    }
  }
  set_representation(r);
  SetFlag(kUseGVN);
  if (representation().IsInteger32()) {
    ClearGVNFlag(kDependsOnOsrEntries);
  }
}


HConstant* HConstant::CopyToRepresentation(Representation r, Zone* zone) const {
  if (r.IsSmi() && !has_smi_value_) return NULL;
  if (r.IsInteger32() && !has_int32_value_) return NULL;
  if (r.IsDouble() && !has_double_value_) return NULL;
  if (has_int32_value_) {
    return new(zone) HConstant(int32_value_, r, is_not_in_new_space_, handle_);
  }
  if (has_double_value_) {
    return new(zone) HConstant(double_value_, r, is_not_in_new_space_, handle_);
  }
  ASSERT(!handle_.is_null());
  return new(zone) HConstant(handle_,
                             unique_id_,
                             r,
                             type_from_value_,
                             is_internalized_string_,
                             is_not_in_new_space_,
                             boolean_value_);
}


HConstant* HConstant::CopyToTruncatedInt32(Zone* zone) const {
  if (has_int32_value_) {
    return new(zone) HConstant(int32_value_,
                               Representation::Integer32(),
                               is_not_in_new_space_,
                               handle_);
  }
  if (has_double_value_) {
    return new(zone) HConstant(DoubleToInt32(double_value_),
                               Representation::Integer32(),
                               is_not_in_new_space_,
                               handle_);
  }
  return NULL;
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
  // When the operation has information about its own output type, don't look
  // at uses.
  if (!observed_output_representation_.IsNone()) return;
  new_rep = RepresentationFromUses();
  UpdateRepresentation(new_rep, h_infer, "uses");
  new_rep = RepresentationFromUseRequirements();
  if (new_rep.fits_into(Representation::Integer32())) {
    UpdateRepresentation(new_rep, h_infer, "use requirements");
  }
}


bool HBinaryOperation::IgnoreObservedOutputRepresentation(
    Representation current_rep) {
  return observed_output_representation_.IsDouble() &&
         current_rep.IsInteger32() &&
         // Mul in Integer32 mode would be too precise.
         !this->IsMul() &&
         CheckUsesForFlag(kTruncatingToInt32);
}


Representation HBinaryOperation::RepresentationFromInputs() {
  // Determine the worst case of observed input representations and
  // the currently assumed output representation.
  Representation rep = representation();
  for (int i = 1; i <= 2; ++i) {
    Representation input_rep = observed_input_representation(i);
    if (input_rep.is_more_general_than(rep)) rep = input_rep;
  }
  // If any of the actual input representation is more general than what we
  // have so far but not Tagged, use that representation instead.
  Representation left_rep = left()->representation();
  Representation right_rep = right()->representation();

  if (left_rep.is_more_general_than(rep) && !left_rep.IsTagged()) {
    rep = left_rep;
  }
  if (right_rep.is_more_general_than(rep) && !right_rep.IsTagged()) {
    rep = right_rep;
  }
  // Consider observed output representation, but ignore it if it's Double,
  // this instruction is not a division, and all its uses are truncating
  // to Integer32.
  if (observed_output_representation_.is_more_general_than(rep) &&
      !IgnoreObservedOutputRepresentation(rep)) {
    rep = observed_output_representation_;
  }
  return rep;
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
    return HValue::InferRange(zone);
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


void HCompareIDAndBranch::AddInformativeDefinitions() {
  NumericRelation r = NumericRelation::FromToken(token());
  if (r.IsNone()) return;

  HNumericConstraint::AddToGraph(left(), r, right(), SuccessorAt(0)->first());
  HNumericConstraint::AddToGraph(
        left(), r.Negated(), right(), SuccessorAt(1)->first());
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


void HCompareIDAndBranch::InferRepresentation(
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
    // tagged-to-double conversion to ensure the HCompareIDAndBranch's inputs
    // are doubles caused 'undefined' to be converted to NaN. That's compatible
    // out-of-the box with ordered relational comparisons (<, >, <=,
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
  if (HasTypeCheck()) {
    stream->Add(" ");
    typecheck()->PrintNameTo(stream);
  }
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
      types_unique_ids_(0, zone),
      name_unique_id_(),
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
    // Deprecated maps are updated to the current map in the type oracle.
    ASSERT(!map->is_deprecated());
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
          if (FLAG_track_double_fields &&
              lookup.representation().IsDouble()) {
            // Since the value needs to be boxed, use a generic handler for
            // loading doubles.
            continue;
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


void HCheckMaps::FinalizeUniqueValueId() {
  if (!map_unique_ids_.is_empty()) return;
  Zone* zone = block()->zone();
  map_unique_ids_.Initialize(map_set_.length(), zone);
  for (int i = 0; i < map_set_.length(); i++) {
    map_unique_ids_.Add(UniqueValueId(map_set_.at(i)), zone);
  }
}


void HLoadNamedFieldPolymorphic::FinalizeUniqueValueId() {
  if (!types_unique_ids_.is_empty()) return;
  Zone* zone = block()->zone();
  types_unique_ids_.Initialize(types_.length(), zone);
  for (int i = 0; i < types_.length(); i++) {
    types_unique_ids_.Add(UniqueValueId(types_.at(i)), zone);
  }
  name_unique_id_ = UniqueValueId(name_);
}


bool HLoadNamedFieldPolymorphic::DataEquals(HValue* value) {
  ASSERT_EQ(types_.length(), types_unique_ids_.length());
  HLoadNamedFieldPolymorphic* other = HLoadNamedFieldPolymorphic::cast(value);
  if (name_unique_id_ != other->name_unique_id_) return false;
  if (types_unique_ids_.length() != other->types_unique_ids_.length()) {
    return false;
  }
  if (need_generic_ != other->need_generic_) return false;
  for (int i = 0; i < types_unique_ids_.length(); i++) {
    bool found = false;
    for (int j = 0; j < types_unique_ids_.length(); j++) {
      if (types_unique_ids_.at(j) == other->types_unique_ids_.at(i)) {
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
  if (!IsFastDoubleElementsKind(elements_kind())) {
    return false;
  }

  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (!use->CheckFlag(HValue::kAllowUndefinedAsNaN)) {
      return false;
    }
  }

  return true;
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
  access_.PrintTo(stream);
  stream->Add(" = ");
  value()->PrintNameTo(stream);
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


void HInnerAllocatedObject::PrintDataTo(StringStream* stream) {
  base_object()->PrintNameTo(stream);
  stream->Add(" offset %d", offset());
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


HType HCheckHeapObject::CalculateInferredType() {
  return HType::NonPrimitive();
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
  ASSERT(!type_from_value_.IsUninitialized());
  return type_from_value_;
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


Representation HUnaryMathOperation::RepresentationFromInputs() {
  Representation rep = representation();
  // If any of the actual input representation is more general than what we
  // have so far but not Tagged, use that representation instead.
  Representation input_rep = value()->representation();
  if (!input_rep.IsTagged()) rep = rep.generalize(input_rep);
  return rep;
}


HType HStringCharFromCode::CalculateInferredType() {
  return HType::String();
}


HType HAllocateObject::CalculateInferredType() {
  return HType::JSObject();
}


HType HAllocate::CalculateInferredType() {
  return type_;
}


void HAllocate::PrintDataTo(StringStream* stream) {
  size()->PrintNameTo(stream);
  if (!GuaranteedInNewSpace()) stream->Add(" (pretenure)");
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
    if (HChange::cast(value())->from().IsInteger32()) {
      return false;
    }
    if (HChange::cast(value())->value()->type().IsSmi()) {
      return false;
    }
  }
  return true;
}


#define H_CONSTANT_INT32(val)                                                  \
new(zone) HConstant(static_cast<int32_t>(val), Representation::Integer32())
#define H_CONSTANT_DOUBLE(val)                                                 \
new(zone) HConstant(static_cast<double>(val), Representation::Double())

#define DEFINE_NEW_H_SIMPLE_ARITHMETIC_INSTR(HInstr, op)                       \
HInstruction* HInstr::New(                                                     \
    Zone* zone, HValue* context, HValue* left, HValue* right) {                \
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {      \
    HConstant* c_left = HConstant::cast(left);                                 \
    HConstant* c_right = HConstant::cast(right);                               \
    if ((c_left->HasNumberValue() && c_right->HasNumberValue())) {             \
      double double_res = c_left->DoubleValue() op c_right->DoubleValue();     \
      if (TypeInfo::IsInt32Double(double_res)) {                               \
        return H_CONSTANT_INT32(double_res);                                   \
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


HInstruction* HStringAdd::New(
    Zone* zone, HValue* context, HValue* left, HValue* right) {
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {
    HConstant* c_right = HConstant::cast(right);
    HConstant* c_left = HConstant::cast(left);
    if (c_left->HasStringValue() && c_right->HasStringValue()) {
      Handle<String> concat = zone->isolate()->factory()->NewFlatConcatString(
          c_left->StringValue(), c_right->StringValue());
      return new(zone) HConstant(concat, Representation::Tagged());
    }
  }
  return new(zone) HStringAdd(context, left, right);
}


HInstruction* HStringCharFromCode::New(
    Zone* zone, HValue* context, HValue* char_code) {
  if (FLAG_fold_constants && char_code->IsConstant()) {
    HConstant* c_code = HConstant::cast(char_code);
    Isolate* isolate = Isolate::Current();
    if (c_code->HasNumberValue()) {
      if (std::isfinite(c_code->DoubleValue())) {
        uint32_t code = c_code->NumberValueAsInteger32() & 0xffff;
        return new(zone) HConstant(LookupSingleCharacterStringFromCode(isolate,
                                                                       code),
                                   Representation::Tagged());
      }
      return new(zone) HConstant(isolate->factory()->empty_string(),
                                 Representation::Tagged());
    }
  }
  return new(zone) HStringCharFromCode(context, char_code);
}


HInstruction* HStringLength::New(Zone* zone, HValue* string) {
  if (FLAG_fold_constants && string->IsConstant()) {
    HConstant* c_string = HConstant::cast(string);
    if (c_string->HasStringValue()) {
      return new(zone) HConstant(c_string->StringValue()->length());
    }
  }
  return new(zone) HStringLength(string);
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
        case kMathSin:
        case kMathCos:
        case kMathTan:
          return H_CONSTANT_DOUBLE(OS::nan_value());
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
        default:
          UNREACHABLE();
          break;
      }
    }
    switch (op) {
      case kMathSin:
        return H_CONSTANT_DOUBLE(fast_sin(d));
      case kMathCos:
        return H_CONSTANT_DOUBLE(fast_cos(d));
      case kMathTan:
        return H_CONSTANT_DOUBLE(fast_tan(d));
      case kMathExp:
        return H_CONSTANT_DOUBLE(fast_exp(d));
      case kMathLog:
        return H_CONSTANT_DOUBLE(fast_log(d));
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
        return H_CONSTANT_DOUBLE(floor(d + 0.5));
      case kMathFloor:
        return H_CONSTANT_DOUBLE(floor(d));
      default:
        UNREACHABLE();
        break;
    }
  } while (false);
  return new(zone) HUnaryMathOperation(context, value, op);
}


HInstruction* HPower::New(Zone* zone, HValue* left, HValue* right) {
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
                        HValue* right,
                        Maybe<int> fixed_right_arg) {
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
        return H_CONSTANT_INT32(res);
      }
    }
  }
  return new(zone) HMod(context, left, right, fixed_right_arg);
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
        if (TypeInfo::IsInt32Double(double_res)) {
          return H_CONSTANT_INT32(double_res);
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
    Zone* zone, Token::Value op, HValue* context, HValue* left, HValue* right) {
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
      return H_CONSTANT_INT32(result);
    }
  }
  return new(zone) HBitwise(op, context, left, right);
}


#define DEFINE_NEW_H_BITWISE_INSTR(HInstr, result)                             \
HInstruction* HInstr::New(                                                     \
    Zone* zone, HValue* context, HValue* left, HValue* right) {                \
  if (FLAG_fold_constants && left->IsConstant() && right->IsConstant()) {      \
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
          new(graph->zone()) HConstant(DoubleToInt32(operand->DoubleValue()),
                                       Representation::Integer32());
      integer_input->InsertAfter(operand);
      SetOperandAt(i, integer_input);
    } else if (operand == graph->GetConstantTrue()) {
      SetOperandAt(i, graph->GetConstant1());
    } else {
      // This catches |false|, |undefined|, strings and objects.
      SetOperandAt(i, graph->GetConstant0());
    }
  }
  // Overwrite observed input representations because they are likely Tagged.
  for (HUseIterator it(uses()); !it.Done(); it.Advance()) {
    HValue* use = it.value();
    if (use->IsBinaryOperation()) {
      HBinaryOperation::cast(use)->set_observed_input_representation(
          it.index(), Representation::Integer32());
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


void HCheckFunction::Verify() {
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


HObjectAccess HObjectAccess::ForJSObjectOffset(int offset) {
  ASSERT(offset >= 0);
  Portion portion = kInobject;

  if (offset == JSObject::kElementsOffset) {
    portion = kElementsPointer;
  } else if (offset == JSObject::kMapOffset) {
    portion = kMaps;
  }
  return HObjectAccess(portion, offset, Handle<String>::null());
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
  return HObjectAccess(portion, offset, Handle<String>::null());
}


HObjectAccess HObjectAccess::ForBackingStoreOffset(int offset) {
  ASSERT(offset >= 0);
  return HObjectAccess(kBackingStore, offset, Handle<String>::null());
}


HObjectAccess HObjectAccess::ForField(Handle<Map> map,
    LookupResult *lookup, Handle<String> name) {
  ASSERT(lookup->IsField() || lookup->IsTransitionToField(*map));
  int index;
  if (lookup->IsField()) {
    index = lookup->GetLocalFieldIndexFromMap(*map);
  } else {
    Map* transition = lookup->GetTransitionMapFromMap(*map);
    int descriptor = transition->LastAdded();
    index = transition->instance_descriptors()->GetFieldIndex(descriptor) -
        map->inobject_properties();
  }
  if (index < 0) {
    // Negative property indices are in-object properties, indexed
    // from the end of the fixed part of the object.
    int offset = (index * kPointerSize) + map->instance_size();
    return HObjectAccess(kInobject, offset);
  } else {
    // Non-negative property indices are in the properties array.
    int offset = (index * kPointerSize) + FixedArray::kHeaderSize;
    return HObjectAccess(kBackingStore, offset, name);
  }
}


void HObjectAccess::SetGVNFlags(HValue *instr, bool is_store) {
  // set the appropriate GVN flags for a given load or store instruction
  if (is_store) {
    // track dominating allocations in order to eliminate write barriers
    instr->SetGVNFlag(kDependsOnNewSpacePromotion);
    instr->SetFlag(HValue::kTrackSideEffectDominators);
  } else {
    // try to GVN loads, but don't hoist above map changes
    instr->SetFlag(HValue::kUseGVN);
    instr->SetGVNFlag(kDependsOnMaps);
  }

  switch (portion()) {
    case kArrayLengths:
      instr->SetGVNFlag(is_store
          ? kChangesArrayLengths : kDependsOnArrayLengths);
      break;
    case kInobject:
      instr->SetGVNFlag(is_store
          ? kChangesInobjectFields : kDependsOnInobjectFields);
      break;
    case kDouble:
      instr->SetGVNFlag(is_store
          ? kChangesDoubleFields : kDependsOnDoubleFields);
      break;
    case kBackingStore:
      instr->SetGVNFlag(is_store
          ? kChangesBackingStoreFields : kDependsOnBackingStoreFields);
      break;
    case kElementsPointer:
      instr->SetGVNFlag(is_store
          ? kChangesElementsPointer : kDependsOnElementsPointer);
      break;
    case kMaps:
      instr->SetGVNFlag(is_store
          ? kChangesMaps : kDependsOnMaps);
      break;
  }
}


void HObjectAccess::PrintTo(StringStream* stream) {
  stream->Add(".");

  switch (portion()) {
    case kArrayLengths:
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
      if (!name_.is_null()) stream->Add(*String::cast(*name_)->ToCString());
      stream->Add("[in-object]");
      break;
    case kBackingStore:
      if (!name_.is_null()) stream->Add(*String::cast(*name_)->ToCString());
      stream->Add("[backing-store]");
      break;
  }

  stream->Add("@%d", offset());
}

} }  // namespace v8::internal
