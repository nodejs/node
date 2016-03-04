// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/debug/debug.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DebugBreak) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  isolate->debug()->Break(args, it.frame());
  isolate->debug()->SetAfterBreakTarget(it.frame());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_HandleDebuggerStatement) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  if (isolate->debug()->break_points_active()) {
    isolate->debug()->HandleDebugBreak();
  }
  return isolate->heap()->undefined_value();
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
        MaybeHandle<Object> maybe_result =
            JSObject::GetPropertyWithAccessor(it, SLOPPY);
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


static Handle<Object> DebugGetProperty(Handle<Object> object,
                                       Handle<Name> name) {
  LookupIterator it(object, name);
  return DebugGetProperty(&it);
}


template <class IteratorType>
static MaybeHandle<JSArray> GetIteratorInternalProperties(
    Isolate* isolate, Handle<IteratorType> object) {
  Factory* factory = isolate->factory();
  Handle<IteratorType> iterator = Handle<IteratorType>::cast(object);
  RUNTIME_ASSERT_HANDLIFIED(iterator->kind()->IsSmi(), JSArray);
  const char* kind = NULL;
  switch (Smi::cast(iterator->kind())->value()) {
    case IteratorType::kKindKeys:
      kind = "keys";
      break;
    case IteratorType::kKindValues:
      kind = "values";
      break;
    case IteratorType::kKindEntries:
      kind = "entries";
      break;
    default:
      RUNTIME_ASSERT_HANDLIFIED(false, JSArray);
  }

  Handle<FixedArray> result = factory->NewFixedArray(2 * 3);
  Handle<String> has_more =
      factory->NewStringFromAsciiChecked("[[IteratorHasMore]]");
  result->set(0, *has_more);
  result->set(1, isolate->heap()->ToBoolean(iterator->HasMore()));

  Handle<String> index =
      factory->NewStringFromAsciiChecked("[[IteratorIndex]]");
  result->set(2, *index);
  result->set(3, iterator->index());

  Handle<String> iterator_kind =
      factory->NewStringFromAsciiChecked("[[IteratorKind]]");
  result->set(4, *iterator_kind);
  Handle<String> kind_str = factory->NewStringFromAsciiChecked(kind);
  result->set(5, *kind_str);
  return factory->NewJSArrayWithElements(result);
}


