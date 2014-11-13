// Copyright 2014 the V8 project authors. All rights reserved.
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

#ifndef V8_TEST_CCTEST_TYPES_H_
#define V8_TEST_CCTEST_TYPES_H_

#include "src/v8.h"

namespace v8 {
namespace internal {


template<class Type, class TypeHandle, class Region>
class Types {
 public:
  Types(Region* region, Isolate* isolate)
      : region_(region), rng_(isolate->random_number_generator()) {
    #define DECLARE_TYPE(name, value) \
      name = Type::name(region); \
      if (SmiValuesAre31Bits() || \
          (!Type::name(region)->Equals(Type::OtherSigned32()) && \
           !Type::name(region)->Equals(Type::OtherUnsigned31()))) { \
        /* Hack: Avoid generating those empty bitset types. */ \
        types.push_back(name); \
      }
    PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE

    object_map = isolate->factory()->NewMap(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array_map = isolate->factory()->NewMap(
        JS_ARRAY_TYPE, JSArray::kSize);
    number_map = isolate->factory()->NewMap(
        HEAP_NUMBER_TYPE, HeapNumber::kSize);
    uninitialized_map = isolate->factory()->uninitialized_map();
    ObjectClass = Type::Class(object_map, region);
    ArrayClass = Type::Class(array_map, region);
    NumberClass = Type::Class(number_map, region);
    UninitializedClass = Type::Class(uninitialized_map, region);

    maps.push_back(object_map);
    maps.push_back(array_map);
    maps.push_back(uninitialized_map);
    for (MapVector::iterator it = maps.begin(); it != maps.end(); ++it) {
      types.push_back(Type::Class(*it, region));
    }

    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    uninitialized = isolate->factory()->uninitialized_value();
    SmiConstant = Type::Constant(smi, region);
    Signed32Constant = Type::Constant(signed32, region);
    ObjectConstant1 = Type::Constant(object1, region);
    ObjectConstant2 = Type::Constant(object2, region);
    ArrayConstant = Type::Constant(array, region);
    UninitializedConstant = Type::Constant(uninitialized, region);

    values.push_back(smi);
    values.push_back(signed32);
    values.push_back(object1);
    values.push_back(object2);
    values.push_back(array);
    values.push_back(uninitialized);
    for (ValueVector::iterator it = values.begin(); it != values.end(); ++it) {
      types.push_back(Type::Constant(*it, region));
    }

    integers.push_back(isolate->factory()->NewNumber(-V8_INFINITY));
    integers.push_back(isolate->factory()->NewNumber(+V8_INFINITY));
    integers.push_back(isolate->factory()->NewNumber(-rng_->NextInt(10)));
    integers.push_back(isolate->factory()->NewNumber(+rng_->NextInt(10)));
    for (int i = 0; i < 10; ++i) {
      double x = rng_->NextInt();
      integers.push_back(isolate->factory()->NewNumber(x));
      x *= rng_->NextInt();
      if (!IsMinusZero(x)) integers.push_back(isolate->factory()->NewNumber(x));
    }

    Integer = Type::Range(isolate->factory()->NewNumber(-V8_INFINITY),
                          isolate->factory()->NewNumber(+V8_INFINITY), region);

    NumberArray = Type::Array(Number, region);
    StringArray = Type::Array(String, region);
    AnyArray = Type::Array(Any, region);

    SignedFunction1 = Type::Function(SignedSmall, SignedSmall, region);
    NumberFunction1 = Type::Function(Number, Number, region);
    NumberFunction2 = Type::Function(Number, Number, Number, region);
    MethodFunction = Type::Function(String, Object, 0, region);

    for (int i = 0; i < 30; ++i) {
      types.push_back(Fuzz());
    }
  }

  Handle<i::Map> object_map;
  Handle<i::Map> array_map;
  Handle<i::Map> number_map;
  Handle<i::Map> uninitialized_map;

  Handle<i::Smi> smi;
  Handle<i::HeapNumber> signed32;
  Handle<i::JSObject> object1;
  Handle<i::JSObject> object2;
  Handle<i::JSArray> array;
  Handle<i::Oddball> uninitialized;

  #define DECLARE_TYPE(name, value) TypeHandle name;
  BITSET_TYPE_LIST(DECLARE_TYPE)
  #undef DECLARE_TYPE

  TypeHandle ObjectClass;
  TypeHandle ArrayClass;
  TypeHandle NumberClass;
  TypeHandle UninitializedClass;

  TypeHandle SmiConstant;
  TypeHandle Signed32Constant;
  TypeHandle ObjectConstant1;
  TypeHandle ObjectConstant2;
  TypeHandle ArrayConstant;
  TypeHandle UninitializedConstant;

  TypeHandle Integer;

  TypeHandle NumberArray;
  TypeHandle StringArray;
  TypeHandle AnyArray;

  TypeHandle SignedFunction1;
  TypeHandle NumberFunction1;
  TypeHandle NumberFunction2;
  TypeHandle MethodFunction;

  typedef std::vector<TypeHandle> TypeVector;
  typedef std::vector<Handle<i::Map> > MapVector;
  typedef std::vector<Handle<i::Object> > ValueVector;

  TypeVector types;
  MapVector maps;
  ValueVector values;
  ValueVector integers;  // "Integer" values used for range limits.

  TypeHandle Of(Handle<i::Object> value) {
    return Type::Of(value, region_);
  }

  TypeHandle NowOf(Handle<i::Object> value) {
    return Type::NowOf(value, region_);
  }

  TypeHandle Class(Handle<i::Map> map) {
    return Type::Class(map, region_);
  }

  TypeHandle Constant(Handle<i::Object> value) {
    return Type::Constant(value, region_);
  }

  TypeHandle Range(Handle<i::Object> min, Handle<i::Object> max) {
    return Type::Range(min, max, region_);
  }

  TypeHandle Context(TypeHandle outer) {
    return Type::Context(outer, region_);
  }

  TypeHandle Array1(TypeHandle element) {
    return Type::Array(element, region_);
  }

  TypeHandle Function0(TypeHandle result, TypeHandle receiver) {
    return Type::Function(result, receiver, 0, region_);
  }

  TypeHandle Function1(TypeHandle result, TypeHandle receiver, TypeHandle arg) {
    TypeHandle type = Type::Function(result, receiver, 1, region_);
    type->AsFunction()->InitParameter(0, arg);
    return type;
  }

  TypeHandle Function2(TypeHandle result, TypeHandle arg1, TypeHandle arg2) {
    return Type::Function(result, arg1, arg2, region_);
  }

  TypeHandle Union(TypeHandle t1, TypeHandle t2) {
    return Type::Union(t1, t2, region_);
  }
  TypeHandle Intersect(TypeHandle t1, TypeHandle t2) {
    return Type::Intersect(t1, t2, region_);
  }

  template<class Type2, class TypeHandle2>
  TypeHandle Convert(TypeHandle2 t) {
    return Type::template Convert<Type2>(t, region_);
  }

  TypeHandle Random() {
    return types[rng_->NextInt(static_cast<int>(types.size()))];
  }

  TypeHandle Fuzz(int depth = 4) {
    switch (rng_->NextInt(depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
        #define COUNT_BITSET_TYPES(type, value) + 1
        int n = 0 PROPER_BITSET_TYPE_LIST(COUNT_BITSET_TYPES);
        #undef COUNT_BITSET_TYPES
        // Pick a bunch of named bitsets and return their intersection.
        TypeHandle result = Type::Any(region_);
        for (int i = 0, m = 1 + rng_->NextInt(3); i < m; ++i) {
          int j = rng_->NextInt(n);
          #define PICK_BITSET_TYPE(type, value) \
            if (j-- == 0) { \
              if (!SmiValuesAre31Bits() && \
                  (Type::type(region_)->Equals(Type::OtherSigned32()) || \
                   Type::type(region_)->Equals(Type::OtherUnsigned31()))) { \
                /* Hack: Avoid generating those empty bitset types. */ \
                continue; \
              } \
              TypeHandle tmp = Type::Intersect( \
                  result, Type::type(region_), region_); \
              if (tmp->Is(Type::None()) && i != 0) { \
                break; \
              } else { \
                result = tmp; \
                continue; \
              } \
            }
          PROPER_BITSET_TYPE_LIST(PICK_BITSET_TYPE)
          #undef PICK_BITSET_TYPE
        }
        return result;
      }
      case 1: {  // class
        int i = rng_->NextInt(static_cast<int>(maps.size()));
        return Type::Class(maps[i], region_);
      }
      case 2: {  // constant
        int i = rng_->NextInt(static_cast<int>(values.size()));
        return Type::Constant(values[i], region_);
      }
      case 3: {  // range
        int i = rng_->NextInt(static_cast<int>(integers.size()));
        int j = rng_->NextInt(static_cast<int>(integers.size()));
        i::Handle<i::Object> min = integers[i];
        i::Handle<i::Object> max = integers[j];
        if (min->Number() > max->Number()) std::swap(min, max);
        return Type::Range(min, max, region_);
      }
      case 4: {  // context
        int depth = rng_->NextInt(3);
        TypeHandle type = Type::Internal(region_);
        for (int i = 0; i < depth; ++i) type = Type::Context(type, region_);
        return type;
      }
      case 5: {  // array
        TypeHandle element = Fuzz(depth / 2);
        return Type::Array(element, region_);
      }
      case 6:
      case 7: {  // function
        TypeHandle result = Fuzz(depth / 2);
        TypeHandle receiver = Fuzz(depth / 2);
        int arity = rng_->NextInt(3);
        TypeHandle type = Type::Function(result, receiver, arity, region_);
        for (int i = 0; i < type->AsFunction()->Arity(); ++i) {
          TypeHandle parameter = Fuzz(depth / 2);
          type->AsFunction()->InitParameter(i, parameter);
        }
        return type;
      }
      default: {  // union
        int n = rng_->NextInt(10);
        TypeHandle type = None;
        for (int i = 0; i < n; ++i) {
          TypeHandle operand = Fuzz(depth - 1);
          type = Type::Union(type, operand, region_);
        }
        return type;
      }
    }
    UNREACHABLE();
  }

  Region* region() { return region_; }

 private:
  Region* region_;
  v8::base::RandomNumberGenerator* rng_;
};


} }  // namespace v8::internal

#endif
