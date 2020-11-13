// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-function.h"

#include "src/codegen/compiler.h"
#include "src/diagnostics/code-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic.h"
#include "src/init/bootstrapper.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/strings/string-builder-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CodeKinds JSFunction::GetAttachedCodeKinds() const {
  CodeKinds result;

  // Note: There's a special case when bytecode has been aged away. After
  // flushing the bytecode, the JSFunction will still have the interpreter
  // entry trampoline attached, but the bytecode is no longer available.
  if (code().is_interpreter_trampoline_builtin()) {
    result |= CodeKindFlag::INTERPRETED_FUNCTION;
  }

  const CodeKind kind = code().kind();
  if (!CodeKindIsOptimizedJSFunction(kind) ||
      code().marked_for_deoptimization()) {
    DCHECK_EQ((result & ~kJSFunctionCodeKindsMask), 0);
    return result;
  }

  DCHECK(CodeKindIsOptimizedJSFunction(kind));
  result |= CodeKindToCodeKindFlag(kind);

  DCHECK_EQ((result & ~kJSFunctionCodeKindsMask), 0);
  return result;
}

CodeKinds JSFunction::GetAvailableCodeKinds() const {
  CodeKinds result = GetAttachedCodeKinds();

  if ((result & CodeKindFlag::INTERPRETED_FUNCTION) == 0) {
    // The SharedFunctionInfo could have attached bytecode.
    if (shared().HasBytecodeArray()) {
      result |= CodeKindFlag::INTERPRETED_FUNCTION;
    }
  }

  if ((result & kOptimizedJSFunctionCodeKindsMask) == 0) {
    // Check the optimized code cache.
    if (has_feedback_vector() && feedback_vector().has_optimized_code() &&
        !feedback_vector().optimized_code().marked_for_deoptimization()) {
      Code code = feedback_vector().optimized_code();
      DCHECK(CodeKindIsOptimizedJSFunction(code.kind()));
      result |= CodeKindToCodeKindFlag(code.kind());
    }
  }

  DCHECK_EQ((result & ~kJSFunctionCodeKindsMask), 0);
  return result;
}

bool JSFunction::HasAttachedOptimizedCode() const {
  CodeKinds result = GetAttachedCodeKinds();
  return (result & kOptimizedJSFunctionCodeKindsMask) != 0;
}

bool JSFunction::HasAvailableOptimizedCode() const {
  CodeKinds result = GetAvailableCodeKinds();
  return (result & kOptimizedJSFunctionCodeKindsMask) != 0;
}

bool JSFunction::HasAvailableCodeKind(CodeKind kind) const {
  CodeKinds result = GetAvailableCodeKinds();
  return (result & CodeKindToCodeKindFlag(kind)) != 0;
}

namespace {

// Returns false if no highest tier exists (i.e. the function is not compiled),
// otherwise returns true and sets highest_tier.
bool HighestTierOf(CodeKinds kinds, CodeKind* highest_tier) {
  DCHECK_EQ((kinds & ~kJSFunctionCodeKindsMask), 0);
  if ((kinds & CodeKindFlag::OPTIMIZED_FUNCTION) != 0) {
    *highest_tier = CodeKind::OPTIMIZED_FUNCTION;
    return true;
  } else if ((kinds & CodeKindFlag::NATIVE_CONTEXT_INDEPENDENT) != 0) {
    *highest_tier = CodeKind::NATIVE_CONTEXT_INDEPENDENT;
    return true;
  } else if ((kinds & CodeKindFlag::INTERPRETED_FUNCTION) != 0) {
    *highest_tier = CodeKind::INTERPRETED_FUNCTION;
    return true;
  }
  DCHECK_EQ(kinds, 0);
  return false;
}

}  // namespace

bool JSFunction::ActiveTierIsIgnition() const {
  CodeKind highest_tier;
  if (!HighestTierOf(GetAvailableCodeKinds(), &highest_tier)) return false;
  bool result = (highest_tier == CodeKind::INTERPRETED_FUNCTION);
  DCHECK_IMPLIES(result,
                 code().is_interpreter_trampoline_builtin() ||
                     (CodeKindIsOptimizedJSFunction(code().kind()) &&
                      code().marked_for_deoptimization()) ||
                     (code().builtin_index() == Builtins::kCompileLazy &&
                      shared().IsInterpreted()));
  return result;
}

bool JSFunction::ActiveTierIsTurbofan() const {
  CodeKind highest_tier;
  if (!HighestTierOf(GetAvailableCodeKinds(), &highest_tier)) return false;
  return highest_tier == CodeKind::OPTIMIZED_FUNCTION;
}

