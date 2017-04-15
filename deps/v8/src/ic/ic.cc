// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic.h"

#include <iostream>

#include "src/accessors.h"
#include "src/api-arguments-inl.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/conversions.h"
#include "src/execution.h"
#include "src/field-type.h"
#include "src/frames-inl.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-stats.h"
#include "src/ic/stub-cache.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler.h"
#include "src/prototype.h"
#include "src/runtime-profiler.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"
#include "src/tracing/trace-event.h"
#include "src/tracing/tracing-category-observer.h"

namespace v8 {
namespace internal {

char IC::TransitionMarkFromState(IC::State state) {
  switch (state) {
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
  return 0;
}


const char* GetTransitionMarkModifier(KeyedAccessStoreMode mode) {
  if (mode == STORE_NO_TRANSITION_HANDLE_COW) return ".COW";
  if (mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
    return ".IGNORE_OOB";
  }
  if (IsGrowStoreMode(mode)) return ".GROW";
  return "";
}


#ifdef DEBUG

#define TRACE_GENERIC_IC(isolate, type, reason)                \
  do {                                                         \
    if (FLAG_trace_ic) {                                       \
      PrintF("[%s patching generic stub in ", type);           \
      JavaScriptFrame::PrintTop(isolate, stdout, false, true); \
      PrintF(" (%s)]\n", reason);                              \
    }                                                          \
  } while (false)

#else

#define TRACE_GENERIC_IC(isolate, type, reason)      \
  do {                                               \
    if (FLAG_trace_ic) {                             \
      PrintF("[%s patching generic stub in ", type); \
      PrintF("(see below) (%s)]\n", reason);         \
    }                                                \
  } while (false)

#endif  // DEBUG


void IC::TraceIC(const char* type, Handle<Object> name) {
  if (FLAG_ic_stats) {
    if (AddressIsDeoptimizedCode()) return;
    DCHECK(UseVector());
    State new_state = nexus()->StateFromFeedback();
    TraceIC(type, name, state(), new_state);
  }
}


void IC::TraceIC(const char* type, Handle<Object> name, State old_state,
                 State new_state) {
  if (V8_LIKELY(!FLAG_ic_stats)) return;

  if (FLAG_ic_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    ICStats::instance()->Begin();
    ICInfo& ic_info = ICStats::instance()->Current();
    ic_info.type = is_keyed() ? "Keyed" : "";
    ic_info.type += type;
  } else {
    PrintF("[%s%s in ", is_keyed() ? "Keyed" : "", type);
  }

  // TODO(jkummerow): Add support for "apply". The logic is roughly:
  // marker = [fp_ + kMarkerOffset];
  // if marker is smi and marker.value == INTERNAL and
  //     the frame's code == builtin(Builtins::kFunctionApply):
  // then print "apply from" and advance one frame

  Object* maybe_function =
      Memory::Object_at(fp_ + JavaScriptFrameConstants::kFunctionOffset);
  if (maybe_function->IsJSFunction()) {
    JSFunction* function = JSFunction::cast(maybe_function);
    int code_offset = 0;
    if (function->IsInterpreted()) {
      code_offset = InterpretedFrame::GetBytecodeOffset(fp());
    } else {
      code_offset =
          static_cast<int>(pc() - function->code()->instruction_start());
    }
    if (FLAG_ic_stats &
        v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
      JavaScriptFrame::CollectFunctionAndOffsetForICStats(
          function, function->abstract_code(), code_offset);
    } else {
      JavaScriptFrame::PrintFunctionAndOffset(
          function, function->abstract_code(), code_offset, stdout, true);
    }
  }

  const char* modifier = "";
  if (kind() == Code::KEYED_STORE_IC) {
    KeyedAccessStoreMode mode =
        casted_nexus<KeyedStoreICNexus>()->GetKeyedAccessStoreMode();
    modifier = GetTransitionMarkModifier(mode);
  }
  Map* map = nullptr;
  if (!receiver_map().is_null()) {
    map = *receiver_map();
  }
  if (FLAG_ic_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    ICInfo& ic_info = ICStats::instance()->Current();
    // Reverse enough space for IC transition state, the longest length is 17.
    ic_info.state.reserve(17);
    ic_info.state = "(";
    ic_info.state += TransitionMarkFromState(old_state);
    ic_info.state += "->";
    ic_info.state += TransitionMarkFromState(new_state);
    ic_info.state += modifier;
    ic_info.state += ")";
    ic_info.map = reinterpret_cast<void*>(map);
  } else {
    PrintF(" (%c->%c%s) map=(%p", TransitionMarkFromState(old_state),
           TransitionMarkFromState(new_state), modifier,
           reinterpret_cast<void*>(map));
  }
  if (map != nullptr) {
    if (FLAG_ic_stats &
        v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
      ICInfo& ic_info = ICStats::instance()->Current();
      ic_info.is_dictionary_map = map->is_dictionary_map();
      ic_info.number_of_own_descriptors = map->NumberOfOwnDescriptors();
      ic_info.instance_type = std::to_string(map->instance_type());
    } else {
      PrintF(" dict=%u own=%u type=", map->is_dictionary_map(),
             map->NumberOfOwnDescriptors());
      std::cout << map->instance_type();
    }
  }
  if (FLAG_ic_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    // TODO(lpy) Add name as key field in ICStats.
    ICStats::instance()->End();
  } else {
    PrintF(") ");
    name->ShortPrint(stdout);
    PrintF("]\n");
  }
}


#define TRACE_IC(type, name) TraceIC(type, name)


IC::IC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus)
    : isolate_(isolate),
      vector_set_(false),
      target_maps_set_(false),
      nexus_(nexus) {
  // To improve the performance of the (much used) IC code, we unfold a few
  // levels of the stack frame iteration code. This yields a ~35% speedup when
  // running DeltaBlue and a ~25% speedup of gbemu with the '--nouse-ic' flag.
  const Address entry = Isolate::c_entry_fp(isolate->thread_local_top());
  Address* constant_pool = NULL;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool = reinterpret_cast<Address*>(
        entry + ExitFrameConstants::kConstantPoolOffset);
  }
  Address* pc_address =
      reinterpret_cast<Address*>(entry + ExitFrameConstants::kCallerPCOffset);
  Address fp = Memory::Address_at(entry + ExitFrameConstants::kCallerFPOffset);
  // If there's another JavaScript frame on the stack or a
  // StubFailureTrampoline, we need to look one frame further down the stack to
  // find the frame pointer and the return address stack slot.
  if (depth == EXTRA_CALL_FRAME) {
    if (FLAG_enable_embedded_constant_pool) {
      constant_pool = reinterpret_cast<Address*>(
          fp + StandardFrameConstants::kConstantPoolOffset);
    }
    const int kCallerPCOffset = StandardFrameConstants::kCallerPCOffset;
    pc_address = reinterpret_cast<Address*>(fp + kCallerPCOffset);
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
#ifdef DEBUG
  StackFrameIterator it(isolate);
  for (int i = 0; i < depth + 1; i++) it.Advance();
  StackFrame* frame = it.frame();
  DCHECK(fp == frame->fp() && pc_address == frame->pc_address());
#endif
  // For interpreted functions, some bytecode handlers construct a
  // frame. We have to skip the constructed frame to find the interpreted
  // function's frame. Check if the there is an additional frame, and if there
  // is skip this frame. However, the pc should not be updated. The call to
  // ICs happen from bytecode handlers.
  Object* frame_type =
      Memory::Object_at(fp + TypedFrameConstants::kFrameTypeOffset);
  if (frame_type == Smi::FromInt(StackFrame::STUB)) {
    fp = Memory::Address_at(fp + TypedFrameConstants::kCallerFPOffset);
  }
  fp_ = fp;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool_address_ = constant_pool;
  }
  pc_address_ = StackFrame::ResolveReturnAddressLocation(pc_address);
  Code* target = this->target();
  kind_ = target->kind();
  state_ = UseVector() ? nexus->StateFromFeedback() : StateFromCode(target);
  old_state_ = state_;
  extra_ic_state_ = target->extra_ic_state();
}

// The ICs that don't pass slot and vector through the stack have to
// save/restore them in the dispatcher.
bool IC::ShouldPushPopSlotAndVector(Code::Kind kind) {
  if (kind == Code::LOAD_IC || kind == Code::LOAD_GLOBAL_IC ||
      kind == Code::KEYED_LOAD_IC || kind == Code::CALL_IC) {
    return true;
  }
  if (kind == Code::STORE_IC || kind == Code::KEYED_STORE_IC) {
    return !StoreWithVectorDescriptor::kPassLastArgsOnStack;
  }
  return false;
}

InlineCacheState IC::StateFromCode(Code* code) {
  Isolate* isolate = code->GetIsolate();
  switch (code->kind()) {
    case Code::BINARY_OP_IC: {
      BinaryOpICState state(isolate, code->extra_ic_state());
      return state.GetICState();
    }
    case Code::COMPARE_IC: {
      CompareICStub stub(isolate, code->extra_ic_state());
      return stub.GetICState();
    }
    case Code::TO_BOOLEAN_IC: {
      ToBooleanICStub stub(isolate, code->extra_ic_state());
      return stub.GetICState();
    }
    default:
      if (code->is_debug_stub()) return UNINITIALIZED;
      UNREACHABLE();
      return UNINITIALIZED;
  }
}

SharedFunctionInfo* IC::GetSharedFunctionInfo() const {
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it(isolate());
  while (it.frame()->fp() != this->fp()) it.Advance();
  JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
  // Find the function on the stack and both the active code for the
  // function and the original code.
  JSFunction* function = frame->function();
  return function->shared();
}


Code* IC::GetCode() const {
  HandleScope scope(isolate());
  Handle<SharedFunctionInfo> shared(GetSharedFunctionInfo(), isolate());
  Code* code = shared->code();
  return code;
}

static void LookupForRead(LookupIterator* it) {
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
        if (!holder->GetNamedInterceptor()->getter()->IsUndefined(
                it->isolate())) {
          return;
        }
        break;
      }
      case LookupIterator::ACCESS_CHECK:
        // PropertyHandlerCompiler::CheckPrototypes() knows how to emit
        // access checks for global proxies.
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

  DCHECK(UseVector());
  maybe_handler_ = nexus()->FindHandlerForMap(receiver_map());

  // This is a contextual access, always just update the handler and stay
  // monomorphic.
  if (kind() == Code::LOAD_GLOBAL_IC) return true;

  // The current map wasn't handled yet. There's no reason to stay monomorphic,
  // *unless* we're moving from a deprecated map to its replacement, or
  // to a more general elements kind.
  // TODO(verwaest): Check if the current map is actually what the old map
  // would transition to.
  if (maybe_handler_.is_null()) {
    if (!receiver_map()->IsJSObjectMap()) return false;
    Map* first_map = FirstTargetMap();
    if (first_map == NULL) return false;
    Handle<Map> old_map(first_map);
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
    DCHECK(UseVector());
    Name* stub_name = nexus()->FindFirstName();
    if (*name != stub_name) return false;
  }

  return true;
}


void IC::UpdateState(Handle<Object> receiver, Handle<Object> name) {
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


MaybeHandle<Object> IC::TypeError(MessageTemplate::Template index,
                                  Handle<Object> object, Handle<Object> key) {
  HandleScope scope(isolate());
  THROW_NEW_ERROR(isolate(), NewTypeError(index, key, object), Object);
}


MaybeHandle<Object> IC::ReferenceError(Handle<Name> name) {
  HandleScope scope(isolate());
  THROW_NEW_ERROR(
      isolate(), NewReferenceError(MessageTemplate::kNotDefined, name), Object);
}


static void ComputeTypeInfoCountDelta(IC::State old_state, IC::State new_state,
                                      int* polymorphic_delta,
                                      int* generic_delta) {
  switch (old_state) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
      if (new_state == UNINITIALIZED || new_state == PREMONOMORPHIC) break;
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) {
        *polymorphic_delta = 1;
      } else if (new_state == MEGAMORPHIC || new_state == GENERIC) {
        *generic_delta = 1;
      }
      break;
    case MONOMORPHIC:
    case POLYMORPHIC:
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) break;
      *polymorphic_delta = -1;
      if (new_state == MEGAMORPHIC || new_state == GENERIC) {
        *generic_delta = 1;
      }
      break;
    case MEGAMORPHIC:
    case GENERIC:
      if (new_state == MEGAMORPHIC || new_state == GENERIC) break;
      *generic_delta = -1;
      if (new_state == MONOMORPHIC || new_state == POLYMORPHIC) {
        *polymorphic_delta = 1;
      }
      break;
    case RECOMPUTE_HANDLER:
      UNREACHABLE();
  }
}

