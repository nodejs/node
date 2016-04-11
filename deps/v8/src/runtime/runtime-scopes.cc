// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/accessors.h"
#include "src/arguments.h"
#include "src/ast/scopeinfo.h"
#include "src/ast/scopes.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"

namespace v8 {
namespace internal {

static Object* ThrowRedeclarationError(Isolate* isolate, Handle<String> name) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kVarRedeclaration, name));
}


RUNTIME_FUNCTION(Runtime_ThrowConstAssignError) {
  HandleScope scope(isolate);
  THROW_NEW_ERROR_RETURN_FAILURE(isolate,
                                 NewTypeError(MessageTemplate::kConstAssign));
}


// May throw a RedeclarationError.
static Object* DeclareGlobals(Isolate* isolate, Handle<JSGlobalObject> global,
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
  DCHECK_EQ(2, args.length());
  Handle<JSGlobalObject> global(isolate->global_object());
  Handle<Context> context(isolate->context());

  CONVERT_ARG_HANDLE_CHECKED(FixedArray, pairs, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);

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

  Handle<JSGlobalObject> global(isolate->context()->global_object());
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

  Handle<JSGlobalObject> global = isolate->global_object();

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


namespace {

Object* DeclareLookupSlot(Isolate* isolate, Handle<String> name,
                          Handle<Object> initial_value,
                          PropertyAttributes attr) {
  // Declarations are always made in a function, eval or script context, or
  // a declaration block scope.
  // In the case of eval code, the context passed is the context of the caller,
  // which may be some nested context and not the declaration context.
  Handle<Context> context_arg(isolate->context(), isolate);
  Handle<Context> context(context_arg->declaration_context(), isolate);

  // TODO(verwaest): Unify the encoding indicating "var" with DeclareGlobals.
  bool is_var = *initial_value == NULL;
  bool is_const = initial_value->IsTheHole();
  bool is_function = initial_value->IsJSFunction();
  DCHECK_EQ(1,
            BoolToInt(is_var) + BoolToInt(is_const) + BoolToInt(is_function));

  int index;
  PropertyAttributes attributes;
  BindingFlags binding_flags;

  if ((attr & EVAL_DECLARED) != 0) {
    // Check for a conflict with a lexically scoped variable
    context_arg->Lookup(name, LEXICAL_TEST, &index, &attributes,
                        &binding_flags);
    if (attributes != ABSENT &&
        (binding_flags == MUTABLE_CHECK_INITIALIZED ||
         binding_flags == IMMUTABLE_CHECK_INITIALIZED ||
         binding_flags == IMMUTABLE_CHECK_INITIALIZED_HARMONY)) {
      return ThrowRedeclarationError(isolate, name);
    }
    attr = static_cast<PropertyAttributes>(attr & ~EVAL_DECLARED);
  }

  Handle<Object> holder = context->Lookup(name, DONT_FOLLOW_CHAINS, &index,
                                          &attributes, &binding_flags);
  if (holder.is_null()) {
    // In case of JSProxy, an exception might have been thrown.
    if (isolate->has_pending_exception()) return isolate->heap()->exception();
  }

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
  if (context_arg->extension()->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global(
        JSGlobalObject::cast(context_arg->extension()), isolate);
    return DeclareGlobals(isolate, global, name, value, attr, is_var, is_const,
                          is_function);
  } else if (context->IsScriptContext()) {
    DCHECK(context->global_object()->IsJSGlobalObject());
    Handle<JSGlobalObject> global(
        JSGlobalObject::cast(context->global_object()), isolate);
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
    if (index != Context::kNotFound) {
      DCHECK(holder.is_identical_to(context));
      context->set(index, *initial_value);
      return isolate->heap()->undefined_value();
    }

    object = Handle<JSObject>::cast(holder);

  } else if (context->has_extension()) {
    // Sloppy varblock contexts might not have an extension object yet,
    // in which case their extension is a ScopeInfo.
    if (context->extension()->IsScopeInfo()) {
      DCHECK(context->IsBlockContext());
      object = isolate->factory()->NewJSObject(
          isolate->context_extension_function());
      Handle<HeapObject> extension =
          isolate->factory()->NewSloppyBlockWithEvalContextExtension(
              handle(context->scope_info()), object);
      context->set_extension(*extension);
    } else {
      object = handle(context->extension_object(), isolate);
    }
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

}  // namespace


RUNTIME_FUNCTION(Runtime_DeclareLookupSlot) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, initial_value, 1);
  CONVERT_ARG_HANDLE_CHECKED(Smi, property_attributes, 2);

  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(property_attributes->value());
  return DeclareLookupSlot(isolate, name, initial_value, attributes);
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
  if (holder.is_null()) {
    // In case of JSProxy, an exception might have been thrown.
    if (isolate->has_pending_exception()) return isolate->heap()->exception();
  }

  if (index != Context::kNotFound) {
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
    if (declaration_context->IsScriptContext()) {
      holder = handle(declaration_context->global_object(), isolate);
    } else {
      holder = handle(declaration_context->extension_object(), isolate);
      DCHECK(!holder.is_null());
    }
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


namespace {

// Find the arguments of the JavaScript function invocation that called
// into C++ code. Collect these in a newly allocated array of handles (possibly
// prefixed by a number of empty handles).
base::SmartArrayPointer<Handle<Object>> GetCallerArguments(Isolate* isolate,
                                                           int prefix_argc,
                                                           int* total_argc) {
  // Find frame containing arguments passed to the caller.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  List<JSFunction*> functions(2);
  frame->GetFunctions(&functions);
  if (functions.length() > 1) {
    int inlined_jsframe_index = functions.length() - 1;
    TranslatedState translated_values(frame);
    translated_values.Prepare(false, frame->fp());

    int argument_count = 0;
    TranslatedFrame* translated_frame =
        translated_values.GetArgumentsInfoFromJSFrameIndex(
            inlined_jsframe_index, &argument_count);
    TranslatedFrame::iterator iter = translated_frame->begin();

    // Skip the function.
    iter++;

    // Skip the receiver.
    iter++;
    argument_count--;

    *total_argc = prefix_argc + argument_count;
    base::SmartArrayPointer<Handle<Object>> param_data(
        NewArray<Handle<Object>>(*total_argc));
    bool should_deoptimize = false;
    for (int i = 0; i < argument_count; i++) {
      should_deoptimize = should_deoptimize || iter->IsMaterializedObject();
      Handle<Object> value = iter->GetValue();
      param_data[prefix_argc + i] = value;
      iter++;
    }

    if (should_deoptimize) {
      translated_values.StoreMaterializedValuesAndDeopt();
    }

    return param_data;
  } else {
    it.AdvanceToArgumentsFrame();
    frame = it.frame();
    int args_count = frame->ComputeParametersCount();

    *total_argc = prefix_argc + args_count;
    base::SmartArrayPointer<Handle<Object>> param_data(
        NewArray<Handle<Object>>(*total_argc));
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = Handle<Object>(frame->GetParameter(i), isolate);
      param_data[prefix_argc + i] = val;
    }
    return param_data;
  }
}


template <typename T>
Handle<JSObject> NewSloppyArguments(Isolate* isolate, Handle<JSFunction> callee,
                                    T parameters, int argument_count) {
  CHECK(!IsSubclassConstructor(callee->shared()->kind()));
  DCHECK(callee->shared()->has_simple_parameters());
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
      result->set_map(isolate->native_context()->fast_aliased_arguments_map());
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
        arguments->set(index, parameters[index]);
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
          arguments->set(index, parameters[index]);
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
        elements->set(i, parameters[i]);
      }
    }
  }
  return result;
}


