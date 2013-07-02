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
                 Handle<Object> name,
                 State old_state,
                 Code* new_target) {
  if (FLAG_trace_ic) {
    Object* undef = new_target->GetHeap()->undefined_value();
    State new_state = StateFrom(new_target, undef, undef);
    PrintF("[%s in ", type);
    Isolate* isolate = new_target->GetIsolate();
    StackFrameIterator it(isolate);
    while (it.frame()->fp() != this->fp()) it.Advance();
    StackFrame* raw_frame = it.frame();
    if (raw_frame->is_internal()) {
      Code* apply_builtin = isolate->builtins()->builtin(
          Builtins::kFunctionApply);
      if (raw_frame->unchecked_code() == apply_builtin) {
        PrintF("apply from ");
        it.Advance();
        raw_frame = it.frame();
      }
    }
    JavaScriptFrame::PrintTop(isolate, stdout, false, true);
    Code::ExtraICState state = new_target->extra_ic_state();
    const char* modifier =
        GetTransitionMarkModifier(Code::GetKeyedAccessStoreMode(state));
    PrintF(" (%c->%c%s)",
           TransitionMarkFromState(old_state),
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

#define TRACE_IC(type, name, old_state, new_target)             \
  ASSERT((TraceIC(type, name, old_state, new_target), true))

IC::IC(FrameDepth depth, Isolate* isolate) : isolate_(isolate) {
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
  JSFunction* function = JSFunction::cast(frame->function());
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


static bool TryRemoveInvalidPrototypeDependentStub(Code* target,
                                                   Object* receiver,
                                                   Object* name) {
  if (target->is_keyed_load_stub() ||
      target->is_keyed_call_stub() ||
      target->is_keyed_store_stub()) {
    // Determine whether the failure is due to a name failure.
    if (!name->IsName()) return false;
    Name* stub_name = target->FindFirstName();
    if (Name::cast(name) != stub_name) return false;
  }

  InlineCacheHolderFlag cache_holder =
      Code::ExtractCacheHolderFromFlags(target->flags());

  Isolate* isolate = target->GetIsolate();
  if (cache_holder == OWN_MAP && !receiver->IsJSObject()) {
    // The stub was generated for JSObject but called for non-JSObject.
    // IC::GetCodeCacheHolder is not applicable.
    return false;
  } else if (cache_holder == PROTOTYPE_MAP &&
             receiver->GetPrototype(isolate)->IsNull()) {
    // IC::GetCodeCacheHolder is not applicable.
    return false;
  }
  Map* map = IC::GetCodeCacheHolder(isolate, receiver, cache_holder)->map();

  // Decide whether the inline cache failed because of changes to the
  // receiver itself or changes to one of its prototypes.
  //
  // If there are changes to the receiver itself, the map of the
  // receiver will have changed and the current target will not be in
  // the receiver map's code cache.  Therefore, if the current target
  // is in the receiver map's code cache, the inline cache failed due
  // to prototype check failure.
  int index = map->IndexInCodeCache(name, target);
  if (index >= 0) {
    map->RemoveFromCodeCache(String::cast(name), target, index);
    // For loads, handlers are stored in addition to the ICs on the map. Remove
    // those, too.
    if (target->is_load_stub() || target->is_keyed_load_stub()) {
      Code* handler = target->FindFirstCode();
      index = map->IndexInCodeCache(name, handler);
      if (index >= 0) {
        map->RemoveFromCodeCache(String::cast(name), handler, index);
      }
    }
    return true;
  }

  // If the IC is shared between multiple receivers (slow dictionary mode), then
  // the map cannot be deprecated and the stub invalidated.
  if (cache_holder != OWN_MAP) return false;

  // The stub is not in the cache. We've ruled out all other kinds of failure
  // except for proptotype chain changes, a deprecated map, or a map that's
  // different from the one that the stub expects. If the map hasn't changed,
  // assume it's a prototype failure. Treat deprecated maps in the same way as
  // prototype failures (stay monomorphic if possible).
  Map* old_map = target->FindFirstMap();
  if (old_map == NULL) return false;
  return old_map == map || old_map->is_deprecated();
}


IC::State IC::StateFrom(Code* target, Object* receiver, Object* name) {
  IC::State state = target->ic_state();

  if (state != MONOMORPHIC || !name->IsString()) return state;
  if (receiver->IsUndefined() || receiver->IsNull()) return state;

  Code::Kind kind = target->kind();
  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  // Call stubs handle this later to allow extra IC state
  // transitions.
  if (kind != Code::CALL_IC && kind != Code::KEYED_CALL_IC &&
      TryRemoveInvalidPrototypeDependentStub(target, receiver, name)) {
    return MONOMORPHIC_PROTOTYPE_FAILURE;
  }

  // The builtins object is special.  It only changes when JavaScript
  // builtins are loaded lazily.  It is important to keep inline
  // caches for the builtins object monomorphic.  Therefore, if we get
  // an inline cache miss for the builtins object after lazily loading
  // JavaScript builtins, we return uninitialized as the state to
  // force the inline cache back to monomorphic state.
  if (receiver->IsJSBuiltinsObject()) {
    return UNINITIALIZED;
  }

  return MONOMORPHIC;
}


RelocInfo::Mode IC::ComputeMode() {
  Address addr = address();
  Code* code = Code::cast(isolate()->heap()->FindCodeObject(addr));
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


void IC::Clear(Address address) {
  Code* target = GetTargetAtAddress(address);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->is_debug_break()) return;

  switch (target->kind()) {
    case Code::LOAD_IC: return LoadIC::Clear(address, target);
    case Code::KEYED_LOAD_IC: return KeyedLoadIC::Clear(address, target);
    case Code::STORE_IC: return StoreIC::Clear(address, target);
    case Code::KEYED_STORE_IC: return KeyedStoreIC::Clear(address, target);
    case Code::CALL_IC: return CallIC::Clear(address, target);
    case Code::KEYED_CALL_IC:  return KeyedCallIC::Clear(address, target);
    case Code::COMPARE_IC: return CompareIC::Clear(address, target);
    case Code::COMPARE_NIL_IC: return CompareNilIC::Clear(address, target);
    case Code::UNARY_OP_IC:
    case Code::BINARY_OP_IC:
    case Code::TO_BOOLEAN_IC:
      // Clearing these is tricky and does not
      // make any performance difference.
      return;
    default: UNREACHABLE();
  }
}


void CallICBase::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  bool contextual = CallICBase::Contextual::decode(target->extra_ic_state());
  Code* code =
      Isolate::Current()->stub_cache()->FindCallInitialize(
          target->arguments_count(),
          contextual ? RelocInfo::CODE_TARGET_CONTEXT : RelocInfo::CODE_TARGET,
          target->kind());
  SetTargetAtAddress(address, code);
}


void KeyedLoadIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  // Make sure to also clear the map used in inline fast cases.  If we
  // do not clear these maps, cached code can keep objects alive
  // through the embedded maps.
  SetTargetAtAddress(address, *initialize_stub());
}


void LoadIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address, *initialize_stub());
}


void StoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address,
      (Code::GetStrictMode(target->extra_ic_state()) == kStrictMode)
        ? *initialize_stub_strict()
        : *initialize_stub());
}


void KeyedStoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address,
      (Code::GetStrictMode(target->extra_ic_state()) == kStrictMode)
        ? *initialize_stub_strict()
        : *initialize_stub());
}


void CompareIC::Clear(Address address, Code* target) {
  ASSERT(target->major_key() == CodeStub::CompareIC);
  CompareIC::State handler_state;
  Token::Value op;
  ICCompareStub::DecodeMinorKey(target->stub_info(), NULL, NULL,
                                &handler_state, &op);
  // Only clear CompareICs that can retain objects.
  if (handler_state != KNOWN_OBJECT) return;
  SetTargetAtAddress(address, GetRawUninitialized(op));
  PatchInlinedSmiCode(address, DISABLE_INLINED_SMI_CHECK);
}


