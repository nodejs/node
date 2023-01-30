// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_
#define V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_

#include "src/codegen/register.h"
#include "src/deoptimizer/translation-opcode.h"
#include "src/objects/fixed-array.h"
#include "src/zone/zone-containers.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/value-type.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

class Factory;

// The TranslationArray is the on-heap representation of translations created
// during code generation in a (zone-allocated) TranslationArrayBuilder. The
// translation array specifies how to transform an optimized frame back into
// one or more unoptimized frames.
// TODO(jgruber): Consider a real type instead of this type alias.
using TranslationArray = ByteArray;

class TranslationArrayIterator {
 public:
  TranslationArrayIterator(TranslationArray buffer, int index);

  int32_t NextOperand();

  uint32_t NextOperandUnsigned();

  TranslationOpcode NextOpcode();

  bool HasNextOpcode() const;

  void SkipOperands(int n) {
    for (int i = 0; i < n; i++) NextOperand();
  }

 private:
  TranslationOpcode NextOpcodeAtPreviousIndex();
  uint32_t NextUnsignedOperandAtPreviousIndex();
  void SkipOpcodeAndItsOperandsAtPreviousIndex();

  std::vector<int32_t> uncompressed_contents_;
  TranslationArray buffer_;
  int index_;

  // This decrementing counter indicates how many more times to read operations
  // from the previous translation before continuing to move the index forward.
  int remaining_ops_to_use_from_previous_translation_ = 0;

  // An index into buffer_ for operations starting at a previous BEGIN, which
  // can be used to read operations referred to by MATCH_PREVIOUS_TRANSLATION.
  int previous_index_ = 0;

  // When starting a new MATCH_PREVIOUS_TRANSLATION operation, we'll need to
  // advance the previous_index_ by this many steps.
  int ops_since_previous_index_was_updated_ = 0;
};

class TranslationArrayBuilder {
 public:
  explicit TranslationArrayBuilder(Zone* zone)
      : contents_(zone),
        contents_for_compression_(zone),
        basis_instructions_(zone),
        zone_(zone) {}

  Handle<TranslationArray> ToTranslationArray(Factory* factory);

  int BeginTranslation(int frame_count, int jsframe_count,
                       int update_feedback_count);

  void BeginInterpretedFrame(BytecodeOffset bytecode_offset, int literal_id,
                             unsigned height, int return_value_offset,
                             int return_value_count);
  void BeginInlinedExtraArguments(int literal_id, unsigned height);
  void BeginConstructStubFrame(BytecodeOffset bailout_id, int literal_id,
                               unsigned height);
  void BeginBuiltinContinuationFrame(BytecodeOffset bailout_id, int literal_id,
                                     unsigned height);
#if V8_ENABLE_WEBASSEMBLY
  void BeginJSToWasmBuiltinContinuationFrame(
      BytecodeOffset bailout_id, int literal_id, unsigned height,
      base::Optional<wasm::ValueKind> return_kind);
#endif  // V8_ENABLE_WEBASSEMBLY
  void BeginJavaScriptBuiltinContinuationFrame(BytecodeOffset bailout_id,
                                               int literal_id, unsigned height);
  void BeginJavaScriptBuiltinContinuationWithCatchFrame(
      BytecodeOffset bailout_id, int literal_id, unsigned height);
  void ArgumentsElements(CreateArgumentsType type);
  void ArgumentsLength();
  void BeginCapturedObject(int length);
  void AddUpdateFeedback(int vector_literal, int slot);
  void DuplicateObject(int object_index);
  void StoreRegister(Register reg);
  void StoreInt32Register(Register reg);
  void StoreInt64Register(Register reg);
  void StoreSignedBigInt64Register(Register reg);
  void StoreUnsignedBigInt64Register(Register reg);
  void StoreUint32Register(Register reg);
  void StoreBoolRegister(Register reg);
  void StoreFloatRegister(FloatRegister reg);
  void StoreDoubleRegister(DoubleRegister reg);
  void StoreStackSlot(int index);
  void StoreInt32StackSlot(int index);
  void StoreInt64StackSlot(int index);
  void StoreSignedBigInt64StackSlot(int index);
  void StoreUnsignedBigInt64StackSlot(int index);
  void StoreUint32StackSlot(int index);
  void StoreBoolStackSlot(int index);
  void StoreFloatStackSlot(int index);
  void StoreDoubleStackSlot(int index);
  void StoreLiteral(int literal_id);
  void StoreOptimizedOut();
  void StoreJSFrameFunction();