bool JSFunction::ActiveTierIsNCI() const {
  CodeKind highest_tier;
  if (!HighestTierOf(GetAvailableCodeKinds(), &highest_tier)) return false;
  return highest_tier == CodeKind::NATIVE_CONTEXT_INDEPENDENT;
}

CodeKind JSFunction::NextTier() const {
  return (FLAG_turbo_nci_as_midtier && ActiveTierIsIgnition())
             ? CodeKind::NATIVE_CONTEXT_INDEPENDENT
             : CodeKind::OPTIMIZED_FUNCTION;
}

bool JSFunction::CanDiscardCompiled() const {
  // Essentially, what we are asking here is, has this function been compiled
  // from JS code? We can currently tell only indirectly, by looking at
  // available code kinds. If any JS code kind exists, we can discard.
  //
  // Attached optimized code that is marked for deoptimization will not show up
  // in the list of available code kinds, thus we must check for it manually.
  //
  // Note that when the function has not yet been compiled we also return
  // false; that's fine, since nothing must be discarded in that case.
  if (code().kind() == CodeKind::OPTIMIZED_FUNCTION) return true;
  CodeKinds result = GetAvailableCodeKinds();
  return (result & kJSFunctionCodeKindsMask) != 0;
}

// static
MaybeHandle<NativeContext> JSBoundFunction::GetFunctionRealm(
    Handle<JSBoundFunction> function) {
  DCHECK(function->map().is_constructor());
  return JSReceiver::GetFunctionRealm(
      handle(function->bound_target_function(), function->GetIsolate()));
}

// static
MaybeHandle<String> JSBoundFunction::GetName(Isolate* isolate,
                                             Handle<JSBoundFunction> function) {
  Handle<String> prefix = isolate->factory()->bound__string();
  Handle<String> target_name = prefix;
  Factory* factory = isolate->factory();
  // Concatenate the "bound " up to the last non-bound target.
  while (function->bound_target_function().IsJSBoundFunction()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, target_name,
                               factory->NewConsString(prefix, target_name),
                               String);
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
  }
  if (function->bound_target_function().IsJSFunction()) {
    Handle<JSFunction> target(
        JSFunction::cast(function->bound_target_function()), isolate);
    Handle<Object> name = JSFunction::GetName(isolate, target);
    if (!name->IsString()) return target_name;
    return factory->NewConsString(target_name, Handle<String>::cast(name));
  }
  // This will omit the proper target name for bound JSProxies.
  return target_name;
}

// static
Maybe<int> JSBoundFunction::GetLength(Isolate* isolate,
                                      Handle<JSBoundFunction> function) {
  int nof_bound_arguments = function->bound_arguments().length();
  while (function->bound_target_function().IsJSBoundFunction()) {
    function = handle(JSBoundFunction::cast(function->bound_target_function()),
                      isolate);
    // Make sure we never overflow {nof_bound_arguments}, the number of
    // arguments of a function is strictly limited by the max length of an
    // JSAarray, Smi::kMaxValue is thus a reasonably good overestimate.
    int length = function->bound_arguments().length();
    if (V8_LIKELY(Smi::kMaxValue - nof_bound_arguments > length)) {
      nof_bound_arguments += length;
    } else {
      nof_bound_arguments = Smi::kMaxValue;
    }
  }
  // All non JSFunction targets get a direct property and don't use this
  // accessor.
  Handle<JSFunction> target(JSFunction::cast(function->bound_target_function()),
                            isolate);
  int target_length = target->length();

  int length = Max(0, target_length - nof_bound_arguments);
  return Just(length);
}

// static
Handle<String> JSBoundFunction::ToString(Handle<JSBoundFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  return isolate->factory()->function_native_code_string();
}

// static
Handle<Object> JSFunction::GetName(Isolate* isolate,
                                   Handle<JSFunction> function) {
  if (function->shared().name_should_print_as_anonymous()) {
    return isolate->factory()->anonymous_string();
  }
  return handle(function->shared().Name(), isolate);
}

// static
Handle<NativeContext> JSFunction::GetFunctionRealm(
    Handle<JSFunction> function) {
  DCHECK(function->map().is_constructor());
  return handle(function->context().native_context(), function->GetIsolate());
}

