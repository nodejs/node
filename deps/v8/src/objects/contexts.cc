// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/contexts.h"

#include "src/ast/modules.h"
#include "src/debug/debug.h"
#include "src/execution/isolate-inl.h"
#include "src/init/bootstrapper.h"
#include "src/objects/module-inl.h"
#include "src/objects/string-set-inl.h"

namespace v8 {
namespace internal {

void ScriptContextTable::AddLocalNamesFromContext(
    Isolate* isolate, Handle<ScriptContextTable> script_context_table,
    Handle<Context> script_context, bool ignore_duplicates,
    int script_context_index) {
  ReadOnlyRoots roots(isolate);
  PtrComprCageBase cage_base(isolate);
  Handle<NameToIndexHashTable> names_table(
      script_context_table->names_to_context_index(cage_base), isolate);
  Handle<ScopeInfo> scope_info(script_context->scope_info(cage_base), isolate);
  int local_count = scope_info->ContextLocalCount();
  names_table = names_table->EnsureCapacity(isolate, names_table, local_count);
  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    Handle<Name> name(it->name(cage_base), isolate);
    if (ignore_duplicates) {
      int32_t hash = NameToIndexShape::Hash(roots, name);
      if (names_table->FindEntry(cage_base, roots, name, hash).is_found()) {
        continue;
      }
    }
    names_table = NameToIndexHashTable::Add(isolate, names_table, name,
                                            script_context_index);
  }
  script_context_table->set_names_to_context_index(*names_table);
}

Handle<ScriptContextTable> ScriptContextTable::Extend(
    Isolate* isolate, Handle<ScriptContextTable> table,
    Handle<Context> script_context, bool ignore_duplicates) {
  Handle<ScriptContextTable> result;
  int used = table->used(kAcquireLoad);
  int length = table->length();
  CHECK(used >= 0 && length > 0 && used < length);
  if (used + kFirstContextSlotIndex == length) {
    CHECK(length < Smi::kMaxValue / 2);
    Handle<FixedArray> copy =
        isolate->factory()->CopyFixedArrayAndGrow(table, length);
    copy->set_map(ReadOnlyRoots(isolate).script_context_table_map());
    result = Handle<ScriptContextTable>::cast(copy);
  } else {
    result = table;
  }
  DCHECK(script_context->IsScriptContext());
  ScriptContextTable::AddLocalNamesFromContext(isolate, result, script_context,
                                               ignore_duplicates, used);
  result->set(used + kFirstContextSlotIndex, *script_context, kReleaseStore);
  result->set_used(used + 1, kReleaseStore);
  return result;
}

void Context::Initialize(Isolate* isolate) {
  ScopeInfo scope_info = this->scope_info();
  int header = scope_info.ContextHeaderLength();
  for (int var = 0; var < scope_info.ContextLocalCount(); var++) {
    if (scope_info.ContextLocalInitFlag(var) == kNeedsInitialization) {
      set(header + var, ReadOnlyRoots(isolate).the_hole_value());
    }
  }
}

bool ScriptContextTable::Lookup(Handle<String> name,
                                VariableLookupResult* result) {
  DisallowGarbageCollection no_gc;
  int index = names_to_context_index().Lookup(name);
  if (index == -1) return false;
  DCHECK_LE(0, index);
  DCHECK_LT(index, used(kAcquireLoad));
  Context context = get_context(index);
  DCHECK(context.IsScriptContext());
  int slot_index = context.scope_info().ContextSlotIndex(name, result);
  if (slot_index >= 0) {
    result->context_index = index;
    result->slot_index = slot_index;
    return true;
  }
  return false;
}

bool Context::is_declaration_context() const {
  if (IsFunctionContext() || IsNativeContext() || IsScriptContext() ||
      IsModuleContext()) {
    return true;
  }
  if (IsEvalContext()) {
    return scope_info().language_mode() == LanguageMode::kStrict;
  }
  if (!IsBlockContext()) return false;
  return scope_info().is_declaration_scope();
}

Context Context::declaration_context() const {
  Context current = *this;
  while (!current.is_declaration_context()) {
    current = current.previous();
  }
  return current;
}

Context Context::closure_context() const {
  Context current = *this;
  while (!current.IsFunctionContext() && !current.IsScriptContext() &&
         !current.IsModuleContext() && !current.IsNativeContext() &&
         !current.IsEvalContext()) {
    current = current.previous();
  }
  return current;
}

JSObject Context::extension_object() const {
  DCHECK(IsNativeContext() || IsFunctionContext() || IsBlockContext() ||
         IsEvalContext() || IsCatchContext());
  HeapObject object = extension();
  if (object.IsUndefined()) return JSObject();
  DCHECK(object.IsJSContextExtensionObject() ||
         (IsNativeContext() && object.IsJSGlobalObject()));
  return JSObject::cast(object);
}

JSReceiver Context::extension_receiver() const {
  DCHECK(IsNativeContext() || IsWithContext() || IsEvalContext() ||
         IsFunctionContext() || IsBlockContext());
  return IsWithContext() ? JSReceiver::cast(extension()) : extension_object();
}

SourceTextModule Context::module() const {
  Context current = *this;
  while (!current.IsModuleContext()) {
    current = current.previous();
  }
  return SourceTextModule::cast(current.extension());
}

JSGlobalObject Context::global_object() const {
  return JSGlobalObject::cast(native_context().extension());
}

Context Context::script_context() const {
  Context current = *this;
  while (!current.IsScriptContext()) {
    current = current.previous();
  }
  return current;
}

JSGlobalProxy Context::global_proxy() const {
  return native_context().global_proxy_object();
}

/**
 * Lookups a property in an object environment, taking the unscopables into
 * account. This is used For HasBinding spec algorithms for ObjectEnvironment.
 */
static Maybe<bool> UnscopableLookup(LookupIterator* it, bool is_with_context) {
  Isolate* isolate = it->isolate();

  Maybe<bool> found = JSReceiver::HasProperty(it);
  if (!is_with_context || found.IsNothing() || !found.FromJust()) return found;

  Handle<Object> unscopables;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, unscopables,
      JSReceiver::GetProperty(isolate,
                              Handle<JSReceiver>::cast(it->GetReceiver()),
                              isolate->factory()->unscopables_symbol()),
      Nothing<bool>());
  if (!unscopables->IsJSReceiver()) return Just(true);
  Handle<Object> blocklist;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, blocklist,
      JSReceiver::GetProperty(isolate, Handle<JSReceiver>::cast(unscopables),
                              it->name()),
      Nothing<bool>());
  return Just(!blocklist->BooleanValue(isolate));
}

