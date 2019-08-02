// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_STACK_FRAME_INFO_INL_H_
#define V8_OBJECTS_STACK_FRAME_INFO_INL_H_

#include "src/objects/stack-frame-info.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/frame-array-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(StackFrameInfo, Struct)

NEVER_READ_ONLY_SPACE_IMPL(StackFrameInfo)

CAST_ACCESSOR(StackFrameInfo)

SMI_ACCESSORS(StackFrameInfo, line_number, kLineNumberOffset)
SMI_ACCESSORS(StackFrameInfo, column_number, kColumnNumberOffset)
SMI_ACCESSORS(StackFrameInfo, script_id, kScriptIdOffset)
SMI_ACCESSORS(StackFrameInfo, promise_all_index, kPromiseAllIndexOffset)
ACCESSORS(StackFrameInfo, script_name, Object, kScriptNameOffset)
ACCESSORS(StackFrameInfo, script_name_or_source_url, Object,
          kScriptNameOrSourceUrlOffset)
ACCESSORS(StackFrameInfo, function_name, Object, kFunctionNameOffset)
ACCESSORS(StackFrameInfo, wasm_module_name, Object, kWasmModuleNameOffset)
SMI_ACCESSORS(StackFrameInfo, flag, kFlagOffset)
BOOL_ACCESSORS(StackFrameInfo, flag, is_eval, kIsEvalBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_constructor, kIsConstructorBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_wasm, kIsWasmBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_user_java_script, kIsUserJavaScriptBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_toplevel, kIsToplevelBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_async, kIsAsyncBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_promise_all, kIsPromiseAllBit)

OBJECT_CONSTRUCTORS_IMPL(StackTraceFrame, Struct)
NEVER_READ_ONLY_SPACE_IMPL(StackTraceFrame)
CAST_ACCESSOR(StackTraceFrame)

ACCESSORS(StackTraceFrame, frame_array, Object, kFrameArrayOffset)
SMI_ACCESSORS(StackTraceFrame, frame_index, kFrameIndexOffset)
ACCESSORS(StackTraceFrame, frame_info, Object, kFrameInfoOffset)
SMI_ACCESSORS(StackTraceFrame, id, kIdOffset)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_INL_H_
