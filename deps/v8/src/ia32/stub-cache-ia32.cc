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

#if defined(V8_TARGET_ARCH_IA32)

#include "ic-inl.h"
#include "codegen.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate,
                       MacroAssembler* masm,
                       Code::Flags flags,
                       StubCache::Table table,
                       Register name,
                       Register offset,
                       Register extra) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));

  Label miss;

  if (extra.is_valid()) {
    // Get the code entry from the cache.
    __ mov(extra, Operand::StaticArray(offset, times_2, value_offset));

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_2, key_offset));
    __ j(not_equal, &miss);

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(extra, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
    __ j(not_equal, &miss);

    // Jump to the first instruction in the code stub.
    __ add(Operand(extra), Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(Operand(extra));

    __ bind(&miss);
  } else {
    // Save the offset on the stack.
    __ push(offset);

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_2, key_offset));
    __ j(not_equal, &miss);

    // Get the code entry from the cache.
    __ mov(offset, Operand::StaticArray(offset, times_2, value_offset));

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(offset, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
    __ j(not_equal, &miss);

    // Restore offset and re-load code entry from cache.
    __ pop(offset);
    __ mov(offset, Operand::StaticArray(offset, times_2, value_offset));

    // Jump to the first instruction in the code stub.
    __ add(Operand(offset), Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(Operand(offset));

    // Pop at miss.
    __ bind(&miss);
    __ pop(offset);
  }
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be a symbol and receiver must be a heap object.
static MaybeObject* GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                                     Label* miss_label,
                                                     Register receiver,
                                                     String* name,
                                                     Register r0,
                                                     Register r1) {
  ASSERT(name->IsSymbol());
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1);

  __ mov(r0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ test_b(FieldOperand(r0, Map::kBitFieldOffset),
            kInterceptorOrAccessCheckNeededMask);
  __ j(not_zero, miss_label);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(r0, FIRST_SPEC_OBJECT_TYPE);
  __ j(below, miss_label);

  // Load properties array.
  Register properties = r0;
  __ mov(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ cmp(FieldOperand(properties, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->hash_table_map()));
  __ j(not_equal, miss_label);

  Label done;
  MaybeObject* result =
      StringDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                         miss_label,
                                                         &done,
                                                         properties,
                                                         name,
                                                         r1);
  if (result->IsFailure()) return result;

  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);

  return result;
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra,
                              Register extra2) {
  Isolate* isolate = Isolate::Current();
  Label miss;
  USE(extra2);  // The register extra2 is not used on the ia32 platform.

  // Make sure that code is valid. The shifting code relies on the
  // entry size being 8.
  ASSERT(sizeof(Entry) == 8);

  // Make sure the flags does not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));
  ASSERT(!extra.is(receiver));
  ASSERT(!extra.is(name));
  ASSERT(!extra.is(scratch));

  // Check scratch and extra registers are valid, and extra2 is unused.
  ASSERT(!scratch.is(no_reg));
  ASSERT(extra2.is(no_reg));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ mov(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ add(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, flags);
  __ and_(scratch, (kPrimaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, name, scratch, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ add(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, flags);
  __ and_(scratch, (kPrimaryTableSize - 1) << kHeapObjectTagSize);
  __ sub(scratch, Operand(name));
  __ add(Operand(scratch), Immediate(flags));
  __ and_(scratch, (kSecondaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, name, scratch, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  __ LoadGlobalFunction(index, prototype);
  __ LoadGlobalFunctionInitialMap(prototype, prototype);
  // Load the prototype from the initial map.
  __ mov(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
  // Check we're still in the same context.
  __ cmp(Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)),
         masm->isolate()->global());
  __ j(not_equal, miss);
  // Get the global function with the given index.
  JSFunction* function =
      JSFunction::cast(masm->isolate()->global_context()->get(index));
  // Load its initial map. The global functions all have initial maps.
  __ Set(prototype, Immediate(Handle<Map>(function->initial_map())));
  // Load the prototype from the initial map.
  __ mov(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, miss_label);

  // Load length directly from the JS array.
  __ mov(eax, FieldOperand(receiver, JSArray::kLengthOffset));
  __ ret(0);
}


// Generate code to check if an object is a string.  If the object is
// a string, the map's instance type is left in the scratch register.
static void GenerateStringCheck(MacroAssembler* masm,
                                Register receiver,
                                Register scratch,
                                Label* smi,
                                Label* non_string_object) {
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, smi);

  // Check that the object is a string.
  __ mov(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ test(scratch, Immediate(kNotStringTag));
  __ j(not_zero, non_string_object);
}


void StubCompiler::GenerateLoadStringLength(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss,
                                            bool support_wrappers) {
  Label check_wrapper;

  // Check if the object is a string leaving the instance type in the
  // scratch register.
  GenerateStringCheck(masm, receiver, scratch1, miss,
                      support_wrappers ? &check_wrapper : miss);

  // Load length from the string and convert to a smi.
  __ mov(eax, FieldOperand(receiver, String::kLengthOffset));
  __ ret(0);

  if (support_wrappers) {
    // Check if the object is a JSValue wrapper.
    __ bind(&check_wrapper);
    __ cmp(scratch1, JS_VALUE_TYPE);
    __ j(not_equal, miss);

    // Check if the wrapped value is a string and load the length
    // directly if it is.
    __ mov(scratch2, FieldOperand(receiver, JSValue::kValueOffset));
    GenerateStringCheck(masm, scratch2, scratch1, miss, miss);
    __ mov(eax, FieldOperand(scratch2, String::kLengthOffset));
    __ ret(0);
  }
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(eax, Operand(scratch1));
  __ ret(0);
}


// Load a fast property out of a holder object (src). In-object properties
// are loaded directly otherwise the property is loaded from the properties
// fixed array.
void StubCompiler::GenerateFastPropertyLoad(MacroAssembler* masm,
                                            Register dst, Register src,
                                            JSObject* holder, int index) {
  // Adjust for the number of properties stored in the holder.
  index -= holder->map()->inobject_properties();
  if (index < 0) {
    // Get the property straight out of the holder.
    int offset = holder->map()->instance_size() + (index * kPointerSize);
    __ mov(dst, FieldOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    __ mov(dst, FieldOperand(src, JSObject::kPropertiesOffset));
    __ mov(dst, FieldOperand(dst, offset));
  }
}


static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Register name,
                                     JSObject* holder_obj) {
  __ push(name);
  InterceptorInfo* interceptor = holder_obj->GetNamedInterceptor();
  ASSERT(!masm->isolate()->heap()->InNewSpace(interceptor));
  Register scratch = name;
  __ mov(scratch, Immediate(Handle<Object>(interceptor)));
  __ push(scratch);
  __ push(receiver);
  __ push(holder);
  __ push(FieldOperand(scratch, InterceptorInfo::kDataOffset));
}


static void CompileCallLoadPropertyWithInterceptor(MacroAssembler* masm,
                                                   Register receiver,
                                                   Register holder,
                                                   Register name,
                                                   JSObject* holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly),
                        masm->isolate()),
      5);
}


// Number of pointers to be reserved on stack for fast API call.
static const int kFastApiCallArguments = 3;


// Reserves space for the extra arguments to API function in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : last argument in the internal frame of the caller
  // -----------------------------------
  __ pop(scratch);
  for (int i = 0; i < kFastApiCallArguments; i++) {
    __ push(Immediate(Smi::FromInt(0)));
  }
  __ push(scratch);
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- esp[0]  : return address.
  //  -- esp[4]  : last fast api call extra argument.
  //  -- ...
  //  -- esp[kFastApiCallArguments * 4] : first fast api call extra argument.
  //  -- esp[kFastApiCallArguments * 4 + 4] : last argument in the internal
  //                                          frame.
  // -----------------------------------
  __ pop(scratch);
  __ add(Operand(esp), Immediate(kPointerSize * kFastApiCallArguments));
  __ push(scratch);
}


// Generates call to API function.
static MaybeObject* GenerateFastApiCall(MacroAssembler* masm,
                                        const CallOptimization& optimization,
                                        int argc) {
  // ----------- S t a t e -------------
  //  -- esp[0]              : return address
  //  -- esp[4]              : object passing the type check
  //                           (last fast api call extra argument,
  //                            set by CheckPrototypes)
  //  -- esp[8]              : api function
  //                           (first fast api call extra argument)
  //  -- esp[12]             : api call data
  //  -- esp[16]             : last argument
  //  -- ...
  //  -- esp[(argc + 3) * 4] : first argument
  //  -- esp[(argc + 4) * 4] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  JSFunction* function = optimization.constant_function();
  __ mov(edi, Immediate(Handle<JSFunction>(function)));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Pass the additional arguments.
  __ mov(Operand(esp, 2 * kPointerSize), edi);
  Object* call_data = optimization.api_call_info()->data();
  Handle<CallHandlerInfo> api_call_info_handle(optimization.api_call_info());
  if (masm->isolate()->heap()->InNewSpace(call_data)) {
    __ mov(ecx, api_call_info_handle);
    __ mov(ebx, FieldOperand(ecx, CallHandlerInfo::kDataOffset));
    __ mov(Operand(esp, 3 * kPointerSize), ebx);
  } else {
    __ mov(Operand(esp, 3 * kPointerSize),
           Immediate(Handle<Object>(call_data)));
  }

  // Prepare arguments.
  __ lea(eax, Operand(esp, 3 * kPointerSize));

  Object* callback = optimization.api_call_info()->callback();
  Address api_function_address = v8::ToCData<Address>(callback);
  ApiFunction fun(api_function_address);

  const int kApiArgc = 1;  // API function gets reference to the v8::Arguments.

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  __ PrepareCallApiFunction(kApiArgc + kApiStackSpace);

  __ mov(ApiParameterOperand(1), eax);  // v8::Arguments::implicit_args_.
  __ add(Operand(eax), Immediate(argc * kPointerSize));
  __ mov(ApiParameterOperand(2), eax);  // v8::Arguments::values_.
  __ Set(ApiParameterOperand(3), Immediate(argc));  // v8::Arguments::length_.
  // v8::Arguments::is_construct_call_.
  __ Set(ApiParameterOperand(4), Immediate(0));

  // v8::InvocationCallback's argument.
  __ lea(eax, ApiParameterOperand(1));
  __ mov(ApiParameterOperand(0), eax);

  // Emitting a stub call may try to allocate (if the code is not
  // already generated).  Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
  return masm->TryCallApiFunctionAndReturn(&fun,
                                           argc + kFastApiCallArguments + 1);
}


