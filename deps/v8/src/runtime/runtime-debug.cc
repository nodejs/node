// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-debug.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DebugBreak) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  isolate->debug()->set_return_value(value);

  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  isolate->debug()->Break(it.frame());

  isolate->debug()->SetAfterBreakTarget(it.frame());
  return *isolate->debug()->return_value();
}

RUNTIME_FUNCTION(Runtime_DebugBreakOnBytecode) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  isolate->debug()->set_return_value(value);

  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  isolate->debug()->Break(it.frame());

  // If live-edit has dropped frames, we are not going back to dispatch.
  if (LiveEdit::SetAfterBreakTarget(isolate->debug())) return Smi::FromInt(0);

  // Return the handler from the original bytecode array.
  DCHECK(it.frame()->is_interpreted());
  InterpretedFrame* interpreted_frame =
      reinterpret_cast<InterpretedFrame*>(it.frame());
  SharedFunctionInfo* shared = interpreted_frame->function()->shared();
  BytecodeArray* bytecode_array = shared->bytecode_array();
  int bytecode_offset = interpreted_frame->GetBytecodeOffset();
  interpreter::Bytecode bytecode =
      interpreter::Bytecodes::FromByte(bytecode_array->get(bytecode_offset));
  return isolate->interpreter()->GetBytecodeHandler(
      bytecode, interpreter::OperandScale::kSingle);
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
  CHECK(args[0]->IsJSFunction() || args[0]->IsUndefined(isolate) ||
        args[0]->IsNull(isolate));
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
            JSObject::GetPropertyWithAccessor(it);
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
  CHECK(iterator->kind()->IsSmi());
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
      UNREACHABLE();
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
  } else if (object->IsJSPromise()) {
    Handle<JSObject> promise = Handle<JSObject>::cast(object);

    Handle<Object> status_obj =
        DebugGetProperty(promise, isolate->factory()->promise_state_symbol());
    CHECK(status_obj->IsSmi());
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
        DebugGetProperty(promise, isolate->factory()->promise_result_symbol());
    Handle<String> promise_value =
        factory->NewStringFromAsciiChecked("[[PromiseValue]]");
    result->set(2, *promise_value);
    result->set(3, *value_obj);
    return factory->NewJSArrayWithElements(result);
  } else if (object->IsJSProxy()) {
    Handle<JSProxy> js_proxy = Handle<JSProxy>::cast(object);
    Handle<FixedArray> result = factory->NewFixedArray(3 * 2);

    Handle<String> handler_str =
        factory->NewStringFromAsciiChecked("[[Handler]]");
    result->set(0, *handler_str);
    result->set(1, js_proxy->handler());

    Handle<String> target_str =
        factory->NewStringFromAsciiChecked("[[Target]]");
    result->set(2, *target_str);
    result->set(3, js_proxy->target());

    Handle<String> is_revoked_str =
        factory->NewStringFromAsciiChecked("[[IsRevoked]]");
    result->set(4, *is_revoked_str);
    result->set(5, isolate->heap()->ToBoolean(js_proxy->IsRevoked()));
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
  RETURN_RESULT_OR_FAILURE(isolate,
                           Runtime::GetInternalProperties(isolate, obj));
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
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, name_obj, 1);

  // Convert the {name_obj} to a Name.
  Handle<Name> name;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, name,
                                     Object::ToName(isolate, name_obj));

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
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, element_or_char, JSReceiver::GetElement(isolate, obj, index));
    details->set(0, *element_or_char);
    details->set(1, PropertyDetails::Empty().AsSmi());
    return *isolate->factory()->NewJSArrayWithElements(details);
  }

  LookupIterator it(obj, name, LookupIterator::OWN);
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
    Handle<AccessorPair> accessors = Handle<AccessorPair>::cast(maybe_pair);
    details->set(3, isolate->heap()->ToBoolean(has_caught));
    Handle<Object> getter =
        AccessorPair::GetComponent(accessors, ACCESSOR_GETTER);
    Handle<Object> setter =
        AccessorPair::GetComponent(accessors, ACCESSOR_SETTER);
    details->set(4, *getter);
    details->set(5, *setter);
  }

  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_DebugGetProperty) {
  HandleScope scope(isolate);

  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
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


RUNTIME_FUNCTION(Runtime_CheckExecutionState) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));
  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(Runtime_GetFrameCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));

  // Count all frames which are relevant to debugging stack trace.
  int n = 0;
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack frame count is 0.
    return Smi::FromInt(0);
  }

  for (StackTraceFrameIterator it(isolate, id); !it.done(); it.Advance()) {
    List<FrameSummary> frames(FLAG_max_inlining_levels + 1);
    if (it.is_wasm()) {
      n++;
    } else {
      it.javascript_frame()->Summarize(&frames);
      for (int i = frames.length() - 1; i >= 0; i--) {
        // Omit functions from native and extension scripts.
        if (frames[i].function()->shared()->IsSubjectToDebugging()) n++;
      }
    }
  }
  return Smi::FromInt(n);
}


