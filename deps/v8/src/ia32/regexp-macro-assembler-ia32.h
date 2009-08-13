// Copyright 2008-2009 the V8 project authors. All rights reserved.
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

#ifndef V8_IA32_REGEXP_MACRO_ASSEMBLER_IA32_H_
#define V8_IA32_REGEXP_MACRO_ASSEMBLER_IA32_H_

namespace v8 {
namespace internal {

class RegExpMacroAssemblerIA32: public RegExpMacroAssembler {
 public:
  // Type of input string to generate code for.
  enum Mode { ASCII = 1, UC16 = 2 };
  // Result of calling the generated RegExp code:
  // RETRY: Something significant changed during execution, and the matching
  //        should be retried from scratch.
  // EXCEPTION: Something failed during execution. If no exception has been
  //        thrown, it's an internal out-of-memory, and the caller should
  //        throw the exception.
  // FAILURE: Matching failed.
  // SUCCESS: Matching succeeded, and the output array has been filled with
  //        capture positions.
  enum Result { RETRY = -2, EXCEPTION = -1, FAILURE = 0, SUCCESS = 1 };

  RegExpMacroAssemblerIA32(Mode mode, int registers_to_save);
  virtual ~RegExpMacroAssemblerIA32();
  virtual int stack_limit_slack();
  virtual void AdvanceCurrentPosition(int by);
  virtual void AdvanceRegister(int reg, int by);
  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void CheckAtStart(Label* on_at_start);
  virtual void CheckBitmap(uc16 start, Label* bitmap, Label* on_zero);
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
                                          int cp_offset,
                                          bool check_offset,
                                          Label* on_no_match);
  virtual void DispatchByteMap(uc16 start,
                               Label* byte_map,
                               const Vector<Label*>& destinations);
  virtual void DispatchHalfNibbleMap(uc16 start,
                                     Label* half_nibble_map,
                                     const Vector<Label*>& destinations);
  virtual void DispatchHighByteMap(byte start,
                                   Label* byte_map,
                                   const Vector<Label*>& destinations);
  virtual void EmitOrLink(Label* label);
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

  static Result Match(Handle<Code> regexp,
                      Handle<String> subject,
                      int* offsets_vector,
                      int offsets_vector_length,
                      int previous_index);

  static Result Execute(Code* code,
                        String* input,
                        int start_offset,
                        const byte* input_start,
                        const byte* input_end,
                        int* output,
                        bool at_start);

 private:
  // Offsets from ebp of function parameters and stored registers.
  static const int kFramePointer = 0;
  // Above the frame pointer - function parameters and return address.
  static const int kReturn_eip = kFramePointer + kPointerSize;
  static const int kFrameAlign = kReturn_eip + kPointerSize;
  // Parameters.
  static const int kInputString = kFrameAlign;
  static const int kStartIndex = kInputString + kPointerSize;
  static const int kInputStart = kStartIndex + kPointerSize;
  static const int kInputEnd = kInputStart + kPointerSize;
  static const int kRegisterOutput = kInputEnd + kPointerSize;
  static const int kAtStart = kRegisterOutput + kPointerSize;
  static const int kStackHighEnd = kAtStart + kPointerSize;
  // Below the frame pointer - local stack variables.
  // When adding local variables remember to push space for them in
  // the frame in GetCode.
  static const int kBackup_esi = kFramePointer - kPointerSize;
  static const int kBackup_edi = kBackup_esi - kPointerSize;
  static const int kBackup_ebx = kBackup_edi - kPointerSize;
  static const int kInputStartMinusOne = kBackup_ebx - kPointerSize;
  // First register address. Following registers are below it on the stack.
  static const int kRegisterZero = kInputStartMinusOne - kPointerSize;

  // Initial size of code buffer.
  static const size_t kRegExpCodeSize = 1024;
  // Initial size of constant buffers allocated during compilation.
  static const int kRegExpConstantsSize = 256;

