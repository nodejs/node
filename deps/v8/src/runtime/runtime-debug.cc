// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/arguments.h"
#include "src/compiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/parser.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DebugBreak) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  isolate->debug()->HandleDebugBreak();
  return isolate->heap()->undefined_value();
}


// Helper functions for wrapping and unwrapping stack frame ids.
static Smi* WrapFrameId(StackFrame::Id id) {
  DCHECK(IsAligned(OffsetFrom(id), static_cast<intptr_t>(4)));
  return Smi::FromInt(id >> 2);
}


static StackFrame::Id UnwrapFrameId(int wrapped) {
  return static_cast<StackFrame::Id>(wrapped << 2);
}


// Adds a JavaScript function as a debug event listener.
// args[0]: debug event listener function to set or null or undefined for
//          clearing the event listener function
// args[1]: object supplied during callback
RUNTIME_FUNCTION(Runtime_SetDebugEventListener) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsJSFunction() || args[0]->IsUndefined() ||
                 args[0]->IsNull());
  CONVERT_ARG_HANDLE_CHECKED(Object, callback, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, data, 1);
  isolate->debug()->SetEventListener(callback, data);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ScheduleBreak) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  isolate->stack_guard()->RequestDebugBreak();
  return isolate->heap()->undefined_value();
}


static Handle<Object> DebugGetProperty(LookupIterator* it,
                                       bool* has_caught = NULL) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::ACCESS_CHECK:
        // Ignore access checks.
        break;
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
      case LookupIterator::INTERCEPTOR:
      case LookupIterator::JSPROXY:
        return it->isolate()->factory()->undefined_value();
      case LookupIterator::ACCESSOR: {
        Handle<Object> accessors = it->GetAccessors();
        if (!accessors->IsAccessorInfo()) {
          return it->isolate()->factory()->undefined_value();
        }
        MaybeHandle<Object> maybe_result = JSObject::GetPropertyWithAccessor(
            it->GetReceiver(), it->name(), it->GetHolder<JSObject>(),
            accessors);
        Handle<Object> result;
        if (!maybe_result.ToHandle(&result)) {
          result = handle(it->isolate()->pending_exception(), it->isolate());
          it->isolate()->clear_pending_exception();
          if (has_caught != NULL) *has_caught = true;
        }
        return result;
      }

      case LookupIterator::DATA:
        return it->GetDataValue();
    }
  }

  return it->isolate()->factory()->undefined_value();
}


// Get debugger related details for an object property, in the following format:
// 0: Property value
// 1: Property details
// 2: Property value is exception
// 3: Getter function if defined
// 4: Setter function if defined
// Items 2-4 are only filled if the property has either a getter or a setter.
RUNTIME_FUNCTION(Runtime_DebugGetPropertyDetails) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  // Make sure to set the current context to the context before the debugger was
  // entered (if the debugger is entered). The reason for switching context here
  // is that for some property lookups (accessors and interceptors) callbacks
  // into the embedding application can occour, and the embedding application
  // could have the assumption that its own native context is the current
  // context and not some internal debugger context.
  SaveContext save(isolate);
  if (isolate->debug()->in_debug_scope()) {
    isolate->set_context(*isolate->debug()->debugger_entry()->GetContext());
  }

  // Check if the name is trivially convertible to an index and get the element
  // if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<FixedArray> details = isolate->factory()->NewFixedArray(2);
    Handle<Object> element_or_char;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, element_or_char,
        Runtime::GetElementOrCharAt(isolate, obj, index));
    details->set(0, *element_or_char);
    details->set(1, PropertyDetails::Empty().AsSmi());
    return *isolate->factory()->NewJSArrayWithElements(details);
  }

  LookupIterator it(obj, name, LookupIterator::HIDDEN);
  bool has_caught = false;
  Handle<Object> value = DebugGetProperty(&it, &has_caught);
  if (!it.IsFound()) return isolate->heap()->undefined_value();

  Handle<Object> maybe_pair;
  if (it.state() == LookupIterator::ACCESSOR) {
    maybe_pair = it.GetAccessors();
  }

  // If the callback object is a fixed array then it contains JavaScript
  // getter and/or setter.
  bool has_js_accessors = !maybe_pair.is_null() && maybe_pair->IsAccessorPair();
  Handle<FixedArray> details =
      isolate->factory()->NewFixedArray(has_js_accessors ? 6 : 3);
  details->set(0, *value);
  // TODO(verwaest): Get rid of this random way of handling interceptors.
  PropertyDetails d = it.state() == LookupIterator::INTERCEPTOR
                          ? PropertyDetails::Empty()
                          : it.property_details();
  details->set(1, d.AsSmi());
  details->set(
      2, isolate->heap()->ToBoolean(it.state() == LookupIterator::INTERCEPTOR));
  if (has_js_accessors) {
    AccessorPair* accessors = AccessorPair::cast(*maybe_pair);
    details->set(3, isolate->heap()->ToBoolean(has_caught));
    details->set(4, accessors->GetComponent(ACCESSOR_GETTER));
    details->set(5, accessors->GetComponent(ACCESSOR_SETTER));
  }

  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_DebugGetProperty) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  LookupIterator it(obj, name);
  return *DebugGetProperty(&it);
}


// Return the property type calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyTypeFromDetails) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.type()));
}


// Return the property attribute calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyAttributesFromDetails) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.attributes()));
}


// Return the property insertion index calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyIndexFromDetails) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  // TODO(verwaest): Works only for dictionary mode holders.
  return Smi::FromInt(details.dictionary_index());
}


// Return property value from named interceptor.
// args[0]: object
// args[1]: property name
RUNTIME_FUNCTION(Runtime_DebugNamedInterceptorPropertyValue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasNamedInterceptor());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::GetProperty(obj, name));
  return *result;
}


// Return element value from indexed interceptor.
// args[0]: object
// args[1]: index
RUNTIME_FUNCTION(Runtime_DebugIndexedInterceptorElementValue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasIndexedInterceptor());
  CONVERT_NUMBER_CHECKED(uint32_t, index, Uint32, args[1]);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetElementWithInterceptor(obj, obj, index, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CheckExecutionState) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));
  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(Runtime_GetFrameCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  // Count all frames which are relevant to debugging stack trace.
  int n = 0;
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack frame count is 0.
    return Smi::FromInt(0);
  }

  for (JavaScriptFrameIterator it(isolate, id); !it.done(); it.Advance()) {
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    it.frame()->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0; i--) {
      // Omit functions from native scripts.
      if (!frames[i].function()->IsFromNativeScript()) n++;
    }
  }
  return Smi::FromInt(n);
}


class FrameInspector {
 public:
  FrameInspector(JavaScriptFrame* frame, int inlined_jsframe_index,
                 Isolate* isolate)
      : frame_(frame), deoptimized_frame_(NULL), isolate_(isolate) {
    has_adapted_arguments_ = frame_->has_adapted_arguments();
    is_bottommost_ = inlined_jsframe_index == 0;
    is_optimized_ = frame_->is_optimized();
    // Calculate the deoptimized frame.
    if (frame->is_optimized()) {
      // TODO(turbofan): Revisit once we support deoptimization.
      if (frame->LookupCode()->is_turbofanned() && !FLAG_turbo_deoptimization) {
        is_optimized_ = false;
        return;
      }

      deoptimized_frame_ = Deoptimizer::DebuggerInspectableFrame(
          frame, inlined_jsframe_index, isolate);
    }
  }

  ~FrameInspector() {
    // Get rid of the calculated deoptimized frame if any.
    if (deoptimized_frame_ != NULL) {
      Deoptimizer::DeleteDebuggerInspectableFrame(deoptimized_frame_, isolate_);
    }
  }

  int GetParametersCount() {
    return is_optimized_ ? deoptimized_frame_->parameters_count()
                         : frame_->ComputeParametersCount();
  }
  int expression_count() { return deoptimized_frame_->expression_count(); }
  Object* GetFunction() {
    return is_optimized_ ? deoptimized_frame_->GetFunction()
                         : frame_->function();
  }
  Object* GetParameter(int index) {
    return is_optimized_ ? deoptimized_frame_->GetParameter(index)
                         : frame_->GetParameter(index);
  }
  Object* GetExpression(int index) {
    // TODO(turbofan): Revisit once we support deoptimization.
    if (frame_->LookupCode()->is_turbofanned() && !FLAG_turbo_deoptimization) {
      return isolate_->heap()->undefined_value();
    }
    return is_optimized_ ? deoptimized_frame_->GetExpression(index)
                         : frame_->GetExpression(index);
  }
  int GetSourcePosition() {
    return is_optimized_ ? deoptimized_frame_->GetSourcePosition()
                         : frame_->LookupCode()->SourcePosition(frame_->pc());
  }
  bool IsConstructor() {
    return is_optimized_ && !is_bottommost_
               ? deoptimized_frame_->HasConstructStub()
               : frame_->IsConstructor();
  }
  Object* GetContext() {
    return is_optimized_ ? deoptimized_frame_->GetContext() : frame_->context();
  }

  // To inspect all the provided arguments the frame might need to be
  // replaced with the arguments frame.
  void SetArgumentsFrame(JavaScriptFrame* frame) {
    DCHECK(has_adapted_arguments_);
    frame_ = frame;
    is_optimized_ = frame_->is_optimized();
    DCHECK(!is_optimized_);
  }

 private:
  JavaScriptFrame* frame_;
  DeoptimizedFrameInfo* deoptimized_frame_;
  Isolate* isolate_;
  bool is_optimized_;
  bool is_bottommost_;
  bool has_adapted_arguments_;

  DISALLOW_COPY_AND_ASSIGN(FrameInspector);
};


static const int kFrameDetailsFrameIdIndex = 0;
static const int kFrameDetailsReceiverIndex = 1;
static const int kFrameDetailsFunctionIndex = 2;
static const int kFrameDetailsArgumentCountIndex = 3;
static const int kFrameDetailsLocalCountIndex = 4;
static const int kFrameDetailsSourcePositionIndex = 5;
static const int kFrameDetailsConstructCallIndex = 6;
static const int kFrameDetailsAtReturnIndex = 7;
static const int kFrameDetailsFlagsIndex = 8;
static const int kFrameDetailsFirstDynamicIndex = 9;


