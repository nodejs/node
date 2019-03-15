// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/arguments-inl.h"
#include "src/compiler.h"
#include "src/counters.h"
#include "src/debug/debug-coverage.h"
#include "src/debug/debug-evaluate.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/heap/heap-inl.h"  // For ToBoolean. TODO(jkummerow): Drop.
#include "src/interpreter/bytecode-array-accessor.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/js-promise-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION_RETURN_PAIR(Runtime_DebugBreakOnBytecode) {
  using interpreter::Bytecode;
  using interpreter::Bytecodes;
  using interpreter::OperandScale;

  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  HandleScope scope(isolate);

  // Return value can be changed by debugger. Last set value will be used as
  // return value.
  ReturnValueScope result_scope(isolate->debug());
  isolate->debug()->set_return_value(*value);

  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  if (isolate->debug_execution_mode() == DebugInfo::kBreakpoints) {
    isolate->debug()->Break(it.frame(),
                            handle(it.frame()->function(), isolate));
  }

  // If we are dropping frames, there is no need to get a return value or
  // bytecode, since we will be restarting execution at a different frame.
  if (isolate->debug()->will_restart()) {
    return MakePair(ReadOnlyRoots(isolate).undefined_value(),
                    Smi::FromInt(static_cast<uint8_t>(Bytecode::kIllegal)));
  }

  // Return the handler from the original bytecode array.
  DCHECK(it.frame()->is_interpreted());
  InterpretedFrame* interpreted_frame =
      reinterpret_cast<InterpretedFrame*>(it.frame());
  SharedFunctionInfo shared = interpreted_frame->function()->shared();
  BytecodeArray bytecode_array = shared->GetBytecodeArray();
  int bytecode_offset = interpreted_frame->GetBytecodeOffset();
  Bytecode bytecode = Bytecodes::FromByte(bytecode_array->get(bytecode_offset));

  bool side_effect_check_failed = false;
  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects) {
    side_effect_check_failed =
        !isolate->debug()->PerformSideEffectCheckAtBytecode(interpreted_frame);
  }

  if (Bytecodes::Returns(bytecode)) {
    // If we are returning (or suspending), reset the bytecode array on the
    // interpreted stack frame to the non-debug variant so that the interpreter
    // entry trampoline sees the return/suspend bytecode rather than the
    // DebugBreak.
    interpreted_frame->PatchBytecodeArray(bytecode_array);
  }

  // We do not have to deal with operand scale here. If the bytecode at the
  // break is prefixed by operand scaling, we would have patched over the
  // scaling prefix. We now simply dispatch to the handler for the prefix.
  // We need to deserialize now to ensure we don't hit the debug break again
  // after deserializing.
  OperandScale operand_scale = OperandScale::kSingle;
  isolate->interpreter()->GetBytecodeHandler(bytecode, operand_scale);

  if (side_effect_check_failed) {
    return MakePair(ReadOnlyRoots(isolate).exception(),
                    Smi::FromInt(static_cast<uint8_t>(bytecode)));
  }
  Object interrupt_object = isolate->stack_guard()->HandleInterrupts();
  if (interrupt_object->IsException(isolate)) {
    return MakePair(interrupt_object,
                    Smi::FromInt(static_cast<uint8_t>(bytecode)));
  }
  return MakePair(isolate->debug()->return_value(),
                  Smi::FromInt(static_cast<uint8_t>(bytecode)));
}

RUNTIME_FUNCTION(Runtime_DebugBreakAtEntry) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  USE(function);

  DCHECK(function->shared()->HasDebugInfo());
  DCHECK(function->shared()->GetDebugInfo()->BreakAtEntry());

  // Get the top-most JavaScript frame.
  JavaScriptFrameIterator it(isolate);
  DCHECK_EQ(*function, it.frame()->function());
  isolate->debug()->Break(it.frame(), function);

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_HandleDebuggerStatement) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  if (isolate->debug()->break_points_active()) {
    isolate->debug()->HandleDebugBreak(kIgnoreIfTopFrameBlackboxed);
  }
  return isolate->stack_guard()->HandleInterrupts();
}

RUNTIME_FUNCTION(Runtime_ScheduleBreak) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  isolate->RequestInterrupt(
      [](v8::Isolate* isolate, void*) { v8::debug::BreakRightNow(isolate); },
      nullptr);
  return ReadOnlyRoots(isolate).undefined_value();
}