MaybeHandle<JSArray> Runtime::GetInternalProperties(Isolate* isolate,
                                                    Handle<Object> object) {
  Factory* factory = isolate->factory();
  if (object->IsJSBoundFunction()) {
    Handle<JSBoundFunction> function = Handle<JSBoundFunction>::cast(object);

    Handle<FixedArray> result = factory->NewFixedArray(2 * 3);
    Handle<String> target =
        factory->NewStringFromAsciiChecked("[[TargetFunction]]");
    result->set(0, *target);
    result->set(1, function->bound_target_function());

    Handle<String> bound_this =
        factory->NewStringFromAsciiChecked("[[BoundThis]]");
    result->set(2, *bound_this);
    result->set(3, function->bound_this());

    Handle<String> bound_args =
        factory->NewStringFromAsciiChecked("[[BoundArgs]]");
    result->set(4, *bound_args);
    Handle<FixedArray> bound_arguments =
        factory->CopyFixedArray(handle(function->bound_arguments(), isolate));
    Handle<JSArray> arguments_array =
        factory->NewJSArrayWithElements(bound_arguments);
    result->set(5, *arguments_array);
    return factory->NewJSArrayWithElements(result);
  } else if (object->IsJSMapIterator()) {
    Handle<JSMapIterator> iterator = Handle<JSMapIterator>::cast(object);
    return GetIteratorInternalProperties(isolate, iterator);
  } else if (object->IsJSSetIterator()) {
    Handle<JSSetIterator> iterator = Handle<JSSetIterator>::cast(object);
    return GetIteratorInternalProperties(isolate, iterator);
  } else if (object->IsJSGeneratorObject()) {
    Handle<JSGeneratorObject> generator =
        Handle<JSGeneratorObject>::cast(object);

    const char* status = "suspended";
    if (generator->is_closed()) {
      status = "closed";
    } else if (generator->is_executing()) {
      status = "running";
    } else {
      DCHECK(generator->is_suspended());
    }

    Handle<FixedArray> result = factory->NewFixedArray(2 * 3);
    Handle<String> generator_status =
        factory->NewStringFromAsciiChecked("[[GeneratorStatus]]");
    result->set(0, *generator_status);
    Handle<String> status_str = factory->NewStringFromAsciiChecked(status);
    result->set(1, *status_str);

    Handle<String> function =
        factory->NewStringFromAsciiChecked("[[GeneratorFunction]]");
    result->set(2, *function);
    result->set(3, generator->function());

    Handle<String> receiver =
        factory->NewStringFromAsciiChecked("[[GeneratorReceiver]]");
    result->set(4, *receiver);
    result->set(5, generator->receiver());
    return factory->NewJSArrayWithElements(result);
  } else if (Object::IsPromise(object)) {
    Handle<JSObject> promise = Handle<JSObject>::cast(object);

    Handle<Object> status_obj =
        DebugGetProperty(promise, isolate->factory()->promise_status_symbol());
    RUNTIME_ASSERT_HANDLIFIED(status_obj->IsSmi(), JSArray);
    const char* status = "rejected";
    int status_val = Handle<Smi>::cast(status_obj)->value();
    switch (status_val) {
      case +1:
        status = "resolved";
        break;
      case 0:
        status = "pending";
        break;
      default:
        DCHECK_EQ(-1, status_val);
    }

    Handle<FixedArray> result = factory->NewFixedArray(2 * 2);
    Handle<String> promise_status =
        factory->NewStringFromAsciiChecked("[[PromiseStatus]]");
    result->set(0, *promise_status);
    Handle<String> status_str = factory->NewStringFromAsciiChecked(status);
    result->set(1, *status_str);

    Handle<Object> value_obj =
        DebugGetProperty(promise, isolate->factory()->promise_value_symbol());
    Handle<String> promise_value =
        factory->NewStringFromAsciiChecked("[[PromiseValue]]");
    result->set(2, *promise_value);
    result->set(3, *value_obj);
    return factory->NewJSArrayWithElements(result);
  } else if (object->IsJSValue()) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);

    Handle<FixedArray> result = factory->NewFixedArray(2);
    Handle<String> primitive_value =
        factory->NewStringFromAsciiChecked("[[PrimitiveValue]]");
    result->set(0, *primitive_value);
    result->set(1, js_value->value());
    return factory->NewJSArrayWithElements(result);
  }
  return factory->NewJSArray(0);
}