static SaveContext* FindSavedContextForFrame(Isolate* isolate,
                                             JavaScriptFrame* frame) {
  SaveContext* save = isolate->save_context();
  while (save != NULL && !save->IsBelowFrame(frame)) {
    save = save->prev();
  }
  DCHECK(save != NULL);
  return save;
}


// Advances the iterator to the frame that matches the index and returns the
// inlined frame index, or -1 if not found.  Skips native JS functions.
int Runtime::FindIndexedNonNativeFrame(JavaScriptFrameIterator* it, int index) {
  int count = -1;
  for (; !it->done(); it->Advance()) {
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    it->frame()->Summarize(&frames);
    for (int i = frames.length() - 1; i >= 0; i--) {
      // Omit functions from native scripts.
      if (frames[i].function()->IsFromNativeScript()) continue;
      if (++count == index) return i;
    }
  }
  return -1;
}


// Return an array with frame details
// args[0]: number: break id
// args[1]: number: frame index
//
// The array returned contains the following information:
// 0: Frame id
// 1: Receiver
// 2: Function
// 3: Argument count
// 4: Local count
// 5: Source position
// 6: Constructor call
// 7: Is at return
// 8: Flags
// Arguments name, value
// Locals name, value
// Return value if any
RUNTIME_FUNCTION(Runtime_GetFrameDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  Heap* heap = isolate->heap();

  // Find the relevant frame with the requested index.
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return heap->undefined_value();
  }

  JavaScriptFrameIterator it(isolate, id);
  // Inlined frame index in optimized frame, starting from outer function.
  int inlined_jsframe_index = Runtime::FindIndexedNonNativeFrame(&it, index);
  if (inlined_jsframe_index == -1) return heap->undefined_value();

  FrameInspector frame_inspector(it.frame(), inlined_jsframe_index, isolate);
  bool is_optimized = it.frame()->is_optimized();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = FindSavedContextForFrame(isolate, it.frame());

  // Get the frame id.
  Handle<Object> frame_id(WrapFrameId(it.frame()->id()), isolate);

  // Find source position in unoptimized code.
  int position = frame_inspector.GetSourcePosition();

  // Check for constructor frame.
  bool constructor = frame_inspector.IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  DCHECK(*scope_info != ScopeInfo::Empty(isolate));

  // Get the locals names and values into a temporary array.
  int local_count = scope_info->LocalCount();
  for (int slot = 0; slot < scope_info->LocalCount(); ++slot) {
    // Hide compiler-introduced temporary variables, whether on the stack or on
    // the context.
    if (scope_info->LocalIsSynthetic(slot)) local_count--;
  }

  Handle<FixedArray> locals =
      isolate->factory()->NewFixedArray(local_count * 2);

  // Fill in the values of the locals.
  int local = 0;
  int i = 0;
  for (; i < scope_info->StackLocalCount(); ++i) {
    // Use the value from the stack.
    if (scope_info->LocalIsSynthetic(i)) continue;
    locals->set(local * 2, scope_info->LocalName(i));
    locals->set(local * 2 + 1, frame_inspector.GetExpression(i));
    local++;
  }
  if (local < local_count) {
    // Get the context containing declarations.
    Handle<Context> context(
        Context::cast(frame_inspector.GetContext())->declaration_context());
    for (; i < scope_info->LocalCount(); ++i) {
      if (scope_info->LocalIsSynthetic(i)) continue;
      Handle<String> name(scope_info->LocalName(i));
      VariableMode mode;
      InitializationFlag init_flag;
      MaybeAssignedFlag maybe_assigned_flag;
      locals->set(local * 2, *name);
      int context_slot_index = ScopeInfo::ContextSlotIndex(
          scope_info, name, &mode, &init_flag, &maybe_assigned_flag);
      Object* value = context->get(context_slot_index);
      locals->set(local * 2 + 1, value);
      local++;
    }
  }

  // Check whether this frame is positioned at return. If not top
  // frame or if the frame is optimized it cannot be at a return.
  bool at_return = false;
  if (!is_optimized && index == 0) {
    at_return = isolate->debug()->IsBreakAtReturn(it.frame());
  }

  // If positioned just before return find the value to be returned and add it
  // to the frame information.
  Handle<Object> return_value = isolate->factory()->undefined_value();
  if (at_return) {
    StackFrameIterator it2(isolate);
    Address internal_frame_sp = NULL;
    while (!it2.done()) {
      if (it2.frame()->is_internal()) {
        internal_frame_sp = it2.frame()->sp();
      } else {
        if (it2.frame()->is_java_script()) {
          if (it2.frame()->id() == it.frame()->id()) {
            // The internal frame just before the JavaScript frame contains the
            // value to return on top. A debug break at return will create an
            // internal frame to store the return value (eax/rax/r0) before
            // entering the debug break exit frame.
            if (internal_frame_sp != NULL) {
              return_value =
                  Handle<Object>(Memory::Object_at(internal_frame_sp), isolate);
              break;
            }
          }
        }

        // Indicate that the previous frame was not an internal frame.
        internal_frame_sp = NULL;
      }
      it2.Advance();
    }
  }

  // Now advance to the arguments adapter frame (if any). It contains all
  // the provided parameters whereas the function frame always have the number
  // of arguments matching the functions parameters. The rest of the
  // information (except for what is collected above) is the same.
  if ((inlined_jsframe_index == 0) && it.frame()->has_adapted_arguments()) {
    it.AdvanceToArgumentsFrame();
    frame_inspector.SetArgumentsFrame(it.frame());
  }

  // Find the number of arguments to fill. At least fill the number of
  // parameters for the function and fill more if more parameters are provided.
  int argument_count = scope_info->ParameterCount();
  if (argument_count < frame_inspector.GetParametersCount()) {
    argument_count = frame_inspector.GetParametersCount();
  }

  // Calculate the size of the result.
  int details_size = kFrameDetailsFirstDynamicIndex +
                     2 * (argument_count + local_count) + (at_return ? 1 : 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Add the frame id.
  details->set(kFrameDetailsFrameIdIndex, *frame_id);

  // Add the function (same as in function frame).
  details->set(kFrameDetailsFunctionIndex, frame_inspector.GetFunction());

  // Add the arguments count.
  details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(argument_count));

  // Add the locals count
  details->set(kFrameDetailsLocalCountIndex, Smi::FromInt(local_count));

  // Add the source position.
  if (position != RelocInfo::kNoPosition) {
    details->set(kFrameDetailsSourcePositionIndex, Smi::FromInt(position));
  } else {
    details->set(kFrameDetailsSourcePositionIndex, heap->undefined_value());
  }

  // Add the constructor information.
  details->set(kFrameDetailsConstructCallIndex, heap->ToBoolean(constructor));

  // Add the at return information.
  details->set(kFrameDetailsAtReturnIndex, heap->ToBoolean(at_return));

  // Add flags to indicate information on whether this frame is
  //   bit 0: invoked in the debugger context.
  //   bit 1: optimized frame.
  //   bit 2: inlined in optimized frame
  int flags = 0;
  if (*save->context() == *isolate->debug()->debug_context()) {
    flags |= 1 << 0;
  }
  if (is_optimized) {
    flags |= 1 << 1;
    flags |= inlined_jsframe_index << 2;
  }
  details->set(kFrameDetailsFlagsIndex, Smi::FromInt(flags));

  // Fill the dynamic part.
  int details_index = kFrameDetailsFirstDynamicIndex;

  // Add arguments name and value.
  for (int i = 0; i < argument_count; i++) {
    // Name of the argument.
    if (i < scope_info->ParameterCount()) {
      details->set(details_index++, scope_info->ParameterName(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }

    // Parameter value.
    if (i < frame_inspector.GetParametersCount()) {
      // Get the value from the stack.
      details->set(details_index++, frame_inspector.GetParameter(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }
  }

  // Add locals name and value from the temporary copy from the function frame.
  for (int i = 0; i < local_count * 2; i++) {
    details->set(details_index++, locals->get(i));
  }

  // Add the value being returned.
  if (at_return) {
    details->set(details_index++, *return_value);
  }

  // Add the receiver (same as in function frame).
  // THIS MUST BE DONE LAST SINCE WE MIGHT ADVANCE
  // THE FRAME ITERATOR TO WRAP THE RECEIVER.
  Handle<Object> receiver(it.frame()->receiver(), isolate);
  if (!receiver->IsJSObject() && is_sloppy(shared->language_mode()) &&
      !function->IsBuiltin()) {
    // If the receiver is not a JSObject and the function is not a
    // builtin or strict-mode we have hit an optimization where a
    // value object is not converted into a wrapped JS objects. To
    // hide this optimization from the debugger, we wrap the receiver
    // by creating correct wrapper object based on the calling frame's
    // native context.
    it.Advance();
    if (receiver->IsUndefined()) {
      receiver = handle(function->global_proxy());
    } else {
      Context* context = Context::cast(it.frame()->context());
      Handle<Context> native_context(Context::cast(context->native_context()));
      if (!Object::ToObject(isolate, receiver, native_context)
               .ToHandle(&receiver)) {
        // This only happens if the receiver is forcibly set in %_CallFunction.
        return heap->undefined_value();
      }
    }
  }
  details->set(kFrameDetailsReceiverIndex, *receiver);

  DCHECK_EQ(details_size, details_index);
  return *isolate->factory()->NewJSArrayWithElements(details);
}


static bool ParameterIsShadowedByContextLocal(Handle<ScopeInfo> info,
                                              Handle<String> parameter_name) {
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  return ScopeInfo::ContextSlotIndex(info, parameter_name, &mode, &init_flag,
                                     &maybe_assigned_flag) != -1;
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
MUST_USE_RESULT
static MaybeHandle<JSObject> MaterializeStackLocalsWithFrameInspector(
    Isolate* isolate, Handle<JSObject> target, Handle<ScopeInfo> scope_info,
    FrameInspector* frame_inspector) {
  // First fill all parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Do not materialize the parameter if it is shadowed by a context local.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    DCHECK_NOT_NULL(frame_inspector);

    HandleScope scope(isolate);
    Handle<Object> value(i < frame_inspector->GetParametersCount()
                             ? frame_inspector->GetParameter(i)
                             : isolate->heap()->undefined_value(),
                         isolate);
    DCHECK(!value->IsTheHole());

    RETURN_ON_EXCEPTION(isolate, Runtime::SetObjectProperty(
                                     isolate, target, name, value, SLOPPY),
                        JSObject);
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    Handle<String> name(scope_info->StackLocalName(i));
    Handle<Object> value(
        frame_inspector->GetExpression(scope_info->StackLocalIndex(i)),
        isolate);
    if (value->IsTheHole()) {
      value = isolate->factory()->undefined_value();
    }

    RETURN_ON_EXCEPTION(isolate, Runtime::SetObjectProperty(
                                     isolate, target, name, value, SLOPPY),
                        JSObject);
  }

  return target;
}

MUST_USE_RESULT
static MaybeHandle<JSObject> MaterializeStackLocalsWithFrameInspector(
    Isolate* isolate, Handle<JSObject> target, Handle<JSFunction> function,
    FrameInspector* frame_inspector) {
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  return MaterializeStackLocalsWithFrameInspector(isolate, target, scope_info,
                                                  frame_inspector);
}


static void UpdateStackLocalsFromMaterializedObject(
    Isolate* isolate, Handle<JSObject> target, Handle<ScopeInfo> scope_info,
    JavaScriptFrame* frame, int inlined_jsframe_index) {
  if (inlined_jsframe_index != 0 || frame->is_optimized()) {
    // Optimized frames are not supported.
    // TODO(yangguo): make sure all code deoptimized when debugger is active
    //                and assert that this cannot happen.
    return;
  }

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Shadowed parameters were not materialized.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    DCHECK(!frame->GetParameter(i)->IsTheHole());
    HandleScope scope(isolate);
    Handle<Object> value =
        Object::GetPropertyOrElement(target, name).ToHandleChecked();
    frame->SetParameterValue(i, *value);
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    int index = scope_info->StackLocalIndex(i);
    if (frame->GetExpression(index)->IsTheHole()) continue;
    HandleScope scope(isolate);
    Handle<Object> value = Object::GetPropertyOrElement(
                               target, handle(scope_info->StackLocalName(i),
                                              isolate)).ToHandleChecked();
    frame->SetExpression(index, *value);
  }
}


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeLocalContext(
    Isolate* isolate, Handle<JSObject> target, Handle<JSFunction> function,
    JavaScriptFrame* frame) {
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  if (!scope_info->HasContext()) return target;

  // Third fill all context locals.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->declaration_context());
  if (!ScopeInfo::CopyContextLocalsToScopeObject(scope_info, function_context,
                                                 target)) {
    return MaybeHandle<JSObject>();
  }

  // Finally copy any properties from the function context extension.
  // These will be variables introduced by eval.
  if (function_context->closure() == *function) {
    if (function_context->has_extension() &&
        !function_context->IsNativeContext()) {
      Handle<JSObject> ext(JSObject::cast(function_context->extension()));
      Handle<FixedArray> keys;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, keys, JSReceiver::GetKeys(ext, JSReceiver::INCLUDE_PROTOS),
          JSObject);

      for (int i = 0; i < keys->length(); i++) {
        // Names of variables introduced by eval are strings.
        DCHECK(keys->get(i)->IsString());
        Handle<String> key(String::cast(keys->get(i)));
        Handle<Object> value;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, Object::GetPropertyOrElement(ext, key), JSObject);
        RETURN_ON_EXCEPTION(isolate, Runtime::SetObjectProperty(
                                         isolate, target, key, value, SLOPPY),
                            JSObject);
      }
    }
  }

  return target;
}


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeScriptScope(
    Handle<GlobalObject> global) {
  Isolate* isolate = global->GetIsolate();
  Handle<ScriptContextTable> script_contexts(
      global->native_context()->script_context_table());

  Handle<JSObject> script_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  for (int context_index = 0; context_index < script_contexts->used();
       context_index++) {
    Handle<Context> context =
        ScriptContextTable::GetContext(script_contexts, context_index);
    Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));
    if (!ScopeInfo::CopyContextLocalsToScopeObject(scope_info, context,
                                                   script_scope)) {
      return MaybeHandle<JSObject>();
    }
  }
  return script_scope;
}


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeLocalScope(
    Isolate* isolate, JavaScriptFrame* frame, int inlined_jsframe_index) {
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));

  Handle<JSObject> local_scope =
      isolate->factory()->NewJSObject(isolate->object_function());
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, local_scope,
      MaterializeStackLocalsWithFrameInspector(isolate, local_scope, function,
                                               &frame_inspector),
      JSObject);

  return MaterializeLocalContext(isolate, local_scope, function, frame);
}


