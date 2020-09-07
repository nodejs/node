// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_H_

#include <iosfwd>

#include "src/base/compiler-specific.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/objects.h"
#include "src/objects/type-hints.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {

// Forward declarations.
enum class AbortReason : uint8_t;
class Zone;

namespace compiler {

// Forward declarations.
class Operator;
struct SimplifiedOperatorGlobalCache;

enum BaseTaggedness : uint8_t { kUntaggedBase, kTaggedBase };

size_t hash_value(BaseTaggedness);

std::ostream& operator<<(std::ostream&, BaseTaggedness);

size_t hash_value(LoadSensitivity);

std::ostream& operator<<(std::ostream&, LoadSensitivity);

struct ConstFieldInfo {
  // the map that introduced the const field, if any. An access is considered
  // mutable iff the handle is null.
  MaybeHandle<Map> owner_map;

  ConstFieldInfo() : owner_map(MaybeHandle<Map>()) {}
  explicit ConstFieldInfo(Handle<Map> owner_map) : owner_map(owner_map) {}

  bool IsConst() const { return !owner_map.is_null(); }

  // No const field owner, i.e., a mutable field
  static ConstFieldInfo None() { return ConstFieldInfo(); }
};

V8_EXPORT_PRIVATE bool operator==(ConstFieldInfo const&, ConstFieldInfo const&);

size_t hash_value(ConstFieldInfo const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           ConstFieldInfo const&);

// An access descriptor for loads/stores of fixed structures like field
// accesses of heap objects. Accesses from either tagged or untagged base
// pointers are supported; untagging is done automatically during lowering.
struct FieldAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int offset;                     // offset of the field, without tag.
  MaybeHandle<Name> name;         // debugging only.
  MaybeHandle<Map> map;           // map of the field value (if known).
  Type type;                      // type of the field.
  MachineType machine_type;       // machine type of the field.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.
  LoadSensitivity load_sensitivity;     // load safety for poisoning.
  ConstFieldInfo const_field_info;      // the constness of this access, and the
                                    // field owner map, if the access is const
  bool is_store_in_literal;  // originates from a kStoreInLiteral access

  FieldAccess()
      : base_is_tagged(kTaggedBase),
        offset(0),
        type(Type::None()),
        machine_type(MachineType::None()),
        write_barrier_kind(kFullWriteBarrier),
        load_sensitivity(LoadSensitivity::kUnsafe),
        const_field_info(ConstFieldInfo::None()),
        is_store_in_literal(false) {}

  FieldAccess(BaseTaggedness base_is_tagged, int offset, MaybeHandle<Name> name,
              MaybeHandle<Map> map, Type type, MachineType machine_type,
              WriteBarrierKind write_barrier_kind,
              LoadSensitivity load_sensitivity = LoadSensitivity::kUnsafe,
              ConstFieldInfo const_field_info = ConstFieldInfo::None(),
              bool is_store_in_literal = false)
      : base_is_tagged(base_is_tagged),
        offset(offset),
        name(name),
        map(map),
        type(type),
        machine_type(machine_type),
        write_barrier_kind(write_barrier_kind),
        load_sensitivity(load_sensitivity),
        const_field_info(const_field_info),
        is_store_in_literal(is_store_in_literal) {
    DCHECK_GE(offset, 0);
  }

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};

V8_EXPORT_PRIVATE bool operator==(FieldAccess const&, FieldAccess const&);

size_t hash_value(FieldAccess const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, FieldAccess const&);

V8_EXPORT_PRIVATE FieldAccess const& FieldAccessOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

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
  Type type;                      // type of the element.
  MachineType machine_type;       // machine type of the element.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.
  LoadSensitivity load_sensitivity;     // load safety for poisoning.

  ElementAccess()
      : base_is_tagged(kTaggedBase),
        header_size(0),
        type(Type::None()),
        machine_type(MachineType::None()),
        write_barrier_kind(kFullWriteBarrier),
        load_sensitivity(LoadSensitivity::kUnsafe) {}

  ElementAccess(BaseTaggedness base_is_tagged, int header_size, Type type,
                MachineType machine_type, WriteBarrierKind write_barrier_kind,
                LoadSensitivity load_sensitivity = LoadSensitivity::kUnsafe)
      : base_is_tagged(base_is_tagged),
        header_size(header_size),
        type(type),
        machine_type(machine_type),
        write_barrier_kind(write_barrier_kind),
        load_sensitivity(load_sensitivity) {}

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};

