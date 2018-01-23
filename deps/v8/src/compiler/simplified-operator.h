// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_H_

#include <iosfwd>

#include "src/base/compiler-specific.h"
#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/deoptimize-reason.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/machine-type.h"
#include "src/objects.h"
#include "src/type-hints.h"
#include "src/vector-slot-pair.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Zone;

namespace compiler {

// Forward declarations.
class Operator;
struct SimplifiedOperatorGlobalCache;

enum BaseTaggedness : uint8_t { kUntaggedBase, kTaggedBase };

size_t hash_value(BaseTaggedness);

std::ostream& operator<<(std::ostream&, BaseTaggedness);

// An access descriptor for loads/stores of fixed structures like field
// accesses of heap objects. Accesses from either tagged or untagged base
// pointers are supported; untagging is done automatically during lowering.
struct FieldAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int offset;                     // offset of the field, without tag.
  MaybeHandle<Name> name;         // debugging only.
  MaybeHandle<Map> map;           // map of the field value (if known).
  Type* type;                     // type of the field.
  MachineType machine_type;       // machine type of the field.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};

V8_EXPORT_PRIVATE bool operator==(FieldAccess const&, FieldAccess const&);

size_t hash_value(FieldAccess const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, FieldAccess const&);

FieldAccess const& FieldAccessOf(const Operator* op) WARN_UNUSED_RESULT;

template <>
void Operator1<FieldAccess>::PrintParameter(std::ostream& os,
                                            PrintVerbosity verbose) const;

// An access descriptor for loads/stores of indexed structures like characters
// in strings or off-heap backing stores. Accesses from either tagged or
// untagged base pointers are supported; untagging is done automatically during
// lowering.
struct ElementAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int header_size;                // size of the header, without tag.
  Type* type;                     // type of the element.
  MachineType machine_type;       // machine type of the element.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};

V8_EXPORT_PRIVATE bool operator==(ElementAccess const&, ElementAccess const&);

size_t hash_value(ElementAccess const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ElementAccess const&);

V8_EXPORT_PRIVATE ElementAccess const& ElementAccessOf(const Operator* op)
    WARN_UNUSED_RESULT;

ExternalArrayType ExternalArrayTypeOf(const Operator* op) WARN_UNUSED_RESULT;

// The ConvertReceiverMode is used as parameter by ConvertReceiver operators.
ConvertReceiverMode ConvertReceiverModeOf(Operator const* op);

// A the parameters for several Check nodes. The {feedback} parameter is
// optional. If {feedback} references a valid CallIC slot and this MapCheck
// fails, then speculation on that CallIC slot will be disabled.
class CheckParameters final {
 public:
  explicit CheckParameters(const VectorSlotPair& feedback)
      : feedback_(feedback) {}

  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  VectorSlotPair feedback_;
};

bool operator==(CheckParameters const&, CheckParameters const&);

size_t hash_value(CheckParameters const&);

std::ostream& operator<<(std::ostream&, CheckParameters const&);

CheckParameters const& CheckParametersOf(Operator const*) WARN_UNUSED_RESULT;

enum class CheckFloat64HoleMode : uint8_t {
  kNeverReturnHole,  // Never return the hole (deoptimize instead).
  kAllowReturnHole   // Allow to return the hole (signaling NaN).
};

size_t hash_value(CheckFloat64HoleMode);

std::ostream& operator<<(std::ostream&, CheckFloat64HoleMode);

CheckFloat64HoleMode CheckFloat64HoleModeOf(const Operator*) WARN_UNUSED_RESULT;

enum class CheckTaggedInputMode : uint8_t {
  kNumber,
  kNumberOrOddball,
};

size_t hash_value(CheckTaggedInputMode);

std::ostream& operator<<(std::ostream&, CheckTaggedInputMode);

CheckTaggedInputMode CheckTaggedInputModeOf(const Operator*);

class CheckTaggedInputParameters {
 public:
  CheckTaggedInputParameters(CheckTaggedInputMode mode,
                             const VectorSlotPair& feedback)
      : mode_(mode), feedback_(feedback) {}

