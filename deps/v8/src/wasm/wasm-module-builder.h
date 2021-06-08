// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_MODULE_BUILDER_H_
#define V8_WASM_WASM_MODULE_BUILDER_H_

#include "src/base/memory.h"
#include "src/base/platform/wrappers.h"
#include "src/codegen/signature.h"
#include "src/utils/vector.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/local-decl-encoder.h"
#include "src/wasm/value-type.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

class ZoneBuffer : public ZoneObject {
 public:
  // This struct is just a type tag for Zone::NewArray<T>(size_t) call.
  struct Buffer {};

  static constexpr size_t kInitialSize = 1024;
  explicit ZoneBuffer(Zone* zone, size_t initial = kInitialSize)
      : zone_(zone), buffer_(zone->NewArray<byte, Buffer>(initial)) {
    pos_ = buffer_;
    end_ = buffer_ + initial;
  }

  void write_u8(uint8_t x) {
    EnsureSpace(1);
    *(pos_++) = x;
  }

  void write_u16(uint16_t x) {
    EnsureSpace(2);
    base::WriteLittleEndianValue<uint16_t>(reinterpret_cast<Address>(pos_), x);
    pos_ += 2;
  }

  void write_u32(uint32_t x) {
    EnsureSpace(4);
    base::WriteLittleEndianValue<uint32_t>(reinterpret_cast<Address>(pos_), x);
    pos_ += 4;
  }

  void write_u64(uint64_t x) {
    EnsureSpace(8);
    base::WriteLittleEndianValue<uint64_t>(reinterpret_cast<Address>(pos_), x);
    pos_ += 8;
  }

  void write_u32v(uint32_t val) {
    EnsureSpace(kMaxVarInt32Size);
    LEBHelper::write_u32v(&pos_, val);
  }

  void write_i32v(int32_t val) {
    EnsureSpace(kMaxVarInt32Size);
    LEBHelper::write_i32v(&pos_, val);
  }

  void write_u64v(uint64_t val) {
    EnsureSpace(kMaxVarInt64Size);
    LEBHelper::write_u64v(&pos_, val);
  }

  void write_i64v(int64_t val) {
    EnsureSpace(kMaxVarInt64Size);
    LEBHelper::write_i64v(&pos_, val);
  }

  void write_size(size_t val) {
    EnsureSpace(kMaxVarInt32Size);
    DCHECK_EQ(val, static_cast<uint32_t>(val));
    LEBHelper::write_u32v(&pos_, static_cast<uint32_t>(val));
  }

  void write_f32(float val) { write_u32(bit_cast<uint32_t>(val)); }

  void write_f64(double val) { write_u64(bit_cast<uint64_t>(val)); }

  void write(const byte* data, size_t size) {
    if (size == 0) return;
    EnsureSpace(size);
    base::Memcpy(pos_, data, size);
    pos_ += size;
  }

