// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_STRINGIFIER_H_
#define V8_JSON_STRINGIFIER_H_

#include "src/v8.h"

#include "src/conversions.h"
#include "src/string-builder.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class BasicJsonStringifier BASE_EMBEDDED {
 public:
  explicit BasicJsonStringifier(Isolate* isolate);

  MUST_USE_RESULT MaybeHandle<Object> Stringify(Handle<Object> object);

  MUST_USE_RESULT INLINE(static MaybeHandle<Object> StringifyString(
      Isolate* isolate,
      Handle<String> object));

 private:
  enum Result { UNCHANGED, SUCCESS, EXCEPTION };

  MUST_USE_RESULT MaybeHandle<Object> ApplyToJsonFunction(
      Handle<Object> object,
      Handle<Object> key);

  Result SerializeGeneric(Handle<Object> object,
                          Handle<Object> key,
                          bool deferred_comma,
                          bool deferred_key);

  // Entry point to serialize the object.
  INLINE(Result SerializeObject(Handle<Object> obj)) {
    return Serialize_<false>(obj, false, factory()->empty_string());
  }

  // Serialize an array element.
  // The index may serve as argument for the toJSON function.
  INLINE(Result SerializeElement(Isolate* isolate,
                                 Handle<Object> object,
                                 int i)) {
    return Serialize_<false>(object,
                             false,
                             Handle<Object>(Smi::FromInt(i), isolate));
  }

  // Serialize a object property.
  // The key may or may not be serialized depending on the property.
  // The key may also serve as argument for the toJSON function.
  INLINE(Result SerializeProperty(Handle<Object> object,
                                  bool deferred_comma,
                                  Handle<String> deferred_key)) {
    DCHECK(!deferred_key.is_null());
    return Serialize_<true>(object, deferred_comma, deferred_key);
  }

  template <bool deferred_string_key>
  Result Serialize_(Handle<Object> object, bool comma, Handle<Object> key);

  void SerializeDeferredKey(bool deferred_comma, Handle<Object> deferred_key) {
    if (deferred_comma) builder_.AppendCharacter(',');
    SerializeString(Handle<String>::cast(deferred_key));
    builder_.AppendCharacter(':');
  }

  Result SerializeSmi(Smi* object);

  Result SerializeDouble(double number);
  INLINE(Result SerializeHeapNumber(Handle<HeapNumber> object)) {
    return SerializeDouble(object->value());
  }

  Result SerializeJSValue(Handle<JSValue> object);

  INLINE(Result SerializeJSArray(Handle<JSArray> object));
  INLINE(Result SerializeJSObject(Handle<JSObject> object));

  Result SerializeJSArraySlow(Handle<JSArray> object, uint32_t length);

  void SerializeString(Handle<String> object);

  template <typename SrcChar, typename DestChar>
  INLINE(static void SerializeStringUnchecked_(
      Vector<const SrcChar> src,
      IncrementalStringBuilder::NoExtend<DestChar>* dest));

  template <typename SrcChar, typename DestChar>
  INLINE(void SerializeString_(Handle<String> string));

  template <typename Char>
  INLINE(static bool DoNotEscape(Char c));

  Result StackPush(Handle<Object> object);
  void StackPop();

  Factory* factory() { return isolate_->factory(); }

  Isolate* isolate_;
  IncrementalStringBuilder builder_;
  Handle<String> tojson_string_;
  Handle<JSArray> stack_;

  static const int kJsonEscapeTableEntrySize = 8;
  static const char* const JsonEscapeTable;
};


