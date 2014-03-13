// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "codegen.h"
#include "execution.h"
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"
#include "v8conversions.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
char IC::TransitionMarkFromState(IC::State state) {
  switch (state) {
    case UNINITIALIZED: return '0';
    case PREMONOMORPHIC: return '.';
    case MONOMORPHIC: return '1';
    case MONOMORPHIC_PROTOTYPE_FAILURE: return '^';
    case POLYMORPHIC: return 'P';
    case MEGAMORPHIC: return 'N';
    case GENERIC: return 'G';

    // We never see the debugger states here, because the state is
    // computed from the original code - not the patched code. Let
    // these cases fall through to the unreachable code below.
    case DEBUG_STUB: break;
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


void IC::TraceIC(const char* type,
                 Handle<Object> name) {
  if (FLAG_trace_ic) {
    Code* new_target = raw_target();
    State new_state = new_target->ic_state();
    PrintF("[%s%s in ", new_target->is_keyed_stub() ? "Keyed" : "", type);
    StackFrameIterator it(isolate());
    while (it.frame()->fp() != this->fp()) it.Advance();
    StackFrame* raw_frame = it.frame();
    if (raw_frame->is_internal()) {
      Code* apply_builtin = isolate()->builtins()->builtin(
          Builtins::kFunctionApply);
      if (raw_frame->unchecked_code() == apply_builtin) {
        PrintF("apply from ");
        it.Advance();
        raw_frame = it.frame();
      }
    }
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    ExtraICState extra_state = new_target->extra_ic_state();
    const char* modifier =
        GetTransitionMarkModifier(
            KeyedStoreIC::GetKeyedAccessStoreMode(extra_state));
    PrintF(" (%c->%c%s)",
           TransitionMarkFromState(state()),
           TransitionMarkFromState(new_state),
           modifier);
    name->Print();
    PrintF("]\n");
  }
}

#define TRACE_GENERIC_IC(isolate, type, reason)                 \
  do {                                                          \
    if (FLAG_trace_ic) {                                        \
      PrintF("[%s patching generic stub in ", type);            \
      JavaScriptFrame::PrintTop(isolate, stdout, false, true);  \
      PrintF(" (%s)]\n", reason);                               \
    }                                                           \
  } while (false)

#else
#define TRACE_GENERIC_IC(isolate, type, reason)
#endif  // DEBUG

#define TRACE_IC(type, name)             \
  ASSERT((TraceIC(type, name), true))

IC::IC(FrameDepth depth, Isolate* isolate)
    : isolate_(isolate),
      target_set_(false) {
  // To improve the performance of the (much used) IC code, we unfold a few
  // levels of the stack frame iteration code. This yields a ~35% speedup when
  // running DeltaBlue and a ~25% speedup of gbemu with the '--nouse-ic' flag.
  const Address entry =
      Isolate::c_entry_fp(isolate->thread_local_top());
  Address* pc_address =
      reinterpret_cast<Address*>(entry + ExitFrameConstants::kCallerPCOffset);
  Address fp = Memory::Address_at(entry + ExitFrameConstants::kCallerFPOffset);
  // If there's another JavaScript frame on the stack or a
  // StubFailureTrampoline, we need to look one frame further down the stack to
  // find the frame pointer and the return address stack slot.
  if (depth == EXTRA_CALL_FRAME) {
    const int kCallerPCOffset = StandardFrameConstants::kCallerPCOffset;
    pc_address = reinterpret_cast<Address*>(fp + kCallerPCOffset);
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
#ifdef DEBUG
  StackFrameIterator it(isolate);
  for (int i = 0; i < depth + 1; i++) it.Advance();
  StackFrame* frame = it.frame();
  ASSERT(fp == frame->fp() && pc_address == frame->pc_address());
#endif
  fp_ = fp;
  pc_address_ = StackFrame::ResolveReturnAddressLocation(pc_address);
  target_ = handle(raw_target(), isolate);
  state_ = target_->ic_state();
  extra_ic_state_ = target_->extra_ic_state();
  target()->FindAllTypes(&types_);
  target()->FindHandlers(&handlers_);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Address IC::OriginalCodeAddress() const {
  HandleScope scope(isolate());
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it(isolate());
  while (it.frame()->fp() != this->fp()) it.Advance();
  JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
  // Find the function on the stack and both the active code for the
  // function and the original code.
  JSFunction* function = frame->function();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate());
  Code* code = shared->code();
  ASSERT(Debug::HasDebugInfo(shared));
  Code* original_code = Debug::GetDebugInfo(shared)->original_code();
  ASSERT(original_code->IsCode());
  // Get the address of the call site in the active code. This is the
  // place where the call to DebugBreakXXX is and where the IC
  // normally would be.
  Address addr = Assembler::target_address_from_return_address(pc());
  // Return the address in the original code. This is the place where
  // the call which has been overwritten by the DebugBreakXXX resides
  // and the place where the inline cache system should look.
  intptr_t delta =
      original_code->instruction_start() - code->instruction_start();
  return addr + delta;
}
#endif


static bool HasInterceptorGetter(JSObject* object) {
  return !object->GetNamedInterceptor()->getter()->IsUndefined();
}


static bool HasInterceptorSetter(JSObject* object) {
  return !object->GetNamedInterceptor()->setter()->IsUndefined();
}


static void LookupForRead(Handle<Object> object,
                          Handle<String> name,
                          LookupResult* lookup) {
  // Skip all the objects with named interceptors, but
  // without actual getter.
  while (true) {
    object->Lookup(*name, lookup);
    // Besides normal conditions (property not found or it's not
    // an interceptor), bail out if lookup is not cacheable: we won't
    // be able to IC it anyway and regular lookup should work fine.
    if (!lookup->IsInterceptor() || !lookup->IsCacheable()) {
      return;
    }

    Handle<JSObject> holder(lookup->holder(), lookup->isolate());
    if (HasInterceptorGetter(*holder)) {
      return;
    }

    holder->LocalLookupRealNamedProperty(*name, lookup);
    if (lookup->IsFound()) {
      ASSERT(!lookup->IsInterceptor());
      return;
    }

    Handle<Object> proto(holder->GetPrototype(), lookup->isolate());
    if (proto->IsNull()) {
      ASSERT(!lookup->IsFound());
      return;
    }

    object = proto;
  }
}


bool IC::TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                                Handle<String> name) {
  if (target()->is_keyed_stub()) {
    // Determine whether the failure is due to a name failure.
    if (!name->IsName()) return false;
    Name* stub_name = target()->FindFirstName();
    if (*name != stub_name) return false;
  }

  InlineCacheHolderFlag cache_holder =
      Code::ExtractCacheHolderFromFlags(target()->flags());

  switch (cache_holder) {
    case OWN_MAP:
      // The stub was generated for JSObject but called for non-JSObject.
      // IC::GetCodeCacheHolder is not applicable.
      if (!receiver->IsJSObject()) return false;
      break;
    case PROTOTYPE_MAP:
      // IC::GetCodeCacheHolder is not applicable.
      if (receiver->GetPrototype(isolate())->IsNull()) return false;
      break;
  }

  Handle<Map> map(
      IC::GetCodeCacheHolder(isolate(), *receiver, cache_holder)->map());

  // Decide whether the inline cache failed because of changes to the
  // receiver itself or changes to one of its prototypes.
  //
  // If there are changes to the receiver itself, the map of the
  // receiver will have changed and the current target will not be in
  // the receiver map's code cache.  Therefore, if the current target
  // is in the receiver map's code cache, the inline cache failed due
  // to prototype check failure.
  int index = map->IndexInCodeCache(*name, *target());
  if (index >= 0) {
    map->RemoveFromCodeCache(*name, *target(), index);
    // Handlers are stored in addition to the ICs on the map. Remove those, too.
    TryRemoveInvalidHandlers(map, name);
    return true;
  }

  // The stub is not in the cache. We've ruled out all other kinds of failure
  // except for proptotype chain changes, a deprecated map, a map that's
  // different from the one that the stub expects, elements kind changes, or a
  // constant global property that will become mutable. Threat all those
  // situations as prototype failures (stay monomorphic if possible).

  // If the IC is shared between multiple receivers (slow dictionary mode), then
  // the map cannot be deprecated and the stub invalidated.
  if (cache_holder == OWN_MAP) {
    Map* old_map = first_map();
    if (old_map == *map) return true;
    if (old_map != NULL) {
      if (old_map->is_deprecated()) return true;
      if (IsMoreGeneralElementsKindTransition(old_map->elements_kind(),
                                              map->elements_kind())) {
        return true;
      }
    }
  }

  if (receiver->IsGlobalObject()) {
    LookupResult lookup(isolate());
    GlobalObject* global = GlobalObject::cast(*receiver);
    global->LocalLookupRealNamedProperty(*name, &lookup);
    if (!lookup.IsFound()) return false;
    PropertyCell* cell = global->GetPropertyCell(&lookup);
    return cell->type()->IsConstant();
  }

  return false;
}


void IC::TryRemoveInvalidHandlers(Handle<Map> map, Handle<String> name) {
  for (int i = 0; i < handlers()->length(); i++) {
    Handle<Code> handler = handlers()->at(i);
    int index = map->IndexInCodeCache(*name, *handler);
    if (index >= 0) {
      map->RemoveFromCodeCache(*name, *handler, index);
      return;
    }
  }
}


void IC::UpdateState(Handle<Object> receiver, Handle<Object> name) {
  if (!name->IsString()) return;
  if (state() != MONOMORPHIC) {
    if (state() == POLYMORPHIC && receiver->IsHeapObject()) {
      TryRemoveInvalidHandlers(
          handle(Handle<HeapObject>::cast(receiver)->map()),
          Handle<String>::cast(name));
    }
    return;
  }
  if (receiver->IsUndefined() || receiver->IsNull()) return;

  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  if (TryRemoveInvalidPrototypeDependentStub(
          receiver, Handle<String>::cast(name))) {
    return MarkMonomorphicPrototypeFailure();
  }

  // The builtins object is special.  It only changes when JavaScript
  // builtins are loaded lazily.  It is important to keep inline
  // caches for the builtins object monomorphic.  Therefore, if we get
  // an inline cache miss for the builtins object after lazily loading
  // JavaScript builtins, we return uninitialized as the state to
  // force the inline cache back to monomorphic state.
  if (receiver->IsJSBuiltinsObject()) state_ = UNINITIALIZED;
}


Failure* IC::TypeError(const char* type,
                       Handle<Object> object,
                       Handle<Object> key) {
  HandleScope scope(isolate());
  Handle<Object> args[2] = { key, object };
  Handle<Object> error = isolate()->factory()->NewTypeError(
      type, HandleVector(args, 2));
  return isolate()->Throw(*error);
}


Failure* IC::ReferenceError(const char* type, Handle<String> name) {
  HandleScope scope(isolate());
  Handle<Object> error = isolate()->factory()->NewReferenceError(
      type, HandleVector(&name, 1));
  return isolate()->Throw(*error);
}


static int ComputeTypeInfoCountDelta(IC::State old_state, IC::State new_state) {
  bool was_uninitialized =
      old_state == UNINITIALIZED || old_state == PREMONOMORPHIC;
  bool is_uninitialized =
      new_state == UNINITIALIZED || new_state == PREMONOMORPHIC;
  return (was_uninitialized && !is_uninitialized) ?  1 :
         (!was_uninitialized && is_uninitialized) ? -1 : 0;
}


void IC::PostPatching(Address address, Code* target, Code* old_target) {
  Isolate* isolate = target->GetHeap()->isolate();
  Code* host = isolate->
      inner_pointer_to_code_cache()->GetCacheEntry(address)->code;
  if (host->kind() != Code::FUNCTION) return;

  if (FLAG_type_info_threshold > 0 &&
      old_target->is_inline_cache_stub() &&
      target->is_inline_cache_stub()) {
    int delta = ComputeTypeInfoCountDelta(old_target->ic_state(),
                                          target->ic_state());
    // Not all Code objects have TypeFeedbackInfo.
    if (host->type_feedback_info()->IsTypeFeedbackInfo() && delta != 0) {
      TypeFeedbackInfo* info =
          TypeFeedbackInfo::cast(host->type_feedback_info());
      info->change_ic_with_type_info_count(delta);
    }
  }
  if (host->type_feedback_info()->IsTypeFeedbackInfo()) {
    TypeFeedbackInfo* info =
        TypeFeedbackInfo::cast(host->type_feedback_info());
    info->change_own_type_change_checksum();
  }
  host->set_profiler_ticks(0);
  isolate->runtime_profiler()->NotifyICChanged();
  // TODO(2029): When an optimized function is patched, it would
  // be nice to propagate the corresponding type information to its
  // unoptimized version for the benefit of later inlining.
}


void IC::Clear(Isolate* isolate, Address address) {
  Code* target = GetTargetAtAddress(address);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->is_debug_stub()) return;

  switch (target->kind()) {
    case Code::LOAD_IC: return LoadIC::Clear(isolate, address, target);
    case Code::KEYED_LOAD_IC:
      return KeyedLoadIC::Clear(isolate, address, target);
    case Code::STORE_IC: return StoreIC::Clear(isolate, address, target);
    case Code::KEYED_STORE_IC:
      return KeyedStoreIC::Clear(isolate, address, target);
    case Code::COMPARE_IC: return CompareIC::Clear(isolate, address, target);
    case Code::COMPARE_NIL_IC: return CompareNilIC::Clear(address, target);
    case Code::BINARY_OP_IC:
    case Code::TO_BOOLEAN_IC:
      // Clearing these is tricky and does not
      // make any performance difference.
      return;
    default: UNREACHABLE();
  }
}


void KeyedLoadIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  // Make sure to also clear the map used in inline fast cases.  If we
  // do not clear these maps, cached code can keep objects alive
  // through the embedded maps.
  SetTargetAtAddress(address, *pre_monomorphic_stub(isolate));
}


void LoadIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  Code* code = target->GetIsolate()->stub_cache()->FindPreMonomorphicIC(
      Code::LOAD_IC, target->extra_ic_state());
  SetTargetAtAddress(address, code);
}


void StoreIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  Code* code = target->GetIsolate()->stub_cache()->FindPreMonomorphicIC(
      Code::STORE_IC, target->extra_ic_state());
  SetTargetAtAddress(address, code);
}


void KeyedStoreIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  SetTargetAtAddress(address,
      *pre_monomorphic_stub(
          isolate, StoreIC::GetStrictMode(target->extra_ic_state())));
}


void CompareIC::Clear(Isolate* isolate, Address address, Code* target) {
  ASSERT(target->major_key() == CodeStub::CompareIC);
  CompareIC::State handler_state;
  Token::Value op;
  ICCompareStub::DecodeMinorKey(target->stub_info(), NULL, NULL,
                                &handler_state, &op);
  // Only clear CompareICs that can retain objects.
  if (handler_state != KNOWN_OBJECT) return;
  SetTargetAtAddress(address, GetRawUninitialized(isolate, op));
  PatchInlinedSmiCode(address, DISABLE_INLINED_SMI_CHECK);
}


static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map()->is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}


MaybeObject* LoadIC::Load(Handle<Object> object,
                          Handle<String> name) {
  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_load", object, name);
  }

  if (FLAG_use_ic) {
    // Use specialized code for getting the length of strings and
    // string wrapper objects.  The length property of string wrapper
    // objects is read-only and therefore always returns the length of
    // the underlying string value.  See ECMA-262 15.5.5.1.
    if (object->IsStringWrapper() &&
        name->Equals(isolate()->heap()->length_string())) {
      Handle<Code> stub;
      if (state() == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state() == PREMONOMORPHIC || state() == MONOMORPHIC) {
        StringLengthStub string_length_stub(kind());
        stub = string_length_stub.GetCode(isolate());
      } else if (state() != MEGAMORPHIC) {
        ASSERT(state() != GENERIC);
        stub = megamorphic_stub();
      }
      if (!stub.is_null()) {
        set_target(*stub);
        if (FLAG_trace_ic) PrintF("[LoadIC : +#length /stringwrapper]\n");
      }
      // Get the string if we have a string wrapper object.
      String* string = String::cast(JSValue::cast(*object)->value());
      return Smi::FromInt(string->length());
    }

    // Use specialized code for getting prototype of functions.
    if (object->IsJSFunction() &&
        name->Equals(isolate()->heap()->prototype_string()) &&
        Handle<JSFunction>::cast(object)->should_have_prototype()) {
      Handle<Code> stub;
      if (state() == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state() == PREMONOMORPHIC) {
        FunctionPrototypeStub function_prototype_stub(kind());
        stub = function_prototype_stub.GetCode(isolate());
      } else if (state() != MEGAMORPHIC) {
        ASSERT(state() != GENERIC);
        stub = megamorphic_stub();
      }
      if (!stub.is_null()) {
        set_target(*stub);
        if (FLAG_trace_ic) PrintF("[LoadIC : +#prototype /function]\n");
      }
      return *Accessors::FunctionGetPrototype(Handle<JSFunction>::cast(object));
    }
  }

  // Check if the name is trivially convertible to an index and get
  // the element or char if so.
  uint32_t index;
  if (kind() == Code::KEYED_LOAD_IC && name->AsArrayIndex(&index)) {
    // Rewrite to the generic keyed load stub.
    if (FLAG_use_ic) set_target(*generic_stub());
    return Runtime::GetElementOrCharAtOrFail(isolate(), object, index);
  }

  bool use_ic = MigrateDeprecated(object) ? false : FLAG_use_ic;

  // Named lookup in the object.
  LookupResult lookup(isolate());
  LookupForRead(object, name, &lookup);

  // If we did not find a property, check if we need to throw an exception.
  if (!lookup.IsFound()) {
    if (IsUndeclaredGlobal(object)) {
      return ReferenceError("not_defined", name);
    }
    LOG(isolate(), SuspectReadEvent(*name, *object));
  }

  // Update inline cache and stub cache.
  if (use_ic) UpdateCaches(&lookup, object, name);

  PropertyAttributes attr;
  // Get the property.
  Handle<Object> result =
      Object::GetProperty(object, object, &lookup, name, &attr);
  RETURN_IF_EMPTY_HANDLE(isolate(), result);
  // If the property is not present, check if we need to throw an
  // exception.
  if ((lookup.IsInterceptor() || lookup.IsHandler()) &&
      attr == ABSENT && IsUndeclaredGlobal(object)) {
    return ReferenceError("not_defined", name);
  }

  return *result;
}


static bool AddOneReceiverMapIfMissing(MapHandleList* receiver_maps,
                                       Handle<Map> new_receiver_map) {
  ASSERT(!new_receiver_map.is_null());
  for (int current = 0; current < receiver_maps->length(); ++current) {
    if (!receiver_maps->at(current).is_null() &&
        receiver_maps->at(current).is_identical_to(new_receiver_map)) {
      return false;
    }
  }
  receiver_maps->Add(new_receiver_map);
  return true;
}


bool IC::UpdatePolymorphicIC(Handle<HeapType> type,
                             Handle<String> name,
                             Handle<Code> code) {
  if (!code->is_handler()) return false;
  int number_of_valid_types;
  int handler_to_overwrite = -1;

  int number_of_types = types()->length();
  number_of_valid_types = number_of_types;

  for (int i = 0; i < number_of_types; i++) {
    Handle<HeapType> current_type = types()->at(i);
    // Filter out deprecated maps to ensure their instances get migrated.
    if (current_type->IsClass() && current_type->AsClass()->is_deprecated()) {
      number_of_valid_types--;
    // If the receiver type is already in the polymorphic IC, this indicates
    // there was a prototoype chain failure. In that case, just overwrite the
    // handler.
    } else if (type->IsCurrently(current_type)) {
      ASSERT(handler_to_overwrite == -1);
      number_of_valid_types--;
      handler_to_overwrite = i;
    }
  }

  if (number_of_valid_types >= 4) return false;
  if (number_of_types == 0) return false;
  if (handlers()->length() < types()->length()) return false;

  number_of_valid_types++;
  if (handler_to_overwrite >= 0) {
    handlers()->Set(handler_to_overwrite, code);
  } else {
    types()->Add(type);
    handlers()->Add(code);
  }

  Handle<Code> ic = isolate()->stub_cache()->ComputePolymorphicIC(
      kind(), types(), handlers(), number_of_valid_types,
      name, extra_ic_state());
  set_target(*ic);
  return true;
}


Handle<HeapType> IC::CurrentTypeOf(Handle<Object> object, Isolate* isolate) {
  return object->IsJSGlobalObject()
      ? HeapType::Constant(Handle<JSGlobalObject>::cast(object), isolate)
      : HeapType::OfCurrently(object, isolate);
}


Handle<Map> IC::TypeToMap(HeapType* type, Isolate* isolate) {
  if (type->Is(HeapType::Number()))
    return isolate->factory()->heap_number_map();
  if (type->Is(HeapType::Boolean())) return isolate->factory()->oddball_map();
  if (type->IsConstant()) {
    return handle(Handle<JSGlobalObject>::cast(type->AsConstant())->map());
  }
  ASSERT(type->IsClass());
  return type->AsClass();
}


template <class T>
typename T::TypeHandle IC::MapToType(Handle<Map> map,
                                     typename T::Region* region) {
  if (map->instance_type() == HEAP_NUMBER_TYPE) {
    return T::Number(region);
  } else if (map->instance_type() == ODDBALL_TYPE) {
    // The only oddballs that can be recorded in ICs are booleans.
    return T::Boolean(region);
  } else {
    return T::Class(map, region);
  }
}


