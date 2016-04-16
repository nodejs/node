// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/accessors.h"

#include "src/api.h"
#include "src/contexts.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/list-inl.h"
#include "src/messages.h"
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
  Handle<AccessorInfo> info = factory->NewAccessorInfo();
  info->set_property_attributes(attributes);
  info->set_all_can_read(false);
  info->set_all_can_write(false);
  info->set_is_special_data_property(true);
  name = factory->InternalizeName(name);
  info->set_name(*name);
  Handle<Object> get = v8::FromCData(isolate, getter);
  if (setter == nullptr) setter = &ReconfigureToDataProperty;
  Handle<Object> set = v8::FromCData(isolate, setter);
  info->set_getter(*get);
  info->set_setter(*set);
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
bool Accessors::IsJSObjectFieldAccessor(Handle<Map> map, Handle<Name> name,
                                        int* object_offset) {
  Isolate* isolate = name->GetIsolate();

  switch (map->instance_type()) {
    case JS_ARRAY_TYPE:
      return
        CheckForName(name, isolate->factory()->length_string(),
                     JSArray::kLengthOffset, object_offset);
    case JS_ARRAY_BUFFER_TYPE:
      return CheckForName(name, isolate->factory()->byte_length_string(),
                          JSArrayBuffer::kByteLengthOffset, object_offset);
    default:
      if (map->instance_type() < FIRST_NONSTRING_TYPE) {
        return CheckForName(name, isolate->factory()->length_string(),
                            String::kLengthOffset, object_offset);
      }

      return false;
  }
}


bool Accessors::IsJSArrayBufferViewFieldAccessor(Handle<Map> map,
                                                 Handle<Name> name,
                                                 int* object_offset) {
  DCHECK(name->IsUniqueName());
  Isolate* isolate = name->GetIsolate();

  switch (map->instance_type()) {
    case JS_TYPED_ARRAY_TYPE: {
      if (!CheckForName(name, isolate->factory()->length_string(),
                        JSTypedArray::kLengthOffset, object_offset) &&
          !CheckForName(name, isolate->factory()->byte_length_string(),
                        JSTypedArray::kByteLengthOffset, object_offset) &&
          !CheckForName(name, isolate->factory()->byte_offset_string(),
                        JSTypedArray::kByteOffsetOffset, object_offset)) {
        return false;
      }

      if (map->is_dictionary_map()) return false;

      // Check if the property is overridden on the instance.
      DescriptorArray* descriptors = map->instance_descriptors();
      int descriptor = descriptors->SearchWithCache(isolate, *name, *map);
      if (descriptor != DescriptorArray::kNotFound) return false;

      Handle<Object> proto = Handle<Object>(map->prototype(), isolate);
      if (!proto->IsJSReceiver()) return false;

      // Check if the property is defined in the prototype chain.
      LookupIterator it(proto, name);
      if (!it.IsFound()) return false;

      Object* original_proto =
          JSFunction::cast(map->GetConstructor())->prototype();

      // Property is not configurable. It is enough to verify that
      // the holder is the same.
      return *it.GetHolder<Object>() == original_proto;
    }
    case JS_DATA_VIEW_TYPE:
      return CheckForName(name, isolate->factory()->byte_length_string(),
                          JSDataView::kByteLengthOffset, object_offset) ||
             CheckForName(name, isolate->factory()->byte_offset_string(),
                          JSDataView::kByteOffsetOffset, object_offset);
    default:
      return false;
  }
}

MUST_USE_RESULT static MaybeHandle<Object> ReplaceAccessorWithDataProperty(
    Isolate* isolate, Handle<JSObject> receiver, Handle<JSObject> holder,
    Handle<Name> name, Handle<Object> value, bool observe) {
  LookupIterator it(receiver, name, holder,
                    LookupIterator::OWN_SKIP_INTERCEPTOR);
  // Skip any access checks we might hit. This accessor should never hit in a
  // situation where the caller does not have access.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    CHECK(it.HasAccess());
    it.Next();
  }
  CHECK_EQ(LookupIterator::ACCESSOR, it.state());

  Handle<Object> old_value;
  bool is_observed = observe && receiver->map()->is_observed();
  if (is_observed) {
    MaybeHandle<Object> maybe_old = Object::GetPropertyWithAccessor(&it);
    if (!maybe_old.ToHandle(&old_value)) return maybe_old;
  }

  it.ReconfigureDataProperty(value, it.property_attributes());

  if (is_observed && !old_value->SameValue(*value)) {
    return JSObject::EnqueueChangeRecord(receiver, "update", name, old_value);
  }

  return value;
}

