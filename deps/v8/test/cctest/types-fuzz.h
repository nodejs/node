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

#include "src/base/utils/random-number-generator.h"
#include "src/v8.h"

namespace v8 {
namespace internal {


class Types {
 public:
  Types(Zone* zone, Isolate* isolate, v8::base::RandomNumberGenerator* rng)
      : zone_(zone), isolate_(isolate), rng_(rng) {
#define DECLARE_TYPE(name, value) \
  name = Type::name();            \
  types.push_back(name);
    PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
    #undef DECLARE_TYPE

    SignedSmall = Type::SignedSmall();
    UnsignedSmall = Type::UnsignedSmall();

    object_map = isolate->factory()->NewMap(
        JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array_map = isolate->factory()->NewMap(
        JS_ARRAY_TYPE, JSArray::kSize);
    number_map = isolate->factory()->NewMap(
        HEAP_NUMBER_TYPE, HeapNumber::kSize);
    uninitialized_map = isolate->factory()->uninitialized_map();
    ObjectClass = Type::Class(object_map, zone);
    ArrayClass = Type::Class(array_map, zone);
    NumberClass = Type::Class(number_map, zone);
    UninitializedClass = Type::Class(uninitialized_map, zone);

    maps.push_back(object_map);
    maps.push_back(array_map);
    maps.push_back(uninitialized_map);
    for (MapVector::iterator it = maps.begin(); it != maps.end(); ++it) {
      types.push_back(Type::Class(*it, zone));
    }

    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    uninitialized = isolate->factory()->uninitialized_value();
    SmiConstant = Type::Constant(smi, zone);
    Signed32Constant = Type::Constant(signed32, zone);

    ObjectConstant1 = Type::Constant(object1, zone);
    ObjectConstant2 = Type::Constant(object2, zone);
    ArrayConstant = Type::Constant(array, zone);
    UninitializedConstant = Type::Constant(uninitialized, zone);

    values.push_back(smi);
    values.push_back(signed32);
    values.push_back(object1);
    values.push_back(object2);
    values.push_back(array);
    values.push_back(uninitialized);
    for (ValueVector::iterator it = values.begin(); it != values.end(); ++it) {
      types.push_back(Type::Constant(*it, zone));
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

    Integer = Type::Range(-V8_INFINITY, +V8_INFINITY, zone);

    NumberArray = Type::Array(Number, zone);
    StringArray = Type::Array(String, zone);
    AnyArray = Type::Array(Any, zone);

    SignedFunction1 = Type::Function(SignedSmall, SignedSmall, zone);
    NumberFunction1 = Type::Function(Number, Number, zone);
    NumberFunction2 = Type::Function(Number, Number, Number, zone);
    MethodFunction = Type::Function(String, Object, 0, zone);

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

#define DECLARE_TYPE(name, value) Type* name;
  PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
  #undef DECLARE_TYPE

#define DECLARE_TYPE(name, value) Type* Mask##name##ForTesting;
  MASK_BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE
  Type* SignedSmall;
  Type* UnsignedSmall;

  Type* ObjectClass;
  Type* ArrayClass;
  Type* NumberClass;
  Type* UninitializedClass;

  Type* SmiConstant;
  Type* Signed32Constant;
  Type* ObjectConstant1;
  Type* ObjectConstant2;
  Type* ArrayConstant;
  Type* UninitializedConstant;

  Type* Integer;

  Type* NumberArray;
  Type* StringArray;
  Type* AnyArray;

  Type* SignedFunction1;
  Type* NumberFunction1;
  Type* NumberFunction2;
  Type* MethodFunction;

  typedef std::vector<Type*> TypeVector;
  typedef std::vector<Handle<i::Map> > MapVector;
  typedef std::vector<Handle<i::Object> > ValueVector;

  TypeVector types;
  MapVector maps;
  ValueVector values;
  ValueVector integers;  // "Integer" values used for range limits.

  Type* Of(Handle<i::Object> value) { return Type::Of(value, zone_); }

  Type* NowOf(Handle<i::Object> value) { return Type::NowOf(value, zone_); }

  Type* Class(Handle<i::Map> map) { return Type::Class(map, zone_); }

  Type* Constant(Handle<i::Object> value) {
    return Type::Constant(value, zone_);
  }

  Type* Range(double min, double max) { return Type::Range(min, max, zone_); }

  Type* Context(Type* outer) { return Type::Context(outer, zone_); }

  Type* Array1(Type* element) { return Type::Array(element, zone_); }

  Type* Function0(Type* result, Type* receiver) {
    return Type::Function(result, receiver, 0, zone_);
  }

  Type* Function1(Type* result, Type* receiver, Type* arg) {
    Type* type = Type::Function(result, receiver, 1, zone_);
    type->AsFunction()->InitParameter(0, arg);
    return type;
  }

  Type* Function2(Type* result, Type* arg1, Type* arg2) {
    return Type::Function(result, arg1, arg2, zone_);
  }

  Type* Union(Type* t1, Type* t2) { return Type::Union(t1, t2, zone_); }

  Type* Intersect(Type* t1, Type* t2) { return Type::Intersect(t1, t2, zone_); }

  Type* Representation(Type* t) { return Type::Representation(t, zone_); }

  Type* Semantic(Type* t) { return Type::Semantic(t, zone_); }

  Type* Random() {
    return types[rng_->NextInt(static_cast<int>(types.size()))];
  }

  Type* Fuzz(int depth = 4) {
    switch (rng_->NextInt(depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
        #define COUNT_BITSET_TYPES(type, value) + 1
        int n = 0 PROPER_BITSET_TYPE_LIST(COUNT_BITSET_TYPES);
        #undef COUNT_BITSET_TYPES
        // Pick a bunch of named bitsets and return their intersection.
        Type* result = Type::Any();
        for (int i = 0, m = 1 + rng_->NextInt(3); i < m; ++i) {
          int j = rng_->NextInt(n);
#define PICK_BITSET_TYPE(type, value)                         \
  if (j-- == 0) {                                             \
    Type* tmp = Type::Intersect(result, Type::type(), zone_); \
    if (tmp->Is(Type::None()) && i != 0) {                    \
      break;                                                  \
    } else {                                                  \
      result = tmp;                                           \
      continue;                                               \
    }                                                         \
  }
          PROPER_BITSET_TYPE_LIST(PICK_BITSET_TYPE)
          #undef PICK_BITSET_TYPE
        }
        return result;
      }
      case 1: {  // class
        int i = rng_->NextInt(static_cast<int>(maps.size()));
        return Type::Class(maps[i], zone_);
      }
      case 2: {  // constant
        int i = rng_->NextInt(static_cast<int>(values.size()));
        return Type::Constant(values[i], zone_);
      }
      case 3: {  // range
        int i = rng_->NextInt(static_cast<int>(integers.size()));
        int j = rng_->NextInt(static_cast<int>(integers.size()));
        double min = integers[i]->Number();
        double max = integers[j]->Number();
        if (min > max) std::swap(min, max);
        return Type::Range(min, max, zone_);
      }
      case 4: {  // context
        int depth = rng_->NextInt(3);
        Type* type = Type::Internal();
        for (int i = 0; i < depth; ++i) type = Type::Context(type, zone_);
        return type;
      }
      case 5: {  // array
        Type* element = Fuzz(depth / 2);
        return Type::Array(element, zone_);
      }
      case 6:
      case 7: {  // function
        Type* result = Fuzz(depth / 2);
        Type* receiver = Fuzz(depth / 2);
        int arity = rng_->NextInt(3);
        Type* type = Type::Function(result, receiver, arity, zone_);
        for (int i = 0; i < type->AsFunction()->Arity(); ++i) {
          Type* parameter = Fuzz(depth / 2);
          type->AsFunction()->InitParameter(i, parameter);
        }
        return type;
      }
      case 8: {  // simd
        static const int num_simd_types =
            #define COUNT_SIMD_TYPE(NAME, Name, name, lane_count, lane_type) +1
            SIMD128_TYPES(COUNT_SIMD_TYPE);
            #undef COUNT_SIMD_TYPE
        Type* (*simd_constructors[num_simd_types])(Isolate*, Zone*) = {
          #define COUNT_SIMD_TYPE(NAME, Name, name, lane_count, lane_type) \
          &Type::Name,
            SIMD128_TYPES(COUNT_SIMD_TYPE)
          #undef COUNT_SIMD_TYPE
        };
        return simd_constructors[rng_->NextInt(num_simd_types)](isolate_,
                                                                zone_);
      }
      default: {  // union
        int n = rng_->NextInt(10);
        Type* type = None;
        for (int i = 0; i < n; ++i) {
          Type* operand = Fuzz(depth - 1);
          type = Type::Union(type, operand, zone_);
        }
        return type;
      }
    }
    UNREACHABLE();
  }

  Zone* zone() { return zone_; }

 private:
  Zone* zone_;
  Isolate* isolate_;
  v8::base::RandomNumberGenerator* rng_;
};


}  // namespace internal
}  // namespace v8

#endif
