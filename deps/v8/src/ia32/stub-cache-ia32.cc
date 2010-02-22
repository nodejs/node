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
  __ SmiTag(eax);
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
  __ SmiTag(eax);
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
  __ push(receiver);
  __ push(holder);
  __ push(name);
  InterceptorInfo* interceptor = holder_obj->GetNamedInterceptor();
  ASSERT(!Heap::InNewSpace(interceptor));
  __ mov(receiver, Immediate(Handle<Object>(interceptor)));
  __ push(receiver);
  __ push(FieldOperand(receiver, InterceptorInfo::kDataOffset));
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

  if (lookup->IsProperty() && lookup->IsCacheable()) {
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
    AccessorInfo* callback = NULL;
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
      __ TailCallRuntime(ref, 5, 1);

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
    __ TailCallRuntime(ref, 5, 1);
  }

 private:
  Register name_;
};


// Holds information about possible function call optimizations.
class CallOptimization BASE_EMBEDDED {
 public:
  explicit CallOptimization(LookupResult* lookup)
    : constant_function_(NULL),
      is_simple_api_call_(false),
      expected_receiver_type_(NULL),
      api_call_info_(NULL) {
    if (!lookup->IsProperty() || !lookup->IsCacheable()) return;

    // We only optimize constant function calls.
    if (lookup->type() != CONSTANT_FUNCTION) return;

    Initialize(lookup->GetConstantFunction());
  }

  explicit CallOptimization(JSFunction* function) {
    Initialize(function);
  }

  bool is_constant_call() const {
    return constant_function_ != NULL;
  }

  JSFunction* constant_function() const {
    ASSERT(constant_function_ != NULL);
    return constant_function_;
  }

  bool is_simple_api_call() const {
    return is_simple_api_call_;
  }

  FunctionTemplateInfo* expected_receiver_type() const {
    ASSERT(is_simple_api_call_);
    return expected_receiver_type_;
  }

  CallHandlerInfo* api_call_info() const {
    ASSERT(is_simple_api_call_);
    return api_call_info_;
  }

  // Returns the depth of the object having the expected type in the
  // prototype chain between the two arguments.
  int GetPrototypeDepthOfExpectedType(JSObject* object,
                                      JSObject* holder) const {
    ASSERT(is_simple_api_call_);
    if (expected_receiver_type_ == NULL) return 0;
    int depth = 0;
    while (object != holder) {
      if (object->IsInstanceOf(expected_receiver_type_)) return depth;
      object = JSObject::cast(object->GetPrototype());
      ++depth;
    }
    if (holder->IsInstanceOf(expected_receiver_type_)) return depth;
    return kInvalidProtoDepth;
  }

 private:
  void Initialize(JSFunction* function) {
    if (!function->is_compiled()) return;

    constant_function_ = function;
    is_simple_api_call_ = false;

    AnalyzePossibleApiFunction(function);
  }

  // Determines whether the given function can be called using the
  // fast api call builtin.
  void AnalyzePossibleApiFunction(JSFunction* function) {
    SharedFunctionInfo* sfi = function->shared();
    if (sfi->function_data()->IsUndefined()) return;
    FunctionTemplateInfo* info =
        FunctionTemplateInfo::cast(sfi->function_data());

    // Require a C++ callback.
    if (info->call_code()->IsUndefined()) return;
    api_call_info_ = CallHandlerInfo::cast(info->call_code());

    // Accept signatures that either have no restrictions at all or
    // only have restrictions on the receiver.
    if (!info->signature()->IsUndefined()) {
      SignatureInfo* signature = SignatureInfo::cast(info->signature());
      if (!signature->args()->IsUndefined()) return;
      if (!signature->receiver()->IsUndefined()) {
        expected_receiver_type_ =
            FunctionTemplateInfo::cast(signature->receiver());
      }
    }

    is_simple_api_call_ = true;
  }

