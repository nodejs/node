// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/json-stringifier.h"

#include "src/conversions.h"
#include "src/lookup.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Translation table to escape Latin1 characters.
// Table entries start at a multiple of 8 and are null-terminated.
const char* const JsonStringifier::JsonEscapeTable =
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

JsonStringifier::JsonStringifier(Isolate* isolate)
    : isolate_(isolate), builder_(isolate), gap_(nullptr), indent_(0) {
  tojson_string_ = factory()->toJSON_string();
  stack_ = factory()->NewJSArray(8);
}

MaybeHandle<Object> JsonStringifier::Stringify(Handle<Object> object,
                                               Handle<Object> replacer,
                                               Handle<Object> gap) {
  if (!InitializeReplacer(replacer)) return MaybeHandle<Object>();
  if (!gap->IsUndefined(isolate_) && !InitializeGap(gap)) {
    return MaybeHandle<Object>();
  }
  Result result = SerializeObject(object);
  if (result == UNCHANGED) return factory()->undefined_value();
  if (result == SUCCESS) return builder_.Finish();
  DCHECK(result == EXCEPTION);
  return MaybeHandle<Object>();
}

bool IsInList(Handle<String> key, List<Handle<String> >* list) {
  // TODO(yangguo): This is O(n^2) for n properties in the list. Deal with this
  // if this becomes an issue.
  for (const Handle<String>& existing : *list) {
    if (String::Equals(existing, key)) return true;
  }
  return false;
}

bool JsonStringifier::InitializeReplacer(Handle<Object> replacer) {
  DCHECK(property_list_.is_null());
  DCHECK(replacer_function_.is_null());
  Maybe<bool> is_array = Object::IsArray(replacer);
  if (is_array.IsNothing()) return false;
  if (is_array.FromJust()) {
    HandleScope handle_scope(isolate_);
    List<Handle<String> > list;
    Handle<Object> length_obj;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, length_obj,
        Object::GetLengthFromArrayLike(isolate_, replacer), false);
    uint32_t length;
    if (!length_obj->ToUint32(&length)) length = kMaxUInt32;
    for (uint32_t i = 0; i < length; i++) {
      Handle<Object> element;
      Handle<String> key;
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(
          isolate_, element, Object::GetElement(isolate_, replacer, i), false);
      if (element->IsNumber() || element->IsString()) {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate_, key, Object::ToString(isolate_, element), false);
      } else if (element->IsJSValue()) {
        Handle<Object> value(Handle<JSValue>::cast(element)->value(), isolate_);
        if (value->IsNumber() || value->IsString()) {
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              isolate_, key, Object::ToString(isolate_, element), false);
        }
      }
      if (key.is_null()) continue;
      if (!IsInList(key, &list)) list.Add(key);
    }
    property_list_ = factory()->NewUninitializedFixedArray(list.length());
    for (int i = 0; i < list.length(); i++) {
      property_list_->set(i, *list[i]);
    }
    property_list_ = handle_scope.CloseAndEscape(property_list_);
  } else if (replacer->IsCallable()) {
    replacer_function_ = Handle<JSReceiver>::cast(replacer);
  }
  return true;
}

bool JsonStringifier::InitializeGap(Handle<Object> gap) {
  DCHECK_NULL(gap_);
  HandleScope scope(isolate_);
  if (gap->IsJSValue()) {
    Handle<Object> value(Handle<JSValue>::cast(gap)->value(), isolate_);
    if (value->IsString()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate_, gap,
                                       Object::ToString(isolate_, gap), false);
    } else if (value->IsNumber()) {
      ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate_, gap, Object::ToNumber(gap),
                                       false);
    }
  }

  if (gap->IsString()) {
    Handle<String> gap_string = Handle<String>::cast(gap);
    if (gap_string->length() > 0) {
      int gap_length = std::min(gap_string->length(), 10);
      gap_ = NewArray<uc16>(gap_length + 1);
      String::WriteToFlat(*gap_string, gap_, 0, gap_length);
      for (int i = 0; i < gap_length; i++) {
        if (gap_[i] > String::kMaxOneByteCharCode) {
          builder_.ChangeEncoding();
          break;
        }
      }
      gap_[gap_length] = '\0';
    }
  } else if (gap->IsNumber()) {
    int num_value = DoubleToInt32(gap->Number());
    if (num_value > 0) {
      int gap_length = std::min(num_value, 10);
      gap_ = NewArray<uc16>(gap_length + 1);
      for (int i = 0; i < gap_length; i++) gap_[i] = ' ';
      gap_[gap_length] = '\0';
    }
  }
  return true;
}

