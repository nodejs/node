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

#if V8_TARGET_ARCH_X64

#include "bootstrapper.h"
#include "code-stubs.h"
#include "regexp-macro-assembler.h"
#include "stub-cache.h"
#include "runtime.h"

namespace v8 {
namespace internal {


void FastNewClosureStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rbx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kNewClosureFromStubFailure)->entry;
}


void ToNumberStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void FastCloneShallowArrayStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax, rbx, rcx };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateArrayLiteralShallow)->entry;
}


void FastCloneShallowObjectStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax, rbx, rcx, rdx };
  descriptor->register_param_count_ = 4;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kCreateObjectLiteralShallow)->entry;
}


void CreateAllocationSiteStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rbx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedLoadFastElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rdx, rax };
  descriptor->register_param_count_ = 2;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure);
}


void LoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedLoadFieldStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rdx };
  descriptor->register_param_count_ = 1;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ = NULL;
}


void KeyedStoreFastElementStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rdx, rcx, rax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(KeyedStoreIC_MissFromStubFailure);
}


void TransitionElementsKindStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax, rbx };
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
  // rax -- number of arguments
  // rdi -- function
  // rbx -- type info cell with elements kind
  static Register registers[] = { rdi, rbx };
  descriptor->register_param_count_ = 2;
  if (constant_stack_parameter_count != 0) {
    // stack param count needs (constructor pointer, and single argument)
    descriptor->stack_parameter_count_ = &rax;
  }
  descriptor->hint_stack_parameter_count_ = constant_stack_parameter_count;
  descriptor->register_params_ = registers;
  descriptor->function_mode_ = JS_FUNCTION_STUB_MODE;
  descriptor->deoptimization_handler_ =
      Runtime::FunctionForId(Runtime::kArrayConstructor)->entry;
}


static void InitializeInternalArrayConstructorDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // rax -- number of arguments
  // rdi -- constructor function
  static Register registers[] = { rdi };
  descriptor->register_param_count_ = 1;

  if (constant_stack_parameter_count != 0) {
    // stack param count needs (constructor pointer, and single argument)
    descriptor->stack_parameter_count_ = &rax;
  }
  descriptor->hint_stack_parameter_count_ = constant_stack_parameter_count;
  descriptor->register_params_ = registers;
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
  static Register registers[] = { rax };
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
  static Register registers[] = { rax };
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
  static Register registers[] = { rdx, rcx, rax };
  descriptor->register_param_count_ = 3;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(StoreIC_MissFromStubFailure);
}


void ElementsTransitionAndStoreStub::InitializeInterfaceDescriptor(
    Isolate* isolate,
    CodeStubInterfaceDescriptor* descriptor) {
  static Register registers[] = { rax, rbx, rcx, rdx };
  descriptor->register_param_count_ = 4;
  descriptor->register_params_ = registers;
  descriptor->deoptimization_handler_ =
      FUNCTION_ADDR(ElementsTransitionAndStoreIC_Miss);
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
           rax.is(descriptor->register_params_[param_count - 1]));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor->register_params_[i]);
    }
    ExternalReference miss = descriptor->miss_handler();
    __ CallExternalReference(miss, descriptor->register_param_count_);
  }

  __ Ret();
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ Allocate((length * kPointerSize) + FixedArray::kHeaderSize,
              rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rcx, args.GetArgumentOperand(0));

  // Set up the object header.
  __ LoadRoot(kScratchRegister, Heap::kFunctionContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ Move(FieldOperand(rax, FixedArray::kLengthOffset), Smi::FromInt(length));

  // Set up the fixed slots.
  __ Set(rbx, 0);  // Set to NULL.
  __ movq(Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)), rcx);
  __ movq(Operand(rax, Context::SlotOffset(Context::PREVIOUS_INDEX)), rsi);
  __ movq(Operand(rax, Context::SlotOffset(Context::EXTENSION_INDEX)), rbx);

  // Copy the global object from the previous context.
  __ movq(rbx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ movq(Operand(rax, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)), rbx);

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ movq(Operand(rax, Context::SlotOffset(i)), rbx);
  }

  // Return and remove the on-stack parameter.
  __ movq(rsi, rax);
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewFunctionContext, 1, 1);
}


void FastNewBlockContextStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + (1 * kPointerSize)] : function
  // [rsp + (2 * kPointerSize)] : serialized scope info

  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ Allocate(FixedArray::SizeFor(length),
              rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rcx, args.GetArgumentOperand(1));
  // Get the serialized scope info from the stack.
  __ movq(rbx, args.GetArgumentOperand(0));

  // Set up the object header.
  __ LoadRoot(kScratchRegister, Heap::kBlockContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ Move(FieldOperand(rax, FixedArray::kLengthOffset), Smi::FromInt(length));

  // If this block context is nested in the native context we get a smi
  // sentinel instead of a function. The block context should get the
  // canonical empty function of the native context as its closure which
  // we still have to look up.
  Label after_sentinel;
  __ JumpIfNotSmi(rcx, &after_sentinel, Label::kNear);
  if (FLAG_debug_code) {
    __ cmpq(rcx, Immediate(0));
    __ Assert(equal, kExpected0AsASmiSentinel);
  }
  __ movq(rcx, GlobalObjectOperand());
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kNativeContextOffset));
  __ movq(rcx, ContextOperand(rcx, Context::CLOSURE_INDEX));
  __ bind(&after_sentinel);

  // Set up the fixed slots.
  __ movq(ContextOperand(rax, Context::CLOSURE_INDEX), rcx);
  __ movq(ContextOperand(rax, Context::PREVIOUS_INDEX), rsi);
  __ movq(ContextOperand(rax, Context::EXTENSION_INDEX), rbx);

  // Copy the global object from the previous context.
  __ movq(rbx, ContextOperand(rsi, Context::GLOBAL_OBJECT_INDEX));
  __ movq(ContextOperand(rax, Context::GLOBAL_OBJECT_INDEX), rbx);

  // Initialize the rest of the slots to the hole value.
  __ LoadRoot(rbx, Heap::kTheHoleValueRootIndex);
  for (int i = 0; i < slots_; i++) {
    __ movq(ContextOperand(rax, i + Context::MIN_CONTEXT_SLOTS), rbx);
  }

  // Return and remove the on-stack parameter.
  __ movq(rsi, rax);
  __ ret(2 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kPushBlockContext, 2, 1);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  __ PushCallerSaved(save_doubles_);
  const int argument_count = 1;
  __ PrepareCallCFunction(argument_count);
  __ LoadAddress(arg_reg_1,
                 ExternalReference::isolate_address(masm->isolate()));

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(masm->isolate()),
      argument_count);
  __ PopCallerSaved(save_doubles_);
  __ ret(0);
}


class FloatingPointHelper : public AllStatic {
 public:
  enum ConvertUndefined {
    CONVERT_UNDEFINED_TO_ZERO,
    BAILOUT_ON_UNDEFINED
  };
  // Load the operands from rdx and rax into xmm0 and xmm1, as doubles.
  // If the operands are not both numbers, jump to not_numbers.
  // Leaves rdx and rax unchanged.  SmiOperands assumes both are smis.
  // NumberOperands assumes both are smis or heap numbers.
  static void LoadSSE2SmiOperands(MacroAssembler* masm);
  static void LoadSSE2UnknownOperands(MacroAssembler* masm,
                                      Label* not_numbers);

  // Takes the operands in rdx and rax and loads them as integers in rax
  // and rcx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             Label* operand_conversion_failure,
                             Register heap_number_map);

  // Tries to convert two values to smis losslessly.
  // This fails if either argument is not a Smi nor a HeapNumber,
  // or if it's a HeapNumber with a value that can't be converted
  // losslessly to a Smi. In that case, control transitions to the
  // on_not_smis label.
  // On success, either control goes to the on_success label (if one is
  // provided), or it falls through at the end of the code (if on_success
  // is NULL).
  // On success, both first and second holds Smi tagged values.
  // One of first or second must be non-Smi when entering.
  static void NumbersToSmis(MacroAssembler* masm,
                            Register first,
                            Register second,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            Label* on_success,
                            Label* on_not_smis,
                            ConvertUndefined convert_undefined);
};


void DoubleToIStub::Generate(MacroAssembler* masm) {
    Register input_reg = this->source();
    Register final_result_reg = this->destination();
    ASSERT(is_truncating());

    Label check_negative, process_64_bits, done;

    int double_offset = offset();

    // Account for return address and saved regs if input is rsp.
    if (input_reg.is(rsp)) double_offset += 3 * kPointerSize;

    MemOperand mantissa_operand(MemOperand(input_reg, double_offset));
    MemOperand exponent_operand(MemOperand(input_reg,
                                           double_offset + kDoubleSize / 2));

    Register scratch1;
    Register scratch_candidates[3] = { rbx, rdx, rdi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (!final_result_reg.is(scratch1) && !input_reg.is(scratch1)) break;
    }

    // Since we must use rcx for shifts below, use some other register (rax)
    // to calculate the result if ecx is the requested return register.
    Register result_reg = final_result_reg.is(rcx) ? rax : final_result_reg;
    // Save ecx if it isn't the return register and therefore volatile, or if it
    // is the return register, then save the temp register we use in its stead
    // for the result.
    Register save_reg = final_result_reg.is(rcx) ? rax : rcx;
    __ push(scratch1);
    __ push(save_reg);

    bool stash_exponent_copy = !input_reg.is(rsp);
    __ movl(scratch1, mantissa_operand);
    __ movsd(xmm0, mantissa_operand);
    __ movl(rcx, exponent_operand);
    if (stash_exponent_copy) __ push(rcx);

    __ andl(rcx, Immediate(HeapNumber::kExponentMask));
    __ shrl(rcx, Immediate(HeapNumber::kExponentShift));
    __ leal(result_reg, MemOperand(rcx, -HeapNumber::kExponentBias));
    __ cmpl(result_reg, Immediate(HeapNumber::kMantissaBits));
    __ j(below, &process_64_bits);

    // Result is entirely in lower 32-bits of mantissa
    int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
    __ subl(rcx, Immediate(delta));
    __ xorl(result_reg, result_reg);
    __ cmpl(rcx, Immediate(31));
    __ j(above, &done);
    __ shll_cl(scratch1);
    __ jmp(&check_negative);

    __ bind(&process_64_bits);
    __ cvttsd2siq(result_reg, xmm0);
    __ jmp(&done, Label::kNear);

    // If the double was negative, negate the integer result.
    __ bind(&check_negative);
    __ movl(result_reg, scratch1);
    __ negl(result_reg);
    if (stash_exponent_copy) {
        __ cmpl(MemOperand(rsp, 0), Immediate(0));
    } else {
        __ cmpl(exponent_operand, Immediate(0));
    }
    __ cmovl(greater, result_reg, scratch1);

    // Restore registers
    __ bind(&done);
    if (stash_exponent_copy) {
        __ addq(rsp, Immediate(kDoubleSize));
    }
    if (!final_result_reg.is(result_reg)) {
        ASSERT(final_result_reg.is(rcx));
        __ movl(final_result_reg, result_reg);
    }
    __ pop(save_reg);
    __ pop(scratch1);
    __ ret(0);
}


void BinaryOpStub::Initialize() {}


void BinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ PopReturnAddressTo(rcx);
  __ push(rdx);
  __ push(rax);
  // Left and right arguments are now on top.
  __ Push(Smi::FromInt(MinorKey()));

  __ PushReturnAddressFrom(rcx);

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

  // Arguments to BinaryOpStub are in rdx and rax.
  const Register left = rdx;
  const Register right = rax;

  // We only generate heapnumber answers for overflowing calculations
  // for the four basic arithmetic operations and logical right shift by 0.
  bool generate_inline_heapnumber_results =
      (allow_heapnumber_results == BinaryOpStub::ALLOW_HEAPNUMBER_RESULTS) &&
      (op == Token::ADD || op == Token::SUB ||
       op == Token::MUL || op == Token::DIV || op == Token::SHR);

  // Smi check of both operands.  If op is BIT_OR, the check is delayed
  // until after the OR operation.
  Label not_smis;
  Label use_fp_on_smis;
  Label fail;

  if (op != Token::BIT_OR) {
    Comment smi_check_comment(masm, "-- Smi check arguments");
    __ JumpIfNotBothSmi(left, right, &not_smis);
  }

  Label smi_values;
  __ bind(&smi_values);
  // Perform the operation.
  Comment perform_smi(masm, "-- Perform smi operation");
  switch (op) {
    case Token::ADD:
      ASSERT(right.is(rax));
      __ SmiAdd(right, right, left, &use_fp_on_smis);  // ADD is commutative.
      break;

    case Token::SUB:
      __ SmiSub(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;

    case Token::MUL:
      ASSERT(right.is(rax));
      __ SmiMul(right, right, left, &use_fp_on_smis);  // MUL is commutative.
      break;

    case Token::DIV:
      // SmiDiv will not accept left in rdx or right in rax.
      __ movq(rbx, rax);
      __ movq(rcx, rdx);
      __ SmiDiv(rax, rcx, rbx, &use_fp_on_smis);
      break;

    case Token::MOD:
      // SmiMod will not accept left in rdx or right in rax.
      __ movq(rbx, rax);
      __ movq(rcx, rdx);
      __ SmiMod(rax, rcx, rbx, &use_fp_on_smis);
      break;

    case Token::BIT_OR: {
      ASSERT(right.is(rax));
      __ SmiOrIfSmis(right, right, left, &not_smis);  // BIT_OR is commutative.
      break;
      }
    case Token::BIT_XOR:
      ASSERT(right.is(rax));
      __ SmiXor(right, right, left);  // BIT_XOR is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(rax));
      __ SmiAnd(right, right, left);  // BIT_AND is commutative.
      break;

    case Token::SHL:
      __ SmiShiftLeft(left, left, right);
      __ movq(rax, left);
      break;

    case Token::SAR:
      __ SmiShiftArithmeticRight(left, left, right);
      __ movq(rax, left);
      break;

    case Token::SHR:
      __ SmiShiftLogicalRight(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;

    default:
      UNREACHABLE();
  }

  // 5. Emit return of result in rax.  Some operations have registers pushed.
  __ ret(0);

  if (use_fp_on_smis.is_linked()) {
    // 6. For some operations emit inline code to perform floating point
    //    operations on known smis (e.g., if the result of the operation
    //    overflowed the smi range).
    __ bind(&use_fp_on_smis);
    if (op == Token::DIV || op == Token::MOD) {
      // Restore left and right to rdx and rax.
      __ movq(rdx, rcx);
      __ movq(rax, rbx);
    }

    if (generate_inline_heapnumber_results) {
      __ AllocateHeapNumber(rcx, rbx, slow);
      Comment perform_float(masm, "-- Perform float operation on smis");
      if (op == Token::SHR) {
        __ SmiToInteger32(left, left);
        __ cvtqsi2sd(xmm0, left);
      } else {
        FloatingPointHelper::LoadSSE2SmiOperands(masm);
        switch (op) {
        case Token::ADD: __ addsd(xmm0, xmm1); break;
        case Token::SUB: __ subsd(xmm0, xmm1); break;
        case Token::MUL: __ mulsd(xmm0, xmm1); break;
        case Token::DIV: __ divsd(xmm0, xmm1); break;
        default: UNREACHABLE();
        }
      }
      __ movsd(FieldOperand(rcx, HeapNumber::kValueOffset), xmm0);
      __ movq(rax, rcx);
      __ ret(0);
    } else {
      __ jmp(&fail);
    }
  }

  // 7. Non-smi operands reach the end of the code generated by
  //    GenerateSmiCode, and fall through to subsequent code,
  //    with the operands in rdx and rax.
  //    But first we check if non-smi values are HeapNumbers holding
  //    values that could be smi.
  __ bind(&not_smis);
  Comment done_comment(masm, "-- Enter non-smi code");
  FloatingPointHelper::ConvertUndefined convert_undefined =
      FloatingPointHelper::BAILOUT_ON_UNDEFINED;
  // This list must be in sync with BinaryOpPatch() behavior in ic.cc.
  if (op == Token::BIT_AND ||
      op == Token::BIT_OR ||
      op == Token::BIT_XOR ||
      op == Token::SAR ||
      op == Token::SHL ||
      op == Token::SHR) {
    convert_undefined = FloatingPointHelper::CONVERT_UNDEFINED_TO_ZERO;
  }
  FloatingPointHelper::NumbersToSmis(masm, left, right, rbx, rdi, rcx,
                                     &smi_values, &fail, convert_undefined);
  __ jmp(&smi_values);
  __ bind(&fail);
}


static void BinaryOpStub_GenerateHeapResultAllocation(MacroAssembler* masm,
                                                      Label* alloc_failure,
                                                      OverwriteMode mode);


static void BinaryOpStub_GenerateFloatingPointCode(MacroAssembler* masm,
                                                   Label* allocation_failure,
                                                   Label* non_numeric_failure,
                                                   Token::Value op,
                                                   OverwriteMode mode) {
  switch (op) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      FloatingPointHelper::LoadSSE2UnknownOperands(masm, non_numeric_failure);

      switch (op) {
        case Token::ADD: __ addsd(xmm0, xmm1); break;
        case Token::SUB: __ subsd(xmm0, xmm1); break;
        case Token::MUL: __ mulsd(xmm0, xmm1); break;
        case Token::DIV: __ divsd(xmm0, xmm1); break;
        default: UNREACHABLE();
      }
      BinaryOpStub_GenerateHeapResultAllocation(
          masm, allocation_failure, mode);
      __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
      __ ret(0);
      break;
    }
    case Token::MOD: {
      // For MOD we jump to the allocation_failure label, to call runtime.
      __ jmp(allocation_failure);
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      Label non_smi_shr_result;
      Register heap_number_map = r9;
      __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      FloatingPointHelper::LoadAsIntegers(masm, non_numeric_failure,
                                          heap_number_map);
      switch (op) {
        case Token::BIT_OR:  __ orl(rax, rcx); break;
        case Token::BIT_AND: __ andl(rax, rcx); break;
        case Token::BIT_XOR: __ xorl(rax, rcx); break;
        case Token::SAR: __ sarl_cl(rax); break;
        case Token::SHL: __ shll_cl(rax); break;
        case Token::SHR: {
          __ shrl_cl(rax);
          // Check if result is negative. This can only happen for a shift
          // by zero.
          __ testl(rax, rax);
          __ j(negative, &non_smi_shr_result);
          break;
        }
        default: UNREACHABLE();
      }
      STATIC_ASSERT(kSmiValueSize == 32);
      // Tag smi result and return.
      __ Integer32ToSmi(rax, rax);
      __ Ret();

      // Logical shift right can produce an unsigned int32 that is not
      // an int32, and so is not in the smi range.  Allocate a heap number
      // in that case.
      if (op == Token::SHR) {
        __ bind(&non_smi_shr_result);
        Label allocation_failed;
        __ movl(rbx, rax);  // rbx holds result value (uint32 value as int64).
        // Allocate heap number in new space.
        // Not using AllocateHeapNumber macro in order to reuse
        // already loaded heap_number_map.
        __ Allocate(HeapNumber::kSize, rax, rdx, no_reg, &allocation_failed,
                    TAG_OBJECT);
        // Set the map.
        __ AssertRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           kHeapNumberMapRegisterClobbered);
        __ movq(FieldOperand(rax, HeapObject::kMapOffset),
                heap_number_map);
        __ cvtqsi2sd(xmm0, rbx);
        __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
        __ Ret();

        __ bind(&allocation_failed);
        // We need tagged values in rdx and rax for the following code,
        // not int32 in rax and rcx.
        __ Integer32ToSmi(rax, rcx);
        __ Integer32ToSmi(rdx, rbx);
        __ jmp(allocation_failure);
      }
      break;
    }
    default: UNREACHABLE(); break;
  }
  // No fall-through from this generated code.
  if (FLAG_debug_code) {
    __ Abort(kUnexpectedFallThroughInBinaryStubGenerateFloatingPointCode);
  }
}


