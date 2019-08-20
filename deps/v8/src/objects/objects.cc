// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <vector>

#include "src/objects/objects-inl.h"

#include "src/api/api-arguments-inl.h"
#include "src/api/api-natives.h"
#include "src/api/api.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/bits.h"
#include "src/base/debug/stack_trace.h"
#include "src/base/overflowing-math.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/accessors.h"
#include "src/builtins/builtins.h"
#include "src/codegen/compiler.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/date/date.h"
#include "src/debug/debug.h"
#include "src/execution/arguments.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/microtask-queue.h"
#include "src/heap/heap-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters-inl.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/allocation-site-scopes.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/cell-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/compilation-cache-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/field-index.h"
#include "src/objects/field-type.h"
#include "src/objects/foreign.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/function-kind.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/keys.h"
#include "src/objects/lookup-inl.h"
#include "src/objects/map-updater.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/utils/identity-map.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-break-iterator.h"
#include "src/objects/js-collator.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-collection-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-date-time-format.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-generator-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-list-format.h"
#include "src/objects/js-locale.h"
#include "src/objects/js-number-format.h"
#include "src/objects/js-plural-rules.h"
#endif  // V8_INTL_SUPPORT
#include "src/objects/js-regexp-inl.h"
#include "src/objects/js-regexp-string-iterator.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/js-relative-time-format.h"
#include "src/objects/js-segment-iterator.h"
#include "src/objects/js-segmenter.h"
#endif  // V8_INTL_SUPPORT
#include "src/codegen/source-position-table.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/map.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/prototype.h"
#include "src/objects/slots-atomic-inl.h"
#include "src/objects/stack-frame-info-inl.h"
#include "src/objects/string-comparator.h"
#include "src/objects/struct-inl.h"
#include "src/objects/template-objects-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/parsing/preparse-data.h"
#include "src/regexp/regexp.h"
#include "src/strings/string-builder-inl.h"
#include "src/strings/string-search.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-decoder.h"
#include "src/strings/unicode-inl.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/zone.h"

#include "torque-generated/class-definitions-tq-inl.h"
#include "torque-generated/internal-class-definitions-tq-inl.h"

namespace v8 {
namespace internal {

ShouldThrow GetShouldThrow(Isolate* isolate, Maybe<ShouldThrow> should_throw) {
  if (should_throw.IsJust()) return should_throw.FromJust();

  LanguageMode mode = isolate->context().scope_info().language_mode();
  if (mode == LanguageMode::kStrict) return kThrowOnError;

  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    if (!(it.frame()->is_optimized() || it.frame()->is_interpreted())) {
      continue;
    }
    // Get the language mode from closure.
    JavaScriptFrame* js_frame = static_cast<JavaScriptFrame*>(it.frame());
    std::vector<SharedFunctionInfo> functions;
    js_frame->GetFunctions(&functions);
    LanguageMode closure_language_mode = functions.back().language_mode();
    if (closure_language_mode > mode) {
      mode = closure_language_mode;
    }
    break;
  }

  return is_sloppy(mode) ? kDontThrow : kThrowOnError;
}

bool ComparisonResultToBool(Operation op, ComparisonResult result) {
  switch (op) {
    case Operation::kLessThan:
      return result == ComparisonResult::kLessThan;
    case Operation::kLessThanOrEqual:
      return result == ComparisonResult::kLessThan ||
             result == ComparisonResult::kEqual;
    case Operation::kGreaterThan:
      return result == ComparisonResult::kGreaterThan;
    case Operation::kGreaterThanOrEqual:
      return result == ComparisonResult::kGreaterThan ||
             result == ComparisonResult::kEqual;
    default:
      break;
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, InstanceType instance_type) {
  switch (instance_type) {
#define WRITE_TYPE(TYPE) \
  case TYPE:             \
    return os << #TYPE;
    INSTANCE_TYPE_LIST(WRITE_TYPE)
#undef WRITE_TYPE
  }
  UNREACHABLE();
}

Handle<FieldType> Object::OptimalType(Isolate* isolate,
                                      Representation representation) {
  if (representation.IsNone()) return FieldType::None(isolate);
  if (FLAG_track_field_types) {
    if (representation.IsHeapObject() && IsHeapObject()) {
      // We can track only JavaScript objects with stable maps.
      Handle<Map> map(HeapObject::cast(*this).map(), isolate);
      if (map->is_stable() && map->IsJSReceiverMap()) {
        return FieldType::Class(map, isolate);
      }
    }
  }
  return FieldType::Any(isolate);
}

Handle<Object> Object::NewStorageFor(Isolate* isolate, Handle<Object> object,
                                     Representation representation) {
  if (!representation.IsDouble()) return object;
  auto result = isolate->factory()->NewMutableHeapNumberWithHoleNaN();
  if (object->IsUninitialized(isolate)) {
    result->set_value_as_bits(kHoleNanInt64);
  } else if (object->IsMutableHeapNumber()) {
    // Ensure that all bits of the double value are preserved.
    result->set_value_as_bits(MutableHeapNumber::cast(*object).value_as_bits());
  } else {
    result->set_value(object->Number());
  }
  return result;
}

Handle<Object> Object::WrapForRead(Isolate* isolate, Handle<Object> object,
                                   Representation representation) {
  DCHECK(!object->IsUninitialized(isolate));
  if (!representation.IsDouble()) {
    DCHECK(object->FitsRepresentation(representation));
    return object;
  }
  return isolate->factory()->NewHeapNumberFromBits(
      MutableHeapNumber::cast(*object).value_as_bits());
}

MaybeHandle<JSReceiver> Object::ToObjectImpl(Isolate* isolate,
                                             Handle<Object> object,
                                             const char* method_name) {
  DCHECK(!object->IsJSReceiver());  // Use ToObject() for fast path.
  Handle<Context> native_context = isolate->native_context();
  Handle<JSFunction> constructor;
  if (object->IsSmi()) {
    constructor = handle(native_context->number_function(), isolate);
  } else {
    int constructor_function_index =
        Handle<HeapObject>::cast(object)->map().GetConstructorFunctionIndex();
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
  Handle<JSPrimitiveWrapper>::cast(result)->set_value(*object);
  return result;
}

// ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
// static
MaybeHandle<JSReceiver> Object::ConvertReceiver(Isolate* isolate,
                                                Handle<Object> object) {
  if (object->IsJSReceiver()) return Handle<JSReceiver>::cast(object);
  if (object->IsNullOrUndefined(isolate)) {
    return isolate->global_proxy();
  }
  return Object::ToObject(isolate, object);
}

// static
MaybeHandle<Object> Object::ConvertToNumberOrNumeric(Isolate* isolate,
                                                     Handle<Object> input,
                                                     Conversion mode) {
  while (true) {
    if (input->IsNumber()) {
      return input;
    }
    if (input->IsString()) {
      return String::ToNumber(isolate, Handle<String>::cast(input));
    }
    if (input->IsOddball()) {
      return Oddball::ToNumber(isolate, Handle<Oddball>::cast(input));
    }
    if (input->IsSymbol()) {
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kSymbolToNumber),
                      Object);
    }
    if (input->IsBigInt()) {
      if (mode == Conversion::kToNumeric) return input;
      DCHECK_EQ(mode, Conversion::kToNumber);
      THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kBigIntToNumber),
                      Object);
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input,
        JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
                                ToPrimitiveHint::kNumber),
        Object);
  }
}

// static
MaybeHandle<Object> Object::ConvertToInteger(Isolate* isolate,
                                             Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumber(DoubleToInteger(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToInt32(Isolate* isolate,
                                           Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return input;
  return isolate->factory()->NewNumberFromInt(DoubleToInt32(input->Number()));
}

// static
MaybeHandle<Object> Object::ConvertToUint32(Isolate* isolate,
                                            Handle<Object> input) {
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, input,
      ConvertToNumberOrNumeric(isolate, input, Conversion::kToNumber), Object);
  if (input->IsSmi()) return handle(Smi::cast(*input).ToUint32Smi(), isolate);
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
    if (input->IsBigInt()) {
      return BigInt::ToString(isolate, Handle<BigInt>::cast(input));
    }
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, input,
        JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(input),
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
  } else if (input->IsBigInt()) {
    MaybeHandle<String> maybe_string =
        BigInt::ToString(isolate, Handle<BigInt>::cast(input), 10, kDontThrow);
    Handle<String> result;
    if (maybe_string.ToHandle(&result)) return result;
    // BigInt-to-String conversion can fail on 32-bit platforms where
    // String::kMaxLength is too small to fit this BigInt.
    return isolate->factory()->NewStringFromStaticChars(
        "<a very large BigInt>");
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

    if (symbol->is_private_name()) {
      return Handle<String>(String::cast(symbol->name()), isolate);
    }

    IncrementalStringBuilder builder(isolate);
    builder.AppendCString("Symbol(");
    if (symbol->name().IsString()) {
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
        Handle<HeapObject>::cast(input)->map().GetConstructorFunctionIndex();
    if (constructor_function_index == Map::kNoConstructorFunctionIndex) {
      return isolate->factory()->NewStringFromAsciiChecked("[object Unknown]");
    }

    receiver = Object::ToObjectImpl(isolate, input).ToHandleChecked();
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
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(isolate, input), Object);
  if (input->IsSmi()) {
    int value = std::max(Smi::ToInt(*input), 0);
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
MaybeHandle<Object> Object::ConvertToIndex(Isolate* isolate,
                                           Handle<Object> input,
                                           MessageTemplate error_index) {
  if (input->IsUndefined(isolate)) return handle(Smi::kZero, isolate);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(isolate, input), Object);
  if (input->IsSmi() && Smi::ToInt(*input) >= 0) return input;
  double len = DoubleToInteger(input->Number()) + 0.0;
  auto js_len = isolate->factory()->NewNumber(len);
  if (len < 0.0 || len > kMaxSafeInteger) {
    THROW_NEW_ERROR(isolate, NewRangeError(error_index, js_len), Object);
  }
  return js_len;
}

bool Object::BooleanValue(Isolate* isolate) {
  if (IsSmi()) return Smi::ToInt(*this) != 0;
  DCHECK(IsHeapObject());
  if (IsBoolean()) return IsTrue(isolate);
  if (IsNullOrUndefined(isolate)) return false;
  if (IsUndetectable()) return false;  // Undetectable object is false.
  if (IsString()) return String::cast(*this).length() != 0;
  if (IsHeapNumber()) return DoubleToBoolean(HeapNumber::cast(*this).value());
  if (IsBigInt()) return BigInt::cast(*this).ToBoolean();
  return true;
}

Object Object::ToBoolean(Isolate* isolate) {
  if (IsBoolean()) return *this;
  return isolate->heap()->ToBoolean(BooleanValue(isolate));
}

namespace {

// TODO(bmeurer): Maybe we should introduce a marker interface Number,
// where we put all these methods at some point?
ComparisonResult StrictNumberCompare(double x, double y) {
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

// See Number case of ES6#sec-strict-equality-comparison
// Returns false if x or y is NaN, treats -0.0 as equal to 0.0.
bool StrictNumberEquals(double x, double y) {
  // Must check explicitly for NaN's on Windows, but -0 works fine.
  if (std::isnan(x) || std::isnan(y)) return false;
  return x == y;
}

bool StrictNumberEquals(const Object x, const Object y) {
  return StrictNumberEquals(x.Number(), y.Number());
}

bool StrictNumberEquals(Handle<Object> x, Handle<Object> y) {
  return StrictNumberEquals(*x, *y);
}

ComparisonResult Reverse(ComparisonResult result) {
  if (result == ComparisonResult::kLessThan) {
    return ComparisonResult::kGreaterThan;
  }
  if (result == ComparisonResult::kGreaterThan) {
    return ComparisonResult::kLessThan;
  }
  return result;
}

}  // anonymous namespace

// static
Maybe<ComparisonResult> Object::Compare(Isolate* isolate, Handle<Object> x,
                                        Handle<Object> y) {
  // ES6 section 7.2.11 Abstract Relational Comparison step 3 and 4.
  if (!Object::ToPrimitive(x, ToPrimitiveHint::kNumber).ToHandle(&x) ||
      !Object::ToPrimitive(y, ToPrimitiveHint::kNumber).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }
  if (x->IsString() && y->IsString()) {
    // ES6 section 7.2.11 Abstract Relational Comparison step 5.
    return Just(String::Compare(isolate, Handle<String>::cast(x),
                                Handle<String>::cast(y)));
  }
  if (x->IsBigInt() && y->IsString()) {
    return Just(BigInt::CompareToString(isolate, Handle<BigInt>::cast(x),
                                        Handle<String>::cast(y)));
  }
  if (x->IsString() && y->IsBigInt()) {
    return Just(Reverse(BigInt::CompareToString(
        isolate, Handle<BigInt>::cast(y), Handle<String>::cast(x))));
  }
  // ES6 section 7.2.11 Abstract Relational Comparison step 6.
  if (!Object::ToNumeric(isolate, x).ToHandle(&x) ||
      !Object::ToNumeric(isolate, y).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }

  bool x_is_number = x->IsNumber();
  bool y_is_number = y->IsNumber();
  if (x_is_number && y_is_number) {
    return Just(StrictNumberCompare(x->Number(), y->Number()));
  } else if (!x_is_number && !y_is_number) {
    return Just(BigInt::CompareToBigInt(Handle<BigInt>::cast(x),
                                        Handle<BigInt>::cast(y)));
  } else if (x_is_number) {
    return Just(Reverse(BigInt::CompareToNumber(Handle<BigInt>::cast(y), x)));
  } else {
    return Just(BigInt::CompareToNumber(Handle<BigInt>::cast(x), y));
  }
}

// static
Maybe<bool> Object::Equals(Isolate* isolate, Handle<Object> x,
                           Handle<Object> y) {
  // This is the generic version of Abstract Equality Comparison. Must be in
  // sync with CodeStubAssembler::Equal.
  while (true) {
    if (x->IsNumber()) {
      if (y->IsNumber()) {
        return Just(StrictNumberEquals(x, y));
      } else if (y->IsBoolean()) {
        return Just(
            StrictNumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsString()) {
        return Just(StrictNumberEquals(
            x, String::ToNumber(isolate, Handle<String>::cast(y))));
      } else if (y->IsBigInt()) {
        return Just(BigInt::EqualToNumber(Handle<BigInt>::cast(y), x));
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
        return Just(String::Equals(isolate, Handle<String>::cast(x),
                                   Handle<String>::cast(y)));
      } else if (y->IsNumber()) {
        x = String::ToNumber(isolate, Handle<String>::cast(x));
        return Just(StrictNumberEquals(x, y));
      } else if (y->IsBoolean()) {
        x = String::ToNumber(isolate, Handle<String>::cast(x));
        return Just(
            StrictNumberEquals(*x, Handle<Oddball>::cast(y)->to_number()));
      } else if (y->IsBigInt()) {
        return Just(BigInt::EqualToString(isolate, Handle<BigInt>::cast(y),
                                          Handle<String>::cast(x)));
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
        return Just(
            StrictNumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsString()) {
        y = String::ToNumber(isolate, Handle<String>::cast(y));
        return Just(
            StrictNumberEquals(Handle<Oddball>::cast(x)->to_number(), *y));
      } else if (y->IsBigInt()) {
        x = Oddball::ToNumber(isolate, Handle<Oddball>::cast(x));
        return Just(BigInt::EqualToNumber(Handle<BigInt>::cast(y), x));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(y))
                 .ToHandle(&y)) {
          return Nothing<bool>();
        }
        x = Oddball::ToNumber(isolate, Handle<Oddball>::cast(x));
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
    } else if (x->IsBigInt()) {
      if (y->IsBigInt()) {
        return Just(BigInt::EqualToBigInt(BigInt::cast(*x), BigInt::cast(*y)));
      }
      return Equals(isolate, y, x);
    } else if (x->IsJSReceiver()) {
      if (y->IsJSReceiver()) {
        return Just(x.is_identical_to(y));
      } else if (y->IsUndetectable()) {
        return Just(x->IsUndetectable());
      } else if (y->IsBoolean()) {
        y = Oddball::ToNumber(isolate, Handle<Oddball>::cast(y));
      } else if (!JSReceiver::ToPrimitive(Handle<JSReceiver>::cast(x))
                      .ToHandle(&x)) {
        return Nothing<bool>();
      }
    } else {
      return Just(x->IsUndetectable() && y->IsUndetectable());
    }
  }
}

bool Object::StrictEquals(Object that) {
  if (this->IsNumber()) {
    if (!that.IsNumber()) return false;
    return StrictNumberEquals(*this, that);
  } else if (this->IsString()) {
    if (!that.IsString()) return false;
    return String::cast(*this).Equals(String::cast(that));
  } else if (this->IsBigInt()) {
    if (!that.IsBigInt()) return false;
    return BigInt::EqualToBigInt(BigInt::cast(*this), BigInt::cast(that));
  }
  return *this == that;
}

// static
Handle<String> Object::TypeOf(Isolate* isolate, Handle<Object> object) {
  if (object->IsNumber()) return isolate->factory()->number_string();
  if (object->IsOddball())
    return handle(Oddball::cast(*object).type_of(), isolate);
  if (object->IsUndetectable()) {
    return isolate->factory()->undefined_string();
  }
  if (object->IsString()) return isolate->factory()->string_string();
  if (object->IsSymbol()) return isolate->factory()->symbol_string();
  if (object->IsBigInt()) return isolate->factory()->bigint_string();
  if (object->IsCallable()) return isolate->factory()->function_string();
  return isolate->factory()->object_string();
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
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToNumber(isolate, rhs),
                             Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToNumber(isolate, lhs),
                             Object);
  return isolate->factory()->NewNumber(lhs->Number() + rhs->Number());
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
      Object::GetProperty(isolate, callable,
                          isolate->factory()->prototype_string()),
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
      Object::GetMethod(Handle<JSReceiver>::cast(callable),
                        isolate->factory()->has_instance_symbol()),
      Object);
  if (!inst_of_handler->IsUndefined(isolate)) {
    // Call the {inst_of_handler} on the {callable}.
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, inst_of_handler, callable, 1, &object),
        Object);
    return isolate->factory()->ToBoolean(result->BooleanValue(isolate));
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
      isolate, result, Object::OrdinaryHasInstance(isolate, callable, object),
      Object);
  return result;
}

// static
MaybeHandle<Object> Object::GetMethod(Handle<JSReceiver> receiver,
                                      Handle<Name> name) {
  Handle<Object> func;
  Isolate* isolate = receiver->GetIsolate();
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, func, JSReceiver::GetProperty(isolate, receiver, name), Object);
  if (func->IsNullOrUndefined(isolate)) {
    return isolate->factory()->undefined_value();
  }
  if (!func->IsCallable()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kPropertyNotFunction, func,
                                 name, receiver),
                    Object);
  }
  return func;
}

namespace {

MaybeHandle<FixedArray> CreateListFromArrayLikeFastPath(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  if (element_types == ElementTypes::kAll) {
    if (object->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(object);
      uint32_t length;
      if (!array->HasArrayPrototype(isolate) ||
          !array->length().ToUint32(&length) || !array->HasFastElements() ||
          !JSObject::PrototypeHasNoElements(isolate, *array)) {
        return MaybeHandle<FixedArray>();
      }
      return array->GetElementsAccessor()->CreateListFromArrayLike(
          isolate, array, length);
    } else if (object->IsJSTypedArray()) {
      Handle<JSTypedArray> array = Handle<JSTypedArray>::cast(object);
      size_t length = array->length();
      if (array->WasDetached() ||
          length > static_cast<size_t>(FixedArray::kMaxLength)) {
        return MaybeHandle<FixedArray>();
      }
      return array->GetElementsAccessor()->CreateListFromArrayLike(
          isolate, array, static_cast<uint32_t>(length));
    }
  }
  return MaybeHandle<FixedArray>();
}

}  // namespace

// static
MaybeHandle<FixedArray> Object::CreateListFromArrayLike(
    Isolate* isolate, Handle<Object> object, ElementTypes element_types) {
  // Fast-path for JSArray and JSTypedArray.
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
                                                   Handle<JSReceiver> object) {
  Handle<Object> val;
  Handle<Name> key = isolate->factory()->length_string();
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, val, JSReceiver::GetProperty(isolate, object, key), Object);
  return Object::ToLength(isolate, val);
}

// static
MaybeHandle<Object> Object::GetProperty(LookupIterator* it,
                                        OnNonExistent on_non_existent) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY: {
        bool was_found;
        Handle<Object> receiver = it->GetReceiver();
        // In case of global IC, the receiver is the global object. Replace by
        // the global proxy.
        if (receiver->IsJSGlobalObject()) {
          receiver = handle(JSGlobalObject::cast(*receiver).global_proxy(),
                            it->isolate());
        }
        MaybeHandle<Object> result =
            JSProxy::GetProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                 it->GetName(), receiver, &was_found);
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

  if (on_non_existent == OnNonExistent::kThrowReferenceError) {
    THROW_NEW_ERROR(it->isolate(),
                    NewReferenceError(MessageTemplate::kNotDefined, it->name()),
                    Object);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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

  MaybeHandle<Object> result =
      JSProxy::CheckGetSetTrapResult(isolate, name, target, trap_result, kGet);
  if (result.is_null()) {
    return result;
  }

  // 11. Return trap_result
  return trap_result;
}

// static
MaybeHandle<Object> JSProxy::CheckGetSetTrapResult(Isolate* isolate,
                                                   Handle<Name> name,
                                                   Handle<JSReceiver> target,
                                                   Handle<Object> trap_result,
                                                   AccessKind access_kind) {
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
      if (access_kind == kGet) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(MessageTemplate::kProxyGetNonConfigurableData, name,
                         target_desc.value(), trap_result),
            Object);
      } else {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxySetFrozenData, name));
        return MaybeHandle<Object>();
      }
    }
    // 10.b. If IsAccessorDescriptor(targetDesc) and targetDesc.[[Configurable]]
    //       is false and targetDesc.[[Get]] is undefined, then
    // 10.b.i. If trapResult is not undefined, throw a TypeError exception.
    if (access_kind == kGet) {
      inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                     !target_desc.configurable() &&
                     target_desc.get()->IsUndefined(isolate) &&
                     !trap_result->IsUndefined(isolate);
    } else {
      inconsistent = PropertyDescriptor::IsAccessorDescriptor(&target_desc) &&
                     !target_desc.configurable() &&
                     target_desc.set()->IsUndefined(isolate);
    }
    if (inconsistent) {
      if (access_kind == kGet) {
        THROW_NEW_ERROR(
            isolate,
            NewTypeError(MessageTemplate::kProxyGetNonConfigurableAccessor,
                         name, trap_result),
            Object);
      } else {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxySetFrozenAccessor, name));
        return MaybeHandle<Object>();
      }
    }
  }
  return isolate->factory()->undefined_value();
}

bool Object::ToInt32(int32_t* value) {
  if (IsSmi()) {
    *value = Smi::ToInt(*this);
    return true;
  }
  if (IsHeapNumber()) {
    double num = HeapNumber::cast(*this).value();
    // Check range before conversion to avoid undefined behavior.
    if (num >= kMinInt && num <= kMaxInt && FastI2D(FastD2I(num)) == num) {
      *value = FastD2I(num);
      return true;
    }
  }
  return false;
}

// static constexpr object declarations need a definition to make the
// compiler happy.
constexpr Object Smi::kZero;
V8_EXPORT_PRIVATE constexpr Object SharedFunctionInfo::kNoSharedNameSentinel;

Handle<SharedFunctionInfo> FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
    Isolate* isolate, Handle<FunctionTemplateInfo> info,
    MaybeHandle<Name> maybe_name) {
  Object current_info = info->shared_function_info();
  if (current_info.IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(current_info), isolate);
  }
  Handle<Name> name;
  Handle<String> name_string;
  if (maybe_name.ToHandle(&name) && name->IsString()) {
    name_string = Handle<String>::cast(name);
  } else if (info->class_name().IsString()) {
    name_string = handle(String::cast(info->class_name()), isolate);
  } else {
    name_string = isolate->factory()->empty_string();
  }
  FunctionKind function_kind;
  if (info->remove_prototype()) {
    function_kind = kConciseMethod;
  } else {
    function_kind = kNormalFunction;
  }
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfoForApiFunction(name_string, info,
                                                              function_kind);

  result->set_length(info->length());
  result->DontAdaptArguments();
  DCHECK(result->IsApiFunction());

  info->set_shared_function_info(*result);
  return result;
}

bool FunctionTemplateInfo::IsTemplateFor(Map map) {
  // There is a constraint on the object; check.
  if (!map.IsJSObjectMap()) return false;
  // Fetch the constructor function of the object.
  Object cons_obj = map.GetConstructor();
  Object type;
  if (cons_obj.IsJSFunction()) {
    JSFunction fun = JSFunction::cast(cons_obj);
    type = fun.shared().function_data();
  } else if (cons_obj.IsFunctionTemplateInfo()) {
    type = FunctionTemplateInfo::cast(cons_obj);
  } else {
    return false;
  }
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  while (type.IsFunctionTemplateInfo()) {
    if (type == *this) return true;
    type = FunctionTemplateInfo::cast(type).GetParentTemplate();
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}

// static
FunctionTemplateRareData FunctionTemplateInfo::AllocateFunctionTemplateRareData(
    Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info) {
  DCHECK(function_template_info->rare_data().IsUndefined(isolate));
  Handle<Struct> struct_obj = isolate->factory()->NewStruct(
      FUNCTION_TEMPLATE_RARE_DATA_TYPE, AllocationType::kOld);
  Handle<FunctionTemplateRareData> rare_data =
      i::Handle<FunctionTemplateRareData>::cast(struct_obj);
  function_template_info->set_rare_data(*rare_data);
  return *rare_data;
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
  fixed_array = FixedArray::SetAndGrow(isolate, fixed_array, index, value);
  fixed_array->set(kLengthIndex, Smi::FromInt(index));
  return Handle<TemplateList>::cast(fixed_array);
}

// ES6 9.5.1
// static
MaybeHandle<HeapObject> JSProxy::GetPrototype(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<String> trap_name = isolate->factory()->getPrototypeOf_string();

  STACK_CHECK(isolate, MaybeHandle<HeapObject>());

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot.
  // 2. If handler is null, throw a TypeError exception.
  // 3. Assert: Type(handler) is Object.
  // 4. Let target be the value of the [[ProxyTarget]] internal slot.
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyRevoked, trap_name),
                    HeapObject);
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  // 5. Let trap be ? GetMethod(handler, "getPrototypeOf").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, trap,
                             Object::GetMethod(handler, trap_name), HeapObject);
  // 6. If trap is undefined, then return target.[[GetPrototypeOf]]().
  if (trap->IsUndefined(isolate)) {
    return JSReceiver::GetPrototype(isolate, target);
  }
  // 7. Let handlerProto be ? Call(trap, handler, «target»).
  Handle<Object> argv[] = {target};
  Handle<Object> handler_proto;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, handler_proto,
      Execution::Call(isolate, trap, handler, arraysize(argv), argv),
      HeapObject);
  // 8. If Type(handlerProto) is neither Object nor Null, throw a TypeError.
  if (!(handler_proto->IsJSReceiver() || handler_proto->IsNull(isolate))) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyGetPrototypeOfInvalid),
                    HeapObject);
  }
  // 9. Let extensibleTarget be ? IsExtensible(target).
  Maybe<bool> is_extensible = JSReceiver::IsExtensible(target);
  MAYBE_RETURN(is_extensible, MaybeHandle<HeapObject>());
  // 10. If extensibleTarget is true, return handlerProto.
  if (is_extensible.FromJust()) return Handle<HeapObject>::cast(handler_proto);
  // 11. Let targetProto be ? target.[[GetPrototypeOf]]().
  Handle<HeapObject> target_proto;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, target_proto,
                             JSReceiver::GetPrototype(isolate, target),
                             HeapObject);
  // 12. If SameValue(handlerProto, targetProto) is false, throw a TypeError.
  if (!handler_proto->SameValue(*target_proto)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kProxyGetPrototypeOfNonExtensible),
        HeapObject);
  }
  // 13. Return handlerProto.
  return Handle<HeapObject>::cast(handler_proto);
}

