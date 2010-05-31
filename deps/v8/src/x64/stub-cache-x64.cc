// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "codegen-inl.h"
#include "stub-cache.h"
#include "macro-assembler-x64.h"

namespace v8 {
namespace internal {

//-----------------------------------------------------------------------------
// StubCompiler static helper functions

#define __ ACCESS_MASM(masm)


static void ProbeTable(MacroAssembler* masm,
                       Code::Flags flags,
                       StubCache::Table table,
                       Register name,
                       Register offset) {
  ASSERT_EQ(8, kPointerSize);
  ASSERT_EQ(16, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(SCTableReference::keyReference(table));
  Label miss;

  __ movq(kScratchRegister, key_offset);
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


void StubCompiler::GenerateLoadMiss(MacroAssembler* masm, Code::Kind kind) {
  ASSERT(kind == Code::LOAD_IC || kind == Code::KEYED_LOAD_IC);
  Code* code = NULL;
  if (kind == Code::LOAD_IC) {
    code = Builtins::builtin(Builtins::LoadIC_Miss);
  } else {
    code = Builtins::builtin(Builtins::KeyedLoadIC_Miss);
  }

  Handle<Code> ic(code);
  __ Jump(ic, RelocInfo::CODE_TARGET);
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
  ASSERT(!Heap::InNewSpace(interceptor));
  __ Move(kScratchRegister, Handle<Object>(interceptor));
  __ push(kScratchRegister);
  __ push(receiver);
  __ push(holder);
  __ push(FieldOperand(kScratchRegister, InterceptorInfo::kDataOffset));
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra) {
  Label miss;
  USE(extra);  // The register extra is not used on the X64 platform.
  // Make sure that code is valid. The shifting code relies on the
  // entry size being 16.
  ASSERT(sizeof(Entry) == 16);

  // Make sure the flags do not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(masm, flags, kPrimary, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, String::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(flags));
  __ and_(scratch, Immediate((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(masm, flags, kSecondary, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
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
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage)), 3, 1);
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
  ASSERT(kNotStringTag != 0);
  __ testl(scratch, Immediate(kNotStringTag));
  __ j(not_zero, non_string_object);
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

  // Load length directly from the string.
  __ movq(rax, FieldOperand(receiver, String::kLengthOffset));
  __ ret(0);

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


static void CompileCallLoadPropertyWithInterceptor(MacroAssembler* masm,
                                                   Register receiver,
                                                   Register holder,
                                                   Register name,
                                                   JSObject* holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly));
  __ movq(rax, Immediate(5));
  __ movq(rbx, ref);

  CEntryStub stub(1);
  __ CallStub(&stub);
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


// Reserves space for the extra arguments to FastHandleApiCall in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument in the internal frame of the caller
  // -----------------------------------
  __ movq(scratch, Operand(rsp, 0));
  __ subq(rsp, Immediate(4 * kPointerSize));
  __ movq(Operand(rsp, 0), scratch);
  __ Move(scratch, Smi::FromInt(0));
  __ movq(Operand(rsp, 1 * kPointerSize), scratch);
  __ movq(Operand(rsp, 2 * kPointerSize), scratch);
  __ movq(Operand(rsp, 3 * kPointerSize), scratch);
  __ movq(Operand(rsp, 4 * kPointerSize), scratch);
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm, Register scratch) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : last fast api call extra argument
  //  -- ...
  //  -- rsp[32] : first fast api call extra argument
  //  -- rsp[40] : last argument in the internal frame
  // -----------------------------------
  __ movq(scratch, Operand(rsp, 0));
  __ movq(Operand(rsp, 4 * kPointerSize), scratch);
  __ addq(rsp, Immediate(kPointerSize * 4));
}


// Generates call to FastHandleApiCall builtin.
static void GenerateFastApiCall(MacroAssembler* masm,
                                const CallOptimization& optimization,
                                int argc) {
  // ----------- S t a t e -------------
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : object passing the type check
  //                           (last fast api call extra argument,
  //                            set by CheckPrototypes)
  //  -- rsp[16]             : api call data
  //  -- rsp[24]             : api callback
  //  -- rsp[32]             : api function
  //                           (first fast api call extra argument)
  //  -- rsp[40]             : last argument
  //  -- ...
  //  -- rsp[(argc + 5) * 8] : first argument
  //  -- rsp[(argc + 6) * 8] : receiver
  // -----------------------------------

  // Get the function and setup the context.
  JSFunction* function = optimization.constant_function();
  __ Move(rdi, Handle<JSFunction>(function));
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Pass the additional arguments FastHandleApiCall expects.
  __ movq(Operand(rsp, 4 * kPointerSize), rdi);
  bool info_loaded = false;
  Object* callback = optimization.api_call_info()->callback();
  if (Heap::InNewSpace(callback)) {
    info_loaded = true;
    __ Move(rcx, Handle<CallHandlerInfo>(optimization.api_call_info()));
    __ movq(rbx, FieldOperand(rcx, CallHandlerInfo::kCallbackOffset));
    __ movq(Operand(rsp, 3 * kPointerSize), rbx);
  } else {
    __ Move(Operand(rsp, 3 * kPointerSize), Handle<Object>(callback));
  }
  Object* call_data = optimization.api_call_info()->data();
  if (Heap::InNewSpace(call_data)) {
    if (!info_loaded) {
      __ Move(rcx, Handle<CallHandlerInfo>(optimization.api_call_info()));
    }
    __ movq(rbx, FieldOperand(rcx, CallHandlerInfo::kDataOffset));
    __ movq(Operand(rsp, 2 * kPointerSize), rbx);
  } else {
    __ Move(Operand(rsp, 2 * kPointerSize), Handle<Object>(call_data));
  }

  // Set the number of arguments.
  __ movq(rax, Immediate(argc + 4));

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
               Label* miss) {
    ASSERT(holder->HasNamedInterceptor());
    ASSERT(!holder->GetNamedInterceptor()->getter()->IsUndefined());

    // Check that the receiver isn't a smi.
    __ JumpIfSmi(receiver, miss);

    CallOptimization optimization(lookup);

    if (optimization.is_constant_call()) {
      CompileCacheable(masm,
                       object,
                       receiver,
                       scratch1,
                       scratch2,
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
                                        scratch2, name, depth1, miss);

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
                                      scratch2, name, depth2, miss);
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
                      String* name,
                      JSObject* interceptor_holder,
                      Label* miss_label) {
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, name,
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
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForCall)),
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
};


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
  __ Move(scratch, Handle<Object>(cell));
  __ Cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
         Factory::the_hole_value());
  __ j(not_equal, miss);
  return cell;
}


