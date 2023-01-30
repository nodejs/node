// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/translation-array.h"

#include "src/base/vlq.h"
#include "src/deoptimizer/translated-state.h"
#include "src/objects/fixed-array-inl.h"

#ifdef V8_USE_ZLIB
#include "third_party/zlib/google/compression_utils_portable.h"
#endif  // V8_USE_ZLIB

namespace v8 {
namespace internal {

namespace {

#ifdef V8_USE_ZLIB
// Constants describing compressed TranslationArray layout. Only relevant if
// --turbo-compress-translation-arrays is enabled.
constexpr int kUncompressedSizeOffset = 0;
constexpr int kUncompressedSizeSize = kInt32Size;
constexpr int kCompressedDataOffset =
    kUncompressedSizeOffset + kUncompressedSizeSize;
constexpr int kTranslationArrayElementSize = kInt32Size;
#endif  // V8_USE_ZLIB

}  // namespace

TranslationArrayIterator::TranslationArrayIterator(TranslationArray buffer,
                                                   int index)
    : buffer_(buffer), index_(index) {
#ifdef V8_USE_ZLIB
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    const int size = buffer_.get_int(kUncompressedSizeOffset);
    uncompressed_contents_.insert(uncompressed_contents_.begin(), size, 0);

    uLongf uncompressed_size = size * kTranslationArrayElementSize;

    CHECK_EQ(zlib_internal::UncompressHelper(
                 zlib_internal::ZRAW,
                 base::bit_cast<Bytef*>(uncompressed_contents_.data()),
                 &uncompressed_size,
                 buffer_.GetDataStartAddress() + kCompressedDataOffset,
                 buffer_.DataSize()),
             Z_OK);
    DCHECK(index >= 0 && index < size);
    return;
  }
#endif  // V8_USE_ZLIB
  DCHECK(!v8_flags.turbo_compress_translation_arrays);
  DCHECK(index >= 0 && index < buffer.length());
  // Starting at a location other than a BEGIN would make
  // MATCH_PREVIOUS_TRANSLATION instructions not work.
  DCHECK_EQ(buffer_.GetDataStartAddress()[index],
            static_cast<byte>(TranslationOpcode::BEGIN));
}

int32_t TranslationArrayIterator::NextOperand() {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    return uncompressed_contents_[index_++];
  } else if (remaining_ops_to_use_from_previous_translation_) {
    int32_t value =
        base::VLQDecode(buffer_.GetDataStartAddress(), &previous_index_);
    DCHECK_LT(previous_index_, index_);
    return value;
  } else {
    int32_t value = base::VLQDecode(buffer_.GetDataStartAddress(), &index_);
    DCHECK_LE(index_, buffer_.length());
    return value;
  }
}

TranslationOpcode TranslationArrayIterator::NextOpcodeAtPreviousIndex() {
  TranslationOpcode opcode =
      static_cast<TranslationOpcode>(buffer_.get(previous_index_++));
  DCHECK_LT(static_cast<uint32_t>(opcode), kNumTranslationOpcodes);
  DCHECK_NE(opcode, TranslationOpcode::MATCH_PREVIOUS_TRANSLATION);
  DCHECK_LT(previous_index_, index_);
  return opcode;
}

uint32_t TranslationArrayIterator::NextUnsignedOperandAtPreviousIndex() {
  uint32_t value =
      base::VLQDecodeUnsigned(buffer_.GetDataStartAddress(), &previous_index_);
  DCHECK_LT(previous_index_, index_);
  return value;
}

uint32_t TranslationArrayIterator::NextOperandUnsigned() {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    return uncompressed_contents_[index_++];
  } else if (remaining_ops_to_use_from_previous_translation_) {
    return NextUnsignedOperandAtPreviousIndex();
  } else {
    uint32_t value =
        base::VLQDecodeUnsigned(buffer_.GetDataStartAddress(), &index_);
    DCHECK_LE(index_, buffer_.length());
    return value;
  }
}