MaybeHandle<Object> Object::GetPropertyWithAccessor(LookupIterator* it) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();
  // In case of global IC, the receiver is the global object. Replace by the
  // global proxy.
  if (receiver->IsJSGlobalObject()) {
    receiver = handle(JSGlobalObject::cast(*receiver).global_proxy(), isolate);
  }

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the getter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  Handle<JSObject> holder = it->GetHolder<JSObject>();
  if (structure->IsAccessorInfo()) {
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                                   name, receiver),
                      Object);
    }

    if (!info->has_getter()) return isolate->factory()->undefined_value();

    if (info->is_sloppy() && !receiver->IsJSReceiver()) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                                 Object::ConvertReceiver(isolate, receiver),
                                 Object);
    }

    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   Just(kDontThrow));
    Handle<Object> result = args.CallAccessorGetter(info, name);
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.is_null()) return isolate->factory()->undefined_value();
    Handle<Object> reboxed_result = handle(*result, isolate);
    if (info->replace_on_access() && receiver->IsJSReceiver()) {
      RETURN_ON_EXCEPTION(isolate,
                          Accessors::ReplaceAccessorWithDataProperty(
                              receiver, holder, name, result),
                          Object);
    }
    return reboxed_result;
  }

  // AccessorPair with 'cached' private property.
  if (it->TryLookupCachedProperty()) {
    return Object::GetProperty(it);
  }

  // Regular accessor.
  Handle<Object> getter(AccessorPair::cast(*structure).getter(), isolate);
  if (getter->IsFunctionTemplateInfo()) {
    SaveAndSwitchContext save(isolate, *holder->GetCreationContext());
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
Address AccessorInfo::redirect(Address address, AccessorComponent component) {
  ApiFunction fun(address);
  DCHECK_EQ(ACCESSOR_GETTER, component);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  return ExternalReference::Create(&fun, type).address();
}

Address AccessorInfo::redirected_getter() const {
  Address accessor = v8::ToCData<Address>(getter());
  if (accessor == kNullAddress) return kNullAddress;
  return redirect(accessor, ACCESSOR_GETTER);
}

Address CallHandlerInfo::redirected_callback() const {
  Address address = v8::ToCData<Address>(callback());
  ApiFunction fun(address);
  ExternalReference::Type type = ExternalReference::DIRECT_API_CALL;
  return ExternalReference::Create(&fun, type).address();
}

bool AccessorInfo::IsCompatibleReceiverMap(Handle<AccessorInfo> info,
                                           Handle<Map> map) {
  if (!info->HasExpectedReceiverType()) return true;
  if (!map->IsJSObjectMap()) return false;
  return FunctionTemplateInfo::cast(info->expected_receiver_type())
      .IsTemplateFor(*map);
}

Maybe<bool> Object::SetPropertyWithAccessor(
    LookupIterator* it, Handle<Object> value,
    Maybe<ShouldThrow> maybe_should_throw) {
  Isolate* isolate = it->isolate();
  Handle<Object> structure = it->GetAccessors();
  Handle<Object> receiver = it->GetReceiver();
  // In case of global IC, the receiver is the global object. Replace by the
  // global proxy.
  if (receiver->IsJSGlobalObject()) {
    receiver = handle(JSGlobalObject::cast(*receiver).global_proxy(), isolate);
  }

  // We should never get here to initialize a const with the hole value since a
  // const declaration would conflict with the setter.
  DCHECK(!structure->IsForeign());

  // API style callbacks.
  Handle<JSObject> holder = it->GetHolder<JSObject>();
  if (structure->IsAccessorInfo()) {
    Handle<Name> name = it->GetName();
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(structure);
    if (!info->IsCompatibleReceiver(*receiver)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kIncompatibleMethodReceiver, name, receiver));
      return Nothing<bool>();
    }

    if (!info->has_setter()) {
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

    // The actual type of setter callback is either
    // v8::AccessorNameSetterCallback or
    // i::Accesors::AccessorNameBooleanSetterCallback, depending on whether the
    // AccessorInfo was created by the API or internally (see accessors.cc).
    // Here we handle both cases using GenericNamedPropertySetterCallback and
    // its Call method.
    PropertyCallbackArguments args(isolate, info->data(), *receiver, *holder,
                                   maybe_should_throw);
    Handle<Object> result = args.CallAccessorSetter(info, name, value);
    // In the case of AccessorNameSetterCallback, we know that the result value
    // cannot have been set, so the result of Call will be null.  In the case of
    // AccessorNameBooleanSetterCallback, the result will either be null
    // (signalling an exception) or a boolean Oddball.
    RETURN_VALUE_IF_SCHEDULED_EXCEPTION(isolate, Nothing<bool>());
    if (result.is_null()) return Just(true);
    DCHECK(result->BooleanValue(isolate) ||
           GetShouldThrow(isolate, maybe_should_throw) == kDontThrow);
    return Just(result->BooleanValue(isolate));
  }

  // Regular accessor.
  Handle<Object> setter(AccessorPair::cast(*structure).setter(), isolate);
  if (setter->IsFunctionTemplateInfo()) {
    SaveAndSwitchContext save(isolate, *holder->GetCreationContext());
    Handle<Object> argv[] = {value};
    RETURN_ON_EXCEPTION_VALUE(
        isolate,
        Builtins::InvokeApiFunction(isolate, false,
                                    Handle<FunctionTemplateInfo>::cast(setter),
                                    receiver, arraysize(argv), argv,
                                    isolate->factory()->undefined_value()),
        Nothing<bool>());
    return Just(true);
  } else if (setter->IsCallable()) {
    // TODO(rossberg): nicer would be to cast to some JSCallable here...
    return SetPropertyWithDefinedSetter(
        receiver, Handle<JSReceiver>::cast(setter), value, maybe_should_throw);
  }

  RETURN_FAILURE(isolate, GetShouldThrow(isolate, maybe_should_throw),
                 NewTypeError(MessageTemplate::kNoSetterInCallback,
                              it->GetName(), it->GetHolder<JSObject>()));
}

MaybeHandle<Object> Object::GetPropertyWithDefinedGetter(
    Handle<Object> receiver, Handle<JSReceiver> getter) {
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

  return Execution::Call(isolate, getter, receiver, 0, nullptr);
}

Maybe<bool> Object::SetPropertyWithDefinedSetter(
    Handle<Object> receiver, Handle<JSReceiver> setter, Handle<Object> value,
    Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = setter->GetIsolate();

  Handle<Object> argv[] = {value};
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      Execution::Call(isolate, setter, receiver, arraysize(argv), argv),
      Nothing<bool>());
  return Just(true);
}

Map Object::GetPrototypeChainRootMap(Isolate* isolate) const {
  DisallowHeapAllocation no_alloc;
  if (IsSmi()) {
    Context native_context = isolate->context().native_context();
    return native_context.number_function().initial_map();
  }

  const HeapObject heap_object = HeapObject::cast(*this);
  return heap_object.map().GetPrototypeChainRootMap(isolate);
}

Smi Object::GetOrCreateHash(Isolate* isolate) {
  DisallowHeapAllocation no_gc;
  Object hash = Object::GetSimpleHash(*this);
  if (hash.IsSmi()) return Smi::cast(hash);

  DCHECK(IsJSReceiver());
  return JSReceiver::cast(*this).GetOrCreateIdentityHash(isolate);
}

bool Object::SameValue(Object other) {
  if (other == *this) return true;

  if (IsNumber() && other.IsNumber()) {
    return SameNumberValue(Number(), other.Number());
  }
  if (IsString() && other.IsString()) {
    return String::cast(*this).Equals(String::cast(other));
  }
  if (IsBigInt() && other.IsBigInt()) {
    return BigInt::EqualToBigInt(BigInt::cast(*this), BigInt::cast(other));
  }
  return false;
}

bool Object::SameValueZero(Object other) {
  if (other == *this) return true;

  if (IsNumber() && other.IsNumber()) {
    double this_value = Number();
    double other_value = other.Number();
    // +0 == -0 is true
    return this_value == other_value ||
           (std::isnan(this_value) && std::isnan(other_value));
  }
  if (IsString() && other.IsString()) {
    return String::cast(*this).Equals(String::cast(other));
  }
  if (IsBigInt() && other.IsBigInt()) {
    return BigInt::EqualToBigInt(BigInt::cast(*this), BigInt::cast(other));
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
        Object::GetProperty(isolate, original_array,
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
          JSReceiver::GetProperty(isolate,
                                  Handle<JSReceiver>::cast(constructor),
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
V8_WARN_UNUSED_RESULT MaybeHandle<Object> Object::SpeciesConstructor(
    Isolate* isolate, Handle<JSReceiver> recv,
    Handle<JSFunction> default_ctor) {
  Handle<Object> ctor_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor_obj,
      JSObject::GetProperty(isolate, recv,
                            isolate->factory()->constructor_string()),
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
      JSObject::GetProperty(isolate, ctor,
                            isolate->factory()->species_symbol()),
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
  JSArray array = JSArray::cast(*this);
  Isolate* isolate = array.GetIsolate();

#ifdef V8_ENABLE_FORCE_SLOW_PATH
  if (isolate->force_slow_path()) return true;
#endif

  // Check that we have the original ArrayPrototype.
  if (!array.map().prototype().IsJSObject()) return true;
  JSObject array_proto = JSObject::cast(array.map().prototype());
  if (!isolate->is_initial_array_prototype(array_proto)) return true;

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  if (!isolate->IsArrayIteratorLookupChainIntact()) return true;

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  ElementsKind array_kind = array.GetElementsKind();
  if (IsFastPackedElementsKind(array_kind)) return false;

  // For FastHoley kinds, an element access on a hole would cause a lookup on
  // the prototype. This could have different results if the prototype has been
  // changed.
  if (IsHoleyElementsKind(array_kind) &&
      isolate->IsNoElementsProtectorIntact()) {
    return false;
  }
  return true;
}

void Object::ShortPrint(FILE* out) const {
  OFStream os(out);
  os << Brief(*this);
}

void Object::ShortPrint(StringStream* accumulator) const {
  std::ostringstream os;
  os << Brief(*this);
  accumulator->Add(os.str().c_str());
}

void Object::ShortPrint(std::ostream& os) const { os << Brief(*this); }

std::ostream& operator<<(std::ostream& os, const Object& obj) {
  obj.ShortPrint(os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const Brief& v) {
  MaybeObject maybe_object(v.value);
  Smi smi;
  HeapObject heap_object;
  if (maybe_object->ToSmi(&smi)) {
    smi.SmiPrint(os);
  } else if (maybe_object->IsCleared()) {
    os << "[cleared]";
  } else if (maybe_object->GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] ";
    heap_object.HeapObjectShortPrint(os);
  } else if (maybe_object->GetHeapObjectIfStrong(&heap_object)) {
    heap_object.HeapObjectShortPrint(os);
  } else {
    UNREACHABLE();
  }
  return os;
}

void Smi::SmiPrint(std::ostream& os) const {  // NOLINT
  os << value();
}

void HeapObject::HeapObjectShortPrint(std::ostream& os) {  // NOLINT
  os << AsHex::Address(this->ptr()) << " ";

  if (IsString()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(*this).StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject()) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObject::cast(*this).JSObjectShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  switch (map().instance_type()) {
    case MAP_TYPE: {
      os << "<Map";
      Map mapInstance = Map::cast(*this);
      if (mapInstance.IsJSObjectMap()) {
        os << "(" << ElementsKindToString(mapInstance.elements_kind()) << ")";
      } else if (mapInstance.instance_size() != kVariableSizeSentinel) {
        os << "[" << mapInstance.instance_size() << "]";
      }
      os << ">";
    } break;
    case AWAIT_CONTEXT_TYPE: {
      os << "<AwaitContext generator= ";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      Context::cast(*this).extension().ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case BLOCK_CONTEXT_TYPE:
      os << "<BlockContext[" << Context::cast(*this).length() << "]>";
      break;
    case CATCH_CONTEXT_TYPE:
      os << "<CatchContext[" << Context::cast(*this).length() << "]>";
      break;
    case DEBUG_EVALUATE_CONTEXT_TYPE:
      os << "<DebugEvaluateContext[" << Context::cast(*this).length() << "]>";
      break;
    case EVAL_CONTEXT_TYPE:
      os << "<EvalContext[" << Context::cast(*this).length() << "]>";
      break;
    case FUNCTION_CONTEXT_TYPE:
      os << "<FunctionContext[" << Context::cast(*this).length() << "]>";
      break;
    case MODULE_CONTEXT_TYPE:
      os << "<ModuleContext[" << Context::cast(*this).length() << "]>";
      break;
    case NATIVE_CONTEXT_TYPE:
      os << "<NativeContext[" << Context::cast(*this).length() << "]>";
      break;
    case SCRIPT_CONTEXT_TYPE:
      os << "<ScriptContext[" << Context::cast(*this).length() << "]>";
      break;
    case WITH_CONTEXT_TYPE:
      os << "<WithContext[" << Context::cast(*this).length() << "]>";
      break;
    case SCRIPT_CONTEXT_TABLE_TYPE:
      os << "<ScriptContextTable[" << FixedArray::cast(*this).length() << "]>";
      break;
    case HASH_TABLE_TYPE:
      os << "<HashTable[" << FixedArray::cast(*this).length() << "]>";
      break;
    case ORDERED_HASH_MAP_TYPE:
      os << "<OrderedHashMap[" << FixedArray::cast(*this).length() << "]>";
      break;
    case ORDERED_HASH_SET_TYPE:
      os << "<OrderedHashSet[" << FixedArray::cast(*this).length() << "]>";
      break;
    case ORDERED_NAME_DICTIONARY_TYPE:
      os << "<OrderedNameDictionary[" << FixedArray::cast(*this).length()
         << "]>";
      break;
    case NAME_DICTIONARY_TYPE:
      os << "<NameDictionary[" << FixedArray::cast(*this).length() << "]>";
      break;
    case GLOBAL_DICTIONARY_TYPE:
      os << "<GlobalDictionary[" << FixedArray::cast(*this).length() << "]>";
      break;
    case NUMBER_DICTIONARY_TYPE:
      os << "<NumberDictionary[" << FixedArray::cast(*this).length() << "]>";
      break;
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
      os << "<SimpleNumberDictionary[" << FixedArray::cast(*this).length()
         << "]>";
      break;
    case STRING_TABLE_TYPE:
      os << "<StringTable[" << FixedArray::cast(*this).length() << "]>";
      break;
    case FIXED_ARRAY_TYPE:
      os << "<FixedArray[" << FixedArray::cast(*this).length() << "]>";
      break;
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
      os << "<ObjectBoilerplateDescription[" << FixedArray::cast(*this).length()
         << "]>";
      break;
    case FIXED_DOUBLE_ARRAY_TYPE:
      os << "<FixedDoubleArray[" << FixedDoubleArray::cast(*this).length()
         << "]>";
      break;
    case BYTE_ARRAY_TYPE:
      os << "<ByteArray[" << ByteArray::cast(*this).length() << "]>";
      break;
    case BYTECODE_ARRAY_TYPE:
      os << "<BytecodeArray[" << BytecodeArray::cast(*this).length() << "]>";
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      os << "<DescriptorArray["
         << DescriptorArray::cast(*this).number_of_descriptors() << "]>";
      break;
    case TRANSITION_ARRAY_TYPE:
      os << "<TransitionArray[" << TransitionArray::cast(*this).length()
         << "]>";
      break;
    case PROPERTY_ARRAY_TYPE:
      os << "<PropertyArray[" << PropertyArray::cast(*this).length() << "]>";
      break;
    case FEEDBACK_CELL_TYPE: {
      {
        ReadOnlyRoots roots = GetReadOnlyRoots();
        os << "<FeedbackCell[";
        if (map() == roots.no_closures_cell_map()) {
          os << "no feedback";
        } else if (map() == roots.no_closures_cell_map()) {
          os << "no closures";
        } else if (map() == roots.one_closure_cell_map()) {
          os << "one closure";
        } else if (map() == roots.many_closures_cell_map()) {
          os << "many closures";
        } else {
          os << "!!!INVALID MAP!!!";
        }
        os << "]>";
      }
      break;
    }
    case CLOSURE_FEEDBACK_CELL_ARRAY_TYPE:
      os << "<ClosureFeedbackCellArray["
         << ClosureFeedbackCellArray::cast(*this).length() << "]>";
      break;
    case FEEDBACK_VECTOR_TYPE:
      os << "<FeedbackVector[" << FeedbackVector::cast(*this).length() << "]>";
      break;
    case FREE_SPACE_TYPE:
      os << "<FreeSpace[" << FreeSpace::cast(*this).size() << "]>";
      break;

    case PREPARSE_DATA_TYPE: {
      PreparseData data = PreparseData::cast(*this);
      os << "<PreparseData[data=" << data.data_length()
         << " children=" << data.children_length() << "]>";
      break;
    }

    case UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE: {
      UncompiledDataWithoutPreparseData data =
          UncompiledDataWithoutPreparseData::cast(*this);
      os << "<UncompiledDataWithoutPreparseData (" << data.start_position()
         << ", " << data.end_position() << ")]>";
      break;
    }

    case UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE: {
      UncompiledDataWithPreparseData data =
          UncompiledDataWithPreparseData::cast(*this);
      os << "<UncompiledDataWithPreparseData (" << data.start_position() << ", "
         << data.end_position() << ") preparsed=" << Brief(data.preparse_data())
         << ">";
      break;
    }

    case SHARED_FUNCTION_INFO_TYPE: {
      SharedFunctionInfo shared = SharedFunctionInfo::cast(*this);
      std::unique_ptr<char[]> debug_name = shared.DebugName().ToCString();
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
#define MAKE_STRUCT_CASE(TYPE, Name, name)   \
  case TYPE:                                 \
    os << "<" #Name;                         \
    Name::cast(*this).BriefPrintDetails(os); \
    os << ">";                               \
    break;
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
    case ALLOCATION_SITE_TYPE: {
      os << "<AllocationSite";
      AllocationSite::cast(*this).BriefPrintDetails(os);
      os << ">";
      break;
    }
    case SCOPE_INFO_TYPE: {
      ScopeInfo scope = ScopeInfo::cast(*this);
      os << "<ScopeInfo";
      if (scope.length()) os << " " << scope.scope_type() << " ";
      os << "[" << scope.length() << "]>";
      break;
    }
    case CODE_TYPE: {
      Code code = Code::cast(*this);
      os << "<Code " << Code::Kind2String(code.kind());
      if (code.is_builtin()) {
        os << " " << Builtins::name(code.builtin_index());
      }
      os << ">";
      break;
    }
    case ODDBALL_TYPE: {
      if (IsUndefined()) {
        os << "<undefined>";
      } else if (IsTheHole()) {
        os << "<the_hole>";
      } else if (IsNull()) {
        os << "<null>";
      } else if (IsTrue()) {
        os << "<true>";
      } else if (IsFalse()) {
        os << "<false>";
      } else {
        os << "<Odd Oddball: ";
        os << Oddball::cast(*this).to_string().ToCString().get();
        os << ">";
      }
      break;
    }
    case SYMBOL_TYPE: {
      Symbol symbol = Symbol::cast(*this);
      symbol.SymbolShortPrint(os);
      break;
    }
    case HEAP_NUMBER_TYPE: {
      os << "<HeapNumber ";
      HeapNumber::cast(*this).HeapNumberPrint(os);
      os << ">";
      break;
    }
    case MUTABLE_HEAP_NUMBER_TYPE: {
      os << "<MutableHeapNumber ";
      MutableHeapNumber::cast(*this).MutableHeapNumberPrint(os);
      os << '>';
      break;
    }
    case BIGINT_TYPE: {
      os << "<BigInt ";
      BigInt::cast(*this).BigIntShortPrint(os);
      os << ">";
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
      Cell::cast(*this).value().ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case PROPERTY_CELL_TYPE: {
      PropertyCell cell = PropertyCell::cast(*this);
      os << "<PropertyCell name=";
      cell.name().ShortPrint(os);
      os << " value=";
      HeapStringAllocator allocator;
      StringStream accumulator(&allocator);
      cell.value().ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case CALL_HANDLER_INFO_TYPE: {
      CallHandlerInfo info = CallHandlerInfo::cast(*this);
      os << "<CallHandlerInfo ";
      os << "callback= " << Brief(info.callback());
      os << ", js_callback= " << Brief(info.js_callback());
      os << ", data= " << Brief(info.data());
      if (info.IsSideEffectFreeCallHandlerInfo()) {
        os << ", side_effect_free= true>";
      } else {
        os << ", side_effect_free= false>";
      }
      break;
    }
    default:
      os << "<Other heap object (" << map().instance_type() << ")>";
      break;
  }
}

void Struct::BriefPrintDetails(std::ostream& os) {}

void Tuple2::BriefPrintDetails(std::ostream& os) {
  os << " " << Brief(value1()) << ", " << Brief(value2());
}

void Tuple3::BriefPrintDetails(std::ostream& os) {
  os << " " << Brief(value1()) << ", " << Brief(value2()) << ", "
     << Brief(value3());
}

void ClassPositions::BriefPrintDetails(std::ostream& os) {
  os << " " << start() << ", " << end();
}

void ArrayBoilerplateDescription::BriefPrintDetails(std::ostream& os) {
  os << " " << elements_kind() << ", " << Brief(constant_elements());
}

void CallableTask::BriefPrintDetails(std::ostream& os) {
  os << " callable=" << Brief(callable());
}

void HeapObject::Iterate(ObjectVisitor* v) { IterateFast<ObjectVisitor>(v); }

void HeapObject::IterateBody(ObjectVisitor* v) {
  Map m = map();
  IterateBodyFast<ObjectVisitor>(m, SizeFromMap(m), v);
}

void HeapObject::IterateBody(Map map, int object_size, ObjectVisitor* v) {
  IterateBodyFast<ObjectVisitor>(map, object_size, v);
}

struct CallIsValidSlot {
  template <typename BodyDescriptor>
  static bool apply(Map map, HeapObject obj, int offset, int) {
    return BodyDescriptor::IsValidSlot(map, obj, offset);
  }
};

bool HeapObject::IsValidSlot(Map map, int offset) {
  DCHECK_NE(0, offset);
  return BodyDescriptorApply<CallIsValidSlot, bool>(map.instance_type(), map,
                                                    *this, offset, 0);
}

int HeapObject::SizeFromMap(Map map) const {
  int instance_size = map.instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // Only inline the most frequent cases.
  InstanceType instance_type = map.instance_type();
  if (IsInRange(instance_type, FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE)) {
    return FixedArray::SizeFor(
        FixedArray::unchecked_cast(*this).synchronized_length());
  }
  if (IsInRange(instance_type, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)) {
    // Native context has fixed size.
    DCHECK_NE(instance_type, NATIVE_CONTEXT_TYPE);
    return Context::SizeFor(Context::unchecked_cast(*this).length());
  }
  if (instance_type == ONE_BYTE_STRING_TYPE ||
      instance_type == ONE_BYTE_INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqOneByteString::SizeFor(
        SeqOneByteString::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return ByteArray::SizeFor(
        ByteArray::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == BYTECODE_ARRAY_TYPE) {
    return BytecodeArray::SizeFor(
        BytecodeArray::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == FREE_SPACE_TYPE) {
    return FreeSpace::unchecked_cast(*this).relaxed_read_size();
  }
  if (instance_type == STRING_TYPE ||
      instance_type == INTERNALIZED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqTwoByteString::SizeFor(
        SeqTwoByteString::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        FixedDoubleArray::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == FEEDBACK_METADATA_TYPE) {
    return FeedbackMetadata::SizeFor(
        FeedbackMetadata::unchecked_cast(*this).synchronized_slot_count());
  }
  if (instance_type == DESCRIPTOR_ARRAY_TYPE) {
    return DescriptorArray::SizeFor(
        DescriptorArray::unchecked_cast(*this).number_of_all_descriptors());
  }
  if (IsInRange(instance_type, FIRST_WEAK_FIXED_ARRAY_TYPE,
                LAST_WEAK_FIXED_ARRAY_TYPE)) {
    return WeakFixedArray::SizeFor(
        WeakFixedArray::unchecked_cast(*this).synchronized_length());
  }
  if (instance_type == WEAK_ARRAY_LIST_TYPE) {
    return WeakArrayList::SizeForCapacity(
        WeakArrayList::unchecked_cast(*this).synchronized_capacity());
  }
  if (instance_type == SMALL_ORDERED_HASH_SET_TYPE) {
    return SmallOrderedHashSet::SizeFor(
        SmallOrderedHashSet::unchecked_cast(*this).Capacity());
  }
  if (instance_type == SMALL_ORDERED_HASH_MAP_TYPE) {
    return SmallOrderedHashMap::SizeFor(
        SmallOrderedHashMap::unchecked_cast(*this).Capacity());
  }
  if (instance_type == SMALL_ORDERED_NAME_DICTIONARY_TYPE) {
    return SmallOrderedNameDictionary::SizeFor(
        SmallOrderedNameDictionary::unchecked_cast(*this).Capacity());
  }
  if (instance_type == PROPERTY_ARRAY_TYPE) {
    return PropertyArray::SizeFor(
        PropertyArray::cast(*this).synchronized_length());
  }
  if (instance_type == FEEDBACK_VECTOR_TYPE) {
    return FeedbackVector::SizeFor(
        FeedbackVector::unchecked_cast(*this).length());
  }
  if (instance_type == BIGINT_TYPE) {
    return BigInt::SizeFor(BigInt::unchecked_cast(*this).length());
  }
  if (instance_type == PREPARSE_DATA_TYPE) {
    PreparseData data = PreparseData::unchecked_cast(*this);
    return PreparseData::SizeFor(data.data_length(), data.children_length());
  }
  if (instance_type == CODE_TYPE) {
    return Code::unchecked_cast(*this).CodeSize();
  }
  DCHECK_EQ(instance_type, EMBEDDER_DATA_ARRAY_TYPE);
  return EmbedderDataArray::SizeFor(
      EmbedderDataArray::unchecked_cast(*this).length());
}

bool HeapObject::NeedsRehashing() const {
  switch (map().instance_type()) {
    case DESCRIPTOR_ARRAY_TYPE:
      return DescriptorArray::cast(*this).number_of_descriptors() > 1;
    case TRANSITION_ARRAY_TYPE:
      return TransitionArray::cast(*this).number_of_entries() > 1;
    case ORDERED_HASH_MAP_TYPE:
      return OrderedHashMap::cast(*this).NumberOfElements() > 0;
    case ORDERED_HASH_SET_TYPE:
      return OrderedHashSet::cast(*this).NumberOfElements() > 0;
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
    case HASH_TABLE_TYPE:
    case SMALL_ORDERED_HASH_MAP_TYPE:
    case SMALL_ORDERED_HASH_SET_TYPE:
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      return true;
    default:
      return false;
  }
}

bool HeapObject::CanBeRehashed() const {
  DCHECK(NeedsRehashing());
  switch (map().instance_type()) {
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
      // TODO(yangguo): actually support rehashing OrderedHash{Map,Set}.
      return false;
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case STRING_TABLE_TYPE:
      return true;
    case DESCRIPTOR_ARRAY_TYPE:
      return true;
    case TRANSITION_ARRAY_TYPE:
      return true;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      return SmallOrderedHashMap::cast(*this).NumberOfElements() == 0;
    case SMALL_ORDERED_HASH_SET_TYPE:
      return SmallOrderedHashMap::cast(*this).NumberOfElements() == 0;
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      return SmallOrderedNameDictionary::cast(*this).NumberOfElements() == 0;
    default:
      return false;
  }
  return false;
}

void HeapObject::RehashBasedOnMap(ReadOnlyRoots roots) {
  switch (map().instance_type()) {
    case HASH_TABLE_TYPE:
      UNREACHABLE();
    case NAME_DICTIONARY_TYPE:
      NameDictionary::cast(*this).Rehash(roots);
      break;
    case GLOBAL_DICTIONARY_TYPE:
      GlobalDictionary::cast(*this).Rehash(roots);
      break;
    case NUMBER_DICTIONARY_TYPE:
      NumberDictionary::cast(*this).Rehash(roots);
      break;
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
      SimpleNumberDictionary::cast(*this).Rehash(roots);
      break;
    case STRING_TABLE_TYPE:
      StringTable::cast(*this).Rehash(roots);
      break;
    case DESCRIPTOR_ARRAY_TYPE:
      DCHECK_LE(1, DescriptorArray::cast(*this).number_of_descriptors());
      DescriptorArray::cast(*this).Sort();
      break;
    case TRANSITION_ARRAY_TYPE:
      TransitionArray::cast(*this).Sort();
      break;
    case SMALL_ORDERED_HASH_MAP_TYPE:
      DCHECK_EQ(0, SmallOrderedHashMap::cast(*this).NumberOfElements());
      break;
    case SMALL_ORDERED_HASH_SET_TYPE:
      DCHECK_EQ(0, SmallOrderedHashSet::cast(*this).NumberOfElements());
      break;
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      DCHECK_EQ(0, SmallOrderedNameDictionary::cast(*this).NumberOfElements());
      break;
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case INTERNALIZED_STRING_TYPE:
      // Rare case, rehash read-only space strings before they are sealed.
      DCHECK(ReadOnlyHeap::Contains(*this));
      String::cast(*this).Hash();
      break;
    default:
      UNREACHABLE();
  }
}

bool HeapObject::IsExternal(Isolate* isolate) const {
  return map().FindRootMap(isolate) == isolate->heap()->external_map();
}

void DescriptorArray::GeneralizeAllFields() {
  int length = number_of_descriptors();
  for (int i = 0; i < length; i++) {
    PropertyDetails details = GetDetails(i);
    details = details.CopyWithRepresentation(Representation::Tagged());
    if (details.location() == kField) {
      DCHECK_EQ(kData, details.kind());
      details = details.CopyWithConstness(PropertyConstness::kMutable);
      SetValue(i, MaybeObject::FromObject(FieldType::Any()));
    }
    SetDetails(i, details);
  }
}

MaybeHandle<Object> Object::SetProperty(Isolate* isolate, Handle<Object> object,
                                        Handle<Name> name, Handle<Object> value,
                                        StoreOrigin store_origin,
                                        Maybe<ShouldThrow> should_throw) {
  LookupIterator it(isolate, object, name);
  MAYBE_RETURN_NULL(SetProperty(&it, value, store_origin, should_throw));
  return value;
}

Maybe<bool> Object::SetPropertyInternal(LookupIterator* it,
                                        Handle<Object> value,
                                        Maybe<ShouldThrow> should_throw,
                                        StoreOrigin store_origin, bool* found) {
  it->UpdateProtector();
  DCHECK(it->IsFound());

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

      case LookupIterator::JSPROXY: {
        Handle<Object> receiver = it->GetReceiver();
        // In case of global IC, the receiver is the global object. Replace by
        // the global proxy.
        if (receiver->IsJSGlobalObject()) {
          receiver = handle(JSGlobalObject::cast(*receiver).global_proxy(),
                            it->isolate());
        }
        return JSProxy::SetProperty(it->GetHolder<JSProxy>(), it->GetName(),
                                    value, receiver, should_throw);
      }

      case LookupIterator::INTERCEPTOR: {
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(it);
          if (maybe_attributes.IsNothing()) return Nothing<bool>();
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
            AccessorInfo::cast(*accessors).is_special_data_property()) {
          *found = false;
          return Nothing<bool>();
        }
        return SetPropertyWithAccessor(it, value, should_throw);
      }
      case LookupIterator::INTEGER_INDEXED_EXOTIC: {
        // IntegerIndexedElementSet converts value to a Number/BigInt prior to
        // the bounds check. The bounds check has already happened here, but
        // perform the possibly effectful ToNumber (or ToBigInt) operation
        // anyways.
        auto holder = it->GetHolder<JSTypedArray>();
        Handle<Object> throwaway_value;
        if (holder->type() == kExternalBigInt64Array ||
            holder->type() == kExternalBigUint64Array) {
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              it->isolate(), throwaway_value,
              BigInt::FromObject(it->isolate(), value), Nothing<bool>());
        } else {
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              it->isolate(), throwaway_value,
              Object::ToNumber(it->isolate(), value), Nothing<bool>());
        }

        // FIXME: Throw a TypeError if the holder is detached here
        // (IntegerIndexedElementSpec step 5).

        // TODO(verwaest): Per spec, we should return false here (steps 6-9
        // in IntegerIndexedElementSpec), resulting in an exception being thrown
        // on OOB accesses in strict code. Historically, v8 has not done made
        // this change due to uncertainty about web compat. (v8:4901)
        return Just(true);
      }

      case LookupIterator::DATA:
        if (it->IsReadOnly()) {
          return WriteToReadOnlyProperty(it, value, should_throw);
        }
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return SetDataProperty(it, value);
        }
        V8_FALLTHROUGH;
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
                                StoreOrigin store_origin,
                                Maybe<ShouldThrow> should_throw) {
  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, should_throw, store_origin, &found);
    if (found) return result;
  }

  // If the receiver is the JSGlobalObject, the store was contextual. In case
  // the property did not exist yet on the global object itself, we have to
  // throw a reference error in strict mode.  In sloppy mode, we continue.
  if (it->GetReceiver()->IsJSGlobalObject() &&
      (GetShouldThrow(it->isolate(), should_throw) ==
       ShouldThrow::kThrowOnError)) {
    it->isolate()->Throw(*it->isolate()->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, it->GetName()));
    return Nothing<bool>();
  }

  return AddDataProperty(it, value, NONE, should_throw, store_origin);
}

Maybe<bool> Object::SetSuperProperty(LookupIterator* it, Handle<Object> value,
                                     StoreOrigin store_origin,
                                     Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();

  if (it->IsFound()) {
    bool found = true;
    Maybe<bool> result =
        SetPropertyInternal(it, value, should_throw, store_origin, &found);
    if (found) return result;
  }

  it->UpdateProtector();

  // The property either doesn't exist on the holder or exists there as a data
  // property.

  if (!it->GetReceiver()->IsJSReceiver()) {
    return WriteToReadOnlyProperty(it, value, should_throw);
  }
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  LookupIterator::Configuration c = LookupIterator::OWN;
  LookupIterator own_lookup =
      it->IsElement() ? LookupIterator(isolate, receiver, it->index(), c)
                      : LookupIterator(isolate, receiver, it->name(), c);

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
          return Object::SetPropertyWithAccessor(&own_lookup, value,
                                                 should_throw);
        }
        V8_FALLTHROUGH;
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

  return AddDataProperty(&own_lookup, value, NONE, should_throw, store_origin);
}

Maybe<bool> Object::CannotCreateProperty(Isolate* isolate,
                                         Handle<Object> receiver,
                                         Handle<Object> name,
                                         Handle<Object> value,
                                         Maybe<ShouldThrow> should_throw) {
  RETURN_FAILURE(
      isolate, GetShouldThrow(isolate, should_throw),
      NewTypeError(MessageTemplate::kStrictCannotCreateProperty, name,
                   Object::TypeOf(isolate, receiver), receiver));
}

Maybe<bool> Object::WriteToReadOnlyProperty(
    LookupIterator* it, Handle<Object> value,
    Maybe<ShouldThrow> maybe_should_throw) {
  ShouldThrow should_throw = GetShouldThrow(it->isolate(), maybe_should_throw);
  if (it->IsFound() && !it->HolderIsReceiver()) {
    // "Override mistake" attempted, record a use count to track this per
    // v8:8175
    v8::Isolate::UseCounterFeature feature =
        should_throw == kThrowOnError
            ? v8::Isolate::kAttemptOverrideReadOnlyOnPrototypeStrict
            : v8::Isolate::kAttemptOverrideReadOnlyOnPrototypeSloppy;
    it->isolate()->CountUsage(feature);
  }
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

Maybe<bool> Object::RedefineIncompatibleProperty(
    Isolate* isolate, Handle<Object> name, Handle<Object> value,
    Maybe<ShouldThrow> should_throw) {
  RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                 NewTypeError(MessageTemplate::kRedefineDisallowed, name));
}

Maybe<bool> Object::SetDataProperty(LookupIterator* it, Handle<Object> value) {
  DCHECK_IMPLIES(it->GetReceiver()->IsJSProxy(),
                 it->GetName()->IsPrivateName());
  DCHECK_IMPLIES(!it->IsElement() && it->GetName()->IsPrivateName(),
                 it->state() == LookupIterator::DATA);
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  // Store on the holder which may be hidden behind the receiver.
  DCHECK(it->HolderIsReceiverOrHiddenPrototype());

  Handle<Object> to_assign = value;
  // Convert the incoming value to a number for storing into typed arrays.
  if (it->IsElement() && receiver->IsJSObject() &&
      JSObject::cast(*receiver).HasTypedArrayElements()) {
    ElementsKind elements_kind = JSObject::cast(*receiver).GetElementsKind();
    if (elements_kind == BIGINT64_ELEMENTS ||
        elements_kind == BIGUINT64_ELEMENTS) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(it->isolate(), to_assign,
                                       BigInt::FromObject(it->isolate(), value),
                                       Nothing<bool>());
      // We have to recheck the length. However, it can only change if the
      // underlying buffer was detached, so just check that.
      if (Handle<JSArrayBufferView>::cast(receiver)->WasDetached()) {
        return Just(true);
        // TODO(neis): According to the spec, this should throw a TypeError.
      }
    } else if (!value->IsNumber() && !value->IsUndefined(it->isolate())) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(it->isolate(), to_assign,
                                       Object::ToNumber(it->isolate(), value),
                                       Nothing<bool>());
      // We have to recheck the length. However, it can only change if the
      // underlying buffer was detached, so just check that.
      if (Handle<JSArrayBufferView>::cast(receiver)->WasDetached()) {
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
    receiver->HeapObjectVerify(it->isolate());
  }
#endif
  return Just(true);
}

Maybe<bool> Object::AddDataProperty(LookupIterator* it, Handle<Object> value,
                                    PropertyAttributes attributes,
                                    Maybe<ShouldThrow> should_throw,
                                    StoreOrigin store_origin) {
  if (!it->GetReceiver()->IsJSReceiver()) {
    return CannotCreateProperty(it->isolate(), it->GetReceiver(), it->GetName(),
                                value, should_throw);
  }

  // Private symbols should be installed on JSProxy using
  // JSProxy::SetPrivateSymbol.
  if (it->GetReceiver()->IsJSProxy() && it->GetName()->IsPrivate() &&
      !it->GetName()->IsPrivateName()) {
    RETURN_FAILURE(it->isolate(), GetShouldThrow(it->isolate(), should_throw),
                   NewTypeError(MessageTemplate::kProxyPrivate));
  }

  DCHECK_NE(LookupIterator::INTEGER_INDEXED_EXOTIC, it->state());

  Handle<JSReceiver> receiver = it->GetStoreTarget<JSReceiver>();
  DCHECK_IMPLIES(receiver->IsJSProxy(), it->GetName()->IsPrivateName());
  DCHECK_IMPLIES(receiver->IsJSProxy(),
                 it->state() == LookupIterator::NOT_FOUND);

  // If the receiver is a JSGlobalProxy, store on the prototype (JSGlobalObject)
  // instead. If the prototype is Null, the proxy is detached.
  if (receiver->IsJSGlobalProxy()) return Just(true);

  Isolate* isolate = it->isolate();

  if (it->ExtendingNonExtensible(receiver)) {
    RETURN_FAILURE(
        isolate, GetShouldThrow(it->isolate(), should_throw),
        NewTypeError(MessageTemplate::kObjectNotExtensible, it->GetName()));
  }

  if (it->IsElement()) {
    if (receiver->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(receiver);
      if (JSArray::WouldChangeReadOnlyLength(array, it->index())) {
        RETURN_FAILURE(isolate, GetShouldThrow(it->isolate(), should_throw),
                       NewTypeError(MessageTemplate::kStrictReadOnlyProperty,
                                    isolate->factory()->length_string(),
                                    Object::TypeOf(isolate, array), array));
      }
    }

    Handle<JSObject> receiver_obj = Handle<JSObject>::cast(receiver);
    JSObject::AddDataElement(receiver_obj, it->index(), value, attributes);
    JSObject::ValidateElements(*receiver_obj);
    return Just(true);
  } else {
    it->UpdateProtector();
    // Migrate to the most up-to-date map that will be able to store |value|
    // under it->name() with |attributes|.
    it->PrepareTransitionToDataProperty(receiver, value, attributes,
                                        store_origin);
    DCHECK_EQ(LookupIterator::TRANSITION, it->state());
    it->ApplyTransitionToDataProperty(receiver);

    // Write the property value.
    it->WriteDataValue(value, true);

#if VERIFY_HEAP
    if (FLAG_verify_heap) {
      receiver->HeapObjectVerify(isolate);
    }
#endif
  }

  return Just(true);
}

template <class T>
static int AppendUniqueCallbacks(Isolate* isolate,
                                 Handle<TemplateList> callbacks,
                                 Handle<typename T::Array> array,
                                 int valid_descriptors) {
  int nof_callbacks = callbacks->length();

  // Fill in new callback descriptors.  Process the callbacks from
  // back to front so that the last callback with a given name takes
  // precedence over previously added callbacks with that name.
  for (int i = nof_callbacks - 1; i >= 0; i--) {
    Handle<AccessorInfo> entry(AccessorInfo::cast(callbacks->get(i)), isolate);
    Handle<Name> key(Name::cast(entry->name()), isolate);
    DCHECK(key->IsUniqueName());
    // Check if a descriptor with this name already exists before writing.
    if (!T::Contains(key, entry, valid_descriptors, array)) {
      T::Insert(key, entry, valid_descriptors, array);
      valid_descriptors++;
    }
  }

  return valid_descriptors;
}

struct FixedArrayAppender {
  using Array = FixedArray;
  static bool Contains(Handle<Name> key, Handle<AccessorInfo> entry,
                       int valid_descriptors, Handle<FixedArray> array) {
    for (int i = 0; i < valid_descriptors; i++) {
      if (*key == AccessorInfo::cast(array->get(i)).name()) return true;
    }
    return false;
  }
  static void Insert(Handle<Name> key, Handle<AccessorInfo> entry,
                     int valid_descriptors, Handle<FixedArray> array) {
    DisallowHeapAllocation no_gc;
    array->set(valid_descriptors, *entry);
  }
};

int AccessorInfo::AppendUnique(Isolate* isolate, Handle<Object> descriptors,
                               Handle<FixedArray> array,
                               int valid_descriptors) {
  Handle<TemplateList> callbacks = Handle<TemplateList>::cast(descriptors);
  DCHECK_GE(array->length(), callbacks->length() + valid_descriptors);
  return AppendUniqueCallbacks<FixedArrayAppender>(isolate, callbacks, array,
                                                   valid_descriptors);
}

void JSProxy::Revoke(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  // ES#sec-proxy-revocation-functions
  if (!proxy->IsRevoked()) {
    // 5. Set p.[[ProxyTarget]] to null.
    proxy->set_target(ReadOnlyRoots(isolate).null_value());
    // 6. Set p.[[ProxyHandler]] to null.
    proxy->set_handler(ReadOnlyRoots(isolate).null_value());
  }
  DCHECK(proxy->IsRevoked());
}

// static
Maybe<bool> JSProxy::IsArray(Handle<JSProxy> proxy) {
  Isolate* isolate = proxy->GetIsolate();
  Handle<JSReceiver> object = Handle<JSReceiver>::cast(proxy);
  for (int i = 0; i < JSProxy::kMaxIterationLimit; i++) {
    Handle<JSProxy> proxy = Handle<JSProxy>::cast(object);
    if (proxy->IsRevoked()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyRevoked,
          isolate->factory()->NewStringFromAsciiChecked("IsArray")));
      return Nothing<bool>();
    }
    object = handle(JSReceiver::cast(proxy->target()), isolate);
    if (object->IsJSArray()) return Just(true);
    if (!object->IsJSProxy()) return Just(false);
  }

  // Too deep recursion, throw a RangeError.
  isolate->StackOverflow();
  return Nothing<bool>();
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "has").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler),
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
  bool boolean_trap_result = trap_result_obj->BooleanValue(isolate);
  // 9. If booleanTrapResult is false, then:
  if (!boolean_trap_result) {
    MAYBE_RETURN(JSProxy::CheckHasTrap(isolate, name, target), Nothing<bool>());
  }
  // 10. Return booleanTrapResult.
  return Just(boolean_trap_result);
}