static void BinaryOpStub_GenerateRegisterArgsPushUnderReturn(
    MacroAssembler* masm) {
  // Push arguments, but ensure they are under the return address
  // for a tail call.
  __ PopReturnAddressTo(rcx);
  __ push(rdx);
  __ push(rax);
  __ PushReturnAddressFrom(rcx);
}


void BinaryOpStub::GenerateAddStrings(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  Label left_not_string, call_runtime;

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &left_not_string, Label::kNear);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &left_not_string, Label::kNear);
  StringAddStub string_add_left_stub(
      (StringAddFlags)(STRING_ADD_CHECK_RIGHT | STRING_ADD_ERECT_FRAME));
  BinaryOpStub_GenerateRegisterArgsPushUnderReturn(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ JumpIfSmi(right, &call_runtime, Label::kNear);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime, Label::kNear);

  StringAddStub string_add_right_stub(
      (StringAddFlags)(STRING_ADD_CHECK_LEFT | STRING_ADD_ERECT_FRAME));
  BinaryOpStub_GenerateRegisterArgsPushUnderReturn(masm);
  __ TailCallStub(&string_add_right_stub);

  // Neither argument is a string.
  __ bind(&call_runtime);
}


void BinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  Label right_arg_changed, call_runtime;

  if (op_ == Token::MOD && encoded_right_arg_.has_value) {
    // It is guaranteed that the value will fit into a Smi, because if it
    // didn't, we wouldn't be here, see BinaryOp_Patch.
    __ Cmp(rax, Smi::FromInt(fixed_right_arg_value()));
    __ j(not_equal, &right_arg_changed);
  }

  if (result_type_ == BinaryOpIC::UNINITIALIZED ||
      result_type_ == BinaryOpIC::SMI) {
    // Only allow smi results.
    BinaryOpStub_GenerateSmiCode(masm, NULL, NO_HEAPNUMBER_RESULTS, op_);
  } else {
    // Allow heap number result and don't make a transition if a heap number
    // cannot be allocated.
    BinaryOpStub_GenerateSmiCode(
        masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS, op_);
  }

  // Code falls through if the result is not returned as either a smi or heap
  // number.
  __ bind(&right_arg_changed);
  GenerateTypeTransition(masm);

  if (call_runtime.is_linked()) {
    __ bind(&call_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      GenerateRegisterArgsPush(masm);
      GenerateCallRuntime(masm);
    }
    __ Ret();
  }
}


void BinaryOpStub::GenerateInt32Stub(MacroAssembler* masm) {
  // The int32 case is identical to the Smi case.  We avoid creating this
  // ic state on x64.
  UNREACHABLE();
}


void BinaryOpStub::GenerateBothStringStub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(left_type_ == BinaryOpIC::STRING && right_type_ == BinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  // If both arguments are strings, call the string add stub.
  // Otherwise, do a transition.

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &call_runtime);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime);

  // Test if right operand is a string.
  __ JumpIfSmi(right, &call_runtime);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime);

  StringAddStub string_add_stub(
      (StringAddFlags)(STRING_ADD_CHECK_NONE | STRING_ADD_ERECT_FRAME));
  BinaryOpStub_GenerateRegisterArgsPushUnderReturn(masm);
  __ TailCallStub(&string_add_stub);

  __ bind(&call_runtime);
  GenerateTypeTransition(masm);
}


void BinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  Label call_runtime;

  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateAddStrings(masm);
  }

  // Convert oddball arguments to numbers.
  Label check, done;
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &check, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(rdx, rdx);
  } else {
    __ LoadRoot(rdx, Heap::kNanValueRootIndex);
  }
  __ jmp(&done, Label::kNear);
  __ bind(&check);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &done, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(rax, rax);
  } else {
    __ LoadRoot(rax, Heap::kNanValueRootIndex);
  }
  __ bind(&done);

  GenerateNumberStub(masm);
}


static void BinaryOpStub_CheckSmiInput(MacroAssembler* masm,
                                       Register input,
                                       Label* fail) {
  Label ok;
  __ JumpIfSmi(input, &ok, Label::kNear);
  Register heap_number_map = r8;
  Register scratch1 = r9;
  Register scratch2 = r10;
  // HeapNumbers containing 32bit integer values are also allowed.
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  __ cmpq(FieldOperand(input, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, fail);
  __ movsd(xmm0, FieldOperand(input, HeapNumber::kValueOffset));
  // Convert, convert back, and compare the two doubles' bits.
  __ cvttsd2siq(scratch2, xmm0);
  __ cvtlsi2sd(xmm1, scratch2);
  __ movq(scratch1, xmm0);
  __ movq(scratch2, xmm1);
  __ cmpq(scratch1, scratch2);
  __ j(not_equal, fail);
  __ bind(&ok);
}


void BinaryOpStub::GenerateNumberStub(MacroAssembler* masm) {
  Label gc_required, not_number;

  // It could be that only SMIs have been seen at either the left
  // or the right operand. For precise type feedback, patch the IC
  // again if this changes.
  if (left_type_ == BinaryOpIC::SMI) {
    BinaryOpStub_CheckSmiInput(masm, rdx, &not_number);
  }
  if (right_type_ == BinaryOpIC::SMI) {
    BinaryOpStub_CheckSmiInput(masm, rax, &not_number);
  }

  BinaryOpStub_GenerateFloatingPointCode(
      masm, &gc_required, &not_number, op_, mode_);

  __ bind(&not_number);
  GenerateTypeTransition(masm);

  __ bind(&gc_required);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    GenerateRegisterArgsPush(masm);
    GenerateCallRuntime(masm);
  }
  __ Ret();
}


void BinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime, call_string_add_or_runtime;

  BinaryOpStub_GenerateSmiCode(
      masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS, op_);

  BinaryOpStub_GenerateFloatingPointCode(
      masm, &call_runtime, &call_string_add_or_runtime, op_, mode_);

  __ bind(&call_string_add_or_runtime);
  if (op_ == Token::ADD) {
    GenerateAddStrings(masm);
  }

  __ bind(&call_runtime);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    GenerateRegisterArgsPush(masm);
    GenerateCallRuntime(masm);
  }
  __ Ret();
}


