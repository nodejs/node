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

#ifndef V8_MIPS_CODE_STUBS_ARM_H_
#define V8_MIPS_CODE_STUBS_ARM_H_

#include "ic-inl.h"


namespace v8 {
namespace internal {


// Compute a transcendental math function natively, or call the
// TranscendentalCache runtime function.
class TranscendentalCacheStub: public CodeStub {
 public:
  enum ArgumentType {
    TAGGED = 0 << TranscendentalCache::kTranscendentalTypeBits,
    UNTAGGED = 1 << TranscendentalCache::kTranscendentalTypeBits
  };

  TranscendentalCacheStub(TranscendentalCache::Type type,
                          ArgumentType argument_type)
      : type_(type), argument_type_(argument_type) { }
  void Generate(MacroAssembler* masm);
 private:
  TranscendentalCache::Type type_;
  ArgumentType argument_type_;
  void GenerateCallCFunction(MacroAssembler* masm, Register scratch);

  Major MajorKey() { return TranscendentalCache; }
  int MinorKey() { return type_ | argument_type_; }
  Runtime::FunctionId RuntimeFunction();
};


class UnaryOpStub: public CodeStub {
 public:
  UnaryOpStub(Token::Value op,
              UnaryOverwriteMode mode,
              UnaryOpIC::TypeInfo operand_type = UnaryOpIC::UNINITIALIZED)
      : op_(op),
        mode_(mode),
        operand_type_(operand_type) {
  }

 private:
  Token::Value op_;
  UnaryOverwriteMode mode_;

  // Operand type information determined at runtime.
  UnaryOpIC::TypeInfo operand_type_;

  virtual void PrintName(StringStream* stream);

  class ModeBits: public BitField<UnaryOverwriteMode, 0, 1> {};
  class OpBits: public BitField<Token::Value, 1, 7> {};
  class OperandTypeInfoBits: public BitField<UnaryOpIC::TypeInfo, 8, 3> {};

  Major MajorKey() { return UnaryOp; }
  int MinorKey() {
    return ModeBits::encode(mode_)
           | OpBits::encode(op_)
           | OperandTypeInfoBits::encode(operand_type_);
  }

  // Note: A lot of the helper functions below will vanish when we use virtual
  // function instead of switch more often.
  void Generate(MacroAssembler* masm);

  void GenerateTypeTransition(MacroAssembler* masm);

  void GenerateSmiStub(MacroAssembler* masm);
  void GenerateSmiStubSub(MacroAssembler* masm);
  void GenerateSmiStubBitNot(MacroAssembler* masm);
  void GenerateSmiCodeSub(MacroAssembler* masm, Label* non_smi, Label* slow);
  void GenerateSmiCodeBitNot(MacroAssembler* masm, Label* slow);

  void GenerateHeapNumberStub(MacroAssembler* masm);
  void GenerateHeapNumberStubSub(MacroAssembler* masm);
  void GenerateHeapNumberStubBitNot(MacroAssembler* masm);
  void GenerateHeapNumberCodeSub(MacroAssembler* masm, Label* slow);
  void GenerateHeapNumberCodeBitNot(MacroAssembler* masm, Label* slow);

  void GenerateGenericStub(MacroAssembler* masm);
  void GenerateGenericStubSub(MacroAssembler* masm);
  void GenerateGenericStubBitNot(MacroAssembler* masm);
  void GenerateGenericCodeFallback(MacroAssembler* masm);

  virtual int GetCodeKind() { return Code::UNARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return UnaryOpIC::ToState(operand_type_);
  }

