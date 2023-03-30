// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/reloc-info.h"

#include "src/base/vlq.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/code-reference.h"
#include "src/codegen/external-reference-encoder.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code-inl.h"
#include "src/snapshot/embedded/embedded-data-inl.h"

namespace v8 {
namespace internal {

const char* const RelocInfo::kFillerCommentString = "DEOPTIMIZATION PADDING";

// -----------------------------------------------------------------------------
// Implementation of RelocInfoWriter and RelocIterator
//
// Relocation information is written backwards in memory, from high addresses
// towards low addresses, byte by byte.  Therefore, in the encodings listed
// below, the first byte listed it at the highest address, and successive
// bytes in the record are at progressively lower addresses.
//
// Encoding
//
// The most common modes are given single-byte encodings.  Also, it is
// easy to identify the type of reloc info and skip unwanted modes in
// an iteration.
//
// The encoding relies on the fact that there are fewer than 14
// different relocation modes using standard non-compact encoding.
//
// The first byte of a relocation record has a tag in its low 2 bits:
// Here are the record schemes, depending on the low tag and optional higher
// tags.
//
// Low tag:
//   00: embedded_object:      [6-bit pc delta] 00
//
//   01: code_target:          [6-bit pc delta] 01
//
//   10: wasm_stub_call:       [6-bit pc delta] 10
//
//   11: long_record           [6 bit reloc mode] 11
//                             followed by pc delta
//                             followed by optional data depending on type.
//
//  If a pc delta exceeds 6 bits, it is split into a remainder that fits into
//  6 bits and a part that does not. The latter is encoded as a long record
//  with PC_JUMP as pseudo reloc info mode. The former is encoded as part of
//  the following record in the usual way. The long pc jump record has variable
//  length:
//               pc-jump:        [PC_JUMP] 11
//                               1 [7 bits data]
//                                  ...
//                               0 [7 bits data]
//               (Bits 6..31 of pc delta, encoded with VLQ.)

const int kTagBits = 2;
const int kTagMask = (1 << kTagBits) - 1;
const int kLongTagBits = 6;

const int kEmbeddedObjectTag = 0;
const int kCodeTargetTag = 1;
const int kWasmStubCallTag = 2;
const int kDefaultTag = 3;

const int kSmallPCDeltaBits = kBitsPerByte - kTagBits;
const int kSmallPCDeltaMask = (1 << kSmallPCDeltaBits) - 1;
const int RelocInfo::kMaxSmallPCDelta = kSmallPCDeltaMask;

uint32_t RelocInfoWriter::WriteLongPCJump(uint32_t pc_delta) {
  // Return if the pc_delta can fit in kSmallPCDeltaBits bits.
  // Otherwise write a variable length PC jump for the bits that do
  // not fit in the kSmallPCDeltaBits bits.
  if (is_uintn(pc_delta, kSmallPCDeltaBits)) return pc_delta;
  WriteMode(RelocInfo::PC_JUMP);
  uint32_t pc_jump = pc_delta >> kSmallPCDeltaBits;
  DCHECK_GT(pc_jump, 0);
  base::VLQEncodeUnsigned(
      [this](byte byte) {
        *--pos_ = byte;
        return pos_;
      },
      pc_jump);
  // Return the remaining kSmallPCDeltaBits of the pc_delta.
  return pc_delta & kSmallPCDeltaMask;
}

void RelocInfoWriter::WriteShortTaggedPC(uint32_t pc_delta, int tag) {
  // Write a byte of tagged pc-delta, possibly preceded by an explicit pc-jump.
  pc_delta = WriteLongPCJump(pc_delta);
  *--pos_ = pc_delta << kTagBits | tag;
}

void RelocInfoWriter::WriteShortData(intptr_t data_delta) {
  *--pos_ = static_cast<byte>(data_delta);
}

void RelocInfoWriter::WriteMode(RelocInfo::Mode rmode) {
  static_assert(RelocInfo::NUMBER_OF_MODES <= (1 << kLongTagBits));
  *--pos_ = static_cast<int>((rmode << kTagBits) | kDefaultTag);
}

void RelocInfoWriter::WriteModeAndPC(uint32_t pc_delta, RelocInfo::Mode rmode) {
  // Write two-byte tagged pc-delta, possibly preceded by var. length pc-jump.
  pc_delta = WriteLongPCJump(pc_delta);
  WriteMode(rmode);
  *--pos_ = pc_delta;
}

void RelocInfoWriter::WriteIntData(int number) {
  for (int i = 0; i < kIntSize; i++) {
    *--pos_ = static_cast<byte>(number);
    // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    number = number >> kBitsPerByte;
  }
}

void RelocInfoWriter::Write(const RelocInfo* rinfo) {
  RelocInfo::Mode rmode = rinfo->rmode();
#ifdef DEBUG
  byte* begin_pos = pos_;
#endif
  DCHECK(rinfo->rmode() < RelocInfo::NUMBER_OF_MODES);
  DCHECK_GE(rinfo->pc() - reinterpret_cast<Address>(last_pc_), 0);
  // Use unsigned delta-encoding for pc.
  uint32_t pc_delta =
      static_cast<uint32_t>(rinfo->pc() - reinterpret_cast<Address>(last_pc_));

  // The two most common modes are given small tags, and usually fit in a byte.
  if (rmode == RelocInfo::FULL_EMBEDDED_OBJECT) {
    WriteShortTaggedPC(pc_delta, kEmbeddedObjectTag);
  } else if (rmode == RelocInfo::CODE_TARGET) {
    WriteShortTaggedPC(pc_delta, kCodeTargetTag);
    DCHECK_LE(begin_pos - pos_, RelocInfo::kMaxCallSize);
  } else if (rmode == RelocInfo::WASM_STUB_CALL) {
    WriteShortTaggedPC(pc_delta, kWasmStubCallTag);
  } else {
    WriteModeAndPC(pc_delta, rmode);
    if (RelocInfo::IsDeoptReason(rmode)) {
      DCHECK_LT(rinfo->data(), 1 << kBitsPerByte);
      WriteShortData(rinfo->data());
    } else if (RelocInfo::IsConstPool(rmode) ||
               RelocInfo::IsVeneerPool(rmode) || RelocInfo::IsDeoptId(rmode) ||
               RelocInfo::IsDeoptPosition(rmode) ||
               RelocInfo::IsDeoptNodeId(rmode)) {
      WriteIntData(static_cast<int>(rinfo->data()));
    }
  }
  last_pc_ = reinterpret_cast<byte*>(rinfo->pc());
#ifdef DEBUG
  DCHECK_LE(begin_pos - pos_, kMaxSize);
#endif
}

inline int RelocIterator::AdvanceGetTag() { return *--pos_ & kTagMask; }

inline RelocInfo::Mode RelocIterator::GetMode() {
  return static_cast<RelocInfo::Mode>((*pos_ >> kTagBits) &
                                      ((1 << kLongTagBits) - 1));
}

inline void RelocIterator::ReadShortTaggedPC() {
  rinfo_.pc_ += *pos_ >> kTagBits;
}

inline void RelocIterator::AdvanceReadPC() { rinfo_.pc_ += *--pos_; }

void RelocIterator::AdvanceReadInt() {
  int x = 0;
  for (int i = 0; i < kIntSize; i++) {
    x |= static_cast<int>(*--pos_) << i * kBitsPerByte;
  }
  rinfo_.data_ = x;
}

void RelocIterator::AdvanceReadData() {
  intptr_t x = 0;
  for (int i = 0; i < kIntptrSize; i++) {
    x |= static_cast<intptr_t>(*--pos_) << i * kBitsPerByte;
  }
  rinfo_.data_ = x;
}

void RelocIterator::AdvanceReadLongPCJump() {
  // Read the 32-kSmallPCDeltaBits most significant bits of the
  // pc jump as a VLQ encoded integer.
  uint32_t pc_jump = base::VLQDecodeUnsigned([this] { return *--pos_; });
  // The least significant kSmallPCDeltaBits bits will be added
  // later.
  rinfo_.pc_ += pc_jump << kSmallPCDeltaBits;
}

inline void RelocIterator::ReadShortData() {
  uint8_t unsigned_b = *pos_;
  rinfo_.data_ = unsigned_b;
}

void RelocIterator::next() {
  DCHECK(!done());
  // Basically, do the opposite of RelocInfoWriter::Write.
  // Reading of data is as far as possible avoided for unwanted modes,
  // but we must always update the pc.
  //
  // We exit this loop by returning when we find a mode we want.
  while (pos_ > end_) {
    int tag = AdvanceGetTag();
    if (tag == kEmbeddedObjectTag) {
      ReadShortTaggedPC();
      if (SetMode(RelocInfo::FULL_EMBEDDED_OBJECT)) return;
    } else if (tag == kCodeTargetTag) {
      ReadShortTaggedPC();
      if (SetMode(RelocInfo::CODE_TARGET)) return;
    } else if (tag == kWasmStubCallTag) {
      ReadShortTaggedPC();
      if (SetMode(RelocInfo::WASM_STUB_CALL)) return;
    } else {
      DCHECK_EQ(tag, kDefaultTag);
      RelocInfo::Mode rmode = GetMode();
      if (rmode == RelocInfo::PC_JUMP) {
        AdvanceReadLongPCJump();
      } else {
        AdvanceReadPC();
        if (RelocInfo::IsDeoptReason(rmode)) {
          Advance();
          if (SetMode(rmode)) {
            ReadShortData();
            return;
          }
        } else if (RelocInfo::IsConstPool(rmode) ||
                   RelocInfo::IsVeneerPool(rmode) ||
                   RelocInfo::IsDeoptId(rmode) ||
                   RelocInfo::IsDeoptPosition(rmode) ||
                   RelocInfo::IsDeoptNodeId(rmode)) {
          if (SetMode(rmode)) {
            AdvanceReadInt();
            return;
          }
          Advance(kIntSize);
        } else if (SetMode(static_cast<RelocInfo::Mode>(rmode))) {
          return;
        }
      }
    }
  }
  done_ = true;
}

RelocIterator::RelocIterator(Code code, int mode_mask)
    : RelocIterator(code, code.unchecked_relocation_info(), mode_mask) {}

RelocIterator::RelocIterator(Code code, ByteArray relocation_info,
                             int mode_mask)
    : RelocIterator(
          code,
          InstructionStream::unchecked_cast(code.raw_instruction_stream()),
          InstructionStream::unchecked_cast(code.raw_instruction_stream())
              .instruction_start(),
          code.constant_pool(), relocation_info.GetDataEndAddress(),
          relocation_info.GetDataStartAddress(), mode_mask) {}

RelocIterator::RelocIterator(Code code, InstructionStream instruction_stream,
                             ByteArray relocation_info, Address constant_pool,
                             int mode_mask)
    : RelocIterator(code, instruction_stream,
                    instruction_stream.instruction_start(), constant_pool,
                    relocation_info.GetDataEndAddress(),
                    relocation_info.GetDataStartAddress(), mode_mask) {}

RelocIterator::RelocIterator(const CodeReference code_reference, int mode_mask)
    : RelocIterator(
          Code(), InstructionStream(), code_reference.instruction_start(),
          code_reference.constant_pool(), code_reference.relocation_end(),
          code_reference.relocation_start(), mode_mask) {}

RelocIterator::RelocIterator(EmbeddedData* embedded_data, Code code,
                             int mode_mask)
    : RelocIterator(code, code.instruction_stream(),
                    embedded_data->InstructionStartOfBuiltin(code.builtin_id()),
                    code.constant_pool(),
                    code.relocation_start() + code.relocation_size(),
                    code.relocation_start(), mode_mask) {}

RelocIterator::RelocIterator(const CodeDesc& desc, int mode_mask)
    : RelocIterator(
          Code(), InstructionStream(), reinterpret_cast<Address>(desc.buffer),
          0, desc.buffer + desc.buffer_size,
          desc.buffer + desc.buffer_size - desc.reloc_size, mode_mask) {}

RelocIterator::RelocIterator(base::Vector<byte> instructions,
                             base::Vector<const byte> reloc_info,
                             Address const_pool, int mode_mask)
    : RelocIterator(Code(), InstructionStream(),
                    reinterpret_cast<Address>(instructions.begin()), const_pool,
                    reloc_info.begin() + reloc_info.size(), reloc_info.begin(),
                    mode_mask) {}

RelocIterator::RelocIterator(Code code, InstructionStream instruction_stream,
                             Address pc, Address constant_pool, const byte* pos,
                             const byte* end, int mode_mask)
    : pos_(pos), end_(end), mode_mask_(mode_mask) {
  // Relocation info is read backwards.
  DCHECK_GE(pos_, end_);
  rinfo_.code_ = code;
  rinfo_.pc_ = pc;
  rinfo_.instruction_stream_ = instruction_stream;
  rinfo_.constant_pool_ = constant_pool;
  if (mode_mask_ == 0) pos_ = end_;
  next();
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

// static
bool RelocInfo::OffHeapTargetIsCodedSpecially() {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_ARM64) || \
    defined(V8_TARGET_ARCH_X64)
  return false;
#elif defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_MIPS64) || \
    defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_PPC64) ||     \
    defined(V8_TARGET_ARCH_S390) || defined(V8_TARGET_ARCH_RISCV64) ||  \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_RISCV32)
  return true;
#endif
}