template
Type* IC::MapToType<Type>(Handle<Map> map, Zone* zone);


template
Handle<HeapType> IC::MapToType<HeapType>(Handle<Map> map, Isolate* region);


void IC::UpdateMonomorphicIC(Handle<HeapType> type,
                             Handle<Code> handler,
                             Handle<String> name) {
  if (!handler->is_handler()) return set_target(*handler);
  Handle<Code> ic = isolate()->stub_cache()->ComputeMonomorphicIC(
      kind(), name, type, handler, extra_ic_state());
  set_target(*ic);
}


void IC::CopyICToMegamorphicCache(Handle<String> name) {
  if (handlers()->length() < types()->length()) return;
  for (int i = 0; i < types()->length(); i++) {
    UpdateMegamorphicCache(*types()->at(i), *name, *handlers()->at(i));
  }
}


bool IC::IsTransitionOfMonomorphicTarget(Map* source_map, Map* target_map) {
  if (source_map == NULL) return true;
  if (target_map == NULL) return false;
  ElementsKind target_elements_kind = target_map->elements_kind();
  bool more_general_transition = IsMoreGeneralElementsKindTransition(
      source_map->elements_kind(), target_elements_kind);
  Map* transitioned_map = more_general_transition
      ? source_map->LookupElementsTransitionMap(target_elements_kind)
      : NULL;
  return transitioned_map == target_map;
}


void IC::PatchCache(Handle<HeapType> type,
                    Handle<String> name,
                    Handle<Code> code) {
  switch (state()) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
    case MONOMORPHIC_PROTOTYPE_FAILURE:
      UpdateMonomorphicIC(type, code, name);
      break;
    case MONOMORPHIC: {
      // For now, call stubs are allowed to rewrite to the same stub. This
      // happens e.g., when the field does not contain a function.
      ASSERT(!target().is_identical_to(code));
      Map* old_map = first_map();
      Code* old_handler = first_handler();
      Map* map = type->IsClass() ? *type->AsClass() : NULL;
      if (old_handler == *code &&
          IsTransitionOfMonomorphicTarget(old_map, map)) {
        UpdateMonomorphicIC(type, code, name);
        break;
      }
      // Fall through.
    }
    case POLYMORPHIC:
      if (!target()->is_keyed_stub()) {
        if (UpdatePolymorphicIC(type, name, code)) break;
        CopyICToMegamorphicCache(name);
      }
      set_target(*megamorphic_stub());
      // Fall through.
    case MEGAMORPHIC:
      UpdateMegamorphicCache(*type, *name, *code);
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
  return isolate->stub_cache()->ComputeLoad(UNINITIALIZED, extra_state);
}


Handle<Code> LoadIC::pre_monomorphic_stub(Isolate* isolate,
                                          ExtraICState extra_state) {
  return isolate->stub_cache()->ComputeLoad(PREMONOMORPHIC, extra_state);
}


Handle<Code> LoadIC::megamorphic_stub() {
  return isolate()->stub_cache()->ComputeLoad(MEGAMORPHIC, extra_ic_state());
}


Handle<Code> LoadIC::SimpleFieldLoad(int offset,
                                     bool inobject,
                                     Representation representation) {
  if (kind() == Code::LOAD_IC) {
    LoadFieldStub stub(inobject, offset, representation);
    return stub.GetCode(isolate());
  } else {
    KeyedLoadFieldStub stub(inobject, offset, representation);
    return stub.GetCode(isolate());
  }
}


void LoadIC::UpdateCaches(LookupResult* lookup,
                          Handle<Object> object,
                          Handle<String> name) {
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    set_target(*pre_monomorphic_stub());
    TRACE_IC("LoadIC", name);
    return;
  }

  Handle<HeapType> type = CurrentTypeOf(object, isolate());
  Handle<Code> code;
  if (!lookup->IsCacheable()) {
    // Bail out if the result is not cacheable.
    code = slow_stub();
  } else if (!lookup->IsProperty()) {
    if (kind() == Code::LOAD_IC) {
      code = isolate()->stub_cache()->ComputeLoadNonexistent(name, type);
    } else {
      code = slow_stub();
    }
  } else {
    code = ComputeHandler(lookup, object, name);
  }

  PatchCache(type, name, code);
  TRACE_IC("LoadIC", name);
}


void IC::UpdateMegamorphicCache(HeapType* type, Name* name, Code* code) {
  // Cache code holding map should be consistent with
  // GenerateMonomorphicCacheProbe.
  Map* map = *TypeToMap(type, isolate());
  isolate()->stub_cache()->Set(name, map, code);
}


Handle<Code> IC::ComputeHandler(LookupResult* lookup,
                                Handle<Object> object,
                                Handle<String> name,
                                Handle<Object> value) {
  InlineCacheHolderFlag cache_holder = GetCodeCacheForObject(*object);
  Handle<HeapObject> stub_holder(GetCodeCacheHolder(
      isolate(), *object, cache_holder));

  Handle<Code> code = isolate()->stub_cache()->FindHandler(
      name, handle(stub_holder->map()), kind(), cache_holder);
  if (!code.is_null()) return code;

  code = CompileHandler(lookup, object, name, value, cache_holder);
  ASSERT(code->is_handler());

  if (code->type() != Code::NORMAL) {
    HeapObject::UpdateMapCodeCache(stub_holder, name, code);
  }

  return code;
}


Handle<Code> LoadIC::CompileHandler(LookupResult* lookup,
                                    Handle<Object> object,
                                    Handle<String> name,
                                    Handle<Object> unused,
                                    InlineCacheHolderFlag cache_holder) {
  if (object->IsString() && name->Equals(isolate()->heap()->length_string())) {
    int length_index = String::kLengthOffset / kPointerSize;
    return SimpleFieldLoad(length_index);
  }

  Handle<HeapType> type = CurrentTypeOf(object, isolate());
  Handle<JSObject> holder(lookup->holder());
  LoadStubCompiler compiler(isolate(), kNoExtraICState, cache_holder, kind());

  switch (lookup->type()) {
    case FIELD: {
      PropertyIndex field = lookup->GetFieldIndex();
      if (object.is_identical_to(holder)) {
        return SimpleFieldLoad(field.translate(holder),
                               field.is_inobject(holder),
                               lookup->representation());
      }
      return compiler.CompileLoadField(
          type, holder, name, field, lookup->representation());
    }
    case CONSTANT: {
      Handle<Object> constant(lookup->GetConstant(), isolate());
      // TODO(2803): Don't compute a stub for cons strings because they cannot
      // be embedded into code.
      if (constant->IsConsString()) break;
      return compiler.CompileLoadConstant(type, holder, name, constant);
    }
    case NORMAL:
      if (kind() != Code::LOAD_IC) break;
      if (holder->IsGlobalObject()) {
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(holder);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        Handle<Code> code = compiler.CompileLoadGlobal(
            type, global, cell, name, lookup->IsDontDelete());
        // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
        Handle<HeapObject> stub_holder(GetCodeCacheHolder(
            isolate(), *object, cache_holder));
        HeapObject::UpdateMapCodeCache(stub_holder, name, code);
        return code;
      }
      // There is only one shared stub for loading normalized
      // properties. It does not traverse the prototype chain, so the
      // property must be found in the object for the stub to be
      // applicable.
      if (!object.is_identical_to(holder)) break;
      return isolate()->builtins()->LoadIC_Normal();
    case CALLBACKS: {
      // Use simple field loads for some well-known callback properties.
      if (object->IsJSObject()) {
        Handle<JSObject> receiver = Handle<JSObject>::cast(object);
        Handle<HeapType> type = IC::MapToType<HeapType>(
            handle(receiver->map()), isolate());
        int object_offset;
        if (Accessors::IsJSObjectFieldAccessor<HeapType>(
                type, name, &object_offset)) {
          return SimpleFieldLoad(object_offset / kPointerSize);
        }
      }

      Handle<Object> callback(lookup->GetCallbackObject(), isolate());
      if (callback->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(callback);
        if (v8::ToCData<Address>(info->getter()) == 0) break;
        if (!info->IsCompatibleReceiver(*object)) break;
        return compiler.CompileLoadCallback(type, holder, name, info);
      } else if (callback->IsAccessorPair()) {
        Handle<Object> getter(Handle<AccessorPair>::cast(callback)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (holder->IsGlobalObject()) break;
        if (!holder->HasFastProperties()) break;
        Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
        if (!object->IsJSObject() &&
            !function->IsBuiltin() &&
            function->shared()->is_classic_mode()) {
          // Calling non-strict non-builtins with a value as the receiver
          // requires boxing.
          break;
        }
        CallOptimization call_optimization(function);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(object, holder)) {
          return compiler.CompileLoadCallback(
              type, holder, name, call_optimization);
        }
        return compiler.CompileLoadViaGetter(type, holder, name, function);
      }
      // TODO(dcarney): Handle correctly.
      if (callback->IsDeclaredAccessorInfo()) break;
      ASSERT(callback->IsForeign());
      // No IC support for old-style native accessors.
      break;
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(*holder));
      return compiler.CompileLoadInterceptor(type, holder, name);
    default:
      break;
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
        key = Handle<Smi>(Smi::FromInt(int_value), isolate);
      }
    }
  } else if (key->IsUndefined()) {
    key = isolate->factory()->undefined_string();
  }
  return key;
}


Handle<Code> KeyedLoadIC::LoadElementStub(Handle<JSObject> receiver) {
  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "non-NORMAL target type");
    return generic_stub();
  }

  Handle<Map> receiver_map(receiver->map(), isolate());
  MapHandleList target_receiver_maps;
  if (state() == UNINITIALIZED || state() == PREMONOMORPHIC) {
    // Optimistically assume that ICs that haven't reached the MONOMORPHIC state
    // yet will do so and stay there.
    return isolate()->stub_cache()->ComputeKeyedLoadElement(receiver_map);
  }

  if (target().is_identical_to(string_stub())) {
    target_receiver_maps.Add(isolate()->factory()->string_map());
  } else {
    GetMapsFromTypes(&target_receiver_maps);
    if (target_receiver_maps.length() == 0) {
      return isolate()->stub_cache()->ComputeKeyedLoadElement(receiver_map);
    }
  }

  // The first time a receiver is seen that is a transitioned version of the
  // previous monomorphic receiver type, assume the new ElementsKind is the
  // monomorphic type. This benefits global arrays that only transition
  // once, and all call sites accessing them are faster if they remain
  // monomorphic. If this optimistic assumption is not true, the IC will
  // miss again and it will become polymorphic and support both the
  // untransitioned and transitioned maps.
  if (state() == MONOMORPHIC &&
      IsMoreGeneralElementsKindTransition(
          target_receiver_maps.at(0)->elements_kind(),
          receiver->GetElementsKind())) {
    return isolate()->stub_cache()->ComputeKeyedLoadElement(receiver_map);
  }

  ASSERT(state() != GENERIC);

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map)) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "same map added twice");
    return generic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "max polymorph exceeded");
    return generic_stub();
  }

  return isolate()->stub_cache()->ComputeLoadElementPolymorphic(
      &target_receiver_maps);
}


