// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_JSON_STRINGIFIER_H_
#define V8_JSON_STRINGIFIER_H_

#include "v8.h"
#include "conversions.h"
#include "utils.h"

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
  static const int kInitialPartLength = 32;
  static const int kMaxPartLength = 16 * 1024;
  static const int kPartLengthGrowthFactor = 2;

  enum Result { UNCHANGED, SUCCESS, EXCEPTION };

  void Accumulate();

  void Extend();

  void ChangeEncoding();

  INLINE(void ShrinkCurrentPart());

  template <bool is_ascii, typename Char>
  INLINE(void Append_(Char c));

  template <bool is_ascii, typename Char>
  INLINE(void Append_(const Char* chars));

  INLINE(void Append(uint8_t c)) {
    if (is_ascii_) {
      Append_<true>(c);
    } else {
      Append_<false>(c);
    }
  }

  INLINE(void AppendAscii(const char* chars)) {
    if (is_ascii_) {
      Append_<true>(reinterpret_cast<const uint8_t*>(chars));
    } else {
      Append_<false>(reinterpret_cast<const uint8_t*>(chars));
    }
  }

  MUST_USE_RESULT MaybeHandle<Object> ApplyToJsonFunction(
      Handle<Object> object,
      Handle<Object> key);

  Result SerializeGeneric(Handle<Object> object,
                          Handle<Object> key,
                          bool deferred_comma,
                          bool deferred_key);

  template <typename ResultType, typename Char>
  INLINE(static Handle<String> StringifyString_(Isolate* isolate,
                                                Vector<Char> vector,
                                                Handle<String> result));

  // Entry point to serialize the object.
  INLINE(Result SerializeObject(Handle<Object> obj)) {
    return Serialize_<false>(obj, false, factory_->empty_string());
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
    ASSERT(!deferred_key.is_null());
    return Serialize_<true>(object, deferred_comma, deferred_key);
  }

  template <bool deferred_string_key>
  Result Serialize_(Handle<Object> object, bool comma, Handle<Object> key);

  void SerializeDeferredKey(bool deferred_comma, Handle<Object> deferred_key) {
    if (deferred_comma) Append(',');
    SerializeString(Handle<String>::cast(deferred_key));
    Append(':');
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
  INLINE(static int SerializeStringUnchecked_(const SrcChar* src,
                                              DestChar* dest,
                                              int length));

  template <bool is_ascii, typename Char>
  INLINE(void SerializeString_(Handle<String> string));

  template <typename Char>
  INLINE(static bool DoNotEscape(Char c));

  template <typename Char>
  INLINE(static Vector<const Char> GetCharVector(Handle<String> string));

  Result StackPush(Handle<Object> object);
  void StackPop();

  INLINE(Handle<String> accumulator()) {
    return Handle<String>(String::cast(accumulator_store_->value()), isolate_);
  }

  INLINE(void set_accumulator(Handle<String> string)) {
    return accumulator_store_->set_value(*string);
  }

  Isolate* isolate_;
  Factory* factory_;
  // We use a value wrapper for the string accumulator to keep the
  // (indirect) handle to it in the outermost handle scope.
  Handle<JSValue> accumulator_store_;
  Handle<String> current_part_;
  Handle<String> tojson_string_;
  Handle<JSArray> stack_;
  int current_index_;
  int part_length_;
  bool is_ascii_;
  bool overflowed_;

  static const int kJsonEscapeTableEntrySize = 8;
  static const char* const JsonEscapeTable;
};


// Translation table to escape ASCII characters.
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
    : isolate_(isolate),
      current_index_(0),
      is_ascii_(true),
      overflowed_(false) {
  factory_ = isolate_->factory();
  accumulator_store_ = Handle<JSValue>::cast(
      Object::ToObject(isolate, factory_->empty_string()).ToHandleChecked());
  part_length_ = kInitialPartLength;
  current_part_ = factory_->NewRawOneByteString(part_length_).ToHandleChecked();
  tojson_string_ = factory_->toJSON_string();
  stack_ = factory_->NewJSArray(8);
}


