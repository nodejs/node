// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/bootstrapper.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_CreateApiFunction) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(FunctionTemplateInfo, data, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  return *isolate->factory()->CreateApiFunction(data, prototype);
}


RUNTIME_FUNCTION(Runtime_IsTemplate) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg, 0);
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(Runtime_GetTemplateField) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(HeapObject, templ, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  int offset = index * kPointerSize + HeapObject::kHeaderSize;
  InstanceType type = templ->map()->instance_type();
  RUNTIME_ASSERT(type == FUNCTION_TEMPLATE_INFO_TYPE ||
                 type == OBJECT_TEMPLATE_INFO_TYPE);
  RUNTIME_ASSERT(offset > 0);
  if (type == FUNCTION_TEMPLATE_INFO_TYPE) {
    RUNTIME_ASSERT(offset < FunctionTemplateInfo::kSize);
  } else {
    RUNTIME_ASSERT(offset < ObjectTemplateInfo::kSize);
  }
  return *HeapObject::RawField(templ, offset);
}


// Transform getter or setter into something DefineAccessor can handle.
static Handle<Object> InstantiateAccessorComponent(Isolate* isolate,
                                                   Handle<Object> component) {
  if (component->IsUndefined()) return isolate->factory()->undefined_value();
  Handle<FunctionTemplateInfo> info =
      Handle<FunctionTemplateInfo>::cast(component);
  return Utils::OpenHandle(*Utils::ToLocal(info)->GetFunction());
}


RUNTIME_FUNCTION(Runtime_DefineApiAccessorProperty) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  CONVERT_SMI_ARG_CHECKED(attribute, 4);
  RUNTIME_ASSERT(getter->IsUndefined() || getter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(setter->IsUndefined() || setter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(PropertyDetails::AttributesField::is_valid(
      static_cast<PropertyAttributes>(attribute)));
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineAccessor(
                   object, name, InstantiateAccessorComponent(isolate, getter),
                   InstantiateAccessorComponent(isolate, setter),
                   static_cast<PropertyAttributes>(attribute)));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_AddPropertyForTemplate) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

#ifdef DEBUG
  bool duplicate;
  if (key->IsName()) {
    LookupIterator it(object, Handle<Name>::cast(key),
                      LookupIterator::OWN_SKIP_INTERCEPTOR);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    DCHECK(maybe.has_value);
    duplicate = it.IsFound();
  } else {
    uint32_t index = 0;
    RUNTIME_ASSERT(key->ToArrayIndex(&index));
    Maybe<bool> maybe = JSReceiver::HasOwnElement(object, index);
    if (!maybe.has_value) return isolate->heap()->exception();
    duplicate = maybe.value;
  }
  if (duplicate) {
    Handle<Object> args[1] = {key};
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError("duplicate_template_property", HandleVector(args, 1)));
  }
#endif

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::DefineObjectProperty(object, key, value, attributes));
  return *result;
}
}
}  // namespace v8::internal
