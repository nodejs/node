// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"
#include "accessors.h"

#include "contexts.h"
#include "deoptimizer.h"
#include "execution.h"
#include "factory.h"
#include "frames-inl.h"
#include "isolate.h"
#include "list-inl.h"
#include "property-details.h"

namespace v8 {
namespace internal {


template <class C>
static C* FindInstanceOf(Isolate* isolate, Object* obj) {
  for (Object* cur = obj; !cur->IsNull(); cur = cur->GetPrototype(isolate)) {
    if (Is<C>(cur)) return C::cast(cur);
  }
  return NULL;
}


// Entry point that never should be called.
MaybeObject* Accessors::IllegalSetter(JSObject*, Object*, void*) {
  UNREACHABLE();
  return NULL;
}


Object* Accessors::IllegalGetAccessor(Object* object, void*) {
  UNREACHABLE();
  return object;
}


MaybeObject* Accessors::ReadOnlySetAccessor(JSObject*, Object* value, void*) {
  // According to ECMA-262, section 8.6.2.2, page 28, setting
  // read-only properties must be silently ignored.
  return value;
}


//
// Accessors::ArrayLength
//


MaybeObject* Accessors::ArrayGetLength(Object* object, void*) {
  // Traverse the prototype chain until we reach an array.
  JSArray* holder = FindInstanceOf<JSArray>(Isolate::Current(), object);
  return holder == NULL ? Smi::FromInt(0) : holder->length();
}


// The helper function will 'flatten' Number objects.
Object* Accessors::FlattenNumber(Object* value) {
  if (value->IsNumber() || !value->IsJSValue()) return value;
  JSValue* wrapper = JSValue::cast(value);
  ASSERT(Isolate::Current()->context()->native_context()->number_function()->
      has_initial_map());
  Map* number_map = Isolate::Current()->context()->native_context()->
      number_function()->initial_map();
  if (wrapper->map() == number_map) return wrapper->value();
  return value;
}


MaybeObject* Accessors::ArraySetLength(JSObject* object, Object* value, void*) {
  Isolate* isolate = object->GetIsolate();

  // This means one of the object's prototypes is a JSArray and the
  // object does not have a 'length' property.  Calling SetProperty
  // causes an infinite loop.
  if (!object->IsJSArray()) {
    return object->SetLocalPropertyIgnoreAttributes(
        isolate->heap()->length_string(), value, NONE);
  }

  value = FlattenNumber(value);

  // Need to call methods that may trigger GC.
  HandleScope scope(isolate);

  // Protect raw pointers.
  Handle<JSArray> array_handle(JSArray::cast(object), isolate);
  Handle<Object> value_handle(value, isolate);

  bool has_exception;
  Handle<Object> uint32_v = Execution::ToUint32(value_handle, &has_exception);
  if (has_exception) return Failure::Exception();
  Handle<Object> number_v = Execution::ToNumber(value_handle, &has_exception);
  if (has_exception) return Failure::Exception();

  if (uint32_v->Number() == number_v->Number()) {
    return array_handle->SetElementsLength(*uint32_v);
  }
  return isolate->Throw(
      *isolate->factory()->NewRangeError("invalid_array_length",
                                         HandleVector<Object>(NULL, 0)));
}


const AccessorDescriptor Accessors::ArrayLength = {
  ArrayGetLength,
  ArraySetLength,
  0
};


//
// Accessors::StringLength
//


MaybeObject* Accessors::StringGetLength(Object* object, void*) {
  Object* value = object;
  if (object->IsJSValue()) value = JSValue::cast(object)->value();
  if (value->IsString()) return Smi::FromInt(String::cast(value)->length());
  // If object is not a string we return 0 to be compatible with WebKit.
  // Note: Firefox returns the length of ToString(object).
  return Smi::FromInt(0);
}


const AccessorDescriptor Accessors::StringLength = {
  StringGetLength,
  IllegalSetter,
  0
};


//
// Accessors::ScriptSource
//


MaybeObject* Accessors::ScriptGetSource(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->source();
}


const AccessorDescriptor Accessors::ScriptSource = {
  ScriptGetSource,
  IllegalSetter,
  0
};


//
// Accessors::ScriptName
//


MaybeObject* Accessors::ScriptGetName(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->name();
}


const AccessorDescriptor Accessors::ScriptName = {
  ScriptGetName,
  IllegalSetter,
  0
};


//
// Accessors::ScriptId
//


MaybeObject* Accessors::ScriptGetId(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->id();
}


const AccessorDescriptor Accessors::ScriptId = {
  ScriptGetId,
  IllegalSetter,
  0
};


//
// Accessors::ScriptLineOffset
//


MaybeObject* Accessors::ScriptGetLineOffset(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->line_offset();
}


const AccessorDescriptor Accessors::ScriptLineOffset = {
  ScriptGetLineOffset,
  IllegalSetter,
  0
};


//
// Accessors::ScriptColumnOffset
//


MaybeObject* Accessors::ScriptGetColumnOffset(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->column_offset();
}


const AccessorDescriptor Accessors::ScriptColumnOffset = {
  ScriptGetColumnOffset,
  IllegalSetter,
  0
};


//
// Accessors::ScriptData
//


MaybeObject* Accessors::ScriptGetData(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->data();
}


const AccessorDescriptor Accessors::ScriptData = {
  ScriptGetData,
  IllegalSetter,
  0
};


//
// Accessors::ScriptType
//


MaybeObject* Accessors::ScriptGetType(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->type();
}


const AccessorDescriptor Accessors::ScriptType = {
  ScriptGetType,
  IllegalSetter,
  0
};


//
// Accessors::ScriptCompilationType
//


MaybeObject* Accessors::ScriptGetCompilationType(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->compilation_type();
}


const AccessorDescriptor Accessors::ScriptCompilationType = {
  ScriptGetCompilationType,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetLineEnds
//


MaybeObject* Accessors::ScriptGetLineEnds(Object* object, void*) {
  JSValue* wrapper = JSValue::cast(object);
  Isolate* isolate = wrapper->GetIsolate();
  HandleScope scope(isolate);
  Handle<Script> script(Script::cast(wrapper->value()), isolate);
  InitScriptLineEnds(script);
  ASSERT(script->line_ends()->IsFixedArray());
  Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
  // We do not want anyone to modify this array from JS.
  ASSERT(*line_ends == isolate->heap()->empty_fixed_array() ||
         line_ends->map() == isolate->heap()->fixed_cow_array_map());
  Handle<JSArray> js_array =
      isolate->factory()->NewJSArrayWithElements(line_ends);
  return *js_array;
}


const AccessorDescriptor Accessors::ScriptLineEnds = {
  ScriptGetLineEnds,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetContextData
//


MaybeObject* Accessors::ScriptGetContextData(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  return Script::cast(script)->context_data();
}


const AccessorDescriptor Accessors::ScriptContextData = {
  ScriptGetContextData,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetEvalFromScript
//


MaybeObject* Accessors::ScriptGetEvalFromScript(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  if (!Script::cast(script)->eval_from_shared()->IsUndefined()) {
    Handle<SharedFunctionInfo> eval_from_shared(
        SharedFunctionInfo::cast(Script::cast(script)->eval_from_shared()));

    if (eval_from_shared->script()->IsScript()) {
      Handle<Script> eval_from_script(Script::cast(eval_from_shared->script()));
      return *GetScriptWrapper(eval_from_script);
    }
  }
  return HEAP->undefined_value();
}


const AccessorDescriptor Accessors::ScriptEvalFromScript = {
  ScriptGetEvalFromScript,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetEvalFromScriptPosition
//


MaybeObject* Accessors::ScriptGetEvalFromScriptPosition(Object* object, void*) {
  Script* raw_script = Script::cast(JSValue::cast(object)->value());
  HandleScope scope(raw_script->GetIsolate());
  Handle<Script> script(raw_script);

  // If this is not a script compiled through eval there is no eval position.
  int compilation_type = Smi::cast(script->compilation_type())->value();
  if (compilation_type != Script::COMPILATION_TYPE_EVAL) {
    return script->GetHeap()->undefined_value();
  }

  // Get the function from where eval was called and find the source position
  // from the instruction offset.
  Handle<Code> code(SharedFunctionInfo::cast(
      script->eval_from_shared())->code());
  return Smi::FromInt(code->SourcePosition(code->instruction_start() +
                      script->eval_from_instructions_offset()->value()));
}


const AccessorDescriptor Accessors::ScriptEvalFromScriptPosition = {
  ScriptGetEvalFromScriptPosition,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetEvalFromFunctionName
//


MaybeObject* Accessors::ScriptGetEvalFromFunctionName(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(
      Script::cast(script)->eval_from_shared()));


  // Find the name of the function calling eval.
  if (!shared->name()->IsUndefined()) {
    return shared->name();
  } else {
    return shared->inferred_name();
  }
}


const AccessorDescriptor Accessors::ScriptEvalFromFunctionName = {
  ScriptGetEvalFromFunctionName,
  IllegalSetter,
  0
};


//
// Accessors::FunctionPrototype
//


Handle<Object> Accessors::FunctionGetPrototype(Handle<Object> object) {
  Isolate* isolate = Isolate::Current();
  CALL_HEAP_FUNCTION(
      isolate, Accessors::FunctionGetPrototype(*object, 0), Object);
}


MaybeObject* Accessors::FunctionGetPrototype(Object* object, void*) {
  Isolate* isolate = Isolate::Current();
  JSFunction* function = FindInstanceOf<JSFunction>(isolate, object);
  if (function == NULL) return isolate->heap()->undefined_value();
  while (!function->should_have_prototype()) {
    function = FindInstanceOf<JSFunction>(isolate, function->GetPrototype());
    // There has to be one because we hit the getter.
    ASSERT(function != NULL);
  }

  if (!function->has_prototype()) {
    Object* prototype;
    { MaybeObject* maybe_prototype
          = isolate->heap()->AllocateFunctionPrototype(function);
      if (!maybe_prototype->ToObject(&prototype)) return maybe_prototype;
    }
    Object* result;
    { MaybeObject* maybe_result = function->SetPrototype(prototype);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
  }
  return function->prototype();
}


MaybeObject* Accessors::FunctionSetPrototype(JSObject* object,
                                             Object* value_raw,
                                             void*) {
  Isolate* isolate = object->GetIsolate();
  Heap* heap = isolate->heap();
  JSFunction* function_raw = FindInstanceOf<JSFunction>(isolate, object);
  if (function_raw == NULL) return heap->undefined_value();
  if (!function_raw->should_have_prototype()) {
    // Since we hit this accessor, object will have no prototype property.
    return object->SetLocalPropertyIgnoreAttributes(heap->prototype_string(),
                                                    value_raw,
                                                    NONE);
  }

  HandleScope scope(isolate);
  Handle<JSFunction> function(function_raw, isolate);
  Handle<Object> value(value_raw, isolate);

  Handle<Object> old_value;
  bool is_observed =
      FLAG_harmony_observation &&
      *function == object &&
      function->map()->is_observed();
  if (is_observed) {
    if (function->has_prototype())
      old_value = handle(function->prototype(), isolate);
    else
      old_value = isolate->factory()->NewFunctionPrototype(function);
  }

  Handle<Object> result;
  MaybeObject* maybe_result = function->SetPrototype(*value);
  if (!maybe_result->ToHandle(&result, isolate)) return maybe_result;
  ASSERT(function->prototype() == *value);

  if (is_observed && !old_value->SameValue(*value)) {
    JSObject::EnqueueChangeRecord(
        function, "updated", isolate->factory()->prototype_string(), old_value);
  }

  return *function;
}


const AccessorDescriptor Accessors::FunctionPrototype = {
  FunctionGetPrototype,
  FunctionSetPrototype,
  0
};


//
// Accessors::FunctionLength
//


MaybeObject* Accessors::FunctionGetLength(Object* object, void*) {
  Isolate* isolate = Isolate::Current();
  JSFunction* function = FindInstanceOf<JSFunction>(isolate, object);
  if (function == NULL) return Smi::FromInt(0);
  // Check if already compiled.
  if (function->shared()->is_compiled()) {
    return Smi::FromInt(function->shared()->length());
  }
  // If the function isn't compiled yet, the length is not computed correctly
  // yet. Compile it now and return the right length.
  HandleScope scope(isolate);
  Handle<JSFunction> handle(function);
  if (JSFunction::CompileLazy(handle, KEEP_EXCEPTION)) {
    return Smi::FromInt(handle->shared()->length());
  }
  return Failure::Exception();
}


const AccessorDescriptor Accessors::FunctionLength = {
  FunctionGetLength,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionName
//


MaybeObject* Accessors::FunctionGetName(Object* object, void*) {
  Isolate* isolate = Isolate::Current();
  JSFunction* holder = FindInstanceOf<JSFunction>(isolate, object);
  return holder == NULL
      ? isolate->heap()->undefined_value()
      : holder->shared()->name();
}


const AccessorDescriptor Accessors::FunctionName = {
  FunctionGetName,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionArguments
//


static MaybeObject* ConstructArgumentsObjectForInlinedFunction(
    JavaScriptFrame* frame,
    Handle<JSFunction> inlined_function,
    int inlined_frame_index) {
  Isolate* isolate = inlined_function->GetIsolate();
  Factory* factory = isolate->factory();
  Vector<SlotRef> args_slots =
      SlotRef::ComputeSlotMappingForArguments(
          frame,
          inlined_frame_index,
          inlined_function->shared()->formal_parameter_count());
  int args_count = args_slots.length();
  Handle<JSObject> arguments =
      factory->NewArgumentsObject(inlined_function, args_count);
  Handle<FixedArray> array = factory->NewFixedArray(args_count);
  for (int i = 0; i < args_count; ++i) {
    Handle<Object> value = args_slots[i].GetValue(isolate);
    array->set(i, *value);
  }
  arguments->set_elements(*array);
  args_slots.Dispose();

  // Return the freshly allocated arguments object.
  return *arguments;
}


MaybeObject* Accessors::FunctionGetArguments(Object* object, void*) {
  Isolate* isolate = Isolate::Current();
  HandleScope scope(isolate);
  JSFunction* holder = FindInstanceOf<JSFunction>(isolate, object);
  if (holder == NULL) return isolate->heap()->undefined_value();
  Handle<JSFunction> function(holder, isolate);

  if (function->shared()->native()) return isolate->heap()->null_value();
  // Find the top invocation of the function by traversing frames.
  List<JSFunction*> functions(2);
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    frame->GetFunctions(&functions);
    for (int i = functions.length() - 1; i >= 0; i--) {
      // Skip all frames that aren't invocations of the given function.
      if (functions[i] != *function) continue;

      if (i > 0) {
        // The function in question was inlined.  Inlined functions have the
        // correct number of arguments and no allocated arguments object, so
        // we can construct a fresh one by interpreting the function's
        // deoptimization input data.
        return ConstructArgumentsObjectForInlinedFunction(frame, function, i);
      }

      if (!frame->is_optimized()) {
        // If there is an arguments variable in the stack, we return that.
        Handle<ScopeInfo> scope_info(function->shared()->scope_info());
        int index = scope_info->StackSlotIndex(
            isolate->heap()->arguments_string());
        if (index >= 0) {
          Handle<Object> arguments(frame->GetExpression(index), isolate);
          if (!arguments->IsArgumentsMarker()) return *arguments;
        }
      }

      // If there is no arguments variable in the stack or we have an
      // optimized frame, we find the frame that holds the actual arguments
      // passed to the function.
      it.AdvanceToArgumentsFrame();
      frame = it.frame();

      // Get the number of arguments and construct an arguments object
      // mirror for the right frame.
      const int length = frame->ComputeParametersCount();
      Handle<JSObject> arguments = isolate->factory()->NewArgumentsObject(
          function, length);
      Handle<FixedArray> array = isolate->factory()->NewFixedArray(length);

      // Copy the parameters to the arguments object.
      ASSERT(array->length() == length);
      for (int i = 0; i < length; i++) array->set(i, frame->GetParameter(i));
      arguments->set_elements(*array);

      // Return the freshly allocated arguments object.
      return *arguments;
    }
    functions.Rewind(0);
  }

  // No frame corresponding to the given function found. Return null.
  return isolate->heap()->null_value();
}


const AccessorDescriptor Accessors::FunctionArguments = {
  FunctionGetArguments,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionCaller
//


class FrameFunctionIterator {
 public:
  FrameFunctionIterator(Isolate* isolate, const DisallowHeapAllocation& promise)
      : frame_iterator_(isolate),
        functions_(2),
        index_(0) {
    GetFunctions();
  }
  JSFunction* next() {
    if (functions_.length() == 0) return NULL;
    JSFunction* next_function = functions_[index_];
    index_--;
    if (index_ < 0) {
      GetFunctions();
    }
    return next_function;
  }

  // Iterate through functions until the first occurence of 'function'.
  // Returns true if 'function' is found, and false if the iterator ends
  // without finding it.
  bool Find(JSFunction* function) {
    JSFunction* next_function;
    do {
      next_function = next();
      if (next_function == function) return true;
    } while (next_function != NULL);
    return false;
  }

 private:
  void GetFunctions() {
    functions_.Rewind(0);
    if (frame_iterator_.done()) return;
    JavaScriptFrame* frame = frame_iterator_.frame();
    frame->GetFunctions(&functions_);
    ASSERT(functions_.length() > 0);
    frame_iterator_.Advance();
    index_ = functions_.length() - 1;
  }
  JavaScriptFrameIterator frame_iterator_;
  List<JSFunction*> functions_;
  int index_;
};


MaybeObject* Accessors::FunctionGetCaller(Object* object, void*) {
  Isolate* isolate = Isolate::Current();
  HandleScope scope(isolate);
  DisallowHeapAllocation no_allocation;
  JSFunction* holder = FindInstanceOf<JSFunction>(isolate, object);
  if (holder == NULL) return isolate->heap()->undefined_value();
  if (holder->shared()->native()) return isolate->heap()->null_value();
  Handle<JSFunction> function(holder, isolate);

  FrameFunctionIterator it(isolate, no_allocation);

  // Find the function from the frames.
  if (!it.Find(*function)) {
    // No frame corresponding to the given function found. Return null.
    return isolate->heap()->null_value();
  }

  // Find previously called non-toplevel function.
  JSFunction* caller;
  do {
    caller = it.next();
    if (caller == NULL) return isolate->heap()->null_value();
  } while (caller->shared()->is_toplevel());

  // If caller is a built-in function and caller's caller is also built-in,
  // use that instead.
  JSFunction* potential_caller = caller;
  while (potential_caller != NULL && potential_caller->IsBuiltin()) {
    caller = potential_caller;
    potential_caller = it.next();
  }
  if (!caller->shared()->native() && potential_caller != NULL) {
    caller = potential_caller;
  }
  // If caller is bound, return null. This is compatible with JSC, and
  // allows us to make bound functions use the strict function map
  // and its associated throwing caller and arguments.
  if (caller->shared()->bound()) {
    return isolate->heap()->null_value();
  }
  // Censor if the caller is not a classic mode function.
  // Change from ES5, which used to throw, see:
  // https://bugs.ecmascript.org/show_bug.cgi?id=310
  if (!caller->shared()->is_classic_mode()) {
    return isolate->heap()->null_value();
  }

  return caller;
}


const AccessorDescriptor Accessors::FunctionCaller = {
  FunctionGetCaller,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::MakeModuleExport
//

static void ModuleGetExport(
    v8::Local<v8::String> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  JSModule* instance = JSModule::cast(*v8::Utils::OpenHandle(*info.Holder()));
  Context* context = Context::cast(instance->context());
  ASSERT(context->IsModuleContext());
  int slot = info.Data()->Int32Value();
  Object* value = context->get(slot);
  Isolate* isolate = instance->GetIsolate();
  if (value->IsTheHole()) {
    Handle<String> name = v8::Utils::OpenHandle(*property);
    isolate->ScheduleThrow(
        *isolate->factory()->NewReferenceError("not_defined",
                                               HandleVector(&name, 1)));
    return;
  }
  info.GetReturnValue().Set(v8::Utils::ToLocal(Handle<Object>(value, isolate)));
}


static void ModuleSetExport(
    v8::Local<v8::String> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  JSModule* instance = JSModule::cast(*v8::Utils::OpenHandle(*info.Holder()));
  Context* context = Context::cast(instance->context());
  ASSERT(context->IsModuleContext());
  int slot = info.Data()->Int32Value();
  Object* old_value = context->get(slot);
  if (old_value->IsTheHole()) {
    Handle<String> name = v8::Utils::OpenHandle(*property);
    Isolate* isolate = instance->GetIsolate();
    isolate->ScheduleThrow(
        *isolate->factory()->NewReferenceError("not_defined",
                                               HandleVector(&name, 1)));
    return;
  }
  context->set(slot, *v8::Utils::OpenHandle(*value));
}


Handle<AccessorInfo> Accessors::MakeModuleExport(
    Handle<String> name,
    int index,
    PropertyAttributes attributes) {
  Factory* factory = name->GetIsolate()->factory();
  Handle<ExecutableAccessorInfo> info = factory->NewExecutableAccessorInfo();
  info->set_property_attributes(attributes);
  info->set_all_can_read(true);
  info->set_all_can_write(true);
  info->set_name(*name);
  info->set_data(Smi::FromInt(index));
  Handle<Object> getter = v8::FromCData(&ModuleGetExport);
  Handle<Object> setter = v8::FromCData(&ModuleSetExport);
  info->set_getter(*getter);
  if (!(attributes & ReadOnly)) info->set_setter(*setter);
  return info;
}


} }  // namespace v8::internal