MaybeHandle<Object> BasicJsonStringifier::Stringify(Handle<Object> object) {
  Result result = SerializeObject(object);
  if (result == UNCHANGED) return isolate_->factory()->undefined_value();
  if (result == SUCCESS) {
    ShrinkCurrentPart();
    Accumulate();
    if (overflowed_) {
      return isolate_->Throw<Object>(
          isolate_->factory()->NewInvalidStringLengthError());
    }
    return accumulator();
  }
  ASSERT(result == EXCEPTION);
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
  ASSERT(object->IsFlat());
  if (object->IsOneByteRepresentationUnderneath()) {
    Handle<String> result = isolate->factory()->NewRawOneByteString(
        worst_case_length).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    return StringifyString_<SeqOneByteString>(
        isolate,
        object->GetFlatContent().ToOneByteVector(),
        result);
  } else {
    Handle<String> result = isolate->factory()->NewRawTwoByteString(
        worst_case_length).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    return StringifyString_<SeqTwoByteString>(
        isolate,
        object->GetFlatContent().ToUC16Vector(),
        result);
  }
}


template <typename ResultType, typename Char>
Handle<String> BasicJsonStringifier::StringifyString_(Isolate* isolate,
                                                      Vector<Char> vector,
                                                      Handle<String> result) {
  DisallowHeapAllocation no_gc;
  int final_size = 0;
  ResultType* dest = ResultType::cast(*result);
  dest->Set(final_size++, '\"');
  final_size += SerializeStringUnchecked_(vector.start(),
                                          dest->GetChars() + 1,
                                          vector.length());
  dest->Set(final_size++, '\"');
  return SeqString::Truncate(Handle<SeqString>::cast(result), final_size);
}


template <bool is_ascii, typename Char>
void BasicJsonStringifier::Append_(Char c) {
  if (is_ascii) {
    SeqOneByteString::cast(*current_part_)->SeqOneByteStringSet(
        current_index_++, c);
  } else {
    SeqTwoByteString::cast(*current_part_)->SeqTwoByteStringSet(
        current_index_++, c);
  }
  if (current_index_ == part_length_) Extend();
}


template <bool is_ascii, typename Char>
void BasicJsonStringifier::Append_(const Char* chars) {
  for ( ; *chars != '\0'; chars++) Append_<is_ascii, Char>(*chars);
}


