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
      if (old_details.IsReadOnly() || old_details.IsDontEnum() ||
          (it.state() == LookupIterator::ACCESSOR &&
           it.GetAccessors()->IsAccessorPair())) {
        return ThrowRedeclarationError(isolate, name);
      }
      // If the existing property is not configurable, keep its attributes. Do
      attr = old_attributes;
    }

    // If the current state is ACCESSOR, this could mean it's an AccessorInfo
    // type property. We are not allowed to call into such setters during global
    // function declaration since this would break e.g., onload. Meaning
    // 'function onload() {}' would invalidly register that function as the
    // onload callback. To avoid this situation, we first delete the property
    // before readding it as a regular data property below.
    if (it.state() == LookupIterator::ACCESSOR) it.Delete();
  }

  // Define or redefine own property.
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attr));

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

  RETURN_FAILURE_ON_EXCEPTION(
      isolate, JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, attr));

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
// into C++ code. Collect these in a newly allocated array of handles.
base::SmartArrayPointer<Handle<Object>> GetCallerArguments(Isolate* isolate,
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

    *total_argc = argument_count;
    base::SmartArrayPointer<Handle<Object>> param_data(
        NewArray<Handle<Object>>(*total_argc));
    bool should_deoptimize = false;
    for (int i = 0; i < argument_count; i++) {
      should_deoptimize = should_deoptimize || iter->IsMaterializedObject();
      Handle<Object> value = iter->GetValue();
      param_data[i] = value;
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

    *total_argc = args_count;
    base::SmartArrayPointer<Handle<Object>> param_data(
        NewArray<Handle<Object>>(*total_argc));
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = Handle<Object>(frame->GetParameter(i), isolate);
      param_data[i] = val;
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
      GetCallerArguments(isolate, &argument_count);
  HandleArguments argument_getter(arguments.get());
  return *NewSloppyArguments(isolate, callee, argument_getter, argument_count);
}


RUNTIME_FUNCTION(Runtime_NewStrictArguments) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  // This generic runtime function can also be used when the caller has been
  // inlined, we use the slow but accurate {GetCallerArguments}.
  int argument_count = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, &argument_count);
  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);
  if (argument_count) {
    Handle<FixedArray> array =
        isolate->factory()->NewUninitializedFixedArray(argument_count);
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < argument_count; i++) {
      array->set(i, *arguments[i], mode);
    }
    result->set_elements(*array);
  }
  return *result;
}


RUNTIME_FUNCTION(Runtime_NewRestParameter) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  int start_index = callee->shared()->internal_formal_parameter_count();
  // This generic runtime function can also be used when the caller has been
  // inlined, we use the slow but accurate {GetCallerArguments}.
  int argument_count = 0;
  base::SmartArrayPointer<Handle<Object>> arguments =
      GetCallerArguments(isolate, &argument_count);
  int num_elements = std::max(0, argument_count - start_index);
  Handle<JSObject> result = isolate->factory()->NewJSArray(
      FAST_ELEMENTS, num_elements, num_elements, Strength::WEAK,
      DONT_INITIALIZE_ARRAY_ELEMENTS);
  {
    DisallowHeapAllocation no_gc;
    FixedArray* elements = FixedArray::cast(result->elements());
    WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < num_elements; i++) {
      elements->set(i, *arguments[i + start_index], mode);
    }
  }
  return *result;
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
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);

  int index;
  PropertyAttributes attributes;
  BindingFlags flags;
  Handle<Object> holder = isolate->context()->Lookup(
      name, FOLLOW_CHAINS, &index, &attributes, &flags);

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