MaybeObject* KeyedLoadIC::Load(Handle<Object> object, Handle<Object> key) {
  if (MigrateDeprecated(object)) {
    return Runtime::GetObjectPropertyOrFail(isolate(), object, key);
  }

  MaybeObject* maybe_object = NULL;
  Handle<Code> stub = generic_stub();

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString()) {
    maybe_object = LoadIC::Load(object, Handle<String>::cast(key));
    if (maybe_object->IsFailure()) return maybe_object;
  } else if (FLAG_use_ic && !object->IsAccessCheckNeeded()) {
    ASSERT(!object->IsAccessCheckNeeded());
    if (object->IsString() && key->IsNumber()) {
      if (state() == UNINITIALIZED) stub = string_stub();
    } else if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      if (receiver->elements()->map() ==
          isolate()->heap()->non_strict_arguments_elements_map()) {
        stub = non_strict_arguments_stub();
      } else if (receiver->HasIndexedInterceptor()) {
        stub = indexed_interceptor_stub();
      } else if (!key->ToSmi()->IsFailure() &&
                 (!target().is_identical_to(non_strict_arguments_stub()))) {
        stub = LoadElementStub(receiver);
      }
    }
  }

  if (!is_target_set()) {
    if (*stub == *generic_stub()) {
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "set generic");
    }
    ASSERT(!stub.is_null());
    set_target(*stub);
    TRACE_IC("LoadIC", key);
  }

  if (maybe_object != NULL) return maybe_object;
  return Runtime::GetObjectPropertyOrFail(isolate(), object, key);
}


static bool LookupForWrite(Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value,
                           LookupResult* lookup,
                           IC* ic) {
  Handle<JSObject> holder = receiver;
  receiver->Lookup(*name, lookup);
  if (lookup->IsFound()) {
    if (lookup->IsInterceptor() && !HasInterceptorSetter(lookup->holder())) {
      receiver->LocalLookupRealNamedProperty(*name, lookup);
      if (!lookup->IsFound()) return false;
    }

    if (lookup->IsReadOnly() || !lookup->IsCacheable()) return false;
    if (lookup->holder() == *receiver) return lookup->CanHoldValue(value);
    if (lookup->IsPropertyCallbacks()) return true;
    // JSGlobalProxy either stores on the global object in the prototype, or
    // goes into the runtime if access checks are needed, so this is always
    // safe.
    if (receiver->IsJSGlobalProxy()) return true;
    // Currently normal holders in the prototype chain are not supported. They
    // would require a runtime positive lookup and verification that the details
    // have not changed.
    if (lookup->IsInterceptor() || lookup->IsNormal()) return false;
    holder = Handle<JSObject>(lookup->holder(), lookup->isolate());
  }

  // While normally LookupTransition gets passed the receiver, in this case we
  // pass the holder of the property that we overwrite. This keeps the holder in
  // the LookupResult intact so we can later use it to generate a prototype
  // chain check. This avoids a double lookup, but requires us to pass in the
  // receiver when trying to fetch extra information from the transition.
  receiver->map()->LookupTransition(*holder, *name, lookup);
  if (!lookup->IsTransition()) return false;
  PropertyDetails target_details = lookup->GetTransitionDetails();
  if (target_details.IsReadOnly()) return false;

  // If the value that's being stored does not fit in the field that the
  // instance would transition to, create a new transition that fits the value.
  // This has to be done before generating the IC, since that IC will embed the
  // transition target.
  // Ensure the instance and its map were migrated before trying to update the
  // transition target.
  ASSERT(!receiver->map()->is_deprecated());
  if (!value->FitsRepresentation(target_details.representation())) {
    Handle<Map> target(lookup->GetTransitionTarget());
    Map::GeneralizeRepresentation(
        target, target->LastAdded(),
        value->OptimalRepresentation(), FORCE_FIELD);
    // Lookup the transition again since the transition tree may have changed
    // entirely by the migration above.
    receiver->map()->LookupTransition(*holder, *name, lookup);
    if (!lookup->IsTransition()) return false;
    ic->MarkMonomorphicPrototypeFailure();
  }
  return true;
}


MaybeObject* StoreIC::Store(Handle<Object> object,
                            Handle<String> name,
                            Handle<Object> value,
                            JSReceiver::StoreFromKeyed store_mode) {
  if (MigrateDeprecated(object) || object->IsJSProxy()) {
    Handle<Object> result = JSReceiver::SetProperty(
        Handle<JSReceiver>::cast(object), name, value, NONE, strict_mode());
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *result;
  }

  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_store", object, name);
  }

  // The length property of string values is read-only. Throw in strict mode.
  if (strict_mode() == kStrictMode && object->IsString() &&
      name->Equals(isolate()->heap()->length_string())) {
    return TypeError("strict_read_only_property", object, name);
  }

  // Ignore other stores where the receiver is not a JSObject.
  // TODO(1475): Must check prototype chains of object wrappers.
  if (!object->IsJSObject()) return *value;

  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  // Check if the given name is an array index.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<Object> result =
        JSObject::SetElement(receiver, index, value, NONE, strict_mode());
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *value;
  }

  // Observed objects are always modified through the runtime.
  if (FLAG_harmony_observation && receiver->map()->is_observed()) {
    Handle<Object> result = JSReceiver::SetProperty(
        receiver, name, value, NONE, strict_mode(), store_mode);
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *result;
  }

  // Use specialized code for setting the length of arrays with fast
  // properties. Slow properties might indicate redefinition of the length
  // property. Note that when redefined using Object.freeze, it's possible
  // to have fast properties but a read-only length.
  if (FLAG_use_ic &&
      receiver->IsJSArray() &&
      name->Equals(isolate()->heap()->length_string()) &&
      Handle<JSArray>::cast(receiver)->AllowsSetElementsLength() &&
      receiver->HasFastProperties() &&
      !receiver->map()->is_frozen()) {
    Handle<Code> stub =
        StoreArrayLengthStub(kind(), strict_mode()).GetCode(isolate());
    set_target(*stub);
    TRACE_IC("StoreIC", name);
    Handle<Object> result = JSReceiver::SetProperty(
        receiver, name, value, NONE, strict_mode(), store_mode);
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *result;
  }

  LookupResult lookup(isolate());
  bool can_store = LookupForWrite(receiver, name, value, &lookup, this);
  if (!can_store &&
      strict_mode() == kStrictMode &&
      !(lookup.IsProperty() && lookup.IsReadOnly()) &&
      object->IsGlobalObject()) {
    // Strict mode doesn't allow setting non-existent global property.
    return ReferenceError("not_defined", name);
  }
  if (FLAG_use_ic) {
    if (state() == UNINITIALIZED) {
      Handle<Code> stub = pre_monomorphic_stub();
      set_target(*stub);
      TRACE_IC("StoreIC", name);
    } else if (can_store) {
      UpdateCaches(&lookup, receiver, name, value);
    } else if (!name->IsCacheable(isolate()) ||
               lookup.IsNormal() ||
               (lookup.IsField() && lookup.CanHoldValue(value))) {
      Handle<Code> stub = generic_stub();
      set_target(*stub);
    }
  }

  // Set the property.
  Handle<Object> result = JSReceiver::SetProperty(
      receiver, name, value, NONE, strict_mode(), store_mode);
  RETURN_IF_EMPTY_HANDLE(isolate(), result);
  return *result;
}


Handle<Code> StoreIC::initialize_stub(Isolate* isolate,
                                      StrictModeFlag strict_mode) {
  ExtraICState extra_state = ComputeExtraICState(strict_mode);
  Handle<Code> ic = isolate->stub_cache()->ComputeStore(
      UNINITIALIZED, extra_state);
  return ic;
}


Handle<Code> StoreIC::megamorphic_stub() {
  return isolate()->stub_cache()->ComputeStore(MEGAMORPHIC, extra_ic_state());
}


Handle<Code> StoreIC::generic_stub() const {
  return isolate()->stub_cache()->ComputeStore(GENERIC, extra_ic_state());
}


Handle<Code> StoreIC::pre_monomorphic_stub(Isolate* isolate,
                                           StrictModeFlag strict_mode) {
  ExtraICState state = ComputeExtraICState(strict_mode);
  return isolate->stub_cache()->ComputeStore(PREMONOMORPHIC, state);
}


void StoreIC::UpdateCaches(LookupResult* lookup,
                           Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value) {
  ASSERT(lookup->IsFound());

  // These are not cacheable, so we never see such LookupResults here.
  ASSERT(!lookup->IsHandler());

  Handle<Code> code = ComputeHandler(lookup, receiver, name, value);

  PatchCache(CurrentTypeOf(receiver, isolate()), name, code);
  TRACE_IC("StoreIC", name);
}


