// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator.h"

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/handles/handles-inl.h"
#include "src/objects/map.h"
#include "src/objects/name.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

size_t hash_value(BaseTaggedness base_taggedness) {
  return static_cast<uint8_t>(base_taggedness);
}

std::ostream& operator<<(std::ostream& os, BaseTaggedness base_taggedness) {
  switch (base_taggedness) {
    case kUntaggedBase:
      return os << "untagged base";
    case kTaggedBase:
      return os << "tagged base";
  }
  UNREACHABLE();
}

bool operator==(FieldAccess const& lhs, FieldAccess const& rhs) {
  // On purpose we don't include the write barrier kind here, as this method is
  // really only relevant for eliminating loads and they don't care about the
  // write barrier mode.
  return lhs.base_is_tagged == rhs.base_is_tagged && lhs.offset == rhs.offset &&
         lhs.map.address() == rhs.map.address() &&
         lhs.machine_type == rhs.machine_type;
}

size_t hash_value(FieldAccess const& access) {
  // On purpose we don't include the write barrier kind here, as this method is
  // really only relevant for eliminating loads and they don't care about the
  // write barrier mode.
  return base::hash_combine(access.base_is_tagged, access.offset,
                            access.machine_type);
}

size_t hash_value(LoadSensitivity load_sensitivity) {
  return static_cast<size_t>(load_sensitivity);
}

std::ostream& operator<<(std::ostream& os, LoadSensitivity load_sensitivity) {
  switch (load_sensitivity) {
    case LoadSensitivity::kCritical:
      return os << "Critical";
    case LoadSensitivity::kSafe:
      return os << "Safe";
    case LoadSensitivity::kUnsafe:
      return os << "Unsafe";
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, FieldAccess const& access) {
  os << "[" << access.base_is_tagged << ", " << access.offset << ", ";
#ifdef OBJECT_PRINT
  Handle<Name> name;
  if (access.name.ToHandle(&name)) {
    name->NamePrint(os);
    os << ", ";
  }
  Handle<Map> map;
  if (access.map.ToHandle(&map)) {
    os << Brief(*map) << ", ";
  }
#endif
  os << access.type << ", " << access.machine_type << ", "
     << access.write_barrier_kind << ", " << access.constness;
  if (FLAG_untrusted_code_mitigations) {
    os << ", " << access.load_sensitivity;
  }
  os << "]";
  return os;
}

template <>
void Operator1<FieldAccess>::PrintParameter(std::ostream& os,
                                            PrintVerbosity verbose) const {
  if (verbose == PrintVerbosity::kVerbose) {
    os << parameter();
  } else {
    os << "[+" << parameter().offset << "]";
  }
}

bool operator==(ElementAccess const& lhs, ElementAccess const& rhs) {
  // On purpose we don't include the write barrier kind here, as this method is
  // really only relevant for eliminating loads and they don't care about the
  // write barrier mode.
  return lhs.base_is_tagged == rhs.base_is_tagged &&
         lhs.header_size == rhs.header_size &&
         lhs.machine_type == rhs.machine_type;
}

size_t hash_value(ElementAccess const& access) {
  // On purpose we don't include the write barrier kind here, as this method is
  // really only relevant for eliminating loads and they don't care about the
  // write barrier mode.
  return base::hash_combine(access.base_is_tagged, access.header_size,
                            access.machine_type);
}

std::ostream& operator<<(std::ostream& os, ElementAccess const& access) {
  os << access.base_is_tagged << ", " << access.header_size << ", "
     << access.type << ", " << access.machine_type << ", "
     << access.write_barrier_kind;
  if (FLAG_untrusted_code_mitigations) {
    os << ", " << access.load_sensitivity;
  }
  return os;
}

bool operator==(ObjectAccess const& lhs, ObjectAccess const& rhs) {
  return lhs.machine_type == rhs.machine_type &&
         lhs.write_barrier_kind == rhs.write_barrier_kind;
}

size_t hash_value(ObjectAccess const& access) {
  return base::hash_combine(access.machine_type, access.write_barrier_kind);
}

std::ostream& operator<<(std::ostream& os, ObjectAccess const& access) {
  os << access.machine_type << ", " << access.write_barrier_kind;
  return os;
}

const FieldAccess& FieldAccessOf(const Operator* op) {
  DCHECK_NOT_NULL(op);
  DCHECK(op->opcode() == IrOpcode::kLoadField ||
         op->opcode() == IrOpcode::kStoreField);
  return OpParameter<FieldAccess>(op);
}

const ElementAccess& ElementAccessOf(const Operator* op) {
  DCHECK_NOT_NULL(op);
  DCHECK(op->opcode() == IrOpcode::kLoadElement ||
         op->opcode() == IrOpcode::kStoreElement);
  return OpParameter<ElementAccess>(op);
}

const ObjectAccess& ObjectAccessOf(const Operator* op) {
  DCHECK_NOT_NULL(op);
  DCHECK(op->opcode() == IrOpcode::kLoadFromObject ||
         op->opcode() == IrOpcode::kStoreToObject);
  return OpParameter<ObjectAccess>(op);
}

ExternalArrayType ExternalArrayTypeOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kLoadTypedElement ||
         op->opcode() == IrOpcode::kLoadDataViewElement ||
         op->opcode() == IrOpcode::kStoreTypedElement ||
         op->opcode() == IrOpcode::kStoreDataViewElement);
  return OpParameter<ExternalArrayType>(op);
}

ConvertReceiverMode ConvertReceiverModeOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kConvertReceiver, op->opcode());
  return OpParameter<ConvertReceiverMode>(op);
}

size_t hash_value(CheckFloat64HoleMode mode) {
  return static_cast<size_t>(mode);
}

std::ostream& operator<<(std::ostream& os, CheckFloat64HoleMode mode) {
  switch (mode) {
    case CheckFloat64HoleMode::kAllowReturnHole:
      return os << "allow-return-hole";
    case CheckFloat64HoleMode::kNeverReturnHole:
      return os << "never-return-hole";
  }
  UNREACHABLE();
}

CheckFloat64HoleParameters const& CheckFloat64HoleParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckFloat64Hole, op->opcode());
  return OpParameter<CheckFloat64HoleParameters>(op);
}

std::ostream& operator<<(std::ostream& os,
                         CheckFloat64HoleParameters const& params) {
  os << params.mode();
  if (params.feedback().IsValid()) os << "; " << params.feedback();
  return os;
}

size_t hash_value(const CheckFloat64HoleParameters& params) {
  return base::hash_combine(params.mode(), params.feedback());
}

bool operator==(CheckFloat64HoleParameters const& lhs,
                CheckFloat64HoleParameters const& rhs) {
  return lhs.mode() == rhs.mode() && lhs.feedback() == rhs.feedback();
}

bool operator!=(CheckFloat64HoleParameters const& lhs,
                CheckFloat64HoleParameters const& rhs) {
  return !(lhs == rhs);
}

CheckForMinusZeroMode CheckMinusZeroModeOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kChangeFloat64ToTagged ||
         op->opcode() == IrOpcode::kCheckedInt32Mul);
  return OpParameter<CheckForMinusZeroMode>(op);
}

size_t hash_value(CheckForMinusZeroMode mode) {
  return static_cast<size_t>(mode);
}

std::ostream& operator<<(std::ostream& os, CheckForMinusZeroMode mode) {
  switch (mode) {
    case CheckForMinusZeroMode::kCheckForMinusZero:
      return os << "check-for-minus-zero";
    case CheckForMinusZeroMode::kDontCheckForMinusZero:
      return os << "dont-check-for-minus-zero";
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, CheckMapsFlags flags) {
  bool empty = true;
  if (flags & CheckMapsFlag::kTryMigrateInstance) {
    os << "TryMigrateInstance";
    empty = false;
  }
  if (empty) os << "None";
  return os;
}

bool operator==(CheckMapsParameters const& lhs,
                CheckMapsParameters const& rhs) {
  return lhs.flags() == rhs.flags() && lhs.maps() == rhs.maps() &&
         lhs.feedback() == rhs.feedback();
}

size_t hash_value(CheckMapsParameters const& p) {
  return base::hash_combine(p.flags(), p.maps(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, CheckMapsParameters const& p) {
  os << p.flags() << p.maps();
  if (p.feedback().IsValid()) {
    os << "; " << p.feedback();
  }
  return os;
}

CheckMapsParameters const& CheckMapsParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckMaps, op->opcode());
  return OpParameter<CheckMapsParameters>(op);
}

ZoneHandleSet<Map> const& CompareMapsParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCompareMaps, op->opcode());
  return OpParameter<ZoneHandleSet<Map>>(op);
}

ZoneHandleSet<Map> const& MapGuardMapsOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kMapGuard, op->opcode());
  return OpParameter<ZoneHandleSet<Map>>(op);
}

size_t hash_value(CheckTaggedInputMode mode) {
  return static_cast<size_t>(mode);
}