// Translation table to escape Latin1 characters.
// Table entries start at a multiple of 8 and are null-terminated.
const char* const BasicJsonStringifier::JsonEscapeTable =
    "\\u0000\0 \\u0001\0 \\u0002\0 \\u0003\0 "
    "\\u0004\0 \\u0005\0 \\u0006\0 \\u0007\0 "
    "\\b\0     \\t\0     \\n\0     \\u000b\0 "
    "\\f\0     \\r\0     \\u000e\0 \\u000f\0 "
    "\\u0010\0 \\u0011\0 \\u0012\0 \\u0013\0 "
    "\\u0014\0 \\u0015\0 \\u0016\0 \\u0017\0 "
    "\\u0018\0 \\u0019\0 \\u001a\0 \\u001b\0 "
    "\\u001c\0 \\u001d\0 \\u001e\0 \\u001f\0 "
    " \0      !\0      \\\"\0     #\0      "
    "$\0      %\0      &\0      '\0      "
    "(\0      )\0      *\0      +\0      "
    ",\0      -\0      .\0      /\0      "
    "0\0      1\0      2\0      3\0      "
    "4\0      5\0      6\0      7\0      "
    "8\0      9\0      :\0      ;\0      "
    "<\0      =\0      >\0      ?\0      "
    "@\0      A\0      B\0      C\0      "
    "D\0      E\0      F\0      G\0      "
    "H\0      I\0      J\0      K\0      "
    "L\0      M\0      N\0      O\0      "
    "P\0      Q\0      R\0      S\0      "
    "T\0      U\0      V\0      W\0      "
    "X\0      Y\0      Z\0      [\0      "
    "\\\\\0     ]\0      ^\0      _\0      "
    "`\0      a\0      b\0      c\0      "
    "d\0      e\0      f\0      g\0      "
    "h\0      i\0      j\0      k\0      "
    "l\0      m\0      n\0      o\0      "
    "p\0      q\0      r\0      s\0      "
    "t\0      u\0      v\0      w\0      "
    "x\0      y\0      z\0      {\0      "
    "|\0      }\0      ~\0      \177\0      "
    "\200\0      \201\0      \202\0      \203\0      "
    "\204\0      \205\0      \206\0      \207\0      "
    "\210\0      \211\0      \212\0      \213\0      "
    "\214\0      \215\0      \216\0      \217\0      "
    "\220\0      \221\0      \222\0      \223\0      "
    "\224\0      \225\0      \226\0      \227\0      "
    "\230\0      \231\0      \232\0      \233\0      "
    "\234\0      \235\0      \236\0      \237\0      "
    "\240\0      \241\0      \242\0      \243\0      "
    "\244\0      \245\0      \246\0      \247\0      "
    "\250\0      \251\0      \252\0      \253\0      "
    "\254\0      \255\0      \256\0      \257\0      "
    "\260\0      \261\0      \262\0      \263\0      "
    "\264\0      \265\0      \266\0      \267\0      "
    "\270\0      \271\0      \272\0      \273\0      "
    "\274\0      \275\0      \276\0      \277\0      "
    "\300\0      \301\0      \302\0      \303\0      "
    "\304\0      \305\0      \306\0      \307\0      "
    "\310\0      \311\0      \312\0      \313\0      "
    "\314\0      \315\0      \316\0      \317\0      "
    "\320\0      \321\0      \322\0      \323\0      "
    "\324\0      \325\0      \326\0      \327\0      "
    "\330\0      \331\0      \332\0      \333\0      "
    "\334\0      \335\0      \336\0      \337\0      "
    "\340\0      \341\0      \342\0      \343\0      "
    "\344\0      \345\0      \346\0      \347\0      "
    "\350\0      \351\0      \352\0      \353\0      "
    "\354\0      \355\0      \356\0      \357\0      "
    "\360\0      \361\0      \362\0      \363\0      "
    "\364\0      \365\0      \366\0      \367\0      "
    "\370\0      \371\0      \372\0      \373\0      "
    "\374\0      \375\0      \376\0      \377\0      ";


BasicJsonStringifier::BasicJsonStringifier(Isolate* isolate)
    : isolate_(isolate), builder_(isolate) {
  tojson_string_ = factory()->toJSON_string();
  stack_ = factory()->NewJSArray(8);
}


MaybeHandle<Object> BasicJsonStringifier::Stringify(Handle<Object> object) {
  Result result = SerializeObject(object);
  if (result == UNCHANGED) return factory()->undefined_value();
  if (result == SUCCESS) return builder_.Finish();
  DCHECK(result == EXCEPTION);
  return MaybeHandle<Object>();
}


