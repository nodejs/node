// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/translation-array.h"

#include "src/base/vlq.h"
#include "src/deoptimizer/translated-state.h"
#include "src/objects/fixed-array-inl.h"
#include "third_party/zlib/google/compression_utils_portable.h"

namespace v8 {
namespace internal {

namespace {

// Constants describing compressed TranslationArray layout. Only relevant if
// --turbo-compress-translation-arrays is enabled.
constexpr int kUncompressedSizeOffset = 0;
constexpr int kUncompressedSizeSize = kInt32Size;
constexpr int kCompressedDataOffset =
    kUncompressedSizeOffset + kUncompressedSizeSize;
constexpr int kTranslationArrayElementSize = kInt32Size;

}  // namespace

TranslationArrayIterator::TranslationArrayIterator(TranslationArray buffer,
                                                   int index)
    : buffer_(buffer), index_(index) {
  if (V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)) {
    const int size = buffer_.get_int(kUncompressedSizeOffset);
    uncompressed_contents_.insert(uncompressed_contents_.begin(), size, 0);

    uLongf uncompressed_size = size * kTranslationArrayElementSize;

    CHECK_EQ(
        zlib_internal::UncompressHelper(
            zlib_internal::ZRAW,
            bit_cast<Bytef*>(uncompressed_contents_.data()), &uncompressed_size,
            buffer_.GetDataStartAddress() + kCompressedDataOffset,
            buffer_.DataSize()),
        Z_OK);
    DCHECK(index >= 0 && index < size);
  } else {
    DCHECK(index >= 0 && index < buffer.length());
  }
}

int32_t TranslationArrayIterator::Next() {
  if (V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)) {
    return uncompressed_contents_[index_++];
  } else {
    int32_t value = base::VLQDecode(buffer_.GetDataStartAddress(), &index_);
    DCHECK_LE(index_, buffer_.length());
    return value;
  }
}

bool TranslationArrayIterator::HasNext() const {
  if (V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)) {
    return index_ < static_cast<int>(uncompressed_contents_.size());
  } else {
    return index_ < buffer_.length();
  }
}

void TranslationArrayBuilder::Add(int32_t value) {
  if (V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)) {
    contents_for_compression_.push_back(value);
  } else {
    base::VLQEncode(&contents_, value);
  }
}

Handle<TranslationArray> TranslationArrayBuilder::ToTranslationArray(
    Factory* factory) {
  if (V8_UNLIKELY(FLAG_turbo_compress_translation_arrays)) {
    const int input_size = SizeInBytes();
    uLongf compressed_data_size = compressBound(input_size);

    ZoneVector<byte> compressed_data(compressed_data_size, zone());

    CHECK_EQ(
        zlib_internal::CompressHelper(
            zlib_internal::ZRAW, compressed_data.data(), &compressed_data_size,
            bit_cast<const Bytef*>(contents_for_compression_.data()),
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
  } else {
    Handle<TranslationArray> result =
        factory->NewByteArray(SizeInBytes(), AllocationType::kOld);
    memcpy(result->GetDataStartAddress(), contents_.data(),
           contents_.size() * sizeof(uint8_t));
    return result;
  }
}

void TranslationArrayBuilder::BeginBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::BUILTIN_CONTINUATION_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 3);
}

#if V8_ENABLE_WEBASSEMBLY
void TranslationArrayBuilder::BeginJSToWasmBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    base::Optional<wasm::ValueKind> return_kind) {
  auto opcode = TranslationOpcode::JS_TO_WASM_BUILTIN_CONTINUATION_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  Add(return_kind ? static_cast<int>(return_kind.value()) : kNoWasmReturnKind);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 4);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 3);
}

void TranslationArrayBuilder::BeginJavaScriptBuiltinContinuationWithCatchFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode =
      TranslationOpcode::JAVA_SCRIPT_BUILTIN_CONTINUATION_WITH_CATCH_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 3);
}

void TranslationArrayBuilder::BeginConstructStubFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height) {
  auto opcode = TranslationOpcode::CONSTRUCT_STUB_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 3);
}