std::ostream& operator<<(std::ostream& os, CheckTaggedInputMode mode) {
  switch (mode) {
    case CheckTaggedInputMode::kNumber:
      return os << "Number";
    case CheckTaggedInputMode::kNumberOrOddball:
      return os << "NumberOrOddball";
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, GrowFastElementsMode mode) {
  switch (mode) {
    case GrowFastElementsMode::kDoubleElements:
      return os << "DoubleElements";
    case GrowFastElementsMode::kSmiOrObjectElements:
      return os << "SmiOrObjectElements";
  }
  UNREACHABLE();
}

bool operator==(const GrowFastElementsParameters& lhs,
                const GrowFastElementsParameters& rhs) {
  return lhs.mode() == rhs.mode() && lhs.feedback() == rhs.feedback();
}

inline size_t hash_value(const GrowFastElementsParameters& params) {
  return base::hash_combine(params.mode(), params.feedback());
}

std::ostream& operator<<(std::ostream& os,
                         const GrowFastElementsParameters& params) {
  os << params.mode();
  if (params.feedback().IsValid()) {
    os << params.feedback();
  }
  return os;
}

const GrowFastElementsParameters& GrowFastElementsParametersOf(
    const Operator* op) {
  DCHECK_EQ(IrOpcode::kMaybeGrowFastElements, op->opcode());
  return OpParameter<GrowFastElementsParameters>(op);
}

bool operator==(ElementsTransition const& lhs, ElementsTransition const& rhs) {
  return lhs.mode() == rhs.mode() &&
         lhs.source().address() == rhs.source().address() &&
         lhs.target().address() == rhs.target().address();
}

size_t hash_value(ElementsTransition transition) {
  return base::hash_combine(static_cast<uint8_t>(transition.mode()),
                            transition.source().address(),
                            transition.target().address());
}

std::ostream& operator<<(std::ostream& os, ElementsTransition transition) {
  switch (transition.mode()) {
    case ElementsTransition::kFastTransition:
      return os << "fast-transition from " << Brief(*transition.source())
                << " to " << Brief(*transition.target());
    case ElementsTransition::kSlowTransition:
      return os << "slow-transition from " << Brief(*transition.source())
                << " to " << Brief(*transition.target());
  }
  UNREACHABLE();
}

ElementsTransition const& ElementsTransitionOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kTransitionElementsKind, op->opcode());
  return OpParameter<ElementsTransition>(op);
}

namespace {

// Parameters for the TransitionAndStoreElement opcode.
class TransitionAndStoreElementParameters final {
 public:
  TransitionAndStoreElementParameters(Handle<Map> double_map,
                                      Handle<Map> fast_map);

  Handle<Map> double_map() const { return double_map_; }
  Handle<Map> fast_map() const { return fast_map_; }

 private:
  Handle<Map> const double_map_;
  Handle<Map> const fast_map_;
};

TransitionAndStoreElementParameters::TransitionAndStoreElementParameters(
    Handle<Map> double_map, Handle<Map> fast_map)
    : double_map_(double_map), fast_map_(fast_map) {}

bool operator==(TransitionAndStoreElementParameters const& lhs,
                TransitionAndStoreElementParameters const& rhs) {
  return lhs.fast_map().address() == rhs.fast_map().address() &&
         lhs.double_map().address() == rhs.double_map().address();
}

size_t hash_value(TransitionAndStoreElementParameters parameters) {
  return base::hash_combine(parameters.fast_map().address(),
                            parameters.double_map().address());
}

std::ostream& operator<<(std::ostream& os,
                         TransitionAndStoreElementParameters parameters) {
  return os << "fast-map" << Brief(*parameters.fast_map()) << " double-map"
            << Brief(*parameters.double_map());
}

}  // namespace

namespace {

// Parameters for the TransitionAndStoreNonNumberElement opcode.
class TransitionAndStoreNonNumberElementParameters final {
 public:
  TransitionAndStoreNonNumberElementParameters(Handle<Map> fast_map,
                                               Type value_type);

  Handle<Map> fast_map() const { return fast_map_; }
  Type value_type() const { return value_type_; }

 private:
  Handle<Map> const fast_map_;
  Type value_type_;
};

TransitionAndStoreNonNumberElementParameters::
    TransitionAndStoreNonNumberElementParameters(Handle<Map> fast_map,
                                                 Type value_type)
    : fast_map_(fast_map), value_type_(value_type) {}

bool operator==(TransitionAndStoreNonNumberElementParameters const& lhs,
                TransitionAndStoreNonNumberElementParameters const& rhs) {
  return lhs.fast_map().address() == rhs.fast_map().address() &&
         lhs.value_type() == rhs.value_type();
}

size_t hash_value(TransitionAndStoreNonNumberElementParameters parameters) {
  return base::hash_combine(parameters.fast_map().address(),
                            parameters.value_type());
}

std::ostream& operator<<(
    std::ostream& os, TransitionAndStoreNonNumberElementParameters parameters) {
  return os << parameters.value_type() << ", fast-map"
            << Brief(*parameters.fast_map());
}

}  // namespace

namespace {

// Parameters for the TransitionAndStoreNumberElement opcode.
class TransitionAndStoreNumberElementParameters final {
 public:
  explicit TransitionAndStoreNumberElementParameters(Handle<Map> double_map);

  Handle<Map> double_map() const { return double_map_; }

 private:
  Handle<Map> const double_map_;
};

TransitionAndStoreNumberElementParameters::
    TransitionAndStoreNumberElementParameters(Handle<Map> double_map)
    : double_map_(double_map) {}

bool operator==(TransitionAndStoreNumberElementParameters const& lhs,
                TransitionAndStoreNumberElementParameters const& rhs) {
  return lhs.double_map().address() == rhs.double_map().address();
}

size_t hash_value(TransitionAndStoreNumberElementParameters parameters) {
  return base::hash_combine(parameters.double_map().address());
}

std::ostream& operator<<(std::ostream& os,
                         TransitionAndStoreNumberElementParameters parameters) {
  return os << "double-map" << Brief(*parameters.double_map());
}

}  // namespace

Handle<Map> DoubleMapParameterOf(const Operator* op) {
  if (op->opcode() == IrOpcode::kTransitionAndStoreElement) {
    return OpParameter<TransitionAndStoreElementParameters>(op).double_map();
  } else if (op->opcode() == IrOpcode::kTransitionAndStoreNumberElement) {
    return OpParameter<TransitionAndStoreNumberElementParameters>(op)
        .double_map();
  }
  UNREACHABLE();
  return Handle<Map>::null();
}

Type ValueTypeParameterOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kTransitionAndStoreNonNumberElement, op->opcode());
  return OpParameter<TransitionAndStoreNonNumberElementParameters>(op)
      .value_type();
}

Handle<Map> FastMapParameterOf(const Operator* op) {
  if (op->opcode() == IrOpcode::kTransitionAndStoreElement) {
    return OpParameter<TransitionAndStoreElementParameters>(op).fast_map();
  } else if (op->opcode() == IrOpcode::kTransitionAndStoreNonNumberElement) {
    return OpParameter<TransitionAndStoreNonNumberElementParameters>(op)
        .fast_map();
  }
  UNREACHABLE();
  return Handle<Map>::null();
}

std::ostream& operator<<(std::ostream& os, BigIntOperationHint hint) {
  switch (hint) {
    case BigIntOperationHint::kBigInt:
      return os << "BigInt";
  }
  UNREACHABLE();
}

size_t hash_value(BigIntOperationHint hint) {
  return static_cast<uint8_t>(hint);
}

std::ostream& operator<<(std::ostream& os, NumberOperationHint hint) {
  switch (hint) {
    case NumberOperationHint::kSignedSmall:
      return os << "SignedSmall";
    case NumberOperationHint::kSignedSmallInputs:
      return os << "SignedSmallInputs";
    case NumberOperationHint::kSigned32:
      return os << "Signed32";
    case NumberOperationHint::kNumber:
      return os << "Number";
    case NumberOperationHint::kNumberOrOddball:
      return os << "NumberOrOddball";
  }
  UNREACHABLE();
}

size_t hash_value(NumberOperationHint hint) {
  return static_cast<uint8_t>(hint);
}

NumberOperationHint NumberOperationHintOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kSpeculativeNumberAdd ||
         op->opcode() == IrOpcode::kSpeculativeNumberSubtract ||
         op->opcode() == IrOpcode::kSpeculativeNumberMultiply ||
         op->opcode() == IrOpcode::kSpeculativeNumberDivide ||
         op->opcode() == IrOpcode::kSpeculativeNumberModulus ||
         op->opcode() == IrOpcode::kSpeculativeNumberShiftLeft ||
         op->opcode() == IrOpcode::kSpeculativeNumberShiftRight ||
         op->opcode() == IrOpcode::kSpeculativeNumberShiftRightLogical ||
         op->opcode() == IrOpcode::kSpeculativeNumberBitwiseAnd ||
         op->opcode() == IrOpcode::kSpeculativeNumberBitwiseOr ||
         op->opcode() == IrOpcode::kSpeculativeNumberBitwiseXor ||
         op->opcode() == IrOpcode::kSpeculativeNumberEqual ||
         op->opcode() == IrOpcode::kSpeculativeNumberLessThan ||
         op->opcode() == IrOpcode::kSpeculativeNumberLessThanOrEqual ||
         op->opcode() == IrOpcode::kSpeculativeSafeIntegerAdd ||
         op->opcode() == IrOpcode::kSpeculativeSafeIntegerSubtract);
  return OpParameter<NumberOperationHint>(op);
}

bool operator==(NumberOperationParameters const& lhs,
                NumberOperationParameters const& rhs) {
  return lhs.hint() == rhs.hint() && lhs.feedback() == rhs.feedback();
}

size_t hash_value(NumberOperationParameters const& p) {
  return base::hash_combine(p.hint(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, NumberOperationParameters const& p) {
  return os << p.hint() << " " << p.feedback();
}

NumberOperationParameters const& NumberOperationParametersOf(
    Operator const* op) {
  DCHECK_EQ(IrOpcode::kSpeculativeToNumber, op->opcode());
  return OpParameter<NumberOperationParameters>(op);
}

size_t hash_value(AllocateParameters info) {
  return base::hash_combine(info.type(),
                            static_cast<int>(info.allocation_type()));
}

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           AllocateParameters info) {
  return os << info.type() << ", " << info.allocation_type();
}

bool operator==(AllocateParameters const& lhs, AllocateParameters const& rhs) {
  return lhs.allocation_type() == rhs.allocation_type() &&
         lhs.type() == rhs.type();
}

const AllocateParameters& AllocateParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kAllocate ||
         op->opcode() == IrOpcode::kAllocateRaw);
  return OpParameter<AllocateParameters>(op);
}

AllocationType AllocationTypeOf(const Operator* op) {
  if (op->opcode() == IrOpcode::kNewDoubleElements ||
      op->opcode() == IrOpcode::kNewSmiOrObjectElements) {
    return OpParameter<AllocationType>(op);
  }
  return AllocateParametersOf(op).allocation_type();
}

