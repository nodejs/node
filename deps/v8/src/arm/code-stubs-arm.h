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

#ifndef V8_ARM_CODE_STUBS_ARM_H_
#define V8_ARM_CODE_STUBS_ARM_H_

#include "ic-inl.h"

namespace v8 {
namespace internal {


// Compute a transcendental math function natively, or call the
// TranscendentalCache runtime function.
class TranscendentalCacheStub: public CodeStub {
 public:
  explicit TranscendentalCacheStub(TranscendentalCache::Type type)
      : type_(type) {}
  void Generate(MacroAssembler* masm);
 private:
  TranscendentalCache::Type type_;
  Major MajorKey() { return TranscendentalCache; }
  int MinorKey() { return type_; }
  Runtime::FunctionId RuntimeFunction();
};


class ToBooleanStub: public CodeStub {
 public:
  explicit ToBooleanStub(Register tos) : tos_(tos) { }

  void Generate(MacroAssembler* masm);

 private:
  Register tos_;
  Major MajorKey() { return ToBoolean; }
  int MinorKey() { return tos_.code(); }
};


class GenericBinaryOpStub : public CodeStub {
 public:
  static const int kUnknownIntValue = -1;

  GenericBinaryOpStub(Token::Value op,
                      OverwriteMode mode,
                      Register lhs,
                      Register rhs,
                      int constant_rhs = kUnknownIntValue)
      : op_(op),
        mode_(mode),
        lhs_(lhs),
        rhs_(rhs),
        constant_rhs_(constant_rhs),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op, constant_rhs)),
        runtime_operands_type_(BinaryOpIC::UNINIT_OR_SMI),
        name_(NULL) { }

  GenericBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        lhs_(LhsRegister(RegisterBits::decode(key))),
        rhs_(RhsRegister(RegisterBits::decode(key))),
        constant_rhs_(KnownBitsForMinorKey(KnownIntBits::decode(key))),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op_, constant_rhs_)),
        runtime_operands_type_(type_info),
        name_(NULL) { }

 private:
  Token::Value op_;
  OverwriteMode mode_;
  Register lhs_;
  Register rhs_;
  int constant_rhs_;
  bool specialized_on_rhs_;
  BinaryOpIC::TypeInfo runtime_operands_type_;
  char* name_;

  static const int kMaxKnownRhs = 0x40000000;
  static const int kKnownRhsKeyBits = 6;

  // Minor key encoding in 17 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 6> {};
  class TypeInfoBits: public BitField<int, 8, 3> {};
  class RegisterBits: public BitField<bool, 11, 1> {};
  class KnownIntBits: public BitField<int, 12, kKnownRhsKeyBits> {};

  Major MajorKey() { return GenericBinaryOp; }
  int MinorKey() {
    ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
           (lhs_.is(r1) && rhs_.is(r0)));
    // Encode the parameters in a unique 18 bit value.
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | KnownIntBits::encode(MinorKeyForKnownInt())
           | TypeInfoBits::encode(runtime_operands_type_)
           | RegisterBits::encode(lhs_.is(r0));
  }

  void Generate(MacroAssembler* masm);
  void HandleNonSmiBitwiseOp(MacroAssembler* masm,
                             Register lhs,
                             Register rhs);
  void HandleBinaryOpSlowCases(MacroAssembler* masm,
                               Label* not_smi,
                               Register lhs,
                               Register rhs,
                               const Builtins::JavaScript& builtin);
  void GenerateTypeTransition(MacroAssembler* masm);

  static bool RhsIsOneWeWantToOptimizeFor(Token::Value op, int constant_rhs) {
    if (constant_rhs == kUnknownIntValue) return false;
    if (op == Token::DIV) return constant_rhs >= 2 && constant_rhs <= 3;
    if (op == Token::MOD) {
      if (constant_rhs <= 1) return false;
      if (constant_rhs <= 10) return true;
      if (constant_rhs <= kMaxKnownRhs && IsPowerOf2(constant_rhs)) return true;
      return false;
    }
    return false;
  }

  int MinorKeyForKnownInt() {
    if (!specialized_on_rhs_) return 0;
    if (constant_rhs_ <= 10) return constant_rhs_ + 1;
    ASSERT(IsPowerOf2(constant_rhs_));
    int key = 12;
    int d = constant_rhs_;
    while ((d & 1) == 0) {
      key++;
      d >>= 1;
    }
    ASSERT(key >= 0 && key < (1 << kKnownRhsKeyBits));
    return key;
  }

  int KnownBitsForMinorKey(int key) {
    if (!key) return 0;
    if (key <= 11) return key - 1;
    int d = 1;
    while (key != 12) {
      key--;
      d <<= 1;
    }
    return d;
  }

  Register LhsRegister(bool lhs_is_r0) {
    return lhs_is_r0 ? r0 : r1;
  }

  Register RhsRegister(bool lhs_is_r0) {
    return lhs_is_r0 ? r1 : r0;
  }

  bool HasSmiSmiFastPath() {
    return op_ != Token::DIV;
  }

  bool ShouldGenerateSmiCode() {
    return ((op_ != Token::DIV && op_ != Token::MOD) || specialized_on_rhs_) &&
        runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
        runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  bool ShouldGenerateFPCode() {
    return runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  virtual int GetCodeKind() { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(runtime_operands_type_);
  }

  const char* GetName();

  virtual void FinishCode(Code* code) {
    code->set_binary_op_type(runtime_operands_type_);
  }

#ifdef DEBUG
  void Print() {
    if (!specialized_on_rhs_) {
      PrintF("GenericBinaryOpStub (%s)\n", Token::String(op_));
    } else {
      PrintF("GenericBinaryOpStub (%s by %d)\n",
             Token::String(op_),
             constant_rhs_);
    }
  }
#endif
};


class TypeRecordingBinaryOpStub: public CodeStub {
 public:
  TypeRecordingBinaryOpStub(Token::Value op, OverwriteMode mode)
      : op_(op),
        mode_(mode),
        operands_type_(TRBinaryOpIC::UNINITIALIZED),
        result_type_(TRBinaryOpIC::UNINITIALIZED),
        name_(NULL) {
    use_vfp3_ = CpuFeatures::IsSupported(VFP3);
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  TypeRecordingBinaryOpStub(
      int key,
      TRBinaryOpIC::TypeInfo operands_type,
      TRBinaryOpIC::TypeInfo result_type = TRBinaryOpIC::UNINITIALIZED)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        use_vfp3_(VFP3Bits::decode(key)),
        operands_type_(operands_type),
        result_type_(result_type),
        name_(NULL) { }

 private:
  enum SmiCodeGenerateHeapNumberResults {
    ALLOW_HEAPNUMBER_RESULTS,
    NO_HEAPNUMBER_RESULTS
  };

  Token::Value op_;
  OverwriteMode mode_;
  bool use_vfp3_;

  // Operand type information determined at runtime.
  TRBinaryOpIC::TypeInfo operands_type_;
  TRBinaryOpIC::TypeInfo result_type_;

  char* name_;

  const char* GetName();

#ifdef DEBUG
  void Print() {
    PrintF("TypeRecordingBinaryOpStub %d (op %s), "
           "(mode %d, runtime_type_info %s)\n",
           MinorKey(),
           Token::String(op_),
           static_cast<int>(mode_),
           TRBinaryOpIC::GetName(operands_type_));
  }
#endif

  // Minor key encoding in 16 bits RRRTTTVOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 7> {};
  class VFP3Bits: public BitField<bool, 9, 1> {};
  class OperandTypeInfoBits: public BitField<TRBinaryOpIC::TypeInfo, 10, 3> {};
  class ResultTypeInfoBits: public BitField<TRBinaryOpIC::TypeInfo, 13, 3> {};

  Major MajorKey() { return TypeRecordingBinaryOp; }
  int MinorKey() {
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | VFP3Bits::encode(use_vfp3_)
           | OperandTypeInfoBits::encode(operands_type_)
           | ResultTypeInfoBits::encode(result_type_);
  }

  void Generate(MacroAssembler* masm);
  void GenerateGeneric(MacroAssembler* masm);
  void GenerateSmiSmiOperation(MacroAssembler* masm);
  void GenerateFPOperation(MacroAssembler* masm,
                           bool smi_operands,
                           Label* not_numbers,
                           Label* gc_required);
  void GenerateSmiCode(MacroAssembler* masm,
                       Label* gc_required,
                       SmiCodeGenerateHeapNumberResults heapnumber_results);
  void GenerateLoadArguments(MacroAssembler* masm);
  void GenerateReturn(MacroAssembler* masm);
  void GenerateUninitializedStub(MacroAssembler* masm);
  void GenerateSmiStub(MacroAssembler* masm);
  void GenerateInt32Stub(MacroAssembler* masm);
  void GenerateHeapNumberStub(MacroAssembler* masm);
  void GenerateOddballStub(MacroAssembler* masm);
  void GenerateStringStub(MacroAssembler* masm);
  void GenerateGenericStub(MacroAssembler* masm);
  void GenerateAddStrings(MacroAssembler* masm);
  void GenerateCallRuntime(MacroAssembler* masm);

  void GenerateHeapResultAllocation(MacroAssembler* masm,
                                    Register result,
                                    Register heap_number_map,
                                    Register scratch1,
                                    Register scratch2,
                                    Label* gc_required);
  void GenerateRegisterArgsPush(MacroAssembler* masm);
  void GenerateTypeTransition(MacroAssembler* masm);
  void GenerateTypeTransitionWithSavedArgs(MacroAssembler* masm);

  virtual int GetCodeKind() { return Code::TYPE_RECORDING_BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return TRBinaryOpIC::ToState(operands_type_);
  }

  virtual void FinishCode(Code* code) {
    code->set_type_recording_binary_op_type(operands_type_);
    code->set_type_recording_binary_op_result_type(result_type_);
  }

  friend class CodeGenerator;
};


// Flag that indicates how to generate code for the stub StringAddStub.
enum StringAddFlags {
  NO_STRING_ADD_FLAGS = 0,
  // Omit left string check in stub (left is definitely a string).
  NO_STRING_CHECK_LEFT_IN_STUB = 1 << 0,
  // Omit right string check in stub (right is definitely a string).
  NO_STRING_CHECK_RIGHT_IN_STUB = 1 << 1,
  // Omit both string checks in stub.
  NO_STRING_CHECK_IN_STUB =
      NO_STRING_CHECK_LEFT_IN_STUB | NO_STRING_CHECK_RIGHT_IN_STUB
};


class StringAddStub: public CodeStub {
 public:
  explicit StringAddStub(StringAddFlags flags) : flags_(flags) {}

 private:
  Major MajorKey() { return StringAdd; }
  int MinorKey() { return flags_; }

  void Generate(MacroAssembler* masm);

  void GenerateConvertArgument(MacroAssembler* masm,
                               int stack_offset,
                               Register arg,
                               Register scratch1,
                               Register scratch2,
                               Register scratch3,
                               Register scratch4,
                               Label* slow);

  const StringAddFlags flags_;
};


class SubStringStub: public CodeStub {
 public:
  SubStringStub() {}

 private:
  Major MajorKey() { return SubString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};



class StringCompareStub: public CodeStub {
 public:
  StringCompareStub() { }

  // Compare two flat ASCII strings and returns result in r0.
  // Does not use the stack.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

 private:
  Major MajorKey() { return StringCompare; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


// This stub can do a fast mod operation without using fp.
// It is tail called from the GenericBinaryOpStub and it always
// returns an answer.  It never causes GC so it doesn't need a real frame.
//
// The inputs are always positive Smis.  This is never called
// where the denominator is a power of 2.  We handle that separately.
//
// If we consider the denominator as an odd number multiplied by a power of 2,
// then:
// * The exponent (power of 2) is in the shift_distance register.
// * The odd number is in the odd_number register.  It is always in the range
//   of 3 to 25.
// * The bits from the numerator that are to be copied to the answer (there are
//   shift_distance of them) are in the mask_bits register.
// * The other bits of the numerator have been shifted down and are in the lhs
//   register.
class IntegerModStub : public CodeStub {
 public:
  IntegerModStub(Register result,
                 Register shift_distance,
                 Register odd_number,
                 Register mask_bits,
                 Register lhs,
                 Register scratch)
      : result_(result),
        shift_distance_(shift_distance),
        odd_number_(odd_number),
        mask_bits_(mask_bits),
        lhs_(lhs),
        scratch_(scratch) {
    // We don't code these in the minor key, so they should always be the same.
    // We don't really want to fix that since this stub is rather large and we
    // don't want many copies of it.
    ASSERT(shift_distance_.is(r9));
    ASSERT(odd_number_.is(r4));
    ASSERT(mask_bits_.is(r3));
    ASSERT(scratch_.is(r5));
  }

 private:
  Register result_;
  Register shift_distance_;
  Register odd_number_;
  Register mask_bits_;
  Register lhs_;
  Register scratch_;

  // Minor key encoding in 16 bits.
  class ResultRegisterBits: public BitField<int, 0, 4> {};
  class LhsRegisterBits: public BitField<int, 4, 4> {};

  Major MajorKey() { return IntegerMod; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return ResultRegisterBits::encode(result_.code())
           | LhsRegisterBits::encode(lhs_.code());
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "IntegerModStub"; }

  // Utility functions.
  void DigitSum(MacroAssembler* masm,
                Register lhs,
                int mask,
                int shift,
                Label* entry);
  void DigitSum(MacroAssembler* masm,
                Register lhs,
                Register scratch,
                int mask,
                int shift1,
                int shift2,
                Label* entry);
  void ModGetInRangeBySubtraction(MacroAssembler* masm,
                                  Register lhs,
                                  int shift,
                                  int rhs);
  void ModReduce(MacroAssembler* masm,
                 Register lhs,
                 int max,
                 int denominator);
  void ModAnswer(MacroAssembler* masm,
                 Register result,
                 Register shift_distance,
                 Register mask_bits,
                 Register sum_of_digits);


#ifdef DEBUG
  void Print() { PrintF("IntegerModStub\n"); }
#endif
};


// This stub can convert a signed int32 to a heap number (double).  It does
// not work for int32s that are in Smi range!  No GC occurs during this stub
// so you don't have to set up the frame.
class WriteInt32ToHeapNumberStub : public CodeStub {
 public:
  WriteInt32ToHeapNumberStub(Register the_int,
                             Register the_heap_number,
                             Register scratch)
      : the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch) { }

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;

  // Minor key encoding in 16 bits.
  class IntRegisterBits: public BitField<int, 0, 4> {};
  class HeapNumberRegisterBits: public BitField<int, 4, 4> {};
  class ScratchRegisterBits: public BitField<int, 8, 4> {};

  Major MajorKey() { return WriteInt32ToHeapNumber; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return IntRegisterBits::encode(the_int_.code())
           | HeapNumberRegisterBits::encode(the_heap_number_.code())
           | ScratchRegisterBits::encode(scratch_.code());
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "WriteInt32ToHeapNumberStub"; }

#ifdef DEBUG
  void Print() { PrintF("WriteInt32ToHeapNumberStub\n"); }
#endif
};


class NumberToStringStub: public CodeStub {
 public:
  NumberToStringStub() { }

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  static void GenerateLookupNumberStringCache(MacroAssembler* masm,
                                              Register object,
                                              Register result,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              bool object_is_smi,
                                              Label* not_found);

 private:
  Major MajorKey() { return NumberToString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "NumberToStringStub"; }
};


// Enter C code from generated RegExp code in a way that allows
// the C code to fix the return address in case of a GC.
// Currently only needed on ARM.
class RegExpCEntryStub: public CodeStub {
 public:
  RegExpCEntryStub() {}
  virtual ~RegExpCEntryStub() {}
  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return RegExpCEntry; }
  int MinorKey() { return 0; }
  const char* GetName() { return "RegExpCEntryStub"; }
};


// Trampoline stub to call into native code. To call safely into native code
// in the presence of compacting GC (which can move code objects) we need to
// keep the code which called into native pinned in the memory. Currently the
// simplest approach is to generate such stub early enough so it can never be
// moved by GC
class DirectCEntryStub: public CodeStub {
 public:
  DirectCEntryStub() {}
  void Generate(MacroAssembler* masm);
  void GenerateCall(MacroAssembler* masm, ExternalReference function);
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  Major MajorKey() { return DirectCEntry; }
  int MinorKey() { return 0; }
  const char* GetName() { return "DirectCEntryStub"; }
};


// Generate code to load an element from a pixel array. The receiver is assumed
// to not be a smi and to have elements, the caller must guarantee this
// precondition. If key is not a smi, then the generated code branches to
// key_not_smi. Callers can specify NULL for key_not_smi to signal that a smi
// check has already been performed on key so that the smi check is not
// generated. If key is not a valid index within the bounds of the pixel array,
// the generated code jumps to out_of_range. receiver, key and elements are
// unchanged throughout the generated code sequence.
void GenerateFastPixelArrayLoad(MacroAssembler* masm,
                                Register receiver,
                                Register key,
                                Register elements_map,
                                Register elements,
                                Register scratch1,
                                Register scratch2,
                                Register result,
                                Label* not_pixel_array,
                                Label* key_not_smi,
                                Label* out_of_range);

// Generate code to store an element into a pixel array, clamping values between
// [0..255]. The receiver is assumed to not be a smi and to have elements, the
// caller must guarantee this precondition. If key is not a smi, then the
// generated code branches to key_not_smi. Callers can specify NULL for
// key_not_smi to signal that a smi check has already been performed on key so
// that the smi check is not generated. If value is not a smi, the generated
// code will branch to value_not_smi.  If the receiver doesn't have pixel array
// elements, the generated code will branch to not_pixel_array, unless
// not_pixel_array is NULL, in which case the caller must ensure that the
// receiver has pixel array elements. If key is not a valid index within the
// bounds of the pixel array, the generated code jumps to out_of_range. If
// load_elements_from_receiver is true, then the elements of receiver is loaded
// into elements, otherwise elements is assumed to already be the receiver's
// elements. If load_elements_map_from_elements is true, elements_map is loaded
// from elements, otherwise it is assumed to already contain the element map.
void GenerateFastPixelArrayStore(MacroAssembler* masm,
                                 Register receiver,
                                 Register key,
                                 Register value,
                                 Register elements,
                                 Register elements_map,
                                 Register scratch1,
                                 Register scratch2,
                                 bool load_elements_from_receiver,
                                 bool load_elements_map_from_elements,
                                 Label* key_not_smi,
                                 Label* value_not_smi,
                                 Label* not_pixel_array,
                                 Label* out_of_range);

} }  // namespace v8::internal

#endif  // V8_ARM_CODE_STUBS_ARM_H_
