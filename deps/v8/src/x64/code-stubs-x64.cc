// Copyright 2010 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "bootstrapper.h"
#include "code-stubs.h"
#include "regexp-macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)
void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in rsi.
  Label gc;
  __ AllocateInNewSpace(JSFunction::kSize, rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function info from the stack.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ movq(rcx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalContextOffset));
  __ movq(rcx, Operand(rcx, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ movq(FieldOperand(rax, JSObject::kMapOffset), rcx);

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ LoadRoot(rbx, Heap::kEmptyFixedArrayRootIndex);
  __ LoadRoot(rcx, Heap::kTheHoleValueRootIndex);
  __ movq(FieldOperand(rax, JSObject::kPropertiesOffset), rbx);
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rbx);
  __ movq(FieldOperand(rax, JSFunction::kPrototypeOrInitialMapOffset), rcx);
  __ movq(FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset), rdx);
  __ movq(FieldOperand(rax, JSFunction::kContextOffset), rsi);
  __ movq(FieldOperand(rax, JSFunction::kLiteralsOffset), rbx);

  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  __ movq(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
  __ lea(rdx, FieldOperand(rdx, Code::kHeaderSize));
  __ movq(FieldOperand(rax, JSFunction::kCodeEntryOffset), rdx);


  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(rcx);  // Temporarily remove return address.
  __ pop(rdx);
  __ push(rsi);
  __ push(rdx);
  __ push(rcx);  // Restore return address.
  __ TailCallRuntime(Runtime::kNewClosure, 2, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace((length * kPointerSize) + FixedArray::kHeaderSize,
                        rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));

  // Setup the object header.
  __ LoadRoot(kScratchRegister, Heap::kContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ Move(FieldOperand(rax, FixedArray::kLengthOffset), Smi::FromInt(length));

  // Setup the fixed slots.
  __ xor_(rbx, rbx);  // Set to NULL.
  __ movq(Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)), rcx);
  __ movq(Operand(rax, Context::SlotOffset(Context::FCONTEXT_INDEX)), rax);
  __ movq(Operand(rax, Context::SlotOffset(Context::PREVIOUS_INDEX)), rbx);
  __ movq(Operand(rax, Context::SlotOffset(Context::EXTENSION_INDEX)), rbx);

  // Copy the global object from the surrounding context.
  __ movq(rbx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(Operand(rax, Context::SlotOffset(Context::GLOBAL_INDEX)), rbx);

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
  __ TailCallRuntime(Runtime::kNewContext, 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + kPointerSize]: constant elements.
  // [rsp + (2 * kPointerSize)]: literal index.
  // [rsp + (3 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into rcx and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ movq(rcx, Operand(rsp, 3 * kPointerSize));
  __ movq(rax, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rcx,
          FieldOperand(rcx, index.reg, index.scale, FixedArray::kHeaderSize));
  __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
  __ j(equal, &slow_case);

  if (FLAG_debug_code) {
    const char* message;
    Heap::RootListIndex expected_map_index;
    if (mode_ == CLONE_ELEMENTS) {
      message = "Expected (writable) fixed array";
      expected_map_index = Heap::kFixedArrayMapRootIndex;
    } else {
      ASSERT(mode_ == COPY_ON_WRITE_ELEMENTS);
      message = "Expected copy-on-write fixed array";
      expected_map_index = Heap::kFixedCOWArrayMapRootIndex;
    }
    __ push(rcx);
    __ movq(rcx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                   expected_map_index);
    __ Assert(equal, message);
    __ pop(rcx);
  }

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size, rax, rbx, rdx, &slow_case, TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ movq(rbx, FieldOperand(rcx, i));
      __ movq(FieldOperand(rax, i), rbx);
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ movq(rcx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ lea(rdx, Operand(rax, JSArray::kSize));
    __ movq(FieldOperand(rax, JSArray::kElementsOffset), rdx);

    // Copy the elements array.
    for (int i = 0; i < elements_size; i += kPointerSize) {
      __ movq(rbx, FieldOperand(rcx, i));
      __ movq(FieldOperand(rdx, i), rbx);
    }
  }

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateArrayLiteralShallow, 3, 1);
}


void ToBooleanStub::Generate(MacroAssembler* masm) {
  Label false_result, true_result, not_string;
  __ movq(rax, Operand(rsp, 1 * kPointerSize));

  // 'null' => false.
  __ CompareRoot(rax, Heap::kNullValueRootIndex);
  __ j(equal, &false_result);

  // Get the map and type of the heap object.
  // We don't use CmpObjectType because we manipulate the type field.
  __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbq(rcx, FieldOperand(rdx, Map::kInstanceTypeOffset));

  // Undetectable => false.
  __ movzxbq(rbx, FieldOperand(rdx, Map::kBitFieldOffset));
  __ and_(rbx, Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, &false_result);

  // JavaScript object => true.
  __ cmpq(rcx, Immediate(FIRST_JS_OBJECT_TYPE));
  __ j(above_equal, &true_result);

  // String value => false iff empty.
  __ cmpq(rcx, Immediate(FIRST_NONSTRING_TYPE));
  __ j(above_equal, &not_string);
  __ movq(rdx, FieldOperand(rax, String::kLengthOffset));
  __ SmiTest(rdx);
  __ j(zero, &false_result);
  __ jmp(&true_result);

  __ bind(&not_string);
  __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &true_result);
  // HeapNumber => false iff +0, -0, or NaN.
  // These three cases set the zero flag when compared to zero using ucomisd.
  __ xorpd(xmm0, xmm0);
  __ ucomisd(xmm0, FieldOperand(rax, HeapNumber::kValueOffset));
  __ j(zero, &false_result);
  // Fall through to |true_result|.

  // Return 1/0 for true/false in rax.
  __ bind(&true_result);
  __ movq(rax, Immediate(1));
  __ ret(1 * kPointerSize);
  __ bind(&false_result);
  __ xor_(rax, rax);
  __ ret(1 * kPointerSize);
}


const char* GenericBinaryOpStub::GetName() {
  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "GenericBinaryOpStub_%s_%s%s_%s%s_%s_%s",
               op_name,
               overwrite_name,
               (flags_ & NO_SMI_CODE_IN_STUB) ? "_NoSmiInStub" : "",
               args_in_registers_ ? "RegArgs" : "StackArgs",
               args_reversed_ ? "_R" : "",
               static_operands_type_.ToString(),
               BinaryOpIC::GetName(runtime_operands_type_));
  return name_;
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Register left,
    Register right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ push(left);
    __ push(right);
  } else {
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (!(left.is(left_arg) && right.is(right_arg))) {
      if (left.is(right_arg) && right.is(left_arg)) {
        if (IsOperationCommutative()) {
          SetArgsReversed();
        } else {
          __ xchg(left, right);
        }
      } else if (left.is(left_arg)) {
        __ movq(right_arg, right);
      } else if (right.is(right_arg)) {
        __ movq(left_arg, left);
      } else if (left.is(right_arg)) {
        if (IsOperationCommutative()) {
          __ movq(left_arg, right);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying left argument.
          __ movq(left_arg, left);
          __ movq(right_arg, right);
        }
      } else if (right.is(left_arg)) {
        if (IsOperationCommutative()) {
          __ movq(right_arg, left);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying right argument.
          __ movq(right_arg, right);
          __ movq(left_arg, left);
        }
      } else {
        // Order of moves is not important.
        __ movq(left_arg, left);
        __ movq(right_arg, right);
      }
    }

    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Register left,
    Smi* right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ push(left);
    __ Push(right);
  } else {
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (left.is(left_arg)) {
      __ Move(right_arg, right);
    } else if (left.is(right_arg) && IsOperationCommutative()) {
      __ Move(left_arg, right);
      SetArgsReversed();
    } else {
      // For non-commutative operations, left and right_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite left before moving
      // it to left_arg.
      __ movq(left_arg, left);
      __ Move(right_arg, right);
    }

    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


void GenericBinaryOpStub::GenerateCall(
    MacroAssembler* masm,
    Smi* left,
    Register right) {
  if (!ArgsInRegistersSupported()) {
    // Pass arguments on the stack.
    __ Push(left);
    __ push(right);
  } else {
    // The calling convention with registers is left in rdx and right in rax.
    Register left_arg = rdx;
    Register right_arg = rax;
    if (right.is(right_arg)) {
      __ Move(left_arg, left);
    } else if (right.is(left_arg) && IsOperationCommutative()) {
      __ Move(right_arg, left);
      SetArgsReversed();
    } else {
      // For non-commutative operations, right and left_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite right before moving
      // it to right_arg.
      __ movq(right_arg, right);
      __ Move(left_arg, left);
    }
    // Update flags to indicate that arguments are in registers.
    SetArgsInRegisters();
    __ IncrementCounter(&Counters::generic_binary_stub_calls_regs, 1);
  }

  // Call the stub.
  __ CallStub(this);
}


class FloatingPointHelper : public AllStatic {
 public:
  // Load the operands from rdx and rax into xmm0 and xmm1, as doubles.
  // If the operands are not both numbers, jump to not_numbers.
  // Leaves rdx and rax unchanged.  SmiOperands assumes both are smis.
  // NumberOperands assumes both are smis or heap numbers.
  static void LoadSSE2SmiOperands(MacroAssembler* masm);
  static void LoadSSE2NumberOperands(MacroAssembler* masm);
  static void LoadSSE2UnknownOperands(MacroAssembler* masm,
                                      Label* not_numbers);

  // Takes the operands in rdx and rax and loads them as integers in rax
  // and rcx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             Label* operand_conversion_failure,
                             Register heap_number_map);
  // As above, but we know the operands to be numbers. In that case,
  // conversion can't fail.
  static void LoadNumbersAsIntegers(MacroAssembler* masm);
};