V8_EXPORT_PRIVATE bool operator==(ElementAccess const&, ElementAccess const&);

size_t hash_value(ElementAccess const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ElementAccess const&);

V8_EXPORT_PRIVATE ElementAccess const& ElementAccessOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

ExternalArrayType ExternalArrayTypeOf(const Operator* op) V8_WARN_UNUSED_RESULT;

// An access descriptor for loads/stores of CSA-accessible structures.
struct ObjectAccess {
  MachineType machine_type;             // machine type of the field.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.

  ObjectAccess()
      : machine_type(MachineType::None()),
        write_barrier_kind(kFullWriteBarrier) {}

  ObjectAccess(MachineType machine_type, WriteBarrierKind write_barrier_kind)
      : machine_type(machine_type), write_barrier_kind(write_barrier_kind) {}

  int tag() const { return kHeapObjectTag; }
};

V8_EXPORT_PRIVATE bool operator==(ObjectAccess const&, ObjectAccess const&);

size_t hash_value(ObjectAccess const&);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, ObjectAccess const&);

V8_EXPORT_PRIVATE ObjectAccess const& ObjectAccessOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

// The ConvertReceiverMode is used as parameter by ConvertReceiver operators.
ConvertReceiverMode ConvertReceiverModeOf(Operator const* op)
    V8_WARN_UNUSED_RESULT;

// A the parameters for several Check nodes. The {feedback} parameter is
// optional. If {feedback} references a valid CallIC slot and this MapCheck
// fails, then speculation on that CallIC slot will be disabled.
class CheckParameters final {
 public:
  explicit CheckParameters(const FeedbackSource& feedback)
      : feedback_(feedback) {}

  FeedbackSource const& feedback() const { return feedback_; }

 private:
  FeedbackSource feedback_;
};

bool operator==(CheckParameters const&, CheckParameters const&);

size_t hash_value(CheckParameters const&);

std::ostream& operator<<(std::ostream&, CheckParameters const&);

CheckParameters const& CheckParametersOf(Operator const*) V8_WARN_UNUSED_RESULT;

enum class CheckBoundsFlag : uint8_t {
  kConvertStringAndMinusZero = 1 << 0,  // instead of deopting on such inputs
  kAbortOnOutOfBounds = 1 << 1,         // instead of deopting if input is OOB
};
using CheckBoundsFlags = base::Flags<CheckBoundsFlag>;
DEFINE_OPERATORS_FOR_FLAGS(CheckBoundsFlags)

class CheckBoundsParameters final {
 public:
  CheckBoundsParameters(const FeedbackSource& feedback, CheckBoundsFlags flags)
      : check_parameters_(feedback), flags_(flags) {}

  CheckBoundsFlags flags() const { return flags_; }
  const CheckParameters& check_parameters() const { return check_parameters_; }

 private:
  CheckParameters check_parameters_;
  CheckBoundsFlags flags_;
};

bool operator==(CheckBoundsParameters const&, CheckBoundsParameters const&);

size_t hash_value(CheckBoundsParameters const&);

std::ostream& operator<<(std::ostream&, CheckBoundsParameters const&);

CheckBoundsParameters const& CheckBoundsParametersOf(Operator const*)
    V8_WARN_UNUSED_RESULT;

class CheckIfParameters final {
 public:
  explicit CheckIfParameters(DeoptimizeReason reason,
                             const FeedbackSource& feedback)
      : reason_(reason), feedback_(feedback) {}

  FeedbackSource const& feedback() const { return feedback_; }
  DeoptimizeReason reason() const { return reason_; }

 private:
  DeoptimizeReason reason_;
  FeedbackSource feedback_;
};

bool operator==(CheckIfParameters const&, CheckIfParameters const&);

size_t hash_value(CheckIfParameters const&);

