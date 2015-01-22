// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/api.h"
#include "src/compiler.h"
#include "src/contexts.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/list-inl.h"
#include "src/property-details.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {


Handle<AccessorInfo> Accessors::MakeAccessor(
    Isolate* isolate,
    Handle<Name> name,
    AccessorNameGetterCallback getter,
    AccessorNameSetterCallback setter,
    PropertyAttributes attributes) {
  Factory* factory = isolate->factory();
  Handle<ExecutableAccessorInfo> info = factory->NewExecutableAccessorInfo();
  info->set_property_attributes(attributes);
  info->set_all_can_read(false);
  info->set_all_can_write(false);
  info->set_name(*name);
  Handle<Object> get = v8::FromCData(isolate, getter);
  Handle<Object> set = v8::FromCData(isolate, setter);
  info->set_getter(*get);
  info->set_setter(*set);
  return info;
}


Handle<ExecutableAccessorInfo> Accessors::CloneAccessor(
    Isolate* isolate,
    Handle<ExecutableAccessorInfo> accessor) {
  Factory* factory = isolate->factory();
  Handle<ExecutableAccessorInfo> info = factory->NewExecutableAccessorInfo();
  info->set_name(accessor->name());
  info->set_flag(accessor->flag());
  info->set_expected_receiver_type(accessor->expected_receiver_type());
  info->set_getter(accessor->getter());
  info->set_setter(accessor->setter());
  info->set_data(accessor->data());
  return info;
}


static V8_INLINE bool CheckForName(Handle<Name> name,
                                   Handle<String> property_name,
                                   int offset,
                                   int* object_offset) {
  if (Name::Equals(name, property_name)) {
    *object_offset = offset;
    return true;
  }
  return false;
}


// Returns true for properties that are accessors to object fields.
// If true, *object_offset contains offset of object field.
template <class T>
bool Accessors::IsJSObjectFieldAccessor(typename T::TypeHandle type,
                                        Handle<Name> name,
                                        int* object_offset) {
  Isolate* isolate = name->GetIsolate();

  if (type->Is(T::String())) {
    return CheckForName(name, isolate->factory()->length_string(),
                        String::kLengthOffset, object_offset);
  }

  if (!type->IsClass()) return false;
  Handle<Map> map = type->AsClass()->Map();

  switch (map->instance_type()) {
    case JS_ARRAY_TYPE:
      return
        CheckForName(name, isolate->factory()->length_string(),
                     JSArray::kLengthOffset, object_offset);
    case JS_TYPED_ARRAY_TYPE:
      return
        CheckForName(name, isolate->factory()->length_string(),
                     JSTypedArray::kLengthOffset, object_offset) ||
        CheckForName(name, isolate->factory()->byte_length_string(),
                     JSTypedArray::kByteLengthOffset, object_offset) ||
        CheckForName(name, isolate->factory()->byte_offset_string(),
                     JSTypedArray::kByteOffsetOffset, object_offset);
    case JS_ARRAY_BUFFER_TYPE:
      return
        CheckForName(name, isolate->factory()->byte_length_string(),
                     JSArrayBuffer::kByteLengthOffset, object_offset);
    case JS_DATA_VIEW_TYPE:
      return
        CheckForName(name, isolate->factory()->byte_length_string(),
                     JSDataView::kByteLengthOffset, object_offset) ||
        CheckForName(name, isolate->factory()->byte_offset_string(),
                     JSDataView::kByteOffsetOffset, object_offset);
    default:
      return false;
  }
}


template
bool Accessors::IsJSObjectFieldAccessor<Type>(Type* type,
                                              Handle<Name> name,
                                              int* object_offset);


template
bool Accessors::IsJSObjectFieldAccessor<HeapType>(Handle<HeapType> type,
                                                  Handle<Name> name,
                                                  int* object_offset);


