// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_FRAMES_INL_H_
#define V8_FRAMES_INL_H_

#include "frames.h"
#include "isolate.h"
#include "v8memory.h"

#if V8_TARGET_ARCH_IA32
#include "ia32/frames-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/frames-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/frames-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/frames-mips.h"
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
  StackFrame::IteratePc(v, pc_address(), holder);
}


inline StackHandler* StackHandler::FromAddress(Address address) {
  return reinterpret_cast<StackHandler*>(address);
}


inline StackHandler::State StackHandler::state() const {
  const int offset = StackHandlerConstants::kStateOffset;
  return static_cast<State>(Memory::int_at(address() + offset));
}


inline Object** StackHandler::context_address() const {
  const int offset = StackHandlerConstants::kContextOffset;
  return reinterpret_cast<Object**>(address() + offset);
}


inline Address* StackHandler::pc_address() const {
  const int offset = StackHandlerConstants::kPCOffset;
  return reinterpret_cast<Address*>(address() + offset);
}


inline StackFrame::StackFrame(StackFrameIterator* iterator)
    : iterator_(iterator), isolate_(iterator_->isolate()) {
}


inline StackHandler* StackFrame::top_handler() const {
  return iterator_->handler();
}


inline Code* StackFrame::GetContainingCode(Isolate* isolate, Address pc) {
  return isolate->pc_to_code_cache()->GetCacheEntry(pc)->code;
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


inline bool StandardFrame::IsArgumentsAdaptorFrame(Address fp) {
  Object* marker =
      Memory::Object_at(fp + StandardFrameConstants::kContextOffset);
  return marker == Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR);
}


inline bool StandardFrame::IsConstructFrame(Address fp) {
  Object* marker =
      Memory::Object_at(fp + StandardFrameConstants::kMarkerOffset);
  return marker == Smi::FromInt(CONSTRUCT);
}


Address JavaScriptFrame::GetParameterSlot(int index) const {
  int param_count = ComputeParametersCount();
  ASSERT(-1 <= index && index < param_count);
  int parameter_offset = (param_count - index - 1) * kPointerSize;
  return caller_sp() + parameter_offset;
}


Object* JavaScriptFrame::GetParameter(int index) const {
  return Memory::Object_at(GetParameterSlot(index));
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


inline Object* JavaScriptFrame::function() const {
  Object* result = function_slot_object();
  ASSERT(result->IsJSFunction());
  return result;
}


template<typename Iterator>
inline JavaScriptFrameIteratorTemp<Iterator>::JavaScriptFrameIteratorTemp(
    Isolate* isolate)
    : iterator_(isolate) {
  if (!done()) Advance();
}

template<typename Iterator>
inline JavaScriptFrame* JavaScriptFrameIteratorTemp<Iterator>::frame() const {
  // TODO(1233797): The frame hierarchy needs to change. It's
  // problematic that we can't use the safe-cast operator to cast to
  // the JavaScript frame type, because we may encounter arguments
  // adaptor frames.
  StackFrame* frame = iterator_.frame();
  ASSERT(frame->is_java_script() || frame->is_arguments_adaptor());
  return static_cast<JavaScriptFrame*>(frame);
}


template<typename Iterator>
JavaScriptFrameIteratorTemp<Iterator>::JavaScriptFrameIteratorTemp(
    Isolate* isolate, StackFrame::Id id)
    : iterator_(isolate) {
  AdvanceToId(id);
}


template<typename Iterator>
void JavaScriptFrameIteratorTemp<Iterator>::Advance() {
  do {
    iterator_.Advance();
  } while (!iterator_.done() && !iterator_.frame()->is_java_script());
}


template<typename Iterator>
void JavaScriptFrameIteratorTemp<Iterator>::AdvanceToArgumentsFrame() {
  if (!frame()->has_adapted_arguments()) return;
  iterator_.Advance();
  ASSERT(iterator_.frame()->is_arguments_adaptor());
}


template<typename Iterator>
void JavaScriptFrameIteratorTemp<Iterator>::AdvanceToId(StackFrame::Id id) {
  while (!done()) {
    Advance();
    if (frame()->id() == id) return;
  }
}


template<typename Iterator>
void JavaScriptFrameIteratorTemp<Iterator>::Reset() {
  iterator_.Reset();
  if (!done()) Advance();
}


} }  // namespace v8::internal

#endif  // V8_FRAMES_INL_H_
