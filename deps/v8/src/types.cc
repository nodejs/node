// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "types.h"
#include "string-stream.h"

namespace v8 {
namespace internal {

template<class Config>
int TypeImpl<Config>::NumClasses() {
  if (this->IsClass()) {
    return 1;
  } else if (this->IsUnion()) {
    UnionedHandle unioned = this->AsUnion();
    int result = 0;
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      if (Config::union_get(unioned, i)->IsClass()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class Config>
int TypeImpl<Config>::NumConstants() {
  if (this->IsConstant()) {
    return 1;
  } else if (this->IsUnion()) {
    UnionedHandle unioned = this->AsUnion();
    int result = 0;
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      if (Config::union_get(unioned, i)->IsConstant()) ++result;
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
  return type_->IsUnion() ? Config::union_get(type_->AsUnion(), index_) : type_;
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
    return type->AsClass();
  }
};

template<class Config>
struct TypeImplIteratorAux<Config, i::Object> {
  static bool matches(typename TypeImpl<Config>::TypeHandle type) {
    return type->IsConstant();
  }
  static i::Handle<i::Object> current(
      typename TypeImpl<Config>::TypeHandle type) {
    return type->AsConstant();
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
  ++index_;
  if (type_->IsUnion()) {
    UnionedHandle unioned = type_->AsUnion();
    for (; index_ < Config::union_length(unioned); ++index_) {
      if (matches(Config::union_get(unioned, index_))) return;
    }
  } else if (index_ == 0 && matches(type_)) {
    return;
  }
  index_ = -1;
}


// Get the smallest bitset subsuming this type.
template<class Config>
int TypeImpl<Config>::LubBitset() {
  if (this->IsBitset()) {
    return this->AsBitset();
  } else if (this->IsUnion()) {
    UnionedHandle unioned = this->AsUnion();
    int bitset = kNone;
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      bitset |= Config::union_get(unioned, i)->LubBitset();
    }
    return bitset;
  } else if (this->IsClass()) {
    return LubBitset(*this->AsClass());
  } else {
    return LubBitset(*this->AsConstant());
  }
}


template<class Config>
int TypeImpl<Config>::LubBitset(i::Object* value) {
  if (value->IsSmi()) return kSmi;
  i::Map* map = i::HeapObject::cast(value)->map();
  if (map->instance_type() == HEAP_NUMBER_TYPE) {
    int32_t i;
    uint32_t u;
    if (value->ToInt32(&i)) return Smi::IsValid(i) ? kSmi : kOtherSigned32;
    if (value->ToUint32(&u)) return kUnsigned32;
    return kDouble;
  }
  if (map->instance_type() == ODDBALL_TYPE) {
    if (value->IsUndefined()) return kUndefined;
    if (value->IsNull()) return kNull;
    if (value->IsBoolean()) return kBoolean;
    if (value->IsTheHole()) return kAny;  // TODO(rossberg): kNone?
    UNREACHABLE();
  }
  return LubBitset(map);
}


template<class Config>
int TypeImpl<Config>::LubBitset(i::Map* map) {
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
    case CONS_INTERNALIZED_STRING_TYPE:
    case CONS_ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE:
    case SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE:
      return kString;
    case SYMBOL_TYPE:
      return kSymbol;
    case ODDBALL_TYPE:
      return kOddball;
    case HEAP_NUMBER_TYPE:
      return kDouble;
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
      return kInternal;
    default:
      UNREACHABLE();
      return kNone;
  }
}


// Get the largest bitset subsumed by this type.
template<class Config>
int TypeImpl<Config>::GlbBitset() {
  if (this->IsBitset()) {
    return this->AsBitset();
  } else if (this->IsUnion()) {
    // All but the first are non-bitsets and thus would yield kNone anyway.
    return Config::union_get(this->AsUnion(), 0)->GlbBitset();
  } else {
    return kNone;
  }
}


// Most precise _current_ type of a value (usually its class).
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::OfCurrently(
    i::Handle<i::Object> value, Region* region) {
  if (value->IsSmi()) return Smi(region);
  i::Map* map = i::HeapObject::cast(*value)->map();
  if (map->instance_type() == HEAP_NUMBER_TYPE ||
      map->instance_type() == ODDBALL_TYPE) {
    return Of(value, region);
  }
  return Class(i::handle(map), region);
}


// Check this <= that.
template<class Config>
bool TypeImpl<Config>::SlowIs(TypeImpl* that) {
  // Fast path for bitsets.
  if (this->IsNone()) return true;
  if (that->IsBitset()) {
    return (this->LubBitset() | that->AsBitset()) == that->AsBitset();
  }

  if (that->IsClass()) {
    return this->IsClass() && *this->AsClass() == *that->AsClass();
  }
  if (that->IsConstant()) {
    return this->IsConstant() && *this->AsConstant() == *that->AsConstant();
  }

  // (T1 \/ ... \/ Tn) <= T  <=>  (T1 <= T) /\ ... /\ (Tn <= T)
  if (this->IsUnion()) {
    UnionedHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle this_i = Config::union_get(unioned, i);
      if (!this_i->Is(that)) return false;
    }
    return true;
  }

  // T <= (T1 \/ ... \/ Tn)  <=>  (T <= T1) \/ ... \/ (T <= Tn)
  // (iff T is not a union)
  ASSERT(!this->IsUnion());
  if (that->IsUnion()) {
    UnionedHandle unioned = that->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle that_i = Config::union_get(unioned, i);
      if (this->Is(that_i)) return true;
      if (this->IsBitset()) break;  // Fast fail, only first field is a bitset.
    }
    return false;
  }

  return false;
}


template<class Config>
bool TypeImpl<Config>::IsCurrently(TypeImpl* that) {
  return this->Is(that) ||
      (this->IsConstant() && that->IsClass() &&
       this->AsConstant()->IsHeapObject() &&
       i::HeapObject::cast(*this->AsConstant())->map() == *that->AsClass());
}


// Check this overlaps that.
template<class Config>
bool TypeImpl<Config>::Maybe(TypeImpl* that) {
  // Fast path for bitsets.
  if (this->IsBitset()) {
    return (this->AsBitset() & that->LubBitset()) != 0;
  }
  if (that->IsBitset()) {
    return (this->LubBitset() & that->AsBitset()) != 0;
  }

  // (T1 \/ ... \/ Tn) overlaps T <=> (T1 overlaps T) \/ ... \/ (Tn overlaps T)
  if (this->IsUnion()) {
    UnionedHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle this_i = Config::union_get(unioned, i);
      if (this_i->Maybe(that)) return true;
    }
    return false;
  }