void GenericBinaryOpStub::GenerateSmiCode(MacroAssembler* masm, Label* slow) {
  // 1. Move arguments into rdx, rax except for DIV and MOD, which need the
  // dividend in rax and rdx free for the division.  Use rax, rbx for those.
  Comment load_comment(masm, "-- Load arguments");
  Register left = rdx;
  Register right = rax;
  if (op_ == Token::DIV || op_ == Token::MOD) {
    left = rax;
    right = rbx;
    if (HasArgsInRegisters()) {
      __ movq(rbx, rax);
      __ movq(rax, rdx);
    }
  }
  if (!HasArgsInRegisters()) {
    __ movq(right, Operand(rsp, 1 * kPointerSize));
    __ movq(left, Operand(rsp, 2 * kPointerSize));
  }

  Label not_smis;
  // 2. Smi check both operands.
  if (static_operands_type_.IsSmi()) {
    // Skip smi check if we know that both arguments are smis.
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(left);
      __ AbortIfNotSmi(right);
    }
    if (op_ == Token::BIT_OR) {
      // Handle OR here, since we do extra smi-checking in the or code below.
      __ SmiOr(right, right, left);
      GenerateReturn(masm);
      return;
    }
  } else {
    if (op_ != Token::BIT_OR) {
      // Skip the check for OR as it is better combined with the
      // actual operation.
      Comment smi_check_comment(masm, "-- Smi check arguments");
      __ JumpIfNotBothSmi(left, right, &not_smis);
    }
  }

  // 3. Operands are both smis (except for OR), perform the operation leaving
  // the result in rax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op_) {
    case Token::ADD: {
      ASSERT(right.is(rax));
      __ SmiAdd(right, right, left, &use_fp_on_smis);  // ADD is commutative.
      break;
    }

    case Token::SUB: {
      __ SmiSub(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;
    }

    case Token::MUL:
      ASSERT(right.is(rax));
      __ SmiMul(right, right, left, &use_fp_on_smis);  // MUL is commutative.
      break;

    case Token::DIV:
      ASSERT(left.is(rax));
      __ SmiDiv(left, left, right, &use_fp_on_smis);
      break;

    case Token::MOD:
      ASSERT(left.is(rax));
      __ SmiMod(left, left, right, slow);
      break;

    case Token::BIT_OR:
      ASSERT(right.is(rax));
      __ movq(rcx, right);  // Save the right operand.
      __ SmiOr(right, right, left);  // BIT_OR is commutative.
      __ testb(right, Immediate(kSmiTagMask));
      __ j(not_zero, &not_smis);
      break;

    case Token::BIT_AND:
      ASSERT(right.is(rax));
      __ SmiAnd(right, right, left);  // BIT_AND is commutative.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(rax));
      __ SmiXor(right, right, left);  // BIT_XOR is commutative.
      break;

    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      switch (op_) {
        case Token::SAR:
          __ SmiShiftArithmeticRight(left, left, right);
          break;
        case Token::SHR:
          __ SmiShiftLogicalRight(left, left, right, slow);
          break;
        case Token::SHL:
          __ SmiShiftLeft(left, left, right);
          break;
        default:
          UNREACHABLE();
      }
      __ movq(rax, left);
      break;

    default:
      UNREACHABLE();
      break;
  }

  // 4. Emit return of result in rax.
  GenerateReturn(masm);

  // 5. For some operations emit inline code to perform floating point
  // operations on known smis (e.g., if the result of the operation
  // overflowed the smi range).
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      ASSERT(use_fp_on_smis.is_linked());
      __ bind(&use_fp_on_smis);
      if (op_ == Token::DIV) {
        __ movq(rdx, rax);
        __ movq(rax, rbx);
      }
      // left is rdx, right is rax.
      __ AllocateHeapNumber(rbx, rcx, slow);
      FloatingPointHelper::LoadSSE2SmiOperands(masm);
      switch (op_) {
        case Token::ADD: __ addsd(xmm0, xmm1); break;
        case Token::SUB: __ subsd(xmm0, xmm1); break;
        case Token::MUL: __ mulsd(xmm0, xmm1); break;
        case Token::DIV: __ divsd(xmm0, xmm1); break;
        default: UNREACHABLE();
      }
      __ movsd(FieldOperand(rbx, HeapNumber::kValueOffset), xmm0);
      __ movq(rax, rbx);
      GenerateReturn(masm);
    }
    default:
      break;
  }

  // 6. Non-smi operands, fall out to the non-smi code with the operands in
  // rdx and rax.
  Comment done_comment(masm, "-- Enter non-smi code");
  __ bind(&not_smis);

  switch (op_) {
    case Token::DIV:
    case Token::MOD:
      // Operands are in rax, rbx at this point.
      __ movq(rdx, rax);
      __ movq(rax, rbx);
      break;

    case Token::BIT_OR:
      // Right operand is saved in rcx and rax was destroyed by the smi
      // operation.
      __ movq(rax, rcx);
      break;

    default:
      break;
  }
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  Label call_runtime;

  if (ShouldGenerateSmiCode()) {
    GenerateSmiCode(masm, &call_runtime);
  } else if (op_ != Token::MOD) {
    if (!HasArgsInRegisters()) {
      GenerateLoadArguments(masm);
    }
  }
  // Floating point case.
  if (ShouldGenerateFPCode()) {
    switch (op_) {
      case Token::ADD:
      case Token::SUB:
      case Token::MUL:
      case Token::DIV: {
        if (runtime_operands_type_ == BinaryOpIC::DEFAULT &&
            HasSmiCodeInStub()) {
          // Execution reaches this point when the first non-smi argument occurs
          // (and only if smi code is generated). This is the right moment to
          // patch to HEAP_NUMBERS state. The transition is attempted only for
          // the four basic operations. The stub stays in the DEFAULT state
          // forever for all other operations (also if smi code is skipped).
          GenerateTypeTransition(masm);
          break;
        }

        Label not_floats;
        // rax: y
        // rdx: x
        if (static_operands_type_.IsNumber()) {
          if (FLAG_debug_code) {
            // Assert at runtime that inputs are only numbers.
            __ AbortIfNotNumber(rdx);
            __ AbortIfNotNumber(rax);
          }
          FloatingPointHelper::LoadSSE2NumberOperands(masm);
        } else {
          FloatingPointHelper::LoadSSE2UnknownOperands(masm, &call_runtime);
        }

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        // Allocate a heap number, if needed.
        Label skip_allocation;
        OverwriteMode mode = mode_;
        if (HasArgsReversed()) {
          if (mode == OVERWRITE_RIGHT) {
            mode = OVERWRITE_LEFT;
          } else if (mode == OVERWRITE_LEFT) {
            mode = OVERWRITE_RIGHT;
          }
        }
        switch (mode) {
          case OVERWRITE_LEFT:
            __ JumpIfNotSmi(rdx, &skip_allocation);
            __ AllocateHeapNumber(rbx, rcx, &call_runtime);
            __ movq(rdx, rbx);
            __ bind(&skip_allocation);
            __ movq(rax, rdx);
            break;
          case OVERWRITE_RIGHT:
            // If the argument in rax is already an object, we skip the
            // allocation of a heap number.
            __ JumpIfNotSmi(rax, &skip_allocation);
            // Fall through!
          case NO_OVERWRITE:
            // Allocate a heap number for the result. Keep rax and rdx intact
            // for the possible runtime call.
            __ AllocateHeapNumber(rbx, rcx, &call_runtime);
            __ movq(rax, rbx);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
        GenerateReturn(masm);
        __ bind(&not_floats);
        if (runtime_operands_type_ == BinaryOpIC::DEFAULT &&
            !HasSmiCodeInStub()) {
            // Execution reaches this point when the first non-number argument
            // occurs (and only if smi code is skipped from the stub, otherwise
            // the patching has already been done earlier in this case branch).
            // A perfect moment to try patching to STRINGS for ADD operation.
            if (op_ == Token::ADD) {
              GenerateTypeTransition(masm);
            }
        }
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
        Label skip_allocation, non_smi_shr_result;
        Register heap_number_map = r9;
        __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
        if (static_operands_type_.IsNumber()) {
          if (FLAG_debug_code) {
            // Assert at runtime that inputs are only numbers.
            __ AbortIfNotNumber(rdx);
            __ AbortIfNotNumber(rax);
          }
          FloatingPointHelper::LoadNumbersAsIntegers(masm);
        } else {
          FloatingPointHelper::LoadAsIntegers(masm,
                                              &call_runtime,
                                              heap_number_map);
        }
        switch (op_) {
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
        GenerateReturn(masm);

        // All bit-ops except SHR return a signed int32 that can be
        // returned immediately as a smi.
        // We might need to allocate a HeapNumber if we shift a negative
        // number right by zero (i.e., convert to UInt32).
        if (op_ == Token::SHR) {
          ASSERT(non_smi_shr_result.is_linked());
          __ bind(&non_smi_shr_result);
          // Allocate a heap number if needed.
          __ movl(rbx, rax);  // rbx holds result value (uint32 value as int64).
          switch (mode_) {
            case OVERWRITE_LEFT:
            case OVERWRITE_RIGHT:
              // If the operand was an object, we skip the
              // allocation of a heap number.
              __ movq(rax, Operand(rsp, mode_ == OVERWRITE_RIGHT ?
                                   1 * kPointerSize : 2 * kPointerSize));
              __ JumpIfNotSmi(rax, &skip_allocation);
              // Fall through!
            case NO_OVERWRITE:
              // Allocate heap number in new space.
              // Not using AllocateHeapNumber macro in order to reuse
              // already loaded heap_number_map.
              __ AllocateInNewSpace(HeapNumber::kSize,
                                    rax,
                                    rcx,
                                    no_reg,
                                    &call_runtime,
                                    TAG_OBJECT);
              // Set the map.
              if (FLAG_debug_code) {
                __ AbortIfNotRootValue(heap_number_map,
                                       Heap::kHeapNumberMapRootIndex,
                                       "HeapNumberMap register clobbered.");
              }
              __ movq(FieldOperand(rax, HeapObject::kMapOffset),
                      heap_number_map);
              __ bind(&skip_allocation);
              break;
            default: UNREACHABLE();
          }
          // Store the result in the HeapNumber and return.
          __ cvtqsi2sd(xmm0, rbx);
          __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
          GenerateReturn(masm);
        }

        break;
      }
      default: UNREACHABLE(); break;
    }
  }

  // If all else fails, use the runtime system to get the correct
  // result. If arguments was passed in registers now place them on the
  // stack in the correct order below the return address.
  __ bind(&call_runtime);

  if (HasArgsInRegisters()) {
    GenerateRegisterArgsPush(masm);
  }

  switch (op_) {
    case Token::ADD: {
      // Registers containing left and right operands respectively.
      Register lhs, rhs;

      if (HasArgsReversed()) {
        lhs = rax;
        rhs = rdx;
      } else {
        lhs = rdx;
        rhs = rax;
      }

      // Test for string arguments before calling runtime.
      Label not_strings, both_strings, not_string1, string1, string1_smi2;

      // If this stub has already generated FP-specific code then the arguments
      // are already in rdx and rax.
      if (!ShouldGenerateFPCode() && !HasArgsInRegisters()) {
        GenerateLoadArguments(masm);
      }

      Condition is_smi;
      is_smi = masm->CheckSmi(lhs);
      __ j(is_smi, &not_string1);
      __ CmpObjectType(lhs, FIRST_NONSTRING_TYPE, r8);
      __ j(above_equal, &not_string1);

      // First argument is a a string, test second.
      is_smi = masm->CheckSmi(rhs);
      __ j(is_smi, &string1_smi2);
      __ CmpObjectType(rhs, FIRST_NONSTRING_TYPE, r9);
      __ j(above_equal, &string1);

      // First and second argument are strings.
      StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
      __ TailCallStub(&string_add_stub);

      __ bind(&string1_smi2);
      // First argument is a string, second is a smi. Try to lookup the number
      // string for the smi in the number string cache.
      NumberToStringStub::GenerateLookupNumberStringCache(
          masm, rhs, rbx, rcx, r8, true, &string1);

      // Replace second argument on stack and tailcall string add stub to make
      // the result.
      __ movq(Operand(rsp, 1 * kPointerSize), rbx);
      __ TailCallStub(&string_add_stub);

      // Only first argument is a string.
      __ bind(&string1);
      __ InvokeBuiltin(Builtins::STRING_ADD_LEFT, JUMP_FUNCTION);

      // First argument was not a string, test second.
      __ bind(&not_string1);
      is_smi = masm->CheckSmi(rhs);
      __ j(is_smi, &not_strings);
      __ CmpObjectType(rhs, FIRST_NONSTRING_TYPE, rhs);
      __ j(above_equal, &not_strings);

      // Only second argument is a string.
      __ InvokeBuiltin(Builtins::STRING_ADD_RIGHT, JUMP_FUNCTION);

      __ bind(&not_strings);
      // Neither argument is a string.
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    }
    case Token::SUB:
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
      break;
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_FUNCTION);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_FUNCTION);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_FUNCTION);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_FUNCTION);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_FUNCTION);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void GenericBinaryOpStub::GenerateLoadArguments(MacroAssembler* masm) {
  ASSERT(!HasArgsInRegisters());
  __ movq(rax, Operand(rsp, 1 * kPointerSize));
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));
}


