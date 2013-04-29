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

#ifndef V8_X64_CODE_STUBS_X64_H_
#define V8_X64_CODE_STUBS_X64_H_

#include "ic-inl.h"
#include "type-info.h"

namespace v8 {
namespace internal {


void ArrayNativeCode(MacroAssembler* masm, Label* call_generic_code);

// Compute a transcendental math function natively, or call the
// TranscendentalCache runtime function.
class TranscendentalCacheStub: public PlatformCodeStub {
 public:
  enum ArgumentType {
    TAGGED = 0,
    UNTAGGED = 1 << TranscendentalCache::kTranscendentalTypeBits
  };

  explicit TranscendentalCacheStub(TranscendentalCache::Type type,
                                   ArgumentType argument_type)
      : type_(type), argument_type_(argument_type) {}
  void Generate(MacroAssembler* masm);
  static void GenerateOperation(MacroAssembler* masm,
                                TranscendentalCache::Type type);
 private:
  TranscendentalCache::Type type_;
  ArgumentType argument_type_;

  Major MajorKey() { return TranscendentalCache; }
  int MinorKey() { return type_ | argument_type_; }
  Runtime::FunctionId RuntimeFunction();
};


class StoreBufferOverflowStub: public PlatformCodeStub {
 public:
  explicit StoreBufferOverflowStub(SaveFPRegsMode save_fp)
      : save_doubles_(save_fp) { }

  void Generate(MacroAssembler* masm);

  virtual bool IsPregenerated() { return true; }
  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);
  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  SaveFPRegsMode save_doubles_;

  Major MajorKey() { return StoreBufferOverflow; }
  int MinorKey() { return (save_doubles_ == kSaveFPRegs) ? 1 : 0; }
};


class UnaryOpStub: public PlatformCodeStub {
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
  void GenerateSmiCodeSub(MacroAssembler* masm,
                          Label* non_smi,
                          Label* slow,
                          Label::Distance non_smi_near = Label::kFar,
                          Label::Distance slow_near = Label::kFar);
  void GenerateSmiCodeBitNot(MacroAssembler* masm,
                             Label* non_smi,
                             Label::Distance non_smi_near);

  void GenerateNumberStub(MacroAssembler* masm);
  void GenerateNumberStubSub(MacroAssembler* masm);
  void GenerateNumberStubBitNot(MacroAssembler* masm);
  void GenerateHeapNumberCodeSub(MacroAssembler* masm, Label* slow);
  void GenerateHeapNumberCodeBitNot(MacroAssembler* masm, Label* slow);

  void GenerateGenericStub(MacroAssembler* masm);
  void GenerateGenericStubSub(MacroAssembler* masm);
  void GenerateGenericStubBitNot(MacroAssembler* masm);
  void GenerateGenericCodeFallback(MacroAssembler* masm);

  virtual Code::Kind GetCodeKind() const { return Code::UNARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return UnaryOpIC::ToState(operand_type_);
  }

  virtual void FinishCode(Handle<Code> code) {
    code->set_unary_op_type(operand_type_);
  }
};


class StringHelper : public AllStatic {
 public:
  // Generate code for copying characters using a simple loop. This should only
  // be used in places where the number of characters is small and the
  // additional setup and checking in GenerateCopyCharactersREP adds too much
  // overhead. Copying of overlapping regions is not supported.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     bool ascii);

  // Generate code for copying characters using the rep movs instruction.
  // Copies rcx characters from rsi to rdi. Copying of overlapping regions is
  // not supported.
  static void GenerateCopyCharactersREP(MacroAssembler* masm,
                                        Register dest,     // Must be rdi.
                                        Register src,      // Must be rsi.
                                        Register count,    // Must be rcx.
                                        bool ascii);


