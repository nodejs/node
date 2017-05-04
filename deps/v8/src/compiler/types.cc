// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>

#include "src/compiler/types.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

// NOTE: If code is marked as being a "shortcut", this means that removing
// the code won't affect the semantics of the surrounding function definition.

// static
bool Type::IsInteger(i::Object* x) {
  return x->IsNumber() && Type::IsInteger(x->Number());
}

// -----------------------------------------------------------------------------
// Range-related helper functions.

bool RangeType::Limits::IsEmpty() { return this->min > this->max; }

RangeType::Limits RangeType::Limits::Intersect(Limits lhs, Limits rhs) {
  DisallowHeapAllocation no_allocation;
  Limits result(lhs);
  if (lhs.min < rhs.min) result.min = rhs.min;
  if (lhs.max > rhs.max) result.max = rhs.max;
  return result;
}

RangeType::Limits RangeType::Limits::Union(Limits lhs, Limits rhs) {
  DisallowHeapAllocation no_allocation;
  if (lhs.IsEmpty()) return rhs;
  if (rhs.IsEmpty()) return lhs;
  Limits result(lhs);
  if (lhs.min > rhs.min) result.min = rhs.min;
  if (lhs.max < rhs.max) result.max = rhs.max;
  return result;
}

bool Type::Overlap(RangeType* lhs, RangeType* rhs) {
  DisallowHeapAllocation no_allocation;
  return !RangeType::Limits::Intersect(RangeType::Limits(lhs),
                                       RangeType::Limits(rhs))
              .IsEmpty();
}

bool Type::Contains(RangeType* lhs, RangeType* rhs) {
  DisallowHeapAllocation no_allocation;
  return lhs->Min() <= rhs->Min() && rhs->Max() <= lhs->Max();
}

bool Type::Contains(RangeType* range, i::Object* val) {
  DisallowHeapAllocation no_allocation;
  return IsInteger(val) && range->Min() <= val->Number() &&
         val->Number() <= range->Max();
}

// -----------------------------------------------------------------------------
// Min and Max computation.

double Type::Min() {
  DCHECK(this->Is(Number()));
  if (this->IsBitset()) return BitsetType::Min(this->AsBitset());
  if (this->IsUnion()) {
    double min = +V8_INFINITY;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      min = std::min(min, this->AsUnion()->Get(i)->Min());
    }
    return min;
  }
  if (this->IsRange()) return this->AsRange()->Min();
  if (this->IsOtherNumberConstant())
    return this->AsOtherNumberConstant()->Value();
  UNREACHABLE();
  return 0;
}

double Type::Max() {
  DCHECK(this->Is(Number()));
  if (this->IsBitset()) return BitsetType::Max(this->AsBitset());
  if (this->IsUnion()) {
    double max = -V8_INFINITY;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      max = std::max(max, this->AsUnion()->Get(i)->Max());
    }
    return max;
  }
  if (this->IsRange()) return this->AsRange()->Max();
  if (this->IsOtherNumberConstant())
    return this->AsOtherNumberConstant()->Value();
  UNREACHABLE();
  return 0;
}

// -----------------------------------------------------------------------------
// Glb and lub computation.

// The largest bitset subsumed by this type.
Type::bitset BitsetType::Glb(Type* type) {
  DisallowHeapAllocation no_allocation;
  // Fast case.
  if (IsBitset(type)) {
    return type->AsBitset();
  } else if (type->IsUnion()) {
    SLOW_DCHECK(type->AsUnion()->Wellformed());
    return type->AsUnion()->Get(0)->BitsetGlb() |
           type->AsUnion()->Get(1)->BitsetGlb();  // Shortcut.
  } else if (type->IsRange()) {
    bitset glb =
        BitsetType::Glb(type->AsRange()->Min(), type->AsRange()->Max());
    return glb;
  } else {
    return kNone;
  }
}

// The smallest bitset subsuming this type, possibly not a proper one.
Type::bitset BitsetType::Lub(Type* type) {
  DisallowHeapAllocation no_allocation;
  if (IsBitset(type)) return type->AsBitset();
  if (type->IsUnion()) {
    // Take the representation from the first element, which is always
    // a bitset.
    int bitset = type->AsUnion()->Get(0)->BitsetLub();
    for (int i = 0, n = type->AsUnion()->Length(); i < n; ++i) {
      // Other elements only contribute their semantic part.
      bitset |= type->AsUnion()->Get(i)->BitsetLub();
    }
    return bitset;
  }
  if (type->IsHeapConstant()) return type->AsHeapConstant()->Lub();
  if (type->IsOtherNumberConstant())
    return type->AsOtherNumberConstant()->Lub();
  if (type->IsRange()) return type->AsRange()->Lub();
  if (type->IsTuple()) return kOtherInternal;
  UNREACHABLE();
  return kNone;
}