class CallInterceptorCompiler BASE_EMBEDDED {
 public:
  CallInterceptorCompiler(StubCompiler* stub_compiler,
                          const ParameterCount& arguments,
                          Register name,
                          Code::ExtraICState extra_ic_state)
      : stub_compiler_(stub_compiler),
        arguments_(arguments),
        name_(name),
        extra_ic_state_(extra_ic_state) {}

  MaybeObject* Compile(MacroAssembler* masm,
                       JSObject* object,
                       JSObject* holder,
                       String* name,
                       LookupResult* lookup,
                       Register receiver,
                       Register scratch1,
                       Register scratch2,
                       Register scratch3,
                       Label* miss) {
    ASSERT(holder->HasNamedInterceptor());
    ASSERT(!holder->GetNamedInterceptor()->getter()->IsUndefined());

    // Check that the receiver isn't a smi.
    __ JumpIfSmi(receiver, miss);

    CallOptimization optimization(lookup);

    if (optimization.is_constant_call()) {
      return CompileCacheable(masm,
                              object,
                              receiver,
                              scratch1,
                              scratch2,
                              scratch3,
                              holder,
                              lookup,
                              name,
                              optimization,
                              miss);
    } else {
      CompileRegular(masm,
                     object,
                     receiver,
                     scratch1,
                     scratch2,
                     scratch3,
                     name,
                     holder,
                     miss);
      return masm->isolate()->heap()->undefined_value();  // Success.
    }
  }

 private:
  MaybeObject* CompileCacheable(MacroAssembler* masm,
                                JSObject* object,
                                Register receiver,
                                Register scratch1,
                                Register scratch2,
                                Register scratch3,
                                JSObject* interceptor_holder,
                                LookupResult* lookup,
                                String* name,
                                const CallOptimization& optimization,
                                Label* miss_label) {
    ASSERT(optimization.is_constant_call());
    ASSERT(!lookup->holder()->IsGlobalObject());

    int depth1 = kInvalidProtoDepth;
    int depth2 = kInvalidProtoDepth;
    bool can_do_fast_api_call = false;
    if (optimization.is_simple_api_call() &&
        !lookup->holder()->IsGlobalObject()) {
      depth1 =
          optimization.GetPrototypeDepthOfExpectedType(object,
                                                       interceptor_holder);
      if (depth1 == kInvalidProtoDepth) {
        depth2 =
            optimization.GetPrototypeDepthOfExpectedType(interceptor_holder,
                                                         lookup->holder());
      }
      can_do_fast_api_call = (depth1 != kInvalidProtoDepth) ||
                             (depth2 != kInvalidProtoDepth);
    }

    Counters* counters = masm->isolate()->counters();
    __ IncrementCounter(counters->call_const_interceptor(), 1);

    if (can_do_fast_api_call) {
      __ IncrementCounter(counters->call_const_interceptor_fast_api(), 1);
      ReserveSpaceForFastApiCall(masm, scratch1);
    }

    // Check that the maps from receiver to interceptor's holder
    // haven't changed and thus we can invoke interceptor.
    Label miss_cleanup;
    Label* miss = can_do_fast_api_call ? &miss_cleanup : miss_label;
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver,
                                        interceptor_holder, scratch1,
                                        scratch2, scratch3, name, depth1, miss);

    // Invoke an interceptor and if it provides a value,
    // branch to |regular_invoke|.
    Label regular_invoke;
    LoadWithInterceptor(masm, receiver, holder, interceptor_holder,
                        &regular_invoke);

    // Interceptor returned nothing for this property.  Try to use cached
    // constant function.

    // Check that the maps from interceptor's holder to constant function's
    // holder haven't changed and thus we can use cached constant function.
    if (interceptor_holder != lookup->holder()) {
      stub_compiler_->CheckPrototypes(interceptor_holder, receiver,
                                      lookup->holder(), scratch1,
                                      scratch2, scratch3, name, depth2, miss);
    } else {
      // CheckPrototypes has a side effect of fetching a 'holder'
      // for API (object which is instanceof for the signature).  It's
      // safe to omit it here, as if present, it should be fetched
      // by the previous CheckPrototypes.
      ASSERT(depth2 == kInvalidProtoDepth);
    }

    // Invoke function.
    if (can_do_fast_api_call) {
      MaybeObject* result =
          GenerateFastApiCall(masm, optimization, arguments_.immediate());
      if (result->IsFailure()) return result;
    } else {
      CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
          ? CALL_AS_FUNCTION
          : CALL_AS_METHOD;
      __ InvokeFunction(optimization.constant_function(), arguments_,
                        JUMP_FUNCTION, NullCallWrapper(), call_kind);
    }

    // Deferred code for fast API call case---clean preallocated space.
    if (can_do_fast_api_call) {
      __ bind(&miss_cleanup);
      FreeSpaceForFastApiCall(masm, scratch1);
      __ jmp(miss_label);
    }

    // Invoke a regular function.
    __ bind(&regular_invoke);
    if (can_do_fast_api_call) {
      FreeSpaceForFastApiCall(masm, scratch1);
    }

    return masm->isolate()->heap()->undefined_value();  // Success.
  }

  void CompileRegular(MacroAssembler* masm,
                      JSObject* object,
                      Register receiver,
                      Register scratch1,
                      Register scratch2,
                      Register scratch3,
                      String* name,
                      JSObject* interceptor_holder,
                      Label* miss_label) {
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, scratch3, name,
                                        miss_label);

    __ EnterInternalFrame();
    // Save the name_ register across the call.
    __ push(name_);

    PushInterceptorArguments(masm,
                             receiver,
                             holder,
                             name_,
                             interceptor_holder);

    __ CallExternalReference(
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForCall),
                          masm->isolate()),
        5);

    // Restore the name_ register.
    __ pop(name_);
    __ LeaveInternalFrame();
  }

  void LoadWithInterceptor(MacroAssembler* masm,
                           Register receiver,
                           Register holder,
                           JSObject* holder_obj,
                           Label* interceptor_succeeded) {
    __ EnterInternalFrame();
    __ push(holder);  // Save the holder.
    __ push(name_);  // Save the name.

    CompileCallLoadPropertyWithInterceptor(masm,
                                           receiver,
                                           holder,
                                           name_,
                                           holder_obj);

    __ pop(name_);  // Restore the name.
    __ pop(receiver);  // Restore the holder.
    __ LeaveInternalFrame();

    __ cmp(eax, masm->isolate()->factory()->no_interceptor_result_sentinel());
    __ j(not_equal, interceptor_succeeded);
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
  Code::ExtraICState extra_ic_state_;
};


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  ASSERT(kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC);
  Code* code = NULL;
  if (kind == Code::LOAD_IC) {
    code = masm->isolate()->builtins()->builtin(Builtins::kLoadIC_Miss);
  } else {
    code = masm->isolate()->builtins()->builtin(Builtins::kKeyedLoadIC_Miss);
  }

  Handle<Code> ic(code);
  __ jmp(ic, RelocInfo::CODE_TARGET);
}


void StubCompiler::GenerateKeyedLoadMissForceGeneric(MacroAssembler* masm) {
  Code* code = masm->isolate()->builtins()->builtin(
      Builtins::kKeyedLoadIC_MissForceGeneric);
  Handle<Code> ic(code);
  __ jmp(ic, RelocInfo::CODE_TARGET);
}


