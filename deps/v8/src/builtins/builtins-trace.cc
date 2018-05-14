// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/counters.h"
#include "src/api.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {

using v8::tracing::TracedValue;

enum Result { NODATA, SUCCESS, FAILED };

class MaybeUtf8 {
 public:
  MaybeUtf8(Handle<String> string) {
    string = String::Flatten(string);
    one_byte_ = string->IsOneByteRepresentation();
    if (one_byte_) {
      data_ = string->length() == 0 ? "" :
          reinterpret_cast<const char*>(Handle<SeqOneByteString>::cast(string)
              ->GetChars());
    } else {
      data_ = nullptr;
      Local<v8::String> local = Utils::ToLocal(string);
      allocated_.reset(new char[local->Utf8Length() + 1]);
      local->WriteUtf8(allocated_.get());
    }
  }
  const char* operator*() const {
    return allocated_ ? const_cast<const char*>(allocated_.get()) : data_;
  }
 private:
  const char* data_;
  std::unique_ptr<char> allocated_;
  bool one_byte_;
};

class ScopedSetTraceValue {
 public:
  ScopedSetTraceValue(std::unique_ptr<TracedValue> traced_value,
                      uint8_t* arg_type, uint64_t* arg_value)
      : traced_value_(std::move(traced_value)),
        arg_type_(arg_type),
        arg_value_(arg_value) {}

  ~ScopedSetTraceValue() {
    tracing::SetTraceValue(std::move(traced_value_), arg_type_, arg_value_);
  }

  TracedValue* operator*() const { return traced_value_.get(); }
  TracedValue* operator->() const { return traced_value_.get(); }
 private:
  std::unique_ptr<TracedValue> traced_value_;
  uint8_t* arg_type_;
  uint64_t* arg_value_;
};

const uint8_t* GetCategoryGroupEnabled(Handle<String> string) {
  MaybeUtf8 category(string);
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(*category);
}

Result ProcessSmi(Smi* smi, uint8_t* arg_type, uint64_t* arg_value) {
  tracing::SetTraceValue(smi->value(), arg_type, arg_value);
  return SUCCESS;
}

Result SetSmi(Handle<String> key, Smi* smi, TracedValue* traced_value) {
  MaybeUtf8 name(key);
  traced_value->SetInteger(*name, smi->value());
  return SUCCESS;
}

Result ProcessDouble(Handle<HeapNumber> object,
                   uint8_t* arg_type,
                   uint64_t* arg_value) {
  double number = Handle<HeapNumber>::cast(object)->value();
  if (std::isinf(number) || std::isnan(number)) {
    return NODATA;
  }
  tracing::SetTraceValue(number, arg_type, arg_value);
  return SUCCESS;
}

Result AppendDouble(Handle<HeapNumber> object, TracedValue* traced_value) {
  double number = Handle<HeapNumber>::cast(object)->value();
  if (std::isinf(number) || std::isnan(number)) {
    traced_value->AppendNull();
  } else {
    traced_value->AppendDouble(number);
  }
  return SUCCESS;
}

Result SetDouble(Handle<String> key,
                 Handle<HeapNumber> object,
                 TracedValue* traced_value) {
  MaybeUtf8 name(key);
  double number = Handle<HeapNumber>::cast(object)->value();
  if (std::isinf(number) || std::isnan(number)) {
    traced_value->SetNull(*name);
  } else {
    traced_value->SetDouble(*name, number);
  }
  return SUCCESS;
}

Result ProcessString(Isolate* isolate,
                     Handle<String> string,
                     uint8_t* arg_type,
                     uint64_t* arg_value) {
  MaybeUtf8 data(string);
  tracing::SetTraceValue(TRACE_STR_COPY(*data), arg_type, arg_value);
  return SUCCESS;
}

Result AppendString(Isolate* isolate,
                    Handle<String> string,
                    TracedValue* traced_value) {
  MaybeUtf8 data(string);
  traced_value->AppendString(*data);
  return SUCCESS;
}

Result SetString(Isolate* isolate,
                 Handle<String> key,
                 Handle<String> string,
                 TracedValue* traced_value) {
  MaybeUtf8 name(key);
  MaybeUtf8 data(string);
  traced_value->SetString(*name, *data);
  return SUCCESS;
}