// static
void IC::OnTypeFeedbackChanged(Isolate* isolate, Code* host) {
  if (host->kind() != Code::FUNCTION) return;

  TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
  info->change_own_type_change_checksum();
  host->set_profiler_ticks(0);
  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
}

void IC::PostPatching(Address address, Code* target, Code* old_target) {
  // Type vector based ICs update these statistics at a different time because
  // they don't always patch on state change.
  if (ICUseVector(target->kind())) return;

  DCHECK(old_target->is_inline_cache_stub());
  DCHECK(target->is_inline_cache_stub());
  State old_state = StateFromCode(old_target);
  State new_state = StateFromCode(target);

  Isolate* isolate = target->GetIsolate();
  Code* host =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  if (host->kind() != Code::FUNCTION) return;

  // Not all Code objects have TypeFeedbackInfo.
  if (host->type_feedback_info()->IsTypeFeedbackInfo()) {
    if (FLAG_type_info_threshold > 0) {
      int polymorphic_delta = 0;  // "Polymorphic" here includes monomorphic.
      int generic_delta = 0;      // "Generic" here includes megamorphic.
      ComputeTypeInfoCountDelta(old_state, new_state, &polymorphic_delta,
                                &generic_delta);
      TypeFeedbackInfo* info =
          TypeFeedbackInfo::cast(host->type_feedback_info());
      info->change_ic_with_type_info_count(polymorphic_delta);
      info->change_ic_generic_count(generic_delta);
    }
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_own_type_change_checksum();
  }
  host->set_profiler_ticks(0);
  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
}

void IC::Clear(Isolate* isolate, Address address, Address constant_pool) {
  Code* target = GetTargetAtAddress(address, constant_pool);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->is_debug_stub()) return;

  if (target->kind() == Code::COMPARE_IC) {
    CompareIC::Clear(isolate, address, target, constant_pool);
  }
}


void KeyedLoadIC::Clear(Isolate* isolate, Code* host, KeyedLoadICNexus* nexus) {
  if (IsCleared(nexus)) return;
  // Make sure to also clear the map used in inline fast cases.  If we
  // do not clear these maps, cached code can keep objects alive
  // through the embedded maps.
  nexus->ConfigurePremonomorphic();
  OnTypeFeedbackChanged(isolate, host);
}


void CallIC::Clear(Isolate* isolate, Code* host, CallICNexus* nexus) {
  // Determine our state.
  Object* feedback = nexus->vector()->Get(nexus->slot());
  State state = nexus->StateFromFeedback();

  if (state != UNINITIALIZED && !feedback->IsAllocationSite()) {
    nexus->ConfigureUninitialized();
    // The change in state must be processed.
    OnTypeFeedbackChanged(isolate, host);
  }
}


void LoadIC::Clear(Isolate* isolate, Code* host, LoadICNexus* nexus) {
  if (IsCleared(nexus)) return;
  nexus->ConfigurePremonomorphic();
  OnTypeFeedbackChanged(isolate, host);
}

void LoadGlobalIC::Clear(Isolate* isolate, Code* host,
                         LoadGlobalICNexus* nexus) {
  if (IsCleared(nexus)) return;
  nexus->ConfigureUninitialized();
  OnTypeFeedbackChanged(isolate, host);
}

void StoreIC::Clear(Isolate* isolate, Code* host, StoreICNexus* nexus) {
  if (IsCleared(nexus)) return;
  nexus->ConfigurePremonomorphic();
  OnTypeFeedbackChanged(isolate, host);
}


void KeyedStoreIC::Clear(Isolate* isolate, Code* host,
                         KeyedStoreICNexus* nexus) {
  if (IsCleared(nexus)) return;
  nexus->ConfigurePremonomorphic();
  OnTypeFeedbackChanged(isolate, host);
}


void CompareIC::Clear(Isolate* isolate, Address address, Code* target,
                      Address constant_pool) {
  DCHECK(CodeStub::GetMajorKey(target) == CodeStub::CompareIC);
  CompareICStub stub(target->stub_key(), isolate);
  // Only clear CompareICs that can retain objects.
  if (stub.state() != CompareICState::KNOWN_RECEIVER) return;
  SetTargetAtAddress(address, GetRawUninitialized(isolate, stub.op()),
                     constant_pool);
  PatchInlinedSmiCode(isolate, address, DISABLE_INLINED_SMI_CHECK);
}

static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map()->is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}

void IC::ConfigureVectorState(IC::State new_state, Handle<Object> key) {
  DCHECK(UseVector());
  if (new_state == PREMONOMORPHIC) {
    nexus()->ConfigurePremonomorphic();
  } else if (new_state == MEGAMORPHIC) {
    if (kind() == Code::LOAD_IC || kind() == Code::STORE_IC) {
      nexus()->ConfigureMegamorphic();
    } else if (kind() == Code::KEYED_LOAD_IC) {
      KeyedLoadICNexus* nexus = casted_nexus<KeyedLoadICNexus>();
      nexus->ConfigureMegamorphicKeyed(key->IsName() ? PROPERTY : ELEMENT);
    } else {
      DCHECK(kind() == Code::KEYED_STORE_IC);
      KeyedStoreICNexus* nexus = casted_nexus<KeyedStoreICNexus>();
      nexus->ConfigureMegamorphicKeyed(key->IsName() ? PROPERTY : ELEMENT);
    }
  } else {
    UNREACHABLE();
  }

  vector_set_ = true;
  OnTypeFeedbackChanged(isolate(), get_host());
}

void IC::ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                              Handle<Object> handler) {
  DCHECK(UseVector());
  if (kind() == Code::LOAD_IC) {
    LoadICNexus* nexus = casted_nexus<LoadICNexus>();
    nexus->ConfigureMonomorphic(map, handler);
  } else if (kind() == Code::LOAD_GLOBAL_IC) {
    LoadGlobalICNexus* nexus = casted_nexus<LoadGlobalICNexus>();
    nexus->ConfigureHandlerMode(handler);
  } else if (kind() == Code::KEYED_LOAD_IC) {
    KeyedLoadICNexus* nexus = casted_nexus<KeyedLoadICNexus>();
    nexus->ConfigureMonomorphic(name, map, handler);
  } else if (kind() == Code::STORE_IC) {
    StoreICNexus* nexus = casted_nexus<StoreICNexus>();
    nexus->ConfigureMonomorphic(map, handler);
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    KeyedStoreICNexus* nexus = casted_nexus<KeyedStoreICNexus>();
    nexus->ConfigureMonomorphic(name, map, handler);
  }

  vector_set_ = true;
  OnTypeFeedbackChanged(isolate(), get_host());
}

void IC::ConfigureVectorState(Handle<Name> name, MapHandleList* maps,
                              List<Handle<Object>>* handlers) {
  DCHECK(UseVector());
  if (kind() == Code::LOAD_IC) {
    LoadICNexus* nexus = casted_nexus<LoadICNexus>();
    nexus->ConfigurePolymorphic(maps, handlers);
  } else if (kind() == Code::KEYED_LOAD_IC) {
    KeyedLoadICNexus* nexus = casted_nexus<KeyedLoadICNexus>();
    nexus->ConfigurePolymorphic(name, maps, handlers);
  } else if (kind() == Code::STORE_IC) {
    StoreICNexus* nexus = casted_nexus<StoreICNexus>();
    nexus->ConfigurePolymorphic(maps, handlers);
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    KeyedStoreICNexus* nexus = casted_nexus<KeyedStoreICNexus>();
    nexus->ConfigurePolymorphic(name, maps, handlers);
  }

  vector_set_ = true;
  OnTypeFeedbackChanged(isolate(), get_host());
}

void IC::ConfigureVectorState(MapHandleList* maps,
                              MapHandleList* transitioned_maps,
                              List<Handle<Object>>* handlers) {
  DCHECK(UseVector());
  DCHECK(kind() == Code::KEYED_STORE_IC);
  KeyedStoreICNexus* nexus = casted_nexus<KeyedStoreICNexus>();
  nexus->ConfigurePolymorphic(maps, transitioned_maps, handlers);

  vector_set_ = true;
  OnTypeFeedbackChanged(isolate(), get_host());
}


MaybeHandle<Object> LoadIC::Load(Handle<Object> object, Handle<Name> name) {
  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsNullOrUndefined(isolate())) {
    if (FLAG_use_ic && state() != UNINITIALIZED && state() != PREMONOMORPHIC) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), LoadIC_NonReceiver);
      update_receiver_map(object);
      PatchCache(name, slow_stub());
      TRACE_IC("LoadIC", name);
    }
    return TypeError(MessageTemplate::kNonObjectPropertyLoad, object, name);
  }

  bool use_ic = MigrateDeprecated(object) ? false : FLAG_use_ic;

  if (state() != UNINITIALIZED) {
    JSObject::MakePrototypesFast(object, kStartAtReceiver, isolate());
    update_receiver_map(object);
  }
  // Named lookup in the object.
  LookupIterator it(object, name);
  LookupForRead(&it);

  if (it.IsFound() || !ShouldThrowReferenceError()) {
    // Update inline cache and stub cache.
    if (use_ic) UpdateCaches(&it);

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
        global->native_context()->script_context_table());

    ScriptContextTable::LookupResult lookup_result;
    if (ScriptContextTable::Lookup(script_contexts, str_name, &lookup_result)) {
      Handle<Object> result =
          FixedArray::get(*ScriptContextTable::GetContext(
                              script_contexts, lookup_result.context_index),
                          lookup_result.slot_index, isolate());
      if (result->IsTheHole(isolate())) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        return ReferenceError(name);
      }

      if (FLAG_use_ic && LoadScriptContextFieldStub::Accepted(&lookup_result)) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadScriptContextFieldStub);
        LoadScriptContextFieldStub stub(isolate(), &lookup_result);
        PatchCache(name, stub.GetCode());
        TRACE_IC("LoadGlobalIC", name);
      }
      return result;
    }
  }
  return LoadIC::Load(global, name);
}

static bool AddOneReceiverMapIfMissing(MapHandleList* receiver_maps,
                                       Handle<Map> new_receiver_map) {
  DCHECK(!new_receiver_map.is_null());
  for (int current = 0; current < receiver_maps->length(); ++current) {
    if (!receiver_maps->at(current).is_null() &&
        receiver_maps->at(current).is_identical_to(new_receiver_map)) {
      return false;
    }
  }
  receiver_maps->Add(new_receiver_map);
  return true;
}

