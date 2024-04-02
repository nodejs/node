// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_EH_FRAME_H_
#define V8_DIAGNOSTICS_EH_FRAME_H_

#include "src/base/compiler-specific.h"
#include "src/base/memory.h"
#include "src/codegen/register.h"
#include "src/common/globals.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class CodeDesc;

class V8_EXPORT_PRIVATE EhFrameConstants final
    : public NON_EXPORTED_BASE(AllStatic) {
 public:
  enum class DwarfOpcodes : byte {
    kNop = 0x00,
    kAdvanceLoc1 = 0x02,
    kAdvanceLoc2 = 0x03,
    kAdvanceLoc4 = 0x04,
    kRestoreExtended = 0x06,
    kSameValue = 0x08,
    kDefCfa = 0x0c,
    kDefCfaRegister = 0x0d,
    kDefCfaOffset = 0x0e,
    kOffsetExtendedSf = 0x11,
  };

  enum DwarfEncodingSpecifiers : byte {
    kUData4 = 0x03,
    kSData4 = 0x0b,
    kPcRel = 0x10,
    kDataRel = 0x30,
    kOmit = 0xff,
  };

  static const int kLocationTag = 1;
  static const int kLocationMask = 0x3f;
  static const int kLocationMaskSize = 6;

  static const int kSavedRegisterTag = 2;
  static const int kSavedRegisterMask = 0x3f;
  static const int kSavedRegisterMaskSize = 6;

  static const int kFollowInitialRuleTag = 3;
  static const int kFollowInitialRuleMask = 0x3f;
  static const int kFollowInitialRuleMaskSize = 6;

  static const int kProcedureAddressOffsetInFde = 2 * kInt32Size;
  static const int kProcedureSizeOffsetInFde = 3 * kInt32Size;

  static const int kInitialStateOffsetInCie = 19;
  static const int kEhFrameTerminatorSize = 4;

  // Defined in eh-writer-<arch>.cc
  static const int kCodeAlignmentFactor;
  static const int kDataAlignmentFactor;

  static const int kFdeVersionSize = 1;
  static const int kFdeEncodingSpecifiersSize = 3;

  static const int kEhFrameHdrVersion = 1;
  static const int kEhFrameHdrSize = 20;
};

class V8_EXPORT_PRIVATE EhFrameWriter {
 public:
  explicit EhFrameWriter(Zone* zone);
  EhFrameWriter(const EhFrameWriter&) = delete;
  EhFrameWriter& operator=(const EhFrameWriter&) = delete;

  // The empty frame is a hack to trigger fp-based unwinding in Linux perf
  // compiled with libunwind support when processing DWARF-based call graphs.
  //
  // It is effectively a valid eh_frame_hdr with an empty look up table.
  //
  static void WriteEmptyEhFrame(std::ostream& stream);

  // Write the CIE and FDE header. Call it before any other method.
  void Initialize();

  void AdvanceLocation(int pc_offset);

  // The <base_address> is the one to which all <offset>s in SaveRegisterToStack
  // directives are relative. It is given by <base_register> + <base_offset>.
  //
  // The <base_offset> must be positive or 0.
  //
  void SetBaseAddressRegister(Register base_register);
  void SetBaseAddressOffset(int base_offset);
  void IncreaseBaseAddressOffset(int base_delta) {
    SetBaseAddressOffset(base_offset_ + base_delta);
  }
  void SetBaseAddressRegisterAndOffset(Register base_register, int base_offset);

  // Register saved at location <base_address> + <offset>.
  // The <offset> must be a multiple of EhFrameConstants::kDataAlignment.
  void RecordRegisterSavedToStack(Register name, int offset) {
    RecordRegisterSavedToStack(RegisterToDwarfCode(name), offset);
  }

  // Directly accepts a DWARF register code, needed for
  // handling pseudo-registers on some platforms.
  void RecordRegisterSavedToStack(int dwarf_register_code, int offset);

  // The register has not been modified from the previous frame.
  void RecordRegisterNotModified(Register name);
  void RecordRegisterNotModified(int dwarf_register_code);

  // The register follows the rule defined in the CIE.
  void RecordRegisterFollowsInitialRule(Register name);
  void RecordRegisterFollowsInitialRule(int dwarf_register_code);

  void Finish(int code_size);

  // Remember to call Finish() before GetEhFrame().
  //
  // The EhFrameWriter instance owns the buffer pointed by
  // CodeDesc::unwinding_info, and must outlive any use of the CodeDesc.
  //
  void GetEhFrame(CodeDesc* desc);

  int last_pc_offset() const { return last_pc_offset_; }
  Register base_register() const { return base_register_; }
  int base_offset() const { return base_offset_; }

 private:
  enum class InternalState { kUndefined, kInitialized, kFinalized };

  static const uint32_t kInt32Placeholder = 0xdeadc0de;

  void WriteSLeb128(int32_t value);
  void WriteULeb128(uint32_t value);