  // T overlaps (T1 \/ ... \/ Tn) <=> (T overlaps T1) \/ ... \/ (T overlaps Tn)
  if (that->IsUnion()) {
    UnionedHandle unioned = that->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle that_i = Config::union_get(unioned, i);
      if (this->Maybe(that_i)) return true;
    }
    return false;
  }

  ASSERT(!this->IsUnion() && !that->IsUnion());
  if (this->IsClass()) {
    return that->IsClass() && *this->AsClass() == *that->AsClass();
  }
  if (this->IsConstant()) {
    return that->IsConstant() && *this->AsConstant() == *that->AsConstant();
  }

  return false;
}


template<class Config>
bool TypeImpl<Config>::InUnion(UnionedHandle unioned, int current_size) {
  ASSERT(!this->IsUnion());
  for (int i = 0; i < current_size; ++i) {
    TypeHandle type = Config::union_get(unioned, i);
    if (this->Is(type)) return true;
  }
  return false;
}


// Get non-bitsets from this which are not subsumed by union, store at unioned,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendUnion(
    UnionedHandle result, TypeHandle type, int current_size) {
  int old_size = current_size;
  if (type->IsClass() || type->IsConstant()) {
    if (!type->InUnion(result, old_size)) {
      Config::union_set(result, current_size++, type);
    }
  } else if (type->IsUnion()) {
    UnionedHandle unioned = type->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle type = Config::union_get(unioned, i);
      ASSERT(i == 0 ||
             !(type->IsBitset() || type->Is(Config::union_get(unioned, 0))));
      if (!type->IsBitset() && !type->InUnion(result, old_size)) {
        Config::union_set(result, current_size++, type);
      }
    }
  }
  return current_size;
}


