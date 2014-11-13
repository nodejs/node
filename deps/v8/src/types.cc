// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/types.h"

#include "src/ostreams.h"
#include "src/types-inl.h"

namespace v8 {
namespace internal {


// NOTE: If code is marked as being a "shortcut", this means that removing
// the code won't affect the semantics of the surrounding function definition.


// -----------------------------------------------------------------------------
// Range-related helper functions.

// The result may be invalid (max < min).
template<class Config>
typename TypeImpl<Config>::Limits TypeImpl<Config>::Intersect(
    Limits lhs, Limits rhs) {
  DisallowHeapAllocation no_allocation;
  Limits result(lhs);
  if (lhs.min->Number() < rhs.min->Number()) result.min = rhs.min;
  if (lhs.max->Number() > rhs.max->Number()) result.max = rhs.max;
  return result;
}


template<class Config>
typename TypeImpl<Config>::Limits TypeImpl<Config>::Union(
    Limits lhs, Limits rhs) {
  DisallowHeapAllocation no_allocation;
  Limits result(lhs);
  if (lhs.min->Number() > rhs.min->Number()) result.min = rhs.min;
  if (lhs.max->Number() < rhs.max->Number()) result.max = rhs.max;
  return result;
}


template<class Config>
bool TypeImpl<Config>::Overlap(
    typename TypeImpl<Config>::RangeType* lhs,
    typename TypeImpl<Config>::RangeType* rhs) {
  DisallowHeapAllocation no_allocation;
  typename TypeImpl<Config>::Limits lim = Intersect(Limits(lhs), Limits(rhs));
  return lim.min->Number() <= lim.max->Number();
}


template<class Config>
bool TypeImpl<Config>::Contains(
    typename TypeImpl<Config>::RangeType* lhs,
    typename TypeImpl<Config>::RangeType* rhs) {
  DisallowHeapAllocation no_allocation;
  return lhs->Min()->Number() <= rhs->Min()->Number()
      && rhs->Max()->Number() <= lhs->Max()->Number();
}


template<class Config>
bool TypeImpl<Config>::Contains(
    typename TypeImpl<Config>::RangeType* range, i::Object* val) {
  DisallowHeapAllocation no_allocation;
  return IsInteger(val)
      && range->Min()->Number() <= val->Number()
      && val->Number() <= range->Max()->Number();
}


// -----------------------------------------------------------------------------
// Min and Max computation.

template<class Config>
double TypeImpl<Config>::Min() {
  DCHECK(this->Is(Number()));
  if (this->IsBitset()) return BitsetType::Min(this->AsBitset());
  if (this->IsUnion()) {
    double min = +V8_INFINITY;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      min = std::min(min, this->AsUnion()->Get(i)->Min());
    }
    return min;
  }
  if (this->IsRange()) return this->AsRange()->Min()->Number();
  if (this->IsConstant()) return this->AsConstant()->Value()->Number();
  UNREACHABLE();
  return 0;
}


template<class Config>
double TypeImpl<Config>::Max() {
  DCHECK(this->Is(Number()));
  if (this->IsBitset()) return BitsetType::Max(this->AsBitset());
  if (this->IsUnion()) {
    double max = -V8_INFINITY;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      max = std::max(max, this->AsUnion()->Get(i)->Max());
    }
    return max;
  }
  if (this->IsRange()) return this->AsRange()->Max()->Number();
  if (this->IsConstant()) return this->AsConstant()->Value()->Number();
  UNREACHABLE();
  return 0;
}


// -----------------------------------------------------------------------------
// Glb and lub computation.


// The largest bitset subsumed by this type.
template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Glb(TypeImpl* type) {
  DisallowHeapAllocation no_allocation;
  if (type->IsBitset()) {
    return type->AsBitset();
  } else if (type->IsUnion()) {
    SLOW_DCHECK(type->AsUnion()->Wellformed());
    return type->AsUnion()->Get(0)->BitsetGlb();  // Shortcut.
    // (The remaining BitsetGlb's are None anyway).
  } else {
    return kNone;
  }
}


