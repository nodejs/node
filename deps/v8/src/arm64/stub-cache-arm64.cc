// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/ic-inl.h"
#include "src/stub-cache.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(!AreAliased(receiver, scratch0, scratch1));
  DCHECK(name->IsUniqueName());
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

  DCHECK(!AreAliased(name, offset, scratch, scratch2, scratch3));

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
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  DCHECK(!AreAliased(receiver, name, scratch, extra, extra2, extra3));

  // Make sure extra and extra2 registers are valid.
  DCHECK(!extra.is(no_reg));
  DCHECK(!extra2.is(no_reg));
  DCHECK(!extra3.is(no_reg));

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
  __ Ubfx(scratch, scratch, kCacheIndexShift,
          CountTrailingZeros(kPrimaryTableSize, 64));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, receiver, name,
      scratch, extra, extra2, extra3);

  // Primary miss: Compute hash for secondary table.
  __ Sub(scratch, scratch, Operand(name, LSR, kCacheIndexShift));
  __ Add(scratch, scratch, flags >> kCacheIndexShift);
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


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
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


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register scratch1,
    Register scratch2, Label* miss_label) {
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
void PropertyHandlerCompiler::GenerateCheckPropertyCell(
    MacroAssembler* masm, Handle<JSGlobalObject> global, Handle<Name> name,
    Register scratch, Label* miss) {
  Handle<Cell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
  DCHECK(cell->value()->IsTheHole());
  __ Mov(scratch, Operand(cell));
  __ Ldr(scratch, FieldMemOperand(scratch, Cell::kValueOffset));
  __ JumpIfNotRoot(scratch, Heap::kTheHoleValueRootIndex, miss);
}


static void PushInterceptorArguments(MacroAssembler* masm, Register receiver,
                                     Register holder, Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsInfoIndex == 1);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex == 2);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex == 3);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsLength == 4);

  __ Push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  DCHECK(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ Mov(scratch, Operand(interceptor));
  __ Push(scratch, receiver, holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm, Register receiver, Register holder, Register name,
    Handle<JSObject> holder_obj, IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  __ CallExternalReference(ExternalReference(IC_Utility(id), masm->isolate()),
                           NamedLoadHandlerCompiler::kInterceptorArgsLength);
}


// Generate call to api function.
void PropertyHandlerCompiler::GenerateFastApiCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch,
    bool is_store, int argc, Register* values) {
  DCHECK(!AreAliased(receiver, scratch));

  MacroAssembler::PushPopQueue queue(masm);
  queue.Queue(receiver);
  // Write the arguments to the stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc - 1 - i];
    DCHECK(!AreAliased(receiver, scratch, arg));
    queue.Queue(arg);
  }
  queue.PushQueued();

  DCHECK(optimization.is_simple_api_call());

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
  ExternalReference ref = ExternalReference(
      &fun, ExternalReference::DIRECT_API_CALL, masm->isolate());
  __ Mov(api_function_address, ref);

  // Jump to stub.
  CallApiFunctionStub stub(isolate, is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


void PropertyAccessCompiler::GenerateTailCall(MacroAssembler* masm,
                                              Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ Bind(label);
    __ Mov(this->name(), Operand(name));
  }
}