// Set the context local variable value.
static bool SetContextLocalValue(Isolate* isolate, Handle<ScopeInfo> scope_info,
                                 Handle<Context> context,
                                 Handle<String> variable_name,
                                 Handle<Object> new_value) {
  for (int i = 0; i < scope_info->ContextLocalCount(); i++) {
    Handle<String> next_name(scope_info->ContextLocalName(i));
    if (String::Equals(variable_name, next_name)) {
      VariableMode mode;
      InitializationFlag init_flag;
      MaybeAssignedFlag maybe_assigned_flag;
      int context_index = ScopeInfo::ContextSlotIndex(
          scope_info, next_name, &mode, &init_flag, &maybe_assigned_flag);
      context->set(context_index, *new_value);
      return true;
    }
  }

  return false;
}


static bool SetLocalVariableValue(Isolate* isolate, JavaScriptFrame* frame,
                                  int inlined_jsframe_index,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  if (inlined_jsframe_index != 0 || frame->is_optimized()) {
    // Optimized frames are not supported.
    return false;
  }

  Handle<JSFunction> function(frame->function());
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  bool default_result = false;

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    HandleScope scope(isolate);
    if (String::Equals(handle(scope_info->ParameterName(i)), variable_name)) {
      frame->SetParameterValue(i, *new_value);
      // Argument might be shadowed in heap context, don't stop here.
      default_result = true;
    }
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    HandleScope scope(isolate);
    if (String::Equals(handle(scope_info->StackLocalName(i)), variable_name)) {
      frame->SetExpression(scope_info->StackLocalIndex(i), *new_value);
      return true;
    }
  }

  if (scope_info->HasContext()) {
    // Context locals.
    Handle<Context> frame_context(Context::cast(frame->context()));
    Handle<Context> function_context(frame_context->declaration_context());
    if (SetContextLocalValue(isolate, scope_info, function_context,
                             variable_name, new_value)) {
      return true;
    }

    // Function context extension. These are variables introduced by eval.
    if (function_context->closure() == *function) {
      if (function_context->has_extension() &&
          !function_context->IsNativeContext()) {
        Handle<JSObject> ext(JSObject::cast(function_context->extension()));

        Maybe<bool> maybe = JSReceiver::HasProperty(ext, variable_name);
        DCHECK(maybe.IsJust());
        if (maybe.FromJust()) {
          // We don't expect this to do anything except replacing
          // property value.
          Runtime::SetObjectProperty(isolate, ext, variable_name, new_value,
                                     SLOPPY).Assert();
          return true;
        }
      }
    }
  }

  return default_result;
}


static bool SetBlockVariableValue(Isolate* isolate,
                                  Handle<Context> block_context,
                                  Handle<ScopeInfo> scope_info,
                                  JavaScriptFrame* frame,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  if (frame != nullptr) {
    for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
      HandleScope scope(isolate);
      if (String::Equals(handle(scope_info->StackLocalName(i)),
                         variable_name)) {
        frame->SetExpression(scope_info->StackLocalIndex(i), *new_value);
        return true;
      }
    }
  }
  if (!block_context.is_null()) {
    return SetContextLocalValue(block_context->GetIsolate(), scope_info,
                                block_context, variable_name, new_value);
  }
  return false;
}


// Create a plain JSObject which materializes the closure content for the
// context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeClosure(
    Isolate* isolate, Handle<Context> context) {
  DCHECK(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Allocate and initialize a JSObject with all the content of this function
  // closure.
  Handle<JSObject> closure_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals to the context extension.
  if (!ScopeInfo::CopyContextLocalsToScopeObject(scope_info, context,
                                                 closure_scope)) {
    return MaybeHandle<JSObject>();
  }

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    Handle<FixedArray> keys;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, keys, JSReceiver::GetKeys(ext, JSReceiver::INCLUDE_PROTOS),
        JSObject);

    for (int i = 0; i < keys->length(); i++) {
      HandleScope scope(isolate);
      // Names of variables introduced by eval are strings.
      DCHECK(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
      Handle<Object> value;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, Object::GetPropertyOrElement(ext, key), JSObject);
      RETURN_ON_EXCEPTION(isolate, Runtime::DefineObjectProperty(
                                       closure_scope, key, value, NONE),
                          JSObject);
    }
  }

  return closure_scope;
}


// This method copies structure of MaterializeClosure method above.
static bool SetClosureVariableValue(Isolate* isolate, Handle<Context> context,
                                    Handle<String> variable_name,
                                    Handle<Object> new_value) {
  DCHECK(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Context locals to the context extension.
  if (SetContextLocalValue(isolate, scope_info, context, variable_name,
                           new_value)) {
    return true;
  }

  // Properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    Maybe<bool> maybe = JSReceiver::HasProperty(ext, variable_name);
    DCHECK(maybe.IsJust());
    if (maybe.FromJust()) {
      // We don't expect this to do anything except replacing property value.
      Runtime::DefineObjectProperty(ext, variable_name, new_value, NONE)
          .Assert();
      return true;
    }
  }

  return false;
}


static bool SetScriptVariableValue(Handle<Context> context,
                                   Handle<String> variable_name,
                                   Handle<Object> new_value) {
  Handle<ScriptContextTable> script_contexts(
      context->global_object()->native_context()->script_context_table());
  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(script_contexts, variable_name,
                                 &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        script_contexts, lookup_result.context_index);
    script_context->set(lookup_result.slot_index, *new_value);
    return true;
  }

  return false;
}