MaybeHandle<Object> BasicJsonStringifier::ApplyToJsonFunction(
    Handle<Object> object, Handle<Object> key) {
  LookupResult lookup(isolate_);
  JSObject::cast(*object)->LookupRealNamedProperty(tojson_string_, &lookup);
  if (!lookup.IsProperty()) return object;
  PropertyAttributes attr;
  Handle<Object> fun;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, fun,
      Object::GetProperty(object, object, &lookup, tojson_string_, &attr),
      Object);
  if (!fun->IsJSFunction()) return object;

  // Call toJSON function.
  if (key->IsSmi()) key = factory_->NumberToString(key);
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
        isolate_->Throw(*factory_->NewTypeError(
            "circular_structure", HandleVector<Object>(NULL, 0)));
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
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeHeapNumber(Handle<HeapNumber>::cast(object));
    case ODDBALL_TYPE:
      switch (Oddball::cast(*object)->kind()) {
        case Oddball::kFalse:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          AppendAscii("false");
          return SUCCESS;
        case Oddball::kTrue:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          AppendAscii("true");
          return SUCCESS;
        case Oddball::kNull:
          if (deferred_string_key) SerializeDeferredKey(comma, key);
          AppendAscii("null");
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
        if (object->IsAccessCheckNeeded()) break;
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
    if (key->IsSmi()) key = factory_->NumberToString(key);
    SerializeDeferredKey(deferred_comma, key);
  }

  Handle<String> result_string = Handle<String>::cast(result);
  // Shrink current part, attach it to the accumulator, also attach the result
  // string to the accumulator, and allocate a new part.
  ShrinkCurrentPart();  // Shrink.
  part_length_ = kInitialPartLength;  // Allocate conservatively.
  Extend();             // Attach current part and allocate new part.
  // Attach result string to the accumulator.
  Handle<String> cons;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, cons,
      factory_->NewConsString(accumulator(), result_string),
      EXCEPTION);
  set_accumulator(cons);
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
    ASSERT(class_name == isolate_->heap()->Boolean_string());
    Object* value = JSValue::cast(*object)->value();
    ASSERT(value->IsBoolean());
    AppendAscii(value->IsTrue() ? "true" : "false");
  }
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeSmi(Smi* object) {
  static const int kBufferSize = 100;
  char chars[kBufferSize];
  Vector<char> buffer(chars, kBufferSize);
  AppendAscii(IntToCString(object->value(), buffer));
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeDouble(
    double number) {
  if (std::isinf(number) || std::isnan(number)) {
    AppendAscii("null");
    return SUCCESS;
  }
  static const int kBufferSize = 100;
  char chars[kBufferSize];
  Vector<char> buffer(chars, kBufferSize);
  AppendAscii(DoubleToCString(number, buffer));
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSArray(
    Handle<JSArray> object) {
  HandleScope handle_scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;
  uint32_t length = 0;
  CHECK(object->length()->ToArrayIndex(&length));
  Append('[');
  switch (object->GetElementsKind()) {
    case FAST_SMI_ELEMENTS: {
      Handle<FixedArray> elements(
          FixedArray::cast(object->elements()), isolate_);
      for (uint32_t i = 0; i < length; i++) {
        if (i > 0) Append(',');
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
        if (i > 0) Append(',');
        SerializeDouble(elements->get_scalar(i));
      }
      break;
    }
    case FAST_ELEMENTS: {
      Handle<FixedArray> elements(
          FixedArray::cast(object->elements()), isolate_);
      for (uint32_t i = 0; i < length; i++) {
        if (i > 0) Append(',');
        Result result =
            SerializeElement(isolate_,
                             Handle<Object>(elements->get(i), isolate_),
                             i);
        if (result == SUCCESS) continue;
        if (result == UNCHANGED) {
          AppendAscii("null");
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
  Append(']');
  StackPop();
  current_part_ = handle_scope.CloseAndEscape(current_part_);
  return SUCCESS;
}


BasicJsonStringifier::Result BasicJsonStringifier::SerializeJSArraySlow(
    Handle<JSArray> object, uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    if (i > 0) Append(',');
    Handle<Object> element;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, element,
        Object::GetElement(isolate_, object, i),
        EXCEPTION);
    if (element->IsUndefined()) {
      AppendAscii("null");
    } else {
      Result result = SerializeElement(isolate_, element, i);
      if (result == SUCCESS) continue;
      if (result == UNCHANGED) {
        AppendAscii("null");
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
  if (object->IsJSGlobalProxy()) {
    object = Handle<JSObject>(
                 JSObject::cast(object->GetPrototype()), isolate_);
    ASSERT(object->IsGlobalObject());
  }

  Append('{');
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
        property = Handle<Object>(
                       object->RawFastPropertyAt(
                           map->instance_descriptors()->GetFieldIndex(i)),
                       isolate_);
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
        JSReceiver::GetKeys(object, JSReceiver::LOCAL_ONLY),
        EXCEPTION);

    for (int i = 0; i < contents->length(); i++) {
      Object* key = contents->get(i);
      Handle<String> key_handle;
      MaybeHandle<Object> maybe_property;
      if (key->IsString()) {
        key_handle = Handle<String>(String::cast(key), isolate_);
        maybe_property = Object::GetPropertyOrElement(object, key_handle);
      } else {
        ASSERT(key->IsNumber());
        key_handle = factory_->NumberToString(Handle<Object>(key, isolate_));
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

  Append('}');
  StackPop();
  current_part_ = handle_scope.CloseAndEscape(current_part_);
  return SUCCESS;
}


void BasicJsonStringifier::ShrinkCurrentPart() {
  ASSERT(current_index_ < part_length_);
  current_part_ = SeqString::Truncate(Handle<SeqString>::cast(current_part_),
                                      current_index_);
}


void BasicJsonStringifier::Accumulate() {
  if (accumulator()->length() + current_part_->length() > String::kMaxLength) {
    // Screw it.  Simply set the flag and carry on.  Throw exception at the end.
    set_accumulator(factory_->empty_string());
    overflowed_ = true;
  } else {
    set_accumulator(factory_->NewConsString(accumulator(),
                                            current_part_).ToHandleChecked());
  }
}


void BasicJsonStringifier::Extend() {
  Accumulate();
  if (part_length_ <= kMaxPartLength / kPartLengthGrowthFactor) {
    part_length_ *= kPartLengthGrowthFactor;
  }
  if (is_ascii_) {
    current_part_ =
        factory_->NewRawOneByteString(part_length_).ToHandleChecked();
  } else {
    current_part_ =
        factory_->NewRawTwoByteString(part_length_).ToHandleChecked();
  }
  ASSERT(!current_part_.is_null());
  current_index_ = 0;
}


void BasicJsonStringifier::ChangeEncoding() {
  ShrinkCurrentPart();
  Accumulate();
  current_part_ =
      factory_->NewRawTwoByteString(part_length_).ToHandleChecked();
  ASSERT(!current_part_.is_null());
  current_index_ = 0;
  is_ascii_ = false;
}


template <typename SrcChar, typename DestChar>
int BasicJsonStringifier::SerializeStringUnchecked_(const SrcChar* src,
                                                    DestChar* dest,
                                                    int length) {
  DestChar* dest_start = dest;

  // Assert that uc16 character is not truncated down to 8 bit.
  // The <uc16, char> version of this method must not be called.
  ASSERT(sizeof(*dest) >= sizeof(*src));

  for (int i = 0; i < length; i++) {
    SrcChar c = src[i];
    if (DoNotEscape(c)) {
      *(dest++) = static_cast<DestChar>(c);
    } else {
      const uint8_t* chars = reinterpret_cast<const uint8_t*>(
          &JsonEscapeTable[c * kJsonEscapeTableEntrySize]);
      while (*chars != '\0') *(dest++) = *(chars++);
    }
  }

  return static_cast<int>(dest - dest_start);
}


template <bool is_ascii, typename Char>
void BasicJsonStringifier::SerializeString_(Handle<String> string) {
  int length = string->length();
  Append_<is_ascii, char>('"');
  // We make a rough estimate to find out if the current string can be
  // serialized without allocating a new string part. The worst case length of
  // an escaped character is 6.  Shifting the remainin string length right by 3
  // is a more pessimistic estimate, but faster to calculate.

  if (((part_length_ - current_index_) >> 3) > length) {
    DisallowHeapAllocation no_gc;
    Vector<const Char> vector = GetCharVector<Char>(string);
    if (is_ascii) {
      current_index_ += SerializeStringUnchecked_(
          vector.start(),
          SeqOneByteString::cast(*current_part_)->GetChars() + current_index_,
          length);
    } else {
      current_index_ += SerializeStringUnchecked_(
          vector.start(),
          SeqTwoByteString::cast(*current_part_)->GetChars() + current_index_,
          length);
    }
  } else {
    String* string_location = NULL;
    Vector<const Char> vector(NULL, 0);
    for (int i = 0; i < length; i++) {
      // If GC moved the string, we need to refresh the vector.
      if (*string != string_location) {
        DisallowHeapAllocation no_gc;
        // This does not actually prevent the string from being relocated later.
        vector = GetCharVector<Char>(string);
        string_location = *string;
      }
      Char c = vector[i];
      if (DoNotEscape(c)) {
        Append_<is_ascii, Char>(c);
      } else {
        Append_<is_ascii, uint8_t>(reinterpret_cast<const uint8_t*>(
            &JsonEscapeTable[c * kJsonEscapeTableEntrySize]));
      }
    }
  }

  Append_<is_ascii, uint8_t>('"');
}


template <>
bool BasicJsonStringifier::DoNotEscape(uint8_t c) {
  return c >= '#' && c <= '~' && c != '\\';
}


template <>
bool BasicJsonStringifier::DoNotEscape(uint16_t c) {
  return c >= '#' && c != '\\' && c != 0x7f;
}


template <>
Vector<const uint8_t> BasicJsonStringifier::GetCharVector(
    Handle<String> string) {
  String::FlatContent flat = string->GetFlatContent();
  ASSERT(flat.IsAscii());
  return flat.ToOneByteVector();
}


template <>
Vector<const uc16> BasicJsonStringifier::GetCharVector(Handle<String> string) {
  String::FlatContent flat = string->GetFlatContent();
  ASSERT(flat.IsTwoByte());
  return flat.ToUC16Vector();
}


void BasicJsonStringifier::SerializeString(Handle<String> object) {
  object = String::Flatten(object);
  if (is_ascii_) {
    if (object->IsOneByteRepresentationUnderneath()) {
      SerializeString_<true, uint8_t>(object);
    } else {
      ChangeEncoding();
      SerializeString(object);
    }
  } else {
    if (object->IsOneByteRepresentationUnderneath()) {
      SerializeString_<false, uint8_t>(object);
    } else {
      SerializeString_<false, uc16>(object);
    }
  }
}

} }  // namespace v8::internal

#endif  // V8_JSON_STRINGIFIER_H_