bool SetPropertyOnInstanceIfInherited(
    Isolate* isolate, const v8::PropertyCallbackInfo<void>& info,
    v8::Local<v8::Name> name, Handle<Object> value) {
  Handle<Object> holder = Utils::OpenHandle(*info.Holder());
  Handle<Object> receiver = Utils::OpenHandle(*info.This());
  if (*holder == *receiver) return false;
  if (receiver->IsJSObject()) {
    Handle<JSObject> object = Handle<JSObject>::cast(receiver);
    // This behaves sloppy since we lost the actual strict-mode.
    // TODO(verwaest): Fix by making ExecutableAccessorInfo behave like data
    // properties.
    if (!object->map()->is_extensible()) return true;
    JSObject::SetOwnPropertyIgnoreAttributes(object, Utils::OpenHandle(*name),
                                             value, NONE).Check();
  }
  return true;
}


//
// Accessors::ArgumentsIterator
//


void Accessors::ArgumentsIteratorGetter(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* result = isolate->native_context()->array_values_iterator();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(result, isolate)));
}


void Accessors::ArgumentsIteratorSetter(
    v8::Local<v8::Name> name, v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSObject> object = Utils::OpenHandle(*info.This());
  Handle<Object> value = Utils::OpenHandle(*val);

  if (SetPropertyOnInstanceIfInherited(isolate, info, name, value)) return;

  LookupIterator it(object, Utils::OpenHandle(*name));
  CHECK_EQ(LookupIterator::ACCESSOR, it.state());
  DCHECK(it.HolderIsReceiverOrHiddenPrototype());

  if (Object::SetDataProperty(&it, value).is_null()) {
    isolate->OptionalRescheduleException(false);
  }
}


Handle<AccessorInfo> Accessors::ArgumentsIteratorInfo(
    Isolate* isolate, PropertyAttributes attributes) {
  Handle<Name> name = isolate->factory()->iterator_symbol();
  return MakeAccessor(isolate, name, &ArgumentsIteratorGetter,
                      &ArgumentsIteratorSetter, attributes);
}


//
// Accessors::ArrayLength
//


// The helper function will 'flatten' Number objects.
Handle<Object> Accessors::FlattenNumber(Isolate* isolate,
                                        Handle<Object> value) {
  if (value->IsNumber() || !value->IsJSValue()) return value;
  Handle<JSValue> wrapper = Handle<JSValue>::cast(value);
  DCHECK(wrapper->GetIsolate()->native_context()->number_function()->
      has_initial_map());
  if (wrapper->map() == isolate->number_function()->initial_map()) {
    return handle(wrapper->value(), isolate);
  }

  return value;
}


void Accessors::ArrayLengthGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  JSArray* holder = JSArray::cast(*Utils::OpenHandle(*info.Holder()));
  Object* result = holder->length();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(result, isolate)));
}


void Accessors::ArrayLengthSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSObject> object = Utils::OpenHandle(*info.This());
  Handle<Object> value = Utils::OpenHandle(*val);
  if (SetPropertyOnInstanceIfInherited(isolate, info, name, value)) {
    return;
  }

  value = FlattenNumber(isolate, value);

  Handle<JSArray> array_handle = Handle<JSArray>::cast(object);
  MaybeHandle<Object> maybe;
  Handle<Object> uint32_v;
  maybe = Execution::ToUint32(isolate, value);
  if (!maybe.ToHandle(&uint32_v)) {
    isolate->OptionalRescheduleException(false);
    return;
  }
  Handle<Object> number_v;
  maybe = Execution::ToNumber(isolate, value);
  if (!maybe.ToHandle(&number_v)) {
    isolate->OptionalRescheduleException(false);
    return;
  }

  if (uint32_v->Number() == number_v->Number()) {
    maybe = JSArray::SetElementsLength(array_handle, uint32_v);
    if (maybe.is_null()) isolate->OptionalRescheduleException(false);
    return;
  }

  Handle<Object> exception;
  maybe = isolate->factory()->NewRangeError("invalid_array_length",
                                            HandleVector<Object>(NULL, 0));
  if (!maybe.ToHandle(&exception)) {
    isolate->OptionalRescheduleException(false);
    return;
  }

  isolate->ScheduleThrow(*exception);
}


