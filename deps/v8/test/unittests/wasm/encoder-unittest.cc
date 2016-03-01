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
  Zone zone;
  WasmModuleBuilder* builder = new (&zone) WasmModuleBuilder(&zone);
  uint16_t f_index = builder->AddFunction();
  WasmFunctionBuilder* function = builder->FunctionAt(f_index);
  uint16_t local_float32 = function->AddLocal(kAstF32);
  uint16_t param_float32 = function->AddParam(kAstF32);
  uint16_t local_int32 = function->AddLocal(kAstI32);
  uint16_t local_float64 = function->AddLocal(kAstF64);
  uint16_t local_int64 = function->AddLocal(kAstI64);
  uint16_t param_int32 = function->AddParam(kAstI32);
  uint16_t local_int32_2 = function->AddLocal(kAstI32);

  byte code[] = {kExprGetLocal, static_cast<uint8_t>(param_float32)};
  uint32_t local_indices[] = {1};
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(param_int32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_int32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_int32_2);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_int64);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_float32);
  function->EmitCode(code, sizeof(code), local_indices, 1);
  code[1] = static_cast<uint8_t>(local_float64);
  function->EmitCode(code, sizeof(code), local_indices, 1);

  WasmFunctionEncoder* f = function->Build(&zone, builder);
  ZoneVector<uint8_t> buffer_vector(f->HeaderSize() + f->BodySize(), &zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + f->HeaderSize();
  f->Serialize(buffer, &header, &body);
  for (size_t i = 0; i < 7; i++) {
    CHECK_EQ(i, static_cast<size_t>(*(buffer + 2 * i + f->HeaderSize() + 1)));
  }
}


TEST_F(EncoderTest, Function_Builder_Indexing_Variable_Width) {
  Zone zone;
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
  for (size_t i = 0; i < 127; i++) {
    CHECK_EQ(kExprGetLocal, static_cast<size_t>(*(body + 2 * i)));
    CHECK_EQ(i + 1, static_cast<size_t>(*(body + 2 * i + 1)));
  }
  CHECK_EQ(kExprGetLocal, static_cast<size_t>(*(body + 2 * 127)));
  CHECK_EQ(0x80, static_cast<size_t>(*(body + 2 * 127 + 1)));
  CHECK_EQ(0x01, static_cast<size_t>(*(body + 2 * 127 + 2)));
  CHECK_EQ(kExprGetLocal, static_cast<size_t>(*(body + 2 * 127 + 3)));
  CHECK_EQ(0x00, static_cast<size_t>(*(body + 2 * 127 + 4)));
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
