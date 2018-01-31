// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include "src/assembler.h"
#include "src/base/lazy-instance.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/handles-inl.h"
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


BranchHint BranchHintOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kBranch, op->opcode());
  return OpParameter<BranchHint>(op);
}

int ValueInputCountOfReturn(Operator const* const op) {
  DCHECK_EQ(IrOpcode::kReturn, op->opcode());
  // Return nodes have a hidden input at index 0 which we ignore in the value
  // input count.
  return op->ValueInputCount() - 1;
}

bool operator==(DeoptimizeParameters lhs, DeoptimizeParameters rhs) {
  return lhs.kind() == rhs.kind() && lhs.reason() == rhs.reason();
}

bool operator!=(DeoptimizeParameters lhs, DeoptimizeParameters rhs) {
  return !(lhs == rhs);
}

size_t hash_value(DeoptimizeParameters p) {
  return base::hash_combine(p.kind(), p.reason());
}

std::ostream& operator<<(std::ostream& os, DeoptimizeParameters p) {
  return os << p.kind() << ":" << p.reason();
}

DeoptimizeParameters const& DeoptimizeParametersOf(Operator const* const op) {
  DCHECK(op->opcode() == IrOpcode::kDeoptimize ||
         op->opcode() == IrOpcode::kDeoptimizeIf ||
         op->opcode() == IrOpcode::kDeoptimizeUnless);
  return OpParameter<DeoptimizeParameters>(op);
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
  return os << p.representation() << "|" << p.hint();
}


SelectParameters const& SelectParametersOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kSelect, op->opcode());
  return OpParameter<SelectParameters>(op);
}

