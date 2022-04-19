// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic.h"

#include "src/api/api-arguments-inl.h"
#include "src/api/api.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/builtins/accessors.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/execution/tiering-manager.h"
#include "src/handles/handles-inl.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-stats.h"
#include "src/ic/stub-cache.h"
#include "src/numbers/conversions.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/instance-type.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/megadom-handler.h"
#include "src/objects/module-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/prototype.h"
#include "src/objects/struct-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/tracing-category-observer.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/struct-types.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

// Aliases to avoid having to repeat the class.
// With C++20 we can use "using" to introduce scoped enums.
constexpr InlineCacheState NO_FEEDBACK = InlineCacheState::NO_FEEDBACK;
constexpr InlineCacheState UNINITIALIZED = InlineCacheState::UNINITIALIZED;
constexpr InlineCacheState MONOMORPHIC = InlineCacheState::MONOMORPHIC;
constexpr InlineCacheState RECOMPUTE_HANDLER =
    InlineCacheState::RECOMPUTE_HANDLER;
constexpr InlineCacheState POLYMORPHIC = InlineCacheState::POLYMORPHIC;
constexpr InlineCacheState MEGAMORPHIC = InlineCacheState::MEGAMORPHIC;
constexpr InlineCacheState MEGADOM = InlineCacheState::MEGADOM;
constexpr InlineCacheState GENERIC = InlineCacheState::GENERIC;

char IC::TransitionMarkFromState(IC::State state) {
  switch (state) {
    case NO_FEEDBACK:
      return 'X';
    case UNINITIALIZED:
      return '0';
    case MONOMORPHIC:
      return '1';
    case RECOMPUTE_HANDLER:
      return '^';
    case POLYMORPHIC:
      return 'P';
    case MEGAMORPHIC:
      return 'N';
    case MEGADOM:
      return 'D';
    case GENERIC:
      return 'G';
  }
  UNREACHABLE();
}

namespace {

const char* GetModifier(KeyedAccessLoadMode mode) {
  if (mode == LOAD_IGNORE_OUT_OF_BOUNDS) return ".IGNORE_OOB";
  return "";
}

const char* GetModifier(KeyedAccessStoreMode mode) {
  switch (mode) {
    case STORE_HANDLE_COW:
      return ".COW";
    case STORE_AND_GROW_HANDLE_COW:
      return ".STORE+COW";
    case STORE_IGNORE_OUT_OF_BOUNDS:
      return ".IGNORE_OOB";
    case STANDARD_STORE:
      return "";
  }
  UNREACHABLE();
}

}  // namespace

void IC::TraceIC(const char* type, Handle<Object> name) {
  if (V8_LIKELY(!TracingFlags::is_ic_stats_enabled())) return;
  State new_state =
      (state() == NO_FEEDBACK) ? NO_FEEDBACK : nexus()->ic_state();
  TraceIC(type, name, state(), new_state);
}

void IC::TraceIC(const char* type, Handle<Object> name, State old_state,
                 State new_state) {
  if (V8_LIKELY(!TracingFlags::is_ic_stats_enabled())) return;

  Handle<Map> map = lookup_start_object_map();  // Might be empty.

  const char* modifier = "";
  if (state() == NO_FEEDBACK) {
    modifier = "";
  } else if (IsKeyedLoadIC()) {
    KeyedAccessLoadMode mode = nexus()->GetKeyedAccessLoadMode();
    modifier = GetModifier(mode);
  } else if (IsKeyedStoreIC() || IsStoreInArrayLiteralIC() ||
             IsDefineKeyedOwnIC()) {
    KeyedAccessStoreMode mode = nexus()->GetKeyedAccessStoreMode();
    modifier = GetModifier(mode);
  }

  bool keyed_prefix = is_keyed() && !IsStoreInArrayLiteralIC();

  if (!(TracingFlags::ic_stats.load(std::memory_order_relaxed) &
        v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    LOG(isolate(), ICEvent(type, keyed_prefix, map, name,
                           TransitionMarkFromState(old_state),
                           TransitionMarkFromState(new_state), modifier,
                           slow_stub_reason_));
    return;
  }

  JavaScriptFrameIterator it(isolate());
  JavaScriptFrame* frame = it.frame();

  DisallowGarbageCollection no_gc;
  JSFunction function = frame->function();

  ICStats::instance()->Begin();
  ICInfo& ic_info = ICStats::instance()->Current();
  ic_info.type = keyed_prefix ? "Keyed" : "";
  ic_info.type += type;

  int code_offset = 0;
  AbstractCode code = function.abstract_code(isolate_);
  if (function.ActiveTierIsIgnition()) {
    code_offset = InterpretedFrame::GetBytecodeOffset(frame->fp());
  } else if (function.ActiveTierIsBaseline()) {
    // TODO(pthier): AbstractCode should fully support Baseline code.
    BaselineFrame* baseline_frame = BaselineFrame::cast(frame);
    code_offset = baseline_frame->GetBytecodeOffset();
    code = AbstractCode::cast(baseline_frame->GetBytecodeArray());
  } else {
    code_offset = static_cast<int>(frame->pc() - function.code_entry_point());
  }
  JavaScriptFrame::CollectFunctionAndOffsetForICStats(function, code,
                                                      code_offset);

  // Reserve enough space for IC transition state, the longest length is 17.
  ic_info.state.reserve(17);
  ic_info.state = "(";
  ic_info.state += TransitionMarkFromState(old_state);
  ic_info.state += "->";
  ic_info.state += TransitionMarkFromState(new_state);
  ic_info.state += modifier;
  ic_info.state += ")";
  if (!map.is_null()) {
    ic_info.map = reinterpret_cast<void*>(map->ptr());
    ic_info.is_dictionary_map = map->is_dictionary_map();
    ic_info.number_of_own_descriptors = map->NumberOfOwnDescriptors();
    ic_info.instance_type = std::to_string(map->instance_type());
  } else {
    ic_info.map = nullptr;
  }
  // TODO(lpy) Add name as key field in ICStats.
  ICStats::instance()->End();
}

IC::IC(Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot slot,
       FeedbackSlotKind kind)
    : isolate_(isolate),
      vector_set_(false),
      kind_(kind),
      target_maps_set_(false),
      slow_stub_reason_(nullptr),
      nexus_(vector, slot) {
  DCHECK_IMPLIES(!vector.is_null(), kind_ == nexus_.kind());
  state_ = (vector.is_null()) ? NO_FEEDBACK : nexus_.ic_state();
  old_state_ = state_;
}

static void LookupForRead(LookupIterator* it, bool is_has_property) {
  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return;
      case LookupIterator::INTERCEPTOR: {
        // If there is a getter, return; otherwise loop to perform the lookup.
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        if (!holder->GetNamedInterceptor().getter().IsUndefined(
                it->isolate())) {
          return;
        }
        if (is_has_property &&
            !holder->GetNamedInterceptor().query().IsUndefined(it->isolate())) {
          return;
        }
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        // ICs know how to perform access checks on global proxies.
        if (it->GetHolder<JSObject>()->IsJSGlobalProxy() && it->HasAccess()) {
          break;
        }
        return;
      case LookupIterator::ACCESSOR:
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
      case LookupIterator::DATA:
        return;
    }
  }
}

bool IC::ShouldRecomputeHandler(Handle<String> name) {
  if (!RecomputeHandlerForName(name)) return false;

  // This is a contextual access, always just update the handler and stay
  // monomorphic.
  if (IsGlobalIC()) return true;

  MaybeObjectHandle maybe_handler =
      nexus()->FindHandlerForMap(lookup_start_object_map());

  // The current map wasn't handled yet. There's no reason to stay monomorphic,
  // *unless* we're moving from a deprecated map to its replacement, or
  // to a more general elements kind.
  // TODO(verwaest): Check if the current map is actually what the old map
  // would transition to.
  if (maybe_handler.is_null()) {
    if (!lookup_start_object_map()->IsJSObjectMap()) return false;
    Map first_map = FirstTargetMap();
    if (first_map.is_null()) return false;
    Handle<Map> old_map(first_map, isolate());
    if (old_map->is_deprecated()) return true;
    return IsMoreGeneralElementsKindTransition(
        old_map->elements_kind(), lookup_start_object_map()->elements_kind());
  }

  return true;
}

bool IC::RecomputeHandlerForName(Handle<Object> name) {
  if (is_keyed()) {
    // Determine whether the failure is due to a name failure.
    if (!name->IsName()) return false;
    Name stub_name = nexus()->GetName();
    if (*name != stub_name) return false;
  }

  return true;
}

void IC::UpdateState(Handle<Object> lookup_start_object, Handle<Object> name) {
  if (state() == NO_FEEDBACK) return;
  update_lookup_start_object_map(lookup_start_object);
  if (!name->IsString()) return;
  if (state() != MONOMORPHIC && state() != POLYMORPHIC) return;
  if (lookup_start_object->IsNullOrUndefined(isolate())) return;

  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  if (ShouldRecomputeHandler(Handle<String>::cast(name))) {
    MarkRecomputeHandler(name);
  }
}

MaybeHandle<Object> IC::TypeError(MessageTemplate index, Handle<Object> object,
                                  Handle<Object> key) {
  HandleScope scope(isolate());
  THROW_NEW_ERROR(isolate(), NewTypeError(index, key, object), Object);
}

MaybeHandle<Object> IC::ReferenceError(Handle<Name> name) {
  HandleScope scope(isolate());
  THROW_NEW_ERROR(
      isolate(), NewReferenceError(MessageTemplate::kNotDefined, name), Object);
}

void IC::OnFeedbackChanged(const char* reason) {
  vector_set_ = true;
  FeedbackVector vector = nexus()->vector();
  FeedbackSlot slot = nexus()->slot();
  OnFeedbackChanged(isolate(), vector, slot, reason);
}

// static
void IC::OnFeedbackChanged(Isolate* isolate, FeedbackVector vector,
                           FeedbackSlot slot, const char* reason) {
  if (FLAG_trace_opt_verbose) {
    if (vector.profiler_ticks() != 0) {
      StdoutStream os;
      os << "[resetting ticks for ";
      vector.shared_function_info().ShortPrint(os);
      os << " from " << vector.profiler_ticks()
         << " due to IC change: " << reason << "]" << std::endl;
    }
  }
  vector.set_profiler_ticks(0);

#ifdef V8_TRACE_FEEDBACK_UPDATES
  if (FLAG_trace_feedback_updates) {
    int slot_count = vector.metadata().slot_count();
    StdoutStream os;
    if (slot.IsInvalid()) {
      os << "[Feedback slots in ";
    } else {
      os << "[Feedback slot " << slot.ToInt() << "/" << slot_count << " in ";
    }
    vector.shared_function_info().ShortPrint(os);
    if (slot.IsInvalid()) {
      os << " updated - ";
    } else {
      os << " updated to ";
      vector.FeedbackSlotPrint(os, slot);
      os << " - ";
    }
    os << reason << "]" << std::endl;
  }
#endif

  isolate->tiering_manager()->NotifyICChanged();
}

namespace {

bool MigrateDeprecated(Isolate* isolate, Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map().is_deprecated()) return false;
  JSObject::MigrateInstance(isolate, receiver);
  return true;
}

}  // namespace

bool IC::ConfigureVectorState(IC::State new_state, Handle<Object> key) {
  DCHECK_EQ(MEGAMORPHIC, new_state);
  DCHECK_IMPLIES(!is_keyed(), key->IsName());
  // Even though we don't change the feedback data, we still want to reset the
  // profiler ticks. Real-world observations suggest that optimizing these
  // functions doesn't improve performance.
  bool changed = nexus()->ConfigureMegamorphic(
      key->IsName() ? IcCheckType::kProperty : IcCheckType::kElement);
  OnFeedbackChanged("Megamorphic");
  return changed;
}

void IC::ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                              Handle<Object> handler) {
  ConfigureVectorState(name, map, MaybeObjectHandle(handler));
}

void IC::ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                              const MaybeObjectHandle& handler) {
  if (IsGlobalIC()) {
    nexus()->ConfigureHandlerMode(handler);
  } else {
    // Non-keyed ICs don't track the name explicitly.
    if (!is_keyed()) name = Handle<Name>::null();
    nexus()->ConfigureMonomorphic(name, map, handler);
  }

  OnFeedbackChanged(IsLoadGlobalIC() ? "LoadGlobal" : "Monomorphic");
}

void IC::ConfigureVectorState(Handle<Name> name, MapHandles const& maps,
                              MaybeObjectHandles* handlers) {
  DCHECK(!IsGlobalIC());
  std::vector<MapAndHandler> maps_and_handlers;
  DCHECK_EQ(maps.size(), handlers->size());
  for (size_t i = 0; i < maps.size(); i++) {
    maps_and_handlers.push_back(MapAndHandler(maps[i], handlers->at(i)));
  }
  ConfigureVectorState(name, maps_and_handlers);
}

void IC::ConfigureVectorState(
    Handle<Name> name, std::vector<MapAndHandler> const& maps_and_handlers) {
  DCHECK(!IsGlobalIC());
  // Non-keyed ICs don't track the name explicitly.
  if (!is_keyed()) name = Handle<Name>::null();
  nexus()->ConfigurePolymorphic(name, maps_and_handlers);

  OnFeedbackChanged("Polymorphic");
}

MaybeHandle<Object> LoadIC::Load(Handle<Object> object, Handle<Name> name,
                                 bool update_feedback,
                                 Handle<Object> receiver) {
  bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic && update_feedback;

  if (receiver.is_null()) {
    receiver = object;
  }

  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (IsAnyHas() ? !object->IsJSReceiver()
                 : object->IsNullOrUndefined(isolate())) {
    if (use_ic) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), LoadIC_NonReceiver);
      update_lookup_start_object_map(object);
      SetCache(name, LoadHandler::LoadSlow(isolate()));
      TraceIC("LoadIC", name);
    }

    if (*name == ReadOnlyRoots(isolate()).iterator_symbol()) {
      return isolate()->Throw<Object>(
          ErrorUtils::NewIteratorError(isolate(), object));
    }

    if (IsAnyHas()) {
      return TypeError(MessageTemplate::kInvalidInOperatorUse, object, name);
    } else {
      DCHECK(object->IsNullOrUndefined(isolate()));
      ErrorUtils::ThrowLoadFromNullOrUndefined(isolate(), object, name);
      return MaybeHandle<Object>();
    }
  }

  // If we encounter an object with a deprecated map, we want to update the
  // feedback vector with the migrated map.
  // Mark ourselves as RECOMPUTE_HANDLER so that we don't turn megamorphic due
  // to seeing the same map and handler.
  if (MigrateDeprecated(isolate(), object)) {
    UpdateState(object, name);
  }

  JSObject::MakePrototypesFast(object, kStartAtReceiver, isolate());
  update_lookup_start_object_map(object);

  PropertyKey key(isolate(), name);
  LookupIterator it = LookupIterator(isolate(), receiver, key, object);

  // Named lookup in the object.
  LookupForRead(&it, IsAnyHas());

  if (name->IsPrivate()) {
    Handle<Symbol> private_symbol = Handle<Symbol>::cast(name);
    if (!IsAnyHas() && private_symbol->is_private_name() && !it.IsFound()) {
      Handle<String> name_string(String::cast(private_symbol->description()),
                                 isolate());
      if (private_symbol->is_private_brand()) {
        Handle<String> class_name =
            (name_string->length() == 0)
                ? isolate()->factory()->anonymous_string()
                : name_string;
        return TypeError(MessageTemplate::kInvalidPrivateBrandInstance, object,
                         class_name);
      }
      return TypeError(MessageTemplate::kInvalidPrivateMemberRead, object,
                       name_string);
    }

    // IC handling of private symbols/fields lookup on JSProxy is not
    // supported.
    if (object->IsJSProxy()) {
      use_ic = false;
    }
  }

  if (it.IsFound() || !ShouldThrowReferenceError()) {
    // Update inline cache and stub cache.
    if (use_ic) {
      UpdateCaches(&it);
    } else if (state() == NO_FEEDBACK) {
      // Tracing IC stats
      IsLoadGlobalIC() ? TraceIC("LoadGlobalIC", name)
                       : TraceIC("LoadIC", name);
    }

    if (IsAnyHas()) {
      // Named lookup in the object.
      Maybe<bool> maybe = JSReceiver::HasProperty(&it);
      if (maybe.IsNothing()) return MaybeHandle<Object>();
      return maybe.FromJust() ? ReadOnlyRoots(isolate()).true_value_handle()
                              : ReadOnlyRoots(isolate()).false_value_handle();
    }

    // Get the property.
    Handle<Object> result;

    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Object::GetProperty(&it, IsLoadGlobalIC()), Object);
    if (it.IsFound()) {
      return result;
    } else if (!ShouldThrowReferenceError()) {
      return result;
    }
  }
  return ReferenceError(name);
}