static const int kFrameDetailsFrameIdIndex = 0;
static const int kFrameDetailsReceiverIndex = 1;
static const int kFrameDetailsFunctionIndex = 2;
static const int kFrameDetailsScriptIndex = 3;
static const int kFrameDetailsArgumentCountIndex = 4;
static const int kFrameDetailsLocalCountIndex = 5;
static const int kFrameDetailsSourcePositionIndex = 6;
static const int kFrameDetailsConstructCallIndex = 7;
static const int kFrameDetailsAtReturnIndex = 8;
static const int kFrameDetailsFlagsIndex = 9;
static const int kFrameDetailsFirstDynamicIndex = 10;

// Return an array with frame details
// args[0]: number: break id
// args[1]: number: frame index
//
// The array returned contains the following information:
// 0: Frame id
// 1: Receiver
// 2: Function
// 3: Script
// 4: Argument count
// 5: Local count
// 6: Source position
// 7: Constructor call
// 8: Is at return
// 9: Flags
// Arguments name, value
// Locals name, value
// Return value if any
RUNTIME_FUNCTION(Runtime_GetFrameDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  Heap* heap = isolate->heap();

  // Find the relevant frame with the requested index.
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return heap->undefined_value();
  }

  StackTraceFrameIterator it(isolate, id);
  // Inlined frame index in optimized frame, starting from outer function.
  int inlined_jsframe_index =
      DebugFrameHelper::FindIndexedNonNativeFrame(&it, index);
  if (inlined_jsframe_index == -1) return heap->undefined_value();

  FrameInspector frame_inspector(it.frame(), inlined_jsframe_index, isolate);

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save =
      DebugFrameHelper::FindSavedContextForFrame(isolate, it.frame());

  // Get the frame id.
  Handle<Object> frame_id(DebugFrameHelper::WrapFrameId(it.frame()->id()),
                          isolate);

  // Find source position in unoptimized code.
  int position = frame_inspector.GetSourcePosition();

  if (it.is_wasm()) {
    // Create the details array (no dynamic information for wasm).
    Handle<FixedArray> details =
        isolate->factory()->NewFixedArray(kFrameDetailsFirstDynamicIndex);

    // Add the frame id.
    details->set(kFrameDetailsFrameIdIndex, *frame_id);

    // Add the function name.
    Handle<Object> wasm_obj(it.wasm_frame()->wasm_obj(), isolate);
    int func_index = it.wasm_frame()->function_index();
    Handle<String> func_name =
        wasm::GetWasmFunctionName(isolate, wasm_obj, func_index);
    details->set(kFrameDetailsFunctionIndex, *func_name);

    // Add the script wrapper
    Handle<Object> script_wrapper =
        Script::GetWrapper(frame_inspector.GetScript());
    details->set(kFrameDetailsScriptIndex, *script_wrapper);

    // Add the arguments count.
    details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(0));

    // Add the locals count
    details->set(kFrameDetailsLocalCountIndex, Smi::FromInt(0));

    // Add the source position.
    if (position != kNoSourcePosition) {
      details->set(kFrameDetailsSourcePositionIndex, Smi::FromInt(position));
    }

    // Add the constructor information.
    details->set(kFrameDetailsConstructCallIndex, heap->ToBoolean(false));

    // Add the at return information.
    details->set(kFrameDetailsAtReturnIndex, heap->ToBoolean(false));

    // Add flags to indicate information on whether this frame is
    //   bit 0: invoked in the debugger context.
    //   bit 1: optimized frame.
    //   bit 2: inlined in optimized frame
    int flags = 0;
    if (*save->context() == *isolate->debug()->debug_context()) {
      flags |= 1 << 0;
    }
    details->set(kFrameDetailsFlagsIndex, Smi::FromInt(flags));

    return *isolate->factory()->NewJSArrayWithElements(details);
  }

  // Handle JavaScript frames.
  bool is_optimized = it.frame()->is_optimized();

  // Check for constructor frame.
  bool constructor = frame_inspector.IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function =
      Handle<JSFunction>::cast(frame_inspector.GetFunction());
  CHECK(function->shared()->IsSubjectToDebugging());
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  DCHECK(*scope_info != ScopeInfo::Empty(isolate));

  // Get the locals names and values into a temporary array.
  Handle<Object> maybe_context = frame_inspector.GetContext();
  const int local_count_with_synthetic = maybe_context->IsContext()
                                             ? scope_info->LocalCount()
                                             : scope_info->StackLocalCount();
  int local_count = local_count_with_synthetic;
  for (int slot = 0; slot < local_count_with_synthetic; ++slot) {
    // Hide compiler-introduced temporary variables, whether on the stack or on
    // the context.
    if (ScopeInfo::VariableIsSynthetic(scope_info->LocalName(slot))) {
      local_count--;
    }
  }

  List<Handle<Object>> locals;
  // Fill in the values of the locals.
  int i = 0;
  for (; i < scope_info->StackLocalCount(); ++i) {
    // Use the value from the stack.
    if (ScopeInfo::VariableIsSynthetic(scope_info->LocalName(i))) continue;
    locals.Add(Handle<String>(scope_info->LocalName(i), isolate));
    Handle<Object> value =
        frame_inspector.GetExpression(scope_info->StackLocalIndex(i));
    // TODO(yangguo): We convert optimized out values to {undefined} when they
    // are passed to the debugger. Eventually we should handle them somehow.
    if (value->IsOptimizedOut(isolate)) {
      value = isolate->factory()->undefined_value();
    }
    locals.Add(value);
  }
  if (locals.length() < local_count * 2) {
    // Get the context containing declarations.
    DCHECK(maybe_context->IsContext());
    Handle<Context> context(Context::cast(*maybe_context)->closure_context());

    for (; i < scope_info->LocalCount(); ++i) {
      Handle<String> name(scope_info->LocalName(i));
      if (ScopeInfo::VariableIsSynthetic(*name)) continue;
      VariableMode mode;
      InitializationFlag init_flag;
      MaybeAssignedFlag maybe_assigned_flag;
      locals.Add(name);
      int context_slot_index = ScopeInfo::ContextSlotIndex(
          scope_info, name, &mode, &init_flag, &maybe_assigned_flag);
      Object* value = context->get(context_slot_index);
      locals.Add(Handle<Object>(value, isolate));
    }
  }

  // Check whether this frame is positioned at return. If not top
  // frame or if the frame is optimized it cannot be at a return.
  bool at_return = false;
  if (!is_optimized && index == 0) {
    at_return = isolate->debug()->IsBreakAtReturn(it.javascript_frame());
  }

  // If positioned just before return find the value to be returned and add it
  // to the frame information.
  Handle<Object> return_value = isolate->factory()->undefined_value();
  if (at_return) {
    return_value = isolate->debug()->return_value();
  }

  // Now advance to the arguments adapter frame (if any). It contains all
  // the provided parameters whereas the function frame always have the number
  // of arguments matching the functions parameters. The rest of the
  // information (except for what is collected above) is the same.
  if ((inlined_jsframe_index == 0) &&
      it.javascript_frame()->has_adapted_arguments()) {
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
  details->set(kFrameDetailsFunctionIndex, *(frame_inspector.GetFunction()));

  // Add the script wrapper
  Handle<Object> script_wrapper =
      Script::GetWrapper(frame_inspector.GetScript());
  details->set(kFrameDetailsScriptIndex, *script_wrapper);

  // Add the arguments count.
  details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(argument_count));

  // Add the locals count
  details->set(kFrameDetailsLocalCountIndex, Smi::FromInt(local_count));

  // Add the source position.
  if (position != kNoSourcePosition) {
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
      details->set(details_index++, *(frame_inspector.GetParameter(i)));
    } else {
      details->set(details_index++, heap->undefined_value());
    }
  }

  // Add locals name and value from the temporary copy from the function frame.
  for (const auto& local : locals) details->set(details_index++, *local);

  // Add the value being returned.
  if (at_return) {
    details->set(details_index++, *return_value);
  }

  // Add the receiver (same as in function frame).
  Handle<Object> receiver(it.frame()->receiver(), isolate);
  DCHECK(!function->shared()->IsBuiltin());
  DCHECK_IMPLIES(is_sloppy(shared->language_mode()), receiver->IsJSReceiver());
  details->set(kFrameDetailsReceiverIndex, *receiver);

  DCHECK_EQ(details_size, details_index);
  return *isolate->factory()->NewJSArrayWithElements(details);
}