static PropertyAttributes GetAttributesForMode(VariableMode mode) {
  DCHECK(IsSerializableVariableMode(mode));
  return IsConstVariableMode(mode) ? READ_ONLY : NONE;
}

// static
Handle<Object> Context::Lookup(Handle<Context> context, Handle<String> name,
                               ContextLookupFlags flags, int* index,
                               PropertyAttributes* attributes,
                               InitializationFlag* init_flag,
                               VariableMode* variable_mode,
                               bool* is_sloppy_function_name) {
  Isolate* isolate = context->GetIsolate();

  bool follow_context_chain = (flags & FOLLOW_CONTEXT_CHAIN) != 0;
  *index = kNotFound;
  *attributes = ABSENT;
  *init_flag = kCreatedInitialized;
  *variable_mode = VariableMode::kVar;
  if (is_sloppy_function_name != nullptr) {
    *is_sloppy_function_name = false;
  }

  if (FLAG_trace_contexts) {
    PrintF("Context::Lookup(");
    name->ShortPrint();
    PrintF(")\n");
  }

  do {
    if (FLAG_trace_contexts) {
      PrintF(" - looking in context %p",
             reinterpret_cast<void*>(context->ptr()));
      if (context->IsScriptContext()) PrintF(" (script context)");
      if (context->IsNativeContext()) PrintF(" (native context)");
      PrintF("\n");
    }

    // 1. Check global objects, subjects of with, and extension objects.
    DCHECK_IMPLIES(context->IsEvalContext() && context->has_extension(),
                   context->extension().IsTheHole(isolate));
    if ((context->IsNativeContext() || context->IsWithContext() ||
         context->IsFunctionContext() || context->IsBlockContext()) &&
        context->has_extension() && !context->extension_receiver().is_null()) {
      Handle<JSReceiver> object(context->extension_receiver(), isolate);

      if (context->IsNativeContext()) {
        DisallowGarbageCollection no_gc;
        if (FLAG_trace_contexts) {
          PrintF(" - trying other script contexts\n");
        }
        // Try other script contexts.
        ScriptContextTable script_contexts =
            context->global_object().native_context().script_context_table();
        VariableLookupResult r;
        if (script_contexts.Lookup(name, &r)) {
          Context script_context = script_contexts.get_context(r.context_index);
          if (FLAG_trace_contexts) {
            PrintF("=> found property in script context %d: %p\n",
                   r.context_index,
                   reinterpret_cast<void*>(script_context.ptr()));
          }
          *index = r.slot_index;
          *variable_mode = r.mode;
          *init_flag = r.init_flag;
          *attributes = GetAttributesForMode(r.mode);
          return handle(script_context, isolate);
        }
      }

      // Context extension objects needs to behave as if they have no
      // prototype.  So even if we want to follow prototype chains, we need
      // to only do a local lookup for context extension objects.
      Maybe<PropertyAttributes> maybe = Nothing<PropertyAttributes>();
      if ((flags & FOLLOW_PROTOTYPE_CHAIN) == 0 ||
          object->IsJSContextExtensionObject()) {
        maybe = JSReceiver::GetOwnPropertyAttributes(object, name);
      } else {
        // A with context will never bind "this", but debug-eval may look into
        // a with context when resolving "this". Other synthetic variables such
        // as new.target may be resolved as VariableMode::kDynamicLocal due to
        // bug v8:5405 , skipping them here serves as a workaround until a more
        // thorough fix can be applied.
        // TODO(v8:5405): Replace this check with a DCHECK when resolution of
        // of synthetic variables does not go through this code path.
        if (ScopeInfo::VariableIsSynthetic(*name)) {
          DCHECK(context->IsWithContext());
          maybe = Just(ABSENT);
        } else {
          LookupIterator it(isolate, object, name, object);
          Maybe<bool> found = UnscopableLookup(&it, context->IsWithContext());
          if (found.IsNothing()) {
            maybe = Nothing<PropertyAttributes>();
          } else {
            // Luckily, consumers of |maybe| only care whether the property
            // was absent or not, so we can return a dummy |NONE| value
            // for its attributes when it was present.
            maybe = Just(found.FromJust() ? NONE : ABSENT);
          }
        }
      }

      if (maybe.IsNothing()) return Handle<Object>();
      DCHECK(!isolate->has_pending_exception());
      *attributes = maybe.FromJust();

      if (maybe.FromJust() != ABSENT) {
        if (FLAG_trace_contexts) {
          PrintF("=> found property in context object %p\n",
                 reinterpret_cast<void*>(object->ptr()));
        }
        return object;
      }
    }

    // 2. Check the context proper if it has slots.
    if (context->IsFunctionContext() || context->IsBlockContext() ||
        context->IsScriptContext() || context->IsEvalContext() ||
        context->IsModuleContext() || context->IsCatchContext()) {
      DisallowGarbageCollection no_gc;
      // Use serialized scope information of functions and blocks to search
      // for the context index.
      ScopeInfo scope_info = context->scope_info();
      VariableLookupResult lookup_result;
      int slot_index = scope_info.ContextSlotIndex(name, &lookup_result);
      DCHECK(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        // Re-direct lookup to the ScriptContextTable in case we find a hole in
        // a REPL script context. REPL scripts allow re-declaration of
        // script-level let bindings. The value itself is stored in the script
        // context of the first script that declared a variable, all other
        // script contexts will contain 'the hole' for that particular name.
        if (scope_info.IsReplModeScope() &&
            context->get(slot_index).IsTheHole(isolate)) {
          context = Handle<Context>(context->previous(), isolate);
          continue;
        }

        if (FLAG_trace_contexts) {
          PrintF("=> found local in context slot %d (mode = %hhu)\n",
                 slot_index, static_cast<uint8_t>(lookup_result.mode));
        }
        *index = slot_index;
        *variable_mode = lookup_result.mode;
        *init_flag = lookup_result.init_flag;
        *attributes = GetAttributesForMode(lookup_result.mode);
        return context;
      }

      // Check the slot corresponding to the intermediate context holding
      // only the function name variable. It's conceptually (and spec-wise)
      // in an outer scope of the function's declaration scope.
      if (follow_context_chain && context->IsFunctionContext()) {
        int function_index = scope_info.FunctionContextSlotIndex(*name);
        if (function_index >= 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found intermediate function in context slot %d\n",
                   function_index);
          }
          *index = function_index;
          *attributes = READ_ONLY;
          *init_flag = kCreatedInitialized;
          *variable_mode = VariableMode::kConst;
          if (is_sloppy_function_name != nullptr &&
              is_sloppy(scope_info.language_mode())) {
            *is_sloppy_function_name = true;
          }
          return context;
        }
      }

      // Lookup variable in module imports and exports.
      if (context->IsModuleContext()) {
        VariableMode mode;
        InitializationFlag flag;
        MaybeAssignedFlag maybe_assigned_flag;
        int cell_index =
            scope_info.ModuleIndex(*name, &mode, &flag, &maybe_assigned_flag);
        if (cell_index != 0) {
          if (FLAG_trace_contexts) {
            PrintF("=> found in module imports or exports\n");
          }
          *index = cell_index;
          *variable_mode = mode;
          *init_flag = flag;
          *attributes = SourceTextModuleDescriptor::GetCellIndexKind(
                            cell_index) == SourceTextModuleDescriptor::kExport
                            ? GetAttributesForMode(mode)
                            : READ_ONLY;
          return handle(context->module(), isolate);
        }
      }
    } else if (context->IsDebugEvaluateContext()) {
      // Check materialized locals.
      Object ext = context->get(EXTENSION_INDEX);
      if (ext.IsJSReceiver()) {
        Handle<JSReceiver> extension(JSReceiver::cast(ext), isolate);
        LookupIterator it(isolate, extension, name, extension);
        Maybe<bool> found = JSReceiver::HasProperty(&it);
        if (found.FromMaybe(false)) {
          *attributes = NONE;
          return extension;
        }
      }

      // Check blocklist. Names that are listed, cannot be resolved further.
      ScopeInfo scope_info = context->scope_info();
      if (scope_info.HasLocalsBlockList() &&
          scope_info.LocalsBlockList().Has(isolate, name)) {
        if (FLAG_trace_contexts) {
          PrintF(" - name is blocklisted. Aborting.\n");
        }
        break;
      }

      // Check the original context, but do not follow its context chain.
      Object obj = context->get(WRAPPED_CONTEXT_INDEX);
      if (obj.IsContext()) {
        Handle<Context> wrapped_context(Context::cast(obj), isolate);
        Handle<Object> result =
            Context::Lookup(wrapped_context, name, DONT_FOLLOW_CHAINS, index,
                            attributes, init_flag, variable_mode);
        if (!result.is_null()) return result;
      }
    }

    // 3. Prepare to continue with the previous (next outermost) context.
    if (context->IsNativeContext()) break;

    context = Handle<Context>(context->previous(), isolate);
  } while (follow_context_chain);

  if (FLAG_trace_contexts) {
    PrintF("=> no property/slot found\n");
  }
  return Handle<Object>::null();
}