Type AllocateTypeOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kAllocate, op->opcode());
  return AllocateParametersOf(op).type();
}

AbortReason AbortReasonOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kRuntimeAbort, op->opcode());
  return static_cast<AbortReason>(OpParameter<int>(op));
}

const CheckTaggedInputParameters& CheckTaggedInputParametersOf(
    const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kCheckedTruncateTaggedToWord32 ||
         op->opcode() == IrOpcode::kCheckedTaggedToFloat64);
  return OpParameter<CheckTaggedInputParameters>(op);
}

std::ostream& operator<<(std::ostream& os,
                         const CheckTaggedInputParameters& params) {
  os << params.mode();
  if (params.feedback().IsValid()) {
    os << "; " << params.feedback();
  }
  return os;
}

size_t hash_value(const CheckTaggedInputParameters& params) {
  return base::hash_combine(params.mode(), params.feedback());
}

bool operator==(CheckTaggedInputParameters const& lhs,
                CheckTaggedInputParameters const& rhs) {
  return lhs.mode() == rhs.mode() && lhs.feedback() == rhs.feedback();
}

const CheckMinusZeroParameters& CheckMinusZeroParametersOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kCheckedTaggedToInt32 ||
         op->opcode() == IrOpcode::kCheckedTaggedToInt64 ||
         op->opcode() == IrOpcode::kCheckedFloat64ToInt32 ||
         op->opcode() == IrOpcode::kCheckedFloat64ToInt64);
  return OpParameter<CheckMinusZeroParameters>(op);
}

std::ostream& operator<<(std::ostream& os,
                         const CheckMinusZeroParameters& params) {
  os << params.mode();
  if (params.feedback().IsValid()) {
    os << "; " << params.feedback();
  }
  return os;
}

size_t hash_value(const CheckMinusZeroParameters& params) {
  return base::hash_combine(params.mode(), params.feedback());
}

bool operator==(CheckMinusZeroParameters const& lhs,
                CheckMinusZeroParameters const& rhs) {
  return lhs.mode() == rhs.mode() && lhs.feedback() == rhs.feedback();
}

#define PURE_OP_LIST(V)                                            \
  V(BooleanNot, Operator::kNoProperties, 1, 0)                     \
  V(NumberEqual, Operator::kCommutative, 2, 0)                     \
  V(NumberLessThan, Operator::kNoProperties, 2, 0)                 \
  V(NumberLessThanOrEqual, Operator::kNoProperties, 2, 0)          \
  V(NumberAdd, Operator::kCommutative, 2, 0)                       \
  V(NumberSubtract, Operator::kNoProperties, 2, 0)                 \
  V(NumberMultiply, Operator::kCommutative, 2, 0)                  \
  V(NumberDivide, Operator::kNoProperties, 2, 0)                   \
  V(NumberModulus, Operator::kNoProperties, 2, 0)                  \
  V(NumberBitwiseOr, Operator::kCommutative, 2, 0)                 \
  V(NumberBitwiseXor, Operator::kCommutative, 2, 0)                \
  V(NumberBitwiseAnd, Operator::kCommutative, 2, 0)                \
  V(NumberShiftLeft, Operator::kNoProperties, 2, 0)                \
  V(NumberShiftRight, Operator::kNoProperties, 2, 0)               \
  V(NumberShiftRightLogical, Operator::kNoProperties, 2, 0)        \
  V(NumberImul, Operator::kCommutative, 2, 0)                      \
  V(NumberAbs, Operator::kNoProperties, 1, 0)                      \
  V(NumberClz32, Operator::kNoProperties, 1, 0)                    \
  V(NumberCeil, Operator::kNoProperties, 1, 0)                     \
  V(NumberFloor, Operator::kNoProperties, 1, 0)                    \
  V(NumberFround, Operator::kNoProperties, 1, 0)                   \
  V(NumberAcos, Operator::kNoProperties, 1, 0)                     \
  V(NumberAcosh, Operator::kNoProperties, 1, 0)                    \
  V(NumberAsin, Operator::kNoProperties, 1, 0)                     \
  V(NumberAsinh, Operator::kNoProperties, 1, 0)                    \
  V(NumberAtan, Operator::kNoProperties, 1, 0)                     \
  V(NumberAtan2, Operator::kNoProperties, 2, 0)                    \
  V(NumberAtanh, Operator::kNoProperties, 1, 0)                    \
  V(NumberCbrt, Operator::kNoProperties, 1, 0)                     \
  V(NumberCos, Operator::kNoProperties, 1, 0)                      \
  V(NumberCosh, Operator::kNoProperties, 1, 0)                     \
  V(NumberExp, Operator::kNoProperties, 1, 0)                      \
  V(NumberExpm1, Operator::kNoProperties, 1, 0)                    \
  V(NumberLog, Operator::kNoProperties, 1, 0)                      \
  V(NumberLog1p, Operator::kNoProperties, 1, 0)                    \
  V(NumberLog10, Operator::kNoProperties, 1, 0)                    \
  V(NumberLog2, Operator::kNoProperties, 1, 0)                     \
  V(NumberMax, Operator::kNoProperties, 2, 0)                      \
  V(NumberMin, Operator::kNoProperties, 2, 0)                      \
  V(NumberPow, Operator::kNoProperties, 2, 0)                      \
  V(NumberRound, Operator::kNoProperties, 1, 0)                    \
  V(NumberSign, Operator::kNoProperties, 1, 0)                     \
  V(NumberSin, Operator::kNoProperties, 1, 0)                      \
  V(NumberSinh, Operator::kNoProperties, 1, 0)                     \
  V(NumberSqrt, Operator::kNoProperties, 1, 0)                     \
  V(NumberTan, Operator::kNoProperties, 1, 0)                      \
  V(NumberTanh, Operator::kNoProperties, 1, 0)                     \
  V(NumberTrunc, Operator::kNoProperties, 1, 0)                    \
  V(NumberToBoolean, Operator::kNoProperties, 1, 0)                \
  V(NumberToInt32, Operator::kNoProperties, 1, 0)                  \
  V(NumberToString, Operator::kNoProperties, 1, 0)                 \
  V(NumberToUint32, Operator::kNoProperties, 1, 0)                 \
  V(NumberToUint8Clamped, Operator::kNoProperties, 1, 0)           \
  V(NumberSilenceNaN, Operator::kNoProperties, 1, 0)               \
  V(BigIntNegate, Operator::kNoProperties, 1, 0)                   \
  V(StringConcat, Operator::kNoProperties, 3, 0)                   \
  V(StringToNumber, Operator::kNoProperties, 1, 0)                 \
  V(StringFromSingleCharCode, Operator::kNoProperties, 1, 0)       \
  V(StringFromSingleCodePoint, Operator::kNoProperties, 1, 0)      \
  V(StringIndexOf, Operator::kNoProperties, 3, 0)                  \
  V(StringLength, Operator::kNoProperties, 1, 0)                   \
  V(StringToLowerCaseIntl, Operator::kNoProperties, 1, 0)          \
  V(StringToUpperCaseIntl, Operator::kNoProperties, 1, 0)          \
  V(TypeOf, Operator::kNoProperties, 1, 1)                         \
  V(PlainPrimitiveToNumber, Operator::kNoProperties, 1, 0)         \
  V(PlainPrimitiveToWord32, Operator::kNoProperties, 1, 0)         \
  V(PlainPrimitiveToFloat64, Operator::kNoProperties, 1, 0)        \
  V(ChangeCompressedSignedToInt32, Operator::kNoProperties, 1, 0)  \
  V(ChangeTaggedSignedToInt32, Operator::kNoProperties, 1, 0)      \
  V(ChangeTaggedSignedToInt64, Operator::kNoProperties, 1, 0)      \
  V(ChangeTaggedToInt32, Operator::kNoProperties, 1, 0)            \
  V(ChangeTaggedToInt64, Operator::kNoProperties, 1, 0)            \
  V(ChangeTaggedToUint32, Operator::kNoProperties, 1, 0)           \
  V(ChangeTaggedToFloat64, Operator::kNoProperties, 1, 0)          \
  V(ChangeTaggedToTaggedSigned, Operator::kNoProperties, 1, 0)     \
  V(ChangeCompressedToTaggedSigned, Operator::kNoProperties, 1, 0) \
  V(ChangeTaggedToCompressedSigned, Operator::kNoProperties, 1, 0) \
  V(ChangeFloat64ToTaggedPointer, Operator::kNoProperties, 1, 0)   \
  V(ChangeInt31ToCompressedSigned, Operator::kNoProperties, 1, 0)  \
  V(ChangeInt31ToTaggedSigned, Operator::kNoProperties, 1, 0)      \
  V(ChangeInt32ToTagged, Operator::kNoProperties, 1, 0)            \
  V(ChangeInt64ToTagged, Operator::kNoProperties, 1, 0)            \
  V(ChangeUint32ToTagged, Operator::kNoProperties, 1, 0)           \
  V(ChangeUint64ToTagged, Operator::kNoProperties, 1, 0)           \
  V(ChangeTaggedToBit, Operator::kNoProperties, 1, 0)              \
  V(ChangeBitToTagged, Operator::kNoProperties, 1, 0)              \
  V(TruncateBigIntToUint64, Operator::kNoProperties, 1, 0)         \
  V(ChangeUint64ToBigInt, Operator::kNoProperties, 1, 0)           \
  V(TruncateTaggedToBit, Operator::kNoProperties, 1, 0)            \
  V(TruncateTaggedPointerToBit, Operator::kNoProperties, 1, 0)     \
  V(TruncateTaggedToWord32, Operator::kNoProperties, 1, 0)         \
  V(TruncateTaggedToFloat64, Operator::kNoProperties, 1, 0)        \
  V(ObjectIsArrayBufferView, Operator::kNoProperties, 1, 0)        \
  V(ObjectIsBigInt, Operator::kNoProperties, 1, 0)                 \
  V(ObjectIsCallable, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsConstructor, Operator::kNoProperties, 1, 0)            \
  V(ObjectIsDetectableCallable, Operator::kNoProperties, 1, 0)     \
  V(ObjectIsMinusZero, Operator::kNoProperties, 1, 0)              \
  V(NumberIsMinusZero, Operator::kNoProperties, 1, 0)              \
  V(ObjectIsNaN, Operator::kNoProperties, 1, 0)                    \
  V(NumberIsNaN, Operator::kNoProperties, 1, 0)                    \
  V(ObjectIsNonCallable, Operator::kNoProperties, 1, 0)            \
  V(ObjectIsNumber, Operator::kNoProperties, 1, 0)                 \
  V(ObjectIsReceiver, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsSmi, Operator::kNoProperties, 1, 0)                    \
  V(ObjectIsString, Operator::kNoProperties, 1, 0)                 \
  V(ObjectIsSymbol, Operator::kNoProperties, 1, 0)                 \
  V(ObjectIsUndetectable, Operator::kNoProperties, 1, 0)           \
  V(NumberIsFloat64Hole, Operator::kNoProperties, 1, 0)            \
  V(NumberIsFinite, Operator::kNoProperties, 1, 0)                 \
  V(ObjectIsFiniteNumber, Operator::kNoProperties, 1, 0)           \
  V(NumberIsInteger, Operator::kNoProperties, 1, 0)                \
  V(ObjectIsSafeInteger, Operator::kNoProperties, 1, 0)            \
  V(NumberIsSafeInteger, Operator::kNoProperties, 1, 0)            \
  V(ObjectIsInteger, Operator::kNoProperties, 1, 0)                \
  V(ConvertTaggedHoleToUndefined, Operator::kNoProperties, 1, 0)   \
  V(SameValue, Operator::kCommutative, 2, 0)                       \
  V(SameValueNumbersOnly, Operator::kCommutative, 2, 0)            \
  V(NumberSameValue, Operator::kCommutative, 2, 0)                 \
  V(ReferenceEqual, Operator::kCommutative, 2, 0)                  \
  V(StringEqual, Operator::kCommutative, 2, 0)                     \
  V(StringLessThan, Operator::kNoProperties, 2, 0)                 \
  V(StringLessThanOrEqual, Operator::kNoProperties, 2, 0)          \
  V(ToBoolean, Operator::kNoProperties, 1, 0)                      \
  V(NewConsString, Operator::kNoProperties, 3, 0)                  \
  V(PoisonIndex, Operator::kNoProperties, 1, 0)