void Accessors::ReconfigureToDataProperty(
    v8::Local<v8::Name> key, v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSObject> receiver =
      Handle<JSObject>::cast(Utils::OpenHandle(*info.This()));
  Handle<JSObject> holder =
      Handle<JSObject>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Name> name = Utils::OpenHandle(*key);
  Handle<Object> value = Utils::OpenHandle(*val);
  MaybeHandle<Object> result = ReplaceAccessorWithDataProperty(
      isolate, receiver, holder, name, value, false);
  if (result.is_null()) isolate->OptionalRescheduleException(false);
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


Handle<AccessorInfo> Accessors::ArgumentsIteratorInfo(
    Isolate* isolate, PropertyAttributes attributes) {
  Handle<Name> name = isolate->factory()->iterator_symbol();
  return MakeAccessor(isolate, name, &ArgumentsIteratorGetter, nullptr,
                      attributes);
}


//
// Accessors::ArrayLength
//


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

  Handle<JSReceiver> object = Utils::OpenHandle(*info.This());
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  Handle<Object> length_obj = Utils::OpenHandle(*val);

  uint32_t length = 0;
  if (!JSArray::AnythingToArrayLength(isolate, length_obj, &length)) {
    isolate->OptionalRescheduleException(false);
    return;
  }

  if (JSArray::ObservableSetLength(array, length).is_null()) {
    isolate->OptionalRescheduleException(false);
  }

  if (info.ShouldThrowOnError()) {
    uint32_t actual_new_len = 0;
    CHECK(array->length()->ToArrayLength(&actual_new_len));
    // Throw TypeError if there were non-deletable elements.
    if (actual_new_len != length) {
      Factory* factory = isolate->factory();
      isolate->Throw(*factory->NewTypeError(
          MessageTemplate::kStrictDeleteProperty,
          factory->NewNumberFromUint(actual_new_len - 1), array));
      isolate->OptionalRescheduleException(false);
    }
  }
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


Handle<AccessorInfo> Accessors::StringLengthInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->length_string(),
                      &StringLengthGetter, nullptr, attributes);
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
  Object* res = Smi::FromInt(
      Script::cast(JSValue::cast(object)->value())->column_offset());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


Handle<AccessorInfo> Accessors::ScriptColumnOffsetInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("column_offset")));
  return MakeAccessor(isolate, name, &ScriptColumnOffsetGetter, nullptr,
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
  Object* id = Smi::FromInt(Script::cast(JSValue::cast(object)->value())->id());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(id, isolate)));
}


Handle<AccessorInfo> Accessors::ScriptIdInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("id")));
  return MakeAccessor(isolate, name, &ScriptIdGetter, nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->name_string(),
                      &ScriptNameGetter, nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptSourceInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->source_string(),
                      &ScriptSourceGetter, nullptr, attributes);
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
  Object* res =
      Smi::FromInt(Script::cast(JSValue::cast(object)->value())->line_offset());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


Handle<AccessorInfo> Accessors::ScriptLineOffsetInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("line_offset")));
  return MakeAccessor(isolate, name, &ScriptLineOffsetGetter, nullptr,
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
  Object* res =
      Smi::FromInt(Script::cast(JSValue::cast(object)->value())->type());
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


Handle<AccessorInfo> Accessors::ScriptTypeInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(
      isolate->factory()->InternalizeOneByteString(STATIC_CHAR_VECTOR("type")));
  return MakeAccessor(isolate, name, &ScriptTypeGetter, nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptCompilationTypeInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("compilation_type")));
  return MakeAccessor(isolate, name, &ScriptCompilationTypeGetter, nullptr,
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


Handle<AccessorInfo> Accessors::ScriptLineEndsInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("line_ends")));
  return MakeAccessor(isolate, name, &ScriptLineEndsGetter, nullptr,
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


Handle<AccessorInfo> Accessors::ScriptSourceUrlInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->source_url_string(),
                      &ScriptSourceUrlGetter, nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptSourceMappingUrlInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->source_mapping_url_string(),
                      &ScriptSourceMappingUrlGetter, nullptr, attributes);
}