// Create a plain JSObject which materializes the scope for the specified
// catch context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeCatchScope(
    Isolate* isolate, Handle<Context> context) {
  DCHECK(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  Handle<Object> thrown_object(context->get(Context::THROWN_OBJECT_INDEX),
                               isolate);
  Handle<JSObject> catch_scope =
      isolate->factory()->NewJSObject(isolate->object_function());
  RETURN_ON_EXCEPTION(isolate, Runtime::DefineObjectProperty(
                                   catch_scope, name, thrown_object, NONE),
                      JSObject);
  return catch_scope;
}


static bool SetCatchVariableValue(Isolate* isolate, Handle<Context> context,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  DCHECK(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  if (!String::Equals(name, variable_name)) {
    return false;
  }
  context->set(Context::THROWN_OBJECT_INDEX, *new_value);
  return true;
}


// Create a plain JSObject which materializes the block scope for the specified
// block context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeBlockScope(
    Isolate* isolate, Handle<ScopeInfo> scope_info, Handle<Context> context,
    JavaScriptFrame* frame, int inlined_jsframe_index) {
  Handle<JSObject> block_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  if (frame != nullptr) {
    FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
    RETURN_ON_EXCEPTION(isolate,
                        MaterializeStackLocalsWithFrameInspector(
                            isolate, block_scope, scope_info, &frame_inspector),
                        JSObject);
  }

  if (!context.is_null()) {
    Handle<ScopeInfo> scope_info_from_context(
        ScopeInfo::cast(context->extension()));
    // Fill all context locals.
    if (!ScopeInfo::CopyContextLocalsToScopeObject(scope_info_from_context,
                                                   context, block_scope)) {
      return MaybeHandle<JSObject>();
    }
  }

  return block_scope;
}


// Create a plain JSObject which materializes the module scope for the specified
// module context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeModuleScope(
    Isolate* isolate, Handle<Context> context) {
  DCHECK(context->IsModuleContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the members of the debugged
  // module.
  Handle<JSObject> module_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals.
  if (!ScopeInfo::CopyContextLocalsToScopeObject(scope_info, context,
                                                 module_scope)) {
    return MaybeHandle<JSObject>();
  }

  return module_scope;
}


// Iterate over the actual scopes visible from a stack frame or from a closure.
// The iteration proceeds from the innermost visible nested scope outwards.
// All scopes are backed by an actual context except the local scope,
// which is inserted "artificially" in the context chain.
class ScopeIterator {
 public:
  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    ScopeTypeCatch,
    ScopeTypeBlock,
    ScopeTypeScript,
    ScopeTypeModule
  };

  ScopeIterator(Isolate* isolate, JavaScriptFrame* frame,
                int inlined_jsframe_index, bool ignore_nested_scopes = false)
      : isolate_(isolate),
        frame_(frame),
        inlined_jsframe_index_(inlined_jsframe_index),
        function_(frame->function()),
        context_(Context::cast(frame->context())),
        nested_scope_chain_(4),
        seen_script_scope_(false),
        failed_(false) {
    // Catch the case when the debugger stops in an internal function.
    Handle<SharedFunctionInfo> shared_info(function_->shared());
    Handle<ScopeInfo> scope_info(shared_info->scope_info());
    if (shared_info->script() == isolate->heap()->undefined_value()) {
      while (context_->closure() == *function_) {
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
      return;
    }

    // Get the debug info (create it if it does not exist).
    if (!isolate->debug()->EnsureDebugInfo(shared_info, function_)) {
      // Return if ensuring debug info failed.
      return;
    }

    // Currently it takes too much time to find nested scopes due to script
    // parsing. Sometimes we want to run the ScopeIterator as fast as possible
    // (for example, while collecting async call stacks on every
    // addEventListener call), even if we drop some nested scopes.
    // Later we may optimize getting the nested scopes (cache the result?)
    // and include nested scopes into the "fast" iteration case as well.
    if (!ignore_nested_scopes) {
      Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared_info);

      // PC points to the instruction after the current one, possibly a break
      // location as well. So the "- 1" to exclude it from the search.
      Address call_pc = frame->pc() - 1;

      // Find the break point where execution has stopped.
      BreakLocation location =
          BreakLocation::FromAddress(debug_info, ALL_BREAK_LOCATIONS, call_pc);

      // Within the return sequence at the moment it is not possible to
      // get a source position which is consistent with the current scope chain.
      // Thus all nested with, catch and block contexts are skipped and we only
      // provide the function scope.
      ignore_nested_scopes = location.IsExit();
    }

    if (ignore_nested_scopes) {
      if (scope_info->HasContext()) {
        context_ = Handle<Context>(context_->declaration_context(), isolate_);
      } else {
        while (context_->closure() == *function_) {
          context_ = Handle<Context>(context_->previous(), isolate_);
        }
      }
      if (scope_info->scope_type() == FUNCTION_SCOPE ||
          scope_info->scope_type() == ARROW_SCOPE) {
        nested_scope_chain_.Add(scope_info);
      }
    } else {
      // Reparse the code and analyze the scopes.
      Handle<Script> script(Script::cast(shared_info->script()));
      Scope* scope = NULL;

      // Check whether we are in global, eval or function code.
      Handle<ScopeInfo> scope_info(shared_info->scope_info());
      Zone zone;
      if (scope_info->scope_type() != FUNCTION_SCOPE &&
          scope_info->scope_type() != ARROW_SCOPE) {
        // Global or eval code.
        ParseInfo info(&zone, script);
        if (scope_info->scope_type() == SCRIPT_SCOPE) {
          info.set_global();
        } else {
          DCHECK(scope_info->scope_type() == EVAL_SCOPE);
          info.set_eval();
          info.set_context(Handle<Context>(function_->context()));
        }
        if (Parser::ParseStatic(&info) && Scope::Analyze(&info)) {
          scope = info.function()->scope();
        }
        RetrieveScopeChain(scope, shared_info);
      } else {
        // Function code
        ParseInfo info(&zone, function_);
        if (Parser::ParseStatic(&info) && Scope::Analyze(&info)) {
          scope = info.function()->scope();
        }
        RetrieveScopeChain(scope, shared_info);
      }
    }
  }

  ScopeIterator(Isolate* isolate, Handle<JSFunction> function)
      : isolate_(isolate),
        frame_(NULL),
        inlined_jsframe_index_(0),
        function_(function),
        context_(function->context()),
        seen_script_scope_(false),
        failed_(false) {
    if (function->IsBuiltin()) {
      context_ = Handle<Context>();
    }
  }

  // More scopes?
  bool Done() {
    DCHECK(!failed_);
    return context_.is_null();
  }

  bool Failed() { return failed_; }

  // Move to the next scope.
  void Next() {
    DCHECK(!failed_);
    ScopeType scope_type = Type();
    if (scope_type == ScopeTypeGlobal) {
      // The global scope is always the last in the chain.
      DCHECK(context_->IsNativeContext());
      context_ = Handle<Context>();
      return;
    }
    if (scope_type == ScopeTypeScript) seen_script_scope_ = true;
    if (nested_scope_chain_.is_empty()) {
      if (scope_type == ScopeTypeScript) {
        if (context_->IsScriptContext()) {
          context_ = Handle<Context>(context_->previous(), isolate_);
        }
        CHECK(context_->IsNativeContext());
      } else {
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
    } else {
      if (nested_scope_chain_.last()->HasContext()) {
        DCHECK(context_->previous() != NULL);
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
      nested_scope_chain_.RemoveLast();
    }
  }

  // Return the type of the current scope.
  ScopeType Type() {
    DCHECK(!failed_);
    if (!nested_scope_chain_.is_empty()) {
      Handle<ScopeInfo> scope_info = nested_scope_chain_.last();
      switch (scope_info->scope_type()) {
        case FUNCTION_SCOPE:
        case ARROW_SCOPE:
          DCHECK(context_->IsFunctionContext() || !scope_info->HasContext());
          return ScopeTypeLocal;
        case MODULE_SCOPE:
          DCHECK(context_->IsModuleContext());
          return ScopeTypeModule;
        case SCRIPT_SCOPE:
          DCHECK(context_->IsScriptContext() || context_->IsNativeContext());
          return ScopeTypeScript;
        case WITH_SCOPE:
          DCHECK(context_->IsWithContext());
          return ScopeTypeWith;
        case CATCH_SCOPE:
          DCHECK(context_->IsCatchContext());
          return ScopeTypeCatch;
        case BLOCK_SCOPE:
          DCHECK(!scope_info->HasContext() || context_->IsBlockContext());
          return ScopeTypeBlock;
        case EVAL_SCOPE:
          UNREACHABLE();
      }
    }
    if (context_->IsNativeContext()) {
      DCHECK(context_->global_object()->IsGlobalObject());
      // If we are at the native context and have not yet seen script scope,
      // fake it.
      return seen_script_scope_ ? ScopeTypeGlobal : ScopeTypeScript;
    }
    if (context_->IsFunctionContext()) {
      return ScopeTypeClosure;
    }
    if (context_->IsCatchContext()) {
      return ScopeTypeCatch;
    }
    if (context_->IsBlockContext()) {
      return ScopeTypeBlock;
    }
    if (context_->IsModuleContext()) {
      return ScopeTypeModule;
    }
    if (context_->IsScriptContext()) {
      return ScopeTypeScript;
    }
    DCHECK(context_->IsWithContext());
    return ScopeTypeWith;
  }

  // Return the JavaScript object with the content of the current scope.
  MaybeHandle<JSObject> ScopeObject() {
    DCHECK(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        return Handle<JSObject>(CurrentContext()->global_object());
      case ScopeIterator::ScopeTypeScript:
        return MaterializeScriptScope(
            Handle<GlobalObject>(CurrentContext()->global_object()));
      case ScopeIterator::ScopeTypeLocal:
        // Materialize the content of the local scope into a JSObject.
        DCHECK(nested_scope_chain_.length() == 1);
        return MaterializeLocalScope(isolate_, frame_, inlined_jsframe_index_);
      case ScopeIterator::ScopeTypeWith:
        // Return the with object.
        return Handle<JSObject>(JSObject::cast(CurrentContext()->extension()));
      case ScopeIterator::ScopeTypeCatch:
        return MaterializeCatchScope(isolate_, CurrentContext());
      case ScopeIterator::ScopeTypeClosure:
        // Materialize the content of the closure scope into a JSObject.
        return MaterializeClosure(isolate_, CurrentContext());
      case ScopeIterator::ScopeTypeBlock: {
        if (!nested_scope_chain_.is_empty()) {
          // this is a block scope on the stack.
          Handle<ScopeInfo> scope_info = nested_scope_chain_.last();
          Handle<Context> context = scope_info->HasContext()
                                        ? CurrentContext()
                                        : Handle<Context>::null();
          return MaterializeBlockScope(isolate_, scope_info, context, frame_,
                                       inlined_jsframe_index_);
        } else {
          return MaterializeBlockScope(isolate_, Handle<ScopeInfo>::null(),
                                       CurrentContext(), nullptr, 0);
        }
      }
      case ScopeIterator::ScopeTypeModule:
        return MaterializeModuleScope(isolate_, CurrentContext());
    }
    UNREACHABLE();
    return Handle<JSObject>();
  }

  bool HasContext() {
    ScopeType type = Type();
    if (type == ScopeTypeBlock || type == ScopeTypeLocal) {
      if (!nested_scope_chain_.is_empty()) {
        return nested_scope_chain_.last()->HasContext();
      }
    }
    return true;
  }

  bool SetVariableValue(Handle<String> variable_name,
                        Handle<Object> new_value) {
    DCHECK(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        break;
      case ScopeIterator::ScopeTypeLocal:
        return SetLocalVariableValue(isolate_, frame_, inlined_jsframe_index_,
                                     variable_name, new_value);
      case ScopeIterator::ScopeTypeWith:
        break;
      case ScopeIterator::ScopeTypeCatch:
        return SetCatchVariableValue(isolate_, CurrentContext(), variable_name,
                                     new_value);
      case ScopeIterator::ScopeTypeClosure:
        return SetClosureVariableValue(isolate_, CurrentContext(),
                                       variable_name, new_value);
      case ScopeIterator::ScopeTypeScript:
        return SetScriptVariableValue(CurrentContext(), variable_name,
                                      new_value);
      case ScopeIterator::ScopeTypeBlock:
        return SetBlockVariableValue(
            isolate_, HasContext() ? CurrentContext() : Handle<Context>::null(),
            CurrentScopeInfo(), frame_, variable_name, new_value);
      case ScopeIterator::ScopeTypeModule:
        // TODO(2399): should we implement it?
        break;
    }
    return false;
  }

  Handle<ScopeInfo> CurrentScopeInfo() {
    DCHECK(!failed_);
    if (!nested_scope_chain_.is_empty()) {
      return nested_scope_chain_.last();
    } else if (context_->IsBlockContext()) {
      return Handle<ScopeInfo>(ScopeInfo::cast(context_->extension()));
    } else if (context_->IsFunctionContext()) {
      return Handle<ScopeInfo>(context_->closure()->shared()->scope_info());
    }
    return Handle<ScopeInfo>::null();
  }

  // Return the context for this scope. For the local context there might not
  // be an actual context.
  Handle<Context> CurrentContext() {
    DCHECK(!failed_);
    if (Type() == ScopeTypeGlobal || Type() == ScopeTypeScript ||
        nested_scope_chain_.is_empty()) {
      return context_;
    } else if (nested_scope_chain_.last()->HasContext()) {
      return context_;
    } else {
      return Handle<Context>();
    }
  }

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint() {
    OFStream os(stdout);
    DCHECK(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        os << "Global:\n";
        CurrentContext()->Print(os);
        break;

      case ScopeIterator::ScopeTypeLocal: {
        os << "Local:\n";
        function_->shared()->scope_info()->Print();
        if (!CurrentContext().is_null()) {
          CurrentContext()->Print(os);
          if (CurrentContext()->has_extension()) {
            Handle<Object> extension(CurrentContext()->extension(), isolate_);
            if (extension->IsJSContextExtensionObject()) {
              extension->Print(os);
            }
          }
        }
        break;
      }

      case ScopeIterator::ScopeTypeWith:
        os << "With:\n";
        CurrentContext()->extension()->Print(os);
        break;

      case ScopeIterator::ScopeTypeCatch:
        os << "Catch:\n";
        CurrentContext()->extension()->Print(os);
        CurrentContext()->get(Context::THROWN_OBJECT_INDEX)->Print(os);
        break;

      case ScopeIterator::ScopeTypeClosure:
        os << "Closure:\n";
        CurrentContext()->Print(os);
        if (CurrentContext()->has_extension()) {
          Handle<Object> extension(CurrentContext()->extension(), isolate_);
          if (extension->IsJSContextExtensionObject()) {
            extension->Print(os);
          }
        }
        break;

      case ScopeIterator::ScopeTypeScript:
        os << "Script:\n";
        CurrentContext()
            ->global_object()
            ->native_context()
            ->script_context_table()
            ->Print(os);
        break;

      default:
        UNREACHABLE();
    }
    PrintF("\n");
  }
#endif

 private:
  Isolate* isolate_;
  JavaScriptFrame* frame_;
  int inlined_jsframe_index_;
  Handle<JSFunction> function_;
  Handle<Context> context_;
  List<Handle<ScopeInfo> > nested_scope_chain_;
  bool seen_script_scope_;
  bool failed_;

  void RetrieveScopeChain(Scope* scope,
                          Handle<SharedFunctionInfo> shared_info) {
    if (scope != NULL) {
      int source_position = shared_info->code()->SourcePosition(frame_->pc());
      scope->GetNestedScopeChain(isolate_, &nested_scope_chain_,
                                 source_position);
    } else {
      // A failed reparse indicates that the preparser has diverged from the
      // parser or that the preparse data given to the initial parse has been
      // faulty. We fail in debug mode but in release mode we only provide the
      // information we get from the context chain but nothing about
      // completely stack allocated scopes or stack allocated locals.
      // Or it could be due to stack overflow.
      DCHECK(isolate_->has_pending_exception());
      failed_ = true;
    }
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};


RUNTIME_FUNCTION(Runtime_GetScopeCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(isolate, id);
  JavaScriptFrame* frame = it.frame();

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, frame, 0); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


// Returns the list of step-in positions (text offset) in a function of the
// stack frame in a range from the current debug break position to the end
// of the corresponding statement.
RUNTIME_FUNCTION(Runtime_GetStepInPositions) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  RUNTIME_ASSERT(!frame_it.done());

  List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
  frame_it.frame()->Summarize(&frames);
  FrameSummary summary = frames.first();

  Handle<JSFunction> fun = Handle<JSFunction>(summary.function());
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>(fun->shared());

  if (!isolate->debug()->EnsureDebugInfo(shared, fun)) {
    return isolate->heap()->undefined_value();
  }

  Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared);

  // Find range of break points starting from the break point where execution
  // has stopped.
  Address call_pc = summary.pc() - 1;
  List<BreakLocation> locations;
  BreakLocation::FromAddressSameStatement(debug_info, ALL_BREAK_LOCATIONS,
                                          call_pc, &locations);

  Handle<JSArray> array = isolate->factory()->NewJSArray(locations.length());

  int index = 0;
  for (BreakLocation location : locations) {
    bool accept;
    if (location.pc() > summary.pc()) {
      accept = true;
    } else {
      StackFrame::Id break_frame_id = isolate->debug()->break_frame_id();
      // The break point is near our pc. Could be a step-in possibility,
      // that is currently taken by active debugger call.
      if (break_frame_id == StackFrame::NO_ID) {
        // We are not stepping.
        accept = false;
      } else {
        JavaScriptFrameIterator additional_frame_it(isolate, break_frame_id);
        // If our frame is a top frame and we are stepping, we can do step-in
        // at this place.
        accept = additional_frame_it.frame()->id() == id;
      }
    }
    if (accept) {
      if (location.IsStepInLocation()) {
        Smi* position_value = Smi::FromInt(location.position());
        RETURN_FAILURE_ON_EXCEPTION(
            isolate, JSObject::SetElement(
                         array, index, Handle<Object>(position_value, isolate),
                         NONE, SLOPPY));
        index++;
      }
    }
  }
  return *array;
}


static const int kScopeDetailsTypeIndex = 0;
static const int kScopeDetailsObjectIndex = 1;
static const int kScopeDetailsSize = 2;


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeScopeDetails(
    Isolate* isolate, ScopeIterator* it) {
  // Calculate the size of the result.
  int details_size = kScopeDetailsSize;
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(it->Type()));
  Handle<JSObject> scope_object;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, scope_object, it->ScopeObject(),
                             JSObject);
  details->set(kScopeDetailsObjectIndex, *scope_object);

  return isolate->factory()->NewJSArrayWithElements(details);
}