// static
void JSFunction::EnsureClosureFeedbackCellArray(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  DCHECK(function->shared().is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  if (function->has_closure_feedback_cell_array() ||
      function->has_feedback_vector()) {
    return;
  }
  if (function->shared().HasAsmWasmData()) return;

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());
  Handle<HeapObject> feedback_cell_array =
      ClosureFeedbackCellArray::New(isolate, shared);
  // Many closure cell is used as a way to specify that there is no
  // feedback cell for this function and a new feedback cell has to be
  // allocated for this funciton. For ex: for eval functions, we have to create
  // a feedback cell and cache it along with the code. It is safe to use
  // many_closure_cell to indicate this because in regular cases, it should
  // already have a feedback_vector / feedback cell array allocated.
  if (function->raw_feedback_cell() == isolate->heap()->many_closures_cell()) {
    Handle<FeedbackCell> feedback_cell =
        isolate->factory()->NewOneClosureCell(feedback_cell_array);
    function->set_raw_feedback_cell(*feedback_cell);
  } else {
    function->raw_feedback_cell().set_value(*feedback_cell_array);
  }
}

// static
void JSFunction::EnsureFeedbackVector(Handle<JSFunction> function,
                                      IsCompiledScope* is_compiled_scope) {
  Isolate* const isolate = function->GetIsolate();
  DCHECK(is_compiled_scope->is_compiled());
  DCHECK(function->shared().HasFeedbackMetadata());
  if (function->has_feedback_vector()) return;
  if (function->shared().HasAsmWasmData()) return;

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  DCHECK(function->shared().HasBytecodeArray());

  EnsureClosureFeedbackCellArray(function);
  Handle<ClosureFeedbackCellArray> closure_feedback_cell_array =
      handle(function->closure_feedback_cell_array(), isolate);
  Handle<HeapObject> feedback_vector = FeedbackVector::New(
      isolate, shared, closure_feedback_cell_array, is_compiled_scope);
  // EnsureClosureFeedbackCellArray should handle the special case where we need
  // to allocate a new feedback cell. Please look at comment in that function
  // for more details.
  DCHECK(function->raw_feedback_cell() !=
         isolate->heap()->many_closures_cell());
  function->raw_feedback_cell().set_value(*feedback_vector);
  function->raw_feedback_cell().SetInterruptBudget();
}

// static
void JSFunction::InitializeFeedbackCell(Handle<JSFunction> function,
                                        IsCompiledScope* is_compiled_scope) {
  Isolate* const isolate = function->GetIsolate();

  if (function->has_feedback_vector()) {
    CHECK_EQ(function->feedback_vector().length(),
             function->feedback_vector().metadata().slot_count());
    return;
  }

  const bool needs_feedback_vector =
      !FLAG_lazy_feedback_allocation || FLAG_always_opt ||
      function->shared().may_have_cached_code() ||
      // We also need a feedback vector for certain log events, collecting type
      // profile and more precise code coverage.
      FLAG_log_function_events || !isolate->is_best_effort_code_coverage() ||
      isolate->is_collecting_type_profile();

  if (needs_feedback_vector) {
    EnsureFeedbackVector(function, is_compiled_scope);
  } else {
    EnsureClosureFeedbackCellArray(function);
  }
}

namespace {

void SetInstancePrototype(Isolate* isolate, Handle<JSFunction> function,
                          Handle<JSReceiver> value) {
  // Now some logic for the maps of the objects that are created by using this
  // function as a constructor.
  if (function->has_initial_map()) {
    // If the function has allocated the initial map replace it with a
    // copy containing the new prototype.  Also complete any in-object
    // slack tracking that is in progress at this point because it is
    // still tracking the old copy.
    function->CompleteInobjectSlackTrackingIfActive();

    Handle<Map> initial_map(function->initial_map(), isolate);

    if (!isolate->bootstrapper()->IsActive() &&
        initial_map->instance_type() == JS_OBJECT_TYPE) {
      // Put the value in the initial map field until an initial map is needed.
      // At that point, a new initial map is created and the prototype is put
      // into the initial map where it belongs.
      function->set_prototype_or_initial_map(*value);
    } else {
      Handle<Map> new_map =
          Map::Copy(isolate, initial_map, "SetInstancePrototype");
      JSFunction::SetInitialMap(function, new_map, value);

      // If the function is used as the global Array function, cache the
      // updated initial maps (and transitioned versions) in the native context.
      Handle<Context> native_context(function->context().native_context(),
                                     isolate);
      Handle<Object> array_function(
          native_context->get(Context::ARRAY_FUNCTION_INDEX), isolate);
      if (array_function->IsJSFunction() &&
          *function == JSFunction::cast(*array_function)) {
        CacheInitialJSArrayMaps(isolate, native_context, new_map);
      }
    }

    // Deoptimize all code that embeds the previous initial map.
    initial_map->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kInitialMapChangedGroup);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    function->set_prototype_or_initial_map(*value);
    if (value->IsJSObject()) {
      // Optimize as prototype to detach it from its transition tree.
      JSObject::OptimizeAsPrototype(Handle<JSObject>::cast(value));
    }
  }
}

}  // anonymous namespace