template <typename T>
Handle<JSObject> NewStrictArguments(Isolate* isolate, Handle<JSFunction> callee,
                                    T parameters, int argument_count) {
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);

  if (argument_count > 0) {
    Handle<FixedArray> array =
        isolate->factory()->NewUninitializedFixedArray(argument_count);
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < argument_count; i++) {
      array->set(i, parameters[i], mode);
    }
    result->set_elements(*array);
  }
  return result;
}


template <typename T>
Handle<JSObject> NewRestArguments(Isolate* isolate, Handle<JSFunction> callee,
                                  T parameters, int argument_count,
                                  int start_index) {
  int num_elements = std::max(0, argument_count - start_index);
  Handle<JSObject> result = isolate->factory()->NewJSArray(
      FAST_ELEMENTS, num_elements, num_elements, Strength::WEAK,
      DONT_INITIALIZE_ARRAY_ELEMENTS);
  {
    DisallowHeapAllocation no_gc;
    FixedArray* elements = FixedArray::cast(result->elements());
    WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < num_elements; i++) {
      elements->set(i, parameters[i + start_index], mode);
    }
  }
  return result;
}


class HandleArguments BASE_EMBEDDED {
 public:
  explicit HandleArguments(Handle<Object>* array) : array_(array) {}
  Object* operator[](int index) { return *array_[index]; }