  CheckTaggedInputMode mode() const { return mode_; }
  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  CheckTaggedInputMode mode_;
  VectorSlotPair feedback_;
};

const CheckTaggedInputParameters& CheckTaggedInputParametersOf(const Operator*)
    WARN_UNUSED_RESULT;

std::ostream& operator<<(std::ostream&,
                         const CheckTaggedInputParameters& params);

size_t hash_value(const CheckTaggedInputParameters& params);

bool operator==(CheckTaggedInputParameters const&,
                CheckTaggedInputParameters const&);

enum class CheckForMinusZeroMode : uint8_t {
  kCheckForMinusZero,
  kDontCheckForMinusZero,
};

size_t hash_value(CheckForMinusZeroMode);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           CheckForMinusZeroMode);

CheckForMinusZeroMode CheckMinusZeroModeOf(const Operator*) WARN_UNUSED_RESULT;

class CheckMinusZeroParameters {
 public:
  CheckMinusZeroParameters(CheckForMinusZeroMode mode,
                           const VectorSlotPair& feedback)
      : mode_(mode), feedback_(feedback) {}

  CheckForMinusZeroMode mode() const { return mode_; }
  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  CheckForMinusZeroMode mode_;
  VectorSlotPair feedback_;
};

const CheckMinusZeroParameters& CheckMinusZeroParametersOf(const Operator* op)
    WARN_UNUSED_RESULT;

std::ostream& operator<<(std::ostream&, const CheckMinusZeroParameters& params);

size_t hash_value(const CheckMinusZeroParameters& params);

bool operator==(CheckMinusZeroParameters const&,
                CheckMinusZeroParameters const&);

// Flags for map checks.
enum class CheckMapsFlag : uint8_t {
  kNone = 0u,
  kTryMigrateInstance = 1u << 0,  // Try instance migration.
};
typedef base::Flags<CheckMapsFlag> CheckMapsFlags;

DEFINE_OPERATORS_FOR_FLAGS(CheckMapsFlags)

std::ostream& operator<<(std::ostream&, CheckMapsFlags);

class MapsParameterInfo {
 public:
  explicit MapsParameterInfo(ZoneHandleSet<Map> const& maps);

  Maybe<InstanceType> instance_type() const { return instance_type_; }
  ZoneHandleSet<Map> const& maps() const { return maps_; }

 private:
  ZoneHandleSet<Map> const maps_;
  Maybe<InstanceType> instance_type_;
};

std::ostream& operator<<(std::ostream&, MapsParameterInfo const&);

bool operator==(MapsParameterInfo const&, MapsParameterInfo const&);
bool operator!=(MapsParameterInfo const&, MapsParameterInfo const&);

size_t hash_value(MapsParameterInfo const&);

// A descriptor for map checks. The {feedback} parameter is optional.
// If {feedback} references a valid CallIC slot and this MapCheck fails,
// then speculation on that CallIC slot will be disabled.
class CheckMapsParameters final {
 public:
  CheckMapsParameters(CheckMapsFlags flags, ZoneHandleSet<Map> const& maps,
                      const VectorSlotPair& feedback)
      : flags_(flags), maps_info_(maps), feedback_(feedback) {}

  CheckMapsFlags flags() const { return flags_; }
  ZoneHandleSet<Map> const& maps() const { return maps_info_.maps(); }
  MapsParameterInfo const& maps_info() const { return maps_info_; }
  VectorSlotPair const& feedback() const { return feedback_; }

 private:
  CheckMapsFlags const flags_;
  MapsParameterInfo const maps_info_;
  VectorSlotPair const feedback_;
};

bool operator==(CheckMapsParameters const&, CheckMapsParameters const&);

size_t hash_value(CheckMapsParameters const&);

std::ostream& operator<<(std::ostream&, CheckMapsParameters const&);

CheckMapsParameters const& CheckMapsParametersOf(Operator const*)
    WARN_UNUSED_RESULT;

MapsParameterInfo const& MapGuardMapsOf(Operator const*) WARN_UNUSED_RESULT;

// Parameters for CompareMaps operator.
MapsParameterInfo const& CompareMapsParametersOf(Operator const*)
    WARN_UNUSED_RESULT;

