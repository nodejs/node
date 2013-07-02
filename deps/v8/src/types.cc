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

namespace v8 {
namespace internal {

int Type::NumClasses() {
  if (is_class()) {
    return 1;
  } else if (is_union()) {
    Handle<Unioned> unioned = as_union();
    int result = 0;
    for (int i = 0; i < unioned->length(); ++i) {
      if (union_get(unioned, i)->is_class()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


int Type::NumConstants() {
  if (is_constant()) {
    return 1;
  } else if (is_union()) {
    Handle<Unioned> unioned = as_union();
    int result = 0;
    for (int i = 0; i < unioned->length(); ++i) {
      if (union_get(unioned, i)->is_constant()) ++result;
    }
    return result;
  } else {
    return 0;
  }
}


template<class T>
Handle<Type> Type::Iterator<T>::get_type() {
  ASSERT(!Done());
  return type_->is_union() ? union_get(type_->as_union(), index_) : type_;
}

template<>
Handle<Map> Type::Iterator<Map>::Current() {
  return get_type()->as_class();
}

template<>
Handle<v8::internal::Object> Type::Iterator<v8::internal::Object>::Current() {
  return get_type()->as_constant();
}


template<>
bool Type::Iterator<Map>::matches(Handle<Type> type) {
  return type->is_class();
}

template<>
bool Type::Iterator<v8::internal::Object>::matches(Handle<Type> type) {
  return type->is_constant();
}


template<class T>
void Type::Iterator<T>::Advance() {
  ++index_;
  if (type_->is_union()) {
    Handle<Unioned> unioned = type_->as_union();
    for (; index_ < unioned->length(); ++index_) {
      if (matches(union_get(unioned, index_))) return;
    }
  } else if (index_ == 0 && matches(type_)) {
    return;
  }
  index_ = -1;
}

template class Type::Iterator<Map>;
template class Type::Iterator<v8::internal::Object>;


// Get the smallest bitset subsuming this type.
int Type::LubBitset() {
  if (this->is_bitset()) {
    return this->as_bitset();
  } else if (this->is_union()) {
    Handle<Unioned> unioned = this->as_union();
    int bitset = kNone;
    for (int i = 0; i < unioned->length(); ++i) {
      bitset |= union_get(unioned, i)->LubBitset();
    }
    return bitset;
  } else {
    Map* map = NULL;
    if (this->is_class()) {
      map = *this->as_class();
    } else {
      Handle<v8::internal::Object> value = this->as_constant();
      if (value->IsSmi()) return kSmi;
      map = HeapObject::cast(*value)->map();
      if (map->instance_type() == ODDBALL_TYPE) {
        if (value->IsUndefined()) return kUndefined;
        if (value->IsNull()) return kNull;
        if (value->IsTrue() || value->IsFalse()) return kBoolean;
        if (value->IsTheHole()) return kAny;
      }
    }
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
        return kInternal;
      default:
        UNREACHABLE();
        return kNone;
    }
  }
}


// Get the largest bitset subsumed by this type.
int Type::GlbBitset() {
  if (this->is_bitset()) {
    return this->as_bitset();
  } else if (this->is_union()) {
    // All but the first are non-bitsets and thus would yield kNone anyway.
    return union_get(this->as_union(), 0)->GlbBitset();
  } else {
    return kNone;
  }
}


// Check this <= that.
bool Type::IsSlowCase(Type* that) {
  // Fast path for bitsets.
  if (that->is_bitset()) {
    return (this->LubBitset() | that->as_bitset()) == that->as_bitset();
  }

  if (that->is_class()) {
    return this->is_class() && *this->as_class() == *that->as_class();
  }
  if (that->is_constant()) {
    return this->is_constant() && *this->as_constant() == *that->as_constant();
  }

  // (T1 \/ ... \/ Tn) <= T  <=>  (T1 <= T) /\ ... /\ (Tn <= T)
  if (this->is_union()) {
    Handle<Unioned> unioned = this->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> this_i = union_get(unioned, i);
      if (!this_i->Is(that)) return false;
    }
    return true;
  }

  // T <= (T1 \/ ... \/ Tn)  <=>  (T <= T1) \/ ... \/ (T <= Tn)
  // (iff T is not a union)
  ASSERT(!this->is_union());
  if (that->is_union()) {
    Handle<Unioned> unioned = that->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> that_i = union_get(unioned, i);
      if (this->Is(that_i)) return true;
      if (this->is_bitset()) break;  // Fast fail, no other field is a bitset.
    }
    return false;
  }

  return false;
}


// Check this overlaps that.
bool Type::Maybe(Type* that) {
  // Fast path for bitsets.
  if (this->is_bitset()) {
    return (this->as_bitset() & that->LubBitset()) != 0;
  }
  if (that->is_bitset()) {
    return (this->LubBitset() & that->as_bitset()) != 0;
  }

  // (T1 \/ ... \/ Tn) overlaps T <=> (T1 overlaps T) \/ ... \/ (Tn overlaps T)
  if (this->is_union()) {
    Handle<Unioned> unioned = this->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> this_i = union_get(unioned, i);
      if (this_i->Maybe(that)) return true;
    }
    return false;
  }

  // T overlaps (T1 \/ ... \/ Tn) <=> (T overlaps T1) \/ ... \/ (T overlaps Tn)
  if (that->is_union()) {
    Handle<Unioned> unioned = that->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> that_i = union_get(unioned, i);
      if (this->Maybe(that_i)) return true;
    }
    return false;
  }