//
// Accessors::ScriptIsEmbedderDebugScript
//


void Accessors::ScriptIsEmbedderDebugScriptGetter(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  DisallowHeapAllocation no_allocation;
  HandleScope scope(isolate);
  Object* object = *Utils::OpenHandle(*info.This());
  bool is_embedder_debug_script = Script::cast(JSValue::cast(object)->value())
                                      ->origin_options()
                                      .IsEmbedderDebugScript();
  Object* res = *isolate->factory()->ToBoolean(is_embedder_debug_script);
  info.GetReturnValue().Set(Utils::ToLocal(Handle<Object>(res, isolate)));
}


Handle<AccessorInfo> Accessors::ScriptIsEmbedderDebugScriptInfo(
    Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("is_debugger_script")));
  return MakeAccessor(isolate, name, &ScriptIsEmbedderDebugScriptGetter,
                      nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptContextDataInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("context_data")));
  return MakeAccessor(isolate, name, &ScriptContextDataGetter, nullptr,
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


Handle<AccessorInfo> Accessors::ScriptEvalFromScriptInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_script")));
  return MakeAccessor(isolate, name, &ScriptEvalFromScriptGetter, nullptr,
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
    result = Handle<Object>(Smi::FromInt(code->SourcePosition(
                                script->eval_from_instructions_offset())),
                            isolate);
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


Handle<AccessorInfo> Accessors::ScriptEvalFromScriptPositionInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_script_position")));
  return MakeAccessor(isolate, name, &ScriptEvalFromScriptPositionGetter,
                      nullptr, attributes);
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


Handle<AccessorInfo> Accessors::ScriptEvalFromFunctionNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  Handle<String> name(isolate->factory()->InternalizeOneByteString(
      STATIC_CHAR_VECTOR("eval_from_function_name")));
  return MakeAccessor(isolate, name, &ScriptEvalFromFunctionNameGetter, nullptr,
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
  DCHECK(function->IsConstructor());
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
    if (Compiler::Compile(function, KEEP_EXCEPTION)) {
      length = function->shared()->length();
    }
    if (isolate->has_pending_exception()) {
      isolate->OptionalRescheduleException(false);
    }
  }
  Handle<Object> result(Smi::FromInt(length), isolate);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}

void Accessors::ObservedReconfigureToDataProperty(
    v8::Local<v8::Name> key, v8::Local<v8::Value> val,
    const v8::PropertyCallbackInfo<void>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSObject> receiver =
      Handle<JSObject>::cast(Utils::OpenHandle(*info.This()));
  Handle<JSObject> holder =
      Handle<JSObject>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Name> name = Utils::OpenHandle(*key);
  Handle<Object> value = Utils::OpenHandle(*val);
  MaybeHandle<Object> result = ReplaceAccessorWithDataProperty(
      isolate, receiver, holder, name, value, true);
  if (result.is_null()) isolate->OptionalRescheduleException(false);
}


Handle<AccessorInfo> Accessors::FunctionLengthInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->length_string(),
                      &FunctionLengthGetter, &ObservedReconfigureToDataProperty,
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
  Handle<Object> result;
  if (function->shared()->name_should_print_as_anonymous()) {
    result = isolate->factory()->anonymous_string();
  } else {
    result = handle(function->shared()->name(), isolate);
  }
  info.GetReturnValue().Set(Utils::ToLocal(result));
}

Handle<AccessorInfo> Accessors::FunctionNameInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->name_string(),
                      &FunctionNameGetter, &ObservedReconfigureToDataProperty,
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

  TranslatedState translated_values(frame);
  translated_values.Prepare(false, frame->fp());

  int argument_count = 0;
  TranslatedFrame* translated_frame =
      translated_values.GetArgumentsInfoFromJSFrameIndex(inlined_frame_index,
                                                         &argument_count);
  TranslatedFrame::iterator iter = translated_frame->begin();

  // Skip the function.
  iter++;

  // Skip the receiver.
  iter++;
  argument_count--;

  Handle<JSObject> arguments =
      factory->NewArgumentsObject(inlined_function, argument_count);
  Handle<FixedArray> array = factory->NewFixedArray(argument_count);
  bool should_deoptimize = false;
  for (int i = 0; i < argument_count; ++i) {
    // If we materialize any object, we should deopt because we might alias
    // an object that was eliminated by escape analysis.
    should_deoptimize = should_deoptimize || iter->IsMaterializedObject();
    Handle<Object> value = iter->GetValue();
    array->set(i, *value);
    iter++;
  }
  arguments->set_elements(*array);

  if (should_deoptimize) {
    translated_values.StoreMaterializedValuesAndDeopt();
  }

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


