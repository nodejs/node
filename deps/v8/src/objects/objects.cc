// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <vector>

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
#include "src/diagnostics/code-tracer.h"
#include "src/execution/arguments.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate-utils-inl.h"
#include "src/execution/isolate-utils.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/protectors-inl.h"
#include "src/heap/factory-inl.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/allocation-site-scopes.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/arguments-inl.h"
#include "src/objects/bigint.h"
#include "src/objects/call-site-info-inl.h"
#include "src/objects/cell-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/compilation-cache-table-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/elements.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/field-index.h"
#include "src/objects/field-type.h"
#include "src/objects/foreign.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/function-kind.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/keys.h"
#include "src/objects/lookup-inl.h"
#include "src/objects/map-updater.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-details.h"
#include "src/roots/roots.h"
#include "src/snapshot/deserializer.h"
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
#include "src/objects/js-segments.h"
#endif  // V8_INTL_SUPPORT
#include "src/codegen/source-position-table.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/map.h"
#include "src/objects/megadom-handler-inl.h"
#include "src/objects/microtask-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/promise-inl.h"
#include "src/objects/property-descriptor-object-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/prototype.h"
#include "src/objects/slots-atomic-inl.h"
#include "src/objects/string-comparator.h"
#include "src/objects/string-set-inl.h"
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
#include "src/utils/hex-format.h"
#include "src/utils/ostreams.h"
#include "src/utils/sha-256.h"
#include "src/utils/utils-inl.h"
#include "src/zone/zone.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

ShouldThrow GetShouldThrow(Isolate* isolate, Maybe<ShouldThrow> should_throw) {
  if (should_throw.IsJust()) return should_throw.FromJust();

  LanguageMode mode = isolate->context().scope_info().language_mode();
  if (mode == LanguageMode::kStrict) return kThrowOnError;

  for (StackFrameIterator it(isolate); !it.done(); it.Advance()) {
    if (!it.frame()->is_java_script()) continue;

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
  if (InstanceTypeChecker::IsJSApiObject(instance_type)) {
    return os << "[api object] "
              << static_cast<int16_t>(instance_type) -
                     i::Internals::kFirstJSApiObjectType;
  }
  switch (instance_type) {
#define WRITE_TYPE(TYPE) \
  case TYPE:             \
    return os << #TYPE;
    INSTANCE_TYPE_LIST(WRITE_TYPE)
#undef WRITE_TYPE
  }
  UNREACHABLE();
}

std::ostream& operator<<(std::ostream& os, PropertyCellType type) {
  switch (type) {
    case PropertyCellType::kUndefined:
      return os << "Undefined";
    case PropertyCellType::kConstant:
      return os << "Constant";
    case PropertyCellType::kConstantType:
      return os << "ConstantType";
    case PropertyCellType::kMutable:
      return os << "Mutable";
    case PropertyCellType::kInTransition:
      return os << "InTransition";
  }
  UNREACHABLE();
}

Handle<FieldType> Object::OptimalType(Isolate* isolate,
                                      Representation representation) {
  if (representation.IsNone()) return FieldType::None(isolate);
  if (v8_flags.track_field_types) {
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
  auto result = isolate->factory()->NewHeapNumberWithHoleNaN();
  if (object->IsUninitialized(isolate)) {
    result->set_value_as_bits(kHoleNanInt64, kRelaxedStore);
  } else if (object->IsHeapNumber()) {
    // Ensure that all bits of the double value are preserved.
    result->set_value_as_bits(
        HeapNumber::cast(*object).value_as_bits(kRelaxedLoad), kRelaxedStore);
  } else {
    result->set_value(object->Number(), kRelaxedStore);
  }
  return result;
}

template <AllocationType allocation_type, typename IsolateT>
Handle<Object> Object::WrapForRead(IsolateT* isolate, Handle<Object> object,
                                   Representation representation) {
  DCHECK(!object->IsUninitialized(isolate));
  if (!representation.IsDouble()) {
    DCHECK(object->FitsRepresentation(representation));
    return object;
  }
  return isolate->factory()->template NewHeapNumberFromBits<allocation_type>(
      HeapNumber::cast(*object).value_as_bits(kRelaxedLoad));
}

template Handle<Object> Object::WrapForRead<AllocationType::kYoung>(
    Isolate* isolate, Handle<Object> object, Representation representation);
template Handle<Object> Object::WrapForRead<AllocationType::kOld>(
    LocalIsolate* isolate, Handle<Object> object,
    Representation representation);

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
        JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(input),
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
      isolate, input,
      Object::ToPrimitive(isolate, input, ToPrimitiveHint::kString), Name);
  if (input->IsName()) return Handle<Name>::cast(input);
  return ToString(isolate, input);
}

// ES6 7.1.14
// static
MaybeHandle<Object> Object::ConvertToPropertyKey(Isolate* isolate,
                                                 Handle<Object> value) {
  // 1. Let key be ToPrimitive(argument, hint String).
  MaybeHandle<Object> maybe_key =
      Object::ToPrimitive(isolate, value, ToPrimitiveHint::kString);
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
        JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(input),
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
  Handle<Symbol> symbol = isolate->factory()->error_stack_symbol();
  return JSReceiver::HasOwnProperty(isolate, Handle<JSReceiver>::cast(object),
                                    symbol)
      .FromMaybe(false);
}

Handle<String> AsStringOrEmpty(Isolate* isolate, Handle<Object> object) {
  return object->IsString() ? Handle<String>::cast(object)
                            : isolate->factory()->empty_string();
}

Handle<String> NoSideEffectsErrorToString(Isolate* isolate,
                                          Handle<JSReceiver> error) {
  Handle<Name> name_key = isolate->factory()->name_string();
  Handle<Object> name = JSReceiver::GetDataProperty(isolate, error, name_key);
  Handle<String> name_str = AsStringOrEmpty(isolate, name);

  Handle<Name> msg_key = isolate->factory()->message_string();
  Handle<Object> msg = JSReceiver::GetDataProperty(isolate, error, msg_key);
  Handle<String> msg_str = AsStringOrEmpty(isolate, msg);

  if (name_str->length() == 0) return msg_str;
  if (msg_str->length() == 0) return name_str;

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(name_str);
  builder.AppendCStringLiteral(": ");

  if (builder.Length() + msg_str->length() <= String::kMaxLength) {
    builder.AppendString(msg_str);
  } else {
    builder.AppendCStringLiteral("<a very large string>");
  }

  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
MaybeHandle<String> Object::NoSideEffectsToMaybeString(Isolate* isolate,
                                                       Handle<Object> input) {
  DisallowJavascriptExecution no_js(isolate);

  if (input->IsString() || input->IsNumber() || input->IsOddball()) {
    return Object::ToString(isolate, input).ToHandleChecked();
  } else if (input->IsJSProxy()) {
    Handle<Object> currInput = input;
    do {
      HeapObject target = Handle<JSProxy>::cast(currInput)->target(isolate);
      currInput = Handle<Object>(target, isolate);
    } while (currInput->IsJSProxy());
    return NoSideEffectsToString(isolate, currInput);
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
    } else if (input->IsJSWrappedFunction()) {
      fun_str =
          JSWrappedFunction::ToString(Handle<JSWrappedFunction>::cast(input));
    } else {
      DCHECK(input->IsJSFunction());
      fun_str = JSFunction::ToString(Handle<JSFunction>::cast(input));
    }

    if (fun_str->length() > 128) {
      IncrementalStringBuilder builder(isolate);
      builder.AppendString(isolate->factory()->NewSubString(fun_str, 0, 111));
      builder.AppendCStringLiteral("...<omitted>...");
      builder.AppendString(isolate->factory()->NewSubString(
          fun_str, fun_str->length() - 2, fun_str->length()));

      return builder.Finish().ToHandleChecked();
    }
    return fun_str;
  } else if (input->IsSymbol()) {
    // -- S y m b o l
    Handle<Symbol> symbol = Handle<Symbol>::cast(input);

    if (symbol->is_private_name()) {
      return Handle<String>(String::cast(symbol->description()), isolate);
    }

    IncrementalStringBuilder builder(isolate);
    builder.AppendCStringLiteral("Symbol(");
    if (symbol->description().IsString()) {
      builder.AppendString(
          handle(String::cast(symbol->description()), isolate));
    }
    builder.AppendCharacter(')');

    return builder.Finish().ToHandleChecked();
  } else if (input->IsJSReceiver()) {
    // -- J S R e c e i v e r
    Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(input);
    Handle<Object> to_string = JSReceiver::GetDataProperty(
        isolate, receiver, isolate->factory()->toString_string());

    if (IsErrorObject(isolate, input) ||
        *to_string == *isolate->error_to_string()) {
      // When internally formatting error objects, use a side-effects-free
      // version of Error.prototype.toString independent of the actually
      // installed toString method.
      return NoSideEffectsErrorToString(isolate,
                                        Handle<JSReceiver>::cast(input));
    } else if (*to_string == *isolate->object_to_string()) {
      Handle<Object> ctor = JSReceiver::GetDataProperty(
          isolate, receiver, isolate->factory()->constructor_string());
      if (ctor->IsFunction()) {
        Handle<String> ctor_name;
        if (ctor->IsJSBoundFunction()) {
          ctor_name = JSBoundFunction::GetName(
                          isolate, Handle<JSBoundFunction>::cast(ctor))
                          .ToHandleChecked();
        } else if (ctor->IsJSFunction()) {
          ctor_name =
              JSFunction::GetName(isolate, Handle<JSFunction>::cast(ctor));
        }

        if (ctor_name->length() != 0) {
          IncrementalStringBuilder builder(isolate);
          builder.AppendCStringLiteral("#<");
          builder.AppendString(ctor_name);
          builder.AppendCharacter('>');

          return builder.Finish().ToHandleChecked();
        }
      }
    }
  }
  return MaybeHandle<String>(kNullMaybeHandle);
}

// static
Handle<String> Object::NoSideEffectsToString(Isolate* isolate,
                                             Handle<Object> input) {
  DisallowJavascriptExecution no_js(isolate);

  // Try to convert input to a meaningful string.
  MaybeHandle<String> maybe_string = NoSideEffectsToMaybeString(isolate, input);
  Handle<String> string_handle;
  if (maybe_string.ToHandle(&string_handle)) {
    return string_handle;
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
      isolate, receiver, isolate->factory()->to_string_tag_symbol());
  Handle<String> tag =
      tag_obj->IsString() ? Handle<String>::cast(tag_obj) : builtin_tag;

  IncrementalStringBuilder builder(isolate);
  builder.AppendCStringLiteral("[object ");
  builder.AppendString(tag);
  builder.AppendCharacter(']');

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
    return handle(Smi::zero(), isolate);
  } else if (len >= kMaxSafeInteger) {
    len = kMaxSafeInteger;
  }
  return isolate->factory()->NewNumber(len);
}

// static
MaybeHandle<Object> Object::ConvertToIndex(Isolate* isolate,
                                           Handle<Object> input,
                                           MessageTemplate error_index) {
  if (input->IsUndefined(isolate)) return handle(Smi::zero(), isolate);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, input, ToNumber(isolate, input), Object);
  if (input->IsSmi() && Smi::ToInt(*input) >= 0) return input;
  double len = DoubleToInteger(input->Number());
  auto js_len = isolate->factory()->NewNumber(len);
  if (len < 0.0 || len > kMaxSafeInteger) {
    THROW_NEW_ERROR(isolate, NewRangeError(error_index, js_len), Object);
  }
  return js_len;
}

template <typename IsolateT>
bool Object::BooleanValue(IsolateT* isolate) {
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
template bool Object::BooleanValue(Isolate*);
template bool Object::BooleanValue(LocalIsolate*);

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
  if (!Object::ToPrimitive(isolate, x, ToPrimitiveHint::kNumber).ToHandle(&x) ||
      !Object::ToPrimitive(isolate, y, ToPrimitiveHint::kNumber).ToHandle(&y)) {
    return Nothing<ComparisonResult>();
  }
  if (x->IsString() && y->IsString()) {
    // ES6 section 7.2.11 Abstract Relational Comparison step 5.
    return Just(String::Compare(isolate, Handle<String>::cast(x),
                                Handle<String>::cast(y)));
  }
  if (x->IsBigInt() && y->IsString()) {
    return BigInt::CompareToString(isolate, Handle<BigInt>::cast(x),
                                   Handle<String>::cast(y));
  }
  if (x->IsString() && y->IsBigInt()) {
    Maybe<ComparisonResult> maybe_result = BigInt::CompareToString(
        isolate, Handle<BigInt>::cast(y), Handle<String>::cast(x));
    ComparisonResult result;
    if (maybe_result.To(&result)) {
      return Just(Reverse(result));
    } else {
      return Nothing<ComparisonResult>();
    }
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
        if (!JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(y))
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
        return BigInt::EqualToString(isolate, Handle<BigInt>::cast(y),
                                     Handle<String>::cast(x));
      } else if (y->IsJSReceiver()) {
        if (!JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(y))
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
        if (!JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(y))
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
        if (!JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(y))
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
      } else if (!JSReceiver::ToPrimitive(isolate, Handle<JSReceiver>::cast(x))
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
  ASSIGN_RETURN_ON_EXCEPTION(isolate, lhs, Object::ToPrimitive(isolate, lhs),
                             Object);
  ASSIGN_RETURN_ON_EXCEPTION(isolate, rhs, Object::ToPrimitive(isolate, rhs),
                             Object);
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
    // Since there is a mutual recursion here, we might run out of stack
    // space for long chains of bound functions.
    STACK_CHECK(isolate, MaybeHandle<Object>());
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
      size_t length = array->GetLength();
      if (array->IsDetachedOrOutOfBounds() ||
          length > static_cast<size_t>(FixedArray::kMaxLength)) {
        return MaybeHandle<FixedArray>();
      }
      static_assert(FixedArray::kMaxLength <=
                    std::numeric_limits<uint32_t>::max());
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
                                        bool is_global_reference) {
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
        if (is_global_reference) {
          Maybe<bool> maybe = JSProxy::HasProperty(
              it->isolate(), it->GetHolder<JSProxy>(), it->GetName());
          if (maybe.IsNothing()) return MaybeHandle<Object>();
          if (!maybe.FromJust()) {
            it->NotFound();
            return it->isolate()->factory()->undefined_value();
          }
        }
        MaybeHandle<Object> result =
            JSProxy::GetProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                 it->GetName(), receiver, &was_found);
        if (!was_found && !is_global_reference) it->NotFound();
        return result;
      }
      case LookupIterator::WASM_OBJECT:
        THROW_NEW_ERROR(it->isolate(),
                        NewTypeError(MessageTemplate::kWasmObjectsAreOpaque),
                        Object);
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
  Handle<JSReceiver> target(JSReceiver::cast(proxy->target()), isolate);
  // 6. Let trap be ? GetMethod(handler, "get").
  Handle<Object> trap;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, trap,
      Object::GetMethod(Handle<JSReceiver>::cast(handler), trap_name), Object);
  // 7. If trap is undefined, then
  if (trap->IsUndefined(isolate)) {
    // 7.a Return target.[[Get]](P, Receiver).
    PropertyKey key(isolate, name);
    LookupIterator it(isolate, receiver, key, target);
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

// static
Handle<TemplateList> TemplateList::New(Isolate* isolate, int size) {
  Handle<FixedArray> list = isolate->factory()->NewFixedArray(
      kLengthIndex + size, AllocationType::kOld);
  list->set(kLengthIndex, Smi::zero());
  return Handle<TemplateList>::cast(list);
}

// static
Handle<TemplateList> TemplateList::Add(Isolate* isolate,
                                       Handle<TemplateList> list,
                                       Handle<i::Object> value) {
  static_assert(kFirstElementIndex == 1);
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
                              isolate, receiver, holder, name, result),
                          Object);
    }
    return reboxed_result;
  }

  Handle<AccessorPair> accessor_pair = Handle<AccessorPair>::cast(structure);
  // AccessorPair with 'cached' private property.
  if (it->TryLookupCachedProperty(accessor_pair)) {
    return Object::GetProperty(it);
  }

  // Regular accessor.
  Handle<Object> getter(accessor_pair->getter(), isolate);
  if (getter->IsFunctionTemplateInfo()) {
    SaveAndSwitchContext save(isolate,
                              *holder->GetCreationContext().ToHandleChecked());
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
    SaveAndSwitchContext save(isolate,
                              *holder->GetCreationContext().ToHandleChecked());
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
  DisallowGarbageCollection no_alloc;
  if (IsSmi()) {
    Context native_context = isolate->context().native_context();
    return native_context.number_function().initial_map();
  }

  const HeapObject heap_object = HeapObject::cast(*this);
  return heap_object.map().GetPrototypeChainRootMap(isolate);
}

