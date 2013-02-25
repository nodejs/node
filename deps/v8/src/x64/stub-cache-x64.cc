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
                       Register receiver,
                       Register name,
                       // The offset is scaled by 4, based on
                       // kHeapObjectTagSize, which is two bits
                       Register offset) {
  // We need to scale up the pointer by 2 because the offset is scaled by less
  // than the pointer size.
  ASSERT(kPointerSizeLog2 == kHeapObjectTagSize + 1);
  ScaleFactor scale_factor = times_2;

  ASSERT_EQ(24, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ lea(offset, Operand(offset, offset, times_2, 0));

  __ LoadAddress(kScratchRegister, key_offset);

  // Check that the key in the entry matches the name.
  // Multiply entry offset by 16 to get the entry address. Since the
  // offset register already holds the entry offset times four, multiply
  // by a further four.
  __ cmpl(name, Operand(kScratchRegister, offset, scale_factor, 0));
  __ j(not_equal, &miss);

  // Get the map entry from the cache.
  // Use key_offset + kPointerSize * 2, rather than loading map_offset.
  __ movq(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, kPointerSize * 2));
  __ cmpq(kScratchRegister, FieldOperand(receiver, HeapObject::kMapOffset));
  __ j(not_equal, &miss);

  // Get the code entry from the cache.
  __ LoadAddress(kScratchRegister, value_offset);
  __ movq(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, 0));

  // Check that the flags match what we're looking for.
  __ movl(offset, FieldOperand(kScratchRegister, Code::kFlagsOffset));
  __ and_(offset, Immediate(~Code::kFlagsNotUsedInLookup));
  __ cmpl(offset, Immediate(flags));
  __ j(not_equal, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

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
static void GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                             Label* miss_label,
                                             Register receiver,
                                             Handle<String> name,
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
  StringDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                     miss_label,
                                                     &done,
                                                     properties,
                                                     name,
                                                     r1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra,
                              Register extra2,
                              Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;
  USE(extra);   // The register extra is not used on the X64 platform.
  USE(extra2);  // The register extra2 is not used on the X64 platform.
  USE(extra3);  // The register extra2 is not used on the X64 platform.
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 24.
  ASSERT(sizeof(Entry) == 24);

  // Make sure the flags do not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));

  // Check scratch register is valid, extra and extra2 are unused.
  ASSERT(!scratch.is(no_reg));
  ASSERT(extra2.is(no_reg));
  ASSERT(extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, receiver, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, receiver, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ movq(prototype,
          Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  __ movq(prototype,
          FieldOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ movq(prototype, Operand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ movq(prototype,
          FieldOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ movq(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
  Isolate* isolate = masm->isolate();
  // Check we're still in the same context.
  __ Move(prototype, isolate->global_object());
  __ cmpq(Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)),
          prototype);
  __ j(not_equal, miss);
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(isolate->native_context()->get(index)));
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
                                            Register dst,
                                            Register src,
                                            Handle<JSObject> holder,
                                            int index) {
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
                                     Handle<JSObject> holder_obj) {
  __ push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  __ Move(kScratchRegister, interceptor);
  __ push(kScratchRegister);
  __ push(receiver);
  __ push(holder);
  __ push(FieldOperand(kScratchRegister, InterceptorInfo::kDataOffset));
  __ PushAddress(ExternalReference::isolate_address());
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly),
                        masm->isolate());
  __ Set(rax, 6);
  __ LoadAddress(rbx, ref);

  CEntryStub stub(1);
  __ CallStub(&stub);
}


// Number of pointers to be reserved on stack for fast API call.
static const int kFastApiCallArguments = 4;


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
static void GenerateFastApiCall(MacroAssembler* masm,
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
  //  -- rsp[32]             : isolate
  //  -- rsp[40]             : last argument
  //  -- ...
  //  -- rsp[(argc + 4) * 8] : first argument
  //  -- rsp[(argc + 5) * 8] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  Handle<JSFunction> function = optimization.constant_function();
  __ LoadHeapObject(rdi, function);
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Pass the additional arguments.
  __ movq(Operand(rsp, 2 * kPointerSize), rdi);
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data(api_call_info->data());
  if (masm->isolate()->heap()->InNewSpace(*call_data)) {
    __ Move(rcx, api_call_info);
    __ movq(rbx, FieldOperand(rcx, CallHandlerInfo::kDataOffset));
    __ movq(Operand(rsp, 3 * kPointerSize), rbx);
  } else {
    __ Move(Operand(rsp, 3 * kPointerSize), call_data);
  }
  __ movq(kScratchRegister, ExternalReference::isolate_address());
  __ movq(Operand(rsp, 4 * kPointerSize), kScratchRegister);

  // Prepare arguments.
  __ lea(rbx, Operand(rsp, 4 * kPointerSize));

#if defined(__MINGW64__)
  Register arguments_arg = rcx;
#elif defined(_WIN64)
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

  // Function address is a foreign pointer outside V8's heap.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  __ CallApiFunctionAndReturn(function_address,
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

  void Compile(MacroAssembler* masm,
               Handle<JSObject> object,
               Handle<JSObject> holder,
               Handle<String> name,
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
      CompileCacheable(masm, object, receiver, scratch1, scratch2, scratch3,
                       holder, lookup, name, optimization, miss);
    } else {
      CompileRegular(masm, object, receiver, scratch1, scratch2, scratch3,
                     name, holder, miss);
    }
  }

 private:
  void CompileCacheable(MacroAssembler* masm,
                        Handle<JSObject> object,
                        Register receiver,
                        Register scratch1,
                        Register scratch2,
                        Register scratch3,
                        Handle<JSObject> interceptor_holder,
                        LookupResult* lookup,
                        Handle<String> name,
                        const CallOptimization& optimization,
                        Label* miss_label) {
    ASSERT(optimization.is_constant_call());
    ASSERT(!lookup->holder()->IsGlobalObject());

    int depth1 = kInvalidProtoDepth;
    int depth2 = kInvalidProtoDepth;
    bool can_do_fast_api_call = false;
    if (optimization.is_simple_api_call() &&
        !lookup->holder()->IsGlobalObject()) {
      depth1 = optimization.GetPrototypeDepthOfExpectedType(
          object, interceptor_holder);
      if (depth1 == kInvalidProtoDepth) {
        depth2 = optimization.GetPrototypeDepthOfExpectedType(
            interceptor_holder, Handle<JSObject>(lookup->holder()));
      }
      can_do_fast_api_call =
          depth1 != kInvalidProtoDepth || depth2 != kInvalidProtoDepth;
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
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, scratch3,
                                        name, depth1, miss);

    // Invoke an interceptor and if it provides a value,
    // branch to |regular_invoke|.
    Label regular_invoke;
    LoadWithInterceptor(masm, receiver, holder, interceptor_holder,
                        &regular_invoke);

    // Interceptor returned nothing for this property.  Try to use cached
    // constant function.

    // Check that the maps from interceptor's holder to constant function's
    // holder haven't changed and thus we can use cached constant function.
    if (*interceptor_holder != lookup->holder()) {
      stub_compiler_->CheckPrototypes(interceptor_holder, receiver,
                                      Handle<JSObject>(lookup->holder()),
                                      scratch1, scratch2, scratch3,
                                      name, depth2, miss);
    } else {
      // CheckPrototypes has a side effect of fetching a 'holder'
      // for API (object which is instanceof for the signature).  It's
      // safe to omit it here, as if present, it should be fetched
      // by the previous CheckPrototypes.
      ASSERT(depth2 == kInvalidProtoDepth);
    }

    // Invoke function.
    if (can_do_fast_api_call) {
      GenerateFastApiCall(masm, optimization, arguments_.immediate());
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
  }

  void CompileRegular(MacroAssembler* masm,
                      Handle<JSObject> object,
                      Register receiver,
                      Register scratch1,
                      Register scratch2,
                      Register scratch3,
                      Handle<String> name,
                      Handle<JSObject> interceptor_holder,
                      Label* miss_label) {
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, scratch3,
                                        name, miss_label);

    FrameScope scope(masm, StackFrame::INTERNAL);
    // Save the name_ register across the call.
    __ push(name_);

    PushInterceptorArguments(masm, receiver, holder, name_, interceptor_holder);

    __ CallExternalReference(
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForCall),
                          masm->isolate()),
        6);

    // Restore the name_ register.
    __ pop(name_);

    // Leave the internal frame.
  }

  void LoadWithInterceptor(MacroAssembler* masm,
                           Register receiver,
                           Register holder,
                           Handle<JSObject> holder_obj,
                           Label* interceptor_succeeded) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(holder);  // Save the holder.
      __ push(name_);  // Save the name.

      CompileCallLoadPropertyWithInterceptor(masm,
                                             receiver,
                                             holder,
                                             name_,
                                             holder_obj);

      __ pop(name_);  // Restore the name.
      __ pop(receiver);  // Restore the holder.
      // Leave the internal frame.
    }

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
  Handle<Code> code = (kind == Code::LOAD_IC)
      ? masm->isolate()->builtins()->LoadIC_Miss()
      : masm->isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(code, RelocInfo::CODE_TARGET);
}