// Generate StoreTransition code, value is passed in x0 register.
// When leaving generated code after success, the receiver_reg and storage_reg
// may be clobbered. Upon branch to miss_label, the receiver and name registers
// have their original values.
void NamedStoreHandlerCompiler::GenerateStoreTransition(
    Handle<Map> transition, Handle<Name> name, Register receiver_reg,
    Register storage_reg, Register value_reg, Register scratch1,
    Register scratch2, Register scratch3, Label* miss_label, Label* slow) {
  Label exit;

  DCHECK(!AreAliased(receiver_reg, storage_reg, value_reg,
                     scratch1, scratch2, scratch3));

  // We don't need scratch3.
  scratch3 = NoReg;

  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), isolate());
    __ LoadObject(scratch1, constant);
    __ Cmp(value_reg, scratch1);
    __ B(ne, miss_label);
  } else if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
    HeapType* field_type = descriptors->GetFieldType(descriptor);
    HeapType::Iterator<Map> it = field_type->Classes();
    if (!it.Done()) {
      __ Ldr(scratch1, FieldMemOperand(value_reg, HeapObject::kMapOffset));
      Label do_store;
      while (true) {
        __ CompareMap(scratch1, it.Current());
        it.Advance();
        if (it.Done()) {
          __ B(ne, miss_label);
          break;
        }
        __ B(eq, &do_store);
      }
      __ Bind(&do_store);
    }
  } else if (representation.IsDouble()) {
    UseScratchRegisterScope temps(masm());
    DoubleRegister temp_double = temps.AcquireD();
    __ SmiUntagToDouble(temp_double, value_reg, kSpeculativeUntag);

    Label do_store;
    __ JumpIfSmi(value_reg, &do_store);

    __ CheckMap(value_reg, scratch1, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ Ldr(temp_double, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ Bind(&do_store);
    __ AllocateHeapNumber(storage_reg, slow, scratch1, scratch2, temp_double,
                          NoReg, MUTABLE);
  }

  // Stub never generated for objects that require access checks.
  DCHECK(!transition->is_access_check_needed());

  // Perform map transition for the receiver if necessary.
  if (details.type() == FIELD &&
      Map::cast(transition->GetBackPointer())->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ Mov(scratch1, Operand(transition));
    __ Push(receiver_reg, scratch1, value_reg);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          isolate()),
        3, 1);
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
    DCHECK(value_reg.is(x0));
    __ Ret();
    return;
  }

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= transition->inobject_properties();

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  Register prop_reg = representation.IsDouble() ? storage_reg : value_reg;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = transition->instance_size() + (index * kPointerSize);
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
  DCHECK(value_reg.is(x0));
  __ Ret();
}


