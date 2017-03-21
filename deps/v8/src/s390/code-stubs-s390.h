// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_S390_CODE_STUBS_S390_H_
#define V8_S390_CODE_STUBS_S390_H_

#include "src/s390/frames-s390.h"

namespace v8 {
namespace internal {

void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);

class StringHelper : public AllStatic {
 public:
  // Compares two flat one-byte strings and returns result in r0.
  static void GenerateCompareFlatOneByteStrings(MacroAssembler* masm,
                                                Register left, Register right,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3);

  // Compares two flat one-byte strings for equality and returns result in r0.
  static void GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                              Register left, Register right,
                                              Register scratch1,
                                              Register scratch2);

 private:
  static void GenerateOneByteCharsCompareLoop(MacroAssembler* masm,
                                              Register left, Register right,
                                              Register length,
                                              Register scratch1,
                                              Label* chars_not_equal);

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};

class StoreRegistersStateStub : public PlatformCodeStub {
 public:
  explicit StoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(StoreRegistersState, PlatformCodeStub);
};

class RestoreRegistersStateStub : public PlatformCodeStub {
 public:
  explicit RestoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(RestoreRegistersState, PlatformCodeStub);
};

class RecordWriteStub : public PlatformCodeStub {
 public:
  RecordWriteStub(Isolate* isolate, Register object, Register value,
                  Register address, RememberedSetAction remembered_set_action,
                  SaveFPRegsMode fp_mode)
      : PlatformCodeStub(isolate),
        regs_(object,   // An input reg.
              address,  // An input reg.
              value) {  // One scratch reg.
    minor_key_ = ObjectBits::encode(object.code()) |
                 ValueBits::encode(value.code()) |
                 AddressBits::encode(address.code()) |
                 RememberedSetActionBits::encode(remembered_set_action) |
                 SaveFPRegsModeBits::encode(fp_mode);
  }

  RecordWriteStub(uint32_t key, Isolate* isolate)
      : PlatformCodeStub(key, isolate), regs_(object(), address(), value()) {}

  enum Mode { STORE_BUFFER_ONLY, INCREMENTAL, INCREMENTAL_COMPACTION };

  bool SometimesSetsUpAFrame() override { return false; }

  // Patch an always taken branch into a NOP branch
  static void PatchBranchCondMask(MacroAssembler* masm, int pos, Condition c) {
    int32_t instrLen = masm->instr_length_at(pos);
    DCHECK(instrLen == 4 || instrLen == 6);

    if (instrLen == 4) {
      // BRC - Branch Mask @ Bits 23-20
      FourByteInstr updatedMask = static_cast<FourByteInstr>(c) << 20;
      masm->instr_at_put<FourByteInstr>(
          pos, (masm->instr_at(pos) & ~kFourByteBrCondMask) | updatedMask);
    } else {
      // BRCL - Branch Mask @ Bits 39-36
      SixByteInstr updatedMask = static_cast<SixByteInstr>(c) << 36;
      masm->instr_at_put<SixByteInstr>(
          pos, (masm->instr_at(pos) & ~kSixByteBrCondMask) | updatedMask);
    }
  }

  static bool isBranchNop(SixByteInstr instr, int instrLength) {
    if ((4 == instrLength && 0 == (instr & kFourByteBrCondMask)) ||
        // BRC - Check for 0x0 mask condition.
        (6 == instrLength && 0 == (instr & kSixByteBrCondMask))) {
      // BRCL - Check for 0x0 mask condition
      return true;
    }
    return false;
  }

  static Mode GetMode(Code* stub) {
    int32_t first_instr_length =
        Instruction::InstructionLength(stub->instruction_start());
    int32_t second_instr_length = Instruction::InstructionLength(
        stub->instruction_start() + first_instr_length);

    uint64_t first_instr = Assembler::instr_at(stub->instruction_start());
    uint64_t second_instr =
        Assembler::instr_at(stub->instruction_start() + first_instr_length);

    DCHECK(first_instr_length == 4 || first_instr_length == 6);
    DCHECK(second_instr_length == 4 || second_instr_length == 6);

    bool isFirstInstrNOP = isBranchNop(first_instr, first_instr_length);
    bool isSecondInstrNOP = isBranchNop(second_instr, second_instr_length);

    // STORE_BUFFER_ONLY has NOP on both branches
    if (isSecondInstrNOP && isFirstInstrNOP) return STORE_BUFFER_ONLY;
    // INCREMENTAL_COMPACTION has NOP on second branch.
    else if (isFirstInstrNOP && !isSecondInstrNOP)
      return INCREMENTAL_COMPACTION;
    // INCREMENTAL has NOP on first branch.
    else if (!isFirstInstrNOP && isSecondInstrNOP)
      return INCREMENTAL;

    DCHECK(false);
    return STORE_BUFFER_ONLY;
  }