// Return an array with scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: inlined frame index
// args[3]: number: scope index
//
// The array returned contains the following information:
// 0: Scope type
// 1: Scope object
RUNTIME_FUNCTION(Runtime_GetScopeDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[3]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, frame, inlined_jsframe_index);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }
  Handle<JSObject> details;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, details,
                                     MaterializeScopeDetails(isolate, &it));
  return *details;
}


// Return an array of scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: inlined frame index
// args[3]: boolean: ignore nested scopes
//
// The array returned contains arrays with the following information:
// 0: Scope type
// 1: Scope object
RUNTIME_FUNCTION(Runtime_GetAllScopesDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3 || args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

  bool ignore_nested_scopes = false;
  if (args.length() == 4) {
    CONVERT_BOOLEAN_ARG_CHECKED(flag, 3);
    ignore_nested_scopes = flag;
  }

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();

  List<Handle<JSObject> > result(4);
  ScopeIterator it(isolate, frame, inlined_jsframe_index, ignore_nested_scopes);
  for (; !it.Done(); it.Next()) {
    Handle<JSObject> details;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, details,
                                       MaterializeScopeDetails(isolate, &it));
    result.Add(details);
  }

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(result.length());
  for (int i = 0; i < result.length(); ++i) {
    array->set(i, *result[i]);
  }
  return *isolate->factory()->NewJSArrayWithElements(array);
}


RUNTIME_FUNCTION(Runtime_GetFunctionScopeCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, fun); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


RUNTIME_FUNCTION(Runtime_GetFunctionScopeDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, fun);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }

  Handle<JSObject> details;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, details,
                                     MaterializeScopeDetails(isolate, &it));
  return *details;
}