void GenericBinaryOpStub::GenerateReturn(MacroAssembler* masm) {
  // If arguments are not passed in registers remove them from the stack before
  // returning.
  if (!HasArgsInRegisters()) {
    __ ret(2 * kPointerSize);  // Remove both operands
  } else {
    __ ret(0);
  }
}


void GenericBinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  ASSERT(HasArgsInRegisters());
  __ pop(rcx);
  if (HasArgsReversed()) {
    __ push(rax);
    __ push(rdx);
  } else {
    __ push(rdx);
    __ push(rax);
  }
  __ push(rcx);
}


void GenericBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  Label get_result;

  // Ensure the operands are on the stack.
  if (HasArgsInRegisters()) {
    GenerateRegisterArgsPush(masm);
  }

  // Left and right arguments are already on stack.
  __ pop(rcx);  // Save the return address.

  // Push this stub's key.
  __ Push(Smi::FromInt(MinorKey()));

  // Although the operation and the type info are encoded into the key,
  // the encoding is opaque, so push them too.
  __ Push(Smi::FromInt(op_));

  __ Push(Smi::FromInt(runtime_operands_type_));

  __ push(rcx);  // The return address.

  // Perform patching to an appropriate fast case and return the result.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch)),
      5,
      1);
}


Handle<Code> GetBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info) {
  GenericBinaryOpStub stub(key, type_info);
  return stub.GetCode();
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // Input on stack:
  // rsp[8]: argument (should be number).
  // rsp[0]: return address.
  Label runtime_call;
  Label runtime_call_clear_stack;
  Label input_not_smi;
  Label loaded;
  // Test that rax is a number.
  __ movq(rax, Operand(rsp, kPointerSize));
  __ JumpIfNotSmi(rax, &input_not_smi);
  // Input is a smi. Untag and load it onto the FPU stack.
  // Then load the bits of the double into rbx.
  __ SmiToInteger32(rax, rax);
  __ subq(rsp, Immediate(kPointerSize));
  __ cvtlsi2sd(xmm1, rax);
  __ movsd(Operand(rsp, 0), xmm1);
  __ movq(rbx, xmm1);
  __ movq(rdx, xmm1);
  __ fld_d(Operand(rsp, 0));
  __ addq(rsp, Immediate(kPointerSize));
  __ jmp(&loaded);

  __ bind(&input_not_smi);
  // Check if input is a HeapNumber.
  __ Move(rbx, Factory::heap_number_map());
  __ cmpq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ j(not_equal, &runtime_call);
  // Input is a HeapNumber. Push it on the FPU stack and load its
  // bits into rbx.
  __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
  __ movq(rbx, FieldOperand(rax, HeapNumber::kValueOffset));
  __ movq(rdx, rbx);
  __ bind(&loaded);
  // ST[0] == double value
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
  ASSERT(IsPowerOf2(TranscendentalCache::kCacheSize));
  __ andl(rcx, Immediate(TranscendentalCache::kCacheSize - 1));

  // ST[0] == double value.
  // rbx = bits of double value.
  // rcx = TranscendentalCache::hash(double value).
  __ movq(rax, ExternalReference::transcendental_cache_array_address());
  // rax points to cache array.
  __ movq(rax, Operand(rax, type_ * sizeof(TranscendentalCache::caches_[0])));
  // rax points to the cache for the type type_.
  // If NULL, the cache hasn't been initialized yet, so go through runtime.
  __ testq(rax, rax);
  __ j(zero, &runtime_call_clear_stack);
#ifdef DEBUG
  // Check that the layout of cache elements match expectations.
  {  // NOLINT - doesn't like a single brace on a line.
    TranscendentalCache::Element test_elem[2];
    char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
    char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
    char* elem_in0  = reinterpret_cast<char*>(&(test_elem[0].in[0]));
    char* elem_in1  = reinterpret_cast<char*>(&(test_elem[0].in[1]));
    char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
    // Two uint_32's and a pointer per element.
    CHECK_EQ(16, static_cast<int>(elem2_start - elem_start));
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
  __ j(not_equal, &cache_miss);
  // Cache hit!
  __ movq(rax, Operand(rcx, 2 * kIntSize));
  __ fstp(0);  // Clear FPU stack.
  __ ret(kPointerSize);

  __ bind(&cache_miss);
  // Update cache with new value.
  Label nan_result;
  GenerateOperation(masm, &nan_result);
  __ AllocateHeapNumber(rax, rdi, &runtime_call_clear_stack);
  __ movq(Operand(rcx, 0), rbx);
  __ movq(Operand(rcx, 2 * kIntSize), rax);
  __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
  __ ret(kPointerSize);

  __ bind(&runtime_call_clear_stack);
  __ fstp(0);
  __ bind(&runtime_call);
  __ TailCallExternalReference(ExternalReference(RuntimeFunction()), 1, 1);

  __ bind(&nan_result);
  __ fstp(0);  // Remove argument from FPU stack.
  __ LoadRoot(rax, Heap::kNanValueRootIndex);
  __ movq(Operand(rcx, 0), rbx);
  __ movq(Operand(rcx, 2 * kIntSize), rax);
  __ ret(kPointerSize);
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    // Add more cases when necessary.
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void TranscendentalCacheStub::GenerateOperation(MacroAssembler* masm,
                                                Label* on_nan_result) {
  // Registers:
  // rbx: Bits of input double. Must be preserved.
  // rcx: Pointer to cache entry. Must be preserved.
  // st(0): Input double
  Label done;
  ASSERT(type_ == TranscendentalCache::SIN ||
         type_ == TranscendentalCache::COS);
  // More transcendental types can be added later.

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
  __ j(equal, on_nan_result);

  // Use fpmod to restrict argument to the range +/-2*PI.
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
  __ bind(&in_range);
  switch (type_) {
    case TranscendentalCache::SIN:
      __ fsin();
      break;
    case TranscendentalCache::COS:
      __ fcos();
      break;
    default:
      UNREACHABLE();
  }
  __ bind(&done);
}


// Get the integer part of a heap number.
// Overwrites the contents of rdi, rbx and rcx. Result cannot be rdi or rbx.
void IntegerConvert(MacroAssembler* masm,
                    Register result,
                    Register source) {
  // Result may be rcx. If result and source are the same register, source will
  // be overwritten.
  ASSERT(!result.is(rdi) && !result.is(rbx));
  // TODO(lrn): When type info reaches here, if value is a 32-bit integer, use
  // cvttsd2si (32-bit version) directly.
  Register double_exponent = rbx;
  Register double_value = rdi;
  Label done, exponent_63_plus;
  // Get double and extract exponent.
  __ movq(double_value, FieldOperand(source, HeapNumber::kValueOffset));
  // Clear result preemptively, in case we need to return zero.
  __ xorl(result, result);
  __ movq(xmm0, double_value);  // Save copy in xmm0 in case we need it there.
  // Double to remove sign bit, shift exponent down to least significant bits.
  // and subtract bias to get the unshifted, unbiased exponent.
  __ lea(double_exponent, Operand(double_value, double_value, times_1, 0));
  __ shr(double_exponent, Immediate(64 - HeapNumber::kExponentBits));
  __ subl(double_exponent, Immediate(HeapNumber::kExponentBias));
  // Check whether the exponent is too big for a 63 bit unsigned integer.
  __ cmpl(double_exponent, Immediate(63));
  __ j(above_equal, &exponent_63_plus);
  // Handle exponent range 0..62.
  __ cvttsd2siq(result, xmm0);
  __ jmp(&done);

  __ bind(&exponent_63_plus);
  // Exponent negative or 63+.
  __ cmpl(double_exponent, Immediate(83));
  // If exponent negative or above 83, number contains no significant bits in
  // the range 0..2^31, so result is zero, and rcx already holds zero.
  __ j(above, &done);

  // Exponent in rage 63..83.
  // Mantissa * 2^exponent contains bits in the range 2^0..2^31, namely
  // the least significant exponent-52 bits.

  // Negate low bits of mantissa if value is negative.
  __ addq(double_value, double_value);  // Move sign bit to carry.
  __ sbbl(result, result);  // And convert carry to -1 in result register.
  // if scratch2 is negative, do (scratch2-1)^-1, otherwise (scratch2-0)^0.
  __ addl(double_value, result);
  // Do xor in opposite directions depending on where we want the result
  // (depending on whether result is rcx or not).

  if (result.is(rcx)) {
    __ xorl(double_value, result);
    // Left shift mantissa by (exponent - mantissabits - 1) to save the
    // bits that have positional values below 2^32 (the extra -1 comes from the
    // doubling done above to move the sign bit into the carry flag).
    __ leal(rcx, Operand(double_exponent, -HeapNumber::kMantissaBits - 1));
    __ shll_cl(double_value);
    __ movl(result, double_value);
  } else {
    // As the then-branch, but move double-value to result before shifting.
    __ xorl(result, double_value);
    __ leal(rcx, Operand(double_exponent, -HeapNumber::kMantissaBits - 1));
    __ shll_cl(result);
  }

  __ bind(&done);
}


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
void FloatingPointHelper::LoadNumbersAsIntegers(MacroAssembler* masm) {
  // Check float operands.
  Label done;
  Label rax_is_smi;
  Label rax_is_object;
  Label rdx_is_object;

  __ JumpIfNotSmi(rdx, &rdx_is_object);
  __ SmiToInteger32(rdx, rdx);
  __ JumpIfSmi(rax, &rax_is_smi);

  __ bind(&rax_is_object);
  IntegerConvert(masm, rcx, rax);  // Uses rdi, rcx and rbx.
  __ jmp(&done);

  __ bind(&rdx_is_object);
  IntegerConvert(masm, rdx, rdx);  // Uses rdi, rcx and rbx.
  __ JumpIfNotSmi(rax, &rax_is_object);
  __ bind(&rax_is_smi);
  __ SmiToInteger32(rcx, rax);

  __ bind(&done);
  __ movl(rax, rdx);
}


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         Label* conversion_failure,
                                         Register heap_number_map) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  __ JumpIfNotSmi(rdx, &arg1_is_object);
  __ SmiToInteger32(rdx, rdx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ movl(rdx, Immediate(0));
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ cmpq(FieldOperand(rdx, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg1);
  // Get the untagged integer version of the edx heap number in rcx.
  IntegerConvert(masm, rdx, rdx);

  // Here rdx has the untagged integer, rax has a Smi or a heap number.
  __ bind(&load_arg2);
  // Test if arg2 is a Smi.
  __ JumpIfNotSmi(rax, &arg2_is_object);
  __ SmiToInteger32(rax, rax);
  __ movl(rcx, rax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ movl(rcx, Immediate(0));
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ cmpq(FieldOperand(rax, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the rax heap number in rcx.
  IntegerConvert(masm, rcx, rax);
  __ bind(&done);
  __ movl(rax, rdx);
}


void FloatingPointHelper::LoadSSE2SmiOperands(MacroAssembler* masm) {
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);
}


void FloatingPointHelper::LoadSSE2NumberOperands(MacroAssembler* masm) {
  Label load_smi_rdx, load_nonsmi_rax, load_smi_rax, done;
  // Load operand in rdx into xmm0.
  __ JumpIfSmi(rdx, &load_smi_rdx);
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  // Load operand in rax into xmm1.
  __ JumpIfSmi(rax, &load_smi_rax);
  __ bind(&load_nonsmi_rax);
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


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done;

  if (op_ == Token::SUB) {
    // Check whether the value is a smi.
    Label try_float;
    __ JumpIfNotSmi(rax, &try_float);

    if (negative_zero_ == kIgnoreNegativeZero) {
      __ SmiCompare(rax, Smi::FromInt(0));
      __ j(equal, &done);
    }

    // Enter runtime system if the value of the smi is zero
    // to make sure that we switch between 0 and -0.
    // Also enter it if the value of the smi is Smi::kMinValue.
    __ SmiNeg(rax, rax, &done);

    // Either zero or Smi::kMinValue, neither of which become a smi when
    // negated.
    if (negative_zero_ == kStrictNegativeZero) {
      __ SmiCompare(rax, Smi::FromInt(0));
      __ j(not_equal, &slow);
      __ Move(rax, Factory::minus_zero_value());
      __ jmp(&done);
    } else  {
      __ jmp(&slow);
    }

    // Try floating point case.
    __ bind(&try_float);
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &slow);
    // Operand is a float, negate its value by flipping sign bit.
    __ movq(rdx, FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(kScratchRegister, Immediate(0x01));
    __ shl(kScratchRegister, Immediate(63));
    __ xor_(rdx, kScratchRegister);  // Flip sign.
    // rdx is value to store.
    if (overwrite_ == UNARY_OVERWRITE) {
      __ movq(FieldOperand(rax, HeapNumber::kValueOffset), rdx);
    } else {
      __ AllocateHeapNumber(rcx, rbx, &slow);
      // rcx: allocated 'empty' number
      __ movq(FieldOperand(rcx, HeapNumber::kValueOffset), rdx);
      __ movq(rax, rcx);
    }
  } else if (op_ == Token::BIT_NOT) {
    // Check if the operand is a heap number.
    __ movq(rdx, FieldOperand(rax, HeapObject::kMapOffset));
    __ CompareRoot(rdx, Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &slow);

    // Convert the heap number in rax to an untagged integer in rcx.
    IntegerConvert(masm, rax, rax);

    // Do the bitwise operation and smi tag the result.
    __ notl(rax);
    __ Integer32ToSmi(rax, rax);
  }

  // Return from the stub.
  __ bind(&done);
  __ StubReturn(1);

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
  __ pop(rcx);  // pop return address
  __ push(rax);
  __ push(rcx);  // push return address
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


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in rdx and the parameter count is in rax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(rdx, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rbx, StandardFrameConstants::kContextOffset),
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
  __ pop(rbx);  // Return address.
  __ push(rdx);
  __ push(rbx);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // rsp[0] : return address
  // rsp[8] : number of parameters
  // rsp[16] : receiver displacement
  // rsp[24] : function

  // The displacement is used for skipping the return address and the
  // frame pointer on the stack. It is the offset of the last
  // parameter (if any) relative to the frame pointer.
  static const int kDisplacement = 2 * kPointerSize;

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ SmiCompare(Operand(rdx, StandardFrameConstants::kContextOffset),
                Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  __ SmiToInteger32(rcx, Operand(rsp, 1 * kPointerSize));
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ SmiToInteger32(rcx,
                    Operand(rdx,
                            ArgumentsAdaptorFrameConstants::kLengthOffset));
  // Space on stack must already hold a smi.
  __ Integer32ToSmiField(Operand(rsp, 1 * kPointerSize), rcx);
  // Do not clobber the length index for the indexing operation since
  // it is used compute the size for allocation later.
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size, kDisplacement));
  __ movq(Operand(rsp, 2 * kPointerSize), rdx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ testl(rcx, rcx);
  __ j(zero, &add_arguments_object);
  __ leal(rcx, Operand(rcx, times_pointer_size, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ addl(rcx, Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(rcx, rax, rdx, rbx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kGlobalContextOffset));
  __ movq(rdi, Operand(rdi, offset));

  // Copy the JS object part.
  STATIC_ASSERT(JSObject::kHeaderSize == 3 * kPointerSize);
  __ movq(kScratchRegister, FieldOperand(rdi, 0 * kPointerSize));
  __ movq(rdx, FieldOperand(rdi, 1 * kPointerSize));
  __ movq(rbx, FieldOperand(rdi, 2 * kPointerSize));
  __ movq(FieldOperand(rax, 0 * kPointerSize), kScratchRegister);
  __ movq(FieldOperand(rax, 1 * kPointerSize), rdx);
  __ movq(FieldOperand(rax, 2 * kPointerSize), rbx);

  // Setup the callee in-object property.
  ASSERT(Heap::arguments_callee_index == 0);
  __ movq(kScratchRegister, Operand(rsp, 3 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize), kScratchRegister);

  // Get the length (smi tagged) and set that as an in-object property too.
  ASSERT(Heap::arguments_length_index == 1);
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize + kPointerSize), rcx);

  // If there are no actual arguments, we're done.
  Label done;
  __ SmiTest(rcx);
  __ j(zero, &done);

  // Get the parameters pointer from the stack and untag the length.
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);
  __ SmiToInteger32(rcx, rcx);  // Untag length for the loop below.

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ movq(kScratchRegister, Operand(rdx, -1 * kPointerSize));  // Skip receiver.
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize), kScratchRegister);
  __ addq(rdi, Immediate(kPointerSize));
  __ subq(rdx, Immediate(kPointerSize));
  __ decl(rcx);
  __ j(not_zero, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#else  // V8_INTERPRETED_REGEXP
  if (!FLAG_regexp_entry_native) {
    __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
    return;
  }

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[8]: last_match_info (expected JSArray)
  //  esp[16]: previous index
  //  esp[24]: subject string
  //  esp[32]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ movq(kScratchRegister, Operand(kScratchRegister, 0));
  __ testq(kScratchRegister, kScratchRegister);
  __ j(zero, &runtime);


  // Check that the first argument is a JSRegExp object.
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the RegExp has been compiled (data contains a fixed array).
  __ movq(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    Condition is_smi = masm->CheckSmi(rcx);
    __ Check(NegateCondition(is_smi),
        "Unexpected type for RegExp data, FixedArray expected");
    __ CmpObjectType(rcx, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Check(equal, "Unexpected type for RegExp data, FixedArray expected");
  }

  // rcx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ SmiToInteger32(rbx, FieldOperand(rcx, JSRegExp::kDataTagOffset));
  __ cmpl(rbx, Immediate(JSRegExp::IRREGEXP));
  __ j(not_equal, &runtime);

  // rcx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ SmiToInteger32(rdx,
                    FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ leal(rdx, Operand(rdx, rdx, times_1, 2));
  // Check that the static offsets vector buffer is large enough.
  __ cmpl(rdx, Immediate(OffsetsVector::kStaticOffsetsVectorSize));
  __ j(above, &runtime);

  // rcx: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the second argument is a string.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ JumpIfSmi(rax, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rax: Subject string.
  // rcx: RegExp data (FixedArray).
  // rdx: Number of capture registers.
  // Check that the third argument is a positive smi less than the string
  // length. A negative value will be greater (unsigned comparison).
  __ movq(rbx, Operand(rsp, kPreviousIndexOffset));
  __ JumpIfNotSmi(rbx, &runtime);
  __ SmiCompare(rbx, FieldOperand(rax, String::kLengthOffset));
  __ j(above_equal, &runtime);

  // rcx: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_ARRAY_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ movq(rbx, FieldOperand(rax, JSArray::kElementsOffset));
  __ movq(rax, FieldOperand(rbx, HeapObject::kMapOffset));
  __ Cmp(rax, Factory::fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information. Ensure no overflow in add.
  STATIC_ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
  __ SmiToInteger32(rax, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ addl(rdx, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmpl(rdx, rax);
  __ j(greater, &runtime);

  // rcx: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_ascii_string, seq_two_byte_string, check_code;
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  // First check for flat two byte string.
  __ andb(rbx, Immediate(
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);
  // Any other flat string must be a flat ascii string.
  __ testb(rbx, Immediate(kIsNotStringMask | kStringRepresentationMask));
  __ j(zero, &seq_ascii_string);

  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  STATIC_ASSERT(kExternalStringTag !=0);
  STATIC_ASSERT((kConsStringTag & kExternalStringTag) == 0);
  __ testb(rbx, Immediate(kIsNotStringMask | kExternalStringTag));
  __ j(not_zero, &runtime);
  // String is a cons string.
  __ movq(rdx, FieldOperand(rax, ConsString::kSecondOffset));
  __ Cmp(rdx, Factory::empty_string());
  __ j(not_equal, &runtime);
  __ movq(rax, FieldOperand(rax, ConsString::kFirstOffset));
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  // String is a cons string with empty second part.
  // rax: first part of cons string.
  // rbx: map of first part of cons string.
  // Is first part a flat two byte string?
  __ testb(FieldOperand(rbx, Map::kInstanceTypeOffset),
           Immediate(kStringRepresentationMask | kStringEncodingMask));
  STATIC_ASSERT((kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);
  // Any other flat string must be ascii.
  __ testb(FieldOperand(rbx, Map::kInstanceTypeOffset),
           Immediate(kStringRepresentationMask));
  __ j(not_zero, &runtime);

  __ bind(&seq_ascii_string);
  // rax: subject string (sequential ascii)
  // rcx: RegExp data (FixedArray)
  __ movq(r11, FieldOperand(rcx, JSRegExp::kDataAsciiCodeOffset));
  __ Set(rdi, 1);  // Type is ascii.
  __ jmp(&check_code);

  __ bind(&seq_two_byte_string);
  // rax: subject string (flat two-byte)
  // rcx: RegExp data (FixedArray)
  __ movq(r11, FieldOperand(rcx, JSRegExp::kDataUC16CodeOffset));
  __ Set(rdi, 0);  // Type is two byte.

  __ bind(&check_code);
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CmpObjectType(r11, CODE_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);

  // rax: subject string
  // rdi: encoding of subject string (1 if ascii, 0 if two_byte);
  // r11: code
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ SmiToInteger64(rbx, Operand(rsp, kPreviousIndexOffset));

  // rax: subject string
  // rbx: previous index
  // rdi: encoding of subject string (1 if ascii 0 if two_byte);
  // r11: code
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1);

  // rsi is caller save on Windows and used to pass parameter on Linux.
  __ push(rsi);

  static const int kRegExpExecuteArguments = 7;
  __ PrepareCallCFunction(kRegExpExecuteArguments);
  int argument_slots_on_stack =
      masm->ArgumentStackSlotsForCFunctionCall(kRegExpExecuteArguments);

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kPointerSize),
          Immediate(1));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ movq(kScratchRegister, address_of_regexp_stack_memory_address);
  __ movq(r9, Operand(kScratchRegister, 0));
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ addq(r9, Operand(kScratchRegister, 0));
  // Argument 6 passed in r9 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 2) * kPointerSize), r9);
#endif

  // Argument 5: static offsets vector buffer.
  __ movq(r8, ExternalReference::address_of_static_offsets_vector());
  // Argument 5 passed in r8 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 3) * kPointerSize), r8);
#endif

  // First four arguments are passed in registers on both Linux and Windows.
#ifdef _WIN64
  Register arg4 = r9;
  Register arg3 = r8;
  Register arg2 = rdx;
  Register arg1 = rcx;
#else
  Register arg4 = rcx;
  Register arg3 = rdx;
  Register arg2 = rsi;
  Register arg1 = rdi;
#endif

  // Keep track on aliasing between argX defined above and the registers used.
  // rax: subject string
  // rbx: previous index
  // rdi: encoding of subject string (1 if ascii 0 if two_byte);
  // r11: code

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label setup_two_byte, setup_rest;
  __ testb(rdi, rdi);
  __ j(zero, &setup_two_byte);
  __ SmiToInteger32(rdi, FieldOperand(rax, String::kLengthOffset));
  __ lea(arg4, FieldOperand(rax, rdi, times_1, SeqAsciiString::kHeaderSize));
  __ lea(arg3, FieldOperand(rax, rbx, times_1, SeqAsciiString::kHeaderSize));
  __ jmp(&setup_rest);
  __ bind(&setup_two_byte);
  __ SmiToInteger32(rdi, FieldOperand(rax, String::kLengthOffset));
  __ lea(arg4, FieldOperand(rax, rdi, times_2, SeqTwoByteString::kHeaderSize));
  __ lea(arg3, FieldOperand(rax, rbx, times_2, SeqTwoByteString::kHeaderSize));

  __ bind(&setup_rest);
  // Argument 2: Previous index.
  __ movq(arg2, rbx);

  // Argument 1: Subject string.
  __ movq(arg1, rax);

  // Locate the code entry and call it.
  __ addq(r11, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ CallCFunction(r11, kRegExpExecuteArguments);

  // rsi is caller save, as it is used to pass parameter.
  __ pop(rsi);

  // Check the result.
  Label success;
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::SUCCESS));
  __ j(equal, &success);
  Label failure;
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::FAILURE));
  __ j(equal, &failure);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ movq(kScratchRegister, pending_exception_address);
  __ Cmp(kScratchRegister, Factory::the_hole_value());
  __ j(equal, &runtime);
  __ bind(&failure);
  // For failure and exception return null.
  __ Move(rax, Factory::null_value());
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
  // Load last_match_info which is still known to be a fast case JSArray.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ movq(rbx, FieldOperand(rax, JSArray::kElementsOffset));

  // rbx: last_match_info backing store (FixedArray)
  // rdx: number of capture registers
  // Store the capture count.
  __ Integer32ToSmi(kScratchRegister, rdx);
  __ movq(FieldOperand(rbx, RegExpImpl::kLastCaptureCountOffset),
          kScratchRegister);
  // Store last subject and last input.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastSubjectOffset), rax);
  __ movq(rcx, rbx);
  __ RecordWrite(rcx, RegExpImpl::kLastSubjectOffset, rax, rdi);
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastInputOffset), rax);
  __ movq(rcx, rbx);
  __ RecordWrite(rcx, RegExpImpl::kLastInputOffset, rax, rdi);

  // Get the static offsets vector filled by the native regexp code.
  __ movq(rcx, ExternalReference::address_of_static_offsets_vector());

  // rbx: last_match_info backing store (FixedArray)
  // rcx: offsets vector
  // rdx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ subq(rdx, Immediate(1));
  __ j(negative, &done);
  // Read the value from the static offsets vector buffer and make it a smi.
  __ movl(rdi, Operand(rcx, rdx, times_int_size, 0));
  __ Integer32ToSmi(rdi, rdi, &runtime);
  // Store the smi value in the last match info.
  __ movq(FieldOperand(rbx,
                       rdx,
                       times_pointer_size,
                       RegExpImpl::kFirstCaptureOffset),
          rdi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#endif  // V8_INTERPRETED_REGEXP
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
  if (!object_is_smi) {
    __ JumpIfSmi(object, &is_smi);
    __ CheckMap(object, Factory::heap_number_map(), not_found, true);

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
    ASSERT(CpuFeatures::IsSupported(SSE2));
    CpuFeatures::Scope fscope(SSE2);
    __ movsd(xmm0, FieldOperand(object, HeapNumber::kValueOffset));
    __ movsd(xmm1, FieldOperand(probe, HeapNumber::kValueOffset));
    __ ucomisd(xmm0, xmm1);
    __ j(parity_even, not_found);  // Bail out if NaN is involved.
    __ j(not_equal, not_found);  // The cache did not contain this value.
    __ jmp(&load_result_from_cache);
  }

  __ bind(&is_smi);
  __ SmiToInteger32(scratch, object);
  GenerateConvertHashCodeToIndex(masm, scratch, mask);

  Register index = scratch;
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
  __ IncrementCounter(&Counters::number_to_string_native, 1);
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

  __ movq(rbx, Operand(rsp, kPointerSize));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, rbx, rax, r8, r9, false, &runtime);
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


void CompareStub::Generate(MacroAssembler* masm) {
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));

  Label check_unequal_objects, done;
  // The compare stub returns a positive, negative, or zero 64-bit integer
  // value in rax, corresponding to result of comparing the two inputs.
  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Two identical objects are equal unless they are both NaN or undefined.
  {
    Label not_identical;
    __ cmpq(rax, rdx);
    __ j(not_equal, &not_identical);

    if (cc_ != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      Label check_for_nan;
      __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
      __ j(not_equal, &check_for_nan);
      __ Set(rax, NegativeComparisonResult(cc_));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
    // Note: if cc_ != equal, never_nan_nan_ is not used.
    // We cannot set rax to EQUAL until just before return because
    // rax must be unchanged on jump to not_identical.

    if (never_nan_nan_ && (cc_ == equal)) {
      __ Set(rax, EQUAL);
      __ ret(0);
    } else {
      Label heap_number;
      // If it's not a heap number, then return equal for (in)equality operator.
      __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
             Factory::heap_number_map());
      __ j(equal, &heap_number);
      if (cc_ != equal) {
        // Call runtime on identical JSObjects.  Otherwise return equal.
        __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
        __ j(above_equal, &not_identical);
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
      if (cc_ == greater_equal || cc_ == greater) {
        __ neg(rax);
      }
      __ ret(0);
    }

    __ bind(&not_identical);
  }

  if (cc_ == equal) {  // Both strict and non-strict.
    Label slow;  // Fallthrough label.

    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    if (strict_) {
      // If either is a Smi (we know that not both are), then they can only
      // be equal if the other is a HeapNumber. If so, use the slow case.
      {
        Label not_smis;
        __ SelectNonSmi(rbx, rax, rdx, &not_smis);

        // Check if the non-smi operand is a heap number.
        __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
               Factory::heap_number_map());
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
      STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
      Label first_non_object;
      __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(below, &first_non_object);
      // Return non-zero (eax (not rax) is not zero)
      Label return_not_equal;
      STATIC_ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ CmpObjectType(rdx, FIRST_JS_OBJECT_TYPE, rcx);
      __ j(above_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Generate the number comparison code.
  if (include_number_compare_) {
    Label non_number_comparison;
    Label unordered;
    FloatingPointHelper::LoadSSE2UnknownOperands(masm, &non_number_comparison);
    __ xorl(rax, rax);
    __ xorl(rcx, rcx);
    __ ucomisd(xmm0, xmm1);

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered);
    // Return a result of -1, 0, or 1, based on EFLAGS.
    __ setcc(above, rax);
    __ setcc(below, rcx);
    __ subq(rax, rcx);
    __ ret(0);

    // If one of the numbers was NaN, then the result is always false.
    // The cc is never not-equal.
    __ bind(&unordered);
    ASSERT(cc_ != not_equal);
    if (cc_ == less || cc_ == less_equal) {
      __ Set(rax, 1);
    } else {
      __ Set(rax, -1);
    }
    __ ret(0);

    // The number comparison code did not provide a valid result.
    __ bind(&non_number_comparison);
  }

  // Fast negative check for symbol-to-symbol equality.
  Label check_for_strings;
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &check_for_strings, rax, kScratchRegister);
    BranchIfNonSymbol(masm, &check_for_strings, rdx, kScratchRegister);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax (not rax) already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(
      rdx, rax, rcx, rbx, &check_unequal_objects);

  // Inline comparison of ascii strings.
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     rdx,
                                                     rax,
                                                     rcx,
                                                     rbx,
                                                     rdi,
                                                     r8);

#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&check_unequal_objects);
  if (cc_ == equal && !strict_) {
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
    __ j(not_zero, &not_both_objects);
    __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rbx);
    __ j(below, &not_both_objects);
    __ CmpObjectType(rdx, FIRST_JS_OBJECT_TYPE, rcx);
    __ j(below, &not_both_objects);
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal);
    __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal);
    // The objects are both undetectable, so they both compare as the value
    // undefined, and are equal.
    __ Set(rax, EQUAL);
    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in eax,
    // or return equal if we fell through to here.
    __ ret(0);
    __ bind(&not_both_objects);
  }

  // Push arguments below the return address to prepare jump to builtin.
  __ pop(rcx);
  __ push(rdx);
  __ push(rax);

  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript builtin;
  if (cc_ == equal) {
    builtin = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    __ Push(Smi::FromInt(NegativeComparisonResult(cc_)));
  }

  // Restore return address on the stack.
  __ push(rcx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);
}


