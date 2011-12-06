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

#ifndef V8_ARM_CODE_STUBS_ARM_H_
#define V8_ARM_CODE_STUBS_ARM_H_

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


class StoreBufferOverflowStub: public CodeStub {
 public:
  explicit StoreBufferOverflowStub(SaveFPRegsMode save_fp)
      : save_doubles_(save_fp) { }

  void Generate(MacroAssembler* masm);

  virtual bool IsPregenerated();
  static void GenerateFixedRegStubsAheadOfTime();
  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  SaveFPRegsMode save_doubles_;

  Major MajorKey() { return StoreBufferOverflow; }
  int MinorKey() { return (save_doubles_ == kSaveFPRegs) ? 1 : 0; }
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

  virtual void FinishCode(Handle<Code> code) {
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
    use_vfp3_ = CpuFeatures::IsSupported(VFP3);
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  BinaryOpStub(
      int key,
      BinaryOpIC::TypeInfo operands_type,
      BinaryOpIC::TypeInfo result_type = BinaryOpIC::UNINITIALIZED)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        use_vfp3_(VFP3Bits::decode(key)),
        operands_type_(operands_type),
        result_type_(result_type) { }

 private:
  enum SmiCodeGenerateHeapNumberResults {
    ALLOW_HEAPNUMBER_RESULTS,
    NO_HEAPNUMBER_RESULTS
  };

  Token::Value op_;
  OverwriteMode mode_;
  bool use_vfp3_;

  // Operand type information determined at runtime.
  BinaryOpIC::TypeInfo operands_type_;
  BinaryOpIC::TypeInfo result_type_;

  virtual void PrintName(StringStream* stream);

  // Minor key encoding in 16 bits RRRTTTVOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 7> {};
  class VFP3Bits: public BitField<bool, 9, 1> {};
  class OperandTypeInfoBits: public BitField<BinaryOpIC::TypeInfo, 10, 3> {};
  class ResultTypeInfoBits: public BitField<BinaryOpIC::TypeInfo, 13, 3> {};

  Major MajorKey() { return BinaryOp; }
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

  virtual void FinishCode(Handle<Code> code) {
    code->set_binary_op_type(operands_type_);
    code->set_binary_op_result_type(result_type_);
  }

  friend class CodeGenerator;
};


class StringHelper : public AllStatic {
 public:
  // Generate code for copying characters using a simple loop. This should only
  // be used in places where the number of characters is small and the
  // additional setup and checking in GenerateCopyCharactersLong adds too much
  // overhead. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     Register scratch,
                                     bool ascii);

  // Generate code for copying a large number of characters. This function
  // is allowed to spend extra time setting up conditions to make copying
  // faster. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  static void GenerateCopyCharactersLong(MacroAssembler* masm,
                                         Register dest,
                                         Register src,
                                         Register count,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Register scratch4,
                                         Register scratch5,
                                         int flags);


  // Probe the symbol table for a two character string. If the string is
  // not found by probing a jump to the label not_found is performed. This jump
  // does not guarantee that the string is not in the symbol table. If the
  // string is found the code falls through with the string in register r0.
  // Contents of both c1 and c2 registers are modified. At the exit c1 is
  // guaranteed to contain halfword with low and high bytes equal to
  // initial contents of c1 and c2 respectively.
  static void GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                   Register c1,
                                                   Register c2,
                                                   Register scratch1,
                                                   Register scratch2,
                                                   Register scratch3,
                                                   Register scratch4,
                                                   Register scratch5,
                                                   Label* not_found);

  // Generate string hash.
  static void GenerateHashInit(MacroAssembler* masm,
                               Register hash,
                               Register character);

  static void GenerateHashAddCharacter(MacroAssembler* masm,
                                       Register hash,
                                       Register character);

  static void GenerateHashGetHash(MacroAssembler* masm,
                                  Register hash);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
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

  // Compares two flat ASCII strings and returns result in r0.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

  // Compares two flat ASCII strings for equality and returns result
  // in r0.
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
                                            Label* chars_not_equal);
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

  bool IsPregenerated();
  static void GenerateFixedRegStubsAheadOfTime();

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


class RecordWriteStub: public CodeStub {
 public:
  RecordWriteStub(Register object,
                  Register value,
                  Register address,
                  RememberedSetAction remembered_set_action,
                  SaveFPRegsMode fp_mode)
      : object_(object),
        value_(value),
        address_(address),
        remembered_set_action_(remembered_set_action),
        save_fp_regs_mode_(fp_mode),
        regs_(object,   // An input reg.
              address,  // An input reg.
              value) {  // One scratch reg.
  }

  enum Mode {
    STORE_BUFFER_ONLY,
    INCREMENTAL,
    INCREMENTAL_COMPACTION
  };

  virtual bool IsPregenerated();
  static void GenerateFixedRegStubsAheadOfTime();
  virtual bool SometimesSetsUpAFrame() { return false; }