namespace {

MaybeHandle<Object> LoadLookupSlot(Handle<String> name,
                                   Object::ShouldThrow should_throw,
                                   Handle<Object>* receiver_return = nullptr) {
  Isolate* const isolate = name->GetIsolate();

  int index;
  PropertyAttributes attributes;
  BindingFlags flags;
  Handle<Object> holder = isolate->context()->Lookup(
      name, FOLLOW_CHAINS, &index, &attributes, &flags);
  if (isolate->has_pending_exception()) return MaybeHandle<Object>();

  if (index != Context::kNotFound) {
    DCHECK(holder->IsContext());
    // If the "property" we were looking for is a local variable, the
    // receiver is the global object; see ECMA-262, 3rd., 10.1.6 and 10.2.3.
    Handle<Object> receiver = isolate->factory()->undefined_value();
    Handle<Object> value = handle(Context::cast(*holder)->get(index), isolate);
    // Check for uninitialized bindings.
    switch (flags) {
      case MUTABLE_CHECK_INITIALIZED:
      case IMMUTABLE_CHECK_INITIALIZED_HARMONY:
        if (value->IsTheHole()) {
          THROW_NEW_ERROR(isolate,
                          NewReferenceError(MessageTemplate::kNotDefined, name),
                          Object);
        }
      // FALLTHROUGH
      case IMMUTABLE_CHECK_INITIALIZED:
        if (value->IsTheHole()) {
          DCHECK(attributes & READ_ONLY);
          value = isolate->factory()->undefined_value();
        }
      // FALLTHROUGH
      case MUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED_HARMONY:
        DCHECK(!value->IsTheHole());
        if (receiver_return) *receiver_return = receiver;
        return value;
      case MISSING_BINDING:
        break;
    }
    UNREACHABLE();
  }

  // Otherwise, if the slot was found the holder is a context extension
  // object, subject of a with, or a global object.  We read the named
  // property from it.
  if (!holder.is_null()) {
    // No need to unhole the value here.  This is taken care of by the
    // GetProperty function.
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, value, Object::GetProperty(holder, name),
        Object);
    if (receiver_return) {
      *receiver_return =
          (holder->IsJSGlobalObject() || holder->IsJSContextExtensionObject())
              ? Handle<Object>::cast(isolate->factory()->undefined_value())
              : holder;
    }
    return value;
  }

  if (should_throw == Object::THROW_ON_ERROR) {
    // The property doesn't exist - throw exception.
    THROW_NEW_ERROR(
        isolate, NewReferenceError(MessageTemplate::kNotDefined, name), Object);
  }

  // The property doesn't exist - return undefined.
  if (receiver_return) *receiver_return = isolate->factory()->undefined_value();
  return isolate->factory()->undefined_value();
}

}  // namespace


RUNTIME_FUNCTION(Runtime_LoadLookupSlot) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, value, LoadLookupSlot(name, Object::THROW_ON_ERROR));
  return *value;
}


RUNTIME_FUNCTION(Runtime_LoadLookupSlotInsideTypeof) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, value, LoadLookupSlot(name, Object::DONT_THROW));
  return *value;
}


RUNTIME_FUNCTION_RETURN_PAIR(Runtime_LoadLookupSlotForCall) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  DCHECK(args[0]->IsString());
  Handle<String> name = args.at<String>(0);
  Handle<Object> value;
  Handle<Object> receiver;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, value, LoadLookupSlot(name, Object::THROW_ON_ERROR, &receiver),
      MakePair(isolate->heap()->exception(), nullptr));
  return MakePair(*value, *receiver);
}


namespace {

MaybeHandle<Object> StoreLookupSlot(Handle<String> name, Handle<Object> value,
                                    LanguageMode language_mode) {
  Isolate* const isolate = name->GetIsolate();
  Handle<Context> context(isolate->context(), isolate);

  int index;
  PropertyAttributes attributes;
  BindingFlags flags;
  Handle<Object> holder =
      context->Lookup(name, FOLLOW_CHAINS, &index, &attributes, &flags);
  if (holder.is_null()) {
    // In case of JSProxy, an exception might have been thrown.
    if (isolate->has_pending_exception()) return MaybeHandle<Object>();
  }

  // The property was found in a context slot.
  if (index != Context::kNotFound) {
    if ((flags == MUTABLE_CHECK_INITIALIZED ||
         flags == IMMUTABLE_CHECK_INITIALIZED_HARMONY) &&
        Handle<Context>::cast(holder)->is_the_hole(index)) {
      THROW_NEW_ERROR(isolate,
                      NewReferenceError(MessageTemplate::kNotDefined, name),
                      Object);
    }
    if ((attributes & READ_ONLY) == 0) {
      Handle<Context>::cast(holder)->set(index, *value);
    } else if (is_strict(language_mode)) {
      // Setting read only property in strict mode.
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kStrictCannotAssign, name),
                      Object);
    }
    return value;
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
    THROW_NEW_ERROR(
        isolate, NewReferenceError(MessageTemplate::kNotDefined, name), Object);
  } else {
    // If absent in sloppy mode: add the property to the global object.
    object = Handle<JSReceiver>(context->global_object());
  }

  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, value, Object::SetProperty(object, name, value, language_mode),
      Object);
  return value;
}

}  // namespace


RUNTIME_FUNCTION(Runtime_StoreLookupSlot_Sloppy) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     StoreLookupSlot(name, value, SLOPPY));
  return *value;
}


RUNTIME_FUNCTION(Runtime_StoreLookupSlot_Strict) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, value,
                                     StoreLookupSlot(name, value, STRICT));
  return *value;
}

}  // namespace internal
}  // namespace v8