  JSFunction* constant_function_;
  bool is_simple_api_call_;
  FunctionTemplateInfo* expected_receiver_type_;
  CallHandlerInfo* api_call_info_;
};


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
               Label* miss) {
    ASSERT(holder->HasNamedInterceptor());
    ASSERT(!holder->GetNamedInterceptor()->getter()->IsUndefined());

    // Check that the receiver isn't a smi.
    __ test(receiver, Immediate(kSmiTagMask));
    __ j(zero, miss, not_taken);

    CallOptimization optimization(lookup);

    if (optimization.is_constant_call() &&
        !Top::CanHaveSpecialFunctions(holder)) {
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
                        JSObject* holder_obj,
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
      depth1 = optimization.GetPrototypeDepthOfExpectedType(object, holder_obj);
      if (depth1 == kInvalidProtoDepth) {
        depth2 = optimization.GetPrototypeDepthOfExpectedType(holder_obj,
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

    Label miss_cleanup;
    Label* miss = can_do_fast_api_call ? &miss_cleanup : miss_label;
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, holder_obj,
                                        scratch1, scratch2, name,
                                        depth1, miss);

    Label regular_invoke;
    LoadWithInterceptor(masm, receiver, holder, holder_obj, &regular_invoke);

    // Generate code for the failed interceptor case.

    // Check the lookup is still valid.
    stub_compiler_->CheckPrototypes(holder_obj, receiver,
                                    lookup->holder(),
                                    scratch1, scratch2, name,
                                    depth2, miss);

    if (can_do_fast_api_call) {
      GenerateFastApiCall(masm, optimization, arguments_.immediate());
    } else {
      __ InvokeFunction(optimization.constant_function(), arguments_,
                        JUMP_FUNCTION);
    }

    if (can_do_fast_api_call) {
      __ bind(&miss_cleanup);
      FreeSpaceForFastApiCall(masm, scratch1);
      __ jmp(miss_label);
    }

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
                      JSObject* holder_obj,
                      Label* miss_label) {
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, holder_obj,
                                        scratch1, scratch2, name,
                                        miss_label);

    __ EnterInternalFrame();
    // Save the name_ register across the call.
    __ push(name_);

    PushInterceptorArguments(masm,
                             receiver,
                             holder,
                             name_,
                             holder_obj);

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
    __ TailCallRuntime(
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


#undef __
#define __ ACCESS_MASM(masm())


Register StubCompiler::CheckPrototypes(JSObject* object,
                                       Register object_reg,
                                       JSObject* holder,
                                       Register holder_reg,
                                       Register scratch,
                                       String* name,
                                       int push_at_depth,
                                       Label* miss) {
  // Check that the maps haven't changed.
  Register result =
      masm()->CheckMaps(object, object_reg, holder, holder_reg, scratch,
                        push_at_depth, miss);

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
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the maps haven't changed.
  Register reg =
      CheckPrototypes(object, receiver, holder,
                      scratch1, scratch2, name, miss);

  Handle<AccessorInfo> callback_handle(callback);

  Register other = reg.is(scratch1) ? scratch2 : scratch1;
  __ EnterInternalFrame();
  __ PushHandleScope(other);
  // Push the stack address where the list of arguments ends
  __ mov(other, esp);
  __ sub(Operand(other), Immediate(2 * kPointerSize));
  __ push(other);
  __ push(receiver);  // receiver
  __ push(reg);  // holder
  __ mov(other, Immediate(callback_handle));
  __ push(other);
  __ push(FieldOperand(other, AccessorInfo::kDataOffset));  // data
  __ push(name_reg);  // name
  // Save a pointer to where we pushed the arguments pointer.
  // This will be passed as the const Arguments& to the C++ callback.
  __ mov(eax, esp);
  __ add(Operand(eax), Immediate(5 * kPointerSize));
  __ mov(ebx, esp);

  // Do call through the api.
  ASSERT_EQ(6, ApiGetterEntryStub::kStackSpace);
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
  Register tmp = other.is(eax) ? reg : other;
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

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, edx, holder, ebx, eax, name, &miss);

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
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
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

  CallOptimization optimization(function);
  int depth = kInvalidProtoDepth;

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
                      ebx, eax, name, depth, &miss);

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
        __ mov(eax, FieldOperand(edx, HeapObject::kMapOffset));
        __ movzx_b(eax, FieldOperand(eax, Map::kInstanceTypeOffset));
        __ cmp(eax, FIRST_NONSTRING_TYPE);
        __ j(above_equal, &miss, not_taken);
        // Check that the maps starting from the prototype haven't changed.
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, name, &miss);
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
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::NUMBER_FUNCTION_INDEX,
                                            eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, name, &miss);
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
        GenerateLoadGlobalFunctionPrototype(masm(),
                                            Context::BOOLEAN_FUNCTION_INDEX,
                                            eax);
        CheckPrototypes(JSObject::cast(object->GetPrototype()), eax, holder,
                        ebx, edx, name, &miss);
      }
      break;
    }

    case JSARRAY_HAS_FAST_ELEMENTS_CHECK:
      CheckPrototypes(JSObject::cast(object), edx, holder,
                      ebx, eax, name, &miss);
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
  Handle<Code> ic = ComputeCallMiss(arguments().immediate());
  __ jmp(ic, RelocInfo::CODE_TARGET);

  // Return the generated code.
  String* function_name = NULL;
  if (function->shared()->name()->IsString()) {
    function_name = String::cast(function->shared()->name());
  }
  return GetCode(CONSTANT_FUNCTION, function_name);
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
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
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
  CheckPrototypes(object, edx, holder, ebx, eax, name, &miss);

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
  __ TailCallRuntime(store_callback_property, 4, 1);

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
  __ TailCallRuntime(store_ic_property, 3, 1);

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
  __ mov(edx, Operand(esp, 2 * kPointerSize));

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

  GenerateLoadField(object, holder, eax, ebx, edx, index, name, &miss);
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
  bool success = GenerateLoadCallback(object, holder, eax, ecx, ebx, edx,
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
  CheckPrototypes(object, eax, holder, ebx, edx, name, &miss);

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

  GenerateLoadField(receiver, holder, edx, ebx, ecx, index, name, &miss);

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
  bool success = GenerateLoadCallback(receiver, holder, edx, eax, ebx, ecx,
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

  GenerateLoadConstant(receiver, holder, edx, ebx, ecx,
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
