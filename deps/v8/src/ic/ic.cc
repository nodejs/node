// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic.h"

#include "src/api/api-arguments-inl.h"
#include "src/api/api.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/builtins/accessors.h"
#include "src/codegen/code-factory.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/execution.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
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
#include "src/objects/js-array-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/struct-inl.h"
#ifdef V8_TRACE_FEEDBACK_UPDATES
#include "src/utils/ostreams.h"
#endif  // V8_TRACE_FEEDBACK_UPDATES
#include "src/execution/runtime-profiler.h"
#include "src/objects/prototype.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/tracing-category-observer.h"

namespace v8 {
namespace internal {

char IC::TransitionMarkFromState(IC::State state) {
  switch (state) {
    case NO_FEEDBACK:
      return 'X';
    case UNINITIALIZED:
      return '0';
    case PREMONOMORPHIC:
      return '.';
    case MONOMORPHIC:
      return '1';
    case RECOMPUTE_HANDLER:
      return '^';
    case POLYMORPHIC:
      return 'P';
    case MEGAMORPHIC:
      return 'N';
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
  if (HostIsDeoptimizedCode()) return;
  State new_state =
      (state() == NO_FEEDBACK) ? NO_FEEDBACK : nexus()->ic_state();
  TraceIC(type, name, state(), new_state);
}

void IC::TraceIC(const char* type, Handle<Object> name, State old_state,
                 State new_state) {
  if (V8_LIKELY(!TracingFlags::is_ic_stats_enabled())) return;

  Map map;
  if (!receiver_map().is_null()) {
    map = *receiver_map();
  }

  const char* modifier = "";
  if (state() == NO_FEEDBACK) {
    modifier = "";
  } else if (IsKeyedLoadIC()) {
    KeyedAccessLoadMode mode = nexus()->GetKeyedAccessLoadMode();
    modifier = GetModifier(mode);
  } else if (IsKeyedStoreIC() || IsStoreInArrayLiteralICKind(kind())) {
    KeyedAccessStoreMode mode = nexus()->GetKeyedAccessStoreMode();
    modifier = GetModifier(mode);
  }

  bool keyed_prefix = is_keyed() && !IsStoreInArrayLiteralICKind(kind());

  if (!(TracingFlags::ic_stats.load(std::memory_order_relaxed) &
        v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    LOG(isolate(), ICEvent(type, keyed_prefix, map, *name,
                           TransitionMarkFromState(old_state),
                           TransitionMarkFromState(new_state), modifier,
                           slow_stub_reason_));
    return;
  }

  ICStats::instance()->Begin();
  ICInfo& ic_info = ICStats::instance()->Current();
  ic_info.type = keyed_prefix ? "Keyed" : "";
  ic_info.type += type;

  Object maybe_function =
      Object(Memory<Address>(fp_ + JavaScriptFrameConstants::kFunctionOffset));
  DCHECK(maybe_function.IsJSFunction());
  JSFunction function = JSFunction::cast(maybe_function);
  int code_offset = 0;
  if (function.IsInterpreted()) {
    code_offset = InterpretedFrame::GetBytecodeOffset(fp());
  } else {
    code_offset = static_cast<int>(pc() - function.code().InstructionStart());
  }
  JavaScriptFrame::CollectFunctionAndOffsetForICStats(
      function, function.abstract_code(), code_offset);

  // Reserve enough space for IC transition state, the longest length is 17.
  ic_info.state.reserve(17);
  ic_info.state = "(";
  ic_info.state += TransitionMarkFromState(old_state);
  ic_info.state += "->";
  ic_info.state += TransitionMarkFromState(new_state);
  ic_info.state += modifier;
  ic_info.state += ")";
  ic_info.map = reinterpret_cast<void*>(map.ptr());
  if (!map.is_null()) {
    ic_info.is_dictionary_map = map.is_dictionary_map();
    ic_info.number_of_own_descriptors = map.NumberOfOwnDescriptors();
    ic_info.instance_type = std::to_string(map.instance_type());
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
  // To improve the performance of the (much used) IC code, we unfold a few
  // levels of the stack frame iteration code. This yields a ~35% speedup when
  // running DeltaBlue and a ~25% speedup of gbemu with the '--nouse-ic' flag.
  const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
  Address* constant_pool = nullptr;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool = reinterpret_cast<Address*>(
        entry + ExitFrameConstants::kConstantPoolOffset);
  }
  Address* pc_address =
      reinterpret_cast<Address*>(entry + ExitFrameConstants::kCallerPCOffset);
  Address fp = Memory<Address>(entry + ExitFrameConstants::kCallerFPOffset);
#ifdef DEBUG
  StackFrameIterator it(isolate);
  for (int i = 0; i < 1; i++) it.Advance();
  StackFrame* frame = it.frame();
  DCHECK(fp == frame->fp() && pc_address == frame->pc_address());
#endif
  // For interpreted functions, some bytecode handlers construct a
  // frame. We have to skip the constructed frame to find the interpreted
  // function's frame. Check if the there is an additional frame, and if there
  // is skip this frame. However, the pc should not be updated. The call to
  // ICs happen from bytecode handlers.
  intptr_t frame_marker =
      Memory<intptr_t>(fp + TypedFrameConstants::kFrameTypeOffset);
  if (frame_marker == StackFrame::TypeToMarker(StackFrame::STUB)) {
    fp = Memory<Address>(fp + TypedFrameConstants::kCallerFPOffset);
  }
  fp_ = fp;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool_address_ = constant_pool;
  }
  pc_address_ = StackFrame::ResolveReturnAddressLocation(pc_address);
  DCHECK_IMPLIES(!vector.is_null(), kind_ == nexus_.kind());
  state_ = (vector.is_null()) ? NO_FEEDBACK : nexus_.ic_state();
  old_state_ = state_;
}

JSFunction IC::GetHostFunction() const {
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it(isolate());
  while (it.frame()->fp() != this->fp()) it.Advance();
  JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
  // Find the function on the stack and both the active code for the
  // function and the original code.
  return frame->function();
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

  maybe_handler_ = nexus()->FindHandlerForMap(receiver_map());

  // The current map wasn't handled yet. There's no reason to stay monomorphic,
  // *unless* we're moving from a deprecated map to its replacement, or
  // to a more general elements kind.
  // TODO(verwaest): Check if the current map is actually what the old map
  // would transition to.
  if (maybe_handler_.is_null()) {
    if (!receiver_map()->IsJSObjectMap()) return false;
    Map first_map = FirstTargetMap();
    if (first_map.is_null()) return false;
    Handle<Map> old_map(first_map, isolate());
    if (old_map->is_deprecated()) return true;
    return IsMoreGeneralElementsKindTransition(old_map->elements_kind(),
                                               receiver_map()->elements_kind());
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

void IC::UpdateState(Handle<Object> receiver, Handle<Object> name) {
  if (state() == NO_FEEDBACK) return;
  update_receiver_map(receiver);
  if (!name->IsString()) return;
  if (state() != MONOMORPHIC && state() != POLYMORPHIC) return;
  if (receiver->IsNullOrUndefined(isolate())) return;

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

// static
void IC::OnFeedbackChanged(Isolate* isolate, FeedbackNexus* nexus,
                           JSFunction host_function, const char* reason) {
  FeedbackVector vector = nexus->vector();
  FeedbackSlot slot = nexus->slot();
  OnFeedbackChanged(isolate, vector, slot, host_function, reason);
}

// static
void IC::OnFeedbackChanged(Isolate* isolate, FeedbackVector vector,
                           FeedbackSlot slot, JSFunction host_function,
                           const char* reason) {
  if (FLAG_trace_opt_verbose) {
    // TODO(leszeks): The host function is only needed for this print, we could
    // remove it as a parameter if we're of with removing this trace (or only
    // tracing the feedback vector, not the function name).
    if (vector.profiler_ticks() != 0) {
      PrintF("[resetting ticks for ");
      host_function.ShortPrint();
      PrintF(" due from %d due to IC change: %s]\n", vector.profiler_ticks(),
             reason);
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

  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
}

static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map().is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}

bool IC::ConfigureVectorState(IC::State new_state, Handle<Object> key) {
  DCHECK_EQ(MEGAMORPHIC, new_state);
  DCHECK_IMPLIES(!is_keyed(), key->IsName());
  // Even though we don't change the feedback data, we still want to reset the
  // profiler ticks. Real-world observations suggest that optimizing these
  // functions doesn't improve performance.
  bool changed =
      nexus()->ConfigureMegamorphic(key->IsName() ? PROPERTY : ELEMENT);
  vector_set_ = true;
  OnFeedbackChanged(isolate(), nexus(), GetHostFunction(), "Megamorphic");
  return changed;
}

void IC::ConfigureVectorState(Handle<Map> map) {
  nexus()->ConfigurePremonomorphic(map);
  vector_set_ = true;
  OnFeedbackChanged(isolate(), nexus(), GetHostFunction(), "Premonomorphic");
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

  vector_set_ = true;
  OnFeedbackChanged(isolate(), nexus(), GetHostFunction(),
                    IsLoadGlobalIC() ? "LoadGlobal" : "Monomorphic");
}

void IC::ConfigureVectorState(Handle<Name> name, MapHandles const& maps,
                              MaybeObjectHandles* handlers) {
  DCHECK(!IsGlobalIC());
  // Non-keyed ICs don't track the name explicitly.
  if (!is_keyed()) name = Handle<Name>::null();
  nexus()->ConfigurePolymorphic(name, maps, handlers);

  vector_set_ = true;
  OnFeedbackChanged(isolate(), nexus(), GetHostFunction(), "Polymorphic");
}

MaybeHandle<Object> LoadIC::Load(Handle<Object> object, Handle<Name> name) {
  bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic;

  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (IsAnyHas() ? !object->IsJSReceiver()
                 : object->IsNullOrUndefined(isolate())) {
    if (use_ic && state() != PREMONOMORPHIC) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), LoadIC_NonReceiver);
      update_receiver_map(object);
      PatchCache(name, slow_stub());
      TraceIC("LoadIC", name);
    }

    if (*name == ReadOnlyRoots(isolate()).iterator_symbol()) {
      return Runtime::ThrowIteratorError(isolate(), object);
    }
    return TypeError(IsAnyHas() ? MessageTemplate::kInvalidInOperatorUse
                                : MessageTemplate::kNonObjectPropertyLoad,
                     object, name);
  }

  if (MigrateDeprecated(object)) use_ic = false;

  if (state() != UNINITIALIZED) {
    JSObject::MakePrototypesFast(object, kStartAtReceiver, isolate());
    update_receiver_map(object);
  }

  LookupIterator it(isolate(), object, name);

  // Named lookup in the object.
  LookupForRead(&it, IsAnyHas());

  if (name->IsPrivate()) {
    if (name->IsPrivateName() && !it.IsFound()) {
      Handle<String> name_string(String::cast(Symbol::cast(*name).name()),
                                 isolate());
      return TypeError(MessageTemplate::kInvalidPrivateFieldRead, object,
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
    if (use_ic) UpdateCaches(&it);

    if (IsAnyHas()) {
      // Named lookup in the object.
      Maybe<bool> maybe = JSReceiver::HasProperty(&it);
      if (maybe.IsNothing()) return MaybeHandle<Object>();
      return maybe.FromJust() ? ReadOnlyRoots(isolate()).true_value_handle()
                              : ReadOnlyRoots(isolate()).false_value_handle();
    }

    // Get the property.
    Handle<Object> result;

    ASSIGN_RETURN_ON_EXCEPTION(isolate(), result, Object::GetProperty(&it),
                               Object);
    if (it.IsFound()) {
      return result;
    } else if (!ShouldThrowReferenceError()) {
      LOG(isolate(), SuspectReadEvent(*name, *object));
      return result;
    }
  }
  return ReferenceError(name);
}

MaybeHandle<Object> LoadGlobalIC::Load(Handle<Name> name) {
  Handle<JSGlobalObject> global = isolate()->global_object();

  if (name->IsString()) {
    // Look up in script context table.
    Handle<String> str_name = Handle<String>::cast(name);
    Handle<ScriptContextTable> script_contexts(
        global->native_context().script_context_table(), isolate());

    ScriptContextTable::LookupResult lookup_result;
    if (ScriptContextTable::Lookup(isolate(), *script_contexts, *str_name,
                                   &lookup_result)) {
      Handle<Context> script_context = ScriptContextTable::GetContext(
          isolate(), script_contexts, lookup_result.context_index);

      Handle<Object> result(script_context->get(lookup_result.slot_index),
                            isolate());

      if (result->IsTheHole(isolate())) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        return ReferenceError(name);
      }

      bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic;
      if (use_ic) {
        if (nexus()->ConfigureLexicalVarMode(
                lookup_result.context_index, lookup_result.slot_index,
                lookup_result.mode == VariableMode::kConst)) {
          TRACE_HANDLER_STATS(isolate(), LoadGlobalIC_LoadScriptContextField);
        } else {
          // Given combination of indices can't be encoded, so use slow stub.
          TRACE_HANDLER_STATS(isolate(), LoadGlobalIC_SlowStub);
          PatchCache(name, slow_stub());
        }
        TraceIC("LoadGlobalIC", name);
      }
      return result;
    }
  }
  return LoadIC::Load(global, name);
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

bool IC::UpdatePolymorphicIC(Handle<Name> name,
                             const MaybeObjectHandle& handler) {
  DCHECK(IsHandler(*handler));
  if (is_keyed() && state() != RECOMPUTE_HANDLER) {
    if (nexus()->GetName() != *name) return false;
  }
  Handle<Map> map = receiver_map();
  MapHandles maps;
  MaybeObjectHandles handlers;

  TargetMaps(&maps);
  int number_of_maps = static_cast<int>(maps.size());
  int deprecated_maps = 0;
  int handler_to_overwrite = -1;
  if (!nexus()->FindHandlers(&handlers, number_of_maps)) return false;

  for (int i = 0; i < number_of_maps; i++) {
    Handle<Map> current_map = maps.at(i);
    if (current_map->is_deprecated()) {
      // Filter out deprecated maps to ensure their instances get migrated.
      ++deprecated_maps;
    } else if (map.is_identical_to(current_map)) {
      // If both map and handler stayed the same (and the name is also the
      // same as checked above, for keyed accesses), we're not progressing
      // in the lattice and need to go MEGAMORPHIC instead. There's one
      // exception to this rule, which is when we're in RECOMPUTE_HANDLER
      // state, there we allow to migrate to a new handler.
      if (handler.is_identical_to(handlers[i]) &&
          state() != RECOMPUTE_HANDLER) {
        return false;
      }
      // If the receiver type is already in the polymorphic IC, this indicates
      // there was a prototoype chain failure. In that case, just overwrite the
      // handler.
      handler_to_overwrite = i;
    } else if (handler_to_overwrite == -1 &&
               IsTransitionOfMonomorphicTarget(*current_map, *map)) {
      handler_to_overwrite = i;
    }
  }

  int number_of_valid_maps =
      number_of_maps - deprecated_maps - (handler_to_overwrite != -1);

  if (number_of_valid_maps >= FLAG_max_polymorphic_map_count) return false;
  if (number_of_maps == 0 && state() != MONOMORPHIC && state() != POLYMORPHIC) {
    return false;
  }

  number_of_valid_maps++;
  if (number_of_valid_maps == 1) {
    ConfigureVectorState(name, receiver_map(), handler);
  } else {
    if (is_keyed() && nexus()->GetName() != *name) return false;
    if (handler_to_overwrite >= 0) {
      handlers[handler_to_overwrite] = handler;
      if (!map.is_identical_to(maps.at(handler_to_overwrite))) {
        maps[handler_to_overwrite] = map;
      }
    } else {
      maps.push_back(map);
      handlers.push_back(handler);
    }

    ConfigureVectorState(name, maps, &handlers);
  }

  return true;
}

void IC::UpdateMonomorphicIC(const MaybeObjectHandle& handler,
                             Handle<Name> name) {
  DCHECK(IsHandler(*handler));
  ConfigureVectorState(name, receiver_map(), handler);
}

void IC::CopyICToMegamorphicCache(Handle<Name> name) {
  MapHandles maps;
  MaybeObjectHandles handlers;
  TargetMaps(&maps);
  if (!nexus()->FindHandlers(&handlers, static_cast<int>(maps.size()))) return;
  for (int i = 0; i < static_cast<int>(maps.size()); i++) {
    UpdateMegamorphicCache(maps.at(i), name, handlers.at(i));
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
    transitioned_map =
        source_map.FindElementsKindTransitionedMap(isolate(), map_list);
  }
  return transitioned_map == target_map;
}

void IC::PatchCache(Handle<Name> name, Handle<Object> handler) {
  PatchCache(name, MaybeObjectHandle(handler));
}

void IC::PatchCache(Handle<Name> name, const MaybeObjectHandle& handler) {
  DCHECK(IsHandler(*handler));
  // Currently only load and store ICs support non-code handlers.
  DCHECK(IsAnyLoad() || IsAnyStore() || IsAnyHas());
  switch (state()) {
    case NO_FEEDBACK:
      break;
    case UNINITIALIZED:
    case PREMONOMORPHIC:
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
      if (!is_keyed() || state() == RECOMPUTE_HANDLER) {
        CopyICToMegamorphicCache(name);
      }
      ConfigureVectorState(MEGAMORPHIC, name);
      V8_FALLTHROUGH;
    case MEGAMORPHIC:
      UpdateMegamorphicCache(receiver_map(), name, handler);
      // Indicate that we've handled this case.
      vector_set_ = true;
      break;
    case GENERIC:
      UNREACHABLE();
  }
}

void LoadIC::UpdateCaches(LookupIterator* lookup) {
  if (!FLAG_lazy_feedback_allocation && state() == UNINITIALIZED &&
      !IsLoadGlobalIC()) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    TRACE_HANDLER_STATS(isolate(), LoadIC_Premonomorphic);
    ConfigureVectorState(receiver_map());
    TraceIC("LoadIC", lookup->name());
    return;
  }

  Handle<Object> code;
  if (lookup->state() == LookupIterator::ACCESS_CHECK) {
    code = slow_stub();
  } else if (!lookup->IsFound()) {
    TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonexistentDH);
    Handle<Smi> smi_handler = LoadHandler::LoadNonExistent(isolate());
    code = LoadHandler::LoadFullChain(
        isolate(), receiver_map(),
        MaybeObjectHandle(isolate()->factory()->null_value()), smi_handler);
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
    code = ComputeHandler(lookup);
  }

  PatchCache(lookup->name(), code);
  TraceIC("LoadIC", lookup->name());
}

StubCache* IC::stub_cache() {
  DCHECK(!IsAnyHas());
  if (IsAnyLoad()) {
    return isolate()->load_stub_cache();
  } else {
    DCHECK(IsAnyStore());
    return isolate()->store_stub_cache();
  }
}

void IC::UpdateMegamorphicCache(Handle<Map> map, Handle<Name> name,
                                const MaybeObjectHandle& handler) {
  if (!IsAnyHas()) {
    stub_cache()->Set(*name, *map, *handler);
  }
}

void IC::TraceHandlerCacheHitStats(LookupIterator* lookup) {
  DCHECK_EQ(LookupIterator::ACCESSOR, lookup->state());
  if (V8_LIKELY(!TracingFlags::is_runtime_stats_enabled())) return;
  if (IsAnyLoad() || IsAnyHas()) {
    TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Accessor);
  } else {
    DCHECK(IsAnyStore());
    TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Accessor);
  }
}

Handle<Object> LoadIC::ComputeHandler(LookupIterator* lookup) {
  Handle<Object> receiver = lookup->GetReceiver();
  ReadOnlyRoots roots(isolate());

  // `in` cannot be called on strings, and will always return true for string
  // wrapper length and function prototypes. The latter two cases are given
  // LoadHandler::LoadNativeDataProperty below.
  if (!IsAnyHas()) {
    if (receiver->IsString() && *lookup->name() == roots.length_string()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_StringLength);
      return BUILTIN_CODE(isolate(), LoadIC_StringLength);
    }

    if (receiver->IsStringWrapper() &&
        *lookup->name() == roots.length_string()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_StringWrapperLength);
      return BUILTIN_CODE(isolate(), LoadIC_StringWrapperLength);
    }