static bool HasInterceptorGetter(JSObject* object) {
  return !object->GetNamedInterceptor()->getter()->IsUndefined();
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


Handle<Object> CallICBase::TryCallAsFunction(Handle<Object> object) {
  Handle<Object> delegate = Execution::GetFunctionDelegate(object);

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


MaybeObject* CallICBase::LoadFunction(State state,
                                      Code::ExtraICState extra_ic_state,
                                      Handle<Object> object,
                                      Handle<String> name) {
  if (object->IsJSObject()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    if (receiver->map()->is_deprecated()) {
      JSObject::MigrateInstance(receiver);
    }
  }

  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, name);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<Object> result = Object::GetElement(object, index);
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
  if (FLAG_use_ic) {
    UpdateCaches(&lookup, state, extra_ic_state, object, name);
  }

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


bool CallICBase::TryUpdateExtraICState(LookupResult* lookup,
                                       Handle<Object> object,
                                       Code::ExtraICState* extra_ic_state) {
  ASSERT(kind_ == Code::CALL_IC);
  if (lookup->type() != CONSTANT_FUNCTION) return false;
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
        if (StringStubState::decode(*extra_ic_state) == DEFAULT_STRING_STUB &&
            argc >= 1 && args[1]->IsNumber()) {
          double index = DoubleToInteger(args.number_at(1));
          if (index < 0 || index >= string->length()) {
            *extra_ic_state =
                StringStubState::update(*extra_ic_state,
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


Handle<Code> CallICBase::ComputeMonomorphicStub(LookupResult* lookup,
                                                State state,
                                                Code::ExtraICState extra_state,
                                                Handle<Object> object,
                                                Handle<String> name) {
  int argc = target()->arguments_count();
  Handle<JSObject> holder(lookup->holder(), isolate());
  switch (lookup->type()) {
    case FIELD: {
      PropertyIndex index = lookup->GetFieldIndex();
      return isolate()->stub_cache()->ComputeCallField(
          argc, kind_, extra_state, name, object, holder, index);
    }
    case CONSTANT_FUNCTION: {
      // Get the constant function and compute the code stub for this
      // call; used for rewriting to monomorphic state and making sure
      // that the code stub is in the stub cache.
      Handle<JSFunction> function(lookup->GetConstantFunction(), isolate());
      return isolate()->stub_cache()->ComputeCallConstant(
          argc, kind_, extra_state, name, object, holder, function);
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
            argc, kind_, extra_state, name, receiver, global, cell, function);
      } else {
        // There is only one shared stub for calling normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the receiver for the stub to be
        // applicable.
        if (!holder.is_identical_to(receiver)) return Handle<Code>::null();
        return isolate()->stub_cache()->ComputeCallNormal(
            argc, kind_, extra_state);
      }
      break;
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(*holder));
      return isolate()->stub_cache()->ComputeCallInterceptor(
          argc, kind_, extra_state, name, object, holder);
    default:
      return Handle<Code>::null();
  }
}


void CallICBase::UpdateCaches(LookupResult* lookup,
                              State state,
                              Code::ExtraICState extra_ic_state,
                              Handle<Object> object,
                              Handle<String> name) {
  // Bail out if we didn't find a result.
  if (!lookup->IsProperty() || !lookup->IsCacheable()) return;

  // Compute the number of arguments.
  int argc = target()->arguments_count();
  Handle<Code> code;
  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = isolate()->stub_cache()->ComputeCallPreMonomorphic(
        argc, kind_, extra_ic_state);
  } else if (state == MONOMORPHIC) {
    if (kind_ == Code::CALL_IC &&
        TryUpdateExtraICState(lookup, object, &extra_ic_state)) {
      code = ComputeMonomorphicStub(lookup, state, extra_ic_state,
                                    object, name);
    } else if (TryRemoveInvalidPrototypeDependentStub(target(),
                                                      *object,
                                                      *name)) {
      state = MONOMORPHIC_PROTOTYPE_FAILURE;
      code = ComputeMonomorphicStub(lookup, state, extra_ic_state,
                                    object, name);
    } else {
      code = isolate()->stub_cache()->ComputeCallMegamorphic(
          argc, kind_, extra_ic_state);
    }
  } else {
    code = ComputeMonomorphicStub(lookup, state, extra_ic_state,
                                  object, name);
  }

  // If there's no appropriate stub we simply avoid updating the caches.
  if (code.is_null()) return;

  // Patch the call site depending on the state of the cache.
  switch (state) {
    case UNINITIALIZED:
    case MONOMORPHIC_PROTOTYPE_FAILURE:
    case PREMONOMORPHIC:
    case MONOMORPHIC:
      set_target(*code);
      break;
    case MEGAMORPHIC: {
      // Cache code holding map should be consistent with
      // GenerateMonomorphicCacheProbe. It is not the map which holds the stub.
      Handle<JSObject> cache_object = object->IsJSObject()
          ? Handle<JSObject>::cast(object)
          : Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate())),
                             isolate());
      // Update the stub cache.
      UpdateMegamorphicCache(cache_object->map(), *name, *code);
      break;
    }
    case DEBUG_STUB:
      break;
    case POLYMORPHIC:
    case GENERIC:
      UNREACHABLE();
      break;
  }

  TRACE_IC(kind_ == Code::CALL_IC ? "CallIC" : "KeyedCallIC",
           name, state, target());
}