// Both name_reg and receiver_reg are preserved on jumps to miss_label,
// but may be destroyed if store is successful.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      JSObject* object,
                                      int index,
                                      Map* transition,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch,
                                      Label* miss_label) {
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver_reg, miss_label);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(receiver_reg, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, miss_label);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver_reg, scratch, miss_label);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if ((transition != NULL) && (object->map()->unused_property_fields() == 0)) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ pop(scratch);  // Return address.
    __ push(receiver_reg);
    __ push(Immediate(Handle<Map>(transition)));
    __ push(eax);
    __ push(scratch);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  if (transition != NULL) {
    // Update the map of the object; no write barrier updating is
    // needed because the map is never in new space.
    __ mov(FieldOperand(receiver_reg, HeapObject::kMapOffset),
           Immediate(Handle<Map>(transition)));
  }

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ mov(FieldOperand(receiver_reg, offset), eax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ mov(name_reg, Operand(eax));
    __ RecordWrite(receiver_reg, offset, name_reg, scratch);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ mov(scratch, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ mov(FieldOperand(scratch, offset), eax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ mov(name_reg, Operand(eax));
    __ RecordWrite(scratch, offset, name_reg, receiver_reg);
  }

  // Return the value (register eax).
  __ ret(0);
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
MUST_USE_RESULT static MaybeObject* GenerateCheckPropertyCell(
    MacroAssembler* masm,
    GlobalObject* global,
    String* name,
    Register scratch,
    Label* miss) {
  Object* probe;
  { MaybeObject* maybe_probe = global->EnsurePropertyCell(name);
    if (!maybe_probe->ToObject(&probe)) return maybe_probe;
  }
  JSGlobalPropertyCell* cell = JSGlobalPropertyCell::cast(probe);
  ASSERT(cell->value()->IsTheHole());
  if (Serializer::enabled()) {
    __ mov(scratch, Immediate(Handle<Object>(cell)));
    __ cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
           Immediate(masm->isolate()->factory()->the_hole_value()));
  } else {
    __ cmp(Operand::Cell(Handle<JSGlobalPropertyCell>(cell)),
           Immediate(masm->isolate()->factory()->the_hole_value()));
  }
  __ j(not_equal, miss);
  return cell;
}


// Calls GenerateCheckPropertyCell for each global object in the prototype chain
// from object to (but not including) holder.
MUST_USE_RESULT static MaybeObject* GenerateCheckPropertyCells(
    MacroAssembler* masm,
    JSObject* object,
    JSObject* holder,
    String* name,
    Register scratch,
    Label* miss) {
  JSObject* current = object;
  while (current != holder) {
    if (current->IsGlobalObject()) {
      // Returns a cell or a failure.
      MaybeObject* result = GenerateCheckPropertyCell(
          masm,
          GlobalObject::cast(current),
          name,
          scratch,
          miss);
      if (result->IsFailure()) return result;
    }
    ASSERT(current->IsJSObject());
    current = JSObject::cast(current->GetPrototype());
  }
  return NULL;
}


#undef __
#define __ ACCESS_MASM(masm())


Register StubCompiler::CheckPrototypes(JSObject* object,
                                       Register object_reg,
                                       JSObject* holder,
                                       Register holder_reg,
                                       Register scratch1,
                                       Register scratch2,
                                       String* name,
                                       int save_at_depth,
                                       Label* miss) {
  // Make sure there's no overlap between holder and object registers.
  ASSERT(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  ASSERT(!scratch2.is(object_reg) && !scratch2.is(holder_reg)
         && !scratch2.is(scratch1));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  JSObject* current = object;
  int depth = 0;

  if (save_at_depth == depth) {
    __ mov(Operand(esp, kPointerSize), reg);
  }

  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (current != holder) {
    depth++;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());

    ASSERT(current->GetPrototype()->IsJSObject());
    JSObject* prototype = JSObject::cast(current->GetPrototype());
    if (!current->HasFastProperties() &&
        !current->IsJSGlobalObject() &&
        !current->IsJSGlobalProxy()) {
      if (!name->IsSymbol()) {
        MaybeObject* maybe_lookup_result = heap()->LookupSymbol(name);
        Object* lookup_result = NULL;  // Initialization to please compiler.
        if (!maybe_lookup_result->ToObject(&lookup_result)) {
          set_failure(Failure::cast(maybe_lookup_result));
          return reg;
        }
        name = String::cast(lookup_result);
      }
      ASSERT(current->property_dictionary()->FindEntry(name) ==
             StringDictionary::kNotFound);

      MaybeObject* negative_lookup = GenerateDictionaryNegativeLookup(masm(),
                                                                      miss,
                                                                      reg,
                                                                      name,
                                                                      scratch1,
                                                                      scratch2);
      if (negative_lookup->IsFailure()) {
        set_failure(Failure::cast(negative_lookup));
        return reg;
      }

      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // from now the object is in holder_reg
      __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else if (heap()->InNewSpace(prototype)) {
      // Get the map of the current object.
      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      __ cmp(Operand(scratch1), Immediate(Handle<Map>(current->map())));
      // Branch on the result of the map check.
      __ j(not_equal, miss);
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch1, miss);

        // Restore scratch register to be the map of the object.
        // We load the prototype from the map in the scratch register.
        __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      // The prototype is in new space; we cannot store a reference
      // to it in the code. Load it from the map.
      reg = holder_reg;  // from now the object is in holder_reg
      __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      // Check the map of the current object.
      __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
             Immediate(Handle<Map>(current->map())));
      // Branch on the result of the map check.
      __ j(not_equal, miss);
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch1, miss);
      }
      // The prototype is in old space; load it directly.
      reg = holder_reg;  // from now the object is in holder_reg
      __ mov(reg, Handle<JSObject>(prototype));
    }

    if (save_at_depth == depth) {
      __ mov(Operand(esp, kPointerSize), reg);
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }
  ASSERT(current == holder);

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  // Check the holder map.
  __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
         Immediate(Handle<Map>(holder->map())));
  __ j(not_equal, miss);

  // Perform security check for access to the global object.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());
  if (holder->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  };

  // If we've skipped any global objects, it's not enough to verify
  // that their maps haven't changed.  We also need to check that the
  // property cell for the property is still empty.
  MaybeObject* result = GenerateCheckPropertyCells(masm(),
                                                   object,
                                                   holder,
                                                   name,
                                                   scratch1,
                                                   miss);
  if (result->IsFailure()) set_failure(Failure::cast(result));

  // Return the register containing the holder.
  return reg;
}


void StubCompiler::GenerateLoadField(JSObject* object,
                                     JSObject* holder,
                                     Register receiver,
                                     Register scratch1,
                                     Register scratch2,
                                     Register scratch3,
                                     int index,
                                     String* name,
                                     Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check the prototype chain.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, scratch3, name, miss);

  // Get the value from the properties.
  GenerateFastPropertyLoad(masm(), eax, reg, holder, index);
  __ ret(0);
}


MaybeObject* StubCompiler::GenerateLoadCallback(JSObject* object,
                                                JSObject* holder,
                                                Register receiver,
                                                Register name_reg,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3,
                                                AccessorInfo* callback,
                                                String* name,
                                                Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder, scratch1,
                      scratch2, scratch3, name, miss);

  Handle<AccessorInfo> callback_handle(callback);

  // Insert additional parameters into the stack frame above return address.
  ASSERT(!scratch3.is(reg));
  __ pop(scratch3);  // Get return address to place it below.

  __ push(receiver);  // receiver
  __ mov(scratch2, Operand(esp));
  ASSERT(!scratch2.is(reg));
  __ push(reg);  // holder
  // Push data from AccessorInfo.
  if (isolate()->heap()->InNewSpace(callback_handle->data())) {
    __ mov(scratch1, Immediate(callback_handle));
    __ push(FieldOperand(scratch1, AccessorInfo::kDataOffset));
  } else {
    __ push(Immediate(Handle<Object>(callback_handle->data())));
  }

  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const AccessorInfo& to the C++ callback.
  __ push(scratch2);

  __ push(name_reg);  // name
  __ mov(ebx, esp);  // esp points to reference to name (handler).

  __ push(scratch3);  // Restore return address.

  // Do call through the api.
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);

  // 3 elements array for v8::Agruments::values_, handler for name and pointer
  // to the values (it considered as smi in GC).
  const int kStackSpace = 5;
  const int kApiArgc = 2;

  __ PrepareCallApiFunction(kApiArgc);
  __ mov(ApiParameterOperand(0), ebx);  // name.
  __ add(Operand(ebx), Immediate(kPointerSize));
  __ mov(ApiParameterOperand(1), ebx);  // arguments pointer.

  // Emitting a stub call may try to allocate (if the code is not
  // already generated).  Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
  return masm()->TryCallApiFunctionAndReturn(&fun, kStackSpace);
}


void StubCompiler::GenerateLoadConstant(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Object* value,
                                        String* name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  CheckPrototypes(object, receiver, holder,
                  scratch1, scratch2, scratch3, name, miss);

  // Return the constant value.
  __ mov(eax, Handle<Object>(value));
  __ ret(0);
}