static bool SetScopeVariableValue(ScopeIterator* it, int index,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  for (int n = 0; !it->Done() && n < index; it->Next()) {
    n++;
  }
  if (it->Done()) {
    return false;
  }
  return it->SetVariableValue(variable_name, new_value);
}


// Change variable value in closure or local scope
// args[0]: number or JsFunction: break id or function
// args[1]: number: frame index (when arg[0] is break id)
// args[2]: number: inlined frame index (when arg[0] is break id)
// args[3]: number: scope index
// args[4]: string: variable name
// args[5]: object: new value
//
// Return true if success and false otherwise
RUNTIME_FUNCTION(Runtime_SetScopeVariableValue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 6);

  // Check arguments.
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[3]);
  CONVERT_ARG_HANDLE_CHECKED(String, variable_name, 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_value, 5);

  bool res;
  if (args[0]->IsNumber()) {
    CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
    RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

    CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
    CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

    // Get the frame where the debugging is performed.
    StackFrame::Id id = UnwrapFrameId(wrapped_id);
    JavaScriptFrameIterator frame_it(isolate, id);
    JavaScriptFrame* frame = frame_it.frame();

    ScopeIterator it(isolate, frame, inlined_jsframe_index);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  } else {
    CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
    ScopeIterator it(isolate, fun);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  }

  return isolate->heap()->ToBoolean(res);
}


RUNTIME_FUNCTION(Runtime_DebugPrintScopes) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);

#ifdef DEBUG
  // Print the scopes for the top frame.
  StackFrameLocator locator(isolate);
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  for (ScopeIterator it(isolate, frame, 0); !it.Done(); it.Next()) {
    it.DebugPrint();
  }
#endif
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetThreadCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  // Count all archived V8 threads.
  int n = 0;
  for (ThreadState* thread = isolate->thread_manager()->FirstThreadStateInUse();
       thread != NULL; thread = thread->Next()) {
    n++;
  }

  // Total number of threads is current thread and archived threads.
  return Smi::FromInt(n + 1);
}


static const int kThreadDetailsCurrentThreadIndex = 0;
static const int kThreadDetailsThreadIdIndex = 1;
static const int kThreadDetailsSize = 2;

// Return an array with thread details
// args[0]: number: break id
// args[1]: number: thread index
//
// The array returned contains the following information:
// 0: Is current thread?
// 1: Thread id
RUNTIME_FUNCTION(Runtime_GetThreadDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Allocate array for result.
  Handle<FixedArray> details =
      isolate->factory()->NewFixedArray(kThreadDetailsSize);

  // Thread index 0 is current thread.
  if (index == 0) {
    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->true_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(ThreadId::Current().ToInteger()));
  } else {
    // Find the thread with the requested index.
    int n = 1;
    ThreadState* thread = isolate->thread_manager()->FirstThreadStateInUse();
    while (index != n && thread != NULL) {
      thread = thread->Next();
      n++;
    }
    if (thread == NULL) {
      return isolate->heap()->undefined_value();
    }

    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->false_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(thread->id().ToInteger()));
  }

  // Convert to JS array and return.
  return *isolate->factory()->NewJSArrayWithElements(details);
}


// Sets the disable break state
// args[0]: disable break state
RUNTIME_FUNCTION(Runtime_SetDisableBreak) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 0);
  isolate->debug()->set_disable_break(disable_break);
  return isolate->heap()->undefined_value();
}


static bool IsPositionAlignmentCodeCorrect(int alignment) {
  return alignment == STATEMENT_ALIGNED || alignment == BREAK_POSITION_ALIGNED;
}


RUNTIME_FUNCTION(Runtime_GetBreakLocations) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[1]);

  if (!IsPositionAlignmentCodeCorrect(statement_aligned_code)) {
    return isolate->ThrowIllegalOperation();
  }
  BreakPositionAlignment alignment =
      static_cast<BreakPositionAlignment>(statement_aligned_code);

  Handle<SharedFunctionInfo> shared(fun->shared());
  // Find the number of break points
  Handle<Object> break_locations =
      Debug::GetSourceBreakLocations(shared, alignment);
  if (break_locations->IsUndefined()) return isolate->heap()->undefined_value();
  // Return array as JS array
  return *isolate->factory()->NewJSArrayWithElements(
      Handle<FixedArray>::cast(break_locations));
}


// Set a break point in a function.
// args[0]: function
// args[1]: number: break source position (within the function source)
// args[2]: number: break point object
RUNTIME_FUNCTION(Runtime_SetFunctionBreakPoint) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= function->shared()->start_position() &&
                 source_position <= function->shared()->end_position());
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 2);

  // Set break point.
  RUNTIME_ASSERT(isolate->debug()->SetBreakPoint(
      function, break_point_object_arg, &source_position));

  return Smi::FromInt(source_position);
}


// Changes the state of a break point in a script and returns source position
// where break point was set. NOTE: Regarding performance see the NOTE for
// GetScriptFromScriptData.
// args[0]: script to set break point in
// args[1]: number: break source position (within the script source)
// args[2]: number, breakpoint position alignment
// args[3]: number: break point object
RUNTIME_FUNCTION(Runtime_SetScriptBreakPoint) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 3);

  if (!IsPositionAlignmentCodeCorrect(statement_aligned_code)) {
    return isolate->ThrowIllegalOperation();
  }
  BreakPositionAlignment alignment =
      static_cast<BreakPositionAlignment>(statement_aligned_code);

  // Get the script from the script wrapper.
  RUNTIME_ASSERT(wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(wrapper->value()));

  // Set break point.
  if (!isolate->debug()->SetBreakPointForScript(script, break_point_object_arg,
                                                &source_position, alignment)) {
    return isolate->heap()->undefined_value();
  }

  return Smi::FromInt(source_position);
}


// Clear a break point
// args[0]: number: break point object
RUNTIME_FUNCTION(Runtime_ClearBreakPoint) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 0);

  // Clear break point.
  isolate->debug()->ClearBreakPoint(break_point_object_arg);

  return isolate->heap()->undefined_value();
}


// Change the state of break on exceptions.
// args[0]: Enum value indicating whether to affect caught/uncaught exceptions.
// args[1]: Boolean indicating on/off.
RUNTIME_FUNCTION(Runtime_ChangeBreakOnException) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(uint32_t, type_arg, Uint32, args[0]);
  CONVERT_BOOLEAN_ARG_CHECKED(enable, 1);

  // If the number doesn't match an enum value, the ChangeBreakOnException
  // function will default to affecting caught exceptions.
  ExceptionBreakType type = static_cast<ExceptionBreakType>(type_arg);
  // Update break point state.
  isolate->debug()->ChangeBreakOnException(type, enable);
  return isolate->heap()->undefined_value();
}


// Returns the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
RUNTIME_FUNCTION(Runtime_IsBreakOnException) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(uint32_t, type_arg, Uint32, args[0]);

  ExceptionBreakType type = static_cast<ExceptionBreakType>(type_arg);
  bool result = isolate->debug()->IsBreakOnException(type);
  return Smi::FromInt(result);
}


// Prepare for stepping
// args[0]: break id for checking execution state
// args[1]: step action from the enumeration StepAction
// args[2]: number of times to perform the step, for step out it is the number
//          of frames to step down.
RUNTIME_FUNCTION(Runtime_PrepareStep) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  if (!args[1]->IsNumber() || !args[2]->IsNumber()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  CONVERT_NUMBER_CHECKED(int, wrapped_frame_id, Int32, args[3]);

  StackFrame::Id frame_id;
  if (wrapped_frame_id == 0) {
    frame_id = StackFrame::NO_ID;
  } else {
    frame_id = UnwrapFrameId(wrapped_frame_id);
  }

  // Get the step action and check validity.
  StepAction step_action = static_cast<StepAction>(NumberToInt32(args[1]));
  if (step_action != StepIn && step_action != StepNext &&
      step_action != StepOut && step_action != StepInMin &&
      step_action != StepMin && step_action != StepFrame) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  if (frame_id != StackFrame::NO_ID && step_action != StepNext &&
      step_action != StepMin && step_action != StepOut) {
    return isolate->ThrowIllegalOperation();
  }

  // Get the number of steps.
  int step_count = NumberToInt32(args[2]);
  if (step_count < 1) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();

  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<StepAction>(step_action),
                                step_count, frame_id);
  return isolate->heap()->undefined_value();
}


// Clear all stepping set by PrepareStep.
RUNTIME_FUNCTION(Runtime_ClearStepping) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


// Helper function to find or create the arguments object for
// Runtime_DebugEvaluate.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeArgumentsObject(
    Isolate* isolate, Handle<JSObject> target, Handle<JSFunction> function) {
  // Do not materialize the arguments object for eval or top-level code.
  // Skip if "arguments" is already taken.
  if (!function->shared()->is_function()) return target;
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(
      target, isolate->factory()->arguments_string());
  if (!maybe.IsJust()) return MaybeHandle<JSObject>();
  if (maybe.FromJust()) return target;

  // FunctionGetArguments can't throw an exception.
  Handle<JSObject> arguments =
      Handle<JSObject>::cast(Accessors::FunctionGetArguments(function));
  Handle<String> arguments_str = isolate->factory()->arguments_string();
  RETURN_ON_EXCEPTION(isolate, Runtime::DefineObjectProperty(
                                   target, arguments_str, arguments, NONE),
                      JSObject);
  return target;
}


// Compile and evaluate source for the given context.
static MaybeHandle<Object> DebugEvaluate(Isolate* isolate,
                                         Handle<SharedFunctionInfo> outer_info,
                                         Handle<Context> context,
                                         Handle<Object> context_extension,
                                         Handle<Object> receiver,
                                         Handle<String> source) {
  if (context_extension->IsJSObject()) {
    Handle<JSObject> extension = Handle<JSObject>::cast(context_extension);
    Handle<JSFunction> closure(context->closure(), isolate);
    context = isolate->factory()->NewWithContext(closure, context, extension);
  }

  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, eval_fun,
                             Compiler::GetFunctionFromEval(
                                 source, outer_info, context, SLOPPY,
                                 NO_PARSE_RESTRICTION, RelocInfo::kNoPosition),
                             Object);

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, Execution::Call(isolate, eval_fun, receiver, 0, NULL),
      Object);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, result);
    // TODO(verwaest): This will crash when the global proxy is detached.
    result = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
  }

  // Clear the oneshot breakpoints so that the debugger does not step further.
  isolate->debug()->ClearStepping();
  return result;
}