Type::bitset BitsetType::Lub(i::Map* map) {
  DisallowHeapAllocation no_allocation;
  switch (map->instance_type()) {
    case STRING_TYPE:
    case ONE_BYTE_STRING_TYPE:
    case CONS_STRING_TYPE:
    case CONS_ONE_BYTE_STRING_TYPE:
    case THIN_STRING_TYPE:
    case THIN_ONE_BYTE_STRING_TYPE:
    case SLICED_STRING_TYPE:
    case SLICED_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_STRING_TYPE:
    case SHORT_EXTERNAL_ONE_BYTE_STRING_TYPE:
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return kOtherString;
    case INTERNALIZED_STRING_TYPE:
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return kInternalizedString;
    case SYMBOL_TYPE:
      return kSymbol;
    case ODDBALL_TYPE: {
      Heap* heap = map->GetHeap();
      if (map == heap->undefined_map()) return kUndefined;
      if (map == heap->null_map()) return kNull;
      if (map == heap->boolean_map()) return kBoolean;
      if (map == heap->the_hole_map()) return kHole;
      DCHECK(map == heap->uninitialized_map() ||
             map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map() ||
             map == heap->optimized_out_map() ||
             map == heap->stale_register_map());
      return kOtherInternal;
    }
    case HEAP_NUMBER_TYPE:
      return kNumber;
    case JS_OBJECT_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_ERROR_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_API_OBJECT_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
      if (map->is_undetectable()) {
        // Currently we assume that every undetectable receiver is also
        // callable, which is what we need to support document.all.  We
        // could add another Type bit to support other use cases in the
        // future if necessary.
        DCHECK(map->is_callable());
        return kOtherUndetectable;
      }
      if (map->is_callable()) {
        return kOtherCallable;
      }
      return kOtherObject;
    case JS_ARRAY_TYPE:
      return kArray;
    case JS_VALUE_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_DATE_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_NAMESPACE_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_REGEXP_TYPE:  // TODO(rossberg): there should be a RegExp type.
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_STRING_ITERATOR_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:

    case JS_TYPED_ARRAY_KEY_ITERATOR_TYPE:
    case JS_FAST_ARRAY_KEY_ITERATOR_TYPE:
    case JS_GENERIC_ARRAY_KEY_ITERATOR_TYPE:
    case JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_GENERIC_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_INT8_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_INT16_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_INT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE:

    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
    case JS_PROMISE_CAPABILITY_TYPE:
    case JS_PROMISE_TYPE:
      DCHECK(!map->is_callable());
      DCHECK(!map->is_undetectable());
      return kOtherObject;
    case JS_BOUND_FUNCTION_TYPE:
      DCHECK(!map->is_undetectable());
      return kBoundFunction;
    case JS_FUNCTION_TYPE:
      DCHECK(!map->is_undetectable());
      return kFunction;
    case JS_PROXY_TYPE:
      DCHECK(!map->is_undetectable());
      if (map->is_callable()) return kCallableProxy;
      return kOtherProxy;
    case MAP_TYPE:
    case ALLOCATION_SITE_TYPE:
    case ACCESSOR_INFO_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case FUNCTION_TEMPLATE_INFO_TYPE:
    case ACCESSOR_PAIR_TYPE:
    case FIXED_ARRAY_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case TRANSITION_ARRAY_TYPE:
    case FOREIGN_TYPE:
    case SCRIPT_TYPE:
    case CODE_TYPE:
    case PROPERTY_CELL_TYPE:
    case MODULE_TYPE:
    case MODULE_INFO_ENTRY_TYPE:
    case CELL_TYPE:
      return kOtherInternal;

    // Remaining instance types are unsupported for now. If any of them do
    // require bit set types, they should get kOtherInternal.
    case MUTABLE_HEAP_NUMBER_TYPE:
    case FREE_SPACE_TYPE:
#define FIXED_TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:

      TYPED_ARRAYS(FIXED_TYPED_ARRAY_CASE)
#undef FIXED_TYPED_ARRAY_CASE
    case FILLER_TYPE:
    case ACCESS_CHECK_INFO_TYPE:
    case INTERCEPTOR_INFO_TYPE:
    case OBJECT_TEMPLATE_INFO_TYPE:
    case ALLOCATION_MEMENTO_TYPE:
    case ALIASED_ARGUMENTS_ENTRY_TYPE:
    case PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE:
    case PROMISE_REACTION_JOB_INFO_TYPE:
    case DEBUG_INFO_TYPE:
    case STACK_FRAME_INFO_TYPE:
    case WEAK_CELL_TYPE:
    case PROTOTYPE_INFO_TYPE:
    case TUPLE2_TYPE:
    case TUPLE3_TYPE:
    case CONTEXT_EXTENSION_TYPE:
    case ASYNC_GENERATOR_REQUEST_TYPE:
    case PADDING_TYPE_1:
    case PADDING_TYPE_2:
    case PADDING_TYPE_3:
    case PADDING_TYPE_4:
      UNREACHABLE();
      return kNone;
  }
  UNREACHABLE();
  return kNone;
}