TranslationOpcode TranslationArrayIterator::NextOpcode() {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    return static_cast<TranslationOpcode>(NextOperandUnsigned());
  }
  if (remaining_ops_to_use_from_previous_translation_) {
    --remaining_ops_to_use_from_previous_translation_;
  }
  if (remaining_ops_to_use_from_previous_translation_) {
    return NextOpcodeAtPreviousIndex();
  }
  TranslationOpcode opcode =
      static_cast<TranslationOpcode>(buffer_.get(index_++));
  DCHECK_LE(index_, buffer_.length());
  DCHECK_LT(static_cast<uint32_t>(opcode), kNumTranslationOpcodes);
  if (opcode == TranslationOpcode::BEGIN) {
    int temp_index = index_;
    // The first argument for BEGIN is the distance, in bytes, since the
    // previous BEGIN, or zero to indicate that MATCH_PREVIOUS_TRANSLATION will
    // not be used in this translation.
    uint32_t lookback_distance =
        base::VLQDecodeUnsigned(buffer_.GetDataStartAddress(), &temp_index);
    if (lookback_distance) {
      previous_index_ = index_ - 1 - lookback_distance;
      DCHECK_EQ(buffer_.get(previous_index_),
                static_cast<byte>(TranslationOpcode::BEGIN));
      // The previous BEGIN should specify zero as its lookback distance,
      // meaning it won't use MATCH_PREVIOUS_TRANSLATION.
      DCHECK_EQ(buffer_.get(previous_index_ + 1), 0);
    }
    ops_since_previous_index_was_updated_ = 1;
  } else if (opcode == TranslationOpcode::MATCH_PREVIOUS_TRANSLATION) {
    remaining_ops_to_use_from_previous_translation_ = NextOperandUnsigned();
    for (int i = 0; i < ops_since_previous_index_was_updated_; ++i) {
      SkipOpcodeAndItsOperandsAtPreviousIndex();
    }
    ops_since_previous_index_was_updated_ = 0;
    opcode = NextOpcodeAtPreviousIndex();
  } else {
    ++ops_since_previous_index_was_updated_;
  }
  return opcode;
}

bool TranslationArrayIterator::HasNextOpcode() const {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    return index_ < static_cast<int>(uncompressed_contents_.size());
  } else {
    return index_ < buffer_.length() ||
           remaining_ops_to_use_from_previous_translation_ > 1;
  }
}

void TranslationArrayIterator::SkipOpcodeAndItsOperandsAtPreviousIndex() {
  TranslationOpcode opcode = NextOpcodeAtPreviousIndex();
  for (int count = TranslationOpcodeOperandCount(opcode); count != 0; --count) {
    NextUnsignedOperandAtPreviousIndex();
  }
}

int TranslationArrayBuilder::BeginTranslation(int frame_count,
                                              int jsframe_count,
                                              int update_feedback_count) {
  FinishPendingInstructionIfNeeded();
  int start_index = Size();
  auto opcode = TranslationOpcode::BEGIN;
  int distance_from_last_start = 0;

  // We should reuse an existing basis translation if:
  // - we just finished writing the basis translation
  //   (match_previous_allowed_ is false), or
  // - the translation we just finished was moderately successful at reusing
  //   instructions from the basis translation. We'll define "moderately
  //   successful" as reusing more than 3/4 of the basis instructions.
  // Otherwise we should reset and write a new basis translation. At the
  // beginning, match_previous_allowed_ is initialized to true so that this
  // logic decides to start a new basis translation.
  if (!match_previous_allowed_ ||
      total_matching_instructions_in_current_translation_ >
          instruction_index_within_translation_ / 4 * 3) {
    // Use the existing basis translation.
    distance_from_last_start = start_index - index_of_basis_translation_start_;
    match_previous_allowed_ = true;
  } else {
    // Abandon the existing basis translation and write a new one.
    basis_instructions_.clear();
    index_of_basis_translation_start_ = start_index;
    match_previous_allowed_ = false;
  }

  total_matching_instructions_in_current_translation_ = 0;
  instruction_index_within_translation_ = 0;

  // BEGIN instructions can't be replaced by MATCH_PREVIOUS_TRANSLATION, so
  // write the data directly rather than calling Add().
  AddRawUnsigned(static_cast<uint32_t>(opcode));
  AddRawUnsigned(distance_from_last_start);
  AddRawSigned(frame_count);
  AddRawSigned(jsframe_count);
  AddRawSigned(update_feedback_count);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 4);
