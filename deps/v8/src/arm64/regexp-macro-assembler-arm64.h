// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_REGEXP_MACRO_ASSEMBLER_ARM64_H_
#define V8_ARM64_REGEXP_MACRO_ASSEMBLER_ARM64_H_

#include "src/macro-assembler.h"

#include "src/arm64/assembler-arm64.h"
#include "src/arm64/assembler-arm64-inl.h"

namespace v8 {
namespace internal {


#ifndef V8_INTERPRETED_REGEXP
class RegExpMacroAssemblerARM64: public NativeRegExpMacroAssembler {
 public:
  RegExpMacroAssemblerARM64(Isolate* isolate, Zone* zone, Mode mode,
                            int registers_to_save);
  virtual ~RegExpMacroAssemblerARM64();
  virtual void AbortedCodeGeneration() { masm_->AbortedCodeGeneration(); }
  virtual int stack_limit_slack();
  virtual void AdvanceCurrentPosition(int by);
  virtual void AdvanceRegister(int reg, int by);
  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void CheckAtStart(Label* on_at_start);
  virtual void CheckCharacter(unsigned c, Label* on_equal);
  virtual void CheckCharacterAfterAnd(unsigned c,
                                      unsigned mask,
                                      Label* on_equal);
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater);
  virtual void CheckCharacterLT(uc16 limit, Label* on_less);
  virtual void CheckCharacters(Vector<const uc16> str,
                               int cp_offset,
                               Label* on_failure,
                               bool check_end_of_string);
  // A "greedy loop" is a loop that is both greedy and with a simple
  // body. It has a particularly simple implementation.
  virtual void CheckGreedyLoop(Label* on_tos_equals_current_position);
  virtual void CheckNotAtStart(Label* on_not_at_start);
  virtual void CheckNotBackReference(int start_reg, Label* on_no_match);
  virtual void CheckNotBackReferenceIgnoreCase(int start_reg,
                                               Label* on_no_match);
  virtual void CheckNotCharacter(unsigned c, Label* on_not_equal);
  virtual void CheckNotCharacterAfterAnd(unsigned c,
                                         unsigned mask,
                                         Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c,
                                              uc16 minus,
                                              uc16 mask,
                                              Label* on_not_equal);
  virtual void CheckCharacterInRange(uc16 from,
                                     uc16 to,
                                     Label* on_in_range);
  virtual void CheckCharacterNotInRange(uc16 from,
                                        uc16 to,
                                        Label* on_not_in_range);
  virtual void CheckBitInTable(Handle<ByteArray> table, Label* on_bit_set);

  // Checks whether the given offset from the current position is before
  // the end of the string.
  virtual void CheckPosition(int cp_offset, Label* on_outside_input);
  virtual bool CheckSpecialCharacterClass(uc16 type,
                                          Label* on_no_match);
  virtual void Fail();
  virtual Handle<HeapObject> GetCode(Handle<String> source);
  virtual void GoTo(Label* label);
  virtual void IfRegisterGE(int reg, int comparand, Label* if_ge);
  virtual void IfRegisterLT(int reg, int comparand, Label* if_lt);
  virtual void IfRegisterEqPos(int reg, Label* if_eq);
  virtual IrregexpImplementation Implementation();
  virtual void LoadCurrentCharacter(int cp_offset,
                                    Label* on_end_of_input,
                                    bool check_bounds = true,
                                    int characters = 1);
  virtual void PopCurrentPosition();
  virtual void PopRegister(int register_index);
  virtual void PushBacktrack(Label* label);
  virtual void PushCurrentPosition();
  virtual void PushRegister(int register_index,
                            StackCheckFlag check_stack_limit);
  virtual void ReadCurrentPositionFromRegister(int reg);
  virtual void ReadStackPointerFromRegister(int reg);
  virtual void SetCurrentPositionFromEnd(int by);
  virtual void SetRegister(int register_index, int to);
  virtual bool Succeed();
  virtual void WriteCurrentPositionToRegister(int reg, int cp_offset);
  virtual void ClearRegisters(int reg_from, int reg_to);
  virtual void WriteStackPointerToRegister(int reg);
  virtual bool CanReadUnaligned();

  // Called from RegExp if the stack-guard is triggered.
  // If the code object is relocated, the return address is fixed before
  // returning.
  static int CheckStackGuardState(Address* return_address,
                                  Code* re_code,
                                  Address re_frame,
                                  int start_offset,
                                  const byte** input_start,
                                  const byte** input_end);

 private:
  // Above the frame pointer - Stored registers and stack passed parameters.
  // Callee-saved registers x19-x29, where x29 is the old frame pointer.
  static const int kCalleeSavedRegisters = 0;
  // Return address.
  // It is placed above the 11 callee-saved registers.
  static const int kReturnAddress = kCalleeSavedRegisters + 11 * kPointerSize;
  static const int kSecondaryReturnAddress = kReturnAddress + kPointerSize;
  // Stack parameter placed by caller.
  static const int kIsolate = kSecondaryReturnAddress + kPointerSize;

  // Below the frame pointer.
  // Register parameters stored by setup code.
  static const int kDirectCall = kCalleeSavedRegisters - kPointerSize;
  static const int kStackBase = kDirectCall - kPointerSize;
  static const int kOutputSize = kStackBase - kPointerSize;
  static const int kInput = kOutputSize - kPointerSize;
  // When adding local variables remember to push space for them in
  // the frame in GetCode.
  static const int kSuccessCounter = kInput - kPointerSize;
  // First position register address on the stack. Following positions are
  // below it. A position is a 32 bit value.
  static const int kFirstRegisterOnStack = kSuccessCounter - kWRegSize;
  // A capture is a 64 bit value holding two position.
  static const int kFirstCaptureOnStack = kSuccessCounter - kXRegSize;