Type::bitset BitsetType::Lub(i::Object* value) {
  DisallowHeapAllocation no_allocation;
  if (value->IsNumber()) {
    return Lub(value->Number());
  }
  return Lub(i::HeapObject::cast(value)->map());
}

Type::bitset BitsetType::Lub(double value) {
  DisallowHeapAllocation no_allocation;
  if (i::IsMinusZero(value)) return kMinusZero;
  if (std::isnan(value)) return kNaN;
  if (IsUint32Double(value) || IsInt32Double(value)) return Lub(value, value);
  return kOtherNumber;
}

// Minimum values of plain numeric bitsets.
const BitsetType::Boundary BitsetType::BoundariesArray[] = {
    {kOtherNumber, kPlainNumber, -V8_INFINITY},
    {kOtherSigned32, kNegative32, kMinInt},
    {kNegative31, kNegative31, -0x40000000},
    {kUnsigned30, kUnsigned30, 0},
    {kOtherUnsigned31, kUnsigned31, 0x40000000},
    {kOtherUnsigned32, kUnsigned32, 0x80000000},
    {kOtherNumber, kPlainNumber, static_cast<double>(kMaxUInt32) + 1}};

const BitsetType::Boundary* BitsetType::Boundaries() { return BoundariesArray; }

size_t BitsetType::BoundariesSize() {
  // Windows doesn't like arraysize here.
  // return arraysize(BoundariesArray);
  return 7;
}

Type::bitset BitsetType::ExpandInternals(Type::bitset bits) {
  DisallowHeapAllocation no_allocation;
  if (!(bits & kPlainNumber)) return bits;  // Shortcut.
  const Boundary* boundaries = Boundaries();
  for (size_t i = 0; i < BoundariesSize(); ++i) {
    DCHECK(BitsetType::Is(boundaries[i].internal, boundaries[i].external));
    if (bits & boundaries[i].internal) bits |= boundaries[i].external;
  }
  return bits;
}

Type::bitset BitsetType::Lub(double min, double max) {
  DisallowHeapAllocation no_allocation;
  int lub = kNone;
  const Boundary* mins = Boundaries();

  for (size_t i = 1; i < BoundariesSize(); ++i) {
    if (min < mins[i].min) {
      lub |= mins[i - 1].internal;
      if (max < mins[i].min) return lub;
    }
  }
  return lub | mins[BoundariesSize() - 1].internal;
}

Type::bitset BitsetType::NumberBits(bitset bits) { return bits & kPlainNumber; }

Type::bitset BitsetType::Glb(double min, double max) {
  DisallowHeapAllocation no_allocation;
  int glb = kNone;
  const Boundary* mins = Boundaries();

  // If the range does not touch 0, the bound is empty.
  if (max < -1 || min > 0) return glb;

  for (size_t i = 1; i + 1 < BoundariesSize(); ++i) {
    if (min <= mins[i].min) {
      if (max + 1 < mins[i + 1].min) break;
      glb |= mins[i].external;
    }
  }
  // OtherNumber also contains float numbers, so it can never be
  // in the greatest lower bound.
  return glb & ~(kOtherNumber);
}

double BitsetType::Min(bitset bits) {
  DisallowHeapAllocation no_allocation;
  DCHECK(Is(bits, kNumber));
  const Boundary* mins = Boundaries();
  bool mz = bits & kMinusZero;
  for (size_t i = 0; i < BoundariesSize(); ++i) {
    if (Is(mins[i].internal, bits)) {
      return mz ? std::min(0.0, mins[i].min) : mins[i].min;
    }
  }
  if (mz) return 0;
  return std::numeric_limits<double>::quiet_NaN();
}

