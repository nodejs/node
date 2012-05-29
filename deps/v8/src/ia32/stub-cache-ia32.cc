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
                       Register receiver,
                       // Number of the cache entry pointer-size scaled.
                       Register offset,
                       Register extra) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ lea(offset, Operand(offset, offset, times_2, 0));

  if (extra.is_valid()) {
    // Get the code entry from the cache.
    __ mov(extra, Operand::StaticArray(offset, times_1, value_offset));

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_1, key_offset));
    __ j(not_equal, &miss);

    // Check the map matches.
    __ mov(offset, Operand::StaticArray(offset, times_1, map_offset));
    __ cmp(offset, FieldOperand(receiver, HeapObject::kMapOffset));
    __ j(not_equal, &miss);

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(extra, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
    __ j(not_equal, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

    // Jump to the first instruction in the code stub.
    __ add(extra, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(extra);

    __ bind(&miss);
  } else {
    // Save the offset on the stack.
    __ push(offset);

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_1, key_offset));
    __ j(not_equal, &miss);

    // Check the map matches.
    __ mov(offset, Operand::StaticArray(offset, times_1, map_offset));
    __ cmp(offset, FieldOperand(receiver, HeapObject::kMapOffset));
    __ j(not_equal, &miss);

    // Restore offset register.
    __ mov(offset, Operand(esp, 0));

    // Get the code entry from the cache.
    __ mov(offset, Operand::StaticArray(offset, times_1, value_offset));

    // Check that the flags match what we're looking for.
    __ mov(offset, FieldOperand(offset, Code::kFlagsOffset));
    __ and_(offset, ~Code::kFlagsNotUsedInLookup);
    __ cmp(offset, flags);
    __ j(not_equal, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

    // Restore offset and re-load code entry from cache.
    __ pop(offset);
    __ mov(offset, Operand::StaticArray(offset, times_1, value_offset));

    // Jump to the first instruction in the code stub.
    __ add(offset, Immediate(Code::kHeaderSize - kHeapObjectTag));
    __ jmp(offset);

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
  Label miss;

  // Assert that code is valid.  The multiplying code relies on the entry size
  // being 12.
  ASSERT(sizeof(Entry) == 12);

  // Assert the flags do not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Assert that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));
  ASSERT(!extra.is(receiver));
  ASSERT(!extra.is(name));
  ASSERT(!extra.is(scratch));

  // Assert scratch and extra registers are valid, and extra2/3 are unused.
  ASSERT(!scratch.is(no_reg));
  ASSERT(extra2.is(no_reg));
  ASSERT(extra3.is(no_reg));

  Register offset = scratch;
  scratch = no_reg;

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ mov(offset, FieldOperand(name, String::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ and_(offset, (kPrimaryTableSize - 1) << kHeapObjectTagSize);
  // ProbeTable expects the offset to be pointer scaled, which it is, because
  // the heap object tag size is 2 and the pointer size log 2 is also 2.
  ASSERT(kHeapObjectTagSize == kPointerSizeLog2);

  // Probe the primary table.
  ProbeTable(isolate(), masm, flags, kPrimary, name, receiver, offset, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(offset, FieldOperand(name, String::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  __ and_(offset, (kPrimaryTableSize - 1) << kHeapObjectTagSize);
  __ sub(offset, name);
  __ add(offset, Immediate(flags));
  __ and_(offset, (kSecondaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the secondary table.
  ProbeTable(
      isolate(), masm, flags, kSecondary, name, receiver, offset, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
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
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
  // Check we're still in the same context.
  __ cmp(Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)),
         masm->isolate()->global());
  __ j(not_equal, miss);
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(masm->isolate()->global_context()->get(index)));
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
  __ mov(eax, scratch1);
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
                                     Handle<JSObject> holder_obj) {
  __ push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ mov(scratch, Immediate(interceptor));
  __ push(scratch);
  __ push(receiver);
  __ push(holder);
  __ push(FieldOperand(scratch, InterceptorInfo::kDataOffset));
  __ push(Immediate(reinterpret_cast<int>(masm->isolate())));
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly),
                        masm->isolate()),
      6);
}


// Number of pointers to be reserved on stack for fast API call.
static const int kFastApiCallArguments = 4;


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
  __ add(esp, Immediate(kPointerSize * kFastApiCallArguments));
  __ push(scratch);
}


// Generates call to API function.
static void GenerateFastApiCall(MacroAssembler* masm,
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
  //  -- esp[16]             : isolate
  //  -- esp[20]             : last argument
  //  -- ...
  //  -- esp[(argc + 4) * 4] : first argument
  //  -- esp[(argc + 5) * 4] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  Handle<JSFunction> function = optimization.constant_function();
  __ LoadHeapObject(edi, function);
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Pass the additional arguments.
  __ mov(Operand(esp, 2 * kPointerSize), edi);
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data(api_call_info->data());
  if (masm->isolate()->heap()->InNewSpace(*call_data)) {
    __ mov(ecx, api_call_info);
    __ mov(ebx, FieldOperand(ecx, CallHandlerInfo::kDataOffset));
    __ mov(Operand(esp, 3 * kPointerSize), ebx);
  } else {
    __ mov(Operand(esp, 3 * kPointerSize), Immediate(call_data));
  }
  __ mov(Operand(esp, 4 * kPointerSize),
         Immediate(reinterpret_cast<int>(masm->isolate())));

  // Prepare arguments.
  __ lea(eax, Operand(esp, 4 * kPointerSize));

  const int kApiArgc = 1;  // API function gets reference to the v8::Arguments.

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  __ PrepareCallApiFunction(kApiArgc + kApiStackSpace);

  __ mov(ApiParameterOperand(1), eax);  // v8::Arguments::implicit_args_.
  __ add(eax, Immediate(argc * kPointerSize));
  __ mov(ApiParameterOperand(2), eax);  // v8::Arguments::values_.
  __ Set(ApiParameterOperand(3), Immediate(argc));  // v8::Arguments::length_.
  // v8::Arguments::is_construct_call_.
  __ Set(ApiParameterOperand(4), Immediate(0));

  // v8::InvocationCallback's argument.
  __ lea(eax, ApiParameterOperand(1));
  __ mov(ApiParameterOperand(0), eax);

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
                          Code::ExtraICState extra_state)
      : stub_compiler_(stub_compiler),
        arguments_(arguments),
        name_(name),
        extra_state_(extra_state) {}

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
      CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
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

    __ cmp(eax, masm->isolate()->factory()->no_interceptor_result_sentinel());
    __ j(not_equal, interceptor_succeeded);
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
  Code::ExtraICState extra_state_;
};


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  ASSERT(kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC);
  Handle<Code> code = (kind == Code::LOAD_IC)
      ? masm->isolate()->builtins()->LoadIC_Miss()
      : masm->isolate()->builtins()->KeyedLoadIC_Miss();
  __ jmp(code, RelocInfo::CODE_TARGET);
}


