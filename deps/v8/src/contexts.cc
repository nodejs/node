// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/contexts.h"

#include "src/bootstrapper.h"
#include "src/debug/debug.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {


Handle<ScriptContextTable> ScriptContextTable::Extend(
    Handle<ScriptContextTable> table, Handle<Context> script_context) {
  Handle<ScriptContextTable> result;
  int used = table->used();
  int length = table->length();
  CHECK(used >= 0 && length > 0 && used < length);
  if (used + kFirstContextSlot == length) {
    CHECK(length < Smi::kMaxValue / 2);
    Isolate* isolate = table->GetIsolate();
    Handle<FixedArray> copy =
        isolate->factory()->CopyFixedArrayAndGrow(table, length);
    copy->set_map(isolate->heap()->script_context_table_map());
    result = Handle<ScriptContextTable>::cast(copy);
  } else {
    result = table;
  }
  result->set_used(used + 1);

  DCHECK(script_context->IsScriptContext());
  result->set(used + kFirstContextSlot, *script_context);
  return result;
}


bool ScriptContextTable::Lookup(Handle<ScriptContextTable> table,
                                Handle<String> name, LookupResult* result) {
  for (int i = 0; i < table->used(); i++) {
    Handle<Context> context = GetContext(table, i);
    DCHECK(context->IsScriptContext());
    Handle<ScopeInfo> scope_info(context->scope_info());
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


bool Context::is_declaration_context() {
  if (IsFunctionContext() || IsNativeContext() || IsScriptContext() ||
      IsModuleContext()) {
    return true;
  }
  if (IsEvalContext()) return closure()->shared()->language_mode() == STRICT;
  if (!IsBlockContext()) return false;
  Object* ext = extension();
  // If we have the special extension, we immediately know it must be a
  // declaration scope. That's just a small performance shortcut.
  return ext->IsContextExtension() ||
         ScopeInfo::cast(ext)->is_declaration_scope();
}


Context* Context::declaration_context() {
  Context* current = this;
  while (!current->is_declaration_context()) {
    current = current->previous();
  }
  return current;
}

Context* Context::closure_context() {
  Context* current = this;
  while (!current->IsFunctionContext() && !current->IsScriptContext() &&
         !current->IsModuleContext() && !current->IsNativeContext() &&
         !current->IsEvalContext()) {
    current = current->previous();
    DCHECK(current->closure() == closure());
  }
  return current;
}

JSObject* Context::extension_object() {
  DCHECK(IsNativeContext() || IsFunctionContext() || IsBlockContext() ||
         IsEvalContext());
  HeapObject* object = extension();
  if (object->IsTheHole(GetIsolate())) return nullptr;
  if (IsBlockContext()) {
    if (!object->IsContextExtension()) return nullptr;
    object = JSObject::cast(ContextExtension::cast(object)->extension());
  }
  DCHECK(object->IsJSContextExtensionObject() ||
         (IsNativeContext() && object->IsJSGlobalObject()));
  return JSObject::cast(object);
}

JSReceiver* Context::extension_receiver() {
  DCHECK(IsNativeContext() || IsWithContext() || IsEvalContext() ||
         IsFunctionContext() || IsBlockContext());
  return IsWithContext() ? JSReceiver::cast(
                               ContextExtension::cast(extension())->extension())
                         : extension_object();
}

ScopeInfo* Context::scope_info() {
  DCHECK(!IsNativeContext());
  if (IsFunctionContext() || IsModuleContext() || IsEvalContext()) {
    return closure()->shared()->scope_info();
  }
  HeapObject* object = extension();
  if (object->IsContextExtension()) {
    DCHECK(IsBlockContext() || IsCatchContext() || IsWithContext() ||
           IsDebugEvaluateContext());
    object = ContextExtension::cast(object)->scope_info();
  }
  return ScopeInfo::cast(object);
}

Module* Context::module() {
  Context* current = this;
  while (!current->IsModuleContext()) {
    current = current->previous();
  }
  return Module::cast(current->extension());
}

String* Context::catch_name() {
  DCHECK(IsCatchContext());
  return String::cast(ContextExtension::cast(extension())->extension());
}


JSGlobalObject* Context::global_object() {
  return JSGlobalObject::cast(native_context()->extension());
}


Context* Context::script_context() {
  Context* current = this;
  while (!current->IsScriptContext()) {
    current = current->previous();
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
static Maybe<bool> UnscopableLookup(LookupIterator* it) {
  Isolate* isolate = it->isolate();

  Maybe<bool> found = JSReceiver::HasProperty(it);
  if (!found.IsJust() || !found.FromJust()) return found;

  Handle<Object> unscopables;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, unscopables,
      JSReceiver::GetProperty(Handle<JSReceiver>::cast(it->GetReceiver()),
                              isolate->factory()->unscopables_symbol()),
      Nothing<bool>());
  if (!unscopables->IsJSReceiver()) return Just(true);
  Handle<Object> blacklist;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, blacklist,
      JSReceiver::GetProperty(Handle<JSReceiver>::cast(unscopables),
                              it->name()),
      Nothing<bool>());
  return Just(!blacklist->BooleanValue());
}

static PropertyAttributes GetAttributesForMode(VariableMode mode) {
  DCHECK(IsDeclaredVariableMode(mode));
  return mode == CONST ? READ_ONLY : NONE;
}

Handle<Object> Context::Lookup(Handle<String> name, ContextLookupFlags flags,
                               int* index, PropertyAttributes* attributes,
                               InitializationFlag* init_flag,
                               VariableMode* variable_mode) {
  DCHECK(!IsModuleContext());
  Isolate* isolate = GetIsolate();
  Handle<Context> context(this, isolate);

  bool follow_context_chain = (flags & FOLLOW_CONTEXT_CHAIN) != 0;
  bool failed_whitelist = false;
  *index = kNotFound;
  *attributes = ABSENT;
  *init_flag = kCreatedInitialized;
  *variable_mode = VAR;

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
    DCHECK_IMPLIES(context->IsEvalContext(),
                   context->extension()->IsTheHole(isolate));
    if ((context->IsNativeContext() ||
         (context->IsWithContext() && ((flags & SKIP_WITH_CONTEXT) == 0)) ||
         context->IsFunctionContext() || context->IsBlockContext()) &&
        context->extension_receiver() != nullptr) {
      Handle<JSReceiver> object(context->extension_receiver());

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
          *variable_mode = r.mode;
          *init_flag = r.init_flag;
          *attributes = GetAttributesForMode(r.mode);
          return ScriptContextTable::GetContext(script_contexts,
                                                r.context_index);
        }
      }

      // Context extension objects needs to behave as if they have no
      // prototype.  So even if we want to follow prototype chains, we need
      // to only do a local lookup for context extension objects.
      Maybe<PropertyAttributes> maybe = Nothing<PropertyAttributes>();
      if ((flags & FOLLOW_PROTOTYPE_CHAIN) == 0 ||
          object->IsJSContextExtensionObject()) {
        maybe = JSReceiver::GetOwnPropertyAttributes(object, name);
      } else if (context->IsWithContext()) {
        // A with context will never bind "this", but debug-eval may look into
        // a with context when resolving "this". Other synthetic variables such
        // as new.target may be resolved as DYNAMIC_LOCAL due to bug v8:5405 ,
        // skipping them here serves as a workaround until a more thorough
        // fix can be applied.
        // TODO(v8:5405): Replace this check with a DCHECK when resolution of
        // of synthetic variables does not go through this code path.
        if (ScopeInfo::VariableIsSynthetic(*name)) {
          maybe = Just(ABSENT);
        } else {
          LookupIterator it(object, name, object);
          Maybe<bool> found = UnscopableLookup(&it);
          if (found.IsNothing()) {
            maybe = Nothing<PropertyAttributes>();
          } else {
            // Luckily, consumers of |maybe| only care whether the property
            // was absent or not, so we can return a dummy |NONE| value
            // for its attributes when it was present.
            maybe = Just(found.FromJust() ? NONE : ABSENT);
          }
        }
      } else {
        maybe = JSReceiver::GetPropertyAttributes(object, name);
      }

      if (!maybe.IsJust()) return Handle<Object>();
      DCHECK(!isolate->has_pending_exception());
      *attributes = maybe.FromJust();

      if (maybe.FromJust() != ABSENT) {
        if (FLAG_trace_contexts) {
          PrintF("=> found property in context object %p\n",
                 reinterpret_cast<void*>(*object));
        }
        return object;
      }
    }

    // 2. Check the context proper if it has slots.
    if (context->IsFunctionContext() || context->IsBlockContext() ||
        context->IsScriptContext() || context->IsEvalContext()) {
      // Use serialized scope information of functions and blocks to search
      // for the context index.
      Handle<ScopeInfo> scope_info(context->scope_info());
      VariableMode mode;
      InitializationFlag flag;
      MaybeAssignedFlag maybe_assigned_flag;
      int slot_index = ScopeInfo::ContextSlotIndex(scope_info, name, &mode,
                                                   &flag, &maybe_assigned_flag);
      DCHECK(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        if (FLAG_trace_contexts) {
          PrintF("=> found local in context slot %d (mode = %d)\n",
                 slot_index, mode);
        }
        *index = slot_index;
        *variable_mode = mode;
        *init_flag = flag;
        *attributes = GetAttributesForMode(mode);
        return context;
      }

      // Check the slot corresponding to the intermediate context holding
      // only the function name variable. It's conceptually (and spec-wise)
      // in an outer scope of the function's declaration scope.
      if (follow_context_chain && (flags & STOP_AT_DECLARATION_SCOPE) == 0 &&
          context->IsFunctionContext()) {
        int function_index = scope_info->FunctionContextSlotIndex(*name);
        if (function_index >= 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found intermediate function in context slot %d\n",
                   function_index);
          }
          *index = function_index;
          *attributes = READ_ONLY;
          *init_flag = kCreatedInitialized;
          *variable_mode = CONST;
          return context;
        }
      }

    } else if (context->IsCatchContext()) {
      // Catch contexts have the variable name in the extension slot.
      if (String::Equals(name, handle(context->catch_name()))) {
        if (FLAG_trace_contexts) {
          PrintF("=> found in catch context\n");
        }
        *index = Context::THROWN_OBJECT_INDEX;
        *attributes = NONE;
        *init_flag = kCreatedInitialized;
        *variable_mode = VAR;
        return context;
      }
    } else if (context->IsDebugEvaluateContext()) {
      // Check materialized locals.
      Object* ext = context->get(EXTENSION_INDEX);
      if (ext->IsContextExtension()) {
        Object* obj = ContextExtension::cast(ext)->extension();
        if (obj->IsJSReceiver()) {
          Handle<JSReceiver> extension(JSReceiver::cast(obj));
          LookupIterator it(extension, name, extension);
          Maybe<bool> found = JSReceiver::HasProperty(&it);
          if (found.FromMaybe(false)) {
            *attributes = NONE;
            return extension;
          }
        }
      }
      // Check the original context, but do not follow its context chain.
      Object* obj = context->get(WRAPPED_CONTEXT_INDEX);
      if (obj->IsContext()) {
        Handle<Object> result =
            Context::cast(obj)->Lookup(name, DONT_FOLLOW_CHAINS, index,
                                       attributes, init_flag, variable_mode);
        if (!result.is_null()) return result;
      }
      // Check whitelist. Names that do not pass whitelist shall only resolve
      // to with, script or native contexts up the context chain.
      obj = context->get(WHITE_LIST_INDEX);
      if (obj->IsStringSet()) {
        failed_whitelist = failed_whitelist || !StringSet::cast(obj)->Has(name);
      }
    }

    // 3. Prepare to continue with the previous (next outermost) context.
    if (context->IsNativeContext() ||
        ((flags & STOP_AT_DECLARATION_SCOPE) != 0 &&
         context->is_declaration_context())) {
      follow_context_chain = false;
    } else {
      do {
        context = Handle<Context>(context->previous(), isolate);
        // If we come across a whitelist context, and the name is not
        // whitelisted, then only consider with, script or native contexts.
      } while (failed_whitelist && !context->IsScriptContext() &&
               !context->IsNativeContext() && !context->IsWithContext());
    }
  } while (follow_context_chain);

  if (FLAG_trace_contexts) {
    PrintF("=> no property/slot found\n");
  }
  return Handle<Object>::null();
}

