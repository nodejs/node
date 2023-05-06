// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_set>

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

namespace {

struct NameHandleHasher {
  size_t operator()(Handle<Name> name) const { return name->hash(); }
};

struct UniqueNameHandleEqual {
  bool operator()(Handle<Name> x, Handle<Name> y) const {
    DCHECK(x->IsUniqueName());
    DCHECK(y->IsUniqueName());
    return *x == *y;
  }
};

using UniqueNameHandleSet =
    std::unordered_set<Handle<Name>, NameHandleHasher, UniqueNameHandleEqual>;

}  // namespace

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

  Handle<DescriptorArray> maybe_descriptors;
  if (num_properties != 0) {
    maybe_descriptors = factory->NewDescriptorArray(num_properties, 0,
                                                    AllocationType::kSharedOld);

    // Build up the descriptor array.
    UniqueNameHandleSet all_field_names;
    for (int i = 0; i < num_properties; ++i) {
      Handle<Object> raw_field_name;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, raw_field_name,
          JSReceiver::GetElement(isolate, field_names_arg, i));
      Handle<Name> field_name;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, field_name, Object::ToName(isolate, raw_field_name));
      field_name = factory->InternalizeName(field_name);

      // TOOD(v8:12547): Support Symbols?
      if (field_name->IsSymbol()) {
        THROW_NEW_ERROR_RETURN_FAILURE(
            isolate, NewTypeError(MessageTemplate::kSymbolToString));
      }

      // Check that there are no duplicates.
      const bool is_duplicate = !all_field_names.insert(field_name).second;
      if (is_duplicate) {
        THROW_NEW_ERROR_RETURN_FAILURE(
            isolate, NewTypeError(MessageTemplate::kDuplicateTemplateProperty,
                                  field_name));
      }

      // Shared structs' fields need to be aligned, so make it all tagged.
      PropertyDetails details(
          PropertyKind::kData, SEALED, PropertyLocation::kField,
          PropertyConstness::kMutable, Representation::Tagged(), i);
      maybe_descriptors->Set(InternalIndex(i), *field_name,
                             MaybeObject::FromObject(FieldType::Any()),
                             details);
    }
    maybe_descriptors->Sort();
  }

  Handle<SharedFunctionInfo> info =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(), Builtin::kSharedStructConstructor,
          FunctionKind::kNormalFunction);
  info->set_internal_formal_parameter_count(JSParameterCount(0));
  info->set_length(0);

  Handle<JSFunction> constructor =
      Factory::JSFunctionBuilder{isolate, info, isolate->native_context()}
          .set_map(isolate->strict_function_with_readonly_prototype_map())
          .Build();

  int instance_size;
  int in_object_properties;
  JSFunction::CalculateInstanceSizeHelper(JS_SHARED_STRUCT_TYPE, false, 0,
                                          num_properties, &instance_size,
                                          &in_object_properties);
  Handle<Map> instance_map = factory->NewMap(
      JS_SHARED_STRUCT_TYPE, instance_size, TERMINAL_FAST_ELEMENTS_KIND,
      in_object_properties, AllocationType::kSharedMap);

  // Structs have fixed layout ahead of time, so there's no slack.
  int out_of_object_properties = num_properties - in_object_properties;
  if (out_of_object_properties == 0) {
    instance_map->SetInObjectUnusedPropertyFields(0);
  } else {
    instance_map->SetOutOfObjectUnusedPropertyFields(0);
  }
  instance_map->set_is_extensible(false);
  JSFunction::SetInitialMap(isolate, constructor, instance_map,
                            factory->null_value(), factory->null_value());
  constructor->map().SetConstructor(ReadOnlyRoots(isolate).null_value());
  constructor->map().set_has_non_instance_prototype(true);

  // Pre-create the enum cache in the shared space, as otherwise for-in
  // enumeration will incorrectly create an enum cache in the per-thread heap.
  if (num_properties == 0) {
    instance_map->SetEnumLength(0);
  } else {
    instance_map->InitializeDescriptors(isolate, *maybe_descriptors);
    FastKeyAccumulator::InitializeFastPropertyEnumCache(
        isolate, instance_map, num_properties, AllocationType::kSharedOld);
    DCHECK_EQ(num_properties, instance_map->EnumLength());
  }

  return *constructor;
}

BUILTIN(SharedStructConstructor) {
  HandleScope scope(isolate);
  return *isolate->factory()->NewJSSharedStruct(args.target());
}

}  // namespace internal
}  // namespace v8
