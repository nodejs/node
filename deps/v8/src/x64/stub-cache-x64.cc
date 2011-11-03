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

#if defined(V8_TARGET_ARCH_X64)

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
                       Register offset) {
  ASSERT_EQ(8, kPointerSize);
  ASSERT_EQ(16, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  Label miss;

  __ LoadAddress(kScratchRegister, key_offset);
  // Check that the key in the entry matches the name.
  // Multiply entry offset by 16 to get the entry address. Since the
  // offset register already holds the entry offset times four, multiply
  // by a further four.
  __ cmpl(name, Operand(kScratchRegister, offset, times_4, 0));
  __ j(not_equal, &miss);
  // Get the code entry from the cache.
  // Use key_offset + kPointerSize, rather than loading value_offset.
  __ movq(kScratchRegister,
          Operand(kScratchRegister, offset, times_4, kPointerSize));
  // Check that the flags match what we're looking for.
  __ movl(offset, FieldOperand(kScratchRegister, Code::kFlagsOffset));
  __ and_(offset, Immediate(~Code::kFlagsNotUsedInLookup));
  __ cmpl(offset, Immediate(flags));
  __ j(not_equal, &miss);

  // Jump to the first instruction in the code stub.
  __ addq(kScratchRegister, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(kScratchRegister);

  __ bind(&miss);
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be a symbol and receiver must be a heap object.
MUST_USE_RESULT static MaybeObject* GenerateDictionaryNegativeLookup(
    MacroAssembler* masm,
    Label* miss_label,
    Register receiver,
    String* name,
    Register r0,
    Register r1) {
  ASSERT(name->IsSymbol());
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1);

  __ movq(r0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ testb(FieldOperand(r0, Map::kBitFieldOffset),
           Immediate(kInterceptorOrAccessCheckNeededMask));
  __ j(not_zero, miss_label);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(r0, FIRST_SPEC_OBJECT_TYPE);
  __ j(below, miss_label);

  // Load properties array.
  Register properties = r0;
  __ movq(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ CompareRoot(FieldOperand(properties, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, miss_label);

  Label done;
  MaybeObject* result = StringDictionaryLookupStub::GenerateNegativeLookup(
      masm,
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
  Isolate* isolate = masm->isolate();
  Label miss;
  USE(extra);   // The register extra is not used on the X64 platform.
  USE(extra2);  // The register extra2 is not used on the X64 platform.
  // Make sure that code is valid. The shifting code relies on the
  // entry size being 16.
  ASSERT(sizeof(Entry) == 16);

  // Make sure the flags do not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));

  // Check scratch register is valid, extra and extra2 are unused.
  ASSERT(!scratch.is(no_reg));
  ASSERT(extra2.is(no_reg));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ movq(prototype,
             Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  __ movq(prototype,
             FieldOperand(prototype, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  __ movq(prototype, Operand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ movq(prototype,
             FieldOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ movq(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
  Isolate* isolate = masm->isolate();
  // Check we're still in the same context.
  __ Move(prototype, isolate->global());
  __ cmpq(Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)),
          prototype);
  __ j(not_equal, miss);
  // Get the global function with the given index.
  JSFunction* function =
      JSFunction::cast(isolate->global_context()->get(index));
  // Load its initial map. The global functions all have initial maps.
  __ Move(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ movq(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
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
  __ movq(rax, FieldOperand(receiver, JSArray::kLengthOffset));
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
  __ movq(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movzxbq(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ testl(scratch, Immediate(kNotStringTag));
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

  // Load length directly from the string.
  __ movq(rax, FieldOperand(receiver, String::kLengthOffset));
  __ ret(0);

  if (support_wrappers) {
    // Check if the object is a JSValue wrapper.
    __ bind(&check_wrapper);
    __ cmpl(scratch1, Immediate(JS_VALUE_TYPE));
    __ j(not_equal, miss);

    // Check if the wrapped value is a string and load the length
    // directly if it is.
    __ movq(scratch2, FieldOperand(receiver, JSValue::kValueOffset));
    GenerateStringCheck(masm, scratch2, scratch1, miss, miss);
    __ movq(rax, FieldOperand(scratch2, String::kLengthOffset));
    __ ret(0);
  }
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register result,
                                                 Register scratch,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, result, miss_label);
  if (!result.is(rax)) __ movq(rax, result);
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
    __ movq(dst, FieldOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    __ movq(dst, FieldOperand(src, JSObject::kPropertiesOffset));
    __ movq(dst, FieldOperand(dst, offset));
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
  __ Move(kScratchRegister, Handle<Object>(interceptor));
  __ push(kScratchRegister);
  __ push(receiver);
  __ push(holder);
  __ push(FieldOperand(kScratchRegister, InterceptorInfo::kDataOffset));
}


static void CompileCallLoadPropertyWithInterceptor(MacroAssembler* masm,
                                                   Register receiver,
                                                   Register holder,
                                                   Register name,
                                                   JSObject* holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly),
                        masm->isolate());
  __ Set(rax, 5);
  __ LoadAddress(rbx, ref);

  CEntryStub stub(1);
  __ CallStub(&stub);
}


// Number of pointers to be reserved on stack for fast API call.
static const int kFastApiCallArguments = 3;


// Reserves space for the extra arguments to API function in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument in the internal frame of the caller
  // -----------------------------------
  __ movq(scratch, Operand(rsp, 0));
  __ subq(rsp, Immediate(kFastApiCallArguments * kPointerSize));
  __ movq(Operand(rsp, 0), scratch);
  __ Move(scratch, Smi::FromInt(0));
  for (int i = 1; i <= kFastApiCallArguments; i++) {
     __ movq(Operand(rsp, i * kPointerSize), scratch);
  }
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address.
  //  -- rsp[8]  : last fast api call extra argument.
  //  -- ...
  //  -- rsp[kFastApiCallArguments * 8] : first fast api call extra argument.
  //  -- rsp[kFastApiCallArguments * 8 + 8] : last argument in the internal
  //                                          frame.
  // -----------------------------------
  __ movq(scratch, Operand(rsp, 0));
  __ movq(Operand(rsp, kFastApiCallArguments * kPointerSize), scratch);
  __ addq(rsp, Immediate(kPointerSize * kFastApiCallArguments));
}


// Generates call to API function.
static MaybeObject* GenerateFastApiCall(MacroAssembler* masm,
                                        const CallOptimization& optimization,
                                        int argc) {
  // ----------- S t a t e -------------
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : object passing the type check
  //                           (last fast api call extra argument,
  //                            set by CheckPrototypes)
  //  -- rsp[16]             : api function
  //                           (first fast api call extra argument)
  //  -- rsp[24]             : api call data
  //  -- rsp[32]             : last argument
  //  -- ...
  //  -- rsp[(argc + 3) * 8] : first argument
  //  -- rsp[(argc + 4) * 8] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  JSFunction* function = optimization.constant_function();
  __ Move(rdi, Handle<JSFunction>(function));
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Pass the additional arguments.
  __ movq(Operand(rsp, 2 * kPointerSize), rdi);
  Object* call_data = optimization.api_call_info()->data();
  Handle<CallHandlerInfo> api_call_info_handle(optimization.api_call_info());
  if (masm->isolate()->heap()->InNewSpace(call_data)) {
    __ Move(rcx, api_call_info_handle);
    __ movq(rbx, FieldOperand(rcx, CallHandlerInfo::kDataOffset));
    __ movq(Operand(rsp, 3 * kPointerSize), rbx);
  } else {
    __ Move(Operand(rsp, 3 * kPointerSize), Handle<Object>(call_data));
  }

  // Prepare arguments.
  __ lea(rbx, Operand(rsp, 3 * kPointerSize));

  Object* callback = optimization.api_call_info()->callback();
  Address api_function_address = v8::ToCData<Address>(callback);
  ApiFunction fun(api_function_address);

#ifdef _WIN64
  // Win64 uses first register--rcx--for returned value.
  Register arguments_arg = rdx;
#else
  Register arguments_arg = rdi;
#endif

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  __ PrepareCallApiFunction(kApiStackSpace);

  __ movq(StackSpaceOperand(0), rbx);  // v8::Arguments::implicit_args_.
  __ addq(rbx, Immediate(argc * kPointerSize));
  __ movq(StackSpaceOperand(1), rbx);  // v8::Arguments::values_.
  __ Set(StackSpaceOperand(2), argc);  // v8::Arguments::length_.
  // v8::Arguments::is_construct_call_.
  __ Set(StackSpaceOperand(3), 0);

  // v8::InvocationCallback's argument.
  __ lea(arguments_arg, StackSpaceOperand(0));
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
      MaybeObject* result = GenerateFastApiCall(masm,
                                                optimization,
                                                arguments_.immediate());
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

    __ CompareRoot(rax, Heap::kNoInterceptorResultSentinelRootIndex);
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
  __ Jump(ic, RelocInfo::CODE_TARGET);
}


void StubCompiler::GenerateKeyedLoadMissForceGeneric(MacroAssembler* masm) {
  Code* code = masm->isolate()->builtins()->builtin(
      Builtins::kKeyedLoadIC_MissForceGeneric);
  Handle<Code> ic(code);
  __ Jump(ic, RelocInfo::CODE_TARGET);
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
  __ Cmp(FieldOperand(receiver_reg, HeapObject::kMapOffset),
         Handle<Map>(object->map()));
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
    __ Push(Handle<Map>(transition));
    __ push(rax);
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
    __ Move(FieldOperand(receiver_reg, HeapObject::kMapOffset),
            Handle<Map>(transition));
  }

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ movq(FieldOperand(receiver_reg, offset), rax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ movq(name_reg, rax);
    __ RecordWrite(receiver_reg, offset, name_reg, scratch);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ movq(scratch, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ movq(FieldOperand(scratch, offset), rax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ movq(name_reg, rax);
    __ RecordWrite(scratch, offset, name_reg, receiver_reg);
  }

  // Return the value (register rax).
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
  __ Move(scratch, Handle<Object>(cell));
  __ Cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
         masm->isolate()->factory()->the_hole_value());
  __ j(not_equal, miss);
  return cell;
}


#undef __
#define __ ACCESS_MASM((masm()))


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

  // Keep track of the current object in register reg.  On the first
  // iteration, reg is an alias for object_reg, on later iterations,
  // it is an alias for holder_reg.
  Register reg = object_reg;
  int depth = 0;

  if (save_at_depth == depth) {
    __ movq(Operand(rsp, kPointerSize), object_reg);
  }

  // Check the maps in the prototype chain.
  // Traverse the prototype chain from the object and do map checks.
  JSObject* current = object;
  while (current != holder) {
    depth++;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());

    JSObject* prototype = JSObject::cast(current->GetPrototype());
    if (!current->HasFastProperties() &&
        !current->IsJSGlobalObject() &&
        !current->IsJSGlobalProxy()) {
      if (!name->IsSymbol()) {
        MaybeObject* lookup_result = heap()->LookupSymbol(name);
        if (lookup_result->IsFailure()) {
          set_failure(Failure::cast(lookup_result));
          return reg;
        } else {
          name = String::cast(lookup_result->ToObjectUnchecked());
        }
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

      __ movq(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // from now the object is in holder_reg
      __ movq(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else if (heap()->InNewSpace(prototype)) {
      // Get the map of the current object.
      __ movq(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      __ Cmp(scratch1, Handle<Map>(current->map()));
      // Branch on the result of the map check.
      __ j(not_equal, miss);
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch1, miss);

        // Restore scratch register to be the map of the object.
        // We load the prototype from the map in the scratch register.
        __ movq(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      // The prototype is in new space; we cannot store a reference
      // to it in the code. Load it from the map.
      reg = holder_reg;  // from now the object is in holder_reg
      __ movq(reg, FieldOperand(scratch1, Map::kPrototypeOffset));

    } else {
      // Check the map of the current object.
      __ Cmp(FieldOperand(reg, HeapObject::kMapOffset),
          Handle<Map>(current->map()));
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
      __ Move(reg, Handle<JSObject>(prototype));
    }

    if (save_at_depth == depth) {
      __ movq(Operand(rsp, kPointerSize), reg);
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }

  // Check the holder map.
  __ Cmp(FieldOperand(reg, HeapObject::kMapOffset), Handle<Map>(holder->map()));
  __ j(not_equal, miss);

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  // Perform security check for access to the global object and return
  // the holder register.
  ASSERT(current == holder);
  ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());
  if (current->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  }

  // If we've skipped any global objects, it's not enough to verify
  // that their maps haven't changed.  We also need to check that the
  // property cell for the property is still empty.
  current = object;
  while (current != holder) {
    if (current->IsGlobalObject()) {
      MaybeObject* cell = GenerateCheckPropertyCell(masm(),
                                                    GlobalObject::cast(current),
                                                    name,
                                                    scratch1,
                                                    miss);
      if (cell->IsFailure()) {
        set_failure(Failure::cast(cell));
        return reg;
      }
    }
    current = JSObject::cast(current->GetPrototype());
  }

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
  GenerateFastPropertyLoad(masm(), rax, reg, holder, index);
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
  ASSERT(!scratch2.is(reg));
  __ pop(scratch2);  // Get return address to place it below.

  __ push(receiver);  // receiver
  __ push(reg);  // holder
  if (heap()->InNewSpace(callback_handle->data())) {
    __ Move(scratch1, callback_handle);
    __ push(FieldOperand(scratch1, AccessorInfo::kDataOffset));  // data
  } else {
    __ Push(Handle<Object>(callback_handle->data()));
  }
  __ push(name_reg);  // name
  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const AccessorInfo& to the C++ callback.

#ifdef _WIN64
  // Win64 uses first register--rcx--for returned value.
  Register accessor_info_arg = r8;
  Register name_arg = rdx;
#else
  Register accessor_info_arg = rsi;
  Register name_arg = rdi;
#endif

  ASSERT(!name_arg.is(scratch2));
  __ movq(name_arg, rsp);
  __ push(scratch2);  // Restore return address.

  // Do call through the api.
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);

  // 3 elements array for v8::Agruments::values_ and handler for name.
  const int kStackSpace = 4;

  // Allocate v8::AccessorInfo in non-GCed stack space.
  const int kArgStackSpace = 1;

  __ PrepareCallApiFunction(kArgStackSpace);
  __ lea(rax, Operand(name_arg, 3 * kPointerSize));

  // v8::AccessorInfo::args_.
  __ movq(StackSpaceOperand(0), rax);

  // The context register (rsi) has been saved in PrepareCallApiFunction and
  // could be used to pass arguments.
  __ lea(accessor_info_arg, StackSpaceOperand(0));

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
  __ Move(rax, Handle<Object>(value));
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
    __ CompareRoot(rax, Heap::kNoInterceptorResultSentinelRootIndex);
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
    // haven't changed.  And load lookup's holder into |holder| register.
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
      GenerateFastPropertyLoad(masm(), rax, holder_reg,
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
      __ Move(holder_reg, Handle<AccessorInfo>(callback));
      __ push(FieldOperand(holder_reg, AccessorInfo::kDataOffset));
      __ push(holder_reg);
      __ push(name_reg);
      __ push(scratch2);  // restore return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            isolate());
      __ TailCallExternalReference(ref, 5, 1);
    }
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    Register holder_reg = CheckPrototypes(object, receiver, interceptor_holder,
                                          scratch1, scratch2, scratch3,
                                          name, miss);
    __ pop(scratch2);  // save old return address
    PushInterceptorArguments(masm(), receiver, holder_reg,
                             name_reg, interceptor_holder);
    __ push(scratch2);  // restore old return address

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad), isolate());
    __ TailCallExternalReference(ref, 5, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(String* name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ Cmp(rcx, Handle<String>(name));
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
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(rdx, miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, rdx, holder, rbx, rax, rdi, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(JSGlobalPropertyCell* cell,
                                                    JSFunction* function,
                                                    Label* miss) {
  // Get the value from the cell.
  __ Move(rdi, Handle<JSGlobalPropertyCell>(cell));
  __ movq(rdi, FieldOperand(rdi, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (heap()->InNewSpace(function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(rdi, miss);
    __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rax);
    __ j(not_equal, miss);

    // Check the shared function info. Make sure it hasn't changed.
    __ Move(rax, Handle<SharedFunctionInfo>(function->shared()));
    __ cmpq(FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset), rax);
    __ j(not_equal, miss);
  } else {
    __ Cmp(rdi, Handle<JSFunction>(function));
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
  __ Jump(Handle<Code>(Code::cast(obj)), RelocInfo::CODE_TARGET);
  return obj;
}


MaybeObject* CallStubCompiler::CompileCallField(JSObject* object,
                                                JSObject* holder,
                                                int index,
                                                String* name) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, rdx, holder, rbx, rax, rdi,
                                 name, &miss);

  GenerateFastPropertyLoad(masm(), rdi, reg, holder, index);

  // Check that the function really is a function.
  __ JumpIfSmi(rdi, &miss);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION,
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
  //  -- rcx                 : name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) return heap()->undefined_value();

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  CheckPrototypes(JSObject::cast(object),
                  rdx,
                  holder,
                  rbx,
                  rax,
                  rdi,
                  name,
                  &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ movq(rax, FieldOperand(rdx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    Label call_builtin;

    // Get the elements array of the object.
    __ movq(rbx, FieldOperand(rdx, JSArray::kElementsOffset));

    // Check that the elements are in fast mode and writable.
    __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
           factory()->fixed_array_map());
    __ j(not_equal, &call_builtin);

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label exit, with_write_barrier, attempt_to_grow_elements;

      // Get the array's length into rax and calculate new length.
      __ SmiToInteger32(rax, FieldOperand(rdx, JSArray::kLengthOffset));
      STATIC_ASSERT(FixedArray::kMaxLength < Smi::kMaxValue);
      __ addl(rax, Immediate(argc));

      // Get the element's length into rcx.
      __ SmiToInteger32(rcx, FieldOperand(rbx, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmpl(rax, rcx);
      __ j(greater, &attempt_to_grow_elements);

      // Save new length.
      __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Push the element.
      __ movq(rcx, Operand(rsp, argc * kPointerSize));
      __ lea(rdx, FieldOperand(rbx,
                               rax, times_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ movq(Operand(rdx, 0), rcx);

      // Check if value is a smi.
      __ Integer32ToSmi(rax, rax);  // Return new length as smi.

      __ JumpIfNotSmi(rcx, &with_write_barrier);

      __ bind(&exit);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ InNewSpace(rbx, rcx, equal, &exit);

      __ RecordWriteHelper(rbx, rdx, rcx);

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
      __ Load(rcx, new_space_allocation_top);

      // Check if it's the end of elements.
      __ lea(rdx, FieldOperand(rbx,
                               rax, times_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ cmpq(rdx, rcx);
      __ j(not_equal, &call_builtin);
      __ addq(rcx, Immediate(kAllocationDelta * kPointerSize));
      Operand limit_operand =
          masm()->ExternalOperand(new_space_allocation_limit);
      __ cmpq(rcx, limit_operand);
      __ j(above, &call_builtin);

      // We fit and could grow elements.
      __ Store(new_space_allocation_top, rcx);
      __ movq(rcx, Operand(rsp, argc * kPointerSize));

      // Push the argument...
      __ movq(Operand(rdx, 0), rcx);
      // ... and fill the rest with holes.
      __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
      for (int i = 1; i < kAllocationDelta; i++) {
        __ movq(Operand(rdx, i * kPointerSize), kScratchRegister);
      }

      // Restore receiver to rdx as finish sequence assumes it's here.
      __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ SmiAddConstant(FieldOperand(rbx, FixedArray::kLengthOffset),
                        Smi::FromInt(kAllocationDelta));

      // Make new length a smi before returning it.
      __ Integer32ToSmi(rax, rax);
      __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Elements are in new space, so write barrier is not required.
      __ ret((argc + 1) * kPointerSize);
    }

    __ bind(&call_builtin);
    __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPush,
                                                   isolate()),
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
  //  -- rcx                 : name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) return heap()->undefined_value();

  Label miss, return_undefined, call_builtin;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  CheckPrototypes(JSObject::cast(object), rdx,
                  holder, rbx,
                  rax, rdi, name, &miss);

  // Get the elements array of the object.
  __ movq(rbx, FieldOperand(rdx, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &call_builtin);

  // Get the array's length into rcx and calculate new length.
  __ SmiToInteger32(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
  __ subl(rcx, Immediate(1));
  __ j(negative, &return_undefined);

  // Get the last element.
  __ LoadRoot(r9, Heap::kTheHoleValueRootIndex);
  __ movq(rax, FieldOperand(rbx,
                            rcx, times_pointer_size,
                            FixedArray::kHeaderSize));
  // Check if element is already the hole.
  __ cmpq(rax, r9);
  // If so, call slow-case to also check prototypes for value.
  __ j(equal, &call_builtin);

  // Set the array's length.
  __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rcx);

  // Fill with the hole and return original value.
  __ movq(FieldOperand(rbx,
                       rcx, times_pointer_size,
                       FixedArray::kHeaderSize),
          r9);
  __ ret((argc + 1) * kPointerSize);

  __ bind(&return_undefined);
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
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
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || cell != NULL) return heap()->undefined_value();

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
                                            rax,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                  rbx, rdx, rdi, name, &miss);

  Register receiver = rbx;
  Register index = rdi;
  Register scratch = rdx;
  Register result = rax;
  __ movq(receiver, Operand(rsp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ movq(index, Operand(rsp, (argc - 0) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
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
    __ LoadRoot(rax, Heap::kNanValueRootIndex);
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in rcx.
  __ Move(rcx, Handle<String>(name));
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
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || cell != NULL) return heap()->undefined_value();

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
                                            rax,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                  rbx, rdx, rdi, name, &miss);

  Register receiver = rax;
  Register index = rdi;
  Register scratch1 = rbx;
  Register scratch2 = rdx;
  Register result = rax;
  __ movq(receiver, Operand(rsp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ movq(index, Operand(rsp, (argc - 0) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
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
    __ LoadRoot(rax, Heap::kEmptyStringRootIndex);
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in rcx.
  __ Move(rcx, Handle<String>(name));
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
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return heap()->undefined_value();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ movq(rdx, Operand(rsp, 2 * kPointerSize));

    __ JumpIfSmi(rdx, &miss);

    CheckPrototypes(JSObject::cast(object), rdx, holder, rbx, rax, rdi, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the char code argument.
  Register code = rbx;
  __ movq(code, Operand(rsp, 1 * kPointerSize));

  // Check the code is a smi.
  Label slow;
  __ JumpIfNotSmi(code, &slow);

  // Convert the smi code to uint16.
  __ SmiAndConstant(code, code, Smi::FromInt(0xffff));

  StringCharFromCodeGenerator char_from_code_generator(code, rax);
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
  // rcx: function name.
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
  // TODO(872): implement this.
  return heap()->undefined_value();
}


MaybeObject* CallStubCompiler::CompileMathAbsCall(Object* object,
                                                  JSObject* holder,
                                                  JSGlobalPropertyCell* cell,
                                                  JSFunction* function,
                                                  String* name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return heap()->undefined_value();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ movq(rdx, Operand(rsp, 2 * kPointerSize));

    __ JumpIfSmi(rdx, &miss);

    CheckPrototypes(JSObject::cast(object), rdx, holder, rbx, rax, rdi, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into rax.
  __ movq(rax, Operand(rsp, 1 * kPointerSize));

  // Check if the argument is a smi.
  Label not_smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(rax, &not_smi);
  __ SmiToInteger32(rax, rax);

  // Set ebx to 1...1 (== -1) if the argument is negative, or to 0...0
  // otherwise.
  __ movl(rbx, rax);
  __ sarl(rbx, Immediate(kBitsPerInt - 1));

  // Do bitwise not or do nothing depending on ebx.
  __ xorl(rax, rbx);

  // Add 1 or do nothing depending on ebx.
  __ subl(rax, rbx);

  // If the result is still negative, go to the slow case.
  // This only happens for the most negative smi.
  Label slow;
  __ j(negative, &slow);

  // Smi case done.
  __ Integer32ToSmi(rax, rax);
  __ ret(2 * kPointerSize);

  // Check if the argument is a heap number and load its value.
  __ bind(&not_smi);
  __ CheckMap(rax, factory()->heap_number_map(), &slow, DONT_DO_SMI_CHECK);
  __ movq(rbx, FieldOperand(rax, HeapNumber::kValueOffset));

  // Check the sign of the argument. If the argument is positive,
  // just return it.
  Label negative_sign;
  const int sign_mask_shift =
      (HeapNumber::kExponentOffset - HeapNumber::kValueOffset) * kBitsPerByte;
  __ movq(rdi, static_cast<int64_t>(HeapNumber::kSignMask) << sign_mask_shift,
          RelocInfo::NONE);
  __ testq(rbx, rdi);
  __ j(not_zero, &negative_sign);
  __ ret(2 * kPointerSize);

  // If the argument is negative, clear the sign, and return a new
  // number. We still have the sign mask in rdi.
  __ bind(&negative_sign);
  __ xor_(rbx, rdi);
  __ AllocateHeapNumber(rax, rdx, &slow);
  __ movq(FieldOperand(rax, HeapNumber::kValueOffset), rbx);
  __ ret(2 * kPointerSize);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  __ bind(&miss);
  // rcx: function name.
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
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss_before_stack_reserved);

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_const(), 1);
  __ IncrementCounter(counters->call_const_fast_api(), 1);

  // Allocate space for v8::Arguments implicit values. Must be initialized
  // before calling any runtime function.
  __ subq(rsp, Immediate(kFastApiCallArguments * kPointerSize));

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(JSObject::cast(object), rdx, holder,
                  rbx, rax, rdi, name, depth, &miss);

  // Move the return address on top of the stack.
  __ movq(rax, Operand(rsp, 3 * kPointerSize));
  __ movq(Operand(rsp, 0 * kPointerSize), rax);

  MaybeObject* result = GenerateFastApiCall(masm(), optimization, argc);
  if (result->IsFailure()) return result;

  __ bind(&miss);
  __ addq(rsp, Immediate(kFastApiCallArguments * kPointerSize));

  __ bind(&miss_before_stack_reserved);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(function);
}


MaybeObject* CallStubCompiler::CompileCallConstant(Object* object,
                                                   JSObject* holder,
                                                   JSFunction* function,
                                                   String* name,
                                                   CheckType check) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
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
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(rdx, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  Counters* counters = isolate()->counters();
  SharedFunctionInfo* function_info = function->shared();
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(counters->call_const(), 1);

      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), rdx, holder,
                      rbx, rax, rdi, name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
        __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
      }
      break;

    case STRING_CHECK:
      if (!function->IsBuiltin() && !function_info->strict_mode()) {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      } else {
        // Check that the object is a two-byte string or a symbol.
        __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, rax);
        __ j(above_equal, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, rax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, rdi, name, &miss);
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
        __ JumpIfSmi(rdx, &fast);
        __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rax);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, rax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, rdi, name, &miss);
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
        __ CompareRoot(rdx, Heap::kTrueValueRootIndex);
        __ j(equal, &fast);
        __ CompareRoot(rdx, Heap::kFalseValueRootIndex);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, rax, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, rdi, name, &miss);
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
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), rcx, extra_ic_state_);
  MaybeObject* result = compiler.Compile(masm(),
                                         object,
                                         holder,
                                         name,
                                         &lookup,
                                         rdx,
                                         rbx,
                                         rdi,
                                         rax,
                                         &miss);
  if (result->IsFailure()) return result;

  // Restore receiver.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the function really is a function.
  __ JumpIfSmi(rax, &miss);
  __ CmpObjectType(rax, JS_FUNCTION_TYPE, rbx);
  __ j(not_equal, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Invoke the function.
  __ movq(rdi, rax);
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle load cache miss.
  __ bind(&miss);
  MaybeObject* maybe_result = GenerateMissBranch();
  if (maybe_result->IsFailure()) return maybe_result;

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* CallStubCompiler::CompileCallGlobal(JSObject* object,
                                                 GlobalObject* holder,
                                                 JSGlobalPropertyCell* cell,
                                                 JSFunction* function,
                                                 String* name) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
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
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Setup the context (function already in rdi).
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

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
    __ movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
    __ InvokeCode(rdx, expected, arguments(), JUMP_FUNCTION,
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
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Generate store field code.  Preserves receiver and name on jump to miss.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     rdx, rcx, rbx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* StoreStubCompiler::CompileStoreCallback(JSObject* object,
                                                     AccessorInfo* callback,
                                                     String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  // Check that the map of the object hasn't changed.
  __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
         Handle<Map>(object->map()));
  __ j(not_equal, &miss);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(rdx, rbx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ pop(rbx);  // remove the return address
  __ push(rdx);  // receiver
  __ Push(Handle<AccessorInfo>(callback));  // callback info
  __ push(rcx);  // name
  __ push(rax);  // value
  __ push(rbx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty), isolate());
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                        String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  // Check that the map of the object hasn't changed.
  __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
         Handle<Map>(receiver->map()));
  __ j(not_equal, &miss);

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(rdx, rbx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ pop(rbx);  // remove the return address
  __ push(rdx);  // receiver
  __ push(rcx);  // name
  __ push(rax);  // value
  __ Push(Smi::FromInt(strict_mode_));
  __ push(rbx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
                                                   JSGlobalPropertyCell* cell,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
         Handle<Map>(object->map()));
  __ j(not_equal, &miss);

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ Move(rbx, Handle<JSGlobalPropertyCell>(cell));
  __ CompareRoot(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset),
                 Heap::kTheHoleValueRootIndex);
  __ j(equal, &miss);

  // Store the value in the cell.
  __ movq(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset), rax);

  // Return the value (register rax).
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_store_global_inline(), 1);
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->named_store_global_inline_miss(), 1);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                       int index,
                                                       Map* transition,
                                                       String* name) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_store_field(), 1);

  // Check that the name has not changed.
  __ Cmp(rcx, Handle<String>(name));
  __ j(not_equal, &miss);

  // Generate store field code.  Preserves receiver and name on jump to miss.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     rdx, rcx, rbx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_store_field(), 1);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
  MaybeObject* maybe_stub =
      KeyedStoreElementStub(is_js_array, elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(rdx,
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
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(rdx, &miss);

  Register map_reg = rbx;
  __ movq(map_reg, FieldOperand(rdx, HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    // Check map and tail call if there's a match
    Handle<Map> map(receiver_maps->at(current));
    __ Cmp(map_reg, map);
    __ j(equal,
         Handle<Code>(handler_ics->at(current)),
         RelocInfo::CODE_TARGET);
  }

  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


MaybeObject* LoadStubCompiler::CompileLoadNonexistent(String* name,
                                                      JSObject* object,
                                                      JSObject* last) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that receiver is not a smi.
  __ JumpIfSmi(rax, &miss);

  // Check the maps of the full prototype chain. Also check that
  // global property cells up to (but not including) the last object
  // in the prototype chain are empty.
  CheckPrototypes(object, rax, last, rbx, rdx, rdi, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    MaybeObject* cell = GenerateCheckPropertyCell(masm(),
                                                  GlobalObject::cast(last),
                                                  name,
                                                  rdx,
                                                  &miss);
    if (cell->IsFailure()) {
      miss.Unuse();
      return cell;
    }
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ ret(0);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NONEXISTENT, heap()->empty_string());
}


MaybeObject* LoadStubCompiler::CompileLoadField(JSObject* object,
                                                JSObject* holder,
                                                int index,
                                                String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadField(object, holder, rax, rbx, rdx, rdi, index, name, &miss);
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
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  MaybeObject* result = GenerateLoadCallback(object, holder, rax, rcx, rdx, rbx,
                                             rdi, callback, name, &miss);
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
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadConstant(object, holder, rax, rbx, rdx, rdi, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


MaybeObject* LoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          rax,
                          rcx,
                          rdx,
                          rbx,
                          rdi,
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
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual loads. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(rax, &miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, rax, holder, rbx, rdx, rdi, name, &miss);

  // Get the value from the cell.
  __ Move(rbx, Handle<JSGlobalPropertyCell>(cell));
  __ movq(rbx, FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1);
  __ movq(rax, rbx);
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
  //  -- rax     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_field(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadField(receiver, holder, rdx, rbx, rcx, rdi, index, name, &miss);

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
  //  -- rax     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_callback(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  MaybeObject* result = GenerateLoadCallback(receiver, holder, rdx, rax, rbx,
                                             rcx, rdi, callback, name, &miss);
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
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_constant_function(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadConstant(receiver, holder, rdx, rbx, rcx, rdi,
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
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_interceptor(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          rdx,
                          rax,
                          rcx,
                          rbx,
                          rdi,
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
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_array_length(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadArrayLength(masm(), rdx, rcx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_array_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadStringLength(masm(), rdx, rcx, rbx, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadFunctionPrototype(masm(), rdx, rcx, rbx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  MaybeObject* maybe_stub = KeyedLoadElementStub(elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(rdx,
                 Handle<Map>(receiver_map),
                 Handle<Code>(stub),
                 DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadMegamorphic(
    MapList* receiver_maps,
    CodeList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(rdx, &miss);

  Register map_reg = rbx;
  __ movq(map_reg, FieldOperand(rdx, HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int current = 0; current < receiver_count; ++current) {
    // Check map and tail call if there's a match
    Handle<Map> map(receiver_maps->at(current));
    __ Cmp(map_reg, map);
    __ j(equal,
         Handle<Code>(handler_ics->at(current)),
         RelocInfo::CODE_TARGET);
  }

  __  bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
MaybeObject* ConstructStubCompiler::CompileConstructStub(JSFunction* function) {
  // ----------- S t a t e -------------
  //  -- rax : argc
  //  -- rdi : constructor
  //  -- rsp[0] : return address
  //  -- rsp[4] : last argument
  // -----------------------------------
  Label generic_stub_call;

  // Use r8 for holding undefined which is used in several places below.
  __ Move(r8, factory()->undefined_value());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Check to see whether there are any break points in the function code. If
  // there are jump to the generic constructor stub which calls the actual
  // code for the function thereby hitting the break points.
  __ movq(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rbx, FieldOperand(rbx, SharedFunctionInfo::kDebugInfoOffset));
  __ cmpq(rbx, r8);
  __ j(not_equal, &generic_stub_call);
#endif

  // Load the initial map and verify that it is in fact a map.
  __ movq(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
  // Will both indicate a NULL and a Smi.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(rbx, &generic_stub_call);
  __ CmpObjectType(rbx, MAP_TYPE, rcx);
  __ j(not_equal, &generic_stub_call);

#ifdef DEBUG
  // Cannot construct functions this way.
  // rdi: constructor
  // rbx: initial map
  __ CmpInstanceType(rbx, JS_FUNCTION_TYPE);
  __ Assert(not_equal, "Function constructed by construct stub.");
#endif

  // Now allocate the JSObject in new space.
  // rdi: constructor
  // rbx: initial map
  __ movzxbq(rcx, FieldOperand(rbx, Map::kInstanceSizeOffset));
  __ shl(rcx, Immediate(kPointerSizeLog2));
  __ AllocateInNewSpace(rcx,
                        rdx,
                        rcx,
                        no_reg,
                        &generic_stub_call,
                        NO_ALLOCATION_FLAGS);

  // Allocated the JSObject, now initialize the fields and add the heap tag.
  // rbx: initial map
  // rdx: JSObject (untagged)
  __ movq(Operand(rdx, JSObject::kMapOffset), rbx);
  __ Move(rbx, factory()->empty_fixed_array());
  __ movq(Operand(rdx, JSObject::kPropertiesOffset), rbx);
  __ movq(Operand(rdx, JSObject::kElementsOffset), rbx);

  // rax: argc
  // rdx: JSObject (untagged)
  // Load the address of the first in-object property into r9.
  __ lea(r9, Operand(rdx, JSObject::kHeaderSize));
  // Calculate the location of the first argument. The stack contains only the
  // return address on top of the argc arguments.
  __ lea(rcx, Operand(rsp, rax, times_pointer_size, 0));

  // rax: argc
  // rcx: first argument
  // rdx: JSObject (untagged)
  // r8: undefined
  // r9: first in-object property of the JSObject
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
  SharedFunctionInfo* shared = function->shared();
  for (int i = 0; i < shared->this_property_assignments_count(); i++) {
    if (shared->IsThisPropertyAssignmentArgument(i)) {
      // Check if the argument assigned to the property is actually passed.
      // If argument is not passed the property is set to undefined,
      // otherwise find it on the stack.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ movq(rbx, r8);
      __ cmpq(rax, Immediate(arg_number));
      __ cmovq(above, rbx, Operand(rcx, arg_number * -kPointerSize));
      // Store value in the property.
      __ movq(Operand(r9, i * kPointerSize), rbx);
    } else {
      // Set the property to the constant value.
      Handle<Object> constant(shared->GetThisPropertyAssignmentConstant(i));
      __ Move(Operand(r9, i * kPointerSize), constant);
    }
  }

  // Fill the unused in-object property fields with undefined.
  ASSERT(function->has_initial_map());
  for (int i = shared->this_property_assignments_count();
       i < function->initial_map()->inobject_properties();
       i++) {
    __ movq(Operand(r9, i * kPointerSize), r8);
  }

  // rax: argc
  // rdx: JSObject (untagged)
  // Move argc to rbx and the JSObject to return to rax and tag it.
  __ movq(rbx, rax);
  __ movq(rax, rdx);
  __ or_(rax, Immediate(kHeapObjectTag));

  // rax: JSObject
  // rbx: argc
  // Remove caller arguments and receiver from the stack and return.
  __ pop(rcx);
  __ lea(rsp, Operand(rsp, rbx, times_pointer_size, 1 * kPointerSize));
  __ push(rcx);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1);
  __ IncrementCounter(counters->constructed_objects_stub(), 1);
  __ ret(0);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Code* code =
      isolate()->builtins()->builtin(Builtins::kJSConstructStubGeneric);
  Handle<Code> generic_construct_stub(code);
  __ Jump(generic_construct_stub, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  __ JumpIfNotSmi(rax, &miss_force_generic);
  __ SmiToInteger32(rbx, rax);
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // rdx: receiver
  // rax: key
  // rbx: key as untagged int32
  // rcx: elements
  __ LoadFromNumberDictionary(&slow, rcx, rax, rbx, r9, rdi, rax);
  __ ret(0);

  __ bind(&slow);
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(slow_ic, RelocInfo::CODE_TARGET);

  __ bind(&miss_force_generic);
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}

void KeyedLoadStubCompiler::GenerateLoadExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &miss_force_generic);

  // Check that the index is in range.
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ SmiToInteger32(rcx, rax);
  __ cmpq(rax, FieldOperand(rbx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &miss_force_generic);

  // rax: index (as a smi)
  // rdx: receiver (JSObject)
  // rcx: untagged index
  // rbx: elements array
  __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
  // rbx: base pointer of external storage
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
      __ movsxbq(rcx, Operand(rbx, rcx, times_1, 0));
      break;
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ movzxbq(rcx, Operand(rbx, rcx, times_1, 0));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
      __ movsxwq(rcx, Operand(rbx, rcx, times_2, 0));
      break;
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ movzxwq(rcx, Operand(rbx, rcx, times_2, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
      __ movsxlq(rcx, Operand(rbx, rcx, times_4, 0));
      break;
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ movl(rcx, Operand(rbx, rcx, times_4, 0));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      __ cvtss2sd(xmm0, Operand(rbx, rcx, times_4, 0));
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ movsd(xmm0, Operand(rbx, rcx, times_8, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }

  // rax: index
  // rdx: receiver
  // For integer array types:
  // rcx: value
  // For floating-point array type:
  // xmm0: value as double.

  ASSERT(kSmiValueSize == 32);
  if (elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    // For the UnsignedInt array type, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;

    __ JumpIfUIntNotValidSmiValue(rcx, &box_int, Label::kNear);

    __ Integer32ToSmi(rax, rcx);
    __ ret(0);

    __ bind(&box_int);

    // Allocate a HeapNumber for the int and perform int-to-double
    // conversion.
    // The value is zero-extended since we loaded the value from memory
    // with movl.
    __ cvtqsi2sd(xmm0, rcx);

    __ AllocateHeapNumber(rcx, rbx, &slow);
    // Set the value.
    __ movsd(FieldOperand(rcx, HeapNumber::kValueOffset), xmm0);
    __ movq(rax, rcx);
    __ ret(0);
  } else if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
             elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    __ AllocateHeapNumber(rcx, rbx, &slow);
    // Set the value.
    __ movsd(FieldOperand(rcx, HeapNumber::kValueOffset), xmm0);
    __ movq(rax, rcx);
    __ ret(0);
  } else {
    __ Integer32ToSmi(rax, rcx);
    __ ret(0);
  }

  // Slow case: Jump to runtime.
  __ bind(&slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_external_array_slow(), 1);

  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  Handle<Code> ic = masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Miss case: Jump to runtime.
  __ bind(&miss_force_generic);

  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label slow, miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &miss_force_generic);

  // Check that the index is in range.
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ SmiToInteger32(rdi, rcx);  // Untag the index.
  __ cmpq(rcx, FieldOperand(rbx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &miss_force_generic);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // rax: value
  // rcx: key (a smi)
  // rdx: receiver (a JSObject)
  // rbx: elements array
  // rdi: untagged key
  Label check_heap_number;
  if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
    // Float to pixel conversion is only implemented in the runtime for now.
    __ JumpIfNotSmi(rax, &slow);
  } else {
    __ JumpIfNotSmi(rax, &check_heap_number, Label::kNear);
  }
  // No more branches to slow case on this path.  Key and receiver not needed.
  __ SmiToInteger32(rdx, rax);
  __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
  // rbx: base pointer of external storage
  switch (elements_kind) {
    case EXTERNAL_PIXEL_ELEMENTS:
      {  // Clamp the value to [0..255].
        Label done;
        __ testl(rdx, Immediate(0xFFFFFF00));
        __ j(zero, &done, Label::kNear);
        __ setcc(negative, rdx);  // 1 if negative, 0 if positive.
        __ decb(rdx);  // 0 if negative, 255 if positive.
        __ bind(&done);
      }
      __ movb(Operand(rbx, rdi, times_1, 0), rdx);
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ movb(Operand(rbx, rdi, times_1, 0), rdx);
      break;
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ movw(Operand(rbx, rdi, times_2, 0), rdx);
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ movl(Operand(rbx, rdi, times_4, 0), rdx);
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      // Need to perform int-to-float conversion.
      __ cvtlsi2ss(xmm0, rdx);
      __ movss(Operand(rbx, rdi, times_4, 0), xmm0);
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      // Need to perform int-to-float conversion.
      __ cvtlsi2sd(xmm0, rdx);
      __ movsd(Operand(rbx, rdi, times_8, 0), xmm0);
      break;
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
  __ ret(0);

  // TODO(danno): handle heap number -> pixel array conversion
  if (elements_kind != EXTERNAL_PIXEL_ELEMENTS) {
    __ bind(&check_heap_number);
    // rax: value
    // rcx: key (a smi)
    // rdx: receiver (a JSObject)
    // rbx: elements array
    // rdi: untagged key
    __ CmpObjectType(rax, HEAP_NUMBER_TYPE, kScratchRegister);
    __ j(not_equal, &slow);
    // No more branches to slow case on this path.

    // The WebGL specification leaves the behavior of storing NaN and
    // +/-Infinity into integer arrays basically undefined. For more
    // reproducible behavior, convert these to zero.
    __ movsd(xmm0, FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
    // rdi: untagged index
    // rbx: base pointer of external storage
    // top of FPU stack: value
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ cvtsd2ss(xmm0, xmm0);
      __ movss(Operand(rbx, rdi, times_4, 0), xmm0);
      __ ret(0);
    } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
      __ movsd(Operand(rbx, rdi, times_8, 0), xmm0);
      __ ret(0);
    } else {
      // Perform float-to-int conversion with truncation (round-to-zero)
      // behavior.

      // Convert to int32 and store the low byte/word.
      // If the value is NaN or +/-infinity, the result is 0x80000000,
      // which is automatically zero when taken mod 2^n, n < 32.
      // rdx: value (converted to an untagged integer)
      // rdi: untagged index
      // rbx: base pointer of external storage
      switch (elements_kind) {
        case EXTERNAL_BYTE_ELEMENTS:
        case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
          __ cvttsd2si(rdx, xmm0);
          __ movb(Operand(rbx, rdi, times_1, 0), rdx);
          break;
        case EXTERNAL_SHORT_ELEMENTS:
        case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
          __ cvttsd2si(rdx, xmm0);
          __ movw(Operand(rbx, rdi, times_2, 0), rdx);
          break;
        case EXTERNAL_INT_ELEMENTS:
        case EXTERNAL_UNSIGNED_INT_ELEMENTS:
          // Convert to int64, so that NaN and infinities become
          // 0x8000000000000000, which is zero mod 2^32.
          __ cvttsd2siq(rdx, xmm0);
          __ movl(Operand(rbx, rdi, times_4, 0), rdx);
          break;
        case EXTERNAL_PIXEL_ELEMENTS:
        case EXTERNAL_FLOAT_ELEMENTS:
        case EXTERNAL_DOUBLE_ELEMENTS:
        case FAST_ELEMENTS:
        case FAST_DOUBLE_ELEMENTS:
        case DICTIONARY_ELEMENTS:
        case NON_STRICT_ARGUMENTS_ELEMENTS:
          UNREACHABLE();
          break;
      }
      __ ret(0);
    }
  }

  // Slow case: call runtime.
  __ bind(&slow);

  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  Handle<Code> ic = masm->isolate()->builtins()->KeyedStoreIC_Slow();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Miss case: call runtime.
  __ bind(&miss_force_generic);

  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastElement(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &miss_force_generic);

  // Get the elements array.
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ AssertFastElements(rcx);

  // Check that the key is within bounds.
  __ SmiCompare(rax, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Load the result and make sure it's not the hole.
  SmiIndex index = masm->SmiToIndex(rbx, rax, kPointerSizeLog2);
  __ movq(rbx, FieldOperand(rcx,
                            index.reg,
                            index.scale,
                            FixedArray::kHeaderSize));
  __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
  __ j(equal, &miss_force_generic);
  __ movq(rax, rbx);
  __ ret(0);

  __ bind(&miss_force_generic);
  Code* code = masm->isolate()->builtins()->builtin(
      Builtins::kKeyedLoadIC_MissForceGeneric);
  Handle<Code> ic(code);
  __ jmp(ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastDoubleElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic, slow_allocate_heapnumber;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &miss_force_generic);

  // Get the elements array.
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ AssertFastElements(rcx);

  // Check that the key is within bounds.
  __ SmiCompare(rax, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Check for the hole
  __ SmiToInteger32(kScratchRegister, rax);
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmpl(FieldOperand(rcx, kScratchRegister, times_8, offset),
          Immediate(kHoleNanUpper32));
  __ j(equal, &miss_force_generic);

  // Always allocate a heap number for the result.
  __ movsd(xmm0, FieldOperand(rcx, kScratchRegister, times_8,
                              FixedDoubleArray::kHeaderSize));
  __ AllocateHeapNumber(rcx, rbx, &slow_allocate_heapnumber);
  // Set the value.
  __ movq(rax, rcx);
  __ movsd(FieldOperand(rcx, HeapNumber::kValueOffset), xmm0);
  __ ret(0);

  __ bind(&slow_allocate_heapnumber);
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
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &miss_force_generic);

  // Get the elements array and make sure it is a fast element array, not 'cow'.
  __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(FieldOperand(rdi, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &miss_force_generic);

  // Check that the key is within bounds.
  if (is_js_array) {
    __ SmiCompare(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
    __ j(above_equal, &miss_force_generic);
  } else {
    __ SmiCompare(rcx, FieldOperand(rdi, FixedArray::kLengthOffset));
    __ j(above_equal, &miss_force_generic);
  }

  // Do the store and update the write barrier. Make sure to preserve
  // the value in register eax.
  __ movq(rdx, rax);
  __ SmiToInteger32(rcx, rcx);
  __ movq(FieldOperand(rdi, rcx, times_pointer_size, FixedArray::kHeaderSize),
          rax);
  __ RecordWrite(rdi, 0, rdx, rcx);

  // Done.
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic, smi_value, is_nan, maybe_nan;
  Label have_double_value, not_nan;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &miss_force_generic);

  // Get the elements array.
  __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
  __ AssertFastElements(rdi);

  // Check that the key is within bounds.
  if (is_js_array) {
    __ SmiCompare(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
  } else {
    __ SmiCompare(rcx, FieldOperand(rdi, FixedDoubleArray::kLengthOffset));
  }
  __ j(above_equal, &miss_force_generic);

  // Handle smi values specially
  __ JumpIfSmi(rax, &smi_value, Label::kNear);

  __ CheckMap(rax,
              masm->isolate()->factory()->heap_number_map(),
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  // Double value, canonicalize NaN.
  uint32_t offset = HeapNumber::kValueOffset + sizeof(kHoleNanLower32);
  __ cmpl(FieldOperand(rax, offset),
          Immediate(kNaNOrInfinityLowerBoundUpper32));
  __ j(greater_equal, &maybe_nan, Label::kNear);

  __ bind(&not_nan);
  __ movsd(xmm0, FieldOperand(rax, HeapNumber::kValueOffset));
  __ bind(&have_double_value);
  __ SmiToInteger32(rcx, rcx);
  __ movsd(FieldOperand(rdi, rcx, times_8, FixedDoubleArray::kHeaderSize),
           xmm0);
  __ ret(0);

  __ bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  __ j(greater, &is_nan, Label::kNear);
  __ cmpl(FieldOperand(rax, HeapNumber::kValueOffset), Immediate(0));
  __ j(zero, &not_nan);
  __ bind(&is_nan);
  // Convert all NaNs to the same canonical NaN value when they are stored in
  // the double array.
  __ Set(kScratchRegister, BitCast<uint64_t>(
      FixedDoubleArray::canonical_not_the_hole_nan_as_double()));
  __ movq(xmm0, kScratchRegister);
  __ jmp(&have_double_value, Label::kNear);

  __ bind(&smi_value);
  // Value is a smi. convert to a double and store.
  // Preserve original value.
  __ SmiToInteger32(rdx, rax);
  __ push(rdx);
  __ fild_s(Operand(rsp, 0));
  __ pop(rdx);
  __ SmiToInteger32(rcx, rcx);
  __ fstp_d(FieldOperand(rdi, rcx, times_8, FixedDoubleArray::kHeaderSize));
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