#ifdef ENABLE_SLOW_DCHECKS
  if (v8_flags.enable_slow_asserts) {
    Instruction instruction;
    instruction.opcode = opcode;
    instruction.operands[0] = distance_from_last_start;
    instruction.operands[1] = base::VLQConvertToUnsigned(frame_count);
    instruction.operands[2] = base::VLQConvertToUnsigned(jsframe_count);
    instruction.operands[3] = base::VLQConvertToUnsigned(update_feedback_count);
    instruction.operands[4] = 0;
    all_instructions_.push_back(instruction);
  }
#endif
  return start_index;
}

void TranslationArrayBuilder::AddRawSigned(int32_t value) {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    contents_for_compression_.push_back(value);
  } else {
    base::VLQEncode(&contents_, value);
  }
}

void TranslationArrayBuilder::AddRawUnsigned(uint32_t value) {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    contents_for_compression_.push_back(value);
  } else {
    base::VLQEncodeUnsigned(&contents_, value);
  }
}

void TranslationArrayBuilder::FinishPendingInstructionIfNeeded() {
  if (matching_instructions_count_) {
    total_matching_instructions_in_current_translation_ +=
        matching_instructions_count_;
    contents_.push_back(
        static_cast<byte>(TranslationOpcode::MATCH_PREVIOUS_TRANSLATION));
    base::VLQEncodeUnsigned(
        &contents_, static_cast<uint32_t>(matching_instructions_count_));
    matching_instructions_count_ = 0;
  }
}

void TranslationArrayBuilder::Add(
    const TranslationArrayBuilder::Instruction& instruction, int value_count) {
  DCHECK_EQ(value_count, TranslationOpcodeOperandCount(instruction.opcode));
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    AddRawUnsigned(static_cast<byte>(instruction.opcode));
    for (int i = 0; i < value_count; ++i) {
      AddRawUnsigned(instruction.operands[i]);
    }
    return;
  }
#ifdef ENABLE_SLOW_DCHECKS
  if (v8_flags.enable_slow_asserts) {
    all_instructions_.push_back(instruction);
  }
#endif
  if (match_previous_allowed_ &&
      instruction_index_within_translation_ < basis_instructions_.size() &&
      instruction ==
          basis_instructions_[instruction_index_within_translation_]) {
    ++matching_instructions_count_;
  } else {
    FinishPendingInstructionIfNeeded();
    AddRawUnsigned(static_cast<byte>(instruction.opcode));
    for (int i = 0; i < value_count; ++i) {
      AddRawUnsigned(instruction.operands[i]);
    }
    if (!match_previous_allowed_) {
      DCHECK_EQ(basis_instructions_.size(),
                instruction_index_within_translation_);
      basis_instructions_.push_back(instruction);
    }
  }
  ++instruction_index_within_translation_;
}

void TranslationArrayBuilder::AddWithNoOperands(TranslationOpcode opcode) {
  AddWithUnsignedOperands(0, opcode);
}

void TranslationArrayBuilder::AddWithSignedOperand(TranslationOpcode opcode,
                                                   int32_t operand) {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    AddWithUnsignedOperand(opcode, operand);
  } else {
    AddWithUnsignedOperand(opcode, base::VLQConvertToUnsigned(operand));
  }
}

void TranslationArrayBuilder::AddWithSignedOperands(
    int operand_count, TranslationOpcode opcode, int32_t operand_1,
    int32_t operand_2, int32_t operand_3, int32_t operand_4,
    int32_t operand_5) {
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    AddWithUnsignedOperands(operand_count, opcode, operand_1, operand_2,
                            operand_3, operand_4, operand_5);
  } else {
    AddWithUnsignedOperands(operand_count, opcode,
                            base::VLQConvertToUnsigned(operand_1),
                            base::VLQConvertToUnsigned(operand_2),
                            base::VLQConvertToUnsigned(operand_3),
                            base::VLQConvertToUnsigned(operand_4),
                            base::VLQConvertToUnsigned(operand_5));
  }
}

void TranslationArrayBuilder::AddWithUnsignedOperand(TranslationOpcode opcode,
                                                     uint32_t operand) {
  AddWithUnsignedOperands(1, opcode, operand);
}