Maybe<bool> JSProxy::CheckHasTrap(Isolate* isolate, Handle<Name> name,
                                  Handle<JSReceiver> target) {
  // 9a. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
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
  return Just(true);
}

Maybe<bool> JSProxy::SetProperty(Handle<JSProxy> proxy, Handle<Name> name,
                                 Handle<Object> value, Handle<Object> receiver,
                                 Maybe<ShouldThrow> should_throw) {
  DCHECK(!name->IsPrivate());
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->set_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  Handle<JSReceiver> handler(JSReceiver::cast(proxy->handler()), isolate);

  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap, Object::GetMethod(handler, trap_name), Nothing<bool>());
  if (trap->IsUndefined(isolate)) {
    LookupIterator it =
        LookupIterator::PropertyOrElement(isolate, receiver, name, target);

    return Object::SetSuperProperty(&it, value, StoreOrigin::kMaybeKeyed,
                                    should_throw);
  }

  Handle<Object> trap_result;
  Handle<Object> args[] = {target, name, value, receiver};
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, trap_result,
      Execution::Call(isolate, trap, handler, arraysize(args), args),
      Nothing<bool>());
  if (!trap_result->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  MaybeHandle<Object> result =
      JSProxy::CheckGetSetTrapResult(isolate, name, target, value, kSet);

  if (result.is_null()) {
    return Nothing<bool>();
  }
  return Just(true);
}

Maybe<bool> JSProxy::DeletePropertyOrElement(Handle<JSProxy> proxy,
                                             Handle<Name> name,
                                             LanguageMode language_mode) {
  DCHECK(!name->IsPrivate());
  ShouldThrow should_throw =
      is_sloppy(language_mode) ? kDontThrow : kThrowOnError;
  Isolate* isolate = proxy->GetIsolate();
  STACK_CHECK(isolate, Nothing<bool>());
  Factory* factory = isolate->factory();
  Handle<String> trap_name = factory->deleteProperty_string();

  if (proxy->IsRevoked()) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyRevoked, trap_name));
    return Nothing<bool>();
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  if (!trap_result->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, should_throw,
                   NewTypeError(MessageTemplate::kProxyTrapReturnedFalsishFor,
                                trap_name, name));
  }

  // Enforce the invariant.
  return JSProxy::CheckDeleteTrap(isolate, name, target);
}

Maybe<bool> JSProxy::CheckDeleteTrap(Isolate* isolate, Handle<Name> name,
                                     Handle<JSReceiver> target) {
  // 10. Let targetDesc be ? target.[[GetOwnProperty]](P).
  PropertyDescriptor target_desc;
  Maybe<bool> target_found =
      JSReceiver::GetOwnPropertyDescriptor(isolate, target, name, &target_desc);
  MAYBE_RETURN(target_found, Nothing<bool>());
  // 11. If targetDesc is undefined, return true.
  if (target_found.FromJust()) {
    // 12. If targetDesc.[[Configurable]] is false, throw a TypeError exception.
    if (!target_desc.configurable()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDeletePropertyNonConfigurable, name));
      return Nothing<bool>();
    }
    // 13. Let extensibleTarget be ? IsExtensible(target).
    Maybe<bool> extensible_target = JSReceiver::IsExtensible(target);
    MAYBE_RETURN(extensible_target, Nothing<bool>());
    // 14. If extensibleTarget is false, throw a TypeError exception.
    if (!extensible_target.FromJust()) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kProxyDeletePropertyNonExtensible, name));
      return Nothing<bool>();
    }
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
  if (target->IsJSProxy() && JSProxy::cast(*target).IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  if (!handler->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  if (handler->IsJSProxy() && JSProxy::cast(*handler).IsRevoked()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kProxyHandlerOrTargetRevoked),
                    JSProxy);
  }
  return isolate->factory()->NewJSProxy(Handle<JSReceiver>::cast(target),
                                        Handle<JSReceiver>::cast(handler));
}

// static
MaybeHandle<NativeContext> JSProxy::GetFunctionRealm(Handle<JSProxy> proxy) {
  DCHECK(proxy->map().is_constructor());
  if (proxy->IsRevoked()) {
    THROW_NEW_ERROR(proxy->GetIsolate(),
                    NewTypeError(MessageTemplate::kProxyRevoked),
                    NativeContext);
  }
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()),
                            proxy->GetIsolate());
  return JSReceiver::GetFunctionRealm(target);
}

Maybe<PropertyAttributes> JSProxy::GetPropertyAttributes(LookupIterator* it) {
  PropertyDescriptor desc;
  Maybe<bool> found = JSProxy::GetOwnPropertyDescriptor(
      it->isolate(), it->GetHolder<JSProxy>(), it->GetName(), &desc);
  MAYBE_RETURN(found, Nothing<PropertyAttributes>());
  if (!found.FromJust()) return Just(ABSENT);
  return Just(desc.ToAttributes());
}

// TODO(jkummerow): Consider unification with FastAsArrayLength() in
// accessors.cc.
bool PropertyKeyToArrayLength(Handle<Object> value, uint32_t* length) {
  DCHECK(value->IsNumber() || value->IsName());
  if (value->ToArrayLength(length)) return true;
  if (value->IsString()) return String::cast(*value).AsArrayIndex(length);
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
                                       Maybe<ShouldThrow> should_throw) {
  // 1. Assert: IsPropertyKey(P) is true. ("P" is |name|.)
  // 2. If P is "length", then:
  // TODO(jkummerow): Check if we need slow string comparison.
  if (*name == ReadOnlyRoots(isolate).length_string()) {
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
      RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                     NewTypeError(MessageTemplate::kDefineDisallowed, name));
    }
    // 3g. Let succeeded be OrdinaryDefineOwnProperty(A, P, Desc).
    Maybe<bool> succeeded =
        OrdinaryDefineOwnProperty(isolate, o, name, desc, should_throw);
    // 3h. Assert: succeeded is not an abrupt completion.
    //     In our case, if should_throw == kThrowOnError, it can be!
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
  if (!Object::ToNumber(isolate, length_object).ToHandle(&number_v)) {
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
                                    Maybe<ShouldThrow> should_throw) {
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
  if (!old_len_desc.writable() ||
      // Also handle the {configurable: true} case since we later use
      // JSArray::SetLength instead of OrdinaryDefineOwnProperty to change
      // the length, and it doesn't have access to the descriptor anymore.
      new_len_desc->configurable()) {
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
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
  CHECK(a->length().ToArrayLength(&actual_new_len));
  // Steps 19d-v, 21. Return false if there were non-deletable elements.
  bool result = actual_new_len == new_len;
  if (!result) {
    RETURN_FAILURE(
        isolate, GetShouldThrow(isolate, should_throw),
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
                                       Maybe<ShouldThrow> should_throw) {
  STACK_CHECK(isolate, Nothing<bool>());
  if (key->IsSymbol() && Handle<Symbol>::cast(key)->IsPrivate()) {
    DCHECK(!Handle<Symbol>::cast(key)->IsPrivateName());
    return JSProxy::SetPrivateSymbol(isolate, proxy, Handle<Symbol>::cast(key),
                                     desc, should_throw);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  if (!trap_result_obj->BooleanValue(isolate)) {
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
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
    Maybe<bool> valid = IsCompatiblePropertyDescriptor(
        isolate, extensible_target, desc, &target_desc, property_name,
        Just(kDontThrow));
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
    // 16c. If IsDataDescriptor(targetDesc) is true,
    // targetDesc.[[Configurable]] is
    //       false, and targetDesc.[[Writable]] is true, then
    if (PropertyDescriptor::IsDataDescriptor(&target_desc) &&
        !target_desc.configurable() && target_desc.writable()) {
      // 16c i. If Desc has a [[Writable]] field and Desc.[[Writable]] is false,
      // throw a TypeError exception.
      if (desc->has_writable() && !desc->writable()) {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::kProxyDefinePropertyNonConfigurableWritable,
            property_name));
        return Nothing<bool>();
      }
    }
  }
  // 17. Return true.
  return Just(true);
}

// static
Maybe<bool> JSProxy::SetPrivateSymbol(Isolate* isolate, Handle<JSProxy> proxy,
                                      Handle<Symbol> private_name,
                                      PropertyDescriptor* desc,
                                      Maybe<ShouldThrow> should_throw) {
  DCHECK(!private_name->IsPrivateName());
  // Despite the generic name, this can only add private data properties.
  if (!PropertyDescriptor::IsDataDescriptor(desc) ||
      desc->ToAttributes() != DONT_ENUM) {
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                   NewTypeError(MessageTemplate::kProxyPrivate));
  }
  DCHECK(proxy->map().is_dictionary_map());
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

  Handle<NameDictionary> dict(proxy->property_dictionary(), isolate);
  PropertyDetails details(kData, DONT_ENUM, PropertyCellType::kNoCell);
  Handle<NameDictionary> result =
      NameDictionary::Add(isolate, dict, private_name, value, details);
  if (!dict.is_identical_to(result)) proxy->SetProperties(*result);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  Maybe<bool> valid = IsCompatiblePropertyDescriptor(
      isolate, extensible_target.FromJust(), desc, &target_desc, name,
      Just(kDontThrow));
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
    // 17b. If resultDesc has a [[Writable]] field and resultDesc.[[Writable]]
    // is false, then
    if (desc->has_writable() && !desc->writable()) {
      // 17b i. If targetDesc.[[Writable]] is true, throw a TypeError exception.
      if (target_desc.writable()) {
        isolate->Throw(*isolate->factory()->NewTypeError(
            MessageTemplate::
                kProxyGetOwnPropertyDescriptorNonConfigurableWritable,
            name));
        return Nothing<bool>();
      }
    }
  }
  // 18. Return resultDesc.
  return Just(true);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  if (!trap_result->BooleanValue(isolate)) {
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  if (target_result.FromJust() != trap_result->BooleanValue(isolate)) {
    isolate->Throw(
        *factory->NewTypeError(MessageTemplate::kProxyIsExtensibleInconsistent,
                               factory->ToBoolean(target_result.FromJust())));
    return Nothing<bool>();
  }
  return target_result;
}

Handle<DescriptorArray> DescriptorArray::CopyUpTo(Isolate* isolate,
                                                  Handle<DescriptorArray> desc,
                                                  int enumeration_index,
                                                  int slack) {
  return DescriptorArray::CopyUpToAddAttributes(isolate, desc,
                                                enumeration_index, NONE, slack);
}

Handle<DescriptorArray> DescriptorArray::CopyUpToAddAttributes(
    Isolate* isolate, Handle<DescriptorArray> desc, int enumeration_index,
    PropertyAttributes attributes, int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;

  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, size, slack);

  if (attributes != NONE) {
    for (int i = 0; i < size; ++i) {
      MaybeObject value_or_field_type = desc->GetValue(i);
      Name key = desc->GetKey(i);
      PropertyDetails details = desc->GetDetails(i);
      // Bulk attribute changes never affect private properties.
      if (!key.IsPrivate()) {
        int mask = DONT_DELETE | DONT_ENUM;
        // READ_ONLY is an invalid attribute for JS setters/getters.
        HeapObject heap_object;
        if (details.kind() != kAccessor ||
            !(value_or_field_type->GetHeapObjectIfStrong(&heap_object) &&
              heap_object.IsAccessorPair())) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      descriptors->Set(i, key, value_or_field_type, details);
    }
  } else {
    for (int i = 0; i < size; ++i) {
      descriptors->CopyFrom(i, *desc);
    }
  }

  if (desc->number_of_descriptors() != enumeration_index) descriptors->Sort();

  return descriptors;
}

// Create a new descriptor array with only enumerable, configurable, writeable
// data properties, but identical field locations.
Handle<DescriptorArray> DescriptorArray::CopyForFastObjectClone(
    Isolate* isolate, Handle<DescriptorArray> src, int enumeration_index,
    int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;
  Handle<DescriptorArray> descriptors =
      DescriptorArray::Allocate(isolate, size, slack);

  for (int i = 0; i < size; ++i) {
    Name key = src->GetKey(i);
    PropertyDetails details = src->GetDetails(i);

    DCHECK(!key.IsPrivateName());
    DCHECK(details.IsEnumerable());
    DCHECK_EQ(details.kind(), kData);

    // Ensure the ObjectClone property details are NONE, and that all source
    // details did not contain DONT_ENUM.
    PropertyDetails new_details(kData, NONE, details.location(),
                                details.constness(), details.representation(),
                                details.field_index());
    // Do not propagate the field type of normal object fields from the
    // original descriptors since FieldType changes don't create new maps.
    MaybeObject type = src->GetValue(i);
    if (details.location() == PropertyLocation::kField) {
      type = MaybeObject::FromObject(FieldType::Any());
      // TODO(bmeurer,ishell): Igor suggested to use some kind of dynamic
      // checks in the fast-path for CloneObjectIC instead to avoid the
      // need to generalize the descriptors here. That will also enable
      // us to skip the defensive copying of the target map whenever a
      // CloneObjectIC misses.
      if (FLAG_modify_field_representation_inplace &&
          (new_details.representation().IsSmi() ||
           new_details.representation().IsHeapObject())) {
        new_details =
            new_details.CopyWithRepresentation(Representation::Tagged());
      }
    }
    descriptors->Set(i, key, type, new_details);
  }

  descriptors->Sort();

  return descriptors;
}

bool DescriptorArray::IsEqualUpTo(DescriptorArray desc, int nof_descriptors) {
  for (int i = 0; i < nof_descriptors; i++) {
    if (GetKey(i) != desc.GetKey(i) || GetValue(i) != desc.GetValue(i)) {
      return false;
    }
    PropertyDetails details = GetDetails(i);
    PropertyDetails other_details = desc.GetDetails(i);
    if (details.kind() != other_details.kind() ||
        details.location() != other_details.location() ||
        !details.representation().Equals(other_details.representation())) {
      return false;
    }
  }
  return true;
}

Handle<FixedArray> FixedArray::SetAndGrow(Isolate* isolate,
                                          Handle<FixedArray> array, int index,
                                          Handle<Object> value,
                                          AllocationType allocation) {
  if (index < array->length()) {
    array->set(index, *value);
    return array;
  }
  int capacity = array->length();
  do {
    capacity = JSObject::NewElementsCapacity(capacity);
  } while (capacity <= index);
  Handle<FixedArray> new_array =
      isolate->factory()->NewUninitializedFixedArray(capacity, allocation);
  array->CopyTo(0, *new_array, 0, array->length());
  new_array->FillWithHoles(array->length(), new_array->length());
  new_array->set(index, *value);
  return new_array;
}

Handle<FixedArray> FixedArray::ShrinkOrEmpty(Isolate* isolate,
                                             Handle<FixedArray> array,
                                             int new_length) {
  if (new_length == 0) {
    return array->GetReadOnlyRoots().empty_fixed_array_handle();
  } else {
    array->Shrink(isolate, new_length);
    return array;
  }
}

void FixedArray::Shrink(Isolate* isolate, int new_length) {
  DCHECK(0 < new_length && new_length <= length());
  if (new_length < length()) {
    isolate->heap()->RightTrimFixedArray(*this, length() - new_length);
  }
}

void FixedArray::CopyTo(int pos, FixedArray dest, int dest_pos, int len) const {
  DisallowHeapAllocation no_gc;
  // Return early if len == 0 so that we don't try to read the write barrier off
  // a canonical read-only empty fixed array.
  if (len == 0) return;
  WriteBarrierMode mode = dest.GetWriteBarrierMode(no_gc);
  for (int index = 0; index < len; index++) {
    dest.set(dest_pos + index, get(pos + index), mode);
  }
}

// static
Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 1);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  array->Set(length, *obj);
  array->SetLength(length + 1);
  return array;
}

