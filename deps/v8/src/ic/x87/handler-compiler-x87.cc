// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {

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
      __ InvokeFunction(getter, expected, actual, CALL_FUNCTION,
                        NullCallWrapper());
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
  NameDictionaryLookupStub::GenerateNegativeLookup(masm, miss_label, &done,
                                                   properties, name, scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);
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
    Register arg = values[argc - 1 - i];
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
  Handle<JSObject> api_holder =
      optimization.LookupHolderOfExpectedType(receiver_map, &holder_lookup);
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
  Handle<PropertyCell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
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
      __ InvokeFunction(setter, expected, actual, CALL_FUNCTION,
                        NullCallWrapper());
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


static void PushInterceptorArguments(MacroAssembler* masm, Register receiver,
                                     Register holder, Register name,
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
    MacroAssembler* masm, Register receiver, Register holder, Register name,
    Handle<JSObject> holder_obj, IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(ExternalReference(IC_Utility(id), masm->isolate()),
                           NamedLoadHandlerCompiler::kInterceptorArgsLength);
}


static void StoreIC_PushArgs(MacroAssembler* masm) {
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();

  DCHECK(!ebx.is(receiver) && !ebx.is(name) && !ebx.is(value));

  __ pop(ebx);
  __ push(receiver);
  __ push(name);
  __ push(value);
  __ push(ebx);
}


void NamedStoreHandlerCompiler::GenerateSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(IC::kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void ElementHandlerCompiler::GenerateStoreSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(IC::kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
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


void NamedStoreHandlerCompiler::GenerateRestoreNameAndMap(
    Handle<Name> name, Handle<Map> transition) {
  __ mov(this->name(), Immediate(name));
  __ mov(StoreTransitionDescriptor::MapRegister(), Immediate(transition));
}


void NamedStoreHandlerCompiler::GenerateConstantCheck(Object* constant,
                                                      Register value_reg,
                                                      Label* miss_label) {
  __ CmpObject(value_reg, handle(constant, isolate()));
  __ j(not_equal, miss_label);
}


void NamedStoreHandlerCompiler::GenerateFieldTypeChecks(HeapType* field_type,
                                                        Register value_reg,
                                                        Label* miss_label) {
  __ JumpIfSmi(value_reg, miss_label);
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
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss,
    PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type(), isolate()));

  // Make sure there's no overlap between holder and object registers.
  DCHECK(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  DCHECK(!scratch2.is(object_reg) && !scratch2.is(holder_reg) &&
         !scratch2.is(scratch1));

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

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name, scratch1,
                                       scratch2);

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
        GenerateCheckPropertyCell(masm(), Handle<JSGlobalObject>::cast(current),
                                  name, scratch2, miss);
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
  Register getter_address = ApiGetterDescriptor::function_address();
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


void NamedLoadHandlerCompiler::GenerateLoadInterceptorWithFollowup(
    LookupIterator* it, Register holder_reg) {
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());

  // Compile the interceptor call, followed by inline code to load the
  // property from further up the prototype chain if the call fails.
  // Check that the maps haven't changed.
  DCHECK(holder_reg.is(receiver()) || holder_reg.is(scratch1()));

  // Preserve the receiver register explicitly whenever it is different from the
  // holder and it is needed should the interceptor return without any result.
  // The ACCESSOR case needs the receiver to be passed into C++ code, the FIELD
  // case might cause a miss during the prototype check.
  bool must_perform_prototype_check =
      !holder().is_identical_to(it->GetHolder<JSObject>());
  bool must_preserve_receiver_reg =
      !receiver().is(holder_reg) &&
      (it->state() == LookupIterator::ACCESSOR || must_perform_prototype_check);

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
      __ mov(receiver(), Immediate(bit_cast<int32_t>(kZapValue)));
      __ mov(holder_reg, Immediate(bit_cast<int32_t>(kZapValue)));
      __ mov(this->name(), Immediate(bit_cast<int32_t>(kZapValue)));
    }

    __ pop(this->name());
    __ pop(holder_reg);
    if (must_preserve_receiver_reg) {
      __ pop(receiver());
    }

    // Leave the internal frame.
  }

  GenerateLoadPostInterceptor(it, holder_reg);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg) {
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());
  // Call the runtime system to load the interceptor.
  __ pop(scratch2());  // save old return address
  PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                           holder());
  __ push(scratch2());  // restore old return address

  ExternalReference ref = ExternalReference(
      IC_Utility(IC::kLoadPropertyWithInterceptor), isolate());
  __ TailCallExternalReference(
      ref, NamedLoadHandlerCompiler::kInterceptorArgsLength, 1);
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


Register NamedStoreHandlerCompiler::value() {
  return StoreDescriptor::ValueRegister();
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadGlobal(
    Handle<PropertyCell> cell, Handle<Name> name, bool is_configurable) {
  Label miss;

  FrontendHeader(receiver(), name, &miss);
  // Get the value from the cell.
  Register result = StoreDescriptor::ValueRegister();
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


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X87