RUNTIME_FUNCTION(Runtime_DebugGetInternalProperties) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  Handle<JSArray> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Runtime::GetInternalProperties(isolate, obj));
  return *result;
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
  // TODO(verwaest): Make sure DebugGetProperty can handle arrays, and remove
  // this special case.
  if (name->AsArrayIndex(&index)) {
    Handle<FixedArray> details = isolate->factory()->NewFixedArray(2);
    Handle<Object> element_or_char;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, element_or_char,
                                       Object::GetElement(isolate, obj, index));
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
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     Object::GetElement(isolate, obj, index));
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
      // Omit functions from native and extension scripts.
      if (frames[i].function()->shared()->IsSubjectToDebugging()) n++;
    }
  }
  return Smi::FromInt(n);
}


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
  int inlined_jsframe_index =
      DebugFrameHelper::FindIndexedNonNativeFrame(&it, index);
  if (inlined_jsframe_index == -1) return heap->undefined_value();

  FrameInspector frame_inspector(it.frame(), inlined_jsframe_index, isolate);
  bool is_optimized = it.frame()->is_optimized();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save =
      DebugFrameHelper::FindSavedContextForFrame(isolate, it.frame());

  // Get the frame id.
  Handle<Object> frame_id(DebugFrameHelper::WrapFrameId(it.frame()->id()),
                          isolate);

  // Find source position in unoptimized code.
  int position = frame_inspector.GetSourcePosition();

  // Check for constructor frame.
  bool constructor = frame_inspector.IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));
  RUNTIME_ASSERT(function->shared()->IsSubjectToDebugging());
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
  Handle<Object> receiver(it.frame()->receiver(), isolate);
  DCHECK(!function->shared()->IsBuiltin());
  if (!receiver->IsJSObject() && is_sloppy(shared->language_mode())) {
    // If the receiver is not a JSObject and the function is not a builtin or
    // strict-mode we have hit an optimization where a value object is not
    // converted into a wrapped JS objects. To hide this optimization from the
    // debugger, we wrap the receiver by creating correct wrapper object based
    // on the function's native context.
    // See ECMA-262 6.0, 9.2.1.2, 6 b iii.
    if (receiver->IsUndefined()) {
      receiver = handle(function->global_proxy());
    } else {
      Context* context = function->context();
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


RUNTIME_FUNCTION(Runtime_GetScopeCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(isolate, id);
  JavaScriptFrame* frame = it.frame();
  FrameInspector frame_inspector(frame, 0, isolate);

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, &frame_inspector); !it.Done(); it.Next()) {
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
  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  RUNTIME_ASSERT(!frame_it.done());

  List<int> positions;
  isolate->debug()->GetStepinPositions(frame_it.frame(), id, &positions);
  Factory* factory = isolate->factory();
  Handle<FixedArray> array = factory->NewFixedArray(positions.length());
  for (int i = 0; i < positions.length(); ++i) {
    array->set(i, Smi::FromInt(positions[i]));
  }
  return *factory->NewJSArrayWithElements(array, FAST_SMI_ELEMENTS);
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
  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, &frame_inspector);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }
  Handle<JSObject> details;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, details,
                                     it.MaterializeScopeDetails());
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

  ScopeIterator::Option option = ScopeIterator::DEFAULT;
  if (args.length() == 4) {
    CONVERT_BOOLEAN_ARG_CHECKED(flag, 3);
    if (flag) option = ScopeIterator::IGNORE_NESTED_SCOPES;
  }

  // Get the frame where the debugging is performed.
  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);

  List<Handle<JSObject> > result(4);
  ScopeIterator it(isolate, &frame_inspector, option);
  for (; !it.Done(); it.Next()) {
    Handle<JSObject> details;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, details,
                                       it.MaterializeScopeDetails());
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
  DCHECK_EQ(1, args.length());

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);

  // Count the visible scopes.
  int n = 0;
  if (function->IsJSFunction()) {
    for (ScopeIterator it(isolate, Handle<JSFunction>::cast(function));
         !it.Done(); it.Next()) {
      n++;
    }
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
                                     it.MaterializeScopeDetails());
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
    StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
    JavaScriptFrameIterator frame_it(isolate, id);
    JavaScriptFrame* frame = frame_it.frame();
    FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);

    ScopeIterator it(isolate, &frame_inspector);
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
  FrameInspector frame_inspector(frame, 0, isolate);

  for (ScopeIterator it(isolate, &frame_inspector); !it.Done(); it.Next()) {
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
RUNTIME_FUNCTION(Runtime_SetBreakPointsActive) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_BOOLEAN_ARG_CHECKED(active, 0);
  isolate->debug()->set_break_points_active(active);
  return isolate->heap()->undefined_value();
}


static bool IsPositionAlignmentCodeCorrect(int alignment) {
  return alignment == STATEMENT_ALIGNED || alignment == BREAK_POSITION_ALIGNED;
}