Handle<AccessorInfo> Accessors::ArrayLengthInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->length_string(),
                      &ArrayLengthGetter,
                      &ArrayLengthSetter,
                      attributes);
}



//
// Accessors::StringLength
//

void Accessors::StringLengthGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);

  // We have a slight impedance mismatch between the external API and the way we
  // use callbacks internally: Externally, callbacks can only be used with
  // v8::Object, but internally we have callbacks on entities which are higher
  // in the hierarchy, in this case for String values.

  Object* value = *Utils::OpenHandle(*v8::Local<v8::Value>(info.This()));
  if (!value->IsString()) {
    // Not a string value. That means that we either got a String wrapper or
    // a Value with a String wrapper in its prototype chain.
    value = JSValue::cast(*Utils::OpenHandle(*info.Holder()))->value();
  }
  Object* result = Smi::FromInt(String::cast(value)->length());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(result, isolate)));
}


void Accessors::StringLengthSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::StringLengthInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->length_string(),
                      &StringLengthGetter,
                      &StringLengthSetter,
                      attributes);
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


// Implements ECMA262 ES6 draft 21.2.5.9
void Accessors::RegExpSourceGetter(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);

  Handle<Object> holder =
      Utils::OpenHandle(*v8::Local<v8::Value>(info.Holder()));
  Handle<JSRegExp> regexp = Handle<JSRegExp>::cast(holder);
  Handle<String> result;
  if (regexp->TypeTag() == JSRegExp::NOT_COMPILED) {
    result = isolate->factory()->empty_string();
  } else {
    Handle<String> pattern(regexp->Pattern(), isolate);
    MaybeHandle<String> maybe = EscapeRegExpSource(isolate, pattern);
    if (!maybe.ToHandle(&result)) {
      isolate->OptionalRescheduleException(false);
      return;
    }
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::RegExpSourceSetter(v8::Local<v8::Name> name,
                                   v8::Local<v8::Value> value,
                                   const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::RegExpSourceInfo(
    Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->source_string(),
                      &RegExpSourceGetter, &RegExpSourceSetter, attributes);
}


//
// Accessors::ScriptColumnOffset
//


void Accessors::ScriptColumnOffsetGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* res = Script::cast(JSValue::cast(object)->value())->column_offset();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


void Accessors::ScriptColumnOffsetSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptColumnOffsetInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("column_offset")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptColumnOffsetGetter,
                      &ScriptColumnOffsetSetter,
                      attributes);
}


//
// Accessors::ScriptId
//


void Accessors::ScriptIdGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* id = Script::cast(JSValue::cast(object)->value())->id();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(id, isolate)));
}


void Accessors::ScriptIdSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptIdInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("id")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptIdGetter,
                      &ScriptIdSetter,
                      attributes);
}


//
// Accessors::ScriptName
//


void Accessors::ScriptNameGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* source = Script::cast(JSValue::cast(object)->value())->name();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(source, isolate)));
}


void Accessors::ScriptNameSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->name_string(),
                      &ScriptNameGetter,
                      &ScriptNameSetter,
                      attributes);
}


//
// Accessors::ScriptSource
//


void Accessors::ScriptSourceGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* source = Script::cast(JSValue::cast(object)->value())->source();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(source, isolate)));
}


void Accessors::ScriptSourceSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptSourceInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->source_string(),
                      &ScriptSourceGetter,
                      &ScriptSourceSetter,
                      attributes);
}


//
// Accessors::ScriptLineOffset
//


void Accessors::ScriptLineOffsetGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* res = Script::cast(JSValue::cast(object)->value())->line_offset();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


