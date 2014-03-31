// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_ARM64

#include "ic-inl.h"
#include "codegen.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void StubCompiler::GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                                    Label* miss_label,
                                                    Register receiver,
                                                    Handle<Name> name,
                                                    Register scratch0,
                                                    Register scratch1) {
  ASSERT(!AreAliased(receiver, scratch0, scratch1));
  ASSERT(name->IsUniqueName());
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1, scratch0, scratch1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);

  Label done;

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  Register map = scratch1;
  __ Ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Ldrb(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ Tst(scratch0, kInterceptorOrAccessCheckNeededMask);
  __ B(ne, miss_label);

  // Check that receiver is a JSObject.
  __ Ldrb(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ Cmp(scratch0, FIRST_SPEC_OBJECT_TYPE);
  __ B(lt, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ Ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ Ldr(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  __ JumpIfNotRoot(map, Heap::kHashTableMapRootIndex, miss_label);

  NameDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                     miss_label,
                                                     &done,
                                                     receiver,
                                                     properties,
                                                     name,
                                                     scratch1);
  __ Bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);
}


// Probe primary or secondary table.
// If the entry is found in the cache, the generated code jump to the first
// instruction of the stub in the cache.
// If there is a miss the code fall trough.
//
// 'receiver', 'name' and 'offset' registers are preserved on miss.
static void ProbeTable(Isolate* isolate,
                       MacroAssembler* masm,
                       Code::Flags flags,
                       StubCache::Table table,
                       Register receiver,
                       Register name,
                       Register offset,
                       Register scratch,
                       Register scratch2,
                       Register scratch3) {
  // Some code below relies on the fact that the Entry struct contains
  // 3 pointers (name, code, map).
  STATIC_ASSERT(sizeof(StubCache::Entry) == (3 * kPointerSize));

  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uintptr_t key_off_addr = reinterpret_cast<uintptr_t>(key_offset.address());
  uintptr_t value_off_addr =
      reinterpret_cast<uintptr_t>(value_offset.address());
  uintptr_t map_off_addr = reinterpret_cast<uintptr_t>(map_offset.address());

  Label miss;

  ASSERT(!AreAliased(name, offset, scratch, scratch2, scratch3));

  // Multiply by 3 because there are 3 fields per entry.
  __ Add(scratch3, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ Mov(scratch, key_offset);
  __ Add(scratch, scratch, Operand(scratch3, LSL, kPointerSizeLog2));

  // Check that the key in the entry matches the name.
  __ Ldr(scratch2, MemOperand(scratch));
  __ Cmp(name, scratch2);
  __ B(ne, &miss);

  // Check the map matches.
  __ Ldr(scratch2, MemOperand(scratch, map_off_addr - key_off_addr));
  __ Ldr(scratch3, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Cmp(scratch2, scratch3);
  __ B(ne, &miss);

  // Get the code entry from the cache.
  __ Ldr(scratch, MemOperand(scratch, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  __ Ldr(scratch2.W(), FieldMemOperand(scratch, Code::kFlagsOffset));
  __ Bic(scratch2.W(), scratch2.W(), Code::kFlagsNotUsedInLookup);
  __ Cmp(scratch2.W(), flags);
  __ B(ne, &miss);

#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    __ B(&miss);
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    __ B(&miss);
  }
#endif

  // Jump to the first instruction in the code stub.
  __ Add(scratch, scratch, Code::kHeaderSize - kHeapObjectTag);
  __ Br(scratch);

  // Miss: fall through.
  __ Bind(&miss);
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

  // Make sure the flags does not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Make sure extra and extra2 registers are valid.
  ASSERT(!extra.is(no_reg));
  ASSERT(!extra2.is(no_reg));
  ASSERT(!extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1,
                      extra2, extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Compute the hash for primary table.
  __ Ldr(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ Ldr(extra, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Add(scratch, scratch, extra);
  __ Eor(scratch, scratch, flags);
  // We shift out the last two bits because they are not part of the hash.
  __ Ubfx(scratch, scratch, kHeapObjectTagSize,
          CountTrailingZeros(kPrimaryTableSize, 64));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, receiver, name,
      scratch, extra, extra2, extra3);

  // Primary miss: Compute hash for secondary table.
  __ Sub(scratch, scratch, Operand(name, LSR, kHeapObjectTagSize));
  __ Add(scratch, scratch, flags >> kHeapObjectTagSize);
  __ And(scratch, scratch, kSecondaryTableSize - 1);

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, receiver, name,
      scratch, extra, extra2, extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ Bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1,
                      extra2, extra3);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ Ldr(prototype, GlobalObjectMemOperand());
  // Load the native context from the global or builtins object.
  __ Ldr(prototype,
         FieldMemOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ Ldr(prototype, ContextMemOperand(prototype, index));
  // Load the initial map. The global functions all have initial maps.
  __ Ldr(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ Ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
  Isolate* isolate = masm->isolate();
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(isolate->native_context()->get(index)));

  // Check we're still in the same context.
  Register scratch = prototype;
  __ Ldr(scratch, GlobalObjectMemOperand());
  __ Ldr(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));
  __ Ldr(scratch, ContextMemOperand(scratch, index));
  __ Cmp(scratch, Operand(function));
  __ B(ne, miss);

  // Load its initial map. The global functions all have initial maps.
  __ Mov(prototype, Operand(Handle<Map>(function->initial_map())));
  // Load the prototype from the initial map.
  __ Ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateFastPropertyLoad(MacroAssembler* masm,
                                            Register dst,
                                            Register src,
                                            bool inobject,
                                            int index,
                                            Representation representation) {
  ASSERT(!representation.IsDouble());
  USE(representation);
  if (inobject) {
    int offset = index * kPointerSize;
    __ Ldr(dst, FieldMemOperand(src, offset));
  } else {
    // Calculate the offset into the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    __ Ldr(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    __ Ldr(dst, FieldMemOperand(dst, offset));
  }
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  ASSERT(!AreAliased(receiver, scratch));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ JumpIfNotObjectType(receiver, scratch, scratch, JS_ARRAY_TYPE,
                         miss_label);

  // Load length directly from the JS array.
  __ Ldr(x0, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Ret();
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  // TryGetFunctionPrototype can't put the result directly in x0 because the
  // 3 inputs registers can't alias and we call this function from
  // LoadIC::GenerateFunctionPrototype, where receiver is x0. So we explicitly
  // move the result in x0.
  __ Mov(x0, scratch1);
  __ Ret();
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
void StubCompiler::GenerateCheckPropertyCell(MacroAssembler* masm,
                                             Handle<JSGlobalObject> global,
                                             Handle<Name> name,
                                             Register scratch,
                                             Label* miss) {
  Handle<Cell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
  ASSERT(cell->value()->IsTheHole());
  __ Mov(scratch, Operand(cell));
  __ Ldr(scratch, FieldMemOperand(scratch, Cell::kValueOffset));
  __ JumpIfNotRoot(scratch, Heap::kTheHoleValueRootIndex, miss);
}


void StoreStubCompiler::GenerateNegativeHolderLookup(
    MacroAssembler* masm,
    Handle<JSObject> holder,
    Register holder_reg,
    Handle<Name> name,
    Label* miss) {
  if (holder->IsJSGlobalObject()) {
    GenerateCheckPropertyCell(
        masm, Handle<JSGlobalObject>::cast(holder), name, scratch1(), miss);
  } else if (!holder->HasFastProperties() && !holder->IsJSGlobalProxy()) {
    GenerateDictionaryNegativeLookup(
        masm, miss, holder_reg, name, scratch1(), scratch2());
  }
}


// Generate StoreTransition code, value is passed in x0 register.
// When leaving generated code after success, the receiver_reg and storage_reg
// may be clobbered. Upon branch to miss_label, the receiver and name registers
// have their original values.
void StoreStubCompiler::GenerateStoreTransition(MacroAssembler* masm,
                                                Handle<JSObject> object,
                                                LookupResult* lookup,
                                                Handle<Map> transition,
                                                Handle<Name> name,
                                                Register receiver_reg,
                                                Register storage_reg,
                                                Register value_reg,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3,
                                                Label* miss_label,
                                                Label* slow) {
  Label exit;

  ASSERT(!AreAliased(receiver_reg, storage_reg, value_reg,
                     scratch1, scratch2, scratch3));

  // We don't need scratch3.
  scratch3 = NoReg;

  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  ASSERT(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), masm->isolate());
    __ LoadObject(scratch1, constant);
    __ Cmp(value_reg, scratch1);
    __ B(ne, miss_label);
  } else if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
  } else if (representation.IsDouble()) {
    UseScratchRegisterScope temps(masm);
    DoubleRegister temp_double = temps.AcquireD();
    __ SmiUntagToDouble(temp_double, value_reg, kSpeculativeUntag);

    Label do_store, heap_number;
    __ AllocateHeapNumber(storage_reg, slow, scratch1, scratch2);

    __ JumpIfSmi(value_reg, &do_store);

    __ CheckMap(value_reg, scratch1, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ Ldr(temp_double, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ Bind(&do_store);
    __ Str(temp_double, FieldMemOperand(storage_reg, HeapNumber::kValueOffset));
  }

  // Stub never generated for non-global objects that require access checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if ((details.type() == FIELD) &&
      (object->map()->unused_property_fields() == 0)) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ Mov(scratch1, Operand(transition));
    __ Push(receiver_reg, scratch1, value_reg);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  // Update the map of the object.
  __ Mov(scratch1, Operand(transition));
  __ Str(scratch1, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));

  // Update the write barrier for the map field.
  __ RecordWriteField(receiver_reg,
                      HeapObject::kMapOffset,
                      scratch1,
                      scratch2,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  if (details.type() == CONSTANT) {
    ASSERT(value_reg.is(x0));
    __ Ret();
    return;
  }

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  Register prop_reg = representation.IsDouble() ? storage_reg : value_reg;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ Str(prop_reg, FieldMemOperand(receiver_reg, offset));

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ Mov(storage_reg, value_reg);
      }
      __ RecordWriteField(receiver_reg,
                          offset,
                          storage_reg,
                          scratch1,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ Ldr(scratch1,
           FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ Str(prop_reg, FieldMemOperand(scratch1, offset));

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ Mov(storage_reg, value_reg);
      }
      __ RecordWriteField(scratch1,
                          offset,
                          storage_reg,
                          receiver_reg,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  }

  __ Bind(&exit);
  // Return the value (register x0).
  ASSERT(value_reg.is(x0));
  __ Ret();
}


// Generate StoreField code, value is passed in x0 register.
// When leaving generated code after success, the receiver_reg and name_reg may
// be clobbered. Upon branch to miss_label, the receiver and name registers have
// their original values.
void StoreStubCompiler::GenerateStoreField(MacroAssembler* masm,
                                           Handle<JSObject> object,
                                           LookupResult* lookup,
                                           Register receiver_reg,
                                           Register name_reg,
                                           Register value_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* miss_label) {
  // x0 : value
  Label exit;

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  int index = lookup->GetFieldIndex().field_index();

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  Representation representation = lookup->representation();
  ASSERT(!representation.IsNone());
  if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
  } else if (representation.IsDouble()) {
    UseScratchRegisterScope temps(masm);
    DoubleRegister temp_double = temps.AcquireD();

    __ SmiUntagToDouble(temp_double, value_reg, kSpeculativeUntag);

    // Load the double storage.
    if (index < 0) {
      int offset = (index * kPointerSize) + object->map()->instance_size();
      __ Ldr(scratch1, FieldMemOperand(receiver_reg, offset));
    } else {
      int offset = (index * kPointerSize) + FixedArray::kHeaderSize;
      __ Ldr(scratch1,
             FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
      __ Ldr(scratch1, FieldMemOperand(scratch1, offset));
    }

    // Store the value into the storage.
    Label do_store, heap_number;

    __ JumpIfSmi(value_reg, &do_store);

    __ CheckMap(value_reg, scratch2, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ Ldr(temp_double, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ Bind(&do_store);
    __ Str(temp_double, FieldMemOperand(scratch1, HeapNumber::kValueOffset));

    // Return the value (register x0).
    ASSERT(value_reg.is(x0));
    __ Ret();
    return;
  }

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ Str(value_reg, FieldMemOperand(receiver_reg, offset));

    if (!representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Pass the now unused name_reg as a scratch register.
      __ Mov(name_reg, value_reg);
      __ RecordWriteField(receiver_reg,
                          offset,
                          name_reg,
                          scratch1,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ Ldr(scratch1,
           FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ Str(value_reg, FieldMemOperand(scratch1, offset));

    if (!representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Ok to clobber receiver_reg and name_reg, since we return.
      __ Mov(name_reg, value_reg);
      __ RecordWriteField(scratch1,
                          offset,
                          name_reg,
                          receiver_reg,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  }

  __ Bind(&exit);
  // Return the value (register x0).
  ASSERT(value_reg.is(x0));
  __ Ret();
}


void StoreStubCompiler::GenerateRestoreName(MacroAssembler* masm,
                                            Label* label,
                                            Handle<Name> name) {
  if (!label->is_unused()) {
    __ Bind(label);
    __ Mov(this->name(), Operand(name));
  }
}


static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(StubCache::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(StubCache::kInterceptorArgsInfoIndex == 1);
  STATIC_ASSERT(StubCache::kInterceptorArgsThisIndex == 2);
  STATIC_ASSERT(StubCache::kInterceptorArgsHolderIndex == 3);
  STATIC_ASSERT(StubCache::kInterceptorArgsLength == 4);

  __ Push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ Mov(scratch, Operand(interceptor));
  __ Push(scratch, receiver, holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj,
    IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  __ CallExternalReference(
      ExternalReference(IC_Utility(id), masm->isolate()),
      StubCache::kInterceptorArgsLength);
}


// Generate call to api function.
void StubCompiler::GenerateFastApiCall(MacroAssembler* masm,
                                       const CallOptimization& optimization,
                                       Handle<Map> receiver_map,
                                       Register receiver,
                                       Register scratch,
                                       bool is_store,
                                       int argc,
                                       Register* values) {
  ASSERT(!AreAliased(receiver, scratch));

  MacroAssembler::PushPopQueue queue(masm);
  queue.Queue(receiver);
  // Write the arguments to the stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc-1-i];
    ASSERT(!AreAliased(receiver, scratch, arg));
    queue.Queue(arg);
  }
  queue.PushQueued();

  ASSERT(optimization.is_simple_api_call());

  // Abi for CallApiFunctionStub.
  Register callee = x0;
  Register call_data = x4;
  Register holder = x2;
  Register api_function_address = x1;

  // Put holder in place.
  CallOptimization::HolderLookup holder_lookup;
  Handle<JSObject> api_holder =
      optimization.LookupHolderOfExpectedType(receiver_map, &holder_lookup);
  switch (holder_lookup) {
    case CallOptimization::kHolderIsReceiver:
      __ Mov(holder, receiver);
      break;
    case CallOptimization::kHolderFound:
      __ LoadObject(holder, api_holder);
      break;
    case CallOptimization::kHolderNotFound:
      UNREACHABLE();
      break;
  }

  Isolate* isolate = masm->isolate();
  Handle<JSFunction> function = optimization.constant_function();
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data_obj(api_call_info->data(), isolate);

  // Put callee in place.
  __ LoadObject(callee, function);

  bool call_data_undefined = false;
  // Put call_data in place.
  if (isolate->heap()->InNewSpace(*call_data_obj)) {
    __ LoadObject(call_data, api_call_info);
    __ Ldr(call_data, FieldMemOperand(call_data, CallHandlerInfo::kDataOffset));
  } else if (call_data_obj->IsUndefined()) {
    call_data_undefined = true;
    __ LoadRoot(call_data, Heap::kUndefinedValueRootIndex);
  } else {
    __ LoadObject(call_data, call_data_obj);
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference ref = ExternalReference(&fun,
                                            ExternalReference::DIRECT_API_CALL,
                                            masm->isolate());
  __ Mov(api_function_address, ref);

  // Jump to stub.
  CallApiFunctionStub stub(is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


void StubCompiler::GenerateTailCall(MacroAssembler* masm, Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM(masm())


Register StubCompiler::CheckPrototypes(Handle<HeapType> type,
                                       Register object_reg,
                                       Handle<JSObject> holder,
                                       Register holder_reg,
                                       Register scratch1,
                                       Register scratch2,
                                       Handle<Name> name,
                                       Label* miss,
                                       PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type, isolate()));

  // object_reg and holder_reg registers can alias.
  ASSERT(!AreAliased(object_reg, scratch1, scratch2));
  ASSERT(!AreAliased(holder_reg, scratch1, scratch2));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type->IsConstant()) {
    current = Handle<JSObject>::cast(type->AsConstant());
  }
  Handle<JSObject> prototype = Handle<JSObject>::null();
  Handle<Map> current_map = receiver_map;
  Handle<Map> holder_map(holder->map());
  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (!current_map.is_identical_to(holder_map)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current_map->IsJSGlobalProxyMap() ||
           !current_map->is_access_check_needed());

    prototype = handle(JSObject::cast(current_map->prototype()));
    if (current_map->is_dictionary_map() &&
        !current_map->IsJSGlobalObjectMap() &&
        !current_map->IsJSGlobalProxyMap()) {
      if (!name->IsUniqueName()) {
        ASSERT(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      ASSERT(current.is_null() ||
             (current->property_dictionary()->FindEntry(*name) ==
              NameDictionary::kNotFound));

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ Ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ Ldr(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool need_map = (depth != 1 || check == CHECK_ALL_MAPS) ||
                      heap()->InNewSpace(*prototype);
      Register map_reg = NoReg;
      if (need_map) {
        map_reg = scratch1;
        __ Ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));
      }

      if (depth != 1 || check == CHECK_ALL_MAPS) {
        __ CheckMap(map_reg, current_map, miss, DONT_DO_SMI_CHECK);
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      if (current_map->IsJSGlobalProxyMap()) {
        UseScratchRegisterScope temps(masm());
        __ CheckAccessGlobalProxy(reg, scratch2, temps.AcquireX(), miss);
      } else if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(
            masm(), Handle<JSGlobalObject>::cast(current), name,
            scratch2, miss);
      }

      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (heap()->InNewSpace(*prototype)) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ Ldr(reg, FieldMemOperand(map_reg, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ Mov(reg, Operand(prototype));
      }
    }

    // Go to the next object in the prototype chain.
    current = prototype;
    current_map = handle(current->map());
  }

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  // Check the holder map.
  if (depth != 0 || check == CHECK_ALL_MAPS) {
    // Check the holder map.
    __ CheckMap(reg, scratch1, current_map, miss, DONT_DO_SMI_CHECK);
  }

  // Perform security check for access to the global object.
  ASSERT(current_map->IsJSGlobalProxyMap() ||
         !current_map->is_access_check_needed());
  if (current_map->IsJSGlobalProxyMap()) {
    __ CheckAccessGlobalProxy(reg, scratch1, scratch2, miss);
  }

  // Return the register containing the holder.
  return reg;
}


void LoadStubCompiler::HandlerFrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    __ Bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void StoreStubCompiler::HandlerFrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    GenerateRestoreName(masm(), miss, name);
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


Register LoadStubCompiler::CallbackHandlerFrontend(Handle<HeapType> type,
                                                   Register object_reg,
                                                   Handle<JSObject> holder,
                                                   Handle<Name> name,
                                                   Handle<Object> callback) {
  Label miss;

  Register reg = HandlerFrontendHeader(type, object_reg, holder, name, &miss);
  // HandlerFrontendHeader can return its result into scratch1() so do not
  // use it.
  Register scratch2 = this->scratch2();
  Register scratch3 = this->scratch3();
  Register dictionary = this->scratch4();
  ASSERT(!AreAliased(reg, scratch2, scratch3, dictionary));

  if (!holder->HasFastProperties() && !holder->IsJSGlobalObject()) {
    // Load the properties dictionary.
    __ Ldr(dictionary, FieldMemOperand(reg, JSObject::kPropertiesOffset));

    // Probe the dictionary.
    Label probe_done;
    NameDictionaryLookupStub::GeneratePositiveLookup(masm(),
                                                     &miss,
                                                     &probe_done,
                                                     dictionary,
                                                     this->name(),
                                                     scratch2,
                                                     scratch3);
    __ Bind(&probe_done);

    // If probing finds an entry in the dictionary, scratch3 contains the
    // pointer into the dictionary. Check that the value is the callback.
    Register pointer = scratch3;
    const int kElementsStartOffset = NameDictionary::kHeaderSize +
        NameDictionary::kElementsStartIndex * kPointerSize;
    const int kValueOffset = kElementsStartOffset + kPointerSize;
    __ Ldr(scratch2, FieldMemOperand(pointer, kValueOffset));
    __ Cmp(scratch2, Operand(callback));
    __ B(ne, &miss);
  }

  HandlerFrontendFooter(name, &miss);
  return reg;
}


void LoadStubCompiler::GenerateLoadField(Register reg,
                                         Handle<JSObject> holder,
                                         PropertyIndex field,
                                         Representation representation) {
  __ Mov(receiver(), reg);
  if (kind() == Code::LOAD_IC) {
    LoadFieldStub stub(field.is_inobject(holder),
                       field.translate(holder),
                       representation);
    GenerateTailCall(masm(), stub.GetCode(isolate()));
  } else {
    KeyedLoadFieldStub stub(field.is_inobject(holder),
                            field.translate(holder),
                            representation);
    GenerateTailCall(masm(), stub.GetCode(isolate()));
  }
}


void LoadStubCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ LoadObject(x0, value);
  __ Ret();
}


void LoadStubCompiler::GenerateLoadCallback(
    Register reg,
    Handle<ExecutableAccessorInfo> callback) {
  ASSERT(!AreAliased(scratch2(), scratch3(), scratch4(), reg));

  // Build ExecutableAccessorInfo::args_ list on the stack and push property
  // name below the exit frame to make GC aware of them and store pointers to
  // them.
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 6);

  __ Push(receiver());

  if (heap()->InNewSpace(callback->data())) {
    __ Mov(scratch3(), Operand(callback));
    __ Ldr(scratch3(), FieldMemOperand(scratch3(),
                                       ExecutableAccessorInfo::kDataOffset));
  } else {
    __ Mov(scratch3(), Operand(Handle<Object>(callback->data(), isolate())));
  }
  __ LoadRoot(scratch4(), Heap::kUndefinedValueRootIndex);
  __ Mov(scratch2(), Operand(ExternalReference::isolate_address(isolate())));
  __ Push(scratch3(), scratch4(), scratch4(), scratch2(), reg, name());

  Register args_addr = scratch2();
  __ Add(args_addr, __ StackPointer(), kPointerSize);

  // Stack at this point:
  //              sp[40] callback data
  //              sp[32] undefined
  //              sp[24] undefined
  //              sp[16] isolate
  // args_addr -> sp[8]  reg
  //              sp[0]  name

  // Abi for CallApiGetter.
  Register getter_address_reg = x2;

  // Set up the call.
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  ExternalReference ref = ExternalReference(&fun, type, isolate());
  __ Mov(getter_address_reg, ref);

  CallApiGetterStub stub;
  __ TailCallStub(&stub);
}


void LoadStubCompiler::GenerateLoadInterceptor(
    Register holder_reg,
    Handle<Object> object,
    Handle<JSObject> interceptor_holder,
    LookupResult* lookup,
    Handle<Name> name) {
  ASSERT(!AreAliased(receiver(), this->name(),
                     scratch1(), scratch2(), scratch3()));
  ASSERT(interceptor_holder->HasNamedInterceptor());
  ASSERT(!interceptor_holder->GetNamedInterceptor()->getter()->IsUndefined());

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added later.
  bool compile_followup_inline = false;
  if (lookup->IsFound() && lookup->IsCacheable()) {
    if (lookup->IsField()) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
               lookup->GetCallbackObject()->IsExecutableAccessorInfo()) {
      ExecutableAccessorInfo* callback =
          ExecutableAccessorInfo::cast(lookup->GetCallbackObject());
      compile_followup_inline = callback->getter() != NULL &&
          callback->IsCompatibleReceiver(*object);
    }
  }

  if (compile_followup_inline) {
    // Compile the interceptor call, followed by inline code to load the
    // property from further up the prototype chain if the call fails.
    // Check that the maps haven't changed.
    ASSERT(holder_reg.is(receiver()) || holder_reg.is(scratch1()));

    // Preserve the receiver register explicitly whenever it is different from
    // the holder and it is needed should the interceptor return without any
    // result. The CALLBACKS case needs the receiver to be passed into C++ code,
    // the FIELD case might cause a miss during the prototype check.
    bool must_perfrom_prototype_check = *interceptor_holder != lookup->holder();
    bool must_preserve_receiver_reg = !receiver().Is(holder_reg) &&
        (lookup->type() == CALLBACKS || must_perfrom_prototype_check);

    // Save necessary data before invoking an interceptor.
    // Requires a frame to make GC aware of pushed pointers.
    {
      FrameScope frame_scope(masm(), StackFrame::INTERNAL);
      if (must_preserve_receiver_reg) {
        __ Push(receiver(), holder_reg, this->name());
      } else {
        __ Push(holder_reg, this->name());
      }
      // Invoke an interceptor.  Note: map checks from receiver to
      // interceptor's holder has been compiled before (see a caller
      // of this method.)
      CompileCallLoadPropertyWithInterceptor(
          masm(), receiver(), holder_reg, this->name(), interceptor_holder,
          IC::kLoadPropertyWithInterceptorOnly);

      // Check if interceptor provided a value for property.  If it's
      // the case, return immediately.
      Label interceptor_failed;
      __ JumpIfRoot(x0,
                    Heap::kNoInterceptorResultSentinelRootIndex,
                    &interceptor_failed);
      frame_scope.GenerateLeaveFrame();
      __ Ret();

      __ Bind(&interceptor_failed);
      if (must_preserve_receiver_reg) {
        __ Pop(this->name(), holder_reg, receiver());
      } else {
        __ Pop(this->name(), holder_reg);
      }
      // Leave the internal frame.
    }
    GenerateLoadPostInterceptor(holder_reg, interceptor_holder, name, lookup);
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    PushInterceptorArguments(
        masm(), receiver(), holder_reg, this->name(), interceptor_holder);

    ExternalReference ref =
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForLoad),
                          isolate());
    __ TailCallExternalReference(ref, StubCache::kInterceptorArgsLength, 1);
  }
}


void StubCompiler::GenerateBooleanCheck(Register object, Label* miss) {
  UseScratchRegisterScope temps(masm());
  // Check that the object is a boolean.
  Register true_root = temps.AcquireX();
  Register false_root = temps.AcquireX();
  ASSERT(!AreAliased(object, true_root, false_root));
  __ LoadTrueFalseRoots(true_root, false_root);
  __ Cmp(object, true_root);
  __ Ccmp(object, false_root, ZFlag, ne);
  __ B(ne, miss);
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  ASM_LOCATION("StoreStubCompiler::CompileStoreCallback");
  Register holder_reg = HandlerFrontend(
      IC::CurrentTypeOf(object, isolate()), receiver(), holder, name);

  // Stub never generated for non-global objects that require access checks.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());

  // receiver() and holder_reg can alias.
  ASSERT(!AreAliased(receiver(), scratch1(), scratch2(), value()));
  ASSERT(!AreAliased(holder_reg, scratch1(), scratch2(), value()));
  __ Mov(scratch1(), Operand(callback));
  __ Mov(scratch2(), Operand(name));
  __ Push(receiver(), holder_reg, scratch1(), scratch2(), value());

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty), isolate());
  __ TailCallExternalReference(store_callback_property, 5, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


#undef __
#define __ ACCESS_MASM(masm)


void StoreStubCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm,
    Handle<HeapType> type,
    Register receiver,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ Push(value());

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ Ldr(receiver,
               FieldMemOperand(
                   receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ Push(receiver, value());
      ParameterCount actual(1);
      ParameterCount expected(setter);
      __ InvokeFunction(setter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ Pop(x0);

    // Restore context register.
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> object,
    Handle<Name> name) {
  Label miss;

  ASM_LOCATION("StoreStubCompiler::CompileStoreInterceptor");

  __ Push(receiver(), this->name(), value());

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<HeapType> type,
                                                      Handle<JSObject> last,
                                                      Handle<Name> name) {
  NonexistentHandlerFrontend(type, last, name);

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(x0, Heap::kUndefinedValueRootIndex);
  __ Ret();

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


// TODO(all): The so-called scratch registers are significant in some cases. For
// example, KeyedStoreStubCompiler::registers()[3] (x3) is actually used for
// KeyedStoreCompiler::transition_map(). We should verify which registers are
// actually scratch registers, and which are important. For now, we use the same
// assignments as ARM to remain on the safe side.

Register* LoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { x0, x2, x3, x1, x4, x5 };
  return registers;
}


Register* KeyedLoadStubCompiler::registers() {
  // receiver, name/key, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { x1, x0, x2, x3, x4, x5 };
  return registers;
}


Register StoreStubCompiler::value() {
  return x0;
}


Register* StoreStubCompiler::registers() {
  // receiver, value, scratch1, scratch2, scratch3.
  static Register registers[] = { x1, x2, x3, x4, x5 };
  return registers;
}


Register* KeyedStoreStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3.
  static Register registers[] = { x2, x1, x3, x4, x5 };
  return registers;
}


#undef __
#define __ ACCESS_MASM(masm)

void LoadStubCompiler::GenerateLoadViaGetter(MacroAssembler* masm,
                                             Handle<HeapType> type,
                                             Register receiver,
                                             Handle<JSFunction> getter) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ Ldr(receiver,
                FieldMemOperand(
                    receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ Push(receiver);
      ParameterCount actual(0);
      ParameterCount expected(getter);
      __ InvokeFunction(getter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> LoadStubCompiler::CompileLoadGlobal(
    Handle<HeapType> type,
    Handle<GlobalObject> global,
    Handle<PropertyCell> cell,
    Handle<Name> name,
    bool is_dont_delete) {
  Label miss;
  HandlerFrontendHeader(type, receiver(), global, name, &miss);

  // Get the value from the cell.
  __ Mov(x3, Operand(cell));
  __ Ldr(x4, FieldMemOperand(x3, Cell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ JumpIfRoot(x4, Heap::kTheHoleValueRootIndex, &miss);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, x1, x3);
  __ Mov(x0, x4);
  __ Ret();

  HandlerFrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, name);
}


Handle<Code> BaseLoadStoreStubCompiler::CompilePolymorphicIC(
    TypeHandleList* types,
    CodeHandleList* handlers,
    Handle<Name> name,
    Code::StubType type,
    IcCheckType check) {
  Label miss;

  if (check == PROPERTY &&
      (kind() == Code::KEYED_LOAD_IC || kind() == Code::KEYED_STORE_IC)) {
    __ CompareAndBranch(this->name(), Operand(name), ne, &miss);
  }

  Label number_case;
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  Register map_reg = scratch1();
  __ Ldr(map_reg, FieldMemOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = types->length();
  int number_of_handled_maps = 0;
  for (int current = 0; current < receiver_count; ++current) {
    Handle<HeapType> type = types->at(current);
    Handle<Map> map = IC::TypeToMap(*type, isolate());
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      Label try_next;
      __ Cmp(map_reg, Operand(map));
      __ B(ne, &try_next);
      if (type->Is(HeapType::Number())) {
        ASSERT(!number_case.is_unused());
        __ Bind(&number_case);
      }
      __ Jump(handlers->at(current), RelocInfo::CODE_TARGET);
      __ Bind(&try_next);
    }
  }
  ASSERT(number_of_handled_maps != 0);

  __ Bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      (number_of_handled_maps > 1) ? POLYMORPHIC : MONOMORPHIC;
  return GetICCode(kind(), type, name, state);
}


void StoreStubCompiler::GenerateStoreArrayLength() {
  // Prepare tail call to StoreIC_ArrayLength.
  __ Push(receiver(), value());

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kStoreIC_ArrayLength),
                        masm()->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;

  ASM_LOCATION("KeyedStoreStubCompiler::CompileStorePolymorphic");

  __ JumpIfSmi(receiver(), &miss);

  int receiver_count = receiver_maps->length();
  __ Ldr(scratch1(), FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_count; i++) {
    __ Cmp(scratch1(), Operand(receiver_maps->at(i)));

    Label skip;
    __ B(&skip, ne);
    if (!transitioned_maps->at(i).is_null()) {
      // This argument is used by the handler stub. For example, see
      // ElementsTransitionGenerator::GenerateMapChangeElementsTransition.
      __ Mov(transition_map(), Operand(transitioned_maps->at(i)));
    }
    __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET);
    __ Bind(&skip);
  }

  __ Bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  return GetICCode(
      kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


#undef __
#define __ ACCESS_MASM(masm)

void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Label slow, miss;

  Register result = x0;
  Register key = x0;
  Register receiver = x1;

  __ JumpIfNotSmi(key, &miss);
  __ Ldr(x4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadFromNumberDictionary(&slow, x4, key, result, x2, x3, x5, x6);
  __ Ret();

  __ Bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(), 1, x2, x3);
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  // Miss case, call the runtime.
  __ Bind(&miss);
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Miss);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
