// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include "src/base/lazy-instance.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/handles/handles-inl.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os, BranchHint hint) {
  switch (hint) {
    case BranchHint::kNone:
      return os << "None";
    case BranchHint::kTrue:
      return os << "True";
    case BranchHint::kFalse:
      return os << "False";
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, IsSafetyCheck is_safety_check) {
  switch (is_safety_check) {
    case IsSafetyCheck::kCriticalSafetyCheck:
      return os << "CriticalSafetyCheck";
    case IsSafetyCheck::kSafetyCheck:
      return os << "SafetyCheck";
    case IsSafetyCheck::kNoSafetyCheck:
      return os << "NoSafetyCheck";
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, TrapId trap_id) {
  switch (trap_id) {
#define TRAP_CASE(Name) \
  case TrapId::k##Name: \
    return os << #Name;
    FOREACH_WASM_TRAPREASON(TRAP_CASE)
#undef TRAP_CASE
    case TrapId::kInvalid:
      return os << "Invalid";
  }
  UNREACHABLE();
}

TrapId TrapIdOf(const Operator* const op) {
  DCHECK(op->opcode() == IrOpcode::kTrapIf ||
         op->opcode() == IrOpcode::kTrapUnless);
  return OpParameter<TrapId>(op);
}

std::ostream& operator<<(std::ostream& os, BranchOperatorInfo info) {
  return os << info.hint << ", " << info.is_safety_check;
}

const BranchOperatorInfo& BranchOperatorInfoOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kBranch, op->opcode());
  return OpParameter<BranchOperatorInfo>(op);
}

BranchHint BranchHintOf(const Operator* const op) {
  switch (op->opcode()) {
    case IrOpcode::kBranch:
      return BranchOperatorInfoOf(op).hint;
    case IrOpcode::kIfValue:
      return IfValueParametersOf(op).hint();
    case IrOpcode::kIfDefault:
      return OpParameter<BranchHint>(op);
    default:
      UNREACHABLE();
  }
}

int ValueInputCountOfReturn(Operator const* const op) {
  DCHECK_EQ(IrOpcode::kReturn, op->opcode());
  // Return nodes have a hidden input at index 0 which we ignore in the value
  // input count.
  return op->ValueInputCount() - 1;
}

bool operator==(DeoptimizeParameters lhs, DeoptimizeParameters rhs) {
  return lhs.kind() == rhs.kind() && lhs.reason() == rhs.reason() &&
         lhs.feedback() == rhs.feedback() &&
         lhs.is_safety_check() == rhs.is_safety_check();
}

bool operator!=(DeoptimizeParameters lhs, DeoptimizeParameters rhs) {
  return !(lhs == rhs);
}

size_t hash_value(DeoptimizeParameters p) {
  FeedbackSource::Hash feebdack_hash;
  return base::hash_combine(p.kind(), p.reason(), feebdack_hash(p.feedback()),
                            p.is_safety_check());
}

std::ostream& operator<<(std::ostream& os, DeoptimizeParameters p) {
  return os << p.kind() << ", " << p.reason() << ", " << p.is_safety_check()
            << ", " << p.feedback();
}

DeoptimizeParameters const& DeoptimizeParametersOf(Operator const* const op) {
  DCHECK(op->opcode() == IrOpcode::kDeoptimize ||
         op->opcode() == IrOpcode::kDeoptimizeIf ||
         op->opcode() == IrOpcode::kDeoptimizeUnless);
  return OpParameter<DeoptimizeParameters>(op);
}

IsSafetyCheck IsSafetyCheckOf(const Operator* op) {
  if (op->opcode() == IrOpcode::kBranch) {
    return BranchOperatorInfoOf(op).is_safety_check;
  }
  return DeoptimizeParametersOf(op).is_safety_check();
}

const Operator* CommonOperatorBuilder::MarkAsSafetyCheck(
    const Operator* op, IsSafetyCheck safety_check) {
  if (op->opcode() == IrOpcode::kBranch) {
    BranchOperatorInfo info = BranchOperatorInfoOf(op);
    if (info.is_safety_check == safety_check) return op;
    return Branch(info.hint, safety_check);
  }
  DeoptimizeParameters p = DeoptimizeParametersOf(op);
  if (p.is_safety_check() == safety_check) return op;
  switch (op->opcode()) {
    case IrOpcode::kDeoptimizeIf:
      return DeoptimizeIf(p.kind(), p.reason(), p.feedback(), safety_check);
    case IrOpcode::kDeoptimizeUnless:
      return DeoptimizeUnless(p.kind(), p.reason(), p.feedback(), safety_check);
    default:
      UNREACHABLE();
  }
}

const Operator* CommonOperatorBuilder::DelayedStringConstant(
    const StringConstantBase* str) {
  return new (zone()) Operator1<const StringConstantBase*>(
      IrOpcode::kDelayedStringConstant, Operator::kPure,
      "DelayedStringConstant", 0, 0, 0, 1, 0, 0, str);
}

bool operator==(SelectParameters const& lhs, SelectParameters const& rhs) {
  return lhs.representation() == rhs.representation() &&
         lhs.hint() == rhs.hint();
}


bool operator!=(SelectParameters const& lhs, SelectParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(SelectParameters const& p) {
  return base::hash_combine(p.representation(), p.hint());
}


std::ostream& operator<<(std::ostream& os, SelectParameters const& p) {
  return os << p.representation() << ", " << p.hint();
}


SelectParameters const& SelectParametersOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kSelect, op->opcode());
  return OpParameter<SelectParameters>(op);
}

CallDescriptor const* CallDescriptorOf(const Operator* const op) {
  DCHECK(op->opcode() == IrOpcode::kCall ||
         op->opcode() == IrOpcode::kTailCall);
  return OpParameter<CallDescriptor const*>(op);
}

size_t ProjectionIndexOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kProjection, op->opcode());
  return OpParameter<size_t>(op);
}


MachineRepresentation PhiRepresentationOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kPhi, op->opcode());
  return OpParameter<MachineRepresentation>(op);
}


int ParameterIndexOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kParameter, op->opcode());
  return OpParameter<ParameterInfo>(op).index();
}


const ParameterInfo& ParameterInfoOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kParameter, op->opcode());
  return OpParameter<ParameterInfo>(op);
}


bool operator==(ParameterInfo const& lhs, ParameterInfo const& rhs) {
  return lhs.index() == rhs.index();
}