void Accessors::ScriptLineOffsetSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptLineOffsetInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("line_offset")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptLineOffsetGetter,
                      &ScriptLineOffsetSetter,
                      attributes);
}


//
// Accessors::ScriptType
//


void Accessors::ScriptTypeGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* res = Script::cast(JSValue::cast(object)->value())->type();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


void Accessors::ScriptTypeSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptTypeInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("type")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptTypeGetter,
                      &ScriptTypeSetter,
                      attributes);
}


//
// Accessors::ScriptCompilationType
//


void Accessors::ScriptCompilationTypeGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* res = Smi::FromInt(
      Script::cast(JSValue::cast(object)->value())->compilation_type());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


void Accessors::ScriptCompilationTypeSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptCompilationTypeInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("compilation_type")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptCompilationTypeGetter,
                      &ScriptCompilationTypeSetter,
                      attributes);
}


//
// Accessors::ScriptGetLineEnds
//


void Accessors::ScriptLineEndsGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<Object> object = Utils::OpenHandle(*info.This());
  Handle<Script> script(
      Script::cast(Handle<JSValue>::cast(object)->value()), isolate);
  Script::InitLineEnds(script);
  DCHECK(script->line_ends()->IsFixedArray());
  Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
  // We do not want anyone to modify this array from JS.
  DCHECK(*line_ends == isolate->heap()->empty_fixed_array() ||
         line_ends->map() == isolate->heap()->fixed_cow_array_map());
  Handle<JSArray> js_array =
      isolate->factory()->NewJSArrayWithElements(line_ends);
  info.GetReturnValue().Set(Utils::ToLocal(js_array));
}


void Accessors::ScriptLineEndsSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptLineEndsInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("line_ends")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptLineEndsGetter,
                      &ScriptLineEndsSetter,
                      attributes);
}


//
// Accessors::ScriptSourceUrl
//


void Accessors::ScriptSourceUrlGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* url = Script::cast(JSValue::cast(object)->value())->source_url();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(url, isolate)));
}


void Accessors::ScriptSourceUrlSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptSourceUrlInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->source_url_string(),
                      &ScriptSourceUrlGetter,
                      &ScriptSourceUrlSetter,
                      attributes);
}


//
// Accessors::ScriptSourceMappingUrl
//


void Accessors::ScriptSourceMappingUrlGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* url =
      Script::cast(JSValue::cast(object)->value())->source_mapping_url();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(url, isolate)));
}


void Accessors::ScriptSourceMappingUrlSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptSourceMappingUrlInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->source_mapping_url_string(),
                      &ScriptSourceMappingUrlGetter,
                      &ScriptSourceMappingUrlSetter,
                      attributes);
}


//
// Accessors::ScriptGetContextData
//


void Accessors::ScriptContextDataGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  Object* res = Script::cast(JSValue::cast(object)->value())->context_data();
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


void Accessors::ScriptContextDataSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptContextDataInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("context_data")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptContextDataGetter,
                      &ScriptContextDataSetter,
                      attributes);
}


//
// Accessors::ScriptGetEvalFromScript
//


void Accessors::ScriptEvalFromScriptGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<Object> object = Utils::OpenHandle(*info.This());
  Handle<Script> script(
      Script::cast(Handle<JSValue>::cast(object)->value()), isolate);
  Handle<Object> result = isolate->factory()->undefined_value();
  if (!script->eval_from_shared()->IsUndefined()) {
    Handle<SharedFunctionInfo> eval_from_shared(
        SharedFunctionInfo::cast(script->eval_from_shared()));
    if (eval_from_shared->script()->IsScript()) {
      Handle<Script> eval_from_script(Script::cast(eval_from_shared->script()));
      result = Script::GetWrapper(eval_from_script);
    }
  }

  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::ScriptEvalFromScriptSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptEvalFromScriptInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_script")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptEvalFromScriptGetter,
                      &ScriptEvalFromScriptSetter,
                      attributes);
}