MaybeObject* KeyedCallIC::LoadFunction(State state,
                                       Handle<Object> object,
                                       Handle<Object> key) {
  if (key->IsInternalizedString()) {
    return CallICBase::LoadFunction(state,
                                    Code::kNoExtraICState,
                                    object,
                                    Handle<String>::cast(key));
  }

  if (object->IsJSObject()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    if (receiver->map()->is_deprecated()) {
      JSObject::MigrateInstance(receiver);
    }
  }

  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, key);
  }

  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded();
  ASSERT(!(use_ic && object->IsJSGlobalProxy()));

  if (use_ic && state != MEGAMORPHIC) {
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
    TRACE_IC("KeyedCallIC", key, state, target());
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


MaybeObject* LoadIC::Load(State state,
                          Handle<Object> object,
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
    if ((object->IsString() || object->IsStringWrapper()) &&
        name->Equals(isolate()->heap()->length_string())) {
      Handle<Code> stub;
      if (state == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state == PREMONOMORPHIC) {
        StringLengthStub string_length_stub(kind(), !object->IsString());
        stub = string_length_stub.GetCode(isolate());
      } else if (state == MONOMORPHIC && object->IsStringWrapper()) {
        StringLengthStub string_length_stub(kind(), true);
        stub = string_length_stub.GetCode(isolate());
      } else if (state != MEGAMORPHIC) {
        ASSERT(state != GENERIC);
        stub = megamorphic_stub();
      }
      if (!stub.is_null()) {
        set_target(*stub);
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#length /string]\n");
#endif
      }
      // Get the string if we have a string wrapper object.
      Handle<Object> string = object->IsJSValue()
          ? Handle<Object>(Handle<JSValue>::cast(object)->value(), isolate())
          : object;
      return Smi::FromInt(String::cast(*string)->length());
    }

    // Use specialized code for getting prototype of functions.
    if (object->IsJSFunction() &&
        name->Equals(isolate()->heap()->prototype_string()) &&
        Handle<JSFunction>::cast(object)->should_have_prototype()) {
      Handle<Code> stub;
      if (state == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state == PREMONOMORPHIC) {
        FunctionPrototypeStub function_prototype_stub(kind());
        stub = function_prototype_stub.GetCode(isolate());
      } else if (state != MEGAMORPHIC) {
        ASSERT(state != GENERIC);
        stub = megamorphic_stub();
      }
      if (!stub.is_null()) {
        set_target(*stub);
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#prototype /function]\n");
#endif
      }
      return *Accessors::FunctionGetPrototype(object);
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

  if (object->IsJSObject()) {
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);
    if (receiver->map()->is_deprecated()) {
      JSObject::MigrateInstance(receiver);
    }
  }

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
  if (FLAG_use_ic) UpdateCaches(&lookup, state, object, name);

  PropertyAttributes attr;
  if (lookup.IsInterceptor() || lookup.IsHandler()) {
    // Get the property.
    Handle<Object> result =
        Object::GetProperty(object, object, &lookup, name, &attr);
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    // If the property is not present, check if we need to throw an
    // exception.
    if (attr == ABSENT && IsUndeclaredGlobal(object)) {
      return ReferenceError("not_defined", name);
    }
    return *result;
  }

  // Get the property.
  return Object::GetPropertyOrFail(object, object, &lookup, name, &attr);
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


bool IC::UpdatePolymorphicIC(State state,
                             StrictModeFlag strict_mode,
                             Handle<JSObject> receiver,
                             Handle<String> name,
                             Handle<Code> code) {
  if (code->type() == Code::NORMAL) return false;
  if (target()->ic_state() == MONOMORPHIC &&
      target()->type() == Code::NORMAL) {
    return false;
  }

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

    // Only allow 0 maps in case target() was reset to UNINITIALIZED by the GC.
    // In that case, allow the IC to go back monomorphic.
    if (number_of_maps == 0 && target()->ic_state() != UNINITIALIZED) {
      return false;
    }
    target()->FindAllCode(&handlers, receiver_maps.length());
  }

  number_of_valid_maps++;
  if (handler_to_overwrite >= 0) {
    handlers.Set(handler_to_overwrite, code);
  } else {
    receiver_maps.Add(new_receiver_map);
    handlers.Add(code);
  }

  Handle<Code> ic = isolate()->stub_cache()->ComputePolymorphicIC(
      &receiver_maps, &handlers, number_of_valid_maps, name);
  set_target(*ic);
  return true;
}


void LoadIC::UpdateMonomorphicIC(Handle<JSObject> receiver,
                                 Handle<Code> handler,
                                 Handle<String> name) {
  if (handler->type() == Code::NORMAL) return set_target(*handler);
  Handle<Code> ic = isolate()->stub_cache()->ComputeMonomorphicIC(
      receiver, handler, name);
  set_target(*ic);
}


void KeyedLoadIC::UpdateMonomorphicIC(Handle<JSObject> receiver,
                                      Handle<Code> handler,
                                      Handle<String> name) {
  if (handler->type() == Code::NORMAL) return set_target(*handler);
  Handle<Code> ic = isolate()->stub_cache()->ComputeKeyedMonomorphicIC(
      receiver, handler, name);
  set_target(*ic);
}


void IC::CopyICToMegamorphicCache(Handle<String> name) {
  MapHandleList receiver_maps;
  CodeHandleList handlers;
  {
    DisallowHeapAllocation no_gc;
    target()->FindAllMaps(&receiver_maps);
    target()->FindAllCode(&handlers, receiver_maps.length());
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


// Since GC may have been invoked, by the time PatchCache is called, |state| is
// not necessarily equal to target()->state().
void IC::PatchCache(State state,
                    StrictModeFlag strict_mode,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Code> code) {
  switch (state) {
    case UNINITIALIZED:
    case PREMONOMORPHIC:
    case MONOMORPHIC_PROTOTYPE_FAILURE:
      UpdateMonomorphicIC(receiver, code, name);
      break;
    case MONOMORPHIC:
      // Only move to megamorphic if the target changes.
      if (target() != *code) {
        if (target()->is_load_stub()) {
          bool is_same_handler = false;
          {
            DisallowHeapAllocation no_allocation;
            Code* old_handler = target()->FindFirstCode();
            is_same_handler = old_handler == *code;
          }
          if (is_same_handler
              && IsTransitionedMapOfMonomorphicTarget(receiver->map())) {
            UpdateMonomorphicIC(receiver, code, name);
            break;
          }
          if (UpdatePolymorphicIC(state, strict_mode, receiver, name, code)) {
            break;
          }

          if (target()->type() != Code::NORMAL) {
            CopyICToMegamorphicCache(name);
          }
        }

        UpdateMegamorphicCache(receiver->map(), *name, *code);
        set_target((strict_mode == kStrictMode)
                     ? *megamorphic_stub_strict()
                     : *megamorphic_stub());
      }
      break;
    case MEGAMORPHIC:
      // Update the stub cache.
      UpdateMegamorphicCache(receiver->map(), *name, *code);
      break;
    case POLYMORPHIC:
      if (target()->is_load_stub()) {
        if (UpdatePolymorphicIC(state, strict_mode, receiver, name, code)) {
          break;
        }
        CopyICToMegamorphicCache(name);
        UpdateMegamorphicCache(receiver->map(), *name, *code);
        set_target(*megamorphic_stub());
      } else {
        // When trying to patch a polymorphic keyed load/store element stub
        // with anything other than another polymorphic stub, go generic.
        set_target((strict_mode == kStrictMode)
                   ? *generic_stub_strict()
                   : *generic_stub());
      }
      break;
    case DEBUG_STUB:
      break;
    case GENERIC:
      UNREACHABLE();
      break;
  }
}


static void GetReceiverMapsForStub(Handle<Code> stub,
                                   MapHandleList* result) {
  ASSERT(stub->is_inline_cache_stub());
  switch (stub->ic_state()) {
    case MONOMORPHIC: {
      Map* map = stub->FindFirstMap();
      if (map != NULL) {
        result->Add(Handle<Map>(map));
      }
      break;
    }
    case POLYMORPHIC: {
      DisallowHeapAllocation no_allocation;
      int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
      for (RelocIterator it(*stub, mask); !it.done(); it.next()) {
        RelocInfo* info = it.rinfo();
        Handle<Object> object(info->target_object(), stub->GetIsolate());
        if (object->IsString()) break;
        ASSERT(object->IsMap());
        AddOneReceiverMapIfMissing(result, Handle<Map>::cast(object));
      }
      break;
    }
    case MEGAMORPHIC:
      break;
    case UNINITIALIZED:
    case PREMONOMORPHIC:
    case MONOMORPHIC_PROTOTYPE_FAILURE:
    case GENERIC:
    case DEBUG_STUB:
      UNREACHABLE();
      break;
  }
}


void LoadIC::UpdateCaches(LookupResult* lookup,
                          State state,
                          Handle<Object> object,
                          Handle<String> name) {
  // Bail out if the result is not cacheable.
  if (!lookup->IsCacheable()) {
    set_target(*generic_stub());
    return;
  }

  // TODO(jkummerow): It would be nice to support non-JSObjects in
  // UpdateCaches, then we wouldn't need to go generic here.
  if (!object->IsJSObject()) {
    set_target(*generic_stub());
    return;
  }

  Handle<JSObject> receiver = Handle<JSObject>::cast(object);
  Handle<Code> code;
  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = pre_monomorphic_stub();
  } else {
    code = ComputeLoadHandler(lookup, receiver, name);
    if (code.is_null()) {
      set_target(*generic_stub());
      return;
    }
  }

  PatchCache(state, kNonStrictMode, receiver, name, code);
  TRACE_IC("LoadIC", name, state, target());
}


void IC::UpdateMegamorphicCache(Map* map, Name* name, Code* code) {
  // Cache code holding map should be consistent with
  // GenerateMonomorphicCacheProbe.
  isolate()->stub_cache()->Set(name, map, code);
}


Handle<Code> LoadIC::ComputeLoadHandler(LookupResult* lookup,
                                        Handle<JSObject> receiver,
                                        Handle<String> name) {
  if (!lookup->IsProperty()) {
    // Nonexistent property. The result is undefined.
    return isolate()->stub_cache()->ComputeLoadNonexistent(name, receiver);
  }

  // Compute monomorphic stub.
  Handle<JSObject> holder(lookup->holder());
  switch (lookup->type()) {
    case FIELD:
      return isolate()->stub_cache()->ComputeLoadField(
          name, receiver, holder,
          lookup->GetFieldIndex(), lookup->representation());
    case CONSTANT_FUNCTION: {
      Handle<JSFunction> constant(lookup->GetConstantFunction());
      return isolate()->stub_cache()->ComputeLoadConstant(
          name, receiver, holder, constant);
    }
    case NORMAL:
      if (holder->IsGlobalObject()) {
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(holder);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        return isolate()->stub_cache()->ComputeLoadGlobal(
            name, receiver, global, cell, lookup->IsDontDelete());
      }
      // There is only one shared stub for loading normalized
      // properties. It does not traverse the prototype chain, so the
      // property must be found in the receiver for the stub to be
      // applicable.
      if (!holder.is_identical_to(receiver)) break;
      return isolate()->stub_cache()->ComputeLoadNormal(name, receiver);
    case CALLBACKS: {
      Handle<Object> callback(lookup->GetCallbackObject(), isolate());
      if (callback->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(callback);
        if (v8::ToCData<Address>(info->getter()) == 0) break;
        if (!info->IsCompatibleReceiver(*receiver)) break;
        return isolate()->stub_cache()->ComputeLoadCallback(
            name, receiver, holder, info);
      } else if (callback->IsAccessorPair()) {
        Handle<Object> getter(Handle<AccessorPair>::cast(callback)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (holder->IsGlobalObject()) break;
        if (!holder->HasFastProperties()) break;
        return isolate()->stub_cache()->ComputeLoadViaGetter(
            name, receiver, holder, Handle<JSFunction>::cast(getter));
      } else if (receiver->IsJSArray() &&
          name->Equals(isolate()->heap()->length_string())) {
        PropertyIndex lengthIndex =
          PropertyIndex::NewHeaderIndex(JSArray::kLengthOffset / kPointerSize);
        return isolate()->stub_cache()->ComputeLoadField(
            name, receiver, holder, lengthIndex, Representation::Tagged());
      }
      // TODO(dcarney): Handle correctly.
      if (callback->IsDeclaredAccessorInfo()) break;
      ASSERT(callback->IsForeign());
      // No IC support for old-style native accessors.
      break;
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(*holder));
      return isolate()->stub_cache()->ComputeLoadInterceptor(
          name, receiver, holder);
    default:
      break;
  }
  return Handle<Code>::null();
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
  State ic_state = target()->ic_state();

  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "non-NORMAL target type");
    return generic_stub();
  }

  Handle<Map> receiver_map(receiver->map(), isolate());
  MapHandleList target_receiver_maps;
  if (ic_state == UNINITIALIZED || ic_state == PREMONOMORPHIC) {
    // Optimistically assume that ICs that haven't reached the MONOMORPHIC state
    // yet will do so and stay there.
    return isolate()->stub_cache()->ComputeKeyedLoadElement(receiver_map);
  }

  if (target() == *string_stub()) {
    target_receiver_maps.Add(isolate()->factory()->string_map());
  } else {
    GetReceiverMapsForStub(Handle<Code>(target(), isolate()),
                           &target_receiver_maps);
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
  if (ic_state == MONOMORPHIC &&
      IsMoreGeneralElementsKindTransition(
          target_receiver_maps.at(0)->elements_kind(),
          receiver->GetElementsKind())) {
    return isolate()->stub_cache()->ComputeKeyedLoadElement(receiver_map);
  }

  ASSERT(ic_state != GENERIC);

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


MaybeObject* KeyedLoadIC::Load(State state,
                               Handle<Object> object,
                               Handle<Object> key,
                               ICMissMode miss_mode) {
  // Check for values that can be converted into an internalized string directly
  // or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString()) {
    return LoadIC::Load(state, object, Handle<String>::cast(key));
  }

  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded();
  ASSERT(!(use_ic && object->IsJSGlobalProxy()));

  if (use_ic) {
    Handle<Code> stub = generic_stub();
    if (miss_mode != MISS_FORCE_GENERIC) {
      if (object->IsString() && key->IsNumber()) {
        if (state == UNINITIALIZED) {
          stub = string_stub();
        }
      } else if (object->IsJSObject()) {
        Handle<JSObject> receiver = Handle<JSObject>::cast(object);
        if (receiver->map()->is_deprecated()) {
          JSObject::MigrateInstance(receiver);
        }

        if (receiver->elements()->map() ==
            isolate()->heap()->non_strict_arguments_elements_map()) {
          stub = non_strict_arguments_stub();
        } else if (receiver->HasIndexedInterceptor()) {
          stub = indexed_interceptor_stub();
        } else if (!key->ToSmi()->IsFailure() &&
                   (target() != *non_strict_arguments_stub())) {
          stub = LoadElementStub(receiver);
        }
      }
    } else {
      TRACE_GENERIC_IC(isolate(), "KeyedLoadIC", "force generic");
    }
    ASSERT(!stub.is_null());
    set_target(*stub);
    TRACE_IC("KeyedLoadIC", key, state, target());
  }


  return Runtime::GetObjectPropertyOrFail(isolate(), object, key);
}


Handle<Code> KeyedLoadIC::ComputeLoadHandler(LookupResult* lookup,
                                             Handle<JSObject> receiver,
                                             Handle<String> name) {
  // Bail out if we didn't find a result.
  if (!lookup->IsProperty()) return Handle<Code>::null();

  // Compute a monomorphic stub.
  Handle<JSObject> holder(lookup->holder(), isolate());
  switch (lookup->type()) {
    case FIELD:
      return isolate()->stub_cache()->ComputeKeyedLoadField(
          name, receiver, holder,
          lookup->GetFieldIndex(), lookup->representation());
    case CONSTANT_FUNCTION: {
      Handle<JSFunction> constant(lookup->GetConstantFunction(), isolate());
      return isolate()->stub_cache()->ComputeKeyedLoadConstant(
          name, receiver, holder, constant);
    }
    case CALLBACKS: {
      Handle<Object> callback_object(lookup->GetCallbackObject(), isolate());
      // TODO(dcarney): Handle DeclaredAccessorInfo correctly.
      if (!callback_object->IsExecutableAccessorInfo()) break;
      Handle<ExecutableAccessorInfo> callback =
          Handle<ExecutableAccessorInfo>::cast(callback_object);
      if (v8::ToCData<Address>(callback->getter()) == 0) break;
      if (!callback->IsCompatibleReceiver(*receiver)) break;
      return isolate()->stub_cache()->ComputeKeyedLoadCallback(
          name, receiver, holder, callback);
    }
    case INTERCEPTOR:
      ASSERT(HasInterceptorGetter(lookup->holder()));
      return isolate()->stub_cache()->ComputeKeyedLoadInterceptor(
          name, receiver, holder);
    default:
      // Always rewrite to the generic case so that we do not
      // repeatedly try to rewrite.
      return generic_stub();
  }
  return Handle<Code>::null();
}


static bool LookupForWrite(Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value,
                           LookupResult* lookup,
                           IC::State* state) {
  Handle<JSObject> holder = receiver;
  receiver->Lookup(*name, lookup);
  if (lookup->IsFound()) {
    if (lookup->IsReadOnly() || !lookup->IsCacheable()) return false;

    if (lookup->holder() == *receiver) {
      if (lookup->IsInterceptor() &&
          receiver->GetNamedInterceptor()->setter()->IsUndefined()) {
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
        target, target->LastAdded(), value->OptimalRepresentation());
    // Lookup the transition again since the transition tree may have changed
    // entirely by the migration above.
    receiver->map()->LookupTransition(*holder, *name, lookup);
    if (!lookup->IsTransition()) return false;
    *state = MONOMORPHIC_PROTOTYPE_FAILURE;
  }
  return true;
}


MaybeObject* StoreIC::Store(State state,
                            StrictModeFlag strict_mode,
                            Handle<Object> object,
                            Handle<String> name,
                            Handle<Object> value,
                            JSReceiver::StoreFromKeyed store_mode) {
  // Handle proxies.
  if (object->IsJSProxy()) {
    return JSReceiver::SetPropertyOrFail(
        Handle<JSReceiver>::cast(object), name, value, NONE, strict_mode);
  }

  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_store", object, name);
  }

  // The length property of string values is read-only. Throw in strict mode.
  if (strict_mode == kStrictMode && object->IsString() &&
      name->Equals(isolate()->heap()->length_string())) {
    return TypeError("strict_read_only_property", object, name);
  }

  // Ignore other stores where the receiver is not a JSObject.
  // TODO(1475): Must check prototype chains of object wrappers.
  if (!object->IsJSObject()) return *value;

  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  if (receiver->map()->is_deprecated()) {
    JSObject::MigrateInstance(receiver);
  }

  // Check if the given name is an array index.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<Object> result =
        JSObject::SetElement(receiver, index, value, NONE, strict_mode);
    RETURN_IF_EMPTY_HANDLE(isolate(), result);
    return *value;
  }

  // Observed objects are always modified through the runtime.
  if (FLAG_harmony_observation && receiver->map()->is_observed()) {
    return JSReceiver::SetPropertyOrFail(
        receiver, name, value, NONE, strict_mode, store_mode);
  }

  // Use specialized code for setting the length of arrays with fast
  // properties. Slow properties might indicate redefinition of the length
  // property.
  if (FLAG_use_ic &&
      receiver->IsJSArray() &&
      name->Equals(isolate()->heap()->length_string()) &&
      Handle<JSArray>::cast(receiver)->AllowsSetElementsLength() &&
      receiver->HasFastProperties()) {
    Handle<Code> stub =
        StoreArrayLengthStub(kind(), strict_mode).GetCode(isolate());
    set_target(*stub);
    TRACE_IC("StoreIC", name, state, *stub);
    return JSReceiver::SetPropertyOrFail(
        receiver, name, value, NONE, strict_mode, store_mode);
  }

  if (receiver->IsJSGlobalProxy()) {
    if (FLAG_use_ic && kind() != Code::KEYED_STORE_IC) {
      // Generate a generic stub that goes to the runtime when we see a global
      // proxy as receiver.
      Handle<Code> stub = (strict_mode == kStrictMode)
          ? global_proxy_stub_strict()
          : global_proxy_stub();
      set_target(*stub);
      TRACE_IC("StoreIC", name, state, *stub);
    }
    return JSReceiver::SetPropertyOrFail(
        receiver, name, value, NONE, strict_mode, store_mode);
  }

  LookupResult lookup(isolate());
  if (LookupForWrite(receiver, name, value, &lookup, &state)) {
    if (FLAG_use_ic) {
      UpdateCaches(&lookup, state, strict_mode, receiver, name, value);
    }
  } else if (strict_mode == kStrictMode &&
             !(lookup.IsProperty() && lookup.IsReadOnly()) &&
             IsUndeclaredGlobal(object)) {
    // Strict mode doesn't allow setting non-existent global property.
    return ReferenceError("not_defined", name);
  } else if (FLAG_use_ic &&
             (lookup.IsNormal() ||
              (lookup.IsField() && lookup.CanHoldValue(value)))) {
    Handle<Code> stub = strict_mode == kStrictMode
        ? generic_stub_strict() : generic_stub();
    set_target(*stub);
  }

  // Set the property.
  return JSReceiver::SetPropertyOrFail(
      receiver, name, value, NONE, strict_mode, store_mode);
}


void StoreIC::UpdateCaches(LookupResult* lookup,
                           State state,
                           StrictModeFlag strict_mode,
                           Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value) {
  ASSERT(!receiver->IsJSGlobalProxy());
  ASSERT(lookup->IsFound());

  // These are not cacheable, so we never see such LookupResults here.
  ASSERT(!lookup->IsHandler());

  Handle<Code> code = ComputeStoreMonomorphic(
      lookup, strict_mode, receiver, name);
  if (code.is_null()) {
    Handle<Code> stub = strict_mode == kStrictMode
        ? generic_stub_strict() : generic_stub();
    set_target(*stub);
    return;
  }

  PatchCache(state, strict_mode, receiver, name, code);
  TRACE_IC("StoreIC", name, state, target());
}


Handle<Code> StoreIC::ComputeStoreMonomorphic(LookupResult* lookup,
                                              StrictModeFlag strict_mode,
                                              Handle<JSObject> receiver,
                                              Handle<String> name) {
  Handle<JSObject> holder(lookup->holder());
  switch (lookup->type()) {
    case FIELD:
      return isolate()->stub_cache()->ComputeStoreField(
          name, receiver, lookup, strict_mode);
    case NORMAL:
      if (receiver->IsGlobalObject()) {
        // The stub generated for the global object picks the value directly
        // from the property cell. So the property must be directly on the
        // global object.
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
        Handle<PropertyCell> cell(
            global->GetPropertyCell(lookup), isolate());
        return isolate()->stub_cache()->ComputeStoreGlobal(
            name, global, cell, strict_mode);
      }
      ASSERT(holder.is_identical_to(receiver));
      return isolate()->stub_cache()->ComputeStoreNormal(strict_mode);
    case CALLBACKS: {
      Handle<Object> callback(lookup->GetCallbackObject(), isolate());
      if (callback->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(callback);
        if (v8::ToCData<Address>(info->setter()) == 0) break;
        if (!holder->HasFastProperties()) break;
        if (!info->IsCompatibleReceiver(*receiver)) break;
        return isolate()->stub_cache()->ComputeStoreCallback(
            name, receiver, holder, info, strict_mode);
      } else if (callback->IsAccessorPair()) {
        Handle<Object> setter(
            Handle<AccessorPair>::cast(callback)->setter(), isolate());
        if (!setter->IsJSFunction()) break;
        if (holder->IsGlobalObject()) break;
        if (!holder->HasFastProperties()) break;
        return isolate()->stub_cache()->ComputeStoreViaSetter(
            name, receiver, holder, Handle<JSFunction>::cast(setter),
            strict_mode);
      }
      // TODO(dcarney): Handle correctly.
      if (callback->IsDeclaredAccessorInfo()) break;
      ASSERT(callback->IsForeign());
      // No IC support for old-style native accessors.
      break;
    }
    case INTERCEPTOR:
      ASSERT(!receiver->GetNamedInterceptor()->setter()->IsUndefined());
      return isolate()->stub_cache()->ComputeStoreInterceptor(
          name, receiver, strict_mode);
    case CONSTANT_FUNCTION:
      break;
    case TRANSITION: {
      // Explicitly pass in the receiver map since LookupForWrite may have
      // stored something else than the receiver in the holder.
      Handle<Map> transition(
          lookup->GetTransitionTarget(receiver->map()), isolate());
      int descriptor = transition->LastAdded();

      DescriptorArray* target_descriptors = transition->instance_descriptors();
      PropertyDetails details = target_descriptors->GetDetails(descriptor);

      if (details.type() == CALLBACKS || details.attributes() != NONE) break;

      return isolate()->stub_cache()->ComputeStoreTransition(
          name, receiver, lookup, transition, strict_mode);
    }
    case NONEXISTENT:
    case HANDLER:
      UNREACHABLE();
      break;
  }
  return Handle<Code>::null();
}


Handle<Code> KeyedStoreIC::StoreElementStub(Handle<JSObject> receiver,
                                            KeyedAccessStoreMode store_mode,
                                            StrictModeFlag strict_mode) {
  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != Code::NORMAL) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "non-NORMAL target type");
    return strict_mode == kStrictMode ? generic_stub_strict() : generic_stub();
  }

  if (!FLAG_compiled_keyed_stores &&
      (store_mode == STORE_NO_TRANSITION_HANDLE_COW ||
       store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS)) {
    // TODO(danno): We'll soon handle MONOMORPHIC ICs that also support
    // copying COW arrays and silently ignoring some OOB stores into external
    // arrays, but for now use the generic.
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "COW/OOB external array");
    return strict_mode == kStrictMode
        ? generic_stub_strict()
        : generic_stub();
  }

  State ic_state = target()->ic_state();
  Handle<Map> receiver_map(receiver->map(), isolate());
  if (ic_state == UNINITIALIZED || ic_state == PREMONOMORPHIC) {
    // Optimistically assume that ICs that haven't reached the MONOMORPHIC state
    // yet will do so and stay there.
    Handle<Map> monomorphic_map = ComputeTransitionedMap(receiver, store_mode);
    store_mode = GetNonTransitioningStoreMode(store_mode);
    return isolate()->stub_cache()->ComputeKeyedStoreElement(
        monomorphic_map, strict_mode, store_mode);
  }

  MapHandleList target_receiver_maps;
  target()->FindAllMaps(&target_receiver_maps);
  if (target_receiver_maps.length() == 0) {
    // In the case that there is a non-map-specific IC is installed (e.g. keyed
    // stores into properties in dictionary mode), then there will be not
    // receiver maps in the target.
    return strict_mode == kStrictMode
        ? generic_stub_strict()
        : generic_stub();
  }

  // There are several special cases where an IC that is MONOMORPHIC can still
  // transition to a different GetNonTransitioningStoreMode IC that handles a
  // superset of the original IC. Handle those here if the receiver map hasn't
  // changed or it has transitioned to a more general kind.
  KeyedAccessStoreMode old_store_mode =
      Code::GetKeyedAccessStoreMode(target()->extra_ic_state());
  Handle<Map> previous_receiver_map = target_receiver_maps.at(0);
  if (ic_state == MONOMORPHIC && old_store_mode == STANDARD_STORE) {
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
          transitioned_receiver_map, strict_mode, store_mode);
    } else if (*previous_receiver_map == receiver->map()) {
      if (IsGrowStoreMode(store_mode) ||
          store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS ||
          store_mode == STORE_NO_TRANSITION_HANDLE_COW) {
        // A "normal" IC that handles stores can switch to a version that can
        // grow at the end of the array, handle OOB accesses or copy COW arrays
        // and still stay MONOMORPHIC.
        return isolate()->stub_cache()->ComputeKeyedStoreElement(
            receiver_map, strict_mode, store_mode);
      }
    }
  }

  ASSERT(ic_state != GENERIC);

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
    return strict_mode == kStrictMode ? generic_stub_strict() : generic_stub();
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    TRACE_GENERIC_IC(isolate(), "KeyedIC", "max polymorph exceeded");
    return strict_mode == kStrictMode ? generic_stub_strict() : generic_stub();
  }

  // Make sure all polymorphic handlers have the same store mode, otherwise the
  // generic stub must be used.
  store_mode = GetNonTransitioningStoreMode(store_mode);
  if (old_store_mode != STANDARD_STORE) {
    if (store_mode == STANDARD_STORE) {
      store_mode = old_store_mode;
    } else if (store_mode != old_store_mode) {
      TRACE_GENERIC_IC(isolate(), "KeyedIC", "store mode mismatch");
      return strict_mode == kStrictMode
          ? generic_stub_strict()
          : generic_stub();
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
      return strict_mode == kStrictMode
          ? generic_stub_strict()
          : generic_stub();
    }
  }

  return isolate()->stub_cache()->ComputeStoreElementPolymorphic(
      &target_receiver_maps, store_mode, strict_mode);
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