#define EFFECT_DEPENDENT_OP_LIST(V)                       \
  V(BigIntAdd, Operator::kNoProperties, 2, 1)             \
  V(StringCharCodeAt, Operator::kNoProperties, 2, 1)      \
  V(StringCodePointAt, Operator::kNoProperties, 2, 1)     \
  V(StringFromCodePointAt, Operator::kNoProperties, 2, 1) \
  V(StringSubstring, Operator::kNoProperties, 3, 1)       \
  V(DateNow, Operator::kNoProperties, 0, 1)

#define SPECULATIVE_NUMBER_BINOP_LIST(V)      \
  SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(V) \
  V(SpeculativeNumberEqual)                   \
  V(SpeculativeNumberLessThan)                \
  V(SpeculativeNumberLessThanOrEqual)

#define CHECKED_OP_LIST(V)                \
  V(CheckEqualsInternalizedString, 2, 0)  \
  V(CheckEqualsSymbol, 2, 0)              \
  V(CheckHeapObject, 1, 1)                \
  V(CheckInternalizedString, 1, 1)        \
  V(CheckNotTaggedHole, 1, 1)             \
  V(CheckReceiver, 1, 1)                  \
  V(CheckReceiverOrNullOrUndefined, 1, 1) \
  V(CheckSymbol, 1, 1)                    \
  V(CheckedInt32Add, 2, 1)                \
  V(CheckedInt32Div, 2, 1)                \
  V(CheckedInt32Mod, 2, 1)                \
  V(CheckedInt32Sub, 2, 1)                \
  V(CheckedUint32Div, 2, 1)               \
  V(CheckedUint32Mod, 2, 1)

#define CHECKED_WITH_FEEDBACK_OP_LIST(V)    \
  V(CheckBounds, 2, 1)                      \
  V(CheckNumber, 1, 1)                      \
  V(CheckSmi, 1, 1)                         \
  V(CheckString, 1, 1)                      \
  V(CheckBigInt, 1, 1)                      \
  V(CheckedInt32ToCompressedSigned, 1, 1)   \
  V(CheckedInt32ToTaggedSigned, 1, 1)       \
  V(CheckedInt64ToInt32, 1, 1)              \
  V(CheckedInt64ToTaggedSigned, 1, 1)       \
  V(CheckedTaggedSignedToInt32, 1, 1)       \
  V(CheckedTaggedToTaggedPointer, 1, 1)     \
  V(CheckedTaggedToTaggedSigned, 1, 1)      \
  V(CheckedCompressedToTaggedPointer, 1, 1) \
  V(CheckedCompressedToTaggedSigned, 1, 1)  \
  V(CheckedTaggedToCompressedPointer, 1, 1) \
  V(CheckedTaggedToCompressedSigned, 1, 1)  \
  V(CheckedUint32ToInt32, 1, 1)             \
  V(CheckedUint32ToTaggedSigned, 1, 1)      \
  V(CheckedUint64Bounds, 2, 1)              \
  V(CheckedUint64ToInt32, 1, 1)             \
  V(CheckedUint64ToTaggedSigned, 1, 1)

#define CHECKED_BOUNDS_OP_LIST(V) V(CheckedUint32Bounds)

struct SimplifiedOperatorGlobalCache final {
#define PURE(Name, properties, value_input_count, control_input_count)     \
  struct Name##Operator final : public Operator {                          \
    Name##Operator()                                                       \
        : Operator(IrOpcode::k##Name, Operator::kPure | properties, #Name, \
                   value_input_count, 0, control_input_count, 1, 0, 0) {}  \
  };                                                                       \
  Name##Operator k##Name;
  PURE_OP_LIST(PURE)
#undef PURE

#define EFFECT_DEPENDENT(Name, properties, value_input_count,              \
                         control_input_count)                              \
  struct Name##Operator final : public Operator {                          \
    Name##Operator()                                                       \
        : Operator(IrOpcode::k##Name,                                      \
                   Operator::kNoDeopt | Operator::kNoWrite |               \
                       Operator::kNoThrow | properties,                    \
                   #Name, value_input_count, 1, control_input_count, 1, 1, \
                   0) {}                                                   \
  };                                                                       \
  Name##Operator k##Name;
  EFFECT_DEPENDENT_OP_LIST(EFFECT_DEPENDENT)
#undef EFFECT_DEPENDENT

#define CHECKED(Name, value_input_count, value_output_count)             \
  struct Name##Operator final : public Operator {                        \
    Name##Operator()                                                     \
        : Operator(IrOpcode::k##Name,                                    \
                   Operator::kFoldable | Operator::kNoThrow, #Name,      \
                   value_input_count, 1, 1, value_output_count, 1, 0) {} \
  };                                                                     \
  Name##Operator k##Name;
  CHECKED_OP_LIST(CHECKED)
#undef CHECKED

#define CHECKED_WITH_FEEDBACK(Name, value_input_count, value_output_count) \
  struct Name##Operator final : public Operator1<CheckParameters> {        \
    Name##Operator()                                                       \
        : Operator1<CheckParameters>(                                      \
              IrOpcode::k##Name, Operator::kFoldable | Operator::kNoThrow, \
              #Name, value_input_count, 1, 1, value_output_count, 1, 0,    \
              CheckParameters(VectorSlotPair())) {}                        \
  };                                                                       \
  Name##Operator k##Name;
  CHECKED_WITH_FEEDBACK_OP_LIST(CHECKED_WITH_FEEDBACK)
#undef CHECKED_WITH_FEEDBACK

#define CHECKED_BOUNDS(Name)                                                  \
  struct Name##Operator final : public Operator1<CheckBoundsParameters> {     \
    Name##Operator(VectorSlotPair feedback, CheckBoundsParameters::Mode mode) \
        : Operator1<CheckBoundsParameters>(                                   \
              IrOpcode::k##Name, Operator::kFoldable | Operator::kNoThrow,    \
              #Name, 2, 1, 1, 1, 1, 0,                                        \
              CheckBoundsParameters(feedback, mode)) {}                       \
  };                                                                          \
  Name##Operator k##Name##Deopting = {                                        \
      VectorSlotPair(), CheckBoundsParameters::kDeoptOnOutOfBounds};          \
  Name##Operator k##Name##Aborting = {                                        \
      VectorSlotPair(), CheckBoundsParameters::kAbortOnOutOfBounds};
  CHECKED_BOUNDS_OP_LIST(CHECKED_BOUNDS)
#undef CHECKED_BOUNDS

  template <DeoptimizeReason kDeoptimizeReason>
  struct CheckIfOperator final : public Operator1<CheckIfParameters> {
    CheckIfOperator()
        : Operator1<CheckIfParameters>(
              IrOpcode::kCheckIf, Operator::kFoldable | Operator::kNoThrow,
              "CheckIf", 1, 1, 1, 0, 1, 0,
              CheckIfParameters(kDeoptimizeReason, VectorSlotPair())) {}
  };
#define CHECK_IF(Name, message) \
  CheckIfOperator<DeoptimizeReason::k##Name> kCheckIf##Name;
  DEOPTIMIZE_REASON_LIST(CHECK_IF)