Handle<Code> StoreIC::CompileHandler(LookupResult* lookup,
                                     Handle<Object> object,
                                     Handle<String> name,
                                     Handle<Object> value,
                                     InlineCacheHolderFlag cache_holder) {
  if (object->IsAccessCheckNeeded()) return slow_stub();
  ASSERT(cache_holder == OWN_MAP);
  // This is currently guaranteed by checks in StoreIC::Store.
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  Handle<JSObject> holder(lookup->holder());
  // Handlers do not use strict mode.
  StoreStubCompiler compiler(isolate(), kNonStrictMode, kind());
  switch (lookup->type()) {
    case FIELD:
      return compiler.CompileStoreField(receiver, lookup, name);
    case TRANSITION: {
      // Explicitly pass in the receiver map since LookupForWrite may have
      // stored something else than the receiver in the holder.
      Handle<Map> transition(lookup->GetTransitionTarget());
      PropertyDetails details = transition->GetLastDescriptorDetails();

      if (details.type() == CALLBACKS || details.attributes() != NONE) break;

      return compiler.CompileStoreTransition(
          receiver, lookup, transition, name);
    }
    case NORMAL:
      if (kind() == Code::KEYED_STORE_IC) break;
      if (receiver->IsJSGlobalProxy() || receiver->IsGlobalObject()) {
        // The stub generated for the global object picks the value directly
        // from the property cell. So the property must be directly on the
        // global object.
        Handle<GlobalObject> global = receiver->IsJSGlobalProxy()
            ? handle(GlobalObject::cast(receiver->GetPrototype()))
            : Handle<GlobalObject>::cast(receiver);
        Handle<PropertyCell> cell(global->GetPropertyCell(lookup), isolate());
        Handle<HeapType> union_type = PropertyCell::UpdatedType(cell, value);
        StoreGlobalStub stub(
            union_type->IsConstant(), receiver->IsJSGlobalProxy());
        Handle<Code> code = stub.GetCodeCopyFromTemplate(
            isolate(), *global, *cell);
        // TODO(verwaest): Move caching of these NORMAL stubs outside as well.
        HeapObject::UpdateMapCodeCache(receiver, name, code);
        return code;
      }
      ASSERT(holder.is_identical_to(receiver));
      return isolate()->builtins()->StoreIC_Normal();
    case CALLBACKS: {
      if (kind() == Code::KEYED_STORE_IC) break;
      Handle<Object> callback(lookup->GetCallbackObject(), isolate());
      if (callback->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(callback);
        if (v8::ToCData<Address>(info->setter()) == 0) break;
        if (!holder->HasFastProperties()) break;
        if (!info->IsCompatibleReceiver(*receiver)) break;
        return compiler.CompileStoreCallback(receiver, holder, name, info);
      } else if (callback->IsAccessorPair()) {
        Handle<Object> setter(
            Handle<AccessorPair>::cast(callback)->setter(), isolate());
        if (!setter->IsJSFunction()) break;
        if (holder->IsGlobalObject()) break;
        if (!holder->HasFastProperties()) break;
        Handle<JSFunction> function = Handle<JSFunction>::cast(setter);
        CallOptimization call_optimization(function);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(receiver, holder)) {
          return compiler.CompileStoreCallback(
              receiver, holder, name, call_optimization);
        }
        return compiler.CompileStoreViaSetter(
            receiver, holder, name, Handle<JSFunction>::cast(setter));
      }
      // TODO(dcarney): Handle correctly.
      if (callback->IsDeclaredAccessorInfo()) break;
      ASSERT(callback->IsForeign());
      // No IC support for old-style native accessors.
      break;
    }
    case INTERCEPTOR:
      if (kind() == Code::KEYED_STORE_IC) break;
      ASSERT(HasInterceptorSetter(*holder));
      return compiler.CompileStoreInterceptor(receiver, name);
    case CONSTANT:
      break;
    case NONEXISTENT:
    case HANDLER:
      UNREACHABLE();
      break;
  }
  return slow_stub();
}


Handle<Code> KeyedStoreIC::StoreElementStub(Handle<JSObject> receiver,
                                            KeyedAccessStoreMode store_mode) {
  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "non-NORMAL target type");
    return generic_stub();
  }

  Handle<Map> receiver_map(receiver->map(), isolate());
  if (state() == UNINITIALIZED || state() == PREMONOMORPHIC) {
    // Optimistically assume that ICs that haven't reached the MONOMORPHIC state
    // yet will do so and stay there.
    Handle<Map> monomorphic_map = ComputeTransitionedMap(receiver, store_mode);
    store_mode = GetNonTransitioningStoreMode(store_mode);
    return isolate()->stub_cache()->ComputeKeyedStoreElement(
        monomorphic_map, strict_mode(), store_mode);
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different GetNonTransitioningStoreMode IC that handles a
  // superset of the original IC. Handle those here if the receiver map hasn't
  // changed or it has transitioned to a more general kind.
  KeyedAccessStoreMode old_store_mode =
      KeyedStoreIC::GetKeyedAccessStoreMode(target()->extra_ic_state());
  if (state() == MONOMORPHIC) {
      // If the "old" and "new" maps are in the same elements map family, stay
      // MONOMORPHIC and use the map for the most generic ElementsKind.
    Handle<Map> transitioned_map = receiver_map;
    if (IsTransitionStoreMode(store_mode)) {
      transitioned_map = ComputeTransitionedMap(receiver, store_mode);
    }
    if (IsTransitionOfMonomorphicTarget(first_map(), *transitioned_map)) {
      // Element family is the same, use the "worst" case map.
      store_mode = GetNonTransitioningStoreMode(store_mode);
      return isolate()->stub_cache()->ComputeKeyedStoreElement(
          transitioned_map, strict_mode(), store_mode);
    } else if (first_map() == receiver->map() &&
               old_store_mode == STANDARD_STORE &&
               (IsGrowStoreMode(store_mode) ||
                store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
                store_mode == STORE_NO_TRANSITION_HANDLE_COW)) {
      // A "normal" IC that handles stores can switch to a version that can
      // grow at the end of the array, handle OOB accesses or copy COW arrays
      // and still stay MONOMORPHIC.
      return isolate()->stub_cache()->ComputeKeyedStoreElement(
          receiver_map, strict_mode(), store_mode);
    }
  }

  ASSERT(state() != GENERIC);

  MapHandleList target_receiver_maps;
  GetMapsFromTypes(&target_receiver_maps);

  bool map_added =
      AddOneReceiverMapIfMissing(&target_receiver_maps, receiver_map);

  if (IsTransitionStoreMode(store_mode)) {
    Handle<Map> transitioned_receiver_map =
        ComputeTransitionedMap(receiver, store_mode);
    map_added |= AddOneReceiverMapIfMissing(&target_receiver_maps,
                                            transitioned_receiver_map);
  }

  if (!map_added) {
    // If the miss wasn't due to an unseen map, a polymorphic stub
    // won't help, use the generic stub.
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "same map added twice");
    return generic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "max polymorph exceeded");
    return generic_stub();
  }

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // generic stub must be used.
  store_mode = GetNonTransitioningStoreMode(store_mode);
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      TRACE_GENERIC_IC(isolate(), "KeyedIC", "store mode mismatch");
      return generic_stub();
    }
  }

  // If the store mode isn't the standard mode, make sure that all polymorphic
  // receivers are either external arrays, or all "normal" arrays. Otherwise,
  // use the generic stub.
  if (store_mode != STANDARD_STORE) {
    int external_arrays = 0;
    for (int i = 0; i < target_receiver_maps.length(); ++i) {
      if (target_receiver_maps[i]->has_external_array_elements() ||
          target_receiver_maps[i]->has_fixed_typed_array_elements()) {
        external_arrays++;
      }
    }
    if (external_arrays != 0 &&
        external_arrays != target_receiver_maps.length()) {
      TRACE_GENERIC_IC(isolate(), "KeyedIC",
          "unsupported combination of external and normal arrays");
      return generic_stub();
    }
  }

  return isolate()->stub_cache()->ComputeStoreElementPolymorphic(
      &target_receiver_maps, store_mode, strict_mode());
}


Handle<Map> KeyedStoreIC::ComputeTransitionedMap(
    Handle<JSObject> receiver,
    KeyedAccessStoreMode store_mode) {
  switch (store_mode) {
    case STORE_TRANSITION_SMI_TO_OBJECT:
    case STORE_TRANSITION_DOUBLE_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT:
      return JSObject::GetElementsTransitionMap(receiver, FAST_ELEMENTS);
    case STORE_TRANSITION_SMI_TO_DOUBLE:
    case STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE:
      return JSObject::GetElementsTransitionMap(receiver, FAST_DOUBLE_ELEMENTS);
    case STORE_TRANSITION_HOLEY_SMI_TO_OBJECT:
    case STORE_TRANSITION_HOLEY_DOUBLE_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_OBJECT:
    case STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT:
      return JSObject::GetElementsTransitionMap(receiver,
                                                FAST_HOLEY_ELEMENTS);
    case STORE_TRANSITION_HOLEY_SMI_TO_DOUBLE:
    case STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_DOUBLE:
      return JSObject::GetElementsTransitionMap(receiver,
                                                FAST_HOLEY_DOUBLE_ELEMENTS);
    case STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS:
      ASSERT(receiver->map()->has_external_array_elements());
      // Fall through
    case STORE_NO_TRANSITION_HANDLE_COW:
    case STANDARD_STORE:
    case STORE_AND_GROW_NO_TRANSITION:
      return Handle<Map>(receiver->map(), isolate());
  }
  return Handle<Map>::null();
}


bool IsOutOfBoundsAccess(Handle<JSObject> receiver,
                         int index) {
  if (receiver->IsJSArray()) {
    return JSArray::cast(*receiver)->length()->IsSmi() &&
        index >= Smi::cast(JSArray::cast(*receiver)->length())->value();
  }
  return index >= receiver->elements()->length();
}