void CompareStub::BranchIfNonSymbol(MacroAssembler* masm,
                                    Label* label,
                                    Register object,
                                    Register scratch) {
  __ JumpIfSmi(object, label);
  __ movq(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzxbq(scratch,
             FieldOperand(scratch, Map::kInstanceTypeOffset));
  // Ensure that no non-strings have the symbol bit set.
  STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
  STATIC_ASSERT(kSymbolTag != 0);
  __ testb(scratch, Immediate(kIsSymbolMask));
  __ j(zero, label);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  // Because builtins always remove the receiver from the stack, we
  // have to fake one to avoid underflowing the stack. The receiver
  // must be inserted below the return address on the stack so we
  // temporarily store that in a register.
  __ pop(rax);
  __ Push(Smi::FromInt(0));
  __ push(rax);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kStackGuard, 1, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // +1 ~ return address
    Label receiver_is_value, receiver_is_js_object;
    __ movq(rax, Operand(rsp, (argc_ + 1) * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ JumpIfSmi(rax, &receiver_is_value);

    // Check if the receiver is a valid JS object.
    __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rdi);
    __ j(above_equal, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(rax);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ LeaveInternalFrame();
    __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rax);

    __ bind(&receiver_is_js_object);
  }

  // Get the function to call from the stack.
  // +2 ~ receiver, return address
  __ movq(rdi, Operand(rsp, (argc_ + 2) * kPointerSize));

  // Check that the function really is a JavaScript function.
  __ JumpIfSmi(rdi, &slow);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rdi);
  __ Set(rax, argc_);
  __ Set(rbx, 0);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
  __ Jump(adaptor, RelocInfo::CODE_TARGET);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  // Check that stack should contain next handler, frame pointer, state and
  // return address in that order.
  STATIC_ASSERT(StackHandlerConstants::kFPOffset + kPointerSize ==
            StackHandlerConstants::kStateOffset);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset + kPointerSize ==
            StackHandlerConstants::kPCOffset);

  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rsp, Operand(kScratchRegister, 0));
  // get next in chain
  __ pop(rcx);
  __ movq(Operand(kScratchRegister, 0), rcx);
  __ pop(rbp);  // pop frame pointer
  __ pop(rdx);  // remove state

  // Before returning we restore the context from the frame pointer if not NULL.
  // The frame pointer is NULL in the exception handler of a JS entry frame.
  __ xor_(rsi, rsi);  // tentatively set context pointer to NULL
  Label skip;
  __ cmpq(rbp, Immediate(0));
  __ j(equal, &skip);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);
  __ ret(0);
}