    // Use specialized code for getting prototype of functions.
    if (receiver->IsJSFunction() &&
        *lookup->name() == roots.prototype_string() &&
        !JSFunction::cast(*receiver).PrototypeRequiresRuntimeLookup()) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_FunctionPrototypeStub);
      return BUILTIN_CODE(isolate(), LoadIC_FunctionPrototype);
    }
  }

  Handle<Map> map = receiver_map();
  Handle<JSObject> holder;
  bool receiver_is_holder;
  if (lookup->state() != LookupIterator::JSPROXY) {
    holder = lookup->GetHolder<JSObject>();
    receiver_is_holder = receiver.is_identical_to(holder);
  }

  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR: {
      Handle<Smi> smi_handler = LoadHandler::LoadInterceptor(isolate());

      if (holder->GetNamedInterceptor().non_masking()) {
        MaybeObjectHandle holder_ref(isolate()->factory()->null_value());
        if (!receiver_is_holder || IsLoadGlobalIC()) {
          holder_ref = MaybeObjectHandle::Weak(holder);
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonMaskingInterceptorDH);
        return LoadHandler::LoadFullChain(isolate(), map, holder_ref,
                                          smi_handler);
      }

      if (receiver_is_holder) {
        DCHECK(map->has_named_interceptor());
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadInterceptorDH);
        return smi_handler;
      }

      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadInterceptorFromPrototypeDH);
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }

    case LookupIterator::ACCESSOR: {
      // Use simple field loads for some well-known callback properties.
      // The method will only return true for absolute truths based on the
      // receiver maps.
      FieldIndex index;
      if (Accessors::IsJSObjectFieldAccessor(isolate(), map, lookup->name(),
                                             &index)) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);
        return LoadHandler::LoadField(isolate(), index);
      }
      if (holder->IsJSModuleNamespace()) {
        Handle<ObjectHashTable> exports(
            Handle<JSModuleNamespace>::cast(holder)->module().exports(),
            isolate());
        int entry = exports->FindEntry(roots, lookup->name(),
                                       Smi::ToInt(lookup->name()->GetHash()));
        // We found the accessor, so the entry must exist.
        DCHECK_NE(entry, ObjectHashTable::kNotFound);
        int index = ObjectHashTable::EntryToValueIndex(entry);
        return LoadHandler::LoadModuleExport(isolate(), index);
      }

      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorPair()) {
        if (lookup->TryLookupCachedProperty()) {
          DCHECK_EQ(LookupIterator::DATA, lookup->state());
          return ComputeHandler(lookup);
        }

        Handle<Object> getter(AccessorPair::cast(*accessors).getter(),
                              isolate());
        if (!getter->IsJSFunction() && !getter->IsFunctionTemplateInfo()) {
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return slow_stub();
        }

        if ((getter->IsFunctionTemplateInfo() &&
             FunctionTemplateInfo::cast(*getter).BreakAtEntry()) ||
            (getter->IsJSFunction() &&
             JSFunction::cast(*getter).shared().BreakAtEntry())) {
          // Do not install an IC if the api function has a breakpoint.
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return slow_stub();
        }

        Handle<Smi> smi_handler;

        CallOptimization call_optimization(isolate(), getter);
        if (call_optimization.is_simple_api_call()) {
          if (!call_optimization.IsCompatibleReceiverMap(map, holder) ||
              !holder->HasFastProperties()) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return slow_stub();
          }

          CallOptimization::HolderLookup holder_lookup;
          call_optimization.LookupHolderOfExpectedType(map, &holder_lookup);

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
          if (receiver_is_holder) return smi_handler;
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
          if (receiver_is_holder) return smi_handler;
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalFromPrototypeDH);
        }

        return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                              smi_handler);
      }

      Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);

      if (v8::ToCData<Address>(info->getter()) == kNullAddress ||
          !AccessorInfo::IsCompatibleReceiverMap(info, map) ||
          !holder->HasFastProperties() ||
          (info->is_sloppy() && !receiver->IsJSReceiver())) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
        return slow_stub();
      }

      Handle<Smi> smi_handler = LoadHandler::LoadNativeDataProperty(
          isolate(), lookup->GetAccessorIndex());
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNativeDataPropertyDH);
      if (receiver_is_holder) return smi_handler;
      TRACE_HANDLER_STATS(isolate(),
                          LoadIC_LoadNativeDataPropertyFromPrototypeDH);
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }

    case LookupIterator::DATA: {
      DCHECK_EQ(kData, lookup->property_details().kind());
      Handle<Smi> smi_handler;
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject()) {
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
        if (receiver_is_holder) return smi_handler;
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormalFromPrototypeDH);

      } else {
        DCHECK_EQ(kField, lookup->property_details().location());
        FieldIndex field = lookup->GetFieldIndex();
        smi_handler = LoadHandler::LoadField(isolate(), field);
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldDH);
        if (receiver_is_holder) return smi_handler;
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldFromPrototypeDH);
      }
      return LoadHandler::LoadFromPrototype(isolate(), map, holder,
                                            smi_handler);
    }
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadIntegerIndexedExoticDH);
      return LoadHandler::LoadNonExistent(isolate());
    case LookupIterator::JSPROXY: {
      Handle<JSProxy> holder_proxy = lookup->GetHolder<JSProxy>();
      bool receiver_is_holder_proxy = receiver.is_identical_to(holder_proxy);
      Handle<Smi> smi_handler = LoadHandler::LoadProxy(isolate());
      if (receiver_is_holder_proxy) {
        return smi_handler;
      }
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

static Handle<Object> TryConvertKey(Handle<Object> key, Isolate* isolate) {
  // This helper implements a few common fast cases for converting
  // non-smi keys of keyed loads/stores to a smi or a string.
  if (key->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(key)->value();
    if (std::isnan(value)) {
      key = isolate->factory()->NaN_string();
    } else {
      // Check bounds first to avoid undefined behavior in the conversion
      // to int.
      if (value <= Smi::kMaxValue && value >= Smi::kMinValue) {
        int int_value = FastD2I(value);
        if (value == int_value) {
          key = handle(Smi::FromInt(int_value), isolate);
        }
      }
    }
  } else if (key->IsString()) {
    key = isolate->factory()->InternalizeString(Handle<String>::cast(key));
  }
  return key;
}

bool KeyedLoadIC::CanChangeToAllowOutOfBounds(Handle<Map> receiver_map) {
  const MaybeObjectHandle& handler = nexus()->FindHandlerForMap(receiver_map);
  if (handler.is_null()) return false;
  return LoadHandler::GetKeyedAccessLoadMode(*handler) == STANDARD_LOAD;
}

void KeyedLoadIC::UpdateLoadElement(Handle<HeapObject> receiver,
                                    KeyedAccessLoadMode load_mode) {
  Handle<Map> receiver_map(receiver->map(), isolate());
  DCHECK(receiver_map->instance_type() != JS_VALUE_TYPE);  // Checked by caller.
  MapHandles target_receiver_maps;
  TargetMaps(&target_receiver_maps);

  if (target_receiver_maps.empty()) {
    Handle<Object> handler = LoadElementHandler(receiver_map, load_mode);
    return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
  }

  for (Handle<Map> map : target_receiver_maps) {
    if (map.is_null()) continue;
    if (map->instance_type() == JS_VALUE_TYPE) {
      set_slow_stub_reason("JSValue");
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
  if (state() == MONOMORPHIC && !receiver->IsString() &&
      !receiver->IsJSProxy() &&
      IsMoreGeneralElementsKindTransition(
          target_receiver_maps.at(0)->elements_kind(),
          Handle<JSObject>::cast(receiver)->GetElementsKind())) {
    Handle<Object> handler = LoadElementHandler(receiver_map, load_mode);
    return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
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
  if (target_receiver_maps.size() > kMaxKeyedPolymorphism) {
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
  if (isolate->IsNoElementsProtectorIntact()) {
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
    return IsAnyHas() ? BUILTIN_CODE(isolate(), HasIndexedInterceptorIC)
                      : BUILTIN_CODE(isolate(), LoadIndexedInterceptorIC);
  }

  InstanceType instance_type = receiver_map->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadIndexedStringDH);
    if (IsAnyHas()) return BUILTIN_CODE(isolate(), HasIC_Slow);
    return LoadHandler::LoadIndexedString(isolate(), load_mode);
  }
  if (instance_type < FIRST_JS_RECEIVER_TYPE) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_SlowStub);
    return IsAnyHas() ? BUILTIN_CODE(isolate(), HasIC_Slow)
                      : BUILTIN_CODE(isolate(), KeyedLoadIC_Slow);
  }
  if (instance_type == JS_PROXY_TYPE) {
    return LoadHandler::LoadProxy(isolate());
  }

  ElementsKind elements_kind = receiver_map->elements_kind();
  if (IsSloppyArgumentsElementsKind(elements_kind)) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_KeyedLoadSloppyArgumentsStub);
    return IsAnyHas() ? BUILTIN_CODE(isolate(), KeyedHasIC_SloppyArguments)
                      : BUILTIN_CODE(isolate(), KeyedLoadIC_SloppyArguments);
  }
  bool is_js_array = instance_type == JS_ARRAY_TYPE;
  if (elements_kind == DICTIONARY_ELEMENTS) {
    TRACE_HANDLER_STATS(isolate(), KeyedLoadIC_LoadElementDH);
    return LoadHandler::LoadElement(isolate(), elements_kind, false,
                                    is_js_array, load_mode);
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsFrozenOrSealedElementsKind(elements_kind) ||
         IsTypedArrayElementsKind(elements_kind));
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
      Map tmap = receiver_map->FindElementsKindTransitionedMap(isolate(),
                                                               *receiver_maps);
      if (!tmap.is_null()) {
        receiver_map->NotifyLeafMapLayoutChange(isolate());
      }
    }
    handlers->push_back(
        MaybeObjectHandle(LoadElementHandler(receiver_map, load_mode)));
  }
}

