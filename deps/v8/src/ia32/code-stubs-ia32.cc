// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "code-stubs.h"
#include "bootstrapper.h"
#include "jsregexp.h"
#include "regexp-macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in eax.
  NearLabel check_heap_number, call_builtin;
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &check_heap_number);
  __ ret(0);

  __ bind(&check_heap_number);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(Operand(ebx), Immediate(Factory::heap_number_map()));
  __ j(not_equal, &call_builtin);
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
  Label gc;
  __ AllocateInNewSpace(JSFunction::kSize, eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the function info from the stack.
  __ mov(edx, Operand(esp, 1 * kPointerSize));

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ mov(ecx, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(ecx, FieldOperand(ecx, GlobalObject::kGlobalContextOffset));
  __ mov(ecx, Operand(ecx, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ mov(FieldOperand(eax, JSObject::kMapOffset), ecx);

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ mov(ebx, Immediate(Factory::empty_fixed_array()));
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset), ebx);
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), ebx);
  __ mov(FieldOperand(eax, JSFunction::kPrototypeOrInitialMapOffset),
         Immediate(Factory::the_hole_value()));
  __ mov(FieldOperand(eax, JSFunction::kSharedFunctionInfoOffset), edx);
  __ mov(FieldOperand(eax, JSFunction::kContextOffset), esi);
  __ mov(FieldOperand(eax, JSFunction::kLiteralsOffset), ebx);
  __ mov(FieldOperand(eax, JSFunction::kNextFunctionLinkOffset),
         Immediate(Factory::undefined_value()));

  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  __ mov(edx, FieldOperand(edx, SharedFunctionInfo::kCodeOffset));
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ mov(FieldOperand(eax, JSFunction::kCodeEntryOffset), edx);

  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(ecx);  // Temporarily remove return address.
  __ pop(edx);
  __ push(esi);
  __ push(edx);
  __ push(Immediate(Factory::false_value()));
  __ push(ecx);  // Restore return address.
  __ TailCallRuntime(Runtime::kNewClosure, 3, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace((length * kPointerSize) + FixedArray::kHeaderSize,
                        eax, ebx, ecx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));

  // Setup the object header.
  __ mov(FieldOperand(eax, HeapObject::kMapOffset), Factory::context_map());
  __ mov(FieldOperand(eax, Context::kLengthOffset),
         Immediate(Smi::FromInt(length)));

  // Setup the fixed slots.
  __ Set(ebx, Immediate(0));  // Set to NULL.
  __ mov(Operand(eax, Context::SlotOffset(Context::CLOSURE_INDEX)), ecx);
  __ mov(Operand(eax, Context::SlotOffset(Context::FCONTEXT_INDEX)), eax);
  __ mov(Operand(eax, Context::SlotOffset(Context::PREVIOUS_INDEX)), ebx);
  __ mov(Operand(eax, Context::SlotOffset(Context::EXTENSION_INDEX)), ebx);

  // Copy the global object from the surrounding context. We go through the
  // context in the function (ecx) to match the allocation behavior we have
  // in the runtime system (see Heap::AllocateFunctionContext).
  __ mov(ebx, FieldOperand(ecx, JSFunction::kContextOffset));
  __ mov(ebx, Operand(ebx, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(Operand(eax, Context::SlotOffset(Context::GLOBAL_INDEX)), ebx);

  // Initialize the rest of the slots to undefined.
  __ mov(ebx, Factory::undefined_value());
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ mov(Operand(eax, Context::SlotOffset(i)), ebx);
  }

  // Return and remove the on-stack parameter.
  __ mov(esi, Operand(eax));
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewContext, 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [esp + kPointerSize]: constant elements.
  // [esp + (2 * kPointerSize)]: literal index.
  // [esp + (3 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into ecx and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ mov(ecx, Operand(esp, 3 * kPointerSize));
  __ mov(eax, Operand(esp, 2 * kPointerSize));
  STATIC_ASSERT(kPointerSize == 4);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(ecx, FieldOperand(ecx, eax, times_half_pointer_size,
                           FixedArray::kHeaderSize));
  __ cmp(ecx, Factory::undefined_value());
  __ j(equal, &slow_case);

  if (FLAG_debug_code) {
    const char* message;
    Handle<Map> expected_map;
    if (mode_ == CLONE_ELEMENTS) {
      message = "Expected (writable) fixed array";
      expected_map = Factory::fixed_array_map();
    } else {
      ASSERT(mode_ == COPY_ON_WRITE_ELEMENTS);
      message = "Expected copy-on-write fixed array";
      expected_map = Factory::fixed_cow_array_map();
    }
    __ push(ecx);
    __ mov(ecx, FieldOperand(ecx, JSArray::kElementsOffset));
    __ cmp(FieldOperand(ecx, HeapObject::kMapOffset), expected_map);
    __ Assert(equal, message);
    __ pop(ecx);
  }

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size, eax, ebx, edx, &slow_case, TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ mov(ebx, FieldOperand(ecx, i));
      __ mov(FieldOperand(eax, i), ebx);
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ mov(ecx, FieldOperand(ecx, JSArray::kElementsOffset));
    __ lea(edx, Operand(eax, JSArray::kSize));
    __ mov(FieldOperand(eax, JSArray::kElementsOffset), edx);

    // Copy the elements array.
    for (int i = 0; i < elements_size; i += kPointerSize) {
      __ mov(ebx, FieldOperand(ecx, i));
      __ mov(FieldOperand(edx, i), ebx);
    }
  }

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateArrayLiteralShallow, 3, 1);
}


// NOTE: The stub does not handle the inlined cases (Smis, Booleans, undefined).
void ToBooleanStub::Generate(MacroAssembler* masm) {
  NearLabel false_result, true_result, not_string;
  __ mov(eax, Operand(esp, 1 * kPointerSize));

  // 'null' => false.
  __ cmp(eax, Factory::null_value());
  __ j(equal, &false_result);

  // Get the map and type of the heap object.
  __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(edx, Map::kInstanceTypeOffset));

  // Undetectable => false.
  __ test_b(FieldOperand(edx, Map::kBitFieldOffset),
            1 << Map::kIsUndetectable);
  __ j(not_zero, &false_result);

  // JavaScript object => true.
  __ CmpInstanceType(edx, FIRST_JS_OBJECT_TYPE);
  __ j(above_equal, &true_result);

  // String value => false iff empty.
  __ CmpInstanceType(edx, FIRST_NONSTRING_TYPE);
  __ j(above_equal, &not_string);
  STATIC_ASSERT(kSmiTag == 0);
  __ cmp(FieldOperand(eax, String::kLengthOffset), Immediate(0));
  __ j(zero, &false_result);
  __ jmp(&true_result);

  __ bind(&not_string);
  // HeapNumber => false iff +0, -0, or NaN.
  __ cmp(edx, Factory::heap_number_map());
  __ j(not_equal, &true_result);
  __ fldz();
  __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
  __ FCmp();
  __ j(zero, &false_result);
  // Fall through to |true_result|.

  // Return 1/0 for true/false in eax.
  __ bind(&true_result);
  __ mov(eax, 1);
  __ ret(1 * kPointerSize);
  __ bind(&false_result);
  __ mov(eax, 0);
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
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (!(left.is(left_arg) && right.is(right_arg))) {
      if (left.is(right_arg) && right.is(left_arg)) {
        if (IsOperationCommutative()) {
          SetArgsReversed();
        } else {
          __ xchg(left, right);
        }
      } else if (left.is(left_arg)) {
        __ mov(right_arg, right);
      } else if (right.is(right_arg)) {
        __ mov(left_arg, left);
      } else if (left.is(right_arg)) {
        if (IsOperationCommutative()) {
          __ mov(left_arg, right);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying left argument.
          __ mov(left_arg, left);
          __ mov(right_arg, right);
        }
      } else if (right.is(left_arg)) {
        if (IsOperationCommutative()) {
          __ mov(right_arg, left);
          SetArgsReversed();
        } else {
          // Order of moves important to avoid destroying right argument.
          __ mov(right_arg, right);
          __ mov(left_arg, left);
        }
      } else {
        // Order of moves is not important.
        __ mov(left_arg, left);
        __ mov(right_arg, right);
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
    __ push(Immediate(right));
  } else {
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (left.is(left_arg)) {
      __ mov(right_arg, Immediate(right));
    } else if (left.is(right_arg) && IsOperationCommutative()) {
      __ mov(left_arg, Immediate(right));
      SetArgsReversed();
    } else {
      // For non-commutative operations, left and right_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite left before moving
      // it to left_arg.
      __ mov(left_arg, left);
      __ mov(right_arg, Immediate(right));
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
    __ push(Immediate(left));
    __ push(right);
  } else {
    // The calling convention with registers is left in edx and right in eax.
    Register left_arg = edx;
    Register right_arg = eax;
    if (right.is(right_arg)) {
      __ mov(left_arg, Immediate(left));
    } else if (right.is(left_arg) && IsOperationCommutative()) {
      __ mov(right_arg, Immediate(left));
      SetArgsReversed();
    } else {
      // For non-commutative operations, right and left_arg might be
      // the same register.  Therefore, the order of the moves is
      // important here in order to not overwrite right before moving
      // it to right_arg.
      __ mov(right_arg, right);
      __ mov(left_arg, Immediate(left));
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

  // Checks that the two floating point numbers on top of the FPU stack
  // have int32 values.
  static void CheckFloatOperandsAreInt32(MacroAssembler* masm,
                                         Label* non_int32);

  // Takes the operands in edx and eax and loads them as integers in eax
  // and ecx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             TypeInfo type_info,
                             bool use_sse3,
                             Label* operand_conversion_failure);
  static void LoadNumbersAsIntegers(MacroAssembler* masm,
                                    TypeInfo type_info,
                                    bool use_sse3,
                                    Label* operand_conversion_failure);
  static void LoadUnknownsAsIntegers(MacroAssembler* masm,
                                     bool use_sse3,
                                     Label* operand_conversion_failure);

  // Must only be called after LoadUnknownsAsIntegers.  Assumes that the
  // operands are pushed on the stack, and that their conversions to int32
  // are in eax and ecx.  Checks that the original numbers were in the int32
  // range.
  static void CheckLoadedIntegersWereInt32(MacroAssembler* masm,
                                           bool use_sse3,
                                           Label* not_int32);

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
};


void GenericBinaryOpStub::GenerateSmiCode(MacroAssembler* masm, Label* slow) {
  // 1. Move arguments into edx, eax except for DIV and MOD, which need the
  // dividend in eax and edx free for the division.  Use eax, ebx for those.
  Comment load_comment(masm, "-- Load arguments");
  Register left = edx;
  Register right = eax;
  if (op_ == Token::DIV || op_ == Token::MOD) {
    left = eax;
    right = ebx;
    if (HasArgsInRegisters()) {
      __ mov(ebx, eax);
      __ mov(eax, edx);
    }
  }
  if (!HasArgsInRegisters()) {
    __ mov(right, Operand(esp, 1 * kPointerSize));
    __ mov(left, Operand(esp, 2 * kPointerSize));
  }

  if (static_operands_type_.IsSmi()) {
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(left);
      __ AbortIfNotSmi(right);
    }
    if (op_ == Token::BIT_OR) {
      __ or_(right, Operand(left));
      GenerateReturn(masm);
      return;
    } else if (op_ == Token::BIT_AND) {
      __ and_(right, Operand(left));
      GenerateReturn(masm);
      return;
    } else if (op_ == Token::BIT_XOR) {
      __ xor_(right, Operand(left));
      GenerateReturn(masm);
      return;
    }
  }

  // 2. Prepare the smi check of both operands by oring them together.
  Comment smi_check_comment(masm, "-- Smi check arguments");
  Label not_smis;
  Register combined = ecx;
  ASSERT(!left.is(combined) && !right.is(combined));
  switch (op_) {
    case Token::BIT_OR:
      // Perform the operation into eax and smi check the result.  Preserve
      // eax in case the result is not a smi.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));  // Bitwise or is commutative.
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
      __ or_(combined, Operand(left));
      break;

    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Move the right operand into ecx for the shift operation, use eax
      // for the smi check register.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));
      combined = right;
      break;

    default:
      break;
  }

  // 3. Perform the smi check of the operands.
  STATIC_ASSERT(kSmiTag == 0);  // Adjust zero check if not the case.
  __ test(combined, Immediate(kSmiTagMask));
  __ j(not_zero, &not_smis, not_taken);

  // 4. Operands are both smis, perform the operation leaving the result in
  // eax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op_) {
    case Token::BIT_OR:
      // Nothing to do.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(eax));
      __ xor_(right, Operand(left));  // Bitwise xor is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(eax));
      __ and_(right, Operand(left));  // Bitwise and is commutative.
      break;

    case Token::SHL:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shl_cl(left);
      // Check that the *signed* result fits in a smi.
      __ cmp(left, 0xc0000000);
      __ j(sign, &use_fp_on_smis, not_taken);
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
      __ j(not_zero, slow, not_taken);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::ADD:
      ASSERT(right.is(eax));
      __ add(right, Operand(left));  // Addition is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      break;

    case Token::SUB:
      __ sub(left, Operand(right));
      __ j(overflow, &use_fp_on_smis, not_taken);
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
      __ imul(right, Operand(left));  // Multiplication is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(right, combined, &use_fp_on_smis);
      break;

    case Token::DIV:
      // We can't revert the division if the result is not a smi so
      // save the left operand.
      __ mov(edi, left);
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &use_fp_on_smis, not_taken);
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
      __ test(edx, Operand(edx));
      __ j(not_zero, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(eax);
      break;

    case Token::MOD:
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &not_smis, not_taken);

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

  // 5. Emit return of result in eax.
  GenerateReturn(masm);

  // 6. For some operations emit inline code to perform floating point
  // operations on known smis (e.g., if the result of the operation
  // overflowed the smi range).
  switch (op_) {
    case Token::SHL: {
      Comment perform_float(masm, "-- Perform float operation on smis");
      __ bind(&use_fp_on_smis);
      if (runtime_operands_type_ != BinaryOpIC::UNINIT_OR_SMI) {
        // Result we want is in left == edx, so we can put the allocated heap
        // number in eax.
        __ AllocateHeapNumber(eax, ecx, ebx, slow);
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(left));
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          // It's OK to overwrite the right argument on the stack because we
          // are about to return.
          __ mov(Operand(esp, 1 * kPointerSize), left);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        }
        GenerateReturn(masm);
      } else {
        ASSERT(runtime_operands_type_ == BinaryOpIC::UNINIT_OR_SMI);
        __ jmp(slow);
      }
      break;
    }

    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Comment perform_float(masm, "-- Perform float operation on smis");
      __ bind(&use_fp_on_smis);
      // Restore arguments to edx, eax.
      switch (op_) {
        case Token::ADD:
          // Revert right = right + left.
          __ sub(right, Operand(left));
          break;
        case Token::SUB:
          // Revert left = left - right.
          __ add(left, Operand(right));
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
      if (runtime_operands_type_ != BinaryOpIC::UNINIT_OR_SMI) {
        __ AllocateHeapNumber(ecx, ebx, no_reg, slow);
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          FloatingPointHelper::LoadSSE2Smis(masm, ebx);
          switch (op_) {
            case Token::ADD: __ addsd(xmm0, xmm1); break;
            case Token::SUB: __ subsd(xmm0, xmm1); break;
            case Token::MUL: __ mulsd(xmm0, xmm1); break;
            case Token::DIV: __ divsd(xmm0, xmm1); break;
            default: UNREACHABLE();
          }
          __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm0);
        } else {  // SSE2 not available, use FPU.
          FloatingPointHelper::LoadFloatSmis(masm, ebx);
          switch (op_) {
            case Token::ADD: __ faddp(1); break;
            case Token::SUB: __ fsubp(1); break;
            case Token::MUL: __ fmulp(1); break;
            case Token::DIV: __ fdivp(1); break;
            default: UNREACHABLE();
          }
          __ fstp_d(FieldOperand(ecx, HeapNumber::kValueOffset));
        }
        __ mov(eax, ecx);
        GenerateReturn(masm);
      } else {
        ASSERT(runtime_operands_type_ == BinaryOpIC::UNINIT_OR_SMI);
        __ jmp(slow);
      }
      break;
    }

    default:
      break;
  }

  // 7. Non-smi operands, fall out to the non-smi code with the operands in
  // edx and eax.
  Comment done_comment(masm, "-- Enter non-smi code");
  __ bind(&not_smis);
  switch (op_) {
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


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  Label call_runtime;

  __ IncrementCounter(&Counters::generic_binary_stub_calls, 1);

  if (runtime_operands_type_ == BinaryOpIC::UNINIT_OR_SMI) {
    Label slow;
    if (ShouldGenerateSmiCode()) GenerateSmiCode(masm, &slow);
    __ bind(&slow);
    GenerateTypeTransition(masm);
  }

  // Generate fast case smi code if requested. This flag is set when the fast
  // case smi code is not generated by the caller. Generating it here will speed
  // up common operations.
  if (ShouldGenerateSmiCode()) {
    GenerateSmiCode(masm, &call_runtime);
  } else if (op_ != Token::MOD) {  // MOD goes straight to runtime.
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
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          if (static_operands_type_.IsNumber()) {
            if (FLAG_debug_code) {
              // Assert at runtime that inputs are only numbers.
              __ AbortIfNotNumber(edx);
              __ AbortIfNotNumber(eax);
            }
            if (static_operands_type_.IsSmi()) {
              if (FLAG_debug_code) {
                __ AbortIfNotSmi(edx);
                __ AbortIfNotSmi(eax);
              }
              FloatingPointHelper::LoadSSE2Smis(masm, ecx);
            } else {
              FloatingPointHelper::LoadSSE2Operands(masm);
            }
          } else {
            FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);
          }

          switch (op_) {
            case Token::ADD: __ addsd(xmm0, xmm1); break;
            case Token::SUB: __ subsd(xmm0, xmm1); break;
            case Token::MUL: __ mulsd(xmm0, xmm1); break;
            case Token::DIV: __ divsd(xmm0, xmm1); break;
            default: UNREACHABLE();
          }
          GenerateHeapResultAllocation(masm, &call_runtime);
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
          GenerateReturn(masm);
        } else {  // SSE2 not available, use FPU.
          if (static_operands_type_.IsNumber()) {
            if (FLAG_debug_code) {
              // Assert at runtime that inputs are only numbers.
              __ AbortIfNotNumber(edx);
              __ AbortIfNotNumber(eax);
            }
          } else {
            FloatingPointHelper::CheckFloatOperands(masm, &not_floats, ebx);
          }
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
          GenerateHeapResultAllocation(masm, &after_alloc_failure);
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
          GenerateReturn(masm);
          __ bind(&after_alloc_failure);
          __ ffree();
          __ jmp(&call_runtime);
        }
        __ bind(&not_floats);
        if (runtime_operands_type_ == BinaryOpIC::DEFAULT &&
            !HasSmiCodeInStub()) {
          // Execution reaches this point when the first non-number argument
          // occurs (and only if smi code is skipped from the stub, otherwise
          // the patching has already been done earlier in this case branch).
          // Try patching to STRINGS for ADD operation.
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
        Label non_smi_result;
        FloatingPointHelper::LoadAsIntegers(masm,
                                            static_operands_type_,
                                            use_sse3_,
                                            &call_runtime);
        switch (op_) {
          case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
          case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
          case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
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
          __ j(negative, &non_smi_result);
        }
        // Tag smi result and return.
        __ SmiTag(eax);
        GenerateReturn(masm);

        // All ops except SHR return a signed int32 that we load in
        // a HeapNumber.
        if (op_ != Token::SHR) {
          __ bind(&non_smi_result);
          // Allocate a heap number if needed.
          __ mov(ebx, Operand(eax));  // ebx: result
          NearLabel skip_allocation;
          switch (mode_) {
            case OVERWRITE_LEFT:
            case OVERWRITE_RIGHT:
              // If the operand was an object, we skip the
              // allocation of a heap number.
              __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                  1 * kPointerSize : 2 * kPointerSize));
              __ test(eax, Immediate(kSmiTagMask));
              __ j(not_zero, &skip_allocation, not_taken);
              // Fall through!
            case NO_OVERWRITE:
              __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
              __ bind(&skip_allocation);
              break;
            default: UNREACHABLE();
          }
          // Store the result in the HeapNumber and return.
          if (CpuFeatures::IsSupported(SSE2)) {
            CpuFeatures::Scope use_sse2(SSE2);
            __ cvtsi2sd(xmm0, Operand(ebx));
            __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
          } else {
            __ mov(Operand(esp, 1 * kPointerSize), ebx);
            __ fild_s(Operand(esp, 1 * kPointerSize));
            __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
          }
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

  // Avoid hitting the string ADD code below when allocation fails in
  // the floating point code above.
  if (op_ != Token::ADD) {
    __ bind(&call_runtime);
  }

  if (HasArgsInRegisters()) {
    GenerateRegisterArgsPush(masm);
  }

  switch (op_) {
    case Token::ADD: {
      // Test for string arguments before calling runtime.

      // If this stub has already generated FP-specific code then the arguments
      // are already in edx, eax
      if (!ShouldGenerateFPCode() && !HasArgsInRegisters()) {
        GenerateLoadArguments(masm);
      }

      // Registers containing left and right operands respectively.
      Register lhs, rhs;
      if (HasArgsReversed()) {
        lhs = eax;
        rhs = edx;
      } else {
        lhs = edx;
        rhs = eax;
      }

      // Test if left operand is a string.
      NearLabel lhs_not_string;
      __ test(lhs, Immediate(kSmiTagMask));
      __ j(zero, &lhs_not_string);
      __ CmpObjectType(lhs, FIRST_NONSTRING_TYPE, ecx);
      __ j(above_equal, &lhs_not_string);

      StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
      __ TailCallStub(&string_add_left_stub);

      NearLabel call_runtime_with_args;
      // Left operand is not a string, test right.
      __ bind(&lhs_not_string);
      __ test(rhs, Immediate(kSmiTagMask));
      __ j(zero, &call_runtime_with_args);
      __ CmpObjectType(rhs, FIRST_NONSTRING_TYPE, ecx);
      __ j(above_equal, &call_runtime_with_args);

      StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
      __ TailCallStub(&string_add_right_stub);

      // Neither argument is a string.
      __ bind(&call_runtime);
      if (HasArgsInRegisters()) {
        GenerateRegisterArgsPush(masm);
      }
      __ bind(&call_runtime_with_args);
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


void GenericBinaryOpStub::GenerateHeapResultAllocation(MacroAssembler* masm,
                                                       Label* alloc_failure) {
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
    case OVERWRITE_LEFT: {
      // If the argument in edx is already an object, we skip the
      // allocation of a heap number.
      __ test(edx, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now edx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(edx, Operand(ebx));
      __ bind(&skip_allocation);
      // Use object in edx as a result holder
      __ mov(eax, Operand(edx));
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in eax is already an object, we skip the
      // allocation of a heap number.
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
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


void GenericBinaryOpStub::GenerateLoadArguments(MacroAssembler* masm) {
  // If arguments are not passed in registers read them from the stack.
  ASSERT(!HasArgsInRegisters());
  __ mov(eax, Operand(esp, 1 * kPointerSize));
  __ mov(edx, Operand(esp, 2 * kPointerSize));
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
  __ pop(ecx);
  if (HasArgsReversed()) {
    __ push(eax);
    __ push(edx);
  } else {
    __ push(edx);
    __ push(eax);
  }
  __ push(ecx);
}


void GenericBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  // Ensure the operands are on the stack.
  if (HasArgsInRegisters()) {
    GenerateRegisterArgsPush(masm);
  }

  __ pop(ecx);  // Save return address.

  // Left and right arguments are now on top.
  // Push this stub's key. Although the operation and the type info are
  // encoded into the key, the encoding is opaque, so push them too.
  __ push(Immediate(Smi::FromInt(MinorKey())));
  __ push(Immediate(Smi::FromInt(op_)));
  __ push(Immediate(Smi::FromInt(runtime_operands_type_)));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch)),
      5,
      1);
}


Handle<Code> GetBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info) {
  GenericBinaryOpStub stub(key, type_info);
  return stub.GetCode();
}