  // Probe the string table for a two character string. If the string is
  // not found by probing a jump to the label not_found is performed. This jump
  // does not guarantee that the string is not in the string table. If the
  // string is found the code falls through with the string in register rax.
  static void GenerateTwoCharacterStringTableProbe(MacroAssembler* masm,
                                                   Register c1,
                                                   Register c2,
                                                   Register scratch1,
                                                   Register scratch2,
                                                   Register scratch3,
                                                   Register scratch4,
                                                   Label* not_found);

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


class StringAddStub: public PlatformCodeStub {
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
                               Label* slow);

  const StringAddFlags flags_;
};


class SubStringStub: public PlatformCodeStub {
 public:
  SubStringStub() {}

 private:
  Major MajorKey() { return SubString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class StringCompareStub: public PlatformCodeStub {
 public:
  StringCompareStub() {}

  // Compares two flat ASCII strings and returns result in rax.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

  // Compares two flat ASCII strings for equality and returns result
  // in rax.
  static void GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                            Register left,
                                            Register right,
                                            Register scratch1,
                                            Register scratch2);

 private:
  virtual Major MajorKey() { return StringCompare; }
  virtual int MinorKey() { return 0; }
  virtual void Generate(MacroAssembler* masm);

  static void GenerateAsciiCharsCompareLoop(
      MacroAssembler* masm,
      Register left,
      Register right,
      Register length,
      Register scratch,
      Label* chars_not_equal,
      Label::Distance near_jump = Label::kFar);
};


class NumberToStringStub: public PlatformCodeStub {
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
                                              bool object_is_smi,
                                              Label* not_found);

 private:
  static void GenerateConvertHashCodeToIndex(MacroAssembler* masm,
                                             Register hash,
                                             Register mask);

  Major MajorKey() { return NumberToString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class NameDictionaryLookupStub: public PlatformCodeStub {
 public:
  enum LookupMode { POSITIVE_LOOKUP, NEGATIVE_LOOKUP };

  NameDictionaryLookupStub(Register dictionary,
                           Register result,
                           Register index,
                           LookupMode mode)
      : dictionary_(dictionary), result_(result), index_(index), mode_(mode) { }

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

  Major MajorKey() { return NameDictionaryLookup; }

  int MinorKey() {
    return DictionaryBits::encode(dictionary_.code()) |
        ResultBits::encode(result_.code()) |
        IndexBits::encode(index_.code()) |
        LookupModeBits::encode(mode_);
  }

  class DictionaryBits: public BitField<int, 0, 4> {};
  class ResultBits: public BitField<int, 4, 4> {};
  class IndexBits: public BitField<int, 8, 4> {};
  class LookupModeBits: public BitField<LookupMode, 12, 1> {};

  Register dictionary_;
  Register result_;
  Register index_;
  LookupMode mode_;
};


class RecordWriteStub: public PlatformCodeStub {
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
  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);
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

    ASSERT(first_instruction == kTwoByteNopInstruction);

    if (second_instruction == kFiveByteJumpInstruction) {
      return INCREMENTAL_COMPACTION;
    }

    ASSERT(second_instruction == kFiveByteNopInstruction);

    return STORE_BUFFER_ONLY;
  }

  static void Patch(Code* stub, Mode mode) {
    switch (mode) {
      case STORE_BUFFER_ONLY:
        ASSERT(GetMode(stub) == INCREMENTAL ||
               GetMode(stub) == INCREMENTAL_COMPACTION);
        stub->instruction_start()[0] = kTwoByteNopInstruction;
        stub->instruction_start()[2] = kFiveByteNopInstruction;
        break;
      case INCREMENTAL:
        ASSERT(GetMode(stub) == STORE_BUFFER_ONLY);
        stub->instruction_start()[0] = kTwoByteJumpInstruction;
        break;
      case INCREMENTAL_COMPACTION:
        ASSERT(GetMode(stub) == STORE_BUFFER_ONLY);
        stub->instruction_start()[0] = kTwoByteNopInstruction;
        stub->instruction_start()[2] = kFiveByteJumpInstruction;
        break;
    }
    ASSERT(GetMode(stub) == mode);
    CPU::FlushICache(stub->instruction_start(), 7);
  }