//
// Accessors::ScriptGetEvalFromScriptPosition
//


void Accessors::ScriptEvalFromScriptPositionGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<Object> object = Utils::OpenHandle(*info.This());
  Handle<Script> script(
      Script::cast(Handle<JSValue>::cast(object)->value()), isolate);
  Handle<Object> result = isolate->factory()->undefined_value();
  if (script->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
    Handle<Code> code(SharedFunctionInfo::cast(
        script->eval_from_shared())->code());
    result = Handle<Object>(
        Smi::FromInt(code->SourcePosition(code->instruction_start() +
                     script->eval_from_instructions_offset()->value())),
        isolate);
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::ScriptEvalFromScriptPositionSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptEvalFromScriptPositionInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_script_position")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptEvalFromScriptPositionGetter,
                      &ScriptEvalFromScriptPositionSetter,
                      attributes);
}


//
// Accessors::ScriptGetEvalFromFunctionName
//


void Accessors::ScriptEvalFromFunctionNameGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<Object> object = Utils::OpenHandle(*info.This());
  Handle<Script> script(
      Script::cast(Handle<JSValue>::cast(object)->value()), isolate);
  Handle<Object> result;
  Handle<SharedFunctionInfo> shared(
      SharedFunctionInfo::cast(script->eval_from_shared()));
  // Find the name of the function calling eval.
  if (!shared->name()->IsUndefined()) {
    result = Handle<Object>(shared->name(), isolate);
  } else {
    result = Handle<Object>(shared->inferred_name(), isolate);
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::ScriptEvalFromFunctionNameSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::ScriptEvalFromFunctionNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_function_name")));
  return MakeAccessor(isolate,
                      name,
                      &ScriptEvalFromFunctionNameGetter,
                      &ScriptEvalFromFunctionNameSetter,
                      attributes);
}


//
// Accessors::FunctionPrototype
//

static Handle<Object> GetFunctionPrototype(Isolate* isolate,
                                           Handle<JSFunction> function) {
  if (!function->has_prototype()) {
    Handle<Object> proto = isolate->factory()->NewFunctionPrototype(function);
    JSFunction::SetPrototype(function, proto);
  }
  return Handle<Object>(function->prototype(), isolate);
}


MUST_USE_RESULT static MaybeHandle<Object> SetFunctionPrototype(
    Isolate* isolate, Handle<JSFunction> function, Handle<Object> value) {
  Handle<Object> old_value;
  bool is_observed = function->map()->is_observed();
  if (is_observed) {
    if (function->has_prototype())
      old_value = handle(function->prototype(), isolate);
    else
      old_value = isolate->factory()->NewFunctionPrototype(function);
  }

  JSFunction::SetPrototype(function, value);
  DCHECK(function->prototype() == *value);

  if (is_observed && !old_value->SameValue(*value)) {
    MaybeHandle<Object> result = JSObject::EnqueueChangeRecord(
        function, "update", isolate->factory()->prototype_string(), old_value);
    if (result.is_null()) return MaybeHandle<Object>();
  }

  return function;
}


MaybeHandle<Object> Accessors::FunctionSetPrototype(Handle<JSFunction> function,
                                                    Handle<Object> prototype) {
  DCHECK(function->should_have_prototype());
  Isolate* isolate = function->GetIsolate();
  return SetFunctionPrototype(isolate, function, prototype);
}


void Accessors::FunctionPrototypeGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Object> result = GetFunctionPrototype(isolate, function);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::FunctionPrototypeSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<Object> value = Utils::OpenHandle(*val);
  if (SetPropertyOnInstanceIfInherited(isolate, info, name, value)) {
    return;
  }
  Handle<JSFunction> object =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  if (SetFunctionPrototype(isolate, object, value).is_null()) {
    isolate->OptionalRescheduleException(false);
  }
}


Handle<AccessorInfo> Accessors::FunctionPrototypeInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->prototype_string(),
                      &FunctionPrototypeGetter,
                      &FunctionPrototypeSetter,
                      attributes);
}