bool IC::UpdatePolymorphicIC(Handle<Name> name, Handle<Object> handler) {
  DCHECK(IsHandler(*handler));
  if (is_keyed() && state() != RECOMPUTE_HANDLER) return false;
  Handle<Map> map = receiver_map();
  MapHandleList maps;
  List<Handle<Object>> handlers;

  TargetMaps(&maps);
  int number_of_maps = maps.length();
  int deprecated_maps = 0;
  int handler_to_overwrite = -1;

  for (int i = 0; i < number_of_maps; i++) {
    Handle<Map> current_map = maps.at(i);
    if (current_map->is_deprecated()) {
      // Filter out deprecated maps to ensure their instances get migrated.
      ++deprecated_maps;
    } else if (map.is_identical_to(current_map)) {
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

  if (number_of_valid_maps >= 4) return false;
  if (number_of_maps == 0 && state() != MONOMORPHIC && state() != POLYMORPHIC) {
    return false;
  }
  DCHECK(UseVector());
  if (!nexus()->FindHandlers(&handlers, maps.length())) return false;

  number_of_valid_maps++;
  if (number_of_valid_maps > 1 && is_keyed()) return false;
  if (number_of_valid_maps == 1) {
    ConfigureVectorState(name, receiver_map(), handler);
  } else {
    if (handler_to_overwrite >= 0) {
      handlers.Set(handler_to_overwrite, handler);
      if (!map.is_identical_to(maps.at(handler_to_overwrite))) {
        maps.Set(handler_to_overwrite, map);
      }
    } else {
      maps.Add(map);
      handlers.Add(handler);
    }

    ConfigureVectorState(name, &maps, &handlers);
  }

  return true;
}

void IC::UpdateMonomorphicIC(Handle<Object> handler, Handle<Name> name) {
  DCHECK(IsHandler(*handler));
  ConfigureVectorState(name, receiver_map(), handler);
}


void IC::CopyICToMegamorphicCache(Handle<Name> name) {
  MapHandleList maps;
  List<Handle<Object>> handlers;
  TargetMaps(&maps);
  if (!nexus()->FindHandlers(&handlers, maps.length())) return;
  for (int i = 0; i < maps.length(); i++) {
    UpdateMegamorphicCache(*maps.at(i), *name, *handlers.at(i));
  }
}


bool IC::IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map) {
  if (source_map == NULL) return true;
  if (target_map == NULL) return false;
  ElementsKind target_elements_kind = target_map->elements_kind();
  bool more_general_transition = IsMoreGeneralElementsKindTransition(
      source_map->elements_kind(), target_elements_kind);
  Map* transitioned_map = nullptr;
  if (more_general_transition) {
    MapHandleList map_list;
    map_list.Add(handle(target_map));
    transitioned_map = source_map->FindElementsKindTransitionedMap(&map_list);
  }
  return transitioned_map == target_map;
}

void IC::PatchCache(Handle<Name> name, Handle<Object> handler) {
  DCHECK(IsHandler(*handler));
  // Currently only LoadIC and KeyedLoadIC support non-code handlers.
  DCHECK_IMPLIES(!handler->IsCode(), kind() == Code::LOAD_IC ||
                                         kind() == Code::LOAD_GLOBAL_IC ||
                                         kind() == Code::KEYED_LOAD_IC ||
                                         kind() == Code::STORE_IC ||
                                         kind() == Code::KEYED_STORE_IC);
  switch (state()) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
      UpdateMonomorphicIC(handler, name);
      break;
    case RECOMPUTE_HANDLER:
    case MONOMORPHIC:
      if (kind() == Code::LOAD_GLOBAL_IC) {
        UpdateMonomorphicIC(handler, name);
        break;
      }
    // Fall through.
    case POLYMORPHIC:
      if (!is_keyed() || state() == RECOMPUTE_HANDLER) {
        if (UpdatePolymorphicIC(name, handler)) break;
        // For keyed stubs, we can't know whether old handlers were for the
        // same key.
        CopyICToMegamorphicCache(name);
      }
      DCHECK(UseVector());
      ConfigureVectorState(MEGAMORPHIC, name);
    // Fall through.
    case MEGAMORPHIC:
      UpdateMegamorphicCache(*receiver_map(), *name, *handler);
      // Indicate that we've handled this case.
      DCHECK(UseVector());
      vector_set_ = true;
      break;
    case GENERIC:
      UNREACHABLE();
      break;
  }
}

Handle<Object> LoadIC::SimpleFieldLoad(Isolate* isolate, FieldIndex index) {
  TRACE_HANDLER_STATS(isolate, LoadIC_LoadFieldDH);
  return LoadHandler::LoadField(isolate, index);
}

namespace {

template <bool fill_array = true>
int InitPrototypeChecks(Isolate* isolate, Handle<Map> receiver_map,
                        Handle<JSObject> holder, Handle<Name> name,
                        Handle<FixedArray> array, int first_index) {
  DCHECK(holder.is_null() || holder->HasFastProperties());

  // We don't encode the requirement to check access rights because we already
  // passed the access check for current native context and the access
  // can't be revoked.

  HandleScope scope(isolate);
  int checks_count = 0;

  if (receiver_map->IsPrimitiveMap() || receiver_map->IsJSGlobalProxyMap()) {
    // The validity cell check for primitive and global proxy receivers does
    // not guarantee that certain native context ever had access to other
    // native context. However, a handler created for one native context could
    // be used in other native context through the megamorphic stub cache.
    // So we record the original native context to which this handler
    // corresponds.
    if (fill_array) {
      Handle<Context> native_context = isolate->native_context();
      array->set(LoadHandler::kFirstPrototypeIndex + checks_count,
                 native_context->self_weak_cell());
    }
    checks_count++;

  } else if (receiver_map->IsJSGlobalObjectMap()) {
    if (fill_array) {
      Handle<JSGlobalObject> global = isolate->global_object();
      Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
          global, name, PropertyCellType::kInvalidated);
      DCHECK(cell->value()->IsTheHole(isolate));
      Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(cell);
      array->set(LoadHandler::kFirstPrototypeIndex + checks_count, *weak_cell);
    }
    checks_count++;
  }

  // Create/count entries for each global or dictionary prototype appeared in
  // the prototype chain contains from receiver till holder.
  PrototypeIterator::WhereToEnd end = name->IsPrivate()
                                          ? PrototypeIterator::END_AT_NON_HIDDEN
                                          : PrototypeIterator::END_AT_NULL;
  for (PrototypeIterator iter(receiver_map, end); !iter.IsAtEnd();
       iter.Advance()) {
    Handle<JSObject> current = PrototypeIterator::GetCurrent<JSObject>(iter);
    if (holder.is_identical_to(current)) break;
    Handle<Map> current_map(current->map(), isolate);

    if (current_map->IsJSGlobalObjectMap()) {
      if (fill_array) {
        Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(current);
        Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
            global, name, PropertyCellType::kInvalidated);
        DCHECK(cell->value()->IsTheHole(isolate));
        Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(cell);
        array->set(first_index + checks_count, *weak_cell);
      }
      checks_count++;

    } else if (current_map->is_dictionary_map()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      if (fill_array) {
        DCHECK_EQ(NameDictionary::kNotFound,
                  current->property_dictionary()->FindEntry(name));
        Handle<WeakCell> weak_cell =
            Map::GetOrCreatePrototypeWeakCell(current, isolate);
        array->set(first_index + checks_count, *weak_cell);
      }
      checks_count++;
    }
  }
  return checks_count;
}

// Returns 0 if the validity cell check is enough to ensure that the
// prototype chain from |receiver_map| till |holder| did not change.
// If the |holder| is an empty handle then the full prototype chain is
// checked.
// Returns -1 if the handler has to be compiled or the number of prototype
// checks otherwise.
int GetPrototypeCheckCount(Isolate* isolate, Handle<Map> receiver_map,
                           Handle<JSObject> holder, Handle<Name> name) {
  return InitPrototypeChecks<false>(isolate, receiver_map, holder, name,
                                    Handle<FixedArray>(), 0);
}

}  // namespace

Handle<Object> LoadIC::LoadFromPrototype(Handle<Map> receiver_map,
                                         Handle<JSObject> holder,
                                         Handle<Name> name,
                                         Handle<Object> smi_handler) {
  int checks_count =
      GetPrototypeCheckCount(isolate(), receiver_map, holder, name);
  DCHECK_LE(0, checks_count);

  if (receiver_map->IsPrimitiveMap() || receiver_map->IsJSGlobalProxyMap()) {
    DCHECK(!receiver_map->is_dictionary_map());
    DCHECK_LE(1, checks_count);  // For native context.
    smi_handler =
        LoadHandler::EnableAccessCheckOnReceiver(isolate(), smi_handler);
  } else if (receiver_map->is_dictionary_map() &&
             !receiver_map->IsJSGlobalObjectMap()) {
    smi_handler =
        LoadHandler::EnableNegativeLookupOnReceiver(isolate(), smi_handler);
  }

  Handle<Cell> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  DCHECK(!validity_cell.is_null());

  Handle<WeakCell> holder_cell =
      Map::GetOrCreatePrototypeWeakCell(holder, isolate());

  if (checks_count == 0) {
    return isolate()->factory()->NewTuple3(holder_cell, smi_handler,
                                           validity_cell);
  }
  Handle<FixedArray> handler_array(isolate()->factory()->NewFixedArray(
      LoadHandler::kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(LoadHandler::kSmiHandlerIndex, *smi_handler);
  handler_array->set(LoadHandler::kValidityCellIndex, *validity_cell);
  handler_array->set(LoadHandler::kHolderCellIndex, *holder_cell);
  InitPrototypeChecks(isolate(), receiver_map, holder, name, handler_array,
                      LoadHandler::kFirstPrototypeIndex);
  return handler_array;
}

Handle<Object> LoadIC::LoadNonExistent(Handle<Map> receiver_map,
                                       Handle<Name> name) {
  Handle<JSObject> holder;  // null handle
  int checks_count =
      GetPrototypeCheckCount(isolate(), receiver_map, holder, name);
  DCHECK_LE(0, checks_count);

  bool do_negative_lookup_on_receiver =
      receiver_map->is_dictionary_map() && !receiver_map->IsJSGlobalObjectMap();
  Handle<Object> smi_handler =
      LoadHandler::LoadNonExistent(isolate(), do_negative_lookup_on_receiver);

  if (receiver_map->IsPrimitiveMap() || receiver_map->IsJSGlobalProxyMap()) {
    DCHECK(!receiver_map->is_dictionary_map());
    DCHECK_LE(1, checks_count);  // For native context.
    smi_handler =
        LoadHandler::EnableAccessCheckOnReceiver(isolate(), smi_handler);
  }

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    validity_cell = handle(Smi::FromInt(0), isolate());
  }

  Factory* factory = isolate()->factory();
  if (checks_count == 0) {
    return factory->NewTuple3(factory->null_value(), smi_handler,
                              validity_cell);
  }
  Handle<FixedArray> handler_array(factory->NewFixedArray(
      LoadHandler::kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(LoadHandler::kSmiHandlerIndex, *smi_handler);
  handler_array->set(LoadHandler::kValidityCellIndex, *validity_cell);
  handler_array->set(LoadHandler::kHolderCellIndex, *factory->null_value());
  InitPrototypeChecks(isolate(), receiver_map, holder, name, handler_array,
                      LoadHandler::kFirstPrototypeIndex);
  return handler_array;
}

bool IsCompatibleReceiver(LookupIterator* lookup, Handle<Map> receiver_map) {
  DCHECK(lookup->state() == LookupIterator::ACCESSOR);
  Isolate* isolate = lookup->isolate();
  Handle<Object> accessors = lookup->GetAccessors();
  if (accessors->IsAccessorInfo()) {
    Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
    if (info->getter() != NULL &&
        !AccessorInfo::IsCompatibleReceiverMap(isolate, info, receiver_map)) {
      return false;
    }
  } else if (accessors->IsAccessorPair()) {
    Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                          isolate);
    if (!getter->IsJSFunction() && !getter->IsFunctionTemplateInfo()) {
      return false;
    }
    Handle<JSObject> holder = lookup->GetHolder<JSObject>();
    Handle<Object> receiver = lookup->GetReceiver();
    if (holder->HasFastProperties()) {
      if (getter->IsJSFunction()) {
        Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
        if (!receiver->IsJSObject() && function->shared()->IsUserJavaScript() &&
            is_sloppy(function->shared()->language_mode())) {
          // Calling sloppy non-builtins with a value as the receiver
          // requires boxing.
          return false;
        }
      }
      CallOptimization call_optimization(getter);
      if (call_optimization.is_simple_api_call() &&
          !call_optimization.IsCompatibleReceiverMap(receiver_map, holder)) {
        return false;
      }
    }
  }
  return true;
}


void LoadIC::UpdateCaches(LookupIterator* lookup) {
  if (state() == UNINITIALIZED && kind() != Code::LOAD_GLOBAL_IC) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    TRACE_HANDLER_STATS(isolate(), LoadIC_Premonomorphic);
    ConfigureVectorState(PREMONOMORPHIC, Handle<Object>());
    TRACE_IC("LoadIC", lookup->name());
    return;
  }

  Handle<Object> code;
  if (lookup->state() == LookupIterator::JSPROXY ||
      lookup->state() == LookupIterator::ACCESS_CHECK) {
    code = slow_stub();
  } else if (!lookup->IsFound()) {
    if (kind() == Code::LOAD_IC || kind() == Code::LOAD_GLOBAL_IC) {
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNonexistentDH);
      code = LoadNonExistent(receiver_map(), lookup->name());
    } else {
      code = slow_stub();
    }
  } else {
    if (kind() == Code::LOAD_GLOBAL_IC &&
        lookup->state() == LookupIterator::DATA &&
        lookup->GetReceiver().is_identical_to(lookup->GetHolder<Object>())) {
      DCHECK(lookup->GetReceiver()->IsJSGlobalObject());
      // Now update the cell in the feedback vector.
      LoadGlobalICNexus* nexus = casted_nexus<LoadGlobalICNexus>();
      nexus->ConfigurePropertyCellMode(lookup->GetPropertyCell());
      TRACE_IC("LoadGlobalIC", lookup->name());
      return;
    } else if (lookup->state() == LookupIterator::ACCESSOR) {
      if (!IsCompatibleReceiver(lookup, receiver_map())) {
        TRACE_GENERIC_IC(isolate(), "LoadIC", "incompatible receiver type");
        code = slow_stub();
      }
    } else if (lookup->state() == LookupIterator::INTERCEPTOR) {
      // Perform a lookup behind the interceptor. Copy the LookupIterator
      // since the original iterator will be used to fetch the value.
      LookupIterator it = *lookup;
      it.Next();
      LookupForRead(&it);
      if (it.state() == LookupIterator::ACCESSOR &&
          !IsCompatibleReceiver(&it, receiver_map())) {
        TRACE_GENERIC_IC(isolate(), "LoadIC", "incompatible receiver type");
        code = slow_stub();
      }
    }
    if (code.is_null()) code = ComputeHandler(lookup);
  }

  PatchCache(lookup->name(), code);
  TRACE_IC("LoadIC", lookup->name());
}