  void write_string(Vector<const char> name) {
    write_size(name.length());
    write(reinterpret_cast<const byte*>(name.begin()), name.length());
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

  void patch_u8(size_t offset, byte val) {
    DCHECK_GE(size(), offset);
    buffer_[offset] = val;
  }

  size_t offset() const { return static_cast<size_t>(pos_ - buffer_); }
  size_t size() const { return static_cast<size_t>(pos_ - buffer_); }
  const byte* data() const { return buffer_; }
  const byte* begin() const { return buffer_; }
  const byte* end() const { return pos_; }

  void EnsureSpace(size_t size) {
    if ((pos_ + size) > end_) {
      size_t new_size = size + (end_ - buffer_) * 2;
      byte* new_buffer = zone_->NewArray<byte, Buffer>(new_size);
      base::Memcpy(new_buffer, buffer_, (pos_ - buffer_));
      pos_ = new_buffer + (pos_ - buffer_);
      buffer_ = new_buffer;
      end_ = new_buffer + new_size;
    }
    DCHECK(pos_ + size <= end_);
  }

  void Truncate(size_t size) {
    DCHECK_GE(offset(), size);
    pos_ = buffer_ + size;
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
  void EmitByte(byte b);
  void EmitI32V(int32_t val);
  void EmitU32V(uint32_t val);
  void EmitCode(const byte* code, uint32_t code_size);
  void Emit(WasmOpcode opcode);
  void EmitWithPrefix(WasmOpcode opcode);
  void EmitGetLocal(uint32_t index);
  void EmitSetLocal(uint32_t index);
  void EmitTeeLocal(uint32_t index);
  void EmitI32Const(int32_t val);
  void EmitI64Const(int64_t val);
  void EmitF32Const(float val);
  void EmitF64Const(double val);
  void EmitS128Const(Simd128 val);
  void EmitWithU8(WasmOpcode opcode, const byte immediate);
  void EmitWithU8U8(WasmOpcode opcode, const byte imm1, const byte imm2);
  void EmitWithI32V(WasmOpcode opcode, int32_t immediate);
  void EmitWithU32V(WasmOpcode opcode, uint32_t immediate);
  void EmitDirectCallIndex(uint32_t index);
  void SetName(Vector<const char> name);
  void AddAsmWasmOffset(size_t call_position, size_t to_number_position);
  void SetAsmFunctionStartPosition(size_t function_position);
  void SetCompilationHint(WasmCompilationHintStrategy strategy,
                          WasmCompilationHintTier baseline,
                          WasmCompilationHintTier top_tier);

  size_t GetPosition() const { return body_.size(); }
  void FixupByte(size_t position, byte value) {
    body_.patch_u8(position, value);
  }
  void DeleteCodeAfter(size_t position);

  void WriteSignature(ZoneBuffer* buffer) const;
  void WriteBody(ZoneBuffer* buffer) const;
  void WriteAsmWasmOffsetTable(ZoneBuffer* buffer) const;

  WasmModuleBuilder* builder() const { return builder_; }
  uint32_t func_index() { return func_index_; }
  FunctionSig* signature();

 private:
  explicit WasmFunctionBuilder(WasmModuleBuilder* builder);
  friend class WasmModuleBuilder;
  friend Zone;

  struct DirectCallIndex {
    size_t offset;
    uint32_t direct_index;
  };

  WasmModuleBuilder* builder_;
  LocalDeclEncoder locals_;
  uint32_t signature_index_;
  uint32_t func_index_;
  ZoneBuffer body_;
  Vector<const char> name_;
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
  uint8_t hint_ = kNoCompilationHint;
};

class V8_EXPORT_PRIVATE WasmModuleBuilder : public ZoneObject {
 public:
  explicit WasmModuleBuilder(Zone* zone);
  WasmModuleBuilder(const WasmModuleBuilder&) = delete;
  WasmModuleBuilder& operator=(const WasmModuleBuilder&) = delete;

  // Building methods.
  uint32_t AddImport(Vector<const char> name, FunctionSig* sig,
                     Vector<const char> module = {});
  WasmFunctionBuilder* AddFunction(FunctionSig* sig = nullptr);
  uint32_t AddGlobal(ValueType type, bool mutability = true,
                     WasmInitExpr init = WasmInitExpr());
  uint32_t AddGlobalImport(Vector<const char> name, ValueType type,
                           bool mutability, Vector<const char> module = {});
  void AddDataSegment(const byte* data, uint32_t size, uint32_t dest);
  uint32_t AddSignature(FunctionSig* sig);
  uint32_t AddException(FunctionSig* type);
  uint32_t AddStructType(StructType* type);
  uint32_t AddArrayType(ArrayType* type);
  // In the current implementation, it's supported to have uninitialized slots
  // at the beginning and/or end of the indirect function table, as long as
  // the filled slots form a contiguous block in the middle.
  uint32_t AllocateIndirectFunctions(uint32_t count);
  void SetIndirectFunction(uint32_t indirect, uint32_t direct);
  void SetMaxTableSize(uint32_t max);
  uint32_t AddTable(ValueType type, uint32_t min_size);
  uint32_t AddTable(ValueType type, uint32_t min_size, uint32_t max_size);
  uint32_t AddTable(ValueType type, uint32_t min_size, uint32_t max_size,
                    WasmInitExpr init);
  void MarkStartFunction(WasmFunctionBuilder* builder);
  void AddExport(Vector<const char> name, ImportExportKindCode kind,
                 uint32_t index);
  void AddExport(Vector<const char> name, WasmFunctionBuilder* builder) {
    AddExport(name, kExternalFunction, builder->func_index());
  }
  uint32_t AddExportedGlobal(ValueType type, bool mutability, WasmInitExpr init,
                             Vector<const char> name);
  void ExportImportedFunction(Vector<const char> name, int import_index);
  void SetMinMemorySize(uint32_t value);
  void SetMaxMemorySize(uint32_t value);
  void SetHasSharedMemory();

  // Writing methods.
  void WriteTo(ZoneBuffer* buffer) const;
  void WriteAsmJsOffsetTable(ZoneBuffer* buffer) const;

  Zone* zone() { return zone_; }

  FunctionSig* GetSignature(uint32_t index) {
    DCHECK(types_[index].kind == Type::kFunctionSig);
    return types_[index].sig;
  }

  int NumExceptions() { return static_cast<int>(exceptions_.size()); }

  FunctionSig* GetExceptionType(int index) {
    return types_[exceptions_[index]].sig;
  }

 private:
  struct Type {
    enum Kind { kFunctionSig, kStructType, kArrayType };
    explicit Type(FunctionSig* signature)
        : kind(kFunctionSig), sig(signature) {}
    explicit Type(StructType* struct_type)
        : kind(kStructType), struct_type(struct_type) {}
    explicit Type(ArrayType* array_type)
        : kind(kArrayType), array_type(array_type) {}
    Kind kind;
    union {
      FunctionSig* sig;
      StructType* struct_type;
      ArrayType* array_type;
    };
  };

  struct WasmFunctionImport {
    Vector<const char> module;
    Vector<const char> name;
    uint32_t sig_index;
  };

  struct WasmGlobalImport {
    Vector<const char> module;
    Vector<const char> name;
    ValueTypeCode type_code;
    bool mutability;
  };

  struct WasmExport {
    Vector<const char> name;
    ImportExportKindCode kind;
    int index;  // Can be negative for re-exported imports.
  };

  struct WasmGlobal {
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(WasmGlobal);

    ValueType type;
    bool mutability;
    WasmInitExpr init;
  };

  struct WasmTable {
    ValueType type;
    uint32_t min_size;
    uint32_t max_size;
    bool has_maximum;
    WasmInitExpr init;
  };

  struct WasmDataSegment {
    ZoneVector<byte> data;
    uint32_t dest;
  };

  friend class WasmFunctionBuilder;
  Zone* zone_;
  ZoneVector<Type> types_;
  ZoneVector<WasmFunctionImport> function_imports_;
  ZoneVector<WasmGlobalImport> global_imports_;
  ZoneVector<WasmExport> exports_;
  ZoneVector<WasmFunctionBuilder*> functions_;
  ZoneVector<WasmTable> tables_;
  ZoneVector<WasmDataSegment> data_segments_;
  ZoneVector<uint32_t> indirect_functions_;
  ZoneVector<WasmGlobal> globals_;
  ZoneVector<int> exceptions_;
  ZoneUnorderedMap<FunctionSig, uint32_t> signature_map_;
  int start_function_index_;
  uint32_t max_table_size_ = 0;
  uint32_t min_memory_size_;
  uint32_t max_memory_size_;
  bool has_max_memory_size_;
  bool has_shared_memory_;
#if DEBUG
  // Once AddExportedImport is called, no more imports can be added.
  bool adding_imports_allowed_ = true;
  // Indirect functions must be allocated before adding extra tables.
  bool allocating_indirect_functions_allowed_ = true;
#endif
};

inline FunctionSig* WasmFunctionBuilder::signature() {
  return builder_->types_[signature_index_].sig;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MODULE_BUILDER_H_