// The smallest bitset subsuming this type.
template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Lub(TypeImpl* type) {
  DisallowHeapAllocation no_allocation;
  if (type->IsBitset()) return type->AsBitset();
  if (type->IsUnion()) {
    int bitset = kNone;
    for (int i = 0, n = type->AsUnion()->Length(); i < n; ++i) {
      bitset |= type->AsUnion()->Get(i)->BitsetLub();
    }
    return bitset;
  }
  if (type->IsClass()) {
    // Little hack to avoid the need for a region for handlification here...
    return Config::is_class(type) ? Lub(*Config::as_class(type)) :
        type->AsClass()->Bound(NULL)->AsBitset();
  }
  if (type->IsConstant()) return type->AsConstant()->Bound()->AsBitset();
  if (type->IsRange()) return type->AsRange()->BitsetLub();
  if (type->IsContext()) return kInternal & kTaggedPtr;
  if (type->IsArray()) return kArray;
  if (type->IsFunction()) return kFunction;
  UNREACHABLE();
  return kNone;
}


template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Lub(i::Map* map) {
  DisallowHeapAllocation no_allocation;
  switch (map->instance_type()) {
    case STRING_TYPE:
    case ONE_BYTE_STRING_TYPE:
    case CONS_STRING_TYPE:
    case CONS_ONE_BYTE_STRING_TYPE:
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
      DCHECK(map == heap->the_hole_map() ||
             map == heap->uninitialized_map() ||
             map == heap->no_interceptor_result_sentinel_map() ||
             map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map());
      return kInternal & kTaggedPtr;
    }
    case HEAP_NUMBER_TYPE:
      return kNumber & kTaggedPtr;
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      if (map->is_undetectable()) return kUndetectable;
      return kOtherObject;
    case JS_ARRAY_TYPE:
      return kArray;
    case JS_FUNCTION_TYPE:
      return kFunction;
    case JS_REGEXP_TYPE:
      return kRegExp;
    case JS_PROXY_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return kProxy;
    case MAP_TYPE:
      // When compiling stub templates, the meta map is used as a place holder
      // for the actual map with which the template is later instantiated.
      // We treat it as a kind of type variable whose upper bound is Any.
      // TODO(rossberg): for caching of CompareNilIC stubs to work correctly,
      // we must exclude Undetectable here. This makes no sense, really,
      // because it means that the template isn't actually parametric.
      // Also, it doesn't apply elsewhere. 8-(
      // We ought to find a cleaner solution for compiling stubs parameterised
      // over type or class variables, esp ones with bounds...
      return kDetectable;
    case DECLARED_ACCESSOR_INFO_TYPE:
    case EXECUTABLE_ACCESSOR_INFO_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case ACCESSOR_PAIR_TYPE:
    case FIXED_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case FOREIGN_TYPE:
    case CODE_TYPE:
      return kInternal & kTaggedPtr;
    default:
      UNREACHABLE();
      return kNone;
  }
}


template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Lub(i::Object* value) {
  DisallowHeapAllocation no_allocation;
  if (value->IsNumber()) {
    return Lub(value->Number()) & (value->IsSmi() ? kTaggedInt : kTaggedPtr);
  }
  return Lub(i::HeapObject::cast(value)->map());
}


template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Lub(double value) {
  DisallowHeapAllocation no_allocation;
  if (i::IsMinusZero(value)) return kMinusZero;
  if (std::isnan(value)) return kNaN;
  if (IsUint32Double(value) || IsInt32Double(value)) return Lub(value, value);
  return kOtherNumber;
}


// Minimum values of regular numeric bitsets when SmiValuesAre31Bits.
template<class Config>
const typename TypeImpl<Config>::BitsetType::BitsetMin
TypeImpl<Config>::BitsetType::BitsetMins31[] = {
    {kOtherNumber, -V8_INFINITY},
    {kOtherSigned32, kMinInt},
    {kOtherSignedSmall, -0x40000000},
    {kUnsignedSmall, 0},
    {kOtherUnsigned31, 0x40000000},
    {kOtherUnsigned32, 0x80000000},
    {kOtherNumber, static_cast<double>(kMaxUInt32) + 1}
};


