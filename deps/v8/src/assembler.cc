// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

#include "src/assembler.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/deoptimizer.h"
#include "src/disassembler.h"
#include "src/instruction-stream.h"
#include "src/isolate.h"
#include "src/ostreams.h"
#include "src/simulator.h"  // For flushing instruction cache.
#include "src/snapshot/serializer-common.h"

namespace v8 {
namespace internal {

const char* const RelocInfo::kFillerCommentString = "DEOPTIMIZATION PADDING";

// -----------------------------------------------------------------------------
// Implementation of AssemblerBase

AssemblerBase::IsolateData::IsolateData(Isolate* isolate)
    : serializer_enabled_(isolate->serializer_enabled())
#if V8_TARGET_ARCH_X64
      ,
      code_range_start_(
          isolate->heap()->memory_allocator()->code_range()->start())
#endif
{
}

AssemblerBase::AssemblerBase(IsolateData isolate_data, void* buffer,
                             int buffer_size)
    : isolate_data_(isolate_data),
      enabled_cpu_features_(0),
      emit_debug_code_(FLAG_debug_code),
      predictable_code_size_(false),
      constant_pool_available_(false),
      jump_optimization_info_(nullptr) {
  own_buffer_ = buffer == nullptr;
  if (buffer_size == 0) buffer_size = kMinimalBufferSize;
  DCHECK_GT(buffer_size, 0);
  if (own_buffer_) buffer = NewArray<byte>(buffer_size);
  buffer_ = static_cast<byte*>(buffer);
  buffer_size_ = buffer_size;
  pc_ = buffer_;
}

AssemblerBase::~AssemblerBase() {
  if (own_buffer_) DeleteArray(buffer_);
}

void AssemblerBase::FlushICache(void* start, size_t size) {
  if (size == 0) return;

#if defined(USE_SIMULATOR)
  base::LockGuard<base::Mutex> lock_guard(Simulator::i_cache_mutex());
  Simulator::FlushICache(Simulator::i_cache(), start, size);
#else
  CpuFeatures::FlushICache(start, size);
#endif  // USE_SIMULATOR
}

void AssemblerBase::Print(Isolate* isolate) {
  OFStream os(stdout);
  v8::internal::Disassembler::Decode(isolate, &os, buffer_, pc_, nullptr);
}

// -----------------------------------------------------------------------------
// Implementation of PredictableCodeSizeScope

PredictableCodeSizeScope::PredictableCodeSizeScope(AssemblerBase* assembler,
                                                   int expected_size)
    : assembler_(assembler),
      expected_size_(expected_size),
      start_offset_(assembler->pc_offset()),
      old_value_(assembler->predictable_code_size()) {
  assembler_->set_predictable_code_size(true);
}

PredictableCodeSizeScope::~PredictableCodeSizeScope() {
  CHECK_EQ(expected_size_, assembler_->pc_offset() - start_offset_);
  assembler_->set_predictable_code_size(old_value_);
}

// -----------------------------------------------------------------------------
// Implementation of CpuFeatureScope

#ifdef DEBUG
CpuFeatureScope::CpuFeatureScope(AssemblerBase* assembler, CpuFeature f,
                                 CheckPolicy check)
    : assembler_(assembler) {
  DCHECK_IMPLIES(check == kCheckSupported, CpuFeatures::IsSupported(f));
  old_enabled_ = assembler_->enabled_cpu_features();
  assembler_->EnableCpuFeature(f);
}

CpuFeatureScope::~CpuFeatureScope() {
  assembler_->set_enabled_cpu_features(old_enabled_);
}
#endif

bool CpuFeatures::initialized_ = false;
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::icache_line_size_ = 0;
unsigned CpuFeatures::dcache_line_size_ = 0;

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
//   10: short_data_record:    [6-bit pc delta] 10 followed by
//                             [8-bit data delta]
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
//                               [7 bits data] 0
//                                  ...
//                               [7 bits data] 1
//               (Bits 6..31 of pc delta, with leading zeroes
//                dropped, and last non-zero chunk tagged with 1.)

const int kTagBits = 2;
const int kTagMask = (1 << kTagBits) - 1;
const int kLongTagBits = 6;

const int kEmbeddedObjectTag = 0;
const int kCodeTargetTag = 1;
const int kLocatableTag = 2;
const int kDefaultTag = 3;

const int kSmallPCDeltaBits = kBitsPerByte - kTagBits;
const int kSmallPCDeltaMask = (1 << kSmallPCDeltaBits) - 1;
const int RelocInfo::kMaxSmallPCDelta = kSmallPCDeltaMask;

const int kChunkBits = 7;
const int kChunkMask = (1 << kChunkBits) - 1;
const int kLastChunkTagBits = 1;
const int kLastChunkTagMask = 1;
const int kLastChunkTag = 1;

// static
bool RelocInfo::OffHeapTargetIsCodedSpecially() {
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_ARM64) || \
    defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32)
  return false;
#elif defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64) || \
    defined(V8_TARGET_ARCH_PPC) || defined(V8_TARGET_ARCH_S390)
  return true;
#endif
}

