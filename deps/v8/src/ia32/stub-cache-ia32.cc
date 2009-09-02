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
  __ mov(scratch, FieldOperand(name, String::kLengthOffset));
  __ add(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xor_(scratch, flags);
  __ and_(scratch, (kPrimaryTableSize - 1) << kHeapObjectTagSize);

  // Probe the primary table.
  ProbeTable(masm, flags, kPrimary, name, scratch, extra);

  // Primary miss: Compute hash for secondary probe.
  __ mov(scratch, FieldOperand(name, String::kLengthOffset));
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


template <typename Pushable>
static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Pushable name,
                                     JSObject* holder_obj) {
  __ push(receiver);
  __ push(holder);
  __ push(name);
  InterceptorInfo* interceptor = holder_obj->GetNamedInterceptor();
  __ mov(receiver, Immediate(Handle<Object>(interceptor)));
  __ push(receiver);
  __ push(FieldOperand(receiver, InterceptorInfo::kDataOffset));
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
                                            Register scratch,
                                            Label* miss) {
  Label load_length, check_wrapper;

  // Check if the object is a string leaving the instance type in the
  // scratch register.
  GenerateStringCheck(masm, receiver, scratch, miss, &check_wrapper);

  // Load length directly from the string.
  __ bind(&load_length);
  __ and_(scratch, kStringSizeMask);
  __ mov(eax, FieldOperand(receiver, String::kLengthOffset));
  // ecx is also the receiver.
  __ lea(ecx, Operand(scratch, String::kLongLengthShift));
  __ shr(eax);  // ecx is implicit shift register.
  __ shl(eax, kSmiTagSize);
  __ ret(0);

  // Check if the object is a JSValue wrapper.
  __ bind(&check_wrapper);
  __ cmp(scratch, JS_VALUE_TYPE);
  __ j(not_equal, miss, not_taken);

  // Check if the wrapped value is a string and load the length
  // directly if it is.
  __ mov(receiver, FieldOperand(receiver, JSValue::kValueOffset));
  GenerateStringCheck(masm, receiver, scratch, miss, miss);
  __ jmp(&load_length);
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


template <class Pushable>
static void CompileCallLoadPropertyWithInterceptor(MacroAssembler* masm,
                                                   Register receiver,
                                                   Register holder,
                                                   Pushable name,
                                                   JSObject* holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly));
  __ mov(eax, Immediate(5));
  __ mov(ebx, Immediate(ref));

  CEntryStub stub;
  __ CallStub(&stub);
}


template <class Compiler>
static void CompileLoadInterceptor(Compiler* compiler,
                                   StubCompiler* stub_compiler,
                                   MacroAssembler* masm,
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
      stub_compiler->CheckPrototypes(object, receiver, holder,
                                     scratch1, scratch2, name, miss);

  if (lookup->IsValid() && lookup->IsCacheable()) {
    compiler->CompileCacheable(masm,
                               stub_compiler,
                               receiver,
                               reg,
                               scratch1,
                               scratch2,
                               holder,
                               lookup,
                               name,
                               miss);
  } else {
    compiler->CompileRegular(masm,
                             receiver,
                             reg,
                             scratch2,
                             holder,
                             miss);
  }
}


static void LookupPostInterceptor(JSObject* holder,
                                  String* name,
                                  LookupResult* lookup) {
  holder->LocalLookupRealNamedProperty(name, lookup);
  if (lookup->IsNotFound()) {
    Object* proto = holder->GetPrototype();
    if (proto != Heap::null_value()) {
      proto->Lookup(name, lookup);
    }
  }
}


class LoadInterceptorCompiler BASE_EMBEDDED {
 public:
  explicit LoadInterceptorCompiler(Register name) : name_(name) {}