static void BinaryOpStub_GenerateHeapResultAllocation(MacroAssembler* masm,
                                                      Label* alloc_failure,
                                                      OverwriteMode mode) {
  Label skip_allocation;
  switch (mode) {
    case OVERWRITE_LEFT: {
      // If the argument in rdx is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(rdx, &skip_allocation);
      // Allocate a heap number for the result. Keep rax and rdx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(rbx, rcx, alloc_failure);
      // Now rdx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ movq(rdx, rbx);
      __ bind(&skip_allocation);
      // Use object in rdx as a result holder
      __ movq(rax, rdx);
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in rax is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(rax, &skip_allocation);
      // Fall through!
    case NO_OVERWRITE:
      // Allocate a heap number for the result. Keep rax and rdx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(rbx, rcx, alloc_failure);
      // Now rax can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ movq(rax, rbx);
      __ bind(&skip_allocation);
      break;
    default: UNREACHABLE();
  }
}


void BinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ push(rdx);
  __ push(rax);
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // TAGGED case:
  //   Input:
  //     rsp[8] : argument (should be number).
  //     rsp[0] : return address.
  //   Output:
  //     rax: tagged double result.
  // UNTAGGED case:
  //   Input::
  //     rsp[0] : return address.
  //     xmm1   : untagged double input argument
  //   Output:
  //     xmm1   : untagged double result.

  Label runtime_call;
  Label runtime_call_clear_stack;
  Label skip_cache;
  const bool tagged = (argument_type_ == TAGGED);
  if (tagged) {
    Label input_not_smi, loaded;

    // Test that rax is a number.
    StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
    __ movq(rax, args.GetArgumentOperand(0));
    __ JumpIfNotSmi(rax, &input_not_smi, Label::kNear);
    // Input is a smi. Untag and load it onto the FPU stack.
    // Then load the bits of the double into rbx.
    __ SmiToInteger32(rax, rax);
    __ subq(rsp, Immediate(kDoubleSize));
    __ cvtlsi2sd(xmm1, rax);
    __ movsd(Operand(rsp, 0), xmm1);
    __ movq(rbx, xmm1);
    __ movq(rdx, xmm1);
    __ fld_d(Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&loaded, Label::kNear);

    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ LoadRoot(rbx, Heap::kHeapNumberMapRootIndex);
    __ cmpq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
    __ j(not_equal, &runtime_call);
    // Input is a HeapNumber. Push it on the FPU stack and load its
    // bits into rbx.
    __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(rbx, FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(rdx, rbx);

    __ bind(&loaded);
  } else {  // UNTAGGED.
    __ movq(rbx, xmm1);
    __ movq(rdx, xmm1);
  }

  // ST[0] == double value, if TAGGED.
  // rbx = bits of double value.
  // rdx = also bits of double value.
  // Compute hash (h is 32 bits, bits are 64 and the shifts are arithmetic):
  //   h = h0 = bits ^ (bits >> 32);
  //   h ^= h >> 16;
  //   h ^= h >> 8;
  //   h = h & (cacheSize - 1);
  // or h = (h0 ^ (h0 >> 8) ^ (h0 >> 16) ^ (h0 >> 24)) & (cacheSize - 1)
  __ sar(rdx, Immediate(32));
  __ xorl(rdx, rbx);
  __ movl(rcx, rdx);
  __ movl(rax, rdx);
  __ movl(rdi, rdx);
  __ sarl(rdx, Immediate(8));
  __ sarl(rcx, Immediate(16));
  __ sarl(rax, Immediate(24));
  __ xorl(rcx, rdx);
  __ xorl(rax, rdi);
  __ xorl(rcx, rax);
  ASSERT(IsPowerOf2(TranscendentalCache::SubCache::kCacheSize));
  __ andl(rcx, Immediate(TranscendentalCache::SubCache::kCacheSize - 1));

  // ST[0] == double value.
  // rbx = bits of double value.
  // rcx = TranscendentalCache::hash(double value).
  ExternalReference cache_array =
      ExternalReference::transcendental_cache_array_address(masm->isolate());
  __ movq(rax, cache_array);
  int cache_array_index =
      type_ * sizeof(masm->isolate()->transcendental_cache()->caches_[0]);
  __ movq(rax, Operand(rax, cache_array_index));
  // rax points to the cache for the type type_.
  // If NULL, the cache hasn't been initialized yet, so go through runtime.
  __ testq(rax, rax);
  __ j(zero, &runtime_call_clear_stack);  // Only clears stack if TAGGED.
#ifdef DEBUG
  // Check that the layout of cache elements match expectations.
  {  // NOLINT - doesn't like a single brace on a line.
    TranscendentalCache::SubCache::Element test_elem[2];
    char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
    char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
    char* elem_in0  = reinterpret_cast<char*>(&(test_elem[0].in[0]));
    char* elem_in1  = reinterpret_cast<char*>(&(test_elem[0].in[1]));
    char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
    // Two uint_32's and a pointer per element.
    CHECK_EQ(2 * kIntSize + 1 * kPointerSize,
             static_cast<int>(elem2_start - elem_start));
    CHECK_EQ(0, static_cast<int>(elem_in0 - elem_start));
    CHECK_EQ(kIntSize, static_cast<int>(elem_in1 - elem_start));
    CHECK_EQ(2 * kIntSize, static_cast<int>(elem_out - elem_start));
  }
#endif
  // Find the address of the rcx'th entry in the cache, i.e., &rax[rcx*16].
  __ addl(rcx, rcx);
  __ lea(rcx, Operand(rax, rcx, times_8, 0));
  // Check if cache matches: Double value is stored in uint32_t[2] array.
  Label cache_miss;
  __ cmpq(rbx, Operand(rcx, 0));
  __ j(not_equal, &cache_miss, Label::kNear);
  // Cache hit!
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->transcendental_cache_hit(), 1);
  __ movq(rax, Operand(rcx, 2 * kIntSize));
  if (tagged) {
    __ fstp(0);  // Clear FPU stack.
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();
  }

  __ bind(&cache_miss);
  __ IncrementCounter(counters->transcendental_cache_miss(), 1);
  // Update cache with new value.
  if (tagged) {
  __ AllocateHeapNumber(rax, rdi, &runtime_call_clear_stack);
  } else {  // UNTAGGED.
    __ AllocateHeapNumber(rax, rdi, &skip_cache);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm1);
    __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
  }
  GenerateOperation(masm, type_);
  __ movq(Operand(rcx, 0), rbx);
  __ movq(Operand(rcx, 2 * kIntSize), rax);
  __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
  if (tagged) {
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();

    // Skip cache and return answer directly, only in untagged case.
    __ bind(&skip_cache);
    __ subq(rsp, Immediate(kDoubleSize));
    __ movsd(Operand(rsp, 0), xmm1);
    __ fld_d(Operand(rsp, 0));
    GenerateOperation(masm, type_);
    __ fstp_d(Operand(rsp, 0));
    __ movsd(xmm1, Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    // We return the value in xmm1 without adding it to the cache, but
    // we cause a scavenging GC so that future allocations will succeed.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      // Allocate an unused object bigger than a HeapNumber.
      __ Push(Smi::FromInt(2 * kDoubleSize));
      __ CallRuntimeSaveDoubles(Runtime::kAllocateInNewSpace);
    }
    __ Ret();
  }

  // Call runtime, doing whatever allocation and cleanup is necessary.
  if (tagged) {
    __ bind(&runtime_call_clear_stack);
    __ fstp(0);
    __ bind(&runtime_call);
    __ TailCallExternalReference(
        ExternalReference(RuntimeFunction(), masm->isolate()), 1, 1);
  } else {  // UNTAGGED.
    __ bind(&runtime_call_clear_stack);
    __ bind(&runtime_call);
    __ AllocateHeapNumber(rax, rdi, &skip_cache);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm1);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(rax);
      __ CallRuntime(RuntimeFunction(), 1);
    }
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();
  }
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    // Add more cases when necessary.
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
  // Registers:
  // rax: Newly allocated HeapNumber, which must be preserved.
  // rbx: Bits of input double. Must be preserved.
  // rcx: Pointer to cache entry. Must be preserved.
  // st(0): Input double
  Label done;
  if (type == TranscendentalCache::SIN ||
      type == TranscendentalCache::COS ||
      type == TranscendentalCache::TAN) {
    // Both fsin and fcos require arguments in the range +/-2^63 and
    // return NaN for infinities and NaN. They can share all code except
    // the actual fsin/fcos operation.
    Label in_range;
    // If argument is outside the range -2^63..2^63, fsin/cos doesn't
    // work. We must reduce it to the appropriate range.
    __ movq(rdi, rbx);
    // Move exponent and sign bits to low bits.
    __ shr(rdi, Immediate(HeapNumber::kMantissaBits));
    // Remove sign bit.
    __ andl(rdi, Immediate((1 << HeapNumber::kExponentBits) - 1));
    int supported_exponent_limit = (63 + HeapNumber::kExponentBias);
    __ cmpl(rdi, Immediate(supported_exponent_limit));
    __ j(below, &in_range);
    // Check for infinity and NaN. Both return NaN for sin.
    __ cmpl(rdi, Immediate(0x7ff));
    Label non_nan_result;
    __ j(not_equal, &non_nan_result, Label::kNear);
    // Input is +/-Infinity or NaN. Result is NaN.
    __ fstp(0);
    // NaN is represented by 0x7ff8000000000000.
    __ subq(rsp, Immediate(kPointerSize));
    __ movl(Operand(rsp, 4), Immediate(0x7ff80000));
    __ movl(Operand(rsp, 0), Immediate(0x00000000));
    __ fld_d(Operand(rsp, 0));
    __ addq(rsp, Immediate(kPointerSize));
    __ jmp(&done);

    __ bind(&non_nan_result);

    // Use fpmod to restrict argument to the range +/-2*PI.
    __ movq(rdi, rax);  // Save rax before using fnstsw_ax.
    __ fldpi();
    __ fadd(0);
    __ fld(1);
    // FPU Stack: input, 2*pi, input.
    {
      Label no_exceptions;
      __ fwait();
      __ fnstsw_ax();
      // Clear if Illegal Operand or Zero Division exceptions are set.
      __ testl(rax, Immediate(5));  // #IO and #ZD flags of FPU status word.
      __ j(zero, &no_exceptions);
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
      __ testl(rax, Immediate(0x400));  // Check C2 bit of FPU status word.
      // If C2 is set, computation only has partial result. Loop to
      // continue computation.
      __ j(not_zero, &partial_remainder_loop);
  }
    // FPU Stack: input, 2*pi, input % 2*pi
    __ fstp(2);
    // FPU Stack: input % 2*pi, 2*pi,
    __ fstp(0);
    // FPU Stack: input % 2*pi
    __ movq(rax, rdi);  // Restore rax, pointer to the new HeapNumber.
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


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
// Jump to conversion_failure: rdx and rax are unchanged.
void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         Label* conversion_failure,
                                         Register heap_number_map) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  __ JumpIfNotSmi(rdx, &arg1_is_object);
  __ SmiToInteger32(r8, rdx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ Set(r8, 0);
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ cmpq(FieldOperand(rdx, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg1);
  // Get the untagged integer version of the rdx heap number in r8.
  __ TruncateHeapNumberToI(r8, rdx);

  // Here r8 has the untagged integer, rax has a Smi or a heap number.
  __ bind(&load_arg2);
  // Test if arg2 is a Smi.
  __ JumpIfNotSmi(rax, &arg2_is_object);
  __ SmiToInteger32(rcx, rax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ Set(rcx, 0);
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ cmpq(FieldOperand(rax, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the rax heap number in rcx.
  __ TruncateHeapNumberToI(rcx, rax);

  __ bind(&done);
  __ movl(rax, r8);
}


void FloatingPointHelper::LoadSSE2SmiOperands(MacroAssembler* masm) {
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);
}


void FloatingPointHelper::LoadSSE2UnknownOperands(MacroAssembler* masm,
                                                  Label* not_numbers) {
  Label load_smi_rdx, load_nonsmi_rax, load_smi_rax, load_float_rax, done;
  // Load operand in rdx into xmm0, or branch to not_numbers.
  __ LoadRoot(rcx, Heap::kHeapNumberMapRootIndex);
  __ JumpIfSmi(rdx, &load_smi_rdx);
  __ cmpq(FieldOperand(rdx, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);  // Argument in rdx is not a number.
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  // Load operand in rax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(rax, &load_smi_rax);

  __ bind(&load_nonsmi_rax);
  __ cmpq(FieldOperand(rax, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);
  __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_rdx);
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ JumpIfNotSmi(rax, &load_nonsmi_rax);

  __ bind(&load_smi_rax);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);
  __ bind(&done);
}


void FloatingPointHelper::NumbersToSmis(MacroAssembler* masm,
                                        Register first,
                                        Register second,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Label* on_success,
                                        Label* on_not_smis,
                                        ConvertUndefined convert_undefined) {
  Register heap_number_map = scratch3;
  Register smi_result = scratch1;
  Label done, maybe_undefined_first, maybe_undefined_second, first_done;

  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  Label first_smi;
  __ JumpIfSmi(first, &first_smi, Label::kNear);
  __ cmpq(FieldOperand(first, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal,
       (convert_undefined == CONVERT_UNDEFINED_TO_ZERO)
           ? &maybe_undefined_first
           : on_not_smis);
  // Convert HeapNumber to smi if possible.
  __ movsd(xmm0, FieldOperand(first, HeapNumber::kValueOffset));
  __ movq(scratch2, xmm0);
  __ cvttsd2siq(smi_result, xmm0);
  // Check if conversion was successful by converting back and
  // comparing to the original double's bits.
  __ cvtlsi2sd(xmm1, smi_result);
  __ movq(kScratchRegister, xmm1);
  __ cmpq(scratch2, kScratchRegister);
  __ j(not_equal, on_not_smis);
  __ Integer32ToSmi(first, smi_result);

  __ bind(&first_done);
  __ JumpIfSmi(second, (on_success != NULL) ? on_success : &done);
  __ bind(&first_smi);
  __ AssertNotSmi(second);
  __ cmpq(FieldOperand(second, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal,
       (convert_undefined == CONVERT_UNDEFINED_TO_ZERO)
           ? &maybe_undefined_second
           : on_not_smis);
  // Convert second to smi, if possible.
  __ movsd(xmm0, FieldOperand(second, HeapNumber::kValueOffset));
  __ movq(scratch2, xmm0);
  __ cvttsd2siq(smi_result, xmm0);
  __ cvtlsi2sd(xmm1, smi_result);
  __ movq(kScratchRegister, xmm1);
  __ cmpq(scratch2, kScratchRegister);
  __ j(not_equal, on_not_smis);
  __ Integer32ToSmi(second, smi_result);
  if (on_success != NULL) {
    __ jmp(on_success);
  } else {
    __ jmp(&done);
  }

  __ bind(&maybe_undefined_first);
  __ CompareRoot(first, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, on_not_smis);
  __ xor_(first, first);
  __ jmp(&first_done);

  __ bind(&maybe_undefined_second);
  __ CompareRoot(second, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, on_not_smis);
  __ xor_(second, second);
  if (on_success != NULL) {
    __ jmp(on_success);
  }
  // Else: fall through.

  __ bind(&done);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register exponent = rdx;
  const Register base = rax;
  const Register scratch = rcx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ movq(scratch, Immediate(1));
  __ cvtlsi2sd(double_result, scratch);

  if (exponent_type_ == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack.
    StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
    __ movq(base, args.GetArgumentOperand(0));
    __ movq(exponent, args.GetArgumentOperand(1));
    __ JumpIfSmi(base, &base_is_smi, Label::kNear);
    __ CompareRoot(FieldOperand(base, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);

    __ movsd(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiToInteger32(base, base);
    __ cvtlsi2sd(double_base, base);
    __ bind(&unpack_exponent);

    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ CompareRoot(FieldOperand(exponent, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);
    __ movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type_ == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type_ != INTEGER) {
    Label fast_power, try_arithmetic_simplification;
    // Detect integer exponents stored as double.
    __ DoubleToI(exponent, double_exponent, double_scratch,
                 TREAT_MINUS_ZERO_AS_ZERO, &try_arithmetic_simplification);
    __ jmp(&int_exponent);

    __ bind(&try_arithmetic_simplification);
    __ cvttsd2si(exponent, double_exponent);
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cmpl(exponent, Immediate(0x80000000u));
    __ j(equal, &call_runtime);

    if (exponent_type_ == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label continue_sqrt, continue_rsqrt, not_plus_half;
      // Test for 0.5.
      // Load double_scratch with 0.5.
      __ movq(scratch, V8_UINT64_C(0x3FE0000000000000), RelocInfo::NONE64);
      __ movq(double_scratch, scratch);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000), RelocInfo::NONE64);
      __ movq(double_scratch, scratch);
      __ ucomisd(double_scratch, double_base);
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
      __ addsd(double_scratch, double_base);  // Convert -0 to 0.
      __ sqrtsd(double_result, double_scratch);
      __ jmp(&done);

      // Test for -0.5.
      __ bind(&not_plus_half);
      // Load double_scratch with -0.5 by substracting 1.
      __ subsd(double_scratch, double_result);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &fast_power, Label::kNear);

      // Calculates reciprocal of square root of base.  Check for the special
      // case of Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000), RelocInfo::NONE64);
      __ movq(double_scratch, scratch);
      __ ucomisd(double_scratch, double_base);
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
    __ subq(rsp, Immediate(kDoubleSize));
    __ movsd(Operand(rsp, 0), double_exponent);
    __ fld_d(Operand(rsp, 0));  // E
    __ movsd(Operand(rsp, 0), double_base);
    __ fld_d(Operand(rsp, 0));  // B, E

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
    __ fstp(1);
    // Bail out to runtime in case of exceptions in the status word.
    __ fnstsw_ax();
    __ testb(rax, Immediate(0x5F));  // Check for all but precision exception.
    __ j(not_zero, &fast_power_failed, Label::kNear);
    __ fstp_d(Operand(rsp, 0));
    __ movsd(double_result, Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&done);

    __ bind(&fast_power_failed);
    __ fninit();
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&call_runtime);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  // Back up exponent as we need to check if exponent is negative later.
  __ movq(scratch, exponent);  // Back up exponent.
  __ movsd(double_scratch, double_base);  // Back up base.
  __ movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, while_false;
  __ testl(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ negl(scratch);
  __ bind(&no_neg);

  __ j(zero, &while_false, Label::kNear);
  __ shrl(scratch, Immediate(1));
  // Above condition means CF==0 && ZF==0.  This means that the
  // bit that has been shifted out is 0 and the result is not 0.
  __ j(above, &while_true, Label::kNear);
  __ movsd(double_result, double_scratch);
  __ j(zero, &while_false, Label::kNear);

  __ bind(&while_true);
  __ shrl(scratch, Immediate(1));
  __ mulsd(double_scratch, double_scratch);
  __ j(above, &while_true, Label::kNear);
  __ mulsd(double_result, double_scratch);
  __ j(not_zero, &while_true);

  __ bind(&while_false);
  // If the exponent is negative, return 1/result.
  __ testl(exponent, exponent);
  __ j(greater, &done);
  __ divsd(double_scratch2, double_result);
  __ movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ xorps(double_scratch2, double_scratch2);
  __ ucomisd(double_scratch2, double_result);
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // input was a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ cvtlsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  Counters* counters = masm->isolate()->counters();
  if (exponent_type_ == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMath_pow_cfunction, 2, 1);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in rax.
    __ bind(&done);
    __ AllocateHeapNumber(rax, rcx, &call_runtime);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), double_result);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    // Move base to the correct argument register.  Exponent is already in xmm1.
    __ movsd(xmm0, double_base);
    ASSERT(double_exponent.is(xmm1));
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(2);
      __ CallCFunction(
          ExternalReference::power_double_double_function(masm->isolate()), 2);
    }
    // Return value is in xmm0.
    __ movsd(double_result, xmm0);
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    __ bind(&done);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(0);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver;
  if (kind() == Code::KEYED_LOAD_IC) {
    // ----------- S t a t e -------------
    //  -- rax    : key
    //  -- rdx    : receiver
    //  -- rsp[0] : return address
    // -----------------------------------
    __ Cmp(rax, masm->isolate()->factory()->prototype_string());
    __ j(not_equal, &miss);
    receiver = rdx;
  } else {
    ASSERT(kind() == Code::LOAD_IC);
    // ----------- S t a t e -------------
    //  -- rax    : receiver
    //  -- rcx    : name
    //  -- rsp[0] : return address
    // -----------------------------------
    receiver = rax;
  }

  StubCompiler::GenerateLoadFunctionPrototype(masm, receiver, r8, r9, &miss);
  __ bind(&miss);
  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
}


void StringLengthStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver;
  if (kind() == Code::KEYED_LOAD_IC) {
    // ----------- S t a t e -------------
    //  -- rax    : key
    //  -- rdx    : receiver
    //  -- rsp[0] : return address
    // -----------------------------------
    __ Cmp(rax, masm->isolate()->factory()->length_string());
    __ j(not_equal, &miss);
    receiver = rdx;
  } else {
    ASSERT(kind() == Code::LOAD_IC);
    // ----------- S t a t e -------------
    //  -- rax    : receiver
    //  -- rcx    : name
    //  -- rsp[0] : return address
    // -----------------------------------
    receiver = rax;
  }

  StubCompiler::GenerateLoadStringLength(masm, receiver, r8, r9, &miss,
                                         support_wrapper_);
  __ bind(&miss);
  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
}


void StoreArrayLengthStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  //
  // This accepts as a receiver anything JSArray::SetElementsLength accepts
  // (currently anything except for external arrays which means anything with
  // elements of FixedArray type).  Value must be a number, but only smis are
  // accepted as the most common case.

  Label miss;

  Register receiver = rdx;
  Register value = rax;
  Register scratch = rbx;
  if (kind() == Code::KEYED_STORE_IC) {
    __ Cmp(rcx, masm->isolate()->factory()->length_string());
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
  __ movq(scratch, FieldOperand(receiver, JSArray::kElementsOffset));
  __ CmpObjectType(scratch, FIXED_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that the array has fast properties, otherwise the length
  // property might have been redefined.
  __ movq(scratch, FieldOperand(receiver, JSArray::kPropertiesOffset));
  __ CompareRoot(FieldOperand(scratch, FixedArray::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(equal, &miss);

  // Check that value is a smi.
  __ JumpIfNotSmi(value, &miss);

  // Prepare tail call to StoreIC_ArrayLength.
  __ PopReturnAddressTo(scratch);
  __ push(receiver);
  __ push(value);
  __ PushReturnAddressFrom(scratch);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kStoreIC_ArrayLength), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&miss);

  StubCompiler::TailCallBuiltin(
      masm, BaseLoadStoreStubCompiler::MissBuiltin(kind()));
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in rdx and the parameter count is in rax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(rdx, &slow);

  // Check if the calling frame is an arguments adaptor frame.  We look at the
  // context offset, and if the frame is not a regular one, then we find a
  // Smi instead of the context.  We can't use SmiCompare here, because that
  // only works for comparing two smis.
  Label adaptor;
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register rax. Use unsigned comparison to get negative
  // check for free.
  __ cmpq(rdx, rax);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ lea(rbx, Operand(rbp, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ movq(rcx, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmpq(rdx, rcx);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  index = masm->SmiToIndex(rax, rcx, kPointerSizeLog2);
  __ lea(rbx, Operand(rbx, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ PopReturnAddressTo(rbx);
  __ push(rdx);
  __ PushReturnAddressFrom(rbx);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictFast(MacroAssembler* masm) {
  // Stack layout:
  //  rsp[0]  : return address
  //  rsp[8]  : number of parameters (tagged)
  //  rsp[16] : receiver displacement
  //  rsp[24] : function
  // Registers used over the whole function:
  //  rbx: the mapped parameter count (untagged)
  //  rax: the allocated object (tagged).

  Factory* factory = masm->isolate()->factory();

  StackArgumentsAccessor args(rsp, 3, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ SmiToInteger64(rbx, args.GetArgumentOperand(2));
  // rbx = parameter count (untagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ movq(rcx, rbx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ SmiToInteger64(rcx,
                    Operand(rdx,
                            ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));
  __ movq(args.GetArgumentOperand(1), rdx);

  // rbx = parameter count (untagged)
  // rcx = argument count (untagged)
  // Compute the mapped parameter count = min(rbx, rcx) in rbx.
  __ cmpq(rbx, rcx);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ movq(rbx, rcx);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ xor_(r8, r8);
  __ testq(rbx, rbx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ lea(r8, Operand(rbx, times_pointer_size, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ lea(r8, Operand(r8, rcx, times_pointer_size, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ addq(r8, Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r8, rax, rdx, rdi, &runtime, TAG_OBJECT);

  // rax = address of new object(s) (tagged)
  // rcx = argument count (untagged)
  // Get the arguments boilerplate from the current native context into rdi.
  Label has_mapped_parameters, copy;
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kNativeContextOffset));
  __ testq(rbx, rbx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);

  const int kIndex = Context::ARGUMENTS_BOILERPLATE_INDEX;
  __ movq(rdi, Operand(rdi, Context::SlotOffset(kIndex)));
  __ jmp(&copy, Label::kNear);

  const int kAliasedIndex = Context::ALIASED_ARGUMENTS_BOILERPLATE_INDEX;
  __ bind(&has_mapped_parameters);
  __ movq(rdi, Operand(rdi, Context::SlotOffset(kAliasedIndex)));
  __ bind(&copy);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // rcx = argument count (untagged)
  // rdi = address of boilerplate object (tagged)
  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ movq(rdx, FieldOperand(rdi, i));
    __ movq(FieldOperand(rax, i), rdx);
  }

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ movq(rdx, args.GetArgumentOperand(0));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsCalleeIndex * kPointerSize),
          rdx);

  // Use the length (smi tagged) and set that as an in-object property too.
  // Note: rcx is tagged from here on.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ Integer32ToSmi(rcx, rcx);
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsLengthIndex * kPointerSize),
          rcx);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, edi will point there, otherwise to the
  // backing store.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // rcx = argument count (tagged)
  // rdi = address of parameter map or backing store (tagged)

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ testq(rbx, rbx);
  __ j(zero, &skip_parameter_map);

  __ LoadRoot(kScratchRegister, Heap::kNonStrictArgumentsElementsMapRootIndex);
  // rbx contains the untagged argument count. Add 2 and tag to write.
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);
  __ Integer64PlusConstantToSmi(r9, rbx, 2);
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), r9);
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize + 0 * kPointerSize), rsi);
  __ lea(r9, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize + 1 * kPointerSize), r9);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;

  // Load tagged parameter count into r9.
  __ Integer32ToSmi(r9, rbx);
  __ Move(r8, Smi::FromInt(Context::MIN_CONTEXT_SLOTS));
  __ addq(r8, args.GetArgumentOperand(2));
  __ subq(r8, r9);
  __ Move(r11, factory->the_hole_value());
  __ movq(rdx, rdi);
  __ lea(rdi, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  // r9 = loop variable (tagged)
  // r8 = mapping index (tagged)
  // r11 = the hole value
  // rdx = address of parameter map (tagged)
  // rdi = address of backing store (tagged)
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ SmiSubConstant(r9, r9, Smi::FromInt(1));
  __ SmiToInteger64(kScratchRegister, r9);
  __ movq(FieldOperand(rdx, kScratchRegister,
                       times_pointer_size,
                       kParameterMapHeaderSize),
          r8);
  __ movq(FieldOperand(rdi, kScratchRegister,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r11);
  __ SmiAddConstant(r8, r8, Smi::FromInt(1));
  __ bind(&parameters_test);
  __ SmiTest(r9);
  __ j(not_zero, &parameters_loop, Label::kNear);

  __ bind(&skip_parameter_map);

  // rcx = argument count (tagged)
  // rdi = address of backing store (tagged)
  // Copy arguments header and remaining slots (if there are any).
  __ Move(FieldOperand(rdi, FixedArray::kMapOffset),
          factory->fixed_array_map());
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);

  Label arguments_loop, arguments_test;
  __ movq(r8, rbx);
  __ movq(rdx, args.GetArgumentOperand(1));
  // Untag rcx for the loop below.
  __ SmiToInteger64(rcx, rcx);
  __ lea(kScratchRegister, Operand(r8, times_pointer_size, 0));
  __ subq(rdx, kScratchRegister);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ subq(rdx, Immediate(kPointerSize));
  __ movq(r9, Operand(rdx, 0));
  __ movq(FieldOperand(rdi, r8,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r9);
  __ addq(r8, Immediate(1));

  __ bind(&arguments_test);
  __ cmpq(r8, rcx);
  __ j(less, &arguments_loop, Label::kNear);

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  // rcx = argument count (untagged)
  __ bind(&runtime);
  __ Integer32ToSmi(rcx, rcx);
  __ movq(args.GetArgumentOperand(2), rcx);  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictSlow(MacroAssembler* masm) {
  // rsp[0]  : return address
  // rsp[8]  : number of parameters
  // rsp[16] : receiver displacement
  // rsp[24] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &runtime);

  // Patch the arguments.length and the parameters pointer.
  StackArgumentsAccessor args(rsp, 3, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rcx, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ movq(args.GetArgumentOperand(2), rcx);
  __ SmiToInteger64(rcx, rcx);
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
              StandardFrameConstants::kCallerSPOffset));
  __ movq(args.GetArgumentOperand(1), rdx);

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // rsp[0]  : return address
  // rsp[8]  : number of parameters
  // rsp[16] : receiver displacement
  // rsp[24] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  StackArgumentsAccessor args(rsp, 3, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rcx, args.GetArgumentOperand(2));
  __ SmiToInteger64(rcx, rcx);
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ movq(rcx, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ movq(args.GetArgumentOperand(2), rcx);
  __ SmiToInteger64(rcx, rcx);
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));
  __ movq(args.GetArgumentOperand(1), rdx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ testq(rcx, rcx);
  __ j(zero, &add_arguments_object, Label::kNear);
  __ lea(rcx, Operand(rcx, times_pointer_size, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ addq(rcx, Immediate(Heap::kArgumentsObjectSizeStrict));

  // Do the allocation of both objects in one go.
  __ Allocate(rcx, rax, rdx, rbx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current native context.
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kNativeContextOffset));
  const int offset =
      Context::SlotOffset(Context::STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX);
  __ movq(rdi, Operand(rdi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ movq(rbx, FieldOperand(rdi, i));
    __ movq(FieldOperand(rax, i), rbx);
  }

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ movq(rcx, args.GetArgumentOperand(2));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsLengthIndex * kPointerSize),
          rcx);

  // If there are no actual arguments, we're done.
  Label done;
  __ testq(rcx, rcx);
  __ j(zero, &done);

  // Get the parameters pointer from the stack.
  __ movq(rdx, args.GetArgumentOperand(1));

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSizeStrict));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);


  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);
  // Untag the length for the loop below.
  __ SmiToInteger64(rcx, rcx);

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ movq(rbx, Operand(rdx, -1 * kPointerSize));  // Skip receiver.
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize), rbx);
  __ addq(rdi, Immediate(kPointerSize));
  __ subq(rdx, Immediate(kPointerSize));
  __ decq(rcx);
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
  //  rsp[0]  : return address
  //  rsp[8]  : last_match_info (expected JSArray)
  //  rsp[16] : previous index
  //  rsp[24] : subject string
  //  rsp[32] : JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;
  // Ensure that a RegExp stack is allocated.
  Isolate* isolate = masm->isolate();
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate);
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate);
  __ Load(kScratchRegister, address_of_regexp_stack_memory_size);
  __ testq(kScratchRegister, kScratchRegister);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ movq(rax, FieldOperand(rax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    Condition is_smi = masm->CheckSmi(rax);
    __ Check(NegateCondition(is_smi),
        kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CmpObjectType(rax, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Check(equal, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // rax: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ SmiToInteger32(rbx, FieldOperand(rax, JSRegExp::kDataTagOffset));
  __ cmpl(rbx, Immediate(JSRegExp::IRREGEXP));
  __ j(not_equal, &runtime);

  // rax: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ SmiToInteger32(rdx,
                    FieldOperand(rax, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or              number_of_captures <= offsets vector size / 2 - 1
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmpl(rdx, Immediate(Isolate::kJSRegexpStaticOffsetsVectorSize / 2 - 1));
  __ j(above, &runtime);

  // Reset offset for possibly sliced string.
  __ Set(r14, 0);
  __ movq(rdi, Operand(rsp, kSubjectOffset));
  __ JumpIfSmi(rdi, &runtime);
  __ movq(r15, rdi);  // Make a copy of the original subject string.
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  // rax: RegExp data (FixedArray)
  // rdi: subject string
  // r15: subject string
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
  __ andb(rbx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kStringEncodingMask |
                         kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).

  // (2) Sequential one byte?  If yes, go to (6).
  // Any other sequential string must be one byte.
  __ andb(rbx, Immediate(kIsNotStringMask |
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
  __ cmpq(rbx, Immediate(kExternalStringTag));
  __ j(greater_equal, &not_seq_nor_cons);  // Go to (7).

  // (4) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ CompareRoot(FieldOperand(rdi, ConsString::kSecondOffset),
                 Heap::kempty_stringRootIndex);
  __ j(not_equal, &runtime);
  __ movq(rdi, FieldOperand(rdi, ConsString::kFirstOffset));
  __ bind(&check_underlying);
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movq(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));

  // (5a) Is subject sequential two byte?  If yes, go to (9).
  __ testb(rbx, Immediate(kStringRepresentationMask | kStringEncodingMask));
  STATIC_ASSERT((kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).
  // (5b) Is subject external?  If yes, go to (8).
  __ testb(rbx, Immediate(kStringRepresentationMask));
  // The underlying external string is never a short external string.
  STATIC_CHECK(ExternalString::kMaxShortLength < ConsString::kMinLength);
  STATIC_CHECK(ExternalString::kMaxShortLength < SlicedString::kMinLength);
  __ j(not_zero, &external_string);  // Go to (8)

  // (6) One byte sequential.  Load regexp code for one byte.
  __ bind(&seq_one_byte_string);
  // rax: RegExp data (FixedArray)
  __ movq(r11, FieldOperand(rax, JSRegExp::kDataAsciiCodeOffset));
  __ Set(rcx, 1);  // Type is one byte.

  // (E) Carry on.  String handling is done.
  __ bind(&check_code);
  // r11: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // smi (code flushing support)
  __ JumpIfSmi(r11, &runtime);

  // rdi: sequential subject string (or look-alike, external string)
  // r15: original subject string
  // rcx: encoding of subject string (1 if ASCII, 0 if two_byte);
  // r11: code
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  // We have to use r15 instead of rdi to load the length because rdi might
  // have been only made to look like a sequential string when it actually
  // is an external string.
  __ movq(rbx, Operand(rsp, kPreviousIndexOffset));
  __ JumpIfNotSmi(rbx, &runtime);
  __ SmiCompare(rbx, FieldOperand(r15, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ SmiToInteger64(rbx, rbx);

  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if ASCII 0 if two_byte);
  // r11: code
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 9;
  int argument_slots_on_stack =
      masm->ArgumentStackSlotsForCFunctionCall(kRegExpExecuteArguments);
  __ EnterApiExitFrame(argument_slots_on_stack);

  // Argument 9: Pass current isolate address.
  __ LoadAddress(kScratchRegister,
                 ExternalReference::isolate_address(masm->isolate()));
  __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kPointerSize),
          kScratchRegister);

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ movq(Operand(rsp, (argument_slots_on_stack - 2) * kPointerSize),
          Immediate(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ movq(kScratchRegister, address_of_regexp_stack_memory_address);
  __ movq(r9, Operand(kScratchRegister, 0));
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ addq(r9, Operand(kScratchRegister, 0));
  __ movq(Operand(rsp, (argument_slots_on_stack - 3) * kPointerSize), r9);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  // Argument 6 is passed in r9 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 4) * kPointerSize),
          Immediate(0));
#else
  __ Set(r9, 0);
#endif

  // Argument 5: static offsets vector buffer.
  __ LoadAddress(r8,
                 ExternalReference::address_of_static_offsets_vector(isolate));
  // Argument 5 passed in r8 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 5) * kPointerSize), r8);
#endif

  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if ASCII 0 if two_byte);
  // r11: code
  // r14: slice offset
  // r15: original subject string

  // Argument 2: Previous index.
  __ movq(arg_reg_2, rbx);

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label setup_two_byte, setup_rest, got_length, length_not_from_slice;
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ addq(rbx, r14);
  __ SmiToInteger32(arg_reg_3, FieldOperand(r15, String::kLengthOffset));
  __ addq(r14, arg_reg_3);  // Using arg3 as scratch.

  // rbx: start index of the input
  // r14: end index of the input
  // r15: original subject string
  __ testb(rcx, rcx);  // Last use of rcx as encoding of subject string.
  __ j(zero, &setup_two_byte, Label::kNear);
  __ lea(arg_reg_4,
         FieldOperand(rdi, r14, times_1, SeqOneByteString::kHeaderSize));
  __ lea(arg_reg_3,
         FieldOperand(rdi, rbx, times_1, SeqOneByteString::kHeaderSize));
  __ jmp(&setup_rest, Label::kNear);
  __ bind(&setup_two_byte);
  __ lea(arg_reg_4,
         FieldOperand(rdi, r14, times_2, SeqTwoByteString::kHeaderSize));
  __ lea(arg_reg_3,
         FieldOperand(rdi, rbx, times_2, SeqTwoByteString::kHeaderSize));
  __ bind(&setup_rest);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use rbp, which points exactly to one pointer size below the previous rsp.
  // (Because creating a new stack frame pushes the previous rbp onto the stack
  // and thereby moves up rsp by one kPointerSize.)
  __ movq(arg_reg_1, r15);

  // Locate the code entry and call it.
  __ addq(r11, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(r11);

  __ LeaveApiExitFrame();

  // Check the result.
  Label success;
  Label exception;
  __ cmpl(rax, Immediate(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ j(equal, &success, Label::kNear);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::EXCEPTION));
  __ j(equal, &exception);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::FAILURE));
  // If none of the above, it can only be retry.
  // Handle that in the runtime system.
  __ j(not_equal, &runtime);

  // For failure return null.
  __ LoadRoot(rax, Heap::kNullValueRootIndex);
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ movq(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  __ SmiToInteger32(rax,
                    FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ leal(rdx, Operand(rax, rax, times_1, 2));

  // rdx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ movq(r15, Operand(rsp, kLastMatchInfoOffset));
  __ JumpIfSmi(r15, &runtime);
  __ CmpObjectType(r15, JS_ARRAY_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ movq(rbx, FieldOperand(r15, JSArray::kElementsOffset));
  __ movq(rax, FieldOperand(rbx, HeapObject::kMapOffset));
  __ CompareRoot(rax, Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information. Ensure no overflow in add.
  STATIC_ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
  __ SmiToInteger32(rax, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ subl(rax, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmpl(rdx, rax);
  __ j(greater, &runtime);

  // rbx: last_match_info backing store (FixedArray)
  // rdx: number of capture registers
  // Store the capture count.
  __ Integer32ToSmi(kScratchRegister, rdx);
  __ movq(FieldOperand(rbx, RegExpImpl::kLastCaptureCountOffset),
          kScratchRegister);
  // Store last subject and last input.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastSubjectOffset), rax);
  __ movq(rcx, rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastSubjectOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);
  __ movq(rax, rcx);
  __ movq(FieldOperand(rbx, RegExpImpl::kLastInputOffset), rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastInputOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  __ LoadAddress(rcx,
                 ExternalReference::address_of_static_offsets_vector(isolate));

  // rbx: last_match_info backing store (FixedArray)
  // rcx: offsets vector
  // rdx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ subq(rdx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer and make it a smi.
  __ movl(rdi, Operand(rcx, rdx, times_int_size, 0));
  __ Integer32ToSmi(rdi, rdi);
  // Store the smi value in the last match info.
  __ movq(FieldOperand(rbx,
                       rdx,
                       times_pointer_size,
                       RegExpImpl::kFirstCaptureOffset),
          rdi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ movq(rax, r15);
  __ ret(4 * kPointerSize);

  __ bind(&exception);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, isolate);
  Operand pending_exception_operand =
      masm->ExternalOperand(pending_exception_address, rbx);
  __ movq(rax, pending_exception_operand);
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ cmpq(rax, rdx);
  __ j(equal, &runtime);
  __ movq(pending_exception_operand, rdx);

  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  Label termination_exception;
  __ j(equal, &termination_exception, Label::kNear);
  __ Throw(rax);

  __ bind(&termination_exception);
  __ ThrowUncatchable(rax);

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
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ testb(rbx, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  __ movq(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ subq(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // (8a) Is the external string one byte?  If yes, go to (6).
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(not_zero, &seq_one_byte_string);  // Goto (6).

  // rdi: subject string (flat two-byte)
  // rax: RegExp data (FixedArray)
  // (9) Two byte sequential.  Load regexp code for one byte.  Go to (E).
  __ bind(&seq_two_byte_string);
  __ movq(r11, FieldOperand(rax, JSRegExp::kDataUC16CodeOffset));
  __ Set(rcx, 0);  // Type is two byte.
  __ jmp(&check_code);  // Go to (E).

  // (10) Not a string or a short external string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ testb(rbx, Immediate(kIsNotStringMask | kShortExternalStringMask));
  __ j(not_zero, &runtime);

  // (11) Sliced string.  Replace subject with parent. Go to (5a).
  // Load offset into r14 and replace subject string with parent.
  __ SmiToInteger32(r14, FieldOperand(rdi, SlicedString::kOffsetOffset));
  __ movq(rdi, FieldOperand(rdi, SlicedString::kParentOffset));
  __ jmp(&check_underlying);
#endif  // V8_INTERPRETED_REGEXP
}


void RegExpConstructResultStub::Generate(MacroAssembler* masm) {
  const int kMaxInlineLength = 100;
  Label slowcase;
  Label done;
  StackArgumentsAccessor args(rsp, 3, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(r8, args.GetArgumentOperand(0));
  __ JumpIfNotSmi(r8, &slowcase);
  __ SmiToInteger32(rbx, r8);
  __ cmpl(rbx, Immediate(kMaxInlineLength));
  __ j(above, &slowcase);
  // Smi-tagging is equivalent to multiplying by 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // Allocate RegExpResult followed by FixedArray with size in rbx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  __ Allocate(JSRegExpResult::kSize + FixedArray::kHeaderSize,
              times_pointer_size,
              rbx,  // In: Number of elements.
              rax,  // Out: Start of allocation (tagged).
              rcx,  // Out: End of allocation.
              rdx,  // Scratch register
              &slowcase,
              TAG_OBJECT);
  // rax: Start of allocated area, object-tagged.
  // rbx: Number of array elements as int32.
  // r8: Number of array elements as smi.

  // Set JSArray map to global.regexp_result_map().
  __ movq(rdx, ContextOperand(rsi, Context::GLOBAL_OBJECT_INDEX));
  __ movq(rdx, FieldOperand(rdx, GlobalObject::kNativeContextOffset));
  __ movq(rdx, ContextOperand(rdx, Context::REGEXP_RESULT_MAP_INDEX));
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), rdx);

  // Set empty properties FixedArray.
  __ LoadRoot(kScratchRegister, Heap::kEmptyFixedArrayRootIndex);
  __ movq(FieldOperand(rax, JSObject::kPropertiesOffset), kScratchRegister);

  // Set elements to point to FixedArray allocated right after the JSArray.
  __ lea(rcx, Operand(rax, JSRegExpResult::kSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rcx);

  // Set input, index and length fields from arguments.
  __ movq(r8, args.GetArgumentOperand(2));
  __ movq(FieldOperand(rax, JSRegExpResult::kInputOffset), r8);
  __ movq(r8, args.GetArgumentOperand(1));
  __ movq(FieldOperand(rax, JSRegExpResult::kIndexOffset), r8);
  __ movq(r8, args.GetArgumentOperand(0));
  __ movq(FieldOperand(rax, JSArray::kLengthOffset), r8);

  // Fill out the elements FixedArray.
  // rax: JSArray.
  // rcx: FixedArray.
  // rbx: Number of elements in array as int32.

  // Set map.
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rcx, HeapObject::kMapOffset), kScratchRegister);
  // Set length.
  __ Integer32ToSmi(rdx, rbx);
  __ movq(FieldOperand(rcx, FixedArray::kLengthOffset), rdx);
  // Fill contents of fixed-array with undefined.
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ lea(rcx, FieldOperand(rcx, FixedArray::kHeaderSize));
  // Fill fixed array elements with undefined.
  // rax: JSArray.
  // rbx: Number of elements in array that remains to be filled, as int32.
  // rcx: Start of elements in FixedArray.
  // rdx: undefined.
  Label loop;
  __ testl(rbx, rbx);
  __ bind(&loop);
  __ j(less_equal, &done);  // Jump if rcx is negative or zero.
  __ subl(rbx, Immediate(1));
  __ movq(Operand(rcx, rbx, times_pointer_size, 0), rdx);
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
                                                         Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch1;
  Register scratch = scratch2;

  // Load the number string cache.
  __ LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ SmiToInteger32(
      mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  __ shrl(mask, Immediate(1));
  __ subq(mask, Immediate(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  Factory* factory = masm->isolate()->factory();
  __ JumpIfSmi(object, &is_smi);
  __ CheckMap(object,
              factory->heap_number_map(),
              not_found,
              DONT_DO_SMI_CHECK);

  STATIC_ASSERT(8 == kDoubleSize);
  __ movl(scratch, FieldOperand(object, HeapNumber::kValueOffset + 4));
  __ xor_(scratch, FieldOperand(object, HeapNumber::kValueOffset));
  GenerateConvertHashCodeToIndex(masm, scratch, mask);

  Register index = scratch;
  Register probe = mask;
  __ movq(probe,
          FieldOperand(number_string_cache,
                        index,
                        times_1,
                        FixedArray::kHeaderSize));
  __ JumpIfSmi(probe, not_found);
  __ movsd(xmm0, FieldOperand(object, HeapNumber::kValueOffset));
  __ movsd(xmm1, FieldOperand(probe, HeapNumber::kValueOffset));
  __ ucomisd(xmm0, xmm1);
  __ j(parity_even, not_found);  // Bail out if NaN is involved.
  __ j(not_equal, not_found);  // The cache did not contain this value.
  __ jmp(&load_result_from_cache);

  __ bind(&is_smi);
  __ SmiToInteger32(scratch, object);
  GenerateConvertHashCodeToIndex(masm, scratch, mask);

  // Check if the entry is the smi we are looking for.
  __ cmpq(object,
          FieldOperand(number_string_cache,
                       index,
                       times_1,
                       FixedArray::kHeaderSize));
  __ j(not_equal, not_found);

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ movq(result,
          FieldOperand(number_string_cache,
                       index,
                       times_1,
                       FixedArray::kHeaderSize + kPointerSize));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->number_to_string_native(), 1);
}


void NumberToStringStub::GenerateConvertHashCodeToIndex(MacroAssembler* masm,
                                                        Register hash,
                                                        Register mask) {
  __ and_(hash, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  __ shl(hash, Immediate(kPointerSizeLog2 + 1));
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rbx, args.GetArgumentOperand(0));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, rbx, rax, r8, r9, &runtime);
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
    __ CompareMap(input, masm->isolate()->factory()->heap_number_map(), NULL);
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
  __ movq(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzxbq(scratch,
             FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ testb(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, label);
}


void ICCompareStub::GenerateGeneric(MacroAssembler* masm) {
  Label check_unequal_objects, done;
  Condition cc = GetCondition();
  Factory* factory = masm->isolate()->factory();

  Label miss;
  CheckInputType(masm, rdx, left_, &miss);
  CheckInputType(masm, rax, right_, &miss);

  // Compare two smis.
  Label non_smi, smi_done;
  __ JumpIfNotBothSmi(rax, rdx, &non_smi);
  __ subq(rdx, rax);
  __ j(no_overflow, &smi_done);
  __ not_(rdx);  // Correct sign in case of overflow. rdx cannot be 0 here.
  __ bind(&smi_done);
  __ movq(rax, rdx);
  __ ret(0);
  __ bind(&non_smi);

  // The compare stub returns a positive, negative, or zero 64-bit integer
  // value in rax, corresponding to result of comparing the two inputs.
  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Two identical objects are equal unless they are both NaN or undefined.
  {
    Label not_identical;
    __ cmpq(rax, rdx);
    __ j(not_equal, &not_identical, Label::kNear);

    if (cc != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      Label check_for_nan;
      __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Set(rax, NegativeComparisonResult(cc));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
    Label heap_number;
    // If it's not a heap number, then return equal for (in)equality operator.
    __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
           factory->heap_number_map());
    __ j(equal, &heap_number, Label::kNear);
    if (cc != equal) {
      // Call runtime on identical objects.  Otherwise return equal.
      __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
      __ j(above_equal, &not_identical, Label::kNear);
    }
    __ Set(rax, EQUAL);
    __ ret(0);

    __ bind(&heap_number);
    // It is a heap number, so return  equal if it's not NaN.
    // For NaN, return 1 for every condition except greater and
    // greater-equal.  Return -1 for them, so the comparison yields
    // false for all conditions except not-equal.
    __ Set(rax, EQUAL);
    __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
    __ ucomisd(xmm0, xmm0);
    __ setcc(parity_even, rax);
    // rax is 0 for equal non-NaN heapnumbers, 1 for NaNs.
    if (cc == greater_equal || cc == greater) {
      __ neg(rax);
    }
    __ ret(0);

    __ bind(&not_identical);
  }

  if (cc == equal) {  // Both strict and non-strict.
    Label slow;  // Fallthrough label.

    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    if (strict()) {
      // If either is a Smi (we know that not both are), then they can only
      // be equal if the other is a HeapNumber. If so, use the slow case.
      {
        Label not_smis;
        __ SelectNonSmi(rbx, rax, rdx, &not_smis);

        // Check if the non-smi operand is a heap number.
        __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
               factory->heap_number_map());
        // If heap number, handle it in the slow case.
        __ j(equal, &slow);
        // Return non-equal.  ebx (the lower half of rbx) is not zero.
        __ movq(rax, rbx);
        __ ret(0);

        __ bind(&not_smis);
      }

      // If either operand is a JSObject or an oddball value, then they are not
      // equal since their pointers are different
      // There is no test for undetectability in strict equality.

      // If the first object is a JS object, we have done pointer comparison.
      STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
      Label first_non_object;
      __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
      __ j(below, &first_non_object, Label::kNear);
      // Return non-zero (rax (not rax) is not zero)
      Label return_not_equal;
      STATIC_ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ CmpObjectType(rdx, FIRST_SPEC_OBJECT_TYPE, rcx);
      __ j(above_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Generate the number comparison code.
  Label non_number_comparison;
  Label unordered;
  FloatingPointHelper::LoadSSE2UnknownOperands(masm, &non_number_comparison);
  __ xorl(rax, rax);
  __ xorl(rcx, rcx);
  __ ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);
  // Return a result of -1, 0, or 1, based on EFLAGS.
  __ setcc(above, rax);
  __ setcc(below, rcx);
  __ subq(rax, rcx);
  __ ret(0);

  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  ASSERT(cc != not_equal);
  if (cc == less || cc == less_equal) {
    __ Set(rax, 1);
  } else {
    __ Set(rax, -1);
  }
  __ ret(0);

  // The number comparison code did not provide a valid result.
  __ bind(&non_number_comparison);

  // Fast negative check for internalized-to-internalized equality.
  Label check_for_strings;
  if (cc == equal) {
    BranchIfNotInternalizedString(
        masm, &check_for_strings, rax, kScratchRegister);
    BranchIfNotInternalizedString(
        masm, &check_for_strings, rdx, kScratchRegister);

    // We've already checked for object identity, so if both operands are
    // internalized strings they aren't equal. Register rax (not rax) already
    // holds a non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(
      rdx, rax, rcx, rbx, &check_unequal_objects);

  // Inline comparison of ASCII strings.
  if (cc == equal) {
    StringCompareStub::GenerateFlatAsciiStringEquals(masm,
                                                     rdx,
                                                     rax,
                                                     rcx,
                                                     rbx);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                       rdx,
                                                       rax,
                                                       rcx,
                                                       rbx,
                                                       rdi,
                                                       r8);
  }

#ifdef DEBUG
  __ Abort(kUnexpectedFallThroughFromStringComparison);
#endif

  __ bind(&check_unequal_objects);
  if (cc == equal && !strict()) {
    // Not strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label not_both_objects, return_unequal;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(rcx, Operand(rax, rdx, times_1, 0));
    __ testb(rcx, Immediate(kSmiTagMask));
    __ j(not_zero, &not_both_objects, Label::kNear);
    __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rbx);
    __ j(below, &not_both_objects, Label::kNear);
    __ CmpObjectType(rdx, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(below, &not_both_objects, Label::kNear);
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal, Label::kNear);
    __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal, Label::kNear);
    // The objects are both undetectable, so they both compare as the value
    // undefined, and are equal.
    __ Set(rax, EQUAL);
    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in rax,
    // or return equal if we fell through to here.
    __ ret(0);
    __ bind(&not_both_objects);
  }

  // Push arguments below the return address to prepare jump to builtin.
  __ PopReturnAddressTo(rcx);
  __ push(rdx);
  __ push(rax);

  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript builtin;
  if (cc == equal) {
    builtin = strict() ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    __ Push(Smi::FromInt(NegativeComparisonResult(cc)));
  }

  __ PushReturnAddressFrom(rcx);

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
  // rbx : cache cell for call target
  // rdi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function;

  // Load the cache state into rcx.
  __ movq(rcx, FieldOperand(rbx, Cell::kValueOffset));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ cmpq(rcx, rdi);
  __ j(equal, &done);
  __ Cmp(rcx, TypeFeedbackCells::MegamorphicSentinel(isolate));
  __ j(equal, &done);

  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the cell either some other function or an
  // AllocationSite. Do a map check on the object in rcx.
  Handle<Map> allocation_site_map(
      masm->isolate()->heap()->allocation_site_map(),
      masm->isolate());
  __ Cmp(FieldOperand(rcx, 0), allocation_site_map);
  __ j(not_equal, &miss);

  // Make sure the function is the Array() function
  __ LoadArrayFunction(rcx);
  __ cmpq(rdi, rcx);
  __ j(not_equal, &megamorphic);
  __ jmp(&done);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ Cmp(rcx, TypeFeedbackCells::UninitializedSentinel(isolate));
  __ j(equal, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ Move(FieldOperand(rbx, Cell::kValueOffset),
          TypeFeedbackCells::MegamorphicSentinel(isolate));
  __ jmp(&done);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ bind(&initialize);
  // Make sure the function is the Array() function
  __ LoadArrayFunction(rcx);
  __ cmpq(rdi, rcx);
  __ j(not_equal, &not_array_function);

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the cell
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    __ Integer32ToSmi(rax, rax);
    __ push(rax);
    __ push(rdi);
    __ push(rbx);

    CreateAllocationSiteStub create_stub;
    __ CallStub(&create_stub);

    __ pop(rbx);
    __ pop(rdi);
    __ pop(rax);
    __ SmiToInteger32(rax, rax);
  }
  __ jmp(&done);

  __ bind(&not_array_function);
  __ movq(FieldOperand(rbx, Cell::kValueOffset), rdi);
  // No need for a write barrier here - cells are rescanned.

  __ bind(&done);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  // rbx : cache cell for call target
  // rdi : the function to call
  Isolate* isolate = masm->isolate();
  Label slow, non_function;
  StackArgumentsAccessor args(rsp, argc_);

  // The receiver might implicitly be the global object. This is
  // indicated by passing the hole as the receiver to the call
  // function stub.
  if (ReceiverMightBeImplicit()) {
    Label call;
    // Get the receiver from the stack.
    __ movq(rax, args.GetReceiverOperand());
    // Call as function is indicated with the hole.
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &call, Label::kNear);
    // Patch the receiver on the stack with the global receiver object.
    __ movq(rcx, GlobalObjectOperand());
    __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalReceiverOffset));
    __ movq(args.GetReceiverOperand(), rcx);
    __ bind(&call);
  }

  // Check that the function really is a JavaScript function.
  __ JumpIfSmi(rdi, &non_function);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm);
  }

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);

  if (ReceiverMightBeImplicit()) {
    Label call_as_function;
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ j(equal, &call_as_function);
    __ InvokeFunction(rdi,
                      actual,
                      JUMP_FUNCTION,
                      NullCallWrapper(),
                      CALL_AS_METHOD);
    __ bind(&call_as_function);
  }
  __ InvokeFunction(rdi,
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
    __ Move(FieldOperand(rbx, Cell::kValueOffset),
            TypeFeedbackCells::MegamorphicSentinel(isolate));
  }
  // Check for function proxy.
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ PopReturnAddressTo(rcx);
  __ push(rdi);  // put proxy as additional argument under return address
  __ PushReturnAddressFrom(rcx);
  __ Set(rax, argc_ + 1);
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor =
      masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
    __ jmp(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ bind(&non_function);
  __ movq(args.GetReceiverOperand(), rdi);
  __ Set(rax, argc_);
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor =
      isolate->builtins()->ArgumentsAdaptorTrampoline();
  __ Jump(adaptor, RelocInfo::CODE_TARGET);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // rax : number of arguments
  // rbx : cache cell for call target
  // rdi : constructor function
  Label slow, non_function_call;

  // Check that function is not a smi.
  __ JumpIfSmi(rdi, &non_function_call);
  // Check that function is a JSFunction.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm);
  }

  // Jump to the function-specific construct stub.
  Register jmp_reg = rcx;
  __ movq(jmp_reg, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(jmp_reg, FieldOperand(jmp_reg,
                                SharedFunctionInfo::kConstructStubOffset));
  __ lea(jmp_reg, FieldOperand(jmp_reg, Code::kHeaderSize));
  __ jmp(jmp_reg);

  // rdi: called object
  // rax: number of arguments
  // rcx: object map
  Label do_call;
  __ bind(&slow);
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function_call);
  __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ jmp(&do_call);

  __ bind(&non_function_call);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ bind(&do_call);
  // Set expected number of arguments to zero (not changing rax).
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


bool CEntryStub::IsPregenerated(Isolate* isolate) {
#ifdef _WIN64
  return result_size_ == 1;
#else
  return true;
#endif
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  RecordWriteStub::GenerateFixedRegStubsAheadOfTime(isolate);
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(1, kDontSaveFPRegs);
  stub.GetCode(isolate)->set_is_pregenerated(true);
  CEntryStub save_doubles(1, kSaveFPRegs);
  save_doubles.GetCode(isolate)->set_is_pregenerated(true);
}


static void JumpIfOOM(MacroAssembler* masm,
                      Register value,
                      Register scratch,
                      Label* oom_label) {
  __ movq(scratch, value);
  STATIC_ASSERT(Failure::OUT_OF_MEMORY_EXCEPTION == 3);
  STATIC_ASSERT(kFailureTag == 3);
  __ and_(scratch, Immediate(0xf));
  __ cmpq(scratch, Immediate(0xf));
  __ j(equal, oom_label);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope) {
  // rax: result parameter for PerformGC, if any.
  // rbx: pointer to C function  (C callee-saved).
  // rbp: frame pointer  (restored after C call).
  // rsp: stack pointer  (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: pointer to the first argument (C callee-saved).
  //      This pointer is reused in LeaveExitFrame(), so it is stored in a
  //      callee-saved register.

  // Simple results returned in rax (both AMD64 and Win64 calling conventions).
  // Complex results must be written to address passed as first argument.
  // AMD64 calling convention: a struct of two pointers in rax+rdx

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }

  if (do_gc) {
    // Pass failure code returned from last attempt as first argument to
    // PerformGC. No need to use PrepareCallCFunction/CallCFunction here as the
    // stack is known to be aligned. This function takes one argument which is
    // passed in register.
    __ movq(arg_reg_1, rax);
    __ movq(kScratchRegister,
            ExternalReference::perform_gc_function(masm->isolate()));
    __ call(kScratchRegister);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth(masm->isolate());
  if (always_allocate_scope) {
    Operand scope_depth_operand = masm->ExternalOperand(scope_depth);
    __ incl(scope_depth_operand);
  }

  // Call C function.
#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9.
  // Pass argv and argc as two parameters. The arguments object will
  // be created by stubs declared by DECLARE_RUNTIME_FUNCTION().
  if (result_size_ < 2) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax).
    __ movq(rcx, r14);  // argc.
    __ movq(rdx, r15);  // argv.
    __ movq(r8, ExternalReference::isolate_address(masm->isolate()));
  } else {
    ASSERT_EQ(2, result_size_);
    // Pass a pointer to the result location as the first argument.
    __ lea(rcx, StackSpaceOperand(2));
    // Pass a pointer to the Arguments object as the second argument.
    __ movq(rdx, r14);  // argc.
    __ movq(r8, r15);   // argv.
    __ movq(r9, ExternalReference::isolate_address(masm->isolate()));
  }

#else  // _WIN64
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9.
  __ movq(rdi, r14);  // argc.
  __ movq(rsi, r15);  // argv.
  __ movq(rdx, ExternalReference::isolate_address(masm->isolate()));
#endif
  __ call(rbx);
  // Result is in rax - do not destroy this register!

  if (always_allocate_scope) {
    Operand scope_depth_operand = masm->ExternalOperand(scope_depth);
    __ decl(scope_depth_operand);
  }

  // Check for failure result.
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
#ifdef _WIN64
  // If return value is on the stack, pop it to registers.
  if (result_size_ > 1) {
    ASSERT_EQ(2, result_size_);
    // Read result values stored on stack. Result is stored
    // above the four argument mirror slots and the two
    // Arguments object slots.
    __ movq(rax, Operand(rsp, 6 * kPointerSize));
    __ movq(rdx, Operand(rsp, 7 * kPointerSize));
  }
#endif
  __ lea(rcx, Operand(rax, 1));
  // Lower 2 bits of rcx are 0 iff rax has failure tag.
  __ testl(rcx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned);

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ testl(rax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, Label::kNear);

  // Special handling of out of memory exceptions.
  JumpIfOOM(masm, rax, kScratchRegister, throw_out_of_memory_exception);

  // Retrieve the pending exception.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, masm->isolate());
  Operand pending_exception_operand =
      masm->ExternalOperand(pending_exception_address);
  __ movq(rax, pending_exception_operand);

  // See if we just retrieved an OOM exception.
  JumpIfOOM(masm, rax, kScratchRegister, throw_out_of_memory_exception);

  // Clear the pending exception.
  pending_exception_operand =
      masm->ExternalOperand(pending_exception_address);
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ movq(pending_exception_operand, rdx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter the exit frame that transitions from JavaScript to C++.
#ifdef _WIN64
  int arg_stack_space = (result_size_ < 2 ? 2 : 4);
#else
  int arg_stack_space = 0;
#endif
  __ EnterExitFrame(arg_stack_space, save_doubles_);

  // rax: Holds the context at this point, but should not be used.
  //      On entry to code generated by GenerateCore, it must hold
  //      a failure result if the collect_garbage argument to GenerateCore
  //      is true.  This failure result can be the result of code
  //      generated by a previous call to GenerateCore.  The value
  //      of rax is then passed to Runtime::PerformGC.
  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: argv pointer (C callee-saved).

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
  __ movq(rax, failure, RelocInfo::NONE64);
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
  __ Set(rax, static_cast<int64_t>(false));
  __ Store(external_caught, rax);

  // Set pending exception and rax to out of memory exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate);
  Label already_have_failure;
  JumpIfOOM(masm, rax, kScratchRegister, &already_have_failure);
  __ movq(rax, Failure::OutOfMemoryException(0x1), RelocInfo::NONE64);
  __ bind(&already_have_failure);
  __ Store(pending_exception, rax);
  // Fall through to the next label.

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(rax);

  __ bind(&throw_normal_exception);
  __ Throw(rax);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  {  // NOLINT. Scope block confuses linter.
    MacroAssembler::NoRootArrayScope uninitialized_root_register(masm);
    // Set up frame.
    __ push(rbp);
    __ movq(rbp, rsp);

    // Push the stack frame type marker twice.
    int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
    // Scratch register is neither callee-save, nor an argument register on any
    // platform. It's free to use at this point.
    // Cannot use smi-register for loading yet.
    __ movq(kScratchRegister,
            reinterpret_cast<uint64_t>(Smi::FromInt(marker)),
            RelocInfo::NONE64);
    __ push(kScratchRegister);  // context slot
    __ push(kScratchRegister);  // function slot
    // Save callee-saved registers (X64/Win64 calling conventions).
    __ push(r12);
    __ push(r13);
    __ push(r14);
    __ push(r15);
#ifdef _WIN64
    __ push(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
    __ push(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif
    __ push(rbx);

#ifdef _WIN64
    // On Win64 XMM6-XMM15 are callee-save
    __ subq(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0), xmm6);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1), xmm7);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2), xmm8);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3), xmm9);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4), xmm10);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5), xmm11);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6), xmm12);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7), xmm13);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8), xmm14);
    __ movdqu(Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9), xmm15);
