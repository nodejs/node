// Copyright 2011 the V8 project authors. All rights reserved.
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
static char TransitionMarkFromState(IC::State state) {
  switch (state) {
    case UNINITIALIZED: return '0';
    case PREMONOMORPHIC: return 'P';
    case MONOMORPHIC: return '1';
    case MONOMORPHIC_PROTOTYPE_FAILURE: return '^';
    case MEGAMORPHIC: return 'N';

    // We never see the debugger states here, because the state is
    // computed from the original code - not the patched code. Let
    // these cases fall through to the unreachable code below.
    case DEBUG_BREAK: break;
    case DEBUG_PREPARE_STEP_IN: break;
  }
  UNREACHABLE();
  return 0;
}

void IC::TraceIC(const char* type,
                 Handle<Object> name,
                 State old_state,
                 Code* new_target) {
  if (FLAG_trace_ic) {
    State new_state = StateFrom(new_target,
                                HEAP->undefined_value(),
                                HEAP->undefined_value());
    PrintF("[%s in ", type);
    StackFrameIterator it;
    while (it.frame()->fp() != this->fp()) it.Advance();
    StackFrame* raw_frame = it.frame();
    if (raw_frame->is_internal()) {
      Isolate* isolate = new_target->GetIsolate();
      Code* apply_builtin = isolate->builtins()->builtin(
          Builtins::kFunctionApply);
      if (raw_frame->unchecked_code() == apply_builtin) {
        PrintF("apply from ");
        it.Advance();
        raw_frame = it.frame();
      }
    }
    if (raw_frame->is_java_script()) {
      JavaScriptFrame* frame = JavaScriptFrame::cast(raw_frame);
      Code* js_code = frame->unchecked_code();
      // Find the function on the stack and both the active code for the
      // function and the original code.
      JSFunction* function = JSFunction::cast(frame->function());
      function->PrintName();
      int code_offset =
          static_cast<int>(address() - js_code->instruction_start());
      PrintF("+%d", code_offset);
    } else {
      PrintF("<unknown>");
    }
    PrintF(" (%c->%c)",
           TransitionMarkFromState(old_state),
           TransitionMarkFromState(new_state));
    name->Print();
    PrintF("]\n");
  }
}
#endif


IC::IC(FrameDepth depth, Isolate* isolate) : isolate_(isolate) {
  ASSERT(isolate == Isolate::Current());
  // To improve the performance of the (much used) IC code, we unfold
  // a few levels of the stack frame iteration code. This yields a
  // ~35% speedup when running DeltaBlue with the '--nouse-ic' flag.
  const Address entry =
      Isolate::c_entry_fp(isolate->thread_local_top());
  Address* pc_address =
      reinterpret_cast<Address*>(entry + ExitFrameConstants::kCallerPCOffset);
  Address fp = Memory::Address_at(entry + ExitFrameConstants::kCallerFPOffset);
  // If there's another JavaScript frame on the stack, we need to look
  // one frame further down the stack to find the frame pointer and
  // the return address stack slot.
  if (depth == EXTRA_CALL_FRAME) {
    const int kCallerPCOffset = StandardFrameConstants::kCallerPCOffset;
    pc_address = reinterpret_cast<Address*>(fp + kCallerPCOffset);
    fp = Memory::Address_at(fp + StandardFrameConstants::kCallerFPOffset);
  }
#ifdef DEBUG
  StackFrameIterator it;
  for (int i = 0; i < depth + 1; i++) it.Advance();
  StackFrame* frame = it.frame();
  ASSERT(fp == frame->fp() && pc_address == frame->pc_address());
#endif
  fp_ = fp;
  pc_address_ = pc_address;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
Address IC::OriginalCodeAddress() {
  HandleScope scope;
  // Compute the JavaScript frame for the frame pointer of this IC
  // structure. We need this to be able to find the function
  // corresponding to the frame.
  StackFrameIterator it;
  while (it.frame()->fp() != this->fp()) it.Advance();
  JavaScriptFrame* frame = JavaScriptFrame::cast(it.frame());
  // Find the function on the stack and both the active code for the
  // function and the original code.
  JSFunction* function = JSFunction::cast(frame->function());
  Handle<SharedFunctionInfo> shared(function->shared());
  Code* code = shared->code();
  ASSERT(Debug::HasDebugInfo(shared));
  Code* original_code = Debug::GetDebugInfo(shared)->original_code();
  ASSERT(original_code->IsCode());
  // Get the address of the call site in the active code. This is the
  // place where the call to DebugBreakXXX is and where the IC
  // normally would be.
  Address addr = pc() - Assembler::kCallTargetAddressOffset;
  // Return the address in the original code. This is the place where
  // the call which has been overwritten by the DebugBreakXXX resides
  // and the place where the inline cache system should look.
  intptr_t delta =
      original_code->instruction_start() - code->instruction_start();
  return addr + delta;
}
#endif


static bool HasNormalObjectsInPrototypeChain(Isolate* isolate,
                                             LookupResult* lookup,
                                             Object* receiver) {
  Object* end = lookup->IsProperty()
      ? lookup->holder() : isolate->heap()->null_value();
  for (Object* current = receiver;
       current != end;
       current = current->GetPrototype()) {
    if (current->IsJSObject() &&
        !JSObject::cast(current)->HasFastProperties() &&
        !current->IsJSGlobalProxy() &&
        !current->IsJSGlobalObject()) {
      return true;
    }
  }

  return false;
}


static bool TryRemoveInvalidPrototypeDependentStub(Code* target,
                                                   Object* receiver,
                                                   Object* name) {
  InlineCacheHolderFlag cache_holder =
      Code::ExtractCacheHolderFromFlags(target->flags());

  if (cache_holder == OWN_MAP && !receiver->IsJSObject()) {
    // The stub was generated for JSObject but called for non-JSObject.
    // IC::GetCodeCacheHolder is not applicable.
    return false;
  } else if (cache_holder == PROTOTYPE_MAP &&
             receiver->GetPrototype()->IsNull()) {
    // IC::GetCodeCacheHolder is not applicable.
    return false;
  }
  Map* map = IC::GetCodeCacheHolder(receiver, cache_holder)->map();

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
    return true;
  }

  return false;
}


IC::State IC::StateFrom(Code* target, Object* receiver, Object* name) {
  IC::State state = target->ic_state();

  if (state != MONOMORPHIC || !name->IsString()) return state;
  if (receiver->IsUndefined() || receiver->IsNull()) return state;

  // For keyed load/store/call, the most likely cause of cache failure is
  // that the key has changed.  We do not distinguish between
  // prototype and non-prototype failures for keyed access.
  Code::Kind kind = target->kind();
  if (kind == Code::KEYED_LOAD_IC ||
      kind == Code::KEYED_STORE_IC ||
      kind == Code::KEYED_CALL_IC) {
    return MONOMORPHIC;
  }

  // Remove the target from the code cache if it became invalid
  // because of changes in the prototype chain to avoid hitting it
  // again.
  // Call stubs handle this later to allow extra IC state
  // transitions.
  if (kind != Code::CALL_IC &&
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
  return RelocInfo::NONE;
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


void IC::Clear(Address address) {
  Code* target = GetTargetAtAddress(address);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->ic_state() == DEBUG_BREAK) return;

  switch (target->kind()) {
    case Code::LOAD_IC: return LoadIC::Clear(address, target);
    case Code::KEYED_LOAD_IC:
      return KeyedLoadIC::Clear(address, target);
    case Code::STORE_IC: return StoreIC::Clear(address, target);
    case Code::KEYED_STORE_IC:
      return KeyedStoreIC::Clear(address, target);
    case Code::CALL_IC: return CallIC::Clear(address, target);
    case Code::KEYED_CALL_IC:  return KeyedCallIC::Clear(address, target);
    case Code::UNARY_OP_IC:
    case Code::BINARY_OP_IC:
    case Code::COMPARE_IC:
    case Code::TO_BOOLEAN_IC:
      // Clearing these is tricky and does not
      // make any performance difference.
      return;
    default: UNREACHABLE();
  }
}


void CallICBase::Clear(Address address, Code* target) {
  bool contextual = CallICBase::Contextual::decode(target->extra_ic_state());
  State state = target->ic_state();
  if (state == UNINITIALIZED) return;
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
  SetTargetAtAddress(address, initialize_stub());
}


void LoadIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address, initialize_stub());
}


void StoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address,
      (target->extra_ic_state() == kStrictMode)
        ? initialize_stub_strict()
        : initialize_stub());
}


void KeyedStoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address,
      (target->extra_ic_state() == kStrictMode)
        ? initialize_stub_strict()
        : initialize_stub());
}


static bool HasInterceptorGetter(JSObject* object) {
  return !object->GetNamedInterceptor()->getter()->IsUndefined();
}


