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

#if defined(V8_TARGET_ARCH_ARM)

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
                       // Number of the cache entry, not scaled.
                       Register offset,
                       Register scratch,
                       Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uint32_t key_off_addr = reinterpret_cast<uint32_t>(key_offset.address());
  uint32_t value_off_addr = reinterpret_cast<uint32_t>(value_offset.address());
  uint32_t map_off_addr = reinterpret_cast<uint32_t>(map_offset.address());

  // Check the relative positions of the address fields.
  ASSERT(value_off_addr > key_off_addr);
  ASSERT((value_off_addr - key_off_addr) % 4 == 0);
  ASSERT((value_off_addr - key_off_addr) < (256 * 4));
  ASSERT(map_off_addr > key_off_addr);
  ASSERT((map_off_addr - key_off_addr) % 4 == 0);
  ASSERT((map_off_addr - key_off_addr) < (256 * 4));

  Label miss;
  Register base_addr = scratch;
  scratch = no_reg;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ add(offset_scratch, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ mov(base_addr, Operand(key_offset));
  __ add(base_addr, base_addr, Operand(offset_scratch, LSL, kPointerSizeLog2));

  // Check that the key in the entry matches the name.
  __ ldr(ip, MemOperand(base_addr, 0));
  __ cmp(name, ip);
  __ b(ne, &miss);

  // Check the map matches.
  __ ldr(ip, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ cmp(ip, scratch2);
  __ b(ne, &miss);

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ ldr(code, MemOperand(base_addr, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  Register flags_reg = base_addr;
  base_addr = no_reg;
  __ ldr(flags_reg, FieldMemOperand(code, Code::kFlagsOffset));
  // It's a nice optimization if this constant is encodable in the bic insn.

  uint32_t mask = Code::kFlagsNotUsedInLookup;
  ASSERT(__ ImmediateFitsAddrMode1Instruction(mask));
  __ bic(flags_reg, flags_reg, Operand(mask));
  // Using cmn and the negative instead of cmp means we can use movw.
  if (flags < 0) {
    __ cmn(flags_reg, Operand(-flags));
  } else {
    __ cmp(flags_reg, Operand(flags));
  }
  __ b(ne, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

  // Jump to the first instruction in the code stub.
  __ add(pc, code, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Miss: fall through.
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
                                             Register scratch0,
                                             Register scratch1) {
  ASSERT(name->IsSymbol());
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1, scratch0, scratch1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);

  Label done;

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  Register map = scratch1;
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ tst(scratch0, Operand(kInterceptorOrAccessCheckNeededMask));
  __ b(ne, miss_label);

  // Check that receiver is a JSObject.
  __ ldrb(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ cmp(scratch0, Operand(FIRST_SPEC_OBJECT_TYPE));
  __ b(lt, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ ldr(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  Register tmp = properties;
  __ LoadRoot(tmp, Heap::kHashTableMapRootIndex);
  __ cmp(map, tmp);
  __ b(ne, miss_label);

  // Restore the temporarily used register.
  __ ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));


  StringDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                     miss_label,
                                                     &done,
                                                     receiver,
                                                     properties,
                                                     name,
                                                     scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);
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

  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  ASSERT(sizeof(Entry) == 12);

  // Make sure the flags does not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));
  ASSERT(!extra.is(receiver));
  ASSERT(!extra.is(name));
  ASSERT(!extra.is(scratch));
  ASSERT(!extra2.is(receiver));
  ASSERT(!extra2.is(name));
  ASSERT(!extra2.is(scratch));
  ASSERT(!extra2.is(extra));

  // Check scratch, extra and extra2 registers are valid.
  ASSERT(!scratch.is(no_reg));
  ASSERT(!extra.is(no_reg));
  ASSERT(!extra2.is(no_reg));
  ASSERT(!extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1,
                      extra2, extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ ldr(scratch, FieldMemOperand(name, String::kHashFieldOffset));
  __ ldr(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, Operand(ip));
  uint32_t mask = kPrimaryTableSize - 1;
  // We shift out the last two bits because they are not part of the hash and
  // they are always 01 for maps.
  __ mov(scratch, Operand(scratch, LSR, kHeapObjectTagSize));
  // Mask down the eor argument to the minimum to keep the immediate
  // ARM-encodable.
  __ eor(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask));
  // Prefer and_ to ubfx here because ubfx takes 2 cycles.
  __ and_(scratch, scratch, Operand(mask));

  // Probe the primary table.
  ProbeTable(isolate,
             masm,
             flags,
             kPrimary,
             receiver,
             name,
             scratch,
             extra,
             extra2,
             extra3);

  // Primary miss: Compute hash for secondary probe.
  __ sub(scratch, scratch, Operand(name, LSR, kHeapObjectTagSize));
  uint32_t mask2 = kSecondaryTableSize - 1;
  __ add(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask2));
  __ and_(scratch, scratch, Operand(mask2));

  // Probe the secondary table.
  ProbeTable(isolate,
             masm,
             flags,
             kSecondary,
             receiver,
             name,
             scratch,
             extra,
             extra2,
             extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1,
                      extra2, extra3);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ ldr(prototype,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  __ ldr(prototype,
         FieldMemOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ ldr(prototype, MemOperand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ ldr(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
  Isolate* isolate = masm->isolate();
  // Check we're still in the same context.
  __ ldr(prototype,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ Move(ip, isolate->global_object());
  __ cmp(prototype, ip);
  __ b(ne, miss);
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(isolate->native_context()->get(index)));
  // Load its initial map. The global functions all have initial maps.
  __ Move(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
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
    __ ldr(dst, FieldMemOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    __ ldr(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    __ ldr(dst, FieldMemOperand(dst, offset));
  }
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ CompareObjectType(receiver, scratch, scratch, JS_ARRAY_TYPE);
  __ b(ne, miss_label);

  // Load length directly from the JS array.
  __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Ret();
}


// Generate code to check if an object is a string.  If the object is a
// heap object, its map's instance type is left in the scratch1 register.
// If this is not needed, scratch1 and scratch2 may be the same register.
static void GenerateStringCheck(MacroAssembler* masm,
                                Register receiver,
                                Register scratch1,
                                Register scratch2,
                                Label* smi,
                                Label* non_string_object) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, smi);

  // Check that the object is a string.
  __ ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ and_(scratch2, scratch1, Operand(kIsNotStringMask));
  // The cast is to resolve the overload for the argument of 0x0.
  __ cmp(scratch2, Operand(static_cast<int32_t>(kStringTag)));
  __ b(ne, non_string_object);
}


// Generate code to load the length from a string object and return the length.
// If the receiver object is not a string or a wrapped string object the
// execution continues at the miss label. The register containing the
// receiver is potentially clobbered.
void StubCompiler::GenerateLoadStringLength(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss,
                                            bool support_wrappers) {
  Label check_wrapper;

  // Check if the object is a string leaving the instance type in the
  // scratch1 register.
  GenerateStringCheck(masm, receiver, scratch1, scratch2, miss,
                      support_wrappers ? &check_wrapper : miss);

  // Load length directly from the string.
  __ ldr(r0, FieldMemOperand(receiver, String::kLengthOffset));
  __ Ret();

  if (support_wrappers) {
    // Check if the object is a JSValue wrapper.
    __ bind(&check_wrapper);
    __ cmp(scratch1, Operand(JS_VALUE_TYPE));
    __ b(ne, miss);

    // Unwrap the value and check if the wrapped value is a string.
    __ ldr(scratch1, FieldMemOperand(receiver, JSValue::kValueOffset));
    GenerateStringCheck(masm, scratch1, scratch2, scratch2, miss, miss);
    __ ldr(r0, FieldMemOperand(scratch1, String::kLengthOffset));
    __ Ret();
  }
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(r0, scratch1);
  __ Ret();
}


// Generate StoreField code, value is passed in r0 register.
// When leaving generated code after success, the receiver_reg and name_reg
// may be clobbered.  Upon branch to miss_label, the receiver and name
// registers have their original values.
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
  // r0 : value
  Label exit;

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
  __ CheckMap(receiver_reg, scratch1, Handle<Map>(object->map()), miss_label,
              DO_SMI_CHECK, mode);

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
    __ push(receiver_reg);
    __ mov(r2, Operand(transition));
    __ Push(r2, r0);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  if (!transition.is_null()) {
    // Update the map of the object.
    __ mov(scratch1, Operand(transition));
    __ str(scratch1, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));

    // Update the write barrier for the map field and pass the now unused
    // name_reg as scratch register.
    __ RecordWriteField(receiver_reg,
                        HeapObject::kMapOffset,
                        scratch1,
                        name_reg,
                        kLRHasNotBeenSaved,
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
    __ str(r0, FieldMemOperand(receiver_reg, offset));

    // Skip updating write barrier if storing a smi.
    __ JumpIfSmi(r0, &exit);

    // Update the write barrier for the array address.
    // Pass the now unused name_reg as a scratch register.
    __ mov(name_reg, r0);
    __ RecordWriteField(receiver_reg,
                        offset,
                        name_reg,
                        scratch1,
                        kLRHasNotBeenSaved,
                        kDontSaveFPRegs);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ ldr(scratch1,
           FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ str(r0, FieldMemOperand(scratch1, offset));

    // Skip updating write barrier if storing a smi.
    __ JumpIfSmi(r0, &exit);

    // Update the write barrier for the array address.
    // Ok to clobber receiver_reg and name_reg, since we return.
    __ mov(name_reg, r0);
    __ RecordWriteField(scratch1,
                        offset,
                        name_reg,
                        receiver_reg,
                        kLRHasNotBeenSaved,
                        kDontSaveFPRegs);
  }

  // Return the value (register r0).
  __ bind(&exit);
  __ Ret();
}


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  ASSERT(kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC);
  Handle<Code> code = (kind == Code::LOAD_IC)
      ? masm->isolate()->builtins()->LoadIC_Miss()
      : masm->isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(code, RelocInfo::CODE_TARGET);
}


static void GenerateCallFunction(MacroAssembler* masm,
                                 Handle<Object> object,
                                 const ParameterCount& arguments,
                                 Label* miss,
                                 Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r0: receiver
  //  -- r1: function to call
  // -----------------------------------

  // Check that the function really is a function.
  __ JumpIfSmi(r1, miss);
  __ CompareObjectType(r1, r3, r3, JS_FUNCTION_TYPE);
  __ b(ne, miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ ldr(r3, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));
    __ str(r3, MemOperand(sp, arguments.immediate() * kPointerSize));
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(r1, arguments, JUMP_FUNCTION, NullCallWrapper(), call_kind);
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
  __ mov(scratch, Operand(interceptor));
  __ push(scratch);
  __ push(receiver);
  __ push(holder);
  __ ldr(scratch, FieldMemOperand(scratch, InterceptorInfo::kDataOffset));
  __ push(scratch);
  __ mov(scratch, Operand(ExternalReference::isolate_address()));
  __ push(scratch);
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
  __ mov(r0, Operand(6));
  __ mov(r1, Operand(ref));

  CEntryStub stub(1);
  __ CallStub(&stub);
}


static const int kFastApiCallArguments = 4;

// Reserves space for the extra arguments to API function in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiDirectCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm,
                                       Register scratch) {
  __ mov(scratch, Operand(Smi::FromInt(0)));
  for (int i = 0; i < kFastApiCallArguments; i++) {
    __ push(scratch);
  }
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm) {
  __ Drop(kFastApiCallArguments);
}


static void GenerateFastApiDirectCall(MacroAssembler* masm,
                                      const CallOptimization& optimization,
                                      int argc) {
  // ----------- S t a t e -------------
  //  -- sp[0]              : holder (set by CheckPrototypes)
  //  -- sp[4]              : callee JS function
  //  -- sp[8]              : call data
  //  -- sp[12]             : isolate
  //  -- sp[16]             : last JS argument
  //  -- ...
  //  -- sp[(argc + 3) * 4] : first JS argument
  //  -- sp[(argc + 4) * 4] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  Handle<JSFunction> function = optimization.constant_function();
  __ LoadHeapObject(r5, function);
  __ ldr(cp, FieldMemOperand(r5, JSFunction::kContextOffset));

  // Pass the additional arguments.
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data(api_call_info->data());
  if (masm->isolate()->heap()->InNewSpace(*call_data)) {
    __ Move(r0, api_call_info);
    __ ldr(r6, FieldMemOperand(r0, CallHandlerInfo::kDataOffset));
  } else {
    __ Move(r6, call_data);
  }
  __ mov(r7, Operand(ExternalReference::isolate_address()));
  // Store JS function, call data and isolate.
  __ stm(ib, sp, r5.bit() | r6.bit() | r7.bit());

  // Prepare arguments.
  __ add(r2, sp, Operand(3 * kPointerSize));

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // r0 = v8::Arguments&
  // Arguments is after the return address.
  __ add(r0, sp, Operand(1 * kPointerSize));
  // v8::Arguments::implicit_args_
  __ str(r2, MemOperand(r0, 0 * kPointerSize));
  // v8::Arguments::values_
  __ add(ip, r2, Operand(argc * kPointerSize));
  __ str(ip, MemOperand(r0, 1 * kPointerSize));
  // v8::Arguments::length_ = argc
  __ mov(ip, Operand(argc));
  __ str(ip, MemOperand(r0, 2 * kPointerSize));
  // v8::Arguments::is_construct_call = 0
  __ mov(ip, Operand(0));
  __ str(ip, MemOperand(r0, 3 * kPointerSize));

  const int kStackUnwindSpace = argc + kFastApiCallArguments + 1;
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference ref = ExternalReference(&fun,
                                            ExternalReference::DIRECT_API_CALL,
                                            masm->isolate());
  AllowExternalCallThatCantCauseGC scope(masm);

  __ CallApiFunctionAndReturn(ref, kStackUnwindSpace);
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
    Counters* counters = masm->isolate()->counters();
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

    __ IncrementCounter(counters->call_const_interceptor(), 1,
                        scratch1, scratch2);

    if (can_do_fast_api_call) {
      __ IncrementCounter(counters->call_const_interceptor_fast_api(), 1,
                          scratch1, scratch2);
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
    LoadWithInterceptor(masm, receiver, holder, interceptor_holder, scratch2,
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
      GenerateFastApiDirectCall(masm, optimization, arguments_.immediate());
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
      FreeSpaceForFastApiCall(masm);
      __ b(miss_label);
    }

    // Invoke a regular function.
    __ bind(&regular_invoke);
    if (can_do_fast_api_call) {
      FreeSpaceForFastApiCall(masm);
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

    // Call a runtime function to load the interceptor property.
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
                           Register scratch,
                           Label* interceptor_succeeded) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(holder, name_);
      CompileCallLoadPropertyWithInterceptor(masm,
                                             receiver,
                                             holder,
                                             name_,
                                             holder_obj);
      __ pop(name_);  // Restore the name.
      __ pop(receiver);  // Restore the holder.
    }
    // If interceptor returns no-result sentinel, call the constant function.
    __ LoadRoot(scratch, Heap::kNoInterceptorResultSentinelRootIndex);
    __ cmp(r0, scratch);
    __ b(ne, interceptor_succeeded);
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
  Code::ExtraICState extra_ic_state_;
};


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
  __ mov(scratch, Operand(cell));
  __ ldr(scratch,
         FieldMemOperand(scratch, JSGlobalPropertyCell::kValueOffset));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch, ip);
  __ b(ne, miss);
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


// Convert and store int passed in register ival to IEEE 754 single precision
// floating point value at memory location (dst + 4 * wordoffset)
// If VFP3 is available use it for conversion.
static void StoreIntAsFloat(MacroAssembler* masm,
                            Register dst,
                            Register wordoffset,
                            Register ival,
                            Register fval,
                            Register scratch1,
                            Register scratch2) {
  if (CpuFeatures::IsSupported(VFP2)) {
    CpuFeatures::Scope scope(VFP2);
    __ vmov(s0, ival);
    __ add(scratch1, dst, Operand(wordoffset, LSL, 2));
    __ vcvt_f32_s32(s0, s0);
    __ vstr(s0, scratch1, 0);
  } else {
    Label not_special, done;
    // Move sign bit from source to destination.  This works because the sign
    // bit in the exponent word of the double has the same position and polarity
    // as the 2's complement sign bit in a Smi.
    ASSERT(kBinary32SignMask == 0x80000000u);

    __ and_(fval, ival, Operand(kBinary32SignMask), SetCC);
    // Negate value if it is negative.
    __ rsb(ival, ival, Operand(0, RelocInfo::NONE), LeaveCC, ne);

    // We have -1, 0 or 1, which we treat specially. Register ival contains
    // absolute value: it is either equal to 1 (special case of -1 and 1),
    // greater than 1 (not a special case) or less than 1 (special case of 0).
    __ cmp(ival, Operand(1));
    __ b(gt, &not_special);

    // For 1 or -1 we need to or in the 0 exponent (biased).
    static const uint32_t exponent_word_for_1 =
        kBinary32ExponentBias << kBinary32ExponentShift;

    __ orr(fval, fval, Operand(exponent_word_for_1), LeaveCC, eq);
    __ b(&done);

    __ bind(&not_special);
    // Count leading zeros.
    // Gets the wrong answer for 0, but we already checked for that case above.
    Register zeros = scratch2;
    __ CountLeadingZeros(zeros, ival, scratch1);

    // Compute exponent and or it into the exponent register.
    __ rsb(scratch1,
           zeros,
           Operand((kBitsPerInt - 1) + kBinary32ExponentBias));

    __ orr(fval,
           fval,
           Operand(scratch1, LSL, kBinary32ExponentShift));

    // Shift up the source chopping the top bit off.
    __ add(zeros, zeros, Operand(1));
    // This wouldn't work for 1 and -1 as the shift would be 32 which means 0.
    __ mov(ival, Operand(ival, LSL, zeros));
    // And the top (top 20 bits).
    __ orr(fval,
           fval,
           Operand(ival, LSR, kBitsPerInt - kBinary32MantissaBits));

    __ bind(&done);
    __ str(fval, MemOperand(dst, wordoffset, LSL, 2));
  }
}


// Convert unsigned integer with specified number of leading zeroes in binary
// representation to IEEE 754 double.
// Integer to convert is passed in register hiword.
// Resulting double is returned in registers hiword:loword.
// This functions does not work correctly for 0.
static void GenerateUInt2Double(MacroAssembler* masm,
                                Register hiword,
                                Register loword,
                                Register scratch,
                                int leading_zeroes) {
  const int meaningful_bits = kBitsPerInt - leading_zeroes - 1;
  const int biased_exponent = HeapNumber::kExponentBias + meaningful_bits;

  const int mantissa_shift_for_hi_word =
      meaningful_bits - HeapNumber::kMantissaBitsInTopWord;

  const int mantissa_shift_for_lo_word =
      kBitsPerInt - mantissa_shift_for_hi_word;

  __ mov(scratch, Operand(biased_exponent << HeapNumber::kExponentShift));
  if (mantissa_shift_for_hi_word > 0) {
    __ mov(loword, Operand(hiword, LSL, mantissa_shift_for_lo_word));
    __ orr(hiword, scratch, Operand(hiword, LSR, mantissa_shift_for_hi_word));
  } else {
    __ mov(loword, Operand(0, RelocInfo::NONE));
    __ orr(hiword, scratch, Operand(hiword, LSL, mantissa_shift_for_hi_word));
  }

  // If least significant bit of biased exponent was not 1 it was corrupted
  // by most significant bit of mantissa so we should fix that.
  if (!(biased_exponent & 1)) {
    __ bic(hiword, hiword, Operand(1 << HeapNumber::kExponentShift));
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
  int depth = 0;

  if (save_at_depth == depth) {
    __ str(reg, MemOperand(sp));
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

      __ ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ ldr(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      Handle<Map> current_map(current->map());
      __ CheckMap(reg, scratch1, current_map, miss, DONT_DO_SMI_CHECK,
                  ALLOW_ELEMENT_TRANSITION_MAPS);

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch2, miss);
      }
      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (heap()->InNewSpace(*prototype)) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ ldr(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ mov(reg, Operand(prototype));
      }
    }

    if (save_at_depth == depth) {
      __ str(reg, MemOperand(sp));
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }

  // Log the check depth.
  LOG(masm()->isolate(), IntEvent("check-maps-depth", depth + 1));

  // Check the holder map.
  __ CheckMap(reg, scratch1, Handle<Map>(current->map()), miss,
              DONT_DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

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

  // Check that the maps haven't changed.
  Register reg = CheckPrototypes(
      object, receiver, holder, scratch1, scratch2, scratch3, name, miss);
  GenerateFastPropertyLoad(masm(), r0, reg, holder, index);
  __ Ret();
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
  __ LoadHeapObject(r0, value);
  __ Ret();
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
  __ ldr(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

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
  // pointer into the dictionary. Check that the value is the callback.
  Register pointer = scratch3;
  const int kElementsStartOffset = StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ ldr(scratch2, FieldMemOperand(pointer, kValueOffset));
  __ cmp(scratch2, Operand(callback));
  __ b(ne, miss);
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

  // Build AccessorInfo::args_ list on the stack and push property name below
  // the exit frame to make GC aware of them and store pointers to them.
  __ push(receiver);
  __ mov(scratch2, sp);  // scratch2 = AccessorInfo::args_
  if (heap()->InNewSpace(callback->data())) {
    __ Move(scratch3, callback);
    __ ldr(scratch3, FieldMemOperand(scratch3, AccessorInfo::kDataOffset));
  } else {
    __ Move(scratch3, Handle<Object>(callback->data()));
  }
  __ Push(reg, scratch3);
  __ mov(scratch3, Operand(ExternalReference::isolate_address()));
  __ Push(scratch3, name_reg);
  __ mov(r0, sp);  // r0 = Handle<String>

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm(), StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create AccessorInfo instance on the stack above the exit frame with
  // scratch2 (internal::Object** args_) as the data.
  __ str(scratch2, MemOperand(sp, 1 * kPointerSize));
  __ add(r1, sp, Operand(1 * kPointerSize));  // r1 = AccessorInfo&

  const int kStackUnwindSpace = 5;
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ExternalReference ref =
      ExternalReference(&fun,
                        ExternalReference::DIRECT_GETTER_CALL,
                        masm()->isolate());
  __ CallApiFunctionAndReturn(ref, kStackUnwindSpace);
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
        __ Push(receiver, holder_reg, name_reg);
      } else {
        __ Push(holder_reg, name_reg);
      }
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
      __ LoadRoot(scratch1, Heap::kNoInterceptorResultSentinelRootIndex);
      __ cmp(r0, scratch1);
      __ b(eq, &interceptor_failed);
      frame_scope.GenerateLeaveFrame();
      __ Ret();

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
      GenerateFastPropertyLoad(masm(), r0, holder_reg,
                               Handle<JSObject>(lookup->holder()),
                               lookup->GetFieldIndex());
      __ Ret();
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
      __ Move(scratch2, callback);
      // holder_reg is either receiver or scratch1.
      if (!receiver.is(holder_reg)) {
        ASSERT(scratch1.is(holder_reg));
        __ Push(receiver, holder_reg);
      } else {
        __ push(receiver);
        __ push(holder_reg);
      }
      __ ldr(scratch3,
             FieldMemOperand(scratch2, AccessorInfo::kDataOffset));
      __ mov(scratch1, Operand(ExternalReference::isolate_address()));
      __ Push(scratch3, scratch1, scratch2, name_reg);

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            masm()->isolate());
      __ TailCallExternalReference(ref, 6, 1);
    }
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    Register holder_reg = CheckPrototypes(object, receiver, interceptor_holder,
                                          scratch1, scratch2, scratch3,
                                          name, miss);
    PushInterceptorArguments(masm(), receiver, holder_reg,
                             name_reg, interceptor_holder);

    ExternalReference ref =
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForLoad),
                          masm()->isolate());
    __ TailCallExternalReference(ref, 6, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(Handle<String> name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ cmp(r2, Operand(name));
    __ b(ne, miss);
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
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));

  // Check that the maps haven't changed.
  __ JumpIfSmi(r0, miss);
  CheckPrototypes(object, r0, holder, r3, r1, r4, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Label* miss) {
  // Get the value from the cell.
  __ mov(r3, Operand(cell));
  __ ldr(r1, FieldMemOperand(r3, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (heap()->InNewSpace(*function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(r1, miss);
    __ CompareObjectType(r1, r3, r3, JS_FUNCTION_TYPE);
    __ b(ne, miss);

    // Check the shared function info. Make sure it hasn't changed.
    __ Move(r3, Handle<SharedFunctionInfo>(function->shared()));
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ cmp(r4, r3);
  } else {
    __ cmp(r1, Operand(function));
  }
  __ b(ne, miss);
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
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  const int argc = arguments().immediate();

  // Get the receiver of the function from the stack into r0.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(r0, &miss);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, r0, holder, r1, r3, r4, name, &miss);
  GenerateFastPropertyLoad(masm(), r1, reg, holder, index);

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_state_);

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
  //  -- r2    : name
  //  -- lr    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  Register receiver = r1;
  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(Handle<JSObject>::cast(object), receiver, holder, r3, r0, r4,
                  name, &miss);

  if (argc == 0) {
    // Nothing to do, just return the length.
    __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
    __ Drop(argc + 1);
    __ Ret();
  } else {
    Label call_builtin;

    if (argc == 1) {  // Otherwise fall through to call the builtin.
      Label attempt_to_grow_elements;

      Register elements = r6;
      Register end_elements = r5;
      // Get the elements array of the object.
      __ ldr(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

      // Check that the elements are in fast mode and writable.
      __ CheckMap(elements,
                  r0,
                  Heap::kFixedArrayMapRootIndex,
                  &call_builtin,
                  DONT_DO_SMI_CHECK);


      // Get the array's length into r0 and calculate new length.
      __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ add(r0, r0, Operand(Smi::FromInt(argc)));

      // Get the elements' length.
      __ ldr(r4, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmp(r0, r4);
      __ b(gt, &attempt_to_grow_elements);

      // Check if value is a smi.
      Label with_write_barrier;
      __ ldr(r4, MemOperand(sp, (argc - 1) * kPointerSize));
      __ JumpIfNotSmi(r4, &with_write_barrier);

      // Save new length.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Store the value.
      // We may need a register containing the address end_elements below,
      // so write back the value in end_elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      const int kEndElementsOffset =
          FixedArray::kHeaderSize - kHeapObjectTag - argc * kPointerSize;
      __ str(r4, MemOperand(end_elements, kEndElementsOffset, PreIndex));

      // Check for a smi.
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&with_write_barrier);

      __ ldr(r3, FieldMemOperand(receiver, HeapObject::kMapOffset));

      if (FLAG_smi_only_arrays  && !FLAG_trace_elements_transitions) {
        Label fast_object, not_fast_object;
        __ CheckFastObjectElements(r3, r7, &not_fast_object);
        __ jmp(&fast_object);
        // In case of fast smi-only, convert to fast object, otherwise bail out.
        __ bind(&not_fast_object);
        __ CheckFastSmiElements(r3, r7, &call_builtin);
        // edx: receiver
        // r3: map
        Label try_holey_map;
        __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                               FAST_ELEMENTS,
                                               r3,
                                               r7,
                                               &try_holey_map);
        __ mov(r2, receiver);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        __ jmp(&fast_object);

        __ bind(&try_holey_map);
        __ LoadTransitionedArrayMapConditional(FAST_HOLEY_SMI_ELEMENTS,
                                               FAST_HOLEY_ELEMENTS,
                                               r3,
                                               r7,
                                               &call_builtin);
        __ mov(r2, receiver);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm());
        __ bind(&fast_object);
      } else {
        __ CheckFastObjectElements(r3, r3, &call_builtin);
      }

      // Save new length.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Store the value.
      // We may need a register containing the address end_elements below,
      // so write back the value in end_elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      __ str(r4, MemOperand(end_elements, kEndElementsOffset, PreIndex));

      __ RecordWrite(elements,
                     end_elements,
                     r4,
                     kLRHasNotBeenSaved,
                     kDontSaveFPRegs,
                     EMIT_REMEMBERED_SET,
                     OMIT_SMI_CHECK);
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&attempt_to_grow_elements);
      // r0: array's length + 1.
      // r4: elements' length.

      if (!FLAG_inline_new) {
        __ b(&call_builtin);
      }

      __ ldr(r2, MemOperand(sp, (argc - 1) * kPointerSize));
      // Growing elements that are SMI-only requires special handling in case
      // the new element is non-Smi. For now, delegate to the builtin.
      Label no_fast_elements_check;
      __ JumpIfSmi(r2, &no_fast_elements_check);
      __ ldr(r7, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ CheckFastObjectElements(r7, r7, &call_builtin);
      __ bind(&no_fast_elements_check);

      Isolate* isolate = masm()->isolate();
      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(isolate);
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(isolate);

      const int kAllocationDelta = 4;
      // Load top and check if it is the end of elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      __ add(end_elements, end_elements, Operand(kEndElementsOffset));
      __ mov(r7, Operand(new_space_allocation_top));
      __ ldr(r3, MemOperand(r7));
      __ cmp(end_elements, r3);
      __ b(ne, &call_builtin);

      __ mov(r9, Operand(new_space_allocation_limit));
      __ ldr(r9, MemOperand(r9));
      __ add(r3, r3, Operand(kAllocationDelta * kPointerSize));
      __ cmp(r3, r9);
      __ b(hi, &call_builtin);

      // We fit and could grow elements.
      // Update new_space_allocation_top.
      __ str(r3, MemOperand(r7));
      // Push the argument.
      __ str(r2, MemOperand(end_elements));
      // Fill the rest with holes.
      __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
      for (int i = 1; i < kAllocationDelta; i++) {
        __ str(r3, MemOperand(end_elements, i * kPointerSize));
      }

      // Update elements' and array's sizes.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      __ add(r4, r4, Operand(Smi::FromInt(kAllocationDelta)));
      __ str(r4, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Elements are in new space, so write barrier is not required.
      __ Drop(argc + 1);
      __ Ret();
    }
    __ bind(&call_builtin);
    __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPush,
                                                   masm()->isolate()),
                                 argc + 1,
                                 1);
  }

  // Handle call cache miss.
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
  //  -- r2    : name
  //  -- lr    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss, return_undefined, call_builtin;
  Register receiver = r1;
  Register elements = r3;
  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(Handle<JSObject>::cast(object), receiver, holder, elements,
                  r4, r0, name, &miss);

  // Get the elements array of the object.
  __ ldr(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ CheckMap(elements,
              r0,
              Heap::kFixedArrayMapRootIndex,
              &call_builtin,
              DONT_DO_SMI_CHECK);

  // Get the array's length into r4 and calculate new length.
  __ ldr(r4, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ sub(r4, r4, Operand(Smi::FromInt(1)), SetCC);
  __ b(lt, &return_undefined);

  // Get the last element.
  __ LoadRoot(r6, Heap::kTheHoleValueRootIndex);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  // We can't address the last element in one operation. Compute the more
  // expensive shift first, and use an offset later on.
  __ add(elements, elements, Operand(r4, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ cmp(r0, r6);
  __ b(eq, &call_builtin);

  // Set the array's length.
  __ str(r4, FieldMemOperand(receiver, JSArray::kLengthOffset));

  // Fill with the hole.
  __ str(r6, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&return_undefined);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&call_builtin);
  __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPop,
                                                 masm()->isolate()),
                               argc + 1,
                               1);

  // Handle call cache miss.
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
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
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
                                            r0,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  r0, holder, r1, r3, r4, name, &miss);

  Register receiver = r1;
  Register index = r4;
  Register result = r0;
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ ldr(index, MemOperand(sp, (argc - 1) * kPointerSize));
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
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(r0, Heap::kNanValueRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in r2.
  __ Move(r2, name);
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
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
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
                                            r0,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(Handle<JSObject>(JSObject::cast(object->GetPrototype())),
                  r0, holder, r1, r3, r4, name, &miss);

  Register receiver = r0;
  Register index = r4;
  Register scratch = r3;
  Register result = r0;
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ ldr(index, MemOperand(sp, (argc - 1) * kPointerSize));
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
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(r0, Heap::kEmptyStringRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in r2.
  __ Move(r2, name);
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
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);

    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the char code argument.
  Register code = r1;
  __ ldr(code, MemOperand(sp, 0 * kPointerSize));

  // Check the code is a smi.
  Label slow;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(code, &slow);

  // Convert the smi code to uint16.
  __ and_(code, code, Operand(Smi::FromInt(0xffff)));

  StringCharFromCodeGenerator generator(code, r0);
  generator.GenerateFast(masm());
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(
      function,  arguments(), JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
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
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  if (!CpuFeatures::IsSupported(VFP2)) {
    return Handle<Code>::null();
  }

  CpuFeatures::Scope scope_vfp2(VFP2);
  const int argc = arguments().immediate();
  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss, slow;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into r0.
  __ ldr(r0, MemOperand(sp, 0 * kPointerSize));

  // If the argument is a smi, just return.
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r0, Operand(kSmiTagMask));
  __ Drop(argc + 1, eq);
  __ Ret(eq);

  __ CheckMap(r0, r1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);

  Label wont_fit_smi, no_vfp_exception, restore_fpscr_and_return;

  // If vfp3 is enabled, we use the fpu rounding with the RM (round towards
  // minus infinity) mode.

  // Load the HeapNumber value.
  // We will need access to the value in the core registers, so we load it
  // with ldrd and move it to the fpu. It also spares a sub instruction for
  // updating the HeapNumber value address, as vldr expects a multiple
  // of 4 offset.
  __ Ldrd(r4, r5, FieldMemOperand(r0, HeapNumber::kValueOffset));
  __ vmov(d1, r4, r5);

  // Backup FPSCR.
  __ vmrs(r3);
  // Set custom FPCSR:
  //  - Set rounding mode to "Round towards Minus Infinity"
  //    (i.e. bits [23:22] = 0b10).
  //  - Clear vfp cumulative exception flags (bits [3:0]).
  //  - Make sure Flush-to-zero mode control bit is unset (bit 22).
  __ bic(r9, r3,
      Operand(kVFPExceptionMask | kVFPRoundingModeMask | kVFPFlushToZeroMask));
  __ orr(r9, r9, Operand(kRoundToMinusInf));
  __ vmsr(r9);

  // Convert the argument to an integer.
  __ vcvt_s32_f64(s0, d1, kFPSCRRounding);

  // Use vcvt latency to start checking for special cases.
  // Get the argument exponent and clear the sign bit.
  __ bic(r6, r5, Operand(HeapNumber::kSignMask));
  __ mov(r6, Operand(r6, LSR, HeapNumber::kMantissaBitsInTopWord));

  // Retrieve FPSCR and check for vfp exceptions.
  __ vmrs(r9);
  __ tst(r9, Operand(kVFPExceptionMask));
  __ b(&no_vfp_exception, eq);

  // Check for NaN, Infinity, and -Infinity.
  // They are invariant through a Math.Floor call, so just
  // return the original argument.
  __ sub(r7, r6, Operand(HeapNumber::kExponentMask
        >> HeapNumber::kMantissaBitsInTopWord), SetCC);
  __ b(&restore_fpscr_and_return, eq);
  // We had an overflow or underflow in the conversion. Check if we
  // have a big exponent.
  __ cmp(r7, Operand(HeapNumber::kMantissaBits));
  // If greater or equal, the argument is already round and in r0.
  __ b(&restore_fpscr_and_return, ge);
  __ b(&wont_fit_smi);

  __ bind(&no_vfp_exception);
  // Move the result back to general purpose register r0.
  __ vmov(r0, s0);
  // Check if the result fits into a smi.
  __ add(r1, r0, Operand(0x40000000), SetCC);
  __ b(&wont_fit_smi, mi);
  // Tag the result.
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));

  // Check for -0.
  __ cmp(r0, Operand(0, RelocInfo::NONE));
  __ b(&restore_fpscr_and_return, ne);
  // r5 already holds the HeapNumber exponent.
  __ tst(r5, Operand(HeapNumber::kSignMask));
  // If our HeapNumber is negative it was -0, so load its address and return.
  // Else r0 is loaded with 0, so we can also just return.
  __ ldr(r0, MemOperand(sp, 0 * kPointerSize), ne);

  __ bind(&restore_fpscr_and_return);
  // Restore FPSCR and return.
  __ vmsr(r3);
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&wont_fit_smi);
  // Restore FPCSR and fall to slow case.
  __ vmsr(r3);

  __ bind(&slow);
  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ InvokeFunction(
      function, arguments(), JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathAbsCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();
  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);
  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into r0.
  __ ldr(r0, MemOperand(sp, 0 * kPointerSize));

  // Check if the argument is a smi.
  Label not_smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(r0, &not_smi);

  // Do bitwise not or do nothing depending on the sign of the
  // argument.
  __ eor(r1, r0, Operand(r0, ASR, kBitsPerInt - 1));

  // Add 1 or do nothing depending on the sign of the argument.
  __ sub(r0, r1, Operand(r0, ASR, kBitsPerInt - 1), SetCC);

  // If the result is still negative, go to the slow case.
  // This only happens for the most negative smi.
  Label slow;
  __ b(mi, &slow);

  // Smi case done.
  __ Drop(argc + 1);
  __ Ret();

  // Check if the argument is a heap number and load its exponent and
  // sign.
  __ bind(&not_smi);
  __ CheckMap(r0, r1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);
  __ ldr(r1, FieldMemOperand(r0, HeapNumber::kExponentOffset));

  // Check the sign of the argument. If the argument is positive,
  // just return it.
  Label negative_sign;
  __ tst(r1, Operand(HeapNumber::kSignMask));
  __ b(ne, &negative_sign);
  __ Drop(argc + 1);
  __ Ret();

  // If the argument is negative, clear the sign, and return a new
  // number.
  __ bind(&negative_sign);
  __ eor(r1, r1, Operand(HeapNumber::kSignMask));
  __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
  __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(r0, r4, r5, r6, &slow);
  __ str(r1, FieldMemOperand(r0, HeapNumber::kExponentOffset));
  __ str(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
  __ Drop(argc + 1);
  __ Ret();

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(
      function, arguments(), JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
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
  Counters* counters = isolate()->counters();

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
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(r1, &miss_before_stack_reserved);

  __ IncrementCounter(counters->call_const(), 1, r0, r3);
  __ IncrementCounter(counters->call_const_fast_api(), 1, r0, r3);

  ReserveSpaceForFastApiCall(masm(), r0);

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4, name,
                  depth, &miss);

  GenerateFastApiDirectCall(masm(), optimization, argc);

  __ bind(&miss);
  FreeSpaceForFastApiCall(masm());

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
  //  -- r2    : name
  //  -- lr    : return address
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

  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(r1, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(masm()->isolate()->counters()->call_const(),
                          1, r0, r3);

      // Check that the maps haven't changed.
      CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                      name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ ldr(r3, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
        __ str(r3, MemOperand(sp, argc * kPointerSize));
      }
      break;

    case STRING_CHECK:
      if (function->IsBuiltin() || !function->shared()->is_classic_mode()) {
        // Check that the object is a two-byte string or a symbol.
        __ CompareObjectType(r1, r3, r3, FIRST_NONSTRING_TYPE);
        __ b(ge, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, r0, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            r0, holder, r3, r1, r4, name, &miss);
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
        __ JumpIfSmi(r1, &fast);
        __ CompareObjectType(r1, r0, r0, HEAP_NUMBER_TYPE);
        __ b(ne, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, r0, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            r0, holder, r3, r1, r4, name, &miss);
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
        __ LoadRoot(ip, Heap::kTrueValueRootIndex);
        __ cmp(r1, ip);
        __ b(eq, &fast);
        __ LoadRoot(ip, Heap::kFalseValueRootIndex);
        __ cmp(r1, ip);
        __ b(ne, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, r0, &miss);
        CheckPrototypes(
            Handle<JSObject>(JSObject::cast(object->GetPrototype())),
            r0, holder, r3, r1, r4, name, &miss);
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
  __ InvokeFunction(
      function, arguments(), JUMP_FUNCTION, NullCallWrapper(), call_kind);

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
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();
  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), r2, extra_state_);
  compiler.Compile(masm(), object, holder, name, &lookup, r1, r3, r4, r0,
                   &miss);

  // Move returned value, the function to call, to r1.
  __ mov(r1, r0);
  // Restore receiver.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_state_);

  // Handle call cache miss.
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
  //  -- r2    : name
  //  -- lr    : return address
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

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ ldr(r3, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));
    __ str(r3, MemOperand(sp, argc * kPointerSize));
  }

  // Set up the context (function already in r1).
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->call_global_inline(), 1, r3, r4);
  ParameterCount expected(function->shared()->formal_parameter_count());
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ InvokeCode(r3, expected, arguments(), JUMP_FUNCTION,
                NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1, r1, r3);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::NORMAL, name);
}