MaybeHandle<Object> LoadGlobalIC::Load(Handle<Name> name,
                                       bool update_feedback) {
  Handle<JSGlobalObject> global = isolate()->global_object();

  if (name->IsString()) {
    // Look up in script context table.
    Handle<String> str_name = Handle<String>::cast(name);
    Handle<ScriptContextTable> script_contexts(
        global->native_context().script_context_table(), isolate());

    VariableLookupResult lookup_result;
    if (script_contexts->Lookup(str_name, &lookup_result)) {
      Handle<Context> script_context = ScriptContextTable::GetContext(
          isolate(), script_contexts, lookup_result.context_index);

      Handle<Object> result(script_context->get(lookup_result.slot_index),
                            isolate());

      if (result->IsTheHole(isolate())) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        THROW_NEW_ERROR(
            isolate(),
            NewReferenceError(MessageTemplate::kAccessedUninitializedVariable,
                              name),
            Object);
      }

      bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic && update_feedback;
      if (use_ic) {
        // 'const' Variables are mutable if REPL mode is enabled. This disables
        // compiler inlining for all 'const' variables declared in REPL mode.
        if (nexus()->ConfigureLexicalVarMode(
                lookup_result.context_index, lookup_result.slot_index,
                (lookup_result.mode == VariableMode::kConst &&
                 !lookup_result.is_repl_mode))) {
          TRACE_HANDLER_STATS(isolate(), LoadGlobalIC_LoadScriptContextField);
        } else {
          // Given combination of indices can't be encoded, so use slow stub.
          TRACE_HANDLER_STATS(isolate(), LoadGlobalIC_SlowStub);
          SetCache(name, LoadHandler::LoadSlow(isolate()));
        }
        TraceIC("LoadGlobalIC", name);
      } else if (state() == NO_FEEDBACK) {
        TraceIC("LoadGlobalIC", name);
      }
      return result;
    }
  }
  return LoadIC::Load(global, name, update_feedback);
}

static bool AddOneReceiverMapIfMissing(MapHandles* receiver_maps,
                                       Handle<Map> new_receiver_map) {
  DCHECK(!new_receiver_map.is_null());
  for (Handle<Map> map : *receiver_maps) {
    if (!map.is_null() && map.is_identical_to(new_receiver_map)) {
      return false;
    }
  }
  receiver_maps->push_back(new_receiver_map);
  return true;
}

static bool AddOneReceiverMapIfMissing(
    std::vector<MapAndHandler>* receiver_maps_and_handlers,
    Handle<Map> new_receiver_map) {
  DCHECK(!new_receiver_map.is_null());
  if (new_receiver_map->is_deprecated()) return false;
  for (MapAndHandler map_and_handler : *receiver_maps_and_handlers) {
    Handle<Map> map = map_and_handler.first;
    if (!map.is_null() && map.is_identical_to(new_receiver_map)) {
      return false;
    }
  }
  receiver_maps_and_handlers->push_back(
      MapAndHandler(new_receiver_map, MaybeObjectHandle()));
  return true;
}

bool IC::UpdateMegaDOMIC(const MaybeObjectHandle& handler, Handle<Name> name) {
  if (!FLAG_enable_mega_dom_ic) return false;

  // TODO(gsathya): Enable fuzzing once this feature is more stable.
  if (FLAG_fuzzing) return false;

  // TODO(gsathya): Support KeyedLoadIC, StoreIC and KeyedStoreIC.
  if (!IsLoadIC()) return false;

  // Check if DOM protector cell is valid.
  if (!Protectors::IsMegaDOMIntact(isolate())) return false;

  // Check if current lookup object is an API object
  Handle<Map> map = lookup_start_object_map();
  if (!InstanceTypeChecker::IsJSApiObject(map->instance_type())) return false;

  Handle<Object> accessor_obj;
  // TODO(gsathya): Check if there are overloads possible for this accessor and
  // transition only if it isn't possible.
  if (!accessor().ToHandle(&accessor_obj)) return false;

  // TODO(gsathya): This is also created in IC::ComputeHandler, find a way to
  // reuse it here.
  CallOptimization call_optimization(isolate(), accessor_obj);

  // Check if accessor is an API function
  if (!call_optimization.is_simple_api_call()) return false;

  // Check if accessor requires access checks
  if (call_optimization.accept_any_receiver()) return false;

  // Check if accessor requires signature checks
  if (!call_optimization.requires_signature_check()) return false;

  // Check if the receiver is the holder
  CallOptimization::HolderLookup holder_lookup;
  call_optimization.LookupHolderOfExpectedType(isolate(), map, &holder_lookup);
  if (holder_lookup != CallOptimization::kHolderIsReceiver) return false;

  Handle<Context> accessor_context(call_optimization.GetAccessorContext(*map),
                                   isolate());

  Handle<MegaDomHandler> new_handler = isolate()->factory()->NewMegaDomHandler(
      MaybeObjectHandle::Weak(accessor_obj),
      MaybeObjectHandle::Weak(accessor_context));
  nexus()->ConfigureMegaDOM(MaybeObjectHandle(new_handler));
  return true;
}

bool IC::UpdatePolymorphicIC(Handle<Name> name,
                             const MaybeObjectHandle& handler) {
  DCHECK(IsHandler(*handler));
  if (is_keyed() && state() != RECOMPUTE_HANDLER) {
    if (nexus()->GetName() != *name) return false;
  }
  Handle<Map> map = lookup_start_object_map();

  std::vector<MapAndHandler> maps_and_handlers;
  maps_and_handlers.reserve(FLAG_max_valid_polymorphic_map_count);
  int deprecated_maps = 0;
  int handler_to_overwrite = -1;

  {
    DisallowGarbageCollection no_gc;
    int i = 0;
    for (FeedbackIterator it(nexus()); !it.done(); it.Advance()) {
      if (it.handler()->IsCleared()) continue;
      MaybeObjectHandle existing_handler = handle(it.handler(), isolate());
      Handle<Map> existing_map = handle(it.map(), isolate());

      maps_and_handlers.push_back(
          MapAndHandler(existing_map, existing_handler));

      if (existing_map->is_deprecated()) {
        // Filter out deprecated maps to ensure their instances get migrated.
        deprecated_maps++;
      } else if (map.is_identical_to(existing_map)) {
        // If both map and handler stayed the same (and the name is also the
        // same as checked above, for keyed accesses), we're not progressing
        // in the lattice and need to go MEGAMORPHIC instead. There's one
        // exception to this rule, which is when we're in RECOMPUTE_HANDLER
        // state, there we allow to migrate to a new handler.
        if (handler.is_identical_to(existing_handler) &&
            state() != RECOMPUTE_HANDLER) {
          return false;
        }

        // If the receiver type is already in the polymorphic IC, this indicates
        // there was a prototoype chain failure. In that case, just overwrite
        // the handler.
        handler_to_overwrite = i;
      } else if (handler_to_overwrite == -1 &&
                 IsTransitionOfMonomorphicTarget(*existing_map, *map)) {
        handler_to_overwrite = i;
      }

      i++;
    }
    DCHECK_LE(i, maps_and_handlers.size());
  }

  int number_of_maps = static_cast<int>(maps_and_handlers.size());
  int number_of_valid_maps =
      number_of_maps - deprecated_maps - (handler_to_overwrite != -1);

  if (number_of_valid_maps >= FLAG_max_valid_polymorphic_map_count)
    return false;
  if (number_of_maps == 0 && state() != MONOMORPHIC && state() != POLYMORPHIC) {
    return false;
  }

  number_of_valid_maps++;
  if (number_of_valid_maps == 1) {
    ConfigureVectorState(name, lookup_start_object_map(), handler);
  } else {
    if (is_keyed() && nexus()->GetName() != *name) return false;
    if (handler_to_overwrite >= 0) {
      maps_and_handlers[handler_to_overwrite].second = handler;
      if (!map.is_identical_to(
              maps_and_handlers.at(handler_to_overwrite).first)) {
        maps_and_handlers[handler_to_overwrite].first = map;
      }
    } else {
      maps_and_handlers.push_back(MapAndHandler(map, handler));
    }

    ConfigureVectorState(name, maps_and_handlers);
  }

  return true;
}

void IC::UpdateMonomorphicIC(const MaybeObjectHandle& handler,
                             Handle<Name> name) {
  DCHECK(IsHandler(*handler));
  ConfigureVectorState(name, lookup_start_object_map(), handler);
}

void IC::CopyICToMegamorphicCache(Handle<Name> name) {
  std::vector<MapAndHandler> maps_and_handlers;
  nexus()->ExtractMapsAndHandlers(&maps_and_handlers);
  for (const MapAndHandler& map_and_handler : maps_and_handlers) {
    UpdateMegamorphicCache(map_and_handler.first, name, map_and_handler.second);
  }
}

bool IC::IsTransitionOfMonomorphicTarget(Map source_map, Map target_map) {
  if (source_map.is_null()) return true;
  if (target_map.is_null()) return false;
  if (source_map.is_abandoned_prototype_map()) return false;
  ElementsKind target_elements_kind = target_map.elements_kind();
  bool more_general_transition = IsMoreGeneralElementsKindTransition(
      source_map.elements_kind(), target_elements_kind);
  Map transitioned_map;
  if (more_general_transition) {
    MapHandles map_list;
    map_list.push_back(handle(target_map, isolate_));
    transitioned_map = source_map.FindElementsKindTransitionedMap(
        isolate(), map_list, ConcurrencyMode::kSynchronous);
  }
  return transitioned_map == target_map;
}

void IC::SetCache(Handle<Name> name, Handle<Object> handler) {
  SetCache(name, MaybeObjectHandle(handler));
}

void IC::SetCache(Handle<Name> name, const MaybeObjectHandle& handler) {
  DCHECK(IsHandler(*handler));
  // Currently only load and store ICs support non-code handlers.
  DCHECK(IsAnyLoad() || IsAnyStore() || IsAnyHas());
  switch (state()) {
    case NO_FEEDBACK:
      UNREACHABLE();
    case UNINITIALIZED:
      UpdateMonomorphicIC(handler, name);
      break;
    case RECOMPUTE_HANDLER:
    case MONOMORPHIC:
      if (IsGlobalIC()) {
        UpdateMonomorphicIC(handler, name);
        break;
      }
      V8_FALLTHROUGH;
    case POLYMORPHIC:
      if (UpdatePolymorphicIC(name, handler)) break;
      if (UpdateMegaDOMIC(handler, name)) break;
      if (!is_keyed() || state() == RECOMPUTE_HANDLER) {
        CopyICToMegamorphicCache(name);
      }
      V8_FALLTHROUGH;
    case MEGADOM:
      ConfigureVectorState(MEGAMORPHIC, name);
      V8_FALLTHROUGH;
    case MEGAMORPHIC:
      UpdateMegamorphicCache(lookup_start_object_map(), name, handler);
      // Indicate that we've handled this case.
      vector_set_ = true;
      break;
    case GENERIC:
      UNREACHABLE();
  }
}

void LoadIC::UpdateCaches(LookupIterator* lookup) {
  Handle<Object> handler;
  if (lookup->state() == LookupIterator::ACCESS_CHECK) {
    handler = LoadHandler::LoadSlow(isolate());
  } else if (!lookup->IsFound()) {
    TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonexistentDH);
    Handle<Smi> smi_handler = LoadHandler::LoadNonExistent(isolate());
    handler = LoadHandler::LoadFullChain(
        isolate(), lookup_start_object_map(),
        MaybeObjectHandle(isolate()->factory()->null_value()), smi_handler);
  } else if (IsLoadGlobalIC() && lookup->state() == LookupIterator::JSPROXY) {
    // If there is proxy just install the slow stub since we need to call the
    // HasProperty trap for global loads. The ProxyGetProperty builtin doesn't
    // handle this case.
    handler = LoadHandler::LoadSlow(isolate());
  } else {
    if (IsLoadGlobalIC()) {
      if (lookup->TryLookupCachedProperty()) {
        DCHECK_EQ(LookupIterator::DATA, lookup->state());
      }
      if (lookup->state() == LookupIterator::DATA &&
          lookup->GetReceiver().is_identical_to(lookup->GetHolder<Object>())) {
        DCHECK(lookup->GetReceiver()->IsJSGlobalObject());
        // Now update the cell in the feedback vector.
        nexus()->ConfigurePropertyCellMode(lookup->GetPropertyCell());
        TraceIC("LoadGlobalIC", lookup->name());
        return;
      }
    }
    handler = ComputeHandler(lookup);
  }
  // Can't use {lookup->name()} because the LookupIterator might be in
  // "elements" mode for keys that are strings representing integers above
  // JSArray::kMaxIndex.
  SetCache(lookup->GetName(), handler);
  TraceIC("LoadIC", lookup->GetName());
}

StubCache* IC::stub_cache() {
  // HasICs and each of the store own ICs require its own stub cache.
  // Until we create them, don't allow accessing the load/store stub caches.
  DCHECK(!IsAnyHas());
  DCHECK(!IsAnyDefineOwn());
  if (IsAnyLoad()) {
    return isolate()->load_stub_cache();
  } else {
    DCHECK(IsAnyStore());
    return isolate()->store_stub_cache();
  }
}

void IC::UpdateMegamorphicCache(Handle<Map> map, Handle<Name> name,
                                const MaybeObjectHandle& handler) {
  if (!IsAnyHas() && !IsAnyDefineOwn()) {
    stub_cache()->Set(*name, *map, *handler);
  }
}