Address RelocInfo::wasm_call_address() const {
  DCHECK_EQ(rmode_, WASM_CALL);
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_wasm_call_address(Address address,
                                      ICacheFlushMode icache_flush_mode) {
  DCHECK_EQ(rmode_, WASM_CALL);
  Assembler::set_target_address_at(pc_, constant_pool_, address,
                                   icache_flush_mode);
}

Address RelocInfo::wasm_stub_call_address() const {
  DCHECK_EQ(rmode_, WASM_STUB_CALL);
  return Assembler::target_address_at(pc_, constant_pool_);
}

void RelocInfo::set_wasm_stub_call_address(Address address,
                                           ICacheFlushMode icache_flush_mode) {
  DCHECK_EQ(rmode_, WASM_STUB_CALL);
  Assembler::set_target_address_at(pc_, constant_pool_, address,
                                   icache_flush_mode);
}

void RelocInfo::set_target_address(Address target,
                                   WriteBarrierMode write_barrier_mode,
                                   ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTargetMode(rmode_) || IsNearBuiltinEntry(rmode_) ||
         IsWasmCall(rmode_));
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
  if (!instruction_stream().is_null() && IsCodeTargetMode(rmode_) &&
      !v8_flags.disable_write_barriers) {
    InstructionStream target_code =
        InstructionStream::FromTargetAddress(target);
    WriteBarrierForCode(instruction_stream(), this, target_code,
                        write_barrier_mode);
  }
}

