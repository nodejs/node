// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_FRAMES_INL_H_
#define V8_EXECUTION_FRAMES_INL_H_

#include "src/base/memory.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/execution/pointer-authentication.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

class InnerPointerToCodeCache {
 public:
  struct InnerPointerToCodeCacheEntry {
    Address inner_pointer;
    Code code;
    SafepointEntry safepoint_entry;
  };

  explicit InnerPointerToCodeCache(Isolate* isolate) : isolate_(isolate) {
    Flush();
  }

  InnerPointerToCodeCache(const InnerPointerToCodeCache&) = delete;
  InnerPointerToCodeCache& operator=(const InnerPointerToCodeCache&) = delete;

  void Flush() { memset(static_cast<void*>(&cache_[0]), 0, sizeof(cache_)); }

  InnerPointerToCodeCacheEntry* GetCacheEntry(Address inner_pointer);

 private:
  InnerPointerToCodeCacheEntry* cache(int index) { return &cache_[index]; }

  Isolate* isolate_;

  static const int kInnerPointerToCodeCacheSize = 1024;
  InnerPointerToCodeCacheEntry cache_[kInnerPointerToCodeCacheSize];
};

inline Address StackHandler::address() const {
  return reinterpret_cast<Address>(const_cast<StackHandler*>(this));
}

inline StackHandler* StackHandler::next() const {
  const int offset = StackHandlerConstants::kNextOffset;
  return FromAddress(base::Memory<Address>(address() + offset));
}

inline Address StackHandler::next_address() const {
  return base::Memory<Address>(address() + StackHandlerConstants::kNextOffset);
}

inline StackHandler* StackHandler::FromAddress(Address address) {
  return reinterpret_cast<StackHandler*>(address);
}

inline StackFrame::StackFrame(StackFrameIteratorBase* iterator)
    : iterator_(iterator), isolate_(iterator_->isolate()) {}

inline StackHandler* StackFrame::top_handler() const {
  return iterator_->handler();
}

inline Address StackFrame::callee_pc() const {
  return state_.callee_pc_address ? ReadPC(state_.callee_pc_address)
                                  : kNullAddress;
}

inline Address StackFrame::pc() const { return ReadPC(pc_address()); }

inline Address StackFrame::unauthenticated_pc() const {
  return PointerAuthentication::StripPAC(*pc_address());
}

inline Address StackFrame::ReadPC(Address* pc_address) {
  return PointerAuthentication::AuthenticatePC(pc_address, kSystemPointerSize);
}

inline Address* StackFrame::ResolveReturnAddressLocation(Address* pc_address) {
  if (return_address_location_resolver_ == nullptr) {
    return pc_address;
  } else {
    return reinterpret_cast<Address*>(return_address_location_resolver_(
        reinterpret_cast<uintptr_t>(pc_address)));
  }
}

inline TypedFrame::TypedFrame(StackFrameIteratorBase* iterator)
    : CommonFrame(iterator) {}

inline CommonFrameWithJSLinkage::CommonFrameWithJSLinkage(
    StackFrameIteratorBase* iterator)
    : CommonFrame(iterator) {}

inline TypedFrameWithJSLinkage::TypedFrameWithJSLinkage(
    StackFrameIteratorBase* iterator)
    : CommonFrameWithJSLinkage(iterator) {}

inline NativeFrame::NativeFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline EntryFrame::EntryFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline ConstructEntryFrame::ConstructEntryFrame(
    StackFrameIteratorBase* iterator)
    : EntryFrame(iterator) {}

inline ExitFrame::ExitFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline BuiltinExitFrame::BuiltinExitFrame(StackFrameIteratorBase* iterator)
    : ExitFrame(iterator) {}

inline Object BuiltinExitFrame::receiver_slot_object() const {
  // The receiver is the first argument on the frame.
  // fp[1]: return address.
  // ------- fixed extra builtin arguments -------
  // fp[2]: new target.
  // fp[3]: target.
  // fp[4]: argc.
  // fp[5]: hole.
  // ------- JS stack arguments ------
  // fp[6]: receiver
  const int receiverOffset = BuiltinExitFrameConstants::kFirstArgumentOffset;
  return Object(base::Memory<Address>(fp() + receiverOffset));
}

inline Object BuiltinExitFrame::argc_slot_object() const {
  return Object(
      base::Memory<Address>(fp() + BuiltinExitFrameConstants::kArgcOffset));
}

inline Object BuiltinExitFrame::target_slot_object() const {
  return Object(
      base::Memory<Address>(fp() + BuiltinExitFrameConstants::kTargetOffset));
}

inline Object BuiltinExitFrame::new_target_slot_object() const {
  return Object(base::Memory<Address>(
      fp() + BuiltinExitFrameConstants::kNewTargetOffset));
}

inline CommonFrame::CommonFrame(StackFrameIteratorBase* iterator)
    : StackFrame(iterator) {}

inline Object CommonFrame::GetExpression(int index) const {
  return Object(base::Memory<Address>(GetExpressionAddress(index)));
}

inline void CommonFrame::SetExpression(int index, Object value) {
  base::Memory<Address>(GetExpressionAddress(index)) = value.ptr();
}

inline Address CommonFrame::caller_fp() const {
  return base::Memory<Address>(fp() + StandardFrameConstants::kCallerFPOffset);
}

inline Address CommonFrame::caller_pc() const {
  return ReadPC(reinterpret_cast<Address*>(ComputePCAddress(fp())));
}

inline Address CommonFrame::ComputePCAddress(Address fp) {
  return fp + StandardFrameConstants::kCallerPCOffset;
}

inline Address CommonFrame::ComputeConstantPoolAddress(Address fp) {
  return fp + StandardFrameConstants::kConstantPoolOffset;
}

