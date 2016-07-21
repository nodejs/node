// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/handles.h"
#include "src/v8.h"
#include "src/zone-containers.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/encoder.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/v8memory.h"

#if DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_encoder) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

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
  LEBHelper::write_u32v(b, static_cast<uint32_t>(val));
}

// Sections all start with a size, but it's unknown at the start.
// We generate a large varint which we then fixup later when the size is known.
//
// TODO(jfb) Not strictly necessary since sizes are calculated ahead of time.
const size_t kPaddedVarintSize = 5;

void FixupSection(byte* start, byte* end) {
  // Same as LEBHelper::write_u32v, but fixed-width with zeroes in the MSBs.
  size_t val = end - start - kPaddedVarintSize;
  TRACE("  fixup %u\n", (unsigned)val);
  for (size_t pos = 0; pos != kPaddedVarintSize; ++pos) {
    size_t next = val >> 7;
    byte out = static_cast<byte>(val & 0x7f);
    if (pos != kPaddedVarintSize - 1) {
      *(start++) = 0x80 | out;
      val = next;
    } else {
      *(start++) = out;
      // TODO(jfb) check that the pre-allocated fixup size isn't overflowed.
    }
  }
}

// Returns the start of the section, where the section VarInt size is.
byte* EmitSection(WasmSection::Code code, byte** b) {
  // Emit the section name.
  const char* name = WasmSection::getName(code);
  TRACE("emit section: %s\n", name);
  size_t length = WasmSection::getNameLength(code);
  EmitVarInt(b, length);  // Section name string size.
  for (size_t i = 0; i != length; ++i) EmitUint8(b, name[i]);

  // Emit a placeholder for the length.
  byte* start = *b;
  for (size_t padding = 0; padding != kPaddedVarintSize; ++padding) {
    EmitUint8(b, 0xff);  // Will get fixed up later.
  }

  return start;
}
}  // namespace

WasmFunctionBuilder::WasmFunctionBuilder(Zone* zone)
    : locals_(zone), exported_(0), body_(zone), name_(zone) {}

void WasmFunctionBuilder::EmitVarInt(uint32_t val) {
  byte buffer[8];
  byte* ptr = buffer;
  LEBHelper::write_u32v(&ptr, val);
  for (byte* p = buffer; p < ptr; p++) {
    body_.push_back(*p);
  }
}

void WasmFunctionBuilder::SetSignature(FunctionSig* sig) {
  DCHECK(!locals_.has_sig());
  locals_.set_sig(sig);
}

uint32_t WasmFunctionBuilder::AddLocal(LocalType type) {
  DCHECK(locals_.has_sig());
  return locals_.AddLocals(1, type);
}

void WasmFunctionBuilder::EmitGetLocal(uint32_t local_index) {
  EmitWithVarInt(kExprGetLocal, local_index);
}

void WasmFunctionBuilder::EmitSetLocal(uint32_t local_index) {
  EmitWithVarInt(kExprSetLocal, local_index);
}

void WasmFunctionBuilder::EmitCode(const byte* code, uint32_t code_size) {
  for (size_t i = 0; i < code_size; i++) {
    body_.push_back(code[i]);
  }
}

void WasmFunctionBuilder::Emit(WasmOpcode opcode) {
  body_.push_back(static_cast<byte>(opcode));
}

void WasmFunctionBuilder::EmitWithU8(WasmOpcode opcode, const byte immediate) {
  body_.push_back(static_cast<byte>(opcode));
  body_.push_back(immediate);
}

void WasmFunctionBuilder::EmitWithU8U8(WasmOpcode opcode, const byte imm1,
                                       const byte imm2) {
  body_.push_back(static_cast<byte>(opcode));
  body_.push_back(imm1);
  body_.push_back(imm2);
}

void WasmFunctionBuilder::EmitWithVarInt(WasmOpcode opcode,
                                         uint32_t immediate) {
  body_.push_back(static_cast<byte>(opcode));
  EmitVarInt(immediate);
}