RUNTIME_FUNCTION(Runtime_GetScopeCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));

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
  CHECK(isolate->debug()->CheckExecutionState(break_id));

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
  RETURN_RESULT_OR_FAILURE(isolate, it.MaterializeScopeDetails());
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
  CHECK(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

  ScopeIterator::Option option = ScopeIterator::DEFAULT;
  if (args.length() == 4) {
    CONVERT_BOOLEAN_ARG_CHECKED(flag, 3);
    if (flag) option = ScopeIterator::IGNORE_NESTED_SCOPES;
  }

  // Get the frame where the debugging is performed.
  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
  StackTraceFrameIterator frame_it(isolate, id);
  StandardFrame* frame = frame_it.frame();
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

  RETURN_RESULT_OR_FAILURE(isolate, it.MaterializeScopeDetails());
}

RUNTIME_FUNCTION(Runtime_GetGeneratorScopeCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  if (!args[0]->IsJSGeneratorObject()) return Smi::FromInt(0);

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, gen); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}

RUNTIME_FUNCTION(Runtime_GetGeneratorScopeDetails) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  if (!args[0]->IsJSGeneratorObject()) {
    return *isolate->factory()->undefined_value();
  }

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, gen);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }

  RETURN_RESULT_OR_FAILURE(isolate, it.MaterializeScopeDetails());
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
    CHECK(isolate->debug()->CheckExecutionState(break_id));

    CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
    CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

    // Get the frame where the debugging is performed.
    StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);
    JavaScriptFrameIterator frame_it(isolate, id);
    JavaScriptFrame* frame = frame_it.frame();
    FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);

    ScopeIterator it(isolate, &frame_inspector);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  } else if (args[0]->IsJSFunction()) {
    CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
    ScopeIterator it(isolate, fun);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  } else {
    CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);
    ScopeIterator it(isolate, gen);
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
  CHECK(isolate->debug()->is_active());
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
  if (break_locations->IsUndefined(isolate)) {
    return isolate->heap()->undefined_value();
  }
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
  CHECK(isolate->debug()->is_active());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  CHECK(source_position >= function->shared()->start_position() &&
        source_position <= function->shared()->end_position());
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 2);

  // Set break point.
  CHECK(isolate->debug()->SetBreakPoint(function, break_point_object_arg,
                                        &source_position));

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
  CHECK(isolate->debug()->is_active());
  CONVERT_ARG_HANDLE_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  CHECK(source_position >= 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 3);

  if (!IsPositionAlignmentCodeCorrect(statement_aligned_code)) {
    return isolate->ThrowIllegalOperation();
  }
  BreakPositionAlignment alignment =
      static_cast<BreakPositionAlignment>(statement_aligned_code);

  // Get the script from the script wrapper.
  CHECK(wrapper->value()->IsScript());
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
  CHECK(isolate->debug()->is_active());
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
  CHECK(isolate->debug()->CheckExecutionState(break_id));

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
  CHECK(isolate->debug()->is_active());
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugEvaluate) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  DCHECK(args.length() == 6);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 4);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, context_extension, 5);

  StackFrame::Id id = DebugFrameHelper::UnwrapFrameId(wrapped_id);

  RETURN_RESULT_OR_FAILURE(
      isolate, DebugEvaluate::Local(isolate, id, inlined_jsframe_index, source,
                                    disable_break, context_extension));
}


