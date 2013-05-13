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

#if defined(V8_TARGET_ARCH_IA32)

#include "bootstrapper.h"
#include "builtins-decls.h"
#include "code-stubs.h"
#include "isolate.h"
#include "jsregexp.h"
#include "regexp-macro-assembler.h"
#include "runtime.h"
#include "stub-cache.h"
#include "codegen.h"
#include "runtime.h"

namespace v8 {
namespace internal {


void FastCloneShallowArrayStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx, ecx };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->stack_parameter_count_ = NULL;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateArrayLiteralShallow)->entry;
}


void FastCloneShallowObjectStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx, ecx, edx };
  descriptor->register_param_count_ = 4;
  descriptor->register_params_ = registers;
  descriptor->stack_parameter_count_ = NULL;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateObjectLiteralShallow)->entry;
}


void KeyedLoadFastElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, ecx };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->stack_parameter_count_ = NULL;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure);
}


void LoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->stack_parameter_count_ = NULL;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedLoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->stack_parameter_count_ = NULL;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedStoreFastElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, ecx, eax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedStoreIC_MissFromStubFailure);
}


void TransitionElementsKindStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kTransitionElementsKind)->entry;
}


static void InitializeArrayConstructorDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // eax -- number of arguments
  // edi -- function
  // ebx -- type info cell with elements kind
  static Register registers[] = { edi, ebx };
  descriptor->register_param_count_ = 2;

  if (constant_stack_parameter_count != 0) {
    // stack param count needs (constructor pointer, and single argument)
    descriptor->stack_parameter_count_ = &eax;
  }
  descriptor->hint_stack_parameter_count_ = constant_stack_parameter_count;
  descriptor->register_params_ = registers;
  descriptor->function_mode_ = JS_FUNCTION_STUB_MODE;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(ArrayConstructor_StubFailure);
}


void ArrayNoArgumentConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate, descriptor, 0);
}


void ArraySingleArgumentConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate, descriptor, 1);
}


void ArrayNArgumentsConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(isolate, descriptor, -1);
}


void CompareNilICStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(CompareNilIC_Miss);
  descriptor->miss_handler_ =
      ExternalReference(IC_Utility(IC::kCompareNilIC_Miss), isolate);
}


#define __ ACCESS_MASM(masm)


void HydrogenCodeStub::GenerateLightweightMiss(MacroAssembler* masm) {
  // Update the static counter each time a new code stub is generated.
  Isolate* isolate = masm->isolate();
  isolate->counters()->code_stubs()->Increment();

  CodeStubInterfaceDescriptor* descriptor = GetInterfaceDescriptor(isolate);
  int param_count = descriptor->register_param_count_;
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    ASSERT(descriptor->register_param_count_ == 0 ||
           eax.is(descriptor->register_params_[param_count - 1]));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor->register_params_[i]);
    }
    ExternalReference miss = descriptor->miss_handler_;
    __ CallExternalReference(miss, descriptor->register_param_count_);
  }

  __ ret(0);
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in eax.
  Label check_heap_number, call_builtin;
  __ JumpIfNotSmi(eax, &check_heap_number, Label::kNear);
  __ ret(0);

  __ bind(&check_heap_number);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  Factory* factory = masm->isolate()->factory();
  __ cmp(ebx, Immediate(factory->heap_number_map()));
  __ j(not_equal, &call_builtin, Label::kNear);
  __ ret(0);

  __ bind(&call_builtin);
  __ pop(ecx);  // Pop return address.
  __ push(eax);
  __ push(ecx);  // Push return address.
  __ InvokeBuiltin(Builtins::TO_NUMBER, JUMP_FUNCTION);
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in esi.
  Counters* counters = masm->isolate()->counters();

  Label gc;
  __ Allocate(JSFunction::kSize, eax, ebx, ecx, &gc, TAG_OBJECT);

  __ IncrementCounter(counters->fast_new_closure_total(), 1);

  // Get the function info from the stack.
  __ mov(edx, Operand(esp, 1 * kPointerSize));

  int map_index = Context::FunctionMapIndex(language_mode_, is_generator_);

  // Compute the function map in the current native context and set that
  // as the map of the allocated object.
  __ mov(ecx, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(ecx, FieldOperand(ecx, GlobalObject::kNativeContextOffset));
  __ mov(ebx, Operand(ecx, Context::SlotOffset(map_index)));
  __ mov(FieldOperand(eax, JSObject::kMapOffset), ebx);

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  Factory* factory = masm->isolate()->factory();
  __ mov(ebx, Immediate(factory->empty_fixed_array()));
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), ebx);
  __ mov(FieldOperand(eax, JSFunction::kPrototypeOrInitialMapOffset),
         Immediate(factory->the_hole_value()));
  __ mov(FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset), edx);
  __ mov(FieldOperand(eax, JSFunction::kContextOffset), esi);
  __ mov(FieldOperand(eax, JSFunction::kLiteralsOffset), ebx);

  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  // But first check if there is an optimized version for our context.
  Label check_optimized;
  Label install_unoptimized;
  if (FLAG_cache_optimized_code) {
    __ mov(ebx, FieldOperand(edx, SharedFunctionInfo::kOptimizedCodeMapOffset));
    __ test(ebx, ebx);
    __ j(not_zero, &check_optimized, Label::kNear);
  }
  __ bind(&install_unoptimized);
  __ mov(FieldOperand(eax, JSFunction::kNextFunctionLinkOffset),
         Immediate(factory->undefined_value()));
  __ mov(edx, FieldOperand(edx, SharedFunctionInfo::kCodeOffset));
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ mov(FieldOperand(eax, JSFunction::kCodeEntryOffset), edx);

  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  __ bind(&check_optimized);

  __ IncrementCounter(counters->fast_new_closure_try_optimized(), 1);

  // ecx holds native context, ebx points to fixed array of 3-element entries
  // (native context, optimized code, literals).
  // Map must never be empty, so check the first elements.
  Label install_optimized;
  // Speculatively move code object into edx.
  __ mov(edx, FieldOperand(ebx, FixedArray::kHeaderSize + kPointerSize));
  __ cmp(ecx, FieldOperand(ebx, FixedArray::kHeaderSize));
  __ j(equal, &install_optimized);

  // Iterate through the rest of map backwards.  edx holds an index as a Smi.
  Label loop;
  Label restore;
  __ mov(edx, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ bind(&loop);
  // Do not double check first entry.
  __ cmp(edx, Immediate(Smi::FromInt(SharedFunctionInfo::kEntryLength)));
  __ j(equal, &restore);
  __ sub(edx, Immediate(Smi::FromInt(
      SharedFunctionInfo::kEntryLength)));  // Skip an entry.
  __ cmp(ecx, CodeGenerator::FixedArrayElementOperand(ebx, edx, 0));
  __ j(not_equal, &loop, Label::kNear);
  // Hit: fetch the optimized code.
  __ mov(edx, CodeGenerator::FixedArrayElementOperand(ebx, edx, 1));

  __ bind(&install_optimized);
  __ IncrementCounter(counters->fast_new_closure_install_optimized(), 1);

  // TODO(fschneider): Idea: store proper code pointers in the optimized code
  // map and either unmangle them on marking or do nothing as the whole map is
  // discarded on major GC anyway.
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ mov(FieldOperand(eax, JSFunction::kCodeEntryOffset), edx);

  // Now link a function into a list of optimized functions.
  __ mov(edx, ContextOperand(ecx, Context::OPTIMIZED_FUNCTIONS_LIST));

  __ mov(FieldOperand(eax, JSFunction::kNextFunctionLinkOffset), edx);
  // No need for write barrier as JSFunction (eax) is in the new space.

  __ mov(ContextOperand(ecx, Context::OPTIMIZED_FUNCTIONS_LIST), eax);
  // Store JSFunction (eax) into edx before issuing write barrier as
  // it clobbers all the registers passed.
  __ mov(edx, eax);
  __ RecordWriteContextSlot(
      ecx,
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST),
      edx,
      ebx,
      kDontSaveFPRegs);

  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  __ bind(&restore);
  // Restore SharedFunctionInfo into edx.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ jmp(&install_unoptimized);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(ecx);  // Temporarily remove return address.
  __ pop(edx);
  __ push(esi);
  __ push(edx);
  __ push(Immediate(factory->false_value()));
  __ push(ecx);  // Restore return address.
  __ TailCallRuntime(Runtime::kNewClosure, 3, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ Allocate((length * kPointerSize) + FixedArray::kHeaderSize,
              eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));

  // Set up the object header.
  Factory* factory = masm->isolate()->factory();
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         factory->function_context_map());
  __ mov(FieldOperand(eax, Context::kLengthOffset),
         Immediate(Smi::FromInt(length)));

  // Set up the fixed slots.
  __ Set(ebx, Immediate(0));  // Set to NULL.
  __ mov(Operand(eax, Context::SlotOffset(Context::CLOSURE_INDEX)), ecx);
  __ mov(Operand(eax, Context::SlotOffset(Context::PREVIOUS_INDEX)), esi);
  __ mov(Operand(eax, Context::SlotOffset(Context::EXTENSION_INDEX)), ebx);

  // Copy the global object from the previous context.
  __ mov(ebx, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(Operand(eax, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)), ebx);

  // Initialize the rest of the slots to undefined.
  __ mov(ebx, factory->undefined_value());
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ mov(Operand(eax, Context::SlotOffset(i)), ebx);
  }

  // Return and remove the on-stack parameter.
  __ mov(esi, eax);
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewFunctionContext, 1, 1);
}


void FastNewBlockContextStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [esp + (1 * kPointerSize)]: function
  // [esp + (2 * kPointerSize)]: serialized scope info

  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ Allocate(FixedArray::SizeFor(length), eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the function or sentinel from the stack.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));

  // Get the serialized scope info from the stack.
  __ mov(ebx, Operand(esp, 2 * kPointerSize));

  // Set up the object header.
  Factory* factory = masm->isolate()->factory();
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         factory->block_context_map());
  __ mov(FieldOperand(eax, Context::kLengthOffset),
         Immediate(Smi::FromInt(length)));

  // If this block context is nested in the native context we get a smi
  // sentinel instead of a function. The block context should get the
  // canonical empty function of the native context as its closure which
  // we still have to look up.
  Label after_sentinel;
  __ JumpIfNotSmi(ecx, &after_sentinel, Label::kNear);
  if (FLAG_debug_code) {
    const char* message = "Expected 0 as a Smi sentinel";
    __ cmp(ecx, 0);
    __ Assert(equal, message);
  }
  __ mov(ecx, GlobalObjectOperand());
  __ mov(ecx, FieldOperand(ecx, GlobalObject::kNativeContextOffset));
  __ mov(ecx, ContextOperand(ecx, Context::CLOSURE_INDEX));
  __ bind(&after_sentinel);

  // Set up the fixed slots.
  __ mov(ContextOperand(eax, Context::CLOSURE_INDEX), ecx);
  __ mov(ContextOperand(eax, Context::PREVIOUS_INDEX), esi);
  __ mov(ContextOperand(eax, Context::EXTENSION_INDEX), ebx);

  // Copy the global object from the previous context.
  __ mov(ebx, ContextOperand(esi, Context::GLOBAL_OBJECT_INDEX));
  __ mov(ContextOperand(eax, Context::GLOBAL_OBJECT_INDEX), ebx);

  // Initialize the rest of the slots to the hole value.
  if (slots_ == 1) {
    __ mov(ContextOperand(eax, Context::MIN_CONTEXT_SLOTS),
           factory->the_hole_value());
  } else {
    __ mov(ebx, factory->the_hole_value());
    for (int i = 0; i < slots_; i++) {
      __ mov(ContextOperand(eax, i + Context::MIN_CONTEXT_SLOTS), ebx);
    }
  }

  // Return and remove the on-stack parameters.
  __ mov(esi, eax);
  __ ret(2 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kPushBlockContext, 2, 1);
}


// The stub expects its argument on the stack and returns its result in tos_:
// zero for false, and a non-zero value for true.
void ToBooleanStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  Label patch;
  Factory* factory = masm->isolate()->factory();
  const Register argument = eax;
  const Register map = edx;

  if (!types_.IsEmpty()) {
    __ mov(argument, Operand(esp, 1 * kPointerSize));
  }

  // undefined -> false
  CheckOddball(masm, UNDEFINED, Heap::kUndefinedValueRootIndex, false);

  // Boolean -> its value
  CheckOddball(masm, BOOLEAN, Heap::kFalseValueRootIndex, false);
  CheckOddball(masm, BOOLEAN, Heap::kTrueValueRootIndex, true);

  // 'null' -> false.
  CheckOddball(masm, NULL_TYPE, Heap::kNullValueRootIndex, false);

  if (types_.Contains(SMI)) {
    // Smis: 0 -> false, all other -> true
    Label not_smi;
    __ JumpIfNotSmi(argument, &not_smi, Label::kNear);
    // argument contains the correct return value already.
    if (!tos_.is(argument)) {
      __ mov(tos_, argument);
    }
    __ ret(1 * kPointerSize);
    __ bind(&not_smi);
  } else if (types_.NeedsMap()) {
    // If we need a map later and have a Smi -> patch.
    __ JumpIfSmi(argument, &patch, Label::kNear);
  }

  if (types_.NeedsMap()) {
    __ mov(map, FieldOperand(argument, HeapObject::kMapOffset));

    if (types_.CanBeUndetectable()) {
      __ test_b(FieldOperand(map, Map::kBitFieldOffset),
                1 << Map::kIsUndetectable);
      // Undetectable -> false.
      Label not_undetectable;
      __ j(zero, &not_undetectable, Label::kNear);
      __ Set(tos_, Immediate(0));
      __ ret(1 * kPointerSize);
      __ bind(&not_undetectable);
    }
  }

  if (types_.Contains(SPEC_OBJECT)) {
    // spec object -> true.
    Label not_js_object;
    __ CmpInstanceType(map, FIRST_SPEC_OBJECT_TYPE);
    __ j(below, &not_js_object, Label::kNear);
    // argument contains the correct return value already.
    if (!tos_.is(argument)) {
      __ Set(tos_, Immediate(1));
    }
    __ ret(1 * kPointerSize);
    __ bind(&not_js_object);
  }

  if (types_.Contains(STRING)) {
    // String value -> false iff empty.
    Label not_string;
    __ CmpInstanceType(map, FIRST_NONSTRING_TYPE);
    __ j(above_equal, &not_string, Label::kNear);
    __ mov(tos_, FieldOperand(argument, String::kLengthOffset));
    __ ret(1 * kPointerSize);  // the string length is OK as the return value
    __ bind(&not_string);
  }

  if (types_.Contains(SYMBOL)) {
    // Symbol value -> true.
    Label not_symbol;
    __ CmpInstanceType(map, SYMBOL_TYPE);
    __ j(not_equal, &not_symbol, Label::kNear);
    __ bind(&not_symbol);
  }

  if (types_.Contains(HEAP_NUMBER)) {
    // heap number -> false iff +0, -0, or NaN.
    Label not_heap_number, false_result;
    __ cmp(map, factory->heap_number_map());
    __ j(not_equal, &not_heap_number, Label::kNear);
    __ fldz();
    __ fld_d(FieldOperand(argument, HeapNumber::kValueOffset));
    __ FCmp();
    __ j(zero, &false_result, Label::kNear);
    // argument contains the correct return value already.
    if (!tos_.is(argument)) {
      __ Set(tos_, Immediate(1));
    }
    __ ret(1 * kPointerSize);
    __ bind(&false_result);
    __ Set(tos_, Immediate(0));
    __ ret(1 * kPointerSize);
    __ bind(&not_heap_number);
  }

  __ bind(&patch);
  GenerateTypeTransition(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ pushad();
  if (save_doubles_ == kSaveFPRegs) {
    CpuFeatureScope scope(masm, SSE2);
    __ sub(esp, Immediate(kDoubleSize * XMMRegister::kNumRegisters));
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      __ movdbl(Operand(esp, i * kDoubleSize), reg);
    }
  }
  const int argument_count = 1;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, ecx);
  __ mov(Operand(esp, 0 * kPointerSize),
         Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(masm->isolate()),
      argument_count);
  if (save_doubles_ == kSaveFPRegs) {
    CpuFeatureScope scope(masm, SSE2);
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      __ movdbl(reg, Operand(esp, i * kDoubleSize));
    }
    __ add(esp, Immediate(kDoubleSize * XMMRegister::kNumRegisters));
  }
  __ popad();
  __ ret(0);
}


void ToBooleanStub::CheckOddball(MacroAssembler* masm,
                                 Type type,
                                 Heap::RootListIndex value,
                                 bool result) {
  const Register argument = eax;
  if (types_.Contains(type)) {
    // If we see an expected oddball, return its ToBoolean value tos_.
    Label different_value;
    __ CompareRoot(argument, value);
    __ j(not_equal, &different_value, Label::kNear);
    if (!result) {
      // If we have to return zero, there is no way around clearing tos_.
      __ Set(tos_, Immediate(0));
    } else if (!tos_.is(argument)) {
      // If we have to return non-zero, we can re-use the argument if it is the
      // same register as the result, because we never see Smi-zero here.
      __ Set(tos_, Immediate(1));
    }
    __ ret(1 * kPointerSize);
    __ bind(&different_value);
  }
}


void ToBooleanStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(ecx);  // Get return address, operand is now on top of stack.
  __ push(Immediate(Smi::FromInt(tos_.code())));
  __ push(Immediate(Smi::FromInt(types_.ToByte())));
  __ push(ecx);  // Push return address.
  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kToBoolean_Patch), masm->isolate()),
      3,
      1);
}


class FloatingPointHelper : public AllStatic {
 public:
  enum ArgLocation {
    ARGS_ON_STACK,
    ARGS_IN_REGISTERS
  };

  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in register number. Returns operand as floating point number
  // on FPU stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register number);

  // Code pattern for loading floating point values. Input values must
  // be either smi or heap number objects (fp values). Requirements:
  // operand_1 on TOS+1 or in edx, operand_2 on TOS+2 or in eax.
  // Returns operands as floating point numbers on FPU stack.
  static void LoadFloatOperands(MacroAssembler* masm,
                                Register scratch,
                                ArgLocation arg_location = ARGS_ON_STACK);

  // Similar to LoadFloatOperand but assumes that both operands are smis.
  // Expects operands in edx, eax.
  static void LoadFloatSmis(MacroAssembler* masm, Register scratch);

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);

  // Takes the operands in edx and eax and loads them as integers in eax
  // and ecx.
  static void LoadUnknownsAsIntegers(MacroAssembler* masm,
                                     bool use_sse3,
                                     BinaryOpIC::TypeInfo left_type,
                                     BinaryOpIC::TypeInfo right_type,
                                     Label* operand_conversion_failure);

  // Assumes that operands are smis or heap numbers and loads them
  // into xmm0 and xmm1. Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm);

  // Test if operands are numbers (smi or HeapNumber objects), and load
  // them into xmm0 and xmm1 if they are.  Jump to label not_numbers if
  // either operand is not a number.  Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm, Label* not_numbers);

  // Similar to LoadSSE2Operands but assumes that both operands are smis.
  // Expects operands in edx, eax.
  static void LoadSSE2Smis(MacroAssembler* masm, Register scratch);

  // Checks that the two floating point numbers loaded into xmm0 and xmm1
  // have int32 values.
  static void CheckSSE2OperandsAreInt32(MacroAssembler* masm,
                                        Label* non_int32,
                                        Register scratch);

  // Checks that |operand| has an int32 value. If |int32_result| is different
  // from |scratch|, it will contain that int32 value.
  static void CheckSSE2OperandIsInt32(MacroAssembler* masm,
                                      Label* non_int32,
                                      XMMRegister operand,
                                      Register int32_result,
                                      Register scratch,
                                      XMMRegister xmm_scratch);
};


// Get the integer part of a heap number.  Surprisingly, all this bit twiddling
// is faster than using the built-in instructions on floating point registers.
// Trashes edi and ebx.  Dest is ecx.  Source cannot be ecx or one of the
// trashed registers.
static void IntegerConvert(MacroAssembler* masm,
                           Register source,
                           bool use_sse3,
                           Label* conversion_failure) {
  ASSERT(!source.is(ecx) && !source.is(edi) && !source.is(ebx));
  Label done, right_exponent, normal_exponent;
  Register scratch = ebx;
  Register scratch2 = edi;
  // Get exponent word.
  __ mov(scratch, FieldOperand(source, HeapNumber::kExponentOffset));
  // Get exponent alone in scratch2.
  __ mov(scratch2, scratch);
  __ and_(scratch2, HeapNumber::kExponentMask);
  __ shr(scratch2, HeapNumber::kExponentShift);
  __ sub(scratch2, Immediate(HeapNumber::kExponentBias));
  // Load ecx with zero.  We use this either for the final shift or
  // for the answer.
  __ xor_(ecx, ecx);
  // If the exponent is above 83, the number contains no significant
  // bits in the range 0..2^31, so the result is zero.
  static const uint32_t kResultIsZeroExponent = 83;
  __ cmp(scratch2, Immediate(kResultIsZeroExponent));
  __ j(above, &done);
  if (use_sse3) {
    CpuFeatureScope scope(masm, SSE3);
    // Check whether the exponent is too big for a 64 bit signed integer.
    static const uint32_t kTooBigExponent = 63;
    __ cmp(scratch2, Immediate(kTooBigExponent));
    __ j(greater_equal, conversion_failure);
    // Load x87 register with heap number.
    __ fld_d(FieldOperand(source, HeapNumber::kValueOffset));
    // Reserve space for 64 bit answer.
    __ sub(esp, Immediate(sizeof(uint64_t)));  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(ecx, Operand(esp, 0));  // Load low word of answer into ecx.
    __ add(esp, Immediate(sizeof(uint64_t)));  // Nolint.
  } else {
    // Check whether the exponent matches a 32 bit signed int that cannot be
    // represented by a Smi.  A non-smi 32 bit integer is 1.xxx * 2^30 so the
    // exponent is 30 (biased).  This is the exponent that we are fastest at and
    // also the highest exponent we can handle here.
    const uint32_t non_smi_exponent = 30;
    __ cmp(scratch2, Immediate(non_smi_exponent));
    // If we have a match of the int32-but-not-Smi exponent then skip some
    // logic.
    __ j(equal, &right_exponent, Label::kNear);
    // If the exponent is higher than that then go to slow case.  This catches
    // numbers that don't fit in a signed int32, infinities and NaNs.
    __ j(less, &normal_exponent, Label::kNear);

    {
      // Handle a big exponent.  The only reason we have this code is that the
      // >>> operator has a tendency to generate numbers with an exponent of 31.
      const uint32_t big_non_smi_exponent = 31;
      __ cmp(scratch2, Immediate(big_non_smi_exponent));
      __ j(not_equal, conversion_failure);
      // We have the big exponent, typically from >>>.  This means the number is
      // in the range 2^31 to 2^32 - 1.  Get the top bits of the mantissa.
      __ mov(scratch2, scratch);
      __ and_(scratch2, HeapNumber::kMantissaMask);
      // Put back the implicit 1.
      __ or_(scratch2, 1 << HeapNumber::kExponentShift);
      // Shift up the mantissa bits to take up the space the exponent used to
      // take. We just orred in the implicit bit so that took care of one and
      // we want to use the full unsigned range so we subtract 1 bit from the
      // shift distance.
      const int big_shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 1;
      __ shl(scratch2, big_shift_distance);
      // Get the second half of the double.
      __ mov(ecx, FieldOperand(source, HeapNumber::kMantissaOffset));
      // Shift down 21 bits to get the most significant 11 bits or the low
      // mantissa word.
      __ shr(ecx, 32 - big_shift_distance);
      __ or_(ecx, scratch2);
      // We have the answer in ecx, but we may need to negate it.
      __ test(scratch, scratch);
      __ j(positive, &done, Label::kNear);
      __ neg(ecx);
      __ jmp(&done, Label::kNear);
    }

    __ bind(&normal_exponent);
    // Exponent word in scratch, exponent in scratch2. Zero in ecx.
    // We know that 0 <= exponent < 30.
    __ mov(ecx, Immediate(30));
    __ sub(ecx, scratch2);

    __ bind(&right_exponent);
    // Here ecx is the shift, scratch is the exponent word.
    // Get the top bits of the mantissa.
    __ and_(scratch, HeapNumber::kMantissaMask);
    // Put back the implicit 1.
    __ or_(scratch, 1 << HeapNumber::kExponentShift);
    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We have kExponentShift + 1 significant bits int he low end of the
    // word.  Shift them to the top bits.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    __ shl(scratch, shift_distance);
    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    __ mov(scratch2, FieldOperand(source, HeapNumber::kMantissaOffset));
    // Shift down 22 bits to get the most significant 10 bits or the low
    // mantissa word.
    __ shr(scratch2, 32 - shift_distance);
    __ or_(scratch2, scratch);
    // Move down according to the exponent.
    __ shr_cl(scratch2);
    // Now the unsigned answer is in scratch2.  We need to move it to ecx and
    // we may need to fix the sign.
    Label negative;
    __ xor_(ecx, ecx);
    __ cmp(ecx, FieldOperand(source, HeapNumber::kExponentOffset));
    __ j(greater, &negative, Label::kNear);
    __ mov(ecx, scratch2);
    __ jmp(&done, Label::kNear);
    __ bind(&negative);
    __ sub(ecx, scratch2);
  }
  __ bind(&done);
}


