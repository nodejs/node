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
SMI_ACCESSORS(StackFrameInfo, wasm_function_index, kWasmFunctionIndexOffset)
SMI_ACCESSORS(StackFrameInfo, promise_all_index, kPromiseAllIndexOffset)
SMI_ACCESSORS_CHECKED(StackFrameInfo, function_offset, kPromiseAllIndexOffset,
                      is_wasm())
ACCESSORS(StackFrameInfo, script_name, Object, kScriptNameOffset)
ACCESSORS(StackFrameInfo, script_name_or_source_url, Object,
          kScriptNameOrSourceUrlOffset)
ACCESSORS(StackFrameInfo, function_name, Object, kFunctionNameOffset)
ACCESSORS(StackFrameInfo, method_name, Object, kMethodNameOffset)
ACCESSORS(StackFrameInfo, type_name, Object, kTypeNameOffset)
ACCESSORS(StackFrameInfo, eval_origin, Object, kEvalOriginOffset)
ACCESSORS(StackFrameInfo, wasm_module_name, Object, kWasmModuleNameOffset)
ACCESSORS(StackFrameInfo, wasm_instance, Object, kWasmInstanceOffset)
SMI_ACCESSORS(StackFrameInfo, flag, kFlagOffset)
BOOL_ACCESSORS(StackFrameInfo, flag, is_eval, kIsEvalBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_constructor, kIsConstructorBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_wasm, kIsWasmBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_asmjs_wasm, kIsAsmJsWasmBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_user_java_script, kIsUserJavaScriptBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_toplevel, kIsToplevelBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_async, kIsAsyncBit)
BOOL_ACCESSORS(StackFrameInfo, flag, is_promise_all, kIsPromiseAllBit)

TQ_OBJECT_CONSTRUCTORS_IMPL(StackTraceFrame)
NEVER_READ_ONLY_SPACE_IMPL(StackTraceFrame)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_INL_H_
