// Copyright 2006-2009 the V8 project authors. All rights reserved.
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
                 Handle<String> name,
                 State old_state,
                 Code* new_target,
                 const char* extra_info) {
  if (FLAG_trace_ic) {
    State new_state = StateFrom(new_target, Heap::undefined_value());
    PrintF("[%s (%c->%c)%s", type,
           TransitionMarkFromState(old_state),
           TransitionMarkFromState(new_state),
           extra_info);
    name->Print();
    PrintF("]\n");
  }
}
#endif


IC::IC(FrameDepth depth) {
  // To improve the performance of the (much used) IC code, we unfold
  // a few levels of the stack frame iteration code. This yields a
  // ~35% speedup when running DeltaBlue with the '--nouse-ic' flag.
  const Address entry = Top::c_entry_fp(Top::GetCurrentThread());
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

IC::State IC::StateFrom(Code* target, Object* receiver) {
  IC::State state = target->ic_state();

  if (state != MONOMORPHIC) return state;
  if (receiver->IsUndefined() || receiver->IsNull()) return state;

  Map* map = GetCodeCacheMapForObject(receiver);

  // Decide whether the inline cache failed because of changes to the
  // receiver itself or changes to one of its prototypes.
  //
  // If there are changes to the receiver itself, the map of the
  // receiver will have changed and the current target will not be in
  // the receiver map's code cache.  Therefore, if the current target
  // is in the receiver map's code cache, the inline cache failed due
  // to prototype check failure.
  int index = map->IndexInCodeCache(target);
  if (index >= 0) {
    // For keyed load/store, the most likely cause of cache failure is
    // that the key has changed.  We do not distinguish between
    // prototype and non-prototype failures for keyed access.
    Code::Kind kind = target->kind();
    if (kind == Code::KEYED_LOAD_IC || kind == Code::KEYED_STORE_IC) {
      return MONOMORPHIC;
    }

    // Remove the target from the code cache to avoid hitting the same
    // invalid stub again.
    map->RemoveFromCodeCache(index);

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
  Code* code = Code::cast(Heap::FindCodeObject(addr));
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
                       Handle<String> name) {
  HandleScope scope;
  Handle<Object> args[2] = { name, object };
  Handle<Object> error = Factory::NewTypeError(type, HandleVector(args, 2));
  return Top::Throw(*error);
}


Failure* IC::ReferenceError(const char* type, Handle<String> name) {
  HandleScope scope;
  Handle<Object> error =
      Factory::NewReferenceError(type, HandleVector(&name, 1));
  return Top::Throw(*error);
}


void IC::Clear(Address address) {
  Code* target = GetTargetAtAddress(address);

  // Don't clear debug break inline cache as it will remove the break point.
  if (target->ic_state() == DEBUG_BREAK) return;

  switch (target->kind()) {
    case Code::LOAD_IC: return LoadIC::Clear(address, target);
    case Code::KEYED_LOAD_IC: return KeyedLoadIC::Clear(address, target);
    case Code::STORE_IC: return StoreIC::Clear(address, target);
    case Code::KEYED_STORE_IC: return KeyedStoreIC::Clear(address, target);
    case Code::CALL_IC: return CallIC::Clear(address, target);
    default: UNREACHABLE();
  }
}


void CallIC::Clear(Address address, Code* target) {
  State state = target->ic_state();
  InLoopFlag in_loop = target->ic_in_loop();
  if (state == UNINITIALIZED) return;
  Code* code =
      StubCache::FindCallInitialize(target->arguments_count(), in_loop);
  SetTargetAtAddress(address, code);
}


void KeyedLoadIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  // Make sure to also clear the map used in inline fast cases.  If we
  // do not clear these maps, cached code can keep objects alive
  // through the embedded maps.
  ClearInlinedVersion(address);
  SetTargetAtAddress(address, initialize_stub());
}


void LoadIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  ClearInlinedVersion(address);
  SetTargetAtAddress(address, initialize_stub());
}


void StoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address, initialize_stub());
}


void KeyedStoreIC::Clear(Address address, Code* target) {
  if (target->ic_state() == UNINITIALIZED) return;
  SetTargetAtAddress(address, initialize_stub());
}