Result ProcessOddball(Oddball* oddball,
                    uint8_t* arg_type,
                    uint64_t* arg_value) {
  switch (oddball->kind()) {
    case Oddball::kFalse:
      tracing::SetTraceValue(false, arg_type, arg_value);
      return SUCCESS;
    case Oddball::kTrue:
      tracing::SetTraceValue(true, arg_type, arg_value);
      return SUCCESS;
    default:
      return NODATA;
  }
  UNREACHABLE();
}

Result AppendOddball(Oddball* oddball, TracedValue* traced_value) {
  switch (oddball->kind()) {
    case Oddball::kFalse:
      traced_value->AppendBoolean(false);
      break;
    case Oddball::kTrue:
      traced_value->AppendBoolean(true);
      break;
    default:
      traced_value->AppendNull();
  }
  return SUCCESS;
}

Result SetOddball(Handle<String> key,
                  Oddball* oddball,
                  TracedValue* traced_value) {
  MaybeUtf8 name(key);
  switch (oddball->kind()) {
    case Oddball::kFalse:
      traced_value->SetBoolean(*name, false);
      break;
    case Oddball::kTrue:
      traced_value->SetBoolean(*name, true);
      break;
    default:
      traced_value->SetNull(*name);
  }
  return SUCCESS;
}

Result ProcessJSValue(Isolate* isolate,
                    Handle<JSValue> object,
                    uint8_t* arg_type,
                    uint64_t* arg_value) {
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value,
                                   Object::ToPrimitive(object),
                                   FAILED);
  if (value->IsString()) {
    return ProcessString(isolate,
                         Handle<String>::cast(value),
                         arg_type, arg_value);
  } else if (value->IsNumber()) {
    if (value->IsSmi()) {
      return ProcessSmi(Smi::cast(*value), arg_type, arg_value);
    }
    return ProcessDouble(Handle<HeapNumber>::cast(value), arg_type, arg_value);
  } else if (value->IsBoolean()) {
    tracing::SetTraceValue(value->IsTrue(isolate) ? true : false,
                           arg_type, arg_value);
    return SUCCESS;
  } else {
    return NODATA;
  }

  UNREACHABLE();
}

Result AppendJSElement(Isolate* isolate,
                       Handle<JSValue> object,
                       TracedValue* traced_value) {
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value,
                                   Object::ToPrimitive(object),
                                   FAILED);
  if (value->IsString()) {
    return AppendString(isolate,
                        Handle<String>::cast(value),
                        traced_value);
  } else if (value->IsNumber()) {
    if (value->IsSmi()) {
      traced_value->AppendInteger(Smi::cast(*value)->value());
      return SUCCESS;
    }
    return AppendDouble(Handle<HeapNumber>::cast(value), traced_value);
  } else if (value->IsBoolean()) {
    traced_value->AppendBoolean(value->IsTrue(isolate) ? true : false);
  } else {
    traced_value->AppendNull();
  }
  return SUCCESS;
}

Result SetJSElement(Isolate* isolate,
                    Handle<String> key,
                    Handle<JSValue> object,
                    TracedValue* traced_value) {
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, value,
                                   Object::ToPrimitive(object),
                                   FAILED);
  if (value->IsString()) {
    return SetString(isolate,
                     key, Handle<String>::cast(value),
                     traced_value);
  } else if (value->IsNumber()) {
    if (value->IsSmi()) {
      return SetSmi(key, Smi::cast(*value), traced_value);
    }
    return SetDouble(key, Handle<HeapNumber>::cast(value), traced_value);
  } else {
    MaybeUtf8 name(key);
    if (value->IsBoolean()) {
      traced_value->SetBoolean(*name, value->IsTrue(isolate) ? true : false);
    } else {
      traced_value->SetNull(*name);
    }
    return SUCCESS;
  }

  UNREACHABLE();
}

Result AppendJSValue(Isolate* isolate,
                     Handle<Object> object,
                     TracedValue* traced_value) {

  if (!object->IsNullOrUndefined(isolate)) {
    if (object->IsSmi()) {
      traced_value->AppendInteger(Smi::cast(*object)->value());
      return SUCCESS;
    }
    switch (HeapObject::cast(*object)->map()->instance_type()) {
      case HEAP_NUMBER_TYPE:
      case MUTABLE_HEAP_NUMBER_TYPE:
        return AppendDouble(Handle<HeapNumber>::cast(object), traced_value);
      case ODDBALL_TYPE:
        return AppendOddball(Oddball::cast(*object), traced_value);
      case JS_VALUE_TYPE:
        return AppendJSElement(isolate,
                               Handle<JSValue>::cast(object),
                               traced_value);
      default:
        if (object->IsString()) {
          return AppendString(isolate,
                              Handle<String>::cast(object),
                              traced_value);
        }
        return FAILED;
    }

    UNREACHABLE();
  } else {
    traced_value->AppendNull();
  }

  return SUCCESS;
}