void JSFunction::SetPrototype(Handle<JSFunction> function,
                              Handle<Object> value) {
  DCHECK(function->IsConstructor() ||
         IsGeneratorFunction(function->shared().kind()));
  Isolate* isolate = function->GetIsolate();
  Handle<JSReceiver> construct_prototype;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Handle<Map> new_map =
        Map::Copy(isolate, handle(function->map(), isolate), "SetPrototype");

    JSObject::MigrateToMap(isolate, function, new_map);
    new_map->SetConstructor(*value);
    new_map->set_has_non_instance_prototype(true);

    FunctionKind kind = function->shared().kind();
    Handle<Context> native_context(function->context().native_context(),
                                   isolate);

    construct_prototype = Handle<JSReceiver>(
        IsGeneratorFunction(kind)
            ? IsAsyncFunction(kind)
                  ? native_context->initial_async_generator_prototype()
                  : native_context->initial_generator_prototype()
            : native_context->initial_object_prototype(),
        isolate);
  } else {
    construct_prototype = Handle<JSReceiver>::cast(value);
    function->map().set_has_non_instance_prototype(false);
  }

  SetInstancePrototype(isolate, function, construct_prototype);
}

void JSFunction::SetInitialMap(Handle<JSFunction> function, Handle<Map> map,
                               Handle<HeapObject> prototype) {
  if (map->prototype() != *prototype)
    Map::SetPrototype(function->GetIsolate(), map, prototype);
  function->set_prototype_or_initial_map(*map);
  map->SetConstructor(*function);
  if (FLAG_trace_maps) {
    LOG(function->GetIsolate(), MapEvent("InitialMap", Handle<Map>(), map, "",
                                         handle(function->shared().DebugName(),
                                                function->GetIsolate())));
  }
}

void JSFunction::EnsureHasInitialMap(Handle<JSFunction> function) {
  DCHECK(function->has_prototype_slot());
  DCHECK(function->IsConstructor() ||
         IsResumableFunction(function->shared().kind()));
  if (function->has_initial_map()) return;
  Isolate* isolate = function->GetIsolate();

  int expected_nof_properties =
      CalculateExpectedNofProperties(isolate, function);

  // {CalculateExpectedNofProperties} can have had the side effect of creating
  // the initial map (e.g. it could have triggered an optimized compilation
  // whose dependency installation reentered {EnsureHasInitialMap}).
  if (function->has_initial_map()) return;

  // Create a new map with the size and number of in-object properties suggested
  // by the function.
  InstanceType instance_type;
  if (IsResumableFunction(function->shared().kind())) {
    instance_type = IsAsyncGeneratorFunction(function->shared().kind())
                        ? JS_ASYNC_GENERATOR_OBJECT_TYPE
                        : JS_GENERATOR_OBJECT_TYPE;
  } else {
    instance_type = JS_OBJECT_TYPE;
  }

  int instance_size;
  int inobject_properties;
  CalculateInstanceSizeHelper(instance_type, false, 0, expected_nof_properties,
                              &instance_size, &inobject_properties);

  Handle<Map> map = isolate->factory()->NewMap(instance_type, instance_size,
                                               TERMINAL_FAST_ELEMENTS_KIND,
                                               inobject_properties);

  // Fetch or allocate prototype.
  Handle<HeapObject> prototype;
  if (function->has_instance_prototype()) {
    prototype = handle(function->instance_prototype(), isolate);
  } else {
    prototype = isolate->factory()->NewFunctionPrototype(function);
  }
  DCHECK(map->has_fast_object_elements());

  // Finally link initial map and constructor function.
  DCHECK(prototype->IsJSReceiver());
  JSFunction::SetInitialMap(function, map, prototype);
  map->StartInobjectSlackTracking();
}