static void LookupForRead(Object* object,
                          String* name,
                          LookupResult* lookup) {
  AssertNoAllocation no_gc;  // pointers must stay valid

  // Skip all the objects with named interceptors, but
  // without actual getter.
  while (true) {
    object->Lookup(name, lookup);
    // Besides normal conditions (property not found or it's not
    // an interceptor), bail out if lookup is not cacheable: we won't
    // be able to IC it anyway and regular lookup should work fine.
    if (!lookup->IsFound()
        || (lookup->type() != INTERCEPTOR)
        || !lookup->IsCacheable()) {
      return;
    }

    JSObject* holder = lookup->holder();
    if (HasInterceptorGetter(holder)) {
      return;
    }

    holder->LocalLookupRealNamedProperty(name, lookup);
    if (lookup->IsProperty()) {
      ASSERT(lookup->type() != INTERCEPTOR);
      return;
    }

    Object* proto = holder->GetPrototype();
    if (proto->IsNull()) {
      lookup->NotFound();
      return;
    }

    object = proto;
  }
}


Object* CallICBase::TryCallAsFunction(Object* object) {
  HandleScope scope(isolate());
  Handle<Object> target(object, isolate());
  Handle<Object> delegate = Execution::GetFunctionDelegate(target);

  if (delegate->IsJSFunction()) {
    // Patch the receiver and use the delegate as the function to
    // invoke. This is used for invoking objects as if they were
    // functions.
    const int argc = this->target()->arguments_count();
    StackFrameLocator locator;
    JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
    int index = frame->ComputeExpressionsCount() - (argc + 1);
    frame->SetExpression(index, *target);
  }

  return *delegate;
}


void CallICBase::ReceiverToObjectIfRequired(Handle<Object> callee,
                                            Handle<Object> object) {
  if (callee->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callee);
    if (function->shared()->strict_mode() || function->IsBuiltin()) {
      // Do not wrap receiver for strict mode functions or for builtins.
      return;
    }
  }

  // And only wrap string, number or boolean.
  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    // Change the receiver to the result of calling ToObject on it.
    const int argc = this->target()->arguments_count();
    StackFrameLocator locator;
    JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
    int index = frame->ComputeExpressionsCount() - (argc + 1);
    frame->SetExpression(index, *isolate()->factory()->ToObject(object));
  }
}


MaybeObject* CallICBase::LoadFunction(State state,
                                      Code::ExtraICState extra_ic_state,
                                      Handle<Object> object,
                                      Handle<String> name) {
  // If the object is undefined or null it's illegal to try to get any
  // of its properties; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, name);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Object* result;
    { MaybeObject* maybe_result = object->GetElement(index);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }

    if (result->IsJSFunction()) return result;

    // Try to find a suitable function delegate for the object at hand.
    result = TryCallAsFunction(result);
    if (result->IsJSFunction()) return result;

    // Otherwise, it will fail in the lookup step.
  }

  // Lookup the property in the object.
  LookupResult lookup;
  LookupForRead(*object, *name, &lookup);

  if (!lookup.IsProperty()) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    if (IsContextual(object)) {
      return ReferenceError("not_defined", name);
    }
    return TypeError("undefined_method", object, name);
  }

  // Lookup is valid: Update inline cache and stub cache.
  if (FLAG_use_ic) {
    UpdateCaches(&lookup, state, extra_ic_state, object, name);
  }

  // Get the property.
  PropertyAttributes attr;
  Object* result;
  { MaybeObject* maybe_result =
        object->GetProperty(*object, &lookup, *name, &attr);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  if (lookup.type() == INTERCEPTOR) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    if (attr == ABSENT) {
      if (IsContextual(object)) {
        return ReferenceError("not_defined", name);
      }
      return TypeError("undefined_method", object, name);
    }
  }

  ASSERT(!result->IsTheHole());

  HandleScope scope(isolate());
  // Wrap result in a handle because ReceiverToObjectIfRequired may allocate
  // new object and cause GC.
  Handle<Object> result_handle(result);
  // Make receiver an object if the callee requires it. Strict mode or builtin
  // functions do not wrap the receiver, non-strict functions and objects
  // called as functions do.
  ReceiverToObjectIfRequired(result_handle, object);

  if (result_handle->IsJSFunction()) {
#ifdef ENABLE_DEBUGGER_SUPPORT
    // Handle stepping into a function if step into is active.
    Debug* debug = isolate()->debug();
    if (debug->StepInActive()) {
      // Protect the result in a handle as the debugger can allocate and might
      // cause GC.
      Handle<JSFunction> function(JSFunction::cast(*result_handle), isolate());
      debug->HandleStepIn(function, object, fp(), false);
      return *function;
    }
#endif

    return *result_handle;
  }

  // Try to find a suitable function delegate for the object at hand.
  result_handle = Handle<Object>(TryCallAsFunction(*result_handle));
  if (result_handle->IsJSFunction()) return *result_handle;

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


MaybeObject* CallICBase::ComputeMonomorphicStub(
    LookupResult* lookup,
    State state,
    Code::ExtraICState extra_ic_state,
    Handle<Object> object,
    Handle<String> name) {
  int argc = target()->arguments_count();
  MaybeObject* maybe_code = NULL;
  switch (lookup->type()) {
    case FIELD: {
      int index = lookup->GetFieldIndex();
      maybe_code = isolate()->stub_cache()->ComputeCallField(argc,
                                                             kind_,
                                                             extra_ic_state,
                                                             *name,
                                                             *object,
                                                             lookup->holder(),
                                                             index);
      break;
    }
    case CONSTANT_FUNCTION: {
      // Get the constant function and compute the code stub for this
      // call; used for rewriting to monomorphic state and making sure
      // that the code stub is in the stub cache.
      JSFunction* function = lookup->GetConstantFunction();
      maybe_code =
          isolate()->stub_cache()->ComputeCallConstant(argc,
                                                       kind_,
                                                       extra_ic_state,
                                                       *name,
                                                       *object,
                                                       lookup->holder(),
                                                       function);
      break;
    }
    case NORMAL: {
      if (!object->IsJSObject()) return NULL;
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);

      if (lookup->holder()->IsGlobalObject()) {
        GlobalObject* global = GlobalObject::cast(lookup->holder());
        JSGlobalPropertyCell* cell =
            JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
        if (!cell->value()->IsJSFunction()) return NULL;
        JSFunction* function = JSFunction::cast(cell->value());
        maybe_code = isolate()->stub_cache()->ComputeCallGlobal(argc,
                                                                kind_,
                                                                extra_ic_state,
                                                                *name,
                                                                *receiver,
                                                                global,
                                                                cell,
                                                                function);
      } else {
        // There is only one shared stub for calling normalized
        // properties. It does not traverse the prototype chain, so the
        // property must be found in the receiver for the stub to be
        // applicable.
        if (lookup->holder() != *receiver) return NULL;
        maybe_code = isolate()->stub_cache()->ComputeCallNormal(argc,
                                                                kind_,
                                                                extra_ic_state,
                                                                *name,
                                                                *receiver);
      }
      break;
    }
    case INTERCEPTOR: {
      ASSERT(HasInterceptorGetter(lookup->holder()));
      maybe_code = isolate()->stub_cache()->ComputeCallInterceptor(
          argc,
          kind_,
          extra_ic_state,
          *name,
          *object,
          lookup->holder());
      break;
    }
    default:
      maybe_code = NULL;
      break;
  }
  return maybe_code;
}


void CallICBase::UpdateCaches(LookupResult* lookup,
                              State state,
                              Code::ExtraICState extra_ic_state,
                              Handle<Object> object,
                              Handle<String> name) {
  // Bail out if we didn't find a result.
  if (!lookup->IsProperty() || !lookup->IsCacheable()) return;

  if (lookup->holder() != *object &&
      HasNormalObjectsInPrototypeChain(
          isolate(), lookup, object->GetPrototype())) {
    // Suppress optimization for prototype chains with slow properties objects
    // in the middle.
    return;
  }

  // Compute the number of arguments.
  int argc = target()->arguments_count();
  MaybeObject* maybe_code = NULL;
  bool had_proto_failure = false;
  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    maybe_code =
        isolate()->stub_cache()->ComputeCallPreMonomorphic(argc,
                                                           kind_,
                                                           extra_ic_state);
  } else if (state == MONOMORPHIC) {
    if (kind_ == Code::CALL_IC &&
        TryUpdateExtraICState(lookup, object, &extra_ic_state)) {
      maybe_code = ComputeMonomorphicStub(lookup,
                                          state,
                                          extra_ic_state,
                                          object,
                                          name);
    } else if (kind_ == Code::CALL_IC &&
               TryRemoveInvalidPrototypeDependentStub(target(),
                                                      *object,
                                                      *name)) {
      had_proto_failure = true;
      maybe_code = ComputeMonomorphicStub(lookup,
                                          state,
                                          extra_ic_state,
                                          object,
                                          name);
    } else {
      maybe_code =
          isolate()->stub_cache()->ComputeCallMegamorphic(argc,
                                                          kind_,
                                                          extra_ic_state);
    }
  } else {
    maybe_code = ComputeMonomorphicStub(lookup,
                                        state,
                                        extra_ic_state,
                                        object,
                                        name);
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  Object* code;
  if (maybe_code == NULL || !maybe_code->ToObject(&code)) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED ||
      state == PREMONOMORPHIC ||
      state == MONOMORPHIC ||
      state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  } else if (state == MEGAMORPHIC) {
    // Cache code holding map should be consistent with
    // GenerateMonomorphicCacheProbe. It is not the map which holds the stub.
    Map* map = JSObject::cast(object->IsJSObject() ? *object :
                              object->GetPrototype())->map();

    // Update the stub cache.
    isolate()->stub_cache()->Set(*name, map, Code::cast(code));
  }

  USE(had_proto_failure);
