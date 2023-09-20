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
                       bool update_feedback);

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
  void StoreRegister(TranslationOpcode opcode, Register reg);
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
    template <typename... T>
    Instruction(TranslationOpcode opcode, T... operands)
        : opcode(opcode),
          operands{operands.value()...}
#ifdef ENABLE_SLOW_DCHECKS
          ,
          is_operand_signed{operands.IsSigned()...}
#endif
    {
    }
    TranslationOpcode opcode;
    // The operands for the instruction. Signed values were static_casted to
    // unsigned.
    uint32_t operands[kMaxTranslationOperandCount];
#ifdef ENABLE_SLOW_DCHECKS
    bool is_operand_signed[kMaxTranslationOperandCount];
#endif
  };

  // Either adds the instruction or increments matching_instructions_count_,
  // depending on whether the instruction matches the corresponding instruction
  // from the previous translation.
  template <typename... T>
  void Add(TranslationOpcode opcode, T... operands);

  // Adds the instruction to contents_, without performing the other steps of
  // Add(). Requires !v8_flags.turbo_compress_translation_arrays.
  template <typename... T>
  void AddRawToContents(TranslationOpcode opcode, T... operands);

  // Adds the instruction to contents_for_compression_, without performing the
  // other steps of Add(). Requires v8_flags.turbo_compress_translation_arrays.
  template <typename... T>
  void AddRawToContentsForCompression(TranslationOpcode opcode, T... operands);

  // Adds a BEGIN instruction to contents_ or contents_for_compression_, but
  // does not update other state. Used by BeginTranslation.
  template <typename... T>
  void AddRawBegin(bool update_feedback, T... operands);

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