namespace {

#ifdef DEBUG
bool CanSubclassHaveInobjectProperties(InstanceType instance_type) {
  switch (instance_type) {
    case JS_API_OBJECT_TYPE:
    case JS_ARRAY_BUFFER_TYPE:
    case JS_ARRAY_TYPE:
    case JS_ASYNC_FROM_SYNC_ITERATOR_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_DATA_VIEW_TYPE:
    case JS_DATE_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GENERATOR_OBJECT_TYPE:
#ifdef V8_INTL_SUPPORT
    case JS_COLLATOR_TYPE:
    case JS_DATE_TIME_FORMAT_TYPE:
    case JS_DISPLAY_NAMES_TYPE:
    case JS_LIST_FORMAT_TYPE:
    case JS_LOCALE_TYPE:
    case JS_NUMBER_FORMAT_TYPE:
    case JS_PLURAL_RULES_TYPE:
    case JS_RELATIVE_TIME_FORMAT_TYPE:
    case JS_SEGMENT_ITERATOR_TYPE:
    case JS_SEGMENTER_TYPE:
    case JS_SEGMENTS_TYPE:
    case JS_V8_BREAK_ITERATOR_TYPE:
#endif
    case JS_ASYNC_FUNCTION_OBJECT_TYPE:
    case JS_ASYNC_GENERATOR_OBJECT_TYPE:
    case JS_MAP_TYPE:
    case JS_MESSAGE_OBJECT_TYPE:
    case JS_OBJECT_TYPE:
    case JS_ERROR_TYPE:
    case JS_FINALIZATION_REGISTRY_TYPE:
    case JS_ARGUMENTS_OBJECT_TYPE:
    case JS_PROMISE_TYPE:
    case JS_REG_EXP_TYPE:
    case JS_SET_TYPE:
    case JS_SPECIAL_API_OBJECT_TYPE:
    case JS_TYPED_ARRAY_TYPE:
    case JS_PRIMITIVE_WRAPPER_TYPE:
    case JS_WEAK_MAP_TYPE:
    case JS_WEAK_REF_TYPE:
    case JS_WEAK_SET_TYPE:
    case WASM_GLOBAL_OBJECT_TYPE:
    case WASM_INSTANCE_OBJECT_TYPE:
    case WASM_MEMORY_OBJECT_TYPE:
    case WASM_MODULE_OBJECT_TYPE:
    case WASM_TABLE_OBJECT_TYPE:
      return true;

    case BIGINT_TYPE:
    case OBJECT_BOILERPLATE_DESCRIPTION_TYPE:
    case BYTECODE_ARRAY_TYPE:
    case BYTE_ARRAY_TYPE:
    case CELL_TYPE:
    case CODE_TYPE:
    case FILLER_TYPE:
    case FIXED_ARRAY_TYPE:
    case SCRIPT_CONTEXT_TABLE_TYPE:
    case FIXED_DOUBLE_ARRAY_TYPE:
    case FEEDBACK_METADATA_TYPE:
    case FOREIGN_TYPE:
    case FREE_SPACE_TYPE:
    case HASH_TABLE_TYPE:
    case ORDERED_HASH_MAP_TYPE:
    case ORDERED_HASH_SET_TYPE:
    case ORDERED_NAME_DICTIONARY_TYPE:
    case NAME_DICTIONARY_TYPE:
    case GLOBAL_DICTIONARY_TYPE:
    case NUMBER_DICTIONARY_TYPE:
    case SIMPLE_NUMBER_DICTIONARY_TYPE:
    case HEAP_NUMBER_TYPE:
    case JS_BOUND_FUNCTION_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_PROXY_TYPE:
    case MAP_TYPE:
    case ODDBALL_TYPE:
    case PROPERTY_CELL_TYPE:
    case SHARED_FUNCTION_INFO_TYPE:
    case SYMBOL_TYPE:
    case ALLOCATION_SITE_TYPE:

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case FIXED_##TYPE##_ARRAY_TYPE:
#undef TYPED_ARRAY_CASE

#define MAKE_STRUCT_CASE(TYPE, Name, name) case TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      // We must not end up here for these instance types at all.
      UNREACHABLE();
    // Fall through.
    default:
      return false;
  }
}
#endif  // DEBUG

bool FastInitializeDerivedMap(Isolate* isolate, Handle<JSFunction> new_target,
                              Handle<JSFunction> constructor,
                              Handle<Map> constructor_initial_map) {
  // Use the default intrinsic prototype instead.
  if (!new_target->has_prototype_slot()) return false;
  // Check that |function|'s initial map still in sync with the |constructor|,
  // otherwise we must create a new initial map for |function|.
  if (new_target->has_initial_map() &&
      new_target->initial_map().GetConstructor() == *constructor) {
    DCHECK(new_target->instance_prototype().IsJSReceiver());
    return true;
  }
  InstanceType instance_type = constructor_initial_map->instance_type();
  DCHECK(CanSubclassHaveInobjectProperties(instance_type));
  // Create a new map with the size and number of in-object properties
  // suggested by |function|.

  // Link initial map and constructor function if the new.target is actually a
  // subclass constructor.
  if (!IsDerivedConstructor(new_target->shared().kind())) return false;

  int instance_size;
  int in_object_properties;
  int embedder_fields =
      JSObject::GetEmbedderFieldCount(*constructor_initial_map);
  // Constructor expects certain number of in-object properties to be in the
  // object. However, CalculateExpectedNofProperties() may return smaller value
  // if 1) the constructor is not in the prototype chain of new_target, or
  // 2) the prototype chain is modified during iteration, or 3) compilation
  // failure occur during prototype chain iteration.
  // So we take the maximum of two values.
  int expected_nof_properties =
      Max(static_cast<int>(constructor->shared().expected_nof_properties()),
          JSFunction::CalculateExpectedNofProperties(isolate, new_target));
  JSFunction::CalculateInstanceSizeHelper(
      instance_type, true, embedder_fields, expected_nof_properties,
      &instance_size, &in_object_properties);

  int pre_allocated = constructor_initial_map->GetInObjectProperties() -
                      constructor_initial_map->UnusedPropertyFields();
  CHECK_LE(constructor_initial_map->UsedInstanceSize(), instance_size);
  int unused_property_fields = in_object_properties - pre_allocated;
  Handle<Map> map =
      Map::CopyInitialMap(isolate, constructor_initial_map, instance_size,
                          in_object_properties, unused_property_fields);
  map->set_new_target_is_base(false);
  Handle<HeapObject> prototype(new_target->instance_prototype(), isolate);
  JSFunction::SetInitialMap(new_target, map, prototype);
  DCHECK(new_target->instance_prototype().IsJSReceiver());
  map->SetConstructor(*constructor);
  map->set_construction_counter(Map::kNoSlackTracking);
  map->StartInobjectSlackTracking();
  return true;
}

}  // namespace

