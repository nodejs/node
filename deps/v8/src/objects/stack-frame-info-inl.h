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

#include "torque-generated/src/objects/stack-frame-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(StackFrameInfo)

NEVER_READ_ONLY_SPACE_IMPL(StackFrameInfo)

SMI_ACCESSORS_CHECKED(StackFrameInfo, function_offset,
                      kPromiseCombinatorIndexOffset, is_wasm())
BOOL_ACCESSORS(StackFrameInfo, flag, is_eval, IsEvalBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_constructor, IsConstructorBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_wasm, IsWasmBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_asmjs_wasm, IsAsmJsWasmBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_user_java_script,
               IsUserJavaScriptBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_toplevel, IsToplevelBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_async, IsAsyncBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_promise_all, IsPromiseAllBit::kShift)
BOOL_ACCESSORS(StackFrameInfo, flag, is_promise_any, IsPromiseAnyBit::kShift)

TQ_OBJECT_CONSTRUCTORS_IMPL(StackTraceFrame)
NEVER_READ_ONLY_SPACE_IMPL(StackTraceFrame)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_STACK_FRAME_INFO_INL_H_
