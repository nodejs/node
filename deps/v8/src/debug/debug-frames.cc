// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-frames.h"

#include "src/builtins/accessors.h"
#include "src/execution/frames-inl.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

FrameInspector::FrameInspector(StandardFrame* frame, int inlined_frame_index,
                               Isolate* isolate)
    : frame_(frame),
      inlined_frame_index_(inlined_frame_index),
      isolate_(isolate) {
  // Extract the relevant information from the frame summary and discard it.
  FrameSummary summary = FrameSummary::Get(frame, inlined_frame_index);
  summary.EnsureSourcePositionsAvailable();

  is_constructor_ = summary.is_constructor();
  source_position_ = summary.SourcePosition();
  function_name_ = summary.FunctionName();
  script_ = Handle<Script>::cast(summary.script());
  receiver_ = summary.receiver();

  if (summary.IsJavaScript()) {
    function_ = summary.AsJavaScript().function();
  }

  JavaScriptFrame* js_frame =
      frame->is_java_script() ? javascript_frame() : nullptr;
  DCHECK(js_frame || frame->is_wasm());
  has_adapted_arguments_ = js_frame && js_frame->has_adapted_arguments();
  is_optimized_ = frame_->is_optimized();
  is_interpreted_ = frame_->is_interpreted();

  // Calculate the deoptimized frame.
  if (is_optimized_) {
    DCHECK_NOT_NULL(js_frame);
    deoptimized_frame_.reset(Deoptimizer::DebuggerInspectableFrame(
        js_frame, inlined_frame_index, isolate));
  }
}

// Destructor needs to be defined in the .cc file, because it instantiates
// std::unique_ptr destructors but the types are not known in the header.
FrameInspector::~FrameInspector() = default;

JavaScriptFrame* FrameInspector::javascript_frame() {
  return frame_->is_arguments_adaptor() ? ArgumentsAdaptorFrame::cast(frame_)
                                        : JavaScriptFrame::cast(frame_);
}

int FrameInspector::GetParametersCount() {
  if (is_optimized_) return deoptimized_frame_->parameters_count();
  return frame_->ComputeParametersCount();
}

Handle<Object> FrameInspector::GetParameter(int index) {
  if (is_optimized_) return deoptimized_frame_->GetParameter(index);
  return handle(frame_->GetParameter(index), isolate_);
}

Handle<Object> FrameInspector::GetExpression(int index) {
  return is_optimized_ ? deoptimized_frame_->GetExpression(index)
                       : handle(frame_->GetExpression(index), isolate_);
}

Handle<Object> FrameInspector::GetContext() {
  return deoptimized_frame_ ? deoptimized_frame_->GetContext()
                            : handle(frame_->context(), isolate_);
}

bool FrameInspector::IsWasm() { return frame_->is_wasm(); }

bool FrameInspector::IsJavaScript() { return frame_->is_java_script(); }

bool FrameInspector::ParameterIsShadowedByContextLocal(
    Handle<ScopeInfo> info, Handle<String> parameter_name) {
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  IsStaticFlag is_static_flag;
  return ScopeInfo::ContextSlotIndex(*info, *parameter_name, &mode, &init_flag,
                                     &maybe_assigned_flag,
                                     &is_static_flag) != -1;
}

RedirectActiveFunctions::RedirectActiveFunctions(SharedFunctionInfo shared,
                                                 Mode mode)
    : shared_(shared), mode_(mode) {
  DCHECK(shared.HasBytecodeArray());
  if (mode == Mode::kUseDebugBytecode) {
    DCHECK(shared.HasDebugInfo());
  }
}

void RedirectActiveFunctions::VisitThread(Isolate* isolate,
                                          ThreadLocalTop* top) {
  for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction function = frame->function();
    if (!frame->is_interpreted()) continue;
    if (function.shared() != shared_) continue;
    InterpretedFrame* interpreted_frame =
        reinterpret_cast<InterpretedFrame*>(frame);
    BytecodeArray bytecode = mode_ == Mode::kUseDebugBytecode
                                 ? shared_.GetDebugInfo().DebugBytecodeArray()
                                 : shared_.GetBytecodeArray();
    interpreted_frame->PatchBytecodeArray(bytecode);
  }
}

}  // namespace internal
}  // namespace v8
