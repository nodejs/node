// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/bootstrapper.h"
#include "src/debug.h"
#include "src/scopeinfo.h"

namespace v8 {
namespace internal {


Handle<ScriptContextTable> ScriptContextTable::Extend(
    Handle<ScriptContextTable> table, Handle<Context> script_context) {
  Handle<ScriptContextTable> result;
  int used = table->used();
  int length = table->length();
  CHECK(used >= 0 && length > 0 && used < length);
  if (used + 1 == length) {
    CHECK(length < Smi::kMaxValue / 2);
    result = Handle<ScriptContextTable>::cast(
        FixedArray::CopySize(table, length * 2));
  } else {
    result = table;
  }
  result->set_used(used + 1);

  DCHECK(script_context->IsScriptContext());
  result->set(used + 1, *script_context);
  return result;
}


bool ScriptContextTable::Lookup(Handle<ScriptContextTable> table,
                                Handle<String> name, LookupResult* result) {
  for (int i = 0; i < table->used(); i++) {
    Handle<Context> context = GetContext(table, i);
    DCHECK(context->IsScriptContext());
    Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));
    int slot_index = ScopeInfo::ContextSlotIndex(
        scope_info, name, &result->mode, &result->init_flag,
        &result->maybe_assigned_flag);

    if (slot_index >= 0) {
      result->context_index = i;
      result->slot_index = slot_index;
      return true;
    }
  }
  return false;
}


Context* Context::declaration_context() {
  Context* current = this;
  while (!current->IsFunctionContext() && !current->IsNativeContext() &&
         !current->IsScriptContext()) {
    current = current->previous();
    DCHECK(current->closure() == closure());
  }
  return current;
}


JSBuiltinsObject* Context::builtins() {
  GlobalObject* object = global_object();
  if (object->IsJSGlobalObject()) {
    return JSGlobalObject::cast(object)->builtins();
  } else {
    DCHECK(object->IsJSBuiltinsObject());
    return JSBuiltinsObject::cast(object);
  }
}


Context* Context::script_context() {
  Context* current = this;
  while (!current->IsScriptContext()) {
    current = current->previous();
  }
  return current;
}


Context* Context::native_context() {
  // Fast case: the global object for this context has been set.  In
  // that case, the global object has a direct pointer to the global
  // context.
  if (global_object()->IsGlobalObject()) {
    return global_object()->native_context();
  }

  // During bootstrapping, the global object might not be set and we
  // have to search the context chain to find the native context.
  DCHECK(this->GetIsolate()->bootstrapper()->IsActive());
  Context* current = this;
  while (!current->IsNativeContext()) {
    JSFunction* closure = JSFunction::cast(current->closure());
    current = Context::cast(closure->context());
  }
  return current;
}


JSObject* Context::global_proxy() {
  return native_context()->global_proxy_object();
}


void Context::set_global_proxy(JSObject* object) {
  native_context()->set_global_proxy_object(object);
}


/**
 * Lookups a property in an object environment, taking the unscopables into
 * account. This is used For HasBinding spec algorithms for ObjectEnvironment.
 */
static Maybe<PropertyAttributes> UnscopableLookup(LookupIterator* it) {
  Isolate* isolate = it->isolate();

  Maybe<PropertyAttributes> attrs = JSReceiver::GetPropertyAttributes(it);
  DCHECK(attrs.has_value || isolate->has_pending_exception());
  if (!attrs.has_value || attrs.value == ABSENT) return attrs;

  Handle<Symbol> unscopables_symbol = isolate->factory()->unscopables_symbol();
  Handle<Object> receiver = it->GetReceiver();
  Handle<Object> unscopables;
  MaybeHandle<Object> maybe_unscopables =
      Object::GetProperty(receiver, unscopables_symbol);
  if (!maybe_unscopables.ToHandle(&unscopables)) {
    return Maybe<PropertyAttributes>();
  }
  if (!unscopables->IsSpecObject()) return attrs;
  Handle<Object> blacklist;
  MaybeHandle<Object> maybe_blacklist =
      Object::GetProperty(unscopables, it->name());
  if (!maybe_blacklist.ToHandle(&blacklist)) {
    DCHECK(isolate->has_pending_exception());
    return Maybe<PropertyAttributes>();
  }
  if (!blacklist->IsUndefined()) return maybe(ABSENT);
  return attrs;
}