CallDescriptor const* CallDescriptorOf(const Operator* const op) {
  DCHECK(op->opcode() == IrOpcode::kCall ||
         op->opcode() == IrOpcode::kCallWithCallerSavedRegisters ||
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
  if (i.debug_name()) os << i.debug_name() << '#';
  os << i.index();
  return os;
}

std::ostream& operator<<(std::ostream& os, ObjectStateInfo const& i) {
  return os << "id:" << i.object_id() << "|size:" << i.size();
}

size_t hash_value(ObjectStateInfo const& p) {
  return base::hash_combine(p.object_id(), p.size());
}

std::ostream& operator<<(std::ostream& os, TypedObjectStateInfo const& i) {
  return os << "id:" << i.object_id() << "|" << i.machine_types();
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
  return base::hash_combine(p.value(), p.rmode(), p.type());
}

std::ostream& operator<<(std::ostream& os,
                         RelocatablePtrConstantInfo const& p) {
  return os << p.value() << "|" << p.rmode() << "|" << p.type();
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
  return os << p.machine_types() << "|" << p.sparse_input_mask();
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

Type* TypeGuardTypeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kTypeGuard, op->opcode());
  return OpParameter<Type*>(op);
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

#define COMMON_CACHED_OP_LIST(V)                                              \
  V(Dead, Operator::kFoldable, 0, 0, 0, 1, 1, 1)                              \
  V(DeadValue, Operator::kFoldable, 0, 0, 0, 1, 0, 0)                         \
  V(Unreachable, Operator::kFoldable, 0, 1, 1, 0, 1, 0)                       \
  V(IfTrue, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                             \
  V(IfFalse, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                            \
  V(IfSuccess, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                          \
  V(IfException, Operator::kKontrol, 0, 1, 1, 1, 1, 1)                        \
  V(IfDefault, Operator::kKontrol, 0, 0, 1, 0, 0, 1)                          \
  V(Throw, Operator::kKontrol, 0, 1, 1, 0, 0, 1)                              \
  V(Terminate, Operator::kKontrol, 0, 1, 1, 0, 0, 1)                          \
  V(OsrNormalEntry, Operator::kFoldable, 0, 1, 1, 0, 1, 1)                    \
  V(OsrLoopEntry, Operator::kFoldable | Operator::kNoThrow, 0, 1, 1, 0, 1, 1) \
  V(LoopExit, Operator::kKontrol, 0, 0, 2, 0, 0, 1)                           \
  V(LoopExitValue, Operator::kPure, 1, 0, 1, 1, 0, 0)                         \
  V(LoopExitEffect, Operator::kNoThrow, 0, 1, 1, 0, 1, 0)                     \
  V(Checkpoint, Operator::kKontrol, 0, 1, 1, 0, 1, 0)                         \
  V(FinishRegion, Operator::kKontrol, 1, 1, 0, 1, 1, 0)                       \
  V(Retain, Operator::kKontrol, 1, 1, 0, 0, 1, 0)

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
  V(Eager, NoReason)                                     \
  V(Eager, WrongMap)                                     \
  V(Soft, InsufficientTypeFeedbackForGenericKeyedAccess) \
  V(Soft, InsufficientTypeFeedbackForGenericNamedAccess)

#define CACHED_DEOPTIMIZE_IF_LIST(V) \
  V(Eager, DivisionByZero)           \
  V(Eager, Hole)                     \
  V(Eager, MinusZero)                \
  V(Eager, Overflow)                 \
  V(Eager, Smi)

#define CACHED_DEOPTIMIZE_UNLESS_LIST(V) \
  V(Eager, LostPrecision)                \
  V(Eager, LostPrecisionOrNaN)           \
  V(Eager, NoReason)                     \
  V(Eager, NotAHeapNumber)               \
  V(Eager, NotANumberOrOddball)          \
  V(Eager, NotASmi)                      \
  V(Eager, OutOfBounds)                  \
  V(Eager, WrongInstanceType)            \
  V(Eager, WrongMap)

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

  template <BranchHint kBranchHint>
  struct BranchOperator final : public Operator1<BranchHint> {
    BranchOperator()
        : Operator1<BranchHint>(                      // --
              IrOpcode::kBranch, Operator::kKontrol,  // opcode
              "Branch",                               // name
              1, 0, 1, 0, 0, 2,                       // counts
              kBranchHint) {}                         // parameter
  };
  BranchOperator<BranchHint::kNone> kBranchNoneOperator;
  BranchOperator<BranchHint::kTrue> kBranchTrueOperator;
  BranchOperator<BranchHint::kFalse> kBranchFalseOperator;

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
              DeoptimizeParameters(kKind, kReason)) {}   // parameter
  };
#define CACHED_DEOPTIMIZE(Kind, Reason)                                    \
  DeoptimizeOperator<DeoptimizeKind::k##Kind, DeoptimizeReason::k##Reason> \
      kDeoptimize##Kind##Reason##Operator;
  CACHED_DEOPTIMIZE_LIST(CACHED_DEOPTIMIZE)
#undef CACHED_DEOPTIMIZE

  template <DeoptimizeKind kKind, DeoptimizeReason kReason>
  struct DeoptimizeIfOperator final : public Operator1<DeoptimizeParameters> {
    DeoptimizeIfOperator()
        : Operator1<DeoptimizeParameters>(               // --
              IrOpcode::kDeoptimizeIf,                   // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "DeoptimizeIf",                            // name
              2, 1, 1, 0, 1, 1,                          // counts
              DeoptimizeParameters(kKind, kReason)) {}   // parameter
  };
#define CACHED_DEOPTIMIZE_IF(Kind, Reason)                                   \
  DeoptimizeIfOperator<DeoptimizeKind::k##Kind, DeoptimizeReason::k##Reason> \
      kDeoptimizeIf##Kind##Reason##Operator;
  CACHED_DEOPTIMIZE_IF_LIST(CACHED_DEOPTIMIZE_IF)
#undef CACHED_DEOPTIMIZE_IF

  template <DeoptimizeKind kKind, DeoptimizeReason kReason>
  struct DeoptimizeUnlessOperator final
      : public Operator1<DeoptimizeParameters> {
    DeoptimizeUnlessOperator()
        : Operator1<DeoptimizeParameters>(               // --
              IrOpcode::kDeoptimizeUnless,               // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "DeoptimizeUnless",                        // name
              2, 1, 1, 0, 1, 1,                          // counts
              DeoptimizeParameters(kKind, kReason)) {}   // parameter
  };
#define CACHED_DEOPTIMIZE_UNLESS(Kind, Reason)          \
  DeoptimizeUnlessOperator<DeoptimizeKind::k##Kind,     \
                           DeoptimizeReason::k##Reason> \
      kDeoptimizeUnless##Kind##Reason##Operator;
  CACHED_DEOPTIMIZE_UNLESS_LIST(CACHED_DEOPTIMIZE_UNLESS)
#undef CACHED_DEOPTIMIZE_UNLESS

  template <int32_t trap_id>
  struct TrapIfOperator final : public Operator1<int32_t> {
    TrapIfOperator()
        : Operator1<int32_t>(                            // --
              IrOpcode::kTrapIf,                         // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "TrapIf",                                  // name
              1, 1, 1, 0, 0, 1,                          // counts
              trap_id) {}                                // parameter
  };
#define CACHED_TRAP_IF(Trap)                                       \
  TrapIfOperator<static_cast<int32_t>(Builtins::kThrowWasm##Trap)> \
      kTrapIf##Trap##Operator;
  CACHED_TRAP_IF_LIST(CACHED_TRAP_IF)
#undef CACHED_TRAP_IF

  template <int32_t trap_id>
  struct TrapUnlessOperator final : public Operator1<int32_t> {
    TrapUnlessOperator()
        : Operator1<int32_t>(                            // --
              IrOpcode::kTrapUnless,                     // opcode
              Operator::kFoldable | Operator::kNoThrow,  // properties
              "TrapUnless",                              // name
              1, 1, 1, 0, 0, 1,                          // counts
              trap_id) {}                                // parameter
  };
#define CACHED_TRAP_UNLESS(Trap)                                       \
  TrapUnlessOperator<static_cast<int32_t>(Builtins::kThrowWasm##Trap)> \
      kTrapUnless##Trap##Operator;
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

static base::LazyInstance<CommonOperatorGlobalCache>::type
    kCommonOperatorGlobalCache = LAZY_INSTANCE_INITIALIZER;

CommonOperatorBuilder::CommonOperatorBuilder(Zone* zone)
    : cache_(kCommonOperatorGlobalCache.Get()), zone_(zone) {}

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


const Operator* CommonOperatorBuilder::Branch(BranchHint hint) {
  switch (hint) {
    case BranchHint::kNone:
      return &cache_.kBranchNoneOperator;
    case BranchHint::kTrue:
      return &cache_.kBranchTrueOperator;
    case BranchHint::kFalse:
      return &cache_.kBranchFalseOperator;
  }
  UNREACHABLE();
}

const Operator* CommonOperatorBuilder::Deoptimize(DeoptimizeKind kind,
                                                  DeoptimizeReason reason) {
#define CACHED_DEOPTIMIZE(Kind, Reason)                 \
  if (kind == DeoptimizeKind::k##Kind &&                \
      reason == DeoptimizeReason::k##Reason) {          \
    return &cache_.kDeoptimize##Kind##Reason##Operator; \
  }
  CACHED_DEOPTIMIZE_LIST(CACHED_DEOPTIMIZE)
#undef CACHED_DEOPTIMIZE
  // Uncached
  DeoptimizeParameters parameter(kind, reason);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimize,                            // opcodes
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "Deoptimize",                                     // name
      1, 1, 1, 0, 0, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::DeoptimizeIf(DeoptimizeKind kind,
                                                    DeoptimizeReason reason) {
#define CACHED_DEOPTIMIZE_IF(Kind, Reason)                \
  if (kind == DeoptimizeKind::k##Kind &&                  \
      reason == DeoptimizeReason::k##Reason) {            \
    return &cache_.kDeoptimizeIf##Kind##Reason##Operator; \
  }
  CACHED_DEOPTIMIZE_IF_LIST(CACHED_DEOPTIMIZE_IF)
#undef CACHED_DEOPTIMIZE_IF
  // Uncached
  DeoptimizeParameters parameter(kind, reason);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimizeIf,                          // opcode
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "DeoptimizeIf",                                   // name
      2, 1, 1, 0, 1, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::DeoptimizeUnless(
    DeoptimizeKind kind, DeoptimizeReason reason) {
#define CACHED_DEOPTIMIZE_UNLESS(Kind, Reason)                \
  if (kind == DeoptimizeKind::k##Kind &&                      \
      reason == DeoptimizeReason::k##Reason) {                \
    return &cache_.kDeoptimizeUnless##Kind##Reason##Operator; \
  }
  CACHED_DEOPTIMIZE_UNLESS_LIST(CACHED_DEOPTIMIZE_UNLESS)
#undef CACHED_DEOPTIMIZE_UNLESS
  // Uncached
  DeoptimizeParameters parameter(kind, reason);
  return new (zone()) Operator1<DeoptimizeParameters>(  // --
      IrOpcode::kDeoptimizeUnless,                      // opcode
      Operator::kFoldable | Operator::kNoThrow,         // properties
      "DeoptimizeUnless",                               // name
      2, 1, 1, 0, 1, 1,                                 // counts
      parameter);                                       // parameter
}

const Operator* CommonOperatorBuilder::TrapIf(int32_t trap_id) {
  switch (trap_id) {
#define CACHED_TRAP_IF(Trap)       \
  case Builtins::kThrowWasm##Trap: \
    return &cache_.kTrapIf##Trap##Operator;
    CACHED_TRAP_IF_LIST(CACHED_TRAP_IF)
#undef CACHED_TRAP_IF
    default:
      break;
  }
  // Uncached
  return new (zone()) Operator1<int>(            // --
      IrOpcode::kTrapIf,                         // opcode
      Operator::kFoldable | Operator::kNoThrow,  // properties
      "TrapIf",                                  // name
      1, 1, 1, 0, 0, 1,                          // counts
      trap_id);                                  // parameter
}

const Operator* CommonOperatorBuilder::TrapUnless(int32_t trap_id) {
  switch (trap_id) {
#define CACHED_TRAP_UNLESS(Trap)   \
  case Builtins::kThrowWasm##Trap: \
    return &cache_.kTrapUnless##Trap##Operator;
    CACHED_TRAP_UNLESS_LIST(CACHED_TRAP_UNLESS)
#undef CACHED_TRAP_UNLESS
    default:
      break;
  }
  // Uncached
  return new (zone()) Operator1<int>(            // --
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


const Operator* CommonOperatorBuilder::IfValue(int32_t index) {
  return new (zone()) Operator1<int32_t>(      // --
      IrOpcode::kIfValue, Operator::kKontrol,  // opcode
      "IfValue",                               // name
      0, 0, 1, 0, 0, 1,                        // counts
      index);                                  // parameter
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

const Operator* CommonOperatorBuilder::TypeGuard(Type* type) {
  return new (zone()) Operator1<Type*>(       // --
      IrOpcode::kTypeGuard, Operator::kPure,  // opcode
      "TypeGuard",                            // name
      1, 0, 1, 1, 0, 0,                       // counts
      type);                                  // parameter
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


const Operator* CommonOperatorBuilder::Call(const CallDescriptor* descriptor) {
  class CallOperator final : public Operator1<const CallDescriptor*> {
   public:
    explicit CallOperator(const CallDescriptor* descriptor)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kCall, descriptor->properties(), "Call",
              descriptor->InputCount() + descriptor->FrameStateCount(),
              Operator::ZeroIfPure(descriptor->properties()),
              Operator::ZeroIfEliminatable(descriptor->properties()),
              descriptor->ReturnCount(),
              Operator::ZeroIfPure(descriptor->properties()),
              Operator::ZeroIfNoThrow(descriptor->properties()), descriptor) {}

    void PrintParameter(std::ostream& os, PrintVerbosity verbose) const {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) CallOperator(descriptor);
}

const Operator* CommonOperatorBuilder::CallWithCallerSavedRegisters(
    const CallDescriptor* descriptor) {
  class CallOperator final : public Operator1<const CallDescriptor*> {
   public:
    explicit CallOperator(const CallDescriptor* descriptor)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kCallWithCallerSavedRegisters, descriptor->properties(),
              "CallWithCallerSavedRegisters",
              descriptor->InputCount() + descriptor->FrameStateCount(),
              Operator::ZeroIfPure(descriptor->properties()),
              Operator::ZeroIfEliminatable(descriptor->properties()),
              descriptor->ReturnCount(),
              Operator::ZeroIfPure(descriptor->properties()),
              Operator::ZeroIfNoThrow(descriptor->properties()), descriptor) {}

    void PrintParameter(std::ostream& os, PrintVerbosity verbose) const {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) CallOperator(descriptor);
}

const Operator* CommonOperatorBuilder::TailCall(
    const CallDescriptor* descriptor) {
  class TailCallOperator final : public Operator1<const CallDescriptor*> {
   public:
    explicit TailCallOperator(const CallDescriptor* descriptor)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kTailCall,
              descriptor->properties() | Operator::kNoThrow, "TailCall",
              descriptor->InputCount() + descriptor->FrameStateCount(), 1, 1, 0,
              0, 1, descriptor) {}

    void PrintParameter(std::ostream& os, PrintVerbosity verbose) const {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) TailCallOperator(descriptor);
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