template <class IteratorType>
static MaybeHandle<JSArray> GetIteratorInternalProperties(
    Isolate* isolate, Handle<IteratorType> object) {
  Factory* factory = isolate->factory();
  Handle<IteratorType> iterator = Handle<IteratorType>::cast(object);
  const char* kind = nullptr;
  switch (iterator->map()->instance_type()) {
    case JS_MAP_KEY_ITERATOR_TYPE:
      kind = "keys";
      break;
    case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
    case JS_SET_KEY_VALUE_ITERATOR_TYPE:
      kind = "entries";
      break;
    case JS_MAP_VALUE_ITERATOR_TYPE:
    case JS_SET_VALUE_ITERATOR_TYPE:
      kind = "values";
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
    Handle<JSPromise> promise = Handle<JSPromise>::cast(object);
    const char* status = JSPromise::Status(promise->status());
    Handle<FixedArray> result = factory->NewFixedArray(2 * 2);
    Handle<String> promise_status =
        factory->NewStringFromAsciiChecked("[[PromiseStatus]]");
    result->set(0, *promise_status);
    Handle<String> status_str = factory->NewStringFromAsciiChecked(status);
    result->set(1, *status_str);

    Handle<Object> value_obj(promise->status() == Promise::kPending
                                 ? ReadOnlyRoots(isolate).undefined_value()
                                 : promise->result(),
                             isolate);
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

RUNTIME_FUNCTION(Runtime_GetGeneratorScopeCount) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());

  if (!args[0]->IsJSGeneratorObject()) return Smi::kZero;

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);

  // Only inspect suspended generator scopes.
  if (!gen->is_suspended()) {
    return Smi::kZero;
  }

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, gen); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}

RUNTIME_FUNCTION(Runtime_GetGeneratorScopeDetails) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  if (!args[0]->IsJSGeneratorObject()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Only inspect suspended generator scopes.
  if (!gen->is_suspended()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, gen);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  return *it.MaterializeScopeDetails();
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
// args[1]: number: scope index
// args[2]: string: variable name
// args[3]: object: new value
//
// Return true if success and false otherwise
RUNTIME_FUNCTION(Runtime_SetGeneratorScopeVariableValue) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, gen, 0);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  CONVERT_ARG_HANDLE_CHECKED(String, variable_name, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_value, 3);
  ScopeIterator it(isolate, gen);
  bool res = SetScopeVariableValue(&it, index, variable_name, new_value);
  return isolate->heap()->ToBoolean(res);
}


RUNTIME_FUNCTION(Runtime_GetBreakLocations) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CHECK(isolate->debug()->is_active());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);

  Handle<SharedFunctionInfo> shared(fun->shared(), isolate);
  // Find the number of break points
  Handle<Object> break_locations =
      Debug::GetSourceBreakLocations(isolate, shared);
  if (break_locations->IsUndefined(isolate)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }
  // Return array as JS array
  return *isolate->factory()->NewJSArrayWithElements(
      Handle<FixedArray>::cast(break_locations));
}


// Returns the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
RUNTIME_FUNCTION(Runtime_IsBreakOnException) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_NUMBER_CHECKED(uint32_t, type_arg, Uint32, args[0]);

  ExceptionBreakType type = static_cast<ExceptionBreakType>(type_arg);
  bool result = isolate->debug()->IsBreakOnException(type);
  return Smi::FromInt(result);
}

// Clear all stepping set by PrepareStep.
RUNTIME_FUNCTION(Runtime_ClearStepping) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  CHECK(isolate->debug()->is_active());
  isolate->debug()->ClearStepping();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugGetLoadedScriptIds) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<FixedArray> instances;
  {
    DebugScope debug_scope(isolate->debug());
    // Fill the script objects.
    instances = isolate->debug()->GetLoadedScripts();
  }

  // Convert the script objects to proper JS objects.
  for (int i = 0; i < instances->length(); i++) {
    Handle<Script> script(Script::cast(instances->get(i)), isolate);
    instances->set(i, Smi::FromInt(script->id()));
  }

  // Return result as a JS array.
  return *isolate->factory()->NewJSArrayWithElements(instances);
}


RUNTIME_FUNCTION(Runtime_FunctionGetInferredName) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());

  CONVERT_ARG_CHECKED(Object, f, 0);
  if (f->IsJSFunction()) {
    return JSFunction::cast(f)->shared()->inferred_name();
  }
  return ReadOnlyRoots(isolate).empty_string();
}


// Performs a GC.
// Presently, it only does a full GC.
RUNTIME_FUNCTION(Runtime_CollectGarbage) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  isolate->heap()->PreciseCollectAllGarbage(Heap::kNoGCFlags,
                                            GarbageCollectionReason::kRuntime);
  return ReadOnlyRoots(isolate).undefined_value();
}


