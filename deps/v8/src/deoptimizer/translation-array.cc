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
  DCHECK(TranslationOpcodeIsBegin(
      static_cast<TranslationOpcode>(buffer_.GetDataStartAddress()[index])));
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
  uint8_t opcode_byte = buffer_.get(index_++);

  // If the opcode byte is greater than any valid opcode, then the opcode is
  // implicitly MATCH_PREVIOUS_TRANSLATION and the operand is the opcode byte
  // minus kNumTranslationOpcodes. This special-case encoding of the most common
  // opcode saves some memory.
  if (opcode_byte >= kNumTranslationOpcodes) {
    remaining_ops_to_use_from_previous_translation_ =
        opcode_byte - kNumTranslationOpcodes;
    opcode_byte =
        static_cast<uint8_t>(TranslationOpcode::MATCH_PREVIOUS_TRANSLATION);
  } else if (opcode_byte ==
             static_cast<uint8_t>(
                 TranslationOpcode::MATCH_PREVIOUS_TRANSLATION)) {
    remaining_ops_to_use_from_previous_translation_ = NextOperandUnsigned();
  }

  TranslationOpcode opcode = static_cast<TranslationOpcode>(opcode_byte);
  DCHECK_LE(index_, buffer_.length());
  DCHECK_LT(static_cast<uint32_t>(opcode), kNumTranslationOpcodes);
  if (TranslationOpcodeIsBegin(opcode)) {
    int temp_index = index_;
    // The first argument for BEGIN is the distance, in bytes, since the
    // previous BEGIN, or zero to indicate that MATCH_PREVIOUS_TRANSLATION will
    // not be used in this translation.
    uint32_t lookback_distance =
        base::VLQDecodeUnsigned(buffer_.GetDataStartAddress(), &temp_index);
    if (lookback_distance) {
      previous_index_ = index_ - 1 - lookback_distance;
      DCHECK(TranslationOpcodeIsBegin(
          static_cast<TranslationOpcode>(buffer_.get(previous_index_))));
      // The previous BEGIN should specify zero as its lookback distance,
      // meaning it won't use MATCH_PREVIOUS_TRANSLATION.
      DCHECK_EQ(buffer_.get(previous_index_ + 1), 0);
    }
    ops_since_previous_index_was_updated_ = 1;
  } else if (opcode == TranslationOpcode::MATCH_PREVIOUS_TRANSLATION) {
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

namespace {

class OperandBase {
 public:
  explicit OperandBase(uint32_t value) : value_(value) {}
  uint32_t value() const { return value_; }

 private:
  uint32_t value_;
};

class SmallUnsignedOperand : public OperandBase {
 public:
  explicit SmallUnsignedOperand(uint32_t value) : OperandBase(value) {
    DCHECK_LE(value, base::kDataMask);
  }
  void WriteVLQ(ZoneVector<uint8_t>* buffer) { buffer->push_back(value()); }
  bool IsSigned() const { return false; }
};

class UnsignedOperand : public OperandBase {
 public:
  explicit UnsignedOperand(uint32_t value) : OperandBase(value) {}
  void WriteVLQ(ZoneVector<uint8_t>* buffer) {
    base::VLQEncodeUnsigned(
        [buffer](byte value) {
          buffer->push_back(value);
          return &buffer->back();
        },
        value());
  }
  bool IsSigned() const { return false; }
};

class SignedOperand : public OperandBase {
 public:
  explicit SignedOperand(int32_t value) : OperandBase(value) {}
  void WriteVLQ(ZoneVector<uint8_t>* buffer) {
    base::VLQEncode(
        [buffer](byte value) {
          buffer->push_back(value);
          return &buffer->back();
        },
        value());
  }
  bool IsSigned() const { return true; }
};

template <typename... T>
inline bool OperandsEqual(uint32_t* expected_operands, T... operands) {
  return (... && (*(expected_operands++) == operands.value()));
}

}  // namespace

template <typename... T>
void TranslationArrayBuilder::AddRawToContents(TranslationOpcode opcode,
                                               T... operands) {
  DCHECK_EQ(sizeof...(T), TranslationOpcodeOperandCount(opcode));
  DCHECK(!v8_flags.turbo_compress_translation_arrays);
  contents_.push_back(static_cast<byte>(opcode));
  (..., operands.WriteVLQ(&contents_));
}

template <typename... T>
void TranslationArrayBuilder::AddRawToContentsForCompression(
    TranslationOpcode opcode, T... operands) {
  DCHECK_EQ(sizeof...(T), TranslationOpcodeOperandCount(opcode));
  DCHECK(v8_flags.turbo_compress_translation_arrays);
  contents_for_compression_.push_back(static_cast<byte>(opcode));
  (..., contents_for_compression_.push_back(operands.value()));
}

template <typename... T>
void TranslationArrayBuilder::AddRawBegin(bool update_feedback, T... operands) {
  auto opcode = update_feedback ? TranslationOpcode::BEGIN_WITH_FEEDBACK
                                : TranslationOpcode::BEGIN_WITHOUT_FEEDBACK;
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    AddRawToContentsForCompression(opcode, operands...);
  } else {
    AddRawToContents(opcode, operands...);
#ifdef ENABLE_SLOW_DCHECKS
    if (v8_flags.enable_slow_asserts) {
      all_instructions_.emplace_back(opcode, operands...);
    }
#endif
  }
}

