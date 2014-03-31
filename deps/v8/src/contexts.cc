// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "bootstrapper.h"
#include "debug.h"
#include "scopeinfo.h"

namespace v8 {
namespace internal {

Context* Context::declaration_context() {
  Context* current = this;
  while (!current->IsFunctionContext() && !current->IsNativeContext()) {
    current = current->previous();
    ASSERT(current->closure() == closure());
  }
  return current;
}


JSBuiltinsObject* Context::builtins() {
  GlobalObject* object = global_object();
  if (object->IsJSGlobalObject()) {
    return JSGlobalObject::cast(object)->builtins();
  } else {
    ASSERT(object->IsJSBuiltinsObject());
    return JSBuiltinsObject::cast(object);
  }
}


Context* Context::global_context() {
  Context* current = this;
  while (!current->IsGlobalContext()) {
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
  ASSERT(this->GetIsolate()->bootstrapper()->IsActive());
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
      if (context->IsNativeContext()) PrintF(" (native context)");
      PrintF("\n");
    }

    // 1. Check global objects, subjects of with, and extension objects.
    if (context->IsNativeContext() ||
        context->IsWithContext() ||
        (context->IsFunctionContext() && context->has_extension())) {
      Handle<JSReceiver> object(
          JSReceiver::cast(context->extension()), isolate);
      // Context extension objects needs to behave as if they have no
      // prototype.  So even if we want to follow prototype chains, we need
      // to only do a local lookup for context extension objects.
      if ((flags & FOLLOW_PROTOTYPE_CHAIN) == 0 ||
          object->IsJSContextExtensionObject()) {
        *attributes = JSReceiver::GetLocalPropertyAttribute(object, name);
      } else {
        *attributes = JSReceiver::GetPropertyAttribute(object, name);
      }
      if (isolate->has_pending_exception()) return Handle<Object>();

      if (*attributes != ABSENT) {
        if (FLAG_trace_contexts) {
          PrintF("=> found property in context object %p\n",
                 reinterpret_cast<void*>(*object));
        }
        return object;
      }
    }

    // 2. Check the context proper if it has slots.
    if (context->IsFunctionContext() || context->IsBlockContext()) {
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
      int slot_index = scope_info->ContextSlotIndex(*name, &mode, &init_flag);
      ASSERT(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        if (FLAG_trace_contexts) {
          PrintF("=> found local in context slot %d (mode = %d)\n",
                 slot_index, mode);
        }
        *index = slot_index;
        // Note: Fixed context slots are statically allocated by the compiler.
        // Statically allocated variables always have a statically known mode,
        // which is the mode with which they were declared when added to the
        // scope. Thus, the DYNAMIC mode (which corresponds to dynamically
        // declared variables that were introduced through declaration nodes)
        // must not appear here.
        switch (mode) {
          case INTERNAL:  // Fall through.
          case VAR:
            *attributes = NONE;
            *binding_flags = MUTABLE_IS_INITIALIZED;
            break;
          case LET:
            *attributes = NONE;
            *binding_flags = (init_flag == kNeedsInitialization)
                ? MUTABLE_CHECK_INITIALIZED : MUTABLE_IS_INITIALIZED;
            break;
          case CONST_LEGACY:
            *attributes = READ_ONLY;
            *binding_flags = (init_flag == kNeedsInitialization)
                ? IMMUTABLE_CHECK_INITIALIZED : IMMUTABLE_IS_INITIALIZED;
            break;
          case CONST:
            *attributes = READ_ONLY;
            *binding_flags = (init_flag == kNeedsInitialization)
                ? IMMUTABLE_CHECK_INITIALIZED_HARMONY :
                IMMUTABLE_IS_INITIALIZED_HARMONY;
            break;
          case MODULE:
            *attributes = READ_ONLY;
            *binding_flags = IMMUTABLE_IS_INITIALIZED_HARMONY;
            break;
          case DYNAMIC:
          case DYNAMIC_GLOBAL:
          case DYNAMIC_LOCAL:
          case TEMPORARY:
            UNREACHABLE();
            break;
        }
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
          ASSERT(mode == CONST_LEGACY || mode == CONST);
          *binding_flags = (mode == CONST_LEGACY)
              ? IMMUTABLE_IS_INITIALIZED : IMMUTABLE_IS_INITIALIZED_HARMONY;
          return context;
        }
      }

    } else if (context->IsCatchContext()) {
      // Catch contexts have the variable name in the extension slot.
      if (name->Equals(String::cast(context->extension()))) {
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
  ASSERT(IsNativeContext());
#ifdef ENABLE_SLOW_ASSERTS
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

  ASSERT(function->next_function_link()->IsUndefined());

  function->set_next_function_link(get(OPTIMIZED_FUNCTIONS_LIST));
  set(OPTIMIZED_FUNCTIONS_LIST, function);
}


void Context::RemoveOptimizedFunction(JSFunction* function) {
  ASSERT(IsNativeContext());
  Object* element = get(OPTIMIZED_FUNCTIONS_LIST);
  JSFunction* prev = NULL;
  while (!element->IsUndefined()) {
    JSFunction* element_function = JSFunction::cast(element);
    ASSERT(element_function->next_function_link()->IsUndefined() ||
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
  ASSERT(IsNativeContext());
  set(OPTIMIZED_FUNCTIONS_LIST, head);
}


Object* Context::OptimizedFunctionsListHead() {
  ASSERT(IsNativeContext());
  return get(OPTIMIZED_FUNCTIONS_LIST);
}


void Context::AddOptimizedCode(Code* code) {
  ASSERT(IsNativeContext());
  ASSERT(code->kind() == Code::OPTIMIZED_FUNCTION);
  ASSERT(code->next_code_link()->IsUndefined());
  code->set_next_code_link(get(OPTIMIZED_CODE_LIST));
  set(OPTIMIZED_CODE_LIST, code);
}


void Context::SetOptimizedCodeListHead(Object* head) {
  ASSERT(IsNativeContext());
  set(OPTIMIZED_CODE_LIST, head);
}


Object* Context::OptimizedCodeListHead() {
  ASSERT(IsNativeContext());
  return get(OPTIMIZED_CODE_LIST);
}


void Context::SetDeoptimizedCodeListHead(Object* head) {
  ASSERT(IsNativeContext());
  set(DEOPTIMIZED_CODE_LIST, head);
}


Object* Context::DeoptimizedCodeListHead() {
  ASSERT(IsNativeContext());
  return get(DEOPTIMIZED_CODE_LIST);
}


Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Handle<Object> result(error_message_for_code_gen_from_strings(),
                        GetIsolate());
  if (!result->IsUndefined()) return result;
  return GetIsolate()->factory()->NewStringFromOneByte(STATIC_ASCII_VECTOR(
      "Code generation from strings disallowed for this context"));
}


#ifdef DEBUG
bool Context::IsBootstrappingOrValidParentContext(
    Object* object, Context* child) {
  // During bootstrapping we allow all objects to pass as
  // contexts. This is necessary to fix circular dependencies.
  if (child->GetIsolate()->bootstrapper()->IsActive()) return true;
  if (!object->IsContext()) return false;
  Context* context = Context::cast(object);
  return context->IsNativeContext() || context->IsGlobalContext() ||
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