// Gets the current heap usage.
RUNTIME_FUNCTION(Runtime_GetHeapUsage) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  int usage = static_cast<int>(isolate->heap()->SizeOfObjects());
  if (!Smi::IsValid(usage)) {
    return *isolate->factory()->NewNumberFromInt(usage);
  }
  return Smi::FromInt(usage);
}

namespace {

int ScriptLinePosition(Handle<Script> script, int line) {
  if (line < 0) return -1;

  if (script->type() == Script::TYPE_WASM) {
    return WasmModuleObject::cast(script->wasm_module_object())
        ->GetFunctionOffset(line);
  }

  Script::InitLineEnds(script);

  FixedArray line_ends_array = FixedArray::cast(script->line_ends());
  const int line_count = line_ends_array->length();
  DCHECK_LT(0, line_count);

  if (line == 0) return 0;
  // If line == line_count, we return the first position beyond the last line.
  if (line > line_count) return -1;
  return Smi::ToInt(line_ends_array->get(line - 1)) + 1;
}

int ScriptLinePositionWithOffset(Handle<Script> script, int line, int offset) {
  if (line < 0 || offset < 0) return -1;

  if (line == 0 || offset == 0)
    return ScriptLinePosition(script, line) + offset;

  Script::PositionInfo info;
  if (!Script::GetPositionInfo(script, offset, &info, Script::NO_OFFSET)) {
    return -1;
  }

  const int total_line = info.line + line;
  return ScriptLinePosition(script, total_line);
}

Handle<Object> GetJSPositionInfo(Handle<Script> script, int position,
                                 Script::OffsetFlag offset_flag,
                                 Isolate* isolate) {
  Script::PositionInfo info;
  if (!Script::GetPositionInfo(script, position, &info, offset_flag)) {
    return isolate->factory()->null_value();
  }

  Handle<String> source = handle(String::cast(script->source()), isolate);
  Handle<String> sourceText = script->type() == Script::TYPE_WASM
                                  ? isolate->factory()->empty_string()
                                  : isolate->factory()->NewSubString(
                                        source, info.line_start, info.line_end);

  Handle<JSObject> jsinfo =
      isolate->factory()->NewJSObject(isolate->object_function());

  JSObject::AddProperty(isolate, jsinfo, isolate->factory()->script_string(),
                        script, NONE);
  JSObject::AddProperty(isolate, jsinfo, isolate->factory()->position_string(),
                        handle(Smi::FromInt(position), isolate), NONE);
  JSObject::AddProperty(isolate, jsinfo, isolate->factory()->line_string(),
                        handle(Smi::FromInt(info.line), isolate), NONE);
  JSObject::AddProperty(isolate, jsinfo, isolate->factory()->column_string(),
                        handle(Smi::FromInt(info.column), isolate), NONE);
  JSObject::AddProperty(isolate, jsinfo,
                        isolate->factory()->sourceText_string(), sourceText,
                        NONE);

  return jsinfo;
}

Handle<Object> ScriptLocationFromLine(Isolate* isolate, Handle<Script> script,
                                      Handle<Object> opt_line,
                                      Handle<Object> opt_column,
                                      int32_t offset) {
  // Line and column are possibly undefined and we need to handle these cases,
  // additionally subtracting corresponding offsets.

  int32_t line = 0;
  if (!opt_line->IsNullOrUndefined(isolate)) {
    CHECK(opt_line->IsNumber());
    line = NumberToInt32(*opt_line) - script->line_offset();
  }

  int32_t column = 0;
  if (!opt_column->IsNullOrUndefined(isolate)) {
    CHECK(opt_column->IsNumber());
    column = NumberToInt32(*opt_column);
    if (line == 0) column -= script->column_offset();
  }

  int line_position = ScriptLinePositionWithOffset(script, line, offset);
  if (line_position < 0 || column < 0) return isolate->factory()->null_value();

  return GetJSPositionInfo(script, line_position + column, Script::NO_OFFSET,
                           isolate);
}

// Slow traversal over all scripts on the heap.
bool GetScriptById(Isolate* isolate, int needle, Handle<Script>* result) {
  Script::Iterator iterator(isolate);
  for (Script script = iterator.Next(); !script.is_null();
       script = iterator.Next()) {
    if (script->id() == needle) {
      *result = handle(script, isolate);
      return true;
    }
  }

  return false;
}

}  // namespace