// static
MaybeHandle<Map> JSFunction::GetDerivedMap(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<JSReceiver> new_target) {
  EnsureHasInitialMap(constructor);

  Handle<Map> constructor_initial_map(constructor->initial_map(), isolate);
  if (*new_target == *constructor) return constructor_initial_map;

  Handle<Map> result_map;
  // Fast case, new.target is a subclass of constructor. The map is cacheable
  // (and may already have been cached). new.target.prototype is guaranteed to
  // be a JSReceiver.
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (FastInitializeDerivedMap(isolate, function, constructor,
                                 constructor_initial_map)) {
      return handle(function->initial_map(), isolate);
    }
  }

  // Slow path, new.target is either a proxy or can't cache the map.
  // new.target.prototype is not guaranteed to be a JSReceiver, and may need to
  // fall back to the intrinsicDefaultProto.
  Handle<Object> prototype;
  if (new_target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(new_target);
    if (function->has_prototype_slot()) {
      // Make sure the new.target.prototype is cached.
      EnsureHasInitialMap(function);
      prototype = handle(function->prototype(), isolate);
    } else {
      // No prototype property, use the intrinsict default proto further down.
      prototype = isolate->factory()->undefined_value();
    }
  } else {
    Handle<String> prototype_string = isolate->factory()->prototype_string();
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, prototype,
        JSReceiver::GetProperty(isolate, new_target, prototype_string), Map);
    // The above prototype lookup might change the constructor and its
    // prototype, hence we have to reload the initial map.
    EnsureHasInitialMap(constructor);
    constructor_initial_map = handle(constructor->initial_map(), isolate);
  }

  // If prototype is not a JSReceiver, fetch the intrinsicDefaultProto from the
  // correct realm. Rather than directly fetching the .prototype, we fetch the
  // constructor that points to the .prototype. This relies on
  // constructor.prototype being FROZEN for those constructors.
  if (!prototype->IsJSReceiver()) {
    Handle<Context> context;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, context,
                               JSReceiver::GetFunctionRealm(new_target), Map);
    DCHECK(context->IsNativeContext());
    Handle<Object> maybe_index = JSReceiver::GetDataProperty(
        constructor, isolate->factory()->native_context_index_symbol());
    int index = maybe_index->IsSmi() ? Smi::ToInt(*maybe_index)
                                     : Context::OBJECT_FUNCTION_INDEX;
    Handle<JSFunction> realm_constructor(JSFunction::cast(context->get(index)),
                                         isolate);
    prototype = handle(realm_constructor->prototype(), isolate);
  }

  Handle<Map> map = Map::CopyInitialMap(isolate, constructor_initial_map);
  map->set_new_target_is_base(false);
  CHECK(prototype->IsJSReceiver());
  if (map->prototype() != *prototype)
    Map::SetPrototype(isolate, map, Handle<HeapObject>::cast(prototype));
  map->SetConstructor(*constructor);
  return map;
}