// Minimum values of regular numeric bitsets when SmiValuesAre32Bits.
// OtherSigned32 and OtherUnsigned31 are empty (see the diagrams in types.h).
template<class Config>
const typename TypeImpl<Config>::BitsetType::BitsetMin
TypeImpl<Config>::BitsetType::BitsetMins32[] = {
    {kOtherNumber, -V8_INFINITY},
    {kOtherSignedSmall, kMinInt},
    {kUnsignedSmall, 0},
    {kOtherUnsigned32, 0x80000000},
    {kOtherNumber, static_cast<double>(kMaxUInt32) + 1}
};


template<class Config>
typename TypeImpl<Config>::bitset
TypeImpl<Config>::BitsetType::Lub(double min, double max) {
  DisallowHeapAllocation no_allocation;
  int lub = kNone;
  const BitsetMin* mins = BitsetMins();

  for (size_t i = 1; i < BitsetMinsSize(); ++i) {
    if (min < mins[i].min) {
      lub |= mins[i-1].bits;
      if (max < mins[i].min) return lub;
    }
  }
  return lub |= mins[BitsetMinsSize()-1].bits;
}


template<class Config>
double TypeImpl<Config>::BitsetType::Min(bitset bits) {
  DisallowHeapAllocation no_allocation;
  DCHECK(Is(bits, kNumber));
  const BitsetMin* mins = BitsetMins();
  bool mz = SEMANTIC(bits & kMinusZero);
  for (size_t i = 0; i < BitsetMinsSize(); ++i) {
    if (Is(SEMANTIC(mins[i].bits), bits)) {
      return mz ? std::min(0.0, mins[i].min) : mins[i].min;
    }
  }
  if (mz) return 0;
  return base::OS::nan_value();
}


template<class Config>
double TypeImpl<Config>::BitsetType::Max(bitset bits) {
  DisallowHeapAllocation no_allocation;
  DCHECK(Is(bits, kNumber));
  const BitsetMin* mins = BitsetMins();
  bool mz = SEMANTIC(bits & kMinusZero);
  if (BitsetType::Is(mins[BitsetMinsSize()-1].bits, bits)) {
    return +V8_INFINITY;
  }
  for (size_t i = BitsetMinsSize()-1; i-- > 0; ) {
    if (Is(SEMANTIC(mins[i].bits), bits)) {
      return mz ?
          std::max(0.0, mins[i+1].min - 1) : mins[i+1].min - 1;
    }
  }
  if (mz) return 0;
  return base::OS::nan_value();
}


// -----------------------------------------------------------------------------
// Predicates.


template<class Config>
bool TypeImpl<Config>::SimplyEquals(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;
  if (this->IsClass()) {
    return that->IsClass()
        && *this->AsClass()->Map() == *that->AsClass()->Map();
  }
  if (this->IsConstant()) {
    return that->IsConstant()
        && *this->AsConstant()->Value() == *that->AsConstant()->Value();
  }
  if (this->IsContext()) {
    return that->IsContext()
        && this->AsContext()->Outer()->Equals(that->AsContext()->Outer());
  }
  if (this->IsArray()) {
    return that->IsArray()
        && this->AsArray()->Element()->Equals(that->AsArray()->Element());
  }
  if (this->IsFunction()) {
    if (!that->IsFunction()) return false;
    FunctionType* this_fun = this->AsFunction();
    FunctionType* that_fun = that->AsFunction();
    if (this_fun->Arity() != that_fun->Arity() ||
        !this_fun->Result()->Equals(that_fun->Result()) ||
        !this_fun->Receiver()->Equals(that_fun->Receiver())) {
      return false;
    }
    for (int i = 0, n = this_fun->Arity(); i < n; ++i) {
      if (!this_fun->Parameter(i)->Equals(that_fun->Parameter(i))) return false;
    }
    return true;
  }
  UNREACHABLE();
  return false;
}


// Check if [this] <= [that].
template<class Config>
bool TypeImpl<Config>::SlowIs(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;

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
    return (this->IsRange() && Contains(that->AsRange(), this->AsRange()))
        || (this->IsConstant() &&
            Contains(that->AsRange(), *this->AsConstant()->Value()));
  }
  if (this->IsRange()) return false;

  return this->SimplyEquals(that);
}