static const int kSharedOffset = 0;
static const int kCachedCodeOffset = 1;
static const int kLiteralsOffset = 2;
static const int kOsrAstIdOffset = 3;
static const int kEntryLength = 4;
static const int kInitialLength = kEntryLength;

int Context::SearchOptimizedCodeMapEntry(SharedFunctionInfo* shared,
                                         BailoutId osr_ast_id) {
  DisallowHeapAllocation no_gc;
  DCHECK(this->IsNativeContext());
  if (!OptimizedCodeMapIsCleared()) {
    FixedArray* optimized_code_map = this->osr_code_table();
    int length = optimized_code_map->length();
    Smi* osr_ast_id_smi = Smi::FromInt(osr_ast_id.ToInt());
    for (int i = 0; i < length; i += kEntryLength) {
      if (WeakCell::cast(optimized_code_map->get(i + kSharedOffset))->value() ==
              shared &&
          optimized_code_map->get(i + kOsrAstIdOffset) == osr_ast_id_smi) {
        return i;
      }
    }
  }
  return -1;
}

void Context::SearchOptimizedCodeMap(SharedFunctionInfo* shared,
                                     BailoutId osr_ast_id, Code** pcode,
                                     LiteralsArray** pliterals) {
  DCHECK(this->IsNativeContext());
  int entry = SearchOptimizedCodeMapEntry(shared, osr_ast_id);
  if (entry != -1) {
    FixedArray* code_map = osr_code_table();
    DCHECK_LE(entry + kEntryLength, code_map->length());
    WeakCell* cell = WeakCell::cast(code_map->get(entry + kCachedCodeOffset));
    WeakCell* literals_cell =
        WeakCell::cast(code_map->get(entry + kLiteralsOffset));

    *pcode = cell->cleared() ? nullptr : Code::cast(cell->value());
    *pliterals = literals_cell->cleared()
                     ? nullptr
                     : LiteralsArray::cast(literals_cell->value());
  } else {
    *pcode = nullptr;
    *pliterals = nullptr;
  }
}