int JSFunction::ComputeInstanceSizeWithMinSlack(Isolate* isolate) {
  CHECK(has_initial_map());
  if (initial_map().IsInobjectSlackTrackingInProgress()) {
    int slack = initial_map().ComputeMinObjectSlack(isolate);
    return initial_map().InstanceSizeFromSlack(slack);
  }
  return initial_map().instance_size();
}

void JSFunction::PrintName(FILE* out) {
  std::unique_ptr<char[]> name = shared().DebugName().ToCString();
  PrintF(out, "%s", name.get());
}

Handle<String> JSFunction::GetName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name =
      JSReceiver::GetDataProperty(function, isolate->factory()->name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return handle(function->shared().DebugName(), isolate);
}

Handle<String> JSFunction::GetDebugName(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<Object> name = JSReceiver::GetDataProperty(
      function, isolate->factory()->display_name_string());
  if (name->IsString()) return Handle<String>::cast(name);
  return JSFunction::GetName(function);
}

bool JSFunction::SetName(Handle<JSFunction> function, Handle<Name> name,
                         Handle<String> prefix) {
  Isolate* isolate = function->GetIsolate();
  Handle<String> function_name;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name,
                                   Name::ToFunctionName(isolate, name), false);
  if (prefix->length() > 0) {
    IncrementalStringBuilder builder(isolate);
    builder.AppendString(prefix);
    builder.AppendCharacter(' ');
    builder.AppendString(function_name);
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(isolate, function_name, builder.Finish(),
                                     false);
  }
  RETURN_ON_EXCEPTION_VALUE(
      isolate,
      JSObject::DefinePropertyOrElementIgnoreAttributes(
          function, isolate->factory()->name_string(), function_name,
          static_cast<PropertyAttributes>(DONT_ENUM | READ_ONLY)),
      false);
  return true;
}

namespace {

Handle<String> NativeCodeFunctionSourceString(
    Handle<SharedFunctionInfo> shared_info) {
  Isolate* const isolate = shared_info->GetIsolate();
  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("function ");
  builder.AppendString(handle(shared_info->Name(), isolate));
  builder.AppendCString("() { [native code] }");
  return builder.Finish().ToHandleChecked();
}

}  // namespace

