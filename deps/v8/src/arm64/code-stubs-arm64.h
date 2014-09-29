// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CODE_STUBS_ARM64_H_
#define V8_ARM64_CODE_STUBS_ARM64_H_

#include "src/ic-inl.h"

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);


class StoreBufferOverflowStub: public PlatformCodeStub {
 public:
  StoreBufferOverflowStub(Isolate* isolate, SaveFPRegsMode save_fp)
      : PlatformCodeStub(isolate), save_doubles_(save_fp) { }

  void Generate(MacroAssembler* masm);

  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);
  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  SaveFPRegsMode save_doubles_;

  Major MajorKey() const { return StoreBufferOverflow; }
  int MinorKey() const { return (save_doubles_ == kSaveFPRegs) ? 1 : 0; }
};


class StringHelper : public AllStatic {
 public:
  // TODO(all): These don't seem to be used any more. Delete them.

  // Generate string hash.
  static void GenerateHashInit(MacroAssembler* masm,
                               Register hash,
                               Register character);

  static void GenerateHashAddCharacter(MacroAssembler* masm,
                                       Register hash,
                                       Register character);

  static void GenerateHashGetHash(MacroAssembler* masm,
                                  Register hash,
                                  Register scratch);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


class StoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit StoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static Register to_be_pushed_lr() { return ip0; }
  static void GenerateAheadOfTime(Isolate* isolate);
 private:
  Major MajorKey() const { return StoreRegistersState; }
  int MinorKey() const { return 0; }

  void Generate(MacroAssembler* masm);
};


class RestoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit RestoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);
 private:
  Major MajorKey() const { return RestoreRegistersState; }
  int MinorKey() const { return 0; }

  void Generate(MacroAssembler* masm);
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
        object_(object),
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

  virtual bool SometimesSetsUpAFrame() { return false; }

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
        reinterpret_cast<Instruction*>(stub->instruction_start()), 2);
    Instruction* instr1 = patcher.InstructionAt(0);
    Instruction* instr2 = patcher.InstructionAt(kInstructionSize);
    // Instructions must be either 'adr' or 'b'.
    DCHECK(instr1->IsPCRelAddressing() || instr1->IsUncondBranchImm());
    DCHECK(instr2->IsPCRelAddressing() || instr2->IsUncondBranchImm());
    // Retrieve the offsets to the labels.
    int32_t offset_to_incremental_noncompacting = instr1->ImmPCOffset();
    int32_t offset_to_incremental_compacting = instr2->ImmPCOffset();

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

  // A list of stub variants which are pregenerated.
  // The variants are stored in the same format as the minor key, so
  // MinorKeyFor() can be used to populate and check this list.
  static const int kAheadOfTime[];

  void Generate(MacroAssembler* masm);
  void GenerateIncremental(MacroAssembler* masm, Mode mode);

  enum OnNoNeedToInformIncrementalMarker {
    kReturnOnNoNeedToInformIncrementalMarker,
    kUpdateRememberedSetOnNoNeedToInformIncrementalMarker
  };

  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm);

  Major MajorKey() const { return RecordWrite; }

  int MinorKey() const {
    return MinorKeyFor(object_, value_, address_, remembered_set_action_,
                       save_fp_regs_mode_);
  }

  static int MinorKeyFor(Register object,
                         Register value,
                         Register address,
                         RememberedSetAction action,
                         SaveFPRegsMode fp_mode) {
    DCHECK(object.Is64Bits());
    DCHECK(value.Is64Bits());
    DCHECK(address.Is64Bits());
    return ObjectBits::encode(object.code()) |
        ValueBits::encode(value.code()) |
        AddressBits::encode(address.code()) |
        RememberedSetActionBits::encode(action) |
        SaveFPRegsModeBits::encode(fp_mode);
  }

  void Activate(Code* code) {
    code->GetHeap()->incremental_marking()->ActivateGeneratedStub(code);
  }

  class ObjectBits: public BitField<int, 0, 5> {};
  class ValueBits: public BitField<int, 5, 5> {};
  class AddressBits: public BitField<int, 10, 5> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 15, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 16, 1> {};

  Register object_;
  Register value_;
  Register address_;
  RememberedSetAction remembered_set_action_;
  SaveFPRegsMode save_fp_regs_mode_;
  Label slow_;
  RegisterAllocation regs_;
};


// Helper to call C++ functions from generated code. The caller must prepare
// the exit frame before doing the call with GenerateCall.
class DirectCEntryStub: public PlatformCodeStub {
 public:
  explicit DirectCEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) {}
  void Generate(MacroAssembler* masm);
  void GenerateCall(MacroAssembler* masm, Register target);

 private:
  Major MajorKey() const { return DirectCEntry; }
  int MinorKey() const { return 0; }

  bool NeedsImmovableCode() { return true; }
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Isolate* isolate, LookupMode mode)
      : PlatformCodeStub(isolate), mode_(mode) { }

  void Generate(MacroAssembler* masm);

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
                                     Register scratch1,
                                     Register scratch2);

  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  static const int kInlinedProbes = 4;
  static const int kTotalProbes = 20;

  static const int kCapacityOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;

  Major MajorKey() const { return NameDictionaryLookup; }

  int MinorKey() const { return LookupModeBits::encode(mode_); }

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  LookupMode mode_;
};


class SubStringStub: public PlatformCodeStub {
 public:
  explicit SubStringStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  Major MajorKey() const { return SubString; }
  int MinorKey() const { return 0; }

  void Generate(MacroAssembler* masm);
};


class StringCompareStub: public PlatformCodeStub {
 public:
  explicit StringCompareStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

  // Compares two flat ASCII strings and returns result in x0.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

  // Compare two flat ASCII strings for equality and returns result
  // in x0.
  static void GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3);

 private:
  virtual Major MajorKey() const { return StringCompare; }
  virtual int MinorKey() const { return 0; }
  virtual void Generate(MacroAssembler* masm);

  static void GenerateAsciiCharsCompareLoop(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register length,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* chars_not_equal);
};


class PlatformInterfaceDescriptor {
 public:
  explicit PlatformInterfaceDescriptor(
      TargetAddressStorageMode storage_mode)
      : storage_mode_(storage_mode) { }

  TargetAddressStorageMode storage_mode() { return storage_mode_; }

 private:
  TargetAddressStorageMode storage_mode_;
};


} }  // namespace v8::internal

#endif  // V8_ARM64_CODE_STUBS_ARM64_H_
