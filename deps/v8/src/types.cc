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
      v8::internal::Object* value = this->as_constant()->value();
      if (value->IsSmi()) return kSmi;
      map = HeapObject::cast(value)->map();
    }
    switch (map->instance_type()) {
      case STRING_TYPE:
      case ASCII_STRING_TYPE:
      case CONS_STRING_TYPE:
      case CONS_ASCII_STRING_TYPE:
      case SLICED_STRING_TYPE:
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
      case JS_WEAK_MAP_TYPE:
      case JS_REGEXP_TYPE:
        return kOtherObject;
      case JS_ARRAY_TYPE:
        return kArray;
      case JS_FUNCTION_TYPE:
        return kFunction;
      case JS_PROXY_TYPE:
      case JS_FUNCTION_PROXY_TYPE:
        return kProxy;
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
bool Type::Is(Handle<Type> that) {
  // Fast path for bitsets.
  if (that->is_bitset()) {
    return (this->LubBitset() | that->as_bitset()) == that->as_bitset();
  }

  if (that->is_class()) {
    return this->is_class() && *this->as_class() == *that->as_class();
  }
  if (that->is_constant()) {
    return this->is_constant() &&
        this->as_constant()->value() == that->as_constant()->value();
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
bool Type::Maybe(Handle<Type> that) {
  // Fast path for bitsets.
  if (this->is_bitset()) {
    return (this->as_bitset() & that->LubBitset()) != 0;
  }
  if (that->is_bitset()) {
    return (this->LubBitset() & that->as_bitset()) != 0;
  }

  if (this->is_class()) {
    return that->is_class() && *this->as_class() == *that->as_class();
  }
  if (this->is_constant()) {
    return that->is_constant() &&
        this->as_constant()->value() == that->as_constant()->value();
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

  return false;
}


bool Type::InUnion(Handle<Unioned> unioned, int current_size) {
  ASSERT(!this->is_union());
  for (int i = 0; i < current_size; ++i) {
    Handle<Type> type = union_get(unioned, i);
    if (type->is_bitset() ? this->Is(type) : this == *type) return true;
  }
  return false;
}

// Get non-bitsets from this which are not subsumed by that, store at unioned,
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


Type* Type::Optional(Handle<Type> type) {
  return type->is_bitset()
      ? from_bitset(type->as_bitset() | kUndefined)
      : Union(type, Undefined()->handle_via_isolate_of(*type));
}

} }  // namespace v8::internal