inline bool CommonFrameWithJSLinkage::IsConstructFrame(Address fp) {
  intptr_t frame_type =
      base::Memory<intptr_t>(fp + TypedFrameConstants::kFrameTypeOffset);
  return frame_type == StackFrame::TypeToMarker(StackFrame::CONSTRUCT);
}

inline JavaScriptFrame::JavaScriptFrame(StackFrameIteratorBase* iterator)
    : CommonFrameWithJSLinkage(iterator) {}

Address CommonFrameWithJSLinkage::GetParameterSlot(int index) const {
  DCHECK_LE(-1, index);
  DCHECK_LT(index,
            std::max(GetActualArgumentCount(), ComputeParametersCount()));
  int parameter_offset = (index + 1) * kSystemPointerSize;
  return caller_sp() + parameter_offset;
}

inline int CommonFrameWithJSLinkage::GetActualArgumentCount() const {
  return 0;
}

inline void JavaScriptFrame::set_receiver(Object value) {
  base::Memory<Address>(GetParameterSlot(-1)) = value.ptr();
}

inline Object JavaScriptFrame::function_slot_object() const {
  const int offset = StandardFrameConstants::kFunctionOffset;
  return Object(base::Memory<Address>(fp() + offset));
}

inline StubFrame::StubFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline OptimizedFrame::OptimizedFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {}

inline UnoptimizedFrame::UnoptimizedFrame(StackFrameIteratorBase* iterator)
    : JavaScriptFrame(iterator) {}

inline InterpretedFrame::InterpretedFrame(StackFrameIteratorBase* iterator)
    : UnoptimizedFrame(iterator) {}

inline BaselineFrame::BaselineFrame(StackFrameIteratorBase* iterator)
    : UnoptimizedFrame(iterator) {}

inline BuiltinFrame::BuiltinFrame(StackFrameIteratorBase* iterator)
    : TypedFrameWithJSLinkage(iterator) {}

#if V8_ENABLE_WEBASSEMBLY
inline WasmFrame::WasmFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline WasmExitFrame::WasmExitFrame(StackFrameIteratorBase* iterator)
    : WasmFrame(iterator) {}

inline WasmDebugBreakFrame::WasmDebugBreakFrame(
    StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline WasmToJsFrame::WasmToJsFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline JsToWasmFrame::JsToWasmFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline CWasmEntryFrame::CWasmEntryFrame(StackFrameIteratorBase* iterator)
    : StubFrame(iterator) {}

inline WasmCompileLazyFrame::WasmCompileLazyFrame(
    StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}
#endif  // V8_ENABLE_WEBASSEMBLY

inline InternalFrame::InternalFrame(StackFrameIteratorBase* iterator)
    : TypedFrame(iterator) {}

inline ConstructFrame::ConstructFrame(StackFrameIteratorBase* iterator)
    : InternalFrame(iterator) {}

inline BuiltinContinuationFrame::BuiltinContinuationFrame(
    StackFrameIteratorBase* iterator)
    : InternalFrame(iterator) {}

inline JavaScriptBuiltinContinuationFrame::JavaScriptBuiltinContinuationFrame(
    StackFrameIteratorBase* iterator)
    : TypedFrameWithJSLinkage(iterator) {}

inline JavaScriptBuiltinContinuationWithCatchFrame::
    JavaScriptBuiltinContinuationWithCatchFrame(
        StackFrameIteratorBase* iterator)
    : JavaScriptBuiltinContinuationFrame(iterator) {}

inline JavaScriptFrameIterator::JavaScriptFrameIterator(Isolate* isolate)
    : iterator_(isolate) {
  if (!done()) Advance();
}

inline JavaScriptFrameIterator::JavaScriptFrameIterator(Isolate* isolate,
                                                        ThreadLocalTop* top)
    : iterator_(isolate, top) {
  if (!done()) Advance();
}

inline JavaScriptFrame* JavaScriptFrameIterator::frame() const {
  StackFrame* frame = iterator_.frame();
  return JavaScriptFrame::cast(frame);
}

inline JavaScriptFrame* JavaScriptFrameIterator::Reframe() {
  StackFrame* frame = iterator_.Reframe();
  return JavaScriptFrame::cast(frame);
}

inline CommonFrame* StackTraceFrameIterator::frame() const {
  StackFrame* frame = iterator_.frame();
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(frame->is_java_script() || frame->is_wasm());
#else
  DCHECK(frame->is_java_script());
#endif  // V8_ENABLE_WEBASSEMBLY
  return static_cast<CommonFrame*>(frame);
}

inline CommonFrame* StackTraceFrameIterator::Reframe() {
  iterator_.Reframe();
  return frame();
}

bool StackTraceFrameIterator::is_javascript() const {
  return frame()->is_java_script();
}

#if V8_ENABLE_WEBASSEMBLY
bool StackTraceFrameIterator::is_wasm() const { return frame()->is_wasm(); }
#endif  // V8_ENABLE_WEBASSEMBLY

JavaScriptFrame* StackTraceFrameIterator::javascript_frame() const {
  return JavaScriptFrame::cast(frame());
}

inline StackFrame* SafeStackFrameIterator::frame() const {
  DCHECK(!done());
#if V8_ENABLE_WEBASSEMBLY
  DCHECK(frame_->is_java_script() || frame_->is_exit() ||
         frame_->is_builtin_exit() || frame_->is_wasm() ||
         frame_->is_wasm_to_js() || frame_->is_js_to_wasm());
#else
  DCHECK(frame_->is_java_script() || frame_->is_exit() ||
         frame_->is_builtin_exit());
#endif  // V8_ENABLE_WEBASSEMBLY
  return frame_;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_FRAMES_INL_H_