Smi Object::GetOrCreateHash(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
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
  if (!v8_flags.builtin_subclassing) return default_species;
  if (original_array->IsJSArray() &&
      Handle<JSArray>::cast(original_array)->HasArrayPrototype(isolate) &&
      Protectors::IsArraySpeciesLookupChainIntact(isolate)) {
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

  // Check that we have the original ArrayPrototype.
  i::HandleScope handle_scope(isolate);
  i::Handle<i::Context> context;
  if (!array.GetCreationContext().ToHandle(&context)) return false;
  if (!array.map().prototype().IsJSObject()) return true;
  JSObject array_proto = JSObject::cast(array.map().prototype());
  auto initial_array_prototype =
      context->native_context().initial_array_prototype();
  if (initial_array_prototype != array_proto) return true;

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  if (!Protectors::IsArrayIteratorLookupChainIntact(isolate)) return true;

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  ElementsKind array_kind = array.GetElementsKind();
  if (IsFastPackedElementsKind(array_kind)) return false;

  // For FastHoley kinds, an element access on a hole would cause a lookup on
  // the prototype. This could have different results if the prototype has been
  // changed.
  if (IsHoleyElementsKind(array_kind) &&
      Protectors::IsNoElementsIntact(isolate)) {
    return false;
  }
  return true;
}

bool Object::IsCodeLike(Isolate* isolate) const {
  DisallowGarbageCollection no_gc;
  return IsJSReceiver() && JSReceiver::cast(*this).IsCodeLike(isolate);
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

void Smi::SmiPrint(std::ostream& os) const { os << value(); }

void HeapObject::HeapObjectShortPrint(std::ostream& os) {
  PtrComprCageBase cage_base = GetPtrComprCageBaseSlow(*this);
  os << AsHex::Address(this->ptr()) << " ";

  if (IsString(cage_base)) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    String::cast(*this).StringShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  if (IsJSObject(cage_base)) {
    HeapStringAllocator allocator;
    StringStream accumulator(&allocator);
    JSObject::cast(*this).JSObjectShortPrint(&accumulator);
    os << accumulator.ToCString().get();
    return;
  }
  switch (map(cage_base).instance_type()) {
    case MAP_TYPE: {
      os << "<Map";
      Map mapInstance = Map::cast(*this);
      if (mapInstance.instance_size() != kVariableSizeSentinel) {
        os << "[" << mapInstance.instance_size() << "]";
      }
      os << "(";
      if (mapInstance.IsJSObjectMap()) {
        os << ElementsKindToString(mapInstance.elements_kind());
      } else {
        os << mapInstance.instance_type();
      }
      os << ")>";
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
    case SWISS_NAME_DICTIONARY_TYPE:
      os << "<SwissNameDictionary["
         << SwissNameDictionary::cast(*this).Capacity() << "]>";
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
      os << "<FreeSpace[" << FreeSpace::cast(*this).size(kRelaxedLoad) << "]>";
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
      std::unique_ptr<char[]> debug_name = shared.DebugNameCStr();
      if (debug_name[0] != '\0') {
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
      if (!scope.IsEmpty()) os << " " << scope.scope_type();
      os << ">";
      break;
    }
    case CODE_DATA_CONTAINER_TYPE: {
#ifdef V8_EXTERNAL_CODE_SPACE
      CodeDataContainer code = CodeDataContainer::cast(*this);
      os << "<CodeDataContainer " << CodeKindToString(code.kind());
      if (code.is_builtin()) {
        os << " " << Builtins::name(code.builtin_id());
      }
      os << ">";
#else
      os << "<CodeDataContainer>";
#endif  // V8_EXTERNAL_CODE_SPACE
      break;
    }
    case CODE_TYPE: {
      Code code = Code::cast(*this);
      os << "<Code " << CodeKindToString(code.kind());
      if (code.is_builtin()) {
        os << " " << Builtins::name(code.builtin_id());
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
      HeapNumber::cast(*this).HeapNumberShortPrint(os);
      os << ">";
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
      cell.value(kAcquireLoad).ShortPrint(&accumulator);
      os << accumulator.ToCString().get();
      os << '>';
      break;
    }
    case ACCESSOR_INFO_TYPE: {
      AccessorInfo info = AccessorInfo::cast(*this);
      os << "<AccessorInfo ";
      os << "name= " << Brief(info.name());
      os << ", data= " << Brief(info.data());
      os << ">";
      break;
    }
    case CALL_HANDLER_INFO_TYPE: {
      CallHandlerInfo info = CallHandlerInfo::cast(*this);
      os << "<CallHandlerInfo ";
      os << "callback= " << reinterpret_cast<void*>(info.callback());
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

void MegaDomHandler::BriefPrintDetails(std::ostream& os) {
  os << " " << Brief(accessor(kAcquireLoad)) << ", " << Brief(context());
}

void ClassPositions::BriefPrintDetails(std::ostream& os) {
  os << " " << start() << ", " << end();
}

void CallableTask::BriefPrintDetails(std::ostream& os) {
  os << " callable=" << Brief(callable());
}

void HeapObject::Iterate(PtrComprCageBase cage_base, ObjectVisitor* v) {
  IterateFast<ObjectVisitor>(cage_base, v);
}

void HeapObject::IterateBody(PtrComprCageBase cage_base, ObjectVisitor* v) {
  Map m = map(cage_base);
  IterateBodyFast<ObjectVisitor>(m, SizeFromMap(m), v);
}

void HeapObject::IterateBody(Map map, int object_size, ObjectVisitor* v) {
  IterateBodyFast<ObjectVisitor>(map, object_size, v);
}

struct CallIsValidSlot {
  template <typename BodyDescriptor>
  static bool apply(Map map, HeapObject obj, int offset) {
    return BodyDescriptor::IsValidSlot(map, obj, offset);
  }
};

bool HeapObject::IsValidSlot(Map map, int offset) {
  DCHECK_NE(0, offset);
  return BodyDescriptorApply<CallIsValidSlot>(map.instance_type(), map, *this,
                                              offset);
}

int HeapObject::SizeFromMap(Map map) const {
  int instance_size = map.instance_size();
  if (instance_size != kVariableSizeSentinel) return instance_size;
  // Only inline the most frequent cases.
  InstanceType instance_type = map.instance_type();
  if (base::IsInRange(instance_type, FIRST_FIXED_ARRAY_TYPE,
                      LAST_FIXED_ARRAY_TYPE)) {
    return FixedArray::SizeFor(
        FixedArray::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (base::IsInRange(instance_type, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)) {
    if (instance_type == NATIVE_CONTEXT_TYPE) return NativeContext::kSize;
    return Context::SizeFor(Context::unchecked_cast(*this).length());
  }
  if (instance_type == ONE_BYTE_STRING_TYPE ||
      instance_type == ONE_BYTE_INTERNALIZED_STRING_TYPE ||
      instance_type == SHARED_ONE_BYTE_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqOneByteString::SizeFor(
        SeqOneByteString::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == BYTE_ARRAY_TYPE) {
    return ByteArray::SizeFor(
        ByteArray::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == BYTECODE_ARRAY_TYPE) {
    return BytecodeArray::SizeFor(
        BytecodeArray::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == FREE_SPACE_TYPE) {
    return FreeSpace::unchecked_cast(*this).size(kRelaxedLoad);
  }
  if (instance_type == STRING_TYPE ||
      instance_type == INTERNALIZED_STRING_TYPE ||
      instance_type == SHARED_STRING_TYPE) {
    // Strings may get concurrently truncated, hence we have to access its
    // length synchronized.
    return SeqTwoByteString::SizeFor(
        SeqTwoByteString::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
    return FixedDoubleArray::SizeFor(
        FixedDoubleArray::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == FEEDBACK_METADATA_TYPE) {
    return FeedbackMetadata::SizeFor(
        FeedbackMetadata::unchecked_cast(*this).slot_count(kAcquireLoad));
  }
  if (base::IsInRange(instance_type, FIRST_DESCRIPTOR_ARRAY_TYPE,
                      LAST_DESCRIPTOR_ARRAY_TYPE)) {
    return DescriptorArray::SizeFor(
        DescriptorArray::unchecked_cast(*this).number_of_all_descriptors());
  }
  if (base::IsInRange(instance_type, FIRST_WEAK_FIXED_ARRAY_TYPE,
                      LAST_WEAK_FIXED_ARRAY_TYPE)) {
    return WeakFixedArray::SizeFor(
        WeakFixedArray::unchecked_cast(*this).length(kAcquireLoad));
  }
  if (instance_type == WEAK_ARRAY_LIST_TYPE) {
    return WeakArrayList::SizeForCapacity(
        WeakArrayList::unchecked_cast(*this).capacity());
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
  if (instance_type == SWISS_NAME_DICTIONARY_TYPE) {
    return SwissNameDictionary::SizeFor(
        SwissNameDictionary::unchecked_cast(*this).Capacity());
  }
  if (instance_type == PROPERTY_ARRAY_TYPE) {
    return PropertyArray::SizeFor(
        PropertyArray::cast(*this).length(kAcquireLoad));
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
#define MAKE_TORQUE_SIZE_FOR(TYPE, TypeName)                \
  if (instance_type == TYPE) {                              \
    return TypeName::unchecked_cast(*this).AllocatedSize(); \
  }
  TORQUE_INSTANCE_TYPE_TO_BODY_DESCRIPTOR_LIST(MAKE_TORQUE_SIZE_FOR)
#undef MAKE_TORQUE_SIZE_FOR

  if (instance_type == CODE_TYPE) {
    return Code::unchecked_cast(*this).CodeSize();
  }
  if (instance_type == COVERAGE_INFO_TYPE) {
    return CoverageInfo::SizeFor(
        CoverageInfo::unchecked_cast(*this).slot_count());
  }
#if V8_ENABLE_WEBASSEMBLY
  if (instance_type == WASM_TYPE_INFO_TYPE) {
    return WasmTypeInfo::SizeFor(
        WasmTypeInfo::unchecked_cast(*this).supertypes_length());
  }
  if (instance_type == WASM_STRUCT_TYPE) {
    return WasmStruct::GcSafeSize(map);
  }
  if (instance_type == WASM_ARRAY_TYPE) {
    return WasmArray::SizeFor(map, WasmArray::unchecked_cast(*this).length());
  }
#endif  // V8_ENABLE_WEBASSEMBLY
  DCHECK_EQ(instance_type, EMBEDDER_DATA_ARRAY_TYPE);
  return EmbedderDataArray::SizeFor(
      EmbedderDataArray::unchecked_cast(*this).length());
}

bool HeapObject::NeedsRehashing(PtrComprCageBase cage_base) const {
  return NeedsRehashing(map(cage_base).instance_type());
}

bool HeapObject::NeedsRehashing(InstanceType instance_type) const {
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    // Use map() only when it's guaranteed that it's not a Code object.
    DCHECK_IMPLIES(instance_type != CODE_TYPE,
                   instance_type == map().instance_type());
  } else {
    DCHECK_EQ(instance_type, map().instance_type());
  }
  switch (instance_type) {
    case DESCRIPTOR_ARRAY_TYPE:
    case STRONG_DESCRIPTOR_ARRAY_TYPE:
      return DescriptorArray::cast(*this).number_of_descriptors() > 1;
    case TRANSITION_ARRAY_TYPE:
      return TransitionArray::cast(*this).number_of_entries() > 1;
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
      return false;  // We'll rehash from the JSMap or JSSet referencing them.
    case NAME_DICTIONARY_TYPE:
    case NAME_TO_INDEX_HASH_TABLE_TYPE:
    case REGISTERED_SYMBOL_TABLE_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case HASH_TABLE_TYPE:
    case SMALL_ORDERED_HASH_MAP_TYPE:
    case SMALL_ORDERED_HASH_SET_TYPE:
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
    case SWISS_NAME_DICTIONARY_TYPE:
    case JS_MAP_TYPE:
    case JS_SET_TYPE:
      return true;
    default:
      return false;
  }
}

bool HeapObject::CanBeRehashed(PtrComprCageBase cage_base) const {
  DCHECK(NeedsRehashing(cage_base));
  switch (map(cage_base).instance_type()) {
    case JS_MAP_TYPE:
    case JS_SET_TYPE:
      return true;
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
      UNREACHABLE();  // We'll rehash from the JSMap or JSSet referencing them.
    case ORDERED_NAME_DICTIONARY_TYPE:
      return false;
    case NAME_DICTIONARY_TYPE:
    case NAME_TO_INDEX_HASH_TABLE_TYPE:
    case REGISTERED_SYMBOL_TABLE_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case SWISS_NAME_DICTIONARY_TYPE:
      return true;
    case DESCRIPTOR_ARRAY_TYPE:
    case STRONG_DESCRIPTOR_ARRAY_TYPE:
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
}

template <typename IsolateT>
void HeapObject::RehashBasedOnMap(IsolateT* isolate) {
  switch (map(isolate).instance_type()) {
    case HASH_TABLE_TYPE:
      UNREACHABLE();
    case NAME_DICTIONARY_TYPE:
      NameDictionary::cast(*this).Rehash(isolate);
      break;
    case NAME_TO_INDEX_HASH_TABLE_TYPE:
      NameToIndexHashTable::cast(*this).Rehash(isolate);
      break;
    case REGISTERED_SYMBOL_TABLE_TYPE:
      RegisteredSymbolTable::cast(*this).Rehash(isolate);
      break;
    case SWISS_NAME_DICTIONARY_TYPE:
      SwissNameDictionary::cast(*this).Rehash(isolate);
      break;
    case GLOBAL_DICTIONARY_TYPE:
      GlobalDictionary::cast(*this).Rehash(isolate);
      break;
    case NUMBER_DICTIONARY_TYPE:
      NumberDictionary::cast(*this).Rehash(isolate);
      break;
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
      SimpleNumberDictionary::cast(*this).Rehash(isolate);
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
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
      UNREACHABLE();  // We'll rehash from the JSMap or JSSet referencing them.
    case JS_MAP_TYPE: {
      JSMap::cast(*this).Rehash(isolate->AsIsolate());
      break;
    }
    case JS_SET_TYPE: {
      JSSet::cast(*this).Rehash(isolate->AsIsolate());
      break;
    }
    case SMALL_ORDERED_NAME_DICTIONARY_TYPE:
      DCHECK_EQ(0, SmallOrderedNameDictionary::cast(*this).NumberOfElements());
      break;
    case ONE_BYTE_INTERNALIZED_STRING_TYPE:
    case INTERNALIZED_STRING_TYPE:
      // Rare case, rehash read-only space strings before they are sealed.
      DCHECK(ReadOnlyHeap::Contains(*this));
      String::cast(*this).EnsureHash();
      break;
    default:
      UNREACHABLE();
  }
}
template void HeapObject::RehashBasedOnMap(Isolate* isolate);
template void HeapObject::RehashBasedOnMap(LocalIsolate* isolate);

void DescriptorArray::GeneralizeAllFields() {
  int length = number_of_descriptors();
  for (InternalIndex i : InternalIndex::Range(length)) {
    PropertyDetails details = GetDetails(i);
    details = details.CopyWithRepresentation(Representation::Tagged());
    if (details.location() == PropertyLocation::kField) {
      DCHECK_EQ(PropertyKind::kData, details.kind());
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

      case LookupIterator::WASM_OBJECT:
        RETURN_FAILURE(it->isolate(), kThrowOnError,
                       NewTypeError(MessageTemplate::kWasmObjectsAreOpaque));

      case LookupIterator::INTERCEPTOR: {
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          Maybe<bool> result =
              JSObject::SetPropertyWithInterceptor(it, should_throw, value);
          if (result.IsNothing() || result.FromJust()) return result;
          // Assuming that the callback have side effects, we use
          // Object::SetSuperProperty() which works properly regardless on
          // whether the property was present on the receiver or not when
          // storing to the receiver.
          // Proceed lookup from the next state.
          it->Next();
        } else {
          Maybe<PropertyAttributes> maybe_attributes =
              JSObject::GetPropertyAttributesWithInterceptor(it);
          if (maybe_attributes.IsNothing()) return Nothing<bool>();
          if ((maybe_attributes.FromJust() & READ_ONLY) != 0) {
            return WriteToReadOnlyProperty(it, value, should_throw);
          }
          // At this point we might have called interceptor's query or getter
          // callback. Assuming that the callbacks have side effects, we use
          // Object::SetSuperProperty() which works properly regardless on
          // whether the property was present on the receiver or not when
          // storing to the receiver.
          if (maybe_attributes.FromJust() == ABSENT) {
            // Proceed lookup from the next state.
            it->Next();
          } else {
            // Finish lookup in order to make Object::SetSuperProperty() store
            // property to the receiver.
            it->NotFound();
          }
        }
        return Object::SetSuperProperty(it, value, store_origin, should_throw);
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

bool Object::CheckContextualStoreToJSGlobalObject(
    LookupIterator* it, Maybe<ShouldThrow> should_throw) {
  Isolate* isolate = it->isolate();

  if (it->GetReceiver()->IsJSGlobalObject(isolate) &&
      (GetShouldThrow(isolate, should_throw) == ShouldThrow::kThrowOnError)) {
    if (it->state() == LookupIterator::TRANSITION) {
      // The property cell that we have created is garbage because we are going
      // to throw now instead of putting it into the global dictionary. However,
      // the cell might already have been stored into the feedback vector, so
      // we must invalidate it nevertheless.
      it->transition_cell()->ClearAndInvalidate(ReadOnlyRoots(isolate));
    }
    isolate->Throw(*isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, it->GetName()));
    return false;
  }
  return true;
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

  if (!CheckContextualStoreToJSGlobalObject(it, should_throw)) {
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

  // Note, the callers rely on the fact that this code is redoing the full own
  // lookup from scratch.
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
          if (!CheckContextualStoreToJSGlobalObject(&own_lookup,
                                                    should_throw)) {
            return Nothing<bool>();
          }
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
      case LookupIterator::WASM_OBJECT:
        UNREACHABLE();
    }
  }

  if (!CheckContextualStoreToJSGlobalObject(&own_lookup, should_throw)) {
    return Nothing<bool>();
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
  Isolate* isolate = it->isolate();
  DCHECK_IMPLIES(it->GetReceiver()->IsJSProxy(isolate),
                 it->GetName()->IsPrivateName(isolate));
  DCHECK_IMPLIES(!it->IsElement() && it->GetName()->IsPrivateName(isolate),
                 it->state() == LookupIterator::DATA);
  Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(it->GetReceiver());

  // Store on the holder which may be hidden behind the receiver.
  DCHECK(it->HolderIsReceiverOrHiddenPrototype());

  Handle<Object> to_assign = value;
  // Convert the incoming value to a number for storing into typed arrays.
  if (it->IsElement() && receiver->IsJSObject(isolate) &&
      JSObject::cast(*receiver).HasTypedArrayOrRabGsabTypedArrayElements(
          isolate)) {
    ElementsKind elements_kind = JSObject::cast(*receiver).GetElementsKind();
    if (IsBigIntTypedArrayElementsKind(elements_kind)) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, to_assign,
                                       BigInt::FromObject(isolate, value),
                                       Nothing<bool>());
      if (Handle<JSTypedArray>::cast(receiver)->IsDetachedOrOutOfBounds()) {
        return Just(true);
      }
    } else if (!value->IsNumber() && !value->IsUndefined(isolate)) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, to_assign,
                                       Object::ToNumber(isolate, value),
                                       Nothing<bool>());
      if (Handle<JSTypedArray>::cast(receiver)->IsDetachedOrOutOfBounds()) {
        return Just(true);
      }
    }
  }

  DCHECK(!receiver->IsWasmObject(isolate));
  if (V8_UNLIKELY(receiver->IsJSSharedStruct(isolate) ||
                  receiver->IsJSSharedArray(isolate))) {
    // Shared structs can only point to primitives or shared values.
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, to_assign, Object::Share(isolate, to_assign, kThrowOnError),
        Nothing<bool>());
    it->WriteDataValue(to_assign, false);
  } else {
    // Possibly migrate to the most up-to-date map that will be able to store
    // |value| under it->name().
    it->PrepareForDataProperty(to_assign);

    // Write the property value.
    it->WriteDataValue(to_assign, false);
  }

#if VERIFY_HEAP
  if (v8_flags.verify_heap) {
    receiver->HeapObjectVerify(isolate);
  }
#endif
  return Just(true);
}

Maybe<bool> Object::AddDataProperty(LookupIterator* it, Handle<Object> value,
                                    PropertyAttributes attributes,
                                    Maybe<ShouldThrow> should_throw,
                                    StoreOrigin store_origin,
                                    EnforceDefineSemantics semantics) {
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
    RETURN_FAILURE(isolate, GetShouldThrow(it->isolate(), should_throw),
                   NewTypeError(semantics == EnforceDefineSemantics::kDefine
                                    ? MessageTemplate::kDefineDisallowed
                                    : MessageTemplate::kObjectNotExtensible,
                                it->GetName()));
  }

  if (it->IsElement(*receiver)) {
    if (receiver->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(receiver);
      if (JSArray::WouldChangeReadOnlyLength(array, it->array_index())) {
        RETURN_FAILURE(isolate, GetShouldThrow(it->isolate(), should_throw),
                       NewTypeError(MessageTemplate::kStrictReadOnlyProperty,
                                    isolate->factory()->length_string(),
                                    Object::TypeOf(isolate, array), array));
      }
    }

    Handle<JSObject> receiver_obj = Handle<JSObject>::cast(receiver);
    MAYBE_RETURN(JSObject::AddDataElement(receiver_obj, it->array_index(),
                                          value, attributes),
                 Nothing<bool>());
    JSObject::ValidateElements(*receiver_obj);
    return Just(true);
  }

  return Object::TransitionAndWriteDataProperty(it, value, attributes,
                                                should_throw, store_origin);
}

// static
Maybe<bool> Object::TransitionAndWriteDataProperty(
    LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
    Maybe<ShouldThrow> should_throw, StoreOrigin store_origin) {
  Handle<JSReceiver> receiver = it->GetStoreTarget<JSReceiver>();
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
  if (v8_flags.verify_heap) {
    receiver->HeapObjectVerify(it->isolate());
  }
#endif

    return Just(true);
}
// static
MaybeHandle<Object> Object::ShareSlow(Isolate* isolate,
                                      Handle<HeapObject> value,
                                      ShouldThrow throw_if_cannot_be_shared) {
  // Use Object::Share() if value might already be shared.
  DCHECK(!value->IsShared());

  SharedObjectSafePublishGuard publish_guard;

  if (value->IsString()) {
    return String::Share(isolate, Handle<String>::cast(value));
  }

  if (value->IsHeapNumber()) {
    uint64_t bits = HeapNumber::cast(*value).value_as_bits(kRelaxedLoad);
    return isolate->factory()
        ->NewHeapNumberFromBits<AllocationType::kSharedOld>(bits);
  }

  if (throw_if_cannot_be_shared == kThrowOnError) {
    THROW_NEW_ERROR(
        isolate, NewTypeError(MessageTemplate::kCannotBeShared, value), Object);
  }
  return MaybeHandle<Object>();
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
    DisallowGarbageCollection no_gc;
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
    proxy = Handle<JSProxy>::cast(object);
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
    return JSReceiver::HasProperty(isolate, target, name);
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
    PropertyKey key(isolate, name);
    LookupIterator it(isolate, receiver, key, target);

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
  if (!handler->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate, NewTypeError(MessageTemplate::kProxyNonObject),
                    JSProxy);
  }
  return isolate->factory()->NewJSProxy(Handle<JSReceiver>::cast(target),
                                        Handle<JSReceiver>::cast(handler));
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
      // Also handle the {configurable: true} and enumerable changes
      // since we later use JSArray::SetLength instead of
      // OrdinaryDefineOwnProperty to change the length,
      // and it doesn't have access to the descriptor anymore.
      new_len_desc->configurable() ||
      (new_len_desc->has_enumerable() &&
       (old_len_desc.enumerable() != new_len_desc->enumerable()))) {
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
  MAYBE_RETURN(JSArray::SetLength(a, new_len), Nothing<bool>());
  // Steps 19d-ii, 20.
  if (!new_writable) {
    PropertyDescriptor readonly;
    readonly.set_writable(false);
    success = OrdinaryDefineOwnProperty(isolate, a,
                                        isolate->factory()->length_string(),
                                        &readonly, should_throw);
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

  LookupIterator it(isolate, proxy, private_name, proxy);

  if (it.IsFound()) {
    DCHECK_EQ(LookupIterator::DATA, it.state());
    DCHECK_EQ(DONT_ENUM, it.property_attributes());
    // We are not tracking constness for private symbols added to JSProxy
    // objects.
    DCHECK_EQ(PropertyConstness::kMutable, it.property_details().constness());
    it.WriteDataValue(value, false);
    return Just(true);
  }

  PropertyDetails details(PropertyKind::kData, DONT_ENUM,
                          PropertyConstness::kMutable);
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    Handle<SwissNameDictionary> dict(proxy->property_dictionary_swiss(),
                                     isolate);
    Handle<SwissNameDictionary> result =
        SwissNameDictionary::Add(isolate, dict, private_name, value, details);
    if (!dict.is_identical_to(result)) proxy->SetProperties(*result);
  } else {
    Handle<NameDictionary> dict(proxy->property_dictionary(), isolate);
    Handle<NameDictionary> result =
        NameDictionary::Add(isolate, dict, private_name, value, details);
    if (!dict.is_identical_to(result)) proxy->SetProperties(*result);
  }
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
    Isolate* isolate, Handle<DescriptorArray> source_handle,
    int enumeration_index, PropertyAttributes attributes, int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;
  Handle<DescriptorArray> copy_handle =
      DescriptorArray::Allocate(isolate, size, slack);

  DisallowGarbageCollection no_gc;
  auto source = *source_handle;
  auto copy = *copy_handle;

  if (attributes != NONE) {
    for (InternalIndex i : InternalIndex::Range(size)) {
      MaybeObject value_or_field_type = source.GetValue(i);
      Name key = source.GetKey(i);
      PropertyDetails details = source.GetDetails(i);
      // Bulk attribute changes never affect private properties.
      if (!key.IsPrivate()) {
        int mask = DONT_DELETE | DONT_ENUM;
        // READ_ONLY is an invalid attribute for JS setters/getters.
        HeapObject heap_object;
        if (details.kind() != PropertyKind::kAccessor ||
            !(value_or_field_type->GetHeapObjectIfStrong(&heap_object) &&
              heap_object.IsAccessorPair())) {
          mask |= READ_ONLY;
        }
        details = details.CopyAddAttributes(
            static_cast<PropertyAttributes>(attributes & mask));
      }
      copy.Set(i, key, value_or_field_type, details);
    }
  } else {
    for (InternalIndex i : InternalIndex::Range(size)) {
      copy.CopyFrom(i, source);
    }
  }

  if (source.number_of_descriptors() != enumeration_index) copy.Sort();

  return copy_handle;
}

// Create a new descriptor array with only enumerable, configurable, writeable
// data properties, but identical field locations.
Handle<DescriptorArray> DescriptorArray::CopyForFastObjectClone(
    Isolate* isolate, Handle<DescriptorArray> src_handle, int enumeration_index,
    int slack) {
  if (enumeration_index + slack == 0) {
    return isolate->factory()->empty_descriptor_array();
  }

  int size = enumeration_index;
  Handle<DescriptorArray> descriptors_handle =
      DescriptorArray::Allocate(isolate, size, slack);

  DisallowGarbageCollection no_gc;
  auto src = *src_handle;
  auto descriptors = *descriptors_handle;

  for (InternalIndex i : InternalIndex::Range(size)) {
    Name key = src.GetKey(i);
    PropertyDetails details = src.GetDetails(i);
    Representation new_representation = details.representation();

    DCHECK(!key.IsPrivateName());
    DCHECK(details.IsEnumerable());
    DCHECK_EQ(details.kind(), PropertyKind::kData);
    // If the new representation is an in-place changeable field, make it
    // generic as possible (under in-place changes) to avoid type confusion if
    // the source representation changes after this feedback has been collected.
    MaybeObject type = src.GetValue(i);
    if (details.location() == PropertyLocation::kField) {
      type = MaybeObject::FromObject(FieldType::Any());
      // TODO(bmeurer,ishell): Igor suggested to use some kind of dynamic
      // checks in the fast-path for CloneObjectIC instead to avoid the
      // need to generalize the descriptors here. That will also enable
      // us to skip the defensive copying of the target map whenever a
      // CloneObjectIC misses.
      new_representation = new_representation.MostGenericInPlaceChange();
    }

    // Ensure the ObjectClone property details are NONE, and that all source
    // details did not contain DONT_ENUM.
    PropertyDetails new_details(PropertyKind::kData, NONE, details.location(),
                                details.constness(), new_representation,
                                details.field_index());

    descriptors.Set(i, key, type, new_details);
  }

  descriptors.Sort();

  return descriptors_handle;
}

bool DescriptorArray::IsEqualUpTo(DescriptorArray desc, int nof_descriptors) {
  for (InternalIndex i : InternalIndex::Range(nof_descriptors)) {
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
                                          Handle<Object> value) {
  int src_length = array->length();
  if (index < src_length) {
    array->set(index, *value);
    return array;
  }
  int capacity = src_length;
  do {
    capacity = JSObject::NewElementsCapacity(capacity);
  } while (capacity <= index);
  Handle<FixedArray> new_array = isolate->factory()->NewFixedArray(capacity);

  DisallowGarbageCollection no_gc;
  auto raw_src = *array;
  auto raw_dst = *new_array;
  raw_src.CopyTo(0, raw_dst, 0, src_length);
  DCHECK_EQ(raw_dst.length(), capacity);
  raw_dst.FillWithHoles(src_length, capacity);
  raw_dst.set(index, *value);

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
  DisallowGarbageCollection no_gc;
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
                                 Handle<Object> obj,
                                 AllocationType allocation) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 1, allocation);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  {
    DisallowGarbageCollection no_gc;
    ArrayList raw_array = *array;
    raw_array.Set(length, *obj);
    raw_array.SetLength(length + 1);
  }
  return array;
}

// static
Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj1, Handle<Object> obj2) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 2);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  {
    DisallowGarbageCollection no_gc;
    ArrayList raw_array = *array;
    raw_array.Set(length, *obj1);
    raw_array.Set(length + 1, *obj2);
    raw_array.SetLength(length + 2);
  }
  return array;
}

Handle<ArrayList> ArrayList::Add(Isolate* isolate, Handle<ArrayList> array,
                                 Handle<Object> obj1, Smi obj2, Smi obj3,
                                 Smi obj4) {
  int length = array->Length();
  array = EnsureSpace(isolate, array, length + 4);
  // Check that GC didn't remove elements from the array.
  DCHECK_EQ(array->Length(), length);
  {
    DisallowGarbageCollection no_gc;
    ArrayList raw_array = *array;
    raw_array.Set(length, *obj1);
    raw_array.Set(length + 1, obj2);
    raw_array.Set(length + 2, obj3);
    raw_array.Set(length + 3, obj4);
    raw_array.SetLength(length + 4);
  }
  return array;
}

// static
Handle<ArrayList> ArrayList::New(Isolate* isolate, int size) {
  return isolate->factory()->NewArrayList(size);
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
                                           Handle<FixedArray> array, int length,
                                           AllocationType allocation) {
  int capacity = array->length();
  if (capacity < length) {
    int new_capacity = length;
    new_capacity = new_capacity + std::max(new_capacity / 2, 2);
    int grow_by = new_capacity - capacity;
    array =
        isolate->factory()->CopyFixedArrayAndGrow(array, grow_by, allocation);
  }
  return array;
}

}  // namespace

// static
Handle<ArrayList> ArrayList::EnsureSpace(Isolate* isolate,
                                         Handle<ArrayList> array, int length,
                                         AllocationType allocation) {
  DCHECK_LT(0, length);
  auto new_array = Handle<ArrayList>::cast(EnsureSpaceInFixedArray(
      isolate, array, kFirstIndex + length, allocation));
  DCHECK_EQ(array->Length(), new_array->Length());
  return new_array;
}

// static
Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              const MaybeObjectHandle& value) {
  int length = array->length();
  array = EnsureSpace(isolate, array, length + 1);
  {
    DisallowGarbageCollection no_gc;
    WeakArrayList raw = *array;
    // Reload length; GC might have removed elements from the array.
    length = raw.length();
    raw.Set(length, *value);
    raw.set_length(length + 1);
  }
  return array;
}

Handle<WeakArrayList> WeakArrayList::AddToEnd(Isolate* isolate,
                                              Handle<WeakArrayList> array,
                                              const MaybeObjectHandle& value1,
                                              const MaybeObjectHandle& value2) {
  int length = array->length();
  array = EnsureSpace(isolate, array, length + 2);
  {
    DisallowGarbageCollection no_gc;
    WeakArrayList raw = *array;
    // Reload length; GC might have removed elements from the array.
    length = array->length();
    raw.Set(length, *value1);
    raw.Set(length + 1, *value2);
    raw.set_length(length + 2);
  }
  return array;
}

// static
Handle<WeakArrayList> WeakArrayList::Append(Isolate* isolate,
                                            Handle<WeakArrayList> array,
                                            const MaybeObjectHandle& value,
                                            AllocationType allocation) {
  int length = 0;
  int new_length = 0;
  {
    DisallowGarbageCollection no_gc;
    WeakArrayList raw = *array;
    length = raw.length();

    if (length < raw.capacity()) {
      raw.Set(length, *value);
      raw.set_length(length + 1);
      return array;
    }

    // Not enough space in the array left, either grow, shrink or
    // compact the array.
    new_length = raw.CountLiveElements() + 1;
  }

  bool shrink = new_length < length / 4;
  bool grow = 3 * (length / 4) < new_length;

  if (shrink || grow) {
    // Grow or shrink array and compact out-of-place.
    int new_capacity = CapacityForLength(new_length);
    array = isolate->factory()->CompactWeakArrayList(array, new_capacity,
                                                     allocation);

  } else {
    // Perform compaction in the current array.
    array->Compact(isolate);
  }

  // Now append value to the array, there should always be enough space now.
  DCHECK_LT(array->length(), array->capacity());

  {
    DisallowGarbageCollection no_gc;
    WeakArrayList raw = *array;
    // Reload length, allocation might have killed some weak refs.
    int index = raw.length();
    raw.Set(index, *value);
    raw.set_length(index + 1);
  }
  return array;
}

void WeakArrayList::Compact(Isolate* isolate) {
  DisallowGarbageCollection no_gc;
  int length = this->length();
  int new_length = 0;

  for (int i = 0; i < length; i++) {
    MaybeObject value = Get(isolate, i);

    if (!value->IsCleared()) {
      if (new_length != i) {
        Set(new_length, value);
      }
      ++new_length;
    }
  }

  set_length(new_length);
}

bool WeakArrayList::IsFull() const { return length() == capacity(); }

// static
Handle<WeakArrayList> WeakArrayList::EnsureSpace(Isolate* isolate,
                                                 Handle<WeakArrayList> array,
                                                 int length,
                                                 AllocationType allocation) {
  int capacity = array->capacity();
  if (capacity < length) {
    int grow_by = CapacityForLength(length) - capacity;
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

int WeakArrayList::CountLiveElements() const {
  int non_cleared_objects = 0;
  for (int i = 0; i < length(); i++) {
    if (!Get(i)->IsCleared()) {
      ++non_cleared_objects;
    }
  }
  return non_cleared_objects;
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

bool WeakArrayList::Contains(MaybeObject value) {
  for (int i = 0; i < length(); ++i) {
    if (Get(i) == value) return true;
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

  if (empty_slot == kNoEmptySlotsMarker) {
    // GCs might have cleared some references, rescan the array for empty slots.
    PrototypeUsers::ScanForEmptySlots(*array);
    empty_slot = Smi::ToInt(empty_slot_index(*array));
  }

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

// static
void PrototypeUsers::ScanForEmptySlots(WeakArrayList array) {
  for (int i = kFirstIndex; i < array.length(); i++) {
    if (array.Get(i)->IsCleared()) {
      PrototypeUsers::MarkSlotEmpty(array, i);
    }
  }
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

Handle<RegExpMatchInfo> RegExpMatchInfo::New(Isolate* isolate,
                                             int capture_count) {
  Handle<RegExpMatchInfo> match_info = isolate->factory()->NewRegExpMatchInfo();
  return ReserveCaptures(isolate, match_info, capture_count);
}

Handle<RegExpMatchInfo> RegExpMatchInfo::ReserveCaptures(
    Isolate* isolate, Handle<RegExpMatchInfo> match_info, int capture_count) {
  DCHECK_GE(match_info->length(), kLastMatchOverhead);

  int capture_register_count =
      JSRegExp::RegistersForCaptureCount(capture_count);
  const int required_length = kFirstCaptureIndex + capture_register_count;
  Handle<RegExpMatchInfo> result =
      Handle<RegExpMatchInfo>::cast(EnsureSpaceInFixedArray(
          isolate, match_info, required_length, AllocationType::kYoung));
  result->SetNumberOfCaptureRegisters(capture_register_count);
  return result;
}

template <typename IsolateT>
Handle<DescriptorArray> DescriptorArray::Allocate(IsolateT* isolate,
                                                  int nof_descriptors,
                                                  int slack,
                                                  AllocationType allocation) {
  return nof_descriptors + slack == 0
             ? isolate->factory()->empty_descriptor_array()
             : isolate->factory()->NewDescriptorArray(nof_descriptors, slack,
                                                      allocation);
}
template Handle<DescriptorArray> DescriptorArray::Allocate(
    Isolate* isolate, int nof_descriptors, int slack,
    AllocationType allocation);
template Handle<DescriptorArray> DescriptorArray::Allocate(
    LocalIsolate* isolate, int nof_descriptors, int slack,
    AllocationType allocation);

void DescriptorArray::Initialize(EnumCache empty_enum_cache,
                                 HeapObject undefined_value,
                                 int nof_descriptors, int slack) {
  DCHECK_GE(nof_descriptors, 0);
  DCHECK_GE(slack, 0);
  DCHECK_LE(nof_descriptors + slack, kMaxNumberOfDescriptors);
  set_number_of_all_descriptors(nof_descriptors + slack);
  set_number_of_descriptors(nof_descriptors);
  set_raw_number_of_marked_descriptors(0);
  set_filler16bits(0);
  set_enum_cache(empty_enum_cache, SKIP_WRITE_BARRIER);
  MemsetTagged(GetDescriptorSlot(0), undefined_value,
               number_of_all_descriptors() * kEntrySize);
}

void DescriptorArray::ClearEnumCache() {
  set_enum_cache(GetReadOnlyRoots().empty_enum_cache(), SKIP_WRITE_BARRIER);
}

void DescriptorArray::Replace(InternalIndex index, Descriptor* descriptor) {
  descriptor->SetSortedKeyIndex(GetSortedKeyIndex(index.as_int()));
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

void DescriptorArray::CopyFrom(InternalIndex index, DescriptorArray src) {
  PropertyDetails details = src.GetDetails(index);
  Set(index, src.GetKey(index), src.GetValue(index), details);
}

void DescriptorArray::Sort() {
  // In-place heap sort.
  const int len = number_of_descriptors();
  // Reset sorting since the descriptor array might contain invalid pointers.
  for (int i = 0; i < len; ++i) SetSortedKey(i, i);
  // Bottom-up max-heap construction.
  // Index of the last node with children.
  int max_parent_index = (len / 2) - 1;
  for (int i = max_parent_index; i >= 0; --i) {
    int parent_index = i;
    const uint32_t parent_hash = GetSortedKey(i).hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetSortedKey(child_index).hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1).hash();
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
    const uint32_t parent_hash = GetSortedKey(parent_index).hash();
    max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetSortedKey(child_index).hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetSortedKey(child_index + 1).hash();
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

void DescriptorArray::CheckNameCollisionDuringInsertion(Descriptor* desc,
                                                        uint32_t desc_hash,
                                                        int insertion_index) {
  DCHECK_GE(insertion_index, 0);
  DCHECK_LE(insertion_index, number_of_all_descriptors());

  if (insertion_index <= 0) return;

  for (int i = insertion_index; i > 0; --i) {
    Name current_key = GetSortedKey(i - 1);
    if (current_key.hash() != desc_hash) return;
    CHECK(current_key != *desc->GetKey());
  }
}

int16_t DescriptorArray::UpdateNumberOfMarkedDescriptors(
    unsigned mark_compact_epoch, int16_t new_marked) {
  static_assert(kMaxNumberOfDescriptors <=
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
  DisallowGarbageCollection no_gc;
  auto raw_src = *pair;
  auto raw_copy = *copy;
  raw_copy.set_getter(raw_src.getter());
  raw_copy.set_setter(raw_src.setter());
  return copy;
}

Handle<Object> AccessorPair::GetComponent(Isolate* isolate,
                                          Handle<NativeContext> native_context,
                                          Handle<AccessorPair> accessor_pair,
                                          AccessorComponent component) {
  Handle<Object> accessor(accessor_pair->get(component), isolate);
  if (accessor->IsFunctionTemplateInfo()) {
    auto function = ApiNatives::InstantiateFunction(
                        isolate, native_context,
                        Handle<FunctionTemplateInfo>::cast(accessor))
                        .ToHandleChecked();
    accessor_pair->set(component, *function, kReleaseStore);
    return function;
  }
  if (accessor->IsNull(isolate)) {
    return isolate->factory()->undefined_value();
  }
  return accessor;
}

#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray other) {
  if (number_of_all_descriptors() != other.number_of_all_descriptors()) {
    return false;
  }
  for (InternalIndex i : InternalIndex::Range(number_of_descriptors())) {
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
  Handle<Object> description(Handle<Symbol>::cast(name)->description(),
                             isolate);
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
int Relocatable::ArchiveSpacePerThread() { return sizeof(Relocatable*); }

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
  DisallowGarbageCollection no_gc;
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
    separator_one_char = SeqOneByteString::cast(separator).GetChars(no_gc)[0];
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
  DisallowGarbageCollection no_gc;
  DisallowJavascriptExecution no_js(isolate);
  FixedArray fixed_array = FixedArray::cast(Object(raw_fixed_array));
  String separator = String::cast(Object(raw_separator));
  String dest = String::cast(Object(raw_dest));
  DCHECK(fixed_array.IsFixedArray());
  DCHECK(StringShape(dest).IsSequentialOneByte() ||
         StringShape(dest).IsSequentialTwoByte());

  if (StringShape(dest).IsSequentialOneByte()) {
    WriteFixedArrayToFlat(fixed_array, static_cast<int>(length), separator,
                          SeqOneByteString::cast(dest).GetChars(no_gc),
                          dest.length());
  } else {
    DCHECK(StringShape(dest).IsSequentialTwoByte());
    WriteFixedArrayToFlat(fixed_array, static_cast<int>(length), separator,
                          SeqTwoByteString::cast(dest).GetChars(no_gc),
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

  DCHECK(String::IsIntegerIndex(value));
  DCHECK_EQ(length <= String::kMaxCachedArrayIndexLength,
            Name::ContainsCachedArrayIndex(value));
  return value;
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
        Handle<HeapNumber>::cast(to_number)->value_as_bits(kRelaxedLoad));
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
      position =
          shared->abstract_code(isolate).SourcePosition(isolate, -position);
    }
    DCHECK_GE(position, 0);
    script->set_eval_from_position(position);
  }
  return position;
}

template <typename IsolateT>
// static
void Script::InitLineEnds(IsolateT* isolate, Handle<Script> script) {
  if (!script->line_ends().IsUndefined(isolate)) return;
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(script->type() != Script::TYPE_WASM ||
         script->source_mapping_url().IsString());
#endif  // V8_ENABLE_WEBASSEMBLY

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

template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void Script::InitLineEnds(
    Isolate* isolate, Handle<Script> script);
template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) void Script::InitLineEnds(
    LocalIsolate* isolate, Handle<Script> script);

bool Script::GetPositionInfo(Handle<Script> script, int position,
                             PositionInfo* info, OffsetFlag offset_flag) {
  bool init_line_ends = true;
#if V8_ENABLE_WEBASSEMBLY
  // For wasm, we do not create an artificial line_ends array, but do the
  // translation directly.
  init_line_ends = script->type() != Script::TYPE_WASM;
#endif  // V8_ENABLE_WEBASSEMBLY
  if (init_line_ends) InitLineEnds(script->GetIsolate(), script);
  return script->GetPositionInfo(position, info, offset_flag);
}

bool Script::IsSubjectToDebugging() const {
  switch (type()) {
    case TYPE_NORMAL:
#if V8_ENABLE_WEBASSEMBLY
    case TYPE_WASM:
#endif  // V8_ENABLE_WEBASSEMBLY
      return true;
  }
  return false;
}

bool Script::IsUserJavaScript() const { return type() == Script::TYPE_NORMAL; }

#if V8_ENABLE_WEBASSEMBLY
bool Script::ContainsAsmModule() {
  DisallowGarbageCollection no_gc;
  SharedFunctionInfo::ScriptIterator iter(this->GetIsolate(), *this);
  for (SharedFunctionInfo info = iter.Next(); !info.is_null();
       info = iter.Next()) {
    if (info.HasAsmWasmData()) return true;
  }
  return false;
}
#endif  // V8_ENABLE_WEBASSEMBLY

namespace {

template <typename Char>
bool GetPositionInfoSlowImpl(const base::Vector<Char>& source, int position,
                             Script::PositionInfo* info) {
  if (position < 0) {
    position = 0;
  }
  int line = 0;
  const auto begin = std::cbegin(source);
  const auto end = std::cend(source);
  for (auto line_begin = begin; line_begin < end;) {
    const auto line_end = std::find(line_begin, end, '\n');
    if (position <= (line_end - begin)) {
      info->line = line;
      info->column = static_cast<int>((begin + position) - line_begin);
      info->line_start = static_cast<int>(line_begin - begin);
      info->line_end = static_cast<int>(line_end - begin);
      return true;
    }
    ++line;
    line_begin = line_end + 1;
  }
  return false;
}
bool GetPositionInfoSlow(const Script script, int position,
                         const DisallowGarbageCollection& no_gc,
                         Script::PositionInfo* info) {
  if (!script.source().IsString()) {
    return false;
  }
  auto source = String::cast(script.source());
  const auto flat = source.GetFlatContent(no_gc);
  return flat.IsOneByte()
             ? GetPositionInfoSlowImpl(flat.ToOneByteVector(), position, info)
             : GetPositionInfoSlowImpl(flat.ToUC16Vector(), position, info);
}

}  // namespace

bool Script::GetPositionInfo(int position, PositionInfo* info,
                             OffsetFlag offset_flag) const {
  DisallowGarbageCollection no_gc;

#if V8_ENABLE_WEBASSEMBLY
  // For wasm, we use the byte offset as the column.
  if (type() == Script::TYPE_WASM) {
    DCHECK_LE(0, position);
    wasm::NativeModule* native_module = wasm_native_module();
    const wasm::WasmModule* module = native_module->module();
    if (module->functions.size() == 0) return false;
    info->line = 0;
    info->column = position;
    info->line_start = module->functions[0].code.offset();
    info->line_end = module->functions.back().code.end_offset();
    return true;
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  if (line_ends().IsUndefined()) {
    // Slow mode: we do not have line_ends. We have to iterate through source.
    if (!GetPositionInfoSlow(*this, position, no_gc, info)) {
      return false;
    }
  } else {
    DCHECK(line_ends().IsFixedArray());
    FixedArray ends = FixedArray::cast(line_ends());

    const int ends_len = ends.length();
    if (ends_len == 0) return false;

    // Return early on invalid positions. Negative positions behave as if 0 was
    // passed, and positions beyond the end of the script return as failure.
    if (position < 0) {
      position = 0;
    } else if (position > Smi::ToInt(ends.get(ends_len - 1))) {
      return false;
    }

    // Determine line number by doing a binary search on the line ends array.
    if (Smi::ToInt(ends.get(0)) >= position) {
      info->line = 0;
      info->line_start = 0;
      info->column = position;
    } else {
      int left = 0;
      int right = ends_len - 1;

      while (right > 0) {
        DCHECK_LE(left, right);
        const int mid = (left + right) / 2;
        if (position > Smi::ToInt(ends.get(mid))) {
          left = mid + 1;
        } else if (position <= Smi::ToInt(ends.get(mid - 1))) {
          right = mid - 1;
        } else {
          info->line = mid;
          break;
        }
      }
      DCHECK(Smi::ToInt(ends.get(info->line)) >= position &&
             Smi::ToInt(ends.get(info->line - 1)) < position);
      info->line_start = Smi::ToInt(ends.get(info->line - 1)) + 1;
      info->column = position - info->line_start;
    }

    // Line end is position of the linebreak character.
    info->line_end = Smi::ToInt(ends.get(info->line));
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

// static
Handle<String> Script::GetScriptHash(Isolate* isolate, Handle<Script> script,
                                     bool forceForInspector) {
  if (script->origin_options().IsOpaque() && !forceForInspector) {
    return isolate->factory()->empty_string();
  }

  PtrComprCageBase cage_base(isolate);
  {
    Object maybe_source_hash = script->source_hash(cage_base);
    if (maybe_source_hash.IsString(cage_base)) {
      Handle<String> precomputed(String::cast(maybe_source_hash), isolate);
      if (precomputed->length() > 0) {
        return precomputed;
      }
    }
  }

  Handle<String> src_text;
  {
    Object maybe_script_source = script->source(cage_base);

    if (!maybe_script_source.IsString(cage_base)) {
      return isolate->factory()->empty_string();
    }
    src_text = handle(String::cast(maybe_script_source), isolate);
  }

  char formatted_hash[kSizeOfFormattedSha256Digest];

  std::unique_ptr<char[]> string_val = src_text->ToCString();
  size_t len = strlen(string_val.get());
  uint8_t hash[kSizeOfSha256Digest];
  SHA256_hash(string_val.get(), len, hash);
  FormatBytesToHex(formatted_hash, kSizeOfFormattedSha256Digest, hash,
                   kSizeOfSha256Digest);
  formatted_hash[kSizeOfSha256Digest * 2] = '\0';

  Handle<String> result =
      isolate->factory()->NewStringFromAsciiChecked(formatted_hash);
  script->set_source_hash(*result);
  return result;
}

template <typename IsolateT>
MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    Handle<Script> script, IsolateT* isolate,
    FunctionLiteral* function_literal) {
  int function_literal_id = function_literal->function_literal_id();
  if (V8_UNLIKELY(script->type() == Script::TYPE_WEB_SNAPSHOT &&
                  function_literal_id >=
                      script->shared_function_info_count())) {
    return FindWebSnapshotSharedFunctionInfo(script, isolate, function_literal);
  }

  CHECK_NE(function_literal_id, kFunctionLiteralIdInvalid);
  // If this check fails, the problem is most probably the function id
  // renumbering done by AstFunctionLiteralIdReindexer; in particular, that
  // AstTraversalVisitor doesn't recurse properly in the construct which
  // triggers the mismatch.
  CHECK_LT(function_literal_id, script->shared_function_info_count());
  MaybeObject shared = script->shared_function_infos().Get(function_literal_id);
  HeapObject heap_object;
  if (!shared->GetHeapObject(&heap_object) ||
      heap_object.IsUndefined(isolate)) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  return handle(SharedFunctionInfo::cast(heap_object), isolate);
}
template MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    Handle<Script> script, Isolate* isolate, FunctionLiteral* function_literal);
template MaybeHandle<SharedFunctionInfo> Script::FindSharedFunctionInfo(
    Handle<Script> script, LocalIsolate* isolate,
    FunctionLiteral* function_literal);

MaybeHandle<SharedFunctionInfo> Script::FindWebSnapshotSharedFunctionInfo(
    Handle<Script> script, Isolate* isolate,
    FunctionLiteral* function_literal) {
  // We might be able to de-dupe the SFI against a SFI that was
  // created when deserializing the snapshot (or when calling a function which
  // was included in the snapshot). In that case, we can find it based on the
  // start position in shared_function_info_table.
  Handle<ObjectHashTable> shared_function_info_table = handle(
      ObjectHashTable::cast(script->shared_function_info_table()), isolate);
  {
    DisallowHeapAllocation no_gc;
    Object index_object = shared_function_info_table->Lookup(
        handle(Smi::FromInt(function_literal->start_position()), isolate));
    if (!index_object.IsTheHole()) {
      int index = Smi::cast(index_object).value();
      DCHECK_LT(index, script->shared_function_info_count());
      MaybeObject maybe_shared = script->shared_function_infos().Get(index);
      HeapObject heap_object;
      if (!maybe_shared->GetHeapObject(&heap_object)) {
        // We found the correct location but it's not filled in (e.g., the weak
        // pointer to the SharedFunctionInfo has been cleared). Record the
        // location in the FunctionLiteral, so that it will be refilled later.
        // SharedFunctionInfo::SetScript will write the SharedFunctionInfo in
        // the shared_function_infos.
        function_literal->set_function_literal_id(index);
        return MaybeHandle<SharedFunctionInfo>();
      }
      SharedFunctionInfo shared = SharedFunctionInfo::cast(heap_object);
      DCHECK_EQ(shared.StartPosition(), function_literal->start_position());
      DCHECK_EQ(shared.EndPosition(), function_literal->end_position());
      return handle(shared, isolate);
    }
  }

  // It's possible that FunctionLiterals which were processed before this one
  // were deduplicated against existing ones. Decrease function_literal_id to
  // avoid holes in shared_function_infos.
  int old_length = script->shared_function_info_count();
  int function_literal_id = old_length;
  function_literal->set_function_literal_id(function_literal_id);

  // Also add to shared_function_info_table.
  shared_function_info_table = ObjectHashTable::Put(
      shared_function_info_table,
      handle(Smi::FromInt(function_literal->start_position()), isolate),
      handle(Smi::FromInt(function_literal_id), isolate));
  script->set_shared_function_info_table(*shared_function_info_table);

  // Grow shared_function_infos if needed (we don't know the correct amount of
  // space needed upfront).
  int new_length = old_length + 1;
  Handle<WeakFixedArray> old_infos =
      handle(script->shared_function_infos(), isolate);
  if (new_length > old_infos->length()) {
    int capacity = WeakArrayList::CapacityForLength(new_length);
    Handle<WeakFixedArray> new_infos(
        isolate->factory()->NewWeakFixedArray(capacity, AllocationType::kOld));
    new_infos->CopyElements(isolate, 0, *old_infos, 0, old_length,
                            WriteBarrierMode::UPDATE_WRITE_BARRIER);
    script->set_shared_function_infos(*new_infos);
  }
  return MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<SharedFunctionInfo> Script::FindWebSnapshotSharedFunctionInfo(
    Handle<Script> script, LocalIsolate* isolate,
    FunctionLiteral* function_literal) {
  // Off-thread serialization of web snapshots is not implemented.
  UNREACHABLE();
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

// static
void JSArray::Initialize(Handle<JSArray> array, int capacity, int length) {
  DCHECK_GE(capacity, 0);
  array->GetIsolate()->factory()->NewJSArrayStorage(
      array, length, capacity, INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);
}

Maybe<bool> JSArray::SetLength(Handle<JSArray> array, uint32_t new_length) {
  if (array->SetLengthWouldNormalize(new_length)) {
    JSObject::NormalizeElements(array);
  }
  return array->GetElementsAccessor()->SetLength(array, new_length);
}

// ES6: 9.5.2 [[SetPrototypeOf]] (V)
// static
Maybe<bool> JSProxy::SetPrototype(Isolate* isolate, Handle<JSProxy> proxy,
                                  Handle<Object> value, bool from_javascript,
                                  ShouldThrow should_throw) {
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
    return JSReceiver::SetPrototype(isolate, target, value, from_javascript,
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
  DCHECK(v8_flags.trace_track_allocation_sites);
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
  if (!V8_ALLOCATION_SITE_TRACKING_BOOL) return false;
  return IsMoreGeneralElementsKindTransition(from, to);
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
}

// static
bool JSArray::MayHaveReadOnlyLength(Map js_array_map) {
  DCHECK(js_array_map.IsJSArrayMap());
  if (js_array_map.is_dictionary_map()) return true;

  // Fast path: "length" is the first fast property of arrays with non
  // dictionary properties. Since it's not configurable, it's guaranteed to be
  // the first in the descriptor array.
  InternalIndex first(0);
  DCHECK(js_array_map.instance_descriptors().GetKey(first) ==
         js_array_map.GetReadOnlyRoots().length_string());
  return js_array_map.instance_descriptors().GetDetails(first).IsReadOnly();
}

bool JSArray::HasReadOnlyLength(Handle<JSArray> array) {
  Map map = array->map();

  // If map guarantees that there can't be a read-only length, we are done.
  if (!MayHaveReadOnlyLength(map)) return false;

  // Look at the object.
  Isolate* isolate = array->GetIsolate();
  LookupIterator it(isolate, array, isolate->factory()->length_string(), array,
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
  if (!description().IsUndefined()) {
    os << " ";
    String description_as_string = String::cast(description());
    description_as_string.PrintUC16(os, 0, description_as_string.length());
  } else {
    os << " (" << PrivateSymbolToName() << ")";
  }
  os << ">";
}

v8::Promise::PromiseState JSPromise::status() const {
  int value = flags() & StatusBits::kMask;
  DCHECK(value == 0 || value == 1 || value == 2);
  return static_cast<v8::Promise::PromiseState>(value);
}

void JSPromise::set_status(Promise::PromiseState status) {
  int value = flags() & ~StatusBits::kMask;
  set_flags(value | status);
}

// static
const char* JSPromise::Status(v8::Promise::PromiseState status) {
  switch (status) {
    case v8::Promise::kFulfilled:
      return "fulfilled";
    case v8::Promise::kPending:
      return "pending";
    case v8::Promise::kRejected:
      return "rejected";
  }
  UNREACHABLE();
}

int JSPromise::async_task_id() const {
  return AsyncTaskIdBits::decode(flags());
}

void JSPromise::set_async_task_id(int id) {
  set_flags(AsyncTaskIdBits::update(flags(), id));
}

// static
Handle<Object> JSPromise::Fulfill(Handle<JSPromise> promise,
                                  Handle<Object> value) {
  Isolate* const isolate = promise->GetIsolate();

#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
  if (isolate->HasContextPromiseHooks()) {
    isolate->raw_native_context().RunPromiseHook(
        PromiseHookType::kResolve, promise,
        isolate->factory()->undefined_value());
  }
#endif

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

static void MoveMessageToPromise(Isolate* isolate, Handle<JSPromise> promise) {
  if (!isolate->has_pending_message()) return;

  Handle<Object> message = handle(isolate->pending_message(), isolate);
  Handle<Symbol> key = isolate->factory()->promise_debug_message_symbol();
  Object::SetProperty(isolate, promise, key, message, StoreOrigin::kMaybeKeyed,
                      Just(ShouldThrow::kThrowOnError))
      .Assert();

  // The message object for a rejected promise was only stored for this purpose.
  // Clear it, otherwise we might leak memory.
  isolate->clear_pending_message();
}

// static
Handle<Object> JSPromise::Reject(Handle<JSPromise> promise,
                                 Handle<Object> reason, bool debug_event) {
  Isolate* const isolate = promise->GetIsolate();
  DCHECK(
      !reinterpret_cast<v8::Isolate*>(isolate)->GetCurrentContext().IsEmpty());

  if (isolate->debug()->is_active()) MoveMessageToPromise(isolate, promise);

  if (debug_event) isolate->debug()->OnPromiseReject(promise, reason);
  isolate->RunAllPromiseHooks(PromiseHookType::kResolve, promise,
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

// https://tc39.es/ecma262/#sec-promise-resolve-functions
// static
MaybeHandle<Object> JSPromise::Resolve(Handle<JSPromise> promise,
                                       Handle<Object> resolution) {
  Isolate* const isolate = promise->GetIsolate();
  DCHECK(
      !reinterpret_cast<v8::Isolate*>(isolate)->GetCurrentContext().IsEmpty());

  isolate->RunPromiseHook(PromiseHookType::kResolve, promise,
                          isolate->factory()->undefined_value());

  // 7. If SameValue(resolution, promise) is true, then
  if (promise.is_identical_to(resolution)) {
    // a. Let selfResolutionError be a newly created TypeError object.
    Handle<Object> self_resolution_error = isolate->factory()->NewTypeError(
        MessageTemplate::kPromiseCyclic, resolution);
    // b. Return RejectPromise(promise, selfResolutionError).
    return Reject(promise, self_resolution_error);
  }

  // 8. If Type(resolution) is not Object, then
  if (!resolution->IsJSReceiver()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 9. Let then be Get(resolution, "then").
  MaybeHandle<Object> then;
  Handle<JSReceiver> receiver(Handle<JSReceiver>::cast(resolution));

  // Make sure a lookup of "then" on any JSPromise whose [[Prototype]] is the
  // initial %PromisePrototype% yields the initial method. In addition this
  // protector also guards the negative lookup of "then" on the intrinsic
  // %ObjectPrototype%, meaning that such lookups are guaranteed to yield
  // undefined without triggering any side-effects.
  if (receiver->IsJSPromise() &&
      isolate->IsInAnyContext(receiver->map().prototype(),
                              Context::PROMISE_PROTOTYPE_INDEX) &&
      Protectors::IsPromiseThenLookupChainIntact(isolate)) {
    // We can skip the "then" lookup on {resolution} if its [[Prototype]]
    // is the (initial) Promise.prototype and the Promise#then protector
    // is intact, as that guards the lookup path for the "then" property
    // on JSPromise instances which have the (initial) %PromisePrototype%.
    then = isolate->promise_then();
  } else {
    then = JSReceiver::GetProperty(isolate, receiver,
                                   isolate->factory()->then_string());
  }

  // 10. If then is an abrupt completion, then
  Handle<Object> then_action;
  if (!then.ToHandle(&then_action)) {
    // The "then" lookup can cause termination.
    if (!isolate->is_catchable_by_javascript(isolate->pending_exception())) {
      return kNullMaybeHandle;
    }

    // a. Return RejectPromise(promise, then.[[Value]]).
    Handle<Object> reason(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();
    return Reject(promise, reason, false);
  }

  // 11. Let thenAction be then.[[Value]].
  // 12. If IsCallable(thenAction) is false, then
  if (!then_action->IsCallable()) {
    // a. Return FulfillPromise(promise, resolution).
    return Fulfill(promise, resolution);
  }

  // 13. Let job be NewPromiseResolveThenableJob(promise, resolution,
  //                                             thenAction).
  Handle<NativeContext> then_context;
  if (!JSReceiver::GetContextForMicrotask(Handle<JSReceiver>::cast(then_action))
           .ToHandle(&then_context)) {
    then_context = isolate->native_context();
  }

  Handle<PromiseResolveThenableJobTask> task =
      isolate->factory()->NewPromiseResolveThenableJobTask(
          promise, Handle<JSReceiver>::cast(resolution),
          Handle<JSReceiver>::cast(then_action), then_context);
  if (isolate->debug()->is_active() && resolution->IsJSPromise()) {
    // Mark the dependency of the new {promise} on the {resolution}.
    Object::SetProperty(isolate, resolution,
                        isolate->factory()->promise_handled_by_symbol(),
                        promise)
        .Check();
  }
  MicrotaskQueue* microtask_queue = then_context->microtask_queue();
  if (microtask_queue) microtask_queue->EnqueueMicrotask(*task);

  // 15. Return undefined.
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
    DisallowGarbageCollection no_gc;
    Object current = *reactions;
    Object reversed = Smi::zero();
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

    // According to HTML, we use the context of the appropriate handler as the
    // context of the microtask. See step 3 of HTML's EnqueueJob:
    // https://html.spec.whatwg.org/C/#enqueuejob(queuename,-job,-arguments)
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

    bool has_handler_context = false;
    if (primary_handler->IsJSReceiver()) {
      has_handler_context = JSReceiver::GetContextForMicrotask(
                                Handle<JSReceiver>::cast(primary_handler))
                                .ToHandle(&handler_context);
    }
    if (!has_handler_context && secondary_handler->IsJSReceiver()) {
      has_handler_context = JSReceiver::GetContextForMicrotask(
                                Handle<JSReceiver>::cast(secondary_handler))
                                .ToHandle(&handler_context);
    }
    if (!has_handler_context) handler_context = isolate->native_context();

    static_assert(
        static_cast<int>(PromiseReaction::kSize) ==
        static_cast<int>(
            PromiseReactionJobTask::kSizeOfAllPromiseReactionJobTasks));
    if (type == PromiseReaction::kFulfill) {
      task->set_map(
          ReadOnlyRoots(isolate).promise_fulfill_reaction_job_task_map(),
          kReleaseStore);
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_argument(
          *argument);
      Handle<PromiseFulfillReactionJobTask>::cast(task)->set_context(
          *handler_context);
      static_assert(
          static_cast<int>(PromiseReaction::kFulfillHandlerOffset) ==
          static_cast<int>(PromiseFulfillReactionJobTask::kHandlerOffset));
      static_assert(
          static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
          static_cast<int>(
              PromiseFulfillReactionJobTask::kPromiseOrCapabilityOffset));
      static_assert(
          static_cast<int>(
              PromiseReaction::kContinuationPreservedEmbedderDataOffset) ==
          static_cast<int>(PromiseFulfillReactionJobTask::
                               kContinuationPreservedEmbedderDataOffset));
    } else {
      DisallowGarbageCollection no_gc;
      task->set_map(
          ReadOnlyRoots(isolate).promise_reject_reaction_job_task_map(),
          kReleaseStore);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_argument(*argument);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_context(
          *handler_context);
      Handle<PromiseRejectReactionJobTask>::cast(task)->set_handler(
          *primary_handler);
      static_assert(
          static_cast<int>(PromiseReaction::kPromiseOrCapabilityOffset) ==
          static_cast<int>(
              PromiseRejectReactionJobTask::kPromiseOrCapabilityOffset));
      static_assert(
          static_cast<int>(
              PromiseReaction::kContinuationPreservedEmbedderDataOffset) ==
          static_cast<int>(PromiseRejectReactionJobTask::
                               kContinuationPreservedEmbedderDataOffset));
    }

    MicrotaskQueue* microtask_queue = handler_context->microtask_queue();
    if (microtask_queue) {
      microtask_queue->EnqueueMicrotask(
          *Handle<PromiseReactionJobTask>::cast(task));
    }
  }

  return isolate->factory()->undefined_value();
}

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
template <typename IsolateT>
Handle<Derived> HashTable<Derived, Shape>::New(
    IsolateT* isolate, int at_least_space_for, AllocationType allocation,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  DCHECK_IMPLIES(capacity_option == USE_CUSTOM_MINIMUM_CAPACITY,
                 base::bits::IsPowerOfTwo(at_least_space_for));

  int capacity = (capacity_option == USE_CUSTOM_MINIMUM_CAPACITY)
                     ? at_least_space_for
                     : ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    isolate->FatalProcessOutOfHeapMemory("invalid table size");
  }
  return NewInternal(isolate, capacity, allocation);
}

template <typename Derived, typename Shape>
template <typename IsolateT>
Handle<Derived> HashTable<Derived, Shape>::NewInternal(
    IsolateT* isolate, int capacity, AllocationType allocation) {
  auto* factory = isolate->factory();
  int length = EntryToIndex(InternalIndex(capacity));
  Handle<FixedArray> array = factory->NewFixedArrayWithMap(
      Derived::GetMap(ReadOnlyRoots(isolate)), length, allocation);
  Handle<Derived> table = Handle<Derived>::cast(array);
  DisallowGarbageCollection no_gc;
  auto raw_table = *table;
  raw_table.SetNumberOfElements(0);
  raw_table.SetNumberOfDeletedElements(0);
  raw_table.SetCapacity(capacity);
  return table;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Rehash(PtrComprCageBase cage_base,
                                       Derived new_table) {
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = new_table.GetWriteBarrierMode(no_gc);

  DCHECK_LT(NumberOfElements(), new_table.Capacity());

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex; i < kElementsStartIndex; i++) {
    new_table.set(i, get(cage_base, i), mode);
  }

  // Rehash the elements.
  ReadOnlyRoots roots = GetReadOnlyRoots(cage_base);
  for (InternalIndex i : this->IterateEntries()) {
    uint32_t from_index = EntryToIndex(i);
    Object k = this->get(cage_base, from_index);
    if (!IsKey(roots, k)) continue;
    uint32_t hash = Shape::HashForObject(roots, k);
    uint32_t insertion_index =
        EntryToIndex(new_table.FindInsertionEntry(cage_base, roots, hash));
    new_table.set_key(insertion_index, get(cage_base, from_index), mode);
    for (int j = 1; j < Shape::kEntrySize; j++) {
      new_table.set(insertion_index + j, get(cage_base, from_index + j), mode);
    }
  }
  new_table.SetNumberOfElements(NumberOfElements());
  new_table.SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
InternalIndex HashTable<Derived, Shape>::EntryForProbe(ReadOnlyRoots roots,
                                                       Object k, int probe,
                                                       InternalIndex expected) {
  uint32_t hash = Shape::HashForObject(roots, k);
  uint32_t capacity = this->Capacity();
  InternalIndex entry = FirstProbe(hash, capacity);
  for (int i = 1; i < probe; i++) {
    if (entry == expected) return expected;
    entry = NextProbe(entry, i, capacity);
  }
  return entry;
}

template <typename Derived, typename Shape>
void HashTable<Derived, Shape>::Swap(InternalIndex entry1, InternalIndex entry2,
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
void HashTable<Derived, Shape>::Rehash(PtrComprCageBase cage_base) {
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = GetWriteBarrierMode(no_gc);
  ReadOnlyRoots roots = GetReadOnlyRoots(cage_base);
  uint32_t capacity = Capacity();
  bool done = false;
  for (int probe = 1; !done; probe++) {
    // All elements at entries given by one of the first _probe_ probes
    // are placed correctly. Other elements might need to be moved.
    done = true;
    for (InternalIndex current(0); current.raw_value() < capacity;
         /* {current} is advanced manually below, when appropriate.*/) {
      Object current_key = KeyAt(cage_base, current);
      if (!IsKey(roots, current_key)) {
        ++current;  // Advance to next entry.
        continue;
      }
      InternalIndex target = EntryForProbe(roots, current_key, probe, current);
      if (current == target) {
        ++current;  // Advance to next entry.
        continue;
      }
      Object target_key = KeyAt(cage_base, target);
      if (!IsKey(roots, target_key) ||
          EntryForProbe(roots, target_key, probe, target) != target) {
        // Put the current element into the correct position.
        Swap(current, target, mode);
        // The other element will be processed on the next iteration,
        // so don't advance {current} here!
      } else {
        // The place for the current element is occupied. Leave the element
        // for the next probe.
        done = false;
        ++current;  // Advance to next entry.
      }
    }
  }
  // Wipe deleted entries.
  Object the_hole = roots.the_hole_value();
  HeapObject undefined = roots.undefined_value();
  Derived* self = static_cast<Derived*>(this);
  for (InternalIndex current : InternalIndex::Range(capacity)) {
    if (KeyAt(cage_base, current) == the_hole) {
      self->set_key(EntryToIndex(current) + kEntryKeyIndex, undefined,
                    SKIP_WRITE_BARRIER);
    }
  }
  SetNumberOfDeletedElements(0);
}

template <typename Derived, typename Shape>
template <typename IsolateT>
Handle<Derived> HashTable<Derived, Shape>::EnsureCapacity(
    IsolateT* isolate, Handle<Derived> table, int n,
    AllocationType allocation) {
  if (table->HasSufficientCapacityToAdd(n)) return table;

  int capacity = table->Capacity();
  int new_nof = table->NumberOfElements() + n;

  bool should_pretenure = allocation == AllocationType::kOld ||
                          ((capacity > kMinCapacityForPretenure) &&
                           !Heap::InYoungGeneration(*table));
  Handle<Derived> new_table = HashTable::New(
      isolate, new_nof,
      should_pretenure ? AllocationType::kOld : AllocationType::kYoung);

  table->Rehash(isolate, *new_table);
  return new_table;
}

template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::HasSufficientCapacityToAdd(
    int number_of_additional_elements) {
  return HasSufficientCapacityToAdd(Capacity(), NumberOfElements(),
                                    NumberOfDeletedElements(),
                                    number_of_additional_elements);
}

// static
template <typename Derived, typename Shape>
bool HashTable<Derived, Shape>::HasSufficientCapacityToAdd(
    int capacity, int number_of_elements, int number_of_deleted_elements,
    int number_of_additional_elements) {
  int nof = number_of_elements + number_of_additional_elements;
  // Return true if:
  //   50% is still free after adding number_of_additional_elements elements and
  //   at most 50% of the free elements are deleted elements.
  if ((nof < capacity) &&
      ((number_of_deleted_elements <= (capacity - nof) / 2))) {
    int needed_free = nof / 2;
    if (nof + needed_free <= capacity) return true;
  }
  return false;
}

// static
template <typename Derived, typename Shape>
int HashTable<Derived, Shape>::ComputeCapacityWithShrink(
    int current_capacity, int at_least_room_for) {
  // Shrink to fit the number of elements if only a quarter of the
  // capacity is filled with elements.
  if (at_least_room_for > (current_capacity / 4)) return current_capacity;
  // Recalculate the smaller capacity actually needed.
  int new_capacity = ComputeCapacity(at_least_room_for);
  DCHECK_GE(new_capacity, at_least_room_for);
  // Don't go lower than room for {kMinShrinkCapacity} elements.
  if (new_capacity < Derived::kMinShrinkCapacity) return current_capacity;
  return new_capacity;
}

// static
template <typename Derived, typename Shape>
Handle<Derived> HashTable<Derived, Shape>::Shrink(Isolate* isolate,
                                                  Handle<Derived> table,
                                                  int additional_capacity) {
  int new_capacity = ComputeCapacityWithShrink(
      table->Capacity(), table->NumberOfElements() + additional_capacity);
  if (new_capacity == table->Capacity()) return table;
  DCHECK_GE(new_capacity, Derived::kMinShrinkCapacity);

  bool pretenure = (new_capacity > kMinCapacityForPretenure) &&
                   !Heap::InYoungGeneration(*table);
  Handle<Derived> new_table =
      HashTable::New(isolate, new_capacity,
                     pretenure ? AllocationType::kOld : AllocationType::kYoung,
                     USE_CUSTOM_MINIMUM_CAPACITY);

  table->Rehash(isolate, *new_table);
  return new_table;
}

template <typename Derived, typename Shape>
InternalIndex HashTable<Derived, Shape>::FindInsertionEntry(
    PtrComprCageBase cage_base, ReadOnlyRoots roots, uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t count = 1;
  // EnsureCapacity will guarantee the hash table is never full.
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    if (!IsKey(roots, KeyAt(cage_base, entry))) return entry;
  }
}

base::Optional<PropertyCell>
GlobalDictionary::TryFindPropertyCellForConcurrentLookupIterator(
    Isolate* isolate, Handle<Name> name, RelaxedLoadTag tag) {
  // This reimplements HashTable::FindEntry for use in a concurrent setting.
  // 1) Atomic loads.
  // 2) IsPendingAllocation checks.
  // 3) Return the PropertyCell value instead of the InternalIndex to avoid a
  //   repeated load (unsafe with concurrent modifications).

  DisallowGarbageCollection no_gc;
  PtrComprCageBase cage_base{isolate};
  ReadOnlyRoots roots(isolate);
  const int32_t hash = ShapeT::Hash(roots, name);
  const uint32_t capacity = Capacity();
  uint32_t count = 1;
  Object undefined = roots.undefined_value();
  Object the_hole = roots.the_hole_value();
  // EnsureCapacity will guarantee the hash table is never full.
  for (InternalIndex entry = FirstProbe(hash, capacity);;
       entry = NextProbe(entry, count++, capacity)) {
    Object element = KeyAt(cage_base, entry, kRelaxedLoad);
    if (isolate->heap()->IsPendingAllocation(element)) return {};
    if (element == undefined) return {};
    if (ShapeT::kMatchNeedsHoleCheck && element == the_hole) continue;
    if (!ShapeT::IsMatch(name, element)) continue;
    CHECK(element.IsPropertyCell(cage_base));
    return PropertyCell::cast(element);
  }
}

Handle<StringSet> StringSet::New(Isolate* isolate) {
  return HashTable::New(isolate, 0);
}

Handle<StringSet> StringSet::Add(Isolate* isolate, Handle<StringSet> stringset,
                                 Handle<String> name) {
  if (!stringset->Has(isolate, name)) {
    stringset = EnsureCapacity(isolate, stringset);
    uint32_t hash = ShapeT::Hash(ReadOnlyRoots(isolate), *name);
    InternalIndex entry = stringset->FindInsertionEntry(isolate, hash);
    stringset->set(EntryToIndex(entry), *name);
    stringset->ElementAdded();
  }
  return stringset;
}

bool StringSet::Has(Isolate* isolate, Handle<String> name) {
  return FindEntry(isolate, *name).is_found();
}

Handle<RegisteredSymbolTable> RegisteredSymbolTable::Add(
    Isolate* isolate, Handle<RegisteredSymbolTable> table, Handle<String> key,
    Handle<Symbol> symbol) {
  // Validate that the key is absent.
  SLOW_DCHECK(table->FindEntry(isolate, key).is_not_found());

  table = EnsureCapacity(isolate, table);
  uint32_t hash = ShapeT::Hash(ReadOnlyRoots(isolate), key);
  InternalIndex entry = table->FindInsertionEntry(isolate, hash);
  table->set(EntryToIndex(entry), *key);
  table->set(EntryToValueIndex(entry), *symbol);
  table->ElementAdded();
  return table;
}

Handle<ObjectHashSet> ObjectHashSet::Add(Isolate* isolate,
                                         Handle<ObjectHashSet> set,
                                         Handle<Object> key) {
  int32_t hash = key->GetOrCreateHash(isolate).value();
  if (!set->Has(isolate, key, hash)) {
    set = EnsureCapacity(isolate, set);
    InternalIndex entry = set->FindInsertionEntry(isolate, hash);
    set->set(EntryToIndex(entry), *key);
    set->ElementAdded();
  }
  return set;
}

template <typename Derived, typename Shape>
template <typename IsolateT>
Handle<Derived> BaseNameDictionary<Derived, Shape>::New(
    IsolateT* isolate, int at_least_space_for, AllocationType allocation,
    MinimumCapacity capacity_option) {
  DCHECK_LE(0, at_least_space_for);
  Handle<Derived> dict = Dictionary<Derived, Shape>::New(
      isolate, at_least_space_for, allocation, capacity_option);
  dict->SetHash(PropertyArray::kNoHashSentinel);
  dict->set_next_enumeration_index(PropertyDetails::kInitialIndex);
  return dict;
}

template <typename Derived, typename Shape>
int BaseNameDictionary<Derived, Shape>::NextEnumerationIndex(
    Isolate* isolate, Handle<Derived> dictionary) {
  int index = dictionary->next_enumeration_index();
  // Check whether the next enumeration index is valid.
  if (!PropertyDetails::IsValidIndex(index)) {
    // If not, we generate new indices for the properties.
    Handle<FixedArray> iteration_order = IterationIndices(isolate, dictionary);
    int length = iteration_order->length();
    DCHECK_LE(length, dictionary->NumberOfElements());

    // Iterate over the dictionary using the enumeration order and update
    // the dictionary with new enumeration indices.
    for (int i = 0; i < length; i++) {
      InternalIndex internal_index(Smi::ToInt(iteration_order->get(i)));
      DCHECK(dictionary->IsKey(dictionary->GetReadOnlyRoots(),
                               dictionary->KeyAt(isolate, internal_index)));

      int enum_index = PropertyDetails::kInitialIndex + i;

      PropertyDetails details = dictionary->DetailsAt(internal_index);
      PropertyDetails new_details = details.set_index(enum_index);
      dictionary->DetailsAtPut(internal_index, new_details);
    }

    index = PropertyDetails::kInitialIndex + length;
  }

  // Don't update the next enumeration index here, since we might be looking at
  // an immutable empty dictionary.
  return index;
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::DeleteEntry(
    Isolate* isolate, Handle<Derived> dictionary, InternalIndex entry) {
  DCHECK(Shape::kEntrySize != 3 ||
         dictionary->DetailsAt(entry).IsConfigurable());
  dictionary->ClearEntry(entry);
  dictionary->ElementRemoved();
  return Shrink(isolate, dictionary);
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::AtPut(Isolate* isolate,
                                                  Handle<Derived> dictionary,
                                                  Key key, Handle<Object> value,
                                                  PropertyDetails details) {
  InternalIndex entry = dictionary->FindEntry(isolate, key);

  // If the entry is present set the value;
  if (entry.is_not_found()) {
    return Derived::Add(isolate, dictionary, key, value, details);
  }

  // We don't need to copy over the enumeration index.
  dictionary->ValueAtPut(entry, *value);
  if (Shape::kEntrySize == 3) dictionary->DetailsAtPut(entry, details);
  return dictionary;
}

template <typename Derived, typename Shape>
template <typename IsolateT>
Handle<Derived>
BaseNameDictionary<Derived, Shape>::AddNoUpdateNextEnumerationIndex(
    IsolateT* isolate, Handle<Derived> dictionary, Key key,
    Handle<Object> value, PropertyDetails details, InternalIndex* entry_out) {
  // Insert element at empty or deleted entry.
  return Dictionary<Derived, Shape>::Add(isolate, dictionary, key, value,
                                         details, entry_out);
}

template <typename Derived, typename Shape>
Handle<Derived> BaseNameDictionary<Derived, Shape>::Add(
    Isolate* isolate, Handle<Derived> dictionary, Key key, Handle<Object> value,
    PropertyDetails details, InternalIndex* entry_out) {
  // Insert element at empty or deleted entry
  DCHECK_EQ(0, details.dictionary_index());
  // Assign an enumeration index to the property and update
  // SetNextEnumerationIndex.
  int index = Derived::NextEnumerationIndex(isolate, dictionary);
  details = details.set_index(index);
  dictionary = AddNoUpdateNextEnumerationIndex(isolate, dictionary, key, value,
                                               details, entry_out);
  // Update enumeration index here in order to avoid potential modification of
  // the canonical empty dictionary which lives in read only space.
  dictionary->set_next_enumeration_index(index + 1);
  return dictionary;
}

template <typename Derived, typename Shape>
template <typename IsolateT>
Handle<Derived> Dictionary<Derived, Shape>::Add(IsolateT* isolate,
                                                Handle<Derived> dictionary,
                                                Key key, Handle<Object> value,
                                                PropertyDetails details,
                                                InternalIndex* entry_out) {
  ReadOnlyRoots roots(isolate);
  uint32_t hash = Shape::Hash(roots, key);
  // Validate that the key is absent.
  SLOW_DCHECK(dictionary->FindEntry(isolate, key).is_not_found());
  // Check whether the dictionary should be extended.
  dictionary = Derived::EnsureCapacity(isolate, dictionary);

  // Compute the key object.
  Handle<Object> k = Shape::AsHandle(isolate, key);

  InternalIndex entry = dictionary->FindInsertionEntry(isolate, roots, hash);
  dictionary->SetEntry(entry, *k, *value, details);
  DCHECK(dictionary->KeyAt(isolate, entry).IsNumber() ||
         Shape::Unwrap(dictionary->KeyAt(isolate, entry)).IsUniqueName());
  dictionary->ElementAdded();
  if (entry_out) *entry_out = entry;
  return dictionary;
}

template <typename Derived, typename Shape>
Handle<Derived> Dictionary<Derived, Shape>::ShallowCopy(
    Isolate* isolate, Handle<Derived> dictionary) {
  return Handle<Derived>::cast(isolate->factory()->CopyFixedArrayWithMap(
      dictionary, Derived::GetMap(ReadOnlyRoots(isolate))));
}

// static
Handle<SimpleNumberDictionary> SimpleNumberDictionary::Set(
    Isolate* isolate, Handle<SimpleNumberDictionary> dictionary, uint32_t key,
    Handle<Object> value) {
  return AtPut(isolate, dictionary, key, value, PropertyDetails::Empty());
}

void NumberDictionary::UpdateMaxNumberKey(uint32_t key,
                                          Handle<JSObject> dictionary_holder) {
  DisallowGarbageCollection no_gc;
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
  // We could call Set with empty dictionaries. UpdateMaxNumberKey doesn't
  // expect empty dictionaries so make sure to call AtPut that correctly handles
  // them by creating new dictionary when required.
  Handle<NumberDictionary> new_dictionary =
      AtPut(isolate, dictionary, key, value, details);
  new_dictionary->UpdateMaxNumberKey(key, dictionary_holder);
  return new_dictionary;
}

void NumberDictionary::CopyValuesTo(FixedArray elements) {
  ReadOnlyRoots roots = GetReadOnlyRoots();
  int pos = 0;
  DisallowGarbageCollection no_gc;
  WriteBarrierMode mode = elements.GetWriteBarrierMode(no_gc);
  for (InternalIndex i : this->IterateEntries()) {
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
  int result = 0;
  for (InternalIndex i : this->IterateEntries()) {
    Object k;
    if (!this->ToKey(roots, i, &k)) continue;
    if (k.FilterKey(ENUMERABLE_STRINGS)) continue;
    PropertyDetails details = this->DetailsAt(i);
    PropertyAttributes attr = details.attributes();
    if ((int{attr} & ONLY_ENUMERABLE) == 0) result++;
  }
  return result;
}

template <typename Derived, typename Shape>
Handle<FixedArray> BaseNameDictionary<Derived, Shape>::IterationIndices(
    Isolate* isolate, Handle<Derived> dictionary) {
  Handle<FixedArray> array =
      isolate->factory()->NewFixedArray(dictionary->NumberOfElements());
  ReadOnlyRoots roots(isolate);
  int array_size = 0;
  {
    DisallowGarbageCollection no_gc;
    Derived raw_dictionary = *dictionary;
    for (InternalIndex i : dictionary->IterateEntries()) {
      Object k;
      if (!raw_dictionary.ToKey(roots, i, &k)) continue;
      array->set(array_size++, Smi::FromInt(i.as_int()));
    }

    // The global dictionary doesn't track its deletion count, so we may iterate
    // fewer entries than the count of elements claimed by the dictionary.
    if (std::is_same<Derived, GlobalDictionary>::value) {
      DCHECK_LE(array_size, dictionary->NumberOfElements());
    } else {
      DCHECK_EQ(array_size, dictionary->NumberOfElements());
    }

    EnumIndexComparator<Derived> cmp(raw_dictionary);
    // Use AtomicSlot wrapper to ensure that std::sort uses atomic load and
    // store operations that are safe for concurrent marking.
    AtomicSlot start(array->GetFirstElementAddress());
    std::sort(start, start + array_size, cmp);
  }
  return FixedArray::ShrinkOrEmpty(isolate, array, array_size);
}

// Backwards lookup (slow).
template <typename Derived, typename Shape>
Object Dictionary<Derived, Shape>::SlowReverseLookup(Object value) {
  Derived dictionary = Derived::cast(*this);
  ReadOnlyRoots roots = dictionary.GetReadOnlyRoots();
  for (InternalIndex i : dictionary.IterateEntries()) {
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
  for (int i = Derived::EntryToIndex(InternalIndex(0)); i < length; i++) {
    table->set_the_hole(i);
  }
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(PtrComprCageBase cage_base,
                                                   Handle<Object> key,
                                                   int32_t hash) {
  DisallowGarbageCollection no_gc;
  ReadOnlyRoots roots = this->GetReadOnlyRoots(cage_base);
  DCHECK(this->IsKey(roots, *key));

  InternalIndex entry = this->FindEntry(cage_base, roots, key, hash);
  if (entry.is_not_found()) return roots.the_hole_value();
  return this->get(Derived::EntryToIndex(entry) + 1);
}

// The implementation should be in sync with
// CodeStubAssembler::NameToIndexHashTableLookup.
int NameToIndexHashTable::Lookup(Handle<Name> key) {
  DisallowGarbageCollection no_gc;
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  ReadOnlyRoots roots = this->GetReadOnlyRoots(cage_base);

  InternalIndex entry = this->FindEntry(cage_base, roots, key, key->hash());
  if (entry.is_not_found()) return -1;
  return Smi::cast(this->get(EntryToValueIndex(entry))).value();
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key) {
  DisallowGarbageCollection no_gc;

  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  ReadOnlyRoots roots = this->GetReadOnlyRoots(cage_base);
  DCHECK(this->IsKey(roots, *key));

  // If the object does not have an identity hash, it was never used as a key.
  Object hash = key->GetHash();
  if (hash.IsUndefined(roots)) {
    return roots.the_hole_value();
  }
  return Lookup(cage_base, key, Smi::ToInt(hash));
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::Lookup(Handle<Object> key,
                                                   int32_t hash) {
  return Lookup(GetPtrComprCageBase(*this), key, hash);
}

template <typename Derived, typename Shape>
Object ObjectHashTableBase<Derived, Shape>::ValueAt(InternalIndex entry) {
  return this->get(EntryToValueIndex(entry));
}

Object RegisteredSymbolTable::ValueAt(InternalIndex entry) {
  return this->get(EntryToValueIndex(entry));
}

Object NameToIndexHashTable::ValueAt(InternalIndex entry) {
  return this->get(EntryToValueIndex(entry));
}

int NameToIndexHashTable::IndexAt(InternalIndex entry) {
  Object value = ValueAt(entry);
  if (value.IsSmi()) {
    int index = Smi::ToInt(value);
    DCHECK_LE(0, index);
    return index;
  }
  return -1;
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

  InternalIndex entry = table->FindEntry(isolate, roots, key, hash);

  // Key is already in table, just overwrite value.
  if (entry.is_found()) {
    table->set(Derived::EntryToValueIndex(entry), *value);
    return table;
  }

  // Rehash if more than 33% of the entries are deleted entries.
  // TODO(verwaest): Consider to shrink the fixed array in place.
  if ((table->NumberOfDeletedElements() << 1) > table->NumberOfElements()) {
    table->Rehash(isolate);
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
      table->Rehash(isolate);
    }
  }

  // Check whether the hash table should be extended.
  table = Derived::EnsureCapacity(isolate, table);
  table->AddEntry(table->FindInsertionEntry(isolate, hash), *key, *value);
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

  InternalIndex entry = table->FindEntry(isolate, roots, key, hash);
  if (entry.is_not_found()) {
    *was_present = false;
    return table;
  }

  *was_present = true;
  table->RemoveEntry(entry);
  return Derived::Shrink(isolate, table);
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::AddEntry(InternalIndex entry,
                                                   Object key, Object value) {
  Derived* self = static_cast<Derived*>(this);
  self->set_key(Derived::EntryToIndex(entry), key);
  self->set(Derived::EntryToValueIndex(entry), value);
  self->ElementAdded();
}

template <typename Derived, typename Shape>
void ObjectHashTableBase<Derived, Shape>::RemoveEntry(InternalIndex entry) {
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

void JSSet::Rehash(Isolate* isolate) {
  Handle<OrderedHashSet> table_handle(OrderedHashSet::cast(table()), isolate);
  Handle<OrderedHashSet> new_table =
      OrderedHashSet::Rehash(isolate, table_handle).ToHandleChecked();
  set_table(*new_table);
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

void JSMap::Rehash(Isolate* isolate) {
  Handle<OrderedHashMap> table_handle(OrderedHashMap::cast(table()), isolate);
  Handle<OrderedHashMap> new_table =
      OrderedHashMap::Rehash(isolate, table_handle).ToHandleChecked();
  set_table(*new_table);
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
    DisallowGarbageCollection no_gc;
    ReadOnlyRoots roots = ReadOnlyRoots(isolate);
    int count = 0;
    for (int i = 0;
         count / values_per_entry < max_entries && i < table->Capacity(); i++) {
      Object key;
      if (table->ToKey(roots, InternalIndex(i), &key)) {
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

void PropertyCell::ClearAndInvalidate(ReadOnlyRoots roots) {
  DCHECK(!value().IsTheHole(roots));
  PropertyDetails details = property_details();
  details = details.set_cell_type(PropertyCellType::kConstant);
  Transition(details, roots.the_hole_value_handle());
  // TODO(11527): pass Isolate as an argument.
  Isolate* isolate = GetIsolateFromWritableObject(*this);
  DependentCode::DeoptimizeDependencyGroups(
      isolate, *this, DependentCode::kPropertyCellChangedGroup);
}

// static
Handle<PropertyCell> PropertyCell::InvalidateAndReplaceEntry(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, InternalIndex entry,
    PropertyDetails new_details, Handle<Object> new_value) {
  Handle<PropertyCell> cell(dictionary->CellAt(entry), isolate);
  Handle<Name> name(cell->name(), isolate);
  DCHECK(cell->property_details().IsConfigurable());
  DCHECK(!cell->value().IsTheHole(isolate));

  // Swap with a new property cell.
  Handle<PropertyCell> new_cell =
      isolate->factory()->NewPropertyCell(name, new_details, new_value);
  dictionary->ValueAtPut(entry, *new_cell);

  cell->ClearAndInvalidate(ReadOnlyRoots(isolate));
  return new_cell;
}

static bool RemainsConstantType(PropertyCell cell, Object value) {
  DisallowGarbageCollection no_gc;
  // TODO(dcarney): double->smi and smi->double transition from kConstant
  if (cell.value().IsSmi() && value.IsSmi()) {
    return true;
  } else if (cell.value().IsHeapObject() && value.IsHeapObject()) {
    Map map = HeapObject::cast(value).map();
    return HeapObject::cast(cell.value()).map() == map && map.is_stable();
  }
  return false;
}

// static
PropertyCellType PropertyCell::InitialType(Isolate* isolate, Object value) {
  return value.IsUndefined(isolate) ? PropertyCellType::kUndefined
                                    : PropertyCellType::kConstant;
}

// static
PropertyCellType PropertyCell::UpdatedType(Isolate* isolate, PropertyCell cell,
                                           Object value,
                                           PropertyDetails details) {
  DisallowGarbageCollection no_gc;
  DCHECK(!value.IsTheHole(isolate));
  DCHECK(!cell.value().IsTheHole(isolate));
  switch (details.cell_type()) {
    case PropertyCellType::kUndefined:
      return PropertyCellType::kConstant;
    case PropertyCellType::kConstant:
      if (value == cell.value()) return PropertyCellType::kConstant;
      V8_FALLTHROUGH;
    case PropertyCellType::kConstantType:
      if (RemainsConstantType(cell, value)) {
        return PropertyCellType::kConstantType;
      }
      V8_FALLTHROUGH;
    case PropertyCellType::kMutable:
      return PropertyCellType::kMutable;
    case PropertyCellType::kInTransition:
      UNREACHABLE();
  }
}

Handle<PropertyCell> PropertyCell::PrepareForAndSetValue(
    Isolate* isolate, Handle<GlobalDictionary> dictionary, InternalIndex entry,
    Handle<Object> value, PropertyDetails details) {
  DCHECK(!value->IsTheHole(isolate));
  PropertyCell raw_cell = dictionary->CellAt(entry);
  CHECK(!raw_cell.value().IsTheHole(isolate));
  const PropertyDetails original_details = raw_cell.property_details();
  // Data accesses could be cached in ics or optimized code.
  bool invalidate = original_details.kind() == PropertyKind::kData &&
                    details.kind() == PropertyKind::kAccessor;
  int index = original_details.dictionary_index();
  DCHECK_LT(0, index);
  details = details.set_index(index);

  PropertyCellType new_type =
      UpdatedType(isolate, raw_cell, *value, original_details);
  details = details.set_cell_type(new_type);

  Handle<PropertyCell> cell(raw_cell, isolate);

  if (invalidate) {
    cell = PropertyCell::InvalidateAndReplaceEntry(isolate, dictionary, entry,
                                                   details, value);
  } else {
    cell->Transition(details, value);
    // Deopt when transitioning from a constant type or when making a writable
    // property read-only. Making a read-only property writable again is not
    // interesting because Turbofan does not currently rely on read-only unless
    // the property is also configurable, in which case it will stay read-only
    // forever.
    if (original_details.cell_type() != new_type ||
        (!original_details.IsReadOnly() && details.IsReadOnly())) {
      DependentCode::DeoptimizeDependencyGroups(
          isolate, *cell, DependentCode::kPropertyCellChangedGroup);
    }
  }
  return cell;
}

// static
void PropertyCell::InvalidateProtector() {
  if (value() != Smi::FromInt(Protectors::kProtectorInvalid)) {
    DCHECK_EQ(value(), Smi::FromInt(Protectors::kProtectorValid));
    set_value(Smi::FromInt(Protectors::kProtectorInvalid), kReleaseStore);
    // TODO(11527): pass Isolate as an argument.
    Isolate* isolate = GetIsolateFromWritableObject(*this);
    DependentCode::DeoptimizeDependencyGroups(
        isolate, *this, DependentCode::kPropertyCellChangedGroup);
  }
}

// static
bool PropertyCell::CheckDataIsCompatible(PropertyDetails details,
                                         Object value) {
  DisallowGarbageCollection no_gc;
  PropertyCellType cell_type = details.cell_type();
  CHECK_NE(cell_type, PropertyCellType::kInTransition);
  if (value.IsTheHole()) {
    CHECK_EQ(cell_type, PropertyCellType::kConstant);
  } else {
    CHECK_EQ(value.IsAccessorInfo() || value.IsAccessorPair(),
             details.kind() == PropertyKind::kAccessor);
    DCHECK_IMPLIES(cell_type == PropertyCellType::kUndefined,
                   value.IsUndefined());
  }
  return true;
}

#ifdef DEBUG
bool PropertyCell::CanTransitionTo(PropertyDetails new_details,
                                   Object new_value) const {
  // Extending the implementation of PropertyCells with additional states
  // and/or transitions likely requires changes to PropertyCellData::Serialize.
  DisallowGarbageCollection no_gc;
  DCHECK(CheckDataIsCompatible(new_details, new_value));
  switch (property_details().cell_type()) {
    case PropertyCellType::kUndefined:
      return new_details.cell_type() != PropertyCellType::kUndefined;
    case PropertyCellType::kConstant:
      return !value().IsTheHole() &&
             new_details.cell_type() != PropertyCellType::kUndefined;
    case PropertyCellType::kConstantType:
      return new_details.cell_type() == PropertyCellType::kConstantType ||
             new_details.cell_type() == PropertyCellType::kMutable ||
             (new_details.cell_type() == PropertyCellType::kConstant &&
              new_value.IsTheHole());
    case PropertyCellType::kMutable:
      return new_details.cell_type() == PropertyCellType::kMutable ||
             (new_details.cell_type() == PropertyCellType::kConstant &&
              new_value.IsTheHole());
    case PropertyCellType::kInTransition:
      UNREACHABLE();
  }
}
#endif  // DEBUG

int JSGeneratorObject::code_offset() const {
  DCHECK(input_or_debug_pos().IsSmi());
  int code_offset = Smi::ToInt(input_or_debug_pos());

  // The stored bytecode offset is relative to a different base than what
  // is used in the source position table, hence the subtraction.
  code_offset -= BytecodeArray::kHeaderSize - kHeapObjectTag;
  return code_offset;
}

int JSGeneratorObject::source_position() const {
  CHECK(is_suspended());
  DCHECK(function().shared().HasBytecodeArray());
  Isolate* isolate = GetIsolate();
  DCHECK(
      function().shared().GetBytecodeArray(isolate).HasSourcePositionTable());
  AbstractCode code =
      AbstractCode::cast(function().shared().GetBytecodeArray(isolate));
  return code.SourcePosition(isolate, code_offset());
}

// static
AccessCheckInfo AccessCheckInfo::Get(Isolate* isolate,
                                     Handle<JSObject> receiver) {
  DisallowGarbageCollection no_gc;
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

Address Smi::LexicographicCompare(Isolate* isolate, Smi x, Smi y) {
  DisallowGarbageCollection no_gc;
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

#define EXTERN_DEFINE_HASH_TABLE(DERIVED, SHAPE)                            \
  template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)                  \
      HashTable<DERIVED, SHAPE>;                                            \
                                                                            \
  template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) Handle<DERIVED>        \
  HashTable<DERIVED, SHAPE>::New(Isolate*, int, AllocationType,             \
                                 MinimumCapacity);                          \
  template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) Handle<DERIVED>        \
  HashTable<DERIVED, SHAPE>::New(LocalIsolate*, int, AllocationType,        \
                                 MinimumCapacity);                          \
                                                                            \
  template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) Handle<DERIVED>        \
  HashTable<DERIVED, SHAPE>::EnsureCapacity(Isolate*, Handle<DERIVED>, int, \
                                            AllocationType);                \
  template EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE) Handle<DERIVED>        \
  HashTable<DERIVED, SHAPE>::EnsureCapacity(LocalIsolate*, Handle<DERIVED>, \
                                            int, AllocationType);

#define EXTERN_DEFINE_OBJECT_BASE_HASH_TABLE(DERIVED, SHAPE) \
  EXTERN_DEFINE_HASH_TABLE(DERIVED, SHAPE)                   \
  template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)   \
      ObjectHashTableBase<DERIVED, SHAPE>;

#define EXTERN_DEFINE_DICTIONARY(DERIVED, SHAPE)                               \
  EXTERN_DEFINE_HASH_TABLE(DERIVED, SHAPE)                                     \
  template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)                     \
      Dictionary<DERIVED, SHAPE>;                                              \
                                                                               \
  template V8_EXPORT_PRIVATE Handle<DERIVED> Dictionary<DERIVED, SHAPE>::Add(  \
      Isolate* isolate, Handle<DERIVED>, Key, Handle<Object>, PropertyDetails, \
      InternalIndex*);                                                         \
  template V8_EXPORT_PRIVATE Handle<DERIVED> Dictionary<DERIVED, SHAPE>::Add(  \
      LocalIsolate* isolate, Handle<DERIVED>, Key, Handle<Object>,             \
      PropertyDetails, InternalIndex*);

#define EXTERN_DEFINE_BASE_NAME_DICTIONARY(DERIVED, SHAPE)                     \
  EXTERN_DEFINE_DICTIONARY(DERIVED, SHAPE)                                     \
  template class EXPORT_TEMPLATE_DEFINE(V8_EXPORT_PRIVATE)                     \
      BaseNameDictionary<DERIVED, SHAPE>;                                      \
                                                                               \
  template V8_EXPORT_PRIVATE Handle<DERIVED>                                   \
  BaseNameDictionary<DERIVED, SHAPE>::New(Isolate*, int, AllocationType,       \
                                          MinimumCapacity);                    \
  template V8_EXPORT_PRIVATE Handle<DERIVED>                                   \
  BaseNameDictionary<DERIVED, SHAPE>::New(LocalIsolate*, int, AllocationType,  \
                                          MinimumCapacity);                    \
                                                                               \
  template Handle<DERIVED>                                                     \
  BaseNameDictionary<DERIVED, SHAPE>::AddNoUpdateNextEnumerationIndex(         \
      Isolate* isolate, Handle<DERIVED>, Key, Handle<Object>, PropertyDetails, \
      InternalIndex*);                                                         \
  template Handle<DERIVED>                                                     \
  BaseNameDictionary<DERIVED, SHAPE>::AddNoUpdateNextEnumerationIndex(         \
      LocalIsolate* isolate, Handle<DERIVED>, Key, Handle<Object>,             \
      PropertyDetails, InternalIndex*);

EXTERN_DEFINE_HASH_TABLE(StringSet, StringSetShape)
EXTERN_DEFINE_HASH_TABLE(CompilationCacheTable, CompilationCacheShape)
EXTERN_DEFINE_HASH_TABLE(ObjectHashSet, ObjectHashSetShape)
EXTERN_DEFINE_HASH_TABLE(NameToIndexHashTable, NameToIndexShape)
EXTERN_DEFINE_HASH_TABLE(RegisteredSymbolTable, RegisteredSymbolTableShape)

EXTERN_DEFINE_OBJECT_BASE_HASH_TABLE(ObjectHashTable, ObjectHashTableShape)
EXTERN_DEFINE_OBJECT_BASE_HASH_TABLE(EphemeronHashTable, ObjectHashTableShape)

EXTERN_DEFINE_DICTIONARY(SimpleNumberDictionary, SimpleNumberDictionaryShape)
EXTERN_DEFINE_DICTIONARY(NumberDictionary, NumberDictionaryShape)

EXTERN_DEFINE_BASE_NAME_DICTIONARY(NameDictionary, NameDictionaryShape)
EXTERN_DEFINE_BASE_NAME_DICTIONARY(GlobalDictionary, GlobalDictionaryShape)

#undef EXTERN_DEFINE_HASH_TABLE
#undef EXTERN_DEFINE_OBJECT_BASE_HASH_TABLE
#undef EXTERN_DEFINE_DICTIONARY
#undef EXTERN_DEFINE_BASE_NAME_DICTIONARY

void JSFinalizationRegistry::RemoveCellFromUnregisterTokenMap(
    Isolate* isolate, Address raw_finalization_registry,
    Address raw_weak_cell) {
  DisallowGarbageCollection no_gc;
  JSFinalizationRegistry finalization_registry =
      JSFinalizationRegistry::cast(Object(raw_finalization_registry));
  WeakCell weak_cell = WeakCell::cast(Object(raw_weak_cell));
  DCHECK(!weak_cell.unregister_token().IsUndefined(isolate));
  HeapObject undefined = ReadOnlyRoots(isolate).undefined_value();

  // Remove weak_cell from the linked list of other WeakCells with the same
  // unregister token and remove its unregister token from key_map if necessary
  // without shrinking it. Since shrinking may allocate, it is performed by the
  // caller after looping, or on exception.
  if (weak_cell.key_list_prev().IsUndefined(isolate)) {
    SimpleNumberDictionary key_map =
        SimpleNumberDictionary::cast(finalization_registry.key_map());
    HeapObject unregister_token = weak_cell.unregister_token();
    uint32_t key = Smi::ToInt(unregister_token.GetHash());
    InternalIndex entry = key_map.FindEntry(isolate, key);
    DCHECK(entry.is_found());

    if (weak_cell.key_list_next().IsUndefined(isolate)) {
      // weak_cell is the only one associated with its key; remove the key
      // from the hash table.
      key_map.ClearEntry(entry);
      key_map.ElementRemoved();
    } else {
      // weak_cell is the list head for its key; we need to change the value
      // of the key in the hash table.
      WeakCell next = WeakCell::cast(weak_cell.key_list_next());
      DCHECK_EQ(next.key_list_prev(), weak_cell);
      next.set_key_list_prev(undefined);
      key_map.ValueAtPut(entry, next);
    }
  } else {
    // weak_cell is somewhere in the middle of its key list.
    WeakCell prev = WeakCell::cast(weak_cell.key_list_prev());
    prev.set_key_list_next(weak_cell.key_list_next());
    if (!weak_cell.key_list_next().IsUndefined()) {
      WeakCell next = WeakCell::cast(weak_cell.key_list_next());
      next.set_key_list_prev(weak_cell.key_list_prev());
    }
  }

  // weak_cell is now removed from the unregister token map, so clear its
  // unregister token-related fields.
  weak_cell.set_unregister_token(undefined);
  weak_cell.set_key_list_prev(undefined);
  weak_cell.set_key_list_next(undefined);
}

}  // namespace internal
}  // namespace v8
