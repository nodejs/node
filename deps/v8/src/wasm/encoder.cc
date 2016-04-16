// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/handles.h"
#include "src/v8.h"
#include "src/zone-containers.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/encoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/v8memory.h"

namespace v8 {
namespace internal {
namespace wasm {

/*TODO: add error cases for adding too many locals, too many functions and bad
  indices in body */

namespace {
void EmitUint8(byte** b, uint8_t x) {
  Memory::uint8_at(*b) = x;
  *b += 1;
}


void EmitUint16(byte** b, uint16_t x) {
  WriteUnalignedUInt16(*b, x);
  *b += 2;
}


void EmitUint32(byte** b, uint32_t x) {
  WriteUnalignedUInt32(*b, x);
  *b += 4;
}


void EmitVarInt(byte** b, size_t val) {
  while (true) {
    size_t next = val >> 7;
    byte out = static_cast<byte>(val & 0x7f);
    if (next) {
      *((*b)++) = 0x80 | out;
      val = next;
    } else {
      *((*b)++) = out;
      break;
    }
  }
}
}  // namespace


struct WasmFunctionBuilder::Type {
  bool param_;
  LocalType type_;
};


WasmFunctionBuilder::WasmFunctionBuilder(Zone* zone)
    : return_type_(kAstI32),
      locals_(zone),
      exported_(0),
      external_(0),
      body_(zone),
      local_indices_(zone),
      name_(zone) {}


uint16_t WasmFunctionBuilder::AddParam(LocalType type) {
  return AddVar(type, true);
}


uint16_t WasmFunctionBuilder::AddLocal(LocalType type) {
  return AddVar(type, false);
}


uint16_t WasmFunctionBuilder::AddVar(LocalType type, bool param) {
  locals_.push_back({param, type});
  return static_cast<uint16_t>(locals_.size() - 1);
}


void WasmFunctionBuilder::ReturnType(LocalType type) { return_type_ = type; }


void WasmFunctionBuilder::EmitCode(const byte* code, uint32_t code_size) {
  EmitCode(code, code_size, nullptr, 0);
}


void WasmFunctionBuilder::EmitCode(const byte* code, uint32_t code_size,
                                   const uint32_t* local_indices,
                                   uint32_t indices_size) {
  size_t size = body_.size();
  for (size_t i = 0; i < code_size; i++) {
    body_.push_back(code[i]);
  }
  for (size_t i = 0; i < indices_size; i++) {
    local_indices_.push_back(local_indices[i] + static_cast<uint32_t>(size));
  }
}


void WasmFunctionBuilder::Emit(WasmOpcode opcode) {
  body_.push_back(static_cast<byte>(opcode));
}


void WasmFunctionBuilder::EmitWithU8(WasmOpcode opcode, const byte immediate) {
  body_.push_back(static_cast<byte>(opcode));
  body_.push_back(immediate);
}


uint32_t WasmFunctionBuilder::EmitEditableImmediate(const byte immediate) {
  body_.push_back(immediate);
  return static_cast<uint32_t>(body_.size()) - 1;
}


void WasmFunctionBuilder::EditImmediate(uint32_t offset, const byte immediate) {
  DCHECK(offset < body_.size());
  body_[offset] = immediate;
}


void WasmFunctionBuilder::Exported(uint8_t flag) { exported_ = flag; }


void WasmFunctionBuilder::External(uint8_t flag) { external_ = flag; }

void WasmFunctionBuilder::SetName(const unsigned char* name, int name_length) {
  name_.clear();
  if (name_length > 0) {
    for (int i = 0; i < name_length; i++) {
      name_.push_back(*(name + i));
    }
    name_.push_back('\0');
  }
}


WasmFunctionEncoder* WasmFunctionBuilder::Build(Zone* zone,
                                                WasmModuleBuilder* mb) const {
  WasmFunctionEncoder* e =
      new (zone) WasmFunctionEncoder(zone, return_type_, exported_, external_);
  uint16_t* var_index = zone->NewArray<uint16_t>(locals_.size());
  IndexVars(e, var_index);
  if (body_.size() > 0) {
    // TODO(titzer): iterate over local indexes, not the bytes.
    const byte* start = &body_[0];
    const byte* end = start + body_.size();
    size_t local_index = 0;
    for (size_t i = 0; i < body_.size();) {
      if (local_index < local_indices_.size() &&
          i == local_indices_[local_index]) {
        int length = 0;
        uint32_t index;
        ReadUnsignedLEB128Operand(start + i, end, &length, &index);
        uint16_t new_index = var_index[index];
        const std::vector<uint8_t>& index_vec = UnsignedLEB128From(new_index);
        for (size_t j = 0; j < index_vec.size(); j++) {
          e->body_.push_back(index_vec.at(j));
        }
        i += length;
        local_index++;
      } else {
        e->body_.push_back(*(start + i));
        i++;
      }
    }
  }
  FunctionSig::Builder sig(zone, return_type_ == kAstStmt ? 0 : 1,
                           e->params_.size());
  if (return_type_ != kAstStmt) {
    sig.AddReturn(static_cast<LocalType>(return_type_));
  }
  for (size_t i = 0; i < e->params_.size(); i++) {
    sig.AddParam(static_cast<LocalType>(e->params_[i]));
  }
  e->signature_index_ = mb->AddSignature(sig.Build());
  e->name_.insert(e->name_.begin(), name_.begin(), name_.end());
  return e;
}


void WasmFunctionBuilder::IndexVars(WasmFunctionEncoder* e,
                                    uint16_t* var_index) const {
  uint16_t param = 0;
  uint16_t i32 = 0;
  uint16_t i64 = 0;
  uint16_t f32 = 0;
  uint16_t f64 = 0;
  for (size_t i = 0; i < locals_.size(); i++) {
    if (locals_.at(i).param_) {
      param++;
    } else if (locals_.at(i).type_ == kAstI32) {
      i32++;
    } else if (locals_.at(i).type_ == kAstI64) {
      i64++;
    } else if (locals_.at(i).type_ == kAstF32) {
      f32++;
    } else if (locals_.at(i).type_ == kAstF64) {
      f64++;
    }
  }
  e->local_i32_count_ = i32;
  e->local_i64_count_ = i64;
  e->local_f32_count_ = f32;
  e->local_f64_count_ = f64;
  f64 = param + i32 + i64 + f32;
  f32 = param + i32 + i64;
  i64 = param + i32;
  i32 = param;
  param = 0;
  for (size_t i = 0; i < locals_.size(); i++) {
    if (locals_.at(i).param_) {
      e->params_.push_back(locals_.at(i).type_);
      var_index[i] = param++;
    } else if (locals_.at(i).type_ == kAstI32) {
      var_index[i] = i32++;
    } else if (locals_.at(i).type_ == kAstI64) {
      var_index[i] = i64++;
    } else if (locals_.at(i).type_ == kAstF32) {
      var_index[i] = f32++;
    } else if (locals_.at(i).type_ == kAstF64) {
      var_index[i] = f64++;
    }
  }
}


WasmFunctionEncoder::WasmFunctionEncoder(Zone* zone, LocalType return_type,
                                         bool exported, bool external)
    : params_(zone),
      exported_(exported),
      external_(external),
      body_(zone),
      name_(zone) {}


uint32_t WasmFunctionEncoder::HeaderSize() const {
  uint32_t size = 3;
  if (HasLocals()) size += 8;
  if (!external_) size += 2;
  if (HasName()) size += 4;
  return size;
}


uint32_t WasmFunctionEncoder::BodySize(void) const {
  return external_ ? 0 : static_cast<uint32_t>(body_.size());
}


uint32_t WasmFunctionEncoder::NameSize() const {
  return HasName() ? static_cast<uint32_t>(name_.size()) : 0;
}


void WasmFunctionEncoder::Serialize(byte* buffer, byte** header,
                                    byte** body) const {
  uint8_t decl_bits = (exported_ ? kDeclFunctionExport : 0) |
                      (external_ ? kDeclFunctionImport : 0) |
                      (HasLocals() ? kDeclFunctionLocals : 0) |
                      (HasName() ? kDeclFunctionName : 0);

  EmitUint8(header, decl_bits);
  EmitUint16(header, signature_index_);

  if (HasName()) {
    uint32_t name_offset = static_cast<uint32_t>(*body - buffer);
    EmitUint32(header, name_offset);
    std::memcpy(*body, &name_[0], name_.size());
    (*body) += name_.size();
  }

  if (HasLocals()) {
    EmitUint16(header, local_i32_count_);
    EmitUint16(header, local_i64_count_);
    EmitUint16(header, local_f32_count_);
    EmitUint16(header, local_f64_count_);
  }

  if (!external_) {
    EmitUint16(header, static_cast<uint16_t>(body_.size()));
    if (body_.size() > 0) {
      std::memcpy(*header, &body_[0], body_.size());
      (*header) += body_.size();
    }
  }
}


WasmDataSegmentEncoder::WasmDataSegmentEncoder(Zone* zone, const byte* data,
                                               uint32_t size, uint32_t dest)
    : data_(zone), dest_(dest) {
  for (size_t i = 0; i < size; i++) {
    data_.push_back(data[i]);
  }
}


uint32_t WasmDataSegmentEncoder::HeaderSize() const {
  static const int kDataSegmentSize = 13;
  return kDataSegmentSize;
}


uint32_t WasmDataSegmentEncoder::BodySize() const {
  return static_cast<uint32_t>(data_.size());
}


void WasmDataSegmentEncoder::Serialize(byte* buffer, byte** header,
                                       byte** body) const {
  uint32_t body_offset = static_cast<uint32_t>(*body - buffer);
  EmitUint32(header, dest_);
  EmitUint32(header, body_offset);
  EmitUint32(header, static_cast<uint32_t>(data_.size()));
  EmitUint8(header, 1);  // init

  std::memcpy(*body, &data_[0], data_.size());
  (*body) += data_.size();
}


WasmModuleBuilder::WasmModuleBuilder(Zone* zone)
    : zone_(zone),
      signatures_(zone),
      functions_(zone),
      data_segments_(zone),
      indirect_functions_(zone),
      globals_(zone),
      signature_map_(zone) {}


uint16_t WasmModuleBuilder::AddFunction() {
  functions_.push_back(new (zone_) WasmFunctionBuilder(zone_));
  return static_cast<uint16_t>(functions_.size() - 1);
}


WasmFunctionBuilder* WasmModuleBuilder::FunctionAt(size_t index) {
  if (functions_.size() > index) {
    return functions_.at(index);
  } else {
    return nullptr;
  }
}


void WasmModuleBuilder::AddDataSegment(WasmDataSegmentEncoder* data) {
  data_segments_.push_back(data);
}


bool WasmModuleBuilder::CompareFunctionSigs::operator()(FunctionSig* a,
                                                        FunctionSig* b) const {
  if (a->return_count() < b->return_count()) return true;
  if (a->return_count() > b->return_count()) return false;
  if (a->parameter_count() < b->parameter_count()) return true;
  if (a->parameter_count() > b->parameter_count()) return false;
  for (size_t r = 0; r < a->return_count(); r++) {
    if (a->GetReturn(r) < b->GetReturn(r)) return true;
    if (a->GetReturn(r) > b->GetReturn(r)) return false;
  }
  for (size_t p = 0; p < a->parameter_count(); p++) {
    if (a->GetParam(p) < b->GetParam(p)) return true;
    if (a->GetParam(p) > b->GetParam(p)) return false;
  }
  return false;
}


uint16_t WasmModuleBuilder::AddSignature(FunctionSig* sig) {
  SignatureMap::iterator pos = signature_map_.find(sig);
  if (pos != signature_map_.end()) {
    return pos->second;
  } else {
    uint16_t index = static_cast<uint16_t>(signatures_.size());
    signature_map_[sig] = index;
    signatures_.push_back(sig);
    return index;
  }
}


void WasmModuleBuilder::AddIndirectFunction(uint16_t index) {
  indirect_functions_.push_back(index);
}


WasmModuleWriter* WasmModuleBuilder::Build(Zone* zone) {
  WasmModuleWriter* writer = new (zone) WasmModuleWriter(zone);
  for (auto function : functions_) {
    writer->functions_.push_back(function->Build(zone, this));
  }
  for (auto segment : data_segments_) {
    writer->data_segments_.push_back(segment);
  }
  for (auto sig : signatures_) {
    writer->signatures_.push_back(sig);
  }
  for (auto index : indirect_functions_) {
    writer->indirect_functions_.push_back(index);
  }
  for (auto global : globals_) {
    writer->globals_.push_back(global);
  }
  return writer;
}


uint32_t WasmModuleBuilder::AddGlobal(MachineType type, bool exported) {
  globals_.push_back(std::make_pair(type, exported));
  return static_cast<uint32_t>(globals_.size() - 1);
}


WasmModuleWriter::WasmModuleWriter(Zone* zone)
    : functions_(zone),
      data_segments_(zone),
      signatures_(zone),
      indirect_functions_(zone),
      globals_(zone) {}


struct Sizes {
  size_t header_size;
  size_t body_size;

