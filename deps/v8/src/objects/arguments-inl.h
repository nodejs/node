// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_INL_H_
#define V8_OBJECTS_ARGUMENTS_INL_H_

#include "src/objects/arguments.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(AliasedArgumentsEntry)
CAST_ACCESSOR(JSArgumentsObject)
CAST_ACCESSOR(JSSloppyArgumentsObject)
CAST_ACCESSOR(SloppyArgumentsElements)

ACCESSORS(JSArgumentsObject, length, Object, kLengthOffset);
ACCESSORS(JSSloppyArgumentsObject, callee, Object, kCalleeOffset);

SMI_ACCESSORS(AliasedArgumentsEntry, aliased_context_slot, kAliasedContextSlot)

TYPE_CHECKER(JSArgumentsObject, JS_ARGUMENTS_TYPE)

Context* SloppyArgumentsElements::context() {
  return Context::cast(get(kContextIndex));
}

FixedArray* SloppyArgumentsElements::arguments() {
  return FixedArray::cast(get(kArgumentsIndex));
}

void SloppyArgumentsElements::set_arguments(FixedArray* arguments) {
  set(kArgumentsIndex, arguments);
}

uint32_t SloppyArgumentsElements::parameter_map_length() {
  return length() - kParameterMapStart;
}

Object* SloppyArgumentsElements::get_mapped_entry(uint32_t entry) {
  return get(entry + kParameterMapStart);
}

void SloppyArgumentsElements::set_mapped_entry(uint32_t entry, Object* object) {
  set(entry + kParameterMapStart, object);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_INL_H_
