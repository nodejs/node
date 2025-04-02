// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/contexts.h"

#include <bit>
#include <limits>
#include <optional>

#include "src/ast/modules.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/execution/isolate-inl.h"
#include "src/flags/flags.h"
#include "src/init/bootstrapper.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/dependent-code.h"
#include "src/objects/heap-number.h"
#include "src/objects/module-inl.h"
#include "src/objects/property-cell.h"
#include "src/objects/string-set-inl.h"
#include "src/utils/boxed-float.h"

namespace v8::internal {

// static
Handle<ScriptContextTable> ScriptContextTable::New(Isolate* isolate,
                                                   int capacity,
                                                   AllocationType allocation) {
  DCHECK_GE(capacity, 0);
  DCHECK_LE(capacity, kMaxCapacity);

  auto names = NameToIndexHashTable::New(isolate, 16);

  std::optional<DisallowGarbageCollection> no_gc;
  Handle<ScriptContextTable> result =
      Allocate(isolate, capacity, &no_gc, allocation);
  result->set_length(0, kReleaseStore);
  result->set_names_to_context_index(*names);
  ReadOnlyRoots roots{isolate};
  MemsetTagged(result->RawFieldOfFirstElement(), roots.undefined_value(),
               capacity);
  return result;
}

namespace {

// Adds local names from `script_context` to the hash table.
Handle<NameToIndexHashTable> AddLocalNamesFromContext(
    Isolate* isolate, Handle<NameToIndexHashTable> names_table,
    DirectHandle<Context> script_context, bool ignore_duplicates,
    int script_context_index) {
  ReadOnlyRoots roots(isolate);
  DirectHandle<ScopeInfo> scope_info(script_context->scope_info(), isolate);
  int local_count = scope_info->ContextLocalCount();
  names_table = names_table->EnsureCapacity(isolate, names_table, local_count);

  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    DirectHandle<Name> name(it->name(), isolate);
    if (ignore_duplicates) {
      int32_t hash = NameToIndexShape::Hash(roots, name);
      if (names_table->FindEntry(isolate, roots, name, hash).is_found()) {
        continue;
      }
    }
    names_table = NameToIndexHashTable::Add(isolate, names_table, name,
                                            script_context_index);
  }

  return names_table;
}

}  // namespace

Handle<ScriptContextTable> ScriptContextTable::Add(
    Isolate* isolate, Handle<ScriptContextTable> table,
    DirectHandle<Context> script_context, bool ignore_duplicates) {
  DCHECK(script_context->IsScriptContext());

  int old_length = table->length(kAcquireLoad);
  int new_length = old_length + 1;
  DCHECK_LE(0, old_length);

  Handle<ScriptContextTable> result = table;
  int old_capacity = table->capacity();
  DCHECK_LE(old_length, old_capacity);
  if (old_length == old_capacity) {
    int new_capacity = NewCapacityForIndex(old_length, old_capacity);
    auto new_table = New(isolate, new_capacity);
    new_table->set_length(old_length, kReleaseStore);
    new_table->set_names_to_context_index(table->names_to_context_index());
    CopyElements(isolate, *new_table, 0, *table, 0, old_length);
    result = new_table;
  }

  Handle<NameToIndexHashTable> names_table(result->names_to_context_index(),
                                           isolate);
  names_table = AddLocalNamesFromContext(isolate, names_table, script_context,
                                         ignore_duplicates, old_length);
  result->set_names_to_context_index(*names_table);

  result->set(old_length, *script_context, kReleaseStore);
  result->set_length(new_length, kReleaseStore);
  return result;
}

void Context::Initialize(Isolate* isolate) {
  Tagged<ScopeInfo> scope_info = this->scope_info();
  int header = scope_info->ContextHeaderLength();
  for (int var = 0; var < scope_info->ContextLocalCount(); var++) {
    if (scope_info->ContextLocalInitFlag(var) == kNeedsInitialization) {
      set(header + var, ReadOnlyRoots(isolate).the_hole_value());
    }
  }
}