void TranslationArrayBuilder::AddWithUnsignedOperands(
    int operand_count, TranslationOpcode opcode, uint32_t operand_1,
    uint32_t operand_2, uint32_t operand_3, uint32_t operand_4,
    uint32_t operand_5) {
  Instruction instruction;
  instruction.opcode = opcode;
  instruction.operands[0] = operand_1;
  instruction.operands[1] = operand_2;
  instruction.operands[2] = operand_3;
  instruction.operands[3] = operand_4;
  instruction.operands[4] = operand_5;
  Add(instruction, operand_count);
}

Handle<TranslationArray> TranslationArrayBuilder::ToTranslationArray(
    Factory* factory) {
#ifdef V8_USE_ZLIB
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    const int input_size = SizeInBytes();
    uLongf compressed_data_size = compressBound(input_size);

    ZoneVector<byte> compressed_data(compressed_data_size, zone());

    CHECK_EQ(
        zlib_internal::CompressHelper(
            zlib_internal::ZRAW, compressed_data.data(), &compressed_data_size,
            base::bit_cast<const Bytef*>(contents_for_compression_.data()),
            input_size, Z_DEFAULT_COMPRESSION, nullptr, nullptr),
        Z_OK);

    const int translation_array_size =
        static_cast<int>(compressed_data_size) + kUncompressedSizeSize;
    Handle<TranslationArray> result =
        factory->NewByteArray(translation_array_size, AllocationType::kOld);

    result->set_int(kUncompressedSizeOffset, Size());
    std::memcpy(result->GetDataStartAddress() + kCompressedDataOffset,
                compressed_data.data(), compressed_data_size);

    return result;
  }
#endif
  DCHECK(!v8_flags.turbo_compress_translation_arrays);
  FinishPendingInstructionIfNeeded();
  Handle<TranslationArray> result =
      factory->NewByteArray(SizeInBytes(), AllocationType::kOld);
  memcpy(result->GetDataStartAddress(), contents_.data(),
         contents_.size() * sizeof(uint8_t));
#ifdef ENABLE_SLOW_DCHECKS
  if (v8_flags.enable_slow_asserts) {
    // Check that we can read back all of the same content we intended to write.
    TranslationArrayIterator it(*result, 0);
    for (size_t i = 0; i < all_instructions_.size(); ++i) {
      CHECK(it.HasNextOpcode());
      const Instruction& instruction = all_instructions_[i];
      CHECK_EQ(instruction.opcode, it.NextOpcode());
      for (int j = 0; j < TranslationOpcodeOperandCount(instruction.opcode);
           ++j) {
        CHECK_EQ(instruction.operands[j], it.NextOperandUnsigned());
      }
    }
  }
#endif
  return result;
}

void TranslationArrayBuilder::BeginBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::BUILTIN_CONTINUATION_FRAME;
  AddWithSignedOperands(3, opcode, bytecode_offset.ToInt(), literal_id, height);
}

#if V8_ENABLE_WEBASSEMBLY
void TranslationArrayBuilder::BeginJSToWasmBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    base::Optional<wasm::ValueKind> return_kind) {
  auto opcode = TranslationOpcode::JS_TO_WASM_BUILTIN_CONTINUATION_FRAME;
  AddWithSignedOperands(
      4, opcode, bytecode_offset.ToInt(), literal_id, height,
      return_kind ? static_cast<int>(return_kind.value()) : kNoWasmReturnKind);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME;
  AddWithSignedOperands(3, opcode, bytecode_offset.ToInt(), literal_id, height);
}

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationWithCatchFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode =
      TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME;
  AddWithSignedOperands(3, opcode, bytecode_offset.ToInt(), literal_id, height);
}

void TranslationArrayBuilder::BeginConstructStubFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::CONSTRUCT_STUB_FRAME;
  AddWithSignedOperands(3, opcode, bytecode_offset.ToInt(), literal_id, height);
}

void TranslationArrayBuilder::BeginInlinedExtraArguments(int literal_id,
                                                         unsigned height) {
  auto opcode = TranslationOpcode::INLINED_EXTRA_ARGUMENTS;
  AddWithSignedOperands(2, opcode, literal_id, height);
}

void TranslationArrayBuilder::BeginInterpretedFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    int return_value_offset, int return_value_count) {
  auto opcode = TranslationOpcode::INTERPRETED_FRAME;
  AddWithSignedOperands(5, opcode, bytecode_offset.ToInt(), literal_id, height,
                        return_value_offset, return_value_count);
}