StubCache* IC::stub_cache() {
  switch (kind()) {
    case Code::LOAD_IC:
    case Code::KEYED_LOAD_IC:
      return isolate()->load_stub_cache();

    case Code::STORE_IC:
    case Code::KEYED_STORE_IC:
      return isolate()->store_stub_cache();

    default:
      break;
  }
  UNREACHABLE();
  return nullptr;
}

void IC::UpdateMegamorphicCache(Map* map, Name* name, Object* handler) {
  stub_cache()->Set(name, map, handler);
}

void IC::TraceHandlerCacheHitStats(LookupIterator* lookup) {
  if (!FLAG_runtime_call_stats) return;

  if (kind() == Code::LOAD_IC || kind() == Code::LOAD_GLOBAL_IC ||
      kind() == Code::KEYED_LOAD_IC) {
    switch (lookup->state()) {
      case LookupIterator::ACCESS_CHECK:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_AccessCheck);
        break;
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Exotic);
        break;
      case LookupIterator::INTERCEPTOR:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Interceptor);
        break;
      case LookupIterator::JSPROXY:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_JSProxy);
        break;
      case LookupIterator::NOT_FOUND:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_NonExistent);
        break;
      case LookupIterator::ACCESSOR:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Accessor);
        break;
      case LookupIterator::DATA:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Data);
        break;
      case LookupIterator::TRANSITION:
        TRACE_HANDLER_STATS(isolate(), LoadIC_HandlerCacheHit_Transition);
        break;
    }
  } else if (kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC) {
    switch (lookup->state()) {
      case LookupIterator::ACCESS_CHECK:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_AccessCheck);
        break;
      case LookupIterator::INTEGER_INDEXED_EXOTIC:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Exotic);
        break;
      case LookupIterator::INTERCEPTOR:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Interceptor);
        break;
      case LookupIterator::JSPROXY:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_JSProxy);
        break;
      case LookupIterator::NOT_FOUND:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_NonExistent);
        break;
      case LookupIterator::ACCESSOR:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Accessor);
        break;
      case LookupIterator::DATA:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Data);
        break;
      case LookupIterator::TRANSITION:
        TRACE_HANDLER_STATS(isolate(), StoreIC_HandlerCacheHit_Transition);
        break;
    }
  } else {
    TRACE_HANDLER_STATS(isolate(), IC_HandlerCacheHit);
  }
}

Handle<Object> IC::ComputeHandler(LookupIterator* lookup,
                                  Handle<Object> value) {
  // Try to find a globally shared handler stub.
  Handle<Object> shared_handler = GetMapIndependentHandler(lookup);
  if (!shared_handler.is_null()) {
    DCHECK(IC::IsHandler(*shared_handler));
    return shared_handler;
  }

  // Otherwise check the map's handler cache for a map-specific handler, and
  // compile one if the cache comes up empty.
  bool receiver_is_holder =
      lookup->GetReceiver().is_identical_to(lookup->GetHolder<JSObject>());
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map;
  if (kind() == Code::LOAD_IC || kind() == Code::LOAD_GLOBAL_IC ||
      kind() == Code::KEYED_LOAD_IC) {
    stub_holder_map = IC::GetHandlerCacheHolder(
        receiver_map(), receiver_is_holder, isolate(), &flag);
  } else {
    DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
    // Store handlers cannot be cached on prototypes.
    flag = kCacheOnReceiver;
    stub_holder_map = receiver_map();
  }

  Handle<Object> handler = PropertyHandlerCompiler::Find(
      lookup->name(), stub_holder_map, kind(), flag);
  // Use the cached value if it exists, and if it is different from the
  // handler that just missed.
  if (!handler.is_null()) {
    Handle<Object> current_handler;
    if (maybe_handler_.ToHandle(&current_handler)) {
      if (!current_handler.is_identical_to(handler)) {
        TraceHandlerCacheHitStats(lookup);
        return handler;
      }
    } else {
      // maybe_handler_ is only populated for MONOMORPHIC and POLYMORPHIC ICs.
      // In MEGAMORPHIC case, check if the handler in the megamorphic stub
      // cache (which just missed) is different from the cached handler.
      if (state() == MEGAMORPHIC && lookup->GetReceiver()->IsHeapObject()) {
        Map* map = Handle<HeapObject>::cast(lookup->GetReceiver())->map();
        Object* megamorphic_cached_handler =
            stub_cache()->Get(*lookup->name(), map);
        if (megamorphic_cached_handler != *handler) {
          TraceHandlerCacheHitStats(lookup);
          return handler;
        }
      } else {
        TraceHandlerCacheHitStats(lookup);
        return handler;
      }
    }
  }

  handler = CompileHandler(lookup, value, flag);
  DCHECK(IC::IsHandler(*handler));
  if (handler->IsCode()) {
    Handle<Code> code = Handle<Code>::cast(handler);
    DCHECK_EQ(Code::ExtractCacheHolderFromFlags(code->flags()), flag);
    Map::UpdateCodeCache(stub_holder_map, lookup->name(), code);
  }
  return handler;
}

Handle<Object> LoadIC::GetMapIndependentHandler(LookupIterator* lookup) {
  Handle<Object> receiver = lookup->GetReceiver();
  if (receiver->IsString() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
    FieldIndex index = FieldIndex::ForInObjectOffset(String::kLengthOffset);
    return SimpleFieldLoad(isolate(), index);
  }

  if (receiver->IsStringWrapper() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
    TRACE_HANDLER_STATS(isolate(), LoadIC_StringLengthStub);
    StringLengthStub string_length_stub(isolate());
    return string_length_stub.GetCode();
  }

  // Use specialized code for getting prototype of functions.
  if (receiver->IsJSFunction() &&
      Name::Equals(isolate()->factory()->prototype_string(), lookup->name()) &&
      receiver->IsConstructor() &&
      !Handle<JSFunction>::cast(receiver)
           ->map()
           ->has_non_instance_prototype()) {
    Handle<Code> stub;
    TRACE_HANDLER_STATS(isolate(), LoadIC_FunctionPrototypeStub);
    FunctionPrototypeStub function_prototype_stub(isolate());
    return function_prototype_stub.GetCode();
  }

  Handle<Map> map = receiver_map();
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  bool receiver_is_holder = receiver.is_identical_to(holder);
  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR:
      break;  // Custom-compiled handler.

    case LookupIterator::ACCESSOR: {
      // Use simple field loads for some well-known callback properties.
      // The method will only return true for absolute truths based on the
      // receiver maps.
      int object_offset;
      if (Accessors::IsJSObjectFieldAccessor(map, lookup->name(),
                                             &object_offset)) {
        FieldIndex index = FieldIndex::ForInObjectOffset(object_offset, *map);
        return SimpleFieldLoad(isolate(), index);
      }

      if (IsCompatibleReceiver(lookup, map)) {
        Handle<Object> accessors = lookup->GetAccessors();
        if (accessors->IsAccessorPair()) {
          if (!holder->HasFastProperties()) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return slow_stub();
          }
          // When debugging we need to go the slow path to flood the accessor.
          if (GetSharedFunctionInfo()->HasDebugInfo()) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return slow_stub();
          }
          break;  // Custom-compiled handler.
        } else if (accessors->IsAccessorInfo()) {
          Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
          if (v8::ToCData<Address>(info->getter()) == nullptr) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return slow_stub();
          }
          // Ruled out by IsCompatibleReceiver() above.
          DCHECK(AccessorInfo::IsCompatibleReceiverMap(isolate(), info, map));
          if (!holder->HasFastProperties() ||
              (info->is_sloppy() && !receiver->IsJSReceiver())) {
            DCHECK(!holder->HasFastProperties() || !receiver_is_holder);
            TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
            return slow_stub();
          }
          Handle<Object> smi_handler =
              LoadHandler::LoadApiGetter(isolate(), lookup->GetAccessorIndex());
          if (receiver_is_holder) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_LoadApiGetterDH);
            return smi_handler;
          }
          if (kind() != Code::LOAD_GLOBAL_IC) {
            TRACE_HANDLER_STATS(isolate(), LoadIC_LoadApiGetterFromPrototypeDH);
            return LoadFromPrototype(map, holder, lookup->name(), smi_handler);
          }
          break;  // Custom-compiled handler.
        }
      }
      TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
      return slow_stub();
    }

    case LookupIterator::DATA: {
      DCHECK_EQ(kData, lookup->property_details().kind());
      if (lookup->is_dictionary_holder()) {
        if (kind() != Code::LOAD_IC && kind() != Code::LOAD_GLOBAL_IC) {
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return slow_stub();
        }
        if (holder->IsJSGlobalObject()) {
          break;  // Custom-compiled handler.
        }
        // There is only one shared stub for loading normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the object for the stub to be
        // applicable.
        if (!receiver_is_holder) {
          TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
          return slow_stub();
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadNormal);
        return isolate()->builtins()->LoadIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().location() == kField) {
        FieldIndex field = lookup->GetFieldIndex();
        Handle<Object> smi_handler = SimpleFieldLoad(isolate(), field);
        if (receiver_is_holder) {
          return smi_handler;
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadFieldFromPrototypeDH);
        return LoadFromPrototype(map, holder, lookup->name(), smi_handler);
      }

      // -------------- Constant properties --------------
      DCHECK_EQ(kDescriptor, lookup->property_details().location());
      Handle<Object> smi_handler =
          LoadHandler::LoadConstant(isolate(), lookup->GetConstantIndex());
      if (receiver_is_holder) {
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadConstantDH);
        return smi_handler;
      }
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadConstantFromPrototypeDH);
      return LoadFromPrototype(map, holder, lookup->name(), smi_handler);
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      TRACE_HANDLER_STATS(isolate(), LoadIC_SlowStub);
      return slow_stub();
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }

  return Handle<Code>::null();
}

Handle<Object> LoadIC::CompileHandler(LookupIterator* lookup,
                                      Handle<Object> unused,
                                      CacheHolderFlag cache_holder) {
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
#ifdef DEBUG
  // Only used by DCHECKs below.
  Handle<Object> receiver = lookup->GetReceiver();
  bool receiver_is_holder = receiver.is_identical_to(holder);
#endif
  // Non-map-specific handler stubs have already been selected.
  DCHECK(!receiver->IsString() ||
         !Name::Equals(isolate()->factory()->length_string(), lookup->name()));
  DCHECK(!receiver->IsStringWrapper() ||
         !Name::Equals(isolate()->factory()->length_string(), lookup->name()));

  DCHECK(!(
      receiver->IsJSFunction() &&
      Name::Equals(isolate()->factory()->prototype_string(), lookup->name()) &&
      receiver->IsConstructor() &&
      !Handle<JSFunction>::cast(receiver)
           ->map()
           ->has_non_instance_prototype()));

  Handle<Map> map = receiver_map();
  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->getter()->IsUndefined(isolate()));
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadInterceptor);
      NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
      // Perform a lookup behind the interceptor. Copy the LookupIterator since
      // the original iterator will be used to fetch the value.
      LookupIterator it = *lookup;
      it.Next();
      LookupForRead(&it);
      return compiler.CompileLoadInterceptor(&it);
    }

    case LookupIterator::ACCESSOR: {
#ifdef DEBUG
      int object_offset;
      DCHECK(!Accessors::IsJSObjectFieldAccessor(map, lookup->name(),
                                                 &object_offset));
#endif

      DCHECK(IsCompatibleReceiver(lookup, map));
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorPair()) {
        if (lookup->TryLookupCachedProperty()) {
          DCHECK_EQ(LookupIterator::DATA, lookup->state());
          return ComputeHandler(lookup);
        }
        DCHECK(holder->HasFastProperties());
        DCHECK(!GetSharedFunctionInfo()->HasDebugInfo());
        Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                              isolate());
        CallOptimization call_optimization(getter);
        NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
        if (call_optimization.is_simple_api_call()) {
          TRACE_HANDLER_STATS(isolate(), LoadIC_LoadCallback);
          int index = lookup->GetAccessorIndex();
          Handle<Code> code = compiler.CompileLoadCallback(
              lookup->name(), call_optimization, index, slow_stub());
          return code;
        }
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadViaGetter);
        int expected_arguments = Handle<JSFunction>::cast(getter)
                                     ->shared()
                                     ->internal_formal_parameter_count();
        return compiler.CompileLoadViaGetter(
            lookup->name(), lookup->GetAccessorIndex(), expected_arguments);
      } else {
        DCHECK(accessors->IsAccessorInfo());
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        DCHECK(v8::ToCData<Address>(info->getter()) != nullptr);
        DCHECK(AccessorInfo::IsCompatibleReceiverMap(isolate(), info, map));
        DCHECK(holder->HasFastProperties());
        DCHECK(!receiver_is_holder);
        DCHECK(!info->is_sloppy() || receiver->IsJSReceiver());
        TRACE_HANDLER_STATS(isolate(), LoadIC_LoadCallback);
        NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
        Handle<Code> code =
            compiler.CompileLoadCallback(lookup->name(), info, slow_stub());
        return code;
      }
      UNREACHABLE();
    }

    case LookupIterator::DATA: {
      DCHECK(lookup->is_dictionary_holder());
      DCHECK(kind() == Code::LOAD_IC || kind() == Code::LOAD_GLOBAL_IC);
      DCHECK(holder->IsJSGlobalObject());
      TRACE_HANDLER_STATS(isolate(), LoadIC_LoadGlobal);
      NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
      Handle<PropertyCell> cell = lookup->GetPropertyCell();
      Handle<Code> code = compiler.CompileLoadGlobal(cell, lookup->name(),
                                                     lookup->IsConfigurable());
      return code;
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }
  UNREACHABLE();
  return slow_stub();
}


