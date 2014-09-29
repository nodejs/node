// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_CODE_STUBS_IA32_H_
#define V8_IA32_CODE_STUBS_IA32_H_

#include "src/ic-inl.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm,
                     bool construct_call,
                     Label* call_generic_code);


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
  // Generate code for copying characters using the rep movs instruction.
  // Copies ecx characters from esi to edi. Copying of overlapping regions is
  // not supported.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     Register scratch,
                                     String::Encoding encoding);

  // Generate string hash.
  static void GenerateHashInit(MacroAssembler* masm,
                               Register hash,
                               Register character,
                               Register scratch);
  static void GenerateHashAddCharacter(MacroAssembler* masm,
                                       Register hash,
                                       Register character,
                                       Register scratch);
  static void GenerateHashGetHash(MacroAssembler* masm,
                                  Register hash,
                                  Register scratch);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
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

  // Compares two flat ASCII strings and returns result in eax.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3);

  // Compares two flat ASCII strings for equality and returns result
  // in eax.
  static void GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register scratch1,
                                            Register scratch2);

 private:
  virtual Major MajorKey() const { return StringCompare; }
  virtual int MinorKey() const { return 0; }
  virtual void Generate(MacroAssembler* masm);

  static void GenerateAsciiCharsCompareLoop(
      MacroAssembler* masm,
      Register left,
      Register right,
      Register length,
      Register scratch,
      Label* chars_not_equal,
      Label::Distance chars_not_equal_near = Label::kFar);
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Isolate* isolate,
                           Register dictionary,
                           Register result,
                           Register index,
                           LookupMode mode)
      : PlatformCodeStub(isolate),
        dictionary_(dictionary), result_(result), index_(index), mode_(mode) { }

  void Generate(MacroAssembler* masm);

  static void GenerateNegativeLookup(MacroAssembler* masm,
                                     Label* miss,
                                     Label* done,
                                     Register properties,
                                     Handle<Name> name,
                                     Register r0);

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
      NameDictionary::kHeaderSize +
      NameDictionary::kCapacityIndex * kPointerSize;

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;

  Major MajorKey() const { return NameDictionaryLookup; }

  int MinorKey() const {
    return DictionaryBits::encode(dictionary_.code()) |
        ResultBits::encode(result_.code()) |
        IndexBits::encode(index_.code()) |
        LookupModeBits::encode(mode_);
  }

  class DictionaryBits: public BitField<int, 0, 3> {};
  class ResultBits: public BitField<int, 3, 3> {};
  class IndexBits: public BitField<int, 6, 3> {};
  class LookupModeBits: public BitField<LookupMode, 9, 1> {};

  Register dictionary_;
  Register result_;
  Register index_;
  LookupMode mode_;
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

  static const byte kTwoByteNopInstruction = 0x3c;  // Cmpb al, #imm8.
  static const byte kTwoByteJumpInstruction = 0xeb;  // Jmp #imm8.

  static const byte kFiveByteNopInstruction = 0x3d;  // Cmpl eax, #imm32.
  static const byte kFiveByteJumpInstruction = 0xe9;  // Jmp #imm32.

  static Mode GetMode(Code* stub) {
    byte first_instruction = stub->instruction_start()[0];
    byte second_instruction = stub->instruction_start()[2];

    if (first_instruction == kTwoByteJumpInstruction) {
      return INCREMENTAL;
    }

    DCHECK(first_instruction == kTwoByteNopInstruction);

    if (second_instruction == kFiveByteJumpInstruction) {
      return INCREMENTAL_COMPACTION;
    }

    DCHECK(second_instruction == kFiveByteNopInstruction);

    return STORE_BUFFER_ONLY;
  }

  static void Patch(Code* stub, Mode mode) {
    switch (mode) {
      case STORE_BUFFER_ONLY:
        DCHECK(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);
        stub->instruction_start()[0] = kTwoByteNopInstruction;
        stub->instruction_start()[2] = kFiveByteNopInstruction;
        break;
      case INCREMENTAL:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        stub->instruction_start()[0] = kTwoByteJumpInstruction;
        break;
      case INCREMENTAL_COMPACTION:
        DCHECK(GetMode(stub) == STORE_BUFFER_ONLY);
        stub->instruction_start()[0] = kTwoByteNopInstruction;
        stub->instruction_start()[2] = kFiveByteJumpInstruction;
        break;
    }
    DCHECK(GetMode(stub) == mode);
    CpuFeatures::FlushICache(stub->instruction_start(), 7);
  }

 private:
  // This is a helper class for freeing up 3 scratch registers, where the third
  // is always ecx (needed for shift operations).  The input is two registers
  // that must be preserved and one scratch register provided by the caller.
  class RegisterAllocation {
   public:
    RegisterAllocation(Register object,
                       Register address,
                       Register scratch0)
        : object_orig_(object),
          address_orig_(address),
          scratch0_orig_(scratch0),
          object_(object),
          address_(address),
          scratch0_(scratch0) {
      DCHECK(!AreAliased(scratch0, object, address, no_reg));
      scratch1_ = GetRegThatIsNotEcxOr(object_, address_, scratch0_);
      if (scratch0.is(ecx)) {
        scratch0_ = GetRegThatIsNotEcxOr(object_, address_, scratch1_);
      }
      if (object.is(ecx)) {
        object_ = GetRegThatIsNotEcxOr(address_, scratch0_, scratch1_);
      }
      if (address.is(ecx)) {
        address_ = GetRegThatIsNotEcxOr(object_, scratch0_, scratch1_);
      }
      DCHECK(!AreAliased(scratch0_, object_, address_, ecx));
    }

    void Save(MacroAssembler* masm) {
      DCHECK(!address_orig_.is(object_));
      DCHECK(object_.is(object_orig_) || address_.is(address_orig_));
      DCHECK(!AreAliased(object_, address_, scratch1_, scratch0_));
      DCHECK(!AreAliased(object_orig_, address_, scratch1_, scratch0_));
      DCHECK(!AreAliased(object_, address_orig_, scratch1_, scratch0_));
      // We don't have to save scratch0_orig_ because it was given to us as
      // a scratch register.  But if we had to switch to a different reg then
      // we should save the new scratch0_.
      if (!scratch0_.is(scratch0_orig_)) masm->push(scratch0_);
      if (!ecx.is(scratch0_orig_) &&
          !ecx.is(object_orig_) &&
          !ecx.is(address_orig_)) {
        masm->push(ecx);
      }
      masm->push(scratch1_);
      if (!address_.is(address_orig_)) {
        masm->push(address_);
        masm->mov(address_, address_orig_);
      }
      if (!object_.is(object_orig_)) {
        masm->push(object_);
        masm->mov(object_, object_orig_);
      }
    }

    void Restore(MacroAssembler* masm) {
      // These will have been preserved the entire time, so we just need to move
      // them back.  Only in one case is the orig_ reg different from the plain
      // one, since only one of them can alias with ecx.
      if (!object_.is(object_orig_)) {
        masm->mov(object_orig_, object_);
        masm->pop(object_);
      }
      if (!address_.is(address_orig_)) {
        masm->mov(address_orig_, address_);
        masm->pop(address_);
      }
      masm->pop(scratch1_);
      if (!ecx.is(scratch0_orig_) &&
          !ecx.is(object_orig_) &&
          !ecx.is(address_orig_)) {
        masm->pop(ecx);
      }
      if (!scratch0_.is(scratch0_orig_)) masm->pop(scratch0_);
    }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.  The caller saved
    // registers are eax, ecx and edx.  The three scratch registers (incl. ecx)
    // will be restored by other means so we don't bother pushing them here.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      if (!scratch0_.is(eax) && !scratch1_.is(eax)) masm->push(eax);
      if (!scratch0_.is(edx) && !scratch1_.is(edx)) masm->push(edx);
      if (mode == kSaveFPRegs) {
        masm->sub(esp,
                  Immediate(kDoubleSize * (XMMRegister::kMaxNumRegisters - 1)));
        // Save all XMM registers except XMM0.
        for (int i = XMMRegister::kMaxNumRegisters - 1; i > 0; i--) {
          XMMRegister reg = XMMRegister::from_code(i);
          masm->movsd(Operand(esp, (i - 1) * kDoubleSize), reg);
        }
      }
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler*masm,
                                           SaveFPRegsMode mode) {
      if (mode == kSaveFPRegs) {
        // Restore all XMM registers except XMM0.
        for (int i = XMMRegister::kMaxNumRegisters - 1; i > 0; i--) {
          XMMRegister reg = XMMRegister::from_code(i);
          masm->movsd(reg, Operand(esp, (i - 1) * kDoubleSize));
        }
        masm->add(esp,
                  Immediate(kDoubleSize * (XMMRegister::kMaxNumRegisters - 1)));
      }
      if (!scratch0_.is(edx) && !scratch1_.is(edx)) masm->pop(edx);
      if (!scratch0_.is(eax) && !scratch1_.is(eax)) masm->pop(eax);
    }

    inline Register object() { return object_; }
    inline Register address() { return address_; }
    inline Register scratch0() { return scratch0_; }
    inline Register scratch1() { return scratch1_; }

   private:
    Register object_orig_;
    Register address_orig_;
    Register scratch0_orig_;
    Register object_;
    Register address_;
    Register scratch0_;
    Register scratch1_;
    // Third scratch register is always ecx.

    Register GetRegThatIsNotEcxOr(Register r1,
                                  Register r2,
                                  Register r3) {
      for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
        Register candidate = Register::FromAllocationIndex(i);
        if (candidate.is(ecx)) continue;
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
  }