Result SetJSValue(Isolate* isolate,
                  Handle<String> key,
                  Handle<Object> object,
                  TracedValue* traced_value) {
  if (!object->IsNullOrUndefined(isolate)) {
    if (object->IsSmi()) {
      return SetSmi(key, Smi::cast(*object), traced_value);
    }
    switch (HeapObject::cast(*object)->map()->instance_type()) {
      case HEAP_NUMBER_TYPE:
      case MUTABLE_HEAP_NUMBER_TYPE:
        return SetDouble(key, Handle<HeapNumber>::cast(object), traced_value);
      case ODDBALL_TYPE:
        return SetOddball(key, Oddball::cast(*object), traced_value);
      case JS_VALUE_TYPE:
        return SetJSElement(isolate,
                            key, Handle<JSValue>::cast(object),
                            traced_value);
      default:
        if (object->IsString()) {
          return SetString(isolate,
                           key, Handle<String>::cast(object),
                           traced_value);
        }
        return FAILED;
    }

    UNREACHABLE();
  } else {
    MaybeUtf8 name(key);
    traced_value->SetNull(*name);
  }

  return SUCCESS;
}

Result ProcessArrayLikeSlow(Isolate* isolate,
                            Handle<JSReceiver> object,
                            uint32_t start,
                            uint32_t length,
                            TracedValue* traced_value) {
  for (uint32_t i = start; i < length; i++) {
    Handle<Object> element;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, element, JSReceiver::GetElement(isolate, object, i),
        FAILED);
    Result result = AppendJSValue(isolate, element, traced_value);
    if (result == SUCCESS) continue;
    if (result == NODATA) {
      traced_value->AppendNull();
    } else {
      return result;
    }
  }
  return SUCCESS;
}

Result ProcessJSArray(Isolate* isolate,
                      Handle<JSArray> object,
                      uint8_t* arg_type,
                      uint64_t* arg_value) {
  ScopedSetTraceValue traced_value(TracedValue::CreateArray(),
                                   arg_type, arg_value);

  uint32_t length = 0;
  uint32_t i = 0;
  CHECK(object->length()->ToArrayLength(&length));
  DCHECK(!object->IsAccessCheckNeeded());

  switch (object->GetElementsKind()) {
    case PACKED_SMI_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(object->elements()),
                                  isolate);
      while (i < length) {
        traced_value->AppendInteger(Smi::cast(elements->get(i))->value());
        i++;
      }
      break;
    }
    case PACKED_DOUBLE_ELEMENTS: {
      if (length == 0) break;
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(object->elements()), isolate);
      while (i < length) {
        traced_value->AppendDouble(elements->get_scalar(i));
        i++;
      }
      break;
    }
    case PACKED_ELEMENTS: {
      Handle<Object> old_length(object->length(), isolate);
      while (i < length) {
        if (object->length() != *old_length ||
            object->GetElementsKind() != PACKED_ELEMENTS) {
          // Fall back to slow path.
          break;
        }
        Result result = AppendJSValue(
            isolate,
            Handle<Object>(FixedArray::cast(object->elements())->get(i),
                           isolate), *traced_value);
        if (result == NODATA) {
          traced_value->AppendNull();
        } else if (result == FAILED) {
          return FAILED;
        }
        i++;
      }
      break;
    }
    default:
      break;
  }
  if (i < length &&
      ProcessArrayLikeSlow(isolate, object, i, length,
                           *traced_value) == FAILED) {
    return FAILED;
  }
  return SUCCESS;
}

Result ProcessJSReceiverSlow(Isolate* isolate,
                             Handle<JSReceiver> object,
                             TracedValue* traced_value) {
  Handle<FixedArray> contents;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, contents,
      KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                              ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString),
      FAILED);
  for (int i = 0; i < contents->length(); i++) {
    Handle<String> key(String::cast(contents->get(i)), isolate);
    Handle<Object> property;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, property,
                                     Object::GetPropertyOrElement(object, key),
                                     FAILED);
    if (SetJSValue(isolate, key, property, traced_value) == FAILED)
      return FAILED;
  }
  return SUCCESS;
}

