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

#include "v8.h"

#include "accessors.h"
#include "execution.h"
#include "factory.h"
#include "scopeinfo.h"
#include "top.h"

namespace v8 {
namespace internal {


template <class C>
static C* FindInPrototypeChain(Object* obj, bool* found_it) {
  ASSERT(!*found_it);
  while (!Is<C>(obj)) {
    if (obj == Heap::null_value()) return NULL;
    obj = obj->GetPrototype();
  }
  *found_it = true;
  return C::cast(obj);
}


// Entry point that never should be called.
Object* Accessors::IllegalSetter(JSObject*, Object*, void*) {
  UNREACHABLE();
  return NULL;
}


Object* Accessors::IllegalGetAccessor(Object* object, void*) {
  UNREACHABLE();
  return object;
}


Object* Accessors::ReadOnlySetAccessor(JSObject*, Object* value, void*) {
  // According to ECMA-262, section 8.6.2.2, page 28, setting
  // read-only properties must be silently ignored.
  return value;
}


//
// Accessors::ArrayLength
//


Object* Accessors::ArrayGetLength(Object* object, void*) {
  // Traverse the prototype chain until we reach an array.
  bool found_it = false;
  JSArray* holder = FindInPrototypeChain<JSArray>(object, &found_it);
  if (!found_it) return Smi::FromInt(0);
  return holder->length();
}


// The helper function will 'flatten' Number objects.
Object* Accessors::FlattenNumber(Object* value) {
  if (value->IsNumber() || !value->IsJSValue()) return value;
  JSValue* wrapper = JSValue::cast(value);
  ASSERT(
      Top::context()->global_context()->number_function()->has_initial_map());
  Map* number_map =
      Top::context()->global_context()->number_function()->initial_map();
  if (wrapper->map() == number_map) return wrapper->value();
  return value;
}


Object* Accessors::ArraySetLength(JSObject* object, Object* value, void*) {
  value = FlattenNumber(value);

  // Need to call methods that may trigger GC.
  HandleScope scope;

  // Protect raw pointers.
  Handle<JSObject> object_handle(object);
  Handle<Object> value_handle(value);

  bool has_exception;
  Handle<Object> uint32_v = Execution::ToUint32(value_handle, &has_exception);
  if (has_exception) return Failure::Exception();
  Handle<Object> number_v = Execution::ToNumber(value_handle, &has_exception);
  if (has_exception) return Failure::Exception();

  // Restore raw pointers,
  object = *object_handle;
  value = *value_handle;

  if (uint32_v->Number() == number_v->Number()) {
    if (object->IsJSArray()) {
      return JSArray::cast(object)->SetElementsLength(*uint32_v);
    } else {
      // This means one of the object's prototypes is a JSArray and
      // the object does not have a 'length' property.
      // Calling SetProperty causes an infinite loop.
      return object->IgnoreAttributesAndSetLocalProperty(Heap::length_symbol(),
                                                         value, NONE);
    }
  }
  return Top::Throw(*Factory::NewRangeError("invalid_array_length",
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


Object* Accessors::StringGetLength(Object* object, void*) {
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


Object* Accessors::ScriptGetSource(Object* object, void*) {
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


Object* Accessors::ScriptGetName(Object* object, void*) {
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


Object* Accessors::ScriptGetId(Object* object, void*) {
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


Object* Accessors::ScriptGetLineOffset(Object* object, void*) {
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


Object* Accessors::ScriptGetColumnOffset(Object* object, void*) {
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


Object* Accessors::ScriptGetData(Object* object, void*) {
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


Object* Accessors::ScriptGetType(Object* object, void*) {
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


Object* Accessors::ScriptGetCompilationType(Object* object, void*) {
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


Object* Accessors::ScriptGetLineEnds(Object* object, void*) {
  HandleScope scope;
  Handle<Script> script(Script::cast(JSValue::cast(object)->value()));
  InitScriptLineEnds(script);
  ASSERT(script->line_ends()->IsFixedArray());
  Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
  Handle<FixedArray> copy = Factory::CopyFixedArray(line_ends);
  Handle<JSArray> js_array = Factory::NewJSArrayWithElements(copy);
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


Object* Accessors::ScriptGetContextData(Object* object, void*) {
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


Object* Accessors::ScriptGetEvalFromScript(Object* object, void*) {
  Object* script = JSValue::cast(object)->value();
  if (!Script::cast(script)->eval_from_shared()->IsUndefined()) {
    Handle<SharedFunctionInfo> eval_from_shared(
        SharedFunctionInfo::cast(Script::cast(script)->eval_from_shared()));

    if (eval_from_shared->script()->IsScript()) {
      Handle<Script> eval_from_script(Script::cast(eval_from_shared->script()));
      return *GetScriptWrapper(eval_from_script);
    }
  }
  return Heap::undefined_value();
}


const AccessorDescriptor Accessors::ScriptEvalFromScript = {
  ScriptGetEvalFromScript,
  IllegalSetter,
  0
};


//
// Accessors::ScriptGetEvalFromScriptPosition
//


Object* Accessors::ScriptGetEvalFromScriptPosition(Object* object, void*) {
  HandleScope scope;
  Handle<Script> script(Script::cast(JSValue::cast(object)->value()));

  // If this is not a script compiled through eval there is no eval position.
  int compilation_type = Smi::cast(script->compilation_type())->value();
  if (compilation_type != Script::COMPILATION_TYPE_EVAL) {
    return Heap::undefined_value();
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


Object* Accessors::ScriptGetEvalFromFunctionName(Object* object, void*) {
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


Object* Accessors::FunctionGetPrototype(Object* object, void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  if (!function->has_prototype()) {
    Object* prototype = Heap::AllocateFunctionPrototype(function);
    if (prototype->IsFailure()) return prototype;
    Object* result = function->SetPrototype(prototype);
    if (result->IsFailure()) return result;
  }
  return function->prototype();
}


Object* Accessors::FunctionSetPrototype(JSObject* object,
                                        Object* value,
                                        void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  if (function->has_initial_map()) {
    // If the function has allocated the initial map
    // replace it with a copy containing the new prototype.
    Object* new_map = function->initial_map()->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;
    function->set_initial_map(Map::cast(new_map));
  }
  Object* prototype = function->SetPrototype(value);
  if (prototype->IsFailure()) return prototype;
  ASSERT(function->prototype() == value);
  return function;
}


const AccessorDescriptor Accessors::FunctionPrototype = {
  FunctionGetPrototype,
  FunctionSetPrototype,
  0
};


//
// Accessors::FunctionLength
//


Object* Accessors::FunctionGetLength(Object* object, void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Smi::FromInt(0);
  // Check if already compiled.
  if (!function->is_compiled()) {
    // If the function isn't compiled yet, the length is not computed
    // correctly yet. Compile it now and return the right length.
    HandleScope scope;
    Handle<SharedFunctionInfo> shared(function->shared());
    if (!CompileLazyShared(shared, KEEP_EXCEPTION)) {
      return Failure::Exception();
    }
    return Smi::FromInt(shared->length());
  } else {
    return Smi::FromInt(function->shared()->length());
  }
}


const AccessorDescriptor Accessors::FunctionLength = {
  FunctionGetLength,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionName
//


Object* Accessors::FunctionGetName(Object* object, void*) {
  bool found_it = false;
  JSFunction* holder = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  return holder->shared()->name();
}


const AccessorDescriptor Accessors::FunctionName = {
  FunctionGetName,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionArguments
//


Object* Accessors::FunctionGetArguments(Object* object, void*) {
  HandleScope scope;
  bool found_it = false;
  JSFunction* holder = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  Handle<JSFunction> function(holder);

  // Find the top invocation of the function by traversing frames.
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    // Skip all frames that aren't invocations of the given function.
    JavaScriptFrame* frame = it.frame();
    if (frame->function() != *function) continue;

    // If there is an arguments variable in the stack, we return that.
    int index = ScopeInfo<>::StackSlotIndex(frame->code(),
                                            Heap::arguments_symbol());
    if (index >= 0) {
      Handle<Object> arguments = Handle<Object>(frame->GetExpression(index));
      if (!arguments->IsTheHole()) return *arguments;
    }

    // If there isn't an arguments variable in the stack, we need to
    // find the frame that holds the actual arguments passed to the
    // function on the stack.
    it.AdvanceToArgumentsFrame();
    frame = it.frame();

    // Get the number of arguments and construct an arguments object
    // mirror for the right frame.
    const int length = frame->GetProvidedParametersCount();
    Handle<JSObject> arguments = Factory::NewArgumentsObject(function, length);
    Handle<FixedArray> array = Factory::NewFixedArray(length);

    // Copy the parameters to the arguments object.
    ASSERT(array->length() == length);
    for (int i = 0; i < length; i++) array->set(i, frame->GetParameter(i));
    arguments->set_elements(*array);

    // Return the freshly allocated arguments object.
    return *arguments;
  }

  // No frame corresponding to the given function found. Return null.
  return Heap::null_value();
}


const AccessorDescriptor Accessors::FunctionArguments = {
  FunctionGetArguments,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::FunctionCaller
//


Object* Accessors::FunctionGetCaller(Object* object, void*) {
  HandleScope scope;
  bool found_it = false;
  JSFunction* holder = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  Handle<JSFunction> function(holder);

  // Find the top invocation of the function by traversing frames.
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    // Skip all frames that aren't invocations of the given function.
    if (it.frame()->function() != *function) continue;
    // Once we have found the frame, we need to go to the caller
    // frame. This may require skipping through a number of top-level
    // frames, e.g. frames for scripts not functions.
    while (true) {
      it.Advance();
      if (it.done()) return Heap::null_value();
      JSFunction* caller = JSFunction::cast(it.frame()->function());
      if (!caller->shared()->is_toplevel()) return caller;
    }
  }

  // No frame corresponding to the given function found. Return null.
  return Heap::null_value();
}


const AccessorDescriptor Accessors::FunctionCaller = {
  FunctionGetCaller,
  ReadOnlySetAccessor,
  0
};


//
// Accessors::ObjectPrototype
//


Object* Accessors::ObjectGetPrototype(Object* receiver, void*) {
  Object* current = receiver->GetPrototype();
  while (current->IsJSObject() &&
         JSObject::cast(current)->map()->is_hidden_prototype()) {
    current = current->GetPrototype();
  }
  return current;
}


Object* Accessors::ObjectSetPrototype(JSObject* receiver,
                                      Object* value,
                                      void*) {
  const bool skip_hidden_prototypes = true;
  // To be consistent with other Set functions, return the value.
  return receiver->SetPrototype(value, skip_hidden_prototypes);
}


const AccessorDescriptor Accessors::ObjectPrototype = {
  ObjectGetPrototype,
  ObjectSetPrototype,
  0
};

} }  // namespace v8::internal