std::ostream& operator<<(std::ostream&, CheckIfParameters const&);

CheckIfParameters const& CheckIfParametersOf(Operator const*)
    V8_WARN_UNUSED_RESULT;

enum class CheckFloat64HoleMode : uint8_t {
  kNeverReturnHole,  // Never return the hole (deoptimize instead).
  kAllowReturnHole   // Allow to return the hole (signaling NaN).
};

size_t hash_value(CheckFloat64HoleMode);

std::ostream& operator<<(std::ostream&, CheckFloat64HoleMode);

class CheckFloat64HoleParameters {
 public:
  CheckFloat64HoleParameters(CheckFloat64HoleMode mode,
                             FeedbackSource const& feedback)
      : mode_(mode), feedback_(feedback) {}

  CheckFloat64HoleMode mode() const { return mode_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  CheckFloat64HoleMode mode_;
  FeedbackSource feedback_;
};

CheckFloat64HoleParameters const& CheckFloat64HoleParametersOf(Operator const*)
    V8_WARN_UNUSED_RESULT;

std::ostream& operator<<(std::ostream&, CheckFloat64HoleParameters const&);

size_t hash_value(CheckFloat64HoleParameters const&);

bool operator==(CheckFloat64HoleParameters const&,
                CheckFloat64HoleParameters const&);
bool operator!=(CheckFloat64HoleParameters const&,
                CheckFloat64HoleParameters const&);

// Parameter for CheckClosure node.
Handle<FeedbackCell> FeedbackCellOf(const Operator* op);

enum class CheckTaggedInputMode : uint8_t {
  kNumber,
  kNumberOrBoolean,
  kNumberOrOddball,
};

size_t hash_value(CheckTaggedInputMode);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, CheckTaggedInputMode);

class CheckTaggedInputParameters {
 public:
  CheckTaggedInputParameters(CheckTaggedInputMode mode,
                             const FeedbackSource& feedback)
      : mode_(mode), feedback_(feedback) {}

  CheckTaggedInputMode mode() const { return mode_; }
  const FeedbackSource& feedback() const { return feedback_; }

 private:
  CheckTaggedInputMode mode_;
  FeedbackSource feedback_;
};

const CheckTaggedInputParameters& CheckTaggedInputParametersOf(const Operator*)
    V8_WARN_UNUSED_RESULT;

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

CheckForMinusZeroMode CheckMinusZeroModeOf(const Operator*)
    V8_WARN_UNUSED_RESULT;

class CheckMinusZeroParameters {
 public:
  CheckMinusZeroParameters(CheckForMinusZeroMode mode,
                           const FeedbackSource& feedback)
      : mode_(mode), feedback_(feedback) {}

  CheckForMinusZeroMode mode() const { return mode_; }
  const FeedbackSource& feedback() const { return feedback_; }

 private:
  CheckForMinusZeroMode mode_;
  FeedbackSource feedback_;
};

V8_EXPORT_PRIVATE const CheckMinusZeroParameters& CheckMinusZeroParametersOf(
    const Operator* op) V8_WARN_UNUSED_RESULT;

V8_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream&, const CheckMinusZeroParameters& params);

size_t hash_value(const CheckMinusZeroParameters& params);

bool operator==(CheckMinusZeroParameters const&,
                CheckMinusZeroParameters const&);

enum class CheckMapsFlag : uint8_t {
  kNone = 0u,
  kTryMigrateInstance = 1u << 0,
};
using CheckMapsFlags = base::Flags<CheckMapsFlag>;

DEFINE_OPERATORS_FOR_FLAGS(CheckMapsFlags)

std::ostream& operator<<(std::ostream&, CheckMapsFlags);

// A descriptor for map checks. The {feedback} parameter is optional.
// If {feedback} references a valid CallIC slot and this MapCheck fails,
// then speculation on that CallIC slot will be disabled.
class CheckMapsParameters final {
 public:
  CheckMapsParameters(CheckMapsFlags flags, ZoneHandleSet<Map> const& maps,
                      const FeedbackSource& feedback)
      : flags_(flags), maps_(maps), feedback_(feedback) {}