#undef CHECK_IF

  struct FindOrderedHashMapEntryOperator final : public Operator {
    FindOrderedHashMapEntryOperator()
        : Operator(IrOpcode::kFindOrderedHashMapEntry, Operator::kEliminatable,
                   "FindOrderedHashMapEntry", 2, 1, 1, 1, 1, 0) {}
  };
  FindOrderedHashMapEntryOperator kFindOrderedHashMapEntry;

  struct FindOrderedHashMapEntryForInt32KeyOperator final : public Operator {
    FindOrderedHashMapEntryForInt32KeyOperator()
        : Operator(IrOpcode::kFindOrderedHashMapEntryForInt32Key,
                   Operator::kEliminatable,
                   "FindOrderedHashMapEntryForInt32Key", 2, 1, 1, 1, 1, 0) {}
  };
  FindOrderedHashMapEntryForInt32KeyOperator
      kFindOrderedHashMapEntryForInt32Key;

  struct ArgumentsFrameOperator final : public Operator {
    ArgumentsFrameOperator()
        : Operator(IrOpcode::kArgumentsFrame, Operator::kPure, "ArgumentsFrame",
                   0, 0, 0, 1, 0, 0) {}
  };
  ArgumentsFrameOperator kArgumentsFrame;

  template <CheckForMinusZeroMode kMode>
  struct ChangeFloat64ToTaggedOperator final
      : public Operator1<CheckForMinusZeroMode> {
    ChangeFloat64ToTaggedOperator()
        : Operator1<CheckForMinusZeroMode>(
              IrOpcode::kChangeFloat64ToTagged, Operator::kPure,
              "ChangeFloat64ToTagged", 1, 0, 0, 1, 0, 0, kMode) {}
  };
  ChangeFloat64ToTaggedOperator<CheckForMinusZeroMode::kCheckForMinusZero>
      kChangeFloat64ToTaggedCheckForMinusZeroOperator;
  ChangeFloat64ToTaggedOperator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kChangeFloat64ToTaggedDontCheckForMinusZeroOperator;

  template <CheckForMinusZeroMode kMode>
  struct CheckedInt32MulOperator final
      : public Operator1<CheckForMinusZeroMode> {
    CheckedInt32MulOperator()
        : Operator1<CheckForMinusZeroMode>(
              IrOpcode::kCheckedInt32Mul,
              Operator::kFoldable | Operator::kNoThrow, "CheckedInt32Mul", 2, 1,
              1, 1, 1, 0, kMode) {}
  };
  CheckedInt32MulOperator<CheckForMinusZeroMode::kCheckForMinusZero>
      kCheckedInt32MulCheckForMinusZeroOperator;
  CheckedInt32MulOperator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kCheckedInt32MulDontCheckForMinusZeroOperator;

  template <CheckForMinusZeroMode kMode>
  struct CheckedFloat64ToInt32Operator final
      : public Operator1<CheckMinusZeroParameters> {
    CheckedFloat64ToInt32Operator()
        : Operator1<CheckMinusZeroParameters>(
              IrOpcode::kCheckedFloat64ToInt32,
              Operator::kFoldable | Operator::kNoThrow, "CheckedFloat64ToInt32",
              1, 1, 1, 1, 1, 0,
              CheckMinusZeroParameters(kMode, VectorSlotPair())) {}
  };
  CheckedFloat64ToInt32Operator<CheckForMinusZeroMode::kCheckForMinusZero>
      kCheckedFloat64ToInt32CheckForMinusZeroOperator;
  CheckedFloat64ToInt32Operator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kCheckedFloat64ToInt32DontCheckForMinusZeroOperator;

  template <CheckForMinusZeroMode kMode>
  struct CheckedFloat64ToInt64Operator final
      : public Operator1<CheckMinusZeroParameters> {
    CheckedFloat64ToInt64Operator()
        : Operator1<CheckMinusZeroParameters>(
              IrOpcode::kCheckedFloat64ToInt64,
              Operator::kFoldable | Operator::kNoThrow, "CheckedFloat64ToInt64",
              1, 1, 1, 1, 1, 0,
              CheckMinusZeroParameters(kMode, VectorSlotPair())) {}
  };
  CheckedFloat64ToInt64Operator<CheckForMinusZeroMode::kCheckForMinusZero>
      kCheckedFloat64ToInt64CheckForMinusZeroOperator;
  CheckedFloat64ToInt64Operator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kCheckedFloat64ToInt64DontCheckForMinusZeroOperator;

  template <CheckForMinusZeroMode kMode>
  struct CheckedTaggedToInt32Operator final
      : public Operator1<CheckMinusZeroParameters> {
    CheckedTaggedToInt32Operator()
        : Operator1<CheckMinusZeroParameters>(
              IrOpcode::kCheckedTaggedToInt32,
              Operator::kFoldable | Operator::kNoThrow, "CheckedTaggedToInt32",
              1, 1, 1, 1, 1, 0,
              CheckMinusZeroParameters(kMode, VectorSlotPair())) {}
  };
  CheckedTaggedToInt32Operator<CheckForMinusZeroMode::kCheckForMinusZero>
      kCheckedTaggedToInt32CheckForMinusZeroOperator;
  CheckedTaggedToInt32Operator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kCheckedTaggedToInt32DontCheckForMinusZeroOperator;

  template <CheckForMinusZeroMode kMode>
  struct CheckedTaggedToInt64Operator final
      : public Operator1<CheckMinusZeroParameters> {
    CheckedTaggedToInt64Operator()
        : Operator1<CheckMinusZeroParameters>(
              IrOpcode::kCheckedTaggedToInt64,
              Operator::kFoldable | Operator::kNoThrow, "CheckedTaggedToInt64",
              1, 1, 1, 1, 1, 0,
              CheckMinusZeroParameters(kMode, VectorSlotPair())) {}
  };
  CheckedTaggedToInt64Operator<CheckForMinusZeroMode::kCheckForMinusZero>
      kCheckedTaggedToInt64CheckForMinusZeroOperator;
  CheckedTaggedToInt64Operator<CheckForMinusZeroMode::kDontCheckForMinusZero>
      kCheckedTaggedToInt64DontCheckForMinusZeroOperator;

  template <CheckTaggedInputMode kMode>
  struct CheckedTaggedToFloat64Operator final
      : public Operator1<CheckTaggedInputParameters> {
    CheckedTaggedToFloat64Operator()
        : Operator1<CheckTaggedInputParameters>(
              IrOpcode::kCheckedTaggedToFloat64,
              Operator::kFoldable | Operator::kNoThrow,
              "CheckedTaggedToFloat64", 1, 1, 1, 1, 1, 0,
              CheckTaggedInputParameters(kMode, VectorSlotPair())) {}
  };
  CheckedTaggedToFloat64Operator<CheckTaggedInputMode::kNumber>
      kCheckedTaggedToFloat64NumberOperator;
  CheckedTaggedToFloat64Operator<CheckTaggedInputMode::kNumberOrOddball>
      kCheckedTaggedToFloat64NumberOrOddballOperator;

  template <CheckTaggedInputMode kMode>
  struct CheckedTruncateTaggedToWord32Operator final
      : public Operator1<CheckTaggedInputParameters> {
    CheckedTruncateTaggedToWord32Operator()
        : Operator1<CheckTaggedInputParameters>(
              IrOpcode::kCheckedTruncateTaggedToWord32,
              Operator::kFoldable | Operator::kNoThrow,
              "CheckedTruncateTaggedToWord32", 1, 1, 1, 1, 1, 0,
              CheckTaggedInputParameters(kMode, VectorSlotPair())) {}
  };
  CheckedTruncateTaggedToWord32Operator<CheckTaggedInputMode::kNumber>
      kCheckedTruncateTaggedToWord32NumberOperator;
  CheckedTruncateTaggedToWord32Operator<CheckTaggedInputMode::kNumberOrOddball>
      kCheckedTruncateTaggedToWord32NumberOrOddballOperator;

  template <ConvertReceiverMode kMode>
  struct ConvertReceiverOperator final : public Operator1<ConvertReceiverMode> {
    ConvertReceiverOperator()
        : Operator1<ConvertReceiverMode>(  // --
              IrOpcode::kConvertReceiver,  // opcode
              Operator::kEliminatable,     // flags
              "ConvertReceiver",           // name
              2, 1, 1, 1, 1, 0,            // counts
              kMode) {}                    // param
  };
  ConvertReceiverOperator<ConvertReceiverMode::kAny>
      kConvertReceiverAnyOperator;
  ConvertReceiverOperator<ConvertReceiverMode::kNullOrUndefined>
      kConvertReceiverNullOrUndefinedOperator;
  ConvertReceiverOperator<ConvertReceiverMode::kNotNullOrUndefined>
      kConvertReceiverNotNullOrUndefinedOperator;

  template <CheckFloat64HoleMode kMode>
  struct CheckFloat64HoleNaNOperator final
      : public Operator1<CheckFloat64HoleParameters> {
    CheckFloat64HoleNaNOperator()
        : Operator1<CheckFloat64HoleParameters>(
              IrOpcode::kCheckFloat64Hole,
              Operator::kFoldable | Operator::kNoThrow, "CheckFloat64Hole", 1,
              1, 1, 1, 1, 0,
              CheckFloat64HoleParameters(kMode, VectorSlotPair())) {}
  };
  CheckFloat64HoleNaNOperator<CheckFloat64HoleMode::kAllowReturnHole>
      kCheckFloat64HoleAllowReturnHoleOperator;
  CheckFloat64HoleNaNOperator<CheckFloat64HoleMode::kNeverReturnHole>
      kCheckFloat64HoleNeverReturnHoleOperator;

  struct EnsureWritableFastElementsOperator final : public Operator {
    EnsureWritableFastElementsOperator()
        : Operator(                                     // --
              IrOpcode::kEnsureWritableFastElements,    // opcode
              Operator::kNoDeopt | Operator::kNoThrow,  // flags
              "EnsureWritableFastElements",             // name
              2, 1, 1, 1, 1, 0) {}                      // counts
  };
  EnsureWritableFastElementsOperator kEnsureWritableFastElements;

  template <GrowFastElementsMode kMode>
  struct GrowFastElementsOperator final
      : public Operator1<GrowFastElementsParameters> {
    GrowFastElementsOperator()
        : Operator1(IrOpcode::kMaybeGrowFastElements, Operator::kNoThrow,
                    "MaybeGrowFastElements", 4, 1, 1, 1, 1, 0,
                    GrowFastElementsParameters(kMode, VectorSlotPair())) {}
  };

  GrowFastElementsOperator<GrowFastElementsMode::kDoubleElements>
      kGrowFastElementsOperatorDoubleElements;
  GrowFastElementsOperator<GrowFastElementsMode::kSmiOrObjectElements>
      kGrowFastElementsOperatorSmiOrObjectElements;

  struct LoadFieldByIndexOperator final : public Operator {
    LoadFieldByIndexOperator()
        : Operator(                         // --
              IrOpcode::kLoadFieldByIndex,  // opcode
              Operator::kEliminatable,      // flags,
              "LoadFieldByIndex",           // name
              2, 1, 1, 1, 1, 0) {}          // counts;
  };
  LoadFieldByIndexOperator kLoadFieldByIndex;

