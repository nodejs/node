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

#if defined(V8_TARGET_ARCH_MIPS)

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
                       Register scratch,
                       Register scratch2) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));

  uint32_t key_off_addr = reinterpret_cast<uint32_t>(key_offset.address());
  uint32_t value_off_addr = reinterpret_cast<uint32_t>(value_offset.address());

  // Check the relative positions of the address fields.
  ASSERT(value_off_addr > key_off_addr);
  ASSERT((value_off_addr - key_off_addr) % 4 == 0);
  ASSERT((value_off_addr - key_off_addr) < (256 * 4));

  Label miss;
  Register offsets_base_addr = scratch;

  // Check that the key in the entry matches the name.
  __ li(offsets_base_addr, Operand(key_offset));
  __ sll(scratch2, offset, 1);
  __ addu(scratch2, offsets_base_addr, scratch2);
  __ lw(scratch2, MemOperand(scratch2));
  __ Branch(&miss, ne, name, Operand(scratch2));

  // Get the code entry from the cache.
  __ Addu(offsets_base_addr, offsets_base_addr,
         Operand(value_off_addr - key_off_addr));
  __ sll(scratch2, offset, 1);
  __ addu(scratch2, offsets_base_addr, scratch2);
  __ lw(scratch2, MemOperand(scratch2));

  // Check that the flags match what we're looking for.
  __ lw(scratch2, FieldMemOperand(scratch2, Code::kFlagsOffset));
  __ And(scratch2, scratch2, Operand(~Code::kFlagsNotUsedInLookup));
  __ Branch(&miss, ne, scratch2, Operand(flags));

  // Re-load code entry from cache.
  __ sll(offset, offset, 1);
  __ addu(offset, offset, offsets_base_addr);
  __ lw(offset, MemOperand(offset));

  // Jump to the first instruction in the code stub.
  __ Addu(offset, offset, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(offset);

  // Miss: fall through.
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
  __ lw(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ lbu(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ And(at, scratch0, Operand(kInterceptorOrAccessCheckNeededMask));
  __ Branch(miss_label, ne, at, Operand(zero_reg));


  // Check that receiver is a JSObject.
  __ lbu(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ Branch(miss_label, lt, scratch0, Operand(FIRST_SPEC_OBJECT_TYPE));

  // Load properties array.
  Register properties = scratch0;
  __ lw(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ lw(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  Register tmp = properties;
  __ LoadRoot(tmp, Heap::kHashTableMapRootIndex);
  __ Branch(miss_label, ne, map, Operand(tmp));

  // Restore the temporarily used register.
  __ lw(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  MaybeObject* result = StringDictionaryLookupStub::GenerateNegativeLookup(
      masm,
      miss_label,
      &done,
      receiver,
      properties,
      name,
      scratch1);
  if (result->IsFailure()) return result;

  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);

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
  ASSERT(!extra2.is(receiver));
  ASSERT(!extra2.is(name));
  ASSERT(!extra2.is(scratch));
  ASSERT(!extra2.is(extra));

  // Check scratch, extra and extra2 registers are valid.
  ASSERT(!scratch.is(no_reg));
  ASSERT(!extra.is(no_reg));
  ASSERT(!extra2.is(no_reg));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss, t0);

  // Get the map of the receiver and compute the hash.
  __ lw(scratch, FieldMemOperand(name, String::kHashFieldOffset));
  __ lw(t8, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Addu(scratch, scratch, Operand(t8));
  __ Xor(scratch, scratch, Operand(flags));
  __ And(scratch,
         scratch,
         Operand((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, name, scratch, extra, extra2);

  // Primary miss: Compute hash for secondary probe.
  __ Subu(scratch, scratch, Operand(name));
  __ Addu(scratch, scratch, Operand(flags));
  __ And(scratch,
         scratch,
         Operand((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, name, scratch, extra, extra2);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ lw(prototype, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  __ lw(prototype,
         FieldMemOperand(prototype, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  __ lw(prototype, MemOperand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ lw(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ lw(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
  Isolate* isolate = masm->isolate();
  // Check we're still in the same context.
  __ lw(prototype, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  ASSERT(!prototype.is(at));
  __ li(at, isolate->global());
  __ Branch(miss, ne, prototype, Operand(at));
  // Get the global function with the given index.
  JSFunction* function =
      JSFunction::cast(isolate->global_context()->get(index));
  // Load its initial map. The global functions all have initial maps.
  __ li(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ lw(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
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
    __ lw(dst, FieldMemOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    __ lw(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    __ lw(dst, FieldMemOperand(dst, offset));
  }
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ And(scratch, receiver, Operand(kSmiTagMask));
  __ Branch(miss_label, eq, scratch, Operand(zero_reg));

  // Check that the object is a JS array.
  __ GetObjectType(receiver, scratch, scratch);
  __ Branch(miss_label, ne, scratch, Operand(JS_ARRAY_TYPE));

  // Load length directly from the JS array.
  __ lw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));
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
  __ JumpIfSmi(receiver, smi, t0);

  // Check that the object is a string.
  __ lw(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ And(scratch2, scratch1, Operand(kIsNotStringMask));
  // The cast is to resolve the overload for the argument of 0x0.
  __ Branch(non_string_object,
            ne,
            scratch2,
            Operand(static_cast<int32_t>(kStringTag)));
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
  __ lw(v0, FieldMemOperand(receiver, String::kLengthOffset));
  __ Ret();

  if (support_wrappers) {
    // Check if the object is a JSValue wrapper.
    __ bind(&check_wrapper);
    __ Branch(miss, ne, scratch1, Operand(JS_VALUE_TYPE));

    // Unwrap the value and check if the wrapped value is a string.
    __ lw(scratch1, FieldMemOperand(receiver, JSValue::kValueOffset));
    GenerateStringCheck(masm, scratch1, scratch2, scratch2, miss, miss);
    __ lw(v0, FieldMemOperand(scratch1, String::kLengthOffset));
    __ Ret();
  }
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(v0, scratch1);
  __ Ret();
}


// Generate StoreField code, value is passed in a0 register.
// After executing generated code, the receiver_reg and name_reg
// may be clobbered.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      JSObject* object,
                                      int index,
                                      Map* transition,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch,
                                      Label* miss_label) {
  // a0 : value.
  Label exit;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver_reg, miss_label, scratch);

  // Check that the map of the receiver hasn't changed.
  __ lw(scratch, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));
  __ Branch(miss_label, ne, scratch, Operand(Handle<Map>(object->map())));

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
    __ push(receiver_reg);
    __ li(a2, Operand(Handle<Map>(transition)));
    __ Push(a2, a0);
    __ TailCallExternalReference(
           ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                             masm->isolate()),
           3, 1);
    return;
  }

  if (transition != NULL) {
    // Update the map of the object; no write barrier updating is
    // needed because the map is never in new space.
    __ li(t0, Operand(Handle<Map>(transition)));
    __ sw(t0, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));
  }

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ sw(a0, FieldMemOperand(receiver_reg, offset));

    // Skip updating write barrier if storing a smi.
    __ JumpIfSmi(a0, &exit, scratch);

    // Update the write barrier for the array address.
    // Pass the now unused name_reg as a scratch register.
    __ RecordWrite(receiver_reg, Operand(offset), name_reg, scratch);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array.
    __ lw(scratch, FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ sw(a0, FieldMemOperand(scratch, offset));

    // Skip updating write barrier if storing a smi.
    __ JumpIfSmi(a0, &exit);

    // Update the write barrier for the array address.
    // Ok to clobber receiver_reg and name_reg, since we return.
    __ RecordWrite(scratch, Operand(offset), name_reg, receiver_reg);
  }

  // Return the value (register v0).
  __ bind(&exit);
  __ mov(v0, a0);
  __ Ret();
}


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


static void GenerateCallFunction(MacroAssembler* masm,
                                 Object* object,
                                 const ParameterCount& arguments,
                                 Label* miss,
                                 Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- a0: receiver
  //  -- a1: function to call
  // -----------------------------------
  // Check that the function really is a function.
  __ JumpIfSmi(a1, miss);
  __ GetObjectType(a1, a3, a3);
  __ Branch(miss, ne, a3, Operand(JS_FUNCTION_TYPE));

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ lw(a3, FieldMemOperand(a0, GlobalObject::kGlobalReceiverOffset));
    __ sw(a3, MemOperand(sp, arguments.immediate() * kPointerSize));
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(a1, arguments, JUMP_FUNCTION, NullCallWrapper(), call_kind);
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
  __ li(scratch, Operand(Handle<Object>(interceptor)));
  __ Push(scratch, receiver, holder);
  __ lw(scratch, FieldMemOperand(scratch, InterceptorInfo::kDataOffset));
  __ push(scratch);
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
  __ li(a0, Operand(5));
  __ li(a1, Operand(ref));

  CEntryStub stub(1);
  __ CallStub(&stub);
}


static const int kFastApiCallArguments = 3;


// Reserves space for the extra arguments to FastHandleApiCall in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiDirectCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm,
                                       Register scratch) {
  ASSERT(Smi::FromInt(0) == 0);
  for (int i = 0; i < kFastApiCallArguments; i++) {
    __ push(zero_reg);
  }
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm) {
  __ Drop(kFastApiCallArguments);
}


static MaybeObject* GenerateFastApiDirectCall(MacroAssembler* masm,
                                      const CallOptimization& optimization,
                                      int argc) {
  // ----------- S t a t e -------------
  //  -- sp[0]              : holder (set by CheckPrototypes)
  //  -- sp[4]              : callee js function
  //  -- sp[8]              : call data
  //  -- sp[12]             : last js argument
  //  -- ...
  //  -- sp[(argc + 3) * 4] : first js argument
  //  -- sp[(argc + 4) * 4] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  JSFunction* function = optimization.constant_function();
  __ li(t1, Operand(Handle<JSFunction>(function)));
  __ lw(cp, FieldMemOperand(t1, JSFunction::kContextOffset));

  // Pass the additional arguments FastHandleApiCall expects.
  Object* call_data = optimization.api_call_info()->data();
  Handle<CallHandlerInfo> api_call_info_handle(optimization.api_call_info());
  if (masm->isolate()->heap()->InNewSpace(call_data)) {
    __ li(a0, api_call_info_handle);
    __ lw(t2, FieldMemOperand(a0, CallHandlerInfo::kDataOffset));
  } else {
    __ li(t2, Operand(Handle<Object>(call_data)));
  }

  // Store js function and call data.
  __ sw(t1, MemOperand(sp, 1 * kPointerSize));
  __ sw(t2, MemOperand(sp, 2 * kPointerSize));

  // a2 points to call data as expected by Arguments
  // (refer to layout above).
  __ Addu(a2, sp, Operand(2 * kPointerSize));

  Object* callback = optimization.api_call_info()->callback();
  Address api_function_address = v8::ToCData<Address>(callback);
  ApiFunction fun(api_function_address);

  const int kApiStackSpace = 4;

  __ EnterExitFrame(false, kApiStackSpace);

  // NOTE: the O32 abi requires a0 to hold a special pointer when returning a
  // struct from the function (which is currently the case). This means we pass
  // the first argument in a1 instead of a0. TryCallApiFunctionAndReturn
  // will handle setting up a0.

  // a1 = v8::Arguments&
  // Arguments is built at sp + 1 (sp is a reserved spot for ra).
  __ Addu(a1, sp, kPointerSize);

  // v8::Arguments::implicit_args = data
  __ sw(a2, MemOperand(a1, 0 * kPointerSize));
  // v8::Arguments::values = last argument
  __ Addu(t0, a2, Operand(argc * kPointerSize));
  __ sw(t0, MemOperand(a1, 1 * kPointerSize));
  // v8::Arguments::length_ = argc
  __ li(t0, Operand(argc));
  __ sw(t0, MemOperand(a1, 2 * kPointerSize));
  // v8::Arguments::is_construct_call = 0
  __ sw(zero_reg, MemOperand(a1, 3 * kPointerSize));

  // Emitting a stub call may try to allocate (if the code is not
  // already generated). Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
  const int kStackUnwindSpace = argc + kFastApiCallArguments + 1;
  ExternalReference ref =
      ExternalReference(&fun,
                        ExternalReference::DIRECT_API_CALL,
                        masm->isolate());
  return masm->TryCallApiFunctionAndReturn(ref, kStackUnwindSpace);
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
      return masm->isolate()->heap()->undefined_value();
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

    Counters* counters = masm->isolate()->counters();

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
      stub_compiler_->CheckPrototypes(object, receiver,
                                      interceptor_holder, scratch1,
                                      scratch2, scratch3, name, depth1, miss);

    // Invoke an interceptor and if it provides a value,
    // branch to |regular_invoke|.
    Label regular_invoke;
    LoadWithInterceptor(masm, receiver, holder, interceptor_holder, scratch2,
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
      MaybeObject* result = GenerateFastApiDirectCall(masm,
                                                      optimization,
                                                      arguments_.immediate());
      if (result->IsFailure()) return result;
    } else {
      CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
          ? CALL_AS_FUNCTION
          : CALL_AS_METHOD;
      __ InvokeFunction(optimization.constant_function(), arguments_,
                        JUMP_FUNCTION, call_kind);
    }

    // Deferred code for fast API call case---clean preallocated space.
    if (can_do_fast_api_call) {
      __ bind(&miss_cleanup);
      FreeSpaceForFastApiCall(masm);
      __ Branch(miss_label);
    }

    // Invoke a regular function.
    __ bind(&regular_invoke);
    if (can_do_fast_api_call) {
      FreeSpaceForFastApiCall(masm);
    }

    return masm->isolate()->heap()->undefined_value();
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

    // Call a runtime function to load the interceptor property.
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
              IC_Utility(IC::kLoadPropertyWithInterceptorForCall),
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
                           Register scratch,
                           Label* interceptor_succeeded) {
    __ EnterInternalFrame();

    __ Push(holder, name_);

    CompileCallLoadPropertyWithInterceptor(masm,
                                           receiver,
                                           holder,
                                           name_,
                                           holder_obj);

    __ pop(name_);  // Restore the name.
    __ pop(receiver);  // Restore the holder.
    __ LeaveInternalFrame();

    // If interceptor returns no-result sentinel, call the constant function.
    __ LoadRoot(scratch, Heap::kNoInterceptorResultSentinelRootIndex);
    __ Branch(interceptor_succeeded, ne, v0, Operand(scratch));
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
  Code::ExtraICState extra_ic_state_;
};



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
  __ li(scratch, Operand(Handle<Object>(cell)));
  __ lw(scratch,
        FieldMemOperand(scratch, JSGlobalPropertyCell::kValueOffset));
  __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
  __ Branch(miss, ne, scratch, Operand(at));
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


// Convert and store int passed in register ival to IEEE 754 single precision
// floating point value at memory location (dst + 4 * wordoffset)
// If FPU is available use it for conversion.
static void StoreIntAsFloat(MacroAssembler* masm,
                            Register dst,
                            Register wordoffset,
                            Register ival,
                            Register fval,
                            Register scratch1,
                            Register scratch2) {
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ mtc1(ival, f0);
    __ cvt_s_w(f0, f0);
    __ sll(scratch1, wordoffset, 2);
    __ addu(scratch1, dst, scratch1);
    __ swc1(f0, MemOperand(scratch1, 0));
  } else {
    // FPU is not available,  do manual conversions.

    Label not_special, done;
    // Move sign bit from source to destination.  This works because the sign
    // bit in the exponent word of the double has the same position and polarity
    // as the 2's complement sign bit in a Smi.
    ASSERT(kBinary32SignMask == 0x80000000u);

    __ And(fval, ival, Operand(kBinary32SignMask));
    // Negate value if it is negative.
    __ subu(scratch1, zero_reg, ival);
    __ movn(ival, scratch1, fval);

    // We have -1, 0 or 1, which we treat specially. Register ival contains
    // absolute value: it is either equal to 1 (special case of -1 and 1),
    // greater than 1 (not a special case) or less than 1 (special case of 0).
    __ Branch(&not_special, gt, ival, Operand(1));

    // For 1 or -1 we need to or in the 0 exponent (biased).
    static const uint32_t exponent_word_for_1 =
        kBinary32ExponentBias << kBinary32ExponentShift;

    __ Xor(scratch1, ival, Operand(1));
    __ li(scratch2, exponent_word_for_1);
    __ or_(scratch2, fval, scratch2);
    __ movz(fval, scratch2, scratch1);  // Only if ival is equal to 1.
    __ Branch(&done);

    __ bind(&not_special);
    // Count leading zeros.
    // Gets the wrong answer for 0, but we already checked for that case above.
    Register zeros = scratch2;
    __ clz(zeros, ival);

    // Compute exponent and or it into the exponent register.
    __ li(scratch1, (kBitsPerInt - 1) + kBinary32ExponentBias);
    __ subu(scratch1, scratch1, zeros);

    __ sll(scratch1, scratch1, kBinary32ExponentShift);
    __ or_(fval, fval, scratch1);

    // Shift up the source chopping the top bit off.
    __ Addu(zeros, zeros, Operand(1));
    // This wouldn't work for 1 and -1 as the shift would be 32 which means 0.
    __ sllv(ival, ival, zeros);
    // And the top (top 20 bits).
    __ srl(scratch1, ival, kBitsPerInt - kBinary32MantissaBits);
    __ or_(fval, fval, scratch1);

    __ bind(&done);

    __ sll(scratch1, wordoffset, 2);
    __ addu(scratch1, dst, scratch1);
    __ sw(fval, MemOperand(scratch1, 0));
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

  __ li(scratch, biased_exponent << HeapNumber::kExponentShift);
  if (mantissa_shift_for_hi_word > 0) {
    __ sll(loword, hiword, mantissa_shift_for_lo_word);
    __ srl(hiword, hiword, mantissa_shift_for_hi_word);
    __ or_(hiword, scratch, hiword);
  } else {
    __ mov(loword, zero_reg);
    __ sll(hiword, hiword, mantissa_shift_for_hi_word);
    __ or_(hiword, scratch, hiword);
  }

  // If least significant bit of biased exponent was not 1 it was corrupted
  // by most significant bit of mantissa so we should fix that.
  if (!(biased_exponent & 1)) {
    __ li(scratch, 1 << HeapNumber::kExponentShift);
    __ nor(scratch, scratch, scratch);
    __ and_(hiword, hiword, scratch);
  }
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
  int depth = 0;

  if (save_at_depth == depth) {
    __ sw(reg, MemOperand(sp));
  }

  // Check the maps in the prototype chain.
  // Traverse the prototype chain from the object and do map checks.
  JSObject* current = object;
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

      __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now the object is in holder_reg.
      __ lw(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else if (heap()->InNewSpace(prototype)) {
      // Get the map of the current object.
      __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));

      // Branch on the result of the map check.
      __ Branch(miss, ne, scratch1, Operand(Handle<Map>(current->map())));

      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch1, miss);
        // Restore scratch register to be the map of the object.  In the
        // new space case below, we load the prototype from the map in
        // the scratch register.
        __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      }

      reg = holder_reg;  // From now the object is in holder_reg.
      // The prototype is in new space; we cannot store a reference
      // to it in the code. Load it from the map.
      __ lw(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      // Check the map of the current object.
      __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      // Branch on the result of the map check.
      __ Branch(miss, ne, scratch1, Operand(Handle<Map>(current->map())));
      // Check access rights to the global object.  This has to happen
      // after the map check so that we know that the object is
      // actually a global object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch1, miss);
      }
      // The prototype is in old space; load it directly.
      reg = holder_reg;  // From now the object is in holder_reg.
      __ li(reg, Operand(Handle<JSObject>(prototype)));
    }

    if (save_at_depth == depth) {
      __ sw(reg, MemOperand(sp));
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }

  // Check the holder map.
  __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ Branch(miss, ne, scratch1, Operand(Handle<Map>(current->map())));

  // Log the check depth.
  LOG(masm()->isolate(), IntEvent("check-maps-depth", depth + 1));
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
  __ And(scratch1, receiver, Operand(kSmiTagMask));
  __ Branch(miss, eq, scratch1, Operand(zero_reg));

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder, scratch1, scratch2, scratch3,
                      name, miss);
  GenerateFastPropertyLoad(masm(), v0, reg, holder, index);
  __ Ret();
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
  __ JumpIfSmi(receiver, miss, scratch1);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, scratch3, name, miss);

  // Return the constant value.
  __ li(v0, Operand(Handle<Object>(value)));
  __ Ret();
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
  __ JumpIfSmi(receiver, miss, scratch1);

  // Check that the maps haven't changed.
  Register reg =
    CheckPrototypes(object, receiver, holder, scratch1, scratch2, scratch3,
                    name, miss);

  // Build AccessorInfo::args_ list on the stack and push property name below
  // the exit frame to make GC aware of them and store pointers to them.
  __ push(receiver);
  __ mov(scratch2, sp);  // scratch2 = AccessorInfo::args_
  Handle<AccessorInfo> callback_handle(callback);
  if (heap()->InNewSpace(callback_handle->data())) {
    __ li(scratch3, callback_handle);
    __ lw(scratch3, FieldMemOperand(scratch3, AccessorInfo::kDataOffset));
  } else {
    __ li(scratch3, Handle<Object>(callback_handle->data()));
  }
  __ Push(reg, scratch3, name_reg);
  __ mov(a2, scratch2);  // Saved in case scratch2 == a1.
  __ mov(a1, sp);  // a1 (first argument - see note below) = Handle<String>

  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);

  // NOTE: the O32 abi requires a0 to hold a special pointer when returning a
  // struct from the function (which is currently the case). This means we pass
  // the arguments in a1-a2 instead of a0-a1. TryCallApiFunctionAndReturn
  // will handle setting up a0.

  const int kApiStackSpace = 1;

  __ EnterExitFrame(false, kApiStackSpace);
  // Create AccessorInfo instance on the stack above the exit frame with
  // scratch2 (internal::Object **args_) as the data.
  __ sw(a2, MemOperand(sp, kPointerSize));
  // a2 (second argument - see note above) = AccessorInfo&
  __ Addu(a2, sp, kPointerSize);

  // Emitting a stub call may try to allocate (if the code is not
  // already generated).  Do not allow the assembler to perform a
  // garbage collection but instead return the allocation failure
  // object.
  ExternalReference ref =
      ExternalReference(&fun,
                        ExternalReference::DIRECT_GETTER_CALL,
                        masm()->isolate());
  // 4 args - will be freed later by LeaveExitFrame.
  return masm()->TryCallApiFunctionAndReturn(ref, 4);
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
      __ Push(receiver, holder_reg, name_reg);
    } else {
      __ Push(holder_reg, name_reg);
    }

    // Invoke an interceptor.  Note: map checks from receiver to
    // interceptor's holder has been compiled before (see a caller
    // of this method).
    CompileCallLoadPropertyWithInterceptor(masm(),
                                           receiver,
                                           holder_reg,
                                           name_reg,
                                           interceptor_holder);

    // Check if interceptor provided a value for property.  If it's
    // the case, return immediately.
    Label interceptor_failed;
    __ LoadRoot(scratch1, Heap::kNoInterceptorResultSentinelRootIndex);
    __ Branch(&interceptor_failed, eq, v0, Operand(scratch1));
    __ LeaveInternalFrame();
    __ Ret();

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
      GenerateFastPropertyLoad(masm(), v0, holder_reg,
                               lookup->holder(), lookup->GetFieldIndex());
      __ Ret();
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
      __ li(scratch2, Handle<AccessorInfo>(callback));
      // holder_reg is either receiver or scratch1.
      if (!receiver.is(holder_reg)) {
        ASSERT(scratch1.is(holder_reg));
        __ Push(receiver, holder_reg);
        __ lw(scratch3,
              FieldMemOperand(scratch2, AccessorInfo::kDataOffset));
        __ Push(scratch3, scratch2, name_reg);
      } else {
        __ push(receiver);
        __ lw(scratch3,
              FieldMemOperand(scratch2, AccessorInfo::kDataOffset));
        __ Push(holder_reg, scratch3, scratch2, name_reg);
      }

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty),
                            masm()->isolate());
      __ TailCallExternalReference(ref, 5, 1);
    }
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    Register holder_reg = CheckPrototypes(object, receiver, interceptor_holder,
                                          scratch1, scratch2, scratch3,
                                          name, miss);
    PushInterceptorArguments(masm(), receiver, holder_reg,
                             name_reg, interceptor_holder);

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad), masm()->isolate());
    __ TailCallExternalReference(ref, 5, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(String* name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ Branch(miss, ne, a2, Operand(Handle<String>(name)));
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
  __ lw(a0, MemOperand(sp, argc * kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(a0, miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, a0, holder, a3, a1, t0, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(JSGlobalPropertyCell* cell,
                                                    JSFunction* function,
                                                    Label* miss) {
  // Get the value from the cell.
  __ li(a3, Operand(Handle<JSGlobalPropertyCell>(cell)));
  __ lw(a1, FieldMemOperand(a3, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (heap()->InNewSpace(function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(a1, miss);
    __ GetObjectType(a1, a3, a3);
    __ Branch(miss, ne, a3, Operand(JS_FUNCTION_TYPE));

    // Check the shared function info. Make sure it hasn't changed.
    __ li(a3, Handle<SharedFunctionInfo>(function->shared()));
    __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ Branch(miss, ne, t0, Operand(a3));
  } else {
    __ Branch(miss, ne, a1, Operand(Handle<JSFunction>(function)));
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
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  const int argc = arguments().immediate();

  // Get the receiver of the function from the stack into a0.
  __ lw(a0, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(a0, &miss, t0);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, a0, holder, a1, a3, t0, name, &miss);
  GenerateFastPropertyLoad(masm(), a1, reg, holder, index);

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_ic_state_);

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
  //  -- a2    : name
  //  -- ra    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) return heap()->undefined_value();

  Label miss;

  GenerateNameCheck(name, &miss);

  Register receiver = a1;

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ lw(receiver, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(JSObject::cast(object), receiver,
                  holder, a3, v0, t0, name, &miss);

  if (argc == 0) {
    // Nothing to do, just return the length.
    __ lw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));
    __ Drop(argc + 1);
    __ Ret();
  } else {
    Label call_builtin;

    Register elements = a3;
    Register end_elements = t1;

    // Get the elements array of the object.
    __ lw(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

    // Check that the elements are in fast mode and writable.
    __ CheckMap(elements,
                v0,
                Heap::kFixedArrayMapRootIndex,
                &call_builtin,
                DONT_DO_SMI_CHECK);

    if (argc == 1) {  // Otherwise fall through to call the builtin.
      Label exit, with_write_barrier, attempt_to_grow_elements;

      // Get the array's length into v0 and calculate new length.
      __ lw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ Addu(v0, v0, Operand(Smi::FromInt(argc)));

      // Get the element's length.
      __ lw(t0, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ Branch(&attempt_to_grow_elements, gt, v0, Operand(t0));

      // Save new length.
      __ sw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Push the element.
      __ lw(t0, MemOperand(sp, (argc - 1) * kPointerSize));
      // We may need a register containing the address end_elements below,
      // so write back the value in end_elements.
      __ sll(end_elements, v0, kPointerSizeLog2 - kSmiTagSize);
      __ Addu(end_elements, elements, end_elements);
      const int kEndElementsOffset =
          FixedArray::kHeaderSize - kHeapObjectTag - argc * kPointerSize;
      __ sw(t0, MemOperand(end_elements, kEndElementsOffset));
      __ Addu(end_elements, end_elements, kPointerSize);

      // Check for a smi.
      __ JumpIfNotSmi(t0, &with_write_barrier);
      __ bind(&exit);
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&with_write_barrier);
      __ InNewSpace(elements, t0, eq, &exit);
      __ RecordWriteHelper(elements, end_elements, t0);
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&attempt_to_grow_elements);
      // v0: array's length + 1.
      // t0: elements' length.

      if (!FLAG_inline_new) {
        __ Branch(&call_builtin);
      }

      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(
              masm()->isolate());
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(
              masm()->isolate());

      const int kAllocationDelta = 4;
      // Load top and check if it is the end of elements.
      __ sll(end_elements, v0, kPointerSizeLog2 - kSmiTagSize);
      __ Addu(end_elements, elements, end_elements);
      __ Addu(end_elements, end_elements, Operand(kEndElementsOffset));
      __ li(t3, Operand(new_space_allocation_top));
      __ lw(t2, MemOperand(t3));
      __ Branch(&call_builtin, ne, end_elements, Operand(t2));

      __ li(t5, Operand(new_space_allocation_limit));
      __ lw(t5, MemOperand(t5));
      __ Addu(t2, t2, Operand(kAllocationDelta * kPointerSize));
      __ Branch(&call_builtin, hi, t2, Operand(t5));

      // We fit and could grow elements.
      // Update new_space_allocation_top.
      __ sw(t2, MemOperand(t3));
      // Push the argument.
      __ lw(t2, MemOperand(sp, (argc - 1) * kPointerSize));
      __ sw(t2, MemOperand(end_elements));
      // Fill the rest with holes.
      __ LoadRoot(t2, Heap::kTheHoleValueRootIndex);
      for (int i = 1; i < kAllocationDelta; i++) {
        __ sw(t2, MemOperand(end_elements, i * kPointerSize));
      }

      // Update elements' and array's sizes.
      __ sw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      __ Addu(t0, t0, Operand(Smi::FromInt(kAllocationDelta)));
      __ sw(t0, FieldMemOperand(elements, FixedArray::kLengthOffset));

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
  //  -- a2    : name
  //  -- ra    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || cell != NULL) return heap()->undefined_value();

  Label miss, return_undefined, call_builtin;

  Register receiver = a1;
  Register elements = a3;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ lw(receiver, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(JSObject::cast(object),
                  receiver, holder, elements, t0, v0, name, &miss);

  // Get the elements array of the object.
  __ lw(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ CheckMap(elements,
              v0,
              Heap::kFixedArrayMapRootIndex,
              &call_builtin,
              DONT_DO_SMI_CHECK);

  // Get the array's length into t0 and calculate new length.
  __ lw(t0, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Subu(t0, t0, Operand(Smi::FromInt(1)));
  __ Branch(&return_undefined, lt, t0, Operand(zero_reg));

  // Get the last element.
  __ LoadRoot(t2, Heap::kTheHoleValueRootIndex);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  // We can't address the last element in one operation. Compute the more
  // expensive shift first, and use an offset later on.
  __ sll(t1, t0, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(elements, elements, t1);
  __ lw(v0, MemOperand(elements, FixedArray::kHeaderSize - kHeapObjectTag));
  __ Branch(&call_builtin, eq, v0, Operand(t2));

  // Set the array's length.
  __ sw(t0, FieldMemOperand(receiver, JSArray::kLengthOffset));

  // Fill with the hole.
  __ sw(t2, MemOperand(elements, FixedArray::kHeaderSize - kHeapObjectTag));
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&return_undefined);
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&call_builtin);
  __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPop,
                                                 masm()->isolate()),
                               argc + 1,
                               1);

  // Handle call cache miss.
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
  //  -- a2                     : function name
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
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
                                            v0,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), v0, holder,
                  a1, a3, t0, name, &miss);

  Register receiver = a1;
  Register index = t1;
  Register scratch = a3;
  Register result = v0;
  __ lw(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ lw(index, MemOperand(sp, (argc - 1) * kPointerSize));
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
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_code_at_generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(v0, Heap::kNanValueRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in a2.
  __ li(a2, Handle<String>(name));
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
  //  -- a2                     : function name
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
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
                                            v0,
                                            &miss);
  ASSERT(object != holder);
  CheckPrototypes(JSObject::cast(object->GetPrototype()), v0, holder,
                  a1, a3, t0, name, &miss);

  Register receiver = v0;
  Register index = t1;
  Register scratch1 = a1;
  Register scratch2 = a3;
  Register result = v0;
  __ lw(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ lw(index, MemOperand(sp, (argc - 1) * kPointerSize));
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
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(v0, Heap::kEmptyStringRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in a2.
  __ li(a2, Handle<String>(name));
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
  //  -- a2                     : function name
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return heap()->undefined_value();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ lw(a1, MemOperand(sp, 1 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(a1, &miss);

    CheckPrototypes(JSObject::cast(object), a1, holder, v0, a3, t0, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the char code argument.
  Register code = a1;
  __ lw(code, MemOperand(sp, 0 * kPointerSize));

  // Check the code is a smi.
  Label slow;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(code, &slow);

  // Convert the smi code to uint16.
  __ And(code, code, Operand(Smi::FromInt(0xffff)));

  StringCharFromCodeGenerator char_from_code_generator(code, v0);
  char_from_code_generator.GenerateFast(masm());
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_from_code_generator.GenerateSlow(masm(), call_helper);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION, CALL_AS_METHOD);

  __ bind(&miss);
  // a2: function name.
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
  //  -- a2                     : function name
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  if (!CpuFeatures::IsSupported(FPU))
    return heap()->undefined_value();
  CpuFeatures::Scope scope_fpu(FPU);

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return heap()->undefined_value();

  Label miss, slow;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ lw(a1, MemOperand(sp, 1 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(a1, &miss);

    CheckPrototypes(JSObject::cast(object), a1, holder, a0, a3, t0, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into v0.
  __ lw(v0, MemOperand(sp, 0 * kPointerSize));

  // If the argument is a smi, just return.
  STATIC_ASSERT(kSmiTag == 0);
  __ And(t0, v0, Operand(kSmiTagMask));
  __ Drop(argc + 1, eq, t0, Operand(zero_reg));
  __ Ret(eq, t0, Operand(zero_reg));

  __ CheckMap(v0, a1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);

  Label wont_fit_smi, no_fpu_error, restore_fcsr_and_return;

  // If fpu is enabled, we use the floor instruction.

  // Load the HeapNumber value.
  __ ldc1(f0, FieldMemOperand(v0, HeapNumber::kValueOffset));

  // Backup FCSR.
  __ cfc1(a3, FCSR);
  // Clearing FCSR clears the exception mask with no side-effects.
  __ ctc1(zero_reg, FCSR);
  // Convert the argument to an integer.
  __ floor_w_d(f0, f0);

  // Start checking for special cases.
  // Get the argument exponent and clear the sign bit.
  __ lw(t1, FieldMemOperand(v0, HeapNumber::kValueOffset + kPointerSize));
  __ And(t2, t1, Operand(~HeapNumber::kSignMask));
  __ srl(t2, t2, HeapNumber::kMantissaBitsInTopWord);

  // Retrieve FCSR and check for fpu errors.
  __ cfc1(t5, FCSR);
  __ And(t5, t5, Operand(kFCSRExceptionFlagMask));
  __ Branch(&no_fpu_error, eq, t5, Operand(zero_reg));

  // Check for NaN, Infinity, and -Infinity.
  // They are invariant through a Math.Floor call, so just
  // return the original argument.
  __ Subu(t3, t2, Operand(HeapNumber::kExponentMask
        >> HeapNumber::kMantissaBitsInTopWord));
  __ Branch(&restore_fcsr_and_return, eq, t3, Operand(zero_reg));
  // We had an overflow or underflow in the conversion. Check if we
  // have a big exponent.
  // If greater or equal, the argument is already round and in v0.
  __ Branch(&restore_fcsr_and_return, ge, t3,
      Operand(HeapNumber::kMantissaBits));
  __ Branch(&wont_fit_smi);

  __ bind(&no_fpu_error);
  // Move the result back to v0.
  __ mfc1(v0, f0);
  // Check if the result fits into a smi.
  __ Addu(a1, v0, Operand(0x40000000));
  __ Branch(&wont_fit_smi, lt, a1, Operand(zero_reg));
  // Tag the result.
  STATIC_ASSERT(kSmiTag == 0);
  __ sll(v0, v0, kSmiTagSize);

  // Check for -0.
  __ Branch(&restore_fcsr_and_return, ne, v0, Operand(zero_reg));
  // t1 already holds the HeapNumber exponent.
  __ And(t0, t1, Operand(HeapNumber::kSignMask));
  // If our HeapNumber is negative it was -0, so load its address and return.
  // Else v0 is loaded with 0, so we can also just return.
  __ Branch(&restore_fcsr_and_return, eq, t0, Operand(zero_reg));
  __ lw(v0, MemOperand(sp, 0 * kPointerSize));

  __ bind(&restore_fcsr_and_return);
  // Restore FCSR and return.
  __ ctc1(a3, FCSR);

  __ Drop(argc + 1);
  __ Ret();

  __ bind(&wont_fit_smi);
  // Restore FCSR and fall to slow case.
  __ ctc1(a3, FCSR);

  __ bind(&slow);
  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION, CALL_AS_METHOD);

  __ bind(&miss);
  // a2: function name.
  MaybeObject* obj = GenerateMissBranch();
  if (obj->IsFailure()) return obj;

  // Return the generated code.
  return (cell == NULL) ? GetCode(function) : GetCode(NORMAL, name);
}


MaybeObject* CallStubCompiler::CompileMathAbsCall(Object* object,
                                                  JSObject* holder,
                                                  JSGlobalPropertyCell* cell,
                                                  JSFunction* function,
                                                  String* name) {
  // ----------- S t a t e -------------
  //  -- a2                     : function name
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return heap()->undefined_value();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell == NULL) {
    __ lw(a1, MemOperand(sp, 1 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(a1, &miss);

    CheckPrototypes(JSObject::cast(object), a1, holder, v0, a3, t0, name,
                    &miss);
  } else {
    ASSERT(cell->value() == function);
    GenerateGlobalReceiverCheck(JSObject::cast(object), holder, name, &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into v0.
  __ lw(v0, MemOperand(sp, 0 * kPointerSize));

  // Check if the argument is a smi.
  Label not_smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(v0, &not_smi);

  // Do bitwise not or do nothing depending on the sign of the
  // argument.
  __ sra(t0, v0, kBitsPerInt - 1);
  __ Xor(a1, v0, t0);

  // Add 1 or do nothing depending on the sign of the argument.
  __ Subu(v0, a1, t0);

  // If the result is still negative, go to the slow case.
  // This only happens for the most negative smi.
  Label slow;
  __ Branch(&slow, lt, v0, Operand(zero_reg));

  // Smi case done.
  __ Drop(argc + 1);
  __ Ret();

  // Check if the argument is a heap number and load its exponent and
  // sign.
  __ bind(&not_smi);
  __ CheckMap(v0, a1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);
  __ lw(a1, FieldMemOperand(v0, HeapNumber::kExponentOffset));

  // Check the sign of the argument. If the argument is positive,
  // just return it.
  Label negative_sign;
  __ And(t0, a1, Operand(HeapNumber::kSignMask));
  __ Branch(&negative_sign, ne, t0, Operand(zero_reg));
  __ Drop(argc + 1);
  __ Ret();

  // If the argument is negative, clear the sign, and return a new
  // number.
  __ bind(&negative_sign);
  __ Xor(a1, a1, Operand(HeapNumber::kSignMask));
  __ lw(a3, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
  __ LoadRoot(t2, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(v0, t0, t1, t2, &slow);
  __ sw(a1, FieldMemOperand(v0, HeapNumber::kExponentOffset));
  __ sw(a3, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
  __ Drop(argc + 1);
  __ Ret();

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION, CALL_AS_METHOD);

  __ bind(&miss);
  // a2: function name.
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

  Counters* counters = isolate()->counters();

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
  __ lw(a1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(a1, &miss_before_stack_reserved);

  __ IncrementCounter(counters->call_const(), 1, a0, a3);
  __ IncrementCounter(counters->call_const_fast_api(), 1, a0, a3);

  ReserveSpaceForFastApiCall(masm(), a0);

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(JSObject::cast(object), a1, holder, a0, a3, t0, name,
                  depth, &miss);

  MaybeObject* result = GenerateFastApiDirectCall(masm(), optimization, argc);
  if (result->IsFailure()) return result;

  __ bind(&miss);
  FreeSpaceForFastApiCall(masm());

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
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  if (HasCustomCallGenerator(function)) {
    MaybeObject* maybe_result = CompileCustomCall(
        object, holder, NULL, function, name);
    Object* result;
    if (!maybe_result->ToObject(&result)) return maybe_result;
    // Undefined means bail out to regular compiler.
    if (!result->IsUndefined()) return result;
  }

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ lw(a1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ And(t1, a1, Operand(kSmiTagMask));
    __ Branch(&miss, eq, t1, Operand(zero_reg));
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  SharedFunctionInfo* function_info = function->shared();
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(masm()->isolate()->counters()->call_const(),
          1, a0, a3);

      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), a1, holder, a0, a3, t0, name,
                      &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ lw(a3, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
        __ sw(a3, MemOperand(sp, argc * kPointerSize));
      }
      break;

    case STRING_CHECK:
      if (!function->IsBuiltin() && !function_info->strict_mode()) {
        // Calling non-strict non-builtins with a value as the receiver
        // requires boxing.
        __ jmp(&miss);
      } else {
        // Check that the object is a two-byte string or a symbol.
        __ GetObjectType(a1, a3, a3);
        __ Branch(&miss, Ugreater_equal, a3, Operand(FIRST_NONSTRING_TYPE));
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::STRING_FUNCTION_INDEX, a0, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), a0, holder, a3,
                        a1, t0, name, &miss);
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
        __ And(t1, a1, Operand(kSmiTagMask));
        __ Branch(&fast, eq, t1, Operand(zero_reg));
        __ GetObjectType(a1, a0, a0);
        __ Branch(&miss, ne, a0, Operand(HEAP_NUMBER_TYPE));
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::NUMBER_FUNCTION_INDEX, a0, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), a0, holder, a3,
                        a1, t0, name, &miss);
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
        __ LoadRoot(t0, Heap::kTrueValueRootIndex);
        __ Branch(&fast, eq, a1, Operand(t0));
        __ LoadRoot(t0, Heap::kFalseValueRootIndex);
        __ Branch(&miss, ne, a1, Operand(t0));
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateDirectLoadGlobalFunctionPrototype(
            masm(), Context::BOOLEAN_FUNCTION_INDEX, a0, &miss);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), a0, holder, a3,
                        a1, t0, name, &miss);
      }
      break;
    }

    default:
      UNREACHABLE();
  }

  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(function, arguments(), JUMP_FUNCTION, call_kind);

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
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------

  Label miss;

  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ lw(a1, MemOperand(sp, argc * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), a2, extra_ic_state_);
  MaybeObject* result = compiler.Compile(masm(),
                                         object,
                                         holder,
                                         name,
                                         &lookup,
                                         a1,
                                         a3,
                                         t0,
                                         a0,
                                         &miss);
  if (result->IsFailure()) {
    return result;
  }

  // Move returned value, the function to call, to a1.
  __ mov(a1, v0);
  // Restore receiver.
  __ lw(a0, MemOperand(sp, argc * kPointerSize));

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_ic_state_);

  // Handle call cache miss.
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
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------

  if (HasCustomCallGenerator(function)) {
    MaybeObject* maybe_result = CompileCustomCall(
        object, holder, cell, function, name);
    Object* result;
    if (!maybe_result->ToObject(&result)) return maybe_result;
    // Undefined means bail out to regular compiler.
    if (!result->IsUndefined()) return result;
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
    __ lw(a3, FieldMemOperand(a0, GlobalObject::kGlobalReceiverOffset));
    __ sw(a3, MemOperand(sp, argc * kPointerSize));
  }

  // Setup the context (function already in r1).
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->call_global_inline(), 1, a3, t0);
  ASSERT(function->is_compiled());
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  if (V8::UseCrankshaft()) {
    UNIMPLEMENTED_MIPS();
  } else {
    __ InvokeCode(code, expected, arguments(), RelocInfo::CODE_TARGET,
                  JUMP_FUNCTION, call_kind);
  }

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1, a1, a3);
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
  //  -- a0    : value
  //  -- a1    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // Name register might be clobbered.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     a1, a2, a3,
                     &miss);
  __ bind(&miss);
  __ li(a2, Operand(Handle<String>(name)));  // Restore name.
  Handle<Code> ic = masm()->isolate()->builtins()->Builtins::StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* StoreStubCompiler::CompileStoreCallback(JSObject* object,
                                                     AccessorInfo* callback,
                                                     String* name) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(a1, &miss);

  // Check that the map of the object hasn't changed.
  __ lw(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&miss, ne, a3, Operand(Handle<Map>(object->map())));

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(a1, a3, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ push(a1);  // Receiver.
  __ li(a3, Operand(Handle<AccessorInfo>(callback)));  // Callback info.
  __ Push(a3, a2, a0);

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
  return GetCode(CALLBACKS, name);
}


MaybeObject* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                        String* name) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // Check that the object isn't a smi.
  __ JumpIfSmi(a1, &miss);

  // Check that the map of the object hasn't changed.
  __ lw(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&miss, ne, a3, Operand(Handle<Map>(receiver->map())));

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(a1, a3, &miss);
  }

  // Stub is never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ Push(a1, a2, a0);  // Receiver, name, value.

  __ li(a0, Operand(Smi::FromInt(strict_mode_)));
  __ push(a0);  // Strict mode.

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty),
          masm()->isolate());
  __ TailCallExternalReference(store_ic_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic = masm()->isolate()->builtins()->Builtins::StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


MaybeObject* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
                                                   JSGlobalPropertyCell* cell,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ lw(a3, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Branch(&miss, ne, a3, Operand(Handle<Map>(object->map())));

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ li(t0, Operand(Handle<JSGlobalPropertyCell>(cell)));
  __ LoadRoot(t1, Heap::kTheHoleValueRootIndex);
  __ lw(t2, FieldMemOperand(t0, JSGlobalPropertyCell::kValueOffset));
  __ Branch(&miss, eq, t1, Operand(t2));

  // Store the value in the cell.
  __ sw(a0, FieldMemOperand(t0, JSGlobalPropertyCell::kValueOffset));
  __ mov(v0, a0);  // Stored value must be returned in v0.
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->named_store_global_inline(), 1, a1, a3);
  __ Ret();

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->named_store_global_inline_miss(), 1, a1, a3);
  Handle<Code> ic = masm()->isolate()->builtins()->StoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* LoadStubCompiler::CompileLoadNonexistent(String* name,
                                                      JSObject* object,
                                                      JSObject* last) {
  // ----------- S t a t e -------------
  //  -- a0    : receiver
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // Check that the receiver is not a smi.
  __ JumpIfSmi(a0, &miss);

  // Check the maps of the full prototype chain.
  CheckPrototypes(object, a0, last, a3, a1, t0, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    MaybeObject* cell = GenerateCheckPropertyCell(masm(),
                                                  GlobalObject::cast(last),
                                                  name,
                                                  a1,
                                                  &miss);
    if (cell->IsFailure()) {
      miss.Unuse();
      return cell;
    }
  }

  // Return undefined if maps of the full prototype chain is still the same.
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ Ret();

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
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  __ mov(v0, a0);

  GenerateLoadField(object, holder, v0, a3, a1, t0, index, name, &miss);
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
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  MaybeObject* result = GenerateLoadCallback(object, holder, a0, a2, a3, a1, t0,
                                             callback, name, &miss);
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
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  GenerateLoadConstant(object, holder, a0, a3, a1, t0, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


MaybeObject* LoadStubCompiler::CompileLoadInterceptor(JSObject* object,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(object,
                          holder,
                          &lookup,
                          a0,
                          a2,
                          a3,
                          a1,
                          t0,
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
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  Label miss;

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ And(t0, a0, Operand(kSmiTagMask));
    __ Branch(&miss, eq, t0, Operand(zero_reg));
  }

  // Check that the map of the global has not changed.
  CheckPrototypes(object, a0, holder, a3, t0, a1, name, &miss);

  // Get the value from the cell.
  __ li(a3, Operand(Handle<JSGlobalPropertyCell>(cell)));
  __ lw(t0, FieldMemOperand(a3, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Branch(&miss, eq, t0, Operand(at));
  }

  __ mov(v0, t0);
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, a1, a3);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->named_load_global_stub_miss(), 1, a1, a3);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                     JSObject* receiver,
                                                     JSObject* holder,
                                                     int index) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  GenerateLoadField(receiver, holder, a1, a2, a3, t0, index, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(FIELD, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadCallback(
    String* name,
    JSObject* receiver,
    JSObject* holder,
    AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  MaybeObject* result = GenerateLoadCallback(receiver, holder, a1, a0, a2, a3,
                                             t0, callback, name, &miss);
  if (result->IsFailure()) {
    miss.Unuse();
    return result;
  }

  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                        JSObject* receiver,
                                                        JSObject* holder,
                                                        Object* value) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  GenerateLoadConstant(receiver, holder, a1, a2, a3, t0, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                           JSObject* holder,
                                                           String* name) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          a1,
                          a0,
                          a2,
                          a3,
                          t0,
                          name,
                          &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(INTERCEPTOR, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  GenerateLoadArrayLength(masm(), a1, a2, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_string_length(), 1, a2, a3);

  // Check the key is the cached one.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  GenerateLoadStringLength(masm(), a1, a2, a3, &miss, true);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_string_length(), 1, a2, a3);

  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_function_prototype(), 1, a2, a3);

  // Check the name hasn't changed.
  __ Branch(&miss, ne, a0, Operand(Handle<String>(name)));

  GenerateLoadFunctionPrototype(masm(), a1, a2, a3, &miss);
  __ bind(&miss);
  __ DecrementCounter(counters->keyed_load_function_prototype(), 1, a2, a3);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  MaybeObject* maybe_stub = KeyedLoadElementStub(elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(a1,
                 a2,
                 Handle<Map>(receiver_map),
                 Handle<Code>(stub),
                 DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL);
}


MaybeObject* KeyedLoadStubCompiler::CompileLoadMegamorphic(
    MapList* receiver_maps,
    CodeList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(a1, &miss);

  int receiver_count = receiver_maps->length();
  __ lw(a2, FieldMemOperand(a1, HeapObject::kMapOffset));
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map(receiver_maps->at(current));
    Handle<Code> code(handler_ics->at(current));
    __ Jump(code, RelocInfo::CODE_TARGET, eq, a2, Operand(map));
  }

  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedLoadIC_Miss();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                       int index,
                                                       Map* transition,
                                                       String* name) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  // -----------------------------------

  Label miss;

  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->keyed_store_field(), 1, a3, t0);

  // Check that the name has not changed.
  __ Branch(&miss, ne, a1, Operand(Handle<String>(name)));

  // a3 is used as scratch register. a1 and a2 keep their values if a jump to
  // the miss label is generated.
  GenerateStoreField(masm(),
                     object,
                     index,
                     transition,
                     a2, a1, a3,
                     &miss);
  __ bind(&miss);

  __ DecrementCounter(counters->keyed_store_field(), 1, a3, t0);
  Handle<Code> ic = masm()->isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreElement(Map* receiver_map) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : scratch
  // -----------------------------------
  Code* stub;
  ElementsKind elements_kind = receiver_map->elements_kind();
  bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
  MaybeObject* maybe_stub =
      KeyedStoreElementStub(is_js_array, elements_kind).TryGetCode();
  if (!maybe_stub->To(&stub)) return maybe_stub;
  __ DispatchMap(a2,
                 a3,
                 Handle<Map>(receiver_map),
                 Handle<Code>(stub),
                 DO_SMI_CHECK);

  Handle<Code> ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL);
}


MaybeObject* KeyedStoreStubCompiler::CompileStoreMegamorphic(
    MapList* receiver_maps,
    CodeList* handler_ics) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : scratch
  // -----------------------------------
  Label miss;
  __ JumpIfSmi(a2, &miss);

  int receiver_count = receiver_maps->length();
  __ lw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map(receiver_maps->at(current));
    Handle<Code> code(handler_ics->at(current));
    __ Jump(code, RelocInfo::CODE_TARGET, eq, a3, Operand(map));
  }

  __ bind(&miss);
  Handle<Code> miss_ic = isolate()->builtins()->KeyedStoreIC_Miss();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, NULL, MEGAMORPHIC);
}


MaybeObject* ConstructStubCompiler::CompileConstructStub(JSFunction* function) {
  // a0    : argc
  // a1    : constructor
  // ra    : return address
  // [sp]  : last argument
  Label generic_stub_call;

  // Use t7 for holding undefined which is used in several places below.
  __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Check to see whether there are any break points in the function code. If
  // there are jump to the generic constructor stub which calls the actual
  // code for the function thereby hitting the break points.
  __ lw(t5, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2, FieldMemOperand(t5, SharedFunctionInfo::kDebugInfoOffset));
  __ Branch(&generic_stub_call, ne, a2, Operand(t7));
#endif

  // Load the initial map and verify that it is in fact a map.
  // a1: constructor function
  // t7: undefined
  __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
  __ And(t0, a2, Operand(kSmiTagMask));
  __ Branch(&generic_stub_call, eq, t0, Operand(zero_reg));
  __ GetObjectType(a2, a3, t0);
  __ Branch(&generic_stub_call, ne, t0, Operand(MAP_TYPE));

#ifdef DEBUG
  // Cannot construct functions this way.
  // a0: argc
  // a1: constructor function
  // a2: initial map
  // t7: undefined
  __ lbu(a3, FieldMemOperand(a2, Map::kInstanceTypeOffset));
  __ Check(ne, "Function constructed by construct stub.",
      a3, Operand(JS_FUNCTION_TYPE));
#endif

  // Now allocate the JSObject in new space.
  // a0: argc
  // a1: constructor function
  // a2: initial map
  // t7: undefined
  __ lbu(a3, FieldMemOperand(a2, Map::kInstanceSizeOffset));
  __ AllocateInNewSpace(a3,
                        t4,
                        t5,
                        t6,
                        &generic_stub_call,
                        SIZE_IN_WORDS);

  // Allocated the JSObject, now initialize the fields. Map is set to initial
  // map and properties and elements are set to empty fixed array.
  // a0: argc
  // a1: constructor function
  // a2: initial map
  // a3: object size (in words)
  // t4: JSObject (not tagged)
  // t7: undefined
  __ LoadRoot(t6, Heap::kEmptyFixedArrayRootIndex);
  __ mov(t5, t4);
  __ sw(a2, MemOperand(t5, JSObject::kMapOffset));
  __ sw(t6, MemOperand(t5, JSObject::kPropertiesOffset));
  __ sw(t6, MemOperand(t5, JSObject::kElementsOffset));
  __ Addu(t5, t5, Operand(3 * kPointerSize));
  ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
  ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
  ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);


  // Calculate the location of the first argument. The stack contains only the
  // argc arguments.
  __ sll(a1, a0, kPointerSizeLog2);
  __ Addu(a1, a1, sp);

  // Fill all the in-object properties with undefined.
  // a0: argc
  // a1: first argument
  // a3: object size (in words)
  // t4: JSObject (not tagged)
  // t5: First in-object property of JSObject (not tagged)
  // t7: undefined
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
  SharedFunctionInfo* shared = function->shared();
  for (int i = 0; i < shared->this_property_assignments_count(); i++) {
    if (shared->IsThisPropertyAssignmentArgument(i)) {
      Label not_passed, next;
      // Check if the argument assigned to the property is actually passed.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ Branch(&not_passed, less_equal, a0, Operand(arg_number));
      // Argument passed - find it on the stack.
      __ lw(a2, MemOperand(a1, (arg_number + 1) * -kPointerSize));
      __ sw(a2, MemOperand(t5));
      __ Addu(t5, t5, kPointerSize);
      __ jmp(&next);
      __ bind(&not_passed);
      // Set the property to undefined.
      __ sw(t7, MemOperand(t5));
      __ Addu(t5, t5, Operand(kPointerSize));
      __ bind(&next);
    } else {
      // Set the property to the constant value.
      Handle<Object> constant(shared->GetThisPropertyAssignmentConstant(i));
      __ li(a2, Operand(constant));
      __ sw(a2, MemOperand(t5));
      __ Addu(t5, t5, kPointerSize);
    }
  }

  // Fill the unused in-object property fields with undefined.
  ASSERT(function->has_initial_map());
  for (int i = shared->this_property_assignments_count();
       i < function->initial_map()->inobject_properties();
       i++) {
      __ sw(t7, MemOperand(t5));
      __ Addu(t5, t5, kPointerSize);
  }

  // a0: argc
  // t4: JSObject (not tagged)
  // Move argc to a1 and the JSObject to return to v0 and tag it.
  __ mov(a1, a0);
  __ mov(v0, t4);
  __ Or(v0, v0, Operand(kHeapObjectTag));

  // v0: JSObject
  // a1: argc
  // Remove caller arguments and receiver from the stack and return.
  __ sll(t0, a1, kPointerSizeLog2);
  __ Addu(sp, sp, t0);
  __ Addu(sp, sp, Operand(kPointerSize));
  Counters* counters = masm()->isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1, a1, a2);
  __ IncrementCounter(counters->constructed_objects_stub(), 1, a1, a2);
  __ Ret();

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Handle<Code> generic_construct_stub =
      masm()->isolate()->builtins()->JSConstructStubGeneric();
  __ Jump(generic_construct_stub, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  Label slow, miss_force_generic;

  Register key = a0;
  Register receiver = a1;

  __ JumpIfNotSmi(key, &miss_force_generic);
  __ lw(t0, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ sra(a2, a0, kSmiTagSize);
  __ LoadFromNumberDictionary(&slow, t0, a0, v0, a2, a3, t1);
  __ Ret();

  // Slow case, key and receiver still in a0 and a1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, a2, a3);
  // Entry registers are intact.
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedLoadIC_Slow();
  __ Jump(slow_ic, RelocInfo::CODE_TARGET);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
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
    case FAST_DOUBLE_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      return false;
  }
  return false;
}