MaybeHandle<Object> JsonStringifier::ApplyToJsonFunction(Handle<Object> object,
                                                         Handle<Object> key) {
  HandleScope scope(isolate_);
  LookupIterator it(object, tojson_string_,
                    LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR);
  Handle<Object> fun;
  ASSIGN_RETURN_ON_EXCEPTION(isolate_, fun, Object::GetProperty(&it), Object);
  if (!fun->IsCallable()) return object;

  // Call toJSON function.
  if (key->IsSmi()) key = factory()->NumberToString(key);
  Handle<Object> argv[] = {key};
  ASSIGN_RETURN_ON_EXCEPTION(isolate_, object,
                             Execution::Call(isolate_, fun, object, 1, argv),
                             Object);
  return scope.CloseAndEscape(object);
}

MaybeHandle<Object> JsonStringifier::ApplyReplacerFunction(
    Handle<Object> value, Handle<Object> key, Handle<Object> initial_holder) {
  HandleScope scope(isolate_);
  if (key->IsSmi()) key = factory()->NumberToString(key);
  Handle<Object> argv[] = {key, value};
  Handle<JSReceiver> holder = CurrentHolder(value, initial_holder);
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, value,
      Execution::Call(isolate_, replacer_function_, holder, 2, argv), Object);
  return scope.CloseAndEscape(value);
}

Handle<JSReceiver> JsonStringifier::CurrentHolder(
    Handle<Object> value, Handle<Object> initial_holder) {
  int length = Smi::cast(stack_->length())->value();
  if (length == 0) {
    Handle<JSObject> holder =
        factory()->NewJSObject(isolate_->object_function());
    JSObject::AddProperty(holder, factory()->empty_string(), initial_holder,
                          NONE);
    return holder;
  } else {
    FixedArray* elements = FixedArray::cast(stack_->elements());
    return Handle<JSReceiver>(JSReceiver::cast(elements->get(length - 1)),
                              isolate_);
  }
}

JsonStringifier::Result JsonStringifier::StackPush(Handle<Object> object) {
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
        Handle<Object> error =
            factory()->NewTypeError(MessageTemplate::kCircularStructure);
        isolate_->Throw(*error);
        return EXCEPTION;
      }
    }
  }
  JSArray::SetLength(stack_, length + 1);
  FixedArray::cast(stack_->elements())->set(length, *object);
  return SUCCESS;
}

void JsonStringifier::StackPop() {
  int length = Smi::cast(stack_->length())->value();
  stack_->set_length(Smi::FromInt(length - 1));
}

template <bool deferred_string_key>
JsonStringifier::Result JsonStringifier::Serialize_(Handle<Object> object,
                                                    bool comma,
                                                    Handle<Object> key) {
  StackLimitCheck interrupt_check(isolate_);
  Handle<Object> initial_value = object;
  if (interrupt_check.InterruptRequested() &&
      isolate_->stack_guard()->HandleInterrupts()->IsException(isolate_)) {
    return EXCEPTION;
  }
  if (object->IsJSReceiver()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, object, ApplyToJsonFunction(object, key), EXCEPTION);
  }
  if (!replacer_function_.is_null()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, object, ApplyReplacerFunction(object, key, initial_value),
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
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeJSArray(Handle<JSArray>::cast(object));
    case JS_VALUE_TYPE:
      if (deferred_string_key) SerializeDeferredKey(comma, key);
      return SerializeJSValue(Handle<JSValue>::cast(object));
    case SIMD128_VALUE_TYPE:
    case SYMBOL_TYPE:
      return UNCHANGED;
    default:
      if (object->IsString()) {
        if (deferred_string_key) SerializeDeferredKey(comma, key);
        SerializeString(Handle<String>::cast(object));
        return SUCCESS;
      } else {
        DCHECK(object->IsJSReceiver());
        if (object->IsCallable()) return UNCHANGED;
        // Go to slow path for global proxy and objects requiring access checks.
        if (deferred_string_key) SerializeDeferredKey(comma, key);
        if (object->IsJSProxy()) {
          return SerializeJSProxy(Handle<JSProxy>::cast(object));
        }
        return SerializeJSObject(Handle<JSObject>::cast(object));
      }
  }

  UNREACHABLE();
  return UNCHANGED;
}