template<class Config>
bool TypeImpl<Config>::NowIs(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;

  // TODO(rossberg): this is incorrect for
  //   Union(Constant(V), T)->NowIs(Class(M))
  // but fuzzing does not cover that!
  if (this->IsConstant()) {
    i::Object* object = *this->AsConstant()->Value();
    if (object->IsHeapObject()) {
      i::Map* map = i::HeapObject::cast(object)->map();
      for (Iterator<i::Map> it = that->Classes(); !it.Done(); it.Advance()) {
        if (*it.Current() == map) return true;
      }
    }
  }
  return this->Is(that);
}


// Check if [this] contains only (currently) stable classes.
template<class Config>
bool TypeImpl<Config>::NowStable() {
  DisallowHeapAllocation no_allocation;
  for (Iterator<i::Map> it = this->Classes(); !it.Done(); it.Advance()) {
    if (!it.Current()->is_stable()) return false;
  }
  return true;
}


// Check if [this] and [that] overlap.
template<class Config>
bool TypeImpl<Config>::Maybe(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;

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

  if (!BitsetType::IsInhabited(this->BitsetLub() & that->BitsetLub()))
    return false;
  if (this->IsBitset() || that->IsBitset()) return true;

  if (this->IsClass() != that->IsClass()) return true;

  if (this->IsRange()) {
    if (that->IsConstant()) {
      return Contains(this->AsRange(), *that->AsConstant()->Value());
    }
    return that->IsRange() && Overlap(this->AsRange(), that->AsRange());
  }
  if (that->IsRange()) {
    if (this->IsConstant()) {
      return Contains(that->AsRange(), *this->AsConstant()->Value());
    }
    return this->IsRange() && Overlap(this->AsRange(), that->AsRange());
  }

  return this->SimplyEquals(that);
}


// Return the range in [this], or [NULL].
template<class Config>
typename TypeImpl<Config>::RangeType* TypeImpl<Config>::GetRange() {
  DisallowHeapAllocation no_allocation;
  if (this->IsRange()) return this->AsRange();
  if (this->IsUnion() && this->AsUnion()->Get(1)->IsRange()) {
    return this->AsUnion()->Get(1)->AsRange();
  }
  return NULL;
}


template<class Config>
bool TypeImpl<Config>::Contains(i::Object* value) {
  DisallowHeapAllocation no_allocation;
  for (Iterator<i::Object> it = this->Constants(); !it.Done(); it.Advance()) {
    if (*it.Current() == value) return true;
  }
  if (IsInteger(value)) {
    RangeType* range = this->GetRange();
    if (range != NULL && Contains(range, value)) return true;
  }
  return BitsetType::New(BitsetType::Lub(value))->Is(this);
}


template<class Config>
bool TypeImpl<Config>::UnionType::Wellformed() {
  DisallowHeapAllocation no_allocation;
  // This checks the invariants of the union representation:
  // 1. There are at least two elements.
  // 2. At most one element is a bitset, and it must be the first one.
  // 3. At most one element is a range, and it must be the second one
  //    (even when the first element is not a bitset).
  // 4. No element is itself a union.
  // 5. No element is a subtype of any other.
  DCHECK(this->Length() >= 2);  // (1)
  for (int i = 0; i < this->Length(); ++i) {
    if (i != 0) DCHECK(!this->Get(i)->IsBitset());  // (2)
    if (i != 1) DCHECK(!this->Get(i)->IsRange());  // (3)
    DCHECK(!this->Get(i)->IsUnion());  // (4)
    for (int j = 0; j < this->Length(); ++j) {
      if (i != j) DCHECK(!this->Get(i)->Is(this->Get(j)));  // (5)
    }
  }
  return true;
}


// -----------------------------------------------------------------------------
// Union and intersection


static bool AddIsSafe(int x, int y) {
  return x >= 0 ?
      y <= std::numeric_limits<int>::max() - x :
      y >= std::numeric_limits<int>::min() - x;
}


