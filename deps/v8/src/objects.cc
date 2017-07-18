// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects.h"

#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "src/objects-inl.h"

#include "src/accessors.h"
#include "src/allocation-site-scopes.h"
#include "src/api-arguments-inl.h"
#include "src/api-natives.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/builtins/builtins.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/compilation-dependencies.h"
#include "src/compiler.h"
#include "src/counters-inl.h"
#include "src/counters.h"
#include "src/date.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/elements.h"
#include "src/execution.h"
#include "src/field-index-inl.h"
#include "src/field-index.h"
#include "src/field-type.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/globals.h"
#include "src/ic/ic.h"
#include "src/identity-map.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-decoder.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/list.h"
#include "src/log.h"
#include "src/lookup.h"
#include "src/macro-assembler.h"
#include "src/map-updater.h"
#include "src/messages.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/objects/code-cache-inl.h"
#include "src/objects/compilation-cache-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/map.h"
#include "src/property-descriptor.h"
#include "src/prototype.h"
#include "src/regexp/jsregexp.h"
#include "src/safepoint-table.h"
#include "src/snapshot/code-serializer.h"
#include "src/source-position-table.h"
#include "src/string-builder.h"
#include "src/string-search.h"
#include "src/string-stream.h"
#include "src/utils.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/zone.h"

#ifdef ENABLE_DISASSEMBLER
#include "src/disasm.h"
#include "src/disassembler.h"
#include "src/eh-frame.h"
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
                                         Handle<Context> native_context,
                                         const char* method_name) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  Handle<JSFunction> constructor;
  if (object->IsSmi()) {
    constructor = handle(native_context->number_function(), isolate);
  } else {
    int constructor_function_index =
        Handle<HeapObject>::cast(object)->map()->GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      if (method_name != nullptr) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(
                MessageTemplate::kCalledOnNullOrUndefined,
                isolate->factory()->NewStringFromAsciiChecked(method_name)),
            JSReceiver);
      }
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

// ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
// static
MaybeHandle<JSReceiver> Object::ConvertReceiver(Isolate* isolate,
                                                Handle<Object> object) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  if (*object == isolate->heap()->null_value() ||
      object->IsUndefined(isolate)) {
    return isolate->global_proxy();
  }
  return Object::ToObject(isolate, object);
}

// static
MaybeHandle<Object> Object::ConvertToNumber(Isolate* isolate,
                                            Handle<Object> input) {
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
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToNumber),
                      Object);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kNumber),
        Object);
  }
}

// static
MaybeHandle<Object> Object::ConvertToInteger(Isolate* isolate,
                                             Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ConvertToNumber(isolate, input),
                             Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumber(DoubleToInteger(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToInt32(Isolate* isolate,
                                           Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ConvertToNumber(isolate, input),
                             Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumberFromInt(DoubleToInt32(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToUint32(Isolate* isolate,
                                            Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ConvertToNumber(isolate, input),
                             Object);
  if (input->IsSmi()) return handle(Smi::cast(*input)->ToUint32Smi(), isolate);
  return isolate->factory()->NewNumberFromUint(DoubleToUint32(input->Number()));
}

// static
MaybeHandle<Name> Object::ConvertToName(Isolate* isolate,
                                        Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input, Object::ToPrimitive(input, ToPrimitiveHint::kString),
      Name);
  if (input->IsName()) return Handle<Name>::cast(input);
  return ToString(isolate, input);
}

// ES6 7.1.14
// static
MaybeHandle<Object> Object::ConvertToPropertyKey(Isolate* isolate,
                                                 Handle<Object> value) {
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

// static
MaybeHandle<String> Object::ConvertToString(Isolate* isolate,
                                            Handle<Object> input) {
  while (true) {
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
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input, JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                                ToPrimitiveHint::kString),
        String);
    // The previous isString() check happened in Object::ToString and thus we
    // put it at the end of the loop in this helper.
    if (input->IsString()) {
      return Handle<String>::cast(input);
    }
  }
}

namespace {

bool IsErrorObject(Isolate* isolate, Handle<Object> object) {
  if (!object->IsJSReceiver()) return false;
  Handle<Symbol> symbol = isolate->factory()->stack_trace_symbol();
  return JSReceiver::HasOwnProperty(Handle<JSReceiver>::cast(object), symbol)
      .FromMaybe(false);
}

Handle<String> AsStringOrEmpty(Isolate* isolate, Handle<Object> object) {
  return object->IsString() ? Handle<String>::cast(object)
                            : isolate->factory()->empty_string();
}

Handle<String> NoSideEffectsErrorToString(Isolate* isolate,
                                          Handle<Object> input) {
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(input);

  Handle<Name> name_key = isolate->factory()->name_string();
  Handle<Object> name = JSReceiver::GetDataProperty(receiver, name_key);
  Handle<String> name_str = AsStringOrEmpty(isolate, name);

  Handle<Name> msg_key = isolate->factory()->message_string();
  Handle<Object> msg = JSReceiver::GetDataProperty(receiver, msg_key);
  Handle<String> msg_str = AsStringOrEmpty(isolate, msg);

  if (name_str->length() == 0) return msg_str;
  if (msg_str->length() == 0) return name_str;

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(name_str);
  builder.AppendCString(": ");
  builder.AppendString(msg_str);

  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<String> Object::NoSideEffectsToString(Isolate* isolate,
                                             Handle<Object> input) {
  DisallowJavascriptExecution no_js(isolate);

  if (input->IsString() || input->IsNumber() || input->IsOddball()) {
    return Object::ToString(isolate, input).ToHandleChecked();
  } else if (input->IsFunction()) {
    // -- F u n c t i o n
    Handle<String> fun_str;
    if (input->IsJSBoundFunction()) {
      fun_str = JSBoundFunction::ToString(Handle<JSBoundFunction>::cast(input));
    } else {
      DCHECK(input->IsJSFunction());
      fun_str = JSFunction::ToString(Handle<JSFunction>::cast(input));
    }

    if (fun_str->length() > 128) {
      IncrementalStringBuilder builder(isolate);
      builder.AppendString(isolate->factory()->NewSubString(fun_str, 0, 111));
      builder.AppendCString("...<omitted>...");
      builder.AppendString(isolate->factory()->NewSubString(
          fun_str, fun_str->length() - 2, fun_str->length()));

      return builder.Finish().ToHandleChecked();
    }
    return fun_str;
  } else if (input->IsSymbol()) {
    // -- S y m b o l
    Handle<Symbol> symbol = Handle<Symbol>::cast(input);

    IncrementalStringBuilder builder(isolate);
    builder.AppendCString("Symbol(");
    if (symbol->name()->IsString()) {
      builder.AppendString(handle(String::cast(symbol->name()), isolate));
    }
    builder.AppendCharacter(')');

    return builder.Finish().ToHandleChecked();
  } else if (input->IsJSReceiver()) {
    // -- J S R e c e i v e r
    Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(input);
    Handle<Object> to_string = JSReceiver::GetDataProperty(
        receiver, isolate->factory()->toString_string());

    if (IsErrorObject(isolate, input) ||
        *to_string == *isolate->error_to_string()) {
      // When internally formatting error objects, use a side-effects-free
      // version of Error.prototype.toString independent of the actually
      // installed toString method.
      return NoSideEffectsErrorToString(isolate, input);
    } else if (*to_string == *isolate->object_to_string()) {
      Handle<Object> ctor = JSReceiver::GetDataProperty(
          receiver, isolate->factory()->constructor_string());
      if (ctor->IsFunction()) {
        Handle<String> ctor_name;
        if (ctor->IsJSBoundFunction()) {
          ctor_name = JSBoundFunction::GetName(
                          isolate, Handle<JSBoundFunction>::cast(ctor))
                          .ToHandleChecked();
        } else if (ctor->IsJSFunction()) {
          Handle<Object> ctor_name_obj =
              JSFunction::GetName(isolate, Handle<JSFunction>::cast(ctor));
          ctor_name = AsStringOrEmpty(isolate, ctor_name_obj);
        }

        if (ctor_name->length() != 0) {
          IncrementalStringBuilder builder(isolate);
          builder.AppendCString("#<");
          builder.AppendString(ctor_name);
          builder.AppendCString(">");

          return builder.Finish().ToHandleChecked();
        }
      }
    }
  }

  // At this point, input is either none of the above or a JSReceiver.

  Handle<JSReceiver> receiver;
  if (input->IsJSReceiver()) {
    receiver = Handle<JSReceiver>::cast(input);
  } else {
    // This is the only case where Object::ToObject throws.
    DCHECK(!input->IsSmi());
    int constructor_function_index =
        Handle<HeapObject>::cast(input)->map()->GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      return isolate->factory()->NewStringFromAsciiChecked("[object Unknown]");
    }

    receiver = Object::ToObject(isolate, input, isolate->native_context())
                   .ToHandleChecked();
  }

  Handle<String> builtin_tag = handle(receiver->class_name(), isolate);
  Handle<Object> tag_obj = JSReceiver::GetDataProperty(
      receiver, isolate->factory()->to_string_tag_symbol());
  Handle<String> tag =
      tag_obj->IsString() ? Handle<String>::cast(tag_obj) : builtin_tag;

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("[object ");
  builder.AppendString(tag);
  builder.AppendCString("]");

  return builder.Finish().ToHandleChecked();
}

// static
MaybeHandle<Object> Object::ConvertToLength(Isolate* isolate,
                                            Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  if (input->IsSmi()) {
    int value = std::max(Smi::cast(*input)->value(), 0);
    return handle(Smi::FromInt(value), isolate);
  }
  double len = DoubleToInteger(input->Number());
  if (len <= 0.0) {
    return handle(Smi::kZero, isolate);
  } else if (len >= kMaxSafeInteger) {
    len = kMaxSafeInteger;
  }
  return isolate->factory()->NewNumber(len);
}

// static
MaybeHandle<Object> Object::ConvertToIndex(
    Isolate* isolate, Handle<Object> input,
    MessageTemplate::Template error_index) {
  if (input->IsUndefined(isolate)) return handle(Smi::kZero, isolate);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(input), Object);
  if (input->IsSmi() && Smi::cast(*input)->value() >= 0) return input;
  double len = DoubleToInteger(input->Number()) + 0.0;
  auto js_len = isolate->factory()->NewNumber(len);
  if (len < 0.0 || len > kMaxSafeInteger) {
    THROW_NEW_ERROR(isolate, NewRangeError(error_index, js_len), Object);
  }
  return js_len;
}

bool Object::BooleanValue() {
  if (IsSmi()) return Smi::cast(this)->value() != 0;
  DCHECK(IsHeapObject());
  Isolate* isolate = HeapObject::cast(this)->GetIsolate();
  if (IsBoolean()) return IsTrue(isolate);
  if (IsNullOrUndefined(isolate)) return false;
  if (IsUndetectable()) return false;  // Undetectable object is false.
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
  // This is the generic version of Abstract Equality Comparison; a version in
  // JavaScript land is available in the EqualStub and NotEqualStub. Whenever
  // you change something functionality wise in here, remember to update the
  // TurboFan code stubs as well.
  while (true) {
    if (x->IsNumber()) {
      if (y->IsNumber()) {
        return Just(NumberEquals(x, y));
      } else if (y->IsBoolean()) {
        return Just(NumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsString()) {
        return Just(NumberEquals(x, String::ToNumber(Handle<String>::cast(y))));
      } else if (y->IsJSReceiver()) {
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
      } else if (y->IsJSReceiver()) {
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
      } else if (y->IsJSReceiver()) {
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
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
      } else {
        return Just(false);
      }
    } else if (x->IsJSReceiver()) {
      if (y->IsJSReceiver()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsUndetectable()) {
        return Just(x->IsUndetectable());
      } else if (y->IsBoolean()) {
        y = Oddball::ToNumber(Handle<Oddball>::cast(y));
      } else if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(x))
                      .ToHandle(&x)) {
        return Nothing<bool>();
      }
    } else {
      return Just(x->IsUndetectable() && y->IsUndetectable());
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
  }
  return this == that;
}


// static
Handle<String> Object::TypeOf(Isolate* isolate, Handle<Object> object) {
  if (object->IsNumber()) return isolate->factory()->number_string();
  if (object->IsOddball()) return handle(Oddball::cast(*object)->type_of());
  if (object->IsUndetectable()) {
    return isolate->factory()->undefined_string();
  }
  if (object->IsString()) return isolate->factory()->string_string();
  if (object->IsSymbol()) return isolate->factory()->symbol_string();
  if (object->IsString()) return isolate->factory()->string_string();
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

// static
MaybeHandle<Object> Object::OrdinaryHasInstance(Isolate* isolate,
                                                Handle<Object> callable,
                                                Handle<Object> object) {
  // The {callable} must have a [[Call]] internal method.
  if (!callable->IsCallable()) return isolate->factory()->false_value();

  // Check if {callable} is a bound function, and if so retrieve its
  // [[BoundTargetFunction]] and use that instead of {callable}.
  if (callable->IsJSBoundFunction()) {
    Handle<Object> bound_callable(
        Handle<JSBoundFunction>::cast(callable)->bound_target_function(),
        isolate);
    return Object::InstanceOf(isolate, object, bound_callable);
  }

  // If {object} is not a receiver, return false.
  if (!object->IsJSReceiver()) return isolate->factory()->false_value();

  // Get the "prototype" of {callable}; raise an error if it's not a receiver.
  Handle<Object> prototype;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, prototype,
      Object::GetProperty(callable, isolate->factory()->prototype_string()),
      Object);
  if (!prototype->IsJSReceiver()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kInstanceofNonobjectProto, prototype),
        Object);
  }

  // Return whether or not {prototype} is in the prototype chain of {object}.
  Maybe<bool> result = JSReceiver::HasInPrototypeChain(
      isolate, Handle<JSReceiver>::cast(object), prototype);
  if (result.IsNothing()) return MaybeHandle<Object>();
  return isolate->factory()->ToBoolean(result.FromJust());
}

// static
MaybeHandle<Object> Object::InstanceOf(Isolate* isolate, Handle<Object> object,
                                       Handle<Object> callable) {
  // The {callable} must be a receiver.
  if (!callable->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kNonObjectInInstanceOfCheck),
                    Object);
  }

  // Lookup the @@hasInstance method on {callable}.
  Handle<Object> inst_of_handler;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, inst_of_handler,
      JSReceiver::GetMethod(Handle<JSReceiver>::cast(callable),
                            isolate->factory()->has_instance_symbol()),
      Object);
  if (!inst_of_handler->IsUndefined(isolate)) {
    // Call the {inst_of_handler} on the {callable}.
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, inst_of_handler, callable, 1, &object),
        Object);
    return isolate->factory()->ToBoolean(result->BooleanValue());
  }

  // The {callable} must have a [[Call]] internal method.
  if (!callable->IsCallable()) {
    THROW_NEW_ERROR(
        isolate, NewTypeError(MessageTemplate::kNonCallableInInstanceOfCheck),
        Object);
  }

  // Fall back to OrdinaryHasInstance with {callable} and {object}.
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      JSReceiver::OrdinaryHasInstance(isolate, callable, object), Object);
  return result;
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


// static
MaybeHandle<Object> Object::GetMethod(Handle<JSReceiver> receiver,
                                      Handle<Name> name) {
  Handle<Object> func;
  Isolate* isolate = receiver->GetIsolate();
  ASSIGN_RETURN_ON_EXCEPTION(isolate, func,
                             JSReceiver::GetProperty(receiver, name), Object);
  if (func->IsNullOrUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  if (!func->IsCallable()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kPropertyNotFunction,
                                          func, name, receiver),
                    Object);
  }
  return func;
}

namespace {
MaybeHandle<FixedArray> CreateListFromArrayLikeFastPath(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  if (element_types != ElementTypes::kAll || !object->IsJSArray()) {
    return MaybeHandle<FixedArray>();
  }
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  uint32_t length;
  if (!array->HasArrayPrototype(isolate) ||
      !array->length()->ToUint32(&length) || !array->HasFastElements() ||
      !JSObject::PrototypeHasNoElements(isolate, *array)) {
    return MaybeHandle<FixedArray>();
  }
  return array->GetElementsAccessor()->CreateListFromArray(isolate, array);
}
}  // namespace

// static
MaybeHandle<FixedArray> Object::CreateListFromArrayLike(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  // Fast-path for JS_ARRAY_TYPE.
  MaybeHandle<FixedArray> fast_result =
      CreateListFromArrayLikeFastPath(isolate, object, element_types);
  if (!fast_result.is_null()) return fast_result;
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
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, raw_length_number,
                             Object::GetLengthFromArrayLike(isolate, receiver),
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
    ASSIGN_RETURN_ON_EXCEPTION(isolate, next,
                               JSReceiver::GetElement(isolate, receiver, index),
                               FixedArray);
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
MaybeHandle<Object> Object::GetLengthFromArrayLike(Isolate* isolate,
                                                   Handle<Object> object) {
  Handle<Object> val;
  Handle<Object> key = isolate->factory()->length_string();
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, val, Runtime::GetObjectProperty(isolate, object, key), Object);
  return Object::ToLength(isolate, val);
}

// static
Maybe<bool> JSReceiver::HasProperty(LookupIterator* it) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return JSProxy::HasProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                    it->GetName());
      case LookupIterator::INTERCEPTOR: {
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithInterceptor(it);
        if (result.IsNothing()) return Nothing<bool>();
        if (result.FromJust() != ABSENT) return Just(true);
        break;
      }
      case LookupIterator::ACCESS_CHECK: {
        if (it->HasAccess()) break;
        Maybe<PropertyAttributes> result =
            JSObject::GetPropertyAttributesWithFailedAccessCheck(it);
        if (result.IsNothing()) return Nothing<bool>();
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
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }
  return it->isolate()->factory()->undefined_value();
}


// static
MaybeHandle<Object> JSProxy::GetProperty(Isolate* isolate,
                                         Handle<JSProxy> proxy,
                                         Handle<Name> name,
                                         Handle<Object> receiver,
                                         bool* was_found) {
  *was_found = true;

  DCHECK(!name->IsPrivate());
  STACK_CHECK(isolate, MaybeHandle<Object>());
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
  if (trap->IsUndefined(isolate)) {
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
                   target_desc.get()->IsUndefined(isolate) &&
                   !trap_result->IsUndefined(isolate);
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

Handle<SharedFunctionInfo> FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
    Isolate* isolate, Handle<FunctionTemplateInfo> info) {
  Object* current_info = info->shared_function_info();
  if (current_info->IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(current_info), isolate);
  }

  Handle<Object> class_name(info->class_name(), isolate);
  Handle<String> name = class_name->IsString()
                            ? Handle<String>::cast(class_name)
                            : isolate->factory()->empty_string();
  Handle<Code> code = isolate->builtins()->HandleApiCall();
  bool is_constructor = !info->remove_prototype();
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfo(name, code, is_constructor);
  if (is_constructor) {
    result->SetConstructStub(*isolate->builtins()->JSConstructStubApi());
  }

  result->set_length(info->length());
  if (class_name->IsString()) result->set_instance_class_name(*class_name);
  result->set_api_func_data(*info);
  result->DontAdaptArguments();
  DCHECK(result->IsApiFunction());

  info->set_shared_function_info(*result);
  return result;
}

bool FunctionTemplateInfo::IsTemplateFor(Map* map) {
  // There is a constraint on the object; check.
  if (!map->IsJSObjectMap()) return false;
  // Fetch the constructor function of the object.
  Object* cons_obj = map->GetConstructor();
  Object* type;
  if (cons_obj->IsJSFunction()) {
    JSFunction* fun = JSFunction::cast(cons_obj);
    type = fun->shared()->function_data();
  } else if (cons_obj->IsFunctionTemplateInfo()) {
    type = FunctionTemplateInfo::cast(cons_obj);
  } else {
    return false;
  }
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  while (type->IsFunctionTemplateInfo()) {
    if (type == this) return true;
    type = FunctionTemplateInfo::cast(type)->parent_template();
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


// static
Handle<TemplateList> TemplateList::New(Isolate* isolate, int size) {
  Handle<FixedArray> list =
      isolate->factory()->NewFixedArray(kLengthIndex + size);
  list->set(kLengthIndex, Smi::kZero);
  return Handle<TemplateList>::cast(list);
}

// static
Handle<TemplateList> TemplateList::Add(Isolate* isolate,
                                       Handle<TemplateList> list,
                                       Handle<i::Object> value) {
  STATIC_ASSERT(kFirstElementIndex == 1);
  int index = list->length() + 1;
  Handle<i::FixedArray> fixed_array = Handle<FixedArray>::cast(list);
  fixed_array = FixedArray::SetAndGrow(fixed_array, index, value);
  fixed_array->set(kLengthIndex, Smi::FromInt(index));
  return Handle<TemplateList>::cast(fixed_array);
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
  if (initial_map->is_dictionary_map()) {
    Handle<NameDictionary> dictionary =
        NameDictionary::New(isolate, NameDictionary::kInitialCapacity);
    result->set_properties(*dictionary);
  }
  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();
  return result;
}

void JSObject::EnsureWritableFastElements(Handle<JSObject> object) {
  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastStringWrapperElements());
  FixedArray* raw_elems = FixedArray::cast(object->elements());
  Heap* heap = object->GetHeap();
  if (raw_elems->map() != heap->fixed_cow_array_map()) return;
  Isolate* isolate = heap->isolate();
  Handle<FixedArray> elems(raw_elems, isolate);
  Handle<FixedArray> writable_elems = isolate->factory()->CopyFixedArrayWithMap(
      elems, isolate->factory()->fixed_array_map());
  object->set_elements(*writable_elems);
  isolate->counters()->cow_arrays_converted()->Increment();
}


// ES6 9.5.1
// static
MaybeHandle<Object> JSProxy::GetPrototype(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> trap_name = isolate->factory()->getPrototypeOf_string();

  STACK_CHECK(isolate, MaybeHandle<Object>());

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
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::GetPrototype(isolate, target);
  }
  // 7. Let handlerProto be ? Call(trap, handler, «target»).
  Handle<Object> argv[] = {target};
  Handle<Object> handler_proto;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, handler_proto,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv), Object);
  // 8. If Type(handlerProto) is neither Object nor Null, throw a TypeError.
  if (!(handler_proto->IsJSReceiver() || handler_proto->IsNull(isolate))) {
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

    if (info->is_sloppy() && !receiver->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                                 Object::ConvertReceiver(isolate, receiver),
                                 Object);
    }

    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   Object::DONT_THROW);
    Handle<Object> result = args.Call(call_fun, name);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.is_null()) return isolate->factory()->undefined_value();
    Handle<Object> reboxed_result = handle(*result, isolate);
    if (info->replace_on_access() && receiver->IsJSReceiver()) {
      args.Call(reinterpret_cast<GenericNamedPropertySetterCallback>(
                    &Accessors::ReconfigureToDataProperty),
                name, result);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    }
    return reboxed_result;
  }

  // AccessorPair with 'cached' private property.
  if (it->TryLookupCachedProperty()) {
    return Object::GetProperty(it);
  }

  // Regular accessor.
  Handle<Object> getter(AccessorPair::cast(*structure)->getter(), isolate);
  if (getter->IsFunctionTemplateInfo()) {
    return Builtins::InvokeApiFunction(
        isolate, false, Handle<FunctionTemplateInfo>::cast(getter), receiver, 0,
        nullptr, isolate->factory()->undefined_value());
  } else if (getter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return Object::GetPropertyWithDefinedGetter(
        receiver, Handle<JSReceiver>::cast(getter));
  }
  // Getter is not a function.
  return isolate->factory()->undefined_value();
}

// static
Address AccessorInfo::redirect(Isolate* isolate, Address address,
                               AccessorComponent component) {
  ApiFunction fun(address);
  DCHECK_EQ(ACCESSOR_GETTER, component);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  return ExternalReference(&fun, type, isolate).address();
}

Address AccessorInfo::redirected_getter() const {
  Address accessor = v8::ToCData<Address>(getter());
  if (accessor == nullptr) return nullptr;
  return redirect(GetIsolate(), accessor, ACCESSOR_GETTER);
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

    // The actual type of call_fun is either v8::AccessorNameSetterCallback or
    // i::Accesors::AccessorNameBooleanSetterCallback, depending on whether the
    // AccessorInfo was created by the API or internally (see accessors.cc).
    // Here we handle both cases using GenericNamedPropertySetterCallback and
    // its Call method.
    GenericNamedPropertySetterCallback call_fun =
        v8::ToCData<GenericNamedPropertySetterCallback>(info->setter());

    if (call_fun == nullptr) {
      // TODO(verwaest): We should not get here anymore once all AccessorInfos
      // are marked as special_data_property. They cannot both be writable and
      // not have a setter.
      return Just(true);
    }

    if (info->is_sloppy() && !receiver->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, receiver, Object::ConvertReceiver(isolate, receiver),
          Nothing<bool>());
    }

    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   should_throw);
    Handle<Object> result = args.Call(call_fun, name, value);
    // In the case of AccessorNameSetterCallback, we know that the result value
    // cannot have been set, so the result of Call will be null.  In the case of
    // AccessorNameBooleanSetterCallback, the result will either be null
    // (signalling an exception) or a boolean Oddball.
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    if (result.is_null()) return Just(true);
    DCHECK(result->BooleanValue() || should_throw == DONT_THROW);
    return Just(result->BooleanValue());
  }

  // Regular accessor.
  Handle<Object> setter(AccessorPair::cast(*structure)->setter(), isolate);
  if (setter->IsFunctionTemplateInfo()) {
    Handle<Object> argv[] = {value};
    RETURN_ON_EXCEPTION_VALUE(
        isolate, Builtins::InvokeApiFunction(
                     isolate, false, Handle<FunctionTemplateInfo>::cast(setter),
                     receiver, arraysize(argv), argv,
                     isolate->factory()->undefined_value()),
        Nothing<bool>());
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

namespace {

MaybeHandle<Object> GetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor, bool* done) {
  *done = false;
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->getter()->IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver), Object);
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Object::DONT_THROW);

  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertyGetterCallback getter =
        v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
    result = args.Call(getter, index);
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());

    DCHECK_IMPLIES(name->IsSymbol(), interceptor->can_intercept_symbols());

    v8::GenericNamedPropertyGetterCallback getter =
        v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
            interceptor->getter());
    result = args.Call(getter, name);
  }

  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  if (result.is_null()) return isolate->factory()->undefined_value();
  *done = true;
  // Rebox handle before return
  return handle(*result, isolate);
}

Maybe<PropertyAttributes> GetPropertyAttributesWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc(isolate);
  HandleScope scope(isolate);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  DCHECK_IMPLIES(!it->IsElement() && it->name()->IsSymbol(),
                 interceptor->can_intercept_symbols());
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<PropertyAttributes>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, Object::DONT_THROW);
  if (!interceptor->query()->IsUndefined(isolate)) {
    Handle<Object> result;
    if (it->IsElement()) {
      uint32_t index = it->index();
      v8::IndexedPropertyQueryCallback query =
          v8::ToCData<v8::IndexedPropertyQueryCallback>(interceptor->query());
      result = args.Call(query, index);
    } else {
      Handle<Name> name = it->name();
      DCHECK(!name->IsPrivate());
      v8::GenericNamedPropertyQueryCallback query =
          v8::ToCData<v8::GenericNamedPropertyQueryCallback>(
              interceptor->query());
      result = args.Call(query, name);
    }
    if (!result.is_null()) {
      int32_t value;
      CHECK(result->ToInt32(&value));
      return Just(static_cast<PropertyAttributes>(value));
    }
  } else if (!interceptor->getter()->IsUndefined(isolate)) {
    // TODO(verwaest): Use GetPropertyWithInterceptor?
    Handle<Object> result;
    if (it->IsElement()) {
      uint32_t index = it->index();
      v8::IndexedPropertyGetterCallback getter =
          v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
      result = args.Call(getter, index);
    } else {
      Handle<Name> name = it->name();
      DCHECK(!name->IsPrivate());
      v8::GenericNamedPropertyGetterCallback getter =
          v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
              interceptor->getter());
      result = args.Call(getter, name);
    }
    if (!result.is_null()) return Just(DONT_ENUM);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
  return Just(ABSENT);
}

Maybe<bool> SetPropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Object::ShouldThrow should_throw, Handle<Object> value) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->setter()->IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertySetterCallback setter =
        v8::ToCData<v8::IndexedPropertySetterCallback>(interceptor->setter());
    // TODO(neis): In the future, we may want to actually return the
    // interceptor's result, which then should be a boolean.
    result = !args.Call(setter, index, value).is_null();
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());

    DCHECK_IMPLIES(name->IsSymbol(), interceptor->can_intercept_symbols());

    v8::GenericNamedPropertySetterCallback setter =
        v8::ToCData<v8::GenericNamedPropertySetterCallback>(
            interceptor->setter());
    result = !args.Call(setter, name, value).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

Maybe<bool> DefinePropertyWithInterceptorInternal(
    LookupIterator* it, Handle<InterceptorInfo> interceptor,
    Object::ShouldThrow should_throw, PropertyDescriptor& desc) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  if (interceptor->definer()->IsUndefined(isolate)) return Just(false);

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  bool result;
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }
  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);

  std::unique_ptr<v8::PropertyDescriptor> descriptor(
      new v8::PropertyDescriptor());
  if (PropertyDescriptor::IsAccessorDescriptor(&desc)) {
    descriptor.reset(new v8::PropertyDescriptor(
        v8::Utils::ToLocal(desc.get()), v8::Utils::ToLocal(desc.set())));
  } else if (PropertyDescriptor::IsDataDescriptor(&desc)) {
    if (desc.has_writable()) {
      descriptor.reset(new v8::PropertyDescriptor(
          v8::Utils::ToLocal(desc.value()), desc.writable()));
    } else {
      descriptor.reset(
          new v8::PropertyDescriptor(v8::Utils::ToLocal(desc.value())));
    }
  }
  if (desc.has_enumerable()) {
    descriptor->set_enumerable(desc.enumerable());
  }
  if (desc.has_configurable()) {
    descriptor->set_configurable(desc.configurable());
  }

  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertyDefinerCallback definer =
        v8::ToCData<v8::IndexedPropertyDefinerCallback>(interceptor->definer());
    result = !args.Call(definer, index, *descriptor).is_null();
  } else {
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());

    DCHECK_IMPLIES(name->IsSymbol(), interceptor->can_intercept_symbols());

    v8::GenericNamedPropertyDefinerCallback definer =
        v8::ToCData<v8::GenericNamedPropertyDefinerCallback>(
            interceptor->definer());
    result = !args.Call(definer, name, *descriptor).is_null();
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(it->isolate(), Nothing<bool>());
  return Just(result);
}

}  // namespace

MaybeHandle<Object> JSObject::GetPropertyWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return GetPropertyWithAccessor(it);
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      bool done;
      Handle<Object> result;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                                 GetPropertyWithInterceptor(it, &done), Object);
      if (done) return result;
    }

  } else {
    Handle<Object> result;
    bool done;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        GetPropertyWithInterceptorInternal(it, interceptor, &done), Object);
    if (done) return result;
  }

  // Cross-Origin [[Get]] of Well-Known Symbols does not throw, and returns
  // undefined.
  Handle<Name> name = it->GetName();
  if (name->IsSymbol() && Symbol::cast(*name)->is_well_known_symbol()) {
    return it->factory()->undefined_value();
  }

  isolate->ReportFailedAccessCheck(checked);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  return it->factory()->undefined_value();
}


Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithFailedAccessCheck(
    LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    while (AllCanRead(it)) {
      if (it->state() == LookupIterator::ACCESSOR) {
        return Just(it->property_attributes());
      }
      DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
      auto result = GetPropertyAttributesWithInterceptor(it);
      if (isolate->has_scheduled_exception()) break;
      if (result.IsJust() && result.FromJust() != ABSENT) return result;
    }
  } else {
    Maybe<PropertyAttributes> result =
        GetPropertyAttributesWithInterceptorInternal(it, interceptor);
    if (isolate->has_pending_exception()) return Nothing<PropertyAttributes>();
    if (result.FromMaybe(ABSENT) != ABSENT) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<PropertyAttributes>());
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
  Isolate* isolate = it->isolate();
  Handle<JSObject> checked = it->GetHolder<JSObject>();
  Handle<InterceptorInfo> interceptor =
      it->GetInterceptorForFailedAccessCheck();
  if (interceptor.is_null()) {
    if (AllCanWrite(it)) {
      return SetPropertyWithAccessor(it, value, should_throw);
    }
  } else {
    Maybe<bool> result = SetPropertyWithInterceptorInternal(
        it, interceptor, should_throw, value);
    if (isolate->has_pending_exception()) return Nothing<bool>();
    if (result.IsJust()) return result;
  }
  isolate->ReportFailedAccessCheck(checked);
  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
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
    Handle<GlobalDictionary> dictionary(object->global_dictionary());

    int entry = dictionary->FindEntry(name);
    if (entry == GlobalDictionary::kNotFound) {
      Isolate* isolate = object->GetIsolate();
      auto cell = isolate->factory()->NewPropertyCell();
      cell->set_value(*value);
      auto cell_type = value->IsUndefined(isolate)
                           ? PropertyCellType::kUndefined
                           : PropertyCellType::kConstant;
      details = details.set_cell_type(cell_type);
      value = cell;
      dictionary = GlobalDictionary::Add(dictionary, name, value, details);
      object->set_properties(*dictionary);
    } else {
      Handle<PropertyCell> cell =
          PropertyCell::PrepareForValue(dictionary, entry, value, details);
      cell->set_value(*value);
    }
  } else {
    Handle<NameDictionary> dictionary(object->property_dictionary());

    int entry = dictionary->FindEntry(name);
    if (entry == NameDictionary::kNotFound) {
      dictionary = NameDictionary::Add(dictionary, name, value, details);
      object->set_properties(*dictionary);
    } else {
      PropertyDetails original_details = dictionary->DetailsAt(entry);
      int enumeration_index = original_details.dictionary_index();
      DCHECK(enumeration_index > 0);
      details = details.set_index(enumeration_index);
      dictionary->SetEntry(entry, name, value, details);
    }
  }
}

// static
Maybe<bool> JSReceiver::HasInPrototypeChain(Isolate* isolate,
                                            Handle<JSReceiver> object,
                                            Handle<Object> proto) {
  PrototypeIterator iter(isolate, object, kStartAtReceiver);
  while (true) {
    if (!iter.AdvanceFollowingProxies()) return Nothing<bool>();
    if (iter.IsAtEnd()) return Just(false);
    if (PrototypeIterator::GetCurrent(iter).is_identical_to(proto)) {
      return Just(true);
    }
  }
}

namespace {

bool HasExcludedProperty(
    const ScopedVector<Handle<Object>>* excluded_properties,
    Handle<Object> search_element) {
  // TODO(gsathya): Change this to be a hashtable.
  for (int i = 0; i < excluded_properties->length(); i++) {
    if (search_element->SameValue(*excluded_properties->at(i))) {
      return true;
    }
  }

  return false;
}

MUST_USE_RESULT Maybe<bool> FastAssign(
    Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  // Non-empty strings are the only non-JSReceivers that need to be handled
  // explicitly by Object.assign.
  if (!source->IsJSReceiver()) {
    return Just(!source->IsString() || String::cast(*source)->length() == 0);
  }

  // If the target is deprecated, the object will be updated on first store. If
  // the source for that store equals the target, this will invalidate the
  // cached representation of the source. Preventively upgrade the target.
  // Do this on each iteration since any property load could cause deprecation.
  if (target->map()->is_deprecated()) {
    JSObject::MigrateInstance(Handle<JSObject>::cast(target));
  }

  Isolate* isolate = target->GetIsolate();
  Handle<Map> map(JSReceiver::cast(*source)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> from = Handle<JSObject>::cast(source);
  if (from->elements() != isolate->heap()->empty_fixed_array()) {
    return Just(false);
  }

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int length = map->NumberOfOwnDescriptors();

  bool stable = true;

  for (int i = 0; i < length; i++) {
    Handle<Name> next_key(descriptors->GetKey(i), isolate);
    Handle<Object> prop_value;
    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(i);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetValue(i), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex index = FieldIndex::ForDescriptor(*map, i);
          prop_value = JSObject::FastPropertyAt(from, representation, index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value, JSReceiver::GetProperty(from, next_key),
            Nothing<bool>());
        stable = from->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(from, next_key, from,
                        LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (use_set) {
      LookupIterator it(target, next_key, target);
      Maybe<bool> result = Object::SetProperty(
          &it, prop_value, STRICT, Object::CERTAINLY_NOT_STORE_FROM_KEYED);
      if (result.IsNothing()) return result;
      if (stable) stable = from->map() == *map;
    } else {
      if (excluded_properties != nullptr &&
          HasExcludedProperty(excluded_properties, next_key)) {
        continue;
      }

      // 4a ii 2. Perform ? CreateDataProperty(target, nextKey, propValue).
      bool success;
      LookupIterator it = LookupIterator::PropertyOrElement(
          isolate, target, next_key, &success, LookupIterator::OWN);
      CHECK(success);
      CHECK(
          JSObject::CreateDataProperty(&it, prop_value, Object::THROW_ON_ERROR)
              .FromJust());
    }
  }

  return Just(true);
}
}  // namespace

// static
Maybe<bool> JSReceiver::SetOrCopyDataProperties(
    Isolate* isolate, Handle<JSReceiver> target, Handle<Object> source,
    const ScopedVector<Handle<Object>>* excluded_properties, bool use_set) {
  Maybe<bool> fast_assign =
      FastAssign(target, source, excluded_properties, use_set);
  if (fast_assign.IsNothing()) return Nothing<bool>();
  if (fast_assign.FromJust()) return Just(true);

  Handle<JSReceiver> from = Object::ToObject(isolate, source).ToHandleChecked();
  // 3b. Let keys be ? from.[[OwnPropertyKeys]]().
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(from, KeyCollectionMode::kOwnOnly, ALL_PROPERTIES,
                              GetKeysConversion::kKeepNumbers),
      Nothing<bool>());

  // 4. Repeat for each element nextKey of keys in List order,
  for (int j = 0; j < keys->length(); ++j) {
    Handle<Object> next_key(keys->get(j), isolate);
    // 4a i. Let desc be ? from.[[GetOwnProperty]](nextKey).
    PropertyDescriptor desc;
    Maybe<bool> found =
        JSReceiver::GetOwnPropertyDescriptor(isolate, from, next_key, &desc);
    if (found.IsNothing()) return Nothing<bool>();
    // 4a ii. If desc is not undefined and desc.[[Enumerable]] is true, then
    if (found.FromJust() && desc.enumerable()) {
      // 4a ii 1. Let propValue be ? Get(from, nextKey).
      Handle<Object> prop_value;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value,
          Runtime::GetObjectProperty(isolate, from, next_key), Nothing<bool>());

      if (use_set) {
        // 4c ii 2. Let status be ? Set(to, nextKey, propValue, true).
        Handle<Object> status;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, status, Runtime::SetObjectProperty(
                                 isolate, target, next_key, prop_value, STRICT),
            Nothing<bool>());
      } else {
        if (excluded_properties != nullptr &&
            HasExcludedProperty(excluded_properties, next_key)) {
          continue;
        }

        // 4a ii 2. Perform ! CreateDataProperty(target, nextKey, propValue).
        bool success;
        LookupIterator it = LookupIterator::PropertyOrElement(
            isolate, target, next_key, &success, LookupIterator::OWN);
        CHECK(success);
        CHECK(JSObject::CreateDataProperty(&it, prop_value,
                                           Object::THROW_ON_ERROR)
                  .FromJust());
      }
    }
  }

  return Just(true);
}

