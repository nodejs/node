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
    Code::ExtraICState extra_state = new_target->extra_ic_state();
    const char* modifier =
        GetTransitionMarkModifier(Code::GetKeyedAccessStoreMode(extra_state));
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


bool CallIC::TryUpdateExtraICState(LookupResult* lookup,
                                   Handle<Object> object) {
  if (!lookup->IsConstantFunction()) return false;
  JSFunction* function = lookup->GetConstantFunction();
  if (!function->shared()->HasBuiltinFunctionId()) return false;

  // Fetch the arguments passed to the called function.
  const int argc = target()->arguments_count();
  Address entry = isolate()->c_entry_fp(isolate()->thread_local_top());
  Address fp = Memory::Address_at(entry + ExitFrameConstants::kCallerFPOffset);
  Arguments args(argc + 1,
                 &Memory::Object_at(fp +
                                    StandardFrameConstants::kCallerSPOffset +
                                    argc * kPointerSize));
  switch (function->shared()->builtin_function_id()) {
    case kStringCharCodeAt:
    case kStringCharAt:
      if (object->IsString()) {
        String* string = String::cast(*object);
        // Check there's the right string value or wrapper in the receiver slot.
        ASSERT(string == args[0] || string == JSValue::cast(args[0])->value());
        // If we're in the default (fastest) state and the index is
        // out of bounds, update the state to record this fact.
        if (StringStubState::decode(extra_ic_state()) == DEFAULT_STRING_STUB &&
            argc >= 1 && args[1]->IsNumber()) {
          double index = DoubleToInteger(args.number_at(1));
          if (index < 0 || index >= string->length()) {
            extra_ic_state_ =
                StringStubState::update(extra_ic_state(),
                                        STRING_INDEX_OUT_OF_BOUNDS);
            return true;
          }
        }
      }
      break;
    default:
      return false;
  }
  return false;
}


