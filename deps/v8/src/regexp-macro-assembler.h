// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef V8_REGEXP_MACRO_ASSEMBLER_H_
#define V8_REGEXP_MACRO_ASSEMBLER_H_

namespace v8 {
namespace internal {

struct DisjunctDecisionRow {
  RegExpCharacterClass cc;
  Label* on_match;
};


class RegExpMacroAssembler {
 public:
  // The implementation must be able to handle at least:
  static const int kMaxRegister = (1 << 16) - 1;
  static const int kMaxCPOffset = (1 << 15) - 1;
  static const int kMinCPOffset = -(1 << 15);
  enum IrregexpImplementation {
    kIA32Implementation,
    kARMImplementation,
    kBytecodeImplementation
  };

  enum StackCheckFlag {
    kNoStackLimitCheck = false,
    kCheckStackLimit = true
  };

  RegExpMacroAssembler();
  virtual ~RegExpMacroAssembler();
  // The maximal number of pushes between stack checks. Users must supply
  // kCheckStackLimit flag to push operations (instead of kNoStackLimitCheck)
  // at least once for every stack_limit() pushes that are executed.
  virtual int stack_limit_slack() = 0;
  virtual void AdvanceCurrentPosition(int by) = 0;  // Signed cp change.
  virtual void AdvanceRegister(int reg, int by) = 0;  // r[reg] += by.
  // Continues execution from the position pushed on the top of the backtrack
  // stack by an earlier PushBacktrack(Label*).
  virtual void Backtrack() = 0;
  virtual void Bind(Label* label) = 0;
  virtual void CheckAtStart(Label* on_at_start) = 0;
  // Check the current character against a bitmap.  The range of the current
  // character must be from start to start + length_of_bitmap_in_bits.
  virtual void CheckBitmap(
      uc16 start,           // The bitmap is indexed from this character.
      Label* bitmap,        // Where the bitmap is emitted.
      Label* on_zero) = 0;  // Where to go if the bit is 0.  Fall through on 1.
  // Dispatch after looking the current character up in a 2-bits-per-entry
  // map.  The destinations vector has up to 4 labels.
  virtual void CheckCharacter(uint32_t c, Label* on_equal) = 0;
  // Bitwise and the current character with the given constant and then
  // check for a match with c.
  virtual void CheckCharacterAfterAnd(uint32_t c,
                                      uint32_t and_with,
                                      Label* on_equal) = 0;
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater) = 0;
  virtual void CheckCharacterLT(uc16 limit, Label* on_less) = 0;
  // Check the current character for a match with a literal string.  If we
  // fail to match then goto the on_failure label.  If check_eos is set then
  // the end of input always fails.  If check_eos is clear then it is the
  // caller's responsibility to ensure that the end of string is not hit.
  // If the label is NULL then we should pop a backtrack address off
  // the stack and go to that.
  virtual void CheckCharacters(
      Vector<const uc16> str,
      int cp_offset,
      Label* on_failure,
      bool check_eos) = 0;
  virtual void CheckGreedyLoop(Label* on_tos_equals_current_position) = 0;
  virtual void CheckNotAtStart(Label* on_not_at_start) = 0;
  virtual void CheckNotBackReference(int start_reg, Label* on_no_match) = 0;
  virtual void CheckNotBackReferenceIgnoreCase(int start_reg,
                                               Label* on_no_match) = 0;
  // Check the current character for a match with a literal character.  If we
  // fail to match then goto the on_failure label.  End of input always
  // matches.  If the label is NULL then we should pop a backtrack address off
  // the stack and go to that.
  virtual void CheckNotCharacter(uint32_t c, Label* on_not_equal) = 0;
  virtual void CheckNotCharacterAfterAnd(uint32_t c,
                                         uint32_t and_with,
                                         Label* on_not_equal) = 0;
  // Subtract a constant from the current character, then or with the given
  // constant and then check for a match with c.
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c,
                                              uc16 minus,
                                              uc16 and_with,
                                              Label* on_not_equal) = 0;
  virtual void CheckNotRegistersEqual(int reg1,
                                      int reg2,
                                      Label* on_not_equal) = 0;

