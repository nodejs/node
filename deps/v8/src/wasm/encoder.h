// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_ENCODER_H_
#define V8_WASM_ENCODER_H_

#include "src/signature.h"
#include "src/zone-containers.h"

#include "src/base/smart-pointers.h"

#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmModuleBuilder;

class WasmFunctionEncoder : public ZoneObject {
 public:
  uint32_t HeaderSize() const;
  uint32_t BodySize() const;
  uint32_t NameSize() const;
  void Serialize(byte* buffer, byte** header, byte** body) const;

 private:
  WasmFunctionEncoder(Zone* zone, LocalDeclEncoder locals, bool exported);
  friend class WasmFunctionBuilder;
  uint32_t signature_index_;
  LocalDeclEncoder locals_;
  bool exported_;
  ZoneVector<uint8_t> body_;
  ZoneVector<char> name_;

  bool HasName() const { return exported_ && name_.size() > 0; }
};

class WasmFunctionBuilder : public ZoneObject {
 public:
  void SetSignature(FunctionSig* sig);
  uint32_t AddLocal(LocalType type);
  void EmitVarInt(uint32_t val);
  void EmitCode(const byte* code, uint32_t code_size);
  void Emit(WasmOpcode opcode);
  void EmitGetLocal(uint32_t index);
  void EmitSetLocal(uint32_t index);
  void EmitI32Const(int32_t val);
  void EmitWithU8(WasmOpcode opcode, const byte immediate);
  void EmitWithU8U8(WasmOpcode opcode, const byte imm1, const byte imm2);
  void EmitWithVarInt(WasmOpcode opcode, uint32_t immediate);
  void Exported(uint8_t flag);
  void SetName(const char* name, int name_length);
  WasmFunctionEncoder* Build(Zone* zone, WasmModuleBuilder* mb) const;

 private:
  explicit WasmFunctionBuilder(Zone* zone);
  friend class WasmModuleBuilder;
  LocalDeclEncoder locals_;
  uint8_t exported_;
  ZoneVector<uint8_t> body_;
  ZoneVector<char> name_;
  void IndexVars(WasmFunctionEncoder* e, uint32_t* var_index) const;
};

class WasmDataSegmentEncoder : public ZoneObject {
 public:
  WasmDataSegmentEncoder(Zone* zone, const byte* data, uint32_t size,
                         uint32_t dest);
  uint32_t HeaderSize() const;
  uint32_t BodySize() const;
  void Serialize(byte* buffer, byte** header, byte** body) const;

 private:
  ZoneVector<byte> data_;
  uint32_t dest_;
};

class WasmModuleIndex : public ZoneObject {
 public:
  const byte* Begin() const { return begin_; }
  const byte* End() const { return end_; }

 private:
  friend class WasmModuleWriter;
  WasmModuleIndex(const byte* begin, const byte* end)
      : begin_(begin), end_(end) {}
  const byte* begin_;
  const byte* end_;
};

struct WasmFunctionImport {
  uint32_t sig_index;
  const char* name;
  int name_length;
};

class WasmModuleWriter : public ZoneObject {
 public:
  WasmModuleIndex* WriteTo(Zone* zone) const;

 private:
  friend class WasmModuleBuilder;
  explicit WasmModuleWriter(Zone* zone);
  ZoneVector<WasmFunctionImport> imports_;
  ZoneVector<WasmFunctionEncoder*> functions_;
  ZoneVector<WasmDataSegmentEncoder*> data_segments_;
  ZoneVector<FunctionSig*> signatures_;
  ZoneVector<uint32_t> indirect_functions_;
  ZoneVector<std::pair<MachineType, bool>> globals_;
  int start_function_index_;
};

class WasmModuleBuilder : public ZoneObject {
 public:
  explicit WasmModuleBuilder(Zone* zone);
  uint32_t AddFunction();
  uint32_t AddGlobal(MachineType type, bool exported);
  WasmFunctionBuilder* FunctionAt(size_t index);
  void AddDataSegment(WasmDataSegmentEncoder* data);
  uint32_t AddSignature(FunctionSig* sig);
  void AddIndirectFunction(uint32_t index);
  void MarkStartFunction(uint32_t index);
  uint32_t AddImport(const char* name, int name_length, FunctionSig* sig);
  WasmModuleWriter* Build(Zone* zone);

  struct CompareFunctionSigs {
    bool operator()(FunctionSig* a, FunctionSig* b) const;
  };
  typedef ZoneMap<FunctionSig*, uint32_t, CompareFunctionSigs> SignatureMap;

 private:
  Zone* zone_;
  ZoneVector<FunctionSig*> signatures_;
  ZoneVector<WasmFunctionImport> imports_;
  ZoneVector<WasmFunctionBuilder*> functions_;
  ZoneVector<WasmDataSegmentEncoder*> data_segments_;
  ZoneVector<uint32_t> indirect_functions_;
  ZoneVector<std::pair<MachineType, bool>> globals_;
  SignatureMap signature_map_;
  int start_function_index_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_ENCODER_H_
