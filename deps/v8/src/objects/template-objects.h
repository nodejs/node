// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_H_

#include "src/objects/fixed-array.h"
#include "src/objects/struct.h"
#include "src/objects/tagged-field.h"
#include "src/objects/torque-defined-classes.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class Oddball;
class StructBodyDescriptor;

#include "torque-generated/src/objects/template-objects-tq.inc"

// TemplateObjectDescription is a tuple of raw strings and cooked strings for
// tagged template literals. Used to communicate with the runtime for template
// object creation within the {Runtime_GetTemplateObject} method.
V8_OBJECT class TemplateObjectDescription final : public StructLayout {
 public:
  static DirectHandle<JSArray> GetTemplateObject(
      Isolate* isolate, DirectHandle<NativeContext> native_context,
      DirectHandle<TemplateObjectDescription> description,
      DirectHandle<SharedFunctionInfo> shared_info, int slot_id);

  inline Tagged<FixedArray> raw_strings() const;
  inline void set_raw_strings(Tagged<FixedArray> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> cooked_strings() const;
  inline void set_cooked_strings(Tagged<FixedArray> value,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  using BodyDescriptor = StructBodyDescriptor;

  DECL_PRINTER(TemplateObjectDescription)
  DECL_VERIFIER(TemplateObjectDescription)

 private:
  friend class Factory;
  friend class TorqueGeneratedTemplateObjectDescriptionAsserts;

  TaggedMember<FixedArray> raw_strings_;
  TaggedMember<FixedArray> cooked_strings_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_H_