  CheckMapsFlags flags() const { return flags_; }
  ZoneHandleSet<Map> const& maps() const { return maps_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  CheckMapsFlags const flags_;
  ZoneHandleSet<Map> const maps_;
  FeedbackSource const feedback_;
};

bool operator==(CheckMapsParameters const&, CheckMapsParameters const&);

size_t hash_value(CheckMapsParameters const&);

std::ostream& operator<<(std::ostream&, CheckMapsParameters const&);

CheckMapsParameters const& CheckMapsParametersOf(Operator const*)
    V8_WARN_UNUSED_RESULT;

ZoneHandleSet<Map> const& MapGuardMapsOf(Operator const*) V8_WARN_UNUSED_RESULT;

// Parameters for CompareMaps operator.
ZoneHandleSet<Map> const& CompareMapsParametersOf(Operator const*)
    V8_WARN_UNUSED_RESULT;

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
                             const FeedbackSource& feedback)
      : mode_(mode), feedback_(feedback) {}

  GrowFastElementsMode mode() const { return mode_; }
  const FeedbackSource& feedback() const { return feedback_; }

 private:
  GrowFastElementsMode mode_;
  FeedbackSource feedback_;
};

bool operator==(const GrowFastElementsParameters&,
                const GrowFastElementsParameters&);

inline size_t hash_value(const GrowFastElementsParameters&);

std::ostream& operator<<(std::ostream&, const GrowFastElementsParameters&);

const GrowFastElementsParameters& GrowFastElementsParametersOf(const Operator*)
    V8_WARN_UNUSED_RESULT;

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
    V8_WARN_UNUSED_RESULT;

// Parameters for TransitionAndStoreElement, or
// TransitionAndStoreNonNumberElement, or
// TransitionAndStoreNumberElement.
Handle<Map> DoubleMapParameterOf(const Operator* op) V8_WARN_UNUSED_RESULT;
Handle<Map> FastMapParameterOf(const Operator* op) V8_WARN_UNUSED_RESULT;

// Parameters for TransitionAndStoreNonNumberElement.
Type ValueTypeParameterOf(const Operator* op) V8_WARN_UNUSED_RESULT;

// A hint for speculative number operations.
enum class NumberOperationHint : uint8_t {
  kSignedSmall,        // Inputs were Smi, output was in Smi.
  kSignedSmallInputs,  // Inputs were Smi, output was Number.
  kSigned32,           // Inputs were Signed32, output was Number.
  kNumber,             // Inputs were Number, output was Number.
  kNumberOrBoolean,    // Inputs were Number or Boolean, output was Number.
  kNumberOrOddball,    // Inputs were Number or Oddball, output was Number.
};

enum class BigIntOperationHint : uint8_t {
  kBigInt,
};

size_t hash_value(NumberOperationHint);
size_t hash_value(BigIntOperationHint);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, NumberOperationHint);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, BigIntOperationHint);
V8_EXPORT_PRIVATE NumberOperationHint NumberOperationHintOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

class NumberOperationParameters {
 public:
  NumberOperationParameters(NumberOperationHint hint,
                            const FeedbackSource& feedback)
      : hint_(hint), feedback_(feedback) {}

  NumberOperationHint hint() const { return hint_; }
  const FeedbackSource& feedback() const { return feedback_; }

 private:
  NumberOperationHint hint_;
  FeedbackSource feedback_;
};

size_t hash_value(NumberOperationParameters const&);
V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           const NumberOperationParameters&);
bool operator==(NumberOperationParameters const&,
                NumberOperationParameters const&);
const NumberOperationParameters& NumberOperationParametersOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

int FormalParameterCountOf(const Operator* op) V8_WARN_UNUSED_RESULT;
bool IsRestLengthOf(const Operator* op) V8_WARN_UNUSED_RESULT;

class AllocateParameters {
 public:
  AllocateParameters(
      Type type, AllocationType allocation_type,
      AllowLargeObjects allow_large_objects = AllowLargeObjects::kFalse)
      : type_(type),
        allocation_type_(allocation_type),
        allow_large_objects_(allow_large_objects) {}

