// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/arguments.h"
#include "src/frames-inl.h"
#include "src/messages.h"
#include "src/runtime/runtime-utils.h"
#include "src/scopeinfo.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {

static Object* ThrowRedeclarationError(Isolate* isolate, Handle<String> name) {
  HandleScope scope(isolate);
  Handle<Object> args[1] = {name};
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError("var_redeclaration", HandleVector(args, 1)));
}


RUNTIME_FUNCTION(Runtime_ThrowConstAssignError) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate,
      NewTypeError("const_assign", HandleVector<Object>(NULL, 0)));
}


// May throw a RedeclarationError.
static Object* DeclareGlobals(Isolate* isolate, Handle<GlobalObject> global,
                              Handle<String> name, Handle<Object> value,
                              PropertyAttributes attr, bool is_var,
                              bool is_const, bool is_function) {
  Handle<ScriptContextTable> script_contexts(
      global->native_context()->script_context_table());
  ScriptContextTable::LookupResult lookup;
  if (ScriptContextTable::Lookup(script_contexts, name, &lookup) &&
      IsLexicalVariableMode(lookup.mode)) {
    return ThrowRedeclarationError(isolate, name);
  }

  // Do the lookup own properties only, see ES5 erratum.
  LookupIterator it(global, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  if (!maybe.IsJust()) return isolate->heap()->exception();

  if (it.IsFound()) {
    PropertyAttributes old_attributes = maybe.FromJust();
    // The name was declared before; check for conflicting re-declarations.
    if (is_const) return ThrowRedeclarationError(isolate, name);

    // Skip var re-declarations.
    if (is_var) return isolate->heap()->undefined_value();

    DCHECK(is_function);
    if ((old_attributes & DONT_DELETE) != 0) {
      // Only allow reconfiguring globals to functions in user code (no
      // natives, which are marked as read-only).
      DCHECK((attr & READ_ONLY) == 0);

      // Check whether we can reconfigure the existing property into a
      // function.
      PropertyDetails old_details = it.property_details();
      // TODO(verwaest): ACCESSOR_CONSTANT invalidly includes
      // ExecutableAccessInfo,
      // which are actually data properties, not accessor properties.
      if (old_details.IsReadOnly() || old_details.IsDontEnum() ||
          old_details.type() == ACCESSOR_CONSTANT) {
        return ThrowRedeclarationError(isolate, name);
      }
      // If the existing property is not configurable, keep its attributes. Do
      attr = old_attributes;
    }
  }

  // Define or redefine own property.
  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           global, name, value, attr));

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeclareGlobals) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  Handle<GlobalObject> global(isolate->global_object());

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, pairs, 1);
  CONVERT_SMI_ARG_CHECKED(flags, 2);

  // Traverse the name/value pairs and set the properties.
  int length = pairs->length();
  for (int i = 0; i < length; i += 2) {
    HandleScope scope(isolate);
    Handle<String> name(String::cast(pairs->get(i)));
    Handle<Object> initial_value(pairs->get(i + 1), isolate);

    // We have to declare a global const property. To capture we only
    // assign to it when evaluating the assignment for "const x =
    // <expr>" the initial value is the hole.
    bool is_var = initial_value->IsUndefined();
    bool is_const = initial_value->IsTheHole();
    bool is_function = initial_value->IsSharedFunctionInfo();
    DCHECK_EQ(1,
              BoolToInt(is_var) + BoolToInt(is_const) + BoolToInt(is_function));

    Handle<Object> value;
    if (is_function) {
      // Copy the function and update its context. Use it as value.
      Handle<SharedFunctionInfo> shared =
          Handle<SharedFunctionInfo>::cast(initial_value);
      Handle<JSFunction> function =
          isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                TENURED);
      value = function;
    } else {
      value = isolate->factory()->undefined_value();
    }

    // Compute the property attributes. According to ECMA-262,
    // the property must be non-configurable except in eval.
    bool is_native = DeclareGlobalsNativeFlag::decode(flags);
    bool is_eval = DeclareGlobalsEvalFlag::decode(flags);
    int attr = NONE;
    if (is_const) attr |= READ_ONLY;
    if (is_function && is_native) attr |= READ_ONLY;
    if (!is_const && !is_eval) attr |= DONT_DELETE;

    Object* result = DeclareGlobals(isolate, global, name, value,
                                    static_cast<PropertyAttributes>(attr),
                                    is_var, is_const, is_function);
    if (isolate->has_pending_exception()) return result;
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InitializeVarGlobal) {
  HandleScope scope(isolate);
  // args[0] == name
  // args[1] == language_mode
  // args[2] == value (optional)

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);

  Handle<GlobalObject> global(isolate->context()->global_object());
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, Object::SetProperty(global, name, value, language_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_InitializeConstGlobal) {
  HandleScope handle_scope(isolate);
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

  Handle<GlobalObject> global = isolate->global_object();

  // Lookup the property as own on the global object.
  LookupIterator it(global, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
  Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
  DCHECK(maybe.IsJust());
  PropertyAttributes old_attributes = maybe.FromJust();

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);
  // Set the value if the property is either missing, or the property attributes
  // allow setting the value without invoking an accessor.
  if (it.IsFound()) {
    // Ignore if we can't reconfigure the value.
    if ((old_attributes & DONT_DELETE) != 0) {
      if ((old_attributes & READ_ONLY) != 0 ||
          it.state() == LookupIterator::ACCESSOR) {
        return *value;
      }
      attr = static_cast<PropertyAttributes>(old_attributes | READ_ONLY);
    }
  }

  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           global, name, value, attr));

  return *value;
}