#undef __

#define __ ACCESS_MASM((masm()))

Object* CallStubCompiler::CompileCallConstant(Object* object,
                                              JSObject* holder,
                                              JSFunction* function,
                                              String* name,
                                              StubCompiler::CheckType check) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
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

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(rdx, &miss_in_smi_check);
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
        ReserveSpaceForFastApiCall(masm(), rax);
      }

      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), rdx, holder,
                      rbx, rax, name, depth, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        ASSERT(depth == kInvalidProtoDepth);
        __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
        __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
      }
      break;

    case STRING_CHECK:
      if (!function->IsBuiltin()) {
        // Calling non-builtins with a value as receiver requires boxing.
        __ jmp(&miss);
      } else {
        // Check that the object is a two-byte string or a symbol.
        __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, rax);
        __ j(above_equal, &miss);
        // Check that the maps starting from the prototype haven't changed.
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            rax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, name, &miss);
      }
      break;

    case NUMBER_CHECK: {
      if (!function->IsBuiltin()) {
        // Calling non-builtins with a value as receiver requires boxing.
        __ jmp(&miss);
      } else {
        Label fast;
        // Check that the object is a smi or a heap number.
        __ JumpIfSmi(rdx, &fast);
        __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rax);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::NUMBER_FUNCTION_INDEX,
                                            rax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, name, &miss);
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
        __ CompareRoot(rdx, Heap::kTrueValueRootIndex);
        __ j(equal, &fast);
        __ CompareRoot(rdx, Heap::kFalseValueRootIndex);
        __ j(not_equal, &miss);
        __ bind(&fast);
        // Check that the maps starting from the prototype haven't changed.
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::BOOLEAN_FUNCTION_INDEX,
                                            rax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), rax, holder,
                        rbx, rdx, name, &miss);
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
    FreeSpaceForFastApiCall(masm(), rax);
  }

  // Handle call cache miss.
  __ bind(&miss_in_smi_check);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileCallField(JSObject* object,
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

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, rdx, holder, rbx, rax, name, &miss);

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
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* CallStubCompiler::CompileArrayPushCall(Object* object,
                                               JSObject* holder,
                                               JSFunction* function,
                                               String* name,
                                               CheckType check) {
  // ----------- S t a t e -------------
  //  -- rcx                 : name
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- ...
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------
  ASSERT(check == RECEIVER_MAP_CHECK);

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray()) {
    return Heap::undefined_value();
  }

  Label miss;

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
                  name,
                  &miss);

  if (argc == 0) {
    // Noop, return the length.
    __ movq(rax, FieldOperand(rdx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
  } else {
    // Get the elements array of the object.
    __ movq(rbx, FieldOperand(rdx, JSArray::kElementsOffset));

    // Check that the elements are in fast mode (not dictionary).
    __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
           Factory::fixed_array_map());
    __ j(not_equal, &miss);

    if (argc == 1) {  // Otherwise fall through to call builtin.
      Label call_builtin, exit, with_write_barrier, attempt_to_grow_elements;

      // Get the array's length into rax and calculate new length.
      __ movq(rax, FieldOperand(rdx, JSArray::kLengthOffset));
      STATIC_ASSERT(FixedArray::kMaxLength < Smi::kMaxValue);
      __ SmiAddConstant(rax, rax, Smi::FromInt(argc));

      // Get the element's length into rcx.
      __ movq(rcx, FieldOperand(rbx, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ SmiCompare(rax, rcx);
      __ j(greater, &attempt_to_grow_elements);

      // Save new length.
      __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Push the element.
      __ movq(rcx, Operand(rsp, argc * kPointerSize));
      SmiIndex index =
          masm()->SmiToIndex(kScratchRegister, rax, times_pointer_size);
      __ lea(rdx, FieldOperand(rbx,
                               index.reg, index.scale,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ movq(Operand(rdx, 0), rcx);

      // Check if value is a smi.
      __ JumpIfNotSmi(rcx, &with_write_barrier);

      __ bind(&exit);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&with_write_barrier);

      __ InNewSpace(rbx, rcx, equal, &exit);

      RecordWriteStub stub(rbx, rdx, rcx);
      __ CallStub(&stub);
      __ ret((argc + 1) * kPointerSize);

      __ bind(&attempt_to_grow_elements);
      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address();
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address();

      const int kAllocationDelta = 4;
      // Load top.
      __ movq(rcx, new_space_allocation_top);
      __ movq(rcx, Operand(rcx, 0));

      // Check if it's the end of elements.
      index = masm()->SmiToIndex(kScratchRegister, rax, times_pointer_size);
      __ lea(rdx, FieldOperand(rbx,
                               index.reg, index.scale,
                               FixedArray::kHeaderSize - argc * kPointerSize));
      __ cmpq(rdx, rcx);
      __ j(not_equal, &call_builtin);
      __ addq(rcx, Immediate(kAllocationDelta * kPointerSize));
      __ movq(kScratchRegister, new_space_allocation_limit);
      __ cmpq(rcx, Operand(kScratchRegister, 0));
      __ j(above, &call_builtin);

      // We fit and could grow elements.
      __ movq(kScratchRegister, new_space_allocation_top);
      __ movq(Operand(kScratchRegister, 0), rcx);
      __ movq(rcx, Operand(rsp, argc * kPointerSize));

      // Push the argument...
      __ movq(Operand(rdx, 0), rcx);
      // ... and fill the rest with holes.
      __ Move(kScratchRegister, Factory::the_hole_value());
      for (int i = 1; i < kAllocationDelta; i++) {
        __ movq(Operand(rdx, i * kPointerSize), kScratchRegister);
      }

      // Restore receiver to rdx as finish sequence assumes it's here.
      __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

      // Increment element's and array's sizes.
      __ SmiAddConstant(FieldOperand(rbx, FixedArray::kLengthOffset),
                        Smi::FromInt(kAllocationDelta));
      __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rax);

      // Elements are in new space, so write barrier is not required.
      __ ret((argc + 1) * kPointerSize);

      __ bind(&call_builtin);
    }

    __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPush),
                                 argc + 1,
                                 1);
  }

  __ bind(&miss);

  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

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

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  CheckPrototypes(JSObject::cast(object), rdx,
                  holder, rbx,
                  rax, name, &miss);

  // Get the elements array of the object.
  __ movq(rbx, FieldOperand(rdx, JSArray::kElementsOffset));

  // Check that the elements are in fast mode (not dictionary).
  __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset), Factory::fixed_array_map());
  __ j(not_equal, &miss);

  // Get the array's length into rcx and calculate new length.
  __ movq(rcx, FieldOperand(rdx, JSArray::kLengthOffset));
  __ SmiSubConstant(rcx, rcx, Smi::FromInt(1));
  __ SmiTest(rcx);
  __ j(negative, &return_undefined);

  // Get the last element.
  __ Move(r9, Factory::the_hole_value());
  SmiIndex index =
      masm()->SmiToIndex(r8, rcx, times_pointer_size);
  __ movq(rax, FieldOperand(rbx,
                            index.reg, index.scale,
                            FixedArray::kHeaderSize));
  // Check if element is already the hole.
  __ cmpq(rax, r9);
  __ j(equal, &call_builtin);

  // Set the array's length.
  __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rcx);

  // Fill with the hole and return original value..
  __ movq(FieldOperand(rbx,
                       index.reg, index.scale,
                       FixedArray::kHeaderSize),
          r9);
  __ ret((argc + 1) * kPointerSize);

  __ bind(&return_undefined);

  __ Move(rax, Factory::undefined_value());
  __ ret((argc + 1) * kPointerSize);

  __ bind(&call_builtin);
  __ TailCallExternalReference(ExternalReference(Builtins::c_ArrayPop),
                               argc + 1,
                               1);
  __ bind(&miss);

  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(function);
}