void KeyedLoadStubCompiler::GenerateLoadExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  Label miss_force_generic, slow, failed_allocation;

  Register key = a0;
  Register receiver = a1;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &miss_force_generic);

  __ lw(a3, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // a3: elements array

  // Check that the index is in range.
  __ lw(t1, FieldMemOperand(a3, ExternalArray::kLengthOffset));
  __ sra(t2, key, kSmiTagSize);
  // Unsigned comparison catches both negative and too-large values.
  __ Branch(&miss_force_generic, Ugreater_equal, key, Operand(t1));

  __ lw(a3, FieldMemOperand(a3, ExternalArray::kExternalPointerOffset));
  // a3: base pointer of external storage

  // We are not untagging smi key and instead work with it
  // as if it was premultiplied by 2.
  STATIC_ASSERT((kSmiTag == 0) && (kSmiTagSize == 1));

  Register value = a2;
  switch (elements_kind) {
    case EXTERNAL_BYTE_ELEMENTS:
      __ srl(t2, key, 1);
      __ addu(t3, a3, t2);
      __ lb(value, MemOperand(t3, 0));
      break;
    case EXTERNAL_PIXEL_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ srl(t2, key, 1);
      __ addu(t3, a3, t2);
      __ lbu(value, MemOperand(t3, 0));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
      __ addu(t3, a3, key);
      __ lh(value, MemOperand(t3, 0));
      break;
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ addu(t3, a3, key);
      __ lhu(value, MemOperand(t3, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ sll(t2, key, 1);
      __ addu(t3, a3, t2);
      __ lw(value, MemOperand(t3, 0));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      __ sll(t3, t2, 2);
      __ addu(t3, a3, t3);
      if (CpuFeatures::IsSupported(FPU)) {
        CpuFeatures::Scope scope(FPU);
        __ lwc1(f0, MemOperand(t3, 0));
      } else {
        __ lw(value, MemOperand(t3, 0));
      }
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ sll(t2, key, 2);
      __ addu(t3, a3, t2);
      if (CpuFeatures::IsSupported(FPU)) {
        CpuFeatures::Scope scope(FPU);
        __ ldc1(f0, MemOperand(t3, 0));
      } else {
        // t3: pointer to the beginning of the double we want to load.
        __ lw(a2, MemOperand(t3, 0));
        __ lw(a3, MemOperand(t3, Register::kSizeInBytes));
      }
      break;
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }

  // For integer array types:
  // a2: value
  // For float array type:
  // f0: value (if FPU is supported)
  // a2: value (if FPU is not supported)
  // For double array type:
  // f0: value (if FPU is supported)
  // a2/a3: value (if FPU is not supported)

  if (elements_kind == EXTERNAL_INT_ELEMENTS) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    __ Subu(t3, value, Operand(0xC0000000));  // Non-smi value gives neg result.
    __ Branch(&box_int, lt, t3, Operand(zero_reg));
    // Tag integer as smi and return it.
    __ sll(v0, value, kSmiTagSize);
    __ Ret();

    __ bind(&box_int);
    // Allocate a HeapNumber for the result and perform int-to-double
    // conversion.
    // The arm version uses a temporary here to save r0, but we don't need to
    // (a0 is not modified).
    __ LoadRoot(t1, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(v0, a3, t0, t1, &slow);

    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      __ mtc1(value, f0);
      __ cvt_d_w(f0, f0);
      __ sdc1(f0, MemOperand(v0, HeapNumber::kValueOffset - kHeapObjectTag));
      __ Ret();
    } else {
      Register dst1 = t2;
      Register dst2 = t3;
      FloatingPointHelper::Destination dest =
          FloatingPointHelper::kCoreRegisters;
      FloatingPointHelper::ConvertIntToDouble(masm,
                                              value,
                                              dest,
                                              f0,
                                              dst1,
                                              dst2,
                                              t1,
                                              f2);
      __ sw(dst1, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
      __ sw(dst2, FieldMemOperand(v0, HeapNumber::kExponentOffset));
      __ Ret();
    }
  } else if (elements_kind == EXTERNAL_UNSIGNED_INT_ELEMENTS) {
    // The test is different for unsigned int values. Since we need
    // the value to be in the range of a positive smi, we can't
    // handle either of the top two bits being set in the value.
    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      Label pl_box_int;
      __ And(t2, value, Operand(0xC0000000));
      __ Branch(&pl_box_int, ne, t2, Operand(zero_reg));

      // It can fit in an Smi.
      // Tag integer as smi and return it.
      __ sll(v0, value, kSmiTagSize);
      __ Ret();

      __ bind(&pl_box_int);
      // Allocate a HeapNumber for the result and perform int-to-double
      // conversion. Don't use a0 and a1 as AllocateHeapNumber clobbers all
      // registers - also when jumping due to exhausted young space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(v0, t2, t3, t6, &slow);

      // This is replaced by a macro:
      // __ mtc1(value, f0);     // LS 32-bits.
      // __ mtc1(zero_reg, f1);  // MS 32-bits are all zero.
      // __ cvt_d_l(f0, f0); // Use 64 bit conv to get correct unsigned 32-bit.

      __ Cvt_d_uw(f0, value, f22);

      __ sdc1(f0, MemOperand(v0, HeapNumber::kValueOffset - kHeapObjectTag));

      __ Ret();
    } else {
      // Check whether unsigned integer fits into smi.
      Label box_int_0, box_int_1, done;
      __ And(t2, value, Operand(0x80000000));
      __ Branch(&box_int_0, ne, t2, Operand(zero_reg));
      __ And(t2, value, Operand(0x40000000));
      __ Branch(&box_int_1, ne, t2, Operand(zero_reg));

      // Tag integer as smi and return it.
      __ sll(v0, value, kSmiTagSize);
      __ Ret();

      Register hiword = value;  // a2.
      Register loword = a3;

      __ bind(&box_int_0);
      // Integer does not have leading zeros.
      GenerateUInt2Double(masm, hiword, loword, t0, 0);
      __ Branch(&done);

      __ bind(&box_int_1);
      // Integer has one leading zero.
      GenerateUInt2Double(masm, hiword, loword, t0, 1);


      __ bind(&done);
      // Integer was converted to double in registers hiword:loword.
      // Wrap it into a HeapNumber. Don't use a0 and a1 as AllocateHeapNumber
      // clobbers all registers - also when jumping due to exhausted young
      // space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(t2, t3, t5, t6, &slow);

      __ sw(hiword, FieldMemOperand(t2, HeapNumber::kExponentOffset));
      __ sw(loword, FieldMemOperand(t2, HeapNumber::kMantissaOffset));

      __ mov(v0, t2);
      __ Ret();
    }
  } else if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      // Allocate a HeapNumber for the result. Don't use a0 and a1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(v0, t3, t5, t6, &slow);
      // The float (single) value is already in fpu reg f0 (if we use float).
      __ cvt_d_s(f0, f0);
      __ sdc1(f0, MemOperand(v0, HeapNumber::kValueOffset - kHeapObjectTag));
      __ Ret();
    } else {
      // Allocate a HeapNumber for the result. Don't use a0 and a1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(v0, t3, t5, t6, &slow);
      // FPU is not available, do manual single to double conversion.

      // a2: floating point value (binary32).
      // v0: heap number for result

      // Extract mantissa to t4.
      __ And(t4, value, Operand(kBinary32MantissaMask));

      // Extract exponent to t5.
      __ srl(t5, value, kBinary32MantissaBits);
      __ And(t5, t5, Operand(kBinary32ExponentMask >> kBinary32MantissaBits));

      Label exponent_rebiased;
      __ Branch(&exponent_rebiased, eq, t5, Operand(zero_reg));

      __ li(t0, 0x7ff);
      __ Xor(t1, t5, Operand(0xFF));
      __ movz(t5, t0, t1);  // Set t5 to 0x7ff only if t5 is equal to 0xff.
      __ Branch(&exponent_rebiased, eq, t0, Operand(0xff));

      // Rebias exponent.
      __ Addu(t5,
              t5,
              Operand(-kBinary32ExponentBias + HeapNumber::kExponentBias));

      __ bind(&exponent_rebiased);
      __ And(a2, value, Operand(kBinary32SignMask));
      value = no_reg;
      __ sll(t0, t5, HeapNumber::kMantissaBitsInTopWord);
      __ or_(a2, a2, t0);

      // Shift mantissa.
      static const int kMantissaShiftForHiWord =
          kBinary32MantissaBits - HeapNumber::kMantissaBitsInTopWord;

      static const int kMantissaShiftForLoWord =
          kBitsPerInt - kMantissaShiftForHiWord;

      __ srl(t0, t4, kMantissaShiftForHiWord);
      __ or_(a2, a2, t0);
      __ sll(a0, t4, kMantissaShiftForLoWord);

      __ sw(a2, FieldMemOperand(v0, HeapNumber::kExponentOffset));
      __ sw(a0, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
      __ Ret();
    }

  } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      // Allocate a HeapNumber for the result. Don't use a0 and a1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(v0, t3, t5, t6, &slow);
      // The double value is already in f0
      __ sdc1(f0, FieldMemOperand(v0, HeapNumber::kValueOffset));
      __ Ret();
    } else {
      // Allocate a HeapNumber for the result. Don't use a0 and a1 as
      // AllocateHeapNumber clobbers all registers - also when jumping due to
      // exhausted young space.
      __ LoadRoot(t6, Heap::kHeapNumberMapRootIndex);
      __ AllocateHeapNumber(v0, t3, t5, t6, &slow);

      __ sw(a2, FieldMemOperand(v0, HeapNumber::kMantissaOffset));
      __ sw(a3, FieldMemOperand(v0, HeapNumber::kExponentOffset));
      __ Ret();
    }

  } else {
    // Tag integer as smi and return it.
    __ sll(v0, value, kSmiTagSize);
    __ Ret();
  }

  // Slow case, key and receiver still in a0 and a1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, a2, a3);

  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------

  __ Push(a1, a0);

  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);

  __ bind(&miss_force_generic);
  Code* stub = masm->isolate()->builtins()->builtin(
      Builtins::kKeyedLoadIC_MissForceGeneric);
  __ Jump(Handle<Code>(stub), RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ---------- S t a t e --------------
  //  -- a0     : value
  //  -- a1     : key
  //  -- a2     : receiver
  //  -- ra     : return address
  // -----------------------------------

  Label slow, check_heap_number, miss_force_generic;

  // Register usage.
  Register value = a0;
  Register key = a1;
  Register receiver = a2;
  // a3 mostly holds the elements array or the destination external array.

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

    // Check that the key is a smi.
  __ JumpIfNotSmi(key, &miss_force_generic);

  __ lw(a3, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check that the index is in range.
  __ SmiUntag(t0, key);
  __ lw(t1, FieldMemOperand(a3, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ Branch(&miss_force_generic, Ugreater_equal, key, Operand(t1));

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // a3: external array.
  // t0: key (integer).

  if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
    // Double to pixel conversion is only implemented in the runtime for now.
    __ JumpIfNotSmi(value, &slow);
  } else {
    __ JumpIfNotSmi(value, &check_heap_number);
  }
  __ SmiUntag(t1, value);
  __ lw(a3, FieldMemOperand(a3, ExternalArray::kExternalPointerOffset));

  // a3: base pointer of external storage.
  // t0: key (integer).
  // t1: value (integer).

  switch (elements_kind) {
    case EXTERNAL_PIXEL_ELEMENTS: {
      // Clamp the value to [0..255].
      // v0 is used as a scratch register here.
      Label done;
      __ li(v0, Operand(255));
      // Normal branch: nop in delay slot.
      __ Branch(&done, gt, t1, Operand(v0));
      // Use delay slot in this branch.
      __ Branch(USE_DELAY_SLOT, &done, lt, t1, Operand(zero_reg));
      __ mov(v0, zero_reg);  // In delay slot.
      __ mov(v0, t1);  // Value is in range 0..255.
      __ bind(&done);
      __ mov(t1, v0);
      __ addu(t8, a3, t0);
      __ sb(t1, MemOperand(t8, 0));
      }
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ addu(t8, a3, t0);
      __ sb(t1, MemOperand(t8, 0));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ sll(t8, t0, 1);
      __ addu(t8, a3, t8);
      __ sh(t1, MemOperand(t8, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ sll(t8, t0, 2);
      __ addu(t8, a3, t8);
      __ sw(t1, MemOperand(t8, 0));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      // Perform int-to-float conversion and store to memory.
      StoreIntAsFloat(masm, a3, t0, t1, t2, t3, t4);
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ sll(t8, t0, 3);
      __ addu(a3, a3, t8);
      // a3: effective address of the double element
      FloatingPointHelper::Destination destination;
      if (CpuFeatures::IsSupported(FPU)) {
        destination = FloatingPointHelper::kFPURegisters;
      } else {
        destination = FloatingPointHelper::kCoreRegisters;
      }
      FloatingPointHelper::ConvertIntToDouble(
          masm, t1, destination,
          f0, t2, t3,  // These are: double_dst, dst1, dst2.
          t0, f2);  // These are: scratch2, single_scratch.
      if (destination == FloatingPointHelper::kFPURegisters) {
        CpuFeatures::Scope scope(FPU);
        __ sdc1(f0, MemOperand(a3, 0));
      } else {
        __ sw(t2, MemOperand(a3, 0));
        __ sw(t3, MemOperand(a3, Register::kSizeInBytes));
      }
      break;
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }

  // Entry registers are intact, a0 holds the value which is the return value.
  __ mov(v0, value);
  __ Ret();

  if (elements_kind != EXTERNAL_PIXEL_ELEMENTS) {
    // a3: external array.
    // t0: index (integer).
    __ bind(&check_heap_number);
    __ GetObjectType(value, t1, t2);
    __ Branch(&slow, ne, t2, Operand(HEAP_NUMBER_TYPE));

    __ lw(a3, FieldMemOperand(a3, ExternalArray::kExternalPointerOffset));

    // a3: base pointer of external storage.
    // t0: key (integer).

    // The WebGL specification leaves the behavior of storing NaN and
    // +/-Infinity into integer arrays basically undefined. For more
    // reproducible behavior, convert these to zero.

    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);

      __ ldc1(f0, FieldMemOperand(a0, HeapNumber::kValueOffset));

      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
        __ cvt_s_d(f0, f0);
        __ sll(t8, t0, 2);
        __ addu(t8, a3, t8);
        __ swc1(f0, MemOperand(t8, 0));
      } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
        __ sll(t8, t0, 3);
        __ addu(t8, a3, t8);
        __ sdc1(f0, MemOperand(t8, 0));
      } else {
        __ EmitECMATruncate(t3, f0, f2, t2, t1, t5);

        switch (elements_kind) {
          case EXTERNAL_BYTE_ELEMENTS:
          case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
            __ addu(t8, a3, t0);
            __ sb(t3, MemOperand(t8, 0));
            break;
          case EXTERNAL_SHORT_ELEMENTS:
          case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
            __ sll(t8, t0, 1);
            __ addu(t8, a3, t8);
            __ sh(t3, MemOperand(t8, 0));
            break;
          case EXTERNAL_INT_ELEMENTS:
          case EXTERNAL_UNSIGNED_INT_ELEMENTS:
            __ sll(t8, t0, 2);
            __ addu(t8, a3, t8);
            __ sw(t3, MemOperand(t8, 0));
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
      }

      // Entry registers are intact, a0 holds the value
      // which is the return value.
      __ mov(v0, value);
      __ Ret();
    } else {
      // FPU is not available, do manual conversions.

      __ lw(t3, FieldMemOperand(value, HeapNumber::kExponentOffset));
      __ lw(t4, FieldMemOperand(value, HeapNumber::kMantissaOffset));

      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
        Label done, nan_or_infinity_or_zero;
        static const int kMantissaInHiWordShift =
            kBinary32MantissaBits - HeapNumber::kMantissaBitsInTopWord;

        static const int kMantissaInLoWordShift =
            kBitsPerInt - kMantissaInHiWordShift;

        // Test for all special exponent values: zeros, subnormal numbers, NaNs
        // and infinities. All these should be converted to 0.
        __ li(t5, HeapNumber::kExponentMask);
        __ and_(t6, t3, t5);
        __ Branch(&nan_or_infinity_or_zero, eq, t6, Operand(zero_reg));

        __ xor_(t1, t6, t5);
        __ li(t2, kBinary32ExponentMask);
        __ movz(t6, t2, t1);  // Only if t6 is equal to t5.
        __ Branch(&nan_or_infinity_or_zero, eq, t6, Operand(t5));

        // Rebias exponent.
        __ srl(t6, t6, HeapNumber::kExponentShift);
        __ Addu(t6,
                t6,
                Operand(kBinary32ExponentBias - HeapNumber::kExponentBias));

        __ li(t1, Operand(kBinary32MaxExponent));
        __ Slt(t1, t1, t6);
        __ And(t2, t3, Operand(HeapNumber::kSignMask));
        __ Or(t2, t2, Operand(kBinary32ExponentMask));
        __ movn(t3, t2, t1);  // Only if t6 is gt kBinary32MaxExponent.
        __ Branch(&done, gt, t6, Operand(kBinary32MaxExponent));

        __ Slt(t1, t6, Operand(kBinary32MinExponent));
        __ And(t2, t3, Operand(HeapNumber::kSignMask));
        __ movn(t3, t2, t1);  // Only if t6 is lt kBinary32MinExponent.
        __ Branch(&done, lt, t6, Operand(kBinary32MinExponent));

        __ And(t7, t3, Operand(HeapNumber::kSignMask));
        __ And(t3, t3, Operand(HeapNumber::kMantissaMask));
        __ sll(t3, t3, kMantissaInHiWordShift);
        __ or_(t7, t7, t3);
        __ srl(t4, t4, kMantissaInLoWordShift);
        __ or_(t7, t7, t4);
        __ sll(t6, t6, kBinary32ExponentShift);
        __ or_(t3, t7, t6);

        __ bind(&done);
        __ sll(t9, a1, 2);
        __ addu(t9, a2, t9);
        __ sw(t3, MemOperand(t9, 0));

        // Entry registers are intact, a0 holds the value which is the return
        // value.
        __ mov(v0, value);
        __ Ret();

        __ bind(&nan_or_infinity_or_zero);
        __ And(t7, t3, Operand(HeapNumber::kSignMask));
        __ And(t3, t3, Operand(HeapNumber::kMantissaMask));
        __ or_(t6, t6, t7);
        __ sll(t3, t3, kMantissaInHiWordShift);
        __ or_(t6, t6, t3);
        __ srl(t4, t4, kMantissaInLoWordShift);
        __ or_(t3, t6, t4);
        __ Branch(&done);
      } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
        __ sll(t8, t0, 3);
        __ addu(t8, a3, t8);
        // t8: effective address of destination element.
        __ sw(t4, MemOperand(t8, 0));
        __ sw(t3, MemOperand(t8, Register::kSizeInBytes));
        __ Ret();
      } else {
        bool is_signed_type = IsElementTypeSigned(elements_kind);
        int meaningfull_bits = is_signed_type ? (kBitsPerInt - 1) : kBitsPerInt;
        int32_t min_value    = is_signed_type ? 0x80000000 : 0x00000000;

        Label done, sign;

        // Test for all special exponent values: zeros, subnormal numbers, NaNs
        // and infinities. All these should be converted to 0.
        __ li(t5, HeapNumber::kExponentMask);
        __ and_(t6, t3, t5);
        __ movz(t3, zero_reg, t6);  // Only if t6 is equal to zero.
        __ Branch(&done, eq, t6, Operand(zero_reg));

        __ xor_(t2, t6, t5);
        __ movz(t3, zero_reg, t2);  // Only if t6 is equal to t5.
        __ Branch(&done, eq, t6, Operand(t5));

        // Unbias exponent.
        __ srl(t6, t6, HeapNumber::kExponentShift);
        __ Subu(t6, t6, Operand(HeapNumber::kExponentBias));
        // If exponent is negative then result is 0.
        __ slt(t2, t6, zero_reg);
        __ movn(t3, zero_reg, t2);  // Only if exponent is negative.
        __ Branch(&done, lt, t6, Operand(zero_reg));

        // If exponent is too big then result is minimal value.
        __ slti(t1, t6, meaningfull_bits - 1);
        __ li(t2, min_value);
        __ movz(t3, t2, t1);  // Only if t6 is ge meaningfull_bits - 1.
        __ Branch(&done, ge, t6, Operand(meaningfull_bits - 1));

        __ And(t5, t3, Operand(HeapNumber::kSignMask));
        __ And(t3, t3, Operand(HeapNumber::kMantissaMask));
        __ Or(t3, t3, Operand(1u << HeapNumber::kMantissaBitsInTopWord));

        __ li(t9, HeapNumber::kMantissaBitsInTopWord);
        __ subu(t6, t9, t6);
        __ slt(t1, t6, zero_reg);
        __ srlv(t2, t3, t6);
        __ movz(t3, t2, t1);  // Only if t6 is positive.
        __ Branch(&sign, ge, t6, Operand(zero_reg));

        __ subu(t6, zero_reg, t6);
        __ sllv(t3, t3, t6);
        __ li(t9, meaningfull_bits);
        __ subu(t6, t9, t6);
        __ srlv(t4, t4, t6);
        __ or_(t3, t3, t4);

        __ bind(&sign);
        __ subu(t2, t3, zero_reg);
        __ movz(t3, t2, t5);  // Only if t5 is zero.

        __ bind(&done);

        // Result is in t3.
        // This switch block should be exactly the same as above (FPU mode).
        switch (elements_kind) {
          case EXTERNAL_BYTE_ELEMENTS:
          case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
            __ addu(t8, a3, t0);
            __ sb(t3, MemOperand(t8, 0));
            break;
          case EXTERNAL_SHORT_ELEMENTS:
          case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
            __ sll(t8, t0, 1);
            __ addu(t8, a3, t8);
            __ sh(t3, MemOperand(t8, 0));
            break;
          case EXTERNAL_INT_ELEMENTS:
          case EXTERNAL_UNSIGNED_INT_ELEMENTS:
            __ sll(t8, t0, 2);
            __ addu(t8, a3, t8);
            __ sw(t3, MemOperand(t8, 0));
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
      }
    }
  }

  // Slow case, key and receiver still in a0 and a1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, a2, a3);
  // Entry registers are intact.
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  Handle<Code> slow_ic =
      masm->isolate()->builtins()->KeyedStoreIC_Slow();
  __ Jump(slow_ic, RelocInfo::CODE_TARGET);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------

  Handle<Code> miss_ic =
     masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(miss_ic, RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastElement(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss_force_generic;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(a0, &miss_force_generic);

  // Get the elements array.
  __ lw(a2, FieldMemOperand(a1, JSObject::kElementsOffset));
  __ AssertFastElements(a2);

  // Check that the key is within bounds.
  __ lw(a3, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ Branch(&miss_force_generic, hs, a0, Operand(a3));

  // Load the result and make sure it's not the hole.
  __ Addu(a3, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ sll(t0, a0, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(t0, t0, a3);
  __ lw(t0, MemOperand(t0));
  __ LoadRoot(t1, Heap::kTheHoleValueRootIndex);
  __ Branch(&miss_force_generic, eq, t0, Operand(t1));
  __ mov(v0, t0);
  __ Ret();

  __ bind(&miss_force_generic);
  Code* stub = masm->isolate()->builtins()->builtin(
      Builtins::kKeyedLoadIC_MissForceGeneric);
  __ Jump(Handle<Code>(stub), RelocInfo::CODE_TARGET);
}


void KeyedLoadStubCompiler::GenerateLoadFastDoubleElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  //  -- a0    : key
  //  -- a1    : receiver
  // -----------------------------------
  Label miss_force_generic, slow_allocate_heapnumber;

  Register key_reg = a0;
  Register receiver_reg = a1;
  Register elements_reg = a2;
  Register heap_number_reg = a2;
  Register indexed_double_offset = a3;
  Register scratch = t0;
  Register scratch2 = t1;
  Register scratch3 = t2;
  Register heap_number_map = t3;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key_reg, &miss_force_generic);

  // Get the elements array.
  __ lw(elements_reg,
        FieldMemOperand(receiver_reg, JSObject::kElementsOffset));

  // Check that the key is within bounds.
  __ lw(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  __ Branch(&miss_force_generic, hs, key_reg, Operand(scratch));

  // Load the upper word of the double in the fixed array and test for NaN.
  __ sll(scratch2, key_reg, kDoubleSizeLog2 - kSmiTagSize);
  __ Addu(indexed_double_offset, elements_reg, Operand(scratch2));
  uint32_t upper_32_offset = FixedArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ lw(scratch, FieldMemOperand(indexed_double_offset, upper_32_offset));
  __ Branch(&miss_force_generic, eq, scratch, Operand(kHoleNanUpper32));

  // Non-NaN. Allocate a new heap number and copy the double value into it.
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(heap_number_reg, scratch2, scratch3,
                        heap_number_map, &slow_allocate_heapnumber);

  // Don't need to reload the upper 32 bits of the double, it's already in
  // scratch.
  __ sw(scratch, FieldMemOperand(heap_number_reg,
                                 HeapNumber::kExponentOffset));
  __ lw(scratch, FieldMemOperand(indexed_double_offset,
                                 FixedArray::kHeaderSize));
  __ sw(scratch, FieldMemOperand(heap_number_reg,
                                 HeapNumber::kMantissaOffset));

  __ mov(v0, heap_number_reg);
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


void KeyedStoreStubCompiler::GenerateStoreFastElement(MacroAssembler* masm,
                                                      bool is_js_array) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : scratch
  //  -- a4    : scratch (elements)
  // -----------------------------------
  Label miss_force_generic;

  Register value_reg = a0;
  Register key_reg = a1;
  Register receiver_reg = a2;
  Register scratch = a3;
  Register elements_reg = t0;
  Register scratch2 = t1;
  Register scratch3 = t2;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key_reg, &miss_force_generic);

  // Get the elements array and make sure it is a fast element array, not 'cow'.
  __ lw(elements_reg,
        FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
  __ CheckMap(elements_reg,
              scratch,
              Heap::kFixedArrayMapRootIndex,
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  // Check that the key is within bounds.
  if (is_js_array) {
    __ lw(scratch, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ lw(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis.
  __ Branch(&miss_force_generic, hs, key_reg, Operand(scratch));

  __ Addu(scratch,
          elements_reg, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ sll(scratch2, key_reg, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(scratch3, scratch2, scratch);
  __ sw(value_reg, MemOperand(scratch3));
  __ RecordWrite(scratch, Operand(scratch2), receiver_reg , elements_reg);

  // value_reg (a0) is preserved.
  // Done.
  __ Ret();

  __ bind(&miss_force_generic);
  Handle<Code> ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(ic, RelocInfo::CODE_TARGET);
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : scratch
  //  -- t0    : scratch (elements_reg)
  //  -- t1    : scratch (mantissa_reg)
  //  -- t2    : scratch (exponent_reg)
  //  -- t3    : scratch4
  // -----------------------------------
  Label miss_force_generic, smi_value, is_nan, maybe_nan, have_double_value;

  Register value_reg = a0;
  Register key_reg = a1;
  Register receiver_reg = a2;
  Register scratch = a3;
  Register elements_reg = t0;
  Register mantissa_reg = t1;
  Register exponent_reg = t2;
  Register scratch4 = t3;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.
  __ JumpIfNotSmi(key_reg, &miss_force_generic);

  __ lw(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));

  // Check that the key is within bounds.
  if (is_js_array) {
    __ lw(scratch, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ lw(scratch,
          FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis, unsigned compare catches both negative and out-of-bound
  // indexes.
  __ Branch(&miss_force_generic, hs, key_reg, Operand(scratch));

  // Handle smi values specially.
  __ JumpIfSmi(value_reg, &smi_value);

  // Ensure that the object is a heap number
  __ CheckMap(value_reg,
              scratch,
              masm->isolate()->factory()->heap_number_map(),
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  // Check for nan: all NaN values have a value greater (signed) than 0x7ff00000
  // in the exponent.
  __ li(scratch, Operand(kNaNOrInfinityLowerBoundUpper32));
  __ lw(exponent_reg, FieldMemOperand(value_reg, HeapNumber::kExponentOffset));
  __ Branch(&maybe_nan, ge, exponent_reg, Operand(scratch));

  __ lw(mantissa_reg, FieldMemOperand(value_reg, HeapNumber::kMantissaOffset));

  __ bind(&have_double_value);
  __ sll(scratch4, key_reg, kDoubleSizeLog2 - kSmiTagSize);
  __ Addu(scratch, elements_reg, Operand(scratch4));
  __ sw(mantissa_reg, FieldMemOperand(scratch, FixedDoubleArray::kHeaderSize));
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ sw(exponent_reg, FieldMemOperand(scratch, offset));
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, value_reg);  // In delay slot.

  __ bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  __ li(scratch, Operand(kNaNOrInfinityLowerBoundUpper32));
  __ Branch(&is_nan, gt, exponent_reg, Operand(scratch));
  __ lw(mantissa_reg, FieldMemOperand(value_reg, HeapNumber::kMantissaOffset));
  __ Branch(&have_double_value, eq, mantissa_reg, Operand(zero_reg));

  __ bind(&is_nan);
  // Load canonical NaN for storing into the double array.
  uint64_t nan_int64 = BitCast<uint64_t>(
      FixedDoubleArray::canonical_not_the_hole_nan_as_double());
  __ li(mantissa_reg, Operand(static_cast<uint32_t>(nan_int64)));
  __ li(exponent_reg, Operand(static_cast<uint32_t>(nan_int64 >> 32)));
  __ jmp(&have_double_value);

  __ bind(&smi_value);
  __ Addu(scratch, elements_reg,
          Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  __ sll(scratch4, key_reg, kDoubleSizeLog2 - kSmiTagSize);
  __ Addu(scratch, scratch, scratch4);
  // scratch is now effective address of the double element

  FloatingPointHelper::Destination destination;
  if (CpuFeatures::IsSupported(FPU)) {
    destination = FloatingPointHelper::kFPURegisters;
  } else {
    destination = FloatingPointHelper::kCoreRegisters;
  }

  Register untagged_value = receiver_reg;
  __ SmiUntag(untagged_value, value_reg);
  FloatingPointHelper::ConvertIntToDouble(
      masm,
      untagged_value,
      destination,
      f0,
      mantissa_reg,
      exponent_reg,
      scratch4,
      f2);
  if (destination == FloatingPointHelper::kFPURegisters) {
    CpuFeatures::Scope scope(FPU);
    __ sdc1(f0, MemOperand(scratch, 0));
  } else {
    __ sw(mantissa_reg, MemOperand(scratch, 0));
    __ sw(exponent_reg, MemOperand(scratch, Register::kSizeInBytes));
  }
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, value_reg);  // In delay slot.

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  Handle<Code> ic =
      masm->isolate()->builtins()->KeyedStoreIC_MissForceGeneric();
  __ Jump(ic, RelocInfo::CODE_TARGET);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
