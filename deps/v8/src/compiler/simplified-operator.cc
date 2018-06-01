// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/simplified-operator.h"

#include "src/base/lazy-instance.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/objects/map.h"
#include "src/objects/name.h"

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
  access.type->PrintTo(os);
  os << ", " << access.machine_type << ", " << access.write_barrier_kind << "]";
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
  os << access.base_is_tagged << ", " << access.header_size << ", ";
  access.type->PrintTo(os);
  os << ", " << access.machine_type << ", " << access.write_barrier_kind;
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

ExternalArrayType ExternalArrayTypeOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kLoadTypedElement ||
         op->opcode() == IrOpcode::kStoreTypedElement);
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

CheckFloat64HoleMode CheckFloat64HoleModeOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kCheckFloat64Hole, op->opcode());
  return OpParameter<CheckFloat64HoleMode>(op);
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

MapsParameterInfo::MapsParameterInfo(ZoneHandleSet<Map> const& maps)
    : maps_(maps), instance_type_(Nothing<InstanceType>()) {
  DCHECK_LT(0, maps.size());
  instance_type_ = Just(maps.at(0)->instance_type());
  for (size_t i = 1; i < maps.size(); ++i) {
    if (instance_type_.FromJust() != maps.at(i)->instance_type()) {
      instance_type_ = Nothing<InstanceType>();
      break;
    }
  }
}

std::ostream& operator<<(std::ostream& os, MapsParameterInfo const& p) {
  ZoneHandleSet<Map> const& maps = p.maps();
  InstanceType instance_type;
  if (p.instance_type().To(&instance_type)) {
    os << ", " << instance_type;
  }
  for (size_t i = 0; i < maps.size(); ++i) {
    os << ", " << Brief(*maps[i]);
  }
  return os;
}

bool operator==(MapsParameterInfo const& lhs, MapsParameterInfo const& rhs) {
  return lhs.maps() == rhs.maps();
}

bool operator!=(MapsParameterInfo const& lhs, MapsParameterInfo const& rhs) {
  return !(lhs == rhs);
}

size_t hash_value(MapsParameterInfo const& p) { return hash_value(p.maps()); }

bool operator==(CheckMapsParameters const& lhs,
                CheckMapsParameters const& rhs) {
  return lhs.flags() == rhs.flags() && lhs.maps() == rhs.maps() &&
         lhs.feedback() == rhs.feedback();
}

size_t hash_value(CheckMapsParameters const& p) {
  return base::hash_combine(p.flags(), p.maps(), p.feedback());
}

std::ostream& operator<<(std::ostream& os, CheckMapsParameters const& p) {
  os << p.flags() << p.maps_info();
  if (p.feedback().IsValid()) {
    os << "; " << p.feedback();
  }
  return os;
}

CheckMapsParameters const& CheckMapsParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCheckMaps, op->opcode());
  return OpParameter<CheckMapsParameters>(op);
}

MapsParameterInfo const& CompareMapsParametersOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kCompareMaps, op->opcode());
  return OpParameter<MapsParameterInfo>(op);
}

MapsParameterInfo const& MapGuardMapsOf(Operator const* op) {
  DCHECK_EQ(IrOpcode::kMapGuard, op->opcode());
  return OpParameter<MapsParameterInfo>(op);
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
                                               Type* value_type);

  Handle<Map> fast_map() const { return fast_map_; }
  Type* value_type() const { return value_type_; }

 private:
  Handle<Map> const fast_map_;
  Type* value_type_;
};

TransitionAndStoreNonNumberElementParameters::
    TransitionAndStoreNonNumberElementParameters(Handle<Map> fast_map,
                                                 Type* value_type)
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
  parameters.value_type()->PrintTo(os);
  return os << ", fast-map" << Brief(*parameters.fast_map());
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

Type* ValueTypeParameterOf(const Operator* op) {
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
  return base::hash_combine(info.type(), info.pretenure());
}

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           AllocateParameters info) {
  info.type()->PrintTo(os);
  return os << ", " << info.pretenure();
}

bool operator==(AllocateParameters const& lhs, AllocateParameters const& rhs) {
  return lhs.pretenure() == rhs.pretenure() && lhs.type() == rhs.type();
}