void RelocInfo::set_global_handle(Address address,
                                  ICacheFlushMode icache_flush_mode) {
  DCHECK_EQ(rmode_, WASM_GLOBAL_HANDLE);
  set_embedded_address(address, icache_flush_mode);
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

Address RelocInfo::global_handle() const {
  DCHECK_EQ(rmode_, WASM_GLOBAL_HANDLE);
  return embedded_address();
}

void RelocInfo::set_target_address(Address target,
                                   WriteBarrierMode write_barrier_mode,
                                   ICacheFlushMode icache_flush_mode) {
  DCHECK(IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_) || IsWasmCall(rmode_));
  Assembler::set_target_address_at(pc_, constant_pool_, target,
                                   icache_flush_mode);
  if (write_barrier_mode == UPDATE_WRITE_BARRIER && host() != nullptr &&
      IsCodeTarget(rmode_)) {
    Code* target_code = Code::GetCodeFromTargetAddress(target);
    host()->GetHeap()->incremental_marking()->RecordWriteIntoCode(host(), this,
                                                                  target_code);
  }
}

uint32_t RelocInfoWriter::WriteLongPCJump(uint32_t pc_delta) {
  // Return if the pc_delta can fit in kSmallPCDeltaBits bits.
  // Otherwise write a variable length PC jump for the bits that do
  // not fit in the kSmallPCDeltaBits bits.
  if (is_uintn(pc_delta, kSmallPCDeltaBits)) return pc_delta;
  WriteMode(RelocInfo::PC_JUMP);
  uint32_t pc_jump = pc_delta >> kSmallPCDeltaBits;
  DCHECK_GT(pc_jump, 0);
  // Write kChunkBits size chunks of the pc_jump.
  for (; pc_jump > 0; pc_jump = pc_jump >> kChunkBits) {
    byte b = pc_jump & kChunkMask;
    *--pos_ = b << kLastChunkTagBits;
  }
  // Tag the last chunk so it can be identified.
  *pos_ = *pos_ | kLastChunkTag;
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
  STATIC_ASSERT(RelocInfo::NUMBER_OF_MODES <= (1 << kLongTagBits));
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

void RelocInfoWriter::WriteData(intptr_t data_delta) {
  for (int i = 0; i < kIntptrSize; i++) {
    *--pos_ = static_cast<byte>(data_delta);
    // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    data_delta = data_delta >> kBitsPerByte;
  }
}

void RelocInfoWriter::Write(const RelocInfo* rinfo) {
  RelocInfo::Mode rmode = rinfo->rmode();
#ifdef DEBUG
  byte* begin_pos = pos_;
#endif
  DCHECK(rinfo->rmode() < RelocInfo::NUMBER_OF_MODES);
  DCHECK_GE(rinfo->pc() - last_pc_, 0);
  // Use unsigned delta-encoding for pc.
  uint32_t pc_delta = static_cast<uint32_t>(rinfo->pc() - last_pc_);

  // The two most common modes are given small tags, and usually fit in a byte.
  if (rmode == RelocInfo::EMBEDDED_OBJECT) {
    WriteShortTaggedPC(pc_delta, kEmbeddedObjectTag);
  } else if (rmode == RelocInfo::CODE_TARGET) {
    WriteShortTaggedPC(pc_delta, kCodeTargetTag);
    DCHECK_LE(begin_pos - pos_, RelocInfo::kMaxCallSize);
  } else if (rmode == RelocInfo::DEOPT_REASON) {
    DCHECK(rinfo->data() < (1 << kBitsPerByte));
    WriteShortTaggedPC(pc_delta, kLocatableTag);
    WriteShortData(rinfo->data());
  } else {
    WriteModeAndPC(pc_delta, rmode);
    if (RelocInfo::IsComment(rmode)) {
      WriteData(rinfo->data());
    } else if (RelocInfo::IsConstPool(rmode) ||
               RelocInfo::IsVeneerPool(rmode) || RelocInfo::IsDeoptId(rmode) ||
               RelocInfo::IsDeoptPosition(rmode)) {
      WriteIntData(static_cast<int>(rinfo->data()));
    }
  }
  last_pc_ = rinfo->pc();
#ifdef DEBUG
  DCHECK_LE(begin_pos - pos_, kMaxSize);
#endif
}

inline int RelocIterator::AdvanceGetTag() {
  return *--pos_ & kTagMask;
}

inline RelocInfo::Mode RelocIterator::GetMode() {
  return static_cast<RelocInfo::Mode>((*pos_ >> kTagBits) &
                                      ((1 << kLongTagBits) - 1));
}

inline void RelocIterator::ReadShortTaggedPC() {
  rinfo_.pc_ += *pos_ >> kTagBits;
}

inline void RelocIterator::AdvanceReadPC() {
  rinfo_.pc_ += *--pos_;
}

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
  // pc jump in kChunkBits bit chunks and shift them into place.
  // Stop when the last chunk is encountered.
  uint32_t pc_jump = 0;
  for (int i = 0; i < kIntSize; i++) {
    byte pc_jump_part = *--pos_;
    pc_jump |= (pc_jump_part >> kLastChunkTagBits) << i * kChunkBits;
    if ((pc_jump_part & kLastChunkTagMask) == 1) break;
  }
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
      if (SetMode(RelocInfo::EMBEDDED_OBJECT)) return;
    } else if (tag == kCodeTargetTag) {
      ReadShortTaggedPC();
      if (SetMode(RelocInfo::CODE_TARGET)) return;
    } else if (tag == kLocatableTag) {
      ReadShortTaggedPC();
      Advance();
      if (SetMode(RelocInfo::DEOPT_REASON)) {
        ReadShortData();
        return;
      }
    } else {
      DCHECK_EQ(tag, kDefaultTag);
      RelocInfo::Mode rmode = GetMode();
      if (rmode == RelocInfo::PC_JUMP) {
        AdvanceReadLongPCJump();
      } else {
        AdvanceReadPC();
        if (RelocInfo::IsComment(rmode)) {
          if (SetMode(rmode)) {
            AdvanceReadData();
            return;
          }
          Advance(kIntptrSize);
        } else if (RelocInfo::IsConstPool(rmode) ||
                   RelocInfo::IsVeneerPool(rmode) ||
                   RelocInfo::IsDeoptId(rmode) ||
                   RelocInfo::IsDeoptPosition(rmode)) {
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

RelocIterator::RelocIterator(Code* code, int mode_mask)
    : mode_mask_(mode_mask) {
  rinfo_.host_ = code;
  rinfo_.pc_ = code->raw_instruction_start();
  rinfo_.data_ = 0;
  rinfo_.constant_pool_ = code->constant_pool();
  // Relocation info is read backwards.
  pos_ = code->relocation_start() + code->relocation_size();
  end_ = code->relocation_start();
  if (mode_mask_ == 0) pos_ = end_;
  next();
}

RelocIterator::RelocIterator(const CodeDesc& desc, int mode_mask)
    : mode_mask_(mode_mask) {
  rinfo_.pc_ = desc.buffer;
  // Relocation info is read backwards.
  pos_ = desc.buffer + desc.buffer_size;
  end_ = pos_ - desc.reloc_size;
  if (mode_mask_ == 0) pos_ = end_;
  next();
}

RelocIterator::RelocIterator(Vector<byte> instructions,
                             Vector<const byte> reloc_info, Address const_pool,
                             int mode_mask)
    : mode_mask_(mode_mask) {
  rinfo_.pc_ = instructions.start();
  rinfo_.constant_pool_ = const_pool;
  rinfo_.flags_ = RelocInfo::kInNativeWasmCode;
  // Relocation info is read backwards.
  pos_ = reloc_info.start() + reloc_info.size();
  end_ = reloc_info.start();
  if (mode_mask_ == 0) pos_ = end_;
  next();
}

// -----------------------------------------------------------------------------
// Implementation of RelocInfo

#ifdef DEBUG
bool RelocInfo::RequiresRelocation(const CodeDesc& desc) {
  // Ensure there are no code targets or embedded objects present in the
  // deoptimization entries, they would require relocation after code
  // generation.
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::kApplyMask;
  RelocIterator it(desc, mode_mask);
  return !it.done();
}
#endif

#ifdef ENABLE_DISASSEMBLER
const char* RelocInfo::RelocModeName(RelocInfo::Mode rmode) {
  switch (rmode) {
    case NONE:
      return "no reloc";
    case EMBEDDED_OBJECT:
      return "embedded object";
    case CODE_TARGET:
      return "code target";
    case RUNTIME_ENTRY:
      return "runtime entry";
    case COMMENT:
      return "comment";
    case EXTERNAL_REFERENCE:
      return "external reference";
    case INTERNAL_REFERENCE:
      return "internal reference";
    case INTERNAL_REFERENCE_ENCODED:
      return "encoded internal reference";
    case OFF_HEAP_TARGET:
      return "off heap target";
    case DEOPT_SCRIPT_OFFSET:
      return "deopt script offset";
    case DEOPT_INLINING_ID:
      return "deopt inlining id";
    case DEOPT_REASON:
      return "deopt reason";
    case DEOPT_ID:
      return "deopt index";
    case CONST_POOL:
      return "constant pool";
    case VENEER_POOL:
      return "veneer pool";
    case WASM_GLOBAL_HANDLE:
      return "global handle";
    case WASM_CALL:
      return "internal wasm call";
    case WASM_CODE_TABLE_ENTRY:
      return "wasm code table entry";
    case JS_TO_WASM_CALL:
      return "js to wasm call";
    case NUMBER_OF_MODES:
    case PC_JUMP:
      UNREACHABLE();
  }
  return "unknown relocation type";
}

void RelocInfo::Print(Isolate* isolate, std::ostream& os) {  // NOLINT
  os << static_cast<const void*>(pc_) << "  " << RelocModeName(rmode_);
  if (IsComment(rmode_)) {
    os << "  (" << reinterpret_cast<char*>(data_) << ")";
  } else if (rmode_ == DEOPT_SCRIPT_OFFSET || rmode_ == DEOPT_INLINING_ID) {
    os << "  (" << data() << ")";
  } else if (rmode_ == DEOPT_REASON) {
    os << "  ("
       << DeoptimizeReasonToString(static_cast<DeoptimizeReason>(data_)) << ")";
  } else if (rmode_ == EMBEDDED_OBJECT) {
    os << "  (" << Brief(target_object()) << ")";
  } else if (rmode_ == EXTERNAL_REFERENCE) {
    ExternalReferenceEncoder ref_encoder(isolate);
    os << " ("
       << ref_encoder.NameOfAddress(isolate, target_external_reference())
       << ")  (" << static_cast<const void*>(target_external_reference())
       << ")";
  } else if (IsCodeTarget(rmode_)) {
    const Address code_target = target_address();
    if (flags_ & kInNativeWasmCode) {
      os << " (wasm trampoline) ";
    } else {
      Code* code = Code::GetCodeFromTargetAddress(code_target);
      DCHECK(code->IsCode());
      os << " (" << Code::Kind2String(code->kind());
      if (Builtins::IsBuiltin(code)) {
        os << " " << Builtins::name(code->builtin_index());
      } else if (code->kind() == Code::STUB) {
        os << " " << CodeStub::MajorName(CodeStub::GetMajorKey(code));
      }
      os << ") ";
    }
    os << " (" << static_cast<const void*>(target_address()) << ")";
  } else if (IsRuntimeEntry(rmode_) && isolate->deoptimizer_data() != nullptr) {
    // Depotimization bailouts are stored as runtime entries.
    int id = Deoptimizer::GetDeoptimizationId(
        isolate, target_address(), Deoptimizer::EAGER);
    if (id != Deoptimizer::kNotDeoptimizationEntry) {
      os << "  (deoptimization bailout " << id << ")";
    }
  } else if (IsConstPool(rmode_)) {
    os << " (size " << static_cast<int>(data_) << ")";
  }

  os << "\n";
}
#endif  // ENABLE_DISASSEMBLER

#ifdef VERIFY_HEAP
void RelocInfo::Verify(Isolate* isolate) {
  switch (rmode_) {
    case EMBEDDED_OBJECT:
      Object::VerifyPointer(target_object());
      break;
    case CODE_TARGET: {
      // convert inline target address to code object
      Address addr = target_address();
      CHECK_NOT_NULL(addr);
      // Check that we can find the right code object.
      Code* code = Code::GetCodeFromTargetAddress(addr);
      Object* found = isolate->FindCodeObject(addr);
      CHECK(found->IsCode());
      CHECK(code->address() == HeapObject::cast(found)->address());
      break;
    }
    case INTERNAL_REFERENCE:
    case INTERNAL_REFERENCE_ENCODED: {
      Address target = target_internal_reference();
      Address pc = target_internal_reference_address();
      Code* code = Code::cast(isolate->FindCodeObject(pc));
      CHECK(target >= code->InstructionStart());
      CHECK(target <= code->InstructionEnd());
      break;
    }
    case OFF_HEAP_TARGET: {
      Address addr = target_off_heap_target();
      CHECK_NOT_NULL(addr);
      CHECK_NOT_NULL(InstructionStream::TryLookupCode(isolate, addr));
      break;
    }
    case RUNTIME_ENTRY:
    case COMMENT:
    case EXTERNAL_REFERENCE:
    case DEOPT_SCRIPT_OFFSET:
    case DEOPT_INLINING_ID:
    case DEOPT_REASON:
    case DEOPT_ID:
    case CONST_POOL:
    case VENEER_POOL:
    case WASM_GLOBAL_HANDLE:
    case WASM_CALL:
    case JS_TO_WASM_CALL:
    case WASM_CODE_TABLE_ENTRY:
    case NONE:
      break;
    case NUMBER_OF_MODES:
    case PC_JUMP:
      UNREACHABLE();
      break;
  }
}
#endif  // VERIFY_HEAP

ConstantPoolBuilder::ConstantPoolBuilder(int ptr_reach_bits,
                                         int double_reach_bits) {
  info_[ConstantPoolEntry::INTPTR].entries.reserve(64);
  info_[ConstantPoolEntry::INTPTR].regular_reach_bits = ptr_reach_bits;
  info_[ConstantPoolEntry::DOUBLE].regular_reach_bits = double_reach_bits;
}

ConstantPoolEntry::Access ConstantPoolBuilder::NextAccess(
    ConstantPoolEntry::Type type) const {
  const PerTypeEntryInfo& info = info_[type];

  if (info.overflow()) return ConstantPoolEntry::OVERFLOWED;

  int dbl_count = info_[ConstantPoolEntry::DOUBLE].regular_count;
  int dbl_offset = dbl_count * kDoubleSize;
  int ptr_count = info_[ConstantPoolEntry::INTPTR].regular_count;
  int ptr_offset = ptr_count * kPointerSize + dbl_offset;

  if (type == ConstantPoolEntry::DOUBLE) {
    // Double overflow detection must take into account the reach for both types
    int ptr_reach_bits = info_[ConstantPoolEntry::INTPTR].regular_reach_bits;
    if (!is_uintn(dbl_offset, info.regular_reach_bits) ||
        (ptr_count > 0 &&
         !is_uintn(ptr_offset + kDoubleSize - kPointerSize, ptr_reach_bits))) {
      return ConstantPoolEntry::OVERFLOWED;
    }
  } else {
    DCHECK(type == ConstantPoolEntry::INTPTR);
    if (!is_uintn(ptr_offset, info.regular_reach_bits)) {
      return ConstantPoolEntry::OVERFLOWED;
    }
  }

  return ConstantPoolEntry::REGULAR;
}

ConstantPoolEntry::Access ConstantPoolBuilder::AddEntry(
    ConstantPoolEntry& entry, ConstantPoolEntry::Type type) {
  DCHECK(!emitted_label_.is_bound());
  PerTypeEntryInfo& info = info_[type];
  const int entry_size = ConstantPoolEntry::size(type);
  bool merged = false;

  if (entry.sharing_ok()) {
    // Try to merge entries
    std::vector<ConstantPoolEntry>::iterator it = info.shared_entries.begin();
    int end = static_cast<int>(info.shared_entries.size());
    for (int i = 0; i < end; i++, it++) {
      if ((entry_size == kPointerSize) ? entry.value() == it->value()
                                       : entry.value64() == it->value64()) {
        // Merge with found entry.
        entry.set_merged_index(i);
        merged = true;
        break;
      }
    }
  }

  // By definition, merged entries have regular access.
  DCHECK(!merged || entry.merged_index() < info.regular_count);
  ConstantPoolEntry::Access access =
      (merged ? ConstantPoolEntry::REGULAR : NextAccess(type));

  // Enforce an upper bound on search time by limiting the search to
  // unique sharable entries which fit in the regular section.
  if (entry.sharing_ok() && !merged && access == ConstantPoolEntry::REGULAR) {
    info.shared_entries.push_back(entry);
  } else {
    info.entries.push_back(entry);
  }

  // We're done if we found a match or have already triggered the
  // overflow state.
  if (merged || info.overflow()) return access;

  if (access == ConstantPoolEntry::REGULAR) {
    info.regular_count++;
  } else {
    info.overflow_start = static_cast<int>(info.entries.size()) - 1;
  }

  return access;
}

void ConstantPoolBuilder::EmitSharedEntries(Assembler* assm,
                                            ConstantPoolEntry::Type type) {
  PerTypeEntryInfo& info = info_[type];
  std::vector<ConstantPoolEntry>& shared_entries = info.shared_entries;
  const int entry_size = ConstantPoolEntry::size(type);
  int base = emitted_label_.pos();
  DCHECK_GT(base, 0);
  int shared_end = static_cast<int>(shared_entries.size());
  std::vector<ConstantPoolEntry>::iterator shared_it = shared_entries.begin();
  for (int i = 0; i < shared_end; i++, shared_it++) {
    int offset = assm->pc_offset() - base;
    shared_it->set_offset(offset);  // Save offset for merged entries.
    if (entry_size == kPointerSize) {
      assm->dp(shared_it->value());
    } else {
      assm->dq(shared_it->value64());
    }
    DCHECK(is_uintn(offset, info.regular_reach_bits));

    // Patch load sequence with correct offset.
    assm->PatchConstantPoolAccessInstruction(shared_it->position(), offset,
                                             ConstantPoolEntry::REGULAR, type);
  }
}

void ConstantPoolBuilder::EmitGroup(Assembler* assm,
                                    ConstantPoolEntry::Access access,
                                    ConstantPoolEntry::Type type) {
  PerTypeEntryInfo& info = info_[type];
  const bool overflow = info.overflow();
  std::vector<ConstantPoolEntry>& entries = info.entries;
  std::vector<ConstantPoolEntry>& shared_entries = info.shared_entries;
  const int entry_size = ConstantPoolEntry::size(type);
  int base = emitted_label_.pos();
  DCHECK_GT(base, 0);
  int begin;
  int end;

  if (access == ConstantPoolEntry::REGULAR) {
    // Emit any shared entries first
    EmitSharedEntries(assm, type);
  }

  if (access == ConstantPoolEntry::REGULAR) {
    begin = 0;
    end = overflow ? info.overflow_start : static_cast<int>(entries.size());
  } else {
    DCHECK(access == ConstantPoolEntry::OVERFLOWED);
    if (!overflow) return;
    begin = info.overflow_start;
    end = static_cast<int>(entries.size());
  }

  std::vector<ConstantPoolEntry>::iterator it = entries.begin();
  if (begin > 0) std::advance(it, begin);
  for (int i = begin; i < end; i++, it++) {
    // Update constant pool if necessary and get the entry's offset.
    int offset;
    ConstantPoolEntry::Access entry_access;
    if (!it->is_merged()) {
      // Emit new entry
      offset = assm->pc_offset() - base;
      entry_access = access;
      if (entry_size == kPointerSize) {
        assm->dp(it->value());
      } else {
        assm->dq(it->value64());
      }
    } else {
      // Retrieve offset from shared entry.
      offset = shared_entries[it->merged_index()].offset();
      entry_access = ConstantPoolEntry::REGULAR;
    }

    DCHECK(entry_access == ConstantPoolEntry::OVERFLOWED ||
           is_uintn(offset, info.regular_reach_bits));

    // Patch load sequence with correct offset.
    assm->PatchConstantPoolAccessInstruction(it->position(), offset,
                                             entry_access, type);
  }
}

// Emit and return position of pool.  Zero implies no constant pool.
int ConstantPoolBuilder::Emit(Assembler* assm) {
  bool emitted = emitted_label_.is_bound();
  bool empty = IsEmpty();

  if (!emitted) {
    // Mark start of constant pool.  Align if necessary.
    if (!empty) assm->DataAlign(kDoubleSize);
    assm->bind(&emitted_label_);
    if (!empty) {
      // Emit in groups based on access and type.
      // Emit doubles first for alignment purposes.
      EmitGroup(assm, ConstantPoolEntry::REGULAR, ConstantPoolEntry::DOUBLE);
      EmitGroup(assm, ConstantPoolEntry::REGULAR, ConstantPoolEntry::INTPTR);
      if (info_[ConstantPoolEntry::DOUBLE].overflow()) {
        assm->DataAlign(kDoubleSize);
        EmitGroup(assm, ConstantPoolEntry::OVERFLOWED,
                  ConstantPoolEntry::DOUBLE);
      }
      if (info_[ConstantPoolEntry::INTPTR].overflow()) {
        EmitGroup(assm, ConstantPoolEntry::OVERFLOWED,
                  ConstantPoolEntry::INTPTR);
      }
    }
  }

  return !empty ? emitted_label_.pos() : 0;
}

HeapObjectRequest::HeapObjectRequest(double heap_number, int offset)
    : kind_(kHeapNumber), offset_(offset) {
  value_.heap_number = heap_number;
  DCHECK(!IsSmiDouble(value_.heap_number));
}

HeapObjectRequest::HeapObjectRequest(CodeStub* code_stub, int offset)
    : kind_(kCodeStub), offset_(offset) {
  value_.code_stub = code_stub;
  DCHECK_NOT_NULL(value_.code_stub);
}

// Platform specific but identical code for all the platforms.

void Assembler::RecordDeoptReason(DeoptimizeReason reason,
                                  SourcePosition position, int id) {
  EnsureSpace ensure_space(this);
  RecordRelocInfo(RelocInfo::DEOPT_SCRIPT_OFFSET, position.ScriptOffset());
  RecordRelocInfo(RelocInfo::DEOPT_INLINING_ID, position.InliningId());
  RecordRelocInfo(RelocInfo::DEOPT_REASON, static_cast<int>(reason));
  RecordRelocInfo(RelocInfo::DEOPT_ID, id);
}

void Assembler::RecordComment(const char* msg) {
  if (FLAG_code_comments) {
    EnsureSpace ensure_space(this);
    RecordRelocInfo(RelocInfo::COMMENT, reinterpret_cast<intptr_t>(msg));
  }
}

void Assembler::DataAlign(int m) {
  DCHECK(m >= 2 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    db(0);
  }
}

void Assembler::RequestHeapObject(HeapObjectRequest request) {
  request.set_offset(pc_offset());
  heap_object_requests_.push_front(request);
}

}  // namespace internal
}  // namespace v8