bool IC::TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                                Handle<String> name) {
  DisallowHeapAllocation no_gc;

  if (target()->is_call_stub()) {
    LookupResult lookup(isolate());
    LookupForRead(receiver, name, &lookup);
    if (static_cast<CallIC*>(this)->TryUpdateExtraICState(&lookup, receiver)) {
      return true;
    }
  }

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
    Map* old_map = target()->FindFirstMap();
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
  CodeHandleList handlers;
  target()->FindHandlers(&handlers);
  for (int i = 0; i < handlers.length(); i++) {
    Handle<Code> handler = handlers.at(i);
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


RelocInfo::Mode IC::ComputeMode() {
  Address addr = address();
  Code* code = Code::cast(isolate()->FindCodeObject(addr));
  for (RelocIterator it(code, RelocInfo::kCodeTargetMask);
       !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    if (info->pc() == addr) return info->rmode();
  }
  UNREACHABLE();
  return RelocInfo::NONE32;
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
  if (FLAG_type_info_threshold == 0 && !FLAG_watch_ic_patching) {
    return;
  }
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
  if (FLAG_watch_ic_patching) {
    host->set_profiler_ticks(0);
    isolate->runtime_profiler()->NotifyICChanged();
  }
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
    case Code::CALL_IC: return CallIC::Clear(address, target);
    case Code::KEYED_CALL_IC:  return KeyedCallIC::Clear(address, target);
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


void CallICBase::Clear(Address address, Code* target) {
  if (IsCleared(target)) return;
  bool contextual = CallICBase::Contextual::decode(target->extra_ic_state());
  Code* code =
      target->GetIsolate()->stub_cache()->FindCallInitialize(
          target->arguments_count(),
          contextual ? RelocInfo::CODE_TARGET_CONTEXT : RelocInfo::CODE_TARGET,
          target->kind());
  SetTargetAtAddress(address, code);
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
  SetTargetAtAddress(address, *pre_monomorphic_stub(isolate));
}


void StoreIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  SetTargetAtAddress(address,
      *pre_monomorphic_stub(
          isolate, Code::GetStrictMode(target->extra_ic_state())));
}


void KeyedStoreIC::Clear(Isolate* isolate, Address address, Code* target) {
  if (IsCleared(target)) return;
  SetTargetAtAddress(address,
      *pre_monomorphic_stub(
          isolate, Code::GetStrictMode(target->extra_ic_state())));
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


Handle<Object> CallICBase::TryCallAsFunction(Handle<Object> object) {
  Handle<Object> delegate = Execution::GetFunctionDelegate(isolate(), object);

  if (delegate->IsJSFunction() && !object->IsJSFunctionProxy()) {
    // Patch the receiver and use the delegate as the function to
    // invoke. This is used for invoking objects as if they were functions.
    const int argc = target()->arguments_count();
    StackFrameLocator locator(isolate());
    JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
    int index = frame->ComputeExpressionsCount() - (argc + 1);
    frame->SetExpression(index, *object);
  }

  return delegate;
}


void CallICBase::ReceiverToObjectIfRequired(Handle<Object> callee,
                                            Handle<Object> object) {
  while (callee->IsJSFunctionProxy()) {
    callee = Handle<Object>(JSFunctionProxy::cast(*callee)->call_trap(),
                            isolate());
  }

  if (callee->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callee);
    if (!function->shared()->is_classic_mode() || function->IsBuiltin()) {
      // Do not wrap receiver for strict mode functions or for builtins.
      return;
    }
  }

  // And only wrap string, number or boolean.
  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    // Change the receiver to the result of calling ToObject on it.
    const int argc = this->target()->arguments_count();
    StackFrameLocator locator(isolate());
    JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
    int index = frame->ComputeExpressionsCount() - (argc + 1);
    frame->SetExpression(index, *isolate()->factory()->ToObject(object));
  }
}


static bool MigrateDeprecated(Handle<Object> object) {
  if (!object->IsJSObject()) return false;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  if (!receiver->map()->is_deprecated()) return false;
  JSObject::MigrateInstance(Handle<JSObject>::cast(object));
  return true;
}


MaybeObject* CallICBase::LoadFunction(Handle<Object> object,
                                      Handle<String> name) {
  bool use_ic = MigrateDeprecated(object) ? false : FLAG_use_ic;

  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, name);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<Object> result = Object::GetElement(isolate(), object, index);
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    if (result->IsJSFunction()) return *result;

    // Try to find a suitable function delegate for the object at hand.
    result = TryCallAsFunction(result);
    if (result->IsJSFunction()) return *result;

    // Otherwise, it will fail in the lookup step.
  }

  // Lookup the property in the object.
  LookupResult lookup(isolate());
  LookupForRead(object, name, &lookup);

  if (!lookup.IsFound()) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    return IsUndeclaredGlobal(object)
        ? ReferenceError("not_defined", name)
        : TypeError("undefined_method", object, name);
  }

  // Lookup is valid: Update inline cache and stub cache.
  if (use_ic) UpdateCaches(&lookup, object, name);

  // Get the property.
  PropertyAttributes attr;
  Handle<Object> result =
      Object::GetProperty(object, object, &lookup, name, &attr);
  RETURN_IF_EMPTY_HANDLE(isolate(), result);

  if (lookup.IsInterceptor() && attr == ABSENT) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    return IsUndeclaredGlobal(object)
        ? ReferenceError("not_defined", name)
        : TypeError("undefined_method", object, name);
  }

  ASSERT(!result->IsTheHole());

  // Make receiver an object if the callee requires it. Strict mode or builtin
  // functions do not wrap the receiver, non-strict functions and objects
  // called as functions do.
  ReceiverToObjectIfRequired(result, object);

  if (result->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(result);
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Handle stepping into a function if step into is active.
    Debug* debug = isolate()->debug();
    if (debug->StepInActive()) {
      // Protect the result in a handle as the debugger can allocate and might
      // cause GC.
      debug->HandleStepIn(function, object, fp(), false);
    }
#endif
    return *function;
  }

  // Try to find a suitable function delegate for the object at hand.
  result = TryCallAsFunction(result);
  if (result->IsJSFunction()) return *result;

  return TypeError("property_not_function", object, name);
}


Handle<Code> CallICBase::ComputeMonomorphicStub(LookupResult* lookup,
                                                Handle<Object> object,
                                                Handle<String> name) {
  int argc = target()->arguments_count();
  Handle<JSObject> holder(lookup->holder(), isolate());
  switch (lookup->type()) {
    case FIELD: {
      PropertyIndex index = lookup->GetFieldIndex();
      return isolate()->stub_cache()->ComputeCallField(
          argc, kind_, extra_ic_state(), name, object, holder, index);
    }
    case CONSTANT: {
      if (!lookup->IsConstantFunction()) return Handle<Code>::null();
      // Get the constant function and compute the code stub for this
      // call; used for rewriting to monomorphic state and making sure
      // that the code stub is in the stub cache.
      Handle<JSFunction> function(lookup->GetConstantFunction(), isolate());
      return isolate()->stub_cache()->ComputeCallConstant(
          argc, kind_, extra_ic_state(), name, object, holder, function);
    }
    case NORMAL: {
      // If we return a null handle, the IC will not be patched.
      if (!object->IsJSObject()) return Handle<Code>::null();
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);

      if (holder->IsGlobalObject()) {
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(holder);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        if (!cell->value()->IsJSFunction()) return Handle<Code>::null();
        Handle<JSFunction> function(JSFunction::cast(cell->value()));
        return isolate()->stub_cache()->ComputeCallGlobal(
            argc, kind_, extra_ic_state(), name,
            receiver, global, cell, function);
      } else {
        // There is only one shared stub for calling normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the receiver for the stub to be
        // applicable.
        if (!holder.is_identical_to(receiver)) return Handle<Code>::null();
        return isolate()->stub_cache()->ComputeCallNormal(
            argc, kind_, extra_ic_state());
      }
      break;
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(*holder));
      return isolate()->stub_cache()->ComputeCallInterceptor(
          argc, kind_, extra_ic_state(), name, object, holder);
    default:
      return Handle<Code>::null();
  }
}