void WasmFunctionBuilder::EmitI32Const(int32_t value) {
  // TODO(titzer): variable-length signed and unsigned i32 constants.
  if (-128 <= value && value <= 127) {
    EmitWithU8(kExprI8Const, static_cast<byte>(value));
  } else {
    byte code[] = {WASM_I32V_5(value)};
    EmitCode(code, sizeof(code));
  }
}

void WasmFunctionBuilder::Exported(uint8_t flag) { exported_ = flag; }

void WasmFunctionBuilder::SetName(const char* name, int name_length) {
  name_.clear();
  if (name_length > 0) {
    for (int i = 0; i < name_length; i++) {
      name_.push_back(*(name + i));
    }
  }
}

WasmFunctionEncoder* WasmFunctionBuilder::Build(Zone* zone,
                                                WasmModuleBuilder* mb) const {
  WasmFunctionEncoder* e =
      new (zone) WasmFunctionEncoder(zone, locals_, exported_);
  // TODO(titzer): lame memcpy here.
  e->body_.insert(e->body_.begin(), body_.begin(), body_.end());
  e->signature_index_ = mb->AddSignature(locals_.get_sig());
  e->name_.insert(e->name_.begin(), name_.begin(), name_.end());
  return e;
}

WasmFunctionEncoder::WasmFunctionEncoder(Zone* zone, LocalDeclEncoder locals,
                                         bool exported)
    : locals_(locals), exported_(exported), body_(zone), name_(zone) {}

uint32_t WasmFunctionEncoder::HeaderSize() const {
  uint32_t size = 3;
  size += 2;
  if (HasName()) {
    uint32_t name_size = NameSize();
    size +=
        static_cast<uint32_t>(LEBHelper::sizeof_u32v(name_size)) + name_size;
  }
  return size;
}

uint32_t WasmFunctionEncoder::BodySize(void) const {
  return static_cast<uint32_t>(body_.size() + locals_.Size());
}

uint32_t WasmFunctionEncoder::NameSize() const {
  return HasName() ? static_cast<uint32_t>(name_.size()) : 0;
}