void NamedStoreHandlerCompiler::GenerateStoreField(LookupResult* lookup,
                                                   Register value_reg,
                                                   Label* miss_label) {
  DCHECK(lookup->representation().IsHeapObject());
  __ JumpIfSmi(value_reg, miss_label);
  HeapType::Iterator<Map> it = lookup->GetFieldType()->Classes();
  __ Ldr(scratch1(), FieldMemOperand(value_reg, HeapObject::kMapOffset));
  Label do_store;
  while (true) {
    __ CompareMap(scratch1(), it.Current());
    it.Advance();
    if (it.Done()) {
      __ B(ne, miss_label);
      break;
    }
    __ B(eq, &do_store);
  }
  __ Bind(&do_store);

  StoreFieldStub stub(isolate(), lookup->GetFieldIndex(),
                      lookup->representation());
  GenerateTailCall(masm(), stub.GetCode());
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss,
    PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type(), isolate()));

  // object_reg and holder_reg registers can alias.
  DCHECK(!AreAliased(object_reg, scratch1, scratch2));
  DCHECK(!AreAliased(holder_reg, scratch1, scratch2));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type()->IsConstant()) {
    current = Handle<JSObject>::cast(type()->AsConstant()->Value());
  }
  Handle<JSObject> prototype = Handle<JSObject>::null();
  Handle<Map> current_map = receiver_map;
  Handle<Map> holder_map(holder()->map());
  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (!current_map.is_identical_to(holder_map)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    DCHECK(current_map->IsJSGlobalProxyMap() ||
           !current_map->is_access_check_needed());

    prototype = handle(JSObject::cast(current_map->prototype()));
    if (current_map->is_dictionary_map() &&
        !current_map->IsJSGlobalObjectMap()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      if (!name->IsUniqueName()) {
        DCHECK(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      DCHECK(current.is_null() ||
             (current->property_dictionary()->FindEntry(name) ==
              NameDictionary::kNotFound));

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ Ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ Ldr(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      // Two possible reasons for loading the prototype from the map:
      // (1) Can't store references to new space in code.
      // (2) Handler is shared for all receivers with the same prototype
      //     map (but not necessarily the same prototype instance).
      bool load_prototype_from_map =
          heap()->InNewSpace(*prototype) || depth == 1;
      Register map_reg = scratch1;
      __ Ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));

      if (depth != 1 || check == CHECK_ALL_MAPS) {
        __ CheckMap(map_reg, current_map, miss, DONT_DO_SMI_CHECK);
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      // This allows us to install generated handlers for accesses to the
      // global proxy (as opposed to using slow ICs). See corresponding code
      // in LookupForRead().
      if (current_map->IsJSGlobalProxyMap()) {
        UseScratchRegisterScope temps(masm());
        __ CheckAccessGlobalProxy(reg, scratch2, temps.AcquireX(), miss);
      } else if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(
            masm(), Handle<JSGlobalObject>::cast(current), name,
            scratch2, miss);
      }

      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (load_prototype_from_map) {
        __ Ldr(reg, FieldMemOperand(map_reg, Map::kPrototypeOffset));
      } else {
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
  DCHECK(current_map->IsJSGlobalProxyMap() ||
         !current_map->is_access_check_needed());
  if (current_map->IsJSGlobalProxyMap()) {
    __ CheckAccessGlobalProxy(reg, scratch1, scratch2, miss);
  }

  // Return the register containing the holder.
  return reg;
}


void NamedLoadHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    __ Bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    GenerateRestoreName(miss, name);
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ LoadObject(x0, value);
  __ Ret();
}


void NamedLoadHandlerCompiler::GenerateLoadCallback(
    Register reg, Handle<ExecutableAccessorInfo> callback) {
  DCHECK(!AreAliased(scratch2(), scratch3(), scratch4(), reg));

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

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg,
                                                       LookupResult* lookup,
                                                       Handle<Name> name) {
  DCHECK(!AreAliased(receiver(), this->name(),
                     scratch1(), scratch2(), scratch3()));
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added later.
  bool compile_followup_inline = false;
  if (lookup->IsFound() && lookup->IsCacheable()) {
    if (lookup->IsField()) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
               lookup->GetCallbackObject()->IsExecutableAccessorInfo()) {
      Handle<ExecutableAccessorInfo> callback(
          ExecutableAccessorInfo::cast(lookup->GetCallbackObject()));
      compile_followup_inline =
          callback->getter() != NULL &&
          ExecutableAccessorInfo::IsCompatibleReceiverType(isolate(), callback,
                                                           type());
    }
  }

  if (compile_followup_inline) {
    // Compile the interceptor call, followed by inline code to load the
    // property from further up the prototype chain if the call fails.
    // Check that the maps haven't changed.
    DCHECK(holder_reg.is(receiver()) || holder_reg.is(scratch1()));

    // Preserve the receiver register explicitly whenever it is different from
    // the holder and it is needed should the interceptor return without any
    // result. The CALLBACKS case needs the receiver to be passed into C++ code,
    // the FIELD case might cause a miss during the prototype check.
    bool must_perfrom_prototype_check = *holder() != lookup->holder();
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
          masm(), receiver(), holder_reg, this->name(), holder(),
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
    GenerateLoadPostInterceptor(holder_reg, name, lookup);
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                             holder());

    ExternalReference ref =
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptor),
                          isolate());
    __ TailCallExternalReference(
        ref, NamedLoadHandlerCompiler::kInterceptorArgsLength, 1);
  }
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  ASM_LOCATION("NamedStoreHandlerCompiler::CompileStoreCallback");
  Register holder_reg = Frontend(receiver(), name);

  // Stub never generated for non-global objects that require access checks.
  DCHECK(holder()->IsJSGlobalProxy() || !holder()->IsAccessCheckNeeded());

  // receiver() and holder_reg can alias.
  DCHECK(!AreAliased(receiver(), scratch1(), scratch2(), value()));
  DCHECK(!AreAliased(holder_reg, scratch1(), scratch2(), value()));
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


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<HeapType> type, Register receiver,
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
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
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