#ifdef DEBUG
  if (had_proto_failure) state = MONOMORPHIC_PROTOTYPE_FAILURE;
  TraceIC(kind_ == Code::CALL_IC ? "CallIC" : "KeyedCallIC",
          name, state, target());
#endif
}


MaybeObject* KeyedCallIC::LoadFunction(State state,
                                       Handle<Object> object,
                                       Handle<Object> key) {
  if (key->IsSymbol()) {
    return CallICBase::LoadFunction(state,
                                    Code::kNoExtraICState,
                                    object,
                                    Handle<String>::cast(key));
  }

  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_call", object, key);
  }

  if (FLAG_use_ic && state != MEGAMORPHIC && object->IsHeapObject()) {
    int argc = target()->arguments_count();
    Heap* heap = Handle<HeapObject>::cast(object)->GetHeap();
    Map* map = heap->non_strict_arguments_elements_map();
    if (object->IsJSObject() &&
        Handle<JSObject>::cast(object)->elements()->map() == map) {
      MaybeObject* maybe_code = isolate()->stub_cache()->ComputeCallArguments(
          argc, Code::KEYED_CALL_IC);
      Object* code;
      if (maybe_code->ToObject(&code)) {
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedCallIC", key, state, target());
#endif
      }
    } else if (FLAG_use_ic && state != MEGAMORPHIC &&
               !object->IsAccessCheckNeeded()) {
      MaybeObject* maybe_code = isolate()->stub_cache()->ComputeCallMegamorphic(
          argc, Code::KEYED_CALL_IC, Code::kNoExtraICState);
      Object* code;
      if (maybe_code->ToObject(&code)) {
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedCallIC", key, state, target());
#endif
      }
    }
  }

  HandleScope scope(isolate());
  Handle<Object> result = GetProperty(object, key);
  RETURN_IF_EMPTY_HANDLE(isolate(), result);

  // Make receiver an object if the callee requires it. Strict mode or builtin
  // functions do not wrap the receiver, non-strict functions and objects
  // called as functions do.
  ReceiverToObjectIfRequired(result, object);

  if (result->IsJSFunction()) return *result;
  result = Handle<Object>(TryCallAsFunction(*result));
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
        name->Equals(isolate()->heap()->length_symbol())) {
      AssertNoAllocation no_allocation;
      Code* stub = NULL;
      if (state == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state == PREMONOMORPHIC) {
        if (object->IsString()) {
          stub = isolate()->builtins()->builtin(
              Builtins::kLoadIC_StringLength);
        } else {
          stub = isolate()->builtins()->builtin(
              Builtins::kLoadIC_StringWrapperLength);
        }
      } else if (state == MONOMORPHIC && object->IsStringWrapper()) {
        stub = isolate()->builtins()->builtin(
            Builtins::kLoadIC_StringWrapperLength);
      } else if (state != MEGAMORPHIC) {
        stub = megamorphic_stub();
      }
      if (stub != NULL) {
        set_target(stub);
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#length /string]\n");
#endif
      }
      // Get the string if we have a string wrapper object.
      if (object->IsJSValue()) {
        return Smi::FromInt(
            String::cast(Handle<JSValue>::cast(object)->value())->length());
      }
      return Smi::FromInt(String::cast(*object)->length());
    }

    // Use specialized code for getting the length of arrays.
    if (object->IsJSArray() &&
        name->Equals(isolate()->heap()->length_symbol())) {
      AssertNoAllocation no_allocation;
      Code* stub = NULL;
      if (state == UNINITIALIZED) {
        stub = pre_monomorphic_stub();
      } else if (state == PREMONOMORPHIC) {
        stub = isolate()->builtins()->builtin(
            Builtins::kLoadIC_ArrayLength);
      } else if (state != MEGAMORPHIC) {
        stub = megamorphic_stub();
      }
      if (stub != NULL) {
        set_target(stub);
#ifdef DEBUG
        if (FLAG_trace_ic) PrintF("[LoadIC : +#length /array]\n");
#endif
      }
      return JSArray::cast(*object)->length();
    }

    // Use specialized code for getting prototype of functions.
    if (object->IsJSFunction() &&
        name->Equals(isolate()->heap()->prototype_symbol()) &&
        JSFunction::cast(*object)->should_have_prototype()) {
      { AssertNoAllocation no_allocation;
        Code* stub = NULL;
        if (state == UNINITIALIZED) {
          stub = pre_monomorphic_stub();
        } else if (state == PREMONOMORPHIC) {
          stub = isolate()->builtins()->builtin(
              Builtins::kLoadIC_FunctionPrototype);
        } else if (state != MEGAMORPHIC) {
          stub = megamorphic_stub();
        }
        if (stub != NULL) {
          set_target(stub);
#ifdef DEBUG
          if (FLAG_trace_ic) PrintF("[LoadIC : +#prototype /function]\n");
#endif
        }
      }
      return Accessors::FunctionGetPrototype(*object, 0);
    }
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) return object->GetElement(index);

  // Named lookup in the object.
  LookupResult lookup;
  LookupForRead(*object, *name, &lookup);

  // If we did not find a property, check if we need to throw an exception.
  if (!lookup.IsProperty()) {
    if (IsContextual(object)) {
      return ReferenceError("not_defined", name);
    }
    LOG(isolate(), SuspectReadEvent(*name, *object));
  }

  // Update inline cache and stub cache.
  if (FLAG_use_ic) {
    UpdateCaches(&lookup, state, object, name);
  }

  PropertyAttributes attr;
  if (lookup.IsProperty() &&
      (lookup.type() == INTERCEPTOR || lookup.type() == HANDLER)) {
    // Get the property.
    Object* result;
    { MaybeObject* maybe_result =
          object->GetProperty(*object, &lookup, *name, &attr);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    // If the property is not present, check if we need to throw an
    // exception.
    if (attr == ABSENT && IsContextual(object)) {
      return ReferenceError("not_defined", name);
    }
    return result;
  }

  // Get the property.
  return object->GetProperty(*object, &lookup, *name, &attr);
}


