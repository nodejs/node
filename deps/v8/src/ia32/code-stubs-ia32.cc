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

#if V8_TARGET_ARCH_IA32

#include "bootstrapper.h"
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


void FastNewClosureStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { ebx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kNewClosureFromStubFailure)->entry;
}


void FastNewContextStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edi };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void ToNumberStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void NumberToStringStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kNumberToString)->entry;
}


void FastCloneShallowArrayStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx, ecx };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateArrayLiteralStubBailout)->entry;
}


void FastCloneShallowObjectStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx, ecx, edx };
  descriptor->register_param_count_ = 4;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateObjectLiteral)->entry;
}


void CreateAllocationSiteStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { ebx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedLoadFastElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, ecx };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure);
}


void KeyedLoadDictionaryElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, ecx };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure);
}


void RegExpConstructResultStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { ecx, ebx, eax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kRegExpConstructResult)->entry;
}


void LoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedLoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
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
  // ebx -- allocation site with elements kind
  static Register registers_variable_args[] = { edi, ebx, eax };
  static Register registers_no_args[] = { edi, ebx };

  if (constant_stack_parameter_count == 0) {
    descriptor->register_param_count_ = 2;
    descriptor->register_params_ = registers_no_args;
  } else {
    // stack param count needs (constructor pointer, and single argument)
    descriptor->handler_arguments_mode_ = PASS_ARGUMENTS;
    descriptor->stack_parameter_count_ = eax;
    descriptor->register_param_count_ = 3;
    descriptor->register_params_ = registers_variable_args;
  }

  descriptor->hint_stack_parameter_count_ = constant_stack_parameter_count;
  descriptor->function_mode_ = JS_FUNCTION_STUB_MODE;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kArrayConstructor)->entry;
}


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // eax -- number of arguments
  // edi -- constructor function
  static Register registers_variable_args[] = { edi, eax };
  static Register registers_no_args[] = { edi };

  if (constant_stack_parameter_count == 0) {
    descriptor->register_param_count_ = 1;
    descriptor->register_params_ = registers_no_args;
  } else {
    // stack param count needs (constructor pointer, and single argument)
    descriptor->handler_arguments_mode_ = PASS_ARGUMENTS;
    descriptor->stack_parameter_count_ = eax;
    descriptor->register_param_count_ = 2;
    descriptor->register_params_ = registers_variable_args;
  }

  descriptor->hint_stack_parameter_count_ = constant_stack_parameter_count;
  descriptor->function_mode_ = JS_FUNCTION_STUB_MODE;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kInternalArrayConstructor)->entry;
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


void InternalArrayNoArgumentConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate, descriptor, 0);
}


void InternalArraySingleArgumentConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate, descriptor, 1);
}


void InternalArrayNArgumentsConstructorStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(isolate, descriptor, -1);
}


void CompareNilICStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(CompareNilIC_Miss);
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kCompareNilIC_Miss), isolate));
}

void ToBooleanStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(ToBooleanIC_Miss);
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kToBooleanIC_Miss), isolate));
}


void StoreGlobalStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, ecx, eax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(StoreIC_MissFromStubFailure);
}


void ElementsTransitionAndStoreStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { eax, ebx, ecx, edx };
  descriptor->register_param_count_ = 4;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(ElementsTransitionAndStoreIC_Miss);
}


void BinaryOpICStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, eax };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = FUNCTION_ADDR(BinaryOpIC_Miss);
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kBinaryOpIC_Miss), isolate));
}


void BinaryOpWithAllocationSiteStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { ecx, edx, eax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(BinaryOpIC_MissWithAllocationSite);
}


void StringAddStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { edx, eax };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kStringAdd)->entry;
}