  virtual void FinishCode(Code* code) {
    code->set_unary_op_type(operand_type_);
  }
};


class BinaryOpStub: public CodeStub {
 public:
  BinaryOpStub(Token::Value op, OverwriteMode mode)
      : op_(op),
        mode_(mode),
        operands_type_(BinaryOpIC::UNINITIALIZED),
        result_type_(BinaryOpIC::UNINITIALIZED) {
    use_fpu_ = CpuFeatures::IsSupported(FPU);
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  BinaryOpStub(
      int key,
      BinaryOpIC::TypeInfo operands_type,
      BinaryOpIC::TypeInfo result_type = BinaryOpIC::UNINITIALIZED)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        use_fpu_(FPUBits::decode(key)),
        operands_type_(operands_type),
        result_type_(result_type) { }

 private:
  enum SmiCodeGenerateHeapNumberResults {
    ALLOW_HEAPNUMBER_RESULTS,
    NO_HEAPNUMBER_RESULTS
  };

  Token::Value op_;
  OverwriteMode mode_;
  bool use_fpu_;

  // Operand type information determined at runtime.
  BinaryOpIC::TypeInfo operands_type_;
  BinaryOpIC::TypeInfo result_type_;

  virtual void PrintName(StringStream* stream);

  // Minor key encoding in 16 bits RRRTTTVOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 7> {};
  class FPUBits: public BitField<bool, 9, 1> {};
  class OperandTypeInfoBits: public BitField<BinaryOpIC::TypeInfo, 10, 3> {};
  class ResultTypeInfoBits: public BitField<BinaryOpIC::TypeInfo, 13, 3> {};

  Major MajorKey() { return BinaryOp; }
  int MinorKey() {
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | FPUBits::encode(use_fpu_)
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
                       Label* use_runtime,
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
  void GenerateBothStringStub(MacroAssembler* masm);
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

  virtual int GetCodeKind() { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(operands_type_);
  }

  virtual void FinishCode(Code* code) {
    code->set_binary_op_type(operands_type_);
    code->set_binary_op_result_type(result_type_);
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

  // Compare two flat ASCII strings and returns result in v0.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

  // Compares two flat ASCII strings for equality and returns result
  // in v0.
  static void GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3);

 private:
  virtual Major MajorKey() { return StringCompare; }
  virtual int MinorKey() { return 0; }
  virtual void Generate(MacroAssembler* masm);

  static void GenerateAsciiCharsCompareLoop(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register length,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3,
                                            Label* chars_not_equal);
};


// This stub can convert a signed int32 to a heap number (double).  It does
// not work for int32s that are in Smi range!  No GC occurs during this stub
// so you don't have to set up the frame.
class WriteInt32ToHeapNumberStub : public CodeStub {
 public:
  WriteInt32ToHeapNumberStub(Register the_int,
                             Register the_heap_number,
                             Register scratch,
                             Register scratch2)
      : the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch),
        sign_(scratch2) { }

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;
  Register sign_;

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
};


// Enter C code from generated RegExp code in a way that allows
// the C code to fix the return address in case of a GC.
// Currently only needed on ARM and MIPS.
class RegExpCEntryStub: public CodeStub {
 public:
  RegExpCEntryStub() {}
  virtual ~RegExpCEntryStub() {}
  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return RegExpCEntry; }
  int MinorKey() { return 0; }

  bool NeedsImmovableCode() { return true; }
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
  void GenerateCall(MacroAssembler* masm,
                                ExternalReference function);
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  Major MajorKey() { return DirectCEntry; }
  int MinorKey() { return 0; }

  bool NeedsImmovableCode() { return true; }
};

class FloatingPointHelper : public AllStatic {
 public:
  enum Destination {
    kFPURegisters,
    kCoreRegisters
  };


  // Loads smis from a0 and a1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either f14 and f12 or in a2/a3 and a0/a1 respectively. If the destination
  // is floating point registers FPU must be supported. If core registers are
  // requested when FPU is supported f12 and f14 will be scratched.
  static void LoadSmis(MacroAssembler* masm,
                       Destination destination,
                       Register scratch1,
                       Register scratch2);

  // Loads objects from a0 and a1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either f14 and f12 or in a2/a3 and a0/a1 respectively. If the destination
  // is floating point registers FPU must be supported. If core registers are
  // requested when FPU is supported f12 and f14 will still be scratched. If
  // either a0 or a1 is not a number (not smi and not heap number object) the
  // not_number label is jumped to with a0 and a1 intact.
  static void LoadOperands(MacroAssembler* masm,
                           FloatingPointHelper::Destination destination,
                           Register heap_number_map,
                           Register scratch1,
                           Register scratch2,
                           Label* not_number);