void LoadIC::UpdateCaches(LookupResult* lookup,
                          State state,
                          Handle<Object> object,
                          Handle<String> name) {
  // Bail out if the result is not cacheable.
  if (!lookup->IsCacheable()) return;

  // Loading properties from values is not common, so don't try to
  // deal with non-JS objects here.
  if (!object->IsJSObject()) return;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  if (HasNormalObjectsInPrototypeChain(isolate(), lookup, *object)) return;

  // Compute the code stub for this load.
  MaybeObject* maybe_code = NULL;
  Object* code;
  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    maybe_code = pre_monomorphic_stub();
  } else if (!lookup->IsProperty()) {
    // Nonexistent property. The result is undefined.
    maybe_code = isolate()->stub_cache()->ComputeLoadNonexistent(*name,
                                                                 *receiver);
  } else {
    // Compute monomorphic stub.
    switch (lookup->type()) {
      case FIELD: {
        maybe_code = isolate()->stub_cache()->ComputeLoadField(
            *name,
            *receiver,
            lookup->holder(),
            lookup->GetFieldIndex());
        break;
      }
      case CONSTANT_FUNCTION: {
        Object* constant = lookup->GetConstantFunction();
        maybe_code = isolate()->stub_cache()->ComputeLoadConstant(
            *name, *receiver, lookup->holder(), constant);
        break;
      }
      case NORMAL: {
        if (lookup->holder()->IsGlobalObject()) {
          GlobalObject* global = GlobalObject::cast(lookup->holder());
          JSGlobalPropertyCell* cell =
              JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
          maybe_code = isolate()->stub_cache()->ComputeLoadGlobal(*name,
                                                    *receiver,
                                                    global,
                                                    cell,
                                                    lookup->IsDontDelete());
        } else {
          // There is only one shared stub for loading normalized
          // properties. It does not traverse the prototype chain, so the
          // property must be found in the receiver for the stub to be
          // applicable.
          if (lookup->holder() != *receiver) return;
          maybe_code = isolate()->stub_cache()->ComputeLoadNormal();
        }
        break;
      }
      case CALLBACKS: {
        if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
        AccessorInfo* callback =
            AccessorInfo::cast(lookup->GetCallbackObject());
        if (v8::ToCData<Address>(callback->getter()) == 0) return;
        maybe_code = isolate()->stub_cache()->ComputeLoadCallback(
            *name, *receiver, lookup->holder(), callback);
        break;
      }
      case INTERCEPTOR: {
        ASSERT(HasInterceptorGetter(lookup->holder()));
        maybe_code = isolate()->stub_cache()->ComputeLoadInterceptor(
            *name, *receiver, lookup->holder());
        break;
      }
      default:
        return;
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (maybe_code == NULL || !maybe_code->ToObject(&code)) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED || state == PREMONOMORPHIC ||
      state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    set_target(megamorphic_stub());
  } else if (state == MEGAMORPHIC) {
    // Cache code holding map should be consistent with
    // GenerateMonomorphicCacheProbe.
    Map* map = JSObject::cast(object->IsJSObject() ? *object :
                              object->GetPrototype())->map();

    isolate()->stub_cache()->Set(*name, map, Code::cast(code));
  }

#ifdef DEBUG
  TraceIC("LoadIC", name, state, target());
#endif
}


MaybeObject* KeyedLoadIC::GetElementStubWithoutMapCheck(
    bool is_js_array,
    ElementsKind elements_kind) {
  return KeyedLoadElementStub(elements_kind).TryGetCode();
}


MaybeObject* KeyedLoadIC::ConstructMegamorphicStub(
    MapList* receiver_maps,
    CodeList* targets,
    StrictModeFlag strict_mode) {
  Object* object;
  KeyedLoadStubCompiler compiler;
  MaybeObject* maybe_code = compiler.CompileLoadMegamorphic(receiver_maps,
                                                            targets);
  if (!maybe_code->ToObject(&object)) return maybe_code;
  isolate()->counters()->keyed_load_polymorphic_stubs()->Increment();
  PROFILE(isolate(), CodeCreateEvent(
      Logger::KEYED_LOAD_MEGAMORPHIC_IC_TAG,
      Code::cast(object), 0));
  return object;
}


MaybeObject* KeyedLoadIC::Load(State state,
                               Handle<Object> object,
                               Handle<Object> key,
                               bool force_generic_stub) {
  // Check for values that can be converted into a symbol.
  // TODO(1295): Remove this code.
  HandleScope scope(isolate());
  if (key->IsHeapNumber() &&
      isnan(HeapNumber::cast(*key)->value())) {
    key = isolate()->factory()->nan_symbol();
  } else if (key->IsUndefined()) {
    key = isolate()->factory()->undefined_symbol();
  }

  if (key->IsSymbol()) {
    Handle<String> name = Handle<String>::cast(key);

    // If the object is undefined or null it's illegal to try to get any
    // of its properties; throw a TypeError in that case.
    if (object->IsUndefined() || object->IsNull()) {
      return TypeError("non_object_property_load", object, name);
    }

    if (FLAG_use_ic) {
      // TODO(1073): don't ignore the current stub state.

      // Use specialized code for getting the length of strings.
      if (object->IsString() &&
          name->Equals(isolate()->heap()->length_symbol())) {
        Handle<String> string = Handle<String>::cast(object);
        Object* code = NULL;
        { MaybeObject* maybe_code =
              isolate()->stub_cache()->ComputeKeyedLoadStringLength(*name,
                                                                    *string);
          if (!maybe_code->ToObject(&code)) return maybe_code;
        }
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedLoadIC", name, state, target());
#endif  // DEBUG
        return Smi::FromInt(string->length());
      }

      // Use specialized code for getting the length of arrays.
      if (object->IsJSArray() &&
          name->Equals(isolate()->heap()->length_symbol())) {
        Handle<JSArray> array = Handle<JSArray>::cast(object);
        Object* code;
        { MaybeObject* maybe_code =
              isolate()->stub_cache()->ComputeKeyedLoadArrayLength(*name,
                                                                   *array);
          if (!maybe_code->ToObject(&code)) return maybe_code;
        }
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedLoadIC", name, state, target());
#endif  // DEBUG
        return JSArray::cast(*object)->length();
      }

      // Use specialized code for getting prototype of functions.
      if (object->IsJSFunction() &&
          name->Equals(isolate()->heap()->prototype_symbol()) &&
        JSFunction::cast(*object)->should_have_prototype()) {
        Handle<JSFunction> function = Handle<JSFunction>::cast(object);
        Object* code;
        { MaybeObject* maybe_code =
              isolate()->stub_cache()->ComputeKeyedLoadFunctionPrototype(
                  *name, *function);
          if (!maybe_code->ToObject(&code)) return maybe_code;
        }
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedLoadIC", name, state, target());
#endif  // DEBUG
        return Accessors::FunctionGetPrototype(*object, 0);
      }
    }

    // Check if the name is trivially convertible to an index and get
    // the element or char if so.
    uint32_t index = 0;
    if (name->AsArrayIndex(&index)) {
      HandleScope scope(isolate());
      // Rewrite to the generic keyed load stub.
      if (FLAG_use_ic) set_target(generic_stub());
      return Runtime::GetElementOrCharAt(isolate(), object, index);
    }

    // Named lookup.
    LookupResult lookup;
    LookupForRead(*object, *name, &lookup);

    // If we did not find a property, check if we need to throw an exception.
    if (!lookup.IsProperty() && IsContextual(object)) {
      return ReferenceError("not_defined", name);
    }

    if (FLAG_use_ic) {
      UpdateCaches(&lookup, state, object, name);
    }

    PropertyAttributes attr;
    if (lookup.IsProperty() && lookup.type() == INTERCEPTOR) {
      // Get the property.
      Object* result;
      { MaybeObject* maybe_result =
            object->GetProperty(*object, &lookup, *name, &attr);
        if (!maybe_result->ToObject(&result)) return maybe_result;
      }
      // If the property is not present, check if we need to throw an
      // exception.
      if (attr == ABSENT && IsContextual(object)) {
        return ReferenceError("not_defined", name);
      }
      return result;
    }

    return object->GetProperty(*object, &lookup, *name, &attr);
  }

  // Do not use ICs for objects that require access checks (including
  // the global object).
  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded();

  if (use_ic) {
    Code* stub = generic_stub();
    if (!force_generic_stub) {
      if (object->IsString() && key->IsNumber()) {
        if (state == UNINITIALIZED) {
          stub = string_stub();
        }
      } else if (object->IsJSObject()) {
        JSObject* receiver = JSObject::cast(*object);
        Heap* heap = Handle<JSObject>::cast(object)->GetHeap();
        Map* elements_map = Handle<JSObject>::cast(object)->elements()->map();
        if (elements_map == heap->non_strict_arguments_elements_map()) {
          stub = non_strict_arguments_stub();
        } else if (receiver->HasIndexedInterceptor()) {
          stub = indexed_interceptor_stub();
        } else if (key->IsSmi() && (target() != non_strict_arguments_stub())) {
          MaybeObject* maybe_stub = ComputeStub(receiver,
                                                false,
                                                kNonStrictMode,
                                                stub);
          stub = maybe_stub->IsFailure() ?
              NULL : Code::cast(maybe_stub->ToObjectUnchecked());
        }
      }
    }
    if (stub != NULL) set_target(stub);
  }

#ifdef DEBUG
  TraceIC("KeyedLoadIC", key, state, target());
#endif  // DEBUG

  // Get the property.
  return Runtime::GetObjectProperty(isolate(), object, key);
}