static Handle<JSObject> NewJSObjectWithNullProto(Isolate* isolate) {
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<Map> new_map =
      Map::Copy(Handle<Map>(result->map()), "ObjectWithNullProto");
  Map::SetPrototype(new_map, isolate->factory()->null_value());
  JSObject::MigrateToMap(result, new_map);
  return result;
}


namespace {

// This class builds a context chain for evaluation of expressions
// in debugger.
// The scope chain leading up to a breakpoint where evaluation occurs
// looks like:
// - [a mix of with, catch and block scopes]
//    - [function stack + context]
//      - [outer context]
// The builder materializes all stack variables into properties of objects;
// the expression is then evaluated as if it is inside a series of 'with'
// statements using those objects. To this end, the builder builds a new
// context chain, based on a scope chain:
//   - every With and Catch scope begets a cloned context
//   - Block scope begets one or two contexts:
//       - if a block has context-allocated varaibles, its context is cloned
//       - stack locals are materizalized as a With context
//   - Local scope begets a With context for materizalized locals, chained to
//     original function context. Original function context is the end of
//     the chain.
class EvaluationContextBuilder {
 public:
  EvaluationContextBuilder(Isolate* isolate, JavaScriptFrame* frame,
                           int inlined_jsframe_index)
      : isolate_(isolate),
        frame_(frame),
        inlined_jsframe_index_(inlined_jsframe_index) {
    FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
    Handle<JSFunction> function =
        handle(JSFunction::cast(frame_inspector.GetFunction()));
    Handle<Context> outer_context = handle(function->context(), isolate);
    outer_info_ = handle(function->shared());
    Handle<Context> inner_context;

    bool stop = false;
    for (ScopeIterator it(isolate, frame, inlined_jsframe_index);
         !it.Failed() && !it.Done() && !stop; it.Next()) {
      ScopeIterator::ScopeType scope_type = it.Type();

      if (scope_type == ScopeIterator::ScopeTypeLocal) {
        Handle<JSObject> materialized_function =
            NewJSObjectWithNullProto(isolate);

        if (!MaterializeStackLocalsWithFrameInspector(
                 isolate, materialized_function, function, &frame_inspector)
                 .ToHandle(&materialized_function))
          return;

        if (!MaterializeArgumentsObject(isolate, materialized_function,
                                        function)
                 .ToHandle(&materialized_function))
          return;

        Handle<Context> parent_context =
            it.HasContext() ? it.CurrentContext() : outer_context;
        Handle<Context> with_context = isolate->factory()->NewWithContext(
            function, parent_context, materialized_function);

        ContextChainElement context_chain_element;
        context_chain_element.original_context = it.CurrentContext();
        context_chain_element.materialized_object = materialized_function;
        context_chain_element.scope_info = it.CurrentScopeInfo();
        context_chain_.Add(context_chain_element);

        stop = true;
        RecordContextsInChain(&inner_context, with_context, with_context);
      } else if (scope_type == ScopeIterator::ScopeTypeCatch ||
                 scope_type == ScopeIterator::ScopeTypeWith) {
        Handle<Context> cloned_context =
            Handle<Context>::cast(FixedArray::CopySize(
                it.CurrentContext(), it.CurrentContext()->length()));

        ContextChainElement context_chain_element;
        context_chain_element.original_context = it.CurrentContext();
        context_chain_element.cloned_context = cloned_context;
        context_chain_.Add(context_chain_element);

        RecordContextsInChain(&inner_context, cloned_context, cloned_context);
      } else if (scope_type == ScopeIterator::ScopeTypeBlock) {
        Handle<JSObject> materialized_object =
            NewJSObjectWithNullProto(isolate);
        if (!MaterializeStackLocalsWithFrameInspector(
                 isolate, materialized_object, it.CurrentScopeInfo(),
                 &frame_inspector).ToHandle(&materialized_object))
          return;
        if (it.HasContext()) {
          Handle<Context> cloned_context =
              Handle<Context>::cast(FixedArray::CopySize(
                  it.CurrentContext(), it.CurrentContext()->length()));
          Handle<Context> with_context = isolate->factory()->NewWithContext(
              function, cloned_context, materialized_object);

          ContextChainElement context_chain_element;
          context_chain_element.original_context = it.CurrentContext();
          context_chain_element.cloned_context = cloned_context;
          context_chain_element.materialized_object = materialized_object;
          context_chain_element.scope_info = it.CurrentScopeInfo();
          context_chain_.Add(context_chain_element);

          RecordContextsInChain(&inner_context, cloned_context, with_context);
        } else {
          Handle<Context> with_context = isolate->factory()->NewWithContext(
              function, outer_context, materialized_object);

          ContextChainElement context_chain_element;
          context_chain_element.materialized_object = materialized_object;
          context_chain_element.scope_info = it.CurrentScopeInfo();
          context_chain_.Add(context_chain_element);

          RecordContextsInChain(&inner_context, with_context, with_context);
        }
      } else {
        stop = true;
      }
    }
    if (innermost_context_.is_null()) {
      innermost_context_ = outer_context;
    }
    DCHECK(!innermost_context_.is_null());
  }

  void UpdateVariables() {
    for (int i = 0; i < context_chain_.length(); i++) {
      ContextChainElement element = context_chain_[i];
      if (!element.original_context.is_null() &&
          !element.cloned_context.is_null()) {
        Handle<Context> cloned_context = element.cloned_context;
        cloned_context->CopyTo(
            Context::MIN_CONTEXT_SLOTS, *element.original_context,
            Context::MIN_CONTEXT_SLOTS,
            cloned_context->length() - Context::MIN_CONTEXT_SLOTS);
      }
      if (!element.materialized_object.is_null()) {
        // Write back potential changes to materialized stack locals to the
        // stack.
        UpdateStackLocalsFromMaterializedObject(
            isolate_, element.materialized_object, element.scope_info, frame_,
            inlined_jsframe_index_);
      }
    }
  }

  Handle<Context> innermost_context() const { return innermost_context_; }
  Handle<SharedFunctionInfo> outer_info() const { return outer_info_; }

 private:
  struct ContextChainElement {
    Handle<Context> original_context;
    Handle<Context> cloned_context;
    Handle<JSObject> materialized_object;
    Handle<ScopeInfo> scope_info;
  };

  void RecordContextsInChain(Handle<Context>* inner_context,
                             Handle<Context> first, Handle<Context> last) {
    if (!inner_context->is_null()) {
      (*inner_context)->set_previous(*last);
    } else {
      innermost_context_ = last;
    }
    *inner_context = first;
  }

  Handle<SharedFunctionInfo> outer_info_;
  Handle<Context> innermost_context_;
  List<ContextChainElement> context_chain_;
  Isolate* isolate_;
  JavaScriptFrame* frame_;
  int inlined_jsframe_index_;
};
}


// Evaluate a piece of JavaScript in the context of a stack frame for
// debugging.  Things that need special attention are:
// - Parameters and stack-allocated locals need to be materialized.  Altered
//   values need to be written back to the stack afterwards.
// - The arguments object needs to materialized.
RUNTIME_FUNCTION(Runtime_DebugEvaluate) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  DCHECK(args.length() == 6);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, context_extension, 5);

  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug(), disable_break);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(isolate, id);
  JavaScriptFrame* frame = it.frame();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = FindSavedContextForFrame(isolate, frame);

  SaveContext savex(isolate);
  isolate->set_context(*(save->context()));

  // Materialize stack locals and the arguments object.

  EvaluationContextBuilder context_builder(isolate, frame,
                                           inlined_jsframe_index);
  if (isolate->has_pending_exception()) {
    return isolate->heap()->exception();
  }


  Handle<Object> receiver(frame->receiver(), isolate);
  MaybeHandle<Object> maybe_result = DebugEvaluate(
      isolate, context_builder.outer_info(),
      context_builder.innermost_context(), context_extension, receiver, source);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, maybe_result);
  context_builder.UpdateVariables();
  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugEvaluateGlobal) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  DCHECK(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, context_extension, 3);

  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug(), disable_break);

  // Enter the top context from before the debugger was invoked.
  SaveContext save(isolate);
  SaveContext* top = &save;
  while (top != NULL && *top->context() == *isolate->debug()->debug_context()) {
    top = top->prev();
  }
  if (top != NULL) {
    isolate->set_context(*top->context());
  }

  // Get the native context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = isolate->native_context();
  Handle<JSObject> receiver(context->global_proxy());
  Handle<SharedFunctionInfo> outer_info(context->closure()->shared(), isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, DebugEvaluate(isolate, outer_info, context,
                                     context_extension, receiver, source));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugGetLoadedScripts) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);

  Handle<FixedArray> instances;
  {
    DebugScope debug_scope(isolate->debug());
    // Fill the script objects.
    instances = isolate->debug()->GetLoadedScripts();
  }

  // Convert the script objects to proper JS objects.
  for (int i = 0; i < instances->length(); i++) {
    Handle<Script> script = Handle<Script>(Script::cast(instances->get(i)));
    // Get the script wrapper in a local handle before calling GetScriptWrapper,
    // because using
    //   instances->set(i, *GetScriptWrapper(script))
    // is unsafe as GetScriptWrapper might call GC and the C++ compiler might
    // already have dereferenced the instances handle.
    Handle<JSObject> wrapper = Script::GetWrapper(script);
    instances->set(i, *wrapper);
  }

  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
}