Handle<Code> CallICBase::megamorphic_stub() {
  return isolate()->stub_cache()->ComputeCallMegamorphic(
      target()->arguments_count(), kind_, extra_ic_state());
}


Handle<Code> CallICBase::pre_monomorphic_stub() {
  return isolate()->stub_cache()->ComputeCallPreMonomorphic(
      target()->arguments_count(), kind_, extra_ic_state());
}


void CallICBase::UpdateCaches(LookupResult* lookup,
                              Handle<Object> object,
                              Handle<String> name) {
  // Bail out if we didn't find a result.
  if (!lookup->IsProperty() || !lookup->IsCacheable()) return;

  // Compute the number of arguments.
  Handle<Code> code;
  code = state() == UNINITIALIZED
      ? pre_monomorphic_stub()
      : ComputeMonomorphicStub(lookup, object, name);

  // If there's no appropriate stub we simply avoid updating the caches.
  // TODO(verwaest): Install a slow fallback in this case to avoid not learning,
  // and deopting Crankshaft code.
  if (code.is_null()) return;

  Handle<JSObject> cache_object = object->IsJSObject()
      ? Handle<JSObject>::cast(object)
      : Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate())),
                         isolate());

  PatchCache(cache_object, name, code);
  TRACE_IC("CallIC", name);
}


MaybeObject* KeyedCallIC::LoadFunction(Handle<Object> object,
                                       Handle<Object> key) {
  if (key->IsInternalizedString()) {
    return CallICBase::LoadFunction(object, Handle<String>::cast(key));
  }

  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, key);
  }

  bool use_ic = MigrateDeprecated(object)
      ? false : FLAG_use_ic && !object->IsAccessCheckNeeded();

  if (use_ic && state() != MEGAMORPHIC) {
    ASSERT(!object->IsJSGlobalProxy());
    int argc = target()->arguments_count();
    Handle<Code> stub = isolate()->stub_cache()->ComputeCallMegamorphic(
        argc, Code::KEYED_CALL_IC, Code::kNoExtraICState);
    if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      if (receiver->elements()->map() ==
          isolate()->heap()->non_strict_arguments_elements_map()) {
        stub = isolate()->stub_cache()->ComputeCallArguments(argc);
      }
    }
    ASSERT(!stub.is_null());
    set_target(*stub);
    TRACE_IC("CallIC", key);
  }

  Handle<Object> result = GetProperty(isolate(), object, key);
  RETURN_IF_EMPTY_HANDLE(isolate(), result);

  // Make receiver an object if the callee requires it. Strict mode or builtin
  // functions do not wrap the receiver, non-strict functions and objects
  // called as functions do.
  ReceiverToObjectIfRequired(result, object);
  if (result->IsJSFunction()) return *result;

  result = TryCallAsFunction(result);
  if (result->IsJSFunction()) return *result;

  return TypeError("property_not_function", object, key);
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
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#length /stringwrapper]\n");
#endif
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
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#prototype /function]\n");
#endif
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


bool IC::UpdatePolymorphicIC(Handle<HeapObject> receiver,
                             Handle<String> name,
                             Handle<Code> code) {
  if (!code->is_handler()) return false;

  MapHandleList receiver_maps;
  CodeHandleList handlers;

  int number_of_valid_maps;
  int handler_to_overwrite = -1;
  Handle<Map> new_receiver_map(receiver->map());
  {
    DisallowHeapAllocation no_gc;
    target()->FindAllMaps(&receiver_maps);
    int number_of_maps = receiver_maps.length();
    number_of_valid_maps = number_of_maps;

    for (int i = 0; i < number_of_maps; i++) {
      Handle<Map> map = receiver_maps.at(i);
      // Filter out deprecated maps to ensure its instances get migrated.
      if (map->is_deprecated()) {
        number_of_valid_maps--;
      // If the receiver map is already in the polymorphic IC, this indicates
      // there was a prototoype chain failure. In that case, just overwrite the
      // handler.
      } else if (map.is_identical_to(new_receiver_map)) {
        number_of_valid_maps--;
        handler_to_overwrite = i;
      }
    }

    if (number_of_valid_maps >= 4) return false;
    if (number_of_maps == 0) return false;

    if (!target()->FindHandlers(&handlers, receiver_maps.length())) {
      return false;
    }
  }

  number_of_valid_maps++;
  if (handler_to_overwrite >= 0) {
    handlers.Set(handler_to_overwrite, code);
  } else {
    receiver_maps.Add(new_receiver_map);
    handlers.Add(code);
  }

  Handle<Code> ic = isolate()->stub_cache()->ComputePolymorphicIC(
      &receiver_maps, &handlers, number_of_valid_maps, name, strict_mode());
  set_target(*ic);
  return true;
}