  ASSERT(!that->is_union());
  if (this->is_class()) {
    return that->is_class() && *this->as_class() == *that->as_class();
  }
  if (this->is_constant()) {
    return that->is_constant() && *this->as_constant() == *that->as_constant();
  }

  return false;
}


bool Type::InUnion(Handle<Unioned> unioned, int current_size) {
  ASSERT(!this->is_union());
  for (int i = 0; i < current_size; ++i) {
    Handle<Type> type = union_get(unioned, i);
    if (this->Is(type)) return true;
  }
  return false;
}

// Get non-bitsets from this which are not subsumed by union, store at unioned,
// starting at index. Returns updated index.
int Type::ExtendUnion(Handle<Unioned> result, int current_size) {
  int old_size = current_size;
  if (this->is_class() || this->is_constant()) {
    if (!this->InUnion(result, old_size)) result->set(current_size++, this);
  } else if (this->is_union()) {
    Handle<Unioned> unioned = this->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> type = union_get(unioned, i);
      ASSERT(i == 0 || !(type->is_bitset() || type->Is(union_get(unioned, 0))));
      if (type->is_bitset()) continue;
      if (!type->InUnion(result, old_size)) result->set(current_size++, *type);
    }
  }
  return current_size;
}


// Union is O(1) on simple bit unions, but O(n*m) on structured unions.
// TODO(rossberg): Should we use object sets somehow? Is it worth it?
Type* Type::Union(Handle<Type> type1, Handle<Type> type2) {
  // Fast case: bit sets.
  if (type1->is_bitset() && type2->is_bitset()) {
    return from_bitset(type1->as_bitset() | type2->as_bitset());
  }

  // Fast case: top or bottom types.
  if (type1->SameValue(Type::Any())) return *type1;
  if (type2->SameValue(Type::Any())) return *type2;
  if (type1->SameValue(Type::None())) return *type2;
  if (type2->SameValue(Type::None())) return *type1;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->is_union() || type2->is_union())) {
    if (type1->Is(type2)) return *type2;
    if (type2->Is(type1)) return *type1;
  }

  // Slow case: may need to produce a Unioned object.
  Isolate* isolate = NULL;
  int size = type1->is_bitset() || type2->is_bitset() ? 1 : 0;
  if (!type1->is_bitset()) {
    isolate = HeapObject::cast(*type1)->GetIsolate();
    size += (type1->is_union() ? type1->as_union()->length() : 1);
  }
  if (!type2->is_bitset()) {
    isolate = HeapObject::cast(*type2)->GetIsolate();
    size += (type2->is_union() ? type2->as_union()->length() : 1);
  }
  ASSERT(isolate != NULL);
  ASSERT(size >= 2);
  Handle<Unioned> unioned = isolate->factory()->NewFixedArray(size);
  size = 0;

  int bitset = type1->GlbBitset() | type2->GlbBitset();
  if (bitset != kNone) unioned->set(size++, from_bitset(bitset));
  size = type1->ExtendUnion(unioned, size);
  size = type2->ExtendUnion(unioned, size);

  if (size == 1) {
    return *union_get(unioned, 0);
  } else if (size == unioned->length()) {
    return from_handle(unioned);
  }

  // There was an overlap. Copy to smaller union.
  Handle<Unioned> result = isolate->factory()->NewFixedArray(size);
  for (int i = 0; i < size; ++i) result->set(i, unioned->get(i));
  return from_handle(result);
}