Handle<Code> StoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                  int index,
                                                  Handle<Map> transition,
                                                  Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     name,
                     r1, r2, r3, r4,
                     &miss);
  __ bind(&miss);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
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
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  // Check that the maps haven't changed.
  __ JumpIfSmi(r1, &miss);
  CheckPrototypes(receiver, r1, holder, r3, r4, r5, name, &miss);

  // Stub never generated for non-global objects that require access checks.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());

  __ push(r1);  // receiver
  __ mov(ip, Operand(callback));  // callback info
  __ Push(ip, r2, r0);

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty),
                        masm()->isolate());
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
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
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ push(r0);

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      __ Push(r1, r0);
      ParameterCount actual(1);
      __ InvokeFunction(setter, actual, CALL_FUNCTION, NullCallWrapper(),
                        CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ pop(r0);

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreViaSetter(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(r1, &miss);
  CheckPrototypes(receiver, r1, holder, r3, r4, r5, name, &miss);

  GenerateStoreViaSetter(masm(), setter);

  __ bind(&miss);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> receiver,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the object hasn't changed.
  __ CheckMap(r1, r3, Handle<Map>(receiver->map()), &miss,
              DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(r1, r3, &miss);
  }

  // Stub is never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ Push(r1, r2, r0);  // Receiver, name, value.

  __ mov(r0, Operand(Smi::FromInt(strict_mode_)));
  __ push(r0);  // strict mode

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty),
                        masm()->isolate());
  __ TailCallExternalReference(store_ic_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> StoreStubCompiler::CompileStoreGlobal(
    Handle<GlobalObject> object,
    Handle<JSGlobalPropertyCell> cell,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ ldr(r3, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ cmp(r3, Operand(Handle<Map>(object->map())));
  __ b(ne, &miss);

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ mov(r4, Operand(cell));
  __ LoadRoot(r5, Heap::kTheHoleValueRootIndex);
  __ ldr(r6, FieldMemOperand(r4, JSGlobalPropertyCell::kValueOffset));
  __ cmp(r5, r6);
  __ b(eq, &miss);

  // Store the value in the cell.
  __ str(r0, FieldMemOperand(r4, JSGlobalPropertyCell::kValueOffset));
  // Cells are always rescanned, so no write barrier here.

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->named_store_global_inline(), 1, r4, r3);
  __ Ret();

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->named_store_global_inline_miss(), 1, r4, r3);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, name);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<String> name,
                                                      Handle<JSObject> object,
                                                      Handle<JSObject> last) {
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that receiver is not a smi.
  __ JumpIfSmi(r0, &miss);

  // Check the maps of the full prototype chain.
  CheckPrototypes(object, r0, last, r3, r1, r4, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    GenerateCheckPropertyCell(
        masm(), Handle<GlobalObject>::cast(last), name, r1, &miss);
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ Ret();

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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateLoadField(object, holder, r0, r3, r1, r4, index, name, &miss);
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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  GenerateLoadCallback(object, holder, r0, r2, r3, r1, r4, r5, callback, name,
                       &miss);
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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      __ push(r0);
      ParameterCount actual(0);
      __ InvokeFunction(getter, actual, CALL_FUNCTION, NullCallWrapper(),
                        CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> LoadStubCompiler::CompileLoadViaGetter(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that the maps haven't changed.
  __ JumpIfSmi(r0, &miss);
  CheckPrototypes(receiver, r0, holder, r3, r4, r1, name, &miss);

  GenerateLoadViaGetter(masm(), getter);

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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateLoadConstant(object, holder, r0, r3, r1, r4, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CONSTANT_FUNCTION, name);
}


Handle<Code> LoadStubCompiler::CompileLoadInterceptor(Handle<JSObject> object,
                                                      Handle<JSObject> holder,
                                                      Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(object, holder, &lookup, r0, r2, r3, r1, r4, name,
                          &miss);
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
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ JumpIfSmi(r0, &miss);
  CheckPrototypes(object, r0, holder, r3, r4, r1, name, &miss);

  // Get the value from the cell.
  __ mov(r3, Operand(cell));
  __ ldr(r4, FieldMemOperand(r3, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(r4, ip);
    __ b(eq, &miss);
  }

  __ mov(r0, r4);
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, r1, r3);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->named_load_global_stub_miss(), 1, r1, r3);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(Code::NORMAL, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadField(Handle<String> name,
                                                     Handle<JSObject> receiver,
                                                     Handle<JSObject> holder,
                                                     int index) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadField(receiver, holder, r1, r2, r3, r4, index, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::FIELD, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadCallback(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<AccessorInfo> callback) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadCallback(receiver, holder, r1, r0, r2, r3, r4, r5, callback, name,
                       &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadConstant(
    Handle<String> name,
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<JSFunction> value) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadConstant(receiver, holder, r1, r2, r3, r4, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(Code::CONSTANT_FUNCTION, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadInterceptor(
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver, holder, &lookup, r1, r0, r2, r3, r4, name,
                          &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadArrayLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadArrayLength(masm(), r1, r2, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadStringLength(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1, r2, r3);

  // Check the key is the cached one.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadStringLength(masm(), r1, r2, r3, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1, r2, r3);

  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadFunctionPrototype(
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1, r2, r3);

  // Check the name hasn't changed.
  __ cmp(r0, Operand(name));
  __ b(ne, &miss);

  GenerateLoadFunctionPrototype(masm(), r1, r2, r3, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1, r2, r3);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(Code::CALLBACKS, name);
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  ElementsKind elements_kind = receiver_map->elements_kind();
  Handle<Code> stub = KeyedLoadElementStub(elements_kind).GetCode();

  __ DispatchMap(r1, r2, receiver_map, stub, DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string());
}


Handle<Code> KeyedLoadStubCompiler::CompileLoadPolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(r1, &miss);

  int receiver_count = receiver_maps->length();
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  for (int current = 0; current < receiver_count; ++current) {
    __ mov(ip, Operand(receiver_maps->at(current)));
    __ cmp(r2, ip);
    __ Jump(handler_ics->at(current), RelocInfo::CODE_TARGET, eq);
  }

  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET, al);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


Handle<Code> KeyedStoreStubCompiler::CompileStoreField(Handle<JSObject> object,
                                                       int index,
                                                       Handle<Map> transition,
                                                       Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : name
  //  -- r2    : receiver
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_store_field(), 1, r3, r4);

  // Check that the name has not changed.
  __ cmp(r1, Operand(name));
  __ b(ne, &miss);

  // r3 is used as scratch register. r1 and r2 keep their values if a jump to
  // the miss label is generated.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     name,
                     r2, r1, r3, r4,
                     &miss);
  __ bind(&miss);

  __ DecrementCounter(counters->keyed_store_field(), 1, r3, r4);
  Handle<Code> ic = masm()->isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition.is_null()
                 ? Code::FIELD
                 : Code::MAP_TRANSITION, name);
}


Handle<Code> KeyedStoreStubCompiler::CompileStoreElement(
    Handle<Map> receiver_map) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch
  // -----------------------------------
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
  Handle<Code> stub =
      KeyedStoreElementStub(is_js_array, elements_kind, grow_mode_).GetCode();

  __ DispatchMap(r2, r3, receiver_map, stub, DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string());
}


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(r2, &miss);

  int receiver_count = receiver_maps->length();
  __ ldr(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
  for (int i = 0; i < receiver_count; ++i) {
    __ mov(ip, Operand(receiver_maps->at(i)));
    __ cmp(r3, ip);
    if (transitioned_maps->at(i).is_null()) {
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET, eq);
    } else {
      Label next_map;
      __ b(ne, &next_map);
      __ mov(r3, Operand(transitioned_maps->at(i)));
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET, al);
      __ bind(&next_map);
    }
  }

  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET, al);

  // Return the generated code.
  return GetCode(Code::NORMAL, factory()->empty_string(), MEGAMORPHIC);
}


Handle<Code> ConstructStubCompiler::CompileConstructStub(
    Handle<JSFunction> function) {
  // ----------- S t a t e -------------
  //  -- r0    : argc
  //  -- r1    : constructor
  //  -- lr    : return address
  //  -- [sp]  : last argument
  // -----------------------------------
  Label generic_stub_call;

  // Use r7 for holding undefined which is used in several places below.
  __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Check to see whether there are any break points in the function code. If
  // there are jump to the generic constructor stub which calls the actual
  // code for the function thereby hitting the break points.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kDebugInfoOffset));
  __ cmp(r2, r7);
  __ b(ne, &generic_stub_call);
#endif

  // Load the initial map and verify that it is in fact a map.
  // r1: constructor function
  // r7: undefined
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(r2, &generic_stub_call);
  __ CompareObjectType(r2, r3, r4, MAP_TYPE);
  __ b(ne, &generic_stub_call);

#ifdef DEBUG
  // Cannot construct functions this way.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r7: undefined
  __ CompareInstanceType(r2, r3, JS_FUNCTION_TYPE);
  __ Check(ne, "Function constructed by construct stub.");
#endif

  // Now allocate the JSObject in new space.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r7: undefined
  __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));
  __ AllocateInNewSpace(r3, r4, r5, r6, &generic_stub_call, SIZE_IN_WORDS);

  // Allocated the JSObject, now initialize the fields. Map is set to initial
  // map and properties and elements are set to empty fixed array.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r3: object size (in words)
  // r4: JSObject (not tagged)
  // r7: undefined
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ mov(r5, r4);
  ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
  __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
  ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
  __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
  ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);
  __ str(r6, MemOperand(r5, kPointerSize, PostIndex));

  // Calculate the location of the first argument. The stack contains only the
  // argc arguments.
  __ add(r1, sp, Operand(r0, LSL, kPointerSizeLog2));

  // Fill all the in-object properties with undefined.
  // r0: argc
  // r1: first argument
  // r3: object size (in words)
  // r4: JSObject (not tagged)
  // r5: First in-object property of JSObject (not tagged)
  // r7: undefined
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
  Handle<SharedFunctionInfo> shared(function->shared());
  for (int i = 0; i < shared->this_property_assignments_count(); i++) {
    if (shared->IsThisPropertyAssignmentArgument(i)) {
      Label not_passed, next;
      // Check if the argument assigned to the property is actually passed.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ cmp(r0, Operand(arg_number));
      __ b(le, &not_passed);
      // Argument passed - find it on the stack.
      __ ldr(r2, MemOperand(r1, (arg_number + 1) * -kPointerSize));
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
      __ b(&next);
      __ bind(&not_passed);
      // Set the property to undefined.
      __ str(r7, MemOperand(r5, kPointerSize, PostIndex));
      __ bind(&next);
    } else {
      // Set the property to the constant value.
      Handle<Object> constant(shared->GetThisPropertyAssignmentConstant(i));
      __ mov(r2, Operand(constant));
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
    }
  }

  // Fill the unused in-object property fields with undefined.
  ASSERT(function->has_initial_map());
  for (int i = shared->this_property_assignments_count();
       i < function->initial_map()->inobject_properties();
       i++) {
      __ str(r7, MemOperand(r5, kPointerSize, PostIndex));
  }

  // r0: argc
  // r4: JSObject (not tagged)
  // Move argc to r1 and the JSObject to return to r0 and tag it.
  __ mov(r1, r0);
  __ mov(r0, r4);
  __ orr(r0, r0, Operand(kHeapObjectTag));

  // r0: JSObject
  // r1: argc
  // Remove caller arguments and receiver from the stack and return.
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2));
  __ add(sp, sp, Operand(kPointerSize));
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1, r1, r2);
  __ IncrementCounter(counters->constructed_objects_stub(), 1, r1, r2);
  __ Jump(lr);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Handle<Code> code = masm()->isolate()->builtins()->JSConstructStubGeneric();
  __ Jump(code, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label slow, miss_force_generic;

  Register key = r0;
  Register receiver = r1;

  __ JumpIfNotSmi(key, &miss_force_generic);
  __ mov(r2, Operand(key, ASR, kSmiTagSize));
  __ ldr(r4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadFromNumberDictionary(&slow, r4, key, r0, r2, r3, r5);
  __ Ret();

  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, r2, r3);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ Jump(slow_ic, RelocInfo::CODE_TARGET);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------

  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);
}


