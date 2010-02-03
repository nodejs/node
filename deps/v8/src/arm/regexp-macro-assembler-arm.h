// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM_REGEXP_MACRO_ASSEMBLER_ARM_H_
#define V8_ARM_REGEXP_MACRO_ASSEMBLER_ARM_H_

namespace v8 {
namespace internal {


#ifndef V8_NATIVE_REGEXP
class RegExpMacroAssemblerARM: public RegExpMacroAssembler {
 public:
  RegExpMacroAssemblerARM();
  virtual ~RegExpMacroAssemblerARM();
};

#else
class RegExpMacroAssemblerARM: public NativeRegExpMacroAssembler {
 public:
  RegExpMacroAssemblerARM(Mode mode, int registers_to_save);
  virtual ~RegExpMacroAssemblerARM();
  virtual int stack_limit_slack();
  virtual void AdvanceCurrentPosition(int by);
  virtual void AdvanceRegister(int reg, int by);
  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void CheckAtStart(Label* on_at_start);
  virtual void CheckCharacter(uint32_t c, Label* on_equal);
  virtual void CheckCharacterAfterAnd(uint32_t c,
                                      uint32_t mask,
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
  virtual void CheckNotRegistersEqual(int reg1, int reg2, Label* on_not_equal);
  virtual void CheckNotCharacter(uint32_t c, Label* on_not_equal);
  virtual void CheckNotCharacterAfterAnd(uint32_t c,
                                         uint32_t mask,
                                         Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c,
                                              uc16 minus,
                                              uc16 mask,
                                              Label* on_not_equal);
  // Checks whether the given offset from the current position is before
  // the end of the string.
  virtual void CheckPosition(int cp_offset, Label* on_outside_input);
  virtual bool CheckSpecialCharacterClass(uc16 type,
                                          Label* on_no_match);
  virtual void Fail();
  virtual Handle<Object> GetCode(Handle<String> source);
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
  virtual void SetRegister(int register_index, int to);
  virtual void Succeed();
  virtual void WriteCurrentPositionToRegister(int reg, int cp_offset);
  virtual void ClearRegisters(int reg_from, int reg_to);
  virtual void WriteStackPointerToRegister(int reg);

  // Called from RegExp if the stack-guard is triggered.
  // If the code object is relocated, the return address is fixed before
  // returning.
  static int CheckStackGuardState(Address* return_address,
                                  Code* re_code,
                                  Address re_frame);
 private:
  // Offsets from frame_pointer() of function parameters and stored registers.
  static const int kFramePointer = 0;

  // Above the frame pointer - Stored registers and stack passed parameters.
  // Register 4..11.
  static const int kStoredRegisters = kFramePointer;
  // Return address (stored from link register, read into pc on return).
  static const int kReturnAddress = kStoredRegisters + 8 * kPointerSize;
  // Stack parameters placed by caller.
  static const int kRegisterOutput = kReturnAddress + kPointerSize;
  static const int kStackHighEnd = kRegisterOutput + kPointerSize;
  static const int kDirectCall = kStackHighEnd + kPointerSize;

  // Below the frame pointer.
  // Register parameters stored by setup code.
  static const int kInputEnd = kFramePointer - kPointerSize;
  static const int kInputStart = kInputEnd - kPointerSize;
  static const int kStartIndex = kInputStart - kPointerSize;
  static const int kInputString = kStartIndex - kPointerSize;
  // When adding local variables remember to push space for them in
  // the frame in GetCode.
  static const int kInputStartMinusOne = kInputString - kPointerSize;
  static const int kAtStart = kInputStartMinusOne - kPointerSize;
  // First register address. Following registers are below it on the stack.
  static const int kRegisterZero = kAtStart - kPointerSize;

  // Initial size of code buffer.
  static const size_t kRegExpCodeSize = 1024;

  static const int kBacktrackConstantPoolSize = 4;

  // Load a number of characters at the given offset from the
  // current position, into the current-character register.
  void LoadCurrentCharacterUnchecked(int cp_offset, int character_count);

  // Check whether preemption has been requested.
  void CheckPreemption();

  // Check whether we are exceeding the stack limit on the backtrack stack.
  void CheckStackLimit();

  void EmitBacktrackConstantPool();
  int GetBacktrackConstantPoolEntry();


  // Generate a call to CheckStackGuardState.
  void CallCheckStackGuardState(Register scratch);

  // The ebp-relative location of a regexp register.
  MemOperand register_location(int register_index);

  // Register holding the current input position as negative offset from
  // the end of the string.
  inline Register current_input_offset() { return r6; }

  // The register containing the current character after LoadCurrentCharacter.
  inline Register current_character() { return r7; }

  // Register holding address of the end of the input string.
  inline Register end_of_input_address() { return r10; }

  // Register holding the frame address. Local variables, parameters and
  // regexp registers are addressed relative to this.
  inline Register frame_pointer() { return fp; }

  // The register containing the backtrack stack top. Provides a meaningful
  // name to the register.
  inline Register backtrack_stackpointer() { return r8; }

  // Register holding pointer to the current code object.
  inline Register code_pointer() { return r5; }

  // Byte size of chars in the string to match (decided by the Mode argument)
  inline int char_size() { return static_cast<int>(mode_); }

  // Equivalent to a conditional branch to the label, unless the label
  // is NULL, in which case it is a conditional Backtrack.
  void BranchOrBacktrack(Condition condition, Label* to);

  // Call and return internally in the generated code in a way that
  // is GC-safe (i.e., doesn't leave absolute code addresses on the stack)
  inline void SafeCall(Label* to, Condition cond = al);
  inline void SafeReturn();
  inline void SafeCallTarget(Label* name);

  // Pushes the value of a register on the backtrack stack. Decrements the
  // stack pointer by a word size and stores the register's value there.
  inline void Push(Register source);

  // Pops a value from the backtrack stack. Reads the word at the stack pointer
  // and increments it by a word size.
  inline void Pop(Register target);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, non-register arguments must be stored in
  // sp[0], sp[4], etc., not pushed. The argument count assumes all arguments
  // are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  inline void FrameAlign(int num_arguments, Register scratch);

  // Calls a C function and cleans up the space for arguments allocated
  // by FrameAlign. The called function is not allowed to trigger a garbage
  // collection.
  inline void CallCFunction(ExternalReference function,
                            int num_arguments);

  // Calls a C function and cleans up the frame alignment done by
  // by FrameAlign. The called function *is* allowed to trigger a garbage
  // collection, but may not take more than four arguments (no arguments
  // passed on the stack), and the first argument will be a pointer to the
  // return address.
  inline void CallCFunctionUsingStub(ExternalReference function,
                                     int num_arguments);


  MacroAssembler* masm_;

  // Which mode to generate code for (ASCII or UC16).
  Mode mode_;

  // One greater than maximal register index actually used.
  int num_registers_;

  // Number of registers to output at the end (the saved registers
  // are always 0..num_saved_registers_-1)
  int num_saved_registers_;

  // Manage a small pre-allocated pool for writing label targets
  // to for pushing backtrack addresses.
  int backtrack_constant_pool_offset_;
  int backtrack_constant_pool_capacity_;

  // Labels used internally.
  Label entry_label_;
  Label start_label_;
  Label success_label_;
  Label backtrack_label_;
  Label exit_label_;
  Label check_preempt_label_;
  Label stack_overflow_label_;
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
  const char* GetName() { return "RegExpCEntryStub"; }
};

#endif  // V8_NATIVE_REGEXP


}}  // namespace v8::internal

#endif  // V8_ARM_REGEXP_MACRO_ASSEMBLER_ARM_H_