void StubCompiler::GenerateKeyedLoadMissForceGeneric(MacroAssembler* masm) {
  Handle<Code> code =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(code, RelocInfo::CODE_TARGET);
}


// Both name_reg and receiver_reg are preserved on jumps to miss_label,
// but may be destroyed if store is successful.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      Handle<JSObject> object,
                                      int index,
                                      Handle<Map> transition,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch,
                                      Label* miss_label) {
  // Check that the map of the object hasn't changed.
  CompareMapMode mode = transition.is_null() ? ALLOW_ELEMENT_TRANSITION_MAPS
                                             : REQUIRE_EXACT_MAP;
  __ CheckMap(receiver_reg, Handle<Map>(object->map()),
              miss_label, DO_SMI_CHECK, mode);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver_reg, scratch, miss_label);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if (!transition.is_null() && (object->map()->unused_property_fields() == 0)) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ pop(scratch);  // Return address.
    __ push(receiver_reg);
    __ push(Immediate(transition));
    __ push(eax);
    __ push(scratch);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  if (!transition.is_null()) {
    // Update the map of the object; no write barrier updating is
    // needed because the map is never in new space.
    __ mov(FieldOperand(receiver_reg, HeapObject::kMapOffset),
           Immediate(transition));
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
    __ mov(name_reg, eax);
    __ RecordWriteField(receiver_reg,
                        offset,
                        name_reg,
                        scratch,
                        kDontSaveFPRegs);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ mov(scratch, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ mov(FieldOperand(scratch, offset), eax);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ mov(name_reg, eax);
    __ RecordWriteField(scratch,
                        offset,
                        name_reg,
                        receiver_reg,
                        kDontSaveFPRegs);
  }

  // Return the value (register eax).
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
  Handle<Oddball> the_hole = masm->isolate()->factory()->the_hole_value();
  if (Serializer::enabled()) {
    __ mov(scratch, Immediate(cell));
    __ cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
           Immediate(the_hole));
  } else {
    __ cmp(Operand::Cell(cell), Immediate(the_hole));
  }
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
#define __ ACCESS_MASM(masm())


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

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  Handle<JSObject> current = object;
  int depth = 0;

  if (save_at_depth == depth) {
    __ mov(Operand(esp, kPointerSize), reg);
  }

  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
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

      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool in_new_space = heap()->InNewSpace(*prototype);
      Handle<Map> current_map(current->map());
      if (in_new_space) {
        // Save the map in scratch1 for later.
        __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK,
                  ALLOW_ELEMENT_TRANSITION_MAPS);

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
        __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ mov(reg, prototype);
      }
    }

    if (save_at_depth == depth) {
      __ mov(Operand(esp, kPointerSize), reg);
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
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());
  if (holder->IsJSGlobalProxy()) {
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
  GenerateFastPropertyLoad(masm(), eax, reg, holder, index);
  __ ret(0);
}