static bool IsElementTypeSigned(ElementsKind elements_kind) {
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
      return true;

    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      return false;

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
      return false;
  }
  return false;
}


static void GenerateSmiKeyCheck(MacroAssembler* masm,
                                Register key,
                                Register scratch0,
                                Register scratch1,
                                DwVfpRegister double_scratch0,
                                DwVfpRegister double_scratch1,
                                Label* fail) {
  if (CpuFeatures::IsSupported(VFP2)) {
    CpuFeatures::Scope scope(VFP2);
    Label key_ok;
    // Check for smi or a smi inside a heap number.  We convert the heap
    // number and check if the conversion is exact and fits into the smi
    // range.
    __ JumpIfSmi(key, &key_ok);
    __ CheckMap(key,
                scratch0,
                Heap::kHeapNumberMapRootIndex,
                fail,
                DONT_DO_SMI_CHECK);
    __ sub(ip, key, Operand(kHeapObjectTag));
    __ vldr(double_scratch0, ip, HeapNumber::kValueOffset);
    __ EmitVFPTruncate(kRoundToZero,
                       scratch0,
                       double_scratch0,
                       scratch1,
                       double_scratch1,
                       kCheckForInexactConversion);
    __ b(ne, fail);
    __ TrySmiTag(scratch0, fail, scratch1);
    __ mov(key, scratch0);
    __ bind(&key_ok);
  } else {
    // Check that the key is a smi.
    __ JumpIfNotSmi(key, fail);
  }
}