Code* KeyedLoadIC::external_array_stub(JSObject::ElementsKind elements_kind) {
  switch (elements_kind) {
    case JSObject::EXTERNAL_BYTE_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalByteArray);
    case JSObject::EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalUnsignedByteArray);
    case JSObject::EXTERNAL_SHORT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalShortArray);
    case JSObject::EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      return Builtins::builtin(
          Builtins::KeyedLoadIC_ExternalUnsignedShortArray);
    case JSObject::EXTERNAL_INT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalIntArray);
    case JSObject::EXTERNAL_UNSIGNED_INT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalUnsignedIntArray);
    case JSObject::EXTERNAL_FLOAT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedLoadIC_ExternalFloatArray);
    default:
      UNREACHABLE();
      return NULL;
  }
}


Code* KeyedStoreIC::external_array_stub(JSObject::ElementsKind elements_kind) {
  switch (elements_kind) {
    case JSObject::EXTERNAL_BYTE_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedStoreIC_ExternalByteArray);
    case JSObject::EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      return Builtins::builtin(
          Builtins::KeyedStoreIC_ExternalUnsignedByteArray);
    case JSObject::EXTERNAL_SHORT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedStoreIC_ExternalShortArray);
    case JSObject::EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      return Builtins::builtin(
          Builtins::KeyedStoreIC_ExternalUnsignedShortArray);
    case JSObject::EXTERNAL_INT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedStoreIC_ExternalIntArray);
    case JSObject::EXTERNAL_UNSIGNED_INT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedStoreIC_ExternalUnsignedIntArray);
    case JSObject::EXTERNAL_FLOAT_ELEMENTS:
      return Builtins::builtin(Builtins::KeyedStoreIC_ExternalFloatArray);
    default:
      UNREACHABLE();
      return NULL;
  }
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
    // an interceptor), bail out of lookup is not cacheable: we won't
    // be able to IC it anyway and regular lookup should work fine.
    if (lookup->IsNotFound() || lookup->type() != INTERCEPTOR ||
        !lookup->IsCacheable()) {
      return;
    }

    JSObject* holder = lookup->holder();
    if (HasInterceptorGetter(holder)) {
      return;
    }

    holder->LocalLookupRealNamedProperty(name, lookup);
    if (lookup->IsValid()) {
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


Object* CallIC::TryCallAsFunction(Object* object) {
  HandleScope scope;
  Handle<Object> target(object);
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


Object* CallIC::LoadFunction(State state,
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
    Object* result = object->GetElement(index);
    if (result->IsJSFunction()) return result;

    // Try to find a suitable function delegate for the object at hand.
    result = TryCallAsFunction(result);
    if (result->IsJSFunction()) return result;

    // Otherwise, it will fail in the lookup step.
  }

  // Lookup the property in the object.
  LookupResult lookup;
  LookupForRead(*object, *name, &lookup);

  if (!lookup.IsValid()) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    if (is_contextual()) {
      return ReferenceError("not_defined", name);
    }
    return TypeError("undefined_method", object, name);
  }

  // Lookup is valid: Update inline cache and stub cache.
  if (FLAG_use_ic && lookup.IsLoaded()) {
    UpdateCaches(&lookup, state, object, name);
  }

  // Get the property.
  PropertyAttributes attr;
  Object* result = object->GetProperty(*object, &lookup, *name, &attr);
  if (result->IsFailure()) return result;
  if (lookup.type() == INTERCEPTOR) {
    // If the object does not have the requested property, check which
    // exception we need to throw.
    if (attr == ABSENT) {
      if (is_contextual()) {
        return ReferenceError("not_defined", name);
      }
      return TypeError("undefined_method", object, name);
    }
  }

  ASSERT(result != Heap::the_hole_value());

  if (result->IsJSFunction()) {
    // Check if there is an optimized (builtin) version of the function.
    // Ignored this will degrade performance for Array.prototype.{push,pop}.
    // Please note we only return the optimized function iff
    // the JSObject has FastElements.
    if (object->IsJSObject() && JSObject::cast(*object)->HasFastElements()) {
      Object* opt = Top::LookupSpecialFunction(JSObject::cast(*object),
                                               lookup.holder(),
                                               JSFunction::cast(result));
      if (opt->IsJSFunction()) return opt;
    }

#ifdef ENABLE_DEBUGGER_SUPPORT
    // Handle stepping into a function if step into is active.
    if (Debug::StepInActive()) {
      // Protect the result in a handle as the debugger can allocate and might
      // cause GC.
      HandleScope scope;
      Handle<JSFunction> function(JSFunction::cast(result));
      Debug::HandleStepIn(function, object, fp(), false);
      return *function;
    }
#endif

    return result;
  }

  // Try to find a suitable function delegate for the object at hand.
  result = TryCallAsFunction(result);
  return result->IsJSFunction() ?
      result : TypeError("property_not_function", object, name);
}