bool ScriptContextTable::Lookup(DirectHandle<String> name,
                                VariableLookupResult* result) {
  DisallowGarbageCollection no_gc;
  int index = names_to_context_index()->Lookup(name);
  if (index == -1) return false;
  DCHECK_LE(0, index);
  DCHECK_LT(index, length(kAcquireLoad));
  Tagged<Context> context = get(index);
  DCHECK(context->IsScriptContext());
  int slot_index = context->scope_info()->ContextSlotIndex(name, result);
  if (slot_index < 0) return false;
  result->context_index = index;
  result->slot_index = slot_index;
  return true;
}

bool Context::is_declaration_context() const {
  if (IsFunctionContext() || IsNativeContext(*this) || IsScriptContext() ||
      IsModuleContext()) {
    return true;
  }
  if (IsEvalContext()) {
    return scope_info()->language_mode() == LanguageMode::kStrict;
  }
  if (!IsBlockContext()) return false;
  return scope_info()->is_declaration_scope();
}

Tagged<Context> Context::declaration_context() const {
  Tagged<Context> current = *this;
  while (!current->is_declaration_context()) {
    current = current->previous();
  }
  return current;
}

Tagged<Context> Context::closure_context() const {
  Tagged<Context> current = *this;
  while (!current->IsFunctionContext() && !current->IsScriptContext() &&
         !current->IsModuleContext() && !IsNativeContext(current) &&
         !current->IsEvalContext()) {
    current = current->previous();
  }
  return current;
}

Tagged<JSObject> Context::extension_object() const {
  DCHECK(IsNativeContext(*this) || IsFunctionContext() || IsBlockContext() ||
         IsEvalContext() || IsCatchContext());
  Tagged<HeapObject> object = extension();
  if (IsUndefined(object)) return JSObject();
  DCHECK(IsJSContextExtensionObject(object) ||
         (IsNativeContext(*this) && IsJSGlobalObject(object)));
  return Cast<JSObject>(object);
}

Tagged<JSReceiver> Context::extension_receiver() const {
  DCHECK(IsNativeContext(*this) || IsWithContext() || IsEvalContext() ||
         IsFunctionContext() || IsBlockContext());
  return IsWithContext() ? Cast<JSReceiver>(extension()) : extension_object();
}

Tagged<SourceTextModule> Context::module() const {
  Tagged<Context> current = *this;
  while (!current->IsModuleContext()) {
    current = current->previous();
  }
  return Cast<SourceTextModule>(current->extension());
}

Tagged<JSGlobalObject> Context::global_object() const {
  return Cast<JSGlobalObject>(native_context()->extension());
}

Tagged<Context> Context::script_context() const {
  Tagged<Context> current = *this;
  while (!current->IsScriptContext()) {
    current = current->previous();
  }
  return current;
}

Tagged<JSGlobalProxy> Context::global_proxy() const {
  return native_context()->global_proxy_object();
}

/**
 * Lookups a property in an object environment, taking the unscopables into
 * account. This is used For HasBinding spec algorithms for ObjectEnvironment.
 */
static Maybe<bool> UnscopableLookup(LookupIterator* it, bool is_with_context) {
  Isolate* isolate = it->isolate();

  Maybe<bool> found = JSReceiver::HasProperty(it);
  if (!is_with_context || found.IsNothing() || !found.FromJust()) return found;

  DirectHandle<Object> unscopables;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, unscopables,
      JSReceiver::GetProperty(isolate, Cast<JSReceiver>(it->GetReceiver()),
                              isolate->factory()->unscopables_symbol()),
      Nothing<bool>());
  if (!IsJSReceiver(*unscopables)) return Just(true);
  DirectHandle<Object> blocklist;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, blocklist,
      JSReceiver::GetProperty(isolate, Cast<JSReceiver>(unscopables),
                              it->name()),
      Nothing<bool>());
  return Just(!Object::BooleanValue(*blocklist, isolate));
}

