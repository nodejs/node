// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/ic.h"

#include "src/accessors.h"
#include "src/api.h"
#include "src/arguments.h"
#include "src/base/bits.h"
#include "src/codegen.h"
#include "src/conversions.h"
#include "src/execution.h"
#include "src/frames-inl.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler.h"
#include "src/prototype.h"
#include "src/runtime/runtime.h"

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
    case PROTOTYPE_FAILURE:
      return '^';
    case POLYMORPHIC:
      return 'P';
    case MEGAMORPHIC:
      return 'N';
    case GENERIC:
      return 'G';

    // We never see the debugger states here, because the state is
    // computed from the original code - not the patched code. Let
    // these cases fall through to the unreachable code below.
    case DEBUG_STUB:
      break;
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
  if (FLAG_trace_ic) {
    if (AddressIsDeoptimizedCode()) return;
    State new_state =
        UseVector() ? nexus()->StateFromFeedback() : raw_target()->ic_state();
    TraceIC(type, name, state(), new_state);
  }
}


void IC::TraceIC(const char* type, Handle<Object> name, State old_state,
                 State new_state) {
  if (FLAG_trace_ic) {
    Code* new_target = raw_target();
    PrintF("[%s%s in ", new_target->is_keyed_stub() ? "Keyed" : "", type);

    // TODO(jkummerow): Add support for "apply". The logic is roughly:
    // marker = [fp_ + kMarkerOffset];
    // if marker is smi and marker.value == INTERNAL and
    //     the frame's code == builtin(Builtins::kFunctionApply):
    // then print "apply from" and advance one frame

    Object* maybe_function =
        Memory::Object_at(fp_ + JavaScriptFrameConstants::kFunctionOffset);
    if (maybe_function->IsJSFunction()) {
      JSFunction* function = JSFunction::cast(maybe_function);
      JavaScriptFrame::PrintFunctionAndOffset(function, function->code(), pc(),
                                              stdout, true);
    }

    const char* modifier = "";
    if (new_target->kind() == Code::KEYED_STORE_IC) {
      KeyedAccessStoreMode mode =
          casted_nexus<KeyedStoreICNexus>()->GetKeyedAccessStoreMode();
      modifier = GetTransitionMarkModifier(mode);
    }
    PrintF(" (%c->%c%s) ", TransitionMarkFromState(old_state),
           TransitionMarkFromState(new_state), modifier);
#ifdef OBJECT_PRINT
    OFStream os(stdout);
    name->Print(os);
#else
    name->ShortPrint(stdout);
#endif
    PrintF("]\n");
  }
}


#define TRACE_IC(type, name) TraceIC(type, name)


IC::IC(FrameDepth depth, Isolate* isolate, FeedbackNexus* nexus)
    : isolate_(isolate),
      target_set_(false),
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
  fp_ = fp;
  if (FLAG_enable_embedded_constant_pool) {
    constant_pool_address_ = constant_pool;
  }
  pc_address_ = StackFrame::ResolveReturnAddressLocation(pc_address);
  target_ = handle(raw_target(), isolate);
  kind_ = target_->kind();
  state_ = UseVector() ? nexus->StateFromFeedback() : target_->ic_state();
  old_state_ = state_;
  extra_ic_state_ = target_->extra_ic_state();
}