void CallIC::UpdateCaches(LookupResult* lookup,
                          State state,
                          Handle<Object> object,
                          Handle<String> name) {
  ASSERT(lookup->IsLoaded());
  // Bail out if we didn't find a result.
  if (!lookup->IsValid() || !lookup->IsCacheable()) return;

  // Compute the number of arguments.
  int argc = target()->arguments_count();
  InLoopFlag in_loop = target()->ic_in_loop();
  Object* code = NULL;

  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = StubCache::ComputeCallPreMonomorphic(argc, in_loop);
  } else if (state == MONOMORPHIC) {
    code = StubCache::ComputeCallMegamorphic(argc, in_loop);
  } else {
    // Compute monomorphic stub.
    switch (lookup->type()) {
      case FIELD: {
        int index = lookup->GetFieldIndex();
        code = StubCache::ComputeCallField(argc, in_loop, *name, *object,
                                           lookup->holder(), index);
        break;
      }
      case CONSTANT_FUNCTION: {
        // Get the constant function and compute the code stub for this
        // call; used for rewriting to monomorphic state and making sure
        // that the code stub is in the stub cache.
        JSFunction* function = lookup->GetConstantFunction();
        code = StubCache::ComputeCallConstant(argc, in_loop, *name, *object,
                                              lookup->holder(), function);
        break;
      }
      case NORMAL: {
        if (!object->IsJSObject()) return;
        Handle<JSObject> receiver = Handle<JSObject>::cast(object);

        if (lookup->holder()->IsGlobalObject()) {
          GlobalObject* global = GlobalObject::cast(lookup->holder());
          JSGlobalPropertyCell* cell =
              JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
          if (!cell->value()->IsJSFunction()) return;
          JSFunction* function = JSFunction::cast(cell->value());
          code = StubCache::ComputeCallGlobal(argc,
                                              in_loop,
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
          if (lookup->holder() != *receiver) return;
          code = StubCache::ComputeCallNormal(argc, in_loop, *name, *receiver);
        }
        break;
      }
      case INTERCEPTOR: {
        ASSERT(HasInterceptorGetter(lookup->holder()));
        code = StubCache::ComputeCallInterceptor(argc, *name, *object,
                                                 lookup->holder());
        break;
      }
      default:
        return;
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (code == NULL || code->IsFailure()) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED ||
      state == PREMONOMORPHIC ||
      state == MONOMORPHIC ||
      state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  }

#ifdef DEBUG
  TraceIC("CallIC", name, state, target(), in_loop ? " (in-loop)" : "");
#endif
}


Object* LoadIC::Load(State state, Handle<Object> object, Handle<String> name) {
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
        name->Equals(Heap::length_symbol())) {
      HandleScope scope;
      // Get the string if we have a string wrapper object.
      if (object->IsJSValue()) {
        object = Handle<Object>(Handle<JSValue>::cast(object)->value());
      }
#ifdef DEBUG
      if (FLAG_trace_ic) PrintF("[LoadIC : +#length /string]\n");
#endif
      Code* target = NULL;
      target = Builtins::builtin(Builtins::LoadIC_StringLength);
      set_target(target);
      StubCache::Set(*name, HeapObject::cast(*object)->map(), target);
      return Smi::FromInt(String::cast(*object)->length());
    }

    // Use specialized code for getting the length of arrays.
    if (object->IsJSArray() && name->Equals(Heap::length_symbol())) {
#ifdef DEBUG
      if (FLAG_trace_ic) PrintF("[LoadIC : +#length /array]\n");
#endif
      Code* target = Builtins::builtin(Builtins::LoadIC_ArrayLength);
      set_target(target);
      StubCache::Set(*name, HeapObject::cast(*object)->map(), target);
      return JSArray::cast(*object)->length();
    }

    // Use specialized code for getting prototype of functions.
    if (object->IsJSFunction() && name->Equals(Heap::prototype_symbol())) {
#ifdef DEBUG
      if (FLAG_trace_ic) PrintF("[LoadIC : +#prototype /function]\n");
#endif
      Code* target = Builtins::builtin(Builtins::LoadIC_FunctionPrototype);
      set_target(target);
      StubCache::Set(*name, HeapObject::cast(*object)->map(), target);
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

  // If lookup is invalid, check if we need to throw an exception.
  if (!lookup.IsValid()) {
    if (FLAG_strict || is_contextual()) {
      return ReferenceError("not_defined", name);
    }
    LOG(SuspectReadEvent(*name, *object));
  }

  bool can_be_inlined =
      FLAG_use_ic &&
      state == PREMONOMORPHIC &&
      lookup.IsValid() &&
      lookup.IsLoaded() &&
      lookup.IsCacheable() &&
      lookup.holder() == *object &&
      lookup.type() == FIELD &&
      !object->IsAccessCheckNeeded();

  if (can_be_inlined) {
    Map* map = lookup.holder()->map();
    // Property's index in the properties array.  If negative we have
    // an inobject property.
    int index = lookup.GetFieldIndex() - map->inobject_properties();
    if (index < 0) {
      // Index is an offset from the end of the object.
      int offset = map->instance_size() + (index * kPointerSize);
      if (PatchInlinedLoad(address(), map, offset)) {
        set_target(megamorphic_stub());
        return lookup.holder()->FastPropertyAt(lookup.GetFieldIndex());
      }
    }
  }

  // Update inline cache and stub cache.
  if (FLAG_use_ic && lookup.IsLoaded()) {
    UpdateCaches(&lookup, state, object, name);
  }

  PropertyAttributes attr;
  if (lookup.IsValid() && lookup.type() == INTERCEPTOR) {
    // Get the property.
    Object* result = object->GetProperty(*object, &lookup, *name, &attr);
    if (result->IsFailure()) return result;
    // If the property is not present, check if we need to throw an
    // exception.
    if (attr == ABSENT && is_contextual()) {
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
  ASSERT(lookup->IsLoaded());
  // Bail out if we didn't find a result.
  if (!lookup->IsValid() || !lookup->IsCacheable()) return;

  // Loading properties from values is not common, so don't try to
  // deal with non-JS objects here.
  if (!object->IsJSObject()) return;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  // Compute the code stub for this load.
  Object* code = NULL;
  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = pre_monomorphic_stub();
  } else {
    // Compute monomorphic stub.
    switch (lookup->type()) {
      case FIELD: {
        code = StubCache::ComputeLoadField(*name, *receiver,
                                           lookup->holder(),
                                           lookup->GetFieldIndex());
        break;
      }
      case CONSTANT_FUNCTION: {
        Object* constant = lookup->GetConstantFunction();
        code = StubCache::ComputeLoadConstant(*name, *receiver,
                                              lookup->holder(), constant);
        break;
      }
      case NORMAL: {
        if (lookup->holder()->IsGlobalObject()) {
          GlobalObject* global = GlobalObject::cast(lookup->holder());
          JSGlobalPropertyCell* cell =
              JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
          code = StubCache::ComputeLoadGlobal(*name,
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
          code = StubCache::ComputeLoadNormal(*name, *receiver);
        }
        break;
      }
      case CALLBACKS: {
        if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
        AccessorInfo* callback =
            AccessorInfo::cast(lookup->GetCallbackObject());
        if (v8::ToCData<Address>(callback->getter()) == 0) return;
        code = StubCache::ComputeLoadCallback(*name, *receiver,
                                              lookup->holder(), callback);
        break;
      }
      case INTERCEPTOR: {
        ASSERT(HasInterceptorGetter(lookup->holder()));
        code = StubCache::ComputeLoadInterceptor(*name, *receiver,
                                                 lookup->holder());
        break;
      }
      default:
        return;
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (code == NULL || code->IsFailure()) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED || state == PREMONOMORPHIC ||
      state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    set_target(megamorphic_stub());
  }

#ifdef DEBUG
  TraceIC("LoadIC", name, state, target());
#endif
}


Object* KeyedLoadIC::Load(State state,
                          Handle<Object> object,
                          Handle<Object> key) {
  if (key->IsSymbol()) {
    Handle<String> name = Handle<String>::cast(key);

    // If the object is undefined or null it's illegal to try to get any
    // of its properties; throw a TypeError in that case.
    if (object->IsUndefined() || object->IsNull()) {
      return TypeError("non_object_property_load", object, name);
    }

    if (FLAG_use_ic) {
      // Use specialized code for getting the length of strings.
      if (object->IsString() && name->Equals(Heap::length_symbol())) {
        Handle<String> string = Handle<String>::cast(object);
        Object* code = NULL;
        code = StubCache::ComputeKeyedLoadStringLength(*name, *string);
        if (code->IsFailure()) return code;
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedLoadIC", name, state, target());
#endif  // DEBUG
        return Smi::FromInt(string->length());
      }

      // Use specialized code for getting the length of arrays.
      if (object->IsJSArray() && name->Equals(Heap::length_symbol())) {
        Handle<JSArray> array = Handle<JSArray>::cast(object);
        Object* code = StubCache::ComputeKeyedLoadArrayLength(*name, *array);
        if (code->IsFailure()) return code;
        set_target(Code::cast(code));
#ifdef DEBUG
        TraceIC("KeyedLoadIC", name, state, target());
#endif  // DEBUG
        return JSArray::cast(*object)->length();
      }

      // Use specialized code for getting prototype of functions.
      if (object->IsJSFunction() && name->Equals(Heap::prototype_symbol())) {
        Handle<JSFunction> function = Handle<JSFunction>::cast(object);
        Object* code =
            StubCache::ComputeKeyedLoadFunctionPrototype(*name, *function);
        if (code->IsFailure()) return code;
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
      HandleScope scope;
      // Rewrite to the generic keyed load stub.
      if (FLAG_use_ic) set_target(generic_stub());
      return Runtime::GetElementOrCharAt(object, index);
    }

    // Named lookup.
    LookupResult lookup;
    LookupForRead(*object, *name, &lookup);

    // If lookup is invalid, check if we need to throw an exception.
    if (!lookup.IsValid()) {
      if (FLAG_strict || is_contextual()) {
        return ReferenceError("not_defined", name);
      }
    }

    if (FLAG_use_ic && lookup.IsLoaded()) {
      UpdateCaches(&lookup, state, object, name);
    }

    PropertyAttributes attr;
    if (lookup.IsValid() && lookup.type() == INTERCEPTOR) {
      // Get the property.
      Object* result = object->GetProperty(*object, &lookup, *name, &attr);
      if (result->IsFailure()) return result;
      // If the property is not present, check if we need to throw an
      // exception.
      if (attr == ABSENT && is_contextual()) {
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
    if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      if (receiver->HasExternalArrayElements()) {
        stub = external_array_stub(receiver->GetElementsKind());
      }
    }
    set_target(stub);
    // For JSObjects that are not value wrappers and that do not have
    // indexed interceptors, we initialize the inlined fast case (if
    // present) by patching the inlined map check.
    if (object->IsJSObject() &&
        !object->IsJSValue() &&
        !JSObject::cast(*object)->HasIndexedInterceptor()) {
      Map* map = JSObject::cast(*object)->map();
      PatchInlinedLoad(address(), map);
    }
  }

  // Get the property.
  return Runtime::GetObjectProperty(object, key);
}


void KeyedLoadIC::UpdateCaches(LookupResult* lookup, State state,
                               Handle<Object> object, Handle<String> name) {
  ASSERT(lookup->IsLoaded());
  // Bail out if we didn't find a result.
  if (!lookup->IsValid() || !lookup->IsCacheable()) return;

  if (!object->IsJSObject()) return;
  Handle<JSObject> receiver = Handle<JSObject>::cast(object);

  // Compute the code stub for this load.
  Object* code = NULL;

  if (state == UNINITIALIZED) {
    // This is the first time we execute this inline cache.
    // Set the target to the pre monomorphic stub to delay
    // setting the monomorphic state.
    code = pre_monomorphic_stub();
  } else {
    // Compute a monomorphic stub.
    switch (lookup->type()) {
      case FIELD: {
        code = StubCache::ComputeKeyedLoadField(*name, *receiver,
                                                lookup->holder(),
                                                lookup->GetFieldIndex());
        break;
      }
      case CONSTANT_FUNCTION: {
        Object* constant = lookup->GetConstantFunction();
        code = StubCache::ComputeKeyedLoadConstant(*name, *receiver,
                                                   lookup->holder(), constant);
        break;
      }
      case CALLBACKS: {
        if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
        AccessorInfo* callback =
            AccessorInfo::cast(lookup->GetCallbackObject());
        if (v8::ToCData<Address>(callback->getter()) == 0) return;
        code = StubCache::ComputeKeyedLoadCallback(*name, *receiver,
                                                   lookup->holder(), callback);
        break;
      }
      case INTERCEPTOR: {
        ASSERT(HasInterceptorGetter(lookup->holder()));
        code = StubCache::ComputeKeyedLoadInterceptor(*name, *receiver,
                                                      lookup->holder());
        break;
      }
      default: {
        // Always rewrite to the generic case so that we do not
        // repeatedly try to rewrite.
        code = generic_stub();
        break;
      }
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (code == NULL || code->IsFailure()) return;

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
  if (!lookup->IsValid() || !lookup->IsCacheable()) return false;

  // If the property is read-only, we leave the IC in its current
  // state.
  if (lookup->IsReadOnly()) return false;

  if (!lookup->IsLoaded()) return false;

  return true;
}


static bool LookupForWrite(JSObject* object,
                           String* name,
                           LookupResult* lookup) {
  object->LocalLookup(name, lookup);
  if (!StoreICableLookup(lookup)) {
    return false;
  }

  if (lookup->type() == INTERCEPTOR) {
    if (object->GetNamedInterceptor()->setter()->IsUndefined()) {
      object->LocalLookupRealNamedProperty(name, lookup);
      return StoreICableLookup(lookup);
    }
  }

  return true;
}


Object* StoreIC::Store(State state,
                       Handle<Object> object,
                       Handle<String> name,
                       Handle<Object> value) {
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
    HandleScope scope;
    Handle<Object> result = SetElement(receiver, index, value);
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  // Lookup the property locally in the receiver.
  if (FLAG_use_ic && !receiver->IsJSGlobalProxy()) {
    LookupResult lookup;
    if (LookupForWrite(*receiver, *name, &lookup)) {
      UpdateCaches(&lookup, state, receiver, name, value);
    }
  }

  // Set the property.
  return receiver->SetProperty(*name, *value, NONE);
}


void StoreIC::UpdateCaches(LookupResult* lookup,
                           State state,
                           Handle<JSObject> receiver,
                           Handle<String> name,
                           Handle<Object> value) {
  ASSERT(lookup->IsLoaded());
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
  Object* code = NULL;
  switch (type) {
    case FIELD: {
      code = StubCache::ComputeStoreField(*name, *receiver,
                                          lookup->GetFieldIndex());
      break;
    }
    case MAP_TRANSITION: {
      if (lookup->GetAttributes() != NONE) return;
      HandleScope scope;
      ASSERT(type == MAP_TRANSITION);
      Handle<Map> transition(lookup->GetTransitionMap());
      int index = transition->PropertyIndexFor(*name);
      code = StubCache::ComputeStoreField(*name, *receiver, index, *transition);
      break;
    }
    case NORMAL: {
      if (!receiver->IsGlobalObject()) {
        return;
      }
      // The stub generated for the global object picks the value directly
      // from the property cell. So the property must be directly on the
      // global object.
      Handle<GlobalObject> global = Handle<GlobalObject>::cast(receiver);
      JSGlobalPropertyCell* cell =
          JSGlobalPropertyCell::cast(global->GetPropertyCell(lookup));
      code = StubCache::ComputeStoreGlobal(*name, *global, cell);
      break;
    }
    case CALLBACKS: {
      if (!lookup->GetCallbackObject()->IsAccessorInfo()) return;
      AccessorInfo* callback = AccessorInfo::cast(lookup->GetCallbackObject());
      if (v8::ToCData<Address>(callback->setter()) == 0) return;
      code = StubCache::ComputeStoreCallback(*name, *receiver, callback);
      break;
    }
    case INTERCEPTOR: {
      ASSERT(!receiver->GetNamedInterceptor()->setter()->IsUndefined());
      code = StubCache::ComputeStoreInterceptor(*name, *receiver);
      break;
    }
    default:
      return;
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (code == NULL || code->IsFailure()) return;

  // Patch the call site depending on the state of the cache.
  if (state == UNINITIALIZED || state == MONOMORPHIC_PROTOTYPE_FAILURE) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    // Only move to mega morphic if the target changes.
    if (target() != Code::cast(code)) set_target(megamorphic_stub());
  }

#ifdef DEBUG
  TraceIC("StoreIC", name, state, target());
#endif
}


Object* KeyedStoreIC::Store(State state,
                            Handle<Object> object,
                            Handle<Object> key,
                            Handle<Object> value) {
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
      HandleScope scope;
      Handle<Object> result = SetElement(receiver, index, value);
      if (result.is_null()) return Failure::Exception();
      return *value;
    }

    // Lookup the property locally in the receiver.
    LookupResult lookup;
    receiver->LocalLookup(*name, &lookup);

    // Update inline cache and stub cache.
    if (FLAG_use_ic && lookup.IsLoaded()) {
      UpdateCaches(&lookup, state, receiver, name, value);
    }

    // Set the property.
    return receiver->SetProperty(*name, *value, NONE);
  }

  // Do not use ICs for objects that require access checks (including
  // the global object).
  bool use_ic = FLAG_use_ic && !object->IsAccessCheckNeeded();
  ASSERT(!(use_ic && object->IsJSGlobalProxy()));

  if (use_ic) {
    Code* stub = generic_stub();
    if (object->IsJSObject()) {
      Handle<JSObject> receiver = Handle<JSObject>::cast(object);
      if (receiver->HasExternalArrayElements()) {
        stub = external_array_stub(receiver->GetElementsKind());
      }
    }
    set_target(stub);
  }

  // Set the property.
  return Runtime::SetObjectProperty(object, key, value, NONE);
}


void KeyedStoreIC::UpdateCaches(LookupResult* lookup,
                                State state,
                                Handle<JSObject> receiver,
                                Handle<String> name,
                                Handle<Object> value) {
  ASSERT(lookup->IsLoaded());

  // Skip JSGlobalProxy.
  if (receiver->IsJSGlobalProxy()) return;

  // Bail out if we didn't find a result.
  if (!lookup->IsValid() || !lookup->IsCacheable()) return;

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
  Object* code = NULL;

  switch (type) {
    case FIELD: {
      code = StubCache::ComputeKeyedStoreField(*name, *receiver,
                                               lookup->GetFieldIndex());
      break;
    }
    case MAP_TRANSITION: {
      if (lookup->GetAttributes() == NONE) {
        HandleScope scope;
        ASSERT(type == MAP_TRANSITION);
        Handle<Map> transition(lookup->GetTransitionMap());
        int index = transition->PropertyIndexFor(*name);
        code = StubCache::ComputeKeyedStoreField(*name, *receiver,
                                                 index, *transition);
        break;
      }
      // fall through.
    }
    default: {
      // Always rewrite to the generic case so that we do not
      // repeatedly try to rewrite.
      code = generic_stub();
      break;
    }
  }

  // If we're unable to compute the stub (not enough memory left), we
  // simply avoid updating the caches.
  if (code == NULL || code->IsFailure()) return;

  // Patch the call site depending on the state of the cache.  Make
  // sure to always rewrite from monomorphic to megamorphic.
  ASSERT(state != MONOMORPHIC_PROTOTYPE_FAILURE);
  if (state == UNINITIALIZED || state == PREMONOMORPHIC) {
    set_target(Code::cast(code));
  } else if (state == MONOMORPHIC) {
    set_target(megamorphic_stub());
  }

#ifdef DEBUG
  TraceIC("KeyedStoreIC", name, state, target());
#endif
}


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

// Used from ic_<arch>.cc.
Object* CallIC_Miss(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  CallIC ic;
  IC::State state = IC::StateFrom(ic.target(), args[0]);
  Object* result =
      ic.LoadFunction(state, args.at<Object>(0), args.at<String>(1));

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

  // Compile now with optimization.
  HandleScope scope;
  Handle<JSFunction> function = Handle<JSFunction>(JSFunction::cast(result));
  InLoopFlag in_loop = ic.target()->ic_in_loop();
  if (in_loop == IN_LOOP) {
    CompileLazyInLoop(function, CLEAR_EXCEPTION);
  } else {
    CompileLazy(function, CLEAR_EXCEPTION);
  }
  return *function;
}


void CallIC::GenerateInitialize(MacroAssembler* masm, int argc) {
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


void CallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


// Used from ic_<arch>.cc.
Object* LoadIC_Miss(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  LoadIC ic;
  IC::State state = IC::StateFrom(ic.target(), args[0]);
  return ic.Load(state, args.at<Object>(0), args.at<String>(1));
}


void LoadIC::GenerateInitialize(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GeneratePreMonomorphic(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


// Used from ic_<arch>.cc
Object* KeyedLoadIC_Miss(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);
  KeyedLoadIC ic;
  IC::State state = IC::StateFrom(ic.target(), args[0]);
  return ic.Load(state, args.at<Object>(0), args.at<Object>(1));
}


void KeyedLoadIC::GenerateInitialize(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kKeyedLoadIC_Miss)));
}


void KeyedLoadIC::GeneratePreMonomorphic(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kKeyedLoadIC_Miss)));
}


// Used from ic_<arch>.cc.
Object* StoreIC_Miss(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  StoreIC ic;
  IC::State state = IC::StateFrom(ic.target(), args[0]);
  return ic.Store(state, args.at<Object>(0), args.at<String>(1),
                  args.at<Object>(2));
}


// Extend storage is called in a store inline cache when
// it is necessary to extend the properties array of a
// JSObject.
Object* SharedStoreIC_ExtendStorage(Arguments args) {
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
  Object* result = old_storage->CopySize(new_size);
  if (result->IsFailure()) return result;
  FixedArray* new_storage = FixedArray::cast(result);
  new_storage->set(old_storage->length(), value);

  // Set the new property value and do the map transition.
  object->set_properties(new_storage);
  object->set_map(transition);

  // Return the stored value.
  return value;
}


void StoreIC::GenerateInitialize(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kStoreIC_Miss)));
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kStoreIC_Miss)));
}


// Used from ic_<arch>.cc.
Object* KeyedStoreIC_Miss(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 3);
  KeyedStoreIC ic;
  IC::State state = IC::StateFrom(ic.target(), args[0]);
  return ic.Store(state, args.at<Object>(0), args.at<Object>(1),
                  args.at<Object>(2));
}


void KeyedStoreIC::GenerateInitialize(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kKeyedStoreIC_Miss)));
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kKeyedStoreIC_Miss)));
}


static Address IC_utilities[] = {
#define ADDR(name) FUNCTION_ADDR(name),
    IC_UTIL_LIST(ADDR)
    NULL
#undef ADDR
};


Address IC::AddressFromUtilityId(IC::UtilityId id) {
  return IC_utilities[id];
}


} }  // namespace v8::internal