// static
Handle<String> JSFunction::ToString(Handle<JSFunction> function) {
  Isolate* const isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  // Check if {function} should hide its source code.
  if (!shared_info->IsUserJavaScript()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // Check if we should print {function} as a class.
  Handle<Object> maybe_class_positions = JSReceiver::GetDataProperty(
      function, isolate->factory()->class_positions_symbol());
  if (maybe_class_positions->IsClassPositions()) {
    ClassPositions class_positions =
        ClassPositions::cast(*maybe_class_positions);
    int start_position = class_positions.start();
    int end_position = class_positions.end();
    Handle<String> script_source(
        String::cast(Script::cast(shared_info->script()).source()), isolate);
    return isolate->factory()->NewSubString(script_source, start_position,
                                            end_position);
  }

  // Check if we have source code for the {function}.
  if (!shared_info->HasSourceCode()) {
    return NativeCodeFunctionSourceString(shared_info);
  }

  // If this function was compiled from asm.js, use the recorded offset
  // information.
  if (shared_info->HasWasmExportedFunctionData()) {
    Handle<WasmExportedFunctionData> function_data(
        shared_info->wasm_exported_function_data(), isolate);
    const wasm::WasmModule* module = function_data->instance().module();
    if (is_asmjs_module(module)) {
      std::pair<int, int> offsets =
          module->asm_js_offset_information->GetFunctionOffsets(
              declared_function_index(module, function_data->function_index()));
      Handle<String> source(
          String::cast(Script::cast(shared_info->script()).source()), isolate);
      return isolate->factory()->NewSubString(source, offsets.first,
                                              offsets.second);
    }
  }

  if (shared_info->function_token_position() == kNoSourcePosition) {
    // If the function token position isn't valid, return [native code] to
    // ensure calling eval on the returned source code throws rather than
    // giving inconsistent call behaviour.
    isolate->CountUsage(
        v8::Isolate::UseCounterFeature::kFunctionTokenOffsetTooLongForToString);
    return NativeCodeFunctionSourceString(shared_info);
  }
  return Handle<String>::cast(
      SharedFunctionInfo::GetSourceCodeHarmony(shared_info));
}

// static
int JSFunction::CalculateExpectedNofProperties(Isolate* isolate,
                                               Handle<JSFunction> function) {
  int expected_nof_properties = 0;
  for (PrototypeIterator iter(isolate, function, kStartAtReceiver);
       !iter.IsAtEnd(); iter.Advance()) {
    Handle<JSReceiver> current =
        PrototypeIterator::GetCurrent<JSReceiver>(iter);
    if (!current->IsJSFunction()) break;
    Handle<JSFunction> func = Handle<JSFunction>::cast(current);
    // The super constructor should be compiled for the number of expected
    // properties to be available.
    Handle<SharedFunctionInfo> shared(func->shared(), isolate);
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
    if (is_compiled_scope.is_compiled() ||
        Compiler::Compile(func, Compiler::CLEAR_EXCEPTION,
                          &is_compiled_scope)) {
      DCHECK(shared->is_compiled());
      int count = shared->expected_nof_properties();
      // Check that the estimate is sensible.
      if (expected_nof_properties <= JSObject::kMaxInObjectProperties - count) {
        expected_nof_properties += count;
      } else {
        return JSObject::kMaxInObjectProperties;
      }
    } else {
      // In case there was a compilation error proceed iterating in case there
      // will be a builtin function in the prototype chain that requires
      // certain number of in-object properties.
      continue;
    }
  }
  // Inobject slack tracking will reclaim redundant inobject space
  // later, so we can afford to adjust the estimate generously,
  // meaning we over-allocate by at least 8 slots in the beginning.
  if (expected_nof_properties > 0) {
    expected_nof_properties += 8;
    if (expected_nof_properties > JSObject::kMaxInObjectProperties) {
      expected_nof_properties = JSObject::kMaxInObjectProperties;
    }
  }
  return expected_nof_properties;
}

// static
void JSFunction::CalculateInstanceSizeHelper(InstanceType instance_type,
                                             bool has_prototype_slot,
                                             int requested_embedder_fields,
                                             int requested_in_object_properties,
                                             int* instance_size,
                                             int* in_object_properties) {
  DCHECK_LE(static_cast<unsigned>(requested_embedder_fields),
            JSObject::kMaxEmbedderFields);
  int header_size = JSObject::GetHeaderSize(instance_type, has_prototype_slot);
  if (requested_embedder_fields) {
    // If there are embedder fields, then the embedder fields start offset must
    // be properly aligned (embedder fields are located between object header
    // and inobject fields).
    header_size = RoundUp<kSystemPointerSize>(header_size);
    requested_embedder_fields *= kEmbedderDataSlotSizeInTaggedSlots;
  }
  int max_nof_fields =
      (JSObject::kMaxInstanceSize - header_size) >> kTaggedSizeLog2;
  CHECK_LE(max_nof_fields, JSObject::kMaxInObjectProperties);
  CHECK_LE(static_cast<unsigned>(requested_embedder_fields),
           static_cast<unsigned>(max_nof_fields));
  *in_object_properties = Min(requested_in_object_properties,
                              max_nof_fields - requested_embedder_fields);
  *instance_size =
      header_size +
      ((requested_embedder_fields + *in_object_properties) << kTaggedSizeLog2);
  CHECK_EQ(*in_object_properties,
           ((*instance_size - header_size) >> kTaggedSizeLog2) -
               requested_embedder_fields);
  CHECK_LE(static_cast<unsigned>(*instance_size),
           static_cast<unsigned>(JSObject::kMaxInstanceSize));
}

void JSFunction::ClearTypeFeedbackInfo() {
  ResetIfBytecodeFlushed();
  if (has_feedback_vector()) {
    FeedbackVector vector = feedback_vector();
    Isolate* isolate = GetIsolate();
    if (vector.ClearSlots(isolate)) {
      IC::OnFeedbackChanged(isolate, vector, FeedbackSlot::Invalid(),
                            "ClearTypeFeedbackInfo");
    }
  }
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"