#endif

    // Set up the roots and smi constant registers.
    // Needs to be done before any further smi loads.
    __ InitializeSmiConstantRegister();
    __ InitializeRootRegister();
  }

  Isolate* isolate = masm->isolate();

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate);
  {
    Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ push(c_entry_fp_operand);
  }

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate);
  __ Load(rax, js_entry_sp);
  __ testq(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movq(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate);
  __ Store(pending_exception, rax);
  __ movq(rax, Failure::Exception(), RelocInfo::NONE64);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.  There's only one
  // handler block in this code object, so its index is 0.
  __ bind(&invoke);
  __ PushTryHandler(StackHandler::JS_ENTRY, 0);

  // Clear any pending exceptions.
  __ LoadRoot(rax, Heap::kTheHoleValueRootIndex);
  __ Store(pending_exception, rax);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. We load the address from an
  // external reference instead of inlining the call target address directly
  // in the code, because the builtin stubs may not have been generated yet
  // at the time this code is generated.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate);
    __ Load(rax, construct_entry);
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate);
    __ Load(rax, entry);
  }
  __ lea(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(rbx);
  __ Cmp(rbx, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ movq(kScratchRegister, js_entry_sp);
  __ movq(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  { Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ pop(c_entry_fp_operand);
  }

  // Restore callee-saved registers (X64 conventions).
#ifdef _WIN64
  // On Win64 XMM6-XMM15 are callee-save
  __ movdqu(xmm6, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 0));
  __ movdqu(xmm7, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 1));
  __ movdqu(xmm8, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 2));
  __ movdqu(xmm9, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 3));
  __ movdqu(xmm10, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 4));
  __ movdqu(xmm11, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 5));
  __ movdqu(xmm12, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 6));
  __ movdqu(xmm13, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 7));
  __ movdqu(xmm14, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 8));
  __ movdqu(xmm15, Operand(rsp, EntryFrameConstants::kXMMRegisterSize * 9));
  __ addq(rsp, Immediate(EntryFrameConstants::kXMMRegistersBlockSize));