RUNTIME_FUNCTION(Runtime_DeclareLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);

  // Declarations are always made in a function, eval or script context. In
  // the case of eval code, the context passed is the context of the caller,
  // which may be some nested context and not the declaration context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 0);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);
  CONVERT_SMI_ARG_CHECKED(attr_arg, 2);
  PropertyAttributes attr = static_cast<PropertyAttributes>(attr_arg);
  RUNTIME_ASSERT(attr == READ_ONLY || attr == NONE);
  CONVERT_ARG_HANDLE_CHECKED(Object, initial_value, 3);

  // TODO(verwaest): Unify the encoding indicating "var" with DeclareGlobals.
  bool is_var = *initial_value == NULL;
  bool is_const = initial_value->IsTheHole();
  bool is_function = initial_value->IsJSFunction();
  DCHECK_EQ(1,
            BoolToInt(is_var) + BoolToInt(is_const) + BoolToInt(is_function));

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  Handle<JSObject> object;
  Handle<Object> value =
      is_function ? initial_value
                  : Handle<Object>::cast(isolate->factory()->undefined_value());

  // TODO(verwaest): This case should probably not be covered by this function,
  // but by DeclareGlobals instead.
  if (attributes != ABSENT && holder->IsJSGlobalObject()) {
    return DeclareGlobals(isolate, Handle<JSGlobalObject>::cast(holder), name,
                          value, attr, is_var, is_const, is_function);
  }
  if (context_arg->has_extension() &&
      context_arg->extension()->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global(
        JSGlobalObject::cast(context_arg->extension()), isolate);
    return DeclareGlobals(isolate, global, name, value, attr, is_var, is_const,
                          is_function);
  }

  if (attributes != ABSENT) {
    // The name was declared before; check for conflicting re-declarations.
    if (is_const || (attributes & READ_ONLY) != 0) {
      return ThrowRedeclarationError(isolate, name);
    }

    // Skip var re-declarations.
    if (is_var) return isolate->heap()->undefined_value();

    DCHECK(is_function);
    if (index >= 0) {
      DCHECK(holder.is_identical_to(context));
      context->set(index, *initial_value);
      return isolate->heap()->undefined_value();
    }

    object = Handle<JSObject>::cast(holder);

  } else if (context->has_extension()) {
    object = handle(JSObject::cast(context->extension()));
    DCHECK(object->IsJSContextExtensionObject() || object->IsJSGlobalObject());
  } else {
    DCHECK(context->IsFunctionContext());
    object =
        isolate->factory()->NewJSObject(isolate->context_extension_function());
    context->set_extension(*object);
  }

  RETURN_FAILURE_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                           object, name, value, attr));

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InitializeLegacyConstLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  DCHECK(!value->IsTheHole());
  // Initializations are always done in a function or native context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 1);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  if (index >= 0) {
    DCHECK(holder->IsContext());
    // Property was found in a context.  Perform the assignment if the constant
    // was uninitialized.
    Handle<Context> context = Handle<Context>::cast(holder);
    DCHECK((attributes & READ_ONLY) != 0);
    if (context->get(index)->IsTheHole()) context->set(index, *value);
    return *value;
  }

  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);

  // Strict mode handling not needed (legacy const is disallowed in strict
  // mode).

  // The declared const was configurable, and may have been deleted in the
  // meanwhile. If so, re-introduce the variable in the context extension.
  if (attributes == ABSENT) {
    Handle<Context> declaration_context(context_arg->declaration_context());
    DCHECK(declaration_context->has_extension());
    holder = handle(declaration_context->extension(), isolate);
    CHECK(holder->IsJSObject());
  } else {
    // For JSContextExtensionObjects, the initializer can be run multiple times
    // if in a for loop: for (var i = 0; i < 2; i++) { const x = i; }. Only the
    // first assignment should go through. For JSGlobalObjects, additionally any
    // code can run in between that modifies the declared property.
    DCHECK(holder->IsJSGlobalObject() || holder->IsJSContextExtensionObject());

    LookupIterator it(holder, name, LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
    Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
    if (!maybe.IsJust()) return isolate->heap()->exception();
    PropertyAttributes old_attributes = maybe.FromJust();

    // Ignore if we can't reconfigure the value.
    if ((old_attributes & DONT_DELETE) != 0) {
      if ((old_attributes & READ_ONLY) != 0 ||
          it.state() == LookupIterator::ACCESSOR) {
        return *value;
      }
      attr = static_cast<PropertyAttributes>(old_attributes | READ_ONLY);
    }
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                   Handle<JSObject>::cast(holder), name, value, attr));

  return *value;
}