// A descriptor for growing elements backing stores.
enum class GrowFastElementsMode : uint8_t {
  kDoubleElements,
  kSmiOrObjectElements
};

inline size_t hash_value(GrowFastElementsMode mode) {
  return static_cast<uint8_t>(mode);
}

std::ostream& operator<<(std::ostream&, GrowFastElementsMode);

class GrowFastElementsParameters {
 public:
  GrowFastElementsParameters(GrowFastElementsMode mode,
                             const VectorSlotPair& feedback)
      : mode_(mode), feedback_(feedback) {}

  GrowFastElementsMode mode() const { return mode_; }
  const VectorSlotPair& feedback() const { return feedback_; }

 private:
  GrowFastElementsMode mode_;
  VectorSlotPair feedback_;
};

bool operator==(const GrowFastElementsParameters&,
                const GrowFastElementsParameters&);

inline size_t hash_value(const GrowFastElementsParameters&);

std::ostream& operator<<(std::ostream&, const GrowFastElementsParameters&);

const GrowFastElementsParameters& GrowFastElementsParametersOf(const Operator*)
    WARN_UNUSED_RESULT;

// A descriptor for elements kind transitions.
class ElementsTransition final {
 public:
  enum Mode : uint8_t {
    kFastTransition,  // simple transition, just updating the map.
    kSlowTransition   // full transition, round-trip to the runtime.
  };

  ElementsTransition(Mode mode, Handle<Map> source, Handle<Map> target)
      : mode_(mode), source_(source), target_(target) {}

  Mode mode() const { return mode_; }
  Handle<Map> source() const { return source_; }
  Handle<Map> target() const { return target_; }

 private:
  Mode const mode_;
  Handle<Map> const source_;
  Handle<Map> const target_;
};

bool operator==(ElementsTransition const&, ElementsTransition const&);

size_t hash_value(ElementsTransition);

std::ostream& operator<<(std::ostream&, ElementsTransition);

ElementsTransition const& ElementsTransitionOf(const Operator* op)
    WARN_UNUSED_RESULT;

// Parameters for TransitionAndStoreElement, or
// TransitionAndStoreNonNumberElement, or
// TransitionAndStoreNumberElement.
Handle<Map> DoubleMapParameterOf(const Operator* op);
Handle<Map> FastMapParameterOf(const Operator* op);

// Parameters for TransitionAndStoreNonNumberElement.
Type* ValueTypeParameterOf(const Operator* op);

// A hint for speculative number operations.
enum class NumberOperationHint : uint8_t {
  kSignedSmall,        // Inputs were Smi, output was in Smi.
  kSignedSmallInputs,  // Inputs were Smi, output was Number.
  kSigned32,           // Inputs were Signed32, output was Number.
  kNumber,             // Inputs were Number, output was Number.
  kNumberOrOddball,    // Inputs were Number or Oddball, output was Number.
};

size_t hash_value(NumberOperationHint);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, NumberOperationHint);

NumberOperationHint NumberOperationHintOf(const Operator* op)
    WARN_UNUSED_RESULT;

int FormalParameterCountOf(const Operator* op) WARN_UNUSED_RESULT;
bool IsRestLengthOf(const Operator* op) WARN_UNUSED_RESULT;

class AllocateParameters {
 public:
  AllocateParameters(Type* type, PretenureFlag pretenure)
      : type_(type), pretenure_(pretenure) {}

  Type* type() const { return type_; }
  PretenureFlag pretenure() const { return pretenure_; }

 private:
  Type* type_;
  PretenureFlag pretenure_;
};

bool IsCheckedWithFeedback(const Operator* op);

size_t hash_value(AllocateParameters);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, AllocateParameters);

bool operator==(AllocateParameters const&, AllocateParameters const&);

PretenureFlag PretenureFlagOf(const Operator* op) WARN_UNUSED_RESULT;

Type* AllocateTypeOf(const Operator* op) WARN_UNUSED_RESULT;

UnicodeEncoding UnicodeEncodingOf(const Operator*) WARN_UNUSED_RESULT;

AbortReason AbortReasonOf(const Operator* op) WARN_UNUSED_RESULT;