void TranslationArrayBuilder::BeginArgumentsAdaptorFrame(int literal_id,
                                                         unsigned height) {
  auto opcode = TranslationOpcode::ARGUMENTS_ADAPTOR_FRAME;
  Add(opcode);
  Add(literal_id);
  Add(height);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 2);
}

void TranslationArrayBuilder::BeginInterpretedFrame(
    BytecodeOffset bytecode_offset, int literal_id, unsigned height,
    int return_value_offset, int return_value_count) {
  auto opcode = TranslationOpcode::INTERPRETED_FRAME;
  Add(opcode);
  Add(bytecode_offset.ToInt());
  Add(literal_id);
  Add(height);
  Add(return_value_offset);
  Add(return_value_count);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 5);
}

void TranslationArrayBuilder::ArgumentsElements(CreateArgumentsType type) {
  auto opcode = TranslationOpcode::ARGUMENTS_ELEMENTS;
  Add(opcode);
  Add(static_cast<uint8_t>(type));
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::ArgumentsLength() {
  auto opcode = TranslationOpcode::ARGUMENTS_LENGTH;
  Add(opcode);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 0);
}

void TranslationArrayBuilder::BeginCapturedObject(int length) {
  auto opcode = TranslationOpcode::CAPTURED_OBJECT;
  Add(opcode);
  Add(length);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::DuplicateObject(int object_index) {
  auto opcode = TranslationOpcode::DUPLICATED_OBJECT;
  Add(opcode);
  Add(object_index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreRegister(Register reg) {
  auto opcode = TranslationOpcode::REGISTER;
  Add(opcode);
  Add(reg.code());
}

void TranslationArrayBuilder::StoreInt32Register(Register reg) {
  auto opcode = TranslationOpcode::INT32_REGISTER;
  Add(opcode);
  Add(reg.code());
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreInt64Register(Register reg) {
  auto opcode = TranslationOpcode::INT64_REGISTER;
  Add(opcode);
  Add(reg.code());
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreUint32Register(Register reg) {
  auto opcode = TranslationOpcode::UINT32_REGISTER;
  Add(opcode);
  Add(reg.code());
}

void TranslationArrayBuilder::StoreBoolRegister(Register reg) {
  auto opcode = TranslationOpcode::BOOL_REGISTER;
  Add(opcode);
  Add(reg.code());
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreFloatRegister(FloatRegister reg) {
  auto opcode = TranslationOpcode::FLOAT_REGISTER;
  Add(opcode);
  Add(reg.code());
}

void TranslationArrayBuilder::StoreDoubleRegister(DoubleRegister reg) {
  auto opcode = TranslationOpcode::DOUBLE_REGISTER;
  Add(opcode);
  Add(reg.code());
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreStackSlot(int index) {
  auto opcode = TranslationOpcode::STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreInt32StackSlot(int index) {
  auto opcode = TranslationOpcode::INT32_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreInt64StackSlot(int index) {
  auto opcode = TranslationOpcode::INT64_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreUint32StackSlot(int index) {
  auto opcode = TranslationOpcode::UINT32_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreBoolStackSlot(int index) {
  auto opcode = TranslationOpcode::BOOL_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreFloatStackSlot(int index) {
  auto opcode = TranslationOpcode::FLOAT_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreDoubleStackSlot(int index) {
  auto opcode = TranslationOpcode::DOUBLE_STACK_SLOT;
  Add(opcode);
  Add(index);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::StoreLiteral(int literal_id) {
  auto opcode = TranslationOpcode::LITERAL;
  Add(opcode);
  Add(literal_id);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 1);
}

void TranslationArrayBuilder::AddUpdateFeedback(int vector_literal, int slot) {
  auto opcode = TranslationOpcode::UPDATE_FEEDBACK;
  Add(opcode);
  Add(vector_literal);
  Add(slot);
  DCHECK_EQ(TranslationOpcodeOperandCount(opcode), 2);
}

void TranslationArrayBuilder::StoreJSFrameFunction() {
  StoreStackSlot((StandardFrameConstants::kCallerPCOffset -
                  StandardFrameConstants::kFunctionOffset) /
                 kSystemPointerSize);
}

}  // namespace internal
}  // namespace v8