static Handle<JSObject> NewSloppyArguments(Isolate* isolate,
                                           Handle<JSFunction> callee,
                                           Object** parameters,
                                           int argument_count) {
  CHECK(!IsSubclassConstructor(callee->shared()->kind()));
  DCHECK(callee->is_simple_parameter_list());
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);

  // Allocate the elements if needed.
  int parameter_count = callee->shared()->internal_formal_parameter_count();
  if (argument_count > 0) {
    if (parameter_count > 0) {
      int mapped_count = Min(argument_count, parameter_count);
      Handle<FixedArray> parameter_map =
          isolate->factory()->NewFixedArray(mapped_count + 2, NOT_TENURED);
      parameter_map->set_map(isolate->heap()->sloppy_arguments_elements_map());

      Handle<Map> map = Map::Copy(handle(result->map()), "NewSloppyArguments");
      map->set_elements_kind(SLOPPY_ARGUMENTS_ELEMENTS);

      result->set_map(*map);
      result->set_elements(*parameter_map);

      // Store the context and the arguments array at the beginning of the
      // parameter map.
      Handle<Context> context(isolate->context());
      Handle<FixedArray> arguments =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      parameter_map->set(0, *context);
      parameter_map->set(1, *arguments);

      // Loop over the actual parameters backwards.
      int index = argument_count - 1;
      while (index >= mapped_count) {
        // These go directly in the arguments array and have no
        // corresponding slot in the parameter map.
        arguments->set(index, *(parameters - index - 1));
        --index;
      }

      Handle<ScopeInfo> scope_info(callee->shared()->scope_info());
      while (index >= 0) {
        // Detect duplicate names to the right in the parameter list.
        Handle<String> name(scope_info->ParameterName(index));
        int context_local_count = scope_info->ContextLocalCount();
        bool duplicate = false;
        for (int j = index + 1; j < parameter_count; ++j) {
          if (scope_info->ParameterName(j) == *name) {
            duplicate = true;
            break;
          }
        }

        if (duplicate) {
          // This goes directly in the arguments array with a hole in the
          // parameter map.
          arguments->set(index, *(parameters - index - 1));
          parameter_map->set_the_hole(index + 2);
        } else {
          // The context index goes in the parameter map with a hole in the
          // arguments array.
          int context_index = -1;
          for (int j = 0; j < context_local_count; ++j) {
            if (scope_info->ContextLocalName(j) == *name) {
              context_index = j;
              break;
            }
          }

          DCHECK(context_index >= 0);
          arguments->set_the_hole(index);
          parameter_map->set(
              index + 2,
              Smi::FromInt(Context::MIN_CONTEXT_SLOTS + context_index));
        }

        --index;
      }
    } else {
      // If there is no aliasing, the arguments object elements are not
      // special in any way.
      Handle<FixedArray> elements =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      result->set_elements(*elements);
      for (int i = 0; i < argument_count; ++i) {
        elements->set(i, *(parameters - i - 1));
      }
    }
  }
  return result;
}


