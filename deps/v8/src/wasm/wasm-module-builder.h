// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MODULE_BUILDER_H_
#define V8_WASM_WASM_MODULE_BUILDER_H_

#include "src/signature.h"
#include "src/zone/zone-containers.h"

#include "src/wasm/leb-helper.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

class ZoneBuffer : public ZoneObject {
 public:
  static const uint32_t kInitialSize = 4096;
  explicit ZoneBuffer(Zone* zone, size_t initial = kInitialSize)
      : zone_(zone), buffer_(reinterpret_cast<byte*>(zone->New(initial))) {
    pos_ = buffer_;
    end_ = buffer_ + initial;
  }

  void write_u8(uint8_t x) {
    EnsureSpace(1);
    *(pos_++) = x;
  }

  void write_u16(uint16_t x) {
    EnsureSpace(2);
    WriteLittleEndianValue<uint16_t>(pos_, x);
    pos_ += 2;
  }

  void write_u32(uint32_t x) {
    EnsureSpace(4);
    WriteLittleEndianValue<uint32_t>(pos_, x);
    pos_ += 4;
  }

  void write_u32v(uint32_t val) {
    EnsureSpace(kMaxVarInt32Size);
    LEBHelper::write_u32v(&pos_, val);
  }

  void write_i32v(int32_t val) {
    EnsureSpace(kMaxVarInt32Size);
    LEBHelper::write_i32v(&pos_, val);
  }

  void write_size(size_t val) {
    EnsureSpace(kMaxVarInt32Size);
    DCHECK_EQ(val, static_cast<uint32_t>(val));
    LEBHelper::write_u32v(&pos_, static_cast<uint32_t>(val));
  }

  void write(const byte* data, size_t size) {
    EnsureSpace(size);
    memcpy(pos_, data, size);
    pos_ += size;
  }

  size_t reserve_u32v() {
    size_t off = offset();
    EnsureSpace(kMaxVarInt32Size);
    pos_ += kMaxVarInt32Size;
    return off;
  }

  // Patch a (padded) u32v at the given offset to be the given value.
  void patch_u32v(size_t offset, uint32_t val) {
    byte* ptr = buffer_ + offset;
    for (size_t pos = 0; pos != kPaddedVarInt32Size; ++pos) {
      uint32_t next = val >> 7;
      byte out = static_cast<byte>(val & 0x7f);
      if (pos != kPaddedVarInt32Size - 1) {
        *(ptr++) = 0x80 | out;
        val = next;
      } else {
        *(ptr++) = out;
      }
    }
  }

  size_t offset() const { return static_cast<size_t>(pos_ - buffer_); }
  size_t size() const { return static_cast<size_t>(pos_ - buffer_); }
  const byte* begin() const { return buffer_; }
  const byte* end() const { return pos_; }

  void EnsureSpace(size_t size) {
    if ((pos_ + size) > end_) {
      size_t new_size = 4096 + size + (end_ - buffer_) * 3;
      byte* new_buffer = reinterpret_cast<byte*>(zone_->New(new_size));
      memcpy(new_buffer, buffer_, (pos_ - buffer_));
      pos_ = new_buffer + (pos_ - buffer_);
      buffer_ = new_buffer;
      end_ = new_buffer + new_size;
    }
    DCHECK(pos_ + size <= end_);
  }

  byte** pos_ptr() { return &pos_; }

 private:
  Zone* zone_;
  byte* buffer_;
  byte* pos_;
  byte* end_;
};

class WasmModuleBuilder;

class V8_EXPORT_PRIVATE WasmFunctionBuilder : public ZoneObject {
 public:
  // Building methods.
  void SetSignature(FunctionSig* sig);
  uint32_t AddLocal(ValueType type);
  void EmitVarInt(uint32_t val);
  void EmitCode(const byte* code, uint32_t code_size);
  void Emit(WasmOpcode opcode);
  void EmitGetLocal(uint32_t index);
  void EmitSetLocal(uint32_t index);
  void EmitTeeLocal(uint32_t index);
  void EmitI32Const(int32_t val);
  void EmitWithU8(WasmOpcode opcode, const byte immediate);
  void EmitWithU8U8(WasmOpcode opcode, const byte imm1, const byte imm2);
  void EmitWithVarInt(WasmOpcode opcode, uint32_t immediate);
  void EmitDirectCallIndex(uint32_t index);
  void ExportAs(Vector<const char> name);
  void SetName(Vector<const char> name);
  void AddAsmWasmOffset(int call_position, int to_number_position);
  void SetAsmFunctionStartPosition(int position);

  void WriteSignature(ZoneBuffer& buffer) const;
  void WriteExports(ZoneBuffer& buffer) const;
  void WriteBody(ZoneBuffer& buffer) const;
  void WriteAsmWasmOffsetTable(ZoneBuffer& buffer) const;