  void CompileCacheable(MacroAssembler* masm,
                        StubCompiler* stub_compiler,
                        Register receiver,
                        Register holder,
                        Register scratch1,
                        Register scratch2,
                        JSObject* holder_obj,
                        LookupResult* lookup,
                        String* name,
                        Label* miss_label) {
    AccessorInfo* callback = 0;
    bool optimize = false;
    // So far the most popular follow ups for interceptor loads are FIELD
    // and CALLBACKS, so inline only them, other cases may be added
    // later.
    if (lookup->type() == FIELD) {
      optimize = true;
    } else if (lookup->type() == CALLBACKS) {
      Object* callback_object = lookup->GetCallbackObject();
      if (callback_object->IsAccessorInfo()) {
        callback = AccessorInfo::cast(callback_object);
        optimize = callback->getter() != NULL;
      }
    }

    if (!optimize) {
      CompileRegular(masm, receiver, holder, scratch2, holder_obj, miss_label);
      return;
    }

    // Note: starting a frame here makes GC aware of pointers pushed below.
    __ EnterInternalFrame();

    if (lookup->type() == CALLBACKS) {
      __ push(receiver);
    }
    __ push(holder);
    __ push(name_);

    CompileCallLoadPropertyWithInterceptor(masm,
                                           receiver,
                                           holder,
                                           name_,
                                           holder_obj);

    Label interceptor_failed;
    __ cmp(eax, Factory::no_interceptor_result_sentinel());
    __ j(equal, &interceptor_failed);
    __ LeaveInternalFrame();
    __ ret(0);

    __ bind(&interceptor_failed);
    __ pop(name_);
    __ pop(holder);
    if (lookup->type() == CALLBACKS) {
      __ pop(receiver);
    }

    __ LeaveInternalFrame();

    if (lookup->type() == FIELD) {
      holder = stub_compiler->CheckPrototypes(holder_obj, holder,
                                              lookup->holder(), scratch1,
                                              scratch2,
                                              name,
                                              miss_label);
      stub_compiler->GenerateFastPropertyLoad(masm, eax,
                                              holder, lookup->holder(),
                                              lookup->GetFieldIndex());
      __ ret(0);
    } else {
      ASSERT(lookup->type() == CALLBACKS);
      ASSERT(lookup->GetCallbackObject()->IsAccessorInfo());
      ASSERT(callback != NULL);
      ASSERT(callback->getter() != NULL);

      Label cleanup;
      __ pop(scratch2);
      __ push(receiver);
      __ push(scratch2);

      holder = stub_compiler->CheckPrototypes(holder_obj, holder,
                                              lookup->holder(), scratch1,
                                              scratch2,
                                              name,
                                              &cleanup);

      __ pop(scratch2);  // save old return address
      __ push(holder);
      __ mov(holder, Immediate(Handle<AccessorInfo>(callback)));
      __ push(holder);
      __ push(FieldOperand(holder, AccessorInfo::kDataOffset));
      __ push(name_);
      __ push(scratch2);  // restore old return address

      ExternalReference ref =
          ExternalReference(IC_Utility(IC::kLoadCallbackProperty));
      __ TailCallRuntime(ref, 5);

      __ bind(&cleanup);
      __ pop(scratch1);
      __ pop(scratch2);
      __ push(scratch1);
    }
  }


  void CompileRegular(MacroAssembler* masm,
                      Register receiver,
                      Register holder,
                      Register scratch,
                      JSObject* holder_obj,
                      Label* miss_label) {
    __ pop(scratch);  // save old return address
    PushInterceptorArguments(masm, receiver, holder, name_, holder_obj);
    __ push(scratch);  // restore old return address

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad));
    __ TailCallRuntime(ref, 5);
  }

 private:
  Register name_;
};


class CallInterceptorCompiler BASE_EMBEDDED {
 public:
  explicit CallInterceptorCompiler(const ParameterCount& arguments)
      : arguments_(arguments), argc_(arguments.immediate()) {}