  // Convert the smi or heap number in object to an int32 using the rules
  // for ToInt32 as described in ECMAScript 9.5.: the value is truncated
  // and brought into the range -2^31 .. +2^31 - 1.
  static void ConvertNumberToInt32(MacroAssembler* masm,
                                   Register object,
                                   Register dst,
                                   Register heap_number_map,
                                   Register scratch1,
                                   Register scratch2,
                                   Register scratch3,
                                   FPURegister double_scratch,
                                   Label* not_int32);

  // Converts the integer (untagged smi) in |int_scratch| to a double, storing
  // the result either in |double_dst| or |dst2:dst1|, depending on
  // |destination|.
  // Warning: The value in |int_scratch| will be changed in the process!
  static void ConvertIntToDouble(MacroAssembler* masm,
                                 Register int_scratch,
                                 Destination destination,
                                 FPURegister double_dst,
                                 Register dst1,
                                 Register dst2,
                                 Register scratch2,
                                 FPURegister single_scratch);

  // Load the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  static void LoadNumberAsInt32Double(MacroAssembler* masm,
                                      Register object,
                                      Destination destination,
                                      FPURegister double_dst,
                                      Register dst1,
                                      Register dst2,
                                      Register heap_number_map,
                                      Register scratch1,
                                      Register scratch2,
                                      FPURegister single_scratch,
                                      Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  // scratch3 is not used when FPU is supported.
  static void LoadNumberAsInt32(MacroAssembler* masm,
                                Register object,
                                Register dst,
                                Register heap_number_map,
                                Register scratch1,
                                Register scratch2,
                                Register scratch3,
                                FPURegister double_scratch,
                                Label* not_int32);

  // Generate non FPU code to check if a double can be exactly represented by a
  // 32-bit integer. This does not check for 0 or -0, which need
  // to be checked for separately.
  // Control jumps to not_int32 if the value is not a 32-bit integer, and falls
  // through otherwise.
  // src1 and src2 will be cloberred.
  //
  // Expected input:
  // - src1: higher (exponent) part of the double value.
  // - src2: lower (mantissa) part of the double value.
  // Output status:
  // - dst: 32 higher bits of the mantissa. (mantissa[51:20])
  // - src2: contains 1.
  // - other registers are clobbered.
  static void DoubleIs32BitInteger(MacroAssembler* masm,
                                   Register src1,
                                   Register src2,
                                   Register dst,
                                   Register scratch,
                                   Label* not_int32);

  // Generates code to call a C function to do a double operation using core
  // registers. (Used when FPU is not supported.)
  // This code never falls through, but returns with a heap number containing
  // the result in v0.
  // Register heapnumber_result must be a heap number in which the
  // result of the operation will be stored.
  // Requires the following layout on entry:
  // a0: Left value (least significant part of mantissa).
  // a1: Left value (sign, exponent, top of mantissa).
  // a2: Right value (least significant part of mantissa).
  // a3: Right value (sign, exponent, top of mantissa).
  static void CallCCodeForDoubleOperation(MacroAssembler* masm,
                                          Token::Value op,
                                          Register heap_number_result,
                                          Register scratch);

 private:
  static void LoadNumber(MacroAssembler* masm,
                         FloatingPointHelper::Destination destination,
                         Register object,
                         FPURegister dst,
                         Register dst1,
                         Register dst2,
                         Register heap_number_map,
                         Register scratch1,
                         Register scratch2,
                         Label* not_number);
};


class StringDictionaryLookupStub: public CodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  explicit StringDictionaryLookupStub(LookupMode mode) : mode_(mode) { }

  void Generate(MacroAssembler* masm);

  MUST_USE_RESULT static MaybeObject* GenerateNegativeLookup(
      MacroAssembler* masm,
      Label* miss,
      Label* done,
      Register receiver,
      Register properties,
      String* name,
      Register scratch0);

  static void GeneratePositiveLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register elements,
                                     Register name,
                                     Register r0,
                                     Register r1);

 private:
  static const int kInlinedProbes = 4;
  static const int kTotalProbes = 20;

  static const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;

  Major MajorKey() { return StringDictionaryNegativeLookup; }

  int MinorKey() {
    return LookupModeBits::encode(mode_);
  }

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  LookupMode mode_;
};


} }  // namespace v8::internal

#endif  // V8_MIPS_CODE_STUBS_ARM_H_
