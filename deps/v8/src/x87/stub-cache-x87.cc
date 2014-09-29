// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/codegen.h"
#include "src/ic-inl.h"
#include "src/stub-cache.h"

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


void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(name->IsUniqueName());
  DCHECK(!receiver.is(scratch0));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1);

  __ mov(scratch0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ test_b(FieldOperand(scratch0, Map::kBitFieldOffset),
            kInterceptorOrAccessCheckNeededMask);
  __ j(not_zero, miss_label);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(scratch0, FIRST_SPEC_OBJECT_TYPE);
  __ j(below, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ mov(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ cmp(FieldOperand(properties, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->hash_table_map()));
  __ j(not_equal, miss_label);

  Label done;
  NameDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   properties,
                                                   name,
                                                   scratch1);
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
  DCHECK(sizeof(Entry) == 12);

  // Assert the flags do not name a specific type.
  DCHECK(Code::ExtractTypeFromFlags(flags) == 0);

  // Assert that there are no register conflicts.
  DCHECK(!scratch.is(receiver));
  DCHECK(!scratch.is(name));
  DCHECK(!extra.is(receiver));
  DCHECK(!extra.is(name));
  DCHECK(!extra.is(scratch));

  // Assert scratch and extra registers are valid, and extra2/3 are unused.
  DCHECK(!scratch.is(no_reg));
  DCHECK(extra2.is(no_reg));
  DCHECK(extra3.is(no_reg));

  Register offset = scratch;
  scratch = no_reg;

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ mov(offset, FieldOperand(name, Name::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  // ProbeTable expects the offset to be pointer scaled, which it is, because
  // the heap object tag size is 2 and the pointer size log 2 is also 2.
  DCHECK(kCacheIndexShift == kPointerSizeLog2);

  // Probe the primary table.
  ProbeTable(isolate(), masm, flags, kPrimary, name, receiver, offset, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(offset, FieldOperand(name, Name::kHashFieldOffset));
  __ add(offset, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(offset, flags);
  __ and_(offset, (kPrimaryTableSize - 1) << kCacheIndexShift);
  __ sub(offset, name);
  __ add(offset, Immediate(flags));
  __ and_(offset, (kSecondaryTableSize - 1) << kCacheIndexShift);

  // Probe the secondary table.
  ProbeTable(
      isolate(), masm, flags, kSecondary, name, receiver, offset, extra);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(masm->isolate()->native_context()->get(index)));
  // Check we're still in the same context.
  Register scratch = prototype;
  const int offset = Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX);
  __ mov(scratch, Operand(esi, offset));
  __ mov(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));
  __ cmp(Operand(scratch, Context::SlotOffset(index)), function);
  __ j(not_equal, miss);

  // Load its initial map. The global functions all have initial maps.
  __ Move(prototype, Immediate(Handle<Map>(function->initial_map())));
  // Load the prototype from the initial map.
  __ mov(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register scratch1,
    Register scratch2, Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(eax, scratch1);
  __ ret(0);
}


static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsInfoIndex == 1);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex == 2);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex == 3);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsLength == 4);
  __ push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  DCHECK(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ mov(scratch, Immediate(interceptor));
  __ push(scratch);
  __ push(receiver);
  __ push(holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj,
    IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(ExternalReference(IC_Utility(id), masm->isolate()),
                           NamedLoadHandlerCompiler::kInterceptorArgsLength);
}


// Generate call to api function.
// This function uses push() to generate smaller, faster code than
// the version above. It is an optimization that should will be removed
// when api call ICs are generated in hydrogen.
void PropertyHandlerCompiler::GenerateFastApiCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch_in,
    bool is_store, int argc, Register* values) {
  // Copy return value.
  __ pop(scratch_in);
  // receiver
  __ push(receiver);
  // Write the arguments to stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc-1-i];
    DCHECK(!receiver.is(arg));
    DCHECK(!scratch_in.is(arg));
    __ push(arg);
  }
  __ push(scratch_in);
  // Stack now matches JSFunction abi.
  DCHECK(optimization.is_simple_api_call());

  // Abi for CallApiFunctionStub.
  Register callee = eax;
  Register call_data = ebx;
  Register holder = ecx;
  Register api_function_address = edx;
  Register scratch = edi;  // scratch_in is no longer valid.

  // Put holder in place.
  CallOptimization::HolderLookup holder_lookup;
  Handle<JSObject> api_holder = optimization.LookupHolderOfExpectedType(
      receiver_map,
      &holder_lookup);
  switch (holder_lookup) {
    case CallOptimization::kHolderIsReceiver:
      __ Move(holder, receiver);
      break;
    case CallOptimization::kHolderFound:
      __ LoadHeapObject(holder, api_holder);
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
  __ LoadHeapObject(callee, function);

  bool call_data_undefined = false;
  // Put call_data in place.
  if (isolate->heap()->InNewSpace(*call_data_obj)) {
    __ mov(scratch, api_call_info);
    __ mov(call_data, FieldOperand(scratch, CallHandlerInfo::kDataOffset));
  } else if (call_data_obj->IsUndefined()) {
    call_data_undefined = true;
    __ mov(call_data, Immediate(isolate->factory()->undefined_value()));
  } else {
    __ mov(call_data, call_data_obj);
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  __ mov(api_function_address, Immediate(function_address));

  // Jump to stub.
  CallApiFunctionStub stub(isolate, is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
void PropertyHandlerCompiler::GenerateCheckPropertyCell(
    MacroAssembler* masm, Handle<JSGlobalObject> global, Handle<Name> name,
    Register scratch, Label* miss) {
  Handle<PropertyCell> cell =
      JSGlobalObject::EnsurePropertyCell(global, name);
  DCHECK(cell->value()->IsTheHole());
  Handle<Oddball> the_hole = masm->isolate()->factory()->the_hole_value();
  if (masm->serializer_enabled()) {
    __ mov(scratch, Immediate(cell));
    __ cmp(FieldOperand(scratch, PropertyCell::kValueOffset),
           Immediate(the_hole));
  } else {
    __ cmp(Operand::ForCell(cell), Immediate(the_hole));
  }
  __ j(not_equal, miss);
}


void PropertyAccessCompiler::GenerateTailCall(MacroAssembler* masm,
                                              Handle<Code> code) {
  __ jmp(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ mov(this->name(), Immediate(name));
  }
}


// Receiver_reg is preserved on jumps to miss_label, but may be destroyed if
// store is successful.
void NamedStoreHandlerCompiler::GenerateStoreTransition(
    Handle<Map> transition, Handle<Name> name, Register receiver_reg,
    Register storage_reg, Register value_reg, Register scratch1,
    Register scratch2, Register unused, Label* miss_label, Label* slow) {
  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), isolate());
    __ CmpObject(value_reg, constant);
    __ j(not_equal, miss_label);
  } else if (representation.IsSmi()) {
      __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
    HeapType* field_type = descriptors->GetFieldType(descriptor);
    HeapType::Iterator<Map> it = field_type->Classes();
    if (!it.Done()) {
      Label do_store;
      while (true) {
        __ CompareMap(value_reg, it.Current());
        it.Advance();
        if (it.Done()) {
          __ j(not_equal, miss_label);
          break;
        }
        __ j(equal, &do_store, Label::kNear);
      }
      __ bind(&do_store);
    }
  } else if (representation.IsDouble()) {
    Label do_store, heap_number;
    __ AllocateHeapNumber(storage_reg, scratch1, scratch2, slow, MUTABLE);

    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiUntag(value_reg);
    __ push(value_reg);
    __ fild_s(Operand(esp, 0));
    __ pop(value_reg);
    __ SmiTag(value_reg);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, isolate()->factory()->heap_number_map(), miss_label,
                DONT_DO_SMI_CHECK);
    __ fld_d(FieldOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ fstp_d(FieldOperand(storage_reg, HeapNumber::kValueOffset));
  }

  // Stub never generated for objects that require access checks.
  DCHECK(!transition->is_access_check_needed());

  // Perform map transition for the receiver if necessary.
  if (details.type() == FIELD &&
      Map::cast(transition->GetBackPointer())->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ pop(scratch1);  // Return address.
    __ push(receiver_reg);
    __ push(Immediate(transition));
    __ push(value_reg);
    __ push(scratch1);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          isolate()),
        3, 1);
    return;
  }

  // Update the map of the object.
  __ mov(scratch1, Immediate(transition));
  __ mov(FieldOperand(receiver_reg, HeapObject::kMapOffset), scratch1);

  // Update the write barrier for the map field.
  __ RecordWriteField(receiver_reg,
                      HeapObject::kMapOffset,
                      scratch1,
                      scratch2,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  if (details.type() == CONSTANT) {
    DCHECK(value_reg.is(eax));
    __ ret(0);
    return;
  }

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= transition->inobject_properties();

  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  // TODO(verwaest): Share this code as a code stub.
  if (index < 0) {
    // Set the property straight into the object.
    int offset = transition->instance_size() + (index * kPointerSize);
    if (representation.IsDouble()) {
      __ mov(FieldOperand(receiver_reg, offset), storage_reg);
    } else {
      __ mov(FieldOperand(receiver_reg, offset), value_reg);
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ mov(storage_reg, value_reg);
      }
      __ RecordWriteField(receiver_reg,
                          offset,
                          storage_reg,
                          scratch1,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ mov(scratch1, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    if (representation.IsDouble()) {
      __ mov(FieldOperand(scratch1, offset), storage_reg);
    } else {
      __ mov(FieldOperand(scratch1, offset), value_reg);
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ mov(storage_reg, value_reg);
      }
      __ RecordWriteField(scratch1,
                          offset,
                          storage_reg,
                          receiver_reg,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  }

  // Return the value (register eax).
  DCHECK(value_reg.is(eax));
  __ ret(0);
}


void NamedStoreHandlerCompiler::GenerateStoreField(LookupResult* lookup,
                                                   Register value_reg,
                                                   Label* miss_label) {
  DCHECK(lookup->representation().IsHeapObject());
  __ JumpIfSmi(value_reg, miss_label);
  HeapType::Iterator<Map> it = lookup->GetFieldType()->Classes();
  Label do_store;
  while (true) {
    __ CompareMap(value_reg, it.Current());
    it.Advance();
    if (it.Done()) {
      __ j(not_equal, miss_label);
      break;
    }
    __ j(equal, &do_store, Label::kNear);
  }
  __ bind(&do_store);

  StoreFieldStub stub(isolate(), lookup->GetFieldIndex(),
                      lookup->representation());
  GenerateTailCall(masm(), stub.GetCode());
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss,
    PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type(), isolate()));

  // Make sure there's no overlap between holder and object registers.
  DCHECK(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  DCHECK(!scratch2.is(object_reg) && !scratch2.is(holder_reg)
         && !scratch2.is(scratch1));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type()->IsConstant())
    current = Handle<JSObject>::cast(type()->AsConstant()->Value());
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
             current->property_dictionary()->FindEntry(name) ==
             NameDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool in_new_space = heap()->InNewSpace(*prototype);
      // Two possible reasons for loading the prototype from the map:
      // (1) Can't store references to new space in code.
      // (2) Handler is shared for all receivers with the same prototype
      //     map (but not necessarily the same prototype instance).
      bool load_prototype_from_map = in_new_space || depth == 1;
      if (depth != 1 || check == CHECK_ALL_MAPS) {
        __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK);
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      // This allows us to install generated handlers for accesses to the
      // global proxy (as opposed to using slow ICs). See corresponding code
      // in LookupForRead().
      if (current_map->IsJSGlobalProxyMap()) {
        __ CheckAccessGlobalProxy(reg, scratch1, scratch2, miss);
      } else if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(
            masm(), Handle<JSGlobalObject>::cast(current), name,
            scratch2, miss);
      }

      if (load_prototype_from_map) {
        // Save the map in scratch1 for later.
        __ mov(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }

      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (load_prototype_from_map) {
        __ mov(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
      } else {
        __ mov(reg, prototype);
      }
    }

    // Go to the next object in the prototype chain.
    current = prototype;
    current_map = handle(current->map());
  }

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  if (depth != 0 || check == CHECK_ALL_MAPS) {
    // Check the holder map.
    __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK);
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
    __ jmp(&success);
    __ bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    GenerateRestoreName(miss, name);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadCallback(
    Register reg, Handle<ExecutableAccessorInfo> callback) {
  // Insert additional parameters into the stack frame above return address.
  DCHECK(!scratch3().is(reg));
  __ pop(scratch3());  // Get return address to place it below.

  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 5);
  __ push(receiver());  // receiver
  // Push data from ExecutableAccessorInfo.
  if (isolate()->heap()->InNewSpace(callback->data())) {
    DCHECK(!scratch2().is(reg));
    __ mov(scratch2(), Immediate(callback));
    __ push(FieldOperand(scratch2(), ExecutableAccessorInfo::kDataOffset));
  } else {
    __ push(Immediate(Handle<Object>(callback->data(), isolate())));
  }
  __ push(Immediate(isolate()->factory()->undefined_value()));  // ReturnValue
  // ReturnValue default value
  __ push(Immediate(isolate()->factory()->undefined_value()));
  __ push(Immediate(reinterpret_cast<int>(isolate())));
  __ push(reg);  // holder

  // Save a pointer to where we pushed the arguments. This will be
  // passed as the const PropertyAccessorInfo& to the C++ callback.
  __ push(esp);

  __ push(name());  // name

  __ push(scratch3());  // Restore return address.

  // Abi for CallApiGetter
  Register getter_address = edx;
  Address function_address = v8::ToCData<Address>(callback->getter());
  __ mov(getter_address, Immediate(function_address));

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ LoadObject(eax, value);
  __ ret(0);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg,
                                                       LookupResult* lookup,
                                                       Handle<Name> name) {
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added
  // later.
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
    bool must_preserve_receiver_reg = !receiver().is(holder_reg) &&
        (lookup->type() == CALLBACKS || must_perfrom_prototype_check);

    // Save necessary data before invoking an interceptor.
    // Requires a frame to make GC aware of pushed pointers.
    {
      FrameScope frame_scope(masm(), StackFrame::INTERNAL);

      if (must_preserve_receiver_reg) {
        __ push(receiver());
      }
      __ push(holder_reg);
      __ push(this->name());

      // Invoke an interceptor.  Note: map checks from receiver to
      // interceptor's holder has been compiled before (see a caller
      // of this method.)
      CompileCallLoadPropertyWithInterceptor(
          masm(), receiver(), holder_reg, this->name(), holder(),
          IC::kLoadPropertyWithInterceptorOnly);

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
        __ mov(receiver(), Immediate(BitCast<int32_t>(kZapValue)));
        __ mov(holder_reg, Immediate(BitCast<int32_t>(kZapValue)));
        __ mov(this->name(), Immediate(BitCast<int32_t>(kZapValue)));
      }

      __ pop(this->name());
      __ pop(holder_reg);
      if (must_preserve_receiver_reg) {
        __ pop(receiver());
      }

      // Leave the internal frame.
    }

    GenerateLoadPostInterceptor(holder_reg, name, lookup);
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    __ pop(scratch2());  // save old return address
    PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                             holder());
    __ push(scratch2());  // restore old return address

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
  Register holder_reg = Frontend(receiver(), name);

  __ pop(scratch1());  // remove the return address
  __ push(receiver());
  __ push(holder_reg);
  __ Push(callback);
  __ Push(name);
  __ push(value());
  __ push(scratch1());  // restore return address

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
  //  -- esp[0] : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ push(value());

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ mov(receiver,
               FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
      }
      __ push(receiver);
      __ push(value());
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
    __ pop(eax);

    // Restore context register.
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> NamedStoreHandlerCompiler::CompileStoreInterceptor(
    Handle<Name> name) {
  __ pop(scratch1());  // remove the return address
  __ push(receiver());
  __ push(this->name());
  __ push(value());
  __ push(scratch1());  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property = ExternalReference(
      IC_Utility(IC::kStorePropertyWithInterceptor), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> PropertyICCompiler::CompileKeyedStorePolymorphic(
    MapHandleList* receiver_maps, CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;
  __ JumpIfSmi(receiver(), &miss, Label::kNear);
  __ mov(scratch1(), FieldOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_maps->length(); ++i) {
    __ cmp(scratch1(), receiver_maps->at(i));
    if (transitioned_maps->at(i).is_null()) {
      __ j(equal, handler_stubs->at(i));
    } else {
      Label next_map;
      __ j(not_equal, &next_map, Label::kNear);
      __ mov(transition_map(), Immediate(transitioned_maps->at(i)));
      __ jmp(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


Register* PropertyAccessCompiler::load_calling_convention() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  Register receiver = LoadIC::ReceiverRegister();
  Register name = LoadIC::NameRegister();
  static Register registers[] = { receiver, name, ebx, eax, edi, no_reg };
  return registers;
}


Register* PropertyAccessCompiler::store_calling_convention() {
  // receiver, name, scratch1, scratch2, scratch3.
  Register receiver = StoreIC::ReceiverRegister();
  Register name = StoreIC::NameRegister();
  DCHECK(ebx.is(KeyedStoreIC::MapRegister()));
  static Register registers[] = { receiver, name, ebx, edi, no_reg };
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
        __ mov(receiver,
               FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
      }
      __ push(receiver);
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
    __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> NamedLoadHandlerCompiler::CompileLoadGlobal(
    Handle<PropertyCell> cell, Handle<Name> name, bool is_configurable) {
  Label miss;

  FrontendHeader(receiver(), name, &miss);
  // Get the value from the cell.
  Register result = StoreIC::ValueRegister();
  if (masm()->serializer_enabled()) {
    __ mov(result, Immediate(cell));
    __ mov(result, FieldOperand(result, PropertyCell::kValueOffset));
  } else {
    __ mov(result, Operand::ForCell(cell));
  }

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ cmp(result, factory()->the_hole_value());
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ cmp(result, factory()->the_hole_value());
    __ Check(not_equal, kDontDeleteCellsCannotContainTheHole);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1);
  // The code above already loads the result into the return register.
  __ ret(0);

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
      __ cmp(this->name(), Immediate(name));
      __ j(not_equal, &miss);
    }
  }

  Label number_case;
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  // Polymorphic keyed stores may use the map register
  Register map_reg = scratch1();
  DCHECK(kind() != Code::KEYED_STORE_IC ||
         map_reg.is(KeyedStoreIC::MapRegister()));
  __ mov(map_reg, FieldOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = types->length();
  int number_of_handled_maps = 0;
  for (int current = 0; current < receiver_count; ++current) {
    Handle<HeapType> type = types->at(current);
    Handle<Map> map = IC::TypeToMap(*type, isolate());
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      __ cmp(map_reg, map);
      if (type->Is(HeapType::Number())) {
        DCHECK(!number_case.is_unused());
        __ bind(&number_case);
      }
      __ j(equal, handlers->at(current));
    }
  }
  DCHECK(number_of_handled_maps != 0);

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      number_of_handled_maps > 1 ? POLYMORPHIC : MONOMORPHIC;
  return GetCode(kind(), type, name, state);
}


#undef __
#define __ ACCESS_MASM(masm)


void ElementHandlerCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  DCHECK(edx.is(LoadIC::ReceiverRegister()));
  DCHECK(ecx.is(LoadIC::NameRegister()));
  Label slow, miss;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.
  __ JumpIfNotSmi(ecx, &miss);
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
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  __ bind(&miss);
  // ----------- S t a t e -------------
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Miss);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X87