void StubCompiler::GenerateLoadInterceptor(JSObject* object,
                                           JSObject* interceptor_holder,
                                           LookupResult* lookup,
                                           Register receiver,
                                           Register name_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           String* name,
                                           Label* miss) {
  ASSERT(interceptor_holder->HasNamedInterceptor());
  ASSERT(!interceptor_holder->GetNamedInterceptor()->getter()->IsUndefined());

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added
  // later.
  bool compile_followup_inline = false;
  if (lookup->IsProperty() && lookup->IsCacheable()) {
    if (lookup->type() == FIELD) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
        lookup->GetCallbackObject()->IsAccessorInfo() &&
        AccessorInfo::cast(lookup->GetCallbackObject())->getter() != NULL) {
      compile_followup_inline = true;
    }
  }

  if (compile_followup_inline) {
    // Compile the interceptor call, followed by inline code to load the
    // property from further up the prototype chain if the call fails.
    // Check that the maps haven't changed.
    Register holder_reg = CheckPrototypes(object, receiver, interceptor_holder,
                                          scratch1, scratch2, scratch3,
                                          name, miss);
    ASSERT(holder_reg.is(receiver) || holder_reg.is(scratch1));

    // Save necessary data before invoking an interceptor.
    // Requires a frame to make GC aware of pushed pointers.
    __ EnterInternalFrame();

    if (lookup->type() == CALLBACKS && !receiver.is(holder_reg)) {
      // CALLBACKS case needs a receiver to be passed into C++ callback.
      __ push(receiver);
    }
    __ push(holder_reg);
    __ push(name_reg);

    // Invoke an interceptor.  Note: map checks from receiver to
    // interceptor's holder has been compiled before (see a caller
    // of this method.)
    CompileCallLoadPropertyWithInterceptor(masm(),
                                           receiver,
                                           holder_reg,
                                           name_reg,
                                           interceptor_holder);

    // Check if interceptor provided a value for property.  If it's
    // the case, return immediately.
    Label interceptor_failed;
    __ cmp(eax, factory()->no_interceptor_result_sentinel());
    __ j(equal, &interceptor_failed);
    __ LeaveInternalFrame();
    __ ret(0);

    __ bind(&interceptor_failed);
    __ pop(name_reg);
    __ pop(holder_reg);
    if (lookup->type() == CALLBACKS && !receiver.is(holder_reg)) {
      __ pop(receiver);
    }

    __ LeaveInternalFrame();

    // Check that the maps from interceptor's holder to lookup's holder
    // haven't changed.  And load lookup's holder into holder_reg.
    if (interceptor_holder != lookup->holder()) {
      holder_reg = CheckPrototypes(interceptor_holder,
                                   holder_reg,
                                   lookup->holder(),
                                   scratch1,
                                   scratch2,
                                   scratch3,
                                   name,
                                   miss);
    }

    if (lookup->type() == FIELD) {
      // We found FIELD property in prototype chain of interceptor's holder.
      // Retrieve a field from field's holder.
      GenerateFastPropertyLoad(masm(), eax, holder_reg,
                               lookup->holder(), lookup->GetFieldIndex());
      __ ret(0);
    } else {
      // We found CALLBACKS property in prototype chain of interceptor's
      // holder.
      ASSERT(lookup->type() == CALLBACKS);
      ASSERT(lookup->GetCallbackObject()->IsAccessorInfo());
      AccessorInfo* callback = AccessorInfo::cast(lookup->GetCallbackObject());
      ASSERT(callback != NULL);
      ASSERT(callback->getter() != NULL);

      // Tail call to runtime.
      // Important invariant in CALLBACKS case: the code above must be
      // structured to never clobber |receiver| register.
      __ pop(scratch2);  // return address
      __ push(receiver);
      __ push(holder_reg);
      __ mov(holder_reg, Immediate(Handle<AccessorInfo>(callback)));
      __ push(FieldOperand(holder_reg, AccessorInfo::kDataOffset));
      __ push(holder_reg);
      __ push(name_reg);
      __ push(scratch2);  // restore return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            masm()->isolate());
      __ TailCallExternalReference(ref, 5, 1);
    }
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    Register holder_reg =
        CheckPrototypes(object, receiver, interceptor_holder,
                        scratch1, scratch2, scratch3, name, miss);
    __ pop(scratch2);  // save old return address
    PushInterceptorArguments(masm(), receiver, holder_reg,
                             name_reg, interceptor_holder);
    __ push(scratch2);  // restore old return address

    ExternalReference ref =
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForLoad),
                          isolate());
    __ TailCallExternalReference(ref, 5, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(String* name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ cmp(Operand(ecx), Immediate(Handle<String>(name)));
    __ j(not_equal, miss);
  }
}