void RelocInfo::set_off_heap_target_address(Address target,
                                            ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTargetMode(rmode_));
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
}

bool RelocInfo::HasTargetAddressAddress() const {
  // TODO(jgruber): Investigate whether WASM_CALL is still appropriate on
  // non-intel platforms now that wasm code is no longer on the heap.
#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)
  static constexpr int kTargetAddressAddressModeMask =
      ModeMask(CODE_TARGET) | ModeMask(FULL_EMBEDDED_OBJECT) |
      ModeMask(COMPRESSED_EMBEDDED_OBJECT) | ModeMask(EXTERNAL_REFERENCE) |
      ModeMask(OFF_HEAP_TARGET) | ModeMask(WASM_CALL) |
      ModeMask(WASM_STUB_CALL);
#else
  static constexpr int kTargetAddressAddressModeMask =
      ModeMask(CODE_TARGET) | ModeMask(RELATIVE_CODE_TARGET) |
      ModeMask(FULL_EMBEDDED_OBJECT) | ModeMask(EXTERNAL_REFERENCE) |
      ModeMask(OFF_HEAP_TARGET) | ModeMask(WASM_CALL);
#endif
  return (ModeMask(rmode_) & kTargetAddressAddressModeMask) != 0;
}

#ifdef ENABLE_DISASSEMBLER
const char* RelocInfo::RelocModeName(RelocInfo::Mode rmode) {
  switch (rmode) {
    case NO_INFO:
      return "no reloc";
    case COMPRESSED_EMBEDDED_OBJECT:
      return "compressed embedded object";
    case FULL_EMBEDDED_OBJECT:
      return "full embedded object";
    case CODE_TARGET:
      return "code target";
    case RELATIVE_CODE_TARGET:
      return "relative code target";
    case EXTERNAL_REFERENCE:
      return "external reference";
    case INTERNAL_REFERENCE:
      return "internal reference";
    case INTERNAL_REFERENCE_ENCODED:
      return "encoded internal reference";
    case OFF_HEAP_TARGET:
      return "off heap target";
    case NEAR_BUILTIN_ENTRY:
      return "near builtin entry";
    case DEOPT_SCRIPT_OFFSET:
      return "deopt script offset";
    case DEOPT_INLINING_ID:
      return "deopt inlining id";
    case DEOPT_REASON:
      return "deopt reason";
    case DEOPT_ID:
      return "deopt index";
    case LITERAL_CONSTANT:
      return "literal constant";
    case DEOPT_NODE_ID:
      return "deopt node id";
    case CONST_POOL:
      return "constant pool";
    case VENEER_POOL:
      return "veneer pool";
    case WASM_CALL:
      return "internal wasm call";
    case WASM_STUB_CALL:
      return "wasm stub call";
    case NUMBER_OF_MODES:
    case PC_JUMP:
      UNREACHABLE();
  }
  return "unknown relocation type";
}