 private:
  Handle<Object>* array_;
};


class ParameterArguments BASE_EMBEDDED {
 public:
  explicit ParameterArguments(Object** parameters) : parameters_(parameters) {}
  Object*& operator[](int index) { return *(parameters_ - index - 1); }

 private:
  Object** parameters_;
};

}  // namespace


RUNTIME_FUNCTION(Runtime_NewSloppyArguments_Generic) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  // This generic runtime function can also be used when the caller has been
  // inlined, we use the slow but accurate {GetCallerArguments}.
  int argument_count = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, 0, &argument_count);
  HandleArguments argument_getter(arguments.get());
  return *NewSloppyArguments(isolate, callee, argument_getter, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewStrictArguments_Generic) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  // This generic runtime function can also be used when the caller has been
  // inlined, we use the slow but accurate {GetCallerArguments}.
  int argument_count = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, 0, &argument_count);
  HandleArguments argument_getter(arguments.get());
  return *NewStrictArguments(isolate, callee, argument_getter, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewRestArguments_Generic) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  CONVERT_SMI_ARG_CHECKED(start_index, 1);
  // This generic runtime function can also be used when the caller has been
  // inlined, we use the slow but accurate {GetCallerArguments}.
  int argument_count = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, 0, &argument_count);
  HandleArguments argument_getter(arguments.get());
  return *NewRestArguments(isolate, callee, argument_getter, argument_count,
                           start_index);
}


RUNTIME_FUNCTION(Runtime_NewSloppyArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
#ifdef DEBUG
  // This runtime function does not materialize the correct arguments when the
  // caller has been inlined, better make sure we are not hitting that case.
  JavaScriptFrameIterator it(isolate);
  DCHECK(!it.frame()->HasInlinedFrames());
#endif  // DEBUG
  ParameterArguments argument_getter(parameters);
  return *NewSloppyArguments(isolate, callee, argument_getter, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewStrictArguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);
#ifdef DEBUG
  // This runtime function does not materialize the correct arguments when the
  // caller has been inlined, better make sure we are not hitting that case.
  JavaScriptFrameIterator it(isolate);
  DCHECK(!it.frame()->HasInlinedFrames());
#endif  // DEBUG
  ParameterArguments argument_getter(parameters);
  return *NewStrictArguments(isolate, callee, argument_getter, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewRestParam) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_SMI_ARG_CHECKED(num_params, 0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(rest_index, 2);
#ifdef DEBUG
  // This runtime function does not materialize the correct arguments when the
  // caller has been inlined, better make sure we are not hitting that case.
  JavaScriptFrameIterator it(isolate);
  DCHECK(!it.frame()->HasInlinedFrames());
#endif  // DEBUG
  Handle<JSFunction> callee;
  ParameterArguments argument_getter(parameters);
  return *NewRestArguments(isolate, callee, argument_getter, num_params,
                           rest_index);
}


RUNTIME_FUNCTION(Runtime_NewClosure) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  Handle<Context> context(isolate->context(), isolate);
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                NOT_TENURED);
}


RUNTIME_FUNCTION(Runtime_NewClosure_Tenured) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  Handle<Context> context(isolate->context(), isolate);
  // The caller ensures that we pretenure closures that are assigned
  // directly to properties.
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(shared, context,
                                                                TENURED);
}