JsonStringifier::Result JsonStringifier::SerializeJSValue(
    Handle<JSValue> object) {
  String* class_name = object->class_name();
  if (class_name == isolate_->heap()->String_string()) {
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, value, Object::ToString(isolate_, object), EXCEPTION);
    SerializeString(Handle<String>::cast(value));
  } else if (class_name == isolate_->heap()->Number_string()) {
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate_, value, Object::ToNumber(object),
                                     EXCEPTION);
    if (value->IsSmi()) return SerializeSmi(Smi::cast(*value));
    SerializeHeapNumber(Handle<HeapNumber>::cast(value));
  } else if (class_name == isolate_->heap()->Boolean_string()) {
    Object* value = JSValue::cast(*object)->value();
    DCHECK(value->IsBoolean());
    builder_.AppendCString(value->IsTrue(isolate_) ? "true" : "false");
  } else {
    // ES6 24.3.2.1 step 10.c, serialize as an ordinary JSObject.
    return SerializeJSObject(object);
  }
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeSmi(Smi* object) {
  static const int kBufferSize = 100;
  char chars[kBufferSize];
  Vector<char> buffer(chars, kBufferSize);
  builder_.AppendCString(IntToCString(object->value(), buffer));
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeDouble(double number) {
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

JsonStringifier::Result JsonStringifier::SerializeJSArray(
    Handle<JSArray> object) {
  HandleScope handle_scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;
  uint32_t length = 0;
  CHECK(object->length()->ToArrayLength(&length));
  DCHECK(!object->IsAccessCheckNeeded());
  builder_.AppendCharacter('[');
  Indent();
  uint32_t i = 0;
  if (replacer_function_.is_null()) {
    switch (object->GetElementsKind()) {
      case FAST_SMI_ELEMENTS: {
        Handle<FixedArray> elements(FixedArray::cast(object->elements()),
                                    isolate_);
        StackLimitCheck interrupt_check(isolate_);
        while (i < length) {
          if (interrupt_check.InterruptRequested() &&
              isolate_->stack_guard()->HandleInterrupts()->IsException(
                  isolate_)) {
            return EXCEPTION;
          }
          Separator(i == 0);
          SerializeSmi(Smi::cast(elements->get(i)));
          i++;
        }
        break;
      }
      case FAST_DOUBLE_ELEMENTS: {
        // Empty array is FixedArray but not FixedDoubleArray.
        if (length == 0) break;
        Handle<FixedDoubleArray> elements(
            FixedDoubleArray::cast(object->elements()), isolate_);
        StackLimitCheck interrupt_check(isolate_);
        while (i < length) {
          if (interrupt_check.InterruptRequested() &&
              isolate_->stack_guard()->HandleInterrupts()->IsException(
                  isolate_)) {
            return EXCEPTION;
          }
          Separator(i == 0);
          SerializeDouble(elements->get_scalar(i));
          i++;
        }
        break;
      }
      case FAST_ELEMENTS: {
        Handle<Object> old_length(object->length(), isolate_);
        while (i < length) {
          if (object->length() != *old_length ||
              object->GetElementsKind() != FAST_ELEMENTS) {
            // Fall back to slow path.
            break;
          }
          Separator(i == 0);
          Result result = SerializeElement(
              isolate_,
              Handle<Object>(FixedArray::cast(object->elements())->get(i),
                             isolate_),
              i);
          if (result == UNCHANGED) {
            builder_.AppendCString("null");
          } else if (result != SUCCESS) {
            return result;
          }
          i++;
        }
        break;
      }
      // The FAST_HOLEY_* cases could be handled in a faster way. They resemble
      // the non-holey cases except that a lookup is necessary for holes.
      default:
        break;
    }
  }
  if (i < length) {
    // Slow path for non-fast elements and fall-back in edge case.
    Result result = SerializeArrayLikeSlow(object, i, length);
    if (result != SUCCESS) return result;
  }
  Unindent();
  if (length > 0) NewLine();
  builder_.AppendCharacter(']');
  StackPop();
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeArrayLikeSlow(
    Handle<JSReceiver> object, uint32_t start, uint32_t length) {
  // We need to write out at least two characters per array element.
  static const int kMaxSerializableArrayLength = String::kMaxLength / 2;
  if (length > kMaxSerializableArrayLength) {
    isolate_->Throw(*isolate_->factory()->NewInvalidStringLengthError());
    return EXCEPTION;
  }
  for (uint32_t i = start; i < length; i++) {
    Separator(i == 0);
    Handle<Object> element;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, element, JSReceiver::GetElement(isolate_, object, i),
        EXCEPTION);
    Result result = SerializeElement(isolate_, element, i);
    if (result == SUCCESS) continue;
    if (result == UNCHANGED) {
      // Detect overflow sooner for large sparse arrays.
      if (builder_.HasOverflowed()) return EXCEPTION;
      builder_.AppendCString("null");
    } else {
      return result;
    }
  }
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeJSObject(
    Handle<JSObject> object) {
  HandleScope handle_scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;

  if (property_list_.is_null() &&
      object->map()->instance_type() > LAST_CUSTOM_ELEMENTS_RECEIVER &&
      object->HasFastProperties() &&
      Handle<JSObject>::cast(object)->elements()->length() == 0) {
    DCHECK(object->IsJSObject());
    DCHECK(!object->IsJSGlobalProxy());
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    DCHECK(!js_obj->HasIndexedInterceptor());
    DCHECK(!js_obj->HasNamedInterceptor());
    Handle<Map> map(js_obj->map());
    builder_.AppendCharacter('{');
    Indent();
    bool comma = false;
    for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
      Handle<Name> name(map->instance_descriptors()->GetKey(i), isolate_);
      // TODO(rossberg): Should this throw?
      if (!name->IsString()) continue;
      Handle<String> key = Handle<String>::cast(name);
      PropertyDetails details = map->instance_descriptors()->GetDetails(i);
      if (details.IsDontEnum()) continue;
      Handle<Object> property;
      if (details.type() == DATA && *map == js_obj->map()) {
        FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
        property = JSObject::FastPropertyAt(js_obj, details.representation(),
                                            field_index);
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate_, property, Object::GetPropertyOrElement(js_obj, key),
            EXCEPTION);
      }
      Result result = SerializeProperty(property, comma, key);
      if (!comma && result == SUCCESS) comma = true;
      if (result == EXCEPTION) return result;
    }
    Unindent();
    if (comma) NewLine();
    builder_.AppendCharacter('}');
  } else {
    Result result = SerializeJSReceiverSlow(object);
    if (result != SUCCESS) return result;
  }
  StackPop();
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeJSReceiverSlow(
    Handle<JSReceiver> object) {
  Handle<FixedArray> contents = property_list_;
  if (contents.is_null()) {
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, contents,
        KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                ENUMERABLE_STRINGS,
                                GetKeysConversion::kConvertToString),
        EXCEPTION);
  }
  builder_.AppendCharacter('{');
  Indent();
  bool comma = false;
  for (int i = 0; i < contents->length(); i++) {
    Handle<String> key(String::cast(contents->get(i)), isolate_);
    Handle<Object> property;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate_, property,
                                     Object::GetPropertyOrElement(object, key),
                                     EXCEPTION);
    Result result = SerializeProperty(property, comma, key);
    if (!comma && result == SUCCESS) comma = true;
    if (result == EXCEPTION) return result;
  }
  Unindent();
  if (comma) NewLine();
  builder_.AppendCharacter('}');
  return SUCCESS;
}

