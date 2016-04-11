// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-frames.h"

#include "src/frames-inl.h"

namespace v8 {
namespace internal {

FrameInspector::FrameInspector(JavaScriptFrame* frame,
                               int inlined_jsframe_index, Isolate* isolate)
    : frame_(frame), deoptimized_frame_(NULL), isolate_(isolate) {
  has_adapted_arguments_ = frame_->has_adapted_arguments();
  is_bottommost_ = inlined_jsframe_index == 0;
  is_optimized_ = frame_->is_optimized();
  // Calculate the deoptimized frame.
  if (frame->is_optimized()) {
    // TODO(turbofan): Revisit once we support deoptimization.
    if (frame->LookupCode()->is_turbofanned() &&
        frame->function()->shared()->asm_function() &&
        !FLAG_turbo_asm_deoptimization) {
      is_optimized_ = false;
      return;
    }

    deoptimized_frame_ = Deoptimizer::DebuggerInspectableFrame(
        frame, inlined_jsframe_index, isolate);
  }
}


FrameInspector::~FrameInspector() {
  // Get rid of the calculated deoptimized frame if any.
  if (deoptimized_frame_ != NULL) {
    Deoptimizer::DeleteDebuggerInspectableFrame(deoptimized_frame_, isolate_);
  }
}


int FrameInspector::GetParametersCount() {
  return is_optimized_ ? deoptimized_frame_->parameters_count()
                       : frame_->ComputeParametersCount();
}


Object* FrameInspector::GetFunction() {
  return is_optimized_ ? deoptimized_frame_->GetFunction() : frame_->function();
}


Object* FrameInspector::GetParameter(int index) {
  return is_optimized_ ? deoptimized_frame_->GetParameter(index)
                       : frame_->GetParameter(index);
}


Object* FrameInspector::GetExpression(int index) {
  // TODO(turbofan): Revisit once we support deoptimization.
  if (frame_->LookupCode()->is_turbofanned() &&
      frame_->function()->shared()->asm_function() &&
      !FLAG_turbo_asm_deoptimization) {
    return isolate_->heap()->undefined_value();
  }
  return is_optimized_ ? deoptimized_frame_->GetExpression(index)
                       : frame_->GetExpression(index);
}


int FrameInspector::GetSourcePosition() {
  return is_optimized_ ? deoptimized_frame_->GetSourcePosition()
                       : frame_->LookupCode()->SourcePosition(frame_->pc());
}


bool FrameInspector::IsConstructor() {
  return is_optimized_ && !is_bottommost_
             ? deoptimized_frame_->HasConstructStub()
             : frame_->IsConstructor();
}


Object* FrameInspector::GetContext() {
  return is_optimized_ ? deoptimized_frame_->GetContext() : frame_->context();
}


// To inspect all the provided arguments the frame might need to be
// replaced with the arguments frame.
void FrameInspector::SetArgumentsFrame(JavaScriptFrame* frame) {
  DCHECK(has_adapted_arguments_);
  frame_ = frame;
  is_optimized_ = frame_->is_optimized();
  DCHECK(!is_optimized_);
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
void FrameInspector::MaterializeStackLocals(Handle<JSObject> target,
                                            Handle<ScopeInfo> scope_info) {
  HandleScope scope(isolate_);
  // First fill all parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Do not materialize the parameter if it is shadowed by a context local.
    // TODO(yangguo): check whether this is necessary, now that we materialize
    //                context locals as well.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    Handle<Object> value(i < GetParametersCount()
                             ? GetParameter(i)
                             : isolate_->heap()->undefined_value(),
                         isolate_);
    DCHECK(!value->IsTheHole());

    JSObject::SetOwnPropertyIgnoreAttributes(target, name, value, NONE).Check();
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    Handle<String> name(scope_info->StackLocalName(i));
    Handle<Object> value(GetExpression(scope_info->StackLocalIndex(i)),
                         isolate_);
    if (value->IsTheHole()) value = isolate_->factory()->undefined_value();

    JSObject::SetOwnPropertyIgnoreAttributes(target, name, value, NONE).Check();
  }
}


void FrameInspector::MaterializeStackLocals(Handle<JSObject> target,
                                            Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  MaterializeStackLocals(target, scope_info);
}


void FrameInspector::UpdateStackLocalsFromMaterializedObject(
    Handle<JSObject> target, Handle<ScopeInfo> scope_info) {
  if (is_optimized_) {
    // Optimized frames are not supported. Simply give up.
    return;
  }

  HandleScope scope(isolate_);

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Shadowed parameters were not materialized.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    DCHECK(!frame_->GetParameter(i)->IsTheHole());
    Handle<Object> value =
        Object::GetPropertyOrElement(target, name).ToHandleChecked();
    frame_->SetParameterValue(i, *value);
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    int index = scope_info->StackLocalIndex(i);
    if (frame_->GetExpression(index)->IsTheHole()) continue;
    Handle<Object> value =
        Object::GetPropertyOrElement(
            target, handle(scope_info->StackLocalName(i), isolate_))
            .ToHandleChecked();
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


SaveContext* DebugFrameHelper::FindSavedContextForFrame(
    Isolate* isolate, JavaScriptFrame* frame) {
  SaveContext* save = isolate->save_context();
  while (save != NULL && !save->IsBelowFrame(frame)) {
    save = save->prev();
  }
  DCHECK(save != NULL);
  return save;
}


int DebugFrameHelper::FindIndexedNonNativeFrame(JavaScriptFrameIterator* it,
                                                int index) {
  int count = -1;
  for (; !it->done(); it->Advance()) {
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    it->frame()->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0; i--) {
      // Omit functions from native and extension scripts.
      if (!frames[i].function()->shared()->IsSubjectToDebugging()) continue;
      if (++count == index) return i;
    }
  }
  return -1;
}


}  // namespace internal
}  // namespace v8