void IC::UpdateMonomorphicIC(Handle<HeapObject> receiver,
                             Handle<Code> handler,
                             Handle<String> name) {
  if (!handler->is_handler()) return set_target(*handler);
  Handle<Code> ic = isolate()->stub_cache()->ComputeMonomorphicIC(
      receiver, handler, name, strict_mode());
  set_target(*ic);
}


void IC::CopyICToMegamorphicCache(Handle<String> name) {
  MapHandleList receiver_maps;
  CodeHandleList handlers;
  {
    DisallowHeapAllocation no_gc;
    target()->FindAllMaps(&receiver_maps);
    if (!target()->FindHandlers(&handlers, receiver_maps.length())) return;
  }
  for (int i = 0; i < receiver_maps.length(); i++) {
    UpdateMegamorphicCache(*receiver_maps.at(i), *name, *handlers.at(i));
  }
}


bool IC::IsTransitionedMapOfMonomorphicTarget(Map* receiver_map) {
  DisallowHeapAllocation no_allocation;

  Map* current_map = target()->FindFirstMap();
  ElementsKind receiver_elements_kind = receiver_map->elements_kind();
  bool more_general_transition =
      IsMoreGeneralElementsKindTransition(
        current_map->elements_kind(), receiver_elements_kind);
  Map* transitioned_map = more_general_transition
      ? current_map->LookupElementsTransitionMap(receiver_elements_kind)
      : NULL;

  return transitioned_map == receiver_map;
}


void IC::PatchCache(Handle<HeapObject> receiver,
                    Handle<String> name,
                    Handle<Code> code) {
  switch (state()) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
    case MONOMORPHIC_PROTOTYPE_FAILURE:
      UpdateMonomorphicIC(receiver, code, name);
      break;
    case MONOMORPHIC:
      // For now, call stubs are allowed to rewrite to the same stub. This
      // happens e.g., when the field does not contain a function.
      ASSERT(target()->is_call_stub() ||
             target()->is_keyed_call_stub() ||
             !target().is_identical_to(code));
      if (!target()->is_keyed_stub()) {
        bool is_same_handler = false;
        {
          DisallowHeapAllocation no_allocation;
          Code* old_handler = target()->FindFirstHandler();
          is_same_handler = old_handler == *code;
        }
        if (is_same_handler
            && IsTransitionedMapOfMonomorphicTarget(receiver->map())) {
          UpdateMonomorphicIC(receiver, code, name);
          break;
        }
        if (UpdatePolymorphicIC(receiver, name, code)) {
          break;
        }

        CopyICToMegamorphicCache(name);
      }

      UpdateMegamorphicCache(receiver->map(), *name, *code);
      set_target(*megamorphic_stub());
      break;
    case MEGAMORPHIC:
      UpdateMegamorphicCache(receiver->map(), *name, *code);
      break;
    case POLYMORPHIC:
      if (target()->is_keyed_stub()) {
        // When trying to patch a polymorphic keyed stub with anything other
        // than another polymorphic stub, go generic.
        set_target(*generic_stub());
      } else {
        if (UpdatePolymorphicIC(receiver, name, code)) {
          break;
        }
        CopyICToMegamorphicCache(name);
        UpdateMegamorphicCache(receiver->map(), *name, *code);
        set_target(*megamorphic_stub());
      }
      break;
    case DEBUG_STUB:
      break;
    case GENERIC:
      UNREACHABLE();
      break;
  }
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
  // TODO(verwaest): It would be nice to support loading fields from smis as
  // well. For now just fail to update the cache.
  if (!object->IsHeapObject()) return;

  Handle<HeapObject> receiver = Handle<HeapObject>::cast(object);

  Handle<Code> code;
  if (state() == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = pre_monomorphic_stub();
  } else if (!lookup->IsCacheable()) {
    // Bail out if the result is not cacheable.
    code = slow_stub();
  } else if (object->IsString() &&
             name->Equals(isolate()->heap()->length_string())) {
    int length_index = String::kLengthOffset / kPointerSize;
    code = SimpleFieldLoad(length_index);
  } else if (!object->IsJSObject()) {
    // TODO(jkummerow): It would be nice to support non-JSObjects in
    // ComputeLoadHandler, then we wouldn't need to go generic here.
    code = slow_stub();
  } else if (!lookup->IsProperty()) {
    code = kind() == Code::LOAD_IC
        ? isolate()->stub_cache()->ComputeLoadNonexistent(
              name, Handle<JSObject>::cast(receiver))
        : slow_stub();
  } else {
    code = ComputeHandler(lookup, Handle<JSObject>::cast(receiver), name);
  }

  PatchCache(receiver, name, code);
  TRACE_IC("LoadIC", name);
}