// Uses SSE2 to convert the heap number in |source| to an integer. Jumps to
// |conversion_failure| if the heap number did not contain an int32 value.
// Result is in ecx. Trashes ebx, xmm0, and xmm1.
static void ConvertHeapNumberToInt32(MacroAssembler* masm,
                                     Register source,
                                     Label* conversion_failure) {
  __ movdbl(xmm0, FieldOperand(source, HeapNumber::kValueOffset));
  FloatingPointHelper::CheckSSE2OperandIsInt32(
      masm, conversion_failure, xmm0, ecx, ebx, xmm1);
}


void UnaryOpStub::PrintName(StringStream* stream) {
  const char* op_name = Token::Name(op_);
  const char* overwrite_name = NULL;  // Make g++ happy.
  switch (mode_) {
    case UNARY_NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case UNARY_OVERWRITE: overwrite_name = "Overwrite"; break;
  }
  stream->Add("UnaryOpStub_%s_%s_%s",
              op_name,
              overwrite_name,
              UnaryOpIC::GetName(operand_type_));
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::Generate(MacroAssembler* masm) {
  switch (operand_type_) {
    case UnaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case UnaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case UnaryOpIC::NUMBER:
      GenerateNumberStub(masm);
      break;
    case UnaryOpIC::GENERIC:
      GenerateGenericStub(masm);
      break;
  }
}


void UnaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(ecx);  // Save return address.

  __ push(eax);  // the operand
  __ push(Immediate(Smi::FromInt(op_)));
  __ push(Immediate(Smi::FromInt(mode_)));
  __ push(Immediate(Smi::FromInt(operand_type_)));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kUnaryOp_Patch), masm->isolate()), 4, 1);
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateSmiStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateSmiStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateSmiStubSub(MacroAssembler* masm) {
  Label non_smi, undo, slow;
  GenerateSmiCodeSub(masm, &non_smi, &undo, &slow,
                     Label::kNear, Label::kNear, Label::kNear);
  __ bind(&undo);
  GenerateSmiCodeUndo(masm);
  __ bind(&non_smi);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiStubBitNot(MacroAssembler* masm) {
  Label non_smi;
  GenerateSmiCodeBitNot(masm, &non_smi);
  __ bind(&non_smi);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiCodeSub(MacroAssembler* masm,
                                     Label* non_smi,
                                     Label* undo,
                                     Label* slow,
                                     Label::Distance non_smi_near,
                                     Label::Distance undo_near,
                                     Label::Distance slow_near) {
  // Check whether the value is a smi.
  __ JumpIfNotSmi(eax, non_smi, non_smi_near);

  // We can't handle -0 with smis, so use a type transition for that case.
  __ test(eax, eax);
  __ j(zero, slow, slow_near);

  // Try optimistic subtraction '0 - value', saving operand in eax for undo.
  __ mov(edx, eax);
  __ Set(eax, Immediate(0));
  __ sub(eax, edx);
  __ j(overflow, undo, undo_near);
  __ ret(0);
}


void UnaryOpStub::GenerateSmiCodeBitNot(
    MacroAssembler* masm,
    Label* non_smi,
    Label::Distance non_smi_near) {
  // Check whether the value is a smi.
  __ JumpIfNotSmi(eax, non_smi, non_smi_near);

  // Flip bits and revert inverted smi-tag.
  __ not_(eax);
  __ and_(eax, ~kSmiTagMask);
  __ ret(0);
}


void UnaryOpStub::GenerateSmiCodeUndo(MacroAssembler* masm) {
  __ mov(eax, edx);
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateNumberStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateNumberStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateNumberStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateNumberStubSub(MacroAssembler* masm) {
  Label non_smi, undo, slow, call_builtin;
  GenerateSmiCodeSub(masm, &non_smi, &undo, &call_builtin, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&undo);
  GenerateSmiCodeUndo(masm);
  __ bind(&slow);
  GenerateTypeTransition(masm);
  __ bind(&call_builtin);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateNumberStubBitNot(
    MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateHeapNumberCodeSub(MacroAssembler* masm,
                                            Label* slow) {
  __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(edx, masm->isolate()->factory()->heap_number_map());
  __ j(not_equal, slow);

  if (mode_ == UNARY_OVERWRITE) {
    __ xor_(FieldOperand(eax, HeapNumber::kExponentOffset),
            Immediate(HeapNumber::kSignMask));  // Flip sign.
  } else {
    __ mov(edx, eax);
    // edx: operand

    Label slow_allocate_heapnumber, heapnumber_allocated;
    __ AllocateHeapNumber(eax, ebx, ecx, &slow_allocate_heapnumber);
    __ jmp(&heapnumber_allocated, Label::kNear);

    __ bind(&slow_allocate_heapnumber);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(edx);
      __ CallRuntime(Runtime::kNumberAlloc, 0);
      __ pop(edx);
    }

    __ bind(&heapnumber_allocated);
    // eax: allocated 'empty' number
    __ mov(ecx, FieldOperand(edx, HeapNumber::kExponentOffset));
    __ xor_(ecx, HeapNumber::kSignMask);  // Flip sign.
    __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), ecx);
    __ mov(ecx, FieldOperand(edx, HeapNumber::kMantissaOffset));
    __ mov(FieldOperand(eax, HeapNumber::kMantissaOffset), ecx);
  }
  __ ret(0);
}


void UnaryOpStub::GenerateHeapNumberCodeBitNot(MacroAssembler* masm,
                                               Label* slow) {
  __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(edx, masm->isolate()->factory()->heap_number_map());
  __ j(not_equal, slow);

  // Convert the heap number in eax to an untagged integer in ecx.
  IntegerConvert(masm, eax, CpuFeatures::IsSupported(SSE3), slow);

  // Do the bitwise operation and check if the result fits in a smi.
  Label try_float;
  __ not_(ecx);
  __ cmp(ecx, 0xc0000000);
  __ j(sign, &try_float, Label::kNear);

  // Tag the result as a smi and we're done.
  STATIC_ASSERT(kSmiTagSize == 1);
  __ lea(eax, Operand(ecx, times_2, kSmiTag));
  __ ret(0);

  // Try to store the result in a heap number.
  __ bind(&try_float);
  if (mode_ == UNARY_NO_OVERWRITE) {
    Label slow_allocate_heapnumber, heapnumber_allocated;
    __ mov(ebx, eax);
    __ AllocateHeapNumber(eax, edx, edi, &slow_allocate_heapnumber);
    __ jmp(&heapnumber_allocated);

    __ bind(&slow_allocate_heapnumber);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      // Push the original HeapNumber on the stack. The integer value can't
      // be stored since it's untagged and not in the smi range (so we can't
      // smi-tag it). We'll recalculate the value after the GC instead.
      __ push(ebx);
      __ CallRuntime(Runtime::kNumberAlloc, 0);
      // New HeapNumber is in eax.
      __ pop(edx);
    }
    // IntegerConvert uses ebx and edi as scratch registers.
    // This conversion won't go slow-case.
    IntegerConvert(masm, edx, CpuFeatures::IsSupported(SSE3), slow);
    __ not_(ecx);

    __ bind(&heapnumber_allocated);
  }
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatureScope use_sse2(masm, SSE2);
    __ cvtsi2sd(xmm0, ecx);
    __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
  } else {
    __ push(ecx);
    __ fild_s(Operand(esp, 0));
    __ pop(ecx);
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
  }
  __ ret(0);
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateGenericStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateGenericStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateGenericStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateGenericStubSub(MacroAssembler* masm)  {
  Label non_smi, undo, slow;
  GenerateSmiCodeSub(masm, &non_smi, &undo, &slow, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&undo);
  GenerateSmiCodeUndo(masm);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericStubBitNot(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericCodeFallback(MacroAssembler* masm) {
  // Handle the slow case by jumping to the corresponding JavaScript builtin.
  __ pop(ecx);  // pop return address.
  __ push(eax);
  __ push(ecx);  // push return address
  switch (op_) {
    case Token::SUB:
      __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_FUNCTION);
      break;
    case Token::BIT_NOT:
      __ InvokeBuiltin(Builtins::BIT_NOT, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::Initialize() {
  platform_specific_bit_ = CpuFeatures::IsSupported(SSE3);
}


void BinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(ecx);  // Save return address.
  __ push(edx);
  __ push(eax);
  // Left and right arguments are now on top.
  __ push(Immediate(Smi::FromInt(MinorKey())));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch),
                        masm->isolate()),
      3,
      1);
}


// Prepare for a type transition runtime call when the args are already on
// the stack, under the return address.
void BinaryOpStub::GenerateTypeTransitionWithSavedArgs(MacroAssembler* masm) {
  __ pop(ecx);  // Save return address.
  // Left and right arguments are already on top of the stack.
  __ push(Immediate(Smi::FromInt(MinorKey())));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch),
                        masm->isolate()),
      3,
      1);
}


static void BinaryOpStub_GenerateSmiCode(
    MacroAssembler* masm,
    Label* slow,
    BinaryOpStub::SmiCodeGenerateHeapNumberResults allow_heapnumber_results,
    Token::Value op) {
  // 1. Move arguments into edx, eax except for DIV and MOD, which need the
  // dividend in eax and edx free for the division.  Use eax, ebx for those.
  Comment load_comment(masm, "-- Load arguments");
  Register left = edx;
  Register right = eax;
  if (op == Token::DIV || op == Token::MOD) {
    left = eax;
    right = ebx;
    __ mov(ebx, eax);
    __ mov(eax, edx);
  }


  // 2. Prepare the smi check of both operands by oring them together.
  Comment smi_check_comment(masm, "-- Smi check arguments");
  Label not_smis;
  Register combined = ecx;
  ASSERT(!left.is(combined) && !right.is(combined));
  switch (op) {
    case Token::BIT_OR:
      // Perform the operation into eax and smi check the result.  Preserve
      // eax in case the result is not a smi.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, left);  // Bitwise or is commutative.
      combined = right;
      break;

    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      __ mov(combined, right);
      __ or_(combined, left);
      break;

    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Move the right operand into ecx for the shift operation, use eax
      // for the smi check register.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, left);
      combined = right;
      break;

    default:
      break;
  }

  // 3. Perform the smi check of the operands.
  STATIC_ASSERT(kSmiTag == 0);  // Adjust zero check if not the case.
  __ JumpIfNotSmi(combined, &not_smis);

  // 4. Operands are both smis, perform the operation leaving the result in
  // eax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op) {
    case Token::BIT_OR:
      // Nothing to do.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(eax));
      __ xor_(right, left);  // Bitwise xor is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(eax));
      __ and_(right, left);  // Bitwise and is commutative.
      break;

    case Token::SHL:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shl_cl(left);
      // Check that the *signed* result fits in a smi.
      __ cmp(left, 0xc0000000);
      __ j(sign, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::SAR:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ sar_cl(left);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::SHR:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shr_cl(left);
      // Check that the *unsigned* result fits in a smi.
      // Neither of the two high-order bits can be set:
      // - 0x80000000: high bit would be lost when smi tagging.
      // - 0x40000000: this number would convert to negative when
      // Smi tagging these two cases can only happen with shifts
      // by 0 or 1 when handed a valid smi.
      __ test(left, Immediate(0xc0000000));
      __ j(not_zero, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::ADD:
      ASSERT(right.is(eax));
      __ add(right, left);  // Addition is commutative.
      __ j(overflow, &use_fp_on_smis);
      break;

    case Token::SUB:
      __ sub(left, right);
      __ j(overflow, &use_fp_on_smis);
      __ mov(eax, left);
      break;

    case Token::MUL:
      // If the smi tag is 0 we can just leave the tag on one operand.
      STATIC_ASSERT(kSmiTag == 0);  // Adjust code below if not the case.
      // We can't revert the multiplication if the result is not a smi
      // so save the right operand.
      __ mov(ebx, right);
      // Remove tag from one of the operands (but keep sign).
      __ SmiUntag(right);
      // Do multiplication.
      __ imul(right, left);  // Multiplication is commutative.
      __ j(overflow, &use_fp_on_smis);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(right, combined, &use_fp_on_smis);
      break;

    case Token::DIV:
      // We can't revert the division if the result is not a smi so
      // save the left operand.
      __ mov(edi, left);
      // Check for 0 divisor.
      __ test(right, right);
      __ j(zero, &use_fp_on_smis);
      // Sign extend left into edx:eax.
      ASSERT(left.is(eax));
      __ cdq();
      // Divide edx:eax by right.
      __ idiv(right);
      // Check for the corner case of dividing the most negative smi by
      // -1. We cannot use the overflow flag, since it is not set by idiv
      // instruction.
      STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
      __ cmp(eax, 0x40000000);
      __ j(equal, &use_fp_on_smis);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(eax, combined, &use_fp_on_smis);
      // Check that the remainder is zero.
      __ test(edx, edx);
      __ j(not_zero, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(eax);
      break;

    case Token::MOD:
      // Check for 0 divisor.
      __ test(right, right);
      __ j(zero, &not_smis);

      // Sign extend left into edx:eax.
      ASSERT(left.is(eax));
      __ cdq();
      // Divide edx:eax by right.
      __ idiv(right);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(edx, combined, slow);
      // Move remainder to register eax.
      __ mov(eax, edx);
      break;

    default:
      UNREACHABLE();
  }

  // 5. Emit return of result in eax.  Some operations have registers pushed.
  switch (op) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      __ ret(0);
      break;
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      __ ret(2 * kPointerSize);
      break;
    default:
      UNREACHABLE();
  }

  // 6. For some operations emit inline code to perform floating point
  // operations on known smis (e.g., if the result of the operation
  // overflowed the smi range).
  if (allow_heapnumber_results == BinaryOpStub::NO_HEAPNUMBER_RESULTS) {
    __ bind(&use_fp_on_smis);
    switch (op) {
      // Undo the effects of some operations, and some register moves.
      case Token::SHL:
        // The arguments are saved on the stack, and only used from there.
        break;
      case Token::ADD:
        // Revert right = right + left.
        __ sub(right, left);
        break;
      case Token::SUB:
        // Revert left = left - right.
        __ add(left, right);
        break;
      case Token::MUL:
        // Right was clobbered but a copy is in ebx.
        __ mov(right, ebx);
        break;
      case Token::DIV:
        // Left was clobbered but a copy is in edi.  Right is in ebx for
        // division.  They should be in eax, ebx for jump to not_smi.
        __ mov(eax, edi);
        break;
      default:
        // No other operators jump to use_fp_on_smis.
        break;
    }
    __ jmp(&not_smis);
  } else {
    ASSERT(allow_heapnumber_results == BinaryOpStub::ALLOW_HEAPNUMBER_RESULTS);
    switch (op) {
      case Token::SHL:
      case Token::SHR: {
        Comment perform_float(masm, "-- Perform float operation on smis");
        __ bind(&use_fp_on_smis);
        // Result we want is in left == edx, so we can put the allocated heap
        // number in eax.
        __ AllocateHeapNumber(eax, ecx, ebx, slow);
        // Store the result in the HeapNumber and return.
        // It's OK to overwrite the arguments on the stack because we
        // are about to return.
        if (op == Token::SHR) {
          __ mov(Operand(esp, 1 * kPointerSize), left);
          __ mov(Operand(esp, 2 * kPointerSize), Immediate(0));
          __ fild_d(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        } else {
          ASSERT_EQ(Token::SHL, op);
          if (CpuFeatures::IsSupported(SSE2)) {
            CpuFeatureScope use_sse2(masm, SSE2);
            __ cvtsi2sd(xmm0, left);
            __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
          } else {
            __ mov(Operand(esp, 1 * kPointerSize), left);
            __ fild_s(Operand(esp, 1 * kPointerSize));
            __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
          }
        }
        __ ret(2 * kPointerSize);
        break;
      }

      case Token::ADD:
      case Token::SUB:
      case Token::MUL:
      case Token::DIV: {
        Comment perform_float(masm, "-- Perform float operation on smis");
        __ bind(&use_fp_on_smis);
        // Restore arguments to edx, eax.
        switch (op) {
          case Token::ADD:
            // Revert right = right + left.
            __ sub(right, left);
            break;
          case Token::SUB:
            // Revert left = left - right.
            __ add(left, right);
            break;
          case Token::MUL:
            // Right was clobbered but a copy is in ebx.
            __ mov(right, ebx);
            break;
          case Token::DIV:
            // Left was clobbered but a copy is in edi.  Right is in ebx for
            // division.
            __ mov(edx, edi);
            __ mov(eax, right);
            break;
          default: UNREACHABLE();
            break;
        }
        __ AllocateHeapNumber(ecx, ebx, no_reg, slow);
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatureScope use_sse2(masm, SSE2);
          FloatingPointHelper::LoadSSE2Smis(masm, ebx);
          switch (op) {
            case Token::ADD: __ addsd(xmm0, xmm1); break;
            case Token::SUB: __ subsd(xmm0, xmm1); break;
            case Token::MUL: __ mulsd(xmm0, xmm1); break;
            case Token::DIV: __ divsd(xmm0, xmm1); break;
            default: UNREACHABLE();
          }
          __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm0);
        } else {  // SSE2 not available, use FPU.
          FloatingPointHelper::LoadFloatSmis(masm, ebx);
          switch (op) {
            case Token::ADD: __ faddp(1); break;
            case Token::SUB: __ fsubp(1); break;
            case Token::MUL: __ fmulp(1); break;
            case Token::DIV: __ fdivp(1); break;
            default: UNREACHABLE();
          }
          __ fstp_d(FieldOperand(ecx, HeapNumber::kValueOffset));
        }
        __ mov(eax, ecx);
        __ ret(0);
        break;
      }

      default:
        break;
    }
  }

  // 7. Non-smi operands, fall out to the non-smi code with the operands in
  // edx and eax.
  Comment done_comment(masm, "-- Enter non-smi code");
  __ bind(&not_smis);
  switch (op) {
    case Token::BIT_OR:
    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Right operand is saved in ecx and eax was destroyed by the smi
      // check.
      __ mov(eax, ecx);
      break;

    case Token::DIV:
    case Token::MOD:
      // Operands are in eax, ebx at this point.
      __ mov(edx, eax);
      __ mov(eax, ebx);
      break;

    default:
      break;
  }
}


void BinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  Label call_runtime;

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      break;
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      GenerateRegisterArgsPush(masm);
      break;
    default:
      UNREACHABLE();
  }

  if (result_type_ == BinaryOpIC::UNINITIALIZED ||
      result_type_ == BinaryOpIC::SMI) {
    BinaryOpStub_GenerateSmiCode(
        masm, &call_runtime, NO_HEAPNUMBER_RESULTS, op_);
  } else {
    BinaryOpStub_GenerateSmiCode(
        masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS, op_);
  }
  __ bind(&call_runtime);
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      GenerateTypeTransition(masm);
      break;
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      GenerateTypeTransitionWithSavedArgs(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::GenerateBothStringStub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(left_type_ == BinaryOpIC::STRING && right_type_ == BinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  // If both arguments are strings, call the string add stub.
  // Otherwise, do a transition.

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &call_runtime, Label::kNear);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &call_runtime, Label::kNear);

  // Test if right operand is a string.
  __ JumpIfSmi(right, &call_runtime, Label::kNear);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &call_runtime, Label::kNear);

  StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_stub);

  __ bind(&call_runtime);
  GenerateTypeTransition(masm);
}


static void BinaryOpStub_GenerateHeapResultAllocation(MacroAssembler* masm,
                                                      Label* alloc_failure,
                                                      OverwriteMode mode);


// Input:
//    edx: left operand (tagged)
//    eax: right operand (tagged)
// Output:
//    eax: result (tagged)
void BinaryOpStub::GenerateInt32Stub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(Max(left_type_, right_type_) == BinaryOpIC::INT32);

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD: {
      Label not_floats;
      Label not_int32;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatureScope use_sse2(masm, SSE2);
        // It could be that only SMIs have been seen at either the left
        // or the right operand. For precise type feedback, patch the IC
        // again if this changes.
        // In theory, we would need the same check in the non-SSE2 case,
        // but since we don't support Crankshaft on such hardware we can
        // afford not to care about precise type feedback.
        if (left_type_ == BinaryOpIC::SMI) {
          __ JumpIfNotSmi(edx, &not_int32);
        }
        if (right_type_ == BinaryOpIC::SMI) {
          __ JumpIfNotSmi(eax, &not_int32);
        }
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);
        FloatingPointHelper::CheckSSE2OperandsAreInt32(masm, &not_int32, ecx);
        if (op_ == Token::MOD) {
          GenerateRegisterArgsPush(masm);
          __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
        } else {
          switch (op_) {
            case Token::ADD: __ addsd(xmm0, xmm1); break;
            case Token::SUB: __ subsd(xmm0, xmm1); break;
            case Token::MUL: __ mulsd(xmm0, xmm1); break;
            case Token::DIV: __ divsd(xmm0, xmm1); break;
            default: UNREACHABLE();
          }
          // Check result type if it is currently Int32.
          if (result_type_ <= BinaryOpIC::INT32) {
            FloatingPointHelper::CheckSSE2OperandIsInt32(
                masm, &not_int32, xmm0, ecx, ecx, xmm2);
          }
          BinaryOpStub_GenerateHeapResultAllocation(masm, &call_runtime, mode_);
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
          __ ret(0);
        }
      } else {  // SSE2 not available, use FPU.
        FloatingPointHelper::CheckFloatOperands(masm, &not_floats, ebx);
        FloatingPointHelper::LoadFloatOperands(
            masm,
            ecx,
            FloatingPointHelper::ARGS_IN_REGISTERS);
        if (op_ == Token::MOD) {
          // The operands are now on the FPU stack, but we don't need them.
          __ fstp(0);
          __ fstp(0);
          GenerateRegisterArgsPush(masm);
          __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
        } else {
          switch (op_) {
            case Token::ADD: __ faddp(1); break;
            case Token::SUB: __ fsubp(1); break;
            case Token::MUL: __ fmulp(1); break;
            case Token::DIV: __ fdivp(1); break;
            default: UNREACHABLE();
          }
          Label after_alloc_failure;
          BinaryOpStub_GenerateHeapResultAllocation(
              masm, &after_alloc_failure, mode_);
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
          __ ret(0);
          __ bind(&after_alloc_failure);
          __ fstp(0);  // Pop FPU stack before calling runtime.
          __ jmp(&call_runtime);
        }
      }

      __ bind(&not_floats);
      __ bind(&not_int32);
      GenerateTypeTransition(masm);
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      GenerateRegisterArgsPush(masm);
      Label not_floats;
      Label not_int32;
      Label non_smi_result;
      bool use_sse3 = platform_specific_bit_;
      FloatingPointHelper::LoadUnknownsAsIntegers(
          masm, use_sse3, left_type_, right_type_, &not_floats);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, ecx); break;
        case Token::BIT_AND: __ and_(eax, ecx); break;
        case Token::BIT_XOR: __ xor_(eax, ecx); break;
        case Token::SAR: __ sar_cl(eax); break;
        case Token::SHL: __ shl_cl(eax); break;
        case Token::SHR: __ shr_cl(eax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative and fits in a smi.
        __ test(eax, Immediate(0xc0000000));
        __ j(not_zero, &call_runtime);
      } else {
        // Check if result fits in a smi.
        __ cmp(eax, 0xc0000000);
        __ j(negative, &non_smi_result, Label::kNear);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, eax);  // ebx: result
        Label skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ JumpIfNotSmi(eax, &skip_allocation, Label::kNear);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatureScope use_sse2(masm, SSE2);
          __ cvtsi2sd(xmm0, ebx);
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          __ mov(Operand(esp, 1 * kPointerSize), ebx);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        }
        __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.
      }

      __ bind(&not_floats);
      __ bind(&not_int32);
      GenerateTypeTransitionWithSavedArgs(masm);
      break;
    }
    default: UNREACHABLE(); break;
  }

  // If an allocation fails, or SHR hits a hard case, use the runtime system to
  // get the correct result.
  __ bind(&call_runtime);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      GenerateRegisterArgsPush(masm);
      break;
    case Token::MOD:
      return;  // Handled above.
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      break;
    default:
      UNREACHABLE();
  }
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateAddStrings(masm);
  }

  Factory* factory = masm->isolate()->factory();

  // Convert odd ball arguments to numbers.
  Label check, done;
  __ cmp(edx, factory->undefined_value());
  __ j(not_equal, &check, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(edx, edx);
  } else {
    __ mov(edx, Immediate(factory->nan_value()));
  }
  __ jmp(&done, Label::kNear);
  __ bind(&check);
  __ cmp(eax, factory->undefined_value());
  __ j(not_equal, &done, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(eax, eax);
  } else {
    __ mov(eax, Immediate(factory->nan_value()));
  }
  __ bind(&done);

  GenerateNumberStub(masm);
}