  static void PatchBranchIntoNop(MacroAssembler* masm, int pos) {
    masm->instr_at_put(pos, (masm->instr_at(pos) & ~B27) | (B24 | B20));
    ASSERT(Assembler::IsTstImmediate(masm->instr_at(pos)));
  }

  static void PatchNopIntoBranch(MacroAssembler* masm, int pos) {
    masm->instr_at_put(pos, (masm->instr_at(pos) & ~(B24 | B20)) | B27);
    ASSERT(Assembler::IsBranch(masm->instr_at(pos)));
  }

  static Mode GetMode(Code* stub) {
    Instr first_instruction = Assembler::instr_at(stub->instruction_start());
    Instr second_instruction = Assembler::instr_at(stub->instruction_start() +
                                                   Assembler::kInstrSize);

    if (Assembler::IsBranch(first_instruction)) {
      return INCREMENTAL;
    }

    ASSERT(Assembler::IsTstImmediate(first_instruction));

    if (Assembler::IsBranch(second_instruction)) {
      return INCREMENTAL_COMPACTION;
    }

    ASSERT(Assembler::IsTstImmediate(second_instruction));

    return STORE_BUFFER_ONLY;
  }

  static void Patch(Code* stub, Mode mode) {
    MacroAssembler masm(NULL,
                        stub->instruction_start(),
                        stub->instruction_size());
    switch (mode) {
      case STORE_BUFFER_ONLY:
        ASSERT(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);
        PatchBranchIntoNop(&masm, 0);
        PatchBranchIntoNop(&masm, Assembler::kInstrSize);
        break;
      case INCREMENTAL:
        ASSERT(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchNopIntoBranch(&masm, 0);
        break;
      case INCREMENTAL_COMPACTION:
        ASSERT(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchNopIntoBranch(&masm, Assembler::kInstrSize);
        break;
    }
    ASSERT(GetMode(stub) == mode);
    CPU::FlushICache(stub->instruction_start(), 2 * Assembler::kInstrSize);
  }

 private:
  // This is a helper class for freeing up 3 scratch registers.  The input is
  // two registers that must be preserved and one scratch register provided by
  // the caller.
  class RegisterAllocation {
   public:
    RegisterAllocation(Register object,
                       Register address,
                       Register scratch0)
        : object_(object),
          address_(address),
          scratch0_(scratch0) {
      ASSERT(!AreAliased(scratch0, object, address, no_reg));
      scratch1_ = GetRegThatIsNotOneOf(object_, address_, scratch0_);
    }

    void Save(MacroAssembler* masm) {
      ASSERT(!AreAliased(object_, address_, scratch1_, scratch0_));
      // We don't have to save scratch0_ because it was given to us as
      // a scratch register.
      masm->push(scratch1_);
    }

    void Restore(MacroAssembler* masm) {
      masm->pop(scratch1_);
    }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.  The scratch registers
    // will be restored by other means so we don't bother pushing them here.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      masm->stm(db_w, sp, (kCallerSaved | lr.bit()) & ~scratch1_.bit());
      if (mode == kSaveFPRegs) {
        CpuFeatures::Scope scope(VFP3);
        masm->sub(sp,
                  sp,
                  Operand(kDoubleSize * (DwVfpRegister::kNumRegisters - 1)));
        // Save all VFP registers except d0.
        for (int i = DwVfpRegister::kNumRegisters - 1; i > 0; i--) {
          DwVfpRegister reg = DwVfpRegister::from_code(i);
          masm->vstr(reg, MemOperand(sp, (i - 1) * kDoubleSize));
        }
      }
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler*masm,
                                           SaveFPRegsMode mode) {
      if (mode == kSaveFPRegs) {
        CpuFeatures::Scope scope(VFP3);
        // Restore all VFP registers except d0.
        for (int i = DwVfpRegister::kNumRegisters - 1; i > 0; i--) {
          DwVfpRegister reg = DwVfpRegister::from_code(i);
          masm->vldr(reg, MemOperand(sp, (i - 1) * kDoubleSize));
        }
        masm->add(sp,
                  sp,
                  Operand(kDoubleSize * (DwVfpRegister::kNumRegisters - 1)));
      }
      masm->ldm(ia_w, sp, (kCallerSaved | lr.bit()) & ~scratch1_.bit());
    }

    inline Register object() { return object_; }
    inline Register address() { return address_; }
    inline Register scratch0() { return scratch0_; }
    inline Register scratch1() { return scratch1_; }

   private:
    Register object_;
    Register address_;
    Register scratch0_;
    Register scratch1_;

    Register GetRegThatIsNotOneOf(Register r1,
                                  Register r2,
                                  Register r3) {
      for (int i = 0; i < Register::kNumAllocatableRegisters; i++) {
        Register candidate = Register::FromAllocationIndex(i);
        if (candidate.is(r1)) continue;
        if (candidate.is(r2)) continue;
        if (candidate.is(r3)) continue;
        return candidate;
      }
      UNREACHABLE();
      return no_reg;
    }
    friend class RecordWriteStub;
  };

  enum OnNoNeedToInformIncrementalMarker {
    kReturnOnNoNeedToInformIncrementalMarker,
    kUpdateRememberedSetOnNoNeedToInformIncrementalMarker
  };

  void Generate(MacroAssembler* masm);
  void GenerateIncremental(MacroAssembler* masm, Mode mode);
  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm, Mode mode);