void StubCompiler::GenerateLoadCallback(Handle<JSObject> object,
                                        Handle<JSObject> holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Handle<AccessorInfo> callback,
                                        Handle<String> name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  Register reg = CheckPrototypes(object, receiver, holder, scratch1,
                                 scratch2, scratch3, name, miss);

  // Insert additional parameters into the stack frame above return address.
  ASSERT(!scratch3.is(reg));
  __ pop(scratch3);  // Get return address to place it below.

  __ push(receiver);  // receiver
  __ mov(scratch2, esp);
  ASSERT(!scratch2.is(reg));
  __ push(reg);  // holder
  // Push data from AccessorInfo.
  if (isolate()->heap()->InNewSpace(callback->data())) {
    __ mov(scratch1, Immediate(callback));
    __ push(FieldOperand(scratch1, AccessorInfo::kDataOffset));
  } else {
    __ push(Immediate(Handle<Object>(callback->data())));
  }
  __ push(Immediate(reinterpret_cast<int>(isolate())));

  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const AccessorInfo& to the C++ callback.
  __ push(scratch2);

  __ push(name_reg);  // name
  __ mov(ebx, esp);  // esp points to reference to name (handler).

  __ push(scratch3);  // Restore return address.

  // 4 elements array for v8::Arguments::values_, handler for name and pointer
  // to the values (it considered as smi in GC).
  const int kStackSpace = 6;
  const int kApiArgc = 2;

  __ PrepareCallApiFunction(kApiArgc);
  __ mov(ApiParameterOperand(0), ebx);  // name.
  __ add(ebx, Immediate(kPointerSize));
  __ mov(ApiParameterOperand(1), ebx);  // arguments pointer.

  // Emitting a stub call may try to allocate (if the code is not
  // already generated).  Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
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
  __ LoadHeapObject(eax, value);
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
    if (lookup->type() == FIELD) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
               lookup->GetCallbackObject()->IsAccessorInfo()) {
      compile_followup_inline =
          AccessorInfo::cast(lookup->GetCallbackObject())->getter() != NULL;
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
      __ cmp(eax, factory()->no_interceptor_result_sentinel());
      __ j(equal, &interceptor_failed);
      frame_scope.GenerateLeaveFrame();
      __ ret(0);

      // Clobber registers when generating debug-code to provoke errors.
      __ bind(&interceptor_failed);
      if (FLAG_debug_code) {
        __ mov(receiver, Immediate(BitCast<int32_t>(kZapValue)));
        __ mov(holder_reg, Immediate(BitCast<int32_t>(kZapValue)));
        __ mov(name_reg, Immediate(BitCast<int32_t>(kZapValue)));
      }

      __ pop(name_reg);
      __ pop(holder_reg);
      if (must_preserve_receiver_reg) {
        __ pop(receiver);
      }

      // Leave the internal frame.
    }

    // Check that the maps from interceptor's holder to lookup's holder
    // haven't changed.  And load lookup's holder into holder_reg.
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

    if (lookup->type() == FIELD) {
      // We found FIELD property in prototype chain of interceptor's holder.
      // Retrieve a field from field's holder.
      GenerateFastPropertyLoad(masm(), eax, holder_reg,
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
      __ mov(holder_reg, Immediate(callback));
      __ push(FieldOperand(holder_reg, AccessorInfo::kDataOffset));
      __ push(Immediate(reinterpret_cast<int>(isolate())));
      __ push(holder_reg);
      __ push(name_reg);
      __ push(scratch2);  // restore return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            masm()->isolate());
      __ TailCallExternalReference(ref, 6, 1);
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
    __ TailCallExternalReference(ref, 6, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(Handle<String> name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ cmp(ecx, Immediate(name));
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
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));


  // Check that the maps haven't changed.
  __ JumpIfSmi(edx, miss);
  CheckPrototypes(object, edx, holder, ebx, eax, edi, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Label* miss) {
  // Get the value from the cell.
  if (Serializer::enabled()) {
    __ mov(edi, Immediate(cell));
    __ mov(edi, FieldOperand(edi, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(edi, Operand::Cell(cell));
  }

  // Check that the cell contains the same function.
  if (isolate()->heap()->InNewSpace(*function)) {
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
  } else {
    __ cmp(edi, Immediate(function));
  }
  __ j(not_equal, miss);
}


void CallStubCompiler::GenerateMissBranch() {
  Handle<Code> code =
      isolate()->stub_cache()->ComputeCallMiss(arguments().immediate(),
                                               kind_,
                                               extra_state_);
  __ jmp(code, RelocInfo::CODE_TARGET);
}


Handle<Code> CallStubCompiler::CompileCallField(Handle<JSObject> object,
                                                Handle<JSObject> holder,
                                                int index,
                                                Handle<String> name) {
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
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(FIELD, name);
}


Handle<Code> CallStubCompiler::CompileArrayPushCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) {
    return Handle<Code>::null();
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);

  CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                  name, &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    Label call_builtin;

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label attempt_to_grow_elements, with_write_barrier;

      // Get the elements array of the object.
      __ mov(edi, FieldOperand(edx, JSArray::kElementsOffset));

      // Check that the elements are in fast mode and writable.
      __ cmp(FieldOperand(edi, HeapObject::kMapOffset),
             Immediate(factory()->fixed_array_map()));
      __ j(not_equal, &call_builtin);

      // Get the array's length into eax and calculate new length.
      __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ add(eax, Immediate(Smi::FromInt(argc)));

      // Get the elements' length into ecx.
      __ mov(ecx, FieldOperand(edi, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmp(eax, ecx);
      __ j(greater, &attempt_to_grow_elements);

      // Check if value is a smi.
      __ mov(ecx, Operand(esp, argc * kPointerSize));
      __ JumpIfNotSmi(ecx, &with_write_barrier);

      // Save new length.
      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      // Store the value.
      __ mov(FieldOperand(edi,
                          eax,
                          times_half_pointer_size,
                          FixedArray::kHeaderSize - argc * kPointerSize),
             ecx);

      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));

      if (FLAG_smi_only_arrays  && !FLAG_trace_elements_transitions) {
        Label fast_object, not_fast_object;
        __ CheckFastObjectElements(ebx, &not_fast_object, Label::kNear);
        __ jmp(&fast_object);
        // In case of fast smi-only, convert to fast object, otherwise bail out.
        __ bind(&not_fast_object);
        __ CheckFastSmiElements(ebx, &call_builtin);
        // edi: elements array
        // edx: receiver
        // ebx: map
        Label try_holey_map;
        __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                               FAST_ELEMENTS,
                                               ebx,
                                               edi,
                                               &try_holey_map);

        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        // Restore edi.
        __ mov(edi, FieldOperand(edx, JSArray::kElementsOffset));
        __ jmp(&fast_object);

        __ bind(&try_holey_map);
        __ LoadTransitionedArrayMapConditional(FAST_HOLEY_SMI_ELEMENTS,
                                               FAST_HOLEY_ELEMENTS,
                                               ebx,
                                               edi,
                                               &call_builtin);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        // Restore edi.
        __ mov(edi, FieldOperand(edx, JSArray::kElementsOffset));
        __ bind(&fast_object);
      } else {
        __ CheckFastObjectElements(ebx, &call_builtin);
      }

      // Save new length.
      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      // Store the value.
      __ lea(edx, FieldOperand(edi,
                               eax, times_half_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ mov(Operand(edx, 0), ecx);

      __ RecordWrite(edi, edx, ecx, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                     OMIT_SMI_CHECK);

      __ ret((argc + 1) * kPointerSize);

      __ bind(&attempt_to_grow_elements);
      if (!FLAG_inline_new) {
        __ jmp(&call_builtin);
      }

      __ mov(ebx, Operand(esp, argc * kPointerSize));
      // Growing elements that are SMI-only requires special handling in case
      // the new element is non-Smi. For now, delegate to the builtin.
      Label no_fast_elements_check;
      __ JumpIfSmi(ebx, &no_fast_elements_check);
      __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
      __ CheckFastObjectElements(ecx, &call_builtin, Label::kFar);
      __ bind(&no_fast_elements_check);

      // We could be lucky and the elements array could be at the top of
      // new-space.  In this case we can just grow it in place by moving the
      // allocation pointer up.

      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(isolate());
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(isolate());

      const int kAllocationDelta = 4;
      // Load top.
      __ mov(ecx, Operand::StaticVariable(new_space_allocation_top));

      // Check if it's the end of elements.
      __ lea(edx, FieldOperand(edi,
                               eax, times_half_pointer_size,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ cmp(edx, ecx);
      __ j(not_equal, &call_builtin);
      __ add(ecx, Immediate(kAllocationDelta * kPointerSize));
      __ cmp(ecx, Operand::StaticVariable(new_space_allocation_limit));
      __ j(above, &call_builtin);

      // We fit and could grow elements.
      __ mov(Operand::StaticVariable(new_space_allocation_top), ecx);

      // Push the argument...
      __ mov(Operand(edx, 0), ebx);
      // ... and fill the rest with holes.
      for (int i = 1; i < kAllocationDelta; i++) {
        __ mov(Operand(edx, i * kPointerSize),
               Immediate(factory()->the_hole_value()));
      }

      // We know the elements array is in new space so we don't need the
      // remembered set, but we just pushed a value onto it so we may have to
      // tell the incremental marker to rescan the object that we just grew.  We
      // don't need to worry about the holes because they are in old space and
      // already marked black.
      __ RecordWrite(edi, edx, ebx, kDontSaveFPRegs, OMIT_REMEMBERED_SET);

      // Restore receiver to edx as finish sequence assumes it's here.
      __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ add(FieldOperand(edi, FixedArray::kLengthOffset),
             Immediate(Smi::FromInt(kAllocationDelta)));

      // NOTE: This only happen in new-space, where we don't
      // care about the black-byte-count on pages. Otherwise we should
      // update that too if the object is black.

      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      __ ret((argc + 1) * kPointerSize);
    }

    __ bind(&call_builtin);
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate()),
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
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) {
    return Handle<Code>::null();
  }

  Label miss, return_undefined, call_builtin;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);
  CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                  name, &miss);

  // Get the elements array of the object.
  __ mov(ebx, FieldOperand(edx, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(factory()->fixed_array_map()));
  __ j(not_equal, &call_builtin);

  // Get the array's length into ecx and calculate new length.
  __ mov(ecx, FieldOperand(edx, JSArray::kLengthOffset));
  __ sub(ecx, Immediate(Smi::FromInt(1)));
  __ j(negative, &return_undefined);

  // Get the last element.
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(eax, FieldOperand(ebx,
                           ecx, times_half_pointer_size,
                           FixedArray::kHeaderSize));
  __ cmp(eax, Immediate(factory()->the_hole_value()));
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
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) {
    return Handle<Code>::null();
  }

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
                                            eax,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  eax, holder, ebx, edx, edi, name, &miss);

  Register receiver = ebx;
  Register index = edi;
  Register result = eax;
  __ mov(receiver, Operand(esp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ mov(index, Operand(esp, (argc - 0) * kPointerSize));
  } else {
    __ Set(index, Immediate(factory()->undefined_value()));
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
    __ Set(eax, Immediate(factory()->nan_value()));
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in ecx.
  __ Set(ecx, Immediate(name));
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
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) {
    return Handle<Code>::null();
  }

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
                                            eax,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  eax, holder, ebx, edx, edi, name, &miss);

  Register receiver = eax;
  Register index = edi;
  Register scratch = edx;
  Register result = eax;
  __ mov(receiver, Operand(esp, (argc + 1) * kPointerSize));
  if (argc > 0) {
    __ mov(index, Operand(esp, (argc - 0) * kPointerSize));
  } else {
    __ Set(index, Immediate(factory()->undefined_value()));
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
    __ Set(eax, Immediate(factory()->empty_string()));
    __ ret((argc + 1) * kPointerSize);
  }

  __ bind(&miss);
  // Restore function name in ecx.
  __ Set(ecx, Immediate(name));
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
    return Handle<Code>::null();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
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

  StringCharFromCodeGenerator generator(code, eax);
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
  // ecx: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathFloorCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  if (!CpuFeatures::IsSupported(SSE2)) {
    return Handle<Code>::null();
  }

  CpuFeatures::Scope use_sse2(SSE2);

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) {
    return Handle<Code>::null();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);

    CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
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
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathAbsCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
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
    return Handle<Code>::null();
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ mov(edx, Operand(esp, 2 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(edx, &miss);

    CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
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
  __ xor_(eax, ebx);

  // Add 1 or do nothing depending on ebx.
  __ sub(eax, ebx);

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
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(NORMAL, name);
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
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss_before_stack_reserved);

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_const(), 1);
  __ IncrementCounter(counters->call_const_fast_api(), 1);

  // Allocate space for v8::Arguments implicit values. Must be initialized
  // before calling any runtime function.
  __ sub(esp, Immediate(kFastApiCallArguments * kPointerSize));

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax, edi,
                  name, depth, &miss);

  // Move the return address on top of the stack.
  __ mov(eax, Operand(esp, 4 * kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), eax);

  // esp[2 * kPointerSize] is uninitialized, esp[3 * kPointerSize] contains
  // duplicate of return address and will be overwritten.
  GenerateFastApiCall(masm(), optimization, argc);

  __ bind(&miss);
  __ add(esp, Immediate(kFastApiCallArguments * kPointerSize));

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
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
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
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(edx, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(isolate()->counters()->call_const(), 1);

      // Check that the maps haven't changed.
      CheckPrototypes(Handle<JSObject>::cast(object), edx, holder, ebx, eax,
                      edi, name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
        __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
      }
      break;

    case STRING_CHECK:
      if (function->IsBuiltin() || !function->shared()->is_classic_mode()) {
        // Check that the object is a string or a symbol.
        __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, eax);
        __ j(above_equal, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            eax, holder, ebx, edx, edi, name, &miss);
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
        __ JumpIfSmi(edx, &fast);
        __ CmpObjectType(edx, HEAP_NUMBER_TYPE, eax);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            eax, holder, ebx, edx, edi, name, &miss);
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
        __ cmp(edx, factory()->true_value());
        __ j(equal, &fast);
        __ cmp(edx, factory()->false_value());
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, eax, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            eax, holder, ebx, edx, edi, name, &miss);
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

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), ecx, extra_state_);
  compiler.Compile(masm(), object, holder, name, &lookup, edx, ebx, edi, eax,
                   &miss);

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
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION,
                    NullCallWrapper(), call_kind);

  // Handle load cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Handle<Code> CallStubCompiler::CompileCallGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
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
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Set up the context (function already in edi).
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

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
  __ InvokeCode(FieldOperand(edi, JSFunction::kCodeEntryOffset),
                expected, arguments(), JUMP_FUNCTION,
                NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Handle<Code> StoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                  int index,
                                                  Handle<Map> transition,
                                                  Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(), object, index, transition, edx, ecx, ebx, &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(ecx, Immediate(name));  // restore name
  Handle<Code> ic = isolate()->builtins()->StoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition.is_null() ? FIELD : MAP_TRANSITION, name);
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<JSObject> object,
    Handle<AccessorInfo> callback,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the object hasn't changed.
  __ CheckMap(edx, Handle<Map>(object->map()),
              &miss, DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(edx, ebx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ pop(ebx);  // remove the return address
  __ push(edx);  // receiver
  __ push(Immediate(callback));  // callback info
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


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> receiver,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the object hasn't changed.
  __ CheckMap(edx, Handle<Map>(receiver->map()),
              &miss, DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

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


Handle<Code> StoreStubCompiler::CompileStoreGlobal(
    Handle<GlobalObject> object,
    Handle<JSGlobalPropertyCell> cell,
    Handle<String> name) {
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
  __ mov(ebx, Immediate(cell));
  Operand cell_operand = FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset);

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ cmp(cell_operand, factory()->the_hole_value());
  __ j(equal, &miss);

  // Store the value in the cell.
  __ mov(cell_operand, eax);
  // No write barrier here, because cells are always rescanned.

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


Handle<Code> KeyedStoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                       int index,
                                                       Handle<Map> transition,
                                                       Handle<String> name) {
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
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(), object, index, transition, edx, ecx, ebx, &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_store_field(), 1);
  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition.is_null() ? FIELD : MAP_TRANSITION, name);
}