#define SPECULATIVE_NUMBER_BINOP(Name)                                      \
  template <NumberOperationHint kHint>                                      \
  struct Name##Operator final : public Operator1<NumberOperationHint> {     \
    Name##Operator()                                                        \
        : Operator1<NumberOperationHint>(                                   \
              IrOpcode::k##Name, Operator::kFoldable | Operator::kNoThrow,  \
              #Name, 2, 1, 1, 1, 1, 0, kHint) {}                            \
  };                                                                        \
  Name##Operator<NumberOperationHint::kSignedSmall>                         \
      k##Name##SignedSmallOperator;                                         \
  Name##Operator<NumberOperationHint::kSignedSmallInputs>                   \
      k##Name##SignedSmallInputsOperator;                                   \
  Name##Operator<NumberOperationHint::kSigned32> k##Name##Signed32Operator; \
  Name##Operator<NumberOperationHint::kNumber> k##Name##NumberOperator;     \
  Name##Operator<NumberOperationHint::kNumberOrOddball>                     \
      k##Name##NumberOrOddballOperator;
  SPECULATIVE_NUMBER_BINOP_LIST(SPECULATIVE_NUMBER_BINOP)
#undef SPECULATIVE_NUMBER_BINOP

  template <NumberOperationHint kHint>
  struct SpeculativeToNumberOperator final
      : public Operator1<NumberOperationParameters> {
    SpeculativeToNumberOperator()
        : Operator1<NumberOperationParameters>(
              IrOpcode::kSpeculativeToNumber,
              Operator::kFoldable | Operator::kNoThrow, "SpeculativeToNumber",
              1, 1, 1, 1, 1, 0,
              NumberOperationParameters(kHint, VectorSlotPair())) {}
  };
  SpeculativeToNumberOperator<NumberOperationHint::kSignedSmall>
      kSpeculativeToNumberSignedSmallOperator;
  SpeculativeToNumberOperator<NumberOperationHint::kSigned32>
      kSpeculativeToNumberSigned32Operator;
  SpeculativeToNumberOperator<NumberOperationHint::kNumber>
      kSpeculativeToNumberNumberOperator;
  SpeculativeToNumberOperator<NumberOperationHint::kNumberOrOddball>
      kSpeculativeToNumberNumberOrOddballOperator;
};

namespace {
DEFINE_LAZY_LEAKY_OBJECT_GETTER(SimplifiedOperatorGlobalCache,
                                GetSimplifiedOperatorGlobalCache)
}

SimplifiedOperatorBuilder::SimplifiedOperatorBuilder(Zone* zone)
    : cache_(*GetSimplifiedOperatorGlobalCache()), zone_(zone) {}

#define GET_FROM_CACHE(Name, ...) \
  const Operator* SimplifiedOperatorBuilder::Name() { return &cache_.k##Name; }
PURE_OP_LIST(GET_FROM_CACHE)
EFFECT_DEPENDENT_OP_LIST(GET_FROM_CACHE)
CHECKED_OP_LIST(GET_FROM_CACHE)
GET_FROM_CACHE(ArgumentsFrame)
GET_FROM_CACHE(FindOrderedHashMapEntry)
GET_FROM_CACHE(FindOrderedHashMapEntryForInt32Key)
GET_FROM_CACHE(LoadFieldByIndex)
#undef GET_FROM_CACHE

#define GET_FROM_CACHE_WITH_FEEDBACK(Name, value_input_count,               \
                                     value_output_count)                    \
  const Operator* SimplifiedOperatorBuilder::Name(                          \
      const VectorSlotPair& feedback) {                                     \
    if (!feedback.IsValid()) {                                              \
      return &cache_.k##Name;                                               \
    }                                                                       \
    return new (zone()) Operator1<CheckParameters>(                         \
        IrOpcode::k##Name, Operator::kFoldable | Operator::kNoThrow, #Name, \
        value_input_count, 1, 1, value_output_count, 1, 0,                  \
        CheckParameters(feedback));                                         \
  }
CHECKED_WITH_FEEDBACK_OP_LIST(GET_FROM_CACHE_WITH_FEEDBACK)
#undef GET_FROM_CACHE_WITH_FEEDBACK

#define GET_FROM_CACHE_WITH_FEEDBACK(Name)                                \
  const Operator* SimplifiedOperatorBuilder::Name(                        \
      const VectorSlotPair& feedback, CheckBoundsParameters::Mode mode) { \
    if (!feedback.IsValid()) {                                            \
      switch (mode) {                                                     \
        case CheckBoundsParameters::kDeoptOnOutOfBounds:                  \
          return &cache_.k##Name##Deopting;                               \
        case CheckBoundsParameters::kAbortOnOutOfBounds:                  \
          return &cache_.k##Name##Aborting;                               \
      }                                                                   \
    }                                                                     \
    return new (zone())                                                   \
        SimplifiedOperatorGlobalCache::Name##Operator(feedback, mode);    \
  }
CHECKED_BOUNDS_OP_LIST(GET_FROM_CACHE_WITH_FEEDBACK)
#undef GET_FROM_CACHE_WITH_FEEDBACK

bool IsCheckedWithFeedback(const Operator* op) {
#define CASE(Name, ...) case IrOpcode::k##Name:
  switch (op->opcode()) {
    CHECKED_WITH_FEEDBACK_OP_LIST(CASE) return true;
    default:
      return false;
  }
#undef CASE
}

const Operator* SimplifiedOperatorBuilder::RuntimeAbort(AbortReason reason) {
  return new (zone()) Operator1<int>(           // --
      IrOpcode::kRuntimeAbort,                  // opcode
      Operator::kNoThrow | Operator::kNoDeopt,  // flags
      "RuntimeAbort",                           // name
      0, 1, 1, 0, 1, 0,                         // counts
      static_cast<int>(reason));                // parameter
}

const Operator* SimplifiedOperatorBuilder::BigIntAsUintN(int bits) {
  CHECK(0 <= bits && bits <= 64);

  return new (zone()) Operator1<int>(IrOpcode::kBigIntAsUintN, Operator::kPure,
                                     "BigIntAsUintN", 1, 0, 0, 1, 0, 0, bits);
}

const Operator* SimplifiedOperatorBuilder::AssertType(Type type) {
  DCHECK(type.IsRange());
  return new (zone()) Operator1<Type>(IrOpcode::kAssertType,
                                      Operator::kNoThrow | Operator::kNoDeopt,
                                      "AssertType", 1, 0, 0, 1, 0, 0, type);
}

const Operator* SimplifiedOperatorBuilder::CheckIf(
    DeoptimizeReason reason, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (reason) {
#define CHECK_IF(Name, message)   \
  case DeoptimizeReason::k##Name: \
    return &cache_.kCheckIf##Name;
    DEOPTIMIZE_REASON_LIST(CHECK_IF)
#undef CHECK_IF
    }
  }
  return new (zone()) Operator1<CheckIfParameters>(
      IrOpcode::kCheckIf, Operator::kFoldable | Operator::kNoThrow, "CheckIf",
      1, 1, 1, 0, 1, 0, CheckIfParameters(reason, feedback));
}

const Operator* SimplifiedOperatorBuilder::ChangeFloat64ToTagged(
    CheckForMinusZeroMode mode) {
  switch (mode) {
    case CheckForMinusZeroMode::kCheckForMinusZero:
      return &cache_.kChangeFloat64ToTaggedCheckForMinusZeroOperator;
    case CheckForMinusZeroMode::kDontCheckForMinusZero:
      return &cache_.kChangeFloat64ToTaggedDontCheckForMinusZeroOperator;
  }
  UNREACHABLE();
}

const Operator* SimplifiedOperatorBuilder::CheckedInt32Mul(
    CheckForMinusZeroMode mode) {
  switch (mode) {
    case CheckForMinusZeroMode::kCheckForMinusZero:
      return &cache_.kCheckedInt32MulCheckForMinusZeroOperator;
    case CheckForMinusZeroMode::kDontCheckForMinusZero:
      return &cache_.kCheckedInt32MulDontCheckForMinusZeroOperator;
  }
  UNREACHABLE();
}

