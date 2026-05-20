// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODE_GENERATOR_H_
#define V8_REGEXP_REGEXP_BYTECODE_GENERATOR_H_

#include "src/base/strings.h"
#include "src/codegen/label.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE RegExpBytecodeWriter {
 public:
  explicit RegExpBytecodeWriter(Zone* zone);
  virtual ~RegExpBytecodeWriter() = default;

  // Helpers for peephole optimization.
  template <typename T>
  void OverwriteValue(T value, int absolute_offset);
  // MUST start and end at a bytecode boundary.
  void EmitRawBytecodeStream(const uint8_t* data, int length);
  void EmitRawBytecodeStream(const RegExpBytecodeWriter* src_writer,
                             int src_offset, int length);
  void Finalize(RegExpBytecode bc);

  // Bytecode buffer access.
  // TODO(jgruber): Remove access to details, at least the non-const accessors.
  int pc() const { return pc_; }
  ZoneVector<uint8_t>& buffer() { return buffer_; }
  const ZoneVector<uint8_t>& buffer() const { return buffer_; }

  // Code and bitmap emission.
  template <typename T>
  inline void Emit(T value, int offset);
  inline void EmitBytecode(RegExpBytecode bc);

  // Update bookkeeping at bytecode boundaries.
  inline void ResetPc(int new_pc);
  // Reset all state.
  void Reset();

  // Templated code emission.
  template <RegExpBytecode bytecode, typename... Args>
  void Emit(Args... args);
  template <RegExpBytecodeOperandType OperandType, typename T>
  void EmitOperand(T value, int offset);
  template <RegExpBytecodeOperandType OperandType, typename T>
  auto GetCheckedBasicOperandValue(T value);

  // Runtime versions.
  template <typename T>
  void EmitOperand(RegExpBytecodeOperandType type, T value, int offset);

  int length() const { return pc_; }
  void CopyBufferTo(uint8_t* a) const;

  ZoneMap<int, int>& jump_edges() { return jump_edges_; }
  const ZoneMap<int, int>& jump_edges() const { return jump_edges_; }

  void PatchJump(int target, int absolute_offset);

#ifdef DEBUG
  // Emit padding from start (inclusive) to end (exclusive)
  inline void EmitPadding(int offset);
#define EMIT_PADDING(offset) EmitPadding(offset)
#else
#define EMIT_PADDING(offset) ((void)0)
#endif

 protected:
  // The buffer into which code and relocation info are generated.
  static constexpr int kInitialBufferSizeInBytes = 1 * KB;
  static constexpr size_t kMaxBufferGrowthInBytes = 1 * MB;
  ZoneVector<uint8_t> buffer_;

  // The program counter. Always points at the beginning of a bytecode while
  // we generate the ByteArray. Points to the end when we are done.
  int pc_;

 private:
  // Stores jump edges emitted for the bytecode (used by
  // RegExpBytecodePeepholeOptimization).
  // Key: jump source (offset in buffer_ where jump destination is stored).
  // Value: jump destination (offset in buffer_ to jump to).
  ZoneMap<int, int> jump_edges_;

#ifdef DEBUG
  // End of the bytecode we are currently emitting (exclusive). Absolute value
  // greater than `pc_`.
  int end_of_bc_;
  // Position (absolute) within the current bytecode. This value is updated with
  // every operand and is guaranteed to be between `pc_` and `end_of_bc_`.
  int pc_within_bc_;
#endif

  // TODO(jgruber): Reasonable protected/private organisation once the dust has
  // settled.
  inline void EnsureCapacity(size_t size);
  void ExpandBuffer(size_t new_size);
};

