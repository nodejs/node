// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_OPERATOR_H_
#define V8_COMPILER_SIMPLIFIED_OPERATOR_H_

#include <iosfwd>

#include "src/compiler/operator.h"
#include "src/compiler/types.h"
#include "src/handles.h"
#include "src/machine-type.h"
#include "src/objects.h"

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


// An access descriptor for loads/stores of array buffers.
class BufferAccess final {
 public:
  explicit BufferAccess(ExternalArrayType external_array_type)
      : external_array_type_(external_array_type) {}

  ExternalArrayType external_array_type() const { return external_array_type_; }
  MachineType machine_type() const;

 private:
  ExternalArrayType const external_array_type_;
};

bool operator==(BufferAccess, BufferAccess);
bool operator!=(BufferAccess, BufferAccess);

size_t hash_value(BufferAccess);

std::ostream& operator<<(std::ostream&, BufferAccess);

BufferAccess const BufferAccessOf(const Operator* op) WARN_UNUSED_RESULT;


// An access descriptor for loads/stores of fixed structures like field
// accesses of heap objects. Accesses from either tagged or untagged base
// pointers are supported; untagging is done automatically during lowering.
struct FieldAccess {
  BaseTaggedness base_is_tagged;  // specifies if the base pointer is tagged.
  int offset;                     // offset of the field, without tag.
  MaybeHandle<Name> name;         // debugging only.
  Type* type;                     // type of the field.
  MachineType machine_type;       // machine type of the field.
  WriteBarrierKind write_barrier_kind;  // write barrier hint.

  int tag() const { return base_is_tagged == kTaggedBase ? kHeapObjectTag : 0; }
};

bool operator==(FieldAccess const&, FieldAccess const&);
bool operator!=(FieldAccess const&, FieldAccess const&);

size_t hash_value(FieldAccess const&);

std::ostream& operator<<(std::ostream&, FieldAccess const&);

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

bool operator==(ElementAccess const&, ElementAccess const&);
bool operator!=(ElementAccess const&, ElementAccess const&);

size_t hash_value(ElementAccess const&);

std::ostream& operator<<(std::ostream&, ElementAccess const&);

ElementAccess const& ElementAccessOf(const Operator* op) WARN_UNUSED_RESULT;

ExternalArrayType ExternalArrayTypeOf(const Operator* op) WARN_UNUSED_RESULT;

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

CheckTaggedInputMode CheckTaggedInputModeOf(const Operator*) WARN_UNUSED_RESULT;

enum class CheckForMinusZeroMode : uint8_t {
  kCheckForMinusZero,
  kDontCheckForMinusZero,
};

size_t hash_value(CheckForMinusZeroMode);

std::ostream& operator<<(std::ostream&, CheckForMinusZeroMode);

CheckForMinusZeroMode CheckMinusZeroModeOf(const Operator*) WARN_UNUSED_RESULT;

// A descriptor for growing elements backing stores.
enum class GrowFastElementsFlag : uint8_t {
  kNone = 0u,
  kArrayObject = 1u << 0,     // Update JSArray::length field.
  kHoleyElements = 1u << 1,   // Backing store is holey.
  kDoubleElements = 1u << 2,  // Backing store contains doubles.
};
typedef base::Flags<GrowFastElementsFlag> GrowFastElementsFlags;

DEFINE_OPERATORS_FOR_FLAGS(GrowFastElementsFlags)

std::ostream& operator<<(std::ostream&, GrowFastElementsFlags);

GrowFastElementsFlags GrowFastElementsFlagsOf(const Operator*)
    WARN_UNUSED_RESULT;

// A descriptor for elements kind transitions.
enum class ElementsTransition : uint8_t {
  kFastTransition,  // simple transition, just updating the map.
  kSlowTransition   // full transition, round-trip to the runtime.
};

size_t hash_value(ElementsTransition);

std::ostream& operator<<(std::ostream&, ElementsTransition);

ElementsTransition ElementsTransitionOf(const Operator* op) WARN_UNUSED_RESULT;

// A hint for speculative number operations.
enum class NumberOperationHint : uint8_t {
  kSignedSmall,      // Inputs were always Smi so far, output was in Smi range.
  kSigned32,         // Inputs and output were Signed32 so far.
  kNumber,           // Inputs were Number, output was Number.
  kNumberOrOddball,  // Inputs were Number or Oddball, output was Number.
};

size_t hash_value(NumberOperationHint);

std::ostream& operator<<(std::ostream&, NumberOperationHint);