void ApiGetterEntryStub::Generate(MacroAssembler* masm) {
  Label empty_result;
  Label prologue;
  Label promote_scheduled_exception;
  __ EnterApiExitFrame(kStackSpace, 0);
  ASSERT_EQ(kArgc, 4);
#ifdef _WIN64
  // All the parameters should be set up by a caller.
#else
  // Set 1st parameter register with property name.
  __ movq(rsi, rdx);
  // Second parameter register rdi should be set with pointer to AccessorInfo
  // by a caller.
#endif
  // Call the api function!
  __ movq(rax,
          reinterpret_cast<int64_t>(fun()->address()),
          RelocInfo::RUNTIME_ENTRY);
  __ call(rax);
  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address();
  __ movq(rsi, scheduled_exception_address);
  __ Cmp(Operand(rsi, 0), Factory::the_hole_value());
  __ j(not_equal, &promote_scheduled_exception);
#ifdef _WIN64
  // rax keeps a pointer to v8::Handle, unpack it.
  __ movq(rax, Operand(rax, 0));
#endif
  // Check if the result handle holds 0.
  __ testq(rax, rax);
  __ j(zero, &empty_result);
  // It was non-zero.  Dereference to get the result value.
  __ movq(rax, Operand(rax, 0));
  __ bind(&prologue);
  __ LeaveExitFrame();
  __ ret(0);
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException, 0, 1);
  __ bind(&empty_result);
  // It was zero; the result is undefined.
  __ Move(rax, Factory::undefined_value());
  __ jmp(&prologue);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope,
                              int /* alignment_skew */) {
  // rax: result parameter for PerformGC, if any.
  // rbx: pointer to C function  (C callee-saved).
  // rbp: frame pointer  (restored after C call).
  // rsp: stack pointer  (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r12: pointer to the first argument (C callee-saved).
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
#ifdef _WIN64
    __ movq(rcx, rax);
#else  // _WIN64
    __ movq(rdi, rax);
#endif
    __ movq(kScratchRegister,
            FUNCTION_ADDR(Runtime::PerformGC),
            RelocInfo::RUNTIME_ENTRY);
    __ call(kScratchRegister);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ incl(Operand(kScratchRegister, 0));
  }

  // Call C function.