SharedFunctionInfo* IC::GetSharedFunctionInfo() const {
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it(isolate());
  while (it.frame()->fp() != this->fp()) it.Advance();
  if (FLAG_ignition && it.frame()->type() == StackFrame::STUB) {
    // Advance over bytecode handler frame.
    // TODO(rmcilroy): Remove this once bytecode handlers don't need a frame.
    it.Advance();
  }
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


bool IC::AddressIsOptimizedCode() const {
  Code* host =
      isolate()->inner_pointer_to_code_cache()->GetCacheEntry(address())->code;
  return host->kind() == Code::OPTIMIZED_FUNCTION;
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
        if (!holder->GetNamedInterceptor()->getter()->IsUndefined()) {
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


bool IC::TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                                Handle<String> name) {
  if (!IsNameCompatibleWithPrototypeFailure(name)) return false;
  if (UseVector()) {
    maybe_handler_ = nexus()->FindHandlerForMap(receiver_map());
  } else {
    maybe_handler_ = target()->FindHandlerForMap(*receiver_map());
  }

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

  CacheHolderFlag flag;
  Handle<Map> ic_holder_map(GetICCacheHolder(receiver_map(), isolate(), &flag));

  DCHECK(flag != kCacheOnReceiver || receiver->IsJSObject());
  DCHECK(flag != kCacheOnPrototype || !receiver->IsJSReceiver());
  DCHECK(flag != kCacheOnPrototypeReceiverIsDictionary);

  if (state() == MONOMORPHIC) {
    int index = ic_holder_map->IndexInCodeCache(*name, *target());
    if (index >= 0) {
      ic_holder_map->RemoveFromCodeCache(*name, *target(), index);
    }
  }

  if (receiver->IsJSGlobalObject()) {
    Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(receiver);
    LookupIterator it(global, name, LookupIterator::OWN_SKIP_INTERCEPTOR);
    if (it.state() == LookupIterator::ACCESS_CHECK) return false;
    if (!it.IsFound()) return false;
    return it.property_details().cell_type() == PropertyCellType::kConstant;
  }

  return true;
}


bool IC::IsNameCompatibleWithPrototypeFailure(Handle<Object> name) {
  if (target()->is_keyed_stub()) {
    // Determine whether the failure is due to a name failure.
    if (!name->IsName()) return false;
    Name* stub_name =
        UseVector() ? nexus()->FindFirstName() : target()->FindFirstName();
    if (*name != stub_name) return false;
  }

  return true;
}


void IC::UpdateState(Handle<Object> receiver, Handle<Object> name) {
  update_receiver_map(receiver);
  if (!name->IsString()) return;
  if (state() != MONOMORPHIC && state() != POLYMORPHIC) return;
  if (receiver->IsUndefined() || receiver->IsNull()) return;

  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  if (TryRemoveInvalidPrototypeDependentStub(receiver,
                                             Handle<String>::cast(name))) {
    MarkPrototypeFailure(name);
    return;
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
    case PROTOTYPE_FAILURE:
    case DEBUG_STUB:
      UNREACHABLE();
  }
}


void IC::OnTypeFeedbackChanged(Isolate* isolate, Address address,
                               State old_state, State new_state,
                               bool target_remains_ic_stub) {
  Code* host =
      isolate->inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  if (host->kind() != Code::FUNCTION) return;

  if (FLAG_type_info_threshold > 0 && target_remains_ic_stub &&
      // Not all Code objects have TypeFeedbackInfo.
      host->type_feedback_info()->IsTypeFeedbackInfo()) {
    int polymorphic_delta = 0;  // "Polymorphic" here includes monomorphic.
    int generic_delta = 0;      // "Generic" here includes megamorphic.
    ComputeTypeInfoCountDelta(old_state, new_state, &polymorphic_delta,
                              &generic_delta);
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_ic_with_type_info_count(polymorphic_delta);
    info->change_ic_generic_count(generic_delta);
  }
  if (host->type_feedback_info()->IsTypeFeedbackInfo()) {
    TypeFeedbackInfo* info = TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_own_type_change_checksum();
  }
  host->set_profiler_ticks(0);
  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
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

  Isolate* isolate = target->GetHeap()->isolate();
  State old_state = UNINITIALIZED;
  State new_state = UNINITIALIZED;
  bool target_remains_ic_stub = false;
  if (old_target->is_inline_cache_stub() && target->is_inline_cache_stub()) {
    old_state = old_target->ic_state();
    new_state = target->ic_state();
    target_remains_ic_stub = true;
  }

  OnTypeFeedbackChanged(isolate, address, old_state, new_state,
                        target_remains_ic_stub);
}


void IC::Clear(Isolate* isolate, Address address, Address constant_pool) {
  Code* target = GetTargetAtAddress(address, constant_pool);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->is_debug_stub()) return;

  switch (target->kind()) {
    case Code::LOAD_IC:
    case Code::KEYED_LOAD_IC:
    case Code::STORE_IC:
    case Code::KEYED_STORE_IC:
      return;
    case Code::COMPARE_IC:
      return CompareIC::Clear(isolate, address, target, constant_pool);
    case Code::COMPARE_NIL_IC:
      return CompareNilIC::Clear(address, target, constant_pool);
    case Code::CALL_IC:  // CallICs are vector-based and cleared differently.
    case Code::BINARY_OP_IC:
    case Code::TO_BOOLEAN_IC:
      // Clearing these is tricky and does not
      // make any performance difference.
      return;
    default:
      UNREACHABLE();
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


void StoreIC::Clear(Isolate* isolate, Address address, Code* target,
                    Address constant_pool) {
  if (IsCleared(target)) return;
  Code* code = PropertyICCompiler::FindPreMonomorphic(isolate, Code::STORE_IC,
                                                      target->extra_ic_state());
  SetTargetAtAddress(address, code, constant_pool);
}


void StoreIC::Clear(Isolate* isolate, Code* host, StoreICNexus* nexus) {
  if (IsCleared(nexus)) return;
  nexus->ConfigurePremonomorphic();
  OnTypeFeedbackChanged(isolate, host);
}


void KeyedStoreIC::Clear(Isolate* isolate, Address address, Code* target,
                         Address constant_pool) {
  if (IsCleared(target)) return;
  Handle<Code> code = pre_monomorphic_stub(
      isolate, StoreICState::GetLanguageMode(target->extra_ic_state()));
  SetTargetAtAddress(address, *code, constant_pool);
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
  SetTargetAtAddress(address,
                     GetRawUninitialized(isolate, stub.op(), stub.strength()),
                     constant_pool);
  PatchInlinedSmiCode(isolate, address, DISABLE_INLINED_SMI_CHECK);
}


// static
Handle<Code> KeyedLoadIC::ChooseMegamorphicStub(Isolate* isolate,
                                                ExtraICState extra_state) {
  if (FLAG_compiled_keyed_generic_loads) {
    return KeyedLoadGenericStub(isolate, LoadICState(extra_state)).GetCode();
  } else {
    return is_strong(LoadICState::GetLanguageMode(extra_state))
               ? isolate->builtins()->KeyedLoadIC_Megamorphic_Strong()
               : isolate->builtins()->KeyedLoadIC_Megamorphic();
  }
}


static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map()->is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}


void IC::ConfigureVectorState(IC::State new_state) {
  DCHECK(UseVector());
  if (new_state == PREMONOMORPHIC) {
    nexus()->ConfigurePremonomorphic();
  } else if (new_state == MEGAMORPHIC) {
    nexus()->ConfigureMegamorphic();
  } else {
    UNREACHABLE();
  }

  vector_set_ = true;
  OnTypeFeedbackChanged(isolate(), get_host());
}


void IC::ConfigureVectorState(Handle<Name> name, Handle<Map> map,
                              Handle<Code> handler) {
  DCHECK(UseVector());
  if (kind() == Code::LOAD_IC) {
    LoadICNexus* nexus = casted_nexus<LoadICNexus>();
    nexus->ConfigureMonomorphic(map, handler);
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
                              CodeHandleList* handlers) {
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
                              CodeHandleList* handlers) {
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
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError(MessageTemplate::kNonObjectPropertyLoad, object, name);
  }

  // Check if the name is trivially convertible to an index and get
  // the element or char if so.
  uint32_t index;
  if (kind() == Code::KEYED_LOAD_IC && name->AsArrayIndex(&index)) {
    // Rewrite to the generic keyed load stub.
    if (FLAG_use_ic) {
      DCHECK(UseVector());
      ConfigureVectorState(MEGAMORPHIC);
      TRACE_IC("LoadIC", name);
      TRACE_GENERIC_IC(isolate(), "LoadIC", "name as array index");
    }
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::GetElement(isolate(), object, index, language_mode()), Object);
    return result;
  }

  bool use_ic = MigrateDeprecated(object) ? false : FLAG_use_ic;

  if (object->IsJSGlobalObject() && name->IsString()) {
    // Look up in script context table.
    Handle<String> str_name = Handle<String>::cast(name);
    Handle<JSGlobalObject> global = Handle<JSGlobalObject>::cast(object);
    Handle<ScriptContextTable> script_contexts(
        global->native_context()->script_context_table());

    ScriptContextTable::LookupResult lookup_result;
    if (ScriptContextTable::Lookup(script_contexts, str_name, &lookup_result)) {
      Handle<Object> result =
          FixedArray::get(ScriptContextTable::GetContext(
                              script_contexts, lookup_result.context_index),
                          lookup_result.slot_index);
      if (*result == *isolate()->factory()->the_hole_value()) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        return ReferenceError(name);
      }

      if (use_ic && LoadScriptContextFieldStub::Accepted(&lookup_result)) {
        LoadScriptContextFieldStub stub(isolate(), &lookup_result);
        PatchCache(name, stub.GetCode());
      }
      return result;
    }
  }

  // Named lookup in the object.
  LookupIterator it(object, name);
  LookupForRead(&it);

  if (it.IsFound() || !ShouldThrowReferenceError(object)) {
    // Update inline cache and stub cache.
    if (use_ic) UpdateCaches(&it);

    // Get the property.
    Handle<Object> result;

    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result, Object::GetProperty(&it, language_mode()), Object);
    if (it.IsFound()) {
      return result;
    } else if (!ShouldThrowReferenceError(object)) {
      LOG(isolate(), SuspectReadEvent(*name, *object));
      return result;
    }
  }
  return ReferenceError(name);
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


bool IC::UpdatePolymorphicIC(Handle<Name> name, Handle<Code> code) {
  if (!code->is_handler()) return false;
  if (target()->is_keyed_stub() && state() != PROTOTYPE_FAILURE) return false;
  Handle<Map> map = receiver_map();
  MapHandleList maps;
  CodeHandleList handlers;

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
  if (UseVector()) {
    if (!nexus()->FindHandlers(&handlers, maps.length())) return false;
  } else {
    if (!target()->FindHandlers(&handlers, maps.length())) return false;
  }

  number_of_valid_maps++;
  if (number_of_valid_maps > 1 && target()->is_keyed_stub()) return false;
  Handle<Code> ic;
  if (number_of_valid_maps == 1) {
    ConfigureVectorState(name, receiver_map(), code);
  } else {
    if (handler_to_overwrite >= 0) {
      handlers.Set(handler_to_overwrite, code);
      if (!map.is_identical_to(maps.at(handler_to_overwrite))) {
        maps.Set(handler_to_overwrite, map);
      }
    } else {
      maps.Add(map);
      handlers.Add(code);
    }

    ConfigureVectorState(name, &maps, &handlers);
  }

  if (!UseVector()) set_target(*ic);
  return true;
}


void IC::UpdateMonomorphicIC(Handle<Code> handler, Handle<Name> name) {
  DCHECK(handler->is_handler());
  ConfigureVectorState(name, receiver_map(), handler);
}


void IC::CopyICToMegamorphicCache(Handle<Name> name) {
  MapHandleList maps;
  CodeHandleList handlers;
  TargetMaps(&maps);
  if (!target()->FindHandlers(&handlers, maps.length())) return;
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
  Map* transitioned_map =
      more_general_transition
          ? source_map->LookupElementsTransitionMap(target_elements_kind)
          : NULL;

  return transitioned_map == target_map;
}


void IC::PatchCache(Handle<Name> name, Handle<Code> code) {
  switch (state()) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
      UpdateMonomorphicIC(code, name);
      break;
    case PROTOTYPE_FAILURE:
    case MONOMORPHIC:
    case POLYMORPHIC:
      if (!target()->is_keyed_stub() || state() == PROTOTYPE_FAILURE) {
        if (UpdatePolymorphicIC(name, code)) break;
        // For keyed stubs, we can't know whether old handlers were for the
        // same key.
        CopyICToMegamorphicCache(name);
      }
      if (UseVector()) {
        ConfigureVectorState(MEGAMORPHIC);
      } else {
        set_target(*megamorphic_stub());
      }
    // Fall through.
    case MEGAMORPHIC:
      UpdateMegamorphicCache(*receiver_map(), *name, *code);
      // Indicate that we've handled this case.
      if (UseVector()) {
        vector_set_ = true;
      } else {
        target_set_ = true;
      }
      break;
    case DEBUG_STUB:
      break;
    case GENERIC:
      UNREACHABLE();
      break;
  }
}