  size_t total() { return header_size + body_size; }

  void Add(size_t header, size_t body) {
    header_size += header;
    body_size += body;
  }

  void AddSection(size_t size) {
    if (size > 0) {
      Add(1, 0);
      while (size > 0) {
        Add(1, 0);
        size = size >> 7;
      }
    }
  }
};


WasmModuleIndex* WasmModuleWriter::WriteTo(Zone* zone) const {
  Sizes sizes = {0, 0};

  sizes.Add(1, 0);
  sizes.Add(kDeclMemorySize, 0);

  sizes.AddSection(signatures_.size());
  for (auto sig : signatures_) {
    sizes.Add(2 + sig->parameter_count(), 0);
  }

  sizes.AddSection(globals_.size());
  if (globals_.size() > 0) {
    sizes.Add(kDeclGlobalSize * globals_.size(), 0);
  }

  sizes.AddSection(functions_.size());
  for (auto function : functions_) {
    sizes.Add(function->HeaderSize() + function->BodySize(),
              function->NameSize());
  }

  sizes.AddSection(data_segments_.size());
  for (auto segment : data_segments_) {
    sizes.Add(segment->HeaderSize(), segment->BodySize());
  }

  sizes.AddSection(indirect_functions_.size());
  sizes.Add(2 * static_cast<uint32_t>(indirect_functions_.size()), 0);

  if (sizes.body_size > 0) sizes.Add(1, 0);

  ZoneVector<uint8_t> buffer_vector(sizes.total(), zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + sizes.header_size;

  // -- emit memory declaration ------------------------------------------------
  EmitUint8(&header, kDeclMemory);
  EmitUint8(&header, 16);  // min memory size
  EmitUint8(&header, 16);  // max memory size
  EmitUint8(&header, 0);   // memory export

  // -- emit globals -----------------------------------------------------------
  if (globals_.size() > 0) {
    EmitUint8(&header, kDeclGlobals);
    EmitVarInt(&header, globals_.size());

    for (auto global : globals_) {
      EmitUint32(&header, 0);
      EmitUint8(&header, WasmOpcodes::MemTypeCodeFor(global.first));
      EmitUint8(&header, global.second);
    }
  }

  // -- emit signatures --------------------------------------------------------
  if (signatures_.size() > 0) {
    EmitUint8(&header, kDeclSignatures);
    EmitVarInt(&header, signatures_.size());

    for (FunctionSig* sig : signatures_) {
      EmitUint8(&header, static_cast<byte>(sig->parameter_count()));
      if (sig->return_count() > 0) {
        EmitUint8(&header, WasmOpcodes::LocalTypeCodeFor(sig->GetReturn()));
      } else {
        EmitUint8(&header, kLocalVoid);
      }
      for (size_t j = 0; j < sig->parameter_count(); j++) {
        EmitUint8(&header, WasmOpcodes::LocalTypeCodeFor(sig->GetParam(j)));
      }
    }
  }

  // -- emit functions ---------------------------------------------------------
  if (functions_.size() > 0) {
    EmitUint8(&header, kDeclFunctions);
    EmitVarInt(&header, functions_.size());

    for (auto func : functions_) {
      func->Serialize(buffer, &header, &body);
    }
  }

  // -- emit data segments -----------------------------------------------------
  if (data_segments_.size() > 0) {
    EmitUint8(&header, kDeclDataSegments);
    EmitVarInt(&header, data_segments_.size());

    for (auto segment : data_segments_) {
      segment->Serialize(buffer, &header, &body);
    }
  }

  // -- emit function table ----------------------------------------------------
  if (indirect_functions_.size() > 0) {
    EmitUint8(&header, kDeclFunctionTable);
    EmitVarInt(&header, indirect_functions_.size());

    for (auto index : indirect_functions_) {
      EmitUint16(&header, index);
    }
  }

  if (sizes.body_size > 0) EmitUint8(&header, kDeclEnd);

  return new (zone) WasmModuleIndex(buffer, buffer + sizes.total());
}


std::vector<uint8_t> UnsignedLEB128From(uint32_t result) {
  std::vector<uint8_t> output;
  uint8_t next = 0;
  int shift = 0;
  do {
    next = static_cast<uint8_t>(result >> shift);
    if (((result >> shift) & 0xFFFFFF80) != 0) {
      next = next | 0x80;
    }
    output.push_back(next);
    shift += 7;
  } while ((next & 0x80) != 0);
  return output;
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