Map* Object::GetPrototypeChainRootMap(Isolate* isolate) {
  DisallowHeapAllocation no_alloc;
  if (IsSmi()) {
    Context* native_context = isolate->context()->native_context();
    return native_context->number_function()->initial_map();
  }

  // The object is either a number, a string, a symbol, a boolean, a real JS
  // object, or a Harmony proxy.
  HeapObject* heap_object = HeapObject::cast(this);
  return heap_object->map()->GetPrototypeChainRootMap(isolate);
}

Map* Map::GetPrototypeChainRootMap(Isolate* isolate) {
  DisallowHeapAllocation no_alloc;
  if (IsJSReceiverMap()) {
    return this;
  }
  int constructor_function_index = GetConstructorFunctionIndex();
  if (constructor_function_index != Map::kNoConstructorFunctionIndex) {
    Context* native_context = isolate->context()->native_context();
    JSFunction* constructor_function =
        JSFunction::cast(native_context->get(constructor_function_index));
    return constructor_function->initial_map();
  }
  return isolate->heap()->null_value()->map();
}

namespace {

// Returns a non-SMI for JSObjects, but returns the hash code for simple
// objects.  This avoids a double lookup in the cases where we know we will
// add the hash to the JSObject if it does not already exist.
Object* GetSimpleHash(Object* object) {
  // The object is either a Smi, a HeapNumber, a name, an odd-ball, a real JS
  // object, or a Harmony proxy.
  if (object->IsSmi()) {
    uint32_t hash =
        ComputeIntegerHash(Smi::cast(object)->value(), kZeroHashSeed);
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (object->IsHeapNumber()) {
    double num = HeapNumber::cast(object)->value();
    if (std::isnan(num)) return Smi::FromInt(Smi::kMaxValue);
    if (i::IsMinusZero(num)) num = 0;
    if (IsSmiDouble(num)) {
      return Smi::FromInt(FastD2I(num))->GetHash();
    }
    uint32_t hash = ComputeLongHash(double_to_uint64(num));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (object->IsName()) {
    uint32_t hash = Name::cast(object)->Hash();
    return Smi::FromInt(hash);
  }
  if (object->IsOddball()) {
    uint32_t hash = Oddball::cast(object)->to_string()->Hash();
    return Smi::FromInt(hash);
  }
  DCHECK(object->IsJSReceiver());
  // Simply return the receiver as it is guaranteed to not be a SMI.
  return object;
}

}  // namespace

Object* Object::GetHash() {
  Object* hash = GetSimpleHash(this);
  if (hash->IsSmi()) return hash;

  DisallowHeapAllocation no_gc;
  DCHECK(IsJSReceiver());
  JSReceiver* receiver = JSReceiver::cast(this);
  Isolate* isolate = receiver->GetIsolate();
  return JSReceiver::GetIdentityHash(isolate, handle(receiver, isolate));
}

Smi* Object::GetOrCreateHash(Isolate* isolate, Handle<Object> object) {
  Object* hash = GetSimpleHash(*object);
  if (hash->IsSmi()) return Smi::cast(hash);

  DCHECK(object->IsJSReceiver());
  return JSReceiver::GetOrCreateIdentityHash(isolate,
                                             Handle<JSReceiver>::cast(object));
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
  return false;
}


MaybeHandle<Object> Object::ArraySpeciesConstructor(
    Isolate* isolate, Handle<Object> original_array) {
  Handle<Object> default_species = isolate->array_function();
  if (original_array->IsJSArray() &&
      Handle<JSArray>::cast(original_array)->HasArrayPrototype(isolate) &&
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
      if (*constructor_context != *isolate->native_context() &&
          *constructor == constructor_context->array_function()) {
        constructor = isolate->factory()->undefined_value();
      }
    }
    if (constructor->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, constructor,
          JSReceiver::GetProperty(Handle<JSReceiver>::cast(constructor),
                                  isolate->factory()->species_symbol()),
          Object);
      if (constructor->IsNull(isolate)) {
        constructor = isolate->factory()->undefined_value();
      }
    }
  }
  if (constructor->IsUndefined(isolate)) {
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

// ES6 section 7.3.20 SpeciesConstructor ( O, defaultConstructor )
MUST_USE_RESULT MaybeHandle<Object> Object::SpeciesConstructor(
    Isolate* isolate, Handle<JSReceiver> recv,
    Handle<JSFunction> default_ctor) {
  Handle<Object> ctor_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor_obj,
      JSObject::GetProperty(recv, isolate->factory()->constructor_string()),
      Object);

  if (ctor_obj->IsUndefined(isolate)) return default_ctor;

  if (!ctor_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kConstructorNotReceiver),
                    Object);
  }

  Handle<JSReceiver> ctor = Handle<JSReceiver>::cast(ctor_obj);

  Handle<Object> species;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, species,
      JSObject::GetProperty(ctor, isolate->factory()->species_symbol()),
      Object);

  if (species->IsNullOrUndefined(isolate)) {
    return default_ctor;
  }

  if (species->IsConstructor()) return species;

  THROW_NEW_ERROR(
      isolate, NewTypeError(MessageTemplate::kSpeciesNotConstructor), Object);
}

bool Object::IterationHasObservableEffects() {
  // Check that this object is an array.
  if (!IsJSArray()) return true;
  JSArray* array = JSArray::cast(this);
  Isolate* isolate = array->GetIsolate();

  // Check that we have the original ArrayPrototype.
  if (!array->map()->prototype()->IsJSObject()) return true;
  JSObject* array_proto = JSObject::cast(array->map()->prototype());
  if (!isolate->is_initial_array_prototype(array_proto)) return true;

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  if (!isolate->IsArrayIteratorLookupChainIntact()) return true;

  // Check that the map of the initial array iterator hasn't changed.
  Map* iterator_map = isolate->initial_array_iterator_prototype()->map();
  if (!isolate->is_initial_array_iterator_prototype_map(iterator_map)) {
    return true;
  }

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  ElementsKind array_kind = array->GetElementsKind();
  if (IsFastPackedElementsKind(array_kind)) return false;

  // For FastHoley kinds, an element access on a hole would cause a lookup on
  // the prototype. This could have different results if the prototype has been
  // changed.
  if (IsFastHoleyElementsKind(array_kind) &&
      isolate->IsFastArrayConstructorPrototypeChainIntact()) {
    return false;
  }
  return true;
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

Handle<String> String::SlowFlatten(Handle<ConsString> cons,
                                   PretenureFlag pretenure) {
  DCHECK(cons->second()->length() != 0);

  // TurboFan can create cons strings with empty first parts.
  while (cons->first()->length() == 0) {
    // We do not want to call this function recursively. Therefore we call
    // String::Flatten only in those cases where String::SlowFlatten is not
    // called again.
    if (cons->second()->IsConsString() && !cons->second()->IsFlat()) {
      cons = handle(ConsString::cast(cons->second()));
    } else {
      return String::Flatten(handle(cons->second()));
    }
  }

  DCHECK(AllowHeapAllocation::IsAllowed());
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
  DisallowHeapAllocation no_allocation;
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
  bool has_pointers = StringShape(this).IsIndirect();
  if (has_pointers) {
    heap->NotifyObjectLayoutChange(this, no_allocation);
  }
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
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size,
                             ClearRecordedSlots::kNo);
  if (has_pointers) {
    heap->ClearRecordedSlotRange(this->address(), this->address() + new_size);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this, new_size - size);
  return true;
}


bool String::MakeExternal(v8::String::ExternalOneByteStringResource* resource) {
  DisallowHeapAllocation no_allocation;
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
  bool has_pointers = StringShape(this).IsIndirect();

  if (has_pointers) {
    heap->NotifyObjectLayoutChange(this, no_allocation);
  }

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
  heap->CreateFillerObjectAt(this->address() + new_size, size - new_size,
                             ClearRecordedSlots::kNo);
  if (has_pointers) {
    heap->ClearRecordedSlotRange(this->address(), this->address() + new_size);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  this->synchronized_set_map(new_map);

  ExternalOneByteString* self = ExternalOneByteString::cast(this);
  self->set_resource(resource);
  if (is_internalized) self->Hash();  // Force regeneration of the hash value.

  heap->AdjustLiveBytes(this, new_size - size);
  return true;
}

void String::StringShortPrint(StringStream* accumulator, bool show_details) {
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
    if (show_details) accumulator->Add("<String[%u]: ", length());
    for (int i = 0; i < len; i++) {
      accumulator->Put(static_cast<char>(stream.GetNext()));
    }
    if (show_details) accumulator->Put('>');
  } else {
    // Backslash indicates that the string contains control
    // characters and that backslashes are therefore escaped.
    if (show_details) accumulator->Add("<String[%u]\\: ", length());
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
    if (show_details) accumulator->Put('>');
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
      double length = JSArray::cast(this)->length()->IsUndefined(GetIsolate())
                          ? 0
                          : JSArray::cast(this)->length()->Number();
      accumulator->Add("<JSArray[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_BOUND_FUNCTION_TYPE: {
      JSBoundFunction* bound_function = JSBoundFunction::cast(this);
      accumulator->Add("<JSBoundFunction");
      accumulator->Add(
          " (BoundTargetFunction %p)>",
          reinterpret_cast<void*>(bound_function->bound_target_function()));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JSWeakMap>");
      break;
    }
    case JS_WEAK_SET_TYPE: {
      accumulator->Add("<JSWeakSet>");
      break;
    }
    case JS_REGEXP_TYPE: {
      accumulator->Add("<JSRegExp");
      JSRegExp* regexp = JSRegExp::cast(this);
      if (regexp->source()->IsString()) {
        accumulator->Add(" ");
        String::cast(regexp->source())->StringShortPrint(accumulator);
      }
      accumulator->Add(">");

      break;
    }
    case JS_FUNCTION_TYPE: {
      JSFunction* function = JSFunction::cast(this);
      Object* fun_name = function->shared()->DebugName();
      bool printed = false;
      if (fun_name->IsString()) {
        String* str = String::cast(fun_name);
        if (str->length() > 0) {
          accumulator->Add("<JSFunction ");
          accumulator->Put(str);
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JSFunction");
      }
      if (FLAG_trace_file_names) {
        Object* source_name =
            Script::cast(function->shared()->script())->name();
        if (source_name->IsString()) {
          String* str = String::cast(source_name);
          if (str->length() > 0) {
            accumulator->Add(" <");
            accumulator->Put(str);
            accumulator->Add(">");
          }
        }
      }
      accumulator->Add(" (sfi = %p)",
                       reinterpret_cast<void*>(function->shared()));
      accumulator->Put('>');
      break;
    }
    case JS_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JSGenerator>");
      break;
    }
    case JS_ASYNC_GENERATOR_OBJECT_TYPE: {
      accumulator->Add("<JS AsyncGenerator>");
      break;
    }

    // All other JSObjects are rather similar to each other (JSObject,
    // JSGlobalProxy, JSGlobalObject, JSUndetectable, JSValue).
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
                accumulator->Add(global_object ? "<GlobalObject " : "<");
                accumulator->Put(str);
                accumulator->Add(
                    " %smap = %p",
                    map_of_this->is_deprecated() ? "deprecated-" : "",
                    map_of_this);
                printed = true;
              }
            }
          }
        } else if (constructor->IsFunctionTemplateInfo()) {
          accumulator->Add(global_object ? "<RemoteObject>" : "<RemoteObject>");
          printed = true;
        }
        if (!printed) {
          accumulator->Add("<JS%sObject", global_object ? "Global " : "");
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
    int descriptors, bool descriptor_to_field,
    Representation old_representation, Representation new_representation,
    MaybeHandle<FieldType> old_field_type, MaybeHandle<Object> old_value,
    MaybeHandle<FieldType> new_field_type, MaybeHandle<Object> new_value) {
  OFStream os(file);
  os << "[generalizing]";
  Name* name = instance_descriptors()->GetKey(modify_index);
  if (name->IsString()) {
    String::cast(name)->PrintOn(file);
  } else {
    os << "{symbol " << static_cast<void*>(name) << "}";
  }
  os << ":";
  if (descriptor_to_field) {
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
  if (new_map->is_dictionary_map()) {
    PrintF(file, "[migrating to slow]\n");
    return;
  }
  PrintF(file, "[migrating]");
  DescriptorArray* o = original_map->instance_descriptors();
  DescriptorArray* n = new_map->instance_descriptors();
  for (int i = 0; i < original_map->NumberOfOwnDescriptors(); i++) {
    Representation o_r = o->GetDetails(i).representation();
    Representation n_r = n->GetDetails(i).representation();
    if (!o_r.Equals(n_r)) {
      String::cast(o->GetKey(i))->PrintOn(file);
      PrintF(file, ":%s->%s ", o_r.Mnemonic(), n_r.Mnemonic());
    } else if (o->GetDetails(i).location() == kDescriptor &&
               n->GetDetails(i).location() == kField) {
      Name* name = o->GetKey(i);
      if (name->IsString()) {
        String::cast(name)->PrintOn(file);
      } else {
        PrintF(file, "{symbol %p}", static_cast<void*>(name));
      }
      PrintF(file, " ");
    }
  }
  if (original_map->elements_kind() != new_map->elements_kind()) {
    PrintF(file, "elements_kind[%i->%i]", original_map->elements_kind(),
           new_map->elements_kind());
  }
  PrintF(file, "\n");
}


void HeapObject::HeapObjectShortPrint(std::ostream& os) {  // NOLINT
  Heap* heap = GetHeap();
  Isolate* isolate = heap->isolate();
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
      std::unique_ptr<char[]> debug_name = shared->DebugName()->ToCString();
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
      os << "<Code " << Code::Kind2String(code->kind()) << ">";
      break;
    }
    case ODDBALL_TYPE: {
      if (IsUndefined(isolate)) {
        os << "<undefined>";
      } else if (IsTheHole(isolate)) {
        os << "<the_hole>";
      } else if (IsNull(isolate)) {
        os << "<null>";
      } else if (IsTrue(isolate)) {
        os << "<true>";
      } else if (IsFalse(isolate)) {
        os << "<false>";
      } else {
        os << "<Odd Oddball: ";
        os << Oddball::cast(this)->to_string()->ToCString().get();
        os << ">";
      }
      break;
    }
    case SYMBOL_TYPE: {
      Symbol* symbol = Symbol::cast(this);
      symbol->SymbolShortPrint(os);
      break;
    }
    case HEAP_NUMBER_TYPE: {
      os << "<Number ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << ">";
      break;
    }
    case MUTABLE_HEAP_NUMBER_TYPE: {
      os << "<MutableNumber ";
      HeapNumber::cast(this)->HeapNumberPrint(os);
      os << '>';
      break;
    }
    case JS_PROXY_TYPE:
      os << "<JSProxy>";
      break;
    case FOREIGN_TYPE:
      os << "<Foreign>";
      break;
    case CELL_TYPE: {
      os << "<Cell value= ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Cell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case PROPERTY_CELL_TYPE: {
      os << "<PropertyCell value=";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      PropertyCell* cell = PropertyCell::cast(this);
      cell->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case WEAK_CELL_TYPE: {
      os << "<WeakCell value= ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      WeakCell::cast(this)->value()->ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
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

String* JSReceiver::class_name() {
  if (IsFunction()) {
    return GetHeap()->Function_string();
  }
  Object* maybe_constructor = map()->GetConstructor();
  if (maybe_constructor->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(maybe_constructor);
    return String::cast(constructor->shared()->instance_class_name());
  } else if (maybe_constructor->IsFunctionTemplateInfo()) {
    FunctionTemplateInfo* info = FunctionTemplateInfo::cast(maybe_constructor);
    return info->class_name()->IsString() ? String::cast(info->class_name())
                                          : GetHeap()->empty_string();
  }

  // If the constructor is not present, return "Object".
  return GetHeap()->Object_string();
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
    } else if (maybe_constructor->IsFunctionTemplateInfo()) {
      FunctionTemplateInfo* info =
          FunctionTemplateInfo::cast(maybe_constructor);
      if (info->class_name()->IsString()) {
        return handle(String::cast(info->class_name()), isolate);
      }
    }
  }

  Handle<Object> maybe_tag = JSReceiver::GetDataProperty(
      receiver, isolate->factory()->to_string_tag_symbol());
  if (maybe_tag->IsString()) return Handle<String>::cast(maybe_tag);

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

Handle<Context> JSReceiver::GetCreationContext() {
  JSReceiver* receiver = this;
  while (receiver->IsJSBoundFunction()) {
    receiver = JSBoundFunction::cast(receiver)->bound_target_function();
  }
  Object* constructor = receiver->map()->GetConstructor();
  JSFunction* function;
  if (constructor->IsJSFunction()) {
    function = JSFunction::cast(constructor);
  } else if (constructor->IsFunctionTemplateInfo()) {
    // Remote objects don't have a creation context.
    return Handle<Context>::null();
  } else {
    // Functions have null as a constructor,
    // but any JSFunction knows its context immediately.
    CHECK(receiver->IsJSFunction());
    function = JSFunction::cast(receiver);
  }

  return function->has_context()
             ? Handle<Context>(function->context()->native_context())
             : Handle<Context>::null();
}

Handle<Object> Map::WrapFieldType(Handle<FieldType> type) {
  if (type->IsClass()) return Map::WeakCellForMap(type->AsClass());
  return type;
}

FieldType* Map::UnwrapFieldType(Object* wrapped_type) {
  Object* value = wrapped_type;
  if (value->IsWeakCell()) {
    if (WeakCell::cast(value)->cleared()) return FieldType::None();
    value = WeakCell::cast(value)->value();
  }
  return FieldType::cast(value);
}

MaybeHandle<Map> Map::CopyWithField(Handle<Map> map, Handle<Name> name,
                                    Handle<FieldType> type,
                                    PropertyAttributes attributes,
                                    PropertyConstness constness,
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
  } else if (IsTransitionableFastElementsKind(map->elements_kind()) &&
             IsInplaceGeneralizableField(constness, representation, *type)) {
    // We don't support propagation of field generalization through elements
    // kind transitions because they are inserted into the transition tree
    // before field transitions. In order to avoid complexity of handling
    // such a case we ensure that all maps with transitionable elements kinds
    // do not have fields that can be generalized in-place (without creation
    // of a new map).
    DCHECK(representation.IsHeapObject());
    type = FieldType::Any(isolate);
  }

  Handle<Object> wrapped_type(WrapFieldType(type));

  DCHECK_IMPLIES(!FLAG_track_constant_fields, constness == kMutable);
  Descriptor d = Descriptor::DataField(name, index, attributes, constness,
                                       representation, wrapped_type);
  Handle<Map> new_map = Map::CopyAddDescriptor(map, &d, flag);
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

  if (FLAG_track_constant_fields) {
    Isolate* isolate = map->GetIsolate();
    Representation representation = constant->OptimalRepresentation();
    Handle<FieldType> type = constant->OptimalType(isolate, representation);
    return CopyWithField(map, name, type, attributes, kConst, representation,
                         flag);
  } else {
    // Allocate new instance descriptors with (name, constant) added.
    Descriptor d = Descriptor::DataConstant(name, 0, constant, attributes);
    Handle<Map> new_map = Map::CopyAddDescriptor(map, &d, flag);
    return new_map;
  }
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

bool Map::TransitionRemovesTaggedField(Map* target) {
  int inobject = NumberOfFields();
  int target_inobject = target->NumberOfFields();
  for (int i = target_inobject; i < inobject; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(this, i);
    if (!IsUnboxedDoubleField(index)) return true;
  }
  return false;
}

bool Map::TransitionChangesTaggedFieldToUntaggedField(Map* target) {
  int inobject = NumberOfFields();
  int target_inobject = target->NumberOfFields();
  int limit = Min(inobject, target_inobject);
  for (int i = 0; i < limit; i++) {
    FieldIndex index = FieldIndex::ForPropertyIndex(target, i);
    if (!IsUnboxedDoubleField(index) && target->IsUnboxedDoubleField(index)) {
      return true;
    }
  }
  return false;
}

bool Map::TransitionRequiresSynchronizationWithGC(Map* target) {
  return TransitionRemovesTaggedField(target) ||
         TransitionChangesTaggedFieldToUntaggedField(target);
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
  DCHECK(old_map->is_prototype_map());
  DCHECK(new_map->is_prototype_map());
  bool was_registered = JSObject::UnregisterPrototypeUser(old_map, isolate);
  new_map->set_prototype_info(old_map->prototype_info());
  old_map->set_prototype_info(Smi::kZero);
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
    int target_index = details.field_index() - new_map->GetInObjectProperties();
    bool have_space = old_map->unused_property_fields() > 0 ||
                      (details.location() == kField && target_index >= 0 &&
                       object->properties()->length() > target_index);
    // Either new_map adds an kDescriptor property, or a kField property for
    // which there is still space, and which does not require a mutable double
    // box (an out-of-object double).
    if (details.location() == kDescriptor ||
        (have_space && ((FLAG_unbox_double_fields && target_index < 0) ||
                        !details.representation().IsDouble()))) {
      object->synchronized_set_map(*new_map);
      return;
    }

    // If there is still space in the object, we need to allocate a mutable
    // double box.
    if (have_space) {
      FieldIndex index =
          FieldIndex::ForDescriptor(*new_map, new_map->LastAdded());
      DCHECK(details.representation().IsDouble());
      DCHECK(!new_map->IsUnboxedDoubleField(index));
      Handle<Object> value = isolate->factory()->NewMutableHeapNumber();
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
      value = isolate->factory()->NewMutableHeapNumber();
    } else {
      value = isolate->factory()->uninitialized_value();
    }
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(kData, details.kind());
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
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    PropertyDetails old_details = old_descriptors->GetDetails(i);
    Representation old_representation = old_details.representation();
    Representation representation = details.representation();
    Handle<Object> value;
    if (old_details.location() == kDescriptor) {
      if (old_details.kind() == kAccessor) {
        // In case of kAccessor -> kData property reconfiguration, the property
        // must already be prepared for data of certain type.
        DCHECK(!details.representation().IsNone());
        if (details.representation().IsDouble()) {
          value = isolate->factory()->NewMutableHeapNumber();
        } else {
          value = isolate->factory()->uninitialized_value();
        }
      } else {
        DCHECK_EQ(kData, old_details.kind());
        value = handle(old_descriptors->GetValue(i), isolate);
        DCHECK(!old_representation.IsDouble() && !representation.IsDouble());
      }
    } else {
      DCHECK_EQ(kField, old_details.location());
      FieldIndex index = FieldIndex::ForDescriptor(*old_map, i);
      if (object->IsUnboxedDoubleField(index)) {
        uint64_t old_bits = object->RawFastDoublePropertyAsBitsAt(index);
        value = isolate->factory()->NewHeapNumberFromBits(
            old_bits, representation.IsDouble() ? MUTABLE : IMMUTABLE);

      } else {
        value = handle(object->RawFastPropertyAt(index), isolate);
        if (!old_representation.IsDouble() && representation.IsDouble()) {
          DCHECK_IMPLIES(old_representation.IsNone(),
                         value->IsUninitialized(isolate));
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
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    Handle<Object> value;
    if (details.representation().IsDouble()) {
      value = isolate->factory()->NewMutableHeapNumber();
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

  heap->NotifyObjectLayoutChange(*object, no_allocation);

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
      // Ensure that all bits of the double value are preserved.
      object->RawFastDoublePropertyAsBitsAtPut(
          index, HeapNumber::cast(value)->value_as_bits());
      if (i < old_number_of_fields && !old_map->IsUnboxedDoubleField(index)) {
        // Transition from tagged to untagged slot.
        heap->ClearRecordedSlot(*object,
                                HeapObject::RawField(*object, index.offset()));
      } else {
        DCHECK(!heap->HasRecordedSlot(
            *object, HeapObject::RawField(*object, index.offset())));
      }
    } else {
      object->RawFastPropertyAtPut(index, value);
    }
  }


  // If there are properties in the new backing store, trim it to the correct
  // size and install the backing store into the object.
  if (external > 0) {
    heap->RightTrimFixedArray(*array, inobject);
    object->set_properties(*array);
  }

  // Create filler object past the new instance size.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = old_map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    Address address = object->address();
    heap->CreateFillerObjectAt(address + new_instance_size, instance_size_delta,
                               ClearRecordedSlots::kYes);
    heap->AdjustLiveBytes(*object, -instance_size_delta);
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
    // Make space for two more properties.
    property_count += NameDictionary::kInitialCapacity;
  }
  Handle<NameDictionary> dictionary =
      NameDictionary::New(isolate, property_count);

  Handle<DescriptorArray> descs(map->instance_descriptors());
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    Handle<Name> key(descs->GetKey(i));
    Handle<Object> value;
    if (details.location() == kField) {
      FieldIndex index = FieldIndex::ForDescriptor(*map, i);
      if (details.kind() == kData) {
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
      } else {
        DCHECK_EQ(kAccessor, details.kind());
        value = handle(object->RawFastPropertyAt(index), isolate);
      }

    } else {
      DCHECK_EQ(kDescriptor, details.location());
      value = handle(descs->GetValue(i), isolate);
    }
    DCHECK(!value.is_null());
    PropertyDetails d(details.kind(), details.attributes(), i + 1,
                      PropertyCellType::kNoCell);
    dictionary = NameDictionary::Add(dictionary, key, value, d);
  }

  // Copy the next enumeration index from instance descriptor.
  dictionary->SetNextEnumerationIndex(real_size + 1);

  // From here on we cannot fail and we shouldn't GC anymore.
  DisallowHeapAllocation no_allocation;

  Heap* heap = isolate->heap();
  heap->NotifyObjectLayoutChange(*object, no_allocation);

  // Resize the object in the heap if necessary.
  int new_instance_size = new_map->instance_size();
  int instance_size_delta = map->instance_size() - new_instance_size;
  DCHECK(instance_size_delta >= 0);

  if (instance_size_delta > 0) {
    heap->CreateFillerObjectAt(object->address() + new_instance_size,
                               instance_size_delta, ClearRecordedSlots::kYes);
    heap->AdjustLiveBytes(*object, -instance_size_delta);
  }

  // We are storing the new map using release store after creating a filler for
  // the left-over space to avoid races with the sweeper thread.
  object->synchronized_set_map(*new_map);

  object->set_properties(*dictionary);

  // Ensure that in-object space of slow-mode object does not contain random
  // garbage.
  int inobject_properties = new_map->GetInObjectProperties();
  if (inobject_properties) {
    Heap* heap = isolate->heap();
    heap->ClearRecordedSlotRange(
        object->address() + map->GetInObjectPropertyOffset(0),
        object->address() + new_instance_size);

    for (int i = 0; i < inobject_properties; i++) {
      FieldIndex index = FieldIndex::ForPropertyIndex(*new_map, i);
      object->RawFastPropertyAtPut(index, Smi::kZero);
    }
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

// static
void JSObject::NotifyMapChange(Handle<Map> old_map, Handle<Map> new_map,
                               Isolate* isolate) {
  if (!old_map->is_prototype_map()) return;

  InvalidatePrototypeChains(*old_map);

  // If the map was registered with its prototype before, ensure that it
  // registers with its new prototype now. This preserves the invariant that
  // when a map on a prototype chain is registered with its prototype, then
  // all prototypes further up the chain are also registered with their
  // respective prototypes.
  UpdatePrototypeUserRegistration(old_map, new_map, isolate);
}

void JSObject::MigrateToMap(Handle<JSObject> object, Handle<Map> new_map,
                            int expected_additional_properties) {
  if (object->map() == *new_map) return;
  Handle<Map> old_map(object->map());
  NotifyMapChange(old_map, new_map, new_map->GetIsolate());

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
      DCHECK(new_map->GetBackPointer()->IsUndefined(new_map->GetIsolate()));
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

void JSObject::ForceSetPrototype(Handle<JSObject> object,
                                 Handle<Object> proto) {
  // object.__proto__ = proto;
  Handle<Map> old_map = Handle<Map>(object->map());
  Handle<Map> new_map = Map::Copy(old_map, "ForceSetPrototype");
  Map::SetPrototype(new_map, proto, FAST_PROTOTYPE);
  JSObject::MigrateToMap(object, new_map);
}

int Map::NumberOfFields() {
  DescriptorArray* descriptors = instance_descriptors();
  int result = 0;
  for (int i = 0; i < NumberOfOwnDescriptors(); i++) {
    if (descriptors->GetDetails(i).location() == kField) result++;
  }
  return result;
}

void DescriptorArray::GeneralizeAllFields() {
  int length = number_of_descriptors();
  for (int i = 0; i < length; i++) {
    PropertyDetails details = GetDetails(i);
    details = details.CopyWithRepresentation(Representation::Tagged());
    if (details.location() == kField) {
      DCHECK_EQ(kData, details.kind());
      details = details.CopyWithConstness(kMutable);
      SetValue(i, FieldType::Any());
    }
    set(ToDetailsIndex(i), details.AsSmi());
  }
}

Handle<Map> Map::CopyGeneralizeAllFields(Handle<Map> map,
                                         ElementsKind elements_kind,
                                         int modify_index, PropertyKind kind,
                                         PropertyAttributes attributes,
                                         const char* reason) {
  Isolate* isolate = map->GetIsolate();
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  Handle<DescriptorArray> descriptors =
      DescriptorArray::CopyUpTo(old_descriptors, number_of_own_descriptors);
  descriptors->GeneralizeAllFields();

  Handle<LayoutDescriptor> new_layout_descriptor(
      LayoutDescriptor::FastPointerLayout(), isolate);
  Handle<Map> new_map = CopyReplaceDescriptors(
      map, descriptors, new_layout_descriptor, OMIT_TRANSITION,
      MaybeHandle<Name>(), reason, SPECIAL_TRANSITION);

  // Unless the instance is being migrated, ensure that modify_index is a field.
  if (modify_index >= 0) {
    PropertyDetails details = descriptors->GetDetails(modify_index);
    if (details.constness() != kMutable || details.location() != kField ||
        details.attributes() != attributes) {
      int field_index = details.location() == kField
                            ? details.field_index()
                            : new_map->NumberOfFields();
      Descriptor d = Descriptor::DataField(
          handle(descriptors->GetKey(modify_index), isolate), field_index,
          attributes, Representation::Tagged());
      descriptors->Replace(modify_index, &d);
      if (details.location() != kField) {
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
      if (details.location() == kField) {
        field_type = handle(
            map->instance_descriptors()->GetFieldType(modify_index), isolate);
      }
      map->PrintGeneralization(
          stdout, reason, modify_index, new_map->NumberOfOwnDescriptors(),
          new_map->NumberOfOwnDescriptors(), details.location() == kDescriptor,
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
  DCHECK(!constructor_or_backpointer()->IsFunctionTemplateInfo());
  deprecate();
  dependent_code()->DeoptimizeDependentCodeGroup(
      GetIsolate(), DependentCode::kTransitionGroup);
  NotifyLeafMapLayoutChange();
}


// Installs |new_descriptors| over the current instance_descriptors to ensure
// proper sharing of descriptor arrays.
void Map::ReplaceDescriptors(DescriptorArray* new_descriptors,
                             LayoutDescriptor* new_layout_descriptor) {
  Isolate* isolate = GetIsolate();
  // Don't overwrite the empty descriptor array or initial map's descriptors.
  if (NumberOfOwnDescriptors() == 0 || GetBackPointer()->IsUndefined(isolate)) {
    return;
  }

  DescriptorArray* to_replace = instance_descriptors();
  isolate->heap()->incremental_marking()->IterateBlackObject(to_replace);
  Map* current = this;
  while (current->instance_descriptors() == to_replace) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined(isolate)) break;  // Stop overwriting at initial map.
    current->SetEnumLength(kInvalidEnumCacheSentinel);
    current->UpdateDescriptors(new_descriptors, new_layout_descriptor);
    current = Map::cast(next);
  }
  set_owns_descriptors(false);
}


Map* Map::FindRootMap() {
  Map* result = this;
  Isolate* isolate = GetIsolate();
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined(isolate)) {
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


Map* Map::FindFieldOwner(int descriptor) {
  DisallowHeapAllocation no_allocation;
  DCHECK_EQ(kField, instance_descriptors()->GetDetails(descriptor).location());
  Map* result = this;
  Isolate* isolate = GetIsolate();
  while (true) {
    Object* back = result->GetBackPointer();
    if (back->IsUndefined(isolate)) break;
    Map* parent = Map::cast(back);
    if (parent->NumberOfOwnDescriptors() <= descriptor) break;
    result = parent;
  }
  return result;
}

void Map::UpdateFieldType(int descriptor, Handle<Name> name,
                          PropertyConstness new_constness,
                          Representation new_representation,
                          Handle<Object> new_wrapped_type) {
  DCHECK(new_wrapped_type->IsSmi() || new_wrapped_type->IsWeakCell());
  // We store raw pointers in the queue, so no allocations are allowed.
  DisallowHeapAllocation no_allocation;
  PropertyDetails details = instance_descriptors()->GetDetails(descriptor);
  if (details.location() != kField) return;
  DCHECK_EQ(kData, details.kind());

  Zone zone(GetIsolate()->allocator(), ZONE_NAME);
  ZoneQueue<Map*> backlog(&zone);
  backlog.push(this);

  while (!backlog.empty()) {
    Map* current = backlog.front();
    backlog.pop();

    Object* transitions = current->raw_transitions();
    int num_transitions = TransitionArray::NumberOfTransitions(transitions);
    for (int i = 0; i < num_transitions; ++i) {
      Map* target = TransitionArray::GetTarget(transitions, i);
      backlog.push(target);
    }
    DescriptorArray* descriptors = current->instance_descriptors();
    PropertyDetails details = descriptors->GetDetails(descriptor);

    // Currently constness change implies map change.
    DCHECK_IMPLIES(new_constness != details.constness(),
                   FLAG_modify_map_inplace);

    // It is allowed to change representation here only from None to something.
    DCHECK(details.representation().Equals(new_representation) ||
           details.representation().IsNone());

    // Skip if already updated the shared descriptor.
    if ((FLAG_modify_map_inplace && new_constness != details.constness()) ||
        descriptors->GetValue(descriptor) != *new_wrapped_type) {
      DCHECK_IMPLIES(!FLAG_track_constant_fields, new_constness == kMutable);
      Descriptor d = Descriptor::DataField(
          name, descriptors->GetFieldIndex(descriptor), details.attributes(),
          new_constness, new_representation, new_wrapped_type);
      descriptors->Replace(descriptor, &d);
    }
  }
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
void Map::GeneralizeField(Handle<Map> map, int modify_index,
                          PropertyConstness new_constness,
                          Representation new_representation,
                          Handle<FieldType> new_field_type) {
  Isolate* isolate = map->GetIsolate();

  // Check if we actually need to generalize the field type at all.
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(), isolate);
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  PropertyConstness old_constness = old_details.constness();
  Representation old_representation = old_details.representation();
  Handle<FieldType> old_field_type(old_descriptors->GetFieldType(modify_index),
                                   isolate);

  // Return if the current map is general enough to hold requested contness and
  // representation/field type.
  if (((FLAG_modify_map_inplace &&
        IsGeneralizableTo(new_constness, old_constness)) ||
       (!FLAG_modify_map_inplace && (old_constness == new_constness))) &&
      old_representation.Equals(new_representation) &&
      !FieldTypeIsCleared(new_representation, *new_field_type) &&
      // Checking old_field_type for being cleared is not necessary because
      // the NowIs check below would fail anyway in that case.
      new_field_type->NowIs(old_field_type)) {
    DCHECK(GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate)
               ->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(modify_index), isolate);
  Handle<DescriptorArray> descriptors(field_owner->instance_descriptors(),
                                      isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  new_field_type =
      Map::GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate);
  if (FLAG_modify_map_inplace) {
    new_constness = GeneralizeConstness(old_constness, new_constness);
  }

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index));

  Handle<Object> wrapped_type(WrapFieldType(new_field_type));
  field_owner->UpdateFieldType(modify_index, name, new_constness,
                               new_representation, wrapped_type);
  field_owner->dependent_code()->DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kFieldOwnerGroup);

  if (FLAG_trace_generalization) {
    map->PrintGeneralization(
        stdout, "field type generalization", modify_index,
        map->NumberOfOwnDescriptors(), map->NumberOfOwnDescriptors(), false,
        details.representation(), details.representation(), old_field_type,
        MaybeHandle<Object>(), new_field_type, MaybeHandle<Object>());
  }
}

// TODO(ishell): remove.
// static
Handle<Map> Map::ReconfigureProperty(Handle<Map> map, int modify_index,
                                     PropertyKind new_kind,
                                     PropertyAttributes new_attributes,
                                     Representation new_representation,
                                     Handle<FieldType> new_field_type) {
  DCHECK_EQ(kData, new_kind);  // Only kData case is supported.
  MapUpdater mu(map->GetIsolate(), map);
  return mu.ReconfigureToDataField(modify_index, new_attributes, kConst,
                                   new_representation, new_field_type);
}

// TODO(ishell): remove.
// static
Handle<Map> Map::ReconfigureElementsKind(Handle<Map> map,
                                         ElementsKind new_elements_kind) {
  MapUpdater mu(map->GetIsolate(), map);
  return mu.ReconfigureElementsKind(new_elements_kind);
}

// Generalize all fields and update the transition tree.
Handle<Map> Map::GeneralizeAllFields(Handle<Map> map) {
  Isolate* isolate = map->GetIsolate();
  Handle<FieldType> any_type = FieldType::Any(isolate);

  Handle<DescriptorArray> descriptors(map->instance_descriptors());
  for (int i = 0; i < map->NumberOfOwnDescriptors(); ++i) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() == kField) {
      DCHECK_EQ(kData, details.kind());
      MapUpdater mu(isolate, map);
      map = mu.ReconfigureToDataField(i, details.attributes(), kMutable,
                                      Representation::Tagged(), any_type);
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
  if (root_map->is_deprecated()) {
    JSFunction* constructor = JSFunction::cast(root_map->GetConstructor());
    DCHECK(constructor->has_initial_map());
    DCHECK(constructor->initial_map()->is_dictionary_map());
    if (constructor->initial_map()->elements_kind() !=
        old_map->elements_kind()) {
      return MaybeHandle<Map>();
    }
    return handle(constructor->initial_map());
  }
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
    if (!IsGeneralizableTo(old_details.constness(), new_details.constness())) {
      return nullptr;
    }
    DCHECK(IsGeneralizableTo(old_details.location(), new_details.location()));
    if (!old_details.representation().fits_into(new_details.representation())) {
      return nullptr;
    }
    if (new_details.location() == kField) {
      if (new_details.kind() == kData) {
        FieldType* new_type = new_descriptors->GetFieldType(i);
        // Cleared field types need special treatment. They represent lost
        // knowledge, so we must first generalize the new_type to "Any".
        if (FieldTypeIsCleared(new_details.representation(), new_type)) {
          return nullptr;
        }
        DCHECK_EQ(kData, old_details.kind());
        if (old_details.location() == kField) {
          FieldType* old_type = old_descriptors->GetFieldType(i);
          if (FieldTypeIsCleared(old_details.representation(), old_type) ||
              !old_type->NowIs(new_type)) {
            return nullptr;
          }
        } else {
          DCHECK_EQ(kDescriptor, old_details.location());
          DCHECK(!FLAG_track_constant_fields);
          Object* old_value = old_descriptors->GetValue(i);
          if (!new_type->NowContains(old_value)) {
            return nullptr;
          }
        }

      } else {
        DCHECK_EQ(kAccessor, new_details.kind());
#ifdef DEBUG
        FieldType* new_type = new_descriptors->GetFieldType(i);
        DCHECK(new_type->IsAny());
#endif
        UNREACHABLE();
      }
    } else {
      DCHECK_EQ(kDescriptor, new_details.location());
      Object* old_value = old_descriptors->GetValue(i);
      Object* new_value = new_descriptors->GetValue(i);
      if (old_details.location() == kField || old_value != new_value) {
        return nullptr;
      }
    }
  }
  if (new_map->NumberOfOwnDescriptors() != old_nof) return nullptr;
  return new_map;
}


// static
Handle<Map> Map::Update(Handle<Map> map) {
  if (!map->is_deprecated()) return map;
  MapUpdater mu(map->GetIsolate(), map);
  return mu.Update();
}

Maybe<bool> JSObject::SetPropertyWithInterceptor(LookupIterator* it,
                                                 ShouldThrow should_throw,
                                                 Handle<Object> value) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return SetPropertyWithInterceptorInternal(it, it->GetInterceptor(),
                                            should_throw, value);
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
  DCHECK(it->IsFound());
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(it->isolate());

  do {
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

      case LookupIterator::INTERCEPTOR: {
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(it);
          if (!maybe_attributes.IsJust()) return Nothing<bool>();
          if ((maybe_attributes.FromJust() & READ_ONLY) != 0) {
            return WriteToReadOnlyProperty(it, value, should_throw);
          }
          if (maybe_attributes.FromJust() == ABSENT) break;
          *found = false;
          return Nothing<bool>();
        }
        break;
      }

      case LookupIterator::ACCESSOR: {
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        Handle<Object> accessors = it->GetAccessors();
        if (accessors->IsAccessorInfo() &&
            !it->HolderIsReceiverOrHiddenPrototype() &&
            AccessorInfo::cast(*accessors)->is_special_data_property()) {
          *found = false;
          return Nothing<bool>();
        }
        return SetPropertyWithAccessor(it, value, should_throw);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        // TODO(verwaest): We should throw an exception if holder is receiver.
        return Just(true);

      case LookupIterator::DATA:
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return SetDataProperty(it, value);
        }
      // Fall through.
      case LookupIterator::TRANSITION:
        *found = false;
        return Nothing<bool>();
    }
    it->Next();
  } while (it->IsFound());

  *found = false;
  return Nothing<bool>();
}


Maybe<bool> Object::SetProperty(LookupIterator* it, Handle<Object> value,
                                LanguageMode language_mode,
                                StoreFromKeyed store_mode) {
  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, language_mode, store_mode, &found);
    if (found) return result;
  }

  // If the receiver is the JSGlobalObject, the store was contextual. In case
  // the property did not exist yet on the global object itself, we have to
  // throw a reference error in strict mode.  In sloppy mode, we continue.
  if (is_strict(language_mode) && it->GetReceiver()->IsJSGlobalObject()) {
    it->isolate()->Throw(*it->isolate()->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, it->name()));
    return Nothing<bool>();
  }

  ShouldThrow should_throw =
      is_sloppy(language_mode) ? DONT_THROW : THROW_ON_ERROR;
  return AddDataProperty(it, value, NONE, should_throw, store_mode);
}


