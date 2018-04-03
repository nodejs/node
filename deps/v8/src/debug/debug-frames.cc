// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-frames.h"

#include "src/accessors.h"
#include "src/frames-inl.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

FrameInspector::FrameInspector(StandardFrame* frame, int inlined_frame_index,
                               Isolate* isolate)
    : frame_(frame),
      isolate_(isolate) {
  // Extract the relevant information from the frame summary and discard it.
  FrameSummary summary = FrameSummary::Get(frame, inlined_frame_index);

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
  } else if (frame_->is_wasm_interpreter_entry()) {
    wasm_interpreted_frame_ =
        summary.AsWasm().wasm_instance()->debug_info()->GetInterpretedFrame(
            frame_->fp(), inlined_frame_index);
    DCHECK(wasm_interpreted_frame_);
  }
}

FrameInspector::~FrameInspector() {
  // Destructor needs to be defined in the .cc file, because it instantiates
  // std::unique_ptr destructors but the types are not known in the header.
}

int FrameInspector::GetParametersCount() {
  if (is_optimized_) return deoptimized_frame_->parameters_count();
  if (wasm_interpreted_frame_)
    return wasm_interpreted_frame_->GetParameterCount();
  return frame_->ComputeParametersCount();
}

Handle<Object> FrameInspector::GetParameter(int index) {
  if (is_optimized_) return deoptimized_frame_->GetParameter(index);
  // TODO(clemensh): Handle wasm_interpreted_frame_.
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

// To inspect all the provided arguments the frame might need to be
// replaced with the arguments frame.
void FrameInspector::SetArgumentsFrame(StandardFrame* frame) {
  DCHECK(has_adapted_arguments_);
  DCHECK(frame->is_arguments_adaptor());
  frame_ = frame;
  is_optimized_ = frame_->is_optimized();
  is_interpreted_ = frame_->is_interpreted();
  DCHECK(!is_optimized_);
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
void FrameInspector::MaterializeStackLocals(Handle<JSObject> target,
                                            Handle<ScopeInfo> scope_info,
                                            bool materialize_arguments_object) {
  HandleScope scope(isolate_);
  // First fill all parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Do not materialize the parameter if it is shadowed by a context local.
    // TODO(yangguo): check whether this is necessary, now that we materialize
    //                context locals as well.
    Handle<String> name(scope_info->ParameterName(i));
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    Handle<Object> value =
        i < GetParametersCount()
            ? GetParameter(i)
            : Handle<Object>::cast(isolate_->factory()->undefined_value());
    DCHECK(!value->IsTheHole(isolate_));

    JSObject::SetOwnPropertyIgnoreAttributes(target, name, value, NONE).Check();
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    Handle<String> name(scope_info->StackLocalName(i));
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    Handle<Object> value = GetExpression(scope_info->StackLocalIndex(i));
    // TODO(yangguo): We convert optimized out values to {undefined} when they
    // are passed to the debugger. Eventually we should handle them somehow.
    if (value->IsTheHole(isolate_)) {
      value = isolate_->factory()->undefined_value();
    }
    if (value->IsOptimizedOut(isolate_)) {
      if (materialize_arguments_object) {
        Handle<String> arguments_str = isolate_->factory()->arguments_string();
        if (String::Equals(name, arguments_str)) continue;
      }
      value = isolate_->factory()->undefined_value();
    }
    JSObject::SetOwnPropertyIgnoreAttributes(target, name, value, NONE).Check();
  }
}


void FrameInspector::UpdateStackLocalsFromMaterializedObject(
    Handle<JSObject> target, Handle<ScopeInfo> scope_info) {
  // Optimized frames and wasm frames are not supported. Simply give up.
  if (is_optimized_ || frame_->is_wasm()) return;

  HandleScope scope(isolate_);

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Shadowed parameters were not materialized.
    Handle<String> name(scope_info->ParameterName(i));
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    DCHECK(!javascript_frame()->GetParameter(i)->IsTheHole(isolate_));
    Handle<Object> value =
        Object::GetPropertyOrElement(target, name).ToHandleChecked();
    javascript_frame()->SetParameterValue(i, *value);
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    Handle<String> name(scope_info->StackLocalName(i));
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    int index = scope_info->StackLocalIndex(i);
    if (frame_->GetExpression(index)->IsTheHole(isolate_)) continue;
    Handle<Object> value =
        Object::GetPropertyOrElement(target, name).ToHandleChecked();
    frame_->SetExpression(index, *value);
  }
}


bool FrameInspector::ParameterIsShadowedByContextLocal(
    Handle<ScopeInfo> info, Handle<String> parameter_name) {
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  return ScopeInfo::ContextSlotIndex(info, parameter_name, &mode, &init_flag,
                                     &maybe_assigned_flag) != -1;
}

SaveContext* DebugFrameHelper::FindSavedContextForFrame(Isolate* isolate,
                                                        StandardFrame* frame) {
  SaveContext* save = isolate->save_context();
  while (save != nullptr && !save->IsBelowFrame(frame)) {
    save = save->prev();
  }
  DCHECK(save != nullptr);
  return save;
}

int DebugFrameHelper::FindIndexedNonNativeFrame(StackTraceFrameIterator* it,
                                                int index) {
  int count = -1;
  for (; !it->done(); it->Advance()) {
    std::vector<FrameSummary> frames;
    it->frame()->Summarize(&frames);
    for (size_t i = frames.size(); i != 0; i--) {
      // Omit functions from native and extension scripts.
      if (!frames[i - 1].is_subject_to_debugging()) continue;
      if (++count == index) return static_cast<int>(i) - 1;
    }
  }
  return -1;
}


}  // namespace internal
}  // namespace v8
