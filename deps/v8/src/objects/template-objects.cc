// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/template-objects.h"

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

bool TemplateObjectDescription::Equals(
    TemplateObjectDescription const* that) const {
  if (this->raw_strings()->length() == that->raw_strings()->length()) {
    for (int i = this->raw_strings()->length(); --i >= 0;) {
      if (this->raw_strings()->get(i) != that->raw_strings()->get(i)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

// static
Handle<JSArray> TemplateObjectDescription::GetTemplateObject(
    Handle<TemplateObjectDescription> description,
    Handle<Context> native_context) {
  DCHECK(native_context->IsNativeContext());
  Isolate* const isolate = native_context->GetIsolate();

  // Check if we already have a [[TemplateMap]] for the {native_context},
  // and if not, just allocate one on the fly (which will be set below).
  Handle<TemplateMap> template_map =
      native_context->template_map()->IsUndefined(isolate)
          ? TemplateMap::New(isolate)
          : handle(TemplateMap::cast(native_context->template_map()), isolate);

  // Check if we already have an appropriate entry.
  Handle<JSArray> template_object;
  if (!TemplateMap::Lookup(template_map, description)
           .ToHandle(&template_object)) {
    // Create the raw object from the {raw_strings}.
    Handle<FixedArray> raw_strings(description->raw_strings(), isolate);
    Handle<JSArray> raw_object = isolate->factory()->NewJSArrayWithElements(
        raw_strings, PACKED_ELEMENTS, raw_strings->length(), TENURED);

    // Create the template object from the {cooked_strings}.
    Handle<FixedArray> cooked_strings(description->cooked_strings(), isolate);
    template_object = isolate->factory()->NewJSArrayWithElements(
        cooked_strings, PACKED_ELEMENTS, cooked_strings->length(), TENURED);

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
                               kThrowOnError)
        .ToChecked();

    // Freeze the {template_object} as well.
    JSObject::SetIntegrityLevel(template_object, FROZEN, kThrowOnError)
        .ToChecked();

    // Remember the {template_object} in the {template_map}.
    template_map = TemplateMap::Add(template_map, description, template_object);
    native_context->set_template_map(*template_map);
  }

  return template_object;
}

// static
bool TemplateMapShape::IsMatch(TemplateObjectDescription* key, Object* value) {
  return key->Equals(TemplateObjectDescription::cast(value));
}

// static
uint32_t TemplateMapShape::Hash(Isolate* isolate,
                                TemplateObjectDescription* key) {
  return key->hash();
}

// static
uint32_t TemplateMapShape::HashForObject(Isolate* isolate, Object* object) {
  return Hash(isolate, TemplateObjectDescription::cast(object));
}

// static
Handle<TemplateMap> TemplateMap::New(Isolate* isolate) {
  return HashTable::New(isolate, 0);
}

// static
MaybeHandle<JSArray> TemplateMap::Lookup(
    Handle<TemplateMap> template_map, Handle<TemplateObjectDescription> key) {
  int const entry = template_map->FindEntry(*key);
  if (entry == kNotFound) return MaybeHandle<JSArray>();
  int const index = EntryToIndex(entry);
  return handle(JSArray::cast(template_map->get(index + 1)));
}

// static
Handle<TemplateMap> TemplateMap::Add(Handle<TemplateMap> template_map,
                                     Handle<TemplateObjectDescription> key,
                                     Handle<JSArray> value) {
  DCHECK_EQ(kNotFound, template_map->FindEntry(*key));
  template_map = EnsureCapacity(template_map, 1);
  uint32_t const hash = ShapeT::Hash(key->GetIsolate(), *key);
  int const entry = template_map->FindInsertionEntry(hash);
  int const index = EntryToIndex(entry);
  template_map->set(index + 0, *key);
  template_map->set(index + 1, *value);
  template_map->ElementAdded();
  return template_map;
}

}  // namespace internal
}  // namespace v8