RUNTIME_FUNCTION(Runtime_GetBreakLocations) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  RUNTIME_ASSERT(isolate->debug()->is_active());
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
  RUNTIME_ASSERT(isolate->debug()->is_active());
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
  RUNTIME_ASSERT(isolate->debug()->is_active());
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
  RUNTIME_ASSERT(isolate->debug()->is_active());
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
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(isolate->debug()->CheckExecutionState(break_id));

  if (!args[1]->IsNumber()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Get the step action and check validity.
  StepAction step_action = static_cast<StepAction>(NumberToInt32(args[1]));
  if (step_action != StepIn && step_action != StepNext &&
      step_action != StepOut && step_action != StepFrame) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();

  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<StepAction>(step_action));
  return isolate->heap()->undefined_value();
}


// Clear all stepping set by PrepareStep.
RUNTIME_FUNCTION(Runtime_ClearStepping) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->debug()->is_active());
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


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
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, context_extension, 5);

  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      DebugEvaluate::Local(isolate, id, inlined_jsframe_index, source,
                           disable_break, context_extension));
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
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, context_extension, 3);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      DebugEvaluate::Global(isolate, source, disable_break, context_extension));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugGetLoadedScripts) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  RUNTIME_ASSERT(isolate->debug()->is_active());

  Handle<FixedArray> instances;
  {
    DebugScope debug_scope(isolate->debug());
    if (debug_scope.failed()) {
      DCHECK(isolate->has_pending_exception());
      return isolate->heap()->exception();
    }
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


static bool HasInPrototypeChainIgnoringProxies(Isolate* isolate, Object* object,
                                               Object* proto) {
  PrototypeIterator iter(isolate, object, PrototypeIterator::START_AT_RECEIVER);
  while (true) {
    iter.AdvanceIgnoringProxies();
    if (iter.IsAtEnd()) return false;
    if (iter.IsAtEnd(proto)) return true;
  }
}


// Scan the heap for objects with direct references to an object
// args[0]: the object to find references to
// args[1]: constructor function for instances to exclude (Mirror)
// args[2]: the the maximum number of objects to return
RUNTIME_FUNCTION(Runtime_DebugReferencedBy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, filter, 1);
  RUNTIME_ASSERT(filter->IsUndefined() || filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);

  List<Handle<JSObject> > instances;
  Heap* heap = isolate->heap();
  {
    HeapIterator iterator(heap, HeapIterator::kFilterUnreachable);
    // Get the constructor function for context extension and arguments array.
    Object* arguments_fun = isolate->sloppy_arguments_map()->GetConstructor();
    HeapObject* heap_obj;
    while ((heap_obj = iterator.next())) {
      if (!heap_obj->IsJSObject()) continue;
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->IsJSContextExtensionObject()) continue;
      if (obj->map()->GetConstructor() == arguments_fun) continue;
      if (!obj->ReferencesObject(*target)) continue;
      // Check filter if supplied. This is normally used to avoid
      // references from mirror objects.
      if (!filter->IsUndefined() &&
          HasInPrototypeChainIgnoringProxies(isolate, obj, *filter)) {
        continue;
      }
      if (obj->IsJSGlobalObject()) {
        obj = JSGlobalObject::cast(obj)->global_proxy();
      }
      instances.Add(Handle<JSObject>(obj));
      if (instances.length() == max_references) break;
    }
    // Iterate the rest of the heap to satisfy HeapIterator constraints.
    while (iterator.next()) {
    }
  }

  Handle<FixedArray> result;
  if (instances.length() == 1 && instances.last().is_identical_to(target)) {
    // Check for circular reference only. This can happen when the object is
    // only referenced from mirrors and has a circular reference in which case
    // the object is not really alive and would have been garbage collected if
    // not referenced from the mirror.
    result = isolate->factory()->empty_fixed_array();
  } else {
    result = isolate->factory()->NewFixedArray(instances.length());
    for (int i = 0; i < instances.length(); ++i) result->set(i, *instances[i]);
  }
  return *isolate->factory()->NewJSArrayWithElements(result);
}