static Handle<Object> TryConvertKey(Handle<Object> key, Isolate* isolate) {
  // This helper implements a few common fast cases for converting
  // non-smi keys of keyed loads/stores to a smi or a string.
  if (key->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(key)->value();
    if (std::isnan(value)) {
      key = isolate->factory()->nan_string();
    } else {
      int int_value = FastD2I(value);
      if (value == int_value && Smi::IsValid(int_value)) {
        key = handle(Smi::FromInt(int_value), isolate);
      }
    }
  } else if (key->IsUndefined(isolate)) {
    key = isolate->factory()->undefined_string();
  }
  return key;
}

void KeyedLoadIC::UpdateLoadElement(Handle<HeapObject> receiver) {
  Handle<Map> receiver_map(receiver->map(), isolate());
  DCHECK(receiver_map->instance_type() != JS_VALUE_TYPE &&
         receiver_map->instance_type() != JS_PROXY_TYPE);  // Checked by caller.
  MapHandleList target_receiver_maps;
  TargetMaps(&target_receiver_maps);

  if (target_receiver_maps.length() == 0) {
    Handle<Object> handler =
        ElementHandlerCompiler::GetKeyedLoadHandler(receiver_map, isolate());
    return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
  }

  for (int i = 0; i < target_receiver_maps.length(); i++) {
    Handle<Map> map = target_receiver_maps.at(i);
    if (map.is_null()) continue;
    if (map->instance_type() == JS_VALUE_TYPE) {
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "JSValue");
      return;
    }
    if (map->instance_type() == JS_PROXY_TYPE) {
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "JSProxy");
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
      IsMoreGeneralElementsKindTransition(
          target_receiver_maps.at(0)->elements_kind(),
          Handle<JSObject>::cast(receiver)->GetElementsKind())) {
    Handle<Object> handler =
        ElementHandlerCompiler::GetKeyedLoadHandler(receiver_map, isolate());
    return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
  }

  DCHECK(state() != GENERIC);

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map)) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "same map added twice");
    return;
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "max polymorph exceeded");
    return;
  }

  List<Handle<Object>> handlers(target_receiver_maps.length());
  ElementHandlerCompiler compiler(isolate());
  compiler.CompileElementHandlers(&target_receiver_maps, &handlers);
  ConfigureVectorState(Handle<Name>(), &target_receiver_maps, &handlers);
}


MaybeHandle<Object> KeyedLoadIC::Load(Handle<Object> object,
                                      Handle<Object> key) {
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Runtime::GetObjectProperty(isolate(), object, key),
        Object);
    return result;
  }

  Handle<Object> load_handle;

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  uint32_t index;
  if ((key->IsInternalizedString() &&
       !String::cast(*key)->AsArrayIndex(&index)) ||
      key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), load_handle,
                               LoadIC::Load(object, Handle<Name>::cast(key)),
                               Object);
  } else if (FLAG_use_ic && !object->IsAccessCheckNeeded() &&
             !object->IsJSValue()) {
    if ((object->IsJSObject() && key->IsSmi()) ||
        (object->IsString() && key->IsNumber())) {
      UpdateLoadElement(Handle<HeapObject>::cast(object));
      if (is_vector_set()) {
        TRACE_IC("LoadIC", key);
      }
    }
  }

  if (!is_vector_set()) {
    ConfigureVectorState(MEGAMORPHIC, key);
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "set generic");
    TRACE_IC("LoadIC", key);
  }

  if (!load_handle.is_null()) return load_handle;

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                             Runtime::GetObjectProperty(isolate(), object, key),
                             Object);
  return result;
}


bool StoreIC::LookupForWrite(LookupIterator* it, Handle<Object> value,
                             JSReceiver::StoreFromKeyed store_mode) {
  // Disable ICs for non-JSObjects for now.
  Handle<Object> object = it->GetReceiver();
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  DCHECK(!receiver->map()->is_deprecated());

  for (; it->IsFound(); it->Next()) {
    switch (it->state()) {
      case LookupIterator::NOT_FOUND:
      case LookupIterator::TRANSITION:
        UNREACHABLE();
      case LookupIterator::JSPROXY:
        return false;
      case LookupIterator::INTERCEPTOR: {
        Handle<JSObject> holder = it->GetHolder<JSObject>();
        InterceptorInfo* info = holder->GetNamedInterceptor();
        if (it->HolderIsReceiverOrHiddenPrototype()) {
          return !info->non_masking() && receiver.is_identical_to(holder) &&
                 !info->setter()->IsUndefined(it->isolate());
        } else if (!info->getter()->IsUndefined(it->isolate()) ||
                   !info->query()->IsUndefined(it->isolate())) {
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
          PrototypeIterator iter(it->isolate(), receiver);
          return it->GetHolder<Object>().is_identical_to(
              PrototypeIterator::GetCurrent(iter));
        }

        if (it->HolderIsReceiverOrHiddenPrototype()) return false;

        if (it->ExtendingNonExtensible(receiver)) return false;
        it->PrepareTransitionToDataProperty(receiver, value, NONE, store_mode);
        return it->IsCacheableTransition();
      }
    }
  }

  receiver = it->GetStoreTarget();
  if (it->ExtendingNonExtensible(receiver)) return false;
  it->PrepareTransitionToDataProperty(receiver, value, NONE, store_mode);
  return it->IsCacheableTransition();
}


MaybeHandle<Object> StoreIC::Store(Handle<Object> object, Handle<Name> name,
                                   Handle<Object> value,
                                   JSReceiver::StoreFromKeyed store_mode) {
  if (object->IsJSGlobalObject() && name->IsString()) {
    // Look up in script context table.
    Handle<String> str_name = Handle<String>::cast(name);
    Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(object);
    Handle<ScriptContextTable> script_contexts(
        global->native_context()->script_context_table());

    ScriptContextTable::LookupResult lookup_result;
    if (ScriptContextTable::Lookup(script_contexts, str_name, &lookup_result)) {
      Handle<Context> script_context = ScriptContextTable::GetContext(
          script_contexts, lookup_result.context_index);
      if (lookup_result.mode == CONST) {
        return TypeError(MessageTemplate::kConstAssign, object, name);
      }

      Handle<Object> previous_value =
          FixedArray::get(*script_context, lookup_result.slot_index, isolate());

      if (previous_value->IsTheHole(isolate())) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        return ReferenceError(name);
      }

      if (FLAG_use_ic &&
          StoreScriptContextFieldStub::Accepted(&lookup_result)) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreScriptContextFieldStub);
        StoreScriptContextFieldStub stub(isolate(), &lookup_result);
        PatchCache(name, stub.GetCode());
      }

      script_context->set(lookup_result.slot_index, *value);
      return value;
    }
  }

  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(object) || object->IsJSProxy()) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::SetProperty(object, name, value, language_mode()), Object);
    return result;
  }

  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsNullOrUndefined(isolate())) {
    if (FLAG_use_ic && state() != UNINITIALIZED && state() != PREMONOMORPHIC) {
      // Ensure the IC state progresses.
      TRACE_HANDLER_STATS(isolate(), StoreIC_NonReceiver);
      update_receiver_map(object);
      PatchCache(name, slow_stub());
      TRACE_IC("StoreIC", name);
    }
    return TypeError(MessageTemplate::kNonObjectPropertyStore, object, name);
  }

  if (state() != UNINITIALIZED) {
    JSObject::MakePrototypesFast(object, kStartAtPrototype, isolate());
  }
  LookupIterator it(object, name);
  if (FLAG_use_ic) UpdateCaches(&it, value, store_mode);

  MAYBE_RETURN_NULL(
      Object::SetProperty(&it, value, language_mode(), store_mode));
  return value;
}

void StoreIC::UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                           JSReceiver::StoreFromKeyed store_mode) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    TRACE_HANDLER_STATS(isolate(), StoreIC_Premonomorphic);
    ConfigureVectorState(PREMONOMORPHIC, Handle<Object>());
    TRACE_IC("StoreIC", lookup->name());
    return;
  }

  bool use_ic = LookupForWrite(lookup, value, store_mode);
  if (!use_ic) {
    TRACE_GENERIC_IC(isolate(), "StoreIC", "LookupForWrite said 'false'");
  }
  Handle<Object> handler = use_ic ? ComputeHandler(lookup, value)
                                  : Handle<Object>::cast(slow_stub());

  PatchCache(lookup->name(), handler);
  TRACE_IC("StoreIC", lookup->name());
}

Handle<Object> StoreIC::StoreTransition(Handle<Map> receiver_map,
                                        Handle<JSObject> holder,
                                        Handle<Map> transition,
                                        Handle<Name> name) {
  int descriptor = transition->LastAdded();
  Handle<DescriptorArray> descriptors(transition->instance_descriptors());
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  // Declarative handlers don't support access checks.
  DCHECK(!transition->is_access_check_needed());

  Handle<Object> smi_handler;
  DCHECK_EQ(kData, details.kind());
  if (details.location() == kDescriptor) {
    smi_handler = StoreHandler::TransitionToConstant(isolate(), descriptor);

  } else {
    DCHECK_EQ(kField, details.location());
    bool extend_storage =
        Map::cast(transition->GetBackPointer())->unused_property_fields() == 0;

    FieldIndex index = FieldIndex::ForDescriptor(*transition, descriptor);
    smi_handler = StoreHandler::TransitionToField(
        isolate(), descriptor, index, representation, extend_storage);
  }
  // |holder| is either a receiver if the property is non-existent or
  // one of the prototypes.
  DCHECK(!holder.is_null());
  bool is_nonexistent = holder->map() == transition->GetBackPointer();
  if (is_nonexistent) holder = Handle<JSObject>::null();

  int checks_count =
      GetPrototypeCheckCount(isolate(), receiver_map, holder, name);
  DCHECK_LE(0, checks_count);
  DCHECK(!receiver_map->IsJSGlobalObjectMap());

  Handle<Object> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  if (validity_cell.is_null()) {
    DCHECK_EQ(0, checks_count);
    validity_cell = handle(Smi::FromInt(0), isolate());
  }

  Handle<WeakCell> transition_cell = Map::WeakCellForMap(transition);

  Factory* factory = isolate()->factory();
  if (checks_count == 0) {
    return factory->NewTuple3(transition_cell, smi_handler, validity_cell);
  }
  Handle<FixedArray> handler_array(factory->NewFixedArray(
      StoreHandler::kFirstPrototypeIndex + checks_count, TENURED));
  handler_array->set(StoreHandler::kSmiHandlerIndex, *smi_handler);
  handler_array->set(StoreHandler::kValidityCellIndex, *validity_cell);
  handler_array->set(StoreHandler::kTransitionCellIndex, *transition_cell);
  InitPrototypeChecks(isolate(), receiver_map, holder, name, handler_array,
                      StoreHandler::kFirstPrototypeIndex);
  return handler_array;
}