//
// Accessors::FunctionLength
//


void Accessors::FunctionLengthGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));

  int length = 0;
  if (function->shared()->is_compiled()) {
    length = function->shared()->length();
  } else {
    // If the function isn't compiled yet, the length is not computed
    // correctly yet. Compile it now and return the right length.
    if (Compiler::EnsureCompiled(function, KEEP_EXCEPTION)) {
      length = function->shared()->length();
    }
    if (isolate->has_pending_exception()) {
      isolate->OptionalRescheduleException(false);
    }
  }
  Handle<Object> result(Smi::FromInt(length), isolate);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::FunctionLengthSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  // Function length is non writable, non configurable.
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::FunctionLengthInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->length_string(),
                      &FunctionLengthGetter,
                      &FunctionLengthSetter,
                      attributes);
}


//
// Accessors::FunctionName
//


void Accessors::FunctionNameGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Object> result(function->shared()->name(), isolate);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::FunctionNameSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  // Function name is non writable, non configurable.
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::FunctionNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->name_string(),
                      &FunctionNameGetter,
                      &FunctionNameSetter,
                      attributes);
}


//
// Accessors::FunctionArguments
//


static Handle<Object> ArgumentsForInlinedFunction(
    JavaScriptFrame* frame,
    Handle<JSFunction> inlined_function,
    int inlined_frame_index) {
  Isolate* isolate = inlined_function->GetIsolate();
  Factory* factory = isolate->factory();
  SlotRefValueBuilder slot_refs(
      frame,
      inlined_frame_index,
      inlined_function->shared()->formal_parameter_count());

  int args_count = slot_refs.args_length();
  Handle<JSObject> arguments =
      factory->NewArgumentsObject(inlined_function, args_count);
  Handle<FixedArray> array = factory->NewFixedArray(args_count);
  slot_refs.Prepare(isolate);
  for (int i = 0; i < args_count; ++i) {
    Handle<Object> value = slot_refs.GetNext(isolate, 0);
    array->set(i, *value);
  }
  slot_refs.Finish(isolate);
  arguments->set_elements(*array);

  // Return the freshly allocated arguments object.
  return arguments;
}


static int FindFunctionInFrame(JavaScriptFrame* frame,
                               Handle<JSFunction> function) {
  DisallowHeapAllocation no_allocation;
  List<JSFunction*> functions(2);
  frame->GetFunctions(&functions);
  for (int i = functions.length() - 1; i >= 0; i--) {
    if (functions[i] == *function) return i;
  }
  return -1;
}


Handle<Object> GetFunctionArguments(Isolate* isolate,
                                    Handle<JSFunction> function) {
  if (function->shared()->native()) return isolate->factory()->null_value();

  // Find the top invocation of the function by traversing frames.
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    int function_index = FindFunctionInFrame(frame, function);
    if (function_index < 0) continue;

    if (function_index > 0) {
      // The function in question was inlined.  Inlined functions have the
      // correct number of arguments and no allocated arguments object, so
      // we can construct a fresh one by interpreting the function's
      // deoptimization input data.
      return ArgumentsForInlinedFunction(frame, function, function_index);
    }

    if (!frame->is_optimized()) {
      // If there is an arguments variable in the stack, we return that.
      Handle<ScopeInfo> scope_info(function->shared()->scope_info());
      int index = scope_info->StackSlotIndex(
          isolate->heap()->arguments_string());
      if (index >= 0) {
        Handle<Object> arguments(frame->GetExpression(index), isolate);
        if (!arguments->IsArgumentsMarker()) return arguments;
      }
    }

    // If there is no arguments variable in the stack or we have an
    // optimized frame, we find the frame that holds the actual arguments
    // passed to the function.
    it.AdvanceToArgumentsFrame();
    frame = it.frame();

    // Get the number of arguments and construct an arguments object
    // mirror for the right frame.
    const int length = frame->ComputeParametersCount();
    Handle<JSObject> arguments = isolate->factory()->NewArgumentsObject(
        function, length);
    Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);

    // Copy the parameters to the arguments object.
    DCHECK(array->length() == length);
    for (int i = 0; i < length; i++) array->set(i, frame->GetParameter(i));
    arguments->set_elements(*array);

    // Return the freshly allocated arguments object.
    return arguments;
  }

  // No frame corresponding to the given function found. Return null.
  return isolate->factory()->null_value();
}