namespace {

bool ConvertKeyToIndex(Handle<Object> receiver, Handle<Object> key,
                       uint32_t* index, InlineCacheState state) {
  if (!FLAG_use_ic || state == NO_FEEDBACK) return false;
  if (receiver->IsAccessCheckNeeded() || receiver->IsJSValue()) return false;

  // For regular JSReceiver or String receivers, the {key} must be a positive
  // array index.
  if (receiver->IsJSReceiver() || receiver->IsString()) {
    if (key->ToArrayIndex(index)) return true;
  }
  // For JSTypedArray receivers, we can also support negative keys, which we
  // just map into the [2**31, 2**32 - 1] range via a bit_cast. This is valid
  // because JSTypedArray::length is always a Smi, so such keys will always
  // be detected as OOB.
  if (receiver->IsJSTypedArray()) {
    int32_t signed_index;
    if (key->ToInt32(&signed_index)) {
      *index = bit_cast<uint32_t>(signed_index);
      return true;
    }
  }
  return false;
}

bool IsOutOfBoundsAccess(Handle<Object> receiver, uint32_t index) {
  size_t length;
  if (receiver->IsJSArray()) {
    length = JSArray::cast(*receiver).length().Number();
  } else if (receiver->IsJSTypedArray()) {
    length = JSTypedArray::cast(*receiver).length();
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
                                uint32_t index) {
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
  if (MigrateDeprecated(object)) {
    return RuntimeLoad(object, key);
  }

  Handle<Object> load_handle;

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  uint32_t index;
  if ((key->IsInternalizedString() &&
       !String::cast(*key).AsArrayIndex(&index)) ||
      key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), load_handle,
                               LoadIC::Load(object, Handle<Name>::cast(key)),
                               Object);
  } else if (ConvertKeyToIndex(object, key, &index, state())) {
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
          if (it->HolderIsReceiverOrHiddenPrototype()) {
            return !info.non_masking() && receiver.is_identical_to(holder) &&
                   !info.setter().IsUndefined(isolate());
          } else if (!info.getter().IsUndefined(isolate()) ||
                     !info.query().IsUndefined(isolate())) {
            return false;
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
            update_receiver_map(receiver);
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

  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(isolate(), *script_contexts, *str_name,
                                 &lookup_result)) {
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
      return ReferenceError(name);
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
        PatchCache(name, slow_stub());
      }
      TraceIC("StoreGlobalIC", name);
    }