MaybeHandle<Object> BasicJsonStringifier::StringifyString(
    Isolate* isolate,  Handle<String> object) {
  static const int kJsonQuoteWorstCaseBlowup = 6;
  static const int kSpaceForQuotes = 2;
  int worst_case_length =
      object->length() * kJsonQuoteWorstCaseBlowup + kSpaceForQuotes;

  if (worst_case_length > 32 * KB) {  // Slow path if too large.
    BasicJsonStringifier stringifier(isolate);
    return stringifier.Stringify(object);
  }

  object = String::Flatten(object);
  DCHECK(object->IsFlat());
  Handle<SeqString> result;
  if (object->IsOneByteRepresentationUnderneath()) {
    result = isolate->factory()
                 ->NewRawOneByteString(worst_case_length)
                 .ToHandleChecked();
    IncrementalStringBuilder::NoExtendString<uint8_t> no_extend(
        result, worst_case_length);
    no_extend.Append('\"');
    SerializeStringUnchecked_(object->GetFlatContent().ToOneByteVector(),
                              &no_extend);
    no_extend.Append('\"');
  } else {
    result = isolate->factory()
                 ->NewRawTwoByteString(worst_case_length)
                 .ToHandleChecked();
    IncrementalStringBuilder::NoExtendString<uc16> no_extend(result,
                                                             worst_case_length);
    no_extend.Append('\"');
    SerializeStringUnchecked_(object->GetFlatContent().ToUC16Vector(),
                              &no_extend);
    no_extend.Append('\"');
  }
  return result;
}


MaybeHandle<Object> BasicJsonStringifier::ApplyToJsonFunction(
    Handle<Object> object, Handle<Object> key) {
  LookupIterator it(object, tojson_string_,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Handle<Object> fun;
  ASSIGN_RETURN_ON_EXCEPTION(isolate_, fun, Object::GetProperty(&it), Object);
  if (!fun->IsJSFunction()) return object;

  // Call toJSON function.
  if (key->IsSmi()) key = factory()->NumberToString(key);
  Handle<Object> argv[] = { key };
  HandleScope scope(isolate_);
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, object,
      Execution::Call(isolate_, fun, object, 1, argv),
      Object);
  return scope.CloseAndEscape(object);
}


BasicJsonStringifier::Result BasicJsonStringifier::StackPush(
    Handle<Object> object) {
  StackLimitCheck check(isolate_);
  if (check.HasOverflowed()) {
    isolate_->StackOverflow();
    return EXCEPTION;
  }

  int length = Smi::cast(stack_->length())->value();
  {
    DisallowHeapAllocation no_allocation;
    FixedArray* elements = FixedArray::cast(stack_->elements());
    for (int i = 0; i < length; i++) {
      if (elements->get(i) == *object) {
        AllowHeapAllocation allow_to_return_error;
        Handle<Object> error;
        MaybeHandle<Object> maybe_error = factory()->NewTypeError(
            "circular_structure", HandleVector<Object>(NULL, 0));
        if (maybe_error.ToHandle(&error)) isolate_->Throw(*error);
        return EXCEPTION;
      }
    }
  }
  JSArray::EnsureSize(stack_, length + 1);
  FixedArray::cast(stack_->elements())->set(length, *object);
  stack_->set_length(Smi::FromInt(length + 1));
  return SUCCESS;
}


void BasicJsonStringifier::StackPop() {
  int length = Smi::cast(stack_->length())->value();
  stack_->set_length(Smi::FromInt(length - 1));
}