static Handle<Code> PropertyCellStoreHandler(
    Isolate* isolate, Handle<JSObject> receiver, Handle<JSGlobalObject> holder,
    Handle<Name> name, Handle<PropertyCell> cell, PropertyCellType type) {
  auto constant_type = Nothing<PropertyCellConstantType>();
  if (type == PropertyCellType::kConstantType) {
    constant_type = Just(cell->GetConstantType());
  }
  StoreGlobalStub stub(isolate, type, constant_type,
                       receiver->IsJSGlobalProxy());
  auto code = stub.GetCodeCopyFromTemplate(holder, cell);
  // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
  HeapObject::UpdateMapCodeCache(receiver, name, code);
  return code;
}

Handle<Object> StoreIC::GetMapIndependentHandler(LookupIterator* lookup) {
  DCHECK_NE(LookupIterator::JSPROXY, lookup->state());

  // This is currently guaranteed by checks in StoreIC::Store.
  Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      auto store_target = lookup->GetStoreTarget();
      if (store_target->IsJSGlobalObject()) {
        break;  // Custom-compiled handler.
      }
      // Currently not handled by CompileStoreTransition.
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "transition from slow");
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        return slow_stub();
      }
      DCHECK(lookup->IsCacheableTransition());
      Handle<Map> transition = lookup->transition_map();
      TRACE_HANDLER_STATS(isolate(), StoreIC_StoreTransitionDH);
      return StoreTransition(receiver_map(), holder, transition,
                             lookup->name());
    }

    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->setter()->IsUndefined(isolate()));
      TRACE_HANDLER_STATS(isolate(), StoreIC_StoreInterceptorStub);
      StoreInterceptorStub stub(isolate());
      return stub.GetCode();
    }

    case LookupIterator::ACCESSOR: {
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "accessor on slow map");
        TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
        return slow_stub();
      }
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->setter()) == nullptr) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter == nullptr");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        if (AccessorInfo::cast(*accessors)->is_special_data_property() &&
            !lookup->HolderIsReceiverOrHiddenPrototype()) {
          TRACE_GENERIC_IC(isolate(), "StoreIC",
                           "special data property in prototype chain");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        if (!AccessorInfo::IsCompatibleReceiverMap(isolate(), info,
                                                   receiver_map())) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "incompatible receiver type");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        if (info->is_sloppy() && !receiver->IsJSReceiver()) {
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        break;  // Custom-compiled handler.
      } else if (accessors->IsAccessorPair()) {
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        if (!setter->IsJSFunction() && !setter->IsFunctionTemplateInfo()) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter not a function");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        CallOptimization call_optimization(setter);
        if (call_optimization.is_simple_api_call()) {
          if (call_optimization.IsCompatibleReceiver(receiver, holder)) {
            break;  // Custom-compiled handler.
          }
          TRACE_GENERIC_IC(isolate(), "StoreIC", "incompatible receiver");
          TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
          return slow_stub();
        }
        break;  // Custom-compiled handler.
      }
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return slow_stub();
    }

    case LookupIterator::DATA: {
      DCHECK_EQ(kData, lookup->property_details().kind());
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject()) {
          break;  // Custom-compiled handler.
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreNormal);
        DCHECK(holder.is_identical_to(receiver));
        return isolate()->builtins()->StoreIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().location() == kField) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreFieldDH);
        int descriptor = lookup->GetFieldDescriptorIndex();
        FieldIndex index = lookup->GetFieldIndex();
        return StoreHandler::StoreField(isolate(), descriptor, index,
                                        lookup->representation());
      }

      // -------------- Constant properties --------------
      DCHECK_EQ(kDescriptor, lookup->property_details().location());
      TRACE_GENERIC_IC(isolate(), "StoreIC", "constant property");
      TRACE_HANDLER_STATS(isolate(), StoreIC_SlowStub);
      return slow_stub();
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
      UNREACHABLE();
  }
  return Handle<Code>::null();
}

Handle<Object> StoreIC::CompileHandler(LookupIterator* lookup,
                                       Handle<Object> value,
                                       CacheHolderFlag cache_holder) {
  DCHECK_NE(LookupIterator::JSPROXY, lookup->state());

  // This is currently guaranteed by checks in StoreIC::Store.
  Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  DCHECK(!receiver->IsAccessCheckNeeded() || lookup->name()->IsPrivate());

  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      auto store_target = lookup->GetStoreTarget();
      if (store_target->IsJSGlobalObject()) {
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobalTransition);
        Handle<PropertyCell> cell = lookup->transition_cell();
        cell->set_value(*value);
        Handle<Code> code = PropertyCellStoreHandler(
            isolate(), store_target, Handle<JSGlobalObject>::cast(store_target),
            lookup->name(), cell, PropertyCellType::kConstant);
        cell->set_value(isolate()->heap()->the_hole_value());
        return code;
      }
      UNREACHABLE();
    }

    case LookupIterator::INTERCEPTOR:
      UNREACHABLE();

    case LookupIterator::ACCESSOR: {
      DCHECK(holder->HasFastProperties());
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        DCHECK(v8::ToCData<Address>(info->setter()) != 0);
        DCHECK(!AccessorInfo::cast(*accessors)->is_special_data_property() ||
               lookup->HolderIsReceiverOrHiddenPrototype());
        DCHECK(AccessorInfo::IsCompatibleReceiverMap(isolate(), info,
                                                     receiver_map()));
        DCHECK(!info->is_sloppy() || receiver->IsJSReceiver());
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreCallback);
        NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
        Handle<Code> code = compiler.CompileStoreCallback(
            receiver, lookup->name(), info, language_mode());
        return code;
      } else {
        DCHECK(accessors->IsAccessorPair());
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        DCHECK(setter->IsJSFunction() || setter->IsFunctionTemplateInfo());
        CallOptimization call_optimization(setter);
        NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
        if (call_optimization.is_simple_api_call()) {
          DCHECK(call_optimization.IsCompatibleReceiver(receiver, holder));
          TRACE_HANDLER_STATS(isolate(), StoreIC_StoreCallback);
          Handle<Code> code = compiler.CompileStoreCallback(
              receiver, lookup->name(), call_optimization,
              lookup->GetAccessorIndex(), slow_stub());
          return code;
        }
        TRACE_HANDLER_STATS(isolate(), StoreIC_StoreViaSetter);
        int expected_arguments = JSFunction::cast(*setter)
                                     ->shared()
                                     ->internal_formal_parameter_count();
        return compiler.CompileStoreViaSetter(receiver, lookup->name(),
                                              lookup->GetAccessorIndex(),
                                              expected_arguments);
      }
    }

    case LookupIterator::DATA: {
      DCHECK(lookup->is_dictionary_holder());
      DCHECK(holder->IsJSGlobalObject());
      TRACE_HANDLER_STATS(isolate(), StoreIC_StoreGlobal);
      DCHECK(holder.is_identical_to(receiver) ||
             receiver->map()->prototype() == *holder);
      auto cell = lookup->GetPropertyCell();
      auto updated_type =
          PropertyCell::UpdatedType(cell, value, lookup->property_details());
      auto code = PropertyCellStoreHandler(isolate(), receiver,
                                           Handle<JSGlobalObject>::cast(holder),
                                           lookup->name(), cell, updated_type);
      return code;
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
      UNREACHABLE();
  }
  UNREACHABLE();
  return slow_stub();
}

void KeyedStoreIC::UpdateStoreElement(Handle<Map> receiver_map,
                                      KeyedAccessStoreMode store_mode) {
  MapHandleList target_receiver_maps;
  TargetMaps(&target_receiver_maps);
  if (target_receiver_maps.length() == 0) {
    Handle<Map> monomorphic_map =
        ComputeTransitionedMap(receiver_map, store_mode);
    store_mode = GetNonTransitioningStoreMode(store_mode);
    Handle<Object> handler =
        PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(monomorphic_map,
                                                                store_mode);
    return ConfigureVectorState(Handle<Name>(), monomorphic_map, handler);
  }

  for (int i = 0; i < target_receiver_maps.length(); i++) {
    if (!target_receiver_maps.at(i).is_null() &&
        target_receiver_maps.at(i)->instance_type() == JS_VALUE_TYPE) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "JSValue");
      return;
    }
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different GetNonTransitioningStoreMode IC that handles a
  // superset of the original IC. Handle those here if the receiver map hasn't
  // changed or it has transitioned to a more general kind.
  KeyedAccessStoreMode old_store_mode = GetKeyedAccessStoreMode();
  Handle<Map> previous_receiver_map = target_receiver_maps.at(0);
  if (state() == MONOMORPHIC) {
    Handle<Map> transitioned_receiver_map = receiver_map;
    if (IsTransitionStoreMode(store_mode)) {
      transitioned_receiver_map =
          ComputeTransitionedMap(receiver_map, store_mode);
    }
    if ((receiver_map.is_identical_to(previous_receiver_map) &&
         IsTransitionStoreMode(store_mode)) ||
        IsTransitionOfMonomorphicTarget(*previous_receiver_map,
                                        *transitioned_receiver_map)) {
      // If the "old" and "new" maps are in the same elements map family, or
      // if they at least come from the same origin for a transitioning store,
      // stay MONOMORPHIC and use the map for the most generic ElementsKind.
      store_mode = GetNonTransitioningStoreMode(store_mode);
      Handle<Object> handler =
          PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
              transitioned_receiver_map, store_mode);
      ConfigureVectorState(Handle<Name>(), transitioned_receiver_map, handler);
      return;
    }
    if (receiver_map.is_identical_to(previous_receiver_map) &&
        old_store_mode == STANDARD_STORE &&
        (store_mode == STORE_AND_GROW_NO_TRANSITION ||
         store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
         store_mode == STORE_NO_TRANSITION_HANDLE_COW)) {
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      Handle<Object> handler =
          PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(receiver_map,
                                                                  store_mode);
      return ConfigureVectorState(Handle<Name>(), receiver_map, handler);
    }
  }

  DCHECK(state() != GENERIC);

  bool map_added =
      AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map);

  if (IsTransitionStoreMode(store_mode)) {
    Handle<Map> transitioned_receiver_map =
        ComputeTransitionedMap(receiver_map, store_mode);
    map_added |= AddOneReceiverMapIfMissing(&target_receiver_maps,
                                            transitioned_receiver_map);
  }

  if (!map_added) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the megamorphic stub which can handle everything.
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "same map added twice");
    return;
  }

  // If the maximum number of receiver maps has been exceeded, use the
  // megamorphic version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) return;

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // megamorphic stub must be used.
  store_mode = GetNonTransitioningStoreMode(store_mode);
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "store mode mismatch");
      return;
    }
  }

  // If the store mode isn't the standard mode, make sure that all polymorphic
  // receivers are either external arrays, or all "normal" arrays. Otherwise,
  // use the megamorphic stub.
  if (store_mode != STANDARD_STORE) {
    int external_arrays = 0;
    for (int i = 0; i < target_receiver_maps.length(); ++i) {
      if (target_receiver_maps[i]->has_fixed_typed_array_elements()) {
        external_arrays++;
      }
    }
    if (external_arrays != 0 &&
        external_arrays != target_receiver_maps.length()) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                       "unsupported combination of external and normal arrays");
      return;
    }
  }

  MapHandleList transitioned_maps(target_receiver_maps.length());
  List<Handle<Object>> handlers(target_receiver_maps.length());
  PropertyICCompiler::ComputeKeyedStorePolymorphicHandlers(
      &target_receiver_maps, &transitioned_maps, &handlers, store_mode);
  ConfigureVectorState(&target_receiver_maps, &transitioned_maps, &handlers);
}


Handle<Map> KeyedStoreIC::ComputeTransitionedMap(
    Handle<Map> map, KeyedAccessStoreMode store_mode) {
  switch (store_mode) {
    case STORE_TRANSITION_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_TO_OBJECT: {
      ElementsKind kind = IsFastHoleyElementsKind(map->elements_kind())
                              ? FAST_HOLEY_ELEMENTS
                              : FAST_ELEMENTS;
      return Map::TransitionElementsTo(map, kind);
    }
    case STORE_TRANSITION_TO_DOUBLE:
    case STORE_AND_GROW_TRANSITION_TO_DOUBLE: {
      ElementsKind kind = IsFastHoleyElementsKind(map->elements_kind())
                              ? FAST_HOLEY_DOUBLE_ELEMENTS
                              : FAST_DOUBLE_ELEMENTS;
      return Map::TransitionElementsTo(map, kind);
    }
    case STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS:
      DCHECK(map->has_fixed_typed_array_elements());
    // Fall through
    case STORE_NO_TRANSITION_HANDLE_COW:
    case STANDARD_STORE:
    case STORE_AND_GROW_NO_TRANSITION:
      return map;
  }
  UNREACHABLE();
  return MaybeHandle<Map>().ToHandleChecked();
}