void KeyedLoadStubCompiler::GenerateLoadExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label miss_force_generic, slow, failed_allocation;

  Register key = r0;
  Register receiver = r1;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key, r4, r5, d1, d2, &miss_force_generic);

  __ ldr(r3, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // r3: elements array

  // Check that the index is in range.
  __ ldr(ip, FieldMemOperand(r3, ExternalArray::kLengthOffset));
  __ cmp(key, ip);
  // Unsigned comparison catches both negative and too-large values.
  __ b(hs, &miss_force_generic);

  __ ldr(r3, FieldMemOperand(r3, ExternalArray::kExternalPointerOffset));
  // r3: base pointer of external storage

  // We are not untagging smi key and instead work with it
  // as if it was premultiplied by 2.
  STATIC_ASSERT((kSmiTag == 0) && (kSmiTagSize == 1));

  Register value = r2;
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
      __ ldrsb(value, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ ldrb(value, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
      __ ldrsh(value, MemOperand(r3, key, LSL, 0));
      break;
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ ldrh(value, MemOperand(r3, key, LSL, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ ldr(value, MemOperand(r3, key, LSL, 1));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      if (CpuFeatures::IsSupported(VFP2)) {
        CpuFeatures::Scope scope(VFP2);
        __ add(r2, r3, Operand(key, LSL, 1));
        __ vldr(s0, r2, 0);
      } else {
        __ ldr(value, MemOperand(r3, key, LSL, 1));
      }
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      if (CpuFeatures::IsSupported(VFP2)) {
        CpuFeatures::Scope scope(VFP2);
        __ add(r2, r3, Operand(key, LSL, 2));
        __ vldr(d0, r2, 0);
      } else {
        __ add(r4, r3, Operand(key, LSL, 2));
        // r4: pointer to the beginning of the double we want to load.
        __ ldr(r2, MemOperand(r4, 0));
        __ ldr(r3, MemOperand(r4, Register::kSizeInBytes));
      }
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

  // For integer array types:
  // r2: value
  // For float array type:
  // s0: value (if VFP3 is supported)
  // r2: value (if VFP3 is not supported)
  // For double array type:
  // d0: value (if VFP3 is supported)
  // r2/r3: value (if VFP3 is not supported)

  if (elements_kind == EXTERNAL_INT_ELEMENTS) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    __ cmp(value, Operand(0xC0000000));
    __ b(mi, &box_int);
    // Tag integer as smi and return it.
    __ mov(r0, Operand(value, LSL, kSmiTagSize));
    __ Ret();

    __ bind(&box_int);
    if (CpuFeatures::IsSupported(VFP2)) {
      CpuFeatures::Scope scope(VFP2);
      // Allocate a HeapNumber for the result and perform int-to-double
      // conversion.  Don't touch r0 or r1 as they are needed if allocation
      // fails.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);

      __ AllocateHeapNumber(r5, r3, r4, r6, &slow, DONT_TAG_RESULT);
      // Now we can use r0 for the result as key is not needed any more.
      __ add(r0, r5, Operand(kHeapObjectTag));
      __ vmov(s0, value);
      __ vcvt_f64_s32(d0, s0);
      __ vstr(d0, r5, HeapNumber::kValueOffset);
      __ Ret();
    } else {
      // Allocate a HeapNumber for the result and perform int-to-double
      // conversion.  Don't touch r0 or r1 as they are needed if allocation
      // fails.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r5, r3, r4, r6, &slow, TAG_RESULT);
      // Now we can use r0 for the result as key is not needed any more.
      __ mov(r0, r5);
      Register dst1 = r1;
      Register dst2 = r3;
      FloatingPointHelper::Destination dest =
          FloatingPointHelper::kCoreRegisters;
      FloatingPointHelper::ConvertIntToDouble(masm,
                                              value,
                                              dest,
                                              d0,
                                              dst1,
                                              dst2,
                                              r9,
                                              s0);
      __ str(dst1, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
      __ str(dst2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      __ Ret();
    }
  } else if (elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    // The test is different for unsigned int values. Since we need
    // the value to be in the range of a positive smi, we can't
    // handle either of the top two bits being set in the value.
    if (CpuFeatures::IsSupported(VFP2)) {
      CpuFeatures::Scope scope(VFP2);
      Label box_int, done;
      __ tst(value, Operand(0xC0000000));
      __ b(ne, &box_int);
      // Tag integer as smi and return it.
      __ mov(r0, Operand(value, LSL, kSmiTagSize));
      __ Ret();

      __ bind(&box_int);
      __ vmov(s0, value);
      // Allocate a HeapNumber for the result and perform int-to-double
      // conversion. Don't use r0 and r1 as AllocateHeapNumber clobbers all
      // registers - also when jumping due to exhausted young space.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r2, r3, r4, r6, &slow, DONT_TAG_RESULT);

      __ vcvt_f64_u32(d0, s0);
      __ vstr(d0, r2, HeapNumber::kValueOffset);

      __ add(r0, r2, Operand(kHeapObjectTag));
      __ Ret();
    } else {
      // Check whether unsigned integer fits into smi.
      Label box_int_0, box_int_1, done;
      __ tst(value, Operand(0x80000000));
      __ b(ne, &box_int_0);
      __ tst(value, Operand(0x40000000));
      __ b(ne, &box_int_1);
      // Tag integer as smi and return it.
      __ mov(r0, Operand(value, LSL, kSmiTagSize));
      __ Ret();

      Register hiword = value;  // r2.
      Register loword = r3;

      __ bind(&box_int_0);
      // Integer does not have leading zeros.
      GenerateUInt2Double(masm, hiword, loword, r4, 0);
      __ b(&done);

      __ bind(&box_int_1);
      // Integer has one leading zero.
      GenerateUInt2Double(masm, hiword, loword, r4, 1);


      __ bind(&done);
      // Integer was converted to double in registers hiword:loword.
      // Wrap it into a HeapNumber. Don't use r0 and r1 as AllocateHeapNumber
      // clobbers all registers - also when jumping due to exhausted young
      // space.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r4, r5, r7, r6, &slow, TAG_RESULT);

      __ str(hiword, FieldMemOperand(r4, HeapNumber::kExponentOffset));
      __ str(loword, FieldMemOperand(r4, HeapNumber::kMantissaOffset));

      __ mov(r0, r4);
      __ Ret();
    }
  } else if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    if (CpuFeatures::IsSupported(VFP2)) {
      CpuFeatures::Scope scope(VFP2);
      // Allocate a HeapNumber for the result. Don't use r0 and r1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r2, r3, r4, r6, &slow, DONT_TAG_RESULT);
      __ vcvt_f64_f32(d0, s0);
      __ vstr(d0, r2, HeapNumber::kValueOffset);

      __ add(r0, r2, Operand(kHeapObjectTag));
      __ Ret();
    } else {
      // Allocate a HeapNumber for the result. Don't use r0 and r1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r3, r4, r5, r6, &slow, TAG_RESULT);
      // VFP is not available, do manual single to double conversion.

      // r2: floating point value (binary32)
      // r3: heap number for result

      // Extract mantissa to r0. OK to clobber r0 now as there are no jumps to
      // the slow case from here.
      __ and_(r0, value, Operand(kBinary32MantissaMask));

      // Extract exponent to r1. OK to clobber r1 now as there are no jumps to
      // the slow case from here.
      __ mov(r1, Operand(value, LSR, kBinary32MantissaBits));
      __ and_(r1, r1, Operand(kBinary32ExponentMask >> kBinary32MantissaBits));

      Label exponent_rebiased;
      __ teq(r1, Operand(0x00));
      __ b(eq, &exponent_rebiased);

      __ teq(r1, Operand(0xff));
      __ mov(r1, Operand(0x7ff), LeaveCC, eq);
      __ b(eq, &exponent_rebiased);

      // Rebias exponent.
      __ add(r1,
             r1,
             Operand(-kBinary32ExponentBias + HeapNumber::kExponentBias));

      __ bind(&exponent_rebiased);
      __ and_(r2, value, Operand(kBinary32SignMask));
      value = no_reg;
      __ orr(r2, r2, Operand(r1, LSL, HeapNumber::kMantissaBitsInTopWord));

      // Shift mantissa.
      static const int kMantissaShiftForHiWord =
          kBinary32MantissaBits - HeapNumber::kMantissaBitsInTopWord;

      static const int kMantissaShiftForLoWord =
          kBitsPerInt - kMantissaShiftForHiWord;

      __ orr(r2, r2, Operand(r0, LSR, kMantissaShiftForHiWord));
      __ mov(r0, Operand(r0, LSL, kMantissaShiftForLoWord));

      __ str(r2, FieldMemOperand(r3, HeapNumber::kExponentOffset));
      __ str(r0, FieldMemOperand(r3, HeapNumber::kMantissaOffset));

      __ mov(r0, r3);
      __ Ret();
    }
  } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    if (CpuFeatures::IsSupported(VFP2)) {
      CpuFeatures::Scope scope(VFP2);
      // Allocate a HeapNumber for the result. Don't use r0 and r1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r2, r3, r4, r6, &slow, DONT_TAG_RESULT);
      __ vstr(d0, r2, HeapNumber::kValueOffset);

      __ add(r0, r2, Operand(kHeapObjectTag));
      __ Ret();
    } else {
      // Allocate a HeapNumber for the result. Don't use r0 and r1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(r7, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(r4, r5, r6, r7, &slow, TAG_RESULT);

      __ str(r2, FieldMemOperand(r4, HeapNumber::kMantissaOffset));
      __ str(r3, FieldMemOperand(r4, HeapNumber::kExponentOffset));
      __ mov(r0, r4);
      __ Ret();
    }

  } else {
    // Tag integer as smi and return it.
    __ mov(r0, Operand(value, LSL, kSmiTagSize));
    __ Ret();
  }

  // Slow case, key and receiver still in r0 and r1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, r2, r3);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------

  __ Push(r1, r0);

  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);

  __ bind(&miss_force_generic);
  Handle<Code> stub =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ Jump(stub, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, check_heap_number, miss_force_generic;

  // Register usage.
  Register value = r0;
  Register key = r1;
  Register receiver = r2;
  // r3 mostly holds the elements array or the destination external array.

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key, r4, r5, d1, d2, &miss_force_generic);

  __ ldr(r3, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check that the index is in range
  __ ldr(ip, FieldMemOperand(r3, ExternalArray::kLengthOffset));
  __ cmp(key, ip);
  // Unsigned comparison catches both negative and too-large values.
  __ b(hs, &miss_force_generic);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // r3: external array.
  if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
    // Double to pixel conversion is only implemented in the runtime for now.
    __ JumpIfNotSmi(value, &slow);
  } else {
    __ JumpIfNotSmi(value, &check_heap_number);
  }
  __ SmiUntag(r5, value);
  __ ldr(r3, FieldMemOperand(r3, ExternalArray::kExternalPointerOffset));

  // r3: base pointer of external storage.
  // r5: value (integer).
  switch (elements_kind) {
    case EXTERNAL_PIXEL_ELEMENTS:
      // Clamp the value to [0..255].
      __ Usat(r5, 8, Operand(r5));
      __ strb(r5, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ strb(r5, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ strh(r5, MemOperand(r3, key, LSL, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ str(r5, MemOperand(r3, key, LSL, 1));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      // Perform int-to-float conversion and store to memory.
      __ SmiUntag(r4, key);
      StoreIntAsFloat(masm, r3, r4, r5, r6, r7, r9);
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ add(r3, r3, Operand(key, LSL, 2));
      // r3: effective address of the double element
      FloatingPointHelper::Destination destination;
      if (CpuFeatures::IsSupported(VFP2)) {
        destination = FloatingPointHelper::kVFPRegisters;
      } else {
        destination = FloatingPointHelper::kCoreRegisters;
      }
      FloatingPointHelper::ConvertIntToDouble(
          masm, r5, destination,
          d0, r6, r7,  // These are: double_dst, dst1, dst2.
          r4, s2);  // These are: scratch2, single_scratch.
      if (destination == FloatingPointHelper::kVFPRegisters) {
        CpuFeatures::Scope scope(VFP2);
        __ vstr(d0, r3, 0);
      } else {
        __ str(r6, MemOperand(r3, 0));
        __ str(r7, MemOperand(r3, Register::kSizeInBytes));
      }
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

  // Entry registers are intact, r0 holds the value which is the return value.
  __ Ret();

  if (elements_kind != EXTERNAL_PIXEL_ELEMENTS) {
    // r3: external array.
    __ bind(&check_heap_number);
    __ CompareObjectType(value, r5, r6, HEAP_NUMBER_TYPE);
    __ b(ne, &slow);

    __ ldr(r3, FieldMemOperand(r3, ExternalArray::kExternalPointerOffset));

    // r3: base pointer of external storage.

    // The WebGL specification leaves the behavior of storing NaN and
    // +/-Infinity into integer arrays basically undefined. For more
    // reproducible behavior, convert these to zero.
    if (CpuFeatures::IsSupported(VFP2)) {
      CpuFeatures::Scope scope(VFP2);

      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
        // vldr requires offset to be a multiple of 4 so we can not
        // include -kHeapObjectTag into it.
        __ sub(r5, r0, Operand(kHeapObjectTag));
        __ vldr(d0, r5, HeapNumber::kValueOffset);
        __ add(r5, r3, Operand(key, LSL, 1));
        __ vcvt_f32_f64(s0, d0);
        __ vstr(s0, r5, 0);
      } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
        __ sub(r5, r0, Operand(kHeapObjectTag));
        __ vldr(d0, r5, HeapNumber::kValueOffset);
        __ add(r5, r3, Operand(key, LSL, 2));
        __ vstr(d0, r5, 0);
      } else {
        // Hoisted load.  vldr requires offset to be a multiple of 4 so we can
        // not include -kHeapObjectTag into it.
        __ sub(r5, value, Operand(kHeapObjectTag));
        __ vldr(d0, r5, HeapNumber::kValueOffset);
        __ EmitECMATruncate(r5, d0, s2, r6, r7, r9);

        switch (elements_kind) {
          case EXTERNAL_BYTE_ELEMENTS:
          case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
            __ strb(r5, MemOperand(r3, key, LSR, 1));
            break;
          case EXTERNAL_SHORT_ELEMENTS:
          case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
            __ strh(r5, MemOperand(r3, key, LSL, 0));
            break;
          case EXTERNAL_INT_ELEMENTS:
          case EXTERNAL_UNSIGNED_INT_ELEMENTS:
            __ str(r5, MemOperand(r3, key, LSL, 1));
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
      }

      // Entry registers are intact, r0 holds the value which is the return
      // value.
      __ Ret();
    } else {
      // VFP3 is not available do manual conversions.
      __ ldr(r5, FieldMemOperand(value, HeapNumber::kExponentOffset));
      __ ldr(r6, FieldMemOperand(value, HeapNumber::kMantissaOffset));

      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
        Label done, nan_or_infinity_or_zero;
        static const int kMantissaInHiWordShift =
            kBinary32MantissaBits - HeapNumber::kMantissaBitsInTopWord;

        static const int kMantissaInLoWordShift =
            kBitsPerInt - kMantissaInHiWordShift;

        // Test for all special exponent values: zeros, subnormal numbers, NaNs
        // and infinities. All these should be converted to 0.
        __ mov(r7, Operand(HeapNumber::kExponentMask));
        __ and_(r9, r5, Operand(r7), SetCC);
        __ b(eq, &nan_or_infinity_or_zero);

        __ teq(r9, Operand(r7));
        __ mov(r9, Operand(kBinary32ExponentMask), LeaveCC, eq);
        __ b(eq, &nan_or_infinity_or_zero);

        // Rebias exponent.
        __ mov(r9, Operand(r9, LSR, HeapNumber::kExponentShift));
        __ add(r9,
               r9,
               Operand(kBinary32ExponentBias - HeapNumber::kExponentBias));

        __ cmp(r9, Operand(kBinary32MaxExponent));
        __ and_(r5, r5, Operand(HeapNumber::kSignMask), LeaveCC, gt);
        __ orr(r5, r5, Operand(kBinary32ExponentMask), LeaveCC, gt);
        __ b(gt, &done);

        __ cmp(r9, Operand(kBinary32MinExponent));
        __ and_(r5, r5, Operand(HeapNumber::kSignMask), LeaveCC, lt);
        __ b(lt, &done);

        __ and_(r7, r5, Operand(HeapNumber::kSignMask));
        __ and_(r5, r5, Operand(HeapNumber::kMantissaMask));
        __ orr(r7, r7, Operand(r5, LSL, kMantissaInHiWordShift));
        __ orr(r7, r7, Operand(r6, LSR, kMantissaInLoWordShift));
        __ orr(r5, r7, Operand(r9, LSL, kBinary32ExponentShift));

        __ bind(&done);
        __ str(r5, MemOperand(r3, key, LSL, 1));
        // Entry registers are intact, r0 holds the value which is the return
        // value.
        __ Ret();

        __ bind(&nan_or_infinity_or_zero);
        __ and_(r7, r5, Operand(HeapNumber::kSignMask));
        __ and_(r5, r5, Operand(HeapNumber::kMantissaMask));
        __ orr(r9, r9, r7);
        __ orr(r9, r9, Operand(r5, LSL, kMantissaInHiWordShift));
        __ orr(r5, r9, Operand(r6, LSR, kMantissaInLoWordShift));
        __ b(&done);
      } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
        __ add(r7, r3, Operand(key, LSL, 2));
        // r7: effective address of destination element.
        __ str(r6, MemOperand(r7, 0));
        __ str(r5, MemOperand(r7, Register::kSizeInBytes));
        __ Ret();
      } else {
        bool is_signed_type = IsElementTypeSigned(elements_kind);
        int meaningfull_bits = is_signed_type ? (kBitsPerInt - 1) : kBitsPerInt;
        int32_t min_value = is_signed_type ? 0x80000000 : 0x00000000;

        Label done, sign;

        // Test for all special exponent values: zeros, subnormal numbers, NaNs
        // and infinities. All these should be converted to 0.
        __ mov(r7, Operand(HeapNumber::kExponentMask));
        __ and_(r9, r5, Operand(r7), SetCC);
        __ mov(r5, Operand(0, RelocInfo::NONE), LeaveCC, eq);
        __ b(eq, &done);

        __ teq(r9, Operand(r7));
        __ mov(r5, Operand(0, RelocInfo::NONE), LeaveCC, eq);
        __ b(eq, &done);

        // Unbias exponent.
        __ mov(r9, Operand(r9, LSR, HeapNumber::kExponentShift));
        __ sub(r9, r9, Operand(HeapNumber::kExponentBias), SetCC);
        // If exponent is negative then result is 0.
        __ mov(r5, Operand(0, RelocInfo::NONE), LeaveCC, mi);
        __ b(mi, &done);

        // If exponent is too big then result is minimal value.
        __ cmp(r9, Operand(meaningfull_bits - 1));
        __ mov(r5, Operand(min_value), LeaveCC, ge);
        __ b(ge, &done);

        __ and_(r7, r5, Operand(HeapNumber::kSignMask), SetCC);
        __ and_(r5, r5, Operand(HeapNumber::kMantissaMask));
        __ orr(r5, r5, Operand(1u << HeapNumber::kMantissaBitsInTopWord));

        __ rsb(r9, r9, Operand(HeapNumber::kMantissaBitsInTopWord), SetCC);
        __ mov(r5, Operand(r5, LSR, r9), LeaveCC, pl);
        __ b(pl, &sign);

        __ rsb(r9, r9, Operand(0, RelocInfo::NONE));
        __ mov(r5, Operand(r5, LSL, r9));
        __ rsb(r9, r9, Operand(meaningfull_bits));
        __ orr(r5, r5, Operand(r6, LSR, r9));

        __ bind(&sign);
        __ teq(r7, Operand(0, RelocInfo::NONE));
        __ rsb(r5, r5, Operand(0, RelocInfo::NONE), LeaveCC, ne);

        __ bind(&done);
        switch (elements_kind) {
          case EXTERNAL_BYTE_ELEMENTS:
          case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
            __ strb(r5, MemOperand(r3, key, LSR, 1));
            break;
          case EXTERNAL_SHORT_ELEMENTS:
          case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
            __ strh(r5, MemOperand(r3, key, LSL, 0));
            break;
          case EXTERNAL_INT_ELEMENTS:
          case EXTERNAL_UNSIGNED_INT_ELEMENTS:
            __ str(r5, MemOperand(r3, key, LSL, 1));
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
      }
    }
  }

  // Slow case, key and receiver still in r0 and r1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, r2, r3);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedStoreIC_Slow();
  __ Jump(slow_ic, RelocInfo::CODE_TARGET);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------

  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastElement(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, r0, r4, r5, d1, d2, &miss_force_generic);

  // Get the elements array.
  __ ldr(r2, FieldMemOperand(r1, JSObject::kElementsOffset));
  __ AssertFastElements(r2);

  // Check that the key is within bounds.
  __ ldr(r3, FieldMemOperand(r2, FixedArray::kLengthOffset));
  __ cmp(r0, Operand(r3));
  __ b(hs, &miss_force_generic);

  // Load the result and make sure it's not the hole.
  __ add(r3, r2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ ldr(r4,
         MemOperand(r3, r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(r4, ip);
  __ b(eq, &miss_force_generic);
  __ mov(r0, r4);
  __ Ret();

  __ bind(&miss_force_generic);
  Handle<Code> stub =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ Jump(stub, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastDoubleElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- r0    : key
  //  -- r1    : receiver
  // -----------------------------------
  Label miss_force_generic, slow_allocate_heapnumber;

  Register key_reg = r0;
  Register receiver_reg = r1;
  Register elements_reg = r2;
  Register heap_number_reg = r2;
  Register indexed_double_offset = r3;
  Register scratch = r4;
  Register scratch2 = r5;
  Register scratch3 = r6;
  Register heap_number_map = r7;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key_reg, r4, r5, d1, d2, &miss_force_generic);

  // Get the elements array.
  __ ldr(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));

  // Check that the key is within bounds.
  __ ldr(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  __ cmp(key_reg, Operand(scratch));
  __ b(hs, &miss_force_generic);

  // Load the upper word of the double in the fixed array and test for NaN.
  __ add(indexed_double_offset, elements_reg,
         Operand(key_reg, LSL, kDoubleSizeLog2 - kSmiTagSize));
  uint32_t upper_32_offset = FixedArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ ldr(scratch, FieldMemOperand(indexed_double_offset, upper_32_offset));
  __ cmp(scratch, Operand(kHoleNanUpper32));
  __ b(&miss_force_generic, eq);

  // Non-NaN. Allocate a new heap number and copy the double value into it.
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(heap_number_reg, scratch2, scratch3,
                        heap_number_map, &slow_allocate_heapnumber, TAG_RESULT);

  // Don't need to reload the upper 32 bits of the double, it's already in
  // scratch.
  __ str(scratch, FieldMemOperand(heap_number_reg,
                                  HeapNumber::kExponentOffset));
  __ ldr(scratch, FieldMemOperand(indexed_double_offset,
                                  FixedArray::kHeaderSize));
  __ str(scratch, FieldMemOperand(heap_number_reg,
                                  HeapNumber::kMantissaOffset));

  __ mov(r0, heap_number_reg);
  __ Ret();

  __ bind(&slow_allocate_heapnumber);
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ Jump(slow_ic, RelocInfo::CODE_TARGET);

  __ bind(&miss_force_generic);
  Handle<Code> miss_ic =
      masm->isolate()->builtins()->KeyedLoadIC_MissForceGeneric();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreFastElement(
    MacroAssembler* masm,
    bool is_js_array,
    ElementsKind elements_kind,
    KeyedAccessGrowMode grow_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch
  //  -- r4    : scratch (elements)
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, grow, slow;
  Label finish_store, check_capacity;

  Register value_reg = r0;
  Register key_reg = r1;
  Register receiver_reg = r2;
  Register scratch = r4;
  Register elements_reg = r3;
  Register length_reg = r5;
  Register scratch2 = r6;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key_reg, r4, r5, d1, d2, &miss_force_generic);

  if (IsFastSmiElementsKind(elements_kind)) {
    __ JumpIfNotSmi(value_reg, &transition_elements_kind);
  }

  // Check that the key is within bounds.
  __ ldr(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
  if (is_js_array) {
    __ ldr(scratch, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ ldr(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis.
  __ cmp(key_reg, scratch);
  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    __ b(hs, &grow);
  } else {
    __ b(hs, &miss_force_generic);
  }

  // Make sure elements is a fast element array, not 'cow'.
  __ CheckMap(elements_reg,
              scratch,
              Heap::kFixedArrayMapRootIndex,
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  __ bind(&finish_store);
  if (IsFastSmiElementsKind(elements_kind)) {
    __ add(scratch,
           elements_reg,
           Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    __ add(scratch,
           scratch,
           Operand(key_reg, LSL, kPointerSizeLog2 - kSmiTagSize));
    __ str(value_reg, MemOperand(scratch));
  } else {
    ASSERT(IsFastObjectElementsKind(elements_kind));
    __ add(scratch,
           elements_reg,
           Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    __ add(scratch,
           scratch,
           Operand(key_reg, LSL, kPointerSizeLog2 - kSmiTagSize));
    __ str(value_reg, MemOperand(scratch));
    __ mov(receiver_reg, value_reg);
    __ RecordWrite(elements_reg,  // Object.
                   scratch,       // Address.
                   receiver_reg,  // Value.
                   kLRHasNotBeenSaved,
                   kDontSaveFPRegs);
  }
  // value_reg (r0) is preserved.
  // Done.
  __ Ret();

  __ bind(&miss_force_generic);
  Handle<Code> ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  __ bind(&transition_elements_kind);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags already set by previous compare.
    __ b(ne, &miss_force_generic);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ ldr(length_reg,
           FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ CompareRoot(elements_reg, Heap::kEmptyFixedArrayRootIndex);
    __ b(ne, &check_capacity);

    int size = FixedArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, elements_reg, scratch, scratch2, &slow,
                          TAG_OBJECT);

    __ LoadRoot(scratch, Heap::kFixedArrayMapRootIndex);
    __ str(scratch, FieldMemOperand(elements_reg, JSObject::kMapOffset));
    __ mov(scratch, Operand(Smi::FromInt(JSArray::kPreallocatedArrayElements)));
    __ str(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    for (int i = 1; i < JSArray::kPreallocatedArrayElements; ++i) {
      __ str(scratch, FieldMemOperand(elements_reg, FixedArray::SizeFor(i)));
    }

    // Store the element at index zero.
    __ str(value_reg, FieldMemOperand(elements_reg, FixedArray::SizeFor(0)));

    // Install the new backing store in the JSArray.
    __ str(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ RecordWriteField(receiver_reg, JSObject::kElementsOffset, elements_reg,
                        scratch, kLRHasNotBeenSaved, kDontSaveFPRegs,
                        EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ mov(length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ Ret();

    __ bind(&check_capacity);
    // Check for cow elements, in general they are not handled by this stub
    __ CheckMap(elements_reg,
                scratch,
                Heap::kFixedCOWArrayMapRootIndex,
                &miss_force_generic,
                DONT_DO_SMI_CHECK);

    __ ldr(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
    __ cmp(length_reg, scratch);
    __ b(hs, &slow);

    // Grow the array and finish the store.
    __ add(length_reg, length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ jmp(&finish_store);

    __ bind(&slow);
    Handle<Code> ic_slow = masm->isolate()->builtins()->KeyedStoreIC_Slow();
    __ Jump(ic_slow, RelocInfo::CODE_TARGET);
  }
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array,
    KeyedAccessGrowMode grow_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch
  //  -- r4    : scratch
  //  -- r5    : scratch
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, grow, slow;
  Label finish_store, check_capacity;

  Register value_reg = r0;
  Register key_reg = r1;
  Register receiver_reg = r2;
  Register elements_reg = r3;
  Register scratch1 = r4;
  Register scratch2 = r5;
  Register scratch3 = r6;
  Register scratch4 = r7;
  Register length_reg = r7;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key_reg, r4, r5, d1, d2, &miss_force_generic);

  __ ldr(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));

  // Check that the key is within bounds.
  if (is_js_array) {
    __ ldr(scratch1, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ ldr(scratch1,
           FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis, unsigned compare catches both negative and out-of-bound
  // indexes.
  __ cmp(key_reg, scratch1);
  if (grow_mode == ALLOW_JSARRAY_GROWTH) {
    __ b(hs, &grow);
  } else {
    __ b(hs, &miss_force_generic);
  }

  __ bind(&finish_store);
  __ StoreNumberToDoubleElements(value_reg,
                                 key_reg,
                                 receiver_reg,
                                 // All registers after this are overwritten.
                                 elements_reg,
                                 scratch1,
                                 scratch2,
                                 scratch3,
                                 scratch4,
                                 &transition_elements_kind);
  __ Ret();

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  __ bind(&transition_elements_kind);
  Handle<Code> ic_miss = masm->isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic_miss, RelocInfo::CODE_TARGET);

  if (is_js_array && grow_mode == ALLOW_JSARRAY_GROWTH) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags already set by previous compare.
    __ b(ne, &miss_force_generic);

    // Transition on values that can't be stored in a FixedDoubleArray.
    Label value_is_smi;
    __ JumpIfSmi(value_reg, &value_is_smi);
    __ ldr(scratch1, FieldMemOperand(value_reg, HeapObject::kMapOffset));
    __ CompareRoot(scratch1, Heap::kHeapNumberMapRootIndex);
    __ b(ne, &transition_elements_kind);
    __ bind(&value_is_smi);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ ldr(length_reg,
           FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ CompareRoot(elements_reg, Heap::kEmptyFixedArrayRootIndex);
    __ b(ne, &check_capacity);

    int size = FixedDoubleArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ AllocateInNewSpace(size, elements_reg, scratch1, scratch2, &slow,
                          TAG_OBJECT);

    // Initialize the new FixedDoubleArray. Leave elements unitialized for
    // efficiency, they are guaranteed to be initialized before use.
    __ LoadRoot(scratch1, Heap::kFixedDoubleArrayMapRootIndex);
    __ str(scratch1, FieldMemOperand(elements_reg, JSObject::kMapOffset));
    __ mov(scratch1,
           Operand(Smi::FromInt(JSArray::kPreallocatedArrayElements)));
    __ str(scratch1,
           FieldMemOperand(elements_reg, FixedDoubleArray::kLengthOffset));

    // Install the new backing store in the JSArray.
    __ str(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ RecordWriteField(receiver_reg, JSObject::kElementsOffset, elements_reg,
                        scratch1, kLRHasNotBeenSaved, kDontSaveFPRegs,
                        EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ mov(length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ jmp(&finish_store);

    __ bind(&check_capacity);
    // Make sure that the backing store can hold additional elements.
    __ ldr(scratch1,
           FieldMemOperand(elements_reg, FixedDoubleArray::kLengthOffset));
    __ cmp(length_reg, scratch1);
    __ b(hs, &slow);

    // Grow the array and finish the store.
    __ add(length_reg, length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ jmp(&finish_store);

    __ bind(&slow);
    Handle<Code> ic_slow = masm->isolate()->builtins()->KeyedStoreIC_Slow();
    __ Jump(ic_slow, RelocInfo::CODE_TARGET);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
