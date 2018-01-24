// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_INL_H_
#define V8_OBJECTS_SCRIPT_INL_H_

#include "src/objects/script.h"

#include "src/objects/string-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(Script)

ACCESSORS(Script, source, Object, kSourceOffset)
ACCESSORS(Script, name, Object, kNameOffset)
SMI_ACCESSORS(Script, id, kIdOffset)
SMI_ACCESSORS(Script, line_offset, kLineOffsetOffset)
SMI_ACCESSORS(Script, column_offset, kColumnOffsetOffset)
ACCESSORS(Script, context_data, Object, kContextOffset)
ACCESSORS(Script, wrapper, HeapObject, kWrapperOffset)
SMI_ACCESSORS(Script, type, kTypeOffset)
ACCESSORS(Script, line_ends, Object, kLineEndsOffset)
ACCESSORS_CHECKED(Script, eval_from_shared, Object, kEvalFromSharedOffset,
                  this->type() != TYPE_WASM)
SMI_ACCESSORS_CHECKED(Script, eval_from_position, kEvalFromPositionOffset,
                      this->type() != TYPE_WASM)
ACCESSORS(Script, shared_function_infos, FixedArray, kSharedFunctionInfosOffset)
SMI_ACCESSORS(Script, flags, kFlagsOffset)
ACCESSORS(Script, source_url, Object, kSourceUrlOffset)
ACCESSORS(Script, source_mapping_url, Object, kSourceMappingUrlOffset)
ACCESSORS(Script, host_defined_options, FixedArray, kHostDefinedOptionsOffset)
ACCESSORS_CHECKED(Script, wasm_compiled_module, Object, kEvalFromSharedOffset,
                  this->type() == TYPE_WASM)

Script::CompilationType Script::compilation_type() {
  return BooleanBit::get(flags(), kCompilationTypeBit) ? COMPILATION_TYPE_EVAL
                                                       : COMPILATION_TYPE_HOST;
}
void Script::set_compilation_type(CompilationType type) {
  set_flags(BooleanBit::set(flags(), kCompilationTypeBit,
                            type == COMPILATION_TYPE_EVAL));
}
Script::CompilationState Script::compilation_state() {
  return BooleanBit::get(flags(), kCompilationStateBit)
             ? COMPILATION_STATE_COMPILED
             : COMPILATION_STATE_INITIAL;
}
void Script::set_compilation_state(CompilationState state) {
  set_flags(BooleanBit::set(flags(), kCompilationStateBit,
                            state == COMPILATION_STATE_COMPILED));
}
ScriptOriginOptions Script::origin_options() {
  return ScriptOriginOptions((flags() & kOriginOptionsMask) >>
                             kOriginOptionsShift);
}
void Script::set_origin_options(ScriptOriginOptions origin_options) {
  DCHECK(!(origin_options.Flags() & ~((1 << kOriginOptionsSize) - 1)));
  set_flags((flags() & ~kOriginOptionsMask) |
            (origin_options.Flags() << kOriginOptionsShift));
}

bool Script::HasValidSource() {
  Object* src = this->source();
  if (!src->IsString()) return true;
  String* src_str = String::cast(src);
  if (!StringShape(src_str).IsExternal()) return true;
  if (src_str->IsOneByteRepresentation()) {
    return ExternalOneByteString::cast(src)->resource() != nullptr;
  } else if (src_str->IsTwoByteRepresentation()) {
    return ExternalTwoByteString::cast(src)->resource() != nullptr;
  }
  return true;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_INL_H_