;
  void Generate(MacroAssembler* masm);
  void GenerateIncremental(MacroAssembler* masm, Mode mode);
  void CheckNeedsToInformIncrementalMarker(
      MacroAssembler* masm,
      OnNoNeedToInformIncrementalMarker on_no_need,
      Mode mode);
  void InformIncrementalMarker(MacroAssembler* masm);

  Major MajorKey() const { return RecordWrite; }

  int MinorKey() const {
    return ObjectBits::encode(object_.code()) |
        ValueBits::encode(value_.code()) |
        AddressBits::encode(address_.code()) |
        RememberedSetActionBits::encode(remembered_set_action_) |
        SaveFPRegsModeBits::encode(save_fp_regs_mode_);
  }

  void Activate(Code* code) {
    code->GetHeap()->incremental_marking()->ActivateGeneratedStub(code);
  }

  class ObjectBits: public BitField<int, 0, 3> {};
  class ValueBits: public BitField<int, 3, 3> {};
  class AddressBits: public BitField<int, 6, 3> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 9, 1> {};
  class SaveFPRegsModeBits: public BitField<SaveFPRegsMode, 10, 1> {};

  Register object_;
  Register value_;
  Register address_;
  RememberedSetAction remembered_set_action_;
  SaveFPRegsMode save_fp_regs_mode_;
  RegisterAllocation regs_;
};


} }  // namespace v8::internal

#endif  // V8_IA32_CODE_STUBS_IA32_H_