#endif

  __ pop(rbx);
#ifdef _WIN64
  // Callee save on in Win64 ABI, arguments/volatile in AMD64 ABI.
  __ pop(rsi);
  __ pop(rdi);
#endif
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ addq(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(rbp);
  __ ret(0);
}


void InstanceofStub::Generate(MacroAssembler* masm) {
  // Implements "value instanceof function" operator.
  // Expected input state with no inline cache:
  //   rsp[0]  : return address
  //   rsp[8]  : function pointer
  //   rsp[16] : value
  // Expected input state with an inline one-element cache:
  //   rsp[0]  : return address
  //   rsp[8]  : offset from return address to location of inline cache
  //   rsp[16] : function pointer
  //   rsp[24] : value
  // Returns a bitwise zero to indicate that the value
  // is and instance of the function and anything else to
  // indicate that the value is not an instance.

  static const int kOffsetToMapCheckValue = 2;
  static const int kOffsetToResultValue = 18;
  // The last 4 bytes of the instruction sequence
  //   movq(rdi, FieldOperand(rax, HeapObject::kMapOffset))
  //   Move(kScratchRegister, Factory::the_hole_value())
  // in front of the hole value address.
  static const unsigned int kWordBeforeMapCheckValue = 0xBA49FF78;
  // The last 4 bytes of the instruction sequence
  //   __ j(not_equal, &cache_miss);
  //   __ LoadRoot(ToRegister(instr->result()), Heap::kTheHoleValueRootIndex);
  // before the offset of the hole value in the root array.
  static const unsigned int kWordBeforeResultValue = 0x458B4909;
  // Only the inline check flag is supported on X64.
  ASSERT(flags_ == kNoFlags || HasCallSiteInlineCheck());
  int extra_argument_offset = HasCallSiteInlineCheck() ? 1 : 0;

  // Get the object - go slow case if it's a smi.
  Label slow;
  StackArgumentsAccessor args(rsp, 2 + extra_argument_offset,
                              ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rax, args.GetArgumentOperand(0));
  __ JumpIfSmi(rax, &slow);

  // Check that the left hand is a JS object. Leave its map in rax.
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rax);
  __ j(below, &slow);
  __ CmpInstanceType(rax, LAST_SPEC_OBJECT_TYPE);
  __ j(above, &slow);

  // Get the prototype of the function.
  __ movq(rdx, args.GetArgumentOperand(1));
  // rdx is function, rax is map.

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    // Look up the function and the map in the instanceof cache.
    Label miss;
    __ CompareRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ CompareRoot(rax, Heap::kInstanceofCacheMapRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ LoadRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
    __ ret(2 * kPointerSize);
    __ bind(&miss);
  }

  __ TryGetFunctionPrototype(rdx, rbx, &slow, true);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(rbx, &slow);
  __ CmpObjectType(rbx, FIRST_SPEC_OBJECT_TYPE, kScratchRegister);
  __ j(below, &slow);
  __ CmpInstanceType(kScratchRegister, LAST_SPEC_OBJECT_TYPE);
  __ j(above, &slow);

  // Register mapping:
  //   rax is object map.
  //   rdx is function.
  //   rbx is function prototype.
  if (!HasCallSiteInlineCheck()) {
    __ StoreRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(rax, Heap::kInstanceofCacheMapRootIndex);
  } else {
    // Get return address and delta to inlined map check.
    __ movq(kScratchRegister, StackOperandForReturnAddress(0));
    __ subq(kScratchRegister, args.GetArgumentOperand(2));
    if (FLAG_debug_code) {
      __ movl(rdi, Immediate(kWordBeforeMapCheckValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToMapCheckValue - 4), rdi);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheCheck);
    }
    __ movq(kScratchRegister,
            Operand(kScratchRegister, kOffsetToMapCheckValue));
    __ movq(Operand(kScratchRegister, 0), rax);
  }

  __ movq(rcx, FieldOperand(rax, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  Label loop, is_instance, is_not_instance;
  __ LoadRoot(kScratchRegister, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmpq(rcx, rbx);
  __ j(equal, &is_instance, Label::kNear);
  __ cmpq(rcx, kScratchRegister);
  // The code at is_not_instance assumes that kScratchRegister contains a
  // non-zero GCable value (the null object in this case).
  __ j(equal, &is_not_instance, Label::kNear);
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rcx, FieldOperand(rcx, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ xorl(rax, rax);
    // Store bitwise zero in the cache.  This is a Smi in GC terms.
    STATIC_ASSERT(kSmiTag == 0);
    __ StoreRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Store offset of true in the root array at the inline check site.
    int true_offset = 0x100 +
        (Heap::kTrueValueRootIndex << kPointerSizeLog2) - kRootRegisterBias;
    // Assert it is a 1-byte signed value.
    ASSERT(true_offset >= 0 && true_offset < 0x100);
    __ movl(rax, Immediate(true_offset));
    __ movq(kScratchRegister, StackOperandForReturnAddress(0));
    __ subq(kScratchRegister, args.GetArgumentOperand(2));
    __ movb(Operand(kScratchRegister, kOffsetToResultValue), rax);
    if (FLAG_debug_code) {
      __ movl(rax, Immediate(kWordBeforeResultValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToResultValue - 4), rax);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheMov);
    }
    __ Set(rax, 0);
  }
  __ ret((2 + extra_argument_offset) * kPointerSize);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    // We have to store a non-zero value in the cache.
    __ StoreRoot(kScratchRegister, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Store offset of false in the root array at the inline check site.
    int false_offset = 0x100 +
        (Heap::kFalseValueRootIndex << kPointerSizeLog2) - kRootRegisterBias;
    // Assert it is a 1-byte signed value.
    ASSERT(false_offset >= 0 && false_offset < 0x100);
    __ movl(rax, Immediate(false_offset));
    __ movq(kScratchRegister, StackOperandForReturnAddress(0));
    __ subq(kScratchRegister, args.GetArgumentOperand(2));
    __ movb(Operand(kScratchRegister, kOffsetToResultValue), rax);
    if (FLAG_debug_code) {
      __ movl(rax, Immediate(kWordBeforeResultValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToResultValue - 4), rax);
      __ Assert(equal, kInstanceofStubUnexpectedCallSiteCacheMov);
    }
  }
  __ ret((2 + extra_argument_offset) * kPointerSize);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  if (HasCallSiteInlineCheck()) {
    // Remove extra value from the stack.
    __ PopReturnAddressTo(rcx);
    __ pop(rax);
    __ PushReturnAddressFrom(rcx);
  }
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
}


// Passing arguments in registers is not supported.
Register InstanceofStub::left() { return no_reg; }


Register InstanceofStub::right() { return no_reg; }


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;
  Label sliced_string;

  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ testb(result_, Immediate(kIsNotStringMask));
  __ j(not_zero, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ SmiCompare(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiToInteger32(index_, index_);

  StringCharLoadGenerator::Generate(
      masm, object_, index_, result_, &call_runtime_);

  __ Integer32ToSmi(result_, result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  Factory* factory = masm->isolate()->factory();
  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              factory->heap_number_map(),
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
  if (!index_.is(rax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ movq(index_, rax);
  }
  __ pop(object_);
  // Reload the instance type.
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ Integer32ToSmi(index_, index_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  __ JumpIfNotSmi(code_, &slow_case_);
  __ SmiCompare(code_, Smi::FromInt(String::kMaxOneByteCharCode));
  __ j(above, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  SmiIndex index = masm->SmiToIndex(kScratchRegister, code_, kPointerSizeLog2);
  __ movq(result_, FieldOperand(result_, index.reg, index.scale,
                                FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
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
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label call_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  // Load the two arguments.
  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rax, args.GetArgumentOperand(0));  // First argument (left).
  __ movq(rdx, args.GetArgumentOperand(1));  // Second argument (right).

  // Make sure that both arguments are strings if not known in advance.
  // Otherwise, at least one of the arguments is definitely a string,
  // and we convert the one that is not known to be a string.
  if ((flags_ & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_BOTH) {
    ASSERT((flags_ & STRING_ADD_CHECK_LEFT) == STRING_ADD_CHECK_LEFT);
    ASSERT((flags_ & STRING_ADD_CHECK_RIGHT) == STRING_ADD_CHECK_RIGHT);
    __ JumpIfSmi(rax, &call_runtime);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, r8);
    __ j(above_equal, &call_runtime);

    // First argument is a a string, test second.
    __ JumpIfSmi(rdx, &call_runtime);
    __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, r9);
    __ j(above_equal, &call_runtime);
  } else if ((flags_ & STRING_ADD_CHECK_LEFT) == STRING_ADD_CHECK_LEFT) {
    ASSERT((flags_ & STRING_ADD_CHECK_RIGHT) == 0);
    GenerateConvertArgument(masm, 2 * kPointerSize, rax, rbx, rcx, rdi,
                            &call_builtin);
    builtin_id = Builtins::STRING_ADD_RIGHT;
  } else if ((flags_ & STRING_ADD_CHECK_RIGHT) == STRING_ADD_CHECK_RIGHT) {
    ASSERT((flags_ & STRING_ADD_CHECK_LEFT) == 0);
    GenerateConvertArgument(masm, 1 * kPointerSize, rdx, rbx, rcx, rdi,
                            &call_builtin);
    builtin_id = Builtins::STRING_ADD_LEFT;
  }

  // Both arguments are strings.
  // rax: first string
  // rdx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ movq(rcx, FieldOperand(rdx, String::kLengthOffset));
  __ SmiTest(rcx);
  __ j(not_zero, &second_not_zero_length, Label::kNear);
  // Second string is empty, result is first string which is already in rax.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ movq(rbx, FieldOperand(rax, String::kLengthOffset));
  __ SmiTest(rbx);
  __ j(not_zero, &both_not_zero_length, Label::kNear);
  // First string is empty, result is second string which is in rdx.
  __ movq(rax, rdx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // rax: first string
  // rbx: length of first string
  // rcx: length of second string
  // rdx: second string
  // r8: map of first string (if flags_ == NO_STRING_ADD_FLAGS)
  // r9: map of second string (if flags_ == NO_STRING_ADD_FLAGS)
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);

  // If arguments where known to be strings, maps are not loaded to r8 and r9
  // by the code above.
  if ((flags_ & STRING_ADD_CHECK_BOTH) != STRING_ADD_CHECK_BOTH) {
    __ movq(r8, FieldOperand(rax, HeapObject::kMapOffset));
    __ movq(r9, FieldOperand(rdx, HeapObject::kMapOffset));
  }
  // Get the instance types of the two strings as they will be needed soon.
  __ movzxbl(r8, FieldOperand(r8, Map::kInstanceTypeOffset));
  __ movzxbl(r9, FieldOperand(r9, Map::kInstanceTypeOffset));

  // Look at the length of the result of adding the two strings.
  STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue / 2);
  __ SmiAdd(rbx, rbx, rcx);
  // Use the string table when adding two one character strings, as it
  // helps later optimizations to return an internalized string here.
  __ SmiCompare(rbx, Smi::FromInt(2));
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ASCII strings.
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(r8, r9, rbx, rcx,
                                                  &call_runtime);

  // Get the two characters forming the sub string.
  __ movzxbq(rbx, FieldOperand(rax, SeqOneByteString::kHeaderSize));
  __ movzxbq(rcx, FieldOperand(rdx, SeqOneByteString::kHeaderSize));

  // Try to lookup two character string in string table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_flat_ascii_string;
  StringHelper::GenerateTwoCharacterStringTableProbe(
      masm, rbx, rcx, r14, r11, rdi, r15, &make_two_character_string);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&make_two_character_string);
  __ Set(rdi, 2);
  __ AllocateAsciiString(rax, rdi, r8, r9, r11, &call_runtime);
  // rbx - first byte: first character
  // rbx - second byte: *maybe* second character
  // Make sure that the second byte of rbx contains the second character.
  __ movzxbq(rcx, FieldOperand(rdx, SeqOneByteString::kHeaderSize));
  __ shll(rcx, Immediate(kBitsPerByte));
  __ orl(rbx, rcx);
  // Write both characters to the new string.
  __ movw(FieldOperand(rax, SeqOneByteString::kHeaderSize), rbx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ SmiCompare(rbx, Smi::FromInt(ConsString::kMinLength));
  __ j(below, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  __ SmiCompare(rbx, Smi::FromInt(String::kMaxLength));
  __ j(above, &call_runtime);

  // If result is not supposed to be flat, allocate a cons string object. If
  // both strings are ASCII the result is an ASCII cons string.
  // rax: first string
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii, allocated, ascii_data;
  __ movl(rcx, r8);
  __ and_(rcx, r9);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ testl(rcx, Immediate(kStringEncodingMask));
  __ j(zero, &non_ascii);
  __ bind(&ascii_data);
  // Allocate an ASCII cons string.
  __ AllocateAsciiConsString(rcx, rdi, no_reg, &call_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ movq(FieldOperand(rcx, ConsString::kLengthOffset), rbx);
  __ movq(FieldOperand(rcx, ConsString::kHashFieldOffset),
          Immediate(String::kEmptyHashField));

  Label skip_write_barrier, after_writing;
  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(masm->isolate());
  __ Load(rbx, high_promotion_mode);
  __ testb(rbx, Immediate(1));
  __ j(zero, &skip_write_barrier);

  __ movq(FieldOperand(rcx, ConsString::kFirstOffset), rax);
  __ RecordWriteField(rcx,
                      ConsString::kFirstOffset,
                      rax,
                      rbx,
                      kDontSaveFPRegs);
  __ movq(FieldOperand(rcx, ConsString::kSecondOffset), rdx);
  __ RecordWriteField(rcx,
                      ConsString::kSecondOffset,
                      rdx,
                      rbx,
                      kDontSaveFPRegs);
  __ jmp(&after_writing);

  __ bind(&skip_write_barrier);
  __ movq(FieldOperand(rcx, ConsString::kFirstOffset), rax);
  __ movq(FieldOperand(rcx, ConsString::kSecondOffset), rdx);

  __ bind(&after_writing);

  __ movq(rax, rcx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only one byte characters.
  // rcx: first instance type AND second instance type.
  // r8: first instance type.
  // r9: second instance type.
  __ testb(rcx, Immediate(kOneByteDataHintMask));
  __ j(not_zero, &ascii_data);
  __ xor_(r8, r9);
  STATIC_ASSERT(kOneByteStringTag != 0 && kOneByteDataHintTag != 0);
  __ andb(r8, Immediate(kOneByteStringTag | kOneByteDataHintTag));
  __ cmpb(r8, Immediate(kOneByteStringTag | kOneByteDataHintTag));
  __ j(equal, &ascii_data);
  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(rcx, rdi, no_reg, &call_runtime);
  __ jmp(&allocated);

  // We cannot encounter sliced strings or cons strings here since:
  STATIC_ASSERT(SlicedString::kMinLength >= ConsString::kMinLength);
  // Handle creating a flat result from either external or sequential strings.
  // Locate the first characters' locations.
  // rax: first string
  // rbx: length of resulting flat string as smi
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  Label first_prepared, second_prepared;
  Label first_is_sequential, second_is_sequential;
  __ bind(&string_add_flat_result);

  __ SmiToInteger32(r14, FieldOperand(rax, SeqString::kLengthOffset));
  // r14: length of first string
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(r8, Immediate(kStringRepresentationMask));
  __ j(zero, &first_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(r8, Immediate(kShortExternalStringMask));
  __ j(not_zero, &call_runtime);
  __ movq(rcx, FieldOperand(rax, ExternalString::kResourceDataOffset));
  __ jmp(&first_prepared, Label::kNear);
  __ bind(&first_is_sequential);
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ lea(rcx, FieldOperand(rax, SeqOneByteString::kHeaderSize));
  __ bind(&first_prepared);

  // Check whether both strings have same encoding.
  __ xorl(r8, r9);
  __ testb(r8, Immediate(kStringEncodingMask));
  __ j(not_zero, &call_runtime);

  __ SmiToInteger32(r15, FieldOperand(rdx, SeqString::kLengthOffset));
  // r15: length of second string
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(r9, Immediate(kStringRepresentationMask));
  __ j(zero, &second_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(r9, Immediate(kShortExternalStringMask));
  __ j(not_zero, &call_runtime);
  __ movq(rdx, FieldOperand(rdx, ExternalString::kResourceDataOffset));
  __ jmp(&second_prepared, Label::kNear);
  __ bind(&second_is_sequential);
  STATIC_ASSERT(SeqOneByteString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ lea(rdx, FieldOperand(rdx, SeqOneByteString::kHeaderSize));
  __ bind(&second_prepared);

  Label non_ascii_string_add_flat_result;
  // r9: instance type of second string
  // First string and second string have the same encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ SmiToInteger32(rbx, rbx);
  __ testb(r9, Immediate(kStringEncodingMask));
  __ j(zero, &non_ascii_string_add_flat_result);

  __ bind(&make_flat_ascii_string);
  // Both strings are ASCII strings. As they are short they are both flat.
  __ AllocateAsciiString(rax, rbx, rdi, r8, r9, &call_runtime);
  // rax: result string
  // Locate first character of result.
  __ lea(rbx, FieldOperand(rax, SeqOneByteString::kHeaderSize));
  // rcx: first char of first string
  // rbx: first character of result
  // r14: length of first string
  StringHelper::GenerateCopyCharacters(masm, rbx, rcx, r14, true);
  // rbx: next character of result
  // rdx: first char of second string
  // r15: length of second string
  StringHelper::GenerateCopyCharacters(masm, rbx, rdx, r15, true);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are ASCII strings. As they are short they are both flat.
  __ AllocateTwoByteString(rax, rbx, rdi, r8, r9, &call_runtime);
  // rax: result string
  // Locate first character of result.
  __ lea(rbx, FieldOperand(rax, SeqTwoByteString::kHeaderSize));
  // rcx: first char of first string
  // rbx: first character of result
  // r14: length of first string
  StringHelper::GenerateCopyCharacters(masm, rbx, rcx, r14, false);
  // rbx: next character of result
  // rdx: first char of second string
  // r15: length of second string
  StringHelper::GenerateCopyCharacters(masm, rbx, rdx, r15, false);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Just jump to runtime to add the two strings.
  __ bind(&call_runtime);

  if ((flags_ & STRING_ADD_ERECT_FRAME) != 0) {
    GenerateRegisterArgsPop(masm, rcx);
    // Build a frame
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      GenerateRegisterArgsPush(masm);
      __ CallRuntime(Runtime::kStringAdd, 2);
    }
    __ Ret();
  } else {
    __ TailCallRuntime(Runtime::kStringAdd, 2, 1);
  }

  if (call_builtin.is_linked()) {
    __ bind(&call_builtin);
    if ((flags_ & STRING_ADD_ERECT_FRAME) != 0) {
      GenerateRegisterArgsPop(masm, rcx);
      // Build a frame
      {
        FrameScope scope(masm, StackFrame::INTERNAL);
        GenerateRegisterArgsPush(masm);
        __ InvokeBuiltin(builtin_id, CALL_FUNCTION);
      }
      __ Ret();
    } else {
      __ InvokeBuiltin(builtin_id, JUMP_FUNCTION);
    }
  }
}


void StringAddStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ push(rax);
  __ push(rdx);
}


void StringAddStub::GenerateRegisterArgsPop(MacroAssembler* masm,
                                            Register temp) {
  __ PopReturnAddressTo(temp);
  __ pop(rdx);
  __ pop(rax);
  __ PushReturnAddressFrom(temp);
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
  __ bind(&not_string);
  // Puts the cached result into scratch1.
  NumberToStringStub::GenerateLookupNumberStringCache(masm,
                                                      arg,
                                                      scratch1,
                                                      scratch2,
                                                      scratch3,
                                                      slow);
  __ movq(arg, scratch1);
  __ movq(Operand(rsp, stack_offset), arg);
  __ bind(&done);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          bool ascii) {
  Label loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ movb(kScratchRegister, Operand(src, 0));
    __ movb(Operand(dest, 0), kScratchRegister);
    __ incq(src);
    __ incq(dest);
  } else {
    __ movzxwl(kScratchRegister, Operand(src, 0));
    __ movw(Operand(dest, 0), kScratchRegister);
    __ addq(src, Immediate(2));
    __ addq(dest, Immediate(2));
  }
  __ decl(count);
  __ j(not_zero, &loop);
}


void StringHelper::GenerateCopyCharactersREP(MacroAssembler* masm,
                                             Register dest,
                                             Register src,
                                             Register count,
                                             bool ascii) {
  // Copy characters using rep movs of doublewords. Align destination on 4 byte
  // boundary before starting rep movs. Copy remaining characters after running
  // rep movs.
  // Count is positive int32, dest and src are character pointers.
  ASSERT(dest.is(rdi));  // rep movs destination
  ASSERT(src.is(rsi));  // rep movs source
  ASSERT(count.is(rcx));  // rep movs count

  // Nothing to do for zero characters.
  Label done;
  __ testl(count, count);
  __ j(zero, &done, Label::kNear);

  // Make count the number of bytes to copy.
  if (!ascii) {
    STATIC_ASSERT(2 == sizeof(uc16));
    __ addl(count, count);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ testl(count, Immediate(~(kPointerSize - 1)));
  __ j(zero, &last_bytes, Label::kNear);

  // Copy from edi to esi using rep movs instruction.
  __ movl(kScratchRegister, count);
  __ shr(count, Immediate(kPointerSizeLog2));  // Number of doublewords to copy.
  __ repmovsq();

  // Find number of bytes left.
  __ movl(count, kScratchRegister);
  __ and_(count, Immediate(kPointerSize - 1));

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ testl(count, count);
  __ j(zero, &done, Label::kNear);

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ movb(kScratchRegister, Operand(src, 0));
  __ movb(Operand(dest, 0), kScratchRegister);
  __ incq(src);
  __ incq(dest);
  __ decl(count);
  __ j(not_zero, &loop);

  __ bind(&done);
}

void StringHelper::GenerateTwoCharacterStringTableProbe(MacroAssembler* masm,
                                                        Register c1,
                                                        Register c2,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4,
                                                        Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the string table.
  Label not_array_index;
  __ leal(scratch, Operand(c1, -'0'));
  __ cmpl(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index, Label::kNear);
  __ leal(scratch, Operand(c2, -'0'));
  __ cmpl(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(below_equal, not_found);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  GenerateHashInit(masm, hash, c1, scratch);
  GenerateHashAddCharacter(masm, hash, c2, scratch);
  GenerateHashGetHash(masm, hash, scratch);

  // Collect the two characters in a register.
  Register chars = c1;
  __ shl(c2, Immediate(kBitsPerByte));
  __ orl(chars, c2);

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load the string table.
  Register string_table = c2;
  __ LoadRoot(string_table, Heap::kStringTableRootIndex);

  // Calculate capacity mask from the string table capacity.
  Register mask = scratch2;
  __ SmiToInteger32(mask,
                    FieldOperand(string_table, StringTable::kCapacityOffset));
  __ decl(mask);

  Register map = scratch4;

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string (32-bit int)
  // string_table: string table
  // mask:         capacity mask (32-bit int)
  // map:          -
  // scratch:      -

  // Perform a number of probes in the string table.
  static const int kProbes = 4;
  Label found_in_string_table;
  Label next_probe[kProbes];
  Register candidate = scratch;  // Scratch register contains candidate.
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in string table.
    __ movl(scratch, hash);
    if (i > 0) {
      __ addl(scratch, Immediate(StringTable::GetProbeOffset(i)));
    }
    __ andl(scratch, mask);

    // Load the entry from the string table.
    STATIC_ASSERT(StringTable::kEntrySize == 1);
    __ movq(candidate,
            FieldOperand(string_table,
                         scratch,
                         times_pointer_size,
                         StringTable::kElementsStartOffset));

    // If entry is undefined no string with this hash can be found.
    Label is_string;
    __ CmpObjectType(candidate, ODDBALL_TYPE, map);
    __ j(not_equal, &is_string, Label::kNear);

    __ CompareRoot(candidate, Heap::kUndefinedValueRootIndex);
    __ j(equal, not_found);
    // Must be the hole (deleted entry).
    if (FLAG_debug_code) {
      __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
      __ cmpq(kScratchRegister, candidate);
      __ Assert(equal, kOddballInStringTableIsNotUndefinedOrTheHole);
    }
    __ jmp(&next_probe[i]);

    __ bind(&is_string);

    // If length is not 2 the string is not a candidate.
    __ SmiCompare(FieldOperand(candidate, String::kLengthOffset),
                  Smi::FromInt(2));
    __ j(not_equal, &next_probe[i]);

    // We use kScratchRegister as a temporary register in assumption that
    // JumpIfInstanceTypeIsNotSequentialAscii does not use it implicitly
    Register temp = kScratchRegister;

    // Check that the candidate is a non-external ASCII string.
    __ movzxbl(temp, FieldOperand(map, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe[i]);

    // Check if the two characters match.
    __ movl(temp, FieldOperand(candidate, SeqOneByteString::kHeaderSize));
    __ andl(temp, Immediate(0x0000ffff));
    __ cmpl(chars, temp);
    __ j(equal, &found_in_string_table);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = candidate;
  __ bind(&found_in_string_table);
  if (!result.is(rax)) {
    __ movq(rax, result);
  }
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = (seed + character) + ((seed + character) << 10);
  __ LoadRoot(scratch, Heap::kHashSeedRootIndex);
  __ SmiToInteger32(scratch, scratch);
  __ addl(scratch, character);
  __ movl(hash, scratch);
  __ shll(scratch, Immediate(10));
  __ addl(hash, scratch);
  // hash ^= hash >> 6;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(6));
  __ xorl(hash, scratch);
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character,
                                            Register scratch) {
  // hash += character;
  __ addl(hash, character);
  // hash += hash << 10;
  __ movl(scratch, hash);
  __ shll(scratch, Immediate(10));
  __ addl(hash, scratch);
  // hash ^= hash >> 6;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(6));
  __ xorl(hash, scratch);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ leal(hash, Operand(hash, hash, times_8, 0));
  // hash ^= hash >> 11;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(11));
  __ xorl(hash, scratch);
  // hash += hash << 15;
  __ movl(scratch, hash);
  __ shll(scratch, Immediate(15));
  __ addl(hash, scratch);

  __ andl(hash, Immediate(String::kHashBitMask));

  // if (hash == 0) hash = 27;
  Label hash_not_zero;
  __ j(not_zero, &hash_not_zero);
  __ Set(hash, StringHasher::kZeroHash);
  __ bind(&hash_not_zero);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]  : return address
  //  rsp[8]  : to
  //  rsp[16] : from
  //  rsp[24] : string

  const int kToOffset = 1 * kPointerSize;
  const int kFromOffset = kToOffset + kPointerSize;
  const int kStringOffset = kFromOffset + kPointerSize;
  const int kArgumentsSize = (kStringOffset + kPointerSize) - kToOffset;

  // Make sure first argument is a string.
  __ movq(rax, Operand(rsp, kStringOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ testl(rax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rax: string
  // rbx: instance type
  // Calculate length of sub string using the smi values.
  __ movq(rcx, Operand(rsp, kToOffset));
  __ movq(rdx, Operand(rsp, kFromOffset));
  __ JumpUnlessBothNonNegativeSmi(rcx, rdx, &runtime);

  __ SmiSub(rcx, rcx, rdx);  // Overflow doesn't happen.
  __ cmpq(rcx, FieldOperand(rax, String::kLengthOffset));
  Label not_original_string;
  // Shorter than original string's length: an actual substring.
  __ j(below, &not_original_string, Label::kNear);
  // Longer than original string's length or negative: unsafe arguments.
  __ j(above, &runtime);
  // Return original string.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);
  __ bind(&not_original_string);

  Label single_char;
  __ SmiCompare(rcx, Smi::FromInt(1));
  __ j(equal, &single_char);

  __ SmiToInteger32(rcx, rcx);

  // rax: string
  // rbx: instance type
  // rcx: sub string length
  // rdx: from index (smi)
  // Deal with different string types: update the index if necessary
  // and put the underlying string into edi.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ testb(rbx, Immediate(kIsIndirectStringMask));
  __ j(zero, &seq_or_external_string, Label::kNear);

  __ testb(rbx, Immediate(kSlicedNotConsMask));
  __ j(not_zero, &sliced_string, Label::kNear);
  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  __ CompareRoot(FieldOperand(rax, ConsString::kSecondOffset),
                 Heap::kempty_stringRootIndex);
  __ j(not_equal, &runtime);
  __ movq(rdi, FieldOperand(rax, ConsString::kFirstOffset));
  // Update instance type.
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ addq(rdx, FieldOperand(rax, SlicedString::kOffsetOffset));
  __ movq(rdi, FieldOperand(rax, SlicedString::kParentOffset));
  // Update instance type.
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the correct register.
  __ movq(rdi, rax);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // rdi: underlying subject string
    // rbx: instance type of underlying subject string
    // rdx: adjusted start index (smi)
    // rcx: length
    // If coming from the make_two_character_string path, the string
    // is too short to be sliced anyways.
    __ cmpq(rcx, Immediate(SlicedString::kMinLength));
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
    __ testb(rbx, Immediate(kStringEncodingMask));
    __ j(zero, &two_byte_slice, Label::kNear);
    __ AllocateAsciiSlicedString(rax, rbx, r14, &runtime);
    __ jmp(&set_slice_header, Label::kNear);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(rax, rbx, r14, &runtime);
    __ bind(&set_slice_header);
    __ Integer32ToSmi(rcx, rcx);
    __ movq(FieldOperand(rax, SlicedString::kLengthOffset), rcx);
    __ movq(FieldOperand(rax, SlicedString::kHashFieldOffset),
           Immediate(String::kEmptyHashField));
    __ movq(FieldOperand(rax, SlicedString::kParentOffset), rdi);
    __ movq(FieldOperand(rax, SlicedString::kOffsetOffset), rdx);
    __ IncrementCounter(counters->sub_string_native(), 1);
    __ ret(kArgumentsSize);

    __ bind(&copy_routine);
  }

  // rdi: underlying subject string
  // rbx: instance type of underlying subject string
  // rdx: adjusted start index (smi)
  // rcx: length
  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label two_byte_sequential, sequential_string;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(rbx, Immediate(kExternalStringTag));
  __ j(zero, &sequential_string);

  // Handle external string.
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ testb(rbx, Immediate(kShortExternalStringMask));
  __ j(not_zero, &runtime);
  __ movq(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ subq(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&sequential_string);
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(zero, &two_byte_sequential);

  // Allocate the result.
  __ AllocateAsciiString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(r14, rsi);  // esi used by following code.
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_1);
    __ lea(rsi, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqOneByteString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqOneByteString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // rdi: first character of result
  // rsi: character of sub string start
  // r14: original value of rsi
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, true);
  __ movq(rsi, r14);  // Restore rsi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);

  __ bind(&two_byte_sequential);
  // Allocate the result.
  __ AllocateTwoByteString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(r14, rsi);  // esi used by following code.
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_2);
    __ lea(rsi, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqOneByteString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqTwoByteString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // rdi: first character of result
  // rsi: character of sub string start
  // r14: original value of rsi
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, false);
  __ movq(rsi, r14);  // Restore esi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);

  __ bind(&single_char);
  // rax: string
  // rbx: instance type
  // rcx: sub string length (smi)
  // rdx: from index (smi)
  StringCharAtGenerator generator(
      rax, rdx, rcx, rax, &runtime, &runtime, &runtime, STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ ret(kArgumentsSize);
  generator.SkipSlow(masm, &runtime);
}


void StringCompareStub::GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                                      Register left,
                                                      Register right,
                                                      Register scratch1,
                                                      Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label check_zero_length;
  __ movq(length, FieldOperand(left, String::kLengthOffset));
  __ SmiCompare(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTest(length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  Label strings_not_equal;
  GenerateAsciiCharsCompareLoop(masm, left, right, length, scratch2,
                                &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Characters are not equal.
  __ bind(&strings_not_equal);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  // Ensure that you can always subtract a string length from a non-negative
  // number (e.g. another length).
  STATIC_ASSERT(String::kMaxLength < 0x7fffffff);

  // Find minimum length and length difference.
  __ movq(scratch1, FieldOperand(left, String::kLengthOffset));
  __ movq(scratch4, scratch1);
  __ SmiSub(scratch4,
            scratch4,
            FieldOperand(right, String::kLengthOffset));
  // Register scratch4 now holds left.length - right.length.
  const Register length_difference = scratch4;
  Label left_shorter;
  __ j(less, &left_shorter, Label::kNear);
  // The right string isn't longer that the left one.
  // Get the right string's length by subtracting the (non-negative) difference
  // from the left string's length.
  __ SmiSub(scratch1, scratch1, length_difference);
  __ bind(&left_shorter);
  // Register scratch1 now holds Min(left.length, right.length).
  const Register min_length = scratch1;

  Label compare_lengths;
  // If min-length is zero, go directly to comparing lengths.
  __ SmiTest(min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare loop.
  Label result_not_equal;
  GenerateAsciiCharsCompareLoop(masm, left, right, min_length, scratch2,
                                &result_not_equal, Label::kNear);

  // Completed loop without finding different characters.
  // Compare lengths (precomputed).
  __ bind(&compare_lengths);
  __ SmiTest(length_difference);
  Label length_not_equal;
  __ j(not_zero, &length_not_equal, Label::kNear);

  // Result is EQUAL.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  Label result_greater;
  Label result_less;
  __ bind(&length_not_equal);
  __ j(greater, &result_greater, Label::kNear);
  __ jmp(&result_less, Label::kNear);
  __ bind(&result_not_equal);
  // Unequal comparison of left to right, either character or length.
  __ j(above, &result_greater, Label::kNear);
  __ bind(&result_less);

  // Result is LESS.
  __ Move(rax, Smi::FromInt(LESS));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(rax, Smi::FromInt(GREATER));
  __ ret(0);
}


void StringCompareStub::GenerateAsciiCharsCompareLoop(
    MacroAssembler* masm,
    Register left,
    Register right,
    Register length,
    Register scratch,
    Label* chars_not_equal,
    Label::Distance near_jump) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiToInteger32(length, length);
  __ lea(left,
         FieldOperand(left, length, times_1, SeqOneByteString::kHeaderSize));
  __ lea(right,
         FieldOperand(right, length, times_1, SeqOneByteString::kHeaderSize));
  __ neg(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ movb(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, near_jump);
  __ incq(index);
  __ j(not_zero, &loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]  : return address
  //  rsp[8]  : right string
  //  rsp[16] : left string

  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rdx, args.GetArgumentOperand(0));  // left
  __ movq(rax, args.GetArgumentOperand(1));  // right

  // Check for identity.
  Label not_same;
  __ cmpq(rdx, rax);
  __ j(not_equal, &not_same, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_compare_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(rdx, rax, rcx, rbx, &runtime);

  // Inline comparison of ASCII strings.
  __ IncrementCounter(counters->string_compare_native(), 1);
  // Drop arguments from the stack
  __ PopReturnAddressTo(rcx);
  __ addq(rsp, Immediate(2 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  GenerateCompareFlatAsciiStrings(masm, rdx, rax, rcx, rbx, rdi, r8);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMI);
  Label miss;
  __ JumpIfNotBothSmi(rdx, rax, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ subq(rax, rdx);
  } else {
    Label done;
    __ subq(rdx, rax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ not_(rdx);
    __ bind(&done);
    __ movq(rax, rdx);
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
    __ JumpIfNotSmi(rdx, &miss);
  }
  if (right_ == CompareIC::SMI) {
    __ JumpIfNotSmi(rax, &miss);
  }

  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(rax, &right_smi, Label::kNear);
  __ CompareMap(rax, masm->isolate()->factory()->heap_number_map(), NULL);
  __ j(not_equal, &maybe_undefined1, Label::kNear);
  __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&left, Label::kNear);
  __ bind(&right_smi);
  __ SmiToInteger32(rcx, rax);  // Can't clobber rax yet.
  __ cvtlsi2sd(xmm1, rcx);

  __ bind(&left);
  __ JumpIfSmi(rdx, &left_smi, Label::kNear);
  __ CompareMap(rdx, masm->isolate()->factory()->heap_number_map(), NULL);
  __ j(not_equal, &maybe_undefined2, Label::kNear);
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  __ jmp(&done);
  __ bind(&left_smi);
  __ SmiToInteger32(rcx, rdx);  // Can't clobber rdx yet.
  __ cvtlsi2sd(xmm0, rcx);

  __ bind(&done);
  // Compare operands
  __ ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  // Return a result of -1, 0, or 1, based on EFLAGS.
  // Performing mov, because xor would destroy the flag register.
  __ movl(rax, Immediate(0));
  __ movl(rcx, Immediate(0));
  __ setcc(above, rax);  // Add one to zero if carry clear and not equal.
  __ sbbq(rax, rcx);  // Subtract one if below (aka. carry set).
  __ ret(0);

  __ bind(&unordered);
  __ bind(&generic_stub);
  ICCompareStub stub(op_, CompareIC::GENERIC, CompareIC::GENERIC,
                     CompareIC::GENERIC);
  __ jmp(stub.GetCode(masm->isolate()), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ Cmp(rax, masm->isolate()->factory()->undefined_value());
    __ j(not_equal, &miss);
    __ JumpIfSmi(rdx, &unordered);
    __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rcx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ Cmp(rdx, masm->isolate()->factory()->undefined_value());
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::INTERNALIZED_STRING);
  ASSERT(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;

  // Check that both operands are heap objects.
  Label miss;
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss, Label::kNear);

  // Check that both operands are internalized strings.
  __ movq(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movq(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbq(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbq(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ or_(tmp1, tmp2);
  __ testb(tmp1, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, &miss, Label::kNear);

  // Internalized strings are compared by identity.
  Label done;
  __ cmpq(left, right);
  // Make sure rax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(rax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateUniqueNames(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::UNIQUE_NAME);
  ASSERT(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;

  // Check that both operands are heap objects.
  Label miss;
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss, Label::kNear);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ movq(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movq(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbq(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbq(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueName(tmp1, &miss, Label::kNear);
  __ JumpIfNotUniqueName(tmp2, &miss, Label::kNear);

  // Unique names are compared by identity.
  Label done;
  __ cmpq(left, right);
  // Make sure rax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(rax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
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
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;
  Register tmp3 = rdi;

  // Check that both operands are heap objects.
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ movq(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movq(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbq(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbq(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ movq(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ or_(tmp3, tmp2);
  __ testb(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmpq(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We also know they are both
  // strings.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag == 0);
    __ or_(tmp1, tmp2);
    __ testb(tmp1, Immediate(kIsNotInternalizedMask));
    __ j(not_zero, &do_compare, Label::kNear);
    // Make sure rax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    ASSERT(right.is(rax));
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
        masm, left, right, tmp1, tmp2, tmp3, kScratchRegister);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ PopReturnAddressTo(tmp1);
  __ push(left);
  __ push(right);
  __ PushReturnAddressFrom(tmp1);
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
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  __ CmpObjectType(rax, JS_OBJECT_TYPE, rcx);
  __ j(not_equal, &miss, Label::kNear);
  __ CmpObjectType(rdx, JS_OBJECT_TYPE, rcx);
  __ j(not_equal, &miss, Label::kNear);

  ASSERT(GetCondition() == equal);
  __ subq(rax, rdx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateKnownObjects(MacroAssembler* masm) {
  Label miss;
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  __ movq(rcx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ Cmp(rcx, known_map_);
  __ j(not_equal, &miss, Label::kNear);
  __ Cmp(rbx, known_map_);
  __ j(not_equal, &miss, Label::kNear);

  __ subq(rax, rdx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    ExternalReference miss =
        ExternalReference(IC_Utility(IC::kCompareIC_Miss), masm->isolate());

    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(rdx);
    __ push(rax);
    __ push(rdx);
    __ push(rax);
    __ Push(Smi::FromInt(op_));
    __ CallExternalReference(miss, 3);

    // Compute the entry point of the rewritten stub.
    __ lea(rdi, FieldOperand(rax, Code::kHeaderSize));
    __ pop(rax);
    __ pop(rdx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(rdi);
}


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
    // r0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ SmiToInteger32(index, FieldOperand(properties, kCapacityOffset));
    __ decl(index);
    __ and_(index,
            Immediate(name->Hash() + NameDictionary::GetProbeOffset(i)));

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.

    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    __ movq(entity_name, Operand(properties,
                                 index,
                                 times_pointer_size,
                                 kElementsStartOffset - kHeapObjectTag));
    __ Cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ Cmp(entity_name, Handle<Name>(name));
    __ j(equal, miss);

    Label good;
    // Check for the hole and skip.
    __ CompareRoot(entity_name, Heap::kTheHoleValueRootIndex);
    __ j(equal, &good, Label::kNear);

    // Check if the entry name is not a unique name.
    __ movq(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ JumpIfNotUniqueName(FieldOperand(entity_name, Map::kInstanceTypeOffset),
                           miss);
    __ bind(&good);
  }

  NameDictionaryLookupStub stub(properties, r0, r0, NEGATIVE_LOOKUP);
  __ Push(Handle<Object>(name));
  __ push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ testq(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found leaving the
// index into the dictionary in |r1|. Jump to the |miss| label
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

  __ SmiToInteger32(r0, FieldOperand(elements, kCapacityOffset));
  __ decl(r0);

  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movl(r1, FieldOperand(name, Name::kHashFieldOffset));
    __ shrl(r1, Immediate(Name::kHashShift));
    if (i > 0) {
      __ addl(r1, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(r1, r0);

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(r1, Operand(r1, r1, times_2, 0));  // r1 = r1 * 3

    // Check if the key is identical to the name.
    __ cmpq(name, Operand(elements, r1, times_pointer_size,
                          kElementsStartOffset - kHeapObjectTag));
    __ j(equal, done);
  }

  NameDictionaryLookupStub stub(elements, r0, r1, POSITIVE_LOOKUP);
  __ push(name);
  __ movl(r0, FieldOperand(name, Name::kHashFieldOffset));
  __ shrl(r0, Immediate(Name::kHashShift));
  __ push(r0);
  __ CallStub(&stub);

  __ testq(r0, r0);
  __ j(zero, miss);
  __ jmp(done);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  rsp[0 * kPointerSize] : return address.
  //  rsp[1 * kPointerSize] : key's hash.
  //  rsp[2 * kPointerSize] : key.
  // Registers:
  //  dictionary_: NameDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result_;

  __ SmiToInteger32(scratch, FieldOperand(dictionary_, kCapacityOffset));
  __ decl(scratch);
  __ push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER,
                              kPointerSize);
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movq(scratch, args.GetArgumentOperand(1));
    if (i > 0) {
      __ addl(scratch, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(rsp, 0));

    // Scale the index by multiplying by the entry size.
    ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index_, Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    __ movq(scratch, Operand(dictionary_,
                             index_,
                             times_pointer_size,
                             kElementsStartOffset - kHeapObjectTag));

    __ Cmp(scratch, masm->isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmpq(scratch, args.GetArgumentOperand(0));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode_ == NEGATIVE_LOOKUP) {
      // If we hit a key that is not a unique name during negative
      // lookup we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a unique name.
      __ movq(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ JumpIfNotUniqueName(FieldOperand(scratch, Map::kInstanceTypeOffset),
                             &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode_ == POSITIVE_LOOKUP) {
    __ movq(scratch, Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ movq(scratch, Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ movq(scratch, Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


struct AheadOfTimeWriteBarrierStubList {
  Register object, value, address;
  RememberedSetAction action;
};


#define REG(Name) { kRegister_ ## Name ## _Code }

struct AheadOfTimeWriteBarrierStubList kAheadOfTime[] = {
  // Used in RegExpExecStub.
  { REG(rbx), REG(rax), REG(rdi), EMIT_REMEMBERED_SET },
  // Used in CompileArrayPushCall.
  { REG(rbx), REG(rcx), REG(rdx), EMIT_REMEMBERED_SET },
  // Used in StoreStubCompiler::CompileStoreField and
  // KeyedStoreStubCompiler::CompileStoreField via GenerateStoreField.
  { REG(rdx), REG(rcx), REG(rbx), EMIT_REMEMBERED_SET },
  // GenerateStoreField calls the stub with two different permutations of
  // registers.  This is the second.
  { REG(rbx), REG(rcx), REG(rdx), EMIT_REMEMBERED_SET },
  // StoreIC::GenerateNormal via GenerateDictionaryStore.
  { REG(rbx), REG(r8), REG(r9), EMIT_REMEMBERED_SET },
  // KeyedStoreIC::GenerateGeneric.
  { REG(rbx), REG(rdx), REG(rcx), EMIT_REMEMBERED_SET},
  // KeyedStoreStubCompiler::GenerateStoreFastElement.
  { REG(rdi), REG(rbx), REG(rcx), EMIT_REMEMBERED_SET},
  { REG(rdx), REG(rdi), REG(rbx), EMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateMapChangeElementTransition
  // and ElementsTransitionGenerator::GenerateSmiToDouble
  // and ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(rdx), REG(rbx), REG(rdi), EMIT_REMEMBERED_SET},
  { REG(rdx), REG(rbx), REG(rdi), OMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateSmiToDouble
  // and ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(rdx), REG(r11), REG(r15), EMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(r11), REG(rax), REG(r15), EMIT_REMEMBERED_SET},
  // StoreArrayLiteralElementStub::Generate
  { REG(rbx), REG(rax), REG(rcx), EMIT_REMEMBERED_SET},
  // FastNewClosureStub::Generate and
  // StringAddStub::Generate
  { REG(rcx), REG(rdx), REG(rbx), EMIT_REMEMBERED_SET},
  // StringAddStub::Generate
  { REG(rcx), REG(rax), REG(rbx), EMIT_REMEMBERED_SET},
  // Null termination.
  { REG(no_reg), REG(no_reg), REG(no_reg), EMIT_REMEMBERED_SET}
};

#undef REG

bool RecordWriteStub::IsPregenerated(Isolate* isolate) {
  for (AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
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
  StoreBufferOverflowStub stub1(kDontSaveFPRegs);
  stub1.GetCode(isolate)->set_is_pregenerated(true);
  StoreBufferOverflowStub stub2(kSaveFPRegs);
  stub2.GetCode(isolate)->set_is_pregenerated(true);
}


void RecordWriteStub::GenerateFixedRegStubsAheadOfTime(Isolate* isolate) {
  for (AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
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
  return true;  // Always have SSE2 on x64.
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
  // See RecordWriteStub::Patch for details.
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

    __ movq(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),
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
        masm, kUpdateRememberedSetOnNoNeedToInformIncrementalMarker, mode);
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
      masm, kReturnOnNoNeedToInformIncrementalMarker, mode);
  InformIncrementalMarker(masm, mode);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm, Mode mode) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode_);
  Register address =
      arg_reg_1.is(regs_.address()) ? kScratchRegister : regs_.address();
  ASSERT(!address.is(regs_.object()));
  ASSERT(!address.is(arg_reg_1));
  __ Move(address, regs_.address());
  __ Move(arg_reg_1, regs_.object());
  // TODO(gc) Can we just set address arg2 in the beginning?
  __ Move(arg_reg_2, address);
  __ LoadAddress(arg_reg_3,
                 ExternalReference::isolate_address(masm->isolate()));
  int argument_count = 3;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count);
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
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_object;

  __ movq(regs_.scratch0(), Immediate(~Page::kPageAlignmentMask));
  __ and_(regs_.scratch0(), regs_.object());
  __ movq(regs_.scratch1(),
         Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset));
  __ subq(regs_.scratch1(), Immediate(1));
  __ movq(Operand(regs_.scratch0(),
                 MemoryChunk::kWriteBarrierCounterOffset),
         regs_.scratch1());
  __ j(negative, &need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &on_black,
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

  __ bind(&on_black);

  // Get the value from the slot.
  __ movq(regs_.scratch0(), Operand(regs_.address(), 0));

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
                     zero,
                     &need_incremental);

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
  //  -- rax     : element value to store
  //  -- rcx     : element index as smi
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : array literal index in function
  //  -- rsp[16] : array literal
  // clobbers rbx, rdx, rdi
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label fast_elements;

  // Get array literal index, array literal and its map.
  StackArgumentsAccessor args(rsp, 2, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rdx, args.GetArgumentOperand(1));
  __ movq(rbx, args.GetArgumentOperand(0));
  __ movq(rdi, FieldOperand(rbx, JSObject::kMapOffset));

  __ CheckFastElements(rdi, &double_elements);

  // FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS
  __ JumpIfSmi(rax, &smi_element);
  __ CheckFastSmiElements(rdi, &fast_elements);

  // Store into the array literal requires a elements transition. Call into
  // the runtime.

  __ bind(&slow_elements);
  __ PopReturnAddressTo(rdi);
  __ push(rbx);
  __ push(rcx);
  __ push(rax);
  __ movq(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rbx, JSFunction::kLiteralsOffset));
  __ push(rdx);
  __ PushReturnAddressFrom(rdi);
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  // Array literal has ElementsKind of FAST_*_ELEMENTS and value is an object.
  __ bind(&fast_elements);
  __ SmiToInteger32(kScratchRegister, rcx);
  __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
  __ lea(rcx, FieldOperand(rbx, kScratchRegister, times_pointer_size,
                           FixedArrayBase::kHeaderSize));
  __ movq(Operand(rcx, 0), rax);
  // Update the write barrier for the array store.
  __ RecordWrite(rbx, rcx, rax,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ ret(0);

  // Array literal has ElementsKind of FAST_*_SMI_ELEMENTS or
  // FAST_*_ELEMENTS, and value is Smi.
  __ bind(&smi_element);
  __ SmiToInteger32(kScratchRegister, rcx);
  __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
  __ movq(FieldOperand(rbx, kScratchRegister, times_pointer_size,
                       FixedArrayBase::kHeaderSize), rax);
  __ ret(0);

  // Array literal has ElementsKind of FAST_DOUBLE_ELEMENTS.
  __ bind(&double_elements);

  __ movq(r9, FieldOperand(rbx, JSObject::kElementsOffset));
  __ SmiToInteger32(r11, rcx);
  __ StoreNumberToDoubleElements(rax,
                                 r9,
                                 r11,
                                 xmm0,
                                 &slow_elements);
  __ ret(0);
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(1, fp_registers_ ? kSaveFPRegs : kDontSaveFPRegs);
  __ Call(ces.GetCode(masm->isolate()), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ movq(rbx, MemOperand(rbp, parameter_count_offset));
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ PopReturnAddressTo(rcx);
  int additional_offset = function_mode_ == JS_FUNCTION_STUB_MODE
      ? kPointerSize
      : 0;
  __ lea(rsp, MemOperand(rsp, rbx, times_pointer_size, additional_offset));
  __ jmp(rcx);  // Return to IC Miss stub, continuation still on stack.
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    // It's always safe to call the entry hook stub, as the hook itself
    // is not allowed to call back to V8.
    AllowStubCallsScope allow_stub_calls(masm, true);

    ProfileEntryHookStub stub;
    masm->CallStub(&stub);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // This stub can be called from essentially anywhere, so it needs to save
  // all volatile and callee-save registers.
  const size_t kNumSavedRegisters = 2;
  __ push(arg_reg_1);
  __ push(arg_reg_2);

  // Calculate the original stack pointer and store it in the second arg.
  __ lea(arg_reg_2, Operand(rsp, (kNumSavedRegisters + 1) * kPointerSize));

  // Calculate the function address to the first arg.
  __ movq(arg_reg_1, Operand(rsp, kNumSavedRegisters * kPointerSize));
  __ subq(arg_reg_1, Immediate(Assembler::kShortCallInstructionLength));

  // Save the remainder of the volatile registers.
  masm->PushCallerSaved(kSaveFPRegs, arg_reg_1, arg_reg_2);

  // Call the entry hook function.
  __ movq(rax, FUNCTION_ADDR(masm->isolate()->function_entry_hook()),
          RelocInfo::NONE64);

  AllowExternalCallThatCantCauseGC scope(masm);

  const int kArgumentCount = 2;
  __ PrepareCallCFunction(kArgumentCount);
  __ CallCFunction(rax, kArgumentCount);

  // Restore volatile regs.
  masm->PopCallerSaved(kSaveFPRegs, arg_reg_1, arg_reg_2);
  __ pop(arg_reg_2);
  __ pop(arg_reg_1);

  __ Ret();
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(GetInitialFastElementsKind(),
           CONTEXT_CHECK_REQUIRED,
           mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmpl(rdx, Immediate(kind));
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
  // rbx - type info cell (if mode != DISABLE_ALLOCATION_SITES)
  // rdx - kind (if mode != DISABLE_ALLOCATION_SITES)
  // rax - number of arguments
  // rdi - constructor?
  // rsp[0] - return address
  // rsp[8] - last argument
  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());

  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    ASSERT(FAST_SMI_ELEMENTS == 0);
    ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    ASSERT(FAST_ELEMENTS == 2);
    ASSERT(FAST_HOLEY_ELEMENTS == 3);
    ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ testb(rdx, Immediate(1));
    __ j(not_zero, &normal_sequence);
  }

  // look at the first argument
  StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
  __ movq(rcx, args.GetArgumentOperand(0));
  __ testq(rcx, rcx);
  __ j(zero, &normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(holey_initial,
                                                  CONTEXT_CHECK_REQUIRED,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(initial,
                                            CONTEXT_CHECK_REQUIRED,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the cell).
    __ incl(rdx);
    __ movq(rcx, FieldOperand(rbx, Cell::kValueOffset));
    if (FLAG_debug_code) {
      Handle<Map> allocation_site_map(
          masm->isolate()->heap()->allocation_site_map(),
          masm->isolate());
      __ Cmp(FieldOperand(rcx, 0), allocation_site_map);
      __ Assert(equal, kExpectedAllocationSiteInCell);
    }

    // Save the resulting elements kind in type info
    __ Integer32ToSmi(rdx, rdx);
    __ movq(FieldOperand(rcx, AllocationSite::kTransitionInfoOffset), rdx);
    __ SmiToInteger32(rdx, rdx);

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmpl(rdx, Immediate(kind));
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
  ElementsKind initial_kind = GetInitialFastElementsKind();
  ElementsKind initial_holey_kind = GetHoleyElementsKind(initial_kind);

  int to_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(kind);
    stub.GetCode(isolate)->set_is_pregenerated(true);
    if (AllocationSite::GetMode(kind) != DONT_TRACK_ALLOCATION_SITE ||
        (!FLAG_track_allocation_sites &&
         (kind == initial_kind || kind == initial_holey_kind))) {
      T stub1(kind, CONTEXT_CHECK_REQUIRED, DISABLE_ALLOCATION_SITES);
      stub1.GetCode(isolate)->set_is_pregenerated(true);
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
    stubh1.GetCode(isolate)->set_is_pregenerated(true);
    InternalArraySingleArgumentConstructorStub stubh2(kinds[i]);
    stubh2.GetCode(isolate)->set_is_pregenerated(true);
    InternalArrayNArgumentsConstructorStub stubh3(kinds[i]);
    stubh3.GetCode(isolate)->set_is_pregenerated(true);
  }
}


void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  if (argument_count_ == ANY) {
    Label not_zero_case, not_one_case;
    __ testq(rax, rax);
    __ j(not_zero, &not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ cmpl(rax, Immediate(1));
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
  //  -- rax    : argc
  //  -- rbx    : type info cell
  //  -- rdi    : constructor
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Handle<Object> undefined_sentinel(
      masm->isolate()->heap()->undefined_value(),
      masm->isolate());

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ movq(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rcx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rcx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in rbx or a valid cell
    Label okay_here;
    Handle<Map> cell_map = masm->isolate()->factory()->cell_map();
    __ Cmp(rbx, undefined_sentinel);
    __ j(equal, &okay_here);
    __ Cmp(FieldOperand(rbx, 0), cell_map);
    __ Assert(equal, kExpectedPropertyCellInRegisterRbx);
    __ bind(&okay_here);
  }

  Label no_info;
  // If the type cell is undefined, or contains anything other than an
  // AllocationSite, call an array constructor that doesn't use AllocationSites.
  __ Cmp(rbx, undefined_sentinel);
  __ j(equal, &no_info);
  __ movq(rdx, FieldOperand(rbx, Cell::kValueOffset));
  __ Cmp(FieldOperand(rdx, 0),
         Handle<Map>(masm->isolate()->heap()->allocation_site_map()));
  __ j(not_equal, &no_info);

  __ movq(rdx, FieldOperand(rdx, AllocationSite::kTransitionInfoOffset));
  __ SmiToInteger32(rdx, rdx);
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ testq(rax, rax);
  __ j(not_zero, &not_zero_case);
  InternalArrayNoArgumentConstructorStub stub0(kind);
  __ TailCallStub(&stub0);

  __ bind(&not_zero_case);
  __ cmpl(rax, Immediate(1));
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    StackArgumentsAccessor args(rsp, 1, ARGUMENTS_DONT_CONTAIN_RECEIVER);
    __ movq(rcx, args.GetArgumentOperand(0));
    __ testq(rcx, rcx);
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
  //  -- rax    : argc
  //  -- rbx    : type info cell
  //  -- rdi    : constructor
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ movq(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rcx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rcx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ movq(rcx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ movzxbq(rcx, FieldOperand(rcx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ and_(rcx, Immediate(Map::kElementsKindMask));
  __ shr(rcx, Immediate(Map::kElementsKindShift));

  if (FLAG_debug_code) {
    Label done;
    __ cmpl(rcx, Immediate(FAST_ELEMENTS));
    __ j(equal, &done);
    __ cmpl(rcx, Immediate(FAST_HOLEY_ELEMENTS));
    __ Assert(equal,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmpl(rcx, Immediate(FAST_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