double BitsetType::Max(bitset bits) {
  DisallowHeapAllocation no_allocation;
  DCHECK(Is(bits, kNumber));
  const Boundary* mins = Boundaries();
  bool mz = bits & kMinusZero;
  if (BitsetType::Is(mins[BoundariesSize() - 1].internal, bits)) {
    return +V8_INFINITY;
  }
  for (size_t i = BoundariesSize() - 1; i-- > 0;) {
    if (Is(mins[i].internal, bits)) {
      return mz ? std::max(0.0, mins[i + 1].min - 1) : mins[i + 1].min - 1;
    }
  }
  if (mz) return 0;
  return std::numeric_limits<double>::quiet_NaN();
}

// static
bool OtherNumberConstantType::IsOtherNumberConstant(double value) {
  // Not an integer, not NaN, and not -0.
  return !std::isnan(value) && !Type::IsInteger(value) &&
         !i::IsMinusZero(value);
}

// static
bool OtherNumberConstantType::IsOtherNumberConstant(Object* value) {
  return value->IsHeapNumber() &&
         IsOtherNumberConstant(HeapNumber::cast(value)->value());
}

HeapConstantType::HeapConstantType(BitsetType::bitset bitset,
                                   i::Handle<i::HeapObject> object)
    : TypeBase(kHeapConstant), bitset_(bitset), object_(object) {
  DCHECK(!object->IsHeapNumber());
  DCHECK_IMPLIES(object->IsString(), object->IsInternalizedString());
}

// -----------------------------------------------------------------------------
// Predicates.

bool Type::SimplyEquals(Type* that) {
  DisallowHeapAllocation no_allocation;
  if (this->IsHeapConstant()) {
    return that->IsHeapConstant() &&
           this->AsHeapConstant()->Value().address() ==
               that->AsHeapConstant()->Value().address();
  }
  if (this->IsOtherNumberConstant()) {
    return that->IsOtherNumberConstant() &&
           this->AsOtherNumberConstant()->Value() ==
               that->AsOtherNumberConstant()->Value();
  }
  if (this->IsRange()) {
    if (that->IsHeapConstant() || that->IsOtherNumberConstant()) return false;
  }
  if (this->IsTuple()) {
    if (!that->IsTuple()) return false;
    TupleType* this_tuple = this->AsTuple();
    TupleType* that_tuple = that->AsTuple();
    if (this_tuple->Arity() != that_tuple->Arity()) {
      return false;
    }
    for (int i = 0, n = this_tuple->Arity(); i < n; ++i) {
      if (!this_tuple->Element(i)->Equals(that_tuple->Element(i))) return false;
    }
    return true;
  }
  UNREACHABLE();
  return false;
}

// Check if [this] <= [that].
bool Type::SlowIs(Type* that) {
  DisallowHeapAllocation no_allocation;

  // Fast bitset cases
  if (that->IsBitset()) {
    return BitsetType::Is(this->BitsetLub(), that->AsBitset());
  }

  if (this->IsBitset()) {
    return BitsetType::Is(this->AsBitset(), that->BitsetGlb());
  }

  // (T1 \/ ... \/ Tn) <= T  if  (T1 <= T) /\ ... /\ (Tn <= T)
  if (this->IsUnion()) {
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      if (!this->AsUnion()->Get(i)->Is(that)) return false;
    }
    return true;
  }

  // T <= (T1 \/ ... \/ Tn)  if  (T <= T1) \/ ... \/ (T <= Tn)
  if (that->IsUnion()) {
    for (int i = 0, n = that->AsUnion()->Length(); i < n; ++i) {
      if (this->Is(that->AsUnion()->Get(i))) return true;
      if (i > 1 && this->IsRange()) return false;  // Shortcut.
    }
    return false;
  }

  if (that->IsRange()) {
    return (this->IsRange() && Contains(that->AsRange(), this->AsRange()));
  }
  if (this->IsRange()) return false;

  return this->SimplyEquals(that);
}