static Handle<JSObject> NewStrictArguments(Isolate* isolate,
                                           Handle<JSFunction> callee,
                                           Object** parameters,
                                           int argument_count) {
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);

  if (argument_count > 0) {
    Handle<FixedArray> array =
        isolate->factory()->NewUninitializedFixedArray(argument_count);
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < argument_count; i++) {
      array->set(i, *--parameters, mode);
    }
    result->set_elements(*array);
  }
  return result;
}


RUNTIME_FUNCTION(Runtime_NewArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  JavaScriptFrameIterator it(isolate);

  // Find the frame that holds the actual arguments passed to the function.
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Determine parameter location on the stack and dispatch on language mode.
  int argument_count = frame->GetArgumentsLength();
  Object** parameters = reinterpret_cast<Object**>(frame->GetParameterSlot(-1));

  return (is_strict(callee->shared()->language_mode()) ||
             !callee->is_simple_parameter_list())
             ? *NewStrictArguments(isolate, callee, parameters, argument_count)
             : *NewSloppyArguments(isolate, callee, parameters, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewSloppyArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
  return *NewSloppyArguments(isolate, callee, parameters, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewStrictArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
  return *NewStrictArguments(isolate, callee, parameters, argument_count);
}


static Handle<JSArray> NewRestParam(Isolate* isolate,
                                    Object** parameters,
                                    int num_params,
                                    int rest_index) {
  parameters -= rest_index;
  int num_elements = std::max(0, num_params - rest_index);
  Handle<FixedArray> elements =
      isolate->factory()->NewUninitializedFixedArray(num_elements);
  for (int i = 0; i < num_elements; ++i) {
    elements->set(i, *--parameters);
  }
  return isolate->factory()->NewJSArrayWithElements(elements, FAST_ELEMENTS,
                                                    num_elements);
}


RUNTIME_FUNCTION(Runtime_NewRestParam) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  Object** parameters = reinterpret_cast<Object**>(args[0]);
  CONVERT_SMI_ARG_CHECKED(num_params, 1);
  CONVERT_SMI_ARG_CHECKED(rest_index, 2);

  return *NewRestParam(isolate, parameters, num_params, rest_index);
}


RUNTIME_FUNCTION(Runtime_NewRestParamSlow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(rest_index, 0);

  JavaScriptFrameIterator it(isolate);

  // Find the frame that holds the actual arguments passed to the function.
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  int argument_count = frame->GetArgumentsLength();
  Object** parameters = reinterpret_cast<Object**>(frame->GetParameterSlot(-1));

  return *NewRestParam(isolate, parameters, argument_count, rest_index);
}


RUNTIME_FUNCTION(Runtime_NewClosureFromStubFailure) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  Handle<Context> context(isolate->context());
  PretenureFlag pretenure_flag = NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                pretenure_flag);
}


RUNTIME_FUNCTION(Runtime_NewClosure) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(pretenure, 2);

  // The caller ensures that we pretenure closures that are assigned
  // directly to properties.
  PretenureFlag pretenure_flag = pretenure ? TENURED : NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                pretenure_flag);
}