Handle<Code> KeyedStoreStubCompiler::CompileStoreElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_jsarray = receiver_map->instance_type() == JS_ARRAY_TYPE;
  Handle<Code> stub =
      KeyedStoreElementStub(is_jsarray, elements_kind, grow_mode_).GetCode();

  __ DispatchMap(edx, receiver_map, stub, DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, factory()->empty_string());
}


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(edx, &miss, Label::kNear);
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  // ebx: receiver->map().
  for (int i = 0; i < receiver_maps->length(); ++i) {
    __ cmp(edi, receiver_maps->at(i));
    if (transitioned_maps->at(i).is_null()) {
      __ j(equal, handler_stubs->at(i));
    } else {
      Label next_map;
      __ j(not_equal, &next_map, Label::kNear);
      __ mov(ebx, Immediate(transitioned_maps->at(i)));
      __ jmp(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }
  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(miss_ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<String> name,
                                                      Handle<JSObject> object,
                                                      Handle<JSObject> last) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &miss);

  ASSERT(last->IsGlobalObject() || last->HasFastProperties());

  // Check the maps of the full prototype chain. Also check that
  // global property cells up to (but not including) the last object
  // in the prototype chain are empty.
  CheckPrototypes(object, edx, last, ebx, eax, edi, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    GenerateCheckPropertyCell(
        masm(), Handle<GlobalObject>::cast(last), name, eax, &miss);
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ mov(eax, isolate()->factory()->undefined_value());
  __ ret(0);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NONEXISTENT, factory()->empty_string());
}


