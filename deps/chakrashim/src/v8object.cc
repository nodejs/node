// Copyright Microsoft. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and / or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include "v8chakra.h"
#include <cassert>
#include <memory>

namespace v8 {

using std::unique_ptr;
using namespace jsrt;

enum AccessorType {
  Setter = 0,
  Getter = 1
};

typedef struct AcessorExternalDataType {
  Persistent<Value> data;
  Persistent<AccessorSignature> signature;
  AccessorType type;
  union {
    AccessorNameGetterCallback getter;
    AccessorNameSetterCallback setter;
  };
  Persistent<Name> propertyName;

  bool CheckSignature(Local<Object> thisPointer, Local<Object>* holder) {
    if (signature.IsEmpty()) {
      *holder = thisPointer;
      return true;
    }

    Local<FunctionTemplate> receiver = signature.As<FunctionTemplate>();
    return Utils::CheckSignature(receiver, thisPointer, holder);
  }
} AccessorExternalData;

struct InternalFieldDataStruct {
  void** internalFieldPointers;
  int size;
};

Maybe<bool> Object::Set(Local<Context> context,
                        Local<Value> key, Local<Value> value) {
  return Set(key, value, PropertyAttribute::None, /*force*/false);
}

bool Object::Set(Handle<Value> key, Handle<Value> value) {
  return FromMaybe(Set(Local<Context>(), key, value));
}

Maybe<bool> Object::Set(Handle<Value> key, Handle<Value> value,
                        PropertyAttribute attribs, bool force) {
  JsPropertyIdRef idRef;

  if (GetPropertyIdFromValue((JsValueRef)*key, &idRef) != JsNoError) {
    return Nothing<bool>();
  }

  // Do it faster if there are no property attributes
  if (!force && attribs == None) {
    if (JsSetProperty((JsValueRef)this,
                      idRef, (JsValueRef)*value, false) != JsNoError) {
      return Nothing<bool>();
    }
  } else {  // we have attributes just use it
    PropertyDescriptorOptionValues writable =
      PropertyDescriptorOptionValues::False;

    if ((attribs & ReadOnly) == 0) {
      writable = PropertyDescriptorOptionValues::True;
    }

    PropertyDescriptorOptionValues enumerable =
      PropertyDescriptorOptionValues::False;

    if ((attribs & DontEnum) == 0) {
      enumerable = PropertyDescriptorOptionValues::True;
    }

    PropertyDescriptorOptionValues configurable =
      PropertyDescriptorOptionValues::False;

    if ((attribs & DontDelete) == 0) {
      configurable = PropertyDescriptorOptionValues::True;
    }


    if (DefineProperty((JsValueRef)this,
                       idRef,
                       writable,
                       enumerable,
                       configurable,
                       (JsValueRef)*value,
                       JS_INVALID_REFERENCE,
                       JS_INVALID_REFERENCE) != JsNoError) {
      return Nothing<bool>();
    }
  }

  return Just(true);
}

Maybe<bool> Object::Set(Local<Context> context,
                        uint32_t index, Local<Value> value) {
  if (SetIndexedProperty((JsValueRef)this,
                         index, (JsValueRef)(*value)) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(true);
}

bool Object::Set(uint32_t index, Handle<Value> value) {
  return FromMaybe(Set(Local<Context>(), index, value));
}

Maybe<bool> Object::ForceSet(Local<Context> context,
                             Local<Value> key, Local<Value> value,
                             PropertyAttribute attribs) {
  return Set(key, value, attribs, /*force*/true);
}

bool Object::ForceSet(Handle<Value> key, Handle<Value> value,
                      PropertyAttribute attribs) {
  return FromMaybe(ForceSet(Local<Context>(), key, value, attribs));
}

MaybeLocal<Value> Object::Get(Local<Context> context, Local<Value> key) {
  JsPropertyIdRef idRef;
  if (GetPropertyIdFromValue((JsValueRef)*key, &idRef) != JsNoError) {
    return Local<Value>();
  }

  JsValueRef valueRef;
  if (JsGetProperty((JsValueRef)this, idRef, &valueRef) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(valueRef);
}

Local<Value> Object::Get(Handle<Value> key) {
  return FromMaybe(Get(Local<Context>(), key));
}

MaybeLocal<Value> Object::Get(Local<Context> context, uint32_t index) {
  JsValueRef valueRef;

  if (GetIndexedProperty((JsValueRef)this, index, &valueRef) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(valueRef);
}

Local<Value> Object::Get(uint32_t index) {
  return FromMaybe(Get(Local<Context>(), index));
}

Maybe<PropertyAttribute> Object::GetPropertyAttributes(Local<Context> context,
                                                       Local<Value> key) {
  JsValueRef desc;
  if (jsrt::GetOwnPropertyDescriptor(this, *key, &desc) != JsNoError) {
    return Nothing<PropertyAttribute>();
  }

  IsolateShim* iso = IsolateShim::FromIsolate(GetIsolate());
  PropertyAttribute attr = PropertyAttribute::None;
  JsValueRef value;

  if (JsGetProperty(desc,
                    iso->GetCachedPropertyIdRef(
                      CachedPropertyIdRef::enumerable),
                    &value) != JsNoError) {
    return Nothing<PropertyAttribute>();
  }
  if (!Local<Value>(value)->BooleanValue()) {
    attr = static_cast<PropertyAttribute>(attr | PropertyAttribute::DontEnum);
  }

  if (JsGetProperty(desc,
                    iso->GetCachedPropertyIdRef(
                      CachedPropertyIdRef::configurable),
                    &value) != JsNoError) {
    return Nothing<PropertyAttribute>();
  }
  if (!Local<Value>(value)->BooleanValue()) {
    attr = static_cast<PropertyAttribute>(attr | PropertyAttribute::DontDelete);
  }

  if (JsGetProperty(desc,
                    iso->GetCachedPropertyIdRef(
                      CachedPropertyIdRef::writable),
                    &value) != JsNoError) {
    return Nothing<PropertyAttribute>();
  }
  if (!Local<Value>(value)->IsUndefined() &&
      !Local<Value>(value)->BooleanValue()) {
    attr = static_cast<PropertyAttribute>(attr | PropertyAttribute::ReadOnly);
  }

  return Just(attr);
}

PropertyAttribute Object::GetPropertyAttributes(Handle<Value> key) {
  return FromMaybe(GetPropertyAttributes(Local<Context>(), key));
}

MaybeLocal<Value> Object::GetOwnPropertyDescriptor(Local<Context> context,
                                                   Local<String> key) {
  JsValueRef result;
  if (jsrt::GetOwnPropertyDescriptor(this, *key, &result) != JsNoError) {
    return Local<Value>();
  }
  return Local<Value>::New(result);
}

Local<Value> Object::GetOwnPropertyDescriptor(Local<String> key) {
  return FromMaybe(GetOwnPropertyDescriptor(Local<Context>(), key));
}

Maybe<bool> Object::Has(Local<Context> context, Local<Value> key) {
  bool result;
  if (jsrt::HasProperty(this, *key, &result) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(result);
}

bool Object::Has(Handle<Value> key) {
  return FromMaybe(Has(Local<Context>(), key));
}

Maybe<bool> Object::Delete(Local<Context> context, Local<Value> key) {
  JsValueRef resultRef;
  if (jsrt::DeleteProperty(this, *key, &resultRef) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(Local<Value>(resultRef)->BooleanValue());
}

bool Object::Delete(Handle<Value> key) {
  return FromMaybe(Delete(Local<Context>(), key));
}

Maybe<bool> Object::Has(Local<Context> context, uint32_t index) {
  bool result;
  if (jsrt::HasIndexedProperty(this, index, &result) != JsNoError) {
    return Nothing<bool>();
  }
  return Just(result);
}

bool Object::Has(uint32_t index) {
  return FromMaybe(Has(Local<Context>(), index));
}

Maybe<bool> Object::Delete(Local<Context> context, uint32_t index) {
  if (DeleteIndexedProperty((JsValueRef)this, index) != JsNoError) {
    return Nothing<bool>();
  }
  return Just(true);
}

bool Object::Delete(uint32_t index) {
  return FromMaybe(Delete(Local<Context>(), index));
}

void CALLBACK AcessorExternalObjectFinalizeCallback(void *data) {
  if (data != nullptr) {
    AccessorExternalData *accessorData =
      static_cast<AccessorExternalData*>(data);
    delete accessorData;
  }
}

Maybe<bool> Object::SetAccessor(Handle<Name> name,
                                AccessorNameGetterCallback getter,
                                AccessorNameSetterCallback setter,
                                v8::Handle<Value> data,
                                AccessControl settings,
                                PropertyAttribute attributes,
                                Handle<AccessorSignature> signature) {
  JsValueRef getterRef = JS_INVALID_REFERENCE;
  JsValueRef setterRef = JS_INVALID_REFERENCE;

  JsPropertyIdRef idRef;
  if (GetPropertyIdFromName((JsValueRef)*name, &idRef) != JsNoError) {
    return Nothing<bool>();
  }

  if (getter != nullptr) {
    AccessorExternalData *externalData = new AccessorExternalData();
    externalData->type = Getter;
    externalData->propertyName = name;
    externalData->getter = getter;
    externalData->data = data;
    externalData->signature = signature;

    if (CreateFunctionWithExternalData(Utils::AccessorHandler,
                                       externalData,
                                       AcessorExternalObjectFinalizeCallback,
                                       &getterRef) != JsNoError) {
      delete externalData;
      return Nothing<bool>();
    }
  }

  if (setter != nullptr) {
    AccessorExternalData *externalData = new AccessorExternalData();
    externalData->type = Setter;
    externalData->propertyName = name;
    externalData->setter = setter;
    externalData->data = data;
    externalData->signature = signature;

    if (CreateFunctionWithExternalData(Utils::AccessorHandler,
                                       externalData,
                                       AcessorExternalObjectFinalizeCallback,
                                       &setterRef) != JsNoError) {
      delete externalData;
      return Nothing<bool>();
    }
  }

  PropertyDescriptorOptionValues enumerable =
    GetPropertyDescriptorOptionValue(!(attributes & DontEnum));
  PropertyDescriptorOptionValues configurable =
    GetPropertyDescriptorOptionValue(!(attributes & DontDelete));

  // CHAKRA-TODO: we ignore  AccessControl for now..

  if (DefineProperty((JsValueRef)this,
                     idRef,
                     PropertyDescriptorOptionValues::None,
                     enumerable,
                     configurable,
                     JS_INVALID_REFERENCE,
                     getterRef,
                     setterRef) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(true);
}

Maybe<bool> Object::SetAccessor(Local<Context> context,
                                Local<Name> name,
                                AccessorNameGetterCallback getter,
                                AccessorNameSetterCallback setter,
                                MaybeLocal<Value> data,
                                AccessControl settings,
                                PropertyAttribute attribute) {
  return SetAccessor(name, getter, setter, FromMaybe(data), settings, attribute,
                     Local<AccessorSignature>());
}

bool Object::SetAccessor(Handle<String> name,
                         AccessorGetterCallback getter,
                         AccessorSetterCallback setter,
                         v8::Handle<Value> data,
                         AccessControl settings,
                         PropertyAttribute attributes) {
  return FromMaybe(SetAccessor(
    name,
    reinterpret_cast<AccessorNameGetterCallback>(getter),
    reinterpret_cast<AccessorNameSetterCallback>(setter),
    data, settings, attributes, Handle<AccessorSignature>()));
}

bool Object::SetAccessor(Handle<Name> name,
                         AccessorNameGetterCallback getter,
                         AccessorNameSetterCallback setter,
                         Handle<Value> data,
                         AccessControl settings,
                         PropertyAttribute attribute) {
  return FromMaybe(SetAccessor(
    Local<Context>(), name, getter, setter, data, settings, attribute));
}

MaybeLocal<Array> Object::GetPropertyNames(Local<Context> context) {
  JsValueRef arrayRef;

  if (jsrt::GetPropertyNames((JsValueRef)this, &arrayRef) != JsNoError) {
    return Local<Array>();
  }

  return Local<Array>::New(arrayRef);
}

Local<Array> Object::GetPropertyNames() {
  return FromMaybe(GetPropertyNames(Local<Context>()));
}

MaybeLocal<Array> Object::GetOwnPropertyNames(Local<Context> context) {
  JsValueRef arrayRef;

  if (JsGetOwnPropertyNames((JsValueRef)this, &arrayRef) != JsNoError) {
    return Local<Array>();
  }

  return Local<Array>::New(arrayRef);
}

Local<Array> Object::GetOwnPropertyNames() {
  return FromMaybe(GetOwnPropertyNames(Local<Context>()));
}

Local<Value> Object::GetPrototype() {
  JsValueRef protoypeRef;

  if (JsGetPrototype((JsValueRef)this, &protoypeRef) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(static_cast<Value*>(protoypeRef));
}

Maybe<bool> Object::SetPrototype(Local<Context> context,
                                 Local<Value> prototype) {
  if (JsSetPrototype((JsValueRef)this, *prototype) != JsNoError) {
    return Nothing<bool>();
  }

  return Just(true);
}

bool Object::SetPrototype(Handle<Value> prototype) {
  return FromMaybe(SetPrototype(Local<Context>(), prototype));
}

MaybeLocal<String> Object::ObjectProtoToString(Local<Context> context) {
  ContextShim* contextShim = ContextShim::GetCurrent();
  JsValueRef toString = contextShim->GetGlobalPrototypeFunction(
    ContextShim::GlobalPrototypeFunction::Object_toString);

  JsValueRef result;
  JsValueRef args[] = { this };
  if (JsCallFunction(toString, args, _countof(args), &result) != JsNoError) {
    return Local<String>();
  }
  return Local<String>::New(result);
}

Local<String> Object::ObjectProtoToString() {
  return FromMaybe(ObjectProtoToString(Local<Context>()));
}

Local<String> v8::Object::GetConstructorName() {
  JsValueRef constructor;
  if (jsrt::GetObjectConstructor((JsValueRef)this,
                                 &constructor) != JsNoError) {
    return Local<String>();
  }

  IsolateShim* iso = IsolateShim::FromIsolate(this->GetIsolate());
  JsPropertyIdRef idRef = iso->GetCachedPropertyIdRef(
    CachedPropertyIdRef::name);

  JsValueRef name;
  if (JsGetProperty(constructor, idRef, &name) != JsNoError) {
    return Local<String>();
  }

  return Local<String>::New(static_cast<String*>(name));
}

Maybe<bool> Object::HasOwnProperty(Local<Context> context, Local<Name> key) {
  JsValueRef result;
  if (jsrt::HasOwnProperty((JsValueRef)this,
                           (JsValueRef)*key, &result) != JsNoError) {
    return Nothing<bool>();
  }

  return Local<Value>(result)->BooleanValue(context);
}

bool Object::HasOwnProperty(Handle<String> key) {
  return FromMaybe(HasOwnProperty(Local<Context>(), key));
}

Maybe<bool> Object::HasRealNamedProperty(Local<Context> context,
                                         Local<Name> key) {
  return Has(context, key);
}

bool Object::HasRealNamedProperty(Handle<String> key) {
  return FromMaybe(HasRealNamedProperty(Local<Context>(), key));
}

Maybe<bool> Object::HasRealIndexedProperty(Local<Context> context,
                                           uint32_t index) {
  return Has(context, index);
}

bool Object::HasRealIndexedProperty(uint32_t index) {
  return FromMaybe(HasRealIndexedProperty(Local<Context>(), index));
}

MaybeLocal<Value> Object::GetRealNamedProperty(Local<Context> context,
                                               Local<Name> key) {
  // CHAKRA-TODO: how to skip interceptors?
  return Get(context, key);
}

Local<Value> Object::GetRealNamedProperty(Handle<String> key) {
  return FromMaybe(GetRealNamedProperty(Local<Context>(), key));
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(
  Local<Context> context, Local<Name> key) {
  // CHAKRA-TODO: This walks prototype chain skipping interceptors
  return GetPropertyAttributes(context, key);
}

JsValueRef CALLBACK Utils::AccessorHandler(JsValueRef callee,
                                           bool isConstructCall,
                                           JsValueRef *arguments,
                                           unsigned short argumentCount,
                                           void *callbackState) {
  void *externalData;
  JsValueRef result = JS_INVALID_REFERENCE;

  if (JsGetUndefinedValue(&result) != JsNoError) {
    return JS_INVALID_REFERENCE;
  }

  if (GetExternalData(callee, &externalData) != JsNoError) {
    return result;
  }

  if (externalData == nullptr) {
    return result;
  }

  AccessorExternalData *accessorData =
    static_cast<AccessorExternalData*>(externalData);
  Local<Value> dataLocal = accessorData->data;

  JsValueRef thisRef = JS_INVALID_REFERENCE;
  if (argumentCount > 0) {
    thisRef = arguments[0];
  }
  // this is ok since the first argument will stay on the stack as long as we
  // are in this function
  Local<Object> thisLocal(static_cast<Object*>(thisRef));

  Local<Object> holder;
  if (!accessorData->CheckSignature(thisLocal, &holder)) {
    return JS_INVALID_REFERENCE;
  }

  Local<Name> propertyNameLocal = accessorData->propertyName;
  switch (accessorData->type) {
    case Setter:
    {
      assert(argumentCount == 2);
      PropertyCallbackInfo<void> info(dataLocal, thisLocal, holder);
      accessorData->setter(
        propertyNameLocal, static_cast<Value*>(arguments[1]), info);
      break;
    }
    case Getter:
    {
      PropertyCallbackInfo<Value> info(dataLocal, thisLocal, holder);
      accessorData->getter(propertyNameLocal, info);
      result = info.GetReturnValue().Get();
      break;
    }
     default:
      break;
  }

  return result;
}

// Create an object that will hold the hidden values
bool Object::SetHiddenValue(Handle<String> key, Handle<Value> value) {
  IsolateShim* iso = IsolateShim::FromIsolate(this->GetIsolate());
  JsPropertyIdRef hiddenValuesIdRef = iso->GetCachedSymbolPropertyIdRef(
    CachedSymbolPropertyIdRef::__hiddenvalues__);

  JsValueRef hiddenValuesTable;
  if (JsGetProperty((JsValueRef)this,
                    hiddenValuesIdRef,
                    &hiddenValuesTable) != JsNoError) {
    return false;
  }

  if (static_cast<Value*>(hiddenValuesTable)->IsUndefined()) {
    if (JsCreateObject(&hiddenValuesTable) != JsNoError) {
      return false;
    }

    if (DefineProperty((JsValueRef)this,
                       hiddenValuesIdRef,
                       PropertyDescriptorOptionValues::False,
                       PropertyDescriptorOptionValues::False,
                       PropertyDescriptorOptionValues::False,
                       hiddenValuesTable,
                       JS_INVALID_REFERENCE,
                       JS_INVALID_REFERENCE) != JsNoError) {
      return false;
    }
  }

  if (jsrt::SetProperty(hiddenValuesTable, *key, *value) != JsNoError) {
    return false;
  }

  return true;
}

Local<Value> Object::GetHiddenValue(Handle<String> key) {
  IsolateShim* iso = IsolateShim::FromIsolate(this->GetIsolate());
  JsPropertyIdRef hiddenValuesIdRef = iso->GetCachedSymbolPropertyIdRef(
    CachedSymbolPropertyIdRef::__hiddenvalues__);

  JsValueRef hiddenValuesTable;
  if (JsGetProperty((JsValueRef)this,
                    hiddenValuesIdRef,
                    &hiddenValuesTable) != JsNoError) {
    return Local<Value>();
  }

  if (static_cast<Value*>(hiddenValuesTable)->IsUndefined()) {
    return Local<Value>();
  }

  JsPropertyIdRef keyIdRef;
  if (GetPropertyIdFromName((JsValueRef)*key, &keyIdRef) != JsNoError) {
    return Local<Value>();
  }

  bool hasKey;
  if (JsHasProperty(hiddenValuesTable, keyIdRef, &hasKey) != JsNoError) {
    return Local<Value>();
  }

  if (!hasKey) {
    return Local<Value>();
  }

  JsValueRef result;
  if (JsGetProperty(hiddenValuesTable, keyIdRef, &result) != JsNoError) {
    return Local<Value>();
  }

  return Local<Value>::New(static_cast<Value*>(result));
}

ObjectTemplate* Object::GetObjectTemplate() {
  ObjectData *objectData = nullptr;
  return GetObjectData(&objectData) == JsNoError && objectData != nullptr ?
    *objectData->objectTemplate : nullptr;
}

JsErrorCode Object::GetObjectData(ObjectData** objectData) {
  *objectData = nullptr;

  if (this->IsUndefined()) {
    return JsNoError;
  }

  JsErrorCode error;
  JsValueRef self = this;
  {
    JsPropertyIdRef selfSymbolIdRef =
      jsrt::IsolateShim::GetCurrent()->GetSelfSymbolPropertyIdRef();
    if (selfSymbolIdRef != JS_INVALID_REFERENCE) {
      JsValueRef result;
      error = JsGetProperty(this, selfSymbolIdRef, &result);
      if (error != JsNoError) {
        return error;
      }

      if (!Local<Value>(result)->IsUndefined()) {
        self = result;
      }
    }
  }

  return JsGetExternalData(self, reinterpret_cast<void **>(objectData));
}

int Object::InternalFieldCount() {
  ObjectData* objectData;
  if (GetObjectData(&objectData) != JsNoError || !objectData) {
    return 0;
  }

  return objectData->internalFieldCount;
}

Local<Value> Object::GetInternalField(int index) {
  ObjectData::FieldValue* field = ObjectData::GetInternalField(this, index);
  return field ? field->GetRef() : nullptr;
}

void Object::SetInternalField(int index, Handle<Value> value) {
  ObjectData::FieldValue* field = ObjectData::GetInternalField(this, index);
  if (field) {
    field->SetRef(*value);
  }
}

void* Object::GetAlignedPointerFromInternalField(int index) {
  ObjectData::FieldValue* field = ObjectData::GetInternalField(this, index);
  return field ? field->GetPointer() : nullptr;
}

void Object::SetAlignedPointerInInternalField(int index, void *value) {
  ObjectData::FieldValue* field = ObjectData::GetInternalField(this, index);
  if (field) {
    field->SetPointer(value);
  }
}

Local<Object> Object::Clone() {
  JsValueRef constructor;
  if (jsrt::GetObjectConstructor((JsValueRef)this,
                                 &constructor) != JsNoError) {
    return Local<Object>();
  }

  JsValueRef obj;
  if (jsrt::ConstructObject(constructor, &obj) != JsNoError) {
    return Local<Object>();
  }

  if (jsrt::CloneObject((JsValueRef)this, obj) != JsNoError) {
    return Local<Object>();
  }

  return Local<Object>::New(obj);
}

Local<Context> Object::CreationContext() {
  jsrt::ContextShim * contextShim =
    jsrt::IsolateShim::GetContextShimOfObject(this);
  if (contextShim == nullptr) {
    return Local<Context>();
  }
  return Local<Context>::New(
    static_cast<Context *>(contextShim->GetContextRef()));
}

Isolate* Object::GetIsolate() {
  return IsolateShim::GetContextShimOfObject(this)->
    GetIsolateShim()->GetCurrentAsIsolate();
}

Local<Object> Object::New(Isolate* isolate) {
  JsValueRef newObjectRef;
  if (JsCreateObject(&newObjectRef) != JsNoError) {
    return Local<Object>();
  }

  return Local<Object>::New(static_cast<Object*>(newObjectRef));
}

Object *Object::Cast(Value *obj) {
  CHAKRA_ASSERT(obj->IsObject());
  return static_cast<Object*>(obj);
}

}  // namespace v8
