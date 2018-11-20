// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MIPS_CODE_STUBS_ARM_H_
#define V8_MIPS_CODE_STUBS_ARM_H_

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);


class StringHelper : public AllStatic {
 public:
  // Generate code for copying a large number of characters. This function
  // is allowed to spend extra time setting up conditions to make copying
  // faster. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     Register scratch,
                                     String::Encoding encoding);

  // Compares two flat one-byte strings and returns result in v0.
  static void GenerateCompareFlatOneByteStrings(
      MacroAssembler* masm, Register left, Register right, Register scratch1,
      Register scratch2, Register scratch3, Register scratch4);

  // Compares two flat one-byte strings for equality and returns result in v0.
  static void GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                              Register left, Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3);

 private:
  static void GenerateOneByteCharsCompareLoop(
      MacroAssembler* masm, Register left, Register right, Register length,
      Register scratch1, Register scratch2, Register scratch3,
      Label* chars_not_equal);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


class StoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit StoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(StoreRegistersState, PlatformCodeStub);
};


class RestoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit RestoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(RestoreRegistersState, PlatformCodeStub);
};


class RecordWriteStub: public PlatformCodeStub {
 public:
  RecordWriteStub(Isolate* isolate,
                  Register object,
                  Register value,
                  Register address,
                  RememberedSetAction remembered_set_action,
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

  enum Mode {
    STORE_BUFFER_ONLY,
    INCREMENTAL,
    INCREMENTAL_COMPACTION
  };

  bool SometimesSetsUpAFrame() OVERRIDE { return false; }

  static void PatchBranchIntoNop(MacroAssembler* masm, int pos) {
    const unsigned offset = masm->instr_at(pos) & kImm16Mask;
    masm->instr_at_put(pos, BNE | (zero_reg.code() << kRsShift) |
        (zero_reg.code() << kRtShift) | (offset & kImm16Mask));
    DCHECK(Assembler::IsBne(masm->instr_at(pos)));
  }

  static void PatchNopIntoBranch(MacroAssembler* masm, int pos) {
    const unsigned offset = masm->instr_at(pos) & kImm16Mask;
    masm->instr_at_put(pos, BEQ | (zero_reg.code() << kRsShift) |
        (zero_reg.code() << kRtShift) | (offset & kImm16Mask));
    DCHECK(Assembler::IsBeq(masm->instr_at(pos)));
  }

  static Mode GetMode(Code* stub) {
    Instr first_instruction = Assembler::instr_at(stub->instruction_start());
    Instr second_instruction = Assembler::instr_at(stub->instruction_start() +
                                                   2 * Assembler::kInstrSize);

    if (Assembler::IsBeq(first_instruction)) {
      return INCREMENTAL;
    }

    DCHECK(Assembler::IsBne(first_instruction));

    if (Assembler::IsBeq(second_instruction)) {
      return INCREMENTAL_COMPACTION;
    }

    DCHECK(Assembler::IsBne(second_instruction));

    return STORE_BUFFER_ONLY;
  }

  static void Patch(Code* stub, Mode mode) {
    MacroAssembler masm(NULL,
                        stub->instruction_start(),
                        stub->instruction_size());
    switch (mode) {
      case STORE_BUFFER_ONLY:
        DCHECK(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);
        PatchBranchIntoNop(&masm, 0);
        PatchBranchIntoNop(&masm, 2 * Assembler::kInstrSize);
        break;
      case INCREMENTAL:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchNopIntoBranch(&masm, 0);
        break;
      case INCREMENTAL_COMPACTION:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        PatchNopIntoBranch(&masm, 2 * Assembler::kInstrSize);
        break;
    }
    DCHECK(GetMode(stub) == mode);
    CpuFeatures::FlushICache(stub->instruction_start(),
                             4 * Assembler::kInstrSize);
  }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();

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
      DCHECK(!AreAliased(scratch0, object, address, no_reg));
      scratch1_ = GetRegisterThatIsNotOneOf(object_, address_, scratch0_);
    }

    void Save(MacroAssembler* masm) {
      DCHECK(!AreAliased(object_, address_, scratch1_, scratch0_));
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
      masm->MultiPush((kJSCallerSaved | ra.bit()) & ~scratch1_.bit());
      if (mode == kSaveFPRegs) {
        masm->MultiPushFPU(kCallerSavedFPU);
      }
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler*masm,
                                           SaveFPRegsMode mode) {
      if (mode == kSaveFPRegs) {
        masm->MultiPopFPU(kCallerSavedFPU);
      }
      masm->MultiPop((kJSCallerSaved | ra.bit()) & ~scratch1_.bit());
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

  inline Major MajorKey() const FINAL { return RecordWrite; }

  void Generate(MacroAssembler* masm) OVERRIDE;
  void GenerateIncremental(MacroAssembler* masm, Mode mode);
  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm);

  void Activate(Code* code) OVERRIDE {
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

  class ObjectBits: public BitField<int, 0, 5> {};
  class ValueBits: public BitField<int, 5, 5> {};
  class AddressBits: public BitField<int, 10, 5> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 15, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 16, 1> {};

  Label slow_;
  RegisterAllocation regs_;

  DISALLOW_COPY_AND_ASSIGN(RecordWriteStub);
};


// Trampoline stub to call into native code. To call safely into native code
// in the presence of compacting GC (which can move code objects) we need to
// keep the code which called into native pinned in the memory. Currently the
// simplest approach is to generate such stub early enough so it can never be
// moved by GC
class DirectCEntryStub: public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  bool NeedsImmovableCode() OVERRIDE { return true; }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DirectCEntry, PlatformCodeStub);
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Isolate* isolate, LookupMode mode)
      : PlatformCodeStub(isolate) {
    minor_key_ = LookupModeBits::encode(mode);
  }

  static void GenerateNegativeLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register receiver,
                                     Register properties,
                                     Handle<Name> name,
                                     Register scratch0);

  static void GeneratePositiveLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register elements,
                                     Register name,
                                     Register r0,
                                     Register r1);

  bool SometimesSetsUpAFrame() OVERRIDE { return false; }

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

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(NameDictionaryLookup, PlatformCodeStub);
};


} }  // namespace v8::internal

#endif  // V8_MIPS_CODE_STUBS_ARM_H_