  void WriteByte(byte value) { eh_frame_buffer_.push_back(value); }
  void WriteOpcode(EhFrameConstants::DwarfOpcodes opcode) {
    WriteByte(static_cast<byte>(opcode));
  }
  void WriteBytes(const byte* start, int size) {
    eh_frame_buffer_.insert(eh_frame_buffer_.end(), start, start + size);
  }
  void WriteInt16(uint16_t value) {
    WriteBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
  }
  void WriteInt32(uint32_t value) {
    WriteBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
  }
  void PatchInt32(int base_offset, uint32_t value) {
    DCHECK_EQ(
        base::ReadUnalignedValue<uint32_t>(
            reinterpret_cast<Address>(eh_frame_buffer_.data()) + base_offset),
        kInt32Placeholder);
    DCHECK_LT(base_offset + kInt32Size, eh_frame_offset());
    base::WriteUnalignedValue<uint32_t>(
        reinterpret_cast<Address>(eh_frame_buffer_.data()) + base_offset,
        value);
  }

  // Write the common information entry, which includes encoding specifiers,
  // alignment factors, the return address (pseudo) register code and the
  // directives to construct the initial state of the unwinding table.
  void WriteCie();

  // Write the header of the function data entry, containing a pointer to the
  // correspondent CIE and the position and size of the associated routine.
  void WriteFdeHeader();

  // Write the contents of the .eh_frame_hdr section, including encoding
  // specifiers and the routine => FDE lookup table.
  void WriteEhFrameHdr(int code_size);

  // Write nops until the size reaches a multiple of 8 bytes.
  void WritePaddingToAlignedSize(int unpadded_size);

  int GetProcedureAddressOffset() const {
    return fde_offset() + EhFrameConstants::kProcedureAddressOffsetInFde;
  }

  int GetProcedureSizeOffset() const {
    return fde_offset() + EhFrameConstants::kProcedureSizeOffsetInFde;
  }

  int eh_frame_offset() const {
    return static_cast<int>(eh_frame_buffer_.size());
  }

  int fde_offset() const { return cie_size_; }

  // Platform specific functions implemented in eh-frame-<arch>.cc

  static int RegisterToDwarfCode(Register name);

  // Write directives to build the initial state in the CIE.
  void WriteInitialStateInCie();

  // Write the return address (pseudo) register code.
  void WriteReturnAddressRegisterCode();

  int cie_size_;
  int last_pc_offset_;
  InternalState writer_state_;
  Register base_register_;
  int base_offset_;
  ZoneVector<byte> eh_frame_buffer_;
};

class V8_EXPORT_PRIVATE EhFrameIterator {
 public:
  EhFrameIterator(const byte* start, const byte* end)
      : start_(start), next_(start), end_(end) {
    DCHECK_LE(start, end);
  }

  void SkipCie() {
    DCHECK_EQ(next_, start_);
    next_ +=
        base::ReadUnalignedValue<uint32_t>(reinterpret_cast<Address>(next_)) +
        kInt32Size;
  }

  void SkipToFdeDirectives() {
    SkipCie();
    // Skip the FDE header.
    Skip(kDirectivesOffsetInFde);
  }

  void Skip(int how_many) {
    DCHECK_GE(how_many, 0);
    next_ += how_many;
    DCHECK_LE(next_, end_);
  }

  uint32_t GetNextUInt32() { return GetNextValue<uint32_t>(); }
  uint16_t GetNextUInt16() { return GetNextValue<uint16_t>(); }
  byte GetNextByte() { return GetNextValue<byte>(); }
  EhFrameConstants::DwarfOpcodes GetNextOpcode() {
    return static_cast<EhFrameConstants::DwarfOpcodes>(GetNextByte());
  }

  uint32_t GetNextULeb128();
  int32_t GetNextSLeb128();

  bool Done() const {
    DCHECK_LE(next_, end_);
    return next_ == end_;
  }

  int GetCurrentOffset() const {
    DCHECK_GE(next_, start_);
    return static_cast<int>(next_ - start_);
  }

  int GetBufferSize() { return static_cast<int>(end_ - start_); }

  const void* current_address() const {
    return reinterpret_cast<const void*>(next_);
  }

 private:
  static const int kDirectivesOffsetInFde = 4 * kInt32Size + 1;

  static uint32_t DecodeULeb128(const byte* encoded, int* encoded_size);
  static int32_t DecodeSLeb128(const byte* encoded, int* encoded_size);

  template <typename T>
  T GetNextValue() {
    T result;
    DCHECK_LE(next_ + sizeof(result), end_);
    result = base::ReadUnalignedValue<T>(reinterpret_cast<Address>(next_));
    next_ += sizeof(result);
    return result;
  }

  const byte* start_;
  const byte* next_;
  const byte* end_;
};

#ifdef ENABLE_DISASSEMBLER

class EhFrameDisassembler final {
 public:
  EhFrameDisassembler(const byte* start, const byte* end)
      : start_(start), end_(end) {
    DCHECK_LT(start, end);
  }
  EhFrameDisassembler(const EhFrameDisassembler&) = delete;
  EhFrameDisassembler& operator=(const EhFrameDisassembler&) = delete;

  void DisassembleToStream(std::ostream& stream);

 private:
  static void DumpDwarfDirectives(std::ostream& stream, const byte* start,
                                  const byte* end);

  static const char* DwarfRegisterCodeToString(int code);

  const byte* start_;
  const byte* end_;
};

#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_DIAGNOSTICS_EH_FRAME_H_
