// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/stack-frame-info.h"

#include "src/objects/stack-frame-info-inl.h"

namespace v8 {
namespace internal {

int StackTraceFrame::GetLineNumber(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  int line = GetFrameInfo(frame)->line_number();
  return line != StackFrameBase::kNone ? line : Message::kNoLineNumberInfo;
}

int StackTraceFrame::GetColumnNumber(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  int column = GetFrameInfo(frame)->column_number();
  return column != StackFrameBase::kNone ? column : Message::kNoColumnInfo;
}

int StackTraceFrame::GetScriptId(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  int id = GetFrameInfo(frame)->script_id();
  return id != StackFrameBase::kNone ? id : Message::kNoScriptIdInfo;
}

Handle<Object> StackTraceFrame::GetFileName(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  auto name = GetFrameInfo(frame)->script_name();
  return handle(name, frame->GetIsolate());
}

Handle<Object> StackTraceFrame::GetScriptNameOrSourceUrl(
    Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  auto name = GetFrameInfo(frame)->script_name_or_source_url();
  return handle(name, frame->GetIsolate());
}

Handle<Object> StackTraceFrame::GetFunctionName(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  auto name = GetFrameInfo(frame)->function_name();
  return handle(name, frame->GetIsolate());
}

bool StackTraceFrame::IsEval(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  return GetFrameInfo(frame)->is_eval();
}

bool StackTraceFrame::IsConstructor(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  return GetFrameInfo(frame)->is_constructor();
}

bool StackTraceFrame::IsWasm(Handle<StackTraceFrame> frame) {
  if (frame->frame_info()->IsUndefined()) InitializeFrameInfo(frame);
  return GetFrameInfo(frame)->is_wasm();
}

Handle<StackFrameInfo> StackTraceFrame::GetFrameInfo(
    Handle<StackTraceFrame> frame) {
  return handle(StackFrameInfo::cast(frame->frame_info()), frame->GetIsolate());
}

void StackTraceFrame::InitializeFrameInfo(Handle<StackTraceFrame> frame) {
  Isolate* isolate = frame->GetIsolate();
  Handle<StackFrameInfo> frame_info = isolate->factory()->NewStackFrameInfo(
      handle(FrameArray::cast(frame->frame_array()), isolate),
      frame->frame_index());
  frame->set_frame_info(*frame_info);

  // After initializing, we no longer need to keep a reference
  // to the frame_array.
  frame->set_frame_array(ReadOnlyRoots(isolate).undefined_value());
  frame->set_frame_index(-1);
}

}  // namespace internal
}  // namespace v8
