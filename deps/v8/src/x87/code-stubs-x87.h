// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X87_CODE_STUBS_X87_H_
#define V8_X87_CODE_STUBS_X87_H_

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm,
                     bool construct_call,
                     Label* call_generic_code);


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

  // Compares two flat one byte strings and returns result in eax.
  static void GenerateCompareFlatOneByteStrings(MacroAssembler* masm,
                                                Register left, Register right,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3);

  // Compares two flat one byte strings for equality and returns result in eax.
  static void GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                              Register left, Register right,
                                              Register scratch1,
                                              Register scratch2);

 private:
  static void GenerateOneByteCharsCompareLoop(
      MacroAssembler* masm, Register left, Register right, Register length,
      Register scratch, Label* chars_not_equal,
      Label::Distance chars_not_equal_near = Label::kFar);

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Isolate* isolate, Register dictionary,
                           Register result, Register index, LookupMode mode)
      : PlatformCodeStub(isolate) {
    minor_key_ = DictionaryBits::encode(dictionary.code()) |
                 ResultBits::encode(result.code()) |
                 IndexBits::encode(index.code()) | LookupModeBits::encode(mode);
  }

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

  Register dictionary() const {
    return Register::from_code(DictionaryBits::decode(minor_key_));
  }

  Register result() const {
    return Register::from_code(ResultBits::decode(minor_key_));
  }

  Register index() const {
    return Register::from_code(IndexBits::decode(minor_key_));
  }

  LookupMode mode() const { return LookupModeBits::decode(minor_key_); }

  class DictionaryBits: public BitField<int, 0, 3> {};
  class ResultBits: public BitField<int, 3, 3> {};
  class IndexBits: public BitField<int, 6, 3> {};
  class LookupModeBits: public BitField<LookupMode, 9, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(NameDictionaryLookup, PlatformCodeStub);
};


class RecordWriteStub: public PlatformCodeStub {
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

  enum Mode {
    STORE_BUFFER_ONLY,
    INCREMENTAL,
    INCREMENTAL_COMPACTION
  };

  bool SometimesSetsUpAFrame() OVERRIDE { return false; }

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

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();

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
        // Save FPU state in m108byte.
        masm->sub(esp, Immediate(108));
        masm->fnsave(Operand(esp, 0));
      }
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler* masm,
                                           SaveFPRegsMode mode) {
      if (mode == kSaveFPRegs) {
        // Restore FPU state in m108byte.
        masm->frstor(Operand(esp, 0));
        masm->add(esp, Immediate(108));
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

  class ObjectBits: public BitField<int, 0, 3> {};
  class ValueBits: public BitField<int, 3, 3> {};
  class AddressBits: public BitField<int, 6, 3> {};
  class RememberedSetActionBits: public BitField<RememberedSetAction, 9, 1> {};
  class SaveFPRegsModeBits : public BitField<SaveFPRegsMode, 10, 1> {};

  RegisterAllocation regs_;

  DISALLOW_COPY_AND_ASSIGN(RecordWriteStub);
};


} }  // namespace v8::internal

#endif  // V8_X87_CODE_STUBS_X87_H_