namespace {

#if V8_ENABLE_WEBASSEMBLY

inline WasmValueType GetWasmValueType(wasm::ValueType type) {
#define TYPE_CASE(Name) \
  case wasm::k##Name:   \
    return WasmValueType::k##Name;

  switch (type.kind()) {
    TYPE_CASE(I8)
    TYPE_CASE(I16)
    TYPE_CASE(I32)
    TYPE_CASE(I64)
    TYPE_CASE(F32)
    TYPE_CASE(F64)
    TYPE_CASE(S128)
    TYPE_CASE(Ref)
    TYPE_CASE(OptRef)

    case wasm::kRtt:
      // Rtt values are not supposed to be made available to JavaScript side.
      UNREACHABLE();

    case wasm::kVoid:
    case wasm::kBottom:
      UNREACHABLE();
  }
#undef TYPE_CASE
}

Handle<Smi> MakeLoadWasmStructFieldHandler(Isolate* isolate,
                                           Handle<JSReceiver> holder,
                                           LookupIterator* lookup) {
  DCHECK(holder->IsWasmObject(isolate));
  WasmValueType type;
  int field_offset;
  if (holder->IsWasmArray(isolate)) {
    // The only named property that WasmArray has is length.
    DCHECK_EQ(0, lookup->property_details().field_index());
    DCHECK_EQ(*isolate->factory()->length_string(), *lookup->name());
    type = WasmValueType::kU32;
    field_offset = WasmArray::kLengthOffset;
  } else {
    wasm::StructType* struct_type = Handle<WasmStruct>::cast(holder)->type();
    int field_index = lookup->property_details().field_index();
    type = GetWasmValueType(struct_type->field(field_index));
    field_offset =
        WasmStruct::kHeaderSize + struct_type->field_offset(field_index);

    const size_t kMaxWasmFieldOffset =
        WasmStruct::kHeaderSize + wasm::StructType::kMaxFieldOffset;
    static_assert(kMaxWasmFieldOffset <= LoadHandler::WasmFieldOffsetBits::kMax,
                  "Bigger numbers of struct fields require different approach");
  }
  return LoadHandler::LoadWasmStructField(isolate, type, field_offset);
}

#endif  // V8_ENABLE_WEBASSEMBLY

}  // namespace

Handle<Object> IC::CodeHandler(Builtin builtin) {
  return MakeCodeHandler(isolate(), builtin);
}

Handle<Object> LoadIC::ComputeHandler(LookupIterator* lookup) {
  Handle<Object> receiver = lookup->GetReceiver();
  ReadOnlyRoots roots(isolate());

  Handle<Object> lookup_start_object = lookup->lookup_start_object();
  // `in` cannot be called on strings, and will always return true for string
  // wrapper length and function prototypes. The latter two cases are given
  // LoadHandler::LoadNativeDataProperty below.
  if (!IsAnyHas() && !lookup->IsElement()) {
    if (lookup_start_object->IsString() &&
        *lookup->name() == roots.length_string()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_StringLength);
      return CodeHandler(Builtin::kLoadIC_StringLength);
    }

    if (lookup_start_object->IsStringWrapper() &&
        *lookup->name() == roots.length_string()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_StringWrapperLength);
      return CodeHandler(Builtin::kLoadIC_StringWrapperLength);
    }

    // Use specialized code for getting prototype of functions.
    if (lookup_start_object->IsJSFunction() &&
        *lookup->name() == roots.prototype_string() &&
        !JSFunction::cast(*lookup_start_object)
             .PrototypeRequiresRuntimeLookup()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_FunctionPrototypeStub);
      return CodeHandler(Builtin::kLoadIC_FunctionPrototype);
    }
  }

  Handle<Map> map = lookup_start_object_map();
  bool holder_is_lookup_start_object =
      lookup_start_object.is_identical_to(lookup->GetHolder<JSReceiver>());

  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR: {
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      Handle<Smi> smi_handler = LoadHandler::LoadInterceptor(isolate());

      if (holder->GetNamedInterceptor().non_masking()) {
        MaybeObjectHandle holder_ref(isolate()->factory()->null_value());
        if (!holder_is_lookup_start_object || IsLoadGlobalIC()) {
          holder_ref = MaybeObjectHandle::Weak(holder);
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonMaskingInterceptorDH);
        return LoadHandler::LoadFullChain(isolate(), map, holder_ref,
                                          smi_handler);
      }

      if (holder_is_lookup_start_object) {
        DCHECK(map->has_named_interceptor());
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadInterceptorDH);
        return smi_handler;
      }

      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadInterceptorFromPrototypeDH);
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }

    case LookupIterator::ACCESSOR: {
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      // Use simple field loads for some well-known callback properties.
      // The method will only return true for absolute truths based on the
      // lookup start object maps.
      FieldIndex field_index;
      if (Accessors::IsJSObjectFieldAccessor(isolate(), map, lookup->name(),
                                             &field_index)) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);
        return LoadHandler::LoadField(isolate(), field_index);
      }
      if (holder->IsJSModuleNamespace()) {
        Handle<ObjectHashTable> exports(
            Handle<JSModuleNamespace>::cast(holder)->module().exports(),
            isolate());
        InternalIndex entry =
            exports->FindEntry(isolate(), roots, lookup->name(),
                               Smi::ToInt(lookup->name()->GetHash()));
        // We found the accessor, so the entry must exist.
        DCHECK(entry.is_found());
        int value_index = ObjectHashTable::EntryToValueIndex(entry);
        Handle<Smi> smi_handler =
            LoadHandler::LoadModuleExport(isolate(), value_index);
        if (holder_is_lookup_start_object) {
          return smi_handler;
        }
        return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                              smi_handler);
      }

      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorPair()) {
        Handle<AccessorPair> accessor_pair =
            Handle<AccessorPair>::cast(accessors);
        if (lookup->TryLookupCachedProperty(accessor_pair)) {
          DCHECK_EQ(LookupIterator::DATA, lookup->state());
          return ComputeHandler(lookup);
        }

        Handle<Object> getter(accessor_pair->getter(), isolate());
        if (!getter->IsJSFunction() && !getter->IsFunctionTemplateInfo()) {
          // TODO(jgruber): Update counter name.
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return LoadHandler::LoadSlow(isolate());
        }
        set_accessor(getter);

        if ((getter->IsFunctionTemplateInfo() &&
             FunctionTemplateInfo::cast(*getter).BreakAtEntry()) ||
            (getter->IsJSFunction() &&
             JSFunction::cast(*getter).shared().BreakAtEntry())) {
          // Do not install an IC if the api function has a breakpoint.
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return LoadHandler::LoadSlow(isolate());
        }

        Handle<Smi> smi_handler;

        CallOptimization call_optimization(isolate(), getter);
        if (call_optimization.is_simple_api_call()) {
          CallOptimization::HolderLookup holder_lookup;
          Handle<JSObject> api_holder =
              call_optimization.LookupHolderOfExpectedType(isolate(), map,
                                                           &holder_lookup);

          if (!call_optimization.IsCompatibleReceiverMap(api_holder, holder,
                                                         holder_lookup) ||
              !holder->HasFastProperties()) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return LoadHandler::LoadSlow(isolate());
          }

          smi_handler = LoadHandler::LoadApiGetter(
              isolate(), holder_lookup == CallOptimization::kHolderIsReceiver);

          Handle<Context> context(
              call_optimization.GetAccessorContext(holder->map()), isolate());

          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadApiGetterFromPrototypeDH);
          return LoadHandler::LoadFromPrototype(
              isolate(), map, holder, smi_handler,
              MaybeObjectHandle::Weak(call_optimization.api_call_info()),
              MaybeObjectHandle::Weak(context));
        }

        if (holder->HasFastProperties()) {
          smi_handler =
              LoadHandler::LoadAccessor(isolate(), lookup->GetAccessorIndex());

          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadAccessorDH);
          if (holder_is_lookup_start_object) return smi_handler;
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadAccessorFromPrototypeDH);
        } else if (holder->IsJSGlobalObject()) {
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadGlobalFromPrototypeDH);
          smi_handler = LoadHandler::LoadGlobal(isolate());
          return LoadHandler::LoadFromPrototype(
              isolate(), map, holder, smi_handler,
              MaybeObjectHandle::Weak(lookup->GetPropertyCell()));
        } else {
          smi_handler = LoadHandler::LoadNormal(isolate());
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalDH);
          if (holder_is_lookup_start_object) return smi_handler;
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalFromPrototypeDH);
        }

        return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                              smi_handler);
      }

      Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);

      if (info->replace_on_access()) {
        set_slow_stub_reason(
            "getter needs to be reconfigured to data property");
        TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
        return LoadHandler::LoadSlow(isolate());
      }

      if (v8::ToCData<Address>(info->getter()) == kNullAddress ||
          !AccessorInfo::IsCompatibleReceiverMap(info, map) ||
          !holder->HasFastProperties() ||
          (info->is_sloppy() && !receiver->IsJSReceiver())) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
        return LoadHandler::LoadSlow(isolate());
      }

      Handle<Smi> smi_handler = LoadHandler::LoadNativeDataProperty(
          isolate(), lookup->GetAccessorIndex());
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNativeDataPropertyDH);
      if (holder_is_lookup_start_object) return smi_handler;
      TRACE_HANDLER_STATS(isolate(),
                          LoadIC_LoadNativeDataPropertyFromPrototypeDH);
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }

    case LookupIterator::DATA: {
      Handle<JSReceiver> holder = lookup->GetHolder<JSReceiver>();
      DCHECK_EQ(PropertyKind::kData, lookup->property_details().kind());
      Handle<Smi> smi_handler;
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject(isolate())) {
          // TODO(verwaest): Also supporting the global object as receiver is a
          // workaround for code that leaks the global object.
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadGlobalDH);
          smi_handler = LoadHandler::LoadGlobal(isolate());
          return LoadHandler::LoadFromPrototype(
              isolate(), map, holder, smi_handler,
              MaybeObjectHandle::Weak(lookup->GetPropertyCell()));
        }
        smi_handler = LoadHandler::LoadNormal(isolate());
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalDH);
        if (holder_is_lookup_start_object) return smi_handler;
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalFromPrototypeDH);
      } else if (lookup->IsElement(*holder)) {
#if V8_ENABLE_WEBASSEMBLY
        if (holder_is_lookup_start_object && holder->IsWasmStruct()) {
          // TODO(ishell): Consider supporting indexed access to WasmStruct
          // fields.
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonexistentDH);
          return LoadHandler::LoadNonExistent(isolate());
        }
#endif  // V8_ENABLE_WEBASSEMBLY
        TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
        return LoadHandler::LoadSlow(isolate());
      } else {
        DCHECK_EQ(PropertyLocation::kField,
                  lookup->property_details().location());
#if V8_ENABLE_WEBASSEMBLY
        if (V8_UNLIKELY(holder->IsWasmObject(isolate()))) {
          smi_handler =
              MakeLoadWasmStructFieldHandler(isolate(), holder, lookup);
        } else  // NOLINT(readability/braces)
#endif          // V8_ENABLE_WEBASSEMBLY
        {
          DCHECK(holder->IsJSObject(isolate()));
          FieldIndex field = lookup->GetFieldIndex();
          smi_handler = LoadHandler::LoadField(isolate(), field);
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);
        if (holder_is_lookup_start_object) return smi_handler;
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldFromPrototypeDH);
      }
      if (lookup->constness() == PropertyConstness::kConst &&
          !holder_is_lookup_start_object) {
        DCHECK_IMPLIES(!V8_DICT_PROPERTY_CONST_TRACKING_BOOL,
                       !lookup->is_dictionary_holder());

        Handle<Object> value = lookup->GetDataValue();

        if (value->IsThinString()) {
          value = handle(ThinString::cast(*value).actual(), isolate());
        }

        // Non internalized strings could turn into thin/cons strings
        // when internalized. Weak references to thin/cons strings are
        // not supported in the GC. If concurrent marking is running
        // and the thin/cons string is marked but the actual string is
        // not, then the weak reference could be missed.
        if (!value->IsString() ||
            (value->IsString() && value->IsInternalizedString())) {
          MaybeObjectHandle weak_value =
              value->IsSmi() ? MaybeObjectHandle(*value, isolate())
                             : MaybeObjectHandle::Weak(*value, isolate());

          smi_handler = LoadHandler::LoadConstantFromPrototype(isolate());
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadConstantFromPrototypeDH);
          return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                                smi_handler, weak_value);
        }
      }
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadIntegerIndexedExoticDH);
      return LoadHandler::LoadNonExistent(isolate());

    case LookupIterator::JSPROXY: {
      Handle<Smi> smi_handler = LoadHandler::LoadProxy(isolate());
      if (holder_is_lookup_start_object) return smi_handler;

      Handle<JSProxy> holder_proxy = lookup->GetHolder<JSProxy>();
      return LoadHandler::LoadFromPrototype(isolate(), map, holder_proxy,
                                            smi_handler);
    }
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }

  return Handle<Code>::null();
}

bool KeyedLoadIC::CanChangeToAllowOutOfBounds(Handle<Map> receiver_map) {
  const MaybeObjectHandle& handler = nexus()->FindHandlerForMap(receiver_map);
  if (handler.is_null()) return false;
  return LoadHandler::GetKeyedAccessLoadMode(*handler) == STANDARD_LOAD;
}

void KeyedLoadIC::UpdateLoadElement(Handle<HeapObject> receiver,
                                    KeyedAccessLoadMode load_mode) {
  Handle<Map> receiver_map(receiver->map(), isolate());
  DCHECK(receiver_map->instance_type() !=
         JS_PRIMITIVE_WRAPPER_TYPE);  // Checked by caller.
  MapHandles target_receiver_maps;
  TargetMaps(&target_receiver_maps);

  if (target_receiver_maps.empty()) {
    Handle<Object> handler = LoadElementHandler(receiver_map, load_mode);
    return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
  }

  for (Handle<Map> map : target_receiver_maps) {
    if (map.is_null()) continue;
    if (map->instance_type() == JS_PRIMITIVE_WRAPPER_TYPE) {
      set_slow_stub_reason("JSPrimitiveWrapper");
      return;
    }
    if (map->instance_type() == JS_PROXY_TYPE) {
      set_slow_stub_reason("JSProxy");
      return;
    }
  }

  // The first time a receiver is seen that is a transitioned version of the
  // previous monomorphic receiver type, assume the new ElementsKind is the
  // monomorphic type. This benefits global arrays that only transition
  // once, and all call sites accessing them are faster if they remain
  // monomorphic. If this optimistic assumption is not true, the IC will
  // miss again and it will become polymorphic and support both the
  // untransitioned and transitioned maps.
  if (state() == MONOMORPHIC) {
    if ((receiver->IsJSObject() &&
         IsMoreGeneralElementsKindTransition(
             target_receiver_maps.at(0)->elements_kind(),
             Handle<JSObject>::cast(receiver)->GetElementsKind()))
#ifdef V8_ENABLE_WEBASSEMBLY
        || receiver->IsWasmObject()
#endif
    ) {
      Handle<Object> handler = LoadElementHandler(receiver_map, load_mode);
      return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
    }
  }

  DCHECK(state() != GENERIC);

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map)) {
    // If the {receiver_map} previously had a handler that didn't handle
    // out-of-bounds access, but can generally handle it, we can just go
    // on and update the handler appropriately below.
    if (load_mode != LOAD_IGNORE_OUT_OF_BOUNDS ||
        !CanChangeToAllowOutOfBounds(receiver_map)) {
      // If the miss wasn't due to an unseen map, a polymorphic stub
      // won't help, use the generic stub.
      set_slow_stub_reason("same map added twice");
      return;
    }
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (static_cast<int>(target_receiver_maps.size()) >
      FLAG_max_valid_polymorphic_map_count) {
    set_slow_stub_reason("max polymorph exceeded");
    return;
  }

  MaybeObjectHandles handlers;
  handlers.reserve(target_receiver_maps.size());
  LoadElementPolymorphicHandlers(&target_receiver_maps, &handlers, load_mode);
  DCHECK_LE(1, target_receiver_maps.size());
  if (target_receiver_maps.size() == 1) {
    ConfigureVectorState(Handle<Name>(), target_receiver_maps[0], handlers[0]);
  } else {
    ConfigureVectorState(Handle<Name>(), target_receiver_maps, &handlers);
  }
}