MaybeObject* KeyedStoreIC::Store(State state,
                                 StrictModeFlag strict_mode,
                                 Handle<Object> object,
                                 Handle<Object> key,
                                 Handle<Object> value,
                                 ICMissMode miss_mode) {
  // Check for values that can be converted into an internalized string directly
  // or is representable as a smi.
  key = TryConvertKey(key, isolate());

  if (key->IsInternalizedString()) {
    return StoreIC::Store(state,
                          strict_mode,
                          object,
                          Handle<String>::cast(key),
                          value,
                          JSReceiver::MAY_BE_STORE_FROM_KEYED);
  }

  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded() &&
      !(FLAG_harmony_observation && object->IsJSObject() &&
        JSObject::cast(*object)->map()->is_observed());
  if (use_ic && !object->IsSmi()) {
    // Don't use ICs for maps of the objects in Array's prototype chain. We
    // expect to be able to trap element sets to objects with those maps in the
    // runtime to enable optimization of element hole access.
    Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
    if (heap_object->map()->IsMapInArrayPrototypeChain()) use_ic = false;
  }
  ASSERT(!(use_ic && object->IsJSGlobalProxy()));

  if (use_ic) {
    Handle<Code> stub = (strict_mode == kStrictMode)
        ? generic_stub_strict()
        : generic_stub();
    if (miss_mode != MISS_FORCE_GENERIC) {
      if (object->IsJSObject()) {
        Handle<JSObject> receiver = Handle<JSObject>::cast(object);
        if (receiver->map()->is_deprecated()) {
          JSObject::MigrateInstance(receiver);
        }
        bool key_is_smi_like = key->IsSmi() ||
            (FLAG_compiled_keyed_stores && !key->ToSmi()->IsFailure());
        if (receiver->elements()->map() ==
            isolate()->heap()->non_strict_arguments_elements_map()) {
          stub = non_strict_arguments_stub();
        } else if (key_is_smi_like &&
                   (target() != *non_strict_arguments_stub())) {
          KeyedAccessStoreMode store_mode = GetStoreMode(receiver, key, value);
          stub = StoreElementStub(receiver, store_mode, strict_mode);
        } else {
          TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "key not a number");
        }
      } else {
        TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "not an object");
      }
    } else {
      TRACE_GENERIC_IC(isolate(), "KeyedStoreIC", "force generic");
    }
    ASSERT(!stub.is_null());
    set_target(*stub);
    TRACE_IC("KeyedStoreIC", key, state, target());
  }

  return Runtime::SetObjectPropertyOrFail(
      isolate(), object , key, value, NONE, strict_mode);
}