void BinaryOpStub::GenerateNumberStub(MacroAssembler* masm) {
  Label call_runtime;

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Label not_floats;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatureScope use_sse2(masm, SSE2);

        // It could be that only SMIs have been seen at either the left
        // or the right operand. For precise type feedback, patch the IC
        // again if this changes.
        // In theory, we would need the same check in the non-SSE2 case,
        // but since we don't support Crankshaft on such hardware we can
        // afford not to care about precise type feedback.
        if (left_type_ == BinaryOpIC::SMI) {
          __ JumpIfNotSmi(edx, &not_floats);
        }
        if (right_type_ == BinaryOpIC::SMI) {
          __ JumpIfNotSmi(eax, &not_floats);
        }
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);
        if (left_type_ == BinaryOpIC::INT32) {
          FloatingPointHelper::CheckSSE2OperandIsInt32(
              masm, &not_floats, xmm0, ecx, ecx, xmm2);
        }
        if (right_type_ == BinaryOpIC::INT32) {
          FloatingPointHelper::CheckSSE2OperandIsInt32(
              masm, &not_floats, xmm1, ecx, ecx, xmm2);
        }

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        BinaryOpStub_GenerateHeapResultAllocation(masm, &call_runtime, mode_);
        __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        __ ret(0);
      } else {  // SSE2 not available, use FPU.
        FloatingPointHelper::CheckFloatOperands(masm, &not_floats, ebx);
        FloatingPointHelper::LoadFloatOperands(
            masm,
            ecx,
            FloatingPointHelper::ARGS_IN_REGISTERS);
        switch (op_) {
          case Token::ADD: __ faddp(1); break;
          case Token::SUB: __ fsubp(1); break;
          case Token::MUL: __ fmulp(1); break;
          case Token::DIV: __ fdivp(1); break;
          default: UNREACHABLE();
        }
        Label after_alloc_failure;
        BinaryOpStub_GenerateHeapResultAllocation(
            masm, &after_alloc_failure, mode_);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(0);
        __ bind(&after_alloc_failure);
        __ fstp(0);  // Pop FPU stack before calling runtime.
        __ jmp(&call_runtime);
      }

      __ bind(&not_floats);
      GenerateTypeTransition(masm);
      break;
    }

    case Token::MOD: {
      // For MOD we go directly to runtime in the non-smi case.
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      GenerateRegisterArgsPush(masm);
      Label not_floats;
      Label non_smi_result;
      // We do not check the input arguments here, as any value is
      // unconditionally truncated to an int32 anyway. To get the
      // right optimized code, int32 type feedback is just right.
      bool use_sse3 = platform_specific_bit_;
      FloatingPointHelper::LoadUnknownsAsIntegers(
                masm, use_sse3, left_type_, right_type_, &not_floats);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, ecx); break;
        case Token::BIT_AND: __ and_(eax, ecx); break;
        case Token::BIT_XOR: __ xor_(eax, ecx); break;
        case Token::SAR: __ sar_cl(eax); break;
        case Token::SHL: __ shl_cl(eax); break;
        case Token::SHR: __ shr_cl(eax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative and fits in a smi.
        __ test(eax, Immediate(0xc0000000));
        __ j(not_zero, &call_runtime);
      } else {
        // Check if result fits in a smi.
        __ cmp(eax, 0xc0000000);
        __ j(negative, &non_smi_result, Label::kNear);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, eax);  // ebx: result
        Label skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ JumpIfNotSmi(eax, &skip_allocation, Label::kNear);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatureScope use_sse2(masm, SSE2);
          __ cvtsi2sd(xmm0, ebx);
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          __ mov(Operand(esp, 1 * kPointerSize), ebx);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        }
        __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.
      }

      __ bind(&not_floats);
      GenerateTypeTransitionWithSavedArgs(masm);
      break;
    }
    default: UNREACHABLE(); break;
  }

  // If an allocation fails, or SHR or MOD hit a hard case,
  // use the runtime system to get the correct result.
  __ bind(&call_runtime);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      GenerateRegisterArgsPush(masm);
      break;
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      break;
    default:
      UNREACHABLE();
  }
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime;

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->generic_binary_stub_calls(), 1);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      break;
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      GenerateRegisterArgsPush(masm);
      break;
    default:
      UNREACHABLE();
  }

  BinaryOpStub_GenerateSmiCode(
      masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS, op_);

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Label not_floats;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatureScope use_sse2(masm, SSE2);
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        BinaryOpStub_GenerateHeapResultAllocation(masm, &call_runtime, mode_);
        __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        __ ret(0);
      } else {  // SSE2 not available, use FPU.
        FloatingPointHelper::CheckFloatOperands(masm, &not_floats, ebx);
        FloatingPointHelper::LoadFloatOperands(
            masm,
            ecx,
            FloatingPointHelper::ARGS_IN_REGISTERS);
        switch (op_) {
          case Token::ADD: __ faddp(1); break;
          case Token::SUB: __ fsubp(1); break;
          case Token::MUL: __ fmulp(1); break;
          case Token::DIV: __ fdivp(1); break;
          default: UNREACHABLE();
        }
        Label after_alloc_failure;
        BinaryOpStub_GenerateHeapResultAllocation(
            masm, &after_alloc_failure, mode_);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(0);
        __ bind(&after_alloc_failure);
        __ fstp(0);  // Pop FPU stack before calling runtime.
        __ jmp(&call_runtime);
      }
        __ bind(&not_floats);
        break;
      }
    case Token::MOD: {
      // For MOD we go directly to runtime in the non-smi case.
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND:
      case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      Label non_smi_result;
      bool use_sse3 = platform_specific_bit_;
      FloatingPointHelper::LoadUnknownsAsIntegers(masm,
                                                  use_sse3,
                                                  BinaryOpIC::GENERIC,
                                                  BinaryOpIC::GENERIC,
                                                  &call_runtime);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, ecx); break;
        case Token::BIT_AND: __ and_(eax, ecx); break;
        case Token::BIT_XOR: __ xor_(eax, ecx); break;
        case Token::SAR: __ sar_cl(eax); break;
        case Token::SHL: __ shl_cl(eax); break;
        case Token::SHR: __ shr_cl(eax); break;
        default: UNREACHABLE();
      }
      if (op_ == Token::SHR) {
        // Check if result is non-negative and fits in a smi.
        __ test(eax, Immediate(0xc0000000));
        __ j(not_zero, &call_runtime);
      } else {
        // Check if result fits in a smi.
        __ cmp(eax, 0xc0000000);
        __ j(negative, &non_smi_result, Label::kNear);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop the arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, eax);  // ebx: result
        Label skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
              // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ JumpIfNotSmi(eax, &skip_allocation, Label::kNear);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatureScope use_sse2(masm, SSE2);
          __ cvtsi2sd(xmm0, ebx);
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          __ mov(Operand(esp, 1 * kPointerSize), ebx);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        }
        __ ret(2 * kPointerSize);
      }
      break;
    }
    default: UNREACHABLE(); break;
  }

  // If all else fails, use the runtime system to get the correct
  // result.
  __ bind(&call_runtime);
  switch (op_) {
    case Token::ADD:
      GenerateAddStrings(masm);
      // Fall through.
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
      GenerateRegisterArgsPush(masm);
      break;
    case Token::MOD:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR:
      break;
    default:
      UNREACHABLE();
  }
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateAddStrings(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  Label left_not_string, call_runtime;

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &left_not_string, Label::kNear);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &left_not_string, Label::kNear);

  StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ JumpIfSmi(right, &call_runtime, Label::kNear);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &call_runtime, Label::kNear);

  StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_right_stub);

  // Neither argument is a string.
  __ bind(&call_runtime);
}


static void BinaryOpStub_GenerateHeapResultAllocation(MacroAssembler* masm,
                                                      Label* alloc_failure,
                                                      OverwriteMode mode) {
  Label skip_allocation;
  switch (mode) {
    case OVERWRITE_LEFT: {
      // If the argument in edx is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(edx, &skip_allocation, Label::kNear);
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now edx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(edx, ebx);
      __ bind(&skip_allocation);
      // Use object in edx as a result holder
      __ mov(eax, edx);
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in eax is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(eax, &skip_allocation, Label::kNear);
      // Fall through!
    case NO_OVERWRITE:
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now eax can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(eax, ebx);
      __ bind(&skip_allocation);
      break;
    default: UNREACHABLE();
  }
}


void BinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ pop(ecx);
  __ push(edx);
  __ push(eax);
  __ push(ecx);
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // TAGGED case:
  //   Input:
  //     esp[4]: tagged number input argument (should be number).
  //     esp[0]: return address.
  //   Output:
  //     eax: tagged double result.
  // UNTAGGED case:
  //   Input::
  //     esp[0]: return address.
  //     xmm1: untagged double input argument
  //   Output:
  //     xmm1: untagged double result.

  Label runtime_call;
  Label runtime_call_clear_stack;
  Label skip_cache;
  const bool tagged = (argument_type_ == TAGGED);
  if (tagged) {
    // Test that eax is a number.
    Label input_not_smi;
    Label loaded;
    __ mov(eax, Operand(esp, kPointerSize));
    __ JumpIfNotSmi(eax, &input_not_smi, Label::kNear);
    // Input is a smi. Untag and load it onto the FPU stack.
    // Then load the low and high words of the double into ebx, edx.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ sar(eax, 1);
    __ sub(esp, Immediate(2 * kPointerSize));
    __ mov(Operand(esp, 0), eax);
    __ fild_s(Operand(esp, 0));
    __ fst_d(Operand(esp, 0));
    __ pop(edx);
    __ pop(ebx);
    __ jmp(&loaded, Label::kNear);
    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
    Factory* factory = masm->isolate()->factory();
    __ cmp(ebx, Immediate(factory->heap_number_map()));
    __ j(not_equal, &runtime_call);
    // Input is a HeapNumber. Push it on the FPU stack and load its
    // low and high words into ebx, edx.
    __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ mov(edx, FieldOperand(eax, HeapNumber::kExponentOffset));
    __ mov(ebx, FieldOperand(eax, HeapNumber::kMantissaOffset));

    __ bind(&loaded);
  } else {  // UNTAGGED.
    CpuFeatureScope scope(masm, SSE2);
    if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatureScope sse4_scope(masm, SSE4_1);
      __ pextrd(edx, xmm1, 0x1);  // copy xmm1[63..32] to edx.
    } else {
      __ pshufd(xmm0, xmm1, 0x1);
      __ movd(edx, xmm0);
    }
    __ movd(ebx, xmm1);
  }

  // ST[0] or xmm1  == double value
  // ebx = low 32 bits of double value
  // edx = high 32 bits of double value
  // Compute hash (the shifts are arithmetic):
  //   h = (low ^ high); h ^= h >> 16; h ^= h >> 8; h = h & (cacheSize - 1);
  __ mov(ecx, ebx);
  __ xor_(ecx, edx);
  __ mov(eax, ecx);
  __ sar(eax, 16);
  __ xor_(ecx, eax);
  __ mov(eax, ecx);
  __ sar(eax, 8);
  __ xor_(ecx, eax);
  ASSERT(IsPowerOf2(TranscendentalCache::SubCache::kCacheSize));
  __ and_(ecx,
          Immediate(TranscendentalCache::SubCache::kCacheSize - 1));

  // ST[0] or xmm1 == double value.
  // ebx = low 32 bits of double value.
  // edx = high 32 bits of double value.
  // ecx = TranscendentalCache::hash(double value).
  ExternalReference cache_array =
      ExternalReference::transcendental_cache_array_address(masm->isolate());
  __ mov(eax, Immediate(cache_array));
  int cache_array_index =
      type_ * sizeof(masm->isolate()->transcendental_cache()->caches_[0]);
  __ mov(eax, Operand(eax, cache_array_index));
  // Eax points to the cache for the type type_.
  // If NULL, the cache hasn't been initialized yet, so go through runtime.
  __ test(eax, eax);
  __ j(zero, &runtime_call_clear_stack);
#ifdef DEBUG
  // Check that the layout of cache elements match expectations.
  { TranscendentalCache::SubCache::Element test_elem[2];
    char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
    char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
    char* elem_in0  = reinterpret_cast<char*>(&(test_elem[0].in[0]));
    char* elem_in1  = reinterpret_cast<char*>(&(test_elem[0].in[1]));
    char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
    CHECK_EQ(12, elem2_start - elem_start);  // Two uint_32's and a pointer.
    CHECK_EQ(0, elem_in0 - elem_start);
    CHECK_EQ(kIntSize, elem_in1 - elem_start);
    CHECK_EQ(2 * kIntSize, elem_out - elem_start);
  }
#endif
  // Find the address of the ecx'th entry in the cache, i.e., &eax[ecx*12].
  __ lea(ecx, Operand(ecx, ecx, times_2, 0));
  __ lea(ecx, Operand(eax, ecx, times_4, 0));
  // Check if cache matches: Double value is stored in uint32_t[2] array.
  Label cache_miss;
  __ cmp(ebx, Operand(ecx, 0));
  __ j(not_equal, &cache_miss, Label::kNear);
  __ cmp(edx, Operand(ecx, kIntSize));
  __ j(not_equal, &cache_miss, Label::kNear);
  // Cache hit!
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->transcendental_cache_hit(), 1);
  __ mov(eax, Operand(ecx, 2 * kIntSize));
  if (tagged) {
    __ fstp(0);
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    CpuFeatureScope scope(masm, SSE2);
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();
  }

  __ bind(&cache_miss);
  __ IncrementCounter(counters->transcendental_cache_miss(), 1);
  // Update cache with new value.
  // We are short on registers, so use no_reg as scratch.
  // This gives slightly larger code.
  if (tagged) {
    __ AllocateHeapNumber(eax, edi, no_reg, &runtime_call_clear_stack);
  } else {  // UNTAGGED.
    CpuFeatureScope scope(masm, SSE2);
    __ AllocateHeapNumber(eax, edi, no_reg, &skip_cache);
    __ sub(esp, Immediate(kDoubleSize));
    __ movdbl(Operand(esp, 0), xmm1);
    __ fld_d(Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));
  }
  GenerateOperation(masm, type_);
  __ mov(Operand(ecx, 0), ebx);
  __ mov(Operand(ecx, kIntSize), edx);
  __ mov(Operand(ecx, 2 * kIntSize), eax);
  __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
  if (tagged) {
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    CpuFeatureScope scope(masm, SSE2);
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();

    // Skip cache and return answer directly, only in untagged case.
    __ bind(&skip_cache);
    __ sub(esp, Immediate(kDoubleSize));
    __ movdbl(Operand(esp, 0), xmm1);
    __ fld_d(Operand(esp, 0));
    GenerateOperation(masm, type_);
    __ fstp_d(Operand(esp, 0));
    __ movdbl(xmm1, Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));
    // We return the value in xmm1 without adding it to the cache, but
    // we cause a scavenging GC so that future allocations will succeed.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      // Allocate an unused object bigger than a HeapNumber.
      __ push(Immediate(Smi::FromInt(2 * kDoubleSize)));
      __ CallRuntimeSaveDoubles(Runtime::kAllocateInNewSpace);
    }
    __ Ret();
  }

  // Call runtime, doing whatever allocation and cleanup is necessary.
  if (tagged) {
    __ bind(&runtime_call_clear_stack);
    __ fstp(0);
    __ bind(&runtime_call);
    ExternalReference runtime =
        ExternalReference(RuntimeFunction(), masm->isolate());
    __ TailCallExternalReference(runtime, 1, 1);
  } else {  // UNTAGGED.
    CpuFeatureScope scope(masm, SSE2);
    __ bind(&runtime_call_clear_stack);
    __ bind(&runtime_call);
    __ AllocateHeapNumber(eax, edi, no_reg, &skip_cache);
    __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm1);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(eax);
      __ CallRuntime(RuntimeFunction(), 1);
    }
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();
  }
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    case TranscendentalCache::TAN: return Runtime::kMath_tan;
    case TranscendentalCache::LOG: return Runtime::kMath_log;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void TranscendentalCacheStub::GenerateOperation(
    MacroAssembler* masm, TranscendentalCache::Type type) {
  // Only free register is edi.
  // Input value is on FP stack, and also in ebx/edx.
  // Input value is possibly in xmm1.
  // Address of result (a newly allocated HeapNumber) may be in eax.
  if (type == TranscendentalCache::SIN ||
      type == TranscendentalCache::COS ||
      type == TranscendentalCache::TAN) {
    // Both fsin and fcos require arguments in the range +/-2^63 and
    // return NaN for infinities and NaN. They can share all code except
    // the actual fsin/fcos operation.
    Label in_range, done;
    // If argument is outside the range -2^63..2^63, fsin/cos doesn't
    // work. We must reduce it to the appropriate range.
    __ mov(edi, edx);
    __ and_(edi, Immediate(0x7ff00000));  // Exponent only.
    int supported_exponent_limit =
        (63 + HeapNumber::kExponentBias) << HeapNumber::kExponentShift;
    __ cmp(edi, Immediate(supported_exponent_limit));
    __ j(below, &in_range, Label::kNear);
    // Check for infinity and NaN. Both return NaN for sin.
    __ cmp(edi, Immediate(0x7ff00000));
    Label non_nan_result;
    __ j(not_equal, &non_nan_result, Label::kNear);
    // Input is +/-Infinity or NaN. Result is NaN.
    __ fstp(0);
    // NaN is represented by 0x7ff8000000000000.
    __ push(Immediate(0x7ff80000));
    __ push(Immediate(0));
    __ fld_d(Operand(esp, 0));
    __ add(esp, Immediate(2 * kPointerSize));
    __ jmp(&done, Label::kNear);

    __ bind(&non_nan_result);

    // Use fpmod to restrict argument to the range +/-2*PI.
    __ mov(edi, eax);  // Save eax before using fnstsw_ax.
    __ fldpi();
    __ fadd(0);
    __ fld(1);
    // FPU Stack: input, 2*pi, input.
    {
      Label no_exceptions;
      __ fwait();
      __ fnstsw_ax();
      // Clear if Illegal Operand or Zero Division exceptions are set.
      __ test(eax, Immediate(5));
      __ j(zero, &no_exceptions, Label::kNear);
      __ fnclex();
      __ bind(&no_exceptions);
    }

    // Compute st(0) % st(1)
    {
      Label partial_remainder_loop;
      __ bind(&partial_remainder_loop);
      __ fprem1();
      __ fwait();
      __ fnstsw_ax();
      __ test(eax, Immediate(0x400 /* C2 */));
      // If C2 is set, computation only has partial result. Loop to
      // continue computation.
      __ j(not_zero, &partial_remainder_loop);
    }
    // FPU Stack: input, 2*pi, input % 2*pi
    __ fstp(2);
    __ fstp(0);
    __ mov(eax, edi);  // Restore eax (allocated HeapNumber pointer).

    // FPU Stack: input % 2*pi
    __ bind(&in_range);
    switch (type) {
      case TranscendentalCache::SIN:
        __ fsin();
        break;
      case TranscendentalCache::COS:
        __ fcos();
        break;
      case TranscendentalCache::TAN:
        // FPTAN calculates tangent onto st(0) and pushes 1.0 onto the
        // FP register stack.
        __ fptan();
        __ fstp(0);  // Pop FP register stack.
        break;
      default:
        UNREACHABLE();
    }
    __ bind(&done);
  } else {
    ASSERT(type == TranscendentalCache::LOG);
    __ fldln2();
    __ fxch();
    __ fyl2x();
  }
}


// Input: edx, eax are the left and right objects of a bit op.
// Output: eax, ecx are left and right integers for a bit op.
// Warning: can clobber inputs even when it jumps to |conversion_failure|!
void FloatingPointHelper::LoadUnknownsAsIntegers(
    MacroAssembler* masm,
    bool use_sse3,
    BinaryOpIC::TypeInfo left_type,
    BinaryOpIC::TypeInfo right_type,
    Label* conversion_failure) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  // Test if arg1 is a Smi.
  if (left_type == BinaryOpIC::SMI) {
    __ JumpIfNotSmi(edx, conversion_failure);
  } else {
    __ JumpIfNotSmi(edx, &arg1_is_object, Label::kNear);
  }

  __ SmiUntag(edx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  Factory* factory = masm->isolate()->factory();
  __ cmp(edx, factory->undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(edx, Immediate(0));
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ebx, factory->heap_number_map());
  __ j(not_equal, &check_undefined_arg1);

  // Get the untagged integer version of the edx heap number in ecx.
  if (left_type == BinaryOpIC::INT32 && CpuFeatures::IsSupported(SSE2)) {
    CpuFeatureScope use_sse2(masm, SSE2);
    ConvertHeapNumberToInt32(masm, edx, conversion_failure);
  } else {
    IntegerConvert(masm, edx, use_sse3, conversion_failure);
  }
  __ mov(edx, ecx);

  // Here edx has the untagged integer, eax has a Smi or a heap number.
  __ bind(&load_arg2);

  // Test if arg2 is a Smi.
  if (right_type == BinaryOpIC::SMI) {
    __ JumpIfNotSmi(eax, conversion_failure);
  } else {
    __ JumpIfNotSmi(eax, &arg2_is_object, Label::kNear);
  }

  __ SmiUntag(eax);
  __ mov(ecx, eax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ cmp(eax, factory->undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(ecx, Immediate(0));
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(ebx, factory->heap_number_map());
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the eax heap number in ecx.

  if (right_type == BinaryOpIC::INT32 && CpuFeatures::IsSupported(SSE2)) {
    CpuFeatureScope use_sse2(masm, SSE2);
    ConvertHeapNumberToInt32(masm, eax, conversion_failure);
  } else {
    IntegerConvert(masm, eax, use_sse3, conversion_failure);
  }

  __ bind(&done);
  __ mov(eax, edx);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ JumpIfSmi(number, &load_smi, Label::kNear);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done, Label::kNear);

  __ bind(&load_smi);
  __ SmiUntag(number);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm) {
  Label load_smi_edx, load_eax, load_smi_eax, done;
  // Load operand in edx into xmm0.
  __ JumpIfSmi(edx, &load_smi_edx, Label::kNear);
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));

  __ bind(&load_eax);
  // Load operand in eax into xmm1.
  __ JumpIfSmi(eax, &load_smi_eax, Label::kNear);
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ jmp(&done, Label::kNear);

  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, edx);
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);

  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, eax);
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm,
                                           Label* not_numbers) {
  Label load_smi_edx, load_eax, load_smi_eax, load_float_eax, done;
  // Load operand in edx into xmm0, or branch to not_numbers.
  __ JumpIfSmi(edx, &load_smi_edx, Label::kNear);
  Factory* factory = masm->isolate()->factory();
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(not_equal, not_numbers);  // Argument in edx is not a number.
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ bind(&load_eax);
  // Load operand in eax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(eax, &load_smi_eax, Label::kNear);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(equal, &load_float_eax, Label::kNear);
  __ jmp(not_numbers);  // Argument in eax is not a number.
  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, edx);
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);
  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, eax);
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.
  __ jmp(&done, Label::kNear);
  __ bind(&load_float_eax);
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Smis(MacroAssembler* masm,
                                       Register scratch) {
  const Register left = edx;
  const Register right = eax;
  __ mov(scratch, left);
  ASSERT(!scratch.is(right));  // We're about to clobber scratch.
  __ SmiUntag(scratch);
  __ cvtsi2sd(xmm0, scratch);

  __ mov(scratch, right);
  __ SmiUntag(scratch);
  __ cvtsi2sd(xmm1, scratch);
}