Handle<Code> LoadIC::initialize_stub(Isolate* isolate,
                                     ExtraICState extra_state) {
  return LoadICTrampolineStub(isolate, LoadICState(extra_state)).GetCode();
}


Handle<Code> LoadIC::initialize_stub_in_optimized_code(
    Isolate* isolate, ExtraICState extra_state, State initialization_state) {
  return LoadICStub(isolate, LoadICState(extra_state)).GetCode();
}


Handle<Code> KeyedLoadIC::initialize_stub(Isolate* isolate,
                                          ExtraICState extra_state) {
  return KeyedLoadICTrampolineStub(isolate, LoadICState(extra_state)).GetCode();
}


Handle<Code> KeyedLoadIC::initialize_stub_in_optimized_code(
    Isolate* isolate, State initialization_state, ExtraICState extra_state) {
  if (initialization_state != MEGAMORPHIC) {
    return KeyedLoadICStub(isolate, LoadICState(extra_state)).GetCode();
  }
  return is_strong(LoadICState::GetLanguageMode(extra_state))
             ? isolate->builtins()->KeyedLoadIC_Megamorphic_Strong()
             : isolate->builtins()->KeyedLoadIC_Megamorphic();
}


static Handle<Code> KeyedStoreICInitializeStubHelper(
    Isolate* isolate, LanguageMode language_mode,
    InlineCacheState initialization_state) {
  switch (initialization_state) {
    case UNINITIALIZED:
      return is_strict(language_mode)
                 ? isolate->builtins()->KeyedStoreIC_Initialize_Strict()
                 : isolate->builtins()->KeyedStoreIC_Initialize();
    case PREMONOMORPHIC:
      return is_strict(language_mode)
                 ? isolate->builtins()->KeyedStoreIC_PreMonomorphic_Strict()
                 : isolate->builtins()->KeyedStoreIC_PreMonomorphic();
    case MEGAMORPHIC:
      return is_strict(language_mode)
                 ? isolate->builtins()->KeyedStoreIC_Megamorphic_Strict()
                 : isolate->builtins()->KeyedStoreIC_Megamorphic();
    default:
      UNREACHABLE();
  }
  return Handle<Code>();
}


Handle<Code> KeyedStoreIC::initialize_stub(Isolate* isolate,
                                           LanguageMode language_mode,
                                           State initialization_state) {
  if (initialization_state != MEGAMORPHIC) {
    VectorKeyedStoreICTrampolineStub stub(isolate, StoreICState(language_mode));
    return stub.GetCode();
  }

  return KeyedStoreICInitializeStubHelper(isolate, language_mode,
                                          initialization_state);
}


Handle<Code> KeyedStoreIC::initialize_stub_in_optimized_code(
    Isolate* isolate, LanguageMode language_mode, State initialization_state) {
  if (initialization_state != MEGAMORPHIC) {
    VectorKeyedStoreICStub stub(isolate, StoreICState(language_mode));
    return stub.GetCode();
  }

  return KeyedStoreICInitializeStubHelper(isolate, language_mode,
                                          initialization_state);
}


Handle<Code> KeyedStoreIC::ChooseMegamorphicStub(Isolate* isolate,
                                                 ExtraICState extra_state) {
  LanguageMode mode = StoreICState::GetLanguageMode(extra_state);
  return KeyedStoreICInitializeStubHelper(isolate, mode, MEGAMORPHIC);
}


Handle<Code> LoadIC::megamorphic_stub() {
  DCHECK_EQ(Code::KEYED_LOAD_IC, kind());
  return KeyedLoadIC::ChooseMegamorphicStub(isolate(), extra_ic_state());
}


Handle<Code> LoadIC::SimpleFieldLoad(FieldIndex index) {
  LoadFieldStub stub(isolate(), index);
  return stub.GetCode();
}


bool IsCompatibleReceiver(LookupIterator* lookup, Handle<Map> receiver_map) {
  DCHECK(lookup->state() == LookupIterator::ACCESSOR);
  Isolate* isolate = lookup->isolate();
  Handle<Object> accessors = lookup->GetAccessors();
  if (accessors->IsExecutableAccessorInfo()) {
    Handle<ExecutableAccessorInfo> info =
        Handle<ExecutableAccessorInfo>::cast(accessors);
    if (info->getter() != NULL &&
        !ExecutableAccessorInfo::IsCompatibleReceiverMap(isolate, info,
                                                         receiver_map)) {
      return false;
    }
  } else if (accessors->IsAccessorPair()) {
    Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                          isolate);
    Handle<JSObject> holder = lookup->GetHolder<JSObject>();
    Handle<Object> receiver = lookup->GetReceiver();
    if (getter->IsJSFunction() && holder->HasFastProperties()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
      if (receiver->IsJSObject() || function->shared()->IsBuiltin() ||
          !is_sloppy(function->shared()->language_mode())) {
        CallOptimization call_optimization(function);
        if (call_optimization.is_simple_api_call() &&
            !call_optimization.IsCompatibleReceiverMap(receiver_map, holder)) {
          return false;
        }
      }
    }
  }
  return true;
}