Handle<Code> KeyedStoreIC::ComputeStoreMonomorphic(LookupResult* lookup,
                                                   StrictModeFlag strict_mode,
                                                   Handle<JSObject> receiver,
                                                   Handle<String> name) {
  // If the property has a non-field type allowing map transitions
  // where there is extra room in the object, we leave the IC in its
  // current state.
  switch (lookup->type()) {
    case FIELD:
      return isolate()->stub_cache()->ComputeKeyedStoreField(
          name, receiver, lookup, strict_mode);
    case TRANSITION: {
      // Explicitly pass in the receiver map since LookupForWrite may have
      // stored something else than the receiver in the holder.
      Handle<Map> transition(
          lookup->GetTransitionTarget(receiver->map()), isolate());
      int descriptor = transition->LastAdded();

      DescriptorArray* target_descriptors = transition->instance_descriptors();
      PropertyDetails details = target_descriptors->GetDetails(descriptor);

      if (details.type() != CALLBACKS && details.attributes() == NONE) {
        return isolate()->stub_cache()->ComputeKeyedStoreTransition(
            name, receiver, lookup, transition, strict_mode);
      }
      // fall through.
    }
    case NORMAL:
    case CONSTANT_FUNCTION:
    case CALLBACKS:
    case INTERCEPTOR:
      // Always rewrite to the generic case so that we do not
      // repeatedly try to rewrite.
      return (strict_mode == kStrictMode)
          ? generic_stub_strict()
          : generic_stub();
    case HANDLER:
    case NONEXISTENT:
      UNREACHABLE();
      break;
  }
  return Handle<Code>::null();
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
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  MaybeObject* maybe_result = ic.LoadFunction(state,
                                              extra_ic_state,
                                              args.at<Object>(0),
                                              args.at<String>(1));
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
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  MaybeObject* maybe_result =
      ic.LoadFunction(state, args.at<Object>(0), args.at<Object>(1));
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
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<String>(1));
}