void KeyedLoadIC::UpdateCaches(LookupResult* lookup, State state,
                               Handle<Object> object, Handle<String> name) {
  // Bail out if we didn't find a result.
  if (!lookup->IsProperty() || !lookup->IsCacheable()) return;

  if (!object->IsJSObject()) return;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  if (HasNormalObjectsInPrototypeChain(isolate(), lookup, *object)) return;

  // Compute the code stub for this load.
  MaybeObject* maybe_code = NULL;
  Object* code;

  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    maybe_code = pre_monomorphic_stub();
  } else {
    // Compute a monomorphic stub.
    switch (lookup->type()) {
      case FIELD: {
        maybe_code = isolate()->stub_cache()->ComputeKeyedLoadField(
            *name, *receiver, lookup->holder(), lookup->GetFieldIndex());
        break;
      }
      case CONSTANT_FUNCTION: {
        Object* constant = lookup->GetConstantFunction();
        maybe_code = isolate()->stub_cache()->ComputeKeyedLoadConstant(
            *name, *receiver, lookup->holder(), constant);
        break;
      }
      case CALLBACKS: {
        if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
        AccessorInfo* callback =
            AccessorInfo::cast(lookup->GetCallbackObject());
        if (v8::ToCData<Address>(callback->getter()) == 0) return;
        maybe_code = isolate()->stub_cache()->ComputeKeyedLoadCallback(
            *name, *receiver, lookup->holder(), callback);
        break;
      }
      case INTERCEPTOR: {
        ASSERT(HasInterceptorGetter(lookup->holder()));
        maybe_code = isolate()->stub_cache()->ComputeKeyedLoadInterceptor(
            *name, *receiver, lookup->holder());
        break;
      }
      default: {
        // Always rewrite to the generic case so that we do not
        // repeatedly try to rewrite.
        maybe_code = generic_stub();
        break;
      }
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (maybe_code == NULL || !maybe_code->ToObject(&code)) return;

  // Patch the call site depending on the state of the cache.  Make
  // sure to always rewrite from monomorphic to megamorphic.
  ASSERT(state != MONOMORPHIC_PROTOTYPE_FAILURE);
  if (state == UNINITIALIZED || state == PREMONOMORPHIC) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    set_target(megamorphic_stub());
  }

#ifdef DEBUG
  TraceIC("KeyedLoadIC", name, state, target());
#endif
}


static bool StoreICableLookup(LookupResult* lookup) {
  // Bail out if we didn't find a result.
  if (!lookup->IsPropertyOrTransition() || !lookup->IsCacheable()) return false;

  // If the property is read-only, we leave the IC in its current
  // state.
  if (lookup->IsReadOnly()) return false;

  return true;
}


static bool LookupForWrite(JSReceiver* receiver,
                           String* name,
                           LookupResult* lookup) {
  receiver->LocalLookup(name, lookup);
  if (!StoreICableLookup(lookup)) {
    return false;
  }

  if (lookup->type() == INTERCEPTOR) {
    JSObject* object = JSObject::cast(receiver);
    if (object->GetNamedInterceptor()->setter()->IsUndefined()) {
      object->LocalLookupRealNamedProperty(name, lookup);
      return StoreICableLookup(lookup);
    }
  }

  return true;
}


MaybeObject* StoreIC::Store(State state,
                            StrictModeFlag strict_mode,
                            Handle<Object> object,
                            Handle<String> name,
                            Handle<Object> value) {
  // If the object is undefined or null it's illegal to try to set any
  // properties on it; throw a TypeError in that case.
  if (object->IsUndefined() || object->IsNull()) {
    return TypeError("non_object_property_store", object, name);
  }

  if (!object->IsJSReceiver()) {
    // The length property of string values is read-only. Throw in strict mode.
    if (strict_mode == kStrictMode && object->IsString() &&
        name->Equals(isolate()->heap()->length_symbol())) {
      return TypeError("strict_read_only_property", object, name);
    }
    // Ignore stores where the receiver is not a JSObject.
    return *value;
  }

  // Handle proxies.
  if (object->IsJSProxy()) {
    return JSReceiver::cast(*object)->
        SetProperty(*name, *value, NONE, strict_mode);
  }

  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  // Check if the given name is an array index.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    HandleScope scope(isolate());
    Handle<Object> result = SetElement(receiver, index, value, strict_mode);
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  // Use specialized code for setting the length of arrays.
  if (receiver->IsJSArray()
      && name->Equals(isolate()->heap()->length_symbol())
      && JSArray::cast(*receiver)->AllowsSetElementsLength()) {
#ifdef DEBUG
    if (FLAG_trace_ic) PrintF("[StoreIC : +#length /array]\n");
#endif
    Builtins::Name target = (strict_mode == kStrictMode)
        ? Builtins::kStoreIC_ArrayLength_Strict
        : Builtins::kStoreIC_ArrayLength;
    set_target(isolate()->builtins()->builtin(target));
    return receiver->SetProperty(*name, *value, NONE, strict_mode);
  }

  // Lookup the property locally in the receiver.
  if (FLAG_use_ic && !receiver->IsJSGlobalProxy()) {
    LookupResult lookup;

    if (LookupForWrite(*receiver, *name, &lookup)) {
      // Generate a stub for this store.
      UpdateCaches(&lookup, state, strict_mode, receiver, name, value);
    } else {
      // Strict mode doesn't allow setting non-existent global property
      // or an assignment to a read only property.
      if (strict_mode == kStrictMode) {
        if (lookup.IsFound() && lookup.IsReadOnly()) {
          return TypeError("strict_read_only_property", object, name);
        } else if (IsContextual(object)) {
          return ReferenceError("not_defined", name);
        }
      }
    }
  }

  if (receiver->IsJSGlobalProxy()) {
    // Generate a generic stub that goes to the runtime when we see a global
    // proxy as receiver.
    Code* stub = (strict_mode == kStrictMode)
        ? global_proxy_stub_strict()
        : global_proxy_stub();
    if (target() != stub) {
      set_target(stub);
#ifdef DEBUG
      TraceIC("StoreIC", name, state, target());
#endif
    }
  }

  // Set the property.
  return receiver->SetProperty(*name, *value, NONE, strict_mode);
}


void StoreIC::UpdateCaches(LookupResult* lookup,
                           State state,
                           StrictModeFlag strict_mode,
                           Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value) {
  // Skip JSGlobalProxy.
  ASSERT(!receiver->IsJSGlobalProxy());

  ASSERT(StoreICableLookup(lookup));

  // If the property has a non-field type allowing map transitions
  // where there is extra room in the object, we leave the IC in its
  // current state.
  PropertyType type = lookup->type();

  // Compute the code stub for this store; used for rewriting to
  // monomorphic state and making sure that the code stub is in the
  // stub cache.
  MaybeObject* maybe_code = NULL;
  Object* code = NULL;
  switch (type) {
    case FIELD: {
      maybe_code = isolate()->stub_cache()->ComputeStoreField(
          *name, *receiver, lookup->GetFieldIndex(), NULL, strict_mode);
      break;
    }
    case MAP_TRANSITION: {
      if (lookup->GetAttributes() != NONE) return;
      HandleScope scope(isolate());
      ASSERT(type == MAP_TRANSITION);
      Handle<Map> transition(lookup->GetTransitionMap());
      int index = transition->PropertyIndexFor(*name);
      maybe_code = isolate()->stub_cache()->ComputeStoreField(
          *name, *receiver, index, *transition, strict_mode);
      break;
    }
    case NORMAL: {
      if (receiver->IsGlobalObject()) {
        // The stub generated for the global object picks the value directly
        // from the property cell. So the property must be directly on the
        // global object.
        Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
        JSGlobalPropertyCell* cell =
            JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
        maybe_code = isolate()->stub_cache()->ComputeStoreGlobal(
            *name, *global, cell, strict_mode);
      } else {
        if (lookup->holder() != *receiver) return;
        maybe_code = isolate()->stub_cache()->ComputeStoreNormal(strict_mode);
      }
      break;
    }
    case CALLBACKS: {
      if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
      AccessorInfo* callback = AccessorInfo::cast(lookup->GetCallbackObject());
      if (v8::ToCData<Address>(callback->setter()) == 0) return;
      maybe_code = isolate()->stub_cache()->ComputeStoreCallback(
          *name, *receiver, callback, strict_mode);
      break;
    }
    case INTERCEPTOR: {
      ASSERT(!receiver->GetNamedInterceptor()->setter()->IsUndefined());
      maybe_code = isolate()->stub_cache()->ComputeStoreInterceptor(
          *name, *receiver, strict_mode);
      break;
    }
    default:
      return;
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (maybe_code == NULL || !maybe_code->ToObject(&code)) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED || state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    // Only move to megamorphic if the target changes.
    if (target() != Code::cast(code)) {
      set_target((strict_mode == kStrictMode)
                   ? megamorphic_stub_strict()
                   : megamorphic_stub());
    }
  } else if (state == MEGAMORPHIC) {
    // Update the stub cache.
    isolate()->stub_cache()->Set(*name,
                                 receiver->map(),
                                 Code::cast(code));
  }

#ifdef DEBUG
  TraceIC("StoreIC", name, state, target());
#endif
}


static bool AddOneReceiverMapIfMissing(MapList* receiver_maps,
                                       Map* new_receiver_map) {
  for (int current = 0; current < receiver_maps->length(); ++current) {
    if (receiver_maps->at(current) == new_receiver_map) {
      return false;
    }
  }
  receiver_maps->Add(new_receiver_map);
  return true;
}


