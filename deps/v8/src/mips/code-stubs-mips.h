// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MIPS_CODE_STUBS_ARM_H_
#define V8_MIPS_CODE_STUBS_ARM_H_

#include "src/ic-inl.h"


namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);


class StoreBufferOverflowStub: public PlatformCodeStub {
 public:
  StoreBufferOverflowStub(Isolate* isolate, SaveFPRegsMode save_fp)
      : PlatformCodeStub(isolate), save_doubles_(save_fp) {}

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


class SubStringStub: public PlatformCodeStub {
 public:
  explicit SubStringStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  Major MajorKey() const { return SubString; }
  int MinorKey() const { return 0; }

  void Generate(MacroAssembler* masm);
};


class StoreRegistersStateStub: public PlatformCodeStub {
 public:
  explicit StoreRegistersStateStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

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

class StringCompareStub: public PlatformCodeStub {
 public:
  explicit StringCompareStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

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
  virtual Major MajorKey() const { return StringCompare; }
  virtual int MinorKey() const { return 0; }
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
class WriteInt32ToHeapNumberStub : public PlatformCodeStub {
 public:
  WriteInt32ToHeapNumberStub(Isolate* isolate,
                             Register the_int,
                             Register the_heap_number,
                             Register scratch,
                             Register scratch2)
      : PlatformCodeStub(isolate),
        the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch),
        sign_(scratch2) {
    DCHECK(IntRegisterBits::is_valid(the_int_.code()));
    DCHECK(HeapNumberRegisterBits::is_valid(the_heap_number_.code()));
    DCHECK(ScratchRegisterBits::is_valid(scratch_.code()));
    DCHECK(SignRegisterBits::is_valid(sign_.code()));
  }

  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;
  Register sign_;

  // Minor key encoding in 16 bits.
  class IntRegisterBits: public BitField<int, 0, 4> {};
  class HeapNumberRegisterBits: public BitField<int, 4, 4> {};
  class ScratchRegisterBits: public BitField<int, 8, 4> {};
  class SignRegisterBits: public BitField<int, 12, 4> {};

  Major MajorKey() const { return WriteInt32ToHeapNumber; }
  int MinorKey() const {
    // Encode the parameters in a unique 16 bit value.
    return IntRegisterBits::encode(the_int_.code())
           | HeapNumberRegisterBits::encode(the_heap_number_.code())
           | ScratchRegisterBits::encode(scratch_.code())
           | SignRegisterBits::encode(sign_.code());
  }

  void Generate(MacroAssembler* masm);
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


// Trampoline stub to call into native code. To call safely into native code
// in the presence of compacting GC (which can move code objects) we need to
// keep the code which called into native pinned in the memory. Currently the
// simplest approach is to generate such stub early enough so it can never be
// moved by GC
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

  int MinorKey() const { return LookupModeBits::encode(mode_); }

  class LookupModeBits: public BitField<LookupMode, 0, 1> {};

  LookupMode mode_;
};


} }  // namespace v8::internal

#endif  // V8_MIPS_CODE_STUBS_ARM_H_