bool IsOutOfBoundsAccess(Handle<JSObject> receiver, uint32_t index) {
  uint32_t length = 0;
  if (receiver->IsJSArray()) {
    JSArray::cast(*receiver)->length()->ToArrayLength(&length);
  } else {
    length = static_cast<uint32_t>(receiver->elements()->length());
  }
  return index >= length;
}


static KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver,
                                         uint32_t index, Handle<Object> value) {
  bool oob_access = IsOutOfBoundsAccess(receiver, index);
  // Don't consider this a growing store if the store would send the receiver to
  // dictionary mode.
  bool allow_growth = receiver->IsJSArray() && oob_access &&
                      !receiver->WouldConvertToSlowElements(index);
  if (allow_growth) {
    // Handle growing array in stub if necessary.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        return STORE_AND_GROW_TRANSITION_TO_DOUBLE;
      }
      if (value->IsHeapObject()) {
        return STORE_AND_GROW_TRANSITION_TO_OBJECT;
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        return STORE_AND_GROW_TRANSITION_TO_OBJECT;
      }
    }
    return STORE_AND_GROW_NO_TRANSITION;
  } else {
    // Handle only in-bounds elements accesses.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        return STORE_TRANSITION_TO_DOUBLE;
      } else if (value->IsHeapObject()) {
        return STORE_TRANSITION_TO_OBJECT;
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        return STORE_TRANSITION_TO_OBJECT;
      }
    }
    if (!FLAG_trace_external_array_abuse &&
        receiver->map()->has_fixed_typed_array_elements() && oob_access) {
      return STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS;
    }
    Heap* heap = receiver->GetHeap();
    if (receiver->elements()->map() == heap->fixed_cow_array_map()) {
      return STORE_NO_TRANSITION_HANDLE_COW;
    } else {
      return STANDARD_STORE;
    }
  }
}


MaybeHandle<Object> KeyedStoreIC::Store(Handle<Object> object,
                                        Handle<Object> key,
                                        Handle<Object> value) {
  // TODO(verwaest): Let SetProperty do the migration, since storing a property
  // might deprecate the current map again, if value does not fit.
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Runtime::SetObjectProperty(isolate(), object, key,
                                                      value, language_mode()),
        Object);
    return result;
  }

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  Handle<Object> store_handle;

  uint32_t index;
  if ((key->IsInternalizedString() &&
       !String::cast(*key)->AsArrayIndex(&index)) ||
      key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), store_handle,
        StoreIC::Store(object, Handle<Name>::cast(key), value,
                       JSReceiver::MAY_BE_STORE_FROM_KEYED),
        Object);
    if (!is_vector_set()) {
      ConfigureVectorState(MEGAMORPHIC, key);
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                       "unhandled internalized string key");
      TRACE_IC("StoreIC", key);
    }
    return store_handle;
  }

  bool use_ic = FLAG_use_ic && !object->IsStringWrapper() &&
                !object->IsAccessCheckNeeded() && !object->IsJSGlobalProxy();
  if (use_ic && !object->IsSmi()) {
    // Don't use ICs for maps of the objects in Array's prototype chain. We
    // expect to be able to trap element sets to objects with those maps in
    // the runtime to enable optimization of element hole access.
    Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
    if (heap_object->map()->IsMapInArrayPrototypeChain()) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "map in array prototype");
      use_ic = false;
    }
  }

  Handle<Map> old_receiver_map;
  bool is_arguments = false;
  bool key_is_valid_index = false;
  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (use_ic && object->IsJSObject()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    old_receiver_map = handle(receiver->map(), isolate());
    is_arguments = receiver->IsJSArgumentsObject();
    if (!is_arguments) {
      key_is_valid_index = key->IsSmi() && Smi::cast(*key)->value() >= 0;
      if (key_is_valid_index) {
        uint32_t index = static_cast<uint32_t>(Smi::cast(*key)->value());
        store_mode = GetStoreMode(receiver, index, value);
      }
    }
  }

  DCHECK(store_handle.is_null());
  ASSIGN_RETURN_ON_EXCEPTION(isolate(), store_handle,
                             Runtime::SetObjectProperty(isolate(), object, key,
                                                        value, language_mode()),
                             Object);

  if (use_ic) {
    if (!old_receiver_map.is_null()) {
      if (is_arguments) {
        TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "arguments receiver");
      } else if (key_is_valid_index) {
        // We should go generic if receiver isn't a dictionary, but our
        // prototype chain does have dictionary elements. This ensures that
        // other non-dictionary receivers in the polymorphic case benefit
        // from fast path keyed stores.
        if (!old_receiver_map->DictionaryElementsInPrototypeChainOnly()) {
          UpdateStoreElement(old_receiver_map, store_mode);
        } else {
          TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                           "dictionary or proxy prototype");
        }
      } else {
        TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-smi-like key");
      }
    } else {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-JSObject receiver");
    }
  }

  if (!is_vector_set()) {
    ConfigureVectorState(MEGAMORPHIC, key);
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "set generic");
  }
  TRACE_IC("StoreIC", key);

  return store_handle;
}


void CallIC::HandleMiss(Handle<Object> function) {
  Handle<Object> name = isolate()->factory()->empty_string();
  CallICNexus* nexus = casted_nexus<CallICNexus>();
  Object* feedback = nexus->GetFeedback();

  // Hand-coded MISS handling is easier if CallIC slots don't contain smis.
  DCHECK(!feedback->IsSmi());

  if (feedback->IsWeakCell() || !function->IsJSFunction() ||
      feedback->IsAllocationSite()) {
    // We are going generic.
    nexus->ConfigureMegamorphic();
  } else {
    DCHECK(feedback == *FeedbackVector::UninitializedSentinel(isolate()));
    Handle<JSFunction> js_function = Handle<JSFunction>::cast(function);

    Handle<JSFunction> array_function =
        Handle<JSFunction>(isolate()->native_context()->array_function());
    if (array_function.is_identical_to(js_function)) {
      // Alter the slot.
      nexus->ConfigureMonomorphicArray();
    } else if (js_function->context()->native_context() !=
               *isolate()->native_context()) {
      // Don't collect cross-native context feedback for the CallIC.
      // TODO(bmeurer): We should collect the SharedFunctionInfo as
      // feedback in this case instead.
      nexus->ConfigureMegamorphic();
    } else {
      nexus->ConfigureMonomorphic(js_function);
    }
  }

  if (function->IsJSFunction()) {
    Handle<JSFunction> js_function = Handle<JSFunction>::cast(function);
    name = handle(js_function->shared()->name(), isolate());
  }

  OnTypeFeedbackChanged(isolate(), get_host());
  TRACE_IC("CallIC", name);
}


#undef TRACE_IC


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_CallIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> function = args.at(0);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(1);
  Handle<Smi> slot = args.at<Smi>(2);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  CallICNexus nexus(vector, vector_slot);
  CallIC ic(isolate, &nexus);
  ic.HandleMiss(function);
  return *function;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_LoadIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  // A monomorphic or polymorphic KeyedLoadIC with a string key can call the
  // LoadIC miss handler if the handler misses. Since the vector Nexus is
  // set up outside the IC, handle that here.
  FeedbackVectorSlotKind kind = vector->GetKind(vector_slot);
  if (kind == FeedbackVectorSlotKind::LOAD_IC) {
    LoadICNexus nexus(vector, vector_slot);
    LoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));

  } else if (kind == FeedbackVectorSlotKind::LOAD_GLOBAL_IC) {
    DCHECK_EQ(*isolate->global_object(), *receiver);
    LoadGlobalICNexus nexus(vector, vector_slot);
    LoadGlobalIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(key));

  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC, kind);
    KeyedLoadICNexus nexus(vector, vector_slot);
    KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
  }
}

// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_LoadGlobalIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<String> name = args.at<String>(0);
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());

  LoadGlobalICNexus nexus(vector, vector_slot);
  LoadGlobalIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  ic.UpdateState(global, name);

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(name));
  return *result;
}

RUNTIME_FUNCTION(Runtime_LoadGlobalIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);

  Handle<Context> native_context = isolate->native_context();
  Handle<ScriptContextTable> script_contexts(
      native_context->script_context_table());

  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(script_contexts, name, &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        script_contexts, lookup_result.context_index);
    Handle<Object> result =
        FixedArray::get(*script_context, lookup_result.slot_index, isolate);
    if (*result == isolate->heap()->the_hole_value()) {
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
    LoadICNexus nexus(isolate);
    LoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    // It is actually a LoadGlobalICs here but the predicate handles this case
    // properly.
    if (ic.ShouldThrowReferenceError()) {
      THROW_NEW_ERROR_RETURN_FAILURE(
          isolate, NewReferenceError(MessageTemplate::kNotDefined, name));
    }
  }
  return *result;
}

// Used from ic-<arch>.cc
RUNTIME_FUNCTION(Runtime_KeyedLoadIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> receiver = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedLoadICNexus nexus(vector, vector_slot);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
}


RUNTIME_FUNCTION(Runtime_KeyedLoadIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  typedef LoadWithVectorDescriptor Descriptor;
  DCHECK_EQ(Descriptor::kParameterCount, args.length());
  Handle<Object> receiver = args.at(Descriptor::kReceiver);
  Handle<Object> key = args.at(Descriptor::kName);
  Handle<Smi> slot = args.at<Smi>(Descriptor::kSlot);
  Handle<FeedbackVector> vector =
      args.at<FeedbackVector>(Descriptor::kVector);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedLoadICNexus nexus(vector, vector_slot);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Load(receiver, key));
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_StoreIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Name> key = args.at<Name>(4);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  if (vector->GetKind(vector_slot) == FeedbackVectorSlotKind::STORE_IC) {
    StoreICNexus nexus(vector, vector_slot);
    StoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC,
              vector->GetKind(vector_slot));
    KeyedStoreICNexus nexus(vector, vector_slot);
    KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
  }
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  Handle<Smi> slot = args.at<Smi>(1);
  Handle<FeedbackVector> vector = args.at<FeedbackVector>(2);
  Handle<Object> receiver = args.at(3);
  Handle<Object> key = args.at(4);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedStoreICNexus nexus(vector, vector_slot);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  RETURN_RESULT_OR_FAILURE(isolate, ic.Store(receiver, key, value));
}


RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK_EQ(5, args.length());
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> value = args.at(0);
  // slot and vector parameters are not used.
  Handle<Object> object = args.at(3);
  Handle<Object> key = args.at(4);
  LanguageMode language_mode;
  KeyedStoreICNexus nexus(isolate);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  language_mode = ic.language_mode();
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
}


RUNTIME_FUNCTION(Runtime_ElementsTransitionAndStoreIC_Miss) {
  HandleScope scope(isolate);
  // Runtime functions don't follow the IC's calling convention.
  Handle<Object> object = args.at(0);
  Handle<Object> key = args.at(1);
  Handle<Object> value = args.at(2);
  Handle<Map> map = args.at<Map>(3);
  LanguageMode language_mode;
  KeyedStoreICNexus nexus(isolate);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  language_mode = ic.language_mode();
  if (object->IsJSObject()) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object),
                                     map->elements_kind());
  }
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
}