void RelocInfo::Print(Isolate* isolate, std::ostream& os) {
  os << reinterpret_cast<const void*>(pc_) << "  " << RelocModeName(rmode_);
  if (rmode_ == DEOPT_SCRIPT_OFFSET || rmode_ == DEOPT_INLINING_ID) {
    os << "  (" << data() << ")";
  } else if (rmode_ == DEOPT_REASON) {
    os << "  ("
       << DeoptimizeReasonToString(static_cast<DeoptimizeReason>(data_)) << ")";
  } else if (rmode_ == FULL_EMBEDDED_OBJECT) {
    os << "  (" << Brief(target_object(isolate)) << ")";
  } else if (rmode_ == COMPRESSED_EMBEDDED_OBJECT) {
    os << "  (" << Brief(target_object(isolate)) << " compressed)";
  } else if (rmode_ == EXTERNAL_REFERENCE) {
    if (isolate) {
      ExternalReferenceEncoder ref_encoder(isolate);
      os << " ("
         << ref_encoder.NameOfAddress(isolate, target_external_reference())
         << ") ";
    }
    os << " (" << reinterpret_cast<const void*>(target_external_reference())
       << ")";
  } else if (IsCodeTargetMode(rmode_)) {
    const Address code_target = target_address();
    Code target_code = Code::FromTargetAddress(code_target);
    os << " (" << CodeKindToString(target_code.kind());
    if (Builtins::IsBuiltin(target_code)) {
      os << " " << Builtins::name(target_code.builtin_id());
    }
    os << ")  (" << reinterpret_cast<const void*>(target_address()) << ")";
  } else if (IsConstPool(rmode_)) {
    os << " (size " << static_cast<int>(data_) << ")";
  } else if (IsWasmStubCall(rmode_)) {
    os << "  (";
    Address addr = target_address();
    if (isolate != nullptr) {
      Builtin builtin = OffHeapInstructionStream::TryLookupCode(isolate, addr);
      os << (Builtins::IsBuiltinId(builtin) ? Builtins::name(builtin)
                                            : "<UNRECOGNIZED>")
         << ")  (";
    }
    os << reinterpret_cast<const void*>(addr) << ")";
  }

  os << "\n";
}
#endif  // ENABLE_DISASSEMBLER