void FloatingPointHelper::CheckSSE2OperandsAreInt32(MacroAssembler* masm,
                                                    Label* non_int32,
                                                    Register scratch) {
  CheckSSE2OperandIsInt32(masm, non_int32, xmm0, scratch, scratch, xmm2);
  CheckSSE2OperandIsInt32(masm, non_int32, xmm1, scratch, scratch, xmm2);
}


void FloatingPointHelper::CheckSSE2OperandIsInt32(MacroAssembler* masm,
                                                  Label* non_int32,
                                                  XMMRegister operand,
                                                  Register int32_result,
                                                  Register scratch,
                                                  XMMRegister xmm_scratch) {
  __ cvttsd2si(int32_result, Operand(operand));
  __ cvtsi2sd(xmm_scratch, int32_result);
  __ pcmpeqd(xmm_scratch, operand);
  __ movmskps(scratch, xmm_scratch);
  // Two least significant bits should be both set.
  __ not_(scratch);
  __ test(scratch, Immediate(3));
  __ j(not_zero, non_int32);
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            Register scratch,
                                            ArgLocation arg_location) {
  Label load_smi_1, load_smi_2, done_load_1, done;
  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, edx);
  } else {
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
  }
  __ JumpIfSmi(scratch, &load_smi_1, Label::kNear);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ bind(&done_load_1);

  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, eax);
  } else {
    __ mov(scratch, Operand(esp, 1 * kPointerSize));
  }
  __ JumpIfSmi(scratch, &load_smi_2, Label::kNear);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ jmp(&done, Label::kNear);

  __ bind(&load_smi_1);
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);
  __ jmp(&done_load_1);

  __ bind(&load_smi_2);
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);

  __ bind(&done);
}


void FloatingPointHelper::LoadFloatSmis(MacroAssembler* masm,
                                        Register scratch) {
  const Register left = edx;
  const Register right = eax;
  __ mov(scratch, left);
  ASSERT(!scratch.is(right));  // We're about to clobber scratch.
  __ SmiUntag(scratch);
  __ push(scratch);
  __ fild_s(Operand(esp, 0));

  __ mov(scratch, right);
  __ SmiUntag(scratch);
  __ mov(Operand(esp, 0), scratch);
  __ fild_s(Operand(esp, 0));
  __ pop(scratch);
}


