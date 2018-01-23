// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FRAMES_INL_H_
#define V8_FRAMES_INL_H_

#include "src/frame-constants.h"
#include "src/frames.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8memory.h"

namespace v8 {
namespace internal {

inline Address StackHandler::address() const {
  return reinterpret_cast<Address>(const_cast<StackHandler*>(this));
}


inline StackHandler* StackHandler::next() const {
  const int offset = StackHandlerConstants::kNextOffset;
  return FromAddress(Memory::Address_at(address() + offset));
}


inline StackHandler* StackHandler::FromAddress(Address address) {
  return reinterpret_cast<StackHandler*>(address);
}


inline StackFrame::StackFrame(StackFrameIteratorBase* iterator)
    : iterator_(iterator), isolate_(iterator_->isolate()) {
}


inline StackHandler* StackFrame::top_handler() const {
  return iterator_->handler();
}


inline Address* StackFrame::ResolveReturnAddressLocation(Address* pc_address) {
  if (return_address_location_resolver_ == nullptr) {
    return pc_address;
  } else {
    return reinterpret_cast<Address*>(
        return_address_location_resolver_(
            reinterpret_cast<uintptr_t>(pc_address)));
  }
}

inline NativeFrame::NativeFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {}

inline Address NativeFrame::GetCallerStackPointer() const {
  return fp() + CommonFrameConstants::kCallerSPOffset;
}

inline EntryFrame::EntryFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {}

inline ConstructEntryFrame::ConstructEntryFrame(
    StackFrameIteratorBase* iterator)
    : EntryFrame(iterator) {}

inline ExitFrame::ExitFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {}

inline BuiltinExitFrame::BuiltinExitFrame(StackFrameIteratorBase* iterator)
    : ExitFrame(iterator) {}

inline Object* BuiltinExitFrame::receiver_slot_object() const {
  // The receiver is the first argument on the frame.
  // fp[1]: return address.
  // fp[2]: the last argument (new target).
  // fp[4]: argc.
  // fp[2 + argc - 1]: receiver.
  Object* argc_slot = argc_slot_object();
  DCHECK(argc_slot->IsSmi());
  int argc = Smi::ToInt(argc_slot);

  const int receiverOffset =
      BuiltinExitFrameConstants::kNewTargetOffset + (argc - 1) * kPointerSize;
  return Memory::Object_at(fp() + receiverOffset);
}

inline Object* BuiltinExitFrame::argc_slot_object() const {
  return Memory::Object_at(fp() + BuiltinExitFrameConstants::kArgcOffset);
}

inline Object* BuiltinExitFrame::target_slot_object() const {
  return Memory::Object_at(fp() + BuiltinExitFrameConstants::kTargetOffset);
}

inline Object* BuiltinExitFrame::new_target_slot_object() const {
  return Memory::Object_at(fp() + BuiltinExitFrameConstants::kNewTargetOffset);
}

inline StandardFrame::StandardFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {
}


inline Object* StandardFrame::GetExpression(int index) const {
  return Memory::Object_at(GetExpressionAddress(index));
}


inline void StandardFrame::SetExpression(int index, Object* value) {
  Memory::Object_at(GetExpressionAddress(index)) = value;
}


inline Address StandardFrame::caller_fp() const {
  return Memory::Address_at(fp() + StandardFrameConstants::kCallerFPOffset);
}


inline Address StandardFrame::caller_pc() const {
  return Memory::Address_at(ComputePCAddress(fp()));
}


inline Address StandardFrame::ComputePCAddress(Address fp) {
  return fp + StandardFrameConstants::kCallerPCOffset;
}


inline Address StandardFrame::ComputeConstantPoolAddress(Address fp) {
  return fp + StandardFrameConstants::kConstantPoolOffset;
}


inline bool StandardFrame::IsArgumentsAdaptorFrame(Address fp) {
  intptr_t frame_type =
      Memory::intptr_at(fp + TypedFrameConstants::kFrameTypeOffset);
  return frame_type == StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR);
}


inline bool StandardFrame::IsConstructFrame(Address fp) {
  intptr_t frame_type =
      Memory::intptr_at(fp + TypedFrameConstants::kFrameTypeOffset);
  return frame_type == StackFrame::TypeToMarker(StackFrame::CONSTRUCT);
}

inline JavaScriptFrame::JavaScriptFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {}

Address JavaScriptFrame::GetParameterSlot(int index) const {
  int param_count = ComputeParametersCount();
  DCHECK(-1 <= index &&
         (index < param_count ||
          param_count == SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  int parameter_offset = (param_count - index - 1) * kPointerSize;
  return caller_sp() + parameter_offset;
}

inline void JavaScriptFrame::set_receiver(Object* value) {
  Memory::Object_at(GetParameterSlot(-1)) = value;
}


inline bool JavaScriptFrame::has_adapted_arguments() const {
  return IsArgumentsAdaptorFrame(caller_fp());
}


inline Object* JavaScriptFrame::function_slot_object() const {
  const int offset = JavaScriptFrameConstants::kFunctionOffset;
  return Memory::Object_at(fp() + offset);
}

inline StubFrame::StubFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {
}


inline OptimizedFrame::OptimizedFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {
}


inline InterpretedFrame::InterpretedFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {}


inline ArgumentsAdaptorFrame::ArgumentsAdaptorFrame(
    StackFrameIteratorBase* iterator) : JavaScriptFrame(iterator) {
}

inline BuiltinFrame::BuiltinFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {}

inline WasmCompiledFrame::WasmCompiledFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {}

inline WasmInterpreterEntryFrame::WasmInterpreterEntryFrame(
    StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {}

inline WasmToJsFrame::WasmToJsFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline JsToWasmFrame::JsToWasmFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline WasmToWasmFrame::WasmToWasmFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline CWasmEntryFrame::CWasmEntryFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline InternalFrame::InternalFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {
}

inline ConstructFrame::ConstructFrame(StackFrameIteratorBase* iterator)
    : InternalFrame(iterator) {
}

inline BuiltinContinuationFrame::BuiltinContinuationFrame(
    StackFrameIteratorBase* iterator)
    : InternalFrame(iterator) {}

inline JavaScriptBuiltinContinuationFrame::JavaScriptBuiltinContinuationFrame(
    StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {}

inline JavaScriptFrameIterator::JavaScriptFrameIterator(
    Isolate* isolate)
    : iterator_(isolate) {
  if (!done()) Advance();
}

inline JavaScriptFrameIterator::JavaScriptFrameIterator(
    Isolate* isolate, ThreadLocalTop* top)
    : iterator_(isolate, top) {
  if (!done()) Advance();
}

inline JavaScriptFrame* JavaScriptFrameIterator::frame() const {
  // TODO(1233797): The frame hierarchy needs to change. It's
  // problematic that we can't use the safe-cast operator to cast to
  // the JavaScript frame type, because we may encounter arguments
  // adaptor frames.
  StackFrame* frame = iterator_.frame();
  DCHECK(frame->is_java_script() || frame->is_arguments_adaptor());
  return static_cast<JavaScriptFrame*>(frame);
}

inline StandardFrame* StackTraceFrameIterator::frame() const {
  StackFrame* frame = iterator_.frame();
  DCHECK(frame->is_java_script() || frame->is_arguments_adaptor() ||
         frame->is_wasm());
  return static_cast<StandardFrame*>(frame);
}

bool StackTraceFrameIterator::is_javascript() const {
  return frame()->is_java_script();
}

bool StackTraceFrameIterator::is_wasm() const { return frame()->is_wasm(); }

JavaScriptFrame* StackTraceFrameIterator::javascript_frame() const {
  return JavaScriptFrame::cast(frame());
}

inline StackFrame* SafeStackFrameIterator::frame() const {
  DCHECK(!done());
  DCHECK(frame_->is_java_script() || frame_->is_exit() ||
         frame_->is_builtin_exit());
  return frame_;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_FRAMES_INL_H_
