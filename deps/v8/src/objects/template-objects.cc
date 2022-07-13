// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/template-objects.h"

#include "src/base/functional.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/template-objects-inl.h"

namespace v8 {
namespace internal {

// static
Handle<JSArray> TemplateObjectDescription::GetTemplateObject(
    Isolate* isolate, Handle<NativeContext> native_context,
    Handle<TemplateObjectDescription> description,
    Handle<SharedFunctionInfo> shared_info, int slot_id) {
  uint32_t hash = shared_info->Hash();

  // Check the template weakmap to see if the template object already exists.
  Handle<EphemeronHashTable> template_weakmap;

  if (native_context->template_weakmap().IsUndefined(isolate)) {
    template_weakmap = EphemeronHashTable::New(isolate, 1);
  } else {
    DisallowGarbageCollection no_gc;
    ReadOnlyRoots roots(isolate);
    template_weakmap = handle(
        EphemeronHashTable::cast(native_context->template_weakmap()), isolate);
    Object maybe_cached_template = template_weakmap->Lookup(shared_info, hash);
    while (!maybe_cached_template.IsTheHole(roots)) {
      CachedTemplateObject cached_template =
          CachedTemplateObject::cast(maybe_cached_template);
      if (cached_template.slot_id() == slot_id) {
        return handle(cached_template.template_object(), isolate);
      }
      maybe_cached_template = cached_template.next();
    }
  }

  // Create the raw object from the {raw_strings}.
  Handle<FixedArray> raw_strings(description->raw_strings(), isolate);
  Handle<JSArray> raw_object = isolate->factory()->NewJSArrayWithElements(
      raw_strings, PACKED_ELEMENTS, raw_strings->length(),
      AllocationType::kOld);

  // Create the template object from the {cooked_strings}.
  Handle<FixedArray> cooked_strings(description->cooked_strings(), isolate);
  Handle<JSArray> template_object = isolate->factory()->NewJSArrayWithElements(
      cooked_strings, PACKED_ELEMENTS, cooked_strings->length(),
      AllocationType::kOld);

  // Freeze the {raw_object}.
  JSObject::SetIntegrityLevel(raw_object, FROZEN, kThrowOnError).ToChecked();

  // Install a "raw" data property for {raw_object} on {template_object}.
  PropertyDescriptor raw_desc;
  raw_desc.set_value(raw_object);
  raw_desc.set_configurable(false);
  raw_desc.set_enumerable(false);
  raw_desc.set_writable(false);
  JSArray::DefineOwnProperty(isolate, template_object,
                             isolate->factory()->raw_string(), &raw_desc,
                             Just(kThrowOnError))
      .ToChecked();

  // Freeze the {template_object} as well.
  JSObject::SetIntegrityLevel(template_object, FROZEN, kThrowOnError)
      .ToChecked();

  // Insert the template object into the template weakmap.
  Handle<HeapObject> previous_cached_templates = handle(
      HeapObject::cast(template_weakmap->Lookup(shared_info, hash)), isolate);
  Handle<CachedTemplateObject> cached_template = CachedTemplateObject::New(
      isolate, slot_id, template_object, previous_cached_templates);
  template_weakmap = EphemeronHashTable::Put(
      isolate, template_weakmap, shared_info, cached_template, hash);
  native_context->set_template_weakmap(*template_weakmap);

  return template_object;
}

Handle<CachedTemplateObject> CachedTemplateObject::New(
    Isolate* isolate, int slot_id, Handle<JSArray> template_object,
    Handle<HeapObject> next) {
  DCHECK(next->IsCachedTemplateObject() || next->IsTheHole());
  Handle<CachedTemplateObject> result_handle =
      Handle<CachedTemplateObject>::cast(isolate->factory()->NewStruct(
          CACHED_TEMPLATE_OBJECT_TYPE, AllocationType::kOld));
  {
    DisallowGarbageCollection no_gc;
    auto result = *result_handle;
    result.set_slot_id(slot_id);
    result.set_template_object(*template_object);
    result.set_next(*next);
  }
  return result_handle;
}

}  // namespace internal
}  // namespace v8