    script_context->set(lookup_result.slot_index, *value);
    return value;
  }

  return StoreIC::Store(global, name, value);
}

MaybeHandle<Object> StoreIC::Store(Handle<Object> object, Handle<Name> name,
                                   Handle<Object> value,
                                   StoreOrigin store_origin) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Object::SetProperty(isolate(), object, name, value),
        Object);
    return result;
  }

  bool use_ic = (state() != NO_FEEDBACK) && FLAG_use_ic;
  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsNullOrUndefined(isolate())) {
    if (use_ic && state() != PREMONOMORPHIC) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), StoreIC_NonReceiver);
      update_receiver_map(object);
      PatchCache(name, slow_stub());
      TraceIC("StoreIC", name);
    }
    return TypeError(MessageTemplate::kNonObjectPropertyStore, object, name);
  }

  if (state() != UNINITIALIZED) {
    JSObject::MakePrototypesFast(object, kStartAtPrototype, isolate());
  }
  LookupIterator it(isolate(), object, name);

  if (name->IsPrivate()) {
    if (name->IsPrivateName() && !it.IsFound()) {
      Handle<String> name_string(String::cast(Symbol::cast(*name).name()),
                                 isolate());
      return TypeError(MessageTemplate::kInvalidPrivateFieldWrite, object,
                       name_string);
    }

    // IC handling of private fields/symbols stores on JSProxy is not
    // supported.
    if (object->IsJSProxy()) {
      use_ic = false;
    }
  }
  if (use_ic) UpdateCaches(&it, value, store_origin);

  MAYBE_RETURN_NULL(Object::SetProperty(&it, value, store_origin));
  return value;
}