// TODO(5530): Rename once conflicting function has been deleted.
RUNTIME_FUNCTION(Runtime_ScriptLocationFromLine2) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_NUMBER_CHECKED(int32_t, scriptid, Int32, args[0]);
  CONVERT_ARG_HANDLE_CHECKED(Object, opt_line, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, opt_column, 2);
  CONVERT_NUMBER_CHECKED(int32_t, offset, Int32, args[3]);

  Handle<Script> script;
  CHECK(GetScriptById(isolate, scriptid, &script));

  return *ScriptLocationFromLine(isolate, script, opt_line, opt_column, offset);
}

// On function call, depending on circumstances, prepare for stepping in,
// or perform a side effect check.
RUNTIME_FUNCTION(Runtime_DebugOnFunctionCall) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  if (isolate->debug()->needs_check_on_function_call()) {
    // Ensure that the callee will perform debug check on function call too.
    Deoptimizer::DeoptimizeFunction(*fun);
    if (isolate->debug()->last_step_action() >= StepIn ||
        isolate->debug()->break_on_next_function_call()) {
      DCHECK_EQ(isolate->debug_execution_mode(), DebugInfo::kBreakpoints);
      isolate->debug()->PrepareStepIn(fun);
    }
    if (isolate->debug_execution_mode() == DebugInfo::kSideEffects &&
        !isolate->debug()->PerformSideEffectCheck(fun, receiver)) {
      return ReadOnlyRoots(isolate).exception();
    }
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

// Set one shot breakpoints for the suspended generator object.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInSuspendedGenerator) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  isolate->debug()->PrepareStepInSuspendedGenerator();
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugPushPromise) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, promise, 0);
  isolate->PushPromise(promise);
  return ReadOnlyRoots(isolate).undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPopPromise) {
  DCHECK_EQ(0, args.length());
  SealHandleScope shs(isolate);
  isolate->PopPromise();
  return ReadOnlyRoots(isolate).undefined_value();
}

namespace {
Handle<JSObject> MakeRangeObject(Isolate* isolate, const CoverageBlock& range) {
  Factory* factory = isolate->factory();

  Handle<String> start_string = factory->InternalizeUtf8String("start");
  Handle<String> end_string = factory->InternalizeUtf8String("end");
  Handle<String> count_string = factory->InternalizeUtf8String("count");

  Handle<JSObject> range_obj = factory->NewJSObjectWithNullProto();
  JSObject::AddProperty(isolate, range_obj, start_string,
                        factory->NewNumberFromInt(range.start), NONE);
  JSObject::AddProperty(isolate, range_obj, end_string,
                        factory->NewNumberFromInt(range.end), NONE);
  JSObject::AddProperty(isolate, range_obj, count_string,
                        factory->NewNumberFromUint(range.count), NONE);

  return range_obj;
}
}  // namespace

RUNTIME_FUNCTION(Runtime_DebugCollectCoverage) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  // Collect coverage data.
  std::unique_ptr<Coverage> coverage;
  if (isolate->is_best_effort_code_coverage()) {
    coverage = Coverage::CollectBestEffort(isolate);
  } else {
    coverage = Coverage::CollectPrecise(isolate);
  }
  Factory* factory = isolate->factory();
  // Turn the returned data structure into JavaScript.
  // Create an array of scripts.
  int num_scripts = static_cast<int>(coverage->size());
  // Prepare property keys.
  Handle<FixedArray> scripts_array = factory->NewFixedArray(num_scripts);
  Handle<String> script_string = factory->NewStringFromStaticChars("script");
  for (int i = 0; i < num_scripts; i++) {
    const auto& script_data = coverage->at(i);
    HandleScope inner_scope(isolate);

    std::vector<CoverageBlock> ranges;
    int num_functions = static_cast<int>(script_data.functions.size());
    for (int j = 0; j < num_functions; j++) {
      const auto& function_data = script_data.functions[j];
      ranges.emplace_back(function_data.start, function_data.end,
                          function_data.count);
      for (size_t k = 0; k < function_data.blocks.size(); k++) {
        const auto& block_data = function_data.blocks[k];
        ranges.emplace_back(block_data.start, block_data.end, block_data.count);
      }
    }

    int num_ranges = static_cast<int>(ranges.size());
    Handle<FixedArray> ranges_array = factory->NewFixedArray(num_ranges);
    for (int j = 0; j < num_ranges; j++) {
      Handle<JSObject> range_object = MakeRangeObject(isolate, ranges[j]);
      ranges_array->set(j, *range_object);
    }

    Handle<JSArray> script_obj =
        factory->NewJSArrayWithElements(ranges_array, PACKED_ELEMENTS);
    JSObject::AddProperty(isolate, script_obj, script_string,
                          handle(script_data.script->source(), isolate), NONE);
    scripts_array->set(i, *script_obj);
  }
  return *factory->NewJSArrayWithElements(scripts_array, PACKED_ELEMENTS);
}