// Get non-bitsets from this which are also in that, store at unioned,
// starting at index. Returns updated index.
int Type::ExtendIntersection(
    Handle<Unioned> result, Handle<Type> that, int current_size) {
  int old_size = current_size;
  if (this->is_class() || this->is_constant()) {
    if (this->Is(that) && !this->InUnion(result, old_size))
      result->set(current_size++, this);
  } else if (this->is_union()) {
    Handle<Unioned> unioned = this->as_union();
    for (int i = 0; i < unioned->length(); ++i) {
      Handle<Type> type = union_get(unioned, i);
      ASSERT(i == 0 || !(type->is_bitset() || type->Is(union_get(unioned, 0))));
      if (type->is_bitset()) continue;
      if (type->Is(that) && !type->InUnion(result, old_size))
        result->set(current_size++, *type);
    }
  }
  return current_size;
}


// Intersection is O(1) on simple bit unions, but O(n*m) on structured unions.
// TODO(rossberg): Should we use object sets somehow? Is it worth it?
Type* Type::Intersect(Handle<Type> type1, Handle<Type> type2) {
  // Fast case: bit sets.
  if (type1->is_bitset() && type2->is_bitset()) {
    return from_bitset(type1->as_bitset() & type2->as_bitset());
  }

  // Fast case: top or bottom types.
  if (type1->SameValue(Type::None())) return *type1;
  if (type2->SameValue(Type::None())) return *type2;
  if (type1->SameValue(Type::Any())) return *type2;
  if (type2->SameValue(Type::Any())) return *type1;

  // Semi-fast case: Unioned objects are neither involved nor produced.
  if (!(type1->is_union() || type2->is_union())) {
    if (type1->Is(type2)) return *type1;
    if (type2->Is(type1)) return *type2;
  }

  // Slow case: may need to produce a Unioned object.
  Isolate* isolate = NULL;
  int size = 0;
  if (!type1->is_bitset()) {
    isolate = HeapObject::cast(*type1)->GetIsolate();
    size = (type1->is_union() ? type1->as_union()->length() : 2);
  }
  if (!type2->is_bitset()) {
    isolate = HeapObject::cast(*type2)->GetIsolate();
    int size2 = (type2->is_union() ? type2->as_union()->length() : 2);
    size = (size == 0 ? size2 : Min(size, size2));
  }
  ASSERT(isolate != NULL);
  ASSERT(size >= 2);
  Handle<Unioned> unioned = isolate->factory()->NewFixedArray(size);
  size = 0;

  int bitset = type1->GlbBitset() & type2->GlbBitset();
  if (bitset != kNone) unioned->set(size++, from_bitset(bitset));
  size = type1->ExtendIntersection(unioned, type2, size);
  size = type2->ExtendIntersection(unioned, type1, size);

  if (size == 0) {
    return None();
  } else if (size == 1) {
    return *union_get(unioned, 0);
  } else if (size == unioned->length()) {
    return from_handle(unioned);
  }

  // There were dropped cases. Copy to smaller union.
  Handle<Unioned> result = isolate->factory()->NewFixedArray(size);
  for (int i = 0; i < size; ++i) result->set(i, unioned->get(i));
  return from_handle(result);
}


Type* Type::Optional(Handle<Type> type) {
  return type->is_bitset()
      ? from_bitset(type->as_bitset() | kUndefined)
      : Union(type, Undefined()->handle_via_isolate_of(*type));
}

} }  // namespace v8::internal