MaybeHandle<Object> BinaryOpIC::Transition(
    Handle<AllocationSite> allocation_site, Handle<Object> left,
    Handle<Object> right) {
  BinaryOpICState state(isolate(), extra_ic_state());

  // Compute the actual result using the builtin for the binary operation.
  Handle<Object> result;
  switch (state.op()) {
    default:
      UNREACHABLE();
    case Token::ADD:
      ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                                 Object::Add(isolate(), left, right), Object);
      break;
    case Token::SUB:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::Subtract(isolate(), left, right), Object);
      break;
    case Token::MUL:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::Multiply(isolate(), left, right), Object);
      break;
    case Token::DIV:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::Divide(isolate(), left, right), Object);
      break;
    case Token::MOD:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::Modulus(isolate(), left, right), Object);
      break;
    case Token::BIT_OR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::BitwiseOr(isolate(), left, right), Object);
      break;
    case Token::BIT_AND:
      ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                                 Object::BitwiseAnd(isolate(), left, right),
                                 Object);
      break;
    case Token::BIT_XOR:
      ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                                 Object::BitwiseXor(isolate(), left, right),
                                 Object);
      break;
    case Token::SAR:
      ASSIGN_RETURN_ON_EXCEPTION(isolate(), result,
                                 Object::ShiftRight(isolate(), left, right),
                                 Object);
      break;
    case Token::SHR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::ShiftRightLogical(isolate(), left, right),
          Object);
      break;
    case Token::SHL:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result, Object::ShiftLeft(isolate(), left, right), Object);
      break;
  }

  // Do not try to update the target if the code was marked for lazy
  // deoptimization. (Since we do not relocate addresses in these
  // code objects, an attempt to access the target could fail.)
  if (AddressIsDeoptimizedCode()) {
    return result;
  }

  // Compute the new state.
  BinaryOpICState old_state(isolate(), target()->extra_ic_state());
  state.Update(left, right, result);

  // Check if we have a string operation here.
  Handle<Code> new_target;
  if (!allocation_site.is_null() || state.ShouldCreateAllocationMementos()) {
    // Setup the allocation site on-demand.
    if (allocation_site.is_null()) {
      allocation_site = isolate()->factory()->NewAllocationSite();
    }

    // Install the stub with an allocation site.
    BinaryOpICWithAllocationSiteStub stub(isolate(), state);
    new_target = stub.GetCodeCopyFromTemplate(allocation_site);

    // Sanity check the trampoline stub.
    DCHECK_EQ(*allocation_site, new_target->FindFirstAllocationSite());
  } else {
    // Install the generic stub.
    BinaryOpICStub stub(isolate(), state);
    new_target = stub.GetCode();

    // Sanity check the generic stub.
    DCHECK_NULL(new_target->FindFirstAllocationSite());
  }
  set_target(*new_target);

  if (FLAG_ic_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    auto ic_stats = ICStats::instance();
    ic_stats->Begin();
    ICInfo& ic_info = ic_stats->Current();
    ic_info.type = "BinaryOpIC";
    ic_info.state = old_state.ToString();
    ic_info.state += " => ";
    ic_info.state += state.ToString();
    JavaScriptFrame::CollectTopFrameForICStats(isolate());
    ic_stats->End();
  } else if (FLAG_ic_stats) {
    // if (FLAG_trace_ic) {
    OFStream os(stdout);
    os << "[BinaryOpIC" << old_state << " => " << state << " @ "
       << static_cast<void*>(*new_target) << " <- ";
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    if (!allocation_site.is_null()) {
      os << " using allocation site " << static_cast<void*>(*allocation_site);
    }
    os << "]" << std::endl;
  }

  // Patch the inlined smi code as necessary.
  if (!old_state.UseInlinedSmiCode() && state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(isolate(), address(), ENABLE_INLINED_SMI_CHECK);
  } else if (old_state.UseInlinedSmiCode() && !state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(isolate(), address(), DISABLE_INLINED_SMI_CHECK);
  }

  return result;
}


RUNTIME_FUNCTION(Runtime_BinaryOpIC_Miss) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  typedef BinaryOpDescriptor Descriptor;
  Handle<Object> left = args.at(Descriptor::kLeft);
  Handle<Object> right = args.at(Descriptor::kRight);
  BinaryOpIC ic(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, ic.Transition(Handle<AllocationSite>::null(), left, right));
}


RUNTIME_FUNCTION(Runtime_BinaryOpIC_MissWithAllocationSite) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  typedef BinaryOpWithAllocationSiteDescriptor Descriptor;
  Handle<AllocationSite> allocation_site =
      args.at<AllocationSite>(Descriptor::kAllocationSite);
  Handle<Object> left = args.at(Descriptor::kLeft);
  Handle<Object> right = args.at(Descriptor::kRight);
  BinaryOpIC ic(isolate);
  RETURN_RESULT_OR_FAILURE(isolate,
                           ic.Transition(allocation_site, left, right));
}

Code* CompareIC::GetRawUninitialized(Isolate* isolate, Token::Value op) {
  CompareICStub stub(isolate, op, CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED);
  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code));
  return code;
}

Code* CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope(isolate());
  CompareICStub old_stub(target()->stub_key(), isolate());
  CompareICState::State new_left =
      CompareICState::NewInputState(old_stub.left(), x);
  CompareICState::State new_right =
      CompareICState::NewInputState(old_stub.right(), y);
  CompareICState::State state = CompareICState::TargetState(
      isolate(), old_stub.state(), old_stub.left(), old_stub.right(), op_,
      HasInlinedSmiCode(address()), x, y);
  CompareICStub stub(isolate(), op_, new_left, new_right, state);
  if (state == CompareICState::KNOWN_RECEIVER) {
    stub.set_known_map(
        Handle<Map>(Handle<JSReceiver>::cast(x)->map(), isolate()));
  }
  Handle<Code> new_target = stub.GetCode();
  set_target(*new_target);

  if (FLAG_ic_stats &
      v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING) {
    auto ic_stats = ICStats::instance();
    ic_stats->Begin();
    ICInfo& ic_info = ic_stats->Current();
    ic_info.type = "CompareIC";
    JavaScriptFrame::CollectTopFrameForICStats(isolate());
    ic_info.state = "((";
    ic_info.state += CompareICState::GetStateName(old_stub.left());
    ic_info.state += "+";
    ic_info.state += CompareICState::GetStateName(old_stub.right());
    ic_info.state += "=";
    ic_info.state += CompareICState::GetStateName(old_stub.state());
    ic_info.state += ")->(";
    ic_info.state += CompareICState::GetStateName(new_left);
    ic_info.state += "+";
    ic_info.state += CompareICState::GetStateName(new_right);
    ic_info.state += "=";
    ic_info.state += CompareICState::GetStateName(state);
    ic_info.state += "))#";
    ic_info.state += Token::Name(op_);
    ic_stats->End();
  } else if (FLAG_ic_stats) {
    // if (FLAG_trace_ic) {
    PrintF("[CompareIC in ");
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    PrintF(" ((%s+%s=%s)->(%s+%s=%s))#%s @ %p]\n",
           CompareICState::GetStateName(old_stub.left()),
           CompareICState::GetStateName(old_stub.right()),
           CompareICState::GetStateName(old_stub.state()),
           CompareICState::GetStateName(new_left),
           CompareICState::GetStateName(new_right),
           CompareICState::GetStateName(state), Token::Name(op_),
           static_cast<void*>(*stub.GetCode()));
  }

  // Activate inlined smi code.
  if (old_stub.state() == CompareICState::UNINITIALIZED) {
    PatchInlinedSmiCode(isolate(), address(), ENABLE_INLINED_SMI_CHECK);
  }

  return *new_target;
}


// Used from CompareICStub::GenerateMiss in code-stubs-<arch>.cc.
RUNTIME_FUNCTION(Runtime_CompareIC_Miss) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  return ic.UpdateCaches(args.at(0), args.at(1));
}


RUNTIME_FUNCTION(Runtime_Unreachable) {
  UNREACHABLE();
  CHECK(false);
  return isolate->heap()->undefined_value();
}


Handle<Object> ToBooleanIC::ToBoolean(Handle<Object> object) {
  ToBooleanICStub stub(isolate(), extra_ic_state());
  bool to_boolean_value = stub.UpdateStatus(object);
  Handle<Code> code = stub.GetCode();
  set_target(*code);
  return isolate()->factory()->ToBoolean(to_boolean_value);
}


RUNTIME_FUNCTION(Runtime_ToBooleanIC_Miss) {
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  Handle<Object> object = args.at(0);
  ToBooleanIC ic(isolate);
  return *ic.ToBoolean(object);
}


RUNTIME_FUNCTION(Runtime_StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<HeapObject> callback_or_cell = args.at<HeapObject>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at(4);
  CONVERT_LANGUAGE_MODE_ARG_CHECKED(language_mode, 5);
  HandleScope scope(isolate);

  if (V8_UNLIKELY(FLAG_runtime_stats)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, Runtime::SetObjectProperty(isolate, receiver, name, value,
                                            language_mode));
  }

  Handle<AccessorInfo> callback(
      callback_or_cell->IsWeakCell()
          ? AccessorInfo::cast(WeakCell::cast(*callback_or_cell)->value())
          : AccessorInfo::cast(*callback_or_cell));

  DCHECK(callback->IsCompatibleReceiver(*receiver));

  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorNameSetterCallback fun =
      FUNCTION_CAST<v8::AccessorNameSetterCallback>(setter_address);
  DCHECK(fun != NULL);

  Object::ShouldThrow should_throw =
      is_sloppy(language_mode) ? Object::DONT_THROW : Object::THROW_ON_ERROR;
  PropertyCallbackArguments custom_args(isolate, callback->data(), *receiver,
                                        *holder, should_throw);
  custom_args.Call(fun, name, value);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return *value;
}


/**
 * Attempts to load a property with an interceptor (which must be present),
 * but doesn't search the prototype chain.
 *
 * Returns |Heap::no_interceptor_result_sentinel()| if interceptor doesn't
 * provide any value for the given name.
 */
RUNTIME_FUNCTION(Runtime_LoadPropertyWithInterceptorOnly) {
  DCHECK(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<Object> receiver =
      args.at(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);
  HandleScope scope(isolate);

  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver));
  }

  InterceptorInfo* interceptor = holder->GetNamedInterceptor();
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *holder, Object::DONT_THROW);

  v8::GenericNamedPropertyGetterCallback getter =
      v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
          interceptor->getter());
  Handle<Object> result = arguments.Call(getter, name);

  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);

  if (!result.is_null()) return *result;
  return isolate->heap()->no_interceptor_result_sentinel();
}


/**
 * Loads a property with an interceptor performing post interceptor
 * lookup if interceptor failed.
 */
RUNTIME_FUNCTION(Runtime_LoadPropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == NamedLoadHandlerCompiler::kInterceptorArgsLength);
  Handle<Name> name =
      args.at<Name>(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex);
  Handle<Object> receiver =
      args.at(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);

  if (!receiver->IsJSReceiver()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, receiver, Object::ConvertReceiver(isolate, receiver));
  }

  InterceptorInfo* interceptor = holder->GetNamedInterceptor();
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *holder, Object::DONT_THROW);

  v8::GenericNamedPropertyGetterCallback getter =
      v8::ToCData<v8::GenericNamedPropertyGetterCallback>(
          interceptor->getter());
  Handle<Object> result = arguments.Call(getter, name);

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

  LoadICNexus nexus(isolate);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  // It could actually be any kind of LoadICs here but the predicate handles
  // all the cases properly.
  if (!ic.ShouldThrowReferenceError()) {
    return isolate->heap()->undefined_value();
  }

  // Throw a reference error.
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewReferenceError(MessageTemplate::kNotDefined, it.name()));
}


RUNTIME_FUNCTION(Runtime_StorePropertyWithInterceptor) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  StoreICNexus nexus(isolate);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<Name> name = args.at<Name>(1);
  Handle<Object> value = args.at(2);

  DCHECK(receiver->HasNamedInterceptor());
  InterceptorInfo* interceptor = receiver->GetNamedInterceptor();
  DCHECK(!interceptor->non_masking());
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *receiver, Object::DONT_THROW);

  v8::GenericNamedPropertySetterCallback setter =
      v8::ToCData<v8::GenericNamedPropertySetterCallback>(
          interceptor->setter());
  Handle<Object> result = arguments.Call(setter, name, value);
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

  MAYBE_RETURN(Object::SetProperty(&it, value, ic.language_mode(),
                                   JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED),
               isolate->heap()->exception());
  return *value;
}


RUNTIME_FUNCTION(Runtime_LoadElementWithInterceptor) {
  // TODO(verwaest): This should probably get the holder and receiver as input.
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  DCHECK(args.smi_at(1) >= 0);
  uint32_t index = args.smi_at(1);

  InterceptorInfo* interceptor = receiver->GetIndexedInterceptor();
  PropertyCallbackArguments arguments(isolate, interceptor->data(), *receiver,
                                      *receiver, Object::DONT_THROW);

  v8::IndexedPropertyGetterCallback getter =
      v8::ToCData<v8::IndexedPropertyGetterCallback>(interceptor->getter());
  Handle<Object> result = arguments.Call(getter, index);

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
}  // namespace internal
}  // namespace v8