// Used from ic-<arch>.cc
RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<Object>(1), MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::EXTRA_CALL_FRAME, isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<Object>(1), MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissForceGeneric) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(IC::NO_EXTRA_FRAME, isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state,
                 args.at<Object>(0),
                 args.at<Object>(1),
                 MISS_FORCE_GENERIC);
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, StoreIC_Miss) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  Code::GetStrictMode(extra_ic_state),
                  args.at<Object>(0),
                  args.at<String>(1),
                  args.at<Object>(2));
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
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  Code::GetStrictMode(extra_ic_state),
                  args.at<Object>(0),
                  args.at<Object>(1),
                  args.at<Object>(2),
                  MISS);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::EXTRA_CALL_FRAME, isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  Code::GetStrictMode(extra_ic_state),
                  args.at<Object>(0),
                  args.at<Object>(1),
                  args.at<Object>(2),
                  MISS);
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_Slow) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  StoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = Code::GetStrictMode(extra_ic_state);
  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    NONE,
                                    strict_mode);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Slow) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(IC::NO_EXTRA_FRAME, isolate);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode = Code::GetStrictMode(extra_ic_state);
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
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  Code::GetStrictMode(extra_ic_state),
                  args.at<Object>(0),
                  args.at<Object>(1),
                  args.at<Object>(2),
                  MISS_FORCE_GENERIC);
}