  // Checks whether the given offset from the current position is before
  // the end of the string.  May overwrite the current character.
  virtual void CheckPosition(int cp_offset, Label* on_outside_input) {
    LoadCurrentCharacter(cp_offset, on_outside_input, true);
  }
  // Check whether a standard/default character class matches the current
  // character. Returns false if the type of special character class does
  // not have custom support.
  // May clobber the current loaded character.
  virtual bool CheckSpecialCharacterClass(uc16 type,
                                          int cp_offset,
                                          bool check_offset,
                                          Label* on_no_match) {
    return false;
  }
  // Dispatch after looking the current character up in a byte map.  The
  // destinations vector has up to 256 labels.
  virtual void DispatchByteMap(
      uc16 start,
      Label* byte_map,
      const Vector<Label*>& destinations) = 0;
  virtual void DispatchHalfNibbleMap(
      uc16 start,
      Label* half_nibble_map,
      const Vector<Label*>& destinations) = 0;
  // Dispatch after looking the high byte of the current character up in a byte
  // map.  The destinations vector has up to 256 labels.
  virtual void DispatchHighByteMap(
      byte start,
      Label* byte_map,
      const Vector<Label*>& destinations) = 0;
  virtual void EmitOrLink(Label* label) = 0;
  virtual void Fail() = 0;
  virtual Handle<Object> GetCode(Handle<String> source) = 0;
  virtual void GoTo(Label* label) = 0;
  // Check whether a register is >= a given constant and go to a label if it
  // is.  Backtracks instead if the label is NULL.
  virtual void IfRegisterGE(int reg, int comparand, Label* if_ge) = 0;
  // Check whether a register is < a given constant and go to a label if it is.
  // Backtracks instead if the label is NULL.
  virtual void IfRegisterLT(int reg, int comparand, Label* if_lt) = 0;
  // Check whether a register is == to the current position and go to a
  // label if it is.
  virtual void IfRegisterEqPos(int reg, Label* if_eq) = 0;
  virtual IrregexpImplementation Implementation() = 0;
  virtual void LoadCurrentCharacter(int cp_offset,
                                    Label* on_end_of_input,
                                    bool check_bounds = true,
                                    int characters = 1) = 0;
  virtual void PopCurrentPosition() = 0;
  virtual void PopRegister(int register_index) = 0;
  // Pushes the label on the backtrack stack, so that a following Backtrack
  // will go to this label. Always checks the backtrack stack limit.
  virtual void PushBacktrack(Label* label) = 0;
  virtual void PushCurrentPosition() = 0;
  virtual void PushRegister(int register_index,
                            StackCheckFlag check_stack_limit) = 0;
  virtual void ReadCurrentPositionFromRegister(int reg) = 0;
  virtual void ReadStackPointerFromRegister(int reg) = 0;
  virtual void SetRegister(int register_index, int to) = 0;
  virtual void Succeed() = 0;
  virtual void WriteCurrentPositionToRegister(int reg, int cp_offset) = 0;
  virtual void ClearRegisters(int reg_from, int reg_to) = 0;
  virtual void WriteStackPointerToRegister(int reg) = 0;

 private:
};


struct ArraySlice {
 public:
  ArraySlice(Handle<ByteArray> array, size_t offset)
    : array_(array), offset_(offset) {}
  Handle<ByteArray> array() { return array_; }
  // Offset in the byte array data.
  size_t offset() { return offset_; }
  // Offset from the ByteArray pointer.
  size_t base_offset() {
    return ByteArray::kHeaderSize - kHeapObjectTag + offset_;
  }
  void* location() {
    return reinterpret_cast<void*>(array_->GetDataStartAddress() + offset_);
  }
  template <typename T>
  T& at(int idx) {
    return reinterpret_cast<T*>(array_->GetDataStartAddress() + offset_)[idx];
  }
 private:
  Handle<ByteArray> array_;
  size_t offset_;
};


class ByteArrayProvider {
 public:
  explicit ByteArrayProvider(unsigned int initial_size);
  // Provides a place to put "size" elements of size "element_size".
  // The information can be stored in the provided ByteArray at the "offset".
  // The offset is aligned to the element size.
  ArraySlice GetBuffer(unsigned int size,
                       unsigned int element_size);
  template <typename T>
  ArraySlice GetBuffer(Vector<T> values);
 private:
  size_t byte_array_size_;
  Handle<ByteArray> current_byte_array_;
  int current_byte_array_free_offset_;
};

} }  // namespace v8::internal

#endif  // V8_REGEXP_MACRO_ASSEMBLER_H_
