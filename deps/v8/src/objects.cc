// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/api.h"
#include "src/api-natives.h"
#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-dependencies.h"
#include "src/compiler.h"
#include "src/date.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/execution.h"
#include "src/field-index-inl.h"
#include "src/field-index.h"
#include "src/field-type.h"
#include "src/full-codegen/full-codegen.h"
#include "src/ic/ic.h"
#include "src/identity-map.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/source-position-table.h"
#include "src/isolate-inl.h"
#include "src/key-accumulator.h"
#include "src/list.h"
#include "src/log.h"
#include "src/lookup.h"
#include "src/macro-assembler.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/profiler/cpu-profiler.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"
#include "src/regexp/jsregexp.h"
#include "src/safepoint-table.h"
#include "src/string-builder.h"
#include "src/string-search.h"
#include "src/string-stream.h"
#include "src/utils.h"
#include "src/zone.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/disasm.h"
#include "src/disassembler.h"
#endif

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& os, InstanceType instance_type) {
  switch (instance_type) {
#define WRITE_TYPE(TYPE) \
  case TYPE:             \
    return os << #TYPE;
    INSTANCE_TYPE_LIST(WRITE_TYPE)
#undef WRITE_TYPE
  }
  UNREACHABLE();
  return os << "UNKNOWN";  // Keep the compiler happy.
}

Handle<FieldType> Object::OptimalType(Isolate* isolate,
                                      Representation representation) {
  if (representation.IsNone()) return FieldType::None(isolate);
  if (FLAG_track_field_types) {
    if (representation.IsHeapObject() && IsHeapObject()) {
      // We can track only JavaScript objects with stable maps.
      Handle<Map> map(HeapObject::cast(this)->map(), isolate);
      if (map->is_stable() && map->IsJSReceiverMap()) {
        return FieldType::Class(map, isolate);
      }
    }
  }
  return FieldType::Any(isolate);
}


MaybeHandle<JSReceiver> Object::ToObject(Isolate* isolate,
                                         Handle<Object> object,
                                         Handle<Context> native_context) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  Handle<JSFunction> constructor;
  if (object->IsSmi()) {
    constructor = handle(native_context->number_function(), isolate);
  } else {
    int constructor_function_index =
        Handle<HeapObject>::cast(object)->map()->GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kUndefinedOrNullToObject),
                      JSReceiver);
    }
    constructor = handle(
        JSFunction::cast(native_context->get(constructor_function_index)),
        isolate);
  }
  Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
  Handle<JSValue>::cast(result)->set_value(*object);
  return result;
}


// static
MaybeHandle<Name> Object::ToName(Isolate* isolate, Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input, Object::ToPrimitive(input, ToPrimitiveHint::kString),
      Name);
  if (input->IsName()) return Handle<Name>::cast(input);
  return ToString(isolate, input);
}


// static
MaybeHandle<Object> Object::ToNumber(Handle<Object> input) {
  while (true) {
    if (input->IsNumber()) {
      return input;
    }
    if (input->IsString()) {
      return String::ToNumber(Handle<String>::cast(input));
    }
    if (input->IsOddball()) {
      return Oddball::ToNumber(Handle<Oddball>::cast(input));
    }
    Isolate* const isolate = Handle<HeapObject>::cast(input)->GetIsolate();
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToNumber),
                      Object);
    }
    if (input->IsSimd128Value()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSimdToNumber),
                      Object);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kNumber),
        Object);
  }
}


// static
MaybeHandle<Object> Object::ToInteger(Isolate* isolate, Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  return isolate->factory()->NewNumber(DoubleToInteger(input->Number()));
}


// static
MaybeHandle<Object> Object::ToInt32(Isolate* isolate, Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  return isolate->factory()->NewNumberFromInt(DoubleToInt32(input->Number()));
}


// static
MaybeHandle<Object> Object::ToUint32(Isolate* isolate, Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  return isolate->factory()->NewNumberFromUint(DoubleToUint32(input->Number()));
}


// static
MaybeHandle<String> Object::ToString(Isolate* isolate, Handle<Object> input) {
  while (true) {
    if (input->IsString()) {
      return Handle<String>::cast(input);
    }
    if (input->IsOddball()) {
      return handle(Handle<Oddball>::cast(input)->to_string(), isolate);
    }
    if (input->IsNumber()) {
      return isolate->factory()->NumberToString(input);
    }
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToString),
                      String);
    }
    if (input->IsSimd128Value()) {
      return Simd128Value::ToString(Handle<Simd128Value>::cast(input));
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kString),
        String);
  }
}


// static
MaybeHandle<Object> Object::ToLength(Isolate* isolate, Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  double len = DoubleToInteger(input->Number());
  if (len <= 0.0) {
    len = 0.0;
  } else if (len >= kMaxSafeInteger) {
    len = kMaxSafeInteger;
  }
  return isolate->factory()->NewNumber(len);
}


bool Object::BooleanValue() {
  if (IsBoolean()) return IsTrue();
  if (IsSmi()) return Smi::cast(this)->value() != 0;
  if (IsUndefined() || IsNull()) return false;
  if (IsUndetectableObject()) return false;   // Undetectable object is false.
  if (IsString()) return String::cast(this)->length() != 0;
  if (IsHeapNumber()) return HeapNumber::cast(this)->HeapNumberBooleanValue();
  return true;
}


namespace {

// TODO(bmeurer): Maybe we should introduce a marker interface Number,
// where we put all these methods at some point?
ComparisonResult NumberCompare(double x, double y) {
  if (std::isnan(x) || std::isnan(y)) {
    return ComparisonResult::kUndefined;
  } else if (x < y) {
    return ComparisonResult::kLessThan;
  } else if (x > y) {
    return ComparisonResult::kGreaterThan;
  } else {
    return ComparisonResult::kEqual;
  }
}


bool NumberEquals(double x, double y) {
  // Must check explicitly for NaN's on Windows, but -0 works fine.
  if (std::isnan(x)) return false;
  if (std::isnan(y)) return false;
  return x == y;
}


bool NumberEquals(const Object* x, const Object* y) {
  return NumberEquals(x->Number(), y->Number());
}


bool NumberEquals(Handle<Object> x, Handle<Object> y) {
  return NumberEquals(*x, *y);
}

}  // namespace


// static
Maybe<ComparisonResult> Object::Compare(Handle<Object> x, Handle<Object> y) {
  // ES6 section 7.2.11 Abstract Relational Comparison step 3 and 4.
  if (!Object::ToPrimitive(x, ToPrimitiveHint::kNumber).ToHandle(&x) ||
      !Object::ToPrimitive(y, ToPrimitiveHint::kNumber).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }
  if (x->IsString() && y->IsString()) {
    // ES6 section 7.2.11 Abstract Relational Comparison step 5.
    return Just(
        String::Compare(Handle<String>::cast(x), Handle<String>::cast(y)));
  }
  // ES6 section 7.2.11 Abstract Relational Comparison step 6.
  if (!Object::ToNumber(x).ToHandle(&x) || !Object::ToNumber(y).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }
  return Just(NumberCompare(x->Number(), y->Number()));
}


// static
Maybe<bool> Object::Equals(Handle<Object> x, Handle<Object> y) {
  while (true) {
    if (x->IsNumber()) {
      if (y->IsNumber()) {
        return Just(NumberEquals(x, y));
      } else if (y->IsBoolean()) {
        return Just(NumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsString()) {
        return Just(NumberEquals(x, String::ToNumber(Handle<String>::cast(y))));
      } else if (y->IsJSReceiver() && !y->IsUndetectableObject()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsString()) {
      if (y->IsString()) {
        return Just(
            String::Equals(Handle<String>::cast(x), Handle<String>::cast(y)));
      } else if (y->IsNumber()) {
        x = String::ToNumber(Handle<String>::cast(x));
        return Just(NumberEquals(x, y));
      } else if (y->IsBoolean()) {
        x = String::ToNumber(Handle<String>::cast(x));
        return Just(NumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsJSReceiver() && !y->IsUndetectableObject()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsBoolean()) {
      if (y->IsOddball()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsNumber()) {
        return Just(NumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsString()) {
        y = String::ToNumber(Handle<String>::cast(y));
        return Just(NumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsJSReceiver() && !y->IsUndetectableObject()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
        x = Oddball::ToNumber(Handle<Oddball>::cast(x));
      } else {
        return Just(false);
      }
    } else if (x->IsSymbol()) {
      if (y->IsSymbol()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsJSReceiver() && !y->IsUndetectableObject()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsSimd128Value()) {
      if (y->IsSimd128Value()) {
        return Just(Simd128Value::Equals(Handle<Simd128Value>::cast(x),
                                         Handle<Simd128Value>::cast(y)));
      } else if (y->IsJSReceiver() && !y->IsUndetectableObject()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsJSReceiver() && !x->IsUndetectableObject()) {
      if (y->IsJSReceiver()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsNull() || y->IsUndefined()) {
        return Just(false);
      } else if (y->IsBoolean()) {
        y = Oddball::ToNumber(Handle<Oddball>::cast(y));
      } else if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(x))
                      .ToHandle(&x)) {
        return Nothing<bool>();
      }
    } else {
      return Just(
          (x->IsNull() || x->IsUndefined() || x->IsUndetectableObject()) &&
          (y->IsNull() || y->IsUndefined() || y->IsUndetectableObject()));
    }
  }
}


bool Object::StrictEquals(Object* that) {
  if (this->IsNumber()) {
    if (!that->IsNumber()) return false;
    return NumberEquals(this, that);
  } else if (this->IsString()) {
    if (!that->IsString()) return false;
    return String::cast(this)->Equals(String::cast(that));
  } else if (this->IsSimd128Value()) {
    if (!that->IsSimd128Value()) return false;
    return Simd128Value::cast(this)->Equals(Simd128Value::cast(that));
  }
  return this == that;
}


// static
Handle<String> Object::TypeOf(Isolate* isolate, Handle<Object> object) {
  if (object->IsNumber()) return isolate->factory()->number_string();
  if (object->IsOddball()) return handle(Oddball::cast(*object)->type_of());
  if (object->IsUndetectableObject()) {
    return isolate->factory()->undefined_string();
  }
  if (object->IsString()) return isolate->factory()->string_string();
  if (object->IsSymbol()) return isolate->factory()->symbol_string();
  if (object->IsString()) return isolate->factory()->string_string();
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type) \
  if (object->Is##Type()) return isolate->factory()->type##_string();
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
  if (object->IsCallable()) return isolate->factory()->function_string();
  return isolate->factory()->object_string();
}


// static
MaybeHandle<Object> Object::Multiply(Isolate* isolate, Handle<Object> lhs,
                                     Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumber(lhs->Number() * rhs->Number());
}


// static
MaybeHandle<Object> Object::Divide(Isolate* isolate, Handle<Object> lhs,
                                   Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumber(lhs->Number() / rhs->Number());
}


// static
MaybeHandle<Object> Object::Modulus(Isolate* isolate, Handle<Object> lhs,
                                    Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumber(modulo(lhs->Number(), rhs->Number()));
}


// static
MaybeHandle<Object> Object::Add(Isolate* isolate, Handle<Object> lhs,
                                Handle<Object> rhs) {
  if (lhs->IsNumber() && rhs->IsNumber()) {
    return isolate->factory()->NewNumber(lhs->Number() + rhs->Number());
  } else if (lhs->IsString() && rhs->IsString()) {
    return isolate->factory()->NewConsString(Handle<String>::cast(lhs),
                                             Handle<String>::cast(rhs));
  }
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToPrimitive(lhs), Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToPrimitive(rhs), Object);
  if (lhs->IsString() || rhs->IsString()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToString(isolate, rhs),
                               Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToString(isolate, lhs),
                               Object);
    return isolate->factory()->NewConsString(Handle<String>::cast(lhs),
                                             Handle<String>::cast(rhs));
  }
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
  return isolate->factory()->NewNumber(lhs->Number() + rhs->Number());
}


// static
MaybeHandle<Object> Object::Subtract(Isolate* isolate, Handle<Object> lhs,
                                     Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumber(lhs->Number() - rhs->Number());
}


// static
MaybeHandle<Object> Object::ShiftLeft(Isolate* isolate, Handle<Object> lhs,
                                      Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromInt(NumberToInt32(*lhs)
                                              << (NumberToUint32(*rhs) & 0x1F));
}


// static
MaybeHandle<Object> Object::ShiftRight(Isolate* isolate, Handle<Object> lhs,
                                       Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromInt(NumberToInt32(*lhs) >>
                                              (NumberToUint32(*rhs) & 0x1F));
}


// static
MaybeHandle<Object> Object::ShiftRightLogical(Isolate* isolate,
                                              Handle<Object> lhs,
                                              Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromUint(NumberToUint32(*lhs) >>
                                               (NumberToUint32(*rhs) & 0x1F));
}


// static
MaybeHandle<Object> Object::BitwiseAnd(Isolate* isolate, Handle<Object> lhs,
                                       Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromInt(NumberToInt32(*lhs) &
                                              NumberToInt32(*rhs));
}


// static
MaybeHandle<Object> Object::BitwiseOr(Isolate* isolate, Handle<Object> lhs,
                                      Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromInt(NumberToInt32(*lhs) |
                                              NumberToInt32(*rhs));
}


// static
MaybeHandle<Object> Object::BitwiseXor(Isolate* isolate, Handle<Object> lhs,
                                       Handle<Object> rhs) {
  if (!lhs->IsNumber() || !rhs->IsNumber()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(lhs), Object);
    ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(rhs), Object);
  }
  return isolate->factory()->NewNumberFromInt(NumberToInt32(*lhs) ^
                                              NumberToInt32(*rhs));
}


Maybe<bool> Object::IsArray(Handle<Object> object) {
  if (object->IsJSArray()) return Just(true);
  if (object->IsJSProxy()) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    Isolate* isolate = proxy->GetIsolate();
    if (proxy->IsRevoked()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyRevoked,
          isolate->factory()->NewStringFromAsciiChecked("IsArray")));
      return Nothing<bool>();
    }
    return Object::IsArray(handle(proxy->target(), isolate));
  }
  return Just(false);
}


bool Object::IsPromise(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  auto js_object = Handle<JSObject>::cast(object);
  // Promises can't have access checks.
  if (js_object->map()->is_access_check_needed()) return false;
  auto isolate = js_object->GetIsolate();
  // TODO(dcarney): this should just be read from the symbol registry so as not
  // to be context dependent.
  auto key = isolate->factory()->promise_status_symbol();
  // Shouldn't be possible to throw here.
  return JSObject::HasRealNamedProperty(js_object, key).FromJust();
}


// static
MaybeHandle<Object> Object::GetMethod(Handle<JSReceiver> receiver,
                                      Handle<Name> name) {
  Handle<Object> func;
  Isolate* isolate = receiver->GetIsolate();
  ASSIGN_RETURN_ON_EXCEPTION(isolate, func,
                             JSReceiver::GetProperty(receiver, name), Object);
  if (func->IsNull() || func->IsUndefined()) {
    return isolate->factory()->undefined_value();
  }
  if (!func->IsCallable()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kPropertyNotFunction,
                                          func, name, receiver),
                    Object);
  }
  return func;
}


// static
MaybeHandle<FixedArray> Object::CreateListFromArrayLike(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  // 1. ReturnIfAbrupt(object).
  // 2. (default elementTypes -- not applicable.)
  // 3. If Type(obj) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject,
                                 isolate->factory()->NewStringFromAsciiChecked(
                                     "CreateListFromArrayLike")),
                    FixedArray);
  }
  // 4. Let len be ? ToLength(? Get(obj, "length")).
  Handle<Object> raw_length_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, raw_length_obj,
      JSReceiver::GetProperty(object, isolate->factory()->length_string()),
      FixedArray);
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, raw_length_number,
                             Object::ToLength(isolate, raw_length_obj),
                             FixedArray);
  uint32_t len;
  if (!raw_length_number->ToUint32(&len) ||
      len > static_cast<uint32_t>(FixedArray::kMaxLength)) {
    THROW_NEW_ERROR(isolate,
                    NewRangeError(MessageTemplate::kInvalidArrayLength),
                    FixedArray);
  }
  // 5. Let list be an empty List.
  Handle<FixedArray> list = isolate->factory()->NewFixedArray(len);
  // 6. Let index be 0.
  // 7. Repeat while index < len:
  for (uint32_t index = 0; index < len; ++index) {
    // 7a. Let indexName be ToString(index).
    // 7b. Let next be ? Get(obj, indexName).
    Handle<Object> next;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, next, Object::GetElement(isolate, object, index), FixedArray);
    switch (element_types) {
      case ElementTypes::kAll:
        // Nothing to do.
        break;
      case ElementTypes::kStringAndSymbol: {
        // 7c. If Type(next) is not an element of elementTypes, throw a
        //     TypeError exception.
        if (!next->IsName()) {
          THROW_NEW_ERROR(isolate,
                          NewTypeError(MessageTemplate::kNotPropertyName, next),
                          FixedArray);
        }
        // 7d. Append next as the last element of list.
        // Internalize on the fly so we can use pointer identity later.
        next = isolate->factory()->InternalizeName(Handle<Name>::cast(next));
        break;
      }
    }
    list->set(index, *next);
    // 7e. Set index to index + 1. (See loop header.)
  }
  // 8. Return list.
  return list;
}


// static
Maybe<bool> JSReceiver::HasProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        // Call the "has" trap on proxies.
        return JSProxy::HasProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                    it->GetName());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (!result.IsJust()) return Nothing<bool>();
        if (result.FromJust() != ABSENT) return Just(true);
        break;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it->HasAccess()) break;
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
        if (!result.IsJust()) return Nothing<bool>();
        return Just(result.FromJust() != ABSENT);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TypedArray out-of-bounds access.
        return Just(false);
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return Just(true);
    }
  }
  return Just(false);
}


// static
MaybeHandle<Object> Object::GetProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY: {
        bool was_found;
        MaybeHandle<Object> result =
            JSProxy::GetProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                 it->GetName(), it->GetReceiver(), &was_found);
        if (!was_found) it->NotFound();
        return result;
      }
      case LookupIterator::INTERCEPTOR: {
        bool done;
        Handle<Object> result;
        ASSIGN_RETURN_ON_EXCEPTION(
            it->isolate(), result,
            JSObject::GetPropertyWithInterceptor(it, &done), Object);
        if (done) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyWithFailedAccessCheck(it);
      case LookupIterator::ACCESSOR:
        return GetPropertyWithAccessor(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return ReadAbsentProperty(it);
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }
  return ReadAbsentProperty(it);
}


#define STACK_CHECK(result_value)                        \
  do {                                                   \
    StackLimitCheck stack_check(isolate);                \
    if (stack_check.HasOverflowed()) {                   \
      isolate->Throw(*isolate->factory()->NewRangeError( \
          MessageTemplate::kStackOverflow));             \
      return result_value;                               \
    }                                                    \
  } while (false)


// static
MaybeHandle<Object> JSProxy::GetProperty(Isolate* isolate,
                                         Handle<JSProxy> proxy,
                                         Handle<Name> name,
                                         Handle<Object> receiver,
                                         bool* was_found) {
  *was_found = true;
  if (receiver->IsJSGlobalObject()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kReadGlobalReferenceThroughProxy, name),
        Object);
  }

  DCHECK(!name->IsPrivate());
  STACK_CHECK(MaybeHandle<Object>());
  Handle<Name> trap_name = isolate->factory()->get_string();
  // 1. Assert: IsPropertyKey(P) is true.
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyRevoked, trap_name),
                    Object);
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 6. Let trap be ? GetMethod(handler, "get").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name), Object);
  // 7. If trap is undefined, then
  if (trap->IsUndefined()) {
    // 7.a Return target.[[Get]](P, Receiver).
    LookupIterator it =
        LookupIterator::PropertyOrElement(isolate, receiver, name, target);
    MaybeHandle<Object> result = Object::GetProperty(&it);
    *was_found = it.IsFound();
    return result;
  }
  // 8. Let trapResult be ? Call(trap, handler, «target, P, Receiver»).
  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name, receiver};
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args), Object);
  // 9. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN_NULL(target_found);
  // 10. If targetDesc is not undefined, then
  if (target_found.FromJust()) {
    // 10.a. If IsDataDescriptor(targetDesc) and targetDesc.[[Configurable]] is
    //       false and targetDesc.[[Writable]] is false, then
    // 10.a.i. If SameValue(trapResult, targetDesc.[[Value]]) is false,
    //        throw a TypeError exception.
    bool inconsistent = PropertyDescriptor::IsDataDescriptor(&target_desc) &&
                        !target_desc.configurable() &&
                        !target_desc.writable() &&
                        !trap_result->SameValue(*target_desc.value());
    if (inconsistent) {
      THROW_NEW_ERROR(
          isolate, NewTypeError(MessageTemplate::kProxyGetNonConfigurableData,
                                name, target_desc.value(), trap_result),
          Object);
    }
    // 10.b. If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]]
    //       is false and targetDesc.[[Get]] is undefined, then
    // 10.b.i. If trapResult is not undefined, throw a TypeError exception.
    inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                   !target_desc.configurable() &&
                   target_desc.get()->IsUndefined() &&
                   !trap_result->IsUndefined();
    if (inconsistent) {
      THROW_NEW_ERROR(
          isolate,
          NewTypeError(MessageTemplate::kProxyGetNonConfigurableAccessor, name,
                       trap_result),
          Object);
    }
  }
  // 11. Return trap_result
  return trap_result;
}


Handle<Object> JSReceiver::GetDataProperty(Handle<JSReceiver> object,
                                           Handle<Name> name) {
  LookupIterator it(object, name,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  return GetDataProperty(&it);
}


Handle<Object> JSReceiver::GetDataProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        // Support calling this method without an active context, but refuse
        // access to access-checked objects in that case.
        if (it->isolate()->context() != nullptr && it->HasAccess()) continue;
      // Fall through.
      case LookupIterator::JSPROXY:
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::ACCESSOR:
        // TODO(verwaest): For now this doesn't call into AccessorInfo, since
        // clients don't need it. Update once relevant.
        it->NotFound();
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }
  return it->isolate()->factory()->undefined_value();
}


bool Object::ToInt32(int32_t* value) {
  if (IsSmi()) {
    *value = Smi::cast(this)->value();
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    if (FastI2D(FastD2I(num)) == num) {
      *value = FastD2I(num);
      return true;
    }
  }
  return false;
}


bool Object::ToUint32(uint32_t* value) {
  if (IsSmi()) {
    int num = Smi::cast(this)->value();
    if (num < 0) return false;
    *value = static_cast<uint32_t>(num);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    if (num < 0) return false;
    uint32_t uint_value = FastD2UI(num);
    if (FastUI2D(uint_value) == num) {
      *value = uint_value;
      return true;
    }
  }
  return false;
}


bool FunctionTemplateInfo::IsTemplateFor(Object* object) {
  if (!object->IsHeapObject()) return false;
  return IsTemplateFor(HeapObject::cast(object)->map());
}


bool FunctionTemplateInfo::IsTemplateFor(Map* map) {
  // There is a constraint on the object; check.
  if (!map->IsJSObjectMap()) return false;
  // Fetch the constructor function of the object.
  Object* cons_obj = map->GetConstructor();
  if (!cons_obj->IsJSFunction()) return false;
  JSFunction* fun = JSFunction::cast(cons_obj);
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  for (Object* type = fun->shared()->function_data();
       type->IsFunctionTemplateInfo();
       type = FunctionTemplateInfo::cast(type)->parent_template()) {
    if (type == this) return true;
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


// TODO(dcarney): CallOptimization duplicates this logic, merge.
Object* FunctionTemplateInfo::GetCompatibleReceiver(Isolate* isolate,
                                                    Object* receiver) {
  // API calls are only supported with JSObject receivers.
  if (!receiver->IsJSObject()) return isolate->heap()->null_value();
  Object* recv_type = this->signature();
  // No signature, return holder.
  if (recv_type->IsUndefined()) return receiver;
  FunctionTemplateInfo* signature = FunctionTemplateInfo::cast(recv_type);
  // Check the receiver.
  for (PrototypeIterator iter(isolate, JSObject::cast(receiver),
                              PrototypeIterator::START_AT_RECEIVER,
                              PrototypeIterator::END_AT_NON_HIDDEN);
       !iter.IsAtEnd(); iter.Advance()) {
    if (signature->IsTemplateFor(iter.GetCurrent())) return iter.GetCurrent();
  }
  return isolate->heap()->null_value();
}


// static
MaybeHandle<JSObject> JSObject::New(Handle<JSFunction> constructor,
                                    Handle<JSReceiver> new_target,
                                    Handle<AllocationSite> site) {
  // If called through new, new.target can be:
  // - a subclass of constructor,
  // - a proxy wrapper around constructor, or
  // - the constructor itself.
  // If called through Reflect.construct, it's guaranteed to be a constructor.
  Isolate* const isolate = constructor->GetIsolate();
  DCHECK(constructor->IsConstructor());
  DCHECK(new_target->IsConstructor());
  DCHECK(!constructor->has_initial_map() ||
         constructor->initial_map()->instance_type() != JS_FUNCTION_TYPE);

  Handle<Map> initial_map;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, initial_map,
      JSFunction::GetDerivedMap(isolate, constructor, new_target), JSObject);
  Handle<JSObject> result =
      isolate->factory()->NewJSObjectFromMap(initial_map, NOT_TENURED, site);
  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();
  return result;
}


Handle<FixedArray> JSObject::EnsureWritableFastElements(
    Handle<JSObject> object) {
  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastStringWrapperElements());
  Isolate* isolate = object->GetIsolate();
  Handle<FixedArray> elems(FixedArray::cast(object->elements()), isolate);
  if (elems->map() != isolate->heap()->fixed_cow_array_map()) return elems;
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
  return writable_elems;
}


// ES6 9.5.1
// static
MaybeHandle<Object> JSProxy::GetPrototype(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> trap_name = isolate->factory()->getPrototypeOf_string();

  STACK_CHECK(MaybeHandle<Object>());

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot.
  // 2. If handler is null, throw a TypeError exception.
  // 3. Assert: Type(handler) is Object.
  // 4. Let target be the value of the [[ProxyTarget]] internal slot.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyRevoked, trap_name),
                    Object);
  }
  Handle<JSReceiver> target(proxy->target(), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  // 5. Let trap be ? GetMethod(handler, "getPrototypeOf").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, trap, GetMethod(handler, trap_name),
                             Object);
  // 6. If trap is undefined, then return target.[[GetPrototypeOf]]().
  if (trap->IsUndefined()) {
    return JSReceiver::GetPrototype(isolate, target);
  }
  // 7. Let handlerProto be ? Call(trap, handler, «target»).
  Handle<Object> argv[] = {target};
  Handle<Object> handler_proto;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, handler_proto,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv), Object);
  // 8. If Type(handlerProto) is neither Object nor Null, throw a TypeError.
  if (!(handler_proto->IsJSReceiver() || handler_proto->IsNull())) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyGetPrototypeOfInvalid),
                    Object);
  }
  // 9. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> is_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN_NULL(is_extensible);
  // 10. If extensibleTarget is true, return handlerProto.
  if (is_extensible.FromJust()) return handler_proto;
  // 11. Let targetProto be ? target.[[GetPrototypeOf]]().
  Handle<Object> target_proto;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, target_proto,
                             JSReceiver::GetPrototype(isolate, target), Object);
  // 12. If SameValue(handlerProto, targetProto) is false, throw a TypeError.
  if (!handler_proto->SameValue(*target_proto)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kProxyGetPrototypeOfNonExtensible),
        Object);
  }
  // 13. Return handlerProto.
  return handler_proto;
}

MaybeHandle<Object> Object::GetPropertyWithAccessor(LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the getter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  if (structure->IsAccessorInfo()) {
    Handle<JSObject> holder = it->GetHolder<JSObject>();
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                   name, receiver),
                      Object);
    }

    v8::AccessorNameGetterCallback call_fun =
        v8::ToCData<v8::AccessorNameGetterCallback>(info->getter());
    if (call_fun == nullptr) return isolate->factory()->undefined_value();

    LOG(isolate, ApiNamedPropertyAccess("load", *holder, *name));
    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   Object::DONT_THROW);
    v8::Local<v8::Value> result = args.Call(call_fun, v8::Utils::ToLocal(name));
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.IsEmpty()) {
      return ReadAbsentProperty(isolate, receiver, name);
    }
    Handle<Object> return_value = v8::Utils::OpenHandle(*result);
    return_value->VerifyApiCallResultType();
    // Rebox handle before return.
    return handle(*return_value, isolate);
  }

  // Regular accessor.
  Handle<Object> getter(AccessorPair::cast(*structure)->getter(), isolate);
  if (getter->IsFunctionTemplateInfo()) {
    auto result = Builtins::InvokeApiFunction(
        Handle<FunctionTemplateInfo>::cast(getter), receiver, 0, nullptr);
    if (isolate->has_pending_exception()) {
      return MaybeHandle<Object>();
    }
    Handle<Object> return_value;
    if (result.ToHandle(&return_value)) {
      return_value->VerifyApiCallResultType();
      return handle(*return_value, isolate);
    }
  } else if (getter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return Object::GetPropertyWithDefinedGetter(
        receiver, Handle<JSReceiver>::cast(getter));
  }
  // Getter is not a function.
  return ReadAbsentProperty(isolate, receiver, it->GetName());
}


bool AccessorInfo::IsCompatibleReceiverMap(Isolate* isolate,
                                           Handle<AccessorInfo> info,
                                           Handle<Map> map) {
  if (!info->HasExpectedReceiverType()) return true;
  if (!map->IsJSObjectMap()) return false;
  return FunctionTemplateInfo::cast(info->expected_receiver_type())
      ->IsTemplateFor(*map);
}

Maybe<bool> Object::SetPropertyWithAccessor(LookupIterator* it,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the setter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  if (structure->IsAccessorInfo()) {
    Handle<JSObject> holder = it->GetHolder<JSObject>();
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kIncompatibleMethodReceiver, name, receiver));
      return Nothing<bool>();
    }

    v8::AccessorNameSetterCallback call_fun =
        v8::ToCData<v8::AccessorNameSetterCallback>(info->setter());
    // TODO(verwaest): We should not get here anymore once all AccessorInfos are
    // marked as special_data_property. They cannot both be writable and not
    // have a setter.
    if (call_fun == nullptr) return Just(true);

    LOG(isolate, ApiNamedPropertyAccess("store", *holder, *name));
    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   should_throw);
    args.Call(call_fun, v8::Utils::ToLocal(name), v8::Utils::ToLocal(value));
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    return Just(true);
  }

  // Regular accessor.
  Handle<Object> setter(AccessorPair::cast(*structure)->setter(), isolate);
  if (setter->IsFunctionTemplateInfo()) {
    Handle<Object> argv[] = {value};
    auto result =
        Builtins::InvokeApiFunction(Handle<FunctionTemplateInfo>::cast(setter),
                                    receiver, arraysize(argv), argv);
    if (isolate->has_pending_exception()) {
      return Nothing<bool>();
    }
    return Just(true);
  } else if (setter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return SetPropertyWithDefinedSetter(
        receiver, Handle<JSReceiver>::cast(setter), value, should_throw);
  }

  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kNoSetterInCallback,
                              it->GetName(), it->GetHolder<JSObject>()));
}


MaybeHandle<Object> Object::GetPropertyWithDefinedGetter(
    Handle<Object> receiver,
    Handle<JSReceiver> getter) {
  Isolate* isolate = getter->GetIsolate();

  // Platforms with simulators like arm/arm64 expose a funny issue. If the
  // simulator has a separate JS stack pointer from the C++ stack pointer, it
  // can miss C++ stack overflows in the stack guard at the start of JavaScript
  // functions. It would be very expensive to check the C++ stack pointer at
  // that location. The best solution seems to be to break the impasse by
  // adding checks at possible recursion points. What's more, we don't put
  // this stack check behind the USE_SIMULATOR define in order to keep
  // behavior the same between hardware and simulators.
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    isolate->StackOverflow();
    return MaybeHandle<Object>();
  }

  return Execution::Call(isolate, getter, receiver, 0, NULL);
}


Maybe<bool> Object::SetPropertyWithDefinedSetter(Handle<Object> receiver,
                                                 Handle<JSReceiver> setter,
                                                 Handle<Object> value,
                                                 ShouldThrow should_throw) {
  Isolate* isolate = setter->GetIsolate();

  Handle<Object> argv[] = { value };
  RETURN_ON_EXCEPTION_VALUE(isolate, Execution::Call(isolate, setter, receiver,
                                                     arraysize(argv), argv),
                            Nothing<bool>());
  return Just(true);
}


// static
bool Object::IsErrorObject(Isolate* isolate, Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  // Use stack_trace_symbol as proxy for [[ErrorData]].
  Handle<Name> symbol = isolate->factory()->stack_trace_symbol();
  Maybe<bool> has_stack_trace =
      JSReceiver::HasOwnProperty(Handle<JSReceiver>::cast(object), symbol);
  DCHECK(!has_stack_trace.IsNothing());
  return has_stack_trace.FromJust();
}


// static
bool JSObject::AllCanRead(LookupIterator* it) {
  // Skip current iteration, it's in state ACCESS_CHECK or INTERCEPTOR, both of
  // which have already been checked.
  DCHECK(it->state() == LookupIterator::ACCESS_CHECK ||
         it->state() == LookupIterator::INTERCEPTOR);
  for (it->Next(); it->IsFound(); it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      auto accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_read()) return true;
      }
    } else if (it->state() == LookupIterator::INTERCEPTOR) {
      if (it->GetInterceptor()->all_can_read()) return true;
    } else if (it->state() == LookupIterator::JSPROXY) {
      // Stop lookupiterating. And no, AllCanNotRead.
      return false;
    }
  }
  return false;
}


MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  while (AllCanRead(it)) {
    if (it->state() == LookupIterator::ACCESSOR) {
      return GetPropertyWithAccessor(it);
    }
    DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
    bool done;
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(it->isolate(), result,
                               GetPropertyWithInterceptor(it, &done), Object);
    if (done) return result;
  }

  // Cross-Origin [[Get]] of Well-Known Symbols does not throw, and returns
  // undefined.
  Handle<Name> name = it->GetName();
  if (name->IsSymbol() && Symbol::cast(*name)->is_well_known_symbol()) {
    return it->factory()->undefined_value();
  }

  it->isolate()->ReportFailedAccessCheck(checked);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(it->isolate(), Object);
  return it->factory()->undefined_value();
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  while (AllCanRead(it)) {
    if (it->state() == LookupIterator::ACCESSOR) {
      return Just(it->property_attributes());
    }
    DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
    auto result = GetPropertyAttributesWithInterceptor(it);
    if (it->isolate()->has_scheduled_exception()) break;
    if (result.IsJust() && result.FromJust() != ABSENT) return result;
  }
  it->isolate()->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(),
                                      Nothing<PropertyAttributes>());
  return Just(ABSENT);
}


// static
bool JSObject::AllCanWrite(LookupIterator* it) {
  for (; it->IsFound() && it->state() != LookupIterator::JSPROXY; it->Next()) {
    if (it->state() == LookupIterator::ACCESSOR) {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        if (AccessorInfo::cast(*accessors)->all_can_write()) return true;
      }
    }
  }
  return false;
}


Maybe<bool> JSObject::SetPropertyWithFailedAccessCheck(
    LookupIterator* it, Handle<Object> value, ShouldThrow should_throw) {
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  if (AllCanWrite(it)) {
    return SetPropertyWithAccessor(it, value, should_throw);
  }

  it->isolate()->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(true);
}


void JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                     Handle<Name> name,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  DCHECK(!object->HasFastProperties());
  if (!name->IsUniqueName()) {
    name = object->GetIsolate()->factory()->InternalizeString(
        Handle<String>::cast(name));
  }

  if (object->IsJSGlobalObject()) {
    Handle<GlobalDictionary> property_dictionary(object->global_dictionary());

    int entry = property_dictionary->FindEntry(name);
    if (entry == GlobalDictionary::kNotFound) {
      auto cell = object->GetIsolate()->factory()->NewPropertyCell();
      cell->set_value(*value);
      auto cell_type = value->IsUndefined() ? PropertyCellType::kUndefined
                                            : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      value = cell;
      property_dictionary =
          GlobalDictionary::Add(property_dictionary, name, value, details);
      object->set_properties(*property_dictionary);
    } else {
      PropertyCell::UpdateCell(property_dictionary, entry, value, details);
    }
  } else {
    Handle<NameDictionary> property_dictionary(object->property_dictionary());

    int entry = property_dictionary->FindEntry(name);
    if (entry == NameDictionary::kNotFound) {
      property_dictionary =
          NameDictionary::Add(property_dictionary, name, value, details);
      object->set_properties(*property_dictionary);
    } else {
      PropertyDetails original_details = property_dictionary->DetailsAt(entry);
      int enumeration_index = original_details.dictionary_index();
      DCHECK(enumeration_index > 0);
      details = details.set_index(enumeration_index);
      property_dictionary->SetEntry(entry, name, value, details);
    }
  }
}

Maybe<bool> JSReceiver::HasInPrototypeChain(Isolate* isolate,
                                            Handle<JSReceiver> object,
                                            Handle<Object> proto) {
  PrototypeIterator iter(isolate, object, PrototypeIterator::START_AT_RECEIVER);
  while (true) {
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
    if (PrototypeIterator::GetCurrent(iter).is_identical_to(proto)) {
      return Just(true);
    }
  }
}


Map* Object::GetRootMap(Isolate* isolate) {
  DisallowHeapAllocation no_alloc;
  if (IsSmi()) {
    Context* native_context = isolate->context()->native_context();
    return native_context->number_function()->initial_map();
  }

  // The object is either a number, a string, a symbol, a boolean, a SIMD value,
  // a real JS object, or a Harmony proxy.
  HeapObject* heap_object = HeapObject::cast(this);
  if (heap_object->IsJSReceiver()) {
    return heap_object->map();
  }
  int constructor_function_index =
      heap_object->map()->GetConstructorFunctionIndex();
  if (constructor_function_index != Map::kNoConstructorFunctionIndex) {
    Context* native_context = isolate->context()->native_context();
    JSFunction* constructor_function =
        JSFunction::cast(native_context->get(constructor_function_index));
    return constructor_function->initial_map();
  }
  return isolate->heap()->null_value()->map();
}


Object* Object::GetHash() {
  Object* hash = GetSimpleHash();
  if (hash->IsSmi()) return hash;

  DCHECK(IsJSReceiver());
  return JSReceiver::cast(this)->GetIdentityHash();
}


Object* Object::GetSimpleHash() {
  // The object is either a Smi, a HeapNumber, a name, an odd-ball,
  // a SIMD value type, a real JS object, or a Harmony proxy.
  if (IsSmi()) {
    uint32_t hash = ComputeIntegerHash(Smi::cast(this)->value(), kZeroHashSeed);
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(this)->value();
    if (std::isnan(num)) return Smi::FromInt(Smi::kMaxValue);
    if (i::IsMinusZero(num)) num = 0;
    if (IsSmiDouble(num)) {
      return Smi::FromInt(FastD2I(num))->GetHash();
    }
    uint32_t hash = ComputeLongHash(double_to_uint64(num));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (IsName()) {
    uint32_t hash = Name::cast(this)->Hash();
    return Smi::FromInt(hash);
  }
  if (IsOddball()) {
    uint32_t hash = Oddball::cast(this)->to_string()->Hash();
    return Smi::FromInt(hash);
  }
  if (IsSimd128Value()) {
    uint32_t hash = Simd128Value::cast(this)->Hash();
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  DCHECK(IsJSReceiver());
  JSReceiver* receiver = JSReceiver::cast(this);
  return receiver->GetHeap()->undefined_value();
}


Handle<Smi> Object::GetOrCreateHash(Isolate* isolate, Handle<Object> object) {
  Handle<Object> hash(object->GetSimpleHash(), isolate);
  if (hash->IsSmi()) return Handle<Smi>::cast(hash);

  DCHECK(object->IsJSReceiver());
  return JSReceiver::GetOrCreateIdentityHash(Handle<JSReceiver>::cast(object));
}


bool Object::SameValue(Object* other) {
  if (other == this) return true;

  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    // SameValue(NaN, NaN) is true.
    if (this_value != other_value) {
      return std::isnan(this_value) && std::isnan(other_value);
    }
    // SameValue(0.0, -0.0) is false.
    return (std::signbit(this_value) == std::signbit(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  if (IsFloat32x4() && other->IsFloat32x4()) {
    Float32x4* a = Float32x4::cast(this);
    Float32x4* b = Float32x4::cast(other);
    for (int i = 0; i < 4; i++) {
      float x = a->get_lane(i);
      float y = b->get_lane(i);
      // Implements the ES5 SameValue operation for floating point types.
      // http://www.ecma-international.org/ecma-262/6.0/#sec-samevalue
      if (x != y && !(std::isnan(x) && std::isnan(y))) return false;
      if (std::signbit(x) != std::signbit(y)) return false;
    }
    return true;
  } else if (IsSimd128Value() && other->IsSimd128Value()) {
    Simd128Value* a = Simd128Value::cast(this);
    Simd128Value* b = Simd128Value::cast(other);
    return a->map() == b->map() && a->BitwiseEquals(b);
  }
  return false;
}


bool Object::SameValueZero(Object* other) {
  if (other == this) return true;

  // The object is either a number, a name, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    // +0 == -0 is true
    return this_value == other_value ||
           (std::isnan(this_value) && std::isnan(other_value));
  }
  if (IsString() && other->IsString()) {
    return String::cast(this)->Equals(String::cast(other));
  }
  if (IsFloat32x4() && other->IsFloat32x4()) {
    Float32x4* a = Float32x4::cast(this);
    Float32x4* b = Float32x4::cast(other);
    for (int i = 0; i < 4; i++) {
      float x = a->get_lane(i);
      float y = b->get_lane(i);
      // Implements the ES6 SameValueZero operation for floating point types.
      // http://www.ecma-international.org/ecma-262/6.0/#sec-samevaluezero
      if (x != y && !(std::isnan(x) && std::isnan(y))) return false;
      // SameValueZero doesn't distinguish between 0 and -0.
    }
    return true;
  } else if (IsSimd128Value() && other->IsSimd128Value()) {
    Simd128Value* a = Simd128Value::cast(this);
    Simd128Value* b = Simd128Value::cast(other);
    return a->map() == b->map() && a->BitwiseEquals(b);
  }
  return false;
}


MaybeHandle<Object> Object::ArraySpeciesConstructor(
    Isolate* isolate, Handle<Object> original_array) {
  Handle<Context> native_context = isolate->native_context();
  Handle<Object> default_species = isolate->array_function();
  if (!FLAG_harmony_species) {
    return default_species;
  }
  if (original_array->IsJSArray() &&
      Handle<JSReceiver>::cast(original_array)->map()->new_target_is_base() &&
      isolate->IsArraySpeciesLookupChainIntact()) {
    return default_species;
  }
  Handle<Object> constructor = isolate->factory()->undefined_value();
  Maybe<bool> is_array = Object::IsArray(original_array);
  MAYBE_RETURN_NULL(is_array);
  if (is_array.FromJust()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, constructor,
        Object::GetProperty(original_array,
                            isolate->factory()->constructor_string()),
        Object);
    if (constructor->IsConstructor()) {
      Handle<Context> constructor_context;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, constructor_context,
          JSReceiver::GetFunctionRealm(Handle<JSReceiver>::cast(constructor)),
          Object);
      if (*constructor_context != *native_context &&
          *constructor == constructor_context->array_function()) {
        constructor = isolate->factory()->undefined_value();
      }
    }
    if (constructor->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, constructor,
          Object::GetProperty(constructor,
                              isolate->factory()->species_symbol()),
          Object);
      if (constructor->IsNull()) {
        constructor = isolate->factory()->undefined_value();
      }
    }
  }
  if (constructor->IsUndefined()) {
    return default_species;
  } else {
    if (!constructor->IsConstructor()) {
      THROW_NEW_ERROR(isolate,
          NewTypeError(MessageTemplate::kSpeciesNotConstructor),
          Object);
    }
    return constructor;
  }
}


void Object::ShortPrint(FILE* out) {
  OFStream os(out);
  os << Brief(this);
}


void Object::ShortPrint(StringStream* accumulator) {
  std::ostringstream os;
  os << Brief(this);
  accumulator->Add(os.str().c_str());
}


void Object::ShortPrint(std::ostream& os) { os << Brief(this); }


std::ostream& operator<<(std::ostream& os, const Brief& v) {
  if (v.value->IsSmi()) {
    Smi::cast(v.value)->SmiPrint(os);
  } else {
    // TODO(svenpanne) Const-correct HeapObjectShortPrint!
    HeapObject* obj = const_cast<HeapObject*>(HeapObject::cast(v.value));
    obj->HeapObjectShortPrint(os);
  }
  return os;
}


void Smi::SmiPrint(std::ostream& os) const {  // NOLINT
  os << value();
}


// Should a word be prefixed by 'a' or 'an' in order to read naturally in
// English?  Returns false for non-ASCII or words that don't start with
// a capital letter.  The a/an rule follows pronunciation in English.
// We don't use the BBC's overcorrect "an historic occasion" though if
// you speak a dialect you may well say "an 'istoric occasion".
static bool AnWord(String* str) {
  if (str->length() == 0) return false;  // A nothing.
  int c0 = str->Get(0);
  int c1 = str->length() > 1 ? str->Get(1) : 0;
  if (c0 == 'U') {
    if (c1 > 'Z') {
      return true;  // An Umpire, but a UTF8String, a U.
    }
  } else if (c0 == 'A' || c0 == 'E' || c0 == 'I' || c0 == 'O') {
    return true;    // An Ape, an ABCBook.
  } else if ((c1 == 0 || (c1 >= 'A' && c1 <= 'Z')) &&
           (c0 == 'F' || c0 == 'H' || c0 == 'M' || c0 == 'N' || c0 == 'R' ||
            c0 == 'S' || c0 == 'X')) {
    return true;    // An MP3File, an M.
  }
  return false;
}


Handle<String> String::SlowFlatten(Handle<ConsString> cons,
                                   PretenureFlag pretenure) {
  DCHECK(AllowHeapAllocation::IsAllowed());
  DCHECK(cons->second()->length() != 0);
  Isolate* isolate = cons->GetIsolate();
  int length = cons->length();
  PretenureFlag tenure = isolate->heap()->InNewSpace(*cons) ? pretenure
                                                            : TENURED;
  Handle<SeqString> result;
  if (cons->IsOneByteRepresentation()) {
    Handle<SeqOneByteString> flat = isolate->factory()->NewRawOneByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  } else {
    Handle<SeqTwoByteString> flat = isolate->factory()->NewRawTwoByteString(
        length, tenure).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    WriteToFlat(*cons, flat->GetChars(), 0, length);
    result = flat;
  }
  cons->set_first(*result);
  cons->set_second(isolate->heap()->empty_string());
  DCHECK(result->IsFlat());
  return result;
}



bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(!this->IsExternalString());
  DCHECK(!resource->IsCompressible());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    ScopedVector<uc16> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kShortSize) return false;
  Heap* heap = GetHeap();
  bool is_one_byte = this->IsOneByteRepresentation();
  bool is_internalized = this->IsInternalizedString();

  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular  external string.
  // Instead, we resort to a short external string instead, omitting
  // the field caching the address of the backing store.  When we encounter
  // short external strings in generated code, we need to bailout to runtime.
  Map* new_map;
  if (size < ExternalString::kSize) {
    new_map = is_internalized
        ? (is_one_byte
           ? heap->short_external_internalized_string_with_one_byte_data_map()
           : heap->short_external_internalized_string_map())
        : (is_one_byte ? heap->short_external_string_with_one_byte_data_map()
                       : heap->short_external_string_map());
  } else {
    new_map = is_internalized
        ? (is_one_byte
           ? heap->external_internalized_string_with_one_byte_data_map()
           : heap->external_internalized_string_map())
        : (is_one_byte ? heap->external_string_with_one_byte_data_map()
                       : heap->external_string_map());
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this, new_size - size, Heap::CONCURRENT_TO_SWEEPER);
  return true;
}


bool String::MakeExternal(v8::String::ExternalOneByteStringResource* resource) {
  // Externalizing twice leaks the external resource, so it's
  // prohibited by the API.
  DCHECK(!this->IsExternalString());
  DCHECK(!resource->IsCompressible());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Assert that the resource and the string are equivalent.
    DCHECK(static_cast<size_t>(this->length()) == resource->length());
    if (this->IsTwoByteRepresentation()) {
      ScopedVector<uint16_t> smart_chars(this->length());
      String::WriteToFlat(this, smart_chars.start(), 0, this->length());
      DCHECK(String::IsOneByte(smart_chars.start(), this->length()));
    }
    ScopedVector<char> smart_chars(this->length());
    String::WriteToFlat(this, smart_chars.start(), 0, this->length());
    DCHECK(memcmp(smart_chars.start(),
                  resource->data(),
                  resource->length() * sizeof(smart_chars[0])) == 0);
  }
#endif  // DEBUG
  int size = this->Size();  // Byte size of the original string.
  // Abort if size does not allow in-place conversion.
  if (size < ExternalString::kShortSize) return false;
  Heap* heap = GetHeap();
  bool is_internalized = this->IsInternalizedString();

  // Morph the string to an external string by replacing the map and
  // reinitializing the fields.  This won't work if the space the existing
  // string occupies is too small for a regular  external string.
  // Instead, we resort to a short external string instead, omitting
  // the field caching the address of the backing store.  When we encounter
  // short external strings in generated code, we need to bailout to runtime.
  Map* new_map;
  if (size < ExternalString::kSize) {
    new_map = is_internalized
                  ? heap->short_external_one_byte_internalized_string_map()
                  : heap->short_external_one_byte_string_map();
  } else {
    new_map = is_internalized
                  ? heap->external_one_byte_internalized_string_map()
                  : heap->external_one_byte_string_map();
  }

  // Byte size of the external String object.
  int new_size = this->SizeFromMap(new_map);
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size);

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalOneByteString* self = ExternalOneByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this, new_size - size, Heap::CONCURRENT_TO_SWEEPER);
  return true;
}


void String::StringShortPrint(StringStream* accumulator) {
  int len = length();
  if (len > kMaxShortPrintLength) {
    accumulator->Add("<Very long string[%u]>", len);
    return;
  }

  if (!LooksValid()) {
    accumulator->Add("<Invalid String>");
    return;
  }

  StringCharacterStream stream(this);

  bool truncated = false;
  if (len > kMaxShortPrintLength) {
    len = kMaxShortPrintLength;
    truncated = true;
  }
  bool one_byte = true;
  for (int i = 0; i < len; i++) {
    uint16_t c = stream.GetNext();

    if (c < 32 || c >= 127) {
      one_byte = false;
    }
  }
  stream.Reset(this);
  if (one_byte) {
    accumulator->Add("<String[%u]: ", length());
    for (int i = 0; i < len; i++) {
      accumulator->Put(static_cast<char>(stream.GetNext()));
    }
    accumulator->Put('>');
  } else {
    // Backslash indicates that the string contains control
    // characters and that backslashes are therefore escaped.
    accumulator->Add("<String[%u]\\: ", length());
    for (int i = 0; i < len; i++) {
      uint16_t c = stream.GetNext();
      if (c == '\n') {
        accumulator->Add("\\n");
      } else if (c == '\r') {
        accumulator->Add("\\r");
      } else if (c == '\\') {
        accumulator->Add("\\\\");
      } else if (c < 32 || c > 126) {
        accumulator->Add("\\x%02x", c);
      } else {
        accumulator->Put(static_cast<char>(c));
      }
    }
    if (truncated) {
      accumulator->Put('.');
      accumulator->Put('.');
      accumulator->Put('.');
    }
    accumulator->Put('>');
  }
  return;
}


void String::PrintUC16(std::ostream& os, int start, int end) {  // NOLINT
  if (end < 0) end = length();
  StringCharacterStream stream(this, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    os << AsUC16(stream.GetNext());
  }
}


void JSObject::JSObjectShortPrint(StringStream* accumulator) {
  switch (map()->instance_type()) {
    case JS_ARRAY_TYPE: {
      double length = JSArray::cast(this)->length()->IsUndefined()
          ? 0
          : JSArray::cast(this)->length()->Number();
      accumulator->Add("<JS Array[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      JSBoundFunction* bound_function = JSBoundFunction::cast(this);
      Object* name = bound_function->name();
      accumulator->Add("<JS BoundFunction");
      if (name->IsString()) {
        String* str = String::cast(name);
        if (str->length() > 0) {
          accumulator->Add(" ");
          accumulator->Put(str);
        }
      }
      accumulator->Add(
          " (BoundTargetFunction %p)>",
          reinterpret_cast<void*>(bound_function->bound_target_function()));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JS WeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JS WeakSet>");
      break;
    }
    case JS_REGEXP_TYPE: {
      accumulator->Add("<JS RegExp>");
      break;
    }
    case JS_FUNCTION_TYPE: {
      JSFunction* function = JSFunction::cast(this);
      Object* fun_name = function->shared()->DebugName();
      bool printed = false;
      if (fun_name->IsString()) {
        String* str = String::cast(fun_name);
        if (str->length() > 0) {
          accumulator->Add("<JS Function ");
          accumulator->Put(str);
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JS Function");
      }
      accumulator->Add(" (SharedFunctionInfo %p)",
                       reinterpret_cast<void*>(function->shared()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS Generator>");
      break;
    }
    case JS_MODULE_TYPE: {
      accumulator->Add("<JS Module>");
      break;
    }
    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectableObject, JSValue).
    default: {
      Map* map_of_this = map();
      Heap* heap = GetHeap();
      Object* constructor = map_of_this->GetConstructor();
      bool printed = false;
      if (constructor->IsHeapObject() &&
          !heap->Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool global_object = IsJSGlobalProxy();
        if (constructor->IsJSFunction()) {
          if (!heap->Contains(JSFunction::cast(constructor)->shared())) {
            accumulator->Add("!!!INVALID SHARED ON CONSTRUCTOR!!!");
          } else {
            Object* constructor_name =
                JSFunction::cast(constructor)->shared()->name();
            if (constructor_name->IsString()) {
              String* str = String::cast(constructor_name);
              if (str->length() > 0) {
                bool vowel = AnWord(str);
                accumulator->Add("<%sa%s ",
                       global_object ? "Global Object: " : "",
                       vowel ? "n" : "");
                accumulator->Put(str);
                accumulator->Add(" with %smap %p",
                    map_of_this->is_deprecated() ? "deprecated " : "",
                    map_of_this);
                printed = true;
              }
            }
          }
        }
        if (!printed) {
          accumulator->Add("<JS %sObject", global_object ? "Global " : "");
        }
      }
      if (IsJSValue()) {
        accumulator->Add(" value = ");
        JSValue::cast(this)->value()->ShortPrint(accumulator);
      }
      accumulator->Put('>');
      break;
    }
  }
}


void JSObject::PrintElementsTransition(
    FILE* file, Handle<JSObject> object,
    ElementsKind from_kind, Handle<FixedArrayBase> from_elements,
    ElementsKind to_kind, Handle<FixedArrayBase> to_elements) {
  if (from_kind != to_kind) {
    OFStream os(file);
    os << "elements transition [" << ElementsKindToString(from_kind) << " -> "
       << ElementsKindToString(to_kind) << "] in ";
    JavaScriptFrame::PrintTop(object->GetIsolate(), file, false, true);
    PrintF(file, " for ");
    object->ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
}


// static
MaybeHandle<JSFunction> Map::GetConstructorFunction(
    Handle<Map> map, Handle<Context> native_context) {
  if (map->IsPrimitiveMap()) {
    int const constructor_function_index = map->GetConstructorFunctionIndex();
    if (constructor_function_index != kNoConstructorFunctionIndex) {
      return handle(
          JSFunction::cast(native_context->get(constructor_function_index)));
    }
  }
  return MaybeHandle<JSFunction>();
}


void Map::PrintReconfiguration(FILE* file, int modify_index, PropertyKind kind,
                               PropertyAttributes attributes) {
  OFStream os(file);
  os << "[reconfiguring]";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ": " << (kind == kData ? "kData" : "ACCESSORS") << ", attrs: ";
  os << attributes << " [";
  JavaScriptFrame::PrintTop(GetIsolate(), file, false, true);
  os << "]\n";
}

void Map::PrintGeneralization(
    FILE* file, const char* reason, int modify_index, int split,
    int descriptors, bool constant_to_field, Representation old_representation,
    Representation new_representation, MaybeHandle<FieldType> old_field_type,
    MaybeHandle<Object> old_value, MaybeHandle<FieldType> new_field_type,
    MaybeHandle<Object> new_value) {
  OFStream os(file);
  os << "[generalizing]";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ":";
  if (constant_to_field) {
    os << "c";
  } else {
    os << old_representation.Mnemonic() << "{";
    if (old_field_type.is_null()) {
      os << Brief(*(old_value.ToHandleChecked()));
    } else {
      old_field_type.ToHandleChecked()->PrintTo(os);
    }
    os << "}";
  }
  os << "->" << new_representation.Mnemonic() << "{";
  if (new_field_type.is_null()) {
    os << Brief(*(new_value.ToHandleChecked()));
  } else {
    new_field_type.ToHandleChecked()->PrintTo(os);
  }
  os << "} (";
  if (strlen(reason) > 0) {
    os << reason;
  } else {
    os << "+" << (descriptors - split) << " maps";
  }
  os << ") [";
  JavaScriptFrame::PrintTop(GetIsolate(), file, false, true);
  os << "]\n";
}


void JSObject::PrintInstanceMigration(FILE* file,
                                      Map* original_map,
                                      Map* new_map) {
  PrintF(file, "[migrating]");
  DescriptorArray* o = original_map->instance_descriptors();
  DescriptorArray* n = new_map->instance_descriptors();
  for (int i = 0; i < original_map->NumberOfOwnDescriptors(); i++) {
    Representation o_r = o->GetDetails(i).representation();
    Representation n_r = n->GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o->GetKey(i))->PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o->GetDetails(i).type() == DATA_CONSTANT &&
               n->GetDetails(i).type() == DATA) {
      Name* name = o->GetKey(i);
      if (name->IsString()) {
        String::cast(name)->PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", static_cast<void*>(name));
      }
      PrintF(file, " ");
    }
  }
  PrintF(file, "\n");
}


void HeapObject::HeapObjectShortPrint(std::ostream& os) {  // NOLINT
  Heap* heap = GetHeap();
  if (!heap->Contains(this)) {
    os << "!!!INVALID POINTER!!!";
    return;
  }
  if (!heap->Contains(map())) {
    os << "!!!INVALID MAP!!!";
    return;
  }

  os << this << " ";

  if (IsString()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(this)->StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObject::cast(this)->JSObjectShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  switch (map()->instance_type()) {
    case MAP_TYPE:
      os << "<Map(" << ElementsKindToString(Map::cast(this)->elements_kind())
         << ")>";
      break;
    case FIXED_ARRAY_TYPE:
      os << "<FixedArray[" << FixedArray::cast(this)->length() << "]>";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      os << "<FixedDoubleArray[" << FixedDoubleArray::cast(this)->length()
         << "]>";
      break;
    case BYTE_ARRAY_TYPE:
      os << "<ByteArray[" << ByteArray::cast(this)->length() << "]>";
      break;
    case BYTECODE_ARRAY_TYPE:
      os << "<BytecodeArray[" << BytecodeArray::cast(this)->length() << "]>";
      break;
    case TRANSITION_ARRAY_TYPE:
      os << "<TransitionArray[" << TransitionArray::cast(this)->length()
         << "]>";
      break;
    case FREE_SPACE_TYPE:
      os << "<FreeSpace[" << FreeSpace::cast(this)->size() << "]>";
      break;
#define TYPED_ARRAY_SHORT_PRINT(Type, type, TYPE, ctype, size)                \
  case FIXED_##TYPE##_ARRAY_TYPE:                                             \
    os << "<Fixed" #Type "Array[" << Fixed##Type##Array::cast(this)->length() \
       << "]>";                                                               \
    break;

    TYPED_ARRAYS(TYPED_ARRAY_SHORT_PRINT)
#undef TYPED_ARRAY_SHORT_PRINT

    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(this);
      base::SmartArrayPointer<char> debug_name =
          shared->DebugName()->ToCString();
      if (debug_name[0] != 0) {
        os << "<SharedFunctionInfo " << debug_name.get() << ">";
      } else {
        os << "<SharedFunctionInfo>";
      }
      break;
    }
    case JS_MESSAGE_OBJECT_TYPE:
      os << "<JSMessageObject>";
      break;
#define MAKE_STRUCT_CASE(NAME, Name, name) \
  case NAME##_TYPE:                        \
    os << "<" #Name ">";                   \
    break;
  STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case CODE_TYPE: {
      Code* code = Code::cast(this);
      os << "<Code: " << Code::Kind2String(code->kind()) << ">";
      break;
    }
    case ODDBALL_TYPE: {
      if (IsUndefined()) {
        os << "<undefined>";
      } else if (IsTheHole()) {
        os << "<the hole>";
      } else if (IsNull()) {
        os << "<null>";
      } else if (IsTrue()) {
        os << "<true>";
      } else if (IsFalse()) {
        os << "<false>";
      } else {
        os << "<Odd Oddball>";
      }
      break;
    }
    case SYMBOL_TYPE: {
      Symbol* symbol = Symbol::cast(this);
      symbol->SymbolShortPrint(os);
      break;
    }
    case HEAP_NUMBER_TYPE: {
      os << "<Number: ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">";
      break;
    }
    case MUTABLE_HEAP_NUMBER_TYPE: {
      os << "<MutableNumber: ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << '>';
      break;
    }
    case SIMD128_VALUE_TYPE: {
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type) \
  if (Is##Type()) {                                           \
    os << "<" #Type ">";                                      \
    break;                                                    \
  }
      SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
      UNREACHABLE();
      break;
    }
    case JS_PROXY_TYPE:
      os << "<JSProxy>";
      break;
    case FOREIGN_TYPE:
      os << "<Foreign>";
      break;
    case CELL_TYPE: {
      os << "Cell for ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Cell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      break;
    }
    case PROPERTY_CELL_TYPE: {
      os << "PropertyCell for ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      PropertyCell* cell = PropertyCell::cast(this);
      cell->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      break;
    }
    case WEAK_CELL_TYPE: {
      os << "WeakCell for ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      WeakCell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      break;
    }
    default:
      os << "<Other heap object (" << map()->instance_type() << ")>";
      break;
  }
}


void HeapObject::Iterate(ObjectVisitor* v) { IterateFast<ObjectVisitor>(v); }


void HeapObject::IterateBody(ObjectVisitor* v) {
  Map* m = map();
  IterateBodyFast<ObjectVisitor>(m->instance_type(), SizeFromMap(m), v);
}


void HeapObject::IterateBody(InstanceType type, int object_size,
                             ObjectVisitor* v) {
  IterateBodyFast<ObjectVisitor>(type, object_size, v);
}


struct CallIsValidSlot {
  template <typename BodyDescriptor>
  static bool apply(HeapObject* obj, int offset, int) {
    return BodyDescriptor::IsValidSlot(obj, offset);
  }
};


bool HeapObject::IsValidSlot(int offset) {
  DCHECK_NE(0, offset);
  return BodyDescriptorApply<CallIsValidSlot, bool>(map()->instance_type(),
                                                    this, offset, 0);
}


bool HeapNumber::HeapNumberBooleanValue() {
  return DoubleToBoolean(value());
}


void HeapNumber::HeapNumberPrint(std::ostream& os) {  // NOLINT
  os << value();
}


#define FIELD_ADDR_CONST(p, offset) \
  (reinterpret_cast<const byte*>(p) + offset - kHeapObjectTag)

#define READ_INT32_FIELD(p, offset) \
  (*reinterpret_cast<const int32_t*>(FIELD_ADDR_CONST(p, offset)))

#define READ_INT64_FIELD(p, offset) \
  (*reinterpret_cast<const int64_t*>(FIELD_ADDR_CONST(p, offset)))

#define READ_BYTE_FIELD(p, offset) \
  (*reinterpret_cast<const byte*>(FIELD_ADDR_CONST(p, offset)))


// static
Handle<String> Simd128Value::ToString(Handle<Simd128Value> input) {
#define SIMD128_TYPE(TYPE, Type, type, lane_count, lane_type) \
  if (input->Is##Type()) return Type::ToString(Handle<Type>::cast(input));
  SIMD128_TYPES(SIMD128_TYPE)
#undef SIMD128_TYPE
  UNREACHABLE();
  return Handle<String>::null();
}


// static
Handle<String> Float32x4::ToString(Handle<Float32x4> input) {
  Isolate* const isolate = input->GetIsolate();
  char arr[100];
  Vector<char> buffer(arr, arraysize(arr));
  std::ostringstream os;
  os << "SIMD.Float32x4("
     << std::string(DoubleToCString(input->get_lane(0), buffer)) << ", "
     << std::string(DoubleToCString(input->get_lane(1), buffer)) << ", "
     << std::string(DoubleToCString(input->get_lane(2), buffer)) << ", "
     << std::string(DoubleToCString(input->get_lane(3), buffer)) << ")";
  return isolate->factory()->NewStringFromAsciiChecked(os.str().c_str());
}


#define SIMD128_BOOL_TO_STRING(Type, lane_count)                            \
  Handle<String> Type::ToString(Handle<Type> input) {                       \
    Isolate* const isolate = input->GetIsolate();                           \
    std::ostringstream os;                                                  \
    os << "SIMD." #Type "(";                                                \
    os << (input->get_lane(0) ? "true" : "false");                          \
    for (int i = 1; i < lane_count; i++) {                                  \
      os << ", " << (input->get_lane(i) ? "true" : "false");                \
    }                                                                       \
    os << ")";                                                              \
    return isolate->factory()->NewStringFromAsciiChecked(os.str().c_str()); \
  }
SIMD128_BOOL_TO_STRING(Bool32x4, 4)
SIMD128_BOOL_TO_STRING(Bool16x8, 8)
SIMD128_BOOL_TO_STRING(Bool8x16, 16)
#undef SIMD128_BOOL_TO_STRING


#define SIMD128_INT_TO_STRING(Type, lane_count)                             \
  Handle<String> Type::ToString(Handle<Type> input) {                       \
    Isolate* const isolate = input->GetIsolate();                           \
    char arr[100];                                                          \
    Vector<char> buffer(arr, arraysize(arr));                               \
    std::ostringstream os;                                                  \
    os << "SIMD." #Type "(";                                                \
    os << IntToCString(input->get_lane(0), buffer);                         \
    for (int i = 1; i < lane_count; i++) {                                  \
      os << ", " << IntToCString(input->get_lane(i), buffer);               \
    }                                                                       \
    os << ")";                                                              \
    return isolate->factory()->NewStringFromAsciiChecked(os.str().c_str()); \
  }
SIMD128_INT_TO_STRING(Int32x4, 4)
SIMD128_INT_TO_STRING(Uint32x4, 4)
SIMD128_INT_TO_STRING(Int16x8, 8)
SIMD128_INT_TO_STRING(Uint16x8, 8)
SIMD128_INT_TO_STRING(Int8x16, 16)
SIMD128_INT_TO_STRING(Uint8x16, 16)
#undef SIMD128_INT_TO_STRING


bool Simd128Value::BitwiseEquals(const Simd128Value* other) const {
  return READ_INT64_FIELD(this, kValueOffset) ==
             READ_INT64_FIELD(other, kValueOffset) &&
         READ_INT64_FIELD(this, kValueOffset + kInt64Size) ==
             READ_INT64_FIELD(other, kValueOffset + kInt64Size);
}


uint32_t Simd128Value::Hash() const {
  uint32_t seed = v8::internal::kZeroHashSeed;
  uint32_t hash;
  hash = ComputeIntegerHash(READ_INT32_FIELD(this, kValueOffset), seed);
  hash = ComputeIntegerHash(
      READ_INT32_FIELD(this, kValueOffset + 1 * kInt32Size), hash * 31);
  hash = ComputeIntegerHash(
      READ_INT32_FIELD(this, kValueOffset + 2 * kInt32Size), hash * 31);
  hash = ComputeIntegerHash(
      READ_INT32_FIELD(this, kValueOffset + 3 * kInt32Size), hash * 31);
  return hash;
}


void Simd128Value::CopyBits(void* destination) const {
  memcpy(destination, &READ_BYTE_FIELD(this, kValueOffset), kSimd128Size);
}


String* JSReceiver::class_name() {
  if (IsFunction()) {
    return GetHeap()->Function_string();
  }
  Object* maybe_constructor = map()->GetConstructor();
  if (maybe_constructor->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(maybe_constructor);
    return String::cast(constructor->shared()->instance_class_name());
  }
  // If the constructor is not present, return "Object".
  return GetHeap()->Object_string();
}


MaybeHandle<String> JSReceiver::BuiltinStringTag(Handle<JSReceiver> object) {
  Maybe<bool> is_array = Object::IsArray(object);
  MAYBE_RETURN(is_array, MaybeHandle<String>());
  Isolate* const isolate = object->GetIsolate();
  if (is_array.FromJust()) {
    return isolate->factory()->Array_string();
  }
  // TODO(adamk): According to ES2015, we should return "Function" when
  // object has a [[Call]] internal method (corresponds to IsCallable).
  // But this is well cemented in layout tests and might cause webbreakage.
  // if (object->IsCallable()) {
  //   return isolate->factory()->Function_string();
  // }
  // TODO(adamk): class_name() is expensive, replace with instance type
  // checks where possible.
  return handle(object->class_name(), isolate);
}


// static
Handle<String> JSReceiver::GetConstructorName(Handle<JSReceiver> receiver) {
  Isolate* isolate = receiver->GetIsolate();

  // If the object was instantiated simply with base == new.target, the
  // constructor on the map provides the most accurate name.
  // Don't provide the info for prototypes, since their constructors are
  // reclaimed and replaced by Object in OptimizeAsPrototype.
  if (!receiver->IsJSProxy() && receiver->map()->new_target_is_base() &&
      !receiver->map()->is_prototype_map()) {
    Object* maybe_constructor = receiver->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      String* name = String::cast(constructor->shared()->name());
      if (name->length() == 0) name = constructor->shared()->inferred_name();
      if (name->length() != 0 &&
          !name->Equals(isolate->heap()->Object_string())) {
        return handle(name, isolate);
      }
    }
  }

  if (FLAG_harmony_tostring) {
    Handle<Object> maybe_tag = JSReceiver::GetDataProperty(
        receiver, isolate->factory()->to_string_tag_symbol());
    if (maybe_tag->IsString()) return Handle<String>::cast(maybe_tag);
  }

  PrototypeIterator iter(isolate, receiver);
  if (iter.IsAtEnd()) return handle(receiver->class_name());
  Handle<JSReceiver> start = PrototypeIterator::GetCurrent<JSReceiver>(iter);
  LookupIterator it(receiver, isolate->factory()->constructor_string(), start,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Handle<Object> maybe_constructor = JSReceiver::GetDataProperty(&it);
  Handle<String> result = isolate->factory()->Object_string();
  if (maybe_constructor->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(*maybe_constructor);
    String* name = String::cast(constructor->shared()->name());
    if (name->length() == 0) name = constructor->shared()->inferred_name();
    if (name->length() > 0) result = handle(name, isolate);
  }

  return result.is_identical_to(isolate->factory()->Object_string())
             ? handle(receiver->class_name())
             : result;
}


Context* JSReceiver::GetCreationContext() {
  JSReceiver* receiver = this;
  while (receiver->IsJSBoundFunction()) {
    receiver = JSBoundFunction::cast(receiver)->bound_target_function();
  }
  Object* constructor = receiver->map()->GetConstructor();
  JSFunction* function;
  if (constructor->IsJSFunction()) {
    function = JSFunction::cast(constructor);
  } else {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    CHECK(receiver->IsJSFunction());
    function = JSFunction::cast(receiver);
  }

  return function->context()->native_context();
}

static Handle<Object> WrapType(Handle<FieldType> type) {
  if (type->IsClass()) return Map::WeakCellForMap(type->AsClass());
  return type;
}

MaybeHandle<Map> Map::CopyWithField(Handle<Map> map, Handle<Name> name,
                                    Handle<FieldType> type,
                                    PropertyAttributes attributes,
                                    Representation representation,
                                    TransitionFlag flag) {
  DCHECK(DescriptorArray::kNotFound ==
         map->instance_descriptors()->Search(
             *name, map->NumberOfOwnDescriptors()));

  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  Isolate* isolate = map->GetIsolate();

  // Compute the new index for new field.
  int index = map->NextFreePropertyIndex();

  if (map->instance_type() == JS_CONTEXT_EXTENSION_OBJECT_TYPE) {
    representation = Representation::Tagged();
    type = FieldType::Any(isolate);
  }

  Handle<Object> wrapped_type(WrapType(type));

  DataDescriptor new_field_desc(name, index, wrapped_type, attributes,
                                representation);
  Handle<Map> new_map = Map::CopyAddDescriptor(map, &new_field_desc, flag);
  int unused_property_fields = new_map->unused_property_fields() - 1;
  if (unused_property_fields < 0) {
    unused_property_fields += JSObject::kFieldsAdded;
  }
  new_map->set_unused_property_fields(unused_property_fields);
  return new_map;
}


MaybeHandle<Map> Map::CopyWithConstant(Handle<Map> map,
                                       Handle<Name> name,
                                       Handle<Object> constant,
                                       PropertyAttributes attributes,
                                       TransitionFlag flag) {
  // Ensure the descriptor array does not get too big.
  if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors) {
    return MaybeHandle<Map>();
  }

  // Allocate new instance descriptors with (name, constant) added.
  DataConstantDescriptor new_constant_desc(name, constant, attributes);
  return Map::CopyAddDescriptor(map, &new_constant_desc, flag);
}


void JSObject::AddSlowProperty(Handle<JSObject> object,
                               Handle<Name> name,
                               Handle<Object> value,
                               PropertyAttributes attributes) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();
  if (object->IsJSGlobalObject()) {
    Handle<GlobalDictionary> dict(object->global_dictionary());
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    int entry = dict->FindEntry(name);
    // If there's a cell there, just invalidate and set the property.
    if (entry != GlobalDictionary::kNotFound) {
      PropertyCell::UpdateCell(dict, entry, value, details);
      // TODO(ishell): move this to UpdateCell.
      // Need to adjust the details.
      int index = dict->NextEnumerationIndex();
      dict->SetNextEnumerationIndex(index + 1);
      PropertyCell* cell = PropertyCell::cast(dict->ValueAt(entry));
      details = cell->property_details().set_index(index);
      cell->set_property_details(details);

    } else {
      auto cell = isolate->factory()->NewPropertyCell();
      cell->set_value(*value);
      auto cell_type = value->IsUndefined() ? PropertyCellType::kUndefined
                                            : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      value = cell;

      Handle<GlobalDictionary> result =
          GlobalDictionary::Add(dict, name, value, details);
      if (*dict != *result) object->set_properties(*result);
    }
  } else {
    Handle<NameDictionary> dict(object->property_dictionary());
    PropertyDetails details(attributes, DATA, 0, PropertyCellType::kNoCell);
    Handle<NameDictionary> result =
        NameDictionary::Add(dict, name, value, details);
    if (*dict != *result) object->set_properties(*result);
  }
}


MaybeHandle<Object> JSObject::EnqueueChangeRecord(Handle<JSObject> object,
                                                  const char* type_str,
                                                  Handle<Name> name,
                                                  Handle<Object> old_value) {
  DCHECK(!object->IsJSGlobalProxy());
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<String> type = isolate->factory()->InternalizeUtf8String(type_str);
  Handle<Object> args[] = { type, object, name, old_value };
  int argc = name.is_null() ? 2 : old_value->IsTheHole() ? 3 : 4;

  return Execution::Call(isolate,
                         Handle<JSFunction>(isolate->observers_notify_change()),
                         isolate->factory()->undefined_value(), argc, args);
}


const char* Representation::Mnemonic() const {
  switch (kind_) {
    case kNone: return "v";
    case kTagged: return "t";
    case kSmi: return "s";
    case kDouble: return "d";
    case kInteger32: return "i";
    case kHeapObject: return "h";
    case kExternal: return "x";
    default:
      UNREACHABLE();
      return NULL;
  }
}

bool Map::InstancesNeedRewriting(Map* target) {
  int target_number_of_fields = target->NumberOfFields();
  int target_inobject = target->GetInObjectProperties();
  int target_unused = target->unused_property_fields();
  int old_number_of_fields;

  return InstancesNeedRewriting(target, target_number_of_fields,
                                target_inobject, target_unused,
                                &old_number_of_fields);
}

bool Map::InstancesNeedRewriting(Map* target, int target_number_of_fields,
                                 int target_inobject, int target_unused,
                                 int* old_number_of_fields) {
  // If fields were added (or removed), rewrite the instance.
  *old_number_of_fields = NumberOfFields();
  DCHECK(target_number_of_fields >= *old_number_of_fields);
  if (target_number_of_fields != *old_number_of_fields) return true;

  // If smi descriptors were replaced by double descriptors, rewrite.
  DescriptorArray* old_desc = instance_descriptors();
  DescriptorArray* new_desc = target->instance_descriptors();
  int limit = NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if (new_desc->GetDetails(i).representation().IsDouble() !=
        old_desc->GetDetails(i).representation().IsDouble()) {
      return true;
    }
  }

  // If no fields were added, and no inobject properties were removed, setting
  // the map is sufficient.
  if (target_inobject == GetInObjectProperties()) return false;
  // In-object slack tracking may have reduced the object size of the new map.
  // In that case, succeed if all existing fields were inobject, and they still
  // fit within the new inobject size.
  DCHECK(target_inobject < GetInObjectProperties());
  if (target_number_of_fields <= target_inobject) {
    DCHECK(target_number_of_fields + target_unused == target_inobject);
    return false;
  }
  // Otherwise, properties will need to be moved to the backing store.
  return true;
}


// static
void JSObject::UpdatePrototypeUserRegistration(Handle<Map> old_map,
                                               Handle<Map> new_map,
                                               Isolate* isolate) {
  if (!FLAG_track_prototype_users) return;
  if (!old_map->is_prototype_map()) return;
  DCHECK(new_map->is_prototype_map());
  bool was_registered = JSObject::UnregisterPrototypeUser(old_map, isolate);
  new_map->set_prototype_info(old_map->prototype_info());
  old_map->set_prototype_info(Smi::FromInt(0));
  if (FLAG_trace_prototype_users) {
    PrintF("Moving prototype_info %p from map %p to map %p.\n",
           reinterpret_cast<void*>(new_map->prototype_info()),
           reinterpret_cast<void*>(*old_map),
           reinterpret_cast<void*>(*new_map));
  }
  if (was_registered) {
    if (new_map->prototype_info()->IsPrototypeInfo()) {
      // The new map isn't registered with its prototype yet; reflect this fact
      // in the PrototypeInfo it just inherited from the old map.
      PrototypeInfo::cast(new_map->prototype_info())
          ->set_registry_slot(PrototypeInfo::UNREGISTERED);
    }
    JSObject::LazyRegisterPrototypeUser(new_map, isolate);
  }
}

namespace {
// To migrate a fast instance to a fast map:
// - First check whether the instance needs to be rewritten. If not, simply
//   change the map.
// - Otherwise, allocate a fixed array large enough to hold all fields, in
//   addition to unused space.
// - Copy all existing properties in, in the following order: backing store
//   properties, unused fields, inobject properties.
// - If all allocation succeeded, commit the state atomically:
//   * Copy inobject properties from the backing store back into the object.
//   * Trim the difference in instance size of the object. This also cleanly
//     frees inobject properties that moved to the backing store.
//   * If there are properties left in the backing store, trim of the space used
//     to temporarily store the inobject properties.
//   * If there are properties left in the backing store, install the backing
//     store.
void MigrateFastToFast(Handle<JSObject> object, Handle<Map> new_map) {
  Isolate* isolate = object->GetIsolate();
  Handle<Map> old_map(object->map());
  // In case of a regular transition.
  if (new_map->GetBackPointer() == *old_map) {
    // If the map does not add named properties, simply set the map.
    if (old_map->NumberOfOwnDescriptors() ==
        new_map->NumberOfOwnDescriptors()) {
      object->synchronized_set_map(*new_map);
      return;
    }

    PropertyDetails details = new_map->GetLastDescriptorDetails();
    // Either new_map adds an kDescriptor property, or a kField property for
    // which there is still space, and which does not require a mutable double
    // box (an out-of-object double).
    if (details.location() == kDescriptor ||
        (old_map->unused_property_fields() > 0 &&
         ((FLAG_unbox_double_fields && object->properties()->length() == 0) ||
          !details.representation().IsDouble()))) {
      object->synchronized_set_map(*new_map);
      return;
    }

    // If there is still space in the object, we need to allocate a mutable
    // double box.
    if (old_map->unused_property_fields() > 0) {
      FieldIndex index =
          FieldIndex::ForDescriptor(*new_map, new_map->LastAdded());
      DCHECK(details.representation().IsDouble());
      DCHECK(!new_map->IsUnboxedDoubleField(index));
      Handle<Object> value = isolate->factory()->NewHeapNumber(0, MUTABLE);
      object->RawFastPropertyAtPut(index, *value);
      object->synchronized_set_map(*new_map);
      return;
    }

    // This migration is a transition from a map that has run out of property
    // space. Extend the backing store.
    int grow_by = new_map->unused_property_fields() + 1;
    Handle<FixedArray> old_storage = handle(object->properties(), isolate);
    Handle<FixedArray> new_storage =
        isolate->factory()->CopyFixedArrayAndGrow(old_storage, grow_by);

    // Properly initialize newly added property.
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumber(0, MUTABLE);
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK_EQ(DATA, details.type());
    int target_index = details.field_index() - new_map->GetInObjectProperties();
    DCHECK(target_index >= 0);  // Must be a backing store index.
    new_storage->set(target_index, *value);

    // From here on we cannot fail and we shouldn't GC anymore.
    DisallowHeapAllocation no_allocation;

    // Set the new property value and do the map transition.
    object->set_properties(*new_storage);
    object->synchronized_set_map(*new_map);
    return;
  }

  int old_number_of_fields;
  int number_of_fields = new_map->NumberOfFields();
  int inobject = new_map->GetInObjectProperties();
  int unused = new_map->unused_property_fields();

  // Nothing to do if no functions were converted to fields and no smis were
  // converted to doubles.
  if (!old_map->InstancesNeedRewriting(*new_map, number_of_fields, inobject,
                                       unused, &old_number_of_fields)) {
    object->synchronized_set_map(*new_map);
    return;
  }

  int total_size = number_of_fields + unused;
  int external = total_size - inobject;

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(total_size);

  Handle<DescriptorArray> old_descriptors(old_map->instance_descriptors());
  Handle<DescriptorArray> new_descriptors(new_map->instance_descriptors());
  int old_nof = old_map->NumberOfOwnDescriptors();
  int new_nof = new_map->NumberOfOwnDescriptors();

  // This method only supports generalizing instances to at least the same
  // number of properties.
  DCHECK(old_nof <= new_nof);

  for (int i = 0; i < old_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.type() != DATA) continue;
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Representation old_representation = old_details.representation();
    Representation representation = details.representation();
    Handle<Object> value;
    if (old_details.type() == ACCESSOR_CONSTANT) {
      // In case of kAccessor -> kData property reconfiguration, the property
      // must already be prepared for data or certain type.
      DCHECK(!details.representation().IsNone());
      if (details.representation().IsDouble()) {
        value = isolate->factory()->NewHeapNumber(0, MUTABLE);
      } else {
        value = isolate->factory()->uninitialized_value();
      }
    } else if (old_details.type() == DATA_CONSTANT) {
      value = handle(old_descriptors->GetValue(i), isolate);
      DCHECK(!old_representation.IsDouble() && !representation.IsDouble());
    } else {
      FieldIndex index = FieldIndex::ForDescriptor(*old_map, i);
      if (object->IsUnboxedDoubleField(index)) {
        double old = object->RawFastDoublePropertyAt(index);
        value = isolate->factory()->NewHeapNumber(
            old, representation.IsDouble() ? MUTABLE : IMMUTABLE);

      } else {
        value = handle(object->RawFastPropertyAt(index), isolate);
        if (!old_representation.IsDouble() && representation.IsDouble()) {
          if (old_representation.IsNone()) {
            value = handle(Smi::FromInt(0), isolate);
          }
          value = Object::NewStorageFor(isolate, value, representation);
        } else if (old_representation.IsDouble() &&
                   !representation.IsDouble()) {
          value = Object::WrapForRead(isolate, value, old_representation);
        }
      }
    }
    DCHECK(!(representation.IsDouble() && value->IsSmi()));
    int target_index = new_descriptors->GetFieldIndex(i) - inobject;
    if (target_index < 0) target_index += total_size;
    array->set(target_index, *value);
  }

  for (int i = old_nof; i < new_nof; i++) {
    PropertyDetails details = new_descriptors->GetDetails(i);
    if (details.type() != DATA) continue;
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewHeapNumber(0, MUTABLE);
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    int target_index = new_descriptors->GetFieldIndex(i) - inobject;
    if (target_index < 0) target_index += total_size;
    array->set(target_index, *value);
  }

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();

  // Copy (real) inobject properties. If necessary, stop at number_of_fields to
  // avoid overwriting |one_pointer_filler_map|.
  int limit = Min(inobject, number_of_fields);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    Object* value = array->get(external + i);
    // Can't use JSObject::FastPropertyAtPut() because proper map was not set
    // yet.
    if (new_map->IsUnboxedDoubleField(index)) {
      DCHECK(value->IsMutableHeapNumber());
      object->RawFastDoublePropertyAtPut(index,
                                         HeapNumber::cast(value)->value());
      if (i < old_number_of_fields && !old_map->IsUnboxedDoubleField(index)) {
        // Transition from tagged to untagged slot.
        heap->ClearRecordedSlot(*object,
                                HeapObject::RawField(*object, index.offset()));
      }
    } else {
      object->RawFastPropertyAtPut(index, value);
    }
  }


  // If there are properties in the new backing store, trim it to the correct
  // size and install the backing store into the object.
  if (external > 0) {
    heap->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(*array, inobject);
    object->set_properties(*array);
  }

  // Create filler object past the new instance size.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(
        address + new_instance_size, instance_size_delta);
    heap->AdjustLiveBytes(*object, -instance_size_delta,
                          Heap::CONCURRENT_TO_SWEEPER);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);
}

void MigrateFastToSlow(Handle<JSObject> object, Handle<Map> new_map,
                       int expected_additional_properties) {
  // The global object is always normalized.
  DCHECK(!object->IsJSGlobalObject());
  // JSGlobalProxy must never be normalized
  DCHECK(!object->IsJSGlobalProxy());

  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Map> map(object->map());

  // Allocate new content.
  int real_size = map->NumberOfOwnDescriptors();
  int property_count = real_size;
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    property_count += 2;  // Make space for two more properties.
  }
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate, property_count);

  Handle<DescriptorArray> descs(map->instance_descriptors());
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Name> key(descs->GetKey(i));
    switch (details.type()) {
      case DATA_CONSTANT: {
        Handle<Object> value(descs->GetConstant(i), isolate);
        PropertyDetails d(details.attributes(), DATA, i + 1,
                          PropertyCellType::kNoCell);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case DATA: {
        FieldIndex index = FieldIndex::ForDescriptor(*map, i);
        Handle<Object> value;
        if (object->IsUnboxedDoubleField(index)) {
          double old_value = object->RawFastDoublePropertyAt(index);
          value = isolate->factory()->NewHeapNumber(old_value);
        } else {
          value = handle(object->RawFastPropertyAt(index), isolate);
          if (details.representation().IsDouble()) {
            DCHECK(value->IsMutableHeapNumber());
            Handle<HeapNumber> old = Handle<HeapNumber>::cast(value);
            value = isolate->factory()->NewHeapNumber(old->value());
          }
        }
        PropertyDetails d(details.attributes(), DATA, i + 1,
                          PropertyCellType::kNoCell);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case ACCESSOR: {
        FieldIndex index = FieldIndex::ForDescriptor(*map, i);
        Handle<Object> value(object->RawFastPropertyAt(index), isolate);
        PropertyDetails d(details.attributes(), ACCESSOR_CONSTANT, i + 1,
                          PropertyCellType::kNoCell);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
      case ACCESSOR_CONSTANT: {
        Handle<Object> value(descs->GetCallbacksObject(i), isolate);
        PropertyDetails d(details.attributes(), ACCESSOR_CONSTANT, i + 1,
                          PropertyCellType::kNoCell);
        dictionary = NameDictionary::Add(dictionary, key, value, d);
        break;
      }
    }
  }

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  // Resize the object in the heap if necessary.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    Heap* heap = isolate->heap();
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta);
    heap->AdjustLiveBytes(*object, -instance_size_delta,
                          Heap::CONCURRENT_TO_SWEEPER);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);

  object->set_properties(*dictionary);

  // Ensure that in-object space of slow-mode object does not contain random
  // garbage.
  int inobject_properties = new_map->GetInObjectProperties();
  for (int i = 0; i < inobject_properties; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
    object->RawFastPropertyAtPut(index, Smi::FromInt(0));
  }

  isolate->counters()->props_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    OFStream os(stdout);
    os << "Object properties have been normalized:\n";
    object->Print(os);
  }
#endif
}

}  // namespace

void JSObject::MigrateToMap(Handle<JSObject> object, Handle<Map> new_map,
                            int expected_additional_properties) {
  if (object->map() == *new_map) return;
  Handle<Map> old_map(object->map());
  if (old_map->is_prototype_map()) {
    // If this object is a prototype (the callee will check), invalidate any
    // prototype chains involving it.
    InvalidatePrototypeChains(object->map());

    // If the map was registered with its prototype before, ensure that it
    // registers with its new prototype now. This preserves the invariant that
    // when a map on a prototype chain is registered with its prototype, then
    // all prototypes further up the chain are also registered with their
    // respective prototypes.
    UpdatePrototypeUserRegistration(old_map, new_map, new_map->GetIsolate());
  }

  if (old_map->is_dictionary_map()) {
    // For slow-to-fast migrations JSObject::MigrateSlowToFast()
    // must be used instead.
    CHECK(new_map->is_dictionary_map());

    // Slow-to-slow migration is trivial.
    object->set_map(*new_map);
  } else if (!new_map->is_dictionary_map()) {
    MigrateFastToFast(object, new_map);
    if (old_map->is_prototype_map()) {
      DCHECK(!old_map->is_stable());
      DCHECK(new_map->is_stable());
      // Clear out the old descriptor array to avoid problems to sharing
      // the descriptor array without using an explicit.
      old_map->InitializeDescriptors(
          old_map->GetHeap()->empty_descriptor_array(),
          LayoutDescriptor::FastPointerLayout());
      // Ensure that no transition was inserted for prototype migrations.
      DCHECK_EQ(
          0, TransitionArray::NumberOfTransitions(old_map->raw_transitions()));
      DCHECK(new_map->GetBackPointer()->IsUndefined());
    }
  } else {
    MigrateFastToSlow(object, new_map, expected_additional_properties);
  }

  // Careful: Don't allocate here!
  // For some callers of this method, |object| might be in an inconsistent
  // state now: the new map might have a new elements_kind, but the object's
  // elements pointer hasn't been updated yet. Callers will fix this, but in
  // the meantime, (indirectly) calling JSObjectVerify() must be avoided.
  // When adding code here, add a DisallowHeapAllocation too.
}

int Map::NumberOfFields() {
  DescriptorArray* descriptors = instance_descriptors();
  int result = 0;
  for (int i = 0; i < NumberOfOwnDescriptors(); i++) {
    if (descriptors->GetDetails(i).location() == kField) result++;
  }
  return result;
}

Handle<Map> Map::CopyGeneralizeAllRepresentations(
    Handle<Map> map, ElementsKind elements_kind, int modify_index,
    StoreMode store_mode, PropertyKind kind, PropertyAttributes attributes,
    const char* reason) {
  Isolate* isolate = map->GetIsolate();
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> descriptors =
      DescriptorArray::CopyUpTo(old_descriptors, number_of_own_descriptors);

  for (int i = 0; i < number_of_own_descriptors; i++) {
    descriptors->SetRepresentation(i, Representation::Tagged());
    if (descriptors->GetDetails(i).type() == DATA) {
      descriptors->SetValue(i, FieldType::Any());
    }
  }

  Handle<LayoutDescriptor> new_layout_descriptor(
      LayoutDescriptor::FastPointerLayout(), isolate);
  Handle<Map> new_map = CopyReplaceDescriptors(
      map, descriptors, new_layout_descriptor, OMIT_TRANSITION,
      MaybeHandle<Name>(), reason, SPECIAL_TRANSITION);

  // Unless the instance is being migrated, ensure that modify_index is a field.
  if (modify_index >= 0) {
    PropertyDetails details = descriptors->GetDetails(modify_index);
    if (store_mode == FORCE_FIELD &&
        (details.type() != DATA || details.attributes() != attributes)) {
      int field_index = details.type() == DATA ? details.field_index()
                                               : new_map->NumberOfFields();
      DataDescriptor d(handle(descriptors->GetKey(modify_index), isolate),
                       field_index, attributes, Representation::Tagged());
      descriptors->Replace(modify_index, &d);
      if (details.type() != DATA) {
        int unused_property_fields = new_map->unused_property_fields() - 1;
        if (unused_property_fields < 0) {
          unused_property_fields += JSObject::kFieldsAdded;
        }
        new_map->set_unused_property_fields(unused_property_fields);
      }
    } else {
      DCHECK(details.attributes() == attributes);
    }

    if (FLAG_trace_generalization) {
      MaybeHandle<FieldType> field_type = FieldType::None(isolate);
      if (details.type() == DATA) {
        field_type = handle(
            map->instance_descriptors()->GetFieldType(modify_index), isolate);
      }
      map->PrintGeneralization(
          stdout, reason, modify_index, new_map->NumberOfOwnDescriptors(),
          new_map->NumberOfOwnDescriptors(),
          details.type() == DATA_CONSTANT && store_mode == FORCE_FIELD,
          details.representation(), Representation::Tagged(), field_type,
          MaybeHandle<Object>(), FieldType::Any(isolate),
          MaybeHandle<Object>());
    }
  }
  new_map->set_elements_kind(elements_kind);
  return new_map;
}


void Map::DeprecateTransitionTree() {
  if (is_deprecated()) return;
  Object* transitions = raw_transitions();
  int num_transitions = TransitionArray::NumberOfTransitions(transitions);
  for (int i = 0; i < num_transitions; ++i) {
    TransitionArray::GetTarget(transitions, i)->DeprecateTransitionTree();
  }
  deprecate();
  dependent_code()->DeoptimizeDependentCodeGroup(
      GetIsolate(), DependentCode::kTransitionGroup);
  NotifyLeafMapLayoutChange();
}


static inline bool EqualImmutableValues(Object* obj1, Object* obj2) {
  if (obj1 == obj2) return true;  // Valid for both kData and kAccessor kinds.
  // TODO(ishell): compare AccessorPairs.
  return false;
}


// Installs |new_descriptors| over the current instance_descriptors to ensure
// proper sharing of descriptor arrays.
void Map::ReplaceDescriptors(DescriptorArray* new_descriptors,
                             LayoutDescriptor* new_layout_descriptor) {
  // Don't overwrite the empty descriptor array or initial map's descriptors.
  if (NumberOfOwnDescriptors() == 0 || GetBackPointer()->IsUndefined()) {
    return;
  }

  DescriptorArray* to_replace = instance_descriptors();
  GetHeap()->incremental_marking()->RecordWrites(to_replace);
  Map* current = this;
  while (current->instance_descriptors() == to_replace) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined()) break;  // Stop overwriting at initial map.
    current->SetEnumLength(kInvalidEnumCacheSentinel);
    current->UpdateDescriptors(new_descriptors, new_layout_descriptor);
    current = Map::cast(next);
  }
  set_owns_descriptors(false);
}


Map* Map::FindRootMap() {
  Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined()) {
      // Initial map always owns descriptors and doesn't have unused entries
      // in the descriptor array.
      DCHECK(result->owns_descriptors());
      DCHECK_EQ(result->NumberOfOwnDescriptors(),
                result->instance_descriptors()->number_of_descriptors());
      return result;
    }
    result = Map::cast(back);
  }
}


Map* Map::FindLastMatchMap(int verbatim,
                           int length,
                           DescriptorArray* descriptors) {
  DisallowHeapAllocation no_allocation;

  // This can only be called on roots of transition trees.
  DCHECK_EQ(verbatim, NumberOfOwnDescriptors());

  Map* current = this;

  for (int i = verbatim; i < length; i++) {
    Name* name = descriptors->GetKey(i);
    PropertyDetails details = descriptors->GetDetails(i);
    Map* next = TransitionArray::SearchTransition(current, details.kind(), name,
                                                  details.attributes());
    if (next == NULL) break;
    DescriptorArray* next_descriptors = next->instance_descriptors();

    PropertyDetails next_details = next_descriptors->GetDetails(i);
    DCHECK_EQ(details.kind(), next_details.kind());
    DCHECK_EQ(details.attributes(), next_details.attributes());
    if (details.location() != next_details.location()) break;
    if (!details.representation().Equals(next_details.representation())) break;

    if (next_details.location() == kField) {
      FieldType* next_field_type = next_descriptors->GetFieldType(i);
      if (!descriptors->GetFieldType(i)->NowIs(next_field_type)) {
        break;
      }
    } else {
      if (!EqualImmutableValues(descriptors->GetValue(i),
                                next_descriptors->GetValue(i))) {
        break;
      }
    }
    current = next;
  }
  return current;
}


Map* Map::FindFieldOwner(int descriptor) {
  DisallowHeapAllocation no_allocation;
  DCHECK_EQ(DATA, instance_descriptors()->GetDetails(descriptor).type());
  Map* result = this;
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined()) break;
    Map* parent = Map::cast(back);
    if (parent->NumberOfOwnDescriptors() <= descriptor) break;
    result = parent;
  }
  return result;
}


void Map::UpdateFieldType(int descriptor, Handle<Name> name,
                          Representation new_representation,
                          Handle<Object> new_wrapped_type) {
  DCHECK(new_wrapped_type->IsSmi() || new_wrapped_type->IsWeakCell());
  DisallowHeapAllocation no_allocation;
  PropertyDetails details = instance_descriptors()->GetDetails(descriptor);
  if (details.type() != DATA) return;
  Object* transitions = raw_transitions();
  int num_transitions = TransitionArray::NumberOfTransitions(transitions);
  for (int i = 0; i < num_transitions; ++i) {
    Map* target = TransitionArray::GetTarget(transitions, i);
    target->UpdateFieldType(descriptor, name, new_representation,
                            new_wrapped_type);
  }
  // It is allowed to change representation here only from None to something.
  DCHECK(details.representation().Equals(new_representation) ||
         details.representation().IsNone());

  // Skip if already updated the shared descriptor.
  if (instance_descriptors()->GetValue(descriptor) == *new_wrapped_type) return;
  DataDescriptor d(name, instance_descriptors()->GetFieldIndex(descriptor),
                   new_wrapped_type, details.attributes(), new_representation);
  instance_descriptors()->Replace(descriptor, &d);
}

bool FieldTypeIsCleared(Representation rep, FieldType* type) {
  return type->IsNone() && rep.IsHeapObject();
}


// static
Handle<FieldType> Map::GeneralizeFieldType(Representation rep1,
                                           Handle<FieldType> type1,
                                           Representation rep2,
                                           Handle<FieldType> type2,
                                           Isolate* isolate) {
  // Cleared field types need special treatment. They represent lost knowledge,
  // so we must be conservative, so their generalization with any other type
  // is "Any".
  if (FieldTypeIsCleared(rep1, *type1) || FieldTypeIsCleared(rep2, *type2)) {
    return FieldType::Any(isolate);
  }
  if (type1->NowIs(type2)) return type2;
  if (type2->NowIs(type1)) return type1;
  return FieldType::Any(isolate);
}


// static
void Map::GeneralizeFieldType(Handle<Map> map, int modify_index,
                              Representation new_representation,
                              Handle<FieldType> new_field_type) {
  Isolate* isolate = map->GetIsolate();

  // Check if we actually need to generalize the field type at all.
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  Representation old_representation =
      old_descriptors->GetDetails(modify_index).representation();
  Handle<FieldType> old_field_type(old_descriptors->GetFieldType(modify_index),
                                   isolate);

  if (old_representation.Equals(new_representation) &&
      !FieldTypeIsCleared(new_representation, *new_field_type) &&
      // Checking old_field_type for being cleared is not necessary because
      // the NowIs check below would fail anyway in that case.
      new_field_type->NowIs(old_field_type)) {
    DCHECK(Map::GeneralizeFieldType(old_representation, old_field_type,
                                    new_representation, new_field_type, isolate)
               ->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(modify_index), isolate);
  Handle<DescriptorArray> descriptors(
      field_owner->instance_descriptors(), isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  new_field_type =
      Map::GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate);

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index));

  Handle<Object> wrapped_type(WrapType(new_field_type));
  field_owner->UpdateFieldType(modify_index, name, new_representation,
                               wrapped_type);
  field_owner->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kFieldTypeGroup);

  if (FLAG_trace_generalization) {
    map->PrintGeneralization(
        stdout, "field type generalization", modify_index,
        map->NumberOfOwnDescriptors(), map->NumberOfOwnDescriptors(), false,
        details.representation(), details.representation(), old_field_type,
        MaybeHandle<Object>(), new_field_type, MaybeHandle<Object>());
  }
}

static inline Handle<FieldType> GetFieldType(
    Isolate* isolate, Handle<DescriptorArray> descriptors, int descriptor,
    PropertyLocation location, Representation representation) {
#ifdef DEBUG
  PropertyDetails details = descriptors->GetDetails(descriptor);
  DCHECK_EQ(kData, details.kind());
  DCHECK_EQ(details.location(), location);
#endif
  if (location == kField) {
    return handle(descriptors->GetFieldType(descriptor), isolate);
  } else {
    return descriptors->GetValue(descriptor)
        ->OptimalType(isolate, representation);
  }
}

// Reconfigures elements kind to |new_elements_kind| and/or property at
// |modify_index| with |new_kind|, |new_attributes|, |store_mode| and/or
// |new_representation|/|new_field_type|.
// If |modify_index| is negative then no properties are reconfigured but the
// map is migrated to the up-to-date non-deprecated state.
//
// This method rewrites or completes the transition tree to reflect the new
// change. To avoid high degrees over polymorphism, and to stabilize quickly,
// on every rewrite the new type is deduced by merging the current type with
// any potential new (partial) version of the type in the transition tree.
// To do this, on each rewrite:
// - Search the root of the transition tree using FindRootMap.
// - Find/create a |root_map| with requested |new_elements_kind|.
// - Find |target_map|, the newest matching version of this map using the
//   virtually "enhanced" |old_map|'s descriptor array (i.e. whose entry at
//   |modify_index| is considered to be of |new_kind| and having
//   |new_attributes|) to walk the transition tree.
// - Merge/generalize the "enhanced" descriptor array of the |old_map| and
//   descriptor array of the |target_map|.
// - Generalize the |modify_index| descriptor using |new_representation| and
//   |new_field_type|.
// - Walk the tree again starting from the root towards |target_map|. Stop at
//   |split_map|, the first map who's descriptor array does not match the merged
//   descriptor array.
// - If |target_map| == |split_map|, |target_map| is in the expected state.
//   Return it.
// - Otherwise, invalidate the outdated transition target from |target_map|, and
//   replace its transition tree with a new branch for the updated descriptors.
Handle<Map> Map::Reconfigure(Handle<Map> old_map,
                             ElementsKind new_elements_kind, int modify_index,
                             PropertyKind new_kind,
                             PropertyAttributes new_attributes,
                             Representation new_representation,
                             Handle<FieldType> new_field_type,
                             StoreMode store_mode) {
  DCHECK_NE(kAccessor, new_kind);  // TODO(ishell): not supported yet.
  DCHECK(store_mode != FORCE_FIELD || modify_index >= 0);
  Isolate* isolate = old_map->GetIsolate();

  Handle<DescriptorArray> old_descriptors(
      old_map->instance_descriptors(), isolate);
  int old_nof = old_map->NumberOfOwnDescriptors();

  // If it's just a representation generalization case (i.e. property kind and
  // attributes stays unchanged) it's fine to transition from None to anything
  // but double without any modification to the object, because the default
  // uninitialized value for representation None can be overwritten by both
  // smi and tagged values. Doubles, however, would require a box allocation.
  if (modify_index >= 0 && !new_representation.IsNone() &&
      !new_representation.IsDouble() &&
      old_map->elements_kind() == new_elements_kind) {
    PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
    Representation old_representation = old_details.representation();

    if (old_representation.IsNone()) {
      DCHECK_EQ(new_kind, old_details.kind());
      DCHECK_EQ(new_attributes, old_details.attributes());
      DCHECK_EQ(DATA, old_details.type());
      if (FLAG_trace_generalization) {
        old_map->PrintGeneralization(
            stdout, "uninitialized field", modify_index,
            old_map->NumberOfOwnDescriptors(),
            old_map->NumberOfOwnDescriptors(), false, old_representation,
            new_representation,
            handle(old_descriptors->GetFieldType(modify_index), isolate),
            MaybeHandle<Object>(), new_field_type, MaybeHandle<Object>());
      }
      Handle<Map> field_owner(old_map->FindFieldOwner(modify_index), isolate);

      GeneralizeFieldType(field_owner, modify_index, new_representation,
                          new_field_type);
      DCHECK(old_descriptors->GetDetails(modify_index)
                 .representation()
                 .Equals(new_representation));
      DCHECK(
          old_descriptors->GetFieldType(modify_index)->NowIs(new_field_type));
      return old_map;
    }
  }

  // Check the state of the root map.
  Handle<Map> root_map(old_map->FindRootMap(), isolate);
  if (!old_map->EquivalentToForTransition(*root_map)) {
    return CopyGeneralizeAllRepresentations(
        old_map, new_elements_kind, modify_index, store_mode, new_kind,
        new_attributes, "GenAll_NotEquivalent");
  }

  ElementsKind from_kind = root_map->elements_kind();
  ElementsKind to_kind = new_elements_kind;
  // TODO(ishell): Add a test for SLOW_SLOPPY_ARGUMENTS_ELEMENTS.
  if (from_kind != to_kind && to_kind != DICTIONARY_ELEMENTS &&
      to_kind != SLOW_STRING_WRAPPER_ELEMENTS &&
      to_kind != SLOW_SLOPPY_ARGUMENTS_ELEMENTS &&
      !(IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind))) {
    return CopyGeneralizeAllRepresentations(
        old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
        "GenAll_InvalidElementsTransition");
  }
  int root_nof = root_map->NumberOfOwnDescriptors();
  if (modify_index >= 0 && modify_index < root_nof) {
    PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
    if (old_details.kind() != new_kind ||
        old_details.attributes() != new_attributes) {
      return CopyGeneralizeAllRepresentations(
          old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
          "GenAll_RootModification1");
    }
    if ((old_details.type() != DATA && store_mode == FORCE_FIELD) ||
        (old_details.type() == DATA &&
         (!new_field_type->NowIs(old_descriptors->GetFieldType(modify_index)) ||
          !new_representation.fits_into(old_details.representation())))) {
      return CopyGeneralizeAllRepresentations(
          old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
          "GenAll_RootModification2");
    }
  }

  // From here on, use the map with correct elements kind as root map.
  if (from_kind != to_kind) {
    root_map = Map::AsElementsKind(root_map, to_kind);
  }

  Handle<Map> target_map = root_map;
  for (int i = root_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyKind next_kind;
    PropertyLocation next_location;
    PropertyAttributes next_attributes;
    Representation next_representation;
    bool property_kind_reconfiguration = false;

    if (modify_index == i) {
      DCHECK_EQ(FORCE_FIELD, store_mode);
      property_kind_reconfiguration = old_details.kind() != new_kind;

      next_kind = new_kind;
      next_location = kField;
      next_attributes = new_attributes;
      // If property kind is not reconfigured merge the result with
      // representation/field type from the old descriptor.
      next_representation = new_representation;
      if (!property_kind_reconfiguration) {
        next_representation =
            next_representation.generalize(old_details.representation());
      }

    } else {
      next_kind = old_details.kind();
      next_location = old_details.location();
      next_attributes = old_details.attributes();
      next_representation = old_details.representation();
    }
    Map* transition = TransitionArray::SearchTransition(
        *target_map, next_kind, old_descriptors->GetKey(i), next_attributes);
    if (transition == NULL) break;
    Handle<Map> tmp_map(transition, isolate);

    Handle<DescriptorArray> tmp_descriptors = handle(
        tmp_map->instance_descriptors(), isolate);

    // Check if target map is incompatible.
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(next_kind, tmp_details.kind());
    DCHECK_EQ(next_attributes, tmp_details.attributes());
    if (next_kind == kAccessor &&
        !EqualImmutableValues(old_descriptors->GetValue(i),
                              tmp_descriptors->GetValue(i))) {
      return CopyGeneralizeAllRepresentations(
          old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
          "GenAll_Incompatible");
    }
    if (next_location == kField && tmp_details.location() == kDescriptor) break;

    Representation tmp_representation = tmp_details.representation();
    if (!next_representation.fits_into(tmp_representation)) break;

    PropertyLocation old_location = old_details.location();
    PropertyLocation tmp_location = tmp_details.location();
    if (tmp_location == kField) {
      if (next_kind == kData) {
        Handle<FieldType> next_field_type;
        if (modify_index == i) {
          next_field_type = new_field_type;
          if (!property_kind_reconfiguration) {
            Handle<FieldType> old_field_type =
                GetFieldType(isolate, old_descriptors, i,
                             old_details.location(), tmp_representation);
            Representation old_representation = old_details.representation();
            next_field_type = GeneralizeFieldType(
                old_representation, old_field_type, new_representation,
                next_field_type, isolate);
          }
        } else {
          Handle<FieldType> old_field_type =
              GetFieldType(isolate, old_descriptors, i, old_details.location(),
                           tmp_representation);
          next_field_type = old_field_type;
        }
        GeneralizeFieldType(tmp_map, i, tmp_representation, next_field_type);
      }
    } else if (old_location == kField ||
               !EqualImmutableValues(old_descriptors->GetValue(i),
                                     tmp_descriptors->GetValue(i))) {
      break;
    }
    DCHECK(!tmp_map->is_deprecated());
    target_map = tmp_map;
  }

  // Directly change the map if the target map is more general.
  Handle<DescriptorArray> target_descriptors(
      target_map->instance_descriptors(), isolate);
  int target_nof = target_map->NumberOfOwnDescriptors();
  if (target_nof == old_nof &&
      (store_mode != FORCE_FIELD ||
       (modify_index >= 0 &&
        target_descriptors->GetDetails(modify_index).location() == kField))) {
#ifdef DEBUG
    if (modify_index >= 0) {
      PropertyDetails details = target_descriptors->GetDetails(modify_index);
      DCHECK_EQ(new_kind, details.kind());
      DCHECK_EQ(new_attributes, details.attributes());
      DCHECK(new_representation.fits_into(details.representation()));
      DCHECK(details.location() != kField ||
             new_field_type->NowIs(
                 target_descriptors->GetFieldType(modify_index)));
    }
#endif
    if (*target_map != *old_map) {
      old_map->NotifyLeafMapLayoutChange();
    }
    return target_map;
  }

  // Find the last compatible target map in the transition tree.
  for (int i = target_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyKind next_kind;
    PropertyAttributes next_attributes;
    if (modify_index == i) {
      next_kind = new_kind;
      next_attributes = new_attributes;
    } else {
      next_kind = old_details.kind();
      next_attributes = old_details.attributes();
    }
    Map* transition = TransitionArray::SearchTransition(
        *target_map, next_kind, old_descriptors->GetKey(i), next_attributes);
    if (transition == NULL) break;
    Handle<Map> tmp_map(transition, isolate);
    Handle<DescriptorArray> tmp_descriptors(
        tmp_map->instance_descriptors(), isolate);

    // Check if target map is compatible.
#ifdef DEBUG
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(next_kind, tmp_details.kind());
    DCHECK_EQ(next_attributes, tmp_details.attributes());
#endif
    if (next_kind == kAccessor &&
        !EqualImmutableValues(old_descriptors->GetValue(i),
                              tmp_descriptors->GetValue(i))) {
      return CopyGeneralizeAllRepresentations(
          old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
          "GenAll_Incompatible");
    }
    DCHECK(!tmp_map->is_deprecated());
    target_map = tmp_map;
  }
  target_nof = target_map->NumberOfOwnDescriptors();
  target_descriptors = handle(target_map->instance_descriptors(), isolate);

  // Allocate a new descriptor array large enough to hold the required
  // descriptors, with minimally the exact same size as the old descriptor
  // array.
  int new_slack = Max(
      old_nof, old_descriptors->number_of_descriptors()) - old_nof;
  Handle<DescriptorArray> new_descriptors = DescriptorArray::Allocate(
      isolate, old_nof, new_slack);
  DCHECK(new_descriptors->length() > target_descriptors->length() ||
         new_descriptors->NumberOfSlackDescriptors() > 0 ||
         new_descriptors->number_of_descriptors() ==
         old_descriptors->number_of_descriptors());
  DCHECK(new_descriptors->number_of_descriptors() == old_nof);

  // 0 -> |root_nof|
  int current_offset = 0;
  for (int i = 0; i < root_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    if (old_details.location() == kField) {
      current_offset += old_details.field_width_in_words();
    }
    Descriptor d(handle(old_descriptors->GetKey(i), isolate),
                 handle(old_descriptors->GetValue(i), isolate),
                 old_details);
    new_descriptors->Set(i, &d);
  }

  // |root_nof| -> |target_nof|
  for (int i = root_nof; i < target_nof; ++i) {
    Handle<Name> target_key(target_descriptors->GetKey(i), isolate);
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    PropertyDetails target_details = target_descriptors->GetDetails(i);

    PropertyKind next_kind;
    PropertyAttributes next_attributes;
    PropertyLocation next_location;
    Representation next_representation;
    bool property_kind_reconfiguration = false;

    if (modify_index == i) {
      DCHECK_EQ(FORCE_FIELD, store_mode);
      property_kind_reconfiguration = old_details.kind() != new_kind;

      next_kind = new_kind;
      next_attributes = new_attributes;
      next_location = kField;

      // Merge new representation/field type with ones from the target
      // descriptor. If property kind is not reconfigured merge the result with
      // representation/field type from the old descriptor.
      next_representation =
          new_representation.generalize(target_details.representation());
      if (!property_kind_reconfiguration) {
        next_representation =
            next_representation.generalize(old_details.representation());
      }
    } else {
      // Merge old_descriptor and target_descriptor entries.
      DCHECK_EQ(target_details.kind(), old_details.kind());
      next_kind = target_details.kind();
      next_attributes = target_details.attributes();
      next_location =
          old_details.location() == kField ||
                  target_details.location() == kField ||
                  !EqualImmutableValues(target_descriptors->GetValue(i),
                                        old_descriptors->GetValue(i))
              ? kField
              : kDescriptor;

      next_representation = old_details.representation().generalize(
          target_details.representation());
    }
    DCHECK_EQ(next_kind, target_details.kind());
    DCHECK_EQ(next_attributes, target_details.attributes());

    if (next_location == kField) {
      if (next_kind == kData) {
        Handle<FieldType> target_field_type =
            GetFieldType(isolate, target_descriptors, i,
                         target_details.location(), next_representation);

        Handle<FieldType> next_field_type;
        if (modify_index == i) {
          next_field_type = GeneralizeFieldType(
              target_details.representation(), target_field_type,
              new_representation, new_field_type, isolate);
          if (!property_kind_reconfiguration) {
            Handle<FieldType> old_field_type =
                GetFieldType(isolate, old_descriptors, i,
                             old_details.location(), next_representation);
            next_field_type = GeneralizeFieldType(
                old_details.representation(), old_field_type,
                next_representation, next_field_type, isolate);
          }
        } else {
          Handle<FieldType> old_field_type =
              GetFieldType(isolate, old_descriptors, i, old_details.location(),
                           next_representation);
          next_field_type = GeneralizeFieldType(
              old_details.representation(), old_field_type, next_representation,
              target_field_type, isolate);
        }
        Handle<Object> wrapped_type(WrapType(next_field_type));
        DataDescriptor d(target_key, current_offset, wrapped_type,
                         next_attributes, next_representation);
        current_offset += d.GetDetails().field_width_in_words();
        new_descriptors->Set(i, &d);
      } else {
        UNIMPLEMENTED();  // TODO(ishell): implement.
      }
    } else {
      PropertyDetails details(next_attributes, next_kind, next_location,
                              next_representation);
      Descriptor d(target_key, handle(target_descriptors->GetValue(i), isolate),
                   details);
      new_descriptors->Set(i, &d);
    }
  }

  // |target_nof| -> |old_nof|
  for (int i = target_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Handle<Name> old_key(old_descriptors->GetKey(i), isolate);

    // Merge old_descriptor entry and modified details together.
    PropertyKind next_kind;
    PropertyAttributes next_attributes;
    PropertyLocation next_location;
    Representation next_representation;
    bool property_kind_reconfiguration = false;

    if (modify_index == i) {
      DCHECK_EQ(FORCE_FIELD, store_mode);
      // In case of property kind reconfiguration it is not necessary to
      // take into account representation/field type of the old descriptor.
      property_kind_reconfiguration = old_details.kind() != new_kind;

      next_kind = new_kind;
      next_attributes = new_attributes;
      next_location = kField;
      next_representation = new_representation;
      if (!property_kind_reconfiguration) {
        next_representation =
            next_representation.generalize(old_details.representation());
      }
    } else {
      next_kind = old_details.kind();
      next_attributes = old_details.attributes();
      next_location = old_details.location();
      next_representation = old_details.representation();
    }

    if (next_location == kField) {
      if (next_kind == kData) {
        Handle<FieldType> next_field_type;
        if (modify_index == i) {
          next_field_type = new_field_type;
          if (!property_kind_reconfiguration) {
            Handle<FieldType> old_field_type =
                GetFieldType(isolate, old_descriptors, i,
                             old_details.location(), next_representation);
            next_field_type = GeneralizeFieldType(
                old_details.representation(), old_field_type,
                next_representation, next_field_type, isolate);
          }
        } else {
          Handle<FieldType> old_field_type =
              GetFieldType(isolate, old_descriptors, i, old_details.location(),
                           next_representation);
          next_field_type = old_field_type;
        }

        Handle<Object> wrapped_type(WrapType(next_field_type));

        DataDescriptor d(old_key, current_offset, wrapped_type, next_attributes,
                         next_representation);
        current_offset += d.GetDetails().field_width_in_words();
        new_descriptors->Set(i, &d);
      } else {
        UNIMPLEMENTED();  // TODO(ishell): implement.
      }
    } else {
      PropertyDetails details(next_attributes, next_kind, next_location,
                              next_representation);
      Descriptor d(old_key, handle(old_descriptors->GetValue(i), isolate),
                   details);
      new_descriptors->Set(i, &d);
    }
  }

  new_descriptors->Sort();

  DCHECK(store_mode != FORCE_FIELD ||
         new_descriptors->GetDetails(modify_index).location() == kField);

  Handle<Map> split_map(root_map->FindLastMatchMap(
          root_nof, old_nof, *new_descriptors), isolate);
  int split_nof = split_map->NumberOfOwnDescriptors();
  DCHECK_NE(old_nof, split_nof);

  PropertyKind split_kind;
  PropertyAttributes split_attributes;
  if (modify_index == split_nof) {
    split_kind = new_kind;
    split_attributes = new_attributes;
  } else {
    PropertyDetails split_prop_details = old_descriptors->GetDetails(split_nof);
    split_kind = split_prop_details.kind();
    split_attributes = split_prop_details.attributes();
  }

  // Invalidate a transition target at |key|.
  Map* maybe_transition = TransitionArray::SearchTransition(
      *split_map, split_kind, old_descriptors->GetKey(split_nof),
      split_attributes);
  if (maybe_transition != NULL) {
    maybe_transition->DeprecateTransitionTree();
  }

  // If |maybe_transition| is not NULL then the transition array already
  // contains entry for given descriptor. This means that the transition
  // could be inserted regardless of whether transitions array is full or not.
  if (maybe_transition == NULL &&
      !TransitionArray::CanHaveMoreTransitions(split_map)) {
    return CopyGeneralizeAllRepresentations(
        old_map, to_kind, modify_index, store_mode, new_kind, new_attributes,
        "GenAll_CantHaveMoreTransitions");
  }

  old_map->NotifyLeafMapLayoutChange();

  if (FLAG_trace_generalization && modify_index >= 0) {
    PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
    PropertyDetails new_details = new_descriptors->GetDetails(modify_index);
    MaybeHandle<FieldType> old_field_type;
    MaybeHandle<FieldType> new_field_type;
    MaybeHandle<Object> old_value;
    MaybeHandle<Object> new_value;
    if (old_details.type() == DATA) {
      old_field_type =
          handle(old_descriptors->GetFieldType(modify_index), isolate);
    } else {
      old_value = handle(old_descriptors->GetValue(modify_index), isolate);
    }
    if (new_details.type() == DATA) {
      new_field_type =
          handle(new_descriptors->GetFieldType(modify_index), isolate);
    } else {
      new_value = handle(new_descriptors->GetValue(modify_index), isolate);
    }

    old_map->PrintGeneralization(
        stdout, "", modify_index, split_nof, old_nof,
        old_details.location() == kDescriptor && store_mode == FORCE_FIELD,
        old_details.representation(), new_details.representation(),
        old_field_type, old_value, new_field_type, new_value);
  }

  Handle<LayoutDescriptor> new_layout_descriptor =
      LayoutDescriptor::New(split_map, new_descriptors, old_nof);

  Handle<Map> new_map =
      AddMissingTransitions(split_map, new_descriptors, new_layout_descriptor);

  // Deprecated part of the transition tree is no longer reachable, so replace
  // current instance descriptors in the "survived" part of the tree with
  // the new descriptors to maintain descriptors sharing invariant.
  split_map->ReplaceDescriptors(*new_descriptors, *new_layout_descriptor);
  return new_map;
}


// Generalize the representation of all DATA descriptors.
Handle<Map> Map::GeneralizeAllFieldRepresentations(
    Handle<Map> map) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); ++i) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.type() == DATA) {
      map = ReconfigureProperty(map, i, kData, details.attributes(),
                                Representation::Tagged(),
                                FieldType::Any(map->GetIsolate()), FORCE_FIELD);
    }
  }
  return map;
}


// static
MaybeHandle<Map> Map::TryUpdate(Handle<Map> old_map) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(old_map->GetIsolate());

  if (!old_map->is_deprecated()) return old_map;

  // Check the state of the root map.
  Map* root_map = old_map->FindRootMap();
  if (!old_map->EquivalentToForTransition(root_map)) return MaybeHandle<Map>();

  ElementsKind from_kind = root_map->elements_kind();
  ElementsKind to_kind = old_map->elements_kind();
  if (from_kind != to_kind) {
    // Try to follow existing elements kind transitions.
    root_map = root_map->LookupElementsTransitionMap(to_kind);
    if (root_map == NULL) return MaybeHandle<Map>();
    // From here on, use the map with correct elements kind as root map.
  }
  Map* new_map = root_map->TryReplayPropertyTransitions(*old_map);
  if (new_map == nullptr) return MaybeHandle<Map>();
  return handle(new_map);
}

Map* Map::TryReplayPropertyTransitions(Map* old_map) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(GetIsolate());

  int root_nof = NumberOfOwnDescriptors();

  int old_nof = old_map->NumberOfOwnDescriptors();
  DescriptorArray* old_descriptors = old_map->instance_descriptors();

  Map* new_map = this;
  for (int i = root_nof; i < old_nof; ++i) {
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Map* transition = TransitionArray::SearchTransition(
        new_map, old_details.kind(), old_descriptors->GetKey(i),
        old_details.attributes());
    if (transition == NULL) return nullptr;
    new_map = transition;
    DescriptorArray* new_descriptors = new_map->instance_descriptors();

    PropertyDetails new_details = new_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), new_details.kind());
    DCHECK_EQ(old_details.attributes(), new_details.attributes());
    if (!old_details.representation().fits_into(new_details.representation())) {
      return nullptr;
    }
    switch (new_details.type()) {
      case DATA: {
        FieldType* new_type = new_descriptors->GetFieldType(i);
        // Cleared field types need special treatment. They represent lost
        // knowledge, so we must first generalize the new_type to "Any".
        if (FieldTypeIsCleared(new_details.representation(), new_type)) {
          return nullptr;
        }
        PropertyType old_property_type = old_details.type();
        if (old_property_type == DATA) {
          FieldType* old_type = old_descriptors->GetFieldType(i);
          if (FieldTypeIsCleared(old_details.representation(), old_type) ||
              !old_type->NowIs(new_type)) {
            return nullptr;
          }
        } else {
          DCHECK(old_property_type == DATA_CONSTANT);
          Object* old_value = old_descriptors->GetValue(i);
          if (!new_type->NowContains(old_value)) {
            return nullptr;
          }
        }
        break;
      }
      case ACCESSOR: {
#ifdef DEBUG
        FieldType* new_type = new_descriptors->GetFieldType(i);
        DCHECK(new_type->IsAny());
#endif
        break;
      }

      case DATA_CONSTANT:
      case ACCESSOR_CONSTANT: {
        Object* old_value = old_descriptors->GetValue(i);
        Object* new_value = new_descriptors->GetValue(i);
        if (old_details.location() == kField || old_value != new_value) {
          return nullptr;
        }
        break;
      }
    }
  }
  if (new_map->NumberOfOwnDescriptors() != old_nof) return nullptr;
  return new_map;
}


// static
Handle<Map> Map::Update(Handle<Map> map) {
  if (!map->is_deprecated()) return map;
  return ReconfigureProperty(map, -1, kData, NONE, Representation::None(),
                             FieldType::None(map->GetIsolate()),
                             ALLOW_IN_DESCRIPTOR);
}


Maybe<bool> JSObject::SetPropertyWithInterceptor(LookupIterator* it,
                                                 ShouldThrow should_throw,
                                                 Handle<Object> value) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->setter()->IsUndefined()) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  v8::Local<v8::Value> result;
  PropertyCallbackArguments args(isolate, interceptor->data(),
                                 *it->GetReceiver(), *holder, should_throw);

  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertySetterCallback setter =
        v8::ToCData<v8::IndexedPropertySetterCallback>(interceptor->setter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-set", *holder, index));
    result = args.Call(setter, index, v8::Utils::ToLocal(value));
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());

    if (name->IsSymbol() && !interceptor->can_intercept_symbols()) {
      return Just(false);
    }

    v8::GenericNamedPropertySetterCallback setter =
        v8::ToCData<v8::GenericNamedPropertySetterCallback>(
            interceptor->setter());
    LOG(it->isolate(),
        ApiNamedPropertyAccess("interceptor-named-set", *holder, *name));
    result =
        args.Call(setter, v8::Utils::ToLocal(name), v8::Utils::ToLocal(value));
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  if (result.IsEmpty()) return Just(false);
#ifdef DEBUG
  Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
  result_internal->VerifyApiCallResultType();
#endif
  return Just(true);
  // TODO(neis): In the future, we may want to actually return the interceptor's
  // result, which then should be a boolean.
}


MaybeHandle<Object> Object::SetProperty(Handle<Object> object,
                                        Handle<Name> name, Handle<Object> value,
                                        LanguageMode language_mode,
                                        StoreFromKeyed store_mode) {
  LookupIterator it(object, name);
  MAYBE_RETURN_NULL(SetProperty(&it, value, language_mode, store_mode));
  return value;
}


Maybe<bool> Object::SetPropertyInternal(LookupIterator* it,
                                        Handle<Object> value,
                                        LanguageMode language_mode,
                                        StoreFromKeyed store_mode,
                                        bool* found) {
  it->UpdateProtector();
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(it->isolate());

  *found = true;

  bool done = false;
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        // Check whether it makes sense to reuse the lookup iterator. Here it
        // might still call into setters up the prototype chain.
        return JSObject::SetPropertyWithFailedAccessCheck(it, value,
                                                          should_throw);

      case LookupIterator::JSPROXY:
        return JSProxy::SetProperty(it->GetHolder<JSProxy>(), it->GetName(),
                                    value, it->GetReceiver(), language_mode);

      case LookupIterator::INTERCEPTOR:
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(it);
          if (!maybe_attributes.IsJust()) return Nothing<bool>();
          done = maybe_attributes.FromJust() != ABSENT;
          if (done && (maybe_attributes.FromJust() & READ_ONLY) != 0) {
            return WriteToReadOnlyProperty(it, value, should_throw);
          }
        }
        break;

      case LookupIterator::ACCESSOR: {
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        Handle<Object> accessors = it->GetAccessors();
        if (accessors->IsAccessorInfo() &&
            !it->HolderIsReceiverOrHiddenPrototype() &&
            AccessorInfo::cast(*accessors)->is_special_data_property()) {
          done = true;
          break;
        }
        return SetPropertyWithAccessor(it, value, should_throw);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TODO(verwaest): We should throw an exception.
        return Just(true);

      case LookupIterator::DATA:
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return SetDataProperty(it, value);
        }
        done = true;
        break;

      case LookupIterator::TRANSITION:
        done = true;
        break;
    }

    if (done) break;
  }

  // If the receiver is the JSGlobalObject, the store was contextual. In case
  // the property did not exist yet on the global object itself, we have to
  // throw a reference error in strict mode.  In sloppy mode, we continue.
  if (it->GetReceiver()->IsJSGlobalObject() && is_strict(language_mode)) {
    it->isolate()->Throw(*it->isolate()->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, it->name()));
    return Nothing<bool>();
  }

  *found = false;
  return Nothing<bool>();
}


Maybe<bool> Object::SetProperty(LookupIterator* it, Handle<Object> value,
                                LanguageMode language_mode,
                                StoreFromKeyed store_mode) {
  bool found = false;
  Maybe<bool> result =
      SetPropertyInternal(it, value, language_mode, store_mode, &found);
  if (found) return result;
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;
  return AddDataProperty(it, value, NONE, should_throw, store_mode);
}


Maybe<bool> Object::SetSuperProperty(LookupIterator* it, Handle<Object> value,
                                     LanguageMode language_mode,
                                     StoreFromKeyed store_mode) {
  Isolate* isolate = it->isolate();

  bool found = false;
  Maybe<bool> result =
      SetPropertyInternal(it, value, language_mode, store_mode, &found);
  if (found) return result;

  // The property either doesn't exist on the holder or exists there as a data
  // property.

  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;

  if (!it->GetReceiver()->IsJSReceiver()) {
    return WriteToReadOnlyProperty(it, value, should_throw);
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  LookupIterator::Configuration c = LookupIterator::OWN;
  LookupIterator own_lookup =
      it->IsElement() ? LookupIterator(isolate, receiver, it->index(), c)
                      : LookupIterator(receiver, it->name(), c);

  for (; own_lookup.IsFound(); own_lookup.Next()) {
    switch (own_lookup.state()) {
      case LookupIterator::ACCESS_CHECK:
        if (!own_lookup.HasAccess()) {
          return JSObject::SetPropertyWithFailedAccessCheck(&own_lookup, value,
                                                            should_throw);
        }
        break;

      case LookupIterator::ACCESSOR:
        if (own_lookup.GetAccessors()->IsAccessorInfo()) {
          if (own_lookup.IsReadOnly()) {
            return WriteToReadOnlyProperty(&own_lookup, value, should_throw);
          }
          return JSObject::SetPropertyWithAccessor(&own_lookup, value,
                                                   should_throw);
        }
      // Fall through.
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return RedefineIncompatibleProperty(isolate, it->GetName(), value,
                                            should_throw);

      case LookupIterator::DATA: {
        if (own_lookup.IsReadOnly()) {
          return WriteToReadOnlyProperty(&own_lookup, value, should_throw);
        }
        return SetDataProperty(&own_lookup, value);
      }

      case LookupIterator::INTERCEPTOR:
      case LookupIterator::JSPROXY: {
        PropertyDescriptor desc;
        Maybe<bool> owned =
            JSReceiver::GetOwnPropertyDescriptor(&own_lookup, &desc);
        MAYBE_RETURN(owned, Nothing<bool>());
        if (!owned.FromJust()) {
          return JSReceiver::CreateDataProperty(&own_lookup, value,
                                                should_throw);
        }
        if (PropertyDescriptor::IsAccessorDescriptor(&desc) ||
            !desc.writable()) {
          return RedefineIncompatibleProperty(isolate, it->GetName(), value,
                                              should_throw);
        }

        PropertyDescriptor value_desc;
        value_desc.set_value(value);
        return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                             &value_desc, should_throw);
      }

      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
    }
  }

  return JSObject::AddDataProperty(&own_lookup, value, NONE, should_throw,
                                   store_mode);
}

MaybeHandle<Object> Object::ReadAbsentProperty(LookupIterator* it) {
  return it->isolate()->factory()->undefined_value();
}

MaybeHandle<Object> Object::ReadAbsentProperty(Isolate* isolate,
                                               Handle<Object> receiver,
                                               Handle<Object> name) {
  return isolate->factory()->undefined_value();
}


Maybe<bool> Object::CannotCreateProperty(Isolate* isolate,
                                         Handle<Object> receiver,
                                         Handle<Object> name,
                                         Handle<Object> value,
                                         ShouldThrow should_throw) {
  RETURN_FAILURE(
      isolate, should_throw,
      NewTypeError(MessageTemplate::kStrictCannotCreateProperty, name,
                   Object::TypeOf(isolate, receiver), receiver));
}


Maybe<bool> Object::WriteToReadOnlyProperty(LookupIterator* it,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  return WriteToReadOnlyProperty(it->isolate(), it->GetReceiver(),
                                 it->GetName(), value, should_throw);
}


Maybe<bool> Object::WriteToReadOnlyProperty(Isolate* isolate,
                                            Handle<Object> receiver,
                                            Handle<Object> name,
                                            Handle<Object> value,
                                            ShouldThrow should_throw) {
  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kStrictReadOnlyProperty, name,
                              Object::TypeOf(isolate, receiver), receiver));
}


Maybe<bool> Object::RedefineIncompatibleProperty(Isolate* isolate,
                                                 Handle<Object> name,
                                                 Handle<Object> value,
                                                 ShouldThrow should_throw) {
  RETURN_FAILURE(isolate, should_throw,
                 NewTypeError(MessageTemplate::kRedefineDisallowed, name));
}


Maybe<bool> Object::SetDataProperty(LookupIterator* it, Handle<Object> value) {
  // Proxies are handled elsewhere. Other non-JSObjects cannot have own
  // properties.
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  // Store on the holder which may be hidden behind the receiver.
  DCHECK(it->HolderIsReceiverOrHiddenPrototype());

  // Old value for the observation change record.
  // Fetch before transforming the object since the encoding may become
  // incompatible with what's cached in |it|.
  bool is_observed = receiver->map()->is_observed() &&
                     (it->IsElement() || !it->name()->IsPrivate());
  MaybeHandle<Object> maybe_old;
  if (is_observed) maybe_old = it->GetDataValue();

  Handle<Object> to_assign = value;
  // Convert the incoming value to a number for storing into typed arrays.
  if (it->IsElement() && receiver->HasFixedTypedArrayElements()) {
    if (!value->IsNumber() && !value->IsUndefined()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          it->isolate(), to_assign, Object::ToNumber(value), Nothing<bool>());
      // We have to recheck the length. However, it can only change if the
      // underlying buffer was neutered, so just check that.
      if (Handle<JSArrayBufferView>::cast(receiver)->WasNeutered()) {
        return Just(true);
        // TODO(neis): According to the spec, this should throw a TypeError.
      }
    }
  }

  // Possibly migrate to the most up-to-date map that will be able to store
  // |value| under it->name().
  it->PrepareForDataProperty(to_assign);

  // Write the property value.
  it->WriteDataValue(to_assign);

  // Send the change record if there are observers.
  if (is_observed && !value->SameValue(*maybe_old.ToHandleChecked())) {
    RETURN_ON_EXCEPTION_VALUE(
        it->isolate(),
        JSObject::EnqueueChangeRecord(receiver, "update", it->GetName(),
                                      maybe_old.ToHandleChecked()),
        Nothing<bool>());
  }

#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    receiver->JSObjectVerify();
  }
#endif
  return Just(true);
}


MUST_USE_RESULT static MaybeHandle<Object> BeginPerformSplice(
    Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = {object};

  return Execution::Call(
      isolate, Handle<JSFunction>(isolate->observers_begin_perform_splice()),
      isolate->factory()->undefined_value(), arraysize(args), args);
}


MUST_USE_RESULT static MaybeHandle<Object> EndPerformSplice(
    Handle<JSArray> object) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> args[] = {object};

  return Execution::Call(
      isolate, Handle<JSFunction>(isolate->observers_end_perform_splice()),
      isolate->factory()->undefined_value(), arraysize(args), args);
}


MUST_USE_RESULT static MaybeHandle<Object> EnqueueSpliceRecord(
    Handle<JSArray> object, uint32_t index, Handle<JSArray> deleted,
    uint32_t add_count) {
  Isolate* isolate = object->GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> index_object = isolate->factory()->NewNumberFromUint(index);
  Handle<Object> add_count_object =
      isolate->factory()->NewNumberFromUint(add_count);

  Handle<Object> args[] = {object, index_object, deleted, add_count_object};

  return Execution::Call(
      isolate, Handle<JSFunction>(isolate->observers_enqueue_splice()),
      isolate->factory()->undefined_value(), arraysize(args), args);
}


Maybe<bool> Object::AddDataProperty(LookupIterator* it, Handle<Object> value,
                                    PropertyAttributes attributes,
                                    ShouldThrow should_throw,
                                    StoreFromKeyed store_mode) {
  if (!it->GetReceiver()->IsJSObject()) {
    if (it->GetReceiver()->IsJSProxy() && it->GetName()->IsPrivate()) {
      RETURN_FAILURE(it->isolate(), should_throw,
                     NewTypeError(MessageTemplate::kProxyPrivate));
    }
    return CannotCreateProperty(it->isolate(), it->GetReceiver(), it->GetName(),
                                value, should_throw);
  }

  DCHECK_NE(LookupIterator::INTEGER_INDEXED_EXOTIC, it->state());

  Handle<JSObject> receiver = it->GetStoreTarget();

  // If the receiver is a JSGlobalProxy, store on the prototype (JSGlobalObject)
  // instead. If the prototype is Null, the proxy is detached.
  if (receiver->IsJSGlobalProxy()) return Just(true);

  Isolate* isolate = it->isolate();

  if (it->ExtendingNonExtensible(receiver)) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kObjectNotExtensible, it->GetName()));
  }

  if (it->IsElement()) {
    if (receiver->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(receiver);
      if (JSArray::WouldChangeReadOnlyLength(array, it->index())) {
        RETURN_FAILURE(array->GetIsolate(), should_throw,
                       NewTypeError(MessageTemplate::kStrictReadOnlyProperty,
                                    isolate->factory()->length_string(),
                                    Object::TypeOf(isolate, array), array));
      }

      if (FLAG_trace_external_array_abuse &&
          array->HasFixedTypedArrayElements()) {
        CheckArrayAbuse(array, "typed elements write", it->index(), true);
      }

      if (FLAG_trace_js_array_abuse && !array->HasFixedTypedArrayElements()) {
        CheckArrayAbuse(array, "elements write", it->index(), false);
      }
    }

    Maybe<bool> result = JSObject::AddDataElement(receiver, it->index(), value,
                                                  attributes, should_throw);
    JSObject::ValidateElements(receiver);
    return result;
  } else {
    // Migrate to the most up-to-date map that will be able to store |value|
    // under it->name() with |attributes|.
    it->PrepareTransitionToDataProperty(receiver, value, attributes,
                                        store_mode);
    DCHECK_EQ(LookupIterator::TRANSITION, it->state());
    it->ApplyTransitionToDataProperty(receiver);

    // TODO(verwaest): Encapsulate dictionary handling better.
    if (receiver->map()->is_dictionary_map()) {
      // TODO(dcarney): just populate TransitionPropertyCell here?
      JSObject::AddSlowProperty(receiver, it->name(), value, attributes);
    } else {
      // Write the property value.
      it->WriteDataValue(value);
    }

    // Send the change record if there are observers.
    if (receiver->map()->is_observed() && !it->name()->IsPrivate()) {
      RETURN_ON_EXCEPTION_VALUE(isolate, JSObject::EnqueueChangeRecord(
                                             receiver, "add", it->name(),
                                             it->factory()->the_hole_value()),
                                Nothing<bool>());
    }
#if VERIFY_HEAP
    if (FLAG_verify_heap) {
      receiver->JSObjectVerify();
    }
#endif
  }

  return Just(true);
}


void Map::EnsureDescriptorSlack(Handle<Map> map, int slack) {
  // Only supports adding slack to owned descriptors.
  DCHECK(map->owns_descriptors());

  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  int old_size = map->NumberOfOwnDescriptors();
  if (slack <= descriptors->NumberOfSlackDescriptors()) return;

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      descriptors, old_size, slack);

  DisallowHeapAllocation no_allocation;
  // The descriptors are still the same, so keep the layout descriptor.
  LayoutDescriptor* layout_descriptor = map->GetLayoutDescriptor();

  if (old_size == 0) {
    map->UpdateDescriptors(*new_descriptors, layout_descriptor);
    return;
  }

  // If the source descriptors had an enum cache we copy it. This ensures
  // that the maps to which we push the new descriptor array back can rely
  // on a cache always being available once it is set. If the map has more
  // enumerated descriptors than available in the original cache, the cache
  // will be lazily replaced by the extended cache when needed.
  if (descriptors->HasEnumCache()) {
    new_descriptors->CopyEnumCacheFrom(*descriptors);
  }

  // Replace descriptors by new_descriptors in all maps that share it.
  map->GetHeap()->incremental_marking()->RecordWrites(*descriptors);

  Map* current = *map;
  while (current->instance_descriptors() == *descriptors) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined()) break;  // Stop overwriting at initial map.
    current->UpdateDescriptors(*new_descriptors, layout_descriptor);
    current = Map::cast(next);
  }
  map->UpdateDescriptors(*new_descriptors, layout_descriptor);
}


template<class T>
static int AppendUniqueCallbacks(NeanderArray* callbacks,
                                 Handle<typename T::Array> array,
                                 int valid_descriptors) {
  int nof_callbacks = callbacks->length();

  Isolate* isolate = array->GetIsolate();
  // Ensure the keys are unique names before writing them into the
  // instance descriptor. Since it may cause a GC, it has to be done before we
  // temporarily put the heap in an invalid state while appending descriptors.
  for (int i = 0; i < nof_callbacks; ++i) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)));
    if (entry->name()->IsUniqueName()) continue;
    Handle<String> key =
        isolate->factory()->InternalizeString(
            Handle<String>(String::cast(entry->name())));
    entry->set_name(*key);
  }

  // Fill in new callback descriptors.  Process the callbacks from
  // back to front so that the last callback with a given name takes
  // precedence over previously added callbacks with that name.
  for (int i = nof_callbacks - 1; i >= 0; i--) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)));
    Handle<Name> key(Name::cast(entry->name()));
    // Check if a descriptor with this name already exists before writing.
    if (!T::Contains(key, entry, valid_descriptors, array)) {
      T::Insert(key, entry, valid_descriptors, array);
      valid_descriptors++;
    }
  }

  return valid_descriptors;
}

struct DescriptorArrayAppender {
  typedef DescriptorArray Array;
  static bool Contains(Handle<Name> key,
                       Handle<AccessorInfo> entry,
                       int valid_descriptors,
                       Handle<DescriptorArray> array) {
    DisallowHeapAllocation no_gc;
    return array->Search(*key, valid_descriptors) != DescriptorArray::kNotFound;
  }
  static void Insert(Handle<Name> key,
                     Handle<AccessorInfo> entry,
                     int valid_descriptors,
                     Handle<DescriptorArray> array) {
    DisallowHeapAllocation no_gc;
    AccessorConstantDescriptor desc(key, entry, entry->property_attributes());
    array->Append(&desc);
  }
};


struct FixedArrayAppender {
  typedef FixedArray Array;
  static bool Contains(Handle<Name> key,
                       Handle<AccessorInfo> entry,
                       int valid_descriptors,
                       Handle<FixedArray> array) {
    for (int i = 0; i < valid_descriptors; i++) {
      if (*key == AccessorInfo::cast(array->get(i))->name()) return true;
    }
    return false;
  }
  static void Insert(Handle<Name> key,
                     Handle<AccessorInfo> entry,
                     int valid_descriptors,
                     Handle<FixedArray> array) {
    DisallowHeapAllocation no_gc;
    array->set(valid_descriptors, *entry);
  }
};


void Map::AppendCallbackDescriptors(Handle<Map> map,
                                    Handle<Object> descriptors) {
  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> array(map->instance_descriptors());
  NeanderArray callbacks(descriptors);
  DCHECK(array->NumberOfSlackDescriptors() >= callbacks.length());
  nof = AppendUniqueCallbacks<DescriptorArrayAppender>(&callbacks, array, nof);
  map->SetNumberOfOwnDescriptors(nof);
}


int AccessorInfo::AppendUnique(Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  NeanderArray callbacks(descriptors);
  DCHECK(array->length() >= callbacks.length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(&callbacks,
                                                   array,
                                                   valid_descriptors);
}


static bool ContainsMap(MapHandleList* maps, Map* map) {
  DCHECK_NOT_NULL(map);
  for (int i = 0; i < maps->length(); ++i) {
    if (!maps->at(i).is_null() && *maps->at(i) == map) return true;
  }
  return false;
}

Map* Map::FindElementsKindTransitionedMap(MapHandleList* candidates) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(GetIsolate());

  ElementsKind kind = elements_kind();
  bool packed = IsFastPackedElementsKind(kind);

  Map* transition = nullptr;
  if (IsTransitionableFastElementsKind(kind)) {
    // Check the state of the root map.
    Map* root_map = FindRootMap();
    if (!EquivalentToForTransition(root_map)) return nullptr;
    root_map = root_map->LookupElementsTransitionMap(kind);
    DCHECK_NOT_NULL(root_map);
    // Starting from the next existing elements kind transition try to
    // replay the property transitions that does not involve instance rewriting
    // (ElementsTransitionAndStoreStub does not support that).
    for (root_map = root_map->ElementsTransitionMap();
         root_map != nullptr && root_map->has_fast_elements();
         root_map = root_map->ElementsTransitionMap()) {
      Map* current = root_map->TryReplayPropertyTransitions(this);
      if (current == nullptr) continue;
      if (InstancesNeedRewriting(current)) continue;

      if (ContainsMap(candidates, current) &&
          (packed || !IsFastPackedElementsKind(current->elements_kind()))) {
        transition = current;
        packed = packed && IsFastPackedElementsKind(current->elements_kind());
      }
    }
  }
  return transition;
}


static Map* FindClosestElementsTransition(Map* map, ElementsKind to_kind) {
  // Ensure we are requested to search elements kind transition "near the root".
  DCHECK_EQ(map->FindRootMap()->NumberOfOwnDescriptors(),
            map->NumberOfOwnDescriptors());
  Map* current_map = map;

  ElementsKind kind = map->elements_kind();
  while (kind != to_kind) {
    Map* next_map = current_map->ElementsTransitionMap();
    if (next_map == nullptr) return current_map;
    kind = next_map->elements_kind();
    current_map = next_map;
  }

  DCHECK_EQ(to_kind, current_map->elements_kind());
  return current_map;
}


Map* Map::LookupElementsTransitionMap(ElementsKind to_kind) {
  Map* to_map = FindClosestElementsTransition(this, to_kind);
  if (to_map->elements_kind() == to_kind) return to_map;
  return nullptr;
}


bool Map::IsMapInArrayPrototypeChain() {
  Isolate* isolate = GetIsolate();
  if (isolate->initial_array_prototype()->map() == this) {
    return true;
  }

  if (isolate->initial_object_prototype()->map() == this) {
    return true;
  }

  return false;
}


Handle<WeakCell> Map::WeakCellForMap(Handle<Map> map) {
  Isolate* isolate = map->GetIsolate();
  if (map->weak_cell_cache()->IsWeakCell()) {
    return Handle<WeakCell>(WeakCell::cast(map->weak_cell_cache()));
  }
  Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(map);
  map->set_weak_cell_cache(*weak_cell);
  return weak_cell;
}


static Handle<Map> AddMissingElementsTransitions(Handle<Map> map,
                                                 ElementsKind to_kind) {
  DCHECK(IsTransitionElementsKind(map->elements_kind()));

  Handle<Map> current_map = map;

  ElementsKind kind = map->elements_kind();
  TransitionFlag flag;
  if (map->is_prototype_map()) {
    flag = OMIT_TRANSITION;
  } else {
    flag = INSERT_TRANSITION;
    if (IsFastElementsKind(kind)) {
      while (kind != to_kind && !IsTerminalElementsKind(kind)) {
        kind = GetNextTransitionElementsKind(kind);
        current_map = Map::CopyAsElementsKind(current_map, kind, flag);
      }
    }
  }

  // In case we are exiting the fast elements kind system, just add the map in
  // the end.
  if (kind != to_kind) {
    current_map = Map::CopyAsElementsKind(current_map, to_kind, flag);
  }

  DCHECK(current_map->elements_kind() == to_kind);
  return current_map;
}


Handle<Map> Map::TransitionElementsTo(Handle<Map> map,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = map->elements_kind();
  if (from_kind == to_kind) return map;

  Isolate* isolate = map->GetIsolate();
  Context* native_context = isolate->context()->native_context();
  if (from_kind == FAST_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context->fast_aliased_arguments_map()) {
      DCHECK_EQ(SLOW_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context->slow_aliased_arguments_map());
    }
  } else if (from_kind == SLOW_SLOPPY_ARGUMENTS_ELEMENTS) {
    if (*map == native_context->slow_aliased_arguments_map()) {
      DCHECK_EQ(FAST_SLOPPY_ARGUMENTS_ELEMENTS, to_kind);
      return handle(native_context->fast_aliased_arguments_map());
    }
  } else if (IsFastElementsKind(from_kind) && IsFastElementsKind(to_kind)) {
    // Reuse map transitions for JSArrays.
    DisallowHeapAllocation no_gc;
    Strength strength = map->is_strong() ? Strength::STRONG : Strength::WEAK;
    if (native_context->get(Context::ArrayMapIndex(from_kind, strength)) ==
        *map) {
      Object* maybe_transitioned_map =
          native_context->get(Context::ArrayMapIndex(to_kind, strength));
      if (maybe_transitioned_map->IsMap()) {
        return handle(Map::cast(maybe_transitioned_map), isolate);
      }
    }
  }

  DCHECK(!map->IsUndefined());
  // Check if we can go back in the elements kind transition chain.
  if (IsHoleyElementsKind(from_kind) &&
      to_kind == GetPackedElementsKind(from_kind) &&
      map->GetBackPointer()->IsMap() &&
      Map::cast(map->GetBackPointer())->elements_kind() == to_kind) {
    return handle(Map::cast(map->GetBackPointer()));
  }

  bool allow_store_transition = IsTransitionElementsKind(from_kind);
  // Only store fast element maps in ascending generality.
  if (IsFastElementsKind(to_kind)) {
    allow_store_transition =
        allow_store_transition && IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind);
  }

  if (!allow_store_transition) {
    return Map::CopyAsElementsKind(map, to_kind, OMIT_TRANSITION);
  }

  return Map::ReconfigureElementsKind(map, to_kind);
}


// static
Handle<Map> Map::AsElementsKind(Handle<Map> map, ElementsKind kind) {
  Handle<Map> closest_map(FindClosestElementsTransition(*map, kind));

  if (closest_map->elements_kind() == kind) {
    return closest_map;
  }

  return AddMissingElementsTransitions(closest_map, kind);
}


Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Handle<Map> map(object->map());
  return Map::TransitionElementsTo(map, to_kind);
}


void JSProxy::Revoke(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  if (!proxy->IsRevoked()) proxy->set_handler(isolate->heap()->null_value());
  DCHECK(proxy->IsRevoked());
}


Maybe<bool> JSProxy::HasProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                 Handle<Name> name) {
  DCHECK(!name->IsPrivate());
  STACK_CHECK(Nothing<bool>());
  // 1. (Assert)
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, isolate->factory()->has_string()));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 6. Let trap be ? GetMethod(handler, "has").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(Handle<JSReceiver>::cast(handler),
                                       isolate->factory()->has_string()),
      Nothing<bool>());
  // 7. If trap is undefined, then
  if (trap->IsUndefined()) {
    // 7a. Return target.[[HasProperty]](P).
    return JSReceiver::HasProperty(target, name);
  }
  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, «target, P»)).
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  bool boolean_trap_result = trap_result_obj->BooleanValue();
  // 9. If booleanTrapResult is false, then:
  if (!boolean_trap_result) {
    // 9a. Let targetDesc be ? target.[[GetOwnProperty]](P).
    PropertyDescriptor target_desc;
    Maybe<bool> target_found = JSReceiver::GetOwnPropertyDescriptor(
        isolate, target, name, &target_desc);
    MAYBE_RETURN(target_found, Nothing<bool>());
    // 9b. If targetDesc is not undefined, then:
    if (target_found.FromJust()) {
      // 9b i. If targetDesc.[[Configurable]] is false, throw a TypeError
      //       exception.
      if (!target_desc.configurable()) {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxyHasNonConfigurable, name));
        return Nothing<bool>();
      }
      // 9b ii. Let extensibleTarget be ? IsExtensible(target).
      Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
      MAYBE_RETURN(extensible_target, Nothing<bool>());
      // 9b iii. If extensibleTarget is false, throw a TypeError exception.
      if (!extensible_target.FromJust()) {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxyHasNonExtensible, name));
        return Nothing<bool>();
      }
    }
  }
  // 10. Return booleanTrapResult.
  return Just(boolean_trap_result);
}


Maybe<bool> JSProxy::SetProperty(Handle<JSProxy> proxy, Handle<Name> name,
                                 Handle<Object> value, Handle<Object> receiver,
                                 LanguageMode language_mode) {
  DCHECK(!name->IsPrivate());
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->set_string();
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(proxy->target(), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined()) {
    LookupIterator it =
        LookupIterator::PropertyOrElement(isolate, receiver, name, target);
    return Object::SetSuperProperty(&it, value, language_mode,
                                    Object::MAY_BE_STORE_FROM_KEYED);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name, value, receiver};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  // Enforce the invariant.
  PropertyDescriptor target_desc;
  Maybe<bool> owned =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(owned, Nothing<bool>());
  if (owned.FromJust()) {
    bool inconsistent = PropertyDescriptor::IsDataDescriptor(&target_desc) &&
                        !target_desc.configurable() &&
                        !target_desc.writable() &&
                        !value->SameValue(*target_desc.value());
    if (inconsistent) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxySetFrozenData, name));
      return Nothing<bool>();
    }
    inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                   !target_desc.configurable() &&
                   target_desc.set()->IsUndefined();
    if (inconsistent) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxySetFrozenAccessor, name));
      return Nothing<bool>();
    }
  }
  return Just(true);
}


Maybe<bool> JSProxy::DeletePropertyOrElement(Handle<JSProxy> proxy,
                                             Handle<Name> name,
                                             LanguageMode language_mode) {
  DCHECK(!name->IsPrivate());
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->deleteProperty_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(proxy->target(), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined()) {
    return JSReceiver::DeletePropertyOrElement(target, name, language_mode);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  // Enforce the invariant.
  PropertyDescriptor target_desc;
  Maybe<bool> owned =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(owned, Nothing<bool>());
  if (owned.FromJust() && !target_desc.configurable()) {
    isolate->Throw(*factory->NewTypeError(
        MessageTemplate::kProxyDeletePropertyNonConfigurable, name));
    return Nothing<bool>();
  }
  return Just(true);
}


// static
MaybeHandle<JSProxy> JSProxy::New(Isolate* isolate, Handle<Object> target,
                                  Handle<Object> handler) {
  if (!target->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  if (target->IsJSProxy() && JSProxy::cast(*target)->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  if (!handler->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  if (handler->IsJSProxy() && JSProxy::cast(*handler)->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  return isolate->factory()->NewJSProxy(Handle<JSReceiver>::cast(target),
                                        Handle<JSReceiver>::cast(handler));
}


// static
MaybeHandle<Context> JSProxy::GetFunctionRealm(Handle<JSProxy> proxy) {
  DCHECK(proxy->map()->is_constructor());
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(proxy->GetIsolate(),
                    NewTypeError(MessageTemplate::kProxyRevoked), Context);
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()));
  return JSReceiver::GetFunctionRealm(target);
}


// static
MaybeHandle<Context> JSBoundFunction::GetFunctionRealm(
    Handle<JSBoundFunction> function) {
  DCHECK(function->map()->is_constructor());
  return JSReceiver::GetFunctionRealm(
      handle(function->bound_target_function()));
}


// static
Handle<Context> JSFunction::GetFunctionRealm(Handle<JSFunction> function) {
  DCHECK(function->map()->is_constructor());
  return handle(function->context()->native_context());
}


// static
MaybeHandle<Context> JSObject::GetFunctionRealm(Handle<JSObject> object) {
  DCHECK(object->map()->is_constructor());
  DCHECK(!object->IsJSFunction());
  return handle(object->GetCreationContext());
}


// static
MaybeHandle<Context> JSReceiver::GetFunctionRealm(Handle<JSReceiver> receiver) {
  if (receiver->IsJSProxy()) {
    return JSProxy::GetFunctionRealm(Handle<JSProxy>::cast(receiver));
  }

  if (receiver->IsJSFunction()) {
    return JSFunction::GetFunctionRealm(Handle<JSFunction>::cast(receiver));
  }

  if (receiver->IsJSBoundFunction()) {
    return JSBoundFunction::GetFunctionRealm(
        Handle<JSBoundFunction>::cast(receiver));
  }

  return JSObject::GetFunctionRealm(Handle<JSObject>::cast(receiver));
}


Maybe<PropertyAttributes> JSProxy::GetPropertyAttributes(LookupIterator* it) {
  Isolate* isolate = it->isolate();
  HandleScope scope(isolate);
  PropertyDescriptor desc;
  Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
      isolate, it->GetHolder<JSProxy>(), it->GetName(), &desc);
  MAYBE_RETURN(found, Nothing<PropertyAttributes>());
  if (!found.FromJust()) return Just(ABSENT);
  return Just(desc.ToAttributes());
}


void JSObject::AllocateStorageForMap(Handle<JSObject> object, Handle<Map> map) {
  DCHECK(object->map()->GetInObjectProperties() ==
         map->GetInObjectProperties());
  ElementsKind obj_kind = object->map()->elements_kind();
  ElementsKind map_kind = map->elements_kind();
  if (map_kind != obj_kind) {
    ElementsKind to_kind = GetMoreGeneralElementsKind(map_kind, obj_kind);
    if (IsDictionaryElementsKind(obj_kind)) {
      to_kind = obj_kind;
    }
    if (IsDictionaryElementsKind(to_kind)) {
      NormalizeElements(object);
    } else {
      TransitionElementsKind(object, to_kind);
    }
    map = Map::ReconfigureElementsKind(map, to_kind);
  }
  JSObject::MigrateToMap(object, map);
}


void JSObject::MigrateInstance(Handle<JSObject> object) {
  Handle<Map> original_map(object->map());
  Handle<Map> map = Map::Update(original_map);
  map->set_migration_target(true);
  MigrateToMap(object, map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, *map);
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify();
  }
#endif
}


// static
bool JSObject::TryMigrateInstance(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  DisallowDeoptimization no_deoptimization(isolate);
  Handle<Map> original_map(object->map(), isolate);
  Handle<Map> new_map;
  if (!Map::TryUpdate(original_map).ToHandle(&new_map)) {
    return false;
  }
  JSObject::MigrateToMap(object, new_map);
  if (FLAG_trace_migration) {
    object->PrintInstanceMigration(stdout, *original_map, object->map());
  }
#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    object->JSObjectVerify();
  }
#endif
  return true;
}


void JSObject::AddProperty(Handle<JSObject> object, Handle<Name> name,
                           Handle<Object> value,
                           PropertyAttributes attributes) {
  LookupIterator it(object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
#ifdef DEBUG
  uint32_t index;
  DCHECK(!object->IsJSProxy());
  DCHECK(!name->AsArrayIndex(&index));
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  DCHECK(!it.IsFound());
  DCHECK(object->map()->is_extensible() || name->IsPrivate());
#endif
  CHECK(AddDataProperty(&it, value, attributes, THROW_ON_ERROR,
                        CERTAINLY_NOT_STORE_FROM_KEYED)
            .IsJust());
}


// Reconfigures a property to a data property with attributes, even if it is not
// reconfigurable.
// Requires a LookupIterator that does not look at the prototype chain beyond
// hidden prototypes.
MaybeHandle<Object> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    AccessorInfoHandling handling) {
  MAYBE_RETURN_NULL(DefineOwnPropertyIgnoreAttributes(
      it, value, attributes, THROW_ON_ERROR, handling));
  return value;
}


Maybe<bool> JSObject::DefineOwnPropertyIgnoreAttributes(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    ShouldThrow should_throw, AccessorInfoHandling handling) {
  it->UpdateProtector();
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  bool is_observed = object->map()->is_observed() &&
                     (it->IsElement() || !it->name()->IsPrivate());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (!it->HasAccess()) {
          it->isolate()->ReportFailedAccessCheck(it->GetHolder<JSObject>());
          RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
          return Just(true);
        }
        break;

      // If there's an interceptor, try to store the property with the
      // interceptor.
      // In case of success, the attributes will have been reset to the default
      // attributes of the interceptor, rather than the incoming attributes.
      //
      // TODO(verwaest): JSProxy afterwards verify the attributes that the
      // JSProxy claims it has, and verifies that they are compatible. If not,
      // they throw. Here we should do the same.
      case LookupIterator::INTERCEPTOR:
        if (handling == DONT_FORCE_FIELD) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        }
        break;

      case LookupIterator::ACCESSOR: {
        Handle<Object> accessors = it->GetAccessors();

        // Special handling for AccessorInfo, which behaves like a data
        // property.
        if (accessors->IsAccessorInfo() && handling == DONT_FORCE_FIELD) {
          PropertyAttributes current_attributes = it->property_attributes();
          // Ensure the context isn't changed after calling into accessors.
          AssertNoContextChange ncc(it->isolate());

          // Update the attributes before calling the setter. The setter may
          // later change the shape of the property.
          if (current_attributes != attributes) {
            it->TransitionToAccessorPair(accessors, attributes);
          }

          Maybe<bool> result =
              JSObject::SetPropertyWithAccessor(it, value, should_throw);

          if (current_attributes == attributes || result.IsNothing()) {
            return result;
          }

        } else {
          it->ReconfigureDataProperty(value, attributes);
        }

        if (is_observed) {
          RETURN_ON_EXCEPTION_VALUE(
              it->isolate(),
              EnqueueChangeRecord(object, "reconfigure", it->GetName(),
                                  it->factory()->the_hole_value()),
              Nothing<bool>());
        }

        return Just(true);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return RedefineIncompatibleProperty(it->isolate(), it->GetName(), value,
                                            should_throw);

      case LookupIterator::DATA: {
        Handle<Object> old_value = it->factory()->the_hole_value();
        // Regular property update if the attributes match.
        if (it->property_attributes() == attributes) {
          return SetDataProperty(it, value);
        }

        // Special case: properties of typed arrays cannot be reconfigured to
        // non-writable nor to non-enumerable.
        if (it->IsElement() && object->HasFixedTypedArrayElements()) {
          return RedefineIncompatibleProperty(it->isolate(), it->GetName(),
                                              value, should_throw);
        }

        // Reconfigure the data property if the attributes mismatch.
        if (is_observed) old_value = it->GetDataValue();

        it->ReconfigureDataProperty(value, attributes);

        if (is_observed) {
          if (old_value->SameValue(*value)) {
            old_value = it->factory()->the_hole_value();
          }
          RETURN_ON_EXCEPTION_VALUE(
              it->isolate(), EnqueueChangeRecord(object, "reconfigure",
                                                 it->GetName(), old_value),
              Nothing<bool>());
        }
        return Just(true);
      }
    }
  }

  return AddDataProperty(it, value, attributes, should_throw,
                         CERTAINLY_NOT_STORE_FROM_KEYED);
}

MaybeHandle<Object> JSObject::SetOwnPropertyIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  DCHECK(!value->IsTheHole());
  LookupIterator it(object, name, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::SetOwnElementIgnoreAttributes(
    Handle<JSObject> object, uint32_t index, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::DefinePropertyOrElementIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it = LookupIterator::PropertyOrElement(isolate, object, name,
                                                        LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);
  HandleScope scope(isolate);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (!it->IsElement() && it->name()->IsSymbol() &&
      !interceptor->can_intercept_symbols()) {
    return Just(ABSENT);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(),
                                 *it->GetReceiver(), *holder,
                                 Object::DONT_THROW);
  if (!interceptor->query()->IsUndefined()) {
    v8::Local<v8::Integer> result;
    if (it->IsElement()) {
      uint32_t index = it->index();
      v8::IndexedPropertyQueryCallback query =
          v8::ToCData<v8::IndexedPropertyQueryCallback>(interceptor->query());
      LOG(isolate,
          ApiIndexedPropertyAccess("interceptor-indexed-has", *holder, index));
      result = args.Call(query, index);
    } else {
      Handle<Name> name = it->name();
      DCHECK(!name->IsPrivate());
      v8::GenericNamedPropertyQueryCallback query =
          v8::ToCData<v8::GenericNamedPropertyQueryCallback>(
              interceptor->query());
      LOG(isolate,
          ApiNamedPropertyAccess("interceptor-named-has", *holder, *name));
      result = args.Call(query, v8::Utils::ToLocal(name));
    }
    if (!result.IsEmpty()) {
      DCHECK(result->IsInt32());
      return Just(static_cast<PropertyAttributes>(
          result->Int32Value(reinterpret_cast<v8::Isolate*>(isolate)
                                 ->GetCurrentContext()).FromJust()));
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    // TODO(verwaest): Use GetPropertyWithInterceptor?
    v8::Local<v8::Value> result;
    if (it->IsElement()) {
      uint32_t index = it->index();
      v8::IndexedPropertyGetterCallback getter =
          v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
      LOG(isolate, ApiIndexedPropertyAccess("interceptor-indexed-get-has",
                                            *holder, index));
      result = args.Call(getter, index);
    } else {
      Handle<Name> name = it->name();
      DCHECK(!name->IsPrivate());
      v8::GenericNamedPropertyGetterCallback getter =
          v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
              interceptor->getter());
      LOG(isolate,
          ApiNamedPropertyAccess("interceptor-named-get-has", *holder, *name));
      result = args.Call(getter, v8::Utils::ToLocal(name));
    }
    if (!result.IsEmpty()) return Just(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}


Maybe<PropertyAttributes> JSReceiver::GetPropertyAttributes(
    LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::GetPropertyAttributes(it);
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (!result.IsJust()) return result;
        if (result.FromJust() != ABSENT) return result;
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        return JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(ABSENT);
      case LookupIterator::ACCESSOR:
      case LookupIterator::DATA:
        return Just(it->property_attributes());
    }
  }
  return Just(ABSENT);
}


Handle<NormalizedMapCache> NormalizedMapCache::New(Isolate* isolate) {
  Handle<FixedArray> array(
      isolate->factory()->NewFixedArray(kEntries, TENURED));
  return Handle<NormalizedMapCache>::cast(array);
}


MaybeHandle<Map> NormalizedMapCache::Get(Handle<Map> fast_map,
                                         PropertyNormalizationMode mode) {
  DisallowHeapAllocation no_gc;
  Object* value = FixedArray::get(GetIndex(fast_map));
  if (!value->IsMap() ||
      !Map::cast(value)->EquivalentToForNormalization(*fast_map, mode)) {
    return MaybeHandle<Map>();
  }
  return handle(Map::cast(value));
}


void NormalizedMapCache::Set(Handle<Map> fast_map,
                             Handle<Map> normalized_map) {
  DisallowHeapAllocation no_gc;
  DCHECK(normalized_map->is_dictionary_map());
  FixedArray::set(GetIndex(fast_map), *normalized_map);
}


void NormalizedMapCache::Clear() {
  int entries = length();
  for (int i = 0; i != entries; i++) {
    set_undefined(i);
  }
}


void HeapObject::UpdateMapCodeCache(Handle<HeapObject> object,
                                    Handle<Name> name,
                                    Handle<Code> code) {
  Handle<Map> map(object->map());
  Map::UpdateCodeCache(map, name, code);
}


void JSObject::NormalizeProperties(Handle<JSObject> object,
                                   PropertyNormalizationMode mode,
                                   int expected_additional_properties,
                                   const char* reason) {
  if (!object->HasFastProperties()) return;

  Handle<Map> map(object->map());
  Handle<Map> new_map = Map::Normalize(map, mode, reason);

  MigrateToMap(object, new_map, expected_additional_properties);
}


void JSObject::MigrateSlowToFast(Handle<JSObject> object,
                                 int unused_property_fields,
                                 const char* reason) {
  if (object->HasFastProperties()) return;
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<NameDictionary> dictionary(object->property_dictionary());

  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  int number_of_elements = dictionary->NumberOfElements();
  if (number_of_elements > kMaxNumberOfDescriptors) return;

  Handle<FixedArray> iteration_order;
  if (number_of_elements != dictionary->NextEnumerationIndex()) {
    iteration_order =
        NameDictionary::DoGenerateNewEnumerationIndices(dictionary);
  } else {
    iteration_order = NameDictionary::BuildIterationIndicesArray(dictionary);
  }

  int instance_descriptor_length = iteration_order->length();
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    DCHECK(dictionary->IsKey(dictionary->KeyAt(index)));

    Object* value = dictionary->ValueAt(index);
    PropertyType type = dictionary->DetailsAt(index).type();
    if (type == DATA && !value->IsJSFunction()) {
      number_of_fields += 1;
    }
  }

  Handle<Map> old_map(object->map(), isolate);

  int inobject_props = old_map->GetInObjectProperties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(old_map);
  new_map->set_dictionary_map(false);

  UpdatePrototypeUserRegistration(old_map, new_map, isolate);

#if TRACE_MAPS
  if (FLAG_trace_maps) {
    PrintF("[TraceMaps: SlowToFast from= %p to= %p reason= %s ]\n",
           reinterpret_cast<void*>(*old_map), reinterpret_cast<void*>(*new_map),
           reason);
  }
#endif

  if (instance_descriptor_length == 0) {
    DisallowHeapAllocation no_gc;
    DCHECK_LE(unused_property_fields, inobject_props);
    // Transform the object.
    new_map->set_unused_property_fields(inobject_props);
    object->synchronized_set_map(*new_map);
    object->set_properties(isolate->heap()->empty_fixed_array());
    // Check that it really works.
    DCHECK(object->HasFastProperties());
    return;
  }

  // Allocate the instance descriptor.
  Handle<DescriptorArray> descriptors = DescriptorArray::Allocate(
      isolate, instance_descriptor_length, 0, TENURED);

  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the fixed array for the fields.
  Handle<FixedArray> fields = factory->NewFixedArray(
      number_of_allocated_fields);

  // Fill in the instance descriptor and the fields.
  int current_offset = 0;
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    Object* k = dictionary->KeyAt(index);
    DCHECK(dictionary->IsKey(k));
    // Dictionary keys are internalized upon insertion.
    // TODO(jkummerow): Turn this into a DCHECK if it's not hit in the wild.
    CHECK(k->IsUniqueName());
    Handle<Name> key(Name::cast(k), isolate);

    Object* value = dictionary->ValueAt(index);

    PropertyDetails details = dictionary->DetailsAt(index);
    int enumeration_index = details.dictionary_index();
    PropertyType type = details.type();

    if (value->IsJSFunction()) {
      DataConstantDescriptor d(key, handle(value, isolate),
                               details.attributes());
      descriptors->Set(enumeration_index - 1, &d);
    } else if (type == DATA) {
      if (current_offset < inobject_props) {
        object->InObjectPropertyAtPut(current_offset, value,
                                      UPDATE_WRITE_BARRIER);
      } else {
        int offset = current_offset - inobject_props;
        fields->set(offset, value);
      }
      DataDescriptor d(key, current_offset, details.attributes(),
                       // TODO(verwaest): value->OptimalRepresentation();
                       Representation::Tagged());
      current_offset += d.GetDetails().field_width_in_words();
      descriptors->Set(enumeration_index - 1, &d);
    } else if (type == ACCESSOR_CONSTANT) {
      AccessorConstantDescriptor d(key, handle(value, isolate),
                                   details.attributes());
      descriptors->Set(enumeration_index - 1, &d);
    } else {
      UNREACHABLE();
    }
  }
  DCHECK(current_offset == number_of_fields);

  descriptors->Sort();

  Handle<LayoutDescriptor> layout_descriptor = LayoutDescriptor::New(
      new_map, descriptors, descriptors->number_of_descriptors());

  DisallowHeapAllocation no_gc;
  new_map->InitializeDescriptors(*descriptors, *layout_descriptor);
  new_map->set_unused_property_fields(unused_property_fields);

  // Transform the object.
  object->synchronized_set_map(*new_map);

  object->set_properties(*fields);
  DCHECK(object->IsJSObject());

  // Check that it really works.
  DCHECK(object->HasFastProperties());
}


void JSObject::ResetElements(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  CHECK(object->map() != isolate->heap()->sloppy_arguments_elements_map());
  if (object->map()->has_dictionary_elements()) {
    Handle<SeededNumberDictionary> new_elements =
        SeededNumberDictionary::New(isolate, 0);
    object->set_elements(*new_elements);
  } else {
    object->set_elements(object->map()->GetInitialElements());
  }
}


static Handle<SeededNumberDictionary> CopyFastElementsToDictionary(
    Handle<FixedArrayBase> array, int length,
    Handle<SeededNumberDictionary> dictionary, bool used_as_prototype) {
  Isolate* isolate = array->GetIsolate();
  Factory* factory = isolate->factory();
  bool has_double_elements = array->IsFixedDoubleArray();
  for (int i = 0; i < length; i++) {
    Handle<Object> value;
    if (has_double_elements) {
      Handle<FixedDoubleArray> double_array =
          Handle<FixedDoubleArray>::cast(array);
      if (double_array->is_the_hole(i)) {
        value = factory->the_hole_value();
      } else {
        value = factory->NewHeapNumber(double_array->get_scalar(i));
      }
    } else {
      value = handle(Handle<FixedArray>::cast(array)->get(i), isolate);
    }
    if (!value->IsTheHole()) {
      PropertyDetails details = PropertyDetails::Empty();
      dictionary = SeededNumberDictionary::AddNumberEntry(
          dictionary, i, value, details, used_as_prototype);
    }
  }
  return dictionary;
}


void JSObject::RequireSlowElements(SeededNumberDictionary* dictionary) {
  if (dictionary->requires_slow_elements()) return;
  dictionary->set_requires_slow_elements();
  // TODO(verwaest): Remove this hack.
  if (map()->is_prototype_map()) {
    TypeFeedbackVector::ClearAllKeyedStoreICs(GetIsolate());
  }
}


Handle<SeededNumberDictionary> JSObject::GetNormalizedElementDictionary(
    Handle<JSObject> object, Handle<FixedArrayBase> elements) {
  DCHECK(!object->HasDictionaryElements());
  DCHECK(!object->HasSlowArgumentsElements());
  Isolate* isolate = object->GetIsolate();
  // Ensure that notifications fire if the array or object prototypes are
  // normalizing.
  isolate->UpdateArrayProtectorOnNormalizeElements(object);
  int length = object->IsJSArray()
                   ? Smi::cast(Handle<JSArray>::cast(object)->length())->value()
                   : elements->length();
  int used = object->GetFastElementsUsage();
  Handle<SeededNumberDictionary> dictionary =
      SeededNumberDictionary::New(isolate, used);
  return CopyFastElementsToDictionary(elements, length, dictionary,
                                      object->map()->is_prototype_map());
}


Handle<SeededNumberDictionary> JSObject::NormalizeElements(
    Handle<JSObject> object) {
  DCHECK(!object->HasFixedTypedArrayElements());
  Isolate* isolate = object->GetIsolate();

  // Find the backing store.
  Handle<FixedArrayBase> elements(object->elements(), isolate);
  bool is_arguments = object->HasSloppyArgumentsElements();
  if (is_arguments) {
    FixedArray* parameter_map = FixedArray::cast(*elements);
    elements = handle(FixedArrayBase::cast(parameter_map->get(1)), isolate);
  }

  if (elements->IsDictionary()) {
    return Handle<SeededNumberDictionary>::cast(elements);
  }

  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastDoubleElements() ||
         object->HasFastArgumentsElements() ||
         object->HasFastStringWrapperElements());

  Handle<SeededNumberDictionary> dictionary =
      GetNormalizedElementDictionary(object, elements);

  // Switch to using the dictionary as the backing storage for elements.
  ElementsKind target_kind = is_arguments
                                 ? SLOW_SLOPPY_ARGUMENTS_ELEMENTS
                                 : object->HasFastStringWrapperElements()
                                       ? SLOW_STRING_WRAPPER_ELEMENTS
                                       : DICTIONARY_ELEMENTS;
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, target_kind);
  // Set the new map first to satify the elements type assert in set_elements().
  JSObject::MigrateToMap(object, new_map);

  if (is_arguments) {
    FixedArray::cast(object->elements())->set(1, *dictionary);
  } else {
    object->set_elements(*dictionary);
  }

  isolate->counters()->elements_to_dictionary()->Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    OFStream os(stdout);
    os << "Object elements have been normalized:\n";
    object->Print(os);
  }
#endif

  DCHECK(object->HasDictionaryElements() ||
         object->HasSlowArgumentsElements() ||
         object->HasSlowStringWrapperElements());
  return dictionary;
}


static Smi* GenerateIdentityHash(Isolate* isolate) {
  int hash_value;
  int attempts = 0;
  do {
    // Generate a random 32-bit hash value but limit range to fit
    // within a smi.
    hash_value = isolate->random_number_generator()->NextInt() & Smi::kMaxValue;
    attempts++;
  } while (hash_value == 0 && attempts < 30);
  hash_value = hash_value != 0 ? hash_value : 1;  // never return 0

  return Smi::FromInt(hash_value);
}


void JSObject::SetIdentityHash(Handle<JSObject> object, Handle<Smi> hash) {
  DCHECK(!object->IsJSGlobalProxy());
  Isolate* isolate = object->GetIsolate();
  Handle<Name> hash_code_symbol(isolate->heap()->hash_code_symbol());
  JSObject::AddProperty(object, hash_code_symbol, hash, NONE);
}


template<typename ProxyType>
static Handle<Smi> GetOrCreateIdentityHashHelper(Handle<ProxyType> proxy) {
  Isolate* isolate = proxy->GetIsolate();

  Handle<Object> maybe_hash(proxy->hash(), isolate);
  if (maybe_hash->IsSmi()) return Handle<Smi>::cast(maybe_hash);

  Handle<Smi> hash(GenerateIdentityHash(isolate), isolate);
  proxy->set_hash(*hash);
  return hash;
}


Object* JSObject::GetIdentityHash() {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = GetIsolate();
  if (IsJSGlobalProxy()) {
    return JSGlobalProxy::cast(this)->hash();
  }
  Handle<Name> hash_code_symbol(isolate->heap()->hash_code_symbol());
  Handle<Object> stored_value =
      Object::GetPropertyOrElement(Handle<Object>(this, isolate),
                                   hash_code_symbol).ToHandleChecked();
  return stored_value->IsSmi() ? *stored_value
                               : isolate->heap()->undefined_value();
}


Handle<Smi> JSObject::GetOrCreateIdentityHash(Handle<JSObject> object) {
  if (object->IsJSGlobalProxy()) {
    return GetOrCreateIdentityHashHelper(Handle<JSGlobalProxy>::cast(object));
  }
  Isolate* isolate = object->GetIsolate();

  Handle<Object> maybe_hash(object->GetIdentityHash(), isolate);
  if (maybe_hash->IsSmi()) return Handle<Smi>::cast(maybe_hash);

  Handle<Smi> hash(GenerateIdentityHash(isolate), isolate);
  Handle<Name> hash_code_symbol(isolate->heap()->hash_code_symbol());
  JSObject::AddProperty(object, hash_code_symbol, hash, NONE);
  return hash;
}


Object* JSProxy::GetIdentityHash() {
  return this->hash();
}


Handle<Smi> JSProxy::GetOrCreateIdentityHash(Handle<JSProxy> proxy) {
  return GetOrCreateIdentityHashHelper(proxy);
}


Object* JSObject::GetHiddenProperty(Handle<Name> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(key->IsUniqueName());
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    PrototypeIterator iter(GetIsolate(), this);
    // If the proxy is detached, return undefined.
    if (iter.IsAtEnd()) return GetHeap()->the_hole_value();
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return iter.GetCurrent<JSObject>()->GetHiddenProperty(key);
  }
  DCHECK(!IsJSGlobalProxy());
  Object* inline_value = GetHiddenPropertiesHashTable();

  if (inline_value->IsUndefined()) return GetHeap()->the_hole_value();

  ObjectHashTable* hashtable = ObjectHashTable::cast(inline_value);
  Object* entry = hashtable->Lookup(key);
  return entry;
}


Handle<Object> JSObject::SetHiddenProperty(Handle<JSObject> object,
                                           Handle<Name> key,
                                           Handle<Object> value) {
  Isolate* isolate = object->GetIsolate();

  DCHECK(key->IsUniqueName());
  if (object->IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    PrototypeIterator iter(isolate, object);
    // If the proxy is detached, return undefined.
    if (iter.IsAtEnd()) return isolate->factory()->undefined_value();
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return SetHiddenProperty(PrototypeIterator::GetCurrent<JSObject>(iter), key,
                             value);
  }
  DCHECK(!object->IsJSGlobalProxy());

  Handle<Object> inline_value(object->GetHiddenPropertiesHashTable(), isolate);

  Handle<ObjectHashTable> hashtable =
      GetOrCreateHiddenPropertiesHashtable(object);

  // If it was found, check if the key is already in the dictionary.
  Handle<ObjectHashTable> new_table = ObjectHashTable::Put(hashtable, key,
                                                           value);
  if (*new_table != *hashtable) {
    // If adding the key expanded the dictionary (i.e., Add returned a new
    // dictionary), store it back to the object.
    SetHiddenPropertiesHashTable(object, new_table);
  }

  // Return this to mark success.
  return object;
}


void JSObject::DeleteHiddenProperty(Handle<JSObject> object, Handle<Name> key) {
  Isolate* isolate = object->GetIsolate();
  DCHECK(key->IsUniqueName());

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return;
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return DeleteHiddenProperty(PrototypeIterator::GetCurrent<JSObject>(iter),
                                key);
  }

  Object* inline_value = object->GetHiddenPropertiesHashTable();

  if (inline_value->IsUndefined()) return;

  Handle<ObjectHashTable> hashtable(ObjectHashTable::cast(inline_value));
  bool was_present = false;
  ObjectHashTable::Remove(hashtable, key, &was_present);
}


bool JSObject::HasHiddenProperties(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  Handle<Symbol> hidden = isolate->factory()->hidden_properties_symbol();
  LookupIterator it(object, hidden);
  Maybe<PropertyAttributes> maybe = GetPropertyAttributes(&it);
  // Cannot get an exception since the hidden_properties_symbol isn't exposed to
  // JS.
  DCHECK(maybe.IsJust());
  return maybe.FromJust() != ABSENT;
}


Object* JSObject::GetHiddenPropertiesHashTable() {
  DCHECK(!IsJSGlobalProxy());
  if (HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden string. Since the
    // hidden strings hash code is zero (and no other name has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = this->map()->instance_descriptors();
    if (descriptors->number_of_descriptors() > 0) {
      int sorted_index = descriptors->GetSortedKeyIndex(0);
      if (descriptors->GetKey(sorted_index) ==
              GetHeap()->hidden_properties_symbol() &&
          sorted_index < map()->NumberOfOwnDescriptors()) {
        DCHECK(descriptors->GetType(sorted_index) == DATA);
        DCHECK(descriptors->GetDetails(sorted_index).representation().
               IsCompatibleForLoad(Representation::Tagged()));
        FieldIndex index = FieldIndex::ForDescriptor(this->map(),
                                                     sorted_index);
        return this->RawFastPropertyAt(index);
      } else {
        return GetHeap()->undefined_value();
      }
    } else {
      return GetHeap()->undefined_value();
    }
  } else {
    Handle<Symbol> hidden = GetIsolate()->factory()->hidden_properties_symbol();
    LookupIterator it(handle(this), hidden);
    // Access check is always skipped for the hidden string anyways.
    return *GetDataProperty(&it);
  }
}

Handle<ObjectHashTable> JSObject::GetOrCreateHiddenPropertiesHashtable(
    Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();

  static const int kInitialCapacity = 4;
  Handle<Object> inline_value(object->GetHiddenPropertiesHashTable(), isolate);
  if (inline_value->IsHashTable()) {
    return Handle<ObjectHashTable>::cast(inline_value);
  }

  Handle<ObjectHashTable> hashtable = ObjectHashTable::New(
      isolate, kInitialCapacity, USE_CUSTOM_MINIMUM_CAPACITY);

  DCHECK(inline_value->IsUndefined());
  SetHiddenPropertiesHashTable(object, hashtable);
  return hashtable;
}


Handle<Object> JSObject::SetHiddenPropertiesHashTable(Handle<JSObject> object,
                                                      Handle<Object> value) {
  DCHECK(!object->IsJSGlobalProxy());
  Isolate* isolate = object->GetIsolate();
  Handle<Symbol> name = isolate->factory()->hidden_properties_symbol();
  SetOwnPropertyIgnoreAttributes(object, name, value, DONT_ENUM).Assert();
  return object;
}


Maybe<bool> JSObject::DeletePropertyWithInterceptor(LookupIterator* it,
                                                    ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->deleter()->IsUndefined()) return Nothing<bool>();

  Handle<JSObject> holder = it->GetHolder<JSObject>();

  PropertyCallbackArguments args(isolate, interceptor->data(),
                                 *it->GetReceiver(), *holder, should_throw);
  v8::Local<v8::Boolean> result;
  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertyDeleterCallback deleter =
        v8::ToCData<v8::IndexedPropertyDeleterCallback>(interceptor->deleter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-delete", *holder, index));
    result = args.Call(deleter, index);
  } else if (it->name()->IsSymbol() && !interceptor->can_intercept_symbols()) {
    return Nothing<bool>();
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());
    v8::GenericNamedPropertyDeleterCallback deleter =
        v8::ToCData<v8::GenericNamedPropertyDeleterCallback>(
            interceptor->deleter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-delete", *holder, *name));
    result = args.Call(deleter, v8::Utils::ToLocal(name));
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.IsEmpty()) return Nothing<bool>();

  DCHECK(result->IsBoolean());
  Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
  result_internal->VerifyApiCallResultType();
  // Rebox CustomArguments::kReturnValueOffset before returning.
  return Just(result_internal->BooleanValue());
}


void JSReceiver::DeleteNormalizedProperty(Handle<JSReceiver> object,
                                          Handle<Name> name, int entry) {
  DCHECK(!object->HasFastProperties());
  Isolate* isolate = object->GetIsolate();

  if (object->IsJSGlobalObject()) {
    // If we have a global object, invalidate the cell and swap in a new one.
    Handle<GlobalDictionary> dictionary(
        JSObject::cast(*object)->global_dictionary());
    DCHECK_NE(GlobalDictionary::kNotFound, entry);

    auto cell = PropertyCell::InvalidateEntry(dictionary, entry);
    cell->set_value(isolate->heap()->the_hole_value());
    // TODO(ishell): InvalidateForDelete
    cell->set_property_details(
        cell->property_details().set_cell_type(PropertyCellType::kInvalidated));
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary());
    DCHECK_NE(NameDictionary::kNotFound, entry);

    NameDictionary::DeleteProperty(dictionary, entry);
    Handle<NameDictionary> new_properties =
        NameDictionary::Shrink(dictionary, name);
    object->set_properties(*new_properties);
  }
}


Maybe<bool> JSReceiver::DeleteProperty(LookupIterator* it,
                                       LanguageMode language_mode) {
  it->UpdateProtector();

  Isolate* isolate = it->isolate();

  if (it->state() == LookupIterator::JSPROXY) {
    return JSProxy::DeletePropertyOrElement(it->GetHolder<JSProxy>(),
                                            it->GetName(), language_mode);
  }

  if (it->GetReceiver()->IsJSProxy()) {
    if (it->state() != LookupIterator::NOT_FOUND) {
      DCHECK_EQ(LookupIterator::DATA, it->state());
      DCHECK(it->name()->IsPrivate());
      it->Delete();
    }
    return Just(true);
  }
  Handle<JSObject> receiver = Handle<JSObject>::cast(it->GetReceiver());

  bool is_observed = receiver->map()->is_observed() &&
                     (it->IsElement() || !it->name()->IsPrivate());

  Handle<Object> old_value = it->factory()->the_hole_value();

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::JSPROXY:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        if (it->HasAccess()) break;
        isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
        RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
        return Just(false);
      case LookupIterator::INTERCEPTOR: {
        ShouldThrow should_throw =
            is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;
        Maybe<bool> result =
            JSObject::DeletePropertyWithInterceptor(it, should_throw);
        // An exception was thrown in the interceptor. Propagate.
        if (isolate->has_pending_exception()) return Nothing<bool>();
        // Delete with interceptor succeeded. Return result.
        // TODO(neis): In strict mode, we should probably throw if the
        // interceptor returns false.
        if (result.IsJust()) return result;
        break;
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return Just(true);
      case LookupIterator::DATA:
        if (is_observed) {
          old_value = it->GetDataValue();
        }
      // Fall through.
      case LookupIterator::ACCESSOR: {
        if (!it->IsConfigurable() || receiver->map()->is_strong()) {
          // Fail if the property is not configurable, or on a strong object.
          if (is_strict(language_mode)) {
            MessageTemplate::Template templ =
                receiver->map()->is_strong()
                    ? MessageTemplate::kStrongDeleteProperty
                    : MessageTemplate::kStrictDeleteProperty;
            isolate->Throw(*isolate->factory()->NewTypeError(
                templ, it->GetName(), receiver));
            return Nothing<bool>();
          }
          return Just(false);
        }

        it->Delete();

        if (is_observed) {
          RETURN_ON_EXCEPTION_VALUE(
              isolate, JSObject::EnqueueChangeRecord(receiver, "delete",
                                                     it->GetName(), old_value),
              Nothing<bool>());
        }

        return Just(true);
      }
    }
  }

  return Just(true);
}


Maybe<bool> JSReceiver::DeleteElement(Handle<JSReceiver> object, uint32_t index,
                                      LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, index,
                    LookupIterator::HIDDEN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       LanguageMode language_mode) {
  LookupIterator it(object, name, LookupIterator::HIDDEN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeletePropertyOrElement(Handle<JSReceiver> object,
                                                Handle<Name> name,
                                                LanguageMode language_mode) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, LookupIterator::HIDDEN);
  return DeleteProperty(&it, language_mode);
}


// ES6 7.1.14
MaybeHandle<Object> ToPropertyKey(Isolate* isolate, Handle<Object> value) {
  // 1. Let key be ToPrimitive(argument, hint String).
  MaybeHandle<Object> maybe_key =
      Object::ToPrimitive(value, ToPrimitiveHint::kString);
  // 2. ReturnIfAbrupt(key).
  Handle<Object> key;
  if (!maybe_key.ToHandle(&key)) return key;
  // 3. If Type(key) is Symbol, then return key.
  if (key->IsSymbol()) return key;
  // 4. Return ToString(key).
  // Extending spec'ed behavior, we'd be happy to return an element index.
  if (key->IsSmi()) return key;
  if (key->IsHeapNumber()) {
    uint32_t uint_value;
    if (value->ToArrayLength(&uint_value) &&
        uint_value <= static_cast<uint32_t>(Smi::kMaxValue)) {
      return handle(Smi::FromInt(static_cast<int>(uint_value)), isolate);
    }
  }
  return Object::ToString(isolate, key);
}


// ES6 19.1.2.4
// static
Object* JSReceiver::DefineProperty(Isolate* isolate, Handle<Object> object,
                                   Handle<Object> key,
                                   Handle<Object> attributes) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperty");
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name));
  }
  // 2. Let key be ToPropertyKey(P).
  // 3. ReturnIfAbrupt(key).
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, key, ToPropertyKey(isolate, key));
  // 4. Let desc be ToPropertyDescriptor(Attributes).
  // 5. ReturnIfAbrupt(desc).
  PropertyDescriptor desc;
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, attributes, &desc)) {
    return isolate->heap()->exception();
  }
  // 6. Let success be DefinePropertyOrThrow(O,key, desc).
  Maybe<bool> success = DefineOwnProperty(
      isolate, Handle<JSReceiver>::cast(object), key, &desc, THROW_ON_ERROR);
  // 7. ReturnIfAbrupt(success).
  MAYBE_RETURN(success, isolate->heap()->exception());
  CHECK(success.FromJust());
  // 8. Return O.
  return *object;
}


// ES6 19.1.2.3.1
// static
MaybeHandle<Object> JSReceiver::DefineProperties(Isolate* isolate,
                                                 Handle<Object> object,
                                                 Handle<Object> properties) {
  // 1. If Type(O) is not Object, throw a TypeError exception.
  if (!object->IsJSReceiver()) {
    Handle<String> fun_name =
        isolate->factory()->InternalizeUtf8String("Object.defineProperties");
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCalledOnNonObject, fun_name),
                    Object);
  }
  // 2. Let props be ToObject(Properties).
  // 3. ReturnIfAbrupt(props).
  Handle<JSReceiver> props;
  if (!Object::ToObject(isolate, properties).ToHandle(&props)) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kUndefinedOrNullToObject),
                    Object);
  }
  // 4. Let keys be props.[[OwnPropertyKeys]]().
  // 5. ReturnIfAbrupt(keys).
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, keys, JSReceiver::GetKeys(props, OWN_ONLY, ALL_PROPERTIES),
      Object);
  // 6. Let descriptors be an empty List.
  int capacity = keys->length();
  std::vector<PropertyDescriptor> descriptors(capacity);
  size_t descriptors_index = 0;
  // 7. Repeat for each element nextKey of keys in List order,
  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> next_key(keys->get(i), isolate);
    // 7a. Let propDesc be props.[[GetOwnProperty]](nextKey).
    // 7b. ReturnIfAbrupt(propDesc).
    bool success = false;
    LookupIterator it = LookupIterator::PropertyOrElement(
        isolate, props, next_key, &success, LookupIterator::HIDDEN);
    DCHECK(success);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (!maybe.IsJust()) return MaybeHandle<Object>();
    PropertyAttributes attrs = maybe.FromJust();
    // 7c. If propDesc is not undefined and propDesc.[[Enumerable]] is true:
    if (attrs == ABSENT) continue;
    if (attrs & DONT_ENUM) continue;
    // 7c i. Let descObj be Get(props, nextKey).
    // 7c ii. ReturnIfAbrupt(descObj).
    Handle<Object> desc_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, desc_obj, Object::GetProperty(&it),
                               Object);
    // 7c iii. Let desc be ToPropertyDescriptor(descObj).
    success = PropertyDescriptor::ToPropertyDescriptor(
        isolate, desc_obj, &descriptors[descriptors_index]);
    // 7c iv. ReturnIfAbrupt(desc).
    if (!success) return MaybeHandle<Object>();
    // 7c v. Append the pair (a two element List) consisting of nextKey and
    //       desc to the end of descriptors.
    descriptors[descriptors_index].set_name(next_key);
    descriptors_index++;
  }
  // 8. For each pair from descriptors in list order,
  for (size_t i = 0; i < descriptors_index; ++i) {
    PropertyDescriptor* desc = &descriptors[i];
    // 8a. Let P be the first element of pair.
    // 8b. Let desc be the second element of pair.
    // 8c. Let status be DefinePropertyOrThrow(O, P, desc).
    Maybe<bool> status =
        DefineOwnProperty(isolate, Handle<JSReceiver>::cast(object),
                          desc->name(), desc, THROW_ON_ERROR);
    // 8d. ReturnIfAbrupt(status).
    if (!status.IsJust()) return MaybeHandle<Object>();
    CHECK(status.FromJust());
  }
  // 9. Return o.
  return object;
}


// static
Maybe<bool> JSReceiver::DefineOwnProperty(Isolate* isolate,
                                          Handle<JSReceiver> object,
                                          Handle<Object> key,
                                          PropertyDescriptor* desc,
                                          ShouldThrow should_throw) {
  if (object->IsJSArray()) {
    return JSArray::DefineOwnProperty(isolate, Handle<JSArray>::cast(object),
                                      key, desc, should_throw);
  }
  if (object->IsJSProxy()) {
    return JSProxy::DefineOwnProperty(isolate, Handle<JSProxy>::cast(object),
                                      key, desc, should_throw);
  }
  // TODO(jkummerow): Support Modules (ES6 9.4.6.6)

  // OrdinaryDefineOwnProperty, by virtue of calling
  // DefineOwnPropertyIgnoreAttributes, can handle arguments (ES6 9.4.4.2)
  // and IntegerIndexedExotics (ES6 9.4.5.3), with one exception:
  // TODO(jkummerow): Setting an indexed accessor on a typed array should throw.
  return OrdinaryDefineOwnProperty(isolate, Handle<JSObject>::cast(object), key,
                                   desc, should_throw);
}


// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(Isolate* isolate,
                                                  Handle<JSObject> object,
                                                  Handle<Object> key,
                                                  PropertyDescriptor* desc,
                                                  ShouldThrow should_throw) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::HIDDEN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.

  // Deal with access checks first.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      return Just(true);
    }
    it.Next();
  }

  return OrdinaryDefineOwnProperty(&it, desc, should_throw);
}


// ES6 9.1.6.1
// static
Maybe<bool> JSReceiver::OrdinaryDefineOwnProperty(LookupIterator* it,
                                                  PropertyDescriptor* desc,
                                                  ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // 1. Let current be O.[[GetOwnProperty]](P).
  // 2. ReturnIfAbrupt(current).
  PropertyDescriptor current;
  MAYBE_RETURN(GetOwnPropertyDescriptor(it, &current), Nothing<bool>());

  // TODO(jkummerow/verwaest): It would be nice if we didn't have to reset
  // the iterator every time. Currently, the reasons why we need it are:
  // - handle interceptors correctly
  // - handle accessors correctly (which might change the holder's map)
  it->Restart();
  // 3. Let extensible be the value of the [[Extensible]] internal slot of O.
  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  bool extensible = JSObject::IsExtensible(object);

  return ValidateAndApplyPropertyDescriptor(isolate, it, extensible, desc,
                                            &current, should_throw);
}


// ES6 9.1.6.2
// static
Maybe<bool> JSReceiver::IsCompatiblePropertyDescriptor(
    Isolate* isolate, bool extensible, PropertyDescriptor* desc,
    PropertyDescriptor* current, Handle<Name> property_name,
    ShouldThrow should_throw) {
  // 1. Return ValidateAndApplyPropertyDescriptor(undefined, undefined,
  //    Extensible, Desc, Current).
  return ValidateAndApplyPropertyDescriptor(
      isolate, NULL, extensible, desc, current, should_throw, property_name);
}


// ES6 9.1.6.3
// static
Maybe<bool> JSReceiver::ValidateAndApplyPropertyDescriptor(
    Isolate* isolate, LookupIterator* it, bool extensible,
    PropertyDescriptor* desc, PropertyDescriptor* current,
    ShouldThrow should_throw, Handle<Name> property_name) {
  // We either need a LookupIterator, or a property name.
  DCHECK((it == NULL) != property_name.is_null());
  Handle<JSObject> object;
  if (it != NULL) object = Handle<JSObject>::cast(it->GetReceiver());
  bool desc_is_data_descriptor = PropertyDescriptor::IsDataDescriptor(desc);
  bool desc_is_accessor_descriptor =
      PropertyDescriptor::IsAccessorDescriptor(desc);
  bool desc_is_generic_descriptor =
      PropertyDescriptor::IsGenericDescriptor(desc);
  // 1. (Assert)
  // 2. If current is undefined, then
  if (current->is_empty()) {
    // 2a. If extensible is false, return false.
    if (!extensible) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kDefineDisallowed,
                                  it != NULL ? it->GetName() : property_name));
    }
    // 2c. If IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then:
    // (This is equivalent to !IsAccessorDescriptor(desc).)
    DCHECK((desc_is_generic_descriptor || desc_is_data_descriptor) ==
           !desc_is_accessor_descriptor);
    if (!desc_is_accessor_descriptor) {
      // 2c i. If O is not undefined, create an own data property named P of
      // object O whose [[Value]], [[Writable]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != NULL) {
        if (!desc->has_writable()) desc->set_writable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> value(
            desc->has_value()
                ? desc->value()
                : Handle<Object>::cast(isolate->factory()->undefined_value()));
        MaybeHandle<Object> result =
            JSObject::DefineOwnPropertyIgnoreAttributes(it, value,
                                                        desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    } else {
      // 2d. Else Desc must be an accessor Property Descriptor,
      DCHECK(desc_is_accessor_descriptor);
      // 2d i. If O is not undefined, create an own accessor property named P
      // of object O whose [[Get]], [[Set]], [[Enumerable]] and
      // [[Configurable]] attribute values are described by Desc. If the value
      // of an attribute field of Desc is absent, the attribute of the newly
      // created property is set to its default value.
      if (it != NULL) {
        if (!desc->has_enumerable()) desc->set_enumerable(false);
        if (!desc->has_configurable()) desc->set_configurable(false);
        Handle<Object> getter(
            desc->has_get()
                ? desc->get()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        Handle<Object> setter(
            desc->has_set()
                ? desc->set()
                : Handle<Object>::cast(isolate->factory()->null_value()));
        MaybeHandle<Object> result =
            JSObject::DefineAccessor(it, getter, setter, desc->ToAttributes());
        if (result.is_null()) return Nothing<bool>();
      }
    }
    // 2e. Return true.
    return Just(true);
  }
  // 3. Return true, if every field in Desc is absent.
  // 4. Return true, if every field in Desc also occurs in current and the
  // value of every field in Desc is the same value as the corresponding field
  // in current when compared using the SameValue algorithm.
  if ((!desc->has_enumerable() ||
       desc->enumerable() == current->enumerable()) &&
      (!desc->has_configurable() ||
       desc->configurable() == current->configurable()) &&
      (!desc->has_value() ||
       (current->has_value() && current->value()->SameValue(*desc->value()))) &&
      (!desc->has_writable() ||
       (current->has_writable() && current->writable() == desc->writable())) &&
      (!desc->has_get() ||
       (current->has_get() && current->get()->SameValue(*desc->get()))) &&
      (!desc->has_set() ||
       (current->has_set() && current->set()->SameValue(*desc->set())))) {
    return Just(true);
  }
  // 5. If the [[Configurable]] field of current is false, then
  if (!current->configurable()) {
    // 5a. Return false, if the [[Configurable]] field of Desc is true.
    if (desc->has_configurable() && desc->configurable()) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kRedefineDisallowed,
                                  it != NULL ? it->GetName() : property_name));
    }
    // 5b. Return false, if the [[Enumerable]] field of Desc is present and the
    // [[Enumerable]] fields of current and Desc are the Boolean negation of
    // each other.
    if (desc->has_enumerable() && desc->enumerable() != current->enumerable()) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kRedefineDisallowed,
                                  it != NULL ? it->GetName() : property_name));
    }
  }

  bool current_is_data_descriptor =
      PropertyDescriptor::IsDataDescriptor(current);
  // 6. If IsGenericDescriptor(Desc) is true, no further validation is required.
  if (desc_is_generic_descriptor) {
    // Nothing to see here.

    // 7. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) have
    // different results, then:
  } else if (current_is_data_descriptor != desc_is_data_descriptor) {
    // 7a. Return false, if the [[Configurable]] field of current is false.
    if (!current->configurable()) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kRedefineDisallowed,
                                  it != NULL ? it->GetName() : property_name));
    }
    // 7b. If IsDataDescriptor(current) is true, then:
    if (current_is_data_descriptor) {
      // 7b i. If O is not undefined, convert the property named P of object O
      // from a data property to an accessor property. Preserve the existing
      // values of the converted property's [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property's attributes to their
      // default values.
      // --> Folded into step 10.
    } else {
      // 7c i. If O is not undefined, convert the property named P of object O
      // from an accessor property to a data property. Preserve the existing
      // values of the converted property’s [[Configurable]] and [[Enumerable]]
      // attributes and set the rest of the property’s attributes to their
      // default values.
      // --> Folded into step 10.
    }

    // 8. Else if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both
    // true, then:
  } else if (current_is_data_descriptor && desc_is_data_descriptor) {
    // 8a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // [Strong mode] Disallow changing writable -> readonly for
      // non-configurable properties.
      if (it != NULL && current->writable() && desc->has_writable() &&
          !desc->writable() && object->map()->is_strong()) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kStrongRedefineDisallowed,
                                    object, it->GetName()));
      }
      // 8a i. Return false, if the [[Writable]] field of current is false and
      // the [[Writable]] field of Desc is true.
      if (!current->writable() && desc->has_writable() && desc->writable()) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != NULL ? it->GetName() : property_name));
      }
      // 8a ii. If the [[Writable]] field of current is false, then:
      if (!current->writable()) {
        // 8a ii 1. Return false, if the [[Value]] field of Desc is present and
        // SameValue(Desc.[[Value]], current.[[Value]]) is false.
        if (desc->has_value() && !desc->value()->SameValue(*current->value())) {
          RETURN_FAILURE(
              isolate, should_throw,
              NewTypeError(MessageTemplate::kRedefineDisallowed,
                           it != NULL ? it->GetName() : property_name));
        }
      }
    }
  } else {
    // 9. Else IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc)
    // are both true,
    DCHECK(PropertyDescriptor::IsAccessorDescriptor(current) &&
           desc_is_accessor_descriptor);
    // 9a. If the [[Configurable]] field of current is false, then:
    if (!current->configurable()) {
      // 9a i. Return false, if the [[Set]] field of Desc is present and
      // SameValue(Desc.[[Set]], current.[[Set]]) is false.
      if (desc->has_set() && !desc->set()->SameValue(*current->set())) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != NULL ? it->GetName() : property_name));
      }
      // 9a ii. Return false, if the [[Get]] field of Desc is present and
      // SameValue(Desc.[[Get]], current.[[Get]]) is false.
      if (desc->has_get() && !desc->get()->SameValue(*current->get())) {
        RETURN_FAILURE(
            isolate, should_throw,
            NewTypeError(MessageTemplate::kRedefineDisallowed,
                         it != NULL ? it->GetName() : property_name));
      }
    }
  }

  // 10. If O is not undefined, then:
  if (it != NULL) {
    // 10a. For each field of Desc that is present, set the corresponding
    // attribute of the property named P of object O to the value of the field.
    PropertyAttributes attrs = NONE;

    if (desc->has_enumerable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->enumerable() ? NONE : DONT_ENUM));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->enumerable() ? NONE : DONT_ENUM));
    }
    if (desc->has_configurable()) {
      attrs = static_cast<PropertyAttributes>(
          attrs | (desc->configurable() ? NONE : DONT_DELETE));
    } else {
      attrs = static_cast<PropertyAttributes>(
          attrs | (current->configurable() ? NONE : DONT_DELETE));
    }
    if (desc_is_data_descriptor ||
        (desc_is_generic_descriptor && current_is_data_descriptor)) {
      if (desc->has_writable()) {
        attrs = static_cast<PropertyAttributes>(
            attrs | (desc->writable() ? NONE : READ_ONLY));
      } else {
        attrs = static_cast<PropertyAttributes>(
            attrs | (current->writable() ? NONE : READ_ONLY));
      }
      Handle<Object> value(
          desc->has_value() ? desc->value()
                            : current->has_value()
                                  ? current->value()
                                  : Handle<Object>::cast(
                                        isolate->factory()->undefined_value()));
      MaybeHandle<Object> result =
          JSObject::DefineOwnPropertyIgnoreAttributes(it, value, attrs);
      if (result.is_null()) return Nothing<bool>();
    } else {
      DCHECK(desc_is_accessor_descriptor ||
             (desc_is_generic_descriptor &&
              PropertyDescriptor::IsAccessorDescriptor(current)));
      Handle<Object> getter(
          desc->has_get()
              ? desc->get()
              : current->has_get()
                    ? current->get()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      Handle<Object> setter(
          desc->has_set()
              ? desc->set()
              : current->has_set()
                    ? current->set()
                    : Handle<Object>::cast(isolate->factory()->null_value()));
      MaybeHandle<Object> result =
          JSObject::DefineAccessor(it, getter, setter, attrs);
      if (result.is_null()) return Nothing<bool>();
    }
  }

  // 11. Return true.
  return Just(true);
}


// static
Maybe<bool> JSReceiver::CreateDataProperty(LookupIterator* it,
                                           Handle<Object> value,
                                           ShouldThrow should_throw) {
  DCHECK(!it->check_prototype_chain());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (receiver->IsJSObject()) {
    return JSObject::CreateDataProperty(it, value);  // Shortcut.
  }

  PropertyDescriptor new_desc;
  new_desc.set_value(value);
  new_desc.set_writable(true);
  new_desc.set_enumerable(true);
  new_desc.set_configurable(true);

  return JSReceiver::DefineOwnProperty(isolate, receiver, it->GetName(),
                                       &new_desc, should_throw);
}


Maybe<bool> JSObject::CreateDataProperty(LookupIterator* it,
                                         Handle<Object> value) {
  DCHECK(it->GetReceiver()->IsJSObject());
  MAYBE_RETURN(JSReceiver::GetPropertyAttributes(it), Nothing<bool>());

  if (it->IsFound()) {
    if (!it->IsConfigurable()) return Just(false);
  } else {
    if (!JSObject::IsExtensible(Handle<JSObject>::cast(it->GetReceiver())))
      return Just(false);
  }

  RETURN_ON_EXCEPTION_VALUE(it->isolate(),
                            DefineOwnPropertyIgnoreAttributes(it, value, NONE),
                            Nothing<bool>());

  return Just(true);
}


// TODO(jkummerow): Consider unification with FastAsArrayLength() in
// accessors.cc.
bool PropertyKeyToArrayLength(Handle<Object> value, uint32_t* length) {
  DCHECK(value->IsNumber() || value->IsName());
  if (value->ToArrayLength(length)) return true;
  if (value->IsString()) return String::cast(*value)->AsArrayIndex(length);
  return false;
}


bool PropertyKeyToArrayIndex(Handle<Object> index_obj, uint32_t* output) {
  return PropertyKeyToArrayLength(index_obj, output) && *output != kMaxUInt32;
}


// ES6 9.4.2.1
// static
Maybe<bool> JSArray::DefineOwnProperty(Isolate* isolate, Handle<JSArray> o,
                                       Handle<Object> name,
                                       PropertyDescriptor* desc,
                                       ShouldThrow should_throw) {
  // 1. Assert: IsPropertyKey(P) is true. ("P" is |name|.)
  // 2. If P is "length", then:
  // TODO(jkummerow): Check if we need slow string comparison.
  if (*name == isolate->heap()->length_string()) {
    // 2a. Return ArraySetLength(A, Desc).
    return ArraySetLength(isolate, o, desc, should_throw);
  }
  // 3. Else if P is an array index, then:
  uint32_t index = 0;
  if (PropertyKeyToArrayIndex(name, &index)) {
    // 3a. Let oldLenDesc be OrdinaryGetOwnProperty(A, "length").
    PropertyDescriptor old_len_desc;
    Maybe<bool> success = GetOwnPropertyDescriptor(
        isolate, o, isolate->factory()->length_string(), &old_len_desc);
    // 3b. (Assert)
    DCHECK(success.FromJust());
    USE(success);
    // 3c. Let oldLen be oldLenDesc.[[Value]].
    uint32_t old_len = 0;
    CHECK(old_len_desc.value()->ToArrayLength(&old_len));
    // 3d. Let index be ToUint32(P).
    // (Already done above.)
    // 3e. (Assert)
    // 3f. If index >= oldLen and oldLenDesc.[[Writable]] is false,
    //     return false.
    if (index >= old_len && old_len_desc.has_writable() &&
        !old_len_desc.writable()) {
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kDefineDisallowed, name));
    }
    // 3g. Let succeeded be OrdinaryDefineOwnProperty(A, P, Desc).
    Maybe<bool> succeeded =
        OrdinaryDefineOwnProperty(isolate, o, name, desc, should_throw);
    // 3h. Assert: succeeded is not an abrupt completion.
    //     In our case, if should_throw == THROW_ON_ERROR, it can be!
    // 3i. If succeeded is false, return false.
    if (succeeded.IsNothing() || !succeeded.FromJust()) return succeeded;
    // 3j. If index >= oldLen, then:
    if (index >= old_len) {
      // 3j i. Set oldLenDesc.[[Value]] to index + 1.
      old_len_desc.set_value(isolate->factory()->NewNumberFromUint(index + 1));
      // 3j ii. Let succeeded be
      //        OrdinaryDefineOwnProperty(A, "length", oldLenDesc).
      succeeded = OrdinaryDefineOwnProperty(isolate, o,
                                            isolate->factory()->length_string(),
                                            &old_len_desc, should_throw);
      // 3j iii. Assert: succeeded is true.
      DCHECK(succeeded.FromJust());
      USE(succeeded);
    }
    // 3k. Return true.
    return Just(true);
  }

  // 4. Return OrdinaryDefineOwnProperty(A, P, Desc).
  return OrdinaryDefineOwnProperty(isolate, o, name, desc, should_throw);
}


// Part of ES6 9.4.2.4 ArraySetLength.
// static
bool JSArray::AnythingToArrayLength(Isolate* isolate,
                                    Handle<Object> length_object,
                                    uint32_t* output) {
  // Fast path: check numbers and strings that can be converted directly
  // and unobservably.
  if (length_object->ToArrayLength(output)) return true;
  if (length_object->IsString() &&
      Handle<String>::cast(length_object)->AsArrayIndex(output)) {
    return true;
  }
  // Slow path: follow steps in ES6 9.4.2.4 "ArraySetLength".
  // 3. Let newLen be ToUint32(Desc.[[Value]]).
  Handle<Object> uint32_v;
  if (!Object::ToUint32(isolate, length_object).ToHandle(&uint32_v)) {
    // 4. ReturnIfAbrupt(newLen).
    return false;
  }
  // 5. Let numberLen be ToNumber(Desc.[[Value]]).
  Handle<Object> number_v;
  if (!Object::ToNumber(length_object).ToHandle(&number_v)) {
    // 6. ReturnIfAbrupt(newLen).
    return false;
  }
  // 7. If newLen != numberLen, throw a RangeError exception.
  if (uint32_v->Number() != number_v->Number()) {
    Handle<Object> exception =
        isolate->factory()->NewRangeError(MessageTemplate::kInvalidArrayLength);
    isolate->Throw(*exception);
    return false;
  }
  CHECK(uint32_v->ToArrayLength(output));
  return true;
}


// ES6 9.4.2.4
// static
Maybe<bool> JSArray::ArraySetLength(Isolate* isolate, Handle<JSArray> a,
                                    PropertyDescriptor* desc,
                                    ShouldThrow should_throw) {
  // 1. If the [[Value]] field of Desc is absent, then
  if (!desc->has_value()) {
    // 1a. Return OrdinaryDefineOwnProperty(A, "length", Desc).
    return OrdinaryDefineOwnProperty(
        isolate, a, isolate->factory()->length_string(), desc, should_throw);
  }
  // 2. Let newLenDesc be a copy of Desc.
  // (Actual copying is not necessary.)
  PropertyDescriptor* new_len_desc = desc;
  // 3. - 7. Convert Desc.[[Value]] to newLen.
  uint32_t new_len = 0;
  if (!AnythingToArrayLength(isolate, desc->value(), &new_len)) {
    DCHECK(isolate->has_pending_exception());
    return Nothing<bool>();
  }
  // 8. Set newLenDesc.[[Value]] to newLen.
  // (Done below, if needed.)
  // 9. Let oldLenDesc be OrdinaryGetOwnProperty(A, "length").
  PropertyDescriptor old_len_desc;
  Maybe<bool> success = GetOwnPropertyDescriptor(
      isolate, a, isolate->factory()->length_string(), &old_len_desc);
  // 10. (Assert)
  DCHECK(success.FromJust());
  USE(success);
  // 11. Let oldLen be oldLenDesc.[[Value]].
  uint32_t old_len = 0;
  CHECK(old_len_desc.value()->ToArrayLength(&old_len));
  // 12. If newLen >= oldLen, then
  if (new_len >= old_len) {
    // 8. Set newLenDesc.[[Value]] to newLen.
    // 12a. Return OrdinaryDefineOwnProperty(A, "length", newLenDesc).
    new_len_desc->set_value(isolate->factory()->NewNumberFromUint(new_len));
    return OrdinaryDefineOwnProperty(isolate, a,
                                     isolate->factory()->length_string(),
                                     new_len_desc, should_throw);
  }
  // 13. If oldLenDesc.[[Writable]] is false, return false.
  if (!old_len_desc.writable()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kRedefineDisallowed,
                                isolate->factory()->length_string()));
  }
  // 14. If newLenDesc.[[Writable]] is absent or has the value true,
  // let newWritable be true.
  bool new_writable = false;
  if (!new_len_desc->has_writable() || new_len_desc->writable()) {
    new_writable = true;
  } else {
    // 15. Else,
    // 15a. Need to defer setting the [[Writable]] attribute to false in case
    //      any elements cannot be deleted.
    // 15b. Let newWritable be false. (It's initialized as "false" anyway.)
    // 15c. Set newLenDesc.[[Writable]] to true.
    // (Not needed.)
  }
  // Most of steps 16 through 19 is implemented by JSArray::SetLength.
  if (JSArray::ObservableSetLength(a, new_len).is_null()) {
    DCHECK(isolate->has_pending_exception());
    return Nothing<bool>();
  }
  // Steps 19d-ii, 20.
  if (!new_writable) {
    PropertyDescriptor readonly;
    readonly.set_writable(false);
    Maybe<bool> success = OrdinaryDefineOwnProperty(
        isolate, a, isolate->factory()->length_string(), &readonly,
        should_throw);
    DCHECK(success.FromJust());
    USE(success);
  }
  uint32_t actual_new_len = 0;
  CHECK(a->length()->ToArrayLength(&actual_new_len));
  // Steps 19d-v, 21. Return false if there were non-deletable elements.
  bool result = actual_new_len == new_len;
  if (!result) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kStrictDeleteProperty,
                     isolate->factory()->NewNumberFromUint(actual_new_len - 1),
                     a));
  }
  return Just(result);
}


// ES6 9.5.6
// static
Maybe<bool> JSProxy::DefineOwnProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                       Handle<Object> key,
                                       PropertyDescriptor* desc,
                                       ShouldThrow should_throw) {
  STACK_CHECK(Nothing<bool>());
  if (key->IsSymbol() && Handle<Symbol>::cast(key)->IsPrivate()) {
    return SetPrivateProperty(isolate, proxy, Handle<Symbol>::cast(key), desc,
                              should_throw);
  }
  Handle<String> trap_name = isolate->factory()->defineProperty_string();
  // 1. Assert: IsPropertyKey(P) is true.
  DCHECK(key->IsName() || key->IsNumber());
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 6. Let trap be ? GetMethod(handler, "defineProperty").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then:
  if (trap->IsUndefined()) {
    // 7a. Return target.[[DefineOwnProperty]](P, Desc).
    return JSReceiver::DefineOwnProperty(isolate, target, key, desc,
                                         should_throw);
  }
  // 8. Let descObj be FromPropertyDescriptor(Desc).
  Handle<Object> desc_obj = desc->ToObject(isolate);
  // 9. Let booleanTrapResult be
  //    ToBoolean(? Call(trap, handler, «target, P, descObj»)).
  Handle<Name> property_name =
      key->IsName()
          ? Handle<Name>::cast(key)
          : Handle<Name>::cast(isolate->factory()->NumberToString(key));
  // Do not leak private property names.
  DCHECK(!property_name->IsPrivate());
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, property_name, desc_obj};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 10. If booleanTrapResult is false, return false.
  if (!trap_result_obj->BooleanValue()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, property_name));
  }
  // 11. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, key, &target_desc);
  MAYBE_RETURN(target_found, Nothing<bool>());
  // 12. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> maybe_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(maybe_extensible, Nothing<bool>());
  bool extensible_target = maybe_extensible.FromJust();
  // 13. If Desc has a [[Configurable]] field and if Desc.[[Configurable]]
  //     is false, then:
  // 13a. Let settingConfigFalse be true.
  // 14. Else let settingConfigFalse be false.
  bool setting_config_false = desc->has_configurable() && !desc->configurable();
  // 15. If targetDesc is undefined, then
  if (!target_found.FromJust()) {
    // 15a. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonExtensible, property_name));
      return Nothing<bool>();
    }
    // 15b. If settingConfigFalse is true, throw a TypeError exception.
    if (setting_config_false) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonConfigurable, property_name));
      return Nothing<bool>();
    }
  } else {
    // 16. Else targetDesc is not undefined,
    // 16a. If IsCompatiblePropertyDescriptor(extensibleTarget, Desc,
    //      targetDesc) is false, throw a TypeError exception.
    Maybe<bool> valid =
        IsCompatiblePropertyDescriptor(isolate, extensible_target, desc,
                                       &target_desc, property_name, DONT_THROW);
    MAYBE_RETURN(valid, Nothing<bool>());
    if (!valid.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyIncompatible, property_name));
      return Nothing<bool>();
    }
    // 16b. If settingConfigFalse is true and targetDesc.[[Configurable]] is
    //      true, throw a TypeError exception.
    if (setting_config_false && target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDefinePropertyNonConfigurable, property_name));
      return Nothing<bool>();
    }
  }
  // 17. Return true.
  return Just(true);
}


// static
Maybe<bool> JSProxy::SetPrivateProperty(Isolate* isolate, Handle<JSProxy> proxy,
                                        Handle<Symbol> private_name,
                                        PropertyDescriptor* desc,
                                        ShouldThrow should_throw) {
  // Despite the generic name, this can only add private data properties.
  if (!PropertyDescriptor::IsDataDescriptor(desc) ||
      desc->ToAttributes() != DONT_ENUM) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyPrivate));
  }
  DCHECK(proxy->map()->is_dictionary_map());
  Handle<Object> value =
      desc->has_value()
          ? desc->value()
          : Handle<Object>::cast(isolate->factory()->undefined_value());

  LookupIterator it(proxy, private_name);

  if (it.IsFound()) {
    DCHECK_EQ(LookupIterator::DATA, it.state());
    DCHECK_EQ(DONT_ENUM, it.property_attributes());
    it.WriteDataValue(value);
    return Just(true);
  }

  Handle<NameDictionary> dict(proxy->property_dictionary());
  PropertyDetails details(DONT_ENUM, DATA, 0, PropertyCellType::kNoCell);
  Handle<NameDictionary> result =
      NameDictionary::Add(dict, private_name, value, details);
  if (!dict.is_identical_to(result)) proxy->set_properties(*result);
  return Just(true);
}


// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(Isolate* isolate,
                                                 Handle<JSReceiver> object,
                                                 Handle<Object> key,
                                                 PropertyDescriptor* desc) {
  bool success = false;
  DCHECK(key->IsName() || key->IsNumber());  // |key| is a PropertyKey...
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, key, &success, LookupIterator::HIDDEN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.
  return GetOwnPropertyDescriptor(&it, desc);
}


// ES6 9.1.5.1
// Returns true on success, false if the property didn't exist, nothing if
// an exception was thrown.
// static
Maybe<bool> JSReceiver::GetOwnPropertyDescriptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  Isolate* isolate = it->isolate();
  // "Virtual" dispatch.
  if (it->IsFound() && it->GetHolder<JSReceiver>()->IsJSProxy()) {
    return JSProxy::GetOwnPropertyDescriptor(isolate, it->GetHolder<JSProxy>(),
                                             it->GetName(), desc);
  }

  // 1. (Assert)
  // 2. If O does not have an own property with key P, return undefined.
  Maybe<PropertyAttributes> maybe = JSObject::GetPropertyAttributes(it);
  MAYBE_RETURN(maybe, Nothing<bool>());
  PropertyAttributes attrs = maybe.FromJust();
  if (attrs == ABSENT) return Just(false);
  DCHECK(!isolate->has_pending_exception());

  // 3. Let D be a newly created Property Descriptor with no fields.
  DCHECK(desc->is_empty());
  // 4. Let X be O's own property whose key is P.
  // 5. If X is a data property, then
  bool is_accessor_pair = it->state() == LookupIterator::ACCESSOR &&
                          it->GetAccessors()->IsAccessorPair();
  if (!is_accessor_pair) {
    // 5a. Set D.[[Value]] to the value of X's [[Value]] attribute.
    Handle<Object> value;
    if (!JSObject::GetProperty(it).ToHandle(&value)) {
      DCHECK(isolate->has_pending_exception());
      return Nothing<bool>();
    }
    desc->set_value(value);
    // 5b. Set D.[[Writable]] to the value of X's [[Writable]] attribute
    desc->set_writable((attrs & READ_ONLY) == 0);
  } else {
    // 6. Else X is an accessor property, so
    Handle<AccessorPair> accessors =
        Handle<AccessorPair>::cast(it->GetAccessors());
    // 6a. Set D.[[Get]] to the value of X's [[Get]] attribute.
    desc->set_get(AccessorPair::GetComponent(accessors, ACCESSOR_GETTER));
    // 6b. Set D.[[Set]] to the value of X's [[Set]] attribute.
    desc->set_set(AccessorPair::GetComponent(accessors, ACCESSOR_SETTER));
  }

  // 7. Set D.[[Enumerable]] to the value of X's [[Enumerable]] attribute.
  desc->set_enumerable((attrs & DONT_ENUM) == 0);
  // 8. Set D.[[Configurable]] to the value of X's [[Configurable]] attribute.
  desc->set_configurable((attrs & DONT_DELETE) == 0);
  // 9. Return D.
  DCHECK(PropertyDescriptor::IsAccessorDescriptor(desc) !=
         PropertyDescriptor::IsDataDescriptor(desc));
  return Just(true);
}


// ES6 9.5.5
// static
Maybe<bool> JSProxy::GetOwnPropertyDescriptor(Isolate* isolate,
                                              Handle<JSProxy> proxy,
                                              Handle<Name> name,
                                              PropertyDescriptor* desc) {
  DCHECK(!name->IsPrivate());
  STACK_CHECK(Nothing<bool>());

  Handle<String> trap_name =
      isolate->factory()->getOwnPropertyDescriptor_string();
  // 1. (Assert)
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 6. Let trap be ? GetMethod(handler, "getOwnPropertyDescriptor").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then
  if (trap->IsUndefined()) {
    // 7a. Return target.[[GetOwnProperty]](P).
    return JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, desc);
  }
  // 8. Let trapResultObj be ? Call(trap, handler, «target, P»).
  Handle<Object> trap_result_obj;
  Handle<Object> args[] = {target, name};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_obj,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 9. If Type(trapResultObj) is neither Object nor Undefined, throw a
  //    TypeError exception.
  if (!trap_result_obj->IsJSReceiver() && !trap_result_obj->IsUndefined()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyGetOwnPropertyDescriptorInvalid, name));
    return Nothing<bool>();
  }
  // 10. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(found, Nothing<bool>());
  // 11. If trapResultObj is undefined, then
  if (trap_result_obj->IsUndefined()) {
    // 11a. If targetDesc is undefined, return undefined.
    if (!found.FromJust()) return Just(false);
    // 11b. If targetDesc.[[Configurable]] is false, throw a TypeError
    //      exception.
    if (!target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorUndefined, name));
      return Nothing<bool>();
    }
    // 11c. Let extensibleTarget be ? IsExtensible(target).
    Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
    MAYBE_RETURN(extensible_target, Nothing<bool>());
    // 11d. (Assert)
    // 11e. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorNonExtensible, name));
      return Nothing<bool>();
    }
    // 11f. Return undefined.
    return Just(false);
  }
  // 12. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(extensible_target, Nothing<bool>());
  // 13. Let resultDesc be ? ToPropertyDescriptor(trapResultObj).
  if (!PropertyDescriptor::ToPropertyDescriptor(isolate, trap_result_obj,
                                                desc)) {
    DCHECK(isolate->has_pending_exception());
    return Nothing<bool>();
  }
  // 14. Call CompletePropertyDescriptor(resultDesc).
  PropertyDescriptor::CompletePropertyDescriptor(isolate, desc);
  // 15. Let valid be IsCompatiblePropertyDescriptor (extensibleTarget,
  //     resultDesc, targetDesc).
  Maybe<bool> valid =
      IsCompatiblePropertyDescriptor(isolate, extensible_target.FromJust(),
                                     desc, &target_desc, name, DONT_THROW);
  MAYBE_RETURN(valid, Nothing<bool>());
  // 16. If valid is false, throw a TypeError exception.
  if (!valid.FromJust()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyGetOwnPropertyDescriptorIncompatible, name));
    return Nothing<bool>();
  }
  // 17. If resultDesc.[[Configurable]] is false, then
  if (!desc->configurable()) {
    // 17a. If targetDesc is undefined or targetDesc.[[Configurable]] is true:
    if (target_desc.is_empty() || target_desc.configurable()) {
      // 17a i. Throw a TypeError exception.
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyGetOwnPropertyDescriptorNonConfigurable,
          name));
      return Nothing<bool>();
    }
  }
  // 18. Return resultDesc.
  return Just(true);
}


bool JSObject::ReferencesObjectFromElements(FixedArray* elements,
                                            ElementsKind kind,
                                            Object* object) {
  if (IsFastObjectElementsKind(kind) || kind == FAST_STRING_WRAPPER_ELEMENTS) {
    int length = IsJSArray()
        ? Smi::cast(JSArray::cast(this)->length())->value()
        : elements->length();
    for (int i = 0; i < length; ++i) {
      Object* element = elements->get(i);
      if (!element->IsTheHole() && element == object) return true;
    }
  } else {
    DCHECK(kind == DICTIONARY_ELEMENTS || kind == SLOW_STRING_WRAPPER_ELEMENTS);
    Object* key =
        SeededNumberDictionary::cast(elements)->SlowReverseLookup(object);
    if (!key->IsUndefined()) return true;
  }
  return false;
}


// Check whether this object references another object.
bool JSObject::ReferencesObject(Object* obj) {
  Map* map_of_this = map();
  Heap* heap = GetHeap();
  DisallowHeapAllocation no_allocation;

  // Is the object the constructor for this object?
  if (map_of_this->GetConstructor() == obj) {
    return true;
  }

  // Is the object the prototype for this object?
  if (map_of_this->prototype() == obj) {
    return true;
  }

  // Check if the object is among the named properties.
  Object* key = SlowReverseLookup(obj);
  if (!key->IsUndefined()) {
    return true;
  }

  // Check if the object is among the indexed properties.
  ElementsKind kind = GetElementsKind();
  switch (kind) {
    // Raw pixels and external arrays do not reference other
    // objects.
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
    case TYPE##_ELEMENTS:                                                      \
      break;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      break;
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
      break;
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(this->elements());
      if (ReferencesObjectFromElements(elements, kind, obj)) return true;
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* parameter_map = FixedArray::cast(elements());
      // Check the mapped parameters.
      int length = parameter_map->length();
      for (int i = 2; i < length; ++i) {
        Object* value = parameter_map->get(i);
        if (!value->IsTheHole() && value == obj) return true;
      }
      // Check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      kind = arguments->IsDictionary() ? DICTIONARY_ELEMENTS :
          FAST_HOLEY_ELEMENTS;
      if (ReferencesObjectFromElements(arguments, kind, obj)) return true;
      break;
    }
    case NO_ELEMENTS:
      break;
  }

  // For functions check the context.
  if (IsJSFunction()) {
    // Get the constructor function for arguments array.
    Map* arguments_map =
        heap->isolate()->context()->native_context()->sloppy_arguments_map();
    JSFunction* arguments_function =
        JSFunction::cast(arguments_map->GetConstructor());

    // Get the context and don't check if it is the native context.
    JSFunction* f = JSFunction::cast(this);
    Context* context = f->context();
    if (context->IsNativeContext()) {
      return false;
    }

    // Check the non-special context slots.
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context->length(); i++) {
      // Only check JS objects.
      if (context->get(i)->IsJSObject()) {
        JSObject* ctxobj = JSObject::cast(context->get(i));
        // If it is an arguments array check the content.
        if (ctxobj->map()->GetConstructor() == arguments_function) {
          if (ctxobj->ReferencesObject(obj)) {
            return true;
          }
        } else if (ctxobj == obj) {
          return true;
        }
      }
    }

    // Check the context extension (if any) if it can have references.
    if (context->has_extension() && !context->IsCatchContext()) {
      // With harmony scoping, a JSFunction may have a script context.
      // TODO(mvstanton): walk into the ScopeInfo.
      if (context->IsScriptContext()) {
        return false;
      }

      return context->extension_object()->ReferencesObject(obj);
    }
  }

  // No references to object.
  return false;
}


Maybe<bool> JSReceiver::SetIntegrityLevel(Handle<JSReceiver> receiver,
                                          IntegrityLevel level,
                                          ShouldThrow should_throw) {
  DCHECK(level == SEALED || level == FROZEN);

  if (receiver->IsJSObject()) {
    Handle<JSObject> object = Handle<JSObject>::cast(receiver);
    if (!object->HasSloppyArgumentsElements() &&
        !object->map()->is_observed() &&
        (!object->map()->is_strong() || level == SEALED)) {  // Fast path.
      if (level == SEALED) {
        return JSObject::PreventExtensionsWithTransition<SEALED>(object,
                                                                 should_throw);
      } else {
        return JSObject::PreventExtensionsWithTransition<FROZEN>(object,
                                                                 should_throw);
      }
    }
  }

  Isolate* isolate = receiver->GetIsolate();

  MAYBE_RETURN(JSReceiver::PreventExtensions(receiver, should_throw),
               Nothing<bool>());

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(receiver), Nothing<bool>());

  PropertyDescriptor no_conf;
  no_conf.set_configurable(false);

  PropertyDescriptor no_conf_no_write;
  no_conf_no_write.set_configurable(false);
  no_conf_no_write.set_writable(false);

  if (level == SEALED) {
    for (int i = 0; i < keys->length(); ++i) {
      Handle<Object> key(keys->get(i), isolate);
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &no_conf, THROW_ON_ERROR),
          Nothing<bool>());
    }
    return Just(true);
  }

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, receiver, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      PropertyDescriptor desc =
          PropertyDescriptor::IsAccessorDescriptor(&current_desc)
              ? no_conf
              : no_conf_no_write;
      MAYBE_RETURN(
          DefineOwnProperty(isolate, receiver, key, &desc, THROW_ON_ERROR),
          Nothing<bool>());
    }
  }
  return Just(true);
}


Maybe<bool> JSReceiver::TestIntegrityLevel(Handle<JSReceiver> object,
                                           IntegrityLevel level) {
  DCHECK(level == SEALED || level == FROZEN);
  Isolate* isolate = object->GetIsolate();

  Maybe<bool> extensible = JSReceiver::IsExtensible(object);
  MAYBE_RETURN(extensible, Nothing<bool>());
  if (extensible.FromJust()) return Just(false);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys, JSReceiver::OwnPropertyKeys(object), Nothing<bool>());

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> key(keys->get(i), isolate);
    PropertyDescriptor current_desc;
    Maybe<bool> owned = JSReceiver::GetOwnPropertyDescriptor(
        isolate, object, key, &current_desc);
    MAYBE_RETURN(owned, Nothing<bool>());
    if (owned.FromJust()) {
      if (current_desc.configurable()) return Just(false);
      if (level == FROZEN &&
          PropertyDescriptor::IsDataDescriptor(&current_desc) &&
          current_desc.writable()) {
        return Just(false);
      }
    }
  }
  return Just(true);
}


Maybe<bool> JSReceiver::PreventExtensions(Handle<JSReceiver> object,
                                          ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::PreventExtensions(Handle<JSProxy>::cast(object),
                                      should_throw);
  }
  DCHECK(object->IsJSObject());
  return JSObject::PreventExtensions(Handle<JSObject>::cast(object),
                                     should_throw);
}


Maybe<bool> JSProxy::PreventExtensions(Handle<JSProxy> proxy,
                                       ShouldThrow should_throw) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->preventExtensions_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(proxy->target(), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined()) {
    return JSReceiver::PreventExtensions(target, should_throw);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue()) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }

  // Enforce the invariant.
  Maybe<bool> target_result = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(target_result, Nothing<bool>());
  if (target_result.FromJust()) {
    isolate->Throw(*factory->NewTypeError(
        MessageTemplate::kProxyPreventExtensionsExtensible));
    return Nothing<bool>();
  }
  return Just(true);
}


Maybe<bool> JSObject::PreventExtensions(Handle<JSObject> object,
                                        ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

  if (!object->HasSloppyArgumentsElements() && !object->map()->is_observed()) {
    return PreventExtensionsWithTransition<NONE>(object, should_throw);
  }

  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (!object->map()->is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensions(PrototypeIterator::GetCurrent<JSObject>(iter),
                             should_throw);
  }

  if (!object->HasFixedTypedArrayElements()) {
    // If there are fast elements we normalize.
    Handle<SeededNumberDictionary> dictionary = NormalizeElements(object);
    DCHECK(object->HasDictionaryElements() ||
           object->HasSlowArgumentsElements());

    // Make sure that we never go back to fast case.
    object->RequireSlowElements(*dictionary);
  }

  // Do a map transition, other objects with this map may still
  // be extensible.
  // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
  Handle<Map> new_map = Map::Copy(handle(object->map()), "PreventExtensions");

  new_map->set_is_extensible(false);
  JSObject::MigrateToMap(object, new_map);
  DCHECK(!object->map()->is_extensible());

  if (object->map()->is_observed()) {
    RETURN_ON_EXCEPTION_VALUE(
        isolate,
        EnqueueChangeRecord(object, "preventExtensions", Handle<Name>(),
                            isolate->factory()->the_hole_value()),
        Nothing<bool>());
  }
  return Just(true);
}


Maybe<bool> JSReceiver::IsExtensible(Handle<JSReceiver> object) {
  if (object->IsJSProxy()) {
    return JSProxy::IsExtensible(Handle<JSProxy>::cast(object));
  }
  return Just(JSObject::IsExtensible(Handle<JSObject>::cast(object)));
}


Maybe<bool> JSProxy::IsExtensible(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->isExtensible_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(proxy->target(), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined()) {
    return JSReceiver::IsExtensible(target);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());

  // Enforce the invariant.
  Maybe<bool> target_result = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(target_result, Nothing<bool>());
  if (target_result.FromJust() != trap_result->BooleanValue()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyIsExtensibleInconsistent,
                               factory->ToBoolean(target_result.FromJust())));
    return Nothing<bool>();
  }
  return target_result;
}


bool JSObject::IsExtensible(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), object)) {
    return true;
  }
  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, *object);
    if (iter.IsAtEnd()) return false;
    DCHECK(iter.GetCurrent()->IsJSGlobalObject());
    return iter.GetCurrent<JSObject>()->map()->is_extensible();
  }
  return object->map()->is_extensible();
}


template <typename Dictionary>
static void ApplyAttributesToDictionary(Dictionary* dictionary,
                                        const PropertyAttributes attributes) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(k) &&
        !(k->IsSymbol() && Symbol::cast(k)->is_private())) {
      PropertyDetails details = dictionary->DetailsAt(i);
      int attrs = attributes;
      // READ_ONLY is an invalid attribute for JS setters/getters.
      if ((attributes & READ_ONLY) && details.type() == ACCESSOR_CONSTANT) {
        Object* v = dictionary->ValueAt(i);
        if (v->IsPropertyCell()) v = PropertyCell::cast(v)->value();
        if (v->IsAccessorPair()) attrs &= ~READ_ONLY;
      }
      details = details.CopyAddAttributes(
          static_cast<PropertyAttributes>(attrs));
      dictionary->DetailsAtPut(i, details);
    }
  }
}


template <PropertyAttributes attrs>
Maybe<bool> JSObject::PreventExtensionsWithTransition(
    Handle<JSObject> object, ShouldThrow should_throw) {
  STATIC_ASSERT(attrs == NONE || attrs == SEALED || attrs == FROZEN);

  // Sealing/freezing sloppy arguments should be handled elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());
  DCHECK(!object->map()->is_observed());

  Isolate* isolate = object->GetIsolate();
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), object)) {
    isolate->ReportFailedAccessCheck(object);
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNoAccess));
  }

  if (attrs == NONE && !object->map()->is_extensible()) return Just(true);

  if (object->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, object);
    if (iter.IsAtEnd()) return Just(true);
    DCHECK(PrototypeIterator::GetCurrent(iter)->IsJSGlobalObject());
    return PreventExtensionsWithTransition<attrs>(
        PrototypeIterator::GetCurrent<JSObject>(iter), should_throw);
  }

  Handle<SeededNumberDictionary> new_element_dictionary;
  if (!object->HasFixedTypedArrayElements() &&
      !object->HasDictionaryElements() &&
      !object->HasSlowStringWrapperElements()) {
    int length =
        object->IsJSArray()
            ? Smi::cast(Handle<JSArray>::cast(object)->length())->value()
            : object->elements()->length();
    new_element_dictionary =
        length == 0 ? isolate->factory()->empty_slow_element_dictionary()
                    : GetNormalizedElementDictionary(
                          object, handle(object->elements()));
  }

  Handle<Symbol> transition_marker;
  if (attrs == NONE) {
    transition_marker = isolate->factory()->nonextensible_symbol();
  } else if (attrs == SEALED) {
    transition_marker = isolate->factory()->sealed_symbol();
  } else {
    DCHECK(attrs == FROZEN);
    transition_marker = isolate->factory()->frozen_symbol();
  }

  Handle<Map> old_map(object->map(), isolate);
  Map* transition =
      TransitionArray::SearchSpecial(*old_map, *transition_marker);
  if (transition != NULL) {
    Handle<Map> transition_map(transition, isolate);
    DCHECK(transition_map->has_dictionary_elements() ||
           transition_map->has_fixed_typed_array_elements() ||
           transition_map->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
    DCHECK(!transition_map->is_extensible());
    JSObject::MigrateToMap(object, transition_map);
  } else if (TransitionArray::CanHaveMoreTransitions(old_map)) {
    // Create a new descriptor array with the appropriate property attributes
    Handle<Map> new_map = Map::CopyForPreventExtensions(
        old_map, attrs, transition_marker, "CopyForPreventExtensions");
    JSObject::MigrateToMap(object, new_map);
  } else {
    DCHECK(old_map->is_dictionary_map() || !old_map->is_prototype_map());
    // Slow path: need to normalize properties for safety
    NormalizeProperties(object, CLEAR_INOBJECT_PROPERTIES, 0,
                        "SlowPreventExtensions");

    // Create a new map, since other objects with this map may be extensible.
    // TODO(adamk): Extend the NormalizedMapCache to handle non-extensible maps.
    Handle<Map> new_map =
        Map::Copy(handle(object->map()), "SlowCopyForPreventExtensions");
    new_map->set_is_extensible(false);
    if (!new_element_dictionary.is_null()) {
      ElementsKind new_kind =
          IsStringWrapperElementsKind(old_map->elements_kind())
              ? SLOW_STRING_WRAPPER_ELEMENTS
              : DICTIONARY_ELEMENTS;
      new_map->set_elements_kind(new_kind);
    }
    JSObject::MigrateToMap(object, new_map);

    if (attrs != NONE) {
      if (object->IsJSGlobalObject()) {
        ApplyAttributesToDictionary(object->global_dictionary(), attrs);
      } else {
        ApplyAttributesToDictionary(object->property_dictionary(), attrs);
      }
    }
  }

  // Both seal and preventExtensions always go through without modifications to
  // typed array elements. Freeze works only if there are no actual elements.
  if (object->HasFixedTypedArrayElements()) {
    if (attrs == FROZEN &&
        JSArrayBufferView::cast(*object)->byte_length()->Number() > 0) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kCannotFreezeArrayBufferView));
      return Nothing<bool>();
    }
    return Just(true);
  }

  DCHECK(object->map()->has_dictionary_elements() ||
         object->map()->elements_kind() == SLOW_STRING_WRAPPER_ELEMENTS);
  if (!new_element_dictionary.is_null()) {
    object->set_elements(*new_element_dictionary);
  }

  if (object->elements() != isolate->heap()->empty_slow_element_dictionary()) {
    SeededNumberDictionary* dictionary = object->element_dictionary();
    // Make sure we never go back to the fast case
    object->RequireSlowElements(dictionary);
    if (attrs != NONE) {
      ApplyAttributesToDictionary(dictionary, attrs);
    }
  }

  return Just(true);
}


void JSObject::SetObserved(Handle<JSObject> object) {
  DCHECK(!object->IsJSGlobalProxy());
  DCHECK(!object->IsJSGlobalObject());
  Isolate* isolate = object->GetIsolate();
  Handle<Map> new_map;
  Handle<Map> old_map(object->map(), isolate);
  DCHECK(!old_map->is_observed());
  Map* transition = TransitionArray::SearchSpecial(
      *old_map, isolate->heap()->observed_symbol());
  if (transition != NULL) {
    new_map = handle(transition, isolate);
    DCHECK(new_map->is_observed());
  } else if (TransitionArray::CanHaveMoreTransitions(old_map)) {
    new_map = Map::CopyForObserved(old_map);
  } else {
    new_map = Map::Copy(old_map, "SlowObserved");
    new_map->set_is_observed();
  }
  JSObject::MigrateToMap(object, new_map);
}


Handle<Object> JSObject::FastPropertyAt(Handle<JSObject> object,
                                        Representation representation,
                                        FieldIndex index) {
  Isolate* isolate = object->GetIsolate();
  if (object->IsUnboxedDoubleField(index)) {
    double value = object->RawFastDoublePropertyAt(index);
    return isolate->factory()->NewHeapNumber(value);
  }
  Handle<Object> raw_value(object->RawFastPropertyAt(index), isolate);
  return Object::WrapForRead(isolate, raw_value, representation);
}

enum class BoilerplateKind { kNormalBoilerplate, kApiBoilerplate };

template <class ContextObject, BoilerplateKind boilerplate_kind>
class JSObjectWalkVisitor {
 public:
  JSObjectWalkVisitor(ContextObject* site_context, bool copying,
                      JSObject::DeepCopyHints hints)
    : site_context_(site_context),
      copying_(copying),
      hints_(hints) {}

  MUST_USE_RESULT MaybeHandle<JSObject> StructureWalk(Handle<JSObject> object);

 protected:
  MUST_USE_RESULT inline MaybeHandle<JSObject> VisitElementOrProperty(
      Handle<JSObject> object,
      Handle<JSObject> value) {
    Handle<AllocationSite> current_site = site_context()->EnterNewScope();
    MaybeHandle<JSObject> copy_of_value = StructureWalk(value);
    site_context()->ExitScope(current_site, value);
    return copy_of_value;
  }

  inline ContextObject* site_context() { return site_context_; }
  inline Isolate* isolate() { return site_context()->isolate(); }

  inline bool copying() const { return copying_; }

 private:
  ContextObject* site_context_;
  const bool copying_;
  const JSObject::DeepCopyHints hints_;
};

template <class ContextObject, BoilerplateKind boilerplate_kind>
MaybeHandle<JSObject> JSObjectWalkVisitor<
    ContextObject, boilerplate_kind>::StructureWalk(Handle<JSObject> object) {
  Isolate* isolate = this->isolate();
  bool copying = this->copying();
  bool shallow = hints_ == JSObject::kObjectIsShallow;

  if (!shallow) {
    StackLimitCheck check(isolate);

    if (check.HasOverflowed()) {
      isolate->StackOverflow();
      return MaybeHandle<JSObject>();
    }
  }

  if (object->map()->is_deprecated()) {
    JSObject::MigrateInstance(object);
  }

  Handle<JSObject> copy;
  if (copying) {
    if (boilerplate_kind == BoilerplateKind::kApiBoilerplate) {
      if (object->IsJSFunction()) {
#ifdef DEBUG
        // Ensure that it is an Api function and template_instantiations_cache
        // contains an entry for function's FunctionTemplateInfo.
        JSFunction* function = JSFunction::cast(*object);
        CHECK(function->shared()->IsApiFunction());
        FunctionTemplateInfo* data = function->shared()->get_api_func_data();
        auto serial_number = handle(Smi::cast(data->serial_number()), isolate);
        CHECK(serial_number->value());
        auto cache = isolate->template_instantiations_cache();
        Object* element = cache->Lookup(serial_number);
        CHECK_EQ(function, element);
#endif
        return object;
      }
    } else {
      // JSFunction objects are not allowed to be in normal boilerplates at all.
      DCHECK(!object->IsJSFunction());
    }
    Handle<AllocationSite> site_to_pass;
    if (site_context()->ShouldCreateMemento(object)) {
      site_to_pass = site_context()->current();
    }
    copy = isolate->factory()->CopyJSObjectWithAllocationSite(
        object, site_to_pass);
  } else {
    copy = object;
  }

  DCHECK(copying || copy.is_identical_to(object));

  ElementsKind kind = copy->GetElementsKind();
  if (copying && IsFastSmiOrObjectElementsKind(kind) &&
      FixedArray::cast(copy->elements())->map() ==
        isolate->heap()->fixed_cow_array_map()) {
    isolate->counters()->cow_arrays_created_runtime()->Increment();
  }

  if (!shallow) {
    HandleScope scope(isolate);

    // Deep copy own properties.
    if (copy->HasFastProperties()) {
      Handle<DescriptorArray> descriptors(copy->map()->instance_descriptors());
      int limit = copy->map()->NumberOfOwnDescriptors();
      for (int i = 0; i < limit; i++) {
        PropertyDetails details = descriptors->GetDetails(i);
        if (details.type() != DATA) continue;
        FieldIndex index = FieldIndex::ForDescriptor(copy->map(), i);
        if (object->IsUnboxedDoubleField(index)) {
          if (copying) {
            double value = object->RawFastDoublePropertyAt(index);
            copy->RawFastDoublePropertyAtPut(index, value);
          }
        } else {
          Handle<Object> value(object->RawFastPropertyAt(index), isolate);
          if (value->IsJSObject()) {
            ASSIGN_RETURN_ON_EXCEPTION(
                isolate, value,
                VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
                JSObject);
            if (copying) {
              copy->FastPropertyAtPut(index, *value);
            }
          } else {
            if (copying) {
              Representation representation = details.representation();
              value = Object::NewStorageFor(isolate, value, representation);
              copy->FastPropertyAtPut(index, *value);
            }
          }
        }
      }
    } else {
      // Only deep copy fields from the object literal expression.
      // In particular, don't try to copy the length attribute of
      // an array.
      PropertyFilter filter = static_cast<PropertyFilter>(
          ONLY_WRITABLE | ONLY_ENUMERABLE | ONLY_CONFIGURABLE);
      KeyAccumulator accumulator(isolate, OWN_ONLY, filter);
      accumulator.NextPrototype();
      copy->CollectOwnPropertyNames(&accumulator, filter);
      Handle<FixedArray> names = accumulator.GetKeys();
      for (int i = 0; i < names->length(); i++) {
        DCHECK(names->get(i)->IsName());
        Handle<Name> name(Name::cast(names->get(i)));
        Handle<Object> value =
            Object::GetProperty(copy, name).ToHandleChecked();
        if (value->IsJSObject()) {
          Handle<JSObject> result;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, result,
              VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
              JSObject);
          if (copying) {
            // Creating object copy for literals. No strict mode needed.
            JSObject::SetProperty(copy, name, result, SLOPPY).Assert();
          }
        }
      }
    }

    // Deep copy own elements.
    switch (kind) {
      case FAST_ELEMENTS:
      case FAST_HOLEY_ELEMENTS: {
        Handle<FixedArray> elements(FixedArray::cast(copy->elements()));
        if (elements->map() == isolate->heap()->fixed_cow_array_map()) {
#ifdef DEBUG
          for (int i = 0; i < elements->length(); i++) {
            DCHECK(!elements->get(i)->IsJSObject());
          }
#endif
        } else {
          for (int i = 0; i < elements->length(); i++) {
            Handle<Object> value(elements->get(i), isolate);
            if (value->IsJSObject()) {
              Handle<JSObject> result;
              ASSIGN_RETURN_ON_EXCEPTION(
                  isolate, result,
                  VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
                  JSObject);
              if (copying) {
                elements->set(i, *result);
              }
            }
          }
        }
        break;
      }
      case DICTIONARY_ELEMENTS: {
        Handle<SeededNumberDictionary> element_dictionary(
            copy->element_dictionary());
        int capacity = element_dictionary->Capacity();
        for (int i = 0; i < capacity; i++) {
          Object* k = element_dictionary->KeyAt(i);
          if (element_dictionary->IsKey(k)) {
            Handle<Object> value(element_dictionary->ValueAt(i), isolate);
            if (value->IsJSObject()) {
              Handle<JSObject> result;
              ASSIGN_RETURN_ON_EXCEPTION(
                  isolate, result,
                  VisitElementOrProperty(copy, Handle<JSObject>::cast(value)),
                  JSObject);
              if (copying) {
                element_dictionary->ValueAtPut(i, *result);
              }
            }
          }
        }
        break;
      }
      case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
        UNIMPLEMENTED();
        break;
      case FAST_STRING_WRAPPER_ELEMENTS:
      case SLOW_STRING_WRAPPER_ELEMENTS:
        UNREACHABLE();
        break;

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
      case TYPE##_ELEMENTS:                                                    \

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // Typed elements cannot be created using an object literal.
      UNREACHABLE();
      break;

      case FAST_SMI_ELEMENTS:
      case FAST_HOLEY_SMI_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_HOLEY_DOUBLE_ELEMENTS:
      case NO_ELEMENTS:
        // No contained objects, nothing to do.
        break;
    }
  }

  return copy;
}


MaybeHandle<JSObject> JSObject::DeepWalk(
    Handle<JSObject> object,
    AllocationSiteCreationContext* site_context) {
  JSObjectWalkVisitor<AllocationSiteCreationContext,
                      BoilerplateKind::kNormalBoilerplate> v(site_context,
                                                             false, kNoHints);
  MaybeHandle<JSObject> result = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}


MaybeHandle<JSObject> JSObject::DeepCopy(
    Handle<JSObject> object,
    AllocationSiteUsageContext* site_context,
    DeepCopyHints hints) {
  JSObjectWalkVisitor<AllocationSiteUsageContext,
                      BoilerplateKind::kNormalBoilerplate> v(site_context, true,
                                                             hints);
  MaybeHandle<JSObject> copy = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!copy.ToHandle(&for_assert) || !for_assert.is_identical_to(object));
  return copy;
}

class DummyContextObject : public AllocationSiteContext {
 public:
  explicit DummyContextObject(Isolate* isolate)
      : AllocationSiteContext(isolate) {}

  bool ShouldCreateMemento(Handle<JSObject> object) { return false; }
  Handle<AllocationSite> EnterNewScope() { return Handle<AllocationSite>(); }
  void ExitScope(Handle<AllocationSite> site, Handle<JSObject> object) {}
};

MaybeHandle<JSObject> JSObject::DeepCopyApiBoilerplate(
    Handle<JSObject> object) {
  DummyContextObject dummy_context_object(object->GetIsolate());
  JSObjectWalkVisitor<DummyContextObject, BoilerplateKind::kApiBoilerplate> v(
      &dummy_context_object, true, kNoHints);
  MaybeHandle<JSObject> copy = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!copy.ToHandle(&for_assert) || !for_assert.is_identical_to(object));
  return copy;
}

// static
MaybeHandle<Object> JSReceiver::ToPrimitive(Handle<JSReceiver> receiver,
                                            ToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<Object> exotic_to_prim;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, exotic_to_prim,
      GetMethod(receiver, isolate->factory()->to_primitive_symbol()), Object);
  if (!exotic_to_prim->IsUndefined()) {
    Handle<Object> hint_string;
    switch (hint) {
      case ToPrimitiveHint::kDefault:
        hint_string = isolate->factory()->default_string();
        break;
      case ToPrimitiveHint::kNumber:
        hint_string = isolate->factory()->number_string();
        break;
      case ToPrimitiveHint::kString:
        hint_string = isolate->factory()->string_string();
        break;
    }
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, exotic_to_prim, receiver, 1, &hint_string),
        Object);
    if (result->IsPrimitive()) return result;
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                    Object);
  }
  return OrdinaryToPrimitive(receiver, (hint == ToPrimitiveHint::kString)
                                           ? OrdinaryToPrimitiveHint::kString
                                           : OrdinaryToPrimitiveHint::kNumber);
}


// static
MaybeHandle<Object> JSReceiver::OrdinaryToPrimitive(
    Handle<JSReceiver> receiver, OrdinaryToPrimitiveHint hint) {
  Isolate* const isolate = receiver->GetIsolate();
  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = isolate->factory()->valueOf_string();
      method_names[1] = isolate->factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = isolate->factory()->toString_string();
      method_names[1] = isolate->factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    Handle<Object> method;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, method,
                               JSReceiver::GetProperty(receiver, name), Object);
    if (method->IsCallable()) {
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, result, Execution::Call(isolate, method, receiver, 0, NULL),
          Object);
      if (result->IsPrimitive()) return result;
    }
  }
  THROW_NEW_ERROR(isolate,
                  NewTypeError(MessageTemplate::kCannotConvertToPrimitive),
                  Object);
}


// TODO(cbruni/jkummerow): Consider moving this into elements.cc.
bool HasEnumerableElements(JSObject* object) {
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      int length = object->IsJSArray()
                       ? Smi::cast(JSArray::cast(object)->length())->value()
                       : object->elements()->length();
      return length > 0;
    }
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(object->elements());
      int length = object->IsJSArray()
                       ? Smi::cast(JSArray::cast(object)->length())->value()
                       : elements->length();
      for (int i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) return true;
      }
      return false;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      int length = object->IsJSArray()
                       ? Smi::cast(JSArray::cast(object)->length())->value()
                       : object->elements()->length();
      // Zero-length arrays would use the empty FixedArray...
      if (length == 0) return false;
      // ...so only cast to FixedDoubleArray otherwise.
      FixedDoubleArray* elements = FixedDoubleArray::cast(object->elements());
      for (int i = 0; i < length; i++) {
        if (!elements->is_the_hole(i)) return true;
      }
      return false;
    }
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
    case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      {
        int length = object->elements()->length();
        return length > 0;
      }
    case DICTIONARY_ELEMENTS: {
      SeededNumberDictionary* elements =
          SeededNumberDictionary::cast(object->elements());
      return elements->NumberOfElementsFilterAttributes(ONLY_ENUMERABLE) > 0;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      // We're approximating non-empty arguments objects here.
      return true;
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      if (String::cast(JSValue::cast(object)->value())->length() > 0) {
        return true;
      }
      return object->elements()->length() > 0;
    case NO_ELEMENTS:
      return false;
  }
  UNREACHABLE();
  return true;
}


// Tests for the fast common case for property enumeration:
// - This object and all prototypes has an enum cache (which means that
//   it is no proxy, has no interceptors and needs no access checks).
// - This object has no elements.
// - No prototype has enumerable properties/elements.
bool JSReceiver::IsSimpleEnum() {
  for (PrototypeIterator iter(GetIsolate(), this,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (!iter.GetCurrent()->IsJSObject()) return false;
    JSObject* current = iter.GetCurrent<JSObject>();
    int enum_length = current->map()->EnumLength();
    if (enum_length == kInvalidEnumCacheSentinel) return false;
    if (current->IsAccessCheckNeeded()) return false;
    DCHECK(!current->HasNamedInterceptor());
    DCHECK(!current->HasIndexedInterceptor());
    if (HasEnumerableElements(current)) return false;
    if (current != this && enum_length != 0) return false;
  }
  return true;
}


int Map::NumberOfDescribedProperties(DescriptorFlag which,
                                     PropertyFilter filter) {
  int result = 0;
  DescriptorArray* descs = instance_descriptors();
  int limit = which == ALL_DESCRIPTORS
      ? descs->number_of_descriptors()
      : NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    if ((descs->GetDetails(i).attributes() & filter) == 0 &&
        !descs->GetKey(i)->FilterKey(filter)) {
      result++;
    }
  }
  return result;
}


int Map::NextFreePropertyIndex() {
  int free_index = 0;
  int number_of_own_descriptors = NumberOfOwnDescriptors();
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < number_of_own_descriptors; i++) {
    PropertyDetails details = descs->GetDetails(i);
    if (details.location() == kField) {
      int candidate = details.field_index() + details.field_width_in_words();
      if (candidate > free_index) free_index = candidate;
    }
  }
  return free_index;
}


static bool ContainsOnlyValidKeys(Handle<FixedArray> array) {
  int len = array->length();
  for (int i = 0; i < len; i++) {
    Object* e = array->get(i);
    if (!(e->IsName() || e->IsNumber())) return false;
  }
  return true;
}


static Handle<FixedArray> ReduceFixedArrayTo(
    Handle<FixedArray> array, int length) {
  DCHECK_LE(length, array->length());
  if (array->length() == length) return array;
  return array->GetIsolate()->factory()->CopyFixedArrayUpTo(array, length);
}

bool Map::OnlyHasSimpleProperties() {
  // Wrapped string elements aren't explicitly stored in the elements backing
  // store, but are loaded indirectly from the underlying string.
  return !IsStringWrapperElementsKind(elements_kind()) &&
         !is_access_check_needed() && !has_named_interceptor() &&
         !has_indexed_interceptor() && !has_hidden_prototype() &&
         !is_dictionary_map();
}

namespace {

Handle<FixedArray> GetFastEnumPropertyKeys(Isolate* isolate,
                                           Handle<JSObject> object) {
  Handle<Map> map(object->map());
  bool cache_enum_length = map->OnlyHasSimpleProperties();

  Handle<DescriptorArray> descs =
      Handle<DescriptorArray>(map->instance_descriptors(), isolate);
  int own_property_count = map->EnumLength();
  // If the enum length of the given map is set to kInvalidEnumCache, this
  // means that the map itself has never used the present enum cache. The
  // first step to using the cache is to set the enum length of the map by
  // counting the number of own descriptors that are ENUMERABLE_STRINGS.
  if (own_property_count == kInvalidEnumCacheSentinel) {
    own_property_count =
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS);
  } else {
    DCHECK(
        own_property_count ==
        map->NumberOfDescribedProperties(OWN_DESCRIPTORS, ENUMERABLE_STRINGS));
  }

  if (descs->HasEnumCache()) {
    Handle<FixedArray> keys(descs->GetEnumCache(), isolate);
    // In case the number of properties required in the enum are actually
    // present, we can reuse the enum cache. Otherwise, this means that the
    // enum cache was generated for a previous (smaller) version of the
    // Descriptor Array. In that case we regenerate the enum cache.
    if (own_property_count <= keys->length()) {
      isolate->counters()->enum_cache_hits()->Increment();
      if (cache_enum_length) map->SetEnumLength(own_property_count);
      return ReduceFixedArrayTo(keys, own_property_count);
    }
  }

  if (descs->IsEmpty()) {
    isolate->counters()->enum_cache_hits()->Increment();
    if (cache_enum_length) map->SetEnumLength(0);
    return isolate->factory()->empty_fixed_array();
  }

  isolate->counters()->enum_cache_misses()->Increment();

  Handle<FixedArray> storage =
      isolate->factory()->NewFixedArray(own_property_count);
  Handle<FixedArray> indices =
      isolate->factory()->NewFixedArray(own_property_count);

  int size = map->NumberOfOwnDescriptors();
  int index = 0;

  for (int i = 0; i < size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    Object* key = descs->GetKey(i);
    if (details.IsDontEnum() || key->IsSymbol()) continue;
    storage->set(index, key);
    if (!indices.is_null()) {
      if (details.type() != DATA) {
        indices = Handle<FixedArray>();
      } else {
        FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
        int load_by_field_index = field_index.GetLoadByFieldIndex();
        indices->set(index, Smi::FromInt(load_by_field_index));
      }
    }
    index++;
  }
  DCHECK(index == storage->length());

  DescriptorArray::SetEnumCache(descs, isolate, storage, indices);
  if (cache_enum_length) {
    map->SetEnumLength(own_property_count);
  }
  return storage;
}

}  // namespace

Handle<FixedArray> JSObject::GetEnumPropertyKeys(Handle<JSObject> object) {
  Isolate* isolate = object->GetIsolate();
  if (object->HasFastProperties()) {
    return GetFastEnumPropertyKeys(isolate, object);
  } else if (object->IsJSGlobalObject()) {
    Handle<GlobalDictionary> dictionary(object->global_dictionary());
    int length = dictionary->NumberOfEnumElements();
    if (length == 0) {
      return Handle<FixedArray>(isolate->heap()->empty_fixed_array());
    }
    Handle<FixedArray> storage = isolate->factory()->NewFixedArray(length);
    dictionary->CopyEnumKeysTo(*storage);
    return storage;
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary());
    int length = dictionary->NumberOfEnumElements();
    if (length == 0) {
      return Handle<FixedArray>(isolate->heap()->empty_fixed_array());
    }
    Handle<FixedArray> storage = isolate->factory()->NewFixedArray(length);
    dictionary->CopyEnumKeysTo(*storage);
    return storage;
  }
}


enum IndexedOrNamed { kIndexed, kNamed };


// Returns |true| on success, |nothing| on exception.
template <class Callback, IndexedOrNamed type>
static Maybe<bool> GetKeysFromInterceptor(Isolate* isolate,
                                          Handle<JSReceiver> receiver,
                                          Handle<JSObject> object,
                                          PropertyFilter filter,
                                          KeyAccumulator* accumulator) {
  if (type == kIndexed) {
    if (!object->HasIndexedInterceptor()) return Just(true);
  } else {
    if (!object->HasNamedInterceptor()) return Just(true);
  }
  Handle<InterceptorInfo> interceptor(type == kIndexed
                                          ? object->GetIndexedInterceptor()
                                          : object->GetNamedInterceptor(),
                                      isolate);
  if ((filter & ONLY_ALL_CAN_READ) && !interceptor->all_can_read()) {
    return Just(true);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *object, Object::DONT_THROW);
  v8::Local<v8::Object> result;
  if (!interceptor->enumerator()->IsUndefined()) {
    Callback enum_fun = v8::ToCData<Callback>(interceptor->enumerator());
    const char* log_tag = type == kIndexed ? "interceptor-indexed-enum"
                                           : "interceptor-named-enum";
    LOG(isolate, ApiObjectAccess(log_tag, *object));
    result = args.Call(enum_fun);
  }
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.IsEmpty()) return Just(true);
  DCHECK(v8::Utils::OpenHandle(*result)->IsJSArray() ||
         (v8::Utils::OpenHandle(*result)->IsJSObject() &&
          Handle<JSObject>::cast(v8::Utils::OpenHandle(*result))
              ->HasSloppyArgumentsElements()));
  // The accumulator takes care of string/symbol filtering.
  if (type == kIndexed) {
    accumulator->AddElementKeysFromInterceptor(
        Handle<JSObject>::cast(v8::Utils::OpenHandle(*result)));
  } else {
    accumulator->AddKeys(Handle<JSObject>::cast(v8::Utils::OpenHandle(*result)),
                         DO_NOT_CONVERT);
  }
  return Just(true);
}


// Returns |true| on success, |false| if prototype walking should be stopped,
// |nothing| if an exception was thrown.
static Maybe<bool> GetKeysFromJSObject(Isolate* isolate,
                                       Handle<JSReceiver> receiver,
                                       Handle<JSObject> object,
                                       PropertyFilter* filter,
                                       KeyCollectionType type,
                                       KeyAccumulator* accumulator) {
  accumulator->NextPrototype();
  // Check access rights if required.
  if (object->IsAccessCheckNeeded() &&
      !isolate->MayAccess(handle(isolate->context()), object)) {
    // The cross-origin spec says that [[Enumerate]] shall return an empty
    // iterator when it doesn't have access...
    if (type == INCLUDE_PROTOS) {
      return Just(false);
    }
    // ...whereas [[OwnPropertyKeys]] shall return whitelisted properties.
    DCHECK_EQ(OWN_ONLY, type);
    *filter = static_cast<PropertyFilter>(*filter | ONLY_ALL_CAN_READ);
  }

  JSObject::CollectOwnElementKeys(object, accumulator, *filter);

  // Add the element keys from the interceptor.
  Maybe<bool> success =
      GetKeysFromInterceptor<v8::IndexedPropertyEnumeratorCallback, kIndexed>(
          isolate, receiver, object, *filter, accumulator);
  MAYBE_RETURN(success, Nothing<bool>());

  if (*filter == ENUMERABLE_STRINGS) {
    Handle<FixedArray> enum_keys = JSObject::GetEnumPropertyKeys(object);
    accumulator->AddKeys(enum_keys, DO_NOT_CONVERT);
  } else {
    object->CollectOwnPropertyNames(accumulator, *filter);
  }

  // Add the property keys from the interceptor.
  success = GetKeysFromInterceptor<v8::GenericNamedPropertyEnumeratorCallback,
                                   kNamed>(isolate, receiver, object, *filter,
                                           accumulator);
  MAYBE_RETURN(success, Nothing<bool>());
  return Just(true);
}


// Helper function for JSReceiver::GetKeys() below. Can be called recursively.
// Returns |true| or |nothing|.
static Maybe<bool> GetKeys_Internal(Isolate* isolate,
                                    Handle<JSReceiver> receiver,
                                    Handle<JSReceiver> object,
                                    KeyCollectionType type,
                                    PropertyFilter filter,
                                    KeyAccumulator* accumulator) {
  PrototypeIterator::WhereToEnd end = type == OWN_ONLY
                                          ? PrototypeIterator::END_AT_NON_HIDDEN
                                          : PrototypeIterator::END_AT_NULL;
  for (PrototypeIterator iter(isolate, object,
                              PrototypeIterator::START_AT_RECEIVER, end);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    Maybe<bool> result = Just(false);  // Dummy initialization.
    if (current->IsJSProxy()) {
      result = JSProxy::OwnPropertyKeys(isolate, receiver,
                                        Handle<JSProxy>::cast(current), filter,
                                        accumulator);
    } else {
      DCHECK(current->IsJSObject());
      result = GetKeysFromJSObject(isolate, receiver,
                                   Handle<JSObject>::cast(current), &filter,
                                   type, accumulator);
    }
    MAYBE_RETURN(result, Nothing<bool>());
    if (!result.FromJust()) break;  // |false| means "stop iterating".
  }
  return Just(true);
}


// ES6 9.5.12
// Returns |true| on success, |nothing| in case of exception.
// static
Maybe<bool> JSProxy::OwnPropertyKeys(Isolate* isolate,
                                     Handle<JSReceiver> receiver,
                                     Handle<JSProxy> proxy,
                                     PropertyFilter filter,
                                     KeyAccumulator* accumulator) {
  STACK_CHECK(Nothing<bool>());
  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 2. If handler is null, throw a TypeError exception.
  // 3. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, isolate->factory()->ownKeys_string()));
    return Nothing<bool>();
  }
  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 5. Let trap be ? GetMethod(handler, "ownKeys").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(Handle<JSReceiver>::cast(handler),
                                       isolate->factory()->ownKeys_string()),
      Nothing<bool>());
  // 6. If trap is undefined, then
  if (trap->IsUndefined()) {
    // 6a. Return target.[[OwnPropertyKeys]]().
    return GetKeys_Internal(isolate, receiver, target, OWN_ONLY, filter,
                            accumulator);
  }
  // 7. Let trapResultArray be Call(trap, handler, «target»).
  Handle<Object> trap_result_array;
  Handle<Object> args[] = {target};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result_array,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  // 8. Let trapResult be ? CreateListFromArrayLike(trapResultArray,
  //    «String, Symbol»).
  Handle<FixedArray> trap_result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Object::CreateListFromArrayLike(isolate, trap_result_array,
                                      ElementTypes::kStringAndSymbol),
      Nothing<bool>());
  // 9. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> maybe_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(maybe_extensible, Nothing<bool>());
  bool extensible_target = maybe_extensible.FromJust();
  // 10. Let targetKeys be ? target.[[OwnPropertyKeys]]().
  Handle<FixedArray> target_keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, target_keys,
                                   JSReceiver::OwnPropertyKeys(target),
                                   Nothing<bool>());
  // 11. (Assert)
  // 12. Let targetConfigurableKeys be an empty List.
  // To save memory, we're re-using target_keys and will modify it in-place.
  Handle<FixedArray> target_configurable_keys = target_keys;
  // 13. Let targetNonconfigurableKeys be an empty List.
  Handle<FixedArray> target_nonconfigurable_keys =
      isolate->factory()->NewFixedArray(target_keys->length());
  int nonconfigurable_keys_length = 0;
  // 14. Repeat, for each element key of targetKeys:
  for (int i = 0; i < target_keys->length(); ++i) {
    // 14a. Let desc be ? target.[[GetOwnProperty]](key).
    PropertyDescriptor desc;
    Maybe<bool> found = JSReceiver::GetOwnPropertyDescriptor(
        isolate, target, handle(target_keys->get(i), isolate), &desc);
    MAYBE_RETURN(found, Nothing<bool>());
    // 14b. If desc is not undefined and desc.[[Configurable]] is false, then
    if (found.FromJust() && !desc.configurable()) {
      // 14b i. Append key as an element of targetNonconfigurableKeys.
      target_nonconfigurable_keys->set(nonconfigurable_keys_length,
                                       target_keys->get(i));
      nonconfigurable_keys_length++;
      // The key was moved, null it out in the original list.
      target_keys->set(i, Smi::FromInt(0));
    } else {
      // 14c. Else,
      // 14c i. Append key as an element of targetConfigurableKeys.
      // (No-op, just keep it in |target_keys|.)
    }
  }
  accumulator->NextPrototype();  // Prepare for accumulating keys.
  // 15. If extensibleTarget is true and targetNonconfigurableKeys is empty,
  //     then:
  if (extensible_target && nonconfigurable_keys_length == 0) {
    // 15a. Return trapResult.
    return accumulator->AddKeysFromProxy(proxy, trap_result);
  }
  // 16. Let uncheckedResultKeys be a new List which is a copy of trapResult.
  Zone set_zone;
  const int kPresent = 1;
  const int kGone = 0;
  IdentityMap<int> unchecked_result_keys(isolate->heap(), &set_zone);
  int unchecked_result_keys_size = 0;
  for (int i = 0; i < trap_result->length(); ++i) {
    DCHECK(trap_result->get(i)->IsUniqueName());
    Object* key = trap_result->get(i);
    int* entry = unchecked_result_keys.Get(key);
    if (*entry != kPresent) {
      *entry = kPresent;
      unchecked_result_keys_size++;
    }
  }
  // 17. Repeat, for each key that is an element of targetNonconfigurableKeys:
  for (int i = 0; i < nonconfigurable_keys_length; ++i) {
    Object* key = target_nonconfigurable_keys->get(i);
    // 17a. If key is not an element of uncheckedResultKeys, throw a
    //      TypeError exception.
    int* found = unchecked_result_keys.Find(key);
    if (found == nullptr || *found == kGone) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyOwnKeysMissing, handle(key, isolate)));
      return Nothing<bool>();
    }
    // 17b. Remove key from uncheckedResultKeys.
    *found = kGone;
    unchecked_result_keys_size--;
  }
  // 18. If extensibleTarget is true, return trapResult.
  if (extensible_target) {
    return accumulator->AddKeysFromProxy(proxy, trap_result);
  }
  // 19. Repeat, for each key that is an element of targetConfigurableKeys:
  for (int i = 0; i < target_configurable_keys->length(); ++i) {
    Object* key = target_configurable_keys->get(i);
    if (key->IsSmi()) continue;  // Zapped entry, was nonconfigurable.
    // 19a. If key is not an element of uncheckedResultKeys, throw a
    //      TypeError exception.
    int* found = unchecked_result_keys.Find(key);
    if (found == nullptr || *found == kGone) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyOwnKeysMissing, handle(key, isolate)));
      return Nothing<bool>();
    }
    // 19b. Remove key from uncheckedResultKeys.
    *found = kGone;
    unchecked_result_keys_size--;
  }
  // 20. If uncheckedResultKeys is not empty, throw a TypeError exception.
  if (unchecked_result_keys_size != 0) {
    DCHECK_GT(unchecked_result_keys_size, 0);
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyOwnKeysNonExtensible));
    return Nothing<bool>();
  }
  // 21. Return trapResult.
  return accumulator->AddKeysFromProxy(proxy, trap_result);
}


MaybeHandle<FixedArray> JSReceiver::GetKeys(Handle<JSReceiver> object,
                                            KeyCollectionType type,
                                            PropertyFilter filter,
                                            GetKeysConversion keys_conversion) {
  USE(ContainsOnlyValidKeys);
  Isolate* isolate = object->GetIsolate();
  KeyAccumulator accumulator(isolate, type, filter);
  MAYBE_RETURN(
      GetKeys_Internal(isolate, object, object, type, filter, &accumulator),
      MaybeHandle<FixedArray>());
  Handle<FixedArray> keys = accumulator.GetKeys(keys_conversion);
  DCHECK(ContainsOnlyValidKeys(keys));
  return keys;
}

MaybeHandle<FixedArray> GetOwnValuesOrEntries(Isolate* isolate,
                                              Handle<JSReceiver> object,
                                              PropertyFilter filter,
                                              bool get_entries) {
  PropertyFilter key_filter =
      static_cast<PropertyFilter>(filter & ~ONLY_ENUMERABLE);
  KeyAccumulator accumulator(isolate, OWN_ONLY, key_filter);
  MAYBE_RETURN(GetKeys_Internal(isolate, object, object, OWN_ONLY, key_filter,
                                &accumulator),
               MaybeHandle<FixedArray>());
  Handle<FixedArray> keys = accumulator.GetKeys(CONVERT_TO_STRING);
  DCHECK(ContainsOnlyValidKeys(keys));

  Handle<FixedArray> values_or_entries =
      isolate->factory()->NewFixedArray(keys->length());
  int length = 0;

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Name> key = Handle<Name>::cast(handle(keys->get(i), isolate));

    if (filter & ONLY_ENUMERABLE) {
      PropertyDescriptor descriptor;
      Maybe<bool> did_get_descriptor = JSReceiver::GetOwnPropertyDescriptor(
          isolate, object, key, &descriptor);
      MAYBE_RETURN(did_get_descriptor, MaybeHandle<FixedArray>());
      if (!did_get_descriptor.FromJust() || !descriptor.enumerable()) continue;
    }

    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, JSReceiver::GetPropertyOrElement(object, key),
        MaybeHandle<FixedArray>());

    if (get_entries) {
      Handle<FixedArray> entry_storage =
          isolate->factory()->NewUninitializedFixedArray(2);
      entry_storage->set(0, *key);
      entry_storage->set(1, *value);
      value = isolate->factory()->NewJSArrayWithElements(entry_storage,
                                                         FAST_ELEMENTS, 2);
    }

    values_or_entries->set(length, *value);
    length++;
  }
  if (length < values_or_entries->length()) values_or_entries->Shrink(length);
  return values_or_entries;
}

MaybeHandle<FixedArray> JSReceiver::GetOwnValues(Handle<JSReceiver> object,
                                                 PropertyFilter filter) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter, false);
}

MaybeHandle<FixedArray> JSReceiver::GetOwnEntries(Handle<JSReceiver> object,
                                                  PropertyFilter filter) {
  return GetOwnValuesOrEntries(object->GetIsolate(), object, filter, true);
}

bool Map::DictionaryElementsInPrototypeChainOnly() {
  if (IsDictionaryElementsKind(elements_kind())) {
    return false;
  }

  for (PrototypeIterator iter(this); !iter.IsAtEnd(); iter.Advance()) {
    // Be conservative, don't walk into proxies.
    if (iter.GetCurrent()->IsJSProxy()) return true;
    // String wrappers have non-configurable, non-writable elements.
    if (iter.GetCurrent()->IsStringWrapper()) return true;
    JSObject* current = iter.GetCurrent<JSObject>();

    if (current->HasDictionaryElements() &&
        current->element_dictionary()->requires_slow_elements()) {
      return true;
    }

    if (current->HasSlowArgumentsElements()) {
      FixedArray* parameter_map = FixedArray::cast(current->elements());
      Object* arguments = parameter_map->get(1);
      if (SeededNumberDictionary::cast(arguments)->requires_slow_elements()) {
        return true;
      }
    }
  }

  return false;
}


MaybeHandle<Object> JSObject::DefineAccessor(Handle<JSObject> object,
                                             Handle<Name> name,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
  return DefineAccessor(&it, getter, setter, attributes);
}


MaybeHandle<Object> JSObject::DefineAccessor(LookupIterator* it,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = it->isolate();

  if (it->state() == LookupIterator::ACCESS_CHECK) {
    if (!it->HasAccess()) {
      isolate->ReportFailedAccessCheck(it->GetHolder<JSObject>());
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }
    it->Next();
  }

  Handle<JSObject> object = Handle<JSObject>::cast(it->GetReceiver());
  // Ignore accessors on typed arrays.
  if (it->IsElement() && object->HasFixedTypedArrayElements()) {
    return it->factory()->undefined_value();
  }

  Handle<Object> old_value = isolate->factory()->the_hole_value();
  bool is_observed = object->map()->is_observed() &&
                     (it->IsElement() || !it->name()->IsPrivate());
  bool preexists = false;
  if (is_observed) {
    CHECK(GetPropertyAttributes(it).IsJust());
    preexists = it->IsFound();
    if (preexists && (it->state() == LookupIterator::DATA ||
                      it->GetAccessors()->IsAccessorInfo())) {
      old_value = GetProperty(it).ToHandleChecked();
    }
  }

  DCHECK(getter->IsCallable() || getter->IsUndefined() || getter->IsNull() ||
         getter->IsFunctionTemplateInfo());
  DCHECK(setter->IsCallable() || setter->IsUndefined() || setter->IsNull() ||
         getter->IsFunctionTemplateInfo());
  // At least one of the accessors needs to be a new value.
  DCHECK(!getter->IsNull() || !setter->IsNull());
  if (!getter->IsNull()) {
    it->TransitionToAccessorProperty(ACCESSOR_GETTER, getter, attributes);
  }
  if (!setter->IsNull()) {
    it->TransitionToAccessorProperty(ACCESSOR_SETTER, setter, attributes);
  }

  if (is_observed) {
    // Make sure the top context isn't changed.
    AssertNoContextChange ncc(isolate);
    const char* type = preexists ? "reconfigure" : "add";
    RETURN_ON_EXCEPTION(
        isolate, EnqueueChangeRecord(object, type, it->GetName(), old_value),
        Object);
  }

  return isolate->factory()->undefined_value();
}


MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<AccessorInfo> info) {
  Isolate* isolate = object->GetIsolate();
  Handle<Name> name(Name::cast(info->name()), isolate);

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);

  // Duplicate ACCESS_CHECK outside of GetPropertyAttributes for the case that
  // the FailedAccessCheckCallbackFunction doesn't throw an exception.
  //
  // TODO(verwaest): Force throw an exception if the callback doesn't, so we can
  // remove reliance on default return values.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    if (!it.HasAccess()) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return it.factory()->undefined_value();
    }
    it.Next();
  }

  // Ignore accessors on typed arrays.
  if (it.IsElement() && object->HasFixedTypedArrayElements()) {
    return it.factory()->undefined_value();
  }

  CHECK(GetPropertyAttributes(&it).IsJust());

  // ES5 forbids turning a property into an accessor if it's not
  // configurable. See 8.6.1 (Table 5).
  if (it.IsFound() && !it.IsConfigurable()) {
    return it.factory()->undefined_value();
  }

  it.TransitionToAccessorPair(info, info->property_attributes());

  return object;
}


MaybeHandle<Object> JSObject::GetAccessor(Handle<JSObject> object,
                                          Handle<Name> name,
                                          AccessorComponent component) {
  Isolate* isolate = object->GetIsolate();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);

  for (; it.IsFound(); it.Next()) {
    switch (it.state()) {
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();

      case LookupIterator::ACCESS_CHECK:
        if (it.HasAccess()) continue;
        isolate->ReportFailedAccessCheck(it.GetHolder<JSObject>());
        RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
        return isolate->factory()->undefined_value();

      case LookupIterator::JSPROXY:
        return isolate->factory()->undefined_value();

      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return isolate->factory()->undefined_value();
      case LookupIterator::DATA:
        continue;
      case LookupIterator::ACCESSOR: {
        Handle<Object> maybe_pair = it.GetAccessors();
        if (maybe_pair->IsAccessorPair()) {
          return AccessorPair::GetComponent(
              Handle<AccessorPair>::cast(maybe_pair), component);
        }
      }
    }
  }

  return isolate->factory()->undefined_value();
}


Object* JSObject::SlowReverseLookup(Object* value) {
  if (HasFastProperties()) {
    int number_of_own_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    bool value_is_number = value->IsNumber();
    for (int i = 0; i < number_of_own_descriptors; i++) {
      if (descs->GetType(i) == DATA) {
        FieldIndex field_index = FieldIndex::ForDescriptor(map(), i);
        if (IsUnboxedDoubleField(field_index)) {
          if (value_is_number) {
            double property = RawFastDoublePropertyAt(field_index);
            if (property == value->Number()) {
              return descs->GetKey(i);
            }
          }
        } else {
          Object* property = RawFastPropertyAt(field_index);
          if (field_index.is_double()) {
            DCHECK(property->IsMutableHeapNumber());
            if (value_is_number && property->Number() == value->Number()) {
              return descs->GetKey(i);
            }
          } else if (property == value) {
            return descs->GetKey(i);
          }
        }
      } else if (descs->GetType(i) == DATA_CONSTANT) {
        if (descs->GetConstant(i) == value) {
          return descs->GetKey(i);
        }
      }
    }
    return GetHeap()->undefined_value();
  } else if (IsJSGlobalObject()) {
    return global_dictionary()->SlowReverseLookup(value);
  } else {
    return property_dictionary()->SlowReverseLookup(value);
  }
}


Handle<Map> Map::RawCopy(Handle<Map> map, int instance_size) {
  Isolate* isolate = map->GetIsolate();
  Handle<Map> result =
      isolate->factory()->NewMap(map->instance_type(), instance_size);
  Handle<Object> prototype(map->prototype(), isolate);
  Map::SetPrototype(result, prototype);
  result->set_constructor_or_backpointer(map->GetConstructor());
  result->set_bit_field(map->bit_field());
  result->set_bit_field2(map->bit_field2());
  int new_bit_field3 = map->bit_field3();
  new_bit_field3 = OwnsDescriptors::update(new_bit_field3, true);
  new_bit_field3 = NumberOfOwnDescriptorsBits::update(new_bit_field3, 0);
  new_bit_field3 = EnumLengthBits::update(new_bit_field3,
                                          kInvalidEnumCacheSentinel);
  new_bit_field3 = Deprecated::update(new_bit_field3, false);
  if (!map->is_dictionary_map()) {
    new_bit_field3 = IsUnstable::update(new_bit_field3, false);
  }
  result->set_bit_field3(new_bit_field3);
  return result;
}


Handle<Map> Map::Normalize(Handle<Map> fast_map, PropertyNormalizationMode mode,
                           const char* reason) {
  DCHECK(!fast_map->is_dictionary_map());

  Isolate* isolate = fast_map->GetIsolate();
  Handle<Object> maybe_cache(isolate->native_context()->normalized_map_cache(),
                             isolate);
  bool use_cache = !fast_map->is_prototype_map() && !maybe_cache->IsUndefined();
  Handle<NormalizedMapCache> cache;
  if (use_cache) cache = Handle<NormalizedMapCache>::cast(maybe_cache);

  Handle<Map> new_map;
  if (use_cache && cache->Get(fast_map, mode).ToHandle(&new_map)) {
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) new_map->DictionaryMapVerify();
#endif
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit,
      // except for the code cache, which can contain some ics which can be
      // applied to the shared map, dependent code and weak cell cache.
      Handle<Map> fresh = Map::CopyNormalized(fast_map, mode);

      if (new_map->is_prototype_map()) {
        // For prototype maps, the PrototypeInfo is not copied.
        DCHECK(memcmp(fresh->address(), new_map->address(),
                      kTransitionsOrPrototypeInfoOffset) == 0);
        DCHECK(fresh->raw_transitions() == Smi::FromInt(0));
        STATIC_ASSERT(kDescriptorsOffset ==
                      kTransitionsOrPrototypeInfoOffset + kPointerSize);
        DCHECK(memcmp(HeapObject::RawField(*fresh, kDescriptorsOffset),
                      HeapObject::RawField(*new_map, kDescriptorsOffset),
                      kCodeCacheOffset - kDescriptorsOffset) == 0);
      } else {
        DCHECK(memcmp(fresh->address(), new_map->address(),
                      Map::kCodeCacheOffset) == 0);
      }
      STATIC_ASSERT(Map::kDependentCodeOffset ==
                    Map::kCodeCacheOffset + kPointerSize);
      STATIC_ASSERT(Map::kWeakCellCacheOffset ==
                    Map::kDependentCodeOffset + kPointerSize);
      int offset = Map::kWeakCellCacheOffset + kPointerSize;
      DCHECK(memcmp(fresh->address() + offset,
                    new_map->address() + offset,
                    Map::kSize - offset) == 0);
    }
#endif
  } else {
    new_map = Map::CopyNormalized(fast_map, mode);
    if (use_cache) {
      cache->Set(fast_map, new_map);
      isolate->counters()->maps_normalized()->Increment();
    }
#if TRACE_MAPS
    if (FLAG_trace_maps) {
      PrintF("[TraceMaps: Normalize from= %p to= %p reason= %s ]\n",
             reinterpret_cast<void*>(*fast_map),
             reinterpret_cast<void*>(*new_map), reason);
    }
#endif
  }
  fast_map->NotifyLeafMapLayoutChange();
  return new_map;
}


Handle<Map> Map::CopyNormalized(Handle<Map> map,
                                PropertyNormalizationMode mode) {
  int new_instance_size = map->instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= map->GetInObjectProperties() * kPointerSize;
  }

  Handle<Map> result = RawCopy(map, new_instance_size);

  if (mode != CLEAR_INOBJECT_PROPERTIES) {
    result->SetInObjectProperties(map->GetInObjectProperties());
  }

  result->set_dictionary_map(true);
  result->set_migration_target(false);
  result->set_construction_counter(kNoSlackTracking);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) result->DictionaryMapVerify();
#endif

  return result;
}


Handle<Map> Map::CopyInitialMap(Handle<Map> map, int instance_size,
                                int in_object_properties,
                                int unused_property_fields) {
#ifdef DEBUG
  Isolate* isolate = map->GetIsolate();
  // Strict and strong function maps have Function as a constructor but the
  // Function's initial map is a sloppy function map. Same holds for
  // GeneratorFunction and its initial map.
  Object* constructor = map->GetConstructor();
  DCHECK(constructor->IsJSFunction());
  DCHECK(*map == JSFunction::cast(constructor)->initial_map() ||
         *map == *isolate->strict_function_map() ||
         *map == *isolate->strong_function_map() ||
         *map == *isolate->strict_generator_function_map() ||
         *map == *isolate->strong_generator_function_map());
#endif
  // Initial maps must always own their descriptors and it's descriptor array
  // does not contain descriptors that do not belong to the map.
  DCHECK(map->owns_descriptors());
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors()->number_of_descriptors());

  Handle<Map> result = RawCopy(map, instance_size);

  // Please note instance_type and instance_size are set when allocated.
  result->SetInObjectProperties(in_object_properties);
  result->set_unused_property_fields(unused_property_fields);

  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors > 0) {
    // The copy will use the same descriptors array.
    result->UpdateDescriptors(map->instance_descriptors(),
                              map->GetLayoutDescriptor());
    result->SetNumberOfOwnDescriptors(number_of_own_descriptors);

    DCHECK_EQ(result->NumberOfFields(),
              in_object_properties - unused_property_fields);
  }

  return result;
}


Handle<Map> Map::CopyDropDescriptors(Handle<Map> map) {
  Handle<Map> result = RawCopy(map, map->instance_size());

  // Please note instance_type and instance_size are set when allocated.
  if (map->IsJSObjectMap()) {
    result->SetInObjectProperties(map->GetInObjectProperties());
    result->set_unused_property_fields(map->unused_property_fields());
  }
  result->ClearCodeCache(map->GetHeap());
  map->NotifyLeafMapLayoutChange();
  return result;
}


Handle<Map> Map::ShareDescriptor(Handle<Map> map,
                                 Handle<DescriptorArray> descriptors,
                                 Descriptor* descriptor) {
  // Sanity check. This path is only to be taken if the map owns its descriptor
  // array, implying that its NumberOfOwnDescriptors equals the number of
  // descriptors in the descriptor array.
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors()->number_of_descriptors());

  Handle<Map> result = CopyDropDescriptors(map);
  Handle<Name> name = descriptor->GetKey();

  // Ensure there's space for the new descriptor in the shared descriptor array.
  if (descriptors->NumberOfSlackDescriptors() == 0) {
    int old_size = descriptors->number_of_descriptors();
    if (old_size == 0) {
      descriptors = DescriptorArray::Allocate(map->GetIsolate(), 0, 1);
    } else {
      int slack = SlackForArraySize(old_size, kMaxNumberOfDescriptors);
      EnsureDescriptorSlack(map, slack);
      descriptors = handle(map->instance_descriptors());
    }
  }

  Handle<LayoutDescriptor> layout_descriptor =
      FLAG_unbox_double_fields
          ? LayoutDescriptor::ShareAppend(map, descriptor->GetDetails())
          : handle(LayoutDescriptor::FastPointerLayout(), map->GetIsolate());

  {
    DisallowHeapAllocation no_gc;
    descriptors->Append(descriptor);
    result->InitializeDescriptors(*descriptors, *layout_descriptor);
  }

  DCHECK(result->NumberOfOwnDescriptors() == map->NumberOfOwnDescriptors() + 1);
  ConnectTransition(map, result, name, SIMPLE_PROPERTY_TRANSITION);

  return result;
}


#if TRACE_MAPS

// static
void Map::TraceTransition(const char* what, Map* from, Map* to, Name* name) {
  if (FLAG_trace_maps) {
    PrintF("[TraceMaps: %s from= %p to= %p name= ", what,
           reinterpret_cast<void*>(from), reinterpret_cast<void*>(to));
    name->NameShortPrint();
    PrintF(" ]\n");
  }
}


// static
void Map::TraceAllTransitions(Map* map) {
  Object* transitions = map->raw_transitions();
  int num_transitions = TransitionArray::NumberOfTransitions(transitions);
  for (int i = -0; i < num_transitions; ++i) {
    Map* target = TransitionArray::GetTarget(transitions, i);
    Name* key = TransitionArray::GetKey(transitions, i);
    Map::TraceTransition("Transition", map, target, key);
    Map::TraceAllTransitions(target);
  }
}

#endif  // TRACE_MAPS


void Map::ConnectTransition(Handle<Map> parent, Handle<Map> child,
                            Handle<Name> name, SimpleTransitionFlag flag) {
  if (!parent->GetBackPointer()->IsUndefined()) {
    parent->set_owns_descriptors(false);
  } else {
    // |parent| is initial map and it must keep the ownership, there must be no
    // descriptors in the descriptors array that do not belong to the map.
    DCHECK(parent->owns_descriptors());
    DCHECK_EQ(parent->NumberOfOwnDescriptors(),
              parent->instance_descriptors()->number_of_descriptors());
  }
  if (parent->is_prototype_map()) {
    DCHECK(child->is_prototype_map());
#if TRACE_MAPS
    Map::TraceTransition("NoTransition", *parent, *child, *name);
#endif
  } else {
    TransitionArray::Insert(parent, name, child, flag);
#if TRACE_MAPS
    Map::TraceTransition("Transition", *parent, *child, *name);
#endif
  }
}


Handle<Map> Map::CopyReplaceDescriptors(
    Handle<Map> map, Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> layout_descriptor, TransitionFlag flag,
    MaybeHandle<Name> maybe_name, const char* reason,
    SimpleTransitionFlag simple_flag) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  Handle<Map> result = CopyDropDescriptors(map);

  if (!map->is_prototype_map()) {
    if (flag == INSERT_TRANSITION &&
        TransitionArray::CanHaveMoreTransitions(map)) {
      result->InitializeDescriptors(*descriptors, *layout_descriptor);

      Handle<Name> name;
      CHECK(maybe_name.ToHandle(&name));
      ConnectTransition(map, result, name, simple_flag);
    } else {
      int length = descriptors->number_of_descriptors();
      for (int i = 0; i < length; i++) {
        descriptors->SetRepresentation(i, Representation::Tagged());
        if (descriptors->GetDetails(i).type() == DATA) {
          descriptors->SetValue(i, FieldType::Any());
        }
      }
      result->InitializeDescriptors(*descriptors,
                                    LayoutDescriptor::FastPointerLayout());
    }
  } else {
    result->InitializeDescriptors(*descriptors, *layout_descriptor);
  }
#if TRACE_MAPS
  if (FLAG_trace_maps &&
      // Mirror conditions above that did not call ConnectTransition().
      (map->is_prototype_map() ||
       !(flag == INSERT_TRANSITION &&
         TransitionArray::CanHaveMoreTransitions(map)))) {
    PrintF("[TraceMaps: ReplaceDescriptors from= %p to= %p reason= %s ]\n",
           reinterpret_cast<void*>(*map), reinterpret_cast<void*>(*result),
           reason);
  }
#endif

  return result;
}


// Creates transition tree starting from |split_map| and adding all descriptors
// starting from descriptor with index |split_map|.NumberOfOwnDescriptors().
// The way how it is done is tricky because of GC and special descriptors
// marking logic.
Handle<Map> Map::AddMissingTransitions(
    Handle<Map> split_map, Handle<DescriptorArray> descriptors,
    Handle<LayoutDescriptor> full_layout_descriptor) {
  DCHECK(descriptors->IsSortedNoDuplicates());
  int split_nof = split_map->NumberOfOwnDescriptors();
  int nof_descriptors = descriptors->number_of_descriptors();
  DCHECK_LT(split_nof, nof_descriptors);

  // Start with creating last map which will own full descriptors array.
  // This is necessary to guarantee that GC will mark the whole descriptor
  // array if any of the allocations happening below fail.
  // Number of unused properties is temporarily incorrect and the layout
  // descriptor could unnecessarily be in slow mode but we will fix after
  // all the other intermediate maps are created.
  Handle<Map> last_map = CopyDropDescriptors(split_map);
  last_map->InitializeDescriptors(*descriptors, *full_layout_descriptor);
  last_map->set_unused_property_fields(0);

  // During creation of intermediate maps we violate descriptors sharing
  // invariant since the last map is not yet connected to the transition tree
  // we create here. But it is safe because GC never trims map's descriptors
  // if there are no dead transitions from that map and this is exactly the
  // case for all the intermediate maps we create here.
  Handle<Map> map = split_map;
  for (int i = split_nof; i < nof_descriptors - 1; ++i) {
    Handle<Map> new_map = CopyDropDescriptors(map);
    InstallDescriptors(map, new_map, i, descriptors, full_layout_descriptor);
    map = new_map;
  }
  map->NotifyLeafMapLayoutChange();
  InstallDescriptors(map, last_map, nof_descriptors - 1, descriptors,
                     full_layout_descriptor);
  return last_map;
}


// Since this method is used to rewrite an existing transition tree, it can
// always insert transitions without checking.
void Map::InstallDescriptors(Handle<Map> parent, Handle<Map> child,
                             int new_descriptor,
                             Handle<DescriptorArray> descriptors,
                             Handle<LayoutDescriptor> full_layout_descriptor) {
  DCHECK(descriptors->IsSortedNoDuplicates());

  child->set_instance_descriptors(*descriptors);
  child->SetNumberOfOwnDescriptors(new_descriptor + 1);

  int unused_property_fields = parent->unused_property_fields();
  PropertyDetails details = descriptors->GetDetails(new_descriptor);
  if (details.location() == kField) {
    unused_property_fields = parent->unused_property_fields() - 1;
    if (unused_property_fields < 0) {
      unused_property_fields += JSObject::kFieldsAdded;
    }
  }
  child->set_unused_property_fields(unused_property_fields);

  if (FLAG_unbox_double_fields) {
    Handle<LayoutDescriptor> layout_descriptor =
        LayoutDescriptor::AppendIfFastOrUseFull(parent, details,
                                                full_layout_descriptor);
    child->set_layout_descriptor(*layout_descriptor);
#ifdef VERIFY_HEAP
    // TODO(ishell): remove these checks from VERIFY_HEAP mode.
    if (FLAG_verify_heap) {
      CHECK(child->layout_descriptor()->IsConsistentWithMap(*child));
    }
#else
    SLOW_DCHECK(child->layout_descriptor()->IsConsistentWithMap(*child));
#endif
    child->set_visitor_id(Heap::GetStaticVisitorIdForMap(*child));
  }

  Handle<Name> name = handle(descriptors->GetKey(new_descriptor));
  ConnectTransition(parent, child, name, SIMPLE_PROPERTY_TRANSITION);
}


Handle<Map> Map::CopyAsElementsKind(Handle<Map> map, ElementsKind kind,
                                    TransitionFlag flag) {
  Map* maybe_elements_transition_map = NULL;
  if (flag == INSERT_TRANSITION) {
    // Ensure we are requested to add elements kind transition "near the root".
    DCHECK_EQ(map->FindRootMap()->NumberOfOwnDescriptors(),
              map->NumberOfOwnDescriptors());

    maybe_elements_transition_map = map->ElementsTransitionMap();
    DCHECK(maybe_elements_transition_map == NULL ||
           (maybe_elements_transition_map->elements_kind() ==
                DICTIONARY_ELEMENTS &&
            kind == DICTIONARY_ELEMENTS));
    DCHECK(!IsFastElementsKind(kind) ||
           IsMoreGeneralElementsKindTransition(map->elements_kind(), kind));
    DCHECK(kind != map->elements_kind());
  }

  bool insert_transition = flag == INSERT_TRANSITION &&
                           TransitionArray::CanHaveMoreTransitions(map) &&
                           maybe_elements_transition_map == NULL;

  if (insert_transition) {
    Handle<Map> new_map = CopyForTransition(map, "CopyAsElementsKind");
    new_map->set_elements_kind(kind);

    Isolate* isolate = map->GetIsolate();
    Handle<Name> name = isolate->factory()->elements_transition_symbol();
    ConnectTransition(map, new_map, name, SPECIAL_TRANSITION);
    return new_map;
  }

  // Create a new free-floating map only if we are not allowed to store it.
  Handle<Map> new_map = Copy(map, "CopyAsElementsKind");
  new_map->set_elements_kind(kind);
  return new_map;
}


Handle<Map> Map::AsLanguageMode(Handle<Map> initial_map,
                                LanguageMode language_mode, FunctionKind kind) {
  DCHECK_EQ(JS_FUNCTION_TYPE, initial_map->instance_type());
  // Initial map for sloppy mode function is stored in the function
  // constructor. Initial maps for strict and strong modes are cached as
  // special transitions using |strict_function_transition_symbol| and
  // |strong_function_transition_symbol| respectively as a key.
  if (language_mode == SLOPPY) return initial_map;
  Isolate* isolate = initial_map->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<Symbol> transition_symbol;

  int map_index = Context::FunctionMapIndex(language_mode, kind);
  Handle<Map> function_map(
      Map::cast(isolate->native_context()->get(map_index)));

  STATIC_ASSERT(LANGUAGE_END == 3);
  switch (language_mode) {
    case STRICT:
      transition_symbol = factory->strict_function_transition_symbol();
      break;
    case STRONG:
      transition_symbol = factory->strong_function_transition_symbol();
      break;
    default:
      UNREACHABLE();
      break;
  }
  Map* maybe_transition =
      TransitionArray::SearchSpecial(*initial_map, *transition_symbol);
  if (maybe_transition != NULL) {
    return handle(maybe_transition, isolate);
  }
  initial_map->NotifyLeafMapLayoutChange();

  // Create new map taking descriptors from the |function_map| and all
  // the other details from the |initial_map|.
  Handle<Map> map =
      Map::CopyInitialMap(function_map, initial_map->instance_size(),
                          initial_map->GetInObjectProperties(),
                          initial_map->unused_property_fields());
  map->SetConstructor(initial_map->GetConstructor());
  map->set_prototype(initial_map->prototype());

  if (TransitionArray::CanHaveMoreTransitions(initial_map)) {
    Map::ConnectTransition(initial_map, map, transition_symbol,
                           SPECIAL_TRANSITION);
  }
  return map;
}


Handle<Map> Map::CopyForObserved(Handle<Map> map) {
  DCHECK(!map->is_observed());

  Isolate* isolate = map->GetIsolate();

  bool insert_transition =
      TransitionArray::CanHaveMoreTransitions(map) && !map->is_prototype_map();

  if (insert_transition) {
    Handle<Map> new_map = CopyForTransition(map, "CopyForObserved");
    new_map->set_is_observed();

    Handle<Name> name = isolate->factory()->observed_symbol();
    ConnectTransition(map, new_map, name, SPECIAL_TRANSITION);
    return new_map;
  }

  // Create a new free-floating map only if we are not allowed to store it.
  Handle<Map> new_map = Map::Copy(map, "CopyForObserved");
  new_map->set_is_observed();
  return new_map;
}


Handle<Map> Map::CopyForTransition(Handle<Map> map, const char* reason) {
  DCHECK(!map->is_prototype_map());
  Handle<Map> new_map = CopyDropDescriptors(map);

  if (map->owns_descriptors()) {
    // In case the map owned its own descriptors, share the descriptors and
    // transfer ownership to the new map.
    // The properties did not change, so reuse descriptors.
    new_map->InitializeDescriptors(map->instance_descriptors(),
                                   map->GetLayoutDescriptor());
  } else {
    // In case the map did not own its own descriptors, a split is forced by
    // copying the map; creating a new descriptor array cell.
    Handle<DescriptorArray> descriptors(map->instance_descriptors());
    int number_of_own_descriptors = map->NumberOfOwnDescriptors();
    Handle<DescriptorArray> new_descriptors =
        DescriptorArray::CopyUpTo(descriptors, number_of_own_descriptors);
    Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                   map->GetIsolate());
    new_map->InitializeDescriptors(*new_descriptors, *new_layout_descriptor);
  }

#if TRACE_MAPS
  if (FLAG_trace_maps) {
    PrintF("[TraceMaps: CopyForTransition from= %p to= %p reason= %s ]\n",
           reinterpret_cast<void*>(*map), reinterpret_cast<void*>(*new_map),
           reason);
  }
#endif

  return new_map;
}


Handle<Map> Map::Copy(Handle<Map> map, const char* reason) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(descriptors, number_of_own_descriptors);
  Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                 map->GetIsolate());
  return CopyReplaceDescriptors(map, new_descriptors, new_layout_descriptor,
                                OMIT_TRANSITION, MaybeHandle<Name>(), reason,
                                SPECIAL_TRANSITION);
}


Handle<Map> Map::Create(Isolate* isolate, int inobject_properties) {
  Handle<Map> copy =
      Copy(handle(isolate->object_function()->initial_map()), "MapCreate");

  // Check that we do not overflow the instance size when adding the extra
  // inobject properties. If the instance size overflows, we allocate as many
  // properties as we can as inobject properties.
  int max_extra_properties =
      (JSObject::kMaxInstanceSize - JSObject::kHeaderSize) >> kPointerSizeLog2;

  if (inobject_properties > max_extra_properties) {
    inobject_properties = max_extra_properties;
  }

  int new_instance_size =
      JSObject::kHeaderSize + kPointerSize * inobject_properties;

  // Adjust the map with the extra inobject properties.
  copy->SetInObjectProperties(inobject_properties);
  copy->set_unused_property_fields(inobject_properties);
  copy->set_instance_size(new_instance_size);
  copy->set_visitor_id(Heap::GetStaticVisitorIdForMap(*copy));
  return copy;
}


Handle<Map> Map::CopyForPreventExtensions(Handle<Map> map,
                                          PropertyAttributes attrs_to_add,
                                          Handle<Symbol> transition_marker,
                                          const char* reason) {
  int num_descriptors = map->NumberOfOwnDescriptors();
  Isolate* isolate = map->GetIsolate();
  Handle<DescriptorArray> new_desc = DescriptorArray::CopyUpToAddAttributes(
      handle(map->instance_descriptors(), isolate), num_descriptors,
      attrs_to_add);
  Handle<LayoutDescriptor> new_layout_descriptor(map->GetLayoutDescriptor(),
                                                 isolate);
  Handle<Map> new_map = CopyReplaceDescriptors(
      map, new_desc, new_layout_descriptor, INSERT_TRANSITION,
      transition_marker, reason, SPECIAL_TRANSITION);
  new_map->set_is_extensible(false);
  if (!IsFixedTypedArrayElementsKind(map->elements_kind())) {
    ElementsKind new_kind = IsStringWrapperElementsKind(map->elements_kind())
                                ? SLOW_STRING_WRAPPER_ELEMENTS
                                : DICTIONARY_ELEMENTS;
    new_map->set_elements_kind(new_kind);
  }
  return new_map;
}

FieldType* DescriptorArray::GetFieldType(int descriptor_number) {
  DCHECK(GetDetails(descriptor_number).location() == kField);
  Object* value = GetValue(descriptor_number);
  if (value->IsWeakCell()) {
    if (WeakCell::cast(value)->cleared()) return FieldType::None();
    value = WeakCell::cast(value)->value();
  }
  return FieldType::cast(value);
}

namespace {

bool CanHoldValue(DescriptorArray* descriptors, int descriptor, Object* value) {
  PropertyDetails details = descriptors->GetDetails(descriptor);
  switch (details.type()) {
    case DATA:
      return value->FitsRepresentation(details.representation()) &&
             descriptors->GetFieldType(descriptor)->NowContains(value);

    case DATA_CONSTANT:
      DCHECK(descriptors->GetConstant(descriptor) != value ||
             value->FitsRepresentation(details.representation()));
      return descriptors->GetConstant(descriptor) == value;

    case ACCESSOR:
    case ACCESSOR_CONSTANT:
      return false;
  }

  UNREACHABLE();
  return false;
}

Handle<Map> UpdateDescriptorForValue(Handle<Map> map, int descriptor,
                                     Handle<Object> value) {
  if (CanHoldValue(map->instance_descriptors(), descriptor, *value)) return map;

  Isolate* isolate = map->GetIsolate();
  PropertyAttributes attributes =
      map->instance_descriptors()->GetDetails(descriptor).attributes();
  Representation representation = value->OptimalRepresentation();
  Handle<FieldType> type = value->OptimalType(isolate, representation);

  return Map::ReconfigureProperty(map, descriptor, kData, attributes,
                                  representation, type, FORCE_FIELD);
}

}  // namespace

// static
Handle<Map> Map::PrepareForDataProperty(Handle<Map> map, int descriptor,
                                        Handle<Object> value) {
  // Dictionaries can store any property value.
  DCHECK(!map->is_dictionary_map());
  // Update to the newest map before storing the property.
  return UpdateDescriptorForValue(Update(map), descriptor, value);
}


Handle<Map> Map::TransitionToDataProperty(Handle<Map> map, Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          StoreFromKeyed store_mode) {
  DCHECK(name->IsUniqueName());
  DCHECK(!map->is_dictionary_map());

  // Migrate to the newest map before storing the property.
  map = Update(map);

  Map* maybe_transition =
      TransitionArray::SearchTransition(*map, kData, *name, attributes);
  if (maybe_transition != NULL) {
    Handle<Map> transition(maybe_transition);
    int descriptor = transition->LastAdded();

    DCHECK_EQ(attributes, transition->instance_descriptors()
                              ->GetDetails(descriptor)
                              .attributes());

    return UpdateDescriptorForValue(transition, descriptor, value);
  }

  TransitionFlag flag = INSERT_TRANSITION;
  MaybeHandle<Map> maybe_map;
  if (value->IsJSFunction()) {
    maybe_map = Map::CopyWithConstant(map, name, value, attributes, flag);
  } else if (!map->TooManyFastProperties(store_mode)) {
    Isolate* isolate = name->GetIsolate();
    Representation representation = value->OptimalRepresentation();
    Handle<FieldType> type = value->OptimalType(isolate, representation);
    maybe_map =
        Map::CopyWithField(map, name, type, attributes, representation, flag);
  }

  Handle<Map> result;
  if (!maybe_map.ToHandle(&result)) {
#if TRACE_MAPS
    if (FLAG_trace_maps) {
      Vector<char> name_buffer = Vector<char>::New(100);
      name->NameShortPrint(name_buffer);
      Vector<char> buffer = Vector<char>::New(128);
      SNPrintF(buffer, "TooManyFastProperties %s", name_buffer.start());
      return Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES, buffer.start());
    }
#endif
    return Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES,
                          "TooManyFastProperties");
  }

  return result;
}


Handle<Map> Map::ReconfigureExistingProperty(Handle<Map> map, int descriptor,
                                             PropertyKind kind,
                                             PropertyAttributes attributes) {
  // Dictionaries have to be reconfigured in-place.
  DCHECK(!map->is_dictionary_map());

  if (!map->GetBackPointer()->IsMap()) {
    // There is no benefit from reconstructing transition tree for maps without
    // back pointers.
    return CopyGeneralizeAllRepresentations(
        map, map->elements_kind(), descriptor, FORCE_FIELD, kind, attributes,
        "GenAll_AttributesMismatchProtoMap");
  }

  if (FLAG_trace_generalization) {
    map->PrintReconfiguration(stdout, descriptor, kind, attributes);
  }

  Isolate* isolate = map->GetIsolate();
  Handle<Map> new_map = ReconfigureProperty(
      map, descriptor, kind, attributes, Representation::None(),
      FieldType::None(isolate), FORCE_FIELD);
  return new_map;
}


Handle<Map> Map::TransitionToAccessorProperty(Handle<Map> map,
                                              Handle<Name> name,
                                              AccessorComponent component,
                                              Handle<Object> accessor,
                                              PropertyAttributes attributes) {
  DCHECK(name->IsUniqueName());
  Isolate* isolate = name->GetIsolate();

  // Dictionary maps can always have additional data properties.
  if (map->is_dictionary_map()) return map;

  // Migrate to the newest map before transitioning to the new property.
  map = Update(map);

  PropertyNormalizationMode mode = map->is_prototype_map()
                                       ? KEEP_INOBJECT_PROPERTIES
                                       : CLEAR_INOBJECT_PROPERTIES;

  Map* maybe_transition =
      TransitionArray::SearchTransition(*map, kAccessor, *name, attributes);
  if (maybe_transition != NULL) {
    Handle<Map> transition(maybe_transition, isolate);
    DescriptorArray* descriptors = transition->instance_descriptors();
    int descriptor = transition->LastAdded();
    DCHECK(descriptors->GetKey(descriptor)->Equals(*name));

    DCHECK_EQ(kAccessor, descriptors->GetDetails(descriptor).kind());
    DCHECK_EQ(attributes, descriptors->GetDetails(descriptor).attributes());

    Handle<Object> maybe_pair(descriptors->GetValue(descriptor), isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(map, mode, "TransitionToAccessorFromNonPair");
    }

    Handle<AccessorPair> pair = Handle<AccessorPair>::cast(maybe_pair);
    if (pair->get(component) != *accessor) {
      return Map::Normalize(map, mode, "TransitionToDifferentAccessor");
    }

    return transition;
  }

  Handle<AccessorPair> pair;
  DescriptorArray* old_descriptors = map->instance_descriptors();
  int descriptor = old_descriptors->SearchWithCache(isolate, *name, *map);
  if (descriptor != DescriptorArray::kNotFound) {
    if (descriptor != map->LastAdded()) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonLast");
    }
    PropertyDetails old_details = old_descriptors->GetDetails(descriptor);
    if (old_details.type() != ACCESSOR_CONSTANT) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonAccessors");
    }

    if (old_details.attributes() != attributes) {
      return Map::Normalize(map, mode, "AccessorsWithAttributes");
    }

    Handle<Object> maybe_pair(old_descriptors->GetValue(descriptor), isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonPair");
    }

    Object* current = Handle<AccessorPair>::cast(maybe_pair)->get(component);
    if (current == *accessor) return map;

    if (!current->IsTheHole()) {
      return Map::Normalize(map, mode, "AccessorsOverwritingAccessors");
    }

    pair = AccessorPair::Copy(Handle<AccessorPair>::cast(maybe_pair));
  } else if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors ||
             map->TooManyFastProperties(CERTAINLY_NOT_STORE_FROM_KEYED)) {
    return Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES, "TooManyAccessors");
  } else {
    pair = isolate->factory()->NewAccessorPair();
  }

  pair->set(component, *accessor);
  TransitionFlag flag = INSERT_TRANSITION;
  AccessorConstantDescriptor new_desc(name, pair, attributes);
  return Map::CopyInsertDescriptor(map, &new_desc, flag);
}


Handle<Map> Map::CopyAddDescriptor(Handle<Map> map,
                                   Descriptor* descriptor,
                                   TransitionFlag flag) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());

  // Share descriptors only if map owns descriptors and it not an initial map.
  if (flag == INSERT_TRANSITION && map->owns_descriptors() &&
      !map->GetBackPointer()->IsUndefined() &&
      TransitionArray::CanHaveMoreTransitions(map)) {
    return ShareDescriptor(map, descriptors, descriptor);
  }

  int nof = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::CopyUpTo(descriptors, nof, 1);
  new_descriptors->Append(descriptor);

  Handle<LayoutDescriptor> new_layout_descriptor =
      FLAG_unbox_double_fields
          ? LayoutDescriptor::New(map, new_descriptors, nof + 1)
          : handle(LayoutDescriptor::FastPointerLayout(), map->GetIsolate());

  return CopyReplaceDescriptors(map, new_descriptors, new_layout_descriptor,
                                flag, descriptor->GetKey(), "CopyAddDescriptor",
                                SIMPLE_PROPERTY_TRANSITION);
}


Handle<Map> Map::CopyInsertDescriptor(Handle<Map> map,
                                      Descriptor* descriptor,
                                      TransitionFlag flag) {
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors());

  // We replace the key if it is already present.
  int index = old_descriptors->SearchWithCache(map->GetIsolate(),
                                               *descriptor->GetKey(), *map);
  if (index != DescriptorArray::kNotFound) {
    return CopyReplaceDescriptor(map, old_descriptors, descriptor, index, flag);
  }
  return CopyAddDescriptor(map, descriptor, flag);
}


Handle<DescriptorArray> DescriptorArray::CopyUpTo(
    Handle<DescriptorArray> desc,
    int enumeration_index,
    int slack) {
  return DescriptorArray::CopyUpToAddAttributes(
      desc, enumeration_index, NONE, slack);
}


Handle<DescriptorArray> DescriptorArray::CopyUpToAddAttributes(
    Handle<DescriptorArray> desc,
    int enumeration_index,
    PropertyAttributes attributes,
    int slack) {
  if (enumeration_index + slack == 0) {
    return desc->GetIsolate()->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(desc->GetIsolate(), size, slack);

  if (attributes != NONE) {
    for (int i = 0; i < size; ++i) {
      Object* value = desc->GetValue(i);
      Name* key = desc->GetKey(i);
      PropertyDetails details = desc->GetDetails(i);
      // Bulk attribute changes never affect private properties.
      if (!key->IsPrivate()) {
        int mask = DONT_DELETE | DONT_ENUM;
        // READ_ONLY is an invalid attribute for JS setters/getters.
        if (details.type() != ACCESSOR_CONSTANT || !value->IsAccessorPair()) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      Descriptor inner_desc(
          handle(key), handle(value, desc->GetIsolate()), details);
      descriptors->SetDescriptor(i, &inner_desc);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      descriptors->CopyFrom(i, *desc);
    }
  }

  if (desc->number_of_descriptors() != enumeration_index) descriptors->Sort();

  return descriptors;
}


bool DescriptorArray::IsEqualUpTo(DescriptorArray* desc, int nof_descriptors) {
  for (int i = 0; i < nof_descriptors; i++) {
    if (GetKey(i) != desc->GetKey(i) || GetValue(i) != desc->GetValue(i)) {
      return false;
    }
    PropertyDetails details = GetDetails(i);
    PropertyDetails other_details = desc->GetDetails(i);
    if (details.type() != other_details.type() ||
        !details.representation().Equals(other_details.representation())) {
      return false;
    }
  }
  return true;
}


Handle<Map> Map::CopyReplaceDescriptor(Handle<Map> map,
                                       Handle<DescriptorArray> descriptors,
                                       Descriptor* descriptor,
                                       int insertion_index,
                                       TransitionFlag flag) {
  Handle<Name> key = descriptor->GetKey();
  DCHECK(*key == descriptors->GetKey(insertion_index));

  Handle<DescriptorArray> new_descriptors = DescriptorArray::CopyUpTo(
      descriptors, map->NumberOfOwnDescriptors());

  new_descriptors->Replace(insertion_index, descriptor);
  Handle<LayoutDescriptor> new_layout_descriptor = LayoutDescriptor::New(
      map, new_descriptors, new_descriptors->number_of_descriptors());

  SimpleTransitionFlag simple_flag =
      (insertion_index == descriptors->number_of_descriptors() - 1)
          ? SIMPLE_PROPERTY_TRANSITION
          : PROPERTY_TRANSITION;
  return CopyReplaceDescriptors(map, new_descriptors, new_layout_descriptor,
                                flag, key, "CopyReplaceDescriptor",
                                simple_flag);
}


void Map::UpdateCodeCache(Handle<Map> map,
                          Handle<Name> name,
                          Handle<Code> code) {
  Isolate* isolate = map->GetIsolate();
  HandleScope scope(isolate);
  // Allocate the code cache if not present.
  if (map->code_cache()->IsFixedArray()) {
    Handle<Object> result = isolate->factory()->NewCodeCache();
    map->set_code_cache(*result);
  }

  // Update the code cache.
  Handle<CodeCache> code_cache(CodeCache::cast(map->code_cache()), isolate);
  CodeCache::Update(code_cache, name, code);
}


Object* Map::FindInCodeCache(Name* name, Code::Flags flags) {
  // Do a lookup if a code cache exists.
  if (!code_cache()->IsFixedArray()) {
    return CodeCache::cast(code_cache())->Lookup(name, flags);
  } else {
    return GetHeap()->undefined_value();
  }
}


int Map::IndexInCodeCache(Object* name, Code* code) {
  // Get the internal index if a code cache exists.
  if (!code_cache()->IsFixedArray()) {
    return CodeCache::cast(code_cache())->GetIndex(name, code);
  }
  return -1;
}


void Map::RemoveFromCodeCache(Name* name, Code* code, int index) {
  // No GC is supposed to happen between a call to IndexInCodeCache and
  // RemoveFromCodeCache so the code cache must be there.
  DCHECK(!code_cache()->IsFixedArray());
  CodeCache::cast(code_cache())->RemoveByIndex(name, code, index);
}


void CodeCache::Update(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // The number of monomorphic stubs for normal load/store/call IC's can grow to
  // a large number and therefore they need to go into a hash table. They are
  // used to load global properties from cells.
  if (code->type() == Code::NORMAL) {
    // Make sure that a hash table is allocated for the normal load code cache.
    if (code_cache->normal_type_cache()->IsUndefined()) {
      Handle<Object> result =
          CodeCacheHashTable::New(code_cache->GetIsolate(),
                                  CodeCacheHashTable::kInitialSize);
      code_cache->set_normal_type_cache(*result);
    }
    UpdateNormalTypeCache(code_cache, name, code);
  } else {
    DCHECK(code_cache->default_cache()->IsFixedArray());
    UpdateDefaultCache(code_cache, name, code);
  }
}


void CodeCache::UpdateDefaultCache(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // When updating the default code cache we disregard the type encoded in the
  // flags. This allows call constant stubs to overwrite call field
  // stubs, etc.
  Code::Flags flags = Code::RemoveTypeFromFlags(code->flags());

  // First check whether we can update existing code cache without
  // extending it.
  Handle<FixedArray> cache = handle(code_cache->default_cache());
  int length = cache->length();
  {
    DisallowHeapAllocation no_alloc;
    int deleted_index = -1;
    for (int i = 0; i < length; i += kCodeCacheEntrySize) {
      Object* key = cache->get(i);
      if (key->IsNull()) {
        if (deleted_index < 0) deleted_index = i;
        continue;
      }
      if (key->IsUndefined()) {
        if (deleted_index >= 0) i = deleted_index;
        cache->set(i + kCodeCacheEntryNameOffset, *name);
        cache->set(i + kCodeCacheEntryCodeOffset, *code);
        return;
      }
      if (name->Equals(Name::cast(key))) {
        Code::Flags found =
            Code::cast(cache->get(i + kCodeCacheEntryCodeOffset))->flags();
        if (Code::RemoveTypeFromFlags(found) == flags) {
          cache->set(i + kCodeCacheEntryCodeOffset, *code);
          return;
        }
      }
    }

    // Reached the end of the code cache.  If there were deleted
    // elements, reuse the space for the first of them.
    if (deleted_index >= 0) {
      cache->set(deleted_index + kCodeCacheEntryNameOffset, *name);
      cache->set(deleted_index + kCodeCacheEntryCodeOffset, *code);
      return;
    }
  }

  // Extend the code cache with some new entries (at least one). Must be a
  // multiple of the entry size.
  Isolate* isolate = cache->GetIsolate();
  int new_length = length + (length >> 1) + kCodeCacheEntrySize;
  new_length = new_length - new_length % kCodeCacheEntrySize;
  DCHECK((new_length % kCodeCacheEntrySize) == 0);
  cache = isolate->factory()->CopyFixedArrayAndGrow(cache, new_length - length);

  // Add the (name, code) pair to the new cache.
  cache->set(length + kCodeCacheEntryNameOffset, *name);
  cache->set(length + kCodeCacheEntryCodeOffset, *code);
  code_cache->set_default_cache(*cache);
}


void CodeCache::UpdateNormalTypeCache(
    Handle<CodeCache> code_cache, Handle<Name> name, Handle<Code> code) {
  // Adding a new entry can cause a new cache to be allocated.
  Handle<CodeCacheHashTable> cache(
      CodeCacheHashTable::cast(code_cache->normal_type_cache()));
  Handle<Object> new_cache = CodeCacheHashTable::Put(cache, name, code);
  code_cache->set_normal_type_cache(*new_cache);
}


Object* CodeCache::Lookup(Name* name, Code::Flags flags) {
  Object* result = LookupDefaultCache(name, Code::RemoveTypeFromFlags(flags));
  if (result->IsCode()) {
    if (Code::cast(result)->flags() == flags) return result;
    return GetHeap()->undefined_value();
  }
  return LookupNormalTypeCache(name, flags);
}


Object* CodeCache::LookupDefaultCache(Name* name, Code::Flags flags) {
  FixedArray* cache = default_cache();
  int length = cache->length();
  for (int i = 0; i < length; i += kCodeCacheEntrySize) {
    Object* key = cache->get(i + kCodeCacheEntryNameOffset);
    // Skip deleted elements.
    if (key->IsNull()) continue;
    if (key->IsUndefined()) return key;
    if (name->Equals(Name::cast(key))) {
      Code* code = Code::cast(cache->get(i + kCodeCacheEntryCodeOffset));
      if (Code::RemoveTypeFromFlags(code->flags()) == flags) {
        return code;
      }
    }
  }
  return GetHeap()->undefined_value();
}


Object* CodeCache::LookupNormalTypeCache(Name* name, Code::Flags flags) {
  if (!normal_type_cache()->IsUndefined()) {
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->Lookup(name, flags);
  } else {
    return GetHeap()->undefined_value();
  }
}


int CodeCache::GetIndex(Object* name, Code* code) {
  if (code->type() == Code::NORMAL) {
    if (normal_type_cache()->IsUndefined()) return -1;
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->GetIndex(Name::cast(name), code->flags());
  }

  FixedArray* array = default_cache();
  int len = array->length();
  for (int i = 0; i < len; i += kCodeCacheEntrySize) {
    if (array->get(i + kCodeCacheEntryCodeOffset) == code) return i + 1;
  }
  return -1;
}


void CodeCache::RemoveByIndex(Object* name, Code* code, int index) {
  if (code->type() == Code::NORMAL) {
    DCHECK(!normal_type_cache()->IsUndefined());
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    DCHECK(cache->GetIndex(Name::cast(name), code->flags()) == index);
    cache->RemoveByIndex(index);
  } else {
    FixedArray* array = default_cache();
    DCHECK(array->length() >= index && array->get(index)->IsCode());
    // Use null instead of undefined for deleted elements to distinguish
    // deleted elements from unused elements.  This distinction is used
    // when looking up in the cache and when updating the cache.
    DCHECK_EQ(1, kCodeCacheEntryCodeOffset - kCodeCacheEntryNameOffset);
    array->set_null(index - 1);  // Name.
    array->set_null(index);  // Code.
  }
}


// The key in the code cache hash table consists of the property name and the
// code object. The actual match is on the name and the code flags. If a key
// is created using the flags and not a code object it can only be used for
// lookup not to create a new entry.
class CodeCacheHashTableKey : public HashTableKey {
 public:
  CodeCacheHashTableKey(Handle<Name> name, Code::Flags flags)
      : name_(name), flags_(flags), code_() { }

  CodeCacheHashTableKey(Handle<Name> name, Handle<Code> code)
      : name_(name), flags_(code->flags()), code_(code) { }

  bool IsMatch(Object* other) override {
    if (!other->IsFixedArray()) return false;
    FixedArray* pair = FixedArray::cast(other);
    Name* name = Name::cast(pair->get(0));
    Code::Flags flags = Code::cast(pair->get(1))->flags();
    if (flags != flags_) {
      return false;
    }
    return name_->Equals(name);
  }

  static uint32_t NameFlagsHashHelper(Name* name, Code::Flags flags) {
    return name->Hash() ^ flags;
  }

  uint32_t Hash() override { return NameFlagsHashHelper(*name_, flags_); }

  uint32_t HashForObject(Object* obj) override {
    FixedArray* pair = FixedArray::cast(obj);
    Name* name = Name::cast(pair->get(0));
    Code* code = Code::cast(pair->get(1));
    return NameFlagsHashHelper(name, code->flags());
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) override {
    Handle<Code> code = code_.ToHandleChecked();
    Handle<FixedArray> pair = isolate->factory()->NewFixedArray(2);
    pair->set(0, *name_);
    pair->set(1, *code);
    return pair;
  }

 private:
  Handle<Name> name_;
  Code::Flags flags_;
  // TODO(jkummerow): We should be able to get by without this.
  MaybeHandle<Code> code_;
};


Object* CodeCacheHashTable::Lookup(Name* name, Code::Flags flags) {
  DisallowHeapAllocation no_alloc;
  CodeCacheHashTableKey key(handle(name), flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<CodeCacheHashTable> CodeCacheHashTable::Put(
    Handle<CodeCacheHashTable> cache, Handle<Name> name, Handle<Code> code) {
  CodeCacheHashTableKey key(name, code);

  Handle<CodeCacheHashTable> new_cache = EnsureCapacity(cache, 1, &key);

  int entry = new_cache->FindInsertionEntry(key.Hash());
  Handle<Object> k = key.AsHandle(cache->GetIsolate());

  new_cache->set(EntryToIndex(entry), *k);
  new_cache->set(EntryToIndex(entry) + 1, *code);
  new_cache->ElementAdded();
  return new_cache;
}


int CodeCacheHashTable::GetIndex(Name* name, Code::Flags flags) {
  DisallowHeapAllocation no_alloc;
  CodeCacheHashTableKey key(handle(name), flags);
  int entry = FindEntry(&key);
  return (entry == kNotFound) ? -1 : entry;
}


void CodeCacheHashTable::RemoveByIndex(int index) {
  DCHECK(index >= 0);
  Heap* heap = GetHeap();
  set(EntryToIndex(index), heap->the_hole_value());
  set(EntryToIndex(index) + 1, heap->the_hole_value());
  ElementRemoved();
}


void PolymorphicCodeCache::Update(Handle<PolymorphicCodeCache> code_cache,
                                  MapHandleList* maps,
                                  Code::Flags flags,
                                  Handle<Code> code) {
  Isolate* isolate = code_cache->GetIsolate();
  if (code_cache->cache()->IsUndefined()) {
    Handle<PolymorphicCodeCacheHashTable> result =
        PolymorphicCodeCacheHashTable::New(
            isolate,
            PolymorphicCodeCacheHashTable::kInitialSize);
    code_cache->set_cache(*result);
  } else {
    // This entry shouldn't be contained in the cache yet.
    DCHECK(PolymorphicCodeCacheHashTable::cast(code_cache->cache())
               ->Lookup(maps, flags)->IsUndefined());
  }
  Handle<PolymorphicCodeCacheHashTable> hash_table =
      handle(PolymorphicCodeCacheHashTable::cast(code_cache->cache()));
  Handle<PolymorphicCodeCacheHashTable> new_cache =
      PolymorphicCodeCacheHashTable::Put(hash_table, maps, flags, code);
  code_cache->set_cache(*new_cache);
}


Handle<Object> PolymorphicCodeCache::Lookup(MapHandleList* maps,
                                            Code::Flags flags) {
  if (!cache()->IsUndefined()) {
    PolymorphicCodeCacheHashTable* hash_table =
        PolymorphicCodeCacheHashTable::cast(cache());
    return Handle<Object>(hash_table->Lookup(maps, flags), GetIsolate());
  } else {
    return GetIsolate()->factory()->undefined_value();
  }
}


// Despite their name, object of this class are not stored in the actual
// hash table; instead they're temporarily used for lookups. It is therefore
// safe to have a weak (non-owning) pointer to a MapList as a member field.
class PolymorphicCodeCacheHashTableKey : public HashTableKey {
 public:
  // Callers must ensure that |maps| outlives the newly constructed object.
  PolymorphicCodeCacheHashTableKey(MapHandleList* maps, int code_flags)
      : maps_(maps),
        code_flags_(code_flags) {}

  bool IsMatch(Object* other) override {
    MapHandleList other_maps(kDefaultListAllocationSize);
    int other_flags;
    FromObject(other, &other_flags, &other_maps);
    if (code_flags_ != other_flags) return false;
    if (maps_->length() != other_maps.length()) return false;
    // Compare just the hashes first because it's faster.
    int this_hash = MapsHashHelper(maps_, code_flags_);
    int other_hash = MapsHashHelper(&other_maps, other_flags);
    if (this_hash != other_hash) return false;

    // Full comparison: for each map in maps_, look for an equivalent map in
    // other_maps. This implementation is slow, but probably good enough for
    // now because the lists are short (<= 4 elements currently).
    for (int i = 0; i < maps_->length(); ++i) {
      bool match_found = false;
      for (int j = 0; j < other_maps.length(); ++j) {
        if (*(maps_->at(i)) == *(other_maps.at(j))) {
          match_found = true;
          break;
        }
      }
      if (!match_found) return false;
    }
    return true;
  }

  static uint32_t MapsHashHelper(MapHandleList* maps, int code_flags) {
    uint32_t hash = code_flags;
    for (int i = 0; i < maps->length(); ++i) {
      hash ^= maps->at(i)->Hash();
    }
    return hash;
  }

  uint32_t Hash() override { return MapsHashHelper(maps_, code_flags_); }

  uint32_t HashForObject(Object* obj) override {
    MapHandleList other_maps(kDefaultListAllocationSize);
    int other_flags;
    FromObject(obj, &other_flags, &other_maps);
    return MapsHashHelper(&other_maps, other_flags);
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) override {
    // The maps in |maps_| must be copied to a newly allocated FixedArray,
    // both because the referenced MapList is short-lived, and because C++
    // objects can't be stored in the heap anyway.
    Handle<FixedArray> list =
        isolate->factory()->NewUninitializedFixedArray(maps_->length() + 1);
    list->set(0, Smi::FromInt(code_flags_));
    for (int i = 0; i < maps_->length(); ++i) {
      list->set(i + 1, *maps_->at(i));
    }
    return list;
  }

 private:
  static MapHandleList* FromObject(Object* obj,
                                   int* code_flags,
                                   MapHandleList* maps) {
    FixedArray* list = FixedArray::cast(obj);
    maps->Rewind(0);
    *code_flags = Smi::cast(list->get(0))->value();
    for (int i = 1; i < list->length(); ++i) {
      maps->Add(Handle<Map>(Map::cast(list->get(i))));
    }
    return maps;
  }

  MapHandleList* maps_;  // weak.
  int code_flags_;
  static const int kDefaultListAllocationSize = kMaxKeyedPolymorphism + 1;
};


Object* PolymorphicCodeCacheHashTable::Lookup(MapHandleList* maps,
                                              int code_kind) {
  DisallowHeapAllocation no_alloc;
  PolymorphicCodeCacheHashTableKey key(maps, code_kind);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Handle<PolymorphicCodeCacheHashTable> PolymorphicCodeCacheHashTable::Put(
      Handle<PolymorphicCodeCacheHashTable> hash_table,
      MapHandleList* maps,
      int code_kind,
      Handle<Code> code) {
  PolymorphicCodeCacheHashTableKey key(maps, code_kind);
  Handle<PolymorphicCodeCacheHashTable> cache =
      EnsureCapacity(hash_table, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());

  Handle<Object> obj = key.AsHandle(hash_table->GetIsolate());
  cache->set(EntryToIndex(entry), *obj);
  cache->set(EntryToIndex(entry) + 1, *code);
  cache->ElementAdded();
  return cache;
}


void FixedArray::Shrink(int new_length) {
  DCHECK(0 <= new_length && new_length <= length());
  if (new_length < length()) {
    GetHeap()->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(
        this, length() - new_length);
  }
}


void FixedArray::CopyTo(int pos, FixedArray* dest, int dest_pos, int len) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = dest->GetWriteBarrierMode(no_gc);
  for (int index = 0; index < len; index++) {
    dest->set(dest_pos+index, get(pos+index), mode);
  }
}


#ifdef DEBUG
bool FixedArray::IsEqualTo(FixedArray* other) {
  if (length() != other->length()) return false;
  for (int i = 0 ; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif


// static
void WeakFixedArray::Set(Handle<WeakFixedArray> array, int index,
                         Handle<HeapObject> value) {
  DCHECK(array->IsEmptySlot(index));  // Don't overwrite anything.
  Handle<WeakCell> cell =
      value->IsMap() ? Map::WeakCellForMap(Handle<Map>::cast(value))
                     : array->GetIsolate()->factory()->NewWeakCell(value);
  Handle<FixedArray>::cast(array)->set(index + kFirstIndex, *cell);
  if (FLAG_trace_weak_arrays) {
    PrintF("[WeakFixedArray: storing at index %d ]\n", index);
  }
  array->set_last_used_index(index);
}


// static
Handle<WeakFixedArray> WeakFixedArray::Add(Handle<Object> maybe_array,
                                           Handle<HeapObject> value,
                                           int* assigned_index) {
  Handle<WeakFixedArray> array =
      (maybe_array.is_null() || !maybe_array->IsWeakFixedArray())
          ? Allocate(value->GetIsolate(), 1, Handle<WeakFixedArray>::null())
          : Handle<WeakFixedArray>::cast(maybe_array);
  // Try to store the new entry if there's room. Optimize for consecutive
  // accesses.
  int first_index = array->last_used_index();
  int length = array->Length();
  if (length > 0) {
    for (int i = first_index;;) {
      if (array->IsEmptySlot((i))) {
        WeakFixedArray::Set(array, i, value);
        if (assigned_index != NULL) *assigned_index = i;
        return array;
      }
      if (FLAG_trace_weak_arrays) {
        PrintF("[WeakFixedArray: searching for free slot]\n");
      }
      i = (i + 1) % length;
      if (i == first_index) break;
    }
  }

  // No usable slot found, grow the array.
  int new_length = length == 0 ? 1 : length + (length >> 1) + 4;
  Handle<WeakFixedArray> new_array =
      Allocate(array->GetIsolate(), new_length, array);
  if (FLAG_trace_weak_arrays) {
    PrintF("[WeakFixedArray: growing to size %d ]\n", new_length);
  }
  WeakFixedArray::Set(new_array, length, value);
  if (assigned_index != NULL) *assigned_index = length;
  return new_array;
}


template <class CompactionCallback>
void WeakFixedArray::Compact() {
  FixedArray* array = FixedArray::cast(this);
  int new_length = kFirstIndex;
  for (int i = kFirstIndex; i < array->length(); i++) {
    Object* element = array->get(i);
    if (element->IsSmi()) continue;
    if (WeakCell::cast(element)->cleared()) continue;
    Object* value = WeakCell::cast(element)->value();
    CompactionCallback::Callback(value, i - kFirstIndex,
                                 new_length - kFirstIndex);
    array->set(new_length++, element);
  }
  array->Shrink(new_length);
  set_last_used_index(0);
}


void WeakFixedArray::Iterator::Reset(Object* maybe_array) {
  if (maybe_array->IsWeakFixedArray()) {
    list_ = WeakFixedArray::cast(maybe_array);
    index_ = 0;
#ifdef DEBUG
    last_used_index_ = list_->last_used_index();
#endif  // DEBUG
  }
}


void JSObject::PrototypeRegistryCompactionCallback::Callback(Object* value,
                                                             int old_index,
                                                             int new_index) {
  DCHECK(value->IsMap() && Map::cast(value)->is_prototype_map());
  Map* map = Map::cast(value);
  DCHECK(map->prototype_info()->IsPrototypeInfo());
  PrototypeInfo* proto_info = PrototypeInfo::cast(map->prototype_info());
  DCHECK_EQ(old_index, proto_info->registry_slot());
  proto_info->set_registry_slot(new_index);
}


template void WeakFixedArray::Compact<WeakFixedArray::NullCallback>();
template void
WeakFixedArray::Compact<JSObject::PrototypeRegistryCompactionCallback>();


bool WeakFixedArray::Remove(Handle<HeapObject> value) {
  if (Length() == 0) return false;
  // Optimize for the most recently added element to be removed again.
  int first_index = last_used_index();
  for (int i = first_index;;) {
    if (Get(i) == *value) {
      Clear(i);
      // Users of WeakFixedArray should make sure that there are no duplicates.
      return true;
    }
    i = (i + 1) % Length();
    if (i == first_index) return false;
  }
  UNREACHABLE();
}


// static
Handle<WeakFixedArray> WeakFixedArray::Allocate(
    Isolate* isolate, int size, Handle<WeakFixedArray> initialize_from) {
  DCHECK(0 <= size);
  Handle<FixedArray> result =
      isolate->factory()->NewUninitializedFixedArray(size + kFirstIndex);
  int index = 0;
  if (!initialize_from.is_null()) {
    DCHECK(initialize_from->Length() <= size);
    Handle<FixedArray> raw_source = Handle<FixedArray>::cast(initialize_from);
    // Copy the entries without compacting, since the PrototypeInfo relies on
    // the index of the entries not to change.
    while (index < raw_source->length()) {
      result->set(index, raw_source->get(index));
      index++;
    }
  }
  while (index < result->length()) {
    result->set(index, Smi::FromInt(0));
    index++;
  }
  return Handle<WeakFixedArray>::cast(result);
}


Handle<ArrayList> ArrayList::Add(Handle<ArrayList> array, Handle<Object> obj,
                                 AddMode mode) {
  int length = array->Length();
  array = EnsureSpace(array, length + 1);
  if (mode == kReloadLengthAfterAllocation) {
    DCHECK(array->Length() <= length);
    length = array->Length();
  }
  array->Set(length, *obj);
  array->SetLength(length + 1);
  return array;
}


Handle<ArrayList> ArrayList::Add(Handle<ArrayList> array, Handle<Object> obj1,
                                 Handle<Object> obj2, AddMode mode) {
  int length = array->Length();
  array = EnsureSpace(array, length + 2);
  if (mode == kReloadLengthAfterAllocation) {
    length = array->Length();
  }
  array->Set(length, *obj1);
  array->Set(length + 1, *obj2);
  array->SetLength(length + 2);
  return array;
}


bool ArrayList::IsFull() {
  int capacity = length();
  return kFirstIndex + Length() == capacity;
}


Handle<ArrayList> ArrayList::EnsureSpace(Handle<ArrayList> array, int length) {
  int capacity = array->length();
  bool empty = (capacity == 0);
  if (capacity < kFirstIndex + length) {
    Isolate* isolate = array->GetIsolate();
    int new_capacity = kFirstIndex + length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array = Handle<ArrayList>::cast(
        isolate->factory()->CopyFixedArrayAndGrow(array, grow_by));
    if (empty) array->SetLength(0);
  }
  return array;
}

Handle<DescriptorArray> DescriptorArray::Allocate(Isolate* isolate,
                                                  int number_of_descriptors,
                                                  int slack,
                                                  PretenureFlag pretenure) {
  DCHECK(0 <= number_of_descriptors);
  Factory* factory = isolate->factory();
  // Do not use DescriptorArray::cast on incomplete object.
  int size = number_of_descriptors + slack;
  if (size == 0) return factory->empty_descriptor_array();
  // Allocate the array of keys.
  Handle<FixedArray> result =
      factory->NewFixedArray(LengthFor(size), pretenure);

  result->set(kDescriptorLengthIndex, Smi::FromInt(number_of_descriptors));
  result->set(kEnumCacheIndex, Smi::FromInt(0));
  return Handle<DescriptorArray>::cast(result);
}


void DescriptorArray::ClearEnumCache() {
  set(kEnumCacheIndex, Smi::FromInt(0));
}


void DescriptorArray::Replace(int index, Descriptor* descriptor) {
  descriptor->SetSortedKeyIndex(GetSortedKeyIndex(index));
  Set(index, descriptor);
}


// static
void DescriptorArray::SetEnumCache(Handle<DescriptorArray> descriptors,
                                   Isolate* isolate,
                                   Handle<FixedArray> new_cache,
                                   Handle<FixedArray> new_index_cache) {
  DCHECK(!descriptors->IsEmpty());
  FixedArray* bridge_storage;
  bool needs_new_enum_cache = !descriptors->HasEnumCache();
  if (needs_new_enum_cache) {
    bridge_storage = *isolate->factory()->NewFixedArray(
        DescriptorArray::kEnumCacheBridgeLength);
  } else {
    bridge_storage = FixedArray::cast(descriptors->get(kEnumCacheIndex));
  }
  bridge_storage->set(kEnumCacheBridgeCacheIndex, *new_cache);
  bridge_storage->set(kEnumCacheBridgeIndicesCacheIndex,
                      new_index_cache.is_null() ? Object::cast(Smi::FromInt(0))
                                                : *new_index_cache);
  if (needs_new_enum_cache) {
    descriptors->set(kEnumCacheIndex, bridge_storage);
  }
}


void DescriptorArray::CopyFrom(int index, DescriptorArray* src) {
  Object* value = src->GetValue(index);
  PropertyDetails details = src->GetDetails(index);
  Descriptor desc(handle(src->GetKey(index)),
                  handle(value, src->GetIsolate()),
                  details);
  SetDescriptor(index, &desc);
}


void DescriptorArray::Sort() {
  // In-place heap sort.
  int len = number_of_descriptors();
  // Reset sorting since the descriptor array might contain invalid pointers.
  for (int i = 0; i < len; ++i) SetSortedKey(i, i);
  // Bottom-up max-heap construction.
  // Index of the last node with children
  const int max_parent_index = (len / 2) - 1;
  for (int i = max_parent_index; i >= 0; --i) {
    int parent_index = i;
    const uint32_t parent_hash = GetSortedKey(i)->Hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      // Now element at child_index could be < its children.
      parent_index = child_index;  // parent_hash remains correct.
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    SwapSortedKeys(0, i);
    // Shift down the new top element.
    int parent_index = 0;
    const uint32_t parent_hash = GetSortedKey(parent_index)->Hash();
    const int max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetSortedKey(child_index)->Hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      SwapSortedKeys(parent_index, child_index);
      parent_index = child_index;
    }
  }
  DCHECK(IsSortedNoDuplicates());
}


Handle<AccessorPair> AccessorPair::Copy(Handle<AccessorPair> pair) {
  Handle<AccessorPair> copy = pair->GetIsolate()->factory()->NewAccessorPair();
  copy->set_getter(pair->getter());
  copy->set_setter(pair->setter());
  return copy;
}

Handle<Object> AccessorPair::GetComponent(Handle<AccessorPair> accessor_pair,
                                          AccessorComponent component) {
  Object* accessor = accessor_pair->get(component);
  if (accessor->IsFunctionTemplateInfo()) {
    return ApiNatives::InstantiateFunction(
               handle(FunctionTemplateInfo::cast(accessor)))
        .ToHandleChecked();
  }
  Isolate* isolate = accessor_pair->GetIsolate();
  if (accessor->IsTheHole()) {
    return isolate->factory()->undefined_value();
  }
  return handle(accessor, isolate);
}

Handle<DeoptimizationInputData> DeoptimizationInputData::New(
    Isolate* isolate, int deopt_entry_count, PretenureFlag pretenure) {
  return Handle<DeoptimizationInputData>::cast(
      isolate->factory()->NewFixedArray(LengthFor(deopt_entry_count),
                                        pretenure));
}


Handle<DeoptimizationOutputData> DeoptimizationOutputData::New(
    Isolate* isolate,
    int number_of_deopt_points,
    PretenureFlag pretenure) {
  Handle<FixedArray> result;
  if (number_of_deopt_points == 0) {
    result = isolate->factory()->empty_fixed_array();
  } else {
    result = isolate->factory()->NewFixedArray(
        LengthOfFixedArray(number_of_deopt_points), pretenure);
  }
  return Handle<DeoptimizationOutputData>::cast(result);
}


// static
Handle<LiteralsArray> LiteralsArray::New(Isolate* isolate,
                                         Handle<TypeFeedbackVector> vector,
                                         int number_of_literals,
                                         PretenureFlag pretenure) {
  Handle<FixedArray> literals = isolate->factory()->NewFixedArray(
      number_of_literals + kFirstLiteralIndex, pretenure);
  Handle<LiteralsArray> casted_literals = Handle<LiteralsArray>::cast(literals);
  casted_literals->set_feedback_vector(*vector);
  return casted_literals;
}

int HandlerTable::LookupRange(int pc_offset, int* data_out,
                              CatchPrediction* prediction_out) {
  int innermost_handler = -1;
#ifdef DEBUG
  // Assuming that ranges are well nested, we don't need to track the innermost
  // offsets. This is just to verify that the table is actually well nested.
  int innermost_start = std::numeric_limits<int>::min();
  int innermost_end = std::numeric_limits<int>::max();
#endif
  for (int i = 0; i < length(); i += kRangeEntrySize) {
    int start_offset = Smi::cast(get(i + kRangeStartIndex))->value();
    int end_offset = Smi::cast(get(i + kRangeEndIndex))->value();
    int handler_field = Smi::cast(get(i + kRangeHandlerIndex))->value();
    int handler_offset = HandlerOffsetField::decode(handler_field);
    CatchPrediction prediction = HandlerPredictionField::decode(handler_field);
    int handler_data = Smi::cast(get(i + kRangeDataIndex))->value();
    if (pc_offset > start_offset && pc_offset <= end_offset) {
      DCHECK_GE(start_offset, innermost_start);
      DCHECK_LT(end_offset, innermost_end);
      innermost_handler = handler_offset;
#ifdef DEBUG
      innermost_start = start_offset;
      innermost_end = end_offset;
#endif
      if (data_out) *data_out = handler_data;
      if (prediction_out) *prediction_out = prediction;
    }
  }
  return innermost_handler;
}


// TODO(turbofan): Make sure table is sorted and use binary search.
int HandlerTable::LookupReturn(int pc_offset, CatchPrediction* prediction_out) {
  for (int i = 0; i < length(); i += kReturnEntrySize) {
    int return_offset = Smi::cast(get(i + kReturnOffsetIndex))->value();
    int handler_field = Smi::cast(get(i + kReturnHandlerIndex))->value();
    if (pc_offset == return_offset) {
      if (prediction_out) {
        *prediction_out = HandlerPredictionField::decode(handler_field);
      }
      return HandlerOffsetField::decode(handler_field);
    }
  }
  return -1;
}


#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray* other) {
  if (IsEmpty()) return other->IsEmpty();
  if (other->IsEmpty()) return false;
  if (length() != other->length()) return false;
  for (int i = 0; i < length(); ++i) {
    if (get(i) != other->get(i)) return false;
  }
  return true;
}
#endif


bool String::LooksValid() {
  if (!GetIsolate()->heap()->Contains(this)) return false;
  return true;
}


// static
MaybeHandle<String> Name::ToFunctionName(Handle<Name> name) {
  if (name->IsString()) return Handle<String>::cast(name);
  // ES6 section 9.2.11 SetFunctionName, step 4.
  Isolate* const isolate = name->GetIsolate();
  Handle<Object> description(Handle<Symbol>::cast(name)->name(), isolate);
  if (description->IsUndefined()) return isolate->factory()->empty_string();
  IncrementalStringBuilder builder(isolate);
  builder.AppendCharacter('[');
  builder.AppendString(Handle<String>::cast(description));
  builder.AppendCharacter(']');
  return builder.Finish();
}


namespace {

bool AreDigits(const uint8_t* s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


int ParseDecimalInteger(const uint8_t* s, int from, int to) {
  DCHECK(to - from < 10);  // Overflow is not possible.
  DCHECK(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}

}  // namespace


// static
Handle<Object> String::ToNumber(Handle<String> subject) {
  Isolate* const isolate = subject->GetIsolate();

  // Flatten {subject} string first.
  subject = String::Flatten(subject);

  // Fast array index case.
  uint32_t index;
  if (subject->AsArrayIndex(&index)) {
    return isolate->factory()->NewNumberFromUint(index);
  }

  // Fast case: short integer or some sorts of junk values.
  if (subject->IsSeqOneByteString()) {
    int len = subject->length();
    if (len == 0) return handle(Smi::FromInt(0), isolate);

    DisallowHeapAllocation no_gc;
    uint8_t const* data = Handle<SeqOneByteString>::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->factory()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit
      // or the 'I' character ('Infinity'). All of that have codes not greater
      // than '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xa0) {
        return isolate->factory()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits
      // we know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->factory()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() && len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t hash = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->Hash();  // Force hash calculation.
        DCHECK_EQ(static_cast<int>(subject->hash_field()),
                  static_cast<int>(hash));
#endif
        subject->set_hash_field(hash);
      }
      return handle(Smi::FromInt(d), isolate);
    }
  }

  // Slower case.
  int flags = ALLOW_HEX | ALLOW_OCTAL | ALLOW_BINARY;
  return isolate->factory()->NewNumber(
      StringToDouble(isolate->unicode_cache(), subject, flags));
}


String::FlatContent String::GetFlatContent() {
  DCHECK(!AllowHeapAllocation::IsAllowed());
  int length = this->length();
  StringShape shape(this);
  String* string = this;
  int offset = 0;
  if (shape.representation_tag() == kConsStringTag) {
    ConsString* cons = ConsString::cast(string);
    if (cons->second()->length() != 0) {
      return FlatContent();
    }
    string = cons->first();
    shape = StringShape(string);
  }
  if (shape.representation_tag() == kSlicedStringTag) {
    SlicedString* slice = SlicedString::cast(string);
    offset = slice->offset();
    string = slice->parent();
    shape = StringShape(string);
    DCHECK(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.encoding_tag() == kOneByteStringTag) {
    const uint8_t* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqOneByteString::cast(string)->GetChars();
    } else {
      start = ExternalOneByteString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  } else {
    DCHECK(shape.encoding_tag() == kTwoByteStringTag);
    const uc16* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqTwoByteString::cast(string)->GetChars();
    } else {
      start = ExternalTwoByteString::cast(string)->GetChars();
    }
    return FlatContent(start + offset, length);
  }
}


base::SmartArrayPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                                RobustnessFlag robust_flag,
                                                int offset, int length,
                                                int* length_return) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return base::SmartArrayPointer<char>(NULL);
  }
  // Negative length means the to the end of the string.
  if (length < 0) length = kMaxInt - offset;

  // Compute the size of the UTF-8 string. Start at the specified offset.
  StringCharacterStream stream(this, offset);
  int character_position = offset;
  int utf8_bytes = 0;
  int last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    utf8_bytes += unibrow::Utf8::Length(character, last);
    last = character;
  }

  if (length_return) {
    *length_return = utf8_bytes;
  }

  char* result = NewArray<char>(utf8_bytes + 1);

  // Convert the UTF-16 string to a UTF-8 buffer. Start at the specified offset.
  stream.Reset(this, offset);
  character_position = offset;
  int utf8_byte_position = 0;
  last = unibrow::Utf16::kNoPreviousCharacter;
  while (stream.HasMore() && character_position++ < offset + length) {
    uint16_t character = stream.GetNext();
    if (allow_nulls == DISALLOW_NULLS && character == 0) {
      character = ' ';
    }
    utf8_byte_position +=
        unibrow::Utf8::Encode(result + utf8_byte_position, character, last);
    last = character;
  }
  result[utf8_byte_position] = 0;
  return base::SmartArrayPointer<char>(result);
}


base::SmartArrayPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                                RobustnessFlag robust_flag,
                                                int* length_return) {
  return ToCString(allow_nulls, robust_flag, 0, -1, length_return);
}


const uc16* String::GetTwoByteData(unsigned start) {
  DCHECK(!IsOneByteRepresentationUnderneath());
  switch (StringShape(this).representation_tag()) {
    case kSeqStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGetData(start);
    case kExternalStringTag:
      return ExternalTwoByteString::cast(this)->
        ExternalTwoByteStringGetData(start);
    case kSlicedStringTag: {
      SlicedString* slice = SlicedString::cast(this);
      return slice->parent()->GetTwoByteData(start + slice->offset());
    }
    case kConsStringTag:
      UNREACHABLE();
      return NULL;
  }
  UNREACHABLE();
  return NULL;
}


base::SmartArrayPointer<uc16> String::ToWideCString(
    RobustnessFlag robust_flag) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return base::SmartArrayPointer<uc16>();
  }
  StringCharacterStream stream(this);

  uc16* result = NewArray<uc16>(length() + 1);

  int i = 0;
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    result[i++] = character;
  }
  result[i] = 0;
  return base::SmartArrayPointer<uc16>(result);
}


const uc16* SeqTwoByteString::SeqTwoByteStringGetData(unsigned start) {
  return reinterpret_cast<uc16*>(
      reinterpret_cast<char*>(this) - kHeapObjectTag + kHeaderSize) + start;
}


void Relocatable::PostGarbageCollectionProcessing(Isolate* isolate) {
  Relocatable* current = isolate->relocatable_top();
  while (current != NULL) {
    current->PostGarbageCollection();
    current = current->prev_;
  }
}


// Reserve space for statics needing saving and restoring.
int Relocatable::ArchiveSpacePerThread() {
  return sizeof(Relocatable*);  // NOLINT
}


// Archive statics that are thread-local.
char* Relocatable::ArchiveState(Isolate* isolate, char* to) {
  *reinterpret_cast<Relocatable**>(to) = isolate->relocatable_top();
  isolate->set_relocatable_top(NULL);
  return to + ArchiveSpacePerThread();
}


// Restore statics that are thread-local.
char* Relocatable::RestoreState(Isolate* isolate, char* from) {
  isolate->set_relocatable_top(*reinterpret_cast<Relocatable**>(from));
  return from + ArchiveSpacePerThread();
}


char* Relocatable::Iterate(ObjectVisitor* v, char* thread_storage) {
  Relocatable* top = *reinterpret_cast<Relocatable**>(thread_storage);
  Iterate(v, top);
  return thread_storage + ArchiveSpacePerThread();
}


void Relocatable::Iterate(Isolate* isolate, ObjectVisitor* v) {
  Iterate(v, isolate->relocatable_top());
}


void Relocatable::Iterate(ObjectVisitor* v, Relocatable* top) {
  Relocatable* current = top;
  while (current != NULL) {
    current->IterateInstance(v);
    current = current->prev_;
  }
}


FlatStringReader::FlatStringReader(Isolate* isolate, Handle<String> str)
    : Relocatable(isolate),
      str_(str.location()),
      length_(str->length()) {
  PostGarbageCollection();
}


FlatStringReader::FlatStringReader(Isolate* isolate, Vector<const char> input)
    : Relocatable(isolate),
      str_(0),
      is_one_byte_(true),
      length_(input.length()),
      start_(input.start()) {}


void FlatStringReader::PostGarbageCollection() {
  if (str_ == NULL) return;
  Handle<String> str(str_);
  DCHECK(str->IsFlat());
  DisallowHeapAllocation no_gc;
  // This does not actually prevent the vector from being relocated later.
  String::FlatContent content = str->GetFlatContent();
  DCHECK(content.IsFlat());
  is_one_byte_ = content.IsOneByte();
  if (is_one_byte_) {
    start_ = content.ToOneByteVector().start();
  } else {
    start_ = content.ToUC16Vector().start();
  }
}


void ConsStringIterator::Initialize(ConsString* cons_string, int offset) {
  DCHECK(cons_string != NULL);
  root_ = cons_string;
  consumed_ = offset;
  // Force stack blown condition to trigger restart.
  depth_ = 1;
  maximum_depth_ = kStackSize + depth_;
  DCHECK(StackBlown());
}


String* ConsStringIterator::Continue(int* offset_out) {
  DCHECK(depth_ != 0);
  DCHECK_EQ(0, *offset_out);
  bool blew_stack = StackBlown();
  String* string = NULL;
  // Get the next leaf if there is one.
  if (!blew_stack) string = NextLeaf(&blew_stack);
  // Restart search from root.
  if (blew_stack) {
    DCHECK(string == NULL);
    string = Search(offset_out);
  }
  // Ensure future calls return null immediately.
  if (string == NULL) Reset(NULL);
  return string;
}


String* ConsStringIterator::Search(int* offset_out) {
  ConsString* cons_string = root_;
  // Reset the stack, pushing the root string.
  depth_ = 1;
  maximum_depth_ = 1;
  frames_[0] = cons_string;
  const int consumed = consumed_;
  int offset = 0;
  while (true) {
    // Loop until the string is found which contains the target offset.
    String* string = cons_string->first();
    int length = string->length();
    int32_t type;
    if (consumed < offset + length) {
      // Target offset is in the left branch.
      // Keep going if we're still in a ConString.
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushLeft(cons_string);
        continue;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
    } else {
      // Descend right.
      // Update progress through the string.
      offset += length;
      // Keep going if we're still in a ConString.
      string = cons_string->second();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) == kConsStringTag) {
        cons_string = ConsString::cast(string);
        PushRight(cons_string);
        continue;
      }
      // Need this to be updated for the current string.
      length = string->length();
      // Account for the possibility of an empty right leaf.
      // This happens only if we have asked for an offset outside the string.
      if (length == 0) {
        // Reset so future operations will return null immediately.
        Reset(NULL);
        return NULL;
      }
      // Tell the stack we're done descending.
      AdjustMaximumDepth();
      // Pop stack so next iteration is in correct place.
      Pop();
    }
    DCHECK(length != 0);
    // Adjust return values and exit.
    consumed_ = offset + length;
    *offset_out = consumed - offset;
    return string;
  }
  UNREACHABLE();
  return NULL;
}


String* ConsStringIterator::NextLeaf(bool* blew_stack) {
  while (true) {
    // Tree traversal complete.
    if (depth_ == 0) {
      *blew_stack = false;
      return NULL;
    }
    // We've lost track of higher nodes.
    if (StackBlown()) {
      *blew_stack = true;
      return NULL;
    }
    // Go right.
    ConsString* cons_string = frames_[OffsetForDepth(depth_ - 1)];
    String* string = cons_string->second();
    int32_t type = string->map()->instance_type();
    if ((type & kStringRepresentationMask) != kConsStringTag) {
      // Pop stack so next iteration is in correct place.
      Pop();
      int length = string->length();
      // Could be a flattened ConsString.
      if (length == 0) continue;
      consumed_ += length;
      return string;
    }
    cons_string = ConsString::cast(string);
    PushRight(cons_string);
    // Need to traverse all the way left.
    while (true) {
      // Continue left.
      string = cons_string->first();
      type = string->map()->instance_type();
      if ((type & kStringRepresentationMask) != kConsStringTag) {
        AdjustMaximumDepth();
        int length = string->length();
        DCHECK(length != 0);
        consumed_ += length;
        return string;
      }
      cons_string = ConsString::cast(string);
      PushLeft(cons_string);
    }
  }
  UNREACHABLE();
  return NULL;
}


uint16_t ConsString::ConsStringGet(int index) {
  DCHECK(index >= 0 && index < this->length());

  // Check for a flattened cons string
  if (second()->length() == 0) {
    String* left = first();
    return left->Get(index);
  }

  String* string = String::cast(this);

  while (true) {
    if (StringShape(string).IsCons()) {
      ConsString* cons_string = ConsString::cast(string);
      String* left = cons_string->first();
      if (left->length() > index) {
        string = left;
      } else {
        index -= left->length();
        string = cons_string->second();
      }
    } else {
      return string->Get(index);
    }
  }

  UNREACHABLE();
  return 0;
}


uint16_t SlicedString::SlicedStringGet(int index) {
  return parent()->Get(offset() + index);
}


template <typename sinkchar>
void String::WriteToFlat(String* src,
                         sinkchar* sink,
                         int f,
                         int t) {
  String* source = src;
  int from = f;
  int to = t;
  while (true) {
    DCHECK(0 <= from && from <= to && to <= source->length());
    switch (StringShape(source).full_representation_tag()) {
      case kOneByteStringTag | kExternalStringTag: {
        CopyChars(sink, ExternalOneByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kExternalStringTag: {
        const uc16* data =
            ExternalTwoByteString::cast(source)->GetChars();
        CopyChars(sink,
                  data + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqOneByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqTwoByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kOneByteStringTag | kConsStringTag:
      case kTwoByteStringTag | kConsStringTag: {
        ConsString* cons_string = ConsString::cast(source);
        String* first = cons_string->first();
        int boundary = first->length();
        if (to - boundary >= boundary - from) {
          // Right hand side is longer.  Recurse over left.
          if (from < boundary) {
            WriteToFlat(first, sink, from, boundary);
            sink += boundary - from;
            from = 0;
          } else {
            from -= boundary;
          }
          to -= boundary;
          source = cons_string->second();
        } else {
          // Left hand side is longer.  Recurse over right.
          if (to > boundary) {
            String* second = cons_string->second();
            // When repeatedly appending to a string, we get a cons string that
            // is unbalanced to the left, a list, essentially.  We inline the
            // common case of sequential one-byte right child.
            if (to - boundary == 1) {
              sink[boundary - from] = static_cast<sinkchar>(second->Get(0));
            } else if (second->IsSeqOneByteString()) {
              CopyChars(sink + boundary - from,
                        SeqOneByteString::cast(second)->GetChars(),
                        to - boundary);
            } else {
              WriteToFlat(second,
                          sink + boundary - from,
                          0,
                          to - boundary);
            }
            to = boundary;
          }
          source = first;
        }
        break;
      }
      case kOneByteStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString* slice = SlicedString::cast(source);
        unsigned offset = slice->offset();
        WriteToFlat(slice->parent(), sink, from + offset, to + offset);
        return;
      }
    }
  }
}



template <typename SourceChar>
static void CalculateLineEndsImpl(Isolate* isolate,
                                  List<int>* line_ends,
                                  Vector<const SourceChar> src,
                                  bool include_ending_line) {
  const int src_len = src.length();
  UnicodeCache* cache = isolate->unicode_cache();
  for (int i = 0; i < src_len - 1; i++) {
    SourceChar current = src[i];
    SourceChar next = src[i + 1];
    if (cache->IsLineTerminatorSequence(current, next)) line_ends->Add(i);
  }

  if (src_len > 0 && cache->IsLineTerminatorSequence(src[src_len - 1], 0)) {
    line_ends->Add(src_len - 1);
  }
  if (include_ending_line) {
    // Include one character beyond the end of script. The rewriter uses that
    // position for the implicit return statement.
    line_ends->Add(src_len);
  }
}


Handle<FixedArray> String::CalculateLineEnds(Handle<String> src,
                                             bool include_ending_line) {
  src = Flatten(src);
  // Rough estimate of line count based on a roughly estimated average
  // length of (unpacked) code.
  int line_count_estimate = src->length() >> 4;
  List<int> line_ends(line_count_estimate);
  Isolate* isolate = src->GetIsolate();
  { DisallowHeapAllocation no_allocation;  // ensure vectors stay valid.
    // Dispatch on type of strings.
    String::FlatContent content = src->GetFlatContent();
    DCHECK(content.IsFlat());
    if (content.IsOneByte()) {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToOneByteVector(),
                            include_ending_line);
    } else {
      CalculateLineEndsImpl(isolate,
                            &line_ends,
                            content.ToUC16Vector(),
                            include_ending_line);
    }
  }
  int line_count = line_ends.length();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(line_count);
  for (int i = 0; i < line_count; i++) {
    array->set(i, Smi::FromInt(line_ends[i]));
  }
  return array;
}


// Compares the contents of two strings by reading and comparing
// int-sized blocks of characters.
template <typename Char>
static inline bool CompareRawStringContents(const Char* const a,
                                            const Char* const b,
                                            int length) {
  return CompareChars(a, b, length) == 0;
}


template<typename Chars1, typename Chars2>
class RawStringComparator : public AllStatic {
 public:
  static inline bool compare(const Chars1* a, const Chars2* b, int len) {
    DCHECK(sizeof(Chars1) != sizeof(Chars2));
    for (int i = 0; i < len; i++) {
      if (a[i] != b[i]) {
        return false;
      }
    }
    return true;
  }
};


template<>
class RawStringComparator<uint16_t, uint16_t> {
 public:
  static inline bool compare(const uint16_t* a, const uint16_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


template<>
class RawStringComparator<uint8_t, uint8_t> {
 public:
  static inline bool compare(const uint8_t* a, const uint8_t* b, int len) {
    return CompareRawStringContents(a, b, len);
  }
};


class StringComparator {
  class State {
   public:
    State() : is_one_byte_(true), length_(0), buffer8_(NULL) {}

    void Init(String* string) {
      ConsString* cons_string = String::VisitFlat(this, string);
      iter_.Reset(cons_string);
      if (cons_string != NULL) {
        int offset;
        string = iter_.Next(&offset);
        String::VisitFlat(this, string, offset);
      }
    }

    inline void VisitOneByteString(const uint8_t* chars, int length) {
      is_one_byte_ = true;
      buffer8_ = chars;
      length_ = length;
    }

    inline void VisitTwoByteString(const uint16_t* chars, int length) {
      is_one_byte_ = false;
      buffer16_ = chars;
      length_ = length;
    }

    void Advance(int consumed) {
      DCHECK(consumed <= length_);
      // Still in buffer.
      if (length_ != consumed) {
        if (is_one_byte_) {
          buffer8_ += consumed;
        } else {
          buffer16_ += consumed;
        }
        length_ -= consumed;
        return;
      }
      // Advance state.
      int offset;
      String* next = iter_.Next(&offset);
      DCHECK_EQ(0, offset);
      DCHECK(next != NULL);
      String::VisitFlat(this, next);
    }

    ConsStringIterator iter_;
    bool is_one_byte_;
    int length_;
    union {
      const uint8_t* buffer8_;
      const uint16_t* buffer16_;
    };

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

 public:
  inline StringComparator() {}

  template<typename Chars1, typename Chars2>
  static inline bool Equals(State* state_1, State* state_2, int to_check) {
    const Chars1* a = reinterpret_cast<const Chars1*>(state_1->buffer8_);
    const Chars2* b = reinterpret_cast<const Chars2*>(state_2->buffer8_);
    return RawStringComparator<Chars1, Chars2>::compare(a, b, to_check);
  }

  bool Equals(String* string_1, String* string_2) {
    int length = string_1->length();
    state_1_.Init(string_1);
    state_2_.Init(string_2);
    while (true) {
      int to_check = Min(state_1_.length_, state_2_.length_);
      DCHECK(to_check > 0 && to_check <= length);
      bool is_equal;
      if (state_1_.is_one_byte_) {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint8_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint8_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      } else {
        if (state_2_.is_one_byte_) {
          is_equal = Equals<uint16_t, uint8_t>(&state_1_, &state_2_, to_check);
        } else {
          is_equal = Equals<uint16_t, uint16_t>(&state_1_, &state_2_, to_check);
        }
      }
      // Looping done.
      if (!is_equal) return false;
      length -= to_check;
      // Exit condition. Strings are equal.
      if (length == 0) return true;
      state_1_.Advance(to_check);
      state_2_.Advance(to_check);
    }
  }

 private:
  State state_1_;
  State state_2_;

  DISALLOW_COPY_AND_ASSIGN(StringComparator);
};


bool String::SlowEquals(String* other) {
  DisallowHeapAllocation no_gc;
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other->length()) return false;
  if (len == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (Hash() != other->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < len; i++) {
          if (Get(i) != other->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (Hash() != other->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (this->Get(0) != other->Get(0)) return false;

  if (IsSeqOneByteString() && other->IsSeqOneByteString()) {
    const uint8_t* str1 = SeqOneByteString::cast(this)->GetChars();
    const uint8_t* str2 = SeqOneByteString::cast(other)->GetChars();
    return CompareRawStringContents(str1, str2, len);
  }

  StringComparator comparator;
  return comparator.Equals(this, other);
}


bool String::SlowEquals(Handle<String> one, Handle<String> two) {
  // Fast check: negative check with lengths.
  int one_length = one->length();
  if (one_length != two->length()) return false;
  if (one_length == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (one->HasHashCode() && two->HasHashCode()) {
#ifdef ENABLE_SLOW_DCHECKS
    if (FLAG_enable_slow_asserts) {
      if (one->Hash() != two->Hash()) {
        bool found_difference = false;
        for (int i = 0; i < one_length; i++) {
          if (one->Get(i) != two->Get(i)) {
            found_difference = true;
            break;
          }
        }
        DCHECK(found_difference);
      }
    }
#endif
    if (one->Hash() != two->Hash()) return false;
  }

  // We know the strings are both non-empty. Compare the first chars
  // before we try to flatten the strings.
  if (one->Get(0) != two->Get(0)) return false;

  one = String::Flatten(one);
  two = String::Flatten(two);

  DisallowHeapAllocation no_gc;
  String::FlatContent flat1 = one->GetFlatContent();
  String::FlatContent flat2 = two->GetFlatContent();

  if (flat1.IsOneByte() && flat2.IsOneByte()) {
      return CompareRawStringContents(flat1.ToOneByteVector().start(),
                                      flat2.ToOneByteVector().start(),
                                      one_length);
  } else {
    for (int i = 0; i < one_length; i++) {
      if (flat1.Get(i) != flat2.Get(i)) return false;
    }
    return true;
  }
}


// static
ComparisonResult String::Compare(Handle<String> x, Handle<String> y) {
  // A few fast case tests before we flatten.
  if (x.is_identical_to(y)) {
    return ComparisonResult::kEqual;
  } else if (y->length() == 0) {
    return x->length() == 0 ? ComparisonResult::kEqual
                            : ComparisonResult::kGreaterThan;
  } else if (x->length() == 0) {
    return ComparisonResult::kLessThan;
  }

  int const d = x->Get(0) - y->Get(0);
  if (d < 0) {
    return ComparisonResult::kLessThan;
  } else if (d > 0) {
    return ComparisonResult::kGreaterThan;
  }

  // Slow case.
  x = String::Flatten(x);
  y = String::Flatten(y);

  DisallowHeapAllocation no_gc;
  ComparisonResult result = ComparisonResult::kEqual;
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    result = ComparisonResult::kGreaterThan;
  } else if (y->length() > prefix_length) {
    result = ComparisonResult::kLessThan;
  }
  int r;
  String::FlatContent x_content = x->GetFlatContent();
  String::FlatContent y_content = y->GetFlatContent();
  if (x_content.IsOneByte()) {
    Vector<const uint8_t> x_chars = x_content.ToOneByteVector();
    if (y_content.IsOneByte()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  } else {
    Vector<const uc16> x_chars = x_content.ToUC16Vector();
    if (y_content.IsOneByte()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  }
  if (r < 0) {
    result = ComparisonResult::kLessThan;
  } else if (r > 0) {
    result = ComparisonResult::kGreaterThan;
  }
  return result;
}


bool String::IsUtf8EqualTo(Vector<const char> str, bool allow_prefix_match) {
  int slen = length();
  // Can't check exact length equality, but we can check bounds.
  int str_len = str.length();
  if (!allow_prefix_match &&
      (str_len < slen ||
          str_len > slen*static_cast<int>(unibrow::Utf8::kMaxEncodedSize))) {
    return false;
  }
  int i;
  size_t remaining_in_str = static_cast<size_t>(str_len);
  const uint8_t* utf8_data = reinterpret_cast<const uint8_t*>(str.start());
  for (i = 0; i < slen && remaining_in_str > 0; i++) {
    size_t cursor = 0;
    uint32_t r = unibrow::Utf8::ValueOf(utf8_data, remaining_in_str, &cursor);
    DCHECK(cursor > 0 && cursor <= remaining_in_str);
    if (r > unibrow::Utf16::kMaxNonSurrogateCharCode) {
      if (i > slen - 1) return false;
      if (Get(i++) != unibrow::Utf16::LeadSurrogate(r)) return false;
      if (Get(i) != unibrow::Utf16::TrailSurrogate(r)) return false;
    } else {
      if (Get(i) != r) return false;
    }
    utf8_data += cursor;
    remaining_in_str -= cursor;
  }
  return (allow_prefix_match || i == slen) && remaining_in_str == 0;
}


bool String::IsOneByteEqualTo(Vector<const uint8_t> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsOneByte()) {
    return CompareChars(content.ToOneByteVector().start(),
                        str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != static_cast<uint16_t>(str[i])) return false;
  }
  return true;
}


bool String::IsTwoByteEqualTo(Vector<const uc16> str) {
  int slen = length();
  if (str.length() != slen) return false;
  DisallowHeapAllocation no_gc;
  FlatContent content = GetFlatContent();
  if (content.IsTwoByte()) {
    return CompareChars(content.ToUC16Vector().start(), str.start(), slen) == 0;
  }
  for (int i = 0; i < slen; i++) {
    if (Get(i) != str[i]) return false;
  }
  return true;
}


uint32_t String::ComputeAndSetHash() {
  // Should only be called if hash code has not yet been computed.
  DCHECK(!HasHashCode());

  // Store the hash code in the object.
  uint32_t field = IteratingStringHasher::Hash(this, GetHeap()->HashSeed());
  set_hash_field(field);

  // Check the hash code is there.
  DCHECK(HasHashCode());
  uint32_t result = field >> kHashShift;
  DCHECK(result != 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}


bool String::ComputeArrayIndex(uint32_t* index) {
  int length = this->length();
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  StringCharacterStream stream(this);
  return StringToArrayIndex(&stream, index);
}


bool String::SlowAsArrayIndex(uint32_t* index) {
  if (length() <= kMaxCachedArrayIndexLength) {
    Hash();  // force computation of hash code
    uint32_t field = hash_field();
    if ((field & kIsNotArrayIndexMask) != 0) return false;
    // Isolate the array index form the full hash field.
    *index = ArrayIndexValueBits::decode(field);
    return true;
  } else {
    return ComputeArrayIndex(index);
  }
}


Handle<String> SeqString::Truncate(Handle<SeqString> string, int new_length) {
  int new_size, old_size;
  int old_length = string->length();
  if (old_length <= new_length) return string;

  if (string->IsSeqOneByteString()) {
    old_size = SeqOneByteString::SizeFor(old_length);
    new_size = SeqOneByteString::SizeFor(new_length);
  } else {
    DCHECK(string->IsSeqTwoByteString());
    old_size = SeqTwoByteString::SizeFor(old_length);
    new_size = SeqTwoByteString::SizeFor(new_length);
  }

  int delta = old_size - new_size;

  Address start_of_string = string->address();
  DCHECK_OBJECT_ALIGNED(start_of_string);
  DCHECK_OBJECT_ALIGNED(start_of_string + new_size);

  Heap* heap = string->GetHeap();
  // Sizes are pointer size aligned, so that we can use filler objects
  // that are a multiple of pointer size.
  heap->CreateFillerObjectAt(start_of_string + new_size, delta);
  heap->AdjustLiveBytes(*string, -delta, Heap::CONCURRENT_TO_SWEEPER);

  // We are storing the new length using release store after creating a filler
  // for the left-over space to avoid races with the sweeper thread.
  string->synchronized_set_length(new_length);

  if (new_length == 0) return heap->isolate()->factory()->empty_string();
  return string;
}


uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, int length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  DCHECK(length > 0);
  DCHECK(length <= String::kMaxArrayIndexSize);
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::ArrayIndexValueBits::kShift;
  value |= length << String::ArrayIndexLengthBits::kShift;

  DCHECK((value & String::kIsNotArrayIndexMask) == 0);
  DCHECK((length > String::kMaxCachedArrayIndexLength) ||
         (value & String::kContainsCachedArrayIndexMask) == 0);
  return value;
}


uint32_t StringHasher::GetHashField() {
  if (length_ <= String::kMaxHashCalcLength) {
    if (is_array_index_) {
      return MakeArrayIndexHash(array_index_, length_);
    }
    return (GetHashCore(raw_running_hash_) << String::kHashShift) |
           String::kIsNotArrayIndexMask;
  } else {
    return (length_ << String::kHashShift) | String::kIsNotArrayIndexMask;
  }
}


uint32_t StringHasher::ComputeUtf8Hash(Vector<const char> chars,
                                       uint32_t seed,
                                       int* utf16_length_out) {
  int vector_length = chars.length();
  // Handle some edge cases
  if (vector_length <= 1) {
    DCHECK(vector_length == 0 ||
           static_cast<uint8_t>(chars.start()[0]) <=
               unibrow::Utf8::kMaxOneByteChar);
    *utf16_length_out = vector_length;
    return HashSequentialString(chars.start(), vector_length, seed);
  }
  // Start with a fake length which won't affect computation.
  // It will be updated later.
  StringHasher hasher(String::kMaxArrayIndexSize, seed);
  size_t remaining = static_cast<size_t>(vector_length);
  const uint8_t* stream = reinterpret_cast<const uint8_t*>(chars.start());
  int utf16_length = 0;
  bool is_index = true;
  DCHECK(hasher.is_array_index_);
  while (remaining > 0) {
    size_t consumed = 0;
    uint32_t c = unibrow::Utf8::ValueOf(stream, remaining, &consumed);
    DCHECK(consumed > 0 && consumed <= remaining);
    stream += consumed;
    remaining -= consumed;
    bool is_two_characters = c > unibrow::Utf16::kMaxNonSurrogateCharCode;
    utf16_length += is_two_characters ? 2 : 1;
    // No need to keep hashing. But we do need to calculate utf16_length.
    if (utf16_length > String::kMaxHashCalcLength) continue;
    if (is_two_characters) {
      uint16_t c1 = unibrow::Utf16::LeadSurrogate(c);
      uint16_t c2 = unibrow::Utf16::TrailSurrogate(c);
      hasher.AddCharacter(c1);
      hasher.AddCharacter(c2);
      if (is_index) is_index = hasher.UpdateIndex(c1);
      if (is_index) is_index = hasher.UpdateIndex(c2);
    } else {
      hasher.AddCharacter(c);
      if (is_index) is_index = hasher.UpdateIndex(c);
    }
  }
  *utf16_length_out = static_cast<int>(utf16_length);
  // Must set length here so that hash computation is correct.
  hasher.length_ = utf16_length;
  return hasher.GetHashField();
}


void IteratingStringHasher::VisitConsString(ConsString* cons_string) {
  // Run small ConsStrings through ConsStringIterator.
  if (cons_string->length() < 64) {
    ConsStringIterator iter(cons_string);
    int offset;
    String* string;
    while (nullptr != (string = iter.Next(&offset))) {
      DCHECK_EQ(0, offset);
      String::VisitFlat(this, string, 0);
    }
    return;
  }
  // Slow case.
  const int max_length = String::kMaxHashCalcLength;
  int length = std::min(cons_string->length(), max_length);
  if (cons_string->HasOnlyOneByteChars()) {
    uint8_t* buffer = new uint8_t[length];
    String::WriteToFlat(cons_string, buffer, 0, length);
    AddCharacters(buffer, length);
    delete[] buffer;
  } else {
    uint16_t* buffer = new uint16_t[length];
    String::WriteToFlat(cons_string, buffer, 0, length);
    AddCharacters(buffer, length);
    delete[] buffer;
  }
}


void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    PrintF(file, "%c", Get(i));
  }
}


int Map::Hash() {
  // For performance reasons we only hash the 3 most variable fields of a map:
  // constructor, prototype and bit_field2. For predictability reasons we
  // use objects' offsets in respective pages for hashing instead of raw
  // addresses.

  // Shift away the tag.
  int hash = ObjectAddressForHashing(GetConstructor()) >> 2;

  // XOR-ing the prototype and constructor directly yields too many zero bits
  // when the two pointers are close (which is fairly common).
  // To avoid this we shift the prototype bits relatively to the constructor.
  hash ^= ObjectAddressForHashing(prototype()) << (32 - kPageSizeBits);

  return hash ^ (hash >> 16) ^ bit_field2();
}


namespace {

bool CheckEquivalent(Map* first, Map* second) {
  return first->GetConstructor() == second->GetConstructor() &&
         first->prototype() == second->prototype() &&
         first->instance_type() == second->instance_type() &&
         first->bit_field() == second->bit_field() &&
         first->is_extensible() == second->is_extensible() &&
         first->is_strong() == second->is_strong() &&
         first->new_target_is_base() == second->new_target_is_base() &&
         first->has_hidden_prototype() == second->has_hidden_prototype();
}

}  // namespace


bool Map::EquivalentToForTransition(Map* other) {
  if (!CheckEquivalent(this, other)) return false;
  if (instance_type() == JS_FUNCTION_TYPE) {
    // JSFunctions require more checks to ensure that sloppy function is
    // not equvalent to strict function.
    int nof = Min(NumberOfOwnDescriptors(), other->NumberOfOwnDescriptors());
    return instance_descriptors()->IsEqualUpTo(other->instance_descriptors(),
                                               nof);
  }
  return true;
}


bool Map::EquivalentToForNormalization(Map* other,
                                       PropertyNormalizationMode mode) {
  int properties =
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : other->GetInObjectProperties();
  return CheckEquivalent(this, other) && bit_field2() == other->bit_field2() &&
         GetInObjectProperties() == properties;
}


bool JSFunction::Inlines(SharedFunctionInfo* candidate) {
  DisallowHeapAllocation no_gc;
  if (shared() == candidate) return true;
  if (code()->kind() != Code::OPTIMIZED_FUNCTION) return false;
  DeoptimizationInputData* const data =
      DeoptimizationInputData::cast(code()->deoptimization_data());
  if (data->length() == 0) return false;
  FixedArray* const literals = data->LiteralArray();
  int const inlined_count = data->InlinedFunctionCount()->value();
  for (int i = 0; i < inlined_count; ++i) {
    if (SharedFunctionInfo::cast(literals->get(i)) == candidate) {
      return true;
    }
  }
  return false;
}


void JSFunction::MarkForOptimization() {
  Isolate* isolate = GetIsolate();
  // Do not optimize if function contains break points.
  if (shared()->HasDebugInfo()) return;
  DCHECK(!IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() ||
         !shared()->optimization_disabled());
  DCHECK(!shared()->HasDebugInfo());
  set_code_no_write_barrier(
      isolate->builtins()->builtin(Builtins::kCompileOptimized));
  // No write barrier required, since the builtin is part of the root set.
}


void JSFunction::AttemptConcurrentOptimization() {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    MarkForOptimization();
    return;
  }
  if (isolate->concurrent_osr_enabled() &&
      isolate->optimizing_compile_dispatcher()->IsQueuedForOSR(this)) {
    // Do not attempt regular recompilation if we already queued this for OSR.
    // TODO(yangguo): This is necessary so that we don't install optimized
    // code on a function that is already optimized, since OSR and regular
    // recompilation race.  This goes away as soon as OSR becomes one-shot.
    return;
  }
  DCHECK(!IsInOptimizationQueue());
  DCHECK(!IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() ||
         !shared()->optimization_disabled());
  DCHECK(isolate->concurrent_recompilation_enabled());
  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Marking ");
    ShortPrint();
    PrintF(" for concurrent recompilation.\n");
  }
  set_code_no_write_barrier(
      isolate->builtins()->builtin(Builtins::kCompileOptimizedConcurrent));
  // No write barrier required, since the builtin is part of the root set.
}


void SharedFunctionInfo::AddSharedCodeToOptimizedCodeMap(
    Handle<SharedFunctionInfo> shared, Handle<Code> code) {
  Isolate* isolate = shared->GetIsolate();
  if (isolate->serializer_enabled()) return;
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
  // Empty code maps are unsupported.
  if (!shared->OptimizedCodeMapIsCleared()) {
    Handle<WeakCell> cell = isolate->factory()->NewWeakCell(code);
    // A collection may have occured and cleared the optimized code map in the
    // allocation above.
    if (!shared->OptimizedCodeMapIsCleared()) {
      shared->optimized_code_map()->set(kSharedCodeIndex, *cell);
    }
  }
}


void SharedFunctionInfo::AddToOptimizedCodeMapInternal(
    Handle<SharedFunctionInfo> shared, Handle<Context> native_context,
    Handle<HeapObject> code, Handle<LiteralsArray> literals,
    BailoutId osr_ast_id) {
  Isolate* isolate = shared->GetIsolate();
  if (isolate->serializer_enabled()) return;
  DCHECK(*code == isolate->heap()->undefined_value() ||
         !shared->SearchOptimizedCodeMap(*native_context, osr_ast_id).code);
  DCHECK(*code == isolate->heap()->undefined_value() ||
         Code::cast(*code)->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kEntryLength == 4);
  Handle<FixedArray> new_code_map;
  int entry;

  if (shared->OptimizedCodeMapIsCleared()) {
    new_code_map = isolate->factory()->NewFixedArray(kInitialLength, TENURED);
    new_code_map->set(kSharedCodeIndex, *isolate->factory()->empty_weak_cell(),
                      SKIP_WRITE_BARRIER);
    entry = kEntriesStart;
  } else {
    Handle<FixedArray> old_code_map(shared->optimized_code_map(), isolate);
    entry = shared->SearchOptimizedCodeMapEntry(*native_context, osr_ast_id);
    if (entry > kSharedCodeIndex) {
      // Found an existing context-specific entry. If the user provided valid
      // code, it must not contain any code.
      DCHECK(code->IsUndefined() ||
             WeakCell::cast(old_code_map->get(entry + kCachedCodeOffset))
                 ->cleared());

      // Just set the code and literals to the entry.
      if (!code->IsUndefined()) {
        Handle<WeakCell> code_cell = isolate->factory()->NewWeakCell(code);
        old_code_map->set(entry + kCachedCodeOffset, *code_cell);
      }
      Handle<WeakCell> literals_cell =
          isolate->factory()->NewWeakCell(literals);
      old_code_map->set(entry + kLiteralsOffset, *literals_cell);
      return;
    }

    // Can we reuse an entry?
    DCHECK(entry < kEntriesStart);
    int length = old_code_map->length();
    for (int i = kEntriesStart; i < length; i += kEntryLength) {
      if (WeakCell::cast(old_code_map->get(i + kContextOffset))->cleared()) {
        new_code_map = old_code_map;
        entry = i;
        break;
      }
    }

    if (entry < kEntriesStart) {
      // Copy old optimized code map and append one new entry.
      new_code_map = isolate->factory()->CopyFixedArrayAndGrow(
          old_code_map, kEntryLength, TENURED);
      // TODO(mstarzinger): Temporary workaround. The allocation above might
      // have flushed the optimized code map and the copy we created is full of
      // holes. For now we just give up on adding the entry and pretend it got
      // flushed.
      if (shared->OptimizedCodeMapIsCleared()) return;
      entry = old_code_map->length();
    }
  }

  Handle<WeakCell> code_cell = code->IsUndefined()
                                   ? isolate->factory()->empty_weak_cell()
                                   : isolate->factory()->NewWeakCell(code);
  Handle<WeakCell> literals_cell = isolate->factory()->NewWeakCell(literals);
  WeakCell* context_cell = native_context->self_weak_cell();

  new_code_map->set(entry + kContextOffset, context_cell);
  new_code_map->set(entry + kCachedCodeOffset, *code_cell);
  new_code_map->set(entry + kLiteralsOffset, *literals_cell);
  new_code_map->set(entry + kOsrAstIdOffset, Smi::FromInt(osr_ast_id.ToInt()));

#ifdef DEBUG
  for (int i = kEntriesStart; i < new_code_map->length(); i += kEntryLength) {
    WeakCell* cell = WeakCell::cast(new_code_map->get(i + kContextOffset));
    DCHECK(cell->cleared() || cell->value()->IsNativeContext());
    cell = WeakCell::cast(new_code_map->get(i + kCachedCodeOffset));
    DCHECK(cell->cleared() ||
           (cell->value()->IsCode() &&
            Code::cast(cell->value())->kind() == Code::OPTIMIZED_FUNCTION));
    cell = WeakCell::cast(new_code_map->get(i + kLiteralsOffset));
    DCHECK(cell->cleared() || cell->value()->IsFixedArray());
    DCHECK(new_code_map->get(i + kOsrAstIdOffset)->IsSmi());
  }
#endif

  FixedArray* old_code_map = shared->optimized_code_map();
  if (old_code_map != *new_code_map) {
    shared->set_optimized_code_map(*new_code_map);
  }
}


void SharedFunctionInfo::ClearOptimizedCodeMap() {
  FixedArray* cleared_map = GetHeap()->cleared_optimized_code_map();
  set_optimized_code_map(cleared_map, SKIP_WRITE_BARRIER);
}


void SharedFunctionInfo::EvictFromOptimizedCodeMap(Code* optimized_code,
                                                   const char* reason) {
  DisallowHeapAllocation no_gc;
  if (OptimizedCodeMapIsCleared()) return;

  Heap* heap = GetHeap();
  FixedArray* code_map = optimized_code_map();
  int dst = kEntriesStart;
  int length = code_map->length();
  for (int src = kEntriesStart; src < length; src += kEntryLength) {
    DCHECK(WeakCell::cast(code_map->get(src))->cleared() ||
           WeakCell::cast(code_map->get(src))->value()->IsNativeContext());
    if (WeakCell::cast(code_map->get(src + kCachedCodeOffset))->value() ==
        optimized_code) {
      BailoutId osr(Smi::cast(code_map->get(src + kOsrAstIdOffset))->value());
      if (FLAG_trace_opt) {
        PrintF("[evicting entry from optimizing code map (%s) for ", reason);
        ShortPrint();
        if (osr.IsNone()) {
          PrintF("]\n");
        } else {
          PrintF(" (osr ast id %d)]\n", osr.ToInt());
        }
      }
      if (!osr.IsNone()) {
        // Evict the src entry by not copying it to the dst entry.
        continue;
      }
      // In case of non-OSR entry just clear the code in order to proceed
      // sharing literals.
      code_map->set(src + kCachedCodeOffset, heap->empty_weak_cell(),
                    SKIP_WRITE_BARRIER);
    }

    // Keep the src entry by copying it to the dst entry.
    if (dst != src) {
      code_map->set(dst + kContextOffset, code_map->get(src + kContextOffset));
      code_map->set(dst + kCachedCodeOffset,
                    code_map->get(src + kCachedCodeOffset));
      code_map->set(dst + kLiteralsOffset,
                    code_map->get(src + kLiteralsOffset));
      code_map->set(dst + kOsrAstIdOffset,
                    code_map->get(src + kOsrAstIdOffset));
    }
    dst += kEntryLength;
  }
  if (WeakCell::cast(code_map->get(kSharedCodeIndex))->value() ==
      optimized_code) {
    // Evict context-independent code as well.
    code_map->set(kSharedCodeIndex, heap->empty_weak_cell(),
                  SKIP_WRITE_BARRIER);
    if (FLAG_trace_opt) {
      PrintF("[evicting entry from optimizing code map (%s) for ", reason);
      ShortPrint();
      PrintF(" (context-independent code)]\n");
    }
  }
  if (dst != length) {
    // Always trim even when array is cleared because of heap verifier.
    heap->RightTrimFixedArray<Heap::CONCURRENT_TO_SWEEPER>(code_map,
                                                           length - dst);
    if (code_map->length() == kEntriesStart &&
        WeakCell::cast(code_map->get(kSharedCodeIndex))->cleared()) {
      ClearOptimizedCodeMap();
    }
  }
}


void SharedFunctionInfo::TrimOptimizedCodeMap(int shrink_by) {
  FixedArray* code_map = optimized_code_map();
  DCHECK(shrink_by % kEntryLength == 0);
  DCHECK(shrink_by <= code_map->length() - kEntriesStart);
  // Always trim even when array is cleared because of heap verifier.
  GetHeap()->RightTrimFixedArray<Heap::SEQUENTIAL_TO_SWEEPER>(code_map,
                                                              shrink_by);
  if (code_map->length() == kEntriesStart &&
      WeakCell::cast(code_map->get(kSharedCodeIndex))->cleared()) {
    ClearOptimizedCodeMap();
  }
}


static void GetMinInobjectSlack(Map* map, void* data) {
  int slack = map->unused_property_fields();
  if (*reinterpret_cast<int*>(data) > slack) {
    *reinterpret_cast<int*>(data) = slack;
  }
}


static void ShrinkInstanceSize(Map* map, void* data) {
  int slack = *reinterpret_cast<int*>(data);
  map->SetInObjectProperties(map->GetInObjectProperties() - slack);
  map->set_unused_property_fields(map->unused_property_fields() - slack);
  map->set_instance_size(map->instance_size() - slack * kPointerSize);
  map->set_construction_counter(Map::kNoSlackTracking);

  // Visitor id might depend on the instance size, recalculate it.
  map->set_visitor_id(Heap::GetStaticVisitorIdForMap(map));
}

static void StopSlackTracking(Map* map, void* data) {
  map->set_construction_counter(Map::kNoSlackTracking);
}

void Map::CompleteInobjectSlackTracking() {
  // Has to be an initial map.
  DCHECK(GetBackPointer()->IsUndefined());

  int slack = unused_property_fields();
  TransitionArray::TraverseTransitionTree(this, &GetMinInobjectSlack, &slack);
  if (slack != 0) {
    // Resize the initial map and all maps in its transition tree.
    TransitionArray::TraverseTransitionTree(this, &ShrinkInstanceSize, &slack);
  } else {
    TransitionArray::TraverseTransitionTree(this, &StopSlackTracking, nullptr);
  }
}


static bool PrototypeBenefitsFromNormalization(Handle<JSObject> object) {
  DisallowHeapAllocation no_gc;
  if (!object->HasFastProperties()) return false;
  Map* map = object->map();
  if (map->is_prototype_map()) return false;
  DescriptorArray* descriptors = map->instance_descriptors();
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() == kDescriptor) continue;
    if (details.representation().IsHeapObject() ||
        details.representation().IsTagged()) {
      FieldIndex index = FieldIndex::ForDescriptor(map, i);
      if (object->RawFastPropertyAt(index)->IsJSFunction()) return true;
    }
  }
  return false;
}


// static
void JSObject::OptimizeAsPrototype(Handle<JSObject> object,
                                   PrototypeOptimizationMode mode) {
  if (object->IsJSGlobalObject()) return;
  if (mode == FAST_PROTOTYPE && PrototypeBenefitsFromNormalization(object)) {
    // First normalize to ensure all JSFunctions are DATA_CONSTANT.
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, 0,
                                  "NormalizeAsPrototype");
  }
  Handle<Map> previous_map(object->map());
  if (!object->HasFastProperties()) {
    JSObject::MigrateSlowToFast(object, 0, "OptimizeAsPrototype");
  }
  if (!object->map()->is_prototype_map()) {
    if (object->map() == *previous_map) {
      Handle<Map> new_map = Map::Copy(handle(object->map()), "CopyAsPrototype");
      JSObject::MigrateToMap(object, new_map);
    }
    object->map()->set_is_prototype_map(true);

    // Replace the pointer to the exact constructor with the Object function
    // from the same context if undetectable from JS. This is to avoid keeping
    // memory alive unnecessarily.
    Object* maybe_constructor = object->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      Isolate* isolate = object->GetIsolate();
      if (!constructor->shared()->IsApiFunction() &&
          object->class_name() == isolate->heap()->Object_string()) {
        Context* context = constructor->context()->native_context();
        JSFunction* object_function = context->object_function();
        object->map()->SetConstructor(object_function);
      }
    }
  }
}


// static
void JSObject::ReoptimizeIfPrototype(Handle<JSObject> object) {
  if (!object->map()->is_prototype_map()) return;
  OptimizeAsPrototype(object, FAST_PROTOTYPE);
}


// static
void JSObject::LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  DCHECK(FLAG_track_prototype_users);
  // Contract: In line with InvalidatePrototypeChains()'s requirements,
  // leaf maps don't need to register as users, only prototypes do.
  DCHECK(user->is_prototype_map());

  Handle<Map> current_user = user;
  Handle<PrototypeInfo> current_user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  for (PrototypeIterator iter(user); !iter.IsAtEnd(); iter.Advance()) {
    // Walk up the prototype chain as far as links haven't been registered yet.
    if (current_user_info->registry_slot() != PrototypeInfo::UNREGISTERED) {
      break;
    }
    Handle<Object> maybe_proto = PrototypeIterator::GetCurrent(iter);
    // Proxies on the prototype chain are not supported. They make it
    // impossible to make any assumptions about the prototype chain anyway.
    if (maybe_proto->IsJSProxy()) return;
    Handle<JSObject> proto = Handle<JSObject>::cast(maybe_proto);
    Handle<PrototypeInfo> proto_info =
        Map::GetOrCreatePrototypeInfo(proto, isolate);
    Handle<Object> maybe_registry(proto_info->prototype_users(), isolate);
    int slot = 0;
    Handle<WeakFixedArray> new_array =
        WeakFixedArray::Add(maybe_registry, current_user, &slot);
    current_user_info->set_registry_slot(slot);
    if (!maybe_registry.is_identical_to(new_array)) {
      proto_info->set_prototype_users(*new_array);
    }
    if (FLAG_trace_prototype_users) {
      PrintF("Registering %p as a user of prototype %p (map=%p).\n",
             reinterpret_cast<void*>(*current_user),
             reinterpret_cast<void*>(*proto),
             reinterpret_cast<void*>(proto->map()));
    }

    current_user = handle(proto->map(), isolate);
    current_user_info = proto_info;
  }
}


// Can be called regardless of whether |user| was actually registered with
// |prototype|. Returns true when there was a registration.
// static
bool JSObject::UnregisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
  DCHECK(user->is_prototype_map());
  // If it doesn't have a PrototypeInfo, it was never registered.
  if (!user->prototype_info()->IsPrototypeInfo()) return false;
  // If it had no prototype before, see if it had users that might expect
  // registration.
  if (!user->prototype()->IsJSObject()) {
    Object* users =
        PrototypeInfo::cast(user->prototype_info())->prototype_users();
    return users->IsWeakFixedArray();
  }
  Handle<JSObject> prototype(JSObject::cast(user->prototype()), isolate);
  Handle<PrototypeInfo> user_info =
      Map::GetOrCreatePrototypeInfo(user, isolate);
  int slot = user_info->registry_slot();
  if (slot == PrototypeInfo::UNREGISTERED) return false;
  DCHECK(prototype->map()->is_prototype_map());
  Object* maybe_proto_info = prototype->map()->prototype_info();
  // User knows its registry slot, prototype info and user registry must exist.
  DCHECK(maybe_proto_info->IsPrototypeInfo());
  Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(maybe_proto_info),
                                   isolate);
  Object* maybe_registry = proto_info->prototype_users();
  DCHECK(maybe_registry->IsWeakFixedArray());
  DCHECK(WeakFixedArray::cast(maybe_registry)->Get(slot) == *user);
  WeakFixedArray::cast(maybe_registry)->Clear(slot);
  if (FLAG_trace_prototype_users) {
    PrintF("Unregistering %p as a user of prototype %p.\n",
           reinterpret_cast<void*>(*user), reinterpret_cast<void*>(*prototype));
  }
  return true;
}


static void InvalidatePrototypeChainsInternal(Map* map) {
  if (!map->is_prototype_map()) return;
  if (FLAG_trace_prototype_users) {
    PrintF("Invalidating prototype map %p 's cell\n",
           reinterpret_cast<void*>(map));
  }
  Object* maybe_proto_info = map->prototype_info();
  if (!maybe_proto_info->IsPrototypeInfo()) return;
  PrototypeInfo* proto_info = PrototypeInfo::cast(maybe_proto_info);
  Object* maybe_cell = proto_info->validity_cell();
  if (maybe_cell->IsCell()) {
    // Just set the value; the cell will be replaced lazily.
    Cell* cell = Cell::cast(maybe_cell);
    cell->set_value(Smi::FromInt(Map::kPrototypeChainInvalid));
  }

  WeakFixedArray::Iterator iterator(proto_info->prototype_users());
  // For now, only maps register themselves as users.
  Map* user;
  while ((user = iterator.Next<Map>())) {
    // Walk the prototype chain (backwards, towards leaf objects) if necessary.
    InvalidatePrototypeChainsInternal(user);
  }
}


// static
void JSObject::InvalidatePrototypeChains(Map* map) {
  if (!FLAG_eliminate_prototype_chain_checks) return;
  DisallowHeapAllocation no_gc;
  InvalidatePrototypeChainsInternal(map);
}


// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<JSObject> prototype,
                                                    Isolate* isolate) {
  Object* maybe_proto_info = prototype->map()->prototype_info();
  if (maybe_proto_info->IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype->map()->set_prototype_info(*proto_info);
  return proto_info;
}


// static
Handle<PrototypeInfo> Map::GetOrCreatePrototypeInfo(Handle<Map> prototype_map,
                                                    Isolate* isolate) {
  Object* maybe_proto_info = prototype_map->prototype_info();
  if (maybe_proto_info->IsPrototypeInfo()) {
    return handle(PrototypeInfo::cast(maybe_proto_info), isolate);
  }
  Handle<PrototypeInfo> proto_info = isolate->factory()->NewPrototypeInfo();
  prototype_map->set_prototype_info(*proto_info);
  return proto_info;
}


// static
Handle<Cell> Map::GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                        Isolate* isolate) {
  Handle<Object> maybe_prototype(map->prototype(), isolate);
  if (!maybe_prototype->IsJSObject()) return Handle<Cell>::null();
  Handle<JSObject> prototype = Handle<JSObject>::cast(maybe_prototype);
  // Ensure the prototype is registered with its own prototypes so its cell
  // will be invalidated when necessary.
  JSObject::LazyRegisterPrototypeUser(handle(prototype->map(), isolate),
                                      isolate);
  Handle<PrototypeInfo> proto_info =
      GetOrCreatePrototypeInfo(prototype, isolate);
  Object* maybe_cell = proto_info->validity_cell();
  // Return existing cell if it's still valid.
  if (maybe_cell->IsCell()) {
    Handle<Cell> cell(Cell::cast(maybe_cell), isolate);
    if (cell->value() == Smi::FromInt(Map::kPrototypeChainValid)) {
      return cell;
    }
  }
  // Otherwise create a new cell.
  Handle<Cell> cell = isolate->factory()->NewCell(
      handle(Smi::FromInt(Map::kPrototypeChainValid), isolate));
  proto_info->set_validity_cell(*cell);
  return cell;
}


// static
void Map::SetPrototype(Handle<Map> map, Handle<Object> prototype,
                       PrototypeOptimizationMode proto_mode) {
  bool is_hidden = false;
  if (prototype->IsJSObject()) {
    Handle<JSObject> prototype_jsobj = Handle<JSObject>::cast(prototype);
    JSObject::OptimizeAsPrototype(prototype_jsobj, proto_mode);

    Object* maybe_constructor = prototype_jsobj->map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      Object* data = constructor->shared()->function_data();
      is_hidden = (data->IsFunctionTemplateInfo() &&
                   FunctionTemplateInfo::cast(data)->hidden_prototype()) ||
                  prototype->IsJSGlobalObject();
    }
  }
  map->set_has_hidden_prototype(is_hidden);

  WriteBarrierMode wb_mode =
      prototype->IsNull() ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  map->set_prototype(*prototype, wb_mode);
}


Handle<Object> CacheInitialJSArrayMaps(
    Handle<Context> native_context, Handle<Map> initial_map) {
  // Replace all of the cached initial array maps in the native context with
  // the appropriate transitioned elements kind maps.
  Strength strength =
      initial_map->is_strong() ? Strength::STRONG : Strength::WEAK;
  Handle<Map> current_map = initial_map;
  ElementsKind kind = current_map->elements_kind();
  DCHECK_EQ(GetInitialFastElementsKind(), kind);
  native_context->set(Context::ArrayMapIndex(kind, strength), *current_map);
  for (int i = GetSequenceIndexFromFastElementsKind(kind) + 1;
       i < kFastElementsKindCount; ++i) {
    Handle<Map> new_map;
    ElementsKind next_kind = GetFastElementsKindFromSequenceIndex(i);
    if (Map* maybe_elements_transition = current_map->ElementsTransitionMap()) {
      new_map = handle(maybe_elements_transition);
    } else {
      new_map = Map::CopyAsElementsKind(
          current_map, next_kind, INSERT_TRANSITION);
    }
    DCHECK_EQ(next_kind, new_map->elements_kind());
    native_context->set(Context::ArrayMapIndex(next_kind, strength), *new_map);
    current_map = new_map;
  }
  return initial_map;
}


void JSFunction::SetInstancePrototype(Handle<JSFunction> function,
                                      Handle<Object> value) {
  Isolate* isolate = function->GetIsolate();

  DCHECK(value->IsJSReceiver());

  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    function->CompleteInobjectSlackTrackingIfActive();

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!initial_map->GetIsolate()->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value);
    } else {
      Handle<Map> new_map = Map::Copy(initial_map, "SetInstancePrototype");
      if (function->map()->is_strong()) {
        new_map->set_is_strong();
      }
      JSFunction::SetInitialMap(function, new_map, value);

      // If the function is used as the global Array function, cache the
      // updated initial maps (and transitioned versions) in the native context.
      Handle<Context> native_context(function->context()->native_context(),
                                     isolate);
      Handle<Object> array_function(
          native_context->get(Context::ARRAY_FUNCTION_INDEX), isolate);
      if (array_function->IsJSFunction() &&
          *function == JSFunction::cast(*array_function)) {
        CacheInitialJSArrayMaps(native_context, new_map);
        Handle<Map> new_strong_map = Map::Copy(new_map, "SetInstancePrototype");
        new_strong_map->set_is_strong();
        CacheInitialJSArrayMaps(native_context, new_strong_map);
      }
    }

    // Deoptimize all code that embeds the previous initial map.
    initial_map->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
    if (value->IsJSObject()) {
      // Optimize as prototype to detach it from its transition tree.
      JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value),
                                    FAST_PROTOTYPE);
    }
  }
  isolate->heap()->ClearInstanceofCache();
}


void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared()->kind()));
  Handle<Object> construct_prototype = value;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map = Map::Copy(handle(function->map()), "SetPrototype");

    JSObject::MigrateToMap(function, new_map);
    new_map->SetConstructor(*value);
    new_map->set_non_instance_prototype(true);
    Isolate* isolate = new_map->GetIsolate();
    construct_prototype = handle(
        function->context()->native_context()->initial_object_prototype(),
        isolate);
  } else {
    function->map()->set_non_instance_prototype(false);
  }

  return SetInstancePrototype(function, construct_prototype);
}


bool JSFunction::RemovePrototype() {
  Context* native_context = context()->native_context();
  Map* no_prototype_map =
      is_strict(shared()->language_mode())
          ? native_context->strict_function_without_prototype_map()
          : native_context->sloppy_function_without_prototype_map();

  if (map() == no_prototype_map) return true;

#ifdef DEBUG
  if (map() != (is_strict(shared()->language_mode())
                    ? native_context->strict_function_map()
                    : native_context->sloppy_function_map())) {
    return false;
  }
#endif

  set_map(no_prototype_map);
  set_prototype_or_initial_map(no_prototype_map->GetHeap()->the_hole_value());
  return true;
}


void JSFunction::SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                               Handle<Object> prototype) {
  if (map->prototype() != *prototype) {
    Map::SetPrototype(map, prototype, FAST_PROTOTYPE);
  }
  function->set_prototype_or_initial_map(*map);
  map->SetConstructor(*function);
#if TRACE_MAPS
  if (FLAG_trace_maps) {
    PrintF("[TraceMaps: InitialMap map= %p SFI= %d_%s ]\n",
           reinterpret_cast<void*>(*map), function->shared()->unique_id(),
           function->shared()->DebugName()->ToCString().get());
  }
#endif
}


#ifdef DEBUG
namespace {

bool CanSubclassHaveInobjectProperties(InstanceType instance_type) {
  switch (instance_type) {
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_MODULE_TYPE:
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REGEXP_TYPE:
    case JS_FUNCTION_TYPE:
      return true;

    case JS_BOUND_FUNCTION_TYPE:
    case JS_PROXY_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case FIXED_ARRAY_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case ODDBALL_TYPE:
    case FOREIGN_TYPE:
    case MAP_TYPE:
    case CODE_TYPE:
    case CELL_TYPE:
    case PROPERTY_CELL_TYPE:
    case WEAK_CELL_TYPE:
    case SYMBOL_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case SIMD128_VALUE_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case FREE_SPACE_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
#undef TYPED_ARRAY_CASE

#define MAKE_STRUCT_CASE(NAME, Name, name) case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      // We must not end up here for these instance types at all.
      UNREACHABLE();
    // Fall through.
    default:
      return false;
  }
}

}  // namespace
#endif


void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  DCHECK(function->IsConstructor() || function->shared()->is_generator());
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  // The constructor should be compiled for the optimization hints to be
  // available.
  Compiler::Compile(function, CLEAR_EXCEPTION);

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  InstanceType instance_type;
  if (function->shared()->is_generator()) {
    instance_type = JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }
  int instance_size;
  int in_object_properties;
  function->CalculateInstanceSize(instance_type, 0, &instance_size,
                                  &in_object_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size);
  if (function->map()->is_strong()) {
    map->set_is_strong();
  }

  // Fetch or allocate prototype.
  Handle<Object> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  map->SetInObjectProperties(in_object_properties);
  map->set_unused_property_fields(in_object_properties);
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  DCHECK(prototype->IsJSReceiver());
  JSFunction::SetInitialMap(function, map, prototype);
  map->StartInobjectSlackTracking();
}


// static
MaybeHandle<Map> JSFunction::GetDerivedMap(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<JSReceiver> new_target) {
  EnsureHasInitialMap(constructor);

  Handle<Map> constructor_initial_map(constructor->initial_map(), isolate);
  if (*new_target == *constructor) return constructor_initial_map;

  // Fast case, new.target is a subclass of constructor. The map is cacheable
  // (and may already have been cached). new.target.prototype is guaranteed to
  // be a JSReceiver.
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);

    // Check that |function|'s initial map still in sync with the |constructor|,
    // otherwise we must create a new initial map for |function|.
    if (function->has_initial_map() &&
        function->initial_map()->GetConstructor() == *constructor) {
      return handle(function->initial_map(), isolate);
    }

    // Create a new map with the size and number of in-object properties
    // suggested by |function|.

    // Link initial map and constructor function if the new.target is actually a
    // subclass constructor.
    if (IsSubclassConstructor(function->shared()->kind())) {
      Handle<Object> prototype(function->instance_prototype(), isolate);
      InstanceType instance_type = constructor_initial_map->instance_type();
      DCHECK(CanSubclassHaveInobjectProperties(instance_type));
      int internal_fields =
          JSObject::GetInternalFieldCount(*constructor_initial_map);
      int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                          constructor_initial_map->unused_property_fields();
      int instance_size;
      int in_object_properties;
      function->CalculateInstanceSizeForDerivedClass(
          instance_type, internal_fields, &instance_size,
          &in_object_properties);

      int unused_property_fields = in_object_properties - pre_allocated;
      Handle<Map> map =
          Map::CopyInitialMap(constructor_initial_map, instance_size,
                              in_object_properties, unused_property_fields);
      map->set_new_target_is_base(false);

      JSFunction::SetInitialMap(function, map, prototype);
      map->SetConstructor(*constructor);
      map->set_construction_counter(Map::kNoSlackTracking);
      map->StartInobjectSlackTracking();
      return map;
    }
  }

  // Slow path, new.target is either a proxy or can't cache the map.
  // new.target.prototype is not guaranteed to be a JSReceiver, and may need to
  // fall back to the intrinsicDefaultProto.
  Handle<Object> prototype;
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    // Make sure the new.target.prototype is cached.
    EnsureHasInitialMap(function);
    prototype = handle(function->prototype(), isolate);
  } else {
    Handle<String> prototype_string = isolate->factory()->prototype_string();
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, prototype,
        JSReceiver::GetProperty(new_target, prototype_string), Map);
    // The above prototype lookup might change the constructor and its
    // prototype, hence we have to reload the initial map.
    EnsureHasInitialMap(constructor);
    constructor_initial_map = handle(constructor->initial_map(), isolate);
  }

  // If prototype is not a JSReceiver, fetch the intrinsicDefaultProto from the
  // correct realm. Rather than directly fetching the .prototype, we fetch the
  // constructor that points to the .prototype. This relies on
  // constructor.prototype being FROZEN for those constructors.
  if (!prototype->IsJSReceiver()) {
    Handle<Context> context;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, context,
                               JSReceiver::GetFunctionRealm(new_target), Map);
    DCHECK(context->IsNativeContext());
    Handle<Object> maybe_index = JSReceiver::GetDataProperty(
        constructor, isolate->factory()->native_context_index_symbol());
    int index = maybe_index->IsSmi() ? Smi::cast(*maybe_index)->value()
                                     : Context::OBJECT_FUNCTION_INDEX;
    Handle<JSFunction> realm_constructor(JSFunction::cast(context->get(index)));
    prototype = handle(realm_constructor->prototype(), isolate);
  }

  Handle<Map> map = Map::CopyInitialMap(constructor_initial_map);
  map->set_new_target_is_base(false);
  DCHECK(prototype->IsJSReceiver());
  if (map->prototype() != *prototype) {
    Map::SetPrototype(map, prototype, FAST_PROTOTYPE);
  }
  map->SetConstructor(*constructor);
  return map;
}


void JSFunction::PrintName(FILE* out) {
  base::SmartArrayPointer<char> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", name.get());
}


// The filter is a pattern that matches function names in this way:
//   "*"      all; the default
//   "-"      all but the top-level function
//   "-name"  all but the function "name"
//   ""       only the top-level function
//   "name"   only the function "name"
//   "name*"  only functions starting with "name"
//   "~"      none; the tilde is not an identifier
bool JSFunction::PassesFilter(const char* raw_filter) {
  if (*raw_filter == '*') return true;
  String* name = shared()->DebugName();
  Vector<const char> filter = CStrVector(raw_filter);
  if (filter.length() == 0) return name->length() == 0;
  if (filter[0] == '-') {
    // Negative filter.
    if (filter.length() == 1) {
      return (name->length() != 0);
    } else if (name->IsUtf8EqualTo(filter.SubVector(1, filter.length()))) {
      return false;
    }
    if (filter[filter.length() - 1] == '*' &&
        name->IsUtf8EqualTo(filter.SubVector(1, filter.length() - 1), true)) {
      return false;
    }
    return true;

  } else if (name->IsUtf8EqualTo(filter)) {
    return true;
  }
  if (filter[filter.length() - 1] == '*' &&
      name->IsUtf8EqualTo(filter.SubVector(0, filter.length() - 1), true)) {
    return true;
  }
  return false;
}


Handle<String> JSFunction::GetName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name =
      JSReceiver::GetDataProperty(function, isolate->factory()->name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return handle(function->shared()->DebugName(), isolate);
}


Handle<String> JSFunction::GetDebugName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name = JSReceiver::GetDataProperty(
      function, isolate->factory()->display_name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return JSFunction::GetName(function);
}

void JSFunction::SetName(Handle<JSFunction> function, Handle<Name> name,
                         Handle<String> prefix) {
  Isolate* isolate = function->GetIsolate();
  Handle<String> function_name = Name::ToFunctionName(name).ToHandleChecked();
  if (prefix->length() > 0) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendString(prefix);
    builder.AppendCharacter(' ');
    builder.AppendString(function_name);
    function_name = builder.Finish().ToHandleChecked();
  }
  JSObject::DefinePropertyOrElementIgnoreAttributes(
      function, isolate->factory()->name_string(), function_name,
      static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY))
      .ToHandleChecked();
}

namespace {

char const kNativeCodeSource[] = "function () { [native code] }";


Handle<String> NativeCodeFunctionSourceString(
    Handle<SharedFunctionInfo> shared_info) {
  Isolate* const isolate = shared_info->GetIsolate();
  if (shared_info->name()->IsString()) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendCString("function ");
    builder.AppendString(handle(String::cast(shared_info->name()), isolate));
    builder.AppendCString("() { [native code] }");
    return builder.Finish().ToHandleChecked();
  }
  return isolate->factory()->NewStringFromAsciiChecked(kNativeCodeSource);
}

}  // namespace


// static
Handle<String> JSBoundFunction::ToString(Handle<JSBoundFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->NewStringFromAsciiChecked(kNativeCodeSource);
}

// static
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  if (!function->bound_target_function()->IsJSFunction()) return prefix;
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  Handle<Object> target_name = JSFunction::GetName(target);
  if (!target_name->IsString()) return prefix;
  Factory* factory = isolate->factory();
  return factory->NewConsString(prefix, Handle<String>::cast(target_name));
}

// static
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->script()->IsScript() ||
      Script::cast(shared_info->script())->hide_source()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // Check if we should print {function} as a class.
  Handle<Object> class_start_position = JSReceiver::GetDataProperty(
      function, isolate->factory()->class_start_position_symbol());
  if (class_start_position->IsSmi()) {
    Handle<Object> class_end_position = JSReceiver::GetDataProperty(
        function, isolate->factory()->class_end_position_symbol());
    Handle<String> script_source(
        String::cast(Script::cast(shared_info->script())->source()), isolate);
    return isolate->factory()->NewSubString(
        script_source, Handle<Smi>::cast(class_start_position)->value(),
        Handle<Smi>::cast(class_end_position)->value());
  }

  // Check if we have source code for the {function}.
  if (!shared_info->HasSourceCode()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  IncrementalStringBuilder builder(isolate);
  if (!shared_info->is_arrow()) {
    if (shared_info->is_concise_method()) {
      if (shared_info->is_generator()) builder.AppendCharacter('*');
    } else {
      if (shared_info->is_generator()) {
        builder.AppendCString("function* ");
      } else {
        builder.AppendCString("function ");
      }
    }
    if (shared_info->name_should_print_as_anonymous()) {
      builder.AppendCString("anonymous");
    } else if (!shared_info->is_anonymous_expression()) {
      builder.AppendString(handle(String::cast(shared_info->name()), isolate));
    }
  }
  builder.AppendString(Handle<String>::cast(shared_info->GetSourceCode()));
  return builder.Finish().ToHandleChecked();
}


void Oddball::Initialize(Isolate* isolate, Handle<Oddball> oddball,
                         const char* to_string, Handle<Object> to_number,
                         const char* type_of, byte kind) {
  Handle<String> internalized_to_string =
      isolate->factory()->InternalizeUtf8String(to_string);
  Handle<String> internalized_type_of =
      isolate->factory()->InternalizeUtf8String(type_of);
  oddball->set_to_number(*to_number);
  oddball->set_to_string(*internalized_to_string);
  oddball->set_type_of(*internalized_type_of);
  oddball->set_kind(kind);
}


void Script::InitLineEnds(Handle<Script> script) {
  if (!script->line_ends()->IsUndefined()) return;

  Isolate* isolate = script->GetIsolate();

  if (!script->source()->IsString()) {
    DCHECK(script->source()->IsUndefined());
    Handle<FixedArray> empty = isolate->factory()->NewFixedArray(0);
    script->set_line_ends(*empty);
    DCHECK(script->line_ends()->IsFixedArray());
    return;
  }

  Handle<String> src(String::cast(script->source()), isolate);

  Handle<FixedArray> array = String::CalculateLineEnds(src, true);

  if (*array != isolate->heap()->empty_fixed_array()) {
    array->set_map(isolate->heap()->fixed_cow_array_map());
  }

  script->set_line_ends(*array);
  DCHECK(script->line_ends()->IsFixedArray());
}


int Script::GetColumnNumber(Handle<Script> script, int code_pos) {
  int line_number = GetLineNumber(script, code_pos);
  if (line_number == -1) return -1;

  DisallowHeapAllocation no_allocation;
  FixedArray* line_ends_array = FixedArray::cast(script->line_ends());
  line_number = line_number - script->line_offset();
  if (line_number == 0) return code_pos + script->column_offset();
  int prev_line_end_pos =
      Smi::cast(line_ends_array->get(line_number - 1))->value();
  return code_pos - (prev_line_end_pos + 1);
}


int Script::GetLineNumberWithArray(int code_pos) {
  DisallowHeapAllocation no_allocation;
  DCHECK(line_ends()->IsFixedArray());
  FixedArray* line_ends_array = FixedArray::cast(line_ends());
  int line_ends_len = line_ends_array->length();
  if (line_ends_len == 0) return -1;

  if ((Smi::cast(line_ends_array->get(0)))->value() >= code_pos) {
    return line_offset();
  }

  int left = 0;
  int right = line_ends_len;
  while (int half = (right - left) / 2) {
    if ((Smi::cast(line_ends_array->get(left + half)))->value() > code_pos) {
      right -= half;
    } else {
      left += half;
    }
  }
  return right + line_offset();
}


int Script::GetLineNumber(Handle<Script> script, int code_pos) {
  InitLineEnds(script);
  return script->GetLineNumberWithArray(code_pos);
}


int Script::GetLineNumber(int code_pos) {
  DisallowHeapAllocation no_allocation;
  if (!line_ends()->IsUndefined()) return GetLineNumberWithArray(code_pos);

  // Slow mode: we do not have line_ends. We have to iterate through source.
  if (!source()->IsString()) return -1;

  String* source_string = String::cast(source());
  int line = 0;
  int len = source_string->length();
  for (int pos = 0; pos < len; pos++) {
    if (pos == code_pos) break;
    if (source_string->Get(pos) == '\n') line++;
  }
  return line;
}


Handle<Object> Script::GetNameOrSourceURL(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  Handle<String> name_or_source_url_key =
      isolate->factory()->InternalizeOneByteString(
          STATIC_CHAR_VECTOR("nameOrSourceURL"));
  Handle<JSObject> script_wrapper = Script::GetWrapper(script);
  Handle<Object> property = Object::GetProperty(
      script_wrapper, name_or_source_url_key).ToHandleChecked();
  DCHECK(property->IsJSFunction());
  Handle<Object> result;
  // Do not check against pending exception, since this function may be called
  // when an exception has already been pending.
  if (!Execution::TryCall(isolate, property, script_wrapper, 0, NULL)
           .ToHandle(&result)) {
    return isolate->factory()->undefined_value();
  }
  return result;
}


Handle<JSObject> Script::GetWrapper(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  if (!script->wrapper()->IsUndefined()) {
    DCHECK(script->wrapper()->IsWeakCell());
    Handle<WeakCell> cell(WeakCell::cast(script->wrapper()));
    if (!cell->cleared()) {
      // Return a handle for the existing script wrapper from the cache.
      return handle(JSObject::cast(cell->value()));
    }
    // If we found an empty WeakCell, that means the script wrapper was
    // GCed.  We are not notified directly of that, so we decrement here
    // so that we at least don't count double for any given script.
    isolate->counters()->script_wrappers()->Decrement();
  }
  // Construct a new script wrapper.
  isolate->counters()->script_wrappers()->Increment();
  Handle<JSFunction> constructor = isolate->script_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(isolate->factory()->NewJSObject(constructor));
  result->set_value(*script);
  Handle<WeakCell> cell = isolate->factory()->NewWeakCell(result);
  script->set_wrapper(*cell);
  return result;
}


MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    FunctionLiteral* fun) {
  WeakFixedArray::Iterator iterator(shared_function_infos());
  SharedFunctionInfo* shared;
  while ((shared = iterator.Next<SharedFunctionInfo>())) {
    if (fun->function_token_position() == shared->function_token_position() &&
        fun->start_position() == shared->start_position()) {
      return Handle<SharedFunctionInfo>(shared);
    }
  }
  return MaybeHandle<SharedFunctionInfo>();
}


Script::Iterator::Iterator(Isolate* isolate)
    : iterator_(isolate->heap()->script_list()) {}


Script* Script::Iterator::Next() { return iterator_.Next<Script>(); }


SharedFunctionInfo::Iterator::Iterator(Isolate* isolate)
    : script_iterator_(isolate),
      sfi_iterator_(isolate->heap()->noscript_shared_function_infos()) {}


bool SharedFunctionInfo::Iterator::NextScript() {
  Script* script = script_iterator_.Next();
  if (script == NULL) return false;
  sfi_iterator_.Reset(script->shared_function_infos());
  return true;
}


SharedFunctionInfo* SharedFunctionInfo::Iterator::Next() {
  do {
    SharedFunctionInfo* next = sfi_iterator_.Next<SharedFunctionInfo>();
    if (next != NULL) return next;
  } while (NextScript());
  return NULL;
}


void SharedFunctionInfo::SetScript(Handle<SharedFunctionInfo> shared,
                                   Handle<Object> script_object) {
  if (shared->script() == *script_object) return;
  Isolate* isolate = shared->GetIsolate();

  // Add shared function info to new script's list. If a collection occurs,
  // the shared function info may be temporarily in two lists.
  // This is okay because the gc-time processing of these lists can tolerate
  // duplicates.
  Handle<Object> list;
  if (script_object->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_object);
    list = handle(script->shared_function_infos(), isolate);
  } else {
    list = isolate->factory()->noscript_shared_function_infos();
  }

#ifdef DEBUG
  {
    WeakFixedArray::Iterator iterator(*list);
    SharedFunctionInfo* next;
    while ((next = iterator.Next<SharedFunctionInfo>())) {
      DCHECK_NE(next, *shared);
    }
  }
#endif  // DEBUG
  list = WeakFixedArray::Add(list, shared);

  if (script_object->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_object);
    script->set_shared_function_infos(*list);
  } else {
    isolate->heap()->SetRootNoScriptSharedFunctionInfos(*list);
  }

  // Remove shared function info from old script's list.
  if (shared->script()->IsScript()) {
    Script* old_script = Script::cast(shared->script());
    if (old_script->shared_function_infos()->IsWeakFixedArray()) {
      WeakFixedArray* list =
          WeakFixedArray::cast(old_script->shared_function_infos());
      list->Remove(shared);
    }
  } else {
    // Remove shared function info from root array.
    Object* list = isolate->heap()->noscript_shared_function_infos();
    CHECK(WeakFixedArray::cast(list)->Remove(shared));
  }

  // Finally set new script.
  shared->set_script(*script_object);
}


String* SharedFunctionInfo::DebugName() {
  Object* n = name();
  if (!n->IsString() || String::cast(n)->length() == 0) return inferred_name();
  return String::cast(n);
}


bool SharedFunctionInfo::HasSourceCode() const {
  return !script()->IsUndefined() &&
         !reinterpret_cast<Script*>(script())->source()->IsUndefined();
}


Handle<Object> SharedFunctionInfo::GetSourceCode() {
  if (!HasSourceCode()) return GetIsolate()->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(script())->source()));
  return GetIsolate()->factory()->NewSubString(
      source, start_position(), end_position());
}


bool SharedFunctionInfo::IsInlineable() {
  // Check that the function has a script associated with it.
  if (!script()->IsScript()) return false;
  return !optimization_disabled();
}


int SharedFunctionInfo::SourceSize() {
  return end_position() - start_position();
}


namespace {

void CalculateInstanceSizeHelper(InstanceType instance_type,
                                 int requested_internal_fields,
                                 int requested_in_object_properties,
                                 int* instance_size,
                                 int* in_object_properties) {
  int header_size = JSObject::GetHeaderSize(instance_type);
  DCHECK_LE(requested_internal_fields,
            (JSObject::kMaxInstanceSize - header_size) >> kPointerSizeLog2);
  *instance_size =
      Min(header_size +
              ((requested_internal_fields + requested_in_object_properties)
               << kPointerSizeLog2),
          JSObject::kMaxInstanceSize);
  *in_object_properties = ((*instance_size - header_size) >> kPointerSizeLog2) -
                          requested_internal_fields;
}

}  // namespace


void JSFunction::CalculateInstanceSize(InstanceType instance_type,
                                       int requested_internal_fields,
                                       int* instance_size,
                                       int* in_object_properties) {
  CalculateInstanceSizeHelper(instance_type, requested_internal_fields,
                              shared()->expected_nof_properties(),
                              instance_size, in_object_properties);
}


void JSFunction::CalculateInstanceSizeForDerivedClass(
    InstanceType instance_type, int requested_internal_fields,
    int* instance_size, int* in_object_properties) {
  Isolate* isolate = GetIsolate();
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, this,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    JSReceiver* current = iter.GetCurrent<JSReceiver>();
    if (!current->IsJSFunction()) break;
    JSFunction* func = JSFunction::cast(current);
    SharedFunctionInfo* shared = func->shared();
    expected_nof_properties += shared->expected_nof_properties();
    if (!IsSubclassConstructor(shared->kind())) {
      break;
    }
  }
  CalculateInstanceSizeHelper(instance_type, requested_internal_fields,
                              expected_nof_properties, instance_size,
                              in_object_properties);
}


// Output the source code without any allocation in the heap.
std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v) {
  const SharedFunctionInfo* s = v.value;
  // For some native functions there is no source.
  if (!s->HasSourceCode()) return os << "<No Source>";

  // Get the source for the script which this function came from.
  // Don't use String::cast because we don't want more assertion errors while
  // we are already creating a stack dump.
  String* script_source =
      reinterpret_cast<String*>(Script::cast(s->script())->source());

  if (!script_source->LooksValid()) return os << "<Invalid Source>";

  if (!s->is_toplevel()) {
    os << "function ";
    Object* name = s->name();
    if (name->IsString() && String::cast(name)->length() > 0) {
      String::cast(name)->PrintUC16(os);
    }
  }

  int len = s->end_position() - s->start_position();
  if (len <= v.max_length || v.max_length < 0) {
    script_source->PrintUC16(os, s->start_position(), s->end_position());
    return os;
  } else {
    script_source->PrintUC16(os, s->start_position(),
                             s->start_position() + v.max_length);
    return os << "...\n";
  }
}


static bool IsCodeEquivalent(Code* code, Code* recompiled) {
  if (code->instruction_size() != recompiled->instruction_size()) return false;
  ByteArray* code_relocation = code->relocation_info();
  ByteArray* recompiled_relocation = recompiled->relocation_info();
  int length = code_relocation->length();
  if (length != recompiled_relocation->length()) return false;
  int compare = memcmp(code_relocation->GetDataStartAddress(),
                       recompiled_relocation->GetDataStartAddress(),
                       length);
  return compare == 0;
}


void SharedFunctionInfo::EnableDeoptimizationSupport(Code* recompiled) {
  DCHECK(!has_deoptimization_support());
  DisallowHeapAllocation no_allocation;
  Code* code = this->code();
  if (IsCodeEquivalent(code, recompiled)) {
    // Copy the deoptimization data from the recompiled code.
    code->set_deoptimization_data(recompiled->deoptimization_data());
    code->set_has_deoptimization_support(true);
  } else {
    // TODO(3025757): In case the recompiled isn't equivalent to the
    // old code, we have to replace it. We should try to avoid this
    // altogether because it flushes valuable type feedback by
    // effectively resetting all IC state.
    ReplaceCode(recompiled);
  }
  DCHECK(has_deoptimization_support());
}


void SharedFunctionInfo::DisableOptimization(BailoutReason reason) {
  // Disable optimization for the shared function info and mark the
  // code as non-optimizable. The marker on the shared function info
  // is there because we flush non-optimized code thereby loosing the
  // non-optimizable information for the code. When the code is
  // regenerated and set on the shared function info it is marked as
  // non-optimizable if optimization is disabled for the shared
  // function info.
  DCHECK(reason != kNoReason);
  set_optimization_disabled(true);
  set_disable_optimization_reason(reason);
  // Code should be the lazy compilation stub or else unoptimized.
  DCHECK(code()->kind() == Code::FUNCTION || code()->kind() == Code::BUILTIN);
  PROFILE(GetIsolate(), CodeDisableOptEvent(code(), this));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}


void SharedFunctionInfo::InitFromFunctionLiteral(
    Handle<SharedFunctionInfo> shared_info, FunctionLiteral* lit) {
  shared_info->set_length(lit->scope()->default_function_length());
  shared_info->set_internal_formal_parameter_count(lit->parameter_count());
  shared_info->set_function_token_position(lit->function_token_position());
  shared_info->set_start_position(lit->start_position());
  shared_info->set_end_position(lit->end_position());
  shared_info->set_is_declaration(lit->is_declaration());
  shared_info->set_is_named_expression(lit->is_named_expression());
  shared_info->set_is_anonymous_expression(lit->is_anonymous_expression());
  shared_info->set_inferred_name(*lit->inferred_name());
  shared_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  shared_info->set_allows_lazy_compilation_without_context(
      lit->AllowsLazyCompilationWithoutContext());
  shared_info->set_language_mode(lit->language_mode());
  shared_info->set_uses_arguments(lit->scope()->arguments() != NULL);
  shared_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
  shared_info->set_ast_node_count(lit->ast_node_count());
  shared_info->set_is_function(lit->is_function());
  if (lit->dont_optimize_reason() != kNoReason) {
    shared_info->DisableOptimization(lit->dont_optimize_reason());
  }
  shared_info->set_dont_crankshaft(lit->flags() &
                                   AstProperties::kDontCrankshaft);
  shared_info->set_kind(lit->kind());
  if (!IsConstructable(lit->kind(), lit->language_mode())) {
    shared_info->set_construct_stub(
        *shared_info->GetIsolate()->builtins()->ConstructedNonConstructable());
  }
  shared_info->set_needs_home_object(lit->scope()->NeedsHomeObject());
  shared_info->set_asm_function(lit->scope()->asm_function());
}


bool SharedFunctionInfo::VerifyBailoutId(BailoutId id) {
  DCHECK(!id.IsNone());
  Code* unoptimized = code();
  DeoptimizationOutputData* data =
      DeoptimizationOutputData::cast(unoptimized->deoptimization_data());
  unsigned ignore = Deoptimizer::GetOutputInfo(data, id, this);
  USE(ignore);
  return true;  // Return true if there was no DCHECK.
}


void Map::StartInobjectSlackTracking() {
  DCHECK(!IsInobjectSlackTrackingInProgress());

  // No tracking during the snapshot construction phase.
  Isolate* isolate = GetIsolate();
  if (isolate->serializer_enabled()) return;

  if (unused_property_fields() == 0) return;

  set_construction_counter(Map::kSlackTrackingCounterStart);
}


void SharedFunctionInfo::ResetForNewContext(int new_ic_age) {
  code()->ClearInlineCaches();
  // If we clear ICs, we need to clear the type feedback vector too, since
  // CallICs are synced with a feedback vector slot.
  ClearTypeFeedbackInfo();
  set_ic_age(new_ic_age);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_profiler_ticks(0);
    if (optimization_disabled() &&
        opt_count() >= FLAG_max_opt_count) {
      // Re-enable optimizations if they were disabled due to opt_count limit.
      set_optimization_disabled(false);
    }
    set_opt_count(0);
    set_deopt_count(0);
  }
}


int SharedFunctionInfo::SearchOptimizedCodeMapEntry(Context* native_context,
                                                    BailoutId osr_ast_id) {
  DisallowHeapAllocation no_gc;
  DCHECK(native_context->IsNativeContext());
  if (!OptimizedCodeMapIsCleared()) {
    FixedArray* optimized_code_map = this->optimized_code_map();
    int length = optimized_code_map->length();
    Smi* osr_ast_id_smi = Smi::FromInt(osr_ast_id.ToInt());
    for (int i = kEntriesStart; i < length; i += kEntryLength) {
      if (WeakCell::cast(optimized_code_map->get(i + kContextOffset))
                  ->value() == native_context &&
          optimized_code_map->get(i + kOsrAstIdOffset) == osr_ast_id_smi) {
        return i;
      }
    }
    Object* shared_code =
        WeakCell::cast(optimized_code_map->get(kSharedCodeIndex))->value();
    if (shared_code->IsCode() && osr_ast_id.IsNone()) {
      return kSharedCodeIndex;
    }
  }
  return -1;
}


CodeAndLiterals SharedFunctionInfo::SearchOptimizedCodeMap(
    Context* native_context, BailoutId osr_ast_id) {
  CodeAndLiterals result = {nullptr, nullptr};
  int entry = SearchOptimizedCodeMapEntry(native_context, osr_ast_id);
  if (entry != kNotFound) {
    FixedArray* code_map = optimized_code_map();
    if (entry == kSharedCodeIndex) {
      // We know the weak cell isn't cleared because we made sure of it in
      // SearchOptimizedCodeMapEntry and performed no allocations since that
      // call.
      result = {
          Code::cast(WeakCell::cast(code_map->get(kSharedCodeIndex))->value()),
          nullptr};
    } else {
      DCHECK_LE(entry + kEntryLength, code_map->length());
      WeakCell* cell = WeakCell::cast(code_map->get(entry + kCachedCodeOffset));
      WeakCell* literals_cell =
          WeakCell::cast(code_map->get(entry + kLiteralsOffset));

      result = {cell->cleared() ? nullptr : Code::cast(cell->value()),
                literals_cell->cleared()
                    ? nullptr
                    : LiteralsArray::cast(literals_cell->value())};
    }
  }
  if (FLAG_trace_opt && !OptimizedCodeMapIsCleared() &&
      result.code == nullptr) {
    PrintF("[didn't find optimized code in optimized code map for ");
    ShortPrint();
    PrintF("]\n");
  }
  return result;
}


#define DECLARE_TAG(ignore1, name, ignore2) name,
const char* const VisitorSynchronization::kTags[
    VisitorSynchronization::kNumberOfSyncTags] = {
  VISITOR_SYNCHRONIZATION_TAGS_LIST(DECLARE_TAG)
};
#undef DECLARE_TAG


#define DECLARE_TAG(ignore1, ignore2, name) name,
const char* const VisitorSynchronization::kTagNames[
    VisitorSynchronization::kNumberOfSyncTags] = {
  VISITOR_SYNCHRONIZATION_TAGS_LIST(DECLARE_TAG)
};
#undef DECLARE_TAG


void ObjectVisitor::VisitCodeTarget(RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitCodeAgeSequence(RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
  Object* stub = rinfo->code_age_stub();
  if (stub) {
    VisitPointer(&stub);
  }
}


void ObjectVisitor::VisitCodeEntry(Address entry_address) {
  Object* code = Code::GetObjectFromEntryAddress(entry_address);
  Object* old_code = code;
  VisitPointer(&code);
  if (code != old_code) {
    Memory::Address_at(entry_address) = reinterpret_cast<Code*>(code)->entry();
  }
}


void ObjectVisitor::VisitCell(RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::CELL);
  Object* cell = rinfo->target_cell();
  Object* old_cell = cell;
  VisitPointer(&cell);
  if (cell != old_cell) {
    rinfo->set_target_cell(reinterpret_cast<Cell*>(cell));
  }
}


void ObjectVisitor::VisitDebugTarget(RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
         rinfo->IsPatchedDebugBreakSlotSequence());
  Object* target = Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
  Object* old_target = target;
  VisitPointer(&target);
  CHECK_EQ(target, old_target);  // VisitPointer doesn't change Code* *target.
}


void ObjectVisitor::VisitEmbeddedPointer(RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  Object* p = rinfo->target_object();
  VisitPointer(&p);
}


void ObjectVisitor::VisitExternalReference(RelocInfo* rinfo) {
  Address p = rinfo->target_external_reference();
  VisitExternalReference(&p);
}


void Code::InvalidateRelocation() {
  InvalidateEmbeddedObjects();
  set_relocation_info(GetHeap()->empty_byte_array());
}


void Code::InvalidateEmbeddedObjects() {
  Object* undefined = GetHeap()->undefined_value();
  Cell* undefined_cell = GetHeap()->undefined_cell();
  int mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::CELL);
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      it.rinfo()->set_target_object(undefined, SKIP_WRITE_BARRIER);
    } else if (mode == RelocInfo::CELL) {
      it.rinfo()->set_target_cell(undefined_cell, SKIP_WRITE_BARRIER);
    }
  }
}


void Code::Relocate(intptr_t delta) {
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  Assembler::FlushICache(GetIsolate(), instruction_start(), instruction_size());
}


void Code::CopyFrom(const CodeDesc& desc) {
  DCHECK(Marking::Color(this) == Marking::WHITE_OBJECT);

  // copy code
  CopyBytes(instruction_start(), desc.buffer,
            static_cast<size_t>(desc.instr_size));

  // copy reloc info
  CopyBytes(relocation_start(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));

  // unbox handles and relocate
  intptr_t delta = instruction_start() - desc.buffer;
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::CELL) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
                  RelocInfo::kApplyMask;
  // Needed to find target_object and runtime_entry on X64
  Assembler* origin = desc.origin;
  AllowDeferredHandleDereference embedding_raw_address;
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CELL) {
      Handle<Cell> cell  = it.rinfo()->target_cell_handle();
      it.rinfo()->set_target_cell(*cell, SKIP_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles in inline cache targets to direct
      // pointers to the first instruction in the code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->instruction_start(),
                                     SKIP_WRITE_BARRIER,
                                     SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(p, SKIP_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CODE_AGE_SEQUENCE) {
      Handle<Object> p = it.rinfo()->code_age_stub_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_code_age_stub(code, SKIP_ICACHE_FLUSH);
    } else {
      it.rinfo()->apply(delta);
    }
  }
  Assembler::FlushICache(GetIsolate(), instruction_start(), instruction_size());
}

// Locate the source position which is closest to the code offset. This is
// using the source position information embedded in the relocation info.
// The position returned is relative to the beginning of the script where the
// source for this function is found.
int Code::SourcePosition(int code_offset) {
  Address pc = instruction_start() + code_offset;
  int distance = kMaxInt;
  int position = RelocInfo::kNoPosition;  // Initially no position found.
  // Run through all the relocation info to find the best matching source
  // position. All the code needs to be considered as the sequence of the
  // instructions in the code does not necessarily follow the same order as the
  // source.
  RelocIterator it(this, RelocInfo::kPositionMask);
  while (!it.done()) {
    // Only look at positions after the current pc.
    if (it.rinfo()->pc() < pc) {
      // Get position and distance.

      int dist = static_cast<int>(pc - it.rinfo()->pc());
      int pos = static_cast<int>(it.rinfo()->data());
      // If this position is closer than the current candidate or if it has the
      // same distance as the current candidate and the position is higher then
      // this position is the new candidate.
      if ((dist < distance) ||
          (dist == distance && pos > position)) {
        position = pos;
        distance = dist;
      }
    }
    it.next();
  }
  return position;
}


// Same as Code::SourcePosition above except it only looks for statement
// positions.
int Code::SourceStatementPosition(int code_offset) {
  // First find the position as close as possible using all position
  // information.
  int position = SourcePosition(code_offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  RelocIterator it(this, RelocInfo::kPositionMask);
  while (!it.done()) {
    if (RelocInfo::IsStatementPosition(it.rinfo()->rmode())) {
      int p = static_cast<int>(it.rinfo()->data());
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
    it.next();
  }
  return statement_position;
}


SafepointEntry Code::GetSafepointEntry(Address pc) {
  SafepointTable table(this);
  return table.FindEntry(pc);
}


Object* Code::FindNthObject(int n, Map* match_map) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsWeakCell()) object = WeakCell::cast(object)->value();
    if (object->IsHeapObject()) {
      if (HeapObject::cast(object)->map() == match_map) {
        if (--n == 0) return object;
      }
    }
  }
  return NULL;
}


AllocationSite* Code::FindFirstAllocationSite() {
  Object* result = FindNthObject(1, GetHeap()->allocation_site_map());
  return (result != NULL) ? AllocationSite::cast(result) : NULL;
}


Map* Code::FindFirstMap() {
  Object* result = FindNthObject(1, GetHeap()->meta_map());
  return (result != NULL) ? Map::cast(result) : NULL;
}


void Code::FindAndReplace(const FindAndReplacePattern& pattern) {
  DCHECK(is_inline_cache_stub() || is_handler());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  STATIC_ASSERT(FindAndReplacePattern::kMaxCount < 32);
  int current_pattern = 0;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsHeapObject()) {
      if (object->IsWeakCell()) {
        object = HeapObject::cast(WeakCell::cast(object)->value());
      }
      Map* map = HeapObject::cast(object)->map();
      if (map == *pattern.find_[current_pattern]) {
        info->set_target_object(*pattern.replace_[current_pattern]);
        if (++current_pattern == pattern.count_) return;
      }
    }
  }
  UNREACHABLE();
}


void Code::FindAllMaps(MapHandleList* maps) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsWeakCell()) object = WeakCell::cast(object)->value();
    if (object->IsMap()) maps->Add(handle(Map::cast(object)));
  }
}


Code* Code::FindFirstHandler() {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  bool skip_next_handler = false;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::EMBEDDED_OBJECT) {
      Object* obj = info->target_object();
      skip_next_handler |= obj->IsWeakCell() && WeakCell::cast(obj)->cleared();
    } else {
      Code* code = Code::GetCodeFromTargetAddress(info->target_address());
      if (code->kind() == Code::HANDLER) {
        if (!skip_next_handler) return code;
        skip_next_handler = false;
      }
    }
  }
  return NULL;
}


bool Code::FindHandlers(CodeHandleList* code_list, int length) {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  bool skip_next_handler = false;
  int i = 0;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    if (i == length) return true;
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::EMBEDDED_OBJECT) {
      Object* obj = info->target_object();
      skip_next_handler |= obj->IsWeakCell() && WeakCell::cast(obj)->cleared();
    } else {
      Code* code = Code::GetCodeFromTargetAddress(info->target_address());
      // IC stubs with handlers never contain non-handler code objects before
      // handler targets.
      if (code->kind() != Code::HANDLER) break;
      if (!skip_next_handler) {
        code_list->Add(Handle<Code>(code));
        i++;
      }
      skip_next_handler = false;
    }
  }
  return i == length;
}


MaybeHandle<Code> Code::FindHandlerForMap(Map* map) {
  DCHECK(is_inline_cache_stub());
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  bool return_next = false;
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->rmode() == RelocInfo::EMBEDDED_OBJECT) {
      Object* object = info->target_object();
      if (object->IsWeakCell()) object = WeakCell::cast(object)->value();
      if (object == map) return_next = true;
    } else if (return_next) {
      Code* code = Code::GetCodeFromTargetAddress(info->target_address());
      DCHECK(code->kind() == Code::HANDLER);
      return handle(code);
    }
  }
  return MaybeHandle<Code>();
}


Name* Code::FindFirstName() {
  DCHECK(is_inline_cache_stub());
  DisallowHeapAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsName()) return Name::cast(object);
  }
  return NULL;
}


void Code::ClearInlineCaches() {
  ClearInlineCaches(NULL);
}


void Code::ClearInlineCaches(Code::Kind kind) {
  ClearInlineCaches(&kind);
}


void Code::ClearInlineCaches(Code::Kind* kind) {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target(Code::GetCodeFromTargetAddress(info->target_address()));
    if (target->is_inline_cache_stub()) {
      if (kind == NULL || *kind == target->kind()) {
        IC::Clear(this->GetIsolate(), info->pc(),
                  info->host()->constant_pool());
      }
    }
  }
}

int AbstractCode::SourcePosition(int offset) {
  return IsBytecodeArray() ? GetBytecodeArray()->SourcePosition(offset)
                           : GetCode()->SourcePosition(offset);
}

int AbstractCode::SourceStatementPosition(int offset) {
  return IsBytecodeArray() ? GetBytecodeArray()->SourceStatementPosition(offset)
                           : GetCode()->SourceStatementPosition(offset);
}

void SharedFunctionInfo::ClearTypeFeedbackInfo() {
  feedback_vector()->ClearSlots(this);
}


void SharedFunctionInfo::ClearTypeFeedbackInfoAtGCTime() {
  feedback_vector()->ClearSlotsAtGCTime(this);
}


BailoutId Code::TranslatePcOffsetToAstId(uint32_t pc_offset) {
  DisallowHeapAllocation no_gc;
  DCHECK(kind() == FUNCTION);
  BackEdgeTable back_edges(this, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (back_edges.pc_offset(i) == pc_offset) return back_edges.ast_id(i);
  }
  return BailoutId::None();
}


uint32_t Code::TranslateAstIdToPcOffset(BailoutId ast_id) {
  DisallowHeapAllocation no_gc;
  DCHECK(kind() == FUNCTION);
  BackEdgeTable back_edges(this, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (back_edges.ast_id(i) == ast_id) return back_edges.pc_offset(i);
  }
  UNREACHABLE();  // We expect to find the back edge.
  return 0;
}


void Code::MakeCodeAgeSequenceYoung(byte* sequence, Isolate* isolate) {
  PatchPlatformCodeAge(isolate, sequence, kNoAgeCodeAge, NO_MARKING_PARITY);
}


void Code::MarkCodeAsExecuted(byte* sequence, Isolate* isolate) {
  PatchPlatformCodeAge(isolate, sequence, kExecutedOnceCodeAge,
      NO_MARKING_PARITY);
}


// NextAge defines the Code::Age state transitions during a GC cycle.
static Code::Age NextAge(Code::Age age) {
  switch (age) {
    case Code::kNotExecutedCodeAge:  // Keep, until we've been executed.
    case Code::kToBeExecutedOnceCodeAge:  // Keep, until we've been executed.
    case Code::kLastCodeAge:  // Clamp at last Code::Age value.
      return age;
    case Code::kExecutedOnceCodeAge:
      // Pre-age code that has only been executed once.
      return static_cast<Code::Age>(Code::kPreAgedCodeAge + 1);
    default:
      return static_cast<Code::Age>(age + 1);  // Default case: Increase age.
  }
}


// IsOldAge defines the collection criteria for a Code object.
static bool IsOldAge(Code::Age age) {
  return age >= Code::kIsOldCodeAge || age == Code::kNotExecutedCodeAge;
}


void Code::MakeYoung(Isolate* isolate) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) MakeCodeAgeSequenceYoung(sequence, isolate);
}


void Code::MarkToBeExecutedOnce(Isolate* isolate) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    PatchPlatformCodeAge(isolate, sequence, kToBeExecutedOnceCodeAge,
                         NO_MARKING_PARITY);
  }
}


void Code::MakeOlder(MarkingParity current_parity) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    Age age;
    MarkingParity code_parity;
    Isolate* isolate = GetIsolate();
    GetCodeAgeAndParity(isolate, sequence, &age, &code_parity);
    Age next_age = NextAge(age);
    if (age != next_age && code_parity != current_parity) {
      PatchPlatformCodeAge(isolate, sequence, next_age, current_parity);
    }
  }
}


bool Code::IsOld() {
  return IsOldAge(GetAge());
}


byte* Code::FindCodeAgeSequence() {
  return FLAG_age_code &&
      prologue_offset() != Code::kPrologueOffsetNotSet &&
      (kind() == OPTIMIZED_FUNCTION ||
       (kind() == FUNCTION && !has_debug_break_slots()))
      ? instruction_start() + prologue_offset()
      : NULL;
}


Code::Age Code::GetAge() {
  byte* sequence = FindCodeAgeSequence();
  if (sequence == NULL) {
    return kNoAgeCodeAge;
  }
  Age age;
  MarkingParity parity;
  GetCodeAgeAndParity(GetIsolate(), sequence, &age, &parity);
  return age;
}


void Code::GetCodeAgeAndParity(Code* code, Age* age,
                               MarkingParity* parity) {
  Isolate* isolate = code->GetIsolate();
  Builtins* builtins = isolate->builtins();
  Code* stub = NULL;
#define HANDLE_CODE_AGE(AGE)                                            \
  stub = *builtins->Make##AGE##CodeYoungAgainEvenMarking();             \
  if (code == stub) {                                                   \
    *age = k##AGE##CodeAge;                                             \
    *parity = EVEN_MARKING_PARITY;                                      \
    return;                                                             \
  }                                                                     \
  stub = *builtins->Make##AGE##CodeYoungAgainOddMarking();              \
  if (code == stub) {                                                   \
    *age = k##AGE##CodeAge;                                             \
    *parity = ODD_MARKING_PARITY;                                       \
    return;                                                             \
  }
  CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
  stub = *builtins->MarkCodeAsExecutedOnce();
  if (code == stub) {
    *age = kNotExecutedCodeAge;
    *parity = NO_MARKING_PARITY;
    return;
  }
  stub = *builtins->MarkCodeAsExecutedTwice();
  if (code == stub) {
    *age = kExecutedOnceCodeAge;
    *parity = NO_MARKING_PARITY;
    return;
  }
  stub = *builtins->MarkCodeAsToBeExecutedOnce();
  if (code == stub) {
    *age = kToBeExecutedOnceCodeAge;
    *parity = NO_MARKING_PARITY;
    return;
  }
  UNREACHABLE();
}


Code* Code::GetCodeAgeStub(Isolate* isolate, Age age, MarkingParity parity) {
  Builtins* builtins = isolate->builtins();
  switch (age) {
#define HANDLE_CODE_AGE(AGE)                                            \
    case k##AGE##CodeAge: {                                             \
      Code* stub = parity == EVEN_MARKING_PARITY                        \
          ? *builtins->Make##AGE##CodeYoungAgainEvenMarking()           \
          : *builtins->Make##AGE##CodeYoungAgainOddMarking();           \
      return stub;                                                      \
    }
    CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
    case kNotExecutedCodeAge: {
      DCHECK(parity == NO_MARKING_PARITY);
      return *builtins->MarkCodeAsExecutedOnce();
    }
    case kExecutedOnceCodeAge: {
      DCHECK(parity == NO_MARKING_PARITY);
      return *builtins->MarkCodeAsExecutedTwice();
    }
    case kToBeExecutedOnceCodeAge: {
      DCHECK(parity == NO_MARKING_PARITY);
      return *builtins->MarkCodeAsToBeExecutedOnce();
    }
    default:
      UNREACHABLE();
      break;
  }
  return NULL;
}


void Code::PrintDeoptLocation(FILE* out, Address pc) {
  Deoptimizer::DeoptInfo info = Deoptimizer::GetDeoptInfo(this, pc);
  class SourcePosition pos = info.position;
  if (info.deopt_reason != Deoptimizer::kNoReason || !pos.IsUnknown()) {
    if (FLAG_hydrogen_track_positions) {
      PrintF(out, "            ;;; deoptimize at %d_%d: %s\n",
             pos.inlining_id(), pos.position(),
             Deoptimizer::GetDeoptReason(info.deopt_reason));
    } else {
      PrintF(out, "            ;;; deoptimize at %d: %s\n", pos.raw(),
             Deoptimizer::GetDeoptReason(info.deopt_reason));
    }
  }
}


bool Code::CanDeoptAt(Address pc) {
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(deoptimization_data());
  Address code_start_address = instruction_start();
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address address = code_start_address + deopt_data->Pc(i)->value();
    if (address == pc && deopt_data->AstId(i) != BailoutId::None()) {
      return true;
    }
  }
  return false;
}


// Identify kind of code.
const char* Code::Kind2String(Kind kind) {
  switch (kind) {
#define CASE(name) case name: return #name;
    CODE_KIND_LIST(CASE)
#undef CASE
    case NUMBER_OF_KINDS: break;
  }
  UNREACHABLE();
  return NULL;
}


Handle<WeakCell> Code::WeakCellFor(Handle<Code> code) {
  DCHECK(code->kind() == OPTIMIZED_FUNCTION);
  WeakCell* raw_cell = code->CachedWeakCell();
  if (raw_cell != NULL) return Handle<WeakCell>(raw_cell);
  Handle<WeakCell> cell = code->GetIsolate()->factory()->NewWeakCell(code);
  DeoptimizationInputData::cast(code->deoptimization_data())
      ->SetWeakCellCache(*cell);
  return cell;
}


WeakCell* Code::CachedWeakCell() {
  DCHECK(kind() == OPTIMIZED_FUNCTION);
  Object* weak_cell_cache =
      DeoptimizationInputData::cast(deoptimization_data())->WeakCellCache();
  if (weak_cell_cache->IsWeakCell()) {
    DCHECK(this == WeakCell::cast(weak_cell_cache)->value());
    return WeakCell::cast(weak_cell_cache);
  }
  return NULL;
}


#ifdef ENABLE_DISASSEMBLER

void DeoptimizationInputData::DeoptimizationInputDataPrint(
    std::ostream& os) {  // NOLINT
  disasm::NameConverter converter;
  int const inlined_function_count = InlinedFunctionCount()->value();
  os << "Inlined functions (count = " << inlined_function_count << ")\n";
  for (int id = 0; id < inlined_function_count; ++id) {
    Object* info = LiteralArray()->get(id);
    os << " " << Brief(SharedFunctionInfo::cast(info)) << "\n";
  }
  os << "\n";
  int deopt_count = DeoptCount();
  os << "Deoptimization Input Data (deopt points = " << deopt_count << ")\n";
  if (0 != deopt_count) {
    os << " index  ast id    argc     pc";
    if (FLAG_print_code_verbose) os << "  commands";
    os << "\n";
  }
  for (int i = 0; i < deopt_count; i++) {
    os << std::setw(6) << i << "  " << std::setw(6) << AstId(i).ToInt() << "  "
       << std::setw(6) << ArgumentsStackHeight(i)->value() << " "
       << std::setw(6) << Pc(i)->value();

    if (!FLAG_print_code_verbose) {
      os << "\n";
      continue;
    }
    // Print details of the frame translation.
    int translation_index = TranslationIndex(i)->value();
    TranslationIterator iterator(TranslationByteArray(), translation_index);
    Translation::Opcode opcode =
        static_cast<Translation::Opcode>(iterator.Next());
    DCHECK(Translation::BEGIN == opcode);
    int frame_count = iterator.Next();
    int jsframe_count = iterator.Next();
    os << "  " << Translation::StringFor(opcode)
       << " {frame count=" << frame_count
       << ", js frame count=" << jsframe_count << "}\n";

    while (iterator.HasNext() &&
           Translation::BEGIN !=
           (opcode = static_cast<Translation::Opcode>(iterator.Next()))) {
      os << std::setw(31) << "    " << Translation::StringFor(opcode) << " ";

      switch (opcode) {
        case Translation::BEGIN:
          UNREACHABLE();
          break;

        case Translation::JS_FRAME: {
          int ast_id = iterator.Next();
          int shared_info_id = iterator.Next();
          unsigned height = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          os << "{ast_id=" << ast_id << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::INTERPRETED_FRAME: {
          int bytecode_offset = iterator.Next();
          int shared_info_id = iterator.Next();
          unsigned height = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          os << "{bytecode_offset=" << bytecode_offset << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::COMPILED_STUB_FRAME: {
          Code::Kind stub_kind = static_cast<Code::Kind>(iterator.Next());
          os << "{kind=" << stub_kind << "}";
          break;
        }

        case Translation::ARGUMENTS_ADAPTOR_FRAME:
        case Translation::CONSTRUCT_STUB_FRAME: {
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::GETTER_STUB_FRAME:
        case Translation::SETTER_STUB_FRAME: {
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          os << "{function=" << Brief(SharedFunctionInfo::cast(shared_info)
                                          ->DebugName()) << "}";
          break;
        }

        case Translation::REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code) << "}";
          break;
        }

        case Translation::INT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code) << "}";
          break;
        }

        case Translation::UINT32_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (unsigned)}";
          break;
        }

        case Translation::BOOL_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << converter.NameOfCPURegister(reg_code)
             << " (bool)}";
          break;
        }

        case Translation::DOUBLE_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input=" << DoubleRegister::from_code(reg_code).ToString()
             << "}";
          break;
        }

        case Translation::STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::INT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::UINT32_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (unsigned)}";
          break;
        }

        case Translation::BOOL_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << " (bool)}";
          break;
        }

        case Translation::DOUBLE_STACK_SLOT: {
          int input_slot_index = iterator.Next();
          os << "{input=" << input_slot_index << "}";
          break;
        }

        case Translation::LITERAL: {
          int literal_index = iterator.Next();
          Object* literal_value = LiteralArray()->get(literal_index);
          os << "{literal_id=" << literal_index << " (" << Brief(literal_value)
             << ")}";
          break;
        }

        case Translation::DUPLICATED_OBJECT: {
          int object_index = iterator.Next();
          os << "{object_index=" << object_index << "}";
          break;
        }

        case Translation::ARGUMENTS_OBJECT:
        case Translation::CAPTURED_OBJECT: {
          int args_length = iterator.Next();
          os << "{length=" << args_length << "}";
          break;
        }
      }
      os << "\n";
    }
  }
}


void DeoptimizationOutputData::DeoptimizationOutputDataPrint(
    std::ostream& os) {  // NOLINT
  os << "Deoptimization Output Data (deopt points = " << this->DeoptPoints()
     << ")\n";
  if (this->DeoptPoints() == 0) return;

  os << "ast id        pc  state\n";
  for (int i = 0; i < this->DeoptPoints(); i++) {
    int pc_and_state = this->PcAndState(i)->value();
    os << std::setw(6) << this->AstId(i).ToInt() << "  " << std::setw(8)
       << FullCodeGenerator::PcField::decode(pc_and_state) << "  "
       << FullCodeGenerator::State2String(
              FullCodeGenerator::StateField::decode(pc_and_state)) << "\n";
  }
}


void HandlerTable::HandlerTableRangePrint(std::ostream& os) {
  os << "   from   to       hdlr\n";
  for (int i = 0; i < length(); i += kRangeEntrySize) {
    int pc_start = Smi::cast(get(i + kRangeStartIndex))->value();
    int pc_end = Smi::cast(get(i + kRangeEndIndex))->value();
    int handler_field = Smi::cast(get(i + kRangeHandlerIndex))->value();
    int handler_offset = HandlerOffsetField::decode(handler_field);
    CatchPrediction prediction = HandlerPredictionField::decode(handler_field);
    int data = Smi::cast(get(i + kRangeDataIndex))->value();
    os << "  (" << std::setw(4) << pc_start << "," << std::setw(4) << pc_end
       << ")  ->  " << std::setw(4) << handler_offset
       << " (prediction=" << prediction << ", data=" << data << ")\n";
  }
}


void HandlerTable::HandlerTableReturnPrint(std::ostream& os) {
  os << "   off      hdlr (c)\n";
  for (int i = 0; i < length(); i += kReturnEntrySize) {
    int pc_offset = Smi::cast(get(i + kReturnOffsetIndex))->value();
    int handler_field = Smi::cast(get(i + kReturnHandlerIndex))->value();
    int handler_offset = HandlerOffsetField::decode(handler_field);
    CatchPrediction prediction = HandlerPredictionField::decode(handler_field);
    os << "  " << std::setw(4) << pc_offset << "  ->  " << std::setw(4)
       << handler_offset << " (prediction=" << prediction << ")\n";
  }
}


const char* Code::ICState2String(InlineCacheState state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case PREMONOMORPHIC: return "PREMONOMORPHIC";
    case MONOMORPHIC: return "MONOMORPHIC";
    case PROTOTYPE_FAILURE:
      return "PROTOTYPE_FAILURE";
    case POLYMORPHIC: return "POLYMORPHIC";
    case MEGAMORPHIC: return "MEGAMORPHIC";
    case GENERIC: return "GENERIC";
    case DEBUG_STUB: return "DEBUG_STUB";
  }
  UNREACHABLE();
  return NULL;
}


const char* Code::StubType2String(StubType type) {
  switch (type) {
    case NORMAL: return "NORMAL";
    case FAST: return "FAST";
  }
  UNREACHABLE();  // keep the compiler happy
  return NULL;
}


void Code::PrintExtraICState(std::ostream& os,  // NOLINT
                             Kind kind, ExtraICState extra) {
  os << "extra_ic_state = ";
  if ((kind == STORE_IC || kind == KEYED_STORE_IC) &&
      is_strict(static_cast<LanguageMode>(extra))) {
    os << "STRICT\n";
  } else {
    os << extra << "\n";
  }
}


void Code::Disassemble(const char* name, std::ostream& os) {  // NOLINT
  os << "kind = " << Kind2String(kind()) << "\n";
  if (IsCodeStubOrIC()) {
    const char* n = CodeStub::MajorName(CodeStub::GetMajorKey(this));
    os << "major_key = " << (n == NULL ? "null" : n) << "\n";
  }
  if (is_inline_cache_stub()) {
    os << "ic_state = " << ICState2String(ic_state()) << "\n";
    PrintExtraICState(os, kind(), extra_ic_state());
    if (ic_state() == MONOMORPHIC) {
      os << "type = " << StubType2String(type()) << "\n";
    }
    if (is_compare_ic_stub()) {
      DCHECK(CodeStub::GetMajorKey(this) == CodeStub::CompareIC);
      CompareICStub stub(stub_key(), GetIsolate());
      os << "compare_state = " << CompareICState::GetStateName(stub.left())
         << "*" << CompareICState::GetStateName(stub.right()) << " -> "
         << CompareICState::GetStateName(stub.state()) << "\n";
      os << "compare_operation = " << Token::Name(stub.op()) << "\n";
    }
  }
  if ((name != NULL) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  } else if (kind() == BUILTIN) {
    name = GetIsolate()->builtins()->Lookup(instruction_start());
    if (name != NULL) {
      os << "name = " << name << "\n";
    }
  }
  if (kind() == OPTIMIZED_FUNCTION) {
    os << "stack_slots = " << stack_slots() << "\n";
  }
  os << "compiler = " << (is_turbofanned()
                              ? "turbofan"
                              : is_crankshafted() ? "crankshaft"
                                                  : kind() == Code::FUNCTION
                                                        ? "full-codegen"
                                                        : "unknown") << "\n";

  os << "Instructions (size = " << instruction_size() << ")\n";
  {
    Isolate* isolate = GetIsolate();
    int size = instruction_size();
    int safepoint_offset =
        is_crankshafted() ? static_cast<int>(safepoint_table_offset()) : size;
    int back_edge_offset = (kind() == Code::FUNCTION)
                               ? static_cast<int>(back_edge_table_offset())
                               : size;
    int constant_pool_offset = FLAG_enable_embedded_constant_pool
                                   ? this->constant_pool_offset()
                                   : size;

    // Stop before reaching any embedded tables
    int code_size = Min(safepoint_offset, back_edge_offset);
    code_size = Min(code_size, constant_pool_offset);
    byte* begin = instruction_start();
    byte* end = begin + code_size;
    Disassembler::Decode(isolate, &os, begin, end, this);

    if (constant_pool_offset < size) {
      int constant_pool_size = size - constant_pool_offset;
      DCHECK((constant_pool_size & kPointerAlignmentMask) == 0);
      os << "\nConstant Pool (size = " << constant_pool_size << ")\n";
      Vector<char> buf = Vector<char>::New(50);
      intptr_t* ptr = reinterpret_cast<intptr_t*>(begin + constant_pool_offset);
      for (int i = 0; i < constant_pool_size; i += kPointerSize, ptr++) {
        SNPrintF(buf, "%4d %08" V8PRIxPTR, i, *ptr);
        os << static_cast<const void*>(ptr) << "  " << buf.start() << "\n";
      }
    }
  }
  os << "\n";

  if (kind() == FUNCTION) {
    DeoptimizationOutputData* data =
        DeoptimizationOutputData::cast(this->deoptimization_data());
    data->DeoptimizationOutputDataPrint(os);
  } else if (kind() == OPTIMIZED_FUNCTION) {
    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(this->deoptimization_data());
    data->DeoptimizationInputDataPrint(os);
  }
  os << "\n";

  if (is_crankshafted()) {
    SafepointTable table(this);
    os << "Safepoints (size = " << table.size() << ")\n";
    for (unsigned i = 0; i < table.length(); i++) {
      unsigned pc_offset = table.GetPcOffset(i);
      os << static_cast<const void*>(instruction_start() + pc_offset) << "  ";
      os << std::setw(4) << pc_offset << "  ";
      table.PrintEntry(i, os);
      os << " (sp -> fp)  ";
      SafepointEntry entry = table.GetEntry(i);
      if (entry.deoptimization_index() != Safepoint::kNoDeoptimizationIndex) {
        os << std::setw(6) << entry.deoptimization_index();
      } else {
        os << "<none>";
      }
      if (entry.argument_count() > 0) {
        os << " argc: " << entry.argument_count();
      }
      os << "\n";
    }
    os << "\n";
  } else if (kind() == FUNCTION) {
    unsigned offset = back_edge_table_offset();
    // If there is no back edge table, the "table start" will be at or after
    // (due to alignment) the end of the instruction stream.
    if (static_cast<int>(offset) < instruction_size()) {
      DisallowHeapAllocation no_gc;
      BackEdgeTable back_edges(this, &no_gc);

      os << "Back edges (size = " << back_edges.length() << ")\n";
      os << "ast_id  pc_offset  loop_depth\n";

      for (uint32_t i = 0; i < back_edges.length(); i++) {
        os << std::setw(6) << back_edges.ast_id(i).ToInt() << "  "
           << std::setw(9) << back_edges.pc_offset(i) << "  " << std::setw(10)
           << back_edges.loop_depth(i) << "\n";
      }

      os << "\n";
    }
#ifdef OBJECT_PRINT
    if (!type_feedback_info()->IsUndefined()) {
      OFStream os(stdout);
      TypeFeedbackInfo::cast(type_feedback_info())->TypeFeedbackInfoPrint(os);
      os << "\n";
    }
#endif
  }

  if (handler_table()->length() > 0) {
    os << "Handler Table (size = " << handler_table()->Size() << ")\n";
    if (kind() == FUNCTION) {
      HandlerTable::cast(handler_table())->HandlerTableRangePrint(os);
    } else if (kind() == OPTIMIZED_FUNCTION) {
      HandlerTable::cast(handler_table())->HandlerTableReturnPrint(os);
    }
    os << "\n";
  }

  os << "RelocInfo (size = " << relocation_size() << ")\n";
  for (RelocIterator it(this); !it.done(); it.next()) {
    it.rinfo()->Print(GetIsolate(), os);
  }
  os << "\n";
}
#endif  // ENABLE_DISASSEMBLER

int BytecodeArray::SourcePosition(int offset) {
  int last_position = 0;
  for (interpreter::SourcePositionTableIterator iterator(this);
       !iterator.done() && iterator.bytecode_offset() <= offset;
       iterator.Advance()) {
    last_position = iterator.source_position();
  }
  return last_position;
}

int BytecodeArray::SourceStatementPosition(int offset) {
  // First find the position as close as possible using all position
  // information.
  int position = SourcePosition(offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  interpreter::SourcePositionTableIterator iterator(this);
  while (!iterator.done()) {
    if (iterator.is_statement()) {
      int p = iterator.source_position();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
    iterator.Advance();
  }
  return statement_position;
}

void BytecodeArray::Disassemble(std::ostream& os) {
  os << "Parameter count " << parameter_count() << "\n";
  os << "Frame size " << frame_size() << "\n";
  Vector<char> buf = Vector<char>::New(50);

  const uint8_t* first_bytecode_address = GetFirstBytecodeAddress();
  int bytecode_size = 0;

  interpreter::SourcePositionTableIterator source_positions(this);

  for (int i = 0; i < this->length(); i += bytecode_size) {
    const uint8_t* bytecode_start = &first_bytecode_address[i];
    interpreter::Bytecode bytecode =
        interpreter::Bytecodes::FromByte(bytecode_start[0]);
    bytecode_size = interpreter::Bytecodes::Size(bytecode);

    if (!source_positions.done() && i == source_positions.bytecode_offset()) {
      os << std::setw(5) << source_positions.source_position();
      os << (source_positions.is_statement() ? " S> " : " E> ");
      source_positions.Advance();
    } else {
      os << "         ";
    }

    SNPrintF(buf, "%p", bytecode_start);
    os << buf.start() << " : ";
    interpreter::Bytecodes::Decode(os, bytecode_start, parameter_count());

    if (interpreter::Bytecodes::IsJumpConstantWide(bytecode)) {
      DCHECK_EQ(bytecode_size, 3);
      int index = static_cast<int>(ReadUnalignedUInt16(bytecode_start + 1));
      int offset = Smi::cast(constant_pool()->get(index))->value();
      SNPrintF(buf, " (%p)", bytecode_start + offset);
      os << buf.start();
    } else if (interpreter::Bytecodes::IsJumpConstant(bytecode)) {
      DCHECK_EQ(bytecode_size, 2);
      int index = static_cast<int>(bytecode_start[1]);
      int offset = Smi::cast(constant_pool()->get(index))->value();
      SNPrintF(buf, " (%p)", bytecode_start + offset);
      os << buf.start();
    } else if (interpreter::Bytecodes::IsJump(bytecode)) {
      DCHECK_EQ(bytecode_size, 2);
      int offset = static_cast<int8_t>(bytecode_start[1]);
      SNPrintF(buf, " (%p)", bytecode_start + offset);
      os << buf.start();
    }

    os << std::endl;
  }

  if (constant_pool()->length() > 0) {
    os << "Constant pool (size = " << constant_pool()->length() << ")\n";
    constant_pool()->Print();
  }

#ifdef ENABLE_DISASSEMBLER
  if (handler_table()->length() > 0) {
    os << "Handler Table (size = " << handler_table()->Size() << ")\n";
    HandlerTable::cast(handler_table())->HandlerTableRangePrint(os);
  }
#endif
}

void BytecodeArray::CopyBytecodesTo(BytecodeArray* to) {
  BytecodeArray* from = this;
  DCHECK_EQ(from->length(), to->length());
  CopyBytes(to->GetFirstBytecodeAddress(), from->GetFirstBytecodeAddress(),
            from->length());
}

// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK(capacity >= 0);
  array->GetIsolate()->factory()->NewJSArrayStorage(
      array, length, capacity, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}


// Returns false if the passed-in index is marked non-configurable, which will
// cause the truncation operation to halt, and thus no further old values need
// be collected.
static bool GetOldValue(Isolate* isolate,
                        Handle<JSObject> object,
                        uint32_t index,
                        List<Handle<Object> >* old_values,
                        List<uint32_t>* indices) {
  LookupIterator it(isolate, object, index, LookupIterator::HIDDEN);
  CHECK(JSReceiver::GetPropertyAttributes(&it).IsJust());
  DCHECK(it.IsFound());
  if (!it.IsConfigurable()) return false;
  Handle<Object> value =
      it.state() == LookupIterator::ACCESSOR
          ? Handle<Object>::cast(isolate->factory()->the_hole_value())
          : JSReceiver::GetDataProperty(&it);
  old_values->Add(value);
  indices->Add(index);
  return true;
}


void JSArray::SetLength(Handle<JSArray> array, uint32_t new_length) {
  // We should never end in here with a pixel or external array.
  DCHECK(array->AllowsSetLength());
  if (array->SetLengthWouldNormalize(new_length)) {
    JSObject::NormalizeElements(array);
  }
  array->GetElementsAccessor()->SetLength(array, new_length);
}


MaybeHandle<Object> JSArray::ObservableSetLength(Handle<JSArray> array,
                                                 uint32_t new_length) {
  if (!array->map()->is_observed()) {
    SetLength(array, new_length);
    return array;
  }

  Isolate* isolate = array->GetIsolate();
  List<uint32_t> indices;
  List<Handle<Object> > old_values;
  Handle<Object> old_length_handle(array->length(), isolate);
  uint32_t old_length = 0;
  CHECK(old_length_handle->ToArrayLength(&old_length));

  int num_elements = array->NumberOfOwnElements(ALL_PROPERTIES);
  if (num_elements > 0) {
    if (old_length == static_cast<uint32_t>(num_elements)) {
      // Simple case for arrays without holes.
      for (uint32_t i = old_length - 1; i + 1 > new_length; --i) {
        if (!GetOldValue(isolate, array, i, &old_values, &indices)) break;
      }
    } else {
      // For sparse arrays, only iterate over existing elements.
      // TODO(rafaelw): For fast, sparse arrays, we can avoid iterating over
      // the to-be-removed indices twice.
      Handle<FixedArray> keys = isolate->factory()->NewFixedArray(num_elements);
      array->GetOwnElementKeys(*keys, ALL_PROPERTIES);
      while (num_elements-- > 0) {
        uint32_t index = NumberToUint32(keys->get(num_elements));
        if (index < new_length) break;
        if (!GetOldValue(isolate, array, index, &old_values, &indices)) break;
      }
    }
  }

  SetLength(array, new_length);

  CHECK(array->length()->ToArrayLength(&new_length));
  if (old_length == new_length) return array;

  RETURN_ON_EXCEPTION(isolate, BeginPerformSplice(array), Object);

  for (int i = 0; i < indices.length(); ++i) {
    // For deletions where the property was an accessor, old_values[i]
    // will be the hole, which instructs EnqueueChangeRecord to elide
    // the "oldValue" property.
    RETURN_ON_EXCEPTION(
        isolate,
        JSObject::EnqueueChangeRecord(
            array, "delete", isolate->factory()->Uint32ToString(indices[i]),
            old_values[i]),
        Object);
  }

  RETURN_ON_EXCEPTION(isolate,
                      JSObject::EnqueueChangeRecord(
                          array, "update", isolate->factory()->length_string(),
                          old_length_handle),
                      Object);

  RETURN_ON_EXCEPTION(isolate, EndPerformSplice(array), Object);

  uint32_t index = Min(old_length, new_length);
  uint32_t add_count = new_length > old_length ? new_length - old_length : 0;
  uint32_t delete_count = new_length < old_length ? old_length - new_length : 0;
  Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
  if (delete_count > 0) {
    for (int i = indices.length() - 1; i >= 0; i--) {
      // Skip deletions where the property was an accessor, leaving holes
      // in the array of old values.
      if (old_values[i]->IsTheHole()) continue;
      JSObject::AddDataElement(deleted, indices[i] - index, old_values[i], NONE)
          .Assert();
    }

    JSArray::SetLength(deleted, delete_count);
  }

  RETURN_ON_EXCEPTION(
      isolate, EnqueueSpliceRecord(array, index, deleted, add_count), Object);

  return array;
}


// static
void Map::AddDependentCode(Handle<Map> map,
                           DependentCode::DependencyGroup group,
                           Handle<Code> code) {
  Handle<WeakCell> cell = Code::WeakCellFor(code);
  Handle<DependentCode> codes = DependentCode::InsertWeakCode(
      Handle<DependentCode>(map->dependent_code()), group, cell);
  if (*codes != map->dependent_code()) map->set_dependent_code(*codes);
}


Handle<DependentCode> DependentCode::InsertCompilationDependencies(
    Handle<DependentCode> entries, DependencyGroup group,
    Handle<Foreign> info) {
  return Insert(entries, group, info);
}


Handle<DependentCode> DependentCode::InsertWeakCode(
    Handle<DependentCode> entries, DependencyGroup group,
    Handle<WeakCell> code_cell) {
  return Insert(entries, group, code_cell);
}


Handle<DependentCode> DependentCode::Insert(Handle<DependentCode> entries,
                                            DependencyGroup group,
                                            Handle<Object> object) {
  if (entries->length() == 0 || entries->group() > group) {
    // There is no such group.
    return DependentCode::New(group, object, entries);
  }
  if (entries->group() < group) {
    // The group comes later in the list.
    Handle<DependentCode> old_next(entries->next_link());
    Handle<DependentCode> new_next = Insert(old_next, group, object);
    if (!old_next.is_identical_to(new_next)) {
      entries->set_next_link(*new_next);
    }
    return entries;
  }
  DCHECK_EQ(group, entries->group());
  int count = entries->count();
  // Check for existing entry to avoid duplicates.
  for (int i = 0; i < count; i++) {
    if (entries->object_at(i) == *object) return entries;
  }
  if (entries->length() < kCodesStartIndex + count + 1) {
    entries = EnsureSpace(entries);
    // Count could have changed, reload it.
    count = entries->count();
  }
  entries->set_object_at(count, *object);
  entries->set_count(count + 1);
  return entries;
}


Handle<DependentCode> DependentCode::New(DependencyGroup group,
                                         Handle<Object> object,
                                         Handle<DependentCode> next) {
  Isolate* isolate = next->GetIsolate();
  Handle<DependentCode> result = Handle<DependentCode>::cast(
      isolate->factory()->NewFixedArray(kCodesStartIndex + 1, TENURED));
  result->set_next_link(*next);
  result->set_flags(GroupField::encode(group) | CountField::encode(1));
  result->set_object_at(0, *object);
  return result;
}


Handle<DependentCode> DependentCode::EnsureSpace(
    Handle<DependentCode> entries) {
  if (entries->Compact()) return entries;
  Isolate* isolate = entries->GetIsolate();
  int capacity = kCodesStartIndex + DependentCode::Grow(entries->count());
  int grow_by = capacity - entries->length();
  return Handle<DependentCode>::cast(
      isolate->factory()->CopyFixedArrayAndGrow(entries, grow_by, TENURED));
}


bool DependentCode::Compact() {
  int old_count = count();
  int new_count = 0;
  for (int i = 0; i < old_count; i++) {
    Object* obj = object_at(i);
    if (!obj->IsWeakCell() || !WeakCell::cast(obj)->cleared()) {
      if (i != new_count) {
        copy(i, new_count);
      }
      new_count++;
    }
  }
  set_count(new_count);
  for (int i = new_count; i < old_count; i++) {
    clear_at(i);
  }
  return new_count < old_count;
}


void DependentCode::UpdateToFinishedCode(DependencyGroup group, Foreign* info,
                                         WeakCell* code_cell) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    next_link()->UpdateToFinishedCode(group, info, code_cell);
    return;
  }
  DCHECK_EQ(group, this->group());
  DisallowHeapAllocation no_gc;
  int count = this->count();
  for (int i = 0; i < count; i++) {
    if (object_at(i) == info) {
      set_object_at(i, code_cell);
      break;
    }
  }
#ifdef DEBUG
  for (int i = 0; i < count; i++) {
    DCHECK(object_at(i) != info);
  }
#endif
}


void DependentCode::RemoveCompilationDependencies(
    DependentCode::DependencyGroup group, Foreign* info) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    next_link()->RemoveCompilationDependencies(group, info);
    return;
  }
  DCHECK_EQ(group, this->group());
  DisallowHeapAllocation no_allocation;
  int old_count = count();
  // Find compilation info wrapper.
  int info_pos = -1;
  for (int i = 0; i < old_count; i++) {
    if (object_at(i) == info) {
      info_pos = i;
      break;
    }
  }
  if (info_pos == -1) return;  // Not found.
  // Use the last code to fill the gap.
  if (info_pos < old_count - 1) {
    copy(old_count - 1, info_pos);
  }
  clear_at(old_count - 1);
  set_count(old_count - 1);

#ifdef DEBUG
  for (int i = 0; i < old_count - 1; i++) {
    DCHECK(object_at(i) != info);
  }
#endif
}


bool DependentCode::Contains(DependencyGroup group, WeakCell* code_cell) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return false;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    return next_link()->Contains(group, code_cell);
  }
  DCHECK_EQ(group, this->group());
  int count = this->count();
  for (int i = 0; i < count; i++) {
    if (object_at(i) == code_cell) return true;
  }
  return false;
}


bool DependentCode::IsEmpty(DependencyGroup group) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return true;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    return next_link()->IsEmpty(group);
  }
  DCHECK_EQ(group, this->group());
  return count() == 0;
}


bool DependentCode::MarkCodeForDeoptimization(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  if (this->length() == 0 || this->group() > group) {
    // There is no such group.
    return false;
  }
  if (this->group() < group) {
    // The group comes later in the list.
    return next_link()->MarkCodeForDeoptimization(isolate, group);
  }
  DCHECK_EQ(group, this->group());
  DisallowHeapAllocation no_allocation_scope;
  // Mark all the code that needs to be deoptimized.
  bool marked = false;
  bool invalidate_embedded_objects = group == kWeakCodeGroup;
  int count = this->count();
  for (int i = 0; i < count; i++) {
    Object* obj = object_at(i);
    if (obj->IsWeakCell()) {
      WeakCell* cell = WeakCell::cast(obj);
      if (cell->cleared()) continue;
      Code* code = Code::cast(cell->value());
      if (!code->marked_for_deoptimization()) {
        SetMarkedForDeoptimization(code, group);
        if (invalidate_embedded_objects) {
          code->InvalidateEmbeddedObjects();
        }
        marked = true;
      }
    } else {
      DCHECK(obj->IsForeign());
      CompilationDependencies* info =
          reinterpret_cast<CompilationDependencies*>(
              Foreign::cast(obj)->foreign_address());
      info->Abort();
    }
  }
  for (int i = 0; i < count; i++) {
    clear_at(i);
  }
  set_count(0);
  return marked;
}


void DependentCode::DeoptimizeDependentCodeGroup(
    Isolate* isolate,
    DependentCode::DependencyGroup group) {
  DCHECK(AllowCodeDependencyChange::IsAllowed());
  DisallowHeapAllocation no_allocation_scope;
  bool marked = MarkCodeForDeoptimization(isolate, group);
  if (marked) Deoptimizer::DeoptimizeMarkedCode(isolate);
}


void DependentCode::SetMarkedForDeoptimization(Code* code,
                                               DependencyGroup group) {
  code->set_marked_for_deoptimization(true);
  if (FLAG_trace_deopt &&
      (code->deoptimization_data() != code->GetHeap()->empty_fixed_array())) {
    DeoptimizationInputData* deopt_data =
        DeoptimizationInputData::cast(code->deoptimization_data());
    CodeTracer::Scope scope(code->GetHeap()->isolate()->GetCodeTracer());
    PrintF(scope.file(), "[marking dependent code 0x%08" V8PRIxPTR
                         " (opt #%d) for deoptimization, reason: %s]\n",
           reinterpret_cast<intptr_t>(code),
           deopt_data->OptimizationId()->value(), DependencyGroupName(group));
  }
}


const char* DependentCode::DependencyGroupName(DependencyGroup group) {
  switch (group) {
    case kWeakCodeGroup:
      return "weak-code";
    case kTransitionGroup:
      return "transition";
    case kPrototypeCheckGroup:
      return "prototype-check";
    case kPropertyCellChangedGroup:
      return "property-cell-changed";
    case kFieldTypeGroup:
      return "field-type";
    case kInitialMapChangedGroup:
      return "initial-map-changed";
    case kAllocationSiteTenuringChangedGroup:
      return "allocation-site-tenuring-changed";
    case kAllocationSiteTransitionChangedGroup:
      return "allocation-site-transition-changed";
  }
  UNREACHABLE();
  return "?";
}


Handle<Map> Map::TransitionToPrototype(Handle<Map> map,
                                       Handle<Object> prototype,
                                       PrototypeOptimizationMode mode) {
  Handle<Map> new_map = TransitionArray::GetPrototypeTransition(map, prototype);
  if (new_map.is_null()) {
    new_map = Copy(map, "TransitionToPrototype");
    TransitionArray::PutPrototypeTransition(map, prototype, new_map);
    Map::SetPrototype(new_map, prototype, mode);
  }
  return new_map;
}


Maybe<bool> JSReceiver::SetPrototype(Handle<JSReceiver> object,
                                     Handle<Object> value, bool from_javascript,
                                     ShouldThrow should_throw) {
  if (object->IsJSProxy()) {
    return JSProxy::SetPrototype(Handle<JSProxy>::cast(object), value,
                                 from_javascript, should_throw);
  }
  return JSObject::SetPrototype(Handle<JSObject>::cast(object), value,
                                from_javascript, should_throw);
}


// ES6: 9.5.2 [[SetPrototypeOf]] (V)
// static
Maybe<bool> JSProxy::SetPrototype(Handle<JSProxy> proxy, Handle<Object> value,
                                  bool from_javascript,
                                  ShouldThrow should_throw) {
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(Nothing<bool>());
  Handle<Name> trap_name = isolate->factory()->setPrototypeOf_string();
  // 1. Assert: Either Type(V) is Object or Type(V) is Null.
  DCHECK(value->IsJSReceiver() || value->IsNull());
  // 2. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Handle<Object> handler(proxy->handler(), isolate);
  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  if (proxy->IsRevoked()) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  // 5. Let target be the value of the [[ProxyTarget]] internal slot.
  Handle<JSReceiver> target(proxy->target(), isolate);
  // 6. Let trap be ? GetMethod(handler, "getPrototypeOf").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name),
      Nothing<bool>());
  // 7. If trap is undefined, then return target.[[SetPrototypeOf]]().
  if (trap->IsUndefined()) {
    return JSReceiver::SetPrototype(target, value, from_javascript,
                                    should_throw);
  }
  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, «target, V»)).
  Handle<Object> argv[] = {target, value};
  Handle<Object> trap_result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv),
      Nothing<bool>());
  bool bool_trap_result = trap_result->BooleanValue();
  // 9. If booleanTrapResult is false, return false.
  if (!bool_trap_result) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }
  // 10. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> is_extensible = JSReceiver::IsExtensible(target);
  if (is_extensible.IsNothing()) return Nothing<bool>();
  // 11. If extensibleTarget is true, return true.
  if (is_extensible.FromJust()) {
    if (bool_trap_result) return Just(true);
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kProxyTrapReturnedFalsish, trap_name));
  }
  // 12. Let targetProto be ? target.[[GetPrototypeOf]]().
  Handle<Object> target_proto;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, target_proto,
                                   JSReceiver::GetPrototype(isolate, target),
                                   Nothing<bool>());
  // 13. If SameValue(V, targetProto) is false, throw a TypeError exception.
  if (bool_trap_result && !value->SameValue(*target_proto)) {
    isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kProxySetPrototypeOfNonExtensible));
    return Nothing<bool>();
  }
  // 14. Return true.
  return Just(true);
}


Maybe<bool> JSObject::SetPrototype(Handle<JSObject> object,
                                   Handle<Object> value, bool from_javascript,
                                   ShouldThrow should_throw) {
  Isolate* isolate = object->GetIsolate();

  // Setting the prototype of an Array instance invalidates the species
  // protector
  // because it could change the constructor property of the instance, which
  // could change the @@species constructor.
  if (object->IsJSArray() && isolate->IsArraySpeciesLookupChainIntact()) {
    isolate->CountUsage(
        v8::Isolate::UseCounterFeature::kArrayInstanceProtoModified);
    isolate->InvalidateArraySpeciesProtector();
  }

  const bool observed = from_javascript && object->map()->is_observed();
  Handle<Object> old_value;
  if (observed) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, old_value,
                                     JSReceiver::GetPrototype(isolate, object),
                                     Nothing<bool>());
  }

  Maybe<bool> result =
      SetPrototypeUnobserved(object, value, from_javascript, should_throw);
  MAYBE_RETURN(result, Nothing<bool>());

  if (result.FromJust() && observed) {
    Handle<Object> new_value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, new_value,
                                     JSReceiver::GetPrototype(isolate, object),
                                     Nothing<bool>());
    if (!new_value->SameValue(*old_value)) {
      RETURN_ON_EXCEPTION_VALUE(
          isolate, JSObject::EnqueueChangeRecord(
                       object, "setPrototype",
                       isolate->factory()->proto_string(), old_value),
          Nothing<bool>());
    }
  }

  return result;
}


Maybe<bool> JSObject::SetPrototypeUnobserved(Handle<JSObject> object,
                                             Handle<Object> value,
                                             bool from_javascript,
                                             ShouldThrow should_throw) {
#ifdef DEBUG
  int size = object->Size();
#endif

  Isolate* isolate = object->GetIsolate();

  if (from_javascript) {
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayAccess(handle(isolate->context()), object)) {
      isolate->ReportFailedAccessCheck(object);
      RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
      RETURN_FAILURE(isolate, should_throw,
                     NewTypeError(MessageTemplate::kNoAccess));
    }
  } else {
    DCHECK(!object->IsAccessCheckNeeded());
  }

  // Strong objects may not have their prototype set via __proto__ or
  // setPrototypeOf.
  if (from_javascript && object->map()->is_strong()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kStrongSetProto, object));
  }
  Heap* heap = isolate->heap();
  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull()) return Just(true);

  bool dictionary_elements_in_chain =
      object->map()->DictionaryElementsInPrototypeChainOnly();

  bool all_extensible = object->map()->is_extensible();
  Handle<JSObject> real_receiver = object;
  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden.
    PrototypeIterator iter(isolate, real_receiver,
                           PrototypeIterator::START_AT_PROTOTYPE,
                           PrototypeIterator::END_AT_NON_HIDDEN);
    while (!iter.IsAtEnd()) {
      // Casting to JSObject is fine because hidden prototypes are never
      // JSProxies.
      real_receiver = PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
      all_extensible = all_extensible && real_receiver->map()->is_extensible();
    }
  }
  Handle<Map> map(real_receiver->map());

  // Nothing to do if prototype is already set.
  if (map->prototype() == *value) return Just(true);

  // From 8.6.2 Object Internal Methods
  // ...
  // In addition, if [[Extensible]] is false the value of the [[Class]] and
  // [[Prototype]] internal properties of the object may not be modified.
  // ...
  // Implementation specific extensions that modify [[Class]], [[Prototype]]
  // or [[Extensible]] must not violate the invariants defined in the preceding
  // paragraph.
  if (!all_extensible) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kNonExtensibleProto, object));
  }

  // Before we can set the prototype we need to be sure prototype cycles are
  // prevented.  It is sufficient to validate that the receiver is not in the
  // new prototype chain.
  if (value->IsJSReceiver()) {
    for (PrototypeIterator iter(isolate, JSReceiver::cast(*value),
                                PrototypeIterator::START_AT_RECEIVER);
         !iter.IsAtEnd(); iter.Advance()) {
      if (iter.GetCurrent<JSReceiver>() == *object) {
        // Cycle detected.
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kCyclicProto));
      }
    }
  }

  // Set the new prototype of the object.

  isolate->UpdateArrayProtectorOnSetPrototype(real_receiver);

  PrototypeOptimizationMode mode =
      from_javascript ? REGULAR_PROTOTYPE : FAST_PROTOTYPE;
  Handle<Map> new_map = Map::TransitionToPrototype(map, value, mode);
  DCHECK(new_map->prototype() == *value);
  JSObject::MigrateToMap(real_receiver, new_map);

  if (from_javascript && !dictionary_elements_in_chain &&
      new_map->DictionaryElementsInPrototypeChainOnly()) {
    // If the prototype chain didn't previously have element callbacks, then
    // KeyedStoreICs need to be cleared to ensure any that involve this
    // map go generic.
    TypeFeedbackVector::ClearAllKeyedStoreICs(isolate);
  }

  heap->ClearInstanceofCache();
  DCHECK(size == object->Size());
  return Just(true);
}


void JSObject::EnsureCanContainElements(Handle<JSObject> object,
                                        Arguments* args,
                                        uint32_t first_arg,
                                        uint32_t arg_count,
                                        EnsureElementsMode mode) {
  // Elements in |Arguments| are ordered backwards (because they're on the
  // stack), but the method that's called here iterates over them in forward
  // direction.
  return EnsureCanContainElements(
      object, args->arguments() - first_arg - (arg_count - 1), arg_count, mode);
}


ElementsAccessor* JSObject::GetElementsAccessor() {
  return ElementsAccessor::ForKind(GetElementsKind());
}


void JSObject::ValidateElements(Handle<JSObject> object) {
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    ElementsAccessor* accessor = object->GetElementsAccessor();
    accessor->Validate(object);
  }
#endif
}


static bool ShouldConvertToSlowElements(JSObject* object, uint32_t capacity,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  STATIC_ASSERT(JSObject::kMaxUncheckedOldFastElementsLength <=
                JSObject::kMaxUncheckedFastElementsLength);
  if (index < capacity) {
    *new_capacity = capacity;
    return false;
  }
  if (index - capacity >= JSObject::kMaxGap) return true;
  *new_capacity = JSObject::NewElementsCapacity(index + 1);
  DCHECK_LT(index, *new_capacity);
  if (*new_capacity <= JSObject::kMaxUncheckedOldFastElementsLength ||
      (*new_capacity <= JSObject::kMaxUncheckedFastElementsLength &&
       object->GetHeap()->InNewSpace(object))) {
    return false;
  }
  // If the fast-case backing storage takes up roughly three times as
  // much space (in machine words) as a dictionary backing storage
  // would, the object should have slow elements.
  int used_elements = object->GetFastElementsUsage();
  int dictionary_size = SeededNumberDictionary::ComputeCapacity(used_elements) *
                        SeededNumberDictionary::kEntrySize;
  return 3 * static_cast<uint32_t>(dictionary_size) <= *new_capacity;
}


bool JSObject::WouldConvertToSlowElements(uint32_t index) {
  if (HasFastElements()) {
    Handle<FixedArrayBase> backing_store(FixedArrayBase::cast(elements()));
    uint32_t capacity = static_cast<uint32_t>(backing_store->length());
    uint32_t new_capacity;
    return ShouldConvertToSlowElements(this, capacity, index, &new_capacity);
  }
  return false;
}


static ElementsKind BestFittingFastElementsKind(JSObject* object) {
  if (object->HasSloppyArgumentsElements()) {
    return FAST_SLOPPY_ARGUMENTS_ELEMENTS;
  }
  if (object->HasStringWrapperElements()) {
    return FAST_STRING_WRAPPER_ELEMENTS;
  }
  DCHECK(object->HasDictionaryElements());
  SeededNumberDictionary* dictionary = object->element_dictionary();
  ElementsKind kind = FAST_HOLEY_SMI_ELEMENTS;
  for (int i = 0; i < dictionary->Capacity(); i++) {
    Object* key = dictionary->KeyAt(i);
    if (key->IsNumber()) {
      Object* value = dictionary->ValueAt(i);
      if (!value->IsNumber()) return FAST_HOLEY_ELEMENTS;
      if (!value->IsSmi()) {
        if (!FLAG_unbox_double_arrays) return FAST_HOLEY_ELEMENTS;
        kind = FAST_HOLEY_DOUBLE_ELEMENTS;
      }
    }
  }
  return kind;
}


static bool ShouldConvertToFastElements(JSObject* object,
                                        SeededNumberDictionary* dictionary,
                                        uint32_t index,
                                        uint32_t* new_capacity) {
  // If properties with non-standard attributes or accessors were added, we
  // cannot go back to fast elements.
  if (dictionary->requires_slow_elements()) return false;

  // Adding a property with this index will require slow elements.
  if (index >= static_cast<uint32_t>(Smi::kMaxValue)) return false;

  if (object->IsJSArray()) {
    Object* length = JSArray::cast(object)->length();
    if (!length->IsSmi()) return false;
    *new_capacity = static_cast<uint32_t>(Smi::cast(length)->value());
  } else {
    *new_capacity = dictionary->max_number_key() + 1;
  }
  *new_capacity = Max(index + 1, *new_capacity);

  uint32_t dictionary_size = static_cast<uint32_t>(dictionary->Capacity()) *
                             SeededNumberDictionary::kEntrySize;

  // Turn fast if the dictionary only saves 50% space.
  return 2 * dictionary_size >= *new_capacity;
}


// static
MaybeHandle<Object> JSObject::AddDataElement(Handle<JSObject> object,
                                             uint32_t index,
                                             Handle<Object> value,
                                             PropertyAttributes attributes) {
  MAYBE_RETURN_NULL(
      AddDataElement(object, index, value, attributes, THROW_ON_ERROR));
  return value;
}


// static
Maybe<bool> JSObject::AddDataElement(Handle<JSObject> object, uint32_t index,
                                     Handle<Object> value,
                                     PropertyAttributes attributes,
                                     ShouldThrow should_throw) {
  DCHECK(object->map()->is_extensible());

  Isolate* isolate = object->GetIsolate();

  uint32_t old_length = 0;
  uint32_t new_capacity = 0;

  Handle<Object> old_length_handle;
  if (object->IsJSArray()) {
    CHECK(JSArray::cast(*object)->length()->ToArrayLength(&old_length));
    if (object->map()->is_observed()) {
      old_length_handle = handle(JSArray::cast(*object)->length(), isolate);
    }
  }

  ElementsKind kind = object->GetElementsKind();
  FixedArrayBase* elements = object->elements();
  ElementsKind dictionary_kind = DICTIONARY_ELEMENTS;
  if (IsSloppyArgumentsElements(kind)) {
    elements = FixedArrayBase::cast(FixedArray::cast(elements)->get(1));
    dictionary_kind = SLOW_SLOPPY_ARGUMENTS_ELEMENTS;
  } else if (IsStringWrapperElementsKind(kind)) {
    dictionary_kind = SLOW_STRING_WRAPPER_ELEMENTS;
  }

  if (attributes != NONE) {
    kind = dictionary_kind;
  } else if (elements->IsSeededNumberDictionary()) {
    kind = ShouldConvertToFastElements(*object,
                                       SeededNumberDictionary::cast(elements),
                                       index, &new_capacity)
               ? BestFittingFastElementsKind(*object)
               : dictionary_kind;  // Overwrite in case of arguments.
  } else if (ShouldConvertToSlowElements(
                 *object, static_cast<uint32_t>(elements->length()), index,
                 &new_capacity)) {
    kind = dictionary_kind;
  }

  ElementsKind to = value->OptimalElementsKind();
  if (IsHoleyElementsKind(kind) || !object->IsJSArray() || index > old_length) {
    to = GetHoleyElementsKind(to);
    kind = GetHoleyElementsKind(kind);
  }
  to = GetMoreGeneralElementsKind(kind, to);
  ElementsAccessor* accessor = ElementsAccessor::ForKind(to);
  accessor->Add(object, index, value, attributes, new_capacity);

  uint32_t new_length = old_length;
  Handle<Object> new_length_handle;
  if (object->IsJSArray() && index >= old_length) {
    new_length = index + 1;
    new_length_handle = isolate->factory()->NewNumberFromUint(new_length);
    JSArray::cast(*object)->set_length(*new_length_handle);
  }

  if (!old_length_handle.is_null() && new_length != old_length) {
    // |old_length_handle| is kept null above unless the object is observed.
    DCHECK(object->map()->is_observed());
    Handle<JSArray> array = Handle<JSArray>::cast(object);
    Handle<String> name = isolate->factory()->Uint32ToString(index);

    RETURN_ON_EXCEPTION_VALUE(isolate, BeginPerformSplice(array),
                              Nothing<bool>());
    RETURN_ON_EXCEPTION_VALUE(
        isolate, EnqueueChangeRecord(array, "add", name,
                                     isolate->factory()->the_hole_value()),
        Nothing<bool>());
    RETURN_ON_EXCEPTION_VALUE(
        isolate, EnqueueChangeRecord(array, "update",
                                     isolate->factory()->length_string(),
                                     old_length_handle),
        Nothing<bool>());
    RETURN_ON_EXCEPTION_VALUE(isolate, EndPerformSplice(array),
                              Nothing<bool>());
    Handle<JSArray> deleted = isolate->factory()->NewJSArray(0);
    RETURN_ON_EXCEPTION_VALUE(isolate,
                              EnqueueSpliceRecord(array, old_length, deleted,
                                                  new_length - old_length),
                              Nothing<bool>());
  } else if (object->map()->is_observed()) {
    Handle<String> name = isolate->factory()->Uint32ToString(index);
    RETURN_ON_EXCEPTION_VALUE(
        isolate, EnqueueChangeRecord(object, "add", name,
                                     isolate->factory()->the_hole_value()),
        Nothing<bool>());
  }

  return Just(true);
}


bool JSArray::SetLengthWouldNormalize(uint32_t new_length) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements()->length());
  uint32_t new_capacity;
  return JSArray::SetLengthWouldNormalize(GetHeap(), new_length) &&
         ShouldConvertToSlowElements(this, capacity, new_length - 1,
                                     &new_capacity);
}


const double AllocationSite::kPretenureRatio = 0.85;


void AllocationSite::ResetPretenureDecision() {
  set_pretenure_decision(kUndecided);
  set_memento_found_count(0);
  set_memento_create_count(0);
}


PretenureFlag AllocationSite::GetPretenureMode() {
  PretenureDecision mode = pretenure_decision();
  // Zombie objects "decide" to be untenured.
  return mode == kTenure ? TENURED : NOT_TENURED;
}


bool AllocationSite::IsNestedSite() {
  DCHECK(FLAG_trace_track_allocation_sites);
  Object* current = GetHeap()->allocation_sites_list();
  while (current->IsAllocationSite()) {
    AllocationSite* current_site = AllocationSite::cast(current);
    if (current_site->nested_site() == this) {
      return true;
    }
    current = current_site->weak_next();
  }
  return false;
}


void AllocationSite::DigestTransitionFeedback(Handle<AllocationSite> site,
                                              ElementsKind to_kind) {
  Isolate* isolate = site->GetIsolate();

  if (site->SitePointsToLiteral() && site->transition_info()->IsJSArray()) {
    Handle<JSArray> transition_info =
        handle(JSArray::cast(site->transition_info()));
    ElementsKind kind = transition_info->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      // If the array is huge, it's not likely to be defined in a local
      // function, so we shouldn't make new instances of it very often.
      uint32_t length = 0;
      CHECK(transition_info->length()->ToArrayLength(&length));
      if (length <= kMaximumArrayBytesToPretransition) {
        if (FLAG_trace_track_allocation_sites) {
          bool is_nested = site->IsNestedSite();
          PrintF(
              "AllocationSite: JSArray %p boilerplate %s updated %s->%s\n",
              reinterpret_cast<void*>(*site),
              is_nested ? "(nested)" : "",
              ElementsKindToString(kind),
              ElementsKindToString(to_kind));
        }
        JSObject::TransitionElementsKind(transition_info, to_kind);
        site->dependent_code()->DeoptimizeDependentCodeGroup(
            isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
      }
    }
  } else {
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      if (FLAG_trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(*site),
               ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->SetElementsKind(to_kind);
      site->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
    }
  }
}


const char* AllocationSite::PretenureDecisionName(PretenureDecision decision) {
  switch (decision) {
    case kUndecided: return "undecided";
    case kDontTenure: return "don't tenure";
    case kMaybeTenure: return "maybe tenure";
    case kTenure: return "tenure";
    case kZombie: return "zombie";
    default: UNREACHABLE();
  }
  return NULL;
}


void JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return;

  Heap* heap = object->GetHeap();
  if (!heap->InNewSpace(*object)) return;

  Handle<AllocationSite> site;
  {
    DisallowHeapAllocation no_allocation;

    AllocationMemento* memento =
        heap->FindAllocationMemento<Heap::kForRuntime>(*object);
    if (memento == NULL) return;

    // Walk through to the Allocation Site
    site = handle(memento->GetAllocationSite());
  }
  AllocationSite::DigestTransitionFeedback(site, to_kind);
}


void JSObject::TransitionElementsKind(Handle<JSObject> object,
                                      ElementsKind to_kind) {
  ElementsKind from_kind = object->GetElementsKind();

  if (IsFastHoleyElementsKind(from_kind)) {
    to_kind = GetHoleyElementsKind(to_kind);
  }

  if (from_kind == to_kind) return;

  // This method should never be called for any other case.
  DCHECK(IsFastElementsKind(from_kind));
  DCHECK(IsFastElementsKind(to_kind));
  DCHECK_NE(TERMINAL_FAST_ELEMENTS_KIND, from_kind);

  UpdateAllocationSite(object, to_kind);
  if (object->elements() == object->GetHeap()->empty_fixed_array() ||
      IsFastDoubleElementsKind(from_kind) ==
          IsFastDoubleElementsKind(to_kind)) {
    // No change is needed to the elements() buffer, the transition
    // only requires a map change.
    Handle<Map> new_map = GetElementsTransitionMap(object, to_kind);
    MigrateToMap(object, new_map);
    if (FLAG_trace_elements_transitions) {
      Handle<FixedArrayBase> elms(object->elements());
      PrintElementsTransition(stdout, object, from_kind, elms, to_kind, elms);
    }
  } else {
    DCHECK((IsFastSmiElementsKind(from_kind) &&
            IsFastDoubleElementsKind(to_kind)) ||
           (IsFastDoubleElementsKind(from_kind) &&
            IsFastObjectElementsKind(to_kind)));
    uint32_t c = static_cast<uint32_t>(object->elements()->length());
    ElementsAccessor::ForKind(to_kind)->GrowCapacityAndConvert(object, c);
  }
}


// static
bool Map::IsValidElementsTransition(ElementsKind from_kind,
                                    ElementsKind to_kind) {
  // Transitions can't go backwards.
  if (!IsMoreGeneralElementsKindTransition(from_kind, to_kind)) {
    return false;
  }

  // Transitions from HOLEY -> PACKED are not allowed.
  return !IsFastHoleyElementsKind(from_kind) ||
      IsFastHoleyElementsKind(to_kind);
}


bool JSArray::HasReadOnlyLength(Handle<JSArray> array) {
  LookupIterator it(array, array->GetIsolate()->factory()->length_string(),
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_NE(LookupIterator::ACCESS_CHECK, it.state());
  CHECK(it.IsFound());
  CHECK_EQ(LookupIterator::ACCESSOR, it.state());
  return it.IsReadOnly();
}


bool JSArray::WouldChangeReadOnlyLength(Handle<JSArray> array,
                                        uint32_t index) {
  uint32_t length = 0;
  CHECK(array->length()->ToArrayLength(&length));
  if (length <= index) return HasReadOnlyLength(array);
  return false;
}


template <typename BackingStore>
static int FastHoleyElementsUsage(JSObject* object, BackingStore* store) {
  int limit = object->IsJSArray()
                  ? Smi::cast(JSArray::cast(object)->length())->value()
                  : store->length();
  int used = 0;
  for (int i = 0; i < limit; ++i) {
    if (!store->is_the_hole(i)) ++used;
  }
  return used;
}


int JSObject::GetFastElementsUsage() {
  FixedArrayBase* store = elements();
  switch (GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_ELEMENTS:
      return IsJSArray() ? Smi::cast(JSArray::cast(this)->length())->value()
                         : store->length();
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
      store = FixedArray::cast(FixedArray::cast(store)->get(1));
    // Fall through.
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS:
      return FastHoleyElementsUsage(this, FixedArray::cast(store));
    case FAST_HOLEY_DOUBLE_ELEMENTS:
      if (elements()->length() == 0) return 0;
      return FastHoleyElementsUsage(this, FixedDoubleArray::cast(store));

    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NO_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                      \
    case TYPE##_ELEMENTS:                                                    \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    UNREACHABLE();
  }
  return 0;
}


// Certain compilers request function template instantiation when they
// see the definition of the other template functions in the
// class. This requires us to have the template functions put
// together, so even though this function belongs in objects-debug.cc,
// we keep it here instead to satisfy certain compilers.
#ifdef OBJECT_PRINT
template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::Print(std::ostream& os) {  // NOLINT
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k)) {
      os << "\n   ";
      if (k->IsString()) {
        String::cast(k)->StringPrint(os);
      } else {
        os << Brief(k);
      }
      os << ": " << Brief(this->ValueAt(i)) << " " << this->DetailsAt(i);
    }
  }
}
#endif


template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CopyValuesTo(FixedArray* elements) {
  int pos = 0;
  int capacity = this->Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k)) {
      elements->set(pos++, this->ValueAt(i), mode);
    }
  }
  DCHECK(pos == elements->length());
}


InterceptorInfo* JSObject::GetNamedInterceptor() {
  DCHECK(map()->has_named_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->GetConstructor());
  DCHECK(constructor->shared()->IsApiFunction());
  Object* result =
      constructor->shared()->get_api_func_data()->named_property_handler();
  return InterceptorInfo::cast(result);
}


MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(LookupIterator* it,
                                                         bool* done) {
  *done = false;
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor = it->GetInterceptor();
  if (interceptor->getter()->IsUndefined()) {
    return isolate->factory()->undefined_value();
  }

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  v8::Local<v8::Value> result;
  PropertyCallbackArguments args(isolate, interceptor->data(),
                                 *it->GetReceiver(), *holder,
                                 Object::DONT_THROW);

  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-get", *holder, index));
    result = args.Call(getter, index);
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());

    if (name->IsSymbol() && !interceptor->can_intercept_symbols()) {
      return isolate->factory()->undefined_value();
    }

    v8::GenericNamedPropertyGetterCallback getter =
        v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
            interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get", *holder, *name));
    result = args.Call(getter, v8::Utils::ToLocal(name));
  }

  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.IsEmpty()) return isolate->factory()->undefined_value();
  Handle<Object> result_internal = v8::Utils::OpenHandle(*result);
  result_internal->VerifyApiCallResultType();
  *done = true;
  // Rebox handle before return
  return handle(*result_internal, isolate);
}


Maybe<bool> JSObject::HasRealNamedProperty(Handle<JSObject> object,
                                           Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}


Maybe<bool> JSObject::HasRealElementProperty(Handle<JSObject> object,
                                             uint32_t index) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  return HasProperty(&it);
}


Maybe<bool> JSObject::HasRealNamedCallbackProperty(Handle<JSObject> object,
                                                   Handle<Name> name) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe_result = GetPropertyAttributes(&it);
  return maybe_result.IsJust() ? Just(it.state() == LookupIterator::ACCESSOR)
                               : Nothing<bool>();
}


void FixedArray::SwapPairs(FixedArray* numbers, int i, int j) {
  Object* temp = get(i);
  set(i, get(j));
  set(j, temp);
  if (this != numbers) {
    temp = numbers->get(i);
    numbers->set(i, Smi::cast(numbers->get(j)));
    numbers->set(j, Smi::cast(temp));
  }
}


static void InsertionSortPairs(FixedArray* content,
                               FixedArray* numbers,
                               int len) {
  for (int i = 1; i < len; i++) {
    int j = i;
    while (j > 0 &&
           (NumberToUint32(numbers->get(j - 1)) >
            NumberToUint32(numbers->get(j)))) {
      content->SwapPairs(numbers, j - 1, j);
      j--;
    }
  }
}


void HeapSortPairs(FixedArray* content, FixedArray* numbers, int len) {
  // In-place heap sort.
  DCHECK(content->length() == numbers->length());

  // Bottom-up max-heap construction.
  for (int i = 1; i < len; ++i) {
    int child_index = i;
    while (child_index > 0) {
      int parent_index = ((child_index + 1) >> 1) - 1;
      uint32_t parent_value = NumberToUint32(numbers->get(parent_index));
      uint32_t child_value = NumberToUint32(numbers->get(child_index));
      if (parent_value < child_value) {
        content->SwapPairs(numbers, parent_index, child_index);
      } else {
        break;
      }
      child_index = parent_index;
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    content->SwapPairs(numbers, 0, i);
    // Sift down the new top element.
    int parent_index = 0;
    while (true) {
      int child_index = ((parent_index + 1) << 1) - 1;
      if (child_index >= i) break;
      uint32_t child1_value = NumberToUint32(numbers->get(child_index));
      uint32_t child2_value = NumberToUint32(numbers->get(child_index + 1));
      uint32_t parent_value = NumberToUint32(numbers->get(parent_index));
      if (child_index + 1 >= i || child1_value > child2_value) {
        if (parent_value > child1_value) break;
        content->SwapPairs(numbers, parent_index, child_index);
        parent_index = child_index;
      } else {
        if (parent_value > child2_value) break;
        content->SwapPairs(numbers, parent_index, child_index + 1);
        parent_index = child_index + 1;
      }
    }
  }
}


// Sort this array and the numbers as pairs wrt. the (distinct) numbers.
void FixedArray::SortPairs(FixedArray* numbers, uint32_t len) {
  DCHECK(this->length() == numbers->length());
  // For small arrays, simply use insertion sort.
  if (len <= 10) {
    InsertionSortPairs(this, numbers, len);
    return;
  }
  // Check the range of indices.
  uint32_t min_index = NumberToUint32(numbers->get(0));
  uint32_t max_index = min_index;
  uint32_t i;
  for (i = 1; i < len; i++) {
    if (NumberToUint32(numbers->get(i)) < min_index) {
      min_index = NumberToUint32(numbers->get(i));
    } else if (NumberToUint32(numbers->get(i)) > max_index) {
      max_index = NumberToUint32(numbers->get(i));
    }
  }
  if (max_index - min_index + 1 == len) {
    // Indices form a contiguous range, unless there are duplicates.
    // Do an in-place linear time sort assuming distinct numbers, but
    // avoid hanging in case they are not.
    for (i = 0; i < len; i++) {
      uint32_t p;
      uint32_t j = 0;
      // While the current element at i is not at its correct position p,
      // swap the elements at these two positions.
      while ((p = NumberToUint32(numbers->get(i)) - min_index) != i &&
             j++ < len) {
        SwapPairs(numbers, i, p);
      }
    }
  } else {
    HeapSortPairs(this, numbers, len);
    return;
  }
}


void JSObject::CollectOwnPropertyNames(KeyAccumulator* keys,
                                       PropertyFilter filter) {
  if (HasFastProperties()) {
    int real_size = map()->NumberOfOwnDescriptors();
    Handle<DescriptorArray> descs(map()->instance_descriptors());
    for (int i = 0; i < real_size; i++) {
      PropertyDetails details = descs->GetDetails(i);
      if ((details.attributes() & filter) != 0) continue;
      if (filter & ONLY_ALL_CAN_READ) {
        if (details.kind() != kAccessor) continue;
        Object* accessors = descs->GetValue(i);
        if (!accessors->IsAccessorInfo()) continue;
        if (!AccessorInfo::cast(accessors)->all_can_read()) continue;
      }
      Name* key = descs->GetKey(i);
      if (key->FilterKey(filter)) continue;
      keys->AddKey(key, DO_NOT_CONVERT);
    }
  } else if (IsJSGlobalObject()) {
    GlobalDictionary::CollectKeysTo(handle(global_dictionary()), keys, filter);
  } else {
    NameDictionary::CollectKeysTo(handle(property_dictionary()), keys, filter);
  }
}


int JSObject::NumberOfOwnElements(PropertyFilter filter) {
  // Fast case for objects with no elements.
  if (!IsJSValue() && HasFastElements()) {
    uint32_t length =
        IsJSArray()
            ? static_cast<uint32_t>(
                  Smi::cast(JSArray::cast(this)->length())->value())
            : static_cast<uint32_t>(FixedArrayBase::cast(elements())->length());
    if (length == 0) return 0;
  }
  // Compute the number of enumerable elements.
  return GetOwnElementKeys(NULL, filter);
}


void JSObject::CollectOwnElementKeys(Handle<JSObject> object,
                                     KeyAccumulator* keys,
                                     PropertyFilter filter) {
  if (filter & SKIP_STRINGS) return;
  ElementsAccessor* accessor = object->GetElementsAccessor();
  accessor->CollectElementIndices(object, keys, kMaxUInt32, filter, 0);
}


int JSObject::GetOwnElementKeys(FixedArray* storage, PropertyFilter filter) {
  int counter = 0;

  // If this is a String wrapper, add the string indices first,
  // as they're guaranteed to precede the elements in numerical order
  // and ascending order is required by ECMA-262, 6th, 9.1.12.
  if (IsJSValue()) {
    Object* val = JSValue::cast(this)->value();
    if (val->IsString()) {
      String* str = String::cast(val);
      if (storage) {
        for (int i = 0; i < str->length(); i++) {
          storage->set(counter + i, Smi::FromInt(i));
        }
      }
      counter += str->length();
    }
  }

  switch (GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_STRING_WRAPPER_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArray::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedArray::cast(elements())->get(i)->IsTheHole()) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i));
          }
          counter++;
        }
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArrayBase::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedDoubleArray::cast(elements())->is_the_hole(i)) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i));
          }
          counter++;
        }
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                      \
    case TYPE##_ELEMENTS:                                                    \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    {
      int length = FixedArrayBase::cast(elements())->length();
      while (counter < length) {
        if (storage != NULL) {
          storage->set(counter, Smi::FromInt(counter));
        }
        counter++;
      }
      DCHECK(!storage || storage->length() >= counter);
      break;
    }

    case DICTIONARY_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS: {
      if (storage != NULL) {
        element_dictionary()->CopyKeysTo(storage, counter, filter,
                                         SeededNumberDictionary::SORTED);
      }
      counter += element_dictionary()->NumberOfElementsFilterAttributes(filter);
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS: {
      FixedArray* parameter_map = FixedArray::cast(elements());
      int mapped_length = parameter_map->length() - 2;
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        // Copy the keys from arguments first, because Dictionary::CopyKeysTo
        // will insert in storage starting at index 0.
        SeededNumberDictionary* dictionary =
            SeededNumberDictionary::cast(arguments);
        if (storage != NULL) {
          dictionary->CopyKeysTo(storage, counter, filter,
                                 SeededNumberDictionary::UNSORTED);
        }
        counter += dictionary->NumberOfElementsFilterAttributes(filter);
        for (int i = 0; i < mapped_length; ++i) {
          if (!parameter_map->get(i + 2)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          }
        }
        if (storage != NULL) storage->SortPairs(storage, counter);

      } else {
        int backing_length = arguments->length();
        int i = 0;
        for (; i < mapped_length; ++i) {
          if (!parameter_map->get(i + 2)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          } else if (i < backing_length && !arguments->get(i)->IsTheHole()) {
            if (storage != NULL) storage->set(counter, Smi::FromInt(i));
            ++counter;
          }
        }
        for (; i < backing_length; ++i) {
          if (storage != NULL) storage->set(counter, Smi::FromInt(i));
          ++counter;
        }
      }
      break;
    }
    case NO_ELEMENTS:
      break;
  }

  DCHECK(!storage || storage->length() == counter);
  return counter;
}


MaybeHandle<String> Object::ObjectProtoToString(Isolate* isolate,
                                                Handle<Object> object) {
  if (object->IsUndefined()) return isolate->factory()->undefined_to_string();
  if (object->IsNull()) return isolate->factory()->null_to_string();

  Handle<JSReceiver> receiver =
      Object::ToObject(isolate, object).ToHandleChecked();

  Handle<String> tag;
  if (FLAG_harmony_tostring) {
    Handle<Object> to_string_tag;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, to_string_tag,
        GetProperty(receiver, isolate->factory()->to_string_tag_symbol()),
        String);
    if (to_string_tag->IsString()) {
      tag = Handle<String>::cast(to_string_tag);
    }
  }

  if (tag.is_null()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, tag,
                               JSReceiver::BuiltinStringTag(receiver), String);
  }

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("[object ");
  builder.AppendString(tag);
  builder.AppendCharacter(']');
  return builder.Finish();
}


const char* Symbol::PrivateSymbolToName() const {
  Heap* heap = GetIsolate()->heap();
#define SYMBOL_CHECK_AND_PRINT(name) \
  if (this == heap->name()) return #name;
  PRIVATE_SYMBOL_LIST(SYMBOL_CHECK_AND_PRINT)
#undef SYMBOL_CHECK_AND_PRINT
  return "UNKNOWN";
}


void Symbol::SymbolShortPrint(std::ostream& os) {
  os << "<Symbol: " << Hash();
  if (!name()->IsUndefined()) {
    os << " ";
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(name())->StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
  } else {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << ">";
}


// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  StringSharedKey(Handle<String> source, Handle<SharedFunctionInfo> shared,
                  LanguageMode language_mode, int scope_position)
      : source_(source),
        shared_(shared),
        language_mode_(language_mode),
        scope_position_(scope_position) {}

  bool IsMatch(Object* other) override {
    DisallowHeapAllocation no_allocation;
    if (!other->IsFixedArray()) {
      if (!other->IsNumber()) return false;
      uint32_t other_hash = static_cast<uint32_t>(other->Number());
      return Hash() == other_hash;
    }
    FixedArray* other_array = FixedArray::cast(other);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    if (shared != *shared_) return false;
    int language_unchecked = Smi::cast(other_array->get(2))->value();
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int scope_position = Smi::cast(other_array->get(3))->value();
    if (scope_position != scope_position_) return false;
    String* source = String::cast(other_array->get(1));
    return source->Equals(*source_);
  }

  static uint32_t StringSharedHashHelper(String* source,
                                         SharedFunctionInfo* shared,
                                         LanguageMode language_mode,
                                         int scope_position) {
    uint32_t hash = source->Hash();
    if (shared->HasSourceCode()) {
      // Instead of using the SharedFunctionInfo pointer in the hash
      // code computation, we use a combination of the hash of the
      // script source code and the start position of the calling scope.
      // We do this to ensure that the cache entries can survive garbage
      // collection.
      Script* script(Script::cast(shared->script()));
      hash ^= String::cast(script->source())->Hash();
      STATIC_ASSERT(LANGUAGE_END == 3);
      if (is_strict(language_mode)) hash ^= 0x8000;
      if (is_strong(language_mode)) hash ^= 0x10000;
      hash += scope_position;
    }
    return hash;
  }

  uint32_t Hash() override {
    return StringSharedHashHelper(*source_, *shared_, language_mode_,
                                  scope_position_);
  }

  uint32_t HashForObject(Object* obj) override {
    DisallowHeapAllocation no_allocation;
    if (obj->IsNumber()) {
      return static_cast<uint32_t>(obj->Number());
    }
    FixedArray* other_array = FixedArray::cast(obj);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(other_array->get(0));
    String* source = String::cast(other_array->get(1));
    int language_unchecked = Smi::cast(other_array->get(2))->value();
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    int scope_position = Smi::cast(other_array->get(3))->value();
    return StringSharedHashHelper(source, shared, language_mode,
                                  scope_position);
  }


  Handle<Object> AsHandle(Isolate* isolate) override {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromInt(language_mode_));
    array->set(3, Smi::FromInt(scope_position_));
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int scope_position_;
};


namespace {

JSRegExp::Flags RegExpFlagsFromString(Handle<String> flags, bool* success) {
  JSRegExp::Flags value = JSRegExp::kNone;
  int length = flags->length();
  // A longer flags string cannot be valid.
  if (length > 5) return JSRegExp::Flags(0);
  for (int i = 0; i < length; i++) {
    JSRegExp::Flag flag = JSRegExp::kNone;
    switch (flags->Get(i)) {
      case 'g':
        flag = JSRegExp::kGlobal;
        break;
      case 'i':
        flag = JSRegExp::kIgnoreCase;
        break;
      case 'm':
        flag = JSRegExp::kMultiline;
        break;
      case 'u':
        if (!FLAG_harmony_unicode_regexps) return JSRegExp::Flags(0);
        flag = JSRegExp::kUnicode;
        break;
      case 'y':
        if (!FLAG_harmony_regexps) return JSRegExp::Flags(0);
        flag = JSRegExp::kSticky;
        break;
      default:
        return JSRegExp::Flags(0);
    }
    // Duplicate flag.
    if (value & flag) return JSRegExp::Flags(0);
    value |= flag;
  }
  *success = true;
  return value;
}

}  // namespace


// static
MaybeHandle<JSRegExp> JSRegExp::New(Handle<String> pattern, Flags flags) {
  Isolate* isolate = pattern->GetIsolate();
  Handle<JSFunction> constructor = isolate->regexp_function();
  Handle<JSRegExp> regexp =
      Handle<JSRegExp>::cast(isolate->factory()->NewJSObject(constructor));

  return JSRegExp::Initialize(regexp, pattern, flags);
}


// static
MaybeHandle<JSRegExp> JSRegExp::New(Handle<String> pattern,
                                    Handle<String> flags_string) {
  Isolate* isolate = pattern->GetIsolate();
  bool success = false;
  Flags flags = RegExpFlagsFromString(flags_string, &success);
  if (!success) {
    THROW_NEW_ERROR(
        isolate,
        NewSyntaxError(MessageTemplate::kInvalidRegExpFlags, flags_string),
        JSRegExp);
  }
  return New(pattern, flags);
}


// static
Handle<JSRegExp> JSRegExp::Copy(Handle<JSRegExp> regexp) {
  Isolate* const isolate = regexp->GetIsolate();
  return Handle<JSRegExp>::cast(isolate->factory()->CopyJSObject(regexp));
}


template <typename Char>
inline int CountRequiredEscapes(Handle<String> source) {
  DisallowHeapAllocation no_gc;
  int escapes = 0;
  Vector<const Char> src = source->GetCharVector<Char>();
  for (int i = 0; i < src.length(); i++) {
    if (src[i] == '/' && (i == 0 || src[i - 1] != '\\')) escapes++;
  }
  return escapes;
}


template <typename Char, typename StringType>
inline Handle<StringType> WriteEscapedRegExpSource(Handle<String> source,
                                                   Handle<StringType> result) {
  DisallowHeapAllocation no_gc;
  Vector<const Char> src = source->GetCharVector<Char>();
  Vector<Char> dst(result->GetChars(), result->length());
  int s = 0;
  int d = 0;
  while (s < src.length()) {
    if (src[s] == '/' && (s == 0 || src[s - 1] != '\\')) dst[d++] = '\\';
    dst[d++] = src[s++];
  }
  DCHECK_EQ(result->length(), d);
  return result;
}


MaybeHandle<String> EscapeRegExpSource(Isolate* isolate,
                                       Handle<String> source) {
  String::Flatten(source);
  if (source->length() == 0) return isolate->factory()->query_colon_string();
  bool one_byte = source->IsOneByteRepresentationUnderneath();
  int escapes = one_byte ? CountRequiredEscapes<uint8_t>(source)
                         : CountRequiredEscapes<uc16>(source);
  if (escapes == 0) return source;
  int length = source->length() + escapes;
  if (one_byte) {
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawOneByteString(length),
                               String);
    return WriteEscapedRegExpSource<uint8_t>(source, result);
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawTwoByteString(length),
                               String);
    return WriteEscapedRegExpSource<uc16>(source, result);
  }
}


// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source,
                                           Handle<String> flags_string) {
  Isolate* isolate = source->GetIsolate();
  bool success = false;
  Flags flags = RegExpFlagsFromString(flags_string, &success);
  if (!success) {
    THROW_NEW_ERROR(
        isolate,
        NewSyntaxError(MessageTemplate::kInvalidRegExpFlags, flags_string),
        JSRegExp);
  }
  return Initialize(regexp, source, flags);
}


// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source, Flags flags) {
  Isolate* isolate = regexp->GetIsolate();
  Factory* factory = isolate->factory();
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (source->length() == 0) source = factory->query_colon_string();

  Handle<String> escaped_source;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, escaped_source,
                             EscapeRegExpSource(isolate, source), JSRegExp);

  regexp->set_source(*escaped_source);
  regexp->set_flags(Smi::FromInt(flags));

  Map* map = regexp->map();
  Object* constructor = map->GetConstructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex,
                                  Smi::FromInt(0), SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    PropertyAttributes writable =
        static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
    JSObject::SetOwnPropertyIgnoreAttributes(
        regexp, factory->last_index_string(),
        Handle<Smi>(Smi::FromInt(0), isolate), writable)
        .Check();
  }

  RETURN_ON_EXCEPTION(isolate, RegExpImpl::Compile(regexp, source, flags),
                      JSRegExp);

  return regexp;
}


// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : string_(string), flags_(Smi::FromInt(flags)) {}

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object* obj) override {
    FixedArray* val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val->get(JSRegExp::kSourceIndex)))
        && (flags_ == val->get(JSRegExp::kFlagsIndex));
  }

  uint32_t Hash() override { return RegExpHash(*string_, flags_); }

  Handle<Object> AsHandle(Isolate* isolate) override {
    // Plain hash maps, which is where regexp keys are used, don't
    // use this function.
    UNREACHABLE();
    return MaybeHandle<Object>().ToHandleChecked();
  }

  uint32_t HashForObject(Object* obj) override {
    FixedArray* val = FixedArray::cast(obj);
    return RegExpHash(String::cast(val->get(JSRegExp::kSourceIndex)),
                      Smi::cast(val->get(JSRegExp::kFlagsIndex)));
  }

  static uint32_t RegExpHash(String* string, Smi* flags) {
    return string->Hash() + flags->value();
  }

  Handle<String> string_;
  Smi* flags_;
};


Handle<Object> OneByteStringKey::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  return isolate->factory()->NewOneByteInternalizedString(string_, hash_field_);
}


Handle<Object> TwoByteStringKey::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  return isolate->factory()->NewTwoByteInternalizedString(string_, hash_field_);
}


Handle<Object> SeqOneByteSubStringKey::AsHandle(Isolate* isolate) {
  if (hash_field_ == 0) Hash();
  return isolate->factory()->NewOneByteInternalizedSubString(
      string_, from_, length_, hash_field_);
}


bool SeqOneByteSubStringKey::IsMatch(Object* string) {
  Vector<const uint8_t> chars(string_->GetChars() + from_, length_);
  return String::cast(string)->IsOneByteEqualTo(chars);
}


// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey : public HashTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : string_(string) { }

  bool IsMatch(Object* string) override {
    return String::cast(string)->Equals(*string_);
  }

  uint32_t Hash() override { return string_->Hash(); }

  uint32_t HashForObject(Object* other) override {
    return String::cast(other)->Hash();
  }

  Handle<Object> AsHandle(Isolate* isolate) override {
    // Internalize the string if possible.
    MaybeHandle<Map> maybe_map =
        isolate->factory()->InternalizedStringMapForString(string_);
    Handle<Map> map;
    if (maybe_map.ToHandle(&map)) {
      string_->set_map_no_write_barrier(*map);
      DCHECK(string_->IsInternalizedString());
      return string_;
    }
    // Otherwise allocate a new internalized string.
    return isolate->factory()->NewInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

  static uint32_t StringHash(Object* obj) {
    return String::cast(obj)->Hash();
  }

  Handle<String> string_;
};


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::IteratePrefix(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(this, 0, kElementsStartOffset, v);
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::IterateElements(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(this, kElementsStartOffset,
                                      kHeaderSize + length() * kPointerSize, v);
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::New(
    Isolate* isolate,
    int at_least_space_for,
    MinimumCapacity capacity_option,
    PretenureFlag pretenure) {
  DCHECK(0 <= at_least_space_for);
  DCHECK(!capacity_option || base::bits::IsPowerOfTwo32(at_least_space_for));

  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid table size", true);
  }

  Factory* factory = isolate->factory();
  int length = EntryToIndex(capacity);
  Handle<FixedArray> array = factory->NewFixedArray(length, pretenure);
  array->set_map_no_write_barrier(*factory->hash_table_map());
  Handle<Derived> table = Handle<Derived>::cast(array);

  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  table->SetCapacity(capacity);
  return table;
}


// Find entry for key otherwise return kNotFound.
template <typename Derived, typename Shape>
int NameDictionaryBase<Derived, Shape>::FindEntry(Handle<Name> key) {
  if (!key->IsUniqueName()) {
    return DerivedDictionary::FindEntry(key);
  }

  // Optimized for unique names. Knowledge of the key type allows:
  // 1. Move the check if the key is unique out of the loop.
  // 2. Avoid comparing hash codes in unique-to-unique comparison.
  // 3. Detect a case when a dictionary key is not unique but the key is.
  //    In case of positive result the dictionary key may be replaced by the
  //    internalized string with minimal performance penalty. It gives a chance
  //    to perform further lookups in code stubs (and significant performance
  //    boost a certain style of code).

  // EnsureCapacity will guarantee the hash table is never full.
  uint32_t capacity = this->Capacity();
  uint32_t entry = Derived::FirstProbe(key->Hash(), capacity);
  uint32_t count = 1;

  while (true) {
    int index = Derived::EntryToIndex(entry);
    Object* element = this->get(index);
    if (element->IsUndefined()) break;  // Empty entry.
    if (*key == element) return entry;
    if (!element->IsUniqueName() &&
        !element->IsTheHole() &&
        Name::cast(element)->Equals(*key)) {
      // Replace a key that is a non-internalized string by the equivalent
      // internalized string for faster further lookups.
      this->set(index, *key);
      return entry;
    }
    DCHECK(element->IsTheHole() || !Name::cast(element)->Equals(*key));
    entry = Derived::NextProbe(entry, count++, capacity);
  }
  return Derived::kNotFound;
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Rehash(
    Handle<Derived> new_table,
    Key key) {
  DCHECK(NumberOfElements() < new_table->Capacity());

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = new_table->GetWriteBarrierMode(no_gc);

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex;
       i < kPrefixStartIndex + Shape::kPrefixSize;
       i++) {
    new_table->set(i, get(i), mode);
  }

  // Rehash the elements.
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object* k = this->get(from_index);
    if (IsKey(k)) {
      uint32_t hash = this->HashForObject(key, k);
      uint32_t insertion_index =
          EntryToIndex(new_table->FindInsertionEntry(hash));
      for (int j = 0; j < Shape::kEntrySize; j++) {
        new_table->set(insertion_index + j, get(from_index + j), mode);
      }
    }
  }
  new_table->SetNumberOfElements(NumberOfElements());
  new_table->SetNumberOfDeletedElements(0);
}


template<typename Derived, typename Shape, typename Key>
uint32_t HashTable<Derived, Shape, Key>::EntryForProbe(
    Key key,
    Object* k,
    int probe,
    uint32_t expected) {
  uint32_t hash = this->HashForObject(key, k);
  uint32_t capacity = this->Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Swap(uint32_t entry1,
                                          uint32_t entry2,
                                          WriteBarrierMode mode) {
  int index1 = EntryToIndex(entry1);
  int index2 = EntryToIndex(entry2);
  Object* temp[Shape::kEntrySize];
  for (int j = 0; j < Shape::kEntrySize; j++) {
    temp[j] = get(index1 + j);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index1 + j, get(index2 + j), mode);
  }
  for (int j = 0; j < Shape::kEntrySize; j++) {
    set(index2 + j, temp[j], mode);
  }
}


template<typename Derived, typename Shape, typename Key>
void HashTable<Derived, Shape, Key>::Rehash(Key key) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = GetWriteBarrierMode(no_gc);
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (uint32_t current = 0; current < capacity; current++) {
      Object* current_key = get(EntryToIndex(current));
      if (IsKey(current_key)) {
        uint32_t target = EntryForProbe(key, current_key, probe, current);
        if (current == target) continue;
        Object* target_key = get(EntryToIndex(target));
        if (!IsKey(target_key) ||
            EntryForProbe(key, target_key, probe, target) != target) {
          // Put the current element into the correct position.
          Swap(current, target, mode);
          // The other element will be processed on the next iteration.
          current--;
        } else {
          // The place for the current element is occupied. Leave the element
          // for the next probe.
          done = false;
        }
      }
    }
  }
  // Wipe deleted entries.
  Heap* heap = GetHeap();
  Object* the_hole = heap->the_hole_value();
  Object* undefined = heap->undefined_value();
  for (uint32_t current = 0; current < capacity; current++) {
    if (get(EntryToIndex(current)) == the_hole) {
      set(EntryToIndex(current), undefined);
    }
  }
  SetNumberOfDeletedElements(0);
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::EnsureCapacity(
    Handle<Derived> table,
    int n,
    Key key,
    PretenureFlag pretenure) {
  Isolate* isolate = table->GetIsolate();
  int capacity = table->Capacity();
  int nof = table->NumberOfElements() + n;

  if (table->HasSufficientCapacityToAdd(n)) return table;

  const int kMinCapacityForPretenure = 256;
  bool should_pretenure = pretenure == TENURED ||
      ((capacity > kMinCapacityForPretenure) &&
          !isolate->heap()->InNewSpace(*table));
  Handle<Derived> new_table = HashTable::New(
      isolate,
      nof * 2,
      USE_DEFAULT_MINIMUM_CAPACITY,
      should_pretenure ? TENURED : NOT_TENURED);

  table->Rehash(new_table, key);
  return new_table;
}

template <typename Derived, typename Shape, typename Key>
bool HashTable<Derived, Shape, Key>::HasSufficientCapacityToAdd(
    int number_of_additional_elements) {
  int capacity = Capacity();
  int nof = NumberOfElements() + number_of_additional_elements;
  int nod = NumberOfDeletedElements();
  // Return true if:
  //   50% is still free after adding number_of_additional_elements elements and
  //   at most 50% of the free elements are deleted elements.
  if ((nof < capacity) && ((nod <= (capacity - nof) >> 1))) {
    int needed_free = nof >> 1;
    if (nof + needed_free <= capacity) return true;
  }
  return false;
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::Shrink(Handle<Derived> table,
                                                       Key key) {
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();

  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (nof > (capacity >> 2)) return table;
  // Allocate a new dictionary with room for at least the current
  // number of elements. The allocation method will make sure that
  // there is extra room in the dictionary for additions. Don't go
  // lower than room for 16 elements.
  int at_least_room_for = nof;
  if (at_least_room_for < 16) return table;

  Isolate* isolate = table->GetIsolate();
  const int kMinCapacityForPretenure = 256;
  bool pretenure =
      (at_least_room_for > kMinCapacityForPretenure) &&
      !isolate->heap()->InNewSpace(*table);
  Handle<Derived> new_table = HashTable::New(
      isolate,
      at_least_room_for,
      USE_DEFAULT_MINIMUM_CAPACITY,
      pretenure ? TENURED : NOT_TENURED);

  table->Rehash(new_table, key);
  return new_table;
}


template<typename Derived, typename Shape, typename Key>
uint32_t HashTable<Derived, Shape, Key>::FindInsertionEntry(uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  while (true) {
    Object* element = KeyAt(entry);
    if (element->IsUndefined() || element->IsTheHole()) break;
    entry = NextProbe(entry, count++, capacity);
  }
  return entry;
}


// Force instantiation of template instances class.
// Please note this list is compiler dependent.

template class HashTable<StringTable, StringTableShape, HashTableKey*>;

template class HashTable<CompilationCacheTable,
                         CompilationCacheShape,
                         HashTableKey*>;

template class HashTable<ObjectHashTable,
                         ObjectHashTableShape,
                         Handle<Object> >;

template class HashTable<WeakHashTable, WeakHashTableShape<2>, Handle<Object> >;

template class Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >;

template class Dictionary<GlobalDictionary, GlobalDictionaryShape,
                          Handle<Name> >;

template class Dictionary<SeededNumberDictionary,
                          SeededNumberDictionaryShape,
                          uint32_t>;

template class Dictionary<UnseededNumberDictionary,
                          UnseededNumberDictionaryShape,
                          uint32_t>;

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    New(Isolate*, int at_least_space_for, PretenureFlag pretenure);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    New(Isolate*, int at_least_space_for, PretenureFlag pretenure);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    New(Isolate*, int n, PretenureFlag pretenure);

template Handle<GlobalDictionary>
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name> >::New(
    Isolate*, int n, PretenureFlag pretenure);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    AtPut(Handle<SeededNumberDictionary>, uint32_t, Handle<Object>);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    AtPut(Handle<UnseededNumberDictionary>, uint32_t, Handle<Object>);

template Object*
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object*
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    SlowReverseLookup(Object* value);

template Handle<Object>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::DeleteProperty(
    Handle<NameDictionary>, int);

template Handle<Object>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
           uint32_t>::DeleteProperty(Handle<SeededNumberDictionary>, int);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    New(Isolate*, int, MinimumCapacity, PretenureFlag);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    Shrink(Handle<NameDictionary>, Handle<Name>);

template Handle<SeededNumberDictionary>
HashTable<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    Shrink(Handle<SeededNumberDictionary>, uint32_t);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::Add(
    Handle<NameDictionary>, Handle<Name>, Handle<Object>, PropertyDetails);

template Handle<GlobalDictionary>
    Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name> >::Add(
        Handle<GlobalDictionary>, Handle<Name>, Handle<Object>,
        PropertyDetails);

template Handle<FixedArray> Dictionary<
    NameDictionary, NameDictionaryShape,
    Handle<Name> >::BuildIterationIndicesArray(Handle<NameDictionary>);

template Handle<FixedArray> Dictionary<
    NameDictionary, NameDictionaryShape,
    Handle<Name> >::GenerateNewEnumerationIndices(Handle<NameDictionary>);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    Add(Handle<SeededNumberDictionary>,
        uint32_t,
        Handle<Object>,
        PropertyDetails);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    Add(Handle<UnseededNumberDictionary>,
        uint32_t,
        Handle<Object>,
        PropertyDetails);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(Handle<SeededNumberDictionary>, int, uint32_t);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape, uint32_t>::
    EnsureCapacity(Handle<UnseededNumberDictionary>, int, uint32_t);

template void Dictionary<NameDictionary, NameDictionaryShape,
                         Handle<Name> >::SetRequiresCopyOnCapacityChange();

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name> >::
    EnsureCapacity(Handle<NameDictionary>, int, Handle<Name>);

template bool Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
                         uint32_t>::HasComplexElements();

template int HashTable<SeededNumberDictionary, SeededNumberDictionaryShape,
                       uint32_t>::FindEntry(uint32_t);

template int NameDictionaryBase<NameDictionary, NameDictionaryShape>::FindEntry(
    Handle<Name>);


Handle<Object> JSObject::PrepareSlowElementsForSort(
    Handle<JSObject> object, uint32_t limit) {
  DCHECK(object->HasDictionaryElements());
  Isolate* isolate = object->GetIsolate();
  // Must stay in dictionary mode, either because of requires_slow_elements,
  // or because we are not going to sort (and therefore compact) all of the
  // elements.
  Handle<SeededNumberDictionary> dict(object->element_dictionary(), isolate);
  Handle<SeededNumberDictionary> new_dict =
      SeededNumberDictionary::New(isolate, dict->NumberOfElements());

  uint32_t pos = 0;
  uint32_t undefs = 0;
  int capacity = dict->Capacity();
  Handle<Smi> bailout(Smi::FromInt(-1), isolate);
  // Entry to the new dictionary does not cause it to grow, as we have
  // allocated one that is large enough for all entries.
  DisallowHeapAllocation no_gc;
  for (int i = 0; i < capacity; i++) {
    Object* k = dict->KeyAt(i);
    if (!dict->IsKey(k)) continue;

    DCHECK(k->IsNumber());
    DCHECK(!k->IsSmi() || Smi::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() <= kMaxUInt32);

    HandleScope scope(isolate);
    Handle<Object> value(dict->ValueAt(i), isolate);
    PropertyDetails details = dict->DetailsAt(i);
    if (details.type() == ACCESSOR_CONSTANT || details.IsReadOnly()) {
      // Bail out and do the sorting of undefineds and array holes in JS.
      // Also bail out if the element is not supposed to be moved.
      return bailout;
    }

    uint32_t key = NumberToUint32(k);
    if (key < limit) {
      if (value->IsUndefined()) {
        undefs++;
      } else if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
        // Adding an entry with the key beyond smi-range requires
        // allocation. Bailout.
        return bailout;
      } else {
        Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
            new_dict, pos, value, details, object->map()->is_prototype_map());
        DCHECK(result.is_identical_to(new_dict));
        USE(result);
        pos++;
      }
    } else if (key > static_cast<uint32_t>(Smi::kMaxValue)) {
      // Adding an entry with the key beyond smi-range requires
      // allocation. Bailout.
      return bailout;
    } else {
      Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
          new_dict, key, value, details, object->map()->is_prototype_map());
      DCHECK(result.is_identical_to(new_dict));
      USE(result);
    }
  }

  uint32_t result = pos;
  PropertyDetails no_details = PropertyDetails::Empty();
  while (undefs > 0) {
    if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
      // Adding an entry with the key beyond smi-range requires
      // allocation. Bailout.
      return bailout;
    }
    HandleScope scope(isolate);
    Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
        new_dict, pos, isolate->factory()->undefined_value(), no_details,
        object->map()->is_prototype_map());
    DCHECK(result.is_identical_to(new_dict));
    USE(result);
    pos++;
    undefs--;
  }

  object->set_elements(*new_dict);

  AllowHeapAllocation allocate_return_value;
  return isolate->factory()->NewNumberFromUint(result);
}


// Collects all defined (non-hole) and non-undefined (array) elements at
// the start of the elements array.
// If the object is in dictionary mode, it is converted to fast elements
// mode.
Handle<Object> JSObject::PrepareElementsForSort(Handle<JSObject> object,
                                                uint32_t limit) {
  Isolate* isolate = object->GetIsolate();
  if (object->HasSloppyArgumentsElements() ||
      object->map()->is_observed()) {
    return handle(Smi::FromInt(-1), isolate);
  }

  if (object->HasStringWrapperElements()) {
    int len = String::cast(Handle<JSValue>::cast(object)->value())->length();
    return handle(Smi::FromInt(len), isolate);
  }

  if (object->HasDictionaryElements()) {
    // Convert to fast elements containing only the existing properties.
    // Ordering is irrelevant, since we are going to sort anyway.
    Handle<SeededNumberDictionary> dict(object->element_dictionary());
    if (object->IsJSArray() || dict->requires_slow_elements() ||
        dict->max_number_key() >= limit) {
      return JSObject::PrepareSlowElementsForSort(object, limit);
    }
    // Convert to fast elements.

    Handle<Map> new_map =
        JSObject::GetElementsTransitionMap(object, FAST_HOLEY_ELEMENTS);

    PretenureFlag tenure = isolate->heap()->InNewSpace(*object) ?
        NOT_TENURED: TENURED;
    Handle<FixedArray> fast_elements =
        isolate->factory()->NewFixedArray(dict->NumberOfElements(), tenure);
    dict->CopyValuesTo(*fast_elements);
    JSObject::ValidateElements(object);

    JSObject::SetMapAndElements(object, new_map, fast_elements);
  } else if (object->HasFixedTypedArrayElements()) {
    // Typed arrays cannot have holes or undefined elements.
    return handle(Smi::FromInt(
        FixedArrayBase::cast(object->elements())->length()), isolate);
  } else if (!object->HasFastDoubleElements()) {
    EnsureWritableFastElements(object);
  }
  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastDoubleElements());

  // Collect holes at the end, undefined before that and the rest at the
  // start, and return the number of non-hole, non-undefined values.

  Handle<FixedArrayBase> elements_base(object->elements());
  uint32_t elements_length = static_cast<uint32_t>(elements_base->length());
  if (limit > elements_length) {
    limit = elements_length;
  }
  if (limit == 0) {
    return handle(Smi::FromInt(0), isolate);
  }

  uint32_t result = 0;
  if (elements_base->map() == isolate->heap()->fixed_double_array_map()) {
    FixedDoubleArray* elements = FixedDoubleArray::cast(*elements_base);
    // Split elements into defined and the_hole, in that order.
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < holes; i++) {
      if (elements->is_the_hole(i)) {
        holes--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (holes > i) {
        if (elements->is_the_hole(holes)) {
          holes--;
        } else {
          elements->set(i, elements->get_scalar(holes));
          break;
        }
      }
    }
    result = holes;
    while (holes < limit) {
      elements->set_the_hole(holes);
      holes++;
    }
  } else {
    FixedArray* elements = FixedArray::cast(*elements_base);
    DisallowHeapAllocation no_gc;

    // Split elements into defined, undefined and the_hole, in that order.  Only
    // count locations for undefined and the hole, and fill them afterwards.
    WriteBarrierMode write_barrier = elements->GetWriteBarrierMode(no_gc);
    unsigned int undefs = limit;
    unsigned int holes = limit;
    // Assume most arrays contain no holes and undefined values, so minimize the
    // number of stores of non-undefined, non-the-hole values.
    for (unsigned int i = 0; i < undefs; i++) {
      Object* current = elements->get(i);
      if (current->IsTheHole()) {
        holes--;
        undefs--;
      } else if (current->IsUndefined()) {
        undefs--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (undefs > i) {
        current = elements->get(undefs);
        if (current->IsTheHole()) {
          holes--;
          undefs--;
        } else if (current->IsUndefined()) {
          undefs--;
        } else {
          elements->set(i, current, write_barrier);
          break;
        }
      }
    }
    result = undefs;
    while (undefs < holes) {
      elements->set_undefined(undefs);
      undefs++;
    }
    while (holes < limit) {
      elements->set_the_hole(holes);
      holes++;
    }
  }

  return isolate->factory()->NewNumberFromUint(result);
}


ExternalArrayType JSTypedArray::type() {
  switch (elements()->map()->instance_type()) {
#define INSTANCE_TYPE_TO_ARRAY_TYPE(Type, type, TYPE, ctype, size)            \
    case FIXED_##TYPE##_ARRAY_TYPE:                                           \
      return kExternal##Type##Array;

    TYPED_ARRAYS(INSTANCE_TYPE_TO_ARRAY_TYPE)
#undef INSTANCE_TYPE_TO_ARRAY_TYPE

    default:
      UNREACHABLE();
      return static_cast<ExternalArrayType>(-1);
  }
}


size_t JSTypedArray::element_size() {
  switch (elements()->map()->instance_type()) {
#define INSTANCE_TYPE_TO_ELEMENT_SIZE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:                                    \
    return size;

    TYPED_ARRAYS(INSTANCE_TYPE_TO_ELEMENT_SIZE)
#undef INSTANCE_TYPE_TO_ELEMENT_SIZE

    default:
      UNREACHABLE();
      return 0;
  }
}


void JSGlobalObject::InvalidatePropertyCell(Handle<JSGlobalObject> global,
                                            Handle<Name> name) {
  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary());
  int entry = dictionary->FindEntry(name);
  if (entry == GlobalDictionary::kNotFound) return;
  PropertyCell::InvalidateEntry(dictionary, entry);
}


// TODO(ishell): rename to EnsureEmptyPropertyCell or something.
Handle<PropertyCell> JSGlobalObject::EnsurePropertyCell(
    Handle<JSGlobalObject> global, Handle<Name> name) {
  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary());
  int entry = dictionary->FindEntry(name);
  Handle<PropertyCell> cell;
  if (entry != GlobalDictionary::kNotFound) {
    // This call should be idempotent.
    DCHECK(dictionary->ValueAt(entry)->IsPropertyCell());
    cell = handle(PropertyCell::cast(dictionary->ValueAt(entry)));
    DCHECK(cell->property_details().cell_type() ==
               PropertyCellType::kUninitialized ||
           cell->property_details().cell_type() ==
               PropertyCellType::kInvalidated);
    DCHECK(cell->value()->IsTheHole());
    return cell;
  }
  Isolate* isolate = global->GetIsolate();
  cell = isolate->factory()->NewPropertyCell();
  PropertyDetails details(NONE, DATA, 0, PropertyCellType::kUninitialized);
  dictionary = GlobalDictionary::Add(dictionary, name, cell, details);
  global->set_properties(*dictionary);
  return cell;
}


// This class is used for looking up two character strings in the string table.
// If we don't have a hit we don't want to waste much time so we unroll the
// string hash calculation loop here for speed.  Doesn't work if the two
// characters form a decimal integer, since such strings have a different hash
// algorithm.
class TwoCharHashTableKey : public HashTableKey {
 public:
  TwoCharHashTableKey(uint16_t c1, uint16_t c2, uint32_t seed)
    : c1_(c1), c2_(c2) {
    // Char 1.
    uint32_t hash = seed;
    hash += c1;
    hash += hash << 10;
    hash ^= hash >> 6;
    // Char 2.
    hash += c2;
    hash += hash << 10;
    hash ^= hash >> 6;
    // GetHash.
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    if ((hash & String::kHashBitMask) == 0) hash = StringHasher::kZeroHash;
    hash_ = hash;
#ifdef DEBUG
    // If this assert fails then we failed to reproduce the two-character
    // version of the string hashing algorithm above.  One reason could be
    // that we were passed two digits as characters, since the hash
    // algorithm is different in that case.
    uint16_t chars[2] = {c1, c2};
    uint32_t check_hash = StringHasher::HashSequentialString(chars, 2, seed);
    hash = (hash << String::kHashShift) | String::kIsNotArrayIndexMask;
    DCHECK_EQ(static_cast<int32_t>(hash), static_cast<int32_t>(check_hash));
#endif
  }

  bool IsMatch(Object* o) override {
    if (!o->IsString()) return false;
    String* other = String::cast(o);
    if (other->length() != 2) return false;
    if (other->Get(0) != c1_) return false;
    return other->Get(1) == c2_;
  }

  uint32_t Hash() override { return hash_; }
  uint32_t HashForObject(Object* key) override {
    if (!key->IsString()) return 0;
    return String::cast(key)->Hash();
  }

  Handle<Object> AsHandle(Isolate* isolate) override {
    // The TwoCharHashTableKey is only used for looking in the string
    // table, not for adding to it.
    UNREACHABLE();
    return MaybeHandle<Object>().ToHandleChecked();
  }

 private:
  uint16_t c1_;
  uint16_t c2_;
  uint32_t hash_;
};


MaybeHandle<String> StringTable::InternalizeStringIfExists(
    Isolate* isolate,
    Handle<String> string) {
  if (string->IsInternalizedString()) {
    return string;
  }
  return LookupStringIfExists(isolate, string);
}


MaybeHandle<String> StringTable::LookupStringIfExists(
    Isolate* isolate,
    Handle<String> string) {
  Handle<StringTable> string_table = isolate->factory()->string_table();
  InternalizedStringKey key(string);
  int entry = string_table->FindEntry(&key);
  if (entry == kNotFound) {
    return MaybeHandle<String>();
  } else {
    Handle<String> result(String::cast(string_table->KeyAt(entry)), isolate);
    DCHECK(StringShape(*result).IsInternalized());
    return result;
  }
}


MaybeHandle<String> StringTable::LookupTwoCharsStringIfExists(
    Isolate* isolate,
    uint16_t c1,
    uint16_t c2) {
  Handle<StringTable> string_table = isolate->factory()->string_table();
  TwoCharHashTableKey key(c1, c2, isolate->heap()->HashSeed());
  int entry = string_table->FindEntry(&key);
  if (entry == kNotFound) {
    return MaybeHandle<String>();
  } else {
    Handle<String> result(String::cast(string_table->KeyAt(entry)), isolate);
    DCHECK(StringShape(*result).IsInternalized());
    return result;
  }
}


void StringTable::EnsureCapacityForDeserialization(Isolate* isolate,
                                                   int expected) {
  Handle<StringTable> table = isolate->factory()->string_table();
  // We need a key instance for the virtual hash function.
  InternalizedStringKey dummy_key(Handle<String>::null());
  table = StringTable::EnsureCapacity(table, expected, &dummy_key);
  isolate->heap()->SetRootStringTable(*table);
}


Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  InternalizedStringKey key(string);
  return LookupKey(isolate, &key);
}


Handle<String> StringTable::LookupKey(Isolate* isolate, HashTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(key);

  // String already in table.
  if (entry != kNotFound) {
    return handle(String::cast(table->KeyAt(entry)), isolate);
  }

  // Adding new string. Grow table if needed.
  table = StringTable::EnsureCapacity(table, 1, key);

  // Create string object.
  Handle<Object> string = key->AsHandle(isolate);
  // There must be no attempts to internalize strings that could throw
  // InvalidStringLength error.
  CHECK(!string.is_null());

  // Add the new string and return it along with the string table.
  entry = table->FindInsertionEntry(key->Hash());
  table->set(EntryToIndex(entry), *string);
  table->ElementAdded();

  isolate->heap()->SetRootStringTable(*table);
  return Handle<String>::cast(string);
}


String* StringTable::LookupKeyIfExists(Isolate* isolate, HashTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(key);
  if (entry != kNotFound) return String::cast(table->KeyAt(entry));
  return NULL;
}


Handle<Object> CompilationCacheTable::Lookup(Handle<String> src,
                                             Handle<Context> context,
                                             LanguageMode language_mode) {
  Isolate* isolate = GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, language_mode, RelocInfo::kNoPosition);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return isolate->factory()->undefined_value();
  return Handle<Object>(get(index + 1), isolate);
}


Handle<Object> CompilationCacheTable::LookupEval(
    Handle<String> src, Handle<SharedFunctionInfo> outer_info,
    LanguageMode language_mode, int scope_position) {
  Isolate* isolate = GetIsolate();
  // Cache key is the tuple (source, outer shared function info, scope position)
  // to unambiguously identify the context chain the cached eval code assumes.
  StringSharedKey key(src, outer_info, language_mode, scope_position);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}


Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowHeapAllocation no_allocation;
  RegExpKey key(src, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}


Handle<CompilationCacheTable> CompilationCacheTable::Put(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> context, LanguageMode language_mode, Handle<Object> value) {
  Isolate* isolate = cache->GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, language_mode, RelocInfo::kNoPosition);
  {
    Handle<Object> k = key.AsHandle(isolate);
    DisallowHeapAllocation no_allocation_scope;
    int entry = cache->FindEntry(&key);
    if (entry != kNotFound) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      return cache;
    }
  }

  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}


Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    int scope_position) {
  Isolate* isolate = cache->GetIsolate();
  StringSharedKey key(src, outer_info, value->language_mode(), scope_position);
  {
    Handle<Object> k = key.AsHandle(isolate);
    DisallowHeapAllocation no_allocation_scope;
    int entry = cache->FindEntry(&key);
    if (entry != kNotFound) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      return cache;
    }
  }

  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}


Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
      Handle<CompilationCacheTable> cache, Handle<String> src,
      JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  // We store the value in the key slot, and compare the search key
  // to the stored value with a custon IsMatch function during lookups.
  cache->set(EntryToIndex(entry), *value);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}


void CompilationCacheTable::Age() {
  DisallowHeapAllocation no_allocation;
  Object* the_hole_value = GetHeap()->the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;

    if (get(entry_index)->IsNumber()) {
      Smi* count = Smi::cast(get(value_index));
      count = Smi::FromInt(count->value() - 1);
      if (count->value() == 0) {
        NoWriteBarrierSet(this, entry_index, the_hole_value);
        NoWriteBarrierSet(this, value_index, the_hole_value);
        ElementRemoved();
      } else {
        NoWriteBarrierSet(this, value_index, count);
      }
    } else if (get(entry_index)->IsFixedArray()) {
      SharedFunctionInfo* info = SharedFunctionInfo::cast(get(value_index));
      if (info->code()->kind() != Code::FUNCTION || info->code()->IsOld()) {
        NoWriteBarrierSet(this, entry_index, the_hole_value);
        NoWriteBarrierSet(this, value_index, the_hole_value);
        ElementRemoved();
      }
    }
  }
}


void CompilationCacheTable::Remove(Object* value) {
  DisallowHeapAllocation no_allocation;
  Object* the_hole_value = GetHeap()->the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;
    if (get(value_index) == value) {
      NoWriteBarrierSet(this, entry_index, the_hole_value);
      NoWriteBarrierSet(this, value_index, the_hole_value);
      ElementRemoved();
    }
  }
  return;
}


// StringsKey used for HashTable where key is array of internalized strings.
class StringsKey : public HashTableKey {
 public:
  explicit StringsKey(Handle<FixedArray> strings) : strings_(strings) { }

  bool IsMatch(Object* strings) override {
    FixedArray* o = FixedArray::cast(strings);
    int len = strings_->length();
    if (o->length() != len) return false;
    for (int i = 0; i < len; i++) {
      if (o->get(i) != strings_->get(i)) return false;
    }
    return true;
  }

  uint32_t Hash() override { return HashForObject(*strings_); }

  uint32_t HashForObject(Object* obj) override {
    FixedArray* strings = FixedArray::cast(obj);
    int len = strings->length();
    uint32_t hash = 0;
    for (int i = 0; i < len; i++) {
      hash ^= String::cast(strings->get(i))->Hash();
    }
    return hash;
  }

  Handle<Object> AsHandle(Isolate* isolate) override { return strings_; }

 private:
  Handle<FixedArray> strings_;
};


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::New(
    Isolate* isolate,
    int at_least_space_for,
    PretenureFlag pretenure) {
  DCHECK(0 <= at_least_space_for);
  Handle<Derived> dict = DerivedHashTable::New(isolate,
                                               at_least_space_for,
                                               USE_DEFAULT_MINIMUM_CAPACITY,
                                               pretenure);

  // Initialize the next enumeration index.
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
}


template <typename Derived, typename Shape, typename Key>
Handle<FixedArray> Dictionary<Derived, Shape, Key>::BuildIterationIndicesArray(
    Handle<Derived> dictionary) {
  Factory* factory = dictionary->GetIsolate()->factory();
  int length = dictionary->NumberOfElements();

  Handle<FixedArray> iteration_order = factory->NewFixedArray(length);
  Handle<FixedArray> enumeration_order = factory->NewFixedArray(length);

  // Fill both the iteration order array and the enumeration order array
  // with property details.
  int capacity = dictionary->Capacity();
  int pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (dictionary->IsKey(dictionary->KeyAt(i))) {
      int index = dictionary->DetailsAt(i).dictionary_index();
      iteration_order->set(pos, Smi::FromInt(i));
      enumeration_order->set(pos, Smi::FromInt(index));
      pos++;
    }
  }
  DCHECK(pos == length);

  // Sort the arrays wrt. enumeration order.
  iteration_order->SortPairs(*enumeration_order, enumeration_order->length());
  return iteration_order;
}


template <typename Derived, typename Shape, typename Key>
Handle<FixedArray>
Dictionary<Derived, Shape, Key>::GenerateNewEnumerationIndices(
    Handle<Derived> dictionary) {
  int length = dictionary->NumberOfElements();

  Handle<FixedArray> iteration_order = BuildIterationIndicesArray(dictionary);
  DCHECK(iteration_order->length() == length);

  // Iterate over the dictionary using the enumeration order and update
  // the dictionary with new enumeration indices.
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    DCHECK(dictionary->IsKey(dictionary->KeyAt(index)));

    int enum_index = PropertyDetails::kInitialIndex + i;

    PropertyDetails details = dictionary->DetailsAt(index);
    PropertyDetails new_details = details.set_index(enum_index);
    dictionary->DetailsAtPut(index, new_details);
  }

  // Set the next enumeration index.
  dictionary->SetNextEnumerationIndex(PropertyDetails::kInitialIndex+length);
  return iteration_order;
}


template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::SetRequiresCopyOnCapacityChange() {
  DCHECK_EQ(0, DerivedHashTable::NumberOfElements());
  DCHECK_EQ(0, DerivedHashTable::NumberOfDeletedElements());
  // Make sure that HashTable::EnsureCapacity will create a copy.
  DerivedHashTable::SetNumberOfDeletedElements(DerivedHashTable::Capacity());
  DCHECK(!DerivedHashTable::HasSufficientCapacityToAdd(1));
}


template <typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::EnsureCapacity(
    Handle<Derived> dictionary, int n, Key key) {
  // Check whether there are enough enumeration indices to add n elements.
  if (Shape::kIsEnumerable &&
      !PropertyDetails::IsValidIndex(dictionary->NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    GenerateNewEnumerationIndices(dictionary);
  }
  return DerivedHashTable::EnsureCapacity(dictionary, n, key);
}


template <typename Derived, typename Shape, typename Key>
Handle<Object> Dictionary<Derived, Shape, Key>::DeleteProperty(
    Handle<Derived> dictionary, int entry) {
  Factory* factory = dictionary->GetIsolate()->factory();
  PropertyDetails details = dictionary->DetailsAt(entry);
  if (!details.IsConfigurable()) return factory->false_value();

  dictionary->SetEntry(
      entry, factory->the_hole_value(), factory->the_hole_value());
  dictionary->ElementRemoved();
  return factory->true_value();
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::AtPut(
    Handle<Derived> dictionary, Key key, Handle<Object> value) {
  int entry = dictionary->FindEntry(key);

  // If the entry is present set the value;
  if (entry != Dictionary::kNotFound) {
    dictionary->ValueAtPut(entry, *value);
    return dictionary;
  }

  // Check whether the dictionary should be extended.
  dictionary = EnsureCapacity(dictionary, 1, key);
#ifdef DEBUG
  USE(Shape::AsHandle(dictionary->GetIsolate(), key));
#endif
  PropertyDetails details = PropertyDetails::Empty();

  AddEntry(dictionary, key, value, details, dictionary->Hash(key));
  return dictionary;
}


template<typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::Add(
    Handle<Derived> dictionary,
    Key key,
    Handle<Object> value,
    PropertyDetails details) {
  // Valdate key is absent.
  SLOW_DCHECK((dictionary->FindEntry(key) == Dictionary::kNotFound));
  // Check whether the dictionary should be extended.
  dictionary = EnsureCapacity(dictionary, 1, key);

  AddEntry(dictionary, key, value, details, dictionary->Hash(key));
  return dictionary;
}


// Add a key, value pair to the dictionary.
template<typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::AddEntry(
    Handle<Derived> dictionary,
    Key key,
    Handle<Object> value,
    PropertyDetails details,
    uint32_t hash) {
  // Compute the key object.
  Handle<Object> k = Shape::AsHandle(dictionary->GetIsolate(), key);

  uint32_t entry = dictionary->FindInsertionEntry(hash);
  // Insert element at empty or deleted entry
  if (details.dictionary_index() == 0 && Shape::kIsEnumerable) {
    // Assign an enumeration index to the property and update
    // SetNextEnumerationIndex.
    int index = dictionary->NextEnumerationIndex();
    details = details.set_index(index);
    dictionary->SetNextEnumerationIndex(index + 1);
  }
  dictionary->SetEntry(entry, k, value, details);
  DCHECK((dictionary->KeyAt(entry)->IsNumber() ||
          dictionary->KeyAt(entry)->IsName()));
  dictionary->ElementAdded();
}


void SeededNumberDictionary::UpdateMaxNumberKey(uint32_t key,
                                                bool used_as_prototype) {
  DisallowHeapAllocation no_allocation;
  // If the dictionary requires slow elements an element has already
  // been added at a high index.
  if (requires_slow_elements()) return;
  // Check if this index is high enough that we should require slow
  // elements.
  if (key > kRequiresSlowElementsLimit) {
    if (used_as_prototype) {
      // TODO(verwaest): Remove this hack.
      TypeFeedbackVector::ClearAllKeyedStoreICs(GetIsolate());
    }
    set_requires_slow_elements();
    return;
  }
  // Update max key value.
  Object* max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object->IsSmi() || max_number_key() < key) {
    FixedArray::set(kMaxNumberKeyIndex,
                    Smi::FromInt(key << kRequiresSlowElementsTagSize));
  }
}


Handle<SeededNumberDictionary> SeededNumberDictionary::AddNumberEntry(
    Handle<SeededNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, PropertyDetails details, bool used_as_prototype) {
  dictionary->UpdateMaxNumberKey(key, used_as_prototype);
  SLOW_DCHECK(dictionary->FindEntry(key) == kNotFound);
  return Add(dictionary, key, value, details);
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::AddNumberEntry(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  SLOW_DCHECK(dictionary->FindEntry(key) == kNotFound);
  return Add(dictionary, key, value, PropertyDetails::Empty());
}


Handle<SeededNumberDictionary> SeededNumberDictionary::AtNumberPut(
    Handle<SeededNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, bool used_as_prototype) {
  dictionary->UpdateMaxNumberKey(key, used_as_prototype);
  return AtPut(dictionary, key, value);
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::AtNumberPut(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  return AtPut(dictionary, key, value);
}


Handle<SeededNumberDictionary> SeededNumberDictionary::Set(
    Handle<SeededNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, PropertyDetails details, bool used_as_prototype) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) {
    return AddNumberEntry(dictionary, key, value, details, used_as_prototype);
  }
  // Preserve enumeration index.
  details = details.set_index(dictionary->DetailsAt(entry).dictionary_index());
  Handle<Object> object_key =
      SeededNumberDictionaryShape::AsHandle(dictionary->GetIsolate(), key);
  dictionary->SetEntry(entry, object_key, value, details);
  return dictionary;
}


Handle<UnseededNumberDictionary> UnseededNumberDictionary::Set(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(dictionary, key, value);
  Handle<Object> object_key =
      UnseededNumberDictionaryShape::AsHandle(dictionary->GetIsolate(), key);
  dictionary->SetEntry(entry, object_key, value);
  return dictionary;
}


template <typename Derived, typename Shape, typename Key>
int Dictionary<Derived, Shape, Key>::NumberOfElementsFilterAttributes(
    PropertyFilter filter) {
  int capacity = this->Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k) && !k->FilterKey(filter)) {
      if (this->IsDeleted(i)) continue;
      PropertyDetails details = this->DetailsAt(i);
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) result++;
    }
  }
  return result;
}


template <typename Derived, typename Shape, typename Key>
bool Dictionary<Derived, Shape, Key>::HasComplexElements() {
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k) && !k->FilterKey(ALL_PROPERTIES)) {
      if (this->IsDeleted(i)) continue;
      PropertyDetails details = this->DetailsAt(i);
      if (details.type() == ACCESSOR_CONSTANT) return true;
      PropertyAttributes attr = details.attributes();
      if (attr & ALL_ATTRIBUTES_MASK) return true;
    }
  }
  return false;
}


template <typename Dictionary>
struct EnumIndexComparator {
  explicit EnumIndexComparator(Dictionary* dict) : dict(dict) {}
  bool operator() (Smi* a, Smi* b) {
    PropertyDetails da(dict->DetailsAt(a->value()));
    PropertyDetails db(dict->DetailsAt(b->value()));
    return da.dictionary_index() < db.dictionary_index();
  }
  Dictionary* dict;
};


template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CopyEnumKeysTo(FixedArray* storage) {
  int length = storage->length();
  int capacity = this->Capacity();
  int properties = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k) && !k->IsSymbol()) {
      PropertyDetails details = this->DetailsAt(i);
      if (details.IsDontEnum() || this->IsDeleted(i)) continue;
      storage->set(properties, Smi::FromInt(i));
      properties++;
      if (properties == length) break;
    }
  }
  CHECK_EQ(length, properties);
  EnumIndexComparator<Derived> cmp(static_cast<Derived*>(this));
  Smi** start = reinterpret_cast<Smi**>(storage->GetFirstElementAddress());
  std::sort(start, start + length, cmp);
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(storage->get(i))->value();
    storage->set(i, this->KeyAt(index));
  }
}


template <typename Derived, typename Shape, typename Key>
int Dictionary<Derived, Shape, Key>::CopyKeysTo(
    FixedArray* storage, int index, PropertyFilter filter,
    typename Dictionary<Derived, Shape, Key>::SortMode sort_mode) {
  DCHECK(storage->length() >= NumberOfElementsFilterAttributes(filter));
  int start_index = index;
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (!this->IsKey(k) || k->FilterKey(filter)) continue;
    if (this->IsDeleted(i)) continue;
    PropertyDetails details = this->DetailsAt(i);
    PropertyAttributes attr = details.attributes();
    if ((attr & filter) != 0) continue;
    storage->set(index++, k);
  }
  if (sort_mode == Dictionary::SORTED) {
    storage->SortPairs(storage, index);
  }
  DCHECK(storage->length() >= index);
  return index - start_index;
}


template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CollectKeysTo(
    Handle<Dictionary<Derived, Shape, Key> > dictionary, KeyAccumulator* keys,
    PropertyFilter filter) {
  int capacity = dictionary->Capacity();
  Handle<FixedArray> array =
      keys->isolate()->factory()->NewFixedArray(dictionary->NumberOfElements());
  int array_size = 0;

  {
    DisallowHeapAllocation no_gc;
    Dictionary<Derived, Shape, Key>* raw_dict = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object* k = raw_dict->KeyAt(i);
      if (!raw_dict->IsKey(k) || k->FilterKey(filter)) continue;
      if (raw_dict->IsDeleted(i)) continue;
      PropertyDetails details = raw_dict->DetailsAt(i);
      if ((details.attributes() & filter) != 0) continue;
      if (filter & ONLY_ALL_CAN_READ) {
        if (details.kind() != kAccessor) continue;
        Object* accessors = raw_dict->ValueAt(i);
        if (accessors->IsPropertyCell()) {
          accessors = PropertyCell::cast(accessors)->value();
        }
        if (!accessors->IsAccessorInfo()) continue;
        if (!AccessorInfo::cast(accessors)->all_can_read()) continue;
      }
      array->set(array_size++, Smi::FromInt(i));
    }

    EnumIndexComparator<Derived> cmp(static_cast<Derived*>(raw_dict));
    Smi** start = reinterpret_cast<Smi**>(array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }

  for (int i = 0; i < array_size; i++) {
    int index = Smi::cast(array->get(i))->value();
    keys->AddKey(dictionary->KeyAt(index), DO_NOT_CONVERT);
  }
}


// Backwards lookup (slow).
template<typename Derived, typename Shape, typename Key>
Object* Dictionary<Derived, Shape, Key>::SlowReverseLookup(Object* value) {
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(k)) {
      Object* e = this->ValueAt(i);
      // TODO(dcarney): this should be templatized.
      if (e->IsPropertyCell()) {
        e = PropertyCell::cast(e)->value();
      }
      if (e == value) return k;
    }
  }
  Heap* heap = Dictionary::GetHeap();
  return heap->undefined_value();
}


Object* ObjectHashTable::Lookup(Isolate* isolate, Handle<Object> key,
                                int32_t hash) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(*key));

  int entry = FindEntry(isolate, key, hash);
  if (entry == kNotFound) return isolate->heap()->the_hole_value();
  return get(EntryToIndex(entry) + 1);
}


Object* ObjectHashTable::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(*key));

  Isolate* isolate = GetIsolate();

  // If the object does not have an identity hash, it was never used as a key.
  Object* hash = key->GetHash();
  if (hash->IsUndefined()) {
    return isolate->heap()->the_hole_value();
  }
  return Lookup(isolate, key, Smi::cast(hash)->value());
}


Object* ObjectHashTable::Lookup(Handle<Object> key, int32_t hash) {
  return Lookup(GetIsolate(), key, hash);
}


Handle<ObjectHashTable> ObjectHashTable::Put(Handle<ObjectHashTable> table,
                                             Handle<Object> key,
                                             Handle<Object> value) {
  DCHECK(table->IsKey(*key));
  DCHECK(!value->IsTheHole());

  Isolate* isolate = table->GetIsolate();
  // Make sure the key object has an identity hash code.
  int32_t hash = Object::GetOrCreateHash(isolate, key)->value();

  return Put(table, key, value, hash);
}


Handle<ObjectHashTable> ObjectHashTable::Put(Handle<ObjectHashTable> table,
                                             Handle<Object> key,
                                             Handle<Object> value,
                                             int32_t hash) {
  DCHECK(table->IsKey(*key));
  DCHECK(!value->IsTheHole());

  Isolate* isolate = table->GetIsolate();

  int entry = table->FindEntry(isolate, key, hash);

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(EntryToIndex(entry) + 1, *value);
    return table;
  }

  // Rehash if more than 25% of the entries are deleted entries.
  // TODO(jochen): Consider to shrink the fixed array in place.
  if ((table->NumberOfDeletedElements() << 1) > table->NumberOfElements()) {
    table->Rehash(isolate->factory()->undefined_value());
  }
  // If we're out of luck, we didn't get a GC recently, and so rehashing
  // isn't enough to avoid a crash.
  if (!table->HasSufficientCapacityToAdd(1)) {
    int nof = table->NumberOfElements() + 1;
    int capacity = ObjectHashTable::ComputeCapacity(nof * 2);
    if (capacity > ObjectHashTable::kMaxCapacity) {
      for (size_t i = 0; i < 2; ++i) {
        isolate->heap()->CollectAllGarbage(
            Heap::kFinalizeIncrementalMarkingMask, "full object hash table");
      }
      table->Rehash(isolate->factory()->undefined_value());
    }
  }

  // Check whether the hash table should be extended.
  table = EnsureCapacity(table, 1, key);
  table->AddEntry(table->FindInsertionEntry(hash), *key, *value);
  return table;
}


Handle<ObjectHashTable> ObjectHashTable::Remove(Handle<ObjectHashTable> table,
                                                Handle<Object> key,
                                                bool* was_present) {
  DCHECK(table->IsKey(*key));

  Object* hash = key->GetHash();
  if (hash->IsUndefined()) {
    *was_present = false;
    return table;
  }

  return Remove(table, key, was_present, Smi::cast(hash)->value());
}


Handle<ObjectHashTable> ObjectHashTable::Remove(Handle<ObjectHashTable> table,
                                                Handle<Object> key,
                                                bool* was_present,
                                                int32_t hash) {
  DCHECK(table->IsKey(*key));

  int entry = table->FindEntry(table->GetIsolate(), key, hash);
  if (entry == kNotFound) {
    *was_present = false;
    return table;
  }

  *was_present = true;
  table->RemoveEntry(entry);
  return Shrink(table, key);
}


void ObjectHashTable::AddEntry(int entry, Object* key, Object* value) {
  set(EntryToIndex(entry), key);
  set(EntryToIndex(entry) + 1, value);
  ElementAdded();
}


void ObjectHashTable::RemoveEntry(int entry) {
  set_the_hole(EntryToIndex(entry));
  set_the_hole(EntryToIndex(entry) + 1);
  ElementRemoved();
}


Object* WeakHashTable::Lookup(Handle<HeapObject> key) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(*key));
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->the_hole_value();
  return get(EntryToValueIndex(entry));
}


Handle<WeakHashTable> WeakHashTable::Put(Handle<WeakHashTable> table,
                                         Handle<HeapObject> key,
                                         Handle<HeapObject> value) {
  DCHECK(table->IsKey(*key));
  int entry = table->FindEntry(key);
  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(EntryToValueIndex(entry), *value);
    return table;
  }

  Handle<WeakCell> key_cell = key->GetIsolate()->factory()->NewWeakCell(key);

  // Check whether the hash table should be extended.
  table = EnsureCapacity(table, 1, key, TENURED);

  table->AddEntry(table->FindInsertionEntry(table->Hash(key)), key_cell, value);
  return table;
}


void WeakHashTable::AddEntry(int entry, Handle<WeakCell> key_cell,
                             Handle<HeapObject> value) {
  DisallowHeapAllocation no_allocation;
  set(EntryToIndex(entry), *key_cell);
  set(EntryToValueIndex(entry), *value);
  ElementAdded();
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure) {
  // Capacity must be a power of two, since we depend on being able
  // to divide and multiple by 2 (kLoadFactor) to derive capacity
  // from number of buckets. If we decide to change kLoadFactor
  // to something other than 2, capacity should be stored as another
  // field of this object.
  capacity = base::bits::RoundUpToPowerOfTwo32(Max(kMinCapacity, capacity));
  if (capacity > kMaxCapacity) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid table size", true);
  }
  int num_buckets = capacity / kLoadFactor;
  Handle<FixedArray> backing_store = isolate->factory()->NewFixedArray(
      kHashTableStartIndex + num_buckets + (capacity * kEntrySize), pretenure);
  backing_store->set_map_no_write_barrier(
      isolate->heap()->ordered_hash_table_map());
  Handle<Derived> table = Handle<Derived>::cast(backing_store);
  for (int i = 0; i < num_buckets; ++i) {
    table->set(kHashTableStartIndex + i, Smi::FromInt(kNotFound));
  }
  table->SetNumberOfBuckets(num_buckets);
  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  return table;
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::EnsureGrowable(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int capacity = table->Capacity();
  if ((nof + nod) < capacity) return table;
  // Don't need to grow if we can simply clear out deleted entries instead.
  // Note that we can't compact in place, though, so we always allocate
  // a new table.
  return Rehash(table, (nod < (capacity >> 1)) ? capacity << 1 : capacity);
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Shrink(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Rehash(table, capacity / 2);
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Clear(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(table->GetIsolate(),
               kMinCapacity,
               table->GetHeap()->InNewSpace(*table) ? NOT_TENURED : TENURED);

  table->SetNextTable(*new_table);
  table->SetNumberOfDeletedElements(kClearedTableSentinel);

  return new_table;
}

template <class Derived, class Iterator, int entrysize>
bool OrderedHashTable<Derived, Iterator, entrysize>::HasKey(
    Handle<Derived> table, Handle<Object> key) {
  int entry = table->KeyToFirstEntry(*key);
  // Walk the chain in the bucket to find the key.
  while (entry != kNotFound) {
    Object* candidate_key = table->KeyAt(entry);
    if (candidate_key->SameValueZero(*key)) return true;
    entry = table->NextChainEntry(entry);
  }
  return false;
}


Handle<OrderedHashSet> OrderedHashSet::Add(Handle<OrderedHashSet> table,
                                           Handle<Object> key) {
  int hash = Object::GetOrCreateHash(table->GetIsolate(), key)->value();
  int entry = table->HashToEntry(hash);
  // Walk the chain of the bucket and try finding the key.
  while (entry != kNotFound) {
    Object* candidate_key = table->KeyAt(entry);
    // Do not add if we have the key already
    if (candidate_key->SameValueZero(*key)) return table;
    entry = table->NextChainEntry(entry);
  }

  table = OrderedHashSet::EnsureGrowable(table);
  // Read the existing bucket values.
  int bucket = table->HashToBucket(hash);
  int previous_entry = table->HashToEntry(hash);
  int nof = table->NumberOfElements();
  // Insert a new entry at the end,
  int new_entry = nof + table->NumberOfDeletedElements();
  int new_index = table->EntryToIndex(new_entry);
  table->set(new_index, *key);
  table->set(new_index + kChainOffset, Smi::FromInt(previous_entry));
  // and point the bucket to the new entry.
  table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
  table->SetNumberOfElements(nof + 1);
  return table;
}


template<class Derived, class Iterator, int entrysize>
Handle<Derived> OrderedHashTable<Derived, Iterator, entrysize>::Rehash(
    Handle<Derived> table, int new_capacity) {
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(table->GetIsolate(),
               new_capacity,
               table->GetHeap()->InNewSpace(*table) ? NOT_TENURED : TENURED);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_buckets = new_table->NumberOfBuckets();
  int new_entry = 0;
  int removed_holes_index = 0;

  for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
    Object* key = table->KeyAt(old_entry);
    if (key->IsTheHole()) {
      table->SetRemovedIndexAt(removed_holes_index++, old_entry);
      continue;
    }

    Object* hash = key->GetHash();
    int bucket = Smi::cast(hash)->value() & (new_buckets - 1);
    Object* chain_entry = new_table->get(kHashTableStartIndex + bucket);
    new_table->set(kHashTableStartIndex + bucket, Smi::FromInt(new_entry));
    int new_index = new_table->EntryToIndex(new_entry);
    int old_index = table->EntryToIndex(old_entry);
    for (int i = 0; i < entrysize; ++i) {
      Object* value = table->get(old_index + i);
      new_table->set(new_index + i, value);
    }
    new_table->set(new_index + kChainOffset, chain_entry);
    ++new_entry;
  }

  DCHECK_EQ(nod, removed_holes_index);

  new_table->SetNumberOfElements(nof);
  table->SetNextTable(*new_table);

  return new_table;
}


template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::EnsureGrowable(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Shrink(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet>
OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::Clear(
    Handle<OrderedHashSet> table);

template bool OrderedHashTable<OrderedHashSet, JSSetIterator, 1>::HasKey(
    Handle<OrderedHashSet> table, Handle<Object> key);


template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::EnsureGrowable(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Shrink(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap>
OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::Clear(
    Handle<OrderedHashMap> table);

template bool OrderedHashTable<OrderedHashMap, JSMapIterator, 2>::HasKey(
    Handle<OrderedHashMap> table, Handle<Object> key);


template<class Derived, class TableType>
void OrderedHashTableIterator<Derived, TableType>::Transition() {
  DisallowHeapAllocation no_allocation;
  TableType* table = TableType::cast(this->table());
  if (!table->IsObsolete()) return;

  int index = Smi::cast(this->index())->value();
  while (table->IsObsolete()) {
    TableType* next_table = table->NextTable();

    if (index > 0) {
      int nod = table->NumberOfDeletedElements();

      if (nod == TableType::kClearedTableSentinel) {
        index = 0;
      } else {
        int old_index = index;
        for (int i = 0; i < nod; ++i) {
          int removed_index = table->RemovedIndexAt(i);
          if (removed_index >= old_index) break;
          --index;
        }
      }
    }

    table = next_table;
  }

  set_table(table);
  set_index(Smi::FromInt(index));
}


template<class Derived, class TableType>
bool OrderedHashTableIterator<Derived, TableType>::HasMore() {
  DisallowHeapAllocation no_allocation;
  if (this->table()->IsUndefined()) return false;

  Transition();

  TableType* table = TableType::cast(this->table());
  int index = Smi::cast(this->index())->value();
  int used_capacity = table->UsedCapacity();

  while (index < used_capacity && table->KeyAt(index)->IsTheHole()) {
    index++;
  }

  set_index(Smi::FromInt(index));

  if (index < used_capacity) return true;

  set_table(GetHeap()->undefined_value());
  return false;
}


template<class Derived, class TableType>
Smi* OrderedHashTableIterator<Derived, TableType>::Next(JSArray* value_array) {
  DisallowHeapAllocation no_allocation;
  if (HasMore()) {
    FixedArray* array = FixedArray::cast(value_array->elements());
    static_cast<Derived*>(this)->PopulateValueArray(array);
    MoveNext();
    return Smi::cast(kind());
  }
  return Smi::FromInt(0);
}


template Smi*
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Next(
    JSArray* value_array);

template bool
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::HasMore();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::MoveNext();

template Object*
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::CurrentKey();

template void
OrderedHashTableIterator<JSSetIterator, OrderedHashSet>::Transition();


template Smi*
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Next(
    JSArray* value_array);

template bool
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::HasMore();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::MoveNext();

template Object*
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::CurrentKey();

template void
OrderedHashTableIterator<JSMapIterator, OrderedHashMap>::Transition();


void JSSet::Initialize(Handle<JSSet> set, Isolate* isolate) {
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  set->set_table(*table);
}


void JSSet::Clear(Handle<JSSet> set) {
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()));
  table = OrderedHashSet::Clear(table);
  set->set_table(*table);
}


void JSMap::Initialize(Handle<JSMap> map, Isolate* isolate) {
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  map->set_table(*table);
}


void JSMap::Clear(Handle<JSMap> map) {
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()));
  table = OrderedHashMap::Clear(table);
  map->set_table(*table);
}


void JSWeakCollection::Initialize(Handle<JSWeakCollection> weak_collection,
                                  Isolate* isolate) {
  Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
}


void JSWeakCollection::Set(Handle<JSWeakCollection> weak_collection,
                           Handle<Object> key, Handle<Object> value,
                           int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  DCHECK(table->IsKey(*key));
  Handle<ObjectHashTable> new_table =
      ObjectHashTable::Put(table, key, value, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
}


bool JSWeakCollection::Delete(Handle<JSWeakCollection> weak_collection,
                              Handle<Object> key, int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  DCHECK(table->IsKey(*key));
  bool was_present = false;
  Handle<ObjectHashTable> new_table =
      ObjectHashTable::Remove(table, key, &was_present, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    table->FillWithHoles(0, table->length());
  }
  return was_present;
}

// Check if there is a break point at this code offset.
bool DebugInfo::HasBreakPoint(int code_offset) {
  // Get the break point info object for this code offset.
  Object* break_point_info = GetBreakPointInfo(code_offset);

  // If there is no break point info object or no break points in the break
  // point info object there is no break point at this code offset.
  if (break_point_info->IsUndefined()) return false;
  return BreakPointInfo::cast(break_point_info)->GetBreakPointCount() > 0;
}

// Get the break point info object for this code offset.
Object* DebugInfo::GetBreakPointInfo(int code_offset) {
  // Find the index of the break point info object for this code offset.
  int index = GetBreakPointInfoIndex(code_offset);

  // Return the break point info object if any.
  if (index == kNoBreakPointInfo) return GetHeap()->undefined_value();
  return BreakPointInfo::cast(break_points()->get(index));
}

// Clear a break point at the specified code offset.
void DebugInfo::ClearBreakPoint(Handle<DebugInfo> debug_info, int code_offset,
                                Handle<Object> break_point_object) {
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_offset),
                                  debug_info->GetIsolate());
  if (break_point_info->IsUndefined()) return;
  BreakPointInfo::ClearBreakPoint(
      Handle<BreakPointInfo>::cast(break_point_info),
      break_point_object);
}

void DebugInfo::SetBreakPoint(Handle<DebugInfo> debug_info, int code_offset,
                              int source_position, int statement_position,
                              Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_offset),
                                  isolate);
  if (!break_point_info->IsUndefined()) {
    BreakPointInfo::SetBreakPoint(
        Handle<BreakPointInfo>::cast(break_point_info),
        break_point_object);
    return;
  }

  // Adding a new break point for a code offset which did not have any
  // break points before. Try to find a free slot.
  int index = kNoBreakPointInfo;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined()) {
      index = i;
      break;
    }
  }
  if (index == kNoBreakPointInfo) {
    // No free slot - extend break point info array.
    Handle<FixedArray> old_break_points =
        Handle<FixedArray>(FixedArray::cast(debug_info->break_points()));
    Handle<FixedArray> new_break_points =
        isolate->factory()->NewFixedArray(
            old_break_points->length() +
            DebugInfo::kEstimatedNofBreakPointsInFunction);

    debug_info->set_break_points(*new_break_points);
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  DCHECK(index != kNoBreakPointInfo);

  // Allocate new BreakPointInfo object and set the break point.
  Handle<BreakPointInfo> new_break_point_info = Handle<BreakPointInfo>::cast(
      isolate->factory()->NewStruct(BREAK_POINT_INFO_TYPE));
  new_break_point_info->set_code_offset(code_offset);
  new_break_point_info->set_source_position(source_position);
  new_break_point_info->set_statement_position(statement_position);
  new_break_point_info->set_break_point_objects(
      isolate->heap()->undefined_value());
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}

// Get the break point objects for a code offset.
Handle<Object> DebugInfo::GetBreakPointObjects(int code_offset) {
  Object* break_point_info = GetBreakPointInfo(code_offset);
  if (break_point_info->IsUndefined()) {
    return GetIsolate()->factory()->undefined_value();
  }
  return Handle<Object>(
      BreakPointInfo::cast(break_point_info)->break_point_objects(),
      GetIsolate());
}


// Get the total number of break points.
int DebugInfo::GetBreakPointCount() {
  if (break_points()->IsUndefined()) return 0;
  int count = 0;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      count += break_point_info->GetBreakPointCount();
    }
  }
  return count;
}


Handle<Object> DebugInfo::FindBreakPointInfo(
    Handle<DebugInfo> debug_info, Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  if (!debug_info->break_points()->IsUndefined()) {
    for (int i = 0; i < debug_info->break_points()->length(); i++) {
      if (!debug_info->break_points()->get(i)->IsUndefined()) {
        Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
            BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
        if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                                break_point_object)) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->factory()->undefined_value();
}


// Find the index of the break point info object for the specified code
// position.
int DebugInfo::GetBreakPointInfoIndex(int code_offset) {
  if (break_points()->IsUndefined()) return kNoBreakPointInfo;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined()) {
      BreakPointInfo* break_point_info =
          BreakPointInfo::cast(break_points()->get(i));
      if (break_point_info->code_offset() == code_offset) {
        return i;
      }
    }
  }
  return kNoBreakPointInfo;
}


// Remove the specified break point object.
void BreakPointInfo::ClearBreakPoint(Handle<BreakPointInfo> break_point_info,
                                     Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined()) return;
  // If there is a single break point clear it if it is the same.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    if (break_point_info->break_point_objects() == *break_point_object) {
      break_point_info->set_break_point_objects(
          isolate->heap()->undefined_value());
    }
    return;
  }
  // If there are multiple break points shrink the array
  DCHECK(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() - 1);
  int found_count = 0;
  for (int i = 0; i < old_array->length(); i++) {
    if (old_array->get(i) == *break_point_object) {
      DCHECK(found_count == 0);
      found_count++;
    } else {
      new_array->set(i - found_count, old_array->get(i));
    }
  }
  // If the break point was found in the list change it.
  if (found_count > 0) break_point_info->set_break_point_objects(*new_array);
}


// Add the specified break point object.
void BreakPointInfo::SetBreakPoint(Handle<BreakPointInfo> break_point_info,
                                   Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();

  // If there was no break point objects before just set it.
  if (break_point_info->break_point_objects()->IsUndefined()) {
    break_point_info->set_break_point_objects(*break_point_object);
    return;
  }
  // If the break point object is the same as before just ignore.
  if (break_point_info->break_point_objects() == *break_point_object) return;
  // If there was one break point object before replace with array.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(2);
    array->set(0, break_point_info->break_point_objects());
    array->set(1, *break_point_object);
    break_point_info->set_break_point_objects(*array);
    return;
  }
  // If there was more than one break point before extend array.
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      isolate->factory()->NewFixedArray(old_array->length() + 1);
  for (int i = 0; i < old_array->length(); i++) {
    // If the break point was there before just ignore.
    if (old_array->get(i) == *break_point_object) return;
    new_array->set(i, old_array->get(i));
  }
  // Add the new break point.
  new_array->set(old_array->length(), *break_point_object);
  break_point_info->set_break_point_objects(*new_array);
}


bool BreakPointInfo::HasBreakPointObject(
    Handle<BreakPointInfo> break_point_info,
    Handle<Object> break_point_object) {
  // No break point.
  if (break_point_info->break_point_objects()->IsUndefined()) return false;
  // Single break point.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    return break_point_info->break_point_objects() == *break_point_object;
  }
  // Multiple break points.
  FixedArray* array = FixedArray::cast(break_point_info->break_point_objects());
  for (int i = 0; i < array->length(); i++) {
    if (array->get(i) == *break_point_object) {
      return true;
    }
  }
  return false;
}


// Get the number of break points.
int BreakPointInfo::GetBreakPointCount() {
  // No break point.
  if (break_point_objects()->IsUndefined()) return 0;
  // Single break point.
  if (!break_point_objects()->IsFixedArray()) return 1;
  // Multiple break points.
  return FixedArray::cast(break_point_objects())->length();
}


// static
MaybeHandle<JSDate> JSDate::New(Handle<JSFunction> constructor,
                                Handle<JSReceiver> new_target, double tv) {
  Isolate* const isolate = constructor->GetIsolate();
  Handle<JSObject> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                             JSObject::New(constructor, new_target), JSDate);
  if (-DateCache::kMaxTimeInMs <= tv && tv <= DateCache::kMaxTimeInMs) {
    tv = DoubleToInteger(tv) + 0.0;
  } else {
    tv = std::numeric_limits<double>::quiet_NaN();
  }
  Handle<Object> value = isolate->factory()->NewNumber(tv);
  Handle<JSDate>::cast(result)->SetValue(*value, std::isnan(tv));
  return Handle<JSDate>::cast(result);
}


// static
double JSDate::CurrentTimeValue(Isolate* isolate) {
  if (FLAG_log_timer_events || FLAG_prof_cpp) LOG(isolate, CurrentTimeEvent());

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  return Floor(FLAG_verify_predictable
                   ? isolate->heap()->MonotonicallyIncreasingTimeInMs()
                   : base::OS::TimeCurrentMillis());
}


// static
Object* JSDate::GetField(Object* object, Smi* index) {
  return JSDate::cast(object)->DoGetField(
      static_cast<FieldIndex>(index->value()));
}


Object* JSDate::DoGetField(FieldIndex index) {
  DCHECK(index != kDateValue);

  DateCache* date_cache = GetIsolate()->date_cache();

  if (index < kFirstUncachedField) {
    Object* stamp = cache_stamp();
    if (stamp != date_cache->stamp() && stamp->IsSmi()) {
      // Since the stamp is not NaN, the value is also not NaN.
      int64_t local_time_ms =
          date_cache->ToLocal(static_cast<int64_t>(value()->Number()));
      SetCachedFields(local_time_ms, date_cache);
    }
    switch (index) {
      case kYear: return year();
      case kMonth: return month();
      case kDay: return day();
      case kWeekday: return weekday();
      case kHour: return hour();
      case kMinute: return min();
      case kSecond: return sec();
      default: UNREACHABLE();
    }
  }

  if (index >= kFirstUTCField) {
    return GetUTCField(index, value()->Number(), date_cache);
  }

  double time = value()->Number();
  if (std::isnan(time)) return GetIsolate()->heap()->nan_value();

  int64_t local_time_ms = date_cache->ToLocal(static_cast<int64_t>(time));
  int days = DateCache::DaysFromTime(local_time_ms);

  if (index == kDays) return Smi::FromInt(days);

  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  if (index == kMillisecond) return Smi::FromInt(time_in_day_ms % 1000);
  DCHECK(index == kTimeInDay);
  return Smi::FromInt(time_in_day_ms);
}


Object* JSDate::GetUTCField(FieldIndex index,
                            double value,
                            DateCache* date_cache) {
  DCHECK(index >= kFirstUTCField);

  if (std::isnan(value)) return GetIsolate()->heap()->nan_value();

  int64_t time_ms = static_cast<int64_t>(value);

  if (index == kTimezoneOffset) {
    return Smi::FromInt(date_cache->TimezoneOffset(time_ms));
  }

  int days = DateCache::DaysFromTime(time_ms);

  if (index == kWeekdayUTC) return Smi::FromInt(date_cache->Weekday(days));

  if (index <= kDayUTC) {
    int year, month, day;
    date_cache->YearMonthDayFromDays(days, &year, &month, &day);
    if (index == kYearUTC) return Smi::FromInt(year);
    if (index == kMonthUTC) return Smi::FromInt(month);
    DCHECK(index == kDayUTC);
    return Smi::FromInt(day);
  }

  int time_in_day_ms = DateCache::TimeInDay(time_ms, days);
  switch (index) {
    case kHourUTC: return Smi::FromInt(time_in_day_ms / (60 * 60 * 1000));
    case kMinuteUTC: return Smi::FromInt((time_in_day_ms / (60 * 1000)) % 60);
    case kSecondUTC: return Smi::FromInt((time_in_day_ms / 1000) % 60);
    case kMillisecondUTC: return Smi::FromInt(time_in_day_ms % 1000);
    case kDaysUTC: return Smi::FromInt(days);
    case kTimeInDayUTC: return Smi::FromInt(time_in_day_ms);
    default: UNREACHABLE();
  }

  UNREACHABLE();
  return NULL;
}


// static
Handle<Object> JSDate::SetValue(Handle<JSDate> date, double v) {
  Isolate* const isolate = date->GetIsolate();
  Handle<Object> value = isolate->factory()->NewNumber(v);
  bool value_is_nan = std::isnan(v);
  date->SetValue(*value, value_is_nan);
  return value;
}


void JSDate::SetValue(Object* value, bool is_value_nan) {
  set_value(value);
  if (is_value_nan) {
    HeapNumber* nan = GetIsolate()->heap()->nan_value();
    set_cache_stamp(nan, SKIP_WRITE_BARRIER);
    set_year(nan, SKIP_WRITE_BARRIER);
    set_month(nan, SKIP_WRITE_BARRIER);
    set_day(nan, SKIP_WRITE_BARRIER);
    set_hour(nan, SKIP_WRITE_BARRIER);
    set_min(nan, SKIP_WRITE_BARRIER);
    set_sec(nan, SKIP_WRITE_BARRIER);
    set_weekday(nan, SKIP_WRITE_BARRIER);
  } else {
    set_cache_stamp(Smi::FromInt(DateCache::kInvalidStamp), SKIP_WRITE_BARRIER);
  }
}


// static
MaybeHandle<Object> JSDate::ToPrimitive(Handle<JSReceiver> receiver,
                                        Handle<Object> hint) {
  Isolate* const isolate = receiver->GetIsolate();
  if (hint->IsString()) {
    Handle<String> hint_string = Handle<String>::cast(hint);
    if (hint_string->Equals(isolate->heap()->number_string())) {
      return JSReceiver::OrdinaryToPrimitive(receiver,
                                             OrdinaryToPrimitiveHint::kNumber);
    }
    if (hint_string->Equals(isolate->heap()->default_string()) ||
        hint_string->Equals(isolate->heap()->string_string())) {
      return JSReceiver::OrdinaryToPrimitive(receiver,
                                             OrdinaryToPrimitiveHint::kString);
    }
  }
  THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kInvalidHint, hint),
                  Object);
}


void JSDate::SetCachedFields(int64_t local_time_ms, DateCache* date_cache) {
  int days = DateCache::DaysFromTime(local_time_ms);
  int time_in_day_ms = DateCache::TimeInDay(local_time_ms, days);
  int year, month, day;
  date_cache->YearMonthDayFromDays(days, &year, &month, &day);
  int weekday = date_cache->Weekday(days);
  int hour = time_in_day_ms / (60 * 60 * 1000);
  int min = (time_in_day_ms / (60 * 1000)) % 60;
  int sec = (time_in_day_ms / 1000) % 60;
  set_cache_stamp(date_cache->stamp());
  set_year(Smi::FromInt(year), SKIP_WRITE_BARRIER);
  set_month(Smi::FromInt(month), SKIP_WRITE_BARRIER);
  set_day(Smi::FromInt(day), SKIP_WRITE_BARRIER);
  set_weekday(Smi::FromInt(weekday), SKIP_WRITE_BARRIER);
  set_hour(Smi::FromInt(hour), SKIP_WRITE_BARRIER);
  set_min(Smi::FromInt(min), SKIP_WRITE_BARRIER);
  set_sec(Smi::FromInt(sec), SKIP_WRITE_BARRIER);
}


void JSArrayBuffer::Neuter() {
  CHECK(is_neuterable());
  CHECK(is_external());
  set_backing_store(NULL);
  set_byte_length(Smi::FromInt(0));
  set_was_neutered(true);
}


void JSArrayBuffer::Setup(Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
                          bool is_external, void* data, size_t allocated_length,
                          SharedFlag shared) {
  DCHECK(array_buffer->GetInternalFieldCount() ==
         v8::ArrayBuffer::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBuffer::kInternalFieldCount; i++) {
    array_buffer->SetInternalField(i, Smi::FromInt(0));
  }
  array_buffer->set_bit_field(0);
  array_buffer->set_is_external(is_external);
  array_buffer->set_is_neuterable(shared == SharedFlag::kNotShared);
  array_buffer->set_is_shared(shared == SharedFlag::kShared);

  Handle<Object> byte_length =
      isolate->factory()->NewNumberFromSize(allocated_length);
  CHECK(byte_length->IsSmi() || byte_length->IsHeapNumber());
  array_buffer->set_byte_length(*byte_length);
  // Initialize backing store at last to avoid handling of |JSArrayBuffers| that
  // are currently being constructed in the |ArrayBufferTracker|. The
  // registration method below handles the case of registering a buffer that has
  // already been promoted.
  array_buffer->set_backing_store(data);

  if (data && !is_external) {
    isolate->heap()->RegisterNewArrayBuffer(*array_buffer);
  }
}


bool JSArrayBuffer::SetupAllocatingData(Handle<JSArrayBuffer> array_buffer,
                                        Isolate* isolate,
                                        size_t allocated_length,
                                        bool initialize, SharedFlag shared) {
  void* data;
  CHECK(isolate->array_buffer_allocator() != NULL);
  // Prevent creating array buffers when serializing.
  DCHECK(!isolate->serializer_enabled());
  if (allocated_length != 0) {
    if (initialize) {
      data = isolate->array_buffer_allocator()->Allocate(allocated_length);
    } else {
      data = isolate->array_buffer_allocator()->AllocateUninitialized(
          allocated_length);
    }
    if (data == NULL) return false;
  } else {
    data = NULL;
  }

  JSArrayBuffer::Setup(array_buffer, isolate, false, data, allocated_length,
                       shared);
  return true;
}


Handle<JSArrayBuffer> JSTypedArray::MaterializeArrayBuffer(
    Handle<JSTypedArray> typed_array) {

  Handle<Map> map(typed_array->map());
  Isolate* isolate = typed_array->GetIsolate();

  DCHECK(IsFixedTypedArrayElementsKind(map->elements_kind()));

  Handle<FixedTypedArrayBase> fixed_typed_array(
      FixedTypedArrayBase::cast(typed_array->elements()));

  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(typed_array->buffer()),
                               isolate);
  void* backing_store =
      isolate->array_buffer_allocator()->AllocateUninitialized(
          fixed_typed_array->DataSize());
  buffer->set_is_external(false);
  DCHECK(buffer->byte_length()->IsSmi() ||
         buffer->byte_length()->IsHeapNumber());
  DCHECK(NumberToInt32(buffer->byte_length()) == fixed_typed_array->DataSize());
  // Initialize backing store at last to avoid handling of |JSArrayBuffers| that
  // are currently being constructed in the |ArrayBufferTracker|. The
  // registration method below handles the case of registering a buffer that has
  // already been promoted.
  buffer->set_backing_store(backing_store);
  isolate->heap()->RegisterNewArrayBuffer(*buffer);
  memcpy(buffer->backing_store(),
         fixed_typed_array->DataPtr(),
         fixed_typed_array->DataSize());
  Handle<FixedTypedArrayBase> new_elements =
      isolate->factory()->NewFixedTypedArrayWithExternalPointer(
          fixed_typed_array->length(), typed_array->type(),
          static_cast<uint8_t*>(buffer->backing_store()));

  typed_array->set_elements(*new_elements);

  return buffer;
}


Handle<JSArrayBuffer> JSTypedArray::GetBuffer() {
  Handle<JSArrayBuffer> array_buffer(JSArrayBuffer::cast(buffer()),
                                     GetIsolate());
  if (array_buffer->was_neutered() ||
      array_buffer->backing_store() != nullptr) {
    return array_buffer;
  }
  Handle<JSTypedArray> self(this);
  return MaterializeArrayBuffer(self);
}


Handle<PropertyCell> PropertyCell::InvalidateEntry(
    Handle<GlobalDictionary> dictionary, int entry) {
  Isolate* isolate = dictionary->GetIsolate();
  // Swap with a copy.
  DCHECK(dictionary->ValueAt(entry)->IsPropertyCell());
  Handle<PropertyCell> cell(PropertyCell::cast(dictionary->ValueAt(entry)));
  auto new_cell = isolate->factory()->NewPropertyCell();
  new_cell->set_value(cell->value());
  dictionary->ValueAtPut(entry, *new_cell);
  bool is_the_hole = cell->value()->IsTheHole();
  // Cell is officially mutable henceforth.
  PropertyDetails details = cell->property_details();
  details = details.set_cell_type(is_the_hole ? PropertyCellType::kInvalidated
                                              : PropertyCellType::kMutable);
  new_cell->set_property_details(details);
  // Old cell is ready for invalidation.
  if (is_the_hole) {
    cell->set_value(isolate->heap()->undefined_value());
  } else {
    cell->set_value(isolate->heap()->the_hole_value());
  }
  details = details.set_cell_type(PropertyCellType::kInvalidated);
  cell->set_property_details(details);
  cell->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kPropertyCellChangedGroup);
  return new_cell;
}


PropertyCellConstantType PropertyCell::GetConstantType() {
  if (value()->IsSmi()) return PropertyCellConstantType::kSmi;
  return PropertyCellConstantType::kStableMap;
}


static bool RemainsConstantType(Handle<PropertyCell> cell,
                                Handle<Object> value) {
  // TODO(dcarney): double->smi and smi->double transition from kConstant
  if (cell->value()->IsSmi() && value->IsSmi()) {
    return true;
  } else if (cell->value()->IsHeapObject() && value->IsHeapObject()) {
    return HeapObject::cast(cell->value())->map() ==
               HeapObject::cast(*value)->map() &&
           HeapObject::cast(*value)->map()->is_stable();
  }
  return false;
}


PropertyCellType PropertyCell::UpdatedType(Handle<PropertyCell> cell,
                                           Handle<Object> value,
                                           PropertyDetails details) {
  PropertyCellType type = details.cell_type();
  DCHECK(!value->IsTheHole());
  if (cell->value()->IsTheHole()) {
    switch (type) {
      // Only allow a cell to transition once into constant state.
      case PropertyCellType::kUninitialized:
        if (value->IsUndefined()) return PropertyCellType::kUndefined;
        return PropertyCellType::kConstant;
      case PropertyCellType::kInvalidated:
        return PropertyCellType::kMutable;
      default:
        UNREACHABLE();
        return PropertyCellType::kMutable;
    }
  }
  switch (type) {
    case PropertyCellType::kUndefined:
      return PropertyCellType::kConstant;
    case PropertyCellType::kConstant:
      if (*value == cell->value()) return PropertyCellType::kConstant;
    // Fall through.
    case PropertyCellType::kConstantType:
      if (RemainsConstantType(cell, value)) {
        return PropertyCellType::kConstantType;
      }
    // Fall through.
    case PropertyCellType::kMutable:
      return PropertyCellType::kMutable;
  }
  UNREACHABLE();
  return PropertyCellType::kMutable;
}


void PropertyCell::UpdateCell(Handle<GlobalDictionary> dictionary, int entry,
                              Handle<Object> value, PropertyDetails details) {
  DCHECK(!value->IsTheHole());
  DCHECK(dictionary->ValueAt(entry)->IsPropertyCell());
  Handle<PropertyCell> cell(PropertyCell::cast(dictionary->ValueAt(entry)));
  const PropertyDetails original_details = cell->property_details();
  // Data accesses could be cached in ics or optimized code.
  bool invalidate =
      original_details.kind() == kData && details.kind() == kAccessor;
  int index = original_details.dictionary_index();
  PropertyCellType old_type = original_details.cell_type();
  // Preserve the enumeration index unless the property was deleted or never
  // initialized.
  if (cell->value()->IsTheHole()) {
    index = dictionary->NextEnumerationIndex();
    dictionary->SetNextEnumerationIndex(index + 1);
    // Negative lookup cells must be invalidated.
    invalidate = true;
  }
  DCHECK(index > 0);
  details = details.set_index(index);

  PropertyCellType new_type = UpdatedType(cell, value, original_details);
  if (invalidate) cell = PropertyCell::InvalidateEntry(dictionary, entry);

  // Install new property details and cell value.
  details = details.set_cell_type(new_type);
  cell->set_property_details(details);
  cell->set_value(*value);

  // Deopt when transitioning from a constant type.
  if (!invalidate && (old_type != new_type ||
                      original_details.IsReadOnly() != details.IsReadOnly())) {
    Isolate* isolate = dictionary->GetIsolate();
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
}


// static
void PropertyCell::SetValueWithInvalidation(Handle<PropertyCell> cell,
                                            Handle<Object> new_value) {
  if (cell->value() != *new_value) {
    cell->set_value(*new_value);
    Isolate* isolate = cell->GetIsolate();
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
}

}  // namespace internal
}  // namespace v8