void Context::AddToOptimizedCodeMap(Handle<Context> native_context,
                                    Handle<SharedFunctionInfo> shared,
                                    Handle<Code> code,
                                    Handle<LiteralsArray> literals,
                                    BailoutId osr_ast_id) {
  DCHECK(native_context->IsNativeContext());
  Isolate* isolate = native_context->GetIsolate();
  if (isolate->serializer_enabled()) return;

  STATIC_ASSERT(kEntryLength == 4);
  Handle<FixedArray> new_code_map;
  int entry;

  if (native_context->OptimizedCodeMapIsCleared()) {
    new_code_map = isolate->factory()->NewFixedArray(kInitialLength, TENURED);
    entry = 0;
  } else {
    Handle<FixedArray> old_code_map(native_context->osr_code_table(), isolate);
    entry = native_context->SearchOptimizedCodeMapEntry(*shared, osr_ast_id);
    if (entry >= 0) {
      // Just set the code and literals of the entry.
      Handle<WeakCell> code_cell = isolate->factory()->NewWeakCell(code);
      old_code_map->set(entry + kCachedCodeOffset, *code_cell);
      Handle<WeakCell> literals_cell =
          isolate->factory()->NewWeakCell(literals);
      old_code_map->set(entry + kLiteralsOffset, *literals_cell);
      return;
    }

    // Can we reuse an entry?
    DCHECK(entry < 0);
    int length = old_code_map->length();
    for (int i = 0; i < length; i += kEntryLength) {
      if (WeakCell::cast(old_code_map->get(i + kSharedOffset))->cleared()) {
        new_code_map = old_code_map;
        entry = i;
        break;
      }
    }

    if (entry < 0) {
      // Copy old optimized code map and append one new entry.
      new_code_map = isolate->factory()->CopyFixedArrayAndGrow(
          old_code_map, kEntryLength, TENURED);
      entry = old_code_map->length();
    }
  }

  Handle<WeakCell> code_cell = isolate->factory()->NewWeakCell(code);
  Handle<WeakCell> literals_cell = isolate->factory()->NewWeakCell(literals);
  Handle<WeakCell> shared_cell = isolate->factory()->NewWeakCell(shared);

  new_code_map->set(entry + kSharedOffset, *shared_cell);
  new_code_map->set(entry + kCachedCodeOffset, *code_cell);
  new_code_map->set(entry + kLiteralsOffset, *literals_cell);
  new_code_map->set(entry + kOsrAstIdOffset, Smi::FromInt(osr_ast_id.ToInt()));

#ifdef DEBUG
  for (int i = 0; i < new_code_map->length(); i += kEntryLength) {
    WeakCell* cell = WeakCell::cast(new_code_map->get(i + kSharedOffset));
    DCHECK(cell->cleared() || cell->value()->IsSharedFunctionInfo());
    cell = WeakCell::cast(new_code_map->get(i + kCachedCodeOffset));
    DCHECK(cell->cleared() ||
           (cell->value()->IsCode() &&
            Code::cast(cell->value())->kind() == Code::OPTIMIZED_FUNCTION));
    cell = WeakCell::cast(new_code_map->get(i + kLiteralsOffset));
    DCHECK(cell->cleared() || cell->value()->IsFixedArray());
    DCHECK(new_code_map->get(i + kOsrAstIdOffset)->IsSmi());
  }
#endif

  FixedArray* old_code_map = native_context->osr_code_table();
  if (old_code_map != *new_code_map) {
    native_context->set_osr_code_table(*new_code_map);
  }
}