  static void Patch(Code* stub, Mode mode) {
    MacroAssembler masm(stub->GetIsolate(), stub->instruction_start(),
                        stub->instruction_size(), CodeObjectRequired::kNo);

    // Get instruction lengths of two branches
    int32_t first_instr_length = masm.instr_length_at(0);
    int32_t second_instr_length = masm.instr_length_at(first_instr_length);

    switch (mode) {
      case STORE_BUFFER_ONLY:
        DCHECK(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);

        PatchBranchCondMask(&masm, 0, CC_NOP);
        PatchBranchCondMask(&masm, first_instr_length, CC_NOP);
        break;
      case INCREMENTAL:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchBranchCondMask(&masm, 0, CC_ALWAYS);
        break;
      case INCREMENTAL_COMPACTION:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchBranchCondMask(&masm, first_instr_length, CC_ALWAYS);
        break;
    }
    DCHECK(GetMode(stub) == mode);
    Assembler::FlushICache(stub->GetIsolate(), stub->instruction_start(),
                           first_instr_length + second_instr_length);
  }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();

 private:
  // This is a helper class for freeing up 3 scratch registers.  The input is
  // two registers that must be preserved and one scratch register provided by
  // the caller.
  class RegisterAllocation {
   public:
    RegisterAllocation(Register object, Register address, Register scratch0)
        : object_(object), address_(address), scratch0_(scratch0) {
      DCHECK(!AreAliased(scratch0, object, address, no_reg));
      scratch1_ = GetRegisterThatIsNotOneOf(object_, address_, scratch0_);
    }

    void Save(MacroAssembler* masm) {
      DCHECK(!AreAliased(object_, address_, scratch1_, scratch0_));
      // We don't have to save scratch0_ because it was given to us as
      // a scratch register.
      masm->push(scratch1_);
    }

    void Restore(MacroAssembler* masm) { masm->pop(scratch1_); }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.  The scratch registers
    // will be restored by other means so we don't bother pushing them here.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      masm->push(r14);
      masm->MultiPush(kJSCallerSaved & ~scratch1_.bit());
      if (mode == kSaveFPRegs) {
        // Save all volatile FP registers except d0.
        masm->MultiPushDoubles(kCallerSavedDoubles & ~d0.bit());
      }
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler* masm,
                                           SaveFPRegsMode mode) {
      if (mode == kSaveFPRegs) {
        // Restore all volatile FP registers except d0.
        masm->MultiPopDoubles(kCallerSavedDoubles & ~d0.bit());
      }
      masm->MultiPop(kJSCallerSaved & ~scratch1_.bit());
      masm->pop(r14);
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

    friend class RecordWriteStub;
  };

  enum OnNoNeedToInformIncrementalMarker {
    kReturnOnNoNeedToInformIncrementalMarker,
    kUpdateRememberedSetOnNoNeedToInformIncrementalMarker
  };

  inline Major MajorKey() const final { return RecordWrite; }

  void Generate(MacroAssembler* masm) override;
  void GenerateIncremental(MacroAssembler* masm, Mode mode);
  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm, OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm);

  void Activate(Code* code) override {
    code->GetHeap()->incremental_marking()->ActivateGeneratedStub(code);
  }

  Register object() const {
    return Register::from_code(ObjectBits::decode(minor_key_));
  }

  Register value() const {
    return Register::from_code(ValueBits::decode(minor_key_));
  }

  Register address() const {
    return Register::from_code(AddressBits::decode(minor_key_));
  }

  RememberedSetAction remembered_set_action() const {
    return RememberedSetActionBits::decode(minor_key_);
  }

  SaveFPRegsMode save_fp_regs_mode() const {
    return SaveFPRegsModeBits::decode(minor_key_);
  }

  class ObjectBits : public BitField<int, 0, 4> {};
  class ValueBits : public BitField<int, 4, 4> {};
  class AddressBits : public BitField<int, 8, 4> {};
  class RememberedSetActionBits : public BitField<RememberedSetAction, 15, 1> {
  };
  class SaveFPRegsModeBits : public BitField<SaveFPRegsMode, 16, 1> {};

  Label slow_;
  RegisterAllocation regs_;

  DISALLOW_COPY_AND_ASSIGN(RecordWriteStub);
};