// Check if [this] and [that] overlap.
bool Type::Maybe(Type* that) {
  DisallowHeapAllocation no_allocation;

  if (!BitsetType::IsInhabited(this->BitsetLub() & that->BitsetLub()))
    return false;

  // (T1 \/ ... \/ Tn) overlaps T  if  (T1 overlaps T) \/ ... \/ (Tn overlaps T)
  if (this->IsUnion()) {
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      if (this->AsUnion()->Get(i)->Maybe(that)) return true;
    }
    return false;
  }

  // T overlaps (T1 \/ ... \/ Tn)  if  (T overlaps T1) \/ ... \/ (T overlaps Tn)
  if (that->IsUnion()) {
    for (int i = 0, n = that->AsUnion()->Length(); i < n; ++i) {
      if (this->Maybe(that->AsUnion()->Get(i))) return true;
    }
    return false;
  }

  if (this->IsBitset() && that->IsBitset()) return true;

  if (this->IsRange()) {
    if (that->IsRange()) {
      return Overlap(this->AsRange(), that->AsRange());
    }
    if (that->IsBitset()) {
      bitset number_bits = BitsetType::NumberBits(that->AsBitset());
      if (number_bits == BitsetType::kNone) {
        return false;
      }
      double min = std::max(BitsetType::Min(number_bits), this->Min());
      double max = std::min(BitsetType::Max(number_bits), this->Max());
      return min <= max;
    }
  }
  if (that->IsRange()) {
    return that->Maybe(this);  // This case is handled above.
  }

  if (this->IsBitset() || that->IsBitset()) return true;

  return this->SimplyEquals(that);
}

// Return the range in [this], or [NULL].
Type* Type::GetRange() {
  DisallowHeapAllocation no_allocation;
  if (this->IsRange()) return this;
  if (this->IsUnion() && this->AsUnion()->Get(1)->IsRange()) {
    return this->AsUnion()->Get(1);
  }
  return NULL;
}

bool UnionType::Wellformed() {
  DisallowHeapAllocation no_allocation;
  // This checks the invariants of the union representation:
  // 1. There are at least two elements.
  // 2. The first element is a bitset, no other element is a bitset.
  // 3. At most one element is a range, and it must be the second one.
  // 4. No element is itself a union.
  // 5. No element (except the bitset) is a subtype of any other.
  // 6. If there is a range, then the bitset type does not contain
  //    plain number bits.
  DCHECK(this->Length() >= 2);       // (1)
  DCHECK(this->Get(0)->IsBitset());  // (2a)

  for (int i = 0; i < this->Length(); ++i) {
    if (i != 0) DCHECK(!this->Get(i)->IsBitset());  // (2b)
    if (i != 1) DCHECK(!this->Get(i)->IsRange());   // (3)
    DCHECK(!this->Get(i)->IsUnion());               // (4)
    for (int j = 0; j < this->Length(); ++j) {
      if (i != j && i != 0) DCHECK(!this->Get(i)->Is(this->Get(j)));  // (5)
    }
  }
  DCHECK(!this->Get(1)->IsRange() ||
         (BitsetType::NumberBits(this->Get(0)->AsBitset()) ==
          BitsetType::kNone));  // (6)
  return true;
}

// -----------------------------------------------------------------------------
// Union and intersection

static bool AddIsSafe(int x, int y) {
  return x >= 0 ? y <= std::numeric_limits<int>::max() - x
                : y >= std::numeric_limits<int>::min() - x;
}

Type* Type::Intersect(Type* type1, Type* type2, Zone* zone) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return BitsetType::New(type1->AsBitset() & type2->AsBitset());
  }

  // Fast case: top or bottom types.
  if (type1->IsNone() || type2->IsAny()) return type1;  // Shortcut.
  if (type2->IsNone() || type1->IsAny()) return type2;  // Shortcut.

  // Semi-fast case.
  if (type1->Is(type2)) return type1;
  if (type2->Is(type1)) return type2;

  // Slow case: create union.

  // Semantic subtyping check - this is needed for consistency with the
  // semi-fast case above.
  if (type1->Is(type2)) {
    type2 = Any();
  } else if (type2->Is(type1)) {
    type1 = Any();
  }

  bitset bits = type1->BitsetGlb() & type2->BitsetGlb();
  int size1 = type1->IsUnion() ? type1->AsUnion()->Length() : 1;
  int size2 = type2->IsUnion() ? type2->AsUnion()->Length() : 1;
  if (!AddIsSafe(size1, size2)) return Any();
  int size = size1 + size2;
  if (!AddIsSafe(size, 2)) return Any();
  size += 2;
  Type* result_type = UnionType::New(size, zone);
  UnionType* result = result_type->AsUnion();
  size = 0;

  // Deal with bitsets.
  result->Set(size++, BitsetType::New(bits));

  RangeType::Limits lims = RangeType::Limits::Empty();
  size = IntersectAux(type1, type2, result, size, &lims, zone);

  // If the range is not empty, then insert it into the union and
  // remove the number bits from the bitset.
  if (!lims.IsEmpty()) {
    size = UpdateRange(RangeType::New(lims, zone), result, size, zone);

    // Remove the number bits.
    bitset number_bits = BitsetType::NumberBits(bits);
    bits &= ~number_bits;
    result->Set(0, BitsetType::New(bits));
  }
  return NormalizeUnion(result_type, size, zone);
}