Maybe<bool> Object::SetSuperProperty(LookupIterator* it, Handle<Object> value,
                                     LanguageMode language_mode,
                                     StoreFromKeyed store_mode) {
  Isolate* isolate = it->isolate();

  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, language_mode, store_mode, &found);
    if (found) return result;
  }

  it->UpdateProtector();

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

  return AddDataProperty(&own_lookup, value, NONE, should_throw, store_mode);
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

  Handle<Object> to_assign = value;
  // Convert the incoming value to a number for storing into typed arrays.
  if (it->IsElement() && receiver->HasFixedTypedArrayElements()) {
    if (!value->IsNumber() && !value->IsUndefined(it->isolate())) {
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
  it->WriteDataValue(to_assign, false);

#if VERIFY_HEAP
  if (FLAG_verify_heap) {
    receiver->JSObjectVerify();
  }
#endif
  return Just(true);
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
    it->UpdateProtector();
    // Migrate to the most up-to-date map that will be able to store |value|
    // under it->name() with |attributes|.
    it->PrepareTransitionToDataProperty(receiver, value, attributes,
                                        store_mode);
    DCHECK_EQ(LookupIterator::TRANSITION, it->state());
    it->ApplyTransitionToDataProperty(receiver);

    // Write the property value.
    it->WriteDataValue(value, true);

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

  Isolate* isolate = map->GetIsolate();
  // Replace descriptors by new_descriptors in all maps that share it.
  isolate->heap()->incremental_marking()->IterateBlackObject(*descriptors);

  Map* current = *map;
  while (current->instance_descriptors() == *descriptors) {
    Object* next = current->GetBackPointer();
    if (next->IsUndefined(isolate)) break;  // Stop overwriting at initial map.
    current->UpdateDescriptors(*new_descriptors, layout_descriptor);
    current = Map::cast(next);
  }
  map->UpdateDescriptors(*new_descriptors, layout_descriptor);
}

// static
Handle<Map> Map::GetObjectCreateMap(Handle<HeapObject> prototype) {
  Isolate* isolate = prototype->GetIsolate();
  Handle<Map> map(isolate->native_context()->object_function()->initial_map(),
                  isolate);
  if (map->prototype() == *prototype) return map;
  if (prototype->IsNull(isolate)) {
    return isolate->slow_object_with_null_prototype_map();
  }
  if (prototype->IsJSObject()) {
    Handle<JSObject> js_prototype = Handle<JSObject>::cast(prototype);
    if (!js_prototype->map()->is_prototype_map()) {
      JSObject::OptimizeAsPrototype(js_prototype, FAST_PROTOTYPE);
    }
    Handle<PrototypeInfo> info =
        Map::GetOrCreatePrototypeInfo(js_prototype, isolate);
    // TODO(verwaest): Use inobject slack tracking for this map.
    if (info->HasObjectCreateMap()) {
      map = handle(info->ObjectCreateMap(), isolate);
    } else {
      map = Map::CopyInitialMap(map);
      Map::SetPrototype(map, prototype, FAST_PROTOTYPE);
      PrototypeInfo::SetObjectCreateMap(info, map);
    }
    return map;
  }

  return Map::TransitionToPrototype(map, prototype, REGULAR_PROTOTYPE);
}

template <class T>
static int AppendUniqueCallbacks(Handle<TemplateList> callbacks,
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
    Descriptor d =
        Descriptor::AccessorConstant(key, entry, entry->property_attributes());
    array->Append(&d);
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
  Handle<TemplateList> callbacks = Handle<TemplateList>::cast(descriptors);
  DCHECK_GE(array->NumberOfSlackDescriptors(), callbacks->length());
  nof = AppendUniqueCallbacks<DescriptorArrayAppender>(callbacks, array, nof);
  map->SetNumberOfOwnDescriptors(nof);
}


int AccessorInfo::AppendUnique(Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  Handle<TemplateList> callbacks = Handle<TemplateList>::cast(descriptors);
  DCHECK_GE(array->length(), callbacks->length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(callbacks, array,
                                                   valid_descriptors);
}

static bool ContainsMap(MapHandles const& maps, Map* map) {
  DCHECK_NOT_NULL(map);
  for (Handle<Map> current : maps) {
    if (!current.is_null() && *current == map) return true;
  }
  return false;
}

Map* Map::FindElementsKindTransitionedMap(MapHandles const& candidates) {
  DisallowHeapAllocation no_allocation;
  DisallowDeoptimization no_deoptimization(GetIsolate());

  ElementsKind kind = elements_kind();
  bool packed = IsFastPackedElementsKind(kind);

  Map* transition = nullptr;
  if (IsTransitionableFastElementsKind(kind)) {
    // Check the state of the root map.
    Map* root_map = FindRootMap();
    if (!EquivalentToForElementsKindTransition(root_map)) return nullptr;
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
    if (native_context->get(Context::ArrayMapIndex(from_kind)) == *map) {
      Object* maybe_transitioned_map =
          native_context->get(Context::ArrayMapIndex(to_kind));
      if (maybe_transitioned_map->IsMap()) {
        return handle(Map::cast(maybe_transitioned_map), isolate);
      }
    }
  }

  DCHECK(!map->IsUndefined(isolate));
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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
                   target_desc.set()->IsUndefined(isolate);
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  if (!function->bound_target_function()->IsJSFunction()) return prefix;
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  Handle<Object> target_name = JSFunction::GetName(isolate, target);
  if (!target_name->IsString()) return prefix;
  Factory* factory = isolate->factory();
  return factory->NewConsString(prefix, Handle<String>::cast(target_name));
}

// static
Handle<Object> JSFunction::GetName(Isolate* isolate,
                                   Handle<JSFunction> function) {
  if (function->shared()->name_should_print_as_anonymous()) {
    return isolate->factory()->anonymous_string();
  }
  return handle(function->shared()->name(), isolate);
}

// static
MaybeHandle<Smi> JSFunction::GetLength(Isolate* isolate,
                                       Handle<JSFunction> function) {
  int length = 0;
  if (function->shared()->is_compiled()) {
    length = function->shared()->GetLength();
  } else {
    // If the function isn't compiled yet, the length is not computed
    // correctly yet. Compile it now and return the right length.
    if (Compiler::Compile(function, Compiler::KEEP_EXCEPTION)) {
      length = function->shared()->GetLength();
    }
    if (isolate->has_pending_exception()) return MaybeHandle<Smi>();
  }
  DCHECK_GE(length, 0);
  return handle(Smi::FromInt(length), isolate);
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
  return object->GetCreationContext();
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
  PropertyDescriptor desc;
  Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
      it->isolate(), it->GetHolder<JSProxy>(), it->GetName(), &desc);
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
  if (FLAG_trace_migration && *original_map != object->map()) {
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
  LookupIterator it(object, name, object, LookupIterator::OWN_SKIP_INTERCEPTOR);
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

          return JSObject::SetPropertyWithAccessor(it, value, should_throw);
        }

        it->ReconfigureDataProperty(value, attributes);
        return Just(true);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        return RedefineIncompatibleProperty(it->isolate(), it->GetName(), value,
                                            should_throw);

      case LookupIterator::DATA: {
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
        it->ReconfigureDataProperty(value, attributes);

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
  DCHECK(!value->IsTheHole(object->GetIsolate()));
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::SetOwnElementIgnoreAttributes(
    Handle<JSObject> object, uint32_t index, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it(isolate, object, index, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

MaybeHandle<Object> JSObject::DefinePropertyOrElementIgnoreAttributes(
    Handle<JSObject> object, Handle<Name> name, Handle<Object> value,
    PropertyAttributes attributes) {
  Isolate* isolate = object->GetIsolate();
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, object, LookupIterator::OWN);
  return DefineOwnPropertyIgnoreAttributes(&it, value, attributes);
}

Maybe<PropertyAttributes> JSObject::GetPropertyAttributesWithInterceptor(
    LookupIterator* it) {
  return GetPropertyAttributesWithInterceptorInternal(it, it->GetInterceptor());
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

  Handle<FixedArray> iteration_order =
      NameDictionary::IterationIndices(dictionary);

  int instance_descriptor_length = iteration_order->length();
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  for (int i = 0; i < instance_descriptor_length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    DCHECK(dictionary->IsKey(isolate, dictionary->KeyAt(index)));

    PropertyKind kind = dictionary->DetailsAt(index).kind();
    if (kind == kData) {
      if (FLAG_track_constant_fields) {
        number_of_fields += 1;
      } else {
        Object* value = dictionary->ValueAt(index);
        if (!value->IsJSFunction()) {
          number_of_fields += 1;
        }
      }
    }
  }

  Handle<Map> old_map(object->map(), isolate);

  int inobject_props = old_map->GetInObjectProperties();

  // Allocate new map.
  Handle<Map> new_map = Map::CopyDropDescriptors(old_map);
  new_map->set_dictionary_map(false);

  NotifyMapChange(old_map, new_map, isolate);

#if V8_TRACE_MAPS
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
    DCHECK(dictionary->IsKey(isolate, k));
    // Dictionary keys are internalized upon insertion.
    // TODO(jkummerow): Turn this into a DCHECK if it's not hit in the wild.
    CHECK(k->IsUniqueName());
    Handle<Name> key(Name::cast(k), isolate);

    Object* value = dictionary->ValueAt(index);

    PropertyDetails details = dictionary->DetailsAt(index);
    DCHECK_EQ(kField, details.location());
    DCHECK_EQ(kMutable, details.constness());

    Descriptor d;
    if (details.kind() == kData) {
      if (!FLAG_track_constant_fields && value->IsJSFunction()) {
        d = Descriptor::DataConstant(key, handle(value, isolate),
                                     details.attributes());
      } else {
        d = Descriptor::DataField(
            key, current_offset, details.attributes(), kDefaultFieldConstness,
            // TODO(verwaest): value->OptimalRepresentation();
            Representation::Tagged(), FieldType::Any(isolate));
      }
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      d = Descriptor::AccessorConstant(key, handle(value, isolate),
                                       details.attributes());
    }
    details = d.GetDetails();
    if (details.location() == kField) {
      if (current_offset < inobject_props) {
        object->InObjectPropertyAtPut(current_offset, value,
                                      UPDATE_WRITE_BARRIER);
      } else {
        int offset = current_offset - inobject_props;
        fields->set(offset, value);
      }
      current_offset += details.field_width_in_words();
    }
    descriptors->Set(i, &d);
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


void JSObject::RequireSlowElements(SeededNumberDictionary* dictionary) {
  if (dictionary->requires_slow_elements()) return;
  dictionary->set_requires_slow_elements();
  if (map()->is_prototype_map()) {
    // If this object is a prototype (the callee will check), invalidate any
    // prototype chains involving it.
    InvalidatePrototypeChains(map());
  }
}


Handle<SeededNumberDictionary> JSObject::NormalizeElements(
    Handle<JSObject> object) {
  DCHECK(!object->HasFixedTypedArrayElements());
  Isolate* isolate = object->GetIsolate();
  bool is_sloppy_arguments = object->HasSloppyArgumentsElements();
  {
    DisallowHeapAllocation no_gc;
    FixedArrayBase* elements = object->elements();

    if (is_sloppy_arguments) {
      elements = SloppyArgumentsElements::cast(elements)->arguments();
    }

    if (elements->IsDictionary()) {
      return handle(SeededNumberDictionary::cast(elements), isolate);
    }
  }

  DCHECK(object->HasFastSmiOrObjectElements() ||
         object->HasFastDoubleElements() ||
         object->HasFastArgumentsElements() ||
         object->HasFastStringWrapperElements());

  Handle<SeededNumberDictionary> dictionary =
      object->GetElementsAccessor()->Normalize(object);

  // Switch to using the dictionary as the backing storage for elements.
  ElementsKind target_kind = is_sloppy_arguments
                                 ? SLOW_SLOPPY_ARGUMENTS_ELEMENTS
                                 : object->HasFastStringWrapperElements()
                                       ? SLOW_STRING_WRAPPER_ELEMENTS
                                       : DICTIONARY_ELEMENTS;
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(object, target_kind);
  // Set the new map first to satify the elements type assert in set_elements().
  JSObject::MigrateToMap(object, new_map);

  if (is_sloppy_arguments) {
    SloppyArgumentsElements::cast(object->elements())
        ->set_arguments(*dictionary);
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


template <typename ProxyType>
static Smi* GetOrCreateIdentityHashHelper(Isolate* isolate,
                                          Handle<ProxyType> proxy) {
  Object* maybe_hash = proxy->hash();
  if (maybe_hash->IsSmi()) return Smi::cast(maybe_hash);

  Smi* hash = Smi::FromInt(isolate->GenerateIdentityHash(Smi::kMaxValue));
  proxy->set_hash(hash);
  return hash;
}

// static
Object* JSObject::GetIdentityHash(Isolate* isolate, Handle<JSObject> object) {
  if (object->IsJSGlobalProxy()) {
    return JSGlobalProxy::cast(*object)->hash();
  }
  Handle<Name> hash_code_symbol = isolate->factory()->hash_code_symbol();
  return *JSReceiver::GetDataProperty(object, hash_code_symbol);
}

// static
Smi* JSObject::GetOrCreateIdentityHash(Isolate* isolate,
                                       Handle<JSObject> object) {
  if (object->IsJSGlobalProxy()) {
    return GetOrCreateIdentityHashHelper(isolate,
                                         Handle<JSGlobalProxy>::cast(object));
  }

  Handle<Name> hash_code_symbol = isolate->factory()->hash_code_symbol();
  LookupIterator it(object, hash_code_symbol, object, LookupIterator::OWN);
  if (it.IsFound()) {
    DCHECK_EQ(LookupIterator::DATA, it.state());
    Object* maybe_hash = *it.GetDataValue();
    if (maybe_hash->IsSmi()) return Smi::cast(maybe_hash);
  }

  Smi* hash = Smi::FromInt(isolate->GenerateIdentityHash(Smi::kMaxValue));
  CHECK(AddDataProperty(&it, handle(hash, isolate), NONE, THROW_ON_ERROR,
                        CERTAINLY_NOT_STORE_FROM_KEYED)
            .IsJust());
  return hash;
}

// static
Object* JSProxy::GetIdentityHash(Handle<JSProxy> proxy) {
  return proxy->hash();
}

Smi* JSProxy::GetOrCreateIdentityHash(Isolate* isolate, Handle<JSProxy> proxy) {
  return GetOrCreateIdentityHashHelper(isolate, proxy);
}


Maybe<bool> JSObject::DeletePropertyWithInterceptor(LookupIterator* it,
                                                    ShouldThrow should_throw) {
  Isolate* isolate = it->isolate();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc(isolate);

  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  Handle<InterceptorInfo> interceptor(it->GetInterceptor());
  if (interceptor->deleter()->IsUndefined(isolate)) return Nothing<bool>();

  Handle<JSObject> holder = it->GetHolder<JSObject>();
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, receiver,
                                     Object::ConvertReceiver(isolate, receiver),
                                     Nothing<bool>());
  }

  PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                 *holder, should_throw);
  Handle<Object> result;
  if (it->IsElement()) {
    uint32_t index = it->index();
    v8::IndexedPropertyDeleterCallback deleter =
        v8::ToCData<v8::IndexedPropertyDeleterCallback>(interceptor->deleter());
    result = args.Call(deleter, index);
  } else {
    DCHECK_IMPLIES(it->name()->IsSymbol(),
                   interceptor->can_intercept_symbols());
    Handle<Name> name = it->name();
    DCHECK(!name->IsPrivate());
    v8::GenericNamedPropertyDeleterCallback deleter =
        v8::ToCData<v8::GenericNamedPropertyDeleterCallback>(
            interceptor->deleter());
    result = args.Call(deleter, name);
  }

  RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
  if (result.is_null()) return Nothing<bool>();

  DCHECK(result->IsBoolean());
  // Rebox CustomArguments::kReturnValueOffset before returning.
  return Just(result->IsTrue(isolate));
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
    cell->set_property_details(
        PropertyDetails::Empty(PropertyCellType::kUninitialized));
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
      case LookupIterator::ACCESSOR: {
        if (!it->IsConfigurable()) {
          // Fail if the property is not configurable.
          if (is_strict(language_mode)) {
            isolate->Throw(*isolate->factory()->NewTypeError(
                MessageTemplate::kStrictDeleteProperty, it->GetName(),
                receiver));
            return Nothing<bool>();
          }
          return Just(false);
        }

        it->Delete();

        return Just(true);
      }
    }
  }

  return Just(true);
}


Maybe<bool> JSReceiver::DeleteElement(Handle<JSReceiver> object, uint32_t index,
                                      LanguageMode language_mode) {
  LookupIterator it(object->GetIsolate(), object, index, object,
                    LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeleteProperty(Handle<JSReceiver> object,
                                       Handle<Name> name,
                                       LanguageMode language_mode) {
  LookupIterator it(object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
}


Maybe<bool> JSReceiver::DeletePropertyOrElement(Handle<JSReceiver> object,
                                                Handle<Name> name,
                                                LanguageMode language_mode) {
  LookupIterator it = LookupIterator::PropertyOrElement(
      name->GetIsolate(), object, name, object, LookupIterator::OWN);
  return DeleteProperty(&it, language_mode);
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
  ASSIGN_RETURN_ON_EXCEPTION(isolate, props,
                             Object::ToObject(isolate, properties), Object);

  // 4. Let keys be props.[[OwnPropertyKeys]]().
  // 5. ReturnIfAbrupt(keys).
  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, keys, KeyAccumulator::GetKeys(props, KeyCollectionMode::kOwnOnly,
                                             ALL_PROPERTIES),
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
        isolate, props, next_key, &success, LookupIterator::OWN);
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
  if (object->IsJSTypedArray()) {
    return JSTypedArray::DefineOwnProperty(
        isolate, Handle<JSTypedArray>::cast(object), key, desc, should_throw);
  }
  // TODO(neis): Special case for JSModuleNamespace?

  // OrdinaryDefineOwnProperty, by virtue of calling
  // DefineOwnPropertyIgnoreAttributes, can handle arguments
  // (ES#sec-arguments-exotic-objects-defineownproperty-p-desc).
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
      isolate, object, key, &success, LookupIterator::OWN);
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

  // Handle interceptor
  if (it.state() == LookupIterator::INTERCEPTOR) {
    if (it.HolderIsReceiverOrHiddenPrototype()) {
      Maybe<bool> result = DefinePropertyWithInterceptorInternal(
          &it, it.GetInterceptor(), should_throw, *desc);
      if (result.IsNothing() || result.FromJust()) {
        return result;
      }
    }
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
    return JSObject::CreateDataProperty(it, value, should_throw);  // Shortcut.
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
                                         Handle<Object> value,
                                         ShouldThrow should_throw) {
  DCHECK(it->GetReceiver()->IsJSObject());
  MAYBE_RETURN(JSReceiver::GetPropertyAttributes(it), Nothing<bool>());
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());
  Isolate* isolate = receiver->GetIsolate();

  if (it->IsFound()) {
    Maybe<PropertyAttributes> attributes = GetPropertyAttributes(it);
    MAYBE_RETURN(attributes, Nothing<bool>());
    if ((attributes.FromJust() & DONT_DELETE) != 0) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kRedefineDisallowed, it->GetName()));
    }
  } else {
    if (!JSObject::IsExtensible(Handle<JSObject>::cast(it->GetReceiver()))) {
      RETURN_FAILURE(
          isolate, should_throw,
          NewTypeError(MessageTemplate::kDefineDisallowed, it->GetName()));
    }
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
  JSArray::SetLength(a, new_len);
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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

  LookupIterator it(proxy, private_name, proxy);

  if (it.IsFound()) {
    DCHECK_EQ(LookupIterator::DATA, it.state());
    DCHECK_EQ(DONT_ENUM, it.property_attributes());
    it.WriteDataValue(value, false);
    return Just(true);
  }

  Handle<NameDictionary> dict(proxy->property_dictionary());
  PropertyDetails details(kData, DONT_ENUM, 0, PropertyCellType::kNoCell);
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
      isolate, object, key, &success, LookupIterator::OWN);
  DCHECK(success);  // ...so creating a LookupIterator can't fail.
  return GetOwnPropertyDescriptor(&it, desc);
}