// Trampoline stub to call into native code. To call safely into native code
// in the presence of compacting GC (which can move code objects) we need to
// keep the code which called into native pinned in the memory. Currently the
// simplest approach is to generate such stub early enough so it can never be
// moved by GC
class DirectCEntryStub : public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  bool NeedsImmovableCode() override { return true; }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DirectCEntry, PlatformCodeStub);
};

class NameDictionaryLookupStub : public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Isolate* isolate, LookupMode mode)
      : PlatformCodeStub(isolate) {
    minor_key_ = LookupModeBits::encode(mode);
  }

  static void GenerateNegativeLookup(MacroAssembler* masm, Label* miss,
                                     Label* done, Register receiver,
                                     Register properties, Handle<Name> name,
                                     Register scratch0);

  bool SometimesSetsUpAFrame() override { return false; }

 private:
  static const int kInlinedProbes = 4;
  static const int kTotalProbes = 20;

  static const int kCapacityOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;

  LookupMode mode() const { return LookupModeBits::decode(minor_key_); }

  class LookupModeBits : public BitField<LookupMode, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(NameDictionaryLookup, PlatformCodeStub);
};

class FloatingPointHelper : public AllStatic {
 public:
  enum Destination { kFPRegisters, kCoreRegisters };

  // Loads smis from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will be scratched.
  static void LoadSmis(MacroAssembler* masm, Register scratch1,
                       Register scratch2);

  // Loads objects from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will still be scratched. If
  // either r0 or r1 is not a number (not smi and not heap number object) the
  // not_number label is jumped to with r0 and r1 intact.
  static void LoadOperands(MacroAssembler* masm, Register heap_number_map,
                           Register scratch1, Register scratch2,
                           Label* not_number);

  // Convert the smi or heap number in object to an int32 using the rules
  // for ToInt32 as described in ECMAScript 9.5.: the value is truncated
  // and brought into the range -2^31 .. +2^31 - 1.
  static void ConvertNumberToInt32(MacroAssembler* masm, Register object,
                                   Register dst, Register heap_number_map,
                                   Register scratch1, Register scratch2,
                                   Register scratch3,
                                   DoubleRegister double_scratch,
                                   Label* not_int32);

  // Converts the integer (untagged smi) in |src| to a double, storing
  // the result to |double_dst|
  static void ConvertIntToDouble(MacroAssembler* masm, Register src,
                                 DoubleRegister double_dst);

  // Converts the unsigned integer (untagged smi) in |src| to
  // a double, storing the result to |double_dst|
  static void ConvertUnsignedIntToDouble(MacroAssembler* masm, Register src,
                                         DoubleRegister double_dst);

  // Converts the integer (untagged smi) in |src| to
  // a float, storing the result in |dst|
  static void ConvertIntToFloat(MacroAssembler* masm, const DoubleRegister dst,
                                const Register src);

  // Load the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  static void LoadNumberAsInt32Double(MacroAssembler* masm, Register object,
                                      DoubleRegister double_dst,
                                      DoubleRegister double_scratch,
                                      Register heap_number_map,
                                      Register scratch1, Register scratch2,
                                      Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  // scratch3 is not used when VFP3 is supported.
  static void LoadNumberAsInt32(MacroAssembler* masm, Register object,
                                Register dst, Register heap_number_map,
                                Register scratch1, Register scratch2,
                                Register scratch3,
                                DoubleRegister double_scratch0,
                                DoubleRegister double_scratch1,
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
  static void DoubleIs32BitInteger(MacroAssembler* masm, Register src1,
                                   Register src2, Register dst,
                                   Register scratch, Label* not_int32);

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
  static void CallCCodeForDoubleOperation(MacroAssembler* masm, Token::Value op,
                                          Register heap_number_result,
                                          Register scratch);

 private:
  static void LoadNumber(MacroAssembler* masm, Register object,
                         DoubleRegister dst, Register heap_number_map,
                         Register scratch1, Register scratch2,
                         Label* not_number);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_S390_CODE_STUBS_S390_H_