#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9
  // Store Arguments object on stack, below the 4 WIN64 ABI parameter slots.
  __ movq(Operand(rsp, 4 * kPointerSize), r14);  // argc.
  __ movq(Operand(rsp, 5 * kPointerSize), r12);  // argv.
  if (result_size_ < 2) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax).
    __ lea(rcx, Operand(rsp, 4 * kPointerSize));
  } else {
    ASSERT_EQ(2, result_size_);
    // Pass a pointer to the result location as the first argument.
    __ lea(rcx, Operand(rsp, 6 * kPointerSize));
    // Pass a pointer to the Arguments object as the second argument.
    __ lea(rdx, Operand(rsp, 4 * kPointerSize));
  }

#else  // _WIN64
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9.
  __ movq(rdi, r14);  // argc.
  __ movq(rsi, r12);  // argv.
#endif
  __ call(rbx);
  // Result is in rax - do not destroy this register!

  if (always_allocate_scope) {
    __ movq(kScratchRegister, scope_depth);
    __ decl(Operand(kScratchRegister, 0));
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
  __ LeaveExitFrame(result_size_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ testl(rax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry);

  // Special handling of out of memory exceptions.
  __ movq(kScratchRegister, Failure::OutOfMemoryException(), RelocInfo::NONE);
  __ cmpq(rax, kScratchRegister);
  __ j(equal, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(Top::k_pending_exception_address);
  __ movq(kScratchRegister, pending_exception_address);
  __ movq(rax, Operand(kScratchRegister, 0));
  __ movq(rdx, ExternalReference::the_hole_value_location());
  __ movq(rdx, Operand(rdx, 0));
  __ movq(Operand(kScratchRegister, 0), rdx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  // Fetch top stack handler.
  ExternalReference handler_address(Top::k_handler_address);
  __ movq(kScratchRegister, handler_address);
  __ movq(rsp, Operand(kScratchRegister, 0));

  // Unwind the handlers until the ENTRY handler is found.
  Label loop, done;
  __ bind(&loop);
  // Load the type of the current stack handler.
  const int kStateOffset = StackHandlerConstants::kStateOffset;
  __ cmpq(Operand(rsp, kStateOffset), Immediate(StackHandler::ENTRY));
  __ j(equal, &done);
  // Fetch the next handler in the list.
  const int kNextOffset = StackHandlerConstants::kNextOffset;
  __ movq(rsp, Operand(rsp, kNextOffset));
  __ jmp(&loop);
  __ bind(&done);

  // Set the top handler address to next handler past the current ENTRY handler.
  __ movq(kScratchRegister, handler_address);
  __ pop(Operand(kScratchRegister, 0));

  if (type == OUT_OF_MEMORY) {
    // Set external caught exception to false.
    ExternalReference external_caught(Top::k_external_caught_exception_address);
    __ movq(rax, Immediate(false));
    __ store_rax(external_caught);

    // Set pending exception and rax to out of memory exception.
    ExternalReference pending_exception(Top::k_pending_exception_address);
    __ movq(rax, Failure::OutOfMemoryException(), RelocInfo::NONE);
    __ store_rax(pending_exception);
  }

  // Clear the context pointer.
  __ xor_(rsi, rsi);

  // Restore registers from handler.
  STATIC_ASSERT(StackHandlerConstants::kNextOffset + kPointerSize ==
            StackHandlerConstants::kFPOffset);
  __ pop(rbp);  // FP
  STATIC_ASSERT(StackHandlerConstants::kFPOffset + kPointerSize ==
            StackHandlerConstants::kStateOffset);
  __ pop(rdx);  // State

  STATIC_ASSERT(StackHandlerConstants::kStateOffset + kPointerSize ==
            StackHandlerConstants::kPCOffset);
  __ ret(0);
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

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(result_size_);

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
  // r12: argv pointer (C callee-saved).

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
  __ movq(rax, failure, RelocInfo::NONE);
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  GenerateThrowUncatchable(masm, OUT_OF_MEMORY);

  __ bind(&throw_termination_exception);
  GenerateThrowUncatchable(masm, TERMINATION);

  __ bind(&throw_normal_exception);
  GenerateThrowTOS(masm);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, exit;
#ifdef ENABLE_LOGGING_AND_PROFILING
  Label not_outermost_js, not_outermost_js_2;
#endif

  // Setup frame.
  __ push(rbp);
  __ movq(rbp, rsp);

  // Push the stack frame type marker twice.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  // Scratch register is neither callee-save, nor an argument register on any
  // platform. It's free to use at this point.
  // Cannot use smi-register for loading yet.
  __ movq(kScratchRegister,
          reinterpret_cast<uint64_t>(Smi::FromInt(marker)),
          RelocInfo::NONE);
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
  // TODO(X64): On Win64, if we ever use XMM6-XMM15, the low low 64 bits are
  // callee save as well.

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Top::k_c_entry_fp_address);
  __ load_rax(c_entry_fp);
  __ push(rax);

  // Set up the roots and smi constant registers.
  // Needs to be done before any further smi loads.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ movq(kRootRegister, roots_address);
  __ InitializeSmiConstantRegister();

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Top::k_js_entry_sp_address);
  __ load_rax(js_entry_sp);
  __ testq(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ movq(rax, rbp);
  __ store_rax(js_entry_sp);
  __ bind(&not_outermost_js);
#endif

  // Call a faked try-block that does the invoke.
  __ call(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ store_rax(pending_exception);
  __ movq(rax, Failure::Exception(), RelocInfo::NONE);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);

  // Clear any pending exceptions.
  __ load_rax(ExternalReference::the_hole_value_location());
  __ store_rax(pending_exception);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline
  // builtin and pop the faked function when we return. We load the address
  // from an external reference instead of inlining the call target address
  // directly in the code, because the builtin stubs may not have been
  // generated yet at the time this code is generated.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ load_rax(construct_entry);
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ load_rax(entry);
  }
  __ lea(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ movq(kScratchRegister, ExternalReference(Top::k_handler_address));
  __ pop(Operand(kScratchRegister, 0));
  // Pop next_sp.
  __ addq(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If current EBP value is the same as js_entry_sp value, it means that
  // the current function is the outermost.
  __ movq(kScratchRegister, js_entry_sp);
  __ cmpq(rbp, Operand(kScratchRegister, 0));
  __ j(not_equal, &not_outermost_js_2);
  __ movq(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);
#endif

  // Restore the top frame descriptor from the stack.
  __ bind(&exit);
  __ movq(kScratchRegister, ExternalReference(Top::k_c_entry_fp_address));
  __ pop(Operand(kScratchRegister, 0));

  // Restore callee-saved registers (X64 conventions).
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
  // Expected input state:
  //   rsp[0] : return address
  //   rsp[1] : function pointer
  //   rsp[2] : value
  // Returns a bitwise zero to indicate that the value
  // is and instance of the function and anything else to
  // indicate that the value is not an instance.

  // Get the object - go slow case if it's a smi.
  Label slow;
  __ movq(rax, Operand(rsp, 2 * kPointerSize));
  __ JumpIfSmi(rax, &slow);

  // Check that the left hand is a JS object. Leave its map in rax.
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rax);
  __ j(below, &slow);
  __ CmpInstanceType(rax, LAST_JS_OBJECT_TYPE);
  __ j(above, &slow);

  // Get the prototype of the function.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));
  // rdx is function, rax is map.

  // Look up the function and the map in the instanceof cache.
  Label miss;
  __ CompareRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
  __ j(not_equal, &miss);
  __ CompareRoot(rax, Heap::kInstanceofCacheMapRootIndex);
  __ j(not_equal, &miss);
  __ LoadRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(2 * kPointerSize);

  __ bind(&miss);
  __ TryGetFunctionPrototype(rdx, rbx, &slow);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(rbx, &slow);
  __ CmpObjectType(rbx, FIRST_JS_OBJECT_TYPE, kScratchRegister);
  __ j(below, &slow);
  __ CmpInstanceType(kScratchRegister, LAST_JS_OBJECT_TYPE);
  __ j(above, &slow);

  // Register mapping:
  //   rax is object map.
  //   rdx is function.
  //   rbx is function prototype.
  __ StoreRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
  __ StoreRoot(rax, Heap::kInstanceofCacheMapRootIndex);

  __ movq(rcx, FieldOperand(rax, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  Label loop, is_instance, is_not_instance;
  __ LoadRoot(kScratchRegister, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmpq(rcx, rbx);
  __ j(equal, &is_instance);
  __ cmpq(rcx, kScratchRegister);
  // The code at is_not_instance assumes that kScratchRegister contains a
  // non-zero GCable value (the null object in this case).
  __ j(equal, &is_not_instance);
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rcx, FieldOperand(rcx, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  __ xorl(rax, rax);
  // Store bitwise zero in the cache.  This is a Smi in GC terms.
  STATIC_ASSERT(kSmiTag == 0);
  __ StoreRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(2 * kPointerSize);

  __ bind(&is_not_instance);
  // We have to store a non-zero value in the cache.
  __ StoreRoot(kScratchRegister, Heap::kInstanceofCacheAnswerRootIndex);
  __ ret(2 * kPointerSize);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
}


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value. To avoid duplicate
  // stubs the never NaN NaN condition is only taken into account if the
  // condition is equals.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 12));
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));
  return ConditionField::encode(static_cast<unsigned>(cc_))
         | RegisterField::encode(false)    // lhs_ and rhs_ are not used
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == equal ? never_nan_nan_ : false)
         | IncludeNumberCompareField::encode(include_number_compare_);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
const char* CompareStub::GetName() {
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));

  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";

  const char* cc_name;
  switch (cc_) {
    case less: cc_name = "LT"; break;
    case greater: cc_name = "GT"; break;
    case less_equal: cc_name = "LE"; break;
    case greater_equal: cc_name = "GE"; break;
    case equal: cc_name = "EQ"; break;
    case not_equal: cc_name = "NE"; break;
    default: cc_name = "UnknownCondition"; break;
  }

  const char* strict_name = "";
  if (strict_ && (cc_ == equal || cc_ == not_equal)) {
    strict_name = "_STRICT";
  }

  const char* never_nan_nan_name = "";
  if (never_nan_nan_ && (cc_ == equal || cc_ == not_equal)) {
    never_nan_nan_name = "_NO_NAN";
  }

  const char* include_number_compare_name = "";
  if (!include_number_compare_) {
    include_number_compare_name = "_NO_NUMBER";
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "CompareStub_%s%s%s%s",
               cc_name,
               strict_name,
               never_nan_nan_name,
               include_number_compare_name);
  return name_;
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;

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

  // Put smi-tagged index into scratch register.
  __ movq(scratch_, index_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ SmiCompare(scratch_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  // We need special handling for non-flat strings.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(result_, Immediate(kStringRepresentationMask));
  __ j(zero, &flat_string);

  // Handle non-flat strings.
  __ testb(result_, Immediate(kIsConsStringMask));
  __ j(zero, &call_runtime_);

  // ConsString.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ CompareRoot(FieldOperand(object_, ConsString::kSecondOffset),
                 Heap::kEmptyStringRootIndex);
  __ j(not_equal, &call_runtime_);
  // Get the first of the two strings and load its instance type.
  __ movq(object_, FieldOperand(object_, ConsString::kFirstOffset));
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the first cons component is also non-flat, then go to runtime.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(result_, Immediate(kStringRepresentationMask));
  __ j(not_zero, &call_runtime_);

  // Check for 1-byte or 2-byte string.
  __ bind(&flat_string);
  STATIC_ASSERT(kAsciiStringTag != 0);
  __ testb(result_, Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the result register.
  __ SmiToInteger32(scratch_, scratch_);
  __ movzxwl(result_, FieldOperand(object_,
                                   scratch_, times_2,
                                   SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);
  __ SmiToInteger32(scratch_, scratch_);
  __ movzxbl(result_, FieldOperand(object_,
                                   scratch_, times_1,
                                   SeqAsciiString::kHeaderSize));
  __ bind(&got_char_code);
  __ Integer32ToSmi(result_, result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharCodeAt slow case");

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_, Factory::heap_number_map(), index_not_number_, true);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ push(index_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  if (!scratch_.is(rax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ movq(scratch_, rax);
  }
  __ pop(index_);
  __ pop(object_);
  // Reload the instance type.
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(scratch_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  __ JumpIfNotSmi(code_, &slow_case_);
  __ SmiCompare(code_, Smi::FromInt(String::kMaxAsciiCharCode));
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
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharFromCode slow case");
}


// -------------------------------------------------------------------------
// StringCharAtGenerator

void StringCharAtGenerator::GenerateFast(MacroAssembler* masm) {
  char_code_at_generator_.GenerateFast(masm);
  char_from_code_generator_.GenerateFast(masm);
}


void StringCharAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  char_code_at_generator_.GenerateSlow(masm, call_helper);
  char_from_code_generator_.GenerateSlow(masm, call_helper);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime;

  // Load the two arguments.
  __ movq(rax, Operand(rsp, 2 * kPointerSize));  // First argument.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (string_check_) {
    Condition is_smi;
    is_smi = masm->CheckSmi(rax);
    __ j(is_smi, &string_add_runtime);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, r8);
    __ j(above_equal, &string_add_runtime);

    // First argument is a a string, test second.
    is_smi = masm->CheckSmi(rdx);
    __ j(is_smi, &string_add_runtime);
    __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, r9);
    __ j(above_equal, &string_add_runtime);
  }

  // Both arguments are strings.
  // rax: first string
  // rdx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ movq(rcx, FieldOperand(rdx, String::kLengthOffset));
  __ SmiTest(rcx);
  __ j(not_zero, &second_not_zero_length);
  // Second string is empty, result is first string which is already in rax.
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ movq(rbx, FieldOperand(rax, String::kLengthOffset));
  __ SmiTest(rbx);
  __ j(not_zero, &both_not_zero_length);
  // First string is empty, result is second string which is in rdx.
  __ movq(rax, rdx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // rax: first string
  // rbx: length of first string
  // rcx: length of second string
  // rdx: second string
  // r8: map of first string if string check was performed above
  // r9: map of second string if string check was performed above
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);

  // If arguments where known to be strings, maps are not loaded to r8 and r9
  // by the code above.
  if (!string_check_) {
    __ movq(r8, FieldOperand(rax, HeapObject::kMapOffset));
    __ movq(r9, FieldOperand(rdx, HeapObject::kMapOffset));
  }
  // Get the instance types of the two strings as they will be needed soon.
  __ movzxbl(r8, FieldOperand(r8, Map::kInstanceTypeOffset));
  __ movzxbl(r9, FieldOperand(r9, Map::kInstanceTypeOffset));

  // Look at the length of the result of adding the two strings.
  STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue / 2);
  __ SmiAdd(rbx, rbx, rcx, NULL);
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ SmiCompare(rbx, Smi::FromInt(2));
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ascii strings.
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(r8, r9, rbx, rcx,
                                                  &string_add_runtime);

  // Get the two characters forming the sub string.
  __ movzxbq(rbx, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  __ movzxbq(rcx, FieldOperand(rdx, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_flat_ascii_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, rbx, rcx, r14, r11, rdi, r12, &make_two_character_string);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&make_two_character_string);
  __ Set(rbx, 2);
  __ jmp(&make_flat_ascii_string);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ SmiCompare(rbx, Smi::FromInt(String::kMinNonFlatLength));
  __ j(below, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  __ SmiCompare(rbx, Smi::FromInt(String::kMaxLength));
  __ j(above, &string_add_runtime);

  // If result is not supposed to be flat, allocate a cons string object. If
  // both strings are ascii the result is an ascii cons string.
  // rax: first string
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii, allocated, ascii_data;
  __ movl(rcx, r8);
  __ and_(rcx, r9);
  STATIC_ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ testl(rcx, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii);
  __ bind(&ascii_data);
  // Allocate an acsii cons string.
  __ AllocateAsciiConsString(rcx, rdi, no_reg, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ movq(FieldOperand(rcx, ConsString::kLengthOffset), rbx);
  __ movq(FieldOperand(rcx, ConsString::kHashFieldOffset),
          Immediate(String::kEmptyHashField));
  __ movq(FieldOperand(rcx, ConsString::kFirstOffset), rax);
  __ movq(FieldOperand(rcx, ConsString::kSecondOffset), rdx);
  __ movq(rax, rcx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ascii characters.
  // rcx: first instance type AND second instance type.
  // r8: first instance type.
  // r9: second instance type.
  __ testb(rcx, Immediate(kAsciiDataHintMask));
  __ j(not_zero, &ascii_data);
  __ xor_(r8, r9);
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ andb(r8, Immediate(kAsciiStringTag | kAsciiDataHintTag));
  __ cmpb(r8, Immediate(kAsciiStringTag | kAsciiDataHintTag));
  __ j(equal, &ascii_data);
  // Allocate a two byte cons string.
  __ AllocateConsString(rcx, rdi, no_reg, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are not
  // external strings.
  // rax: first string
  // rbx: length of resulting flat string as smi
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  __ bind(&string_add_flat_result);
  __ SmiToInteger32(rbx, rbx);
  __ movl(rcx, r8);
  __ and_(rcx, Immediate(kStringRepresentationMask));
  __ cmpl(rcx, Immediate(kExternalStringTag));
  __ j(equal, &string_add_runtime);
  __ movl(rcx, r9);
  __ and_(rcx, Immediate(kStringRepresentationMask));
  __ cmpl(rcx, Immediate(kExternalStringTag));
  __ j(equal, &string_add_runtime);
  // Now check if both strings are ascii strings.
  // rax: first string
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii_string_add_flat_result;
  STATIC_ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ testl(r8, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii_string_add_flat_result);
  __ testl(r9, Immediate(kAsciiStringTag));
  __ j(zero, &string_add_runtime);

  __ bind(&make_flat_ascii_string);
  // Both strings are ascii strings. As they are short they are both flat.
  __ AllocateAsciiString(rcx, rbx, rdi, r14, r11, &string_add_runtime);
  // rcx: result string
  __ movq(rbx, rcx);
  // Locate first character of result.
  __ addq(rcx, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument
  __ SmiToInteger32(rdi, FieldOperand(rax, String::kLengthOffset));
  __ addq(rax, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // rax: first char of first argument
  // rbx: result string
  // rcx: first character of result
  // rdx: second string
  // rdi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, rcx, rax, rdi, true);
  // Locate first character of second argument.
  __ SmiToInteger32(rdi, FieldOperand(rdx, String::kLengthOffset));
  __ addq(rdx, Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // rbx: result string
  // rcx: next character of result
  // rdx: first char of second argument
  // rdi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, rcx, rdx, rdi, true);
  __ movq(rax, rbx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Handle creating a flat two byte result.
  // rax: first string - known to be two byte
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  __ bind(&non_ascii_string_add_flat_result);
  __ and_(r9, Immediate(kAsciiStringTag));
  __ j(not_zero, &string_add_runtime);
  // Both strings are two byte strings. As they are short they are both
  // flat.
  __ AllocateTwoByteString(rcx, rbx, rdi, r14, r11, &string_add_runtime);
  // rcx: result string
  __ movq(rbx, rcx);
  // Locate first character of result.
  __ addq(rcx, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ SmiToInteger32(rdi, FieldOperand(rax, String::kLengthOffset));
  __ addq(rax, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // rax: first char of first argument
  // rbx: result string
  // rcx: first character of result
  // rdx: second argument
  // rdi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, rcx, rax, rdi, false);
  // Locate first character of second argument.
  __ SmiToInteger32(rdi, FieldOperand(rdx, String::kLengthOffset));
  __ addq(rdx, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // rbx: result string
  // rcx: next character of result
  // rdx: first char of second argument
  // rdi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, rcx, rdx, rdi, false);
  __ movq(rax, rbx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
  __ TailCallRuntime(Runtime::kStringAdd, 2, 1);
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
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (!ascii) {
    STATIC_ASSERT(2 == sizeof(uc16));
    __ addl(count, count);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ testl(count, Immediate(~7));
  __ j(zero, &last_bytes);

  // Copy from edi to esi using rep movs instruction.
  __ movl(kScratchRegister, count);
  __ shr(count, Immediate(3));  // Number of doublewords to copy.
  __ repmovsq();

  // Find number of bytes left.
  __ movl(count, kScratchRegister);
  __ and_(count, Immediate(7));

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ testl(count, count);
  __ j(zero, &done);

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

void StringHelper::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
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
  // different hash algorithm. Don't try to look for these in the symbol table.
  Label not_array_index;
  __ leal(scratch, Operand(c1, -'0'));
  __ cmpl(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index);
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

  // Load the symbol table.
  Register symbol_table = c2;
  __ LoadRoot(symbol_table, Heap::kSymbolTableRootIndex);

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ SmiToInteger32(mask,
                    FieldOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ decl(mask);

  Register undefined = scratch4;
  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string (32-bit int)
  // symbol_table: symbol table
  // mask:         capacity mask (32-bit int)
  // undefined:    undefined value
  // scratch:      -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes];
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in symbol table.
    __ movl(scratch, hash);
    if (i > 0) {
      __ addl(scratch, Immediate(SymbolTable::GetProbeOffset(i)));
    }
    __ andl(scratch, mask);

    // Load the entry from the symble table.
    Register candidate = scratch;  // Scratch register contains candidate.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ movq(candidate,
            FieldOperand(symbol_table,
                         scratch,
                         times_pointer_size,
                         SymbolTable::kElementsStartOffset));

    // If entry is undefined no string with this hash can be found.
    __ cmpq(candidate, undefined);
    __ j(equal, not_found);

    // If length is not 2 the string is not a candidate.
    __ SmiCompare(FieldOperand(candidate, String::kLengthOffset),
                  Smi::FromInt(2));
    __ j(not_equal, &next_probe[i]);

    // We use kScratchRegister as a temporary register in assumption that
    // JumpIfInstanceTypeIsNotSequentialAscii does not use it implicitly
    Register temp = kScratchRegister;

    // Check that the candidate is a non-external ascii string.
    __ movq(temp, FieldOperand(candidate, HeapObject::kMapOffset));
    __ movzxbl(temp, FieldOperand(temp, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe[i]);

    // Check if the two characters match.
    __ movl(temp, FieldOperand(candidate, SeqAsciiString::kHeaderSize));
    __ andl(temp, Immediate(0x0000ffff));
    __ cmpl(chars, temp);
    __ j(equal, &found_in_symbol_table);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = scratch;
  __ bind(&found_in_symbol_table);
  if (!result.is(rax)) {
    __ movq(rax, result);
  }
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = character + (character << 10);
  __ movl(hash, character);
  __ shll(hash, Immediate(10));
  __ addl(hash, character);
  // hash ^= hash >> 6;
  __ movl(scratch, hash);
  __ sarl(scratch, Immediate(6));
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
  __ sarl(scratch, Immediate(6));
  __ xorl(hash, scratch);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ leal(hash, Operand(hash, hash, times_8, 0));
  // hash ^= hash >> 11;
  __ movl(scratch, hash);
  __ sarl(scratch, Immediate(11));
  __ xorl(hash, scratch);
  // hash += hash << 15;
  __ movl(scratch, hash);
  __ shll(scratch, Immediate(15));
  __ addl(hash, scratch);

  // if (hash == 0) hash = 27;
  Label hash_not_zero;
  __ j(not_zero, &hash_not_zero);
  __ movl(hash, Immediate(27));
  __ bind(&hash_not_zero);
}

void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: to
  //  rsp[16]: from
  //  rsp[24]: string

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
  Label result_longer_than_two;
  __ movq(rcx, Operand(rsp, kToOffset));
  __ movq(rdx, Operand(rsp, kFromOffset));
  __ JumpIfNotBothPositiveSmi(rcx, rdx, &runtime);

  __ SmiSub(rcx, rcx, rdx, NULL);  // Overflow doesn't happen.
  __ cmpq(FieldOperand(rax, String::kLengthOffset), rcx);
  Label return_rax;
  __ j(equal, &return_rax);
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ SmiToInteger32(rcx, rcx);
  __ cmpl(rcx, Immediate(2));
  __ j(greater, &result_longer_than_two);
  __ j(less, &runtime);

  // Sub string of length 2 requested.
  // rax: string
  // rbx: instance type
  // rcx: sub string length (value is 2)
  // rdx: from index (smi)
  __ JumpIfInstanceTypeIsNotSequentialAscii(rbx, rbx, &runtime);

  // Get the two characters forming the sub string.
  __ SmiToInteger32(rdx, rdx);  // From index is no longer smi.
  __ movzxbq(rbx, FieldOperand(rax, rdx, times_1, SeqAsciiString::kHeaderSize));
  __ movzxbq(rcx,
             FieldOperand(rax, rdx, times_1, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, rbx, rcx, rax, rdx, rdi, r14, &make_two_character_string);
  __ ret(3 * kPointerSize);

  __ bind(&make_two_character_string);
  // Setup registers for allocating the two character string.
  __ movq(rax, Operand(rsp, kStringOffset));
  __ movq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ Set(rcx, 2);

  __ bind(&result_longer_than_two);

  // rax: string
  // rbx: instance type
  // rcx: result string length
  // Check for flat ascii string
  Label non_ascii_flat;
  __ JumpIfInstanceTypeIsNotSequentialAscii(rbx, rbx, &non_ascii_flat);

  // Allocate the result.
  __ AllocateAsciiString(rax, rcx, rbx, rdx, rdi, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(rdx, rsi);  // esi used by following code.
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  // Load string argument and locate character of sub string start.
  __ movq(rsi, Operand(rsp, kStringOffset));
  __ movq(rbx, Operand(rsp, kFromOffset));
  {
    SmiIndex smi_as_index = masm->SmiToIndex(rbx, rbx, times_1);
    __ lea(rsi, Operand(rsi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }

  // rax: result string
  // rcx: result length
  // rdx: original value of rsi
  // rdi: first character of result
  // rsi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, true);
  __ movq(rsi, rdx);  // Restore rsi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(kArgumentsSize);

  __ bind(&non_ascii_flat);
  // rax: string
  // rbx: instance type & kStringRepresentationMask | kStringEncodingMask
  // rcx: result string length
  // Check for sequential two byte string
  __ cmpb(rbx, Immediate(kSeqStringTag | kTwoByteStringTag));
  __ j(not_equal, &runtime);

  // Allocate the result.
  __ AllocateTwoByteString(rax, rcx, rbx, rdx, rdi, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(rdx, rsi);  // esi used by following code.
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqTwoByteString::kHeaderSize));
  // Load string argument and locate character of sub string start.
  __ movq(rsi, Operand(rsp, kStringOffset));
  __ movq(rbx, Operand(rsp, kFromOffset));
  {
    SmiIndex smi_as_index = masm->SmiToIndex(rbx, rbx, times_2);
    __ lea(rsi, Operand(rsi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }

  // rax: result string
  // rcx: result length
  // rdx: original value of rsi
  // rdi: first character of result
  // rsi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, false);
  __ movq(rsi, rdx);  // Restore esi.

  __ bind(&return_rax);
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(kArgumentsSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
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
            FieldOperand(right, String::kLengthOffset),
            NULL);
  // Register scratch4 now holds left.length - right.length.
  const Register length_difference = scratch4;
  Label left_shorter;
  __ j(less, &left_shorter);
  // The right string isn't longer that the left one.
  // Get the right string's length by subtracting the (non-negative) difference
  // from the left string's length.
  __ SmiSub(scratch1, scratch1, length_difference, NULL);
  __ bind(&left_shorter);
  // Register scratch1 now holds Min(left.length, right.length).
  const Register min_length = scratch1;

  Label compare_lengths;
  // If min-length is zero, go directly to comparing lengths.
  __ SmiTest(min_length);
  __ j(zero, &compare_lengths);

  __ SmiToInteger32(min_length, min_length);

  // Registers scratch2 and scratch3 are free.
  Label result_not_equal;
  Label loop;
  {
    // Check characters 0 .. min_length - 1 in a loop.
    // Use scratch3 as loop index, min_length as limit and scratch2
    // for computation.
    const Register index = scratch3;
    __ movl(index, Immediate(0));  // Index into strings.
    __ bind(&loop);
    // Compare characters.
    // TODO(lrn): Could we load more than one character at a time?
    __ movb(scratch2, FieldOperand(left,
                                   index,
                                   times_1,
                                   SeqAsciiString::kHeaderSize));
    // Increment index and use -1 modifier on next load to give
    // the previous load extra time to complete.
    __ addl(index, Immediate(1));
    __ cmpb(scratch2, FieldOperand(right,
                                   index,
                                   times_1,
                                   SeqAsciiString::kHeaderSize - 1));
    __ j(not_equal, &result_not_equal);
    __ cmpl(index, min_length);
    __ j(not_equal, &loop);
  }
  // Completed loop without finding different characters.
  // Compare lengths (precomputed).
  __ bind(&compare_lengths);
  __ SmiTest(length_difference);
  __ j(not_zero, &result_not_equal);

  // Result is EQUAL.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  Label result_greater;
  __ bind(&result_not_equal);
  // Unequal comparison of left to right, either character or length.
  __ j(greater, &result_greater);

  // Result is LESS.
  __ Move(rax, Smi::FromInt(LESS));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(rax, Smi::FromInt(GREATER));
  __ ret(0);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: right string
  //  rsp[16]: left string

  __ movq(rdx, Operand(rsp, 2 * kPointerSize));  // left
  __ movq(rax, Operand(rsp, 1 * kPointerSize));  // right

  // Check for identity.
  Label not_same;
  __ cmpq(rdx, rax);
  __ j(not_equal, &not_same);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ IncrementCounter(&Counters::string_compare_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(rdx, rax, rcx, rbx, &runtime);

  // Inline comparison of ascii strings.
  __ IncrementCounter(&Counters::string_compare_native, 1);
  // Drop arguments from the stack
  __ pop(rcx);
  __ addq(rsp, Immediate(2 * kPointerSize));
  __ push(rcx);
  GenerateCompareFlatAsciiStrings(masm, rdx, rax, rcx, rbx, rdi, r8);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
