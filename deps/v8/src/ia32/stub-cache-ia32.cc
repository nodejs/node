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

#if defined(V8_TARGET_ARCH_IA32)

#include "ic-inl.h"
#include "codegen-inl.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(MacroAssembler* masm,
                       Code::Flags flags,
                       StubCache::Table table,
                       Register name,
                       Register offset,
                       Register extra) {
  ExternalReference key_offset(SCTableReference::keyReference(table));
  ExternalReference value_offset(SCTableReference::valueReference(table));

  Label miss;

  if (extra.is_valid()) {
    // Get the code entry from the cache.
    __ mov(extra, Operand::StaticArray(offset, times_2, value_offset));

    // Check that the key in the entry matches the name.
    __ cmp(name, Operand::StaticArray(offset, times_2, key_offset));
    __ j(not_equal, &miss, not_taken);

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
    __ j(not_equal, &miss, not_taken);

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
static void GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                             Label* miss_label,
                                             Register receiver,
                                             String* name,
                                             Register r0,
                                             Register r1) {
  ASSERT(name->IsSymbol());
  __ IncrementCounter(&Counters::negative_lookups, 1);
  __ IncrementCounter(&Counters::negative_lookups_miss, 1);

  Label done;
  __ mov(r0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ test_b(FieldOperand(r0, Map::kBitFieldOffset),
            kInterceptorOrAccessCheckNeededMask);
  __ j(not_zero, miss_label, not_taken);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(r0, FIRST_JS_OBJECT_TYPE);
  __ j(below, miss_label, not_taken);

  // Load properties array.
  Register properties = r0;
  __ mov(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ cmp(FieldOperand(properties, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(not_equal, miss_label);

  // Compute the capacity mask.
  const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;

  // Generate an unrolled loop that performs a few probes before
  // giving up.
  static const int kProbes = 4;
  const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = 0; i < kProbes; i++) {
    // r0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r1;
    // Capacity is smi 2^n.
    __ mov(index, FieldOperand(properties, kCapacityOffset));
    __ dec(index);
    __ and_(Operand(index),
            Immediate(Smi::FromInt(name->Hash() +
                                   StringDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.

    Register entity_name = r1;
    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    __ mov(entity_name, Operand(properties, index, times_half_pointer_size,
                                kElementsStartOffset - kHeapObjectTag));
    __ cmp(entity_name, Factory::undefined_value());
    if (i != kProbes - 1) {
      __ j(equal, &done, taken);

      // Stop if found the property.
      __ cmp(entity_name, Handle<String>(name));
      __ j(equal, miss_label, not_taken);

      // Check if the entry name is not a symbol.
      __ mov(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
      __ test_b(FieldOperand(entity_name, Map::kInstanceTypeOffset),
                kIsSymbolMask);
      __ j(zero, miss_label, not_taken);
    } else {
      // Give up probing if still not found the undefined value.
      __ j(not_equal, miss_label, not_taken);
    }
  }

  __ bind(&done);
  __ DecrementCounter(&Counters::negative_lookups_miss, 1);
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra) {
  Label miss;

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

  // Check that the receiver isn't a smi.
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Get the map of the receiver and compute the hash.
  __ mov(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ add(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, flags);
  __ and_(scratch, (kPrimaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the primary table.
  ProbeTable(masm, flags, kPrimary, name, scratch, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ add(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, flags);
  __ and_(scratch, (kPrimaryTableSize - 1) << kHeapObjectTagSize);
  __ sub(scratch, Operand(name));
  __ add(Operand(scratch), Immediate(flags));
  __ and_(scratch, (kSecondaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the secondary table.
  ProbeTable(masm, flags, kSecondary, name, scratch, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ mov(prototype, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  __ mov(prototype,
         FieldOperand(prototype, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  __ mov(prototype, Operand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ mov(prototype,
         FieldOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ mov(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype) {
  // Get the global function with the given index.
  JSFunction* function = JSFunction::cast(Top::global_context()->get(index));
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss_label, not_taken);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, miss_label, not_taken);

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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, smi, not_taken);

  // Check that the object is a string.
  __ mov(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  ASSERT(kNotStringTag != 0);
  __ test(scratch, Immediate(kNotStringTag));
  __ j(not_zero, non_string_object, not_taken);
}


void StubCompiler::GenerateLoadStringLength(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss) {
  Label check_wrapper;

  // Check if the object is a string leaving the instance type in the
  // scratch register.
  GenerateStringCheck(masm, receiver, scratch1, miss, &check_wrapper);

  // Load length from the string and convert to a smi.
  __ mov(eax, FieldOperand(receiver, String::kLengthOffset));
  __ ret(0);

  // Check if the object is a JSValue wrapper.
  __ bind(&check_wrapper);
  __ cmp(scratch1, JS_VALUE_TYPE);
  __ j(not_equal, miss, not_taken);

  // Check if the wrapped value is a string and load the length
  // directly if it is.
  __ mov(scratch2, FieldOperand(receiver, JSValue::kValueOffset));
  GenerateStringCheck(masm, scratch2, scratch1, miss, miss);
  __ mov(eax, FieldOperand(scratch2, String::kLengthOffset));
  __ ret(0);
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
  ASSERT(!Heap::InNewSpace(interceptor));
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
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly)),
        5);
}


// Reserves space for the extra arguments to FastHandleApiCall in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : last argument in the internal frame of the caller
  // -----------------------------------
  __ pop(scratch);
  __ push(Immediate(Smi::FromInt(0)));
  __ push(Immediate(Smi::FromInt(0)));
  __ push(Immediate(Smi::FromInt(0)));
  __ push(Immediate(Smi::FromInt(0)));
  __ push(scratch);
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- esp[0]  : return address
  //  -- esp[4]  : last fast api call extra argument
  //  -- ...
  //  -- esp[16] : first fast api call extra argument
  //  -- esp[20] : last argument in the internal frame
  // -----------------------------------
  __ pop(scratch);
  __ add(Operand(esp), Immediate(kPointerSize * 4));
  __ push(scratch);
}


// Generates call to FastHandleApiCall builtin.
static void GenerateFastApiCall(MacroAssembler* masm,
                                const CallOptimization& optimization,
                                int argc) {
  // ----------- S t a t e -------------
  //  -- esp[0]              : return address
  //  -- esp[4]              : object passing the type check
  //                           (last fast api call extra argument,
  //                            set by CheckPrototypes)
  //  -- esp[8]              : api call data
  //  -- esp[12]             : api callback
  //  -- esp[16]             : api function
  //                           (first fast api call extra argument)
  //  -- esp[20]             : last argument
  //  -- ...
  //  -- esp[(argc + 5) * 4] : first argument
  //  -- esp[(argc + 6) * 4] : receiver
  // -----------------------------------

  // Get the function and setup the context.
  JSFunction* function = optimization.constant_function();
  __ mov(edi, Immediate(Handle<JSFunction>(function)));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Pass the additional arguments FastHandleApiCall expects.
  __ mov(Operand(esp, 4 * kPointerSize), edi);
  bool info_loaded = false;
  Object* callback = optimization.api_call_info()->callback();
  if (Heap::InNewSpace(callback)) {
    info_loaded = true;
    __ mov(ecx, Handle<CallHandlerInfo>(optimization.api_call_info()));
    __ mov(ebx, FieldOperand(ecx, CallHandlerInfo::kCallbackOffset));
    __ mov(Operand(esp, 3 * kPointerSize), ebx);
  } else {
    __ mov(Operand(esp, 3 * kPointerSize), Immediate(Handle<Object>(callback)));
  }
  Object* call_data = optimization.api_call_info()->data();
  if (Heap::InNewSpace(call_data)) {
    if (!info_loaded) {
      __ mov(ecx, Handle<CallHandlerInfo>(optimization.api_call_info()));
    }
    __ mov(ebx, FieldOperand(ecx, CallHandlerInfo::kDataOffset));
    __ mov(Operand(esp, 2 * kPointerSize), ebx);
  } else {
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(Handle<Object>(call_data)));
  }

  // Set the number of arguments.
  __ mov(eax, Immediate(argc + 4));

  // Jump to the fast api call builtin (tail call).
  Handle<Code> code = Handle<Code>(
      Builtins::builtin(Builtins::FastHandleApiCall));
  ParameterCount expected(0);
  __ InvokeCode(code, expected, expected,
                RelocInfo::CODE_TARGET, JUMP_FUNCTION);
}


class CallInterceptorCompiler BASE_EMBEDDED {
 public:
  CallInterceptorCompiler(StubCompiler* stub_compiler,
                          const ParameterCount& arguments,
                          Register name)
      : stub_compiler_(stub_compiler),
        arguments_(arguments),
        name_(name) {}

  void Compile(MacroAssembler* masm,
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
    __ test(receiver, Immediate(kSmiTagMask));
    __ j(zero, miss, not_taken);

    CallOptimization optimization(lookup);

    if (optimization.is_constant_call()) {
      CompileCacheable(masm,
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
    }
  }

 private:
  void CompileCacheable(MacroAssembler* masm,
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

    __ IncrementCounter(&Counters::call_const_interceptor, 1);

    if (can_do_fast_api_call) {
      __ IncrementCounter(&Counters::call_const_interceptor_fast_api, 1);
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
      GenerateFastApiCall(masm, optimization, arguments_.immediate());
    } else {
      __ InvokeFunction(optimization.constant_function(), arguments_,
                        JUMP_FUNCTION);
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
          ExternalReference(
              IC_Utility(IC::kLoadPropertyWithInterceptorForCall)),
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

    __ cmp(eax, Factory::no_interceptor_result_sentinel());
    __ j(not_equal, interceptor_succeeded);
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
};


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  ASSERT(kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC);
  Code* code = NULL;
  if (kind == Code::LOAD_IC) {
    code = Builtins::builtin(Builtins::LoadIC_Miss);
  } else {
    code = Builtins::builtin(Builtins::KeyedLoadIC_Miss);
  }

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
  __ test(receiver_reg, Immediate(kSmiTagMask));
  __ j(zero, miss_label, not_taken);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(receiver_reg, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, miss_label, not_taken);

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
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage)), 3, 1);
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
static Object* GenerateCheckPropertyCell(MacroAssembler* masm,
                                         GlobalObject* global,
                                         String* name,
                                         Register scratch,
                                         Label* miss) {
  Object* probe = global->EnsurePropertyCell(name);
  if (probe->IsFailure()) return probe;
  JSGlobalPropertyCell* cell = JSGlobalPropertyCell::cast(probe);
  ASSERT(cell->value()->IsTheHole());
  __ mov(scratch, Immediate(Handle<Object>(cell)));
  __ cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
         Immediate(Factory::the_hole_value()));
  __ j(not_equal, miss, not_taken);
  return cell;
}


// Calls GenerateCheckPropertyCell for each global object in the prototype chain
// from object to (but not including) holder.
static Object* GenerateCheckPropertyCells(MacroAssembler* masm,
                                          JSObject* object,
                                          JSObject* holder,
                                          String* name,
                                          Register scratch,
                                          Label* miss) {
  JSObject* current = object;
  while (current != holder) {
    if (current->IsGlobalObject()) {
      Object* cell = GenerateCheckPropertyCell(masm,
                                               GlobalObject::cast(current),
                                               name,
                                               scratch,
                                               miss);
      if (cell->IsFailure()) {
        return cell;
      }
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
        Object* lookup_result = Heap::LookupSymbol(name);
        if (lookup_result->IsFailure()) {
          set_failure(Failure::cast(lookup_result));
          return reg;
        } else {
          name = String::cast(lookup_result);
        }
      }
      ASSERT(current->property_dictionary()->FindEntry(name) ==
             StringDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(),
                                       miss,
                                       reg,
                                       name,
                                       scratch1,
                                       scratch2);
      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // from now the object is in holder_reg
      __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else if (Heap::InNewSpace(prototype)) {
      // Get the map of the current object.
      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      __ cmp(Operand(scratch1), Immediate(Handle<Map>(current->map())));
      // Branch on the result of the map check.
      __ j(not_equal, miss, not_taken);
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
      __ j(not_equal, miss, not_taken);
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
  LOG(IntEvent("check-maps-depth", depth + 1));

  // Check the holder map.
  __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
         Immediate(Handle<Map>(holder->map())));
  __ j(not_equal, miss, not_taken);

  // Perform security check for access to the global object.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());
  if (holder->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  };

  // If we've skipped any global objects, it's not enough to verify
  // that their maps haven't changed.  We also need to check that the
  // property cell for the property is still empty.
  Object* result = GenerateCheckPropertyCells(masm(),
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check the prototype chain.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, scratch3, name, miss);

  // Get the value from the properties.
  GenerateFastPropertyLoad(masm(), eax, reg, holder, index);
  __ ret(0);
}


bool StubCompiler::GenerateLoadCallback(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        AccessorInfo* callback,
                                        String* name,
                                        Label* miss,
                                        Failure** failure) {
  // Check that the receiver isn't a smi.
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder, scratch1,
                      scratch2, scratch3, name, miss);

  Handle<AccessorInfo> callback_handle(callback);

  __ EnterInternalFrame();
  __ PushHandleScope(scratch2);
  // Push the stack address where the list of arguments ends.
  __ mov(scratch2, esp);
  __ sub(Operand(scratch2), Immediate(2 * kPointerSize));
  __ push(scratch2);
  __ push(receiver);  // receiver
  __ push(reg);  // holder
  // Push data from AccessorInfo.
  if (Heap::InNewSpace(callback_handle->data())) {
    __ mov(scratch2, Immediate(callback_handle));
    __ push(FieldOperand(scratch2, AccessorInfo::kDataOffset));
  } else {
    __ push(Immediate(Handle<Object>(callback_handle->data())));
  }
  __ push(name_reg);  // name
  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const AccessorInfo& to the C++ callback.
  __ mov(eax, esp);
  __ add(Operand(eax), Immediate(4 * kPointerSize));
  __ mov(ebx, esp);

  // Do call through the api.
  ASSERT_EQ(5, ApiGetterEntryStub::kStackSpace);
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ApiGetterEntryStub stub(callback_handle, &fun);
  // Emitting a stub call may try to allocate (if the code is not
  // already generated).  Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
  Object* result = masm()->TryCallStub(&stub);
  if (result->IsFailure()) {
    *failure = Failure::cast(result);
    return false;
  }

  // We need to avoid using eax since that now holds the result.
  Register tmp = scratch2.is(eax) ? reg : scratch2;
  // Emitting PopHandleScope may try to allocate.  Do not allow the
  // assembler to perform a garbage collection but instead return a
  // failure object.
  result = masm()->TryPopHandleScope(eax, tmp);
  if (result->IsFailure()) {
    *failure = Failure::cast(result);
    return false;
  }
  __ LeaveInternalFrame();

  __ ret(0);
  return true;
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

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
    __ cmp(eax, Factory::no_interceptor_result_sentinel());
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
      __ push(holder_reg);
      __ push(FieldOperand(holder_reg, AccessorInfo::kDataOffset));
      __ push(name_reg);
      __ push(scratch2);  // restore return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty));
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

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad));
    __ TailCallExternalReference(ref, 5, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(String* name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ cmp(Operand(ecx), Immediate(Handle<String>(name)));
    __ j(not_equal, miss, not_taken);
  }
}


Object* CallStubCompiler::GenerateMissBranch() {
  Object* obj = StubCache::ComputeCallMiss(arguments().immediate(), kind_);
  if (obj->IsFailure()) return obj;
  __ jmp(Handle<Code>(Code::cast(obj)), RelocInfo::CODE_TARGET);
  return obj;
}


Object* CallStubCompiler::CompileCallField(JSObject* object,
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
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, edx, holder, ebx, eax, edi,
                                 name, &miss);

  GenerateFastPropertyLoad(masm(), edi, reg, holder, index);

  // Check that the function really is a function.
  __ test(edi, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &miss, not_taken);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* CallStubCompiler::CompileArrayPushCall(Object* object,
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
  ASSERT(check == RECEIVER_MAP_CHECK);

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray()) {
    return Heap::undefined_value();
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss);

  CheckPrototypes(JSObject::cast(object), edx,
                  holder, ebx,
                  eax, edi, name, &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    // Get the elements array of the object.
    __ mov(ebx, FieldOperand(edx, JSArray::kElementsOffset));

    // Check that the elements are in fast mode (not dictionary).
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(Factory::fixed_array_map()));
    __ j(not_equal, &miss);

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label call_builtin, exit, with_write_barrier, attempt_to_grow_elements;

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
      __ test(ecx, Immediate(kSmiTagMask));
      __ j(not_zero, &with_write_barrier);

      __ bind(&exit);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ InNewSpace(ebx, ecx, equal, &exit);

      __ RecordWriteHelper(ebx, edx, ecx);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&attempt_to_grow_elements);
      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address();
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address();

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
               Immediate(Factory::the_hole_value()));
      }

      // Restore receiver to edx as finish sequence assumes it's here.
      __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ add(FieldOperand(ebx, FixedArray::kLengthOffset),
             Immediate(Smi::FromInt(kAllocationDelta)));
      __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

      // Elements are in new space, so write barrier is not required.
      __ ret((argc + 1) * kPointerSize);

      __ bind(&call_builtin);
    }

    __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPush),
                                 argc + 1,
                                 1);
  }

  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileArrayPopCall(Object* object,
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
  ASSERT(check == RECEIVER_MAP_CHECK);

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray()) {
    return Heap::undefined_value();
  }

  Label miss, return_undefined, call_builtin;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss);
  CheckPrototypes(JSObject::cast(object), edx,
                  holder, ebx,
                  eax, edi, name, &miss);

  // Get the elements array of the object.
  __ mov(ebx, FieldOperand(edx, JSArray::kElementsOffset));

  // Check that the elements are in fast mode (not dictionary).
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(Factory::fixed_array_map()));
  __ j(not_equal, &miss);

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
  __ cmp(Operand(eax), Immediate(Factory::the_hole_value()));
  __ j(equal, &call_builtin);

  // Set the array's length.
  __ mov(FieldOperand(edx, JSArray::kLengthOffset), ecx);

  // Fill with the hole.
  __ mov(FieldOperand(ebx,
                      ecx, times_half_pointer_size,
                      FixedArray::kHeaderSize),
         Immediate(Factory::the_hole_value()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&return_undefined);
  __ mov(eax, Immediate(Factory::undefined_value()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&call_builtin);
  __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPop),
                               argc + 1,
                               1);

  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileStringCharCodeAtCall(Object* object,
                                                      JSObject* holder,
                                                      JSFunction* function,
                                                      String* name,
                                                      CheckType check) {
  // ----------- S t a t e -------------
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString()) return Heap::undefined_value();

  const int argc = arguments().immediate();

  Label miss;
  Label index_out_of_range;
  GenerateNameCheck(name, &miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            eax);
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
    __ Set(index, Immediate(Factory::undefined_value()));
  }

  StringCharCodeAtGenerator char_code_at_generator(receiver,
                                                   index,
                                                   scratch,
                                                   result,
                                                   &miss,  // When not a string.
                                                   &miss,  // When not a number.
                                                   &index_out_of_range,
                                                   STRING_INDEX_IS_NUMBER);
  char_code_at_generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  ICRuntimeCallHelper call_helper;
  char_code_at_generator.GenerateSlow(masm(), call_helper);

  __ bind(&index_out_of_range);
  __ Set(eax, Immediate(Factory::nan_value()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileStringCharAtCall(Object* object,
                                                  JSObject* holder,
                                                  JSFunction* function,
                                                  String* name,
                                                  CheckType check) {
  // ----------- S t a t e -------------
  //  -- ecx                 : function name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString()) return Heap::undefined_value();

  const int argc = arguments().immediate();

  Label miss;
  Label index_out_of_range;

  GenerateNameCheck(name, &miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            eax);
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
    __ Set(index, Immediate(Factory::undefined_value()));
  }

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch1,
                                          scratch2,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &index_out_of_range,
                                          STRING_INDEX_IS_NUMBER);
  char_at_generator.GenerateFast(masm());
  __ ret((argc + 1) * kPointerSize);

  ICRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm(), call_helper);

  __ bind(&index_out_of_range);
  __ Set(eax, Immediate(Factory::empty_string()));
  __ ret((argc + 1) * kPointerSize);

  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileCallConstant(Object* object,
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

  SharedFunctionInfo* function_info = function->shared();
  if (function_info->HasCustomCallGenerator()) {
    const int id = function_info->custom_call_generator_id();
    Object* result =
        CompileCustomCall(id, object, holder, function, name, check);
    // undefined means bail out to regular compiler.
    if (!result->IsUndefined()) {
      return result;
    }
  }

  Label miss_in_smi_check;

  GenerateNameCheck(name, &miss_in_smi_check);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ test(edx, Immediate(kSmiTagMask));
    __ j(zero, &miss_in_smi_check, not_taken);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  CallOptimization optimization(function);
  int depth = kInvalidProtoDepth;
  Label miss;

  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(&Counters::call_const, 1);

      if (optimization.is_simple_api_call() && !object->IsGlobalObject()) {
        depth = optimization.GetPrototypeDepthOfExpectedType(
            JSObject::cast(object), holder);
      }

      if (depth != kInvalidProtoDepth) {
        __ IncrementCounter(&Counters::call_const_fast_api, 1);
        ReserveSpaceForFastApiCall(masm(), eax);
      }

      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, eax, edi, name, depth, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        ASSERT(depth == kInvalidProtoDepth);
        __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
        __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
      }
      break;

    case STRING_CHECK:
      if (!function->IsBuiltin()) {
        // Calling non-builtins with a value as receiver requires boxing.
        __ jmp(&miss);
      } else {
        // Check that the object is a string or a symbol.
        __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, eax);
        __ j(above_equal, &miss, not_taken);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;

    case NUMBER_CHECK: {
      if (!function->IsBuiltin()) {
        // Calling non-builtins with a value as receiver requires boxing.
        __ jmp(&miss);
      } else {
        Label fast;
        // Check that the object is a smi or a heap number.
        __ test(edx, Immediate(kSmiTagMask));
        __ j(zero, &fast, taken);
        __ CmpObjectType(edx, HEAP_NUMBER_TYPE, eax);
        __ j(not_equal, &miss, not_taken);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;
    }

    case BOOLEAN_CHECK: {
      if (!function->IsBuiltin()) {
        // Calling non-builtins with a value as receiver requires boxing.
        __ jmp(&miss);
      } else {
        Label fast;
        // Check that the object is a boolean.
        __ cmp(edx, Factory::true_value());
        __ j(equal, &fast, taken);
        __ cmp(edx, Factory::false_value());
        __ j(not_equal, &miss, not_taken);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, edi, name, &miss);
      }
      break;
    }

    default:
      UNREACHABLE();
  }

  if (depth != kInvalidProtoDepth) {
    GenerateFastApiCall(masm(), optimization, argc);
  } else {
    __ InvokeFunction(function, arguments(), JUMP_FUNCTION);
  }

  // Handle call cache miss.
  __ bind(&miss);
  if (depth != kInvalidProtoDepth) {
    FreeSpaceForFastApiCall(masm(), eax);
  }
  __ bind(&miss_in_smi_check);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileCallInterceptor(JSObject* object,
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

  CallInterceptorCompiler compiler(this, arguments(), ecx);
  compiler.Compile(masm(),
                   object,
                   holder,
                   name,
                   &lookup,
                   edx,
                   ebx,
                   edi,
                   eax,
                   &miss);

  // Restore receiver.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the function really is a function.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);
  __ CmpObjectType(eax, JS_FUNCTION_TYPE, ebx);
  __ j(not_equal, &miss, not_taken);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  __ mov(edi, eax);
  __ InvokeFunction(edi, arguments(), JUMP_FUNCTION);

  // Handle load cache miss.
  __ bind(&miss);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* CallStubCompiler::CompileCallGlobal(JSObject* object,
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
  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ test(edx, Immediate(kSmiTagMask));
    __ j(zero, &miss, not_taken);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, edx, holder, ebx, eax, edi, name, &miss);

  // Get the value from the cell.
  __ mov(edi, Immediate(Handle<JSGlobalPropertyCell>(cell)));
  __ mov(edi, FieldOperand(edi, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (Heap::InNewSpace(function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ test(edi, Immediate(kSmiTagMask));
    __ j(zero, &miss, not_taken);
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ebx);
    __ j(not_equal, &miss, not_taken);

    // Check the shared function info. Make sure it hasn't changed.
    __ cmp(FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset),
           Immediate(Handle<SharedFunctionInfo>(function->shared())));
    __ j(not_equal, &miss, not_taken);
  } else {
    __ cmp(Operand(edi), Immediate(Handle<JSFunction>(function)));
    __ j(not_equal, &miss, not_taken);
  }

  // Patch the receiver on the stack with the global proxy.
  if (object->IsGlobalObject()) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Setup the context (function already in edi).
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  __ IncrementCounter(&Counters::call_global_inline, 1);
  ASSERT(function->is_compiled());
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  __ InvokeCode(code, expected, arguments(),
                RelocInfo::CODE_TARGET, JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(&Counters::call_global_inline_miss, 1);
  Object* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* StoreStubCompiler::CompileStoreField(JSObject* object,
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
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


Object* StoreStubCompiler::CompileStoreCallback(JSObject* object,
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
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, &miss, not_taken);

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
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty));
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(receiver->map())));
  __ j(not_equal, &miss, not_taken);

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
  __ push(ebx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty));
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
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
  __ j(not_equal, &miss, not_taken);

  // Store the value in the cell.
  __ mov(ecx, Immediate(Handle<JSGlobalPropertyCell>(cell)));
  __ mov(FieldOperand(ecx, JSGlobalPropertyCell::kValueOffset), eax);

  // Return the value (register eax).
  __ IncrementCounter(&Counters::named_store_global_inline, 1);
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(&Counters::named_store_global_inline_miss, 1);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
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

  __ IncrementCounter(&Counters::keyed_store_field, 1);

  // Check that the name has not changed.
  __ cmp(Operand(ecx), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     edx, ecx, ebx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_store_field, 1);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