DeoptimizeReason DeoptimizeReasonOf(const Operator* op) WARN_UNUSED_RESULT;

// Interface for building simplified operators, which represent the
// medium-level operations of V8, including adding numbers, allocating objects,
// indexing into objects and arrays, etc.
// All operators are typed but many are representation independent.

// Number values from JS can be in one of these representations:
//   - Tagged: word-sized integer that is either
//     - a signed small integer (31 or 32 bits plus a tag)
//     - a tagged pointer to a HeapNumber object that has a float64 field
//   - Int32: an untagged signed 32-bit integer
//   - Uint32: an untagged unsigned 32-bit integer
//   - Float64: an untagged float64

// Additional representations for intermediate code or non-JS code:
//   - Int64: an untagged signed 64-bit integer
//   - Uint64: an untagged unsigned 64-bit integer
//   - Float32: an untagged float32

// Boolean values can be:
//   - Bool: a tagged pointer to either the canonical JS #false or
//           the canonical JS #true object
//   - Bit: an untagged integer 0 or 1, but word-sized
class V8_EXPORT_PRIVATE SimplifiedOperatorBuilder final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  explicit SimplifiedOperatorBuilder(Zone* zone);

  const Operator* BooleanNot();

  const Operator* NumberEqual();
  const Operator* NumberLessThan();
  const Operator* NumberLessThanOrEqual();
  const Operator* NumberAdd();
  const Operator* NumberSubtract();
  const Operator* NumberMultiply();
  const Operator* NumberDivide();
  const Operator* NumberModulus();
  const Operator* NumberBitwiseOr();
  const Operator* NumberBitwiseXor();
  const Operator* NumberBitwiseAnd();
  const Operator* NumberShiftLeft();
  const Operator* NumberShiftRight();
  const Operator* NumberShiftRightLogical();
  const Operator* NumberImul();
  const Operator* NumberAbs();
  const Operator* NumberClz32();
  const Operator* NumberCeil();
  const Operator* NumberFloor();
  const Operator* NumberFround();
  const Operator* NumberAcos();
  const Operator* NumberAcosh();
  const Operator* NumberAsin();
  const Operator* NumberAsinh();
  const Operator* NumberAtan();
  const Operator* NumberAtan2();
  const Operator* NumberAtanh();
  const Operator* NumberCbrt();
  const Operator* NumberCos();
  const Operator* NumberCosh();
  const Operator* NumberExp();
  const Operator* NumberExpm1();
  const Operator* NumberLog();
  const Operator* NumberLog1p();
  const Operator* NumberLog10();
  const Operator* NumberLog2();
  const Operator* NumberMax();
  const Operator* NumberMin();
  const Operator* NumberPow();
  const Operator* NumberRound();
  const Operator* NumberSign();
  const Operator* NumberSin();
  const Operator* NumberSinh();
  const Operator* NumberSqrt();
  const Operator* NumberTan();
  const Operator* NumberTanh();
  const Operator* NumberTrunc();
  const Operator* NumberToBoolean();
  const Operator* NumberToInt32();
  const Operator* NumberToString();
  const Operator* NumberToUint32();
  const Operator* NumberToUint8Clamped();

  const Operator* NumberSilenceNaN();

  const Operator* SpeculativeSafeIntegerAdd(NumberOperationHint hint);
  const Operator* SpeculativeSafeIntegerSubtract(NumberOperationHint hint);

  const Operator* SpeculativeNumberAdd(NumberOperationHint hint);
  const Operator* SpeculativeNumberSubtract(NumberOperationHint hint);
  const Operator* SpeculativeNumberMultiply(NumberOperationHint hint);
  const Operator* SpeculativeNumberDivide(NumberOperationHint hint);
  const Operator* SpeculativeNumberModulus(NumberOperationHint hint);
  const Operator* SpeculativeNumberShiftLeft(NumberOperationHint hint);
  const Operator* SpeculativeNumberShiftRight(NumberOperationHint hint);
  const Operator* SpeculativeNumberShiftRightLogical(NumberOperationHint hint);
  const Operator* SpeculativeNumberBitwiseAnd(NumberOperationHint hint);
  const Operator* SpeculativeNumberBitwiseOr(NumberOperationHint hint);
  const Operator* SpeculativeNumberBitwiseXor(NumberOperationHint hint);

  const Operator* SpeculativeNumberLessThan(NumberOperationHint hint);
  const Operator* SpeculativeNumberLessThanOrEqual(NumberOperationHint hint);
  const Operator* SpeculativeNumberEqual(NumberOperationHint hint);

  const Operator* ReferenceEqual();
  const Operator* SameValue();

  const Operator* TypeOf();
  const Operator* ClassOf();

  const Operator* ToBoolean();

  const Operator* StringEqual();
  const Operator* StringLessThan();
  const Operator* StringLessThanOrEqual();
  const Operator* StringCharAt();
  const Operator* StringCharCodeAt();
  const Operator* SeqStringCharCodeAt();
  const Operator* StringCodePointAt();
  const Operator* SeqStringCodePointAt();
  const Operator* StringFromCharCode();
  const Operator* StringFromCodePoint(UnicodeEncoding encoding);
  const Operator* StringIndexOf();
  const Operator* StringLength();
  const Operator* StringToLowerCaseIntl();
  const Operator* StringToUpperCaseIntl();

  const Operator* FindOrderedHashMapEntry();
  const Operator* FindOrderedHashMapEntryForInt32Key();

  const Operator* SpeculativeToNumber(NumberOperationHint hint);

  const Operator* StringToNumber();
  const Operator* PlainPrimitiveToNumber();
  const Operator* PlainPrimitiveToWord32();
  const Operator* PlainPrimitiveToFloat64();

  const Operator* ChangeTaggedSignedToInt32();
  const Operator* ChangeTaggedToInt32();
  const Operator* ChangeTaggedToUint32();
  const Operator* ChangeTaggedToFloat64();
  const Operator* ChangeTaggedToTaggedSigned();
  const Operator* ChangeInt31ToTaggedSigned();
  const Operator* ChangeInt32ToTagged();
  const Operator* ChangeUint32ToTagged();
  const Operator* ChangeFloat64ToTagged(CheckForMinusZeroMode);
  const Operator* ChangeFloat64ToTaggedPointer();
  const Operator* ChangeTaggedToBit();
  const Operator* ChangeBitToTagged();
  const Operator* TruncateTaggedToWord32();
  const Operator* TruncateTaggedToFloat64();
  const Operator* TruncateTaggedToBit();
  const Operator* TruncateTaggedPointerToBit();

  const Operator* MaskIndexWithBound();
  const Operator* CompareMaps(ZoneHandleSet<Map>);
  const Operator* MapGuard(ZoneHandleSet<Map> maps);

  const Operator* CheckBounds(const VectorSlotPair& feedback);
  const Operator* CheckEqualsInternalizedString();
  const Operator* CheckEqualsSymbol();
  const Operator* CheckFloat64Hole(CheckFloat64HoleMode);
  const Operator* CheckHeapObject();
  const Operator* CheckIf(DeoptimizeReason deoptimize_reason);
  const Operator* CheckInternalizedString();
  const Operator* CheckMaps(CheckMapsFlags, ZoneHandleSet<Map>,
                            const VectorSlotPair& = VectorSlotPair());
  const Operator* CheckNotTaggedHole();
  const Operator* CheckNumber(const VectorSlotPair& feedback);
  const Operator* CheckReceiver();
  const Operator* CheckSeqString();
  const Operator* CheckSmi(const VectorSlotPair& feedback);
  const Operator* CheckString(const VectorSlotPair& feedback);
  const Operator* CheckSymbol();

  const Operator* CheckedFloat64ToInt32(CheckForMinusZeroMode,
                                        const VectorSlotPair& feedback);
  const Operator* CheckedInt32Add();
  const Operator* CheckedInt32Div();
  const Operator* CheckedInt32Mod();
  const Operator* CheckedInt32Mul(CheckForMinusZeroMode);
  const Operator* CheckedInt32Sub();
  const Operator* CheckedInt32ToTaggedSigned(const VectorSlotPair& feedback);
  const Operator* CheckedTaggedSignedToInt32(const VectorSlotPair& feedback);
  const Operator* CheckedTaggedToFloat64(CheckTaggedInputMode);
  const Operator* CheckedTaggedToInt32(CheckForMinusZeroMode,
                                       const VectorSlotPair& feedback);
  const Operator* CheckedTaggedToTaggedPointer(const VectorSlotPair& feedback);
  const Operator* CheckedTaggedToTaggedSigned(const VectorSlotPair& feedback);
  const Operator* CheckedTruncateTaggedToWord32(CheckTaggedInputMode,
                                                const VectorSlotPair& feedback);
  const Operator* CheckedUint32Div();
  const Operator* CheckedUint32Mod();
  const Operator* CheckedUint32ToInt32(const VectorSlotPair& feedback);
  const Operator* CheckedUint32ToTaggedSigned(const VectorSlotPair& feedback);

  const Operator* ConvertReceiver(ConvertReceiverMode);

  const Operator* ConvertTaggedHoleToUndefined();

  const Operator* ObjectIsArrayBufferView();
  const Operator* ObjectIsBigInt();
  const Operator* ObjectIsCallable();
  const Operator* ObjectIsConstructor();
  const Operator* ObjectIsDetectableCallable();
  const Operator* ObjectIsMinusZero();
  const Operator* ObjectIsNaN();
  const Operator* ObjectIsNonCallable();
  const Operator* ObjectIsNumber();
  const Operator* ObjectIsReceiver();
  const Operator* ObjectIsSmi();
  const Operator* ObjectIsString();
  const Operator* ObjectIsSymbol();
  const Operator* ObjectIsUndetectable();

  const Operator* NumberIsFloat64Hole();

  const Operator* ArgumentsFrame();
  const Operator* ArgumentsLength(int formal_parameter_count,
                                  bool is_rest_length);

  const Operator* NewDoubleElements(PretenureFlag);
  const Operator* NewSmiOrObjectElements(PretenureFlag);

  // new-arguments-elements arguments-frame, arguments-length
  const Operator* NewArgumentsElements(int mapped_count);

  // new-cons-string length, first, second
  const Operator* NewConsString();

  // array-buffer-was-neutered buffer
  const Operator* ArrayBufferWasNeutered();

  // ensure-writable-fast-elements object, elements
  const Operator* EnsureWritableFastElements();

  // maybe-grow-fast-elements object, elements, index, length
  const Operator* MaybeGrowFastElements(GrowFastElementsMode mode,
                                        const VectorSlotPair& feedback);

  // transition-elements-kind object, from-map, to-map
  const Operator* TransitionElementsKind(ElementsTransition transition);

  const Operator* Allocate(Type* type, PretenureFlag pretenure = NOT_TENURED);
  const Operator* AllocateRaw(Type* type,
                              PretenureFlag pretenure = NOT_TENURED);

  const Operator* LoadFieldByIndex();
  const Operator* LoadField(FieldAccess const&);
  const Operator* StoreField(FieldAccess const&);

  // load-element [base + index]
  const Operator* LoadElement(ElementAccess const&);

  // store-element [base + index], value
  const Operator* StoreElement(ElementAccess const&);

  // store-element [base + index], value, only with fast arrays.
  const Operator* TransitionAndStoreElement(Handle<Map> double_map,
                                            Handle<Map> fast_map);
  // store-element [base + index], smi value, only with fast arrays.
  const Operator* StoreSignedSmallElement();

  // store-element [base + index], double value, only with fast arrays.
  const Operator* TransitionAndStoreNumberElement(Handle<Map> double_map);

  // store-element [base + index], object value, only with fast arrays.
  const Operator* TransitionAndStoreNonNumberElement(Handle<Map> fast_map,
                                                     Type* value_type);

  // load-typed-element buffer, [base + external + index]
  const Operator* LoadTypedElement(ExternalArrayType const&);

  // store-typed-element buffer, [base + external + index], value
  const Operator* StoreTypedElement(ExternalArrayType const&);

  // Abort (for terminating execution on internal error).
  const Operator* RuntimeAbort(AbortReason reason);

 private:
  Zone* zone() const { return zone_; }

  const SimplifiedOperatorGlobalCache& cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(SimplifiedOperatorBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_OPERATOR_H_