  Type type() const { return type_; }
  AllocationType allocation_type() const { return allocation_type_; }
  AllowLargeObjects allow_large_objects() const { return allow_large_objects_; }

 private:
  Type type_;
  AllocationType allocation_type_;
  AllowLargeObjects allow_large_objects_;
};

bool IsCheckedWithFeedback(const Operator* op);

size_t hash_value(AllocateParameters);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, AllocateParameters);

bool operator==(AllocateParameters const&, AllocateParameters const&);

const AllocateParameters& AllocateParametersOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

AllocationType AllocationTypeOf(const Operator* op) V8_WARN_UNUSED_RESULT;

Type AllocateTypeOf(const Operator* op) V8_WARN_UNUSED_RESULT;

UnicodeEncoding UnicodeEncodingOf(const Operator*) V8_WARN_UNUSED_RESULT;

AbortReason AbortReasonOf(const Operator* op) V8_WARN_UNUSED_RESULT;

DeoptimizeReason DeoptimizeReasonOf(const Operator* op) V8_WARN_UNUSED_RESULT;

int NewArgumentsElementsMappedCountOf(const Operator* op) V8_WARN_UNUSED_RESULT;

class FastApiCallParameters {
 public:
  explicit FastApiCallParameters(const CFunctionInfo* signature,
                                 FeedbackSource const& feedback)
      : signature_(signature), feedback_(feedback) {}

  const CFunctionInfo* signature() const { return signature_; }
  FeedbackSource const& feedback() const { return feedback_; }

 private:
  const CFunctionInfo* signature_;
  const FeedbackSource feedback_;
};

FastApiCallParameters const& FastApiCallParametersOf(const Operator* op)
    V8_WARN_UNUSED_RESULT;

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           FastApiCallParameters const&);

size_t hash_value(FastApiCallParameters const&);

