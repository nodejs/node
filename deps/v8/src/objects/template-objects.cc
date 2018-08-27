// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/template-objects.h"

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/property-descriptor.h"

namespace v8 {
namespace internal {

// static
Handle<JSArray> TemplateObjectDescription::CreateTemplateObject(
    Handle<TemplateObjectDescription> description) {
  Isolate* const isolate = description->GetIsolate();

  // Create the raw object from the {raw_strings}.
  Handle<FixedArray> raw_strings(description->raw_strings(), isolate);
  Handle<JSArray> raw_object = isolate->factory()->NewJSArrayWithElements(
      raw_strings, PACKED_ELEMENTS, raw_strings->length(), TENURED);

  // Create the template object from the {cooked_strings}.
  Handle<FixedArray> cooked_strings(description->cooked_strings(), isolate);
  Handle<JSArray> template_object = isolate->factory()->NewJSArrayWithElements(
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

  return template_object;
}

}  // namespace internal
}  // namespace v8