static PropertyAttributes GetAttributesForMode(VariableMode mode) {
  DCHECK(IsSerializableVariableMode(mode));
  return IsImmutableLexicalOrPrivateVariableMode(mode) ? READ_ONLY : NONE;
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
  bool has_seen_debug_evaluate_context = false;
  *index = kNotFound;
  *attributes = ABSENT;
  *init_flag = kCreatedInitialized;
  *variable_mode = VariableMode::kVar;
  if (is_sloppy_function_name != nullptr) {
    *is_sloppy_function_name = false;
  }

  if (v8_flags.trace_contexts) {
    PrintF("Context::Lookup(");
    ShortPrint(*name);
    PrintF(")\n");
  }

  do {
    if (v8_flags.trace_contexts) {
      PrintF(" - looking in context %p",
             reinterpret_cast<void*>(context->ptr()));
      if (context->IsScriptContext()) PrintF(" (script context)");
      if (IsNativeContext(*context)) PrintF(" (native context)");
      if (context->IsDebugEvaluateContext()) PrintF(" (debug context)");
      PrintF("\n");
    }

    // 1. Check global objects, subjects of with, and extension objects.
    DCHECK_IMPLIES(context->IsEvalContext() && context->has_extension(),
                   IsTheHole(context->extension(), isolate));
    if ((IsNativeContext(*context) || context->IsWithContext() ||
         context->IsFunctionContext() || context->IsBlockContext()) &&
        context->has_extension() && !context->extension_receiver().is_null()) {
      Handle<JSReceiver> object(context->extension_receiver(), isolate);

      if (IsNativeContext(*context)) {
        DisallowGarbageCollection no_gc;
        if (v8_flags.trace_contexts) {
          PrintF(" - trying other script contexts\n");
        }
        // Try other script contexts.
        Tagged<ScriptContextTable> script_contexts =
            context->native_context()->script_context_table();
        VariableLookupResult r;
        if (script_contexts->Lookup(name, &r)) {
          Tagged<Context> script_context =
              script_contexts->get(r.context_index);
          if (v8_flags.trace_contexts) {
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
          IsJSContextExtensionObject(*object)) {
        maybe = JSReceiver::GetOwnPropertyAttributes(isolate, object, name);
      } else {
        // A with context will never bind "this", but debug-eval may look into
        // a with context when resolving "this". Other synthetic variables such
        // as new.target may be resolved as VariableMode::kDynamicLocal due to
        // bug v8:5405 , skipping them here serves as a workaround until a more
        // thorough fix can be applied.
        // TODO(v8:5405): Replace this check with a DCHECK when resolution of
        // of synthetic variables does not go through this code path.
        if (ScopeInfo::VariableIsSynthetic(*name)) {
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
      DCHECK(!isolate->has_exception());
      *attributes = maybe.FromJust();

      if (maybe.FromJust() != ABSENT) {
        if (v8_flags.trace_contexts) {
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
      Tagged<ScopeInfo> scope_info = context->scope_info();
      VariableLookupResult lookup_result;
      int slot_index = scope_info->ContextSlotIndex(name, &lookup_result);
      DCHECK(slot_index < 0 || slot_index >= MIN_CONTEXT_SLOTS);
      if (slot_index >= 0) {
        // Re-direct lookup to the ScriptContextTable in case we find a hole in
        // a REPL script context. REPL scripts allow re-declaration of
        // script-level let bindings. The value itself is stored in the script
        // context of the first script that declared a variable, all other
        // script contexts will contain 'the hole' for that particular name.
        if (scope_info->IsReplModeScope() &&
            IsTheHole(context->get(slot_index), isolate)) {
          context = Handle<Context>(context->previous(), isolate);
          continue;
        }

        if (v8_flags.trace_contexts) {
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
        int function_index = scope_info->FunctionContextSlotIndex(*name);
        if (function_index >= 0) {
          if (v8_flags.trace_contexts) {
            PrintF("=> found intermediate function in context slot %d\n",
                   function_index);
          }
          *index = function_index;
          *attributes = READ_ONLY;
          *init_flag = kCreatedInitialized;
          *variable_mode = VariableMode::kConst;
          if (is_sloppy_function_name != nullptr &&
              is_sloppy(scope_info->language_mode())) {
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
            scope_info->ModuleIndex(*name, &mode, &flag, &maybe_assigned_flag);
        if (cell_index != 0) {
          if (v8_flags.trace_contexts) {
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
      has_seen_debug_evaluate_context = true;

      // Check materialized locals.
      Tagged<Object> ext = context->get(EXTENSION_INDEX);
      if (IsJSReceiver(ext)) {
        Handle<JSReceiver> extension(Cast<JSReceiver>(ext), isolate);
        LookupIterator it(isolate, extension, name, extension);
        Maybe<bool> found = JSReceiver::HasProperty(&it);
        if (found.FromMaybe(false)) {
          *attributes = NONE;
          return extension;
        }
      }

      // Check the original context, but do not follow its context chain.
      Tagged<Object> obj = context->get(WRAPPED_CONTEXT_INDEX);
      if (IsContext(obj)) {
        Handle<Context> wrapped_context(Cast<Context>(obj), isolate);
        Handle<Object> result =
            Context::Lookup(wrapped_context, name, DONT_FOLLOW_CHAINS, index,
                            attributes, init_flag, variable_mode);
        if (!result.is_null()) return result;
      }
    }

    // 3. Prepare to continue with the previous (next outermost) context.
    if (IsNativeContext(*context)) break;

    // In case we saw any DebugEvaluateContext, we'll need to check the block
    // list before we can advance to properly "shadow" stack-allocated
    // variables.
    // Note that this implicitly skips the block list check for the
    // "wrapped" context lookup for DebugEvaluateContexts. In that case
    // `has_seen_debug_evaluate_context` will always be false.
    if (has_seen_debug_evaluate_context &&
        IsEphemeronHashTable(isolate->heap()->locals_block_list_cache())) {
      DirectHandle<ScopeInfo> scope_info =
          direct_handle(context->scope_info(), isolate);
      Tagged<Object> maybe_outer_block_list =
          isolate->LocalsBlockListCacheGet(scope_info);
      if (IsStringSet(maybe_outer_block_list) &&
          Cast<StringSet>(maybe_outer_block_list)->Has(isolate, name)) {
        if (v8_flags.trace_contexts) {
          PrintF(" - name is blocklisted. Aborting.\n");
        }
        break;
      }
    }

    context = Handle<Context>(context->previous(), isolate);
  } while (follow_context_chain);

  if (v8_flags.trace_contexts) {
    PrintF("=> no property/slot found\n");
  }
  return Handle<Object>::null();
}

Tagged<ContextSidePropertyCell> Context::GetOrCreateContextSidePropertyCell(
    DirectHandle<Context> script_context, size_t index,
    ContextSidePropertyCell::Property property, Isolate* isolate) {
  DCHECK(v8_flags.script_context_mutable_heap_number ||
         v8_flags.const_tracking_let);
  DCHECK(script_context->IsScriptContext());
  DCHECK_NE(property, ContextSidePropertyCell::kOther);
  int side_data_index =
      static_cast<int>(index - Context::MIN_CONTEXT_EXTENDED_SLOTS);
  DirectHandle<FixedArray> side_data(
      Cast<FixedArray>(script_context->get(CONTEXT_SIDE_TABLE_PROPERTY_INDEX)),
      isolate);
  Tagged<Object> object = side_data->get(side_data_index);
  if (!IsContextSidePropertyCell(object)) {
    // If these CHECKs fail, there's a code path which initializes or assigns a
    // top-level `let` variable but doesn't update the side data.
    object = *isolate->factory()->NewContextSidePropertyCell(property);
    side_data->set(side_data_index, object);
  }
  return Cast<ContextSidePropertyCell>(object);
}

std::optional<ContextSidePropertyCell::Property>
Context::GetScriptContextSideProperty(size_t index) const {
  DCHECK(v8_flags.script_context_mutable_heap_number ||
         v8_flags.const_tracking_let);
  DCHECK(IsScriptContext());
  int side_data_index =
      static_cast<int>(index - Context::MIN_CONTEXT_EXTENDED_SLOTS);
  Tagged<FixedArray> side_data =
      Cast<FixedArray>(get(CONTEXT_SIDE_TABLE_PROPERTY_INDEX));
  Tagged<Object> object = side_data->get(side_data_index);
  if (IsUndefined(object)) return {};
  if (IsContextSidePropertyCell(object)) {
    return Cast<ContextSidePropertyCell>(object)->context_side_property();
  }
  CHECK(IsSmi(object));
  return ContextSidePropertyCell::FromSmi(object.ToSmi());
}

namespace {
std::optional<int32_t> DoubleFitsInInt32(double value) {
  constexpr double int32_min = std::numeric_limits<int32_t>::min();
  constexpr double int32_max = std::numeric_limits<int32_t>::max();
  // Check -0.0 first.
  if (value == 0.0 && std::signbit(value)) return {};
  double trunc_value = std::trunc(value);
  if (int32_min <= value && value <= int32_max && value == trunc_value) {
    return static_cast<int32_t>(trunc_value);
  }
  return {};
}

DirectHandle<Object> TryLoadMutableHeapNumber(
    DirectHandle<Context> script_context, int index, DirectHandle<Object> value,
    Isolate* isolate) {
  DCHECK(v8_flags.script_context_mutable_heap_number);
  DCHECK(script_context->IsScriptContext());
  if (!IsHeapNumber(*value)) return value;
  const int side_data_index = index - Context::MIN_CONTEXT_EXTENDED_SLOTS;
  Tagged<FixedArray> side_data_table = Cast<FixedArray>(
      script_context->get(Context::CONTEXT_SIDE_TABLE_PROPERTY_INDEX));
  if (side_data_table == ReadOnlyRoots(isolate).empty_fixed_array()) {
    // No side data (maybe the context was created while the side data
    // collection was disabled).
    return value;
  }

  Tagged<Object> data = side_data_table->get(side_data_index);
  ContextSidePropertyCell::Property property;
  if (IsSmi(data)) {
    property =
        static_cast<ContextSidePropertyCell::Property>(data.ToSmi().value());
  } else {
    CHECK(Is<ContextSidePropertyCell>(data));
    property = Cast<ContextSidePropertyCell>(data)->context_side_property();
  }
  if (property == ContextSidePropertyCell::kMutableInt32) {
    int32_t int32_value =
        static_cast<int32_t>(Cast<HeapNumber>(*value)->value_as_bits());
    return isolate->factory()->NewHeapNumber(static_cast<double>(int32_value));
  } else if (property == ContextSidePropertyCell::kMutableHeapNumber) {
    return isolate->factory()->NewHeapNumber(Cast<HeapNumber>(*value)->value());
  } else {
    return value;
  }
}
}  // namespace

DirectHandle<Object> Context::LoadScriptContextElement(
    DirectHandle<Context> script_context, int index, DirectHandle<Object> value,
    Isolate* isolate) {
  DCHECK(v8_flags.script_context_mutable_heap_number);
  DCHECK(script_context->IsScriptContext());
  return TryLoadMutableHeapNumber(script_context, index, value, isolate);
}

void Context::StoreScriptContextAndUpdateSlotProperty(
    DirectHandle<Context> script_context, int index,
    DirectHandle<Object> new_value, Isolate* isolate) {
  DCHECK(v8_flags.script_context_mutable_heap_number ||
         v8_flags.const_tracking_let);
  DCHECK(script_context->IsScriptContext());

  const int side_data_index = index - Context::MIN_CONTEXT_EXTENDED_SLOTS;
  DirectHandle<FixedArray> side_data(
      Cast<FixedArray>(
          script_context->get(Context::CONTEXT_SIDE_TABLE_PROPERTY_INDEX)),
      isolate);
  if (*side_data == ReadOnlyRoots(isolate).empty_fixed_array()) {
    // No side data (maybe the context was created while the side data
    // collection was disabled).
    script_context->set(index, *new_value);
    return;
  }

  DirectHandle<Object> old_value(script_context->get(index), isolate);
  if (IsTheHole(*old_value)) {
    // Setting the initial value. Here we cannot assert the corresponding side
    // data is `undefined` - that won't hold w/ variable redefinitions in REPL.
    script_context->set(index, *new_value);
    side_data->set(side_data_index, ContextSidePropertyCell::Const());
    return;
  }

  // If we are assigning the same value, the property won't change.
  if (*old_value == *new_value) {
    return;
  }
  // If both values are HeapNumbers with the same double value, the property
  // won't change either.
  if (Is<HeapNumber>(*old_value) && Is<HeapNumber>(*new_value)) {
    double old_number = Cast<HeapNumber>(*old_value)->value();
    double new_number = Cast<HeapNumber>(*new_value)->value();
    if (old_number == new_number && old_number != 0) {
      return;
    }
  }

  // From now on, we know the value is no longer a constant.

  Tagged<Object> data = side_data->get(side_data_index);
  std::optional<Tagged<ContextSidePropertyCell>> maybe_cell;
  ContextSidePropertyCell::Property property;

  if (IsSmi(data)) {
    property = ContextSidePropertyCell::FromSmi(data.ToSmi());
  } else {
    CHECK(Is<ContextSidePropertyCell>(data));
    maybe_cell = Cast<ContextSidePropertyCell>(data);
    property = maybe_cell.value()->context_side_property();
  }

  switch (property) {
    case ContextSidePropertyCell::kConst:
      if (maybe_cell) {
        DependentCode::DeoptimizeDependencyGroups(
            isolate, maybe_cell.value(),
            DependentCode::kScriptContextSlotPropertyChangedGroup);
      }
      if (v8_flags.script_context_mutable_heap_number) {
        // It can transition to Smi, MutableInt32, MutableHeapNumber or Other.
        if (IsHeapNumber(*new_value)) {
          double double_value = Cast<HeapNumber>(*new_value)->value();
          auto maybe_int32_value = DoubleFitsInInt32(double_value);
          if (v8_flags.script_context_mutable_heap_int32 && maybe_int32_value) {
            auto new_number =
                isolate->factory()->NewHeapInt32(*maybe_int32_value);
            script_context->set(index, *new_number);
            side_data->set(side_data_index,
                           ContextSidePropertyCell::MutableInt32());
          } else {
            auto new_number = isolate->factory()->NewHeapNumber(double_value);
            script_context->set(index, *new_number);
            side_data->set(side_data_index,
                           ContextSidePropertyCell::MutableHeapNumber());
          }
        } else {
          script_context->set(index, *new_value);
          side_data->set(side_data_index,
                         IsSmi(*new_value)
                             ? ContextSidePropertyCell::SmiMarker()
                             : ContextSidePropertyCell::Other());
        }
      } else {
        // MutableHeapNumber is not supported, just transition the property to
        // kOther.
        script_context->set(index, *new_value);
        side_data->set(side_data_index, ContextSidePropertyCell::Other());
      }

      break;
    case ContextSidePropertyCell::kSmi:
      if (IsSmi(*new_value)) {
        script_context->set(index, *new_value);
      } else {
        if (maybe_cell) {
          DependentCode::DeoptimizeDependencyGroups(
              isolate, maybe_cell.value(),
              DependentCode::kScriptContextSlotPropertyChangedGroup);
        }
        // It can transition to MutableInt32, MutableHeapNumber or Other.
        if (IsHeapNumber(*new_value)) {
          double double_value = Cast<HeapNumber>(*new_value)->value();
          auto maybe_int32_value = DoubleFitsInInt32(double_value);
          if (v8_flags.script_context_mutable_heap_int32 && maybe_int32_value) {
            auto new_number =
                isolate->factory()->NewHeapInt32(*maybe_int32_value);
            script_context->set(index, *new_number);
            side_data->set(side_data_index,
                           ContextSidePropertyCell::MutableInt32());
          } else {
            auto new_number = isolate->factory()->NewHeapNumber(double_value);
            script_context->set(index, *new_number);
            side_data->set(side_data_index,
                           ContextSidePropertyCell::MutableHeapNumber());
          }
        } else {
          script_context->set(index, *new_value);
          side_data->set(side_data_index, ContextSidePropertyCell::Other());
        }
      }
      break;
    case ContextSidePropertyCell::kMutableInt32: {
      CHECK(IsHeapNumber(*old_value));
      DirectHandle<HeapNumber> old_number = Cast<HeapNumber>(old_value);
      if (IsSmi(*new_value)) {
        old_number->set_value_as_bits(
            (static_cast<uint64_t>(kHoleNanUpper32) << 32) |
            Cast<Smi>(*new_value).value());
      } else if (IsHeapNumber(*new_value)) {
        double double_value = Cast<HeapNumber>(*new_value)->value();
        auto maybe_int32_value = DoubleFitsInInt32(double_value);
        if (v8_flags.script_context_mutable_heap_int32 && maybe_int32_value) {
          old_number->set_value_as_bits(
              (static_cast<uint64_t>(kHoleNanUpper32) << 32) |
              *maybe_int32_value);
        } else {
          if (maybe_cell) {
            DependentCode::DeoptimizeDependencyGroups(
                isolate, maybe_cell.value(),
                DependentCode::kScriptContextSlotPropertyChangedGroup);
          }
          old_number->set_value(double_value);
          side_data->set(side_data_index,
                         ContextSidePropertyCell::MutableHeapNumber());
        }
      } else {
        if (maybe_cell) {
          DependentCode::DeoptimizeDependencyGroups(
              isolate, maybe_cell.value(),
              DependentCode::kScriptContextSlotPropertyChangedGroup);
        }
        // It can only transition to Other.
        script_context->set(index, *new_value);
        side_data->set(side_data_index, ContextSidePropertyCell::Other());
      }
      break;
    }
    case ContextSidePropertyCell::kMutableHeapNumber:
      CHECK(IsHeapNumber(*old_value));
      if (IsSmi(*new_value)) {
        Cast<HeapNumber>(old_value)->set_value(
            static_cast<double>(Cast<Smi>(*new_value).value()));
      } else if (IsHeapNumber(*new_value)) {
        Cast<HeapNumber>(old_value)->set_value(
            Cast<HeapNumber>(*new_value)->value());
      } else {
        if (maybe_cell) {
          DependentCode::DeoptimizeDependencyGroups(
              isolate, maybe_cell.value(),
              DependentCode::kScriptContextSlotPropertyChangedGroup);
        }
        // It can only transition to Other.
        script_context->set(index, *new_value);
        side_data->set(side_data_index, ContextSidePropertyCell::Other());
      }
      break;
    case ContextSidePropertyCell::kOther:
      // We should not have a code depending on Other.
      DCHECK(!maybe_cell.has_value());
      // No need to update side data, this is a sink state...
      script_context->set(index, *new_value);
      break;
    default:
      UNREACHABLE();
  }
}

bool NativeContext::HasTemplateLiteralObject(Tagged<JSArray> array) {
  return array->map() == js_array_template_literal_object_map();
}

Handle<Object> Context::ErrorMessageForCodeGenerationFromStrings() {
  Isolate* isolate = GetIsolate();
  Handle<Object> result(error_message_for_code_gen_from_strings(), isolate);
  if (!IsUndefined(*result, isolate)) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Code generation from strings disallowed for this context");
}

DirectHandle<Object> Context::ErrorMessageForWasmCodeGeneration() {
  Isolate* isolate = GetIsolate();
  DirectHandle<Object> result(error_message_for_wasm_code_gen(), isolate);
  if (!IsUndefined(*result, isolate)) return result;
  return isolate->factory()->NewStringFromStaticChars(
      "Wasm code generation disallowed by embedder");
}

#ifdef VERIFY_HEAP
namespace {
// TODO(v8:12298): Fix js-context-specialization cctests to set up full
// native contexts instead of using dummy internalized strings as
// extensions.
bool IsContexExtensionTestObject(Tagged<HeapObject> extension) {
  return IsInternalizedString(extension) &&
         Cast<String>(extension)->length() == 1;
}
}  // namespace

void Context::VerifyExtensionSlot(Tagged<HeapObject> extension) {
  CHECK(scope_info()->HasContextExtensionSlot());
  // Early exit for potentially uninitialized contexfts.
  if (IsUndefined(extension)) return;
  if (IsJSContextExtensionObject(extension)) {
    CHECK((IsBlockContext() && scope_info()->is_declaration_scope()) ||
          IsFunctionContext());
  } else if (IsModuleContext()) {
    CHECK(IsSourceTextModule(extension));
  } else if (IsDebugEvaluateContext() || IsWithContext()) {
    CHECK(IsJSReceiver(extension) ||
          (IsWithContext() && IsContexExtensionTestObject(extension)));
  } else if (IsNativeContext(*this)) {
    CHECK(IsJSGlobalObject(extension) ||
          IsContexExtensionTestObject(extension));
  } else if (IsScriptContext()) {
    // Host-defined options can be stored on the context for classic scripts.
    CHECK(IsFixedArray(extension));
  }
}
#endif  // VERIFY_HEAP

void Context::set_extension(Tagged<HeapObject> object, WriteBarrierMode mode) {
  DCHECK(scope_info()->HasContextExtensionSlot());
#ifdef VERIFY_HEAP
  if (v8_flags.verify_heap) VerifyExtensionSlot(object);
#endif
  set(EXTENSION_INDEX, object, mode);
}

#ifdef DEBUG

bool Context::IsBootstrappingOrValidParentContext(Tagged<Object> object,
                                                  Tagged<Context> child) {
  // During bootstrapping we allow all objects to pass as
  // contexts. This is necessary to fix circular dependencies.
  if (child->GetIsolate()->bootstrapper()->IsActive()) return true;
  if (!IsContext(object)) return false;
  Tagged<Context> context = Cast<Context>(object);
  return IsNativeContext(context) || context->IsScriptContext() ||
         context->IsModuleContext() || !child->IsModuleContext();
}

#endif

void NativeContext::ResetErrorsThrown() { set_errors_thrown(Smi::FromInt(0)); }

void NativeContext::IncrementErrorsThrown() {
  int previous_value = errors_thrown().value();
  set_errors_thrown(Smi::FromInt(previous_value + 1));
}

int NativeContext::GetErrorsThrown() { return errors_thrown().value(); }

static_assert(Context::MIN_CONTEXT_SLOTS == 2);
static_assert(Context::MIN_CONTEXT_EXTENDED_SLOTS == 3);
static_assert(NativeContext::kScopeInfoOffset ==
              Context::OffsetOfElementAt(NativeContext::SCOPE_INFO_INDEX));
static_assert(NativeContext::kPreviousOffset ==
              Context::OffsetOfElementAt(NativeContext::PREVIOUS_INDEX));
static_assert(NativeContext::kExtensionOffset ==
              Context::OffsetOfElementAt(NativeContext::EXTENSION_INDEX));

static_assert(NativeContext::kStartOfStrongFieldsOffset ==
              Context::OffsetOfElementAt(-1));
static_assert(NativeContext::kStartOfWeakFieldsOffset ==
              Context::OffsetOfElementAt(NativeContext::FIRST_WEAK_SLOT));
static_assert(NativeContext::kMicrotaskQueueOffset ==
              Context::SizeFor(NativeContext::NATIVE_CONTEXT_SLOTS));
static_assert(NativeContext::kSize ==
              (Context::SizeFor(NativeContext::NATIVE_CONTEXT_SLOTS) +
               kSystemPointerSize));

#ifdef V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS
void NativeContext::RunPromiseHook(PromiseHookType type,
                                   DirectHandle<JSPromise> promise,
                                   DirectHandle<Object> parent) {
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

  DirectHandle<Object> hook(isolate->native_context()->get(contextSlot),
                            isolate);
  if (IsUndefined(*hook)) return;

  size_t argc = type == PromiseHookType::kInit ? 2 : 1;
  DirectHandle<Object> argv[2] = {Cast<Object>(promise), parent};

  DirectHandle<Object> receiver = isolate->global_proxy();

  StackLimitCheck check(isolate);
  bool failed = false;
  if (check.HasOverflowed()) {
    isolate->StackOverflow();
    failed = true;
  } else {
    failed = Execution::Call(isolate, hook, receiver, {argv, argc}).is_null();
  }
  if (failed) {
    DCHECK(isolate->has_exception());
    DirectHandle<Object> exception(isolate->exception(), isolate);

    MessageLocation* no_location = nullptr;
    DirectHandle<JSMessageObject> message =
        isolate->CreateMessageOrAbort(exception, no_location);
    MessageHandler::ReportMessage(isolate, no_location, message);

    isolate->clear_exception();
  }
}
#endif  // V8_ENABLE_JAVASCRIPT_PROMISE_HOOKS

}  // namespace v8::internal
