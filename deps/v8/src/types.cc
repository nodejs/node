// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "types.h"

#include "string-stream.h"
#include "types-inl.h"

namespace v8 {
namespace internal {

template<class Config>
int TypeImpl<Config>::NumClasses() {
  DisallowHeapAllocation no_allocation;
  if (this->IsClass()) {
    return 1;
  } else if (this->IsUnion()) {
    UnionHandle unioned = handle(this->AsUnion());
    int result = 0;
    for (int i = 0; i < unioned->Length(); ++i) {
      if (unioned->Get(i)->IsClass()) ++result;
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
    UnionHandle unioned = handle(this->AsUnion());
    int result = 0;
    for (int i = 0; i < unioned->Length(); ++i) {
      if (unioned->Get(i)->IsConstant()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config> template<class T>
typename TypeImpl<Config>::TypeHandle
TypeImpl<Config>::Iterator<T>::get_type() {
  ASSERT(!Done());
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
    UnionHandle unioned = handle(type_->AsUnion());
    for (; index_ < unioned->Length(); ++index_) {
      if (matches(unioned->Get(index_))) return;
    }
  } else if (index_ == 0 && matches(type_)) {
    return;
  }
  index_ = -1;
}


// Get the largest bitset subsumed by this type.
template<class Config>
int TypeImpl<Config>::BitsetType::Glb(TypeImpl* type) {
  DisallowHeapAllocation no_allocation;
  if (type->IsBitset()) {
    return type->AsBitset();
  } else if (type->IsUnion()) {
    // All but the first are non-bitsets and thus would yield kNone anyway.
    return type->AsUnion()->Get(0)->BitsetGlb();
  } else {
    return kNone;
  }
}


// Get the smallest bitset subsuming this type.
template<class Config>
int TypeImpl<Config>::BitsetType::Lub(TypeImpl* type) {
  DisallowHeapAllocation no_allocation;
  if (type->IsBitset()) {
    return type->AsBitset();
  } else if (type->IsUnion()) {
    UnionHandle unioned = handle(type->AsUnion());
    int bitset = kNone;
    for (int i = 0; i < unioned->Length(); ++i) {
      bitset |= unioned->Get(i)->BitsetLub();
    }
    return bitset;
  } else if (type->IsClass()) {
    int bitset = Config::lub_bitset(type);
    return bitset ? bitset : Lub(*type->AsClass()->Map());
  } else if (type->IsConstant()) {
    int bitset = Config::lub_bitset(type);
    return bitset ? bitset : Lub(*type->AsConstant()->Value());
  } else if (type->IsArray()) {
    return kArray;
  } else if (type->IsFunction()) {
    return kFunction;
  } else {
    UNREACHABLE();
    return kNone;
  }
}


template<class Config>
int TypeImpl<Config>::BitsetType::Lub(i::Object* value) {
  DisallowHeapAllocation no_allocation;
  if (value->IsSmi()) return kSignedSmall & kTaggedInt;
  i::Map* map = i::HeapObject::cast(value)->map();
  if (map->instance_type() == HEAP_NUMBER_TYPE) {
    int32_t i;
    uint32_t u;
    return kTaggedPtr & (
        value->ToInt32(&i) ? (Smi::IsValid(i) ? kSignedSmall : kOtherSigned32) :
        value->ToUint32(&u) ? kUnsigned32 : kFloat);
  }
  return Lub(map);
}


template<class Config>
int TypeImpl<Config>::BitsetType::Lub(i::Map* map) {
  DisallowHeapAllocation no_allocation;
  switch (map->instance_type()) {
    case STRING_TYPE:
    case ASCII_STRING_TYPE:
    case CONS_STRING_TYPE:
    case CONS_ASCII_STRING_TYPE:
    case SLICED_STRING_TYPE:
    case SLICED_ASCII_STRING_TYPE:
    case EXTERNAL_STRING_TYPE:
    case EXTERNAL_ASCII_STRING_TYPE:
    case EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_STRING_TYPE:
    case SHORT_EXTERNAL_ASCII_STRING_TYPE:
    case SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case INTERNALIZED_STRING_TYPE:
    case ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return kString;
    case SYMBOL_TYPE:
      return kSymbol;
    case ODDBALL_TYPE: {
      Heap* heap = map->GetHeap();
      if (map == heap->undefined_map()) return kUndefined;
      if (map == heap->the_hole_map()) return kAny;  // TODO(rossberg): kNone?
      if (map == heap->null_map()) return kNull;
      if (map == heap->boolean_map()) return kBoolean;
      ASSERT(map == heap->uninitialized_map() ||
             map == heap->no_interceptor_result_sentinel_map() ||
             map == heap->termination_exception_map() ||
             map == heap->arguments_marker_map());
      return kInternal & kTaggedPtr;
    }
    case HEAP_NUMBER_TYPE:
      return kFloat & kTaggedPtr;
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
    case ACCESSOR_PAIR_TYPE:
    case FIXED_ARRAY_TYPE:
    case FOREIGN_TYPE:
      return kInternal & kTaggedPtr;
    default:
      UNREACHABLE();
      return kNone;
  }
}


// Most precise _current_ type of a value (usually its class).
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::NowOf(
    i::Object* value, Region* region) {
  if (value->IsSmi() ||
      i::HeapObject::cast(value)->map()->instance_type() == HEAP_NUMBER_TYPE) {
    return Of(value, region);
  }
  return Class(i::handle(i::HeapObject::cast(value)->map()), region);
}


// Check this <= that.
template<class Config>
bool TypeImpl<Config>::SlowIs(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;

  // Fast path for bitsets.
  if (this->IsNone()) return true;
  if (that->IsBitset()) {
    return (BitsetType::Lub(this) | that->AsBitset()) == that->AsBitset();
  }

  if (that->IsClass()) {
    return this->IsClass()
        && *this->AsClass()->Map() == *that->AsClass()->Map();
  }
  if (that->IsConstant()) {
    return this->IsConstant()
        && *this->AsConstant()->Value() == *that->AsConstant()->Value();
  }
  if (that->IsArray()) {
    return this->IsArray()
        && this->AsArray()->Element()->Equals(that->AsArray()->Element());
  }
  if (that->IsFunction()) {
    // We currently do not allow for any variance here, in order to keep
    // Union and Intersect operations simple.
    if (!this->IsFunction()) return false;
    FunctionType* this_fun = this->AsFunction();
    FunctionType* that_fun = that->AsFunction();
    if (this_fun->Arity() != that_fun->Arity() ||
        !this_fun->Result()->Equals(that_fun->Result()) ||
        !that_fun->Receiver()->Equals(this_fun->Receiver())) {
      return false;
    }
    for (int i = 0; i < this_fun->Arity(); ++i) {
      if (!that_fun->Parameter(i)->Equals(this_fun->Parameter(i))) return false;
    }
    return true;
  }

  // (T1 \/ ... \/ Tn) <= T  <=>  (T1 <= T) /\ ... /\ (Tn <= T)
  if (this->IsUnion()) {
    UnionHandle unioned = handle(this->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      if (!unioned->Get(i)->Is(that)) return false;
    }
    return true;
  }

  // T <= (T1 \/ ... \/ Tn)  <=>  (T <= T1) \/ ... \/ (T <= Tn)
  // (iff T is not a union)
  ASSERT(!this->IsUnion());
  if (that->IsUnion()) {
    UnionHandle unioned = handle(that->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      if (this->Is(unioned->Get(i))) return true;
      if (this->IsBitset()) break;  // Fast fail, only first field is a bitset.
    }
    return false;
  }

  return false;
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


// Check if this contains only (currently) stable classes.
template<class Config>
bool TypeImpl<Config>::NowStable() {
  DisallowHeapAllocation no_allocation;
  for (Iterator<i::Map> it = this->Classes(); !it.Done(); it.Advance()) {
    if (!it.Current()->is_stable()) return false;
  }
  return true;
}


// Check this overlaps that.
template<class Config>
bool TypeImpl<Config>::Maybe(TypeImpl* that) {
  DisallowHeapAllocation no_allocation;

  // (T1 \/ ... \/ Tn) overlaps T <=> (T1 overlaps T) \/ ... \/ (Tn overlaps T)
  if (this->IsUnion()) {
    UnionHandle unioned = handle(this->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      if (unioned->Get(i)->Maybe(that)) return true;
    }
    return false;
  }

  // T overlaps (T1 \/ ... \/ Tn) <=> (T overlaps T1) \/ ... \/ (T overlaps Tn)
  if (that->IsUnion()) {
    UnionHandle unioned = handle(that->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      if (this->Maybe(unioned->Get(i))) return true;
    }
    return false;
  }

  ASSERT(!this->IsUnion() && !that->IsUnion());
  if (this->IsBitset()) {
    return BitsetType::IsInhabited(this->AsBitset() & that->BitsetLub());
  }
  if (that->IsBitset()) {
    return BitsetType::IsInhabited(this->BitsetLub() & that->AsBitset());
  }
  if (this->IsClass()) {
    return that->IsClass()
        && *this->AsClass()->Map() == *that->AsClass()->Map();
  }
  if (this->IsConstant()) {
    return that->IsConstant()
        && *this->AsConstant()->Value() == *that->AsConstant()->Value();
  }
  if (this->IsArray()) {
    // There is no variance!
    return this->Equals(that);
  }
  if (this->IsFunction()) {
    // There is no variance!
    return this->Equals(that);
  }

  return false;
}


template<class Config>
bool TypeImpl<Config>::Contains(i::Object* value) {
  DisallowHeapAllocation no_allocation;

  for (Iterator<i::Object> it = this->Constants(); !it.Done(); it.Advance()) {
    if (*it.Current() == value) return true;
  }
  return BitsetType::New(BitsetType::Lub(value))->Is(this);
}


template<class Config>
bool TypeImpl<Config>::InUnion(UnionHandle unioned, int current_size) {
  ASSERT(!this->IsUnion());
  for (int i = 0; i < current_size; ++i) {
    if (this->Is(unioned->Get(i))) return true;
  }
  return false;
}


// Get non-bitsets from this which are not subsumed by union, store at result,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendUnion(
    UnionHandle result, TypeHandle type, int current_size) {
  int old_size = current_size;
  if (type->IsUnion()) {
    UnionHandle unioned = handle(type->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      TypeHandle type = unioned->Get(i);
      ASSERT(i == 0 || !(type->IsBitset() || type->Is(unioned->Get(0))));
      if (!type->IsBitset() && !type->InUnion(result, old_size)) {
        result->Set(current_size++, type);
      }
    }
  } else if (!type->IsBitset()) {
    // For all structural types, subtyping implies equivalence.
    ASSERT(type->IsClass() || type->IsConstant() ||
           type->IsArray() || type->IsFunction());
    if (!type->InUnion(result, old_size)) {
      result->Set(current_size++, type);
    }
  }
  return current_size;
}


// Union is O(1) on simple bit unions, but O(n*m) on structured unions.
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

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type2;
    if (type2->Is(type1)) return type1;
  }

  // Slow case: may need to produce a Unioned object.
  int size = 0;
  if (!type1->IsBitset()) {
    size += (type1->IsUnion() ? type1->AsUnion()->Length() : 1);
  }
  if (!type2->IsBitset()) {
    size += (type2->IsUnion() ? type2->AsUnion()->Length() : 1);
  }
  int bitset = type1->BitsetGlb() | type2->BitsetGlb();
  if (bitset != BitsetType::kNone) ++size;
  ASSERT(size >= 1);

  UnionHandle unioned = UnionType::New(size, region);
  size = 0;
  if (bitset != BitsetType::kNone) {
    unioned->Set(size++, BitsetType::New(bitset, region));
  }
  size = ExtendUnion(unioned, type1, size);
  size = ExtendUnion(unioned, type2, size);

  if (size == 1) {
    return unioned->Get(0);
  } else {
    unioned->Shrink(size);
    return unioned;
  }
}


// Get non-bitsets from type which are also in other, store at result,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendIntersection(
    UnionHandle result, TypeHandle type, TypeHandle other, int current_size) {
  int old_size = current_size;
  if (type->IsUnion()) {
    UnionHandle unioned = handle(type->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      TypeHandle type = unioned->Get(i);
      ASSERT(i == 0 || !(type->IsBitset() || type->Is(unioned->Get(0))));
      if (!type->IsBitset() && type->Is(other) &&
          !type->InUnion(result, old_size)) {
        result->Set(current_size++, type);
      }
    }
  } else if (!type->IsBitset()) {
    // For all structural types, subtyping implies equivalence.
    ASSERT(type->IsClass() || type->IsConstant() ||
           type->IsArray() || type->IsFunction());
    if (type->Is(other) && !type->InUnion(result, old_size)) {
      result->Set(current_size++, type);
    }
  }
  return current_size;
}


// Intersection is O(1) on simple bit unions, but O(n*m) on structured unions.
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Intersect(
    TypeHandle type1, TypeHandle type2, Region* region) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return BitsetType::New(type1->AsBitset() & type2->AsBitset(), region);
  }