void IC::UpdateMegamorphicCache(Map* map, Name* name, Code* code) {
  // Cache code holding map should be consistent with
  // GenerateMonomorphicCacheProbe.
  isolate()->stub_cache()->Set(name, map, code);
}


Handle<Code> IC::ComputeHandler(LookupResult* lookup,
                                Handle<JSObject> receiver,
                                Handle<String> name,
                                Handle<Object> value) {
  Handle<Code> code = isolate()->stub_cache()->FindHandler(
      name, receiver, kind());
  if (!code.is_null()) return code;

  code = CompileHandler(lookup, receiver, name, value);

  if (code->is_handler() && code->type() != Code::NORMAL) {
    HeapObject::UpdateMapCodeCache(receiver, name, code);
  }

  return code;
}


Handle<Code> LoadIC::CompileHandler(LookupResult* lookup,
                                    Handle<JSObject> receiver,
                                    Handle<String> name,
                                    Handle<Object> unused) {
  Handle<JSObject> holder(lookup->holder());
  LoadStubCompiler compiler(isolate(), kind());

  switch (lookup->type()) {
    case FIELD: {
      PropertyIndex field = lookup->GetFieldIndex();
      if (receiver.is_identical_to(holder)) {
        return SimpleFieldLoad(field.translate(holder),
                               field.is_inobject(holder),
                               lookup->representation());
      }
      return compiler.CompileLoadField(
          receiver, holder, name, field, lookup->representation());
    }
    case CONSTANT: {
      Handle<Object> constant(lookup->GetConstant(), isolate());
      // TODO(2803): Don't compute a stub for cons strings because they cannot
      // be embedded into code.
      if (constant->IsConsString()) break;
      return compiler.CompileLoadConstant(receiver, holder, name, constant);
    }
    case NORMAL:
      if (kind() != Code::LOAD_IC) break;
      if (holder->IsGlobalObject()) {
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(holder);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        // TODO(verwaest): Turn into a handler.
        return isolate()->stub_cache()->ComputeLoadGlobal(
            name, receiver, global, cell, lookup->IsDontDelete());
      }
      // There is only one shared stub for loading normalized
      // properties. It does not traverse the prototype chain, so the
      // property must be found in the receiver for the stub to be
      // applicable.
      if (!holder.is_identical_to(receiver)) break;
      return isolate()->builtins()->LoadIC_Normal();
    case CALLBACKS: {
      // Use simple field loads for some well-known callback properties.
      int object_offset;
      Handle<Map> map(receiver->map());
      if (Accessors::IsJSObjectFieldAccessor(map, name, &object_offset)) {
        PropertyIndex index =
            PropertyIndex::NewHeaderIndex(object_offset / kPointerSize);
        return compiler.CompileLoadField(
            receiver, receiver, name, index, Representation::Tagged());
      }

      Handle<Object> callback(lookup->GetCallbackObject(), isolate());
      if (callback->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(callback);
        if (v8::ToCData<Address>(info->getter()) == 0) break;
        if (!info->IsCompatibleReceiver(*receiver)) break;
        return compiler.CompileLoadCallback(receiver, holder, name, info);
      } else if (callback->IsAccessorPair()) {
        Handle<Object> getter(Handle<AccessorPair>::cast(callback)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (holder->IsGlobalObject()) break;
        if (!holder->HasFastProperties()) break;
        Handle<JSFunction> function = Handle<JSFunction>::cast(getter);
        CallOptimization call_optimization(function);
        if (call_optimization.is_simple_api_call() &&
            call_optimization.IsCompatibleReceiver(*receiver)) {
          return compiler.CompileLoadCallback(
              receiver, holder, name, call_optimization);
        }
        return compiler.CompileLoadViaGetter(receiver, holder, name, function);
      }
      // TODO(dcarney): Handle correctly.
      if (callback->IsDeclaredAccessorInfo()) break;
      ASSERT(callback->IsForeign());
      // No IC support for old-style native accessors.
      break;
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(*holder));
      return compiler.CompileLoadInterceptor(receiver, holder, name);
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
    target()->FindAllMaps(&target_receiver_maps);
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


MaybeObject* KeyedLoadIC::Load(Handle<Object> object,
                               Handle<Object> key,
                               ICMissMode miss_mode) {
  if (MigrateDeprecated(object)) {
    return Runtime::GetObjectPropertyOrFail(isolate(), object, key);
  }

  MaybeObject* maybe_object = NULL;
  Handle<Code> stub = generic_stub();

  // Check for values that can be converted into an internalized string directly
  // or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString()) {
    maybe_object = LoadIC::Load(object, Handle<String>::cast(key));
    if (maybe_object->IsFailure()) return maybe_object;
  } else if (FLAG_use_ic && !object->IsAccessCheckNeeded()) {
    ASSERT(!object->IsJSGlobalProxy());
    if (miss_mode != MISS_FORCE_GENERIC) {
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
    if (lookup->IsReadOnly() || !lookup->IsCacheable()) return false;

    if (lookup->holder() == *receiver) {
      if (lookup->IsInterceptor() && !HasInterceptorSetter(*receiver)) {
        receiver->LocalLookupRealNamedProperty(*name, lookup);
        return lookup->IsFound() &&
            !lookup->IsReadOnly() &&
            lookup->CanHoldValue(value) &&
            lookup->IsCacheable();
      }
      return lookup->CanHoldValue(value);
    }

    if (lookup->IsPropertyCallbacks()) return true;

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
  PropertyDetails target_details =
      lookup->GetTransitionDetails(receiver->map());
  if (target_details.IsReadOnly()) return false;

  // If the value that's being stored does not fit in the field that the
  // instance would transition to, create a new transition that fits the value.
  // This has to be done before generating the IC, since that IC will embed the
  // transition target.
  // Ensure the instance and its map were migrated before trying to update the
  // transition target.
  ASSERT(!receiver->map()->is_deprecated());
  if (!value->FitsRepresentation(target_details.representation())) {
    Handle<Map> target(lookup->GetTransitionMapFromMap(receiver->map()));
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

  if (receiver->IsJSGlobalProxy()) {
    if (FLAG_use_ic && kind() != Code::KEYED_STORE_IC) {
      // Generate a generic stub that goes to the runtime when we see a global
      // proxy as receiver.
      Handle<Code> stub = global_proxy_stub();
      set_target(*stub);
      TRACE_IC("StoreIC", name);
    }
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
      IsUndeclaredGlobal(object)) {
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


void StoreIC::UpdateCaches(LookupResult* lookup,
                           Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value) {
  ASSERT(!receiver->IsJSGlobalProxy());
  ASSERT(lookup->IsFound());

  // These are not cacheable, so we never see such LookupResults here.
  ASSERT(!lookup->IsHandler());

  Handle<Code> code = ComputeHandler(lookup, receiver, name, value);

  PatchCache(receiver, name, code);
  TRACE_IC("StoreIC", name);
}


Handle<Code> StoreIC::CompileHandler(LookupResult* lookup,
                                     Handle<JSObject> receiver,
                                     Handle<String> name,
                                     Handle<Object> value) {
  Handle<JSObject> holder(lookup->holder());
  StoreStubCompiler compiler(isolate(), strict_mode(), kind());
  switch (lookup->type()) {
    case FIELD:
      return compiler.CompileStoreField(receiver, lookup, name);
    case TRANSITION: {
      // Explicitly pass in the receiver map since LookupForWrite may have
      // stored something else than the receiver in the holder.
      Handle<Map> transition(
          lookup->GetTransitionTarget(receiver->map()), isolate());
      int descriptor = transition->LastAdded();

      DescriptorArray* target_descriptors = transition->instance_descriptors();
      PropertyDetails details = target_descriptors->GetDetails(descriptor);

      if (details.type() == CALLBACKS || details.attributes() != NONE) break;

      return compiler.CompileStoreTransition(
          receiver, lookup, transition, name);
    }
    case NORMAL:
      if (kind() == Code::KEYED_STORE_IC) break;
      if (receiver->IsGlobalObject()) {
        // The stub generated for the global object picks the value directly
        // from the property cell. So the property must be directly on the
        // global object.
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        // TODO(verwaest): Turn into a handler.
        return isolate()->stub_cache()->ComputeStoreGlobal(
            name, global, cell, value, strict_mode());
      }
      ASSERT(holder.is_identical_to(receiver));
      return strict_mode() == kStrictMode
          ? isolate()->builtins()->StoreIC_Normal_Strict()
          : isolate()->builtins()->StoreIC_Normal();
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
            call_optimization.IsCompatibleReceiver(*receiver)) {
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
      ASSERT(HasInterceptorSetter(*receiver));
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

  MapHandleList target_receiver_maps;
  target()->FindAllMaps(&target_receiver_maps);
  if (target_receiver_maps.length() == 0) {
    // In the case that there is a non-map-specific IC is installed (e.g. keyed
    // stores into properties in dictionary mode), then there will be not
    // receiver maps in the target.
    return generic_stub();
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different GetNonTransitioningStoreMode IC that handles a
  // superset of the original IC. Handle those here if the receiver map hasn't
  // changed or it has transitioned to a more general kind.
  KeyedAccessStoreMode old_store_mode =
      Code::GetKeyedAccessStoreMode(target()->extra_ic_state());
  Handle<Map> previous_receiver_map = target_receiver_maps.at(0);
  if (state() == MONOMORPHIC) {
      // If the "old" and "new" maps are in the same elements map family, stay
      // MONOMORPHIC and use the map for the most generic ElementsKind.
    Handle<Map> transitioned_receiver_map = receiver_map;
    if (IsTransitionStoreMode(store_mode)) {
      transitioned_receiver_map =
          ComputeTransitionedMap(receiver, store_mode);
    }
    if (IsTransitionedMapOfMonomorphicTarget(*transitioned_receiver_map)) {
      // Element family is the same, use the "worst" case map.
      store_mode = GetNonTransitioningStoreMode(store_mode);
      return isolate()->stub_cache()->ComputeKeyedStoreElement(
          transitioned_receiver_map, strict_mode(), store_mode);
    } else if (*previous_receiver_map == receiver->map() &&
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
      if (target_receiver_maps[i]->has_external_array_elements()) {
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
                                 Handle<Object> value,
                                 ICMissMode miss_mode) {
  if (MigrateDeprecated(object)) {
    return Runtime::SetObjectPropertyOrFail(
        isolate(), object , key, value, NONE, strict_mode());
  }

  // Check for values that can be converted into an internalized string directly
  // or is representable as a smi.
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
      ASSERT(!object->IsJSGlobalProxy());

      if (miss_mode != MISS_FORCE_GENERIC) {
        if (object->IsJSObject()) {
          Handle<JSObject> receiver = Handle<JSObject>::cast(object);
          bool key_is_smi_like = key->IsSmi() || !key->ToSmi()->IsFailure();
          if (receiver->elements()->map() ==
              isolate()->heap()->non_strict_arguments_elements_map()) {
            stub = non_strict_arguments_stub();
          } else if (key_is_smi_like &&
                     (!target().is_identical_to(non_strict_arguments_stub()))) {
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
  return Runtime::SetObjectPropertyOrFail(
      isolate(), object , key, value, NONE, strict_mode());
}


#undef TRACE_IC


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, CallIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CallIC ic(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<String> key = args.at<String>(1);
  ic.UpdateState(receiver, key);
  MaybeObject* maybe_result = ic.LoadFunction(receiver, key);
  JSFunction* raw_function;
  if (!maybe_result->To(&raw_function)) return maybe_result;

  // The first time the inline cache is updated may be the first time the
  // function it references gets called. If the function is lazily compiled
  // then the first call will trigger a compilation. We check for this case
  // and we do the compilation immediately, instead of waiting for the stub
  // currently attached to the JSFunction object to trigger compilation.
  if (raw_function->is_compiled()) return raw_function;

  Handle<JSFunction> function(raw_function);
  JSFunction::CompileLazy(function, CLEAR_EXCEPTION);
  return *function;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, KeyedCallIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedCallIC ic(isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  MaybeObject* maybe_result = ic.LoadFunction(receiver, key);
  // Result could be a function or a failure.
  JSFunction* raw_function = NULL;
  if (!maybe_result->To(&raw_function)) return maybe_result;

  if (raw_function->is_compiled()) return raw_function;

  Handle<JSFunction> function(raw_function, isolate);
  JSFunction::CompileLazy(function, CLEAR_EXCEPTION);
  return *function;
}


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
  return ic.Load(receiver, key, MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Load(receiver, key, MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissForceGeneric) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Load(receiver, key, MISS_FORCE_GENERIC);
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
  return ic.Store(receiver, key, args.at<Object>(2), MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2), MISS);
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_Slow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = ic.strict_mode();
  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    NONE,
                                    strict_mode);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Slow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = ic.strict_mode();
  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    NONE,
                                    strict_mode);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissForceGeneric) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Handle<Object> receiver = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  ic.UpdateState(receiver, key);
  return ic.Store(receiver, key, args.at<Object>(2), MISS_FORCE_GENERIC);
}


RUNTIME_FUNCTION(MaybeObject*, ElementsTransitionAndStoreIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  Handle<Object> value = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(2);
  Handle<Object> object = args.at<Object>(3);
  StrictModeFlag strict_mode = ic.strict_mode();
  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    NONE,
                                    strict_mode);
}


const char* BinaryOpIC::GetName(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED: return "Uninitialized";
    case SMI: return "Smi";
    case INT32: return "Int32";
    case NUMBER: return "Number";
    case ODDBALL: return "Oddball";
    case STRING: return "String";
    case GENERIC: return "Generic";
    default: return "Invalid";
  }
}


MaybeObject* BinaryOpIC::Transition(Handle<Object> left, Handle<Object> right) {
  Code::ExtraICState extra_ic_state = target()->extended_extra_ic_state();
  BinaryOpStub stub(extra_ic_state);

  Handle<Type> left_type = stub.GetLeftType(isolate());
  Handle<Type> right_type = stub.GetRightType(isolate());
  bool smi_was_enabled = left_type->Maybe(Type::Smi()) &&
                         right_type->Maybe(Type::Smi());

  Maybe<Handle<Object> > result = stub.Result(left, right, isolate());
  if (!result.has_value) return Failure::Exception();

#ifdef DEBUG
  if (FLAG_trace_ic) {
    char buffer[100];
    NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
    StringStream stream(&allocator);
    stream.Add("[");
    stub.PrintName(&stream);

    stub.UpdateStatus(left, right, result);

    stream.Add(" => ");
    stub.PrintState(&stream);
    stream.Add(" ");
    stream.OutputToStdOut();
    PrintF(" @ %p <- ", static_cast<void*>(*stub.GetCode(isolate())));
    JavaScriptFrame::PrintTop(isolate(), stdout, false, true);
    PrintF("]\n");
  } else {
    stub.UpdateStatus(left, right, result);
  }
#else
  stub.UpdateStatus(left, right, result);
#endif

  Handle<Code> code = stub.GetCode(isolate());
  set_target(*code);

  left_type = stub.GetLeftType(isolate());
  right_type = stub.GetRightType(isolate());
  bool enable_smi = left_type->Maybe(Type::Smi()) &&
                    right_type->Maybe(Type::Smi());

  if (!smi_was_enabled && enable_smi) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  } else if (smi_was_enabled && !enable_smi) {
    PatchInlinedSmiCode(address(), DISABLE_INLINED_SMI_CHECK);
  }

  ASSERT(result.has_value);
  return static_cast<MaybeObject*>(*result.value);
}


RUNTIME_FUNCTION(MaybeObject*, BinaryOpIC_Miss) {
  HandleScope scope(isolate);
  Handle<Object> left = args.at<Object>(0);
  Handle<Object> right = args.at<Object>(1);
  BinaryOpIC ic(isolate);
  return ic.Transition(left, right);
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


Handle<Type> CompareIC::StateToType(
    Isolate* isolate,
    CompareIC::State state,
    Handle<Map> map) {
  switch (state) {
    case CompareIC::UNINITIALIZED:
      return handle(Type::None(), isolate);
    case CompareIC::SMI:
      return handle(Type::Smi(), isolate);
    case CompareIC::NUMBER:
      return handle(Type::Number(), isolate);
    case CompareIC::STRING:
      return handle(Type::String(), isolate);
    case CompareIC::INTERNALIZED_STRING:
      return handle(Type::InternalizedString(), isolate);
    case CompareIC::UNIQUE_NAME:
      return handle(Type::UniqueName(), isolate);
    case CompareIC::OBJECT:
      return handle(Type::Receiver(), isolate);
    case CompareIC::KNOWN_OBJECT:
      return handle(
          map.is_null() ? Type::Receiver() : Type::Class(map), isolate);
    case CompareIC::GENERIC:
      return handle(Type::Any(), isolate);
  }
  UNREACHABLE();
  return Handle<Type>();
}


void CompareIC::StubInfoToType(int stub_minor_key,
                               Handle<Type>* left_type,
                               Handle<Type>* right_type,
                               Handle<Type>* overall_type,
                               Handle<Map> map,
                               Isolate* isolate) {
  State left_state, right_state, handler_state;
  ICCompareStub::DecodeMinorKey(stub_minor_key, &left_state, &right_state,
                                &handler_state, NULL);
  *left_type = StateToType(isolate, left_state);
  *right_type = StateToType(isolate, right_state);
  *overall_type = StateToType(isolate, handler_state, map);
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


void CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
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
  set_target(*stub.GetCode(isolate()));

#ifdef DEBUG
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
#endif

  // Activate inlined smi code.
  if (previous_state == UNINITIALIZED) {
    PatchInlinedSmiCode(address(), ENABLE_INLINED_SMI_CHECK);
  }
}


// Used from ICCompareStub::GenerateMiss in code-stubs-<arch>.cc.
RUNTIME_FUNCTION(Code*, CompareIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
  return ic.raw_target();
}


void CompareNilIC::Clear(Address address, Code* target) {
  if (IsCleared(target)) return;
  Code::ExtraICState state = target->extended_extra_ic_state();

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
  Code::ExtraICState extra_ic_state = target()->extended_extra_ic_state();

  CompareNilICStub stub(extra_ic_state);

  // Extract the current supported types from the patched IC and calculate what
  // types must be supported as a result of the miss.
  bool already_monomorphic = stub.IsMonomorphic();

  stub.UpdateStatus(object);

  NilValue nil = stub.GetNilValue();

  // Find or create the specialized stub to support the new set of types.
  Handle<Code> code;
  if (stub.IsMonomorphic()) {
    Handle<Map> monomorphic_map(already_monomorphic
                                ? target()->FindFirstMap()
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


MaybeObject* ToBooleanIC::ToBoolean(Handle<Object> object,
                                    Code::ExtraICState extra_ic_state) {
  ToBooleanStub stub(extra_ic_state);
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
  Code::ExtraICState extra_ic_state = ic.target()->extended_extra_ic_state();
  return ic.ToBoolean(object, extra_ic_state);
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