void LoadIC::UpdateCaches(LookupIterator* lookup) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    ConfigureVectorState(PREMONOMORPHIC);
    TRACE_IC("LoadIC", lookup->name());
    return;
  }

  Handle<Code> code;
  if (lookup->state() == LookupIterator::JSPROXY ||
      lookup->state() == LookupIterator::ACCESS_CHECK) {
    code = slow_stub();
  } else if (!lookup->IsFound()) {
    if (kind() == Code::LOAD_IC && !is_strong(language_mode())) {
      code = NamedLoadHandlerCompiler::ComputeLoadNonexistent(lookup->name(),
                                                              receiver_map());
      // TODO(jkummerow/verwaest): Introduce a builtin that handles this case.
      if (code.is_null()) code = slow_stub();
    } else {
      code = slow_stub();
    }
  } else {
    if (lookup->state() == LookupIterator::ACCESSOR) {
      if (!IsCompatibleReceiver(lookup, receiver_map())) {
        TRACE_GENERIC_IC(isolate(), "LoadIC", "incompatible receiver type");
        code = slow_stub();
      }
    } else if (lookup->state() == LookupIterator::INTERCEPTOR) {
      // Perform a lookup behind the interceptor. Copy the LookupIterator since
      // the original iterator will be used to fetch the value.
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


void IC::UpdateMegamorphicCache(Map* map, Name* name, Code* code) {
  isolate()->stub_cache()->Set(name, map, code);
}


Handle<Code> IC::ComputeHandler(LookupIterator* lookup, Handle<Object> value) {
  bool receiver_is_holder =
      lookup->GetReceiver().is_identical_to(lookup->GetHolder<JSObject>());
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map = IC::GetHandlerCacheHolder(
      receiver_map(), receiver_is_holder, isolate(), &flag);

  Handle<Code> code = PropertyHandlerCompiler::Find(
      lookup->name(), stub_holder_map, kind(), flag,
      lookup->is_dictionary_holder() ? Code::NORMAL : Code::FAST);
  // Use the cached value if it exists, and if it is different from the
  // handler that just missed.
  if (!code.is_null()) {
    if (!maybe_handler_.is_null() &&
        !maybe_handler_.ToHandleChecked().is_identical_to(code)) {
      return code;
    }
    if (maybe_handler_.is_null()) {
      // maybe_handler_ is only populated for MONOMORPHIC and POLYMORPHIC ICs.
      // In MEGAMORPHIC case, check if the handler in the megamorphic stub
      // cache (which just missed) is different from the cached handler.
      if (state() == MEGAMORPHIC && lookup->GetReceiver()->IsHeapObject()) {
        Map* map = Handle<HeapObject>::cast(lookup->GetReceiver())->map();
        Code* megamorphic_cached_code =
            isolate()->stub_cache()->Get(*lookup->name(), map, code->flags());
        if (megamorphic_cached_code != *code) return code;
      } else {
        return code;
      }
    }
  }

  code = CompileHandler(lookup, value, flag);
  DCHECK(code->is_handler());

  // TODO(mvstanton): we'd only like to cache code on the map when it's custom
  // code compiled for this map, otherwise it's already cached in the global
  // code
  // cache. We are also guarding against installing code with flags that don't
  // match the desired CacheHolderFlag computed above, which would lead to
  // invalid lookups later.
  if (code->type() != Code::NORMAL &&
      Code::ExtractCacheHolderFromFlags(code->flags()) == flag) {
    Map::UpdateCodeCache(stub_holder_map, lookup->name(), code);
  }

  return code;
}


Handle<Code> LoadIC::CompileHandler(LookupIterator* lookup,
                                    Handle<Object> unused,
                                    CacheHolderFlag cache_holder) {
  Handle<Object> receiver = lookup->GetReceiver();
  if (receiver->IsString() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
    FieldIndex index = FieldIndex::ForInObjectOffset(String::kLengthOffset);
    return SimpleFieldLoad(index);
  }

  if (receiver->IsStringWrapper() &&
      Name::Equals(isolate()->factory()->length_string(), lookup->name())) {
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
    FunctionPrototypeStub function_prototype_stub(isolate());
    return function_prototype_stub.GetCode();
  }

  Handle<Map> map = receiver_map();
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  bool receiver_is_holder = receiver.is_identical_to(holder);
  switch (lookup->state()) {
    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->getter()->IsUndefined());
      NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
      // Perform a lookup behind the interceptor. Copy the LookupIterator since
      // the original iterator will be used to fetch the value.
      LookupIterator it = *lookup;
      it.Next();
      LookupForRead(&it);
      return compiler.CompileLoadInterceptor(&it);
    }

    case LookupIterator::ACCESSOR: {
      // Use simple field loads for some well-known callback properties.
      // The method will only return true for absolute truths based on the
      // receiver maps.
      int object_offset;
      if (Accessors::IsJSObjectFieldAccessor(map, lookup->name(),
                                             &object_offset)) {
        FieldIndex index = FieldIndex::ForInObjectOffset(object_offset, *map);
        return SimpleFieldLoad(index);
      }
      if (Accessors::IsJSArrayBufferViewFieldAccessor(map, lookup->name(),
                                                      &object_offset)) {
        FieldIndex index = FieldIndex::ForInObjectOffset(object_offset, *map);
        ArrayBufferViewLoadFieldStub stub(isolate(), index);
        return stub.GetCode();
      }

      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->getter()) == 0) break;
        if (!ExecutableAccessorInfo::IsCompatibleReceiverMap(isolate(), info,
                                                             map)) {
          // This case should be already handled in LoadIC::UpdateCaches.
          UNREACHABLE();
          break;
        }
        if (!holder->HasFastProperties()) break;
        NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
        return compiler.CompileLoadCallback(lookup->name(), info);
      }
      if (accessors->IsAccessorPair()) {
        Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (!holder->HasFastProperties()) break;
        // When debugging we need to go the slow path to flood the accessor.
        if (GetSharedFunctionInfo()->HasDebugInfo()) break;
        Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
        if (!receiver->IsJSObject() && !function->shared()->IsBuiltin() &&
            is_sloppy(function->shared()->language_mode())) {
          // Calling sloppy non-builtins with a value as the receiver
          // requires boxing.
          break;
        }
        CallOptimization call_optimization(function);
        NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
        if (call_optimization.is_simple_api_call()) {
          if (call_optimization.IsCompatibleReceiver(receiver, holder)) {
            return compiler.CompileLoadCallback(
                lookup->name(), call_optimization, lookup->GetAccessorIndex());
          } else {
            // This case should be already handled in LoadIC::UpdateCaches.
            UNREACHABLE();
          }
        }
        int expected_arguments =
            function->shared()->internal_formal_parameter_count();
        return compiler.CompileLoadViaGetter(
            lookup->name(), lookup->GetAccessorIndex(), expected_arguments);
      }
      break;
    }

    case LookupIterator::DATA: {
      if (lookup->is_dictionary_holder()) {
        if (kind() != Code::LOAD_IC) break;
        if (holder->IsJSGlobalObject()) {
          NamedLoadHandlerCompiler compiler(isolate(), map, holder,
                                            cache_holder);
          Handle<PropertyCell> cell = lookup->GetPropertyCell();
          Handle<Code> code = compiler.CompileLoadGlobal(
              cell, lookup->name(), lookup->IsConfigurable());
          // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
          CacheHolderFlag flag;
          Handle<Map> stub_holder_map =
              GetHandlerCacheHolder(map, receiver_is_holder, isolate(), &flag);
          Map::UpdateCodeCache(stub_holder_map, lookup->name(), code);
          return code;
        }
        // There is only one shared stub for loading normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the object for the stub to be
        // applicable.
        if (!receiver_is_holder) break;
        return is_strong(language_mode())
                   ? isolate()->builtins()->LoadIC_Normal_Strong()
                   : isolate()->builtins()->LoadIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().type() == DATA) {
        FieldIndex field = lookup->GetFieldIndex();
        if (receiver_is_holder) {
          return SimpleFieldLoad(field);
        }
        NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
        return compiler.CompileLoadField(lookup->name(), field);
      }

      // -------------- Constant properties --------------
      DCHECK(lookup->property_details().type() == DATA_CONSTANT);
      if (receiver_is_holder) {
        LoadConstantStub stub(isolate(), lookup->GetConstantIndex());
        return stub.GetCode();
      }
      NamedLoadHandlerCompiler compiler(isolate(), map, holder, cache_holder);
      return compiler.CompileLoadConstant(lookup->name(),
                                          lookup->GetConstantIndex());
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      return slow_stub();
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
  }

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
  } else if (key->IsUndefined()) {
    key = isolate->factory()->undefined_string();
  }
  return key;
}


Handle<Code> KeyedLoadIC::LoadElementStub(Handle<HeapObject> receiver) {
  Handle<Code> null_handle;
  Handle<Map> receiver_map(receiver->map(), isolate());
  MapHandleList target_receiver_maps;
  TargetMaps(&target_receiver_maps);


  if (target_receiver_maps.length() == 0) {
    Handle<Code> handler =
        PropertyICCompiler::ComputeKeyedLoadMonomorphicHandler(
            receiver_map, extra_ic_state());
    ConfigureVectorState(Handle<Name>::null(), receiver_map, handler);
    return null_handle;
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
    Handle<Code> handler =
        PropertyICCompiler::ComputeKeyedLoadMonomorphicHandler(
            receiver_map, extra_ic_state());
    ConfigureVectorState(Handle<Name>::null(), receiver_map, handler);
    return null_handle;
  }

  DCHECK(state() != GENERIC);

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map)) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "same map added twice");
    return megamorphic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "max polymorph exceeded");
    return megamorphic_stub();
  }

  CodeHandleList handlers(target_receiver_maps.length());
  ElementHandlerCompiler compiler(isolate());
  compiler.CompileElementHandlers(&target_receiver_maps, &handlers,
                                  language_mode());
  ConfigureVectorState(Handle<Name>::null(), &target_receiver_maps, &handlers);
  return null_handle;
}


MaybeHandle<Object> KeyedLoadIC::Load(Handle<Object> object,
                                      Handle<Object> key) {
  if (MigrateDeprecated(object)) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Runtime::GetObjectProperty(isolate(), object, key, language_mode()),
        Object);
    return result;
  }

  Handle<Object> load_handle;
  Handle<Code> stub = megamorphic_stub();

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString() || key->IsSymbol()) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate(), load_handle,
                               LoadIC::Load(object, Handle<Name>::cast(key)),
                               Object);
  } else if (FLAG_use_ic && !object->IsAccessCheckNeeded()) {
    if (object->IsJSObject() || (object->IsString() && key->IsNumber())) {
      Handle<HeapObject> receiver = Handle<HeapObject>::cast(object);
      if (object->IsString() || key->IsSmi()) stub = LoadElementStub(receiver);
    }
  }

  DCHECK(UseVector());
  if (!is_vector_set() || stub.is_null()) {
    Code* generic = *megamorphic_stub();
    if (!stub.is_null() && *stub == generic) {
      ConfigureVectorState(MEGAMORPHIC);
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "set generic");
    }

    TRACE_IC("LoadIC", key);
  }

  if (!load_handle.is_null()) return load_handle;

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate(), result,
      Runtime::GetObjectProperty(isolate(), object, key, language_mode()),
      Object);
  return result;
}