  // Fast case: top or bottom types.
  if (type1->IsNone() || type2->IsAny()) return type1;
  if (type2->IsNone() || type1->IsAny()) return type2;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type1;
    if (type2->Is(type1)) return type2;
  }

  // Slow case: may need to produce a Unioned object.
  int size = 0;
  if (!type1->IsBitset()) {
    size += (type1->IsUnion() ? type1->AsUnion()->Length() : 1);
  }
  if (!type2->IsBitset()) {
    size += (type2->IsUnion() ? type2->AsUnion()->Length() : 1);
  }
  int bitset = type1->BitsetGlb() & type2->BitsetGlb();
  if (bitset != BitsetType::kNone) ++size;
  ASSERT(size >= 1);

  UnionHandle unioned = UnionType::New(size, region);
  size = 0;
  if (bitset != BitsetType::kNone) {
    unioned->Set(size++, BitsetType::New(bitset, region));
  }
  size = ExtendIntersection(unioned, type1, type2, size);
  size = ExtendIntersection(unioned, type2, type1, size);

  if (size == 0) {
    return None(region);
  } else if (size == 1) {
    return unioned->Get(0);
  } else {
    unioned->Shrink(size);
    return unioned;
  }
}


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
  } else if (type->IsUnion()) {
    int length = type->AsUnion()->Length();
    UnionHandle unioned = UnionType::New(length, region);
    for (int i = 0; i < length; ++i) {
      unioned->Set(i, Convert<OtherType>(type->AsUnion()->Get(i), region));
    }
    return unioned;
  } else if (type->IsArray()) {
    return ArrayType::New(
        Convert<OtherType>(type->AsArray()->Element(), region), region);
  } else if (type->IsFunction()) {
    FunctionHandle function = FunctionType::New(
        Convert<OtherType>(type->AsFunction()->Result(), region),
        Convert<OtherType>(type->AsFunction()->Receiver(), region),
        type->AsFunction()->Arity(), region);
    for (int i = 0; i < function->Arity(); ++i) {
      function->InitParameter(i,
          Convert<OtherType>(type->AsFunction()->Parameter(i), region));
    }
    return function;
  } else {
    UNREACHABLE();
    return None(region);
  }
}