Handle<Object> Accessors::FunctionGetArguments(Handle<JSFunction> function) {
  return GetFunctionArguments(function->GetIsolate(), function);
}


void Accessors::FunctionArgumentsGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Object> result = GetFunctionArguments(isolate, function);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::FunctionArgumentsSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  // Function arguments is non writable, non configurable.
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::FunctionArgumentsInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->arguments_string(),
                      &FunctionArgumentsGetter,
                      &FunctionArgumentsSetter,
                      attributes);
}


//
// Accessors::FunctionCaller
//


static inline bool AllowAccessToFunction(Context* current_context,
                                         JSFunction* function) {
  return current_context->HasSameSecurityTokenAs(function->context());
}


class FrameFunctionIterator {
 public:
  FrameFunctionIterator(Isolate* isolate, const DisallowHeapAllocation& promise)
      : isolate_(isolate),
        frame_iterator_(isolate),
        functions_(2),
        index_(0) {
    GetFunctions();
  }
  JSFunction* next() {
    while (true) {
      if (functions_.length() == 0) return NULL;
      JSFunction* next_function = functions_[index_];
      index_--;
      if (index_ < 0) {
        GetFunctions();
      }
      // Skip functions from other origins.
      if (!AllowAccessToFunction(isolate_->context(), next_function)) continue;
      return next_function;
    }
  }

  // Iterate through functions until the first occurence of 'function'.
  // Returns true if 'function' is found, and false if the iterator ends
  // without finding it.
  bool Find(JSFunction* function) {
    JSFunction* next_function;
    do {
      next_function = next();
      if (next_function == function) return true;
    } while (next_function != NULL);
    return false;
  }

 private:
  void GetFunctions() {
    functions_.Rewind(0);
    if (frame_iterator_.done()) return;
    JavaScriptFrame* frame = frame_iterator_.frame();
    frame->GetFunctions(&functions_);
    DCHECK(functions_.length() > 0);
    frame_iterator_.Advance();
    index_ = functions_.length() - 1;
  }
  Isolate* isolate_;
  JavaScriptFrameIterator frame_iterator_;
  List<JSFunction*> functions_;
  int index_;
};


MaybeHandle<JSFunction> FindCaller(Isolate* isolate,
                                   Handle<JSFunction> function) {
  DisallowHeapAllocation no_allocation;
  FrameFunctionIterator it(isolate, no_allocation);
  if (function->shared()->native()) {
    return MaybeHandle<JSFunction>();
  }
  // Find the function from the frames.
  if (!it.Find(*function)) {
    // No frame corresponding to the given function found. Return null.
    return MaybeHandle<JSFunction>();
  }
  // Find previously called non-toplevel function.
  JSFunction* caller;
  do {
    caller = it.next();
    if (caller == NULL) return MaybeHandle<JSFunction>();
  } while (caller->shared()->is_toplevel());

  // If caller is a built-in function and caller's caller is also built-in,
  // use that instead.
  JSFunction* potential_caller = caller;
  while (potential_caller != NULL && potential_caller->IsBuiltin()) {
    caller = potential_caller;
    potential_caller = it.next();
  }
  if (!caller->shared()->native() && potential_caller != NULL) {
    caller = potential_caller;
  }
  // If caller is bound, return null. This is compatible with JSC, and
  // allows us to make bound functions use the strict function map
  // and its associated throwing caller and arguments.
  if (caller->shared()->bound()) {
    return MaybeHandle<JSFunction>();
  }
  // Censor if the caller is not a sloppy mode function.
  // Change from ES5, which used to throw, see:
  // https://bugs.ecmascript.org/show_bug.cgi?id=310
  if (caller->shared()->strict_mode() == STRICT) {
    return MaybeHandle<JSFunction>();
  }
  // Don't return caller from another security context.
  if (!AllowAccessToFunction(isolate->context(), caller)) {
    return MaybeHandle<JSFunction>();
  }
  return Handle<JSFunction>(caller);
}