static Object* FindNameClash(Handle<ScopeInfo> scope_info,
                             Handle<GlobalObject> global_object,
                             Handle<ScriptContextTable> script_context) {
  Isolate* isolate = scope_info->GetIsolate();
  for (int var = 0; var < scope_info->ContextLocalCount(); var++) {
    Handle<String> name(scope_info->ContextLocalName(var));
    VariableMode mode = scope_info->ContextLocalMode(var);
    ScriptContextTable::LookupResult lookup;
    if (ScriptContextTable::Lookup(script_context, name, &lookup)) {
      if (IsLexicalVariableMode(mode) || IsLexicalVariableMode(lookup.mode)) {
        return ThrowRedeclarationError(isolate, name);
      }
    }

    if (IsLexicalVariableMode(mode)) {
      LookupIterator it(global_object, name,
                        LookupIterator::HIDDEN_SKIP_INTERCEPTOR);
      Maybe<PropertyAttributes> maybe = JSReceiver::GetPropertyAttributes(&it);
      if (!maybe.IsJust()) return isolate->heap()->exception();
      if ((maybe.FromJust() & DONT_DELETE) != 0) {
        return ThrowRedeclarationError(isolate, name);
      }

      GlobalObject::InvalidatePropertyCell(global_object, name);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NewScriptContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);
  Handle<GlobalObject> global_object(function->context()->global_object());
  Handle<Context> native_context(global_object->native_context());
  Handle<ScriptContextTable> script_context_table(
      native_context->script_context_table());

  Handle<String> clashed_name;
  Object* name_clash_result =
      FindNameClash(scope_info, global_object, script_context_table);
  if (isolate->has_pending_exception()) return name_clash_result;

  Handle<Context> result =
      isolate->factory()->NewScriptContext(function, scope_info);

  DCHECK(function->context() == isolate->context());
  DCHECK(function->context()->global_object() == result->global_object());

  Handle<ScriptContextTable> new_script_context_table =
      ScriptContextTable::Extend(script_context_table, result);
  native_context->set_script_context_table(*new_script_context_table);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NewFunctionContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  DCHECK(function->context() == isolate->context());
  int length = function->shared()->scope_info()->ContextLength();
  return *isolate->factory()->NewFunctionContext(length, function);
}


RUNTIME_FUNCTION(Runtime_PushWithContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  Handle<JSReceiver> extension_object;
  if (args[0]->IsJSReceiver()) {
    extension_object = args.at<JSReceiver>(0);
  } else {
    // Try to convert the object to a proper JavaScript object.
    MaybeHandle<JSReceiver> maybe_object =
        Object::ToObject(isolate, args.at<Object>(0));
    if (!maybe_object.ToHandle(&extension_object)) {
      Handle<Object> handle = args.at<Object>(0);
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kWithExpression, handle));
    }
  }

  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }

  Handle<Context> current(isolate->context());
  Handle<Context> context =
      isolate->factory()->NewWithContext(function, current, extension_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushCatchContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, thrown_object, 1);
  Handle<JSFunction> function;
  if (args[2]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(2);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewCatchContext(
      function, current, name, thrown_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushBlockContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 0);
  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context =
      isolate->factory()->NewBlockContext(function, current, scope_info);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_IsJSModule) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSModule());
}


RUNTIME_FUNCTION(Runtime_PushModuleContext) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(index, 0);

  if (!args[1]->IsScopeInfo()) {
    // Module already initialized. Find hosting context and retrieve context.
    Context* host = Context::cast(isolate->context())->script_context();
    Context* context = Context::cast(host->get(index));
    DCHECK(context->previous() == isolate->context());
    isolate->set_context(context);
    return context;
  }

  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);

  // Allocate module context.
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();
  Handle<Context> context = factory->NewModuleContext(scope_info);
  Handle<JSModule> module = factory->NewJSModule(context, scope_info);
  context->set_module(*module);
  Context* previous = isolate->context();
  context->set_previous(previous);
  context->set_closure(previous->closure());
  context->set_global_object(previous->global_object());
  isolate->set_context(*context);

  // Find hosting scope and initialize internal variable holding module there.
  previous->script_context()->set(index, *context);

  return *context;
}