bool operator!=(ParameterInfo const& lhs, ParameterInfo const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(ParameterInfo const& p) { return p.index(); }


std::ostream& operator<<(std::ostream& os, ParameterInfo const& i) {
  os << i.index();
  if (i.debug_name()) os << ", debug name: " << i.debug_name();
  return os;
}

std::ostream& operator<<(std::ostream& os, ObjectStateInfo const& i) {
  return os << "id:" << i.object_id() << ", size:" << i.size();
}

size_t hash_value(ObjectStateInfo const& p) {
  return base::hash_combine(p.object_id(), p.size());
}

std::ostream& operator<<(std::ostream& os, TypedObjectStateInfo const& i) {
  return os << "id:" << i.object_id() << ", " << i.machine_types();
}

size_t hash_value(TypedObjectStateInfo const& p) {
  return base::hash_combine(p.object_id(), p.machine_types());
}

bool operator==(RelocatablePtrConstantInfo const& lhs,
                RelocatablePtrConstantInfo const& rhs) {
  return lhs.rmode() == rhs.rmode() && lhs.value() == rhs.value() &&
         lhs.type() == rhs.type();
}

bool operator!=(RelocatablePtrConstantInfo const& lhs,
                RelocatablePtrConstantInfo const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(RelocatablePtrConstantInfo const& p) {
  return base::hash_combine(p.value(), int8_t{p.rmode()}, p.type());
}

std::ostream& operator<<(std::ostream& os,
                         RelocatablePtrConstantInfo const& p) {
  return os << p.value() << ", " << static_cast<int>(p.rmode()) << ", "
            << p.type();
}

SparseInputMask::InputIterator::InputIterator(
    SparseInputMask::BitMaskType bit_mask, Node* parent)
    : bit_mask_(bit_mask), parent_(parent), real_index_(0) {
#if DEBUG
  if (bit_mask_ != SparseInputMask::kDenseBitMask) {
    DCHECK_EQ(base::bits::CountPopulation(bit_mask_) -
                  base::bits::CountPopulation(kEndMarker),
              parent->InputCount());
  }
#endif
}

void SparseInputMask::InputIterator::Advance() {
  DCHECK(!IsEnd());

  if (IsReal()) {
    ++real_index_;
  }
  bit_mask_ >>= 1;
}

size_t SparseInputMask::InputIterator::AdvanceToNextRealOrEnd() {
  DCHECK_NE(bit_mask_, SparseInputMask::kDenseBitMask);

  size_t count = base::bits::CountTrailingZeros(bit_mask_);
  bit_mask_ >>= count;
  DCHECK(IsReal() || IsEnd());
  return count;
}

Node* SparseInputMask::InputIterator::GetReal() const {
  DCHECK(IsReal());
  return parent_->InputAt(real_index_);
}

bool SparseInputMask::InputIterator::IsReal() const {
  return bit_mask_ == SparseInputMask::kDenseBitMask ||
         (bit_mask_ & kEntryMask);
}

bool SparseInputMask::InputIterator::IsEnd() const {
  return (bit_mask_ == kEndMarker) ||
         (bit_mask_ == SparseInputMask::kDenseBitMask &&
          real_index_ >= parent_->InputCount());
}

int SparseInputMask::CountReal() const {
  DCHECK(!IsDense());
  return base::bits::CountPopulation(bit_mask_) -
         base::bits::CountPopulation(kEndMarker);
}

SparseInputMask::InputIterator SparseInputMask::IterateOverInputs(Node* node) {
  DCHECK(IsDense() || CountReal() == node->InputCount());
  return InputIterator(bit_mask_, node);
}

bool operator==(SparseInputMask const& lhs, SparseInputMask const& rhs) {
  return lhs.mask() == rhs.mask();
}

bool operator!=(SparseInputMask const& lhs, SparseInputMask const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(SparseInputMask const& p) {
  return base::hash_value(p.mask());
}

std::ostream& operator<<(std::ostream& os, SparseInputMask const& p) {
  if (p.IsDense()) {
    return os << "dense";
  } else {
    SparseInputMask::BitMaskType mask = p.mask();
    DCHECK_NE(mask, SparseInputMask::kDenseBitMask);

    os << "sparse:";

    while (mask != SparseInputMask::kEndMarker) {
      if (mask & SparseInputMask::kEntryMask) {
        os << "^";
      } else {
        os << ".";
      }
      mask >>= 1;
    }
    return os;
  }
}

bool operator==(TypedStateValueInfo const& lhs,
                TypedStateValueInfo const& rhs) {
  return lhs.machine_types() == rhs.machine_types() &&
         lhs.sparse_input_mask() == rhs.sparse_input_mask();
}

bool operator!=(TypedStateValueInfo const& lhs,
                TypedStateValueInfo const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(TypedStateValueInfo const& p) {
  return base::hash_combine(p.machine_types(), p.sparse_input_mask());
}

std::ostream& operator<<(std::ostream& os, TypedStateValueInfo const& p) {
  return os << p.machine_types() << ", " << p.sparse_input_mask();
}

size_t hash_value(RegionObservability observability) {
  return static_cast<size_t>(observability);
}

std::ostream& operator<<(std::ostream& os, RegionObservability observability) {
  switch (observability) {
    case RegionObservability::kObservable:
      return os << "observable";
    case RegionObservability::kNotObservable:
      return os << "not-observable";
  }
  UNREACHABLE();
}

RegionObservability RegionObservabilityOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kBeginRegion, op->opcode());
  return OpParameter<RegionObservability>(op);
}

Type TypeGuardTypeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kTypeGuard, op->opcode());
  return OpParameter<Type>(op);
}

std::ostream& operator<<(std::ostream& os,
                         const ZoneVector<MachineType>* types) {
  // Print all the MachineTypes, separated by commas.
  bool first = true;
  for (MachineType elem : *types) {
    if (!first) {
      os << ", ";
    }
    first = false;
    os << elem;
  }
  return os;
}

int OsrValueIndexOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kOsrValue, op->opcode());
  return OpParameter<int>(op);
}

SparseInputMask SparseInputMaskOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kStateValues ||
         op->opcode() == IrOpcode::kTypedStateValues);

  if (op->opcode() == IrOpcode::kTypedStateValues) {
    return OpParameter<TypedStateValueInfo>(op).sparse_input_mask();
  }
  return OpParameter<SparseInputMask>(op);
}

ZoneVector<MachineType> const* MachineTypesOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kTypedObjectState ||
         op->opcode() == IrOpcode::kTypedStateValues);

  if (op->opcode() == IrOpcode::kTypedStateValues) {
    return OpParameter<TypedStateValueInfo>(op).machine_types();
  }
  return OpParameter<TypedObjectStateInfo>(op).machine_types();
}

V8_EXPORT_PRIVATE bool operator==(IfValueParameters const& l,
                                  IfValueParameters const& r) {
  return l.value() == r.value() &&
         r.comparison_order() == r.comparison_order() && l.hint() == r.hint();
}

size_t hash_value(IfValueParameters const& p) {
  return base::hash_combine(p.value(), p.comparison_order(), p.hint());
}

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& out,
                                           IfValueParameters const& p) {
  out << p.value() << " (order " << p.comparison_order() << ", hint "
      << p.hint() << ")";
  return out;
}

IfValueParameters const& IfValueParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kIfValue);
  return OpParameter<IfValueParameters>(op);
}