namespace {

Handle<Object> GetFunctionArguments(Isolate* isolate,
                                    Handle<JSFunction> function) {
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

    // Find the frame that holds the actual arguments passed to the function.
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

}  // namespace


Handle<JSObject> Accessors::FunctionGetArguments(Handle<JSFunction> function) {
  Handle<Object> arguments =
      GetFunctionArguments(function->GetIsolate(), function);
  CHECK(arguments->IsJSObject());
  return Handle<JSObject>::cast(arguments);
}


void Accessors::FunctionArgumentsGetter(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(info.GetIsolate());
  HandleScope scope(isolate);
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(Utils::OpenHandle(*info.Holder()));
  Handle<Object> result =
      function->shared()->native()
          ? Handle<Object>::cast(isolate->factory()->null_value())
          : GetFunctionArguments(isolate, function);
  info.GetReturnValue().Set(Utils::ToLocal(result));
}


Handle<AccessorInfo> Accessors::FunctionArgumentsInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->arguments_string(),
                      &FunctionArgumentsGetter, nullptr, attributes);
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
  while (potential_caller != NULL && potential_caller->shared()->IsBuiltin()) {
    caller = potential_caller;
    potential_caller = it.next();
  }
  if (!caller->shared()->native() && potential_caller != NULL) {
    caller = potential_caller;
  }
  // Censor if the caller is not a sloppy mode function.
  // Change from ES5, which used to throw, see:
  // https://bugs.ecmascript.org/show_bug.cgi?id=310
  if (is_strict(caller->shared()->language_mode())) {
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


Handle<AccessorInfo> Accessors::FunctionCallerInfo(
      Isolate* isolate, PropertyAttributes attributes) {
  return MakeAccessor(isolate, isolate->factory()->caller_string(),
                      &FunctionCallerGetter, nullptr, attributes);
}


//
// Accessors::MakeModuleExport
//

static void ModuleGetExport(v8::Local<v8::Name> property,
                            const v8::PropertyCallbackInfo<v8::Value>& info) {
  JSModule* instance = JSModule::cast(*v8::Utils::OpenHandle(*info.Holder()));
  Context* context = Context::cast(instance->context());
  DCHECK(context->IsModuleContext());
  Isolate* isolate = instance->GetIsolate();
  int slot = info.Data()
                 ->Int32Value(info.GetIsolate()->GetCurrentContext())
                 .FromMaybe(-1);
  if (slot < 0 || slot >= context->length()) {
    Handle<Name> name = v8::Utils::OpenHandle(*property);

    Handle<Object> exception = isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, name);
    isolate->ScheduleThrow(*exception);
    return;
  }
  Object* value = context->get(slot);
  if (value->IsTheHole()) {
    Handle<Name> name = v8::Utils::OpenHandle(*property);

    Handle<Object> exception = isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, name);
    isolate->ScheduleThrow(*exception);
    return;
  }
  info.GetReturnValue().Set(v8::Utils::ToLocal(Handle<Object>(value, isolate)));
}


static void ModuleSetExport(v8::Local<v8::Name> property,
                            v8::Local<v8::Value> value,
                            const v8::PropertyCallbackInfo<void>& info) {
  if (!info.ShouldThrowOnError()) return;
  Handle<Name> name = v8::Utils::OpenHandle(*property);
  Isolate* isolate = name->GetIsolate();
  Handle<Object> exception =
      isolate->factory()->NewTypeError(MessageTemplate::kNotDefined, name);
  isolate->ScheduleThrow(*exception);
}


Handle<AccessorInfo> Accessors::MakeModuleExport(
    Handle<String> name,
    int index,
    PropertyAttributes attributes) {
  Isolate* isolate = name->GetIsolate();
  Handle<AccessorInfo> info = MakeAccessor(isolate, name, &ModuleGetExport,
                                           &ModuleSetExport, attributes);
  info->set_data(Smi::FromInt(index));
  return info;
}


}  // namespace internal
}  // namespace v8