void StoreIC::UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                           StoreOrigin store_origin) {
  if (state() == UNINITIALIZED && !IsStoreGlobalIC()) {
    // This is the first time we execute this inline cache. Transition
    // to premonomorphic state to delay setting the monomorphic state.
    TRACE_HANDLER_STATS(isolate(), StoreIC_Premonomorphic);
    ConfigureVectorState(receiver_map());
    TraceIC("StoreIC", lookup->name());
    return;
  }

  MaybeObjectHandle handler;
  if (LookupForWrite(lookup, value, store_origin)) {
    if (IsStoreGlobalIC()) {
      if (lookup->state() == LookupIterator::DATA &&
          lookup->GetReceiver().is_identical_to(lookup->GetHolder<Object>())) {
        DCHECK(lookup->GetReceiver()->IsJSGlobalObject());
        // Now update the cell in the feedback vector.
        nexus()->ConfigurePropertyCellMode(lookup->GetPropertyCell());
        TraceIC("StoreGlobalIC", lookup->name());
        return;
      }
    }
    handler = ComputeHandler(lookup);
  } else {
    if (state() == UNINITIALIZED && IsStoreGlobalIC() &&
        lookup->state() == LookupIterator::INTERCEPTOR) {
      InterceptorInfo info =
          lookup->GetHolder<JSObject>()->GetNamedInterceptor();
      if (!lookup->HolderIsReceiverOrHiddenPrototype() &&
          !info.getter().IsUndefined(isolate())) {
        // Utilize premonomorphic state for global store ics that run into
        // an interceptor because the property doesn't exist yet.
        // After we actually set the property, we'll have more information.
        // Premonomorphism gives us a chance to find more information the
        // second time.
        TRACE_HANDLER_STATS(isolate(), StoreGlobalIC_Premonomorphic);
        ConfigureVectorState(receiver_map());
        TraceIC("StoreGlobalIC", lookup->name());
        return;
      }
    }

    set_slow_stub_reason("LookupForWrite said 'false'");
    // TODO(marja): change slow_stub to return MaybeObjectHandle.
    handler = MaybeObjectHandle(slow_stub());
  }

  PatchCache(lookup->name(), handler);
  TraceIC("StoreIC", lookup->name());
}