 private:
  struct Instruction {
    Instruction() = default;
    bool operator==(const Instruction& other) const {
      static_assert(kMaxTranslationOperandCount == 5);
      return opcode == other.opcode && operands[0] == other.operands[0] &&
             operands[1] == other.operands[1] &&
             operands[2] == other.operands[2] &&
             operands[3] == other.operands[3] &&
             operands[4] == other.operands[4];
    }
    TranslationOpcode opcode;
    // The operands for the instruction. If turbo_compress_translation_arrays is
    // set, then signed values were static_casted to unsigned. Otherwise, signed
    // values have been encoded by base::VLQConvertToUnsigned. Operands not used
    // by this instruction are zero.
    uint32_t operands[kMaxTranslationOperandCount];
  };

  // Either adds the instruction or increments matching_instructions_count_,
  // depending on whether the instruction matches the corresponding instruction
  // from the previous translation.
  void Add(const Instruction& instruction, int value_count);

  void AddRawSigned(int32_t value);
  void AddRawUnsigned(uint32_t value);

  // Convenience methods which wrap calls to Add(). Unsigned versions are
  // generally preferable if the data is known to be unsigned.
  void AddWithNoOperands(TranslationOpcode opcode);
  void AddWithSignedOperand(TranslationOpcode opcode, int32_t operand);
  void AddWithSignedOperands(int operand_count, TranslationOpcode opcode,
                             int32_t operand_1, int32_t operand_2,
                             int32_t operand_3 = 0, int32_t operand_4 = 0,
                             int32_t operand_5 = 0);
  void AddWithUnsignedOperand(TranslationOpcode opcode, uint32_t operand);
  void AddWithUnsignedOperands(int operand_count, TranslationOpcode opcode,
                               uint32_t operand_1 = 0, uint32_t operand_2 = 0,
                               uint32_t operand_3 = 0, uint32_t operand_4 = 0,
                               uint32_t operand_5 = 0);

  int Size() const {
    return V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)
               ? static_cast<int>(contents_for_compression_.size())
               : static_cast<int>(contents_.size());
  }
  int SizeInBytes() const {
    return V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)
               ? Size() * kInt32Size
               : Size();
  }

  Zone* zone() const { return zone_; }

  void FinishPendingInstructionIfNeeded();

  ZoneVector<uint8_t> contents_;
  ZoneVector<int32_t> contents_for_compression_;
  // If match_previous_allowed_ is false, then this vector contains the
  // instructions written so far in the current translation (since the last
  // BEGIN). If match_previous_allowed_ is true, then this vector contains the
  // instructions from the basis translation (the one written with
  // !match_previous_allowed_). This allows Add() to easily check whether a
  // newly added instruction matches the corresponding one from the basis
  // translation.
  ZoneVector<Instruction> basis_instructions_;
#ifdef ENABLE_SLOW_DCHECKS
  std::vector<Instruction> all_instructions_;
#endif
  Zone* const zone_;
  // How many consecutive instructions we've skipped writing because they match
  // the basis translation.
  size_t matching_instructions_count_ = 0;
  size_t total_matching_instructions_in_current_translation_ = 0;
  // The current index within basis_instructions_.
  size_t instruction_index_within_translation_ = 0;
  // The byte index within the contents_ array of the BEGIN instruction for the
  // basis translation (the most recent translation which was fully written out,
  // not using MATCH_PREVIOUS_TRANSLATION instructions).
  int index_of_basis_translation_start_ = 0;
  // Whether the builder can use MATCH_PREVIOUS_TRANSLATION in the current
  // translation.
  bool match_previous_allowed_ = true;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_TRANSLATION_ARRAY_H_