Handle<Code> GetTypeRecordingBinaryOpStub(int key,
    TRBinaryOpIC::TypeInfo type_info,
    TRBinaryOpIC::TypeInfo result_type_info) {
  TypeRecordingBinaryOpStub stub(key, type_info, result_type_info);
  return stub.GetCode();
}


void TypeRecordingBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(ecx);  // Save return address.
  __ push(edx);
  __ push(eax);
  // Left and right arguments are now on top.
  // Push this stub's key. Although the operation and the type info are
  // encoded into the key, the encoding is opaque, so push them too.
  __ push(Immediate(Smi::FromInt(MinorKey())));
  __ push(Immediate(Smi::FromInt(op_)));
  __ push(Immediate(Smi::FromInt(operands_type_)));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kTypeRecordingBinaryOp_Patch)),
      5,
      1);
}


// Prepare for a type transition runtime call when the args are already on
// the stack, under the return address.
void TypeRecordingBinaryOpStub::GenerateTypeTransitionWithSavedArgs(
    MacroAssembler* masm) {
  __ pop(ecx);  // Save return address.
  // Left and right arguments are already on top of the stack.
  // Push this stub's key. Although the operation and the type info are
  // encoded into the key, the encoding is opaque, so push them too.
  __ push(Immediate(Smi::FromInt(MinorKey())));
  __ push(Immediate(Smi::FromInt(op_)));
  __ push(Immediate(Smi::FromInt(operands_type_)));

  __ push(ecx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kTypeRecordingBinaryOp_Patch)),
      5,
      1);
}


void TypeRecordingBinaryOpStub::Generate(MacroAssembler* masm) {
  switch (operands_type_) {
    case TRBinaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case TRBinaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case TRBinaryOpIC::INT32:
      GenerateInt32Stub(masm);
      break;
    case TRBinaryOpIC::HEAP_NUMBER:
      GenerateHeapNumberStub(masm);
      break;
    case TRBinaryOpIC::ODDBALL:
      GenerateOddballStub(masm);
      break;
    case TRBinaryOpIC::STRING:
      GenerateStringStub(masm);
      break;
    case TRBinaryOpIC::GENERIC:
      GenerateGeneric(masm);
      break;
    default:
      UNREACHABLE();
  }
}


const char* TypeRecordingBinaryOpStub::GetName() {
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
               "TypeRecordingBinaryOpStub_%s_%s_%s",
               op_name,
               overwrite_name,
               TRBinaryOpIC::GetName(operands_type_));
  return name_;
}