static void GetAttributesAndBindingFlags(VariableMode mode,
                                         InitializationFlag init_flag,
                                         PropertyAttributes* attributes,
                                         BindingFlags* binding_flags) {
  switch (mode) {
    case INTERNAL:  // Fall through.
    case VAR:
      *attributes = NONE;
      *binding_flags = MUTABLE_IS_INITIALIZED;
      break;
    case LET:
      *attributes = NONE;
      *binding_flags = (init_flag == kNeedsInitialization)
                           ? MUTABLE_CHECK_INITIALIZED
                           : MUTABLE_IS_INITIALIZED;
      break;
    case CONST_LEGACY:
      *attributes = READ_ONLY;
      *binding_flags = (init_flag == kNeedsInitialization)
                           ? IMMUTABLE_CHECK_INITIALIZED
                           : IMMUTABLE_IS_INITIALIZED;
      break;
    case CONST:
      *attributes = READ_ONLY;
      *binding_flags = (init_flag == kNeedsInitialization)
                           ? IMMUTABLE_CHECK_INITIALIZED_HARMONY
                           : IMMUTABLE_IS_INITIALIZED_HARMONY;
      break;
    case MODULE:
      *attributes = READ_ONLY;
      *binding_flags = IMMUTABLE_IS_INITIALIZED_HARMONY;
      break;
    case DYNAMIC:
    case DYNAMIC_GLOBAL:
    case DYNAMIC_LOCAL:
    case TEMPORARY:
      // Note: Fixed context slots are statically allocated by the compiler.
      // Statically allocated variables always have a statically known mode,
      // which is the mode with which they were declared when added to the
      // scope. Thus, the DYNAMIC mode (which corresponds to dynamically
      // declared variables that were introduced through declaration nodes)
      // must not appear here.
      UNREACHABLE();
      break;
  }
}