  void CompileCacheable(MacroAssembler* masm,
                        StubCompiler* stub_compiler,
                        Register receiver,
                        Register holder,
                        Register scratch1,
                        Register scratch2,
                        JSObject* holder_obj,
                        LookupResult* lookup,
                        String* name,
                        Label* miss_label) {
    JSFunction* function = 0;
    bool optimize = false;
    // So far the most popular case for failed interceptor is
    // CONSTANT_FUNCTION sitting below.
    if (lookup->type() == CONSTANT_FUNCTION) {
      function = lookup->GetConstantFunction();
      // JSArray holder is a special case for call constant function
      // (see the corresponding code).
      if (function->is_compiled() && !holder_obj->IsJSArray()) {
        optimize = true;
      }
    }

    if (!optimize) {
      CompileRegular(masm, receiver, holder, scratch2, holder_obj, miss_label);
      return;
    }

    __ EnterInternalFrame();
    __ push(holder);  // save the holder

    CompileCallLoadPropertyWithInterceptor(
        masm,
        receiver,
        holder,
        // Under EnterInternalFrame this refers to name.
        Operand(ebp, (argc_ + 3) * kPointerSize),
        holder_obj);

    __ pop(receiver);  // restore holder
    __ LeaveInternalFrame();

    __ cmp(eax, Factory::no_interceptor_result_sentinel());
    Label invoke;
    __ j(not_equal, &invoke);

    stub_compiler->CheckPrototypes(holder_obj, receiver,
                                   lookup->holder(), scratch1,
                                   scratch2,
                                   name,
                                   miss_label);
    if (lookup->holder()->IsGlobalObject()) {
      __ mov(edx, Operand(esp, (argc_ + 1) * kPointerSize));
      __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
      __ mov(Operand(esp, (argc_ + 1) * kPointerSize), edx);
    }

    ASSERT(function->is_compiled());
    // Get the function and setup the context.
    __ mov(edi, Immediate(Handle<JSFunction>(function)));
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

    // Jump to the cached code (tail call).
    ASSERT(function->is_compiled());
    Handle<Code> code(function->code());
    ParameterCount expected(function->shared()->formal_parameter_count());
    __ InvokeCode(code, expected, arguments_,
                  RelocInfo::CODE_TARGET, JUMP_FUNCTION);

    __ bind(&invoke);
  }

  void CompileRegular(MacroAssembler* masm,
                      Register receiver,
                      Register holder,
                      Register scratch,
                      JSObject* holder_obj,
                      Label* miss_label) {
    __ EnterInternalFrame();

    PushInterceptorArguments(masm,
                             receiver,
                             holder,
                             Operand(ebp, (argc_ + 3) * kPointerSize),
                             holder_obj);

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForCall));
    __ mov(eax, Immediate(5));
    __ mov(ebx, Immediate(ref));

    CEntryStub stub;
    __ CallStub(&stub);

    __ LeaveInternalFrame();
  }

 private:
  const ParameterCount& arguments_;
  int argc_;
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


void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      Builtins::Name storage_extend,
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
    __ mov(ecx, Immediate(Handle<Map>(transition)));
    Handle<Code> ic(Builtins::builtin(storage_extend));
    __ jmp(ic, RelocInfo::CODE_TARGET);
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


#undef __
#define __ ACCESS_MASM(masm())


Register StubCompiler::CheckPrototypes(JSObject* object,
                                       Register object_reg,
                                       JSObject* holder,
                                       Register holder_reg,
                                       Register scratch,
                                       String* name,
                                       Label* miss) {
  // Check that the maps haven't changed.
  Register result =
      masm()->CheckMaps(object, object_reg, holder, holder_reg, scratch, miss);

  // If we've skipped any global objects, it's not enough to verify
  // that their maps haven't changed.
  while (object != holder) {
    if (object->IsGlobalObject()) {
      GlobalObject* global = GlobalObject::cast(object);
      Object* probe = global->EnsurePropertyCell(name);
      if (probe->IsFailure()) {
        set_failure(Failure::cast(probe));
        return result;
      }
      JSGlobalPropertyCell* cell = JSGlobalPropertyCell::cast(probe);
      ASSERT(cell->value()->IsTheHole());
      __ mov(scratch, Immediate(Handle<Object>(cell)));
      __ cmp(FieldOperand(scratch, JSGlobalPropertyCell::kValueOffset),
             Immediate(Factory::the_hole_value()));
      __ j(not_equal, miss, not_taken);
    }
    object = JSObject::cast(object->GetPrototype());
  }

  // Return the register containin the holder.
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check the prototype chain.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Get the value from the properties.
  GenerateFastPropertyLoad(masm(), eax, reg, holder, index);
  __ ret(0);
}