JsonStringifier::Result JsonStringifier::SerializeJSProxy(
    Handle<JSProxy> object) {
  HandleScope scope(isolate_);
  Result stack_push = StackPush(object);
  if (stack_push != SUCCESS) return stack_push;
  Maybe<bool> is_array = Object::IsArray(object);
  if (is_array.IsNothing()) return EXCEPTION;
  if (is_array.FromJust()) {
    Handle<Object> length_object;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate_, length_object,
        Object::GetLengthFromArrayLike(isolate_, object), EXCEPTION);
    uint32_t length;
    if (!length_object->ToUint32(&length)) {
      // Technically, we need to be able to handle lengths outside the
      // uint32_t range. However, we would run into string size overflow
      // if we tried to stringify such an array.
      isolate_->Throw(*isolate_->factory()->NewInvalidStringLengthError());
      return EXCEPTION;
    }
    builder_.AppendCharacter('[');
    Indent();
    Result result = SerializeArrayLikeSlow(object, 0, length);
    if (result != SUCCESS) return result;
    Unindent();
    if (length > 0) NewLine();
    builder_.AppendCharacter(']');
  } else {
    Result result = SerializeJSReceiverSlow(object);
    if (result != SUCCESS) return result;
  }
  StackPop();
  return SUCCESS;
}

template <typename SrcChar, typename DestChar>
void JsonStringifier::SerializeStringUnchecked_(
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
void JsonStringifier::SerializeString_(Handle<String> string) {
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
bool JsonStringifier::DoNotEscape(uint8_t c) {
  return c >= '#' && c <= '~' && c != '\\';
}

template <>
bool JsonStringifier::DoNotEscape(uint16_t c) {
  return c >= '#' && c != '\\' && c != 0x7f;
}

void JsonStringifier::NewLine() {
  if (gap_ == nullptr) return;
  builder_.AppendCharacter('\n');
  for (int i = 0; i < indent_; i++) builder_.AppendCString(gap_);
}

void JsonStringifier::Separator(bool first) {
  if (!first) builder_.AppendCharacter(',');
  NewLine();
}

void JsonStringifier::SerializeDeferredKey(bool deferred_comma,
                                           Handle<Object> deferred_key) {
  Separator(!deferred_comma);
  SerializeString(Handle<String>::cast(deferred_key));
  builder_.AppendCharacter(':');
  if (gap_ != nullptr) builder_.AppendCharacter(' ');
}

void JsonStringifier::SerializeString(Handle<String> object) {
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

}  // namespace internal
}  // namespace v8