  Major MajorKey() { return RecordWrite; }

  int MinorKey() {
    return ObjectBits::encode(object_.code()) |
        ValueBits::encode(value_.code()) |
        AddressBits::encode(address_.code()) |
        RememberedSetActionBits::encode(remembered_set_action_) |
        SaveFPRegsModeBits::encode(save_fp_regs_mode_);
  }

  void Activate(Code* code) {
    code->GetHeap()->incremental_marking()->ActivateGeneratedStub(code);
  }

  class ObjectBits: public BitField<int, 0, 4> {};
  class ValueBits: public BitField<int, 4, 4> {};
  class AddressBits: public BitField<int, 8, 4> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 12, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 13, 1> {};

  Register object_;
  Register value_;
  Register address_;
  RememberedSetAction remembered_set_action_;
  SaveFPRegsMode save_fp_regs_mode_;
  Label slow_;
  RegisterAllocation regs_;
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
  void GenerateCall(MacroAssembler* masm, ExternalReference function);
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  Major MajorKey() { return DirectCEntry; }
  int MinorKey() { return 0; }

  bool NeedsImmovableCode() { return true; }
};


class FloatingPointHelper : public AllStatic {
 public:
  enum Destination {
    kVFPRegisters,
    kCoreRegisters
  };


  // Loads smis from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will be scratched.
  static void LoadSmis(MacroAssembler* masm,
                       Destination destination,
                       Register scratch1,
                       Register scratch2);

  // Loads objects from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will still be scratched. If
  // either r0 or r1 is not a number (not smi and not heap number object) the
  // not_number label is jumped to with r0 and r1 intact.
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
                                   DwVfpRegister double_scratch,
                                   Label* not_int32);

  // Converts the integer (untagged smi) in |int_scratch| to a double, storing
  // the result either in |double_dst| or |dst2:dst1|, depending on
  // |destination|.
  // Warning: The value in |int_scratch| will be changed in the process!
  static void ConvertIntToDouble(MacroAssembler* masm,
                                 Register int_scratch,
                                 Destination destination,
                                 DwVfpRegister double_dst,
                                 Register dst1,
                                 Register dst2,
                                 Register scratch2,
                                 SwVfpRegister single_scratch);

  // Load the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  static void LoadNumberAsInt32Double(MacroAssembler* masm,
                                      Register object,
                                      Destination destination,
                                      DwVfpRegister double_dst,
                                      Register dst1,
                                      Register dst2,
                                      Register heap_number_map,
                                      Register scratch1,
                                      Register scratch2,
                                      SwVfpRegister single_scratch,
                                      Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  // scratch3 is not used when VFP3 is supported.
  static void LoadNumberAsInt32(MacroAssembler* masm,
                                Register object,
                                Register dst,
                                Register heap_number_map,
                                Register scratch1,
                                Register scratch2,
                                Register scratch3,
                                DwVfpRegister double_scratch,
                                Label* not_int32);

  // Generate non VFP3 code to check if a double can be exactly represented by a
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
  // registers. (Used when VFP3 is not supported.)
  // This code never falls through, but returns with a heap number containing
  // the result in r0.
  // Register heapnumber_result must be a heap number in which the
  // result of the operation will be stored.
  // Requires the following layout on entry:
  // r0: Left value (least significant part of mantissa).
  // r1: Left value (sign, exponent, top of mantissa).
  // r2: Right value (least significant part of mantissa).
  // r3: Right value (sign, exponent, top of mantissa).
  static void CallCCodeForDoubleOperation(MacroAssembler* masm,
                                          Token::Value op,
                                          Register heap_number_result,
                                          Register scratch);

 private:
  static void LoadNumber(MacroAssembler* masm,
                         FloatingPointHelper::Destination destination,
                         Register object,
                         DwVfpRegister dst,
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

  static void GenerateNegativeLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register receiver,
                                     Register properties,
                                     Handle<String> name,
                                     Register scratch0);

  static void GeneratePositiveLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register elements,
                                     Register name,
                                     Register r0,
                                     Register r1);

  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  static const int kInlinedProbes = 4;
  static const int kTotalProbes = 20;

  static const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;

  Major MajorKey() { return StringDictionaryLookup; }

  int MinorKey() {
    return LookupModeBits::encode(mode_);
  }

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  LookupMode mode_;
};


} }  // namespace v8::internal

#endif  // V8_ARM_CODE_STUBS_ARM_H_