void TypeRecordingBinaryOpStub::GenerateSmiCode(MacroAssembler* masm,
    Label* slow,
    SmiCodeGenerateHeapNumberResults allow_heapnumber_results) {
  // 1. Move arguments into edx, eax except for DIV and MOD, which need the
  // dividend in eax and edx free for the division.  Use eax, ebx for those.
  Comment load_comment(masm, "-- Load arguments");
  Register left = edx;
  Register right = eax;
  if (op_ == Token::DIV || op_ == Token::MOD) {
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
  switch (op_) {
    case Token::BIT_OR:
      // Perform the operation into eax and smi check the result.  Preserve
      // eax in case the result is not a smi.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));  // Bitwise or is commutative.
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
      __ or_(combined, Operand(left));
      break;

    case Token::SHL:
    case Token::SAR:
    case Token::SHR:
      // Move the right operand into ecx for the shift operation, use eax
      // for the smi check register.
      ASSERT(!left.is(ecx) && !right.is(ecx));
      __ mov(ecx, right);
      __ or_(right, Operand(left));
      combined = right;
      break;

    default:
      break;
  }

  // 3. Perform the smi check of the operands.
  STATIC_ASSERT(kSmiTag == 0);  // Adjust zero check if not the case.
  __ test(combined, Immediate(kSmiTagMask));
  __ j(not_zero, &not_smis, not_taken);

  // 4. Operands are both smis, perform the operation leaving the result in
  // eax and check the result if necessary.
  Comment perform_smi(masm, "-- Perform smi operation");
  Label use_fp_on_smis;
  switch (op_) {
    case Token::BIT_OR:
      // Nothing to do.
      break;

    case Token::BIT_XOR:
      ASSERT(right.is(eax));
      __ xor_(right, Operand(left));  // Bitwise xor is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(eax));
      __ and_(right, Operand(left));  // Bitwise and is commutative.
      break;

    case Token::SHL:
      // Remove tags from operands (but keep sign).
      __ SmiUntag(left);
      __ SmiUntag(ecx);
      // Perform the operation.
      __ shl_cl(left);
      // Check that the *signed* result fits in a smi.
      __ cmp(left, 0xc0000000);
      __ j(sign, &use_fp_on_smis, not_taken);
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
      __ j(not_zero, slow, not_taken);
      // Tag the result and store it in register eax.
      __ SmiTag(left);
      __ mov(eax, left);
      break;

    case Token::ADD:
      ASSERT(right.is(eax));
      __ add(right, Operand(left));  // Addition is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      break;

    case Token::SUB:
      __ sub(left, Operand(right));
      __ j(overflow, &use_fp_on_smis, not_taken);
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
      __ imul(right, Operand(left));  // Multiplication is commutative.
      __ j(overflow, &use_fp_on_smis, not_taken);
      // Check for negative zero result.  Use combined = left | right.
      __ NegativeZeroTest(right, combined, &use_fp_on_smis);
      break;

    case Token::DIV:
      // We can't revert the division if the result is not a smi so
      // save the left operand.
      __ mov(edi, left);
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &use_fp_on_smis, not_taken);
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
      __ test(edx, Operand(edx));
      __ j(not_zero, &use_fp_on_smis);
      // Tag the result and store it in register eax.
      __ SmiTag(eax);
      break;

    case Token::MOD:
      // Check for 0 divisor.
      __ test(right, Operand(right));
      __ j(zero, &not_smis, not_taken);

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
  switch (op_) {
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
  if (allow_heapnumber_results == NO_HEAPNUMBER_RESULTS) {
    __ bind(&use_fp_on_smis);
    switch (op_) {
      // Undo the effects of some operations, and some register moves.
      case Token::SHL:
        // The arguments are saved on the stack, and only used from there.
        break;
      case Token::ADD:
        // Revert right = right + left.
        __ sub(right, Operand(left));
        break;
      case Token::SUB:
        // Revert left = left - right.
        __ add(left, Operand(right));
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
    ASSERT(allow_heapnumber_results == ALLOW_HEAPNUMBER_RESULTS);
    switch (op_) {
      case Token::SHL: {
        Comment perform_float(masm, "-- Perform float operation on smis");
        __ bind(&use_fp_on_smis);
        // Result we want is in left == edx, so we can put the allocated heap
        // number in eax.
        __ AllocateHeapNumber(eax, ecx, ebx, slow);
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(left));
          __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        } else {
          // It's OK to overwrite the right argument on the stack because we
          // are about to return.
          __ mov(Operand(esp, 1 * kPointerSize), left);
          __ fild_s(Operand(esp, 1 * kPointerSize));
          __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
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
        switch (op_) {
          case Token::ADD:
            // Revert right = right + left.
            __ sub(right, Operand(left));
            break;
          case Token::SUB:
            // Revert left = left - right.
            __ add(left, Operand(right));
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
          CpuFeatures::Scope use_sse2(SSE2);
          FloatingPointHelper::LoadSSE2Smis(masm, ebx);
          switch (op_) {
            case Token::ADD: __ addsd(xmm0, xmm1); break;
            case Token::SUB: __ subsd(xmm0, xmm1); break;
            case Token::MUL: __ mulsd(xmm0, xmm1); break;
            case Token::DIV: __ divsd(xmm0, xmm1); break;
            default: UNREACHABLE();
          }
          __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm0);
        } else {  // SSE2 not available, use FPU.
          FloatingPointHelper::LoadFloatSmis(masm, ebx);
          switch (op_) {
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
  switch (op_) {
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


void TypeRecordingBinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
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

  if (result_type_ == TRBinaryOpIC::UNINITIALIZED ||
      result_type_ == TRBinaryOpIC::SMI) {
    GenerateSmiCode(masm, &call_runtime, NO_HEAPNUMBER_RESULTS);
  } else {
    GenerateSmiCode(masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);
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


void TypeRecordingBinaryOpStub::GenerateStringStub(MacroAssembler* masm) {
  ASSERT(operands_type_ == TRBinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  // Try to add arguments as strings, otherwise, transition to the generic
  // TRBinaryOpIC type.
  GenerateAddStrings(masm);
  GenerateTypeTransition(masm);
}


void TypeRecordingBinaryOpStub::GenerateInt32Stub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(operands_type_ == TRBinaryOpIC::INT32);

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Label not_floats;
      Label not_int32;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);
        FloatingPointHelper::CheckSSE2OperandsAreInt32(masm, &not_int32, ecx);
        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        // Check result type if it is currently Int32.
        if (result_type_ <= TRBinaryOpIC::INT32) {
          __ cvttsd2si(ecx, Operand(xmm0));
          __ cvtsi2sd(xmm2, Operand(ecx));
          __ ucomisd(xmm0, xmm2);
          __ j(not_zero, &not_int32);
          __ j(carry, &not_int32);
        }
        GenerateHeapResultAllocation(masm, &call_runtime);
        __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
        __ ret(0);
      } else {  // SSE2 not available, use FPU.
        FloatingPointHelper::CheckFloatOperands(masm, &not_floats, ebx);
        FloatingPointHelper::LoadFloatOperands(
            masm,
            ecx,
            FloatingPointHelper::ARGS_IN_REGISTERS);
        FloatingPointHelper::CheckFloatOperandsAreInt32(masm, &not_int32);
        switch (op_) {
          case Token::ADD: __ faddp(1); break;
          case Token::SUB: __ fsubp(1); break;
          case Token::MUL: __ fmulp(1); break;
          case Token::DIV: __ fdivp(1); break;
          default: UNREACHABLE();
        }
        Label after_alloc_failure;
        GenerateHeapResultAllocation(masm, &after_alloc_failure);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(0);
        __ bind(&after_alloc_failure);
        __ ffree();
        __ jmp(&call_runtime);
      }

      __ bind(&not_floats);
      __ bind(&not_int32);
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
      Label not_int32;
      Label non_smi_result;
      /*  {
        CpuFeatures::Scope use_sse2(SSE2);
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);
        FloatingPointHelper::CheckSSE2OperandsAreInt32(masm, &not_int32, ecx);
        }*/
      FloatingPointHelper::LoadUnknownsAsIntegers(masm,
                                                  use_sse3_,
                                                  &not_floats);
      FloatingPointHelper::CheckLoadedIntegersWereInt32(masm, use_sse3_,
                                                        &not_int32);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
        case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
        case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
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
        __ j(negative, &non_smi_result);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, Operand(eax));  // ebx: result
        NearLabel skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ test(eax, Immediate(kSmiTagMask));
            __ j(not_zero, &skip_allocation, not_taken);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(ebx));
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

  // If an allocation fails, or SHR or MOD hit a hard case,
  // use the runtime system to get the correct result.
  __ bind(&call_runtime);

  switch (op_) {
    case Token::ADD:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    case Token::SUB:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      GenerateRegisterArgsPush(masm);
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


void TypeRecordingBinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  Label call_runtime;

  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateAddStrings(masm);
  }

  // Convert odd ball arguments to numbers.
  NearLabel check, done;
  __ cmp(edx, Factory::undefined_value());
  __ j(not_equal, &check);
  if (Token::IsBitOp(op_)) {
    __ xor_(edx, Operand(edx));
  } else {
    __ mov(edx, Immediate(Factory::nan_value()));
  }
  __ jmp(&done);
  __ bind(&check);
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, &done);
  if (Token::IsBitOp(op_)) {
    __ xor_(eax, Operand(eax));
  } else {
    __ mov(eax, Immediate(Factory::nan_value()));
  }
  __ bind(&done);

  GenerateHeapNumberStub(masm);
}


void TypeRecordingBinaryOpStub::GenerateHeapNumberStub(MacroAssembler* masm) {
  Label call_runtime;

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Label not_floats;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        GenerateHeapResultAllocation(masm, &call_runtime);
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
        GenerateHeapResultAllocation(masm, &after_alloc_failure);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(0);
        __ bind(&after_alloc_failure);
        __ ffree();
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
      FloatingPointHelper::LoadUnknownsAsIntegers(masm,
                                                  use_sse3_,
                                                  &not_floats);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
        case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
        case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
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
        __ j(negative, &non_smi_result);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop two pushed arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, Operand(eax));  // ebx: result
        NearLabel skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
            // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ test(eax, Immediate(kSmiTagMask));
            __ j(not_zero, &skip_allocation, not_taken);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(ebx));
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
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    case Token::SUB:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      GenerateRegisterArgsPush(masm);
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


void TypeRecordingBinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime;

  __ IncrementCounter(&Counters::generic_binary_stub_calls, 1);

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

  GenerateSmiCode(masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);

  // Floating point case.
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      Label not_floats;
      if (CpuFeatures::IsSupported(SSE2)) {
        CpuFeatures::Scope use_sse2(SSE2);
        FloatingPointHelper::LoadSSE2Operands(masm, &not_floats);

        switch (op_) {
          case Token::ADD: __ addsd(xmm0, xmm1); break;
          case Token::SUB: __ subsd(xmm0, xmm1); break;
          case Token::MUL: __ mulsd(xmm0, xmm1); break;
          case Token::DIV: __ divsd(xmm0, xmm1); break;
          default: UNREACHABLE();
        }
        GenerateHeapResultAllocation(masm, &call_runtime);
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
        GenerateHeapResultAllocation(masm, &after_alloc_failure);
        __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
        __ ret(0);
        __ bind(&after_alloc_failure);
          __ ffree();
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
      FloatingPointHelper::LoadUnknownsAsIntegers(masm,
                                                  use_sse3_,
                                                  &call_runtime);
      switch (op_) {
        case Token::BIT_OR:  __ or_(eax, Operand(ecx)); break;
        case Token::BIT_AND: __ and_(eax, Operand(ecx)); break;
        case Token::BIT_XOR: __ xor_(eax, Operand(ecx)); break;
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
        __ j(negative, &non_smi_result);
      }
      // Tag smi result and return.
      __ SmiTag(eax);
      __ ret(2 * kPointerSize);  // Drop the arguments from the stack.

      // All ops except SHR return a signed int32 that we load in
      // a HeapNumber.
      if (op_ != Token::SHR) {
        __ bind(&non_smi_result);
        // Allocate a heap number if needed.
        __ mov(ebx, Operand(eax));  // ebx: result
        NearLabel skip_allocation;
        switch (mode_) {
          case OVERWRITE_LEFT:
          case OVERWRITE_RIGHT:
            // If the operand was an object, we skip the
              // allocation of a heap number.
            __ mov(eax, Operand(esp, mode_ == OVERWRITE_RIGHT ?
                                1 * kPointerSize : 2 * kPointerSize));
            __ test(eax, Immediate(kSmiTagMask));
            __ j(not_zero, &skip_allocation, not_taken);
            // Fall through!
          case NO_OVERWRITE:
            __ AllocateHeapNumber(eax, ecx, edx, &call_runtime);
            __ bind(&skip_allocation);
            break;
          default: UNREACHABLE();
        }
        // Store the result in the HeapNumber and return.
        if (CpuFeatures::IsSupported(SSE2)) {
          CpuFeatures::Scope use_sse2(SSE2);
          __ cvtsi2sd(xmm0, Operand(ebx));
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
    case Token::ADD: {
      GenerateAddStrings(masm);
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    }
    case Token::SUB:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      GenerateRegisterArgsPush(masm);
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      GenerateRegisterArgsPush(masm);
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


void TypeRecordingBinaryOpStub::GenerateAddStrings(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  NearLabel left_not_string, call_runtime;

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;

  // Test if left operand is a string.
  __ test(left, Immediate(kSmiTagMask));
  __ j(zero, &left_not_string);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &left_not_string);

  StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ test(right, Immediate(kSmiTagMask));
  __ j(zero, &call_runtime);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &call_runtime);

  StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_right_stub);

  // Neither argument is a string.
  __ bind(&call_runtime);
}


void TypeRecordingBinaryOpStub::GenerateHeapResultAllocation(
    MacroAssembler* masm,
    Label* alloc_failure) {
  Label skip_allocation;
  OverwriteMode mode = mode_;
  switch (mode) {
    case OVERWRITE_LEFT: {
      // If the argument in edx is already an object, we skip the
      // allocation of a heap number.
      __ test(edx, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(ebx, ecx, no_reg, alloc_failure);
      // Now edx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ mov(edx, Operand(ebx));
      __ bind(&skip_allocation);
      // Use object in edx as a result holder
      __ mov(eax, Operand(edx));
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in eax is already an object, we skip the
      // allocation of a heap number.
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &skip_allocation, not_taken);
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


void TypeRecordingBinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
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
    NearLabel input_not_smi;
    NearLabel loaded;
    __ mov(eax, Operand(esp, kPointerSize));
    __ test(eax, Immediate(kSmiTagMask));
    __ j(not_zero, &input_not_smi);
    // Input is a smi. Untag and load it onto the FPU stack.
    // Then load the low and high words of the double into ebx, edx.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ sar(eax, 1);
    __ sub(Operand(esp), Immediate(2 * kPointerSize));
    __ mov(Operand(esp, 0), eax);
    __ fild_s(Operand(esp, 0));
    __ fst_d(Operand(esp, 0));
    __ pop(edx);
    __ pop(ebx);
    __ jmp(&loaded);
    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
    __ cmp(Operand(ebx), Immediate(Factory::heap_number_map()));
    __ j(not_equal, &runtime_call);
    // Input is a HeapNumber. Push it on the FPU stack and load its
    // low and high words into ebx, edx.
    __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ mov(edx, FieldOperand(eax, HeapNumber::kExponentOffset));
    __ mov(ebx, FieldOperand(eax, HeapNumber::kMantissaOffset));

    __ bind(&loaded);
  } else {  // UNTAGGED.
    if (CpuFeatures::IsSupported(SSE4_1)) {
      CpuFeatures::Scope sse4_scope(SSE4_1);
      __ pextrd(Operand(edx), xmm1, 0x1);  // copy xmm1[63..32] to edx.
    } else {
      __ pshufd(xmm0, xmm1, 0x1);
      __ movd(Operand(edx), xmm0);
    }
    __ movd(Operand(ebx), xmm1);
  }

  // ST[0] or xmm1  == double value
  // ebx = low 32 bits of double value
  // edx = high 32 bits of double value
  // Compute hash (the shifts are arithmetic):
  //   h = (low ^ high); h ^= h >> 16; h ^= h >> 8; h = h & (cacheSize - 1);
  __ mov(ecx, ebx);
  __ xor_(ecx, Operand(edx));
  __ mov(eax, ecx);
  __ sar(eax, 16);
  __ xor_(ecx, Operand(eax));
  __ mov(eax, ecx);
  __ sar(eax, 8);
  __ xor_(ecx, Operand(eax));
  ASSERT(IsPowerOf2(TranscendentalCache::kCacheSize));
  __ and_(Operand(ecx), Immediate(TranscendentalCache::kCacheSize - 1));

  // ST[0] or xmm1 == double value.
  // ebx = low 32 bits of double value.
  // edx = high 32 bits of double value.
  // ecx = TranscendentalCache::hash(double value).
  __ mov(eax,
         Immediate(ExternalReference::transcendental_cache_array_address()));
  // Eax points to cache array.
  __ mov(eax, Operand(eax, type_ * sizeof(TranscendentalCache::caches_[0])));
  // Eax points to the cache for the type type_.
  // If NULL, the cache hasn't been initialized yet, so go through runtime.
  __ test(eax, Operand(eax));
  __ j(zero, &runtime_call_clear_stack);
#ifdef DEBUG
  // Check that the layout of cache elements match expectations.
  { TranscendentalCache::Element test_elem[2];
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
  NearLabel cache_miss;
  __ cmp(ebx, Operand(ecx, 0));
  __ j(not_equal, &cache_miss);
  __ cmp(edx, Operand(ecx, kIntSize));
  __ j(not_equal, &cache_miss);
  // Cache hit!
  __ mov(eax, Operand(ecx, 2 * kIntSize));
  if (tagged) {
    __ fstp(0);
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();
  }

  __ bind(&cache_miss);
  // Update cache with new value.
  // We are short on registers, so use no_reg as scratch.
  // This gives slightly larger code.
  if (tagged) {
    __ AllocateHeapNumber(eax, edi, no_reg, &runtime_call_clear_stack);
  } else {  // UNTAGGED.
    __ AllocateHeapNumber(eax, edi, no_reg, &skip_cache);
    __ sub(Operand(esp), Immediate(kDoubleSize));
    __ movdbl(Operand(esp, 0), xmm1);
    __ fld_d(Operand(esp, 0));
    __ add(Operand(esp), Immediate(kDoubleSize));
  }
  GenerateOperation(masm);
  __ mov(Operand(ecx, 0), ebx);
  __ mov(Operand(ecx, kIntSize), edx);
  __ mov(Operand(ecx, 2 * kIntSize), eax);
  __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
  if (tagged) {
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();

    // Skip cache and return answer directly, only in untagged case.
    __ bind(&skip_cache);
    __ sub(Operand(esp), Immediate(kDoubleSize));
    __ movdbl(Operand(esp, 0), xmm1);
    __ fld_d(Operand(esp, 0));
    GenerateOperation(masm);
    __ fstp_d(Operand(esp, 0));
    __ movdbl(xmm1, Operand(esp, 0));
    __ add(Operand(esp), Immediate(kDoubleSize));
    // We return the value in xmm1 without adding it to the cache, but
    // we cause a scavenging GC so that future allocations will succeed.
    __ EnterInternalFrame();
    // Allocate an unused object bigger than a HeapNumber.
    __ push(Immediate(Smi::FromInt(2 * kDoubleSize)));
    __ CallRuntimeSaveDoubles(Runtime::kAllocateInNewSpace);
    __ LeaveInternalFrame();
    __ Ret();
  }

  // Call runtime, doing whatever allocation and cleanup is necessary.
  if (tagged) {
    __ bind(&runtime_call_clear_stack);
    __ fstp(0);
    __ bind(&runtime_call);
    __ TailCallExternalReference(ExternalReference(RuntimeFunction()), 1, 1);
  } else {  // UNTAGGED.
    __ bind(&runtime_call_clear_stack);
    __ bind(&runtime_call);
    __ AllocateHeapNumber(eax, edi, no_reg, &skip_cache);
    __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm1);
    __ EnterInternalFrame();
    __ push(eax);
    __ CallRuntime(RuntimeFunction(), 1);
    __ LeaveInternalFrame();
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
    __ Ret();
  }
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    case TranscendentalCache::LOG: return Runtime::kMath_log;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void TranscendentalCacheStub::GenerateOperation(MacroAssembler* masm) {
  // Only free register is edi.
  // Input value is on FP stack, and also in ebx/edx.
  // Input value is possibly in xmm1.
  // Address of result (a newly allocated HeapNumber) may be in eax.
  if (type_ == TranscendentalCache::SIN || type_ == TranscendentalCache::COS) {
    // Both fsin and fcos require arguments in the range +/-2^63 and
    // return NaN for infinities and NaN. They can share all code except
    // the actual fsin/fcos operation.
    NearLabel in_range, done;
    // If argument is outside the range -2^63..2^63, fsin/cos doesn't
    // work. We must reduce it to the appropriate range.
    __ mov(edi, edx);
    __ and_(Operand(edi), Immediate(0x7ff00000));  // Exponent only.
    int supported_exponent_limit =
        (63 + HeapNumber::kExponentBias) << HeapNumber::kExponentShift;
    __ cmp(Operand(edi), Immediate(supported_exponent_limit));
    __ j(below, &in_range, taken);
    // Check for infinity and NaN. Both return NaN for sin.
    __ cmp(Operand(edi), Immediate(0x7ff00000));
    NearLabel non_nan_result;
    __ j(not_equal, &non_nan_result, taken);
    // Input is +/-Infinity or NaN. Result is NaN.
    __ fstp(0);
    // NaN is represented by 0x7ff8000000000000.
    __ push(Immediate(0x7ff80000));
    __ push(Immediate(0));
    __ fld_d(Operand(esp, 0));
    __ add(Operand(esp), Immediate(2 * kPointerSize));
    __ jmp(&done);

    __ bind(&non_nan_result);

    // Use fpmod to restrict argument to the range +/-2*PI.
    __ mov(edi, eax);  // Save eax before using fnstsw_ax.
    __ fldpi();
    __ fadd(0);
    __ fld(1);
    // FPU Stack: input, 2*pi, input.
    {
      NearLabel no_exceptions;
      __ fwait();
      __ fnstsw_ax();
      // Clear if Illegal Operand or Zero Division exceptions are set.
      __ test(Operand(eax), Immediate(5));
      __ j(zero, &no_exceptions);
      __ fnclex();
      __ bind(&no_exceptions);
    }

    // Compute st(0) % st(1)
    {
      NearLabel partial_remainder_loop;
      __ bind(&partial_remainder_loop);
      __ fprem1();
      __ fwait();
      __ fnstsw_ax();
      __ test(Operand(eax), Immediate(0x400 /* C2 */));
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
  } else {
    ASSERT(type_ == TranscendentalCache::LOG);
    __ fldln2();
    __ fxch();
    __ fyl2x();
  }
}


// Get the integer part of a heap number.  Surprisingly, all this bit twiddling
// is faster than using the built-in instructions on floating point registers.
// Trashes edi and ebx.  Dest is ecx.  Source cannot be ecx or one of the
// trashed registers.
void IntegerConvert(MacroAssembler* masm,
                    Register source,
                    TypeInfo type_info,
                    bool use_sse3,
                    Label* conversion_failure) {
  ASSERT(!source.is(ecx) && !source.is(edi) && !source.is(ebx));
  Label done, right_exponent, normal_exponent;
  Register scratch = ebx;
  Register scratch2 = edi;
  if (type_info.IsInteger32() && CpuFeatures::IsEnabled(SSE2)) {
    CpuFeatures::Scope scope(SSE2);
    __ cvttsd2si(ecx, FieldOperand(source, HeapNumber::kValueOffset));
    return;
  }
  if (!type_info.IsInteger32() || !use_sse3) {
    // Get exponent word.
    __ mov(scratch, FieldOperand(source, HeapNumber::kExponentOffset));
    // Get exponent alone in scratch2.
    __ mov(scratch2, scratch);
    __ and_(scratch2, HeapNumber::kExponentMask);
  }
  if (use_sse3) {
    CpuFeatures::Scope scope(SSE3);
    if (!type_info.IsInteger32()) {
      // Check whether the exponent is too big for a 64 bit signed integer.
      static const uint32_t kTooBigExponent =
          (HeapNumber::kExponentBias + 63) << HeapNumber::kExponentShift;
      __ cmp(Operand(scratch2), Immediate(kTooBigExponent));
      __ j(greater_equal, conversion_failure);
    }
    // Load x87 register with heap number.
    __ fld_d(FieldOperand(source, HeapNumber::kValueOffset));
    // Reserve space for 64 bit answer.
    __ sub(Operand(esp), Immediate(sizeof(uint64_t)));  // Nolint.
    // Do conversion, which cannot fail because we checked the exponent.
    __ fisttp_d(Operand(esp, 0));
    __ mov(ecx, Operand(esp, 0));  // Load low word of answer into ecx.
    __ add(Operand(esp), Immediate(sizeof(uint64_t)));  // Nolint.
  } else {
    // Load ecx with zero.  We use this either for the final shift or
    // for the answer.
    __ xor_(ecx, Operand(ecx));
    // Check whether the exponent matches a 32 bit signed int that cannot be
    // represented by a Smi.  A non-smi 32 bit integer is 1.xxx * 2^30 so the
    // exponent is 30 (biased).  This is the exponent that we are fastest at and
    // also the highest exponent we can handle here.
    const uint32_t non_smi_exponent =
        (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
    __ cmp(Operand(scratch2), Immediate(non_smi_exponent));
    // If we have a match of the int32-but-not-Smi exponent then skip some
    // logic.
    __ j(equal, &right_exponent);
    // If the exponent is higher than that then go to slow case.  This catches
    // numbers that don't fit in a signed int32, infinities and NaNs.
    __ j(less, &normal_exponent);

    {
      // Handle a big exponent.  The only reason we have this code is that the
      // >>> operator has a tendency to generate numbers with an exponent of 31.
      const uint32_t big_non_smi_exponent =
          (HeapNumber::kExponentBias + 31) << HeapNumber::kExponentShift;
      __ cmp(Operand(scratch2), Immediate(big_non_smi_exponent));
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
      __ or_(ecx, Operand(scratch2));
      // We have the answer in ecx, but we may need to negate it.
      __ test(scratch, Operand(scratch));
      __ j(positive, &done);
      __ neg(ecx);
      __ jmp(&done);
    }

    __ bind(&normal_exponent);
    // Exponent word in scratch, exponent part of exponent word in scratch2.
    // Zero in ecx.
    // We know the exponent is smaller than 30 (biased).  If it is less than
    // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, ie
    // it rounds to zero.
    const uint32_t zero_exponent =
        (HeapNumber::kExponentBias + 0) << HeapNumber::kExponentShift;
    __ sub(Operand(scratch2), Immediate(zero_exponent));
    // ecx already has a Smi zero.
    __ j(less, &done);

    // We have a shifted exponent between 0 and 30 in scratch2.
    __ shr(scratch2, HeapNumber::kExponentShift);
    __ mov(ecx, Immediate(30));
    __ sub(ecx, Operand(scratch2));

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
    __ or_(scratch2, Operand(scratch));
    // Move down according to the exponent.
    __ shr_cl(scratch2);
    // Now the unsigned answer is in scratch2.  We need to move it to ecx and
    // we may need to fix the sign.
    NearLabel negative;
    __ xor_(ecx, Operand(ecx));
    __ cmp(ecx, FieldOperand(source, HeapNumber::kExponentOffset));
    __ j(greater, &negative);
    __ mov(ecx, scratch2);
    __ jmp(&done);
    __ bind(&negative);
    __ sub(ecx, Operand(scratch2));
    __ bind(&done);
  }
}


// Input: edx, eax are the left and right objects of a bit op.
// Output: eax, ecx are left and right integers for a bit op.
void FloatingPointHelper::LoadNumbersAsIntegers(MacroAssembler* masm,
                                                TypeInfo type_info,
                                                bool use_sse3,
                                                Label* conversion_failure) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  if (!type_info.IsDouble()) {
    if (!type_info.IsSmi()) {
      __ test(edx, Immediate(kSmiTagMask));
      __ j(not_zero, &arg1_is_object);
    } else {
      if (FLAG_debug_code) __ AbortIfNotSmi(edx);
    }
    __ SmiUntag(edx);
    __ jmp(&load_arg2);
  }

  __ bind(&arg1_is_object);

  // Get the untagged integer version of the edx heap number in ecx.
  IntegerConvert(masm, edx, type_info, use_sse3, conversion_failure);
  __ mov(edx, ecx);

  // Here edx has the untagged integer, eax has a Smi or a heap number.
  __ bind(&load_arg2);
  if (!type_info.IsDouble()) {
    // Test if arg2 is a Smi.
    if (!type_info.IsSmi()) {
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &arg2_is_object);
    } else {
      if (FLAG_debug_code) __ AbortIfNotSmi(eax);
    }
    __ SmiUntag(eax);
    __ mov(ecx, eax);
    __ jmp(&done);
  }

  __ bind(&arg2_is_object);

  // Get the untagged integer version of the eax heap number in ecx.
  IntegerConvert(masm, eax, type_info, use_sse3, conversion_failure);
  __ bind(&done);
  __ mov(eax, edx);
}


// Input: edx, eax are the left and right objects of a bit op.
// Output: eax, ecx are left and right integers for a bit op.
void FloatingPointHelper::LoadUnknownsAsIntegers(MacroAssembler* masm,
                                                 bool use_sse3,
                                                 Label* conversion_failure) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  // Test if arg1 is a Smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &arg1_is_object);

  __ SmiUntag(edx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ cmp(edx, Factory::undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(edx, Immediate(0));
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(ebx, Factory::heap_number_map());
  __ j(not_equal, &check_undefined_arg1);

  // Get the untagged integer version of the edx heap number in ecx.
  IntegerConvert(masm,
                 edx,
                 TypeInfo::Unknown(),
                 use_sse3,
                 conversion_failure);
  __ mov(edx, ecx);

  // Here edx has the untagged integer, eax has a Smi or a heap number.
  __ bind(&load_arg2);

  // Test if arg2 is a Smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &arg2_is_object);

  __ SmiUntag(eax);
  __ mov(ecx, eax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ cmp(eax, Factory::undefined_value());
  __ j(not_equal, conversion_failure);
  __ mov(ecx, Immediate(0));
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(ebx, Factory::heap_number_map());
  __ j(not_equal, &check_undefined_arg2);

  // Get the untagged integer version of the eax heap number in ecx.
  IntegerConvert(masm,
                 eax,
                 TypeInfo::Unknown(),
                 use_sse3,
                 conversion_failure);
  __ bind(&done);
  __ mov(eax, edx);
}


void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         TypeInfo type_info,
                                         bool use_sse3,
                                         Label* conversion_failure) {
  if (type_info.IsNumber()) {
    LoadNumbersAsIntegers(masm, type_info, use_sse3, conversion_failure);
  } else {
    LoadUnknownsAsIntegers(masm, use_sse3, conversion_failure);
  }
}


void FloatingPointHelper::CheckLoadedIntegersWereInt32(MacroAssembler* masm,
                                                       bool use_sse3,
                                                       Label* not_int32) {
  return;
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  NearLabel load_smi, done;

  __ test(number, Immediate(kSmiTagMask));
  __ j(zero, &load_smi, not_taken);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi);
  __ SmiUntag(number);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm) {
  NearLabel load_smi_edx, load_eax, load_smi_eax, done;
  // Load operand in edx into xmm0.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_edx, not_taken);  // Argument in edx is a smi.
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));

  __ bind(&load_eax);
  // Load operand in eax into xmm1.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_eax, not_taken);  // Argument in eax is a smi.
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, Operand(edx));
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);

  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, Operand(eax));
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2Operands(MacroAssembler* masm,
                                           Label* not_numbers) {
  NearLabel load_smi_edx, load_eax, load_smi_eax, load_float_eax, done;
  // Load operand in edx into xmm0, or branch to not_numbers.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_edx, not_taken);  // Argument in edx is a smi.
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(not_equal, not_numbers);  // Argument in edx is not a number.
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
  __ bind(&load_eax);
  // Load operand in eax into xmm1, or branch to not_numbers.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_eax, not_taken);  // Argument in eax is a smi.
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset), Factory::heap_number_map());
  __ j(equal, &load_float_eax);
  __ jmp(not_numbers);  // Argument in eax is not a number.
  __ bind(&load_smi_edx);
  __ SmiUntag(edx);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm0, Operand(edx));
  __ SmiTag(edx);  // Retag smi for heap number overwriting test.
  __ jmp(&load_eax);
  __ bind(&load_smi_eax);
  __ SmiUntag(eax);  // Untag smi before converting to float.
  __ cvtsi2sd(xmm1, Operand(eax));
  __ SmiTag(eax);  // Retag smi for heap number overwriting test.
  __ jmp(&done);
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
  __ cvtsi2sd(xmm0, Operand(scratch));

  __ mov(scratch, right);
  __ SmiUntag(scratch);
  __ cvtsi2sd(xmm1, Operand(scratch));
}