void FloatingPointHelper::CheckFloatOperands(MacroAssembler* masm,
                                             Label* non_float,
                                             Register scratch) {
  Label test_other, done;
  // Test if both operands are floats or smi -> scratch=k_is_float;
  // Otherwise scratch = k_not_float.
  __ JumpIfSmi(edx, &test_other, Label::kNear);
  __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
  Factory* factory = masm->isolate()->factory();
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in edx is not a number -> NaN

  __ bind(&test_other);
  __ JumpIfSmi(eax, &done, Label::kNear);
  __ mov(scratch, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in eax is not a number -> NaN

  // Fall-through: Both operands are numbers.
  __ bind(&done);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  CpuFeatureScope use_sse2(masm, SSE2);
  Factory* factory = masm->isolate()->factory();
  const Register exponent = eax;
  const Register base = edx;
  const Register scratch = ecx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ mov(scratch, Immediate(1));
  __ cvtsi2sd(double_result, scratch);

  if (exponent_type_ == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack.
    __ mov(base, Operand(esp, 2 * kPointerSize));
    __ mov(exponent, Operand(esp, 1 * kPointerSize));

    __ JumpIfSmi(base, &base_is_smi, Label::kNear);
    __ cmp(FieldOperand(base, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(not_equal, &call_runtime);

    __ movdbl(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiUntag(base);
    __ cvtsi2sd(double_base, base);

    __ bind(&unpack_exponent);
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ cmp(FieldOperand(exponent, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(not_equal, &call_runtime);
    __ movdbl(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type_ == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ movdbl(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type_ != INTEGER) {
    Label fast_power;
    // Detect integer exponents stored as double.
    __ cvttsd2si(exponent, Operand(double_exponent));
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cmp(exponent, Immediate(0x80000000u));
    __ j(equal, &call_runtime);
    __ cvtsi2sd(double_scratch, exponent);
    // Already ruled out NaNs for exponent.
    __ ucomisd(double_exponent, double_scratch);
    __ j(equal, &int_exponent);

    if (exponent_type_ == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label continue_sqrt, continue_rsqrt, not_plus_half;
      // Test for 0.5.
      // Load double_scratch with 0.5.
      __ mov(scratch, Immediate(0x3F000000u));
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      // According to IEEE-754, single-precision -Infinity has the highest
      // 9 bits set and the lowest 23 bits cleared.
      __ mov(scratch, 0xFF800000u);
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      __ ucomisd(double_base, double_scratch);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_sqrt, Label::kNear);
      __ j(carry, &continue_sqrt, Label::kNear);

      // Set result to Infinity in the special case.
      __ xorps(double_result, double_result);
      __ subsd(double_result, double_scratch);
      __ jmp(&done);

      __ bind(&continue_sqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_scratch, double_scratch);
      __ addsd(double_scratch, double_base);  // Convert -0 to +0.
      __ sqrtsd(double_result, double_scratch);
      __ jmp(&done);

      // Test for -0.5.
      __ bind(&not_plus_half);
      // Load double_exponent with -0.5 by substracting 1.
      __ subsd(double_scratch, double_result);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &fast_power, Label::kNear);

      // Calculates reciprocal of square root of base.  Check for the special
      // case of Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      // According to IEEE-754, single-precision -Infinity has the highest
      // 9 bits set and the lowest 23 bits cleared.
      __ mov(scratch, 0xFF800000u);
      __ movd(double_scratch, scratch);
      __ cvtss2sd(double_scratch, double_scratch);
      __ ucomisd(double_base, double_scratch);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_rsqrt, Label::kNear);
      __ j(carry, &continue_rsqrt, Label::kNear);

      // Set result to 0 in the special case.
      __ xorps(double_result, double_result);
      __ jmp(&done);

      __ bind(&continue_rsqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_exponent, double_exponent);
      __ addsd(double_exponent, double_base);  // Convert -0 to +0.
      __ sqrtsd(double_exponent, double_exponent);
      __ divsd(double_result, double_exponent);
      __ jmp(&done);
    }

    // Using FPU instructions to calculate power.
    Label fast_power_failed;
    __ bind(&fast_power);
    __ fnclex();  // Clear flags to catch exceptions later.
    // Transfer (B)ase and (E)xponent onto the FPU register stack.
    __ sub(esp, Immediate(kDoubleSize));
    __ movdbl(Operand(esp, 0), double_exponent);
    __ fld_d(Operand(esp, 0));  // E
    __ movdbl(Operand(esp, 0), double_base);
    __ fld_d(Operand(esp, 0));  // B, E

    // Exponent is in st(1) and base is in st(0)
    // B ^ E = (2^(E * log2(B)) - 1) + 1 = (2^X - 1) + 1 for X = E * log2(B)
    // FYL2X calculates st(1) * log2(st(0))
    __ fyl2x();    // X
    __ fld(0);     // X, X
    __ frndint();  // rnd(X), X
    __ fsub(1);    // rnd(X), X-rnd(X)
    __ fxch(1);    // X - rnd(X), rnd(X)
    // F2XM1 calculates 2^st(0) - 1 for -1 < st(0) < 1
    __ f2xm1();    // 2^(X-rnd(X)) - 1, rnd(X)
    __ fld1();     // 1, 2^(X-rnd(X)) - 1, rnd(X)
    __ faddp(1);   // 2^(X-rnd(X)), rnd(X)
    // FSCALE calculates st(0) * 2^st(1)
    __ fscale();   // 2^X, rnd(X)
    __ fstp(1);    // 2^X
    // Bail out to runtime in case of exceptions in the status word.
    __ fnstsw_ax();
    __ test_b(eax, 0x5F);  // We check for all but precision exception.
    __ j(not_zero, &fast_power_failed, Label::kNear);
    __ fstp_d(Operand(esp, 0));
    __ movdbl(double_result, Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done);

    __ bind(&fast_power_failed);
    __ fninit();
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&call_runtime);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  __ mov(scratch, exponent);  // Back up exponent.
  __ movsd(double_scratch, double_base);  // Back up base.
  __ movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ test(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ neg(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shr(scratch, 1);
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shr(scratch, 1);
  __ mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // scratch has the original value of the exponent - if the exponent is
  // negative, return 1/result.
  __ test(exponent, exponent);
  __ j(positive, &done);
  __ divsd(double_scratch2, double_result);
  __ movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ xorps(double_scratch2, double_scratch2);
  __ ucomisd(double_scratch2, double_result);  // Result cannot be NaN.
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // exponent is a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ cvtsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  Counters* counters = masm->isolate()->counters();
  if (exponent_type_ == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMath_pow_cfunction, 2, 1);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in exponent.
    __ bind(&done);
    __ AllocateHeapNumber(eax, scratch, base, &call_runtime);
    __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), double_result);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(4, scratch);
      __ movdbl(Operand(esp, 0 * kDoubleSize), double_base);
      __ movdbl(Operand(esp, 1 * kDoubleSize), double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(masm->isolate()), 4);
    }
    // Return value is in st(0) on ia32.
    // Store it into the (fixed) result register.
    __ sub(esp, Immediate(kDoubleSize));
    __ fstp_d(Operand(esp, 0));
    __ movdbl(double_result, Operand(esp, 0));
    __ add(esp, Immediate(kDoubleSize));

    __ bind(&done);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(0);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  if (kind() == Code::KEYED_LOAD_IC) {
    __ cmp(ecx, Immediate(masm->isolate()->factory()->prototype_string()));
    __ j(not_equal, &miss);
  }

  StubCompiler::GenerateLoadFunctionPrototype(masm, edx, eax, ebx, &miss);
  __ bind(&miss);
  StubCompiler::TailCallBuiltin(masm, StubCompiler::MissBuiltin(kind()));
}


void StringLengthStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  if (kind() == Code::KEYED_LOAD_IC) {
    __ cmp(ecx, Immediate(masm->isolate()->factory()->length_string()));
    __ j(not_equal, &miss);
  }

  StubCompiler::GenerateLoadStringLength(masm, edx, eax, ebx, &miss,
                                         support_wrapper_);
  __ bind(&miss);
  StubCompiler::TailCallBuiltin(masm, StubCompiler::MissBuiltin(kind()));
}


void StoreArrayLengthStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  //
  // This accepts as a receiver anything JSArray::SetElementsLength accepts
  // (currently anything except for external arrays which means anything with
  // elements of FixedArray type).  Value must be a number, but only smis are
  // accepted as the most common case.

  Label miss;

  Register receiver = edx;
  Register value = eax;
  Register scratch = ebx;

  if (kind() == Code::KEYED_STORE_IC) {
    __ cmp(ecx, Immediate(masm->isolate()->factory()->length_string()));
    __ j(not_equal, &miss);
  }

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that elements are FixedArray.
  // We rely on StoreIC_ArrayLength below to deal with all types of
  // fast elements (including COW).
  __ mov(scratch, FieldOperand(receiver, JSArray::kElementsOffset));
  __ CmpObjectType(scratch, FIXED_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that the array has fast properties, otherwise the length
  // property might have been redefined.
  __ mov(scratch, FieldOperand(receiver, JSArray::kPropertiesOffset));
  __ CompareRoot(FieldOperand(scratch, FixedArray::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(equal, &miss);

  // Check that value is a smi.
  __ JumpIfNotSmi(value, &miss);

  // Prepare tail call to StoreIC_ArrayLength.
  __ pop(scratch);
  __ push(receiver);
  __ push(value);
  __ push(scratch);  // return address

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kStoreIC_ArrayLength), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&miss);

  StubCompiler::TailCallBuiltin(masm, StubCompiler::MissBuiltin(kind()));
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in edx and the parameter count is in eax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(edx, &slow, Label::kNear);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(ebx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor, Label::kNear);

  // Check index against formal parameters count limit passed in
  // through register eax. Use unsigned comparison to get negative
  // check for free.
  __ cmp(edx, eax);
  __ j(above_equal, &slow, Label::kNear);

  // Read the argument from the stack and return it.
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);  // Shifting code depends on these.
  __ lea(ebx, Operand(ebp, eax, times_2, 0));
  __ neg(edx);
  __ mov(eax, Operand(ebx, edx, times_2, kDisplacement));
  __ ret(0);

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ mov(ecx, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmp(edx, ecx);
  __ j(above_equal, &slow, Label::kNear);

  // Read the argument from the stack and return it.
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);  // Shifting code depends on these.
  __ lea(ebx, Operand(ebx, ecx, times_2, 0));
  __ neg(edx);
  __ mov(eax, Operand(ebx, edx, times_2, kDisplacement));
  __ ret(0);

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ pop(ebx);  // Return address.
  __ push(edx);
  __ push(ebx);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictSlow(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[4] : number of parameters
  // esp[8] : receiver displacement
  // esp[12] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &runtime, Label::kNear);

  // Patch the arguments.length and the parameters pointer.
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(Operand(esp, 1 * kPointerSize), ecx);
  __ lea(edx, Operand(edx, ecx, times_2,
              StandardFrameConstants::kCallerSPOffset));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictFast(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[4] : number of parameters (tagged)
  // esp[8] : receiver displacement
  // esp[12] : function

  // ebx = parameter count (tagged)
  __ mov(ebx, Operand(esp, 1 * kPointerSize));

  // Check if the calling frame is an arguments adaptor frame.
  // TODO(rossberg): Factor out some of the bits that are shared with the other
  // Generate* functions.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame, Label::kNear);

  // No adaptor, parameter count = argument count.
  __ mov(ecx, ebx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lea(edx, Operand(edx, ecx, times_2,
                      StandardFrameConstants::kCallerSPOffset));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  // ebx = parameter count (tagged)
  // ecx = argument count (tagged)
  // esp[4] = parameter count (tagged)
  // esp[8] = address of receiver argument
  // Compute the mapped parameter count = min(ebx, ecx) in ebx.
  __ cmp(ebx, ecx);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ mov(ebx, ecx);

  __ bind(&try_allocate);

  // Save mapped parameter count.
  __ push(ebx);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ lea(ebx, Operand(ebx, times_2, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ lea(ebx, Operand(ebx, ecx, times_2, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ add(ebx, Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(ebx, eax, edx, edi, &runtime, TAG_OBJECT);

  // eax = address of new object(s) (tagged)
  // ecx = argument count (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[8] = parameter count (tagged)
  // esp[12] = address of receiver argument
  // Get the arguments boilerplate from the current native context into edi.
  Label has_mapped_parameters, copy;
  __ mov(edi, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(edi, FieldOperand(edi, GlobalObject::kNativeContextOffset));
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ test(ebx, ebx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);
  __ mov(edi, Operand(edi,
         Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX)));
  __ jmp(&copy, Label::kNear);

  __ bind(&has_mapped_parameters);
  __ mov(edi, Operand(edi,
            Context::SlotOffset(Context::ALIASED_ARGUMENTS_BOILERPLATE_INDEX)));
  __ bind(&copy);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (tagged)
  // edi = address of boilerplate object (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[8] = parameter count (tagged)
  // esp[12] = address of receiver argument
  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ mov(edx, FieldOperand(edi, i));
    __ mov(FieldOperand(eax, i), edx);
  }

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ mov(edx, Operand(esp, 4 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize +
                      Heap::kArgumentsCalleeIndex * kPointerSize),
         edx);

  // Use the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ mov(FieldOperand(eax, JSObject::kHeaderSize +
                      Heap::kArgumentsLengthIndex * kPointerSize),
         ecx);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, edi will point there, otherwise to the
  // backing store.
  __ lea(edi, Operand(eax, Heap::kArgumentsObjectSize));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (tagged)
  // edi = address of parameter map or backing store (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[8] = parameter count (tagged)
  // esp[12] = address of receiver argument
  // Free a register.
  __ push(eax);

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &skip_parameter_map);

  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(FACTORY->non_strict_arguments_elements_map()));
  __ lea(eax, Operand(ebx, reinterpret_cast<intptr_t>(Smi::FromInt(2))));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), eax);
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 0 * kPointerSize), esi);
  __ lea(eax, Operand(edi, ebx, times_2, kParameterMapHeaderSize));
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 1 * kPointerSize), eax);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ push(ecx);
  __ mov(eax, Operand(esp, 2 * kPointerSize));
  __ mov(ebx, Immediate(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ add(ebx, Operand(esp, 4 * kPointerSize));
  __ sub(ebx, eax);
  __ mov(ecx, FACTORY->the_hole_value());
  __ mov(edx, edi);
  __ lea(edi, Operand(edi, eax, times_2, kParameterMapHeaderSize));
  // eax = loop variable (tagged)
  // ebx = mapping index (tagged)
  // ecx = the hole value
  // edx = address of parameter map (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = argument count (tagged)
  // esp[4] = address of new object (tagged)
  // esp[8] = mapped parameter count (tagged)
  // esp[16] = parameter count (tagged)
  // esp[20] = address of receiver argument
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ sub(eax, Immediate(Smi::FromInt(1)));
  __ mov(FieldOperand(edx, eax, times_2, kParameterMapHeaderSize), ebx);
  __ mov(FieldOperand(edi, eax, times_2, FixedArray::kHeaderSize), ecx);
  __ add(ebx, Immediate(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ test(eax, eax);
  __ j(not_zero, &parameters_loop, Label::kNear);
  __ pop(ecx);

  __ bind(&skip_parameter_map);

  // ecx = argument count (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = address of new object (tagged)
  // esp[4] = mapped parameter count (tagged)
  // esp[12] = parameter count (tagged)
  // esp[16] = address of receiver argument
  // Copy arguments header and remaining slots (if there are any).
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(FACTORY->fixed_array_map()));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);

  Label arguments_loop, arguments_test;
  __ mov(ebx, Operand(esp, 1 * kPointerSize));
  __ mov(edx, Operand(esp, 4 * kPointerSize));
  __ sub(edx, ebx);  // Is there a smarter way to do negative scaling?
  __ sub(edx, ebx);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ sub(edx, Immediate(kPointerSize));
  __ mov(eax, Operand(edx, 0));
  __ mov(FieldOperand(edi, ebx, times_2, FixedArray::kHeaderSize), eax);
  __ add(ebx, Immediate(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ cmp(ebx, ecx);
  __ j(less, &arguments_loop, Label::kNear);

  // Restore.
  __ pop(eax);  // Address of arguments object.
  __ pop(ebx);  // Parameter count.

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ pop(eax);  // Remove saved parameter count.
  __ mov(Operand(esp, 1 * kPointerSize), ecx);  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[4] : number of parameters
  // esp[8] : receiver displacement
  // esp[12] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(ecx, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame, Label::kNear);

  // Get the length from the frame.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ jmp(&try_allocate, Label::kNear);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(Operand(esp, 1 * kPointerSize), ecx);
  __ lea(edx, Operand(edx, ecx, times_2,
                      StandardFrameConstants::kCallerSPOffset));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ test(ecx, ecx);
  __ j(zero, &add_arguments_object, Label::kNear);
  __ lea(ecx, Operand(ecx, times_2, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ add(ecx, Immediate(Heap::kArgumentsObjectSizeStrict));

  // Do the allocation of both objects in one go.
  __ Allocate(ecx, eax, edx, ebx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current native context.
  __ mov(edi, Operand(esi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ mov(edi, FieldOperand(edi, GlobalObject::kNativeContextOffset));
  const int offset =
      Context::SlotOffset(Context::STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX);
  __ mov(edi, Operand(edi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ mov(ebx, FieldOperand(edi, i));
    __ mov(FieldOperand(eax, i), ebx);
  }

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize +
                      Heap::kArgumentsLengthIndex * kPointerSize),
         ecx);

  // If there are no actual arguments, we're done.
  Label done;
  __ test(ecx, ecx);
  __ j(zero, &done, Label::kNear);

  // Get the parameters pointer from the stack.
  __ mov(edx, Operand(esp, 2 * kPointerSize));

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(edi, Operand(eax, Heap::kArgumentsObjectSizeStrict));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(FACTORY->fixed_array_map()));

  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);
  // Untag the length for the loop below.
  __ SmiUntag(ecx);

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ mov(ebx, Operand(edx, -1 * kPointerSize));  // Skip receiver.
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize), ebx);
  __ add(edi, Immediate(kPointerSize));
  __ sub(edx, Immediate(kPointerSize));
  __ dec(ecx);
  __ j(not_zero, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewStrictArgumentsFast, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: last_match_info (expected JSArray)
  //  esp[8]: previous index
  //  esp[12]: subject string
  //  esp[16]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;
  Factory* factory = masm->isolate()->factory();

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(
          masm->isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(masm->isolate());
  __ mov(ebx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ test(ebx, ebx);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ecx);
  __ j(not_equal, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Check(not_zero, "Unexpected type for RegExp data, FixedArray expected");
    __ CmpObjectType(ecx, FIXED_ARRAY_TYPE, ebx);
    __ Check(equal, "Unexpected type for RegExp data, FixedArray expected");
  }

  // ecx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ mov(ebx, FieldOperand(ecx, JSRegExp::kDataTagOffset));
  __ cmp(ebx, Immediate(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ j(not_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Multiplying by 2 comes for free since edx is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmp(edx, Isolate::kJSRegexpStaticOffsetsVectorSize - 2);
  __ j(above, &runtime);

  // Reset offset for possibly sliced string.
  __ Set(edi, Immediate(0));
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ JumpIfSmi(eax, &runtime);
  __ mov(edx, eax);  // Make a copy of the original subject string.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));

  // eax: subject string
  // edx: subject string
  // ebx: subject string instance type
  // ecx: RegExp data (FixedArray)
  // Handle subject string according to its encoding and representation:
  // (1) Sequential two byte?  If yes, go to (9).
  // (2) Sequential one byte?  If yes, go to (6).
  // (3) Anything but sequential or cons?  If yes, go to (7).
  // (4) Cons string.  If the string is flat, replace subject with first string.
  //     Otherwise bailout.
  // (5a) Is subject sequential two byte?  If yes, go to (9).
  // (5b) Is subject external?  If yes, go to (8).
  // (6) One byte sequential.  Load regexp code for one byte.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (7) Not a long external string?  If yes, go to (10).
  // (8) External string.  Make it, offset-wise, look like a sequential string.
  // (8a) Is the external string one byte?  If yes, go to (6).
  // (9) Two byte sequential.  Load regexp code for one byte. Go to (E).
  // (10) Short external string or not a string?  If yes, bail out to runtime.
  // (11) Sliced string.  Replace subject with parent. Go to (5a).

  Label seq_one_byte_string /* 6 */, seq_two_byte_string /* 9 */,
        external_string /* 8 */, check_underlying /* 5a */,
        not_seq_nor_cons /* 7 */, check_code /* E */,
        not_long_external /* 10 */;

  // (1) Sequential two byte?  If yes, go to (9).
  __ and_(ebx, kIsNotStringMask |
               kStringRepresentationMask |
               kStringEncodingMask |
               kShortExternalStringMask);
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).

  // (2) Sequential one byte?  If yes, go to (6).
  // Any other sequential string must be one byte.
  __ and_(ebx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kShortExternalStringMask));
  __ j(zero, &seq_one_byte_string, Label::kNear);  // Go to (6).

  // (3) Anything but sequential or cons?  If yes, go to (7).
  // We check whether the subject string is a cons, since sequential strings
  // have already been covered.
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ cmp(ebx, Immediate(kExternalStringTag));
  __ j(greater_equal, &not_seq_nor_cons);  // Go to (7).

  // (4) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ cmp(FieldOperand(eax, ConsString::kSecondOffset), factory->empty_string());
  __ j(not_equal, &runtime);
  __ mov(eax, FieldOperand(eax, ConsString::kFirstOffset));
  __ bind(&check_underlying);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ mov(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));

  // (5a) Is subject sequential two byte?  If yes, go to (9).
  __ test_b(ebx, kStringRepresentationMask | kStringEncodingMask);
  STATIC_ASSERT((kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).
  // (5b) Is subject external?  If yes, go to (8).
  __ test_b(ebx, kStringRepresentationMask);
  // The underlying external string is never a short external string.
  STATIC_CHECK(ExternalString::kMaxShortLength < ConsString::kMinLength);
  STATIC_CHECK(ExternalString::kMaxShortLength < SlicedString::kMinLength);
  __ j(not_zero, &external_string);  // Go to (8).

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (6) One byte sequential.  Load regexp code for one byte.
  __ bind(&seq_one_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataAsciiCodeOffset));
  __ Set(ecx, Immediate(1));  // Type is one byte.

  // (E) Carry on.  String handling is done.
  __ bind(&check_code);
  // edx: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(edx, &runtime);

  // eax: subject string
  // ebx: previous index (smi)
  // edx: code
  // ecx: encoding of subject string (1 if ASCII, 0 if two_byte);
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 9;
  __ EnterApiExitFrame(kRegExpExecuteArguments);

  // Argument 9: Pass current isolate address.
  __ mov(Operand(esp, 8 * kPointerSize),
      Immediate(ExternalReference::isolate_address(masm->isolate())));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ mov(Operand(esp, 7 * kPointerSize), Immediate(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ mov(esi, Operand::StaticVariable(address_of_regexp_stack_memory_address));
  __ add(esi, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ mov(Operand(esp, 6 * kPointerSize), esi);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  __ mov(Operand(esp, 5 * kPointerSize), Immediate(0));

  // Argument 5: static offsets vector buffer.
  __ mov(Operand(esp, 4 * kPointerSize),
         Immediate(ExternalReference::address_of_static_offsets_vector(
             masm->isolate())));

  // Argument 2: Previous index.
  __ SmiUntag(ebx);
  __ mov(Operand(esp, 1 * kPointerSize), ebx);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use ebp, which points exactly to one pointer size below the previous esp.
  // (Because creating a new stack frame pushes the previous ebp onto the stack
  // and thereby moves up esp by one kPointerSize.)
  __ mov(esi, Operand(ebp, kSubjectOffset + kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), esi);

  // esi: original subject string
  // eax: underlying subject string
  // ebx: previous index
  // ecx: encoding of subject string (1 if ASCII 0 if two_byte);
  // edx: code
  // Argument 4: End of string data
  // Argument 3: Start of string data
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ mov(esi, FieldOperand(esi, String::kLengthOffset));
  __ add(esi, edi);  // Calculate input end wrt offset.
  __ SmiUntag(edi);
  __ add(ebx, edi);  // Calculate input start wrt offset.

  // ebx: start index of the input string
  // esi: end index of the input string
  Label setup_two_byte, setup_rest;
  __ test(ecx, ecx);
  __ j(zero, &setup_two_byte, Label::kNear);
  __ SmiUntag(esi);
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.
  __ jmp(&setup_rest, Label::kNear);

  __ bind(&setup_two_byte);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);  // esi is smi (powered by 2).
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_2, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.

  __ bind(&setup_rest);

  // Locate the code entry and call it.
  __ add(edx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(edx);

  // Drop arguments and come back to JS mode.
  __ LeaveApiExitFrame();

  // Check the result.
  Label success;
  __ cmp(eax, 1);
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ j(equal, &success);
  Label failure;
  __ cmp(eax, NativeRegExpMacroAssembler::FAILURE);
  __ j(equal, &failure);
  __ cmp(eax, NativeRegExpMacroAssembler::EXCEPTION);
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      masm->isolate());
  __ mov(edx, Immediate(masm->isolate()->factory()->the_hole_value()));
  __ mov(eax, Operand::StaticVariable(pending_exception));
  __ cmp(edx, eax);
  __ j(equal, &runtime);
  // For exception, throw the exception again.

  // Clear the pending exception variable.
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(eax, factory->termination_exception());
  Label throw_termination_exception;
  __ j(equal, &throw_termination_exception, Label::kNear);

  // Handle normal exception by following handler chain.
  __ Throw(eax);

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(eax);

  __ bind(&failure);
  // For failure to match, return null.
  __ mov(eax, factory->null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(edx, Immediate(2));  // edx was a smi.

  // edx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  // Check that the fourth object is a JSArray object.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));
  __ mov(eax, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(eax, factory->fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ mov(eax, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ SmiUntag(eax);
  __ sub(eax, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmp(edx, eax);
  __ j(greater, &runtime);

  // ebx: last_match_info backing store (FixedArray)
  // edx: number of capture registers
  // Store the capture count.
  __ SmiTag(edx);  // Number of capture registers to smi.
  __ mov(FieldOperand(ebx, RegExpImpl::kLastCaptureCountOffset), edx);
  __ SmiUntag(edx);  // Number of capture registers back from smi.
  // Store last subject and last input.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(ecx, eax);
  __ mov(FieldOperand(ebx, RegExpImpl::kLastSubjectOffset), eax);
  __ RecordWriteField(ebx,
                      RegExpImpl::kLastSubjectOffset,
                      eax,
                      edi,
                      kDontSaveFPRegs);
  __ mov(eax, ecx);
  __ mov(FieldOperand(ebx, RegExpImpl::kLastInputOffset), eax);
  __ RecordWriteField(ebx,
                      RegExpImpl::kLastInputOffset,
                      eax,
                      edi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(masm->isolate());
  __ mov(ecx, Immediate(address_of_static_offsets_vector));

  // ebx: last_match_info backing store (FixedArray)
  // ecx: offsets vector
  // edx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ sub(edx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer.
  __ mov(edi, Operand(ecx, edx, times_int_size, 0));
  __ SmiTag(edi);
  // Store the smi value in the last match info.
  __ mov(FieldOperand(ebx,
                      edx,
                      times_pointer_size,
                      RegExpImpl::kFirstCaptureOffset),
                      edi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);

  // Deferred code for string handling.
  // (7) Not a long external string?  If yes, go to (10).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set from (3).
  __ j(greater, &not_long_external, Label::kNear);  // Go to (10).

  // (8) External string.  Short external strings have been ruled out.
  __ bind(&external_string);
  // Reload instance type.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test_b(ebx, kIsIndirectStringMask);
    __ Assert(zero, "external string expected, but not found");
  }
  __ mov(eax, FieldOperand(eax, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(eax, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // (8a) Is the external string one byte?  If yes, go to (6).
  __ test_b(ebx, kStringEncodingMask);
  __ j(not_zero, &seq_one_byte_string);  // Goto (6).

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (9) Two byte sequential.  Load regexp code for one byte. Go to (E).
  __ bind(&seq_two_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataUC16CodeOffset));
  __ Set(ecx, Immediate(0));  // Type is two byte.
  __ jmp(&check_code);  // Go to (E).

  // (10) Not a string or a short external string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ test(ebx, Immediate(kIsNotStringMask | kShortExternalStringTag));
  __ j(not_zero, &runtime);

  // (11) Sliced string.  Replace subject with parent.  Go to (5a).
  // Load offset into edi and replace subject string with parent.
  __ mov(edi, FieldOperand(eax, SlicedString::kOffsetOffset));
  __ mov(eax, FieldOperand(eax, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (5a).
#endif  // V8_INTERPRETED_REGEXP
}


void RegExpConstructResultStub::Generate(MacroAssembler* masm) {
  const int kMaxInlineLength = 100;
  Label slowcase;
  Label done;
  __ mov(ebx, Operand(esp, kPointerSize * 3));
  __ JumpIfNotSmi(ebx, &slowcase);
  __ cmp(ebx, Immediate(Smi::FromInt(kMaxInlineLength)));
  __ j(above, &slowcase);
  // Smi-tagging is equivalent to multiplying by 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // Allocate RegExpResult followed by FixedArray with size in ebx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  __ Allocate(JSRegExpResult::kSize + FixedArray::kHeaderSize,
              times_pointer_size,
              ebx,  // In: Number of elements as a smi
              REGISTER_VALUE_IS_SMI,
              eax,  // Out: Start of allocation (tagged).
              ecx,  // Out: End of allocation.
              edx,  // Scratch register
              &slowcase,
              TAG_OBJECT);
  // eax: Start of allocated area, object-tagged.

  // Set JSArray map to global.regexp_result_map().
  // Set empty properties FixedArray.
  // Set elements to point to FixedArray allocated right after the JSArray.
  // Interleave operations for better latency.
  __ mov(edx, ContextOperand(esi, Context::GLOBAL_OBJECT_INDEX));
  Factory* factory = masm->isolate()->factory();
  __ mov(ecx, Immediate(factory->empty_fixed_array()));
  __ lea(ebx, Operand(eax, JSRegExpResult::kSize));
  __ mov(edx, FieldOperand(edx, GlobalObject::kNativeContextOffset));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset), ecx);
  __ mov(edx, ContextOperand(edx, Context::REGEXP_RESULT_MAP_INDEX));
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), edx);

  // Set input, index and length fields from arguments.
  __ mov(ecx, Operand(esp, kPointerSize * 1));
  __ mov(FieldOperand(eax, JSRegExpResult::kInputOffset), ecx);
  __ mov(ecx, Operand(esp, kPointerSize * 2));
  __ mov(FieldOperand(eax, JSRegExpResult::kIndexOffset), ecx);
  __ mov(ecx, Operand(esp, kPointerSize * 3));
  __ mov(FieldOperand(eax, JSArray::kLengthOffset), ecx);

  // Fill out the elements FixedArray.
  // eax: JSArray.
  // ebx: FixedArray.
  // ecx: Number of elements in array, as smi.

  // Set map.
  __ mov(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(factory->fixed_array_map()));
  // Set length.
  __ mov(FieldOperand(ebx, FixedArray::kLengthOffset), ecx);
  // Fill contents of fixed-array with undefined.
  __ SmiUntag(ecx);
  __ mov(edx, Immediate(factory->undefined_value()));
  __ lea(ebx, FieldOperand(ebx, FixedArray::kHeaderSize));
  // Fill fixed array elements with undefined.
  // eax: JSArray.
  // ecx: Number of elements to fill.
  // ebx: Start of elements in FixedArray.
  // edx: undefined.
  Label loop;
  __ test(ecx, ecx);
  __ bind(&loop);
  __ j(less_equal, &done, Label::kNear);  // Jump if ecx is negative or zero.
  __ sub(ecx, Immediate(1));
  __ mov(Operand(ebx, ecx, times_pointer_size, 0), edx);
  __ jmp(&loop);

  __ bind(&done);
  __ ret(3 * kPointerSize);

  __ bind(&slowcase);
  __ TailCallRuntime(Runtime::kRegExpConstructResult, 3, 1);
}


void NumberToStringStub::GenerateLookupNumberStringCache(MacroAssembler* masm,
                                                         Register object,
                                                         Register result,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         bool object_is_smi,
                                                         Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch1;
  Register scratch = scratch2;

  // Load the number string cache.
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(masm->isolate());
  __ mov(scratch, Immediate(Heap::kNumberStringCacheRootIndex));
  __ mov(number_string_cache,
         Operand::StaticArray(scratch, times_pointer_size, roots_array_start));
  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ mov(mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  __ shr(mask, kSmiTagSize + 1);  // Untag length and divide it by two.
  __ sub(mask, Immediate(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label smi_hash_calculated;
  Label load_result_from_cache;
  if (object_is_smi) {
    __ mov(scratch, object);
    __ SmiUntag(scratch);
  } else {
    Label not_smi;
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfNotSmi(object, &not_smi, Label::kNear);
    __ mov(scratch, object);
    __ SmiUntag(scratch);
    __ jmp(&smi_hash_calculated, Label::kNear);
    __ bind(&not_smi);
    __ cmp(FieldOperand(object, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, not_found);
    STATIC_ASSERT(8 == kDoubleSize);
    __ mov(scratch, FieldOperand(object, HeapNumber::kValueOffset));
    __ xor_(scratch, FieldOperand(object, HeapNumber::kValueOffset + 4));
    // Object is heap number and hash is now in scratch. Calculate cache index.
    __ and_(scratch, mask);
    Register index = scratch;
    Register probe = mask;
    __ mov(probe,
           FieldOperand(number_string_cache,
                        index,
                        times_twice_pointer_size,
                        FixedArray::kHeaderSize));
    __ JumpIfSmi(probe, not_found);
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatureScope fscope(masm, SSE2);
      __ movdbl(xmm0, FieldOperand(object, HeapNumber::kValueOffset));
      __ movdbl(xmm1, FieldOperand(probe, HeapNumber::kValueOffset));
      __ ucomisd(xmm0, xmm1);
    } else {
      __ fld_d(FieldOperand(object, HeapNumber::kValueOffset));
      __ fld_d(FieldOperand(probe, HeapNumber::kValueOffset));
      __ FCmp();
    }
    __ j(parity_even, not_found);  // Bail out if NaN is involved.
    __ j(not_equal, not_found);  // The cache did not contain this value.
    __ jmp(&load_result_from_cache, Label::kNear);
  }

  __ bind(&smi_hash_calculated);
  // Object is smi and hash is now in scratch. Calculate cache index.
  __ and_(scratch, mask);
  Register index = scratch;
  // Check if the entry is the smi we are looking for.
  __ cmp(object,
         FieldOperand(number_string_cache,
                      index,
                      times_twice_pointer_size,
                      FixedArray::kHeaderSize));
  __ j(not_equal, not_found);

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ mov(result,
         FieldOperand(number_string_cache,
                      index,
                      times_twice_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->number_to_string_native(), 1);
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ mov(ebx, Operand(esp, kPointerSize));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, ebx, eax, ecx, edx, false, &runtime);
  __ ret(1 * kPointerSize);

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(Runtime::kNumberToStringSkipCache, 1, 1);
}


static int NegativeComparisonResult(Condition cc) {
  ASSERT(cc != equal);
  ASSERT((cc == less) || (cc == less_equal)
      || (cc == greater) || (cc == greater_equal));
  return (cc == greater || cc == greater_equal) ? LESS : GREATER;
}


static void CheckInputType(MacroAssembler* masm,
                           Register input,
                           CompareIC::State expected,
                           Label* fail) {
  Label ok;
  if (expected == CompareIC::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareIC::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ cmp(FieldOperand(input, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
    __ j(not_equal, fail);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


static void BranchIfNotInternalizedString(MacroAssembler* masm,
                                          Label* label,
                                          Register object,
                                          Register scratch) {
  __ JumpIfSmi(object, label);
  __ mov(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, kIsInternalizedMask | kIsNotStringMask);
  __ cmp(scratch, kInternalizedTag | kStringTag);
  __ j(not_equal, label);
}


void ICCompareStub::GenerateGeneric(MacroAssembler* masm) {
  Label check_unequal_objects;
  Condition cc = GetCondition();

  Label miss;
  CheckInputType(masm, edx, left_, &miss);
  CheckInputType(masm, eax, right_, &miss);

  // Compare two smis.
  Label non_smi, smi_done;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &non_smi, Label::kNear);
  __ sub(edx, eax);  // Return on the result of the subtraction.
  __ j(no_overflow, &smi_done, Label::kNear);
  __ not_(edx);  // Correct sign in case of overflow. edx is never 0 here.
  __ bind(&smi_done);
  __ mov(eax, edx);
  __ ret(0);
  __ bind(&non_smi);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Identical objects can be compared fast, but there are some tricky cases
  // for NaN and undefined.
  Label generic_heap_number_comparison;
  {
    Label not_identical;
    __ cmp(eax, edx);
    __ j(not_equal, &not_identical);

    if (cc != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      Label check_for_nan;
      __ cmp(edx, masm->isolate()->factory()->undefined_value());
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Set(eax, Immediate(Smi::FromInt(NegativeComparisonResult(cc))));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Compare heap numbers in a general way,
    // to hanlde NaNs correctly.
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
    __ j(equal, &generic_heap_number_comparison, Label::kNear);
    if (cc != equal) {
      // Call runtime on identical JSObjects.  Otherwise return equal.
      __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
      __ j(above_equal, &not_identical);
    }
    __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
    __ ret(0);


    __ bind(&not_identical);
  }

  // Strict equality can quickly decide whether objects are equal.
  // Non-strict object equality is slower, so it is handled later in the stub.
  if (cc == equal && strict()) {
    Label slow;  // Fallthrough label.
    Label not_smis;
    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    // If either is a Smi (we know that not both are), then they can only
    // be equal if the other is a HeapNumber. If so, use the slow case.
    STATIC_ASSERT(kSmiTag == 0);
    ASSERT_EQ(0, Smi::FromInt(0));
    __ mov(ecx, Immediate(kSmiTagMask));
    __ and_(ecx, eax);
    __ test(ecx, edx);
    __ j(not_zero, &not_smis, Label::kNear);
    // One operand is a smi.

    // Check whether the non-smi is a heap number.
    STATIC_ASSERT(kSmiTagMask == 1);
    // ecx still holds eax & kSmiTag, which is either zero or one.
    __ sub(ecx, Immediate(0x01));
    __ mov(ebx, edx);
    __ xor_(ebx, eax);
    __ and_(ebx, ecx);  // ebx holds either 0 or eax ^ edx.
    __ xor_(ebx, eax);
    // if eax was smi, ebx is now edx, else eax.

    // Check if the non-smi operand is a heap number.
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
    // If heap number, handle it in the slow case.
    __ j(equal, &slow, Label::kNear);
    // Return non-equal (ebx is not zero)
    __ mov(eax, ebx);
    __ ret(0);

    __ bind(&not_smis);
    // If either operand is a JSObject or an oddball value, then they are not
    // equal since their pointers are different
    // There is no test for undetectability in strict equality.

    // Get the type of the first operand.
    // If the first object is a JS object, we have done pointer comparison.
    Label first_non_object;
    STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
    __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(below, &first_non_object, Label::kNear);

    // Return non-zero (eax is not zero)
    Label return_not_equal;
    STATIC_ASSERT(kHeapObjectTag != 0);
    __ bind(&return_not_equal);
    __ ret(0);

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    __ CmpObjectType(edx, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(above_equal, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    // Fall through to the general case.
    __ bind(&slow);
  }

  // Generate the number comparison code.
  Label non_number_comparison;
  Label unordered;
  __ bind(&generic_heap_number_comparison);
  if (CpuFeatures::IsSupported(SSE2)) {
    CpuFeatureScope use_sse2(masm, SSE2);
    CpuFeatureScope use_cmov(masm, CMOV);

    FloatingPointHelper::LoadSSE2Operands(masm, &non_number_comparison);
    __ ucomisd(xmm0, xmm1);

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered, Label::kNear);
    // Return a result of -1, 0, or 1, based on EFLAGS.
    __ mov(eax, 0);  // equal
    __ mov(ecx, Immediate(Smi::FromInt(1)));
    __ cmov(above, eax, ecx);
    __ mov(ecx, Immediate(Smi::FromInt(-1)));
    __ cmov(below, eax, ecx);
    __ ret(0);
  } else {
    FloatingPointHelper::CheckFloatOperands(
        masm, &non_number_comparison, ebx);
    FloatingPointHelper::LoadFloatOperand(masm, eax);
    FloatingPointHelper::LoadFloatOperand(masm, edx);
    __ FCmp();

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered, Label::kNear);

    Label below_label, above_label;
    // Return a result of -1, 0, or 1, based on EFLAGS.
    __ j(below, &below_label, Label::kNear);
    __ j(above, &above_label, Label::kNear);

    __ Set(eax, Immediate(0));
    __ ret(0);

    __ bind(&below_label);
    __ mov(eax, Immediate(Smi::FromInt(-1)));
    __ ret(0);

    __ bind(&above_label);
    __ mov(eax, Immediate(Smi::FromInt(1)));
    __ ret(0);
  }

  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  ASSERT(cc != not_equal);
  if (cc == less || cc == less_equal) {
    __ mov(eax, Immediate(Smi::FromInt(1)));
  } else {
    __ mov(eax, Immediate(Smi::FromInt(-1)));
  }
  __ ret(0);

  // The number comparison code did not provide a valid result.
  __ bind(&non_number_comparison);

  // Fast negative check for internalized-to-internalized equality.
  Label check_for_strings;
  if (cc == equal) {
    BranchIfNotInternalizedString(masm, &check_for_strings, eax, ecx);
    BranchIfNotInternalizedString(masm, &check_for_strings, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are internalized they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx,
                                         &check_unequal_objects);

  // Inline comparison of ASCII strings.
  if (cc == equal) {
    StringCompareStub::GenerateFlatAsciiStringEquals(masm,
                                                     edx,
                                                     eax,
                                                     ecx,
                                                     ebx);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                       edx,
                                                       eax,
                                                       ecx,
                                                       ebx,
                                                       edi);
  }
#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&check_unequal_objects);
  if (cc == equal && !strict()) {
    // Non-strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label not_both_objects;
    Label return_unequal;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(ecx, Operand(eax, edx, times_1, 0));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &not_both_objects, Label::kNear);
    __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
    __ j(below, &not_both_objects, Label::kNear);
    __ CmpObjectType(edx, FIRST_SPEC_OBJECT_TYPE, ebx);
    __ j(below, &not_both_objects, Label::kNear);
    // We do not bail out after this point.  Both are JSObjects, and
    // they are equal if and only if both are undetectable.
    // The and of the undetectable flags is 1 if and only if they are equal.
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(zero, &return_unequal, Label::kNear);
    __ test_b(FieldOperand(ebx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(zero, &return_unequal, Label::kNear);
    // The objects are both undetectable, so they both compare as the value
    // undefined, and are equal.
    __ Set(eax, Immediate(EQUAL));
    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in eax,
    // or return equal if we fell through to here.
    __ ret(0);  // rax, rdx were pushed
    __ bind(&not_both_objects);
  }

  // Push arguments below the return address.
  __ pop(ecx);
  __ push(edx);
  __ push(eax);

  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript builtin;
  if (cc == equal) {
    builtin = strict() ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    __ push(Immediate(Smi::FromInt(NegativeComparisonResult(cc))));
  }

  // Restore return address on the stack.
  __ push(ecx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);

  __ bind(&miss);
  GenerateMiss(masm);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kStackGuard, 0, 1);
}


void InterruptStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kInterrupt, 0, 1);
}


static void GenerateRecordCallTargetNoArray(MacroAssembler* masm) {
  // Cache the called function in a global property cell.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // ebx : cache cell for call target
  // edi : the function to call
  ASSERT(!FLAG_optimize_constructed_arrays);
  Isolate* isolate = masm->isolate();
  Label initialize, done;

  // Load the cache state into ecx.
  __ mov(ecx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ cmp(ecx, edi);
  __ j(equal, &done, Label::kNear);
  __ cmp(ecx, Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  __ j(equal, &done, Label::kNear);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ cmp(ecx, Immediate(TypeFeedbackCells::UninitializedSentinel(isolate)));
  __ j(equal, &initialize, Label::kNear);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset),
         Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  __ jmp(&done, Label::kNear);

  // An uninitialized cache is patched with the function.
  __ bind(&initialize);
  __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset), edi);
  // No need for a write barrier here - cells are rescanned.

  __ bind(&done);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a global property cell.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // ebx : cache cell for call target
  // edi : the function to call
  ASSERT(FLAG_optimize_constructed_arrays);
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function;

  // Load the cache state into ecx.
  __ mov(ecx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ cmp(ecx, edi);
  __ j(equal, &done);
  __ cmp(ecx, Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  __ j(equal, &done);

  // Special handling of the Array() function, which caches not only the
  // monomorphic Array function but the initial ElementsKind with special
  // sentinels
  Handle<Object> terminal_kind_sentinel =
      TypeFeedbackCells::MonomorphicArraySentinel(isolate,
                                                  LAST_FAST_ELEMENTS_KIND);
  __ JumpIfNotSmi(ecx, &miss);
  __ cmp(ecx, Immediate(terminal_kind_sentinel));
  __ j(above, &miss);
  // Load the global or builtins object from the current context
  __ LoadGlobalContext(ecx);
  // Make sure the function is the Array() function
  __ cmp(edi, Operand(ecx,
                      Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
  __ j(not_equal, &megamorphic);
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ cmp(ecx, Immediate(TypeFeedbackCells::UninitializedSentinel(isolate)));
  __ j(equal, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset),
         Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  __ jmp(&done, Label::kNear);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ bind(&initialize);
  __ LoadGlobalContext(ecx);
  // Make sure the function is the Array() function
  __ cmp(edi, Operand(ecx,
                      Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
  __ j(not_equal, &not_array_function);

  // The target function is the Array constructor, install a sentinel value in
  // the constructor's type info cell that will track the initial ElementsKind
  // that should be used for the array when its constructed.
  Handle<Object> initial_kind_sentinel =
      TypeFeedbackCells::MonomorphicArraySentinel(isolate,
          GetInitialFastElementsKind());
  __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset),
         Immediate(initial_kind_sentinel));
  __ jmp(&done);

  __ bind(&not_array_function);
  __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset), edi);
  // No need for a write barrier here - cells are rescanned.

  __ bind(&done);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  // ebx : cache cell for call target
  // edi : the function to call
  Isolate* isolate = masm->isolate();
  Label slow, non_function;

  // The receiver might implicitly be the global object. This is
  // indicated by passing the hole as the receiver to the call
  // function stub.
  if (ReceiverMightBeImplicit()) {
    Label receiver_ok;
    // Get the receiver from the stack.
    // +1 ~ return address
    __ mov(eax, Operand(esp, (argc_ + 1) * kPointerSize));
    // Call as function is indicated with the hole.
    __ cmp(eax, isolate->factory()->the_hole_value());
    __ j(not_equal, &receiver_ok, Label::kNear);
    // Patch the receiver on the stack with the global receiver object.
    __ mov(ecx, GlobalObjectOperand());
    __ mov(ecx, FieldOperand(ecx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc_ + 1) * kPointerSize), ecx);
    __ bind(&receiver_ok);
  }

  // Check that the function really is a JavaScript function.
  __ JumpIfSmi(edi, &non_function);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &slow);

  if (RecordCallTarget()) {
    if (FLAG_optimize_constructed_arrays) {
      GenerateRecordCallTarget(masm);
    } else {
      GenerateRecordCallTargetNoArray(masm);
    }
  }

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);

  if (ReceiverMightBeImplicit()) {
    Label call_as_function;
    __ cmp(eax, isolate->factory()->the_hole_value());
    __ j(equal, &call_as_function);
    __ InvokeFunction(edi,
                      actual,
                      JUMP_FUNCTION,
                      NullCallWrapper(),
                      CALL_AS_METHOD);
    __ bind(&call_as_function);
  }
  __ InvokeFunction(edi,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    CALL_AS_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  if (RecordCallTarget()) {
    // If there is a call target cache, mark it megamorphic in the
    // non-function case.  MegamorphicSentinel is an immortal immovable
    // object (undefined) so no write barrier is needed.
    __ mov(FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset),
           Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  }
  // Check for function proxy.
  __ CmpInstanceType(ecx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ pop(ecx);
  __ push(edi);  // put proxy as additional argument under return address
  __ push(ecx);
  __ Set(eax, Immediate(argc_ + 1));
  __ Set(ebx, Immediate(0));
  __ SetCallKind(ecx, CALL_AS_FUNCTION);
  __ GetBuiltinEntry(edx, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor = isolate->builtins()->ArgumentsAdaptorTrampoline();
    __ jmp(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ bind(&non_function);
  __ mov(Operand(esp, (argc_ + 1) * kPointerSize), edi);
  __ Set(eax, Immediate(argc_));
  __ Set(ebx, Immediate(0));
  __ SetCallKind(ecx, CALL_AS_METHOD);
  __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor = isolate->builtins()->ArgumentsAdaptorTrampoline();
  __ jmp(adaptor, RelocInfo::CODE_TARGET);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // eax : number of arguments
  // ebx : cache cell for call target
  // edi : constructor function
  Label slow, non_function_call;

  // Check that function is not a smi.
  __ JumpIfSmi(edi, &non_function_call);
  // Check that function is a JSFunction.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &slow);

  if (RecordCallTarget()) {
    if (FLAG_optimize_constructed_arrays) {
      GenerateRecordCallTarget(masm);
    } else {
      GenerateRecordCallTargetNoArray(masm);
    }
  }

  // Jump to the function-specific construct stub.
  Register jmp_reg = FLAG_optimize_constructed_arrays ? ecx : ebx;
  __ mov(jmp_reg, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(jmp_reg, FieldOperand(jmp_reg,
                               SharedFunctionInfo::kConstructStubOffset));
  __ lea(jmp_reg, FieldOperand(jmp_reg, Code::kHeaderSize));
  __ jmp(jmp_reg);

  // edi: called object
  // eax: number of arguments
  // ecx: object map
  Label do_call;
  __ bind(&slow);
  __ CmpInstanceType(ecx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function_call);
  __ GetBuiltinEntry(edx, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ jmp(&do_call);

  __ bind(&non_function_call);
  __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ bind(&do_call);
  // Set expected number of arguments to zero (not changing eax).
  __ Set(ebx, Immediate(0));
  Handle<Code> arguments_adaptor =
      masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
  __ SetCallKind(ecx, CALL_AS_METHOD);
  __ jmp(arguments_adaptor, RelocInfo::CODE_TARGET);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


bool CEntryStub::IsPregenerated() {
  return (!save_doubles_ || ISOLATE->fp_stubs_generated()) &&
          result_size_ == 1;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  RecordWriteStub::GenerateFixedRegStubsAheadOfTime(isolate);
  if (FLAG_optimize_constructed_arrays) {
    ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  }
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
  if (CpuFeatures::IsSupported(SSE2)) {
    CEntryStub save_doubles(1, kSaveFPRegs);
    // Stubs might already be in the snapshot, detect that and don't regenerate,
    // which would lead to code stub initialization state being messed up.
    Code* save_doubles_code;
    if (!save_doubles.FindCodeInCache(&save_doubles_code, isolate)) {
      save_doubles_code = *(save_doubles.GetCode(isolate));
    }
    save_doubles_code->set_is_pregenerated(true);
    isolate->set_fp_stubs_generated(true);
  }
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(1, kDontSaveFPRegs);
  Handle<Code> code = stub.GetCode(isolate);
  code->set_is_pregenerated(true);
}


static void JumpIfOOM(MacroAssembler* masm,
                      Register value,
                      Register scratch,
                      Label* oom_label) {
  __ mov(scratch, value);
  STATIC_ASSERT(Failure::OUT_OF_MEMORY_EXCEPTION == 3);
  STATIC_ASSERT(kFailureTag == 3);
  __ and_(scratch, 0xf);
  __ cmp(scratch, 0xf);
  __ j(equal, oom_label);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope) {
  // eax: result parameter for PerformGC, if any
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result_size_ is 2.

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }

  if (do_gc) {
    // Pass failure code returned from last attempt as first argument to
    // PerformGC. No need to use PrepareCallCFunction/CallCFunction here as the
    // stack alignment is known to be correct. This function takes one argument
    // which is passed on the stack, and we know that the stack has been
    // prepared to pass at least one argument.
    __ mov(Operand(esp, 0 * kPointerSize), eax);  // Result.
    __ call(FUNCTION_ADDR(Runtime::PerformGC), RelocInfo::RUNTIME_ENTRY);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth(masm->isolate());
  if (always_allocate_scope) {
    __ inc(Operand::StaticVariable(scope_depth));
  }

  // Call C function.
  __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
  __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
  __ mov(Operand(esp, 2 * kPointerSize),
         Immediate(ExternalReference::isolate_address(masm->isolate())));
  __ call(ebx);
  // Result is in eax or edx:eax - do not destroy these registers!

  if (always_allocate_scope) {
    __ dec(Operand::StaticVariable(scope_depth));
  }

  // Runtime functions should not return 'the hole'.  Allowing it to escape may
  // lead to crashes in the IC code later.
  if (FLAG_debug_code) {
    Label okay;
    __ cmp(eax, masm->isolate()->factory()->the_hole_value());
    __ j(not_equal, &okay, Label::kNear);
    // TODO(wingo): Currently SuspendJSGeneratorObject returns the hole.  Change
    // to return another sentinel like a harmony symbol.
    __ cmp(ebx, Immediate(ExternalReference(
        Runtime::kSuspendJSGeneratorObject, masm->isolate())));
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
  }

  // Check for failure result.
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ lea(ecx, Operand(eax, 1));
  // Lower 2 bits of ecx are 0 iff eax has failure tag.
  __ test(ecx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned);

  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, masm->isolate());

  // Check that there is no pending exception, otherwise we
  // should have returned some failure value.
  if (FLAG_debug_code) {
    __ push(edx);
    __ mov(edx, Immediate(masm->isolate()->factory()->the_hole_value()));
    Label okay;
    __ cmp(edx, Operand::StaticVariable(pending_exception_address));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles_ == kSaveFPRegs);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ test(eax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, Label::kNear);

  // Special handling of out of memory exceptions.
  JumpIfOOM(masm, eax, ecx, throw_out_of_memory_exception);

  // Retrieve the pending exception.
  __ mov(eax, Operand::StaticVariable(pending_exception_address));

  // See if we just retrieved an OOM exception.
  JumpIfOOM(masm, eax, ecx, throw_out_of_memory_exception);

  // Clear the pending exception.
  __ mov(edx, Immediate(masm->isolate()->factory()->the_hole_value()));
  __ mov(Operand::StaticVariable(pending_exception_address), edx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(eax, masm->isolate()->factory()->termination_exception());
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects instead
  // of a proper result. The builtin entry handles this by performing
  // a garbage collection and retrying the builtin (twice).

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(save_doubles_ == kSaveFPRegs);

  // eax: result parameter for PerformGC, if any (setup below)
  // ebx: pointer to builtin function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver (C callee-saved)
  // esi: argv pointer (C callee-saved)

  Label throw_normal_exception;
  Label throw_termination_exception;
  Label throw_out_of_memory_exception;

  // Call into the runtime system.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               false,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ mov(eax, Immediate(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  // Set external caught exception to false.
  Isolate* isolate = masm->isolate();
  ExternalReference external_caught(Isolate::kExternalCaughtExceptionAddress,
                                    isolate);
  __ mov(Operand::StaticVariable(external_caught), Immediate(false));

  // Set pending exception and eax to out of memory exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate);
  Label already_have_failure;
  JumpIfOOM(masm, eax, ecx, &already_have_failure);
  __ mov(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException(0x1)));
  __ bind(&already_have_failure);
  __ mov(Operand::StaticVariable(pending_exception), eax);
  // Fall through to the next label.

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(eax);

  __ bind(&throw_normal_exception);
  __ Throw(eax);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  // Set up frame.
  __ push(ebp);
  __ mov(ebp, esp);

  // Push marker in two places.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ push(Immediate(Smi::FromInt(marker)));  // context slot
  __ push(Immediate(Smi::FromInt(marker)));  // function slot
  // Save callee-saved registers (C calling conventions).
  __ push(edi);
  __ push(esi);
  __ push(ebx);

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, masm->isolate());
  __ push(Operand::StaticVariable(c_entry_fp));

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress,
                                masm->isolate());
  __ cmp(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ j(not_equal, &not_outermost_js, Label::kNear);
  __ mov(Operand::StaticVariable(js_entry_sp), ebp);
  __ push(Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ jmp(&invoke, Label::kNear);
  __ bind(&not_outermost_js);
  __ push(Immediate(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      masm->isolate());
  __ mov(Operand::StaticVariable(pending_exception), eax);
  __ mov(eax, reinterpret_cast<int32_t>(Failure::Exception()));
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.  There's only one
  // handler block in this code object, so its index is 0.
  __ bind(&invoke);
  __ PushTryHandler(StackHandler::JS_ENTRY, 0);

  // Clear any pending exceptions.
  __ mov(edx, Immediate(masm->isolate()->factory()->the_hole_value()));
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. Notice that we cannot store a
  // reference to the trampoline code directly in this stub, because the
  // builtin stubs may not have been generated yet.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      masm->isolate());
    __ mov(edx, Immediate(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline,
                            masm->isolate());
    __ mov(edx, Immediate(entry));
  }
  __ mov(edx, Operand(edx, 0));  // deref address
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ call(edx);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(ebx);
  __ cmp(ebx, Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  __ pop(Operand::StaticVariable(ExternalReference(
      Isolate::kCEntryFPAddress,
      masm->isolate())));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(esp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(ebp);
  __ ret(0);
}


// Generate stub code for instanceof.
// This code can patch a call site inlined cache of the instance of check,
// which looks like this.
//
//   81 ff XX XX XX XX   cmp    edi, <the hole, patched to a map>
//   75 0a               jne    <some near label>
//   b8 XX XX XX XX      mov    eax, <the hole, patched to either true or false>
//
// If call site patching is requested the stack will have the delta from the
// return address to the cmp instruction just below the return address. This
// also means that call site patching can only take place with arguments in
// registers. TOS looks like this when call site patching is requested
//
//   esp[0] : return address
//   esp[4] : delta from return address to cmp instruction
//
void InstanceofStub::Generate(MacroAssembler* masm) {
  // Call site inlining and patching implies arguments in registers.
  ASSERT(HasArgsInRegisters() || !HasCallSiteInlineCheck());

  // Fixed register usage throughout the stub.
  Register object = eax;  // Object (lhs).
  Register map = ebx;  // Map of the object.
  Register function = edx;  // Function (rhs).
  Register prototype = edi;  // Prototype of the function.
  Register scratch = ecx;

  // Constants describing the call site code to patch.
  static const int kDeltaToCmpImmediate = 2;
  static const int kDeltaToMov = 8;
  static const int kDeltaToMovImmediate = 9;
  static const int8_t kCmpEdiOperandByte1 = BitCast<int8_t, uint8_t>(0x3b);
  static const int8_t kCmpEdiOperandByte2 = BitCast<int8_t, uint8_t>(0x3d);
  static const int8_t kMovEaxImmediateByte = BitCast<int8_t, uint8_t>(0xb8);

  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(masm->isolate());

  ASSERT_EQ(object.code(), InstanceofStub::left().code());
  ASSERT_EQ(function.code(), InstanceofStub::right().code());

  // Get the object and function - they are always both needed.
  Label slow, not_js_object;
  if (!HasArgsInRegisters()) {
    __ mov(object, Operand(esp, 2 * kPointerSize));
    __ mov(function, Operand(esp, 1 * kPointerSize));
  }

  // Check that the left hand is a JS object.
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    // Look up the function and the map in the instanceof cache.
    Label miss;
    __ mov(scratch, Immediate(Heap::kInstanceofCacheFunctionRootIndex));
    __ cmp(function, Operand::StaticArray(scratch,
                                          times_pointer_size,
                                          roots_array_start));
    __ j(not_equal, &miss, Label::kNear);
    __ mov(scratch, Immediate(Heap::kInstanceofCacheMapRootIndex));
    __ cmp(map, Operand::StaticArray(
        scratch, times_pointer_size, roots_array_start));
    __ j(not_equal, &miss, Label::kNear);
    __ mov(scratch, Immediate(Heap::kInstanceofCacheAnswerRootIndex));
    __ mov(eax, Operand::StaticArray(
        scratch, times_pointer_size, roots_array_start));
    __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);
    __ bind(&miss);
  }

  // Get the prototype of the function.
  __ TryGetFunctionPrototype(function, prototype, scratch, &slow, true);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(prototype, &slow);
  __ IsObjectJSObjectType(prototype, scratch, scratch, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (!HasCallSiteInlineCheck()) {
  __ mov(scratch, Immediate(Heap::kInstanceofCacheMapRootIndex));
  __ mov(Operand::StaticArray(scratch, times_pointer_size, roots_array_start),
         map);
  __ mov(scratch, Immediate(Heap::kInstanceofCacheFunctionRootIndex));
  __ mov(Operand::StaticArray(scratch, times_pointer_size, roots_array_start),
         function);
  } else {
    // The constants for the code patching are based on no push instructions
    // at the call site.
    ASSERT(HasArgsInRegisters());
    // Get return address and delta to inlined map check.
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, 0), kCmpEdiOperandByte1);
      __ Assert(equal, "InstanceofStub unexpected call site cache (cmp 1)");
      __ cmpb(Operand(scratch, 1), kCmpEdiOperandByte2);
      __ Assert(equal, "InstanceofStub unexpected call site cache (cmp 2)");
    }
    __ mov(scratch, Operand(scratch, kDeltaToCmpImmediate));
    __ mov(Operand(scratch, 0), map);
  }

  // Loop through the prototype chain of the object looking for the function
  // prototype.
  __ mov(scratch, FieldOperand(map, Map::kPrototypeOffset));
  Label loop, is_instance, is_not_instance;
  __ bind(&loop);
  __ cmp(scratch, prototype);
  __ j(equal, &is_instance, Label::kNear);
  Factory* factory = masm->isolate()->factory();
  __ cmp(scratch, Immediate(factory->null_value()));
  __ j(equal, &is_not_instance, Label::kNear);
  __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
  __ mov(scratch, FieldOperand(scratch, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ Set(eax, Immediate(0));
    __ mov(scratch, Immediate(Heap::kInstanceofCacheAnswerRootIndex));
    __ mov(Operand::StaticArray(scratch,
                                times_pointer_size, roots_array_start), eax);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, factory->true_value());
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, kDeltaToMov), kMovEaxImmediateByte);
      __ Assert(equal, "InstanceofStub unexpected call site cache (mov)");
    }
    __ mov(Operand(scratch, kDeltaToMovImmediate), eax);
    if (!ReturnTrueFalseObject()) {
      __ Set(eax, Immediate(0));
    }
  }
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ Set(eax, Immediate(Smi::FromInt(1)));
    __ mov(scratch, Immediate(Heap::kInstanceofCacheAnswerRootIndex));
    __ mov(Operand::StaticArray(
        scratch, times_pointer_size, roots_array_start), eax);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, factory->false_value());
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, kDeltaToMov), kMovEaxImmediateByte);
      __ Assert(equal, "InstanceofStub unexpected call site cache (mov)");
    }
    __ mov(Operand(scratch, kDeltaToMovImmediate), eax);
    if (!ReturnTrueFalseObject()) {
      __ Set(eax, Immediate(Smi::FromInt(1)));
    }
  }
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  Label object_not_null, object_not_null_or_smi;
  __ bind(&not_js_object);
  // Before null, smi and string value checks, check that the rhs is a function
  // as for a non-function rhs an exception needs to be thrown.
  __ JumpIfSmi(function, &slow, Label::kNear);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, scratch);
  __ j(not_equal, &slow, Label::kNear);

  // Null is not instance of anything.
  __ cmp(object, factory->null_value());
  __ j(not_equal, &object_not_null, Label::kNear);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&object_not_null);
  // Smi values is not instance of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi, Label::kNear);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&object_not_null_or_smi);
  // String values is not instance of anything.
  Condition is_string = masm->IsObjectStringType(object, scratch, scratch);
  __ j(NegateCondition(is_string), &slow, Label::kNear);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  if (!ReturnTrueFalseObject()) {
    // Tail call the builtin which returns 0 or 1.
    if (HasArgsInRegisters()) {
      // Push arguments below return address.
      __ pop(scratch);
      __ push(object);
      __ push(function);
      __ push(scratch);
    }
    __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
  } else {
    // Call the builtin and convert 0/1 to true/false.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(object);
      __ push(function);
      __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
    }
    Label true_value, done;
    __ test(eax, eax);
    __ j(zero, &true_value, Label::kNear);
    __ mov(eax, factory->false_value());
    __ jmp(&done, Label::kNear);
    __ bind(&true_value);
    __ mov(eax, factory->true_value());
    __ bind(&done);
    __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);
  }
}


Register InstanceofStub::left() { return eax; }


Register InstanceofStub::right() { return edx; }


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ test(result_, Immediate(kIsNotStringMask));
  __ j(not_zero, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ cmp(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiUntag(index_);

  Factory* factory = masm->isolate()->factory();
  StringCharLoadGenerator::Generate(
      masm, factory, object_, index_, result_, &call_runtime_);

  __ SmiTag(result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharCodeAt slow case");

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              masm->isolate()->factory()->heap_number_map(),
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  if (!index_.is(eax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ mov(index_, eax);
  }
  __ pop(object_);
  // Reload the instance type.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ SmiTag(index_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  ASSERT(IsPowerOf2(String::kMaxOneByteCharCode + 1));
  __ test(code_,
          Immediate(kSmiTagMask |
                    ((~String::kMaxOneByteCharCode) << kSmiTagSize)));
  __ j(not_zero, &slow_case_);

  Factory* factory = masm->isolate()->factory();
  __ Set(result_, Immediate(factory->single_character_string_cache()));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiShiftSize == 0);
  // At this point code register contains smi tagged ASCII char code.
  __ mov(result_, FieldOperand(result_,
                               code_, times_half_pointer_size,
                               FixedArray::kHeaderSize));
  __ cmp(result_, factory->undefined_value());
  __ j(equal, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharFromCode slow case");
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label call_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  // Load the two arguments.
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (flags_ == NO_STRING_ADD_FLAGS) {
    __ JumpIfSmi(eax, &call_runtime);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &call_runtime);

    // First argument is a a string, test second.
    __ JumpIfSmi(edx, &call_runtime);
    __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &call_runtime);
  } else {
    // Here at least one of the arguments is definitely a string.
    // We convert the one that is not known to be a string.
    if ((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) != 0);
      GenerateConvertArgument(masm, 2 * kPointerSize, eax, ebx, ecx, edi,
                              &call_builtin);
      builtin_id = Builtins::STRING_ADD_RIGHT;
    } else if ((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) != 0);
      GenerateConvertArgument(masm, 1 * kPointerSize, edx, ebx, ecx, edi,
                              &call_builtin);
      builtin_id = Builtins::STRING_ADD_LEFT;
    }
  }

  // Both arguments are strings.
  // eax: first string
  // edx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ mov(ecx, FieldOperand(edx, String::kLengthOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ test(ecx, ecx);
  __ j(not_zero, &second_not_zero_length, Label::kNear);
  // Second string is empty, result is first string which is already in eax.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ test(ebx, ebx);
  __ j(not_zero, &both_not_zero_length, Label::kNear);
  // First string is empty, result is second string which is in edx.
  __ mov(eax, edx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // eax: first string
  // ebx: length of first string as a smi
  // ecx: length of second string as a smi
  // edx: second string
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);
  __ add(ebx, ecx);
  STATIC_ASSERT(Smi::kMaxValue == String::kMaxLength);
  // Handle exceptionally long strings in the runtime system.
  __ j(overflow, &call_runtime);
  // Use the string table when adding two one character strings, as it
  // helps later optimizations to return an internalized string here.
  __ cmp(ebx, Immediate(Smi::FromInt(2)));
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(eax, edx, ebx, ecx, &call_runtime);

  // Get the two characters forming the new string.
  __ movzx_b(ebx, FieldOperand(eax, SeqOneByteString::kHeaderSize));
  __ movzx_b(ecx, FieldOperand(edx, SeqOneByteString::kHeaderSize));

  // Try to lookup two character string in string table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_two_character_string_no_reload;
  StringHelper::GenerateTwoCharacterStringTableProbe(
      masm, ebx, ecx, eax, edx, edi,
      &make_two_character_string_no_reload, &make_two_character_string);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Allocate a two character string.
  __ bind(&make_two_character_string);
  // Reload the arguments.
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.
  // Get the two characters forming the new string.
  __ movzx_b(ebx, FieldOperand(eax, SeqOneByteString::kHeaderSize));
  __ movzx_b(ecx, FieldOperand(edx, SeqOneByteString::kHeaderSize));
  __ bind(&make_two_character_string_no_reload);
  __ IncrementCounter(counters->string_add_make_two_char(), 1);
  __ AllocateAsciiString(eax, 2, edi, edx, &call_runtime);
  // Pack both characters in ebx.
  __ shl(ecx, kBitsPerByte);
  __ or_(ebx, ecx);
  // Set the characters in the new string.
  __ mov_w(FieldOperand(eax, SeqOneByteString::kHeaderSize), ebx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ cmp(ebx, Immediate(Smi::FromInt(ConsString::kMinLength)));
  __ j(below, &string_add_flat_result);

  // If result is not supposed to be flat allocate a cons string object. If both
  // strings are ASCII the result is an ASCII cons string.
  Label non_ascii, allocated, ascii_data;
  __ mov(edi, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(edi, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ and_(ecx, edi);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ test(ecx, Immediate(kStringEncodingMask));
  __ j(zero, &non_ascii);
  __ bind(&ascii_data);
  // Allocate an ASCII cons string.
  __ AllocateAsciiConsString(ecx, edi, no_reg, &call_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ AssertSmi(ebx);
  __ mov(FieldOperand(ecx, ConsString::kLengthOffset), ebx);
  __ mov(FieldOperand(ecx, ConsString::kHashFieldOffset),
         Immediate(String::kEmptyHashField));

  Label skip_write_barrier, after_writing;
  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(masm->isolate());
  __ test(Operand::StaticVariable(high_promotion_mode), Immediate(1));
  __ j(zero, &skip_write_barrier);

  __ mov(FieldOperand(ecx, ConsString::kFirstOffset), eax);
  __ RecordWriteField(ecx,
                     ConsString::kFirstOffset,
                     eax,
                     ebx,
                     kDontSaveFPRegs);
  __ mov(FieldOperand(ecx, ConsString::kSecondOffset), edx);
  __ RecordWriteField(ecx,
                     ConsString::kSecondOffset,
                     edx,
                     ebx,
                     kDontSaveFPRegs);
  __ jmp(&after_writing);

  __ bind(&skip_write_barrier);
  __ mov(FieldOperand(ecx, ConsString::kFirstOffset), eax);
  __ mov(FieldOperand(ecx, ConsString::kSecondOffset), edx);

  __ bind(&after_writing);

  __ mov(eax, ecx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only one byte characters.
  // ecx: first instance type AND second instance type.
  // edi: second instance type.
  __ test(ecx, Immediate(kOneByteDataHintMask));
  __ j(not_zero, &ascii_data);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ xor_(edi, ecx);
  STATIC_ASSERT(kOneByteStringTag != 0 && kOneByteDataHintTag != 0);
  __ and_(edi, kOneByteStringTag | kOneByteDataHintTag);
  __ cmp(edi, kOneByteStringTag | kOneByteDataHintTag);
  __ j(equal, &ascii_data);
  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(ecx, edi, no_reg, &call_runtime);
  __ jmp(&allocated);

  // We cannot encounter sliced strings or cons strings here since:
  STATIC_ASSERT(SlicedString::kMinLength >= ConsString::kMinLength);
  // Handle creating a flat result from either external or sequential strings.
  // Locate the first characters' locations.
  // eax: first string
  // ebx: length of resulting flat string as a smi
  // edx: second string
  Label first_prepared, second_prepared;
  Label first_is_sequential, second_is_sequential;
  __ bind(&string_add_flat_result);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  // ecx: instance type of first string
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test_b(ecx, kStringRepresentationMask);
  __ j(zero, &first_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ test_b(ecx, kShortExternalStringMask);
  __ j(not_zero, &call_runtime);
  __ mov(eax, FieldOperand(eax, ExternalString::kResourceDataOffset));
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ jmp(&first_prepared, Label::kNear);
  __ bind(&first_is_sequential);
  __ add(eax, Immediate(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ bind(&first_prepared);

  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(edi, FieldOperand(edi, Map::kInstanceTypeOffset));
  // Check whether both strings have same encoding.
  // edi: instance type of second string
  __ xor_(ecx, edi);
  __ test_b(ecx, kStringEncodingMask);
  __ j(not_zero, &call_runtime);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test_b(edi, kStringRepresentationMask);
  __ j(zero, &second_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ test_b(edi, kShortExternalStringMask);
  __ j(not_zero, &call_runtime);
  __ mov(edx, FieldOperand(edx, ExternalString::kResourceDataOffset));
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ jmp(&second_prepared, Label::kNear);
  __ bind(&second_is_sequential);
  __ add(edx, Immediate(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ bind(&second_prepared);

  // Push the addresses of both strings' first characters onto the stack.
  __ push(edx);
  __ push(eax);

  Label non_ascii_string_add_flat_result, call_runtime_drop_two;
  // edi: instance type of second string
  // First string and second string have the same encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ test_b(edi, kStringEncodingMask);
  __ j(zero, &non_ascii_string_add_flat_result);

  // Both strings are ASCII strings.
  // ebx: length of resulting flat string as a smi
  __ SmiUntag(ebx);
  __ AllocateAsciiString(eax, ebx, ecx, edx, edi, &call_runtime_drop_two);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(ecx, Immediate(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  // Load first argument's length and first character location.  Account for
  // values currently on the stack when fetching arguments from it.
  __ mov(edx, Operand(esp, 4 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ pop(edx);
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  // Load second argument's length and first character location.  Account for
  // values currently on the stack when fetching arguments from it.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ pop(edx);
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Handle creating a flat two byte result.
  // eax: first string - known to be two byte
  // ebx: length of resulting flat string as a smi
  // edx: second string
  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are two byte strings.
  __ SmiUntag(ebx);
  __ AllocateTwoByteString(eax, ebx, ecx, edx, edi, &call_runtime_drop_two);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(ecx, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load second argument's length and first character location.  Account for
  // values currently on the stack when fetching arguments from it.
  __ mov(edx, Operand(esp, 4 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ pop(edx);
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  // Load second argument's length and first character location.  Account for
  // values currently on the stack when fetching arguments from it.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ pop(edx);
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Recover stack pointer before jumping to runtime.
  __ bind(&call_runtime_drop_two);
  __ Drop(2);
  // Just jump to runtime to add the two strings.
  __ bind(&call_runtime);
  __ TailCallRuntime(Runtime::kStringAdd, 2, 1);

  if (call_builtin.is_linked()) {
    __ bind(&call_builtin);
    __ InvokeBuiltin(builtin_id, JUMP_FUNCTION);
  }
}


void StringAddStub::GenerateConvertArgument(MacroAssembler* masm,
                                            int stack_offset,
                                            Register arg,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3,
                                            Label* slow) {
  // First check if the argument is already a string.
  Label not_string, done;
  __ JumpIfSmi(arg, &not_string);
  __ CmpObjectType(arg, FIRST_NONSTRING_TYPE, scratch1);
  __ j(below, &done);

  // Check the number to string cache.
  Label not_cached;
  __ bind(&not_string);
  // Puts the cached result into scratch1.
  NumberToStringStub::GenerateLookupNumberStringCache(masm,
                                                      arg,
                                                      scratch1,
                                                      scratch2,
                                                      scratch3,
                                                      false,
                                                      &not_cached);
  __ mov(arg, scratch1);
  __ mov(Operand(esp, stack_offset), arg);
  __ jmp(&done);

  // Check if the argument is a safe string wrapper.
  __ bind(&not_cached);
  __ JumpIfSmi(arg, slow);
  __ CmpObjectType(arg, JS_VALUE_TYPE, scratch1);  // map -> scratch1.
  __ j(not_equal, slow);
  __ test_b(FieldOperand(scratch1, Map::kBitField2Offset),
            1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ j(zero, slow);
  __ mov(arg, FieldOperand(arg, JSValue::kValueOffset));
  __ mov(Operand(esp, stack_offset), arg);

  __ bind(&done);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          bool ascii) {
  Label loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ mov_b(scratch, Operand(src, 0));
    __ mov_b(Operand(dest, 0), scratch);
    __ add(src, Immediate(1));
    __ add(dest, Immediate(1));
  } else {
    __ mov_w(scratch, Operand(src, 0));
    __ mov_w(Operand(dest, 0), scratch);
    __ add(src, Immediate(2));
    __ add(dest, Immediate(2));
  }
  __ sub(count, Immediate(1));
  __ j(not_zero, &loop);
}


void StringHelper::GenerateCopyCharactersREP(MacroAssembler* masm,
                                             Register dest,
                                             Register src,
                                             Register count,
                                             Register scratch,
                                             bool ascii) {
  // Copy characters using rep movs of doublewords.
  // The destination is aligned on a 4 byte boundary because we are
  // copying to the beginning of a newly allocated string.
  ASSERT(dest.is(edi));  // rep movs destination
  ASSERT(src.is(esi));  // rep movs source
  ASSERT(count.is(ecx));  // rep movs count
  ASSERT(!scratch.is(dest));
  ASSERT(!scratch.is(src));
  ASSERT(!scratch.is(count));

  // Nothing to do for zero characters.
  Label done;
  __ test(count, count);
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (!ascii) {
    __ shl(count, 1);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ test(count, Immediate(~3));
  __ j(zero, &last_bytes, Label::kNear);

  // Copy from edi to esi using rep movs instruction.
  __ mov(scratch, count);
  __ sar(count, 2);  // Number of doublewords to copy.
  __ cld();
  __ rep_movs();

  // Find number of bytes left.
  __ mov(count, scratch);
  __ and_(count, 3);

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ test(count, count);
  __ j(zero, &done);

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(src, 0));
  __ mov_b(Operand(dest, 0), scratch);
  __ add(src, Immediate(1));
  __ add(dest, Immediate(1));
  __ sub(count, Immediate(1));
  __ j(not_zero, &loop);

  __ bind(&done);
}


void StringHelper::GenerateTwoCharacterStringTableProbe(MacroAssembler* masm,
                                                        Register c1,
                                                        Register c2,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Label* not_probed,
                                                        Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the string table.
  Label not_array_index;
  __ mov(scratch, c1);
  __ sub(scratch, Immediate(static_cast<int>('0')));
  __ cmp(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index, Label::kNear);
  __ mov(scratch, c2);
  __ sub(scratch, Immediate(static_cast<int>('0')));
  __ cmp(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(below_equal, not_probed);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  GenerateHashInit(masm, hash, c1, scratch);
  GenerateHashAddCharacter(masm, hash, c2, scratch);
  GenerateHashGetHash(masm, hash, scratch);

  // Collect the two characters in a register.
  Register chars = c1;
  __ shl(c2, kBitsPerByte);
  __ or_(chars, c2);

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load the string table.
  Register string_table = c2;
  ExternalReference roots_array_start =
      ExternalReference::roots_array_start(masm->isolate());
  __ mov(scratch, Immediate(Heap::kStringTableRootIndex));
  __ mov(string_table,
         Operand::StaticArray(scratch, times_pointer_size, roots_array_start));

  // Calculate capacity mask from the string table capacity.
  Register mask = scratch2;
  __ mov(mask, FieldOperand(string_table, StringTable::kCapacityOffset));
  __ SmiUntag(mask);
  __ sub(mask, Immediate(1));

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string
  // string_table: string table
  // mask:         capacity mask
  // scratch:      -

  // Perform a number of probes in the string table.
  static const int kProbes = 4;
  Label found_in_string_table;
  Label next_probe[kProbes], next_probe_pop_mask[kProbes];
  Register candidate = scratch;  // Scratch register contains candidate.
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in string table.
    __ mov(scratch, hash);
    if (i > 0) {
      __ add(scratch, Immediate(StringTable::GetProbeOffset(i)));
    }
    __ and_(scratch, mask);

    // Load the entry from the string table.
    STATIC_ASSERT(StringTable::kEntrySize == 1);
    __ mov(candidate,
           FieldOperand(string_table,
                        scratch,
                        times_pointer_size,
                        StringTable::kElementsStartOffset));

    // If entry is undefined no string with this hash can be found.
    Factory* factory = masm->isolate()->factory();
    __ cmp(candidate, factory->undefined_value());
    __ j(equal, not_found);
    __ cmp(candidate, factory->the_hole_value());
    __ j(equal, &next_probe[i]);

    // If length is not 2 the string is not a candidate.
    __ cmp(FieldOperand(candidate, String::kLengthOffset),
           Immediate(Smi::FromInt(2)));
    __ j(not_equal, &next_probe[i]);

    // As we are out of registers save the mask on the stack and use that
    // register as a temporary.
    __ push(mask);
    Register temp = mask;

    // Check that the candidate is a non-external ASCII string.
    __ mov(temp, FieldOperand(candidate, HeapObject::kMapOffset));
    __ movzx_b(temp, FieldOperand(temp, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe_pop_mask[i]);

    // Check if the two characters match.
    __ mov(temp, FieldOperand(candidate, SeqOneByteString::kHeaderSize));
    __ and_(temp, 0x0000ffff);
    __ cmp(chars, temp);
    __ j(equal, &found_in_string_table);
    __ bind(&next_probe_pop_mask[i]);
    __ pop(mask);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = candidate;
  __ bind(&found_in_string_table);
  __ pop(mask);  // Pop saved mask from the stack.
  if (!result.is(eax)) {
    __ mov(eax, result);
  }
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = (seed + character) + ((seed + character) << 10);
  if (Serializer::enabled()) {
    ExternalReference roots_array_start =
        ExternalReference::roots_array_start(masm->isolate());
    __ mov(scratch, Immediate(Heap::kHashSeedRootIndex));
    __ mov(scratch, Operand::StaticArray(scratch,
                                         times_pointer_size,
                                         roots_array_start));
    __ SmiUntag(scratch);
    __ add(scratch, character);
    __ mov(hash, scratch);
    __ shl(scratch, 10);
    __ add(hash, scratch);
  } else {
    int32_t seed = masm->isolate()->heap()->HashSeed();
    __ lea(scratch, Operand(character, seed));
    __ shl(scratch, 10);
    __ lea(hash, Operand(scratch, character, times_1, seed));
  }
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ shr(scratch, 6);
  __ xor_(hash, scratch);
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character,
                                            Register scratch) {
  // hash += character;
  __ add(hash, character);
  // hash += hash << 10;
  __ mov(scratch, hash);
  __ shl(scratch, 10);
  __ add(hash, scratch);
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ shr(scratch, 6);
  __ xor_(hash, scratch);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ mov(scratch, hash);
  __ shl(scratch, 3);
  __ add(hash, scratch);
  // hash ^= hash >> 11;
  __ mov(scratch, hash);
  __ shr(scratch, 11);
  __ xor_(hash, scratch);
  // hash += hash << 15;
  __ mov(scratch, hash);
  __ shl(scratch, 15);
  __ add(hash, scratch);

  __ and_(hash, String::kHashBitMask);

  // if (hash == 0) hash = 27;
  Label hash_not_zero;
  __ j(not_zero, &hash_not_zero, Label::kNear);
  __ mov(hash, Immediate(StringHasher::kZeroHash));
  __ bind(&hash_not_zero);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: to
  //  esp[8]: from
  //  esp[12]: string

  // Make sure first argument is a string.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);

  // eax: string
  // ebx: instance type

  // Calculate length of sub string using the smi values.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));  // To index.
  __ JumpIfNotSmi(ecx, &runtime);
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // From index.
  __ JumpIfNotSmi(edx, &runtime);
  __ sub(ecx, edx);
  __ cmp(ecx, FieldOperand(eax, String::kLengthOffset));
  Label not_original_string;
  // Shorter than original string's length: an actual substring.
  __ j(below, &not_original_string, Label::kNear);
  // Longer than original string's length or negative: unsafe arguments.
  __ j(above, &runtime);
  // Return original string.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);
  __ bind(&not_original_string);

  Label single_char;
  __ cmp(ecx, Immediate(Smi::FromInt(1)));
  __ j(equal, &single_char);

  // eax: string
  // ebx: instance type
  // ecx: sub string length (smi)
  // edx: from index (smi)
  // Deal with different string types: update the index if necessary
  // and put the underlying string into edi.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ test(ebx, Immediate(kIsIndirectStringMask));
  __ j(zero, &seq_or_external_string, Label::kNear);

  Factory* factory = masm->isolate()->factory();
  __ test(ebx, Immediate(kSlicedNotConsMask));
  __ j(not_zero, &sliced_string, Label::kNear);
  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  __ cmp(FieldOperand(eax, ConsString::kSecondOffset),
         factory->empty_string());
  __ j(not_equal, &runtime);
  __ mov(edi, FieldOperand(eax, ConsString::kFirstOffset));
  // Update instance type.
  __ mov(ebx, FieldOperand(edi, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and adjust start index by offset.
  __ add(edx, FieldOperand(eax, SlicedString::kOffsetOffset));
  __ mov(edi, FieldOperand(eax, SlicedString::kParentOffset));
  // Update instance type.
  __ mov(ebx, FieldOperand(edi, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the expected register.
  __ mov(edi, eax);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // edi: underlying subject string
    // ebx: instance type of underlying subject string
    // edx: adjusted start index (smi)
    // ecx: length (smi)
    __ cmp(ecx, Immediate(Smi::FromInt(SlicedString::kMinLength)));
    // Short slice.  Copy instead of slicing.
    __ j(less, &copy_routine);
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ test(ebx, Immediate(kStringEncodingMask));
    __ j(zero, &two_byte_slice, Label::kNear);
    __ AllocateAsciiSlicedString(eax, ebx, no_reg, &runtime);
    __ jmp(&set_slice_header, Label::kNear);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(eax, ebx, no_reg, &runtime);
    __ bind(&set_slice_header);
    __ mov(FieldOperand(eax, SlicedString::kLengthOffset), ecx);
    __ mov(FieldOperand(eax, SlicedString::kHashFieldOffset),
           Immediate(String::kEmptyHashField));
    __ mov(FieldOperand(eax, SlicedString::kParentOffset), edi);
    __ mov(FieldOperand(eax, SlicedString::kOffsetOffset), edx);
    __ IncrementCounter(counters->sub_string_native(), 1);
    __ ret(3 * kPointerSize);

    __ bind(&copy_routine);
  }

  // edi: underlying subject string
  // ebx: instance type of underlying subject string
  // edx: adjusted start index (smi)
  // ecx: length (smi)
  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label two_byte_sequential, runtime_drop_two, sequential_string;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test_b(ebx, kExternalStringTag);
  __ j(zero, &sequential_string);

  // Handle external string.
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ test_b(ebx, kShortExternalStringMask);
  __ j(not_zero, &runtime);
  __ mov(edi, FieldOperand(edi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(edi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&sequential_string);
  // Stash away (adjusted) index and (underlying) string.
  __ push(edx);
  __ push(edi);
  __ SmiUntag(ecx);
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ test_b(ebx, kStringEncodingMask);
  __ j(zero, &two_byte_sequential);

  // Sequential ASCII string.  Allocate the result.
  __ AllocateAsciiString(eax, ecx, ebx, edx, edi, &runtime_drop_two);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(edi, Immediate(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ pop(esi);
  __ pop(ebx);
  __ SmiUntag(ebx);
  __ lea(esi, FieldOperand(esi, ebx, times_1, SeqOneByteString::kHeaderSize));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, true);
  __ mov(esi, edx);  // Restore esi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  __ bind(&two_byte_sequential);
  // Sequential two-byte string.  Allocate the result.
  __ AllocateTwoByteString(eax, ecx, ebx, edx, edi, &runtime_drop_two);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(edi,
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ pop(esi);
  __ pop(ebx);
  // As from is a smi it is 2 times the value which matches the size of a two
  // byte character.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ lea(esi, FieldOperand(esi, ebx, times_1, SeqTwoByteString::kHeaderSize));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, false);
  __ mov(esi, edx);  // Restore esi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  // Drop pushed values on the stack before tail call.
  __ bind(&runtime_drop_two);
  __ Drop(2);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);

  __ bind(&single_char);
  // eax: string
  // ebx: instance type
  // ecx: sub string length (smi)
  // edx: from index (smi)
  StringCharAtGenerator generator(
      eax, edx, ecx, eax, &runtime, &runtime, &runtime, STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ ret(3 * kPointerSize);
  generator.SkipSlow(masm, &runtime);
}


void StringCompareStub::GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                                      Register left,
                                                      Register right,
                                                      Register scratch1,
                                                      Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ mov(length, FieldOperand(left, String::kLengthOffset));
  __ cmp(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ bind(&strings_not_equal);
  __ Set(eax, Immediate(Smi::FromInt(NOT_EQUAL)));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ test(length, length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  GenerateAsciiCharsCompareLoop(masm, left, right, length, scratch2,
                                &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3) {
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_compare_native(), 1);

  // Find minimum length.
  Label left_shorter;
  __ mov(scratch1, FieldOperand(left, String::kLengthOffset));
  __ mov(scratch3, scratch1);
  __ sub(scratch3, FieldOperand(right, String::kLengthOffset));

  Register length_delta = scratch3;

  __ j(less_equal, &left_shorter, Label::kNear);
  // Right string is shorter. Change scratch1 to be length of right string.
  __ sub(scratch1, length_delta);
  __ bind(&left_shorter);

  Register min_length = scratch1;

  // If either length is zero, just compare lengths.
  Label compare_lengths;
  __ test(min_length, min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare characters.
  Label result_not_equal;
  GenerateAsciiCharsCompareLoop(masm, left, right, min_length, scratch2,
                                &result_not_equal, Label::kNear);

  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  __ test(length_delta, length_delta);
  Label length_not_equal;
  __ j(not_zero, &length_not_equal, Label::kNear);

  // Result is EQUAL.
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  Label result_greater;
  Label result_less;
  __ bind(&length_not_equal);
  __ j(greater, &result_greater, Label::kNear);
  __ jmp(&result_less, Label::kNear);
  __ bind(&result_not_equal);
  __ j(above, &result_greater, Label::kNear);
  __ bind(&result_less);

  // Result is LESS.
  __ Set(eax, Immediate(Smi::FromInt(LESS)));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Set(eax, Immediate(Smi::FromInt(GREATER)));
  __ ret(0);
}


void StringCompareStub::GenerateAsciiCharsCompareLoop(
    MacroAssembler* masm,
    Register left,
    Register right,
    Register length,
    Register scratch,
    Label* chars_not_equal,
    Label::Distance chars_not_equal_near) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ lea(left,
         FieldOperand(left, length, times_1, SeqOneByteString::kHeaderSize));
  __ lea(right,
         FieldOperand(right, length, times_1, SeqOneByteString::kHeaderSize));
  __ neg(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, chars_not_equal_near);
  __ inc(index);
  __ j(not_zero, &loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: right string
  //  esp[8]: left string

  __ mov(edx, Operand(esp, 2 * kPointerSize));  // left
  __ mov(eax, Operand(esp, 1 * kPointerSize));  // right

  Label not_same;
  __ cmp(edx, eax);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ IncrementCounter(masm->isolate()->counters()->string_compare_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both objects are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx, &runtime);

  // Compare flat ASCII strings.
  // Drop arguments from the stack.
  __ pop(ecx);
  __ add(esp, Immediate(2 * kPointerSize));
  __ push(ecx);
  GenerateCompareFlatAsciiStrings(masm, edx, eax, ecx, ebx, edi);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMI);
  Label miss;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ sub(eax, edx);
  } else {
    Label done;
    __ sub(edx, eax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ not_(edx);
    __ bind(&done);
    __ mov(eax, edx);
  }
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateNumbers(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::NUMBER);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;

  if (left_ == CompareIC::SMI) {
    __ JumpIfNotSmi(edx, &miss);
  }
  if (right_ == CompareIC::SMI) {
    __ JumpIfNotSmi(eax, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved or SSE2 or CMOV is unsupported.
  if (CpuFeatures::IsSupported(SSE2) && CpuFeatures::IsSupported(CMOV)) {
    CpuFeatureScope scope1(masm, SSE2);
    CpuFeatureScope scope2(masm, CMOV);

    // Load left and right operand.
    Label done, left, left_smi, right_smi;
    __ JumpIfSmi(eax, &right_smi, Label::kNear);
    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, &maybe_undefined1, Label::kNear);
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ jmp(&left, Label::kNear);
    __ bind(&right_smi);
    __ mov(ecx, eax);  // Can't clobber eax because we can still jump away.
    __ SmiUntag(ecx);
    __ cvtsi2sd(xmm1, ecx);

    __ bind(&left);
    __ JumpIfSmi(edx, &left_smi, Label::kNear);
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
    __ jmp(&done);
    __ bind(&left_smi);
    __ mov(ecx, edx);  // Can't clobber edx because we can still jump away.
    __ SmiUntag(ecx);
    __ cvtsi2sd(xmm0, ecx);

    __ bind(&done);
    // Compare operands.
    __ ucomisd(xmm0, xmm1);

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered, Label::kNear);

    // Return a result of -1, 0, or 1, based on EFLAGS.
    // Performing mov, because xor would destroy the flag register.
    __ mov(eax, 0);  // equal
    __ mov(ecx, Immediate(Smi::FromInt(1)));
    __ cmov(above, eax, ecx);
    __ mov(ecx, Immediate(Smi::FromInt(-1)));
    __ cmov(below, eax, ecx);
    __ ret(0);
  } else {
    __ mov(ecx, edx);
    __ and_(ecx, eax);
    __ JumpIfSmi(ecx, &generic_stub, Label::kNear);

    __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, &maybe_undefined1, Label::kNear);
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, &maybe_undefined2, Label::kNear);
  }

  __ bind(&unordered);
  __ bind(&generic_stub);
  ICCompareStub stub(op_, CompareIC::GENERIC, CompareIC::GENERIC,
                     CompareIC::GENERIC);
  __ jmp(stub.GetCode(masm->isolate()), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ cmp(eax, Immediate(masm->isolate()->factory()->undefined_value()));
    __ j(not_equal, &miss);
    __ JumpIfSmi(edx, &unordered);
    __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ecx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ cmp(edx, Immediate(masm->isolate()->factory()->undefined_value()));
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::INTERNALIZED_STRING);
  ASSERT(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are internalized strings.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag != 0);
  __ and_(tmp1, tmp2);
  __ test(tmp1, Immediate(kIsInternalizedMask));
  __ j(zero, &miss, Label::kNear);

  // Internalized strings are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateUniqueNames(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::UNIQUE_NAME);
  ASSERT(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  STATIC_ASSERT(kInternalizedTag != 0);
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  Label succeed1;
  __ test(tmp1, Immediate(kIsInternalizedMask));
  __ j(not_zero, &succeed1);
  __ cmpb(tmp1, static_cast<uint8_t>(SYMBOL_TYPE));
  __ j(not_equal, &miss);
  __ bind(&succeed1);

  Label succeed2;
  __ test(tmp2, Immediate(kIsInternalizedMask));
  __ j(not_zero, &succeed2);
  __ cmpb(tmp2, static_cast<uint8_t>(SYMBOL_TYPE));
  __ j(not_equal, &miss);
  __ bind(&succeed2);

  // Unique names are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateStrings(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::STRING);
  Label miss;

  bool equality = Token::IsEqualityOp(op_);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;
  Register tmp3 = edi;

  // Check that both operands are heap objects.
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ mov(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ or_(tmp3, tmp2);
  __ test(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmp(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are internalized. If they are, we're done
  // because we already know they are not identical.  But in the case of
  // non-equality compare, we still need to determine the order.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag != 0);
    __ and_(tmp1, tmp2);
    __ test(tmp1, Immediate(kIsInternalizedMask));
    __ j(zero, &do_compare, Label::kNear);
    // Make sure eax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    ASSERT(right.is(eax));
    __ ret(0);
    __ bind(&do_compare);
  }

  // Check that both strings are sequential ASCII.
  Label runtime;
  __ JumpIfNotBothSequentialAsciiStrings(left, right, tmp1, tmp2, &runtime);

  // Compare flat ASCII strings. Returns when done.
  if (equality) {
    StringCompareStub::GenerateFlatAsciiStringEquals(
        masm, left, right, tmp1, tmp2);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(
        masm, left, right, tmp1, tmp2, tmp3);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ pop(tmp1);  // Return address.
  __ push(left);
  __ push(right);
  __ push(tmp1);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals, 2, 1);
  } else {
    __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::OBJECT);
  Label miss;
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  __ CmpObjectType(eax, JS_OBJECT_TYPE, ecx);
  __ j(not_equal, &miss, Label::kNear);
  __ CmpObjectType(edx, JS_OBJECT_TYPE, ecx);
  __ j(not_equal, &miss, Label::kNear);

  ASSERT(GetCondition() == equal);
  __ sub(eax, edx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateKnownObjects(MacroAssembler* masm) {
  Label miss;
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ecx, known_map_);
  __ j(not_equal, &miss, Label::kNear);
  __ cmp(ebx, known_map_);
  __ j(not_equal, &miss, Label::kNear);

  __ sub(eax, edx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    ExternalReference miss = ExternalReference(IC_Utility(IC::kCompareIC_Miss),
                                               masm->isolate());
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(edx);  // Preserve edx and eax.
    __ push(eax);
    __ push(edx);  // And also use them as the arguments.
    __ push(eax);
    __ push(Immediate(Smi::FromInt(op_)));
    __ CallExternalReference(miss, 3);
    // Compute the entry point of the rewritten stub.
    __ lea(edi, FieldOperand(eax, Code::kHeaderSize));
    __ pop(eax);
    __ pop(edx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(edi);
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be a unique name and receiver must be a heap object.
void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register properties,
                                                      Handle<Name> name,
                                                      Register r0) {
  ASSERT(name->IsUniqueName());

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ mov(index, FieldOperand(properties, kCapacityOffset));
    __ dec(index);
    __ and_(index,
            Immediate(Smi::FromInt(name->Hash() +
                                   NameDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.
    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    __ mov(entity_name, Operand(properties, index, times_half_pointer_size,
                                kElementsStartOffset - kHeapObjectTag));
    __ cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ cmp(entity_name, Handle<Name>(name));
    __ j(equal, miss);

    Label good;
    // Check for the hole and skip.
    __ cmp(entity_name, masm->isolate()->factory()->the_hole_value());
    __ j(equal, &good, Label::kNear);

    // Check if the entry name is not a unique name.
    __ mov(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ test_b(FieldOperand(entity_name, Map::kInstanceTypeOffset),
              kIsInternalizedMask);
    __ j(not_zero, &good);
    __ cmpb(FieldOperand(entity_name, Map::kInstanceTypeOffset),
            static_cast<uint8_t>(SYMBOL_TYPE));
    __ j(not_equal, miss);
    __ bind(&good);
  }

  NameDictionaryLookupStub stub(properties, r0, r0, NEGATIVE_LOOKUP);
  __ push(Immediate(Handle<Object>(name)));
  __ push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ test(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found leaving the
// index into the dictionary in |r0|. Jump to the |miss| label
// otherwise.
void NameDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register elements,
                                                      Register name,
                                                      Register r0,
                                                      Register r1) {
  ASSERT(!elements.is(r0));
  ASSERT(!elements.is(r1));
  ASSERT(!name.is(r0));
  ASSERT(!name.is(r1));

  __ AssertName(name);

  __ mov(r1, FieldOperand(elements, kCapacityOffset));
  __ shr(r1, kSmiTagSize);  // convert smi to int
  __ dec(r1);

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(r0, FieldOperand(name, Name::kHashFieldOffset));
    __ shr(r0, Name::kHashShift);
    if (i > 0) {
      __ add(r0, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(r0, r1);

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(r0, Operand(r0, r0, times_2, 0));  // r0 = r0 * 3

    // Check if the key is identical to the name.
    __ cmp(name, Operand(elements,
                         r0,
                         times_4,
                         kElementsStartOffset - kHeapObjectTag));
    __ j(equal, done);
  }

  NameDictionaryLookupStub stub(elements, r1, r0, POSITIVE_LOOKUP);
  __ push(name);
  __ mov(r0, FieldOperand(name, Name::kHashFieldOffset));
  __ shr(r0, Name::kHashShift);
  __ push(r0);
  __ CallStub(&stub);

  __ test(r1, r1);
  __ j(zero, miss);
  __ jmp(done);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  esp[0 * kPointerSize]: return address.
  //  esp[1 * kPointerSize]: key's hash.
  //  esp[2 * kPointerSize]: key.
  // Registers:
  //  dictionary_: NameDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result_;

  __ mov(scratch, FieldOperand(dictionary_, kCapacityOffset));
  __ dec(scratch);
  __ SmiUntag(scratch);
  __ push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
    if (i > 0) {
      __ add(scratch, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(esp, 0));

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index_, Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    __ mov(scratch, Operand(dictionary_,
                            index_,
                            times_pointer_size,
                            kElementsStartOffset - kHeapObjectTag));
    __ cmp(scratch, masm->isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmp(scratch, Operand(esp, 3 * kPointerSize));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode_ == NEGATIVE_LOOKUP) {
      // If we hit a key that is not a unique name during negative
      // lookup we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a unique name.
      Label cont;
      __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ test_b(FieldOperand(scratch, Map::kInstanceTypeOffset),
                kIsInternalizedMask);
      __ j(not_zero, &cont);
      __ cmpb(FieldOperand(scratch, Map::kInstanceTypeOffset),
              static_cast<uint8_t>(SYMBOL_TYPE));
      __ j(not_equal, &maybe_in_dictionary);
      __ bind(&cont);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode_ == POSITIVE_LOOKUP) {
    __ mov(result_, Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ mov(result_, Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ mov(result_, Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


struct AheadOfTimeWriteBarrierStubList {
  Register object, value, address;
  RememberedSetAction action;
};


#define REG(Name) { kRegister_ ## Name ## _Code }

static const AheadOfTimeWriteBarrierStubList kAheadOfTime[] = {
  // Used in RegExpExecStub.
  { REG(ebx), REG(eax), REG(edi), EMIT_REMEMBERED_SET },
  // Used in CompileArrayPushCall.
  { REG(ebx), REG(ecx), REG(edx), EMIT_REMEMBERED_SET },
  { REG(ebx), REG(edi), REG(edx), OMIT_REMEMBERED_SET },
  // Used in CompileStoreGlobal and CallFunctionStub.
  { REG(ebx), REG(ecx), REG(edx), OMIT_REMEMBERED_SET },
  // Used in StoreStubCompiler::CompileStoreField and
  // KeyedStoreStubCompiler::CompileStoreField via GenerateStoreField.
  { REG(edx), REG(ecx), REG(ebx), EMIT_REMEMBERED_SET },
  // GenerateStoreField calls the stub with two different permutations of
  // registers.  This is the second.
  { REG(ebx), REG(ecx), REG(edx), EMIT_REMEMBERED_SET },
  // StoreIC::GenerateNormal via GenerateDictionaryStore
  { REG(ebx), REG(edi), REG(edx), EMIT_REMEMBERED_SET },
  // KeyedStoreIC::GenerateGeneric.
  { REG(ebx), REG(edx), REG(ecx), EMIT_REMEMBERED_SET},
  // KeyedStoreStubCompiler::GenerateStoreFastElement.
  { REG(edi), REG(ebx), REG(ecx), EMIT_REMEMBERED_SET},
  { REG(edx), REG(edi), REG(ebx), EMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateMapChangeElementTransition
  // and ElementsTransitionGenerator::GenerateSmiToDouble
  // and ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(edx), REG(ebx), REG(edi), EMIT_REMEMBERED_SET},
  { REG(edx), REG(ebx), REG(edi), OMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(eax), REG(edx), REG(esi), EMIT_REMEMBERED_SET},
  { REG(edx), REG(eax), REG(edi), EMIT_REMEMBERED_SET},
  // StoreArrayLiteralElementStub::Generate
  { REG(ebx), REG(eax), REG(ecx), EMIT_REMEMBERED_SET},
  // FastNewClosureStub and StringAddStub::Generate
  { REG(ecx), REG(edx), REG(ebx), EMIT_REMEMBERED_SET},
  // StringAddStub::Generate
  { REG(ecx), REG(eax), REG(ebx), EMIT_REMEMBERED_SET},
  // Null termination.
  { REG(no_reg), REG(no_reg), REG(no_reg), EMIT_REMEMBERED_SET}
};

#undef REG

bool RecordWriteStub::IsPregenerated() {
  for (const AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
       !entry->object.is(no_reg);
       entry++) {
    if (object_.is(entry->object) &&
        value_.is(entry->value) &&
        address_.is(entry->address) &&
        remembered_set_action_ == entry->action &&
        save_fp_regs_mode_ == kDontSaveFPRegs) {
      return true;
    }
  }
  return false;
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub(kDontSaveFPRegs);
  stub.GetCode(isolate)->set_is_pregenerated(true);
  if (CpuFeatures::IsSafeForSnapshot(SSE2)) {
    StoreBufferOverflowStub stub2(kSaveFPRegs);
    stub2.GetCode(isolate)->set_is_pregenerated(true);
  }
}


void RecordWriteStub::GenerateFixedRegStubsAheadOfTime(Isolate* isolate) {
  for (const AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
       !entry->object.is(no_reg);
       entry++) {
    RecordWriteStub stub(entry->object,
                         entry->value,
                         entry->address,
                         entry->action,
                         kDontSaveFPRegs);
    stub.GetCode(isolate)->set_is_pregenerated(true);
  }
}


bool CodeStub::CanUseFPRegisters() {
  return CpuFeatures::IsSupported(SSE2);
}


// Takes the input in 3 registers: address_ value_ and object_.  A pointer to
// the value has just been written into the object, now this stub makes sure
// we keep the GC informed.  The word in the object where the value has been
// written is in the address register.
void RecordWriteStub::Generate(MacroAssembler* masm) {
  Label skip_to_incremental_noncompacting;
  Label skip_to_incremental_compacting;

  // The first two instructions are generated with labels so as to get the
  // offset fixed up correctly by the bind(Label*) call.  We patch it back and
  // forth between a compare instructions (a nop in this position) and the
  // real branch when we start and stop incremental heap marking.
  __ jmp(&skip_to_incremental_noncompacting, Label::kNear);
  __ jmp(&skip_to_incremental_compacting, Label::kFar);

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.
  masm->set_byte_at(0, kTwoByteNopInstruction);
  masm->set_byte_at(2, kFiveByteNopInstruction);
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ mov(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch0(),
                     1 << MemoryChunk::SCAN_ON_SCAVENGE,
                     not_zero,
                     &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm,
        kUpdateRememberedSetOnNoNeedToInformIncrementalMarker,
        mode);
    InformIncrementalMarker(masm, mode);
    regs_.Restore(masm);
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm,
      kReturnOnNoNeedToInformIncrementalMarker,
      mode);
  InformIncrementalMarker(masm, mode);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm, Mode mode) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode_);
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  __ mov(Operand(esp, 0 * kPointerSize), regs_.object());
  __ mov(Operand(esp, 1 * kPointerSize), regs_.address());  // Slot.
  __ mov(Operand(esp, 2 * kPointerSize),
         Immediate(ExternalReference::isolate_address(masm->isolate())));

  AllowExternalCallThatCantCauseGC scope(masm);
  if (mode == INCREMENTAL_COMPACTION) {
    __ CallCFunction(
        ExternalReference::incremental_evacuation_record_write_function(
            masm->isolate()),
        argument_count);
  } else {
    ASSERT(mode == INCREMENTAL);
    __ CallCFunction(
        ExternalReference::incremental_marking_record_write_function(
            masm->isolate()),
        argument_count);
  }
  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode_);
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label object_is_black, need_incremental, need_incremental_pop_object;

  __ mov(regs_.scratch0(), Immediate(~Page::kPageAlignmentMask));
  __ and_(regs_.scratch0(), regs_.object());
  __ mov(regs_.scratch1(),
         Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset));
  __ sub(regs_.scratch1(), Immediate(1));
  __ mov(Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset),
         regs_.scratch1());
  __ j(negative, &need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &object_is_black,
                 Label::kNear);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&object_is_black);

  // Get the value from the slot.
  __ mov(regs_.scratch0(), Operand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask,
                     zero,
                     &ensure_not_white,
                     Label::kNear);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                     not_zero,
                     &ensure_not_white,
                     Label::kNear);

    __ jmp(&need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need an extra register for this, so we push the object register
  // temporarily.
  __ push(regs_.object());
  __ EnsureNotWhite(regs_.scratch0(),  // The value.
                    regs_.scratch1(),  // Scratch.
                    regs_.object(),  // Scratch.
                    &need_incremental_pop_object,
                    Label::kNear);
  __ pop(regs_.object());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&need_incremental_pop_object);
  __ pop(regs_.object());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StoreArrayLiteralElementStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : element value to store
  //  -- ebx    : array literal
  //  -- edi    : map of array literal
  //  -- ecx    : element index as smi
  //  -- edx    : array literal index in function
  //  -- esp[0] : return address
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label slow_elements_from_double;
  Label fast_elements;

  __ CheckFastElements(edi, &double_elements);

  // Check for FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS elements
  __ JumpIfSmi(eax, &smi_element);
  __ CheckFastSmiElements(edi, &fast_elements, Label::kNear);

  // Store into the array literal requires a elements transition. Call into
  // the runtime.

  __ bind(&slow_elements);
  __ pop(edi);  // Pop return address and remember to put back later for tail
                // call.
  __ push(ebx);
  __ push(ecx);
  __ push(eax);
  __ mov(ebx, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(ebx, JSFunction::kLiteralsOffset));
  __ push(edx);
  __ push(edi);  // Return return address so that tail call returns to right
                 // place.
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  __ bind(&slow_elements_from_double);
  __ pop(edx);
  __ jmp(&slow_elements);

  // Array literal has ElementsKind of FAST_*_ELEMENTS and value is an object.
  __ bind(&fast_elements);
  __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
  __ lea(ecx, FieldOperand(ebx, ecx, times_half_pointer_size,
                           FixedArrayBase::kHeaderSize));
  __ mov(Operand(ecx, 0), eax);
  // Update the write barrier for the array store.
  __ RecordWrite(ebx, ecx, eax,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ ret(0);

  // Array literal has ElementsKind of FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS,
  // and value is Smi.
  __ bind(&smi_element);
  __ mov(ebx, FieldOperand(ebx, JSObject::kElementsOffset));
  __ mov(FieldOperand(ebx, ecx, times_half_pointer_size,
                      FixedArrayBase::kHeaderSize), eax);
  __ ret(0);

  // Array literal has ElementsKind of FAST_*_DOUBLE_ELEMENTS.
  __ bind(&double_elements);

  __ push(edx);
  __ mov(edx, FieldOperand(ebx, JSObject::kElementsOffset));
  __ StoreNumberToDoubleElements(eax,
                                 edx,
                                 ecx,
                                 edi,
                                 xmm0,
                                 &slow_elements_from_double,
                                 false);
  __ pop(edx);
  __ ret(0);
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(1, fp_registers_ ? kSaveFPRegs : kDontSaveFPRegs);
  __ call(ces.GetCode(masm->isolate()), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ mov(ebx, MemOperand(ebp, parameter_count_offset));
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ pop(ecx);
  int additional_offset = function_mode_ == JS_FUNCTION_STUB_MODE
      ? kPointerSize
      : 0;
  __ lea(esp, MemOperand(esp, ebx, times_pointer_size, additional_offset));
  __ jmp(ecx);  // Return to IC Miss stub, continuation still on stack.
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (entry_hook_ != NULL) {
    ProfileEntryHookStub stub;
    masm->CallStub(&stub);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // Ecx is the only volatile register we must save.
  __ push(ecx);

  // Calculate and push the original stack pointer.
  __ lea(eax, Operand(esp, kPointerSize));
  __ push(eax);

  // Calculate and push the function address.
  __ mov(eax, Operand(eax, 0));
  __ sub(eax, Immediate(Assembler::kCallInstructionLength));
  __ push(eax);

  // Call the entry hook.
  int32_t hook_location = reinterpret_cast<int32_t>(&entry_hook_);
  __ call(Operand(hook_location, RelocInfo::NONE32));
  __ add(esp, Immediate(2 * kPointerSize));

  // Restore ecx.
  __ pop(ecx);
  __ ret(0);
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm) {
  int last_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= last_index; ++i) {
    Label next;
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    __ cmp(edx, kind);
    __ j(not_equal, &next);
    T stub(kind);
    __ TailCallStub(&stub);
    __ bind(&next);
  }

  // If we reached this point there is a problem.
  __ Abort("Unexpected ElementsKind in array constructor");
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm) {
  // ebx - type info cell
  // edx - kind
  // eax - number of arguments
  // edi - constructor?
  // esp[0] - return address
  // esp[4] - last argument
  ASSERT(FAST_SMI_ELEMENTS == 0);
  ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  ASSERT(FAST_ELEMENTS == 2);
  ASSERT(FAST_HOLEY_ELEMENTS == 3);
  ASSERT(FAST_DOUBLE_ELEMENTS == 4);
  ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());

  // is the low bit set? If so, we are holey and that is good.
  __ test_b(edx, 1);
  Label normal_sequence;
  __ j(not_zero, &normal_sequence);

  // look at the first argument
  __ mov(ecx, Operand(esp, kPointerSize));
  __ test(ecx, ecx);
  __ j(zero, &normal_sequence);

  // We are going to create a holey array, but our kind is non-holey.
  // Fix kind and retry
  __ inc(edx);
  __ cmp(ebx, Immediate(undefined_sentinel));
  __ j(equal, &normal_sequence);

  // Save the resulting elements kind in type info
  __ SmiTag(edx);
  __ mov(FieldOperand(ebx, kPointerSize), edx);
  __ SmiUntag(edx);

  __ bind(&normal_sequence);
  int last_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= last_index; ++i) {
    Label next;
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    __ cmp(edx, kind);
    __ j(not_equal, &next);
    ArraySingleArgumentConstructorStub stub(kind);
    __ TailCallStub(&stub);
    __ bind(&next);
  }

  // If we reached this point there is a problem.
  __ Abort("Unexpected ElementsKind in array constructor");
}


template<class T>
static void ArrayConstructorStubAheadOfTimeHelper(Isolate* isolate) {
  int to_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(kind);
    stub.GetCode(isolate)->set_is_pregenerated(true);
  }
}


void ArrayConstructorStubBase::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArrayNArgumentsConstructorStub>(
      isolate);
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc (only if argument_count_ == ANY)
  //  -- ebx : type info cell
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, "Unexpected initial map for Array function");
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, "Unexpected initial map for Array function");

    // We should either have undefined in ebx or a valid jsglobalpropertycell
    Label okay_here;
    Handle<Map> global_property_cell_map(
        masm->isolate()->heap()->global_property_cell_map());
    __ cmp(ebx, Immediate(undefined_sentinel));
    __ j(equal, &okay_here);
    __ cmp(FieldOperand(ebx, 0), Immediate(global_property_cell_map));
    __ Assert(equal, "Expected property cell in register ebx");
    __ bind(&okay_here);
  }

  if (FLAG_optimize_constructed_arrays) {
    Label no_info, switch_ready;
    // Get the elements kind and case on that.
    __ cmp(ebx, Immediate(undefined_sentinel));
    __ j(equal, &no_info);
    __ mov(edx, FieldOperand(ebx, JSGlobalPropertyCell::kValueOffset));
    __ JumpIfNotSmi(edx, &no_info);
    __ SmiUntag(edx);
    __ jmp(&switch_ready);
    __ bind(&no_info);
    __ mov(edx, Immediate(GetInitialFastElementsKind()));
    __ bind(&switch_ready);

    if (argument_count_ == ANY) {
      Label not_zero_case, not_one_case;
      __ test(eax, eax);
      __ j(not_zero, &not_zero_case);
      CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm);

      __ bind(&not_zero_case);
      __ cmp(eax, 1);
      __ j(greater, &not_one_case);
      CreateArrayDispatchOneArgument(masm);

      __ bind(&not_one_case);
      CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm);
    } else if (argument_count_ == NONE) {
      CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm);
    } else if (argument_count_ == ONE) {
      CreateArrayDispatchOneArgument(masm);
    } else if (argument_count_ == MORE_THAN_ONE) {
      CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm);
    } else {
      UNREACHABLE();
    }
  } else {
    Label generic_constructor;
    // Run the native code for the Array function called as constructor.
    ArrayNativeCode(masm, true, &generic_constructor);

    // Jump to the generic construct code in case the specialized code cannot
    // handle the construction.
    __ bind(&generic_constructor);
    Handle<Code> generic_construct_stub =
        masm->isolate()->builtins()->JSConstructStubGeneric();
    __ jmp(generic_construct_stub, RelocInfo::CODE_TARGET);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