void CallStubCompiler::GenerateGlobalReceiverCheck(JSObject* object,
                                                   JSObject* holder,
                                                   String* name,
                                                   Label* miss) {
  ASSERT(holder->IsGlobalObject());

  // Get the number of arguments.
  const int argc = arguments().immediate();

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(edx, miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, edx, holder, ebx, eax, edi, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(JSGlobalPropertyCell* cell,
                                                    JSFunction* function,
                                                    Label* miss) {
  // Get the value from the cell.
  if (Serializer::enabled()) {
    __ mov(edi, Immediate(Handle<JSGlobalPropertyCell>(cell)));
    __ mov(edi, FieldOperand(edi, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(edi, Operand::Cell(Handle<JSGlobalPropertyCell>(cell)));
  }

  // Check that the cell contains the same function.
  if (isolate()->heap()->InNewSpace(function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(edi, miss);
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ebx);
    __ j(not_equal, miss);

    // Check the shared function info. Make sure it hasn't changed.
    __ cmp(FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset),
           Immediate(Handle<SharedFunctionInfo>(function->shared())));
    __ j(not_equal, miss);
  } else {
    __ cmp(Operand(edi), Immediate(Handle<JSFunction>(function)));
    __ j(not_equal, miss);
  }
}


MaybeObject* CallStubCompiler::GenerateMissBranch() {
  MaybeObject* maybe_obj =
      isolate()->stub_cache()->ComputeCallMiss(arguments().immediate(),
                                               kind_,
                                               extra_ic_state_);
  Object* obj;
  if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  __ jmp(Handle<Code>(Code::cast(obj)), RelocInfo::CODE_TARGET);
  return obj;
}


MUST_USE_RESULT MaybeObject* CallStubCompiler::CompileCallField(
    JSObject* object,
    JSObject* holder,
    int index,
    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, edx, holder, ebx, eax, edi,
                                 name, &miss);

  GenerateFastPropertyLoad(masm(), edi, reg, holder, index);

  // Check that the function really is a function.
  __ JumpIfSmi(edi, &miss);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(FIELD, name);
}


MaybeObject* CallStubCompiler::CompileArrayPushCall(Object* object,
                                                    JSObject* holder,
                                                    JSGlobalPropertyCell* cell,
                                                    JSFunction* function,
                                                    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) {
    return isolate()->heap()->undefined_value();
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);

  CheckPrototypes(JSObject::cast(object), edx,
                  holder, ebx,
                  eax, edi, name, &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    Label call_builtin;

    // Get the elements array of the object.
    __ mov(ebx, FieldOperand(edx, JSArray::kElementsOffset));

    // Check that the elements are in fast mode and writable.
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(factory()->fixed_array_map()));
    __ j(not_equal, &call_builtin);

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label exit, with_write_barrier, attempt_to_grow_elements;

      // Get the array's length into eax and calculate new length.
      __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ add(Operand(eax), Immediate(Smi::FromInt(argc)));

      // Get the element's length into ecx.
      __ mov(ecx, FieldOperand(ebx, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmp(eax, Operand(ecx));
      __ j(greater, &attempt_to_grow_elements);

      // Save new length.
      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      // Push the element.
      __ lea(edx, FieldOperand(ebx,
                               eax, times_half_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ mov(ecx, Operand(esp, argc * kPointerSize));
      __ mov(Operand(edx, 0), ecx);

      // Check if value is a smi.
      __ JumpIfNotSmi(ecx, &with_write_barrier);

      __ bind(&exit);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ InNewSpace(ebx, ecx, equal, &exit);

      __ RecordWriteHelper(ebx, edx, ecx);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&attempt_to_grow_elements);
      if (!FLAG_inline_new) {
        __ jmp(&call_builtin);
      }

      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(isolate());
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(isolate());

      const int kAllocationDelta = 4;
      // Load top.
      __ mov(ecx, Operand::StaticVariable(new_space_allocation_top));

      // Check if it's the end of elements.
      __ lea(edx, FieldOperand(ebx,
                               eax, times_half_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ cmp(edx, Operand(ecx));
      __ j(not_equal, &call_builtin);
      __ add(Operand(ecx), Immediate(kAllocationDelta * kPointerSize));
      __ cmp(ecx, Operand::StaticVariable(new_space_allocation_limit));
      __ j(above, &call_builtin);

      // We fit and could grow elements.
      __ mov(Operand::StaticVariable(new_space_allocation_top), ecx);
      __ mov(ecx, Operand(esp, argc * kPointerSize));

      // Push the argument...
      __ mov(Operand(edx, 0), ecx);
      // ... and fill the rest with holes.
      for (int i = 1; i < kAllocationDelta; i++) {
        __ mov(Operand(edx, i * kPointerSize),
               Immediate(factory()->the_hole_value()));
      }

      // Restore receiver to edx as finish sequence assumes it's here.
      __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ add(FieldOperand(ebx, FixedArray::kLengthOffset),
             Immediate(Smi::FromInt(kAllocationDelta)));
      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      // Elements are in new space, so write barrier is not required.
      __ ret((argc + 1) * kPointerSize);
    }

    __ bind(&call_builtin);
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate()),
        argc + 1,
        1);
  }

  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileArrayPopCall(Object* object,
                                                   JSObject* holder,
                                                   JSGlobalPropertyCell* cell,
                                                   JSFunction* function,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) {
    return heap()->undefined_value();
  }

  Label miss, return_undefined, call_builtin;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);
  CheckPrototypes(JSObject::cast(object), edx,
                  holder, ebx,
                  eax, edi, name, &miss);

  // Get the elements array of the object.
  __ mov(ebx, FieldOperand(edx, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(factory()->fixed_array_map()));
  __ j(not_equal, &call_builtin);

  // Get the array's length into ecx and calculate new length.
  __ mov(ecx, FieldOperand(edx, JSArray::kLengthOffset));
  __ sub(Operand(ecx), Immediate(Smi::FromInt(1)));
  __ j(negative, &return_undefined);

  // Get the last element.
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(eax, FieldOperand(ebx,
                           ecx, times_half_pointer_size,
                           FixedArray::kHeaderSize));
  __ cmp(Operand(eax), Immediate(factory()->the_hole_value()));
  __ j(equal, &call_builtin);

  // Set the array's length.
  __ mov(FieldOperand(edx, JSArray::kLengthOffset), ecx);

  // Fill with the hole.
  __ mov(FieldOperand(ebx,
                      ecx, times_half_pointer_size,
                      FixedArray::kHeaderSize),
         Immediate(factory()->the_hole_value()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&return_undefined);
  __ mov(eax, Immediate(factory()->undefined_value()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&call_builtin);
  __ TailCallExternalReference(
      ExternalReference(Builtins::c_ArrayPop, isolate()),
      argc + 1,
      1);

  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileStringCharCodeAtCall(
    Object* object,
    JSObject* holder,
    JSGlobalPropertyCell* cell,
    JSFunction* function,
    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || cell != NULL) {
    return isolate()->heap()->undefined_value();
  }

  const int argc = arguments().immediate();

  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;

  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_ic_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }

  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            eax,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                  ebx, edx, edi, name, &miss);

  Register receiver = ebx;
  Register index = edi;
  Register scratch = edx;
  Register result = eax;
  __ mov(receiver, Operand(esp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ mov(index, Operand(esp, (argc - 0) * kPointerSize));
  } else {
    __ Set(index, Immediate(factory()->undefined_value()));
  }

  StringCharCodeAtGenerator char_code_at_generator(receiver,
                                                   index,
                                                   scratch,
                                                   result,
                                                   &miss,  // When not a string.
                                                   &miss,  // When not a number.
                                                   index_out_of_range_label,
                                                   STRING_INDEX_IS_NUMBER);
  char_code_at_generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  StubRuntimeCallHelper call_helper;
  char_code_at_generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ Set(eax, Immediate(factory()->nan_value()));
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in ecx.
  __ Set(ecx, Immediate(Handle<String>(name)));
  __ bind(&name_miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileStringCharAtCall(
    Object* object,
    JSObject* holder,
    JSGlobalPropertyCell* cell,
    JSFunction* function,
    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || cell != NULL) {
    return heap()->undefined_value();
  }

  const int argc = arguments().immediate();

  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;

  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_ic_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }

  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            eax,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                  ebx, edx, edi, name, &miss);

  Register receiver = eax;
  Register index = edi;
  Register scratch1 = ebx;
  Register scratch2 = edx;
  Register result = eax;
  __ mov(receiver, Operand(esp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ mov(index, Operand(esp, (argc - 0) * kPointerSize));
  } else {
    __ Set(index, Immediate(factory()->undefined_value()));
  }

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch1,
                                          scratch2,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          index_out_of_range_label,
                                          STRING_INDEX_IS_NUMBER);
  char_at_generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ Set(eax, Immediate(factory()->empty_string()));
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in ecx.
  __ Set(ecx, Immediate(Handle<String>(name)));
  __ bind(&name_miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileStringFromCharCodeCall(
    Object* object,
    JSObject* holder,
    JSGlobalPropertyCell* cell,
    JSFunction* function,
    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) {
    return isolate()->heap()->undefined_value();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);

    CheckPrototypes(JSObject::cast(object), edx, holder, ebx, eax, edi, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the char code argument.
  Register code = ebx;
  __ mov(code, Operand(esp, 1 * kPointerSize));

  // Check the code is a smi.
  Label slow;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(code, &slow);

  // Convert the smi code to uint16.
  __ and_(code, Immediate(Smi::FromInt(0xffff)));

  StringCharFromCodeGenerator char_from_code_generator(code, eax);
  char_from_code_generator.GenerateFast(masm());
  __ ret(2 * kPointerSize);

  StubRuntimeCallHelper call_helper;
  char_from_code_generator.GenerateSlow(masm(), call_helper);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  __ bind(&miss);
  // ecx: function name.
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return (cell == NULL) ? GetCode(function) : GetCode(NORMAL, name);
}


MaybeObject* CallStubCompiler::CompileMathFloorCall(Object* object,
                                                    JSObject* holder,
                                                    JSGlobalPropertyCell* cell,
                                                    JSFunction* function,
                                                    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  if (!CpuFeatures::IsSupported(SSE2)) {
    return isolate()->heap()->undefined_value();
  }

  CpuFeatures::Scope use_sse2(SSE2);

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) {
    return isolate()->heap()->undefined_value();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);

    CheckPrototypes(JSObject::cast(object), edx, holder, ebx, eax, edi, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into eax.
  __ mov(eax, Operand(esp, 1 * kPointerSize));

  // Check if the argument is a smi.
  Label smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &smi);

  // Check if the argument is a heap number and load its value into xmm0.
  Label slow;
  __ CheckMap(eax, factory()->heap_number_map(), &slow, DONT_DO_SMI_CHECK);
  __ movdbl(xmm0, FieldOperand(eax, HeapNumber::kValueOffset));

  // Check if the argument is strictly positive. Note this also
  // discards NaN.
  __ xorpd(xmm1, xmm1);
  __ ucomisd(xmm0, xmm1);
  __ j(below_equal, &slow);

  // Do a truncating conversion.
  __ cvttsd2si(eax, Operand(xmm0));

  // Check if the result fits into a smi. Note this also checks for
  // 0x80000000 which signals a failed conversion.
  Label wont_fit_into_smi;
  __ test(eax, Immediate(0xc0000000));
  __ j(not_zero, &wont_fit_into_smi);

  // Smi tag and return.
  __ SmiTag(eax);
  __ bind(&smi);
  __ ret(2 * kPointerSize);

  // Check if the argument is < 2^kMantissaBits.
  Label already_round;
  __ bind(&wont_fit_into_smi);
  __ LoadPowerOf2(xmm1, ebx, HeapNumber::kMantissaBits);
  __ ucomisd(xmm0, xmm1);
  __ j(above_equal, &already_round);

  // Save a copy of the argument.
  __ movaps(xmm2, xmm0);

  // Compute (argument + 2^kMantissaBits) - 2^kMantissaBits.
  __ addsd(xmm0, xmm1);
  __ subsd(xmm0, xmm1);

  // Compare the argument and the tentative result to get the right mask:
  //   if xmm2 < xmm0:
  //     xmm2 = 1...1
  //   else:
  //     xmm2 = 0...0
  __ cmpltsd(xmm2, xmm0);

  // Subtract 1 if the argument was less than the tentative result.
  __ LoadPowerOf2(xmm1, ebx, 0);
  __ andpd(xmm1, xmm2);
  __ subsd(xmm0, xmm1);

  // Return a new heap number.
  __ AllocateHeapNumber(eax, ebx, edx, &slow);
  __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
  __ ret(2 * kPointerSize);

  // Return the argument (when it's an already round heap number).
  __ bind(&already_round);
  __ mov(eax, Operand(esp, 1 * kPointerSize));
  __ ret(2 * kPointerSize);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // ecx: function name.
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return (cell == NULL) ? GetCode(function) : GetCode(NORMAL, name);
}


MaybeObject* CallStubCompiler::CompileMathAbsCall(Object* object,
                                                  JSObject* holder,
                                                  JSGlobalPropertyCell* cell,
                                                  JSFunction* function,
                                                  String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) {
    return isolate()->heap()->undefined_value();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);

    CheckPrototypes(JSObject::cast(object), edx, holder, ebx, eax, edi, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into eax.
  __ mov(eax, Operand(esp, 1 * kPointerSize));

  // Check if the argument is a smi.
  Label not_smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(eax, &not_smi);

  // Set ebx to 1...1 (== -1) if the argument is negative, or to 0...0
  // otherwise.
  __ mov(ebx, eax);
  __ sar(ebx, kBitsPerInt - 1);

  // Do bitwise not or do nothing depending on ebx.
  __ xor_(eax, Operand(ebx));

  // Add 1 or do nothing depending on ebx.
  __ sub(eax, Operand(ebx));

  // If the result is still negative, go to the slow case.
  // This only happens for the most negative smi.
  Label slow;
  __ j(negative, &slow);

  // Smi case done.
  __ ret(2 * kPointerSize);

  // Check if the argument is a heap number and load its exponent and
  // sign into ebx.
  __ bind(&not_smi);
  __ CheckMap(eax, factory()->heap_number_map(), &slow, DONT_DO_SMI_CHECK);
  __ mov(ebx, FieldOperand(eax, HeapNumber::kExponentOffset));

  // Check the sign of the argument. If the argument is positive,
  // just return it.
  Label negative_sign;
  __ test(ebx, Immediate(HeapNumber::kSignMask));
  __ j(not_zero, &negative_sign);
  __ ret(2 * kPointerSize);

  // If the argument is negative, clear the sign, and return a new
  // number.
  __ bind(&negative_sign);
  __ and_(ebx, ~HeapNumber::kSignMask);
  __ mov(ecx, FieldOperand(eax, HeapNumber::kMantissaOffset));
  __ AllocateHeapNumber(eax, edi, edx, &slow);
  __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), ebx);
  __ mov(FieldOperand(eax, HeapNumber::kMantissaOffset), ecx);
  __ ret(2 * kPointerSize);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // ecx: function name.
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return (cell == NULL) ? GetCode(function) : GetCode(NORMAL, name);
}


MaybeObject* CallStubCompiler::CompileFastApiCall(
    const CallOptimization& optimization,
    Object* object,
    JSObject* holder,
    JSGlobalPropertyCell* cell,
    JSFunction* function,
    String* name) {
  ASSERT(optimization.is_simple_api_call());
  // Bail out if object is a global object as we don't want to
  // repatch it to global receiver.
  if (object->IsGlobalObject()) return heap()->undefined_value();
  if (cell != NULL) return heap()->undefined_value();
  if (!object->IsJSObject()) return heap()->undefined_value();
  int depth = optimization.GetPrototypeDepthOfExpectedType(
            JSObject::cast(object), holder);
  if (depth == kInvalidProtoDepth) return heap()->undefined_value();

  Label miss, miss_before_stack_reserved;

  GenerateNameCheck(name, &miss_before_stack_reserved);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss_before_stack_reserved);

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_const(), 1);
  __ IncrementCounter(counters->call_const_fast_api(), 1);

  // Allocate space for v8::Arguments implicit values. Must be initialized
  // before calling any runtime function.
  __ sub(Operand(esp), Immediate(kFastApiCallArguments * kPointerSize));

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(JSObject::cast(object), edx, holder,
                  ebx, eax, edi, name, depth, &miss);

  // Move the return address on top of the stack.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), eax);

  // esp[2 * kPointerSize] is uninitialized, esp[3 * kPointerSize] contains
  // duplicate of return address and will be overwritten.
  MaybeObject* result = GenerateFastApiCall(masm(), optimization, argc);
  if (result->IsFailure()) return result;

  __ bind(&miss);
  __ add(Operand(esp), Immediate(kFastApiCallArguments * kPointerSize));

  __ bind(&miss_before_stack_reserved);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileCallConstant(
    Object* object,
    JSObject* holder,
    JSFunction* function,
    String* name,
    CheckType check) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  if (HasCustomCallGenerator(function)) {
    MaybeObject* maybe_result = CompileCustomCall(
        object, holder, NULL, function, name);
    Object* result;
    if (!maybe_result->ToObject(&result)) return maybe_result;
    // undefined means bail out to regular compiler.
    if (!result->IsUndefined()) return result;
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(edx, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  SharedFunctionInfo* function_info = function->shared();
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(isolate()->counters()->call_const(), 1);

      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, eax, edi, name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
        __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
      }
      break;

    case STRING_CHECK:
      if (!function->IsBuiltin() && !function_info->strict_mode()) {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      } else {
        // Check that the object is a string or a symbol.
        __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, eax);
        __ j(above_equal, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;

    case NUMBER_CHECK: {
      if (!function->IsBuiltin() && !function_info->strict_mode()) {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      } else {
        Label fast;
        // Check that the object is a smi or a heap number.
        __ JumpIfSmi(edx, &fast);
        __ CmpObjectType(edx, HEAP_NUMBER_TYPE, eax);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;
    }

    case BOOLEAN_CHECK: {
      if (!function->IsBuiltin() && !function_info->strict_mode()) {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      } else {
        Label fast;
        // Check that the object is a boolean.
        __ cmp(edx, factory()->true_value());
        __ j(equal, &fast);
        __ cmp(edx, factory()->false_value());
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;
    }

    default:
      UNREACHABLE();
  }

  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileCallInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), ecx, extra_ic_state_);
  MaybeObject* result = compiler.Compile(masm(),
                                         object,
                                         holder,
                                         name,
                                         &lookup,
                                         edx,
                                         ebx,
                                         edi,
                                         eax,
                                         &miss);
  if (result->IsFailure()) return result;

  // Restore receiver.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the function really is a function.
  __ JumpIfSmi(eax, &miss);
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  __ mov(edi, eax);
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle load cache miss.
  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* CallStubCompiler::CompileCallGlobal(
    JSObject* object,
    GlobalObject* holder,
    JSGlobalPropertyCell* cell,
    JSFunction* function,
    String* name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  if (HasCustomCallGenerator(function)) {
    MaybeObject* maybe_result = CompileCustomCall(
        object, holder, cell, function, name);
    Object* result;
    if (!maybe_result->ToObject(&result)) return maybe_result;
    // undefined means bail out to regular compiler.
    if (!result->IsUndefined()) return result;
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();

  GenerateGlobalReceiverCheck(object, holder, name, &miss);

  GenerateLoadFunctionFromCell(cell, function, &miss);

  // Patch the receiver on the stack with the global proxy.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Setup the context (function already in edi).
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_global_inline(), 1);
  ASSERT(function->is_compiled());
  ParameterCount expected(function->shared()->formal_parameter_count());
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  if (V8::UseCrankshaft()) {
    // TODO(kasperl): For now, we always call indirectly through the
    // code field in the function to allow recompilation to take effect
    // without changing any of the call sites.
    __ InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
                  expected, arguments(), JUMP_FUNCTION,
                  NullCallWrapper(), call_kind);
  } else {
    Handle<Code> code(function->code());
    __ InvokeCode(code, expected, arguments(),
                  RelocInfo::CODE_TARGET, JUMP_FUNCTION,
                  NullCallWrapper(), call_kind);
  }

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* StoreStubCompiler::CompileStoreField(JSObject* object,
                                                  int index,
                                                  Map* transition,
                                                  String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     edx, ecx, ebx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(ecx, Immediate(Handle<String>(name)));  // restore name
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* StoreStubCompiler::CompileStoreCallback(JSObject* object,
                                                     AccessorInfo* callback,
                                                     String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(edx, &miss);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, &miss);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(edx, ebx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ pop(ebx);  // remove the return address
  __ push(edx);  // receiver
  __ push(Immediate(Handle<AccessorInfo>(callback)));  // callback info
  __ push(ecx);  // name
  __ push(eax);  // value
  __ push(ebx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty), isolate());
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                        String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(edx, &miss);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(receiver->map())));
  __ j(not_equal, &miss);

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(edx, ebx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ pop(ebx);  // remove the return address
  __ push(edx);  // receiver
  __ push(ecx);  // name
  __ push(eax);  // value
  __ push(Immediate(Smi::FromInt(strict_mode_)));
  __ push(ebx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
                                                   JSGlobalPropertyCell* cell,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, &miss);


  // Compute the cell operand to use.
  Operand cell_operand = Operand::Cell(Handle<JSGlobalPropertyCell>(cell));
  if (Serializer::enabled()) {
    __ mov(ebx, Immediate(Handle<JSGlobalPropertyCell>(cell)));
    cell_operand = FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset);
  }

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ cmp(cell_operand, factory()->the_hole_value());
  __ j(equal, &miss);

  // Store the value in the cell.
  __ mov(cell_operand, eax);

  // Return the value (register eax).
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_store_global_inline(), 1);
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->named_store_global_inline_miss(), 1);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                       int index,
                                                       Map* transition,
                                                       String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_store_field(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(ecx), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     edx, ecx, ebx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_store_field(), 1);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_jsarray = receiver_map->instance_type() == JS_ARRAY_TYPE;
  MaybeObject* maybe_stub =
      KeyedStoreElementStub(is_jsarray, elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(edx,
                 Handle<Map>(receiver_map),
                 Handle<Code>(stub),
                 DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreMegamorphic(
    MapList* receiver_maps,
    CodeList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(edx, &miss);

  Register map_reg = ebx;
  __ mov(map_reg, FieldOperand(edx, HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map(receiver_maps->at(current));
    __ cmp(map_reg, map);
    __ j(equal, Handle<Code>(handler_ics->at(current)));
  }
  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


MaybeObject* LoadStubCompiler::CompileLoadNonexistent(String* name,
                                                      JSObject* object,
                                                      JSObject* last) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(eax, &miss);

  ASSERT(last->IsGlobalObject() || last->HasFastProperties());

  // Check the maps of the full prototype chain. Also check that
  // global property cells up to (but not including) the last object
  // in the prototype chain are empty.
  CheckPrototypes(object, eax, last, ebx, edx, edi, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    MaybeObject* cell = GenerateCheckPropertyCell(masm(),
                                                  GlobalObject::cast(last),
                                                  name,
                                                  edx,
                                                  &miss);
    if (cell->IsFailure()) {
      miss.Unuse();
      return cell;
    }
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ mov(eax, isolate()->factory()->undefined_value());
  __ ret(0);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NONEXISTENT, isolate()->heap()->empty_string());
}


MaybeObject* LoadStubCompiler::CompileLoadField(JSObject* object,
                                                JSObject* holder,
                                                int index,
                                                String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadField(object, holder, eax, ebx, edx, edi, index, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


MaybeObject* LoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* object,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  MaybeObject* result = GenerateLoadCallback(object, holder, eax, ecx, ebx, edx,
                                             edi, callback, name, &miss);
  if (result->IsFailure()) {
    miss.Unuse();
    return result;
  }

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* LoadStubCompiler::CompileLoadConstant(JSObject* object,
                                                   JSObject* holder,
                                                   Object* value,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadConstant(object, holder, eax, ebx, edx, edi, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


MaybeObject* LoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          eax,
                          ecx,
                          edx,
                          ebx,
                          edi,
                          name,
                          &miss);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* LoadStubCompiler::CompileLoadGlobal(JSObject* object,
                                                 GlobalObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 String* name,
                                                 bool is_dont_delete) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual loads. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(eax, &miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, eax, holder, ebx, edx, edi, name, &miss);

  // Get the value from the cell.
  if (Serializer::enabled()) {
    __ mov(ebx, Immediate(Handle<JSGlobalPropertyCell>(cell)));
    __ mov(ebx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(ebx, Operand::Cell(Handle<JSGlobalPropertyCell>(cell)));
  }

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ cmp(ebx, factory()->the_hole_value());
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ cmp(ebx, factory()->the_hole_value());
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1);
  __ mov(eax, ebx);
  __ ret(0);

  __ bind(&miss);
  __ IncrementCounter(counters->named_load_global_stub_miss(), 1);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                     JSObject* receiver,
                                                     JSObject* holder,
                                                     int index) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_field(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  GenerateLoadField(receiver, holder, edx, ebx, ecx, edi, index, name, &miss);

  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_field(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadCallback(
    String* name,
    JSObject* receiver,
    JSObject* holder,
    AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_callback(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  MaybeObject* result = GenerateLoadCallback(receiver, holder, edx, eax, ebx,
                                             ecx, edi, callback, name, &miss);
  if (result->IsFailure()) {
    miss.Unuse();
    return result;
  }

  __ bind(&miss);

  __ DecrementCounter(counters->keyed_load_callback(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                        JSObject* receiver,
                                                        JSObject* holder,
                                                        Object* value) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_constant_function(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  GenerateLoadConstant(receiver, holder, edx, ebx, ecx, edi,
                       value, name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_constant_function(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                           JSObject* holder,
                                                           String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_interceptor(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          edx,
                          eax,
                          ecx,
                          ebx,
                          edi,
                          name,
                          &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_interceptor(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_array_length(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  GenerateLoadArrayLength(masm(), edx, ecx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_array_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  GenerateLoadStringLength(masm(), edx, ecx, ebx, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss);

  GenerateLoadFunctionPrototype(masm(), edx, ecx, ebx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  MaybeObject* maybe_stub = KeyedLoadElementStub(elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(edx,
                 Handle<Map>(receiver_map),
                 Handle<Code>(stub),
                 DO_SMI_CHECK);

  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, NULL);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadMegamorphic(
    MapList* receiver_maps,
    CodeList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(edx, &miss);

  Register map_reg = ebx;
  __ mov(map_reg, FieldOperand(edx, HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map(receiver_maps->at(current));
    __ cmp(map_reg, map);
    __ j(equal, Handle<Code>(handler_ics->at(current)));
  }

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
MaybeObject* ConstructStubCompiler::CompileConstructStub(JSFunction* function) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Label generic_stub_call;
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Check to see whether there are any break points in the function code. If
  // there are jump to the generic constructor stub which calls the actual
  // code for the function thereby hitting the break points.
  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx, FieldOperand(ebx, SharedFunctionInfo::kDebugInfoOffset));
  __ cmp(ebx, factory()->undefined_value());
  __ j(not_equal, &generic_stub_call);
#endif

  // Load the initial map and verify that it is in fact a map.
  __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
  // Will both indicate a NULL and a Smi.
  __ JumpIfSmi(ebx, &generic_stub_call);
  __ CmpObjectType(ebx, MAP_TYPE, ecx);
  __ j(not_equal, &generic_stub_call);

#ifdef DEBUG
  // Cannot construct functions this way.
  // edi: constructor
  // ebx: initial map
  __ CmpInstanceType(ebx, JS_FUNCTION_TYPE);
  __ Assert(not_equal, "Function constructed by construct stub.");
#endif

  // Now allocate the JSObject on the heap by moving the new space allocation
  // top forward.
  // edi: constructor
  // ebx: initial map
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInstanceSizeOffset));
  __ shl(ecx, kPointerSizeLog2);
  __ AllocateInNewSpace(ecx,
                        edx,
                        ecx,
                        no_reg,
                        &generic_stub_call,
                        NO_ALLOCATION_FLAGS);

  // Allocated the JSObject, now initialize the fields and add the heap tag.
  // ebx: initial map
  // edx: JSObject (untagged)
  __ mov(Operand(edx, JSObject::kMapOffset), ebx);
  __ mov(ebx, factory()->empty_fixed_array());
  __ mov(Operand(edx, JSObject::kPropertiesOffset), ebx);
  __ mov(Operand(edx, JSObject::kElementsOffset), ebx);

  // Push the allocated object to the stack. This is the object that will be
  // returned (after it is tagged).
  __ push(edx);

  // eax: argc
  // edx: JSObject (untagged)
  // Load the address of the first in-object property into edx.
  __ lea(edx, Operand(edx, JSObject::kHeaderSize));
  // Calculate the location of the first argument. The stack contains the
  // allocated object and the return address on top of the argc arguments.
  __ lea(ecx, Operand(esp, eax, times_4, 1 * kPointerSize));

  // Use edi for holding undefined which is used in several places below.
  __ mov(edi, factory()->undefined_value());

  // eax: argc
  // ecx: first argument
  // edx: first in-object property of the JSObject
  // edi: undefined
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
  SharedFunctionInfo* shared = function->shared();
  for (int i = 0; i < shared->this_property_assignments_count(); i++) {
    if (shared->IsThisPropertyAssignmentArgument(i)) {
      // Check if the argument assigned to the property is actually passed.
      // If argument is not passed the property is set to undefined,
      // otherwise find it on the stack.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ mov(ebx, edi);
      __ cmp(eax, arg_number);
      if (CpuFeatures::IsSupported(CMOV)) {
        CpuFeatures::Scope use_cmov(CMOV);
        __ cmov(above, ebx, Operand(ecx, arg_number * -kPointerSize));
      } else {
        Label not_passed;
        __ j(below_equal, &not_passed);
        __ mov(ebx, Operand(ecx, arg_number * -kPointerSize));
        __ bind(&not_passed);
      }
      // Store value in the property.
      __ mov(Operand(edx, i * kPointerSize), ebx);
    } else {
      // Set the property to the constant value.
      Handle<Object> constant(shared->GetThisPropertyAssignmentConstant(i));
      __ mov(Operand(edx, i * kPointerSize), Immediate(constant));
    }
  }

  // Fill the unused in-object property fields with undefined.
  ASSERT(function->has_initial_map());
  for (int i = shared->this_property_assignments_count();
       i < function->initial_map()->inobject_properties();
       i++) {
    __ mov(Operand(edx, i * kPointerSize), edi);
  }

  // Move argc to ebx and retrieve and tag the JSObject to return.
  __ mov(ebx, eax);
  __ pop(eax);
  __ or_(Operand(eax), Immediate(kHeapObjectTag));

  // Remove caller arguments and receiver from the stack and return.
  __ pop(ecx);
  __ lea(esp, Operand(esp, ebx, times_pointer_size, 1 * kPointerSize));
  __ push(ecx);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1);
  __ IncrementCounter(counters->constructed_objects_stub(), 1);
  __ ret(0);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Handle<Code> generic_construct_stub =
      isolate()->builtins()->JSConstructStubGeneric();
  __ jmp(generic_construct_stub, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.
  __ JumpIfNotSmi(eax, &miss_force_generic);
  __ mov(ebx, eax);
  __ SmiUntag(ebx);
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));

  // Push receiver on the stack to free up a register for the dictionary
  // probing.
  __ push(edx);
  __ LoadFromNumberDictionary(&slow,
                              ecx,
                              eax,
                              ebx,
                              edx,
                              edi,
                              eax);
  // Pop receiver before returning.
  __ pop(edx);
  __ ret(0);

  __ bind(&slow);
  __ pop(edx);

  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(slow_ic, RelocInfo::CODE_TARGET);

  __ bind(&miss_force_generic);
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> miss_force_generic_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_force_generic_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, failed_allocation, slow;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(eax, &miss_force_generic);

  // Check that the index is in range.
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(eax, FieldOperand(ebx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &miss_force_generic);
  __ mov(ebx, FieldOperand(ebx, ExternalArray::kExternalPointerOffset));
  // ebx: base pointer of external storage
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
      __ SmiUntag(eax);  // Untag the index.
      __ movsx_b(eax, Operand(ebx, eax, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      __ SmiUntag(eax);  // Untag the index.
      __ movzx_b(eax, Operand(ebx, eax, times_1, 0));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
      __ movsx_w(eax, Operand(ebx, eax, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ movzx_w(eax, Operand(ebx, eax, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
      __ mov(ecx, Operand(ebx, eax, times_2, 0));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      __ fld_s(Operand(ebx, eax, times_2, 0));
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ fld_d(Operand(ebx, eax, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }

  // For integer array types:
  // ecx: value
  // For floating-point array type:
  // FP(0): value

  if (elements_kind == EXTERNAL_INT_ELEMENTS ||
      elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    if (elements_kind == EXTERNAL_INT_ELEMENTS) {
      __ cmp(ecx, 0xC0000000);
      __ j(sign, &box_int);
    } else {
      ASSERT_EQ(EXTERNAL_UNSIGNED_INT_ELEMENTS, elements_kind);
      // The test is different for unsigned int values. Since we need
      // the value to be in the range of a positive smi, we can't
      // handle either of the top two bits being set in the value.
      __ test(ecx, Immediate(0xC0000000));
      __ j(not_zero, &box_int);
    }

    __ mov(eax, ecx);
    __ SmiTag(eax);
    __ ret(0);

    __ bind(&box_int);

    // Allocate a HeapNumber for the int and perform int-to-double
    // conversion.
    if (elements_kind == EXTERNAL_INT_ELEMENTS) {
      __ push(ecx);
      __ fild_s(Operand(esp, 0));
      __ pop(ecx);
    } else {
      ASSERT_EQ(EXTERNAL_UNSIGNED_INT_ELEMENTS, elements_kind);
      // Need to zero-extend the value.
      // There's no fild variant for unsigned values, so zero-extend
      // to a 64-bit int manually.
      __ push(Immediate(0));
      __ push(ecx);
      __ fild_d(Operand(esp, 0));
      __ pop(ecx);
      __ pop(ecx);
    }
    // FP(0): value
    __ AllocateHeapNumber(ecx, ebx, edi, &failed_allocation);
    // Set the value.
    __ mov(eax, ecx);
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
             elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    __ AllocateHeapNumber(ecx, ebx, edi, &failed_allocation);
    // Set the value.
    __ mov(eax, ecx);
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else {
    __ SmiTag(eax);
    __ ret(0);
  }

  // If we fail allocation of the HeapNumber, we still have a value on
  // top of the FPU stack. Remove it.
  __ bind(&failed_allocation);
  __ ffree();
  __ fincstp();
  // Fall through to slow case.

  // Slow case: Jump to runtime.
  __ bind(&slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_external_array_slow(), 1);

  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> ic = masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  // Miss case: Jump to runtime.
  __ bind(&miss_force_generic);
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, slow, check_heap_number;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(ecx, &miss_force_generic);

  // Check that the index is in range.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(ecx, FieldOperand(edi, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &slow);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // eax: value
  // edx: receiver
  // ecx: key
  // edi: elements array
  if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
    __ JumpIfNotSmi(eax, &slow);
  } else {
    __ JumpIfNotSmi(eax, &check_heap_number);
  }

  // smi case
  __ mov(ebx, eax);  // Preserve the value in eax as the return value.
  __ SmiUntag(ebx);
  __ mov(edi, FieldOperand(edi, ExternalArray::kExternalPointerOffset));
  // edi: base pointer of external storage
  switch (elements_kind) {
    case EXTERNAL_PIXEL_ELEMENTS:
      __ ClampUint8(ebx);
      __ SmiUntag(ecx);
      __ mov_b(Operand(edi, ecx, times_1, 0), ebx);
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ SmiUntag(ecx);
      __ mov_b(Operand(edi, ecx, times_1, 0), ebx);
      break;
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ mov_w(Operand(edi, ecx, times_1, 0), ebx);
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ mov(Operand(edi, ecx, times_2, 0), ebx);
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
      // Need to perform int-to-float conversion.
      __ push(ebx);
      __ fild_s(Operand(esp, 0));
      __ pop(ebx);
      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
        __ fstp_s(Operand(edi, ecx, times_2, 0));
      } else {  // elements_kind == EXTERNAL_DOUBLE_ELEMENTS.
        __ fstp_d(Operand(edi, ecx, times_4, 0));
      }
      break;
    default:
      UNREACHABLE();
      break;
  }
  __ ret(0);  // Return the original value.

  // TODO(danno): handle heap number -> pixel array conversion
  if (elements_kind != EXTERNAL_PIXEL_ELEMENTS) {
    __ bind(&check_heap_number);
    // eax: value
    // edx: receiver
    // ecx: key
    // edi: elements array
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
    __ j(not_equal, &slow);

    // The WebGL specification leaves the behavior of storing NaN and
    // +/-Infinity into integer arrays basically undefined. For more
    // reproducible behavior, convert these to zero.
    __ mov(edi, FieldOperand(edi, ExternalArray::kExternalPointerOffset));
    // edi: base pointer of external storage
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
      __ fstp_s(Operand(edi, ecx, times_2, 0));
      __ ret(0);
    } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
      __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
      __ fstp_d(Operand(edi, ecx, times_4, 0));
      __ ret(0);
    } else {
      // Perform float-to-int conversion with truncation (round-to-zero)
      // behavior.

      // For the moment we make the slow call to the runtime on
      // processors that don't support SSE2. The code in IntegerConvert
      // (code-stubs-ia32.cc) is roughly what is needed here though the
      // conversion failure case does not need to be handled.
      if (CpuFeatures::IsSupported(SSE2)) {
        if (elements_kind != EXTERNAL_INT_ELEMENTS &&
            elements_kind != EXTERNAL_UNSIGNED_INT_ELEMENTS) {
          ASSERT(CpuFeatures::IsSupported(SSE2));
          CpuFeatures::Scope scope(SSE2);
          __ cvttsd2si(ebx, FieldOperand(eax, HeapNumber::kValueOffset));
          // ecx: untagged integer value
          switch (elements_kind) {
            case EXTERNAL_PIXEL_ELEMENTS:
              __ ClampUint8(ebx);
              // Fall through.
            case EXTERNAL_BYTE_ELEMENTS:
            case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
              __ SmiUntag(ecx);
              __ mov_b(Operand(edi, ecx, times_1, 0), ebx);
              break;
            case EXTERNAL_SHORT_ELEMENTS:
            case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
              __ mov_w(Operand(edi, ecx, times_1, 0), ebx);
              break;
            default:
              UNREACHABLE();
              break;
          }
        } else {
          if (CpuFeatures::IsSupported(SSE3)) {
            CpuFeatures::Scope scope(SSE3);
            // fisttp stores values as signed integers. To represent the
            // entire range of int and unsigned int arrays, store as a
            // 64-bit int and discard the high 32 bits.
            // If the value is NaN or +/-infinity, the result is 0x80000000,
            // which is automatically zero when taken mod 2^n, n < 32.
            __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
            __ sub(Operand(esp), Immediate(2 * kPointerSize));
            __ fisttp_d(Operand(esp, 0));
            __ pop(ebx);
            __ add(Operand(esp), Immediate(kPointerSize));
          } else {
            ASSERT(CpuFeatures::IsSupported(SSE2));
            CpuFeatures::Scope scope(SSE2);
            // We can easily implement the correct rounding behavior for the
            // range [0, 2^31-1]. For the time being, to keep this code simple,
            // make the slow runtime call for values outside this range.
            // Note: we could do better for signed int arrays.
            __ movd(xmm0, FieldOperand(eax, HeapNumber::kValueOffset));
            // We will need the key if we have to make the slow runtime call.
            __ push(ebx);
            __ LoadPowerOf2(xmm1, ebx, 31);
            __ pop(ebx);
            __ ucomisd(xmm1, xmm0);
            __ j(above_equal, &slow);
            __ cvttsd2si(ebx, Operand(xmm0));
          }
          // ebx: untagged integer value
          __ mov(Operand(edi, ecx, times_2, 0), ebx);
        }
        __ ret(0);  // Return original value.
      }
    }
  }

  // Slow case: call runtime.
  __ bind(&slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_store_external_array_slow(), 1);

  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> ic = masm->isolate()->builtins()->KeyedStoreIC_Slow();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ bind(&miss_force_generic);
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastElement(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(eax, &miss_force_generic);

  // Get the elements array.
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(ecx);

  // Check that the key is within bounds.
  __ cmp(eax, FieldOperand(ecx, FixedArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Load the result and make sure it's not the hole.
  __ mov(ebx, Operand(ecx, eax, times_2,
                      FixedArray::kHeaderSize - kHeapObjectTag));
  __ cmp(ebx, masm->isolate()->factory()->the_hole_value());
  __ j(equal, &miss_force_generic);
  __ mov(eax, ebx);
  __ ret(0);

  __ bind(&miss_force_generic);
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastDoubleElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, slow_allocate_heapnumber;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(eax, &miss_force_generic);

  // Get the elements array.
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(ecx);

  // Check that the key is within bounds.
  __ cmp(eax, FieldOperand(ecx, FixedDoubleArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Check for the hole
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(ecx, eax, times_4, offset), Immediate(kHoleNanUpper32));
  __ j(equal, &miss_force_generic);

  // Always allocate a heap number for the result.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(xmm0, FieldOperand(ecx, eax, times_4,
                                 FixedDoubleArray::kHeaderSize));
  } else {
    __ fld_d(FieldOperand(ecx, eax, times_4, FixedDoubleArray::kHeaderSize));
  }
  __ AllocateHeapNumber(ecx, ebx, edi, &slow_allocate_heapnumber);
  // Set the value.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm0);
  } else {
    __ fstp_d(FieldOperand(ecx, HeapNumber::kValueOffset));
  }
  __ mov(eax, ecx);
  __ ret(0);

  __ bind(&slow_allocate_heapnumber);
  // A value was pushed on the floating point stack before the allocation, if
  // the allocation fails it needs to be removed.
  if (!CpuFeatures::IsSupported(SSE2)) {
    __ ffree();
    __ fincstp();
  }
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(slow_ic, RelocInfo::CODE_TARGET);

  __ bind(&miss_force_generic);
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreFastElement(MacroAssembler* masm,
                                                      bool is_js_array) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(ecx, &miss_force_generic);

  // Get the elements array and make sure it is a fast element array, not 'cow'.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(FieldOperand(edi, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_array_map()));
  __ j(not_equal, &miss_force_generic);

  if (is_js_array) {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // smis.
    __ j(above_equal, &miss_force_generic);
  } else {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edi, FixedArray::kLengthOffset));  // smis.
    __ j(above_equal, &miss_force_generic);
  }

  // Do the store and update the write barrier. Make sure to preserve
  // the value in register eax.
  __ mov(edx, Operand(eax));
  __ mov(FieldOperand(edi, ecx, times_2, FixedArray::kHeaderSize), eax);
  __ RecordWrite(edi, 0, edx, ecx);

  // Done.
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, smi_value, is_nan, maybe_nan;
  Label have_double_value, not_nan;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(ecx, &miss_force_generic);

  // Get the elements array.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(edi);

  if (is_js_array) {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // smis.
  } else {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edi, FixedArray::kLengthOffset));  // smis.
  }
  __ j(above_equal, &miss_force_generic);

  __ JumpIfSmi(eax, &smi_value, Label::kNear);

  __ CheckMap(eax,
              masm->isolate()->factory()->heap_number_map(),
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  // Double value, canonicalize NaN.
  uint32_t offset = HeapNumber::kValueOffset + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(eax, offset), Immediate(kNaNOrInfinityLowerBoundUpper32));
  __ j(greater_equal, &maybe_nan, Label::kNear);

  __ bind(&not_nan);
  ExternalReference canonical_nan_reference =
      ExternalReference::address_of_canonical_non_hole_nan();
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(xmm0, FieldOperand(eax, HeapNumber::kValueOffset));
    __ bind(&have_double_value);
    __ movdbl(FieldOperand(edi, ecx, times_4, FixedDoubleArray::kHeaderSize),
              xmm0);
    __ ret(0);
  } else {
    __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ bind(&have_double_value);
    __ fstp_d(FieldOperand(edi, ecx, times_4, FixedDoubleArray::kHeaderSize));
    __ ret(0);
  }

  __ bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  __ j(greater, &is_nan, Label::kNear);
  __ cmp(FieldOperand(eax, HeapNumber::kValueOffset), Immediate(0));
  __ j(zero, &not_nan);
  __ bind(&is_nan);
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(xmm0, Operand::StaticVariable(canonical_nan_reference));
  } else {
    __ fld_d(Operand::StaticVariable(canonical_nan_reference));
  }
  __ jmp(&have_double_value, Label::kNear);

  __ bind(&smi_value);
  // Value is a smi. convert to a double and store.
  // Preserve original value.
  __ mov(edx, eax);
  __ SmiUntag(edx);
  __ push(edx);
  __ fild_s(Operand(esp, 0));
  __ pop(edx);
  __ fstp_d(FieldOperand(edi, ecx, times_4, FixedDoubleArray::kHeaderSize));
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