namespace {

bool AllowConvertHoleElementToUndefined(Isolate* isolate,
                                        Handle<Map> receiver_map) {
  if (receiver_map->IsJSTypedArrayMap()) {
    // For JSTypedArray we never lookup elements in the prototype chain.
    return true;
  }

  // For other {receiver}s we need to check the "no elements" protector.
  if (Protectors::IsNoElementsIntact(isolate)) {
    if (receiver_map->IsStringMap()) {
      return true;
    }
    if (receiver_map->IsJSObjectMap()) {
      // For other JSObjects (including JSArrays) we can only continue if
      // the {receiver}s prototype is either the initial Object.prototype
      // or the initial Array.prototype, which are both guarded by the
      // "no elements" protector checked above.
      Handle<Object> receiver_prototype(receiver_map->prototype(), isolate);

      if (isolate->IsInAnyContext(*receiver_prototype,
                                  Context::INITIAL_ARRAY_PROTOTYPE_INDEX) ||
          isolate->IsInAnyContext(*receiver_prototype,
                                  Context::INITIAL_OBJECT_PROTOTYPE_INDEX)) {
        return true;
      }
    }
  }

  return false;
}
}  // namespace

Handle<Object> KeyedLoadIC::LoadElementHandler(Handle<Map> receiver_map,
                                               KeyedAccessLoadMode load_mode) {
  // Has a getter interceptor, or is any has and has a query interceptor.
  if (receiver_map->has_indexed_interceptor() &&
      (!receiver_map->GetIndexedInterceptor().getter().IsUndefined(isolate()) ||
       (IsAnyHas() &&
        !receiver_map->GetIndexedInterceptor().query().IsUndefined(
            isolate()))) &&
      !receiver_map->GetIndexedInterceptor().non_masking()) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadIndexedInterceptorStub);
    return IsAnyHas() ? CodeHandler(Builtin::kHasIndexedInterceptorIC)
                      : CodeHandler(Builtin::kLoadIndexedInterceptorIC);
  }

  InstanceType instance_type = receiver_map->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadIndexedStringDH);
    if (IsAnyHas()) return LoadHandler::LoadSlow(isolate());
    return LoadHandler::LoadIndexedString(isolate(), load_mode);
  }
  if (instance_type < FIRST_JS_RECEIVER_TYPE) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_SlowStub);
    return LoadHandler::LoadSlow(isolate());
  }
  if (instance_type == JS_PROXY_TYPE) {
    return LoadHandler::LoadProxy(isolate());
  }
#if V8_ENABLE_WEBASSEMBLY
  if (InstanceTypeChecker::IsWasmObject(instance_type)) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_SlowStub);
    return LoadHandler::LoadSlow(isolate());
  }
#endif  // V8_ENABLE_WEBASSEMBLY

  ElementsKind elements_kind = receiver_map->elements_kind();
  if (IsSloppyArgumentsElementsKind(elements_kind)) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_KeyedLoadSloppyArgumentsStub);
    return IsAnyHas() ? CodeHandler(Builtin::kKeyedHasIC_SloppyArguments)
                      : CodeHandler(Builtin::kKeyedLoadIC_SloppyArguments);
  }
  bool is_js_array = instance_type == JS_ARRAY_TYPE;
  if (elements_kind == DICTIONARY_ELEMENTS) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadElementDH);
    return LoadHandler::LoadElement(isolate(), elements_kind, false,
                                    is_js_array, load_mode);
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsAnyNonextensibleElementsKind(elements_kind) ||
         IsTypedArrayOrRabGsabTypedArrayElementsKind(elements_kind));
  bool convert_hole_to_undefined =
      (elements_kind == HOLEY_SMI_ELEMENTS ||
       elements_kind == HOLEY_ELEMENTS) &&
      AllowConvertHoleElementToUndefined(isolate(), receiver_map);
  TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadElementDH);
  return LoadHandler::LoadElement(isolate(), elements_kind,
                                  convert_hole_to_undefined, is_js_array,
                                  load_mode);
}

void KeyedLoadIC::LoadElementPolymorphicHandlers(
    MapHandles* receiver_maps, MaybeObjectHandles* handlers,
    KeyedAccessLoadMode load_mode) {
  // Filter out deprecated maps to ensure their instances get migrated.
  receiver_maps->erase(
      std::remove_if(
          receiver_maps->begin(), receiver_maps->end(),
          [](const Handle<Map>& map) { return map->is_deprecated(); }),
      receiver_maps->end());

  for (Handle<Map> receiver_map : *receiver_maps) {
    // Mark all stable receiver maps that have elements kind transition map
    // among receiver_maps as unstable because the optimizing compilers may
    // generate an elements kind transition for this kind of receivers.
    if (receiver_map->is_stable()) {
      Map tmap = receiver_map->FindElementsKindTransitionedMap(
          isolate(), *receiver_maps, ConcurrencyMode::kSynchronous);
      if (!tmap.is_null()) {
        receiver_map->NotifyLeafMapLayoutChange(isolate());
      }
    }
    handlers->push_back(
        MaybeObjectHandle(LoadElementHandler(receiver_map, load_mode)));
  }
}

namespace {

enum KeyType { kIntPtr, kName, kBailout };

// The cases where kIntPtr is returned must match what
// CodeStubAssembler::TryToIntptr can handle!
KeyType TryConvertKey(Handle<Object> key, Isolate* isolate, intptr_t* index_out,
                      Handle<Name>* name_out) {
  if (key->IsSmi()) {
    *index_out = Smi::ToInt(*key);
    return kIntPtr;
  }
  if (key->IsHeapNumber()) {
    double num = HeapNumber::cast(*key).value();
    if (!(num >= -kMaxSafeInteger)) return kBailout;
    if (num > kMaxSafeInteger) return kBailout;
    *index_out = static_cast<intptr_t>(num);
    if (*index_out != num) return kBailout;
    return kIntPtr;
  }
  if (key->IsString()) {
    key = isolate->factory()->InternalizeString(Handle<String>::cast(key));
    uint32_t maybe_array_index;
    if (String::cast(*key).AsArrayIndex(&maybe_array_index)) {
      if (maybe_array_index <= INT_MAX) {
        *index_out = static_cast<intptr_t>(maybe_array_index);
        return kIntPtr;
      }
      // {key} is a string representation of an array index beyond the range
      // that the IC could handle. Don't try to take the named-property path.
      return kBailout;
    }
    *name_out = Handle<String>::cast(key);
    return kName;
  }
  if (key->IsSymbol()) {
    *name_out = Handle<Symbol>::cast(key);
    return kName;
  }
  return kBailout;
}

bool IntPtrKeyToSize(intptr_t index, Handle<HeapObject> receiver, size_t* out) {
  if (index < 0) {
    if (receiver->IsJSTypedArray()) {
      // For JSTypedArray receivers, we can support negative keys, which we
      // just map to a very large value. This is valid because all OOB accesses
      // (negative or positive) are handled the same way, and size_t::max is
      // guaranteed to be an OOB access.
      *out = std::numeric_limits<size_t>::max();
      return true;
    }
    return false;
  }
#if V8_HOST_ARCH_64_BIT
  if (index > JSObject::kMaxElementIndex && !receiver->IsJSTypedArray()) {
    return false;
  }
#else
  // On 32-bit platforms, any intptr_t is less than kMaxElementIndex.
  STATIC_ASSERT(
      static_cast<double>(std::numeric_limits<decltype(index)>::max()) <=
      static_cast<double>(JSObject::kMaxElementIndex));
#endif
  *out = static_cast<size_t>(index);
  return true;
}

bool CanCache(Handle<Object> receiver, InlineCacheState state) {
  if (!FLAG_use_ic || state == NO_FEEDBACK) return false;
  if (!receiver->IsJSReceiver() && !receiver->IsString()) return false;
  return !receiver->IsAccessCheckNeeded() && !receiver->IsJSPrimitiveWrapper();
}

bool IsOutOfBoundsAccess(Handle<Object> receiver, size_t index) {
  size_t length;
  if (receiver->IsJSArray()) {
    length = JSArray::cast(*receiver).length().Number();
  } else if (receiver->IsJSTypedArray()) {
    length = JSTypedArray::cast(*receiver).GetLength();
  } else if (receiver->IsJSObject()) {
    length = JSObject::cast(*receiver).elements().length();
  } else if (receiver->IsString()) {
    length = String::cast(*receiver).length();
  } else {
    return false;
  }
  return index >= length;
}

KeyedAccessLoadMode GetLoadMode(Isolate* isolate, Handle<Object> receiver,
                                size_t index) {
  if (IsOutOfBoundsAccess(receiver, index)) {
    DCHECK(receiver->IsHeapObject());
    Handle<Map> receiver_map(Handle<HeapObject>::cast(receiver)->map(),
                             isolate);
    if (AllowConvertHoleElementToUndefined(isolate, receiver_map)) {
      return LOAD_IGNORE_OUT_OF_BOUNDS;
    }
  }
  return STANDARD_LOAD;
}

}  // namespace

MaybeHandle<Object> KeyedLoadIC::RuntimeLoad(Handle<Object> object,
                                             Handle<Object> key) {
  Handle<Object> result;

  if (IsKeyedLoadIC()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Runtime::GetObjectProperty(isolate(), object, key),
        Object);
  } else {
    DCHECK(IsKeyedHasIC());
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                               Runtime::HasProperty(isolate(), object, key),
                               Object);
  }
  return result;
}

MaybeHandle<Object> KeyedLoadIC::Load(Handle<Object> object,
                                      Handle<Object> key) {
  if (MigrateDeprecated(isolate(), object)) {
    return RuntimeLoad(object, key);
  }

  Handle<Object> load_handle;

  intptr_t maybe_index;
  size_t index;
  Handle<Name> maybe_name;
  KeyType key_type = TryConvertKey(key, isolate(), &maybe_index, &maybe_name);

  if (key_type == kName) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), load_handle,
                               LoadIC::Load(object, maybe_name), Object);
  } else if (key_type == kIntPtr && CanCache(object, state()) &&
             IntPtrKeyToSize(maybe_index, Handle<HeapObject>::cast(object),
                             &index)) {
    KeyedAccessLoadMode load_mode = GetLoadMode(isolate(), object, index);
    UpdateLoadElement(Handle<HeapObject>::cast(object), load_mode);
    if (is_vector_set()) {
      TraceIC("LoadIC", key);
    }
  }

  if (vector_needs_update()) {
    ConfigureVectorState(MEGAMORPHIC, key);
    TraceIC("LoadIC", key);
  }

  if (!load_handle.is_null()) return load_handle;

  return RuntimeLoad(object, key);
}

bool StoreIC::LookupForWrite(LookupIterator* it, Handle<Object> value,
                             StoreOrigin store_origin) {
  // Disable ICs for non-JSObjects for now.
  Handle<Object> object = it->GetReceiver();
  if (object->IsJSProxy()) return true;
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  DCHECK(!receiver->map().is_deprecated());

  if (it->state() != LookupIterator::TRANSITION) {
    for (; it->IsFound(); it->Next()) {
      switch (it->state()) {
        case LookupIterator::NOT_FOUND:
        case LookupIterator::TRANSITION:
          UNREACHABLE();
        case LookupIterator::JSPROXY:
          return true;
        case LookupIterator::INTERCEPTOR: {
          Handle<JSObject> holder = it->GetHolder<JSObject>();
          InterceptorInfo info = holder->GetNamedInterceptor();
          if (it->HolderIsReceiverOrHiddenPrototype() ||
              !info.getter().IsUndefined(isolate()) ||
              !info.query().IsUndefined(isolate())) {
            return true;
          }
          break;
        }
        case LookupIterator::ACCESS_CHECK:
          if (it->GetHolder<JSObject>()->IsAccessCheckNeeded()) return false;
          break;
        case LookupIterator::ACCESSOR:
          return !it->IsReadOnly();
        case LookupIterator::INTEGER_INDEXED_EXOTIC:
          return false;
        case LookupIterator::DATA: {
          if (it->IsReadOnly()) return false;
          Handle<JSObject> holder = it->GetHolder<JSObject>();
          if (receiver.is_identical_to(holder)) {
            it->PrepareForDataProperty(value);
            // The previous receiver map might just have been deprecated,
            // so reload it.
            update_lookup_start_object_map(receiver);
            return true;
          }

          // Receiver != holder.
          if (receiver->IsJSGlobalProxy()) {
            PrototypeIterator iter(isolate(), receiver);
            return it->GetHolder<Object>().is_identical_to(
                PrototypeIterator::GetCurrent(iter));
          }

          if (it->HolderIsReceiverOrHiddenPrototype()) return false;

          if (it->ExtendingNonExtensible(receiver)) return false;
          it->PrepareTransitionToDataProperty(receiver, value, NONE,
                                              store_origin);
          return it->IsCacheableTransition();
        }
      }
    }
  }

  // If we are in StoreGlobal then check if we should throw on non-existent
  // properties.
  if (IsStoreGlobalIC() &&
      (GetShouldThrow(it->isolate(), Nothing<ShouldThrow>()) ==
       ShouldThrow::kThrowOnError)) {
    // ICs typically does the store in two steps: prepare receiver for the
    // transition followed by the actual store. For global objects we create a
    // property cell when preparing for transition and install this cell in the
    // handler. In strict mode, we throw and never initialize this property
    // cell. The IC handler assumes that the property cell it is holding is for
    // a property that is existing. This case violates this assumption. If we
    // happen to invalidate this property cell later, it leads to incorrect
    // behaviour. For now just use a slow stub and don't install the property
    // cell for these cases. Hopefully these cases are not frequent enough to
    // impact performance.
    //
    // TODO(mythria): If we find this to be happening often, we could install a
    // new kind of handler for non-existent properties. These handlers can then
    // miss to runtime if the value is not hole (i.e. cell got invalidated) and
    // handle these stores correctly.
    return false;
  }
  receiver = it->GetStoreTarget<JSObject>();
  if (it->ExtendingNonExtensible(receiver)) return false;
  it->PrepareTransitionToDataProperty(receiver, value, NONE, store_origin);
  return it->IsCacheableTransition();
}