int Type::UpdateRange(Type* range, UnionType* result, int size, Zone* zone) {
  if (size == 1) {
    result->Set(size++, range);
  } else {
    // Make space for the range.
    result->Set(size++, result->Get(1));
    result->Set(1, range);
  }

  // Remove any components that just got subsumed.
  for (int i = 2; i < size;) {
    if (result->Get(i)->Is(range)) {
      result->Set(i, result->Get(--size));
    } else {
      ++i;
    }
  }
  return size;
}

RangeType::Limits Type::ToLimits(bitset bits, Zone* zone) {
  bitset number_bits = BitsetType::NumberBits(bits);

  if (number_bits == BitsetType::kNone) {
    return RangeType::Limits::Empty();
  }

  return RangeType::Limits(BitsetType::Min(number_bits),
                           BitsetType::Max(number_bits));
}

RangeType::Limits Type::IntersectRangeAndBitset(Type* range, Type* bitset,
                                                Zone* zone) {
  RangeType::Limits range_lims(range->AsRange());
  RangeType::Limits bitset_lims = ToLimits(bitset->AsBitset(), zone);
  return RangeType::Limits::Intersect(range_lims, bitset_lims);
}

int Type::IntersectAux(Type* lhs, Type* rhs, UnionType* result, int size,
                       RangeType::Limits* lims, Zone* zone) {
  if (lhs->IsUnion()) {
    for (int i = 0, n = lhs->AsUnion()->Length(); i < n; ++i) {
      size =
          IntersectAux(lhs->AsUnion()->Get(i), rhs, result, size, lims, zone);
    }
    return size;
  }
  if (rhs->IsUnion()) {
    for (int i = 0, n = rhs->AsUnion()->Length(); i < n; ++i) {
      size =
          IntersectAux(lhs, rhs->AsUnion()->Get(i), result, size, lims, zone);
    }
    return size;
  }

  if (!BitsetType::IsInhabited(lhs->BitsetLub() & rhs->BitsetLub())) {
    return size;
  }

  if (lhs->IsRange()) {
    if (rhs->IsBitset()) {
      RangeType::Limits lim = IntersectRangeAndBitset(lhs, rhs, zone);

      if (!lim.IsEmpty()) {
        *lims = RangeType::Limits::Union(lim, *lims);
      }
      return size;
    }
    if (rhs->IsRange()) {
      RangeType::Limits lim = RangeType::Limits::Intersect(
          RangeType::Limits(lhs->AsRange()), RangeType::Limits(rhs->AsRange()));
      if (!lim.IsEmpty()) {
        *lims = RangeType::Limits::Union(lim, *lims);
      }
    }
    return size;
  }
  if (rhs->IsRange()) {
    // This case is handled symmetrically above.
    return IntersectAux(rhs, lhs, result, size, lims, zone);
  }
  if (lhs->IsBitset() || rhs->IsBitset()) {
    return AddToUnion(lhs->IsBitset() ? rhs : lhs, result, size, zone);
  }
  if (lhs->SimplyEquals(rhs)) {
    return AddToUnion(lhs, result, size, zone);
  }
  return size;
}

// Make sure that we produce a well-formed range and bitset:
// If the range is non-empty, the number bits in the bitset should be
// clear. Moreover, if we have a canonical range (such as Signed32),
// we want to produce a bitset rather than a range.
Type* Type::NormalizeRangeAndBitset(Type* range, bitset* bits, Zone* zone) {
  // Fast path: If the bitset does not mention numbers, we can just keep the
  // range.
  bitset number_bits = BitsetType::NumberBits(*bits);
  if (number_bits == 0) {
    return range;
  }

  // If the range is semantically contained within the bitset, return None and
  // leave the bitset untouched.
  bitset range_lub = range->BitsetLub();
  if (BitsetType::Is(range_lub, *bits)) {
    return None();
  }

  // Slow path: reconcile the bitset range and the range.
  double bitset_min = BitsetType::Min(number_bits);
  double bitset_max = BitsetType::Max(number_bits);

  double range_min = range->Min();
  double range_max = range->Max();

  // Remove the number bits from the bitset, they would just confuse us now.
  // NOTE: bits contains OtherNumber iff bits contains PlainNumber, in which
  // case we already returned after the subtype check above.
  *bits &= ~number_bits;

  if (range_min <= bitset_min && range_max >= bitset_max) {
    // Bitset is contained within the range, just return the range.
    return range;
  }

  if (bitset_min < range_min) {
    range_min = bitset_min;
  }
  if (bitset_max > range_max) {
    range_max = bitset_max;
  }
  return RangeType::New(range_min, range_max, zone);
}