const Operator* SimplifiedOperatorBuilder::CheckedFloat64ToInt32(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckForMinusZeroMode::kCheckForMinusZero:
        return &cache_.kCheckedFloat64ToInt32CheckForMinusZeroOperator;
      case CheckForMinusZeroMode::kDontCheckForMinusZero:
        return &cache_.kCheckedFloat64ToInt32DontCheckForMinusZeroOperator;
    }
  }
  return new (zone()) Operator1<CheckMinusZeroParameters>(
      IrOpcode::kCheckedFloat64ToInt32,
      Operator::kFoldable | Operator::kNoThrow, "CheckedFloat64ToInt32", 1, 1,
      1, 1, 1, 0, CheckMinusZeroParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckedFloat64ToInt64(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckForMinusZeroMode::kCheckForMinusZero:
        return &cache_.kCheckedFloat64ToInt64CheckForMinusZeroOperator;
      case CheckForMinusZeroMode::kDontCheckForMinusZero:
        return &cache_.kCheckedFloat64ToInt64DontCheckForMinusZeroOperator;
    }
  }
  return new (zone()) Operator1<CheckMinusZeroParameters>(
      IrOpcode::kCheckedFloat64ToInt64,
      Operator::kFoldable | Operator::kNoThrow, "CheckedFloat64ToInt64", 1, 1,
      1, 1, 1, 0, CheckMinusZeroParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckedTaggedToInt32(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckForMinusZeroMode::kCheckForMinusZero:
        return &cache_.kCheckedTaggedToInt32CheckForMinusZeroOperator;
      case CheckForMinusZeroMode::kDontCheckForMinusZero:
        return &cache_.kCheckedTaggedToInt32DontCheckForMinusZeroOperator;
    }
  }
  return new (zone()) Operator1<CheckMinusZeroParameters>(
      IrOpcode::kCheckedTaggedToInt32, Operator::kFoldable | Operator::kNoThrow,
      "CheckedTaggedToInt32", 1, 1, 1, 1, 1, 0,
      CheckMinusZeroParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckedTaggedToInt64(
    CheckForMinusZeroMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckForMinusZeroMode::kCheckForMinusZero:
        return &cache_.kCheckedTaggedToInt64CheckForMinusZeroOperator;
      case CheckForMinusZeroMode::kDontCheckForMinusZero:
        return &cache_.kCheckedTaggedToInt64DontCheckForMinusZeroOperator;
    }
  }
  return new (zone()) Operator1<CheckMinusZeroParameters>(
      IrOpcode::kCheckedTaggedToInt64, Operator::kFoldable | Operator::kNoThrow,
      "CheckedTaggedToInt64", 1, 1, 1, 1, 1, 0,
      CheckMinusZeroParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckedTaggedToFloat64(
    CheckTaggedInputMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckTaggedInputMode::kNumber:
        return &cache_.kCheckedTaggedToFloat64NumberOperator;
      case CheckTaggedInputMode::kNumberOrOddball:
        return &cache_.kCheckedTaggedToFloat64NumberOrOddballOperator;
    }
  }
  return new (zone()) Operator1<CheckTaggedInputParameters>(
      IrOpcode::kCheckedTaggedToFloat64,
      Operator::kFoldable | Operator::kNoThrow, "CheckedTaggedToFloat64", 1, 1,
      1, 1, 1, 0, CheckTaggedInputParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckedTruncateTaggedToWord32(
    CheckTaggedInputMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckTaggedInputMode::kNumber:
        return &cache_.kCheckedTruncateTaggedToWord32NumberOperator;
      case CheckTaggedInputMode::kNumberOrOddball:
        return &cache_.kCheckedTruncateTaggedToWord32NumberOrOddballOperator;
    }
  }
  return new (zone()) Operator1<CheckTaggedInputParameters>(
      IrOpcode::kCheckedTruncateTaggedToWord32,
      Operator::kFoldable | Operator::kNoThrow, "CheckedTruncateTaggedToWord32",
      1, 1, 1, 1, 1, 0, CheckTaggedInputParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::CheckMaps(
    CheckMapsFlags flags, ZoneHandleSet<Map> maps,
    const VectorSlotPair& feedback) {
  CheckMapsParameters const parameters(flags, maps, feedback);
  return new (zone()) Operator1<CheckMapsParameters>(  // --
      IrOpcode::kCheckMaps,                            // opcode
      Operator::kNoThrow | Operator::kNoWrite,         // flags
      "CheckMaps",                                     // name
      1, 1, 1, 0, 1, 0,                                // counts
      parameters);                                     // parameter
}

const Operator* SimplifiedOperatorBuilder::MapGuard(ZoneHandleSet<Map> maps) {
  DCHECK_LT(0, maps.size());
  return new (zone()) Operator1<ZoneHandleSet<Map>>(  // --
      IrOpcode::kMapGuard, Operator::kEliminatable,   // opcode
      "MapGuard",                                     // name
      1, 1, 1, 0, 1, 0,                               // counts
      maps);                                          // parameter
}

const Operator* SimplifiedOperatorBuilder::CompareMaps(
    ZoneHandleSet<Map> maps) {
  DCHECK_LT(0, maps.size());
  return new (zone()) Operator1<ZoneHandleSet<Map>>(  // --
      IrOpcode::kCompareMaps,                         // opcode
      Operator::kNoThrow | Operator::kNoWrite,        // flags
      "CompareMaps",                                  // name
      1, 1, 1, 1, 1, 0,                               // counts
      maps);                                          // parameter
}

const Operator* SimplifiedOperatorBuilder::ConvertReceiver(
    ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kAny:
      return &cache_.kConvertReceiverAnyOperator;
    case ConvertReceiverMode::kNullOrUndefined:
      return &cache_.kConvertReceiverNullOrUndefinedOperator;
    case ConvertReceiverMode::kNotNullOrUndefined:
      return &cache_.kConvertReceiverNotNullOrUndefinedOperator;
  }
  UNREACHABLE();
  return nullptr;
}

const Operator* SimplifiedOperatorBuilder::CheckFloat64Hole(
    CheckFloat64HoleMode mode, VectorSlotPair const& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case CheckFloat64HoleMode::kAllowReturnHole:
        return &cache_.kCheckFloat64HoleAllowReturnHoleOperator;
      case CheckFloat64HoleMode::kNeverReturnHole:
        return &cache_.kCheckFloat64HoleNeverReturnHoleOperator;
    }
    UNREACHABLE();
  }
  return new (zone()) Operator1<CheckFloat64HoleParameters>(
      IrOpcode::kCheckFloat64Hole, Operator::kFoldable | Operator::kNoThrow,
      "CheckFloat64Hole", 1, 1, 1, 1, 1, 0,
      CheckFloat64HoleParameters(mode, feedback));
}

const Operator* SimplifiedOperatorBuilder::SpeculativeBigIntAdd(
    BigIntOperationHint hint) {
  return new (zone()) Operator1<BigIntOperationHint>(
      IrOpcode::kSpeculativeBigIntAdd, Operator::kFoldable | Operator::kNoThrow,
      "SpeculativeBigIntAdd", 2, 1, 1, 1, 1, 0, hint);
}

const Operator* SimplifiedOperatorBuilder::SpeculativeBigIntNegate(
    BigIntOperationHint hint) {
  return new (zone()) Operator1<BigIntOperationHint>(
      IrOpcode::kSpeculativeBigIntNegate,
      Operator::kFoldable | Operator::kNoThrow, "SpeculativeBigIntNegate", 1, 1,
      1, 1, 1, 0, hint);
}

const Operator* SimplifiedOperatorBuilder::SpeculativeToNumber(
    NumberOperationHint hint, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (hint) {
      case NumberOperationHint::kSignedSmall:
        return &cache_.kSpeculativeToNumberSignedSmallOperator;
      case NumberOperationHint::kSignedSmallInputs:
        break;
      case NumberOperationHint::kSigned32:
        return &cache_.kSpeculativeToNumberSigned32Operator;
      case NumberOperationHint::kNumber:
        return &cache_.kSpeculativeToNumberNumberOperator;
      case NumberOperationHint::kNumberOrOddball:
        return &cache_.kSpeculativeToNumberNumberOrOddballOperator;
    }
  }
  return new (zone()) Operator1<NumberOperationParameters>(
      IrOpcode::kSpeculativeToNumber, Operator::kFoldable | Operator::kNoThrow,
      "SpeculativeToNumber", 1, 1, 1, 1, 1, 0,
      NumberOperationParameters(hint, feedback));
}

const Operator* SimplifiedOperatorBuilder::EnsureWritableFastElements() {
  return &cache_.kEnsureWritableFastElements;
}

const Operator* SimplifiedOperatorBuilder::MaybeGrowFastElements(
    GrowFastElementsMode mode, const VectorSlotPair& feedback) {
  if (!feedback.IsValid()) {
    switch (mode) {
      case GrowFastElementsMode::kDoubleElements:
        return &cache_.kGrowFastElementsOperatorDoubleElements;
      case GrowFastElementsMode::kSmiOrObjectElements:
        return &cache_.kGrowFastElementsOperatorSmiOrObjectElements;
    }
  }
  return new (zone()) Operator1<GrowFastElementsParameters>(  // --
      IrOpcode::kMaybeGrowFastElements,                       // opcode
      Operator::kNoThrow,                                     // flags
      "MaybeGrowFastElements",                                // name
      4, 1, 1, 1, 1, 0,                                       // counts
      GrowFastElementsParameters(mode, feedback));            // parameter
}

const Operator* SimplifiedOperatorBuilder::TransitionElementsKind(
    ElementsTransition transition) {
  return new (zone()) Operator1<ElementsTransition>(  // --
      IrOpcode::kTransitionElementsKind,              // opcode
      Operator::kNoThrow,                             // flags
      "TransitionElementsKind",                       // name
      1, 1, 1, 0, 1, 0,                               // counts
      transition);                                    // parameter
}

namespace {

struct ArgumentsLengthParameters {
  int formal_parameter_count;
  bool is_rest_length;
};

bool operator==(ArgumentsLengthParameters first,
                ArgumentsLengthParameters second) {
  return first.formal_parameter_count == second.formal_parameter_count &&
         first.is_rest_length == second.is_rest_length;
}

size_t hash_value(ArgumentsLengthParameters param) {
  return base::hash_combine(param.formal_parameter_count, param.is_rest_length);
}

std::ostream& operator<<(std::ostream& os, ArgumentsLengthParameters param) {
  return os << param.formal_parameter_count << ", "
            << (param.is_rest_length ? "rest length" : "not rest length");
}

}  // namespace

const Operator* SimplifiedOperatorBuilder::ArgumentsLength(
    int formal_parameter_count, bool is_rest_length) {
  return new (zone()) Operator1<ArgumentsLengthParameters>(  // --
      IrOpcode::kArgumentsLength,                            // opcode
      Operator::kPure,                                       // flags
      "ArgumentsLength",                                     // name
      1, 0, 0, 1, 0, 0,                                      // counts
      ArgumentsLengthParameters{formal_parameter_count,
                                is_rest_length});  // parameter
}

int FormalParameterCountOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kArgumentsLength, op->opcode());
  return OpParameter<ArgumentsLengthParameters>(op).formal_parameter_count;
}

bool IsRestLengthOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kArgumentsLength, op->opcode());
  return OpParameter<ArgumentsLengthParameters>(op).is_rest_length;
}

bool operator==(CheckParameters const& lhs, CheckParameters const& rhs) {
  return lhs.feedback() == rhs.feedback();
}

size_t hash_value(CheckParameters const& p) { return hash_value(p.feedback()); }

std::ostream& operator<<(std::ostream& os, CheckParameters const& p) {
  return os << p.feedback();
}

CheckParameters const& CheckParametersOf(Operator const* op) {
  if (op->opcode() == IrOpcode::kCheckedUint32Bounds) {
    return OpParameter<CheckBoundsParameters>(op).check_parameters();
  }
#define MAKE_OR(name, arg2, arg3) op->opcode() == IrOpcode::k##name ||
  CHECK((CHECKED_WITH_FEEDBACK_OP_LIST(MAKE_OR) false));
#undef MAKE_OR
  return OpParameter<CheckParameters>(op);
}

bool operator==(CheckBoundsParameters const& lhs,
                CheckBoundsParameters const& rhs) {
  return lhs.check_parameters() == rhs.check_parameters() &&
         lhs.mode() == rhs.mode();
}

size_t hash_value(CheckBoundsParameters const& p) {
  return base::hash_combine(hash_value(p.check_parameters()), p.mode());
}

std::ostream& operator<<(std::ostream& os, CheckBoundsParameters const& p) {
  os << p.check_parameters() << ",";
  switch (p.mode()) {
    case CheckBoundsParameters::kDeoptOnOutOfBounds:
      os << "deopt";
      break;
    case CheckBoundsParameters::kAbortOnOutOfBounds:
      os << "abort";
      break;
  }
  return os;
}

CheckBoundsParameters const& CheckBoundsParametersOf(Operator const* op) {
  CHECK_EQ(op->opcode(), IrOpcode::kCheckedUint32Bounds);
  return OpParameter<CheckBoundsParameters>(op);
}

bool operator==(CheckIfParameters const& lhs, CheckIfParameters const& rhs) {
  return lhs.reason() == rhs.reason() && lhs.feedback() == rhs.feedback();
}

size_t hash_value(CheckIfParameters const& p) {
  return base::hash_combine(p.reason(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, CheckIfParameters const& p) {
  return os << p.reason() << p.feedback();
}

CheckIfParameters const& CheckIfParametersOf(Operator const* op) {
  CHECK(op->opcode() == IrOpcode::kCheckIf);
  return OpParameter<CheckIfParameters>(op);
}

const Operator* SimplifiedOperatorBuilder::NewDoubleElements(
    AllocationType allocation) {
  return new (zone()) Operator1<AllocationType>(  // --
      IrOpcode::kNewDoubleElements,               // opcode
      Operator::kEliminatable,                    // flags
      "NewDoubleElements",                        // name
      1, 1, 1, 1, 1, 0,                           // counts
      allocation);                                // parameter
}

const Operator* SimplifiedOperatorBuilder::NewSmiOrObjectElements(
    AllocationType allocation) {
  return new (zone()) Operator1<AllocationType>(  // --
      IrOpcode::kNewSmiOrObjectElements,          // opcode
      Operator::kEliminatable,                    // flags
      "NewSmiOrObjectElements",                   // name
      1, 1, 1, 1, 1, 0,                           // counts
      allocation);                                // parameter
}

const Operator* SimplifiedOperatorBuilder::NewArgumentsElements(
    int mapped_count) {
  return new (zone()) Operator1<int>(   // --
      IrOpcode::kNewArgumentsElements,  // opcode
      Operator::kEliminatable,          // flags
      "NewArgumentsElements",           // name
      2, 1, 0, 1, 1, 0,                 // counts
      mapped_count);                    // parameter
}

int NewArgumentsElementsMappedCountOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kNewArgumentsElements, op->opcode());
  return OpParameter<int>(op);
}

const Operator* SimplifiedOperatorBuilder::Allocate(Type type,
                                                    AllocationType allocation) {
  return new (zone()) Operator1<AllocateParameters>(
      IrOpcode::kAllocate,
      Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite, "Allocate",
      1, 1, 1, 1, 1, 0, AllocateParameters(type, allocation));
}

const Operator* SimplifiedOperatorBuilder::AllocateRaw(
    Type type, AllocationType allocation,
    AllowLargeObjects allow_large_objects) {
  // We forbid optimized allocations to allocate in a different generation than
  // requested.
  DCHECK(!(allow_large_objects == AllowLargeObjects::kTrue &&
           allocation == AllocationType::kYoung &&
           !FLAG_young_generation_large_objects));
  return new (zone()) Operator1<AllocateParameters>(
      IrOpcode::kAllocateRaw,
      Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,
      "AllocateRaw", 1, 1, 1, 1, 1, 1,
      AllocateParameters(type, allocation, allow_large_objects));
}

#define SPECULATIVE_NUMBER_BINOP(Name)                                        \
  const Operator* SimplifiedOperatorBuilder::Name(NumberOperationHint hint) { \
    switch (hint) {                                                           \
      case NumberOperationHint::kSignedSmall:                                 \
        return &cache_.k##Name##SignedSmallOperator;                          \
      case NumberOperationHint::kSignedSmallInputs:                           \
        return &cache_.k##Name##SignedSmallInputsOperator;                    \
      case NumberOperationHint::kSigned32:                                    \
        return &cache_.k##Name##Signed32Operator;                             \
      case NumberOperationHint::kNumber:                                      \
        return &cache_.k##Name##NumberOperator;                               \
      case NumberOperationHint::kNumberOrOddball:                             \
        return &cache_.k##Name##NumberOrOddballOperator;                      \
    }                                                                         \
    UNREACHABLE();                                                            \
    return nullptr;                                                           \
  }
SPECULATIVE_NUMBER_BINOP_LIST(SPECULATIVE_NUMBER_BINOP)
#undef SPECULATIVE_NUMBER_BINOP

#define ACCESS_OP_LIST(V)                                                \
  V(LoadField, FieldAccess, Operator::kNoWrite, 1, 1, 1)                 \
  V(StoreField, FieldAccess, Operator::kNoRead, 2, 1, 0)                 \
  V(LoadElement, ElementAccess, Operator::kNoWrite, 2, 1, 1)             \
  V(StoreElement, ElementAccess, Operator::kNoRead, 3, 1, 0)             \
  V(LoadTypedElement, ExternalArrayType, Operator::kNoWrite, 4, 1, 1)    \
  V(LoadFromObject, ObjectAccess, Operator::kNoWrite, 2, 1, 1)           \
  V(StoreTypedElement, ExternalArrayType, Operator::kNoRead, 5, 1, 0)    \
  V(StoreToObject, ObjectAccess, Operator::kNoRead, 3, 1, 0)             \
  V(LoadDataViewElement, ExternalArrayType, Operator::kNoWrite, 4, 1, 1) \
  V(StoreDataViewElement, ExternalArrayType, Operator::kNoRead, 5, 1, 0)

#define ACCESS(Name, Type, properties, value_input_count, control_input_count, \
               output_count)                                                   \
  const Operator* SimplifiedOperatorBuilder::Name(const Type& access) {        \
    return new (zone())                                                        \
        Operator1<Type>(IrOpcode::k##Name,                                     \
                        Operator::kNoDeopt | Operator::kNoThrow | properties,  \
                        #Name, value_input_count, 1, control_input_count,      \
                        output_count, 1, 0, access);                           \
  }
ACCESS_OP_LIST(ACCESS)
#undef ACCESS

const Operator* SimplifiedOperatorBuilder::TransitionAndStoreElement(
    Handle<Map> double_map, Handle<Map> fast_map) {
  TransitionAndStoreElementParameters parameters(double_map, fast_map);
  return new (zone()) Operator1<TransitionAndStoreElementParameters>(
      IrOpcode::kTransitionAndStoreElement,
      Operator::kNoDeopt | Operator::kNoThrow, "TransitionAndStoreElement", 3,
      1, 1, 0, 1, 0, parameters);
}

const Operator* SimplifiedOperatorBuilder::StoreSignedSmallElement() {
  return new (zone()) Operator(IrOpcode::kStoreSignedSmallElement,
                               Operator::kNoDeopt | Operator::kNoThrow,
                               "StoreSignedSmallElement", 3, 1, 1, 0, 1, 0);
}

const Operator* SimplifiedOperatorBuilder::TransitionAndStoreNumberElement(
    Handle<Map> double_map) {
  TransitionAndStoreNumberElementParameters parameters(double_map);
  return new (zone()) Operator1<TransitionAndStoreNumberElementParameters>(
      IrOpcode::kTransitionAndStoreNumberElement,
      Operator::kNoDeopt | Operator::kNoThrow,
      "TransitionAndStoreNumberElement", 3, 1, 1, 0, 1, 0, parameters);
}

const Operator* SimplifiedOperatorBuilder::TransitionAndStoreNonNumberElement(
    Handle<Map> fast_map, Type value_type) {
  TransitionAndStoreNonNumberElementParameters parameters(fast_map, value_type);
  return new (zone()) Operator1<TransitionAndStoreNonNumberElementParameters>(
      IrOpcode::kTransitionAndStoreNonNumberElement,
      Operator::kNoDeopt | Operator::kNoThrow,
      "TransitionAndStoreNonNumberElement", 3, 1, 1, 0, 1, 0, parameters);
}

#undef PURE_OP_LIST
#undef EFFECT_DEPENDENT_OP_LIST
#undef SPECULATIVE_NUMBER_BINOP_LIST
#undef CHECKED_WITH_FEEDBACK_OP_LIST
#undef CHECKED_BOUNDS_OP_LIST
#undef CHECKED_OP_LIST
#undef ACCESS_OP_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