void Accessors::FunctionCallerGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Object> result;
  MaybeHandle<JSFunction> maybe_caller;
  maybe_caller = FindCaller(isolate, function);
  Handle<JSFunction> caller;
  if (maybe_caller.ToHandle(&caller)) {
    result = caller;
  } else {
    result = isolate->factory()->null_value();
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


void Accessors::FunctionCallerSetter(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  // Function caller is non writable, non configurable.
  UNREACHABLE();
}


Handle<AccessorInfo> Accessors::FunctionCallerInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate,
                      isolate->factory()->caller_string(),
                      &FunctionCallerGetter,
                      &FunctionCallerSetter,
                      attributes);
}


//
// Accessors::MakeModuleExport
//

static void ModuleGetExport(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  JSModule* instance = JSModule::cast(*v8::Utils::OpenHandle(*info.Holder()));
  Context* context = Context::cast(instance->context());
  DCHECK(context->IsModuleContext());
  int slot = info.Data()->Int32Value();
  Object* value = context->get(slot);
  Isolate* isolate = instance->GetIsolate();
  if (value->IsTheHole()) {
    Handle<String> name = v8::Utils::OpenHandle(*property);

    Handle<Object> exception;
    MaybeHandle<Object> maybe = isolate->factory()->NewReferenceError(
        "not_defined", HandleVector(&name, 1));
    if (!maybe.ToHandle(&exception)) {
      isolate->OptionalRescheduleException(false);
      return;
    }

    isolate->ScheduleThrow(*exception);
    return;
  }
  info.GetReturnValue().Set(v8::Utils::ToLocal(Handle<Object>(value, isolate)));
}


static void ModuleSetExport(
    v8::Local<v8::String> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  JSModule* instance = JSModule::cast(*v8::Utils::OpenHandle(*info.Holder()));
  Context* context = Context::cast(instance->context());
  DCHECK(context->IsModuleContext());
  int slot = info.Data()->Int32Value();
  Object* old_value = context->get(slot);
  Isolate* isolate = context->GetIsolate();
  if (old_value->IsTheHole()) {
    Handle<String> name = v8::Utils::OpenHandle(*property);
    Handle<Object> exception;
    MaybeHandle<Object> maybe = isolate->factory()->NewReferenceError(
        "not_defined", HandleVector(&name, 1));
    if (!maybe.ToHandle(&exception)) {
      isolate->OptionalRescheduleException(false);
      return;
    }

    isolate->ScheduleThrow(*exception);
    return;
  }
  context->set(slot, *v8::Utils::OpenHandle(*value));
}


Handle<AccessorInfo> Accessors::MakeModuleExport(
    Handle<String> name,
    int index,
    PropertyAttributes attributes) {
  Isolate* isolate = name->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<ExecutableAccessorInfo> info = factory->NewExecutableAccessorInfo();
  info->set_property_attributes(attributes);
  info->set_all_can_read(true);
  info->set_all_can_write(true);
  info->set_name(*name);
  info->set_data(Smi::FromInt(index));
  Handle<Object> getter = v8::FromCData(isolate, &ModuleGetExport);
  Handle<Object> setter = v8::FromCData(isolate, &ModuleSetExport);
  info->set_getter(*getter);
  if (!(attributes & ReadOnly)) info->set_setter(*setter);
  return info;
}


} }  // namespace v8::internal