template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Intersect(
    TypeHandle type1, TypeHandle type2, Region* region) {
  bitset bits = type1->BitsetGlb() & type2->BitsetGlb();
  if (!BitsetType::IsInhabited(bits)) bits = BitsetType::kNone;

  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return BitsetType::New(bits, region);
  }

  // Fast case: top or bottom types.
  if (type1->IsNone() || type2->IsAny()) return type1;  // Shortcut.
  if (type2->IsNone() || type1->IsAny()) return type2;  // Shortcut.

  // Semi-fast case.
  if (type1->Is(type2)) return type1;
  if (type2->Is(type1)) return type2;

  // Slow case: create union.
  int size1 = type1->IsUnion() ? type1->AsUnion()->Length() : 1;
  int size2 = type2->IsUnion() ? type2->AsUnion()->Length() : 1;
  if (!AddIsSafe(size1, size2)) return Any(region);
  int size = size1 + size2;
  if (!AddIsSafe(size, 2)) return Any(region);
  size += 2;
  UnionHandle result = UnionType::New(size, region);
  size = 0;

  // Deal with bitsets.
  result->Set(size++, BitsetType::New(bits, region));

  // Deal with ranges.
  TypeHandle range = None(region);
  RangeType* range1 = type1->GetRange();
  RangeType* range2 = type2->GetRange();
  if (range1 != NULL && range2 != NULL) {
    Limits lim = Intersect(Limits(range1), Limits(range2));
    if (lim.min->Number() <= lim.max->Number()) {
      range = RangeType::New(lim, region);
    }
  }
  result->Set(size++, range);

  size = IntersectAux(type1, type2, result, size, region);
  return NormalizeUnion(result, size);
}


template<class Config>
int TypeImpl<Config>::UpdateRange(
    RangeHandle range, UnionHandle result, int size, Region* region) {
  TypeHandle old_range = result->Get(1);
  DCHECK(old_range->IsRange() || old_range->IsNone());
  if (range->Is(old_range)) return size;
  if (!old_range->Is(range->unhandle())) {
    range = RangeType::New(
        Union(Limits(range->AsRange()), Limits(old_range->AsRange())), region);
  }
  result->Set(1, range);

  // Remove any components that just got subsumed.
  for (int i = 2; i < size; ) {
    if (result->Get(i)->Is(range->unhandle())) {
      result->Set(i, result->Get(--size));
    } else {
      ++i;
    }
  }
  return size;
}


template<class Config>
int TypeImpl<Config>::IntersectAux(
    TypeHandle lhs, TypeHandle rhs,
    UnionHandle result, int size, Region* region) {
  if (lhs->IsUnion()) {
    for (int i = 0, n = lhs->AsUnion()->Length(); i < n; ++i) {
      size = IntersectAux(lhs->AsUnion()->Get(i), rhs, result, size, region);
    }
    return size;
  }
  if (rhs->IsUnion()) {
    for (int i = 0, n = rhs->AsUnion()->Length(); i < n; ++i) {
      size = IntersectAux(lhs, rhs->AsUnion()->Get(i), result, size, region);
    }
    return size;
  }

  if (!BitsetType::IsInhabited(lhs->BitsetLub() & rhs->BitsetLub())) {
    return size;
  }

  if (lhs->IsRange()) {
    if (rhs->IsBitset() || rhs->IsClass()) {
      return UpdateRange(
          Config::template cast<RangeType>(lhs), result, size, region);
    }
    if (rhs->IsConstant() &&
        Contains(lhs->AsRange(), *rhs->AsConstant()->Value())) {
      return AddToUnion(rhs, result, size, region);
    }
    return size;
  }
  if (rhs->IsRange()) {
    if (lhs->IsBitset() || lhs->IsClass()) {
      return UpdateRange(
          Config::template cast<RangeType>(rhs), result, size, region);
    }
    if (lhs->IsConstant() &&
        Contains(rhs->AsRange(), *lhs->AsConstant()->Value())) {
      return AddToUnion(lhs, result, size, region);
    }
    return size;
  }

  if (lhs->IsBitset() || rhs->IsBitset()) {
    return AddToUnion(lhs->IsBitset() ? rhs : lhs, result, size, region);
  }
  if (lhs->IsClass() != rhs->IsClass()) {
    return AddToUnion(lhs->IsClass() ? rhs : lhs, result, size, region);
  }
  if (lhs->SimplyEquals(rhs->unhandle())) {
    return AddToUnion(lhs, result, size, region);
  }
  return size;
}