// static
Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj1, Handle<Object> obj2) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 2);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  array->Set(length, *obj1);
  array->Set(length + 1, *obj2);
  array->SetLength(length + 2);
  return array;
}

// static
Handle<ArrayList> ArrayList::New(Isolate* isolate, int size) {
  Handle<FixedArray> fixed_array =
      isolate->factory()->NewFixedArray(size + kFirstIndex);
  fixed_array->set_map_no_write_barrier(
      ReadOnlyRoots(isolate).array_list_map());
  Handle<ArrayList> result = Handle<ArrayList>::cast(fixed_array);
  result->SetLength(0);
  return result;
}

Handle<FixedArray> ArrayList::Elements(Isolate* isolate,
                                       Handle<ArrayList> array) {
  int length = array->Length();
  Handle<FixedArray> result = isolate->factory()->NewFixedArray(length);
  // Do not copy the first entry, i.e., the length.
  array->CopyTo(kFirstIndex, *result, 0, length);
  return result;
}

namespace {

Handle<FixedArray> EnsureSpaceInFixedArray(Isolate* isolate,
                                           Handle<FixedArray> array,
                                           int length) {
  int capacity = array->length();
  if (capacity < length) {
    int new_capacity = length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array = isolate->factory()->CopyFixedArrayAndGrow(array, grow_by);
  }
  return array;
}

}  // namespace

// static
Handle<ArrayList> ArrayList::EnsureSpace(Isolate* isolate,
                                         Handle<ArrayList> array, int length) {
  const bool empty = (array->length() == 0);
  Handle<FixedArray> ret =
      EnsureSpaceInFixedArray(isolate, array, kFirstIndex + length);
  if (empty) {
    ret->set_map_no_write_barrier(array->GetReadOnlyRoots().array_list_map());

    Handle<ArrayList>::cast(ret)->SetLength(0);
  }
  return Handle<ArrayList>::cast(ret);
}

// static
Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              const MaybeObjectHandle& value) {
  int length = array->length();
  array = EnsureSpace(isolate, array, length + 1);
  // Reload length; GC might have removed elements from the array.
  length = array->length();
  array->Set(length, *value);
  array->set_length(length + 1);
  return array;
}

bool WeakArrayList::IsFull() { return length() == capacity(); }

// static
Handle<WeakArrayList> WeakArrayList::EnsureSpace(Isolate* isolate,
                                                 Handle<WeakArrayList> array,
                                                 int length,
                                                 AllocationType allocation) {
  int capacity = array->capacity();
  if (capacity < length) {
    int new_capacity = length;
    new_capacity = new_capacity + Max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array = isolate->factory()->CopyWeakArrayListAndGrow(array, grow_by,
                                                         allocation);
  }
  return array;
}

int WeakArrayList::CountLiveWeakReferences() const {
  int live_weak_references = 0;
  for (int i = 0; i < length(); i++) {
    if (Get(i)->IsWeak()) {
      ++live_weak_references;
    }
  }
  return live_weak_references;
}

bool WeakArrayList::RemoveOne(const MaybeObjectHandle& value) {
  if (length() == 0) return false;
  // Optimize for the most recently added element to be removed again.
  MaybeObject cleared_weak_ref =
      HeapObjectReference::ClearedValue(GetIsolate());
  int last_index = length() - 1;
  for (int i = last_index; i >= 0; --i) {
    if (Get(i) == *value) {
      // Move the last element into the this slot (or no-op, if this is the
      // last slot).
      Set(i, Get(last_index));
      Set(last_index, cleared_weak_ref);
      set_length(last_index);
      return true;
    }
  }
  return false;
}

// static
Handle<WeakArrayList> PrototypeUsers::Add(Isolate* isolate,
                                          Handle<WeakArrayList> array,
                                          Handle<Map> value,
                                          int* assigned_index) {
  int length = array->length();
  if (length == 0) {
    // Uninitialized WeakArrayList; need to initialize empty_slot_index.
    array = WeakArrayList::EnsureSpace(isolate, array, kFirstIndex + 1);
    set_empty_slot_index(*array, kNoEmptySlotsMarker);
    array->Set(kFirstIndex, HeapObjectReference::Weak(*value));
    array->set_length(kFirstIndex + 1);
    if (assigned_index != nullptr) *assigned_index = kFirstIndex;
    return array;
  }

  // If the array has unfilled space at the end, use it.
  if (!array->IsFull()) {
    array->Set(length, HeapObjectReference::Weak(*value));
    array->set_length(length + 1);
    if (assigned_index != nullptr) *assigned_index = length;
    return array;
  }

  // If there are empty slots, use one of them.
  int empty_slot = Smi::ToInt(empty_slot_index(*array));
  if (empty_slot != kNoEmptySlotsMarker) {
    DCHECK_GE(empty_slot, kFirstIndex);
    CHECK_LT(empty_slot, array->length());
    int next_empty_slot = array->Get(empty_slot).ToSmi().value();

    array->Set(empty_slot, HeapObjectReference::Weak(*value));
    if (assigned_index != nullptr) *assigned_index = empty_slot;

    set_empty_slot_index(*array, next_empty_slot);
    return array;
  } else {
    DCHECK_EQ(empty_slot, kNoEmptySlotsMarker);
  }

  // Array full and no empty slots. Grow the array.
  array = WeakArrayList::EnsureSpace(isolate, array, length + 1);
  array->Set(length, HeapObjectReference::Weak(*value));
  array->set_length(length + 1);
  if (assigned_index != nullptr) *assigned_index = length;
  return array;
}

WeakArrayList PrototypeUsers::Compact(Handle<WeakArrayList> array, Heap* heap,
                                      CompactionCallback callback,
                                      AllocationType allocation) {
  if (array->length() == 0) {
    return *array;
  }
  int new_length = kFirstIndex + array->CountLiveWeakReferences();
  if (new_length == array->length()) {
    return *array;
  }

  Handle<WeakArrayList> new_array = WeakArrayList::EnsureSpace(
      heap->isolate(),
      handle(ReadOnlyRoots(heap).empty_weak_array_list(), heap->isolate()),
      new_length, allocation);
  // Allocation might have caused GC and turned some of the elements into
  // cleared weak heap objects. Count the number of live objects again.
  int copy_to = kFirstIndex;
  for (int i = kFirstIndex; i < array->length(); i++) {
    MaybeObject element = array->Get(i);
    HeapObject value;
    if (element->GetHeapObjectIfWeak(&value)) {
      callback(value, i, copy_to);
      new_array->Set(copy_to++, element);
    } else {
      DCHECK(element->IsCleared() || element->IsSmi());
    }
  }
  new_array->set_length(copy_to);
  set_empty_slot_index(*new_array, kNoEmptySlotsMarker);
  return *new_array;
}

Handle<RegExpMatchInfo> RegExpMatchInfo::ReserveCaptures(
    Isolate* isolate, Handle<RegExpMatchInfo> match_info, int capture_count) {
  DCHECK_GE(match_info->length(), kLastMatchOverhead);
  const int required_length = kFirstCaptureIndex + capture_count;
  return Handle<RegExpMatchInfo>::cast(
      EnsureSpaceInFixedArray(isolate, match_info, required_length));
}

// static
Handle<FrameArray> FrameArray::AppendJSFrame(Handle<FrameArray> in,
                                             Handle<Object> receiver,
                                             Handle<JSFunction> function,
                                             Handle<AbstractCode> code,
                                             int offset, int flags,
                                             Handle<FixedArray> parameters) {
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array =
      EnsureSpace(function->GetIsolate(), in, new_length);
  array->SetReceiver(frame_count, *receiver);
  array->SetFunction(frame_count, *function);
  array->SetCode(frame_count, *code);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->SetParameters(frame_count, *parameters);
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

// static
Handle<FrameArray> FrameArray::AppendWasmFrame(
    Handle<FrameArray> in, Handle<WasmInstanceObject> wasm_instance,
    int wasm_function_index, wasm::WasmCode* code, int offset, int flags) {
  Isolate* isolate = wasm_instance->GetIsolate();
  const int frame_count = in->FrameCount();
  const int new_length = LengthFor(frame_count + 1);
  Handle<FrameArray> array = EnsureSpace(isolate, in, new_length);
  // The {code} will be {nullptr} for interpreted wasm frames.
  Handle<Object> code_ref = isolate->factory()->undefined_value();
  if (code) {
    auto native_module = wasm_instance->module_object().shared_native_module();
    code_ref = Managed<wasm::GlobalWasmCodeRef>::Allocate(
        isolate, 0, code, std::move(native_module));
  }
  array->SetWasmInstance(frame_count, *wasm_instance);
  array->SetWasmFunctionIndex(frame_count, Smi::FromInt(wasm_function_index));
  array->SetWasmCodeObject(frame_count, *code_ref);
  array->SetOffset(frame_count, Smi::FromInt(offset));
  array->SetFlags(frame_count, Smi::FromInt(flags));
  array->set(kFrameCountIndex, Smi::FromInt(frame_count + 1));
  return array;
}

void FrameArray::ShrinkToFit(Isolate* isolate) {
  Shrink(isolate, LengthFor(FrameCount()));
}

// static
Handle<FrameArray> FrameArray::EnsureSpace(Isolate* isolate,
                                           Handle<FrameArray> array,
                                           int length) {
  return Handle<FrameArray>::cast(
      EnsureSpaceInFixedArray(isolate, array, length));
}

Handle<DescriptorArray> DescriptorArray::Allocate(Isolate* isolate,
                                                  int nof_descriptors,
                                                  int slack,
                                                  AllocationType allocation) {
  return nof_descriptors + slack == 0
             ? isolate->factory()->empty_descriptor_array()
             : isolate->factory()->NewDescriptorArray(nof_descriptors, slack,
                                                      allocation);
}

void DescriptorArray::Initialize(EnumCache enum_cache,
                                 HeapObject undefined_value,
                                 int nof_descriptors, int slack) {
  DCHECK_GE(nof_descriptors, 0);
  DCHECK_GE(slack, 0);
  DCHECK_LE(nof_descriptors + slack, kMaxNumberOfDescriptors);
  set_number_of_all_descriptors(nof_descriptors + slack);
  set_number_of_descriptors(nof_descriptors);
  set_raw_number_of_marked_descriptors(0);
  set_filler16bits(0);
  set_enum_cache(enum_cache);
  MemsetTagged(GetDescriptorSlot(0), undefined_value,
               number_of_all_descriptors() * kEntrySize);
}

void DescriptorArray::ClearEnumCache() {
  set_enum_cache(GetReadOnlyRoots().empty_enum_cache());
}

void DescriptorArray::Replace(int index, Descriptor* descriptor) {
  descriptor->SetSortedKeyIndex(GetSortedKeyIndex(index));
  Set(index, descriptor);
}

// static
void DescriptorArray::InitializeOrChangeEnumCache(
    Handle<DescriptorArray> descriptors, Isolate* isolate,
    Handle<FixedArray> keys, Handle<FixedArray> indices) {
  EnumCache enum_cache = descriptors->enum_cache();
  if (enum_cache == ReadOnlyRoots(isolate).empty_enum_cache()) {
    enum_cache = *isolate->factory()->NewEnumCache(keys, indices);
    descriptors->set_enum_cache(enum_cache);
  } else {
    enum_cache.set_keys(*keys);
    enum_cache.set_indices(*indices);
  }
}

void DescriptorArray::CopyFrom(int index, DescriptorArray src) {
  PropertyDetails details = src.GetDetails(index);
  Set(index, src.GetKey(index), src.GetValue(index), details);
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
    const uint32_t parent_hash = GetSortedKey(i).Hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetSortedKey(child_index).Hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1).Hash();
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
    const uint32_t parent_hash = GetSortedKey(parent_index).Hash();
    const int max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetSortedKey(child_index).Hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1).Hash();
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

int16_t DescriptorArray::UpdateNumberOfMarkedDescriptors(
    unsigned mark_compact_epoch, int16_t new_marked) {
  STATIC_ASSERT(kMaxNumberOfDescriptors <=
                NumberOfMarkedDescriptors::kMaxNumberOfMarkedDescriptors);
  int16_t old_raw_marked = raw_number_of_marked_descriptors();
  int16_t old_marked =
      NumberOfMarkedDescriptors::decode(mark_compact_epoch, old_raw_marked);
  int16_t new_raw_marked =
      NumberOfMarkedDescriptors::encode(mark_compact_epoch, new_marked);
  while (old_marked < new_marked) {
    int16_t actual_raw_marked = CompareAndSwapRawNumberOfMarkedDescriptors(
        old_raw_marked, new_raw_marked);
    if (actual_raw_marked == old_raw_marked) {
      break;
    }
    old_raw_marked = actual_raw_marked;
    old_marked =
        NumberOfMarkedDescriptors::decode(mark_compact_epoch, old_raw_marked);
  }
  return old_marked;
}

Handle<AccessorPair> AccessorPair::Copy(Isolate* isolate,
                                        Handle<AccessorPair> pair) {
  Handle<AccessorPair> copy = isolate->factory()->NewAccessorPair();
  copy->set_getter(pair->getter());
  copy->set_setter(pair->setter());
  return copy;
}

Handle<Object> AccessorPair::GetComponent(Isolate* isolate,
                                          Handle<AccessorPair> accessor_pair,
                                          AccessorComponent component) {
  Object accessor = accessor_pair->get(component);
  if (accessor.IsFunctionTemplateInfo()) {
    return ApiNatives::InstantiateFunction(
               handle(FunctionTemplateInfo::cast(accessor), isolate))
        .ToHandleChecked();
  }
  if (accessor.IsNull(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return handle(accessor, isolate);
}

#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray other) {
  if (number_of_all_descriptors() != other.number_of_all_descriptors()) {
    return false;
  }
  for (int i = 0; i < number_of_descriptors(); ++i) {
    if (GetKey(i) != other.GetKey(i)) return false;
    if (GetDetails(i).AsSmi() != other.GetDetails(i).AsSmi()) return false;
    if (GetValue(i) != other.GetValue(i)) return false;
  }
  return true;
}
#endif

// static
MaybeHandle<String> Name::ToFunctionName(Isolate* isolate, Handle<Name> name) {
  if (name->IsString()) return Handle<String>::cast(name);
  // ES6 section 9.2.11 SetFunctionName, step 4.
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
MaybeHandle<String> Name::ToFunctionName(Isolate* isolate, Handle<Name> name,
                                         Handle<String> prefix) {
  Handle<String> name_string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name_string,
                             ToFunctionName(isolate, name), String);
  IncrementalStringBuilder builder(isolate);
  builder.AppendString(prefix);
  builder.AppendCharacter(' ');
  builder.AppendString(name_string);
  return builder.Finish();
}

void Relocatable::PostGarbageCollectionProcessing(Isolate* isolate) {
  Relocatable* current = isolate->relocatable_top();
  while (current != nullptr) {
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
  isolate->set_relocatable_top(nullptr);
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
  while (current != nullptr) {
    current->IterateInstance(v);
    current = current->prev_;
  }
}

namespace {

template <typename sinkchar>
void WriteFixedArrayToFlat(FixedArray fixed_array, int length, String separator,
                           sinkchar* sink, int sink_length) {
  DisallowHeapAllocation no_allocation;
  CHECK_GT(length, 0);
  CHECK_LE(length, fixed_array.length());
#ifdef DEBUG
  sinkchar* sink_end = sink + sink_length;
#endif

  const int separator_length = separator.length();
  const bool use_one_byte_separator_fast_path =
      separator_length == 1 && sizeof(sinkchar) == 1 &&
      StringShape(separator).IsSequentialOneByte();
  uint8_t separator_one_char;
  if (use_one_byte_separator_fast_path) {
    CHECK(StringShape(separator).IsSequentialOneByte());
    CHECK_EQ(separator.length(), 1);
    separator_one_char =
        SeqOneByteString::cast(separator).GetChars(no_allocation)[0];
  }

  uint32_t num_separators = 0;
  for (int i = 0; i < length; i++) {
    Object element = fixed_array.get(i);
    const bool element_is_separator_sequence = element.IsSmi();

    // If element is a Smi, it represents the number of separators to write.
    if (V8_UNLIKELY(element_is_separator_sequence)) {
      CHECK(element.ToUint32(&num_separators));
      // Verify that Smis (number of separators) only occur when necessary:
      //   1) at the beginning
      //   2) at the end
      //   3) when the number of separators > 1
      //     - It is assumed that consecutive Strings will have one separator,
      //       so there is no need for a Smi.
      DCHECK(i == 0 || i == length - 1 || num_separators > 1);
    }

    // Write separator(s) if necessary.
    if (num_separators > 0 && separator_length > 0) {
      // TODO(pwong): Consider doubling strategy employed by runtime-strings.cc
      //              WriteRepeatToFlat().
      // Fast path for single character, single byte separators.
      if (use_one_byte_separator_fast_path) {
        DCHECK_LE(sink + num_separators, sink_end);
        memset(sink, separator_one_char, num_separators);
        DCHECK_EQ(separator_length, 1);
        sink += num_separators;
      } else {
        for (uint32_t j = 0; j < num_separators; j++) {
          DCHECK_LE(sink + separator_length, sink_end);
          String::WriteToFlat(separator, sink, 0, separator_length);
          sink += separator_length;
        }
      }
    }

    if (V8_UNLIKELY(element_is_separator_sequence)) {
      num_separators = 0;
    } else {
      DCHECK(element.IsString());
      String string = String::cast(element);
      const int string_length = string.length();

      DCHECK(string_length == 0 || sink < sink_end);
      String::WriteToFlat(string, sink, 0, string_length);
      sink += string_length;

      // Next string element, needs at least one separator preceding it.
      num_separators = 1;
    }
  }

  // Verify we have written to the end of the sink.
  DCHECK_EQ(sink, sink_end);
}

}  // namespace

// static
Address JSArray::ArrayJoinConcatToSequentialString(Isolate* isolate,
                                                   Address raw_fixed_array,
                                                   intptr_t length,
                                                   Address raw_separator,
                                                   Address raw_dest) {
  DisallowHeapAllocation no_allocation;
  DisallowJavascriptExecution no_js(isolate);
  FixedArray fixed_array = FixedArray::cast(Object(raw_fixed_array));
  String separator = String::cast(Object(raw_separator));
  String dest = String::cast(Object(raw_dest));
  DCHECK(fixed_array.IsFixedArray());
  DCHECK(StringShape(dest).IsSequentialOneByte() ||
         StringShape(dest).IsSequentialTwoByte());

  if (StringShape(dest).IsSequentialOneByte()) {
    WriteFixedArrayToFlat(fixed_array, static_cast<int>(length), separator,
                          SeqOneByteString::cast(dest).GetChars(no_allocation),
                          dest.length());
  } else {
    DCHECK(StringShape(dest).IsSequentialTwoByte());
    WriteFixedArrayToFlat(fixed_array, static_cast<int>(length), separator,
                          SeqTwoByteString::cast(dest).GetChars(no_allocation),
                          dest.length());
  }
  return dest.ptr();
}

uint32_t StringHasher::MakeArrayIndexHash(uint32_t value, int length) {
  // For array indexes mix the length into the hash as an array index could
  // be zero.
  DCHECK_GT(length, 0);
  DCHECK_LE(length, String::kMaxArrayIndexSize);
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));

  value <<= String::ArrayIndexValueBits::kShift;
  value |= length << String::ArrayIndexLengthBits::kShift;

  DCHECK_EQ(value & String::kIsNotArrayIndexMask, 0);
  DCHECK_EQ(length <= String::kMaxCachedArrayIndexLength,
            Name::ContainsCachedArrayIndex(value));
  return value;
}

Handle<Object> CacheInitialJSArrayMaps(Isolate* isolate,
                                       Handle<Context> native_context,
                                       Handle<Map> initial_map) {
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
    Map maybe_elements_transition = current_map->ElementsTransitionMap(isolate);
    if (!maybe_elements_transition.is_null()) {
      new_map = handle(maybe_elements_transition, isolate);
    } else {
      new_map = Map::CopyAsElementsKind(isolate, current_map, next_kind,
                                        INSERT_TRANSITION);
    }
    DCHECK_EQ(next_kind, new_map->elements_kind());
    native_context->set(Context::ArrayMapIndex(next_kind), *new_map);
    current_map = new_map;
  }
  return initial_map;
}

STATIC_ASSERT_FIELD_OFFSETS_EQUAL(HeapNumber::kValueOffset,
                                  Oddball::kToNumberRawOffset);

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

// static
int Script::GetEvalPosition(Isolate* isolate, Handle<Script> script) {
  DCHECK(script->compilation_type() == Script::COMPILATION_TYPE_EVAL);
  int position = script->eval_from_position();
  if (position < 0) {
    // Due to laziness, the position may not have been translated from code
    // offset yet, which would be encoded as negative integer. In that case,
    // translate and set the position.
    if (!script->has_eval_from_shared()) {
      position = 0;
    } else {
      Handle<SharedFunctionInfo> shared =
          handle(script->eval_from_shared(), isolate);
      SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared);
      position = shared->abstract_code().SourcePosition(-position);
    }
    DCHECK_GE(position, 0);
    script->set_eval_from_position(position);
  }
  return position;
}

void Script::InitLineEnds(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  if (!script->line_ends().IsUndefined(isolate)) return;
  DCHECK(script->type() != Script::TYPE_WASM ||
         script->source_mapping_url().IsString());

  Object src_obj = script->source();
  if (!src_obj.IsString()) {
    DCHECK(src_obj.IsUndefined(isolate));
    script->set_line_ends(ReadOnlyRoots(isolate).empty_fixed_array());
  } else {
    DCHECK(src_obj.IsString());
    Handle<String> src(String::cast(src_obj), isolate);
    Handle<FixedArray> array = String::CalculateLineEnds(isolate, src, true);
    script->set_line_ends(*array);
  }

  DCHECK(script->line_ends().IsFixedArray());
}

bool Script::GetPositionInfo(Handle<Script> script, int position,
                             PositionInfo* info, OffsetFlag offset_flag) {
  // For wasm, we do not create an artificial line_ends array, but do the
  // translation directly.
  if (script->type() != Script::TYPE_WASM) InitLineEnds(script);
  return script->GetPositionInfo(position, info, offset_flag);
}

bool Script::IsUserJavaScript() { return type() == Script::TYPE_NORMAL; }

bool Script::ContainsAsmModule() {
  DisallowHeapAllocation no_gc;
  SharedFunctionInfo::ScriptIterator iter(this->GetIsolate(), *this);
  for (SharedFunctionInfo info = iter.Next(); !info.is_null();
       info = iter.Next()) {
    if (info.HasAsmWasmData()) return true;
  }
  return false;
}