// An assembler/generator for the Irregexp byte code.
class V8_EXPORT_PRIVATE RegExpBytecodeGenerator : public RegExpMacroAssembler,
                                                  public RegExpBytecodeWriter {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // The assembler allocates and grows its own buffer, and buffer_size
  // determines the initial buffer size. The buffer is owned by the assembler
  // and deallocated upon destruction of the assembler.
  RegExpBytecodeGenerator(Isolate* isolate, Zone* zone, Mode mode);
  ~RegExpBytecodeGenerator() override;
  void Bind(Label* label) override;
  void AdvanceCurrentPosition(int by) override;  // Signed cp change.
  void PopCurrentPosition() override;
  void PushCurrentPosition() override;
  void Backtrack() override;
  void GoTo(Label* label) override;
  void PushBacktrack(Label* label) override;
  bool Succeed() override;
  void Fail() override;
  void PopRegister(int register_index) override;
  void PushRegister(int register_index,
                    StackCheckFlag check_stack_limit) override;
  void AdvanceRegister(int register_index, int by) override;  // r[reg] += by.
  void SetCurrentPositionFromEnd(int by) override;
  void SetRegister(int register_index, int to) override;
  void WriteCurrentPositionToRegister(int register_index,
                                      int cp_offset) override;
  void ClearRegisters(int reg_from, int reg_to) override;
  void ReadCurrentPositionFromRegister(int reg) override;
  void WriteStackPointerToRegister(int register_index) override;
  void ReadStackPointerFromRegister(int register_index) override;
  void CheckPosition(int cp_offset, Label* on_outside_input) override;
  void CheckSpecialClassRanges(StandardCharacterSet type,
                               Label* on_no_match) override;
  void LoadCurrentCharacterImpl(int cp_offset, Label* on_end_of_input,
                                bool check_bounds, int characters,
                                int eats_at_least) override;
  void CheckCharacter(unsigned c, Label* on_equal) override;
  void CheckCharacterAfterAnd(unsigned c, unsigned mask,
                              Label* on_equal) override;
  void CheckCharacterGT(base::uc16 limit, Label* on_greater) override;
  void CheckCharacterLT(base::uc16 limit, Label* on_less) override;
  void CheckFixedLengthLoop(Label* on_tos_equals_current_position) override;
  void CheckAtStart(int cp_offset, Label* on_at_start) override;
  void CheckNotAtStart(int cp_offset, Label* on_not_at_start) override;
  void CheckNotCharacter(unsigned c, Label* on_not_equal) override;
  void CheckNotCharacterAfterAnd(unsigned c, unsigned mask,
                                 Label* on_not_equal) override;
  void CheckNotCharacterAfterMinusAnd(base::uc16 c, base::uc16 minus,
                                      base::uc16 mask,
                                      Label* on_not_equal) override;
  void CheckCharacterInRange(base::uc16 from, base::uc16 to,
                             Label* on_in_range) override;
  void CheckCharacterNotInRange(base::uc16 from, base::uc16 to,
                                Label* on_not_in_range) override;
  bool CheckCharacterInRangeArray(const ZoneList<CharacterRange>* ranges,
                                  Label* on_in_range) override {
    // Disabled in the interpreter, because 1) there is no constant pool that
    // could store the ByteArray pointer, 2) bytecode size limits are not as
    // restrictive as code (e.g. branch distances on arm), 3) bytecode for
    // large character classes is already quite compact.
    // TODO(jgruber): Consider using BytecodeArrays (with a constant pool)
    // instead of plain ByteArrays; then we could implement
    // CheckCharacterInRangeArray in the interpreter.
    return false;
  }
  bool CheckCharacterNotInRangeArray(const ZoneList<CharacterRange>* ranges,
                                     Label* on_not_in_range) override {
    return false;
  }
  void CheckBitInTable(Handle<ByteArray> table, Label* on_bit_set) override;
  void SkipUntilBitInTable(int cp_offset, Handle<ByteArray> table,
                           Handle<ByteArray> nibble_table, int advance_by,
                           Label* on_match, Label* on_no_match) override;
  void SkipUntilCharAnd(int cp_offset, int advance_by, unsigned character,
                        unsigned mask, int eats_at_least, Label* on_match,
                        Label* on_no_match) override;
  void SkipUntilChar(int cp_offset, int advance_by, unsigned character,
                     Label* on_match, Label* on_no_match) override;
  void SkipUntilCharPosChecked(int cp_offset, int advance_by,
                               unsigned character, int eats_at_least,
                               Label* on_match, Label* on_no_match) override;
  void SkipUntilCharOrChar(int cp_offset, int advance_by, unsigned char1,
                           unsigned char2, Label* on_match,
                           Label* on_no_match) override;
  void SkipUntilGtOrNotBitInTable(int cp_offset, int advance_by,
                                  unsigned character, Handle<ByteArray> table,
                                  Label* on_match, Label* on_no_match) override;
  void SkipUntilOneOfMasked(int cp_offset, int advance_by, unsigned both_chars,
                            unsigned both_mask, int max_offset, unsigned chars1,
                            unsigned mask1, unsigned chars2, unsigned mask2,
                            Label* on_match1, Label* on_match2,
                            Label* on_failure) override;
  void SkipUntilOneOfMasked3(const SkipUntilOneOfMasked3Args& args) override;
  void CheckNotBackReference(int start_reg, bool read_backward,
                             Label* on_no_match) override;
  void CheckNotBackReferenceIgnoreCase(int start_reg, bool read_backward,
                                       bool unicode,
                                       Label* on_no_match) override;
  void IfRegisterLT(int register_index, int comparand,
                    Label* on_less_than) override;
  void IfRegisterGE(int register_index, int comparand,
                    Label* on_greater_or_equal) override;
  void IfRegisterEqPos(int register_index, Label* on_equal) override;
  void RecordComment(std::string_view comment) override {}
  MacroAssembler* masm() override { return nullptr; }

  IrregexpImplementation Implementation() override;
  DirectHandle<HeapObject> GetCode(DirectHandle<String> source,
                                   RegExpFlags flags) override;

 private:
  template <RegExpBytecode bytecode, typename... Args>
  void Emit(Args... args);
  using RegExpBytecodeWriter::Emit;

  void EmitSkipTable(DirectHandle<ByteArray> table);

  Label backtrack_;

  Isolate* isolate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RegExpBytecodeGenerator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODE_GENERATOR_H_