Handle<Code> LoadStubCompiler::CompileLoadField(Handle<JSObject> object,
                                                Handle<JSObject> holder,
                                                int index,
                                                Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadField(object, holder, edx, ebx, eax, edi, index, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Handle<Code> LoadStubCompiler::CompileLoadCallback(
    Handle<String> name,
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadCallback(object, holder, edx, ecx, ebx, eax, edi, callback,
                       name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Handle<Code> LoadStubCompiler::CompileLoadConstant(Handle<JSObject> object,
                                                   Handle<JSObject> holder,
                                                   Handle<JSFunction> value,
                                                   Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateLoadConstant(object, holder, edx, ebx, eax, edi, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Handle<Code> LoadStubCompiler::CompileLoadInterceptor(Handle<JSObject> receiver,
                                                      Handle<JSObject> holder,
                                                      Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver, holder, &lookup, edx, ecx, eax, ebx, edi,
                          name, &miss);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Handle<Code> LoadStubCompiler::CompileLoadGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<String> name,
    bool is_dont_delete) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(edx, &miss);
  CheckPrototypes(object, edx, holder, ebx, eax, edi, name, &miss);

  // Get the value from the cell.
  if (Serializer::enabled()) {
    __ mov(ebx, Immediate(cell));
    __ mov(ebx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(ebx, Operand::Cell(cell));
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


Handle<Code> KeyedLoadStubCompiler::CompileLoadField(Handle<String> name,
                                                     Handle<JSObject> receiver,
                                                     Handle<JSObject> holder,
                                                     int index) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_field(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadField(receiver, holder, edx, ebx, eax, edi, index, name, &miss);

  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_field(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadCallback(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_callback(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadCallback(receiver, holder, edx, ecx, ebx, eax, edi, callback,
                       name, &miss);

  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_callback(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadConstant(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> value) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_constant_function(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadConstant(
      receiver, holder, edx, ebx, eax, edi, value, name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_constant_function(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadInterceptor(
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_interceptor(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver, holder, &lookup, edx, ecx, eax, ebx, edi,
                          name, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_interceptor(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadArrayLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_array_length(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadArrayLength(masm(), edx, eax, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_array_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadStringLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadStringLength(masm(), edx, eax, ebx, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadFunctionPrototype(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1);

  // Check that the name has not changed.
  __ cmp(ecx, Immediate(name));
  __ j(not_equal, &miss);

  GenerateLoadFunctionPrototype(masm(), edx, eax, ebx, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  ElementsKind elements_kind = receiver_map->elements_kind();
  Handle<Code> stub = KeyedLoadElementStub(elements_kind).GetCode();

  __ DispatchMap(edx, receiver_map, stub, DO_SMI_CHECK);

  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, factory()->empty_string());
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadPolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_ics) {
  // ----------- S t a t e -------------
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
    __ cmp(map_reg, receiver_maps->at(current));
    __ j(equal, handler_ics->at(current));
  }

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
Handle<Code> ConstructStubCompiler::CompileConstructStub(
    Handle<JSFunction> function) {
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
  __ AllocateInNewSpace(ecx, edx, ecx, no_reg,
                        &generic_stub_call, NO_ALLOCATION_FLAGS);

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
  Handle<SharedFunctionInfo> shared(function->shared());
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
  __ or_(eax, Immediate(kHeapObjectTag));

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
  Handle<Code> code = isolate()->builtins()->JSConstructStubGeneric();
  __ jmp(code, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.
  __ JumpIfNotSmi(ecx, &miss_force_generic);
  __ mov(ebx, ecx);
  __ SmiUntag(ebx);
  __ mov(eax, FieldOperand(edx, JSObject::kElementsOffset));

  // Push receiver on the stack to free up a register for the dictionary
  // probing.
  __ push(edx);
  __ LoadFromNumberDictionary(&slow, eax, ecx, ebx, edx, edi, eax);
  // Pop receiver before returning.
  __ pop(edx);
  __ ret(0);

  __ bind(&slow);
  __ pop(edx);

  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(slow_ic, RelocInfo::CODE_TARGET);

  __ bind(&miss_force_generic);
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> miss_force_generic_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ jmp(miss_force_generic_ic, RelocInfo::CODE_TARGET);
}


static void GenerateSmiKeyCheck(MacroAssembler* masm,
                                Register key,
                                Register scratch,
                                XMMRegister xmm_scratch0,
                                XMMRegister xmm_scratch1,
                                Label* fail) {
  // Check that key is a smi and if SSE2 is available a heap number
  // containing a smi and branch if the check fails.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    Label key_ok;
    __ JumpIfSmi(key, &key_ok);
    __ cmp(FieldOperand(key, HeapObject::kMapOffset),
           Immediate(Handle<Map>(masm->isolate()->heap()->heap_number_map())));
    __ j(not_equal, fail);
    __ movdbl(xmm_scratch0, FieldOperand(key, HeapNumber::kValueOffset));
    __ cvttsd2si(scratch, Operand(xmm_scratch0));
    __ cvtsi2sd(xmm_scratch1, scratch);
    __ ucomisd(xmm_scratch1, xmm_scratch0);
    __ j(not_equal, fail);
    __ j(parity_even, fail);  // NaN.
    // Check if the key fits in the smi range.
    __ cmp(scratch, 0xc0000000);
    __ j(sign, fail);
    __ SmiTag(scratch);
    __ mov(key, scratch);
    __ bind(&key_ok);
  } else {
    __ JumpIfNotSmi(key, fail);
  }
}


void KeyedLoadStubCompiler::GenerateLoadExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, failed_allocation, slow;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, eax, xmm0, xmm1, &miss_force_generic);

  // Check that the index is in range.
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(ecx, FieldOperand(ebx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &miss_force_generic);
  __ mov(ebx, FieldOperand(ebx, ExternalArray::kExternalPointerOffset));
  // ebx: base pointer of external storage
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
      __ SmiUntag(ecx);  // Untag the index.
      __ movsx_b(eax, Operand(ebx, ecx, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      __ SmiUntag(ecx);  // Untag the index.
      __ movzx_b(eax, Operand(ebx, ecx, times_1, 0));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
      __ movsx_w(eax, Operand(ebx, ecx, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ movzx_w(eax, Operand(ebx, ecx, times_1, 0));
      break;
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
      __ mov(eax, Operand(ebx, ecx, times_2, 0));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      __ fld_s(Operand(ebx, ecx, times_2, 0));
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ fld_d(Operand(ebx, ecx, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }

  // For integer array types:
  // eax: value
  // For floating-point array type:
  // FP(0): value

  if (elements_kind == EXTERNAL_INT_ELEMENTS ||
      elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    if (elements_kind == EXTERNAL_INT_ELEMENTS) {
      __ cmp(eax, 0xc0000000);
      __ j(sign, &box_int);
    } else {
      ASSERT_EQ(EXTERNAL_UNSIGNED_INT_ELEMENTS, elements_kind);
      // The test is different for unsigned int values. Since we need
      // the value to be in the range of a positive smi, we can't
      // handle either of the top two bits being set in the value.
      __ test(eax, Immediate(0xc0000000));
      __ j(not_zero, &box_int);
    }

    __ SmiTag(eax);
    __ ret(0);

    __ bind(&box_int);

    // Allocate a HeapNumber for the int and perform int-to-double
    // conversion.
    if (elements_kind == EXTERNAL_INT_ELEMENTS) {
      __ push(eax);
      __ fild_s(Operand(esp, 0));
      __ pop(eax);
    } else {
      ASSERT_EQ(EXTERNAL_UNSIGNED_INT_ELEMENTS, elements_kind);
      // Need to zero-extend the value.
      // There's no fild variant for unsigned values, so zero-extend
      // to a 64-bit int manually.
      __ push(Immediate(0));
      __ push(eax);
      __ fild_d(Operand(esp, 0));
      __ pop(eax);
      __ pop(eax);
    }
    // FP(0): value
    __ AllocateHeapNumber(eax, ebx, edi, &failed_allocation);
    // Set the value.
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
             elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    __ AllocateHeapNumber(eax, ebx, edi, &failed_allocation);
    // Set the value.
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else {
    __ SmiTag(eax);
    __ ret(0);
  }

  // If we fail allocation of the HeapNumber, we still have a value on
  // top of the FPU stack. Remove it.
  __ bind(&failed_allocation);
  __ fstp(0);
  // Fall through to slow case.

  // Slow case: Jump to runtime.
  __ bind(&slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_external_array_slow(), 1);

  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Handle<Code> ic = masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // ----------- S t a t e -------------
  //  -- ecx    : key
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
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, slow, check_heap_number;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, ebx, xmm0, xmm1, &miss_force_generic);

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
        if ((elements_kind == EXTERNAL_INT_ELEMENTS ||
             elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) &&
            CpuFeatures::IsSupported(SSE3)) {
          CpuFeatures::Scope scope(SSE3);
          // fisttp stores values as signed integers. To represent the
          // entire range of int and unsigned int arrays, store as a
          // 64-bit int and discard the high 32 bits.
          __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
          __ sub(esp, Immediate(2 * kPointerSize));
          __ fisttp_d(Operand(esp, 0));

          // If conversion failed (NaN, infinity, or a number outside
          // signed int64 range), the result is 0x8000000000000000, and
          // we must handle this case in the runtime.
          Label ok;
          __ cmp(Operand(esp, kPointerSize), Immediate(0x80000000u));
          __ j(not_equal, &ok);
          __ cmp(Operand(esp, 0), Immediate(0));
          __ j(not_equal, &ok);
          __ add(esp, Immediate(2 * kPointerSize));  // Restore the stack.
          __ jmp(&slow);

          __ bind(&ok);
          __ pop(ebx);
          __ add(esp, Immediate(kPointerSize));
          __ mov(Operand(edi, ecx, times_2, 0), ebx);
        } else {
          ASSERT(CpuFeatures::IsSupported(SSE2));
          CpuFeatures::Scope scope(SSE2);
          __ cvttsd2si(ebx, FieldOperand(eax, HeapNumber::kValueOffset));
          __ cmp(ebx, 0x80000000u);
          __ j(equal, &slow);
          // ebx: untagged integer value
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
            case EXTERNAL_INT_ELEMENTS:
            case EXTERNAL_UNSIGNED_INT_ELEMENTS:
              __ mov(Operand(edi, ecx, times_2, 0), ebx);
              break;
            default:
              UNREACHABLE();
              break;
          }
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
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, eax, xmm0, xmm1, &miss_force_generic);

  // Get the elements array.
  __ mov(eax, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(eax);

  // Check that the key is within bounds.
  __ cmp(ecx, FieldOperand(eax, FixedArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Load the result and make sure it's not the hole.
  __ mov(ebx, Operand(eax, ecx, times_2,
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
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, slow_allocate_heapnumber;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, eax, xmm0, xmm1, &miss_force_generic);

  // Get the elements array.
  __ mov(eax, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(eax);

  // Check that the key is within bounds.
  __ cmp(ecx, FieldOperand(eax, FixedDoubleArray::kLengthOffset));
  __ j(above_equal, &miss_force_generic);

  // Check for the hole
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(eax, ecx, times_4, offset), Immediate(kHoleNanUpper32));
  __ j(equal, &miss_force_generic);

  // Always allocate a heap number for the result.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(xmm0, FieldOperand(eax, ecx, times_4,
                                 FixedDoubleArray::kHeaderSize));
  } else {
    __ fld_d(FieldOperand(eax, ecx, times_4, FixedDoubleArray::kHeaderSize));
  }
  __ AllocateHeapNumber(eax, ebx, edi, &slow_allocate_heapnumber);
  // Set the value.
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatures::Scope use_sse2(SSE2);
    __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
  } else {
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
  }
  __ ret(0);

  __ bind(&slow_allocate_heapnumber);
  // A value was pushed on the floating point stack before the allocation, if
  // the allocation fails it needs to be removed.
  if (!CpuFeatures::IsSupported(SSE2)) {
    __ fstp(0);
  }
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
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, grow, slow, transition_elements_kind;
  Label check_capacity, prepare_slow, finish_store, commit_backing_store;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, ebx, xmm0, xmm1, &miss_force_generic);

  if (IsFastSmiElementsKind(elements_kind)) {
    __ JumpIfNotSmi(eax, &transition_elements_kind);
  }

  // Get the elements array and make sure it is a fast element array, not 'cow'.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  if (is_js_array) {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // smis.
    if (grow_mode == ALLOW_JSARRAY_GROWTH) {
      __ j(above_equal, &grow);
    } else {
      __ j(above_equal, &miss_force_generic);
    }
  } else {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edi, FixedArray::kLengthOffset));  // smis.
    __ j(above_equal, &miss_force_generic);
  }

  __ cmp(FieldOperand(edi, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_array_map()));
  __ j(not_equal, &miss_force_generic);

  __ bind(&finish_store);
  if (IsFastSmiElementsKind(elements_kind)) {
    // ecx is a smi, use times_half_pointer_size instead of
    // times_pointer_size
    __ mov(FieldOperand(edi,
                        ecx,
                        times_half_pointer_size,
                        FixedArray::kHeaderSize), eax);
  } else {
    ASSERT(IsFastObjectElementsKind(elements_kind));
    // Do the store and update the write barrier.
    // ecx is a smi, use times_half_pointer_size instead of
    // times_pointer_size
    __ lea(ecx, FieldOperand(edi,
                             ecx,
                             times_half_pointer_size,
                             FixedArray::kHeaderSize));
    __ mov(Operand(ecx, 0), eax);
    // Make sure to preserve the value in register eax.
    __ mov(ebx, eax);
    __ RecordWrite(edi, ecx, ebx, kDontSaveFPRegs);
  }

  // Done.
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);

  // Handle transition to other elements kinds without using the generic stub.
  __ bind(&transition_elements_kind);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Handle transition requiring the array to grow.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags are already set by previous
    // compare.
    __ j(not_equal, &miss_force_generic);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
    __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
    __ j(not_equal, &check_capacity);

    int size = FixedArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, edi, ebx, ecx, &prepare_slow, TAG_OBJECT);
    // Restore the key, which is known to be the array length.

    // eax: value
    // ecx: key
    // edx: receiver
    // edi: elements
    // Make sure that the backing store can hold additional elements.
    __ mov(FieldOperand(edi, JSObject::kMapOffset),
           Immediate(masm->isolate()->factory()->fixed_array_map()));
    __ mov(FieldOperand(edi, FixedArray::kLengthOffset),
           Immediate(Smi::FromInt(JSArray::kPreallocatedArrayElements)));
    __ mov(ebx, Immediate(masm->isolate()->factory()->the_hole_value()));
    for (int i = 1; i < JSArray::kPreallocatedArrayElements; ++i) {
      __ mov(FieldOperand(edi, FixedArray::SizeFor(i)), ebx);
    }

    // Store the element at index zero.
    __ mov(FieldOperand(edi, FixedArray::SizeFor(0)), eax);

    // Install the new backing store in the JSArray.
    __ mov(FieldOperand(edx, JSObject::kElementsOffset), edi);
    __ RecordWriteField(edx, JSObject::kElementsOffset, edi, ebx,
                        kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ mov(FieldOperand(edx, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
    __ ret(0);

    __ bind(&check_capacity);
    __ cmp(FieldOperand(edi, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->fixed_cow_array_map()));
    __ j(equal, &miss_force_generic);

    // eax: value
    // ecx: key
    // edx: receiver
    // edi: elements
    // Make sure that the backing store can hold additional elements.
    __ cmp(ecx, FieldOperand(edi, FixedArray::kLengthOffset));
    __ j(above_equal, &slow);

    // Grow the array and finish the store.
    __ add(FieldOperand(edx, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
    __ jmp(&finish_store);

    __ bind(&prepare_slow);
    // Restore the key, which is known to be the array length.
    __ mov(ecx, Immediate(0));

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
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, grow, slow;
  Label check_capacity, prepare_slow, finish_store, commit_backing_store;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, ecx, ebx, xmm0, xmm1, &miss_force_generic);

  // Get the elements array.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ AssertFastElements(edi);

  if (is_js_array) {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // smis.
    if (grow_mode == ALLOW_JSARRAY_GROWTH) {
      __ j(above_equal, &grow);
    } else {
      __ j(above_equal, &miss_force_generic);
    }
  } else {
    // Check that the key is within bounds.
    __ cmp(ecx, FieldOperand(edi, FixedArray::kLengthOffset));  // smis.
    __ j(above_equal, &miss_force_generic);
  }

  __ bind(&finish_store);
  __ StoreNumberToDoubleElements(eax, edi, ecx, edx, xmm0,
                                 &transition_elements_kind, true);
  __ ret(0);

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic_force_generic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ jmp(ic_force_generic, RelocInfo::CODE_TARGET);

  // Handle transition to other elements kinds without using the generic stub.
  __ bind(&transition_elements_kind);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ jmp(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Handle transition requiring the array to grow.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags are already set by previous
    // compare.
    __ j(not_equal, &miss_force_generic);

    // Transition on values that can't be stored in a FixedDoubleArray.
    Label value_is_smi;
    __ JumpIfSmi(eax, &value_is_smi);
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           Immediate(Handle<Map>(masm->isolate()->heap()->heap_number_map())));
    __ j(not_equal, &transition_elements_kind);
    __ bind(&value_is_smi);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
    __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
    __ j(not_equal, &check_capacity);

    int size = FixedDoubleArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, edi, ebx, ecx, &prepare_slow, TAG_OBJECT);

    // Restore the key, which is known to be the array length.
    __ mov(ecx, Immediate(0));

    // eax: value
    // ecx: key
    // edx: receiver
    // edi: elements
    // Initialize the new FixedDoubleArray. Leave elements unitialized for
    // efficiency, they are guaranteed to be initialized before use.
    __ mov(FieldOperand(edi, JSObject::kMapOffset),
           Immediate(masm->isolate()->factory()->fixed_double_array_map()));
    __ mov(FieldOperand(edi, FixedDoubleArray::kLengthOffset),
           Immediate(Smi::FromInt(JSArray::kPreallocatedArrayElements)));

    // Install the new backing store in the JSArray.
    __ mov(FieldOperand(edx, JSObject::kElementsOffset), edi);
    __ RecordWriteField(edx, JSObject::kElementsOffset, edi, ebx,
                        kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ add(FieldOperand(edx, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
    __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
    __ jmp(&finish_store);

    __ bind(&check_capacity);
    // eax: value
    // ecx: key
    // edx: receiver
    // edi: elements
    // Make sure that the backing store can hold additional elements.
    __ cmp(ecx, FieldOperand(edi, FixedDoubleArray::kLengthOffset));
    __ j(above_equal, &slow);

    // Grow the array and finish the store.
    __ add(FieldOperand(edx, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
    __ jmp(&finish_store);

    __ bind(&prepare_slow);
    // Restore the key, which is known to be the array length.
    __ mov(ecx, Immediate(0));

    __ bind(&slow);
    Handle<Code> ic_slow = masm->isolate()->builtins()->KeyedStoreIC_Slow();
    __ jmp(ic_slow, RelocInfo::CODE_TARGET);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