  // Initial size of code buffer.
  static const size_t kRegExpCodeSize = 1024;

  // When initializing registers to a non-position value we can unroll
  // the loop. Set the limit of registers to unroll.
  static const int kNumRegistersToUnroll = 16;

  // We are using x0 to x7 as a register cache. Each hardware register must
  // contain one capture, that is two 32 bit registers. We can cache at most
  // 16 registers.
  static const int kNumCachedRegisters = 16;

  // Load a number of characters at the given offset from the
  // current position, into the current-character register.
  void LoadCurrentCharacterUnchecked(int cp_offset, int character_count);

  // Check whether preemption has been requested.
  void CheckPreemption();

  // Check whether we are exceeding the stack limit on the backtrack stack.
  void CheckStackLimit();

  // Generate a call to CheckStackGuardState.
  void CallCheckStackGuardState(Register scratch);

  // Location of a 32 bit position register.
  MemOperand register_location(int register_index);

  // Location of a 64 bit capture, combining two position registers.
  MemOperand capture_location(int register_index, Register scratch);

  // Register holding the current input position as negative offset from
  // the end of the string.
  Register current_input_offset() { return w21; }

  // The register containing the current character after LoadCurrentCharacter.
  Register current_character() { return w22; }

  // Register holding address of the end of the input string.
  Register input_end() { return x25; }

  // Register holding address of the start of the input string.
  Register input_start() { return x26; }

  // Register holding the offset from the start of the string where we should
  // start matching.
  Register start_offset() { return w27; }

  // Pointer to the output array's first element.
  Register output_array() { return x28; }

  // Register holding the frame address. Local variables, parameters and
  // regexp registers are addressed relative to this.
  Register frame_pointer() { return fp; }

  // The register containing the backtrack stack top. Provides a meaningful
  // name to the register.
  Register backtrack_stackpointer() { return x23; }

  // Register holding pointer to the current code object.
  Register code_pointer() { return x20; }

  // Register holding the value used for clearing capture registers.
  Register non_position_value() { return w24; }
  // The top 32 bit of this register is used to store this value
  // twice. This is used for clearing more than one register at a time.
  Register twice_non_position_value() { return x24; }

  // Byte size of chars in the string to match (decided by the Mode argument)
  int char_size() { return static_cast<int>(mode_); }

  // Equivalent to a conditional branch to the label, unless the label
  // is NULL, in which case it is a conditional Backtrack.
  void BranchOrBacktrack(Condition condition, Label* to);

  // Compares reg against immmediate before calling BranchOrBacktrack.
  // It makes use of the Cbz and Cbnz instructions.
  void CompareAndBranchOrBacktrack(Register reg,
                                   int immediate,
                                   Condition condition,
                                   Label* to);

  inline void CallIf(Label* to, Condition condition);

  // Save and restore the link register on the stack in a way that
  // is GC-safe.
  inline void SaveLinkRegister();
  inline void RestoreLinkRegister();

  // Pushes the value of a register on the backtrack stack. Decrements the
  // stack pointer by a word size and stores the register's value there.
  inline void Push(Register source);

  // Pops a value from the backtrack stack. Reads the word at the stack pointer
  // and increments it by a word size.
  inline void Pop(Register target);

  // This state indicates where the register actually is.
  enum RegisterState {
    STACKED,     // Resides in memory.
    CACHED_LSW,  // Least Significant Word of a 64 bit hardware register.
    CACHED_MSW   // Most Significant Word of a 64 bit hardware register.
  };

  RegisterState GetRegisterState(int register_index) {
    DCHECK(register_index >= 0);
    if (register_index >= kNumCachedRegisters) {
      return STACKED;
    } else {
      if ((register_index % 2) == 0) {
        return CACHED_LSW;
      } else {
        return CACHED_MSW;
      }
    }
  }

  // Store helper that takes the state of the register into account.
  inline void StoreRegister(int register_index, Register source);

  // Returns a hardware W register that holds the value of the capture
  // register.
  //
  // This function will try to use an existing cache register (w0-w7) for the
  // result. Otherwise, it will load the value into maybe_result.
  //
  // If the returned register is anything other than maybe_result, calling code
  // must not write to it.
  inline Register GetRegister(int register_index, Register maybe_result);

  // Returns the harware register (x0-x7) holding the value of the capture
  // register.
  // This assumes that the state of the register is not STACKED.
  inline Register GetCachedRegister(int register_index);

  Isolate* isolate() const { return masm_->isolate(); }

  MacroAssembler* masm_;

  // Which mode to generate code for (LATIN1 or UC16).
  Mode mode_;

  // One greater than maximal register index actually used.
  int num_registers_;

  // Number of registers to output at the end (the saved registers
  // are always 0..num_saved_registers_-1)
  int num_saved_registers_;

  // Labels used internally.
  Label entry_label_;
  Label start_label_;
  Label success_label_;
  Label backtrack_label_;
  Label exit_label_;
  Label check_preempt_label_;
  Label stack_overflow_label_;
};

#endif  // V8_INTERPRETED_REGEXP


}}  // namespace v8::internal

#endif  // V8_ARM64_REGEXP_MACRO_ASSEMBLER_ARM64_H_