namespace {

Maybe<bool> GetPropertyDescriptorWithInterceptor(LookupIterator* it,
                                                 PropertyDescriptor* desc) {
  bool has_access = true;
  if (it->state() == LookupIterator::ACCESS_CHECK) {
    has_access = it->HasAccess() || JSObject::AllCanRead(it);
    it->Next();
  }

  if (has_access && it->state() == LookupIterator::INTERCEPTOR) {
    Isolate* isolate = it->isolate();
    Handle<InterceptorInfo> interceptor = it->GetInterceptor();
    if (!interceptor->descriptor()->IsUndefined(isolate)) {
      Handle<Object> result;
      Handle<JSObject> holder = it->GetHolder<JSObject>();

      Handle<Object> receiver = it->GetReceiver();
      if (!receiver->IsJSReceiver()) {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, receiver, Object::ConvertReceiver(isolate, receiver),
            Nothing<bool>());
      }

      PropertyCallbackArguments args(isolate, interceptor->data(), *receiver,
                                     *holder, Object::DONT_THROW);
      if (it->IsElement()) {
        uint32_t index = it->index();
        v8::IndexedPropertyDescriptorCallback descriptorCallback =
            v8::ToCData<v8::IndexedPropertyDescriptorCallback>(
                interceptor->descriptor());

        result = args.Call(descriptorCallback, index);
      } else {
        Handle<Name> name = it->name();
        DCHECK(!name->IsPrivate());
        v8::GenericNamedPropertyDescriptorCallback descriptorCallback =
            v8::ToCData<v8::GenericNamedPropertyDescriptorCallback>(
                interceptor->descriptor());
        result = args.Call(descriptorCallback, name);
      }
      if (!result.is_null()) {
        // Request successfully intercepted, try to set the property
        // descriptor.
        Utils::ApiCheck(
            PropertyDescriptor::ToPropertyDescriptor(isolate, result, desc),
            it->IsElement() ? "v8::IndexedPropertyDescriptorCallback"
                            : "v8::NamedPropertyDescriptorCallback",
            "Invalid property descriptor.");

        return Just(true);
      }
    }
  }
  it->Restart();
  return Just(false);
}
}  // namespace

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

  Maybe<bool> intercepted = GetPropertyDescriptorWithInterceptor(it, desc);
  MAYBE_RETURN(intercepted, Nothing<bool>());
  if (intercepted.FromJust()) {
    return Just(true);
  }

  // Request was not intercepted, continue as normal.
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
    if (!Object::GetProperty(it).ToHandle(&value)) {
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
  STACK_CHECK(isolate, Nothing<bool>());

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
  if (trap->IsUndefined(isolate)) {
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
  if (!trap_result_obj->IsJSReceiver() &&
      !trap_result_obj->IsUndefined(isolate)) {
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
  if (trap_result_obj->IsUndefined(isolate)) {
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
  Isolate* isolate = elements->GetIsolate();
  if (IsFastObjectElementsKind(kind) || kind == FAST_STRING_WRAPPER_ELEMENTS) {
    int length = IsJSArray()
        ? Smi::cast(JSArray::cast(this)->length())->value()
        : elements->length();
    for (int i = 0; i < length; ++i) {
      Object* element = elements->get(i);
      if (!element->IsTheHole(isolate) && element == object) return true;
    }
  } else {
    DCHECK(kind == DICTIONARY_ELEMENTS || kind == SLOW_STRING_WRAPPER_ELEMENTS);
    Object* key =
        SeededNumberDictionary::cast(elements)->SlowReverseLookup(object);
    if (!key->IsUndefined(isolate)) return true;
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
  if (!key->IsUndefined(heap->isolate())) {
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
      SloppyArgumentsElements* elements =
          SloppyArgumentsElements::cast(this->elements());
      // Check the mapped parameters.
      for (uint32_t i = 0; i < elements->parameter_map_length(); ++i) {
        Object* value = elements->get_mapped_entry(i);
        if (!value->IsTheHole(heap->isolate()) && value == obj) return true;
      }
      // Check the arguments.
      FixedArray* arguments = elements->arguments();
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
    if (!object->HasSloppyArgumentsElements()) {  // Fast path.
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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

  if (!object->HasSloppyArgumentsElements()) {
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

  if (object->map()->has_named_interceptor() ||
      object->map()->has_indexed_interceptor()) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kCannotPreventExt));
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
  STACK_CHECK(isolate, Nothing<bool>());
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
  if (trap->IsUndefined(isolate)) {
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

namespace {

template <typename Dictionary>
void DictionaryDetailsAtPut(Isolate* isolate, Handle<Dictionary> dictionary,
                            int entry, PropertyDetails details) {
  dictionary->DetailsAtPut(entry, details);
}

template <>
void DictionaryDetailsAtPut<GlobalDictionary>(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry,
    PropertyDetails details) {
  Object* value = dictionary->ValueAt(entry);
  DCHECK(value->IsPropertyCell());
  value = PropertyCell::cast(value)->value();
  if (value->IsTheHole(isolate)) return;
  PropertyCell::PrepareForValue(dictionary, entry, handle(value, isolate),
                                details);
}

template <typename Dictionary>
void ApplyAttributesToDictionary(Isolate* isolate,
                                 Handle<Dictionary> dictionary,
                                 const PropertyAttributes attributes) {
  int capacity = dictionary->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = dictionary->KeyAt(i);
    if (dictionary->IsKey(isolate, k) &&
        !(k->IsSymbol() && Symbol::cast(k)->is_private())) {
      PropertyDetails details = dictionary->DetailsAt(i);
      int attrs = attributes;
      // READ_ONLY is an invalid attribute for JS setters/getters.
      if ((attributes & READ_ONLY) && details.kind() == kAccessor) {
        Object* v = dictionary->ValueAt(i);
        if (v->IsPropertyCell()) v = PropertyCell::cast(v)->value();
        if (v->IsAccessorPair()) attrs &= ~READ_ONLY;
      }
      details = details.CopyAddAttributes(
          static_cast<PropertyAttributes>(attrs));
      DictionaryDetailsAtPut<Dictionary>(isolate, dictionary, i, details);
    }
  }
}

}  // namespace

template <PropertyAttributes attrs>
Maybe<bool> JSObject::PreventExtensionsWithTransition(
    Handle<JSObject> object, ShouldThrow should_throw) {
  STATIC_ASSERT(attrs == NONE || attrs == SEALED || attrs == FROZEN);

  // Sealing/freezing sloppy arguments should be handled elsewhere.
  DCHECK(!object->HasSloppyArgumentsElements());

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

  if (object->map()->has_named_interceptor() ||
      object->map()->has_indexed_interceptor()) {
    MessageTemplate::Template message = MessageTemplate::kNone;
    switch (attrs) {
      case NONE:
        message = MessageTemplate::kCannotPreventExt;
        break;

      case SEALED:
        message = MessageTemplate::kCannotSeal;
        break;

      case FROZEN:
        message = MessageTemplate::kCannotFreeze;
        break;
    }
    RETURN_FAILURE(isolate, should_throw, NewTypeError(message));
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
                    : object->GetElementsAccessor()->Normalize(object);
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
        Handle<GlobalDictionary> dictionary(object->global_dictionary(),
                                            isolate);
        ApplyAttributesToDictionary(isolate, dictionary, attrs);
      } else {
        Handle<NameDictionary> dictionary(object->property_dictionary(),
                                          isolate);
        ApplyAttributesToDictionary(isolate, dictionary, attrs);
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
    Handle<SeededNumberDictionary> dictionary(object->element_dictionary(),
                                              isolate);
    // Make sure we never go back to the fast case
    object->RequireSlowElements(*dictionary);
    if (attrs != NONE) {
      ApplyAttributesToDictionary(isolate, dictionary, attrs);
    }
  }

  return Just(true);
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

template <class ContextObject>
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

template <class ContextObject>
MaybeHandle<JSObject> JSObjectWalkVisitor<ContextObject>::StructureWalk(
    Handle<JSObject> object) {
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
    // JSFunction objects are not allowed to be in normal boilerplates at all.
    DCHECK(!object->IsJSFunction());
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
        if (details.location() != kField) continue;
        DCHECK_EQ(kData, details.kind());
        FieldIndex index = FieldIndex::ForDescriptor(copy->map(), i);
        if (object->IsUnboxedDoubleField(index)) {
          if (copying) {
            // Ensure that all bits of the double value are preserved.
            uint64_t value = object->RawFastDoublePropertyAsBitsAt(index);
            copy->RawFastDoublePropertyAsBitsAtPut(index, value);
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
      KeyAccumulator accumulator(isolate, KeyCollectionMode::kOwnOnly, filter);
      accumulator.CollectOwnPropertyNames(copy, copy);
      Handle<FixedArray> names = accumulator.GetKeys();
      for (int i = 0; i < names->length(); i++) {
        DCHECK(names->get(i)->IsName());
        Handle<Name> name(Name::cast(names->get(i)));
        Handle<Object> value =
            JSObject::GetProperty(copy, name).ToHandleChecked();
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
          if (element_dictionary->IsKey(isolate, k)) {
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
  JSObjectWalkVisitor<AllocationSiteCreationContext> v(site_context, false,
                                                       kNoHints);
  MaybeHandle<JSObject> result = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}


MaybeHandle<JSObject> JSObject::DeepCopy(
    Handle<JSObject> object,
    AllocationSiteUsageContext* site_context,
    DeepCopyHints hints) {
  JSObjectWalkVisitor<AllocationSiteUsageContext> v(site_context, true, hints);
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
  if (!exotic_to_prim->IsUndefined(isolate)) {
    Handle<Object> hint_string =
        isolate->factory()->ToPrimitiveHintString(hint);
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
bool JSObject::HasEnumerableElements() {
  // TODO(cbruni): cleanup
  JSObject* object = this;
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
      Isolate* isolate = GetIsolate();
      for (int i = 0; i < length; i++) {
        if (!elements->is_the_hole(isolate, i)) return true;
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


bool Map::OnlyHasSimpleProperties() {
  // Wrapped string elements aren't explicitly stored in the elements backing
  // store, but are loaded indirectly from the underlying string.
  return !IsStringWrapperElementsKind(elements_kind()) &&
         !IsSpecialReceiverMap() && !has_hidden_prototype() &&
         !is_dictionary_map();
}

MUST_USE_RESULT Maybe<bool> FastGetOwnValuesOrEntries(
    Isolate* isolate, Handle<JSReceiver> receiver, bool get_entries,
    Handle<FixedArray>* result) {
  Handle<Map> map(JSReceiver::cast(*receiver)->map(), isolate);

  if (!map->IsJSObjectMap()) return Just(false);
  if (!map->OnlyHasSimpleProperties()) return Just(false);

  Handle<JSObject> object(JSObject::cast(*receiver));

  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate);
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  int number_of_own_elements =
      object->GetElementsAccessor()->GetCapacity(*object, object->elements());
  Handle<FixedArray> values_or_entries = isolate->factory()->NewFixedArray(
      number_of_own_descriptors + number_of_own_elements);
  int count = 0;

  if (object->elements() != isolate->heap()->empty_fixed_array()) {
    MAYBE_RETURN(object->GetElementsAccessor()->CollectValuesOrEntries(
                     isolate, object, values_or_entries, get_entries, &count,
                     ENUMERABLE_STRINGS),
                 Nothing<bool>());
  }

  bool stable = object->map() == *map;

  for (int index = 0; index < number_of_own_descriptors; index++) {
    Handle<Name> next_key(descriptors->GetKey(index), isolate);
    if (!next_key->IsString()) continue;
    Handle<Object> prop_value;

    // Directly decode from the descriptor array if |from| did not change shape.
    if (stable) {
      PropertyDetails details = descriptors->GetDetails(index);
      if (!details.IsEnumerable()) continue;
      if (details.kind() == kData) {
        if (details.location() == kDescriptor) {
          prop_value = handle(descriptors->GetValue(index), isolate);
        } else {
          Representation representation = details.representation();
          FieldIndex field_index = FieldIndex::ForDescriptor(*map, index);
          prop_value =
              JSObject::FastPropertyAt(object, representation, field_index);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, prop_value, JSReceiver::GetProperty(object, next_key),
            Nothing<bool>());
        stable = object->map() == *map;
      }
    } else {
      // If the map did change, do a slower lookup. We are still guaranteed that
      // the object has a simple shape, and that the key is a name.
      LookupIterator it(object, next_key, LookupIterator::OWN_SKIP_INTERCEPTOR);
      if (!it.IsFound()) continue;
      DCHECK(it.state() == LookupIterator::DATA ||
             it.state() == LookupIterator::ACCESSOR);
      if (!it.IsEnumerable()) continue;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate, prop_value, Object::GetProperty(&it), Nothing<bool>());
    }

    if (get_entries) {
      prop_value = MakeEntryPair(isolate, next_key, prop_value);
    }

    values_or_entries->set(count, *prop_value);
    count++;
  }

  if (count < values_or_entries->length()) values_or_entries->Shrink(count);
  *result = values_or_entries;
  return Just(true);
}

MaybeHandle<FixedArray> GetOwnValuesOrEntries(Isolate* isolate,
                                              Handle<JSReceiver> object,
                                              PropertyFilter filter,
                                              bool get_entries) {
  Handle<FixedArray> values_or_entries;
  if (filter == ENUMERABLE_STRINGS) {
    Maybe<bool> fast_values_or_entries = FastGetOwnValuesOrEntries(
        isolate, object, get_entries, &values_or_entries);
    if (fast_values_or_entries.IsNothing()) return MaybeHandle<FixedArray>();
    if (fast_values_or_entries.FromJust()) return values_or_entries;
  }

  PropertyFilter key_filter =
      static_cast<PropertyFilter>(filter & ~ONLY_ENUMERABLE);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly, key_filter,
                              GetKeysConversion::kConvertToString),
      MaybeHandle<FixedArray>());

  values_or_entries = isolate->factory()->NewFixedArray(keys->length());
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
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
  return DefineAccessor(&it, getter, setter, attributes);
}


MaybeHandle<Object> JSObject::DefineAccessor(LookupIterator* it,
                                             Handle<Object> getter,
                                             Handle<Object> setter,
                                             PropertyAttributes attributes) {
  Isolate* isolate = it->isolate();

  it->UpdateProtector();

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

  DCHECK(getter->IsCallable() || getter->IsUndefined(isolate) ||
         getter->IsNull(isolate) || getter->IsFunctionTemplateInfo());
  DCHECK(setter->IsCallable() || setter->IsUndefined(isolate) ||
         setter->IsNull(isolate) || setter->IsFunctionTemplateInfo());
  it->TransitionToAccessorProperty(getter, setter, attributes);

  return isolate->factory()->undefined_value();
}


MaybeHandle<Object> JSObject::SetAccessor(Handle<JSObject> object,
                                          Handle<AccessorInfo> info) {
  Isolate* isolate = object->GetIsolate();
  Handle<Name> name(Name::cast(info->name()), isolate);

  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, object, name, LookupIterator::OWN_SKIP_INTERCEPTOR);

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

Object* JSObject::SlowReverseLookup(Object* value) {
  if (HasFastProperties()) {
    int number_of_own_descriptors = map()->NumberOfOwnDescriptors();
    DescriptorArray* descs = map()->instance_descriptors();
    bool value_is_number = value->IsNumber();
    for (int i = 0; i < number_of_own_descriptors; i++) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.location() == kField) {
        DCHECK_EQ(kData, details.kind());
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
      } else {
        DCHECK_EQ(kDescriptor, details.location());
        if (details.kind() == kData) {
          if (descs->GetValue(i) == value) {
            return descs->GetKey(i);
          }
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
  bool use_cache =
      !fast_map->is_prototype_map() && !maybe_cache->IsUndefined(isolate);
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
        DCHECK(fresh->raw_transitions() == Smi::kZero);
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
#if V8_TRACE_MAPS
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

// Return an immutable prototype exotic object version of the input map.
// Never even try to cache it in the transition tree, as it is intended
// for the global object and its prototype chain, and excluding it saves
// memory on the map transition tree.

// static
Handle<Map> Map::TransitionToImmutableProto(Handle<Map> map) {
  Handle<Map> new_map = Map::Copy(map, "ImmutablePrototype");
  new_map->set_immutable_proto(true);
  return new_map;
}

namespace {
void EnsureInitialMap(Handle<Map> map) {
#ifdef DEBUG
  Isolate* isolate = map->GetIsolate();
  // Strict function maps have Function as a constructor but the
  // Function's initial map is a sloppy function map. Same holds for
  // GeneratorFunction / AsyncFunction and its initial map.
  Object* constructor = map->GetConstructor();
  DCHECK(constructor->IsJSFunction());
  DCHECK(*map == JSFunction::cast(constructor)->initial_map() ||
         *map == *isolate->strict_function_map() ||
         *map == *isolate->generator_function_map() ||
         *map == *isolate->async_function_map());
#endif
  // Initial maps must always own their descriptors and it's descriptor array
  // does not contain descriptors that do not belong to the map.
  DCHECK(map->owns_descriptors());
  DCHECK_EQ(map->NumberOfOwnDescriptors(),
            map->instance_descriptors()->number_of_descriptors());
}
}  // namespace

// static
Handle<Map> Map::CopyInitialMapNormalized(Handle<Map> map,
                                          PropertyNormalizationMode mode) {
  EnsureInitialMap(map);
  return CopyNormalized(map, mode);
}

// static
Handle<Map> Map::CopyInitialMap(Handle<Map> map, int instance_size,
                                int in_object_properties,
                                int unused_property_fields) {
  EnsureInitialMap(map);
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

#if V8_TRACE_MAPS

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

#endif  // V8_TRACE_MAPS

void Map::ConnectTransition(Handle<Map> parent, Handle<Map> child,
                            Handle<Name> name, SimpleTransitionFlag flag) {
  Isolate* isolate = parent->GetIsolate();
  // Do not track transitions during bootstrap except for element transitions.
  if (isolate->bootstrapper()->IsActive() &&
      !name.is_identical_to(isolate->factory()->elements_transition_symbol())) {
    return;
  }
  if (!parent->GetBackPointer()->IsUndefined(isolate)) {
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
#if V8_TRACE_MAPS
    Map::TraceTransition("NoTransition", *parent, *child, *name);
#endif
  } else {
    TransitionArray::Insert(parent, name, child, flag);
#if V8_TRACE_MAPS
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
      descriptors->GeneralizeAllFields();
      result->InitializeDescriptors(*descriptors,
                                    LayoutDescriptor::FastPointerLayout());
    }
  } else {
    result->InitializeDescriptors(*descriptors, *layout_descriptor);
  }
#if V8_TRACE_MAPS
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
  // constructor. Initial maps for strict mode are cached as special transitions
  // using |strict_function_transition_symbol| as a key.
  if (language_mode == SLOPPY) return initial_map;
  Isolate* isolate = initial_map->GetIsolate();

  int map_index = Context::FunctionMapIndex(language_mode, kind);
  Handle<Map> function_map(
      Map::cast(isolate->native_context()->get(map_index)));

  STATIC_ASSERT(LANGUAGE_END == 2);
  DCHECK_EQ(STRICT, language_mode);
  Handle<Symbol> transition_symbol =
      isolate->factory()->strict_function_transition_symbol();
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

#if V8_TRACE_MAPS
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

namespace {

bool CanHoldValue(DescriptorArray* descriptors, int descriptor,
                  PropertyConstness constness, Object* value) {
  PropertyDetails details = descriptors->GetDetails(descriptor);
  if (details.location() == kField) {
    if (details.kind() == kData) {
      return IsGeneralizableTo(constness, details.constness()) &&
             value->FitsRepresentation(details.representation()) &&
             descriptors->GetFieldType(descriptor)->NowContains(value);
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      return false;
    }

  } else {
    DCHECK_EQ(kDescriptor, details.location());
    DCHECK_EQ(kConst, details.constness());
    if (details.kind() == kData) {
      DCHECK(!FLAG_track_constant_fields);
      DCHECK(descriptors->GetValue(descriptor) != value ||
             value->FitsRepresentation(details.representation()));
      return descriptors->GetValue(descriptor) == value;
    } else {
      DCHECK_EQ(kAccessor, details.kind());
      return false;
    }
  }
  UNREACHABLE();
  return false;
}

Handle<Map> UpdateDescriptorForValue(Handle<Map> map, int descriptor,
                                     PropertyConstness constness,
                                     Handle<Object> value) {
  if (CanHoldValue(map->instance_descriptors(), descriptor, constness,
                   *value)) {
    return map;
  }

  Isolate* isolate = map->GetIsolate();
  PropertyAttributes attributes =
      map->instance_descriptors()->GetDetails(descriptor).attributes();
  Representation representation = value->OptimalRepresentation();
  Handle<FieldType> type = value->OptimalType(isolate, representation);

  MapUpdater mu(isolate, map);
  return mu.ReconfigureToDataField(descriptor, attributes, constness,
                                   representation, type);
}

}  // namespace

// static
Handle<Map> Map::PrepareForDataProperty(Handle<Map> map, int descriptor,
                                        PropertyConstness constness,
                                        Handle<Object> value) {
  // Dictionaries can store any property value.
  DCHECK(!map->is_dictionary_map());
  // Update to the newest map before storing the property.
  return UpdateDescriptorForValue(Update(map), descriptor, constness, value);
}

Handle<Map> Map::TransitionToDataProperty(Handle<Map> map, Handle<Name> name,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          PropertyConstness constness,
                                          StoreFromKeyed store_mode) {
  RuntimeCallTimerScope stats_scope(
      *map, map->is_prototype_map()
                ? &RuntimeCallStats::PrototypeMap_TransitionToDataProperty
                : &RuntimeCallStats::Map_TransitionToDataProperty);

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

    return UpdateDescriptorForValue(transition, descriptor, constness, value);
  }

  TransitionFlag flag = INSERT_TRANSITION;
  MaybeHandle<Map> maybe_map;
  if (!FLAG_track_constant_fields && value->IsJSFunction()) {
    maybe_map = Map::CopyWithConstant(map, name, value, attributes, flag);
  } else if (!map->TooManyFastProperties(store_mode)) {
    Isolate* isolate = name->GetIsolate();
    Representation representation = value->OptimalRepresentation();
    Handle<FieldType> type = value->OptimalType(isolate, representation);
    maybe_map = Map::CopyWithField(map, name, type, attributes, constness,
                                   representation, flag);
  }

  Handle<Map> result;
  if (!maybe_map.ToHandle(&result)) {
    Isolate* isolate = name->GetIsolate();
    const char* reason = "TooManyFastProperties";
#if V8_TRACE_MAPS
    std::unique_ptr<ScopedVector<char>> buffer;
    if (FLAG_trace_maps) {
      ScopedVector<char> name_buffer(100);
      name->NameShortPrint(name_buffer);
      buffer.reset(new ScopedVector<char>(128));
      SNPrintF(*buffer, "TooManyFastProperties %s", name_buffer.start());
      reason = buffer->start();
    }
#endif
    Handle<Object> maybe_constructor(map->GetConstructor(), isolate);
    if (FLAG_feedback_normalization && map->new_target_is_base() &&
        maybe_constructor->IsJSFunction() &&
        !JSFunction::cast(*maybe_constructor)->shared()->native()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(maybe_constructor);
      DCHECK_NE(*constructor,
                constructor->context()->native_context()->object_function());
      Handle<Map> initial_map(constructor->initial_map(), isolate);
      result = Map::Normalize(initial_map, CLEAR_INOBJECT_PROPERTIES, reason);
      initial_map->DeprecateTransitionTree();
      Handle<Object> prototype(result->prototype(), isolate);
      JSFunction::SetInitialMap(constructor, result, prototype);

      // Deoptimize all code that embeds the previous initial map.
      initial_map->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kInitialMapChangedGroup);
      if (!result->EquivalentToForNormalization(*map,
                                                CLEAR_INOBJECT_PROPERTIES)) {
        result = Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES, reason);
      }
    } else {
      result = Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES, reason);
    }
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
    return CopyGeneralizeAllFields(map, map->elements_kind(), descriptor, kind,
                                   attributes,
                                   "GenAll_AttributesMismatchProtoMap");
  }

  if (FLAG_trace_generalization) {
    map->PrintReconfiguration(stdout, descriptor, kind, attributes);
  }

  Isolate* isolate = map->GetIsolate();

  MapUpdater mu(isolate, map);
  DCHECK_EQ(kData, kind);  // Only kData case is supported so far.
  Handle<Map> new_map = mu.ReconfigureToDataField(
      descriptor, attributes, kDefaultFieldConstness, Representation::None(),
      FieldType::None(isolate));
  return new_map;
}

Handle<Map> Map::TransitionToAccessorProperty(Isolate* isolate, Handle<Map> map,
                                              Handle<Name> name, int descriptor,
                                              Handle<Object> getter,
                                              Handle<Object> setter,
                                              PropertyAttributes attributes) {
  RuntimeCallTimerScope stats_scope(
      isolate,
      map->is_prototype_map()
          ? &RuntimeCallStats::PrototypeMap_TransitionToAccessorProperty
          : &RuntimeCallStats::Map_TransitionToAccessorProperty);

  // At least one of the accessors needs to be a new value.
  DCHECK(!getter->IsNull(isolate) || !setter->IsNull(isolate));
  DCHECK(name->IsUniqueName());

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
    if (!pair->Equals(*getter, *setter)) {
      return Map::Normalize(map, mode, "TransitionToDifferentAccessor");
    }

    return transition;
  }

  Handle<AccessorPair> pair;
  DescriptorArray* old_descriptors = map->instance_descriptors();
  if (descriptor != DescriptorArray::kNotFound) {
    if (descriptor != map->LastAdded()) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonLast");
    }
    PropertyDetails old_details = old_descriptors->GetDetails(descriptor);
    if (old_details.kind() != kAccessor) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonAccessors");
    }

    if (old_details.attributes() != attributes) {
      return Map::Normalize(map, mode, "AccessorsWithAttributes");
    }

    Handle<Object> maybe_pair(old_descriptors->GetValue(descriptor), isolate);
    if (!maybe_pair->IsAccessorPair()) {
      return Map::Normalize(map, mode, "AccessorsOverwritingNonPair");
    }

    Handle<AccessorPair> current_pair = Handle<AccessorPair>::cast(maybe_pair);
    if (current_pair->Equals(*getter, *setter)) return map;

    bool overwriting_accessor = false;
    if (!getter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_GETTER)->IsNull(isolate) &&
        current_pair->get(ACCESSOR_GETTER) != *getter) {
      overwriting_accessor = true;
    }
    if (!setter->IsNull(isolate) &&
        !current_pair->get(ACCESSOR_SETTER)->IsNull(isolate) &&
        current_pair->get(ACCESSOR_SETTER) != *setter) {
      overwriting_accessor = true;
    }
    if (overwriting_accessor) {
      return Map::Normalize(map, mode, "AccessorsOverwritingAccessors");
    }

    pair = AccessorPair::Copy(Handle<AccessorPair>::cast(maybe_pair));
  } else if (map->NumberOfOwnDescriptors() >= kMaxNumberOfDescriptors ||
             map->TooManyFastProperties(CERTAINLY_NOT_STORE_FROM_KEYED)) {
    return Map::Normalize(map, CLEAR_INOBJECT_PROPERTIES, "TooManyAccessors");
  } else {
    pair = isolate->factory()->NewAccessorPair();
  }

  pair->SetComponents(*getter, *setter);

  TransitionFlag flag = INSERT_TRANSITION;
  Descriptor d = Descriptor::AccessorConstant(name, pair, attributes);
  return Map::CopyInsertDescriptor(map, &d, flag);
}


Handle<Map> Map::CopyAddDescriptor(Handle<Map> map,
                                   Descriptor* descriptor,
                                   TransitionFlag flag) {
  Handle<DescriptorArray> descriptors(map->instance_descriptors());

  // Share descriptors only if map owns descriptors and it not an initial map.
  if (flag == INSERT_TRANSITION && map->owns_descriptors() &&
      !map->GetBackPointer()->IsUndefined(map->GetIsolate()) &&
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
        if (details.kind() != kAccessor || !value->IsAccessorPair()) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      descriptors->Set(i, key, value, details);
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
    if (details.kind() != other_details.kind() ||
        details.location() != other_details.location() ||
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

// Helper class to manage a Map's code cache. The layout depends on the number
// of entries; this is worthwhile because most code caches are very small,
// but some are huge (thousands of entries).
// For zero entries, the EmptyFixedArray is used.
// For one entry, we use a 2-element FixedArray containing [name, code].
// For 2..100 entries, we use a FixedArray with linear lookups, the layout is:
//   [0] - number of slots that are currently in use
//   [1] - first name
//   [2] - first code
//   [3] - second name
//   [4] - second code
//   etc.
// For more than 128 entries, we use a CodeCacheHashTable.
class CodeCache : public AllStatic {
 public:
  // Returns the new cache, to be stored on the map.
  static Handle<FixedArray> Put(Isolate* isolate, Handle<FixedArray> cache,
                                Handle<Name> name, Handle<Code> code) {
    int length = cache->length();
    if (length == 0) return PutFirstElement(isolate, name, code);
    if (length == kEntrySize) {
      return PutSecondElement(isolate, cache, name, code);
    }
    if (length <= kLinearMaxSize) {
      Handle<FixedArray> result = PutLinearElement(isolate, cache, name, code);
      if (!result.is_null()) return result;
      // Fall through if linear storage is getting too large.
    }
    return PutHashTableElement(isolate, cache, name, code);
  }

  static Code* Lookup(FixedArray* cache, Name* name, Code::Flags flags) {
    int length = cache->length();
    if (length == 0) return nullptr;
    if (length == kEntrySize) return OneElementLookup(cache, name, flags);
    if (!cache->IsCodeCacheHashTable()) {
      return LinearLookup(cache, name, flags);
    } else {
      return CodeCacheHashTable::cast(cache)->Lookup(name, flags);
    }
  }

 private:
  static const int kNameIndex = 0;
  static const int kCodeIndex = 1;
  static const int kEntrySize = 2;

  static const int kLinearUsageIndex = 0;
  static const int kLinearReservedSlots = 1;
  static const int kLinearInitialCapacity = 2;
  static const int kLinearMaxSize = 257;  // == LinearSizeFor(128);

  static const int kHashTableInitialCapacity = 200;  // Number of entries.

  static int LinearSizeFor(int entries) {
    return kLinearReservedSlots + kEntrySize * entries;
  }

  static int LinearNewSize(int old_size) {
    int old_entries = (old_size - kLinearReservedSlots) / kEntrySize;
    return LinearSizeFor(old_entries * 2);
  }

  static Code* OneElementLookup(FixedArray* cache, Name* name,
                                Code::Flags flags) {
    DCHECK_EQ(cache->length(), kEntrySize);
    if (cache->get(kNameIndex) != name) return nullptr;
    Code* maybe_code = Code::cast(cache->get(kCodeIndex));
    if (maybe_code->flags() != flags) return nullptr;
    return maybe_code;
  }

  static Code* LinearLookup(FixedArray* cache, Name* name, Code::Flags flags) {
    DCHECK_GE(cache->length(), kEntrySize);
    DCHECK(!cache->IsCodeCacheHashTable());
    int usage = GetLinearUsage(cache);
    for (int i = kLinearReservedSlots; i < usage; i += kEntrySize) {
      if (cache->get(i + kNameIndex) != name) continue;
      Code* code = Code::cast(cache->get(i + kCodeIndex));
      if (code->flags() == flags) return code;
    }
    return nullptr;
  }

  static Handle<FixedArray> PutFirstElement(Isolate* isolate, Handle<Name> name,
                                            Handle<Code> code) {
    Handle<FixedArray> cache = isolate->factory()->NewFixedArray(kEntrySize);
    cache->set(kNameIndex, *name);
    cache->set(kCodeIndex, *code);
    return cache;
  }

  static Handle<FixedArray> PutSecondElement(Isolate* isolate,
                                             Handle<FixedArray> cache,
                                             Handle<Name> name,
                                             Handle<Code> code) {
    DCHECK_EQ(cache->length(), kEntrySize);
    Handle<FixedArray> new_cache = isolate->factory()->NewFixedArray(
        LinearSizeFor(kLinearInitialCapacity));
    new_cache->set(kLinearReservedSlots + kNameIndex, cache->get(kNameIndex));
    new_cache->set(kLinearReservedSlots + kCodeIndex, cache->get(kCodeIndex));
    new_cache->set(LinearSizeFor(1) + kNameIndex, *name);
    new_cache->set(LinearSizeFor(1) + kCodeIndex, *code);
    new_cache->set(kLinearUsageIndex, Smi::FromInt(LinearSizeFor(2)));
    return new_cache;
  }

  static Handle<FixedArray> PutLinearElement(Isolate* isolate,
                                             Handle<FixedArray> cache,
                                             Handle<Name> name,
                                             Handle<Code> code) {
    int length = cache->length();
    int usage = GetLinearUsage(*cache);
    DCHECK_LE(usage, length);
    // Check if we need to grow.
    if (usage == length) {
      int new_length = LinearNewSize(length);
      if (new_length > kLinearMaxSize) return Handle<FixedArray>::null();
      Handle<FixedArray> new_cache =
          isolate->factory()->NewFixedArray(new_length);
      for (int i = kLinearReservedSlots; i < length; i++) {
        new_cache->set(i, cache->get(i));
      }
      cache = new_cache;
    }
    // Store new entry.
    DCHECK_GE(cache->length(), usage + kEntrySize);
    cache->set(usage + kNameIndex, *name);
    cache->set(usage + kCodeIndex, *code);
    cache->set(kLinearUsageIndex, Smi::FromInt(usage + kEntrySize));
    return cache;
  }

  static Handle<FixedArray> PutHashTableElement(Isolate* isolate,
                                                Handle<FixedArray> cache,
                                                Handle<Name> name,
                                                Handle<Code> code) {
    // Check if we need to transition from linear to hash table storage.
    if (!cache->IsCodeCacheHashTable()) {
      // Check that the initial hash table capacity is large enough.
      DCHECK_EQ(kLinearMaxSize, LinearSizeFor(128));
      STATIC_ASSERT(kHashTableInitialCapacity > 128);

      int length = cache->length();
      // Only migrate from linear storage when it's full.
      DCHECK_EQ(length, GetLinearUsage(*cache));
      DCHECK_EQ(length, kLinearMaxSize);
      Handle<CodeCacheHashTable> table =
          CodeCacheHashTable::New(isolate, kHashTableInitialCapacity);
      HandleScope scope(isolate);
      for (int i = kLinearReservedSlots; i < length; i += kEntrySize) {
        Handle<Name> old_name(Name::cast(cache->get(i + kNameIndex)), isolate);
        Handle<Code> old_code(Code::cast(cache->get(i + kCodeIndex)), isolate);
        CodeCacheHashTable::Put(table, old_name, old_code);
      }
      cache = table;
    }
    // Store new entry.
    DCHECK(cache->IsCodeCacheHashTable());
    return CodeCacheHashTable::Put(Handle<CodeCacheHashTable>::cast(cache),
                                   name, code);
  }

  static inline int GetLinearUsage(FixedArray* linear_cache) {
    DCHECK_GT(linear_cache->length(), kEntrySize);
    return Smi::cast(linear_cache->get(kLinearUsageIndex))->value();
  }
};

void Map::UpdateCodeCache(Handle<Map> map,
                          Handle<Name> name,
                          Handle<Code> code) {
  Isolate* isolate = map->GetIsolate();
  Handle<FixedArray> cache(map->code_cache(), isolate);
  Handle<FixedArray> new_cache = CodeCache::Put(isolate, cache, name, code);
  map->set_code_cache(*new_cache);
}

Code* Map::LookupInCodeCache(Name* name, Code::Flags flags) {
  return CodeCache::Lookup(code_cache(), name, flags);
}


// The key in the code cache hash table consists of the property name and the
// code object. The actual match is on the name and the code flags. If a key
// is created using the flags and not a code object it can only be used for
// lookup not to create a new entry.
class CodeCacheHashTableKey : public HashTableKey {
 public:
  CodeCacheHashTableKey(Handle<Name> name, Code::Flags flags)
      : name_(name), flags_(flags), code_() {
    DCHECK(name_->IsUniqueName());
  }

  CodeCacheHashTableKey(Handle<Name> name, Handle<Code> code)
      : name_(name), flags_(code->flags()), code_(code) {
    DCHECK(name_->IsUniqueName());
  }

  bool IsMatch(Object* other) override {
    DCHECK(other->IsFixedArray());
    FixedArray* pair = FixedArray::cast(other);
    Name* name = Name::cast(pair->get(0));
    Code::Flags flags = Code::cast(pair->get(1))->flags();
    if (flags != flags_) return false;
    DCHECK(name->IsUniqueName());
    return *name_ == name;
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


Handle<CodeCacheHashTable> CodeCacheHashTable::Put(
    Handle<CodeCacheHashTable> cache, Handle<Name> name, Handle<Code> code) {
  CodeCacheHashTableKey key(name, code);

  Handle<CodeCacheHashTable> new_cache = EnsureCapacity(cache, 1, &key);

  int entry = new_cache->FindInsertionEntry(key.Hash());
  Handle<Object> k = key.AsHandle(cache->GetIsolate());

  new_cache->set(EntryToIndex(entry), *k);
  new_cache->ElementAdded();
  return new_cache;
}

Code* CodeCacheHashTable::Lookup(Name* name, Code::Flags flags) {
  DisallowHeapAllocation no_alloc;
  CodeCacheHashTableKey key(handle(name), flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return nullptr;
  return Code::cast(FixedArray::cast(get(EntryToIndex(entry)))->get(1));
}

Handle<FixedArray> FixedArray::SetAndGrow(Handle<FixedArray> array, int index,
                                          Handle<Object> value) {
  if (index < array->length()) {
    array->set(index, *value);
    return array;
  }
  int capacity = array->length();
  do {
    capacity = JSObject::NewElementsCapacity(capacity);
  } while (capacity <= index);
  Handle<FixedArray> new_array =
      array->GetIsolate()->factory()->NewUninitializedFixedArray(capacity);
  array->CopyTo(0, *new_array, 0, array->length());
  new_array->FillWithHoles(array->length(), new_array->length());
  new_array->set(index, *value);
  return new_array;
}

void FixedArray::Shrink(int new_length) {
  DCHECK(0 <= new_length && new_length <= length());
  if (new_length < length()) {
    GetHeap()->RightTrimFixedArray(this, length() - new_length);
  }
}

void FixedArray::CopyTo(int pos, FixedArray* dest, int dest_pos,
                        int len) const {
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
    result->set(index, Smi::kZero);
    index++;
  }
  return Handle<WeakFixedArray>::cast(result);
}

// static
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

// static
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

// static
Handle<ArrayList> ArrayList::New(Isolate* isolate, int size) {
  Handle<ArrayList> result = Handle<ArrayList>::cast(
      isolate->factory()->NewFixedArray(size + kFirstIndex));
  result->SetLength(0);
  return result;
}

Handle<FixedArray> ArrayList::Elements() const {
  Handle<FixedArray> result = GetIsolate()->factory()->NewFixedArray(Length());
  // Do not copy the first entry, i.e., the length.
  CopyTo(kFirstIndex, *result, 0, Length());
  return result;
}

bool ArrayList::IsFull() {
  int capacity = length();
  return kFirstIndex + Length() == capacity;
}

namespace {

Handle<FixedArray> EnsureSpaceInFixedArray(Handle<FixedArray> array,
                                           int length) {
  int capacity = array->length();
  if (capacity < length) {
    Isolate* isolate = array->GetIsolate();
    int new_capacity = length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array = isolate->factory()->CopyFixedArrayAndGrow(array, grow_by);
  }
  return array;
}

}  // namespace

// static
Handle<ArrayList> ArrayList::EnsureSpace(Handle<ArrayList> array, int length) {
  const bool empty = (array->length() == 0);
  auto ret = Handle<ArrayList>::cast(
      EnsureSpaceInFixedArray(array, kFirstIndex + length));
  if (empty) ret->SetLength(0);
  return ret;
}

Handle<RegExpMatchInfo> RegExpMatchInfo::ReserveCaptures(
    Handle<RegExpMatchInfo> match_info, int capture_count) {
  DCHECK_GE(match_info->length(), kLastMatchOverhead);
  const int required_length = kFirstCaptureIndex + capture_count;
  Handle<FixedArray> result =
      EnsureSpaceInFixedArray(match_info, required_length);
  return Handle<RegExpMatchInfo>::cast(result);
}

// static
Handle<FrameArray> FrameArray::AppendJSFrame(Handle<FrameArray> in,
                                             Handle<Object> receiver,
                                             Handle<JSFunction> function,
                                             Handle<AbstractCode> code,
                                             int offset, int flags) {
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array = EnsureSpace(in, new_length);
  array->SetReceiver(frame_count, *receiver);
  array->SetFunction(frame_count, *function);
  array->SetCode(frame_count, *code);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

// static
Handle<FrameArray> FrameArray::AppendWasmFrame(Handle<FrameArray> in,
                                               Handle<Object> wasm_instance,
                                               int wasm_function_index,
                                               Handle<AbstractCode> code,
                                               int offset, int flags) {
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array = EnsureSpace(in, new_length);
  array->SetWasmInstance(frame_count, *wasm_instance);
  array->SetWasmFunctionIndex(frame_count, Smi::FromInt(wasm_function_index));
  // code will be a null handle for interpreted wasm frames.
  if (!code.is_null()) array->SetCode(frame_count, *code);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

void FrameArray::ShrinkToFit() { Shrink(LengthFor(FrameCount())); }

// static
Handle<FrameArray> FrameArray::EnsureSpace(Handle<FrameArray> array,
                                           int length) {
  return Handle<FrameArray>::cast(EnsureSpaceInFixedArray(array, length));
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
  result->set(kEnumCacheBridgeIndex, Smi::kZero);
  return Handle<DescriptorArray>::cast(result);
}

void DescriptorArray::ClearEnumCache() {
  set(kEnumCacheBridgeIndex, Smi::kZero);
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
    bridge_storage = FixedArray::cast(descriptors->get(kEnumCacheBridgeIndex));
  }
  bridge_storage->set(kEnumCacheBridgeCacheIndex, *new_cache);
  bridge_storage->set(
      kEnumCacheBridgeIndicesCacheIndex,
      new_index_cache.is_null() ? Object::cast(Smi::kZero) : *new_index_cache);
  if (needs_new_enum_cache) {
    descriptors->set(kEnumCacheBridgeIndex, bridge_storage);
  }
}

void DescriptorArray::CopyFrom(int index, DescriptorArray* src) {
  PropertyDetails details = src->GetDetails(index);
  Set(index, src->GetKey(index), src->GetValue(index), details);
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
  if (accessor->IsNull(isolate)) {
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

SharedFunctionInfo* DeoptimizationInputData::GetInlinedFunction(int index) {
  if (index == -1) {
    return SharedFunctionInfo::cast(SharedFunctionInfo());
  } else {
    return SharedFunctionInfo::cast(LiteralArray()->get(index));
  }
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
    if (pc_offset >= start_offset && pc_offset < end_offset) {
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
int HandlerTable::LookupReturn(int pc_offset) {
  for (int i = 0; i < length(); i += kReturnEntrySize) {
    int return_offset = Smi::cast(get(i + kReturnOffsetIndex))->value();
    int handler_field = Smi::cast(get(i + kReturnHandlerIndex))->value();
    if (pc_offset == return_offset) {
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

// static
Handle<String> String::Trim(Handle<String> string, TrimMode mode) {
  Isolate* const isolate = string->GetIsolate();
  string = String::Flatten(string);
  int const length = string->length();

  // Perform left trimming if requested.
  int left = 0;
  UnicodeCache* unicode_cache = isolate->unicode_cache();
  if (mode == kTrim || mode == kTrimLeft) {
    while (left < length &&
           unicode_cache->IsWhiteSpaceOrLineTerminator(string->Get(left))) {
      left++;
    }
  }

  // Perform right trimming if requested.
  int right = length;
  if (mode == kTrim || mode == kTrimRight) {
    while (
        right > left &&
        unicode_cache->IsWhiteSpaceOrLineTerminator(string->Get(right - 1))) {
      right--;
    }
  }

  return isolate->factory()->NewSubString(string, left, right);
}

bool String::LooksValid() { return GetIsolate()->heap()->Contains(this); }

// static
MaybeHandle<String> Name::ToFunctionName(Handle<Name> name) {
  if (name->IsString()) return Handle<String>::cast(name);
  // ES6 section 9.2.11 SetFunctionName, step 4.
  Isolate* const isolate = name->GetIsolate();
  Handle<Object> description(Handle<Symbol>::cast(name)->name(), isolate);
  if (description->IsUndefined(isolate)) {
    return isolate->factory()->empty_string();
  }
  IncrementalStringBuilder builder(isolate);
  builder.AppendCharacter('[');
  builder.AppendString(Handle<String>::cast(description));
  builder.AppendCharacter(']');
  return builder.Finish();
}

// static
MaybeHandle<String> Name::ToFunctionName(Handle<Name> name,
                                         Handle<String> prefix) {
  Handle<String> name_string;
  Isolate* const isolate = name->GetIsolate();
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name_string, ToFunctionName(name),
                             String);
  IncrementalStringBuilder builder(isolate);
  builder.AppendString(prefix);
  builder.AppendCharacter(' ');
  builder.AppendString(name_string);
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
    if (len == 0) return handle(Smi::kZero, isolate);

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
  } else if (shape.representation_tag() == kSlicedStringTag) {
    SlicedString* slice = SlicedString::cast(string);
    offset = slice->offset();
    string = slice->parent();
    shape = StringShape(string);
    DCHECK(shape.representation_tag() != kConsStringTag &&
           shape.representation_tag() != kSlicedStringTag);
  }
  if (shape.representation_tag() == kThinStringTag) {
    ThinString* thin = ThinString::cast(string);
    string = thin->actual();
    shape = StringShape(string);
    DCHECK(!shape.IsCons());
    DCHECK(!shape.IsSliced());
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

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
                                          RobustnessFlag robust_flag,
                                          int offset, int length,
                                          int* length_return) {
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return std::unique_ptr<char[]>();
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
  return std::unique_ptr<char[]>(result);
}

std::unique_ptr<char[]> String::ToCString(AllowNullsFlag allow_nulls,
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
    case kThinStringTag:
      UNREACHABLE();
      return NULL;
  }
  UNREACHABLE();
  return NULL;
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

char* Relocatable::Iterate(RootVisitor* v, char* thread_storage) {
  Relocatable* top = *reinterpret_cast<Relocatable**>(thread_storage);
  Iterate(v, top);
  return thread_storage + ArchiveSpacePerThread();
}

void Relocatable::Iterate(Isolate* isolate, RootVisitor* v) {
  Iterate(v, isolate->relocatable_top());
}

void Relocatable::Iterate(RootVisitor* v, Relocatable* top) {
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
        if (length == 0) break;  // Skip empty left-hand sides of ConsStrings.
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

uint16_t ThinString::ThinStringGet(int index) { return actual()->Get(index); }

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
            if (from == 0 && cons_string->second() == first) {
              CopyChars(sink + boundary, sink, boundary);
              return;
            }
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
      case kOneByteStringTag | kThinStringTag:
      case kTwoByteStringTag | kThinStringTag:
        source = ThinString::cast(source)->actual();
        break;
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

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (this->IsThinString() || other->IsThinString()) {
    if (other->IsThinString()) other = ThinString::cast(other)->actual();
    if (this->IsThinString()) {
      return ThinString::cast(this)->actual()->Equals(other);
    } else {
      return this->Equals(other);
    }
  }

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

  // Fast check: if at least one ThinString is involved, dereference it/them
  // and restart.
  if (one->IsThinString() || two->IsThinString()) {
    if (one->IsThinString()) one = handle(ThinString::cast(*one)->actual());
    if (two->IsThinString()) two = handle(ThinString::cast(*two)->actual());
    return String::Equals(one, two);
  }

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

Object* String::IndexOf(Isolate* isolate, Handle<Object> receiver,
                        Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.indexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToInteger(isolate, position));

  uint32_t index = receiver_string->ToValidIndex(*position);
  return Smi::FromInt(
      String::IndexOf(isolate, receiver_string, search_string, index));
}

namespace {

template <typename T>
int SearchString(Isolate* isolate, String::FlatContent receiver_content,
                 Vector<T> pat_vector, int start_index) {
  if (receiver_content.IsOneByte()) {
    return SearchString(isolate, receiver_content.ToOneByteVector(), pat_vector,
                        start_index);
  }
  return SearchString(isolate, receiver_content.ToUC16Vector(), pat_vector,
                      start_index);
}

}  // namespace

int String::IndexOf(Isolate* isolate, Handle<String> receiver,
                    Handle<String> search, int start_index) {
  DCHECK(0 <= start_index);
  DCHECK(start_index <= receiver->length());

  uint32_t search_length = search->length();
  if (search_length == 0) return start_index;

  uint32_t receiver_length = receiver->length();
  if (start_index + search_length > receiver_length) return -1;

  receiver = String::Flatten(receiver);
  search = String::Flatten(search);

  DisallowHeapAllocation no_gc;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before getting encoding.
  String::FlatContent receiver_content = receiver->GetFlatContent();
  String::FlatContent search_content = search->GetFlatContent();

  // dispatch on type of strings
  if (search_content.IsOneByte()) {
    Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    return SearchString<const uint8_t>(isolate, receiver_content, pat_vector,
                                       start_index);
  }
  Vector<const uc16> pat_vector = search_content.ToUC16Vector();
  return SearchString<const uc16>(isolate, receiver_content, pat_vector,
                                  start_index);
}

MaybeHandle<String> String::GetSubstitution(Isolate* isolate, Match* match,
                                            Handle<String> replacement,
                                            int start_index) {
  DCHECK_IMPLIES(match->HasNamedCaptures(), FLAG_harmony_regexp_named_captures);
  DCHECK_GE(start_index, 0);

  Factory* factory = isolate->factory();

  const int replacement_length = replacement->length();
  const int captures_length = match->CaptureCount();

  replacement = String::Flatten(replacement);

  Handle<String> dollar_string =
      factory->LookupSingleCharacterStringFromCode('$');
  int next_dollar_ix =
      String::IndexOf(isolate, replacement, dollar_string, start_index);
  if (next_dollar_ix < 0) {
    return replacement;
  }

  IncrementalStringBuilder builder(isolate);

  if (next_dollar_ix > 0) {
    builder.AppendString(factory->NewSubString(replacement, 0, next_dollar_ix));
  }

  while (true) {
    const int peek_ix = next_dollar_ix + 1;
    if (peek_ix >= replacement_length) {
      builder.AppendCharacter('$');
      return builder.Finish();
    }

    int continue_from_ix = -1;
    const uint16_t peek = replacement->Get(peek_ix);
    switch (peek) {
      case '$':  // $$
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix + 1;
        break;
      case '&':  // $& - match
        builder.AppendString(match->GetMatch());
        continue_from_ix = peek_ix + 1;
        break;
      case '`':  // $` - prefix
        builder.AppendString(match->GetPrefix());
        continue_from_ix = peek_ix + 1;
        break;
      case '\'':  // $' - suffix
        builder.AppendString(match->GetSuffix());
        continue_from_ix = peek_ix + 1;
        break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9': {
        // Valid indices are $1 .. $9, $01 .. $09 and $10 .. $99
        int scaled_index = (peek - '0');
        int advance = 1;

        if (peek_ix + 1 < replacement_length) {
          const uint16_t next_peek = replacement->Get(peek_ix + 1);
          if (next_peek >= '0' && next_peek <= '9') {
            const int new_scaled_index = scaled_index * 10 + (next_peek - '0');
            if (new_scaled_index < captures_length) {
              scaled_index = new_scaled_index;
              advance = 2;
            }
          }
        }

        if (scaled_index == 0 || scaled_index >= captures_length) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        bool capture_exists;
        Handle<String> capture;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture, match->GetCapture(scaled_index, &capture_exists),
            String);
        if (capture_exists) builder.AppendString(capture);
        continue_from_ix = peek_ix + advance;
        break;
      }
      case '<': {  // $<name> - named capture
        typedef String::Match::CaptureState CaptureState;

        if (!match->HasNamedCaptures()) {
          builder.AppendCharacter('$');
          continue_from_ix = peek_ix;
          break;
        }

        Handle<String> bracket_string =
            factory->LookupSingleCharacterStringFromCode('>');
        const int closing_bracket_ix =
            String::IndexOf(isolate, replacement, bracket_string, peek_ix + 1);

        if (closing_bracket_ix == -1) {
          THROW_NEW_ERROR(
              isolate,
              NewSyntaxError(MessageTemplate::kRegExpInvalidReplaceString,
                             replacement),
              String);
        }

        Handle<String> capture_name =
            factory->NewSubString(replacement, peek_ix + 1, closing_bracket_ix);
        Handle<String> capture;
        CaptureState capture_state;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, capture,
            match->GetNamedCapture(capture_name, &capture_state), String);

        switch (capture_state) {
          case CaptureState::INVALID:
            THROW_NEW_ERROR(
                isolate,
                NewSyntaxError(MessageTemplate::kRegExpInvalidReplaceString,
                               replacement),
                String);
            break;
          case CaptureState::UNMATCHED:
            break;
          case CaptureState::MATCHED:
            builder.AppendString(capture);
            break;
        }

        continue_from_ix = closing_bracket_ix + 1;
        break;
      }
      default:
        builder.AppendCharacter('$');
        continue_from_ix = peek_ix;
        break;
    }

    // Go the the next $ in the replacement.
    // TODO(jgruber): Single-char lookups could be much more efficient.
    DCHECK_NE(continue_from_ix, -1);
    next_dollar_ix =
        String::IndexOf(isolate, replacement, dollar_string, continue_from_ix);

    // Return if there are no more $ characters in the replacement. If we
    // haven't reached the end, we need to append the suffix.
    if (next_dollar_ix < 0) {
      if (continue_from_ix < replacement_length) {
        builder.AppendString(factory->NewSubString(
            replacement, continue_from_ix, replacement_length));
      }
      return builder.Finish();
    }

    // Append substring between the previous and the next $ character.
    if (next_dollar_ix > continue_from_ix) {
      builder.AppendString(
          factory->NewSubString(replacement, continue_from_ix, next_dollar_ix));
    }
  }

  UNREACHABLE();
  return MaybeHandle<String>();
}

namespace {  // for String.Prototype.lastIndexOf

template <typename schar, typename pchar>
int StringMatchBackwards(Vector<const schar> subject,
                         Vector<const pchar> pattern, int idx) {
  int pattern_length = pattern.length();
  DCHECK(pattern_length >= 1);
  DCHECK(idx + pattern_length <= subject.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pattern_length; i++) {
      uc16 c = pattern[i];
      if (c > String::kMaxOneByteCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pattern[0];
  for (int i = idx; i >= 0; i--) {
    if (subject[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i + j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}

}  // namespace

Object* String::LastIndexOf(Isolate* isolate, Handle<Object> receiver,
                            Handle<Object> search, Handle<Object> position) {
  if (receiver->IsNullOrUndefined(isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "String.prototype.lastIndexOf")));
  }
  Handle<String> receiver_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, receiver_string,
                                     Object::ToString(isolate, receiver));

  Handle<String> search_string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, search_string,
                                     Object::ToString(isolate, search));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                     Object::ToNumber(position));

  uint32_t start_index;

  if (position->IsNaN()) {
    start_index = receiver_string->length();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, position,
                                       Object::ToInteger(isolate, position));
    start_index = receiver_string->ToValidIndex(*position);
  }

  uint32_t pattern_length = search_string->length();
  uint32_t receiver_length = receiver_string->length();

  if (start_index + pattern_length > receiver_length) {
    start_index = receiver_length - pattern_length;
  }

  if (pattern_length == 0) {
    return Smi::FromInt(start_index);
  }

  receiver_string = String::Flatten(receiver_string);
  search_string = String::Flatten(search_string);

  int last_index = -1;
  DisallowHeapAllocation no_gc;  // ensure vectors stay valid

  String::FlatContent receiver_content = receiver_string->GetFlatContent();
  String::FlatContent search_content = search_string->GetFlatContent();

  if (search_content.IsOneByte()) {
    Vector<const uint8_t> pat_vector = search_content.ToOneByteVector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  } else {
    Vector<const uc16> pat_vector = search_content.ToUC16Vector();
    if (receiver_content.IsOneByte()) {
      last_index = StringMatchBackwards(receiver_content.ToOneByteVector(),
                                        pat_vector, start_index);
    } else {
      last_index = StringMatchBackwards(receiver_content.ToUC16Vector(),
                                        pat_vector, start_index);
    }
  }
  return Smi::FromInt(last_index);
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

template <>
bool String::IsEqualTo(Vector<const uint8_t> str) {
  return IsOneByteEqualTo(str);
}

template <>
bool String::IsEqualTo(Vector<const uc16> str) {
  return IsTwoByteEqualTo(str);
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
  Heap* heap = string->GetHeap();
  if (new_length == 0) return heap->isolate()->factory()->empty_string();

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

  // Sizes are pointer size aligned, so that we can use filler objects
  // that are a multiple of pointer size.
  heap->CreateFillerObjectAt(start_of_string + new_size, delta,
                             ClearRecordedSlots::kNo);
  heap->AdjustLiveBytes(*string, -delta);

  // We are storing the new length using release store after creating a filler
  // for the left-over space to avoid races with the sweeper thread.
  string->synchronized_set_length(new_length);

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
  DCHECK_EQ(length <= String::kMaxCachedArrayIndexLength,
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
         first->new_target_is_base() == second->new_target_is_base() &&
         first->has_hidden_prototype() == second->has_hidden_prototype();
}

}  // namespace


bool Map::EquivalentToForTransition(Map* other) {
  if (!CheckEquivalent(this, other)) return false;
  if (instance_type() == JS_FUNCTION_TYPE) {
    // JSFunctions require more checks to ensure that sloppy function is
    // not equivalent to strict function.
    int nof = Min(NumberOfOwnDescriptors(), other->NumberOfOwnDescriptors());
    return instance_descriptors()->IsEqualUpTo(other->instance_descriptors(),
                                               nof);
  }
  return true;
}

bool Map::EquivalentToForElementsKindTransition(Map* other) {
  if (!EquivalentToForTransition(other)) return false;
#ifdef DEBUG
  // Ensure that we don't try to generate elements kind transitions from maps
  // with fields that may be generalized in-place. This must already be handled
  // during addition of a new field.
  DescriptorArray* descriptors = instance_descriptors();
  int nof = NumberOfOwnDescriptors();
  for (int i = 0; i < nof; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() == kField) {
      DCHECK(!IsInplaceGeneralizableField(details.constness(),
                                          details.representation(),
                                          descriptors->GetFieldType(i)));
    }
  }
#endif
  return true;
}

bool Map::EquivalentToForNormalization(Map* other,
                                       PropertyNormalizationMode mode) {
  int properties =
      mode == CLEAR_INOBJECT_PROPERTIES ? 0 : other->GetInObjectProperties();
  return CheckEquivalent(this, other) && bit_field2() == other->bit_field2() &&
         GetInObjectProperties() == properties &&
         JSObject::GetEmbedderFieldCount(this) ==
             JSObject::GetEmbedderFieldCount(other);
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
  DCHECK(!IsOptimized());
  DCHECK(shared()->allows_lazy_compilation() ||
         !shared()->optimization_disabled());
  set_code_no_write_barrier(
      isolate->builtins()->builtin(Builtins::kCompileOptimized));
  // No write barrier required, since the builtin is part of the root set.
  if (FLAG_mark_shared_functions_for_tier_up) {
    shared()->set_marked_for_tier_up(true);
  }
}


void JSFunction::AttemptConcurrentOptimization() {
  Isolate* isolate = GetIsolate();
  if (!isolate->concurrent_recompilation_enabled() ||
      isolate->bootstrapper()->IsActive()) {
    MarkForOptimization();
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
  if (FLAG_mark_shared_functions_for_tier_up) {
    // TODO(leszeks): The compilation isn't concurrent if we trigger it using
    // this bit.
    shared()->set_marked_for_tier_up(true);
  }
}

// static
void JSFunction::EnsureLiterals(Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared(function->shared());
  Isolate* isolate = shared->GetIsolate();

  FeedbackVectorState state = function->GetFeedbackVectorState(isolate);
  switch (state) {
    case TOP_LEVEL_SCRIPT_NEEDS_VECTOR: {
      // A top level script didn't get it's literals installed.
      Handle<FeedbackVector> feedback_vector =
          FeedbackVector::New(isolate, shared);
      Handle<Cell> new_cell =
          isolate->factory()->NewOneClosureCell(feedback_vector);
      function->set_feedback_vector_cell(*new_cell);
      break;
    }
    case NEEDS_VECTOR: {
      Handle<FeedbackVector> feedback_vector =
          FeedbackVector::New(isolate, shared);
      function->feedback_vector_cell()->set_value(*feedback_vector);
      break;
    }
    case HAS_VECTOR:
      // Nothing to do.
      break;
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
  DCHECK(GetBackPointer()->IsUndefined(GetIsolate()));

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
  if (object->IsJSGlobalProxy()) return false;
  if (object->GetIsolate()->bootstrapper()->IsActive()) return false;
  return !object->map()->is_prototype_map() ||
         !object->map()->should_be_fast_prototype_map();
}

// static
void JSObject::MakePrototypesFast(Handle<Object> receiver,
                                  WhereToStart where_to_start,
                                  Isolate* isolate) {
  if (!receiver->IsJSReceiver()) return;
  for (PrototypeIterator iter(isolate, Handle<JSReceiver>::cast(receiver),
                              where_to_start);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) return;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    Map* current_map = current_obj->map();
    if (current_map->is_prototype_map()) {
      // If the map is already marked as should be fast, we're done. Its
      // prototypes will have been marked already as well.
      if (current_map->should_be_fast_prototype_map()) return;
      Handle<Map> map(current_map);
      Map::SetShouldBeFastPrototypeMap(map, true, isolate);
      JSObject::OptimizeAsPrototype(current_obj, FAST_PROTOTYPE);
    }
  }
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
  if (object->map()->is_prototype_map()) {
    if (object->map()->should_be_fast_prototype_map() &&
        !object->HasFastProperties()) {
      JSObject::MigrateSlowToFast(object, 0, "OptimizeAsPrototype");
    }
  } else {
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
  if (!object->map()->should_be_fast_prototype_map()) return;
  OptimizeAsPrototype(object, FAST_PROTOTYPE);
}


// static
void JSObject::LazyRegisterPrototypeUser(Handle<Map> user, Isolate* isolate) {
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
  DCHECK(map->is_prototype_map());
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
  while ((user = iterator.Next<Map>()) != nullptr) {
    // Walk the prototype chain (backwards, towards leaf objects) if necessary.
    InvalidatePrototypeChainsInternal(user);
  }
}


// static
void JSObject::InvalidatePrototypeChains(Map* map) {
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
void Map::SetShouldBeFastPrototypeMap(Handle<Map> map, bool value,
                                      Isolate* isolate) {
  if (value == false && !map->prototype_info()->IsPrototypeInfo()) {
    // "False" is the implicit default value, so there's nothing to do.
    return;
  }
  GetOrCreatePrototypeInfo(map, isolate)->set_should_be_fast_map(value);
}

// static
Handle<Cell> Map::GetOrCreatePrototypeChainValidityCell(Handle<Map> map,
                                                        Isolate* isolate) {
  Handle<Object> maybe_prototype;
  if (map->IsJSGlobalObjectMap()) {
    DCHECK(map->is_prototype_map());
    // Global object is prototype of a global proxy and therefore we can
    // use its validity cell for guarding global object's prototype change.
    maybe_prototype = isolate->global_object();
  } else {
    maybe_prototype =
        handle(map->GetPrototypeChainRootMap(isolate)->prototype(), isolate);
    if (!maybe_prototype->IsJSObject()) return Handle<Cell>::null();
  }
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
Handle<WeakCell> Map::GetOrCreatePrototypeWeakCell(Handle<JSObject> prototype,
                                                   Isolate* isolate) {
  DCHECK(!prototype.is_null());
  Handle<PrototypeInfo> proto_info =
      GetOrCreatePrototypeInfo(prototype, isolate);
  Object* maybe_cell = proto_info->weak_cell();
  // Return existing cell if it's already created.
  if (maybe_cell->IsWeakCell()) {
    Handle<WeakCell> cell(WeakCell::cast(maybe_cell), isolate);
    DCHECK(!cell->cleared());
    return cell;
  }
  // Otherwise create a new cell.
  Handle<WeakCell> cell = isolate->factory()->NewWeakCell(prototype);
  proto_info->set_weak_cell(*cell);
  return cell;
}

// static
void Map::SetPrototype(Handle<Map> map, Handle<Object> prototype,
                       PrototypeOptimizationMode proto_mode) {
  RuntimeCallTimerScope stats_scope(*map, &RuntimeCallStats::Map_SetPrototype);

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
    } else if (maybe_constructor->IsFunctionTemplateInfo()) {
      is_hidden =
          FunctionTemplateInfo::cast(maybe_constructor)->hidden_prototype() ||
          prototype->IsJSGlobalObject();
    }
  }
  map->set_has_hidden_prototype(is_hidden);

  WriteBarrierMode wb_mode = prototype->IsNull(map->GetIsolate())
                                 ? SKIP_WRITE_BARRIER
                                 : UPDATE_WRITE_BARRIER;
  map->set_prototype(*prototype, wb_mode);
}


Handle<Object> CacheInitialJSArrayMaps(
    Handle<Context> native_context, Handle<Map> initial_map) {
  // Replace all of the cached initial array maps in the native context with
  // the appropriate transitioned elements kind maps.
  Handle<Map> current_map = initial_map;
  ElementsKind kind = current_map->elements_kind();
  DCHECK_EQ(GetInitialFastElementsKind(), kind);
  native_context->set(Context::ArrayMapIndex(kind), *current_map);
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
    native_context->set(Context::ArrayMapIndex(next_kind), *new_map);
    current_map = new_map;
  }
  return initial_map;
}

namespace {

void SetInstancePrototype(Isolate* isolate, Handle<JSFunction> function,
                          Handle<JSReceiver> value) {
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

}  // anonymous namespace

void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared()->kind()));
  Isolate* isolate = function->GetIsolate();
  Handle<JSReceiver> construct_prototype;

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

    FunctionKind kind = function->shared()->kind();
    Handle<Context> native_context(function->context()->native_context());

    construct_prototype = Handle<JSReceiver>(
        IsGeneratorFunction(kind)
            ? IsAsyncFunction(kind)
                  ? native_context->initial_async_generator_prototype()
                  : native_context->initial_generator_prototype()
            : native_context->initial_object_prototype(),
        isolate);
  } else {
    construct_prototype = Handle<JSReceiver>::cast(value);
    function->map()->set_non_instance_prototype(false);
  }

  SetInstancePrototype(isolate, function, construct_prototype);
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
#if V8_TRACE_MAPS
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
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_DATE_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MAP_ITERATOR_TYPE:
    case JS_MAP_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_ARGUMENTS_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REGEXP_TYPE:
    case JS_SET_ITERATOR_TYPE:
    case JS_SET_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_VALUE_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_SET_TYPE:
      return true;

    case BYTECODE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case CELL_TYPE:
    case CODE_TYPE:
    case FILLER_TYPE:
    case FIXED_ARRAY_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case FOREIGN_TYPE:
    case FREE_SPACE_TYPE:
    case HEAP_NUMBER_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_PROXY_TYPE:
    case MAP_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
    case ODDBALL_TYPE:
    case PROPERTY_CELL_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case SYMBOL_TYPE:
    case WEAK_CELL_TYPE:

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
  DCHECK(function->IsConstructor() ||
         IsResumableFunction(function->shared()->kind()));
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  InstanceType instance_type;
  if (IsResumableFunction(function->shared()->kind())) {
    instance_type = IsAsyncGeneratorFunction(function->shared()->kind())
                        ? JS_ASYNC_GENERATOR_OBJECT_TYPE
                        : JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }

  // The constructor should be compiled for the optimization hints to be
  // available.
  int expected_nof_properties = 0;
  if (function->shared()->is_compiled() ||
      Compiler::Compile(function, Compiler::CLEAR_EXCEPTION)) {
    DCHECK(function->shared()->is_compiled());
    expected_nof_properties = function->shared()->expected_nof_properties();
  }

  int instance_size;
  int in_object_properties;
  CalculateInstanceSizeHelper(instance_type, 0, expected_nof_properties,
                              &instance_size, &in_object_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size);

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
    if (IsDerivedConstructor(function->shared()->kind())) {
      Handle<Object> prototype(function->instance_prototype(), isolate);
      InstanceType instance_type = constructor_initial_map->instance_type();
      DCHECK(CanSubclassHaveInobjectProperties(instance_type));
      int embedder_fields =
          JSObject::GetEmbedderFieldCount(*constructor_initial_map);
      int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                          constructor_initial_map->unused_property_fields();
      int instance_size;
      int in_object_properties;
      CalculateInstanceSizeForDerivedClass(function, instance_type,
                                           embedder_fields, &instance_size,
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
  std::unique_ptr<char[]> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", name.get());
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
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->IsUserJavaScript()) {
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

  if (FLAG_harmony_function_tostring) {
    return Handle<String>::cast(shared_info->GetSourceCodeHarmony());
  }

  IncrementalStringBuilder builder(isolate);
  FunctionKind kind = shared_info->kind();
  if (!IsArrowFunction(kind)) {
    if (IsConciseMethod(kind)) {
      if (IsAsyncGeneratorFunction(kind)) {
        builder.AppendCString("async *");
      } else if (IsGeneratorFunction(kind)) {
        builder.AppendCharacter('*');
      } else if (IsAsyncFunction(kind)) {
        builder.AppendCString("async ");
      }
    } else {
      if (IsAsyncGeneratorFunction(kind)) {
        builder.AppendCString("async function* ");
      } else if (IsGeneratorFunction(kind)) {
        builder.AppendCString("function* ");
      } else if (IsAsyncFunction(kind)) {
        builder.AppendCString("async function ");
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
  if (to_number->IsHeapNumber()) {
    oddball->set_to_number_raw_as_bits(
        Handle<HeapNumber>::cast(to_number)->value_as_bits());
  } else {
    oddball->set_to_number_raw(to_number->Number());
  }
  oddball->set_to_number(*to_number);
  oddball->set_to_string(*internalized_to_string);
  oddball->set_type_of(*internalized_type_of);
  oddball->set_kind(kind);
}

void Script::SetEvalOrigin(Handle<Script> script,
                           Handle<SharedFunctionInfo> outer_info,
                           int eval_position) {
  if (eval_position == kNoSourcePosition) {
    // If the position is missing, attempt to get the code offset from the
    // current activation.  Do not translate the code offset into source
    // position, but store it as negative value for lazy translation.
    StackTraceFrameIterator it(script->GetIsolate());
    if (!it.done() && it.is_javascript()) {
      FrameSummary summary = FrameSummary::GetTop(it.javascript_frame());
      script->set_eval_from_shared(summary.AsJavaScript().function()->shared());
      script->set_eval_from_position(-summary.code_offset());
      return;
    }
    eval_position = 0;
  }
  script->set_eval_from_shared(*outer_info);
  script->set_eval_from_position(eval_position);
}

int Script::GetEvalPosition() {
  DisallowHeapAllocation no_gc;
  DCHECK(compilation_type() == Script::COMPILATION_TYPE_EVAL);
  int position = eval_from_position();
  if (position < 0) {
    // Due to laziness, the position may not have been translated from code
    // offset yet, which would be encoded as negative integer. In that case,
    // translate and set the position.
    if (eval_from_shared()->IsUndefined(GetIsolate())) {
      position = 0;
    } else {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(eval_from_shared());
      position = shared->abstract_code()->SourcePosition(-position);
    }
    DCHECK(position >= 0);
    set_eval_from_position(position);
  }
  return position;
}

void Script::InitLineEnds(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  if (!script->line_ends()->IsUndefined(isolate)) return;
  DCHECK_NE(Script::TYPE_WASM, script->type());

  Object* src_obj = script->source();
  if (!src_obj->IsString()) {
    DCHECK(src_obj->IsUndefined(isolate));
    script->set_line_ends(isolate->heap()->empty_fixed_array());
  } else {
    DCHECK(src_obj->IsString());
    Handle<String> src(String::cast(src_obj), isolate);
    Handle<FixedArray> array = String::CalculateLineEnds(src, true);
    script->set_line_ends(*array);
  }

  DCHECK(script->line_ends()->IsFixedArray());
}

bool Script::GetPositionInfo(Handle<Script> script, int position,
                             PositionInfo* info, OffsetFlag offset_flag) {
  // For wasm, we do not create an artificial line_ends array, but do the
  // translation directly.
  if (script->type() != Script::TYPE_WASM) InitLineEnds(script);
  return script->GetPositionInfo(position, info, offset_flag);
}

bool Script::IsUserJavaScript() { return type() == Script::TYPE_NORMAL; }

namespace {
bool GetPositionInfoSlow(const Script* script, int position,
                         Script::PositionInfo* info) {
  if (!script->source()->IsString()) return false;
  if (position < 0) position = 0;

  String* source_string = String::cast(script->source());
  int line = 0;
  int line_start = 0;
  int len = source_string->length();
  for (int pos = 0; pos <= len; ++pos) {
    if (pos == len || source_string->Get(pos) == '\n') {
      if (position <= pos) {
        info->line = line;
        info->column = position - line_start;
        info->line_start = line_start;
        info->line_end = pos;
        return true;
      }
      line++;
      line_start = pos + 1;
    }
  }
  return false;
}
}  // namespace

#define SMI_VALUE(x) (Smi::cast(x)->value())
bool Script::GetPositionInfo(int position, PositionInfo* info,
                             OffsetFlag offset_flag) const {
  DisallowHeapAllocation no_allocation;

  // For wasm, we do not rely on the line_ends array, but do the translation
  // directly.
  if (type() == Script::TYPE_WASM) {
    Handle<WasmCompiledModule> compiled_module(
        WasmCompiledModule::cast(wasm_compiled_module()));
    DCHECK_LE(0, position);
    return compiled_module->GetPositionInfo(static_cast<uint32_t>(position),
                                            info);
  }

  if (line_ends()->IsUndefined(GetIsolate())) {
    // Slow mode: we do not have line_ends. We have to iterate through source.
    if (!GetPositionInfoSlow(this, position, info)) return false;
  } else {
    DCHECK(line_ends()->IsFixedArray());
    FixedArray* ends = FixedArray::cast(line_ends());

    const int ends_len = ends->length();
    if (ends_len == 0) return false;

    // Return early on invalid positions. Negative positions behave as if 0 was
    // passed, and positions beyond the end of the script return as failure.
    if (position < 0) {
      position = 0;
    } else if (position > SMI_VALUE(ends->get(ends_len - 1))) {
      return false;
    }

    // Determine line number by doing a binary search on the line ends array.
    if (SMI_VALUE(ends->get(0)) >= position) {
      info->line = 0;
      info->line_start = 0;
      info->column = position;
    } else {
      int left = 0;
      int right = ends_len - 1;

      while (right > 0) {
        DCHECK_LE(left, right);
        const int mid = (left + right) / 2;
        if (position > SMI_VALUE(ends->get(mid))) {
          left = mid + 1;
        } else if (position <= SMI_VALUE(ends->get(mid - 1))) {
          right = mid - 1;
        } else {
          info->line = mid;
          break;
        }
      }
      DCHECK(SMI_VALUE(ends->get(info->line)) >= position &&
             SMI_VALUE(ends->get(info->line - 1)) < position);
      info->line_start = SMI_VALUE(ends->get(info->line - 1)) + 1;
      info->column = position - info->line_start;
    }

    // Line end is position of the linebreak character.
    info->line_end = SMI_VALUE(ends->get(info->line));
    if (info->line_end > 0) {
      DCHECK(source()->IsString());
      String* src = String::cast(source());
      if (src->length() >= info->line_end &&
          src->Get(info->line_end - 1) == '\r') {
        info->line_end--;
      }
    }
  }

  // Add offsets if requested.
  if (offset_flag == WITH_OFFSET) {
    if (info->line == 0) {
      info->column += column_offset();
    }
    info->line += line_offset();
  }

  return true;
}
#undef SMI_VALUE

int Script::GetColumnNumber(Handle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info, WITH_OFFSET);
  return info.column;
}

int Script::GetColumnNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info, WITH_OFFSET);
  return info.column;
}

int Script::GetLineNumber(Handle<Script> script, int code_pos) {
  PositionInfo info;
  GetPositionInfo(script, code_pos, &info, WITH_OFFSET);
  return info.line;
}

int Script::GetLineNumber(int code_pos) const {
  PositionInfo info;
  GetPositionInfo(code_pos, &info, WITH_OFFSET);
  return info.line;
}

Object* Script::GetNameOrSourceURL() {
  Isolate* isolate = GetIsolate();
  // Keep in sync with ScriptNameOrSourceURL in messages.js.
  if (!source_url()->IsUndefined(isolate)) return source_url();
  return name();
}


Handle<JSObject> Script::GetWrapper(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  if (!script->wrapper()->IsUndefined(isolate)) {
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
    Isolate* isolate, const FunctionLiteral* fun) {
  DCHECK_NE(fun->function_literal_id(), FunctionLiteral::kIdTypeInvalid);
  DCHECK_LT(fun->function_literal_id(), shared_function_infos()->length());
  Object* shared = shared_function_infos()->get(fun->function_literal_id());
  if (shared->IsUndefined(isolate) || WeakCell::cast(shared)->cleared()) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  return handle(SharedFunctionInfo::cast(WeakCell::cast(shared)->value()));
}

Script::Iterator::Iterator(Isolate* isolate)
    : iterator_(isolate->heap()->script_list()) {}


Script* Script::Iterator::Next() { return iterator_.Next<Script>(); }

bool Script::HasPreparsedScopeData() const {
  return preparsed_scope_data()->length() > 0;
}

SharedFunctionInfo::ScriptIterator::ScriptIterator(Handle<Script> script)
    : ScriptIterator(script->GetIsolate(),
                     handle(script->shared_function_infos())) {}

SharedFunctionInfo::ScriptIterator::ScriptIterator(
    Isolate* isolate, Handle<FixedArray> shared_function_infos)
    : isolate_(isolate),
      shared_function_infos_(shared_function_infos),
      index_(0) {}

SharedFunctionInfo* SharedFunctionInfo::ScriptIterator::Next() {
  while (index_ < shared_function_infos_->length()) {
    Object* raw = shared_function_infos_->get(index_++);
    if (raw->IsUndefined(isolate_) || WeakCell::cast(raw)->cleared()) continue;
    return SharedFunctionInfo::cast(WeakCell::cast(raw)->value());
  }
  return nullptr;
}

void SharedFunctionInfo::ScriptIterator::Reset(Handle<Script> script) {
  shared_function_infos_ = handle(script->shared_function_infos());
  index_ = 0;
}

SharedFunctionInfo::GlobalIterator::GlobalIterator(Isolate* isolate)
    : script_iterator_(isolate),
      noscript_sfi_iterator_(isolate->heap()->noscript_shared_function_infos()),
      sfi_iterator_(handle(script_iterator_.Next(), isolate)) {}

SharedFunctionInfo* SharedFunctionInfo::GlobalIterator::Next() {
  SharedFunctionInfo* next = noscript_sfi_iterator_.Next<SharedFunctionInfo>();
  if (next != nullptr) return next;
  for (;;) {
    next = sfi_iterator_.Next();
    if (next != nullptr) return next;
    Script* next_script = script_iterator_.Next();
    if (next_script == nullptr) return nullptr;
    sfi_iterator_.Reset(handle(next_script));
  }
}


void SharedFunctionInfo::SetScript(Handle<SharedFunctionInfo> shared,
                                   Handle<Object> script_object) {
  DCHECK_NE(shared->function_literal_id(), FunctionLiteral::kIdTypeInvalid);
  if (shared->script() == *script_object) return;
  Isolate* isolate = shared->GetIsolate();

  // Add shared function info to new script's list. If a collection occurs,
  // the shared function info may be temporarily in two lists.
  // This is okay because the gc-time processing of these lists can tolerate
  // duplicates.
  if (script_object->IsScript()) {
    Handle<Script> script = Handle<Script>::cast(script_object);
    Handle<FixedArray> list = handle(script->shared_function_infos(), isolate);
#ifdef DEBUG
    DCHECK_LT(shared->function_literal_id(), list->length());
    if (list->get(shared->function_literal_id())->IsWeakCell() &&
        !WeakCell::cast(list->get(shared->function_literal_id()))->cleared()) {
      DCHECK(
          WeakCell::cast(list->get(shared->function_literal_id()))->value() ==
          *shared);
    }
#endif
    Handle<WeakCell> cell = isolate->factory()->NewWeakCell(shared);
    list->set(shared->function_literal_id(), *cell);
  } else {
    Handle<Object> list = isolate->factory()->noscript_shared_function_infos();

#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      WeakFixedArray::Iterator iterator(*list);
      SharedFunctionInfo* next;
      while ((next = iterator.Next<SharedFunctionInfo>()) != nullptr) {
        DCHECK_NE(next, *shared);
      }
    }
#endif  // DEBUG

    list = WeakFixedArray::Add(list, shared);

    isolate->heap()->SetRootNoScriptSharedFunctionInfos(*list);
  }

  if (shared->script()->IsScript()) {
    // Remove shared function info from old script's list.
    Script* old_script = Script::cast(shared->script());

    // Due to liveedit, it might happen that the old_script doesn't know
    // about the SharedFunctionInfo, so we have to guard against that.
    Handle<FixedArray> infos(old_script->shared_function_infos(), isolate);
    if (shared->function_literal_id() < infos->length()) {
      Object* raw = old_script->shared_function_infos()->get(
          shared->function_literal_id());
      if (!raw->IsWeakCell() || WeakCell::cast(raw)->value() == *shared) {
        old_script->shared_function_infos()->set(
            shared->function_literal_id(), isolate->heap()->undefined_value());
      }
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

bool SharedFunctionInfo::HasNoSideEffect() {
  if (!computed_has_no_side_effect()) {
    DisallowHeapAllocation not_handlified;
    Handle<SharedFunctionInfo> info(this);
    set_has_no_side_effect(DebugEvaluate::FunctionHasNoSideEffect(info));
    set_computed_has_no_side_effect(true);
  }
  return has_no_side_effect();
}

// The filter is a pattern that matches function names in this way:
//   "*"      all; the default
//   "-"      all but the top-level function
//   "-name"  all but the function "name"
//   ""       only the top-level function
//   "name"   only the function "name"
//   "name*"  only functions starting with "name"
//   "~"      none; the tilde is not an identifier
bool SharedFunctionInfo::PassesFilter(const char* raw_filter) {
  if (*raw_filter == '*') return true;
  String* name = DebugName();
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

bool SharedFunctionInfo::HasSourceCode() const {
  Isolate* isolate = GetIsolate();
  return !script()->IsUndefined(isolate) &&
         !reinterpret_cast<Script*>(script())->source()->IsUndefined(isolate);
}


Handle<Object> SharedFunctionInfo::GetSourceCode() {
  if (!HasSourceCode()) return GetIsolate()->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(script())->source()));
  return GetIsolate()->factory()->NewSubString(
      source, start_position(), end_position());
}

Handle<Object> SharedFunctionInfo::GetSourceCodeHarmony() {
  Isolate* isolate = GetIsolate();
  if (!HasSourceCode()) return isolate->factory()->undefined_value();
  Handle<String> script_source(String::cast(Script::cast(script())->source()));
  int start_pos = function_token_position();
  if (start_pos == kNoSourcePosition) start_pos = start_position();
  return isolate->factory()->NewSubString(script_source, start_pos,
                                          end_position());
}

bool SharedFunctionInfo::IsInlineable() {
  // Check that the function has a script associated with it.
  if (!script()->IsScript()) return false;
  if (GetIsolate()->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    return false;
  }
  return !optimization_disabled();
}

int SharedFunctionInfo::SourceSize() {
  return end_position() - start_position();
}

void JSFunction::CalculateInstanceSizeHelper(InstanceType instance_type,
                                             int requested_embedder_fields,
                                             int requested_in_object_properties,
                                             int* instance_size,
                                             int* in_object_properties) {
  int header_size = JSObject::GetHeaderSize(instance_type);
  DCHECK_LE(requested_embedder_fields,
            (JSObject::kMaxInstanceSize - header_size) >> kPointerSizeLog2);
  *instance_size =
      Min(header_size +
              ((requested_embedder_fields + requested_in_object_properties)
               << kPointerSizeLog2),
          JSObject::kMaxInstanceSize);
  *in_object_properties = ((*instance_size - header_size) >> kPointerSizeLog2) -
                          requested_embedder_fields;
}

void JSFunction::CalculateInstanceSizeForDerivedClass(
    Handle<JSFunction> function, InstanceType instance_type,
    int requested_embedder_fields, int* instance_size,
    int* in_object_properties) {
  Isolate* isolate = function->GetIsolate();
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, function, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (!current->IsJSFunction()) break;
    Handle<JSFunction> func(Handle<JSFunction>::cast(current));
    // The super constructor should be compiled for the number of expected
    // properties to be available.
    Handle<SharedFunctionInfo> shared(func->shared());
    if (shared->is_compiled() ||
        Compiler::Compile(func, Compiler::CLEAR_EXCEPTION)) {
      DCHECK(shared->is_compiled());
      expected_nof_properties += shared->expected_nof_properties();
    }
    if (!IsDerivedConstructor(shared->kind())) {
      break;
    }
  }
  CalculateInstanceSizeHelper(instance_type, requested_embedder_fields,
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
  DCHECK(abstract_code()->kind() == AbstractCode::FUNCTION ||
         abstract_code()->kind() == AbstractCode::INTERPRETED_FUNCTION ||
         abstract_code()->kind() == AbstractCode::BUILTIN);
  PROFILE(GetIsolate(), CodeDisableOptEvent(abstract_code(), this));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}

void SharedFunctionInfo::InitFromFunctionLiteral(
    Handle<SharedFunctionInfo> shared_info, FunctionLiteral* lit) {
  // When adding fields here, make sure DeclarationScope::AnalyzePartially is
  // updated accordingly.
  shared_info->set_internal_formal_parameter_count(lit->parameter_count());
  shared_info->set_function_token_position(lit->function_token_position());
  shared_info->set_start_position(lit->start_position());
  shared_info->set_end_position(lit->end_position());
  shared_info->set_is_declaration(lit->is_declaration());
  shared_info->set_is_named_expression(lit->is_named_expression());
  shared_info->set_is_anonymous_expression(lit->is_anonymous_expression());
  shared_info->set_inferred_name(*lit->inferred_name());
  shared_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  shared_info->set_language_mode(lit->language_mode());
  shared_info->set_uses_arguments(lit->scope()->arguments() != NULL);
  shared_info->set_kind(lit->kind());
  if (!IsConstructable(lit->kind())) {
    shared_info->SetConstructStub(
        *shared_info->GetIsolate()->builtins()->ConstructedNonConstructable());
  }
  shared_info->set_needs_home_object(lit->scope()->NeedsHomeObject());
  shared_info->set_asm_function(lit->scope()->asm_function());
  shared_info->set_function_literal_id(lit->function_literal_id());

  // For lazy parsed functions, the following flags will be inaccurate since we
  // don't have the information yet. They're set later in
  // SetSharedFunctionFlagsFromLiteral (compiler.cc), when the function is
  // really parsed and compiled.
  if (lit->body() != nullptr) {
    shared_info->set_length(lit->function_length());
    shared_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
    shared_info->SetExpectedNofPropertiesFromEstimate(lit);
  } else {
    // Set an invalid length for lazy functions. This way we can set the correct
    // value after compiling, but avoid overwriting values set manually by the
    // bootstrapper.
    shared_info->set_length(SharedFunctionInfo::kInvalidLength);
  }
}

void SharedFunctionInfo::SetExpectedNofPropertiesFromEstimate(
    FunctionLiteral* literal) {
  int estimate = literal->expected_property_count();

  // If no properties are added in the constructor, they are more likely
  // to be added later.
  if (estimate == 0) estimate = 2;

  // Inobject slack tracking will reclaim redundant inobject space later,
  // so we can afford to adjust the estimate generously.
  estimate += 8;

  set_expected_nof_properties(estimate);
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

void SharedFunctionInfo::SetConstructStub(Code* code) {
  if (code->kind() == Code::BUILTIN) code->set_is_construct_stub(true);
  set_construct_stub(code);
}

void Map::StartInobjectSlackTracking() {
  DCHECK(!IsInobjectSlackTrackingInProgress());
  if (unused_property_fields() == 0) return;
  set_construction_counter(Map::kSlackTrackingCounterStart);
}


void SharedFunctionInfo::ResetForNewContext(int new_ic_age) {
  code()->ClearInlineCaches();
  set_ic_age(new_ic_age);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_profiler_ticks(0);
    if (optimization_disabled() && deopt_count() >= FLAG_max_deopt_count) {
      // Re-enable optimizations if they were disabled due to deopt_count limit.
      set_optimization_disabled(false);
    }
    set_opt_count(0);
    set_deopt_count(0);
  } else if (IsInterpreted()) {
    set_profiler_ticks(0);
    if (optimization_disabled() && deopt_count() >= FLAG_max_deopt_count) {
      // Re-enable optimizations if they were disabled due to deopt_count limit.
      set_optimization_disabled(false);
    }
    set_opt_count(0);
    set_deopt_count(0);
  }
}

void ObjectVisitor::VisitCodeTarget(Code* host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeTarget(rinfo->rmode()));
  Object* old_pointer = Code::GetCodeFromTargetAddress(rinfo->target_address());
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitCodeAgeSequence(Code* host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsCodeAgeSequence(rinfo->rmode()));
  Object* old_pointer = rinfo->code_age_stub();
  Object* new_pointer = old_pointer;
  if (old_pointer != nullptr) {
    VisitPointer(host, &new_pointer);
    DCHECK_EQ(old_pointer, new_pointer);
  }
}

void ObjectVisitor::VisitCodeEntry(JSFunction* host, Address entry_address) {
  Object* old_pointer = Code::GetObjectFromEntryAddress(entry_address);
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitCellPointer(Code* host, RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::CELL);
  Object* old_pointer = rinfo->target_cell();
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitDebugTarget(Code* host, RelocInfo* rinfo) {
  DCHECK(RelocInfo::IsDebugBreakSlot(rinfo->rmode()) &&
         rinfo->IsPatchedDebugBreakSlotSequence());
  Object* old_pointer =
      Code::GetCodeFromTargetAddress(rinfo->debug_call_address());
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}

void ObjectVisitor::VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) {
  DCHECK(rinfo->rmode() == RelocInfo::EMBEDDED_OBJECT);
  Object* old_pointer = rinfo->target_object();
  Object* new_pointer = old_pointer;
  VisitPointer(host, &new_pointer);
  DCHECK_EQ(old_pointer, new_pointer);
}


void Code::InvalidateRelocation() {
  InvalidateEmbeddedObjects();
  set_relocation_info(GetHeap()->empty_byte_array());
}


void Code::InvalidateEmbeddedObjects() {
  HeapObject* undefined = GetHeap()->undefined_value();
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
  if (trap_handler::UseTrapHandler() && is_wasm_code()) {
    const int index = trap_handler_index()->value();
    if (index >= 0) {
      trap_handler::UpdateHandlerDataCodePointer(index, instruction_start());
    }
  }
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  Assembler::FlushICache(GetIsolate(), instruction_start(), instruction_size());
}


void Code::CopyFrom(const CodeDesc& desc) {
  // copy code
  CopyBytes(instruction_start(), desc.buffer,
            static_cast<size_t>(desc.instr_size));

  // copy unwinding info, if any
  if (desc.unwinding_info) {
    DCHECK_GT(desc.unwinding_info_size, 0);
    set_unwinding_info_size(desc.unwinding_info_size);
    CopyBytes(unwinding_info_start(), desc.unwinding_info,
              static_cast<size_t>(desc.unwinding_info_size));
  }

  // copy reloc info
  CopyBytes(relocation_start(),
            desc.buffer + desc.buffer_size - desc.reloc_size,
            static_cast<size_t>(desc.reloc_size));

  // unbox handles and relocate
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
      Handle<HeapObject> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(*p, UPDATE_WRITE_BARRIER,
                                    SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CELL) {
      Handle<Cell> cell  = it.rinfo()->target_cell_handle();
      it.rinfo()->set_target_cell(*cell, UPDATE_WRITE_BARRIER,
                                  SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles to direct pointers to the first instruction in the
      // code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(GetIsolate(), code->instruction_start(),
                                     UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (RelocInfo::IsRuntimeEntry(mode)) {
      Address p = it.rinfo()->target_runtime_entry(origin);
      it.rinfo()->set_target_runtime_entry(
          GetIsolate(), p, UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
    } else if (mode == RelocInfo::CODE_AGE_SEQUENCE) {
      Handle<Object> p = it.rinfo()->code_age_stub_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_code_age_stub(code, SKIP_ICACHE_FLUSH);
    } else {
      intptr_t delta = instruction_start() - desc.buffer;
      it.rinfo()->apply(delta);
    }
  }
  Assembler::FlushICache(GetIsolate(), instruction_start(), instruction_size());
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
    HeapObject* object = info->target_object();
    if (object->IsWeakCell()) {
      object = HeapObject::cast(WeakCell::cast(object)->value());
    }
    Map* map = object->map();
    if (map == *pattern.find_[current_pattern]) {
      info->set_target_object(*pattern.replace_[current_pattern]);
      if (++current_pattern == pattern.count_) return;
    }
  }
  UNREACHABLE();
}


void Code::ClearInlineCaches() {
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
             RelocInfo::ModeMask(RelocInfo::CODE_TARGET_WITH_ID);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Code* target(Code::GetCodeFromTargetAddress(info->target_address()));
    if (target->is_inline_cache_stub()) {
      ICUtility::Clear(this->GetIsolate(), info->pc(),
                       info->host()->constant_pool());
    }
  }
}

namespace {
template <typename Code>
void SetStackFrameCacheCommon(Handle<Code> code,
                              Handle<UnseededNumberDictionary> cache) {
  Handle<Object> maybe_table(code->source_position_table(), code->GetIsolate());
  if (maybe_table->IsSourcePositionTableWithFrameCache()) {
    Handle<SourcePositionTableWithFrameCache>::cast(maybe_table)
        ->set_stack_frame_cache(*cache);
    return;
  }
  DCHECK(maybe_table->IsByteArray());
  Handle<ByteArray> table(Handle<ByteArray>::cast(maybe_table));
  Handle<SourcePositionTableWithFrameCache> table_with_cache =
      code->GetIsolate()->factory()->NewSourcePositionTableWithFrameCache(
          table, cache);
  code->set_source_position_table(*table_with_cache);
}
}  // namespace

// static
void AbstractCode::SetStackFrameCache(Handle<AbstractCode> abstract_code,
                                      Handle<UnseededNumberDictionary> cache) {
  if (abstract_code->IsCode()) {
    SetStackFrameCacheCommon(handle(abstract_code->GetCode()), cache);
  } else {
    SetStackFrameCacheCommon(handle(abstract_code->GetBytecodeArray()), cache);
  }
}

namespace {
template <typename Code>
void DropStackFrameCacheCommon(Code* code) {
  i::Object* maybe_table = code->source_position_table();
  if (maybe_table->IsByteArray()) return;
  DCHECK(maybe_table->IsSourcePositionTableWithFrameCache());
  code->set_source_position_table(
      i::SourcePositionTableWithFrameCache::cast(maybe_table)
          ->source_position_table());
}
}  // namespace

void AbstractCode::DropStackFrameCache() {
  if (IsCode()) {
    DropStackFrameCacheCommon(GetCode());
  } else {
    DropStackFrameCacheCommon(GetBytecodeArray());
  }
}

int AbstractCode::SourcePosition(int offset) {
  int position = 0;
  // Subtract one because the current PC is one instruction after the call site.
  if (IsCode()) offset--;
  for (SourcePositionTableIterator iterator(source_position_table());
       !iterator.done() && iterator.code_offset() <= offset;
       iterator.Advance()) {
    position = iterator.source_position().ScriptOffset();
  }
  return position;
}

int AbstractCode::SourceStatementPosition(int offset) {
  // First find the closest position.
  int position = SourcePosition(offset);
  // Now find the closest statement position before the position.
  int statement_position = 0;
  for (SourcePositionTableIterator it(source_position_table()); !it.done();
       it.Advance()) {
    if (it.is_statement()) {
      int p = it.source_position().ScriptOffset();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
  }
  return statement_position;
}

void JSFunction::ClearTypeFeedbackInfo() {
  if (feedback_vector_cell()->value()->IsFeedbackVector()) {
    FeedbackVector* vector = feedback_vector();
    vector->ClearSlots(this);
  }
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
  PatchPlatformCodeAge(isolate, sequence, kNoAgeCodeAge);
}


void Code::MarkCodeAsExecuted(byte* sequence, Isolate* isolate) {
  PatchPlatformCodeAge(isolate, sequence, kExecutedOnceCodeAge);
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

void Code::PreAge(Isolate* isolate) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    PatchPlatformCodeAge(isolate, sequence, kPreAgedCodeAge);
  }
}

void Code::MarkToBeExecutedOnce(Isolate* isolate) {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    PatchPlatformCodeAge(isolate, sequence, kToBeExecutedOnceCodeAge);
  }
}

void Code::MakeOlder() {
  byte* sequence = FindCodeAgeSequence();
  if (sequence != NULL) {
    Isolate* isolate = GetIsolate();
    Age age = GetCodeAge(isolate, sequence);
    Age next_age = NextAge(age);
    if (age != next_age) {
      PatchPlatformCodeAge(isolate, sequence, next_age);
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
  return GetCodeAge(GetIsolate(), sequence);
}

Code::Age Code::GetAgeOfCodeAgeStub(Code* code) {
  Isolate* isolate = code->GetIsolate();
  Builtins* builtins = isolate->builtins();
#define HANDLE_CODE_AGE(AGE)                            \
  if (code == *builtins->Make##AGE##CodeYoungAgain()) { \
    return k##AGE##CodeAge;                             \
  }
  CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
  if (code == *builtins->MarkCodeAsExecutedOnce()) {
    return kNotExecutedCodeAge;
  }
  if (code == *builtins->MarkCodeAsExecutedTwice()) {
    return kExecutedOnceCodeAge;
  }
  if (code == *builtins->MarkCodeAsToBeExecutedOnce()) {
    return kToBeExecutedOnceCodeAge;
  }
  UNREACHABLE();
  return kNoAgeCodeAge;
}

Code* Code::GetCodeAgeStub(Isolate* isolate, Age age) {
  Builtins* builtins = isolate->builtins();
  switch (age) {
#define HANDLE_CODE_AGE(AGE)                       \
  case k##AGE##CodeAge: {                          \
    return *builtins->Make##AGE##CodeYoungAgain(); \
  }
    CODE_AGE_LIST(HANDLE_CODE_AGE)
#undef HANDLE_CODE_AGE
    case kNotExecutedCodeAge: {
      return *builtins->MarkCodeAsExecutedOnce();
    }
    case kExecutedOnceCodeAge: {
      return *builtins->MarkCodeAsExecutedTwice();
    }
    case kToBeExecutedOnceCodeAge: {
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
  if (info.deopt_reason != DeoptimizeReason::kNoReason || pos.IsKnown()) {
    if (FLAG_hydrogen_track_positions) {
      PrintF(out, "            ;;; deoptimize at %d_%d: %s\n", pos.InliningId(),
             pos.ScriptOffset(), DeoptimizeReasonToString(info.deopt_reason));
    } else {
      PrintF(out, "            ;;; deoptimize at ");
      OFStream outstr(out);
      pos.Print(outstr, this);
      PrintF(out, ", %s\n", DeoptimizeReasonToString(info.deopt_reason));
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

// Identify kind of code.
const char* AbstractCode::Kind2String(Kind kind) {
  if (kind < AbstractCode::INTERPRETED_FUNCTION)
    return Code::Kind2String((Code::Kind)kind);
  if (kind == AbstractCode::INTERPRETED_FUNCTION) return "INTERPRETED_FUNCTION";
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

#if defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

const char* Code::ICState2String(InlineCacheState state) {
  switch (state) {
    case UNINITIALIZED:
      return "UNINITIALIZED";
    case PREMONOMORPHIC:
      return "PREMONOMORPHIC";
    case MONOMORPHIC:
      return "MONOMORPHIC";
    case RECOMPUTE_HANDLER:
      return "RECOMPUTE_HANDLER";
    case POLYMORPHIC:
      return "POLYMORPHIC";
    case MEGAMORPHIC:
      return "MEGAMORPHIC";
    case GENERIC:
      return "GENERIC";
  }
  UNREACHABLE();
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

#endif  // defined(OBJECT_PRINT) || defined(ENABLE_DISASSEMBLER)

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

        case Translation::CONSTRUCT_STUB_FRAME: {
          int bailout_id = iterator.Next();
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{bailout_id=" << bailout_id << ", function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::COMPILED_STUB_FRAME: {
          Code::Kind stub_kind = static_cast<Code::Kind>(iterator.Next());
          os << "{kind=" << stub_kind << "}";
          break;
        }

        case Translation::ARGUMENTS_ADAPTOR_FRAME: {
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          unsigned height = iterator.Next();
          os << "{function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << ", height=" << height << "}";
          break;
        }

        case Translation::TAIL_CALLER_FRAME: {
          int shared_info_id = iterator.Next();
          Object* shared_info = LiteralArray()->get(shared_info_id);
          os << "{function="
             << Brief(SharedFunctionInfo::cast(shared_info)->DebugName())
             << "}";
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

        case Translation::FLOAT_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input="
             << RegisterConfiguration::Crankshaft()->GetFloatRegisterName(
                    reg_code)
             << "}";
          break;
        }

        case Translation::DOUBLE_REGISTER: {
          int reg_code = iterator.Next();
          os << "{input="
             << RegisterConfiguration::Crankshaft()->GetDoubleRegisterName(
                    reg_code)
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

        case Translation::FLOAT_STACK_SLOT:
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

        case Translation::ARGUMENTS_ELEMENTS:
        case Translation::ARGUMENTS_LENGTH: {
          bool is_rest = iterator.Next();
          os << "{is_rest=" << (is_rest ? "true" : "false") << "}";
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
       << Deoptimizer::BailoutStateToString(
              FullCodeGenerator::BailoutStateField::decode(pc_and_state))
       << "\n";
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


void Code::Disassemble(const char* name, std::ostream& os) {  // NOLINT
  os << "kind = " << Kind2String(kind()) << "\n";
  if (IsCodeStubOrIC()) {
    const char* n = CodeStub::MajorName(CodeStub::GetMajorKey(this));
    os << "major_key = " << (n == NULL ? "null" : n) << "\n";
  }
  if (is_inline_cache_stub()) {
    if (is_compare_ic_stub() || is_to_boolean_ic_stub() ||
        is_binary_op_stub()) {
      InlineCacheState ic_state = IC::StateFromCode(this);
      os << "ic_state = " << ICState2String(ic_state) << "\n";
      PrintExtraICState(os, kind(), extra_ic_state());
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
  if ((name != nullptr) && (name[0] != '\0')) {
    os << "name = " << name << "\n";
  } else if (kind() == BUILTIN) {
    name = GetIsolate()->builtins()->Lookup(instruction_start());
    if (name != nullptr) {
      os << "name = " << name << "\n";
    }
  } else if (kind() == BYTECODE_HANDLER) {
    name = GetIsolate()->interpreter()->LookupNameOfBytecodeHandler(this);
    if (name != nullptr) {
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

  SourcePositionTableIterator it(SourcePositionTable());
  if (!it.done()) {
    os << "Source positions:\n pc offset  position\n";
    for (; !it.done(); it.Advance()) {
      os << std::setw(10) << std::hex << it.code_offset() << std::dec
         << std::setw(10) << it.source_position().ScriptOffset()
         << (it.is_statement() ? "  statement" : "") << "\n";
    }
    os << "\n";
  }

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
      os << std::setw(4) << std::hex << pc_offset << std::dec << "  ";
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
           << std::setw(9) << std::hex << back_edges.pc_offset(i) << std::dec
           << "  " << std::setw(10) << back_edges.loop_depth(i) << "\n";
      }

      os << "\n";
    }
#ifdef OBJECT_PRINT
    if (!type_feedback_info()->IsUndefined(GetIsolate())) {
      TypeFeedbackInfo* info = TypeFeedbackInfo::cast(type_feedback_info());
      HeapObject::PrintHeader(os, "TypeFeedbackInfo");
      os << "\n - ic_total_count: " << info->ic_total_count()
         << ", ic_with_type_info_count: " << info->ic_with_type_info_count()
         << ", ic_generic_count: " << info->ic_generic_count() << "\n";
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

  if (has_unwinding_info()) {
    os << "UnwindingInfo (size = " << unwinding_info_size() << ")\n";
    EhFrameDisassembler eh_frame_disassembler(unwinding_info_start(),
                                              unwinding_info_end());
    eh_frame_disassembler.DisassembleToStream(os);
    os << "\n";
  }
}
#endif  // ENABLE_DISASSEMBLER


void BytecodeArray::Disassemble(std::ostream& os) {
  os << "Parameter count " << parameter_count() << "\n";
  os << "Frame size " << frame_size() << "\n";

  const uint8_t* base_address = GetFirstBytecodeAddress();
  SourcePositionTableIterator source_positions(SourcePositionTable());

  interpreter::BytecodeArrayIterator iterator(handle(this));
  while (!iterator.done()) {
    if (!source_positions.done() &&
        iterator.current_offset() == source_positions.code_offset()) {
      os << std::setw(5) << source_positions.source_position().ScriptOffset();
      os << (source_positions.is_statement() ? " S> " : " E> ");
      source_positions.Advance();
    } else {
      os << "         ";
    }
    const uint8_t* current_address = base_address + iterator.current_offset();
    os << reinterpret_cast<const void*>(current_address) << " @ "
       << std::setw(4) << iterator.current_offset() << " : ";
    interpreter::BytecodeDecoder::Decode(os, current_address,
                                         parameter_count());
    if (interpreter::Bytecodes::IsJump(iterator.current_bytecode())) {
      const void* jump_target = base_address + iterator.GetJumpTargetOffset();
      os << " (" << jump_target << " @ " << iterator.GetJumpTargetOffset()
         << ")";
    }
    if (interpreter::Bytecodes::IsSwitch(iterator.current_bytecode())) {
      os << " {";
      bool first_entry = true;
      for (const auto& entry : iterator.GetJumpTableTargetOffsets()) {
        if (first_entry) {
          first_entry = false;
        } else {
          os << ",";
        }
        os << " " << entry.case_value << ": @" << entry.target_offset;
      }
      os << " }";
    }
    os << std::endl;
    iterator.Advance();
  }

  os << "Constant pool (size = " << constant_pool()->length() << ")\n";
#ifdef OBJECT_PRINT
  if (constant_pool()->length() > 0) {
    constant_pool()->Print();
  }
#endif

  os << "Handler Table (size = " << handler_table()->Size() << ")\n";
#ifdef ENABLE_DISASSEMBLER
  if (handler_table()->length() > 0) {
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

void BytecodeArray::MakeOlder() {
  Age age = bytecode_age();
  if (age < kLastBytecodeAge) {
    set_bytecode_age(static_cast<Age>(age + 1));
  }
  DCHECK_GE(bytecode_age(), kFirstBytecodeAge);
  DCHECK_LE(bytecode_age(), kLastBytecodeAge);
}

bool BytecodeArray::IsOld() const {
  return bytecode_age() >= kIsOldBytecodeAge;
}

// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK(capacity >= 0);
  array->GetIsolate()->factory()->NewJSArrayStorage(
      array, length, capacity, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}

void JSArray::SetLength(Handle<JSArray> array, uint32_t new_length) {
  // We should never end in here with a pixel or external array.
  DCHECK(array->AllowsSetLength());
  if (array->SetLengthWouldNormalize(new_length)) {
    JSObject::NormalizeElements(array);
  }
  array->GetElementsAccessor()->SetLength(array, new_length);
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
  DisallowHeapAllocation no_allocation_scope;
  bool marked = MarkCodeForDeoptimization(isolate, group);
  if (marked) {
    DCHECK(AllowCodeDependencyChange::IsAllowed());
    Deoptimizer::DeoptimizeMarkedCode(isolate);
  }
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
    case kFieldOwnerGroup:
      return "field-owner";
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
  STACK_CHECK(isolate, Nothing<bool>());
  Handle<Name> trap_name = isolate->factory()->setPrototypeOf_string();
  // 1. Assert: Either Type(V) is Object or Type(V) is Null.
  DCHECK(value->IsJSReceiver() || value->IsNull(isolate));
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
  if (trap->IsUndefined(isolate)) {
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

#ifdef DEBUG
  int size = object->Size();
#endif

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

  Heap* heap = isolate->heap();
  // Silently ignore the change if value is not a JSObject or null.
  // SpiderMonkey behaves this way.
  if (!value->IsJSReceiver() && !value->IsNull(isolate)) return Just(true);

  bool all_extensible = object->map()->is_extensible();
  Handle<JSObject> real_receiver = object;
  if (from_javascript) {
    // Find the first object in the chain whose prototype object is not
    // hidden.
    PrototypeIterator iter(isolate, real_receiver, kStartAtPrototype,
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

  bool immutable_proto = map->is_immutable_proto();
  if (immutable_proto) {
    RETURN_FAILURE(
        isolate, should_throw,
        NewTypeError(MessageTemplate::kImmutablePrototypeSet, object));
  }

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
                                kStartAtReceiver);
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

  heap->ClearInstanceofCache();
  DCHECK(size == object->Size());
  return Just(true);
}

// static
void JSObject::SetImmutableProto(Handle<JSObject> object) {
  DCHECK(!object->IsAccessCheckNeeded());  // Never called from JS
  Handle<Map> map(object->map());

  // Nothing to do if prototype is already set.
  if (map->is_immutable_proto()) return;

  Handle<Map> new_map = Map::TransitionToImmutableProto(map);
  object->set_map(*new_map);
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
  // If the fast-case backing storage takes up much more memory than a
  // dictionary backing storage would, the object should have slow elements.
  int used_elements = object->GetFastElementsUsage();
  uint32_t size_threshold =
      SeededNumberDictionary::kPreferFastElementsSizeFactor *
      SeededNumberDictionary::ComputeCapacity(used_elements) *
      SeededNumberDictionary::kEntrySize;
  return size_threshold <= *new_capacity;
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
  } else if (object->IsJSSloppyArgumentsObject()) {
    return false;
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

  if (object->IsJSArray()) {
    CHECK(JSArray::cast(*object)->length()->ToArrayLength(&old_length));
  }

  ElementsKind kind = object->GetElementsKind();
  FixedArrayBase* elements = object->elements();
  ElementsKind dictionary_kind = DICTIONARY_ELEMENTS;
  if (IsSloppyArgumentsElementsKind(kind)) {
    elements = SloppyArgumentsElements::cast(elements)->arguments();
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
               : dictionary_kind;
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

  if (object->IsJSArray() && index >= old_length) {
    Handle<Object> new_length =
        isolate->factory()->NewNumberFromUint(index + 1);
    JSArray::cast(*object)->set_length(*new_length);
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

template <AllocationSiteUpdateMode update_or_check>
bool AllocationSite::DigestTransitionFeedback(Handle<AllocationSite> site,
                                              ElementsKind to_kind) {
  Isolate* isolate = site->GetIsolate();
  bool result = false;

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
        if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) {
          return true;
        }
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
        result = true;
      }
    }
  } else {
    ElementsKind kind = site->GetElementsKind();
    // if kind is holey ensure that to_kind is as well.
    if (IsHoleyElementsKind(kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
    }
    if (IsMoreGeneralElementsKindTransition(kind, to_kind)) {
      if (update_or_check == AllocationSiteUpdateMode::kCheckOnly) return true;
      if (FLAG_trace_track_allocation_sites) {
        PrintF("AllocationSite: JSArray %p site updated %s->%s\n",
               reinterpret_cast<void*>(*site),
               ElementsKindToString(kind),
               ElementsKindToString(to_kind));
      }
      site->SetElementsKind(to_kind);
      site->dependent_code()->DeoptimizeDependentCodeGroup(
          isolate, DependentCode::kAllocationSiteTransitionChangedGroup);
      result = true;
    }
  }
  return result;
}

AllocationSiteMode AllocationSite::GetMode(ElementsKind from, ElementsKind to) {
  if (IsFastSmiElementsKind(from) &&
      IsMoreGeneralElementsKindTransition(from, to)) {
    return TRACK_ALLOCATION_SITE;
  }

  return DONT_TRACK_ALLOCATION_SITE;
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

template <AllocationSiteUpdateMode update_or_check>
bool JSObject::UpdateAllocationSite(Handle<JSObject> object,
                                    ElementsKind to_kind) {
  if (!object->IsJSArray()) return false;

  Heap* heap = object->GetHeap();
  if (!heap->InNewSpace(*object)) return false;

  Handle<AllocationSite> site;
  {
    DisallowHeapAllocation no_allocation;

    AllocationMemento* memento =
        heap->FindAllocationMemento<Heap::kForRuntime>(*object);
    if (memento == NULL) return false;

    // Walk through to the Allocation Site
    site = handle(memento->GetAllocationSite());
  }
  return AllocationSite::DigestTransitionFeedback<update_or_check>(site,
                                                                   to_kind);
}

template bool
JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kCheckOnly>(
    Handle<JSObject> object, ElementsKind to_kind);

template bool JSObject::UpdateAllocationSite<AllocationSiteUpdateMode::kUpdate>(
    Handle<JSObject> object, ElementsKind to_kind);

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
  Map* map = array->map();
  // Fast path: "length" is the first fast property of arrays. Since it's not
  // configurable, it's guaranteed to be the first in the descriptor array.
  if (!map->is_dictionary_map()) {
    DCHECK(map->instance_descriptors()->GetKey(0) ==
           array->GetHeap()->length_string());
    return map->instance_descriptors()->GetDetails(0).IsReadOnly();
  }

  Isolate* isolate = array->GetIsolate();
  LookupIterator it(array, isolate->factory()->length_string(), array,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
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
  Isolate* isolate = store->GetIsolate();
  int limit = object->IsJSArray()
                  ? Smi::cast(JSArray::cast(object)->length())->value()
                  : store->length();
  int used = 0;
  for (int i = 0; i < limit; ++i) {
    if (!store->is_the_hole(isolate, i)) ++used;
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
      store = SloppyArgumentsElements::cast(store)->arguments();
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
  Isolate* isolate = this->GetIsolate();
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(isolate, k)) {
      os << "\n   ";
      if (k->IsString()) {
        String::cast(k)->StringPrint(os);
      } else {
        os << Brief(k);
      }
      os << ": " << Brief(this->ValueAt(i)) << " ";
      this->DetailsAt(i).PrintAsSlowTo(os);
    }
  }
}
template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::Print() {
  OFStream os(stdout);
  Print(os);
}
#endif


MaybeHandle<Object> JSObject::GetPropertyWithInterceptor(LookupIterator* it,
                                                         bool* done) {
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it->state());
  return GetPropertyWithInterceptorInternal(it, it->GetInterceptor(), done);
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
  LookupIterator it(isolate, object, index, object,
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

int FixedArrayBase::GetMaxLengthForNewSpaceAllocation(ElementsKind kind) {
  return ((kMaxRegularHeapObjectSize - FixedArrayBase::kHeaderSize) >>
          ElementsKindToShiftSize(kind));
}

bool JSObject::WasConstructedFromApiFunction() {
  auto instance_type = map()->instance_type();
  bool is_api_object = instance_type == JS_API_OBJECT_TYPE ||
                       instance_type == JS_SPECIAL_API_OBJECT_TYPE;
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    Object* maybe_constructor = map()->GetConstructor();
    if (maybe_constructor->IsJSFunction()) {
      JSFunction* constructor = JSFunction::cast(maybe_constructor);
      if (constructor->shared()->IsApiFunction()) {
        DCHECK(is_api_object);
      } else {
        DCHECK(!is_api_object);
      }
    } else if (maybe_constructor->IsFunctionTemplateInfo()) {
      DCHECK(is_api_object);
    } else {
      return false;
    }
  }
#endif
  return is_api_object;
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
  os << "<Symbol:";
  if (!name()->IsUndefined(GetIsolate())) {
    os << " ";
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(name())->StringShortPrint(&accumulator, false);
    os << accumulator.ToCString().get();
  } else {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << ">";
}


// StringSharedKeys are used as keys in the eval cache.
class StringSharedKey : public HashTableKey {
 public:
  // This tuple unambiguously identifies calls to eval() or
  // CreateDynamicFunction() (such as through the Function() constructor).
  // * source is the string passed into eval(). For dynamic functions, this is
  //   the effective source for the function, some of which is implicitly
  //   generated.
  // * shared is the shared function info for the function containing the call
  //   to eval(). for dynamic functions, shared is the native context closure.
  // * When positive, position is the position in the source where eval is
  //   called. When negative, position is the negation of the position in the
  //   dynamic function's effective source where the ')' ends the parameters.
  StringSharedKey(Handle<String> source, Handle<SharedFunctionInfo> shared,
                  LanguageMode language_mode, int position)
      : source_(source),
        shared_(shared),
        language_mode_(language_mode),
        position_(position) {}

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
    int position = Smi::cast(other_array->get(3))->value();
    if (position != position_) return false;
    String* source = String::cast(other_array->get(1));
    return source->Equals(*source_);
  }

  static uint32_t StringSharedHashHelper(String* source,
                                         SharedFunctionInfo* shared,
                                         LanguageMode language_mode,
                                         int position) {
    uint32_t hash = source->Hash();
    if (shared->HasSourceCode()) {
      // Instead of using the SharedFunctionInfo pointer in the hash
      // code computation, we use a combination of the hash of the
      // script source code and the start position of the calling scope.
      // We do this to ensure that the cache entries can survive garbage
      // collection.
      Script* script(Script::cast(shared->script()));
      hash ^= String::cast(script->source())->Hash();
      STATIC_ASSERT(LANGUAGE_END == 2);
      if (is_strict(language_mode)) hash ^= 0x8000;
      hash += position;
    }
    return hash;
  }

  uint32_t Hash() override {
    return StringSharedHashHelper(*source_, *shared_, language_mode_,
                                  position_);
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
    int position = Smi::cast(other_array->get(3))->value();
    return StringSharedHashHelper(source, shared, language_mode, position);
  }


  Handle<Object> AsHandle(Isolate* isolate) override {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromInt(language_mode_));
    array->set(3, Smi::FromInt(position_));
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int position_;
};

// static
const char* JSPromise::Status(int status) {
  switch (status) {
    case v8::Promise::kFulfilled:
      return "resolved";
    case v8::Promise::kPending:
      return "pending";
    case v8::Promise::kRejected:
      return "rejected";
  }
  UNREACHABLE();
  return NULL;
}

namespace {

JSRegExp::Flags RegExpFlagsFromString(Handle<String> flags, bool* success) {
  JSRegExp::Flags value = JSRegExp::kNone;
  int length = flags->length();
  // A longer flags string cannot be valid.
  if (length > JSRegExp::FlagCount()) return JSRegExp::Flags(0);
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
      case 's':
        if (FLAG_harmony_regexp_dotall) {
          flag = JSRegExp::kDotAll;
        } else {
          return JSRegExp::Flags(0);
        }
        break;
      case 'u':
        flag = JSRegExp::kUnicode;
        break;
      case 'y':
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
    if (src[i] == '\\') {
      // Escape. Skip next character;
      i++;
    } else if (src[i] == '/') {
      // Not escaped forward-slash needs escape.
      escapes++;
    }
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
    if (src[s] == '\\') {
      // Escape. Copy this and next character.
      dst[d++] = src[s++];
      if (s == src.length()) break;
    } else if (src[s] == '/') {
      // Not escaped forward-slash needs escape.
      dst[d++] = '\\';
    }
    dst[d++] = src[s++];
  }
  DCHECK_EQ(result->length(), d);
  return result;
}


MaybeHandle<String> EscapeRegExpSource(Isolate* isolate,
                                       Handle<String> source) {
  DCHECK(source->IsFlat());
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

  source = String::Flatten(source);

  Handle<String> escaped_source;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, escaped_source,
                             EscapeRegExpSource(isolate, source), JSRegExp);

  RETURN_ON_EXCEPTION(isolate, RegExpImpl::Compile(regexp, source, flags),
                      JSRegExp);

  regexp->set_source(*escaped_source);
  regexp->set_flags(Smi::FromInt(flags));

  Map* map = regexp->map();
  Object* constructor = map->GetConstructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex, Smi::kZero,
                                  SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    RETURN_ON_EXCEPTION(isolate, JSReceiver::SetProperty(
                                     regexp, factory->lastIndex_string(),
                                     Handle<Smi>(Smi::kZero, isolate), STRICT),
                        JSRegExp);
  }

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
      : string_(String::Flatten(string)) {}

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
    if (FLAG_thin_strings) {
      // External strings get special treatment, to avoid copying their
      // contents.
      if (string_->IsExternalOneByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalOneByteString>(string_);
      } else if (string_->IsExternalTwoByteString()) {
        return isolate->factory()
            ->InternalizeExternalString<ExternalTwoByteString>(string_);
      }
    }
    // Otherwise allocate a new internalized string.
    return isolate->factory()->NewInternalizedStringImpl(
        string_, string_->length(), string_->hash_field());
  }

  static uint32_t StringHash(Object* obj) {
    return String::cast(obj)->Hash();
  }

 private:
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
  DCHECK_IMPLIES(capacity_option == USE_CUSTOM_MINIMUM_CAPACITY,
                 base::bits::IsPowerOfTwo32(at_least_space_for));

  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid table size", true);
  }
  return New(isolate, capacity, pretenure);
}

template <typename Derived, typename Shape, typename Key>
Handle<Derived> HashTable<Derived, Shape, Key>::New(Isolate* isolate,
                                                    int capacity,
                                                    PretenureFlag pretenure) {
  Factory* factory = isolate->factory();
  int length = EntryToIndex(capacity);
  Handle<FixedArray> array = factory->NewFixedArray(length, pretenure);
  array->set_map_no_write_barrier(Shape::GetMap(isolate));
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
  Isolate* isolate = this->GetIsolate();
  while (true) {
    Object* element = this->KeyAt(entry);
    if (element->IsUndefined(isolate)) break;  // Empty entry.
    if (*key == element) return entry;
    DCHECK(element->IsTheHole(isolate) || element->IsUniqueName());
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
  Heap* heap = new_table->GetHeap();
  Object* the_hole = heap->the_hole_value();
  Object* undefined = heap->undefined_value();
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object* k = this->get(from_index);
    if (k != the_hole && k != undefined) {
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
  Isolate* isolate = GetIsolate();
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (uint32_t current = 0; current < capacity; current++) {
      Object* current_key = KeyAt(current);
      if (IsKey(isolate, current_key)) {
        uint32_t target = EntryForProbe(key, current_key, probe, current);
        if (current == target) continue;
        Object* target_key = KeyAt(target);
        if (!IsKey(isolate, target_key) ||
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
  Object* the_hole = isolate->heap()->the_hole_value();
  Object* undefined = isolate->heap()->undefined_value();
  for (uint32_t current = 0; current < capacity; current++) {
    if (KeyAt(current) == the_hole) {
      set(EntryToIndex(current) + Derived::kEntryKeyIndex, undefined);
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
  if (table->HasSufficientCapacityToAdd(n)) return table;

  Isolate* isolate = table->GetIsolate();
  int capacity = table->Capacity();
  int new_nof = table->NumberOfElements() + n;

  const int kMinCapacityForPretenure = 256;
  bool should_pretenure = pretenure == TENURED ||
      ((capacity > kMinCapacityForPretenure) &&
          !isolate->heap()->InNewSpace(*table));
  Handle<Derived> new_table =
      HashTable::New(isolate, new_nof, USE_DEFAULT_MINIMUM_CAPACITY,
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
  Isolate* isolate = GetIsolate();
  while (true) {
    Object* element = KeyAt(entry);
    if (!IsKey(isolate, element)) break;
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

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    HashTable<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>;

template class Dictionary<UnseededNumberDictionary,
                          UnseededNumberDictionaryShape,
                          uint32_t>;

template void
HashTable<GlobalDictionary, GlobalDictionaryShape, Handle<Name> >::Rehash(Handle<Name> key);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::New(
    Isolate*, int at_least_space_for, PretenureFlag pretenure,
    MinimumCapacity capacity_option);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
           uint32_t>::NewEmpty(Isolate*, PretenureFlag pretenure);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape,
           uint32_t>::NewEmpty(Isolate*, PretenureFlag pretenure);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape,
           uint32_t>::New(Isolate*, int at_least_space_for,
                          PretenureFlag pretenure,
                          MinimumCapacity capacity_option);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::New(
    Isolate*, int n, PretenureFlag pretenure, MinimumCapacity capacity_option);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::NewEmpty(
    Isolate*, PretenureFlag pretenure);

template Handle<GlobalDictionary>
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::New(
    Isolate*, int n, PretenureFlag pretenure, MinimumCapacity capacity_option);

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

template Handle<Object>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape,
           uint32_t>::DeleteProperty(Handle<UnseededNumberDictionary>, int);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    New(Isolate*, int, MinimumCapacity, PretenureFlag);

template Handle<ObjectHashSet> HashTable<ObjectHashSet, ObjectHashSetShape,
                                         Handle<Object>>::New(Isolate*, int n,
                                                              MinimumCapacity,
                                                              PretenureFlag);

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape, Handle<Name> >::
    Shrink(Handle<NameDictionary>, Handle<Name>);

template Handle<UnseededNumberDictionary>
    HashTable<UnseededNumberDictionary, UnseededNumberDictionaryShape,
              uint32_t>::Shrink(Handle<UnseededNumberDictionary>, uint32_t);

template Handle<NameDictionary>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::Add(
    Handle<NameDictionary>, Handle<Name>, Handle<Object>, PropertyDetails,
    int*);

template Handle<GlobalDictionary>
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::Add(
    Handle<GlobalDictionary>, Handle<Name>, Handle<Object>, PropertyDetails,
    int*);

template Handle<SeededNumberDictionary>
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape, uint32_t>::Add(
    Handle<SeededNumberDictionary>, uint32_t, Handle<Object>, PropertyDetails,
    int*);

template Handle<UnseededNumberDictionary>
Dictionary<UnseededNumberDictionary, UnseededNumberDictionaryShape,
           uint32_t>::Add(Handle<UnseededNumberDictionary>, uint32_t,
                          Handle<Object>, PropertyDetails, int*);

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

template int NameDictionaryBase<NameDictionary, NameDictionaryShape>::FindEntry(
    Handle<Name>);

template int Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::
    NumberOfElementsFilterAttributes(PropertyFilter filter);

template int Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::
    NumberOfElementsFilterAttributes(PropertyFilter filter);

template void
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::
    CopyEnumKeysTo(Handle<Dictionary<GlobalDictionary, GlobalDictionaryShape,
                                     Handle<Name>>>
                       dictionary,
                   Handle<FixedArray> storage, KeyCollectionMode mode,
                   KeyAccumulator* accumulator);

template void
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::CopyEnumKeysTo(
    Handle<Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>>
        dictionary,
    Handle<FixedArray> storage, KeyCollectionMode mode,
    KeyAccumulator* accumulator);

template Handle<FixedArray>
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::
    IterationIndices(
        Handle<
            Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>>
            dictionary);
template void
Dictionary<GlobalDictionary, GlobalDictionaryShape, Handle<Name>>::
    CollectKeysTo(Handle<Dictionary<GlobalDictionary, GlobalDictionaryShape,
                                    Handle<Name>>>
                      dictionary,
                  KeyAccumulator* keys);

template Handle<FixedArray>
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::IterationIndices(
    Handle<Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>>
        dictionary);
template void
Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>::CollectKeysTo(
    Handle<Dictionary<NameDictionary, NameDictionaryShape, Handle<Name>>>
        dictionary,
    KeyAccumulator* keys);

template int
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
           uint32_t>::AddEntry(Handle<SeededNumberDictionary> dictionary,
                               uint32_t key, Handle<Object> value,
                               PropertyDetails details, uint32_t hash);

template int
Dictionary<SeededNumberDictionary, SeededNumberDictionaryShape,
           uint32_t>::NumberOfElementsFilterAttributes(PropertyFilter filter);

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
    if (!dict->IsKey(isolate, k)) continue;

    DCHECK(k->IsNumber());
    DCHECK(!k->IsSmi() || Smi::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() >= 0);
    DCHECK(!k->IsHeapNumber() || HeapNumber::cast(k)->value() <= kMaxUInt32);

    HandleScope scope(isolate);
    Handle<Object> value(dict->ValueAt(i), isolate);
    PropertyDetails details = dict->DetailsAt(i);
    if (details.kind() == kAccessor || details.IsReadOnly()) {
      // Bail out and do the sorting of undefineds and array holes in JS.
      // Also bail out if the element is not supposed to be moved.
      return bailout;
    }

    uint32_t key = NumberToUint32(k);
    if (key < limit) {
      if (value->IsUndefined(isolate)) {
        undefs++;
      } else if (pos > static_cast<uint32_t>(Smi::kMaxValue)) {
        // Adding an entry with the key beyond smi-range requires
        // allocation. Bailout.
        return bailout;
      } else {
        Handle<Object> result = SeededNumberDictionary::AddNumberEntry(
            new_dict, pos, value, details, object);
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
          new_dict, key, value, details, object);
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
        object);
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
  if (object->HasSloppyArgumentsElements() || !object->map()->is_extensible()) {
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
    return handle(Smi::kZero, isolate);
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
      if (current->IsTheHole(isolate)) {
        holes--;
        undefs--;
      } else if (current->IsUndefined(isolate)) {
        undefs--;
      } else {
        continue;
      }
      // Position i needs to be filled.
      while (undefs > i) {
        current = elements->get(undefs);
        if (current->IsTheHole(isolate)) {
          holes--;
          undefs--;
        } else if (current->IsUndefined(isolate)) {
          undefs--;
        } else {
          elements->set(i, current, write_barrier);
          break;
        }
      }
    }
    result = undefs;
    while (undefs < holes) {
      elements->set_undefined(isolate, undefs);
      undefs++;
    }
    while (holes < limit) {
      elements->set_the_hole(isolate, holes);
      holes++;
    }
  }

  return isolate->factory()->NewNumberFromUint(result);
}

namespace {

bool CanonicalNumericIndexString(Isolate* isolate, Handle<Object> s,
                                 Handle<Object>* index) {
  DCHECK(s->IsString() || s->IsSmi());

  Handle<Object> result;
  if (s->IsSmi()) {
    result = s;
  } else {
    result = String::ToNumber(Handle<String>::cast(s));
    if (!result->IsMinusZero()) {
      Handle<String> str = Object::ToString(isolate, result).ToHandleChecked();
      // Avoid treating strings like "2E1" and "20" as the same key.
      if (!str->SameValue(*s)) return false;
    }
  }
  *index = result;
  return true;
}

}  // anonymous namespace

// ES#sec-integer-indexed-exotic-objects-defineownproperty-p-desc
// static
Maybe<bool> JSTypedArray::DefineOwnProperty(Isolate* isolate,
                                            Handle<JSTypedArray> o,
                                            Handle<Object> key,
                                            PropertyDescriptor* desc,
                                            ShouldThrow should_throw) {
  // 1. Assert: IsPropertyKey(P) is true.
  DCHECK(key->IsName() || key->IsNumber());
  // 2. Assert: O is an Object that has a [[ViewedArrayBuffer]] internal slot.
  // 3. If Type(P) is String, then
  if (key->IsString() || key->IsSmi()) {
    // 3a. Let numericIndex be ! CanonicalNumericIndexString(P)
    // 3b. If numericIndex is not undefined, then
    Handle<Object> numeric_index;
    if (CanonicalNumericIndexString(isolate, key, &numeric_index)) {
      // 3b i. If IsInteger(numericIndex) is false, return false.
      // 3b ii. If numericIndex = -0, return false.
      // 3b iii. If numericIndex < 0, return false.
      // FIXME: the standard allows up to 2^53 elements.
      uint32_t index;
      if (numeric_index->IsMinusZero() || !numeric_index->ToUint32(&index)) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kInvalidTypedArrayIndex));
      }
      // 3b iv. Let length be O.[[ArrayLength]].
      uint32_t length = o->length()->Number();
      // 3b v. If numericIndex ≥ length, return false.
      if (index >= length) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kInvalidTypedArrayIndex));
      }
      // 3b vi. If IsAccessorDescriptor(Desc) is true, return false.
      if (PropertyDescriptor::IsAccessorDescriptor(desc)) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kRedefineDisallowed, key));
      }
      // 3b vii. If Desc has a [[Configurable]] field and if
      //         Desc.[[Configurable]] is true, return false.
      // 3b viii. If Desc has an [[Enumerable]] field and if Desc.[[Enumerable]]
      //          is false, return false.
      // 3b ix. If Desc has a [[Writable]] field and if Desc.[[Writable]] is
      //        false, return false.
      if ((desc->has_configurable() && desc->configurable()) ||
          (desc->has_enumerable() && !desc->enumerable()) ||
          (desc->has_writable() && !desc->writable())) {
        RETURN_FAILURE(isolate, should_throw,
                       NewTypeError(MessageTemplate::kRedefineDisallowed, key));
      }
      // 3b x. If Desc has a [[Value]] field, then
      //   3b x 1. Let value be Desc.[[Value]].
      //   3b x 2. Return ? IntegerIndexedElementSet(O, numericIndex, value).
      if (desc->has_value()) {
        if (!desc->has_configurable()) desc->set_configurable(false);
        if (!desc->has_enumerable()) desc->set_enumerable(true);
        if (!desc->has_writable()) desc->set_writable(true);
        Handle<Object> value = desc->value();
        RETURN_ON_EXCEPTION_VALUE(isolate,
                                  SetOwnElementIgnoreAttributes(
                                      o, index, value, desc->ToAttributes()),
                                  Nothing<bool>());
      }
      // 3b xi. Return true.
      return Just(true);
    }
  }
  // 4. Return ! OrdinaryDefineOwnProperty(O, P, Desc).
  return OrdinaryDefineOwnProperty(isolate, o, key, desc, should_throw);
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

// static
MaybeHandle<JSTypedArray> JSTypedArray::Create(Isolate* isolate,
                                               Handle<Object> default_ctor,
                                               int argc, Handle<Object>* argv,
                                               const char* method_name) {
  // 1. Let newTypedArray be ? Construct(constructor, argumentList).
  Handle<Object> new_obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, new_obj,
                             Execution::New(isolate, default_ctor, argc, argv),
                             JSTypedArray);

  // 2. Perform ? ValidateTypedArray(newTypedArray).
  Handle<JSTypedArray> new_array;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, new_array, JSTypedArray::Validate(isolate, new_obj, method_name),
      JSTypedArray);

  // 3. If argumentList is a List of a single Number, then
  // If newTypedArray.[[ArrayLength]] < size, throw a TypeError exception.
  DCHECK_IMPLIES(argc == 1, argv[0]->IsSmi());
  if (argc == 1 && new_array->length_value() < argv[0]->Number()) {
    const MessageTemplate::Template message =
        MessageTemplate::kTypedArrayTooShort;
    THROW_NEW_ERROR(isolate, NewTypeError(message), JSTypedArray);
  }

  // 4. Return newTypedArray.
  return new_array;
}

// static
MaybeHandle<JSTypedArray> JSTypedArray::SpeciesCreate(
    Isolate* isolate, Handle<JSTypedArray> exemplar, int argc,
    Handle<Object>* argv, const char* method_name) {
  // 1. Assert: exemplar is an Object that has a [[TypedArrayName]] internal
  // slot.
  DCHECK(exemplar->IsJSTypedArray());

  // 2. Let defaultConstructor be the intrinsic object listed in column one of
  // Table 51 for exemplar.[[TypedArrayName]].
  Handle<JSFunction> default_ctor = isolate->uint8_array_fun();
  switch (exemplar->type()) {
#define TYPED_ARRAY_CTOR(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array: {                        \
    default_ctor = isolate->type##_array_fun();         \
    break;                                              \
  }

    TYPED_ARRAYS(TYPED_ARRAY_CTOR)
#undef TYPED_ARRAY_CTOR
    default:
      UNREACHABLE();
  }

  // 3. Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  Handle<Object> ctor;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor,
      Object::SpeciesConstructor(isolate, exemplar, default_ctor),
      JSTypedArray);

  // 4. Return ? TypedArrayCreate(constructor, argumentList).
  return Create(isolate, ctor, argc, argv, method_name);
}

void JSGlobalObject::InvalidatePropertyCell(Handle<JSGlobalObject> global,
                                            Handle<Name> name) {
  DCHECK(!global->HasFastProperties());
  auto dictionary = handle(global->global_dictionary());
  int entry = dictionary->FindEntry(name);
  if (entry == GlobalDictionary::kNotFound) return;
  PropertyCell::InvalidateEntry(dictionary, entry);
}

Handle<PropertyCell> JSGlobalObject::EnsureEmptyPropertyCell(
    Handle<JSGlobalObject> global, Handle<Name> name,
    PropertyCellType cell_type, int* entry_out) {
  Isolate* isolate = global->GetIsolate();
  DCHECK(!global->HasFastProperties());
  Handle<GlobalDictionary> dictionary(global->global_dictionary(), isolate);
  int entry = dictionary->FindEntry(name);
  Handle<PropertyCell> cell;
  if (entry != GlobalDictionary::kNotFound) {
    if (entry_out) *entry_out = entry;
    // This call should be idempotent.
    DCHECK(dictionary->ValueAt(entry)->IsPropertyCell());
    cell = handle(PropertyCell::cast(dictionary->ValueAt(entry)));
    PropertyCellType original_cell_type = cell->property_details().cell_type();
    DCHECK(original_cell_type == PropertyCellType::kInvalidated ||
           original_cell_type == PropertyCellType::kUninitialized);
    DCHECK(cell->value()->IsTheHole(isolate));
    if (original_cell_type == PropertyCellType::kInvalidated) {
      cell = PropertyCell::InvalidateEntry(dictionary, entry);
    }
    PropertyDetails details(kData, NONE, 0, cell_type);
    cell->set_property_details(details);
    return cell;
  }
  cell = isolate->factory()->NewPropertyCell();
  PropertyDetails details(kData, NONE, 0, cell_type);
  dictionary =
      GlobalDictionary::Add(dictionary, name, cell, details, entry_out);
  // {*entry_out} is initialized inside GlobalDictionary::Add().
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
  if (string->IsThinString()) {
    return handle(Handle<ThinString>::cast(string)->actual(), isolate);
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
  InternalizedStringKey dummy_key(isolate->factory()->empty_string());
  table = StringTable::EnsureCapacity(table, expected, &dummy_key);
  isolate->heap()->SetRootStringTable(*table);
}

namespace {

template <class StringClass>
void MigrateExternalStringResource(Isolate* isolate, String* from, String* to) {
  StringClass* cast_from = StringClass::cast(from);
  StringClass* cast_to = StringClass::cast(to);
  const typename StringClass::Resource* to_resource = cast_to->resource();
  if (to_resource == nullptr) {
    // |to| is a just-created internalized copy of |from|. Migrate the resource.
    cast_to->set_resource(cast_from->resource());
    // Zap |from|'s resource pointer to reflect the fact that |from| has
    // relinquished ownership of its resource.
    cast_from->set_resource(nullptr);
  } else if (to_resource != cast_from->resource()) {
    // |to| already existed and has its own resource. Finalize |from|.
    isolate->heap()->FinalizeExternalString(from);
  }
}

void MakeStringThin(String* string, String* internalized, Isolate* isolate) {
  if (string->IsExternalString()) {
    if (internalized->IsExternalOneByteString()) {
      MigrateExternalStringResource<ExternalOneByteString>(isolate, string,
                                                           internalized);
    } else if (internalized->IsExternalTwoByteString()) {
      MigrateExternalStringResource<ExternalTwoByteString>(isolate, string,
                                                           internalized);
    } else {
      // If the external string is duped into an existing non-external
      // internalized string, free its resource (it's about to be rewritten
      // into a ThinString below).
      isolate->heap()->FinalizeExternalString(string);
    }
  }

  if (!string->IsInternalizedString()) {
    DisallowHeapAllocation no_gc;
    isolate->heap()->NotifyObjectLayoutChange(string, no_gc);
    bool one_byte = internalized->IsOneByteRepresentation();
    Handle<Map> map = one_byte ? isolate->factory()->thin_one_byte_string_map()
                               : isolate->factory()->thin_string_map();
    int old_size = string->Size();
    DCHECK(old_size >= ThinString::kSize);
    string->synchronized_set_map(*map);
    ThinString* thin = ThinString::cast(string);
    thin->set_actual(internalized);
    Address thin_end = thin->address() + ThinString::kSize;
    int size_delta = old_size - ThinString::kSize;
    if (size_delta != 0) {
      Heap* heap = isolate->heap();
      heap->CreateFillerObjectAt(thin_end, size_delta, ClearRecordedSlots::kNo);
      heap->AdjustLiveBytes(thin, -size_delta);
    }
  }
}

}  // namespace

Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  if (string->IsThinString()) {
    DCHECK(Handle<ThinString>::cast(string)->actual()->IsInternalizedString());
    return handle(Handle<ThinString>::cast(string)->actual(), isolate);
  }
  if (string->IsConsString() && string->IsFlat()) {
    string = handle(Handle<ConsString>::cast(string)->first(), isolate);
    if (string->IsInternalizedString()) return string;
  }

  InternalizedStringKey key(string);
  Handle<String> result = LookupKey(isolate, &key);

  if (FLAG_thin_strings) {
    MakeStringThin(*string, *result, isolate);
  } else {  // !FLAG_thin_strings
    if (string->IsConsString()) {
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(isolate->heap()->empty_string());
    } else if (string->IsSlicedString()) {
      STATIC_ASSERT(ConsString::kSize == SlicedString::kSize);
      DisallowHeapAllocation no_gc;
      bool one_byte = result->IsOneByteRepresentation();
      Handle<Map> map = one_byte
                            ? isolate->factory()->cons_one_byte_string_map()
                            : isolate->factory()->cons_string_map();
      string->set_map(*map);
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(isolate->heap()->empty_string());
    }
  }
  return result;
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

namespace {

class StringTableNoAllocateKey : public HashTableKey {
 public:
  StringTableNoAllocateKey(String* string, uint32_t seed)
      : string_(string), length_(string->length()) {
    StringShape shape(string);
    one_byte_ = shape.HasOnlyOneByteChars();
    DCHECK(!shape.IsInternalized());
    DCHECK(!shape.IsThin());
    if (shape.IsCons() && length_ <= String::kMaxHashCalcLength) {
      special_flattening_ = true;
      uint32_t hash_field = 0;
      if (one_byte_) {
        one_byte_content_ = new uint8_t[length_];
        String::WriteToFlat(string, one_byte_content_, 0, length_);
        hash_field = StringHasher::HashSequentialString(one_byte_content_,
                                                        length_, seed);
      } else {
        two_byte_content_ = new uint16_t[length_];
        String::WriteToFlat(string, two_byte_content_, 0, length_);
        hash_field = StringHasher::HashSequentialString(two_byte_content_,
                                                        length_, seed);
      }
      string->set_hash_field(hash_field);
    } else {
      special_flattening_ = false;
      one_byte_content_ = nullptr;
    }
    hash_ = string->Hash();
  }

  ~StringTableNoAllocateKey() {
    if (one_byte_) {
      delete[] one_byte_content_;
    } else {
      delete[] two_byte_content_;
    }
  }

  bool IsMatch(Object* otherstring) override {
    String* other = String::cast(otherstring);
    DCHECK(other->IsInternalizedString());
    DCHECK(other->IsFlat());
    if (hash_ != other->Hash()) return false;
    int len = length_;
    if (len != other->length()) return false;

    if (!special_flattening_) {
      if (string_->Get(0) != other->Get(0)) return false;
      if (string_->IsFlat()) {
        StringShape shape1(string_);
        StringShape shape2(other);
        if (shape1.encoding_tag() == kOneByteStringTag &&
            shape2.encoding_tag() == kOneByteStringTag) {
          String::FlatContent flat1 = string_->GetFlatContent();
          String::FlatContent flat2 = other->GetFlatContent();
          return CompareRawStringContents(flat1.ToOneByteVector().start(),
                                          flat2.ToOneByteVector().start(), len);
        }
        if (shape1.encoding_tag() == kTwoByteStringTag &&
            shape2.encoding_tag() == kTwoByteStringTag) {
          String::FlatContent flat1 = string_->GetFlatContent();
          String::FlatContent flat2 = other->GetFlatContent();
          return CompareRawStringContents(flat1.ToUC16Vector().start(),
                                          flat2.ToUC16Vector().start(), len);
        }
      }
      StringComparator comparator;
      return comparator.Equals(string_, other);
    }

    String::FlatContent flat_content = other->GetFlatContent();
    if (one_byte_) {
      if (flat_content.IsOneByte()) {
        return CompareRawStringContents(
            one_byte_content_, flat_content.ToOneByteVector().start(), len);
      } else {
        DCHECK(flat_content.IsTwoByte());
        for (int i = 0; i < len; i++) {
          if (flat_content.Get(i) != one_byte_content_[i]) return false;
        }
        return true;
      }
    } else {
      if (flat_content.IsTwoByte()) {
        return CompareRawStringContents(
            two_byte_content_, flat_content.ToUC16Vector().start(), len);
      } else {
        DCHECK(flat_content.IsOneByte());
        for (int i = 0; i < len; i++) {
          if (flat_content.Get(i) != two_byte_content_[i]) return false;
        }
        return true;
      }
    }
  }

  uint32_t Hash() override { return hash_; }

  uint32_t HashForObject(Object* key) override {
    return String::cast(key)->Hash();
  }

  MUST_USE_RESULT Handle<Object> AsHandle(Isolate* isolate) override {
    UNREACHABLE();
    return Handle<String>();
  }

 private:
  String* string_;
  int length_;
  bool one_byte_;
  bool special_flattening_;
  uint32_t hash_ = 0;
  union {
    uint8_t* one_byte_content_;
    uint16_t* two_byte_content_;
  };
};

}  // namespace

// static
Object* StringTable::LookupStringIfExists_NoAllocate(String* string) {
  DisallowHeapAllocation no_gc;
  Heap* heap = string->GetHeap();
  Isolate* isolate = heap->isolate();
  StringTable* table = heap->string_table();

  StringTableNoAllocateKey key(string, heap->HashSeed());

  // String could be an array index.
  DCHECK(string->HasHashCode());
  uint32_t hash = string->hash_field();

  // Valid array indices are >= 0, so they cannot be mixed up with any of
  // the result sentinels, which are negative.
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kUnsupported));
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kNotFound));

  if ((hash & Name::kContainsCachedArrayIndexMask) == 0) {
    return Smi::FromInt(String::ArrayIndexValueBits::decode(hash));
  }
  if ((hash & Name::kIsNotArrayIndexMask) == 0) {
    // It is an indexed, but it's not cached.
    return Smi::FromInt(ResultSentinel::kUnsupported);
  }

  int entry = table->FindEntry(isolate, &key, key.Hash());
  if (entry != kNotFound) {
    String* internalized = String::cast(table->KeyAt(entry));
    if (FLAG_thin_strings) {
      MakeStringThin(string, internalized, isolate);
    }
    return internalized;
  }
  // A string that's not an array index, and not in the string table,
  // cannot have been used as a property name before.
  return Smi::FromInt(ResultSentinel::kNotFound);
}

String* StringTable::LookupKeyIfExists(Isolate* isolate, HashTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(isolate, key);
  if (entry != kNotFound) return String::cast(table->KeyAt(entry));
  return NULL;
}

Handle<StringSet> StringSet::New(Isolate* isolate) {
  return HashTable::New(isolate, 0);
}

Handle<StringSet> StringSet::Add(Handle<StringSet> stringset,
                                 Handle<String> name) {
  if (!stringset->Has(name)) {
    stringset = EnsureCapacity(stringset, 1, *name);
    uint32_t hash = StringSetShape::Hash(*name);
    int entry = stringset->FindInsertionEntry(hash);
    stringset->set(EntryToIndex(entry), *name);
    stringset->ElementAdded();
  }
  return stringset;
}

bool StringSet::Has(Handle<String> name) {
  return FindEntry(*name) != kNotFound;
}

Handle<ObjectHashSet> ObjectHashSet::Add(Handle<ObjectHashSet> set,
                                         Handle<Object> key) {
  Isolate* isolate = set->GetIsolate();
  int32_t hash = Object::GetOrCreateHash(isolate, key)->value();

  if (!set->Has(isolate, key, hash)) {
    set = EnsureCapacity(set, 1, key);
    int entry = set->FindInsertionEntry(hash);
    set->set(EntryToIndex(entry), *key);
    set->ElementAdded();
  }
  return set;
}

Handle<Object> CompilationCacheTable::Lookup(Handle<String> src,
                                             Handle<Context> context,
                                             LanguageMode language_mode) {
  Isolate* isolate = GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return isolate->factory()->undefined_value();
  return Handle<Object>(get(index + 1), isolate);
}

namespace {

const int kLiteralEntryLength = 2;
const int kLiteralInitialLength = 2;
const int kLiteralContextOffset = 0;
const int kLiteralLiteralsOffset = 1;

int SearchLiteralsMapEntry(CompilationCacheTable* cache, int cache_entry,
                           Context* native_context) {
  DisallowHeapAllocation no_gc;
  DCHECK(native_context->IsNativeContext());
  Object* obj = cache->get(cache_entry);

  if (obj->IsFixedArray()) {
    FixedArray* literals_map = FixedArray::cast(obj);
    int length = literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (WeakCell::cast(literals_map->get(i + kLiteralContextOffset))
              ->value() == native_context) {
        return i;
      }
    }
  }
  return -1;
}

void AddToLiteralsMap(Handle<CompilationCacheTable> cache, int cache_entry,
                      Handle<Context> native_context, Handle<Cell> literals) {
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kLiteralEntryLength == 2);
  Handle<FixedArray> new_literals_map;
  int entry;

  Object* obj = cache->get(cache_entry);

  if (!obj->IsFixedArray() || FixedArray::cast(obj)->length() == 0) {
    new_literals_map =
        isolate->factory()->NewFixedArray(kLiteralInitialLength, TENURED);
    entry = 0;
  } else {
    Handle<FixedArray> old_literals_map(FixedArray::cast(obj), isolate);
    entry = SearchLiteralsMapEntry(*cache, cache_entry, *native_context);
    if (entry >= 0) {
      // Just set the code of the entry.
      Handle<WeakCell> literals_cell =
          isolate->factory()->NewWeakCell(literals);
      old_literals_map->set(entry + kLiteralLiteralsOffset, *literals_cell);
      return;
    }

    // Can we reuse an entry?
    DCHECK(entry < 0);
    int length = old_literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (WeakCell::cast(old_literals_map->get(i + kLiteralContextOffset))
              ->cleared()) {
        new_literals_map = old_literals_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_literals_map = isolate->factory()->CopyFixedArrayAndGrow(
          old_literals_map, kLiteralEntryLength, TENURED);
      entry = old_literals_map->length();
    }
  }

  Handle<WeakCell> literals_cell = isolate->factory()->NewWeakCell(literals);
  WeakCell* context_cell = native_context->self_weak_cell();

  new_literals_map->set(entry + kLiteralContextOffset, context_cell);
  new_literals_map->set(entry + kLiteralLiteralsOffset, *literals_cell);

#ifdef DEBUG
  for (int i = 0; i < new_literals_map->length(); i += kLiteralEntryLength) {
    WeakCell* cell =
        WeakCell::cast(new_literals_map->get(i + kLiteralContextOffset));
    DCHECK(cell->cleared() || cell->value()->IsNativeContext());
    cell = WeakCell::cast(new_literals_map->get(i + kLiteralLiteralsOffset));
    DCHECK(cell->cleared() || (cell->value()->IsCell()));
  }
#endif

  Object* old_literals_map = cache->get(cache_entry);
  if (old_literals_map != *new_literals_map) {
    cache->set(cache_entry, *new_literals_map);
  }
}

Cell* SearchLiteralsMap(CompilationCacheTable* cache, int cache_entry,
                        Context* native_context) {
  Cell* result = nullptr;
  int entry = SearchLiteralsMapEntry(cache, cache_entry, native_context);
  if (entry >= 0) {
    FixedArray* literals_map = FixedArray::cast(cache->get(cache_entry));
    DCHECK_LE(entry + kLiteralEntryLength, literals_map->length());
    WeakCell* cell =
        WeakCell::cast(literals_map->get(entry + kLiteralLiteralsOffset));

    result = cell->cleared() ? nullptr : Cell::cast(cell->value());
  }
  DCHECK(result == nullptr || result->IsCell());
  return result;
}

}  // namespace

InfoVectorPair CompilationCacheTable::LookupScript(Handle<String> src,
                                                   Handle<Context> context,
                                                   LanguageMode language_mode) {
  InfoVectorPair empty_result;
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return empty_result;
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return empty_result;
  Object* obj = get(index + 1);
  if (obj->IsSharedFunctionInfo()) {
    Cell* literals =
        SearchLiteralsMap(this, index + 2, context->native_context());
    return InfoVectorPair(SharedFunctionInfo::cast(obj), literals);
  }
  return empty_result;
}

InfoVectorPair CompilationCacheTable::LookupEval(
    Handle<String> src, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> native_context, LanguageMode language_mode, int position) {
  InfoVectorPair empty_result;
  StringSharedKey key(src, outer_info, language_mode, position);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return empty_result;
  int index = EntryToIndex(entry);
  if (!get(index)->IsFixedArray()) return empty_result;
  Object* obj = get(EntryToIndex(entry) + 1);
  if (obj->IsSharedFunctionInfo()) {
    Cell* literals =
        SearchLiteralsMap(this, EntryToIndex(entry) + 2, *native_context);
    return InfoVectorPair(SharedFunctionInfo::cast(obj), literals);
  }
  return empty_result;
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
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  Handle<Object> k = key.AsHandle(isolate);
  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutScript(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> context, LanguageMode language_mode,
    Handle<SharedFunctionInfo> value, Handle<Cell> literals) {
  Isolate* isolate = cache->GetIsolate();
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<Context> native_context(context->native_context());
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  Handle<Object> k = key.AsHandle(isolate);
  cache = EnsureCapacity(cache, 1, &key);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  AddToLiteralsMap(cache, EntryToIndex(entry) + 2, native_context, literals);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    Handle<Context> native_context, Handle<Cell> literals, int position) {
  Isolate* isolate = cache->GetIsolate();
  StringSharedKey key(src, outer_info, value->language_mode(), position);
  {
    Handle<Object> k = key.AsHandle(isolate);
    int entry = cache->FindEntry(&key);
    if (entry != kNotFound) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      // AddToLiteralsMap may allocate a new sub-array to live in the entry,
      // but it won't change the cache array. Therefore EntryToIndex and
      // entry remains correct.
      AddToLiteralsMap(cache, EntryToIndex(entry) + 2, native_context,
                       literals);
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
      bool is_old =
          info->IsInterpreted()
              ? info->bytecode_array()->IsOld()
              : info->code()->kind() != Code::FUNCTION || info->code()->IsOld();
      if (is_old) {
        for (int i = 0; i < kEntrySize; i++) {
          NoWriteBarrierSet(this, entry_index + i, the_hole_value);
        }
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
      for (int i = 0; i < kEntrySize; i++) {
        NoWriteBarrierSet(this, entry_index + i, the_hole_value);
      }
      ElementRemoved();
    }
  }
  return;
}

template <typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::New(
    Isolate* isolate, int at_least_space_for, PretenureFlag pretenure,
    MinimumCapacity capacity_option) {
  DCHECK(0 <= at_least_space_for);
  Handle<Derived> dict = DerivedHashTable::New(isolate, at_least_space_for,
                                               capacity_option, pretenure);

  // Initialize the next enumeration index.
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
}

template <typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::NewEmpty(
    Isolate* isolate, PretenureFlag pretenure) {
  Handle<Derived> dict = DerivedHashTable::New(isolate, 1, pretenure);
  // Attempt to add one element to the empty dictionary must cause reallocation.
  DCHECK(!dict->HasSufficientCapacityToAdd(1));
  // Initialize the next enumeration index.
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
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
    int length = dictionary->NumberOfElements();

    Handle<FixedArray> iteration_order = IterationIndices(dictionary);
    DCHECK_EQ(length, iteration_order->length());

    // Iterate over the dictionary using the enumeration order and update
    // the dictionary with new enumeration indices.
    for (int i = 0; i < length; i++) {
      int index = Smi::cast(iteration_order->get(i))->value();
      DCHECK(dictionary->IsKey(dictionary->GetIsolate(),
                               dictionary->KeyAt(index)));

      int enum_index = PropertyDetails::kInitialIndex + i;

      PropertyDetails details = dictionary->DetailsAt(index);
      PropertyDetails new_details = details.set_index(enum_index);
      dictionary->DetailsAtPut(index, new_details);
    }

    // Set the next enumeration index.
    dictionary->SetNextEnumerationIndex(PropertyDetails::kInitialIndex +
                                        length);
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

template <typename Derived, typename Shape, typename Key>
Handle<Derived> Dictionary<Derived, Shape, Key>::Add(Handle<Derived> dictionary,
                                                     Key key,
                                                     Handle<Object> value,
                                                     PropertyDetails details,
                                                     int* entry_out) {
  // Valdate key is absent.
  SLOW_DCHECK((dictionary->FindEntry(key) == Dictionary::kNotFound));
  // Check whether the dictionary should be extended.
  dictionary = EnsureCapacity(dictionary, 1, key);

  int entry = AddEntry(dictionary, key, value, details, dictionary->Hash(key));
  if (entry_out) *entry_out = entry;
  return dictionary;
}

// Add a key, value pair to the dictionary. Returns entry value.
template <typename Derived, typename Shape, typename Key>
int Dictionary<Derived, Shape, Key>::AddEntry(Handle<Derived> dictionary,
                                              Key key, Handle<Object> value,
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
  return entry;
}

bool SeededNumberDictionary::HasComplexElements() {
  if (!requires_slow_elements()) return false;
  Isolate* isolate = this->GetIsolate();
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (!this->IsKey(isolate, k)) continue;
    DCHECK(!IsDeleted(i));
    PropertyDetails details = this->DetailsAt(i);
    if (details.kind() == kAccessor) return true;
    PropertyAttributes attr = details.attributes();
    if (attr & ALL_ATTRIBUTES_MASK) return true;
  }
  return false;
}

void SeededNumberDictionary::UpdateMaxNumberKey(
    uint32_t key, Handle<JSObject> dictionary_holder) {
  DisallowHeapAllocation no_allocation;
  // If the dictionary requires slow elements an element has already
  // been added at a high index.
  if (requires_slow_elements()) return;
  // Check if this index is high enough that we should require slow
  // elements.
  if (key > kRequiresSlowElementsLimit) {
    if (!dictionary_holder.is_null()) {
      dictionary_holder->RequireSlowElements(this);
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
    Handle<Object> value, PropertyDetails details,
    Handle<JSObject> dictionary_holder) {
  dictionary->UpdateMaxNumberKey(key, dictionary_holder);
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

Handle<UnseededNumberDictionary> UnseededNumberDictionary::DeleteKey(
    Handle<UnseededNumberDictionary> dictionary, uint32_t key) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) return dictionary;

  Factory* factory = dictionary->GetIsolate()->factory();
  dictionary->SetEntry(entry, factory->the_hole_value(),
                       factory->the_hole_value());
  dictionary->ElementRemoved();
  return dictionary->Shrink(dictionary, key);
}

Handle<SeededNumberDictionary> SeededNumberDictionary::AtNumberPut(
    Handle<SeededNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, Handle<JSObject> dictionary_holder) {
  dictionary->UpdateMaxNumberKey(key, dictionary_holder);
  return AtPut(dictionary, key, value);
}

void SeededNumberDictionary::CopyValuesTo(FixedArray* elements) {
  Isolate* isolate = this->GetIsolate();
  int pos = 0;
  int capacity = this->Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(isolate, k)) {
      elements->set(pos++, this->ValueAt(i), mode);
    }
  }
  DCHECK(pos == elements->length());
}

Handle<UnseededNumberDictionary> UnseededNumberDictionary::AtNumberPut(
    Handle<UnseededNumberDictionary> dictionary,
    uint32_t key,
    Handle<Object> value) {
  return AtPut(dictionary, key, value);
}

Handle<SeededNumberDictionary> SeededNumberDictionary::Set(
    Handle<SeededNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, PropertyDetails details,
    Handle<JSObject> dictionary_holder) {
  int entry = dictionary->FindEntry(key);
  if (entry == kNotFound) {
    return AddNumberEntry(dictionary, key, value, details, dictionary_holder);
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
  Isolate* isolate = this->GetIsolate();
  int capacity = this->Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (this->IsKey(isolate, k) && !k->FilterKey(filter)) {
      if (this->IsDeleted(i)) continue;
      PropertyDetails details = this->DetailsAt(i);
      PropertyAttributes attr = details.attributes();
      if ((attr & filter) == 0) result++;
    }
  }
  return result;
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
void Dictionary<Derived, Shape, Key>::CopyEnumKeysTo(
    Handle<Dictionary<Derived, Shape, Key>> dictionary,
    Handle<FixedArray> storage, KeyCollectionMode mode,
    KeyAccumulator* accumulator) {
  DCHECK_IMPLIES(mode != KeyCollectionMode::kOwnOnly, accumulator != nullptr);
  Isolate* isolate = dictionary->GetIsolate();
  int length = storage->length();
  int capacity = dictionary->Capacity();
  int properties = 0;
  for (int i = 0; i < capacity; i++) {
    Object* key = dictionary->KeyAt(i);
    bool is_shadowing_key = false;
    if (!dictionary->IsKey(isolate, key)) continue;
    if (key->IsSymbol()) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    if (details.IsDontEnum()) {
      if (mode == KeyCollectionMode::kIncludePrototypes) {
        is_shadowing_key = true;
      } else {
        continue;
      }
    }
    if (dictionary->IsDeleted(i)) continue;
    if (is_shadowing_key) {
      accumulator->AddShadowingKey(key);
      continue;
    } else {
      storage->set(properties, Smi::FromInt(i));
    }
    properties++;
    if (mode == KeyCollectionMode::kOwnOnly && properties == length) break;
  }

  CHECK_EQ(length, properties);
  DisallowHeapAllocation no_gc;
  Dictionary<Derived, Shape, Key>* raw_dictionary = *dictionary;
  FixedArray* raw_storage = *storage;
  EnumIndexComparator<Derived> cmp(static_cast<Derived*>(*dictionary));
  Smi** start = reinterpret_cast<Smi**>(storage->GetFirstElementAddress());
  std::sort(start, start + length, cmp);
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(raw_storage->get(i))->value();
    raw_storage->set(i, raw_dictionary->KeyAt(index));
  }
}

template <typename Derived, typename Shape, typename Key>
Handle<FixedArray> Dictionary<Derived, Shape, Key>::IterationIndices(
    Handle<Dictionary<Derived, Shape, Key>> dictionary) {
  Isolate* isolate = dictionary->GetIsolate();
  int capacity = dictionary->Capacity();
  int length = dictionary->NumberOfElements();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
  int array_size = 0;
  {
    DisallowHeapAllocation no_gc;
    Dictionary<Derived, Shape, Key>* raw_dict = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object* k = raw_dict->KeyAt(i);
      if (!raw_dict->IsKey(isolate, k)) continue;
      if (raw_dict->IsDeleted(i)) continue;
      array->set(array_size++, Smi::FromInt(i));
    }

    DCHECK_EQ(array_size, length);

    EnumIndexComparator<Derived> cmp(static_cast<Derived*>(raw_dict));
    Smi** start = reinterpret_cast<Smi**>(array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }
  array->Shrink(array_size);
  return array;
}

template <typename Derived, typename Shape, typename Key>
void Dictionary<Derived, Shape, Key>::CollectKeysTo(
    Handle<Dictionary<Derived, Shape, Key>> dictionary, KeyAccumulator* keys) {
  Isolate* isolate = keys->isolate();
  int capacity = dictionary->Capacity();
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(dictionary->NumberOfElements());
  int array_size = 0;
  PropertyFilter filter = keys->filter();
  {
    DisallowHeapAllocation no_gc;
    Dictionary<Derived, Shape, Key>* raw_dict = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object* k = raw_dict->KeyAt(i);
      if (!raw_dict->IsKey(isolate, k) || k->FilterKey(filter)) continue;
      if (raw_dict->IsDeleted(i)) continue;
      PropertyDetails details = raw_dict->DetailsAt(i);
      if ((details.attributes() & filter) != 0) {
        keys->AddShadowingKey(k);
        continue;
      }
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

  bool has_seen_symbol = false;
  for (int i = 0; i < array_size; i++) {
    int index = Smi::cast(array->get(i))->value();
    Object* key = dictionary->KeyAt(index);
    if (key->IsSymbol()) {
      has_seen_symbol = true;
      continue;
    }
    keys->AddKey(key, DO_NOT_CONVERT);
  }
  if (has_seen_symbol) {
    for (int i = 0; i < array_size; i++) {
      int index = Smi::cast(array->get(i))->value();
      Object* key = dictionary->KeyAt(index);
      if (!key->IsSymbol()) continue;
      keys->AddKey(key, DO_NOT_CONVERT);
    }
  }
}


// Backwards lookup (slow).
template<typename Derived, typename Shape, typename Key>
Object* Dictionary<Derived, Shape, Key>::SlowReverseLookup(Object* value) {
  Isolate* isolate = this->GetIsolate();
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = this->KeyAt(i);
    if (!this->IsKey(isolate, k)) continue;
    Object* e = this->ValueAt(i);
    // TODO(dcarney): this should be templatized.
    if (e->IsPropertyCell()) {
      e = PropertyCell::cast(e)->value();
    }
    if (e == value) return k;
  }
  return isolate->heap()->undefined_value();
}


Object* ObjectHashTable::Lookup(Isolate* isolate, Handle<Object> key,
                                int32_t hash) {
  DisallowHeapAllocation no_gc;
  DCHECK(IsKey(isolate, *key));

  int entry = FindEntry(isolate, key, hash);
  if (entry == kNotFound) return isolate->heap()->the_hole_value();
  return get(EntryToIndex(entry) + 1);
}


Object* ObjectHashTable::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;

  Isolate* isolate = GetIsolate();
  DCHECK(IsKey(isolate, *key));

  // If the object does not have an identity hash, it was never used as a key.
  Object* hash = key->GetHash();
  if (hash->IsUndefined(isolate)) {
    return isolate->heap()->the_hole_value();
  }
  return Lookup(isolate, key, Smi::cast(hash)->value());
}

Object* ObjectHashTable::ValueAt(int entry) {
  return get(EntryToValueIndex(entry));
}

Object* ObjectHashTable::Lookup(Handle<Object> key, int32_t hash) {
  return Lookup(GetIsolate(), key, hash);
}


Handle<ObjectHashTable> ObjectHashTable::Put(Handle<ObjectHashTable> table,
                                             Handle<Object> key,
                                             Handle<Object> value) {
  Isolate* isolate = table->GetIsolate();
  DCHECK(table->IsKey(isolate, *key));
  DCHECK(!value->IsTheHole(isolate));

  // Make sure the key object has an identity hash code.
  int32_t hash = Object::GetOrCreateHash(isolate, key)->value();

  return Put(table, key, value, hash);
}


Handle<ObjectHashTable> ObjectHashTable::Put(Handle<ObjectHashTable> table,
                                             Handle<Object> key,
                                             Handle<Object> value,
                                             int32_t hash) {
  Isolate* isolate = table->GetIsolate();
  DCHECK(table->IsKey(isolate, *key));
  DCHECK(!value->IsTheHole(isolate));

  int entry = table->FindEntry(isolate, key, hash);

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(EntryToIndex(entry) + 1, *value);
    return table;
  }

  // Rehash if more than 33% of the entries are deleted entries.
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
            Heap::kFinalizeIncrementalMarkingMask,
            GarbageCollectionReason::kFullHashtable);
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
  DCHECK(table->IsKey(table->GetIsolate(), *key));

  Object* hash = key->GetHash();
  if (hash->IsUndefined(table->GetIsolate())) {
    *was_present = false;
    return table;
  }

  return Remove(table, key, was_present, Smi::cast(hash)->value());
}


Handle<ObjectHashTable> ObjectHashTable::Remove(Handle<ObjectHashTable> table,
                                                Handle<Object> key,
                                                bool* was_present,
                                                int32_t hash) {
  Isolate* isolate = table->GetIsolate();
  DCHECK(table->IsKey(isolate, *key));

  int entry = table->FindEntry(isolate, key, hash);
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
  Isolate* isolate = GetIsolate();
  DCHECK(IsKey(isolate, *key));
  int entry = FindEntry(key);
  if (entry == kNotFound) return isolate->heap()->the_hole_value();
  return get(EntryToValueIndex(entry));
}


Handle<WeakHashTable> WeakHashTable::Put(Handle<WeakHashTable> table,
                                         Handle<HeapObject> key,
                                         Handle<HeapObject> value) {
  Isolate* isolate = key->GetIsolate();
  DCHECK(table->IsKey(isolate, *key));
  int entry = table->FindEntry(key);
  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(EntryToValueIndex(entry), *value);
    return table;
  }

  Handle<WeakCell> key_cell = isolate->factory()->NewWeakCell(key);

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

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Allocate(
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

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::EnsureGrowable(
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

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Shrink(
    Handle<Derived> table) {
  DCHECK(!table->IsObsolete());

  int nof = table->NumberOfElements();
  int capacity = table->Capacity();
  if (nof >= (capacity >> 2)) return table;
  return Rehash(table, capacity / 2);
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Clear(
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

template <class Derived, int entrysize>
bool OrderedHashTable<Derived, entrysize>::HasKey(Handle<Derived> table,
                                                  Handle<Object> key) {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = table->GetIsolate();
  Object* raw_key = *key;
  int entry = table->KeyToFirstEntry(isolate, raw_key);
  // Walk the chain in the bucket to find the key.
  while (entry != kNotFound) {
    Object* candidate_key = table->KeyAt(entry);
    if (candidate_key->SameValueZero(raw_key)) return true;
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

Handle<FixedArray> OrderedHashSet::ConvertToKeysArray(
    Handle<OrderedHashSet> table, GetKeysConversion convert) {
  Isolate* isolate = table->GetIsolate();
  int length = table->NumberOfElements();
  int nof_buckets = table->NumberOfBuckets();
  // Convert the dictionary to a linear list.
  Handle<FixedArray> result = Handle<FixedArray>::cast(table);
  // From this point on table is no longer a valid OrderedHashSet.
  result->set_map(isolate->heap()->fixed_array_map());
  for (int i = 0; i < length; i++) {
    int index = kHashTableStartIndex + nof_buckets + (i * kEntrySize);
    Object* key = table->get(index);
    if (convert == GetKeysConversion::kConvertToString) {
      uint32_t index_value;
      if (key->ToArrayIndex(&index_value)) {
        key = *isolate->factory()->Uint32ToString(index_value);
      } else {
        CHECK(key->IsName());
      }
    }
    result->set(i, key);
  }
  result->Shrink(length);
  return result;
}

template <class Derived, int entrysize>
Handle<Derived> OrderedHashTable<Derived, entrysize>::Rehash(
    Handle<Derived> table, int new_capacity) {
  Isolate* isolate = table->GetIsolate();
  DCHECK(!table->IsObsolete());

  Handle<Derived> new_table =
      Allocate(isolate, new_capacity,
               isolate->heap()->InNewSpace(*table) ? NOT_TENURED : TENURED);
  int nof = table->NumberOfElements();
  int nod = table->NumberOfDeletedElements();
  int new_buckets = new_table->NumberOfBuckets();
  int new_entry = 0;
  int removed_holes_index = 0;

  DisallowHeapAllocation no_gc;
  for (int old_entry = 0; old_entry < (nof + nod); ++old_entry) {
    Object* key = table->KeyAt(old_entry);
    if (key->IsTheHole(isolate)) {
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

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashSet> OrderedHashTable<
    OrderedHashSet, 1>::EnsureGrowable(Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Shrink(
    Handle<OrderedHashSet> table);

template Handle<OrderedHashSet> OrderedHashTable<OrderedHashSet, 1>::Clear(
    Handle<OrderedHashSet> table);

template bool OrderedHashTable<OrderedHashSet, 1>::HasKey(
    Handle<OrderedHashSet> table, Handle<Object> key);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Allocate(
    Isolate* isolate, int capacity, PretenureFlag pretenure);

template Handle<OrderedHashMap> OrderedHashTable<
    OrderedHashMap, 2>::EnsureGrowable(Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Shrink(
    Handle<OrderedHashMap> table);

template Handle<OrderedHashMap> OrderedHashTable<OrderedHashMap, 2>::Clear(
    Handle<OrderedHashMap> table);

template bool OrderedHashTable<OrderedHashMap, 2>::HasKey(
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
  Isolate* isolate = this->GetIsolate();
  if (this->table()->IsUndefined(isolate)) return false;

  Transition();

  TableType* table = TableType::cast(this->table());
  int index = Smi::cast(this->index())->value();
  int used_capacity = table->UsedCapacity();

  while (index < used_capacity && table->KeyAt(index)->IsTheHole(isolate)) {
    index++;
  }

  set_index(Smi::FromInt(index));

  if (index < used_capacity) return true;

  set_table(isolate->heap()->undefined_value());
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
  return Smi::kZero;
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
  DCHECK(table->IsKey(table->GetIsolate(), *key));
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
  DCHECK(table->IsKey(table->GetIsolate(), *key));
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

Handle<JSArray> JSWeakCollection::GetEntries(Handle<JSWeakCollection> holder,
                                             int max_entries) {
  Isolate* isolate = holder->GetIsolate();
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  if (max_entries == 0 || max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }
  int values_per_entry = holder->IsJSWeakMap() ? 2 : 1;
  Handle<FixedArray> entries =
      isolate->factory()->NewFixedArray(max_entries * values_per_entry);
  // Recompute max_values because GC could have removed elements from the table.
  if (max_entries > table->NumberOfElements()) {
    max_entries = table->NumberOfElements();
  }

  {
    DisallowHeapAllocation no_gc;
    int count = 0;
    for (int i = 0;
         count / values_per_entry < max_entries && i < table->Capacity(); i++) {
      Handle<Object> key(table->KeyAt(i), isolate);
      if (table->IsKey(isolate, *key)) {
        entries->set(count++, *key);
        if (values_per_entry > 1) {
          Object* value = table->Lookup(key);
          entries->set(count++, value);
        }
      }
    }
    DCHECK_EQ(max_entries * values_per_entry, count);
  }
  return isolate->factory()->NewJSArrayWithElements(entries);
}

// Check if there is a break point at this source position.
bool DebugInfo::HasBreakPoint(int source_position) {
  // Get the break point info object for this code offset.
  Object* break_point_info = GetBreakPointInfo(source_position);

  // If there is no break point info object or no break points in the break
  // point info object there is no break point at this code offset.
  if (break_point_info->IsUndefined(GetIsolate())) return false;
  return BreakPointInfo::cast(break_point_info)->GetBreakPointCount() > 0;
}

// Get the break point info object for this source position.
Object* DebugInfo::GetBreakPointInfo(int source_position) {
  Isolate* isolate = GetIsolate();
  if (!break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < break_points()->length(); i++) {
      if (!break_points()->get(i)->IsUndefined(isolate)) {
        BreakPointInfo* break_point_info =
            BreakPointInfo::cast(break_points()->get(i));
        if (break_point_info->source_position() == source_position) {
          return break_point_info;
        }
      }
    }
  }
  return isolate->heap()->undefined_value();
}

bool DebugInfo::ClearBreakPoint(Handle<DebugInfo> debug_info,
                                Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  if (debug_info->break_points()->IsUndefined(isolate)) return false;

  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) continue;
    Handle<BreakPointInfo> break_point_info = Handle<BreakPointInfo>(
        BreakPointInfo::cast(debug_info->break_points()->get(i)), isolate);
    if (BreakPointInfo::HasBreakPointObject(break_point_info,
                                            break_point_object)) {
      BreakPointInfo::ClearBreakPoint(break_point_info, break_point_object);
      return true;
    }
  }
  return false;
}

void DebugInfo::SetBreakPoint(Handle<DebugInfo> debug_info, int source_position,
                              Handle<Object> break_point_object) {
  Isolate* isolate = debug_info->GetIsolate();
  Handle<Object> break_point_info(
      debug_info->GetBreakPointInfo(source_position), isolate);
  if (!break_point_info->IsUndefined(isolate)) {
    BreakPointInfo::SetBreakPoint(
        Handle<BreakPointInfo>::cast(break_point_info),
        break_point_object);
    return;
  }

  // Adding a new break point for a code offset which did not have any
  // break points before. Try to find a free slot.
  static const int kNoBreakPointInfo = -1;
  int index = kNoBreakPointInfo;
  for (int i = 0; i < debug_info->break_points()->length(); i++) {
    if (debug_info->break_points()->get(i)->IsUndefined(isolate)) {
      index = i;
      break;
    }
  }
  if (index == kNoBreakPointInfo) {
    // No free slot - extend break point info array.
    Handle<FixedArray> old_break_points = Handle<FixedArray>(
        FixedArray::cast(debug_info->break_points()), isolate);
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
  Handle<BreakPointInfo> new_break_point_info =
      isolate->factory()->NewBreakPointInfo(source_position);
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}

// Get the break point objects for a source position.
Handle<Object> DebugInfo::GetBreakPointObjects(int source_position) {
  Object* break_point_info = GetBreakPointInfo(source_position);
  Isolate* isolate = GetIsolate();
  if (break_point_info->IsUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return Handle<Object>(
      BreakPointInfo::cast(break_point_info)->break_point_objects(), isolate);
}


// Get the total number of break points.
int DebugInfo::GetBreakPointCount() {
  Isolate* isolate = GetIsolate();
  if (break_points()->IsUndefined(isolate)) return 0;
  int count = 0;
  for (int i = 0; i < break_points()->length(); i++) {
    if (!break_points()->get(i)->IsUndefined(isolate)) {
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
  if (!debug_info->break_points()->IsUndefined(isolate)) {
    for (int i = 0; i < debug_info->break_points()->length(); i++) {
      if (!debug_info->break_points()->get(i)->IsUndefined(isolate)) {
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

// Remove the specified break point object.
void BreakPointInfo::ClearBreakPoint(Handle<BreakPointInfo> break_point_info,
                                     Handle<Object> break_point_object) {
  Isolate* isolate = break_point_info->GetIsolate();
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) return;
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
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
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
  Isolate* isolate = break_point_info->GetIsolate();
  if (break_point_info->break_point_objects()->IsUndefined(isolate)) {
    return false;
  }
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
  if (break_point_objects()->IsUndefined(GetIsolate())) return 0;
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

namespace {

Script* ScriptFromJSValue(Object* in) {
  DCHECK(in->IsJSValue());
  JSValue* jsvalue = JSValue::cast(in);
  DCHECK(jsvalue->value()->IsScript());
  return Script::cast(jsvalue->value());
}

}  // namespace

int JSMessageObject::GetLineNumber() const {
  if (start_position() == -1) return Message::kNoLineNumberInfo;

  Handle<Script> the_script = handle(ScriptFromJSValue(script()));

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return Message::kNoLineNumberInfo;
  }

  return info.line + 1;
}

int JSMessageObject::GetColumnNumber() const {
  if (start_position() == -1) return -1;

  Handle<Script> the_script = handle(ScriptFromJSValue(script()));

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return -1;
  }

  return info.column;  // Note: No '+1' in contrast to GetLineNumber.
}

Handle<String> JSMessageObject::GetSourceLine() const {
  Handle<Script> the_script = handle(ScriptFromJSValue(script()));

  Isolate* isolate = the_script->GetIsolate();
  if (the_script->type() == Script::TYPE_WASM) {
    return isolate->factory()->empty_string();
  }

  Script::PositionInfo info;
  const Script::OffsetFlag offset_flag = Script::WITH_OFFSET;
  if (!Script::GetPositionInfo(the_script, start_position(), &info,
                               offset_flag)) {
    return isolate->factory()->empty_string();
  }

  Handle<String> src = handle(String::cast(the_script->source()), isolate);
  return isolate->factory()->NewSubString(src, info.line_start, info.line_end);
}

void JSArrayBuffer::Neuter() {
  CHECK(is_neuterable());
  CHECK(is_external());
  set_backing_store(nullptr);
  set_byte_length(Smi::kZero);
  set_allocation_base(nullptr);
  set_allocation_length(0);
  set_was_neutered(true);
  // Invalidate the neutering protector.
  Isolate* const isolate = GetIsolate();
  if (isolate->IsArrayBufferNeuteringIntact()) {
    isolate->InvalidateArrayBufferNeuteringProtector();
  }
}

void JSArrayBuffer::FreeBackingStore() {
  if (allocation_base() == nullptr) {
    return;
  }
  using AllocationMode = ArrayBuffer::Allocator::AllocationMode;
  const size_t length = allocation_length();
  const AllocationMode mode = allocation_mode();
  GetIsolate()->array_buffer_allocator()->Free(allocation_base(), length, mode);

  // Zero out the backing store and allocation base to avoid dangling
  // pointers.
  set_backing_store(nullptr);
  // TODO(eholk): set_byte_length(0) once we aren't using Smis for the
  // byte_length. We can't do it now because the GC needs to call
  // FreeBackingStore while it is collecting.
  set_allocation_base(nullptr);
  set_allocation_length(0);
}

void JSArrayBuffer::Setup(Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
                          bool is_external, void* data, size_t allocated_length,
                          SharedFlag shared) {
  return Setup(array_buffer, isolate, is_external, data, allocated_length, data,
               allocated_length, shared);
}

void JSArrayBuffer::Setup(Handle<JSArrayBuffer> array_buffer, Isolate* isolate,
                          bool is_external, void* allocation_base,
                          size_t allocation_length, void* data,
                          size_t byte_length, SharedFlag shared) {
  DCHECK(array_buffer->GetEmbedderFieldCount() ==
         v8::ArrayBuffer::kEmbedderFieldCount);
  for (int i = 0; i < v8::ArrayBuffer::kEmbedderFieldCount; i++) {
    array_buffer->SetEmbedderField(i, Smi::kZero);
  }
  array_buffer->set_bit_field(0);
  array_buffer->set_is_external(is_external);
  array_buffer->set_is_neuterable(shared == SharedFlag::kNotShared);
  array_buffer->set_is_shared(shared == SharedFlag::kShared);

  Handle<Object> heap_byte_length =
      isolate->factory()->NewNumberFromSize(byte_length);
  CHECK(heap_byte_length->IsSmi() || heap_byte_length->IsHeapNumber());
  array_buffer->set_byte_length(*heap_byte_length);
  // Initialize backing store at last to avoid handling of |JSArrayBuffers| that
  // are currently being constructed in the |ArrayBufferTracker|. The
  // registration method below handles the case of registering a buffer that has
  // already been promoted.
  array_buffer->set_backing_store(data);

  array_buffer->set_allocation_base(data);
  array_buffer->set_allocation_length(allocation_length);

  if (data && !is_external) {
    isolate->heap()->RegisterNewArrayBuffer(*array_buffer);
  }
}

namespace {

inline int ConvertToMb(size_t size) {
  return static_cast<int>(size / static_cast<size_t>(MB));
}

}  // namespace

bool JSArrayBuffer::SetupAllocatingData(Handle<JSArrayBuffer> array_buffer,
                                        Isolate* isolate,
                                        size_t allocated_length,
                                        bool initialize, SharedFlag shared) {
  void* data;
  CHECK(isolate->array_buffer_allocator() != NULL);
  // Prevent creating array buffers when serializing.
  DCHECK(!isolate->serializer_enabled());
  if (allocated_length != 0) {
    if (allocated_length >= MB)
      isolate->counters()->array_buffer_big_allocations()->AddSample(
          ConvertToMb(allocated_length));
    if (initialize) {
      data = isolate->array_buffer_allocator()->Allocate(allocated_length);
    } else {
      data = isolate->array_buffer_allocator()->AllocateUninitialized(
          allocated_length);
    }
    if (data == NULL) {
      isolate->counters()->array_buffer_new_size_failures()->AddSample(
          ConvertToMb(allocated_length));
      return false;
    }
  } else {
    data = NULL;
  }

  const bool is_external = false;
  JSArrayBuffer::Setup(array_buffer, isolate, is_external, data,
                       allocated_length, shared);
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
  buffer->set_allocation_base(backing_store);
  buffer->set_allocation_length(NumberToSize(buffer->byte_length()));
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
  Handle<PropertyCell> new_cell = isolate->factory()->NewPropertyCell();
  new_cell->set_value(cell->value());
  dictionary->ValueAtPut(entry, *new_cell);
  bool is_the_hole = cell->value()->IsTheHole(isolate);
  // Cell is officially mutable henceforth.
  PropertyDetails details = cell->property_details();
  details = details.set_cell_type(is_the_hole ? PropertyCellType::kUninitialized
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
  Isolate* isolate = cell->GetIsolate();
  DCHECK(!value->IsTheHole(isolate));
  if (cell->value()->IsTheHole(isolate)) {
    switch (type) {
      // Only allow a cell to transition once into constant state.
      case PropertyCellType::kUninitialized:
        if (value->IsUndefined(isolate)) return PropertyCellType::kUndefined;
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

Handle<PropertyCell> PropertyCell::PrepareForValue(
    Handle<GlobalDictionary> dictionary, int entry, Handle<Object> value,
    PropertyDetails details) {
  Isolate* isolate = dictionary->GetIsolate();
  DCHECK(!value->IsTheHole(isolate));
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
  if (cell->value()->IsTheHole(isolate)) {
    index = dictionary->NextEnumerationIndex();
    dictionary->SetNextEnumerationIndex(index + 1);
  }
  DCHECK(index > 0);
  details = details.set_index(index);

  PropertyCellType new_type = UpdatedType(cell, value, original_details);
  if (invalidate) cell = PropertyCell::InvalidateEntry(dictionary, entry);

  // Install new property details.
  details = details.set_cell_type(new_type);
  cell->set_property_details(details);

  // Deopt when transitioning from a constant type.
  if (!invalidate && (old_type != new_type ||
                      original_details.IsReadOnly() != details.IsReadOnly())) {
    cell->dependent_code()->DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
  return cell;
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

int JSGeneratorObject::source_position() const {
  CHECK(is_suspended());
  DCHECK(function()->shared()->HasBytecodeArray());
  DCHECK(!function()->shared()->HasBaselineCode());

  int code_offset;
  const JSAsyncGeneratorObject* async =
      IsJSAsyncGeneratorObject() ? JSAsyncGeneratorObject::cast(this) : nullptr;
  if (async != nullptr && async->awaited_promise()->IsJSPromise()) {
    code_offset = Smi::cast(async->await_input_or_debug_pos())->value();
  } else {
    code_offset = Smi::cast(input_or_debug_pos())->value();
  }

  // The stored bytecode offset is relative to a different base than what
  // is used in the source position table, hence the subtraction.
  code_offset -= BytecodeArray::kHeaderSize - kHeapObjectTag;
  AbstractCode* code =
      AbstractCode::cast(function()->shared()->bytecode_array());
  return code->SourcePosition(code_offset);
}

// static
AccessCheckInfo* AccessCheckInfo::Get(Isolate* isolate,
                                      Handle<JSObject> receiver) {
  DisallowHeapAllocation no_gc;
  DCHECK(receiver->map()->is_access_check_needed());
  Object* maybe_constructor = receiver->map()->GetConstructor();
  if (maybe_constructor->IsFunctionTemplateInfo()) {
    Object* data_obj =
        FunctionTemplateInfo::cast(maybe_constructor)->access_check_info();
    if (data_obj->IsUndefined(isolate)) return nullptr;
    return AccessCheckInfo::cast(data_obj);
  }
  // Might happen for a detached context.
  if (!maybe_constructor->IsJSFunction()) return nullptr;
  JSFunction* constructor = JSFunction::cast(maybe_constructor);
  // Might happen for the debug context.
  if (!constructor->shared()->IsApiFunction()) return nullptr;

  Object* data_obj =
      constructor->shared()->get_api_func_data()->access_check_info();
  if (data_obj->IsUndefined(isolate)) return nullptr;

  return AccessCheckInfo::cast(data_obj);
}

bool JSReceiver::HasProxyInPrototype(Isolate* isolate) {
  for (PrototypeIterator iter(isolate, this, kStartAtReceiver,
                              PrototypeIterator::END_AT_NULL);
       !iter.IsAtEnd(); iter.AdvanceIgnoringProxies()) {
    if (iter.GetCurrent<Object>()->IsJSProxy()) return true;
  }
  return false;
}

MaybeHandle<Object> JSModuleNamespace::GetExport(Handle<String> name) {
  Isolate* isolate = name->GetIsolate();

  Handle<Object> object(module()->exports()->Lookup(name), isolate);
  if (object->IsTheHole(isolate)) {
    return isolate->factory()->undefined_value();
  }

  Handle<Object> value(Handle<Cell>::cast(object)->value(), isolate);
  if (value->IsTheHole(isolate)) {
    THROW_NEW_ERROR(
        isolate, NewReferenceError(MessageTemplate::kNotDefined, name), Object);
  }

  return value;
}

namespace {

struct ModuleHandleHash {
  V8_INLINE size_t operator()(Handle<Module> module) const {
    return module->hash();
  }
};

struct ModuleHandleEqual {
  V8_INLINE bool operator()(Handle<Module> lhs, Handle<Module> rhs) const {
    return *lhs == *rhs;
  }
};

struct StringHandleHash {
  V8_INLINE size_t operator()(Handle<String> string) const {
    return string->Hash();
  }
};

struct StringHandleEqual {
  V8_INLINE bool operator()(Handle<String> lhs, Handle<String> rhs) const {
    return lhs->Equals(*rhs);
  }
};

class UnorderedStringSet
    : public std::unordered_set<Handle<String>, StringHandleHash,
                                StringHandleEqual,
                                ZoneAllocator<Handle<String>>> {
 public:
  explicit UnorderedStringSet(Zone* zone)
      : std::unordered_set<Handle<String>, StringHandleHash, StringHandleEqual,
                           ZoneAllocator<Handle<String>>>(
            2 /* bucket count */, StringHandleHash(), StringHandleEqual(),
            ZoneAllocator<Handle<String>>(zone)) {}
};

class UnorderedModuleSet
    : public std::unordered_set<Handle<Module>, ModuleHandleHash,
                                ModuleHandleEqual,
                                ZoneAllocator<Handle<Module>>> {
 public:
  explicit UnorderedModuleSet(Zone* zone)
      : std::unordered_set<Handle<Module>, ModuleHandleHash, ModuleHandleEqual,
                           ZoneAllocator<Handle<Module>>>(
            2 /* bucket count */, ModuleHandleHash(), ModuleHandleEqual(),
            ZoneAllocator<Handle<Module>>(zone)) {}
};

class UnorderedStringMap
    : public std::unordered_map<
          Handle<String>, Handle<Object>, StringHandleHash, StringHandleEqual,
          ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>> {
 public:
  explicit UnorderedStringMap(Zone* zone)
      : std::unordered_map<
            Handle<String>, Handle<Object>, StringHandleHash, StringHandleEqual,
            ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>>(
            2 /* bucket count */, StringHandleHash(), StringHandleEqual(),
            ZoneAllocator<std::pair<const Handle<String>, Handle<Object>>>(
                zone)) {}
};

}  // anonymous namespace

class Module::ResolveSet
    : public std::unordered_map<
          Handle<Module>, UnorderedStringSet*, ModuleHandleHash,
          ModuleHandleEqual,
          ZoneAllocator<std::pair<const Handle<Module>, UnorderedStringSet*>>> {
 public:
  explicit ResolveSet(Zone* zone)
      : std::unordered_map<Handle<Module>, UnorderedStringSet*,
                           ModuleHandleHash, ModuleHandleEqual,
                           ZoneAllocator<std::pair<const Handle<Module>,
                                                   UnorderedStringSet*>>>(
            2 /* bucket count */, ModuleHandleHash(), ModuleHandleEqual(),
            ZoneAllocator<std::pair<const Handle<Module>, UnorderedStringSet*>>(
                zone)),
        zone_(zone) {}

  Zone* zone() const { return zone_; }

 private:
  Zone* zone_;
};

namespace {

int ExportIndex(int cell_index) {
  DCHECK_EQ(ModuleDescriptor::GetCellIndexKind(cell_index),
            ModuleDescriptor::kExport);
  return cell_index - 1;
}

int ImportIndex(int cell_index) {
  DCHECK_EQ(ModuleDescriptor::GetCellIndexKind(cell_index),
            ModuleDescriptor::kImport);
  return -cell_index - 1;
}

}  // anonymous namespace

void Module::CreateIndirectExport(Handle<Module> module, Handle<String> name,
                                  Handle<ModuleInfoEntry> entry) {
  Isolate* isolate = module->GetIsolate();
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  DCHECK(exports->Lookup(name)->IsTheHole(isolate));
  exports = ObjectHashTable::Put(exports, name, entry);
  module->set_exports(*exports);
}

void Module::CreateExport(Handle<Module> module, int cell_index,
                          Handle<FixedArray> names) {
  DCHECK_LT(0, names->length());
  Isolate* isolate = module->GetIsolate();

  Handle<Cell> cell =
      isolate->factory()->NewCell(isolate->factory()->undefined_value());
  module->regular_exports()->set(ExportIndex(cell_index), *cell);

  Handle<ObjectHashTable> exports(module->exports(), isolate);
  for (int i = 0, n = names->length(); i < n; ++i) {
    Handle<String> name(String::cast(names->get(i)), isolate);
    DCHECK(exports->Lookup(name)->IsTheHole(isolate));
    exports = ObjectHashTable::Put(exports, name, cell);
  }
  module->set_exports(*exports);
}

Cell* Module::GetCell(int cell_index) {
  DisallowHeapAllocation no_gc;
  Object* cell;
  switch (ModuleDescriptor::GetCellIndexKind(cell_index)) {
    case ModuleDescriptor::kImport:
      cell = regular_imports()->get(ImportIndex(cell_index));
      break;
    case ModuleDescriptor::kExport:
      cell = regular_exports()->get(ExportIndex(cell_index));
      break;
    case ModuleDescriptor::kInvalid:
      UNREACHABLE();
      cell = nullptr;
      break;
  }
  return Cell::cast(cell);
}

Handle<Object> Module::LoadVariable(Handle<Module> module, int cell_index) {
  Isolate* isolate = module->GetIsolate();
  return handle(module->GetCell(cell_index)->value(), isolate);
}

void Module::StoreVariable(Handle<Module> module, int cell_index,
                           Handle<Object> value) {
  DCHECK_EQ(ModuleDescriptor::GetCellIndexKind(cell_index),
            ModuleDescriptor::kExport);
  module->GetCell(cell_index)->set_value(*value);
}

MaybeHandle<Cell> Module::ResolveImport(Handle<Module> module,
                                        Handle<String> name, int module_request,
                                        MessageLocation loc, bool must_resolve,
                                        Module::ResolveSet* resolve_set) {
  Isolate* isolate = module->GetIsolate();
  Handle<Module> requested_module(
      Module::cast(module->requested_modules()->get(module_request)), isolate);
  return Module::ResolveExport(requested_module, name, loc, must_resolve,
                               resolve_set);
}

MaybeHandle<Cell> Module::ResolveExport(Handle<Module> module,
                                        Handle<String> name,
                                        MessageLocation loc, bool must_resolve,
                                        Module::ResolveSet* resolve_set) {
  DCHECK_EQ(module->status(), kPrepared);
  Isolate* isolate = module->GetIsolate();
  Handle<Object> object(module->exports()->Lookup(name), isolate);
  if (object->IsCell()) {
    // Already resolved (e.g. because it's a local export).
    return Handle<Cell>::cast(object);
  }

  // Check for cycle before recursing.
  {
    // Attempt insertion with a null string set.
    auto result = resolve_set->insert({module, nullptr});
    UnorderedStringSet*& name_set = result.first->second;
    if (result.second) {
      // |module| wasn't in the map previously, so allocate a new name set.
      Zone* zone = resolve_set->zone();
      name_set =
          new (zone->New(sizeof(UnorderedStringSet))) UnorderedStringSet(zone);
    } else if (name_set->count(name)) {
      // Cycle detected.
      if (must_resolve) {
        return isolate->Throw<Cell>(
            isolate->factory()->NewSyntaxError(
                MessageTemplate::kCyclicModuleDependency, name),
            &loc);
      }
      return MaybeHandle<Cell>();
    }
    name_set->insert(name);
  }

  if (object->IsModuleInfoEntry()) {
    // Not yet resolved indirect export.
    Handle<ModuleInfoEntry> entry = Handle<ModuleInfoEntry>::cast(object);
    Handle<String> import_name(String::cast(entry->import_name()), isolate);
    Handle<Script> script(
        Script::cast(JSFunction::cast(module->code())->shared()->script()),
        isolate);
    MessageLocation new_loc(script, entry->beg_pos(), entry->end_pos());

    Handle<Cell> cell;
    if (!ResolveImport(module, import_name, entry->module_request(), new_loc,
                       true, resolve_set)
             .ToHandle(&cell)) {
      DCHECK(isolate->has_pending_exception());
      return MaybeHandle<Cell>();
    }

    // The export table may have changed but the entry in question should be
    // unchanged.
    Handle<ObjectHashTable> exports(module->exports(), isolate);
    DCHECK(exports->Lookup(name)->IsModuleInfoEntry());

    exports = ObjectHashTable::Put(exports, name, cell);
    module->set_exports(*exports);
    return cell;
  }

  DCHECK(object->IsTheHole(isolate));
  return Module::ResolveExportUsingStarExports(module, name, loc, must_resolve,
                                               resolve_set);
}

MaybeHandle<Cell> Module::ResolveExportUsingStarExports(
    Handle<Module> module, Handle<String> name, MessageLocation loc,
    bool must_resolve, Module::ResolveSet* resolve_set) {
  Isolate* isolate = module->GetIsolate();
  if (!name->Equals(isolate->heap()->default_string())) {
    // Go through all star exports looking for the given name.  If multiple star
    // exports provide the name, make sure they all map it to the same cell.
    Handle<Cell> unique_cell;
    Handle<FixedArray> special_exports(module->info()->special_exports(),
                                       isolate);
    for (int i = 0, n = special_exports->length(); i < n; ++i) {
      i::Handle<i::ModuleInfoEntry> entry(
          i::ModuleInfoEntry::cast(special_exports->get(i)), isolate);
      if (!entry->export_name()->IsUndefined(isolate)) {
        continue;  // Indirect export.
      }

      Handle<Script> script(
          Script::cast(JSFunction::cast(module->code())->shared()->script()),
          isolate);
      MessageLocation new_loc(script, entry->beg_pos(), entry->end_pos());

      Handle<Cell> cell;
      if (ResolveImport(module, name, entry->module_request(), new_loc, false,
                        resolve_set)
              .ToHandle(&cell)) {
        if (unique_cell.is_null()) unique_cell = cell;
        if (*unique_cell != *cell) {
          return isolate->Throw<Cell>(
              isolate->factory()->NewSyntaxError(
                  MessageTemplate::kAmbiguousExport, name),
              &loc);
        }
      } else if (isolate->has_pending_exception()) {
        return MaybeHandle<Cell>();
      }
    }

    if (!unique_cell.is_null()) {
      // Found a unique star export for this name.
      Handle<ObjectHashTable> exports(module->exports(), isolate);
      DCHECK(exports->Lookup(name)->IsTheHole(isolate));
      exports = ObjectHashTable::Put(exports, name, unique_cell);
      module->set_exports(*exports);
      return unique_cell;
    }
  }

  // Unresolvable.
  if (must_resolve) {
    return isolate->Throw<Cell>(isolate->factory()->NewSyntaxError(
                                    MessageTemplate::kUnresolvableExport, name),
                                &loc);
  }
  return MaybeHandle<Cell>();
}

bool Module::Instantiate(Handle<Module> module, v8::Local<v8::Context> context,
                         v8::Module::ResolveCallback callback) {
  return PrepareInstantiate(module, context, callback) &&
         FinishInstantiate(module, context);
}

bool Module::PrepareInstantiate(Handle<Module> module,
                                v8::Local<v8::Context> context,
                                v8::Module::ResolveCallback callback) {
  if (module->status() == kPrepared) return true;

  // Obtain requested modules.
  Isolate* isolate = module->GetIsolate();
  Handle<ModuleInfo> module_info(module->info(), isolate);
  Handle<FixedArray> module_requests(module_info->module_requests(), isolate);
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = module_requests->length(); i < length; ++i) {
    Handle<String> specifier(String::cast(module_requests->get(i)), isolate);
    v8::Local<v8::Module> api_requested_module;
    if (!callback(context, v8::Utils::ToLocal(specifier),
                  v8::Utils::ToLocal(module))
             .ToLocal(&api_requested_module)) {
      isolate->PromoteScheduledException();
      return false;
    }
    Handle<Module> requested_module = Utils::OpenHandle(*api_requested_module);
    requested_modules->set(i, *requested_module);
  }

  // Recurse.
  module->set_status(kPrepared);
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    if (!PrepareInstantiate(requested_module, context, callback)) return false;
  }

  // Set up local exports.
  // TODO(neis): Create regular_exports array here instead of in factory method?
  for (int i = 0, n = module_info->RegularExportCount(); i < n; ++i) {
    int cell_index = module_info->RegularExportCellIndex(i);
    Handle<FixedArray> export_names(module_info->RegularExportExportNames(i),
                                    isolate);
    CreateExport(module, cell_index, export_names);
  }

  // Partially set up indirect exports.
  // For each indirect export, we create the appropriate slot in the export
  // table and store its ModuleInfoEntry there.  When we later find the correct
  // Cell in the module that actually provides the value, we replace the
  // ModuleInfoEntry by that Cell (see ResolveExport).
  Handle<FixedArray> special_exports(module_info->special_exports(), isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<ModuleInfoEntry> entry(
        ModuleInfoEntry::cast(special_exports->get(i)), isolate);
    Handle<Object> export_name(entry->export_name(), isolate);
    if (export_name->IsUndefined(isolate)) continue;  // Star export.
    CreateIndirectExport(module, Handle<String>::cast(export_name), entry);
  }

  DCHECK_EQ(module->status(), kPrepared);
  DCHECK(!module->instantiated());
  return true;
}

bool Module::FinishInstantiate(Handle<Module> module,
                               v8::Local<v8::Context> context) {
  DCHECK_EQ(module->status(), kPrepared);
  if (module->instantiated()) return true;

  // Instantiate SharedFunctionInfo and mark module as instantiated for
  // the recursion.
  Isolate* isolate = module->GetIsolate();
  Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(module->code()),
                                    isolate);
  Handle<JSFunction> function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared,
          handle(Utils::OpenHandle(*context)->native_context(), isolate));
  module->set_code(*function);
  DCHECK(module->instantiated());

  // Recurse.
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> requested_module(Module::cast(requested_modules->get(i)),
                                    isolate);
    if (!FinishInstantiate(requested_module, context)) return false;
  }

  Zone zone(isolate->allocator(), ZONE_NAME);

  // Resolve imports.
  Handle<ModuleInfo> module_info(shared->scope_info()->ModuleDescriptorInfo(),
                                 isolate);
  Handle<FixedArray> regular_imports(module_info->regular_imports(), isolate);
  for (int i = 0, n = regular_imports->length(); i < n; ++i) {
    Handle<ModuleInfoEntry> entry(
        ModuleInfoEntry::cast(regular_imports->get(i)), isolate);
    Handle<String> name(String::cast(entry->import_name()), isolate);
    Handle<Script> script(
        Script::cast(JSFunction::cast(module->code())->shared()->script()),
        isolate);
    MessageLocation loc(script, entry->beg_pos(), entry->end_pos());
    ResolveSet resolve_set(&zone);
    Handle<Cell> cell;
    if (!ResolveImport(module, name, entry->module_request(), loc, true,
                       &resolve_set)
             .ToHandle(&cell)) {
      return false;
    }
    module->regular_imports()->set(ImportIndex(entry->cell_index()), *cell);
  }

  // Resolve indirect exports.
  Handle<FixedArray> special_exports(module_info->special_exports(), isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<ModuleInfoEntry> entry(
        ModuleInfoEntry::cast(special_exports->get(i)), isolate);
    Handle<Object> name(entry->export_name(), isolate);
    if (name->IsUndefined(isolate)) continue;  // Star export.
    Handle<Script> script(
        Script::cast(JSFunction::cast(module->code())->shared()->script()),
        isolate);
    MessageLocation loc(script, entry->beg_pos(), entry->end_pos());
    ResolveSet resolve_set(&zone);
    if (ResolveExport(module, Handle<String>::cast(name), loc, true,
                      &resolve_set)
            .is_null()) {
      return false;
    }
  }

  return true;
}

MaybeHandle<Object> Module::Evaluate(Handle<Module> module) {
  DCHECK(module->instantiated());

  // Each module can only be evaluated once.
  Isolate* isolate = module->GetIsolate();
  if (module->evaluated()) return isolate->factory()->undefined_value();
  Handle<JSFunction> function(JSFunction::cast(module->code()), isolate);
  module->set_evaluated();

  // Initialization.
  DCHECK_EQ(MODULE_SCOPE, function->shared()->scope_info()->scope_type());
  Handle<Object> receiver = isolate->factory()->undefined_value();
  Handle<Object> argv[] = {module};
  Handle<Object> generator;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, generator,
      Execution::Call(isolate, function, receiver, arraysize(argv), argv),
      Object);

  // Recursion.
  Handle<FixedArray> requested_modules(module->requested_modules(), isolate);
  for (int i = 0, length = requested_modules->length(); i < length; ++i) {
    Handle<Module> import(Module::cast(requested_modules->get(i)), isolate);
    RETURN_ON_EXCEPTION(isolate, Evaluate(import), Object);
  }

  // Evaluation of module body.
  Handle<JSFunction> resume(
      isolate->native_context()->generator_next_internal(), isolate);
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, Execution::Call(isolate, resume, generator, 0, nullptr),
      Object);
  DCHECK(static_cast<JSIteratorResult*>(JSObject::cast(*result))
             ->done()
             ->BooleanValue());
  return handle(
      static_cast<JSIteratorResult*>(JSObject::cast(*result))->value(),
      isolate);
}

namespace {

void FetchStarExports(Handle<Module> module, Zone* zone,
                      UnorderedModuleSet* visited) {
  DCHECK(module->instantiated());

  bool cycle = !visited->insert(module).second;
  if (cycle) return;

  Isolate* isolate = module->GetIsolate();
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  UnorderedStringMap more_exports(zone);

  // TODO(neis): Only allocate more_exports if there are star exports.
  // Maybe split special_exports into indirect_exports and star_exports.

  Handle<FixedArray> special_exports(module->info()->special_exports(),
                                     isolate);
  for (int i = 0, n = special_exports->length(); i < n; ++i) {
    Handle<ModuleInfoEntry> entry(
        ModuleInfoEntry::cast(special_exports->get(i)), isolate);
    if (!entry->export_name()->IsUndefined(isolate)) {
      continue;  // Indirect export.
    }

    Handle<Module> requested_module(
        Module::cast(module->requested_modules()->get(entry->module_request())),
        isolate);

    // Recurse.
    FetchStarExports(requested_module, zone, visited);

    // Collect all of [requested_module]'s exports that must be added to
    // [module]'s exports (i.e. to [exports]).  We record these in
    // [more_exports].  Ambiguities (conflicting exports) are marked by mapping
    // the name to undefined instead of a Cell.
    Handle<ObjectHashTable> requested_exports(requested_module->exports(),
                                              isolate);
    for (int i = 0, n = requested_exports->Capacity(); i < n; ++i) {
      Handle<Object> key(requested_exports->KeyAt(i), isolate);
      if (!requested_exports->IsKey(isolate, *key)) continue;
      Handle<String> name = Handle<String>::cast(key);

      if (name->Equals(isolate->heap()->default_string())) continue;
      if (!exports->Lookup(name)->IsTheHole(isolate)) continue;

      Handle<Cell> cell(Cell::cast(requested_exports->ValueAt(i)), isolate);
      auto insert_result = more_exports.insert(std::make_pair(name, cell));
      if (!insert_result.second) {
        auto it = insert_result.first;
        if (*it->second == *cell || it->second->IsUndefined(isolate)) {
          // We already recorded this mapping before, or the name is already
          // known to be ambiguous.  In either case, there's nothing to do.
        } else {
          DCHECK(it->second->IsCell());
          // Different star exports provide different cells for this name, hence
          // mark the name as ambiguous.
          it->second = isolate->factory()->undefined_value();
        }
      }
    }
  }

  // Copy [more_exports] into [exports].
  for (const auto& elem : more_exports) {
    if (elem.second->IsUndefined(isolate)) continue;  // Ambiguous export.
    DCHECK(!elem.first->Equals(isolate->heap()->default_string()));
    DCHECK(elem.second->IsCell());
    exports = ObjectHashTable::Put(exports, elem.first, elem.second);
  }
  module->set_exports(*exports);
}

}  // anonymous namespace

Handle<JSModuleNamespace> Module::GetModuleNamespace(Handle<Module> module,
                                                     int module_request) {
  Isolate* isolate = module->GetIsolate();
  Handle<Module> requested_module(
      Module::cast(module->requested_modules()->get(module_request)), isolate);
  return Module::GetModuleNamespace(requested_module);
}

Handle<JSModuleNamespace> Module::GetModuleNamespace(Handle<Module> module) {
  Isolate* isolate = module->GetIsolate();

  Handle<HeapObject> object(module->module_namespace(), isolate);
  if (!object->IsUndefined(isolate)) {
    // Namespace object already exists.
    return Handle<JSModuleNamespace>::cast(object);
  }

  // Create the namespace object (initially empty).
  Handle<JSModuleNamespace> ns = isolate->factory()->NewJSModuleNamespace();
  ns->set_module(*module);
  module->set_module_namespace(*ns);

  // Collect the export names.
  Zone zone(isolate->allocator(), ZONE_NAME);
  UnorderedModuleSet visited(&zone);
  FetchStarExports(module, &zone, &visited);
  Handle<ObjectHashTable> exports(module->exports(), isolate);
  ZoneVector<Handle<String>> names(&zone);
  names.reserve(exports->NumberOfElements());
  for (int i = 0, n = exports->Capacity(); i < n; ++i) {
    Handle<Object> key(exports->KeyAt(i), isolate);
    if (!exports->IsKey(isolate, *key)) continue;
    DCHECK(exports->ValueAt(i)->IsCell());
    names.push_back(Handle<String>::cast(key));
  }
  DCHECK_EQ(static_cast<int>(names.size()), exports->NumberOfElements());

  // Sort them alphabetically.
  struct {
    bool operator()(Handle<String> a, Handle<String> b) {
      return String::Compare(a, b) == ComparisonResult::kLessThan;
    }
  } StringLess;
  std::sort(names.begin(), names.end(), StringLess);

  // Create the corresponding properties in the namespace object.
  PropertyAttributes attr = DONT_DELETE;
  for (const auto& name : names) {
    JSObject::SetAccessor(
        ns, Accessors::ModuleNamespaceEntryInfo(isolate, name, attr))
        .Check();
  }
  JSObject::PreventExtensions(ns, THROW_ON_ERROR).ToChecked();

  return ns;
}

MaybeHandle<Name> FunctionTemplateInfo::TryGetCachedPropertyName(
    Isolate* isolate, Handle<Object> getter) {
  if (getter->IsFunctionTemplateInfo()) {
    Handle<FunctionTemplateInfo> fti =
        Handle<FunctionTemplateInfo>::cast(getter);
    // Check if the accessor uses a cached property.
    if (!fti->cached_property_name()->IsTheHole(isolate)) {
      return handle(Name::cast(fti->cached_property_name()));
    }
  }
  return MaybeHandle<Name>();
}

// static
ElementsKind JSArrayIterator::ElementsKindForInstanceType(InstanceType type) {
  DCHECK_GE(type, FIRST_ARRAY_ITERATOR_TYPE);
  DCHECK_LE(type, LAST_ARRAY_ITERATOR_TYPE);

  if (type <= LAST_ARRAY_KEY_ITERATOR_TYPE) {
    // Should be ignored for key iterators.
    return FAST_ELEMENTS;
  } else {
    ElementsKind kind;
    if (type < FIRST_ARRAY_VALUE_ITERATOR_TYPE) {
      // Convert `type` to a value iterator from an entries iterator
      type = static_cast<InstanceType>(type +
                                       (FIRST_ARRAY_VALUE_ITERATOR_TYPE -
                                        FIRST_ARRAY_KEY_VALUE_ITERATOR_TYPE));
      DCHECK_GE(type, FIRST_ARRAY_VALUE_ITERATOR_TYPE);
      DCHECK_LE(type, LAST_ARRAY_ITERATOR_TYPE);
    }

    if (type <= JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE) {
      kind =
          static_cast<ElementsKind>(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND +
                                    (type - FIRST_ARRAY_VALUE_ITERATOR_TYPE));
      DCHECK_LE(kind, LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);
    } else if (type < JS_GENERIC_ARRAY_VALUE_ITERATOR_TYPE) {
      kind = static_cast<ElementsKind>(
          FIRST_FAST_ELEMENTS_KIND +
          (type - JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE));
      DCHECK_LE(kind, LAST_FAST_ELEMENTS_KIND);
    } else {
      // For any slow element cases, the actual elements kind is not known.
      // Simply
      // return a slow elements kind in this case. Users of this function must
      // not
      // depend on this.
      return DICTIONARY_ELEMENTS;
    }
    DCHECK_LE(kind, LAST_ELEMENTS_KIND);
    return kind;
  }
}
}  // namespace internal
}  // namespace v8