NumberOperationHint NumberOperationHintOf(const Operator* op)
    WARN_UNUSED_RESULT;

PretenureFlag PretenureFlagOf(const Operator* op) WARN_UNUSED_RESULT;

UnicodeEncoding UnicodeEncodingOf(const Operator*) WARN_UNUSED_RESULT;

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
class SimplifiedOperatorBuilder final : public ZoneObject {
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
  const Operator* NumberToUint32();

  const Operator* NumberSilenceNaN();

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

  const Operator* StringEqual();
  const Operator* StringLessThan();
  const Operator* StringLessThanOrEqual();
  const Operator* StringCharCodeAt();
  const Operator* StringFromCharCode();
  const Operator* StringFromCodePoint(UnicodeEncoding encoding);

  const Operator* PlainPrimitiveToNumber();
  const Operator* PlainPrimitiveToWord32();
  const Operator* PlainPrimitiveToFloat64();

  const Operator* ChangeTaggedSignedToInt32();
  const Operator* ChangeTaggedToInt32();
  const Operator* ChangeTaggedToUint32();
  const Operator* ChangeTaggedToFloat64();
  const Operator* ChangeInt31ToTaggedSigned();
  const Operator* ChangeInt32ToTagged();
  const Operator* ChangeUint32ToTagged();
  const Operator* ChangeFloat64ToTagged();
  const Operator* ChangeTaggedToBit();
  const Operator* ChangeBitToTagged();
  const Operator* TruncateTaggedToWord32();
  const Operator* TruncateTaggedToFloat64();
  const Operator* TruncateTaggedToBit();

  const Operator* CheckIf();
  const Operator* CheckBounds();
  const Operator* CheckMaps(int map_input_count);

  const Operator* CheckHeapObject();
  const Operator* CheckNumber();
  const Operator* CheckSmi();
  const Operator* CheckString();

  const Operator* CheckedInt32Add();
  const Operator* CheckedInt32Sub();
  const Operator* CheckedInt32Div();
  const Operator* CheckedInt32Mod();
  const Operator* CheckedUint32Div();
  const Operator* CheckedUint32Mod();
  const Operator* CheckedInt32Mul(CheckForMinusZeroMode);
  const Operator* CheckedInt32ToTaggedSigned();
  const Operator* CheckedUint32ToInt32();
  const Operator* CheckedUint32ToTaggedSigned();
  const Operator* CheckedFloat64ToInt32(CheckForMinusZeroMode);
  const Operator* CheckedTaggedSignedToInt32();
  const Operator* CheckedTaggedToInt32(CheckForMinusZeroMode);
  const Operator* CheckedTaggedToFloat64(CheckTaggedInputMode);
  const Operator* CheckedTaggedToTaggedSigned();
  const Operator* CheckedTruncateTaggedToWord32();

  const Operator* CheckFloat64Hole(CheckFloat64HoleMode);
  const Operator* CheckTaggedHole();
  const Operator* ConvertTaggedHoleToUndefined();

  const Operator* ObjectIsCallable();
  const Operator* ObjectIsNumber();
  const Operator* ObjectIsReceiver();
  const Operator* ObjectIsSmi();
  const Operator* ObjectIsString();
  const Operator* ObjectIsUndetectable();

  // array-buffer-was-neutered buffer
  const Operator* ArrayBufferWasNeutered();

  // ensure-writable-fast-elements object, elements
  const Operator* EnsureWritableFastElements();

  // maybe-grow-fast-elements object, elements, index, length
  const Operator* MaybeGrowFastElements(GrowFastElementsFlags flags);

  // transition-elements-kind object, from-map, to-map
  const Operator* TransitionElementsKind(ElementsTransition transition);

  const Operator* Allocate(PretenureFlag pretenure = NOT_TENURED);

  const Operator* LoadField(FieldAccess const&);
  const Operator* StoreField(FieldAccess const&);

  // load-buffer buffer, offset, length
  const Operator* LoadBuffer(BufferAccess);

  // store-buffer buffer, offset, length, value
  const Operator* StoreBuffer(BufferAccess);

  // load-element [base + index]
  const Operator* LoadElement(ElementAccess const&);

  // store-element [base + index], value
  const Operator* StoreElement(ElementAccess const&);

  // load-typed-element buffer, [base + external + index]
  const Operator* LoadTypedElement(ExternalArrayType const&);

  // store-typed-element buffer, [base + external + index], value
  const Operator* StoreTypedElement(ExternalArrayType const&);

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