void FloatingPointHelper::CheckSSE2OperandsAreInt32(MacroAssembler* masm,
                                                    Label* non_int32,
                                                    Register scratch) {
  __ cvttsd2si(scratch, Operand(xmm0));
  __ cvtsi2sd(xmm2, Operand(scratch));
  __ ucomisd(xmm0, xmm2);
  __ j(not_zero, non_int32);
  __ j(carry, non_int32);
  __ cvttsd2si(scratch, Operand(xmm1));
  __ cvtsi2sd(xmm2, Operand(scratch));
  __ ucomisd(xmm1, xmm2);
  __ j(not_zero, non_int32);
  __ j(carry, non_int32);
}


void FloatingPointHelper::LoadFloatOperands(MacroAssembler* masm,
                                            Register scratch,
                                            ArgLocation arg_location) {
  NearLabel load_smi_1, load_smi_2, done_load_1, done;
  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, edx);
  } else {
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
  }
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_1, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ bind(&done_load_1);

  if (arg_location == ARGS_IN_REGISTERS) {
    __ mov(scratch, eax);
  } else {
    __ mov(scratch, Operand(esp, 1 * kPointerSize));
  }
  __ test(scratch, Immediate(kSmiTagMask));
  __ j(zero, &load_smi_2, not_taken);
  __ fld_d(FieldOperand(scratch, HeapNumber::kValueOffset));
  __ jmp(&done);

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
  NearLabel test_other, done;
  // Test if both operands are floats or smi -> scratch=k_is_float;
  // Otherwise scratch = k_not_float.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &test_other, not_taken);  // argument in edx is OK
  __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
  __ cmp(scratch, Factory::heap_number_map());
  __ j(not_equal, non_float);  // argument in edx is not a number -> NaN

  __ bind(&test_other);
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &done);  // argument in eax is OK
  __ mov(scratch, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(scratch, Factory::heap_number_map());
  __ j(not_equal, non_float);  // argument in eax is not a number -> NaN

  // Fall-through: Both operands are numbers.
  __ bind(&done);
}


void FloatingPointHelper::CheckFloatOperandsAreInt32(MacroAssembler* masm,
                                                     Label* non_int32) {
  return;
}


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done, undo;

  if (op_ == Token::SUB) {
    if (include_smi_code_) {
      // Check whether the value is a smi.
      NearLabel try_float;
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &try_float, not_taken);

      if (negative_zero_ == kStrictNegativeZero) {
        // Go slow case if the value of the expression is zero
        // to make sure that we switch between 0 and -0.
        __ test(eax, Operand(eax));
        __ j(zero, &slow, not_taken);
      }

      // The value of the expression is a smi that is not zero.  Try
      // optimistic subtraction '0 - value'.
      __ mov(edx, Operand(eax));
      __ Set(eax, Immediate(0));
      __ sub(eax, Operand(edx));
      __ j(overflow, &undo, not_taken);
      __ StubReturn(1);

      // Try floating point case.
      __ bind(&try_float);
    } else if (FLAG_debug_code) {
      __ AbortIfSmi(eax);
    }

    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ cmp(edx, Factory::heap_number_map());
    __ j(not_equal, &slow);
    if (overwrite_ == UNARY_OVERWRITE) {
      __ mov(edx, FieldOperand(eax, HeapNumber::kExponentOffset));
      __ xor_(edx, HeapNumber::kSignMask);  // Flip sign.
      __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), edx);
    } else {
      __ mov(edx, Operand(eax));
      // edx: operand
      __ AllocateHeapNumber(eax, ebx, ecx, &undo);
      // eax: allocated 'empty' number
      __ mov(ecx, FieldOperand(edx, HeapNumber::kExponentOffset));
      __ xor_(ecx, HeapNumber::kSignMask);  // Flip sign.
      __ mov(FieldOperand(eax, HeapNumber::kExponentOffset), ecx);
      __ mov(ecx, FieldOperand(edx, HeapNumber::kMantissaOffset));
      __ mov(FieldOperand(eax, HeapNumber::kMantissaOffset), ecx);
    }
  } else if (op_ == Token::BIT_NOT) {
    if (include_smi_code_) {
      Label non_smi;
      __ test(eax, Immediate(kSmiTagMask));
      __ j(not_zero, &non_smi);
      __ not_(eax);
      __ and_(eax, ~kSmiTagMask);  // Remove inverted smi-tag.
      __ ret(0);
      __ bind(&non_smi);
    } else if (FLAG_debug_code) {
      __ AbortIfSmi(eax);
    }

    // Check if the operand is a heap number.
    __ mov(edx, FieldOperand(eax, HeapObject::kMapOffset));
    __ cmp(edx, Factory::heap_number_map());
    __ j(not_equal, &slow, not_taken);

    // Convert the heap number in eax to an untagged integer in ecx.
    IntegerConvert(masm,
                   eax,
                   TypeInfo::Unknown(),
                   CpuFeatures::IsSupported(SSE3),
                   &slow);

    // Do the bitwise operation and check if the result fits in a smi.
    NearLabel try_float;
    __ not_(ecx);
    __ cmp(ecx, 0xc0000000);
    __ j(sign, &try_float, not_taken);

    // Tag the result as a smi and we're done.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ lea(eax, Operand(ecx, times_2, kSmiTag));
    __ jmp(&done);

    // Try to store the result in a heap number.
    __ bind(&try_float);
    if (overwrite_ == UNARY_NO_OVERWRITE) {
      // Allocate a fresh heap number, but don't overwrite eax until
      // we're sure we can do it without going through the slow case
      // that needs the value in eax.
      __ AllocateHeapNumber(ebx, edx, edi, &slow);
      __ mov(eax, Operand(ebx));
    }
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatures::Scope use_sse2(SSE2);
      __ cvtsi2sd(xmm0, Operand(ecx));
      __ movdbl(FieldOperand(eax, HeapNumber::kValueOffset), xmm0);
    } else {
      __ push(ecx);
      __ fild_s(Operand(esp, 0));
      __ pop(ecx);
      __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    }
  } else {
    UNIMPLEMENTED();
  }

  // Return from the stub.
  __ bind(&done);
  __ StubReturn(1);

  // Restore eax and go slow case.
  __ bind(&undo);
  __ mov(eax, Operand(edx));

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
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