void UnaryOpIC::patch(Code* code) {
  set_target(code);
}


const char* UnaryOpIC::GetName(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED: return "Uninitialized";
    case SMI: return "Smi";
    case NUMBER: return "Number";
    case GENERIC: return "Generic";
    default: return "Invalid";
  }
}


UnaryOpIC::State UnaryOpIC::ToState(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED:
      return v8::internal::UNINITIALIZED;
    case SMI:
    case NUMBER:
      return MONOMORPHIC;
    case GENERIC:
      return v8::internal::GENERIC;
  }
  UNREACHABLE();
  return v8::internal::UNINITIALIZED;
}


Handle<Type> UnaryOpIC::TypeInfoToType(TypeInfo type_info, Isolate* isolate) {
  switch (type_info) {
    case UNINITIALIZED:
      return handle(Type::None(), isolate);
    case SMI:
      return handle(Type::Smi(), isolate);
    case NUMBER:
      return handle(Type::Number(), isolate);
    case GENERIC:
      return handle(Type::Any(), isolate);
  }
  UNREACHABLE();
  return handle(Type::Any(), isolate);
}


UnaryOpIC::TypeInfo UnaryOpIC::GetTypeInfo(Handle<Object> operand) {
  v8::internal::TypeInfo operand_type =
      v8::internal::TypeInfo::FromValue(operand);
  if (operand_type.IsSmi()) {
    return SMI;
  } else if (operand_type.IsNumber()) {
    return NUMBER;
  } else {
    return GENERIC;
  }
}


UnaryOpIC::TypeInfo UnaryOpIC::ComputeNewType(
    TypeInfo current_type,
    TypeInfo previous_type) {
  switch (previous_type) {
    case UNINITIALIZED:
      return current_type;
    case SMI:
      return (current_type == GENERIC) ? GENERIC : NUMBER;
    case NUMBER:
      return GENERIC;
    case GENERIC:
      // We should never do patching if we are in GENERIC state.
      UNREACHABLE();
      return GENERIC;
  }
  UNREACHABLE();
  return GENERIC;
}


void BinaryOpIC::patch(Code* code) {
  set_target(code);
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


BinaryOpIC::State BinaryOpIC::ToState(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED:
      return ::v8::internal::UNINITIALIZED;
    case SMI:
    case INT32:
    case NUMBER:
    case ODDBALL:
    case STRING:
      return MONOMORPHIC;
    case GENERIC:
      return ::v8::internal::GENERIC;
  }
  UNREACHABLE();
  return ::v8::internal::UNINITIALIZED;
}


Handle<Type> BinaryOpIC::TypeInfoToType(BinaryOpIC::TypeInfo binary_type,
                                        Isolate* isolate) {
  switch (binary_type) {
    case UNINITIALIZED:
      return handle(Type::None(), isolate);
    case SMI:
      return handle(Type::Smi(), isolate);
    case INT32:
      return handle(Type::Signed32(), isolate);
    case NUMBER:
      return handle(Type::Number(), isolate);
    case ODDBALL:
      return handle(Type::Optional(
          handle(Type::Union(
              handle(Type::Number(), isolate),
              handle(Type::String(), isolate)), isolate)), isolate);
    case STRING:
      return handle(Type::String(), isolate);
    case GENERIC:
      return handle(Type::Any(), isolate);
  }
  UNREACHABLE();
  return handle(Type::Any(), isolate);
}


void BinaryOpIC::StubInfoToType(int minor_key,
                                Handle<Type>* left,
                                Handle<Type>* right,
                                Handle<Type>* result,
                                Isolate* isolate) {
  TypeInfo left_typeinfo, right_typeinfo, result_typeinfo;
  BinaryOpStub::decode_types_from_minor_key(
      minor_key, &left_typeinfo, &right_typeinfo, &result_typeinfo);
  *left = TypeInfoToType(left_typeinfo, isolate);
  *right = TypeInfoToType(right_typeinfo, isolate);
  *result = TypeInfoToType(result_typeinfo, isolate);
}


RUNTIME_FUNCTION(MaybeObject*, UnaryOp_Patch) {
  ASSERT(args.length() == 4);

  HandleScope scope(isolate);
  Handle<Object> operand = args.at<Object>(0);
  Token::Value op = static_cast<Token::Value>(args.smi_at(1));
  UnaryOverwriteMode mode = static_cast<UnaryOverwriteMode>(args.smi_at(2));
  UnaryOpIC::TypeInfo previous_type =
      static_cast<UnaryOpIC::TypeInfo>(args.smi_at(3));

  UnaryOpIC::TypeInfo type = UnaryOpIC::GetTypeInfo(operand);
  type = UnaryOpIC::ComputeNewType(type, previous_type);

  UnaryOpStub stub(op, mode, type);
  Handle<Code> code = stub.GetCode(isolate);
  if (!code.is_null()) {
    if (FLAG_trace_ic) {
      PrintF("[UnaryOpIC in ");
      JavaScriptFrame::PrintTop(isolate, stdout, false, true);
      PrintF(" %s => %s #%s @ %p]\n",
             UnaryOpIC::GetName(previous_type),
             UnaryOpIC::GetName(type),
             Token::Name(op),
             static_cast<void*>(*code));
    }
    UnaryOpIC ic(isolate);
    ic.patch(*code);
  }

  Handle<JSBuiltinsObject> builtins(isolate->js_builtins_object());
  Object* builtin = NULL;  // Initialization calms down the compiler.
  switch (op) {
    case Token::SUB:
      builtin = builtins->javascript_builtin(Builtins::UNARY_MINUS);
      break;
    case Token::BIT_NOT:
      builtin = builtins->javascript_builtin(Builtins::BIT_NOT);
      break;
    default:
      UNREACHABLE();
  }

  Handle<JSFunction> builtin_function(JSFunction::cast(builtin), isolate);

  bool caught_exception;
  Handle<Object> result = Execution::Call(builtin_function, operand, 0, NULL,
                                          &caught_exception);
  if (caught_exception) {
    return Failure::Exception();
  }
  return *result;
}


static BinaryOpIC::TypeInfo TypeInfoFromValue(Handle<Object> value,
                                              Token::Value op) {
  v8::internal::TypeInfo type = v8::internal::TypeInfo::FromValue(value);
  if (type.IsSmi()) return BinaryOpIC::SMI;
  if (type.IsInteger32()) {
    if (kSmiValueSize == 32) return BinaryOpIC::SMI;
    return BinaryOpIC::INT32;
  }
  if (type.IsNumber()) return BinaryOpIC::NUMBER;
  if (type.IsString()) return BinaryOpIC::STRING;
  if (value->IsUndefined()) {
    if (op == Token::BIT_AND ||
        op == Token::BIT_OR ||
        op == Token::BIT_XOR ||
        op == Token::SAR ||
        op == Token::SHL ||
        op == Token::SHR) {
      if (kSmiValueSize == 32) return BinaryOpIC::SMI;
      return BinaryOpIC::INT32;
    }
    return BinaryOpIC::ODDBALL;
  }
  return BinaryOpIC::GENERIC;
}