RUNTIME_FUNCTION(Runtime_DebugEvaluateGlobal) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  DCHECK(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  CHECK(isolate->debug()->CheckExecutionState(break_id));

  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 2);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, context_extension, 3);

  RETURN_RESULT_OR_FAILURE(
      isolate,
      DebugEvaluate::Global(isolate, source, disable_break, context_extension));
}


RUNTIME_FUNCTION(Runtime_DebugGetLoadedScripts) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);

  // This runtime function is used by the debugger to determine whether the
  // debugger is active or not. Hence we fail gracefully here and don't crash.
  if (!isolate->debug()->is_active()) return isolate->ThrowIllegalOperation();

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
  return *isolate->factory()->NewJSArrayWithElements(instances);
}

static bool HasInPrototypeChainIgnoringProxies(Isolate* isolate,
                                               JSObject* object,
                                               Object* proto) {
  PrototypeIterator iter(isolate, object, kStartAtReceiver);
  while (true) {
    iter.AdvanceIgnoringProxies();
    if (iter.IsAtEnd()) return false;
    if (iter.GetCurrent() == proto) return true;
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
  CHECK(filter->IsUndefined(isolate) || filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  CHECK(max_references >= 0);

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
      if (!filter->IsUndefined(isolate) &&
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
  CHECK(max_references >= 0);

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
  // TODO(1543): Come up with a solution for clients to handle potential errors
  // thrown by an intermediate proxy.
  RETURN_RESULT_OR_FAILURE(isolate, JSReceiver::GetPrototype(isolate, obj));
}


// Patches script source (should be called upon BeforeCompile event).
RUNTIME_FUNCTION(Runtime_DebugSetScriptSource) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, script_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  CHECK(script_wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(script_wrapper->value()));

  // The following condition is not guaranteed to hold and a failure is also
  // propagated to callers. Hence we fail gracefully here and don't crash.
  if (script->compilation_state() != Script::COMPILATION_STATE_INITIAL) {
    return isolate->ThrowIllegalOperation();
  }

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
    RETURN_RESULT_OR_FAILURE(
        isolate, JSBoundFunction::GetName(
                     isolate, Handle<JSBoundFunction>::cast(function)));
  } else {
    return *JSFunction::GetDebugName(Handle<JSFunction>::cast(function));
  }
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

  RETURN_RESULT_OR_FAILURE(
      isolate, Execution::Call(isolate, function,
                               handle(function->global_proxy()), 0, NULL));
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
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                     GarbageCollectionReason::kRuntime);
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

RUNTIME_FUNCTION(Runtime_ScriptLineCount) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(JSValue, script, 0);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  Script::InitLineEnds(script_handle);

  FixedArray* line_ends_array = FixedArray::cast(script_handle->line_ends());
  return Smi::FromInt(line_ends_array->length());
}