  static const byte* StringCharacterPosition(String* subject, int start_index);

  // Compares two-byte strings case insensitively.
  // Called from generated RegExp code.
  static int CaseInsensitiveCompareUC16(Address byte_offset1,
                                        Address byte_offset2,
                                        size_t byte_length);

  // Load a number of characters at the given offset from the
  // current position, into the current-character register.
  void LoadCurrentCharacterUnchecked(int cp_offset, int character_count);

  // Check whether preemption has been requested.
  void CheckPreemption();

  // Check whether we are exceeding the stack limit on the backtrack stack.
  void CheckStackLimit();

  // Called from RegExp if the stack-guard is triggered.
  // If the code object is relocated, the return address is fixed before
  // returning.
  static int CheckStackGuardState(Address* return_address,
                                  Code* re_code,
                                  Address re_frame);

  // Generate a call to CheckStackGuardState.
  void CallCheckStackGuardState(Register scratch);

  // Called from RegExp if the backtrack stack limit is hit.
  // Tries to expand the stack. Returns the new stack-pointer if
  // successful, and updates the stack_top address, or returns 0 if unable
  // to grow the stack.
  // This function must not trigger a garbage collection.
  static Address GrowStack(Address stack_pointer, Address* stack_top);

  // The ebp-relative location of a regexp register.
  Operand register_location(int register_index);

  // The register containing the current character after LoadCurrentCharacter.
  inline Register current_character() { return edx; }

  // The register containing the backtrack stack top. Provides a meaningful
  // name to the register.
  inline Register backtrack_stackpointer() { return ecx; }

  // Byte size of chars in the string to match (decided by the Mode argument)
  inline int char_size() { return static_cast<int>(mode_); }

  // Equivalent to a conditional branch to the label, unless the label
  // is NULL, in which case it is a conditional Backtrack.
  void BranchOrBacktrack(Condition condition, Label* to, Hint hint = no_hint);

  // Load the address of a "constant buffer" (a slice of a byte array)
  // into a register. The address is computed from the ByteArray* address
  // and an offset. Uses no extra registers.
  void LoadConstantBufferAddress(Register reg, ArraySlice* buffer);

  // Call and return internally in the generated code in a way that
  // is GC-safe (i.e., doesn't leave absolute code addresses on the stack)
  inline void SafeCall(Label* to);
  inline void SafeReturn();
  inline void SafeCallTarget(Label* name);

  // Pushes the value of a register on the backtrack stack. Decrements the
  // stack pointer (ecx) by a word size and stores the register's value there.
  inline void Push(Register source);

  // Pushes a value on the backtrack stack. Decrements the stack pointer (ecx)
  // by a word size and stores the value there.
  inline void Push(Immediate value);

  // Pops a value from the backtrack stack. Reads the word at the stack pointer
  // (ecx) and increments it by a word size.
  inline void Pop(Register target);

  // Before calling a C-function from generated code, align arguments on stack.
  // After aligning the frame, arguments must be stored in esp[0], esp[4],
  // etc., not pushed. The argument count assumes all arguments are word sized.
  // Some compilers/platforms require the stack to be aligned when calling
  // C++ code.
  // Needs a scratch register to do some arithmetic. This register will be
  // trashed.
  inline void FrameAlign(int num_arguments, Register scratch);

  // Calls a C function and cleans up the space for arguments allocated
  // by FrameAlign. The called function is not allowed to trigger a garbage
  // collection, since that might move the code and invalidate the return
  // address (unless this is somehow accounted for).
  inline void CallCFunction(Address function_address, int num_arguments);

  MacroAssembler* masm_;

  // Constant buffer provider. Allocates external storage for storing
  // constants.
  ByteArrayProvider constants_;

  // Which mode to generate code for (ASCII or UC16).
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

}}  // namespace v8::internal

#endif  // V8_IA32_REGEXP_MACRO_ASSEMBLER_IA32_H_