#ifdef VERIFY_HEAP
void RelocInfo::Verify(Isolate* isolate) {
  switch (rmode_) {
    case COMPRESSED_EMBEDDED_OBJECT:
      Object::VerifyPointer(isolate, target_object(isolate));
      break;
    case FULL_EMBEDDED_OBJECT:
      Object::VerifyAnyTagged(isolate, target_object(isolate));
      break;
    case CODE_TARGET:
    case RELATIVE_CODE_TARGET: {
      // convert inline target address to code object
      Address addr = target_address();
      CHECK_NE(addr, kNullAddress);
      // Check that we can find the right code object.
      InstructionStream code = InstructionStream::FromTargetAddress(addr);
      Code lookup_result = isolate->heap()->FindCodeForInnerPointer(addr);
      CHECK_EQ(code.address(), lookup_result.instruction_stream().address());
      break;
    }
    case INTERNAL_REFERENCE:
    case INTERNAL_REFERENCE_ENCODED: {
      Address target = target_internal_reference();
      Address pc = target_internal_reference_address();
      Code lookup_result = isolate->heap()->FindCodeForInnerPointer(pc);
      CHECK_GE(target, lookup_result.InstructionStart());
      CHECK_LT(target, lookup_result.InstructionEnd());
      break;
    }
    case OFF_HEAP_TARGET: {
      Address addr = target_off_heap_target();
      CHECK_NE(addr, kNullAddress);
      CHECK(Builtins::IsBuiltinId(
          OffHeapInstructionStream::TryLookupCode(isolate, addr)));
      break;
    }
    case WASM_STUB_CALL:
    case NEAR_BUILTIN_ENTRY: {
      Address addr = target_address();
      CHECK_NE(addr, kNullAddress);
      CHECK(Builtins::IsBuiltinId(
          OffHeapInstructionStream::TryLookupCode(isolate, addr)));
      break;
    }
    case EXTERNAL_REFERENCE:
    case DEOPT_SCRIPT_OFFSET:
    case DEOPT_INLINING_ID:
    case DEOPT_REASON:
    case DEOPT_ID:
    case LITERAL_CONSTANT:
    case DEOPT_NODE_ID:
    case CONST_POOL:
    case VENEER_POOL:
    case WASM_CALL:
    case NO_INFO:
      break;
    case NUMBER_OF_MODES:
    case PC_JUMP:
      UNREACHABLE();
  }
}
#endif  // VERIFY_HEAP

}  // namespace internal
}  // namespace v8