KeyedAccessStoreMode KeyedStoreIC::GetStoreMode(Handle<JSObject> receiver,
                                                Handle<Object> key,
                                                Handle<Object> value) {
  ASSERT(!key->ToSmi()->IsFailure());
  Smi* smi_key = NULL;
  key->ToSmi()->To(&smi_key);
  int index = smi_key->value();
  bool oob_access = IsOutOfBoundsAccess(receiver, index);
  bool allow_growth = receiver->IsJSArray() && oob_access;
  if (allow_growth) {
    // Handle growing array in stub if necessary.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_DOUBLE;
        } else {
          return STORE_AND_GROW_TRANSITION_SMI_TO_DOUBLE;
        }
      }
      if (value->IsHeapObject()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_SMI_TO_OBJECT;
        } else {
          return STORE_AND_GROW_TRANSITION_SMI_TO_OBJECT;
        }
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_AND_GROW_TRANSITION_HOLEY_DOUBLE_TO_OBJECT;
        } else {
          return STORE_AND_GROW_TRANSITION_DOUBLE_TO_OBJECT;
        }
      }
    }
    return STORE_AND_GROW_NO_TRANSITION;
  } else {
    // Handle only in-bounds elements accesses.
    if (receiver->HasFastSmiElements()) {
      if (value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_SMI_TO_DOUBLE;
        } else {
          return STORE_TRANSITION_SMI_TO_DOUBLE;
        }
      } else if (value->IsHeapObject()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_SMI_TO_OBJECT;
        } else {
          return STORE_TRANSITION_SMI_TO_OBJECT;
        }
      }
    } else if (receiver->HasFastDoubleElements()) {
      if (!value->IsSmi() && !value->IsHeapNumber()) {
        if (receiver->HasFastHoleyElements()) {
          return STORE_TRANSITION_HOLEY_DOUBLE_TO_OBJECT;
        } else {
          return STORE_TRANSITION_DOUBLE_TO_OBJECT;
        }
      }
    }
    if (!FLAG_trace_external_array_abuse &&
        receiver->map()->has_external_array_elements() && oob_access) {
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


MaybeObject* KeyedStoreIC::Store(Handle<Object> object,
                                 Handle<Object> key,
                                 Handle<Object> value) {
  if (MigrateDeprecated(object)) {
    Handle<Object> result = Runtime::SetObjectProperty(isolate(), object,
                                                       key,
                                                       value,
                                                       NONE,
                                                       strict_mode());
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *result;
  }

  // Check for non-string values that can be converted into an
  // internalized string directly or is representable as a smi.
  key = TryConvertKey(key, isolate());

  MaybeObject* maybe_object = NULL;
  Handle<Code> stub = generic_stub();

  if (key->IsInternalizedString()) {
    maybe_object = StoreIC::Store(object,
                                  Handle<String>::cast(key),
                                  value,
                                  JSReceiver::MAY_BE_STORE_FROM_KEYED);
    if (maybe_object->IsFailure()) return maybe_object;
  } else {
    bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded() &&
        !(FLAG_harmony_observation && object->IsJSObject() &&
          JSObject::cast(*object)->map()->is_observed());
    if (use_ic && !object->IsSmi()) {
      // Don't use ICs for maps of the objects in Array's prototype chain. We
      // expect to be able to trap element sets to objects with those maps in
      // the runtime to enable optimization of element hole access.
      Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
      if (heap_object->map()->IsMapInArrayPrototypeChain()) use_ic = false;
    }

    if (use_ic) {
      ASSERT(!object->IsAccessCheckNeeded());

      if (object->IsJSObject()) {
        Handle<JSObject> receiver = Handle<JSObject>::cast(object);
        bool key_is_smi_like = key->IsSmi() || !key->ToSmi()->IsFailure();
        if (receiver->elements()->map() ==
            isolate()->heap()->non_strict_arguments_elements_map()) {
          stub = non_strict_arguments_stub();
        } else if (key_is_smi_like &&
                   !(target().is_identical_to(non_strict_arguments_stub()))) {
          // We should go generic if receiver isn't a dictionary, but our
          // prototype chain does have dictionary elements. This ensures that
          // other non-dictionary receivers in the polymorphic case benefit
          // from fast path keyed stores.
          if (!(receiver->map()->DictionaryElementsInPrototypeChainOnly())) {
            KeyedAccessStoreMode store_mode =
                GetStoreMode(receiver, key, value);
            stub = StoreElementStub(receiver, store_mode);
          }
        }
      }
    }
  }

  if (!is_target_set()) {
    if (*stub == *generic_stub()) {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "set generic");
    }
    ASSERT(!stub.is_null());
    set_target(*stub);
    TRACE_IC("StoreIC", key);
  }

  if (maybe_object) return maybe_object;
  Handle<Object> result = Runtime::SetObjectProperty(isolate(), object, key,
                                                     value,
                                                     NONE,
                                                     strict_mode());
  RETURN_IF_EMPTY_HANDLE(isolate(), result);
  return *result;
}


#undef TRACE_IC


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

// Used from ic-<arch>.cc.
// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, LoadIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  LoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  return ic.Load(receiver, key);
}


// Used from ic-<arch>.cc
RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Load(receiver, key);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Load(receiver, key);
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, StoreIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2));
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2));
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_ArrayLength) {
  SealHandleScope shs(isolate);

  ASSERT(args.length() == 2);
  JSArray* receiver = JSArray::cast(args[0]);
  Object* len = args[1];

  // The generated code should filter out non-Smis before we get here.
  ASSERT(len->IsSmi());

#ifdef DEBUG
  // The length property has to be a writable callback property.
  LookupResult debug_lookup(isolate);
  receiver->LocalLookup(isolate->heap()->length_string(), &debug_lookup);
  ASSERT(debug_lookup.IsPropertyCallbacks() && !debug_lookup.IsReadOnly());
#endif

  Object* result;
  MaybeObject* maybe_result = receiver->SetElementsLength(len);
  if (!maybe_result->To(&result)) return maybe_result;

  return len;
}


// Extend storage is called in a store inline cache when
// it is necessary to extend the properties array of a
// JSObject.
RUNTIME_FUNCTION(MaybeObject*, SharedStoreIC_ExtendStorage) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  // Convert the parameters
  JSObject* object = JSObject::cast(args[0]);
  Map* transition = Map::cast(args[1]);
  Object* value = args[2];

  // Check the object has run out out property space.
  ASSERT(object->HasFastProperties());
  ASSERT(object->map()->unused_property_fields() == 0);

  // Expand the properties array.
  FixedArray* old_storage = object->properties();
  int new_unused = transition->unused_property_fields();
  int new_size = old_storage->length() + new_unused + 1;
  Object* result;
  MaybeObject* maybe_result = old_storage->CopySize(new_size);
  if (!maybe_result->ToObject(&result)) return maybe_result;

  FixedArray* new_storage = FixedArray::cast(result);

  Object* to_store = value;

  if (FLAG_track_double_fields) {
    DescriptorArray* descriptors = transition->instance_descriptors();
    PropertyDetails details = descriptors->GetDetails(transition->LastAdded());
    if (details.representation().IsDouble()) {
      MaybeObject* maybe_storage =
          isolate->heap()->AllocateHeapNumber(value->Number());
      if (!maybe_storage->To(&to_store)) return maybe_storage;
    }
  }

  new_storage->set(old_storage->length(), to_store);

  // Set the new property value and do the map transition.
  object->set_properties(new_storage);
  object->set_map(transition);

  // Return the stored value.
  return value;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2));
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2));
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_Slow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = ic.strict_mode();
  Handle<Object> result = Runtime::SetObjectProperty(isolate, object, key,
                                                     value,
                                                     NONE,
                                                     strict_mode);
  RETURN_IF_EMPTY_HANDLE(isolate, result);
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = ic.strict_mode();
  Handle<Object> result = Runtime::SetObjectProperty(isolate, object, key,
                                                     value,
                                                     NONE,
                                                     strict_mode);
  RETURN_IF_EMPTY_HANDLE(isolate, result);
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, ElementsTransitionAndStoreIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> value = args.at<Object>(0);
  Handle<Map> map = args.at<Map>(1);
  Handle<Object> key = args.at<Object>(2);
  Handle<Object> object = args.at<Object>(3);
  StrictModeFlag strict_mode = ic.strict_mode();
  if (object->IsJSObject()) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object),
                                     map->elements_kind());
  }
  Handle<Object> result = Runtime::SetObjectProperty(isolate, object, key,
                                                     value,
                                                     NONE,
                                                     strict_mode);
  RETURN_IF_EMPTY_HANDLE(isolate, result);
  return *result;
}


BinaryOpIC::State::State(ExtraICState extra_ic_state) {
  // We don't deserialize the SSE2 Field, since this is only used to be able
  // to include SSE2 as well as non-SSE2 versions in the snapshot. For code
  // generation we always want it to reflect the current state.
  op_ = static_cast<Token::Value>(
      FIRST_TOKEN + OpField::decode(extra_ic_state));
  mode_ = OverwriteModeField::decode(extra_ic_state);
  fixed_right_arg_ = Maybe<int>(
      HasFixedRightArgField::decode(extra_ic_state),
      1 << FixedRightArgValueField::decode(extra_ic_state));
  left_kind_ = LeftKindField::decode(extra_ic_state);
  if (fixed_right_arg_.has_value) {
    right_kind_ = Smi::IsValid(fixed_right_arg_.value) ? SMI : INT32;
  } else {
    right_kind_ = RightKindField::decode(extra_ic_state);
  }
  result_kind_ = ResultKindField::decode(extra_ic_state);
  ASSERT_LE(FIRST_TOKEN, op_);
  ASSERT_LE(op_, LAST_TOKEN);
}


ExtraICState BinaryOpIC::State::GetExtraICState() const {
  bool sse2 = (Max(result_kind_, Max(left_kind_, right_kind_)) > SMI &&
               CpuFeatures::IsSafeForSnapshot(SSE2));
  ExtraICState extra_ic_state =
      SSE2Field::encode(sse2) |
      OpField::encode(op_ - FIRST_TOKEN) |
      OverwriteModeField::encode(mode_) |
      LeftKindField::encode(left_kind_) |
      ResultKindField::encode(result_kind_) |
      HasFixedRightArgField::encode(fixed_right_arg_.has_value);
  if (fixed_right_arg_.has_value) {
    extra_ic_state = FixedRightArgValueField::update(
        extra_ic_state, WhichPowerOf2(fixed_right_arg_.value));
  } else {
    extra_ic_state = RightKindField::update(extra_ic_state, right_kind_);
  }
  return extra_ic_state;
}


// static
void BinaryOpIC::State::GenerateAheadOfTime(
    Isolate* isolate, void (*Generate)(Isolate*, const State&)) {
  // TODO(olivf) We should investigate why adding stubs to the snapshot is so
  // expensive at runtime. When solved we should be able to add most binops to
  // the snapshot instead of hand-picking them.
  // Generated list of commonly used stubs
#define GENERATE(op, left_kind, right_kind, result_kind, mode)  \
  do {                                                          \
    State state(op, mode);                                      \
    state.left_kind_ = left_kind;                               \
    state.fixed_right_arg_.has_value = false;                   \
    state.right_kind_ = right_kind;                             \
    state.result_kind_ = result_kind;                           \
    Generate(isolate, state);                                   \
  } while (false)
  GENERATE(Token::ADD, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::ADD, SMI, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::ADD, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, INT32, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, NUMBER, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, SMI, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, NUMBER, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_OR, NUMBER, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, SMI, INT32, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, INT32, INT32, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, NUMBER, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::BIT_XOR, NUMBER, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, NUMBER, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, INT32, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, INT32, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::DIV, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::DIV, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::DIV, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::MOD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::MUL, INT32, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::MUL, SMI, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::MUL, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MUL, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SAR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SAR, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SAR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SAR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, INT32, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::SHL, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHL, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, NUMBER, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, SMI, SMI, INT32, NO_OVERWRITE);
  GENERATE(Token::SHL, SMI, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SHL, SMI, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHL, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHL, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHL, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, INT32, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, INT32, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, INT32, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, NUMBER, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, NUMBER, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, NUMBER, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SHR, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SHR, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SHR, SMI, SMI, SMI, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, INT32, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::SUB, INT32, INT32, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SUB, INT32, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, INT32, SMI, INT32, OVERWRITE_LEFT);
  GENERATE(Token::SUB, INT32, SMI, INT32, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, NUMBER, INT32, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, INT32, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, SMI, INT32, INT32, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_LEFT);
  GENERATE(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT);
  GENERATE(Token::SUB, SMI, SMI, SMI, NO_OVERWRITE);
  GENERATE(Token::SUB, SMI, SMI, SMI, OVERWRITE_LEFT);
  GENERATE(Token::SUB, SMI, SMI, SMI, OVERWRITE_RIGHT);
#undef GENERATE
#define GENERATE(op, left_kind, fixed_right_arg_value, result_kind, mode) \
  do {                                                                    \
    State state(op, mode);                                                \
    state.left_kind_ = left_kind;                                         \
    state.fixed_right_arg_.has_value = true;                              \
    state.fixed_right_arg_.value = fixed_right_arg_value;                 \
    state.right_kind_ = SMI;                                              \
    state.result_kind_ = result_kind;                                     \
    Generate(isolate, state);                                             \
  } while (false)
  GENERATE(Token::MOD, SMI, 2, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 4, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 4, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, 8, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 16, SMI, OVERWRITE_LEFT);
  GENERATE(Token::MOD, SMI, 32, SMI, NO_OVERWRITE);
  GENERATE(Token::MOD, SMI, 2048, SMI, NO_OVERWRITE);
#undef GENERATE
}


