// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_INL_H_
#define V8_OBJECTS_ARGUMENTS_INL_H_

#include "src/objects/arguments.h"

#include "src/contexts-inl.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"
#include "src/objects/fixed-array-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(SloppyArgumentsElements, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(JSArgumentsObject, JSObject)
OBJECT_CONSTRUCTORS_IMPL(AliasedArgumentsEntry, Struct)

CAST_ACCESSOR(AliasedArgumentsEntry)
CAST_ACCESSOR(SloppyArgumentsElements)
CAST_ACCESSOR(JSArgumentsObject)

SMI_ACCESSORS(AliasedArgumentsEntry, aliased_context_slot,
              kAliasedContextSlotOffset)

Context SloppyArgumentsElements::context() {
  return Context::cast(get(kContextIndex));
}

FixedArray SloppyArgumentsElements::arguments() {
  return FixedArray::cast(get(kArgumentsIndex));
}

void SloppyArgumentsElements::set_arguments(FixedArray arguments) {
  set(kArgumentsIndex, arguments);
}

uint32_t SloppyArgumentsElements::parameter_map_length() {
  return length() - kParameterMapStart;
}

Object SloppyArgumentsElements::get_mapped_entry(uint32_t entry) {
  return get(entry + kParameterMapStart);
}

void SloppyArgumentsElements::set_mapped_entry(uint32_t entry, Object object) {
  set(entry + kParameterMapStart, object);
}

// TODO(danno): This shouldn't be inline here, but to defensively avoid
// regressions associated with the fix for the bug 778574, it's staying that way
// until the splice implementation in builtin-arrays.cc can be removed and this
// function can be moved into runtime-arrays.cc near its other usage.
bool JSSloppyArgumentsObject::GetSloppyArgumentsLength(Isolate* isolate,
                                                       Handle<JSObject> object,
                                                       int* out) {
  Context context = *isolate->native_context();
  Map map = object->map();
  if (map != context->sloppy_arguments_map() &&
      map != context->strict_arguments_map() &&
      map != context->fast_aliased_arguments_map()) {
    return false;
  }
  DCHECK(object->HasFastElements() || object->HasFastArgumentsElements());
  Object len_obj =
      object->InObjectPropertyAt(JSArgumentsObjectWithLength::kLengthIndex);
  if (!len_obj->IsSmi()) return false;
  *out = Max(0, Smi::ToInt(len_obj));

  FixedArray parameters = FixedArray::cast(object->elements());
  if (object->HasSloppyArgumentsElements()) {
    FixedArray arguments = FixedArray::cast(parameters->get(1));
    return *out <= arguments->length();
  }
  return *out <= parameters->length();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_INL_H_