MaybeObjectHandle StoreIC::ComputeHandler(LookupIterator* lookup) {
  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      Handle<JSObject> store_target = lookup->GetStoreTarget<JSObject>();
      if (store_target->IsJSGlobalObject()) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobalTransitionDH);

        if (receiver_map()->IsJSGlobalObject()) {
          DCHECK(IsStoreGlobalIC());
#ifdef DEBUG
          Handle<JSObject> holder = lookup->GetHolder<JSObject>();
          DCHECK_EQ(*lookup->GetReceiver(), *holder);
          DCHECK_EQ(*store_target, *holder);
#endif
          return StoreHandler::StoreGlobal(lookup->transition_cell());
        }

        Handle<Smi> smi_handler = StoreHandler::StoreGlobalProxy(isolate());
        Handle<Object> handler = StoreHandler::StoreThroughPrototype(
            isolate(), receiver_map(), store_target, smi_handler,
            MaybeObjectHandle::Weak(lookup->transition_cell()));
        return MaybeObjectHandle(handler);
      }
      // Dictionary-to-fast transitions are not expected and not supported.
      DCHECK_IMPLIES(!lookup->transition_map()->is_dictionary_map(),
                     !receiver_map()->is_dictionary_map());

      DCHECK(lookup->IsCacheableTransition());

      return StoreHandler::StoreTransition(isolate(), lookup->transition_map());
    }

    case LookupIterator::INTERCEPTOR: {
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      USE(holder);

      DCHECK(!holder->GetNamedInterceptor().setter().IsUndefined(isolate()));
      // TODO(jgruber): Update counter name.
      TRACE_HANDLER_STATS(isolate(), StoreIC_StoreInterceptorStub);
      return MaybeObjectHandle(BUILTIN_CODE(isolate(), StoreInterceptorIC));
    }

    case LookupIterator::ACCESSOR: {
      // This is currently guaranteed by checks in StoreIC::Store.
      Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

      if (!holder->HasFastProperties()) {
        set_slow_stub_reason("accessor on slow map");
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        return MaybeObjectHandle(slow_stub());
      }
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->setter()) == kNullAddress) {
          set_slow_stub_reason("setter == kNullAddress");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        }
        if (AccessorInfo::cast(*accessors).is_special_data_property() &&
            !lookup->HolderIsReceiverOrHiddenPrototype()) {
          set_slow_stub_reason("special data property in prototype chain");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        }
        if (!AccessorInfo::IsCompatibleReceiverMap(info, receiver_map())) {
          set_slow_stub_reason("incompatible receiver type");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
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
            isolate(), receiver_map(), holder, smi_handler));

      } else if (accessors->IsAccessorPair()) {
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        if (!setter->IsJSFunction() && !setter->IsFunctionTemplateInfo()) {
          set_slow_stub_reason("setter not a function");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        }

        if ((setter->IsFunctionTemplateInfo() &&
             FunctionTemplateInfo::cast(*setter).BreakAtEntry()) ||
            (setter->IsJSFunction() &&
             JSFunction::cast(*setter).shared().BreakAtEntry())) {
          // Do not install an IC if the api function has a breakpoint.
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        }

        CallOptimization call_optimization(isolate(), setter);
        if (call_optimization.is_simple_api_call()) {
          if (call_optimization.IsCompatibleReceiver(receiver, holder)) {
            CallOptimization::HolderLookup holder_lookup;
            call_optimization.LookupHolderOfExpectedType(receiver_map(),
                                                         &holder_lookup);

            Handle<Smi> smi_handler = StoreHandler::StoreApiSetter(
                isolate(),
                holder_lookup == CallOptimization::kHolderIsReceiver);

            Handle<Context> context(
                call_optimization.GetAccessorContext(holder->map()), isolate());
            TRACE_HANDLER_STATS(isolate(), StoreIC_StoreApiSetterOnPrototypeDH);
            return MaybeObjectHandle(StoreHandler::StoreThroughPrototype(
                isolate(), receiver_map(), holder, smi_handler,
                MaybeObjectHandle::Weak(call_optimization.api_call_info()),
                MaybeObjectHandle::Weak(context)));
          }
          set_slow_stub_reason("incompatible receiver");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        } else if (setter->IsFunctionTemplateInfo()) {
          set_slow_stub_reason("setter non-simple template");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return MaybeObjectHandle(slow_stub());
        }

        Handle<Smi> smi_handler =
            StoreHandler::StoreAccessor(isolate(), lookup->GetAccessorIndex());

        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreAccessorDH);
        if (receiver.is_identical_to(holder)) {
          return MaybeObjectHandle(smi_handler);
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreAccessorOnPrototypeDH);

        return MaybeObjectHandle(StoreHandler::StoreThroughPrototype(
            isolate(), receiver_map(), holder, smi_handler));
      }
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return MaybeObjectHandle(slow_stub());
    }

    case LookupIterator::DATA: {
      // This is currently guaranteed by checks in StoreIC::Store.
      Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
      USE(receiver);
      Handle<JSObject> holder = lookup->GetHolder<JSObject>();
      DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

      DCHECK_EQ(kData, lookup->property_details().kind());
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject()) {
          TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobalDH);
          return MaybeObjectHandle(
              StoreHandler::StoreGlobal(lookup->GetPropertyCell()));
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreNormalDH);
        DCHECK(holder.is_identical_to(receiver));
        return MaybeObjectHandle(StoreHandler::StoreNormal(isolate()));
      }

      // -------------- Fields --------------
      if (lookup->property_details().location() == kField) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreFieldDH);
        int descriptor = lookup->GetFieldDescriptorIndex();
        FieldIndex index = lookup->GetFieldIndex();
        PropertyConstness constness = lookup->constness();
        if (constness == PropertyConstness::kConst &&
            IsStoreOwnICKind(nexus()->kind())) {
          // StoreOwnICs are used for initializing object literals therefore
          // we must store the value unconditionally even to
          // VariableMode::kConst fields.
          constness = PropertyConstness::kMutable;
        }
        return MaybeObjectHandle(StoreHandler::StoreField(
            isolate(), descriptor, index, constness, lookup->representation()));
      }

      // -------------- Constant properties --------------
      DCHECK_EQ(kDescriptor, lookup->property_details().location());
      set_slow_stub_reason("constant property");
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return MaybeObjectHandle(slow_stub());
    }
    case LookupIterator::JSPROXY: {
      Handle<JSReceiver> receiver =
          Handle<JSReceiver>::cast(lookup->GetReceiver());
      Handle<JSProxy> holder = lookup->GetHolder<JSProxy>();
      return MaybeObjectHandle(StoreHandler::StoreProxy(
          isolate(), receiver_map(), holder, receiver));
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
  MapHandles target_receiver_maps;
  TargetMaps(&target_receiver_maps);
  if (target_receiver_maps.empty()) {
    Handle<Map> monomorphic_map = receiver_map;
    // If we transitioned to a map that is a more general map than incoming
    // then use the new map.
    if (IsTransitionOfMonomorphicTarget(*receiver_map, *new_receiver_map)) {
      monomorphic_map = new_receiver_map;
    }
    Handle<Object> handler = StoreElementHandler(monomorphic_map, store_mode);
    return ConfigureVectorState(Handle<Name>(), monomorphic_map, handler);
  }

  for (Handle<Map> map : target_receiver_maps) {
    if (!map.is_null() && map->instance_type() == JS_VALUE_TYPE) {
      DCHECK(!IsStoreInArrayLiteralICKind(kind()));
      set_slow_stub_reason("JSValue");
      return;
    }
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different IC that handles a superset of the original IC.
  // Handle those here if the receiver map hasn't changed or it has transitioned
  // to a more general kind.
  KeyedAccessStoreMode old_store_mode = GetKeyedAccessStoreMode();
  Handle<Map> previous_receiver_map = target_receiver_maps.at(0);
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
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      Handle<Object> handler = StoreElementHandler(receiver_map, store_mode);
      return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
    }
  }

  DCHECK(state() != GENERIC);

  bool map_added =
      AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map);

  if (IsTransitionOfMonomorphicTarget(*receiver_map, *new_receiver_map)) {
    map_added |=
        AddOneReceiverMapIfMissing(&target_receiver_maps, new_receiver_map);
  }

  if (!map_added) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the megamorphic stub which can handle everything.
    set_slow_stub_reason("same map added twice");
    return;
  }

  // If the maximum number of receiver maps has been exceeded, use the
  // megamorphic version of the IC.
  if (target_receiver_maps.size() > kMaxKeyedPolymorphism) return;

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
  // receivers are either external arrays, or all "normal" arrays. Otherwise,
  // use the megamorphic stub.
  if (store_mode != STANDARD_STORE) {
    size_t external_arrays = 0;
    for (Handle<Map> map : target_receiver_maps) {
      if (map->has_typed_array_elements()) {
        DCHECK(!IsStoreInArrayLiteralICKind(kind()));
        external_arrays++;
      }
    }
    if (external_arrays != 0 &&
        external_arrays != target_receiver_maps.size()) {
      DCHECK(!IsStoreInArrayLiteralICKind(kind()));
      set_slow_stub_reason(
          "unsupported combination of external and normal arrays");
      return;
    }
  }

  MaybeObjectHandles handlers;
  handlers.reserve(target_receiver_maps.size());
  StoreElementPolymorphicHandlers(&target_receiver_maps, &handlers, store_mode);
  if (target_receiver_maps.size() == 0) {
    // Transition to PREMONOMORPHIC state here and remember a weak-reference
    // to the {receiver_map} in case TurboFan sees this function before the
    // IC can transition further.
    ConfigureVectorState(receiver_map);
  } else if (target_receiver_maps.size() == 1) {
    ConfigureVectorState(Handle<Name>(), target_receiver_maps[0], handlers[0]);
  } else {
    ConfigureVectorState(Handle<Name>(), target_receiver_maps, &handlers);
  }
}