void StubCompiler::GenerateLoadCallback(JSObject* object,
                                        JSObject* holder,
                                        Register receiver,
                                        Register name_reg,
                                        Register scratch1,
                                        Register scratch2,
                                        AccessorInfo* callback,
                                        String* name,
                                        Label* miss) {
  // Check that the receiver isn't a smi.
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Push the arguments on the JS stack of the caller.
  __ pop(scratch2);  // remove return address
  __ push(receiver);  // receiver
  __ push(reg);  // holder
  __ mov(reg, Immediate(Handle<AccessorInfo>(callback)));  // callback data
  __ push(reg);
  __ push(FieldOperand(reg, AccessorInfo::kDataOffset));
  __ push(name_reg);  // name
  __ push(scratch2);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference load_callback_property =
      ExternalReference(IC_Utility(IC::kLoadCallbackProperty));
  __ TailCallRuntime(load_callback_property, 5);
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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  // Return the constant value.
  __ mov(eax, Handle<Object>(value));
  __ ret(0);
}


void StubCompiler::GenerateLoadInterceptor(JSObject* object,
                                           JSObject* holder,
                                           LookupResult* lookup,
                                           Register receiver,
                                           Register name_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           String* name,
                                           Label* miss) {
  LoadInterceptorCompiler compiler(name_reg);
  CompileLoadInterceptor(&compiler,
                         this,
                         masm(),
                         object,
                         holder,
                         name,
                         lookup,
                         receiver,
                         scratch1,
                         scratch2,
                         miss);
}


// TODO(1241006): Avoid having lazy compile stubs specialized by the
// number of arguments. It is not needed anymore.
Object* StubCompiler::CompileLazyCompile(Code::Flags flags) {
  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push a copy of the function onto the stack.
  __ push(edi);

  __ push(edi);  // function is also the parameter to the runtime call
  __ CallRuntime(Runtime::kLazyCompile, 1);
  __ pop(edi);

  // Tear down temporary frame.
  __ LeaveInternalFrame();

  // Do a tail-call of the compiled function.
  __ lea(ecx, FieldOperand(eax, Code::kHeaderSize));
  __ jmp(Operand(ecx));

  return GetCodeWithFlags(flags, "LazyCompileStub");
}


Object* CallStubCompiler::CompileCallField(Object* object,
                                           JSObject* holder,
                                           int index,
                                           String* name) {
  // ----------- S t a t e -------------
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Do the right check and compute the holder register.
  Register reg =
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, ecx, name, &miss);

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
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(FIELD, name);
}


Object* CallStubCompiler::CompileCallConstant(Object* object,
                                              JSObject* holder,
                                              JSFunction* function,
                                              String* name,
                                              CheckType check) {
  // ----------- S t a t e -------------
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ test(edx, Immediate(kSmiTagMask));
    __ j(zero, &miss, not_taken);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);

  switch (check) {
    case RECEIVER_MAP_CHECK:
      // Check that the maps haven't changed.
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, ecx, name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
        __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
      }
      break;

    case STRING_CHECK:
      // Check that the object is a two-byte string or a symbol.
      __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
      __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
      __ cmp(ecx, FIRST_NONSTRING_TYPE);
      __ j(above_equal, &miss, not_taken);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::STRING_FUNCTION_INDEX,
                                          ecx);
      CheckPrototypes(JSObject::cast(object->GetPrototype()), ecx, holder,
                      ebx, edx, name, &miss);
      break;

    case NUMBER_CHECK: {
      Label fast;
      // Check that the object is a smi or a heap number.
      __ test(edx, Immediate(kSmiTagMask));
      __ j(zero, &fast, taken);
      __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ecx);
      __ j(not_equal, &miss, not_taken);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::NUMBER_FUNCTION_INDEX,
                                          ecx);
      CheckPrototypes(JSObject::cast(object->GetPrototype()), ecx, holder,
                      ebx, edx, name, &miss);
      break;
    }

    case BOOLEAN_CHECK: {
      Label fast;
      // Check that the object is a boolean.
      __ cmp(edx, Factory::true_value());
      __ j(equal, &fast, taken);
      __ cmp(edx, Factory::false_value());
      __ j(not_equal, &miss, not_taken);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateLoadGlobalFunctionPrototype(masm(),
                                          Context::BOOLEAN_FUNCTION_INDEX,
                                          ecx);
      CheckPrototypes(JSObject::cast(object->GetPrototype()), ecx, holder,
                      ebx, edx, name, &miss);
      break;
    }

    case JSARRAY_HAS_FAST_ELEMENTS_CHECK:
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, ecx, name, &miss);
      // Make sure object->HasFastElements().
      // Get the elements array of the object.
      __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
      // Check that the object is in fast mode (not dictionary).
      __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
             Immediate(Factory::fixed_array_map()));
      __ j(not_equal, &miss, not_taken);
      break;

    default:
      UNREACHABLE();
  }

  // Get the function and setup the context.
  __ mov(edi, Immediate(Handle<JSFunction>(function)));
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  ASSERT(function->is_compiled());
  Handle<Code> code(function->code());
  ParameterCount expected(function->shared()->formal_parameter_count());
  __ InvokeCode(code, expected, arguments(),
                RelocInfo::CODE_TARGET, JUMP_FUNCTION);

  // Handle call cache miss.
  __ bind(&miss);
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  String* function_name = NULL;
  if (function->shared()->name()->IsString()) {
    function_name = String::cast(function->shared()->name());
  }
  return GetCode(CONSTANT_FUNCTION, function_name);
}