int TranslationArrayBuilder::BeginTranslation(int frame_count,
                                              int jsframe_count,
                                              bool update_feedback) {
  FinishPendingInstructionIfNeeded();
  int start_index = Size();
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
  // use a special helper function rather than calling Add().
  AddRawBegin(update_feedback, UnsignedOperand(distance_from_last_start),
              SignedOperand(frame_count), SignedOperand(jsframe_count));
  return start_index;
}

void TranslationArrayBuilder::FinishPendingInstructionIfNeeded() {
  if (matching_instructions_count_) {
    total_matching_instructions_in_current_translation_ +=
        matching_instructions_count_;

    // There is a short form for the MATCH_PREVIOUS_TRANSLATION instruction
    // because it's the most common opcode: rather than spending a byte on the
    // opcode and a second byte on the operand, we can use only a single byte
    // which doesn't match any valid opcode.
    const int kMaxShortenableOperand =
        std::numeric_limits<uint8_t>::max() - kNumTranslationOpcodes;
    if (matching_instructions_count_ <= kMaxShortenableOperand) {
      contents_.push_back(kNumTranslationOpcodes +
                          matching_instructions_count_);
    } else {
      // The operand didn't fit in the opcode byte, so encode it normally.
      AddRawToContents(
          TranslationOpcode::MATCH_PREVIOUS_TRANSLATION,
          UnsignedOperand(static_cast<uint32_t>(matching_instructions_count_)));
    }
    matching_instructions_count_ = 0;
  }
}

template <typename... T>
void TranslationArrayBuilder::Add(TranslationOpcode opcode, T... operands) {
  DCHECK_EQ(sizeof...(T), TranslationOpcodeOperandCount(opcode));
  if (V8_UNLIKELY(v8_flags.turbo_compress_translation_arrays)) {
    AddRawToContentsForCompression(opcode, operands...);
    return;
  }
#ifdef ENABLE_SLOW_DCHECKS
  if (v8_flags.enable_slow_asserts) {
    all_instructions_.emplace_back(opcode, operands...);
  }
#endif
  if (match_previous_allowed_ &&
      instruction_index_within_translation_ < basis_instructions_.size() &&
      opcode ==
          basis_instructions_[instruction_index_within_translation_].opcode &&
      OperandsEqual(
          basis_instructions_[instruction_index_within_translation_].operands,
          operands...)) {
    ++matching_instructions_count_;
  } else {
    FinishPendingInstructionIfNeeded();
    AddRawToContents(opcode, operands...);
    if (!match_previous_allowed_) {
      // Include this instruction in basis_instructions_ so that future
      // translations can check whether they match with it.
      DCHECK_EQ(basis_instructions_.size(),
                instruction_index_within_translation_);
      basis_instructions_.emplace_back(opcode, operands...);
    }
  }
  ++instruction_index_within_translation_;
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
        uint32_t operand = instruction.is_operand_signed[j]
                               ? it.NextOperand()
                               : it.NextOperandUnsigned();
        CHECK_EQ(instruction.operands[j], operand);
      }
    }
  }
#endif
  return result;
}

void TranslationArrayBuilder::BeginBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::BUILTIN_CONTINUATION_FRAME;
  Add(opcode, SignedOperand(bytecode_offset.ToInt()), SignedOperand(literal_id),
      SignedOperand(height));
}

#if V8_ENABLE_WEBASSEMBLY
void TranslationArrayBuilder::BeginJSToWasmBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    base::Optional<wasm::ValueKind> return_kind) {
  auto opcode = TranslationOpcode::JS_TO_WASM_BUILTIN_CONTINUATION_FRAME;
  Add(opcode, SignedOperand(bytecode_offset.ToInt()), SignedOperand(literal_id),
      SignedOperand(height),
      SignedOperand(return_kind ? static_cast<int>(return_kind.value())
                                : kNoWasmReturnKind));
}
#endif  // V8_ENABLE_WEBASSEMBLY

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME;
  Add(opcode, SignedOperand(bytecode_offset.ToInt()), SignedOperand(literal_id),
      SignedOperand(height));
}

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationWithCatchFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode =
      TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME;
  Add(opcode, SignedOperand(bytecode_offset.ToInt()), SignedOperand(literal_id),
      SignedOperand(height));
}

void TranslationArrayBuilder::BeginConstructStubFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::CONSTRUCT_STUB_FRAME;
  Add(opcode, SignedOperand(bytecode_offset.ToInt()), SignedOperand(literal_id),
      SignedOperand(height));
}

void TranslationArrayBuilder::BeginInlinedExtraArguments(int literal_id,
                                                         unsigned height) {
  auto opcode = TranslationOpcode::INLINED_EXTRA_ARGUMENTS;
  Add(opcode, SignedOperand(literal_id), SignedOperand(height));
}

