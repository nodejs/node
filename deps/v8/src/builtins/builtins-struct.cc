// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/js-struct-inl.h"
#include "src/objects/property-details.h"

namespace v8 {
namespace internal {

constexpr int kMaxJSStructFields = 999;
// Note: For Wasm structs, we currently allow 2000 fields, because there was
// specific demand for that. Ideally we'd have the same limit, but JS structs
// rely on DescriptorArrays and are hence limited to 1020 fields at most.
static_assert(kMaxJSStructFields <= kMaxNumberOfDescriptors);

BUILTIN(SharedStructTypeConstructor) {
  DCHECK(v8_flags.shared_string_table);

  HandleScope scope(isolate);
  static const char method_name[] = "SharedStructType";
  auto* factory = isolate->factory();

  Handle<JSReceiver> field_names_arg;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, field_names_arg,
      Object::ToObject(isolate, args.atOrUndefined(isolate, 1), method_name));

  // Treat field_names_arg as arraylike.
  Handle<Object> raw_length_number;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, raw_length_number,
      Object::GetLengthFromArrayLike(isolate, field_names_arg));
  double num_properties_double = raw_length_number->Number();
  if (num_properties_double < 0 || num_properties_double > kMaxJSStructFields) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kStructFieldCountOutOfRange));
  }
  int num_properties = static_cast<int>(num_properties_double);

  Handle<DescriptorArray> descriptors = factory->NewDescriptorArray(
      num_properties, 0, AllocationType::kSharedOld);

  // Build up the descriptor array.
  for (int i = 0; i < num_properties; ++i) {
    Handle<Object> raw_field_name;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, raw_field_name,
        JSReceiver::GetElement(isolate, field_names_arg, i));
    Handle<Name> field_name;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, field_name,
                                       Object::ToName(isolate, raw_field_name));
    field_name = factory->InternalizeName(field_name);

    // Shared structs' fields need to be aligned, so make it all tagged.
    PropertyDetails details(
        PropertyKind::kData, SEALED, PropertyLocation::kField,
        PropertyConstness::kMutable, Representation::Tagged(), i);
    descriptors->Set(InternalIndex(i), *field_name,
                     MaybeObject::FromObject(FieldType::Any()), details);
  }
  descriptors->Sort();

  Handle<SharedFunctionInfo> info =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), Builtin::kSharedStructConstructor,
          FunctionKind::kNormalFunction);
  info->set_internal_formal_parameter_count(JSParameterCount(0));
  info->set_length(0);

  Handle<JSFunction> constructor =
      Factory::JSFunctionBuilder{isolate, info, isolate->native_context()}
          .set_map(isolate->strict_function_map())
          .Build();

  int instance_size;
  int in_object_properties;
  JSFunction::CalculateInstanceSizeHelper(JS_SHARED_STRUCT_TYPE, false, 0,
                                          num_properties, &instance_size,
                                          &in_object_properties);
  Handle<Map> instance_map = factory->NewMap(
      JS_SHARED_STRUCT_TYPE, instance_size, TERMINAL_FAST_ELEMENTS_KIND,
      in_object_properties, AllocationType::kSharedMap);

  instance_map->InitializeDescriptors(isolate, *descriptors);
  // Structs have fixed layout ahead of time, so there's no slack.
  instance_map->SetInObjectUnusedPropertyFields(0);
  instance_map->set_is_extensible(false);
  JSFunction::SetInitialMap(isolate, constructor, instance_map,
                            factory->null_value());

  // The constructor is not a shared object, so the shared map should not point
  // to it.
  instance_map->set_constructor_or_back_pointer(*factory->null_value());

  return *constructor;
}

BUILTIN(SharedStructConstructor) {
  HandleScope scope(isolate);
  return *isolate->factory()->NewJSSharedStruct(args.target());
}

}  // namespace internal
}  // namespace v8