// Scan the heap for objects constructed by a specific function.
// args[0]: the constructor to find instances of
// args[1]: the the maximum number of objects to return
RUNTIME_FUNCTION(Runtime_DebugConstructedBy) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  List<Handle<JSObject> > instances;
  Heap* heap = isolate->heap();
  {
    HeapIterator iterator(heap, HeapIterator::kFilterUnreachable);
    HeapObject* heap_obj;
    while ((heap_obj = iterator.next())) {
      if (!heap_obj->IsJSObject()) continue;
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->map()->GetConstructor() != *constructor) continue;
      instances.Add(Handle<JSObject>(obj));
      if (instances.length() == max_references) break;
    }
    // Iterate the rest of the heap to satisfy HeapIterator constraints.
    while (iterator.next()) {
    }
  }

  Handle<FixedArray> result =
      isolate->factory()->NewFixedArray(instances.length());
  for (int i = 0; i < instances.length(); ++i) result->set(i, *instances[i]);
  return *isolate->factory()->NewJSArrayWithElements(result);
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
RUNTIME_FUNCTION(Runtime_DebugGetPrototype) {
  HandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  Handle<Object> prototype;
  // TODO(1543): Come up with a solution for clients to handle potential errors
  // thrown by an intermediate proxy.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, prototype,
                                     Object::GetPrototype(isolate, obj));
  return *prototype;
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
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(Object, f, 0);
  if (f->IsJSFunction()) {
    return JSFunction::cast(f)->shared()->inferred_name();
  }
  return isolate->heap()->empty_string();
}


RUNTIME_FUNCTION(Runtime_FunctionGetDebugName) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, function, 0);

  if (function->IsJSBoundFunction()) {
    return Handle<JSBoundFunction>::cast(function)->name();
  }
  Handle<Object> name =
      JSFunction::GetDebugName(Handle<JSFunction>::cast(function));
  return *name;
}


// A testing entry. Returns statement position which is the closest to
// source_position.
RUNTIME_FUNCTION(Runtime_GetFunctionCodePositionFromSource) {
  HandleScope scope(isolate);
  CHECK(isolate->debug()->live_edit_enabled());
  DCHECK(args.length() == 2);
  RUNTIME_ASSERT(isolate->debug()->is_active());
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
  if (debug_scope.failed()) {
    DCHECK(isolate->has_pending_exception());
    return isolate->heap()->exception();
  }

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
  Handle<Context> context;
  {
    DebugScope debug_scope(isolate->debug());
    if (debug_scope.failed()) {
      DCHECK(isolate->has_pending_exception());
      return isolate->heap()->exception();
    }
    context = isolate->debug()->GetDebugContext();
  }
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
  {
    Script::Iterator iterator(isolate);
    Script* script = NULL;
    while ((script = iterator.Next()) != NULL) {
      if (!script->name()->IsString()) continue;
      String* name = String::cast(script->name());
      if (name->Equals(*script_name)) {
        found = Handle<Script>(script, isolate);
        break;
      }
    }
  }

  if (found.is_null()) return isolate->heap()->undefined_value();
  return *Script::GetWrapper(found);
}


// Set one shot breakpoints for the callback function that is passed to a
// built-in function such as Array.forEach to enable stepping into the callback,
// if we are indeed stepping and the callback is subject to debugging.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInIfStepping) {
  DCHECK(args.length() == 1);
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

  isolate->debug()->PrepareStepIn(fun);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPushPromise) {
  DCHECK(args.length() == 2);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 1);
  isolate->PushPromise(promise, function);
  // If we are in step-in mode, flood the handler.
  isolate->debug()->EnableStepIn();
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
}  // namespace internal
}  // namespace v8