Handle<Object> KeyedStoreIC::StoreElementHandler(
    Handle<Map> receiver_map, KeyedAccessStoreMode store_mode) {
  DCHECK_IMPLIES(
      receiver_map->DictionaryElementsInPrototypeChainOnly(isolate()),
      IsStoreInArrayLiteralICKind(kind()));

  if (receiver_map->IsJSProxyMap()) {
    return StoreHandler::StoreProxy(isolate());
  }

  // TODO(ishell): move to StoreHandler::StoreElement().
  Handle<Code> code;
  if (receiver_map->has_sloppy_arguments_elements()) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_KeyedStoreSloppyArgumentsStub);
    code =
        CodeFactory::KeyedStoreIC_SloppyArguments(isolate(), store_mode).code();
  } else if (receiver_map->has_fast_elements() ||
             receiver_map->has_sealed_elements() ||
             receiver_map->has_typed_array_elements()) {
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreFastElementStub);
    code = CodeFactory::StoreFastElementIC(isolate(), store_mode).code();
    if (receiver_map->has_typed_array_elements()) return code;
  } else if (IsStoreInArrayLiteralICKind(kind())) {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), StoreInArrayLiteralIC_SlowStub);
    code =
        CodeFactory::StoreInArrayLiteralIC_Slow(isolate(), store_mode).code();
  } else {
    // TODO(jgruber): Update counter name.
    TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_StoreElementStub);
    DCHECK(DICTIONARY_ELEMENTS == receiver_map->elements_kind() ||
           receiver_map->has_frozen_elements());
    code = CodeFactory::KeyedStoreIC_Slow(isolate(), store_mode).code();
  }

  if (IsStoreInArrayLiteralICKind(kind())) return code;

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
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
    MapHandles* receiver_maps, MaybeObjectHandles* handlers,
    KeyedAccessStoreMode store_mode) {
  // Filter out deprecated maps to ensure their instances get migrated.
  receiver_maps->erase(
      std::remove_if(
          receiver_maps->begin(), receiver_maps->end(),
          [](const Handle<Map>& map) { return map->is_deprecated(); }),
      receiver_maps->end());

  for (Handle<Map> receiver_map : *receiver_maps) {
    Handle<Object> handler;
    Handle<Map> transition;

    if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE ||
        receiver_map->DictionaryElementsInPrototypeChainOnly(isolate())) {
      // TODO(mvstanton): Consider embedding store_mode in the state of the slow
      // keyed store ic for uniformity.
      TRACE_HANDLER_STATS(isolate(), KeyedStoreIC_SlowStub);
      handler = slow_stub();

    } else {
      {
        Map tmap = receiver_map->FindElementsKindTransitionedMap(
            isolate(), *receiver_maps);
        if (!tmap.is_null()) {
          if (receiver_map->is_stable()) {
            receiver_map->NotifyLeafMapLayoutChange(isolate());
          }
          transition = handle(tmap, isolate());
        }
      }

      // TODO(mvstanton): The code below is doing pessimistic elements
      // transitions. I would like to stop doing that and rely on Allocation
      // Site Tracking to do a better job of ensuring the data types are what
      // they need to be. Not all the elements are in place yet, pessimistic
      // elements transitions are still important for performance.
      if (!transition.is_null()) {
        TRACE_HANDLER_STATS(isolate(),
                            KeyedStoreIC_ElementsTransitionAndStoreStub);
        handler = StoreHandler::StoreElementTransition(isolate(), receiver_map,
                                                       transition, store_mode);
      } else {
        handler = StoreElementHandler(receiver_map, store_mode);
      }
    }
    DCHECK(!handler.is_null());
    handlers->push_back(MaybeObjectHandle(handler));
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

KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver, uint32_t index) {
  bool oob_access = IsOutOfBoundsAccess(receiver, index);
  // Don't consider this a growing store if the store would send the receiver to
  // dictionary mode. Also make sure we don't consider this a growing store if
  // there's any JSTypedArray in the {receiver}'s prototype chain, since that
  // prototype is going to swallow all stores that are out-of-bounds for said
  // prototype, and we just let the runtime deal with the complexity of this.
  bool allow_growth = receiver->IsJSArray() && oob_access &&
                      !receiver->WouldConvertToSlowElements(index) &&
                      !MayHaveTypedArrayInPrototypeChain(receiver);
  if (allow_growth) {
    return STORE_AND_GROW_HANDLE_COW;
  }
  if (receiver->map().has_typed_array_elements() && oob_access) {
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
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Runtime::SetObjectProperty(isolate(), object, key, value,
                                   StoreOrigin::kMaybeKeyed),
        Object);
    return result;
  }

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  Handle<Object> store_handle;

  uint32_t index;
  if ((key->IsInternalizedString() &&
       !String::cast(*key).AsArrayIndex(&index)) ||
      key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), store_handle,
                               StoreIC::Store(object, Handle<Name>::cast(key),
                                              value, StoreOrigin::kMaybeKeyed),
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
  bool key_is_valid_index = false;
  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (use_ic && object->IsJSReceiver()) {
    Handle<JSReceiver> receiver = Handle<JSReceiver>::cast(object);
    old_receiver_map = handle(receiver->map(), isolate());
    is_arguments = receiver->IsJSArgumentsObject();
    bool is_proxy = receiver->IsJSProxy();
    // For JSTypedArray {object}s we can handle negative indices as OOB
    // accesses, since integer indexed properties are never looked up
    // on the prototype chain. For this we simply map the negative {key}s
    // to the [2**31,2**32-1] range, which is safe since JSTypedArray::length
    // is always an unsigned Smi.
    key_is_valid_index =
        key->IsSmi() && (Smi::ToInt(*key) >= 0 || object->IsJSTypedArray());
    if (!is_arguments && !is_proxy) {
      if (key_is_valid_index) {
        uint32_t index = static_cast<uint32_t>(Smi::ToInt(*key));
        Handle<JSObject> receiver_object = Handle<JSObject>::cast(object);
        store_mode = GetStoreMode(receiver_object, index);
      }
    }
  }

  DCHECK(store_handle.is_null());
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), store_handle,
      Runtime::SetObjectProperty(isolate(), object, key, value,
                                 StoreOrigin::kMaybeKeyed),
      Object);

  if (use_ic) {
    if (!old_receiver_map.is_null()) {
      if (is_arguments) {
        set_slow_stub_reason("arguments receiver");
      } else if (key_is_valid_index) {
        if (old_receiver_map->is_abandoned_prototype_map()) {
          set_slow_stub_reason("receiver with prototype map");
        } else if (!old_receiver_map->DictionaryElementsInPrototypeChainOnly(
                       isolate())) {
          // We should go generic if receiver isn't a dictionary, but our
          // prototype chain does have dictionary elements. This ensures that
          // other non-dictionary receivers in the polymorphic case benefit
          // from fast path keyed stores.
          Handle<HeapObject> receiver = Handle<HeapObject>::cast(object);
          UpdateStoreElement(old_receiver_map, store_mode,
                             handle(receiver->map(), isolate()));
        } else {
          set_slow_stub_reason("dictionary or proxy prototype");
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
void StoreOwnElement(Isolate* isolate, Handle<JSArray> array,
                     Handle<Object> index, Handle<Object> value) {
  DCHECK(index->IsNumber());
  bool success = false;
  LookupIterator it = LookupIterator::PropertyOrElement(
      isolate, array, index, &success, LookupIterator::OWN);
  DCHECK(success);

  CHECK(JSObject::DefineOwnPropertyIgnoreAttributes(
            &it, value, NONE, Just(ShouldThrow::kThrowOnError))
            .FromJust());
}
}  // namespace

void StoreInArrayLiteralIC::Store(Handle<JSArray> array, Handle<Object> index,
                                  Handle<Object> value) {
  DCHECK(!array->map().IsMapInArrayPrototypeChain(isolate()));
  DCHECK(index->IsNumber());

  if (!FLAG_use_ic || state() == NO_FEEDBACK || MigrateDeprecated(array)) {
    StoreOwnElement(isolate(), array, index, value);
    TraceIC("StoreInArrayLiteralIC", index);
    return;
  }

  // TODO(neis): Convert HeapNumber to Smi if possible?

  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (index->IsSmi()) {
    DCHECK_GE(Smi::ToInt(*index), 0);
    uint32_t index32 = static_cast<uint32_t>(Smi::ToInt(*index));
    store_mode = GetStoreMode(array, index32);
  }

  Handle<Map> old_array_map(array->map(), isolate());
  StoreOwnElement(isolate(), array, index, value);

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
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(3);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  // A monomorphic or polymorphic KeyedLoadIC with a string key can call the
  // LoadIC miss handler if the handler misses. Since the vector Nexus is
  // set up outside the IC, handle that here.
  // The only case where we call without a vector is from the LoadNamedProperty
  // bytecode handler. Also, when there is no feedback vector, there is no
  // difference between LoadProperty or LoadKeyed kind.
  FeedbackSlotKind kind = FeedbackSlotKind::kLoadProperty;
  if (!vector.is_null()) {
    kind = vector->GetKind(vector_slot);
  }
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

RUNTIME_FUNCTION(Runtime_LoadGlobalIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<String> name = args.at<String>(0);
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  CONVERT_INT32_ARG_CHECKED(typeof_value, 3);
  TypeofMode typeof_mode = static_cast<TypeofMode>(typeof_value);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }

  FeedbackSlotKind kind = (typeof_mode == TypeofMode::INSIDE_TYPEOF)
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
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);

  Handle<Context> native_context = isolate->native_context();
  Handle<ScriptContextTable> script_contexts(
      native_context->script_context_table(), isolate);

  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(isolate, *script_contexts, *name,
                                 &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        isolate, script_contexts, lookup_result.context_index);
    Handle<Object> result(script_context->get(lookup_result.slot_index),
                          isolate);
    if (*result == ReadOnlyRoots(isolate).the_hole_value()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
    }
    return *result;
  }

  Handle<JSGlobalObject> global(native_context->global_object(), isolate);
  Handle<Object> result;
  bool is_found = false;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::GetObjectProperty(isolate, global, name, &is_found));
  if (!is_found) {
    Handle<Smi> slot = args.at<Smi>(1);
    Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
    FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());
    FeedbackSlotKind kind = vector->GetKind(vector_slot);
    // It is actually a LoadGlobalICs here but the predicate handles this case
    // properly.
    if (LoadIC::ShouldThrowReferenceError(kind)) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
    }
  }
  return *result;
}

RUNTIME_FUNCTION(Runtime_KeyedLoadIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Smi> slot = args.at<Smi>(2);
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
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Name> key = args.at<Name>(4);

  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // When there is no feedback vector it is OK to use the StoreNamedStrict as
  // the feedback slot kind. We only need if it is StoreOwnICKind when
  // installing the handler for storing const properties. This will happen only
  // when feedback vector is available.
  FeedbackSlotKind kind = FeedbackSlotKind::kStoreNamedStrict;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
  }

  DCHECK(IsStoreICKind(kind) || IsStoreOwnICKind(kind));
  StoreIC ic(isolate, vector, vector_slot, kind);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
}