  uint32_t func_index() { return func_index_; }
  FunctionSig* signature();

 private:
  explicit WasmFunctionBuilder(WasmModuleBuilder* builder);
  friend class WasmModuleBuilder;
  friend class WasmTemporary;

  struct DirectCallIndex {
    size_t offset;
    uint32_t direct_index;
  };

  WasmModuleBuilder* builder_;
  LocalDeclEncoder locals_;
  uint32_t signature_index_;
  uint32_t func_index_;
  ZoneVector<uint8_t> body_;
  ZoneVector<char> name_;
  ZoneVector<ZoneVector<char>> exported_names_;
  ZoneVector<uint32_t> i32_temps_;
  ZoneVector<uint32_t> i64_temps_;
  ZoneVector<uint32_t> f32_temps_;
  ZoneVector<uint32_t> f64_temps_;
  ZoneVector<DirectCallIndex> direct_calls_;

  // Delta-encoded mapping from wasm bytes to asm.js source positions.
  ZoneBuffer asm_offsets_;
  uint32_t last_asm_byte_offset_ = 0;
  uint32_t last_asm_source_position_ = 0;
  uint32_t asm_func_start_source_position_ = 0;
};

class WasmTemporary {
 public:
  WasmTemporary(WasmFunctionBuilder* builder, ValueType type) {
    switch (type) {
      case kWasmI32:
        temporary_ = &builder->i32_temps_;
        break;
      case kWasmI64:
        temporary_ = &builder->i64_temps_;
        break;
      case kWasmF32:
        temporary_ = &builder->f32_temps_;
        break;
      case kWasmF64:
        temporary_ = &builder->f64_temps_;
        break;
      default:
        UNREACHABLE();
        temporary_ = nullptr;
    }
    if (temporary_->size() == 0) {
      // Allocate a new temporary.
      index_ = builder->AddLocal(type);
    } else {
      // Reuse a previous temporary.
      index_ = temporary_->back();
      temporary_->pop_back();
    }
  }
  ~WasmTemporary() {
    temporary_->push_back(index_);  // return the temporary to the list.
  }
  uint32_t index() { return index_; }

 private:
  ZoneVector<uint32_t>* temporary_;
  uint32_t index_;
};

class V8_EXPORT_PRIVATE WasmModuleBuilder : public ZoneObject {
 public:
  explicit WasmModuleBuilder(Zone* zone);

  // Building methods.
  uint32_t AddImport(const char* name, int name_length, FunctionSig* sig);
  void SetImportName(uint32_t index, const char* name, int name_length) {
    imports_[index].name = name;
    imports_[index].name_length = name_length;
  }
  WasmFunctionBuilder* AddFunction(FunctionSig* sig = nullptr);
  uint32_t AddGlobal(ValueType type, bool exported, bool mutability = true,
                     const WasmInitExpr& init = WasmInitExpr());
  void AddDataSegment(const byte* data, uint32_t size, uint32_t dest);
  uint32_t AddSignature(FunctionSig* sig);
  uint32_t AllocateIndirectFunctions(uint32_t count);
  void SetIndirectFunction(uint32_t indirect, uint32_t direct);
  void MarkStartFunction(WasmFunctionBuilder* builder);

  // Writing methods.
  void WriteTo(ZoneBuffer& buffer) const;
  void WriteAsmJsOffsetTable(ZoneBuffer& buffer) const;

  // TODO(titzer): use SignatureMap from signature-map.h here.
  // This signature map is zone-allocated, but the other is heap allocated.
  struct CompareFunctionSigs {
    bool operator()(FunctionSig* a, FunctionSig* b) const;
  };
  typedef ZoneMap<FunctionSig*, uint32_t, CompareFunctionSigs> SignatureMap;

  Zone* zone() { return zone_; }

  FunctionSig* GetSignature(uint32_t index) { return signatures_[index]; }

 private:
  struct WasmFunctionImport {
    uint32_t sig_index;
    const char* name;
    int name_length;
  };

  struct WasmGlobal {
    ValueType type;
    bool exported;
    bool mutability;
    WasmInitExpr init;
  };

  struct WasmDataSegment {
    ZoneVector<byte> data;
    uint32_t dest;
  };

  friend class WasmFunctionBuilder;
  Zone* zone_;
  ZoneVector<FunctionSig*> signatures_;
  ZoneVector<WasmFunctionImport> imports_;
  ZoneVector<WasmFunctionBuilder*> functions_;
  ZoneVector<WasmDataSegment> data_segments_;
  ZoneVector<uint32_t> indirect_functions_;
  ZoneVector<WasmGlobal> globals_;
  SignatureMap signature_map_;
  int start_function_index_;
};

inline FunctionSig* WasmFunctionBuilder::signature() {
  return builder_->signatures_[signature_index_];
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MODULE_BUILDER_H_