bool StoreIC::LookupForWrite(LookupIterator* it, Handle<Object> value,
                             JSReceiver::StoreFromKeyed store_mode) {
  // Disable ICs for non-JSObjects for now.
  Handle<Object> receiver = it->GetReceiver();
  if (!receiver->IsJSObject()) return false;
  DCHECK(!Handle<JSObject>::cast(receiver)->map()->is_deprecated());

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
          if (!info->setter()->IsUndefined()) return true;
        } else if (!info->getter()->IsUndefined() ||
                   !info->query()->IsUndefined()) {
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
        PrototypeIterator iter(it->isolate(), receiver);
        if (receiver->IsJSGlobalProxy()) {
          return it->GetHolder<Object>().is_identical_to(
              PrototypeIterator::GetCurrent(iter));
        }

        if (it->HolderIsReceiverOrHiddenPrototype()) return false;

        it->PrepareTransitionToDataProperty(value, NONE, store_mode);
        return it->IsCacheableTransition();
      }
    }
  }

  it->PrepareTransitionToDataProperty(value, NONE, store_mode);
  return it->IsCacheableTransition();
}


MaybeHandle<Object> StoreIC::Store(Handle<Object> object, Handle<Name> name,
                                   Handle<Object> value,
                                   JSReceiver::StoreFromKeyed store_mode) {
  // Check if the name is trivially convertible to an index and set the element.
  uint32_t index;
  if (kind() == Code::KEYED_STORE_IC && name->AsArrayIndex(&index)) {
    // Rewrite to the generic keyed store stub.
    if (FLAG_use_ic) {
      if (UseVector()) {
        ConfigureVectorState(MEGAMORPHIC);
      } else if (!AddressIsDeoptimizedCode()) {
        set_target(*megamorphic_stub());
      }
      TRACE_IC("StoreIC", name);
      TRACE_GENERIC_IC(isolate(), "StoreIC", "name as array index");
    }
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::SetElement(isolate(), object, index, value, language_mode()),
        Object);
    return result;
  }

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
          FixedArray::get(script_context, lookup_result.slot_index);

      if (*previous_value == *isolate()->factory()->the_hole_value()) {
        // Do not install stubs and stay pre-monomorphic for
        // uninitialized accesses.
        return ReferenceError(name);
      }

      if (FLAG_use_ic &&
          StoreScriptContextFieldStub::Accepted(&lookup_result)) {
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
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError(MessageTemplate::kNonObjectPropertyStore, object, name);
  }

  // Observed objects are always modified through the runtime.
  if (object->IsHeapObject() &&
      Handle<HeapObject>::cast(object)->map()->is_observed()) {
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate(), result,
        Object::SetProperty(object, name, value, language_mode(), store_mode),
        Object);
    return result;
  }

  LookupIterator it(object, name);
  if (FLAG_use_ic) UpdateCaches(&it, value, store_mode);

  MAYBE_RETURN_NULL(
      Object::SetProperty(&it, value, language_mode(), store_mode));
  return value;
}


Handle<Code> CallIC::initialize_stub(Isolate* isolate, int argc,
                                     ConvertReceiverMode mode) {
  CallICTrampolineStub stub(isolate, CallICState(argc, mode));
  Handle<Code> code = stub.GetCode();
  return code;
}


Handle<Code> CallIC::initialize_stub_in_optimized_code(
    Isolate* isolate, int argc, ConvertReceiverMode mode) {
  CallICStub stub(isolate, CallICState(argc, mode));
  Handle<Code> code = stub.GetCode();
  return code;
}


static Handle<Code> StoreICInitializeStubHelper(
    Isolate* isolate, ExtraICState extra_state,
    InlineCacheState initialization_state) {
  Handle<Code> ic = PropertyICCompiler::ComputeStore(
      isolate, initialization_state, extra_state);
  return ic;
}


Handle<Code> StoreIC::initialize_stub(Isolate* isolate,
                                      LanguageMode language_mode,
                                      State initialization_state) {
  DCHECK(initialization_state == UNINITIALIZED ||
         initialization_state == PREMONOMORPHIC ||
         initialization_state == MEGAMORPHIC);
  VectorStoreICTrampolineStub stub(isolate, StoreICState(language_mode));
  return stub.GetCode();
}


Handle<Code> StoreIC::initialize_stub_in_optimized_code(
    Isolate* isolate, LanguageMode language_mode, State initialization_state) {
  DCHECK(initialization_state == UNINITIALIZED ||
         initialization_state == PREMONOMORPHIC ||
         initialization_state == MEGAMORPHIC);
  if (initialization_state != MEGAMORPHIC) {
    VectorStoreICStub stub(isolate, StoreICState(language_mode));
    return stub.GetCode();
  }

  return StoreICInitializeStubHelper(
      isolate, ComputeExtraICState(language_mode), initialization_state);
}


Handle<Code> StoreIC::megamorphic_stub() {
  if (kind() == Code::STORE_IC) {
    return PropertyICCompiler::ComputeStore(isolate(), MEGAMORPHIC,
                                            extra_ic_state());
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    if (is_strict(language_mode())) {
      return isolate()->builtins()->KeyedStoreIC_Megamorphic_Strict();
    } else {
      return isolate()->builtins()->KeyedStoreIC_Megamorphic();
    }
  }
}


Handle<Code> StoreIC::slow_stub() const {
  if (kind() == Code::STORE_IC) {
    return isolate()->builtins()->StoreIC_Slow();
  } else {
    DCHECK(kind() == Code::KEYED_STORE_IC);
    return isolate()->builtins()->KeyedStoreIC_Slow();
  }
}


Handle<Code> StoreIC::pre_monomorphic_stub(Isolate* isolate,
                                           LanguageMode language_mode) {
  ExtraICState state = ComputeExtraICState(language_mode);
  return PropertyICCompiler::ComputeStore(isolate, PREMONOMORPHIC, state);
}


void StoreIC::UpdateCaches(LookupIterator* lookup, Handle<Object> value,
                           JSReceiver::StoreFromKeyed store_mode) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache. Set the target to
    // the pre monomorphic stub to delay setting the monomorphic state.
    ConfigureVectorState(PREMONOMORPHIC);
    TRACE_IC("StoreIC", lookup->name());
    return;
  }

  bool use_ic = LookupForWrite(lookup, value, store_mode);
  if (!use_ic) {
    TRACE_GENERIC_IC(isolate(), "StoreIC", "LookupForWrite said 'false'");
  }
  Handle<Code> code = use_ic ? ComputeHandler(lookup, value) : slow_stub();

  PatchCache(lookup->name(), code);
  TRACE_IC("StoreIC", lookup->name());
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