void KeyedIC::GetReceiverMapsForStub(Code* stub, MapList* result) {
  ASSERT(stub->is_inline_cache_stub());
  if (stub == string_stub()) {
    return result->Add(isolate()->heap()->string_map());
  } else if (stub->is_keyed_load_stub() || stub->is_keyed_store_stub()) {
    if (stub->ic_state() == MONOMORPHIC) {
      result->Add(Map::cast(stub->FindFirstMap()));
    } else {
      ASSERT(stub->ic_state() == MEGAMORPHIC);
      AssertNoAllocation no_allocation;
      int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
      for (RelocIterator it(stub, mask); !it.done(); it.next()) {
        RelocInfo* info = it.rinfo();
        Object* object = info->target_object();
        ASSERT(object->IsMap());
        result->Add(Map::cast(object));
      }
    }
  }
}


MaybeObject* KeyedIC::ComputeStub(JSObject* receiver,
                                  bool is_store,
                                  StrictModeFlag strict_mode,
                                  Code* generic_stub) {
  State ic_state = target()->ic_state();
  if (ic_state == UNINITIALIZED || ic_state == PREMONOMORPHIC) {
    Code* monomorphic_stub;
    MaybeObject* maybe_stub = ComputeMonomorphicStub(receiver,
                                                     is_store,
                                                     strict_mode,
                                                     generic_stub);
    if (!maybe_stub->To(&monomorphic_stub)) return maybe_stub;

    return monomorphic_stub;
  }
  ASSERT(target() != generic_stub);

  // Don't handle megamorphic property accesses for INTERCEPTORS or CALLBACKS
  // via megamorphic stubs, since they don't have a map in their relocation info
  // and so the stubs can't be harvested for the object needed for a map check.
  if (target()->type() != NORMAL) {
    return generic_stub;
  }

  // Determine the list of receiver maps that this call site has seen,
  // adding the map that was just encountered.
  MapList target_receiver_maps;
  GetReceiverMapsForStub(target(), &target_receiver_maps);
  if (!AddOneReceiverMapIfMissing(&target_receiver_maps, receiver->map())) {
    // If the miss wasn't due to an unseen map, a MEGAMORPHIC stub
    // won't help, use the generic stub.
    return generic_stub;
  }

  // If the maximum number of receiver maps has been exceeded, use the generic
  // version of the IC.
  if (target_receiver_maps.length() > kMaxKeyedPolymorphism) {
    return generic_stub;
  }

  PolymorphicCodeCache* cache = isolate()->heap()->polymorphic_code_cache();
  Code::Flags flags = Code::ComputeFlags(this->kind(),
                                         MEGAMORPHIC,
                                         strict_mode);
  Object* maybe_cached_stub = cache->Lookup(&target_receiver_maps, flags);
  // If there is a cached stub, use it.
  if (!maybe_cached_stub->IsUndefined()) {
    ASSERT(maybe_cached_stub->IsCode());
    return Code::cast(maybe_cached_stub);
  }
  // Collect MONOMORPHIC stubs for all target_receiver_maps.
  CodeList handler_ics(target_receiver_maps.length());
  for (int i = 0; i < target_receiver_maps.length(); ++i) {
    Map* receiver_map(target_receiver_maps.at(i));
    MaybeObject* maybe_cached_stub = ComputeMonomorphicStubWithoutMapCheck(
        receiver_map, strict_mode);
    Code* cached_stub;
    if (!maybe_cached_stub->To(&cached_stub)) return maybe_cached_stub;
    handler_ics.Add(cached_stub);
  }
  // Build the MEGAMORPHIC stub.
  Code* stub;
  MaybeObject* maybe_stub = ConstructMegamorphicStub(&target_receiver_maps,
                                                     &handler_ics,
                                                     strict_mode);
  if (!maybe_stub->To(&stub)) return maybe_stub;
  MaybeObject* maybe_update = cache->Update(&target_receiver_maps, flags, stub);
  if (maybe_update->IsFailure()) return maybe_update;
  return stub;
}


MaybeObject* KeyedIC::ComputeMonomorphicStubWithoutMapCheck(
    Map* receiver_map,
    StrictModeFlag strict_mode) {
  if ((receiver_map->instance_type() & kNotStringTag) == 0) {
    ASSERT(string_stub() != NULL);
    return string_stub();
  } else {
    ASSERT(receiver_map->has_dictionary_elements() ||
           receiver_map->has_fast_elements() ||
           receiver_map->has_fast_double_elements() ||
           receiver_map->has_external_array_elements());
    bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
    return GetElementStubWithoutMapCheck(is_js_array,
                                         receiver_map->elements_kind());
  }
}


MaybeObject* KeyedIC::ComputeMonomorphicStub(JSObject* receiver,
                                             bool is_store,
                                             StrictModeFlag strict_mode,
                                             Code* generic_stub) {
  Code* result = NULL;
  if (receiver->HasFastElements() ||
      receiver->HasExternalArrayElements() ||
      receiver->HasFastDoubleElements() ||
      receiver->HasDictionaryElements()) {
    MaybeObject* maybe_stub =
        isolate()->stub_cache()->ComputeKeyedLoadOrStoreElement(
            receiver, is_store, strict_mode);
    if (!maybe_stub->To(&result)) return maybe_stub;
  } else {
    result = generic_stub;
  }
  return result;
}


MaybeObject* KeyedStoreIC::GetElementStubWithoutMapCheck(
    bool is_js_array,
    ElementsKind elements_kind) {
  return KeyedStoreElementStub(is_js_array, elements_kind).TryGetCode();
}


MaybeObject* KeyedStoreIC::ConstructMegamorphicStub(
    MapList* receiver_maps,
    CodeList* targets,
    StrictModeFlag strict_mode) {
  Object* object;
  KeyedStoreStubCompiler compiler(strict_mode);
  MaybeObject* maybe_code = compiler.CompileStoreMegamorphic(receiver_maps,
                                                             targets);
  if (!maybe_code->ToObject(&object)) return maybe_code;
  isolate()->counters()->keyed_store_polymorphic_stubs()->Increment();
  PROFILE(isolate(), CodeCreateEvent(
      Logger::KEYED_STORE_MEGAMORPHIC_IC_TAG,
      Code::cast(object), 0));
  return object;
}


MaybeObject* KeyedStoreIC::Store(State state,
                                 StrictModeFlag strict_mode,
                                 Handle<Object> object,
                                 Handle<Object> key,
                                 Handle<Object> value,
                                 bool force_generic) {
  if (key->IsSymbol()) {
    Handle<String> name = Handle<String>::cast(key);

    // If the object is undefined or null it's illegal to try to set any
    // properties on it; throw a TypeError in that case.
    if (object->IsUndefined() || object->IsNull()) {
      return TypeError("non_object_property_store", object, name);
    }

    // Ignore stores where the receiver is not a JSObject.
    if (!object->IsJSObject()) return *value;
    Handle<JSObject> receiver = Handle<JSObject>::cast(object);

    // Check if the given name is an array index.
    uint32_t index;
    if (name->AsArrayIndex(&index)) {
      HandleScope scope(isolate());
      Handle<Object> result = SetElement(receiver, index, value, strict_mode);
      if (result.is_null()) return Failure::Exception();
      return *value;
    }

    // Lookup the property locally in the receiver.
    LookupResult lookup;
    receiver->LocalLookup(*name, &lookup);

    // Update inline cache and stub cache.
    if (FLAG_use_ic) {
      UpdateCaches(&lookup, state, strict_mode, receiver, name, value);
    }

    // Set the property.
    return receiver->SetProperty(*name, *value, NONE, strict_mode);
  }

  // Do not use ICs for objects that require access checks (including
  // the global object).
  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded();
  ASSERT(!(use_ic && object->IsJSGlobalProxy()));

  if (use_ic) {
    Code* stub = (strict_mode == kStrictMode)
        ? generic_stub_strict()
        : generic_stub();
    if (object->IsJSObject()) {
      JSObject* receiver = JSObject::cast(*object);
      Heap* heap = Handle<JSObject>::cast(object)->GetHeap();
      Map* elements_map = Handle<JSObject>::cast(object)->elements()->map();
      if (elements_map == heap->non_strict_arguments_elements_map()) {
        stub = non_strict_arguments_stub();
      } else if (!force_generic) {
        if (key->IsSmi() && (target() != non_strict_arguments_stub())) {
          HandleScope scope(isolate());
          MaybeObject* maybe_stub = ComputeStub(receiver,
                                                true,
                                                strict_mode,
                                                stub);
          stub = maybe_stub->IsFailure() ?
              NULL : Code::cast(maybe_stub->ToObjectUnchecked());
        }
      }
    }
    if (stub != NULL) set_target(stub);
  }

#ifdef DEBUG
  TraceIC("KeyedStoreIC", key, state, target());
#endif

  // Set the property.
  return Runtime::SetObjectProperty(
      isolate(), object , key, value, NONE, strict_mode);
}