void TranslationArrayBuilder::ArgumentsElements(CreateArgumentsType type) {
  auto opcode = TranslationOpcode::ARGUMENTS_ELEMENTS;
  AddWithSignedOperand(opcode, static_cast<uint8_t>(type));
}

void TranslationArrayBuilder::ArgumentsLength() {
  auto opcode = TranslationOpcode::ARGUMENTS_LENGTH;
  AddWithNoOperands(opcode);
}

void TranslationArrayBuilder::BeginCapturedObject(int length) {
  auto opcode = TranslationOpcode::CAPTURED_OBJECT;
  AddWithSignedOperand(opcode, length);
}

void TranslationArrayBuilder::DuplicateObject(int object_index) {
  auto opcode = TranslationOpcode::DUPLICATED_OBJECT;
  AddWithSignedOperand(opcode, object_index);
}

static uint32_t RegisterToUint32(Register reg) {
  static_assert(Register::kNumRegisters - 1 <= base::kDataMask);
  return static_cast<byte>(reg.code());
}

void TranslationArrayBuilder::StoreRegister(Register reg) {
  auto opcode = TranslationOpcode::REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreInt32Register(Register reg) {
  auto opcode = TranslationOpcode::INT32_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreInt64Register(Register reg) {
  auto opcode = TranslationOpcode::INT64_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreSignedBigInt64Register(Register reg) {
  auto opcode = TranslationOpcode::SIGNED_BIGINT64_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreUnsignedBigInt64Register(Register reg) {
  auto opcode = TranslationOpcode::UNSIGNED_BIGINT64_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreUint32Register(Register reg) {
  auto opcode = TranslationOpcode::UINT32_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreBoolRegister(Register reg) {
  auto opcode = TranslationOpcode::BOOL_REGISTER;
  AddWithUnsignedOperand(opcode, RegisterToUint32(reg));
}

void TranslationArrayBuilder::StoreFloatRegister(FloatRegister reg) {
  static_assert(FloatRegister::kNumRegisters - 1 <= base::kDataMask);
  auto opcode = TranslationOpcode::FLOAT_REGISTER;
  AddWithUnsignedOperand(opcode, static_cast<byte>(reg.code()));
}

void TranslationArrayBuilder::StoreDoubleRegister(DoubleRegister reg) {
  static_assert(DoubleRegister::kNumRegisters - 1 <= base::kDataMask);
  auto opcode = TranslationOpcode::DOUBLE_REGISTER;
  AddWithUnsignedOperand(opcode, static_cast<byte>(reg.code()));
}

void TranslationArrayBuilder::StoreStackSlot(int index) {
  auto opcode = TranslationOpcode::STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreInt32StackSlot(int index) {
  auto opcode = TranslationOpcode::INT32_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::INT64_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreSignedBigInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::SIGNED_BIGINT64_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreUnsignedBigInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::UNSIGNED_BIGINT64_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreUint32StackSlot(int index) {
  auto opcode = TranslationOpcode::UINT32_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreBoolStackSlot(int index) {
  auto opcode = TranslationOpcode::BOOL_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreFloatStackSlot(int index) {
  auto opcode = TranslationOpcode::FLOAT_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreDoubleStackSlot(int index) {
  auto opcode = TranslationOpcode::DOUBLE_STACK_SLOT;
  AddWithSignedOperand(opcode, index);
}

void TranslationArrayBuilder::StoreLiteral(int literal_id) {
  auto opcode = TranslationOpcode::LITERAL;
  AddWithSignedOperand(opcode, literal_id);
}

void TranslationArrayBuilder::StoreOptimizedOut() {
  auto opcode = TranslationOpcode::OPTIMIZED_OUT;
  AddWithNoOperands(opcode);
}

void TranslationArrayBuilder::AddUpdateFeedback(int vector_literal, int slot) {
  auto opcode = TranslationOpcode::UPDATE_FEEDBACK;
  AddWithSignedOperands(2, opcode, vector_literal, slot);
}

void TranslationArrayBuilder::StoreJSFrameFunction() {
  StoreStackSlot((StandardFrameConstants::kCallerPCOffset -
                  StandardFrameConstants::kFunctionOffset) /
                 kSystemPointerSize);
}

}  // namespace internal
}  // namespace v8