template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Union(
    TypeHandle type1, TypeHandle type2, Region* region) {

  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return BitsetType::New(type1->AsBitset() | type2->AsBitset(), region);
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
  if (!AddIsSafe(size1, size2)) return Any(region);
  int size = size1 + size2;
  if (!AddIsSafe(size, 2)) return Any(region);
  size += 2;
  UnionHandle result = UnionType::New(size, region);
  size = 0;

  // Deal with bitsets.
  TypeHandle bits = BitsetType::New(
      type1->BitsetGlb() | type2->BitsetGlb(), region);
  result->Set(size++, bits);

  // Deal with ranges.
  TypeHandle range = None(region);
  RangeType* range1 = type1->GetRange();
  RangeType* range2 = type2->GetRange();
  if (range1 != NULL && range2 != NULL) {
    range = RangeType::New(Union(Limits(range1), Limits(range2)), region);
  } else if (range1 != NULL) {
    range = handle(range1);
  } else if (range2 != NULL) {
    range = handle(range2);
  }
  result->Set(size++, range);

  size = AddToUnion(type1, result, size, region);
  size = AddToUnion(type2, result, size, region);
  return NormalizeUnion(result, size);
}


// Add [type] to [result] unless [type] is bitset, range, or already subsumed.
// Return new size of [result].
template<class Config>
int TypeImpl<Config>::AddToUnion(
    TypeHandle type, UnionHandle result, int size, Region* region) {
  if (type->IsBitset() || type->IsRange()) return size;
  if (type->IsUnion()) {
    for (int i = 0, n = type->AsUnion()->Length(); i < n; ++i) {
      size = AddToUnion(type->AsUnion()->Get(i), result, size, region);
    }
    return size;
  }
  for (int i = 0; i < size; ++i) {
    if (type->Is(result->Get(i))) return size;
  }
  result->Set(size++, type);
  return size;
}


template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::NormalizeUnion(
    UnionHandle unioned, int size) {
  DCHECK(size >= 2);
  // If range is subsumed by bitset, use its place for a different type.
  if (unioned->Get(1)->Is(unioned->Get(0))) {
    unioned->Set(1, unioned->Get(--size));
  }
  // If bitset is None, use its place for a different type.
  if (size >= 2 && unioned->Get(0)->IsNone()) {
    unioned->Set(0, unioned->Get(--size));
  }
  if (size == 1) return unioned->Get(0);
  unioned->Shrink(size);
  SLOW_DCHECK(unioned->Wellformed());
  return unioned;
}


// -----------------------------------------------------------------------------
// Iteration.