template <bool deferred_string_key>
BasicJsonStringifier::Result BasicJsonStringifier::Serialize_(
    Handle<Object> object, bool comma, Handle<Object> key) {
  if (object->IsJSObject()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, object,
        ApplyToJsonFunction(object, key),
        EXCEPTION);
  }

  if (object->IsSmi()) {
    if (deferred_string_key) SerializeDeferredKey(comma, key);
    return SerializeSmi(Smi::cast(*object));
  }

  switch (HeapObject::cast(*object)->map()->instance_type()) {
    case HEAP_NUMBER_TYPE:
    case MUTABLE_HEAP_NUMBER_TYPE:
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeHeapNumber(Handle<HeapNumber>::cast(object));
    case ODDBALL_TYPE:
      switch (Oddball::cast(*object)->kind()) {
        case Oddball::kFalse:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          builder_.AppendCString("false");
          return SUCCESS;
        case Oddball::kTrue:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          builder_.AppendCString("true");
          return SUCCESS;
        case Oddball::kNull:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          builder_.AppendCString("null");
          return SUCCESS;
        default:
          return UNCHANGED;
      }
    case JS_ARRAY_TYPE:
      if (object->IsAccessCheckNeeded()) break;
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeJSArray(Handle<JSArray>::cast(object));
    case JS_VALUE_TYPE:
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeJSValue(Handle<JSValue>::cast(object));
    case JS_FUNCTION_TYPE:
      return UNCHANGED;
    default:
      if (object->IsString()) {
        if (deferred_string_key) SerializeDeferredKey(comma, key);
        SerializeString(Handle<String>::cast(object));
        return SUCCESS;
      } else if (object->IsJSObject()) {
        // Go to slow path for global proxy and objects requiring access checks.
        if (object->IsAccessCheckNeeded() || object->IsJSGlobalProxy()) break;
        if (deferred_string_key) SerializeDeferredKey(comma, key);
        return SerializeJSObject(Handle<JSObject>::cast(object));
      }
  }

  return SerializeGeneric(object, key, comma, deferred_string_key);
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeGeneric(
    Handle<Object> object,
    Handle<Object> key,
    bool deferred_comma,
    bool deferred_key) {
  Handle<JSObject> builtins(isolate_->native_context()->builtins(), isolate_);
  Handle<JSFunction> builtin = Handle<JSFunction>::cast(Object::GetProperty(
      isolate_, builtins, "JSONSerializeAdapter").ToHandleChecked());

  Handle<Object> argv[] = { key, object };
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, result,
      Execution::Call(isolate_, builtin, object, 2, argv),
      EXCEPTION);
  if (result->IsUndefined()) return UNCHANGED;
  if (deferred_key) {
    if (key->IsSmi()) key = factory()->NumberToString(key);
    SerializeDeferredKey(deferred_comma, key);
  }

  builder_.AppendString(Handle<String>::cast(result));
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSValue(
    Handle<JSValue> object) {
  String* class_name = object->class_name();
  if (class_name == isolate_->heap()->String_string()) {
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, value, Execution::ToString(isolate_, object), EXCEPTION);
    SerializeString(Handle<String>::cast(value));
  } else if (class_name == isolate_->heap()->Number_string()) {
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, value, Execution::ToNumber(isolate_, object), EXCEPTION);
    if (value->IsSmi()) return SerializeSmi(Smi::cast(*value));
    SerializeHeapNumber(Handle<HeapNumber>::cast(value));
  } else {
    DCHECK(class_name == isolate_->heap()->Boolean_string());
    Object* value = JSValue::cast(*object)->value();
    DCHECK(value->IsBoolean());
    builder_.AppendCString(value->IsTrue() ? "true" : "false");
  }
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeSmi(Smi* object) {
  static const int kBufferSize = 100;
  char chars[kBufferSize];
  Vector<char> buffer(chars, kBufferSize);
  builder_.AppendCString(IntToCString(object->value(), buffer));
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeDouble(
    double number) {
  if (std::isinf(number) || std::isnan(number)) {
    builder_.AppendCString("null");
    return SUCCESS;
  }
  static const int kBufferSize = 100;
  char chars[kBufferSize];
  Vector<char> buffer(chars, kBufferSize);
  builder_.AppendCString(DoubleToCString(number, buffer));
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSArray(
    Handle<JSArray> object) {
  HandleScope handle_scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;
  uint32_t length = 0;
  CHECK(object->length()->ToArrayIndex(&length));
  builder_.AppendCharacter('[');
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS: {
      Handle<FixedArray> elements(
          FixedArray::cast(object->elements()), isolate_);
      for (uint32_t i = 0; i < length; i++) {
        if (i > 0) builder_.AppendCharacter(',');
        SerializeSmi(Smi::cast(elements->get(i)));
      }
      break;
    }
    case FAST_DOUBLE_ELEMENTS: {
      // Empty array is FixedArray but not FixedDoubleArray.
      if (length == 0) break;
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(object->elements()), isolate_);
      for (uint32_t i = 0; i < length; i++) {
        if (i > 0) builder_.AppendCharacter(',');
        SerializeDouble(elements->get_scalar(i));
      }
      break;
    }
    case FAST_ELEMENTS: {
      Handle<FixedArray> elements(
          FixedArray::cast(object->elements()), isolate_);
      for (uint32_t i = 0; i < length; i++) {
        if (i > 0) builder_.AppendCharacter(',');
        Result result =
            SerializeElement(isolate_,
                             Handle<Object>(elements->get(i), isolate_),
                             i);
        if (result == SUCCESS) continue;
        if (result == UNCHANGED) {
          builder_.AppendCString("null");
        } else {
          return result;
        }
      }
      break;
    }
    // TODO(yangguo):  The FAST_HOLEY_* cases could be handled in a faster way.
    // They resemble the non-holey cases except that a prototype chain lookup
    // is necessary for holes.
    default: {
      Result result = SerializeJSArraySlow(object, length);
      if (result != SUCCESS) return result;
      break;
    }
  }
  builder_.AppendCharacter(']');
  StackPop();
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSArraySlow(
    Handle<JSArray> object, uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    if (i > 0) builder_.AppendCharacter(',');
    Handle<Object> element;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, element,
        Object::GetElement(isolate_, object, i),
        EXCEPTION);
    if (element->IsUndefined()) {
      builder_.AppendCString("null");
    } else {
      Result result = SerializeElement(isolate_, element, i);
      if (result == SUCCESS) continue;
      if (result == UNCHANGED) {
        builder_.AppendCString("null");
      } else {
        return result;
      }
    }
  }
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSObject(
    Handle<JSObject> object) {
  HandleScope handle_scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;
  DCHECK(!object->IsJSGlobalProxy() && !object->IsGlobalObject());

  builder_.AppendCharacter('{');
  bool comma = false;

  if (object->HasFastProperties() &&
      !object->HasIndexedInterceptor() &&
      !object->HasNamedInterceptor() &&
      object->elements()->length() == 0) {
    Handle<Map> map(object->map());
    for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
      Handle<Name> name(map->instance_descriptors()->GetKey(i), isolate_);
      // TODO(rossberg): Should this throw?
      if (!name->IsString()) continue;
      Handle<String> key = Handle<String>::cast(name);
      PropertyDetails details = map->instance_descriptors()->GetDetails(i);
      if (details.IsDontEnum()) continue;
      Handle<Object> property;
      if (details.type() == FIELD && *map == object->map()) {
        FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
        Isolate* isolate = object->GetIsolate();
        if (object->IsUnboxedDoubleField(field_index)) {
          double value = object->RawFastDoublePropertyAt(field_index);
          property = isolate->factory()->NewHeapNumber(value);

        } else {
          property = handle(object->RawFastPropertyAt(field_index), isolate);
        }
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate_, property,
            Object::GetPropertyOrElement(object, key),
            EXCEPTION);
      }
      Result result = SerializeProperty(property, comma, key);
      if (!comma && result == SUCCESS) comma = true;
      if (result == EXCEPTION) return result;
    }
  } else {
    Handle<FixedArray> contents;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, contents,
        JSReceiver::GetKeys(object, JSReceiver::OWN_ONLY),
        EXCEPTION);

    for (int i = 0; i < contents->length(); i++) {
      Object* key = contents->get(i);
      Handle<String> key_handle;
      MaybeHandle<Object> maybe_property;
      if (key->IsString()) {
        key_handle = Handle<String>(String::cast(key), isolate_);
        maybe_property = Object::GetPropertyOrElement(object, key_handle);
      } else {
        DCHECK(key->IsNumber());
        key_handle = factory()->NumberToString(Handle<Object>(key, isolate_));
        uint32_t index;
        if (key->IsSmi()) {
          maybe_property = Object::GetElement(
              isolate_, object, Smi::cast(key)->value());
        } else if (key_handle->AsArrayIndex(&index)) {
          maybe_property = Object::GetElement(isolate_, object, index);
        } else {
          maybe_property = Object::GetPropertyOrElement(object, key_handle);
        }
      }
      Handle<Object> property;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate_, property, maybe_property, EXCEPTION);
      Result result = SerializeProperty(property, comma, key_handle);
      if (!comma && result == SUCCESS) comma = true;
      if (result == EXCEPTION) return result;
    }
  }

  builder_.AppendCharacter('}');
  StackPop();
  return SUCCESS;
}