RUNTIME_FUNCTION(Runtime_DeclareModules) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, descriptions, 0);
  Context* host_context = isolate->context();

  for (int i = 0; i < descriptions->length(); ++i) {
    Handle<ModuleInfo> description(ModuleInfo::cast(descriptions->get(i)));
    int host_index = description->host_index();
    Handle<Context> context(Context::cast(host_context->get(host_index)));
    Handle<JSModule> module(context->module());

    for (int j = 0; j < description->length(); ++j) {
      Handle<String> name(description->name(j));
      VariableMode mode = description->mode(j);
      int index = description->index(j);
      switch (mode) {
        case VAR:
        case LET:
        case CONST:
        case CONST_LEGACY:
        case IMPORT: {
          PropertyAttributes attr =
              IsImmutableVariableMode(mode) ? FROZEN : SEALED;
          Handle<AccessorInfo> info =
              Accessors::MakeModuleExport(name, index, attr);
          Handle<Object> result =
              JSObject::SetAccessor(module, info).ToHandleChecked();
          DCHECK(!result->IsUndefined());
          USE(result);
          break;
        }
        case INTERNAL:
        case TEMPORARY:
        case DYNAMIC:
        case DYNAMIC_GLOBAL:
        case DYNAMIC_LOCAL:
          UNREACHABLE();
      }
    }

    JSObject::PreventExtensions(module).Assert();
  }

  DCHECK(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeleteLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  // If the slot was not found the result is true.
  if (holder.is_null()) {
    return isolate->heap()->true_value();
  }

  // If the slot was found in a context, it should be DONT_DELETE.
  if (holder->IsContext()) {
    return isolate->heap()->false_value();
  }

  // The slot was found in a JSObject, either a context extension object,
  // the global object, or the subject of a with.  Try to delete it
  // (respecting DONT_DELETE).
  Handle<JSObject> object = Handle<JSObject>::cast(holder);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSReceiver::DeleteProperty(object, name));
  return *result;
}


static Object* ComputeReceiverForNonGlobal(Isolate* isolate, JSObject* holder) {
  DCHECK(!holder->IsGlobalObject());

  // If the holder isn't a context extension object, we just return it
  // as the receiver. This allows arguments objects to be used as
  // receivers, but only if they are put in the context scope chain
  // explicitly via a with-statement.
  if (holder->map()->instance_type() != JS_CONTEXT_EXTENSION_OBJECT_TYPE) {
    return holder;
  }
  // Fall back to using the global object as the implicit receiver if
  // the property turns out to be a local variable allocated in a
  // context extension object - introduced via eval.
  return isolate->heap()->undefined_value();
}


static ObjectPair LoadLookupSlotHelper(Arguments args, Isolate* isolate,
                                       bool throw_error) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());

  if (!args[0]->IsContext() || !args[1]->IsString()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }
  Handle<Context> context = args.at<Context>(0);
  Handle<String> name = args.at<String>(1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);
  if (isolate->has_pending_exception()) {
    return MakePair(isolate->heap()->exception(), NULL);
  }

  // If the index is non-negative, the slot has been found in a context.
  if (index >= 0) {
    DCHECK(holder->IsContext());
    // If the "property" we were looking for is a local variable, the
    // receiver is the global object; see ECMA-262, 3rd., 10.1.6 and 10.2.3.
    Handle<Object> receiver = isolate->factory()->undefined_value();
    Object* value = Context::cast(*holder)->get(index);
    // Check for uninitialized bindings.
    switch (binding_flags) {
      case MUTABLE_CHECK_INITIALIZED:
      case IMMUTABLE_CHECK_INITIALIZED_HARMONY:
        if (value->IsTheHole()) {
          Handle<Object> error = isolate->factory()->NewReferenceError(
              MessageTemplate::kNotDefined, name);
          isolate->Throw(*error);
          return MakePair(isolate->heap()->exception(), NULL);
        }
      // FALLTHROUGH
      case MUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED_HARMONY:
        DCHECK(!value->IsTheHole());
        return MakePair(value, *receiver);
      case IMMUTABLE_CHECK_INITIALIZED:
        if (value->IsTheHole()) {
          DCHECK((attributes & READ_ONLY) != 0);
          value = isolate->heap()->undefined_value();
        }
        return MakePair(value, *receiver);
      case MISSING_BINDING:
        UNREACHABLE();
        return MakePair(NULL, NULL);
    }
  }

  // Otherwise, if the slot was found the holder is a context extension
  // object, subject of a with, or a global object.  We read the named
  // property from it.
  if (!holder.is_null()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
    // GetProperty below can cause GC.
    Handle<Object> receiver_handle(
        object->IsGlobalObject()
            ? Object::cast(isolate->heap()->undefined_value())
            : object->IsJSProxy() ? static_cast<Object*>(*object)
                                  : ComputeReceiverForNonGlobal(
                                        isolate, JSObject::cast(*object)),
        isolate);

    // No need to unhole the value here.  This is taken care of by the
    // GetProperty function.
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value, Object::GetProperty(object, name),
        MakePair(isolate->heap()->exception(), NULL));
    return MakePair(*value, *receiver_handle);
  }

  if (throw_error) {
    // The property doesn't exist - throw exception.
    Handle<Object> error = isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, name);
    isolate->Throw(*error);
    return MakePair(isolate->heap()->exception(), NULL);
  } else {
    // The property doesn't exist - return undefined.
    return MakePair(isolate->heap()->undefined_value(),
                    isolate->heap()->undefined_value());
  }
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_LoadLookupSlot) {
  return LoadLookupSlotHelper(args, isolate, true);
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_LoadLookupSlotNoReferenceError) {
  return LoadLookupSlotHelper(args, isolate, false);
}