Object* CallStubCompiler::CompileCallInterceptor(Object* object,
                                                 JSObject* holder,
                                                 String* name) {
  // ----------- S t a t e -------------
  // -----------------------------------
  Label miss;

  // Get the number of arguments.
  const int argc = arguments().immediate();

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  CallInterceptorCompiler compiler(arguments());
  CompileLoadInterceptor(&compiler,
                         this,
                         masm(),
                         JSObject::cast(object),
                         holder,
                         name,
                         &lookup,
                         edx,
                         ebx,
                         ecx,
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
  Handle<Code> ic = ComputeCallMiss(argc);
  __ jmp(ic, RelocInfo::CODE_TARGET);

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
  Label miss;

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
  CheckPrototypes(object, edx, holder, ebx, ecx, name, &miss);

  // Get the value from the cell.
  __ mov(edi, Immediate(Handle<JSGlobalPropertyCell>(cell)));
  __ mov(edi, FieldOperand(edi, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  __ cmp(Operand(edi), Immediate(Handle<JSFunction>(function)));
  __ j(not_equal, &miss, not_taken);

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
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

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
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  // Get the object from the stack.
  __ mov(ebx, Operand(esp, 1 * kPointerSize));

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(),
                     Builtins::StoreIC_ExtendStorage,
                     object,
                     index,
                     transition,
                     ebx, ecx, edx,
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
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  // Get the object from the stack.
  __ mov(ebx, Operand(esp, 1 * kPointerSize));

  // Check that the object isn't a smi.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(object->map())));
  __ j(not_equal, &miss, not_taken);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(ebx, edx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ pop(ebx);  // remove the return address
  __ push(Operand(esp, 0));  // receiver
  __ push(Immediate(Handle<AccessorInfo>(callback)));  // callback info
  __ push(ecx);  // name
  __ push(eax);  // value
  __ push(ebx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty));
  __ TailCallRuntime(store_callback_property, 4);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(ecx, Immediate(Handle<String>(name)));  // restore name
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
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  // Get the object from the stack.
  __ mov(ebx, Operand(esp, 1 * kPointerSize));

  // Check that the object isn't a smi.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the map of the object hasn't changed.
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(Handle<Map>(receiver->map())));
  __ j(not_equal, &miss, not_taken);

  // Perform global security token check if needed.
  if (receiver->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(ebx, edx, &miss);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(receiver->IsJSGlobalProxy() || !receiver->IsAccessCheckNeeded());

  __ pop(ebx);  // remove the return address
  __ push(Operand(esp, 0));  // receiver
  __ push(ecx);  // name
  __ push(eax);  // value
  __ push(ebx);  // restore return address

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty));
  __ TailCallRuntime(store_ic_property, 3);

  // Handle store cache miss.
  __ bind(&miss);
  __ mov(ecx, Immediate(Handle<String>(name)));  // restore name
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
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  // Check that the map of the global has not changed.
  __ mov(ebx, Operand(esp, kPointerSize));
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
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
  //  -- esp[0] : return address
  //  -- esp[4] : key
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ IncrementCounter(&Counters::keyed_store_field, 1);

  // Get the name from the stack.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  // Check that the name has not changed.
  __ cmp(Operand(ecx), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  // Get the object from the stack.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));

  // Generate store field code.  Trashes the name register.
  GenerateStoreField(masm(),
                     Builtins::KeyedStoreIC_ExtendStorage,
                     object,
                     index,
                     transition,
                     ebx, ecx, edx,
                     &miss);

  // Handle store cache miss.
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_store_field, 1);
  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Miss));
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode(transition == NULL ? FIELD : MAP_TRANSITION, name);
}



