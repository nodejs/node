// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CODE_STUBS_ARM64_H_
#define V8_ARM64_CODE_STUBS_ARM64_H_

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);


class StringHelper : public AllStatic {
 public:
  // Compares two flat one-byte strings and returns result in x0.
  static void GenerateCompareFlatOneByteStrings(
      MacroAssembler* masm, Register left, Register right, Register scratch1,
      Register scratch2, Register scratch3, Register scratch4);

  // Compare two flat one-byte strings for equality and returns result in x0.
  static void GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                              Register left, Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3);

 private:
  static void GenerateOneByteCharsCompareLoop(
      MacroAssembler* masm, Register left, Register right, Register length,
      Register scratch1, Register scratch2, Label* chars_not_equal);

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


class StoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit StoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static Register to_be_pushed_lr() { return ip0; }

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
  // Stub to record the write of 'value' at 'address' in 'object'.
  // Typically 'address' = 'object' + <some offset>.
  // See MacroAssembler::RecordWriteField() for example.
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
    DCHECK(object.Is64Bits());
    DCHECK(value.Is64Bits());
    DCHECK(address.Is64Bits());
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

  bool SometimesSetsUpAFrame() override { return false; }

  static Mode GetMode(Code* stub) {
    // Find the mode depending on the first two instructions.
    Instruction* instr1 =
      reinterpret_cast<Instruction*>(stub->instruction_start());
    Instruction* instr2 = instr1->following();

    if (instr1->IsUncondBranchImm()) {
      DCHECK(instr2->IsPCRelAddressing() && (instr2->Rd() == xzr.code()));
      return INCREMENTAL;
    }

    DCHECK(instr1->IsPCRelAddressing() && (instr1->Rd() == xzr.code()));

    if (instr2->IsUncondBranchImm()) {
      return INCREMENTAL_COMPACTION;
    }

    DCHECK(instr2->IsPCRelAddressing());

    return STORE_BUFFER_ONLY;
  }

  // We patch the two first instructions of the stub back and forth between an
  // adr and branch when we start and stop incremental heap marking.
  // The branch is
  //   b label
  // The adr is
  //   adr xzr label
  // so effectively a nop.
  static void Patch(Code* stub, Mode mode) {
    // We are going to patch the two first instructions of the stub.
    PatchingAssembler patcher(
        stub->GetIsolate(),
        reinterpret_cast<Instruction*>(stub->instruction_start()), 2);
    Instruction* instr1 = patcher.InstructionAt(0);
    Instruction* instr2 = patcher.InstructionAt(kInstructionSize);
    // Instructions must be either 'adr' or 'b'.
    DCHECK(instr1->IsPCRelAddressing() || instr1->IsUncondBranchImm());
    DCHECK(instr2->IsPCRelAddressing() || instr2->IsUncondBranchImm());
    // Retrieve the offsets to the labels.
    auto offset_to_incremental_noncompacting =
        static_cast<int32_t>(instr1->ImmPCOffset());
    auto offset_to_incremental_compacting =
        static_cast<int32_t>(instr2->ImmPCOffset());

    switch (mode) {
      case STORE_BUFFER_ONLY:
        DCHECK(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);
        patcher.adr(xzr, offset_to_incremental_noncompacting);
        patcher.adr(xzr, offset_to_incremental_compacting);
        break;
      case INCREMENTAL:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        patcher.b(offset_to_incremental_noncompacting >> kInstructionSizeLog2);
        patcher.adr(xzr, offset_to_incremental_compacting);
        break;
      case INCREMENTAL_COMPACTION:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        patcher.adr(xzr, offset_to_incremental_noncompacting);
        patcher.b(offset_to_incremental_compacting >> kInstructionSizeLog2);
        break;
    }
    DCHECK(GetMode(stub) == mode);
  }

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();

 private:
  // This is a helper class to manage the registers associated with the stub.
  // The 'object' and 'address' registers must be preserved.
  class RegisterAllocation {
   public:
    RegisterAllocation(Register object,
                       Register address,
                       Register scratch)
        : object_(object),
          address_(address),
          scratch0_(scratch),
          saved_regs_(kCallerSaved),
          saved_fp_regs_(kCallerSavedFP) {
      DCHECK(!AreAliased(scratch, object, address));

      // The SaveCallerSaveRegisters method needs to save caller-saved
      // registers, but we don't bother saving MacroAssembler scratch registers.
      saved_regs_.Remove(MacroAssembler::DefaultTmpList());
      saved_fp_regs_.Remove(MacroAssembler::DefaultFPTmpList());

      // We would like to require more scratch registers for this stub,
      // but the number of registers comes down to the ones used in
      // FullCodeGen::SetVar(), which is architecture independent.
      // We allocate 2 extra scratch registers that we'll save on the stack.
      CPURegList pool_available = GetValidRegistersForAllocation();
      CPURegList used_regs(object, address, scratch);
      pool_available.Remove(used_regs);
      scratch1_ = Register(pool_available.PopLowestIndex());
      scratch2_ = Register(pool_available.PopLowestIndex());

      // The scratch registers will be restored by other means so we don't need
      // to save them with the other caller saved registers.
      saved_regs_.Remove(scratch0_);
      saved_regs_.Remove(scratch1_);
      saved_regs_.Remove(scratch2_);
    }

    void Save(MacroAssembler* masm) {
      // We don't have to save scratch0_ because it was given to us as
      // a scratch register.
      masm->Push(scratch1_, scratch2_);
    }

    void Restore(MacroAssembler* masm) {
      masm->Pop(scratch2_, scratch1_);
    }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      // TODO(all): This can be very expensive, and it is likely that not every
      // register will need to be preserved. Can we improve this?
      masm->PushCPURegList(saved_regs_);
      if (mode == kSaveFPRegs) {
        masm->PushCPURegList(saved_fp_regs_);
      }
    }

    void RestoreCallerSaveRegisters(MacroAssembler*masm, SaveFPRegsMode mode) {
      // TODO(all): This can be very expensive, and it is likely that not every
      // register will need to be preserved. Can we improve this?
      if (mode == kSaveFPRegs) {
        masm->PopCPURegList(saved_fp_regs_);
      }
      masm->PopCPURegList(saved_regs_);
    }

    Register object() { return object_; }
    Register address() { return address_; }
    Register scratch0() { return scratch0_; }
    Register scratch1() { return scratch1_; }
    Register scratch2() { return scratch2_; }

   private:
    Register object_;
    Register address_;
    Register scratch0_;
    Register scratch1_;
    Register scratch2_;
    CPURegList saved_regs_;
    CPURegList saved_fp_regs_;

    // TODO(all): We should consider moving this somewhere else.
    static CPURegList GetValidRegistersForAllocation() {
      // The list of valid registers for allocation is defined as all the
      // registers without those with a special meaning.
      //
      // The default list excludes registers x26 to x31 because they are
      // reserved for the following purpose:
      //  - x26 root register
      //  - x27 context pointer register
      //  - x28 jssp
      //  - x29 frame pointer
      //  - x30 link register(lr)
      //  - x31 xzr/stack pointer
      CPURegList list(CPURegister::kRegister, kXRegSizeInBits, 0, 25);

      // We also remove MacroAssembler's scratch registers.
      list.Remove(MacroAssembler::DefaultTmpList());

      return list;
    }

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
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
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

  class ObjectBits: public BitField<int, 0, 5> {};
  class ValueBits: public BitField<int, 5, 5> {};
  class AddressBits: public BitField<int, 10, 5> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 15, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 16, 1> {};

  Label slow_;
  RegisterAllocation regs_;
};


// Helper to call C++ functions from generated code. The caller must prepare
// the exit frame before doing the call with GenerateCall.
class DirectCEntryStub: public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  bool NeedsImmovableCode() override { return true; }

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

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(NameDictionaryLookup, PlatformCodeStub);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_CODE_STUBS_ARM64_H_