MaybeHandle<Object> StoreGlobalIC::Store(Handle<Name> name,
                                         Handle<Object> value) {
  DCHECK(name->IsString());

  // Look up in script context table.
  Handle<String> str_name = Handle<String>::cast(name);
  Handle<JSGlobalObject> global = isolate()->global_object();
  Handle<ScriptContextTable> script_contexts(
      global->native_context().script_context_table(), isolate());

  VariableLookupResult lookup_result;
  if (script_contexts->Lookup(str_name, &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        isolate(), script_contexts, lookup_result.context_index);
    if (lookup_result.mode == VariableMode::kConst) {
      return TypeError(MessageTemplate::kConstAssign, global, name);
    }

    Handle<Object> previous_value(script_context->get(lookup_result.slot_index),
                                  isolate());

    if (previous_value->IsTheHole(isolate())) {
      // Do not install stubs and stay pre-monomorphic for
      // uninitialized accesses.
      THROW_NEW_ERROR(
          isolate(),
          NewReferenceError(MessageTemplate::kAccessedUninitializedVariable,
                            name),
          Object);
    }

    bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic;
    if (use_ic) {
      if (nexus()->ConfigureLexicalVarMode(
              lookup_result.context_index, lookup_result.slot_index,
              lookup_result.mode == VariableMode::kConst)) {
        TRACE_HANDLER_STATS(isolate(), StoreGlobalIC_StoreScriptContextField);
      } else {
        // Given combination of indices can't be encoded, so use slow stub.
        TRACE_HANDLER_STATS(isolate(), StoreGlobalIC_SlowStub);
        SetCache(name, StoreHandler::StoreSlow(isolate()));
      }
      TraceIC("StoreGlobalIC", name);
    } else if (state() == NO_FEEDBACK) {
      TraceIC("StoreGlobalIC", name);
    }
    script_context->set(lookup_result.slot_index, *value);
    return value;
  }

  return StoreIC::Store(global, name, value);
}

namespace {
Maybe<bool> DefineOwnDataProperty(LookupIterator* it,
                                  LookupIterator::State original_state,
                                  Handle<Object> value,
                                  Maybe<ShouldThrow> should_throw,
                                  StoreOrigin store_origin) {
  // It should not be possible to call DefineOwnDataProperty in a
  // contextual store (indicated by IsJSGlobalObject()).
  DCHECK(!it->GetReceiver()->IsJSGlobalObject(it->isolate()));

  // Handle special cases that can't be handled by
  // DefineOwnPropertyIgnoreAttributes first.
  switch (it->state()) {
    case LookupIterator::JSPROXY: {
      PropertyDescriptor new_desc;
      new_desc.set_value(value);
      new_desc.set_writable(true);
      new_desc.set_enumerable(true);
      new_desc.set_configurable(true);
      DCHECK_EQ(original_state, LookupIterator::JSPROXY);
      // TODO(joyee): this will start the lookup again. Ideally we should
      // implement something that reuses the existing LookupIterator.
      return JSProxy::DefineOwnProperty(it->isolate(), it->GetHolder<JSProxy>(),
                                        it->GetName(), &new_desc, should_throw);
    }
    // When lazy feedback is disabled, the original state could be different
    // while the object is already prepared for TRANSITION.
    case LookupIterator::TRANSITION: {
      switch (original_state) {
        case LookupIterator::JSPROXY:
        case LookupIterator::TRANSITION:
        case LookupIterator::DATA:
        case LookupIterator::INTERCEPTOR:
        case LookupIterator::ACCESSOR:
        case LookupIterator::INTEGER_INDEXED_EXOTIC:
          UNREACHABLE();
        case LookupIterator::ACCESS_CHECK: {
          DCHECK(!it->GetHolder<JSObject>()->IsAccessCheckNeeded());
          V8_FALLTHROUGH;
        }
        case LookupIterator::NOT_FOUND:
          return Object::AddDataProperty(it, value, NONE,
                                         Nothing<ShouldThrow>(), store_origin,
                                         EnforceDefineSemantics::kDefine);
      }
    }
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::DATA:
    case LookupIterator::ACCESSOR:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      break;
  }

  // We need to restart to handle interceptors properly.
  it->Restart();

  return JSObject::DefineOwnPropertyIgnoreAttributes(
      it, value, NONE, should_throw, JSObject::DONT_FORCE_FIELD,
      EnforceDefineSemantics::kDefine, store_origin);
}
}  // namespace

MaybeHandle<Object> StoreIC::Store(Handle<Object> object, Handle<Name> name,
                                   Handle<Object> value,
                                   StoreOrigin store_origin) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(isolate(), object)) {
    // KeyedStoreIC should handle DefineKeyedOwnIC with deprecated maps directly
    // instead of reusing this method.
    DCHECK(!IsDefineKeyedOwnIC());
    DCHECK(!name->IsPrivateName());

    PropertyKey key(isolate(), name);
    LookupIterator it(
        isolate(), object, key,
        IsDefineNamedOwnIC() ? LookupIterator::OWN : LookupIterator::DEFAULT);
    DCHECK_IMPLIES(IsDefineNamedOwnIC(), it.IsFound() && it.HolderIsReceiver());
    // TODO(v8:12548): refactor DefinedNamedOwnIC and SetNamedIC as subclasses
    // of StoreIC so their logic doesn't get mixed here.
    if (IsDefineNamedOwnIC()) {
      MAYBE_RETURN_NULL(
          JSReceiver::CreateDataProperty(&it, value, Nothing<ShouldThrow>()));
    } else {
      MAYBE_RETURN_NULL(Object::SetProperty(&it, value, StoreOrigin::kNamed));
    }
    return value;
  }

  bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic;
  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsNullOrUndefined(isolate())) {
    if (use_ic) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), StoreIC_NonReceiver);
      update_lookup_start_object_map(object);
      SetCache(name, StoreHandler::StoreSlow(isolate()));
      TraceIC("StoreIC", name);
    }
    return TypeError(MessageTemplate::kNonObjectPropertyStoreWithProperty, name,
                     object);
  }

  JSObject::MakePrototypesFast(object, kStartAtPrototype, isolate());
  PropertyKey key(isolate(), name);
  LookupIterator it(
      isolate(), object, key,
      IsAnyDefineOwn() ? LookupIterator::OWN : LookupIterator::DEFAULT);

  if (name->IsPrivate()) {
    bool exists = it.IsFound();
    if (name->IsPrivateName() && exists == IsDefineKeyedOwnIC()) {
      Handle<String> name_string(
          String::cast(Symbol::cast(*name).description()), isolate());
      if (exists) {
        MessageTemplate message =
            name->IsPrivateBrand()
                ? MessageTemplate::kInvalidPrivateBrandReinitialization
                : MessageTemplate::kInvalidPrivateFieldReinitialization;
        return TypeError(message, object, name_string);
      } else {
        return TypeError(MessageTemplate::kInvalidPrivateMemberWrite, object,
                         name_string);
      }
    }

    // IC handling of private fields/symbols stores on JSProxy is not
    // supported.
    if (object->IsJSProxy()) {
      use_ic = false;
    }
  }

  // For IsAnyDefineOwn(), we can't simply do CreateDataProperty below
  // because we need to check the attributes before UpdateCaches updates
  // the state of the LookupIterator.
  LookupIterator::State original_state = it.state();
  // We'll defer the check for JSProxy and objects with named interceptors,
  // because the defineProperty traps need to be called first if they are
  // present. We can also skip this for private names since they are not
  // bound by configurability or extensibility checks, and errors would've
  // been thrown if the private field already exists in the object.
  if (IsAnyDefineOwn() && !name->IsPrivateName() && !object->IsJSProxy() &&
      !Handle<JSObject>::cast(object)->HasNamedInterceptor()) {
    Maybe<bool> can_define = JSReceiver::CheckIfCanDefine(
        isolate(), &it, value, Nothing<ShouldThrow>());
    if (can_define.IsNothing() || !can_define.FromJust()) {
      return MaybeHandle<Object>();
    }
  }

  if (use_ic) {
    UpdateCaches(&it, value, store_origin);
  } else if (state() == NO_FEEDBACK) {
    // Tracing IC Stats for No Feedback State.
    IsStoreGlobalIC() ? TraceIC("StoreGlobalIC", name)
                      : TraceIC("StoreIC", name);
  }

  // TODO(v8:12548): refactor DefinedNamedOwnIC and SetNamedIC as subclasses
  // of StoreIC so their logic doesn't get mixed here.
  // ES #sec-definefield
  // ES #sec-runtime-semantics-propertydefinitionevaluation
  // IsAnyDefineOwn() can be true when this method is reused by KeyedStoreIC.
  if (IsAnyDefineOwn()) {
    if (name->IsPrivateName()) {
      // We should define private fields without triggering traps or checking
      // extensibility.
      MAYBE_RETURN_NULL(
          JSReceiver::AddPrivateField(&it, value, Nothing<ShouldThrow>()));
    } else {
      MAYBE_RETURN_NULL(DefineOwnDataProperty(
          &it, original_state, value, Nothing<ShouldThrow>(), store_origin));
    }
  } else {
    MAYBE_RETURN_NULL(Object::SetProperty(&it, value, store_origin));
  }
  return value;
}

void StoreIC::UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                           StoreOrigin store_origin) {
  MaybeObjectHandle handler;
  if (LookupForWrite(lookup, value, store_origin)) {
    if (IsStoreGlobalIC()) {
      if (lookup->state() == LookupIterator::DATA &&
          lookup->GetReceiver().is_identical_to(lookup->GetHolder<Object>())) {
        DCHECK(lookup->GetReceiver()->IsJSGlobalObject());
        // Now update the cell in the feedback vector.
        nexus()->ConfigurePropertyCellMode(lookup->GetPropertyCell());
        TraceIC("StoreGlobalIC", lookup->GetName());
        return;
      }
    }
    handler = ComputeHandler(lookup);
  } else {
    set_slow_stub_reason("LookupForWrite said 'false'");
    handler = MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
  }
  // Can't use {lookup->name()} because the LookupIterator might be in
  // "elements" mode for keys that are strings representing integers above
  // JSArray::kMaxIndex.
  SetCache(lookup->GetName(), handler);
  TraceIC("StoreIC", lookup->GetName());
}