static Object* FindNameClash(Handle<ScopeInfo> scope_info,
                             Handle<JSGlobalObject> global_object,
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

      JSGlobalObject::InvalidatePropertyCell(global_object, name);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NewScriptContext) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);
  Handle<JSGlobalObject> global_object(function->context()->global_object());
  Handle<Context> native_context(global_object->native_context());
  Handle<ScriptContextTable> script_context_table(
      native_context->script_context_table());

  Object* name_clash_result =
      FindNameClash(scope_info, global_object, script_context_table);
  if (isolate->has_pending_exception()) return name_clash_result;

  // Script contexts have a canonical empty function as their closure, not the
  // anonymous closure containing the global code.  See
  // FullCodeGenerator::PushFunctionArgumentForContextAllocation.
  Handle<JSFunction> closure(
      function->shared()->IsBuiltin() ? *function : native_context->closure());
  Handle<Context> result =
      isolate->factory()->NewScriptContext(closure, scope_info);

  result->InitializeGlobalSlots();

  DCHECK(function->context() == isolate->context());
  DCHECK(*global_object == result->global_object());

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
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, extension_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 1);
  Handle<Context> current(isolate->context());
  Handle<Context> context =
      isolate->factory()->NewWithContext(function, current, extension_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushCatchContext) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, thrown_object, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 2);
  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewCatchContext(
      function, current, name, thrown_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_PushBlockContext) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 1);
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
  context->set_native_context(previous->native_context());
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
        case TEMPORARY:
        case DYNAMIC:
        case DYNAMIC_GLOBAL:
        case DYNAMIC_LOCAL:
          UNREACHABLE();
      }
    }

    if (JSObject::PreventExtensions(module, Object::THROW_ON_ERROR)
            .IsNothing()) {
      DCHECK(false);
    }
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
    // In case of JSProxy, an exception might have been thrown.
    if (isolate->has_pending_exception()) return isolate->heap()->exception();
    return isolate->heap()->true_value();
  }

  // If the slot was found in a context, it should be DONT_DELETE.
  if (holder->IsContext()) {
    return isolate->heap()->false_value();
  }

  // The slot was found in a JSReceiver, either a context extension object,
  // the global object, or the subject of a with.  Try to delete it
  // (respecting DONT_DELETE).
  Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
  Maybe<bool> result = JSReceiver::DeleteProperty(object, name);
  MAYBE_RETURN(result, isolate->heap()->exception());
  return isolate->heap()->ToBoolean(result.FromJust());
}


static Object* ComputeReceiverForNonGlobal(Isolate* isolate, JSObject* holder) {
  DCHECK(!holder->IsJSGlobalObject());

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

  if (index != Context::kNotFound) {
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
        object->IsJSGlobalObject()
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
  if (holder.is_null()) {
    // In case of JSProxy, an exception might have been thrown.
    if (isolate->has_pending_exception()) return isolate->heap()->exception();
  }

  // The property was found in a context slot.
  if (index != Context::kNotFound) {
    if ((binding_flags == MUTABLE_CHECK_INITIALIZED ||
         binding_flags == IMMUTABLE_CHECK_INITIALIZED_HARMONY) &&
        Handle<Context>::cast(holder)->is_the_hole(index)) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
    }
    if ((attributes & READ_ONLY) == 0) {
      Handle<Context>::cast(holder)->set(index, *value);
    } else if (is_strict(language_mode)) {
      // Setting read only property in strict mode.
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kStrictCannotAssign, name));
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


RUNTIME_FUNCTION(Runtime_ArgumentsLength) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  int argument_count = 0;
  GetCallerArguments(isolate, 0, &argument_count);
  return Smi::FromInt(argument_count);
}


RUNTIME_FUNCTION(Runtime_Arguments) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, raw_key, 0);

  // Determine the actual arguments passed to the function.
  int argument_count_signed = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, 0, &argument_count_signed);
  const uint32_t argument_count = argument_count_signed;

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index = 0;
  if (raw_key->ToArrayIndex(&index) && index < argument_count) {
    return *arguments[index];
  }

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
                                     Object::ToString(isolate, raw_key));
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < argument_count) {
      return *arguments[index];
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
    return Smi::FromInt(argument_count);
  }
  if (String::Equals(isolate->factory()->callee_string(), key)) {
    JavaScriptFrameIterator it(isolate);
    JSFunction* function = it.frame()->function();
    if (is_strict(function->shared()->language_mode())) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kStrictPoisonPill));
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
}  // namespace internal
}  // namespace v8