void TranslationArrayBuilder::BeginInterpretedFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    int return_value_offset, int return_value_count) {
  if (return_value_count == 0) {
    auto opcode = TranslationOpcode::INTERPRETED_FRAME_WITHOUT_RETURN;
    Add(opcode, SignedOperand(bytecode_offset.ToInt()),
        SignedOperand(literal_id), SignedOperand(height));
  } else {
    auto opcode = TranslationOpcode::INTERPRETED_FRAME_WITH_RETURN;
    Add(opcode, SignedOperand(bytecode_offset.ToInt()),
        SignedOperand(literal_id), SignedOperand(height),
        SignedOperand(return_value_offset), SignedOperand(return_value_count));
  }
}

void TranslationArrayBuilder::ArgumentsElements(CreateArgumentsType type) {
  auto opcode = TranslationOpcode::ARGUMENTS_ELEMENTS;
  Add(opcode, SignedOperand(static_cast<uint8_t>(type)));
}

void TranslationArrayBuilder::ArgumentsLength() {
  auto opcode = TranslationOpcode::ARGUMENTS_LENGTH;
  Add(opcode);
}

void TranslationArrayBuilder::BeginCapturedObject(int length) {
  auto opcode = TranslationOpcode::CAPTURED_OBJECT;
  Add(opcode, SignedOperand(length));
}

void TranslationArrayBuilder::DuplicateObject(int object_index) {
  auto opcode = TranslationOpcode::DUPLICATED_OBJECT;
  Add(opcode, SignedOperand(object_index));
}

void TranslationArrayBuilder::StoreRegister(TranslationOpcode opcode,
                                            Register reg) {
  static_assert(Register::kNumRegisters - 1 <= base::kDataMask);
  Add(opcode, SmallUnsignedOperand(static_cast<byte>(reg.code())));
}

void TranslationArrayBuilder::StoreRegister(Register reg) {
  auto opcode = TranslationOpcode::REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreInt32Register(Register reg) {
  auto opcode = TranslationOpcode::INT32_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreInt64Register(Register reg) {
  auto opcode = TranslationOpcode::INT64_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreSignedBigInt64Register(Register reg) {
  auto opcode = TranslationOpcode::SIGNED_BIGINT64_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreUnsignedBigInt64Register(Register reg) {
  auto opcode = TranslationOpcode::UNSIGNED_BIGINT64_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreUint32Register(Register reg) {
  auto opcode = TranslationOpcode::UINT32_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreBoolRegister(Register reg) {
  auto opcode = TranslationOpcode::BOOL_REGISTER;
  StoreRegister(opcode, reg);
}

void TranslationArrayBuilder::StoreFloatRegister(FloatRegister reg) {
  static_assert(FloatRegister::kNumRegisters - 1 <= base::kDataMask);
  auto opcode = TranslationOpcode::FLOAT_REGISTER;
  Add(opcode, SmallUnsignedOperand(static_cast<byte>(reg.code())));
}

void TranslationArrayBuilder::StoreDoubleRegister(DoubleRegister reg) {
  static_assert(DoubleRegister::kNumRegisters - 1 <= base::kDataMask);
  auto opcode = TranslationOpcode::DOUBLE_REGISTER;
  Add(opcode, SmallUnsignedOperand(static_cast<byte>(reg.code())));
}

void TranslationArrayBuilder::StoreStackSlot(int index) {
  auto opcode = TranslationOpcode::STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreInt32StackSlot(int index) {
  auto opcode = TranslationOpcode::INT32_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::INT64_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreSignedBigInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::SIGNED_BIGINT64_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreUnsignedBigInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::UNSIGNED_BIGINT64_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreUint32StackSlot(int index) {
  auto opcode = TranslationOpcode::UINT32_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreBoolStackSlot(int index) {
  auto opcode = TranslationOpcode::BOOL_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreFloatStackSlot(int index) {
  auto opcode = TranslationOpcode::FLOAT_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreDoubleStackSlot(int index) {
  auto opcode = TranslationOpcode::DOUBLE_STACK_SLOT;
  Add(opcode, SignedOperand(index));
}

void TranslationArrayBuilder::StoreLiteral(int literal_id) {
  auto opcode = TranslationOpcode::LITERAL;
  DCHECK_GE(literal_id, 0);
  Add(opcode, SignedOperand(literal_id));
}

void TranslationArrayBuilder::StoreOptimizedOut() {
  auto opcode = TranslationOpcode::OPTIMIZED_OUT;
  Add(opcode);
}

void TranslationArrayBuilder::AddUpdateFeedback(int vector_literal, int slot) {
  auto opcode = TranslationOpcode::UPDATE_FEEDBACK;
  Add(opcode, SignedOperand(vector_literal), SignedOperand(slot));
}

void TranslationArrayBuilder::StoreJSFrameFunction() {
  StoreStackSlot((StandardFrameConstants::kCallerPCOffset -
                  StandardFrameConstants::kFunctionOffset) /
                 kSystemPointerSize);
}

}  // namespace internal
}  // namespace v8