Type* BinaryOpIC::State::GetResultType(Zone* zone) const {
  Kind result_kind = result_kind_;
  if (HasSideEffects()) {
    result_kind = NONE;
  } else if (result_kind == GENERIC && op_ == Token::ADD) {
    return Type::Union(Type::Number(zone), Type::String(zone), zone);
  } else if (result_kind == NUMBER && op_ == Token::SHR) {
    return Type::Unsigned32(zone);
  }
  ASSERT_NE(GENERIC, result_kind);
  return KindToType(result_kind, zone);
}


void BinaryOpIC::State::Print(StringStream* stream) const {
  stream->Add("(%s", Token::Name(op_));
  if (mode_ == OVERWRITE_LEFT) stream->Add("_ReuseLeft");
  else if (mode_ == OVERWRITE_RIGHT) stream->Add("_ReuseRight");
  if (CouldCreateAllocationMementos()) stream->Add("_CreateAllocationMementos");
  stream->Add(":%s*", KindToString(left_kind_));
  if (fixed_right_arg_.has_value) {
    stream->Add("%d", fixed_right_arg_.value);
  } else {
    stream->Add("%s", KindToString(right_kind_));
  }
  stream->Add("->%s)", KindToString(result_kind_));
}


void BinaryOpIC::State::Update(Handle<Object> left,
                               Handle<Object> right,
                               Handle<Object> result) {
  ExtraICState old_extra_ic_state = GetExtraICState();

  left_kind_ = UpdateKind(left, left_kind_);
  right_kind_ = UpdateKind(right, right_kind_);

  int32_t fixed_right_arg_value = 0;
  bool has_fixed_right_arg =
      op_ == Token::MOD &&
      right->ToInt32(&fixed_right_arg_value) &&
      fixed_right_arg_value > 0 &&
      IsPowerOf2(fixed_right_arg_value) &&
      FixedRightArgValueField::is_valid(WhichPowerOf2(fixed_right_arg_value)) &&
      (left_kind_ == SMI || left_kind_ == INT32) &&
      (result_kind_ == NONE || !fixed_right_arg_.has_value);
  fixed_right_arg_ = Maybe<int32_t>(has_fixed_right_arg,
                                    fixed_right_arg_value);

  result_kind_ = UpdateKind(result, result_kind_);

  if (!Token::IsTruncatingBinaryOp(op_)) {
    Kind input_kind = Max(left_kind_, right_kind_);
    if (result_kind_ < input_kind && input_kind <= NUMBER) {
      result_kind_ = input_kind;
    }
  }

  // We don't want to distinguish INT32 and NUMBER for string add (because
  // NumberToString can't make use of this anyway).
  if (left_kind_ == STRING && right_kind_ == INT32) {
    ASSERT_EQ(STRING, result_kind_);
    ASSERT_EQ(Token::ADD, op_);
    right_kind_ = NUMBER;
  } else if (right_kind_ == STRING && left_kind_ == INT32) {
    ASSERT_EQ(STRING, result_kind_);
    ASSERT_EQ(Token::ADD, op_);
    left_kind_ = NUMBER;
  }

  // Reset overwrite mode unless we can actually make use of it, or may be able
  // to make use of it at some point in the future.
  if ((mode_ == OVERWRITE_LEFT && left_kind_ > NUMBER) ||
      (mode_ == OVERWRITE_RIGHT && right_kind_ > NUMBER) ||
      result_kind_ > NUMBER) {
    mode_ = NO_OVERWRITE;
  }

  if (old_extra_ic_state == GetExtraICState()) {
    // Tagged operations can lead to non-truncating HChanges
    if (left->IsUndefined() || left->IsBoolean()) {
      left_kind_ = GENERIC;
    } else if (right->IsUndefined() || right->IsBoolean()) {
      right_kind_ = GENERIC;
    } else {
      // Since the X87 is too precise, we might bail out on numbers which
      // actually would truncate with 64 bit precision.
      ASSERT(!CpuFeatures::IsSupported(SSE2));
      ASSERT(result_kind_ < NUMBER);
      result_kind_ = NUMBER;
    }
  }
}


BinaryOpIC::State::Kind BinaryOpIC::State::UpdateKind(Handle<Object> object,
                                                      Kind kind) const {
  Kind new_kind = GENERIC;
  bool is_truncating = Token::IsTruncatingBinaryOp(op());
  if (object->IsBoolean() && is_truncating) {
    // Booleans will be automatically truncated by HChange.
    new_kind = INT32;
  } else if (object->IsUndefined()) {
    // Undefined will be automatically truncated by HChange.
    new_kind = is_truncating ? INT32 : NUMBER;
  } else if (object->IsSmi()) {
    new_kind = SMI;
  } else if (object->IsHeapNumber()) {
    double value = Handle<HeapNumber>::cast(object)->value();
    new_kind = IsInt32Double(value) ? INT32 : NUMBER;
  } else if (object->IsString() && op() == Token::ADD) {
    new_kind = STRING;
  }
  if (new_kind == INT32 && SmiValuesAre32Bits()) {
    new_kind = NUMBER;
  }
  if (kind != NONE &&
      ((new_kind <= NUMBER && kind > NUMBER) ||
       (new_kind > NUMBER && kind <= NUMBER))) {
    new_kind = GENERIC;
  }
  return Max(kind, new_kind);
}


// static
const char* BinaryOpIC::State::KindToString(Kind kind) {
  switch (kind) {
    case NONE: return "None";
    case SMI: return "Smi";
    case INT32: return "Int32";
    case NUMBER: return "Number";
    case STRING: return "String";
    case GENERIC: return "Generic";
  }
  UNREACHABLE();
  return NULL;
}


// static
Type* BinaryOpIC::State::KindToType(Kind kind, Zone* zone) {
  switch (kind) {
    case NONE: return Type::None(zone);
    case SMI: return Type::Smi(zone);
    case INT32: return Type::Signed32(zone);
    case NUMBER: return Type::Number(zone);
    case STRING: return Type::String(zone);
    case GENERIC: return Type::Any(zone);
  }
  UNREACHABLE();
  return NULL;
}


MaybeObject* BinaryOpIC::Transition(Handle<AllocationSite> allocation_site,
                                    Handle<Object> left,
                                    Handle<Object> right) {
  State state(target()->extra_ic_state());

  // Compute the actual result using the builtin for the binary operation.
  Object* builtin = isolate()->js_builtins_object()->javascript_builtin(
      TokenToJSBuiltin(state.op()));
  Handle<JSFunction> function = handle(JSFunction::cast(builtin), isolate());
  bool caught_exception;
  Handle<Object> result = Execution::Call(
      isolate(), function, left, 1, &right, &caught_exception);
  if (caught_exception) return Failure::Exception();

  // Compute the new state.
  State old_state = state;
  state.Update(left, right, result);

  // Check if we have a string operation here.
  Handle<Code> target;
  if (!allocation_site.is_null() || state.ShouldCreateAllocationMementos()) {
    // Setup the allocation site on-demand.
    if (allocation_site.is_null()) {
      allocation_site = isolate()->factory()->NewAllocationSite();
    }

    // Install the stub with an allocation site.
    BinaryOpICWithAllocationSiteStub stub(state);
    target = stub.GetCodeCopyFromTemplate(isolate(), allocation_site);

    // Sanity check the trampoline stub.
    ASSERT_EQ(*allocation_site, target->FindFirstAllocationSite());
  } else {
    // Install the generic stub.
    BinaryOpICStub stub(state);
    target = stub.GetCode(isolate());

    // Sanity check the generic stub.
    ASSERT_EQ(NULL, target->FindFirstAllocationSite());
  }
  set_target(*target);

  if (FLAG_trace_ic) {
    char buffer[150];
    NoAllocationStringAllocator allocator(
        buffer, static_cast<unsigned>(sizeof(buffer)));
    StringStream stream(&allocator);
    stream.Add("[BinaryOpIC");
    old_state.Print(&stream);
    stream.Add(" => ");
    state.Print(&stream);
    stream.Add(" @ %p <- ", static_cast<void*>(*target));
    stream.OutputToStdOut();
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    if (!allocation_site.is_null()) {
      PrintF(" using allocation site %p", static_cast<void*>(*allocation_site));
    }
    PrintF("]\n");
  }

  // Patch the inlined smi code as necessary.
  if (!old_state.UseInlinedSmiCode() && state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  } else if (old_state.UseInlinedSmiCode() && !state.UseInlinedSmiCode()) {
    PatchInlinedSmiCode(address(), DISABLE_INLINED_SMI_CHECK);
  }

  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, BinaryOpIC_Miss) {
  HandleScope scope(isolate);
  ASSERT_EQ(2, args.length());
  Handle<Object> left = args.at<Object>(BinaryOpICStub::kLeft);
  Handle<Object> right = args.at<Object>(BinaryOpICStub::kRight);
  BinaryOpIC ic(isolate);
  return ic.Transition(Handle<AllocationSite>::null(), left, right);
}


RUNTIME_FUNCTION(MaybeObject*, BinaryOpIC_MissWithAllocationSite) {
  HandleScope scope(isolate);
  ASSERT_EQ(3, args.length());
  Handle<AllocationSite> allocation_site = args.at<AllocationSite>(
      BinaryOpWithAllocationSiteStub::kAllocationSite);
  Handle<Object> left = args.at<Object>(
      BinaryOpWithAllocationSiteStub::kLeft);
  Handle<Object> right = args.at<Object>(
      BinaryOpWithAllocationSiteStub::kRight);
  BinaryOpIC ic(isolate);
  return ic.Transition(allocation_site, left, right);
}


Code* CompareIC::GetRawUninitialized(Isolate* isolate, Token::Value op) {
  ICCompareStub stub(op, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code, isolate));
  return code;
}


Handle<Code> CompareIC::GetUninitialized(Isolate* isolate, Token::Value op) {
  ICCompareStub stub(op, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
  return stub.GetCode(isolate);
}


const char* CompareIC::GetStateName(State state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case SMI: return "SMI";
    case NUMBER: return "NUMBER";
    case INTERNALIZED_STRING: return "INTERNALIZED_STRING";
    case STRING: return "STRING";
    case UNIQUE_NAME: return "UNIQUE_NAME";
    case OBJECT: return "OBJECT";
    case KNOWN_OBJECT: return "KNOWN_OBJECT";
    case GENERIC: return "GENERIC";
  }
  UNREACHABLE();
  return NULL;
}