// TODO(rossberg): this does not belong here.
Representation Representation::FromType(Type* type) {
  DisallowHeapAllocation no_allocation;
  if (type->Is(Type::None())) return Representation::None();
  if (type->Is(Type::SignedSmall())) return Representation::Smi();
  if (type->Is(Type::Signed32())) return Representation::Integer32();
  if (type->Is(Type::Number())) return Representation::Double();
  return Representation::Tagged();
}


template<class Config>
void TypeImpl<Config>::TypePrint(PrintDimension dim) {
  TypePrint(stdout, dim);
  PrintF(stdout, "\n");
  Flush(stdout);
}


template<class Config>
const char* TypeImpl<Config>::BitsetType::Name(int bitset) {
  switch (bitset) {
    case kAny & kRepresentation: return "Any";
    #define PRINT_COMPOSED_TYPE(type, value) \
    case k##type & kRepresentation: return #type;
    REPRESENTATION_BITSET_TYPE_LIST(PRINT_COMPOSED_TYPE)
    #undef PRINT_COMPOSED_TYPE

    #define PRINT_COMPOSED_TYPE(type, value) \
    case k##type & kSemantic: return #type;
    SEMANTIC_BITSET_TYPE_LIST(PRINT_COMPOSED_TYPE)
    #undef PRINT_COMPOSED_TYPE

    default:
      return NULL;
  }
}