void NativeContext::AddOptimizedCode(CodeT code) {
  DCHECK(CodeKindCanDeoptimize(code.kind()));
  DCHECK(code.next_code_link().IsUndefined());
  code.set_next_code_link(OptimizedCodeListHead());
  set(OPTIMIZED_CODE_LIST, code, UPDATE_WEAK_WRITE_BARRIER, kReleaseStore);
}

Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Isolate* isolate = GetIsolate();
  Handle<Object> result(error_message_for_code_gen_from_strings(), isolate);
  if (!result->IsUndefined(isolate)) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Code generation from strings disallowed for this context");
}

#define COMPARE_NAME(index, type, name) \
  if (string->IsOneByteEqualTo(base::StaticCharVector(#name))) return index;

int Context::IntrinsicIndexForName(Handle<String> string) {
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#define COMPARE_NAME(index, type, name)                                      \
  {                                                                          \
    const int name_length = static_cast<int>(arraysize(#name)) - 1;          \
    if ((length == name_length) && strncmp(string, #name, name_length) == 0) \
      return index;                                                          \
  }

int Context::IntrinsicIndexForName(const unsigned char* unsigned_string,
                                   int length) {
  const char* string = reinterpret_cast<const char*>(unsigned_string);
  NATIVE_CONTEXT_INTRINSIC_FUNCTIONS(COMPARE_NAME);
  return kNotFound;
}

#undef COMPARE_NAME

#ifdef VERIFY_HEAP
namespace {
// TODO(v8:12298): Fix js-context-specialization cctests to set up full
// native contexts instead of using dummy internalized strings as
// extensions.
bool IsContexExtensionTestObject(HeapObject extension) {
  return extension.IsInternalizedString() &&
         String::cast(extension).length() == 1;
}
}  // namespace

void Context::VerifyExtensionSlot(HeapObject extension) {
  CHECK(scope_info().HasContextExtensionSlot());
  // Early exit for potentially uninitialized contexfts.
  if (extension.IsUndefined()) return;
  if (extension.IsJSContextExtensionObject()) {
    CHECK((IsBlockContext() && scope_info().is_declaration_scope()) ||
          IsFunctionContext());
  } else if (IsModuleContext()) {
    CHECK(extension.IsSourceTextModule());
  } else if (IsDebugEvaluateContext() || IsWithContext()) {
    CHECK(extension.IsJSReceiver() ||
          (IsWithContext() && IsContexExtensionTestObject(extension)));
  } else if (IsNativeContext()) {
    CHECK(extension.IsJSGlobalObject() ||
          IsContexExtensionTestObject(extension));
  } else if (IsScriptContext()) {
    // Host-defined options can be stored on the context for classic scripts.
    CHECK(extension.IsFixedArray());
  }
}
#endif  // VERIFY_HEAP

void Context::set_extension(HeapObject object, WriteBarrierMode mode) {
  DCHECK(scope_info().HasContextExtensionSlot());
#ifdef VERIFY_HEAP
  VerifyExtensionSlot(object);
#endif
  set(EXTENSION_INDEX, object, mode);
}

#ifdef DEBUG

bool Context::IsBootstrappingOrValidParentContext(Object object,
                                                  Context child) {
  // During bootstrapping we allow all objects to pass as
  // contexts. This is necessary to fix circular dependencies.
  if (child.GetIsolate()->bootstrapper()->IsActive()) return true;
  if (!object.IsContext()) return false;
  Context context = Context::cast(object);
  return context.IsNativeContext() || context.IsScriptContext() ||
         context.IsModuleContext() || !child.IsModuleContext();
}

#endif

void NativeContext::ResetErrorsThrown() { set_errors_thrown(Smi::FromInt(0)); }

void NativeContext::IncrementErrorsThrown() {
  int previous_value = errors_thrown().value();
  set_errors_thrown(Smi::FromInt(previous_value + 1));
}

int NativeContext::GetErrorsThrown() { return errors_thrown().value(); }

STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 2);
STATIC_ASSERT(Context::MIN_CONTEXT_EXTENDED_SLOTS == 3);
STATIC_ASSERT(NativeContext::kScopeInfoOffset ==
              Context::OffsetOfElementAt(NativeContext::SCOPE_INFO_INDEX));
STATIC_ASSERT(NativeContext::kPreviousOffset ==
              Context::OffsetOfElementAt(NativeContext::PREVIOUS_INDEX));
STATIC_ASSERT(NativeContext::kExtensionOffset ==
              Context::OffsetOfElementAt(NativeContext::EXTENSION_INDEX));

STATIC_ASSERT(NativeContext::kStartOfStrongFieldsOffset ==
              Context::OffsetOfElementAt(-1));
STATIC_ASSERT(NativeContext::kStartOfWeakFieldsOffset ==
              Context::OffsetOfElementAt(NativeContext::FIRST_WEAK_SLOT));
STATIC_ASSERT(NativeContext::kMicrotaskQueueOffset ==
              Context::SizeFor(NativeContext::NATIVE_CONTEXT_SLOTS));
STATIC_ASSERT(NativeContext::kSize ==
              (Context::SizeFor(NativeContext::NATIVE_CONTEXT_SLOTS) +
               kSystemPointerSize));

void NativeContext::RunPromiseHook(PromiseHookType type,
                                   Handle<JSPromise> promise,
                                   Handle<Object> parent) {
  Isolate* isolate = promise->GetIsolate();
  DCHECK(isolate->HasContextPromiseHooks());
  int contextSlot;

  switch (type) {
    case PromiseHookType::kInit:
      contextSlot = PROMISE_HOOK_INIT_FUNCTION_INDEX;
      break;
    case PromiseHookType::kResolve:
      contextSlot = PROMISE_HOOK_RESOLVE_FUNCTION_INDEX;
      break;
    case PromiseHookType::kBefore:
      contextSlot = PROMISE_HOOK_BEFORE_FUNCTION_INDEX;
      break;
    case PromiseHookType::kAfter:
      contextSlot = PROMISE_HOOK_AFTER_FUNCTION_INDEX;
      break;
    default:
      UNREACHABLE();
  }

  Handle<Object> hook(isolate->native_context()->get(contextSlot), isolate);
  if (hook->IsUndefined()) return;

  int argc = type == PromiseHookType::kInit ? 2 : 1;
  Handle<Object> argv[2] = {
    Handle<Object>::cast(promise),
    parent
  };

  Handle<Object> receiver = isolate->global_proxy();

  StackLimitCheck check(isolate);
  bool failed = false;
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    failed = true;
  } else {
    failed = Execution::Call(isolate, hook, receiver, argc, argv).is_null();
  }
  if (failed) {
    DCHECK(isolate->has_pending_exception());
    Handle<Object> exception(isolate->pending_exception(), isolate);

    MessageLocation* no_location = nullptr;
    Handle<JSMessageObject> message =
        isolate->CreateMessageOrAbort(exception, no_location);
    MessageHandler::ReportMessage(isolate, no_location, message);

    isolate->clear_pending_exception();
  }
}

}  // namespace internal
}  // namespace v8