static BinaryOpIC::TypeInfo InputState(BinaryOpIC::TypeInfo old_type,
                                       Handle<Object> value,
                                       Token::Value op) {
  BinaryOpIC::TypeInfo new_type = TypeInfoFromValue(value, op);
  if (old_type == BinaryOpIC::STRING) {
    if (new_type == BinaryOpIC::STRING) return new_type;
    return BinaryOpIC::GENERIC;
  }
  return Max(old_type, new_type);
}


#ifdef DEBUG
static void TraceBinaryOp(BinaryOpIC::TypeInfo left,
                          BinaryOpIC::TypeInfo right,
                          Maybe<int32_t> fixed_right_arg,
                          BinaryOpIC::TypeInfo result) {
  PrintF("%s*%s", BinaryOpIC::GetName(left), BinaryOpIC::GetName(right));
  if (fixed_right_arg.has_value) PrintF("{%d}", fixed_right_arg.value);
  PrintF("->%s", BinaryOpIC::GetName(result));
}
#endif


RUNTIME_FUNCTION(MaybeObject*, BinaryOp_Patch) {
  ASSERT(args.length() == 3);

  HandleScope scope(isolate);
  Handle<Object> left = args.at<Object>(0);
  Handle<Object> right = args.at<Object>(1);
  int key = args.smi_at(2);
  Token::Value op = BinaryOpStub::decode_op_from_minor_key(key);

  BinaryOpIC::TypeInfo previous_left, previous_right, previous_result;
  BinaryOpStub::decode_types_from_minor_key(
      key, &previous_left, &previous_right, &previous_result);

  BinaryOpIC::TypeInfo new_left = InputState(previous_left, left, op);
  BinaryOpIC::TypeInfo new_right = InputState(previous_right, right, op);
  BinaryOpIC::TypeInfo result_type = BinaryOpIC::UNINITIALIZED;

  // STRING is only used for ADD operations.
  if ((new_left == BinaryOpIC::STRING || new_right == BinaryOpIC::STRING) &&
      op != Token::ADD) {
    new_left = new_right = BinaryOpIC::GENERIC;
  }

  BinaryOpIC::TypeInfo new_overall = Max(new_left, new_right);
  BinaryOpIC::TypeInfo previous_overall = Max(previous_left, previous_right);

  Maybe<int> previous_fixed_right_arg =
      BinaryOpStub::decode_fixed_right_arg_from_minor_key(key);

  int32_t value;
  bool new_has_fixed_right_arg =
      op == Token::MOD &&
      right->ToInt32(&value) &&
      BinaryOpStub::can_encode_arg_value(value) &&
      (previous_overall == BinaryOpIC::UNINITIALIZED ||
       (previous_fixed_right_arg.has_value &&
        previous_fixed_right_arg.value == value));
  Maybe<int32_t> new_fixed_right_arg(
      new_has_fixed_right_arg, new_has_fixed_right_arg ? value : 1);

  if (previous_fixed_right_arg.has_value == new_fixed_right_arg.has_value) {
    if (new_overall == BinaryOpIC::SMI && previous_overall == BinaryOpIC::SMI) {
      if (op == Token::DIV ||
          op == Token::MUL ||
          op == Token::SHR ||
          kSmiValueSize == 32) {
        // Arithmetic on two Smi inputs has yielded a heap number.
        // That is the only way to get here from the Smi stub.
        // With 32-bit Smis, all overflows give heap numbers, but with
        // 31-bit Smis, most operations overflow to int32 results.
        result_type = BinaryOpIC::NUMBER;
      } else {
        // Other operations on SMIs that overflow yield int32s.
        result_type = BinaryOpIC::INT32;
      }
    }
    if (new_overall == BinaryOpIC::INT32 &&
        previous_overall == BinaryOpIC::INT32) {
      if (new_left == previous_left && new_right == previous_right) {
        result_type = BinaryOpIC::NUMBER;
      }
    }
  }

  BinaryOpStub stub(key, new_left, new_right, result_type, new_fixed_right_arg);
  Handle<Code> code = stub.GetCode(isolate);
  if (!code.is_null()) {
#ifdef DEBUG
    if (FLAG_trace_ic) {
      PrintF("[BinaryOpIC in ");
      JavaScriptFrame::PrintTop(isolate, stdout, false, true);
      PrintF(" ");
      TraceBinaryOp(previous_left, previous_right, previous_fixed_right_arg,
                    previous_result);
      PrintF(" => ");
      TraceBinaryOp(new_left, new_right, new_fixed_right_arg, result_type);
      PrintF(" #%s @ %p]\n", Token::Name(op), static_cast<void*>(*code));
    }
#endif
    BinaryOpIC ic(isolate);
    ic.patch(*code);

    // Activate inlined smi code.
    if (previous_overall == BinaryOpIC::UNINITIALIZED) {
      PatchInlinedSmiCode(ic.address(), ENABLE_INLINED_SMI_CHECK);
    }
  }

  Handle<JSBuiltinsObject> builtins(isolate->js_builtins_object());
  Object* builtin = NULL;  // Initialization calms down the compiler.
  switch (op) {
    case Token::ADD:
      builtin = builtins->javascript_builtin(Builtins::ADD);
      break;
    case Token::SUB:
      builtin = builtins->javascript_builtin(Builtins::SUB);
      break;
    case Token::MUL:
      builtin = builtins->javascript_builtin(Builtins::MUL);
      break;
    case Token::DIV:
      builtin = builtins->javascript_builtin(Builtins::DIV);
      break;
    case Token::MOD:
      builtin = builtins->javascript_builtin(Builtins::MOD);
      break;
    case Token::BIT_AND:
      builtin = builtins->javascript_builtin(Builtins::BIT_AND);
      break;
    case Token::BIT_OR:
      builtin = builtins->javascript_builtin(Builtins::BIT_OR);
      break;
    case Token::BIT_XOR:
      builtin = builtins->javascript_builtin(Builtins::BIT_XOR);
      break;
    case Token::SHR:
      builtin = builtins->javascript_builtin(Builtins::SHR);
      break;
    case Token::SAR:
      builtin = builtins->javascript_builtin(Builtins::SAR);
      break;
    case Token::SHL:
      builtin = builtins->javascript_builtin(Builtins::SHL);
      break;
    default:
      UNREACHABLE();
  }

  Handle<JSFunction> builtin_function(JSFunction::cast(builtin), isolate);

  bool caught_exception;
  Handle<Object> builtin_args[] = { right };
  Handle<Object> result = Execution::Call(builtin_function,
                                          left,
                                          ARRAY_SIZE(builtin_args),
                                          builtin_args,
                                          &caught_exception);
  if (caught_exception) {
    return Failure::Exception();
  }
  return *result;
}


Code* CompareIC::GetRawUninitialized(Token::Value op) {
  ICCompareStub stub(op, UNINITIALIZED, UNINITIALIZED, UNINITIALIZED);
  Code* code = NULL;
  CHECK(stub.FindCodeInCache(&code, Isolate::Current()));
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
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
  return ic.target();
}


void CompareNilIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
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

  CompareNilICStub::State old_state = stub.GetState();
  stub.Record(object);
  old_state.TraceTransition(stub.GetState());

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


MaybeObject* ToBooleanIC::ToBoolean(Handle<Object> object,
                                    Code::ExtraICState extra_ic_state) {
  ToBooleanStub stub(extra_ic_state);
  bool to_boolean_value = stub.Record(object);
  Handle<Code> code = stub.GetCode(isolate());
  set_target(*code);
  return Smi::FromInt(to_boolean_value ? 1 : 0);
}


RUNTIME_FUNCTION(MaybeObject*, ToBooleanIC_Miss) {
  ASSERT(args.length() == 1);
  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  ToBooleanIC ic(isolate);
  Code::ExtraICState ic_state = ic.target()->extended_extra_ic_state();
  return ic.ToBoolean(object, ic_state);
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
