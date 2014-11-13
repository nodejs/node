// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FRAMES_INL_H_
#define V8_FRAMES_INL_H_

#include "src/frames.h"
#include "src/isolate.h"
#include "src/v8memory.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/frames-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/x64/frames-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/frames-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/arm/frames-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/frames-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/frames-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/x87/frames-x87.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


inline Address StackHandler::address() const {
  return reinterpret_cast<Address>(const_cast<StackHandler*>(this));
}


inline StackHandler* StackHandler::next() const {
  const int offset = StackHandlerConstants::kNextOffset;
  return FromAddress(Memory::Address_at(address() + offset));
}


inline bool StackHandler::includes(Address address) const {
  Address start = this->address();
  Address end = start + StackHandlerConstants::kSize;
  return start <= address && address <= end;
}


inline void StackHandler::Iterate(ObjectVisitor* v, Code* holder) const {
  v->VisitPointer(context_address());
  v->VisitPointer(code_address());
}


inline StackHandler* StackHandler::FromAddress(Address address) {
  return reinterpret_cast<StackHandler*>(address);
}


inline bool StackHandler::is_js_entry() const {
  return kind() == JS_ENTRY;
}


inline bool StackHandler::is_catch() const {
  return kind() == CATCH;
}


inline bool StackHandler::is_finally() const {
  return kind() == FINALLY;
}


inline StackHandler::Kind StackHandler::kind() const {
  const int offset = StackHandlerConstants::kStateIntOffset;
  return KindField::decode(Memory::unsigned_at(address() + offset));
}


inline unsigned StackHandler::index() const {
  const int offset = StackHandlerConstants::kStateIntOffset;
  return IndexField::decode(Memory::unsigned_at(address() + offset));
}


inline Object** StackHandler::context_address() const {
  const int offset = StackHandlerConstants::kContextOffset;
  return reinterpret_cast<Object**>(address() + offset);
}


inline Object** StackHandler::code_address() const {
  const int offset = StackHandlerConstants::kCodeOffset;
  return reinterpret_cast<Object**>(address() + offset);
}


inline StackFrame::StackFrame(StackFrameIteratorBase* iterator)
    : iterator_(iterator), isolate_(iterator_->isolate()) {
}


inline StackHandler* StackFrame::top_handler() const {
  return iterator_->handler();
}


inline Code* StackFrame::LookupCode() const {
  return GetContainingCode(isolate(), pc());
}


inline Code* StackFrame::GetContainingCode(Isolate* isolate, Address pc) {
  return isolate->inner_pointer_to_code_cache()->GetCacheEntry(pc)->code;
}


inline Address* StackFrame::ResolveReturnAddressLocation(Address* pc_address) {
  if (return_address_location_resolver_ == NULL) {
    return pc_address;
  } else {
    return reinterpret_cast<Address*>(
        return_address_location_resolver_(
            reinterpret_cast<uintptr_t>(pc_address)));
  }
}


inline EntryFrame::EntryFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {
}


inline EntryConstructFrame::EntryConstructFrame(
    StackFrameIteratorBase* iterator)
    : EntryFrame(iterator) {
}


inline ExitFrame::ExitFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {
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


inline Object* StandardFrame::context() const {
  const int offset = StandardFrameConstants::kContextOffset;
  return Memory::Object_at(fp() + offset);
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
  Object* marker =
      Memory::Object_at(fp + StandardFrameConstants::kContextOffset);
  return marker == Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR);
}


inline bool StandardFrame::IsConstructFrame(Address fp) {
  Object* marker =
      Memory::Object_at(fp + StandardFrameConstants::kMarkerOffset);
  return marker == Smi::FromInt(StackFrame::CONSTRUCT);
}


inline JavaScriptFrame::JavaScriptFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {
}


Address JavaScriptFrame::GetParameterSlot(int index) const {
  int param_count = ComputeParametersCount();
  DCHECK(-1 <= index && index < param_count);
  int parameter_offset = (param_count - index - 1) * kPointerSize;
  return caller_sp() + parameter_offset;
}


Object* JavaScriptFrame::GetParameter(int index) const {
  return Memory::Object_at(GetParameterSlot(index));
}


inline Address JavaScriptFrame::GetOperandSlot(int index) const {
  Address base = fp() + JavaScriptFrameConstants::kLocal0Offset;
  DCHECK(IsAddressAligned(base, kPointerSize));
  DCHECK_EQ(type(), JAVA_SCRIPT);
  DCHECK_LT(index, ComputeOperandsCount());
  DCHECK_LE(0, index);
  // Operand stack grows down.
  return base - index * kPointerSize;
}


inline Object* JavaScriptFrame::GetOperand(int index) const {
  return Memory::Object_at(GetOperandSlot(index));
}


inline int JavaScriptFrame::ComputeOperandsCount() const {
  Address base = fp() + JavaScriptFrameConstants::kLocal0Offset;
  // Base points to low address of first operand and stack grows down, so add
  // kPointerSize to get the actual stack size.
  intptr_t stack_size_in_bytes = (base + kPointerSize) - sp();
  DCHECK(IsAligned(stack_size_in_bytes, kPointerSize));
  DCHECK(type() == JAVA_SCRIPT);
  DCHECK(stack_size_in_bytes >= 0);
  return static_cast<int>(stack_size_in_bytes >> kPointerSizeLog2);
}


inline Object* JavaScriptFrame::receiver() const {
  return GetParameter(-1);
}


inline void JavaScriptFrame::set_receiver(Object* value) {
  Memory::Object_at(GetParameterSlot(-1)) = value;
}


inline bool JavaScriptFrame::has_adapted_arguments() const {
  return IsArgumentsAdaptorFrame(caller_fp());
}


inline JSFunction* JavaScriptFrame::function() const {
  return JSFunction::cast(function_slot_object());
}


inline StubFrame::StubFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {
}


inline OptimizedFrame::OptimizedFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {
}


inline ArgumentsAdaptorFrame::ArgumentsAdaptorFrame(
    StackFrameIteratorBase* iterator) : JavaScriptFrame(iterator) {
}


inline InternalFrame::InternalFrame(StackFrameIteratorBase* iterator)
    : StandardFrame(iterator) {
}


inline StubFailureTrampolineFrame::StubFailureTrampolineFrame(
    StackFrameIteratorBase* iterator) : StandardFrame(iterator) {
}


inline ConstructFrame::ConstructFrame(StackFrameIteratorBase* iterator)
    : InternalFrame(iterator) {
}


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


inline StackFrame* SafeStackFrameIterator::frame() const {
  DCHECK(!done());
  DCHECK(frame_->is_java_script() || frame_->is_exit());
  return frame_;
}


} }  // namespace v8::internal

#endif  // V8_FRAMES_INL_H_