Type* Type::NewConstant(double value, Zone* zone) {
  if (IsInteger(value)) {
    return Range(value, value, zone);
  } else if (i::IsMinusZero(value)) {
    return Type::MinusZero();
  } else if (std::isnan(value)) {
    return Type::NaN();
  }

  DCHECK(OtherNumberConstantType::IsOtherNumberConstant(value));
  return OtherNumberConstant(value, zone);
}

Type* Type::NewConstant(i::Handle<i::Object> value, Zone* zone) {
  if (IsInteger(*value)) {
    double v = value->Number();
    return Range(v, v, zone);
  } else if (value->IsHeapNumber()) {
    return NewConstant(value->Number(), zone);
  } else if (value->IsString() && !value->IsInternalizedString()) {
    return Type::OtherString();
  }
  return HeapConstant(i::Handle<i::HeapObject>::cast(value), zone);
}

Type* Type::Union(Type* type1, Type* type2, Zone* zone) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return BitsetType::New(type1->AsBitset() | type2->AsBitset());
  }

  // Fast case: top or bottom types.
  if (type1->IsAny() || type2->IsNone()) return type1;
  if (type2->IsAny() || type1->IsNone()) return type2;

  // Semi-fast case.
  if (type1->Is(type2)) return type2;
  if (type2->Is(type1)) return type1;

  // Slow case: create union.
  int size1 = type1->IsUnion() ? type1->AsUnion()->Length() : 1;
  int size2 = type2->IsUnion() ? type2->AsUnion()->Length() : 1;
  if (!AddIsSafe(size1, size2)) return Any();
  int size = size1 + size2;
  if (!AddIsSafe(size, 2)) return Any();
  size += 2;
  Type* result_type = UnionType::New(size, zone);
  UnionType* result = result_type->AsUnion();
  size = 0;

  // Compute the new bitset.
  bitset new_bitset = type1->BitsetGlb() | type2->BitsetGlb();

  // Deal with ranges.
  Type* range = None();
  Type* range1 = type1->GetRange();
  Type* range2 = type2->GetRange();
  if (range1 != NULL && range2 != NULL) {
    RangeType::Limits lims =
        RangeType::Limits::Union(RangeType::Limits(range1->AsRange()),
                                 RangeType::Limits(range2->AsRange()));
    Type* union_range = RangeType::New(lims, zone);
    range = NormalizeRangeAndBitset(union_range, &new_bitset, zone);
  } else if (range1 != NULL) {
    range = NormalizeRangeAndBitset(range1, &new_bitset, zone);
  } else if (range2 != NULL) {
    range = NormalizeRangeAndBitset(range2, &new_bitset, zone);
  }
  Type* bits = BitsetType::New(new_bitset);
  result->Set(size++, bits);
  if (!range->IsNone()) result->Set(size++, range);

  size = AddToUnion(type1, result, size, zone);
  size = AddToUnion(type2, result, size, zone);
  return NormalizeUnion(result_type, size, zone);
}

// Add [type] to [result] unless [type] is bitset, range, or already subsumed.
// Return new size of [result].
int Type::AddToUnion(Type* type, UnionType* result, int size, Zone* zone) {
  if (type->IsBitset() || type->IsRange()) return size;
  if (type->IsUnion()) {
    for (int i = 0, n = type->AsUnion()->Length(); i < n; ++i) {
      size = AddToUnion(type->AsUnion()->Get(i), result, size, zone);
    }
    return size;
  }
  for (int i = 0; i < size; ++i) {
    if (type->Is(result->Get(i))) return size;
  }
  result->Set(size++, type);
  return size;
}