namespace {
bool GetPositionInfoSlow(const Script script, int position,
                         Script::PositionInfo* info) {
  if (!script.source().IsString()) return false;
  if (position < 0) position = 0;

  String source_string = String::cast(script.source());
  int line = 0;
  int line_start = 0;
  int len = source_string.length();
  for (int pos = 0; pos <= len; ++pos) {
    if (pos == len || source_string.Get(pos) == '\n') {
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

#define SMI_VALUE(x) (Smi::ToInt(x))
bool Script::GetPositionInfo(int position, PositionInfo* info,
                             OffsetFlag offset_flag) const {
  DisallowHeapAllocation no_allocation;

  // For wasm, we do not rely on the line_ends array, but do the translation
  // directly.
  if (type() == Script::TYPE_WASM) {
    DCHECK_LE(0, position);
    return WasmModuleObject::cast(wasm_module_object())
        .GetPositionInfo(static_cast<uint32_t>(position), info);
  }

  if (line_ends().IsUndefined()) {
    // Slow mode: we do not have line_ends. We have to iterate through source.
    if (!GetPositionInfoSlow(*this, position, info)) return false;
  } else {
    DCHECK(line_ends().IsFixedArray());
    FixedArray ends = FixedArray::cast(line_ends());

    const int ends_len = ends.length();
    if (ends_len == 0) return false;

    // Return early on invalid positions. Negative positions behave as if 0 was
    // passed, and positions beyond the end of the script return as failure.
    if (position < 0) {
      position = 0;
    } else if (position > SMI_VALUE(ends.get(ends_len - 1))) {
      return false;
    }

    // Determine line number by doing a binary search on the line ends array.
    if (SMI_VALUE(ends.get(0)) >= position) {
      info->line = 0;
      info->line_start = 0;
      info->column = position;
    } else {
      int left = 0;
      int right = ends_len - 1;

      while (right > 0) {
        DCHECK_LE(left, right);
        const int mid = (left + right) / 2;
        if (position > SMI_VALUE(ends.get(mid))) {
          left = mid + 1;
        } else if (position <= SMI_VALUE(ends.get(mid - 1))) {
          right = mid - 1;
        } else {
          info->line = mid;
          break;
        }
      }
      DCHECK(SMI_VALUE(ends.get(info->line)) >= position &&
             SMI_VALUE(ends.get(info->line - 1)) < position);
      info->line_start = SMI_VALUE(ends.get(info->line - 1)) + 1;
      info->column = position - info->line_start;
    }

    // Line end is position of the linebreak character.
    info->line_end = SMI_VALUE(ends.get(info->line));
    if (info->line_end > 0) {
      DCHECK(source().IsString());
      String src = String::cast(source());
      if (src.length() >= info->line_end &&
          src.Get(info->line_end - 1) == '\r') {
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

Object Script::GetNameOrSourceURL() {
  // Keep in sync with ScriptNameOrSourceURL in messages.js.
  if (!source_url().IsUndefined()) return source_url();
  return name();
}

MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    Isolate* isolate, const FunctionLiteral* fun) {
  CHECK_NE(fun->function_literal_id(), kFunctionLiteralIdInvalid);
  // If this check fails, the problem is most probably the function id
  // renumbering done by AstFunctionLiteralIdReindexer; in particular, that
  // AstTraversalVisitor doesn't recurse properly in the construct which
  // triggers the mismatch.
  CHECK_LT(fun->function_literal_id(), shared_function_infos().length());
  MaybeObject shared = shared_function_infos().Get(fun->function_literal_id());
  HeapObject heap_object;
  if (!shared->GetHeapObject(&heap_object) ||
      heap_object.IsUndefined(isolate)) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  return handle(SharedFunctionInfo::cast(heap_object), isolate);
}

std::unique_ptr<v8::tracing::TracedValue> Script::ToTracedValue() {
  auto value = v8::tracing::TracedValue::Create();
  if (name().IsString()) {
    value->SetString("name", String::cast(name()).ToCString());
  }
  value->SetInteger("lineOffset", line_offset());
  value->SetInteger("columnOffset", column_offset());
  if (source_mapping_url().IsString()) {
    value->SetString("sourceMappingURL",
                     String::cast(source_mapping_url()).ToCString());
  }
  if (source().IsString()) {
    value->SetString("source", String::cast(source()).ToCString());
  }
  return value;
}

// static
const char* Script::kTraceScope = "v8::internal::Script";

uint64_t Script::TraceID() const { return id(); }

std::unique_ptr<v8::tracing::TracedValue> Script::TraceIDRef() const {
  auto value = v8::tracing::TracedValue::Create();
  std::ostringstream ost;
  ost << "0x" << std::hex << TraceID();
  value->SetString("id_ref", ost.str());
  value->SetString("scope", kTraceScope);
  return value;
}

Script::Iterator::Iterator(Isolate* isolate)
    : iterator_(isolate->heap()->script_list()) {}

Script Script::Iterator::Next() {
  Object o = iterator_.Next();
  if (o != Object()) {
    return Script::cast(o);
  }
  return Script();
}

uint32_t SharedFunctionInfo::Hash() {
  // Hash SharedFunctionInfo based on its start position and script id. Note: we
  // don't use the function's literal id since getting that is slow for compiled
  // funcitons.
  int start_pos = StartPosition();
  int script_id = script().IsScript() ? Script::cast(script()).id() : 0;
  return static_cast<uint32_t>(base::hash_combine(start_pos, script_id));
}

std::unique_ptr<v8::tracing::TracedValue> SharedFunctionInfo::ToTracedValue(
    FunctionLiteral* literal) {
  auto value = v8::tracing::TracedValue::Create();
  if (HasSharedName()) {
    value->SetString("name", Name().ToCString());
  }
  if (HasInferredName()) {
    value->SetString("inferredName", inferred_name().ToCString());
  }
  if (is_toplevel()) {
    value->SetBoolean("isToplevel", true);
  }
  value->SetInteger("formalParameterCount", internal_formal_parameter_count());
  value->SetString("languageMode", LanguageMode2String(language_mode()));
  value->SetString("kind", FunctionKind2String(kind()));
  if (script().IsScript()) {
    value->SetValue("script", Script::cast(script()).TraceIDRef());
    value->BeginDictionary("sourcePosition");
    Script::PositionInfo info;
    // We get the start position from the {literal} here, because the
    // SharedFunctionInfo itself might not have a way to get to the
    // start position early on (currently that's the case when it's
    // marked for eager compilation).
    if (Script::cast(script()).GetPositionInfo(literal->start_position(), &info,
                                               Script::WITH_OFFSET)) {
      value->SetInteger("line", info.line + 1);
      value->SetInteger("column", info.column + 1);
    }
    value->EndDictionary();
  }
  return value;
}

// static
const char* SharedFunctionInfo::kTraceScope =
    "v8::internal::SharedFunctionInfo";

uint64_t SharedFunctionInfo::TraceID(FunctionLiteral* literal) const {
  int literal_id =
      literal ? literal->function_literal_id() : function_literal_id();
  Script script = Script::cast(this->script());
  return (static_cast<uint64_t>(script.id() + 1) << 32) |
         (static_cast<uint64_t>(literal_id));
}

std::unique_ptr<v8::tracing::TracedValue> SharedFunctionInfo::TraceIDRef()
    const {
  auto value = v8::tracing::TracedValue::Create();
  std::ostringstream ost;
  ost << "0x" << std::hex << TraceID();
  value->SetString("id_ref", ost.str());
  value->SetString("scope", kTraceScope);
  return value;
}

Code SharedFunctionInfo::GetCode() const {
  // ======
  // NOTE: This chain of checks MUST be kept in sync with the equivalent CSA
  // GetSharedFunctionInfoCode method in code-stub-assembler.cc.
  // ======

  Isolate* isolate = GetIsolate();
  Object data = function_data();
  if (data.IsSmi()) {
    // Holding a Smi means we are a builtin.
    DCHECK(HasBuiltinId());
    return isolate->builtins()->builtin(builtin_id());
  } else if (data.IsBytecodeArray()) {
    // Having a bytecode array means we are a compiled, interpreted function.
    DCHECK(HasBytecodeArray());
    return isolate->builtins()->builtin(Builtins::kInterpreterEntryTrampoline);
  } else if (data.IsAsmWasmData()) {
    // Having AsmWasmData means we are an asm.js/wasm function.
    DCHECK(HasAsmWasmData());
    return isolate->builtins()->builtin(Builtins::kInstantiateAsmJs);
  } else if (data.IsUncompiledData()) {
    // Having uncompiled data (with or without scope) means we need to compile.
    DCHECK(HasUncompiledData());
    return isolate->builtins()->builtin(Builtins::kCompileLazy);
  } else if (data.IsFunctionTemplateInfo()) {
    // Having a function template info means we are an API function.
    DCHECK(IsApiFunction());
    return isolate->builtins()->builtin(Builtins::kHandleApiCall);
  } else if (data.IsWasmExportedFunctionData()) {
    // Having a WasmExportedFunctionData means the code is in there.
    DCHECK(HasWasmExportedFunctionData());
    return wasm_exported_function_data().wrapper_code();
  } else if (data.IsInterpreterData()) {
    Code code = InterpreterTrampoline();
    DCHECK(code.IsCode());
    DCHECK(code.is_interpreter_trampoline_builtin());
    return code;
  } else if (data.IsWasmJSFunctionData()) {
    return wasm_js_function_data().wrapper_code();
  } else if (data.IsWasmCapiFunctionData()) {
    return wasm_capi_function_data().wrapper_code();
  }
  UNREACHABLE();
}

WasmExportedFunctionData SharedFunctionInfo::wasm_exported_function_data()
    const {
  DCHECK(HasWasmExportedFunctionData());
  return WasmExportedFunctionData::cast(function_data());
}

WasmJSFunctionData SharedFunctionInfo::wasm_js_function_data() const {
  DCHECK(HasWasmJSFunctionData());
  return WasmJSFunctionData::cast(function_data());
}

WasmCapiFunctionData SharedFunctionInfo::wasm_capi_function_data() const {
  DCHECK(HasWasmCapiFunctionData());
  return WasmCapiFunctionData::cast(function_data());
}

SharedFunctionInfo::ScriptIterator::ScriptIterator(Isolate* isolate,
                                                   Script script)
    : ScriptIterator(handle(script.shared_function_infos(), isolate)) {}

SharedFunctionInfo::ScriptIterator::ScriptIterator(
    Handle<WeakFixedArray> shared_function_infos)
    : shared_function_infos_(shared_function_infos), index_(0) {}

SharedFunctionInfo SharedFunctionInfo::ScriptIterator::Next() {
  while (index_ < shared_function_infos_->length()) {
    MaybeObject raw = shared_function_infos_->Get(index_++);
    HeapObject heap_object;
    if (!raw->GetHeapObject(&heap_object) || heap_object.IsUndefined()) {
      continue;
    }
    return SharedFunctionInfo::cast(heap_object);
  }
  return SharedFunctionInfo();
}

void SharedFunctionInfo::ScriptIterator::Reset(Isolate* isolate,
                                               Script script) {
  shared_function_infos_ = handle(script.shared_function_infos(), isolate);
  index_ = 0;
}

SharedFunctionInfo::GlobalIterator::GlobalIterator(Isolate* isolate)
    : isolate_(isolate),
      script_iterator_(isolate),
      noscript_sfi_iterator_(isolate->heap()->noscript_shared_function_infos()),
      sfi_iterator_(isolate, script_iterator_.Next()) {}

SharedFunctionInfo SharedFunctionInfo::GlobalIterator::Next() {
  HeapObject next = noscript_sfi_iterator_.Next();
  if (!next.is_null()) return SharedFunctionInfo::cast(next);
  for (;;) {
    next = sfi_iterator_.Next();
    if (!next.is_null()) return SharedFunctionInfo::cast(next);
    Script next_script = script_iterator_.Next();
    if (next_script.is_null()) return SharedFunctionInfo();
    sfi_iterator_.Reset(isolate_, next_script);
  }
}

void SharedFunctionInfo::SetScript(Handle<SharedFunctionInfo> shared,
                                   Handle<Object> script_object,
                                   int function_literal_id,
                                   bool reset_preparsed_scope_data) {
  if (shared->script() == *script_object) return;
  Isolate* isolate = shared->GetIsolate();

  if (reset_preparsed_scope_data &&
      shared->HasUncompiledDataWithPreparseData()) {
    shared->ClearPreparseData();
  }

  // Add shared function info to new script's list. If a collection occurs,
  // the shared function info may be temporarily in two lists.
  // This is okay because the gc-time processing of these lists can tolerate
  // duplicates.
  if (script_object->IsScript()) {
    DCHECK(!shared->script().IsScript());
    Handle<Script> script = Handle<Script>::cast(script_object);
    Handle<WeakFixedArray> list =
        handle(script->shared_function_infos(), isolate);
#ifdef DEBUG
    DCHECK_LT(function_literal_id, list->length());
    MaybeObject maybe_object = list->Get(function_literal_id);
    HeapObject heap_object;
    if (maybe_object->GetHeapObjectIfWeak(&heap_object)) {
      DCHECK_EQ(heap_object, *shared);
    }
#endif
    list->Set(function_literal_id, HeapObjectReference::Weak(*shared));

    // Remove shared function info from root array.
    WeakArrayList noscript_list =
        isolate->heap()->noscript_shared_function_infos();
    CHECK(noscript_list.RemoveOne(MaybeObjectHandle::Weak(shared)));
  } else {
    DCHECK(shared->script().IsScript());
    Handle<WeakArrayList> list =
        isolate->factory()->noscript_shared_function_infos();

#ifdef DEBUG
    if (FLAG_enable_slow_asserts) {
      WeakArrayList::Iterator iterator(*list);
      for (HeapObject next = iterator.Next(); !next.is_null();
           next = iterator.Next()) {
        DCHECK_NE(next, *shared);
      }
    }
#endif  // DEBUG

    list =
        WeakArrayList::AddToEnd(isolate, list, MaybeObjectHandle::Weak(shared));

    isolate->heap()->SetRootNoScriptSharedFunctionInfos(*list);

    // Remove shared function info from old script's list.
    Script old_script = Script::cast(shared->script());

    // Due to liveedit, it might happen that the old_script doesn't know
    // about the SharedFunctionInfo, so we have to guard against that.
    Handle<WeakFixedArray> infos(old_script.shared_function_infos(), isolate);
    if (function_literal_id < infos->length()) {
      MaybeObject raw =
          old_script.shared_function_infos().Get(function_literal_id);
      HeapObject heap_object;
      if (raw->GetHeapObjectIfWeak(&heap_object) && heap_object == *shared) {
        old_script.shared_function_infos().Set(
            function_literal_id, HeapObjectReference::Strong(
                                     ReadOnlyRoots(isolate).undefined_value()));
      }
    }
  }

  // Finally set new script.
  shared->set_script(*script_object);
}

bool SharedFunctionInfo::HasBreakInfo() const {
  if (!HasDebugInfo()) return false;
  DebugInfo info = GetDebugInfo();
  bool has_break_info = info.HasBreakInfo();
  return has_break_info;
}

bool SharedFunctionInfo::BreakAtEntry() const {
  if (!HasDebugInfo()) return false;
  DebugInfo info = GetDebugInfo();
  bool break_at_entry = info.BreakAtEntry();
  return break_at_entry;
}

bool SharedFunctionInfo::HasCoverageInfo() const {
  if (!HasDebugInfo()) return false;
  DebugInfo info = GetDebugInfo();
  bool has_coverage_info = info.HasCoverageInfo();
  return has_coverage_info;
}

CoverageInfo SharedFunctionInfo::GetCoverageInfo() const {
  DCHECK(HasCoverageInfo());
  return CoverageInfo::cast(GetDebugInfo().coverage_info());
}

String SharedFunctionInfo::DebugName() {
  DisallowHeapAllocation no_gc;
  String function_name = Name();
  if (function_name.length() > 0) return function_name;
  return inferred_name();
}

bool SharedFunctionInfo::PassesFilter(const char* raw_filter) {
  Vector<const char> filter = CStrVector(raw_filter);
  std::unique_ptr<char[]> cstrname(DebugName().ToCString());
  return v8::internal::PassesFilter(CStrVector(cstrname.get()), filter);
}

bool SharedFunctionInfo::HasSourceCode() const {
  Isolate* isolate = GetIsolate();
  return !script().IsUndefined(isolate) &&
         !Script::cast(script()).source().IsUndefined(isolate);
}

void SharedFunctionInfo::DiscardCompiledMetadata(
    Isolate* isolate,
    std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
        gc_notify_updated_slot) {
  DisallowHeapAllocation no_gc;
  if (is_compiled()) {
    HeapObject outer_scope_info;
    if (scope_info().HasOuterScopeInfo()) {
      outer_scope_info = scope_info().OuterScopeInfo();
    } else {
      outer_scope_info = ReadOnlyRoots(isolate).the_hole_value();
    }

    // Raw setter to avoid validity checks, since we're performing the unusual
    // task of decompiling.
    set_raw_outer_scope_info_or_feedback_metadata(outer_scope_info);
    gc_notify_updated_slot(
        *this,
        RawField(SharedFunctionInfo::kOuterScopeInfoOrFeedbackMetadataOffset),
        outer_scope_info);
  } else {
    DCHECK(outer_scope_info().IsScopeInfo() || outer_scope_info().IsTheHole());
  }

  // TODO(rmcilroy): Possibly discard ScopeInfo here as well.
}

// static
void SharedFunctionInfo::DiscardCompiled(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_info) {
  DCHECK(shared_info->CanDiscardCompiled());

  Handle<String> inferred_name_val =
      handle(shared_info->inferred_name(), isolate);
  int start_position = shared_info->StartPosition();
  int end_position = shared_info->EndPosition();

  shared_info->DiscardCompiledMetadata(isolate);

  // Replace compiled data with a new UncompiledData object.
  if (shared_info->HasUncompiledDataWithPreparseData()) {
    // If this is uncompiled data with a pre-parsed scope data, we can just
    // clear out the scope data and keep the uncompiled data.
    shared_info->ClearPreparseData();
  } else {
    // Create a new UncompiledData, without pre-parsed scope, and update the
    // function data to point to it. Use the raw function data setter to avoid
    // validity checks, since we're performing the unusual task of decompiling.
    Handle<UncompiledData> data =
        isolate->factory()->NewUncompiledDataWithoutPreparseData(
            inferred_name_val, start_position, end_position);
    shared_info->set_function_data(*data);
  }
}

// static
Handle<Object> SharedFunctionInfo::GetSourceCode(
    Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  if (!shared->HasSourceCode()) return isolate->factory()->undefined_value();
  Handle<String> source(String::cast(Script::cast(shared->script()).source()),
                        isolate);
  return isolate->factory()->NewSubString(source, shared->StartPosition(),
                                          shared->EndPosition());
}

// static
Handle<Object> SharedFunctionInfo::GetSourceCodeHarmony(
    Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  if (!shared->HasSourceCode()) return isolate->factory()->undefined_value();
  Handle<String> script_source(
      String::cast(Script::cast(shared->script()).source()), isolate);
  int start_pos = shared->function_token_position();
  DCHECK_NE(start_pos, kNoSourcePosition);
  Handle<String> source = isolate->factory()->NewSubString(
      script_source, start_pos, shared->EndPosition());
  if (!shared->is_wrapped()) return source;

  DCHECK(!shared->name_should_print_as_anonymous());
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("function ");
  builder.AppendString(Handle<String>(shared->Name(), isolate));
  builder.AppendCString("(");
  Handle<FixedArray> args(Script::cast(shared->script()).wrapped_arguments(),
                          isolate);
  int argc = args->length();
  for (int i = 0; i < argc; i++) {
    if (i > 0) builder.AppendCString(", ");
    builder.AppendString(Handle<String>(String::cast(args->get(i)), isolate));
  }
  builder.AppendCString(") {\n");
  builder.AppendString(source);
  builder.AppendCString("\n}");
  return builder.Finish().ToHandleChecked();
}

namespace {
void TraceInlining(SharedFunctionInfo shared, const char* msg) {
  if (FLAG_trace_turbo_inlining) {
    StdoutStream os;
    os << Brief(shared) << ": IsInlineable? " << msg << "\n";
  }
}
}  // namespace

bool SharedFunctionInfo::IsInlineable() {
  if (!script().IsScript()) {
    TraceInlining(*this, "false (no Script associated with it)");
    return false;
  }

  if (GetIsolate()->is_precise_binary_code_coverage() &&
      !has_reported_binary_coverage()) {
    // We may miss invocations if this function is inlined.
    TraceInlining(*this, "false (requires reported binary coverage)");
    return false;
  }

  if (optimization_disabled()) {
    TraceInlining(*this, "false (optimization disabled)");
    return false;
  }

  // Built-in functions are handled by the JSCallReducer.
  if (HasBuiltinId()) {
    TraceInlining(*this, "false (is a builtin)");
    return false;
  }

  if (!IsUserJavaScript()) {
    TraceInlining(*this, "false (is not user code)");
    return false;
  }

  // If there is no bytecode array, it is either not compiled or it is compiled
  // with WebAssembly for the asm.js pipeline. In either case we don't want to
  // inline.
  if (!HasBytecodeArray()) {
    TraceInlining(*this, "false (has no BytecodeArray)");
    return false;
  }

  if (GetBytecodeArray().length() > FLAG_max_inlined_bytecode_size) {
    TraceInlining(*this, "false (length > FLAG_max_inlined_bytecode_size)");
    return false;
  }

  if (HasBreakInfo()) {
    TraceInlining(*this, "false (may contain break points)");
    return false;
  }

  TraceInlining(*this, "true");
  return true;
}

int SharedFunctionInfo::SourceSize() { return EndPosition() - StartPosition(); }

// Output the source code without any allocation in the heap.
std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v) {
  const SharedFunctionInfo s = v.value;
  // For some native functions there is no source.
  if (!s.HasSourceCode()) return os << "<No Source>";

  // Get the source for the script which this function came from.
  // Don't use String::cast because we don't want more assertion errors while
  // we are already creating a stack dump.
  String script_source =
      String::unchecked_cast(Script::cast(s.script()).source());

  if (!script_source.LooksValid()) return os << "<Invalid Source>";

  if (!s.is_toplevel()) {
    os << "function ";
    String name = s.Name();
    if (name.length() > 0) {
      name.PrintUC16(os);
    }
  }

  int len = s.EndPosition() - s.StartPosition();
  if (len <= v.max_length || v.max_length < 0) {
    script_source.PrintUC16(os, s.StartPosition(), s.EndPosition());
    return os;
  } else {
    script_source.PrintUC16(os, s.StartPosition(),
                            s.StartPosition() + v.max_length);
    return os << "...\n";
  }
}

void SharedFunctionInfo::DisableOptimization(BailoutReason reason) {
  DCHECK_NE(reason, BailoutReason::kNoReason);

  set_flags(DisabledOptimizationReasonBits::update(flags(), reason));
  // Code should be the lazy compilation stub or else interpreted.
  DCHECK(abstract_code().kind() == AbstractCode::INTERPRETED_FUNCTION ||
         abstract_code().kind() == AbstractCode::BUILTIN);
  PROFILE(GetIsolate(), CodeDisableOptEvent(abstract_code(), *this));
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for ");
    ShortPrint();
    PrintF(", reason: %s]\n", GetBailoutReason(reason));
  }
}

void SharedFunctionInfo::InitFromFunctionLiteral(
    Handle<SharedFunctionInfo> shared_info, FunctionLiteral* lit,
    bool is_toplevel) {
  Isolate* isolate = shared_info->GetIsolate();
  bool needs_position_info = true;

  // When adding fields here, make sure DeclarationScope::AnalyzePartially is
  // updated accordingly.
  shared_info->set_internal_formal_parameter_count(lit->parameter_count());
  shared_info->SetFunctionTokenPosition(lit->function_token_position(),
                                        lit->start_position());
  if (shared_info->scope_info().HasPositionInfo()) {
    shared_info->scope_info().SetPositionInfo(lit->start_position(),
                                              lit->end_position());
    needs_position_info = false;
  }
  shared_info->set_is_declaration(lit->is_declaration());
  shared_info->set_is_named_expression(lit->is_named_expression());
  shared_info->set_is_anonymous_expression(lit->is_anonymous_expression());
  shared_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  shared_info->set_language_mode(lit->language_mode());
  shared_info->set_is_wrapped(lit->is_wrapped());
  shared_info->set_function_literal_id(lit->function_literal_id());
  //  shared_info->set_kind(lit->kind());
  // FunctionKind must have already been set.
  DCHECK(lit->kind() == shared_info->kind());
  shared_info->set_needs_home_object(lit->scope()->NeedsHomeObject());
  DCHECK_IMPLIES(lit->requires_instance_members_initializer(),
                 IsClassConstructor(lit->kind()));
  shared_info->set_requires_instance_members_initializer(
      lit->requires_instance_members_initializer());

  shared_info->set_is_toplevel(is_toplevel);
  DCHECK(shared_info->outer_scope_info().IsTheHole());
  if (!is_toplevel) {
    Scope* outer_scope = lit->scope()->GetOuterScopeWithContext();
    if (outer_scope) {
      shared_info->set_outer_scope_info(*outer_scope->scope_info());
    }
  }

  shared_info->set_length(lit->function_length());

  // For lazy parsed functions, the following flags will be inaccurate since we
  // don't have the information yet. They're set later in
  // SetSharedFunctionFlagsFromLiteral (compiler.cc), when the function is
  // really parsed and compiled.
  if (lit->ShouldEagerCompile()) {
    shared_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
    shared_info->UpdateAndFinalizeExpectedNofPropertiesFromEstimate(lit);
    shared_info->set_is_safe_to_skip_arguments_adaptor(
        lit->SafeToSkipArgumentsAdaptor());
    DCHECK_NULL(lit->produced_preparse_data());
    // If we're about to eager compile, we'll have the function literal
    // available, so there's no need to wastefully allocate an uncompiled data.
    // TODO(leszeks): This should be explicitly passed as a parameter, rather
    // than relying on a property of the literal.
    needs_position_info = false;
  } else {
    shared_info->set_is_safe_to_skip_arguments_adaptor(false);
    ProducedPreparseData* scope_data = lit->produced_preparse_data();
    if (scope_data != nullptr) {
      Handle<PreparseData> preparse_data =
          scope_data->Serialize(shared_info->GetIsolate());
      Handle<UncompiledData> data =
          isolate->factory()->NewUncompiledDataWithPreparseData(
              lit->inferred_name(), lit->start_position(), lit->end_position(),
              preparse_data);
      shared_info->set_uncompiled_data(*data);
      needs_position_info = false;
    }
    shared_info->UpdateExpectedNofPropertiesFromEstimate(lit);
  }
  if (needs_position_info) {
    Handle<UncompiledData> data =
        isolate->factory()->NewUncompiledDataWithoutPreparseData(
            lit->inferred_name(), lit->start_position(), lit->end_position());
    shared_info->set_uncompiled_data(*data);
  }
}

uint16_t SharedFunctionInfo::get_property_estimate_from_literal(
    FunctionLiteral* literal) {
  int estimate = literal->expected_property_count();

  // If this is a class constructor, we may have already parsed fields.
  if (is_class_constructor()) {
    estimate += expected_nof_properties();
  }
  return estimate;
}

void SharedFunctionInfo::UpdateExpectedNofPropertiesFromEstimate(
    FunctionLiteral* literal) {
  set_expected_nof_properties(get_property_estimate_from_literal(literal));
}

void SharedFunctionInfo::UpdateAndFinalizeExpectedNofPropertiesFromEstimate(
    FunctionLiteral* literal) {
  DCHECK(literal->ShouldEagerCompile());
  if (are_properties_final()) {
    return;
  }
  int estimate = get_property_estimate_from_literal(literal);

  // If no properties are added in the constructor, they are more likely
  // to be added later.
  if (estimate == 0) estimate = 2;

  // Limit actual estimate to fit in a 8 bit field, we will never allocate
  // more than this in any case.
  STATIC_ASSERT(JSObject::kMaxInObjectProperties <= kMaxUInt8);
  estimate = std::min(estimate, kMaxUInt8);

  set_expected_nof_properties(estimate);
  set_are_properties_final(true);
}

void SharedFunctionInfo::SetFunctionTokenPosition(int function_token_position,
                                                  int start_position) {
  int offset;
  if (function_token_position == kNoSourcePosition) {
    offset = 0;
  } else {
    offset = start_position - function_token_position;
  }

  if (offset > kMaximumFunctionTokenOffset) {
    offset = kFunctionTokenOutOfRange;
  }
  set_raw_function_token_offset(offset);
}

int SharedFunctionInfo::StartPosition() const {
  Object maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo info = ScopeInfo::cast(maybe_scope_info);
    if (info.HasPositionInfo()) {
      return info.StartPosition();
    }
  } else if (HasUncompiledData()) {
    // Works with or without scope.
    return uncompiled_data().start_position();
  } else if (IsApiFunction() || HasBuiltinId()) {
    DCHECK_IMPLIES(HasBuiltinId(), builtin_id() != Builtins::kCompileLazy);
    return 0;
  }
  return kNoSourcePosition;
}

int SharedFunctionInfo::EndPosition() const {
  Object maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo info = ScopeInfo::cast(maybe_scope_info);
    if (info.HasPositionInfo()) {
      return info.EndPosition();
    }
  } else if (HasUncompiledData()) {
    // Works with or without scope.
    return uncompiled_data().end_position();
  } else if (IsApiFunction() || HasBuiltinId()) {
    DCHECK_IMPLIES(HasBuiltinId(), builtin_id() != Builtins::kCompileLazy);
    return 0;
  }
  return kNoSourcePosition;
}

void SharedFunctionInfo::SetPosition(int start_position, int end_position) {
  Object maybe_scope_info = name_or_scope_info();
  if (maybe_scope_info.IsScopeInfo()) {
    ScopeInfo info = ScopeInfo::cast(maybe_scope_info);
    if (info.HasPositionInfo()) {
      info.SetPositionInfo(start_position, end_position);
    }
  } else if (HasUncompiledData()) {
    if (HasUncompiledDataWithPreparseData()) {
      // Clear out preparsed scope data, since the position setter invalidates
      // any scope data.
      ClearPreparseData();
    }
    uncompiled_data().set_start_position(start_position);
    uncompiled_data().set_end_position(end_position);
  } else {
    UNREACHABLE();
  }
}

bool SharedFunctionInfo::AreSourcePositionsAvailable() const {
  if (FLAG_enable_lazy_source_positions) {
    return !HasBytecodeArray() || GetBytecodeArray().HasSourcePositionTable();
  }
  return true;
}

// static
void SharedFunctionInfo::EnsureSourcePositionsAvailable(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_info) {
  if (FLAG_enable_lazy_source_positions && shared_info->HasBytecodeArray() &&
      !shared_info->GetBytecodeArray().HasSourcePositionTable()) {
    Compiler::CollectSourcePositions(isolate, shared_info);
  }
}

// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK_GE(capacity, 0);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
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
  bool bool_trap_result = trap_result->BooleanValue(isolate);
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

bool JSArray::SetLengthWouldNormalize(uint32_t new_length) {
  if (!HasFastElements()) return false;
  uint32_t capacity = static_cast<uint32_t>(elements().length());
  uint32_t new_capacity;
  return JSArray::SetLengthWouldNormalize(GetHeap(), new_length) &&
         ShouldConvertToSlowElements(*this, capacity, new_length - 1,
                                     &new_capacity);
}

const double AllocationSite::kPretenureRatio = 0.85;

void AllocationSite::ResetPretenureDecision() {
  set_pretenure_decision(kUndecided);
  set_memento_found_count(0);
  set_memento_create_count(0);
}

AllocationType AllocationSite::GetAllocationType() const {
  PretenureDecision mode = pretenure_decision();
  // Zombie objects "decide" to be untenured.
  return mode == kTenure ? AllocationType::kOld : AllocationType::kYoung;
}

bool AllocationSite::IsNested() {
  DCHECK(FLAG_trace_track_allocation_sites);
  Object current = boilerplate().GetHeap()->allocation_sites_list();
  while (current.IsAllocationSite()) {
    AllocationSite current_site = AllocationSite::cast(current);
    if (current_site.nested_site() == *this) {
      return true;
    }
    current = current_site.weak_next();
  }
  return false;
}

bool AllocationSite::ShouldTrack(ElementsKind from, ElementsKind to) {
  return IsSmiElementsKind(from) &&
         IsMoreGeneralElementsKindTransition(from, to);
}

const char* AllocationSite::PretenureDecisionName(PretenureDecision decision) {
  switch (decision) {
    case kUndecided:
      return "undecided";
    case kDontTenure:
      return "don't tenure";
    case kMaybeTenure:
      return "maybe tenure";
    case kTenure:
      return "tenure";
    case kZombie:
      return "zombie";
    default:
      UNREACHABLE();
  }
  return nullptr;
}

bool JSArray::HasReadOnlyLength(Handle<JSArray> array) {
  Map map = array->map();
  // Fast path: "length" is the first fast property of arrays. Since it's not
  // configurable, it's guaranteed to be the first in the descriptor array.
  if (!map.is_dictionary_map()) {
    DCHECK(map.instance_descriptors().GetKey(0) ==
           array->GetReadOnlyRoots().length_string());
    return map.instance_descriptors().GetDetails(0).IsReadOnly();
  }

  Isolate* isolate = array->GetIsolate();
  LookupIterator it(array, isolate->factory()->length_string(), array,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  CHECK_EQ(LookupIterator::ACCESSOR, it.state());
  return it.IsReadOnly();
}

bool JSArray::WouldChangeReadOnlyLength(Handle<JSArray> array, uint32_t index) {
  uint32_t length = 0;
  CHECK(array->length().ToArrayLength(&length));
  if (length <= index) return HasReadOnlyLength(array);
  return false;
}

// Certain compilers request function template instantiation when they
// see the definition of the other template functions in the
// class. This requires us to have the template functions put
// together, so even though this function belongs in objects-debug.cc,
// we keep it here instead to satisfy certain compilers.
#ifdef OBJECT_PRINT
template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::Print(std::ostream& os) {
  DisallowHeapAllocation no_gc;
  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  Derived dictionary = Derived::cast(*this);
  int capacity = dictionary.Capacity();
  for (int i = 0; i < capacity; i++) {
    Object k = dictionary.KeyAt(i);
    if (!dictionary.ToKey(roots, i, &k)) continue;
    os << "\n   ";
    if (k.IsString()) {
      String::cast(k).StringPrint(os);
    } else {
      os << Brief(k);
    }
    os << ": " << Brief(dictionary.ValueAt(i)) << " ";
    dictionary.DetailsAt(i).PrintAsSlowTo(os);
  }
}
template <typename Derived, typename Shape>
void Dictionary<Derived, Shape>::Print() {
  StdoutStream os;
  Print(os);
  os << std::endl;
}
#endif

int FixedArrayBase::GetMaxLengthForNewSpaceAllocation(ElementsKind kind) {
  return ((kMaxRegularHeapObjectSize - FixedArrayBase::kHeaderSize) >>
          ElementsKindToShiftSize(kind));
}

bool FixedArrayBase::IsCowArray() const {
  return map() == GetReadOnlyRoots().fixed_cow_array_map();
}

const char* Symbol::PrivateSymbolToName() const {
  ReadOnlyRoots roots = GetReadOnlyRoots();
#define SYMBOL_CHECK_AND_PRINT(_, name) \
  if (*this == roots.name()) return #name;
  PRIVATE_SYMBOL_LIST_GENERATOR(SYMBOL_CHECK_AND_PRINT, /* not used */)
#undef SYMBOL_CHECK_AND_PRINT
  return "UNKNOWN";
}

void Symbol::SymbolShortPrint(std::ostream& os) {
  os << "<Symbol:";
  if (!name().IsUndefined()) {
    os << " ";
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(name()).StringShortPrint(&accumulator, false);
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
      : HashTableKey(CompilationCacheShape::StringSharedHash(
            *source, *shared, language_mode, position)),
        source_(source),
        shared_(shared),
        language_mode_(language_mode),
        position_(position) {}

  bool IsMatch(Object other) override {
    DisallowHeapAllocation no_allocation;
    if (!other.IsFixedArray()) {
      DCHECK(other.IsNumber());
      uint32_t other_hash = static_cast<uint32_t>(other.Number());
      return Hash() == other_hash;
    }
    FixedArray other_array = FixedArray::cast(other);
    SharedFunctionInfo shared = SharedFunctionInfo::cast(other_array.get(0));
    if (shared != *shared_) return false;
    int language_unchecked = Smi::ToInt(other_array.get(2));
    DCHECK(is_valid_language_mode(language_unchecked));
    LanguageMode language_mode = static_cast<LanguageMode>(language_unchecked);
    if (language_mode != language_mode_) return false;
    int position = Smi::ToInt(other_array.get(3));
    if (position != position_) return false;
    String source = String::cast(other_array.get(1));
    return source.Equals(*source_);
  }

  Handle<Object> AsHandle(Isolate* isolate) {
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(4);
    array->set(0, *shared_);
    array->set(1, *source_);
    array->set(2, Smi::FromEnum(language_mode_));
    array->set(3, Smi::FromInt(position_));
    array->set_map(ReadOnlyRoots(isolate).fixed_cow_array_map());
    return array;
  }

 private:
  Handle<String> source_;
  Handle<SharedFunctionInfo> shared_;
  LanguageMode language_mode_;
  int position_;
};

v8::Promise::PromiseState JSPromise::status() const {
  int value = flags() & kStatusMask;
  DCHECK(value == 0 || value == 1 || value == 2);
  return static_cast<v8::Promise::PromiseState>(value);
}

void JSPromise::set_status(Promise::PromiseState status) {
  int value = flags() & ~kStatusMask;
  set_flags(value | status);
}

// static
const char* JSPromise::Status(v8::Promise::PromiseState status) {
  switch (status) {
    case v8::Promise::kFulfilled:
      return "resolved";
    case v8::Promise::kPending:
      return "pending";
    case v8::Promise::kRejected:
      return "rejected";
  }
  UNREACHABLE();
}

int JSPromise::async_task_id() const {
  return AsyncTaskIdField::decode(flags());
}

void JSPromise::set_async_task_id(int id) {
  set_flags(AsyncTaskIdField::update(flags(), id));
}

// static
Handle<Object> JSPromise::Fulfill(Handle<JSPromise> promise,
                                  Handle<Object> value) {
  Isolate* const isolate = promise->GetIsolate();

  // 1. Assert: The value of promise.[[PromiseState]] is "pending".
  CHECK_EQ(Promise::kPending, promise->status());

  // 2. Let reactions be promise.[[PromiseFulfillReactions]].
  Handle<Object> reactions(promise->reactions(), isolate);

  // 3. Set promise.[[PromiseResult]] to value.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  promise->set_reactions_or_result(*value);

  // 6. Set promise.[[PromiseState]] to "fulfilled".
  promise->set_status(Promise::kFulfilled);

  // 7. Return TriggerPromiseReactions(reactions, value).
  return TriggerPromiseReactions(isolate, reactions, value,
                                 PromiseReaction::kFulfill);
}

// static
Handle<Object> JSPromise::Reject(Handle<JSPromise> promise,
                                 Handle<Object> reason, bool debug_event) {
  Isolate* const isolate = promise->GetIsolate();

  if (debug_event) isolate->debug()->OnPromiseReject(promise, reason);
  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  // 1. Assert: The value of promise.[[PromiseState]] is "pending".
  CHECK_EQ(Promise::kPending, promise->status());

  // 2. Let reactions be promise.[[PromiseRejectReactions]].
  Handle<Object> reactions(promise->reactions(), isolate);

  // 3. Set promise.[[PromiseResult]] to reason.
  // 4. Set promise.[[PromiseFulfillReactions]] to undefined.
  // 5. Set promise.[[PromiseRejectReactions]] to undefined.
  promise->set_reactions_or_result(*reason);

  // 6. Set promise.[[PromiseState]] to "rejected".
  promise->set_status(Promise::kRejected);

  // 7. If promise.[[PromiseIsHandled]] is false, perform
  //    HostPromiseRejectionTracker(promise, "reject").
  if (!promise->has_handler()) {
    isolate->ReportPromiseReject(promise, reason, kPromiseRejectWithNoHandler);
  }

  // 8. Return TriggerPromiseReactions(reactions, reason).
  return TriggerPromiseReactions(isolate, reactions, reason,
                                 PromiseReaction::kReject);
}

// static
MaybeHandle<Object> JSPromise::Resolve(Handle<JSPromise> promise,
                                       Handle<Object> resolution) {
  Isolate* const isolate = promise->GetIsolate();

  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  // 6. If SameValue(resolution, promise) is true, then
  if (promise.is_identical_to(resolution)) {
    // a. Let selfResolutionError be a newly created TypeError object.
    Handle<Object> self_resolution_error = isolate->factory()->NewTypeError(
        MessageTemplate::kPromiseCyclic, resolution);
    // b. Return RejectPromise(promise, selfResolutionError).
    return Reject(promise, self_resolution_error);
  }

  // 7. If Type(resolution) is not Object, then
  if (!resolution->IsJSReceiver()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 8. Let then be Get(resolution, "then").
  MaybeHandle<Object> then;
  if (isolate->IsPromiseThenLookupChainIntact(
          Handle<JSReceiver>::cast(resolution))) {
    // We can skip the "then" lookup on {resolution} if its [[Prototype]]
    // is the (initial) Promise.prototype and the Promise#then protector
    // is intact, as that guards the lookup path for the "then" property
    // on JSPromise instances which have the (initial) %PromisePrototype%.
    then = isolate->promise_then();
  } else {
    then =
        JSReceiver::GetProperty(isolate, Handle<JSReceiver>::cast(resolution),
                                isolate->factory()->then_string());
  }

  // 9. If then is an abrupt completion, then
  Handle<Object> then_action;
  if (!then.ToHandle(&then_action)) {
    // a. Return RejectPromise(promise, then.[[Value]]).
    Handle<Object> reason(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();
    return Reject(promise, reason, false);
  }

  // 10. Let thenAction be then.[[Value]].
  // 11. If IsCallable(thenAction) is false, then
  if (!then_action->IsCallable()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 12. Perform EnqueueJob("PromiseJobs", PromiseResolveThenableJob,
  //                        «promise, resolution, thenAction»).
  Handle<PromiseResolveThenableJobTask> task =
      isolate->factory()->NewPromiseResolveThenableJobTask(
          promise, Handle<JSReceiver>::cast(then_action),
          Handle<JSReceiver>::cast(resolution), isolate->native_context());
  if (isolate->debug()->is_active() && resolution->IsJSPromise()) {
    // Mark the dependency of the new {promise} on the {resolution}.
    Object::SetProperty(isolate, resolution,
                        isolate->factory()->promise_handled_by_symbol(),
                        promise)
        .Check();
  }
  MicrotaskQueue* microtask_queue =
      isolate->native_context()->microtask_queue();
  if (microtask_queue) microtask_queue->EnqueueMicrotask(*task);

  // 13. Return undefined.
  return isolate->factory()->undefined_value();
}

// static
Handle<Object> JSPromise::TriggerPromiseReactions(Isolate* isolate,
                                                  Handle<Object> reactions,
                                                  Handle<Object> argument,
                                                  PromiseReaction::Type type) {
  CHECK(reactions->IsSmi() || reactions->IsPromiseReaction());

  // We need to reverse the {reactions} here, since we record them
  // on the JSPromise in the reverse order.
  {
    DisallowHeapAllocation no_gc;
    Object current = *reactions;
    Object reversed = Smi::kZero;
    while (!current.IsSmi()) {
      Object next = PromiseReaction::cast(current).next();
      PromiseReaction::cast(current).set_next(reversed);
      reversed = current;
      current = next;
    }
    reactions = handle(reversed, isolate);
  }

  // Morph the {reactions} into PromiseReactionJobTasks
  // and push them onto the microtask queue.
  while (!reactions->IsSmi()) {
    Handle<HeapObject> task = Handle<HeapObject>::cast(reactions);
    Handle<PromiseReaction> reaction = Handle<PromiseReaction>::cast(task);
    reactions = handle(reaction->next(), isolate);

    Handle<NativeContext> handler_context;

    Handle<HeapObject> primary_handler;
    Handle<HeapObject> secondary_handler;
    if (type == PromiseReaction::kFulfill) {
      primary_handler = handle(reaction->fulfill_handler(), isolate);
      secondary_handler = handle(reaction->reject_handler(), isolate);
    } else {
      primary_handler = handle(reaction->reject_handler(), isolate);
      secondary_handler = handle(reaction->fulfill_handler(), isolate);
    }

    if (primary_handler->IsJSReceiver()) {
      JSReceiver::GetContextForMicrotask(
          Handle<JSReceiver>::cast(primary_handler))
          .ToHandle(&handler_context);
    }
    if (handler_context.is_null() && secondary_handler->IsJSReceiver()) {
      JSReceiver::GetContextForMicrotask(
          Handle<JSReceiver>::cast(secondary_handler))
          .ToHandle(&handler_context);
    }
    if (handler_context.is_null()) handler_context = isolate->native_context();

    STATIC_ASSERT(
        static_cast<int>(PromiseReaction::kSize) ==
        static_cast<int>(
            PromiseReactionJobTask::kSizeOfAllPromiseReactionJobTasks));
    if (type == PromiseReaction::kFulfill) {
      task->synchronized_set_map(
          ReadOnlyRoots(isolate).promise_fulfill_reaction_job_task_map());
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_argument(
          *argument);
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_context(
          *handler_context);
      STATIC_ASSERT(
          static_cast<int>(PromiseReaction::kFulfillHandlerOffset) ==
          static_cast<int>(PromiseFulfillReactionJobTask::kHandlerOffset));
      STATIC_ASSERT(
          static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
          static_cast<int>(
              PromiseFulfillReactionJobTask::kPromiseOrCapabilityOffset));
    } else {
      DisallowHeapAllocation no_gc;
      task->synchronized_set_map(
          ReadOnlyRoots(isolate).promise_reject_reaction_job_task_map());
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_argument(*argument);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_context(
          *handler_context);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_handler(
          *primary_handler);
      STATIC_ASSERT(
          static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
          static_cast<int>(
              PromiseRejectReactionJobTask::kPromiseOrCapabilityOffset));
    }

    MicrotaskQueue* microtask_queue = handler_context->microtask_queue();
    if (microtask_queue) {
      microtask_queue->EnqueueMicrotask(
          *Handle<PromiseReactionJobTask>::cast(task));
    }
  }

  return isolate->factory()->undefined_value();
}

namespace {

JSRegExp::Flags RegExpFlagsFromString(Isolate* isolate, Handle<String> flags,
                                      bool* success) {
  STATIC_ASSERT(JSRegExp::FlagFromChar('g') == JSRegExp::kGlobal);
  STATIC_ASSERT(JSRegExp::FlagFromChar('i') == JSRegExp::kIgnoreCase);
  STATIC_ASSERT(JSRegExp::FlagFromChar('m') == JSRegExp::kMultiline);
  STATIC_ASSERT(JSRegExp::FlagFromChar('s') == JSRegExp::kDotAll);
  STATIC_ASSERT(JSRegExp::FlagFromChar('u') == JSRegExp::kUnicode);
  STATIC_ASSERT(JSRegExp::FlagFromChar('y') == JSRegExp::kSticky);

  int length = flags->length();
  if (length == 0) {
    *success = true;
    return JSRegExp::kNone;
  }
  // A longer flags string cannot be valid.
  if (length > JSRegExp::kFlagCount) return JSRegExp::Flags(0);
  // Initialize {value} to {kInvalid} to allow 2-in-1 duplicate/invalid check.
  JSRegExp::Flags value = JSRegExp::kInvalid;
  if (flags->IsSeqOneByteString()) {
    DisallowHeapAllocation no_gc;
    SeqOneByteString seq_flags = SeqOneByteString::cast(*flags);
    for (int i = 0; i < length; i++) {
      JSRegExp::Flag flag = JSRegExp::FlagFromChar(seq_flags.Get(i));
      // Duplicate or invalid flag.
      if (value & flag) return JSRegExp::Flags(0);
      value |= flag;
    }
  } else {
    flags = String::Flatten(isolate, flags);
    DisallowHeapAllocation no_gc;
    String::FlatContent flags_content = flags->GetFlatContent(no_gc);
    for (int i = 0; i < length; i++) {
      JSRegExp::Flag flag = JSRegExp::FlagFromChar(flags_content.Get(i));
      // Duplicate or invalid flag.
      if (value & flag) return JSRegExp::Flags(0);
      value |= flag;
    }
  }
  *success = true;
  // Drop the initially set {kInvalid} bit.
  value ^= JSRegExp::kInvalid;
  return value;
}

}  // namespace

// static
MaybeHandle<JSRegExp> JSRegExp::New(Isolate* isolate, Handle<String> pattern,
                                    Flags flags) {
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

namespace {

template <typename Char>
int CountRequiredEscapes(Handle<String> source) {
  DisallowHeapAllocation no_gc;
  int escapes = 0;
  bool in_char_class = false;
  Vector<const Char> src = source->GetCharVector<Char>(no_gc);
  for (int i = 0; i < src.length(); i++) {
    const Char c = src[i];
    if (c == '\\') {
      // Escape. Skip next character;
      i++;
    } else if (c == '/' && !in_char_class) {
      // Not escaped forward-slash needs escape.
      escapes++;
    } else if (c == '[') {
      in_char_class = true;
    } else if (c == ']') {
      in_char_class = false;
    } else if (c == '\n') {
      escapes++;
    } else if (c == '\r') {
      escapes++;
    } else if (static_cast<int>(c) == 0x2028) {
      escapes += std::strlen("\\u2028") - 1;
    } else if (static_cast<int>(c) == 0x2029) {
      escapes += std::strlen("\\u2029") - 1;
    } else {
      DCHECK(!unibrow::IsLineTerminator(static_cast<unibrow::uchar>(c)));
    }
  }
  DCHECK(!in_char_class);
  return escapes;
}

template <typename Char>
void WriteStringToCharVector(Vector<Char> v, int* d, const char* string) {
  int s = 0;
  while (string[s] != '\0') v[(*d)++] = string[s++];
}

template <typename Char, typename StringType>
Handle<StringType> WriteEscapedRegExpSource(Handle<String> source,
                                            Handle<StringType> result) {
  DisallowHeapAllocation no_gc;
  Vector<const Char> src = source->GetCharVector<Char>(no_gc);
  Vector<Char> dst(result->GetChars(no_gc), result->length());
  int s = 0;
  int d = 0;
  bool in_char_class = false;
  while (s < src.length()) {
    if (src[s] == '\\') {
      // Escape. Copy this and next character.
      dst[d++] = src[s++];
      if (s == src.length()) break;
    } else if (src[s] == '/' && !in_char_class) {
      // Not escaped forward-slash needs escape.
      dst[d++] = '\\';
    } else if (src[s] == '[') {
      in_char_class = true;
    } else if (src[s] == ']') {
      in_char_class = false;
    } else if (src[s] == '\n') {
      WriteStringToCharVector(dst, &d, "\\n");
      s++;
      continue;
    } else if (src[s] == '\r') {
      WriteStringToCharVector(dst, &d, "\\r");
      s++;
      continue;
    } else if (static_cast<int>(src[s]) == 0x2028) {
      WriteStringToCharVector(dst, &d, "\\u2028");
      s++;
      continue;
    } else if (static_cast<int>(src[s]) == 0x2029) {
      WriteStringToCharVector(dst, &d, "\\u2029");
      s++;
      continue;
    }
    dst[d++] = src[s++];
  }
  DCHECK_EQ(result->length(), d);
  DCHECK(!in_char_class);
  return result;
}

MaybeHandle<String> EscapeRegExpSource(Isolate* isolate,
                                       Handle<String> source) {
  DCHECK(source->IsFlat());
  if (source->length() == 0) return isolate->factory()->query_colon_string();
  bool one_byte = String::IsOneByteRepresentationUnderneath(*source);
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

}  // namespace

// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source,
                                           Handle<String> flags_string) {
  Isolate* isolate = regexp->GetIsolate();
  bool success = false;
  Flags flags = RegExpFlagsFromString(isolate, flags_string, &success);
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

  source = String::Flatten(isolate, source);

  RETURN_ON_EXCEPTION(isolate, RegExp::Compile(isolate, regexp, source, flags),
                      JSRegExp);

  Handle<String> escaped_source;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, escaped_source,
                             EscapeRegExpSource(isolate, source), JSRegExp);

  regexp->set_source(*escaped_source);
  regexp->set_flags(Smi::FromInt(flags));

  Map map = regexp->map();
  Object constructor = map.GetConstructor();
  if (constructor.IsJSFunction() &&
      JSFunction::cast(constructor).initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex, Smi::kZero,
                                  SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    RETURN_ON_EXCEPTION(
        isolate,
        Object::SetProperty(isolate, regexp, factory->lastIndex_string(),
                            Handle<Smi>(Smi::zero(), isolate)),
        JSRegExp);
  }

  return regexp;
}

// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(Handle<String> string, JSRegExp::Flags flags)
      : HashTableKey(
            CompilationCacheShape::RegExpHash(*string, Smi::FromInt(flags))),
        string_(string),
        flags_(Smi::FromInt(flags)) {}

  // Rather than storing the key in the hash table, a pointer to the
  // stored value is stored where the key should be.  IsMatch then
  // compares the search key to the found object, rather than comparing
  // a key to a key.
  bool IsMatch(Object obj) override {
    FixedArray val = FixedArray::cast(obj);
    return string_->Equals(String::cast(val.get(JSRegExp::kSourceIndex))) &&
           (flags_ == val.get(JSRegExp::kFlagsIndex));
  }

  Handle<String> string_;
  Smi flags_;
};

// InternalizedStringKey carries a string/internalized-string object as key.
class InternalizedStringKey final : public StringTableKey {
 public:
  explicit InternalizedStringKey(Handle<String> string)
      : StringTableKey(0, string->length()), string_(string) {
    DCHECK(!string->IsInternalizedString());
    DCHECK(string->IsFlat());
    // Make sure hash_field is computed.
    string->Hash();
    set_hash_field(string->hash_field());
  }

  bool IsMatch(String string) override { return string_->SlowEquals(string); }

  Handle<String> AsHandle(Isolate* isolate) override {
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

 private:
  Handle<String> string_;
};

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::IteratePrefix(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(*this, 0, kElementsStartOffset, v);
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::IterateElements(ObjectVisitor* v) {
  BodyDescriptorBase::IteratePointers(*this, kElementsStartOffset,
                                      SizeFor(length()), v);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::New(
    Isolate* isolate, int at_least_space_for, AllocationType allocation,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  DCHECK_IMPLIES(capacity_option == USE_CUSTOM_MINIMUM_CAPACITY,
                 base::bits::IsPowerOfTwo(at_least_space_for));

  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    isolate->heap()->FatalProcessOutOfMemory("invalid table size");
  }
  return NewInternal(isolate, capacity, allocation);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::NewInternal(
    Isolate* isolate, int capacity, AllocationType allocation) {
  Factory* factory = isolate->factory();
  int length = EntryToIndex(capacity);
  RootIndex map_root_index = Shape::GetMapRootIndex();
  Handle<FixedArray> array =
      factory->NewFixedArrayWithMap(map_root_index, length, allocation);
  Handle<Derived> table = Handle<Derived>::cast(array);

  table->SetNumberOfElements(0);
  table->SetNumberOfDeletedElements(0);
  table->SetCapacity(capacity);
  return table;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Rehash(ReadOnlyRoots roots, Derived new_table) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = new_table.GetWriteBarrierMode(no_gc);

  DCHECK_LT(NumberOfElements(), new_table.Capacity());

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex; i < kElementsStartIndex; i++) {
    new_table.set(i, get(i), mode);
  }

  // Rehash the elements.
  int capacity = this->Capacity();
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object k = this->get(from_index);
    if (!Shape::IsLive(roots, k)) continue;
    uint32_t hash = Shape::HashForObject(roots, k);
    uint32_t insertion_index = EntryToIndex(new_table.FindInsertionEntry(hash));
    new_table.set_key(insertion_index, get(from_index), mode);
    for (int j = 1; j < Shape::kEntrySize; j++) {
      new_table.set(insertion_index + j, get(from_index + j), mode);
    }
  }
  new_table.SetNumberOfElements(NumberOfElements());
  new_table.SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
uint32_t HashTable<Derived, Shape>::EntryForProbe(ReadOnlyRoots roots, Object k,
                                                  int probe,
                                                  uint32_t expected) {
  uint32_t hash = Shape::HashForObject(roots, k);
  uint32_t capacity = this->Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Swap(uint32_t entry1, uint32_t entry2,
                                     WriteBarrierMode mode) {
  int index1 = EntryToIndex(entry1);
  int index2 = EntryToIndex(entry2);
  Object temp[Shape::kEntrySize];
  Derived* self = static_cast<Derived*>(this);
  for (int j = 0; j < Shape::kEntrySize; j++) {
    temp[j] = get(index1 + j);
  }
  self->set_key(index1, get(index2), mode);
  for (int j = 1; j < Shape::kEntrySize; j++) {
    set(index1 + j, get(index2 + j), mode);
  }
  self->set_key(index2, temp[0], mode);
  for (int j = 1; j < Shape::kEntrySize; j++) {
    set(index2 + j, temp[j], mode);
  }
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Rehash(ReadOnlyRoots roots) {
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = GetWriteBarrierMode(no_gc);
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (uint32_t current = 0; current < capacity; current++) {
      Object current_key = KeyAt(current);
      if (!Shape::IsLive(roots, current_key)) continue;
      uint32_t target = EntryForProbe(roots, current_key, probe, current);
      if (current == target) continue;
      Object target_key = KeyAt(target);
      if (!Shape::IsLive(roots, target_key) ||
          EntryForProbe(roots, target_key, probe, target) != target) {
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
  // Wipe deleted entries.
  Object the_hole = roots.the_hole_value();
  HeapObject undefined = roots.undefined_value();
  Derived* self = static_cast<Derived*>(this);
  for (uint32_t current = 0; current < capacity; current++) {
    if (KeyAt(current) == the_hole) {
      self->set_key(EntryToIndex(current) + kEntryKeyIndex, undefined,
                    SKIP_WRITE_BARRIER);
    }
  }
  SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::EnsureCapacity(
    Isolate* isolate, Handle<Derived> table, int n, AllocationType allocation) {
  if (table->HasSufficientCapacityToAdd(n)) return table;

  int capacity = table->Capacity();
  int new_nof = table->NumberOfElements() + n;

  const int kMinCapacityForPretenure = 256;
  bool should_pretenure = allocation == AllocationType::kOld ||
                          ((capacity > kMinCapacityForPretenure) &&
                           !Heap::InYoungGeneration(*table));
  Handle<Derived> new_table = HashTable::New(
      isolate, new_nof,
      should_pretenure ? AllocationType::kOld : AllocationType::kYoung);

  table->Rehash(ReadOnlyRoots(isolate), *new_table);
  return new_table;
}

template bool
HashTable<NameDictionary, NameDictionaryShape>::HasSufficientCapacityToAdd(int);

template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::HasSufficientCapacityToAdd(
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

template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::Shrink(Isolate* isolate,
                                                  Handle<Derived> table,
                                                  int additionalCapacity) {
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();

  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (nof > (capacity >> 2)) return table;
  // Allocate a new dictionary with room for at least the current number of
  // elements + {additionalCapacity}. The allocation method will make sure that
  // there is extra room in the dictionary for additions. Don't go lower than
  // room for {kMinShrinkCapacity} elements.
  int at_least_room_for = nof + additionalCapacity;
  int new_capacity = ComputeCapacity(at_least_room_for);
  if (new_capacity < Derived::kMinShrinkCapacity) return table;
  if (new_capacity == capacity) return table;

  const int kMinCapacityForPretenure = 256;
  bool pretenure = (at_least_room_for > kMinCapacityForPretenure) &&
                   !Heap::InYoungGeneration(*table);
  Handle<Derived> new_table =
      HashTable::New(isolate, new_capacity,
                     pretenure ? AllocationType::kOld : AllocationType::kYoung,
                     USE_CUSTOM_MINIMUM_CAPACITY);

  table->Rehash(ReadOnlyRoots(isolate), *new_table);
  return new_table;
}

template <typename Derived, typename Shape>
uint32_t HashTable<Derived, Shape>::FindInsertionEntry(uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(hash, capacity);
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  ReadOnlyRoots roots = GetReadOnlyRoots();
  while (true) {
    if (!Shape::IsLive(roots, KeyAt(entry))) break;
    entry = NextProbe(entry, count++, capacity);
  }
  return entry;
}

void StringTable::EnsureCapacityForDeserialization(Isolate* isolate,
                                                   int expected) {
  Handle<StringTable> table = isolate->factory()->string_table();
  // We need a key instance for the virtual hash function.
  table = StringTable::EnsureCapacity(isolate, table, expected);
  isolate->heap()->SetRootStringTable(*table);
}

// static
Handle<String> StringTable::LookupString(Isolate* isolate,
                                         Handle<String> string) {
  string = String::Flatten(isolate, string);
  if (string->IsInternalizedString()) return string;

  InternalizedStringKey key(string);
  Handle<String> result = LookupKey(isolate, &key);

  if (FLAG_thin_strings) {
    if (!string->IsInternalizedString()) {
      string->MakeThin(isolate, *result);
    }
  } else {  // !FLAG_thin_strings
    if (string->IsConsString()) {
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(ReadOnlyRoots(isolate).empty_string());
    } else if (string->IsSlicedString()) {
      STATIC_ASSERT(static_cast<int>(ConsString::kSize) ==
                    static_cast<int>(SlicedString::kSize));
      DisallowHeapAllocation no_gc;
      bool one_byte = result->IsOneByteRepresentation();
      Handle<Map> map = one_byte
                            ? isolate->factory()->cons_one_byte_string_map()
                            : isolate->factory()->cons_string_map();
      string->set_map(*map);
      Handle<ConsString> cons = Handle<ConsString>::cast(string);
      cons->set_first(*result);
      cons->set_second(ReadOnlyRoots(isolate).empty_string());
    }
  }
  return result;
}

// static
template <typename StringTableKey>
Handle<String> StringTable::LookupKey(Isolate* isolate, StringTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  int entry = table->FindEntry(isolate, key);

  // String already in table.
  if (entry != kNotFound) {
    return handle(String::cast(table->KeyAt(entry)), isolate);
  }

  table = StringTable::CautiousShrink(isolate, table);
  // Adding new string. Grow table if needed.
  table = StringTable::EnsureCapacity(isolate, table, 1);
  isolate->heap()->SetRootStringTable(*table);

  return AddKeyNoResize(isolate, key);
}

template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               OneByteStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               TwoByteStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               SeqOneByteSubStringKey* key);
template Handle<String> StringTable::LookupKey(Isolate* isolate,
                                               SeqTwoByteSubStringKey* key);

Handle<String> StringTable::AddKeyNoResize(Isolate* isolate,
                                           StringTableKey* key) {
  Handle<StringTable> table = isolate->factory()->string_table();
  DCHECK(table->HasSufficientCapacityToAdd(1));
  // Create string object.
  Handle<String> string = key->AsHandle(isolate);
  // There must be no attempts to internalize strings that could throw
  // InvalidStringLength error.
  CHECK(!string.is_null());
  DCHECK(string->HasHashCode());
  DCHECK_EQ(table->FindEntry(isolate, key), kNotFound);

  // Add the new string and return it along with the string table.
  int entry = table->FindInsertionEntry(key->hash());
  table->set(EntryToIndex(entry), *string);
  table->ElementAdded();

  return Handle<String>::cast(string);
}

Handle<StringTable> StringTable::CautiousShrink(Isolate* isolate,
                                                Handle<StringTable> table) {
  // Only shrink if the table is very empty to avoid performance penalty.
  int capacity = table->Capacity();
  int nof = table->NumberOfElements();
  if (capacity <= StringTable::kMinCapacity) return table;
  if (nof > (capacity / kMaxEmptyFactor)) return table;
  // Keep capacity for at least half of the current nof elements.
  int slack_capacity = nof >> 2;
  return Shrink(isolate, table, slack_capacity);
}

namespace {

template <typename Char>
Address LookupString(Isolate* isolate, String string, String source,
                     size_t start) {
  DisallowHeapAllocation no_gc;
  StringTable table = isolate->heap()->string_table();
  uint64_t seed = HashSeed(isolate);

  int length = string.length();

  std::unique_ptr<Char[]> buffer;
  const Char* chars;

  if (source.IsConsString()) {
    DCHECK(!source.IsFlat());
    buffer.reset(new Char[length]);
    String::WriteToFlat(source, buffer.get(), 0, length);
    chars = buffer.get();
  } else {
    chars = source.GetChars<Char>(no_gc) + start;
  }
  // TODO(verwaest): Internalize to one-byte when possible.
  SequentialStringKey<Char> key(Vector<const Char>(chars, length), seed);

  // String could be an array index.
  uint32_t hash_field = key.hash_field();

  if (Name::ContainsCachedArrayIndex(hash_field)) {
    return Smi::FromInt(String::ArrayIndexValueBits::decode(hash_field)).ptr();
  }

  if ((hash_field & Name::kIsNotArrayIndexMask) == 0) {
    // It is an indexed, but it's not cached.
    return Smi::FromInt(ResultSentinel::kUnsupported).ptr();
  }

  int entry = table.FindEntry(ReadOnlyRoots(isolate), &key, key.hash());
  if (entry == kNotFound) {
    // A string that's not an array index, and not in the string table,
    // cannot have been used as a property name before.
    return Smi::FromInt(ResultSentinel::kNotFound).ptr();
  }

  String internalized = String::cast(table.KeyAt(entry));
  if (FLAG_thin_strings) {
    string.MakeThin(isolate, internalized);
  }
  return internalized.ptr();
}

}  // namespace

// static
Address StringTable::LookupStringIfExists_NoAllocate(Isolate* isolate,
                                                     Address raw_string) {
  String string = String::cast(Object(raw_string));
  DCHECK(!string.IsInternalizedString());

  // Valid array indices are >= 0, so they cannot be mixed up with any of
  // the result sentinels, which are negative.
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kUnsupported));
  STATIC_ASSERT(
      !String::ArrayIndexValueBits::is_valid(ResultSentinel::kNotFound));

  size_t start = 0;
  String source = string;
  if (source.IsSlicedString()) {
    SlicedString sliced = SlicedString::cast(source);
    start = sliced.offset();
    source = sliced.parent();
  } else if (source.IsConsString() && source.IsFlat()) {
    source = ConsString::cast(source).first();
  }
  if (source.IsThinString()) {
    source = ThinString::cast(source).actual();
    if (string.length() == source.length()) {
      return source.ptr();
    }
  }
  if (source.IsOneByteRepresentation()) {
    return i::LookupString<uint8_t>(isolate, string, source, start);
  }
  return i::LookupString<uint16_t>(isolate, string, source, start);
}

Handle<StringSet> StringSet::New(Isolate* isolate) {
  return HashTable::New(isolate, 0);
}

Handle<StringSet> StringSet::Add(Isolate* isolate, Handle<StringSet> stringset,
                                 Handle<String> name) {
  if (!stringset->Has(isolate, name)) {
    stringset = EnsureCapacity(isolate, stringset, 1);
    uint32_t hash = ShapeT::Hash(isolate, *name);
    int entry = stringset->FindInsertionEntry(hash);
    stringset->set(EntryToIndex(entry), *name);
    stringset->ElementAdded();
  }
  return stringset;
}

bool StringSet::Has(Isolate* isolate, Handle<String> name) {
  return FindEntry(isolate, *name) != kNotFound;
}

Handle<ObjectHashSet> ObjectHashSet::Add(Isolate* isolate,
                                         Handle<ObjectHashSet> set,
                                         Handle<Object> key) {
  int32_t hash = key->GetOrCreateHash(isolate).value();
  if (!set->Has(isolate, key, hash)) {
    set = EnsureCapacity(isolate, set, 1);
    int entry = set->FindInsertionEntry(hash);
    set->set(EntryToIndex(entry), *key);
    set->ElementAdded();
  }
  return set;
}

namespace {

const int kLiteralEntryLength = 2;
const int kLiteralInitialLength = 2;
const int kLiteralContextOffset = 0;
const int kLiteralLiteralsOffset = 1;

int SearchLiteralsMapEntry(CompilationCacheTable cache, int cache_entry,
                           Context native_context) {
  DisallowHeapAllocation no_gc;
  DCHECK(native_context.IsNativeContext());
  Object obj = cache.get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (obj.IsWeakFixedArray()) {
    WeakFixedArray literals_map = WeakFixedArray::cast(obj);
    int length = literals_map.length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      DCHECK(literals_map.Get(i + kLiteralContextOffset)->IsWeakOrCleared());
      if (literals_map.Get(i + kLiteralContextOffset) ==
          HeapObjectReference::Weak(native_context)) {
        return i;
      }
    }
  }
  return -1;
}

void AddToFeedbackCellsMap(Handle<CompilationCacheTable> cache, int cache_entry,
                           Handle<Context> native_context,
                           Handle<FeedbackCell> feedback_cell) {
  Isolate* isolate = native_context->GetIsolate();
  DCHECK(native_context->IsNativeContext());
  STATIC_ASSERT(kLiteralEntryLength == 2);
  Handle<WeakFixedArray> new_literals_map;
  int entry;

  Object obj = cache->get(cache_entry);

  // Check that there's no confusion between FixedArray and WeakFixedArray (the
  // object used to be a FixedArray here).
  DCHECK(!obj.IsFixedArray());
  if (!obj.IsWeakFixedArray() || WeakFixedArray::cast(obj).length() == 0) {
    new_literals_map = isolate->factory()->NewWeakFixedArray(
        kLiteralInitialLength, AllocationType::kOld);
    entry = 0;
  } else {
    Handle<WeakFixedArray> old_literals_map(WeakFixedArray::cast(obj), isolate);
    entry = SearchLiteralsMapEntry(*cache, cache_entry, *native_context);
    if (entry >= 0) {
      // Just set the code of the entry.
      old_literals_map->Set(entry + kLiteralLiteralsOffset,
                            HeapObjectReference::Weak(*feedback_cell));
      return;
    }

    // Can we reuse an entry?
    DCHECK_LT(entry, 0);
    int length = old_literals_map->length();
    for (int i = 0; i < length; i += kLiteralEntryLength) {
      if (old_literals_map->Get(i + kLiteralContextOffset)->IsCleared()) {
        new_literals_map = old_literals_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_literals_map = isolate->factory()->CopyWeakFixedArrayAndGrow(
          old_literals_map, kLiteralEntryLength, AllocationType::kOld);
      entry = old_literals_map->length();
    }
  }

  new_literals_map->Set(entry + kLiteralContextOffset,
                        HeapObjectReference::Weak(*native_context));
  new_literals_map->Set(entry + kLiteralLiteralsOffset,
                        HeapObjectReference::Weak(*feedback_cell));

#ifdef DEBUG
  for (int i = 0; i < new_literals_map->length(); i += kLiteralEntryLength) {
    MaybeObject object = new_literals_map->Get(i + kLiteralContextOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsNativeContext());
    object = new_literals_map->Get(i + kLiteralLiteralsOffset);
    DCHECK(object->IsCleared() ||
           object->GetHeapObjectAssumeWeak().IsFeedbackCell());
  }
#endif

  Object old_literals_map = cache->get(cache_entry);
  if (old_literals_map != *new_literals_map) {
    cache->set(cache_entry, *new_literals_map);
  }
}

FeedbackCell SearchLiteralsMap(CompilationCacheTable cache, int cache_entry,
                               Context native_context) {
  FeedbackCell result;
  int entry = SearchLiteralsMapEntry(cache, cache_entry, native_context);
  if (entry >= 0) {
    WeakFixedArray literals_map = WeakFixedArray::cast(cache.get(cache_entry));
    DCHECK_LE(entry + kLiteralEntryLength, literals_map.length());
    MaybeObject object = literals_map.Get(entry + kLiteralLiteralsOffset);

    if (!object->IsCleared()) {
      result = FeedbackCell::cast(object->GetHeapObjectAssumeWeak());
    }
  }
  DCHECK(result.is_null() || result.IsFeedbackCell());
  return result;
}

}  // namespace

MaybeHandle<SharedFunctionInfo> CompilationCacheTable::LookupScript(
    Handle<CompilationCacheTable> table, Handle<String> src,
    Handle<Context> native_context, LanguageMode language_mode) {
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the PartialSnapshotCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function().shared(),
                                    native_context->GetIsolate());
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  int entry = table->FindEntry(isolate, &key);
  if (entry == kNotFound) return MaybeHandle<SharedFunctionInfo>();
  int index = EntryToIndex(entry);
  if (!table->get(index).IsFixedArray()) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  Object obj = table->get(index + 1);
  if (obj.IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(obj), native_context->GetIsolate());
  }
  return MaybeHandle<SharedFunctionInfo>();
}

InfoCellPair CompilationCacheTable::LookupEval(
    Handle<CompilationCacheTable> table, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<Context> native_context,
    LanguageMode language_mode, int position) {
  InfoCellPair empty_result;
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, outer_info, language_mode, position);
  int entry = table->FindEntry(isolate, &key);
  if (entry == kNotFound) return empty_result;
  int index = EntryToIndex(entry);
  if (!table->get(index).IsFixedArray()) return empty_result;
  Object obj = table->get(EntryToIndex(entry) + 1);
  if (obj.IsSharedFunctionInfo()) {
    FeedbackCell feedback_cell =
        SearchLiteralsMap(*table, EntryToIndex(entry) + 2, *native_context);
    return InfoCellPair(SharedFunctionInfo::cast(obj), feedback_cell);
  }
  return empty_result;
}

Handle<Object> CompilationCacheTable::LookupRegExp(Handle<String> src,
                                                   JSRegExp::Flags flags) {
  Isolate* isolate = GetIsolate();
  DisallowHeapAllocation no_allocation;
  RegExpKey key(src, flags);
  int entry = FindEntry(isolate, &key);
  if (entry == kNotFound) return isolate->factory()->undefined_value();
  return Handle<Object>(get(EntryToIndex(entry) + 1), isolate);
}

Handle<CompilationCacheTable> CompilationCacheTable::PutScript(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<Context> native_context, LanguageMode language_mode,
    Handle<SharedFunctionInfo> value) {
  Isolate* isolate = native_context->GetIsolate();
  // We use the empty function SFI as part of the key. Although the
  // empty_function is native context dependent, the SFI is de-duped on
  // snapshot builds by the PartialSnapshotCache, and so this does not prevent
  // reuse of scripts in the compilation cache across native contexts.
  Handle<SharedFunctionInfo> shared(native_context->empty_function().shared(),
                                    isolate);
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, shared, language_mode, kNoSourcePosition);
  Handle<Object> k = key.AsHandle(isolate);
  cache = EnsureCapacity(isolate, cache, 1);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, *value);
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutEval(
    Handle<CompilationCacheTable> cache, Handle<String> src,
    Handle<SharedFunctionInfo> outer_info, Handle<SharedFunctionInfo> value,
    Handle<Context> native_context, Handle<FeedbackCell> feedback_cell,
    int position) {
  Isolate* isolate = native_context->GetIsolate();
  src = String::Flatten(isolate, src);
  StringSharedKey key(src, outer_info, value->language_mode(), position);
  {
    Handle<Object> k = key.AsHandle(isolate);
    int entry = cache->FindEntry(isolate, &key);
    if (entry != kNotFound) {
      cache->set(EntryToIndex(entry), *k);
      cache->set(EntryToIndex(entry) + 1, *value);
      // AddToFeedbackCellsMap may allocate a new sub-array to live in the
      // entry, but it won't change the cache array. Therefore EntryToIndex
      // and entry remains correct.
      AddToFeedbackCellsMap(cache, EntryToIndex(entry) + 2, native_context,
                            feedback_cell);
      // Add hash again even on cache hit to avoid unnecessary cache delay in
      // case of hash collisions.
    }
  }

  cache = EnsureCapacity(isolate, cache, 1);
  int entry = cache->FindInsertionEntry(key.Hash());
  Handle<Object> k =
      isolate->factory()->NewNumber(static_cast<double>(key.Hash()));
  cache->set(EntryToIndex(entry), *k);
  cache->set(EntryToIndex(entry) + 1, Smi::FromInt(kHashGenerations));
  cache->ElementAdded();
  return cache;
}

Handle<CompilationCacheTable> CompilationCacheTable::PutRegExp(
    Isolate* isolate, Handle<CompilationCacheTable> cache, Handle<String> src,
    JSRegExp::Flags flags, Handle<FixedArray> value) {
  RegExpKey key(src, flags);
  cache = EnsureCapacity(isolate, cache, 1);
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
  Object the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;

    if (get(entry_index).IsNumber()) {
      Smi count = Smi::cast(get(value_index));
      count = Smi::FromInt(count.value() - 1);
      if (count.value() == 0) {
        NoWriteBarrierSet(*this, entry_index, the_hole_value);
        NoWriteBarrierSet(*this, value_index, the_hole_value);
        ElementRemoved();
      } else {
        NoWriteBarrierSet(*this, value_index, count);
      }
    } else if (get(entry_index).IsFixedArray()) {
      SharedFunctionInfo info = SharedFunctionInfo::cast(get(value_index));
      if (info.IsInterpreted() && info.GetBytecodeArray().IsOld()) {
        for (int i = 0; i < kEntrySize; i++) {
          NoWriteBarrierSet(*this, entry_index + i, the_hole_value);
        }
        ElementRemoved();
      }
    }
  }
}

void CompilationCacheTable::Remove(Object value) {
  DisallowHeapAllocation no_allocation;
  Object the_hole_value = GetReadOnlyRoots().the_hole_value();
  for (int entry = 0, size = Capacity(); entry < size; entry++) {
    int entry_index = EntryToIndex(entry);
    int value_index = entry_index + 1;
    if (get(value_index) == value) {
      for (int i = 0; i < kEntrySize; i++) {
        NoWriteBarrierSet(*this, entry_index + i, the_hole_value);
      }
      ElementRemoved();
    }
  }
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::New(
    Isolate* isolate, int at_least_space_for, AllocationType allocation,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  Handle<Derived> dict = Dictionary<Derived, Shape>::New(
      isolate, at_least_space_for, allocation, capacity_option);
  dict->SetHash(PropertyArray::kNoHashSentinel);
  dict->SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return dict;
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::EnsureCapacity(
    Isolate* isolate, Handle<Derived> dictionary, int n) {
  // Check whether there are enough enumeration indices to add n elements.
  if (!PropertyDetails::IsValidIndex(dictionary->NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    int length = dictionary->NumberOfElements();

    Handle<FixedArray> iteration_order = IterationIndices(isolate, dictionary);
    DCHECK_EQ(length, iteration_order->length());

    // Iterate over the dictionary using the enumeration order and update
    // the dictionary with new enumeration indices.
    for (int i = 0; i < length; i++) {
      int index = Smi::ToInt(iteration_order->get(i));
      DCHECK(dictionary->IsKey(dictionary->GetReadOnlyRoots(),
                               dictionary->KeyAt(index)));

      int enum_index = PropertyDetails::kInitialIndex + i;

      PropertyDetails details = dictionary->DetailsAt(index);
      PropertyDetails new_details = details.set_index(enum_index);
      dictionary->DetailsAtPut(isolate, index, new_details);
    }

    // Set the next enumeration index.
    dictionary->SetNextEnumerationIndex(PropertyDetails::kInitialIndex +
                                        length);
  }
  return HashTable<Derived, Shape>::EnsureCapacity(isolate, dictionary, n);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::DeleteEntry(
    Isolate* isolate, Handle<Derived> dictionary, int entry) {
  DCHECK(Shape::kEntrySize != 3 ||
         dictionary->DetailsAt(entry).IsConfigurable());
  dictionary->ClearEntry(isolate, entry);
  dictionary->ElementRemoved();
  return Shrink(isolate, dictionary);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::AtPut(Isolate* isolate,
                                                  Handle<Derived> dictionary,
                                                  Key key, Handle<Object> value,
                                                  PropertyDetails details) {
  int entry = dictionary->FindEntry(isolate, key);

  // If the entry is present set the value;
  if (entry == Dictionary::kNotFound) {
    return Derived::Add(isolate, dictionary, key, value, details);
  }

  // We don't need to copy over the enumeration index.
  dictionary->ValueAtPut(entry, *value);
  if (Shape::kEntrySize == 3) dictionary->DetailsAtPut(isolate, entry, details);
  return dictionary;
}

template <typename Derived, typename Shape>
Handle<Derived>
BaseNameDictionary<Derived, Shape>::AddNoUpdateNextEnumerationIndex(
    Isolate* isolate, Handle<Derived> dictionary, Key key, Handle<Object> value,
    PropertyDetails details, int* entry_out) {
  // Insert element at empty or deleted entry
  return Dictionary<Derived, Shape>::Add(isolate, dictionary, key, value,
                                         details, entry_out);
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::Add(
    Isolate* isolate, Handle<Derived> dictionary, Key key, Handle<Object> value,
    PropertyDetails details, int* entry_out) {
  // Insert element at empty or deleted entry
  DCHECK_EQ(0, details.dictionary_index());
  // Assign an enumeration index to the property and update
  // SetNextEnumerationIndex.
  int index = dictionary->NextEnumerationIndex();
  details = details.set_index(index);
  dictionary = AddNoUpdateNextEnumerationIndex(isolate, dictionary, key, value,
                                               details, entry_out);
  // Update enumeration index here in order to avoid potential modification of
  // the canonical empty dictionary which lives in read only space.
  dictionary->SetNextEnumerationIndex(index + 1);
  return dictionary;
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::Add(Isolate* isolate,
                                                Handle<Derived> dictionary,
                                                Key key, Handle<Object> value,
                                                PropertyDetails details,
                                                int* entry_out) {
  uint32_t hash = Shape::Hash(isolate, key);
  // Valdate key is absent.
  SLOW_DCHECK((dictionary->FindEntry(isolate, key) == Dictionary::kNotFound));
  // Check whether the dictionary should be extended.
  dictionary = Derived::EnsureCapacity(isolate, dictionary, 1);

  // Compute the key object.
  Handle<Object> k = Shape::AsHandle(isolate, key);

  uint32_t entry = dictionary->FindInsertionEntry(hash);
  dictionary->SetEntry(isolate, entry, *k, *value, details);
  DCHECK(dictionary->KeyAt(entry).IsNumber() ||
         Shape::Unwrap(dictionary->KeyAt(entry)).IsUniqueName());
  dictionary->ElementAdded();
  if (entry_out) *entry_out = entry;
  return dictionary;
}

// static
Handle<SimpleNumberDictionary> SimpleNumberDictionary::Set(
    Isolate* isolate, Handle<SimpleNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value) {
  return AtPut(isolate, dictionary, key, value, PropertyDetails::Empty());
}

void NumberDictionary::UpdateMaxNumberKey(uint32_t key,
                                          Handle<JSObject> dictionary_holder) {
  DisallowHeapAllocation no_allocation;
  // If the dictionary requires slow elements an element has already
  // been added at a high index.
  if (requires_slow_elements()) return;
  // Check if this index is high enough that we should require slow
  // elements.
  if (key > kRequiresSlowElementsLimit) {
    if (!dictionary_holder.is_null()) {
      dictionary_holder->RequireSlowElements(*this);
    }
    set_requires_slow_elements();
    return;
  }
  // Update max key value.
  Object max_index_object = get(kMaxNumberKeyIndex);
  if (!max_index_object.IsSmi() || max_number_key() < key) {
    FixedArray::set(kMaxNumberKeyIndex,
                    Smi::FromInt(key << kRequiresSlowElementsTagSize));
  }
}

Handle<NumberDictionary> NumberDictionary::Set(
    Isolate* isolate, Handle<NumberDictionary> dictionary, uint32_t key,
    Handle<Object> value, Handle<JSObject> dictionary_holder,
    PropertyDetails details) {
  dictionary->UpdateMaxNumberKey(key, dictionary_holder);
  return AtPut(isolate, dictionary, key, value, details);
}

void NumberDictionary::CopyValuesTo(FixedArray elements) {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  int pos = 0;
  int capacity = this->Capacity();
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = elements.GetWriteBarrierMode(no_gc);
  for (int i = 0; i < capacity; i++) {
    Object k;
    if (this->ToKey(roots, i, &k)) {
      elements.set(pos++, this->ValueAt(i), mode);
    }
  }
  DCHECK_EQ(pos, elements.length());
}

template <typename Derived, typename Shape>
int Dictionary<Derived, Shape>::NumberOfEnumerableProperties() {
  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  int capacity = this->Capacity();
  int result = 0;
  for (int i = 0; i < capacity; i++) {
    Object k;
    if (!this->ToKey(roots, i, &k)) continue;
    if (k.FilterKey(ENUMERABLE_STRINGS)) continue;
    PropertyDetails details = this->DetailsAt(i);
    PropertyAttributes attr = details.attributes();
    if ((attr & ONLY_ENUMERABLE) == 0) result++;
  }
  return result;
}

template <typename Dictionary>
struct EnumIndexComparator {
  explicit EnumIndexComparator(Dictionary dict) : dict(dict) {}
  bool operator()(Tagged_t a, Tagged_t b) {
    PropertyDetails da(dict.DetailsAt(Smi(static_cast<Address>(a)).value()));
    PropertyDetails db(dict.DetailsAt(Smi(static_cast<Address>(b)).value()));
    return da.dictionary_index() < db.dictionary_index();
  }
  Dictionary dict;
};

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::CopyEnumKeysTo(
    Isolate* isolate, Handle<Derived> dictionary, Handle<FixedArray> storage,
    KeyCollectionMode mode, KeyAccumulator* accumulator) {
  DCHECK_IMPLIES(mode != KeyCollectionMode::kOwnOnly, accumulator != nullptr);
  int length = storage->length();
  int capacity = dictionary->Capacity();
  int properties = 0;
  ReadOnlyRoots roots(isolate);
  for (int i = 0; i < capacity; i++) {
    Object key;
    if (!dictionary->ToKey(roots, i, &key)) continue;
    bool is_shadowing_key = false;
    if (key.IsSymbol()) continue;
    PropertyDetails details = dictionary->DetailsAt(i);
    if (details.IsDontEnum()) {
      if (mode == KeyCollectionMode::kIncludePrototypes) {
        is_shadowing_key = true;
      } else {
        continue;
      }
    }
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
  Derived raw_dictionary = *dictionary;
  FixedArray raw_storage = *storage;
  EnumIndexComparator<Derived> cmp(raw_dictionary);
  // Use AtomicSlot wrapper to ensure that std::sort uses atomic load and
  // store operations that are safe for concurrent marking.
  AtomicSlot start(storage->GetFirstElementAddress());
  std::sort(start, start + length, cmp);
  for (int i = 0; i < length; i++) {
    int index = Smi::ToInt(raw_storage.get(i));
    raw_storage.set(i, raw_dictionary.NameAt(index));
  }
}

template <typename Derived, typename Shape>
Handle<FixedArray> BaseNameDictionary<Derived, Shape>::IterationIndices(
    Isolate* isolate, Handle<Derived> dictionary) {
  int capacity = dictionary->Capacity();
  int length = dictionary->NumberOfElements();
  Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);
  ReadOnlyRoots roots(isolate);
  int array_size = 0;
  {
    DisallowHeapAllocation no_gc;
    Derived raw_dictionary = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object k;
      if (!raw_dictionary.ToKey(roots, i, &k)) continue;
      array->set(array_size++, Smi::FromInt(i));
    }

    DCHECK_EQ(array_size, length);

    EnumIndexComparator<Derived> cmp(raw_dictionary);
    // Use AtomicSlot wrapper to ensure that std::sort uses atomic load and
    // store operations that are safe for concurrent marking.
    AtomicSlot start(array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }
  return FixedArray::ShrinkOrEmpty(isolate, array, array_size);
}

template <typename Derived, typename Shape>
void BaseNameDictionary<Derived, Shape>::CollectKeysTo(
    Handle<Derived> dictionary, KeyAccumulator* keys) {
  Isolate* isolate = keys->isolate();
  ReadOnlyRoots roots(isolate);
  int capacity = dictionary->Capacity();
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(dictionary->NumberOfElements());
  int array_size = 0;
  PropertyFilter filter = keys->filter();
  {
    DisallowHeapAllocation no_gc;
    Derived raw_dictionary = *dictionary;
    for (int i = 0; i < capacity; i++) {
      Object k;
      if (!raw_dictionary.ToKey(roots, i, &k)) continue;
      if (k.FilterKey(filter)) continue;
      PropertyDetails details = raw_dictionary.DetailsAt(i);
      if ((details.attributes() & filter) != 0) {
        keys->AddShadowingKey(k);
        continue;
      }
      if (filter & ONLY_ALL_CAN_READ) {
        if (details.kind() != kAccessor) continue;
        Object accessors = raw_dictionary.ValueAt(i);
        if (!accessors.IsAccessorInfo()) continue;
        if (!AccessorInfo::cast(accessors).all_can_read()) continue;
      }
      array->set(array_size++, Smi::FromInt(i));
    }

    EnumIndexComparator<Derived> cmp(raw_dictionary);
    // Use AtomicSlot wrapper to ensure that std::sort uses atomic load and
    // store operations that are safe for concurrent marking.
    AtomicSlot start(array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }

  bool has_seen_symbol = false;
  for (int i = 0; i < array_size; i++) {
    int index = Smi::ToInt(array->get(i));
    Object key = dictionary->NameAt(index);
    if (key.IsSymbol()) {
      has_seen_symbol = true;
      continue;
    }
    keys->AddKey(key, DO_NOT_CONVERT);
  }
  if (has_seen_symbol) {
    for (int i = 0; i < array_size; i++) {
      int index = Smi::ToInt(array->get(i));
      Object key = dictionary->NameAt(index);
      if (!key.IsSymbol()) continue;
      keys->AddKey(key, DO_NOT_CONVERT);
    }
  }
}

// Backwards lookup (slow).
template <typename Derived, typename Shape>
Object Dictionary<Derived, Shape>::SlowReverseLookup(Object value) {
  Derived dictionary = Derived::cast(*this);
  ReadOnlyRoots roots = dictionary.GetReadOnlyRoots();
  int capacity = dictionary.Capacity();
  for (int i = 0; i < capacity; i++) {
    Object k;
    if (!dictionary.ToKey(roots, i, &k)) continue;
    Object e = dictionary.ValueAt(i);
    if (e == value) return k;
  }
  return roots.undefined_value();
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::FillEntriesWithHoles(
    Handle<Derived> table) {
  int length = table->length();
  for (int i = Derived::EntryToIndex(0); i < length; i++) {
    table->set_the_hole(i);
  }
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(ReadOnlyRoots roots,
                                                   Handle<Object> key,
                                                   int32_t hash) {
  DisallowHeapAllocation no_gc;
  DCHECK(this->IsKey(roots, *key));

  int entry = this->FindEntry(roots, key, hash);
  if (entry == kNotFound) return roots.the_hole_value();
  return this->get(Derived::EntryToIndex(entry) + 1);
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key) {
  DisallowHeapAllocation no_gc;

  ReadOnlyRoots roots = this->GetReadOnlyRoots();
  DCHECK(this->IsKey(roots, *key));

  // If the object does not have an identity hash, it was never used as a key.
  Object hash = key->GetHash();
  if (hash.IsUndefined(roots)) {
    return roots.the_hole_value();
  }
  return Lookup(roots, key, Smi::ToInt(hash));
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key,
                                                   int32_t hash) {
  return Lookup(this->GetReadOnlyRoots(), key, hash);
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::ValueAt(int entry) {
  return this->get(EntryToValueIndex(entry));
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Put(Handle<Derived> table,
                                                         Handle<Object> key,
                                                         Handle<Object> value) {
  Isolate* isolate = Heap::FromWritableHeapObject(*table)->isolate();
  DCHECK(table->IsKey(ReadOnlyRoots(isolate), *key));
  DCHECK(!value->IsTheHole(ReadOnlyRoots(isolate)));

  // Make sure the key object has an identity hash code.
  int32_t hash = key->GetOrCreateHash(isolate).value();

  return ObjectHashTableBase<Derived, Shape>::Put(isolate, table, key, value,
                                                  hash);
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Put(Isolate* isolate,
                                                         Handle<Derived> table,
                                                         Handle<Object> key,
                                                         Handle<Object> value,
                                                         int32_t hash) {
  ReadOnlyRoots roots(isolate);
  DCHECK(table->IsKey(roots, *key));
  DCHECK(!value->IsTheHole(roots));

  int entry = table->FindEntry(roots, key, hash);

  // Key is already in table, just overwrite value.
  if (entry != kNotFound) {
    table->set(Derived::EntryToValueIndex(entry), *value);
    return table;
  }

  // Rehash if more than 33% of the entries are deleted entries.
  // TODO(jochen): Consider to shrink the fixed array in place.
  if ((table->NumberOfDeletedElements() << 1) > table->NumberOfElements()) {
    table->Rehash(roots);
  }
  // If we're out of luck, we didn't get a GC recently, and so rehashing
  // isn't enough to avoid a crash.
  if (!table->HasSufficientCapacityToAdd(1)) {
    int nof = table->NumberOfElements() + 1;
    int capacity = ObjectHashTable::ComputeCapacity(nof * 2);
    if (capacity > ObjectHashTable::kMaxCapacity) {
      for (size_t i = 0; i < 2; ++i) {
        isolate->heap()->CollectAllGarbage(
            Heap::kNoGCFlags, GarbageCollectionReason::kFullHashtable);
      }
      table->Rehash(roots);
    }
  }

  // Check whether the hash table should be extended.
  table = Derived::EnsureCapacity(isolate, table, 1);
  table->AddEntry(table->FindInsertionEntry(hash), *key, *value);
  return table;
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Remove(
    Isolate* isolate, Handle<Derived> table, Handle<Object> key,
    bool* was_present) {
  DCHECK(table->IsKey(table->GetReadOnlyRoots(), *key));

  Object hash = key->GetHash();
  if (hash.IsUndefined()) {
    *was_present = false;
    return table;
  }

  return Remove(isolate, table, key, was_present, Smi::ToInt(hash));
}

template <typename Derived, typename Shape>
Handle<Derived> ObjectHashTableBase<Derived, Shape>::Remove(
    Isolate* isolate, Handle<Derived> table, Handle<Object> key,
    bool* was_present, int32_t hash) {
  ReadOnlyRoots roots = table->GetReadOnlyRoots();
  DCHECK(table->IsKey(roots, *key));

  int entry = table->FindEntry(roots, key, hash);
  if (entry == kNotFound) {
    *was_present = false;
    return table;
  }

  *was_present = true;
  table->RemoveEntry(entry);
  return Derived::Shrink(isolate, table);
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::AddEntry(int entry, Object key,
                                                   Object value) {
  Derived* self = static_cast<Derived*>(this);
  self->set_key(Derived::EntryToIndex(entry), key);
  self->set(Derived::EntryToValueIndex(entry), value);
  self->ElementAdded();
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::RemoveEntry(int entry) {
  this->set_the_hole(Derived::EntryToIndex(entry));
  this->set_the_hole(Derived::EntryToValueIndex(entry));
  this->ElementRemoved();
}

void JSSet::Initialize(Handle<JSSet> set, Isolate* isolate) {
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  set->set_table(*table);
}

void JSSet::Clear(Isolate* isolate, Handle<JSSet> set) {
  Handle<OrderedHashSet> table(OrderedHashSet::cast(set->table()), isolate);
  table = OrderedHashSet::Clear(isolate, table);
  set->set_table(*table);
}

void JSMap::Initialize(Handle<JSMap> map, Isolate* isolate) {
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  map->set_table(*table);
}

void JSMap::Clear(Isolate* isolate, Handle<JSMap> map) {
  Handle<OrderedHashMap> table(OrderedHashMap::cast(map->table()), isolate);
  table = OrderedHashMap::Clear(isolate, table);
  map->set_table(*table);
}

void JSWeakCollection::Initialize(Handle<JSWeakCollection> weak_collection,
                                  Isolate* isolate) {
  Handle<EphemeronHashTable> table = EphemeronHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
}

void JSWeakCollection::Set(Handle<JSWeakCollection> weak_collection,
                           Handle<Object> key, Handle<Object> value,
                           int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()),
      weak_collection->GetIsolate());
  DCHECK(table->IsKey(weak_collection->GetReadOnlyRoots(), *key));
  Handle<EphemeronHashTable> new_table = EphemeronHashTable::Put(
      weak_collection->GetIsolate(), table, key, value, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    EphemeronHashTable::FillEntriesWithHoles(table);
  }
}

bool JSWeakCollection::Delete(Handle<JSWeakCollection> weak_collection,
                              Handle<Object> key, int32_t hash) {
  DCHECK(key->IsJSReceiver() || key->IsSymbol());
  Handle<EphemeronHashTable> table(
      EphemeronHashTable::cast(weak_collection->table()),
      weak_collection->GetIsolate());
  DCHECK(table->IsKey(weak_collection->GetReadOnlyRoots(), *key));
  bool was_present = false;
  Handle<EphemeronHashTable> new_table = EphemeronHashTable::Remove(
      weak_collection->GetIsolate(), table, key, &was_present, hash);
  weak_collection->set_table(*new_table);
  if (*table != *new_table) {
    // Zap the old table since we didn't record slots for its elements.
    EphemeronHashTable::FillEntriesWithHoles(table);
  }
  return was_present;
}

Handle<JSArray> JSWeakCollection::GetEntries(Handle<JSWeakCollection> holder,
                                             int max_entries) {
  Isolate* isolate = holder->GetIsolate();
  Handle<EphemeronHashTable> table(EphemeronHashTable::cast(holder->table()),
                                   isolate);
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
    ReadOnlyRoots roots = ReadOnlyRoots(isolate);
    int count = 0;
    for (int i = 0;
         count / values_per_entry < max_entries && i < table->Capacity(); i++) {
      Object key;
      if (table->ToKey(roots, i, &key)) {
        entries->set(count++, key);
        if (values_per_entry > 1) {
          Object value = table->Lookup(handle(key, isolate));
          entries->set(count++, value);
        }
      }
    }
    DCHECK_EQ(max_entries * values_per_entry, count);
  }
  return isolate->factory()->NewJSArrayWithElements(entries);
}

Handle<PropertyCell> PropertyCell::InvalidateEntry(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry) {
  // Swap with a copy.
  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  Handle<Name> name(cell->name(), isolate);
  Handle<PropertyCell> new_cell = isolate->factory()->NewPropertyCell(name);
  new_cell->set_value(cell->value());
  dictionary->ValueAtPut(entry, *new_cell);
  bool is_the_hole = cell->value().IsTheHole(isolate);
  // Cell is officially mutable henceforth.
  PropertyDetails details = cell->property_details();
  details = details.set_cell_type(is_the_hole ? PropertyCellType::kUninitialized
                                              : PropertyCellType::kMutable);
  new_cell->set_property_details(details);
  // Old cell is ready for invalidation.
  if (is_the_hole) {
    cell->set_value(ReadOnlyRoots(isolate).undefined_value());
  } else {
    cell->set_value(ReadOnlyRoots(isolate).the_hole_value());
  }
  details = details.set_cell_type(PropertyCellType::kInvalidated);
  cell->set_property_details(details);
  cell->dependent_code().DeoptimizeDependentCodeGroup(
      isolate, DependentCode::kPropertyCellChangedGroup);
  return new_cell;
}

PropertyCellConstantType PropertyCell::GetConstantType() {
  if (value().IsSmi()) return PropertyCellConstantType::kSmi;
  return PropertyCellConstantType::kStableMap;
}

static bool RemainsConstantType(Handle<PropertyCell> cell,
                                Handle<Object> value) {
  // TODO(dcarney): double->smi and smi->double transition from kConstant
  if (cell->value().IsSmi() && value->IsSmi()) {
    return true;
  } else if (cell->value().IsHeapObject() && value->IsHeapObject()) {
    return HeapObject::cast(cell->value()).map() ==
               HeapObject::cast(*value).map() &&
           HeapObject::cast(*value).map().is_stable();
  }
  return false;
}

PropertyCellType PropertyCell::UpdatedType(Isolate* isolate,
                                           Handle<PropertyCell> cell,
                                           Handle<Object> value,
                                           PropertyDetails details) {
  PropertyCellType type = details.cell_type();
  DCHECK(!value->IsTheHole(isolate));
  if (cell->value().IsTheHole(isolate)) {
    switch (type) {
      // Only allow a cell to transition once into constant state.
      case PropertyCellType::kUninitialized:
        if (value->IsUndefined(isolate)) return PropertyCellType::kUndefined;
        return PropertyCellType::kConstant;
      case PropertyCellType::kInvalidated:
        return PropertyCellType::kMutable;
      default:
        UNREACHABLE();
    }
  }
  switch (type) {
    case PropertyCellType::kUndefined:
      return PropertyCellType::kConstant;
    case PropertyCellType::kConstant:
      if (*value == cell->value()) return PropertyCellType::kConstant;
      V8_FALLTHROUGH;
    case PropertyCellType::kConstantType:
      if (RemainsConstantType(cell, value)) {
        return PropertyCellType::kConstantType;
      }
      V8_FALLTHROUGH;
    case PropertyCellType::kMutable:
      return PropertyCellType::kMutable;
  }
  UNREACHABLE();
}

Handle<PropertyCell> PropertyCell::PrepareForValue(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, int entry,
    Handle<Object> value, PropertyDetails details) {
  DCHECK(!value->IsTheHole(isolate));
  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  const PropertyDetails original_details = cell->property_details();
  // Data accesses could be cached in ics or optimized code.
  bool invalidate =
      (original_details.kind() == kData && details.kind() == kAccessor) ||
      (!original_details.IsReadOnly() && details.IsReadOnly());
  int index;
  PropertyCellType old_type = original_details.cell_type();
  // Preserve the enumeration index unless the property was deleted or never
  // initialized.
  if (cell->value().IsTheHole(isolate)) {
    index = dictionary->NextEnumerationIndex();
    dictionary->SetNextEnumerationIndex(index + 1);
  } else {
    index = original_details.dictionary_index();
  }
  DCHECK_LT(0, index);
  details = details.set_index(index);

  PropertyCellType new_type =
      UpdatedType(isolate, cell, value, original_details);
  if (invalidate) {
    cell = PropertyCell::InvalidateEntry(isolate, dictionary, entry);
  }

  // Install new property details.
  details = details.set_cell_type(new_type);
  cell->set_property_details(details);

  if (new_type == PropertyCellType::kConstant ||
      new_type == PropertyCellType::kConstantType) {
    // Store the value now to ensure that the cell contains the constant or
    // type information. Otherwise subsequent store operation will turn
    // the cell to mutable.
    cell->set_value(*value);
  }

  // Deopt when transitioning from a constant type.
  if (!invalidate && (old_type != new_type ||
                      original_details.IsReadOnly() != details.IsReadOnly())) {
    cell->dependent_code().DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
  return cell;
}

// static
void PropertyCell::SetValueWithInvalidation(Isolate* isolate,
                                            const char* cell_name,
                                            Handle<PropertyCell> cell,
                                            Handle<Object> new_value) {
  if (cell->value() != *new_value) {
    if (FLAG_trace_protector_invalidation) {
      isolate->TraceProtectorInvalidation(cell_name);
    }
    cell->set_value(*new_value);
    cell->dependent_code().DeoptimizeDependentCodeGroup(
        isolate, DependentCode::kPropertyCellChangedGroup);
  }
}

int JSGeneratorObject::source_position() const {
  CHECK(is_suspended());
  DCHECK(function().shared().HasBytecodeArray());
  DCHECK(function().shared().GetBytecodeArray().HasSourcePositionTable());

  int code_offset = Smi::ToInt(input_or_debug_pos());

  // The stored bytecode offset is relative to a different base than what
  // is used in the source position table, hence the subtraction.
  code_offset -= BytecodeArray::kHeaderSize - kHeapObjectTag;
  AbstractCode code =
      AbstractCode::cast(function().shared().GetBytecodeArray());
  return code.SourcePosition(code_offset);
}

// static
AccessCheckInfo AccessCheckInfo::Get(Isolate* isolate,
                                     Handle<JSObject> receiver) {
  DisallowHeapAllocation no_gc;
  DCHECK(receiver->map().is_access_check_needed());
  Object maybe_constructor = receiver->map().GetConstructor();
  if (maybe_constructor.IsFunctionTemplateInfo()) {
    Object data_obj =
        FunctionTemplateInfo::cast(maybe_constructor).GetAccessCheckInfo();
    if (data_obj.IsUndefined(isolate)) return AccessCheckInfo();
    return AccessCheckInfo::cast(data_obj);
  }
  // Might happen for a detached context.
  if (!maybe_constructor.IsJSFunction()) return AccessCheckInfo();
  JSFunction constructor = JSFunction::cast(maybe_constructor);
  // Might happen for the debug context.
  if (!constructor.shared().IsApiFunction()) return AccessCheckInfo();

  Object data_obj =
      constructor.shared().get_api_func_data().GetAccessCheckInfo();
  if (data_obj.IsUndefined(isolate)) return AccessCheckInfo();

  return AccessCheckInfo::cast(data_obj);
}

MaybeHandle<Name> FunctionTemplateInfo::TryGetCachedPropertyName(
    Isolate* isolate, Handle<Object> getter) {
  if (getter->IsFunctionTemplateInfo()) {
    Handle<FunctionTemplateInfo> fti =
        Handle<FunctionTemplateInfo>::cast(getter);
    // Check if the accessor uses a cached property.
    if (!fti->cached_property_name().IsTheHole(isolate)) {
      return handle(Name::cast(fti->cached_property_name()), isolate);
    }
  }
  return MaybeHandle<Name>();
}

Address Smi::LexicographicCompare(Isolate* isolate, Smi x, Smi y) {
  DisallowHeapAllocation no_allocation;
  DisallowJavascriptExecution no_js(isolate);

  int x_value = Smi::ToInt(x);
  int y_value = Smi::ToInt(y);

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(0).ptr();

  // If one of the integers is zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0) {
    return Smi::FromInt(x_value < y_value ? -1 : 1).ptr();
  }

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.

  // Use unsigned values otherwise the logic is incorrect for -MIN_INT on
  // architectures using 32-bit Smis.
  uint32_t x_scaled = x_value;
  uint32_t y_scaled = y_value;
  if (x_value < 0) {
    if (y_value >= 0) {
      return Smi::FromInt(-1).ptr();
    } else {
      y_scaled = base::NegateWithWraparound(y_value);
    }
    x_scaled = base::NegateWithWraparound(x_value);
  } else if (y_value < 0) {
    return Smi::FromInt(1).ptr();
  }

  // clang-format off
  static const uint32_t kPowersOf10[] = {
      1,                 10,                100,         1000,
      10 * 1000,         100 * 1000,        1000 * 1000, 10 * 1000 * 1000,
      100 * 1000 * 1000, 1000 * 1000 * 1000};
  // clang-format on

  // If the integers have the same number of decimal digits they can be
  // compared directly as the numeric order is the same as the
  // lexicographic order.  If one integer has fewer digits, it is scaled
  // by some power of 10 to have the same number of digits as the longer
  // integer.  If the scaled integers are equal it means the shorter
  // integer comes first in the lexicographic order.

  // From http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
  int x_log2 = 31 - base::bits::CountLeadingZeros(x_scaled);
  int x_log10 = ((x_log2 + 1) * 1233) >> 12;
  x_log10 -= x_scaled < kPowersOf10[x_log10];

  int y_log2 = 31 - base::bits::CountLeadingZeros(y_scaled);
  int y_log10 = ((y_log2 + 1) * 1233) >> 12;
  y_log10 -= y_scaled < kPowersOf10[y_log10];

  int tie = 0;

  if (x_log10 < y_log10) {
    // X has fewer digits.  We would like to simply scale up X but that
    // might overflow, e.g when comparing 9 with 1_000_000_000, 9 would
    // be scaled up to 9_000_000_000. So we scale up by the next
    // smallest power and scale down Y to drop one digit. It is OK to
    // drop one digit from the longer integer since the final digit is
    // past the length of the shorter integer.
    x_scaled *= kPowersOf10[y_log10 - x_log10 - 1];
    y_scaled /= 10;
    tie = -1;
  } else if (y_log10 < x_log10) {
    y_scaled *= kPowersOf10[x_log10 - y_log10 - 1];
    x_scaled /= 10;
    tie = 1;
  }

  if (x_scaled < y_scaled) return Smi::FromInt(-1).ptr();
  if (x_scaled > y_scaled) return Smi::FromInt(1).ptr();
  return Smi::FromInt(tie).ptr();
}

// Force instantiation of template instances class.
// Please note this list is compiler dependent.
// Keep this at the end of this file

template class HashTable<StringTable, StringTableShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<CompilationCacheTable, CompilationCacheShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<ObjectHashTable, ObjectHashTableShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<ObjectHashSet, ObjectHashSetShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    ObjectHashTableBase<ObjectHashTable, ObjectHashTableShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<EphemeronHashTable, EphemeronHashTableShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    ObjectHashTableBase<EphemeronHashTable, EphemeronHashTableShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    BaseNameDictionary<NameDictionary, NameDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    BaseNameDictionary<GlobalDictionary, GlobalDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<NameDictionary, NameDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<GlobalDictionary, GlobalDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(
    V8_EXPORT_PRIVATE) HashTable<NumberDictionary, NumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<NumberDictionary, NumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    HashTable<SimpleNumberDictionary, SimpleNumberDictionaryShape>;

template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)
    Dictionary<SimpleNumberDictionary, SimpleNumberDictionaryShape>;

template Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape>::New(Isolate*, int,
                                                    AllocationType,
                                                    MinimumCapacity);

template V8_EXPORT_PRIVATE Handle<NameDictionary>
HashTable<NameDictionary, NameDictionaryShape>::Shrink(Isolate* isolate,
                                                       Handle<NameDictionary>,
                                                       int additionalCapacity);

void JSFinalizationGroup::Cleanup(
    Isolate* isolate, Handle<JSFinalizationGroup> finalization_group,
    Handle<Object> cleanup) {
  DCHECK(cleanup->IsCallable());
  // It's possible that the cleared_cells list is empty, since
  // FinalizationGroup.unregister() removed all its elements before this task
  // ran. In that case, don't call the cleanup function.
  if (!finalization_group->cleared_cells().IsUndefined(isolate)) {
    // Construct the iterator.
    Handle<JSFinalizationGroupCleanupIterator> iterator;
    {
      Handle<Map> cleanup_iterator_map(
          isolate->native_context()
              ->js_finalization_group_cleanup_iterator_map(),
          isolate);
      iterator = Handle<JSFinalizationGroupCleanupIterator>::cast(
          isolate->factory()->NewJSObjectFromMap(
              cleanup_iterator_map, AllocationType::kYoung,
              Handle<AllocationSite>::null()));
      iterator->set_finalization_group(*finalization_group);
    }

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    v8::Local<v8::Value> result;
    MaybeHandle<Object> exception;
    Handle<Object> args[] = {iterator};
    bool has_pending_exception = !ToLocal<Value>(
        Execution::TryCall(
            isolate, cleanup,
            handle(ReadOnlyRoots(isolate).undefined_value(), isolate), 1, args,
            Execution::MessageHandling::kReport, &exception),
        &result);
    // TODO(marja): (spec): What if there's an exception?
    USE(has_pending_exception);

    // TODO(marja): (spec): Should the iterator be invalidated after the
    // function returns?
  }
}

MaybeHandle<FixedArray> JSReceiver::GetPrivateEntries(
    Isolate* isolate, Handle<JSReceiver> receiver) {
  PropertyFilter key_filter = static_cast<PropertyFilter>(PRIVATE_NAMES_ONLY);

  Handle<FixedArray> keys;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, keys,
      KeyAccumulator::GetKeys(receiver, KeyCollectionMode::kOwnOnly, key_filter,
                              GetKeysConversion::kConvertToString),
      MaybeHandle<FixedArray>());

  Handle<FixedArray> entries =
      isolate->factory()->NewFixedArray(keys->length() * 2);
  int length = 0;

  for (int i = 0; i < keys->length(); ++i) {
    Handle<Object> obj_key = handle(keys->get(i), isolate);
    Handle<Symbol> key(Symbol::cast(*obj_key), isolate);
    CHECK(key->is_private_name());
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, Object::GetProperty(isolate, receiver, key),
        MaybeHandle<FixedArray>());

    entries->set(length++, *key);
    entries->set(length++, *value);
  }
  DCHECK_EQ(length, entries->length());
  return FixedArray::ShrinkOrEmpty(isolate, entries, length);
}

}  // namespace internal
}  // namespace v8
