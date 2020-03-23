// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ARGUMENTS_INL_H_
#define V8_OBJECTS_ARGUMENTS_INL_H_

#include "src/objects/arguments.h"

#include "src/execution/isolate-inl.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(SloppyArgumentsElements, FixedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(JSArgumentsObject)
TQ_OBJECT_CONSTRUCTORS_IMPL(AliasedArgumentsEntry)

CAST_ACCESSOR(SloppyArgumentsElements)

DEF_GETTER(SloppyArgumentsElements, context, Context) {
  return TaggedField<Context>::load(isolate, *this,
                                    OffsetOfElementAt(kContextIndex));
}

DEF_GETTER(SloppyArgumentsElements, arguments, FixedArray) {
  return TaggedField<FixedArray>::load(isolate, *this,
                                       OffsetOfElementAt(kArgumentsIndex));
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

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ARGUMENTS_INL_H_