MaybeObjectHandle StoreIC::ComputeHandler(LookupIterator* lookup) {
  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      Handle<JSObject> store_target = lookup->GetStoreTarget<JSObject>();
      if (store_target->IsJSGlobalObject()) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobalTransitionDH);

        if (lookup_start_object_map()->IsJSGlobalObject()) {
          DCHECK(IsStoreGlobalIC());
#ifdef DEBUG
          Handle<JSObject> holder = lookup->GetHolder<JSObject>();
          DCHECK_EQ(*lookup->GetReceiver(), *holder);
          DCHECK_EQ(*store_target, *holder);
#endif
          return StoreHandler::StoreGlobal(lookup->transition_cell());
        }
        if (IsDefineKeyedOwnIC()) {
          // Private field can't be deleted from this global object and can't
          // be overwritten, so install slow handler in order to make store IC
          // throw if a private name already exists.
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }

        Handle<Smi> smi_handler = StoreHandler::StoreGlobalProxy(isolate());
        Handle<Object> handler = StoreHandler::StoreThroughPrototype(
            isolate(), lookup_start_object_map(), store_target, smi_handler,
            MaybeObjectHandle::Weak(lookup->transition_cell()));
        return MaybeObjectHandle(handler);
      }
      // Dictionary-to-fast transitions are not expected and not supported.
      DCHECK_IMPLIES(!lookup->transition_map()->is_dictionary_map(),
                     !lookup_start_object_map()->is_dictionary_map());

      DCHECK(lookup->IsCacheableTransition());
      if (IsAnyDefineOwn()) {
        return StoreHandler::StoreOwnTransition(isolate(),
                                                lookup->transition_map());
      }
      return StoreHandler::StoreTransition(isolate(), lookup->transition_map());
    }

    case LookupIterator::INTERCEPTOR: {
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      InterceptorInfo info = holder->GetNamedInterceptor();

      // If the interceptor is on the receiver...
      if (lookup->HolderIsReceiverOrHiddenPrototype() && !info.non_masking()) {
        // ...return a store interceptor Smi handler if there is a setter
        // interceptor and it's not DefineNamedOwnIC or DefineKeyedOwnIC
        // (which should call the definer)...
        if (!info.setter().IsUndefined(isolate()) && !IsAnyDefineOwn()) {
          return MaybeObjectHandle(StoreHandler::StoreInterceptor(isolate()));
        }
        // ...otherwise return a slow-case Smi handler, which invokes the
        // definer for DefineNamedOwnIC.
        return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
      }

      // If the interceptor is a getter/query interceptor on the prototype
      // chain, return an invalidatable slow handler so it can turn fast if the
      // interceptor is masked by a regular property later.
      DCHECK(!info.getter().IsUndefined(isolate()) ||
             !info.query().IsUndefined(isolate()));
      Handle<Object> handler = StoreHandler::StoreThroughPrototype(
          isolate(), lookup_start_object_map(), holder,
          StoreHandler::StoreSlow(isolate()));
      return MaybeObjectHandle(handler);
    }

    case LookupIterator::ACCESSOR: {
      // This is currently guaranteed by checks in StoreIC::Store.
      Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

      if (!holder->HasFastProperties()) {
        set_slow_stub_reason("accessor on slow map");
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        MaybeObjectHandle handler =
            MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        return handler;
      }
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->setter()) == kNullAddress) {
          set_slow_stub_reason("setter == kNullAddress");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }
        if (AccessorInfo::cast(*accessors).is_special_data_property() &&
            !lookup->HolderIsReceiverOrHiddenPrototype()) {
          set_slow_stub_reason("special data property in prototype chain");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }
        if (!AccessorInfo::IsCompatibleReceiverMap(info,
                                                   lookup_start_object_map())) {
          set_slow_stub_reason("incompatible receiver type");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }

        Handle<Smi> smi_handler = StoreHandler::StoreNativeDataProperty(
            isolate(), lookup->GetAccessorIndex());
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreNativeDataPropertyDH);
        if (receiver.is_identical_to(holder)) {
          return MaybeObjectHandle(smi_handler);
        }
        TRACE_HANDLER_STATS(isolate(),
                            StoreIC_StoreNativeDataPropertyOnPrototypeDH);
        return MaybeObjectHandle(StoreHandler::StoreThroughPrototype(
            isolate(), lookup_start_object_map(), holder, smi_handler));

      } else if (accessors->IsAccessorPair()) {
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        if (!setter->IsJSFunction() && !setter->IsFunctionTemplateInfo()) {
          set_slow_stub_reason("setter not a function");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }

        if ((setter->IsFunctionTemplateInfo() &&
             FunctionTemplateInfo::cast(*setter).BreakAtEntry()) ||
            (setter->IsJSFunction() &&
             JSFunction::cast(*setter).shared().BreakAtEntry())) {
          // Do not install an IC if the api function has a breakpoint.
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }

        CallOptimization call_optimization(isolate(), setter);
        if (call_optimization.is_simple_api_call()) {
          CallOptimization::HolderLookup holder_lookup;
          Handle<JSObject> api_holder =
              call_optimization.LookupHolderOfExpectedType(
                  isolate(), lookup_start_object_map(), &holder_lookup);
          if (call_optimization.IsCompatibleReceiverMap(api_holder, holder,
                                                        holder_lookup)) {
            Handle<Smi> smi_handler = StoreHandler::StoreApiSetter(
                isolate(),
                holder_lookup == CallOptimization::kHolderIsReceiver);

            Handle<Context> context(
                call_optimization.GetAccessorContext(holder->map()), isolate());
            TRACE_HANDLER_STATS(isolate(), StoreIC_StoreApiSetterOnPrototypeDH);
            return MaybeObjectHandle(StoreHandler::StoreThroughPrototype(
                isolate(), lookup_start_object_map(), holder, smi_handler,
                MaybeObjectHandle::Weak(call_optimization.api_call_info()),
                MaybeObjectHandle::Weak(context)));
          }
          set_slow_stub_reason("incompatible receiver");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        } else if (setter->IsFunctionTemplateInfo()) {
          set_slow_stub_reason("setter non-simple template");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
        }

        Handle<Smi> smi_handler =
            StoreHandler::StoreAccessor(isolate(), lookup->GetAccessorIndex());

        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreAccessorDH);
        if (receiver.is_identical_to(holder)) {
          return MaybeObjectHandle(smi_handler);
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreAccessorOnPrototypeDH);

        return MaybeObjectHandle(StoreHandler::StoreThroughPrototype(
            isolate(), lookup_start_object_map(), holder, smi_handler));
      }
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
    }

    case LookupIterator::DATA: {
      // This is currently guaranteed by checks in StoreIC::Store.
      Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
      USE(receiver);
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

      DCHECK_EQ(PropertyKind::kData, lookup->property_details().kind());
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject()) {
          TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobalDH);
          return MaybeObjectHandle(
              StoreHandler::StoreGlobal(lookup->GetPropertyCell()));
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreNormalDH);
        DCHECK(holder.is_identical_to(receiver));
        DCHECK_IMPLIES(!V8_DICT_PROPERTY_CONST_TRACKING_BOOL,
                       lookup->constness() == PropertyConstness::kMutable);

        Handle<Smi> handler = StoreHandler::StoreNormal(isolate());
        return MaybeObjectHandle(handler);
      }

      // -------------- Elements (for TypedArrays) -------------
      if (lookup->IsElement(*holder)) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
      }

      // -------------- Fields --------------
      if (lookup->property_details().location() == PropertyLocation::kField) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreFieldDH);
        int descriptor = lookup->GetFieldDescriptorIndex();
        FieldIndex index = lookup->GetFieldIndex();
        if (V8_UNLIKELY(holder->IsJSSharedStruct())) {
          return MaybeObjectHandle(StoreHandler::StoreSharedStructField(
              isolate(), descriptor, index, lookup->representation()));
        }
        PropertyConstness constness = lookup->constness();
        if (constness == PropertyConstness::kConst &&
            IsDefineNamedOwnICKind(nexus()->kind())) {
          // DefineNamedOwnICs are used for initializing object literals
          // therefore we must store the value unconditionally even to
          // VariableMode::kConst fields.
          constness = PropertyConstness::kMutable;
        }
        return MaybeObjectHandle(StoreHandler::StoreField(
            isolate(), descriptor, index, constness, lookup->representation()));
      }

      // -------------- Constant properties --------------
      DCHECK_EQ(PropertyLocation::kDescriptor,
                lookup->property_details().location());
      set_slow_stub_reason("constant property");
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
    }
    case LookupIterator::JSPROXY: {
      Handle<JSReceiver> receiver =
          Handle<JSReceiver>::cast(lookup->GetReceiver());
      Handle<JSProxy> holder = lookup->GetHolder<JSProxy>();

      // IsDefineNamedOwnIC() is true when we are defining public fields on a
      // Proxy. In that case use the slow stub to invoke the define trap.
      if (IsDefineNamedOwnIC()) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        return MaybeObjectHandle(StoreHandler::StoreSlow(isolate()));
      }

      return MaybeObjectHandle(StoreHandler::StoreProxy(
          isolate(), lookup_start_object_map(), holder, receiver));
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::NOT_FOUND:
      UNREACHABLE();
  }
  return MaybeObjectHandle();
}

void KeyedStoreIC::UpdateStoreElement(Handle<Map> receiver_map,
                                      KeyedAccessStoreMode store_mode,
                                      Handle<Map> new_receiver_map) {
  std::vector<MapAndHandler> target_maps_and_handlers;
  nexus()->ExtractMapsAndHandlers(
      &target_maps_and_handlers,
      [this](Handle<Map> map) { return Map::TryUpdate(isolate(), map); });
  if (target_maps_and_handlers.empty()) {
    Handle<Map> monomorphic_map = receiver_map;
    // If we transitioned to a map that is a more general map than incoming
    // then use the new map.
    if (IsTransitionOfMonomorphicTarget(*receiver_map, *new_receiver_map)) {
      monomorphic_map = new_receiver_map;
    }
    Handle<Object> handler = StoreElementHandler(monomorphic_map, store_mode);
    return ConfigureVectorState(Handle<Name>(), monomorphic_map, handler);
  }

  for (const MapAndHandler& map_and_handler : target_maps_and_handlers) {
    Handle<Map> map = map_and_handler.first;
    if (!map.is_null() && map->instance_type() == JS_PRIMITIVE_WRAPPER_TYPE) {
      DCHECK(!IsStoreInArrayLiteralIC());
      set_slow_stub_reason("JSPrimitiveWrapper");
      return;
    }
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different IC that handles a superset of the original IC.
  // Handle those here if the receiver map hasn't changed or it has transitioned
  // to a more general kind.
  KeyedAccessStoreMode old_store_mode = GetKeyedAccessStoreMode();
  Handle<Map> previous_receiver_map = target_maps_and_handlers.at(0).first;
  if (state() == MONOMORPHIC) {
    Handle<Map> transitioned_receiver_map = new_receiver_map;
    if (IsTransitionOfMonomorphicTarget(*previous_receiver_map,
                                        *transitioned_receiver_map)) {
      // If the "old" and "new" maps are in the same elements map family, or
      // if they at least come from the same origin for a transitioning store,
      // stay MONOMORPHIC and use the map for the most generic ElementsKind.
      Handle<Object> handler =
          StoreElementHandler(transitioned_receiver_map, store_mode);
      ConfigureVectorState(Handle<Name>(), transitioned_receiver_map, handler);
      return;
    }
    // If there is no transition and if we have seen the same map earlier and
    // there is only a change in the store_mode we can still stay monomorphic.
    if (receiver_map.is_identical_to(previous_receiver_map) &&
        new_receiver_map.is_identical_to(receiver_map) &&
        old_store_mode == STANDARD_STORE && store_mode != STANDARD_STORE) {
      if (receiver_map->IsJSArrayMap() &&
          JSArray::MayHaveReadOnlyLength(*receiver_map)) {
        set_slow_stub_reason(
            "can't generalize store mode (potentially read-only length)");
        return;
      }
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      Handle<Object> handler = StoreElementHandler(receiver_map, store_mode);
      return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
    }
  }

  DCHECK(state() != GENERIC);

  bool map_added =
      AddOneReceiverMapIfMissing(&target_maps_and_handlers, receiver_map);

  if (IsTransitionOfMonomorphicTarget(*receiver_map, *new_receiver_map)) {
    map_added |=
        AddOneReceiverMapIfMissing(&target_maps_and_handlers, new_receiver_map);
  }

  if (!map_added) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the megamorphic stub which can handle everything.
    set_slow_stub_reason("same map added twice");
    return;
  }

  // If the maximum number of receiver maps has been exceeded, use the
  // megamorphic version of the IC.
  if (static_cast<int>(target_maps_and_handlers.size()) >
      FLAG_max_valid_polymorphic_map_count) {
    return;
  }

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // megamorphic stub must be used.
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      set_slow_stub_reason("store mode mismatch");
      return;
    }
  }

  // If the store mode isn't the standard mode, make sure that all polymorphic
  // receivers are either external arrays, or all "normal" arrays with writable
  // length. Otherwise, use the megamorphic stub.
  if (store_mode != STANDARD_STORE) {
    size_t external_arrays = 0;
    for (MapAndHandler map_and_handler : target_maps_and_handlers) {
      Handle<Map> map = map_and_handler.first;
      if (map->IsJSArrayMap() && JSArray::MayHaveReadOnlyLength(*map)) {
        set_slow_stub_reason(
            "unsupported combination of arrays (potentially read-only length)");
        return;

      } else if (map->has_typed_array_or_rab_gsab_typed_array_elements()) {
        DCHECK(!IsStoreInArrayLiteralIC());
        external_arrays++;
      }
    }
    if (external_arrays != 0 &&
        external_arrays != target_maps_and_handlers.size()) {
      DCHECK(!IsStoreInArrayLiteralIC());
      set_slow_stub_reason(
          "unsupported combination of external and normal arrays");
      return;
    }
  }

  StoreElementPolymorphicHandlers(&target_maps_and_handlers, store_mode);
  if (target_maps_and_handlers.size() == 0) {
    Handle<Object> handler = StoreElementHandler(receiver_map, store_mode);
    ConfigureVectorState(Handle<Name>(), receiver_map, handler);
  } else if (target_maps_and_handlers.size() == 1) {
    ConfigureVectorState(Handle<Name>(), target_maps_and_handlers[0].first,
                         target_maps_and_handlers[0].second);
  } else {
    ConfigureVectorState(Handle<Name>(), target_maps_and_handlers);
  }
}

Handle<Object> KeyedStoreIC::StoreElementHandler(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode,
    MaybeHandle<Object> prev_validity_cell) {
  // The only case when could keep using non-slow element store handler for
  // a fast array with potentially read-only elements is when it's an
  // initializing store to array literal.
  DCHECK_IMPLIES(
      !receiver_map->has_dictionary_elements() &&
          receiver_map->MayHaveReadOnlyElementsInPrototypeChain(isolate()),
      IsStoreInArrayLiteralIC());

  if (receiver_map->IsJSProxyMap()) {
    return StoreHandler::StoreProxy(isolate());
  }

  // TODO(ishell): move to StoreHandler::StoreElement().
  Handle<Object> code;
  if (receiver_map->has_sloppy_arguments_elements()) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_KeyedStoreSloppyArgumentsStub);
    code = CodeHandler(StoreHandler::StoreSloppyArgumentsBuiltin(store_mode));
  } else if (receiver_map->has_fast_elements() ||
             receiver_map->has_sealed_elements() ||
             receiver_map->has_nonextensible_elements() ||
             receiver_map->has_typed_array_or_rab_gsab_typed_array_elements()) {
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreFastElementStub);
    code = CodeHandler(StoreHandler::StoreFastElementBuiltin(store_mode));
    if (receiver_map->has_typed_array_or_rab_gsab_typed_array_elements()) {
      return code;
    }
  } else if (IsStoreInArrayLiteralIC()) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), StoreInArrayLiteralIC_SlowStub);
    return StoreHandler::StoreSlow(isolate(), store_mode);
  } else {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreElementStub);
    DCHECK(DICTIONARY_ELEMENTS == receiver_map->elements_kind() ||
           receiver_map->has_frozen_elements());
    code = StoreHandler::StoreSlow(isolate(), store_mode);
  }

  if (IsAnyDefineOwn() || IsStoreInArrayLiteralIC()) return code;
  Handle<Object> validity_cell;
  if (!prev_validity_cell.ToHandle(&validity_cell)) {
    validity_cell =
        Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  }
  if (validity_cell->IsSmi()) {
    // There's no prototype validity cell to check, so we can just use the stub.
    return code;
  }
  Handle<StoreHandler> handler = isolate()->factory()->NewStoreHandler(0);
  handler->set_validity_cell(*validity_cell);
  handler->set_smi_handler(*code);
  return handler;
}

void KeyedStoreIC::StoreElementPolymorphicHandlers(
    std::vector<MapAndHandler>* receiver_maps_and_handlers,
    KeyedAccessStoreMode store_mode) {
  std::vector<Handle<Map>> receiver_maps;
  for (size_t i = 0; i < receiver_maps_and_handlers->size(); i++) {
    receiver_maps.push_back(receiver_maps_and_handlers->at(i).first);
  }

  for (size_t i = 0; i < receiver_maps_and_handlers->size(); i++) {
    Handle<Map> receiver_map = receiver_maps_and_handlers->at(i).first;
    DCHECK(!receiver_map->is_deprecated());
    MaybeObjectHandle old_handler = receiver_maps_and_handlers->at(i).second;
    Handle<Object> handler;
    Handle<Map> transition;

    if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE ||
        receiver_map->MayHaveReadOnlyElementsInPrototypeChain(isolate())) {
      // TODO(mvstanton): Consider embedding store_mode in the state of the slow
      // keyed store ic for uniformity.
      TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_SlowStub);
      handler = StoreHandler::StoreSlow(isolate());

    } else {
      {
        Map tmap = receiver_map->FindElementsKindTransitionedMap(
            isolate(), receiver_maps, ConcurrencyMode::kSynchronous);
        if (!tmap.is_null()) {
          if (receiver_map->is_stable()) {
            receiver_map->NotifyLeafMapLayoutChange(isolate());
          }
          transition = handle(tmap, isolate());
        }
      }

      MaybeHandle<Object> validity_cell;
      HeapObject old_handler_obj;
      if (!old_handler.is_null() &&
          old_handler->GetHeapObject(&old_handler_obj) &&
          old_handler_obj.IsDataHandler()) {
        validity_cell = MaybeHandle<Object>(
            DataHandler::cast(old_handler_obj).validity_cell(), isolate());
      }
      // TODO(mythria): Do not recompute the handler if we know there is no
      // change in the handler.
      // TODO(mvstanton): The code below is doing pessimistic elements
      // transitions. I would like to stop doing that and rely on Allocation
      // Site Tracking to do a better job of ensuring the data types are what
      // they need to be. Not all the elements are in place yet, pessimistic
      // elements transitions are still important for performance.
      if (!transition.is_null()) {
        TRACE_HANDLER_STATS(isolate(),
                            KeyedStoreIC_ElementsTransitionAndStoreStub);
        handler = StoreHandler::StoreElementTransition(
            isolate(), receiver_map, transition, store_mode, validity_cell);
      } else {
        handler = StoreElementHandler(receiver_map, store_mode, validity_cell);
      }
    }
    DCHECK(!handler.is_null());
    receiver_maps_and_handlers->at(i) =
        MapAndHandler(receiver_map, MaybeObjectHandle(handler));
  }
}

