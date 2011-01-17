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
#include "ast.h"
#include "deoptimizer.h"
#include "execution.h"
#include "factory.h"
#include "safepoint-table.h"
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


MaybeObject* Accessors::ArraySetLength(JSObject* object, Object* value, void*) {
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
      return object->SetLocalPropertyIgnoreAttributes(Heap::length_symbol(),
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
  HandleScope scope;
  Handle<Script> script(Script::cast(JSValue::cast(object)->value()));
  InitScriptLineEnds(script);
  ASSERT(script->line_ends()->IsFixedArray());
  Handle<FixedArray> line_ends(FixedArray::cast(script->line_ends()));
  // We do not want anyone to modify this array from JS.
  ASSERT(*line_ends == Heap::empty_fixed_array() ||
         line_ends->map() == Heap::fixed_cow_array_map());
  Handle<JSArray> js_array = Factory::NewJSArrayWithElements(line_ends);
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


MaybeObject* Accessors::ScriptGetEvalFromScriptPosition(Object* object, void*) {
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


MaybeObject* Accessors::FunctionGetPrototype(Object* object, void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  if (!function->has_prototype()) {
    Object* prototype;
    { MaybeObject* maybe_prototype = Heap::AllocateFunctionPrototype(function);
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
                                             Object* value,
                                             void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  if (function->has_initial_map()) {
    // If the function has allocated the initial map
    // replace it with a copy containing the new prototype.
    Object* new_map;
    { MaybeObject* maybe_new_map =
          function->initial_map()->CopyDropTransitions();
      if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
    }
    function->set_initial_map(Map::cast(new_map));
  }
  Object* prototype;
  { MaybeObject* maybe_prototype = function->SetPrototype(value);
    if (!maybe_prototype->ToObject(&prototype)) return maybe_prototype;
  }
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


MaybeObject* Accessors::FunctionGetLength(Object* object, void*) {
  bool found_it = false;
  JSFunction* function = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Smi::FromInt(0);
  // Check if already compiled.
  if (!function->shared()->is_compiled()) {
    // If the function isn't compiled yet, the length is not computed
    // correctly yet. Compile it now and return the right length.
    HandleScope scope;
    Handle<JSFunction> handle(function);
    if (!CompileLazy(handle, KEEP_EXCEPTION)) return Failure::Exception();
    return Smi::FromInt(handle->shared()->length());
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


MaybeObject* Accessors::FunctionGetName(Object* object, void*) {
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

static Address SlotAddress(JavaScriptFrame* frame, int slot_index) {
  if (slot_index >= 0) {
    const int offset = JavaScriptFrameConstants::kLocal0Offset;
    return frame->fp() + offset - (slot_index * kPointerSize);
  } else {
    const int offset = JavaScriptFrameConstants::kReceiverOffset;
    return frame->caller_sp() + offset + (slot_index * kPointerSize);
  }
}


// We can't intermix stack decoding and allocations because
// deoptimization infrastracture is not GC safe.
// Thus we build a temporary structure in malloced space.
class SlotRef BASE_EMBEDDED {
 public:
  enum SlotRepresentation {
    UNKNOWN,
    TAGGED,
    INT32,
    DOUBLE,
    LITERAL
  };

  SlotRef()
      : addr_(NULL), representation_(UNKNOWN) { }

  SlotRef(Address addr, SlotRepresentation representation)
      : addr_(addr), representation_(representation) { }

  explicit SlotRef(Object* literal)
      : literal_(literal), representation_(LITERAL) { }

  Handle<Object> GetValue() {
    switch (representation_) {
      case TAGGED:
        return Handle<Object>(Memory::Object_at(addr_));

      case INT32: {
        int value = Memory::int32_at(addr_);
        if (Smi::IsValid(value)) {
          return Handle<Object>(Smi::FromInt(value));
        } else {
          return Factory::NewNumberFromInt(value);
        }
      }

      case DOUBLE: {
        double value = Memory::double_at(addr_);
        return Factory::NewNumber(value);
      }

      case LITERAL:
        return literal_;

      default:
        UNREACHABLE();
        return Handle<Object>::null();
    }
  }

 private:
  Address addr_;
  Handle<Object> literal_;
  SlotRepresentation representation_;
};


static SlotRef ComputeSlotForNextArgument(TranslationIterator* iterator,
                                          DeoptimizationInputData* data,
                                          JavaScriptFrame* frame) {
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());

  switch (opcode) {
    case Translation::BEGIN:
    case Translation::FRAME:
      // Peeled off before getting here.
      break;

    case Translation::ARGUMENTS_OBJECT:
      // This can be only emitted for local slots not for argument slots.
      break;

    case Translation::REGISTER:
    case Translation::INT32_REGISTER:
    case Translation::DOUBLE_REGISTER:
    case Translation::DUPLICATE:
      // We are at safepoint which corresponds to call.  All registers are
      // saved by caller so there would be no live registers at this
      // point. Thus these translation commands should not be used.
      break;

    case Translation::STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::TAGGED);
    }

    case Translation::INT32_STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::INT32);
    }

    case Translation::DOUBLE_STACK_SLOT: {
      int slot_index = iterator->Next();
      Address slot_addr = SlotAddress(frame, slot_index);
      return SlotRef(slot_addr, SlotRef::DOUBLE);
    }

    case Translation::LITERAL: {
      int literal_index = iterator->Next();
      return SlotRef(data->LiteralArray()->get(literal_index));
    }
  }

  UNREACHABLE();
  return SlotRef();
}





static void ComputeSlotMappingForArguments(JavaScriptFrame* frame,
                                           int inlined_frame_index,
                                           Vector<SlotRef>* args_slots) {
  AssertNoAllocation no_gc;

  int deopt_index = AstNode::kNoNumber;

  DeoptimizationInputData* data =
      static_cast<OptimizedFrame*>(frame)->GetDeoptimizationData(&deopt_index);

  TranslationIterator it(data->TranslationByteArray(),
                         data->TranslationIndex(deopt_index)->value());

  Translation::Opcode opcode = static_cast<Translation::Opcode>(it.Next());
  ASSERT(opcode == Translation::BEGIN);
  int frame_count = it.Next();

  USE(frame_count);
  ASSERT(frame_count > inlined_frame_index);

  int frames_to_skip = inlined_frame_index;
  while (true) {
    opcode = static_cast<Translation::Opcode>(it.Next());

    // Skip over operands to advance to the next opcode.
    it.Skip(Translation::NumberOfOperandsFor(opcode));

    if (opcode == Translation::FRAME) {
      if (frames_to_skip == 0) {
        // We reached frame corresponding to inlined function in question.
        // Process translation commands for arguments.

        // Skip translation command for receiver.
        it.Skip(Translation::NumberOfOperandsFor(
            static_cast<Translation::Opcode>(it.Next())));

        // Compute slots for arguments.
        for (int i = 0; i < args_slots->length(); ++i) {
          (*args_slots)[i] = ComputeSlotForNextArgument(&it, data, frame);
        }

        return;
      }

      frames_to_skip--;
    }
  }

  UNREACHABLE();
}


static MaybeObject* ConstructArgumentsObjectForInlinedFunction(
    JavaScriptFrame* frame,
    Handle<JSFunction> inlined_function,
    int inlined_frame_index) {

  int args_count = inlined_function->shared()->formal_parameter_count();

  ScopedVector<SlotRef> args_slots(args_count);

  ComputeSlotMappingForArguments(frame, inlined_frame_index, &args_slots);

  Handle<JSObject> arguments =
      Factory::NewArgumentsObject(inlined_function, args_count);

  Handle<FixedArray> array = Factory::NewFixedArray(args_count);
  for (int i = 0; i < args_count; ++i) {
    Handle<Object> value = args_slots[i].GetValue();
    array->set(i, *value);
  }
  arguments->set_elements(*array);

  // Return the freshly allocated arguments object.
  return *arguments;
}


MaybeObject* Accessors::FunctionGetArguments(Object* object, void*) {
  HandleScope scope;
  bool found_it = false;
  JSFunction* holder = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  Handle<JSFunction> function(holder);

  // Find the top invocation of the function by traversing frames.
  List<JSFunction*> functions(2);
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    frame->GetFunctions(&functions);
    for (int i = functions.length() - 1; i >= 0; i--) {
      // Skip all frames that aren't invocations of the given function.
      if (functions[i] != *function) continue;

      if (i > 0) {
        // Function in question was inlined.
        return ConstructArgumentsObjectForInlinedFunction(frame, function, i);
      } else {
        // If there is an arguments variable in the stack, we return that.
        int index = function->shared()->scope_info()->
            StackSlotIndex(Heap::arguments_symbol());
        if (index >= 0) {
          Handle<Object> arguments =
              Handle<Object>(frame->GetExpression(index));
          if (!arguments->IsArgumentsMarker()) return *arguments;
        }

        // If there isn't an arguments variable in the stack, we need to
        // find the frame that holds the actual arguments passed to the
        // function on the stack.
        it.AdvanceToArgumentsFrame();
        frame = it.frame();

        // Get the number of arguments and construct an arguments object
        // mirror for the right frame.
        const int length = frame->GetProvidedParametersCount();
        Handle<JSObject> arguments = Factory::NewArgumentsObject(function,
                                                                 length);
        Handle<FixedArray> array = Factory::NewFixedArray(length);

        // Copy the parameters to the arguments object.
        ASSERT(array->length() == length);
        for (int i = 0; i < length; i++) array->set(i, frame->GetParameter(i));
        arguments->set_elements(*array);

        // Return the freshly allocated arguments object.
        return *arguments;
      }
    }
    functions.Rewind(0);
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


MaybeObject* Accessors::FunctionGetCaller(Object* object, void*) {
  HandleScope scope;
  AssertNoAllocation no_alloc;
  bool found_it = false;
  JSFunction* holder = FindInPrototypeChain<JSFunction>(object, &found_it);
  if (!found_it) return Heap::undefined_value();
  Handle<JSFunction> function(holder);

  List<JSFunction*> functions(2);
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    frame->GetFunctions(&functions);
    for (int i = functions.length() - 1; i >= 0; i--) {
      if (functions[i] == *function) {
        // Once we have found the frame, we need to go to the caller
        // frame. This may require skipping through a number of top-level
        // frames, e.g. frames for scripts not functions.
        if (i > 0) {
          ASSERT(!functions[i - 1]->shared()->is_toplevel());
          return functions[i - 1];
        } else {
          for (it.Advance(); !it.done(); it.Advance()) {
            frame = it.frame();
            functions.Rewind(0);
            frame->GetFunctions(&functions);
            if (!functions.last()->shared()->is_toplevel()) {
              return functions.last();
            }
            ASSERT(functions.length() == 1);
          }
          if (it.done()) return Heap::null_value();
          break;
        }
      }
    }
    functions.Rewind(0);
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


MaybeObject* Accessors::ObjectGetPrototype(Object* receiver, void*) {
  Object* current = receiver->GetPrototype();
  while (current->IsJSObject() &&
         JSObject::cast(current)->map()->is_hidden_prototype()) {
    current = current->GetPrototype();
  }
  return current;
}


MaybeObject* Accessors::ObjectSetPrototype(JSObject* receiver,
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