void WasmFunctionEncoder::Serialize(byte* buffer, byte** header,
                                    byte** body) const {
  uint8_t decl_bits = (exported_ ? kDeclFunctionExport : 0) |
                      (HasName() ? kDeclFunctionName : 0);

  EmitUint8(header, decl_bits);
  EmitUint16(header, signature_index_);

  if (HasName()) {
    EmitVarInt(header, NameSize());
    for (size_t i = 0; i < name_.size(); ++i) {
      EmitUint8(header, name_[i]);
    }
  }

  EmitUint16(header, static_cast<uint16_t>(body_.size() + locals_.Size()));
  (*header) += locals_.Emit(*header);
  if (body_.size() > 0) {
    std::memcpy(*header, &body_[0], body_.size());
    (*header) += body_.size();
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
  EmitVarInt(header, dest_);
  EmitVarInt(header, static_cast<uint32_t>(data_.size()));

  std::memcpy(*header, &data_[0], data_.size());
  (*header) += data_.size();
}

WasmModuleBuilder::WasmModuleBuilder(Zone* zone)
    : zone_(zone),
      signatures_(zone),
      imports_(zone),
      functions_(zone),
      data_segments_(zone),
      indirect_functions_(zone),
      globals_(zone),
      signature_map_(zone),
      start_function_index_(-1) {}

uint32_t WasmModuleBuilder::AddFunction() {
  functions_.push_back(new (zone_) WasmFunctionBuilder(zone_));
  return static_cast<uint32_t>(functions_.size() - 1);
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

uint32_t WasmModuleBuilder::AddSignature(FunctionSig* sig) {
  SignatureMap::iterator pos = signature_map_.find(sig);
  if (pos != signature_map_.end()) {
    return pos->second;
  } else {
    uint32_t index = static_cast<uint32_t>(signatures_.size());
    signature_map_[sig] = index;
    signatures_.push_back(sig);
    return index;
  }
}

void WasmModuleBuilder::AddIndirectFunction(uint32_t index) {
  indirect_functions_.push_back(index);
}

uint32_t WasmModuleBuilder::AddImport(const char* name, int name_length,
                                      FunctionSig* sig) {
  imports_.push_back({AddSignature(sig), name, name_length});
  return static_cast<uint32_t>(imports_.size() - 1);
}

void WasmModuleBuilder::MarkStartFunction(uint32_t index) {
  start_function_index_ = index;
}

WasmModuleWriter* WasmModuleBuilder::Build(Zone* zone) {
  WasmModuleWriter* writer = new (zone) WasmModuleWriter(zone);
  for (auto import : imports_) {
    writer->imports_.push_back(import);
  }
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
  writer->start_function_index_ = start_function_index_;
  return writer;
}

uint32_t WasmModuleBuilder::AddGlobal(MachineType type, bool exported) {
  globals_.push_back(std::make_pair(type, exported));
  return static_cast<uint32_t>(globals_.size() - 1);
}

WasmModuleWriter::WasmModuleWriter(Zone* zone)
    : imports_(zone),
      functions_(zone),
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

  void AddSection(WasmSection::Code code, size_t other_size) {
    Add(kPaddedVarintSize +
            LEBHelper::sizeof_u32v(WasmSection::getNameLength(code)) +
            WasmSection::getNameLength(code),
        0);
    if (other_size) Add(LEBHelper::sizeof_u32v(other_size), 0);
  }
};

WasmModuleIndex* WasmModuleWriter::WriteTo(Zone* zone) const {
  Sizes sizes = {0, 0};

  sizes.Add(2 * sizeof(uint32_t), 0);  // header

  if (globals_.size() > 0) {
    sizes.AddSection(WasmSection::Code::Globals, globals_.size());
    /* These globals never have names, so are always 3 bytes. */
    sizes.Add(3 * globals_.size(), 0);
    TRACE("Size after globals: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (signatures_.size() > 0) {
    sizes.AddSection(WasmSection::Code::Signatures, signatures_.size());
    for (auto sig : signatures_) {
      sizes.Add(1 + LEBHelper::sizeof_u32v(sig->parameter_count()) +
                    sig->parameter_count() +
                    LEBHelper::sizeof_u32v(sig->return_count()) +
                    sig->return_count(),
                0);
    }
    TRACE("Size after signatures: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (functions_.size() > 0) {
    sizes.AddSection(WasmSection::Code::OldFunctions, functions_.size());
    for (auto function : functions_) {
      sizes.Add(function->HeaderSize() + function->BodySize(),
                function->NameSize());
    }
    TRACE("Size after functions: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (imports_.size() > 0) {
    sizes.AddSection(WasmSection::Code::ImportTable, imports_.size());
    for (auto import : imports_) {
      sizes.Add(LEBHelper::sizeof_u32v(import.sig_index), 0);
      sizes.Add(LEBHelper::sizeof_u32v(import.name_length), 0);
      sizes.Add(import.name_length, 0);
      sizes.Add(1, 0);
    }
    TRACE("Size after imports: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (indirect_functions_.size() > 0) {
    sizes.AddSection(WasmSection::Code::FunctionTable,
                     indirect_functions_.size());
    for (auto function_index : indirect_functions_) {
      sizes.Add(LEBHelper::sizeof_u32v(function_index), 0);
    }
    TRACE("Size after indirect functions: %u, %u\n",
          (unsigned)sizes.header_size, (unsigned)sizes.body_size);
  }

  sizes.AddSection(WasmSection::Code::Memory, 0);
  sizes.Add(kDeclMemorySize, 0);
  TRACE("Size after memory: %u, %u\n", (unsigned)sizes.header_size,
        (unsigned)sizes.body_size);

  if (start_function_index_ >= 0) {
    sizes.AddSection(WasmSection::Code::StartFunction, 0);
    sizes.Add(LEBHelper::sizeof_u32v(start_function_index_), 0);
    TRACE("Size after start: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (data_segments_.size() > 0) {
    sizes.AddSection(WasmSection::Code::DataSegments, data_segments_.size());
    for (auto segment : data_segments_) {
      sizes.Add(segment->HeaderSize(), segment->BodySize());
    }
    TRACE("Size after data segments: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  if (sizes.body_size > 0) {
    sizes.AddSection(WasmSection::Code::End, 0);
    TRACE("Size after end: %u, %u\n", (unsigned)sizes.header_size,
          (unsigned)sizes.body_size);
  }

  ZoneVector<uint8_t> buffer_vector(sizes.total(), zone);
  byte* buffer = &buffer_vector[0];
  byte* header = buffer;
  byte* body = buffer + sizes.header_size;

  // -- emit magic -------------------------------------------------------------
  TRACE("emit magic\n");
  EmitUint32(&header, kWasmMagic);
  EmitUint32(&header, kWasmVersion);

  // -- emit globals -----------------------------------------------------------
  if (globals_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::Globals, &header);
    EmitVarInt(&header, globals_.size());

    for (auto global : globals_) {
      EmitVarInt(&header, 0);  // Length of the global name.
      EmitUint8(&header, WasmOpcodes::MemTypeCodeFor(global.first));
      EmitUint8(&header, global.second);
    }
    FixupSection(section, header);
  }

  // -- emit signatures --------------------------------------------------------
  if (signatures_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::Signatures, &header);
    EmitVarInt(&header, signatures_.size());

    for (FunctionSig* sig : signatures_) {
      EmitUint8(&header, kWasmFunctionTypeForm);
      EmitVarInt(&header, sig->parameter_count());
      for (size_t j = 0; j < sig->parameter_count(); j++) {
        EmitUint8(&header, WasmOpcodes::LocalTypeCodeFor(sig->GetParam(j)));
      }
      EmitVarInt(&header, sig->return_count());
      for (size_t j = 0; j < sig->return_count(); j++) {
        EmitUint8(&header, WasmOpcodes::LocalTypeCodeFor(sig->GetReturn(j)));
      }
    }
    FixupSection(section, header);
  }

  // -- emit imports -----------------------------------------------------------
  if (imports_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::ImportTable, &header);
    EmitVarInt(&header, imports_.size());
    for (auto import : imports_) {
      EmitVarInt(&header, import.sig_index);
      EmitVarInt(&header, import.name_length);
      std::memcpy(header, import.name, import.name_length);
      header += import.name_length;
      EmitVarInt(&header, 0);
    }
    FixupSection(section, header);
  }

  // -- emit functions ---------------------------------------------------------
  if (functions_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::OldFunctions, &header);
    EmitVarInt(&header, functions_.size());

    for (auto func : functions_) {
      func->Serialize(buffer, &header, &body);
    }
    FixupSection(section, header);
  }

  // -- emit function table ----------------------------------------------------
  if (indirect_functions_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::FunctionTable, &header);
    EmitVarInt(&header, indirect_functions_.size());

    for (auto index : indirect_functions_) {
      EmitVarInt(&header, index);
    }
    FixupSection(section, header);
  }

  // -- emit memory declaration ------------------------------------------------
  {
    byte* section = EmitSection(WasmSection::Code::Memory, &header);
    EmitVarInt(&header, 16);  // min memory size
    EmitVarInt(&header, 16);  // max memory size
    EmitUint8(&header, 0);    // memory export
    static_assert(kDeclMemorySize == 3, "memory size must match emit above");
    FixupSection(section, header);
  }

  // -- emit start function index ----------------------------------------------
  if (start_function_index_ >= 0) {
    byte* section = EmitSection(WasmSection::Code::StartFunction, &header);
    EmitVarInt(&header, start_function_index_);
    FixupSection(section, header);
  }

  // -- emit data segments -----------------------------------------------------
  if (data_segments_.size() > 0) {
    byte* section = EmitSection(WasmSection::Code::DataSegments, &header);
    EmitVarInt(&header, data_segments_.size());

    for (auto segment : data_segments_) {
      segment->Serialize(buffer, &header, &body);
    }
    FixupSection(section, header);
  }

  if (sizes.body_size > 0) {
    byte* section = EmitSection(WasmSection::Code::End, &header);
    FixupSection(section, header);
  }

  return new (zone) WasmModuleIndex(buffer, buffer + sizes.total());
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