void Context::EvictFromOptimizedCodeMap(Code* optimized_code,
                                        const char* reason) {
  DCHECK(IsNativeContext());
  DisallowHeapAllocation no_gc;
  if (OptimizedCodeMapIsCleared()) return;

  Heap* heap = GetHeap();
  FixedArray* code_map = osr_code_table();
  int dst = 0;
  int length = code_map->length();
  for (int src = 0; src < length; src += kEntryLength) {
    if (WeakCell::cast(code_map->get(src + kCachedCodeOffset))->value() ==
        optimized_code) {
      BailoutId osr(Smi::cast(code_map->get(src + kOsrAstIdOffset))->value());
      if (FLAG_trace_opt) {
        PrintF(
            "[evicting entry from native context optimizing code map (%s) for ",
            reason);
        ShortPrint();
        DCHECK(!osr.IsNone());
        PrintF(" (osr ast id %d)]\n", osr.ToInt());
      }
      // Evict the src entry by not copying it to the dst entry.
      continue;
    }
    // Keep the src entry by copying it to the dst entry.
    if (dst != src) {
      code_map->set(dst + kSharedOffset, code_map->get(src + kSharedOffset));
      code_map->set(dst + kCachedCodeOffset,
                    code_map->get(src + kCachedCodeOffset));
      code_map->set(dst + kLiteralsOffset,
                    code_map->get(src + kLiteralsOffset));
      code_map->set(dst + kOsrAstIdOffset,
                    code_map->get(src + kOsrAstIdOffset));
    }
    dst += kEntryLength;
  }
  if (dst != length) {
    // Always trim even when array is cleared because of heap verifier.
    heap->RightTrimFixedArray(code_map, length - dst);
    if (code_map->length() == 0) {
      ClearOptimizedCodeMap();
    }
  }
}