namespace {

bool MayHaveTypedArrayInPrototypeChain(Handle<JSObject> object) {
  for (PrototypeIterator iter(object->GetIsolate(), *object); !iter.IsAtEnd();
       iter.Advance()) {
    // Be conservative, don't walk into proxies.
    if (iter.GetCurrent().IsJSProxy()) return true;
    if (iter.GetCurrent().IsJSTypedArray()) return true;
  }
  return false;
}

KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver, size_t index) {
  bool oob_access = IsOutOfBoundsAccess(receiver, index);
  // Don't consider this a growing store if the store would send the receiver to
  // dictionary mode.
  bool allow_growth =
      receiver->IsJSArray() && oob_access && index <= JSArray::kMaxArrayIndex &&
      !receiver->WouldConvertToSlowElements(static_cast<uint32_t>(index));
  if (allow_growth) {
    return STORE_AND_GROW_HANDLE_COW;
  }
  if (receiver->map().has_typed_array_or_rab_gsab_typed_array_elements() &&
      oob_access) {
    return STORE_IGNORE_OUT_OF_BOUNDS;
  }
  return receiver->elements().IsCowArray() ? STORE_HANDLE_COW : STANDARD_STORE;
}

}  // namespace

MaybeHandle<Object> KeyedStoreIC::Store(Handle<Object> object,
                                        Handle<Object> key,
                                        Handle<Object> value) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(isolate(), object)) {
    Handle<Object> result;
    // TODO(v8:12548): refactor DefineKeyedOwnIC as a subclass of StoreIC
    // so the logic doesn't get mixed here.
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        IsDefineKeyedOwnIC()
            ? Runtime::DefineObjectOwnProperty(isolate(), object, key, value,
                                               StoreOrigin::kMaybeKeyed)
            : Runtime::SetObjectProperty(isolate(), object, key, value,
                                         StoreOrigin::kMaybeKeyed),
        Object);
    return result;
  }

  Handle<Object> store_handle;

  intptr_t maybe_index;
  Handle<Name> maybe_name;
  KeyType key_type = TryConvertKey(key, isolate(), &maybe_index, &maybe_name);

  if (key_type == kName) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), store_handle,
        StoreIC::Store(object, maybe_name, value, StoreOrigin::kMaybeKeyed),
        Object);
    if (vector_needs_update()) {
      if (ConfigureVectorState(MEGAMORPHIC, key)) {
        set_slow_stub_reason("unhandled internalized string key");
        TraceIC("StoreIC", key);
      }
    }
    return store_handle;
  }

  JSObject::MakePrototypesFast(object, kStartAtPrototype, isolate());

  // TODO(jkummerow): Refactor the condition logic here and below.
  bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic &&
                !object->IsStringWrapper() && !object->IsAccessCheckNeeded() &&
                !object->IsJSGlobalProxy();
  if (use_ic && !object->IsSmi()) {
    // Don't use ICs for maps of the objects in Array's prototype chain. We
    // expect to be able to trap element sets to objects with those maps in
    // the runtime to enable optimization of element hole access.
    Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
    if (heap_object->map().IsMapInArrayPrototypeChain(isolate())) {
      set_slow_stub_reason("map in array prototype");
      use_ic = false;
    }
  }

  Handle<Map> old_receiver_map;
  bool is_arguments = false;
  bool key_is_valid_index = (key_type == kIntPtr);
  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (use_ic && object->IsJSReceiver() && key_is_valid_index) {
    Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);
    old_receiver_map = handle(receiver->map(), isolate());
    is_arguments = receiver->IsJSArgumentsObject();
    bool is_proxy = receiver->IsJSProxy();
    size_t index;
    key_is_valid_index = IntPtrKeyToSize(maybe_index, receiver, &index);
    if (!is_arguments && !is_proxy) {
      if (key_is_valid_index) {
        Handle<JSObject> receiver_object = Handle<JSObject>::cast(object);
        store_mode = GetStoreMode(receiver_object, index);
      }
    }
  }

  DCHECK(store_handle.is_null());
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), store_handle,
      // TODO(v8:12548): refactor DefineKeyedOwnIC as a subclass of StoreIC
      // so the logic doesn't get mixed here.
      IsDefineKeyedOwnIC()
          ? Runtime::DefineObjectOwnProperty(isolate(), object, key, value,
                                             StoreOrigin::kMaybeKeyed)
          : Runtime::SetObjectProperty(isolate(), object, key, value,
                                       StoreOrigin::kMaybeKeyed),
      Object);
  if (use_ic) {
    if (!old_receiver_map.is_null()) {
      if (is_arguments) {
        set_slow_stub_reason("arguments receiver");
      } else if (object->IsJSArray() && IsGrowStoreMode(store_mode) &&
                 JSArray::HasReadOnlyLength(Handle<JSArray>::cast(object))) {
        set_slow_stub_reason("array has read only length");
      } else if (object->IsJSObject() && MayHaveTypedArrayInPrototypeChain(
                                             Handle<JSObject>::cast(object))) {
        // Make sure we don't handle this in IC if there's any JSTypedArray in
        // the {receiver}'s prototype chain, since that prototype is going to
        // swallow all stores that are out-of-bounds for said prototype, and we
        // just let the runtime deal with the complexity of this.
        set_slow_stub_reason("typed array in the prototype chain");
      } else if (key_is_valid_index) {
        if (old_receiver_map->is_abandoned_prototype_map()) {
          set_slow_stub_reason("receiver with prototype map");
        } else if (old_receiver_map->has_dictionary_elements() ||
                   !old_receiver_map->MayHaveReadOnlyElementsInPrototypeChain(
                       isolate())) {
          // We should go generic if receiver isn't a dictionary, but our
          // prototype chain does have dictionary elements. This ensures that
          // other non-dictionary receivers in the polymorphic case benefit
          // from fast path keyed stores.
          Handle<HeapObject> receiver = Handle<HeapObject>::cast(object);
          UpdateStoreElement(old_receiver_map, store_mode,
                             handle(receiver->map(), isolate()));
        } else {
          set_slow_stub_reason("prototype with potentially read-only elements");
        }
      } else {
        set_slow_stub_reason("non-smi-like key");
      }
    } else {
      set_slow_stub_reason("non-JSObject receiver");
    }
  }

  if (vector_needs_update()) {
    ConfigureVectorState(MEGAMORPHIC, key);
  }
  TraceIC("StoreIC", key);

  return store_handle;
}

namespace {
Maybe<bool> StoreOwnElement(Isolate* isolate, Handle<JSArray> array,
                            Handle<Object> index, Handle<Object> value) {
  DCHECK(index->IsNumber());
  PropertyKey key(isolate, index);
  LookupIterator it(isolate, array, key, LookupIterator::OWN);

  MAYBE_RETURN(JSObject::DefineOwnPropertyIgnoreAttributes(
                   &it, value, NONE, Just(ShouldThrow::kThrowOnError)),
               Nothing<bool>());
  return Just(true);
}
}  // namespace

MaybeHandle<Object> StoreInArrayLiteralIC::Store(Handle<JSArray> array,
                                                 Handle<Object> index,
                                                 Handle<Object> value) {
  DCHECK(!array->map().IsMapInArrayPrototypeChain(isolate()));
  DCHECK(index->IsNumber());

  if (!FLAG_use_ic || state() == NO_FEEDBACK ||
      MigrateDeprecated(isolate(), array)) {
    MAYBE_RETURN_NULL(StoreOwnElement(isolate(), array, index, value));
    TraceIC("StoreInArrayLiteralIC", index);
    return value;
  }

  // TODO(neis): Convert HeapNumber to Smi if possible?

  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (index->IsSmi()) {
    DCHECK_GE(Smi::ToInt(*index), 0);
    uint32_t index32 = static_cast<uint32_t>(Smi::ToInt(*index));
    store_mode = GetStoreMode(array, index32);
  }

  Handle<Map> old_array_map(array->map(), isolate());
  MAYBE_RETURN_NULL(StoreOwnElement(isolate(), array, index, value));

  if (index->IsSmi()) {
    DCHECK(!old_array_map->is_abandoned_prototype_map());
    UpdateStoreElement(old_array_map, store_mode,
                       handle(array->map(), isolate()));
  } else {
    set_slow_stub_reason("index out of Smi range");
  }

  if (vector_needs_update()) {
    ConfigureVectorState(MEGAMORPHIC, index);
  }
  TraceIC("StoreInArrayLiteralIC", index);
  return value;
}

// ----------------------------------------------------------------------------
// Static IC stub generators.
//
//
RUNTIME_FUNCTION(Runtime_LoadIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(2);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(3);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // A monomorphic or polymorphic KeyedLoadIC with a string key can call the
  // LoadIC miss handler if the handler misses. Since the vector Nexus is
  // set up outside the IC, handle that here.
  FeedbackSlotKind kind = vector->GetKind(vector_slot);
  if (IsLoadICKind(kind)) {
    LoadIC ic(isolate, vector, vector_slot, kind);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));

  } else if (IsLoadGlobalICKind(kind)) {
    DCHECK_EQ(isolate->native_context()->global_proxy(), *receiver);
    receiver = isolate->global_object();
    LoadGlobalIC ic(isolate, vector, vector_slot, kind);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(key));

  } else {
    DCHECK(IsKeyedLoadICKind(kind));
    KeyedLoadIC ic(isolate, vector, vector_slot, kind);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
  }
}

RUNTIME_FUNCTION(Runtime_LoadNoFeedbackIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Name> key = args.at<Name>(1);
  int slot_kind = args.smi_value_at(2);
  FeedbackSlotKind kind = static_cast<FeedbackSlotKind>(slot_kind);

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  FeedbackSlot vector_slot = FeedbackSlot::Invalid();
  // This function is only called after looking up in the ScriptContextTable so
  // it is safe to call LoadIC::Load for global loads as well.
  LoadIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
}

RUNTIME_FUNCTION(Runtime_LoadWithReceiverNoFeedbackIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> object = args.at(1);
  Handle<Name> key = args.at<Name>(2);

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  FeedbackSlot vector_slot = FeedbackSlot::Invalid();
  LoadIC ic(isolate, vector, vector_slot, FeedbackSlotKind::kLoadProperty);
  ic.UpdateState(object, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(object, key, true, receiver));
}

RUNTIME_FUNCTION(Runtime_LoadGlobalIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<String> name = args.at<String>(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  int typeof_value = args.smi_value_at(3);
  TypeofMode typeof_mode = static_cast<TypeofMode>(typeof_value);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }

  FeedbackSlotKind kind = (typeof_mode == TypeofMode::kInside)
                              ? FeedbackSlotKind::kLoadGlobalInsideTypeof
                              : FeedbackSlotKind::kLoadGlobalNotInsideTypeof;
  LoadGlobalIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(global, name);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(name));
  return *result;
}

RUNTIME_FUNCTION(Runtime_LoadGlobalIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<String> name = args.at<String>(0);

  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  FeedbackSlotKind kind = vector->GetKind(vector_slot);

  LoadGlobalIC ic(isolate, vector, vector_slot, kind);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(name, false));
  return *result;
}

RUNTIME_FUNCTION(Runtime_LoadWithReceiverIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> object = args.at(1);
  Handle<Name> key = args.at<Name>(2);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(3);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(4);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  DCHECK(IsLoadICKind(vector->GetKind(vector_slot)));
  LoadIC ic(isolate, vector, vector_slot, FeedbackSlotKind::kLoadProperty);
  ic.UpdateState(object, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(object, key, true, receiver));
}

RUNTIME_FUNCTION(Runtime_KeyedLoadIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(2);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(3);

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  KeyedLoadIC ic(isolate, vector, vector_slot, FeedbackSlotKind::kLoadKeyed);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
}

RUNTIME_FUNCTION(Runtime_StoreIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Name> key = args.at<Name>(4);

  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // When there is no feedback vector it is OK to use the SetNamedStrict as
  // the feedback slot kind. We only reuse this for DefineNamedOwnIC when
  // installing the handler for storing const properties. This will happen only
  // when feedback vector is available.
  FeedbackSlotKind kind = FeedbackSlotKind::kSetNamedStrict;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
  }

  DCHECK(IsSetNamedICKind(kind) || IsDefineNamedOwnICKind(kind));
  StoreIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
}

RUNTIME_FUNCTION(Runtime_DefineNamedOwnIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Name> key = args.at<Name>(4);

  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // When there is no feedback vector it is OK to use the DefineNamedOwn
  // feedback kind. There _should_ be a vector, though.
  FeedbackSlotKind kind = FeedbackSlotKind::kDefineNamedOwn;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
  }

  DCHECK(IsDefineNamedOwnICKind(kind));

  // TODO(v8:12548): refactor DefineNamedOwnIC as a subclass of StoreIC, which
  // can be called here.
  StoreIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
}

RUNTIME_FUNCTION(Runtime_DefineNamedOwnIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  Handle<Object> value = args.at(0);
  Handle<Object> object = args.at(1);
  Handle<Object> key = args.at(2);

  // Unlike DefineKeyedOwnIC, DefineNamedOwnIC doesn't handle private
  // fields and is used for defining data properties in object literals
  // and defining named public class fields.
  DCHECK(!key->IsSymbol() || !Symbol::cast(*key).is_private_name());

  PropertyKey lookup_key(isolate, key);
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);

  MAYBE_RETURN(
      JSReceiver::CreateDataProperty(&it, value, Nothing<ShouldThrow>()),
      ReadOnlyRoots(isolate).exception());
  return *value;
}

RUNTIME_FUNCTION(Runtime_StoreGlobalIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  Handle<Name> key = args.at<Name>(3);

  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  FeedbackSlotKind kind = vector->GetKind(vector_slot);
  StoreGlobalIC ic(isolate, vector, vector_slot, kind);
  Handle<JSGlobalObject> global = isolate->global_object();
  ic.UpdateState(global, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(key, value));
}

RUNTIME_FUNCTION(Runtime_StoreGlobalICNoFeedback_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Name> key = args.at<Name>(1);

  // TODO(mythria): Replace StoreGlobalStrict/Sloppy with SetNamedProperty.
  StoreGlobalIC ic(isolate, Handle<FeedbackVector>(), FeedbackSlot(),
                   FeedbackSlotKind::kStoreGlobalStrict);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(key, value));
}