Object* LoadStubCompiler::CompileLoadField(JSObject* object,
                                           JSObject* holder,
                                           int index,
                                           String* name) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  GenerateLoadField(object, holder, eax, ebx, edx, index, name, &miss);
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
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  GenerateLoadCallback(object, holder, eax, ecx, ebx, edx,
                       callback, name, &miss);
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
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  GenerateLoadConstant(object, holder, eax, ebx, edx, value, name, &miss);
  __ bind(&miss);
  GenerateLoadMiss(masm(), Code::LOAD_IC);

  // Return the generated code.
  return GetCode(CONSTANT_FUNCTION, name);
}


Object* LoadStubCompiler::CompileLoadInterceptor(JSObject* receiver,
                                                 JSObject* holder,
                                                 String* name) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);

  __ mov(eax, Operand(esp, kPointerSize));
  // TODO(368): Compile in the whole chain: all the interceptors in
  // prototypes and ultimate answer.
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          eax,
                          ecx,
                          edx,
                          ebx,
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
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------
  Label miss;

  // Get the receiver from the stack.
  __ mov(eax, Operand(esp, kPointerSize));

  // If the object is the holder then we know that it's a global
  // object which can only happen for contextual loads. In this case,
  // the receiver cannot be a smi.
  if (object != holder) {
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &miss, not_taken);
  }

  // Check that the maps haven't changed.
  CheckPrototypes(object, eax, holder, ebx, edx, name, &miss);

  // Get the value from the cell.
  __ mov(eax, Immediate(Handle<JSGlobalPropertyCell>(cell)));
  __ mov(eax, FieldOperand(eax, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ cmp(eax, Factory::the_hole_value());
    __ j(equal, &miss, not_taken);
  } else if (FLAG_debug_code) {
    __ cmp(eax, Factory::the_hole_value());
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


Object* KeyedLoadStubCompiler::CompileLoadField(String* name,
                                                JSObject* receiver,
                                                JSObject* holder,
                                                int index) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_field, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadField(receiver, holder, ecx, ebx, edx, index, name, &miss);

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
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_callback, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadCallback(receiver, holder, ecx, eax, ebx, edx,
                       callback, name, &miss);
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
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_constant_function, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadConstant(receiver, holder, ecx, ebx, edx,
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
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_interceptor, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  LookupResult lookup;
  LookupPostInterceptor(holder, name, &lookup);
  GenerateLoadInterceptor(receiver,
                          holder,
                          &lookup,
                          ecx,
                          eax,
                          edx,
                          ebx,
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
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_array_length, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadArrayLength(masm(), ecx, edx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_array_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadStringLength(String* name) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_string_length, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadStringLength(masm(), ecx, edx, &miss);
  __ bind(&miss);
  __ DecrementCounter(&Counters::keyed_load_string_length, 1);
  GenerateLoadMiss(masm(), Code::KEYED_LOAD_IC);

  // Return the generated code.
  return GetCode(CALLBACKS, name);
}


Object* KeyedLoadStubCompiler::CompileLoadFunctionPrototype(String* name) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));
  __ IncrementCounter(&Counters::keyed_load_function_prototype, 1);

  // Check that the name has not changed.
  __ cmp(Operand(eax), Immediate(Handle<String>(name)));
  __ j(not_equal, &miss, not_taken);

  GenerateLoadFunctionPrototype(masm(), ecx, edx, ebx, &miss);
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
  // Make sure that the maximum heap object size will never cause us
  // problems here.
  ASSERT(Heap::MaxObjectSizeInPagedSpace() >= JSObject::kMaxInstanceSize);
  __ AllocateObjectInNewSpace(ecx, edx, ecx, no_reg, &generic_stub_call, false);

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
      Label not_passed;
      // Set the property to undefined.
      __ mov(Operand(edx, i * kPointerSize), edi);
      // Check if the argument assigned to the property is actually passed.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ cmp(eax, arg_number);
      __ j(below_equal, &not_passed);
      // Argument passed - find it on the stack.
      __ mov(ebx, Operand(ecx, arg_number * -kPointerSize));
      __ mov(Operand(edx, i * kPointerSize), ebx);
      __ bind(&not_passed);
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