void CallDescriptors::InitializeForIsolate(Isolate* isolate) {
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::ArgumentAdaptorCall);
    static Register registers[] = { edi,  // JSFunction
                                    esi,  // context
                                    eax,  // actual number of arguments
                                    ebx,  // expected number of arguments
    };
    static Representation representations[] = {
        Representation::Tagged(),     // JSFunction
        Representation::Tagged(),     // context
        Representation::Integer32(),  // actual number of arguments
        Representation::Integer32(),  // expected number of arguments
    };
    descriptor->register_param_count_ = 4;
    descriptor->register_params_ = registers;
    descriptor->param_representations_ = representations;
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::KeyedCall);
    static Register registers[] = { esi,  // context
                                    ecx,  // key
    };
    static Representation representations[] = {
        Representation::Tagged(),     // context
        Representation::Tagged(),     // key
    };
    descriptor->register_param_count_ = 2;
    descriptor->register_params_ = registers;
    descriptor->param_representations_ = representations;
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::NamedCall);
    static Register registers[] = { esi,  // context
                                    ecx,  // name
    };
    static Representation representations[] = {
        Representation::Tagged(),     // context
        Representation::Tagged(),     // name
    };
    descriptor->register_param_count_ = 2;
    descriptor->register_params_ = registers;
    descriptor->param_representations_ = representations;
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::CallHandler);
    static Register registers[] = { esi,  // context
                                    edx,  // receiver
    };
    static Representation representations[] = {
        Representation::Tagged(),  // context
        Representation::Tagged(),  // receiver
    };
    descriptor->register_param_count_ = 2;
    descriptor->register_params_ = registers;
    descriptor->param_representations_ = representations;
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::ApiFunctionCall);
    static Register registers[] = { eax,  // callee
                                    ebx,  // call_data
                                    ecx,  // holder
                                    edx,  // api_function_address
                                    esi,  // context
    };
    static Representation representations[] = {
        Representation::Tagged(),    // callee
        Representation::Tagged(),    // call_data
        Representation::Tagged(),    // holder
        Representation::External(),  // api_function_address
        Representation::Tagged(),    // context
    };
    descriptor->register_param_count_ = 5;
    descriptor->register_params_ = registers;
    descriptor->param_representations_ = representations;
  }
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
    ExternalReference miss = descriptor->miss_handler();
    __ CallExternalReference(miss, descriptor->register_param_count_);
  }

  __ ret(0);
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
      __ movsd(Operand(esp, i * kDoubleSize), reg);
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
      __ movsd(reg, Operand(esp, i * kDoubleSize));
    }
    __ add(esp, Immediate(kDoubleSize * XMMRegister::kNumRegisters));
  }
  __ popad();
  __ ret(0);
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

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);

  // Test if operands are numbers (smi or HeapNumber objects), and load
  // them into xmm0 and xmm1 if they are.  Jump to label not_numbers if
  // either operand is not a number.  Operands are in edx and eax.
  // Leaves operands unchanged.
  static void LoadSSE2Operands(MacroAssembler* masm, Label* not_numbers);
};


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Register input_reg = this->source();
  Register final_result_reg = this->destination();
  ASSERT(is_truncating());

  Label check_negative, process_64_bits, done, done_no_stash;

  int double_offset = offset();

  // Account for return address and saved regs if input is esp.
  if (input_reg.is(esp)) double_offset += 3 * kPointerSize;

  MemOperand mantissa_operand(MemOperand(input_reg, double_offset));
  MemOperand exponent_operand(MemOperand(input_reg,
                                         double_offset + kDoubleSize / 2));

  Register scratch1;
  {
    Register scratch_candidates[3] = { ebx, edx, edi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (!final_result_reg.is(scratch1) && !input_reg.is(scratch1)) break;
    }
  }
  // Since we must use ecx for shifts below, use some other register (eax)
  // to calculate the result if ecx is the requested return register.
  Register result_reg = final_result_reg.is(ecx) ? eax : final_result_reg;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead for
  // the result.
  Register save_reg = final_result_reg.is(ecx) ? eax : ecx;
  __ push(scratch1);
  __ push(save_reg);

  bool stash_exponent_copy = !input_reg.is(esp);
  __ mov(scratch1, mantissa_operand);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    // Load x87 register with heap number.
    __ fld_d(mantissa_operand);
  }
  __ mov(ecx, exponent_operand);
  if (stash_exponent_copy) __ push(ecx);

  __ and_(ecx, HeapNumber::kExponentMask);
  __ shr(ecx, HeapNumber::kExponentShift);
  __ lea(result_reg, MemOperand(ecx, -HeapNumber::kExponentBias));
  __ cmp(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  if (CpuFeatures::IsSupported(SSE3)) {
    __ fstp(0);
  }
  __ sub(ecx, Immediate(delta));
  __ xor_(result_reg, result_reg);
  __ cmp(ecx, Immediate(31));
  __ j(above, &done);
  __ shl_cl(scratch1);
  __ jmp(&check_negative);

  __ bind(&process_64_bits);
  if (CpuFeatures::IsSupported(SSE3)) {
    CpuFeatureScope scope(masm, SSE3);
    if (stash_exponent_copy) {
      // Already a copy of the exponent on the stack, overwrite it.
      STATIC_ASSERT(kDoubleSize == 2 * kPointerSize);
      __ sub(esp, Immediate(kDoubleSize / 2));
    } else {
      // Reserve space for 64 bit answer.
      __ sub(esp, Immediate(kDoubleSize));  // Nolint.
    }
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(result_reg, Operand(esp, 0));  // Load low word of answer as result
    __ add(esp, Immediate(kDoubleSize));
    __ jmp(&done_no_stash);
  } else {
    // Result must be extracted from shifted 32-bit mantissa
    __ sub(ecx, Immediate(delta));
    __ neg(ecx);
    if (stash_exponent_copy) {
      __ mov(result_reg, MemOperand(esp, 0));
    } else {
      __ mov(result_reg, exponent_operand);
    }
    __ and_(result_reg,
            Immediate(static_cast<uint32_t>(Double::kSignificandMask >> 32)));
    __ add(result_reg,
           Immediate(static_cast<uint32_t>(Double::kHiddenBit >> 32)));
    __ shrd(result_reg, scratch1);
    __ shr_cl(result_reg);
    __ test(ecx, Immediate(32));
    if (CpuFeatures::IsSupported(CMOV)) {
      CpuFeatureScope use_cmov(masm, CMOV);
      __ cmov(not_equal, scratch1, result_reg);
    } else {
      Label skip_mov;
      __ j(equal, &skip_mov, Label::kNear);
      __ mov(scratch1, result_reg);
      __ bind(&skip_mov);
    }
  }

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ mov(result_reg, scratch1);
  __ neg(result_reg);
  if (stash_exponent_copy) {
    __ cmp(MemOperand(esp, 0), Immediate(0));
  } else {
    __ cmp(exponent_operand, Immediate(0));
  }
  if (CpuFeatures::IsSupported(CMOV)) {
    CpuFeatureScope use_cmov(masm, CMOV);
    __ cmov(greater, result_reg, scratch1);
  } else {
    Label skip_mov;
    __ j(less_equal, &skip_mov, Label::kNear);
    __ mov(result_reg, scratch1);
    __ bind(&skip_mov);
  }

  // Restore registers
  __ bind(&done);
  if (stash_exponent_copy) {
    __ add(esp, Immediate(kDoubleSize / 2));
  }
  __ bind(&done_no_stash);
  if (!final_result_reg.is(result_reg)) {
    ASSERT(final_result_reg.is(ecx));
    __ mov(final_result_reg, result_reg);
  }
  __ pop(save_reg);
  __ pop(scratch1);
  __ ret(0);
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


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm,
                                           Label* not_numbers) {
  Label load_smi_edx, load_eax, load_smi_eax, load_float_eax, done;
  // Load operand in edx into xmm0, or branch to not_numbers.
  __ JumpIfSmi(edx, &load_smi_edx, Label::kNear);
  Factory* factory = masm->isolate()->factory();
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(not_equal, not_numbers);  // Argument in edx is not a number.
  __ movsd(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ bind(&load_eax);
  // Load operand in eax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(eax, &load_smi_eax, Label::kNear);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), factory->heap_number_map());
  __ j(equal, &load_float_eax, Label::kNear);
  __ jmp(not_numbers);  // Argument in eax is not a number.
  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ Cvtsi2sd(xmm0, edx);
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);
  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ Cvtsi2sd(xmm1, eax);
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.
  __ jmp(&done, Label::kNear);
  __ bind(&load_float_eax);
  __ movsd(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ bind(&done);
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
  __ Cvtsi2sd(double_result, scratch);

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

    __ movsd(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiUntag(base);
    __ Cvtsi2sd(double_base, base);

    __ bind(&unpack_exponent);
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ cmp(FieldOperand(exponent, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(not_equal, &call_runtime);
    __ movsd(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type_ == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiUntag(exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ movsd(double_exponent,
              FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type_ != INTEGER) {
    Label fast_power, try_arithmetic_simplification;
    __ DoubleToI(exponent, double_exponent, double_scratch,
                 TREAT_MINUS_ZERO_AS_ZERO, &try_arithmetic_simplification);
    __ jmp(&int_exponent);

    __ bind(&try_arithmetic_simplification);
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cvttsd2si(exponent, Operand(double_exponent));
    __ cmp(exponent, Immediate(0x80000000u));
    __ j(equal, &call_runtime);

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
    __ movsd(Operand(esp, 0), double_exponent);
    __ fld_d(Operand(esp, 0));  // E
    __ movsd(Operand(esp, 0), double_base);
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
    __ movsd(double_result, Operand(esp, 0));
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
  __ Cvtsi2sd(double_exponent, exponent);

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
    __ movsd(FieldOperand(eax, HeapNumber::kValueOffset), double_result);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(4, scratch);
      __ movsd(Operand(esp, 0 * kDoubleSize), double_base);
      __ movsd(Operand(esp, 1 * kDoubleSize), double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(masm->isolate()), 4);
    }
    // Return value is in st(0) on ia32.
    // Store it into the (fixed) result register.
    __ sub(esp, Immediate(kDoubleSize));
    __ fstp_d(Operand(esp, 0));
    __ movsd(double_result, Operand(esp, 0));
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
  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
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

  StubCompiler::GenerateLoadStringLength(masm, edx, eax, ebx, &miss);
  __ bind(&miss);
  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
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

  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
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
  Isolate* isolate = masm->isolate();

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
         Immediate(isolate->factory()->non_strict_arguments_elements_map()));
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
  __ mov(ecx, isolate->factory()->the_hole_value());
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
         Immediate(isolate->factory()->fixed_array_map()));
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
  Isolate* isolate = masm->isolate();

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
         Immediate(isolate->factory()->fixed_array_map()));

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
    __ Check(not_zero, kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CmpObjectType(ecx, FIXED_ARRAY_TYPE, ebx);
    __ Check(equal, kUnexpectedTypeForRegExpDataFixedArrayExpected);
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
  __ LeaveApiExitFrame(true);

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
    __ Assert(zero, kExternalStringExpectedButNotFound);
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
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ test(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, label);
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
  __ Abort(kUnexpectedFallThroughFromStringComparison);
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


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a global property cell.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // eax : number of arguments to the construct function
  // ebx : cache cell for call target
  // edi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function;

  // Load the cache state into ecx.
  __ mov(ecx, FieldOperand(ebx, Cell::kValueOffset));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ cmp(ecx, edi);
  __ j(equal, &done);
  __ cmp(ecx, Immediate(TypeFeedbackCells::MegamorphicSentinel(isolate)));
  __ j(equal, &done);

  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the cell either some other function or an
  // AllocationSite. Do a map check on the object in ecx.
  Handle<Map> allocation_site_map =
      masm->isolate()->factory()->allocation_site_map();
  __ cmp(FieldOperand(ecx, 0), Immediate(allocation_site_map));
  __ j(not_equal, &miss);

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
  __ mov(FieldOperand(ebx, Cell::kValueOffset),
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

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the cell
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Arguments register must be smi-tagged to call out.
    __ SmiTag(eax);
    __ push(eax);
    __ push(edi);
    __ push(ebx);

    CreateAllocationSiteStub create_stub;
    __ CallStub(&create_stub);

    __ pop(ebx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }
  __ jmp(&done);

  __ bind(&not_array_function);
  __ mov(FieldOperand(ebx, Cell::kValueOffset), edi);
  // No need for a write barrier here - cells are rescanned.

  __ bind(&done);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  // ebx : cache cell for call target
  // edi : the function to call
  Isolate* isolate = masm->isolate();
  Label slow, non_function, wrap, cont;

  if (NeedsChecks()) {
    // Check that the function really is a JavaScript function.
    __ JumpIfSmi(edi, &non_function);

    // Goto slow case if we do not have a function.
    __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
    __ j(not_equal, &slow);

    if (RecordCallTarget()) {
      GenerateRecordCallTarget(masm);
    }
  }

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);

  if (CallAsMethod()) {
    if (NeedsChecks()) {
      // Do not transform the receiver for strict mode functions.
      __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
      __ test_b(FieldOperand(ecx, SharedFunctionInfo::kStrictModeByteOffset),
                1 << SharedFunctionInfo::kStrictModeBitWithinByte);
      __ j(not_equal, &cont);

      // Do not transform the receiver for natives (shared already in ecx).
      __ test_b(FieldOperand(ecx, SharedFunctionInfo::kNativeByteOffset),
                1 << SharedFunctionInfo::kNativeBitWithinByte);
      __ j(not_equal, &cont);
    }

    // Load the receiver from the stack.
    __ mov(eax, Operand(esp, (argc_ + 1) * kPointerSize));

    if (NeedsChecks()) {
      __ JumpIfSmi(eax, &wrap);

      __ CmpObjectType(eax, FIRST_SPEC_OBJECT_TYPE, ecx);
      __ j(below, &wrap);
    } else {
      __ jmp(&wrap);
    }

    __ bind(&cont);
  }

  __ InvokeFunction(edi, actual, JUMP_FUNCTION, NullCallWrapper());

  if (NeedsChecks()) {
    // Slow-case: Non-function called.
    __ bind(&slow);
    if (RecordCallTarget()) {
      // If there is a call target cache, mark it megamorphic in the
      // non-function case.  MegamorphicSentinel is an immortal immovable
      // object (undefined) so no write barrier is needed.
      __ mov(FieldOperand(ebx, Cell::kValueOffset),
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
    __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
    Handle<Code> adaptor = isolate->builtins()->ArgumentsAdaptorTrampoline();
    __ jmp(adaptor, RelocInfo::CODE_TARGET);
  }

  if (CallAsMethod()) {
    __ bind(&wrap);
    // Wrap the receiver and patch it back onto the stack.
    { FrameScope frame_scope(masm, StackFrame::INTERNAL);
      __ push(edi);
      __ push(eax);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ pop(edi);
    }
    __ mov(Operand(esp, (argc_ + 1) * kPointerSize), eax);
    __ jmp(&cont);
  }
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
    GenerateRecordCallTarget(masm);
  }

  // Jump to the function-specific construct stub.
  Register jmp_reg = ecx;
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
  __ jmp(arguments_adaptor, RelocInfo::CODE_TARGET);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  if (Serializer::enabled()) {
    PlatformFeatureScope sse2(SSE2);
    BinaryOpICStub::GenerateAheadOfTime(isolate);
    BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  } else {
    BinaryOpICStub::GenerateAheadOfTime(isolate);
    BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
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
    isolate->set_fp_stubs_generated(true);
  }
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(1, kDontSaveFPRegs);
  stub.GetCode(isolate);
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
    __ mov(Operand(esp, 1 * kPointerSize),
           Immediate(ExternalReference::isolate_address(masm->isolate())));
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

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

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

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

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
    __ CompareRoot(function, scratch, Heap::kInstanceofCacheFunctionRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ CompareRoot(map, scratch, Heap::kInstanceofCacheMapRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ LoadRoot(eax, Heap::kInstanceofCacheAnswerRootIndex);
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
    __ StoreRoot(map, scratch, Heap::kInstanceofCacheMapRootIndex);
    __ StoreRoot(function, scratch, Heap::kInstanceofCacheFunctionRootIndex);
  } else {
    // The constants for the code patching are based on no push instructions
    // at the call site.
    ASSERT(HasArgsInRegisters());
    // Get return address and delta to inlined map check.
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, 0), kCmpEdiOperandByte1);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheCmp1);
      __ cmpb(Operand(scratch, 1), kCmpEdiOperandByte2);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheCmp2);
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
    __ mov(eax, Immediate(0));
    __ StoreRoot(eax, scratch, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, factory->true_value());
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, kDeltaToMov), kMovEaxImmediateByte);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheMov);
    }
    __ mov(Operand(scratch, kDeltaToMovImmediate), eax);
    if (!ReturnTrueFalseObject()) {
      __ Set(eax, Immediate(0));
    }
  }
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ mov(eax, Immediate(Smi::FromInt(1)));
    __ StoreRoot(eax, scratch, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, factory->false_value());
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, kDeltaToMov), kMovEaxImmediateByte);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheMov);
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
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

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

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
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
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
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


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = (seed + character) + ((seed + character) << 10);
  if (Serializer::enabled()) {
    __ LoadRoot(scratch, Heap::kHashSeedRootIndex);
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


void ArrayPushStub::Generate(MacroAssembler* masm) {
  int argc = arguments_count();

  if (argc == 0) {
    // Noop, return the length.
    __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
    __ ret((argc + 1) * kPointerSize);
    return;
  }

  Isolate* isolate = masm->isolate();

  if (argc != 1) {
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate), argc + 1, 1);
    return;
  }

  Label call_builtin, attempt_to_grow_elements, with_write_barrier;

  // Get the elements array of the object.
  __ mov(edi, FieldOperand(edx, JSArray::kElementsOffset));

  if (IsFastSmiOrObjectElementsKind(elements_kind())) {
    // Check that the elements are in fast mode and writable.
    __ cmp(FieldOperand(edi, HeapObject::kMapOffset),
           isolate->factory()->fixed_array_map());
    __ j(not_equal, &call_builtin);
  }

  // Get the array's length into eax and calculate new length.
  __ mov(eax, FieldOperand(edx, JSArray::kLengthOffset));
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ add(eax, Immediate(Smi::FromInt(argc)));

  // Get the elements' length into ecx.
  __ mov(ecx, FieldOperand(edi, FixedArray::kLengthOffset));

  // Check if we could survive without allocation.
  __ cmp(eax, ecx);

  if (IsFastSmiOrObjectElementsKind(elements_kind())) {
    __ j(greater, &attempt_to_grow_elements);

    // Check if value is a smi.
    __ mov(ecx, Operand(esp, argc * kPointerSize));
    __ JumpIfNotSmi(ecx, &with_write_barrier);

    // Store the value.
    __ mov(FieldOperand(edi, eax, times_half_pointer_size,
                        FixedArray::kHeaderSize - argc * kPointerSize),
           ecx);
  } else {
    __ j(greater, &call_builtin);

    __ mov(ecx, Operand(esp, argc * kPointerSize));
    __ StoreNumberToDoubleElements(
        ecx, edi, eax, ecx, xmm0, &call_builtin, true, argc * kDoubleSize);
  }

  // Save new length.
  __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);
  __ ret((argc + 1) * kPointerSize);

  if (IsFastDoubleElementsKind(elements_kind())) {
    __ bind(&call_builtin);
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate), argc + 1, 1);
    return;
  }

  __ bind(&with_write_barrier);

  if (IsFastSmiElementsKind(elements_kind())) {
    if (FLAG_trace_elements_transitions) __ jmp(&call_builtin);

    __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
           isolate->factory()->heap_number_map());
    __ j(equal, &call_builtin);

    ElementsKind target_kind = IsHoleyElementsKind(elements_kind())
        ? FAST_HOLEY_ELEMENTS : FAST_ELEMENTS;
    __ mov(ebx, ContextOperand(esi, Context::GLOBAL_OBJECT_INDEX));
    __ mov(ebx, FieldOperand(ebx, GlobalObject::kNativeContextOffset));
    __ mov(ebx, ContextOperand(ebx, Context::JS_ARRAY_MAPS_INDEX));
    const int header_size = FixedArrayBase::kHeaderSize;
    // Verify that the object can be transitioned in place.
    const int origin_offset = header_size + elements_kind() * kPointerSize;
    __ mov(edi, FieldOperand(ebx, origin_offset));
    __ cmp(edi, FieldOperand(edx, HeapObject::kMapOffset));
    __ j(not_equal, &call_builtin);

    const int target_offset = header_size + target_kind * kPointerSize;
    __ mov(ebx, FieldOperand(ebx, target_offset));
    ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
        masm, DONT_TRACK_ALLOCATION_SITE, NULL);
    // Restore edi used as a scratch register for the write barrier used while
    // setting the map.
    __ mov(edi, FieldOperand(edx, JSArray::kElementsOffset));
  }

  // Save new length.
  __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);

  // Store the value.
  __ lea(edx, FieldOperand(edi, eax, times_half_pointer_size,
                           FixedArray::kHeaderSize - argc * kPointerSize));
  __ mov(Operand(edx, 0), ecx);

  __ RecordWrite(edi, edx, ecx, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);

  __ ret((argc + 1) * kPointerSize);

  __ bind(&attempt_to_grow_elements);
  if (!FLAG_inline_new) {
    __ bind(&call_builtin);
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate), argc + 1, 1);
    return;
  }

  __ mov(ebx, Operand(esp, argc * kPointerSize));
  // Growing elements that are SMI-only requires special handling in case the
  // new element is non-Smi. For now, delegate to the builtin.
  if (IsFastSmiElementsKind(elements_kind())) {
    __ JumpIfNotSmi(ebx, &call_builtin);
  }

  // We could be lucky and the elements array could be at the top of new-space.
  // In this case we can just grow it in place by moving the allocation pointer
  // up.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate);
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address(isolate);

  const int kAllocationDelta = 4;
  ASSERT(kAllocationDelta >= argc);
  // Load top.
  __ mov(ecx, Operand::StaticVariable(new_space_allocation_top));

  // Check if it's the end of elements.
  __ lea(edx, FieldOperand(edi, eax, times_half_pointer_size,
                           FixedArray::kHeaderSize - argc * kPointerSize));
  __ cmp(edx, ecx);
  __ j(not_equal, &call_builtin);
  __ add(ecx, Immediate(kAllocationDelta * kPointerSize));
  __ cmp(ecx, Operand::StaticVariable(new_space_allocation_limit));
  __ j(above, &call_builtin);

  // We fit and could grow elements.
  __ mov(Operand::StaticVariable(new_space_allocation_top), ecx);

  // Push the argument...
  __ mov(Operand(edx, 0), ebx);
  // ... and fill the rest with holes.
  for (int i = 1; i < kAllocationDelta; i++) {
    __ mov(Operand(edx, i * kPointerSize),
           isolate->factory()->the_hole_value());
  }

  if (IsFastObjectElementsKind(elements_kind())) {
    // We know the elements array is in new space so we don't need the
    // remembered set, but we just pushed a value onto it so we may have to tell
    // the incremental marker to rescan the object that we just grew.  We don't
    // need to worry about the holes because they are in old space and already
    // marked black.
    __ RecordWrite(edi, edx, ebx, kDontSaveFPRegs, OMIT_REMEMBERED_SET);
  }

  // Restore receiver to edx as finish sequence assumes it's here.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Increment element's and array's sizes.
  __ add(FieldOperand(edi, FixedArray::kLengthOffset),
         Immediate(Smi::FromInt(kAllocationDelta)));

  // NOTE: This only happen in new-space, where we don't care about the
  // black-byte-count on pages. Otherwise we should update that too if the
  // object is black.

  __ mov(FieldOperand(edx, JSArray::kLengthOffset), eax);
  __ ret((argc + 1) * kPointerSize);

  __ bind(&call_builtin);
  __ TailCallExternalReference(
      ExternalReference(Builtins::c_ArrayPush, isolate), argc + 1, 1);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : left
  //  -- eax    : right
  //  -- esp[0] : return address
  // -----------------------------------
  Isolate* isolate = masm->isolate();

  // Load ecx with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ mov(ecx, handle(isolate->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_equal, kExpectedAllocationSite);
    __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
           isolate->factory()->allocation_site_map());
    __ Assert(equal, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(state_);
  __ TailCallStub(&stub);
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
    __ movsd(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ jmp(&left, Label::kNear);
    __ bind(&right_smi);
    __ mov(ecx, eax);  // Can't clobber eax because we can still jump away.
    __ SmiUntag(ecx);
    __ Cvtsi2sd(xmm1, ecx);

    __ bind(&left);
    __ JumpIfSmi(edx, &left_smi, Label::kNear);
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           masm->isolate()->factory()->heap_number_map());
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ movsd(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
    __ jmp(&done);
    __ bind(&left_smi);
    __ mov(ecx, edx);  // Can't clobber edx because we can still jump away.
    __ SmiUntag(ecx);
    __ Cvtsi2sd(xmm0, ecx);

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
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ or_(tmp1, tmp2);
  __ test(tmp1, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, &miss, Label::kNear);

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
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueName(tmp1, &miss, Label::kNear);
  __ JumpIfNotUniqueName(tmp2, &miss, Label::kNear);

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
  // non-equality compare, we still need to determine the order. We
  // also know they are both strings.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag == 0);
    __ or_(tmp1, tmp2);
    __ test(tmp1, Immediate(kIsNotInternalizedMask));
    __ j(not_zero, &do_compare, Label::kNear);
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
    __ JumpIfNotUniqueName(FieldOperand(entity_name, Map::kInstanceTypeOffset),
                           miss);
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
      __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ JumpIfNotUniqueName(FieldOperand(scratch, Map::kInstanceTypeOffset),
                             &maybe_in_dictionary);
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


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub(kDontSaveFPRegs);
  stub.GetCode(isolate);
  if (CpuFeatures::IsSafeForSnapshot(SSE2)) {
    StoreBufferOverflowStub stub2(kSaveFPRegs);
    stub2.GetCode(isolate);
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
  //  -- ecx    : element index as smi
  //  -- esp[0] : return address
  //  -- esp[4] : array literal index in function
  //  -- esp[8] : array literal
  // clobbers ebx, edx, edi
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label slow_elements_from_double;
  Label fast_elements;

  // Get array literal index, array literal and its map.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(ebx, JSObject::kMapOffset));

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
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub;
    masm->CallStub(&stub);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // Save volatile registers.
  const int kNumSavedRegisters = 3;
  __ push(eax);
  __ push(ecx);
  __ push(edx);

  // Calculate and push the original stack pointer.
  __ lea(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ push(eax);

  // Retrieve our return address and use it to calculate the calling
  // function's address.
  __ mov(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ sub(eax, Immediate(Assembler::kCallInstructionLength));
  __ push(eax);

  // Call the entry hook.
  ASSERT(masm->isolate()->function_entry_hook() != NULL);
  __ call(FUNCTION_ADDR(masm->isolate()->function_entry_hook()),
          RelocInfo::RUNTIME_ENTRY);
  __ add(esp, Immediate(2 * kPointerSize));

  // Restore ecx.
  __ pop(edx);
  __ pop(ecx);
  __ pop(eax);

  __ ret(0);
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(GetInitialFastElementsKind(),
           mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
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
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // ebx - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // edx - kind (if mode != DISABLE_ALLOCATION_SITES)
  // eax - number of arguments
  // edi - constructor?
  // esp[0] - return address
  // esp[4] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    ASSERT(FAST_SMI_ELEMENTS == 0);
    ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    ASSERT(FAST_ELEMENTS == 2);
    ASSERT(FAST_HOLEY_ELEMENTS == 3);
    ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ test_b(edx, 1);
    __ j(not_zero, &normal_sequence);
  }

  // look at the first argument
  __ mov(ecx, Operand(esp, kPointerSize));
  __ test(ecx, ecx);
  __ j(zero, &normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(holey_initial,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry.
    __ inc(edx);

    if (FLAG_debug_code) {
      Handle<Map> allocation_site_map =
          masm->isolate()->factory()->allocation_site_map();
      __ cmp(FieldOperand(ebx, 0), Immediate(allocation_site_map));
      __ Assert(equal, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ add(FieldOperand(ebx, AllocationSite::kTransitionInfoOffset),
           Immediate(Smi::FromInt(kFastElementsKindPackedToHoley)));

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
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


template<class T>
static void ArrayConstructorStubAheadOfTimeHelper(Isolate* isolate) {
  int to_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(kind);
    stub.GetCode(isolate);
    if (AllocationSite::GetMode(kind) != DONT_TRACK_ALLOCATION_SITE) {
      T stub1(kind, DISABLE_ALLOCATION_SITES);
      stub1.GetCode(isolate);
    }
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


void InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(
    Isolate* isolate) {
  ElementsKind kinds[2] = { FAST_ELEMENTS, FAST_HOLEY_ELEMENTS };
  for (int i = 0; i < 2; i++) {
    // For internal arrays we only need a few things
    InternalArrayNoArgumentConstructorStub stubh1(kinds[i]);
    stubh1.GetCode(isolate);
    InternalArraySingleArgumentConstructorStub stubh2(kinds[i]);
    stubh2.GetCode(isolate);
    InternalArrayNArgumentsConstructorStub stubh3(kinds[i]);
    stubh3.GetCode(isolate);
  }
}


void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  if (argument_count_ == ANY) {
    Label not_zero_case, not_one_case;
    __ test(eax, eax);
    __ j(not_zero, &not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ cmp(eax, 1);
    __ j(greater, &not_one_case);
    CreateArrayDispatchOneArgument(masm, mode);

    __ bind(&not_one_case);
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else if (argument_count_ == NONE) {
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);
  } else if (argument_count_ == ONE) {
    CreateArrayDispatchOneArgument(masm, mode);
  } else if (argument_count_ == MORE_THAN_ONE) {
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else {
    UNREACHABLE();
  }
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
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in ebx or a valid cell
    Label okay_here;
    Handle<Map> cell_map = masm->isolate()->factory()->cell_map();
    __ cmp(ebx, Immediate(undefined_sentinel));
    __ j(equal, &okay_here);
    __ cmp(FieldOperand(ebx, 0), Immediate(cell_map));
    __ Assert(equal, kExpectedPropertyCellInRegisterEbx);
    __ bind(&okay_here);
  }

  Label no_info;
  // If the type cell is undefined, or contains anything other than an
  // AllocationSite, call an array constructor that doesn't use AllocationSites.
  __ cmp(ebx, Immediate(undefined_sentinel));
  __ j(equal, &no_info);
  __ mov(ebx, FieldOperand(ebx, Cell::kValueOffset));
  __ cmp(FieldOperand(ebx, 0), Immediate(
      masm->isolate()->factory()->allocation_site_map()));
  __ j(not_equal, &no_info);

  // Only look at the lower 16 bits of the transition info.
  __ mov(edx, FieldOperand(ebx, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(edx);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ and_(edx, Immediate(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ test(eax, eax);
  __ j(not_zero, &not_zero_case);
  InternalArrayNoArgumentConstructorStub stub0(kind);
  __ TailCallStub(&stub0);

  __ bind(&not_zero_case);
  __ cmp(eax, 1);
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ mov(ecx, Operand(esp, kPointerSize));
    __ test(ecx, ecx);
    __ j(zero, &normal_sequence);

    InternalArraySingleArgumentConstructorStub
        stub1_holey(GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey);
  }

  __ bind(&normal_sequence);
  InternalArraySingleArgumentConstructorStub stub1(kind);
  __ TailCallStub(&stub1);

  __ bind(&not_one_case);
  InternalArrayNArgumentsConstructorStub stubN(kind);
  __ TailCallStub(&stubN);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- ebx : type info cell
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ and_(ecx, Map::kElementsKindMask);
  __ shr(ecx, Map::kElementsKindShift);

  if (FLAG_debug_code) {
    Label done;
    __ cmp(ecx, Immediate(FAST_ELEMENTS));
    __ j(equal, &done);
    __ cmp(ecx, Immediate(FAST_HOLEY_ELEMENTS));
    __ Assert(equal,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(ecx, Immediate(FAST_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax                 : callee
  //  -- ebx                 : call_data
  //  -- ecx                 : holder
  //  -- edx                 : api_function_address
  //  -- esi                 : context
  //  --
  //  -- esp[0]              : return address
  //  -- esp[4]              : last argument
  //  -- ...
  //  -- esp[argc * 4]       : first argument
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  Register callee = eax;
  Register call_data = ebx;
  Register holder = ecx;
  Register api_function_address = edx;
  Register return_address = edi;
  Register context = esi;

  int argc = ArgumentBits::decode(bit_field_);
  bool restore_context = RestoreContextBits::decode(bit_field_);
  bool call_data_undefined = CallDataUndefinedBits::decode(bit_field_);

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  Isolate* isolate = masm->isolate();

  __ pop(return_address);

  // context save
  __ push(context);
  // load context from callee
  __ mov(context, FieldOperand(callee, JSFunction::kContextOffset));

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined) {
    // return value
    __ push(Immediate(isolate->factory()->undefined_value()));
    // return value default
    __ push(Immediate(isolate->factory()->undefined_value()));
  } else {
    // return value
    __ push(scratch);
    // return value default
    __ push(scratch);
  }
  // isolate
  __ push(Immediate(reinterpret_cast<int>(isolate)));
  // holder
  __ push(holder);

  __ mov(scratch, esp);

  // return address
  __ push(return_address);

  // API function gets reference to the v8::Arguments. If CPU profiler
  // is enabled wrapper function will be called and we need to pass
  // address of the callback as additional parameter, always allocate
  // space for it.
  const int kApiArgc = 1 + 1;

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  __ PrepareCallApiFunction(kApiArgc + kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_.
  __ mov(ApiParameterOperand(2), scratch);
  __ add(scratch, Immediate((argc + FCA::kArgsLength - 1) * kPointerSize));
  // FunctionCallbackInfo::values_.
  __ mov(ApiParameterOperand(3), scratch);
  // FunctionCallbackInfo::length_.
  __ Set(ApiParameterOperand(4), Immediate(argc));
  // FunctionCallbackInfo::is_construct_call_.
  __ Set(ApiParameterOperand(5), Immediate(0));

  // v8::InvocationCallback's argument.
  __ lea(scratch, ApiParameterOperand(2));
  __ mov(ApiParameterOperand(0), scratch);

  Address thunk_address = FUNCTION_ADDR(&InvokeFunctionCallback);

  Operand context_restore_operand(ebp,
                                  (2 + FCA::kContextSaveIndex) * kPointerSize);
  Operand return_value_operand(ebp,
                               (2 + FCA::kReturnValueOffset) * kPointerSize);
  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_address,
                              ApiParameterOperand(1),
                              argc + FCA::kArgsLength + 1,
                              return_value_operand,
                              restore_context ?
                                  &context_restore_operand : NULL);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- esp[0]                  : return address
  //  -- esp[4]                  : name
  //  -- esp[8 - kArgsLength*4]  : PropertyCallbackArguments object
  //  -- ...
  //  -- edx                    : api_function_address
  // -----------------------------------

  // array for v8::Arguments::values_, handler for name and pointer
  // to the values (it considered as smi in GC).
  const int kStackSpace = PropertyCallbackArguments::kArgsLength + 2;
  // Allocate space for opional callback address parameter in case
  // CPU profiler is active.
  const int kApiArgc = 2 + 1;

  Register api_function_address = edx;
  Register scratch = ebx;

  // load address of name
  __ lea(scratch, Operand(esp, 1 * kPointerSize));

  __ PrepareCallApiFunction(kApiArgc);
  __ mov(ApiParameterOperand(0), scratch);  // name.
  __ add(scratch, Immediate(kPointerSize));
  __ mov(ApiParameterOperand(1), scratch);  // arguments pointer.

  Address thunk_address = FUNCTION_ADDR(&InvokeAccessorGetterCallback);

  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_address,
                              ApiParameterOperand(2),
                              kStackSpace,
                              Operand(ebp, 7 * kPointerSize),
                              NULL);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