Result ProcessObject(Isolate* isolate,
                     Handle<JSObject> object,
                     uint8_t* arg_type,
                     uint64_t* arg_value) {
  ScopedSetTraceValue traced_value(TracedValue::Create(), arg_type, arg_value);
  HandleScope handle_scope(isolate);

  if (object->map()->instance_type() > LAST_CUSTOM_ELEMENTS_RECEIVER &&
      object->HasFastProperties() &&
      Handle<JSObject>::cast(object)->elements()->length() == 0) {
    DCHECK(object->IsJSObject());
    DCHECK(!object->IsJSGlobalProxy());
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    DCHECK(!js_obj->HasIndexedInterceptor());
    DCHECK(!js_obj->HasNamedInterceptor());
    Handle<Map> map(js_obj->map());

    for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
      Handle<Name> name(map->instance_descriptors()->GetKey(i), isolate);
      if (!name->IsString()) continue;
      Handle<String> key = Handle<String>::cast(name);
      PropertyDetails details = map->instance_descriptors()->GetDetails(i);
      if (details.IsDontEnum()) continue;
      Handle<Object> property;
      if (details.location() == kField && *map == js_obj->map()) {
        DCHECK_EQ(kData, details.kind());
        FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
        property = JSObject::FastPropertyAt(js_obj, details.representation(),
                                            field_index);
      } else {
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, property, Object::GetPropertyOrElement(js_obj, key),
            FAILED);
      }
      if (SetJSValue(isolate, key, property, *traced_value) == FAILED)
        return FAILED;
    }
  } else {
    if (ProcessJSReceiverSlow(isolate, object, *traced_value) == FAILED)
      return FAILED;
  }

  return SUCCESS;
}

Result ProcessJSProxy(Isolate* isolate,
                      Handle<JSProxy> object,
                      int32_t* num_args,
                      uint8_t* arg_type,
                      uint64_t* arg_value) {
  HandleScope scope(isolate);
  Maybe<bool> is_array = Object::IsArray(object);
  if (is_array.IsNothing()) return FAILED;
  if (is_array.FromJust()) {
    Handle<Object> length_object;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, length_object,
        Object::GetLengthFromArrayLike(isolate, object), FAILED);
    *num_args = 1;
    uint32_t length;
    if (!length_object->ToUint32(&length))
      return FAILED;

    ScopedSetTraceValue traced_value(TracedValue::CreateArray(),
                                     arg_type, arg_value);

    *num_args = 1;
    if (ProcessArrayLikeSlow(isolate, object, 0, length,
                             *traced_value) == FAILED) {
      return FAILED;
    }
  } else {
    *num_args = 1;
    ScopedSetTraceValue traced_value(TracedValue::Create(),
                                     arg_type, arg_value);
    if (ProcessJSReceiverSlow(isolate, object, *traced_value) == FAILED) {
      return FAILED;
    }
  }
  return SUCCESS;
}

Result ProcessData(Isolate* isolate,
                   Handle<Object> object,
                   int32_t* num_args,
                   uint8_t* arg_type,
                   uint64_t* arg_value);

Result ProcessCallable(Isolate* isolate,
                       Handle<JSReceiver> callable,
                       int32_t* num_args,
                       uint8_t* arg_type,
                       uint64_t* arg_value) {
  HandleScope scope(isolate);
  DCHECK(callback->IsCallable());
  Handle<Object> argv[] = {};
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value,
      Execution::Call(isolate, callable, isolate->global_proxy(),
                      0, argv), FAILED);
  if (isolate->has_pending_exception())
    return FAILED;
  return ProcessData(isolate, scope.CloseAndEscape(value),
                     num_args, arg_type, arg_value);
}

