// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
#define V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_

#include "src/objects/template-objects.h"

#include "src/objects/js-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(TemplateObjectDescription)
OBJECT_CONSTRUCTORS_IMPL(CachedTemplateObject, Tuple3)

CAST_ACCESSOR(CachedTemplateObject)

SMI_ACCESSORS(CachedTemplateObject, slot_id, kSlotIdOffset)
ACCESSORS(CachedTemplateObject, template_object, JSArray, kTemplateObjectOffset)
ACCESSORS(CachedTemplateObject, next, HeapObject, kNextOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TEMPLATE_OBJECTS_INL_H_