#define COMMON_CACHED_OP_LIST(V)                                              \
  V(Dead, Operator::kFoldable, 0, 0, 0, 1, 1, 1)                              \
  V(Unreachable, Operator::kFoldable, 0, 1, 1, 1, 1, 0)                       \
  V(IfTrue, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                             \
  V(IfFalse, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                            \
  V(IfSuccess, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                          \
  V(IfException, Operator::kKontrol, 0, 1, 1, 1, 1, 1)                        \
  V(Throw, Operator::kKontrol, 0, 1, 1, 0, 0, 1)                              \
  V(Terminate, Operator::kKontrol, 0, 1, 1, 0, 0, 1)                          \
  V(LoopExit, Operator::kKontrol, 0, 0, 2, 0, 0, 1)                           \
  V(LoopExitValue, Operator::kPure, 1, 0, 1, 1, 0, 0)                         \
  V(LoopExitEffect, Operator::kNoThrow, 0, 1, 1, 0, 1, 0)                     \
  V(Checkpoint, Operator::kKontrol, 0, 1, 1, 0, 1, 0)                         \
  V(FinishRegion, Operator::kKontrol, 1, 1, 0, 1, 1, 0)                       \
  V(Retain, Operator::kKontrol, 1, 1, 0, 0, 1, 0)                             \
  V(StaticAssert, Operator::kFoldable, 1, 1, 0, 0, 1, 0)

#define CACHED_BRANCH_LIST(V)   \
  V(None, CriticalSafetyCheck)  \
  V(True, CriticalSafetyCheck)  \
  V(False, CriticalSafetyCheck) \
  V(None, SafetyCheck)          \
  V(True, SafetyCheck)          \
  V(False, SafetyCheck)         \
  V(None, NoSafetyCheck)        \
  V(True, NoSafetyCheck)        \
  V(False, NoSafetyCheck)

#define CACHED_RETURN_LIST(V) \
  V(1)                        \
  V(2)                        \
  V(3)                        \
  V(4)

#define CACHED_END_LIST(V) \
  V(1)                     \
  V(2)                     \
  V(3)                     \
  V(4)                     \
  V(5)                     \
  V(6)                     \
  V(7)                     \
  V(8)


#define CACHED_EFFECT_PHI_LIST(V) \
  V(1)                            \
  V(2)                            \
  V(3)                            \
  V(4)                            \
  V(5)                            \
  V(6)

#define CACHED_INDUCTION_VARIABLE_PHI_LIST(V) \
  V(4)                                        \
  V(5)                                        \
  V(6)                                        \
  V(7)

#define CACHED_LOOP_LIST(V) \
  V(1)                      \
  V(2)


#define CACHED_MERGE_LIST(V) \
  V(1)                       \
  V(2)                       \
  V(3)                       \
  V(4)                       \
  V(5)                       \
  V(6)                       \
  V(7)                       \
  V(8)

#define CACHED_DEOPTIMIZE_LIST(V)                        \
  V(Eager, MinusZero)                                    \
  V(Eager, WrongMap)                                     \
  V(Soft, InsufficientTypeFeedbackForGenericKeyedAccess) \
  V(Soft, InsufficientTypeFeedbackForGenericNamedAccess)

#define CACHED_DEOPTIMIZE_IF_LIST(V)      \
  V(Eager, DivisionByZero, NoSafetyCheck) \
  V(Eager, DivisionByZero, SafetyCheck)   \
  V(Eager, Hole, NoSafetyCheck)           \
  V(Eager, Hole, SafetyCheck)             \
  V(Eager, MinusZero, NoSafetyCheck)      \
  V(Eager, MinusZero, SafetyCheck)        \
  V(Eager, Overflow, NoSafetyCheck)       \
  V(Eager, Overflow, SafetyCheck)         \
  V(Eager, Smi, SafetyCheck)

#define CACHED_DEOPTIMIZE_UNLESS_LIST(V)      \
  V(Eager, LostPrecision, NoSafetyCheck)      \
  V(Eager, LostPrecision, SafetyCheck)        \
  V(Eager, LostPrecisionOrNaN, NoSafetyCheck) \
  V(Eager, LostPrecisionOrNaN, SafetyCheck)   \
  V(Eager, NotAHeapNumber, SafetyCheck)       \
  V(Eager, NotANumberOrOddball, SafetyCheck)  \
  V(Eager, NotASmi, SafetyCheck)              \
  V(Eager, OutOfBounds, SafetyCheck)          \
  V(Eager, WrongInstanceType, SafetyCheck)    \
  V(Eager, WrongMap, SafetyCheck)

#define CACHED_TRAP_IF_LIST(V) \
  V(TrapDivUnrepresentable)    \
  V(TrapFloatUnrepresentable)

// The reason for a trap.
#define CACHED_TRAP_UNLESS_LIST(V) \
  V(TrapUnreachable)               \
  V(TrapMemOutOfBounds)            \
  V(TrapDivByZero)                 \
  V(TrapDivUnrepresentable)        \
  V(TrapRemByZero)                 \
  V(TrapFloatUnrepresentable)      \
  V(TrapFuncInvalid)               \
  V(TrapFuncSigMismatch)

#define CACHED_PARAMETER_LIST(V) \
  V(0)                           \
  V(1)                           \
  V(2)                           \
  V(3)                           \
  V(4)                           \
  V(5)                           \
  V(6)


#define CACHED_PHI_LIST(V) \
  V(kTagged, 1)            \
  V(kTagged, 2)            \
  V(kTagged, 3)            \
  V(kTagged, 4)            \
  V(kTagged, 5)            \
  V(kTagged, 6)            \
  V(kBit, 2)               \
  V(kFloat64, 2)           \
  V(kWord32, 2)


#define CACHED_PROJECTION_LIST(V) \
  V(0)                            \
  V(1)


#define CACHED_STATE_VALUES_LIST(V) \
  V(0)                              \
  V(1)                              \
  V(2)                              \
  V(3)                              \
  V(4)                              \
  V(5)                              \
  V(6)                              \
  V(7)                              \
  V(8)                              \
  V(10)                             \
  V(11)                             \
  V(12)                             \
  V(13)                             \
  V(14)


struct CommonOperatorGlobalCache final {
#define CACHED(Name, properties, value_input_count, effect_input_count,      \
               control_input_count, value_output_count, effect_output_count, \
               control_output_count)                                         \
  struct Name##Operator final : public Operator {                            \
    Name##Operator()                                                         \
        : Operator(IrOpcode::k##Name, properties, #Name, value_input_count,  \
                   effect_input_count, control_input_count,                  \
                   value_output_count, effect_output_count,                  \
                   control_output_count) {}                                  \
  };                                                                         \
  Name##Operator k##Name##Operator;
  COMMON_CACHED_OP_LIST(CACHED)
#undef CACHED

  template <size_t kInputCount>
  struct EndOperator final : public Operator {
    EndOperator()
        : Operator(                                // --
              IrOpcode::kEnd, Operator::kKontrol,  // opcode
              "End",                               // name
              0, 0, kInputCount, 0, 0, 0) {}       // counts
  };
#define CACHED_END(input_count) \
  EndOperator<input_count> kEnd##input_count##Operator;
  CACHED_END_LIST(CACHED_END)
#undef CACHED_END

  template <size_t kValueInputCount>
  struct ReturnOperator final : public Operator {
    ReturnOperator()
        : Operator(                                    // --
              IrOpcode::kReturn, Operator::kNoThrow,   // opcode
              "Return",                                // name
              kValueInputCount + 1, 1, 1, 0, 0, 1) {}  // counts
  };
#define CACHED_RETURN(value_input_count) \
  ReturnOperator<value_input_count> kReturn##value_input_count##Operator;
  CACHED_RETURN_LIST(CACHED_RETURN)
#undef CACHED_RETURN

  template <BranchHint hint, IsSafetyCheck is_safety_check>
  struct BranchOperator final : public Operator1<BranchOperatorInfo> {
    BranchOperator()
        : Operator1<BranchOperatorInfo>(                     // --
              IrOpcode::kBranch, Operator::kKontrol,         // opcode
              "Branch",                                      // name
              1, 0, 1, 0, 0, 2,                              // counts
              BranchOperatorInfo{hint, is_safety_check}) {}  // parameter
  };
#define CACHED_BRANCH(Hint, IsCheck)                             \
  BranchOperator<BranchHint::k##Hint, IsSafetyCheck::k##IsCheck> \
      kBranch##Hint##IsCheck##Operator;
  CACHED_BRANCH_LIST(CACHED_BRANCH)
#undef CACHED_BRANCH

  template <int kEffectInputCount>
  struct EffectPhiOperator final : public Operator {
    EffectPhiOperator()
        : Operator(                                      // --
              IrOpcode::kEffectPhi, Operator::kKontrol,  // opcode
              "EffectPhi",                               // name
              0, kEffectInputCount, 1, 0, 1, 0) {}       // counts
  };
#define CACHED_EFFECT_PHI(input_count) \
  EffectPhiOperator<input_count> kEffectPhi##input_count##Operator;
  CACHED_EFFECT_PHI_LIST(CACHED_EFFECT_PHI)
#undef CACHED_EFFECT_PHI

  template <RegionObservability kRegionObservability>
  struct BeginRegionOperator final : public Operator1<RegionObservability> {
    BeginRegionOperator()
        : Operator1<RegionObservability>(                  // --
              IrOpcode::kBeginRegion, Operator::kKontrol,  // opcode
              "BeginRegion",                               // name
              0, 1, 0, 0, 1, 0,                            // counts
              kRegionObservability) {}                     // parameter
  };
  BeginRegionOperator<RegionObservability::kObservable>
      kBeginRegionObservableOperator;
  BeginRegionOperator<RegionObservability::kNotObservable>
      kBeginRegionNotObservableOperator;

  template <size_t kInputCount>
  struct LoopOperator final : public Operator {
    LoopOperator()
        : Operator(                                 // --
              IrOpcode::kLoop, Operator::kKontrol,  // opcode
              "Loop",                               // name
              0, 0, kInputCount, 0, 0, 1) {}        // counts
  };
#define CACHED_LOOP(input_count) \
  LoopOperator<input_count> kLoop##input_count##Operator;
  CACHED_LOOP_LIST(CACHED_LOOP)
#undef CACHED_LOOP

  template <size_t kInputCount>
  struct MergeOperator final : public Operator {
    MergeOperator()
        : Operator(                                  // --
              IrOpcode::kMerge, Operator::kKontrol,  // opcode
              "Merge",                               // name
              0, 0, kInputCount, 0, 0, 1) {}         // counts
  };
#define CACHED_MERGE(input_count) \
  MergeOperator<input_count> kMerge##input_count##Operator;
  CACHED_MERGE_LIST(CACHED_MERGE)
#undef CACHED_MERGE

  template <DeoptimizeKind kKind, DeoptimizeReason kReason>
  struct DeoptimizeOperator final : public Operator1<DeoptimizeParameters> {
    DeoptimizeOperator()
        : Operator1<DeoptimizeParameters>(               // --
              IrOpcode::kDeoptimize,                     // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "Deoptimize",                              // name
              1, 1, 1, 0, 0, 1,                          // counts
              DeoptimizeParameters(kKind, kReason, FeedbackSource(),
                                   IsSafetyCheck::kNoSafetyCheck)) {}
  };
#define CACHED_DEOPTIMIZE(Kind, Reason)                                    \
  DeoptimizeOperator<DeoptimizeKind::k##Kind, DeoptimizeReason::k##Reason> \
      kDeoptimize##Kind##Reason##Operator;
  CACHED_DEOPTIMIZE_LIST(CACHED_DEOPTIMIZE)
#undef CACHED_DEOPTIMIZE

  template <DeoptimizeKind kKind, DeoptimizeReason kReason,
            IsSafetyCheck is_safety_check>
  struct DeoptimizeIfOperator final : public Operator1<DeoptimizeParameters> {
    DeoptimizeIfOperator()
        : Operator1<DeoptimizeParameters>(               // --
              IrOpcode::kDeoptimizeIf,                   // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "DeoptimizeIf",                            // name
              2, 1, 1, 0, 1, 1,                          // counts
              DeoptimizeParameters(kKind, kReason, FeedbackSource(),
                                   is_safety_check)) {}
  };
#define CACHED_DEOPTIMIZE_IF(Kind, Reason, IsCheck)                          \
  DeoptimizeIfOperator<DeoptimizeKind::k##Kind, DeoptimizeReason::k##Reason, \
                       IsSafetyCheck::k##IsCheck>                            \
      kDeoptimizeIf##Kind##Reason##IsCheck##Operator;
  CACHED_DEOPTIMIZE_IF_LIST(CACHED_DEOPTIMIZE_IF)
#undef CACHED_DEOPTIMIZE_IF

  template <DeoptimizeKind kKind, DeoptimizeReason kReason,
            IsSafetyCheck is_safety_check>
  struct DeoptimizeUnlessOperator final
      : public Operator1<DeoptimizeParameters> {
    DeoptimizeUnlessOperator()
        : Operator1<DeoptimizeParameters>(               // --
              IrOpcode::kDeoptimizeUnless,               // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "DeoptimizeUnless",                        // name
              2, 1, 1, 0, 1, 1,                          // counts
              DeoptimizeParameters(kKind, kReason, FeedbackSource(),
                                   is_safety_check)) {}
  };
#define CACHED_DEOPTIMIZE_UNLESS(Kind, Reason, IsCheck) \
  DeoptimizeUnlessOperator<DeoptimizeKind::k##Kind,     \
                           DeoptimizeReason::k##Reason, \
                           IsSafetyCheck::k##IsCheck>   \
      kDeoptimizeUnless##Kind##Reason##IsCheck##Operator;
  CACHED_DEOPTIMIZE_UNLESS_LIST(CACHED_DEOPTIMIZE_UNLESS)
#undef CACHED_DEOPTIMIZE_UNLESS

  template <TrapId trap_id>
  struct TrapIfOperator final : public Operator1<TrapId> {
    TrapIfOperator()
        : Operator1<TrapId>(                             // --
              IrOpcode::kTrapIf,                         // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "TrapIf",                                  // name
              1, 1, 1, 0, 0, 1,                          // counts
              trap_id) {}                                // parameter
  };
#define CACHED_TRAP_IF(Trap) \
  TrapIfOperator<TrapId::k##Trap> kTrapIf##Trap##Operator;
  CACHED_TRAP_IF_LIST(CACHED_TRAP_IF)
#undef CACHED_TRAP_IF

  template <TrapId trap_id>
  struct TrapUnlessOperator final : public Operator1<TrapId> {
    TrapUnlessOperator()
        : Operator1<TrapId>(                             // --
              IrOpcode::kTrapUnless,                     // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "TrapUnless",                              // name
              1, 1, 1, 0, 0, 1,                          // counts
              trap_id) {}                                // parameter
  };
#define CACHED_TRAP_UNLESS(Trap) \
  TrapUnlessOperator<TrapId::k##Trap> kTrapUnless##Trap##Operator;
  CACHED_TRAP_UNLESS_LIST(CACHED_TRAP_UNLESS)
#undef CACHED_TRAP_UNLESS

  template <MachineRepresentation kRep, int kInputCount>
  struct PhiOperator final : public Operator1<MachineRepresentation> {
    PhiOperator()
        : Operator1<MachineRepresentation>(     //--
              IrOpcode::kPhi, Operator::kPure,  // opcode
              "Phi",                            // name
              kInputCount, 0, 1, 1, 0, 0,       // counts
              kRep) {}                          // parameter
  };
#define CACHED_PHI(rep, input_count)                   \
  PhiOperator<MachineRepresentation::rep, input_count> \
      kPhi##rep##input_count##Operator;
  CACHED_PHI_LIST(CACHED_PHI)
#undef CACHED_PHI

  template <int kInputCount>
  struct InductionVariablePhiOperator final : public Operator {
    InductionVariablePhiOperator()
        : Operator(                                              //--
              IrOpcode::kInductionVariablePhi, Operator::kPure,  // opcode
              "InductionVariablePhi",                            // name
              kInputCount, 0, 1, 1, 0, 0) {}                     // counts
  };
#define CACHED_INDUCTION_VARIABLE_PHI(input_count) \
  InductionVariablePhiOperator<input_count>        \
      kInductionVariablePhi##input_count##Operator;
  CACHED_INDUCTION_VARIABLE_PHI_LIST(CACHED_INDUCTION_VARIABLE_PHI)
#undef CACHED_INDUCTION_VARIABLE_PHI

  template <int kIndex>
  struct ParameterOperator final : public Operator1<ParameterInfo> {
    ParameterOperator()
        : Operator1<ParameterInfo>(                   // --
              IrOpcode::kParameter, Operator::kPure,  // opcode
              "Parameter",                            // name
              1, 0, 0, 1, 0, 0,                       // counts,
              ParameterInfo(kIndex, nullptr)) {}      // parameter and name
  };
#define CACHED_PARAMETER(index) \
  ParameterOperator<index> kParameter##index##Operator;
  CACHED_PARAMETER_LIST(CACHED_PARAMETER)
#undef CACHED_PARAMETER

  template <size_t kIndex>
  struct ProjectionOperator final : public Operator1<size_t> {
    ProjectionOperator()
        : Operator1<size_t>(          // --
              IrOpcode::kProjection,  // opcode
              Operator::kPure,        // flags
              "Projection",           // name
              1, 0, 1, 1, 0, 0,       // counts,
              kIndex) {}              // parameter
  };
#define CACHED_PROJECTION(index) \
  ProjectionOperator<index> kProjection##index##Operator;
  CACHED_PROJECTION_LIST(CACHED_PROJECTION)
#undef CACHED_PROJECTION

  template <int kInputCount>
  struct StateValuesOperator final : public Operator1<SparseInputMask> {
    StateValuesOperator()
        : Operator1<SparseInputMask>(       // --
              IrOpcode::kStateValues,       // opcode
              Operator::kPure,              // flags
              "StateValues",                // name
              kInputCount, 0, 0, 1, 0, 0,   // counts
              SparseInputMask::Dense()) {}  // parameter
  };
#define CACHED_STATE_VALUES(input_count) \
  StateValuesOperator<input_count> kStateValues##input_count##Operator;
  CACHED_STATE_VALUES_LIST(CACHED_STATE_VALUES)
#undef CACHED_STATE_VALUES
};

namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(CommonOperatorGlobalCache,
                                GetCommonOperatorGlobalCache)
}