RUNTIME_FUNCTION(Runtime_ScriptLineStartPosition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_NUMBER_CHECKED(int32_t, line, Int32, args[1]);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  Script::InitLineEnds(script_handle);

  FixedArray* line_ends_array = FixedArray::cast(script_handle->line_ends());
  const int line_count = line_ends_array->length();

  // If line == line_count, we return the first position beyond the last line.
  if (line < 0 || line > line_count) {
    return Smi::FromInt(-1);
  } else if (line == 0) {
    return Smi::FromInt(0);
  } else {
    DCHECK(0 < line && line <= line_count);
    const int pos = Smi::cast(line_ends_array->get(line - 1))->value() + 1;
    return Smi::FromInt(pos);
  }
}

RUNTIME_FUNCTION(Runtime_ScriptLineEndPosition) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_NUMBER_CHECKED(int32_t, line, Int32, args[1]);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  Script::InitLineEnds(script_handle);

  FixedArray* line_ends_array = FixedArray::cast(script_handle->line_ends());
  const int line_count = line_ends_array->length();

  if (line < 0 || line >= line_count) {
    return Smi::FromInt(-1);
  } else {
    return Smi::cast(line_ends_array->get(line));
  }
}

static Handle<Object> GetJSPositionInfo(Handle<Script> script, int position,
                                        Script::OffsetFlag offset_flag,
                                        Isolate* isolate) {
  Script::PositionInfo info;
  if (!script->GetPositionInfo(position, &info, offset_flag)) {
    return isolate->factory()->null_value();
  }

  Handle<String> source = handle(String::cast(script->source()), isolate);
  Handle<String> sourceText = script->type() == Script::TYPE_WASM
                                  ? isolate->factory()->empty_string()
                                  : isolate->factory()->NewSubString(
                                        source, info.line_start, info.line_end);

  Handle<JSObject> jsinfo =
      isolate->factory()->NewJSObject(isolate->object_function());

  JSObject::AddProperty(jsinfo, isolate->factory()->script_string(), script,
                        NONE);
  JSObject::AddProperty(jsinfo, isolate->factory()->position_string(),
                        handle(Smi::FromInt(position), isolate), NONE);
  JSObject::AddProperty(jsinfo, isolate->factory()->line_string(),
                        handle(Smi::FromInt(info.line), isolate), NONE);
  JSObject::AddProperty(jsinfo, isolate->factory()->column_string(),
                        handle(Smi::FromInt(info.column), isolate), NONE);
  JSObject::AddProperty(jsinfo, isolate->factory()->sourceText_string(),
                        sourceText, NONE);

  return jsinfo;
}