RUNTIME_FUNCTION(Runtime_StoreLookupSlot) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 3);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);
  // In case of JSProxy, an exception might have been thrown.
  if (isolate->has_pending_exception()) return isolate->heap()->exception();

  // The property was found in a context slot.
  if (index >= 0) {
    if ((attributes & READ_ONLY) == 0) {
      Handle<Context>::cast(holder)->set(index, *value);
    } else if (is_strict(language_mode)) {
      // Setting read only property in strict mode.
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate,
          NewTypeError("strict_cannot_assign", HandleVector(&name, 1)));
    }
    return *value;
  }

  // Slow case: The property is not in a context slot.  It is either in a
  // context extension object, a property of the subject of a with, or a
  // property of the global object.
  Handle<JSReceiver> object;
  if (attributes != ABSENT) {
    // The property exists on the holder.
    object = Handle<JSReceiver>::cast(holder);
  } else if (is_strict(language_mode)) {
    // If absent in strict mode: throw.
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
  } else {
    // If absent in sloppy mode: add the property to the global object.
    object = Handle<JSReceiver>(context->global_object());
  }

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, Object::SetProperty(object, name, value, language_mode));

  return *value;
}


RUNTIME_FUNCTION(Runtime_GetArgumentsProperty) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, raw_key, 0);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it(isolate);
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->ComputeParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (raw_key->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  HandleScope scope(isolate);
  if (raw_key->IsSymbol()) {
    Handle<Symbol> symbol = Handle<Symbol>::cast(raw_key);
    if (Name::Equals(symbol, isolate->factory()->iterator_symbol())) {
      return isolate->native_context()->array_values_iterator();
    }
    // Lookup in the initial Object.prototype object.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Object::GetProperty(isolate->initial_object_prototype(),
                            Handle<Symbol>::cast(raw_key)));
    return *result;
  }

  // Convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, converted,
                                     Execution::ToString(isolate, raw_key));
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < n) {
      return frame->GetParameter(index);
    } else {
      Handle<Object> initial_prototype(isolate->initial_object_prototype());
      Handle<Object> result;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, result,
          Object::GetElement(isolate, initial_prototype, index));
      return *result;
    }
  }

  // Handle special arguments properties.
  if (String::Equals(isolate->factory()->length_string(), key)) {
    return Smi::FromInt(n);
  }
  if (String::Equals(isolate->factory()->callee_string(), key)) {
    JSFunction* function = frame->function();
    if (is_strict(function->shared()->language_mode())) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError("strict_arguments_callee",
                                HandleVector<Object>(NULL, 0)));
    }
    return function;
  }

  // Lookup in the initial Object.prototype object.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::GetProperty(isolate->initial_object_prototype(), key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ArgumentsLength) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 0);
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  return Smi::FromInt(frame->GetArgumentsLength());
}


RUNTIME_FUNCTION(Runtime_Arguments) {
  SealHandleScope shs(isolate);
  return __RT_impl_Runtime_GetArgumentsProperty(args, isolate);
}
}
}  // namespace v8::internal