Handle<Object> Context::Lookup(Handle<String> name,
                               ContextLookupFlags flags,
                               int* index,
                               PropertyAttributes* attributes,
                               BindingFlags* binding_flags) {
  Isolate* isolate = GetIsolate();
  Handle<Context> context(this, isolate);

  bool follow_context_chain = (flags & FOLLOW_CONTEXT_CHAIN) != 0;
  *index = -1;
  *attributes = ABSENT;
  *binding_flags = MISSING_BINDING;

  if (FLAG_trace_contexts) {
    PrintF("Context::Lookup(");
    name->ShortPrint();
    PrintF(")\n");
  }

  do {
    if (FLAG_trace_contexts) {
      PrintF(" - looking in context %p", reinterpret_cast<void*>(*context));
      if (context->IsScriptContext()) PrintF(" (script context)");
      if (context->IsNativeContext()) PrintF(" (native context)");
      PrintF("\n");
    }


    // 1. Check global objects, subjects of with, and extension objects.
    if (context->IsNativeContext() ||
        context->IsWithContext() ||
        (context->IsFunctionContext() && context->has_extension())) {
      Handle<JSReceiver> object(
          JSReceiver::cast(context->extension()), isolate);

      if (context->IsNativeContext()) {
        if (FLAG_trace_contexts) {
          PrintF(" - trying other script contexts\n");
        }
        // Try other script contexts.
        Handle<ScriptContextTable> script_contexts(
            context->global_object()->native_context()->script_context_table());
        ScriptContextTable::LookupResult r;
        if (ScriptContextTable::Lookup(script_contexts, name, &r)) {
          if (FLAG_trace_contexts) {
            Handle<Context> c = ScriptContextTable::GetContext(script_contexts,
                                                               r.context_index);
            PrintF("=> found property in script context %d: %p\n",
                   r.context_index, reinterpret_cast<void*>(*c));
          }
          *index = r.slot_index;
          GetAttributesAndBindingFlags(r.mode, r.init_flag, attributes,
                                       binding_flags);
          return ScriptContextTable::GetContext(script_contexts,
                                                r.context_index);
        }
      }

      // Context extension objects needs to behave as if they have no
      // prototype.  So even if we want to follow prototype chains, we need
      // to only do a local lookup for context extension objects.
      Maybe<PropertyAttributes> maybe;
      if ((flags & FOLLOW_PROTOTYPE_CHAIN) == 0 ||
          object->IsJSContextExtensionObject()) {
        maybe = JSReceiver::GetOwnPropertyAttributes(object, name);
      } else if (context->IsWithContext()) {
        LookupIterator it(object, name);
        maybe = UnscopableLookup(&it);
      } else {
        maybe = JSReceiver::GetPropertyAttributes(object, name);
      }

      if (!maybe.has_value) return Handle<Object>();
      DCHECK(!isolate->has_pending_exception());
      *attributes = maybe.value;

      if (maybe.value != ABSENT) {
        if (FLAG_trace_contexts) {
          PrintF("=> found property in context object %p\n",
                 reinterpret_cast<void*>(*object));
        }
        return object;
      }
    }

    // 2. Check the context proper if it has slots.
    if (context->IsFunctionContext() || context->IsBlockContext() ||
        (FLAG_harmony_scoping && context->IsScriptContext())) {
      // Use serialized scope information of functions and blocks to search
      // for the context index.
      Handle<ScopeInfo> scope_info;
      if (context->IsFunctionContext()) {
        scope_info = Handle<ScopeInfo>(
            context->closure()->shared()->scope_info(), isolate);
      } else {
        scope_info = Handle<ScopeInfo>(
            ScopeInfo::cast(context->extension()), isolate);
      }
      VariableMode mode;
      InitializationFlag init_flag;
      // TODO(sigurds) Figure out whether maybe_assigned_flag should
      // be used to compute binding_flags.
      MaybeAssignedFlag maybe_assigned_flag;
      int slot_index = ScopeInfo::ContextSlotIndex(
          scope_info, name, &mode, &init_flag, &maybe_assigned_flag);
      DCHECK(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        if (FLAG_trace_contexts) {
          PrintF("=> found local in context slot %d (mode = %d)\n",
                 slot_index, mode);
        }
        *index = slot_index;
        GetAttributesAndBindingFlags(mode, init_flag, attributes,
                                     binding_flags);
        return context;
      }

      // Check the slot corresponding to the intermediate context holding
      // only the function name variable.
      if (follow_context_chain && context->IsFunctionContext()) {
        VariableMode mode;
        int function_index = scope_info->FunctionContextSlotIndex(*name, &mode);
        if (function_index >= 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found intermediate function in context slot %d\n",
                   function_index);
          }
          *index = function_index;
          *attributes = READ_ONLY;
          DCHECK(mode == CONST_LEGACY || mode == CONST);
          *binding_flags = (mode == CONST_LEGACY)
              ? IMMUTABLE_IS_INITIALIZED : IMMUTABLE_IS_INITIALIZED_HARMONY;
          return context;
        }
      }

    } else if (context->IsCatchContext()) {
      // Catch contexts have the variable name in the extension slot.
      if (String::Equals(name, handle(String::cast(context->extension())))) {
        if (FLAG_trace_contexts) {
          PrintF("=> found in catch context\n");
        }
        *index = Context::THROWN_OBJECT_INDEX;
        *attributes = NONE;
        *binding_flags = MUTABLE_IS_INITIALIZED;
        return context;
      }
    }

    // 3. Prepare to continue with the previous (next outermost) context.
    if (context->IsNativeContext()) {
      follow_context_chain = false;
    } else {
      context = Handle<Context>(context->previous(), isolate);
    }
  } while (follow_context_chain);

  if (FLAG_trace_contexts) {
    PrintF("=> no property/slot found\n");
  }
  return Handle<Object>::null();
}