// Union is O(1) on simple bit unions, but O(n*m) on structured unions.
// TODO(rossberg): Should we use object sets somehow? Is it worth it?
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Union(
    TypeHandle type1, TypeHandle type2, Region* region) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return Config::from_bitset(type1->AsBitset() | type2->AsBitset(), region);
  }

  // Fast case: top or bottom types.
  if (type1->IsAny()) return type1;
  if (type2->IsAny()) return type2;
  if (type1->IsNone()) return type2;
  if (type2->IsNone()) return type1;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type2;
    if (type2->Is(type1)) return type1;
  }

  // Slow case: may need to produce a Unioned object.
  int size = type1->IsBitset() || type2->IsBitset() ? 1 : 0;
  if (!type1->IsBitset()) {
    size += (type1->IsUnion() ? Config::union_length(type1->AsUnion()) : 1);
  }
  if (!type2->IsBitset()) {
    size += (type2->IsUnion() ? Config::union_length(type2->AsUnion()) : 1);
  }
  ASSERT(size >= 2);
  UnionedHandle unioned = Config::union_create(size, region);
  size = 0;

  int bitset = type1->GlbBitset() | type2->GlbBitset();
  if (bitset != kNone) {
    Config::union_set(unioned, size++, Config::from_bitset(bitset, region));
  }
  size = ExtendUnion(unioned, type1, size);
  size = ExtendUnion(unioned, type2, size);

  if (size == 1) {
    return Config::union_get(unioned, 0);
  } else {
    Config::union_shrink(unioned, size);
    return Config::from_union(unioned);
  }
}


// Get non-bitsets from type which are also in other, store at unioned,
// starting at index. Returns updated index.
template<class Config>
int TypeImpl<Config>::ExtendIntersection(
    UnionedHandle result, TypeHandle type, TypeHandle other, int current_size) {
  int old_size = current_size;
  if (type->IsClass() || type->IsConstant()) {
    if (type->Is(other) && !type->InUnion(result, old_size)) {
      Config::union_set(result, current_size++, type);
    }
  } else if (type->IsUnion()) {
    UnionedHandle unioned = type->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle type = Config::union_get(unioned, i);
      ASSERT(i == 0 ||
             !(type->IsBitset() || type->Is(Config::union_get(unioned, 0))));
      if (!type->IsBitset() && type->Is(other) &&
          !type->InUnion(result, old_size)) {
        Config::union_set(result, current_size++, type);
      }
    }
  }
  return current_size;
}


// Intersection is O(1) on simple bit unions, but O(n*m) on structured unions.
// TODO(rossberg): Should we use object sets somehow? Is it worth it?
template<class Config>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Intersect(
    TypeHandle type1, TypeHandle type2, Region* region) {
  // Fast case: bit sets.
  if (type1->IsBitset() && type2->IsBitset()) {
    return Config::from_bitset(type1->AsBitset() & type2->AsBitset(), region);
  }

  // Fast case: top or bottom types.
  if (type1->IsNone()) return type1;
  if (type2->IsNone()) return type2;
  if (type1->IsAny()) return type2;
  if (type2->IsAny()) return type1;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->IsUnion() || type2->IsUnion())) {
    if (type1->Is(type2)) return type1;
    if (type2->Is(type1)) return type2;
  }

  // Slow case: may need to produce a Unioned object.
  int size = 0;
  if (!type1->IsBitset()) {
    size = (type1->IsUnion() ? Config::union_length(type1->AsUnion()) : 2);
  }
  if (!type2->IsBitset()) {
    int size2 = (type2->IsUnion() ? Config::union_length(type2->AsUnion()) : 2);
    size = (size == 0 ? size2 : Min(size, size2));
  }
  ASSERT(size >= 2);
  UnionedHandle unioned = Config::union_create(size, region);
  size = 0;

  int bitset = type1->GlbBitset() & type2->GlbBitset();
  if (bitset != kNone) {
    Config::union_set(unioned, size++, Config::from_bitset(bitset, region));
  }
  size = ExtendIntersection(unioned, type1, type2, size);
  size = ExtendIntersection(unioned, type2, type1, size);

  if (size == 0) {
    return None(region);
  } else if (size == 1) {
    return Config::union_get(unioned, 0);
  } else {
    Config::union_shrink(unioned, size);
    return Config::from_union(unioned);
  }
}