Handle<Code> StoreIC::CompileHandler(LookupIterator* lookup,
                                     Handle<Object> value,
                                     CacheHolderFlag cache_holder) {
  DCHECK_NE(LookupIterator::JSPROXY, lookup->state());

  // This is currently guaranteed by checks in StoreIC::Store.
  Handle<JSObject> receiver = Handle<JSObject>::cast(lookup->GetReceiver());
  Handle<JSObject> holder = lookup->GetHolder<JSObject>();
  DCHECK(!receiver->IsAccessCheckNeeded() ||
         isolate()->IsInternallyUsedPropertyName(lookup->name()));

  switch (lookup->state()) {
    case LookupIterator::TRANSITION: {
      auto store_target = lookup->GetStoreTarget();
      if (store_target->IsJSGlobalObject()) {
        // TODO(dcarney): this currently just deopts. Use the transition cell.
        auto cell = isolate()->factory()->NewPropertyCell();
        cell->set_value(*value);
        auto code = PropertyCellStoreHandler(
            isolate(), store_target, Handle<JSGlobalObject>::cast(store_target),
            lookup->name(), cell, PropertyCellType::kConstant);
        cell->set_value(isolate()->heap()->the_hole_value());
        return code;
      }
      Handle<Map> transition = lookup->transition_map();
      // Currently not handled by CompileStoreTransition.
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "transition from slow");
        break;
      }

      DCHECK(lookup->IsCacheableTransition());
      NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
      return compiler.CompileStoreTransition(transition, lookup->name());
    }

    case LookupIterator::INTERCEPTOR: {
      DCHECK(!holder->GetNamedInterceptor()->setter()->IsUndefined());
      NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
      return compiler.CompileStoreInterceptor(lookup->name());
    }

    case LookupIterator::ACCESSOR: {
      if (!holder->HasFastProperties()) {
        TRACE_GENERIC_IC(isolate(), "StoreIC", "accessor on slow map");
        break;
      }
      Handle<Object> accessors = lookup->GetAccessors();
      if (accessors->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(accessors);
        if (v8::ToCData<Address>(info->setter()) == 0) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter == 0");
          break;
        }
        if (AccessorInfo::cast(*accessors)->is_special_data_property() &&
            !lookup->HolderIsReceiverOrHiddenPrototype()) {
          TRACE_GENERIC_IC(isolate(), "StoreIC",
                           "special data property in prototype chain");
          break;
        }
        if (!ExecutableAccessorInfo::IsCompatibleReceiverMap(isolate(), info,
                                                             receiver_map())) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "incompatible receiver type");
          break;
        }
        NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
        return compiler.CompileStoreCallback(receiver, lookup->name(), info);
      } else if (accessors->IsAccessorPair()) {
        Handle<Object> setter(Handle<AccessorPair>::cast(accessors)->setter(),
                              isolate());
        if (!setter->IsJSFunction()) {
          TRACE_GENERIC_IC(isolate(), "StoreIC", "setter not a function");
          break;
        }
        Handle<JSFunction> function = Handle<JSFunction>::cast(setter);
        CallOptimization call_optimization(function);
        NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(receiver, holder)) {
          return compiler.CompileStoreCallback(receiver, lookup->name(),
                                               call_optimization,
                                               lookup->GetAccessorIndex());
        }
        int expected_arguments =
            function->shared()->internal_formal_parameter_count();
        return compiler.CompileStoreViaSetter(receiver, lookup->name(),
                                              lookup->GetAccessorIndex(),
                                              expected_arguments);
      }
      break;
    }

    case LookupIterator::DATA: {
      if (lookup->is_dictionary_holder()) {
        if (holder->IsJSGlobalObject()) {
          DCHECK(holder.is_identical_to(receiver) ||
                 receiver->map()->prototype() == *holder);
          auto cell = lookup->GetPropertyCell();
          auto updated_type = PropertyCell::UpdatedType(
              cell, value, lookup->property_details());
          auto code = PropertyCellStoreHandler(
              isolate(), receiver, Handle<JSGlobalObject>::cast(holder),
              lookup->name(), cell, updated_type);
          return code;
        }
        DCHECK(holder.is_identical_to(receiver));
        return isolate()->builtins()->StoreIC_Normal();
      }

      // -------------- Fields --------------
      if (lookup->property_details().type() == DATA) {
        bool use_stub = true;
        if (lookup->representation().IsHeapObject()) {
          // Only use a generic stub if no types need to be tracked.
          Handle<HeapType> field_type = lookup->GetFieldType();
          HeapType::Iterator<Map> it = field_type->Classes();
          use_stub = it.Done();
        }
        if (use_stub) {
          StoreFieldStub stub(isolate(), lookup->GetFieldIndex(),
                              lookup->representation());
          return stub.GetCode();
        }
        NamedStoreHandlerCompiler compiler(isolate(), receiver_map(), holder);
        return compiler.CompileStoreField(lookup);
      }

      // -------------- Constant properties --------------
      DCHECK(lookup->property_details().type() == DATA_CONSTANT);
      TRACE_GENERIC_IC(isolate(), "StoreIC", "constant property");
      break;
    }

    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
      UNREACHABLE();
  }
  return slow_stub();
}


Handle<Code> KeyedStoreIC::StoreElementStub(Handle<Map> receiver_map,
                                            KeyedAccessStoreMode store_mode) {
  Handle<Code> null_handle;
  // Don't handle megamorphic property accesses for INTERCEPTORS or
  // ACCESSOR_CONSTANT
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "non-NORMAL target type");
    return megamorphic_stub();
  }

  MapHandleList target_receiver_maps;
  TargetMaps(&target_receiver_maps);
  if (target_receiver_maps.length() == 0) {
    Handle<Map> monomorphic_map =
        ComputeTransitionedMap(receiver_map, store_mode);
    store_mode = GetNonTransitioningStoreMode(store_mode);
    Handle<Code> handler =
        PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
            monomorphic_map, language_mode(), store_mode);
    ConfigureVectorState(Handle<Name>::null(), monomorphic_map, handler);
    return null_handle;
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
      Handle<Code> handler =
          PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
              transitioned_receiver_map, language_mode(), store_mode);
      ConfigureVectorState(Handle<Name>::null(), transitioned_receiver_map,
                           handler);
      return null_handle;
    } else if (receiver_map.is_identical_to(previous_receiver_map) &&
               old_store_mode == STANDARD_STORE &&
               (store_mode == STORE_AND_GROW_NO_TRANSITION ||
                store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
                store_mode == STORE_NO_TRANSITION_HANDLE_COW)) {
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      Handle<Code> handler =
          PropertyICCompiler::ComputeKeyedStoreMonomorphicHandler(
              receiver_map, language_mode(), store_mode);
      ConfigureVectorState(Handle<Name>::null(), receiver_map, handler);
      return null_handle;
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
    return megamorphic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the
  // megamorphic version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    return megamorphic_stub();
  }

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // megamorphic stub must be used.
  store_mode = GetNonTransitioningStoreMode(store_mode);
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "store mode mismatch");
      return megamorphic_stub();
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
      return megamorphic_stub();
    }
  }

  MapHandleList transitioned_maps(target_receiver_maps.length());
  CodeHandleList handlers(target_receiver_maps.length());
  PropertyICCompiler::ComputeKeyedStorePolymorphicHandlers(
      &target_receiver_maps, &transitioned_maps, &handlers, store_mode,
      language_mode());
  ConfigureVectorState(&target_receiver_maps, &transitioned_maps, &handlers);
  return null_handle;
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
  Handle<Code> stub = megamorphic_stub();

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
      ConfigureVectorState(MEGAMORPHIC);
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                       "unhandled internalized string key");
      TRACE_IC("StoreIC", key);
    }
    return store_handle;
  }

  bool use_ic =
      FLAG_use_ic && !object->IsStringWrapper() &&
      !object->IsAccessCheckNeeded() && !object->IsJSGlobalProxy() &&
      !(object->IsJSObject() && JSObject::cast(*object)->map()->is_observed());
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
  bool sloppy_arguments_elements = false;
  bool key_is_valid_index = false;
  KeyedAccessStoreMode store_mode = STANDARD_STORE;
  if (use_ic && object->IsJSObject()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    old_receiver_map = handle(receiver->map(), isolate());
    sloppy_arguments_elements =
        !is_sloppy(language_mode()) &&
        receiver->elements()->map() ==
            isolate()->heap()->sloppy_arguments_elements_map();
    if (!sloppy_arguments_elements) {
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
      if (sloppy_arguments_elements) {
        TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "arguments receiver");
      } else if (key_is_valid_index) {
        // We should go generic if receiver isn't a dictionary, but our
        // prototype chain does have dictionary elements. This ensures that
        // other non-dictionary receivers in the polymorphic case benefit
        // from fast path keyed stores.
        if (!old_receiver_map->DictionaryElementsInPrototypeChainOnly()) {
          stub = StoreElementStub(old_receiver_map, store_mode);
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

  if (!is_vector_set() || stub.is_null()) {
    Code* megamorphic = *megamorphic_stub();
    if (!stub.is_null() && (*stub == megamorphic || *stub == *slow_stub())) {
      ConfigureVectorState(MEGAMORPHIC);
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC",
                       *stub == megamorphic ? "set generic" : "slow stub");
    }
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
    DCHECK(feedback == *TypeFeedbackVector::UninitializedSentinel(isolate()));
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
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  Handle<Object> function = args.at<Object>(0);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(1);
  Handle<Smi> slot = args.at<Smi>(2);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  CallICNexus nexus(vector, vector_slot);
  CallIC ic(isolate, &nexus);
  ic.HandleMiss(function);
  return *function;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_LoadIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Object> result;

  DCHECK(args.length() == 4);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  // A monomorphic or polymorphic KeyedLoadIC with a string key can call the
  // LoadIC miss handler if the handler misses. Since the vector Nexus is
  // set up outside the IC, handle that here.
  if (vector->GetKind(vector_slot) == FeedbackVectorSlotKind::LOAD_IC) {
    LoadICNexus nexus(vector, vector_slot);
    LoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC,
              vector->GetKind(vector_slot));
    KeyedLoadICNexus nexus(vector, vector_slot);
    KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  }
  return *result;
}


// Used from ic-<arch>.cc
RUNTIME_FUNCTION(Runtime_KeyedLoadIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> result;

  DCHECK(args.length() == 4);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedLoadICNexus nexus(vector, vector_slot);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_KeyedLoadIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> result;

  DCHECK(args.length() == 4);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedLoadICNexus nexus(vector, vector_slot);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));

  return *result;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_StoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Object> value = args.at<Object>(2);
  Handle<Object> result;

  DCHECK(args.length() == 5 || args.length() == 6);
  Handle<Smi> slot = args.at<Smi>(3);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(4);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  if (vector->GetKind(vector_slot) == FeedbackVectorSlotKind::STORE_IC) {
    StoreICNexus nexus(vector, vector_slot);
    StoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                       ic.Store(receiver, key, value));
  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC,
              vector->GetKind(vector_slot));
    KeyedStoreICNexus nexus(vector, vector_slot);
    KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                       ic.Store(receiver, key, value));
  }
  return *result;
}