bool operator==(FastApiCallParameters const&, FastApiCallParameters const&);

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
  const Operator* NumberSameValue();
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

  const Operator* BigIntAdd();
  const Operator* BigIntSubtract();
  const Operator* BigIntNegate();

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

  const Operator* SpeculativeBigIntAdd(BigIntOperationHint hint);
  const Operator* SpeculativeBigIntSubtract(BigIntOperationHint hint);
  const Operator* SpeculativeBigIntNegate(BigIntOperationHint hint);
  const Operator* BigIntAsUintN(int bits);

  const Operator* ReferenceEqual();
  const Operator* SameValue();
  const Operator* SameValueNumbersOnly();

  const Operator* TypeOf();

  const Operator* ToBoolean();

  const Operator* StringConcat();
  const Operator* StringEqual();
  const Operator* StringLessThan();
  const Operator* StringLessThanOrEqual();
  const Operator* StringCharCodeAt();
  const Operator* StringCodePointAt();
  const Operator* StringFromSingleCharCode();
  const Operator* StringFromSingleCodePoint();
  const Operator* StringFromCodePointAt();
  const Operator* StringIndexOf();
  const Operator* StringLength();
  const Operator* StringToLowerCaseIntl();
  const Operator* StringToUpperCaseIntl();
  const Operator* StringSubstring();

  const Operator* FindOrderedHashMapEntry();
  const Operator* FindOrderedHashMapEntryForInt32Key();

  const Operator* SpeculativeToNumber(NumberOperationHint hint,
                                      const FeedbackSource& feedback);

  const Operator* StringToNumber();
  const Operator* PlainPrimitiveToNumber();
  const Operator* PlainPrimitiveToWord32();
  const Operator* PlainPrimitiveToFloat64();

  const Operator* ChangeTaggedSignedToInt32();
  const Operator* ChangeTaggedSignedToInt64();
  const Operator* ChangeTaggedToInt32();
  const Operator* ChangeTaggedToInt64();
  const Operator* ChangeTaggedToUint32();
  const Operator* ChangeTaggedToFloat64();
  const Operator* ChangeTaggedToTaggedSigned();
  const Operator* ChangeInt31ToTaggedSigned();
  const Operator* ChangeInt32ToTagged();
  const Operator* ChangeInt64ToTagged();
  const Operator* ChangeUint32ToTagged();
  const Operator* ChangeUint64ToTagged();
  const Operator* ChangeFloat64ToTagged(CheckForMinusZeroMode);
  const Operator* ChangeFloat64ToTaggedPointer();
  const Operator* ChangeTaggedToBit();
  const Operator* ChangeBitToTagged();
  const Operator* TruncateBigIntToUint64();
  const Operator* ChangeUint64ToBigInt();
  const Operator* TruncateTaggedToWord32();
  const Operator* TruncateTaggedToFloat64();
  const Operator* TruncateTaggedToBit();
  const Operator* TruncateTaggedPointerToBit();

  const Operator* PoisonIndex();
  const Operator* CompareMaps(ZoneHandleSet<Map>);
  const Operator* MapGuard(ZoneHandleSet<Map> maps);

  const Operator* CheckBounds(const FeedbackSource& feedback,
                              CheckBoundsFlags flags = {});
  const Operator* CheckedUint32Bounds(const FeedbackSource& feedback,
                                      CheckBoundsFlags flags);
  const Operator* CheckedUint64Bounds(const FeedbackSource& feedback,
                                      CheckBoundsFlags flags);

  const Operator* CheckClosure(const Handle<FeedbackCell>& feedback_cell);
  const Operator* CheckEqualsInternalizedString();
  const Operator* CheckEqualsSymbol();
  const Operator* CheckFloat64Hole(CheckFloat64HoleMode, FeedbackSource const&);
  const Operator* CheckHeapObject();
  const Operator* CheckIf(DeoptimizeReason deoptimize_reason,
                          const FeedbackSource& feedback = FeedbackSource());
  const Operator* CheckInternalizedString();
  const Operator* CheckMaps(CheckMapsFlags, ZoneHandleSet<Map>,
                            const FeedbackSource& = FeedbackSource());
  const Operator* CheckNotTaggedHole();
  const Operator* CheckNumber(const FeedbackSource& feedback);
  const Operator* CheckReceiver();
  const Operator* CheckReceiverOrNullOrUndefined();
  const Operator* CheckSmi(const FeedbackSource& feedback);
  const Operator* CheckString(const FeedbackSource& feedback);
  const Operator* CheckSymbol();

  const Operator* CheckedFloat64ToInt32(CheckForMinusZeroMode,
                                        const FeedbackSource& feedback);
  const Operator* CheckedFloat64ToInt64(CheckForMinusZeroMode,
                                        const FeedbackSource& feedback);
  const Operator* CheckedInt32Add();
  const Operator* CheckedInt32Div();
  const Operator* CheckedInt32Mod();
  const Operator* CheckedInt32Mul(CheckForMinusZeroMode);
  const Operator* CheckedInt32Sub();
  const Operator* CheckedInt32ToTaggedSigned(const FeedbackSource& feedback);
  const Operator* CheckedInt64ToInt32(const FeedbackSource& feedback);
  const Operator* CheckedInt64ToTaggedSigned(const FeedbackSource& feedback);
  const Operator* CheckedTaggedSignedToInt32(const FeedbackSource& feedback);
  const Operator* CheckedTaggedToFloat64(CheckTaggedInputMode,
                                         const FeedbackSource& feedback);
  const Operator* CheckedTaggedToInt32(CheckForMinusZeroMode,
                                       const FeedbackSource& feedback);
  const Operator* CheckedTaggedToArrayIndex(const FeedbackSource& feedback);
  const Operator* CheckedTaggedToInt64(CheckForMinusZeroMode,
                                       const FeedbackSource& feedback);
  const Operator* CheckedTaggedToTaggedPointer(const FeedbackSource& feedback);
  const Operator* CheckedTaggedToTaggedSigned(const FeedbackSource& feedback);
  const Operator* CheckBigInt(const FeedbackSource& feedback);
  const Operator* CheckedTruncateTaggedToWord32(CheckTaggedInputMode,
                                                const FeedbackSource& feedback);
  const Operator* CheckedUint32Div();
  const Operator* CheckedUint32Mod();
  const Operator* CheckedUint32ToInt32(const FeedbackSource& feedback);
  const Operator* CheckedUint32ToTaggedSigned(const FeedbackSource& feedback);
  const Operator* CheckedUint64ToInt32(const FeedbackSource& feedback);
  const Operator* CheckedUint64ToTaggedSigned(const FeedbackSource& feedback);

  const Operator* ConvertReceiver(ConvertReceiverMode);

  const Operator* ConvertTaggedHoleToUndefined();

  const Operator* ObjectIsArrayBufferView();
  const Operator* ObjectIsBigInt();
  const Operator* ObjectIsCallable();
  const Operator* ObjectIsConstructor();
  const Operator* ObjectIsDetectableCallable();
  const Operator* ObjectIsMinusZero();
  const Operator* NumberIsMinusZero();
  const Operator* ObjectIsNaN();
  const Operator* NumberIsNaN();
  const Operator* ObjectIsNonCallable();
  const Operator* ObjectIsNumber();
  const Operator* ObjectIsReceiver();
  const Operator* ObjectIsSmi();
  const Operator* ObjectIsString();
  const Operator* ObjectIsSymbol();
  const Operator* ObjectIsUndetectable();

  const Operator* NumberIsFloat64Hole();
  const Operator* NumberIsFinite();
  const Operator* ObjectIsFiniteNumber();
  const Operator* NumberIsInteger();
  const Operator* ObjectIsSafeInteger();
  const Operator* NumberIsSafeInteger();
  const Operator* ObjectIsInteger();

  const Operator* ArgumentsFrame();
  const Operator* ArgumentsLength(int formal_parameter_count,
                                  bool is_rest_length);

  const Operator* NewDoubleElements(AllocationType);
  const Operator* NewSmiOrObjectElements(AllocationType);

  // new-arguments-elements arguments-frame, arguments-length
  const Operator* NewArgumentsElements(int mapped_count);

  // new-cons-string length, first, second
  const Operator* NewConsString();

  // ensure-writable-fast-elements object, elements
  const Operator* EnsureWritableFastElements();

  // maybe-grow-fast-elements object, elements, index, length
  const Operator* MaybeGrowFastElements(GrowFastElementsMode mode,
                                        const FeedbackSource& feedback);

  // transition-elements-kind object, from-map, to-map
  const Operator* TransitionElementsKind(ElementsTransition transition);

  const Operator* Allocate(Type type,
                           AllocationType allocation = AllocationType::kYoung);
  const Operator* AllocateRaw(
      Type type, AllocationType allocation = AllocationType::kYoung,
      AllowLargeObjects allow_large_objects = AllowLargeObjects::kFalse);

  const Operator* LoadMessage();
  const Operator* StoreMessage();

  const Operator* LoadFieldByIndex();
  const Operator* LoadField(FieldAccess const&);
  const Operator* StoreField(FieldAccess const&);

  // load-element [base + index]
  const Operator* LoadElement(ElementAccess const&);

  // load-stack-argument [base + index]
  const Operator* LoadStackArgument();

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
                                                     Type value_type);

  // load-from-object [base + offset]
  const Operator* LoadFromObject(ObjectAccess const&);

  // store-to-object [base + offset], value
  const Operator* StoreToObject(ObjectAccess const&);

  // load-typed-element buffer, [base + external + index]
  const Operator* LoadTypedElement(ExternalArrayType const&);

  // load-data-view-element object, [base + index]
  const Operator* LoadDataViewElement(ExternalArrayType const&);

  // store-typed-element buffer, [base + external + index], value
  const Operator* StoreTypedElement(ExternalArrayType const&);

  // store-data-view-element object, [base + index], value
  const Operator* StoreDataViewElement(ExternalArrayType const&);

  // Abort (for terminating execution on internal error).
  const Operator* RuntimeAbort(AbortReason reason);

  // Abort if the value input does not inhabit the given type
  const Operator* AssertType(Type type);

  const Operator* DateNow();

  // Stores the signature and feedback of a fast C call
  const Operator* FastApiCall(const CFunctionInfo* signature,
                              FeedbackSource const& feedback);

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