template<class Config>
template<class OtherType>
typename TypeImpl<Config>::TypeHandle TypeImpl<Config>::Convert(
    typename OtherType::TypeHandle type, Region* region) {
  if (type->IsBitset()) {
    return Config::from_bitset(type->AsBitset(), region);
  } else if (type->IsClass()) {
    return Config::from_class(type->AsClass(), region);
  } else if (type->IsConstant()) {
    return Config::from_constant(type->AsConstant(), region);
  } else {
    ASSERT(type->IsUnion());
    typename OtherType::UnionedHandle unioned = type->AsUnion();
    int length = OtherType::UnionLength(unioned);
    UnionedHandle new_unioned = Config::union_create(length, region);
    for (int i = 0; i < length; ++i) {
      Config::union_set(new_unioned, i,
          Convert<OtherType>(OtherType::UnionGet(unioned, i), region));
    }
    return Config::from_union(new_unioned);
  }
}


// TODO(rossberg): this does not belong here.
Representation Representation::FromType(Type* type) {
  if (type->Is(Type::None())) return Representation::None();
  if (type->Is(Type::Smi())) return Representation::Smi();
  if (type->Is(Type::Signed32())) return Representation::Integer32();
  if (type->Is(Type::Number())) return Representation::Double();
  return Representation::Tagged();
}


#ifdef OBJECT_PRINT
template<class Config>
void TypeImpl<Config>::TypePrint() {
  TypePrint(stdout);
  PrintF(stdout, "\n");
  Flush(stdout);
}


template<class Config>
const char* TypeImpl<Config>::bitset_name(int bitset) {
  switch (bitset) {
    #define PRINT_COMPOSED_TYPE(type, value) case k##type: return #type;
    BITSET_TYPE_LIST(PRINT_COMPOSED_TYPE)
    #undef PRINT_COMPOSED_TYPE
    default:
      return NULL;
  }
}


template<class Config>
void TypeImpl<Config>::TypePrint(FILE* out) {
  if (this->IsBitset()) {
    int bitset = this->AsBitset();
    const char* name = bitset_name(bitset);
    if (name != NULL) {
      PrintF(out, "%s", name);
    } else {
      bool is_first = true;
      PrintF(out, "(");
      for (int mask = 1; mask != 0; mask = mask << 1) {
        if ((bitset & mask) != 0) {
          if (!is_first) PrintF(out, " | ");
          is_first = false;
          PrintF(out, "%s", bitset_name(mask));
        }
      }
      PrintF(out, ")");
    }
  } else if (this->IsConstant()) {
    PrintF(out, "Constant(%p : ", static_cast<void*>(*this->AsConstant()));
    Config::from_bitset(this->LubBitset())->TypePrint(out);
    PrintF(")");
  } else if (this->IsClass()) {
    PrintF(out, "Class(%p < ", static_cast<void*>(*this->AsClass()));
    Config::from_bitset(this->LubBitset())->TypePrint(out);
    PrintF(")");
  } else if (this->IsUnion()) {
    PrintF(out, "(");
    UnionedHandle unioned = this->AsUnion();
    for (int i = 0; i < Config::union_length(unioned); ++i) {
      TypeHandle type_i = Config::union_get(unioned, i);
      if (i > 0) PrintF(out, " | ");
      type_i->TypePrint(out);
    }
    PrintF(out, ")");
  }
}
#endif


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