void KeyedStoreIC::UpdateCaches(LookupResult* lookup,
                                State state,
                                StrictModeFlag strict_mode,
                                Handle<JSObject> receiver,
                                Handle<String> name,
                                Handle<Object> value) {
  // Skip JSGlobalProxy.
  if (receiver->IsJSGlobalProxy()) return;

  // Bail out if we didn't find a result.
  if (!lookup->IsPropertyOrTransition() || !lookup->IsCacheable()) return;

  // If the property is read-only, we leave the IC in its current
  // state.
  if (lookup->IsReadOnly()) return;

  // If the property has a non-field type allowing map transitions
  // where there is extra room in the object, we leave the IC in its
  // current state.
  PropertyType type = lookup->type();

  // Compute the code stub for this store; used for rewriting to
  // monomorphic state and making sure that the code stub is in the
  // stub cache.
  MaybeObject* maybe_code = NULL;
  Object* code = NULL;

  switch (type) {
    case FIELD: {
      maybe_code = isolate()->stub_cache()->ComputeKeyedStoreField(
          *name, *receiver, lookup->GetFieldIndex(), NULL, strict_mode);
      break;
    }
    case MAP_TRANSITION: {
      if (lookup->GetAttributes() == NONE) {
        HandleScope scope(isolate());
        ASSERT(type == MAP_TRANSITION);
        Handle<Map> transition(lookup->GetTransitionMap());
        int index = transition->PropertyIndexFor(*name);
        maybe_code = isolate()->stub_cache()->ComputeKeyedStoreField(
            *name, *receiver, index, *transition, strict_mode);
        break;
      }
      // fall through.
    }
    default: {
      // Always rewrite to the generic case so that we do not
      // repeatedly try to rewrite.
      maybe_code = (strict_mode == kStrictMode)
          ? generic_stub_strict()
          : generic_stub();
      break;
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (maybe_code == NULL || !maybe_code->ToObject(&code)) return;

  // Patch the call site depending on the state of the cache.  Make
  // sure to always rewrite from monomorphic to megamorphic.
  ASSERT(state != MONOMORPHIC_PROTOTYPE_FAILURE);
  if (state == UNINITIALIZED || state == PREMONOMORPHIC) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    set_target((strict_mode == kStrictMode)
                 ? megamorphic_stub_strict()
                 : megamorphic_stub());
  }

#ifdef DEBUG
  TraceIC("KeyedStoreIC", name, state, target());
#endif
}


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

static JSFunction* CompileFunction(Isolate* isolate,
                                   JSFunction* function) {
  // Compile now with optimization.
  HandleScope scope(isolate);
  Handle<JSFunction> function_handle(function, isolate);
  CompileLazy(function_handle, CLEAR_EXCEPTION);
  return *function_handle;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, CallIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  CallIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  MaybeObject* maybe_result = ic.LoadFunction(state,
                                              extra_ic_state,
                                              args.at<Object>(0),
                                              args.at<String>(1));
  Object* result;
  if (!maybe_result->ToObject(&result)) return maybe_result;

  // The first time the inline cache is updated may be the first time the
  // function it references gets called.  If the function was lazily compiled
  // then the first call will trigger a compilation.  We check for this case
  // and we do the compilation immediately, instead of waiting for the stub
  // currently attached to the JSFunction object to trigger compilation.  We
  // do this in the case where we know that the inline cache is inside a loop,
  // because then we know that we want to optimize the function.
  if (!result->IsJSFunction() || JSFunction::cast(result)->is_compiled()) {
    return result;
  }
  return CompileFunction(isolate, JSFunction::cast(result));
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, KeyedCallIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  KeyedCallIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Object* result;
  { MaybeObject* maybe_result =
      ic.LoadFunction(state, args.at<Object>(0), args.at<Object>(1));
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  if (!result->IsJSFunction() || JSFunction::cast(result)->is_compiled()) {
    return result;
  }
  return CompileFunction(isolate, JSFunction::cast(result));
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, LoadIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  LoadIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<String>(1));
}


// Used from ic-<arch>.cc
RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<Object>(1), false);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissForceGeneric) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  KeyedLoadIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  return ic.Load(state, args.at<Object>(0), args.at<Object>(1), true);
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, StoreIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  StoreIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  static_cast<StrictModeFlag>(extra_ic_state & kStrictMode),
                  args.at<Object>(0),
                  args.at<String>(1),
                  args.at<Object>(2));
}


RUNTIME_FUNCTION(MaybeObject*, StoreIC_ArrayLength) {
  NoHandleAllocation nha;

  ASSERT(args.length() == 2);
  JSObject* receiver = JSObject::cast(args[0]);
  Object* len = args[1];

  // The generated code should filter out non-Smis before we get here.
  ASSERT(len->IsSmi());

  Object* result;
  { MaybeObject* maybe_result = receiver->SetElementsLength(len);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return len;
}


// Extend storage is called in a store inline cache when
// it is necessary to extend the properties array of a
// JSObject.
RUNTIME_FUNCTION(MaybeObject*, SharedStoreIC_ExtendStorage) {
  NoHandleAllocation na;
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
  { MaybeObject* maybe_result = old_storage->CopySize(new_size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  FixedArray* new_storage = FixedArray::cast(result);
  new_storage->set(old_storage->length(), value);

  // Set the new property value and do the map transition.
  object->set_properties(new_storage);
  object->set_map(transition);

  // Return the stored value.
  return value;
}


// Used from ic-<arch>.cc.
RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  static_cast<StrictModeFlag>(extra_ic_state & kStrictMode),
                  args.at<Object>(0),
                  args.at<Object>(1),
                  args.at<Object>(2),
                  false);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_Slow) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(isolate);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  StrictModeFlag strict_mode =
      static_cast<StrictModeFlag>(extra_ic_state & kStrictMode);
  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    NONE,
                                    strict_mode);
}


RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissForceGeneric) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  KeyedStoreIC ic(isolate);
  IC::State state = IC::StateFrom(ic.target(), args[0], args[1]);
  Code::ExtraICState extra_ic_state = ic.target()->extra_ic_state();
  return ic.Store(state,
                  static_cast<StrictModeFlag>(extra_ic_state & kStrictMode),
                  args.at<Object>(0),
                  args.at<Object>(1),
                  args.at<Object>(2),
                  true);
}


void UnaryOpIC::patch(Code* code) {
  set_target(code);
}


const char* UnaryOpIC::GetName(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED: return "Uninitialized";
    case SMI: return "Smi";
    case HEAP_NUMBER: return "HeapNumbers";
    case GENERIC: return "Generic";
    default: return "Invalid";
  }
}


UnaryOpIC::State UnaryOpIC::ToState(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED:
      return ::v8::internal::UNINITIALIZED;
    case SMI:
    case HEAP_NUMBER:
      return MONOMORPHIC;
    case GENERIC:
      return MEGAMORPHIC;
  }
  UNREACHABLE();
  return ::v8::internal::UNINITIALIZED;
}

UnaryOpIC::TypeInfo UnaryOpIC::GetTypeInfo(Handle<Object> operand) {
  ::v8::internal::TypeInfo operand_type =
      ::v8::internal::TypeInfo::TypeFromValue(operand);
  if (operand_type.IsSmi()) {
    return SMI;
  } else if (operand_type.IsNumber()) {
    return HEAP_NUMBER;
  } else {
    return GENERIC;
  }
}


UnaryOpIC::TypeInfo UnaryOpIC::ComputeNewType(
    UnaryOpIC::TypeInfo current_type,
    UnaryOpIC::TypeInfo previous_type) {
  switch (previous_type) {
    case UnaryOpIC::UNINITIALIZED:
      return current_type;
    case UnaryOpIC::SMI:
      return (current_type == UnaryOpIC::GENERIC)
          ? UnaryOpIC::GENERIC
          : UnaryOpIC::HEAP_NUMBER;
    case UnaryOpIC::HEAP_NUMBER:
      return UnaryOpIC::GENERIC;
    case UnaryOpIC::GENERIC:
      // We should never do patching if we are in GENERIC state.
      UNREACHABLE();
      return UnaryOpIC::GENERIC;
  }
  UNREACHABLE();
  return UnaryOpIC::GENERIC;
}


void BinaryOpIC::patch(Code* code) {
  set_target(code);
}