RUNTIME_FUNCTION(Runtime_StoreIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Object> value = args.at<Object>(2);
  Handle<Object> result;

  int length = args.length();
  DCHECK(length == 5 || length == 6);
  // We might have slot and vector, for a normal miss (slot(3), vector(4)).
  // Or, map and vector for a transitioning store miss (map(3), vector(4)).
  // In this case, we need to recover the slot from a virtual register.
  // If length == 6, then a map is included (map(3), slot(4), vector(5)).
  Handle<Smi> slot;
  Handle<TypeFeedbackVector> vector;
  if (length == 5) {
    if (args.at<Object>(3)->IsMap()) {
      vector = args.at<TypeFeedbackVector>(4);
      slot = handle(
          *reinterpret_cast<Smi**>(isolate->virtual_slot_register_address()),
          isolate);
    } else {
      vector = args.at<TypeFeedbackVector>(4);
      slot = args.at<Smi>(3);
    }
  } else {
    vector = args.at<TypeFeedbackVector>(5);
    slot = args.at<Smi>(4);
  }

  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  if (vector->GetKind(vector_slot) == FeedbackVectorSlotKind::STORE_IC) {
    StoreICNexus nexus(vector, vector_slot);
    StoreIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                       ic.Store(receiver, key, value));
  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_STORE_IC,
              vector->GetKind(vector_slot));
    KeyedStoreICNexus nexus(vector, vector_slot);
    KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                       ic.Store(receiver, key, value));
  }
  return *result;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  Handle<Object> result;

  DCHECK(args.length() == 5);
  Handle<Smi> slot = args.at<Smi>(3);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(4);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedStoreICNexus nexus(vector, vector_slot);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     ic.Store(receiver, key, value));
  return *result;
}


RUNTIME_FUNCTION(Runtime_KeyedStoreIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  Handle<Object> result;

  DCHECK(args.length() == 5);
  Handle<Smi> slot = args.at<Smi>(3);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(4);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  KeyedStoreICNexus nexus(vector, vector_slot);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
  ic.UpdateState(receiver, key);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     ic.Store(receiver, key, value));
  return *result;
}


RUNTIME_FUNCTION(Runtime_StoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  LanguageMode language_mode;
  StoreICNexus nexus(isolate);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  language_mode = ic.language_mode();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 5);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  LanguageMode language_mode;
  KeyedStoreICNexus nexus(isolate);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  language_mode = ic.language_mode();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ElementsTransitionAndStoreIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  // Length == 5 or 6, depending on whether the vector slot
  // is passed in a virtual register or not.
  DCHECK(args.length() == 5 || args.length() == 6);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  Handle<Map> map = args.at<Map>(3);
  LanguageMode language_mode;
  KeyedStoreICNexus nexus(isolate);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
  language_mode = ic.language_mode();
  if (object->IsJSObject()) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object),
                                     map->elements_kind());
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(isolate, object, key, value, language_mode));
  return *result;
}


MaybeHandle<Object> BinaryOpIC::Transition(
    Handle<AllocationSite> allocation_site, Handle<Object> left,
    Handle<Object> right) {
  BinaryOpICState state(isolate(), target()->extra_ic_state());

  // Compute the actual result using the builtin for the binary operation.
  Handle<Object> result;
  switch (state.op()) {
    default:
      UNREACHABLE();
    case Token::ADD:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::Add(isolate(), left, right, state.strength()), Object);
      break;
    case Token::SUB:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::Subtract(isolate(), left, right, state.strength()), Object);
      break;
    case Token::MUL:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::Multiply(isolate(), left, right, state.strength()), Object);
      break;
    case Token::DIV:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::Divide(isolate(), left, right, state.strength()), Object);
      break;
    case Token::MOD:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::Modulus(isolate(), left, right, state.strength()), Object);
      break;
    case Token::BIT_OR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::BitwiseOr(isolate(), left, right, state.strength()), Object);
      break;
    case Token::BIT_AND:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::BitwiseAnd(isolate(), left, right, state.strength()), Object);
      break;
    case Token::BIT_XOR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::BitwiseXor(isolate(), left, right, state.strength()), Object);
      break;
    case Token::SAR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::ShiftRight(isolate(), left, right, state.strength()), Object);
      break;
    case Token::SHR:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::ShiftRightLogical(isolate(), left, right, state.strength()),
          Object);
      break;
    case Token::SHL:
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate(), result,
          Object::ShiftLeft(isolate(), left, right, state.strength()), Object);
      break;
  }

  // Do not try to update the target if the code was marked for lazy
  // deoptimization. (Since we do not relocate addresses in these
  // code objects, an attempt to access the target could fail.)
  if (AddressIsDeoptimizedCode()) {
    return result;
  }

  // Execution::Call can execute arbitrary JavaScript, hence potentially
  // update the state of this very IC, so we must update the stored state.
  UpdateTarget();

  // Compute the new state.
  BinaryOpICState old_state(isolate(), target()->extra_ic_state());
  state.Update(left, right, result);

  // Check if we have a string operation here.
  Handle<Code> target;
  if (!allocation_site.is_null() || state.ShouldCreateAllocationMementos()) {
    // Setup the allocation site on-demand.
    if (allocation_site.is_null()) {
      allocation_site = isolate()->factory()->NewAllocationSite();
    }

    // Install the stub with an allocation site.
    BinaryOpICWithAllocationSiteStub stub(isolate(), state);
    target = stub.GetCodeCopyFromTemplate(allocation_site);

    // Sanity check the trampoline stub.
    DCHECK_EQ(*allocation_site, target->FindFirstAllocationSite());
  } else {
    // Install the generic stub.
    BinaryOpICStub stub(isolate(), state);
    target = stub.GetCode();

    // Sanity check the generic stub.
    DCHECK_NULL(target->FindFirstAllocationSite());
  }
  set_target(*target);

  if (FLAG_trace_ic) {
    OFStream os(stdout);
    os << "[BinaryOpIC" << old_state << " => " << state << " @ "
       << static_cast<void*>(*target) << " <- ";
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
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  Handle<Object> left = args.at<Object>(BinaryOpICStub::kLeft);
  Handle<Object> right = args.at<Object>(BinaryOpICStub::kRight);
  BinaryOpIC ic(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      ic.Transition(Handle<AllocationSite>::null(), left, right));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BinaryOpIC_MissWithAllocationSite) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  Handle<AllocationSite> allocation_site =
      args.at<AllocationSite>(BinaryOpWithAllocationSiteStub::kAllocationSite);
  Handle<Object> left = args.at<Object>(BinaryOpWithAllocationSiteStub::kLeft);
  Handle<Object> right =
      args.at<Object>(BinaryOpWithAllocationSiteStub::kRight);
  BinaryOpIC ic(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, ic.Transition(allocation_site, left, right));
  return *result;
}