void MathPowStub::Generate(MacroAssembler* masm) {
  // Registers are used as follows:
  // edx = base
  // eax = exponent
  // ecx = temporary, result

  CpuFeatures::Scope use_sse2(SSE2);
  Label allocate_return, call_runtime;

  // Load input parameters.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(eax, Operand(esp, 1 * kPointerSize));

  // Save 1 in xmm3 - we need this several times later on.
  __ mov(ecx, Immediate(1));
  __ cvtsi2sd(xmm3, Operand(ecx));

  Label exponent_nonsmi;
  Label base_nonsmi;
  // If the exponent is a heap number go to that specific case.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &exponent_nonsmi);
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &base_nonsmi);

  // Optimized version when both exponent and base are smis.
  Label powi;
  __ SmiUntag(edx);
  __ cvtsi2sd(xmm0, Operand(edx));
  __ jmp(&powi);
  // exponent is smi and base is a heapnumber.
  __ bind(&base_nonsmi);
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Factory::heap_number_map());
  __ j(not_equal, &call_runtime);

  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));

  // Optimized version of pow if exponent is a smi.
  // xmm0 contains the base.
  __ bind(&powi);
  __ SmiUntag(eax);

  // Save exponent in base as we need to check if exponent is negative later.
  // We know that base and exponent are in different registers.
  __ mov(edx, eax);

  // Get absolute value of exponent.
  NearLabel no_neg;
  __ cmp(eax, 0);
  __ j(greater_equal, &no_neg);
  __ neg(eax);
  __ bind(&no_neg);

  // Load xmm1 with 1.
  __ movsd(xmm1, xmm3);
  NearLabel while_true;
  NearLabel no_multiply;

  __ bind(&while_true);
  __ shr(eax, 1);
  __ j(not_carry, &no_multiply);
  __ mulsd(xmm1, xmm0);
  __ bind(&no_multiply);
  __ mulsd(xmm0, xmm0);
  __ j(not_zero, &while_true);

  // base has the original value of the exponent - if the exponent  is
  // negative return 1/result.
  __ test(edx, Operand(edx));
  __ j(positive, &allocate_return);
  // Special case if xmm1 has reached infinity.
  __ mov(ecx, Immediate(0x7FB00000));
  __ movd(xmm0, Operand(ecx));
  __ cvtss2sd(xmm0, xmm0);
  __ ucomisd(xmm0, xmm1);
  __ j(equal, &call_runtime);
  __ divsd(xmm3, xmm1);
  __ movsd(xmm1, xmm3);
  __ jmp(&allocate_return);

  // exponent (or both) is a heapnumber - no matter what we should now work
  // on doubles.
  __ bind(&exponent_nonsmi);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         Factory::heap_number_map());
  __ j(not_equal, &call_runtime);
  __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));
  // Test if exponent is nan.
  __ ucomisd(xmm1, xmm1);
  __ j(parity_even, &call_runtime);

  NearLabel base_not_smi;
  NearLabel handle_special_cases;
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &base_not_smi);
  __ SmiUntag(edx);
  __ cvtsi2sd(xmm0, Operand(edx));
  __ jmp(&handle_special_cases);

  __ bind(&base_not_smi);
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         Factory::heap_number_map());
  __ j(not_equal, &call_runtime);
  __ mov(ecx, FieldOperand(edx, HeapNumber::kExponentOffset));
  __ and_(ecx, HeapNumber::kExponentMask);
  __ cmp(Operand(ecx), Immediate(HeapNumber::kExponentMask));
  // base is NaN or +/-Infinity
  __ j(greater_equal, &call_runtime);
  __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));

  // base is in xmm0 and exponent is in xmm1.
  __ bind(&handle_special_cases);
  NearLabel not_minus_half;
  // Test for -0.5.
  // Load xmm2 with -0.5.
  __ mov(ecx, Immediate(0xBF000000));
  __ movd(xmm2, Operand(ecx));
  __ cvtss2sd(xmm2, xmm2);
  // xmm2 now has -0.5.
  __ ucomisd(xmm2, xmm1);
  __ j(not_equal, &not_minus_half);

  // Calculates reciprocal of square root.
  // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
  __ xorpd(xmm1, xmm1);
  __ addsd(xmm1, xmm0);
  __ sqrtsd(xmm1, xmm1);
  __ divsd(xmm3, xmm1);
  __ movsd(xmm1, xmm3);
  __ jmp(&allocate_return);

  // Test for 0.5.
  __ bind(&not_minus_half);
  // Load xmm2 with 0.5.
  // Since xmm3 is 1 and xmm2 is -0.5 this is simply xmm2 + xmm3.
  __ addsd(xmm2, xmm3);
  // xmm2 now has 0.5.
  __ ucomisd(xmm2, xmm1);
  __ j(not_equal, &call_runtime);
  // Calculates square root.
  // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
  __ xorpd(xmm1, xmm1);
  __ addsd(xmm1, xmm0);
  __ sqrtsd(xmm1, xmm1);

  __ bind(&allocate_return);
  __ AllocateHeapNumber(ecx, eax, edx, &call_runtime);
  __ movdbl(FieldOperand(ecx, HeapNumber::kValueOffset), xmm1);
  __ mov(eax, ecx);
  __ ret(2 * kPointerSize);

  __ bind(&call_runtime);
  __ TailCallRuntime(Runtime::kMath_pow_cfunction, 2, 1);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in edx and the parameter count is in eax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);

  // Check if the calling frame is an arguments adaptor frame.
  NearLabel adaptor;
  __ mov(ebx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(ebx, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(ecx), Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register eax. Use unsigned comparison to get negative
  // check for free.
  __ cmp(edx, Operand(eax));
  __ j(above_equal, &slow, not_taken);

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
  __ cmp(edx, Operand(ecx));
  __ j(above_equal, &slow, not_taken);

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


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[4] : number of parameters
  // esp[8] : receiver displacement
  // esp[16] : function

  // The displacement is used for skipping the return address and the
  // frame pointer on the stack. It is the offset of the last
  // parameter (if any) relative to the frame pointer.
  static const int kDisplacement = 2 * kPointerSize;

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(ecx), Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ mov(Operand(esp, 1 * kPointerSize), ecx);
  __ lea(edx, Operand(edx, ecx, times_2, kDisplacement));
  __ mov(Operand(esp, 2 * kPointerSize), edx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  NearLabel add_arguments_object;
  __ bind(&try_allocate);
  __ test(ecx, Operand(ecx));
  __ j(zero, &add_arguments_object);
  __ lea(ecx, Operand(ecx, times_2, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ add(Operand(ecx), Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(ecx, eax, edx, ebx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ mov(edi, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(edi, FieldOperand(edi, GlobalObject::kGlobalContextOffset));
  __ mov(edi, Operand(edi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ mov(ebx, FieldOperand(edi, i));
    __ mov(FieldOperand(eax, i), ebx);
  }

  // Setup the callee in-object property.
  STATIC_ASSERT(Heap::arguments_callee_index == 0);
  __ mov(ebx, Operand(esp, 3 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize), ebx);

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::arguments_length_index == 1);
  __ mov(ecx, Operand(esp, 1 * kPointerSize));
  __ mov(FieldOperand(eax, JSObject::kHeaderSize + kPointerSize), ecx);

  // If there are no actual arguments, we're done.
  Label done;
  __ test(ecx, Operand(ecx));
  __ j(zero, &done);

  // Get the parameters pointer from the stack.
  __ mov(edx, Operand(esp, 2 * kPointerSize));

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(edi, Operand(eax, Heap::kArgumentsObjectSize));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(Factory::fixed_array_map()));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);
  // Untag the length for the loop below.
  __ SmiUntag(ecx);

  // Copy the fixed array slots.
  NearLabel loop;
  __ bind(&loop);
  __ mov(ebx, Operand(edx, -1 * kPointerSize));  // Skip receiver.
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize), ebx);
  __ add(Operand(edi), Immediate(kPointerSize));
  __ sub(Operand(edx), Immediate(kPointerSize));
  __ dec(ecx);
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
  //  esp[4]: last_match_info (expected JSArray)
  //  esp[8]: previous index
  //  esp[12]: subject string
  //  esp[16]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime, invoke_regexp;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ mov(ebx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ test(ebx, Operand(ebx));
  __ j(zero, &runtime, not_taken);

  // Check that the first argument is a JSRegExp object.
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
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
  __ cmp(Operand(ebx), Immediate(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ j(not_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2. This
  // uses the asumption that smis are 2 * their untagged value.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(Operand(edx), Immediate(2));  // edx was a smi.
  // Check that the static offsets vector buffer is large enough.
  __ cmp(edx, OffsetsVector::kStaticOffsetsVectorSize);
  __ j(above, &runtime);

  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the second argument is a string.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);
  // Get the length of the string to ebx.
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));

  // ebx: Length of subject string as a smi
  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (unsigned comparison).
  __ mov(eax, Operand(esp, kPreviousIndexOffset));
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &runtime);
  __ cmp(eax, Operand(ebx));
  __ j(above_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // edx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  __ CmpObjectType(eax, JS_ARRAY_TYPE, ebx);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));
  __ mov(eax, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(eax, Factory::fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ mov(eax, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ SmiUntag(eax);
  __ add(Operand(edx), Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmp(edx, Operand(eax));
  __ j(greater, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_ascii_string, seq_two_byte_string, check_code;
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  // First check for flat two byte string.
  __ and_(ebx,
          kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask);
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);
  // Any other flat string must be a flat ascii string.
  __ test(Operand(ebx),
          Immediate(kIsNotStringMask | kStringRepresentationMask));
  __ j(zero, &seq_ascii_string);

  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT((kConsStringTag & kExternalStringTag) == 0);
  __ test(Operand(ebx),
          Immediate(kIsNotStringMask | kExternalStringTag));
  __ j(not_zero, &runtime);
  // String is a cons string.
  __ mov(edx, FieldOperand(eax, ConsString::kSecondOffset));
  __ cmp(Operand(edx), Factory::empty_string());
  __ j(not_equal, &runtime);
  __ mov(eax, FieldOperand(eax, ConsString::kFirstOffset));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  // String is a cons string with empty second part.
  // eax: first part of cons string.
  // ebx: map of first part of cons string.
  // Is first part a flat two byte string?
  __ test_b(FieldOperand(ebx, Map::kInstanceTypeOffset),
            kStringRepresentationMask | kStringEncodingMask);
  STATIC_ASSERT((kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);
  // Any other flat string must be ascii.
  __ test_b(FieldOperand(ebx, Map::kInstanceTypeOffset),
            kStringRepresentationMask);
  __ j(not_zero, &runtime);

  __ bind(&seq_ascii_string);
  // eax: subject string (flat ascii)
  // ecx: RegExp data (FixedArray)
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataAsciiCodeOffset));
  __ Set(edi, Immediate(1));  // Type is ascii.
  __ jmp(&check_code);

  __ bind(&seq_two_byte_string);
  // eax: subject string (flat two byte)
  // ecx: RegExp data (FixedArray)
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataUC16CodeOffset));
  __ Set(edi, Immediate(0));  // Type is two byte.

  __ bind(&check_code);
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CmpObjectType(edx, CODE_TYPE, ebx);
  __ j(not_equal, &runtime);

  // eax: subject string
  // edx: code
  // edi: encoding of subject string (1 if ascii, 0 if two_byte);
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ SmiUntag(ebx);  // Previous index from smi.

  // eax: subject string
  // ebx: previous index
  // edx: code
  // edi: encoding of subject string (1 if ascii 0 if two_byte);
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1);

  static const int kRegExpExecuteArguments = 7;
  __ EnterApiExitFrame(kRegExpExecuteArguments);

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ mov(Operand(esp, 6 * kPointerSize), Immediate(1));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ mov(ecx, Operand::StaticVariable(address_of_regexp_stack_memory_address));
  __ add(ecx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ mov(Operand(esp, 5 * kPointerSize), ecx);

  // Argument 5: static offsets vector buffer.
  __ mov(Operand(esp, 4 * kPointerSize),
         Immediate(ExternalReference::address_of_static_offsets_vector()));

  // Argument 4: End of string data
  // Argument 3: Start of string data
  NearLabel setup_two_byte, setup_rest;
  __ test(edi, Operand(edi));
  __ mov(edi, FieldOperand(eax, String::kLengthOffset));
  __ j(zero, &setup_two_byte);
  __ SmiUntag(edi);
  __ lea(ecx, FieldOperand(eax, edi, times_1, SeqAsciiString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_1, SeqAsciiString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.
  __ jmp(&setup_rest);

  __ bind(&setup_two_byte);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);  // edi is smi (powered by 2).
  __ lea(ecx, FieldOperand(eax, edi, times_1, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_2, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.

  __ bind(&setup_rest);

  // Argument 2: Previous index.
  __ mov(Operand(esp, 1 * kPointerSize), ebx);

  // Argument 1: Subject string.
  __ mov(Operand(esp, 0 * kPointerSize), eax);

  // Locate the code entry and call it.
  __ add(Operand(edx), Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(Operand(edx));

  // Drop arguments and come back to JS mode.
  __ LeaveApiExitFrame();

  // Check the result.
  Label success;
  __ cmp(eax, NativeRegExpMacroAssembler::SUCCESS);
  __ j(equal, &success, taken);
  Label failure;
  __ cmp(eax, NativeRegExpMacroAssembler::FAILURE);
  __ j(equal, &failure, taken);
  __ cmp(eax, NativeRegExpMacroAssembler::EXCEPTION);
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(eax, Operand::StaticVariable(pending_exception));
  __ cmp(edx, Operand(eax));
  __ j(equal, &runtime);
  // For exception, throw the exception again.

  // Clear the pending exception variable.
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(eax, Factory::termination_exception());
  Label throw_termination_exception;
  __ j(equal, &throw_termination_exception);

  // Handle normal exception by following handler chain.
  __ Throw(eax);

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(TERMINATION, eax);

  __ bind(&failure);
  // For failure to match, return null.
  __ mov(Operand(eax), Factory::null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(Operand(edx), Immediate(2));  // edx was a smi.

  // edx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  __ mov(eax, Operand(esp, kLastMatchInfoOffset));
  __ mov(ebx, FieldOperand(eax, JSArray::kElementsOffset));

  // ebx: last_match_info backing store (FixedArray)
  // edx: number of capture registers
  // Store the capture count.
  __ SmiTag(edx);  // Number of capture registers to smi.
  __ mov(FieldOperand(ebx, RegExpImpl::kLastCaptureCountOffset), edx);
  __ SmiUntag(edx);  // Number of capture registers back from smi.
  // Store last subject and last input.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(FieldOperand(ebx, RegExpImpl::kLastSubjectOffset), eax);
  __ mov(ecx, ebx);
  __ RecordWrite(ecx, RegExpImpl::kLastSubjectOffset, eax, edi);
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(FieldOperand(ebx, RegExpImpl::kLastInputOffset), eax);
  __ mov(ecx, ebx);
  __ RecordWrite(ecx, RegExpImpl::kLastInputOffset, eax, edi);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector();
  __ mov(ecx, Immediate(address_of_static_offsets_vector));

  // ebx: last_match_info backing store (FixedArray)
  // ecx: offsets vector
  // edx: number of capture registers
  NearLabel next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ sub(Operand(edx), Immediate(1));
  __ j(negative, &done);
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
#endif  // V8_INTERPRETED_REGEXP
}


void RegExpConstructResultStub::Generate(MacroAssembler* masm) {
  const int kMaxInlineLength = 100;
  Label slowcase;
  NearLabel done;
  __ mov(ebx, Operand(esp, kPointerSize * 3));
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(not_zero, &slowcase);
  __ cmp(Operand(ebx), Immediate(Smi::FromInt(kMaxInlineLength)));
  __ j(above, &slowcase);
  // Smi-tagging is equivalent to multiplying by 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // Allocate RegExpResult followed by FixedArray with size in ebx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  __ AllocateInNewSpace(JSRegExpResult::kSize + FixedArray::kHeaderSize,
                        times_half_pointer_size,
                        ebx,  // In: Number of elements (times 2, being a smi)
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
  __ mov(edx, ContextOperand(esi, Context::GLOBAL_INDEX));
  __ mov(ecx, Immediate(Factory::empty_fixed_array()));
  __ lea(ebx, Operand(eax, JSRegExpResult::kSize));
  __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalContextOffset));
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
         Immediate(Factory::fixed_array_map()));
  // Set length.
  __ mov(FieldOperand(ebx, FixedArray::kLengthOffset), ecx);
  // Fill contents of fixed-array with the-hole.
  __ SmiUntag(ecx);
  __ mov(edx, Immediate(Factory::the_hole_value()));
  __ lea(ebx, FieldOperand(ebx, FixedArray::kHeaderSize));
  // Fill fixed array elements with hole.
  // eax: JSArray.
  // ecx: Number of elements to fill.
  // ebx: Start of elements in FixedArray.
  // edx: the hole.
  Label loop;
  __ test(ecx, Operand(ecx));
  __ bind(&loop);
  __ j(less_equal, &done);  // Jump if ecx is negative or zero.
  __ sub(Operand(ecx), Immediate(1));
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
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(scratch, Immediate(Heap::kNumberStringCacheRootIndex));
  __ mov(number_string_cache,
         Operand::StaticArray(scratch, times_pointer_size, roots_address));
  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ mov(mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  __ shr(mask, kSmiTagSize + 1);  // Untag length and divide it by two.
  __ sub(Operand(mask), Immediate(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  NearLabel smi_hash_calculated;
  NearLabel load_result_from_cache;
  if (object_is_smi) {
    __ mov(scratch, object);
    __ SmiUntag(scratch);
  } else {
    NearLabel not_smi, hash_calculated;
    STATIC_ASSERT(kSmiTag == 0);
    __ test(object, Immediate(kSmiTagMask));
    __ j(not_zero, &not_smi);
    __ mov(scratch, object);
    __ SmiUntag(scratch);
    __ jmp(&smi_hash_calculated);
    __ bind(&not_smi);
    __ cmp(FieldOperand(object, HeapObject::kMapOffset),
           Factory::heap_number_map());
    __ j(not_equal, not_found);
    STATIC_ASSERT(8 == kDoubleSize);
    __ mov(scratch, FieldOperand(object, HeapNumber::kValueOffset));
    __ xor_(scratch, FieldOperand(object, HeapNumber::kValueOffset + 4));
    // Object is heap number and hash is now in scratch. Calculate cache index.
    __ and_(scratch, Operand(mask));
    Register index = scratch;
    Register probe = mask;
    __ mov(probe,
           FieldOperand(number_string_cache,
                        index,
                        times_twice_pointer_size,
                        FixedArray::kHeaderSize));
    __ test(probe, Immediate(kSmiTagMask));
    __ j(zero, not_found);
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatures::Scope fscope(SSE2);
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
    __ jmp(&load_result_from_cache);
  }

  __ bind(&smi_hash_calculated);
  // Object is smi and hash is now in scratch. Calculate cache index.
  __ and_(scratch, Operand(mask));
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
  __ IncrementCounter(&Counters::number_to_string_native, 1);
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

void CompareStub::Generate(MacroAssembler* masm) {
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));

  Label check_unequal_objects, done;

  // Compare two smis if required.
  if (include_smi_compare_) {
    Label non_smi, smi_done;
    __ mov(ecx, Operand(edx));
    __ or_(ecx, Operand(eax));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &non_smi, not_taken);
    __ sub(edx, Operand(eax));  // Return on the result of the subtraction.
    __ j(no_overflow, &smi_done);
    __ not_(edx);  // Correct sign in case of overflow. edx is never 0 here.
    __ bind(&smi_done);
    __ mov(eax, edx);
    __ ret(0);
    __ bind(&non_smi);
  } else if (FLAG_debug_code) {
    __ mov(ecx, Operand(edx));
    __ or_(ecx, Operand(eax));
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, "Unexpected smi operands.");
  }

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Identical objects can be compared fast, but there are some tricky cases
  // for NaN and undefined.
  {
    Label not_identical;
    __ cmp(eax, Operand(edx));
    __ j(not_equal, &not_identical);

    if (cc_ != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      NearLabel check_for_nan;
      __ cmp(edx, Factory::undefined_value());
      __ j(not_equal, &check_for_nan);
      __ Set(eax, Immediate(Smi::FromInt(NegativeComparisonResult(cc_))));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
    // Note: if cc_ != equal, never_nan_nan_ is not used.
    if (never_nan_nan_ && (cc_ == equal)) {
      __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
      __ ret(0);
    } else {
      NearLabel heap_number;
      __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
             Immediate(Factory::heap_number_map()));
      __ j(equal, &heap_number);
      if (cc_ != equal) {
        // Call runtime on identical JSObjects.  Otherwise return equal.
        __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ecx);
        __ j(above_equal, &not_identical);
      }
      __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
      __ ret(0);

      __ bind(&heap_number);
      // It is a heap number, so return non-equal if it's NaN and equal if
      // it's not NaN.
      // The representation of NaN values has all exponent bits (52..62) set,
      // and not all mantissa bits (0..51) clear.
      // We only accept QNaNs, which have bit 51 set.
      // Read top bits of double representation (second word of value).

      // Value is a QNaN if value & kQuietNaNMask == kQuietNaNMask, i.e.,
      // all bits in the mask are set. We only need to check the word
      // that contains the exponent and high bit of the mantissa.
      STATIC_ASSERT(((kQuietNaNHighBitsMask << 1) & 0x80000000u) != 0);
      __ mov(edx, FieldOperand(edx, HeapNumber::kExponentOffset));
      __ Set(eax, Immediate(0));
      // Shift value and mask so kQuietNaNHighBitsMask applies to topmost
      // bits.
      __ add(edx, Operand(edx));
      __ cmp(edx, kQuietNaNHighBitsMask << 1);
      if (cc_ == equal) {
        STATIC_ASSERT(EQUAL != 1);
        __ setcc(above_equal, eax);
        __ ret(0);
      } else {
        NearLabel nan;
        __ j(above_equal, &nan);
        __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
        __ ret(0);
        __ bind(&nan);
        __ Set(eax, Immediate(Smi::FromInt(NegativeComparisonResult(cc_))));
        __ ret(0);
      }
    }

    __ bind(&not_identical);
  }

  // Strict equality can quickly decide whether objects are equal.
  // Non-strict object equality is slower, so it is handled later in the stub.
  if (cc_ == equal && strict_) {
    Label slow;  // Fallthrough label.
    NearLabel not_smis;
    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    // If either is a Smi (we know that not both are), then they can only
    // be equal if the other is a HeapNumber. If so, use the slow case.
    STATIC_ASSERT(kSmiTag == 0);
    ASSERT_EQ(0, Smi::FromInt(0));
    __ mov(ecx, Immediate(kSmiTagMask));
    __ and_(ecx, Operand(eax));
    __ test(ecx, Operand(edx));
    __ j(not_zero, &not_smis);
    // One operand is a smi.

    // Check whether the non-smi is a heap number.
    STATIC_ASSERT(kSmiTagMask == 1);
    // ecx still holds eax & kSmiTag, which is either zero or one.
    __ sub(Operand(ecx), Immediate(0x01));
    __ mov(ebx, edx);
    __ xor_(ebx, Operand(eax));
    __ and_(ebx, Operand(ecx));  // ebx holds either 0 or eax ^ edx.
    __ xor_(ebx, Operand(eax));
    // if eax was smi, ebx is now edx, else eax.

    // Check if the non-smi operand is a heap number.
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(Factory::heap_number_map()));
    // If heap number, handle it in the slow case.
    __ j(equal, &slow);
    // Return non-equal (ebx is not zero)
    __ mov(eax, ebx);
    __ ret(0);

    __ bind(&not_smis);
    // If either operand is a JSObject or an oddball value, then they are not
    // equal since their pointers are different
    // There is no test for undetectability in strict equality.

    // Get the type of the first operand.
    // If the first object is a JS object, we have done pointer comparison.
    NearLabel first_non_object;
    STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
    __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ecx);
    __ j(below, &first_non_object);

    // Return non-zero (eax is not zero)
    NearLabel return_not_equal;
    STATIC_ASSERT(kHeapObjectTag != 0);
    __ bind(&return_not_equal);
    __ ret(0);

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    __ CmpObjectType(edx, FIRST_JS_OBJECT_TYPE, ecx);
    __ j(above_equal, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    // Fall through to the general case.
    __ bind(&slow);
  }

  // Generate the number comparison code.
  if (include_number_compare_) {
    Label non_number_comparison;
    Label unordered;
    if (CpuFeatures::IsSupported(SSE2)) {
      CpuFeatures::Scope use_sse2(SSE2);
      CpuFeatures::Scope use_cmov(CMOV);

      FloatingPointHelper::LoadSSE2Operands(masm, &non_number_comparison);
      __ ucomisd(xmm0, xmm1);

      // Don't base result on EFLAGS when a NaN is involved.
      __ j(parity_even, &unordered, not_taken);
      // Return a result of -1, 0, or 1, based on EFLAGS.
      __ mov(eax, 0);  // equal
      __ mov(ecx, Immediate(Smi::FromInt(1)));
      __ cmov(above, eax, Operand(ecx));
      __ mov(ecx, Immediate(Smi::FromInt(-1)));
      __ cmov(below, eax, Operand(ecx));
      __ ret(0);
    } else {
      FloatingPointHelper::CheckFloatOperands(
          masm, &non_number_comparison, ebx);
      FloatingPointHelper::LoadFloatOperand(masm, eax);
      FloatingPointHelper::LoadFloatOperand(masm, edx);
      __ FCmp();

      // Don't base result on EFLAGS when a NaN is involved.
      __ j(parity_even, &unordered, not_taken);

      NearLabel below_label, above_label;
      // Return a result of -1, 0, or 1, based on EFLAGS.
      __ j(below, &below_label, not_taken);
      __ j(above, &above_label, not_taken);

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
    ASSERT(cc_ != not_equal);
    if (cc_ == less || cc_ == less_equal) {
      __ mov(eax, Immediate(Smi::FromInt(1)));
    } else {
      __ mov(eax, Immediate(Smi::FromInt(-1)));
    }
    __ ret(0);

    // The number comparison code did not provide a valid result.
    __ bind(&non_number_comparison);
  }

  // Fast negative check for symbol-to-symbol equality.
  Label check_for_strings;
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &check_for_strings, eax, ecx);
    BranchIfNonSymbol(masm, &check_for_strings, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx,
                                         &check_unequal_objects);

  // Inline comparison of ascii strings.
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     edx,
                                                     eax,
                                                     ecx,
                                                     ebx,
                                                     edi);
#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&check_unequal_objects);
  if (cc_ == equal && !strict_) {
    // Non-strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    NearLabel not_both_objects;
    NearLabel return_unequal;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(ecx, Operand(eax, edx, times_1, 0));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &not_both_objects);
    __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, ecx);
    __ j(below, &not_both_objects);
    __ CmpObjectType(edx, FIRST_JS_OBJECT_TYPE, ebx);
    __ j(below, &not_both_objects);
    // We do not bail out after this point.  Both are JSObjects, and
    // they are equal if and only if both are undetectable.
    // The and of the undetectable flags is 1 if and only if they are equal.
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(zero, &return_unequal);
    __ test_b(FieldOperand(ebx, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(zero, &return_unequal);
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
  if (cc_ == equal) {
    builtin = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    __ push(Immediate(Smi::FromInt(NegativeComparisonResult(cc_))));
  }

  // Restore return address on the stack.
  __ push(ecx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);
}


void CompareStub::BranchIfNonSymbol(MacroAssembler* masm,
                                    Label* label,
                                    Register object,
                                    Register scratch) {
  __ test(object, Immediate(kSmiTagMask));
  __ j(zero, label);
  __ mov(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  __ and_(scratch, kIsSymbolMask | kIsNotStringMask);
  __ cmp(scratch, kSymbolTag | kStringTag);
  __ j(not_equal, label);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kStackGuard, 0, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // +1 ~ return address
    Label receiver_is_value, receiver_is_js_object;
    __ mov(eax, Operand(esp, (argc_ + 1) * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &receiver_is_value, not_taken);

    // Check if the receiver is a valid JS object.
    __ CmpObjectType(eax, FIRST_JS_OBJECT_TYPE, edi);
    __ j(above_equal, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(eax);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ LeaveInternalFrame();
    __ mov(Operand(esp, (argc_ + 1) * kPointerSize), eax);

    __ bind(&receiver_is_js_object);
  }

  // Get the function to call from the stack.
  // +2 ~ receiver, return address
  __ mov(edi, Operand(esp, (argc_ + 2) * kPointerSize));

  // Check that the function really is a JavaScript function.
  __ test(edi, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &slow, not_taken);

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ mov(Operand(esp, (argc_ + 1) * kPointerSize), edi);
  __ Set(eax, Immediate(argc_));
  __ Set(ebx, Immediate(0));
  __ GetBuiltinEntry(edx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline));
  __ jmp(adaptor, RelocInfo::CODE_TARGET);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  __ Throw(eax);
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
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate_scope) {
    __ inc(Operand::StaticVariable(scope_depth));
  }

  // Call C function.
  __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
  __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
  __ call(Operand(ebx));
  // Result is in eax or edx:eax - do not destroy these registers!

  if (always_allocate_scope) {
    __ dec(Operand::StaticVariable(scope_depth));
  }

  // Make sure we're not trying to return 'the hole' from the runtime
  // call as this may lead to crashes in the IC code later.
  if (FLAG_debug_code) {
    NearLabel okay;
    __ cmp(eax, Factory::the_hole_value());
    __ j(not_equal, &okay);
    __ int3();
    __ bind(&okay);
  }

  // Check for failure result.
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ lea(ecx, Operand(eax, 1));
  // Lower 2 bits of ecx are 0 iff eax has failure tag.
  __ test(ecx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned, not_taken);

  ExternalReference pending_exception_address(Top::k_pending_exception_address);

  // Check that there is no pending exception, otherwise we
  // should have returned some failure value.
  if (FLAG_debug_code) {
    __ push(edx);
    __ mov(edx, Operand::StaticVariable(
           ExternalReference::the_hole_value_location()));
    NearLabel okay;
    __ cmp(edx, Operand::StaticVariable(pending_exception_address));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ test(eax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, taken);

  // Special handling of out of memory exceptions.
  __ cmp(eax, reinterpret_cast<int32_t>(Failure::OutOfMemoryException()));
  __ j(equal, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  __ mov(eax, Operand::StaticVariable(pending_exception_address));
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(Operand::StaticVariable(pending_exception_address), edx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(eax, Factory::termination_exception());
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  __ ThrowUncatchable(type, eax);
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
  __ EnterExitFrame(save_doubles_);

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
  __ push(ebp);
  __ mov(ebp, Operand(esp));

  // Push marker in two places.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ push(Immediate(Smi::FromInt(marker)));  // context slot
  __ push(Immediate(Smi::FromInt(marker)));  // function slot
  // Save callee-saved registers (C calling conventions).
  __ push(edi);
  __ push(esi);
  __ push(ebx);

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Top::k_c_entry_fp_address);
  __ push(Operand::StaticVariable(c_entry_fp));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Top::k_js_entry_sp_address);
  __ cmp(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ j(not_equal, &not_outermost_js);
  __ mov(Operand::StaticVariable(js_entry_sp), ebp);
  __ bind(&not_outermost_js);
#endif

  // Call a faked try-block that does the invoke.
  __ call(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Top::k_pending_exception_address);
  __ mov(Operand::StaticVariable(pending_exception), eax);
  __ mov(eax, reinterpret_cast<int32_t>(Failure::Exception()));
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);

  // Clear any pending exceptions.
  __ mov(edx,
         Operand::StaticVariable(ExternalReference::the_hole_value_location()));
  __ mov(Operand::StaticVariable(pending_exception), edx);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline
  // builtin and pop the faked function when we return. Notice that we
  // cannot store a reference to the trampoline code directly in this
  // stub, because the builtin stubs may not have been generated yet.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ mov(edx, Immediate(construct_entry));
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ mov(edx, Immediate(entry));
  }
  __ mov(edx, Operand(edx, 0));  // deref address
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ call(Operand(edx));

  // Unlink this frame from the handler chain.
  __ pop(Operand::StaticVariable(ExternalReference(Top::k_handler_address)));
  // Pop next_sp.
  __ add(Operand(esp), Immediate(StackHandlerConstants::kSize - kPointerSize));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If current EBP value is the same as js_entry_sp value, it means that
  // the current function is the outermost.
  __ cmp(ebp, Operand::StaticVariable(js_entry_sp));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ bind(&not_outermost_js_2);
#endif

  // Restore the top frame descriptor from the stack.
  __ bind(&exit);
  __ pop(Operand::StaticVariable(ExternalReference(Top::k_c_entry_fp_address)));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(Operand(esp), Immediate(2 * kPointerSize));  // remove markers

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
  static const int8_t kCmpEdiImmediateByte1 = BitCast<int8_t, uint8_t>(0x81);
  static const int8_t kCmpEdiImmediateByte2 = BitCast<int8_t, uint8_t>(0xff);
  static const int8_t kMovEaxImmediateByte = BitCast<int8_t, uint8_t>(0xb8);

  ExternalReference roots_address = ExternalReference::roots_address();

  ASSERT_EQ(object.code(), InstanceofStub::left().code());
  ASSERT_EQ(function.code(), InstanceofStub::right().code());

  // Get the object and function - they are always both needed.
  Label slow, not_js_object;
  if (!HasArgsInRegisters()) {
    __ mov(object, Operand(esp, 2 * kPointerSize));
    __ mov(function, Operand(esp, 1 * kPointerSize));
  }

  // Check that the left hand is a JS object.
  __ test(object, Immediate(kSmiTagMask));
  __ j(zero, &not_js_object, not_taken);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    // Look up the function and the map in the instanceof cache.
    NearLabel miss;
    __ mov(scratch, Immediate(Heap::kInstanceofCacheFunctionRootIndex));
    __ cmp(function,
           Operand::StaticArray(scratch, times_pointer_size, roots_address));
    __ j(not_equal, &miss);
    __ mov(scratch, Immediate(Heap::kInstanceofCacheMapRootIndex));
    __ cmp(map, Operand::StaticArray(
        scratch, times_pointer_size, roots_address));
    __ j(not_equal, &miss);
    __ mov(scratch, Immediate(Heap::kInstanceofCacheAnswerRootIndex));
    __ mov(eax, Operand::StaticArray(
        scratch, times_pointer_size, roots_address));
    __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);
    __ bind(&miss);
  }

  // Get the prototype of the function.
  __ TryGetFunctionPrototype(function, prototype, scratch, &slow);

  // Check that the function prototype is a JS object.
  __ test(prototype, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  __ IsObjectJSObjectType(prototype, scratch, scratch, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (!HasCallSiteInlineCheck()) {
  __ mov(scratch, Immediate(Heap::kInstanceofCacheMapRootIndex));
  __ mov(Operand::StaticArray(scratch, times_pointer_size, roots_address), map);
  __ mov(scratch, Immediate(Heap::kInstanceofCacheFunctionRootIndex));
  __ mov(Operand::StaticArray(scratch, times_pointer_size, roots_address),
         function);
  } else {
    // The constants for the code patching are based on no push instructions
    // at the call site.
    ASSERT(HasArgsInRegisters());
    // Get return address and delta to inlined map check.
    __ mov(scratch, Operand(esp, 0 * kPointerSize));
    __ sub(scratch, Operand(esp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ cmpb(Operand(scratch, 0), kCmpEdiImmediateByte1);
      __ Assert(equal, "InstanceofStub unexpected call site cache (cmp 1)");
      __ cmpb(Operand(scratch, 1), kCmpEdiImmediateByte2);
      __ Assert(equal, "InstanceofStub unexpected call site cache (cmp 2)");
    }
    __ mov(Operand(scratch, kDeltaToCmpImmediate), map);
  }

  // Loop through the prototype chain of the object looking for the function
  // prototype.
  __ mov(scratch, FieldOperand(map, Map::kPrototypeOffset));
  NearLabel loop, is_instance, is_not_instance;
  __ bind(&loop);
  __ cmp(scratch, Operand(prototype));
  __ j(equal, &is_instance);
  __ cmp(Operand(scratch), Immediate(Factory::null_value()));
  __ j(equal, &is_not_instance);
  __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
  __ mov(scratch, FieldOperand(scratch, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ Set(eax, Immediate(0));
    __ mov(scratch, Immediate(Heap::kInstanceofCacheAnswerRootIndex));
    __ mov(Operand::StaticArray(scratch,
                                times_pointer_size, roots_address), eax);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, Factory::true_value());
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
        scratch, times_pointer_size, roots_address), eax);
  } else {
    // Get return address and delta to inlined map check.
    __ mov(eax, Factory::false_value());
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
  __ test(function, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  __ CmpObjectType(function, JS_FUNCTION_TYPE, scratch);
  __ j(not_equal, &slow, not_taken);

  // Null is not instance of anything.
  __ cmp(object, Factory::null_value());
  __ j(not_equal, &object_not_null);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&object_not_null);
  // Smi values is not instance of anything.
  __ test(object, Immediate(kSmiTagMask));
  __ j(not_zero, &object_not_null_or_smi, not_taken);
  __ Set(eax, Immediate(Smi::FromInt(1)));
  __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);

  __ bind(&object_not_null_or_smi);
  // String values is not instance of anything.
  Condition is_string = masm->IsObjectStringType(object, scratch, scratch);
  __ j(NegateCondition(is_string), &slow);
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
    __ EnterInternalFrame();
    __ push(object);
    __ push(function);
    __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
    __ LeaveInternalFrame();
    NearLabel true_value, done;
    __ test(eax, Operand(eax));
    __ j(zero, &true_value);
    __ mov(eax, Factory::false_value());
    __ jmp(&done);
    __ bind(&true_value);
    __ mov(eax, Factory::true_value());
    __ bind(&done);
    __ ret((HasArgsInRegisters() ? 0 : 2) * kPointerSize);
  }
}


Register InstanceofStub::left() { return eax; }


Register InstanceofStub::right() { return edx; }


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value. To avoid duplicate
  // stubs the never NaN NaN condition is only taken into account if the
  // condition is equals.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 12));
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));
  return ConditionField::encode(static_cast<unsigned>(cc_))
         | RegisterField::encode(false)   // lhs_ and rhs_ are not used
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == equal ? never_nan_nan_ : false)
         | IncludeNumberCompareField::encode(include_number_compare_)
         | IncludeSmiCompareField::encode(include_smi_compare_);
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

  const char* include_smi_compare_name = "";
  if (!include_smi_compare_) {
    include_smi_compare_name = "_NO_SMI";
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "CompareStub_%s%s%s%s%s",
               cc_name,
               strict_name,
               never_nan_nan_name,
               include_number_compare_name,
               include_smi_compare_name);
  return name_;
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;

  // If the receiver is a smi trigger the non-string case.
  STATIC_ASSERT(kSmiTag == 0);
  __ test(object_, Immediate(kSmiTagMask));
  __ j(zero, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ test(result_, Immediate(kIsNotStringMask));
  __ j(not_zero, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  STATIC_ASSERT(kSmiTag == 0);
  __ test(index_, Immediate(kSmiTagMask));
  __ j(not_zero, &index_not_smi_);

  // Put smi-tagged index into scratch register.
  __ mov(scratch_, index_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ cmp(scratch_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  // We need special handling for non-flat strings.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test(result_, Immediate(kStringRepresentationMask));
  __ j(zero, &flat_string);

  // Handle non-flat strings.
  __ test(result_, Immediate(kIsConsStringMask));
  __ j(zero, &call_runtime_);

  // ConsString.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ cmp(FieldOperand(object_, ConsString::kSecondOffset),
         Immediate(Factory::empty_string()));
  __ j(not_equal, &call_runtime_);
  // Get the first of the two strings and load its instance type.
  __ mov(object_, FieldOperand(object_, ConsString::kFirstOffset));
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the first cons component is also non-flat, then go to runtime.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test(result_, Immediate(kStringRepresentationMask));
  __ j(not_zero, &call_runtime_);

  // Check for 1-byte or 2-byte string.
  __ bind(&flat_string);
  STATIC_ASSERT(kAsciiStringTag != 0);
  __ test(result_, Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the result register.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movzx_w(result_, FieldOperand(object_,
                                   scratch_, times_1,  // Scratch is smi-tagged.
                                   SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);
  __ SmiUntag(scratch_);
  __ movzx_b(result_, FieldOperand(object_,
                                   scratch_, times_1,
                                   SeqAsciiString::kHeaderSize));
  __ bind(&got_char_code);
  __ SmiTag(result_);
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
  if (!scratch_.is(eax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ mov(scratch_, eax);
  }
  __ pop(index_);
  __ pop(object_);
  // Reload the instance type.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  STATIC_ASSERT(kSmiTag == 0);
  __ test(scratch_, Immediate(kSmiTagMask));
  __ j(not_zero, index_out_of_range_);
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
  ASSERT(IsPowerOf2(String::kMaxAsciiCharCode + 1));
  __ test(code_,
          Immediate(kSmiTagMask |
                    ((~String::kMaxAsciiCharCode) << kSmiTagSize)));
  __ j(not_zero, &slow_case_, not_taken);

  __ Set(result_, Immediate(Factory::single_character_string_cache()));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiShiftSize == 0);
  // At this point code register contains smi tagged ascii char code.
  __ mov(result_, FieldOperand(result_,
                               code_, times_half_pointer_size,
                               FixedArray::kHeaderSize));
  __ cmp(result_, Factory::undefined_value());
  __ j(equal, &slow_case_, not_taken);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
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
  Label string_add_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  // Load the two arguments.
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (flags_ == NO_STRING_ADD_FLAGS) {
    __ test(eax, Immediate(kSmiTagMask));
    __ j(zero, &string_add_runtime);
    __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &string_add_runtime);

    // First argument is a a string, test second.
    __ test(edx, Immediate(kSmiTagMask));
    __ j(zero, &string_add_runtime);
    __ CmpObjectType(edx, FIRST_NONSTRING_TYPE, ebx);
    __ j(above_equal, &string_add_runtime);
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
  NearLabel second_not_zero_length, both_not_zero_length;
  __ mov(ecx, FieldOperand(edx, String::kLengthOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ test(ecx, Operand(ecx));
  __ j(not_zero, &second_not_zero_length);
  // Second string is empty, result is first string which is already in eax.
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ test(ebx, Operand(ebx));
  __ j(not_zero, &both_not_zero_length);
  // First string is empty, result is second string which is in edx.
  __ mov(eax, edx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // eax: first string
  // ebx: length of first string as a smi
  // ecx: length of second string as a smi
  // edx: second string
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);
  __ add(ebx, Operand(ecx));
  STATIC_ASSERT(Smi::kMaxValue == String::kMaxLength);
  // Handle exceptionally long strings in the runtime system.
  __ j(overflow, &string_add_runtime);
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ cmp(Operand(ebx), Immediate(Smi::FromInt(2)));
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(eax, edx, ebx, ecx,
                                         &string_add_runtime);

  // Get the two characters forming the new string.
  __ movzx_b(ebx, FieldOperand(eax, SeqAsciiString::kHeaderSize));
  __ movzx_b(ecx, FieldOperand(edx, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_two_character_string_no_reload;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, ebx, ecx, eax, edx, edi,
      &make_two_character_string_no_reload, &make_two_character_string);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Allocate a two character string.
  __ bind(&make_two_character_string);
  // Reload the arguments.
  __ mov(eax, Operand(esp, 2 * kPointerSize));  // First argument.
  __ mov(edx, Operand(esp, 1 * kPointerSize));  // Second argument.
  // Get the two characters forming the new string.
  __ movzx_b(ebx, FieldOperand(eax, SeqAsciiString::kHeaderSize));
  __ movzx_b(ecx, FieldOperand(edx, SeqAsciiString::kHeaderSize));
  __ bind(&make_two_character_string_no_reload);
  __ IncrementCounter(&Counters::string_add_make_two_char, 1);
  __ AllocateAsciiString(eax,  // Result.
                         2,    // Length.
                         edi,  // Scratch 1.
                         edx,  // Scratch 2.
                         &string_add_runtime);
  // Pack both characters in ebx.
  __ shl(ecx, kBitsPerByte);
  __ or_(ebx, Operand(ecx));
  // Set the characters in the new string.
  __ mov_w(FieldOperand(eax, SeqAsciiString::kHeaderSize), ebx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ cmp(Operand(ebx), Immediate(Smi::FromInt(String::kMinNonFlatLength)));
  __ j(below, &string_add_flat_result);

  // If result is not supposed to be flat allocate a cons string object. If both
  // strings are ascii the result is an ascii cons string.
  Label non_ascii, allocated, ascii_data;
  __ mov(edi, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(edi, FieldOperand(edi, Map::kInstanceTypeOffset));
  __ and_(ecx, Operand(edi));
  STATIC_ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ test(ecx, Immediate(kAsciiStringTag));
  __ j(zero, &non_ascii);
  __ bind(&ascii_data);
  // Allocate an acsii cons string.
  __ AllocateAsciiConsString(ecx, edi, no_reg, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  if (FLAG_debug_code) __ AbortIfNotSmi(ebx);
  __ mov(FieldOperand(ecx, ConsString::kLengthOffset), ebx);
  __ mov(FieldOperand(ecx, ConsString::kHashFieldOffset),
         Immediate(String::kEmptyHashField));
  __ mov(FieldOperand(ecx, ConsString::kFirstOffset), eax);
  __ mov(FieldOperand(ecx, ConsString::kSecondOffset), edx);
  __ mov(eax, ecx);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ascii characters.
  // ecx: first instance type AND second instance type.
  // edi: second instance type.
  __ test(ecx, Immediate(kAsciiDataHintMask));
  __ j(not_zero, &ascii_data);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ xor_(edi, Operand(ecx));
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ and_(edi, kAsciiStringTag | kAsciiDataHintTag);
  __ cmp(edi, kAsciiStringTag | kAsciiDataHintTag);
  __ j(equal, &ascii_data);
  // Allocate a two byte cons string.
  __ AllocateConsString(ecx, edi, no_reg, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are not
  // external strings.
  // eax: first string
  // ebx: length of resulting flat string as a smi
  // edx: second string
  __ bind(&string_add_flat_result);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ and_(ecx, kStringRepresentationMask);
  __ cmp(ecx, kExternalStringTag);
  __ j(equal, &string_add_runtime);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ and_(ecx, kStringRepresentationMask);
  __ cmp(ecx, kExternalStringTag);
  __ j(equal, &string_add_runtime);
  // Now check if both strings are ascii strings.
  // eax: first string
  // ebx: length of resulting flat string as a smi
  // edx: second string
  Label non_ascii_string_add_flat_result;
  STATIC_ASSERT(kStringEncodingMask == kAsciiStringTag);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kInstanceTypeOffset), kAsciiStringTag);
  __ j(zero, &non_ascii_string_add_flat_result);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kInstanceTypeOffset), kAsciiStringTag);
  __ j(zero, &string_add_runtime);

  // Both strings are ascii strings.  As they are short they are both flat.
  // ebx: length of resulting flat string as a smi
  __ SmiUntag(ebx);
  __ AllocateAsciiString(eax, ebx, ecx, edx, edi, &string_add_runtime);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(Operand(ecx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Load first argument and locate first character.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  // Load second argument and locate first character.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, true);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Handle creating a flat two byte result.
  // eax: first string - known to be two byte
  // ebx: length of resulting flat string as a smi
  // edx: second string
  __ bind(&non_ascii_string_add_flat_result);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ test_b(FieldOperand(ecx, Map::kInstanceTypeOffset), kAsciiStringTag);
  __ j(not_zero, &string_add_runtime);
  // Both strings are two byte strings. As they are short they are both
  // flat.
  __ SmiUntag(ebx);
  __ AllocateTwoByteString(eax, ebx, ecx, edx, edi, &string_add_runtime);
  // eax: result string
  __ mov(ecx, eax);
  // Locate first character of result.
  __ add(Operand(ecx),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load first argument and locate first character.
  __ mov(edx, Operand(esp, 2 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ add(Operand(edx),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: first character of result
  // edx: first char of first argument
  // edi: length of first argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  // Load second argument and locate first character.
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ mov(edi, FieldOperand(edx, String::kLengthOffset));
  __ SmiUntag(edi);
  __ add(Operand(edx), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // eax: result string
  // ecx: next character of result
  // edx: first char of second argument
  // edi: length of second argument
  StringHelper::GenerateCopyCharacters(masm, ecx, edx, edi, ebx, false);
  __ IncrementCounter(&Counters::string_add_native, 1);
  __ ret(2 * kPointerSize);

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
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
  __ test(arg, Immediate(kSmiTagMask));
  __ j(zero, &not_string);
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
  __ test(arg, Immediate(kSmiTagMask));
  __ j(zero, slow);
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
  NearLabel loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ mov_b(scratch, Operand(src, 0));
    __ mov_b(Operand(dest, 0), scratch);
    __ add(Operand(src), Immediate(1));
    __ add(Operand(dest), Immediate(1));
  } else {
    __ mov_w(scratch, Operand(src, 0));
    __ mov_w(Operand(dest, 0), scratch);
    __ add(Operand(src), Immediate(2));
    __ add(Operand(dest), Immediate(2));
  }
  __ sub(Operand(count), Immediate(1));
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
  __ test(count, Operand(count));
  __ j(zero, &done);

  // Make count the number of bytes to copy.
  if (!ascii) {
    __ shl(count, 1);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  NearLabel last_bytes;
  __ test(count, Immediate(~3));
  __ j(zero, &last_bytes);

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
  __ test(count, Operand(count));
  __ j(zero, &done);

  // Copy remaining characters.
  NearLabel loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(src, 0));
  __ mov_b(Operand(dest, 0), scratch);
  __ add(Operand(src), Immediate(1));
  __ add(Operand(dest), Immediate(1));
  __ sub(Operand(count), Immediate(1));
  __ j(not_zero, &loop);

  __ bind(&done);
}


void StringHelper::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
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
  // different hash algorithm. Don't try to look for these in the symbol table.
  NearLabel not_array_index;
  __ mov(scratch, c1);
  __ sub(Operand(scratch), Immediate(static_cast<int>('0')));
  __ cmp(Operand(scratch), Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index);
  __ mov(scratch, c2);
  __ sub(Operand(scratch), Immediate(static_cast<int>('0')));
  __ cmp(Operand(scratch), Immediate(static_cast<int>('9' - '0')));
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
  __ or_(chars, Operand(c2));

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load the symbol table.
  Register symbol_table = c2;
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(scratch, Immediate(Heap::kSymbolTableRootIndex));
  __ mov(symbol_table,
         Operand::StaticArray(scratch, times_pointer_size, roots_address));

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ mov(mask, FieldOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ SmiUntag(mask);
  __ sub(Operand(mask), Immediate(1));

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string
  // symbol_table: symbol table
  // mask:         capacity mask
  // scratch:      -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes], next_probe_pop_mask[kProbes];
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in symbol table.
    __ mov(scratch, hash);
    if (i > 0) {
      __ add(Operand(scratch), Immediate(SymbolTable::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(mask));

    // Load the entry from the symbol table.
    Register candidate = scratch;  // Scratch register contains candidate.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ mov(candidate,
           FieldOperand(symbol_table,
                        scratch,
                        times_pointer_size,
                        SymbolTable::kElementsStartOffset));

    // If entry is undefined no string with this hash can be found.
    __ cmp(candidate, Factory::undefined_value());
    __ j(equal, not_found);

    // If length is not 2 the string is not a candidate.
    __ cmp(FieldOperand(candidate, String::kLengthOffset),
           Immediate(Smi::FromInt(2)));
    __ j(not_equal, &next_probe[i]);

    // As we are out of registers save the mask on the stack and use that
    // register as a temporary.
    __ push(mask);
    Register temp = mask;

    // Check that the candidate is a non-external ascii string.
    __ mov(temp, FieldOperand(candidate, HeapObject::kMapOffset));
    __ movzx_b(temp, FieldOperand(temp, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe_pop_mask[i]);

    // Check if the two characters match.
    __ mov(temp, FieldOperand(candidate, SeqAsciiString::kHeaderSize));
    __ and_(temp, 0x0000ffff);
    __ cmp(chars, Operand(temp));
    __ j(equal, &found_in_symbol_table);
    __ bind(&next_probe_pop_mask[i]);
    __ pop(mask);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = scratch;
  __ bind(&found_in_symbol_table);
  __ pop(mask);  // Pop saved mask from the stack.
  if (!result.is(eax)) {
    __ mov(eax, result);
  }
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = character + (character << 10);
  __ mov(hash, character);
  __ shl(hash, 10);
  __ add(hash, Operand(character));
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ sar(scratch, 6);
  __ xor_(hash, Operand(scratch));
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character,
                                            Register scratch) {
  // hash += character;
  __ add(hash, Operand(character));
  // hash += hash << 10;
  __ mov(scratch, hash);
  __ shl(scratch, 10);
  __ add(hash, Operand(scratch));
  // hash ^= hash >> 6;
  __ mov(scratch, hash);
  __ sar(scratch, 6);
  __ xor_(hash, Operand(scratch));
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ mov(scratch, hash);
  __ shl(scratch, 3);
  __ add(hash, Operand(scratch));
  // hash ^= hash >> 11;
  __ mov(scratch, hash);
  __ sar(scratch, 11);
  __ xor_(hash, Operand(scratch));
  // hash += hash << 15;
  __ mov(scratch, hash);
  __ shl(scratch, 15);
  __ add(hash, Operand(scratch));

  // if (hash == 0) hash = 27;
  NearLabel hash_not_zero;
  __ test(hash, Operand(hash));
  __ j(not_zero, &hash_not_zero);
  __ mov(hash, Immediate(27));
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
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(eax, ebx, ebx);
  __ j(NegateCondition(is_string), &runtime);

  // eax: string
  // ebx: instance type

  // Calculate length of sub string using the smi values.
  Label result_longer_than_two;
  __ mov(ecx, Operand(esp, 1 * kPointerSize));  // To index.
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, &runtime);
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // From index.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(not_zero, &runtime);
  __ sub(ecx, Operand(edx));
  __ cmp(ecx, FieldOperand(eax, String::kLengthOffset));
  Label return_eax;
  __ j(equal, &return_eax);
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ SmiUntag(ecx);  // Result length is no longer smi.
  __ cmp(ecx, 2);
  __ j(greater, &result_longer_than_two);
  __ j(less, &runtime);

  // Sub string of length 2 requested.
  // eax: string
  // ebx: instance type
  // ecx: sub string length (value is 2)
  // edx: from index (smi)
  __ JumpIfInstanceTypeIsNotSequentialAscii(ebx, ebx, &runtime);

  // Get the two characters forming the sub string.
  __ SmiUntag(edx);  // From index is no longer smi.
  __ movzx_b(ebx, FieldOperand(eax, edx, times_1, SeqAsciiString::kHeaderSize));
  __ movzx_b(ecx,
             FieldOperand(eax, edx, times_1, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, ebx, ecx, eax, edx, edi,
      &make_two_character_string, &make_two_character_string);
  __ ret(3 * kPointerSize);

  __ bind(&make_two_character_string);
  // Setup registers for allocating the two character string.
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ Set(ecx, Immediate(2));

  __ bind(&result_longer_than_two);
  // eax: string
  // ebx: instance type
  // ecx: result string length
  // Check for flat ascii string
  Label non_ascii_flat;
  __ JumpIfInstanceTypeIsNotSequentialAscii(ebx, ebx, &non_ascii_flat);

  // Allocate the result.
  __ AllocateAsciiString(eax, ecx, ebx, edx, edi, &runtime);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(Operand(edi), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ mov(esi, Operand(esp, 3 * kPointerSize));
  __ add(Operand(esi), Immediate(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ mov(ebx, Operand(esp, 2 * kPointerSize));  // from
  __ SmiUntag(ebx);
  __ add(esi, Operand(ebx));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, true);
  __ mov(esi, edx);  // Restore esi.
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(3 * kPointerSize);

  __ bind(&non_ascii_flat);
  // eax: string
  // ebx: instance type & kStringRepresentationMask | kStringEncodingMask
  // ecx: result string length
  // Check for flat two byte string
  __ cmp(ebx, kSeqStringTag | kTwoByteStringTag);
  __ j(not_equal, &runtime);

  // Allocate the result.
  __ AllocateTwoByteString(eax, ecx, ebx, edx, edi, &runtime);

  // eax: result string
  // ecx: result string length
  __ mov(edx, esi);  // esi used by following code.
  // Locate first character of result.
  __ mov(edi, eax);
  __ add(Operand(edi),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Load string argument and locate character of sub string start.
  __ mov(esi, Operand(esp, 3 * kPointerSize));
  __ add(Operand(esi),
         Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ mov(ebx, Operand(esp, 2 * kPointerSize));  // from
  // As from is a smi it is 2 times the value which matches the size of a two
  // byte character.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(esi, Operand(ebx));

  // eax: result string
  // ecx: result length
  // edx: original value of esi
  // edi: first character of result
  // esi: character of sub string start
  StringHelper::GenerateCopyCharactersREP(masm, edi, esi, ecx, ebx, false);
  __ mov(esi, edx);  // Restore esi.

  __ bind(&return_eax);
  __ IncrementCounter(&Counters::sub_string_native, 1);
  __ ret(3 * kPointerSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3) {
  Label result_not_equal;
  Label result_greater;
  Label compare_lengths;

  __ IncrementCounter(&Counters::string_compare_native, 1);

  // Find minimum length.
  NearLabel left_shorter;
  __ mov(scratch1, FieldOperand(left, String::kLengthOffset));
  __ mov(scratch3, scratch1);
  __ sub(scratch3, FieldOperand(right, String::kLengthOffset));

  Register length_delta = scratch3;

  __ j(less_equal, &left_shorter);
  // Right string is shorter. Change scratch1 to be length of right string.
  __ sub(scratch1, Operand(length_delta));
  __ bind(&left_shorter);

  Register min_length = scratch1;

  // If either length is zero, just compare lengths.
  __ test(min_length, Operand(min_length));
  __ j(zero, &compare_lengths);

  // Change index to run from -min_length to -1 by adding min_length
  // to string start. This means that loop ends when index reaches zero,
  // which doesn't need an additional compare.
  __ SmiUntag(min_length);
  __ lea(left,
         FieldOperand(left,
                      min_length, times_1,
                      SeqAsciiString::kHeaderSize));
  __ lea(right,
         FieldOperand(right,
                      min_length, times_1,
                      SeqAsciiString::kHeaderSize));
  __ neg(min_length);

  Register index = min_length;  // index = -min_length;

  {
    // Compare loop.
    NearLabel loop;
    __ bind(&loop);
    // Compare characters.
    __ mov_b(scratch2, Operand(left, index, times_1, 0));
    __ cmpb(scratch2, Operand(right, index, times_1, 0));
    __ j(not_equal, &result_not_equal);
    __ add(Operand(index), Immediate(1));
    __ j(not_zero, &loop);
  }

  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  __ test(length_delta, Operand(length_delta));
  __ j(not_zero, &result_not_equal);

  // Result is EQUAL.
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  __ bind(&result_not_equal);
  __ j(greater, &result_greater);

  // Result is LESS.
  __ Set(eax, Immediate(Smi::FromInt(LESS)));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Set(eax, Immediate(Smi::FromInt(GREATER)));
  __ ret(0);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: right string
  //  esp[8]: left string

  __ mov(edx, Operand(esp, 2 * kPointerSize));  // left
  __ mov(eax, Operand(esp, 1 * kPointerSize));  // right

  NearLabel not_same;
  __ cmp(edx, Operand(eax));
  __ j(not_equal, &not_same);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Set(eax, Immediate(Smi::FromInt(EQUAL)));
  __ IncrementCounter(&Counters::string_compare_native, 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both objects are sequential ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(edx, eax, ecx, ebx, &runtime);

  // Compare flat ascii strings.
  // Drop arguments from the stack.
  __ pop(ecx);
  __ add(Operand(esp), Immediate(2 * kPointerSize));
  __ push(ecx);
  GenerateCompareFlatAsciiStrings(masm, edx, eax, ecx, ebx, edi);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void StringCharAtStub::Generate(MacroAssembler* masm) {
  // Expects two arguments (object, index) on the stack:

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: index
  //  esp[8]: object

  Register object = ebx;
  Register index = eax;
  Register scratch1 = ecx;
  Register scratch2 = edx;
  Register result = eax;

  __ pop(scratch1);  // Return address.
  __ pop(index);
  __ pop(object);
  __ push(scratch1);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  scratch1,
                                  scratch2,
                                  result,
                                  &need_conversion,
                                  &need_conversion,
                                  &index_out_of_range,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ jmp(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ Set(result, Immediate(Factory::empty_string()));
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ Set(result, Immediate(Smi::FromInt(0)));
  __ jmp(&done);

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm, call_helper);

  __ bind(&done);
  __ ret(0);
}

void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMIS);
  NearLabel miss;
  __ mov(ecx, Operand(edx));
  __ or_(ecx, Operand(eax));
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, &miss, not_taken);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ sub(eax, Operand(edx));
  } else {
    NearLabel done;
    __ sub(edx, Operand(eax));
    __ j(no_overflow, &done);
    // Correct sign of result in case of overflow.
    __ not_(edx);
    __ bind(&done);
    __ mov(eax, edx);
  }
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateHeapNumbers(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::HEAP_NUMBERS);

  NearLabel generic_stub;
  NearLabel unordered;
  NearLabel miss;
  __ mov(ecx, Operand(edx));
  __ and_(ecx, Operand(eax));
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(zero, &generic_stub, not_taken);

  __ CmpObjectType(eax, HEAP_NUMBER_TYPE, ecx);
  __ j(not_equal, &miss, not_taken);
  __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ecx);
  __ j(not_equal, &miss, not_taken);

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved or SS2 or CMOV is unsupported.
  if (CpuFeatures::IsSupported(SSE2) && CpuFeatures::IsSupported(CMOV)) {
    CpuFeatures::Scope scope1(SSE2);
    CpuFeatures::Scope scope2(CMOV);

    // Load left and right operand
    __ movdbl(xmm0, FieldOperand(edx, HeapNumber::kValueOffset));
    __ movdbl(xmm1, FieldOperand(eax, HeapNumber::kValueOffset));

    // Compare operands
    __ ucomisd(xmm0, xmm1);

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered, not_taken);

    // Return a result of -1, 0, or 1, based on EFLAGS.
    // Performing mov, because xor would destroy the flag register.
    __ mov(eax, 0);  // equal
    __ mov(ecx, Immediate(Smi::FromInt(1)));
    __ cmov(above, eax, Operand(ecx));
    __ mov(ecx, Immediate(Smi::FromInt(-1)));
    __ cmov(below, eax, Operand(ecx));
    __ ret(0);

    __ bind(&unordered);
  }

  CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS);
  __ bind(&generic_stub);
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::OBJECTS);
  NearLabel miss;
  __ mov(ecx, Operand(edx));
  __ and_(ecx, Operand(eax));
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  __ CmpObjectType(eax, JS_OBJECT_TYPE, ecx);
  __ j(not_equal, &miss, not_taken);
  __ CmpObjectType(edx, JS_OBJECT_TYPE, ecx);
  __ j(not_equal, &miss, not_taken);

  ASSERT(GetCondition() == equal);
  __ sub(eax, Operand(edx));
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  // Save the registers.
  __ pop(ecx);
  __ push(edx);
  __ push(eax);
  __ push(ecx);

  // Call the runtime system in a fresh internal frame.
  ExternalReference miss = ExternalReference(IC_Utility(IC::kCompareIC_Miss));
  __ EnterInternalFrame();
  __ push(edx);
  __ push(eax);
  __ push(Immediate(Smi::FromInt(op_)));
  __ CallExternalReference(miss, 3);
  __ LeaveInternalFrame();

  // Compute the entry point of the rewritten stub.
  __ lea(edi, FieldOperand(eax, Code::kHeaderSize));

  // Restore registers.
  __ pop(ecx);
  __ pop(eax);
  __ pop(edx);
  __ push(ecx);

  // Do a tail call to the rewritten stub.
  __ jmp(Operand(edi));
}


// Loads a indexed element from a pixel array.
void GenerateFastPixelArrayLoad(MacroAssembler* masm,
                                Register receiver,
                                Register key,
                                Register elements,
                                Register untagged_key,
                                Register result,
                                Label* not_pixel_array,
                                Label* key_not_smi,
                                Label* out_of_range) {
  // Register use:
  //   receiver - holds the receiver and is unchanged.
  //   key - holds the key and is unchanged (must be a smi).
  //   elements - is set to the the receiver's element if
  //       the receiver doesn't have a pixel array or the
  //       key is not a smi, otherwise it's the elements'
  //       external pointer.
  //   untagged_key - is set to the untagged key

  // Some callers already have verified that the key is a smi.  key_not_smi is
  // set to NULL as a sentinel for that case.  Otherwise, add an explicit check
  // to ensure the key is a smi must be added.
  if (key_not_smi != NULL) {
    __ JumpIfNotSmi(key, key_not_smi);
  } else {
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(key);
    }
  }
  __ mov(untagged_key, key);
  __ SmiUntag(untagged_key);

  __ mov(elements, FieldOperand(receiver, JSObject::kElementsOffset));
  // By passing NULL as not_pixel_array, callers signal that they have already
  // verified that the receiver has pixel array elements.
  if (not_pixel_array != NULL) {
    __ CheckMap(elements, Factory::pixel_array_map(), not_pixel_array, true);
  } else {
    if (FLAG_debug_code) {
      // Map check should have already made sure that elements is a pixel array.
      __ cmp(FieldOperand(elements, HeapObject::kMapOffset),
             Immediate(Factory::pixel_array_map()));
      __ Assert(equal, "Elements isn't a pixel array");
    }
  }

  // Key must be in range.
  __ cmp(untagged_key, FieldOperand(elements, PixelArray::kLengthOffset));
  __ j(above_equal, out_of_range);  // unsigned check handles negative keys.

  // Perform the indexed load and tag the result as a smi.
  __ mov(elements, FieldOperand(elements, PixelArray::kExternalPointerOffset));
  __ movzx_b(result, Operand(elements, untagged_key, times_1, 0));
  __ SmiTag(result);
  __ ret(0);
}


// Stores an indexed element into a pixel array, clamping the stored value.
void GenerateFastPixelArrayStore(MacroAssembler* masm,
                                 Register receiver,
                                 Register key,
                                 Register value,
                                 Register elements,
                                 Register scratch1,
                                 bool load_elements_from_receiver,
                                 Label* key_not_smi,
                                 Label* value_not_smi,
                                 Label* not_pixel_array,
                                 Label* out_of_range) {
  // Register use:
  //   receiver - holds the receiver and is unchanged unless the
  //              store succeeds.
  //   key - holds the key (must be a smi) and is unchanged.
  //   value - holds the value (must be a smi) and is unchanged.
  //   elements - holds the element object of the receiver on entry if
  //              load_elements_from_receiver is false, otherwise used
  //              internally to store the pixel arrays elements and
  //              external array pointer.
  //
  // receiver, key and value remain unmodified until it's guaranteed that the
  // store will succeed.
  Register external_pointer = elements;
  Register untagged_key = scratch1;
  Register untagged_value = receiver;  // Only set once success guaranteed.

  // Fetch the receiver's elements if the caller hasn't already done so.
  if (load_elements_from_receiver) {
    __ mov(elements, FieldOperand(receiver, JSObject::kElementsOffset));
  }

  // By passing NULL as not_pixel_array, callers signal that they have already
  // verified that the receiver has pixel array elements.
  if (not_pixel_array != NULL) {
    __ CheckMap(elements, Factory::pixel_array_map(), not_pixel_array, true);
  } else {
    if (FLAG_debug_code) {
      // Map check should have already made sure that elements is a pixel array.
      __ cmp(FieldOperand(elements, HeapObject::kMapOffset),
             Immediate(Factory::pixel_array_map()));
      __ Assert(equal, "Elements isn't a pixel array");
    }
  }

  // Some callers already have verified that the key is a smi.  key_not_smi is
  // set to NULL as a sentinel for that case.  Otherwise, add an explicit check
  // to ensure the key is a smi must be added.
  if (key_not_smi != NULL) {
    __ JumpIfNotSmi(key, key_not_smi);
  } else {
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(key);
    }
  }

  // Key must be a smi and it must be in range.
  __ mov(untagged_key, key);
  __ SmiUntag(untagged_key);
  __ cmp(untagged_key, FieldOperand(elements, PixelArray::kLengthOffset));
  __ j(above_equal, out_of_range);  // unsigned check handles negative keys.

  // Value must be a smi.
  __ JumpIfNotSmi(value, value_not_smi);
  __ mov(untagged_value, value);
  __ SmiUntag(untagged_value);

  {  // Clamp the value to [0..255].
    NearLabel done;
    __ test(untagged_value, Immediate(0xFFFFFF00));
    __ j(zero, &done);
    __ setcc(negative, untagged_value);  // 1 if negative, 0 if positive.
    __ dec_b(untagged_value);  // 0 if negative, 255 if positive.
    __ bind(&done);
  }

  __ mov(external_pointer,
         FieldOperand(elements, PixelArray::kExternalPointerOffset));
  __ mov_b(Operand(external_pointer, untagged_key, times_1, 0), untagged_value);
  __ ret(0);  // Return value in eax.
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