CommonOperatorBuilder::CommonOperatorBuilder(Zone* zone)
    : cache_(*GetCommonOperatorGlobalCache()), zone_(zone) {}

#define CACHED(Name, properties, value_input_count, effect_input_count,      \
               control_input_count, value_output_count, effect_output_count, \
               control_output_count)                                         \
  const Operator* CommonOperatorBuilder::Name() {                            \
    return &cache_.k##Name##Operator;                                        \
  }
COMMON_CACHED_OP_LIST(CACHED)
#undef CACHED


const Operator* CommonOperatorBuilder::End(size_t control_input_count) {
  switch (control_input_count) {
#define CACHED_END(input_count) \
  case input_count:             \
    return &cache_.kEnd##input_count##Operator;
    CACHED_END_LIST(CACHED_END)
#undef CACHED_END
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(             //--
      IrOpcode::kEnd, Operator::kKontrol,   // opcode
      "End",                                // name
      0, 0, control_input_count, 0, 0, 0);  // counts
}

const Operator* CommonOperatorBuilder::Return(int value_input_count) {
  switch (value_input_count) {
#define CACHED_RETURN(input_count) \
  case input_count:                \
    return &cache_.kReturn##input_count##Operator;
    CACHED_RETURN_LIST(CACHED_RETURN)
#undef CACHED_RETURN
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(               //--
      IrOpcode::kReturn, Operator::kNoThrow,  // opcode
      "Return",                               // name
      value_input_count + 1, 1, 1, 0, 0, 1);  // counts
}

const Operator* CommonOperatorBuilder::Branch(BranchHint hint,
                                              IsSafetyCheck is_safety_check) {
#define CACHED_BRANCH(Hint, IsCheck)                  \
  if (hint == BranchHint::k##Hint &&                  \
      is_safety_check == IsSafetyCheck::k##IsCheck) { \
    return &cache_.kBranch##Hint##IsCheck##Operator;  \
  }
  CACHED_BRANCH_LIST(CACHED_BRANCH)
#undef CACHED_BRANCH
  UNREACHABLE();
}

const Operator* CommonOperatorBuilder::Deoptimize(
    DeoptimizeKind kind, DeoptimizeReason reason,
    FeedbackSource const& feedback) {
#define CACHED_DEOPTIMIZE(Kind, Reason)                               \
  if (kind == DeoptimizeKind::k##Kind &&                              \
      reason == DeoptimizeReason::k##Reason && !feedback.IsValid()) { \
    return &cache_.kDeoptimize##Kind##Reason##Operator;               \
  }
  CACHED_DEOPTIMIZE_LIST(CACHED_DEOPTIMIZE)
#undef CACHED_DEOPTIMIZE
  // Uncached
  DeoptimizeParameters parameter(kind, reason, feedback,
                                 IsSafetyCheck::kNoSafetyCheck);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimize,                            // opcodes
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "Deoptimize",                                     // name
      1, 1, 1, 0, 0, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::DeoptimizeIf(
    DeoptimizeKind kind, DeoptimizeReason reason,
    FeedbackSource const& feedback, IsSafetyCheck is_safety_check) {
#define CACHED_DEOPTIMIZE_IF(Kind, Reason, IsCheck)                          \
  if (kind == DeoptimizeKind::k##Kind &&                                     \
      reason == DeoptimizeReason::k##Reason &&                               \
      is_safety_check == IsSafetyCheck::k##IsCheck && !feedback.IsValid()) { \
    return &cache_.kDeoptimizeIf##Kind##Reason##IsCheck##Operator;           \
  }
  CACHED_DEOPTIMIZE_IF_LIST(CACHED_DEOPTIMIZE_IF)
#undef CACHED_DEOPTIMIZE_IF
  // Uncached
  DeoptimizeParameters parameter(kind, reason, feedback, is_safety_check);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimizeIf,                          // opcode
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "DeoptimizeIf",                                   // name
      2, 1, 1, 0, 1, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::DeoptimizeUnless(
    DeoptimizeKind kind, DeoptimizeReason reason,
    FeedbackSource const& feedback, IsSafetyCheck is_safety_check) {
#define CACHED_DEOPTIMIZE_UNLESS(Kind, Reason, IsCheck)                      \
  if (kind == DeoptimizeKind::k##Kind &&                                     \
      reason == DeoptimizeReason::k##Reason &&                               \
      is_safety_check == IsSafetyCheck::k##IsCheck && !feedback.IsValid()) { \
    return &cache_.kDeoptimizeUnless##Kind##Reason##IsCheck##Operator;       \
  }
  CACHED_DEOPTIMIZE_UNLESS_LIST(CACHED_DEOPTIMIZE_UNLESS)
#undef CACHED_DEOPTIMIZE_UNLESS
  // Uncached
  DeoptimizeParameters parameter(kind, reason, feedback, is_safety_check);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimizeUnless,                      // opcode
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "DeoptimizeUnless",                               // name
      2, 1, 1, 0, 1, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::TrapIf(TrapId trap_id) {
  switch (trap_id) {
#define CACHED_TRAP_IF(Trap) \
  case TrapId::k##Trap:      \
    return &cache_.kTrapIf##Trap##Operator;
    CACHED_TRAP_IF_LIST(CACHED_TRAP_IF)
#undef CACHED_TRAP_IF
    default:
      break;
  }
  // Uncached
  return new (zone()) Operator1<TrapId>(         // --
      IrOpcode::kTrapIf,                         // opcode
      Operator::kFoldable | Operator::kNoThrow,  // properties
      "TrapIf",                                  // name
      1, 1, 1, 0, 0, 1,                          // counts
      trap_id);                                  // parameter
}

const Operator* CommonOperatorBuilder::TrapUnless(TrapId trap_id) {
  switch (trap_id) {
#define CACHED_TRAP_UNLESS(Trap) \
  case TrapId::k##Trap:          \
    return &cache_.kTrapUnless##Trap##Operator;
    CACHED_TRAP_UNLESS_LIST(CACHED_TRAP_UNLESS)
#undef CACHED_TRAP_UNLESS
    default:
      break;
  }
  // Uncached
  return new (zone()) Operator1<TrapId>(         // --
      IrOpcode::kTrapUnless,                     // opcode
      Operator::kFoldable | Operator::kNoThrow,  // properties
      "TrapUnless",                              // name
      1, 1, 1, 0, 0, 1,                          // counts
      trap_id);                                  // parameter
}

const Operator* CommonOperatorBuilder::Switch(size_t control_output_count) {
  return new (zone()) Operator(               // --
      IrOpcode::kSwitch, Operator::kKontrol,  // opcode
      "Switch",                               // name
      1, 0, 1, 0, 0, control_output_count);   // counts
}

const Operator* CommonOperatorBuilder::IfValue(int32_t index,
                                               int32_t comparison_order,
                                               BranchHint hint) {
  return new (zone()) Operator1<IfValueParameters>(       // --
      IrOpcode::kIfValue, Operator::kKontrol,             // opcode
      "IfValue",                                          // name
      0, 0, 1, 0, 0, 1,                                   // counts
      IfValueParameters(index, comparison_order, hint));  // parameter
}

const Operator* CommonOperatorBuilder::IfDefault(BranchHint hint) {
  return new (zone()) Operator1<BranchHint>(     // --
      IrOpcode::kIfDefault, Operator::kKontrol,  // opcode
      "IfDefault",                               // name
      0, 0, 1, 0, 0, 1,                          // counts
      hint);                                     // parameter
}

const Operator* CommonOperatorBuilder::Start(int value_output_count) {
  return new (zone()) Operator(                                    // --
      IrOpcode::kStart, Operator::kFoldable | Operator::kNoThrow,  // opcode
      "Start",                                                     // name
      0, 0, 0, value_output_count, 1, 1);                          // counts
}


const Operator* CommonOperatorBuilder::Loop(int control_input_count) {
  switch (control_input_count) {
#define CACHED_LOOP(input_count) \
  case input_count:              \
    return &cache_.kLoop##input_count##Operator;
    CACHED_LOOP_LIST(CACHED_LOOP)
#undef CACHED_LOOP
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(             // --
      IrOpcode::kLoop, Operator::kKontrol,  // opcode
      "Loop",                               // name
      0, 0, control_input_count, 0, 0, 1);  // counts
}


const Operator* CommonOperatorBuilder::Merge(int control_input_count) {
  switch (control_input_count) {
#define CACHED_MERGE(input_count) \
  case input_count:               \
    return &cache_.kMerge##input_count##Operator;
    CACHED_MERGE_LIST(CACHED_MERGE)
#undef CACHED_MERGE
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(              // --
      IrOpcode::kMerge, Operator::kKontrol,  // opcode
      "Merge",                               // name
      0, 0, control_input_count, 0, 0, 1);   // counts
}


const Operator* CommonOperatorBuilder::Parameter(int index,
                                                 const char* debug_name) {
  if (!debug_name) {
    switch (index) {
#define CACHED_PARAMETER(index) \
  case index:                   \
    return &cache_.kParameter##index##Operator;
      CACHED_PARAMETER_LIST(CACHED_PARAMETER)
#undef CACHED_PARAMETER
      default:
        break;
    }
  }
  // Uncached.
  return new (zone()) Operator1<ParameterInfo>(  // --
      IrOpcode::kParameter, Operator::kPure,     // opcode
      "Parameter",                               // name
      1, 0, 0, 1, 0, 0,                          // counts
      ParameterInfo(index, debug_name));         // parameter info
}

const Operator* CommonOperatorBuilder::OsrValue(int index) {
  return new (zone()) Operator1<int>(                // --
      IrOpcode::kOsrValue, Operator::kNoProperties,  // opcode
      "OsrValue",                                    // name
      0, 0, 1, 1, 0, 0,                              // counts
      index);                                        // parameter
}

const Operator* CommonOperatorBuilder::Int32Constant(int32_t value) {
  return new (zone()) Operator1<int32_t>(         // --
      IrOpcode::kInt32Constant, Operator::kPure,  // opcode
      "Int32Constant",                            // name
      0, 0, 0, 1, 0, 0,                           // counts
      value);                                     // parameter
}


const Operator* CommonOperatorBuilder::Int64Constant(int64_t value) {
  return new (zone()) Operator1<int64_t>(         // --
      IrOpcode::kInt64Constant, Operator::kPure,  // opcode
      "Int64Constant",                            // name
      0, 0, 0, 1, 0, 0,                           // counts
      value);                                     // parameter
}

const Operator* CommonOperatorBuilder::TaggedIndexConstant(int32_t value) {
  return new (zone()) Operator1<int32_t>(               // --
      IrOpcode::kTaggedIndexConstant, Operator::kPure,  // opcode
      "TaggedIndexConstant",                            // name
      0, 0, 0, 1, 0, 0,                                 // counts
      value);                                           // parameter
}

const Operator* CommonOperatorBuilder::Float32Constant(volatile float value) {
  return new (zone()) Operator1<float>(             // --
      IrOpcode::kFloat32Constant, Operator::kPure,  // opcode
      "Float32Constant",                            // name
      0, 0, 0, 1, 0, 0,                             // counts
      value);                                       // parameter
}


const Operator* CommonOperatorBuilder::Float64Constant(volatile double value) {
  return new (zone()) Operator1<double>(            // --
      IrOpcode::kFloat64Constant, Operator::kPure,  // opcode
      "Float64Constant",                            // name
      0, 0, 0, 1, 0, 0,                             // counts
      value);                                       // parameter
}


const Operator* CommonOperatorBuilder::ExternalConstant(
    const ExternalReference& value) {
  return new (zone()) Operator1<ExternalReference>(  // --
      IrOpcode::kExternalConstant, Operator::kPure,  // opcode
      "ExternalConstant",                            // name
      0, 0, 0, 1, 0, 0,                              // counts
      value);                                        // parameter
}


const Operator* CommonOperatorBuilder::NumberConstant(volatile double value) {
  return new (zone()) Operator1<double>(           // --
      IrOpcode::kNumberConstant, Operator::kPure,  // opcode
      "NumberConstant",                            // name
      0, 0, 0, 1, 0, 0,                            // counts
      value);                                      // parameter
}

const Operator* CommonOperatorBuilder::PointerConstant(intptr_t value) {
  return new (zone()) Operator1<intptr_t>(          // --
      IrOpcode::kPointerConstant, Operator::kPure,  // opcode
      "PointerConstant",                            // name
      0, 0, 0, 1, 0, 0,                             // counts
      value);                                       // parameter
}

const Operator* CommonOperatorBuilder::HeapConstant(
    const Handle<HeapObject>& value) {
  return new (zone()) Operator1<Handle<HeapObject>>(  // --
      IrOpcode::kHeapConstant, Operator::kPure,       // opcode
      "HeapConstant",                                 // name
      0, 0, 0, 1, 0, 0,                               // counts
      value);                                         // parameter
}

const Operator* CommonOperatorBuilder::CompressedHeapConstant(
    const Handle<HeapObject>& value) {
  return new (zone()) Operator1<Handle<HeapObject>>(       // --
      IrOpcode::kCompressedHeapConstant, Operator::kPure,  // opcode
      "CompressedHeapConstant",                            // name
      0, 0, 0, 1, 0, 0,                                    // counts
      value);                                              // parameter
}

Handle<HeapObject> HeapConstantOf(const Operator* op) {
  DCHECK(IrOpcode::kHeapConstant == op->opcode() ||
         IrOpcode::kCompressedHeapConstant == op->opcode());
  return OpParameter<Handle<HeapObject>>(op);
}

const StringConstantBase* StringConstantBaseOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kDelayedStringConstant, op->opcode());
  return OpParameter<const StringConstantBase*>(op);
}

const Operator* CommonOperatorBuilder::RelocatableInt32Constant(
    int32_t value, RelocInfo::Mode rmode) {
  return new (zone()) Operator1<RelocatablePtrConstantInfo>(  // --
      IrOpcode::kRelocatableInt32Constant, Operator::kPure,   // opcode
      "RelocatableInt32Constant",                             // name
      0, 0, 0, 1, 0, 0,                                       // counts
      RelocatablePtrConstantInfo(value, rmode));              // parameter
}

const Operator* CommonOperatorBuilder::RelocatableInt64Constant(
    int64_t value, RelocInfo::Mode rmode) {
  return new (zone()) Operator1<RelocatablePtrConstantInfo>(  // --
      IrOpcode::kRelocatableInt64Constant, Operator::kPure,   // opcode
      "RelocatableInt64Constant",                             // name
      0, 0, 0, 1, 0, 0,                                       // counts
      RelocatablePtrConstantInfo(value, rmode));              // parameter
}

const Operator* CommonOperatorBuilder::ObjectId(uint32_t object_id) {
  return new (zone()) Operator1<uint32_t>(   // --
      IrOpcode::kObjectId, Operator::kPure,  // opcode
      "ObjectId",                            // name
      0, 0, 0, 1, 0, 0,                      // counts
      object_id);                            // parameter
}

const Operator* CommonOperatorBuilder::Select(MachineRepresentation rep,
                                              BranchHint hint) {
  return new (zone()) Operator1<SelectParameters>(  // --
      IrOpcode::kSelect, Operator::kPure,           // opcode
      "Select",                                     // name
      3, 0, 0, 1, 0, 0,                             // counts
      SelectParameters(rep, hint));                 // parameter
}


const Operator* CommonOperatorBuilder::Phi(MachineRepresentation rep,
                                           int value_input_count) {
  DCHECK_LT(0, value_input_count);  // Disallow empty phis.
#define CACHED_PHI(kRep, kValueInputCount)                 \
  if (MachineRepresentation::kRep == rep &&                \
      kValueInputCount == value_input_count) {             \
    return &cache_.kPhi##kRep##kValueInputCount##Operator; \
  }
  CACHED_PHI_LIST(CACHED_PHI)
#undef CACHED_PHI
  // Uncached.
  return new (zone()) Operator1<MachineRepresentation>(  // --
      IrOpcode::kPhi, Operator::kPure,                   // opcode
      "Phi",                                             // name
      value_input_count, 0, 1, 1, 0, 0,                  // counts
      rep);                                              // parameter
}

const Operator* CommonOperatorBuilder::TypeGuard(Type type) {
  return new (zone()) Operator1<Type>(        // --
      IrOpcode::kTypeGuard, Operator::kPure,  // opcode
      "TypeGuard",                            // name
      1, 1, 1, 1, 1, 0,                       // counts
      type);                                  // parameter
}

const Operator* CommonOperatorBuilder::FoldConstant() {
  return new (zone()) Operator(                  // --
      IrOpcode::kFoldConstant, Operator::kPure,  // opcode
      "FoldConstant",                            // name
      2, 0, 0, 1, 0, 0);                         // counts
}

const Operator* CommonOperatorBuilder::EffectPhi(int effect_input_count) {
  DCHECK_LT(0, effect_input_count);  // Disallow empty effect phis.
  switch (effect_input_count) {
#define CACHED_EFFECT_PHI(input_count) \
  case input_count:                    \
    return &cache_.kEffectPhi##input_count##Operator;
    CACHED_EFFECT_PHI_LIST(CACHED_EFFECT_PHI)
#undef CACHED_EFFECT_PHI
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(                  // --
      IrOpcode::kEffectPhi, Operator::kKontrol,  // opcode
      "EffectPhi",                               // name
      0, effect_input_count, 1, 0, 1, 0);        // counts
}

const Operator* CommonOperatorBuilder::InductionVariablePhi(int input_count) {
  DCHECK_LE(4, input_count);  // There must be always the entry, backedge,
                              // increment and at least one bound.
  switch (input_count) {
#define CACHED_INDUCTION_VARIABLE_PHI(input_count) \
  case input_count:                                \
    return &cache_.kInductionVariablePhi##input_count##Operator;
    CACHED_INDUCTION_VARIABLE_PHI_LIST(CACHED_INDUCTION_VARIABLE_PHI)
#undef CACHED_INDUCTION_VARIABLE_PHI
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(                          // --
      IrOpcode::kInductionVariablePhi, Operator::kPure,  // opcode
      "InductionVariablePhi",                            // name
      input_count, 0, 1, 1, 0, 0);                       // counts
}

const Operator* CommonOperatorBuilder::BeginRegion(
    RegionObservability region_observability) {
  switch (region_observability) {
    case RegionObservability::kObservable:
      return &cache_.kBeginRegionObservableOperator;
    case RegionObservability::kNotObservable:
      return &cache_.kBeginRegionNotObservableOperator;
  }
  UNREACHABLE();
}

const Operator* CommonOperatorBuilder::StateValues(int arguments,
                                                   SparseInputMask bitmask) {
  if (bitmask.IsDense()) {
    switch (arguments) {
#define CACHED_STATE_VALUES(arguments) \
  case arguments:                      \
    return &cache_.kStateValues##arguments##Operator;
      CACHED_STATE_VALUES_LIST(CACHED_STATE_VALUES)
#undef CACHED_STATE_VALUES
      default:
        break;
    }
  }

#if DEBUG
  DCHECK(bitmask.IsDense() || bitmask.CountReal() == arguments);
#endif

  // Uncached.
  return new (zone()) Operator1<SparseInputMask>(  // --
      IrOpcode::kStateValues, Operator::kPure,     // opcode
      "StateValues",                               // name
      arguments, 0, 0, 1, 0, 0,                    // counts
      bitmask);                                    // parameter
}

const Operator* CommonOperatorBuilder::TypedStateValues(
    const ZoneVector<MachineType>* types, SparseInputMask bitmask) {
#if DEBUG
  DCHECK(bitmask.IsDense() ||
         bitmask.CountReal() == static_cast<int>(types->size()));
#endif

  return new (zone()) Operator1<TypedStateValueInfo>(  // --
      IrOpcode::kTypedStateValues, Operator::kPure,    // opcode
      "TypedStateValues",                              // name
      static_cast<int>(types->size()), 0, 0, 1, 0, 0,  // counts
      TypedStateValueInfo(types, bitmask));            // parameters
}

const Operator* CommonOperatorBuilder::ArgumentsElementsState(
    ArgumentsStateType type) {
  return new (zone()) Operator1<ArgumentsStateType>(       // --
      IrOpcode::kArgumentsElementsState, Operator::kPure,  // opcode
      "ArgumentsElementsState",                            // name
      0, 0, 0, 1, 0, 0,                                    // counts
      type);                                               // parameter
}

const Operator* CommonOperatorBuilder::ArgumentsLengthState(
    ArgumentsStateType type) {
  return new (zone()) Operator1<ArgumentsStateType>(     // --
      IrOpcode::kArgumentsLengthState, Operator::kPure,  // opcode
      "ArgumentsLengthState",                            // name
      0, 0, 0, 1, 0, 0,                                  // counts
      type);                                             // parameter
}

ArgumentsStateType ArgumentsStateTypeOf(Operator const* op) {
  DCHECK(op->opcode() == IrOpcode::kArgumentsElementsState ||
         op->opcode() == IrOpcode::kArgumentsLengthState);
  return OpParameter<ArgumentsStateType>(op);
}

const Operator* CommonOperatorBuilder::ObjectState(uint32_t object_id,
                                                   int pointer_slots) {
  return new (zone()) Operator1<ObjectStateInfo>(  // --
      IrOpcode::kObjectState, Operator::kPure,     // opcode
      "ObjectState",                               // name
      pointer_slots, 0, 0, 1, 0, 0,                // counts
      ObjectStateInfo{object_id, pointer_slots});  // parameter
}

const Operator* CommonOperatorBuilder::TypedObjectState(
    uint32_t object_id, const ZoneVector<MachineType>* types) {
  return new (zone()) Operator1<TypedObjectStateInfo>(  // --
      IrOpcode::kTypedObjectState, Operator::kPure,     // opcode
      "TypedObjectState",                               // name
      static_cast<int>(types->size()), 0, 0, 1, 0, 0,   // counts
      TypedObjectStateInfo(object_id, types));          // parameter
}

uint32_t ObjectIdOf(Operator const* op) {
  switch (op->opcode()) {
    case IrOpcode::kObjectState:
      return OpParameter<ObjectStateInfo>(op).object_id();
    case IrOpcode::kTypedObjectState:
      return OpParameter<TypedObjectStateInfo>(op).object_id();
    case IrOpcode::kObjectId:
      return OpParameter<uint32_t>(op);
    default:
      UNREACHABLE();
  }
}

MachineRepresentation DeadValueRepresentationOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kDeadValue, op->opcode());
  return OpParameter<MachineRepresentation>(op);
}

const Operator* CommonOperatorBuilder::FrameState(
    BailoutId bailout_id, OutputFrameStateCombine state_combine,
    const FrameStateFunctionInfo* function_info) {
  FrameStateInfo state_info(bailout_id, state_combine, function_info);
  return new (zone()) Operator1<FrameStateInfo>(  // --
      IrOpcode::kFrameState, Operator::kPure,     // opcode
      "FrameState",                               // name
      5, 0, 0, 1, 0, 0,                           // counts
      state_info);                                // parameter
}

const Operator* CommonOperatorBuilder::Call(
    const CallDescriptor* call_descriptor) {
  class CallOperator final : public Operator1<const CallDescriptor*> {
   public:
    explicit CallOperator(const CallDescriptor* call_descriptor)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kCall, call_descriptor->properties(), "Call",
              call_descriptor->InputCount() +
                  call_descriptor->FrameStateCount(),
              Operator::ZeroIfPure(call_descriptor->properties()),
              Operator::ZeroIfEliminatable(call_descriptor->properties()),
              call_descriptor->ReturnCount(),
              Operator::ZeroIfPure(call_descriptor->properties()),
              Operator::ZeroIfNoThrow(call_descriptor->properties()),
              call_descriptor) {}

    void PrintParameter(std::ostream& os,
                        PrintVerbosity verbose) const override {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) CallOperator(call_descriptor);
}

const Operator* CommonOperatorBuilder::TailCall(
    const CallDescriptor* call_descriptor) {
  class TailCallOperator final : public Operator1<const CallDescriptor*> {
   public:
    explicit TailCallOperator(const CallDescriptor* call_descriptor)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kTailCall,
              call_descriptor->properties() | Operator::kNoThrow, "TailCall",
              call_descriptor->InputCount() +
                  call_descriptor->FrameStateCount(),
              1, 1, 0, 0, 1, call_descriptor) {}

    void PrintParameter(std::ostream& os,
                        PrintVerbosity verbose) const override {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) TailCallOperator(call_descriptor);
}

const Operator* CommonOperatorBuilder::Projection(size_t index) {
  switch (index) {
#define CACHED_PROJECTION(index) \
  case index:                    \
    return &cache_.kProjection##index##Operator;
    CACHED_PROJECTION_LIST(CACHED_PROJECTION)
#undef CACHED_PROJECTION
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator1<size_t>(  // --
      IrOpcode::kProjection,              // opcode
      Operator::kPure,                    // flags
      "Projection",                       // name
      1, 0, 1, 1, 0, 0,                   // counts
      index);                             // parameter
}


const Operator* CommonOperatorBuilder::ResizeMergeOrPhi(const Operator* op,
                                                        int size) {
  if (op->opcode() == IrOpcode::kPhi) {
    return Phi(PhiRepresentationOf(op), size);
  } else if (op->opcode() == IrOpcode::kEffectPhi) {
    return EffectPhi(size);
  } else if (op->opcode() == IrOpcode::kMerge) {
    return Merge(size);
  } else if (op->opcode() == IrOpcode::kLoop) {
    return Loop(size);
  } else {
    UNREACHABLE();
  }
}

const FrameStateFunctionInfo*
CommonOperatorBuilder::CreateFrameStateFunctionInfo(
    FrameStateType type, int parameter_count, int local_count,
    Handle<SharedFunctionInfo> shared_info) {
  return new (zone()->New(sizeof(FrameStateFunctionInfo)))
      FrameStateFunctionInfo(type, parameter_count, local_count, shared_info);
}

const Operator* CommonOperatorBuilder::DeadValue(MachineRepresentation rep) {
  return new (zone()) Operator1<MachineRepresentation>(  // --
      IrOpcode::kDeadValue, Operator::kPure,             // opcode
      "DeadValue",                                       // name
      1, 0, 0, 1, 0, 0,                                  // counts
      rep);                                              // parameter
}

const FrameStateInfo& FrameStateInfoOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kFrameState, op->opcode());
  return OpParameter<FrameStateInfo>(op);
}

IsSafetyCheck CombineSafetyChecks(IsSafetyCheck a, IsSafetyCheck b) {
  if (a == IsSafetyCheck::kCriticalSafetyCheck ||
      b == IsSafetyCheck::kCriticalSafetyCheck) {
    return IsSafetyCheck::kCriticalSafetyCheck;
  }
  if (a == IsSafetyCheck::kSafetyCheck || b == IsSafetyCheck::kSafetyCheck) {
    return IsSafetyCheck::kSafetyCheck;
  }
  return IsSafetyCheck::kNoSafetyCheck;
}

#undef COMMON_CACHED_OP_LIST
#undef CACHED_BRANCH_LIST
#undef CACHED_RETURN_LIST
#undef CACHED_END_LIST
#undef CACHED_EFFECT_PHI_LIST
#undef CACHED_INDUCTION_VARIABLE_PHI_LIST
#undef CACHED_LOOP_LIST
#undef CACHED_MERGE_LIST
#undef CACHED_DEOPTIMIZE_LIST
#undef CACHED_DEOPTIMIZE_IF_LIST
#undef CACHED_DEOPTIMIZE_UNLESS_LIST
#undef CACHED_TRAP_IF_LIST
#undef CACHED_TRAP_UNLESS_LIST
#undef CACHED_PARAMETER_LIST
#undef CACHED_PHI_LIST
#undef CACHED_PROJECTION_LIST
#undef CACHED_STATE_VALUES_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