PretenureFlag PretenureFlagOf(const Operator* op) {
  if (op->opcode() == IrOpcode::kNewDoubleElements ||
      op->opcode() == IrOpcode::kNewSmiOrObjectElements) {
    return OpParameter<PretenureFlag>(op);
  }
  DCHECK(op->opcode() == IrOpcode::kAllocate ||
         op->opcode() == IrOpcode::kAllocateRaw);
  return OpParameter<AllocateParameters>(op).pretenure();
}

Type* AllocateTypeOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kAllocate, op->opcode());
  return OpParameter<AllocateParameters>(op).type();
}

UnicodeEncoding UnicodeEncodingOf(const Operator* op) {
  DCHECK(op->opcode() == IrOpcode::kStringFromSingleCodePoint ||
         op->opcode() == IrOpcode::kStringCodePointAt);
  return OpParameter<UnicodeEncoding>(op);
}

AbortReason AbortReasonOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kRuntimeAbort, op->opcode());
  return static_cast<AbortReason>(OpParameter<int>(op));
}

DeoptimizeReason DeoptimizeReasonOf(const Operator* op) {
  DCHECK_EQ(IrOpcode::kCheckIf, op->opcode());
  return OpParameter<DeoptimizeReason>(op);
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
  DCHECK(IrOpcode::kCheckedTaggedToInt32 == op->opcode() ||
         IrOpcode::kCheckedFloat64ToInt32 == op->opcode());
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

#define PURE_OP_LIST(V)                                          \
  V(BooleanNot, Operator::kNoProperties, 1, 0)                   \
  V(NumberEqual, Operator::kCommutative, 2, 0)                   \
  V(NumberLessThan, Operator::kNoProperties, 2, 0)               \
  V(NumberLessThanOrEqual, Operator::kNoProperties, 2, 0)        \
  V(NumberAdd, Operator::kCommutative, 2, 0)                     \
  V(NumberSubtract, Operator::kNoProperties, 2, 0)               \
  V(NumberMultiply, Operator::kCommutative, 2, 0)                \
  V(NumberDivide, Operator::kNoProperties, 2, 0)                 \
  V(NumberModulus, Operator::kNoProperties, 2, 0)                \
  V(NumberBitwiseOr, Operator::kCommutative, 2, 0)               \
  V(NumberBitwiseXor, Operator::kCommutative, 2, 0)              \
  V(NumberBitwiseAnd, Operator::kCommutative, 2, 0)              \
  V(NumberShiftLeft, Operator::kNoProperties, 2, 0)              \
  V(NumberShiftRight, Operator::kNoProperties, 2, 0)             \
  V(NumberShiftRightLogical, Operator::kNoProperties, 2, 0)      \
  V(NumberImul, Operator::kCommutative, 2, 0)                    \
  V(NumberAbs, Operator::kNoProperties, 1, 0)                    \
  V(NumberClz32, Operator::kNoProperties, 1, 0)                  \
  V(NumberCeil, Operator::kNoProperties, 1, 0)                   \
  V(NumberFloor, Operator::kNoProperties, 1, 0)                  \
  V(NumberFround, Operator::kNoProperties, 1, 0)                 \
  V(NumberAcos, Operator::kNoProperties, 1, 0)                   \
  V(NumberAcosh, Operator::kNoProperties, 1, 0)                  \
  V(NumberAsin, Operator::kNoProperties, 1, 0)                   \
  V(NumberAsinh, Operator::kNoProperties, 1, 0)                  \
  V(NumberAtan, Operator::kNoProperties, 1, 0)                   \
  V(NumberAtan2, Operator::kNoProperties, 2, 0)                  \
  V(NumberAtanh, Operator::kNoProperties, 1, 0)                  \
  V(NumberCbrt, Operator::kNoProperties, 1, 0)                   \
  V(NumberCos, Operator::kNoProperties, 1, 0)                    \
  V(NumberCosh, Operator::kNoProperties, 1, 0)                   \
  V(NumberExp, Operator::kNoProperties, 1, 0)                    \
  V(NumberExpm1, Operator::kNoProperties, 1, 0)                  \
  V(NumberLog, Operator::kNoProperties, 1, 0)                    \
  V(NumberLog1p, Operator::kNoProperties, 1, 0)                  \
  V(NumberLog10, Operator::kNoProperties, 1, 0)                  \
  V(NumberLog2, Operator::kNoProperties, 1, 0)                   \
  V(NumberMax, Operator::kNoProperties, 2, 0)                    \
  V(NumberMin, Operator::kNoProperties, 2, 0)                    \
  V(NumberPow, Operator::kNoProperties, 2, 0)                    \
  V(NumberRound, Operator::kNoProperties, 1, 0)                  \
  V(NumberSign, Operator::kNoProperties, 1, 0)                   \
  V(NumberSin, Operator::kNoProperties, 1, 0)                    \
  V(NumberSinh, Operator::kNoProperties, 1, 0)                   \
  V(NumberSqrt, Operator::kNoProperties, 1, 0)                   \
  V(NumberTan, Operator::kNoProperties, 1, 0)                    \
  V(NumberTanh, Operator::kNoProperties, 1, 0)                   \
  V(NumberTrunc, Operator::kNoProperties, 1, 0)                  \
  V(NumberToBoolean, Operator::kNoProperties, 1, 0)              \
  V(NumberToInt32, Operator::kNoProperties, 1, 0)                \
  V(NumberToString, Operator::kNoProperties, 1, 0)               \
  V(NumberToUint32, Operator::kNoProperties, 1, 0)               \
  V(NumberToUint8Clamped, Operator::kNoProperties, 1, 0)         \
  V(NumberSilenceNaN, Operator::kNoProperties, 1, 0)             \
  V(StringToNumber, Operator::kNoProperties, 1, 0)               \
  V(StringFromSingleCharCode, Operator::kNoProperties, 1, 0)     \
  V(StringIndexOf, Operator::kNoProperties, 3, 0)                \
  V(StringLength, Operator::kNoProperties, 1, 0)                 \
  V(StringToLowerCaseIntl, Operator::kNoProperties, 1, 0)        \
  V(StringToUpperCaseIntl, Operator::kNoProperties, 1, 0)        \
  V(TypeOf, Operator::kNoProperties, 1, 1)                       \
  V(PlainPrimitiveToNumber, Operator::kNoProperties, 1, 0)       \
  V(PlainPrimitiveToWord32, Operator::kNoProperties, 1, 0)       \
  V(PlainPrimitiveToFloat64, Operator::kNoProperties, 1, 0)      \
  V(ChangeTaggedSignedToInt32, Operator::kNoProperties, 1, 0)    \
  V(ChangeTaggedToInt32, Operator::kNoProperties, 1, 0)          \
  V(ChangeTaggedToUint32, Operator::kNoProperties, 1, 0)         \
  V(ChangeTaggedToFloat64, Operator::kNoProperties, 1, 0)        \
  V(ChangeTaggedToTaggedSigned, Operator::kNoProperties, 1, 0)   \
  V(ChangeFloat64ToTaggedPointer, Operator::kNoProperties, 1, 0) \
  V(ChangeInt31ToTaggedSigned, Operator::kNoProperties, 1, 0)    \
  V(ChangeInt32ToTagged, Operator::kNoProperties, 1, 0)          \
  V(ChangeUint32ToTagged, Operator::kNoProperties, 1, 0)         \
  V(ChangeTaggedToBit, Operator::kNoProperties, 1, 0)            \
  V(ChangeBitToTagged, Operator::kNoProperties, 1, 0)            \
  V(TruncateTaggedToBit, Operator::kNoProperties, 1, 0)          \
  V(TruncateTaggedPointerToBit, Operator::kNoProperties, 1, 0)   \
  V(TruncateTaggedToWord32, Operator::kNoProperties, 1, 0)       \
  V(TruncateTaggedToFloat64, Operator::kNoProperties, 1, 0)      \
  V(ObjectIsArrayBufferView, Operator::kNoProperties, 1, 0)      \
  V(ObjectIsBigInt, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsCallable, Operator::kNoProperties, 1, 0)             \
  V(ObjectIsConstructor, Operator::kNoProperties, 1, 0)          \
  V(ObjectIsDetectableCallable, Operator::kNoProperties, 1, 0)   \
  V(ObjectIsMinusZero, Operator::kNoProperties, 1, 0)            \
  V(ObjectIsNaN, Operator::kNoProperties, 1, 0)                  \
  V(ObjectIsNonCallable, Operator::kNoProperties, 1, 0)          \
  V(ObjectIsNumber, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsReceiver, Operator::kNoProperties, 1, 0)             \
  V(ObjectIsSmi, Operator::kNoProperties, 1, 0)                  \
  V(ObjectIsString, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsSymbol, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsUndetectable, Operator::kNoProperties, 1, 0)         \
  V(NumberIsFloat64Hole, Operator::kNoProperties, 1, 0)          \
  V(NumberIsFinite, Operator::kNoProperties, 1, 0)               \
  V(ObjectIsFiniteNumber, Operator::kNoProperties, 1, 0)         \
  V(NumberIsInteger, Operator::kNoProperties, 1, 0)              \
  V(ObjectIsSafeInteger, Operator::kNoProperties, 1, 0)          \
  V(NumberIsSafeInteger, Operator::kNoProperties, 1, 0)          \
  V(ObjectIsInteger, Operator::kNoProperties, 1, 0)              \
  V(ConvertTaggedHoleToUndefined, Operator::kNoProperties, 1, 0) \
  V(SameValue, Operator::kCommutative, 2, 0)                     \
  V(ReferenceEqual, Operator::kCommutative, 2, 0)                \
  V(StringEqual, Operator::kCommutative, 2, 0)                   \
  V(StringLessThan, Operator::kNoProperties, 2, 0)               \
  V(StringLessThanOrEqual, Operator::kNoProperties, 2, 0)        \
  V(ToBoolean, Operator::kNoProperties, 1, 0)                    \
  V(NewConsString, Operator::kNoProperties, 3, 0)                \
  V(MaskIndexWithBound, Operator::kNoProperties, 2, 0)

#define EFFECT_DEPENDENT_OP_LIST(V)                     \
  V(StringCharCodeAt, Operator::kNoProperties, 2, 1)    \
  V(StringSubstring, Operator::kNoProperties, 3, 1)

#define SPECULATIVE_NUMBER_BINOP_LIST(V)      \
  SIMPLIFIED_SPECULATIVE_NUMBER_BINOP_LIST(V) \
  V(SpeculativeNumberEqual)                   \
  V(SpeculativeNumberLessThan)                \
  V(SpeculativeNumberLessThanOrEqual)

#define CHECKED_OP_LIST(V)               \
  V(CheckEqualsInternalizedString, 2, 0) \
  V(CheckEqualsSymbol, 2, 0)             \
  V(CheckHeapObject, 1, 1)               \
  V(CheckInternalizedString, 1, 1)       \
  V(CheckNotTaggedHole, 1, 1)            \
  V(CheckReceiver, 1, 1)                 \
  V(CheckSymbol, 1, 1)                   \
  V(CheckedInt32Add, 2, 1)               \
  V(CheckedInt32Div, 2, 1)               \
  V(CheckedInt32Mod, 2, 1)               \
  V(CheckedInt32Sub, 2, 1)               \
  V(CheckedUint32Div, 2, 1)              \
  V(CheckedUint32Mod, 2, 1)

#define CHECKED_WITH_FEEDBACK_OP_LIST(V) \
  V(CheckBounds, 2, 1)                   \
  V(CheckNumber, 1, 1)                   \
  V(CheckSmi, 1, 1)                      \
  V(CheckString, 1, 1)                   \
  V(CheckedInt32ToTaggedSigned, 1, 1)    \
  V(CheckedTaggedSignedToInt32, 1, 1)    \
  V(CheckedTaggedToTaggedPointer, 1, 1)  \
  V(CheckedTaggedToTaggedSigned, 1, 1)   \
  V(CheckedUint32ToInt32, 1, 1)          \
  V(CheckedUint32ToTaggedSigned, 1, 1)

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

  template <DeoptimizeReason kDeoptimizeReason>
  struct CheckIfOperator final : public Operator1<DeoptimizeReason> {
    CheckIfOperator()
        : Operator1<DeoptimizeReason>(
              IrOpcode::kCheckIf, Operator::kFoldable | Operator::kNoThrow,
              "CheckIf", 1, 1, 1, 0, 1, 0, kDeoptimizeReason) {}
  };
#define CHECK_IF(Name, message) \
  CheckIfOperator<DeoptimizeReason::k##Name> kCheckIf##Name;
  DEOPTIMIZE_REASON_LIST(CHECK_IF)
#undef CHECK_IF

  template <UnicodeEncoding kEncoding>
  struct StringCodePointAtOperator final : public Operator1<UnicodeEncoding> {
    StringCodePointAtOperator()
        : Operator1<UnicodeEncoding>(IrOpcode::kStringCodePointAt,
                                     Operator::kFoldable | Operator::kNoThrow,
                                     "StringCodePointAt", 2, 1, 1, 1, 1, 0,
                                     kEncoding) {}
  };
  StringCodePointAtOperator<UnicodeEncoding::UTF16>
      kStringCodePointAtOperatorUTF16;
  StringCodePointAtOperator<UnicodeEncoding::UTF32>
      kStringCodePointAtOperatorUTF32;

  template <UnicodeEncoding kEncoding>
  struct StringFromSingleCodePointOperator final
      : public Operator1<UnicodeEncoding> {
    StringFromSingleCodePointOperator()
        : Operator1<UnicodeEncoding>(
              IrOpcode::kStringFromSingleCodePoint, Operator::kPure,
              "StringFromSingleCodePoint", 1, 0, 0, 1, 0, 0, kEncoding) {}
  };
  StringFromSingleCodePointOperator<UnicodeEncoding::UTF16>
      kStringFromSingleCodePointOperatorUTF16;
  StringFromSingleCodePointOperator<UnicodeEncoding::UTF32>
      kStringFromSingleCodePointOperatorUTF32;

  struct ArrayBufferWasNeuteredOperator final : public Operator {
    ArrayBufferWasNeuteredOperator()
        : Operator(IrOpcode::kArrayBufferWasNeutered, Operator::kEliminatable,
                   "ArrayBufferWasNeutered", 1, 1, 1, 1, 1, 0) {}
  };
  ArrayBufferWasNeuteredOperator kArrayBufferWasNeutered;

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
      : public Operator1<CheckFloat64HoleMode> {
    CheckFloat64HoleNaNOperator()
        : Operator1<CheckFloat64HoleMode>(
              IrOpcode::kCheckFloat64Hole,
              Operator::kFoldable | Operator::kNoThrow, "CheckFloat64Hole", 1,
              1, 1, 1, 1, 0, kMode) {}
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

static base::LazyInstance<SimplifiedOperatorGlobalCache>::type
    kSimplifiedOperatorGlobalCache = LAZY_INSTANCE_INITIALIZER;

SimplifiedOperatorBuilder::SimplifiedOperatorBuilder(Zone* zone)
    : cache_(kSimplifiedOperatorGlobalCache.Get()), zone_(zone) {}

#define GET_FROM_CACHE(Name, ...) \
  const Operator* SimplifiedOperatorBuilder::Name() { return &cache_.k##Name; }
PURE_OP_LIST(GET_FROM_CACHE)
EFFECT_DEPENDENT_OP_LIST(GET_FROM_CACHE)
CHECKED_OP_LIST(GET_FROM_CACHE)
GET_FROM_CACHE(ArrayBufferWasNeutered)
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

const Operator* SimplifiedOperatorBuilder::CheckIf(DeoptimizeReason reason) {
  switch (reason) {
#define CHECK_IF(Name, message)   \
  case DeoptimizeReason::k##Name: \
    return &cache_.kCheckIf##Name;
    DEOPTIMIZE_REASON_LIST(CHECK_IF)
#undef CHECK_IF
  }
  UNREACHABLE();
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
  return new (zone()) Operator1<MapsParameterInfo>(  // --
      IrOpcode::kMapGuard, Operator::kEliminatable,  // opcode
      "MapGuard",                                    // name
      1, 1, 1, 0, 1, 0,                              // counts
      MapsParameterInfo(maps));                      // parameter
}

const Operator* SimplifiedOperatorBuilder::CompareMaps(
    ZoneHandleSet<Map> maps) {
  return new (zone()) Operator1<MapsParameterInfo>(  // --
      IrOpcode::kCompareMaps,                        // opcode
      Operator::kEliminatable,                       // flags
      "CompareMaps",                                 // name
      1, 1, 1, 1, 1, 0,                              // counts
      MapsParameterInfo(maps));                      // parameter
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
    CheckFloat64HoleMode mode) {
  switch (mode) {
    case CheckFloat64HoleMode::kAllowReturnHole:
      return &cache_.kCheckFloat64HoleAllowReturnHoleOperator;
    case CheckFloat64HoleMode::kNeverReturnHole:
      return &cache_.kCheckFloat64HoleNeverReturnHoleOperator;
  }
  UNREACHABLE();
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
      Operator::kNoDeopt | Operator::kNoThrow,        // flags
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
#define MAKE_OR(name, arg2, arg3) op->opcode() == IrOpcode::k##name ||
  CHECK((CHECKED_WITH_FEEDBACK_OP_LIST(MAKE_OR) false));
#undef MAKE_OR
  return OpParameter<CheckParameters>(op);
}

const Operator* SimplifiedOperatorBuilder::NewDoubleElements(
    PretenureFlag pretenure) {
  return new (zone()) Operator1<PretenureFlag>(  // --
      IrOpcode::kNewDoubleElements,              // opcode
      Operator::kEliminatable,                   // flags
      "NewDoubleElements",                       // name
      1, 1, 1, 1, 1, 0,                          // counts
      pretenure);                                // parameter
}

const Operator* SimplifiedOperatorBuilder::NewSmiOrObjectElements(
    PretenureFlag pretenure) {
  return new (zone()) Operator1<PretenureFlag>(  // --
      IrOpcode::kNewSmiOrObjectElements,         // opcode
      Operator::kEliminatable,                   // flags
      "NewSmiOrObjectElements",                  // name
      1, 1, 1, 1, 1, 0,                          // counts
      pretenure);                                // parameter
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

const Operator* SimplifiedOperatorBuilder::Allocate(Type* type,
                                                    PretenureFlag pretenure) {
  return new (zone()) Operator1<AllocateParameters>(
      IrOpcode::kAllocate,
      Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite, "Allocate",
      1, 1, 1, 1, 1, 0, AllocateParameters(type, pretenure));
}

const Operator* SimplifiedOperatorBuilder::AllocateRaw(
    Type* type, PretenureFlag pretenure) {
  return new (zone()) Operator1<AllocateParameters>(
      IrOpcode::kAllocateRaw,
      Operator::kNoDeopt | Operator::kNoThrow | Operator::kNoWrite,
      "AllocateRaw", 1, 1, 1, 1, 1, 1, AllocateParameters(type, pretenure));
}

const Operator* SimplifiedOperatorBuilder::StringCodePointAt(
    UnicodeEncoding encoding) {
  switch (encoding) {
    case UnicodeEncoding::UTF16:
      return &cache_.kStringCodePointAtOperatorUTF16;
    case UnicodeEncoding::UTF32:
      return &cache_.kStringCodePointAtOperatorUTF32;
  }
  UNREACHABLE();
}

const Operator* SimplifiedOperatorBuilder::StringFromSingleCodePoint(
    UnicodeEncoding encoding) {
  switch (encoding) {
    case UnicodeEncoding::UTF16:
      return &cache_.kStringFromSingleCodePointOperatorUTF16;
    case UnicodeEncoding::UTF32:
      return &cache_.kStringFromSingleCodePointOperatorUTF32;
  }
  UNREACHABLE();
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

#define ACCESS_OP_LIST(V)                                             \
  V(LoadField, FieldAccess, Operator::kNoWrite, 1, 1, 1)              \
  V(StoreField, FieldAccess, Operator::kNoRead, 2, 1, 0)              \
  V(LoadElement, ElementAccess, Operator::kNoWrite, 2, 1, 1)          \
  V(StoreElement, ElementAccess, Operator::kNoRead, 3, 1, 0)          \
  V(LoadTypedElement, ExternalArrayType, Operator::kNoWrite, 4, 1, 1) \
  V(StoreTypedElement, ExternalArrayType, Operator::kNoRead, 5, 1, 0)

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
    Handle<Map> fast_map, Type* value_type) {
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
#undef CHECKED_OP_LIST
#undef ACCESS_OP_LIST

}  // namespace compiler
}  // namespace internal
}  // namespace v8