// Get information on a specific source line and column possibly offset by a
// fixed source position. This function is used to find a source position from
// a line and column position. The fixed source position offset is typically
// used to find a source position in a function based on a line and column in
// the source for the function alone. The offset passed will then be the
// start position of the source for the function within the full script source.
// Note that incoming line and column parameters may be undefined, and are
// assumed to be passed *with* offsets.
RUNTIME_FUNCTION(Runtime_ScriptLocationFromLine) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_CHECKED(JSValue, script, 0);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  // Line and column are possibly undefined and we need to handle these cases,
  // additionally subtracting corresponding offsets.

  int32_t line;
  if (args[1]->IsNull(isolate) || args[1]->IsUndefined(isolate)) {
    line = 0;
  } else {
    CHECK(args[1]->IsNumber());
    line = NumberToInt32(args[1]) - script_handle->line_offset();
  }

  int32_t column;
  if (args[2]->IsNull(isolate) || args[2]->IsUndefined(isolate)) {
    column = 0;
  } else {
    CHECK(args[2]->IsNumber());
    column = NumberToInt32(args[2]);
    if (line == 0) column -= script_handle->column_offset();
  }

  CONVERT_NUMBER_CHECKED(int32_t, offset_position, Int32, args[3]);

  if (line < 0 || column < 0 || offset_position < 0) {
    return isolate->heap()->null_value();
  }

  Script::InitLineEnds(script_handle);

  FixedArray* line_ends_array = FixedArray::cast(script_handle->line_ends());
  const int line_count = line_ends_array->length();

  int position;
  if (line == 0) {
    position = offset_position + column;
  } else {
    Script::PositionInfo info;
    if (!script_handle->GetPositionInfo(offset_position, &info,
                                        Script::NO_OFFSET) ||
        info.line + line >= line_count) {
      return isolate->heap()->null_value();
    }

    const int offset_line = info.line + line;
    const int offset_line_position =
        (offset_line == 0)
            ? 0
            : Smi::cast(line_ends_array->get(offset_line - 1))->value() + 1;
    position = offset_line_position + column;
  }

  return *GetJSPositionInfo(script_handle, position, Script::NO_OFFSET,
                            isolate);
}

