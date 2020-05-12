// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_H_

#include "src/objects/fixed-array.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// CachedTemplateObject is a tuple used to cache a TemplateObject that has been
// created. All the CachedTemplateObject's for a given SharedFunctionInfo form a
// linked list via the next fields.
class CachedTemplateObject final
    : public TorqueGeneratedCachedTemplateObject<CachedTemplateObject, Struct> {
 public:
  static Handle<CachedTemplateObject> New(Isolate* isolate, int slot_id,
                                          Handle<JSArray> template_object,
                                          Handle<HeapObject> next);

  TQ_OBJECT_CONSTRUCTORS(CachedTemplateObject)
};

// TemplateObjectDescription is a tuple of raw strings and cooked strings for
// tagged template literals. Used to communicate with the runtime for template
// object creation within the {Runtime_GetTemplateObject} method.
class TemplateObjectDescription final
    : public TorqueGeneratedTemplateObjectDescription<TemplateObjectDescription,
                                                      Struct> {
 public:
  static Handle<JSArray> GetTemplateObject(
      Isolate* isolate, Handle<NativeContext> native_context,
      Handle<TemplateObjectDescription> description,
      Handle<SharedFunctionInfo> shared_info, int slot_id);

  TQ_OBJECT_CONSTRUCTORS(TemplateObjectDescription)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_H_