 private:
  // This is a helper class for freeing up 3 scratch registers, where the third
  // is always rcx (needed for shift operations).  The input is two registers
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
      ASSERT(!AreAliased(scratch0, object, address, no_reg));
      scratch1_ = GetRegThatIsNotRcxOr(object_, address_, scratch0_);
      if (scratch0.is(rcx)) {
        scratch0_ = GetRegThatIsNotRcxOr(object_, address_, scratch1_);
      }
      if (object.is(rcx)) {
        object_ = GetRegThatIsNotRcxOr(address_, scratch0_, scratch1_);
      }
      if (address.is(rcx)) {
        address_ = GetRegThatIsNotRcxOr(object_, scratch0_, scratch1_);
      }
      ASSERT(!AreAliased(scratch0_, object_, address_, rcx));
    }

    void Save(MacroAssembler* masm) {
      ASSERT(!address_orig_.is(object_));
      ASSERT(object_.is(object_orig_) || address_.is(address_orig_));
      ASSERT(!AreAliased(object_, address_, scratch1_, scratch0_));
      ASSERT(!AreAliased(object_orig_, address_, scratch1_, scratch0_));
      ASSERT(!AreAliased(object_, address_orig_, scratch1_, scratch0_));
      // We don't have to save scratch0_orig_ because it was given to us as
      // a scratch register.  But if we had to switch to a different reg then
      // we should save the new scratch0_.
      if (!scratch0_.is(scratch0_orig_)) masm->push(scratch0_);
      if (!rcx.is(scratch0_orig_) &&
          !rcx.is(object_orig_) &&
          !rcx.is(address_orig_)) {
        masm->push(rcx);
      }
      masm->push(scratch1_);
      if (!address_.is(address_orig_)) {
        masm->push(address_);
        masm->movq(address_, address_orig_);
      }
      if (!object_.is(object_orig_)) {
        masm->push(object_);
        masm->movq(object_, object_orig_);
      }
    }

    void Restore(MacroAssembler* masm) {
      // These will have been preserved the entire time, so we just need to move
      // them back.  Only in one case is the orig_ reg different from the plain
      // one, since only one of them can alias with rcx.
      if (!object_.is(object_orig_)) {
        masm->movq(object_orig_, object_);
        masm->pop(object_);
      }
      if (!address_.is(address_orig_)) {
        masm->movq(address_orig_, address_);
        masm->pop(address_);
      }
      masm->pop(scratch1_);
      if (!rcx.is(scratch0_orig_) &&
          !rcx.is(object_orig_) &&
          !rcx.is(address_orig_)) {
        masm->pop(rcx);
      }
      if (!scratch0_.is(scratch0_orig_)) masm->pop(scratch0_);
    }

    // If we have to call into C then we need to save and restore all caller-
    // saved registers that were not already preserved.

    // The three scratch registers (incl. rcx) will be restored by other means
    // so we don't bother pushing them here.  Rbx, rbp and r12-15 are callee
    // save and don't need to be preserved.
    void SaveCallerSaveRegisters(MacroAssembler* masm, SaveFPRegsMode mode) {
      masm->PushCallerSaved(mode, scratch0_, scratch1_, rcx);
    }

    inline void RestoreCallerSaveRegisters(MacroAssembler*masm,
                                           SaveFPRegsMode mode) {
      masm->PopCallerSaved(mode, scratch0_, scratch1_, rcx);
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
    // Third scratch register is always rcx.

    Register GetRegThatIsNotRcxOr(Register r1,
                                  Register r2,
                                  Register r3) {
      for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
        Register candidate = Register::FromAllocationIndex(i);
        if (candidate.is(rcx)) continue;
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


} }  // namespace v8::internal

#endif  // V8_X64_CODE_STUBS_X64_H_