Type* CompareIC::StateToType(
    Zone* zone,
    CompareIC::State state,
    Handle<Map> map) {
  switch (state) {
    case CompareIC::UNINITIALIZED: return Type::None(zone);
    case CompareIC::SMI: return Type::Smi(zone);
    case CompareIC::NUMBER: return Type::Number(zone);
    case CompareIC::STRING: return Type::String(zone);
    case CompareIC::INTERNALIZED_STRING: return Type::InternalizedString(zone);
    case CompareIC::UNIQUE_NAME: return Type::UniqueName(zone);
    case CompareIC::OBJECT: return Type::Receiver(zone);
    case CompareIC::KNOWN_OBJECT:
      return map.is_null() ? Type::Receiver(zone) : Type::Class(map, zone);
    case CompareIC::GENERIC: return Type::Any(zone);
  }
  UNREACHABLE();
  return NULL;
}


void CompareIC::StubInfoToType(int stub_minor_key,
                               Type** left_type,
                               Type** right_type,
                               Type** overall_type,
                               Handle<Map> map,
                               Zone* zone) {
  State left_state, right_state, handler_state;
  ICCompareStub::DecodeMinorKey(stub_minor_key, &left_state, &right_state,
                                &handler_state, NULL);
  *left_type = StateToType(zone, left_state);
  *right_type = StateToType(zone, right_state);
  *overall_type = StateToType(zone, handler_state, map);
}


CompareIC::State CompareIC::NewInputState(State old_state,
                                          Handle<Object> value) {
  switch (old_state) {
    case UNINITIALIZED:
      if (value->IsSmi()) return SMI;
      if (value->IsHeapNumber()) return NUMBER;
      if (value->IsInternalizedString()) return INTERNALIZED_STRING;
      if (value->IsString()) return STRING;
      if (value->IsSymbol()) return UNIQUE_NAME;
      if (value->IsJSObject()) return OBJECT;
      break;
    case SMI:
      if (value->IsSmi()) return SMI;
      if (value->IsHeapNumber()) return NUMBER;
      break;
    case NUMBER:
      if (value->IsNumber()) return NUMBER;
      break;
    case INTERNALIZED_STRING:
      if (value->IsInternalizedString()) return INTERNALIZED_STRING;
      if (value->IsString()) return STRING;
      if (value->IsSymbol()) return UNIQUE_NAME;
      break;
    case STRING:
      if (value->IsString()) return STRING;
      break;
    case UNIQUE_NAME:
      if (value->IsUniqueName()) return UNIQUE_NAME;
      break;
    case OBJECT:
      if (value->IsJSObject()) return OBJECT;
      break;
    case GENERIC:
      break;
    case KNOWN_OBJECT:
      UNREACHABLE();
      break;
  }
  return GENERIC;
}


CompareIC::State CompareIC::TargetState(State old_state,
                                        State old_left,
                                        State old_right,
                                        bool has_inlined_smi_code,
                                        Handle<Object> x,
                                        Handle<Object> y) {
  switch (old_state) {
    case UNINITIALIZED:
      if (x->IsSmi() && y->IsSmi()) return SMI;
      if (x->IsNumber() && y->IsNumber()) return NUMBER;
      if (Token::IsOrderedRelationalCompareOp(op_)) {
        // Ordered comparisons treat undefined as NaN, so the
        // NUMBER stub will do the right thing.
        if ((x->IsNumber() && y->IsUndefined()) ||
            (y->IsNumber() && x->IsUndefined())) {
          return NUMBER;
        }
      }
      if (x->IsInternalizedString() && y->IsInternalizedString()) {
        // We compare internalized strings as plain ones if we need to determine
        // the order in a non-equality compare.
        return Token::IsEqualityOp(op_) ? INTERNALIZED_STRING : STRING;
      }
      if (x->IsString() && y->IsString()) return STRING;
      if (!Token::IsEqualityOp(op_)) return GENERIC;
      if (x->IsUniqueName() && y->IsUniqueName()) return UNIQUE_NAME;
      if (x->IsJSObject() && y->IsJSObject()) {
        if (Handle<JSObject>::cast(x)->map() ==
            Handle<JSObject>::cast(y)->map()) {
          return KNOWN_OBJECT;
        } else {
          return OBJECT;
        }
      }
      return GENERIC;
    case SMI:
      return x->IsNumber() && y->IsNumber() ? NUMBER : GENERIC;
    case INTERNALIZED_STRING:
      ASSERT(Token::IsEqualityOp(op_));
      if (x->IsString() && y->IsString()) return STRING;
      if (x->IsUniqueName() && y->IsUniqueName()) return UNIQUE_NAME;
      return GENERIC;
    case NUMBER:
      // If the failure was due to one side changing from smi to heap number,
      // then keep the state (if other changed at the same time, we will get
      // a second miss and then go to generic).
      if (old_left == SMI && x->IsHeapNumber()) return NUMBER;
      if (old_right == SMI && y->IsHeapNumber()) return NUMBER;
      return GENERIC;
    case KNOWN_OBJECT:
      ASSERT(Token::IsEqualityOp(op_));
      if (x->IsJSObject() && y->IsJSObject()) return OBJECT;
      return GENERIC;
    case STRING:
    case UNIQUE_NAME:
    case OBJECT:
    case GENERIC:
      return GENERIC;
  }
  UNREACHABLE();
  return GENERIC;  // Make the compiler happy.
}


Code* CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope(isolate());
  State previous_left, previous_right, previous_state;
  ICCompareStub::DecodeMinorKey(target()->stub_info(), &previous_left,
                                &previous_right, &previous_state, NULL);
  State new_left = NewInputState(previous_left, x);
  State new_right = NewInputState(previous_right, y);
  State state = TargetState(previous_state, previous_left, previous_right,
                            HasInlinedSmiCode(address()), x, y);
  ICCompareStub stub(op_, new_left, new_right, state);
  if (state == KNOWN_OBJECT) {
    stub.set_known_map(
        Handle<Map>(Handle<JSObject>::cast(x)->map(), isolate()));
  }
  Handle<Code> new_target = stub.GetCode(isolate());
  set_target(*new_target);

  if (FLAG_trace_ic) {
    PrintF("[CompareIC in ");
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    PrintF(" ((%s+%s=%s)->(%s+%s=%s))#%s @ %p]\n",
           GetStateName(previous_left),
           GetStateName(previous_right),
           GetStateName(previous_state),
           GetStateName(new_left),
           GetStateName(new_right),
           GetStateName(state),
           Token::Name(op_),
           static_cast<void*>(*stub.GetCode(isolate())));
  }

  // Activate inlined smi code.
  if (previous_state == UNINITIALIZED) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  }

  return *new_target;
}


// Used from ICCompareStub::GenerateMiss in code-stubs-<arch>.cc.
RUNTIME_FUNCTION(Code*, CompareIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  return ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
}


void CompareNilIC::Clear(Address address, Code* target) {
  if (IsCleared(target)) return;
  ExtraICState state = target->extra_ic_state();

  CompareNilICStub stub(state, HydrogenCodeStub::UNINITIALIZED);
  stub.ClearState();

  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code, target->GetIsolate()));

  SetTargetAtAddress(address, code);
}


MaybeObject* CompareNilIC::DoCompareNilSlow(NilValue nil,
                                            Handle<Object> object) {
  if (object->IsNull() || object->IsUndefined()) {
    return Smi::FromInt(true);
  }
  return Smi::FromInt(object->IsUndetectableObject());
}


MaybeObject* CompareNilIC::CompareNil(Handle<Object> object) {
  ExtraICState extra_ic_state = target()->extra_ic_state();

  CompareNilICStub stub(extra_ic_state);

  // Extract the current supported types from the patched IC and calculate what
  // types must be supported as a result of the miss.
  bool already_monomorphic = stub.IsMonomorphic();

  stub.UpdateStatus(object);

  NilValue nil = stub.GetNilValue();

  // Find or create the specialized stub to support the new set of types.
  Handle<Code> code;
  if (stub.IsMonomorphic()) {
    Handle<Map> monomorphic_map(already_monomorphic && (first_map() != NULL)
                                ? first_map()
                                : HeapObject::cast(*object)->map());
    code = isolate()->stub_cache()->ComputeCompareNil(monomorphic_map, stub);
  } else {
    code = stub.GetCode(isolate());
  }
  set_target(*code);
  return DoCompareNilSlow(nil, object);
}


RUNTIME_FUNCTION(MaybeObject*, CompareNilIC_Miss) {
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  CompareNilIC ic(isolate);
  return ic.CompareNil(object);
}


RUNTIME_FUNCTION(MaybeObject*, Unreachable) {
  UNREACHABLE();
  CHECK(false);
  return isolate->heap()->undefined_value();
}


Builtins::JavaScript BinaryOpIC::TokenToJSBuiltin(Token::Value op) {
  switch (op) {
    default:
      UNREACHABLE();
    case Token::ADD:
      return Builtins::ADD;
      break;
    case Token::SUB:
      return Builtins::SUB;
      break;
    case Token::MUL:
      return Builtins::MUL;
      break;
    case Token::DIV:
      return Builtins::DIV;
      break;
    case Token::MOD:
      return Builtins::MOD;
      break;
    case Token::BIT_OR:
      return Builtins::BIT_OR;
      break;
    case Token::BIT_AND:
      return Builtins::BIT_AND;
      break;
    case Token::BIT_XOR:
      return Builtins::BIT_XOR;
      break;
    case Token::SAR:
      return Builtins::SAR;
      break;
    case Token::SHR:
      return Builtins::SHR;
      break;
    case Token::SHL:
      return Builtins::SHL;
      break;
  }
}


MaybeObject* ToBooleanIC::ToBoolean(Handle<Object> object) {
  ToBooleanStub stub(target()->extra_ic_state());
  bool to_boolean_value = stub.UpdateStatus(object);
  Handle<Code> code = stub.GetCode(isolate());
  set_target(*code);
  return Smi::FromInt(to_boolean_value ? 1 : 0);
}


RUNTIME_FUNCTION(MaybeObject*, ToBooleanIC_Miss) {
  ASSERT(args.length() == 1);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  ToBooleanIC ic(isolate);
  return ic.ToBoolean(object);
}


static const Address IC_utilities[] = {
#define ADDR(name) FUNCTION_ADDR(name),
    IC_UTIL_LIST(ADDR)
    NULL
#undef ADDR
};


Address IC::AddressFromUtilityId(IC::UtilityId id) {
  return IC_utilities[id];
}


} }  // namespace v8::internal