Handle<Code> NamedStoreHandlerCompiler::CompileStoreInterceptor(
    Handle<Name> name) {
  Label miss;

  ASM_LOCATION("NamedStoreHandlerCompiler::CompileStoreInterceptor");

  __ Push(receiver(), this->name(), value());

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property = ExternalReference(
      IC_Utility(IC::kStorePropertyWithInterceptor), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


// TODO(all): The so-called scratch registers are significant in some cases. For
// example, PropertyAccessCompiler::keyed_store_calling_convention()[3] (x3) is
// actually
// used for KeyedStoreCompiler::transition_map(). We should verify which
// registers are actually scratch registers, and which are important. For now,
// we use the same assignments as ARM to remain on the safe side.

Register* PropertyAccessCompiler::load_calling_convention() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  Register receiver = LoadIC::ReceiverRegister();
  Register name = LoadIC::NameRegister();
  static Register registers[] = { receiver, name, x3, x0, x4, x5 };
  return registers;
}


Register* PropertyAccessCompiler::store_calling_convention() {
  // receiver, value, scratch1, scratch2, scratch3.
  Register receiver = StoreIC::ReceiverRegister();
  Register name = StoreIC::NameRegister();
  DCHECK(x3.is(KeyedStoreIC::MapRegister()));
  static Register registers[] = { receiver, name, x3, x4, x5 };
  return registers;
}


Register NamedStoreHandlerCompiler::value() { return StoreIC::ValueRegister(); }


#undef __
#define __ ACCESS_MASM(masm)

void NamedLoadHandlerCompiler::GenerateLoadViaGetter(
    MacroAssembler* masm, Handle<HeapType> type, Register receiver,
    Handle<JSFunction> getter) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ Ldr(receiver,
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
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


Handle<Code> NamedLoadHandlerCompiler::CompileLoadGlobal(
    Handle<PropertyCell> cell, Handle<Name> name, bool is_configurable) {
  Label miss;
  FrontendHeader(receiver(), name, &miss);

  // Get the value from the cell.
  Register result = StoreIC::ValueRegister();
  __ Mov(result, Operand(cell));
  __ Ldr(result, FieldMemOperand(result, Cell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ JumpIfRoot(result, Heap::kTheHoleValueRootIndex, &miss);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, x1, x3);
  __ Ret();

  FrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, name);
}


Handle<Code> PropertyICCompiler::CompilePolymorphic(TypeHandleList* types,
                                                    CodeHandleList* handlers,
                                                    Handle<Name> name,
                                                    Code::StubType type,
                                                    IcCheckType check) {
  Label miss;

  if (check == PROPERTY &&
      (kind() == Code::KEYED_LOAD_IC || kind() == Code::KEYED_STORE_IC)) {
    // In case we are compiling an IC for dictionary loads and stores, just
    // check whether the name is unique.
    if (name.is_identical_to(isolate()->factory()->normal_ic_symbol())) {
      __ JumpIfNotUniqueName(this->name(), &miss);
    } else {
      __ CompareAndBranch(this->name(), Operand(name), ne, &miss);
    }
  }

  Label number_case;
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  // Polymorphic keyed stores may use the map register
  Register map_reg = scratch1();
  DCHECK(kind() != Code::KEYED_STORE_IC ||
         map_reg.is(KeyedStoreIC::MapRegister()));
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
        DCHECK(!number_case.is_unused());
        __ Bind(&number_case);
      }
      __ Jump(handlers->at(current), RelocInfo::CODE_TARGET);
      __ Bind(&try_next);
    }
  }
  DCHECK(number_of_handled_maps != 0);

  __ Bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      (number_of_handled_maps > 1) ? POLYMORPHIC : MONOMORPHIC;
  return GetCode(kind(), type, name, state);
}


Handle<Code> PropertyICCompiler::CompileKeyedStorePolymorphic(
    MapHandleList* receiver_maps, CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;

  ASM_LOCATION("PropertyICCompiler::CompileStorePolymorphic");

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

  return GetCode(kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


#undef __
#define __ ACCESS_MASM(masm)

void ElementHandlerCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // The return address is in lr.
  Label slow, miss;

  Register result = x0;
  Register key = LoadIC::NameRegister();
  Register receiver = LoadIC::ReceiverRegister();
  DCHECK(receiver.is(x1));
  DCHECK(key.is(x2));

  __ JumpIfNotSmi(key, &miss);
  __ Ldr(x4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadFromNumberDictionary(&slow, x4, key, result, x7, x3, x5, x6);
  __ Ret();

  __ Bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(), 1, x4, x3);
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  // Miss case, call the runtime.
  __ Bind(&miss);
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Miss);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