Object* CallStubCompiler::CompileStringCharAtCall(Object* object,
                                                  JSObject* holder,
                                                  JSFunction* function,
                                                  String* name,
                                                  CheckType check) {
  // TODO(722): implement this.
  return Heap::undefined_value();
}


Object* CallStubCompiler::CompileStringCharCodeAtCall(Object* object,
                                                      JSObject* holder,
                                                      JSFunction* function,
                                                      String* name,
                                                      CheckType check) {
  // TODO(722): implement this.
  return Heap::undefined_value();
}



Object* CallStubCompiler::CompileCallInterceptor(JSObject* object,
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

  // Get the number of arguments.
  const int argc = arguments().immediate();

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), rcx);
  compiler.Compile(masm(),
                   object,
                   holder,
                   name,
                   &lookup,
                   rdx,
                   rbx,
                   rdi,
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
  __ InvokeFunction(rdi, arguments(), JUMP_FUNCTION);

  // Handle load cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(argc);
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* CallStubCompiler::CompileCallGlobal(JSObject* object,
                                            GlobalObject* holder,
                                            JSGlobalPropertyCell* cell,
                                            JSFunction* function,
                                            String* name) {
  // ----------- S t a t e -------------
  // -----------------------------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  Label miss;

  // Get the number of arguments.
  const int argc = arguments().immediate();

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual calls. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(rdx, &miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, rdx, holder, rbx, rax, name, &miss);

  // Get the value from the cell.
  __ Move(rdi, Handle<JSGlobalPropertyCell>(cell));
  __ movq(rdi, FieldOperand(rdi, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (Heap::InNewSpace(function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(rdi, &miss);
    __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rax);
    __ j(not_equal, &miss);

    // Check the shared function info. Make sure it hasn't changed.
    __ Move(rax, Handle<SharedFunctionInfo>(function->shared()));
    __ cmpq(FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset), rax);
    __ j(not_equal, &miss);
  } else {
    __ Cmp(rdi, Handle<JSFunction>(function));
    __ j(not_equal, &miss);
  }

  // Patch the receiver on the stack with the global proxy.
  if (object->IsGlobalObject()) {
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Setup the context (function already in edi).
  __ movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

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
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* LoadStubCompiler::CompileLoadCallback(String* name,
                                              JSObject* object,
                                              JSObject* holder,
                                              AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  Failure* failure = Failure::InternalError();
  bool success = GenerateLoadCallback(object, holder, rax, rcx, rbx, rdx,
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
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  GenerateLoadConstant(object, holder, rax, rbx, rdx, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* LoadStubCompiler::CompileLoadNonexistent(String* name,
                                                 JSObject* object,
                                                 JSObject* last) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  // Load receiver.
  __ movq(rax, Operand(rsp, kPointerSize));

  // Chech that receiver is not a smi.
  __ JumpIfSmi(rax, &miss);

  // Check the maps of the full prototype chain. Also check that
  // global property cells up to (but not including) the last object
  // in the prototype chain are empty.
  CheckPrototypes(object, rax, last, rbx, rdx, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (last->IsGlobalObject()) {
    Object* cell = GenerateCheckPropertyCell(masm(),
                                             GlobalObject::cast(last),
                                             name,
                                             rdx,
                                             &miss);
    if (cell->IsFailure()) return cell;
  }

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
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
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  GenerateLoadField(object, holder, rax, rbx, rdx, index, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* LoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                 JSObject* holder,
                                                 String* name) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  __ movq(rax, Operand(rsp, kPointerSize));
  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          rax,
                          rcx,
                          rdx,
                          rbx,
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
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack.
  __ movq(rax, Operand(rsp, kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual loads. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ JumpIfSmi(rax, &miss);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, rax, holder, rbx, rdx, name, &miss);

  // Get the value from the cell.
  __ Move(rax, Handle<JSGlobalPropertyCell>(cell));
  __ movq(rax, FieldOperand(rax, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  __ IncrementCounter(&Counters::named_load_global_inline, 1);
  __ ret(0);

  __ bind(&miss);
  __ IncrementCounter(&Counters::named_load_global_inline_miss, 1);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_callback, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  Failure* failure = Failure::InternalError();
  bool success = GenerateLoadCallback(receiver, holder, rcx, rax, rbx, rdx,
                                      callback, name, &miss, &failure);
  if (!success) return failure;

  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_callback, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_array_length, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadArrayLength(masm(), rcx, rdx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_array_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   Object* value) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_constant_function, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadConstant(receiver, holder, rcx, rbx, rdx,
                       value, name, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_constant_function, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_function_prototype, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadFunctionPrototype(masm(), rcx, rdx, rbx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_function_prototype, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_interceptor, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          rcx,
                          rax,
                          rdx,
                          rbx,
                          name,
                          &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_interceptor, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_string_length, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadStringLength(masm(), rcx, rdx, rbx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_string_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* StoreStubCompiler::CompileStoreCallback(JSObject* object,
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
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty));
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* StoreStubCompiler::CompileStoreField(JSObject* object,
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
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


Object* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
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
  __ push(rbx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty));
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Handle store cache miss.
  __ bind(&miss);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* StoreStubCompiler::CompileStoreGlobal(GlobalObject* object,
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

  // Store the value in the cell.
  __ Move(rcx, Handle<JSGlobalPropertyCell>(cell));
  __ movq(FieldOperand(rcx, JSGlobalPropertyCell::kValueOffset), rax);

  // Return the value (register rax).
  __ IncrementCounter(&Counters::named_store_global_inline, 1);
  __ ret(0);

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(&Counters::named_store_global_inline_miss, 1);
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(NORMAL, name);
}


Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int index) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_field, 1);

  // Check that the name has not changed.
  __ Cmp(rax, Handle<String>(name));
  __ j(not_equal, &miss);

  GenerateLoadField(receiver, holder, rcx, rbx, rdx, index, name, &miss);

  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_field, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
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

  __ IncrementCounter(&Counters::keyed_store_field, 1);

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
  __ DecrementCounter(&Counters::keyed_store_field, 1);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


// TODO(1241006): Avoid having lazy compile stubs specialized by the
// number of arguments. It is not needed anymore.
Object* StubCompiler::CompileLazyCompile(Code::Flags flags) {
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push a copy of the function onto the stack.
  __ push(rdi);

  __ push(rdi);  // function is also the parameter to the runtime call
  __ CallRuntime(Runtime::kLazyCompile, 1);
  __ pop(rdi);

  // Tear down temporary frame.
  __ LeaveInternalFrame();

  // Do a tail-call of the compiled function.
  __ lea(rcx, FieldOperand(rax, Code::kHeaderSize));
  __ jmp(rcx);

  return GetCodeWithFlags(flags, "LazyCompileStub");
}


void StubCompiler::GenerateLoadInterceptor(JSObject* object,
                                           JSObject* interceptor_holder,
                                           LookupResult* lookup,
                                           Register receiver,
                                           Register name_reg,
                                           Register scratch1,
                                           Register scratch2,
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
                                          scratch1, scratch2, name, miss);
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
    Register holder_reg = CheckPrototypes(object, receiver, interceptor_holder,
                                          scratch1, scratch2, name, miss);
    __ pop(scratch2);  // save old return address
    PushInterceptorArguments(masm(), receiver, holder_reg,
                             name_reg, interceptor_holder);
    __ push(scratch2);  // restore old return address

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad));
    __ TailCallExternalReference(ref, 5, 1);
  }
}


bool StubCompiler::GenerateLoadCallback(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        AccessorInfo* callback,
                                        String* name,
                                        Label* miss,
                                        Failure** failure) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Push the arguments on the JS stack of the caller.
  __ pop(scratch2);  // remove return address
  __ push(receiver);  // receiver
  __ push(reg);  // holder
  __ Move(reg, Handle<AccessorInfo>(callback));  // callback data
  __ push(reg);
  __ push(FieldOperand(reg, AccessorInfo::kDataOffset));
  __ push(name_reg);  // name
  __ push(scratch2);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference load_callback_property =
      ExternalReference(IC_Utility(IC::kLoadCallbackProperty));
  __ TailCallExternalReference(load_callback_property, 5, 1);

  return true;
}


Register StubCompiler::CheckPrototypes(JSObject* object,
                                       Register object_reg,
                                       JSObject* holder,
                                       Register holder_reg,
                                       Register scratch,
                                       String* name,
                                       int save_at_depth,
                                       Label* miss) {
  // Check that the maps haven't changed.
  Register result =
      masm()->CheckMaps(object,
                        object_reg,
                        holder,
                        holder_reg,
                        scratch,
                        save_at_depth,
                        miss);

  // If we've skipped any global objects, it's not enough to verify
  // that their maps haven't changed.  We also need to check that the
  // property cell for the property is still empty.
  while (object != holder) {
    if (object->IsGlobalObject()) {
      Object* cell = GenerateCheckPropertyCell(masm(),
                                               GlobalObject::cast(object),
                                               name,
                                               scratch,
                                               miss);
      if (cell->IsFailure()) {
        set_failure(Failure::cast(cell));
        return result;
      }
    }
    object = JSObject::cast(object->GetPrototype());
  }

  // Return the register containing the holder.
  return result;
}


void StubCompiler::GenerateLoadField(JSObject* object,
                                     JSObject* holder,
                                     Register receiver,
                                     Register scratch1,
                                     Register scratch2,
                                     int index,
                                     String* name,
                                     Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check the prototype chain.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Get the value from the properties.
  GenerateFastPropertyLoad(masm(), rax, reg, holder, index);
  __ ret(0);
}


void StubCompiler::GenerateLoadConstant(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Object* value,
                                        String* name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Return the constant value.
  __ Move(rax, Handle<Object>(value));
  __ ret(0);
}


// Specialized stub for constructing objects from functions which only have only
// simple assignments of the form this.x = ...; in their body.
Object* ConstructStubCompiler::CompileConstructStub(
    SharedFunctionInfo* shared) {
  // ----------- S t a t e -------------
  //  -- rax : argc
  //  -- rdi : constructor
  //  -- rsp[0] : return address
  //  -- rsp[4] : last argument
  // -----------------------------------
  Label generic_stub_call;

  // Use r8 for holding undefined which is used in several places below.
  __ Move(r8, Factory::undefined_value());

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
  ASSERT(kSmiTag == 0);
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
  __ Move(rbx, Factory::empty_fixed_array());
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
  for (int i = shared->this_property_assignments_count();
       i < shared->CalculateInObjectProperties();
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
  __ IncrementCounter(&Counters::constructed_objects, 1);
  __ IncrementCounter(&Counters::constructed_objects_stub, 1);
  __ ret(0);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Code* code = Builtins::builtin(Builtins::JSConstructStubGeneric);
  Handle<Code> generic_construct_stub(code);
  __ Jump(generic_construct_stub, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