template<class Config>
int TypeImpl<Config>::NumClasses() {
  DisallowHeapAllocation no_allocation;
  if (this->IsClass()) {
    return 1;
  } else if (this->IsUnion()) {
    int result = 0;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      if (this->AsUnion()->Get(i)->IsClass()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config>
int TypeImpl<Config>::NumConstants() {
  DisallowHeapAllocation no_allocation;
  if (this->IsConstant()) {
    return 1;
  } else if (this->IsUnion()) {
    int result = 0;
    for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
      if (this->AsUnion()->Get(i)->IsConstant()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config> template<class T>
typename TypeImpl<Config>::TypeHandle
TypeImpl<Config>::Iterator<T>::get_type() {
  DCHECK(!Done());
  return type_->IsUnion() ? type_->AsUnion()->Get(index_) : type_;
}


// C++ cannot specialise nested templates, so we have to go through this
// contortion with an auxiliary template to simulate it.
template<class Config, class T>
struct TypeImplIteratorAux {
  static bool matches(typename TypeImpl<Config>::TypeHandle type);
  static i::Handle<T> current(typename TypeImpl<Config>::TypeHandle type);
};

template<class Config>
struct TypeImplIteratorAux<Config, i::Map> {
  static bool matches(typename TypeImpl<Config>::TypeHandle type) {
    return type->IsClass();
  }
  static i::Handle<i::Map> current(typename TypeImpl<Config>::TypeHandle type) {
    return type->AsClass()->Map();
  }
};

template<class Config>
struct TypeImplIteratorAux<Config, i::Object> {
  static bool matches(typename TypeImpl<Config>::TypeHandle type) {
    return type->IsConstant();
  }
  static i::Handle<i::Object> current(
      typename TypeImpl<Config>::TypeHandle type) {
    return type->AsConstant()->Value();
  }
};

template<class Config> template<class T>
bool TypeImpl<Config>::Iterator<T>::matches(TypeHandle type) {
  return TypeImplIteratorAux<Config, T>::matches(type);
}

template<class Config> template<class T>
i::Handle<T> TypeImpl<Config>::Iterator<T>::Current() {
  return TypeImplIteratorAux<Config, T>::current(get_type());
}


template<class Config> template<class T>
void TypeImpl<Config>::Iterator<T>::Advance() {
  DisallowHeapAllocation no_allocation;
  ++index_;
  if (type_->IsUnion()) {
    for (int n = type_->AsUnion()->Length(); index_ < n; ++index_) {
      if (matches(type_->AsUnion()->Get(index_))) return;
    }
  } else if (index_ == 0 && matches(type_)) {
    return;
  }
  index_ = -1;
}


// -----------------------------------------------------------------------------
// Conversion between low-level representations.

template<class Config>
template<class OtherType>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Convert(
    typename OtherType::TypeHandle type, Region* region) {
  if (type->IsBitset()) {
    return BitsetType::New(type->AsBitset(), region);
  } else if (type->IsClass()) {
    return ClassType::New(type->AsClass()->Map(), region);
  } else if (type->IsConstant()) {
    return ConstantType::New(type->AsConstant()->Value(), region);
  } else if (type->IsRange()) {
    return RangeType::New(
        type->AsRange()->Min(), type->AsRange()->Max(), region);
  } else if (type->IsContext()) {
    TypeHandle outer = Convert<OtherType>(type->AsContext()->Outer(), region);
    return ContextType::New(outer, region);
  } else if (type->IsUnion()) {
    int length = type->AsUnion()->Length();
    UnionHandle unioned = UnionType::New(length, region);
    for (int i = 0; i < length; ++i) {
      TypeHandle t = Convert<OtherType>(type->AsUnion()->Get(i), region);
      unioned->Set(i, t);
    }
    return unioned;
  } else if (type->IsArray()) {
    TypeHandle element = Convert<OtherType>(type->AsArray()->Element(), region);
    return ArrayType::New(element, region);
  } else if (type->IsFunction()) {
    TypeHandle res = Convert<OtherType>(type->AsFunction()->Result(), region);
    TypeHandle rcv = Convert<OtherType>(type->AsFunction()->Receiver(), region);
    FunctionHandle function = FunctionType::New(
        res, rcv, type->AsFunction()->Arity(), region);
    for (int i = 0; i < function->Arity(); ++i) {
      TypeHandle param = Convert<OtherType>(
          type->AsFunction()->Parameter(i), region);
      function->InitParameter(i, param);
    }
    return function;
  } else {
    UNREACHABLE();
    return None(region);
  }
}


// -----------------------------------------------------------------------------
// Printing.

template<class Config>
const char* TypeImpl<Config>::BitsetType::Name(bitset bits) {
  switch (bits) {
    case REPRESENTATION(kAny): return "Any";
    #define RETURN_NAMED_REPRESENTATION_TYPE(type, value) \
    case REPRESENTATION(k##type): return #type;
    REPRESENTATION_BITSET_TYPE_LIST(RETURN_NAMED_REPRESENTATION_TYPE)
    #undef RETURN_NAMED_REPRESENTATION_TYPE

    #define RETURN_NAMED_SEMANTIC_TYPE(type, value) \
    case SEMANTIC(k##type): return #type;
    SEMANTIC_BITSET_TYPE_LIST(RETURN_NAMED_SEMANTIC_TYPE)
    #undef RETURN_NAMED_SEMANTIC_TYPE

    default:
      return NULL;
  }
}


template <class Config>
void TypeImpl<Config>::BitsetType::Print(std::ostream& os,  // NOLINT
                                         bitset bits) {
  DisallowHeapAllocation no_allocation;
  const char* name = Name(bits);
  if (name != NULL) {
    os << name;
    return;
  }

  static const bitset named_bitsets[] = {
#define BITSET_CONSTANT(type, value) REPRESENTATION(k##type),
      REPRESENTATION_BITSET_TYPE_LIST(BITSET_CONSTANT)
#undef BITSET_CONSTANT

#define BITSET_CONSTANT(type, value) SEMANTIC(k##type),
      SEMANTIC_BITSET_TYPE_LIST(BITSET_CONSTANT)
#undef BITSET_CONSTANT
  };

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


template <class Config>
void TypeImpl<Config>::PrintTo(std::ostream& os, PrintDimension dim) {
  DisallowHeapAllocation no_allocation;
  if (dim != REPRESENTATION_DIM) {
    if (this->IsBitset()) {
      BitsetType::Print(os, SEMANTIC(this->AsBitset()));
    } else if (this->IsClass()) {
      os << "Class(" << static_cast<void*>(*this->AsClass()->Map()) << " < ";
      BitsetType::New(BitsetType::Lub(this))->PrintTo(os, dim);
      os << ")";
    } else if (this->IsConstant()) {
      os << "Constant(" << Brief(*this->AsConstant()->Value()) << ")";
    } else if (this->IsRange()) {
      os << "Range(" << this->AsRange()->Min()->Number()
         << ", " << this->AsRange()->Max()->Number() << ")";
    } else if (this->IsContext()) {
      os << "Context(";
      this->AsContext()->Outer()->PrintTo(os, dim);
      os << ")";
    } else if (this->IsUnion()) {
      os << "(";
      for (int i = 0, n = this->AsUnion()->Length(); i < n; ++i) {
        TypeHandle type_i = this->AsUnion()->Get(i);
        if (i > 0) os << " | ";
        type_i->PrintTo(os, dim);
      }
      os << ")";
    } else if (this->IsArray()) {
      os << "Array(";
      AsArray()->Element()->PrintTo(os, dim);
      os << ")";
    } else if (this->IsFunction()) {
      if (!this->AsFunction()->Receiver()->IsAny()) {
        this->AsFunction()->Receiver()->PrintTo(os, dim);
        os << ".";
      }
      os << "(";
      for (int i = 0; i < this->AsFunction()->Arity(); ++i) {
        if (i > 0) os << ", ";
        this->AsFunction()->Parameter(i)->PrintTo(os, dim);
      }
      os << ")->";
      this->AsFunction()->Result()->PrintTo(os, dim);
    } else {
      UNREACHABLE();
    }
  }
  if (dim == BOTH_DIMS) os << "/";
  if (dim != SEMANTIC_DIM) {
    BitsetType::Print(os, REPRESENTATION(this->BitsetLub()));
  }
}


#ifdef DEBUG
template <class Config>
void TypeImpl<Config>::Print() {
  OFStream os(stdout);
  PrintTo(os);
  os << std::endl;
}
template <class Config>
void TypeImpl<Config>::BitsetType::Print(bitset bits) {
  OFStream os(stdout);
  Print(os, bits);
  os << std::endl;
}
#endif


// -----------------------------------------------------------------------------
// Instantiations.

template class TypeImpl<ZoneTypeConfig>;
template class TypeImpl<ZoneTypeConfig>::Iterator<i::Map>;
template class TypeImpl<ZoneTypeConfig>::Iterator<i::Object>;

template class TypeImpl<HeapTypeConfig>;
template class TypeImpl<HeapTypeConfig>::Iterator<i::Map>;
template class TypeImpl<HeapTypeConfig>::Iterator<i::Object>;

template TypeImpl<ZoneTypeConfig>::TypeHandle
  TypeImpl<ZoneTypeConfig>::Convert<HeapType>(
    TypeImpl<HeapTypeConfig>::TypeHandle, TypeImpl<ZoneTypeConfig>::Region*);
template TypeImpl<HeapTypeConfig>::TypeHandle
  TypeImpl<HeapTypeConfig>::Convert<Type>(
    TypeImpl<ZoneTypeConfig>::TypeHandle, TypeImpl<HeapTypeConfig>::Region*);

} }  // namespace v8::internal