RUNTIME_FUNCTION(Runtime_StoreGlobalIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Smi> slot = args.at<Smi>(1);
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

  // TODO(mythria): Replace StoreGlobalStrict/Sloppy with StoreNamed.
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
  CONVERT_ARG_HANDLE_CHECKED(String, name, 4);

#ifdef DEBUG
  {
    Handle<Smi> slot = args.at<Smi>(1);
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

  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(isolate, *script_contexts, *name,
                                 &lookup_result)) {
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
          isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
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
  Handle<Smi> slot = args.at<Smi>(1);
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
  FeedbackSlotKind kind = FeedbackSlotKind::kStoreKeyedStrict;
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
    kind = vector->GetKind(vector_slot);
  }

  // The elements store stubs miss into this function, but they are shared by
  // different ICs.
  if (IsKeyedStoreICKind(kind)) {
    KeyedStoreIC ic(isolate, vector, vector_slot, kind);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
  } else {
    DCHECK(IsStoreInArrayLiteralICKind(kind));
    DCHECK(receiver->IsJSArray());
    DCHECK(key->IsNumber());
    StoreInArrayLiteralIC ic(isolate, vector, vector_slot);
    ic.UpdateState(receiver, key);
    ic.Store(Handle<JSArray>::cast(receiver), key, value);
    return *value;
  }
}

RUNTIME_FUNCTION(Runtime_StoreInArrayLiteralIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Smi> slot = args.at<Smi>(1);
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
  ic.Store(Handle<JSArray>::cast(receiver), key, value);
  return *value;
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
  Handle<Smi> slot = args.at<Smi>(4);
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
    DCHECK(IsKeyedStoreICKind(kind) || IsStoreICKind(kind));
    RETURN_RESULT_OR_FAILURE(
        isolate, Runtime::SetObjectProperty(isolate, object, key, value,
                                            StoreOrigin::kMaybeKeyed));
  }
}

static bool CanFastCloneObject(Handle<Map> map) {
  DisallowHeapAllocation no_gc;
  if (map->IsNullOrUndefinedMap()) return true;
  if (!map->IsJSObjectMap() ||
      !IsSmiOrObjectElementsKind(map->elements_kind()) ||
      !map->OnlyHasSimpleProperties()) {
    return false;
  }

  DescriptorArray descriptors = map->instance_descriptors();
  for (int i = 0; i < map->NumberOfOwnDescriptors(); i++) {
    PropertyDetails details = descriptors.GetDetails(i);
    Name key = descriptors.GetKey(i);
    if (details.kind() != kData || !details.IsEnumerable() ||
        key.IsPrivateName()) {
      return false;
    }
  }

  return true;
}

static Handle<Map> FastCloneObjectMap(Isolate* isolate,
                                      Handle<HeapObject> source, int flags) {
  Handle<Map> source_map(source->map(), isolate);
  SLOW_DCHECK(source->IsNullOrUndefined() || CanFastCloneObject(source_map));
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

  if (source->IsNullOrUndefined() || !source_map->NumberOfOwnDescriptors()) {
    return map;
  }

  if (map.is_identical_to(initial_map)) {
    map = Map::Copy(isolate, map, "InitializeClonedDescriptors");
  }

  Handle<DescriptorArray> source_descriptors(source_map->instance_descriptors(),
                                             isolate);
  int size = source_map->NumberOfOwnDescriptors();
  int slack = 0;
  Handle<DescriptorArray> descriptors = DescriptorArray::CopyForFastObjectClone(
      isolate, source_descriptors, size, slack);
  Handle<LayoutDescriptor> layout =
      LayoutDescriptor::New(isolate, map, descriptors, size);
  map->InitializeDescriptors(isolate, *descriptors, *layout);
  map->CopyUnusedPropertyFieldsAdjustedForInstanceSize(*source_map);

  // Update bitfields
  map->set_may_have_interesting_symbols(
      source_map->may_have_interesting_symbols());

  return map;
}

static MaybeHandle<JSObject> CloneObjectSlowPath(Isolate* isolate,
                                                 Handle<HeapObject> source,
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

  MAYBE_RETURN(JSReceiver::SetOrCopyDataProperties(isolate, new_object, source,
                                                   nullptr, false),
               MaybeHandle<JSObject>());
  return new_object;
}

RUNTIME_FUNCTION(Runtime_CloneObjectIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<HeapObject> source = args.at<HeapObject>(0);
  int flags = args.smi_at(1);

  MigrateDeprecated(source);

  FeedbackSlot slot = FeedbackVector::ToSlot(args.smi_at(2));
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(3);
  if (maybe_vector->IsUndefined()) {
    RETURN_RESULT_OR_FAILURE(isolate,
                             CloneObjectSlowPath(isolate, source, flags));
  }

  DCHECK(maybe_vector->IsFeedbackVector());
  Handle<FeedbackVector> vector = Handle<FeedbackVector>::cast(maybe_vector);

  FeedbackNexus nexus(vector, slot);
  Handle<Map> source_map(source->map(), isolate);

  if (!CanFastCloneObject(source_map) || nexus.IsMegamorphic()) {
    // Migrate to slow mode if needed.
    nexus.ConfigureMegamorphic();
    RETURN_RESULT_OR_FAILURE(isolate,
                             CloneObjectSlowPath(isolate, source, flags));
  }

  Handle<Map> result_map = FastCloneObjectMap(isolate, source, flags);
  nexus.ConfigureCloneObject(source_map, result_map);

  return *result_map;
}

RUNTIME_FUNCTION(Runtime_StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<AccessorInfo> info = args.at<AccessorInfo>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at(4);
  HandleScope scope(isolate);

  if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {
    RETURN_RESULT_OR_FAILURE(
        isolate, Runtime::SetObjectProperty(isolate, receiver, name, value,
                                            StoreOrigin::kMaybeKeyed));
  }

  DCHECK(info->IsCompatibleReceiver(*receiver));

  PropertyCallbackArguments arguments(isolate, info->data(), *receiver, *holder,
                                      Nothing<ShouldThrow>());
  arguments.CallAccessorSetter(info, name, value);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return *value;
}

RUNTIME_FUNCTION(Runtime_LoadCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<AccessorInfo> info = args.at<AccessorInfo>(2);
  Handle<Name> name = args.at<Name>(3);
  HandleScope scope(isolate);

  DCHECK(info->IsCompatibleReceiver(*receiver));

  PropertyCallbackArguments custom_args(isolate, info->data(), *receiver,
                                        *holder, Just(kThrowOnError));
  Handle<Object> result = custom_args.CallAccessorGetter(info, name);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (result.is_null()) return ReadOnlyRoots(isolate).undefined_value();
  return *result;
}

RUNTIME_FUNCTION(Runtime_LoadAccessorProperty) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 3);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  int handler_kind = args.smi_at(1);
  Handle<CallHandlerInfo> call_handler_info = args.at<CallHandlerInfo>(2);

  Object holder = *receiver;
  if (handler_kind == LoadHandler::kApiGetterHolderIsPrototype) {
    holder = receiver->map().prototype();
  } else {
    DCHECK_EQ(handler_kind, LoadHandler::kApiGetter);
  }

  // Call the accessor without additional arguments.
  FunctionCallbackArguments custom(isolate, call_handler_info->data(),
                                   *receiver, holder, HeapObject(), nullptr, 0);
  Handle<Object> result_handle = custom.Call(*call_handler_info);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (result_handle.is_null()) return ReadOnlyRoots(isolate).undefined_value();
  return *result_handle;
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

  LookupIterator it(receiver, name, holder);
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

  Handle<Smi> slot = args.at<Smi>(3);
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
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  Handle<JSObject> receiver = args.at<JSObject>(3);
  Handle<Name> name = args.at<Name>(4);
  FeedbackSlot vector_slot = FeedbackVector::ToSlot(slot->value());

  // TODO(ishell): Cache interceptor_holder in the store handler like we do
  // for LoadHandler::kInterceptor case.
  Handle<JSObject> interceptor_holder = receiver;
  if (receiver->IsJSGlobalProxy()) {
    FeedbackSlotKind kind = vector->GetKind(vector_slot);
    if (IsStoreGlobalICKind(kind)) {
      interceptor_holder = Handle<JSObject>::cast(isolate->global_object());
    }
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

  LookupIterator it(receiver, name, receiver);
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
  DCHECK_GE(args.smi_at(1), 0);
  uint32_t index = args.smi_at(1);

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
  Handle<Smi> slot = args.at<Smi>(2);
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
  DCHECK_GE(args.smi_at(1), 0);
  uint32_t index = args.smi_at(1);

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