template <typename SrcChar, typename DestChar>
void BasicJsonStringifier::SerializeStringUnchecked_(
    Vector<const SrcChar> src,
    IncrementalStringBuilder::NoExtend<DestChar>* dest) {
  // Assert that uc16 character is not truncated down to 8 bit.
  // The <uc16, char> version of this method must not be called.
  DCHECK(sizeof(DestChar) >= sizeof(SrcChar));

  for (int i = 0; i < src.length(); i++) {
    SrcChar c = src[i];
    if (DoNotEscape(c)) {
      dest->Append(c);
    } else {
      dest->AppendCString(&JsonEscapeTable[c * kJsonEscapeTableEntrySize]);
    }
  }
}


template <typename SrcChar, typename DestChar>
void BasicJsonStringifier::SerializeString_(Handle<String> string) {
  int length = string->length();
  builder_.Append<uint8_t, DestChar>('"');
  // We make a rough estimate to find out if the current string can be
  // serialized without allocating a new string part. The worst case length of
  // an escaped character is 6.  Shifting the remainin string length right by 3
  // is a more pessimistic estimate, but faster to calculate.
  int worst_case_length = length << 3;
  if (builder_.CurrentPartCanFit(worst_case_length)) {
    DisallowHeapAllocation no_gc;
    Vector<const SrcChar> vector = string->GetCharVector<SrcChar>();
    IncrementalStringBuilder::NoExtendBuilder<DestChar> no_extend(
        &builder_, worst_case_length);
    SerializeStringUnchecked_(vector, &no_extend);
  } else {
    FlatStringReader reader(isolate_, string);
    for (int i = 0; i < reader.length(); i++) {
      SrcChar c = reader.Get<SrcChar>(i);
      if (DoNotEscape(c)) {
        builder_.Append<SrcChar, DestChar>(c);
      } else {
        builder_.AppendCString(&JsonEscapeTable[c * kJsonEscapeTableEntrySize]);
      }
    }
  }

  builder_.Append<uint8_t, DestChar>('"');
}


template <>
bool BasicJsonStringifier::DoNotEscape(uint8_t c) {
  return c >= '#' && c <= '~' && c != '\\';
}


template <>
bool BasicJsonStringifier::DoNotEscape(uint16_t c) {
  return c >= '#' && c != '\\' && c != 0x7f;
}


void BasicJsonStringifier::SerializeString(Handle<String> object) {
  object = String::Flatten(object);
  if (builder_.CurrentEncoding() == String::ONE_BYTE_ENCODING) {
    if (object->IsOneByteRepresentationUnderneath()) {
      SerializeString_<uint8_t, uint8_t>(object);
    } else {
      builder_.ChangeEncoding();
      SerializeString(object);
    }
  } else {
    if (object->IsOneByteRepresentationUnderneath()) {
      SerializeString_<uint8_t, uc16>(object);
    } else {
      SerializeString_<uc16, uc16>(object);
    }
  }
}

} }  // namespace v8::internal

#endif  // V8_JSON_STRINGIFIER_H_
