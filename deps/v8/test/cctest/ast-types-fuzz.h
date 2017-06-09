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
#include "src/factory.h"
#include "src/isolate.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class AstTypes {
 public:
  AstTypes(Zone* zone, Isolate* isolate, v8::base::RandomNumberGenerator* rng)
      : zone_(zone), isolate_(isolate), rng_(rng) {
#define DECLARE_TYPE(name, value) \
  name = AstType::name();         \
  types.push_back(name);
    AST_PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE

    SignedSmall = AstType::SignedSmall();
    UnsignedSmall = AstType::UnsignedSmall();

    object_map =
        isolate->factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    array_map = isolate->factory()->NewMap(JS_ARRAY_TYPE, JSArray::kSize);
    number_map =
        isolate->factory()->NewMap(HEAP_NUMBER_TYPE, HeapNumber::kSize);
    uninitialized_map = isolate->factory()->uninitialized_map();
    ObjectClass = AstType::Class(object_map, zone);
    ArrayClass = AstType::Class(array_map, zone);
    NumberClass = AstType::Class(number_map, zone);
    UninitializedClass = AstType::Class(uninitialized_map, zone);

    maps.push_back(object_map);
    maps.push_back(array_map);
    maps.push_back(uninitialized_map);
    for (MapVector::iterator it = maps.begin(); it != maps.end(); ++it) {
      types.push_back(AstType::Class(*it, zone));
    }

    smi = handle(Smi::FromInt(666), isolate);
    signed32 = isolate->factory()->NewHeapNumber(0x40000000);
    object1 = isolate->factory()->NewJSObjectFromMap(object_map);
    object2 = isolate->factory()->NewJSObjectFromMap(object_map);
    array = isolate->factory()->NewJSArray(20);
    uninitialized = isolate->factory()->uninitialized_value();
    SmiConstant = AstType::Constant(smi, zone);
    Signed32Constant = AstType::Constant(signed32, zone);

    ObjectConstant1 = AstType::Constant(object1, zone);
    ObjectConstant2 = AstType::Constant(object2, zone);
    ArrayConstant = AstType::Constant(array, zone);
    UninitializedConstant = AstType::Constant(uninitialized, zone);

    values.push_back(smi);
    values.push_back(signed32);
    values.push_back(object1);
    values.push_back(object2);
    values.push_back(array);
    values.push_back(uninitialized);
    for (ValueVector::iterator it = values.begin(); it != values.end(); ++it) {
      types.push_back(AstType::Constant(*it, zone));
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

    Integer = AstType::Range(-V8_INFINITY, +V8_INFINITY, zone);

    NumberArray = AstType::Array(Number, zone);
    StringArray = AstType::Array(String, zone);
    AnyArray = AstType::Array(Any, zone);

    SignedFunction1 = AstType::Function(SignedSmall, SignedSmall, zone);
    NumberFunction1 = AstType::Function(Number, Number, zone);
    NumberFunction2 = AstType::Function(Number, Number, Number, zone);
    MethodFunction = AstType::Function(String, Object, 0, zone);

    for (int i = 0; i < 30; ++i) {
      types.push_back(Fuzz());
    }
    USE(isolate_);  // Currently unused.
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

#define DECLARE_TYPE(name, value) AstType* name;
  AST_PROPER_BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE

#define DECLARE_TYPE(name, value) AstType* Mask##name##ForTesting;
  AST_MASK_BITSET_TYPE_LIST(DECLARE_TYPE)
#undef DECLARE_TYPE
  AstType* SignedSmall;
  AstType* UnsignedSmall;

  AstType* ObjectClass;
  AstType* ArrayClass;
  AstType* NumberClass;
  AstType* UninitializedClass;

  AstType* SmiConstant;
  AstType* Signed32Constant;
  AstType* ObjectConstant1;
  AstType* ObjectConstant2;
  AstType* ArrayConstant;
  AstType* UninitializedConstant;

  AstType* Integer;

  AstType* NumberArray;
  AstType* StringArray;
  AstType* AnyArray;

  AstType* SignedFunction1;
  AstType* NumberFunction1;
  AstType* NumberFunction2;
  AstType* MethodFunction;

  typedef std::vector<AstType*> TypeVector;
  typedef std::vector<Handle<i::Map> > MapVector;
  typedef std::vector<Handle<i::Object> > ValueVector;

  TypeVector types;
  MapVector maps;
  ValueVector values;
  ValueVector integers;  // "Integer" values used for range limits.

  AstType* Of(Handle<i::Object> value) { return AstType::Of(value, zone_); }

  AstType* NowOf(Handle<i::Object> value) {
    return AstType::NowOf(value, zone_);
  }

  AstType* Class(Handle<i::Map> map) { return AstType::Class(map, zone_); }

  AstType* Constant(Handle<i::Object> value) {
    return AstType::Constant(value, zone_);
  }

  AstType* Range(double min, double max) {
    return AstType::Range(min, max, zone_);
  }

  AstType* Context(AstType* outer) { return AstType::Context(outer, zone_); }

  AstType* Array1(AstType* element) { return AstType::Array(element, zone_); }

  AstType* Function0(AstType* result, AstType* receiver) {
    return AstType::Function(result, receiver, 0, zone_);
  }

  AstType* Function1(AstType* result, AstType* receiver, AstType* arg) {
    AstType* type = AstType::Function(result, receiver, 1, zone_);
    type->AsFunction()->InitParameter(0, arg);
    return type;
  }

  AstType* Function2(AstType* result, AstType* arg1, AstType* arg2) {
    return AstType::Function(result, arg1, arg2, zone_);
  }

  AstType* Union(AstType* t1, AstType* t2) {
    return AstType::Union(t1, t2, zone_);
  }

  AstType* Intersect(AstType* t1, AstType* t2) {
    return AstType::Intersect(t1, t2, zone_);
  }

  AstType* Representation(AstType* t) {
    return AstType::Representation(t, zone_);
  }

  AstType* Semantic(AstType* t) { return AstType::Semantic(t, zone_); }

  AstType* Random() {
    return types[rng_->NextInt(static_cast<int>(types.size()))];
  }

  AstType* Fuzz(int depth = 4) {
    switch (rng_->NextInt(depth == 0 ? 3 : 20)) {
      case 0: {  // bitset
#define COUNT_BITSET_TYPES(type, value) +1
        int n = 0 AST_PROPER_BITSET_TYPE_LIST(COUNT_BITSET_TYPES);
#undef COUNT_BITSET_TYPES
        // Pick a bunch of named bitsets and return their intersection.
        AstType* result = AstType::Any();
        for (int i = 0, m = 1 + rng_->NextInt(3); i < m; ++i) {
          int j = rng_->NextInt(n);
#define PICK_BITSET_TYPE(type, value)                                  \
  if (j-- == 0) {                                                      \
    AstType* tmp = AstType::Intersect(result, AstType::type(), zone_); \
    if (tmp->Is(AstType::None()) && i != 0) {                          \
      break;                                                           \
    } else {                                                           \
      result = tmp;                                                    \
      continue;                                                        \
    }                                                                  \
  }
          AST_PROPER_BITSET_TYPE_LIST(PICK_BITSET_TYPE)
#undef PICK_BITSET_TYPE
        }
        return result;
      }
      case 1: {  // class
        int i = rng_->NextInt(static_cast<int>(maps.size()));
        return AstType::Class(maps[i], zone_);
      }
      case 2: {  // constant
        int i = rng_->NextInt(static_cast<int>(values.size()));
        return AstType::Constant(values[i], zone_);
      }
      case 3: {  // range
        int i = rng_->NextInt(static_cast<int>(integers.size()));
        int j = rng_->NextInt(static_cast<int>(integers.size()));
        double min = integers[i]->Number();
        double max = integers[j]->Number();
        if (min > max) std::swap(min, max);
        return AstType::Range(min, max, zone_);
      }
      case 4: {  // context
        int depth = rng_->NextInt(3);
        AstType* type = AstType::Internal();
        for (int i = 0; i < depth; ++i) type = AstType::Context(type, zone_);
        return type;
      }
      case 5: {  // array
        AstType* element = Fuzz(depth / 2);
        return AstType::Array(element, zone_);
      }
      case 6:
      case 7: {  // function
        AstType* result = Fuzz(depth / 2);
        AstType* receiver = Fuzz(depth / 2);
        int arity = rng_->NextInt(3);
        AstType* type = AstType::Function(result, receiver, arity, zone_);
        for (int i = 0; i < type->AsFunction()->Arity(); ++i) {
          AstType* parameter = Fuzz(depth / 2);
          type->AsFunction()->InitParameter(i, parameter);
        }
        return type;
      }
      default: {  // union
        int n = rng_->NextInt(10);
        AstType* type = None;
        for (int i = 0; i < n; ++i) {
          AstType* operand = Fuzz(depth - 1);
          type = AstType::Union(type, operand, zone_);
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