Code* CompareIC::GetRawUninitialized(Isolate* isolate, Token::Value op,
                                     Strength strength) {
  CompareICStub stub(isolate, op, strength, CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED);
  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code));
  return code;
}


Handle<Code> CompareIC::GetUninitialized(Isolate* isolate, Token::Value op,
                                         Strength strength) {
  CompareICStub stub(isolate, op, strength, CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED,
                     CompareICState::UNINITIALIZED);
  return stub.GetCode();
}


Code* CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope(isolate());
  CompareICStub old_stub(target()->stub_key(), isolate());
  CompareICState::State new_left =
      CompareICState::NewInputState(old_stub.left(), x);
  CompareICState::State new_right =
      CompareICState::NewInputState(old_stub.right(), y);
  CompareICState::State state = CompareICState::TargetState(
      old_stub.state(), old_stub.left(), old_stub.right(), op_,
      HasInlinedSmiCode(address()), x, y);
  CompareICStub stub(isolate(), op_, old_stub.strength(), new_left, new_right,
                     state);
  if (state == CompareICState::KNOWN_RECEIVER) {
    stub.set_known_map(
        Handle<Map>(Handle<JSReceiver>::cast(x)->map(), isolate()));
  }
  Handle<Code> new_target = stub.GetCode();
  set_target(*new_target);

  if (FLAG_trace_ic) {
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
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  return ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
}


void CompareNilIC::Clear(Address address, Code* target, Address constant_pool) {
  if (IsCleared(target)) return;
  ExtraICState state = target->extra_ic_state();

  CompareNilICStub stub(target->GetIsolate(), state,
                        HydrogenCodeStub::UNINITIALIZED);
  stub.ClearState();

  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code));

  SetTargetAtAddress(address, code, constant_pool);
}


Handle<Object> CompareNilIC::DoCompareNilSlow(Isolate* isolate, NilValue nil,
                                              Handle<Object> object) {
  if (object->IsNull() || object->IsUndefined()) {
    return isolate->factory()->true_value();
  }
  return isolate->factory()->ToBoolean(object->IsUndetectableObject());
}


Handle<Object> CompareNilIC::CompareNil(Handle<Object> object) {
  ExtraICState extra_ic_state = target()->extra_ic_state();

  CompareNilICStub stub(isolate(), extra_ic_state);

  // Extract the current supported types from the patched IC and calculate what
  // types must be supported as a result of the miss.
  bool already_monomorphic = stub.IsMonomorphic();

  stub.UpdateStatus(object);

  NilValue nil = stub.nil_value();

  // Find or create the specialized stub to support the new set of types.
  Handle<Code> code;
  if (stub.IsMonomorphic()) {
    Handle<Map> monomorphic_map(already_monomorphic && FirstTargetMap() != NULL
                                    ? FirstTargetMap()
                                    : HeapObject::cast(*object)->map());
    code = PropertyICCompiler::ComputeCompareNil(monomorphic_map, &stub);
  } else {
    code = stub.GetCode();
  }
  set_target(*code);
  return DoCompareNilSlow(isolate(), nil, object);
}


RUNTIME_FUNCTION(Runtime_CompareNilIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  CompareNilIC ic(isolate);
  return *ic.CompareNil(object);
}


RUNTIME_FUNCTION(Runtime_Unreachable) {
  UNREACHABLE();
  CHECK(false);
  return isolate->heap()->undefined_value();
}


Handle<Object> ToBooleanIC::ToBoolean(Handle<Object> object) {
  ToBooleanStub stub(isolate(), target()->extra_ic_state());
  bool to_boolean_value = stub.UpdateStatus(object);
  Handle<Code> code = stub.GetCode();
  set_target(*code);
  return isolate()->factory()->ToBoolean(to_boolean_value);
}


RUNTIME_FUNCTION(Runtime_ToBooleanIC_Miss) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  DCHECK(args.length() == 1);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  ToBooleanIC ic(isolate);
  return *ic.ToBoolean(object);
}


RUNTIME_FUNCTION(Runtime_StoreCallbackProperty) {
  Handle<JSObject> receiver = args.at<JSObject>(0);
  Handle<JSObject> holder = args.at<JSObject>(1);
  Handle<HeapObject> callback_or_cell = args.at<HeapObject>(2);
  Handle<Name> name = args.at<Name>(3);
  Handle<Object> value = args.at<Object>(4);
  HandleScope scope(isolate);

  Handle<ExecutableAccessorInfo> callback(
      callback_or_cell->IsWeakCell()
          ? ExecutableAccessorInfo::cast(
                WeakCell::cast(*callback_or_cell)->value())
          : ExecutableAccessorInfo::cast(*callback_or_cell));

  DCHECK(callback->IsCompatibleReceiver(*receiver));

  Address setter_address = v8::ToCData<Address>(callback->setter());
  v8::AccessorNameSetterCallback fun =
      FUNCTION_CAST<v8::AccessorNameSetterCallback>(setter_address);
  DCHECK(fun != NULL);

  LOG(isolate, ApiNamedPropertyAccess("store", *receiver, *name));
  PropertyCallbackArguments custom_args(isolate, callback->data(), *receiver,
                                        *holder);
  custom_args.Call(fun, v8::Utils::ToLocal(name), v8::Utils::ToLocal(value));
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
  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);
  HandleScope scope(isolate);
  LookupIterator it(receiver, name, holder, LookupIterator::OWN);
  bool done;
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::GetPropertyWithInterceptor(&it, &done));
  if (done) return *result;
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
  Handle<JSObject> receiver =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex);
  Handle<JSObject> holder =
      args.at<JSObject>(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex);

  Handle<Object> result;
  LookupIterator it(receiver, name, holder);
  // TODO(conradw): Investigate strong mode semantics for this.
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     JSObject::GetProperty(&it));

  if (it.IsFound()) return *result;

  // Return the undefined result if the reference error should not be thrown.
  // Note that both keyed and non-keyed loads may end up here.
  LoadICNexus nexus(isolate);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate, &nexus);
  if (!ic.ShouldThrowReferenceError(it.GetReceiver())) {
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
  Handle<Object> value = args.at<Object>(2);
#ifdef DEBUG
  PrototypeIterator iter(isolate, receiver,
                         PrototypeIterator::START_AT_RECEIVER);
  bool found = false;
  for (; !iter.IsAtEnd(PrototypeIterator::END_AT_NON_HIDDEN); iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (current->IsJSObject() &&
        Handle<JSObject>::cast(current)->HasNamedInterceptor()) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetProperty(receiver, name, value, ic.language_mode()));
  return *result;
}


RUNTIME_FUNCTION(Runtime_LoadElementWithInterceptor) {
  // TODO(verwaest): This should probably get the holder and receiver as input.
  HandleScope scope(isolate);
  Handle<JSObject> receiver = args.at<JSObject>(0);
  DCHECK(args.smi_at(1) >= 0);
  uint32_t index = args.smi_at(1);
  Handle<Object> result;
  // TODO(conradw): Investigate strong mode semantics for this.
  LanguageMode language_mode = SLOPPY;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::GetElement(isolate, receiver, index, language_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_LoadIC_MissFromStubFailure) {
  TimerEventScope<TimerEventIcMiss> timer(isolate);
  HandleScope scope(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Name> key = args.at<Name>(1);
  Handle<Object> result;

  DCHECK(args.length() == 4);
  Handle<Smi> slot = args.at<Smi>(2);
  Handle<TypeFeedbackVector> vector = args.at<TypeFeedbackVector>(3);
  FeedbackVectorSlot vector_slot = vector->ToSlot(slot->value());
  // A monomorphic or polymorphic KeyedLoadIC with a string key can call the
  // LoadIC miss handler if the handler misses. Since the vector Nexus is
  // set up outside the IC, handle that here.
  if (vector->GetKind(vector_slot) == FeedbackVectorSlotKind::LOAD_IC) {
    LoadICNexus nexus(vector, vector_slot);
    LoadIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  } else {
    DCHECK_EQ(FeedbackVectorSlotKind::KEYED_LOAD_IC,
              vector->GetKind(vector_slot));
    KeyedLoadICNexus nexus(vector, vector_slot);
    KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate, &nexus);
    ic.UpdateState(receiver, key);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, ic.Load(receiver, key));
  }

  return *result;
}
}  // namespace internal
}  // namespace v8