// Helper function used by Runtime_DebugReferencedBy below.
static int DebugReferencedBy(HeapIterator* iterator, JSObject* target,
                             Object* instance_filter, int max_references,
                             FixedArray* instances, int instances_size,
                             JSFunction* arguments_function) {
  Isolate* isolate = target->GetIsolate();
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_allocation;

  // Iterate the heap.
  int count = 0;
  JSObject* last = NULL;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator->next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      // Skip context extension objects and argument arrays as these are
      // checked in the context of functions using them.
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->IsJSContextExtensionObject() ||
          obj->map()->GetConstructor() == arguments_function) {
        continue;
      }

      // Check if the JS object has a reference to the object looked for.
      if (obj->ReferencesObject(target)) {
        // Check instance filter if supplied. This is normally used to avoid
        // references from mirror objects (see Runtime_IsInPrototypeChain).
        if (!instance_filter->IsUndefined()) {
          for (PrototypeIterator iter(isolate, obj); !iter.IsAtEnd();
               iter.Advance()) {
            if (iter.GetCurrent() == instance_filter) {
              obj = NULL;  // Don't add this object.
              break;
            }
          }
        }

        if (obj != NULL) {
          // Valid reference found add to instance array if supplied an update
          // count.
          if (instances != NULL && count < instances_size) {
            instances->set(count, obj);
          }
          last = obj;
          count++;
        }
      }
    }
  }

  // Check for circular reference only. This can happen when the object is only
  // referenced from mirrors and has a circular reference in which case the
  // object is not really alive and would have been garbage collected if not
  // referenced from the mirror.
  if (count == 1 && last == target) {
    count = 0;
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects with direct references to an object
// args[0]: the object to find references to
// args[1]: constructor function for instances to exclude (Mirror)
// args[2]: the the maximum number of objects to return
RUNTIME_FUNCTION(Runtime_DebugReferencedBy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);

  // Check parameters.
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, instance_filter, 1);
  RUNTIME_ASSERT(instance_filter->IsUndefined() ||
                 instance_filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);


  // Get the constructor function for context extension and arguments array.
  Handle<JSFunction> arguments_function(
      JSFunction::cast(isolate->sloppy_arguments_map()->GetConstructor()));

  // Get the number of referencing objects.
  int count;
  // First perform a full GC in order to avoid dead objects and to make the heap
  // iterable.
  Heap* heap = isolate->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "%DebugConstructedBy");
  {
    HeapIterator heap_iterator(heap);
    count = DebugReferencedBy(&heap_iterator, *target, *instance_filter,
                              max_references, NULL, 0, *arguments_function);
  }

  // Allocate an array to hold the result.
  Handle<FixedArray> instances = isolate->factory()->NewFixedArray(count);

  // Fill the referencing objects.
  {
    HeapIterator heap_iterator(heap);
    count = DebugReferencedBy(&heap_iterator, *target, *instance_filter,
                              max_references, *instances, count,
                              *arguments_function);
  }

  // Return result as JS array.
  Handle<JSFunction> constructor = isolate->array_function();

  Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
}


// Helper function used by Runtime_DebugConstructedBy below.
static int DebugConstructedBy(HeapIterator* iterator, JSFunction* constructor,
                              int max_references, FixedArray* instances,
                              int instances_size) {
  DisallowHeapAllocation no_allocation;

  // Iterate the heap.
  int count = 0;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator->next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->map()->GetConstructor() == constructor) {
        // Valid reference found add to instance array if supplied an update
        // count.
        if (instances != NULL && count < instances_size) {
          instances->set(count, obj);
        }
        count++;
      }
    }
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects constructed by a specific function.
// args[0]: the constructor to find instances of
// args[1]: the the maximum number of objects to return
RUNTIME_FUNCTION(Runtime_DebugConstructedBy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);


  // Check parameters.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the number of referencing objects.
  int count;
  // First perform a full GC in order to avoid dead objects and to make the heap
  // iterable.
  Heap* heap = isolate->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "%DebugConstructedBy");
  {
    HeapIterator heap_iterator(heap);
    count = DebugConstructedBy(&heap_iterator, *constructor, max_references,
                               NULL, 0);
  }

  // Allocate an array to hold the result.
  Handle<FixedArray> instances = isolate->factory()->NewFixedArray(count);

  // Fill the referencing objects.
  {
    HeapIterator heap_iterator2(heap);
    count = DebugConstructedBy(&heap_iterator2, *constructor, max_references,
                               *instances, count);
  }

  // Return result as JS array.
  Handle<JSFunction> array_function = isolate->array_function();
  Handle<JSObject> result = isolate->factory()->NewJSObject(array_function);
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
RUNTIME_FUNCTION(Runtime_DebugGetPrototype) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  return *Object::GetPrototypeSkipHiddenPrototypes(isolate, obj);
}


// Patches script source (should be called upon BeforeCompile event).
RUNTIME_FUNCTION(Runtime_DebugSetScriptSource) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, script_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  RUNTIME_ASSERT(script_wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(script_wrapper->value()));

  int compilation_state = script->compilation_state();
  RUNTIME_ASSERT(compilation_state == Script::COMPILATION_STATE_INITIAL);
  script->set_source(*source);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetInferredName) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return f->shared()->inferred_name();
}


// A testing entry. Returns statement position which is the closest to
// source_position.
RUNTIME_FUNCTION(Runtime_GetFunctionCodePositionFromSource) {
  HandleScope scope(isolate);
  CHECK(isolate->debug()->live_edit_enabled());
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);

  Handle<Code> code(function->code(), isolate);

  if (code->kind() != Code::FUNCTION &&
      code->kind() != Code::OPTIMIZED_FUNCTION) {
    return isolate->heap()->undefined_value();
  }

  RelocIterator it(*code, RelocInfo::ModeMask(RelocInfo::STATEMENT_POSITION));
  int closest_pc = 0;
  int distance = kMaxInt;
  while (!it.done()) {
    int statement_position = static_cast<int>(it.rinfo()->data());
    // Check if this break point is closer that what was previously found.
    if (source_position <= statement_position &&
        statement_position - source_position < distance) {
      closest_pc =
          static_cast<int>(it.rinfo()->pc() - code->instruction_start());
      distance = statement_position - source_position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    it.next();
  }

  return Smi::FromInt(closest_pc);
}


// Calls specified function with or without entering the debugger.
// This is used in unit tests to run code as if debugger is entered or simply
// to have a stack with C++ frame in the middle.
RUNTIME_FUNCTION(Runtime_ExecuteInDebugContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DebugScope debug_scope(isolate->debug());
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function, handle(function->global_proxy()), 0,
                      NULL));
  return *result;
}


RUNTIME_FUNCTION(Runtime_GetDebugContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  Handle<Context> context = isolate->debug()->GetDebugContext();
  if (context.is_null()) return isolate->heap()->undefined_value();
  context->set_security_token(isolate->native_context()->security_token());
  return context->global_proxy();
}


// Performs a GC.
// Presently, it only does a full GC.
RUNTIME_FUNCTION(Runtime_CollectGarbage) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags, "%CollectGarbage");
  return isolate->heap()->undefined_value();
}


// Gets the current heap usage.
RUNTIME_FUNCTION(Runtime_GetHeapUsage) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  int usage = static_cast<int>(isolate->heap()->SizeOfObjects());
  if (!Smi::IsValid(usage)) {
    return *isolate->factory()->NewNumberFromInt(usage);
  }
  return Smi::FromInt(usage);
}


// Finds the script object from the script data. NOTE: This operation uses
// heap traversal to find the function generated for the source position
// for the requested break point. For lazily compiled functions several heap
// traversals might be required rendering this operation as a rather slow
// operation. However for setting break points which is normally done through
// some kind of user interaction the performance is not crucial.
RUNTIME_FUNCTION(Runtime_GetScript) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, script_name, 0);

  Handle<Script> found;
  Heap* heap = isolate->heap();
  {
    HeapIterator iterator(heap);
    HeapObject* obj = NULL;
    while ((obj = iterator.next()) != NULL) {
      if (!obj->IsScript()) continue;
      Script* script = Script::cast(obj);
      if (!script->name()->IsString()) continue;
      String* name = String::cast(script->name());
      if (name->Equals(*script_name)) {
        found = Handle<Script>(script, isolate);
        break;
      }
    }
  }

  if (found.is_null()) return heap->undefined_value();
  return *Script::GetWrapper(found);
}


// Check whether debugger is about to step into the callback that is passed
// to a built-in function such as Array.forEach.
RUNTIME_FUNCTION(Runtime_DebugCallbackSupportsStepping) {
  DCHECK(args.length() == 1);
  Debug* debug = isolate->debug();
  if (!debug->is_active() || !debug->IsStepping() ||
      debug->last_step_action() != StepIn) {
    return isolate->heap()->false_value();
  }
  CONVERT_ARG_CHECKED(Object, callback, 0);
  // We do not step into the callback if it's a builtin other than a bound,
  // or not even a function.
  return isolate->heap()->ToBoolean(
      callback->IsJSFunction() &&
      (!JSFunction::cast(callback)->IsBuiltin() ||
       JSFunction::cast(callback)->shared()->bound()));
}


// Set one shot breakpoints for the callback function that is passed to a
// built-in function such as Array.forEach to enable stepping into the callback.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInIfStepping) {
  DCHECK(args.length() == 1);
  Debug* debug = isolate->debug();
  if (!debug->IsStepping()) return isolate->heap()->undefined_value();

  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RUNTIME_ASSERT(object->IsJSFunction() || object->IsJSGeneratorObject());
  Handle<JSFunction> fun;
  if (object->IsJSFunction()) {
    fun = Handle<JSFunction>::cast(object);
  } else {
    fun = Handle<JSFunction>(
        Handle<JSGeneratorObject>::cast(object)->function(), isolate);
  }
  // When leaving the function, step out has been activated, but not performed
  // if we do not leave the builtin.  To be able to step into the function
  // again, we need to clear the step out at this point.
  debug->ClearStepOut();
  debug->FloodWithOneShotGeneric(fun);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPushPromise) {
  DCHECK(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 1);
  isolate->PushPromise(promise, function);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPopPromise) {
  DCHECK(args.length() == 0);
  SealHandleScope shs(isolate);
  isolate->PopPromise();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPromiseEvent) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, data, 0);
  isolate->debug()->OnPromiseEvent(data);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugAsyncTaskEvent) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, data, 0);
  isolate->debug()->OnAsyncTaskEvent(data);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugIsActive) {
  SealHandleScope shs(isolate);
  return Smi::FromInt(isolate->debug()->is_active());
}


RUNTIME_FUNCTION(Runtime_DebugBreakInOptimizedCode) {
  UNIMPLEMENTED();
  return NULL;
}
}
}  // namespace v8::internal