void Context::ClearOptimizedCodeMap() {
  DCHECK(IsNativeContext());
  FixedArray* empty_fixed_array = GetHeap()->empty_fixed_array();
  set_osr_code_table(empty_fixed_array);
}

void Context::AddOptimizedFunction(JSFunction* function) {
  DCHECK(IsNativeContext());
  Isolate* isolate = GetIsolate();
#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    Object* element = get(OPTIMIZED_FUNCTIONS_LIST);
    while (!element->IsUndefined(isolate)) {
      CHECK(element != function);
      element = JSFunction::cast(element)->next_function_link();
    }
  }

  // Check that the context belongs to the weak native contexts list.
  bool found = false;
  Object* context = isolate->heap()->native_contexts_list();
  while (!context->IsUndefined(isolate)) {
    if (context == this) {
      found = true;
      break;
    }
    context = Context::cast(context)->next_context_link();
  }
  CHECK(found);
#endif

  // If the function link field is already used then the function was
  // enqueued as a code flushing candidate and we remove it now.
  if (!function->next_function_link()->IsUndefined(isolate)) {
    CodeFlusher* flusher = GetHeap()->mark_compact_collector()->code_flusher();
    flusher->EvictCandidate(function);
  }

  DCHECK(function->next_function_link()->IsUndefined(isolate));

  function->set_next_function_link(get(OPTIMIZED_FUNCTIONS_LIST),
                                   UPDATE_WEAK_WRITE_BARRIER);
  set(OPTIMIZED_FUNCTIONS_LIST, function, UPDATE_WEAK_WRITE_BARRIER);
}