// TODO(mythria): Remove Feedback vector and slot. Since they are not used apart
// from the DCHECK.
RUNTIME_FUNCTION(Runtime_StoreGlobalIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<String> name = args.at<String>(4);

#ifdef DEBUG
  {
    Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
    Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
    FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
    FeedbackSlotKind slot_kind = vector->GetKind(vector_slot);
    DCHECK(IsStoreGlobalICKind(slot_kind));
    Handle<Object> receiver = args.at(3);
    DCHECK(receiver->IsJSGlobalProxy());
  }
#endif

  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<Context> native_context = isolate->native_context();
  Handle<ScriptContextTable> script_contexts(
      native_context->script_context_table(), isolate);

  VariableLookupResult lookup_result;
  if (script_contexts->Lookup(name, &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        isolate, script_contexts, lookup_result.context_index);
    if (lookup_result.mode == VariableMode::kConst) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewTypeError(MessageTemplate::kConstAssign, global, name));
    }

    Handle<Object> previous_value(script_context->get(lookup_result.slot_index),
                                  isolate);

    if (previous_value->IsTheHole(isolate)) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewReferenceError(
                       MessageTemplate::kAccessedUninitializedVariable, name));
    }

    script_context->set(lookup_result.slot_index, *value);
    return *value;
  }

  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, global, name, value,
                                          StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Object> key = args.at(4);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // When the feedback vector is not valid the slot can only be of type
  // StoreKeyed. Storing in array literals falls back to
  // StoreInArrayLiterIC_Miss. This function is also used from store handlers
  // installed in feedback vectors. In such cases, we need to get the kind from
  // feedback vector slot since the handlers are used for both for StoreKeyed
  // and StoreInArrayLiteral kinds.
  FeedbackSlotKind kind = FeedbackSlotKind::kSetKeyedStrict;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
  }

  // The elements store stubs miss into this function, but they are shared by
  // different ICs.
  // TODO(v8:12548): refactor DefineKeyedOwnIC as a subclass of KeyedStoreIC,
  // which can be called here.
  if (IsKeyedStoreICKind(kind) || IsDefineKeyedOwnICKind(kind)) {
    KeyedStoreIC ic(isolate, vector, vector_slot, kind);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
  } else {
    DCHECK(IsStoreInArrayLiteralICKind(kind));
    DCHECK(receiver->IsJSArray());
    DCHECK(key->IsNumber());
    StoreInArrayLiteralIC ic(isolate, vector, vector_slot);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(
        isolate, ic.Store(Handle<JSArray>::cast(receiver), key, value));
  }
}

RUNTIME_FUNCTION(Runtime_DefineKeyedOwnIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Object> key = args.at(4);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  FeedbackSlotKind kind = FeedbackSlotKind::kDefineKeyedOwn;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
    DCHECK(IsDefineKeyedOwnICKind(kind));
  }

  // TODO(v8:12548): refactor DefineKeyedOwnIC as a subclass of KeyedStoreIC,
  // which can be called here.
  KeyedStoreIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
}

RUNTIME_FUNCTION(Runtime_StoreInArrayLiteralIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Object> key = args.at(4);
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  DCHECK(receiver->IsJSArray());
  DCHECK(key->IsNumber());
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  StoreInArrayLiteralIC ic(isolate, vector, vector_slot);
  RETURN_RESULT_OR_FAILURE(
      isolate, ic.Store(Handle<JSArray>::cast(receiver), key, value));
}

RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Object> object = args.at(1);
  Handle<Object> key = args.at(2);
  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                          StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_DefineKeyedOwnIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Object> object = args.at(1);
  Handle<Object> key = args.at(2);
  RETURN_RESULT_OR_FAILURE(
      isolate, Runtime::DefineObjectOwnProperty(isolate, object, key, value,
                                                StoreOrigin::kMaybeKeyed));
}

RUNTIME_FUNCTION(Runtime_StoreInArrayLiteralIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Object> array = args.at(1);
  Handle<Object> index = args.at(2);
  StoreOwnElement(isolate, Handle<JSArray>::cast(array), index, value);
  return *value;
}

RUNTIME_FUNCTION(Runtime_ElementsTransitionAndStoreIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(6, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);
  Handle<Map> map = args.at<Map>(3);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(4);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(5);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  FeedbackSlotKind kind = vector->GetKind(vector_slot);

  if (object->IsJSObject()) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object),
                                     map->elements_kind());
  }

  if (IsStoreInArrayLiteralICKind(kind)) {
    StoreOwnElement(isolate, Handle<JSArray>::cast(object), key, value);
    return *value;
  } else {
    DCHECK(IsKeyedStoreICKind(kind) || IsSetNamedICKind(kind) ||
           IsDefineKeyedOwnICKind(kind));
    RETURN_RESULT_OR_FAILURE(
        isolate,
        IsDefineKeyedOwnICKind(kind)
            ? Runtime::DefineObjectOwnProperty(isolate, object, key, value,
                                               StoreOrigin::kMaybeKeyed)
            : Runtime::SetObjectProperty(isolate, object, key, value,
                                         StoreOrigin::kMaybeKeyed));
  }
}

static bool CanFastCloneObject(Handle<Map> map) {
  DisallowGarbageCollection no_gc;
  if (map->IsNullOrUndefinedMap()) return true;
  if (!map->IsJSObjectMap() ||
      !IsSmiOrObjectElementsKind(map->elements_kind()) ||
      !map->OnlyHasSimpleProperties()) {
    return false;
  }

  DescriptorArray descriptors = map->instance_descriptors();
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details = descriptors.GetDetails(i);
    Name key = descriptors.GetKey(i);
    if (details.kind() != PropertyKind::kData || !details.IsEnumerable() ||
        key.IsPrivateName()) {
      return false;
    }
  }

  return true;
}

static Handle<Map> FastCloneObjectMap(Isolate* isolate, Handle<Map> source_map,
                                      int flags) {
  SLOW_DCHECK(CanFastCloneObject(source_map));
  Handle<JSFunction> constructor(isolate->native_context()->object_function(),
                                 isolate);
  DCHECK(constructor->has_initial_map());
  Handle<Map> initial_map(constructor->initial_map(), isolate);
  Handle<Map> map = initial_map;

  if (source_map->IsJSObjectMap() && source_map->GetInObjectProperties() !=
                                         initial_map->GetInObjectProperties()) {
    int inobject_properties = source_map->GetInObjectProperties();
    int instance_size =
        JSObject::kHeaderSize + kTaggedSize * inobject_properties;
    int unused = source_map->UnusedInObjectProperties();
    DCHECK(instance_size <= JSObject::kMaxInstanceSize);
    map = Map::CopyInitialMap(isolate, map, instance_size, inobject_properties,
                              unused);
  }

  if (flags & ObjectLiteral::kHasNullPrototype) {
    if (map.is_identical_to(initial_map)) {
      map = Map::Copy(isolate, map, "ObjectWithNullProto");
    }
    Map::SetPrototype(isolate, map, isolate->factory()->null_value());
  }

  if (source_map->NumberOfOwnDescriptors() == 0) {
    return map;
  }
  DCHECK(!source_map->IsNullOrUndefinedMap());

  if (map.is_identical_to(initial_map)) {
    map = Map::Copy(isolate, map, "InitializeClonedDescriptors");
  }

  Handle<DescriptorArray> source_descriptors(
      source_map->instance_descriptors(isolate), isolate);
  int size = source_map->NumberOfOwnDescriptors();
  int slack = 0;
  Handle<DescriptorArray> descriptors = DescriptorArray::CopyForFastObjectClone(
      isolate, source_descriptors, size, slack);
  map->InitializeDescriptors(isolate, *descriptors);
  map->CopyUnusedPropertyFieldsAdjustedForInstanceSize(*source_map);

  // Update bitfields
  map->set_may_have_interesting_symbols(
      source_map->may_have_interesting_symbols());

  return map;
}

static MaybeHandle<JSObject> CloneObjectSlowPath(Isolate* isolate,
                                                 Handle<Object> source,
                                                 int flags) {
  Handle<JSObject> new_object;
  if (flags & ObjectLiteral::kHasNullPrototype) {
    new_object = isolate->factory()->NewJSObjectWithNullProto();
  } else {
    Handle<JSFunction> constructor(isolate->native_context()->object_function(),
                                   isolate);
    new_object = isolate->factory()->NewJSObject(constructor);
  }

  if (source->IsNullOrUndefined()) {
    return new_object;
  }

  MAYBE_RETURN(
      JSReceiver::SetOrCopyDataProperties(
          isolate, new_object, source,
          PropertiesEnumerationMode::kPropertyAdditionOrder, nullptr, false),
      MaybeHandle<JSObject>());
  return new_object;
}

RUNTIME_FUNCTION(Runtime_CloneObjectIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<Object> source = args.at(0);
  int flags = args.smi_value_at(1);

  if (!MigrateDeprecated(isolate, source)) {
    int index = args.tagged_index_value_at(2);
    FeedbackSlot slot = FeedbackVector::ToSlot(index);
    Handle<HeapObject> maybe_vector = args.at<HeapObject>(3);
    if (maybe_vector->IsFeedbackVector()) {
      FeedbackNexus nexus(Handle<FeedbackVector>::cast(maybe_vector), slot);
      if (!source->IsSmi() && !nexus.IsMegamorphic()) {
        Handle<Map> source_map(Handle<HeapObject>::cast(source)->map(),
                               isolate);
        if (CanFastCloneObject(source_map)) {
          Handle<Map> target_map =
              FastCloneObjectMap(isolate, source_map, flags);
          nexus.ConfigureCloneObject(source_map, target_map);
          return *target_map;
        }

        nexus.ConfigureMegamorphic();
      }
    }
  }

  RETURN_RESULT_OR_FAILURE(isolate,
                           CloneObjectSlowPath(isolate, source, flags));
}

RUNTIME_FUNCTION(Runtime_StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<AccessorInfo> info = args.at<AccessorInfo>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at(4);
  HandleScope scope(isolate);

#ifdef V8_RUNTIME_CALL_STATS
  if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {
    RETURN_RESULT_OR_FAILURE(
        isolate, Runtime::SetObjectProperty(isolate, receiver, name, value,
                                            StoreOrigin::kMaybeKeyed));
  }
#endif

  DCHECK(info->IsCompatibleReceiver(*receiver));

  PropertyCallbackArguments arguments(isolate, info->data(), *receiver, *holder,
                                      Nothing<ShouldThrow>());
  arguments.CallAccessorSetter(info, name, value);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return *value;
}

/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
RUNTIME_FUNCTION(Runtime_LoadPropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  Handle<Name> name = args.at<Name>(0);
  Handle<Object> receiver = args.at(1);
  Handle<JSObject> holder = args.at<JSObject>(2);

  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver));
  }

  Handle<InterceptorInfo> interceptor(holder->GetNamedInterceptor(), isolate);
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *holder, Just(kDontThrow));
  Handle<Object> result = arguments.CallNamedGetter(interceptor, name);

  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);

  if (!result.is_null()) return *result;

  LookupIterator it(isolate, receiver, name, holder);
  // Skip any lookup work until we hit the (possibly non-masking) interceptor.
  while (it.state() != LookupIterator::INTERCEPTOR ||
         !it.GetHolder<JSObject>().is_identical_to(holder)) {
    DCHECK(it.state() != LookupIterator::ACCESS_CHECK || it.HasAccess());
    it.Next();
  }
  // Skip past the interceptor.
  it.Next();
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, Object::GetProperty(&it));

  if (it.IsFound()) return *result;

  Handle<TaggedIndex> slot = args.at<TaggedIndex>(3);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(4);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  FeedbackSlotKind slot_kind = vector->GetKind(vector_slot);
  // It could actually be any kind of load IC slot here but the predicate
  // handles all the cases properly.
  if (!LoadIC::ShouldThrowReferenceError(slot_kind)) {
    return ReadOnlyRoots(isolate).undefined_value();
  }

  // Throw a reference error.
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNotDefined, it.name()));
}

RUNTIME_FUNCTION(Runtime_StorePropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<JSObject> receiver = args.at<JSObject>(1);
  Handle<Name> name = args.at<Name>(2);

  // TODO(ishell): Cache interceptor_holder in the store handler like we do
  // for LoadHandler::kInterceptor case.
  Handle<JSObject> interceptor_holder = receiver;
  if (receiver->IsJSGlobalProxy() &&
      (!receiver->HasNamedInterceptor() ||
       receiver->GetNamedInterceptor().non_masking())) {
    interceptor_holder =
        handle(JSObject::cast(receiver->map().prototype()), isolate);
  }
  DCHECK(interceptor_holder->HasNamedInterceptor());
  Handle<InterceptorInfo> interceptor(interceptor_holder->GetNamedInterceptor(),
                                      isolate);

  DCHECK(!interceptor->non_masking());
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *receiver, Just(kDontThrow));

  Handle<Object> result = arguments.CallNamedSetter(interceptor, name, value);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (!result.is_null()) return *value;

  LookupIterator it(isolate, receiver, name, receiver);
  // Skip past any access check on the receiver.
  if (it.state() == LookupIterator::ACCESS_CHECK) {
    DCHECK(it.HasAccess());
    it.Next();
  }
  // Skip past the interceptor on the receiver.
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it.state());
  it.Next();

  MAYBE_RETURN(Object::SetProperty(&it, value, StoreOrigin::kNamed),
               ReadOnlyRoots(isolate).exception());
  return *value;
}

RUNTIME_FUNCTION(Runtime_LoadElementWithInterceptor) {
  // TODO(verwaest): This should probably get the holder and receiver as input.
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  DCHECK_GE(args.smi_value_at(1), 0);
  uint32_t index = args.smi_value_at(1);

  Handle<InterceptorInfo> interceptor(receiver->GetIndexedInterceptor(),
                                      isolate);
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *receiver, Just(kDontThrow));
  Handle<Object> result = arguments.CallIndexedGetter(interceptor, index);

  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);

  if (result.is_null()) {
    LookupIterator it(isolate, receiver, index, receiver);
    DCHECK_EQ(LookupIterator::INTERCEPTOR, it.state());
    it.Next();
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                       Object::GetProperty(&it));
  }

  return *result;
}

RUNTIME_FUNCTION(Runtime_KeyedHasIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<TaggedIndex> slot = args.at<TaggedIndex>(2);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(3);

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
  KeyedLoadIC ic(isolate, vector, vector_slot, FeedbackSlotKind::kHasKeyed);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
}

RUNTIME_FUNCTION(Runtime_HasElementWithInterceptor) {
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  DCHECK_GE(args.smi_value_at(1), 0);
  uint32_t index = args.smi_value_at(1);

  Handle<InterceptorInfo> interceptor(receiver->GetIndexedInterceptor(),
                                      isolate);
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *receiver, Just(kDontThrow));

  if (!interceptor->query().IsUndefined(isolate)) {
    Handle<Object> result = arguments.CallIndexedQuery(interceptor, index);
    if (!result.is_null()) {
      int32_t value;
      CHECK(result->ToInt32(&value));
      return value == ABSENT ? ReadOnlyRoots(isolate).false_value()
                             : ReadOnlyRoots(isolate).true_value();
    }
  } else if (!interceptor->getter().IsUndefined(isolate)) {
    Handle<Object> result = arguments.CallIndexedGetter(interceptor, index);
    if (!result.is_null()) {
      return ReadOnlyRoots(isolate).true_value();
    }
  }

  LookupIterator it(isolate, receiver, index, receiver);
  DCHECK_EQ(LookupIterator::INTERCEPTOR, it.state());
  it.Next();
  Maybe<bool> maybe = JSReceiver::HasProperty(&it);
  if (maybe.IsNothing()) return ReadOnlyRoots(isolate).exception();
  return maybe.FromJust() ? ReadOnlyRoots(isolate).true_value()
                          : ReadOnlyRoots(isolate).false_value();
}

}  // namespace internal
}  // namespace v8