Object* LoadStubCompiler::CompileLoadNonexistent(String* name,
                                                 JSObject* object,
                                                 JSObject* last) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  // Check that the receiver isn't a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  ASSERT(last->IsGlobalObject() || last->HasFastProperties());

  // Check the maps of the full prototype chain. Also check that
  // global property cells up to (but not including) the last object
  // in the prototype chain are empty.
  CheckPrototypes(object, eax, last, ebx, edx, edi, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    Object* cell = GenerateCheckPropertyCell(masm(),
                                             GlobalObject::cast(last),
                                             name,
                                             edx,
                                             &miss);
    if (cell->IsFailure()) return cell;
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ mov(eax, Factory::undefined_value());
  __ ret(0);

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NONEXISTENT, Heap::empty_string());
}


Object* LoadStubCompiler::CompileLoadField(JSObject* object,
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


Object* LoadStubCompiler::CompileLoadCallback(String* name,
                                              JSObject* object,
                                              JSObject* holder,
                                              AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Failure* failure = Failure::InternalError();
  bool success = GenerateLoadCallback(object, holder, eax, ecx, ebx, edx, edi,
                                      callback, name, &miss, &failure);
  if (!success) return failure;

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* LoadStubCompiler::CompileLoadConstant(JSObject* object,
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


Object* LoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
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


Object* LoadStubCompiler::CompileLoadGlobal(JSObject* object,
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
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &miss, not_taken);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, eax, holder, ebx, edx, edi, name, &miss);

  // Get the value from the cell.
  __ mov(ebx, Immediate(Handle<JSGlobalPropertyCell>(cell)));
  __ mov(ebx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ cmp(ebx, Factory::the_hole_value());
    __ j(equal, &miss, not_taken);
  } else if (FLAG_debug_code) {
    __ cmp(ebx, Factory::the_hole_value());
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  __ IncrementCounter(&Counters::named_load_global_inline, 1);
  __ mov(eax, ebx);
  __ ret(0);

  __ bind(&miss);
  __ IncrementCounter(&Counters::named_load_global_inline_miss, 1);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int index) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_field, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadField(receiver, holder, edx, ebx, ecx, edi, index, name, &miss);

  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_field, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_callback, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  Failure* failure = Failure::InternalError();
  bool success = GenerateLoadCallback(receiver, holder, edx, eax, ebx, ecx, edi,
                                      callback, name, &miss, &failure);
  if (!success) return failure;

  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_callback, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   Object* value) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_constant_function, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadConstant(receiver, holder, edx, ebx, ecx, edi,
                       value, name, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_constant_function, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_interceptor, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

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
  __ DecrementCounter(&Counters::keyed_load_interceptor, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}




Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_array_length, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadArrayLength(masm(), edx, ecx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_array_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_string_length, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadStringLength(masm(), edx, ecx, ebx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_string_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_load_function_prototype, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadFunctionPrototype(masm(), edx, ecx, ebx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_function_prototype, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
Object* ConstructStubCompiler::CompileConstructStub(
    SharedFunctionInfo* shared) {
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
  __ cmp(ebx, Factory::undefined_value());
  __ j(not_equal, &generic_stub_call, not_taken);
#endif

  // Load the initial map and verify that it is in fact a map.
  __ mov(ebx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
  // Will both indicate a NULL and a Smi.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &generic_stub_call);
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
  __ mov(ebx, Factory::empty_fixed_array());
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
  __ mov(edi, Factory::undefined_value());

  // eax: argc
  // ecx: first argument
  // edx: first in-object property of the JSObject
  // edi: undefined
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
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
  for (int i = shared->this_property_assignments_count();
       i < shared->CalculateInObjectProperties();
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
  __ IncrementCounter(&Counters::constructed_objects, 1);
  __ IncrementCounter(&Counters::constructed_objects_stub, 1);
  __ ret(0);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Code* code = Builtins::builtin(Builtins::JSConstructStubGeneric);
  Handle<Code> generic_construct_stub(code);
  __ jmp(generic_construct_stub, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