const char* BinaryOpIC::GetName(TypeInfo type_info) {
  switch (type_info) {
    case UNINITIALIZED: return "Uninitialized";
    case SMI: return "SMI";
    case INT32: return "Int32s";
    case HEAP_NUMBER: return "HeapNumbers";
    case ODDBALL: return "Oddball";
    case BOTH_STRING: return "BothStrings";
    case STRING: return "Strings";
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
    case HEAP_NUMBER:
    case ODDBALL:
    case BOTH_STRING:
    case STRING:
      return MONOMORPHIC;
    case GENERIC:
      return MEGAMORPHIC;
  }
  UNREACHABLE();
  return ::v8::internal::UNINITIALIZED;
}


BinaryOpIC::TypeInfo BinaryOpIC::JoinTypes(BinaryOpIC::TypeInfo x,
                                           BinaryOpIC::TypeInfo y) {
  if (x == UNINITIALIZED) return y;
  if (y == UNINITIALIZED) return x;
  if (x == y) return x;
  if (x == BOTH_STRING && y == STRING) return STRING;
  if (x == STRING && y == BOTH_STRING) return STRING;
  if (x == STRING || x == BOTH_STRING || y == STRING || y == BOTH_STRING) {
    return GENERIC;
  }
  if (x > y) return x;
  return y;
}


BinaryOpIC::TypeInfo BinaryOpIC::GetTypeInfo(Handle<Object> left,
                                             Handle<Object> right) {
  ::v8::internal::TypeInfo left_type =
      ::v8::internal::TypeInfo::TypeFromValue(left);
  ::v8::internal::TypeInfo right_type =
      ::v8::internal::TypeInfo::TypeFromValue(right);

  if (left_type.IsSmi() && right_type.IsSmi()) {
    return SMI;
  }

  if (left_type.IsInteger32() && right_type.IsInteger32()) {
    // Platforms with 32-bit Smis have no distinct INT32 type.
    if (kSmiValueSize == 32) return SMI;
    return INT32;
  }

  if (left_type.IsNumber() && right_type.IsNumber()) {
    return HEAP_NUMBER;
  }

  // Patching for fast string ADD makes sense even if only one of the
  // arguments is a string.
  if (left_type.IsString())  {
    return right_type.IsString() ? BOTH_STRING : STRING;
  } else if (right_type.IsString()) {
    return STRING;
  }

  // Check for oddball objects.
  if (left->IsUndefined() && right->IsNumber()) return ODDBALL;
  if (left->IsNumber() && right->IsUndefined()) return ODDBALL;

  return GENERIC;
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
  Handle<Code> code = stub.GetCode();
  if (!code.is_null()) {
    if (FLAG_trace_ic) {
      PrintF("[UnaryOpIC (%s->%s)#%s]\n",
             UnaryOpIC::GetName(previous_type),
             UnaryOpIC::GetName(type),
             Token::Name(op));
    }
    UnaryOpIC ic(isolate);
    ic.patch(*code);
  }

  Handle<JSBuiltinsObject> builtins = Handle<JSBuiltinsObject>(
      isolate->thread_local_top()->context_->builtins(), isolate);
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

RUNTIME_FUNCTION(MaybeObject*, BinaryOp_Patch) {
  ASSERT(args.length() == 5);

  HandleScope scope(isolate);
  Handle<Object> left = args.at<Object>(0);
  Handle<Object> right = args.at<Object>(1);
  int key = args.smi_at(2);
  Token::Value op = static_cast<Token::Value>(args.smi_at(3));
  BinaryOpIC::TypeInfo previous_type =
      static_cast<BinaryOpIC::TypeInfo>(args.smi_at(4));

  BinaryOpIC::TypeInfo type = BinaryOpIC::GetTypeInfo(left, right);
  type = BinaryOpIC::JoinTypes(type, previous_type);
  BinaryOpIC::TypeInfo result_type = BinaryOpIC::UNINITIALIZED;
  if ((type == BinaryOpIC::STRING || type == BinaryOpIC::BOTH_STRING) &&
      op != Token::ADD) {
    type = BinaryOpIC::GENERIC;
  }
  if (type == BinaryOpIC::SMI && previous_type == BinaryOpIC::SMI) {
    if (op == Token::DIV ||
        op == Token::MUL ||
        op == Token::SHR ||
        kSmiValueSize == 32) {
      // Arithmetic on two Smi inputs has yielded a heap number.
      // That is the only way to get here from the Smi stub.
      // With 32-bit Smis, all overflows give heap numbers, but with
      // 31-bit Smis, most operations overflow to int32 results.
      result_type = BinaryOpIC::HEAP_NUMBER;
    } else {
      // Other operations on SMIs that overflow yield int32s.
      result_type = BinaryOpIC::INT32;
    }
  }
  if (type == BinaryOpIC::INT32 && previous_type == BinaryOpIC::INT32) {
    // We must be here because an operation on two INT32 types overflowed.
    result_type = BinaryOpIC::HEAP_NUMBER;
  }

  BinaryOpStub stub(key, type, result_type);
  Handle<Code> code = stub.GetCode();
  if (!code.is_null()) {
    if (FLAG_trace_ic) {
      PrintF("[BinaryOpIC (%s->(%s->%s))#%s]\n",
             BinaryOpIC::GetName(previous_type),
             BinaryOpIC::GetName(type),
             BinaryOpIC::GetName(result_type),
             Token::Name(op));
    }
    BinaryOpIC ic(isolate);
    ic.patch(*code);

    // Activate inlined smi code.
    if (previous_type == BinaryOpIC::UNINITIALIZED) {
      PatchInlinedSmiCode(ic.address());
    }
  }

  Handle<JSBuiltinsObject> builtins = Handle<JSBuiltinsObject>(
      isolate->thread_local_top()->context_->builtins(), isolate);
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
  Object** builtin_args[] = { right.location() };
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


Handle<Code> CompareIC::GetUninitialized(Token::Value op) {
  ICCompareStub stub(op, UNINITIALIZED);
  return stub.GetCode();
}


CompareIC::State CompareIC::ComputeState(Code* target) {
  int key = target->major_key();
  if (key == CodeStub::Compare) return GENERIC;
  ASSERT(key == CodeStub::CompareIC);
  return static_cast<State>(target->compare_state());
}


const char* CompareIC::GetStateName(State state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case SMIS: return "SMIS";
    case HEAP_NUMBERS: return "HEAP_NUMBERS";
    case OBJECTS: return "OBJECTS";
    case SYMBOLS: return "SYMBOLS";
    case STRINGS: return "STRINGS";
    case GENERIC: return "GENERIC";
    default:
      UNREACHABLE();
      return NULL;
  }
}


CompareIC::State CompareIC::TargetState(State state,
                                        bool has_inlined_smi_code,
                                        Handle<Object> x,
                                        Handle<Object> y) {
  if (!has_inlined_smi_code && state != UNINITIALIZED && state != SYMBOLS) {
    return GENERIC;
  }
  if (state == UNINITIALIZED && x->IsSmi() && y->IsSmi()) return SMIS;
  if ((state == UNINITIALIZED || (state == SMIS && has_inlined_smi_code)) &&
      x->IsNumber() && y->IsNumber()) return HEAP_NUMBERS;
  if (op_ != Token::EQ && op_ != Token::EQ_STRICT) return GENERIC;
  if (state == UNINITIALIZED &&
      x->IsSymbol() && y->IsSymbol()) return SYMBOLS;
  if ((state == UNINITIALIZED || state == SYMBOLS) &&
      x->IsString() && y->IsString()) return STRINGS;
  if (state == UNINITIALIZED &&
      x->IsJSObject() && y->IsJSObject()) return OBJECTS;
  return GENERIC;
}


// Used from ic_<arch>.cc.
RUNTIME_FUNCTION(Code*, CompareIC_Miss) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  CompareIC ic(isolate, static_cast<Token::Value>(args.smi_at(2)));
  ic.UpdateCaches(args.at<Object>(0), args.at<Object>(1));
  return ic.target();
}


RUNTIME_FUNCTION(MaybeObject*, ToBoolean_Patch) {
  ASSERT(args.length() == 3);

  HandleScope scope(isolate);
  Handle<Object> object = args.at<Object>(0);
  Register tos = Register::from_code(args.smi_at(1));
  ToBooleanStub::Types old_types(args.smi_at(2));

  ToBooleanStub::Types new_types(old_types);
  bool to_boolean_value = new_types.Record(object);
  old_types.TraceTransition(new_types);

  ToBooleanStub stub(tos, new_types);
  Handle<Code> code = stub.GetCode();
  ToBooleanIC ic(isolate);
  ic.patch(*code);
  return Smi::FromInt(to_boolean_value ? 1 : 0);
}


void ToBooleanIC::patch(Code* code) {
  set_target(code);
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