RUNTIME_FUNCTION(Runtime_DebugTogglePreciseCoverage) {
  SealHandleScope shs(isolate);
  CONVERT_BOOLEAN_ARG_CHECKED(enable, 0);
  Coverage::SelectMode(isolate, enable ? debug::CoverageMode::kPreciseCount
                                       : debug::CoverageMode::kBestEffort);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugToggleBlockCoverage) {
  SealHandleScope shs(isolate);
  CONVERT_BOOLEAN_ARG_CHECKED(enable, 0);
  Coverage::SelectMode(isolate, enable ? debug::CoverageMode::kBlockCount
                                       : debug::CoverageMode::kBestEffort);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_IncBlockCounter) {
  SealHandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_SMI_ARG_CHECKED(coverage_array_slot_index, 1);

  // It's quite possible that a function contains IncBlockCounter bytecodes, but
  // no coverage info exists. This happens e.g. by selecting the best-effort
  // coverage collection mode, which triggers deletion of all coverage infos in
  // order to avoid memory leaks.

  SharedFunctionInfo shared = function->shared();
  if (shared->HasCoverageInfo()) {
    CoverageInfo coverage_info = shared->GetCoverageInfo();
    coverage_info->IncrementBlockCount(coverage_array_slot_index);
  }

  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugAsyncFunctionEntered) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  isolate->RunPromiseHook(PromiseHookType::kInit, promise,
                          isolate->factory()->undefined_value());
  if (isolate->debug()->is_active()) isolate->PushPromise(promise);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_DebugAsyncFunctionFinished) {
  DCHECK_EQ(2, args.length());
  HandleScope scope(isolate);
  CONVERT_BOOLEAN_ARG_CHECKED(has_suspend, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 1);
  isolate->PopPromise();
  if (has_suspend) {
    isolate->OnAsyncFunctionStateChanged(promise,
                                         debug::kAsyncFunctionFinished);
  }
  return *promise;
}

RUNTIME_FUNCTION(Runtime_DebugAsyncFunctionSuspended) {
  DCHECK_EQ(1, args.length());
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSPromise, promise, 0);
  isolate->OnAsyncFunctionStateChanged(promise, debug::kAsyncFunctionSuspended);
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_LiveEditPatchScript) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, script_function, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, new_source, 1);

  Handle<Script> script(Script::cast(script_function->shared()->script()),
                        isolate);
  v8::debug::LiveEditResult result;
  LiveEdit::PatchScript(isolate, script, new_source, false, &result);
  switch (result.status) {
    case v8::debug::LiveEditResult::COMPILE_ERROR:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: COMPILE_ERROR"));
    case v8::debug::LiveEditResult::BLOCKED_BY_RUNNING_GENERATOR:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: BLOCKED_BY_RUNNING_GENERATOR"));
    case v8::debug::LiveEditResult::BLOCKED_BY_FUNCTION_ABOVE_BREAK_FRAME:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: BLOCKED_BY_FUNCTION_ABOVE_BREAK_FRAME"));
    case v8::debug::LiveEditResult::
        BLOCKED_BY_FUNCTION_BELOW_NON_DROPPABLE_FRAME:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: BLOCKED_BY_FUNCTION_BELOW_NON_DROPPABLE_FRAME"));
    case v8::debug::LiveEditResult::BLOCKED_BY_ACTIVE_FUNCTION:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: BLOCKED_BY_ACTIVE_FUNCTION"));
    case v8::debug::LiveEditResult::BLOCKED_BY_NEW_TARGET_IN_RESTART_FRAME:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: BLOCKED_BY_NEW_TARGET_IN_RESTART_FRAME"));
    case v8::debug::LiveEditResult::FRAME_RESTART_IS_NOT_SUPPORTED:
      return isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
          "LiveEdit failed: FRAME_RESTART_IS_NOT_SUPPORTED"));
    case v8::debug::LiveEditResult::OK:
      return ReadOnlyRoots(isolate).undefined_value();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

RUNTIME_FUNCTION(Runtime_PerformSideEffectCheckForObject) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);

  DCHECK_EQ(isolate->debug_execution_mode(), DebugInfo::kSideEffects);
  if (!isolate->debug()->PerformSideEffectCheckForObject(object)) {
    DCHECK(isolate->has_pending_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