Type* Type::NormalizeUnion(Type* union_type, int size, Zone* zone) {
  UnionType* unioned = union_type->AsUnion();
  DCHECK(size >= 1);
  DCHECK(unioned->Get(0)->IsBitset());
  // If the union has just one element, return it.
  if (size == 1) {
    return unioned->Get(0);
  }
  bitset bits = unioned->Get(0)->AsBitset();
  // If the union only consists of a range, we can get rid of the union.
  if (size == 2 && bits == BitsetType::kNone) {
    if (unioned->Get(1)->IsRange()) {
      return RangeType::New(unioned->Get(1)->AsRange()->Min(),
                            unioned->Get(1)->AsRange()->Max(), zone);
    }
  }
  unioned->Shrink(size);
  SLOW_DCHECK(unioned->Wellformed());
  return union_type;
}

int Type::NumConstants() {
  DisallowHeapAllocation no_allocation;
  if (this->IsHeapConstant() || this->IsOtherNumberConstant()) {
    return 1;
  } else if (this->IsUnion()) {
    int result = 0;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      if (this->AsUnion()->Get(i)->IsHeapConstant()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}

// -----------------------------------------------------------------------------
// Printing.

const char* BitsetType::Name(bitset bits) {
  switch (bits) {
#define RETURN_NAMED_TYPE(type, value) \
  case k##type:                        \
    return #type;
    PROPER_BITSET_TYPE_LIST(RETURN_NAMED_TYPE)
    INTERNAL_BITSET_TYPE_LIST(RETURN_NAMED_TYPE)
#undef RETURN_NAMED_TYPE

    default:
      return NULL;
  }
}

void BitsetType::Print(std::ostream& os,  // NOLINT
                       bitset bits) {
  DisallowHeapAllocation no_allocation;
  const char* name = Name(bits);
  if (name != NULL) {
    os << name;
    return;
  }

  // clang-format off
  static const bitset named_bitsets[] = {
#define BITSET_CONSTANT(type, value) k##type,
    INTERNAL_BITSET_TYPE_LIST(BITSET_CONSTANT)
    PROPER_BITSET_TYPE_LIST(BITSET_CONSTANT)
#undef BITSET_CONSTANT
  };
  // clang-format on

  bool is_first = true;
  os << "(";
  for (int i(arraysize(named_bitsets) - 1); bits != 0 && i >= 0; --i) {
    bitset subset = named_bitsets[i];
    if ((bits & subset) == subset) {
      if (!is_first) os << " | ";
      is_first = false;
      os << Name(subset);
      bits -= subset;
    }
  }
  DCHECK(bits == 0);
  os << ")";
}

void Type::PrintTo(std::ostream& os) {
  DisallowHeapAllocation no_allocation;
  if (this->IsBitset()) {
    BitsetType::Print(os, this->AsBitset());
  } else if (this->IsHeapConstant()) {
    os << "HeapConstant(" << Brief(*this->AsHeapConstant()->Value()) << ")";
  } else if (this->IsOtherNumberConstant()) {
    os << "OtherNumberConstant(" << this->AsOtherNumberConstant()->Value()
       << ")";
  } else if (this->IsRange()) {
    std::ostream::fmtflags saved_flags = os.setf(std::ios::fixed);
    std::streamsize saved_precision = os.precision(0);
    os << "Range(" << this->AsRange()->Min() << ", " << this->AsRange()->Max()
       << ")";
    os.flags(saved_flags);
    os.precision(saved_precision);
  } else if (this->IsUnion()) {
    os << "(";
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      Type* type_i = this->AsUnion()->Get(i);
      if (i > 0) os << " | ";
      type_i->PrintTo(os);
    }
    os << ")";
  } else if (this->IsTuple()) {
    os << "<";
    for (int i = 0, n = this->AsTuple()->Arity(); i < n; ++i) {
      Type* type_i = this->AsTuple()->Element(i);
      if (i > 0) os << ", ";
      type_i->PrintTo(os);
    }
    os << ">";
  } else {
    UNREACHABLE();
  }
}

#ifdef DEBUG
void Type::Print() {
  OFStream os(stdout);
  PrintTo(os);
  os << std::endl;
}
void BitsetType::Print(bitset bits) {
  OFStream os(stdout);
  Print(os, bits);
  os << std::endl;
}
#endif

BitsetType::bitset BitsetType::SignedSmall() {
  return i::SmiValuesAre31Bits() ? kSigned31 : kSigned32;
}

BitsetType::bitset BitsetType::UnsignedSmall() {
  return i::SmiValuesAre31Bits() ? kUnsigned30 : kUnsigned31;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