Result ProcessData(Isolate* isolate,
                   Handle<Object> object,
                   int32_t* num_args,
                   uint8_t* arg_type,
                   uint64_t* arg_value) {
  // Only supports primitives, objects with primitive own values,
  // arrays with primitives, and functions that return primitives.
  if (!object->IsNullOrUndefined(isolate)) {
    if (object->IsSmi()) {
      return ProcessSmi(Smi::cast(*object), arg_type, arg_value);
    }
    switch (HeapObject::cast(*object)->map()->instance_type()) {
      case HEAP_NUMBER_TYPE:
      case MUTABLE_HEAP_NUMBER_TYPE:
        *num_args = 1;
        return ProcessDouble(Handle<HeapNumber>::cast(object),
                             arg_type, arg_value);
      case ODDBALL_TYPE:
        *num_args = 1;
        return ProcessOddball(Oddball::cast(*object), arg_type, arg_value);
      case JS_VALUE_TYPE:
        *num_args = 1;
        return ProcessJSValue(isolate,
                              Handle<JSValue>::cast(object),
                              arg_type, arg_value);
      case JS_ARRAY_TYPE:
        *num_args = 1;
        return ProcessJSArray(isolate,
                              Handle<JSArray>::cast(object),
                              arg_type, arg_value);
      default:
        if (object->IsString()) {
          *num_args = 1;
          return ProcessString(isolate,
                               Handle<String>::cast(object),
                               arg_type, arg_value);
        } else {
          DCHECK(object->IsJSReceiver());
          if (object->IsCallable()) {
            return ProcessCallable(isolate, Handle<JSReceiver>::cast(object),
                                   num_args, arg_type, arg_value);
          }
          if (object->IsJSProxy()) {
            return ProcessJSProxy(isolate, Handle<JSProxy>::cast(object),
                                  num_args, arg_type, arg_value);
          }
          *num_args = 1;
          return ProcessObject(isolate, Handle<JSObject>::cast(object),
                               arg_type, arg_value);
        }
    }

    UNREACHABLE();
  }

  return NODATA;
}

} // namespace

// Builins::kIsTraceCategoryEnabled(category) : bool
BUILTIN(IsTraceCategoryEnabled) {
  HandleScope scope(isolate);
  Handle<Object> category = args.atOrUndefined(isolate, 1);
  if (!category->IsString()) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kTraceEventCategory));
  }
  return isolate->heap()->ToBoolean(
      *GetCategoryGroupEnabled(Handle<String>::cast(category)));
}

// Builtins::kTrace(phase, category, name, id, data) : bool
BUILTIN(Trace) {
  static const char* arg_name = "data";
  HandleScope handle_scope(isolate);

  Handle<Object> phase_arg = args.atOrUndefined(isolate, 1);
  Handle<Object> category = args.atOrUndefined(isolate, 2);
  Handle<Object> name_arg = args.atOrUndefined(isolate, 3);
  Handle<Object> id_arg = args.atOrUndefined(isolate, 4);
  Handle<Object> data_arg = args.atOrUndefined(isolate, 5);

  bool result = false;

  if (!phase_arg->IsNumber()) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kTraceEventPhase));
  }
  if (!category->IsString()) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kTraceEventCategory));
  }
  if (!name_arg->IsString()) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kTraceEventName));
  }
  if (!id_arg->IsNumber()) {
    return isolate->Throw(*isolate->factory()->NewTypeError(
        MessageTemplate::kTraceEventID));
  }

  const uint8_t* category_group_enabled =
      GetCategoryGroupEnabled(Handle<String>::cast(category));

  if (*category_group_enabled) {
    int32_t num_args = 0;
    uint8_t arg_type;
    uint64_t arg_value;
    int32_t id = 0;
    uint32_t flags = TRACE_EVENT_FLAG_COPY;
    if (!id_arg->IsNullOrUndefined(isolate)) {
      flags |= TRACE_EVENT_FLAG_HAS_ID;
      id = DoubleToInt32(id_arg->Number());
    }

    char phase = static_cast<char>(DoubleToInt32(phase_arg->Number()));

    if (ProcessData(isolate, data_arg, &num_args,
                    &arg_type, &arg_value) == FAILED) {

      if (isolate->has_pending_exception())
        return isolate->heap()->exception();

      return isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kTraceEventData));
    }

    Handle<String> name_str = Handle<String>::cast(name_arg);
    if (name_str->length() == 0) {
      return isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kTraceEventNameLength));
    }
    MaybeUtf8 name(name_str);
    TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_group_enabled, *name, tracing::kGlobalScope, id,
      tracing::kNoId, num_args, &arg_name, &arg_type, &arg_value, flags);
    result = true;
  }
  return isolate->heap()->ToBoolean(result);
}

}  // namespace internal
}  // namespace v8