template<class Config>
void TypeImpl<Config>::BitsetType::BitsetTypePrint(FILE* out, int bitset) {
  DisallowHeapAllocation no_allocation;
  const char* name = Name(bitset);
  if (name != NULL) {
    PrintF(out, "%s", name);
  } else {
    static const int named_bitsets[] = {
      #define BITSET_CONSTANT(type, value) k##type & kRepresentation,
      REPRESENTATION_BITSET_TYPE_LIST(BITSET_CONSTANT)
      #undef BITSET_CONSTANT

      #define BITSET_CONSTANT(type, value) k##type & kSemantic,
      SEMANTIC_BITSET_TYPE_LIST(BITSET_CONSTANT)
      #undef BITSET_CONSTANT
    };

    bool is_first = true;
    PrintF(out, "(");
    for (int i(ARRAY_SIZE(named_bitsets) - 1); bitset != 0 && i >= 0; --i) {
      int subset = named_bitsets[i];
      if ((bitset & subset) == subset) {
        if (!is_first) PrintF(out, " | ");
        is_first = false;
        PrintF(out, "%s", Name(subset));
        bitset -= subset;
      }
    }
    ASSERT(bitset == 0);
    PrintF(out, ")");
  }
}


template<class Config>
void TypeImpl<Config>::TypePrint(FILE* out, PrintDimension dim) {
  DisallowHeapAllocation no_allocation;
  if (this->IsBitset()) {
    int bitset = this->AsBitset();
    switch (dim) {
      case BOTH_DIMS:
        BitsetType::BitsetTypePrint(out, bitset & BitsetType::kSemantic);
        PrintF(out, "/");
        BitsetType::BitsetTypePrint(out, bitset & BitsetType::kRepresentation);
        break;
      case SEMANTIC_DIM:
        BitsetType::BitsetTypePrint(out, bitset & BitsetType::kSemantic);
        break;
      case REPRESENTATION_DIM:
        BitsetType::BitsetTypePrint(out, bitset & BitsetType::kRepresentation);
        break;
    }
  } else if (this->IsConstant()) {
    PrintF(out, "Constant(%p : ",
        static_cast<void*>(*this->AsConstant()->Value()));
    BitsetType::New(BitsetType::Lub(this))->TypePrint(out, dim);
    PrintF(out, ")");
  } else if (this->IsClass()) {
    PrintF(out, "Class(%p < ", static_cast<void*>(*this->AsClass()->Map()));
    BitsetType::New(BitsetType::Lub(this))->TypePrint(out, dim);
    PrintF(out, ")");
  } else if (this->IsUnion()) {
    PrintF(out, "(");
    UnionHandle unioned = handle(this->AsUnion());
    for (int i = 0; i < unioned->Length(); ++i) {
      TypeHandle type_i = unioned->Get(i);
      if (i > 0) PrintF(out, " | ");
      type_i->TypePrint(out, dim);
    }
    PrintF(out, ")");
  } else if (this->IsArray()) {
    PrintF(out, "[");
    AsArray()->Element()->TypePrint(out, dim);
    PrintF(out, "]");
  } else if (this->IsFunction()) {
    if (!this->AsFunction()->Receiver()->IsAny()) {
      this->AsFunction()->Receiver()->TypePrint(out, dim);
      PrintF(out, ".");
    }
    PrintF(out, "(");
    for (int i = 0; i < this->AsFunction()->Arity(); ++i) {
      if (i > 0) PrintF(out, ", ");
      this->AsFunction()->Parameter(i)->TypePrint(out, dim);
    }
    PrintF(out, ")->");
    this->AsFunction()->Result()->TypePrint(out, dim);
  } else {
    UNREACHABLE();
  }
}


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
