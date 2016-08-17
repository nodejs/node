// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/test-utils.h"

#include "src/v8.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/encoder.h"

namespace v8 {
namespace internal {
namespace wasm {

class EncoderTest : public TestWithZone {
 protected:
  void AddLocal(WasmFunctionBuilder* f, LocalType type) {
    uint16_t index = f->AddLocal(type);
    const std::vector<uint8_t>& out_index = UnsignedLEB128From(index);
    std::vector<uint8_t> code;
    code.push_back(kExprGetLocal);
    for (size_t i = 0; i < out_index.size(); i++) {
      code.push_back(out_index.at(i));
    }
    uint32_t local_indices[] = {1};
    f->EmitCode(&code[0], static_cast<uint32_t>(code.size()), local_indices, 1);
  }

  void CheckReadValue(uint8_t* leb_value, uint32_t expected_result,
                      int expected_length,
                      ReadUnsignedLEB128ErrorCode expected_error_code) {
    int length;
    uint32_t result;
    ReadUnsignedLEB128ErrorCode error_code =
        ReadUnsignedLEB128Operand(leb_value, leb_value + 5, &length, &result);
    CHECK_EQ(error_code, expected_error_code);
    if (error_code == 0) {
      CHECK_EQ(result, expected_result);
      CHECK_EQ(length, expected_length);
    }
  }

  void CheckWriteValue(uint32_t input, int length, uint8_t* vals) {
    const std::vector<uint8_t> result = UnsignedLEB128From(input);
    CHECK_EQ(result.size(), length);
    for (int i = 0; i < length; i++) {
      CHECK_EQ(result.at(i), vals[i]);
    }
  }
};


TEST_F(EncoderTest, Function_Builder_Variable_Indexing) {
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  uint16_t local_f32 = function->AddLocal(kAstF32);
  uint16_t param_float32 = function->AddParam(kAstF32);
  uint16_t local_i32 = function->AddLocal(kAstI32);
  uint16_t local_f64 = function->AddLocal(kAstF64);
  uint16_t local_i64 = function->AddLocal(kAstI64);
  uint16_t param_int32 = function->AddParam(kAstI32);
  uint16_t local_i32_2 = function->AddLocal(kAstI32);

  byte code[] = {kExprGetLocal, static_cast<uint8_t>(param_float32)};
  uint32_t local_indices[] = {1};
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(param_int32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_i32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_i32_2);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_i64);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_f32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_f64);
  function->EmitCode(code, sizeof(code), local_indices, 1);

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  ZoneVector<uint8_t> buffer_vector(f->HeaderSize() + f->BodySize(), &zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + f->HeaderSize();
  f->Serialize(buffer, &header, &body);
}


TEST_F(EncoderTest, Function_Builder_Indexing_Variable_Width) {
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  for (size_t i = 0; i < 128; i++) {
    AddLocal(function, kAstF32);
  }
  AddLocal(function, kAstI32);

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  ZoneVector<uint8_t> buffer_vector(f->HeaderSize() + f->BodySize(), &zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + f->HeaderSize();
  f->Serialize(buffer, &header, &body);
  body = buffer + f->HeaderSize();
}

TEST_F(EncoderTest, Function_Builder_Block_Variable_Width) {
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  function->EmitWithVarInt(kExprBlock, 200);
  for (int i = 0; i < 200; ++i) {
    function->Emit(kExprNop);
  }

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  CHECK_EQ(f->BodySize(), 204);
}

TEST_F(EncoderTest, Function_Builder_EmitEditableVarIntImmediate) {
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  function->Emit(kExprLoop);
  uint32_t offset = function->EmitEditableVarIntImmediate();
  for (int i = 0; i < 200; ++i) {
    function->Emit(kExprNop);
  }
  function->EditVarIntImmediate(offset, 200);

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  CHECK_EQ(f->BodySize(), 204);
}

TEST_F(EncoderTest, Function_Builder_EmitEditableVarIntImmediate_Locals) {
  base::AccountingAllocator allocator;
  Zone zone(&allocator);
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  function->Emit(kExprBlock);
  uint32_t offset = function->EmitEditableVarIntImmediate();
  for (int i = 0; i < 200; ++i) {
    AddLocal(function, kAstI32);
  }
  function->EditVarIntImmediate(offset, 200);

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  ZoneVector<uint8_t> buffer_vector(f->HeaderSize() + f->BodySize(), &zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + f->HeaderSize();
  f->Serialize(buffer, &header, &body);
  body = buffer + f->HeaderSize();

  CHECK_EQ(f->BodySize(), 479);
  const uint8_t varint200_low = (200 & 0x7f) | 0x80;
  const uint8_t varint200_high = (200 >> 7) & 0x7f;
  offset = 0;
  CHECK_EQ(body[offset++], 1);  // Local decl count.
  CHECK_EQ(body[offset++], varint200_low);
  CHECK_EQ(body[offset++], varint200_high);
  CHECK_EQ(body[offset++], kLocalI32);
  CHECK_EQ(body[offset++], kExprBlock);
  CHECK_EQ(body[offset++], varint200_low);
  CHECK_EQ(body[offset++], varint200_high);
  // GetLocal with one-byte indices.
  for (int i = 0; i <= 127; ++i) {
    CHECK_EQ(body[offset++], kExprGetLocal);
    CHECK_EQ(body[offset++], i);
  }
  // GetLocal with two-byte indices.
  for (int i = 128; i < 200; ++i) {
    CHECK_EQ(body[offset++], kExprGetLocal);
    CHECK_EQ(body[offset++], (i & 0x7f) | 0x80);
    CHECK_EQ(body[offset++], (i >> 7) & 0x7f);
  }
  CHECK_EQ(offset, 479);
}

TEST_F(EncoderTest, LEB_Functions) {
  byte leb_value[5] = {0, 0, 0, 0, 0};
  CheckReadValue(leb_value, 0, 1, kNoError);
  CheckWriteValue(0, 1, leb_value);
  leb_value[0] = 23;
  CheckReadValue(leb_value, 23, 1, kNoError);
  CheckWriteValue(23, 1, leb_value);
  leb_value[0] = 0x80;
  leb_value[1] = 0x01;
  CheckReadValue(leb_value, 128, 2, kNoError);
  CheckWriteValue(128, 2, leb_value);
  leb_value[0] = 0x80;
  leb_value[1] = 0x80;
  leb_value[2] = 0x80;
  leb_value[3] = 0x80;
  leb_value[4] = 0x01;
  CheckReadValue(leb_value, 0x10000000, 5, kNoError);
  CheckWriteValue(0x10000000, 5, leb_value);
  leb_value[0] = 0x80;
  leb_value[1] = 0x80;
  leb_value[2] = 0x80;
  leb_value[3] = 0x80;
  leb_value[4] = 0x80;
  CheckReadValue(leb_value, -1, -1, kInvalidLEB128);
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
