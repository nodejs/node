// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#ifdef ARM
#include "frames-arm.h"
#else
#include "frames-ia32.h"
#endif


namespace v8 { namespace internal {


inline Address StackHandler::address() const {
  // NOTE: There's an obvious problem with the address of the NULL
  // stack handler. Right now, it benefits us that the subtraction
  // leads to a very high address (above everything else on the
  // stack), but maybe we should stop relying on it?
  const int displacement = StackHandlerConstants::kAddressDisplacement;
  Address address = reinterpret_cast<Address>(const_cast<StackHandler*>(this));
  return address + displacement;
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


inline void StackHandler::Iterate(ObjectVisitor* v) const {
  // Stack handlers do not contain any pointers that need to be
  // traversed. The only field that have to worry about is the code
  // field which is unused and should always be uninitialized.
#ifdef DEBUG
  const int offset = StackHandlerConstants::kCodeOffset;
  Object* code = Memory::Object_at(address() + offset);
  ASSERT(Smi::cast(code)->value() == StackHandler::kCodeNotPresent);
#endif
}


inline StackHandler* StackHandler::FromAddress(Address address) {
  return reinterpret_cast<StackHandler*>(address);
}


inline StackHandler::State StackHandler::state() const {
  const int offset = StackHandlerConstants::kStateOffset;
  return static_cast<State>(Memory::int_at(address() + offset));
}


inline Address StackHandler::pc() const {
  const int offset = StackHandlerConstants::kPCOffset;
  return Memory::Address_at(address() + offset);
}


inline void StackHandler::set_pc(Address value) {
  const int offset = StackHandlerConstants::kPCOffset;
  Memory::Address_at(address() + offset) = value;
}


inline StackHandler* StackFrame::top_handler() const {
  return iterator_->handler();
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


inline Address StandardFrame::caller_sp() const {
  return pp();
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
  int context = Memory::int_at(fp + StandardFrameConstants::kContextOffset);
  return context == ArgumentsAdaptorFrame::SENTINEL;
}


inline bool StandardFrame::IsConstructFrame(Address fp) {
  Object* marker =
      Memory::Object_at(fp + StandardFrameConstants::kMarkerOffset);
  return marker == Smi::FromInt(CONSTRUCT);
}


inline Object* JavaScriptFrame::receiver() const {
  const int offset = JavaScriptFrameConstants::kReceiverOffset;
  return Memory::Object_at(pp() + offset);
}


inline void JavaScriptFrame::set_receiver(Object* value) {
  const int offset = JavaScriptFrameConstants::kReceiverOffset;
  Memory::Object_at(pp() + offset) = value;
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
    StackFrame::Id id) {
  while (!done()) {
    Advance();
    if (frame()->id() == id) return;
  }
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
void JavaScriptFrameIteratorTemp<Iterator>::Reset() {
  iterator_.Reset();
  if (!done()) Advance();
}


} }  // namespace v8::internal

#endif  // V8_FRAMES_INL_H_