void Context::AddOptimizedFunction(JSFunction* function) {
  DCHECK(IsNativeContext());
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    Object* element = get(OPTIMIZED_FUNCTIONS_LIST);
    while (!element->IsUndefined()) {
      CHECK(element != function);
      element = JSFunction::cast(element)->next_function_link();
    }
  }

  // Check that the context belongs to the weak native contexts list.
  bool found = false;
  Object* context = GetHeap()->native_contexts_list();
  while (!context->IsUndefined()) {
    if (context == this) {
      found = true;
      break;
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
  CHECK(found);
#endif

  // If the function link field is already used then the function was
  // enqueued as a code flushing candidate and we remove it now.
  if (!function->next_function_link()->IsUndefined()) {
    CodeFlusher* flusher = GetHeap()->mark_compact_collector()->code_flusher();
    flusher->EvictCandidate(function);
  }

  DCHECK(function->next_function_link()->IsUndefined());

  function->set_next_function_link(get(OPTIMIZED_FUNCTIONS_LIST));
  set(OPTIMIZED_FUNCTIONS_LIST, function);
}


void Context::RemoveOptimizedFunction(JSFunction* function) {
  DCHECK(IsNativeContext());
  Object* element = get(OPTIMIZED_FUNCTIONS_LIST);
  JSFunction* prev = NULL;
  while (!element->IsUndefined()) {
    JSFunction* element_function = JSFunction::cast(element);
    DCHECK(element_function->next_function_link()->IsUndefined() ||
           element_function->next_function_link()->IsJSFunction());
    if (element_function == function) {
      if (prev == NULL) {
        set(OPTIMIZED_FUNCTIONS_LIST, element_function->next_function_link());
      } else {
        prev->set_next_function_link(element_function->next_function_link());
      }
      element_function->set_next_function_link(GetHeap()->undefined_value());
      return;
    }
    prev = element_function;
    element = element_function->next_function_link();
  }
  UNREACHABLE();
}


void Context::SetOptimizedFunctionsListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(OPTIMIZED_FUNCTIONS_LIST, head);
}


Object* Context::OptimizedFunctionsListHead() {
  DCHECK(IsNativeContext());
  return get(OPTIMIZED_FUNCTIONS_LIST);
}


void Context::AddOptimizedCode(Code* code) {
  DCHECK(IsNativeContext());
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(code->next_code_link()->IsUndefined());
  code->set_next_code_link(get(OPTIMIZED_CODE_LIST));
  set(OPTIMIZED_CODE_LIST, code);
}


void Context::SetOptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(OPTIMIZED_CODE_LIST, head);
}


Object* Context::OptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(OPTIMIZED_CODE_LIST);
}


void Context::SetDeoptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(DEOPTIMIZED_CODE_LIST, head);
}


Object* Context::DeoptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(DEOPTIMIZED_CODE_LIST);
}


Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Isolate* isolate = GetIsolate();
  Handle<Object> result(error_message_for_code_gen_from_strings(), isolate);
  if (!result->IsUndefined()) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Code generation from strings disallowed for this context");
}


#ifdef DEBUG
bool Context::IsBootstrappingOrValidParentContext(
    Object* object, Context* child) {
  // During bootstrapping we allow all objects to pass as
  // contexts. This is necessary to fix circular dependencies.
  if (child->GetIsolate()->bootstrapper()->IsActive()) return true;
  if (!object->IsContext()) return false;
  Context* context = Context::cast(object);
  return context->IsNativeContext() || context->IsScriptContext() ||
         context->IsModuleContext() || !child->IsModuleContext();
}


bool Context::IsBootstrappingOrGlobalObject(Isolate* isolate, Object* object) {
  // During bootstrapping we allow all objects to pass as global
  // objects. This is necessary to fix circular dependencies.
  return isolate->heap()->gc_state() != Heap::NOT_IN_GC ||
      isolate->bootstrapper()->IsActive() ||
      object->IsGlobalObject();
}
#endif

} }  // namespace v8::internal