RUNTIME_FUNCTION(Runtime_ScriptPositionInfo) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_NUMBER_CHECKED(int32_t, position, Int32, args[1]);
  CONVERT_BOOLEAN_ARG_CHECKED(with_offset, 2);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  const Script::OffsetFlag offset_flag =
      with_offset ? Script::WITH_OFFSET : Script::NO_OFFSET;
  return *GetJSPositionInfo(script_handle, position, offset_flag, isolate);
}

// Returns the given line as a string, or null if line is out of bounds.
// The parameter line is expected to include the script's line offset.
RUNTIME_FUNCTION(Runtime_ScriptSourceLine) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_NUMBER_CHECKED(int32_t, line, Int32, args[1]);

  CHECK(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  Script::InitLineEnds(script_handle);

  FixedArray* line_ends_array = FixedArray::cast(script_handle->line_ends());
  const int line_count = line_ends_array->length();

  line -= script_handle->line_offset();
  if (line < 0 || line_count <= line) {
    return isolate->heap()->null_value();
  }

  const int start =
      (line == 0) ? 0 : Smi::cast(line_ends_array->get(line - 1))->value() + 1;
  const int end = Smi::cast(line_ends_array->get(line))->value();

  Handle<String> source =
      handle(String::cast(script_handle->source()), isolate);
  Handle<String> str = isolate->factory()->NewSubString(source, start, end);

  return *str;
}

// Set one shot breakpoints for the callback function that is passed to a
// built-in function such as Array.forEach to enable stepping into the callback,
// if we are indeed stepping and the callback is subject to debugging.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInIfStepping) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  isolate->debug()->PrepareStepIn(fun);
  return isolate->heap()->undefined_value();
}

// Set one shot breakpoints for the suspended generator object.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInSuspendedGenerator) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->debug()->PrepareStepInSuspendedGenerator();
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugRecordAsyncFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);
  CHECK(isolate->debug()->last_step_action() >= StepNext);
  isolate->debug()->RecordAsyncFunction(generator);
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugPushPromise) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  isolate->PushPromise(promise);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPopPromise) {
  DCHECK(args.length() == 0);
  SealHandleScope shs(isolate);
  isolate->PopPromise();
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

RUNTIME_FUNCTION(Runtime_GetWasmFunctionOffsetTable) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSValue, script_val, 0);

  CHECK(script_val->value()->IsScript());
  Handle<Script> script = Handle<Script>(Script::cast(script_val->value()));

  Handle<wasm::WasmDebugInfo> debug_info =
      wasm::GetDebugInfo(handle(script->wasm_object(), isolate));
  Handle<FixedArray> elements = wasm::WasmDebugInfo::GetFunctionOffsetTable(
      debug_info, script->wasm_function_index());
  return *isolate->factory()->NewJSArrayWithElements(elements);
}

RUNTIME_FUNCTION(Runtime_DisassembleWasmFunction) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_CHECKED(JSValue, script_val, 0);

  CHECK(script_val->value()->IsScript());
  Handle<Script> script = Handle<Script>(Script::cast(script_val->value()));

  Handle<wasm::WasmDebugInfo> debug_info =
      wasm::GetDebugInfo(handle(script->wasm_object(), isolate));
  return *wasm::WasmDebugInfo::DisassembleFunction(
      debug_info, script->wasm_function_index());
}

}  // namespace internal
}  // namespace v8