void StubCompiler::GenerateKeyedLoadMissForceGeneric(MacroAssembler* masm) {
  Handle<Code> code =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ Jump(code, RelocInfo::CODE_TARGET);
}


// Both name_reg and receiver_reg are preserved on jumps to miss_label,
// but may be destroyed if store is successful.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      Handle<JSObject> object,
                                      int index,
                                      Handle<Map> transition,
                                      Handle<String> name,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch1,
                                      Register scratch2,
                                      Label* miss_label) {
  LookupResult lookup(masm->isolate());
  object->Lookup(*name, &lookup);
  if (lookup.IsFound() && (lookup.IsReadOnly() || !lookup.IsCacheable())) {
    // In sloppy mode, we could just return the value and be done. However, we
    // might be in strict mode, where we have to throw. Since we cannot tell,
    // go into slow case unconditionally.
    __ jmp(miss_label);
    return;
  }

  // Check that the map of the object hasn't changed.
  CompareMapMode mode = transition.is_null() ? ALLOW_ELEMENT_TRANSITION_MAPS
                                             : REQUIRE_EXACT_MAP;
  __ CheckMap(receiver_reg, Handle<Map>(object->map()),
              miss_label, DO_SMI_CHECK, mode);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver_reg, scratch1, miss_label);
  }

  // Check that we are allowed to write this.
  if (!transition.is_null() && object->GetPrototype()->IsJSObject()) {
    JSObject* holder;
    if (lookup.IsFound()) {
      holder = lookup.holder();
    } else {
      // Find the top object.
      holder = *object;
      do {
        holder = JSObject::cast(holder->GetPrototype());
      } while (holder->GetPrototype()->IsJSObject());
    }
    // We need an extra register, push
    __ push(name_reg);
    Label miss_pop, done_check;
    CheckPrototypes(object, receiver_reg, Handle<JSObject>(holder), name_reg,
                    scratch1, scratch2, name, &miss_pop);
    __ jmp(&done_check);
    __ bind(&miss_pop);
    __ pop(name_reg);
    __ jmp(miss_label);
    __ bind(&done_check);
    __ pop(name_reg);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if (!transition.is_null() && (object->map()->unused_property_fields() == 0)) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ pop(scratch1);  // Return address.
    __ push(receiver_reg);
    __ Push(transition);
    __ push(rax);
    __ push(scratch1);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  if (!transition.is_null()) {
    // Update the map of the object.
    __ Move(scratch1, transition);
    __ movq(FieldOperand(receiver_reg, HeapObject::kMapOffset), scratch1);

    // Update the write barrier for the map field and pass the now unused
    // name_reg as scratch register.
    __ RecordWriteField(receiver_reg,
                        HeapObject::kMapOffset,
                        scratch1,
                        name_reg,
                        kDontSaveFPRegs,
                        OMIT_REMEMBERED_SET,
                        OMIT_SMI_CHECK);
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
    __ RecordWriteField(
        receiver_reg, offset, name_reg, scratch1, kDontSaveFPRegs);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ movq(scratch1, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ movq(FieldOperand(scratch1, offset), rax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ movq(name_reg, rax);
    __ RecordWriteField(
        scratch1, offset, name_reg, receiver_reg, kDontSaveFPRegs);
  }

  // Return the value (register rax).
  __ ret(0);
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
static void GenerateCheckPropertyCell(MacroAssembler* masm,
                                      Handle<GlobalObject> global,
                                      Handle<String> name,
                                      Register scratch,
                                      Label* miss) {
  Handle<JSGlobalPropertyCell> cell =
      GlobalObject::EnsurePropertyCell(global, name);
  ASSERT(cell->value()->IsTheHole());
  __ Move(scratch, cell);
  __ Cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
         masm->isolate()->factory()->the_hole_value());
  __ j(not_equal, miss);
}


// Calls GenerateCheckPropertyCell for each global object in the prototype chain
// from object to (but not including) holder.
static void GenerateCheckPropertyCells(MacroAssembler* masm,
                                       Handle<JSObject> object,
                                       Handle<JSObject> holder,
                                       Handle<String> name,
                                       Register scratch,
                                       Label* miss) {
  Handle<JSObject> current = object;
  while (!current.is_identical_to(holder)) {
    if (current->IsGlobalObject()) {
      GenerateCheckPropertyCell(masm,
                                Handle<GlobalObject>::cast(current),
                                name,
                                scratch,
                                miss);
    }
    current = Handle<JSObject>(JSObject::cast(current->GetPrototype()));
  }
}

#undef __
#define __ ACCESS_MASM((masm()))


Register StubCompiler::CheckPrototypes(Handle<JSObject> object,
                                       Register object_reg,
                                       Handle<JSObject> holder,
                                       Register holder_reg,
                                       Register scratch1,
                                       Register scratch2,
                                       Handle<String> name,
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
  Handle<JSObject> current = object;
  while (!current.is_identical_to(holder)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());

    Handle<JSObject> prototype(JSObject::cast(current->GetPrototype()));
    if (!current->HasFastProperties() &&
        !current->IsJSGlobalObject() &&
        !current->IsJSGlobalProxy()) {
      if (!name->IsSymbol()) {
        name = factory()->LookupSymbol(name);
      }
      ASSERT(current->property_dictionary()->FindEntry(*name) ==
             StringDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ movq(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ movq(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool in_new_space = heap()->InNewSpace(*prototype);
      Handle<Map> current_map(current->map());
      if (in_new_space) {
        // Save the map in scratch1 for later.
        __ movq(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      __ CheckMap(reg, Handle<Map>(current_map),
                  miss, DONT_DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch2, miss);
      }
      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (in_new_space) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ movq(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ Move(reg, prototype);
      }
    }

    if (save_at_depth == depth) {
      __ movq(Operand(rsp, kPointerSize), reg);
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }
  ASSERT(current.is_identical_to(holder));

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  // Check the holder map.
  __ CheckMap(reg, Handle<Map>(holder->map()),
              miss, DONT_DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

  // Perform security check for access to the global object.
  ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());
  if (current->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  }

  // If we've skipped any global objects, it's not enough to verify that
  // their maps haven't changed.  We also need to check that the property
  // cell for the property is still empty.
  GenerateCheckPropertyCells(masm(), object, holder, name, scratch1, miss);

  // Return the register containing the holder.
  return reg;
}


void StubCompiler::GenerateLoadField(Handle<JSObject> object,
                                     Handle<JSObject> holder,
                                     Register receiver,
                                     Register scratch1,
                                     Register scratch2,
                                     Register scratch3,
                                     int index,
                                     Handle<String> name,
                                     Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check the prototype chain.
  Register reg = CheckPrototypes(
      object, receiver, holder, scratch1, scratch2, scratch3, name, miss);

  // Get the value from the properties.
  GenerateFastPropertyLoad(masm(), rax, reg, holder, index);
  __ ret(0);
}


void StubCompiler::GenerateDictionaryLoadCallback(Register receiver,
                                                  Register name_reg,
                                                  Register scratch1,
                                                  Register scratch2,
                                                  Register scratch3,
                                                  Handle<AccessorInfo> callback,
                                                  Handle<String> name,
                                                  Label* miss) {
  ASSERT(!receiver.is(scratch1));
  ASSERT(!receiver.is(scratch2));
  ASSERT(!receiver.is(scratch3));

  // Load the properties dictionary.
  Register dictionary = scratch1;
  __ movq(dictionary, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Probe the dictionary.
  Label probe_done;
  StringDictionaryLookupStub::GeneratePositiveLookup(masm(),
                                                     miss,
                                                     &probe_done,
                                                     dictionary,
                                                     name_reg,
                                                     scratch2,
                                                     scratch3);
  __ bind(&probe_done);

  // If probing finds an entry in the dictionary, scratch3 contains the
  // index into the dictionary. Check that the value is the callback.
  Register index = scratch3;
  const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ movq(scratch2,
          Operand(dictionary, index, times_pointer_size,
                  kValueOffset - kHeapObjectTag));
  __ movq(scratch3, callback, RelocInfo::EMBEDDED_OBJECT);
  __ cmpq(scratch2, scratch3);
  __ j(not_equal, miss);
}


void StubCompiler::GenerateLoadCallback(Handle<JSObject> object,
                                        Handle<JSObject> holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Register scratch4,
                                        Handle<AccessorInfo> callback,
                                        Handle<String> name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  Register reg = CheckPrototypes(object, receiver, holder, scratch1,
                                 scratch2, scratch3, name, miss);

  if (!holder->HasFastProperties() && !holder->IsJSGlobalObject()) {
    GenerateDictionaryLoadCallback(
        reg, name_reg, scratch2, scratch3, scratch4, callback, name, miss);
  }

  // Insert additional parameters into the stack frame above return address.
  ASSERT(!scratch2.is(reg));
  __ pop(scratch2);  // Get return address to place it below.

  __ push(receiver);  // receiver
  __ push(reg);  // holder
  if (heap()->InNewSpace(callback->data())) {
    __ Move(scratch1, callback);
    __ push(FieldOperand(scratch1, AccessorInfo::kDataOffset));  // data
  } else {
    __ Push(Handle<Object>(callback->data()));
  }
  __ PushAddress(ExternalReference::isolate_address());  // isolate
  __ push(name_reg);  // name
  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const AccessorInfo& to the C++ callback.

#if defined(__MINGW64__)
  Register accessor_info_arg = rdx;
  Register name_arg = rcx;
#elif defined(_WIN64)
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

  // 4 elements array for v8::Arguments::values_ and handler for name.
  const int kStackSpace = 5;

  // Allocate v8::AccessorInfo in non-GCed stack space.
  const int kArgStackSpace = 1;

  __ PrepareCallApiFunction(kArgStackSpace);
  __ lea(rax, Operand(name_arg, 4 * kPointerSize));

  // v8::AccessorInfo::args_.
  __ movq(StackSpaceOperand(0), rax);

  // The context register (rsi) has been saved in PrepareCallApiFunction and
  // could be used to pass arguments.
  __ lea(accessor_info_arg, StackSpaceOperand(0));

  Address getter_address = v8::ToCData<Address>(callback->getter());
  __ CallApiFunctionAndReturn(getter_address, kStackSpace);
}


void StubCompiler::GenerateLoadConstant(Handle<JSObject> object,
                                        Handle<JSObject> holder,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Handle<JSFunction> value,
                                        Handle<String> name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  CheckPrototypes(
      object, receiver, holder, scratch1, scratch2, scratch3, name, miss);

  // Return the constant value.
  __ LoadHeapObject(rax, value);
  __ ret(0);
}


void StubCompiler::GenerateLoadInterceptor(Handle<JSObject> object,
                                           Handle<JSObject> interceptor_holder,
                                           LookupResult* lookup,
                                           Register receiver,
                                           Register name_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Handle<String> name,
                                           Label* miss) {
  ASSERT(interceptor_holder->HasNamedInterceptor());
  ASSERT(!interceptor_holder->GetNamedInterceptor()->getter()->IsUndefined());

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added
  // later.
  bool compile_followup_inline = false;
  if (lookup->IsFound() && lookup->IsCacheable()) {
    if (lookup->IsField()) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
               lookup->GetCallbackObject()->IsAccessorInfo()) {
      AccessorInfo* callback = AccessorInfo::cast(lookup->GetCallbackObject());
      compile_followup_inline = callback->getter() != NULL &&
          callback->IsCompatibleReceiver(*object);
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

    // Preserve the receiver register explicitly whenever it is different from
    // the holder and it is needed should the interceptor return without any
    // result. The CALLBACKS case needs the receiver to be passed into C++ code,
    // the FIELD case might cause a miss during the prototype check.
    bool must_perfrom_prototype_check = *interceptor_holder != lookup->holder();
    bool must_preserve_receiver_reg = !receiver.is(holder_reg) &&
        (lookup->type() == CALLBACKS || must_perfrom_prototype_check);

    // Save necessary data before invoking an interceptor.
    // Requires a frame to make GC aware of pushed pointers.
    {
      FrameScope frame_scope(masm(), StackFrame::INTERNAL);

      if (must_preserve_receiver_reg) {
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
      frame_scope.GenerateLeaveFrame();
      __ ret(0);

      __ bind(&interceptor_failed);
      __ pop(name_reg);
      __ pop(holder_reg);
      if (must_preserve_receiver_reg) {
        __ pop(receiver);
      }

      // Leave the internal frame.
    }

    // Check that the maps from interceptor's holder to lookup's holder
    // haven't changed.  And load lookup's holder into |holder| register.
    if (must_perfrom_prototype_check) {
      holder_reg = CheckPrototypes(interceptor_holder,
                                   holder_reg,
                                   Handle<JSObject>(lookup->holder()),
                                   scratch1,
                                   scratch2,
                                   scratch3,
                                   name,
                                   miss);
    }

    if (lookup->IsField()) {
      // We found FIELD property in prototype chain of interceptor's holder.
      // Retrieve a field from field's holder.
      GenerateFastPropertyLoad(masm(), rax, holder_reg,
                               Handle<JSObject>(lookup->holder()),
                               lookup->GetFieldIndex());
      __ ret(0);
    } else {
      // We found CALLBACKS property in prototype chain of interceptor's
      // holder.
      ASSERT(lookup->type() == CALLBACKS);
      Handle<AccessorInfo> callback(
          AccessorInfo::cast(lookup->GetCallbackObject()));
      ASSERT(callback->getter() != NULL);

      // Tail call to runtime.
      // Important invariant in CALLBACKS case: the code above must be
      // structured to never clobber |receiver| register.
      __ pop(scratch2);  // return address
      __ push(receiver);
      __ push(holder_reg);
      __ Move(holder_reg, callback);
      __ push(FieldOperand(holder_reg, AccessorInfo::kDataOffset));
      __ PushAddress(ExternalReference::isolate_address());
      __ push(holder_reg);
      __ push(name_reg);
      __ push(scratch2);  // restore return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            isolate());
      __ TailCallExternalReference(ref, 6, 1);
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
    __ TailCallExternalReference(ref, 6, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(Handle<String> name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ Cmp(rcx, name);
    __ j(not_equal, miss);
  }
}


void CallStubCompiler::GenerateGlobalReceiverCheck(Handle<JSObject> object,
                                                   Handle<JSObject> holder,
                                                   Handle<String> name,
                                                   Label* miss) {
  ASSERT(holder->IsGlobalObject());

  // Get the number of arguments.
  const int argc = arguments().immediate();

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));


  // Check that the maps haven't changed.
  __ JumpIfSmi(rdx, miss);
  CheckPrototypes(object, rdx, holder, rbx, rax, rdi, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Label* miss) {
  // Get the value from the cell.
  __ Move(rdi, cell);
  __ movq(rdi, FieldOperand(rdi, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (heap()->InNewSpace(*function)) {
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
  } else {
    __ Cmp(rdi, function);
  }
  __ j(not_equal, miss);
}


void CallStubCompiler::GenerateMissBranch() {
  Handle<Code> code =
      isolate()->stub_cache()->ComputeCallMiss(arguments().immediate(),
                                               kind_,
                                               extra_state_);
  __ Jump(code, RelocInfo::CODE_TARGET);
}


Handle<Code> CallStubCompiler::CompileCallField(Handle<JSObject> object,
                                                Handle<JSObject> holder,
                                                int index,
                                                Handle<String> name) {
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
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::FIELD, name);
}


Handle<Code> CallStubCompiler::CompileArrayPushCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax, rdi,
                  name, &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ movq(rax, FieldOperand(rdx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    Label call_builtin;

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label attempt_to_grow_elements, with_write_barrier;

      // Get the elements array of the object.
      __ movq(rdi, FieldOperand(rdx, JSArray::kElementsOffset));

      // Check that the elements are in fast mode and writable.
      __ Cmp(FieldOperand(rdi, HeapObject::kMapOffset),
             factory()->fixed_array_map());
      __ j(not_equal, &call_builtin);

      // Get the array's length into rax and calculate new length.
      __ SmiToInteger32(rax, FieldOperand(rdx, JSArray::kLengthOffset));
      STATIC_ASSERT(FixedArray::kMaxLength < Smi::kMaxValue);
      __ addl(rax, Immediate(argc));

      // Get the elements' length into rcx.
      __ SmiToInteger32(rcx, FieldOperand(rdi, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmpl(rax, rcx);
      __ j(greater, &attempt_to_grow_elements);

      // Check if value is a smi.
      __ movq(rcx, Operand(rsp, argc * kPointerSize));
      __ JumpIfNotSmi(rcx, &with_write_barrier);

      // Save new length.
      __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Store the value.
      __ movq(FieldOperand(rdi,
                           rax,
                           times_pointer_size,
                           FixedArray::kHeaderSize - argc * kPointerSize),
              rcx);

      __ Integer32ToSmi(rax, rax);  // Return new length as smi.
      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));

      if (FLAG_smi_only_arrays  && !FLAG_trace_elements_transitions) {
        Label fast_object, not_fast_object;
        __ CheckFastObjectElements(rbx, &not_fast_object, Label::kNear);
        __ jmp(&fast_object);
        // In case of fast smi-only, convert to fast object, otherwise bail out.
        __ bind(&not_fast_object);
        __ CheckFastSmiElements(rbx, &call_builtin);
        // rdx: receiver
        // rbx: map

        Label try_holey_map;
        __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                               FAST_ELEMENTS,
                                               rbx,
                                               rdi,
                                               &try_holey_map);

        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        // Restore edi.
        __ movq(rdi, FieldOperand(rdx, JSArray::kElementsOffset));
        __ jmp(&fast_object);

        __ bind(&try_holey_map);
        __ LoadTransitionedArrayMapConditional(FAST_HOLEY_SMI_ELEMENTS,
                                               FAST_HOLEY_ELEMENTS,
                                               rbx,
                                               rdi,
                                               &call_builtin);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        __ movq(rdi, FieldOperand(rdx, JSArray::kElementsOffset));
        __ bind(&fast_object);
      } else {
        __ CheckFastObjectElements(rbx, &call_builtin);
      }

      // Save new length.
      __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Store the value.
      __ lea(rdx, FieldOperand(rdi,
                               rax, times_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ movq(Operand(rdx, 0), rcx);

      __ RecordWrite(rdi, rdx, rcx, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                     OMIT_SMI_CHECK);

      __ Integer32ToSmi(rax, rax);  // Return new length as smi.
      __ ret((argc + 1) * kPointerSize);

      __ bind(&attempt_to_grow_elements);
      if (!FLAG_inline_new) {
        __ jmp(&call_builtin);
      }

      __ movq(rbx, Operand(rsp, argc * kPointerSize));
      // Growing elements that are SMI-only requires special handling in case
      // the new element is non-Smi. For now, delegate to the builtin.
      Label no_fast_elements_check;
      __ JumpIfSmi(rbx, &no_fast_elements_check);
      __ movq(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
      __ CheckFastObjectElements(rcx, &call_builtin, Label::kFar);
      __ bind(&no_fast_elements_check);

      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(isolate());
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(isolate());

      const int kAllocationDelta = 4;
      // Load top.
      __ Load(rcx, new_space_allocation_top);

      // Check if it's the end of elements.
      __ lea(rdx, FieldOperand(rdi,
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

      // Push the argument...
      __ movq(Operand(rdx, 0), rbx);
      // ... and fill the rest with holes.
      __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
      for (int i = 1; i < kAllocationDelta; i++) {
        __ movq(Operand(rdx, i * kPointerSize), kScratchRegister);
      }

      // We know the elements array is in new space so we don't need the
      // remembered set, but we just pushed a value onto it so we may have to
      // tell the incremental marker to rescan the object that we just grew.  We
      // don't need to worry about the holes because they are in old space and
      // already marked black.
      __ RecordWrite(rdi, rdx, rbx, kDontSaveFPRegs, OMIT_REMEMBERED_SET);

      // Restore receiver to rdx as finish sequence assumes it's here.
      __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ SmiAddConstant(FieldOperand(rdi, FixedArray::kLengthOffset),
                        Smi::FromInt(kAllocationDelta));

      // Make new length a smi before returning it.
      __ Integer32ToSmi(rax, rax);
      __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      __ ret((argc + 1) * kPointerSize);
    }

    __ bind(&call_builtin);
    __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPush,
                                                   isolate()),
                                 argc + 1,
                                 1);
  }

  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileArrayPopCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss, return_undefined, call_builtin;
  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax, rdi,
                  name, &miss);

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
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringCharCodeAtCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) return Handle<Code>::null();

  const int argc = arguments().immediate();

  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;
  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }
  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            rax,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  rax, holder, rbx, rdx, rdi, name, &miss);

  Register receiver = rbx;
  Register index = rdi;
  Register result = rax;
  __ movq(receiver, Operand(rsp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ movq(index, Operand(rsp, (argc - 0) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
  }

  StringCharCodeAtGenerator generator(receiver,
                                      index,
                                      result,
                                      &miss,  // When not a string.
                                      &miss,  // When not a number.
                                      index_out_of_range_label,
                                      STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(rax, Heap::kNanValueRootIndex);
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in rcx.
  __ Move(rcx, name);
  __ bind(&name_miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringCharAtCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) return Handle<Code>::null();

  const int argc = arguments().immediate();
  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;
  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }
  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            rax,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  rax, holder, rbx, rdx, rdi, name, &miss);

  Register receiver = rax;
  Register index = rdi;
  Register scratch = rdx;
  Register result = rax;
  __ movq(receiver, Operand(rsp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ movq(index, Operand(rsp, (argc - 0) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
  }

  StringCharAtGenerator generator(receiver,
                                  index,
                                  scratch,
                                  result,
                                  &miss,  // When not a string.
                                  &miss,  // When not a number.
                                  index_out_of_range_label,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(rax, Heap::kEmptyStringRootIndex);
    __ ret((argc + 1) * kPointerSize);
  }
  __ bind(&miss);
  // Restore function name in rcx.
  __ Move(rcx, name);
  __ bind(&name_miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringFromCharCodeCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  const int argc = arguments().immediate();
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ movq(rdx, Operand(rsp, 2 * kPointerSize));
    __ JumpIfSmi(rdx, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax, rdi,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
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

  StringCharFromCodeGenerator generator(code, rax);
  generator.GenerateFast(masm());
  __ ret(2 * kPointerSize);

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  __ bind(&miss);
  // rcx: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathFloorCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // TODO(872): implement this.
  return Handle<Code>::null();
}


Handle<Code> CallStubCompiler::CompileMathAbsCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rcx                 : function name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  const int argc = arguments().immediate();
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ movq(rdx, Operand(rsp, 2 * kPointerSize));
    __ JumpIfSmi(rdx, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax, rdi,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
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
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  __ bind(&miss);
  // rcx: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileFastApiCall(
    const CallOptimization& optimization,
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  ASSERT(optimization.is_simple_api_call());
  // Bail out if object is a global object as we don't want to
  // repatch it to global receiver.
  if (object->IsGlobalObject()) return Handle<Code>::null();
  if (!cell.is_null()) return Handle<Code>::null();
  if (!object->IsJSObject()) return Handle<Code>::null();
  int depth = optimization.GetPrototypeDepthOfExpectedType(
      Handle<JSObject>::cast(object), holder);
  if (depth == kInvalidProtoDepth) return Handle<Code>::null();

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
  CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax, rdi,
                  name, depth, &miss);

  // Move the return address on top of the stack.
  __ movq(rax, Operand(rsp, 4 * kPointerSize));
  __ movq(Operand(rsp, 0 * kPointerSize), rax);

  GenerateFastApiCall(masm(), optimization, argc);

  __ bind(&miss);
  __ addq(rsp, Immediate(kFastApiCallArguments * kPointerSize));

  __ bind(&miss_before_stack_reserved);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileCallConstant(Handle<Object> object,
                                                   Handle<JSObject> holder,
                                                   Handle<JSFunction> function,
                                                   Handle<String> name,
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
    Handle<Code> code = CompileCustomCall(object, holder,
                                          Handle<JSGlobalPropertyCell>::null(),
                                          function, name);
    // A null handle means bail out to the regular compiler code below.
    if (!code.is_null()) return code;
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
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(counters->call_const(), 1);

      // Check that the maps haven't changed.
      CheckPrototypes(Handle<JSObject>::cast(object), rdx, holder, rbx, rax,
                      rdi, name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
        __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
      }
      break;

    case STRING_CHECK:
      if (function->IsBuiltin() || !function->shared()->is_classic_mode()) {
        // Check that the object is a two-byte string or a symbol.
        __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, rax);
        __ j(above_equal, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, rax, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            rax, holder, rbx, rdx, rdi, name, &miss);
      } else {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      }
      break;

    case NUMBER_CHECK:
      if (function->IsBuiltin() || !function->shared()->is_classic_mode()) {
        Label fast;
        // Check that the object is a smi or a heap number.
        __ JumpIfSmi(rdx, &fast);
        __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rax);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, rax, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            rax, holder, rbx, rdx, rdi, name, &miss);
      } else {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      }
      break;

    case BOOLEAN_CHECK:
      if (function->IsBuiltin() || !function->shared()->is_classic_mode()) {
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
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            rax, holder, rbx, rdx, rdi, name, &miss);
      } else {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      }
      break;
  }

  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileCallInterceptor(Handle<JSObject> object,
                                                      Handle<JSObject> holder,
                                                      Handle<String> name) {
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

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), rcx, extra_state_);
  compiler.Compile(masm(), object, holder, name, &lookup, rdx, rbx, rdi, rax,
                   &miss);

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
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle load cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> CallStubCompiler::CompileCallGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
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
    Handle<Code> code = CompileCustomCall(object, holder, cell, function, name);
    // A null handle means bail out to the regular compiler code below.
    if (!code.is_null()) return code;
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

  // Set up the context (function already in rdi).
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_global_inline(), 1);
  ParameterCount expected(function->shared()->formal_parameter_count());
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  __ movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ InvokeCode(rdx, expected, arguments(), JUMP_FUNCTION,
                NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::NORMAL, name);
}


Handle<Code> StoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                  int index,
                                                  Handle<Map> transition,
                                                  Handle<String> name) {
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
                     name,
                     rdx, rcx, rbx, rdi,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition.is_null()
                 ? Code::FIELD
                 : Code::MAP_TRANSITION, name);
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  // Check that the maps haven't changed.
  __ JumpIfSmi(rdx, &miss);
  CheckPrototypes(receiver, rdx, holder, rbx, r8, rdi, name, &miss);

  // Stub never generated for non-global objects that require access checks.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());

  __ pop(rbx);  // remove the return address
  __ push(rdx);  // receiver
  __ Push(callback);  // callback info
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
  return GetCode(Code::CALLBACKS, name);
}


#undef __
#define __ ACCESS_MASM(masm)


void StoreStubCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ push(rax);

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      __ push(rdx);
      __ push(rax);
      ParameterCount actual(1);
      __ InvokeFunction(setter, actual, CALL_FUNCTION, NullCallWrapper(),
                        CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ pop(rax);

    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreViaSetter(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(rdx, &miss);
  CheckPrototypes(receiver, rdx, holder, rbx, r8, rdi, name, &miss);

  GenerateStoreViaSetter(masm(), setter);

  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> receiver,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the object hasn't changed.
  __ CheckMap(rdx, Handle<Map>(receiver->map()), &miss,
              DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

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
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> StoreStubCompiler::CompileStoreGlobal(
    Handle<GlobalObject> object,
    Handle<JSGlobalPropertyCell> cell,
    Handle<String> name) {
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

  // Compute the cell operand to use.
  __ Move(rbx, cell);
  Operand cell_operand = FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset);

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ CompareRoot(cell_operand, Heap::kTheHoleValueRootIndex);
  __ j(equal, &miss);

  // Store the value in the cell.
  __ movq(cell_operand, rax);
  // Cells are always rescanned, so no write barrier here.

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
  return GetCode(Code::NORMAL, name);
}


Handle<Code> KeyedStoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                       int index,
                                                       Handle<Map> transition,
                                                       Handle<String> name) {
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
  __ Cmp(rcx, name);
  __ j(not_equal, &miss);

  // Generate store field code.  Preserves receiver and name on jump to miss.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     name,
                     rdx, rcx, rbx, rdi,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_store_field(), 1);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition.is_null()
                 ? Code::FIELD
                 : Code::MAP_TRANSITION, name);
}


Handle<Code> KeyedStoreStubCompiler::CompileStoreElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
  Handle<Code> stub =
      KeyedStoreElementStub(is_js_array, elements_kind, grow_mode_).GetCode();

  __ DispatchMap(rdx, receiver_map, stub, DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string());
}


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(rdx, &miss, Label::kNear);

  __ movq(rdi, FieldOperand(rdx, HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int i = 0; i < receiver_count; ++i) {
    // Check map and tail call if there's a match
    __ Cmp(rdi, receiver_maps->at(i));
    if (transitioned_maps->at(i).is_null()) {
      __ j(equal, handler_stubs->at(i), RelocInfo::CODE_TARGET);
    } else {
      Label next_map;
      __ j(not_equal, &next_map, Label::kNear);
      __ movq(rbx, transitioned_maps->at(i), RelocInfo::EMBEDDED_OBJECT);
      __ jmp(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }

  __ bind(&miss);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<String> name,
                                                      Handle<JSObject> object,
                                                      Handle<JSObject> last) {
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
    GenerateCheckPropertyCell(
        masm(), Handle<GlobalObject>::cast(last), name, rdx, &miss);
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ ret(0);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::NONEXISTENT, factory()->empty_string());
}


Handle<Code> LoadStubCompiler::CompileLoadField(Handle<JSObject> object,
                                                Handle<JSObject> holder,
                                                int index,
                                                Handle<String> name) {
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
  return GetCode(Code::FIELD, name);
}


Handle<Code> LoadStubCompiler::CompileLoadCallback(
    Handle<String> name,
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  GenerateLoadCallback(object, holder, rax, rcx, rdx, rbx, rdi, r8, callback,
                       name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


#undef __
#define __ ACCESS_MASM(masm)


void LoadStubCompiler::GenerateLoadViaGetter(MacroAssembler* masm,
                                             Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      __ push(rax);
      ParameterCount actual(0);
      __ InvokeFunction(getter, actual, CALL_FUNCTION, NullCallWrapper(),
                        CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> LoadStubCompiler::CompileLoadViaGetter(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(rax, &miss);
  CheckPrototypes(receiver, rax, holder, rbx, rdx, rdi, name, &miss);

  GenerateLoadViaGetter(masm(), getter),

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> LoadStubCompiler::CompileLoadConstant(Handle<JSObject> object,
                                                   Handle<JSObject> holder,
                                                   Handle<JSFunction> value,
                                                   Handle<String> name) {
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
  return GetCode(Code::CONSTANT_FUNCTION, name);
}


Handle<Code> LoadStubCompiler::CompileLoadInterceptor(Handle<JSObject> receiver,
                                                      Handle<JSObject> holder,
                                                      Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver, holder, &lookup, rax, rcx, rdx, rbx, rdi,
                          name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> LoadStubCompiler::CompileLoadGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<String> name,
    bool is_dont_delete) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(rax, &miss);
  CheckPrototypes(object, rax, holder, rbx, rdx, rdi, name, &miss);

  // Get the value from the cell.
  __ Move(rbx, cell);
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
  return GetCode(Code::NORMAL, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadField(Handle<String> name,
                                                     Handle<JSObject> receiver,
                                                     Handle<JSObject> holder,
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
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadField(receiver, holder, rdx, rbx, rcx, rdi, index, name, &miss);

  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_field(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::FIELD, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadCallback(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- rax     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_callback(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadCallback(receiver, holder, rdx, rax, rbx, rcx, rdi, r8, callback,
                       name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_callback(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadConstant(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> value) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_constant_function(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadConstant(receiver, holder, rdx, rbx, rcx, rdi,
                       value, name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_constant_function(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CONSTANT_FUNCTION, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadInterceptor(
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_interceptor(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver, holder, &lookup, rdx, rax, rcx, rbx, rdi,
                          name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_interceptor(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadArrayLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_array_length(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadArrayLength(masm(), rdx, rcx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_array_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadStringLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadStringLength(masm(), rdx, rcx, rbx, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadFunctionPrototype(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1);

  // Check that the name has not changed.
  __ Cmp(rax, name);
  __ j(not_equal, &miss);

  GenerateLoadFunctionPrototype(masm(), rdx, rcx, rbx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  ElementsKind elements_kind = receiver_map->elements_kind();
  Handle<Code> stub = KeyedLoadElementStub(elements_kind).GetCode();

  __ DispatchMap(rdx, receiver_map, stub, DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string());
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadPolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_ics) {
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
    __ Cmp(map_reg, receiver_maps->at(current));
    __ j(equal, handler_ics->at(current), RelocInfo::CODE_TARGET);
  }

  __  bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
Handle<Code> ConstructStubCompiler::CompileConstructStub(
    Handle<JSFunction> function) {
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
  __ AllocateInNewSpace(rcx, rdx, rcx, no_reg,
                        &generic_stub_call, NO_ALLOCATION_FLAGS);

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
  Handle<SharedFunctionInfo> shared(function->shared());
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
  Handle<Code> code = isolate()->builtins()->JSConstructStubGeneric();
  __ Jump(code, RelocInfo::CODE_TARGET);

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


static void GenerateSmiKeyCheck(MacroAssembler* masm,
                                Register key,
                                Register scratch,
                                XMMRegister xmm_scratch0,
                                XMMRegister xmm_scratch1,
                                Label* fail) {
  // Check that key is a smi or a heap number containing a smi and branch
  // if the check fails.
  Label key_ok;
  __ JumpIfSmi(key, &key_ok);
  __ CheckMap(key,
              masm->isolate()->factory()->heap_number_map(),
              fail,
              DONT_DO_SMI_CHECK);
  __ movsd(xmm_scratch0, FieldOperand(key, HeapNumber::kValueOffset));
  __ cvttsd2si(scratch, xmm_scratch0);
  __ cvtlsi2sd(xmm_scratch1, scratch);
  __ ucomisd(xmm_scratch1, xmm_scratch0);
  __ j(not_equal, fail);
  __ j(parity_even, fail);  // NaN.
  __ Integer32ToSmi(key, scratch);
  __ bind(&key_ok);
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

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rax, rcx, xmm0, xmm1, &miss_force_generic);

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

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rcx, rbx, xmm0, xmm1, &miss_force_generic);

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
    case FAST_SMI_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
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
      // Fast path: use machine instruction to convert to int64. If that
      // fails (out-of-range), go into the runtime.
      __ cvttsd2siq(r8, xmm0);
      __ Set(kScratchRegister, V8_UINT64_C(0x8000000000000000));
      __ cmpq(r8, kScratchRegister);
      __ j(equal, &slow);

      // rdx: value (converted to an untagged integer)
      // rdi: untagged index
      // rbx: base pointer of external storage
      switch (elements_kind) {
        case EXTERNAL_BYTE_ELEMENTS:
        case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
          __ movb(Operand(rbx, rdi, times_1, 0), r8);
          break;
        case EXTERNAL_SHORT_ELEMENTS:
        case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
          __ movw(Operand(rbx, rdi, times_2, 0), r8);
          break;
        case EXTERNAL_INT_ELEMENTS:
        case EXTERNAL_UNSIGNED_INT_ELEMENTS:
          __ movl(Operand(rbx, rdi, times_4, 0), r8);
          break;
        case EXTERNAL_PIXEL_ELEMENTS:
        case EXTERNAL_FLOAT_ELEMENTS:
        case EXTERNAL_DOUBLE_ELEMENTS:
        case FAST_ELEMENTS:
        case FAST_SMI_ELEMENTS:
        case FAST_DOUBLE_ELEMENTS:
        case FAST_HOLEY_ELEMENTS:
        case FAST_HOLEY_SMI_ELEMENTS:
        case FAST_HOLEY_DOUBLE_ELEMENTS:
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

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rax, rcx, xmm0, xmm1, &miss_force_generic);

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

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rax, rcx, xmm0, xmm1, &miss_force_generic);

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


void KeyedStoreStubCompiler::GenerateStoreFastElement(
    MacroAssembler* masm,
    bool is_js_array,
    ElementsKind elements_kind,
    KeyedAccessGrowMode grow_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, finish_store, grow;
  Label check_capacity, slow;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rcx, rbx, xmm0, xmm1, &miss_force_generic);

  if (IsFastSmiElementsKind(elements_kind)) {
    __ JumpIfNotSmi(rax, &transition_elements_kind);
  }

  // Get the elements array and make sure it is a fast element array, not 'cow'.
  __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
  // Check that the key is within bounds.
  if (is_js_array) {
    __ SmiCompare(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
    if (grow_mode == ALLOW_JSARRAY_GROWTH) {
      __ j(above_equal, &grow);
    } else {
      __ j(above_equal, &miss_force_generic);
    }
  } else {
    __ SmiCompare(rcx, FieldOperand(rdi, FixedArray::kLengthOffset));
    __ j(above_equal, &miss_force_generic);
  }

  __ CompareRoot(FieldOperand(rdi, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &miss_force_generic);

  __ bind(&finish_store);
  if (IsFastSmiElementsKind(elements_kind)) {
    __ SmiToInteger32(rcx, rcx);
    __ movq(FieldOperand(rdi, rcx, times_pointer_size, FixedArray::kHeaderSize),
            rax);
  } else {
    // Do the store and update the write barrier.
    ASSERT(IsFastObjectElementsKind(elements_kind));
    __ SmiToInteger32(rcx, rcx);
    __ lea(rcx,
           FieldOperand(rdi, rcx, times_pointer_size, FixedArray::kHeaderSize));
    __ movq(Operand(rcx, 0), rax);
    // Make sure to preserve the value in register rax.
    __ movq(rbx, rax);
    __ RecordWrite(rdi, rcx, rbx, kDontSaveFPRegs);
  }

  // Done.
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);

  __ bind(&transition_elements_kind);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags are already set by previous
    // compare.
    __ j(not_equal, &miss_force_generic);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
    __ CompareRoot(rdi, Heap::kEmptyFixedArrayRootIndex);
    __ j(not_equal, &check_capacity);

    int size = FixedArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, rdi, rbx, r8, &slow, TAG_OBJECT);

    // rax: value
    // rcx: key
    // rdx: receiver
    // rdi: elements
    // Make sure that the backing store can hold additional elements.
    __ Move(FieldOperand(rdi, JSObject::kMapOffset),
            masm->isolate()->factory()->fixed_array_map());
    __ Move(FieldOperand(rdi, FixedArray::kLengthOffset),
            Smi::FromInt(JSArray::kPreallocatedArrayElements));
    __ LoadRoot(rbx, Heap::kTheHoleValueRootIndex);
    for (int i = 1; i < JSArray::kPreallocatedArrayElements; ++i) {
      __ movq(FieldOperand(rdi, FixedArray::SizeFor(i)), rbx);
    }

    // Store the element at index zero.
    __ movq(FieldOperand(rdi, FixedArray::SizeFor(0)), rax);

    // Install the new backing store in the JSArray.
    __ movq(FieldOperand(rdx, JSObject::kElementsOffset), rdi);
    __ RecordWriteField(rdx, JSObject::kElementsOffset, rdi, rbx,
                        kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ Move(FieldOperand(rdx, JSArray::kLengthOffset), Smi::FromInt(1));
    __ ret(0);

    __ bind(&check_capacity);
    // Check for cow elements, in general they are not handled by this stub.
    __ CompareRoot(FieldOperand(rdi, HeapObject::kMapOffset),
                   Heap::kFixedCOWArrayMapRootIndex);
    __ j(equal, &miss_force_generic);

    // rax: value
    // rcx: key
    // rdx: receiver
    // rdi: elements
    // Make sure that the backing store can hold additional elements.
    __ cmpq(rcx, FieldOperand(rdi, FixedArray::kLengthOffset));
    __ j(above_equal, &slow);

    // Grow the array and finish the store.
    __ SmiAddConstant(FieldOperand(rdx, JSArray::kLengthOffset),
                      Smi::FromInt(1));
    __ jmp(&finish_store);

    __ bind(&slow);
    Handle<Code> ic_slow = masm->isolate()->builtins()->KeyedStoreIC_Slow();
    __ jmp(ic_slow, RelocInfo::CODE_TARGET);
  }
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array,
    KeyedAccessGrowMode grow_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, finish_store;
  Label grow, slow, check_capacity;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, rcx, rbx, xmm0, xmm1, &miss_force_generic);

  // Get the elements array.
  __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
  __ AssertFastElements(rdi);

  // Check that the key is within bounds.
  if (is_js_array) {
      __ SmiCompare(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
      if (grow_mode == ALLOW_JSARRAY_GROWTH) {
        __ j(above_equal, &grow);
      } else {
        __ j(above_equal, &miss_force_generic);
      }
  } else {
    __ SmiCompare(rcx, FieldOperand(rdi, FixedDoubleArray::kLengthOffset));
    __ j(above_equal, &miss_force_generic);
  }

  // Handle smi values specially
  __ bind(&finish_store);
  __ SmiToInteger32(rcx, rcx);
  __ StoreNumberToDoubleElements(rax, rdi, rcx, xmm0,
                                 &transition_elements_kind);
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);

  __ bind(&transition_elements_kind);
  // Restore smi-tagging of rcx.
  __ Integer32ToSmi(rcx, rcx);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags are already set by previous
    // compare.
    __ j(not_equal, &miss_force_generic);

    // Transition on values that can't be stored in a FixedDoubleArray.
    Label value_is_smi;
    __ JumpIfSmi(rax, &value_is_smi);
    __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &transition_elements_kind);
    __ bind(&value_is_smi);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
    __ CompareRoot(rdi, Heap::kEmptyFixedArrayRootIndex);
    __ j(not_equal, &check_capacity);

    int size = FixedDoubleArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, rdi, rbx, r8, &slow, TAG_OBJECT);

    // rax: value
    // rcx: key
    // rdx: receiver
    // rdi: elements
    // Initialize the new FixedDoubleArray. Leave elements unitialized for
    // efficiency, they are guaranteed to be initialized before use.
    __ Move(FieldOperand(rdi, JSObject::kMapOffset),
            masm->isolate()->factory()->fixed_double_array_map());
    __ Move(FieldOperand(rdi, FixedDoubleArray::kLengthOffset),
            Smi::FromInt(JSArray::kPreallocatedArrayElements));

    // Install the new backing store in the JSArray.
    __ movq(FieldOperand(rdx, JSObject::kElementsOffset), rdi);
    __ RecordWriteField(rdx, JSObject::kElementsOffset, rdi, rbx,
                        kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ Move(FieldOperand(rdx, JSArray::kLengthOffset), Smi::FromInt(1));
    __ movq(rdi, FieldOperand(rdx, JSObject::kElementsOffset));
    __ jmp(&finish_store);

    __ bind(&check_capacity);
    // rax: value
    // rcx: key
    // rdx: receiver
    // rdi: elements
    // Make sure that the backing store can hold additional elements.
    __ cmpq(rcx, FieldOperand(rdi, FixedDoubleArray::kLengthOffset));
    __ j(above_equal, &slow);

    // Grow the array and finish the store.
    __ SmiAddConstant(FieldOperand(rdx, JSArray::kLengthOffset),
                      Smi::FromInt(1));
    __ jmp(&finish_store);

    __ bind(&slow);
    Handle<Code> ic_slow = masm->isolate()->builtins()->KeyedStoreIC_Slow();
    __ jmp(ic_slow, RelocInfo::CODE_TARGET);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