void Context::RemoveOptimizedFunction(JSFunction* function) {
  DCHECK(IsNativeContext());
  Object* element = get(OPTIMIZED_FUNCTIONS_LIST);
  JSFunction* prev = NULL;
  Isolate* isolate = function->GetIsolate();
  while (!element->IsUndefined(isolate)) {
    JSFunction* element_function = JSFunction::cast(element);
    DCHECK(element_function->next_function_link()->IsUndefined(isolate) ||
           element_function->next_function_link()->IsJSFunction());
    if (element_function == function) {
      if (prev == NULL) {
        set(OPTIMIZED_FUNCTIONS_LIST, element_function->next_function_link(),
            UPDATE_WEAK_WRITE_BARRIER);
      } else {
        prev->set_next_function_link(element_function->next_function_link(),
                                     UPDATE_WEAK_WRITE_BARRIER);
      }
      element_function->set_next_function_link(GetHeap()->undefined_value(),
                                               UPDATE_WEAK_WRITE_BARRIER);
      return;
    }
    prev = element_function;
    element = element_function->next_function_link();
  }
  UNREACHABLE();
}


void Context::SetOptimizedFunctionsListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(OPTIMIZED_FUNCTIONS_LIST, head, UPDATE_WEAK_WRITE_BARRIER);
}


Object* Context::OptimizedFunctionsListHead() {
  DCHECK(IsNativeContext());
  return get(OPTIMIZED_FUNCTIONS_LIST);
}


void Context::AddOptimizedCode(Code* code) {
  DCHECK(IsNativeContext());
  DCHECK(code->kind() == Code::OPTIMIZED_FUNCTION);
  DCHECK(code->next_code_link()->IsUndefined(GetIsolate()));
  code->set_next_code_link(get(OPTIMIZED_CODE_LIST));
  set(OPTIMIZED_CODE_LIST, code, UPDATE_WEAK_WRITE_BARRIER);
}


void Context::SetOptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(OPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER);
}


Object* Context::OptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(OPTIMIZED_CODE_LIST);
}


void Context::SetDeoptimizedCodeListHead(Object* head) {
  DCHECK(IsNativeContext());
  set(DEOPTIMIZED_CODE_LIST, head, UPDATE_WEAK_WRITE_BARRIER);
}


Object* Context::DeoptimizedCodeListHead() {
  DCHECK(IsNativeContext());
  return get(DEOPTIMIZED_CODE_LIST);
}


Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Isolate* isolate = GetIsolate();
  Handle<Object> result(error_message_for_code_gen_from_strings(), isolate);
  if (!result->IsUndefined(isolate)) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Code generation from strings disallowed for this context");
}


#define COMPARE_NAME(index, type, name) \
  if (string->IsOneByteEqualTo(STATIC_CHAR_VECTOR(#name))) return index;

int Context::ImportedFieldIndexForName(Handle<String> string) {
  NATIVE_CONTEXT_IMPORTED_FIELDS(COMPARE_NAME)
  return kNotFound;
}


int Context::IntrinsicIndexForName(Handle<String> string) {
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#define COMPARE_NAME(index, type, name) \
  if (strncmp(string, #name, length) == 0) return index;

int Context::IntrinsicIndexForName(const unsigned char* unsigned_string,
                                   int length) {
  const char* string = reinterpret_cast<const char*>(unsigned_string);
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#ifdef DEBUG

bool Context::IsBootstrappingOrNativeContext(Isolate* isolate, Object* object) {
  // During bootstrapping we allow all objects to pass as global
  // objects. This is necessary to fix circular dependencies.
  return isolate->heap()->gc_state() != Heap::NOT_IN_GC ||
         isolate->bootstrapper()->IsActive() || object->IsNativeContext();
}


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

#endif

void Context::ResetErrorsThrown() {
  DCHECK(IsNativeContext());
  set_errors_thrown(Smi::FromInt(0));
}

void Context::IncrementErrorsThrown() {
  DCHECK(IsNativeContext());

  int previous_value = errors_thrown()->value();
  set_errors_thrown(Smi::FromInt(previous_value + 1));
}


int Context::GetErrorsThrown() { return errors_thrown()->value(); }

}  // namespace internal
}  // namespace v8
