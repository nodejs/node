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
                       Register offset) {
  ExternalReference key_offset(SCTableReference::keyReference(table));
  ExternalReference value_offset(SCTableReference::valueReference(table));

  Label miss;

  // Save the offset on the stack.
  __ push(offset);

  // Check that the key in the entry matches the name.
  __ mov(ip, Operand(key_offset));
  __ ldr(ip, MemOperand(ip, offset, LSL, 1));
  __ cmp(name, Operand(ip));
  __ b(ne, &miss);

  // Get the code entry from the cache.
  __ mov(ip, Operand(value_offset));
  __ ldr(offset, MemOperand(ip, offset, LSL, 1));

  // Check that the flags match what we're looking for.
  __ ldr(offset, FieldMemOperand(offset, Code::kFlagsOffset));
  __ and_(offset, offset, Operand(~Code::kFlagsNotUsedInLookup));
  __ cmp(offset, Operand(flags));
  __ b(ne, &miss);

  // Restore offset and re-load code entry from cache.
  __ pop(offset);
  __ mov(ip, Operand(value_offset));
  __ ldr(offset, MemOperand(ip, offset, LSL, 1));

  // Jump to the first instruction in the code stub.
  __ add(offset, offset, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(offset);

  // Miss: Restore offset and fall through.
  __ bind(&miss);
  __ pop(offset);
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch) {
  Label miss;

  // Make sure that code is valid. The shifting code relies on the
  // entry size being 8.
  ASSERT(sizeof(Entry) == 8);

  // Make sure the flags does not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));

  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Get the map of the receiver and compute the hash.
  __ ldr(scratch, FieldMemOperand(name, String::kLengthOffset));
  __ ldr(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, Operand(ip));
  __ eor(scratch, scratch, Operand(flags));
  __ and_(scratch,
          scratch,
          Operand((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(masm, flags, kPrimary, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ sub(scratch, scratch, Operand(name));
  __ add(scratch, scratch, Operand(flags));
  __ and_(scratch,
          scratch,
          Operand((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(masm, flags, kSecondary, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ ldr(prototype, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  __ ldr(prototype,
         FieldMemOperand(prototype, GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  __ ldr(prototype, MemOperand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ ldr(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
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
    __ ldr(dst, FieldMemOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + Array::kHeaderSize;
    __ ldr(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    __ ldr(dst, FieldMemOperand(dst, offset));
  }
}


void StubCompiler::GenerateLoadField(MacroAssembler* masm,
                                     JSObject* object,
                                     JSObject* holder,
                                     Register receiver,
                                     Register scratch1,
                                     Register scratch2,
                                     int index,
                                     Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the maps haven't changed.
  Register reg =
      masm->CheckMaps(object, receiver, holder, scratch1, scratch2, miss_label);
  GenerateFastPropertyLoad(masm, r0, reg, holder, index);
  __ Ret();
}


void StubCompiler::GenerateLoadConstant(MacroAssembler* masm,
                                        JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Object* value,
                                        Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the maps haven't changed.
  Register reg =
      masm->CheckMaps(object, receiver, holder, scratch1, scratch2, miss_label);

  // Return the constant value.
  __ mov(r0, Operand(Handle<Object>(value)));
  __ Ret();
}


void StubCompiler::GenerateLoadCallback(MacroAssembler* masm,
                                        JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register name,
                                        Register scratch1,
                                        Register scratch2,
                                        AccessorInfo* callback,
                                        Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the maps haven't changed.
  Register reg =
      masm->CheckMaps(object, receiver, holder, scratch1, scratch2, miss_label);

  // Push the arguments on the JS stack of the caller.
  __ push(receiver);  // receiver
  __ mov(ip, Operand(Handle<AccessorInfo>(callback)));  // callback data
  __ push(ip);
  __ push(name);  // name
  __ push(reg);  // holder

  // Do tail-call to the runtime system.
  ExternalReference load_callback_property =
      ExternalReference(IC_Utility(IC::kLoadCallbackProperty));
  __ TailCallRuntime(load_callback_property, 4);
}


void StubCompiler::GenerateLoadInterceptor(MacroAssembler* masm,
                                           JSObject* object,
                                           JSObject* holder,
                                           Smi* lookup_hint,
                                           Register receiver,
                                           Register name,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the maps haven't changed.
  Register reg =
      masm->CheckMaps(object, receiver, holder, scratch1, scratch2, miss_label);

  // Push the arguments on the JS stack of the caller.
  __ push(receiver);  // receiver
  __ push(reg);  // holder
  __ push(name);  // name
  __ mov(scratch1, Operand(lookup_hint));
  __ push(scratch1);

  // Do tail-call to the runtime system.
  ExternalReference load_ic_property =
      ExternalReference(IC_Utility(IC::kLoadInterceptorProperty));
  __ TailCallRuntime(load_ic_property, 4);
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the object is a JS array.
  __ ldr(scratch, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(JS_ARRAY_TYPE));
  __ b(ne, miss_label);

  // Load length directly from the JS array.
  __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Ret();
}


// Generate code to check if an object is a string.  If the object is
// a string, the map's instance type is left in the scratch1 register.
static void GenerateStringCheck(MacroAssembler* masm,
                                Register receiver,
                                Register scratch1,
                                Register scratch2,
                                Label* smi,
                                Label* non_string_object) {
  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, smi);

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
void StubCompiler::GenerateLoadStringLength2(MacroAssembler* masm,
                                             Register receiver,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* miss) {
  Label check_string, check_wrapper;

  __ bind(&check_string);
  // Check if the object is a string leaving the instance type in the
  // scratch1 register.
  GenerateStringCheck(masm, receiver, scratch1, scratch2,
                      miss, &check_wrapper);

  // Load length directly from the string.
  __ and_(scratch1, scratch1, Operand(kStringSizeMask));
  __ add(scratch1, scratch1, Operand(String::kHashShift));
  __ ldr(r0, FieldMemOperand(receiver, String::kLengthOffset));
  __ mov(r0, Operand(r0, LSR, scratch1));
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ Ret();

  // Check if the object is a JSValue wrapper.
  __ bind(&check_wrapper);
  __ cmp(scratch1, Operand(JS_VALUE_TYPE));
  __ b(ne, miss);

  // Unwrap the value in place and check if the wrapped value is a string.
  __ ldr(receiver, FieldMemOperand(receiver, JSValue::kValueOffset));
  __ b(&check_string);
}


// Generate StoreField code, value is passed in r0 register.
// After executing generated code, the receiver_reg and name_reg
// may be clobbered.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      Builtins::Name storage_extend,
                                      JSObject* object,
                                      int index,
                                      Map* transition,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register scratch,
                                      Label* miss_label) {
  // r0 : value
  Label exit;

  // Check that the receiver isn't a smi.
  __ tst(receiver_reg, Operand(kSmiTagMask));
  __ b(eq, miss_label);

  // Check that the map of the receiver hasn't changed.
  __ ldr(scratch, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(Handle<Map>(object->map())));
  __ b(ne, miss_label);

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
    __ mov(r2, Operand(Handle<Map>(transition)));
    // Please note, if we implement keyed store for arm we need
    // to call the Builtins::KeyedStoreIC_ExtendStorage.
    Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_ExtendStorage));
    __ Jump(ic, RelocInfo::CODE_TARGET);
    return;
  }

  if (transition != NULL) {
    // Update the map of the object; no write barrier updating is
    // needed because the map is never in new space.
    __ mov(ip, Operand(Handle<Map>(transition)));
    __ str(ip, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));
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
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &exit);

    // Update the write barrier for the array address.
    // Pass the value being stored in the now unused name_reg.
    __ mov(name_reg, Operand(offset));
    __ RecordWrite(receiver_reg, name_reg, scratch);
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + Array::kHeaderSize;
    // Get the properties array
    __ ldr(scratch, FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ str(r0, FieldMemOperand(scratch, offset));

    // Skip updating write barrier if storing a smi.
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &exit);

    // Update the write barrier for the array address.
    // Ok to clobber receiver_reg and name_reg, since we return.
    __ mov(name_reg, Operand(offset));
    __ RecordWrite(scratch, name_reg, receiver_reg);
  }

  // Return the value (register r0).
  __ bind(&exit);
  __ Ret();
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


#undef __
#define __ ACCESS_MASM(masm())


Object* StubCompiler::CompileLazyCompile(Code::Flags flags) {
  // ----------- S t a t e -------------
  //  -- r1: function
  //  -- lr: return address
  // -----------------------------------

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Preserve the function.
  __ push(r1);

  // Push the function on the stack as the argument to the runtime function.
  __ push(r1);
  __ CallRuntime(Runtime::kLazyCompile, 1);

  // Calculate the entry point.
  __ add(r2, r0, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Restore saved function.
  __ pop(r1);

  // Tear down temporary frame.
  __ LeaveInternalFrame();

  // Do a tail-call of the compiled function.
  __ Jump(r2);

  return GetCodeWithFlags(flags, "LazyCompileStub");
}


Object* CallStubCompiler::CompileCallField(Object* object,
                                           JSObject* holder,
                                           int index,
                                           String* name,
                                           Code::Flags flags) {
  ASSERT_EQ(FIELD, Code::ExtractTypeFromFlags(flags));
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------
  Label miss;

  const int argc = arguments().immediate();

  // Get the receiver of the function from the stack into r0.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Do the right check and compute the holder register.
  Register reg =
      masm()->CheckMaps(JSObject::cast(object), r0, holder, r3, r2, &miss);
  GenerateFastPropertyLoad(masm(), r1, reg, holder, index);

  // Check that the function really is a function.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &miss);
  // Get the map.
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  __ cmp(r2, Operand(JS_FUNCTION_TYPE));
  __ b(ne, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ ldr(r3, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));
    __ str(r3, MemOperand(sp, argc * kPointerSize));
  }

  // Invoke the function.
  __ InvokeFunction(r1, arguments(), JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCodeWithFlags(flags, name);
}


Object* CallStubCompiler::CompileCallConstant(Object* object,
                                              JSObject* holder,
                                              JSFunction* function,
                                              CheckType check,
                                              Code::Flags flags) {
  ASSERT_EQ(CONSTANT_FUNCTION, Code::ExtractTypeFromFlags(flags));
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ tst(r1, Operand(kSmiTagMask));
    __ b(eq, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  switch (check) {
    case RECEIVER_MAP_CHECK:
      // Check that the maps haven't changed.
      __ CheckMaps(JSObject::cast(object), r1, holder, r3, r2, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ ldr(r3, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
        __ str(r3, MemOperand(sp, argc * kPointerSize));
      }
      break;

    case STRING_CHECK:
      // Check that the object is a two-byte string or a symbol.
      __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
      __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
      __ cmp(r2, Operand(FIRST_NONSTRING_TYPE));
      __ b(hs, &miss);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::STRING_FUNCTION_INDEX,
                                          r2);
      __ CheckMaps(JSObject::cast(object->GetPrototype()),
                   r2, holder, r3, r1, &miss);
      break;

    case NUMBER_CHECK: {
      Label fast;
      // Check that the object is a smi or a heap number.
      __ tst(r1, Operand(kSmiTagMask));
      __ b(eq, &fast);
      __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
      __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
      __ cmp(r2, Operand(HEAP_NUMBER_TYPE));
      __ b(ne, &miss);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::NUMBER_FUNCTION_INDEX,
                                          r2);
      __ CheckMaps(JSObject::cast(object->GetPrototype()),
                   r2, holder, r3, r1, &miss);
      break;
    }

    case BOOLEAN_CHECK: {
      Label fast;
      // Check that the object is a boolean.
      __ cmp(r1, Operand(Factory::true_value()));
      __ b(eq, &fast);
      __ cmp(r1, Operand(Factory::false_value()));
      __ b(ne, &miss);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::BOOLEAN_FUNCTION_INDEX,
                                          r2);
      __ CheckMaps(JSObject::cast(object->GetPrototype()),
                   r2, holder, r3, r1, &miss);
      break;
    }

    case JSARRAY_HAS_FAST_ELEMENTS_CHECK:
      __ CheckMaps(JSObject::cast(object), r1, holder, r3, r2, &miss);
      // Make sure object->elements()->map() != Heap::hash_table_map()
      // Get the elements array of the object.
      __ ldr(r3, FieldMemOperand(r1, JSObject::kElementsOffset));
      // Check that the object is in fast mode (not dictionary).
      __ ldr(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
      __ cmp(r2, Operand(Factory::hash_table_map()));
      __ b(eq, &miss);
      break;

    default:
      UNREACHABLE();
  }

  // Get the function and setup the context.
  __ mov(r1, Operand(Handle<JSFunction>(function)));
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  __ InvokeCode(code, expected, arguments(),
                RelocInfo::CODE_TARGET, JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  String* function_name = NULL;
  if (function->shared()->name()->IsString()) {
    function_name = String::cast(function->shared()->name());
  }
  return GetCodeWithFlags(flags, function_name);
}


Object* CallStubCompiler::CompileCallInterceptor(Object* object,
                                                 JSObject* holder,
                                                 String* name) {
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------
  Label miss;

  // TODO(1224669): Implement.

  // Handle call cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* StoreStubCompiler::CompileStoreField(JSObject* object,
                                             int index,
                                             Map* transition,
                                             String* name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack.
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));

  // name register might be clobbered.
  GenerateStoreField(masm(),
                     Builtins::StoreIC_ExtendStorage,
                     object,
                     index,
                     transition,
                     r3, r2, r1,
                     &miss);
  __ bind(&miss);
  __ mov(r2, Operand(Handle<String>(name)));  // restore name
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


Object* StoreStubCompiler::CompileStoreCallback(JSObject* object,
                                                AccessorInfo* callback,
                                                String* name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  // Get the object from the stack.
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));

  // Check that the object isn't a smi.
  __ tst(r3, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Check that the map of the object hasn't changed.
  __ ldr(r1, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ cmp(r1, Operand(Handle<Map>(object->map())));
  __ b(ne, &miss);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(r3, r1, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ ldr(ip, MemOperand(sp));  // receiver
  __ push(ip);
  __ mov(ip, Operand(Handle<AccessorInfo>(callback)));  // callback info
  __ push(ip);
  __ push(r2);  // name
  __ push(r0);  // value

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty));
  __ TailCallRuntime(store_callback_property, 4);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(r2, Operand(Handle<String>(name)));  // restore name
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* StoreStubCompiler::CompileStoreInterceptor(JSObject* receiver,
                                                   String* name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  // Get the object from the stack.
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));

  // Check that the object isn't a smi.
  __ tst(r3, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Check that the map of the object hasn't changed.
  __ ldr(r1, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ cmp(r1, Operand(Handle<Map>(receiver->map())));
  __ b(ne, &miss);

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(r3, r1, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ ldr(ip, MemOperand(sp));  // receiver
  __ push(ip);
  __ push(r2);  // name
  __ push(r0);  // value

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty));
  __ TailCallRuntime(store_ic_property, 3);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(r2, Operand(Handle<String>(name)));  // restore name
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


Object* LoadStubCompiler::CompileLoadField(JSObject* object,
                                           JSObject* holder,
                                           int index,
                                           String* name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r0, MemOperand(sp, 0));

  GenerateLoadField(masm(), object, holder, r0, r3, r1, index, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* LoadStubCompiler::CompileLoadCallback(JSObject* object,
                                              JSObject* holder,
                                              AccessorInfo* callback,
                                              String* name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r0, MemOperand(sp, 0));
  GenerateLoadCallback(masm(), object, holder, r0, r2, r3, r1, callback, &miss);
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
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp] : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r0, MemOperand(sp, 0));

  GenerateLoadConstant(masm(), object, holder, r0, r3, r1, value, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* LoadStubCompiler::CompileLoadInterceptor(JSObject* object,
                                                 JSObject* holder,
                                                 String* name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r0, MemOperand(sp, 0));

  GenerateLoadInterceptor(masm(),
                          object,
                          holder,
                          holder->InterceptorPropertyLookupHint(name),
                          r0,
                          r2,
                          r3,
                          r1,
                          &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(INTERCEPTOR, name);
}


// TODO(1224671): IC stubs for keyed loads have not been implemented
// for ARM.
Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int index) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r2, MemOperand(sp, 0));
  __ ldr(r0, MemOperand(sp, kPointerSize));

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadField(masm(), receiver, holder, r0, r3, r1, index, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(FIELD, name);
}


Object* KeyedLoadStubCompiler::CompileLoadCallback(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   AccessorInfo* callback) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r2, MemOperand(sp, 0));
  __ ldr(r0, MemOperand(sp, kPointerSize));

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadCallback(masm(), receiver, holder, r0, r2, r3,
                       r1, callback, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadConstant(String* name,
                                                   JSObject* receiver,
                                                   JSObject* holder,
                                                   Object* value) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one
  __ ldr(r2, MemOperand(sp, 0));
  __ ldr(r0, MemOperand(sp, kPointerSize));

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadConstant(masm(), receiver, holder, r0, r3, r1, value, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* KeyedLoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                      JSObject* holder,
                                                      String* name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one
  __ ldr(r2, MemOperand(sp, 0));
  __ ldr(r0, MemOperand(sp, kPointerSize));

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadInterceptor(masm(),
                          receiver,
                          holder,
                          Smi::FromInt(JSObject::kLookupInHolder),
                          r0,
                          r2,
                          r3,
                          r1,
                          &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(INTERCEPTOR, name);
}


Object* KeyedLoadStubCompiler::CompileLoadArrayLength(String* name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;

  // Check the key is the cached one
  __ ldr(r2, MemOperand(sp, 0));
  __ ldr(r0, MemOperand(sp, kPointerSize));

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadArrayLength(masm(), r0, r3, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  Label miss;
  __ IncrementCounter(&Counters::keyed_load_string_length, 1, r1, r3);

  __ ldr(r2, MemOperand(sp));
  __ ldr(r0, MemOperand(sp, kPointerSize));  // receiver

  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  GenerateLoadStringLength2(masm(), r0, r1, r3, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_string_length, 1, r1, r3);

  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


// TODO(1224671): implement the fast case.
Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  //  -- sp[0] : key
  //  -- sp[4] : receiver
  // -----------------------------------
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  return GetCode(CALLBACKS, name);
}


Object* KeyedStoreStubCompiler::CompileStoreField(JSObject* object,
                                                  int index,
                                                  Map* transition,
                                                  String* name) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_store_field, 1, r1, r3);

  // Check that the name has not changed.
  __ cmp(r2, Operand(Handle<String>(name)));
  __ b(ne, &miss);

  // Load receiver from the stack.
  __ ldr(r3, MemOperand(sp));
  // r1 is used as scratch register, r3 and r2 might be clobbered.
  GenerateStoreField(masm(),
                     Builtins::StoreIC_ExtendStorage,
                     object,
                     index,
                     transition,
                     r3, r2, r1,
                     &miss);
  __ bind(&miss);

  __ DecrementCounter(&Counters::keyed_store_field, 1, r1, r3);
  __ mov(r2, Operand(Handle<String>(name)));  // restore name register.
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}


#undef __

} }  // namespace v8::internal
