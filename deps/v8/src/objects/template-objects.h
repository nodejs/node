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
class CachedTemplateObject final : public Tuple3 {
 public:
  DECL_INT_ACCESSORS(slot_id)
  DECL_ACCESSORS(template_object, JSArray)
  DECL_ACCESSORS(next, HeapObject)

  static Handle<CachedTemplateObject> New(Isolate* isolate, int slot_id,
                                          Handle<JSArray> template_object,
                                          Handle<HeapObject> next);

  DECL_CAST(CachedTemplateObject)

  static constexpr int kSlotIdOffset = kValue1Offset;
  static constexpr int kTemplateObjectOffset = kValue2Offset;
  static constexpr int kNextOffset = kValue3Offset;

  OBJECT_CONSTRUCTORS(CachedTemplateObject, Tuple3);
};

// TemplateObjectDescription is a tuple of raw strings and cooked strings for
// tagged template literals. Used to communicate with the runtime for template
// object creation within the {Runtime_GetTemplateObject} method.
class TemplateObjectDescription final : public Struct {
 public:
  DECL_ACCESSORS(raw_strings, FixedArray)
  DECL_ACCESSORS(cooked_strings, FixedArray)

  DECL_CAST(TemplateObjectDescription)

  static Handle<JSArray> GetTemplateObject(
      Isolate* isolate, Handle<Context> native_context,
      Handle<TemplateObjectDescription> description,
      Handle<SharedFunctionInfo> shared_info, int slot_id);

  DECL_PRINTER(TemplateObjectDescription)
  DECL_VERIFIER(TemplateObjectDescription)

  DEFINE_FIELD_OFFSET_CONSTANTS(
      Struct::kHeaderSize, TORQUE_GENERATED_TEMPLATE_OBJECT_DESCRIPTION_FIELDS)

  OBJECT_CONSTRUCTORS(TemplateObjectDescription, Struct);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_H_
